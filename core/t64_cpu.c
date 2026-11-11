#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>

#define T64_CPU_REGIMM UINT32_C(0x01)
#define T64_CPU_J UINT32_C(0x02)
#define T64_CPU_JAL UINT32_C(0x03)
#define T64_CPU_BEQ UINT32_C(0x04)
#define T64_CPU_BNE UINT32_C(0x05)
#define T64_CPU_BLEZ UINT32_C(0x06)
#define T64_CPU_BGTZ UINT32_C(0x07)
#define T64_CPU_BEQL UINT32_C(0x14)
#define T64_CPU_BNEL UINT32_C(0x15)
#define T64_CPU_BLEZL UINT32_C(0x16)
#define T64_CPU_BGTZL UINT32_C(0x17)

static uint64_t sign_extend_32(uint32_t value) {
  if ((value & UINT32_C(0x80000000)) != 0u) {
    return UINT64_C(0xffffffff00000000) | (uint64_t)value;
  }
  return (uint64_t)value;
}

static uint64_t sign_extend_16(uint16_t value) {
  if ((value & UINT16_C(0x8000)) != 0u) {
    return UINT64_C(0xffffffffffff0000) | (uint64_t)value;
  }
  return (uint64_t)value;
}

static uint32_t sign_extend_16_32(uint16_t value) {
  if ((value & UINT16_C(0x8000)) != 0u) {
    return UINT32_C(0xffff0000) | (uint32_t)value;
  }
  return (uint32_t)value;
}

static bool signed_less_64(uint64_t left, uint64_t right) {
  bool left_negative = (left >> 63) != 0u;
  bool right_negative = (right >> 63) != 0u;

  if (left_negative != right_negative) {
    return left_negative;
  }
  return left < right;
}

static uint32_t arithmetic_shift_right_32(uint32_t value, uint32_t amount) {
  uint32_t shifted;

  if (amount == 0u) {
    return value;
  }
  shifted = value >> amount;
  if ((value & UINT32_C(0x80000000)) != 0u) {
    shifted |= UINT32_MAX << (32u - amount);
  }
  return shifted;
}

static uint64_t arithmetic_shift_right_64(uint64_t value, uint32_t amount) {
  uint64_t shifted;

  if (amount == 0u) {
    return value;
  }
  shifted = value >> amount;
  if ((value & UINT64_C(0x8000000000000000)) != 0u) {
    shifted |= UINT64_MAX << (64u - amount);
  }
  return shifted;
}

static bool add_overflow_32(uint32_t left, uint32_t right,
                            uint32_t result) {
  return ((~(left ^ right) & (left ^ result)) & UINT32_C(0x80000000)) != 0u;
}

static bool subtract_overflow_32(uint32_t left, uint32_t right,
                                 uint32_t result) {
  return (((left ^ right) & (left ^ result)) & UINT32_C(0x80000000)) != 0u;
}

static bool add_overflow_64(uint64_t left, uint64_t right,
                            uint64_t result) {
  return ((~(left ^ right) & (left ^ result)) &
          UINT64_C(0x8000000000000000)) != 0u;
}

static bool subtract_overflow_64(uint64_t left, uint64_t right,
                                 uint64_t result) {
  return (((left ^ right) & (left ^ result)) &
          UINT64_C(0x8000000000000000)) != 0u;
}

bool t64_cpu_translate(uint64_t virtual_address,
                       uint32_t *physical_address_out) {
  uint32_t address32;
  uint64_t canonical_address;

  if (physical_address_out == NULL) {
    return false;
  }
  address32 = (uint32_t)virtual_address;
  canonical_address = (uint64_t)address32;
  if ((address32 & UINT32_C(0x80000000)) != 0u) {
    canonical_address |= UINT64_C(0xffffffff00000000);
  }
  if (virtual_address != canonical_address ||
      address32 < UINT32_C(0x80000000) ||
      address32 >= UINT32_C(0xc0000000)) {
    return false;
  }
  *physical_address_out = address32 & UINT32_C(0x1fffffff);
  return true;
}

