#include "t64_internal.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define T64_SP_DMEM_BASE UINT32_C(0x04000000)
#define T64_SP_IMEM_BASE UINT32_C(0x04001000)
#define T64_SP_MEMORY_END UINT32_C(0x04002000)
#define T64_CART_BASE UINT32_C(0x10000000)
#define T64_CART_END UINT32_C(0x1fc00000)
#define T64_ICACHE_LINE_MASK (T64_VR4300_ICACHE_LINE_BYTES - 1u)
#define T64_DCACHE_LINE_MASK (T64_VR4300_DCACHE_LINE_BYTES - 1u)
#define T64_CACHE_TAGLO_PTAG_MASK UINT32_C(0x0fffff00)
#define T64_CACHE_TAGLO_PSTATE_MASK UINT32_C(0x000000c0)
#define T64_CACHE_TAGLO_PTAG_SHIFT UINT32_C(8)
#define T64_CACHE_TAGLO_PSTATE_SHIFT UINT32_C(6)
#define T64_CACHE_CP0_TAGLO UINT32_C(28)
#define T64_CACHE_CP0_TAGHI UINT32_C(29)

#define T64_CACHE_INDEX_INVALIDATE_I UINT32_C(0x00)
#define T64_CACHE_INDEX_WRITEBACK_INVALIDATE_D UINT32_C(0x01)
#define T64_CACHE_INDEX_LOAD_TAG_I UINT32_C(0x04)
#define T64_CACHE_INDEX_LOAD_TAG_D UINT32_C(0x05)
#define T64_CACHE_INDEX_STORE_TAG_I UINT32_C(0x08)
#define T64_CACHE_INDEX_STORE_TAG_D UINT32_C(0x09)
#define T64_CACHE_CREATE_DIRTY_EXCLUSIVE_D UINT32_C(0x0d)
#define T64_CACHE_HIT_INVALIDATE_I UINT32_C(0x10)
#define T64_CACHE_HIT_INVALIDATE_D UINT32_C(0x11)
#define T64_CACHE_FILL_I UINT32_C(0x14)
#define T64_CACHE_HIT_WRITEBACK_INVALIDATE_D UINT32_C(0x15)
#define T64_CACHE_HIT_WRITEBACK_I UINT32_C(0x18)
#define T64_CACHE_HIT_WRITEBACK_D UINT32_C(0x19)

typedef enum {
  T64_ICACHE_BACKING_UNSUPPORTED = 0,
  T64_ICACHE_BACKING_RDRAM,
  T64_ICACHE_BACKING_SP_DMEM,
  T64_ICACHE_BACKING_SP_IMEM,
  T64_ICACHE_BACKING_CARTRIDGE
} t64_icache_backing_kind;

static uint32_t icache_index(uint64_t virtual_address) {
  return (uint32_t)((virtual_address >> 5) &
                    (T64_VR4300_ICACHE_LINE_COUNT - 1u));
}

static uint32_t icache_physical_tag(uint32_t physical_address) {
  return physical_address & ~T64_ICACHE_LINE_MASK;
}

static t64_icache_backing_kind icache_backing_kind_at(
    const t64_machine *machine, uint32_t physical_address) {
  if (machine->ram != NULL &&
      (uint64_t)physical_address < machine->profile.rdram_bytes) {
    return T64_ICACHE_BACKING_RDRAM;
  }
  if (physical_address >= T64_SP_DMEM_BASE &&
      physical_address < T64_SP_IMEM_BASE) {
    return T64_ICACHE_BACKING_SP_DMEM;
  }
  if (physical_address >= T64_SP_IMEM_BASE &&
      physical_address < T64_SP_MEMORY_END) {
    return T64_ICACHE_BACKING_SP_IMEM;
  }
  if (physical_address >= T64_CART_BASE && physical_address < T64_CART_END) {
    return T64_ICACHE_BACKING_CARTRIDGE;
  }
  return T64_ICACHE_BACKING_UNSUPPORTED;
}

