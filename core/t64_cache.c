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
