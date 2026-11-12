#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  bool recognized;
  bool load;
  bool store;
  bool partial_left;
  bool partial_right;
  bool sign_extend;
  bool reverse_endian;
  uint32_t destination;
  uint32_t width;
  uint32_t bus_width;
  uint64_t virtual_address;
  uint32_t physical_address;
  uint8_t byte_mask;
  uint64_t bus_value;
  uint64_t result_value;
} t64_staged_memory_effect;

#define T64_STATUS_EXL UINT32_C(0x00000002)
#define T64_STATUS_ERL UINT32_C(0x00000004)
#define T64_STATUS_KSU UINT32_C(0x00000018)
#define T64_STATUS_USER UINT32_C(0x00000010)
#define T64_STATUS_RE UINT32_C(0x02000000)
#define T64_CONFIG_BE UINT32_C(0x00008000)

static bool reverse_endian_active(const t64_machine *machine) {
  uint32_t status = (uint32_t)machine->cpu.cp0[12];
  uint32_t config = (uint32_t)machine->cpu.cp0[16];

  return (status & (T64_STATUS_EXL | T64_STATUS_ERL)) == 0u &&
         (status & T64_STATUS_KSU) == T64_STATUS_USER &&
         (status & T64_STATUS_RE) != 0u && (config & T64_CONFIG_BE) != 0u;
}

/* In BE mode the VR4300's RE transform selects a byte lane in the physical
 * 64-bit data container. The bus exposes those canonical physical lanes
 * directly, so apply the address XOR here and keep the transferred bytes in
 * big-endian physical order. */

static uint64_t sign_extend_16(uint16_t value) {
  return (value & UINT16_C(0x8000)) != 0u
             ? UINT64_C(0xffffffffffff0000) | (uint64_t)value
             : (uint64_t)value;
}

static uint64_t sign_extend_32(uint32_t value) {
  return (value & UINT32_C(0x80000000)) != 0u
             ? UINT64_C(0xffffffff00000000) | (uint64_t)value
             : (uint64_t)value;
}

static bool decode_memory_effect(t64_machine *machine, uint32_t opcode,
                                 t64_staged_memory_effect *effect_out) {
  uint32_t operation = opcode >> 26;
  uint32_t rs = (opcode >> 21) & UINT32_C(0x1f);
  uint32_t rt = (opcode >> 16) & UINT32_C(0x1f);
  uint16_t immediate = (uint16_t)opcode;
  uint64_t address;

  if (machine == NULL || effect_out == NULL) {
    return false;
  }
  *effect_out = (t64_staged_memory_effect){0};
  effect_out->destination = rt;
  switch (operation) {
  case 0x1au: effect_out->width = 8u; effect_out->load = true;
    effect_out->partial_left = true; break;
  case 0x1bu: effect_out->width = 8u; effect_out->load = true;
    effect_out->partial_right = true; break;
  case 0x20u: effect_out->width = 1u; effect_out->load = true;
    effect_out->sign_extend = true; break;
  case 0x21u: effect_out->width = 2u; effect_out->load = true;
    effect_out->sign_extend = true; break;
  case 0x22u: effect_out->width = 4u; effect_out->load = true;
    effect_out->partial_left = true; break;
  case 0x23u: effect_out->width = 4u; effect_out->load = true;
    effect_out->sign_extend = true; break;
  case 0x24u: effect_out->width = 1u; effect_out->load = true; break;
  case 0x25u: effect_out->width = 2u; effect_out->load = true; break;
  case 0x26u: effect_out->width = 4u; effect_out->load = true;
    effect_out->partial_right = true; break;
  case 0x27u: effect_out->width = 4u; effect_out->load = true; break;
  case 0x28u: effect_out->width = 1u; effect_out->store = true; break;
  case 0x29u: effect_out->width = 2u; effect_out->store = true; break;
  case 0x2au: effect_out->width = 4u; effect_out->store = true;
    effect_out->partial_left = true; break;
  case 0x2bu: effect_out->width = 4u; effect_out->store = true; break;
  case 0x2cu: effect_out->width = 8u; effect_out->store = true;
    effect_out->partial_left = true; break;
  case 0x2du: effect_out->width = 8u; effect_out->store = true;
    effect_out->partial_right = true; break;
  case 0x2eu: effect_out->width = 4u; effect_out->store = true;
    effect_out->partial_right = true; break;
  case 0x37u: effect_out->width = 8u; effect_out->load = true; break;
  case 0x3fu: effect_out->width = 8u; effect_out->store = true; break;
  default: return false;
  }
  effect_out->recognized = true;
  address = machine->cpu.gpr[rs] + sign_extend_16(immediate);
  effect_out->virtual_address = address;
  return true;
}