static t64_status icache_backing_byte(const t64_machine *machine,
                                      uint32_t physical_address,
                                      t64_icache_backing_kind expected_kind,
                                      const uint8_t **byte_out) {
  t64_icache_backing_kind kind;
  size_t offset;

  if (machine == NULL || byte_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  kind = icache_backing_kind_at(machine, physical_address);
  if (kind != expected_kind) {
    return kind == T64_ICACHE_BACKING_UNSUPPORTED
               ? T64_STATUS_BUS_UNSUPPORTED
               : T64_STATUS_BUS_RANGE;
  }
  switch (kind) {
  case T64_ICACHE_BACKING_RDRAM:
    offset = (size_t)physical_address;
    if ((uint64_t)physical_address >= machine->profile.rdram_bytes) {
      return T64_STATUS_BUS_RANGE;
    }
    *byte_out = machine->ram + offset;
    return T64_STATUS_OK;
  case T64_ICACHE_BACKING_SP_DMEM:
    offset = (size_t)(physical_address - T64_SP_DMEM_BASE);
    *byte_out = machine->sp_dmem + offset;
    return T64_STATUS_OK;
  case T64_ICACHE_BACKING_SP_IMEM:
    offset = (size_t)(physical_address - T64_SP_IMEM_BASE);
    *byte_out = machine->sp_imem + offset;
    return T64_STATUS_OK;
  case T64_ICACHE_BACKING_CARTRIDGE:
    offset = (size_t)(physical_address - T64_CART_BASE);
    if (!machine->has_image || machine->image_storage.data == NULL ||
        offset >= machine->image_length) {
      return T64_STATUS_BUS_RANGE;
    }
    *byte_out = (const uint8_t *)machine->image_storage.data + offset;
    return T64_STATUS_OK;
  case T64_ICACHE_BACKING_UNSUPPORTED:
    break;
  }
  return T64_STATUS_BUS_UNSUPPORTED;
}

static t64_status icache_stage_line(const t64_machine *machine,
                                    uint32_t physical_address,
                                    uint8_t staged[T64_VR4300_ICACHE_LINE_BYTES]) {
  const uint32_t line_address =
      physical_address & ~T64_ICACHE_LINE_MASK;
  const uint64_t physical_end =
      (uint64_t)line_address + T64_VR4300_ICACHE_LINE_BYTES;
  const t64_icache_backing_kind backing_kind =
      icache_backing_kind_at(machine, line_address);
  uint32_t offset;

  if (backing_kind == T64_ICACHE_BACKING_UNSUPPORTED) {
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  if (physical_end > UINT64_C(0x100000000)) {
    return T64_STATUS_BUS_RANGE;
  }
  for (offset = 0u; offset < T64_VR4300_ICACHE_LINE_BYTES; offset++) {
    const uint8_t *byte = NULL;
    t64_status status =
        icache_backing_byte(machine, line_address + offset, backing_kind,
                            &byte);
    if (status != T64_STATUS_OK || byte == NULL) {
      return status == T64_STATUS_OK ? T64_STATUS_BUS_UNSUPPORTED : status;
    }
  }
  for (offset = 0u; offset < T64_VR4300_ICACHE_LINE_BYTES; offset++) {
    const uint8_t *byte = NULL;
    t64_status status =
        icache_backing_byte(machine, line_address + offset, backing_kind,
                            &byte);
    if (status != T64_STATUS_OK || byte == NULL) {
      return status == T64_STATUS_OK ? T64_STATUS_BUS_UNSUPPORTED : status;
    }
    staged[offset] = *byte;
  }
  return T64_STATUS_OK;
}

t64_status t64_cpu_icache_fetch(t64_machine *machine,
                                uint64_t virtual_address,
                                uint32_t physical_address,
                                uint32_t *opcode_out) {
  const uint32_t index = icache_index(virtual_address);
  const uint32_t tag = icache_physical_tag(physical_address);
  const uint32_t offset = physical_address & T64_ICACHE_LINE_MASK;
  t64_vr4300_icache_line *line;
  uint8_t staged[T64_VR4300_ICACHE_LINE_BYTES];
  uint32_t line_address;
  t64_status status;

  if (machine == NULL || opcode_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  line = &machine->cpu.icache[index];
  if (line->valid && line->physical_tag == tag) {
    *opcode_out = ((uint32_t)line->data[offset] << 24) |
                  ((uint32_t)line->data[offset + 1u] << 16) |
                  ((uint32_t)line->data[offset + 2u] << 8) |
                  (uint32_t)line->data[offset + 3u];
    return T64_STATUS_OK;
  }

  status = icache_stage_line(machine, physical_address, staged);
  if (status != T64_STATUS_OK) {
    return status;
  }
  line_address = tag;
  (void)memcpy(line->data, staged, sizeof line->data);
  line->physical_tag = line_address;
  line->valid = true;
  *opcode_out = ((uint32_t)line->data[offset] << 24) |
                ((uint32_t)line->data[offset + 1u] << 16) |
                ((uint32_t)line->data[offset + 2u] << 8) |
                (uint32_t)line->data[offset + 3u];
  return T64_STATUS_OK;
}

t64_status t64_cpu_icache_fill(t64_machine *machine,
                               uint64_t virtual_address,
                               uint32_t physical_address) {
  const uint32_t index = icache_index(virtual_address);
  const uint32_t tag = icache_physical_tag(physical_address);
  t64_vr4300_icache_line *line;
  uint8_t staged[T64_VR4300_ICACHE_LINE_BYTES];
  t64_status status;

  if (machine == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  status = icache_stage_line(machine, physical_address, staged);
  if (status != T64_STATUS_OK) {
    return status;
  }
  line = &machine->cpu.icache[index];
  (void)memcpy(line->data, staged, sizeof line->data);
  line->physical_tag = tag;
  line->valid = true;
  return T64_STATUS_OK;
}

void t64_cpu_icache_hit_invalidate(t64_machine *machine,
                                   uint64_t virtual_address,
                                   uint32_t physical_address) {
  const uint32_t index = icache_index(virtual_address);
  const uint32_t tag = icache_physical_tag(physical_address);
  t64_vr4300_icache_line *line;

  if (machine == NULL) {
    return;
  }
  line = &machine->cpu.icache[index];
  if (line->valid && line->physical_tag == tag) {
    line->valid = false;
  }
}

static uint32_t dcache_index(uint64_t virtual_address) {
  return (uint32_t)((virtual_address >> 4) &
                    (T64_VR4300_DCACHE_LINE_COUNT - 1u));
}

static uint32_t dcache_physical_tag(uint32_t physical_address) {
  return physical_address & ~T64_DCACHE_LINE_MASK;
}

static t64_status dcache_stage_line(
    const t64_machine *machine, uint32_t physical_address,
    uint8_t staged[T64_VR4300_DCACHE_LINE_BYTES]) {
  const uint32_t line_address = dcache_physical_tag(physical_address);
  const uint64_t physical_end =
      (uint64_t)line_address + T64_VR4300_DCACHE_LINE_BYTES;
  const t64_icache_backing_kind backing_kind =
      icache_backing_kind_at(machine, line_address);
  uint32_t offset;

  if (backing_kind == T64_ICACHE_BACKING_UNSUPPORTED) {
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  if (physical_end > UINT64_C(0x100000000)) {
    return T64_STATUS_BUS_RANGE;
  }
  for (offset = 0u; offset < T64_VR4300_DCACHE_LINE_BYTES; offset++) {
    const uint8_t *byte = NULL;
    t64_status status =
        icache_backing_byte(machine, line_address + offset, backing_kind,
                            &byte);
    if (status != T64_STATUS_OK || byte == NULL) {
      return status == T64_STATUS_OK ? T64_STATUS_BUS_UNSUPPORTED : status;
    }
  }
  for (offset = 0u; offset < T64_VR4300_DCACHE_LINE_BYTES; offset++) {
    const uint8_t *byte = NULL;
    t64_status status =
        icache_backing_byte(machine, line_address + offset, backing_kind,
                            &byte);
    if (status != T64_STATUS_OK || byte == NULL) {
      return status == T64_STATUS_OK ? T64_STATUS_BUS_UNSUPPORTED : status;
    }
    staged[offset] = *byte;
  }
  return T64_STATUS_OK;
}

static t64_status dcache_get_line(t64_machine *machine,
                                  uint64_t virtual_address,
                                  uint32_t physical_address,
                                  t64_vr4300_dcache_line **line_out) {
  const uint32_t index = dcache_index(virtual_address);
  const uint32_t tag = dcache_physical_tag(physical_address);
  t64_vr4300_dcache_line *line = &machine->cpu.dcache[index];
  uint8_t staged[T64_VR4300_DCACHE_LINE_BYTES];
  t64_status status;

  if (line->valid && line->physical_tag == tag) {
    *line_out = line;
    return T64_STATUS_OK;
  }
  status = dcache_stage_line(machine, physical_address, staged);
  if (status != T64_STATUS_OK) {
    return status;
  }
  if (line->valid && line->dirty) {
    status = t64_bus_validate_dcache_line_writeback(machine,
                                                    line->physical_tag);
    if (status != T64_STATUS_OK) {
      return status;
    }
    status = t64_bus_writeback_dcache_line(machine, line->physical_tag,
                                           line->data);
    if (status != T64_STATUS_OK) {
      return status;
    }
  }
  (void)memcpy(line->data, staged, sizeof line->data);
  line->physical_tag = tag;
  line->valid = true;
  line->dirty = false;
  *line_out = line;
  return T64_STATUS_OK;
}

static bool dcache_range_is_valid(uint32_t physical_address, uint32_t width,
                                  uint32_t *offset_out) {
  uint32_t offset = physical_address & T64_DCACHE_LINE_MASK;

  if ((width != 1u && width != 2u && width != 4u && width != 8u) ||
      offset + width > T64_VR4300_DCACHE_LINE_BYTES ||
      (uint64_t)physical_address + width > UINT64_C(0x100000000)) {
    return false;
  }
  *offset_out = offset;
  return true;
}

t64_status t64_cpu_dcache_read(t64_machine *machine,
                               uint64_t virtual_address,
                               uint32_t physical_address, uint32_t width,
                               uint64_t *value_out) {
  t64_vr4300_dcache_line *line = NULL;
  uint32_t offset;
  uint32_t index;
  uint64_t value = 0u;
  t64_status status;

  if (machine == NULL || value_out == NULL ||
      !dcache_range_is_valid(physical_address, width, &offset)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  status = dcache_get_line(machine, virtual_address, physical_address, &line);
  if (status != T64_STATUS_OK) {
    return status;
  }
  for (index = 0u; index < width; index++) {
    value = (value << 8) | line->data[offset + index];
  }
  *value_out = value;
  return T64_STATUS_OK;
}

t64_status t64_cpu_dcache_write(t64_machine *machine,
                                uint64_t virtual_address,
                                uint32_t physical_address, uint32_t width,
                                uint8_t byte_mask, uint64_t value) {
  const uint32_t line_address = dcache_physical_tag(physical_address);
  t64_vr4300_dcache_line *line = NULL;
  uint32_t offset;
  uint32_t index;
  uint32_t valid_mask;
  t64_status status;

  if (machine == NULL ||
      !dcache_range_is_valid(physical_address, width, &offset)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  valid_mask =
      width == 8u ? UINT32_C(0xff) : (UINT32_C(1) << width) - 1u;
  if (byte_mask == 0u || ((uint32_t)byte_mask & ~valid_mask) != 0u) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  /* A CPU store must not create an undrainable dirty line or evict another
   * line before the entire new backing destination has been validated. */
  status = t64_bus_validate_dcache_line_writeback(machine, line_address);
  if (status != T64_STATUS_OK) {
    return status;
  }
  status = dcache_get_line(machine, virtual_address, physical_address, &line);
  if (status != T64_STATUS_OK) {
    return status;
  }
  for (index = 0u; index < width; index++) {
    if (((uint32_t)byte_mask & (UINT32_C(1) << index)) != 0u) {
      const uint32_t shift = (width - index - 1u) * 8u;
      line->data[offset + index] = (uint8_t)(value >> shift);
      t64_cpu_link_invalidate_range(machine, physical_address + index, 1u);
    }
  }
  line->dirty = true;
  return T64_STATUS_OK;
}

t64_status t64_cpu_dcache_hit_writeback(t64_machine *machine,
                                        uint64_t virtual_address,
                                        uint32_t physical_address) {
  const uint32_t index = dcache_index(virtual_address);
  const uint32_t tag = dcache_physical_tag(physical_address);
  t64_vr4300_dcache_line *line;
  t64_status status;

  if (machine == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  line = &machine->cpu.dcache[index];
  if (!line->valid || line->physical_tag != tag || !line->dirty) {
    return T64_STATUS_OK;
  }
  status = t64_bus_validate_dcache_line_writeback(machine,
                                                  line->physical_tag);
  if (status != T64_STATUS_OK) {
    return status;
  }
  status = t64_bus_writeback_dcache_line(machine, line->physical_tag,
                                         line->data);
  if (status == T64_STATUS_OK) {
    line->dirty = false;
  }
  return status;
}

static uint32_t cache_tag_from_taglo(uint64_t virtual_address,
                                     uint32_t taglo,
                                     uint32_t line_bytes) {
  const uint32_t low_mask = UINT32_C(0x1000) - line_bytes;
  const uint32_t ptag =
      (taglo & T64_CACHE_TAGLO_PTAG_MASK) >> T64_CACHE_TAGLO_PTAG_SHIFT;

  return (ptag << 12) | ((uint32_t)virtual_address & low_mask);
}

static uint32_t taglo_from_cache_line(uint32_t physical_tag, uint32_t state) {
  return ((physical_tag >> 12) << T64_CACHE_TAGLO_PTAG_SHIFT) |
         (state << T64_CACHE_TAGLO_PSTATE_SHIFT);
}

static t64_status index_store_tag(t64_machine *machine,
                                  uint64_t virtual_address,
                                  uint32_t operation) {
  uint32_t taglo = (uint32_t)machine->cpu.cp0[T64_CACHE_CP0_TAGLO];
  uint32_t state =
      (taglo & T64_CACHE_TAGLO_PSTATE_MASK) >>
      T64_CACHE_TAGLO_PSTATE_SHIFT;

  if (operation == T64_CACHE_INDEX_STORE_TAG_I) {
    t64_vr4300_icache_line *line =
        &machine->cpu.icache[icache_index(virtual_address)];
    if (state != 0u && state != 2u) {
      return T64_STATUS_BUS_UNSUPPORTED;
    }
    line->physical_tag = cache_tag_from_taglo(
        virtual_address, taglo, T64_VR4300_ICACHE_LINE_BYTES);
    line->valid = state == 2u;
    return T64_STATUS_OK;
  }
  if (operation == T64_CACHE_INDEX_STORE_TAG_D) {
    t64_vr4300_dcache_line *line =
        &machine->cpu.dcache[dcache_index(virtual_address)];
    if (state == 1u) {
      return T64_STATUS_BUS_UNSUPPORTED;
    }
    line->physical_tag = cache_tag_from_taglo(
        virtual_address, taglo, T64_VR4300_DCACHE_LINE_BYTES);
    line->valid = state != 0u;
    line->dirty = state == 3u;
    return T64_STATUS_OK;
  }
  return T64_STATUS_INVALID_ARGUMENT;
}

t64_status t64_cpu_cache_operation(t64_machine *machine,
                                   uint32_t operation,
                                   uint64_t virtual_address,
                                   uint32_t physical_address) {
  t64_status status;

  if (machine == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  switch (operation) {
  case T64_CACHE_INDEX_INVALIDATE_I:
    machine->cpu.icache[icache_index(virtual_address)].valid = false;
    return T64_STATUS_OK;
  case T64_CACHE_INDEX_WRITEBACK_INVALIDATE_D: {
    t64_vr4300_dcache_line *line =
        &machine->cpu.dcache[dcache_index(virtual_address)];
    if (!line->valid) {
      return T64_STATUS_OK;
    }
    /* The primary table says any non-Invalid line is written back, including
     * a clean line; do not narrow this to the dirty-only hit operations. */
    status = t64_bus_writeback_cache_line(
        machine, line->physical_tag, line->data,
        T64_VR4300_DCACHE_LINE_BYTES);
    if (status != T64_STATUS_OK) {
      return status;
    }
    t64_cpu_link_invalidate_dcache_line(machine, line->physical_tag);
    line->valid = false;
    line->dirty = false;
    return T64_STATUS_OK;
  }
  case T64_CACHE_INDEX_LOAD_TAG_I: {
    const t64_vr4300_icache_line *line =
        &machine->cpu.icache[icache_index(virtual_address)];
    machine->cpu.cp0[T64_CACHE_CP0_TAGLO] = taglo_from_cache_line(
        line->physical_tag, line->valid ? 2u : 0u);
    machine->cpu.cp0[T64_CACHE_CP0_TAGHI] = 0u;
    return T64_STATUS_OK;
  }
  case T64_CACHE_INDEX_LOAD_TAG_D: {
    const t64_vr4300_dcache_line *line =
        &machine->cpu.dcache[dcache_index(virtual_address)];
    uint32_t state = !line->valid ? 0u : (line->dirty ? 3u : 2u);
    machine->cpu.cp0[T64_CACHE_CP0_TAGLO] =
        taglo_from_cache_line(line->physical_tag, state);
    machine->cpu.cp0[T64_CACHE_CP0_TAGHI] = 0u;
    return T64_STATUS_OK;
  }
  case T64_CACHE_INDEX_STORE_TAG_I:
  case T64_CACHE_INDEX_STORE_TAG_D:
    return index_store_tag(machine, virtual_address, operation);
  case T64_CACHE_CREATE_DIRTY_EXCLUSIVE_D: {
    t64_vr4300_dcache_line *line =
        &machine->cpu.dcache[dcache_index(virtual_address)];
    const uint32_t tag = dcache_physical_tag(physical_address);
    if (!line->valid || line->physical_tag != tag) {
      if (line->valid && line->dirty) {
        status = t64_bus_validate_dcache_line_writeback(
            machine, line->physical_tag);
        if (status != T64_STATUS_OK) {
          return status;
        }
        status = t64_bus_writeback_dcache_line(
            machine, line->physical_tag, line->data);
        if (status != T64_STATUS_OK) {
          return status;
        }
      }
      line->physical_tag = tag;
    }
    line->valid = true;
    line->dirty = true;
    return T64_STATUS_OK;
  }
  case T64_CACHE_HIT_INVALIDATE_I: {
    t64_vr4300_icache_line *line =
        &machine->cpu.icache[icache_index(virtual_address)];
    if (line->valid &&
        line->physical_tag == icache_physical_tag(physical_address)) {
      line->valid = false;
    }
    return T64_STATUS_OK;
  }
  case T64_CACHE_HIT_INVALIDATE_D: {
    t64_vr4300_dcache_line *line =
        &machine->cpu.dcache[dcache_index(virtual_address)];
    if (line->valid &&
        line->physical_tag == dcache_physical_tag(physical_address)) {
      line->valid = false;
      line->dirty = false;
      t64_cpu_link_invalidate_dcache_line(machine, physical_address);
    }
    return T64_STATUS_OK;
  }
  case T64_CACHE_FILL_I:
    return t64_cpu_icache_fill(machine, virtual_address, physical_address);
  case T64_CACHE_HIT_WRITEBACK_INVALIDATE_D: {
    t64_vr4300_dcache_line *line =
        &machine->cpu.dcache[dcache_index(virtual_address)];
    if (!line->valid ||
        line->physical_tag != dcache_physical_tag(physical_address)) {
      return T64_STATUS_OK;
    }
    if (line->dirty) {
      status = t64_bus_validate_dcache_line_writeback(
          machine, line->physical_tag);
      if (status != T64_STATUS_OK) {
        return status;
      }
      status = t64_bus_writeback_dcache_line(
          machine, line->physical_tag, line->data);
      if (status != T64_STATUS_OK) {
        return status;
      }
    }
    t64_cpu_link_invalidate_dcache_line(machine, physical_address);
    line->valid = false;
    line->dirty = false;
    return T64_STATUS_OK;
  }
  case T64_CACHE_HIT_WRITEBACK_I: {
    t64_vr4300_icache_line *line =
        &machine->cpu.icache[icache_index(virtual_address)];
    if (!line->valid ||
        line->physical_tag != icache_physical_tag(physical_address)) {
      return T64_STATUS_OK;
    }
    status = t64_bus_validate_cache_writeback(
        machine, line->physical_tag, T64_VR4300_ICACHE_LINE_BYTES);
    if (status != T64_STATUS_OK) {
      return status;
    }
    return t64_bus_writeback_cache_line(
        machine, line->physical_tag, line->data,
        T64_VR4300_ICACHE_LINE_BYTES);
  }
  case T64_CACHE_HIT_WRITEBACK_D:
    return t64_cpu_dcache_hit_writeback(machine, virtual_address,
                                        physical_address);
  default:
    return T64_STATUS_INVALID_ARGUMENT;
  }
}