static bool cpu_read_bytes(t64_machine *machine, uint64_t virtual_address,
                           uint8_t *bytes_out, uint32_t width) {
  uint32_t physical_address;
  uint32_t index;

  if (machine == NULL || bytes_out == NULL || width == 0u ||
      !t64_cpu_translate(virtual_address, &physical_address) ||
      (uint64_t)physical_address + (uint64_t)width >
          UINT64_C(0x100000000)) {
    return false;
  }
  for (index = 0u; index < width; index++) {
    uint32_t byte_value = 0u;
    t64_status status = t64_bus_read(machine, physical_address + index, 1u,
                                     &byte_value);

    if (status != T64_STATUS_OK) {
      return false;
    }
    bytes_out[index] = (uint8_t)byte_value;
  }
  return true;
}

static bool cpu_read_scalar(t64_machine *machine, uint64_t virtual_address,
                            uint32_t width, uint64_t *value_out) {
  uint32_t physical_address;

  if (machine == NULL || value_out == NULL ||
      !t64_cpu_translate(virtual_address, &physical_address) ||
      (uint64_t)physical_address + (uint64_t)width >
          UINT64_C(0x100000000)) {
    return false;
  }
  if (width == 1u || width == 4u) {
    uint32_t value = 0u;
    t64_status status = t64_bus_read(machine, physical_address, width, &value);

    if (status != T64_STATUS_OK) {
      return false;
    }
    *value_out = (uint64_t)value;
    return true;
  }
  if (width == 2u || width == 8u) {
    uint8_t bytes[8];
    uint32_t index;
    uint64_t value = 0u;

    if (!cpu_read_bytes(machine, virtual_address, bytes, width)) {
      return false;
    }
    for (index = 0u; index < width; index++) {
      value = (value << 8) | (uint64_t)bytes[index];
    }
    *value_out = value;
    return true;
  }
  return false;
}

static bool cpu_write_bytes(t64_machine *machine, uint64_t virtual_address,
                            const uint8_t *bytes, uint32_t width) {
  uint32_t physical_address;
  uint32_t index;

  if (machine == NULL || bytes == NULL || width == 0u ||
      !t64_cpu_translate(virtual_address, &physical_address) ||
      (uint64_t)physical_address + (uint64_t)width >
          UINT64_C(0x100000000)) {
    return false;
  }
  /* Validate the complete byte range before the first write. */
  for (index = 0u; index < width; index++) {
    uint32_t ignored = 0u;

    if (t64_bus_read(machine, physical_address + index, 1u, &ignored) !=
        T64_STATUS_OK) {
      return false;
    }
  }
  for (index = 0u; index < width; index++) {
    if (t64_bus_write(machine, physical_address + index, 1u,
                      (uint32_t)bytes[index]) != T64_STATUS_OK) {
      return false;
    }
  }
  return true;
}

static bool cpu_write_scalar(t64_machine *machine, uint64_t virtual_address,
                             uint32_t width, uint64_t value) {
  uint32_t physical_address;

  if (machine == NULL ||
      !t64_cpu_translate(virtual_address, &physical_address) ||
      (uint64_t)physical_address + (uint64_t)width >
          UINT64_C(0x100000000)) {
    return false;
  }
  if (width == 1u || width == 4u) {
    return t64_bus_write(machine, physical_address, width, (uint32_t)value) ==
           T64_STATUS_OK;
  }
  if (width == 2u || width == 8u) {
    uint8_t bytes[8];
    uint32_t index;

    for (index = 0u; index < width; index++) {
      uint32_t shift = (width - index - 1u) * 8u;
      bytes[index] = (uint8_t)(value >> shift);
    }
    return cpu_write_bytes(machine, virtual_address, bytes, width);
  }
  return false;
}