static bool translate_effect(t64_machine *machine,
                             t64_staged_memory_effect *effect) {
  t64_mmu_request request;
  t64_mmu_result translation = {0};
  bool partial = effect->partial_left || effect->partial_right;

  request.virtual_address = effect->virtual_address;
  request.width = partial ? 1u : effect->width;
  request.kind = effect->store ? T64_MMU_ACCESS_WRITE : T64_MMU_ACCESS_READ;
  if (!t64_mmu_translate(machine, &request, &translation) ||
      translation.fault != T64_MMU_FAULT_NONE || !translation.translated ||
      translation.physical_address > UINT32_MAX) {
    return false;
  }
  effect->physical_address = (uint32_t)translation.physical_address;
  if (partial) {
    effect->bus_width = effect->reverse_endian ? 8u : effect->width;
    effect->physical_address &= ~(effect->bus_width - 1u);
  } else {
    effect->bus_width = effect->width;
    if (effect->reverse_endian) {
      uint32_t lane_xor = 8u - effect->width;
      effect->physical_address =
          (effect->physical_address & ~UINT32_C(7)) |
          ((effect->physical_address & UINT32_C(7)) ^ lane_xor);
    }
  }
  return true;
}

static uint64_t read_bus_value(t64_machine *machine, uint32_t address,
                               uint32_t width, bool *okay_out) {
  uint64_t value = 0u;
  uint32_t part = 0u;
  t64_status status;

  *okay_out = false;
  if (width == 8u) {
    status = t64_bus_read_wide(machine, address, &value);
  } else {
    status = t64_bus_read(machine, address, width, &part);
    value = (uint64_t)part;
  }
  if (status == T64_STATUS_OK) {
    *okay_out = true;
  }
  return value;
}

static uint64_t byte_at(uint64_t value, uint32_t width, uint32_t index) {
  uint32_t shift = (width - index - 1u) * 8u;
  return (value >> shift) & UINT64_C(0xff);
}

static uint64_t merge_load(uint64_t memory, uint64_t previous,
                           uint32_t width, uint32_t offset, bool left,
                           bool little_endian) {
  uint64_t result = previous &
                    (width == 4u ? UINT64_C(0xffffffff) : UINT64_MAX);
  uint32_t count = little_endian ? (left ? offset + 1u : width - offset)
                                 : (left ? width - offset : offset + 1u);
  uint32_t index;

  for (index = 0u; index < count; index++) {
    uint32_t memory_index;
    uint32_t register_index;
    uint32_t shift;
    uint64_t lane_mask;

    if (little_endian) {
      memory_index = left ? index : offset + index;
      register_index = left ? offset - index : width - 1u - index;
    } else {
      memory_index = left ? offset + index : index;
      register_index = left ? index : width - count + index;
    }
    shift = (width - register_index - 1u) * 8u;
    lane_mask = UINT64_C(0xff) << shift;

    result = (result & ~lane_mask) | (byte_at(memory, width, memory_index)
                                      << shift);
  }
  return result;
}

static uint32_t reverse_merge_location(uint32_t width, uint32_t offset,
                                       bool left, uint32_t *count_out,
                                       uint32_t *register_start_out) {
  uint32_t base;

  if (width == 4u) {
    uint32_t within = offset & 3u;
    base = offset < 4u ? 4u : 0u;
    *count_out = left ? within + 1u : 4u - within;
    *register_start_out = left ? 0u : 4u - *count_out;
    return left ? base + 4u - *count_out : base;
  }

  *count_out = left ? offset + 1u : 8u - offset;
  *register_start_out = left ? 0u : 8u - *count_out;
  return left ? 8u - *count_out : 0u;
}

static uint64_t merge_load_reverse(uint64_t memory, uint64_t previous,
                                   uint32_t width, uint32_t offset,
                                   bool left) {
  uint32_t count;
  uint32_t register_start;
  uint32_t memory_start = reverse_merge_location(
      width, offset, left, &count, &register_start);
  uint64_t result = width == 4u ? (uint32_t)previous : previous;
  uint32_t index;

  for (index = 0u; index < count; index++) {
    uint32_t register_index = register_start + index;
    uint32_t shift = (width - register_index - 1u) * 8u;
    uint64_t lane_mask = UINT64_C(0xff) << shift;

    result = (result & ~lane_mask) |
             (byte_at(memory, 8u, memory_start + index) << shift);
  }
  return result;
}

static uint8_t merge_store_mask_reverse(uint32_t width, uint32_t offset,
                                        bool left) {
  uint32_t count;
  uint32_t register_start;
  uint32_t memory_start = reverse_merge_location(
      width, offset, left, &count, &register_start);
  uint8_t mask = 0u;
  uint32_t index;

  (void)register_start;
  for (index = 0u; index < count; index++) {
    mask = (uint8_t)(mask | (uint8_t)(UINT8_C(1) << (memory_start + index)));
  }
  return mask;
}

static uint64_t merge_store_value_reverse(uint64_t source, uint32_t width,
                                          uint32_t offset, bool left) {
  uint32_t count;
  uint32_t register_start;
  uint32_t memory_start = reverse_merge_location(
      width, offset, left, &count, &register_start);
  uint64_t value = 0u;
  uint32_t index;

  for (index = 0u; index < count; index++) {
    uint32_t memory_index = memory_start + index;
    uint32_t shift = (8u - memory_index - 1u) * 8u;

    value |= byte_at(source, width, register_start + index) << shift;
  }
  return value;
}

static uint8_t merge_store_mask(uint32_t width, uint32_t offset, bool left,
                                bool little_endian) {
  uint32_t count = little_endian ? (left ? offset + 1u : width - offset)
                                 : (left ? width - offset : offset + 1u);
  uint8_t mask = 0u;
  uint32_t index;

  for (index = 0u; index < count; index++) {
    uint32_t memory_index = little_endian
                                ? (left ? index : offset + index)
                                : (left ? offset + index : index);
    mask = (uint8_t)(mask | (uint8_t)(UINT8_C(1) << memory_index));
  }
  return mask;
}

static uint64_t merge_store_value(uint64_t source, uint32_t width,
                                  uint32_t offset, bool left,
                                  bool little_endian) {
  uint32_t count = little_endian ? (left ? offset + 1u : width - offset)
                                 : (left ? width - offset : offset + 1u);
  uint64_t value = 0u;
  uint32_t index;

  for (index = 0u; index < count; index++) {
    uint32_t memory_index;
    uint32_t register_index;
    uint32_t shift;

    if (little_endian) {
      memory_index = left ? index : offset + index;
      register_index = left ? offset - index : width - 1u - index;
    } else {
      memory_index = left ? offset + index : index;
      register_index = left ? index : width - count + index;
    }
    shift = (width - memory_index - 1u) * 8u;

    value |= byte_at(source, width, register_index) << shift;
  }
  return value;
}