static uint64_t effective_address(const t64_cpu_state *cpu, uint32_t rs,
                                  uint16_t immediate) {
  return cpu->gpr[rs] + sign_extend_16(immediate);
}

static uint64_t bytes_to_u64(const uint8_t *bytes, uint32_t width) {
  uint32_t index;
  uint64_t value = 0u;

  for (index = 0u; index < width; index++) {
    value = (value << 8) | (uint64_t)bytes[index];
  }
  return value;
}

static void u64_to_bytes(uint64_t value, uint8_t *bytes, uint32_t width) {
  uint32_t index;

  for (index = 0u; index < width; index++) {
    uint32_t shift = (width - index - 1u) * 8u;
    bytes[index] = (uint8_t)(value >> shift);
  }
}

static bool load_partial(t64_machine *machine, uint64_t address,
                         uint32_t width, bool left, uint64_t previous,
                         uint64_t *result_out) {
  uint8_t memory[8];
  uint8_t result[8];
  uint32_t offset = (uint32_t)address & (width - 1u);
  uint32_t count = left ? width - offset : offset + 1u;
  uint32_t memory_start = left ? 0u : width - count;
  uint32_t register_start = memory_start;
  uint32_t index;
  uint64_t aligned_address = (address & ~((uint64_t)width - 1u));

  if (!cpu_read_bytes(machine, aligned_address, memory, width)) {
    return false;
  }
  u64_to_bytes(previous, result, width);
  for (index = 0u; index < count; index++) {
    result[register_start + index] = memory[memory_start + index];
  }
  *result_out = bytes_to_u64(result, width);
  return true;
}

static bool store_partial(t64_machine *machine, uint64_t address,
                          uint32_t width, bool left, uint64_t value) {
  uint8_t memory[8];
  uint8_t source[8];
  uint32_t offset = (uint32_t)address & (width - 1u);
  uint32_t count = left ? width - offset : offset + 1u;
  uint32_t memory_start = left ? 0u : width - count;
  uint32_t source_start = memory_start;
  uint64_t aligned_address = (address & ~((uint64_t)width - 1u));
  uint32_t index;

  u64_to_bytes(value, source, width);
  if (!cpu_read_bytes(machine, aligned_address, memory, width)) {
    return false;
  }
  for (index = 0u; index < count; index++) {
    memory[memory_start + index] = source[source_start + index];
  }
  return cpu_write_bytes(machine, aligned_address, memory, width);
}

static bool branch_condition(uint32_t opcode, const t64_cpu_state *cpu,
                             bool *likely_out, bool *link_out,
                             uint32_t *link_register_out) {
  uint32_t operation = opcode >> 26;
  uint32_t rs = (opcode >> 21) & UINT32_C(0x1f);
  uint32_t rt = (opcode >> 16) & UINT32_C(0x1f);
  uint64_t left = cpu->gpr[rs];
  uint64_t right = cpu->gpr[rt];

  *likely_out = false;
  *link_out = false;
  *link_register_out = 0u;
  switch (operation) {
  case T64_CPU_BEQ:
  case T64_CPU_BEQL:
  case T64_CPU_BNE:
  case T64_CPU_BNEL:
    *likely_out = operation == T64_CPU_BEQL || operation == T64_CPU_BNEL;
    return operation == T64_CPU_BEQ || operation == T64_CPU_BEQL
               ? left == right
               : left != right;
  case T64_CPU_BLEZ:
  case T64_CPU_BLEZL:
    if (rt != 0u) {
      return false;
    }
    *likely_out = operation == T64_CPU_BLEZL;
    return !signed_less_64(0u, left);
  case T64_CPU_BGTZ:
  case T64_CPU_BGTZL:
    if (rt != 0u) {
      return false;
    }
    *likely_out = operation == T64_CPU_BGTZL;
    return signed_less_64(0u, left);
  case T64_CPU_REGIMM:
    switch (rt) {
    case 0u: /* BLTZ */
      return signed_less_64(left, 0u);
    case 1u: /* BGEZ */
      return !signed_less_64(left, 0u);
    case 2u: /* BLTZL */
      *likely_out = true;
      return signed_less_64(left, 0u);
    case 3u: /* BGEZL */
      *likely_out = true;
      return !signed_less_64(left, 0u);
    case 16u: /* BLTZAL */
    case 17u: /* BGEZAL */
    case 18u: /* BLTZALL */
    case 19u: /* BGEZALL */
      *link_out = true;
      *link_register_out = 31u;
      *likely_out = rt >= 18u;
      return (rt == 16u || rt == 18u)
                 ? signed_less_64(left, 0u)
                 : !signed_less_64(left, 0u);
    default:
      return false;
    }
  default:
    return false;
  }
}

static bool is_branch_instruction(uint32_t opcode) {
  uint32_t operation = opcode >> 26;

  if (operation == T64_CPU_REGIMM || operation == T64_CPU_J ||
      operation == T64_CPU_JAL || operation == T64_CPU_BEQ ||
      operation == T64_CPU_BNE || operation == T64_CPU_BLEZ ||
      operation == T64_CPU_BGTZ || operation == T64_CPU_BEQL ||
      operation == T64_CPU_BNEL || operation == T64_CPU_BLEZL ||
      operation == T64_CPU_BGTZL) {
    return true;
  }
  return operation == 0u &&
         (((opcode & UINT32_C(0x3f)) == 8u) ||
          ((opcode & UINT32_C(0x3f)) == 9u));
}

static bool execute_branch(uint32_t pc, uint32_t opcode,
                           const t64_cpu_state *cpu, bool *taken_out,
                           bool *likely_out, bool *link_out,
                           uint32_t *link_register_out,
                           uint32_t *target_out) {
  uint32_t operation = opcode >> 26;
  uint32_t rs = (opcode >> 21) & UINT32_C(0x1f);
  uint32_t immediate = opcode & UINT32_C(0xffff);
  uint32_t pc_after_delay = pc + UINT32_C(8);
  uint32_t target;
  bool taken;

  *taken_out = false;
  *likely_out = false;
  *link_out = false;
  *link_register_out = 0u;
  *target_out = 0u;
  if (operation == T64_CPU_J || operation == T64_CPU_JAL) {
    target = ((pc + UINT32_C(4)) & UINT32_C(0xf0000000)) |
             ((opcode & UINT32_C(0x03ffffff)) << 2);
    *taken_out = true;
    *target_out = target;
    if (operation == T64_CPU_JAL) {
      *link_out = true;
      *link_register_out = 31u;
    }
    return true;
  }
  if ((opcode >> 26) == 0u) {
    uint32_t function = opcode & UINT32_C(0x3f);
    uint32_t rd = (opcode >> 11) & UINT32_C(0x1f);

    if (function == 8u) {
      *taken_out = true;
      *target_out = (uint32_t)cpu->gpr[rs];
      return true;
    }
    if (function == 9u) {
      *taken_out = true;
      *target_out = (uint32_t)cpu->gpr[rs];
      *link_out = true;
      *link_register_out = rd;
      return true;
    }
    return false;
  }
  taken = branch_condition(opcode, cpu, likely_out, link_out,
                           link_register_out);
  if (operation == T64_CPU_BLEZ || operation == T64_CPU_BLEZL ||
      operation == T64_CPU_BGTZ || operation == T64_CPU_BGTZL) {
    uint32_t rt = (opcode >> 16) & UINT32_C(0x1f);
    if (rt != 0u) {
      return false;
    }
  }
  if (operation == T64_CPU_REGIMM) {
    uint32_t rt = (opcode >> 16) & UINT32_C(0x1f);
    if (rt != 0u && rt != 1u && rt != 2u && rt != 3u && rt != 16u &&
        rt != 17u && rt != 18u && rt != 19u) {
      return false;
    }
  }
  target = pc + UINT32_C(4) +
           (uint32_t)(sign_extend_16_32((uint16_t)immediate) << 2);
  *taken_out = taken;
  *target_out = taken ? target : pc_after_delay;
  return true;
}