static bool stage_memory_effect(t64_machine *machine, uint32_t opcode,
                                t64_staged_memory_effect *effect) {
  uint32_t rt = (opcode >> 16) & UINT32_C(0x1f);
  bool okay;

  if (!decode_memory_effect(machine, opcode, effect)) {
    return false;
  }
  effect->reverse_endian = reverse_endian_active(machine);
  if (!translate_effect(machine, effect)) {
    return false;
  }
  if (effect->load) {
    uint32_t offset =
        (uint32_t)effect->virtual_address & (effect->width - 1u);
    bool partial = effect->partial_left || effect->partial_right;

    effect->byte_mask = !partial
        ? (uint8_t)((UINT32_C(1) << effect->width) - 1u)
        : effect->reverse_endian
              ? merge_store_mask_reverse(
                    effect->width,
                    (uint32_t)effect->virtual_address & 7u,
                    effect->partial_left)
              : merge_store_mask(effect->width, offset,
                                 effect->partial_left, false);

    effect->bus_value = read_bus_value(machine, effect->physical_address,
                                       effect->bus_width, &okay);
    if (!okay) {
      return false;
    }
    if (effect->partial_left || effect->partial_right) {
      effect->result_value = effect->reverse_endian
          ? merge_load_reverse(effect->bus_value, machine->cpu.gpr[rt],
                               effect->width,
                               (uint32_t)effect->virtual_address & 7u,
                               effect->partial_left)
          : merge_load(effect->bus_value, machine->cpu.gpr[rt], effect->width,
                       offset, effect->partial_left, false);
    } else {
      effect->result_value = effect->bus_value;
      if (effect->sign_extend && effect->width == 1u &&
          (effect->bus_value & UINT64_C(0x80)) != 0u) {
        effect->result_value |= UINT64_C(0xffffffffffffff00);
      } else if (effect->sign_extend && effect->width == 2u &&
                 (effect->bus_value & UINT64_C(0x8000)) != 0u) {
        effect->result_value |= UINT64_C(0xffffffffffff0000);
      } else if (effect->sign_extend && effect->width == 4u) {
        effect->result_value = sign_extend_32((uint32_t)effect->bus_value);
      }
    }
    if (effect->width == 4u && (effect->partial_left || effect->partial_right)) {
      effect->result_value = sign_extend_32((uint32_t)effect->result_value);
    }
    return true;
  }

  if (effect->store) {
    uint64_t source = machine->cpu.gpr[rt];
    uint32_t offset =
        (uint32_t)effect->virtual_address & (effect->width - 1u);

    if (effect->partial_left || effect->partial_right) {
      effect->byte_mask = effect->reverse_endian
          ? merge_store_mask_reverse(
                effect->width,
                (uint32_t)effect->virtual_address & 7u,
                effect->partial_left)
          : merge_store_mask(effect->width, offset, effect->partial_left,
                             false);
      effect->bus_value = effect->reverse_endian
          ? merge_store_value_reverse(
                source, effect->width,
                (uint32_t)effect->virtual_address & 7u,
                effect->partial_left)
          : merge_store_value(source, effect->width, offset,
                              effect->partial_left, false);
    } else {
      effect->byte_mask = (uint8_t)((UINT32_C(1) << effect->width) - 1u);
      effect->bus_value = source;
    }
    return true;
  }
  return false;
}

static bool commit_memory_effect(t64_machine *machine,
                                 t64_staged_memory_effect *effect) {
  t64_status status;

  if (effect->store) {
    if (effect->bus_width <= 4u && effect->byte_mask ==
                                        (uint8_t)((UINT32_C(1) << effect->width) -
                                                  1u)) {
      status = t64_bus_write(machine, effect->physical_address,
                             effect->width, (uint32_t)effect->bus_value);
    } else {
      status = t64_bus_write_masked(machine, effect->physical_address,
                                    effect->bus_width, effect->byte_mask,
                                    effect->bus_value);
    }
    return status == T64_STATUS_OK;
  }
  return effect->load;
}

void t64_cpu_link_invalidate_range(t64_machine *machine,
                                   uint32_t physical_address,
                                   uint32_t width) {
  uint64_t write_start = (uint64_t)physical_address;
  uint64_t write_end = write_start + (uint64_t)width;
  uint64_t link_start;
  uint64_t link_end;

  if (machine == NULL || width == 0u || !machine->cpu.linked_valid) {
    return;
  }
  link_start = (uint64_t)machine->cpu.linked_physical_address;
  link_end = link_start + (uint64_t)machine->cpu.linked_width;
  if (write_start < link_end && link_start < write_end) {
    machine->cpu.linked_valid = false;
    machine->cpu.linked_physical_address = 0u;
    machine->cpu.linked_width = 0u;
  }
}