static bool execute_memory(t64_machine *machine, uint32_t opcode,
                           bool *write_gpr_out, uint32_t *destination_out,
                           uint64_t *value_out) {
  uint32_t operation = opcode >> 26;
  uint32_t rs = (opcode >> 21) & UINT32_C(0x1f);
  uint32_t rt = (opcode >> 16) & UINT32_C(0x1f);
  uint16_t immediate = (uint16_t)(opcode & UINT32_C(0xffff));
  uint64_t address = effective_address(&machine->cpu, rs, immediate);
  uint64_t value;
  uint32_t width;
  bool is_load = false;
  bool is_store = false;
  bool sign_extend = false;
  bool partial_left = false;
  bool partial_right = false;

  switch (operation) {
  case 0x1au: width = 8u; is_load = true; partial_left = true; break;
  case 0x1bu: width = 8u; is_load = true; partial_right = true; break;
  case 0x20u: width = 1u; is_load = true; sign_extend = true; break;
  case 0x21u: width = 2u; is_load = true; sign_extend = true; break;
  case 0x22u: width = 4u; is_load = true; partial_left = true; break;
  case 0x23u: width = 4u; is_load = true; sign_extend = true; break;
  case 0x24u: width = 1u; is_load = true; break;
  case 0x25u: width = 2u; is_load = true; break;
  case 0x26u: width = 4u; is_load = true; partial_right = true; break;
  case 0x27u: width = 4u; is_load = true; break;
  case 0x28u: width = 1u; is_store = true; break;
  case 0x29u: width = 2u; is_store = true; break;
  case 0x2au: width = 4u; is_store = true; partial_left = true; break;
  case 0x2bu: width = 4u; is_store = true; break;
  case 0x2cu: width = 8u; is_store = true; partial_left = true; break;
  case 0x2du: width = 8u; is_store = true; partial_right = true; break;
  case 0x2eu: width = 4u; is_store = true; partial_right = true; break;
  case 0x37u: width = 8u; is_load = true; break;
  case 0x3fu: width = 8u; is_store = true; break;
  default: return false;
  }

  if (partial_left || partial_right) {
    if (is_load) {
      if (!load_partial(machine, address, width, partial_left,
                        machine->cpu.gpr[rt], &value)) {
        return false;
      }
      *write_gpr_out = true;
      *destination_out = rt;
      *value_out = width == 4u ? sign_extend_32((uint32_t)value) : value;
      return true;
    }
    if (is_store) {
      return store_partial(machine, address, width, partial_left,
                           machine->cpu.gpr[rt]);
    }
    return false;
  }
  if ((width == 2u || width == 4u || width == 8u) &&
      ((uint32_t)address & (width - 1u)) != 0u) {
    return false;
  }
  if (is_load) {
    if (!cpu_read_scalar(machine, address, width, &value)) {
      return false;
    }
    if (sign_extend && width == 1u && (value & UINT64_C(0x80)) != 0u) {
      value |= UINT64_C(0xffffffffffffff00);
    } else if (sign_extend && width == 2u &&
               (value & UINT64_C(0x8000)) != 0u) {
      value |= UINT64_C(0xffffffffffff0000);
    } else if (sign_extend && width == 4u) {
      value = sign_extend_32((uint32_t)value);
    }
    *write_gpr_out = true;
    *destination_out = rt;
    *value_out = value;
    return true;
  }
  if (is_store) {
    return cpu_write_scalar(machine, address, width,
                            machine->cpu.gpr[rt]);
  }
  return false;
}

static bool execute_special(t64_machine *machine, uint32_t pc,
                            uint32_t opcode, bool *write_gpr_out,
                            uint32_t *destination_out, uint64_t *value_out) {
  uint32_t rs = (opcode >> 21) & UINT32_C(0x1f);
  uint32_t rt = (opcode >> 16) & UINT32_C(0x1f);
  uint32_t rd = (opcode >> 11) & UINT32_C(0x1f);
  uint32_t shift = (opcode >> 6) & UINT32_C(0x1f);
  uint32_t function = opcode & UINT32_C(0x3f);
  uint32_t left32 = (uint32_t)machine->cpu.gpr[rs];
  uint32_t right32 = (uint32_t)machine->cpu.gpr[rt];
  uint32_t result32;
  uint64_t left64 = machine->cpu.gpr[rs];
  uint64_t right64 = machine->cpu.gpr[rt];
  uint64_t result64;

  switch (function) {
  case 0x00u:
    *value_out = sign_extend_32(right32 << shift);
    break;
  case 0x02u:
    *value_out = sign_extend_32(right32 >> shift);
    break;
  case 0x03u:
    *value_out = sign_extend_32(arithmetic_shift_right_32(right32, shift));
    break;
  case 0x04u:
    *value_out = sign_extend_32(right32 << (left32 & UINT32_C(0x1f)));
    break;
  case 0x06u:
    *value_out = sign_extend_32(right32 >> (left32 & UINT32_C(0x1f)));
    break;
  case 0x07u:
    *value_out = sign_extend_32(
        arithmetic_shift_right_32(right32, left32 & UINT32_C(0x1f)));
    break;
  case 0x10u:
    *value_out = machine->cpu.hi;
    break;
  case 0x11u:
    machine->cpu.hi = left64;
    return true;
  case 0x12u:
    *value_out = machine->cpu.lo;
    break;
  case 0x13u:
    machine->cpu.lo = left64;
    return true;
  case 0x14u:
    *value_out = right64 << (left32 & UINT32_C(0x3f));
    break;
  case 0x16u:
    *value_out = right64 >> (left32 & UINT32_C(0x3f));
    break;
  case 0x17u:
    *value_out = arithmetic_shift_right_64(right64,
                                           left32 & UINT32_C(0x3f));
    break;
  case 0x20u:
  case 0x21u:
    result32 = left32 + right32;
    if (function == 0x20u && add_overflow_32(left32, right32, result32)) {
      return false;
    }
    *value_out = sign_extend_32(result32);
    break;
  case 0x22u:
  case 0x23u:
    result32 = left32 - right32;
    if (function == 0x22u &&
        subtract_overflow_32(left32, right32, result32)) {
      return false;
    }
    *value_out = sign_extend_32(result32);
    break;
  case 0x24u: *value_out = left64 & right64; break;
  case 0x25u: *value_out = left64 | right64; break;
  case 0x26u: *value_out = left64 ^ right64; break;
  case 0x27u: *value_out = ~(left64 | right64); break;
  case 0x2au: *value_out = signed_less_64(left64, right64) ? 1u : 0u; break;
  case 0x2bu: *value_out = left64 < right64 ? 1u : 0u; break;
  case 0x2cu:
  case 0x2du:
    result64 = left64 + right64;
    if (function == 0x2cu && add_overflow_64(left64, right64, result64)) {
      return false;
    }
    *value_out = result64;
    break;
  case 0x2eu:
  case 0x2fu:
    result64 = left64 - right64;
    if (function == 0x2eu && subtract_overflow_64(left64, right64, result64)) {
      return false;
    }
    *value_out = result64;
    break;
  case 0x38u: *value_out = right64 << shift; break;
  case 0x3au: *value_out = right64 >> shift; break;
  case 0x3bu: *value_out = arithmetic_shift_right_64(right64, shift); break;
  case 0x3cu: *value_out = right64 << (shift + 32u); break;
  case 0x3eu: *value_out = right64 >> (shift + 32u); break;
  case 0x3fu:
    *value_out = arithmetic_shift_right_64(right64, shift + 32u);
    break;
  default:
    return false;
  }
  *write_gpr_out = true;
  *destination_out = rd;
  (void)pc;
  return true;
}