static bool execute_linked_memory(t64_machine *machine, uint32_t opcode,
                                  bool *write_gpr_out,
                                  uint32_t *destination_out,
                                  uint64_t *value_out) {
  uint32_t operation = opcode >> 26;
  uint32_t rs = (opcode >> 21) & UINT32_C(0x1f);
  uint32_t rt = (opcode >> 16) & UINT32_C(0x1f);
  uint32_t width;
  uint16_t immediate = (uint16_t)opcode;
  bool load;
  uint64_t virtual_address;
  uint32_t physical_address;
  t64_mmu_request request;
  t64_mmu_result translation = {0};
  bool okay;

  *write_gpr_out = false;
  *destination_out = 0u;
  *value_out = 0u;
  switch (operation) {
  case 0x30u: width = 4u; load = true; break;
  case 0x34u: width = 8u; load = true; break;
  case 0x38u: width = 4u; load = false; break;
  case 0x3cu: width = 8u; load = false; break;
  default: return false;
  }

  virtual_address = machine->cpu.gpr[rs] + sign_extend_16(immediate);
  request.virtual_address = virtual_address;
  request.width = width;
  request.kind = load ? T64_MMU_ACCESS_READ : T64_MMU_ACCESS_WRITE;
  if (!t64_mmu_translate(machine, &request, &translation) ||
      translation.fault != T64_MMU_FAULT_NONE || !translation.translated ||
      !translation.cacheable || translation.physical_address > UINT32_MAX) {
    return false;
  }
  physical_address = (uint32_t)translation.physical_address;
  if (reverse_endian_active(machine)) {
    uint32_t lane_xor = 8u - width;
    physical_address = (physical_address & ~UINT32_C(7)) |
                       ((physical_address & UINT32_C(7)) ^ lane_xor);
  }

  if (load) {
    *value_out = read_bus_value(machine, physical_address, width, &okay);
    if (!okay) {
      return false;
    }
    if (width == 4u) {
      *value_out = sign_extend_32((uint32_t)*value_out);
    }
    machine->cpu.linked_physical_address = physical_address;
    machine->cpu.linked_width = width;
    machine->cpu.linked_valid = true;
    machine->cpu.cp0[17] = (uint64_t)(physical_address >> 4);
    *write_gpr_out = true;
    *destination_out = rt;
    return true;
  }

  {
    bool success = machine->cpu.linked_valid &&
                   machine->cpu.linked_physical_address == physical_address &&
                   machine->cpu.linked_width == width;
    uint64_t source = machine->cpu.gpr[rt];
    t64_status status = T64_STATUS_OK;

    machine->cpu.linked_valid = false;
    machine->cpu.linked_physical_address = 0u;
    machine->cpu.linked_width = 0u;
    if (success) {
      status = width == 4u
                   ? t64_bus_write(machine, physical_address, 4u,
                                   (uint32_t)source)
                   : t64_bus_write_masked(machine, physical_address, 8u,
                                          UINT8_C(0xff), source);
      if (status != T64_STATUS_OK) {
        return false;
      }
    }
    *write_gpr_out = true;
    *destination_out = rt;
    *value_out = success ? UINT64_C(1) : UINT64_C(0);
  }
  return true;
}

bool t64_memory_execute(t64_machine *machine, uint32_t opcode,
                        bool *write_gpr_out, uint32_t *destination_out,
                        uint64_t *value_out) {
  t64_staged_memory_effect effect;
  uint32_t operation = opcode >> 26;

  if (operation == 0x30u || operation == 0x34u || operation == 0x38u ||
      operation == 0x3cu) {
    if (machine == NULL || write_gpr_out == NULL ||
        destination_out == NULL || value_out == NULL) {
      return false;
    }
    return execute_linked_memory(machine, opcode, write_gpr_out,
                                 destination_out, value_out);
  }
  if (write_gpr_out == NULL || destination_out == NULL || value_out == NULL ||
      !stage_memory_effect(machine, opcode, &effect) ||
      !commit_memory_effect(machine, &effect)) {
    return false;
  }
  if (effect.load) {
    *write_gpr_out = true;
    *destination_out = effect.destination;
    *value_out = effect.result_value;
  }
  return true;
}