static bool execute_integer(t64_machine *machine, uint32_t opcode,
                            bool *write_gpr_out, uint32_t *destination_out,
                            uint64_t *value_out) {
  uint32_t operation = opcode >> 26;
  uint32_t rs = (opcode >> 21) & UINT32_C(0x1f);
  uint32_t rt = (opcode >> 16) & UINT32_C(0x1f);
  uint32_t immediate = opcode & UINT32_C(0xffff);
  uint32_t left32 = (uint32_t)machine->cpu.gpr[rs];
  uint32_t result32;
  uint64_t left64 = machine->cpu.gpr[rs];
  uint64_t result64;

  *write_gpr_out = false;
  *destination_out = 0u;
  *value_out = 0u;
  if (operation == 0u) {
    return execute_special(machine, machine->cpu.pc, opcode, write_gpr_out,
                           destination_out, value_out);
  }
  if (operation >= 0x1au && operation <= 0x2fu) {
    return execute_memory(machine, opcode, write_gpr_out, destination_out,
                          value_out);
  }
  if (operation == 0x37u || operation == 0x3fu) {
    return execute_memory(machine, opcode, write_gpr_out, destination_out,
                          value_out);
  }
  switch (operation) {
  case 0x08u:
  case 0x09u:
    result32 = left32 + sign_extend_16_32((uint16_t)immediate);
    if (operation == 0x08u &&
        add_overflow_32(left32, sign_extend_16_32((uint16_t)immediate),
                        result32)) {
      return false;
    }
    result64 = sign_extend_32(result32);
    break;
  case 0x0au:
    result64 = signed_less_64(left64, sign_extend_16((uint16_t)immediate))
                   ? 1u
                   : 0u;
    break;
  case 0x0bu:
    result64 = left64 < sign_extend_16((uint16_t)immediate) ? 1u : 0u;
    break;
  case 0x0cu: result64 = left64 & (uint64_t)immediate; break;
  case 0x0du: result64 = left64 | (uint64_t)immediate; break;
  case 0x0eu: result64 = left64 ^ (uint64_t)immediate; break;
  case 0x0fu:
    result64 = sign_extend_32((uint32_t)(immediate << 16));
    break;
  case 0x18u:
  case 0x19u:
    result64 = left64 + sign_extend_16((uint16_t)immediate);
    if (operation == 0x18u && add_overflow_64(
                                    left64,
                                    sign_extend_16((uint16_t)immediate),
                                    result64)) {
      return false;
    }
    break;
  default:
    return execute_memory(machine, opcode, write_gpr_out, destination_out,
                          value_out);
  }
  *write_gpr_out = true;
  *destination_out = rt;
  *value_out = result64;
  return true;
}

static void cpu_stop_unsupported(t64_machine *machine, uint32_t pc,
                                 uint32_t opcode) {
  machine->cpu.stopped = true;
  machine->cpu.stop_reason = T64_STOP_REASON_CPU_UNSUPPORTED;
  machine->cpu.stop_pc = pc;
  machine->cpu.stop_opcode = opcode;
  t64_trace_emit(machine, machine->tick, T64_TRACE_DEVICE_MACHINE,
                 T64_TRACE_KIND_CPU_UNSUPPORTED,
                 ((uint64_t)pc << 32) | (uint64_t)opcode);
}

t64_status t64_cpu_schedule(t64_machine *machine) {
  t64_event_reservation reservation;
  t64_status status;

  if (machine == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  status = t64_schedule_reserve_after(
      machine, UINT64_C(1), T64_DEVICE_CPU, T64_CPU_EVENT_STEP, UINT64_C(0),
      UINT64_C(0), t64_cpu_step, &reservation);
  if (status != T64_STATUS_OK) {
    return status;
  }
  return t64_schedule_commit(machine, &reservation);
}

t64_status t64_cpu_step(t64_machine *machine, const t64_event *event) {
  uint32_t pc;
  uint32_t physical_pc;
  uint32_t opcode = 0u;
  uint32_t next_pc;
  uint32_t target = 0u;
  uint32_t link_register = 0u;
  uint32_t destination = 0u;
  uint64_t value = 0u;
  bool in_delay_slot;
  bool is_branch;
  bool branch_taken = false;
  bool branch_likely = false;
  bool link = false;
  bool write_gpr = false;
  bool supported;
  t64_status status;

  (void)event;
  if (machine == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (machine->cpu.stopped) {
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }
  pc = machine->cpu.pc;
  in_delay_slot = machine->cpu.delay_slot_pending;
  machine->cpu.last_pc = pc;
  if (!t64_cpu_translate(sign_extend_32(pc), &physical_pc) ||
      t64_bus_read(machine, physical_pc, 4u, &opcode) != T64_STATUS_OK) {
    machine->cpu.last_opcode = 0u;
    cpu_stop_unsupported(machine, pc, 0u);
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }
  machine->cpu.last_opcode = opcode;
  if (machine->cpu.retired_instructions == UINT64_MAX ||
      machine->instruction_count == UINT64_MAX) {
    cpu_stop_unsupported(machine, pc, opcode);
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }

  is_branch = is_branch_instruction(opcode);
  if (in_delay_slot && is_branch) {
    cpu_stop_unsupported(machine, pc, opcode);
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }
  if (is_branch) {
    supported = execute_branch(pc, opcode, &machine->cpu, &branch_taken,
                               &branch_likely, &link, &link_register,
                               &target);
    if (!supported) {
      cpu_stop_unsupported(machine, pc, opcode);
      return T64_STATUS_RESOURCE_EXHAUSTED;
    }
  } else {
    supported = execute_integer(machine, opcode, &write_gpr, &destination,
                                &value);
    if (!supported) {
      cpu_stop_unsupported(machine, pc, opcode);
      return T64_STATUS_RESOURCE_EXHAUSTED;
    }
  }

  if (write_gpr && destination != 0u) {
    machine->cpu.gpr[destination] = value;
  }
  if (link && (!branch_likely || branch_taken) && link_register != 0u) {
    machine->cpu.gpr[link_register] =
        sign_extend_32(pc + UINT32_C(8));
  }

  if (in_delay_slot) {
    next_pc = machine->cpu.delay_target;
    machine->cpu.delay_slot_pending = false;
    machine->cpu.delay_slot_pc = 0u;
    machine->cpu.delay_target = 0u;
  } else if (is_branch && branch_likely && !branch_taken) {
    next_pc = pc + UINT32_C(8);
    machine->cpu.delay_slot_pending = false;
    machine->cpu.delay_slot_pc = 0u;
    machine->cpu.delay_target = 0u;
  } else if (is_branch) {
    next_pc = pc + UINT32_C(4);
    machine->cpu.delay_slot_pending = true;
    machine->cpu.delay_slot_pc = next_pc;
    machine->cpu.delay_target = target;
  } else {
    next_pc = pc + UINT32_C(4);
    machine->cpu.delay_slot_pending = false;
    machine->cpu.delay_slot_pc = 0u;
    machine->cpu.delay_target = 0u;
  }
  machine->cpu.pc = next_pc;
  machine->cpu.gpr[0] = 0u;
  machine->cpu.retired_instructions++;
  machine->instruction_count++;
  t64_trace_emit(machine, machine->tick, T64_TRACE_DEVICE_MACHINE,
                 T64_TRACE_KIND_CPU_RETIREMENT,
                 ((uint64_t)pc << 32) | (uint64_t)opcode);
  status = t64_cpu_schedule(machine);
  if (status != T64_STATUS_OK) {
    return status;
  }
  return T64_STATUS_OK;
}
