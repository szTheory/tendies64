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
#define T64_CPU_COP0 UINT32_C(0x10)
#define T64_CPU_COP1 UINT32_C(0x11)
#define T64_CPU_CACHE UINT32_C(0x2f)

#define T64_CP0_COUNT UINT32_C(9)
#define T64_CP0_COMPARE UINT32_C(11)
#define T64_CP0_STATUS UINT32_C(12)
#define T64_CP0_CAUSE UINT32_C(13)
#define T64_CP0_EPC UINT32_C(14)
#define T64_CP0_ERROR_EPC UINT32_C(30)
#define T64_CP0_WATCHLO UINT32_C(18)
#define T64_CP0_TAGLO UINT32_C(28)
#define T64_CP0_TAGHI UINT32_C(29)
#define T64_CP0_STATUS_TS UINT32_C(0x00200000)
#define T64_CP0_CAUSE_SOFTWARE_MASK UINT32_C(0x00000300)
#define T64_CP0_CAUSE_TIMER_MASK UINT32_C(0x00008000)
#define T64_CP0_WATCHLO_MASK UINT32_C(0xfffffffb)
#define T64_CP0_TAGLO_MASK UINT32_C(0x0fffffc0)

#define T64_STATUS_ERL UINT32_C(0x00000004)
#define T64_CPU_COP1_CAUSE_MASK UINT32_C(0x0003f000)
#define T64_STATUS_EXL UINT32_C(0x00000002)
#define T64_STATUS_KSU UINT32_C(0x00000018)
#define T64_STATUS_UX UINT32_C(0x00000020)
#define T64_STATUS_SX UINT32_C(0x00000040)
#define T64_STATUS_FR UINT32_C(0x04000000)
#define T64_STATUS_RE UINT32_C(0x02000000)
#define T64_STATUS_USER UINT32_C(0x00000010)
#define T64_STATUS_SUPERVISOR UINT32_C(0x00000008)
#define T64_WATCHLO_R UINT32_C(0x00000002)
#define T64_WATCHLO_W UINT32_C(0x00000001)

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

static uint64_t sign_extend_32(uint32_t value) {
  return t64_cpu_canonical_address(value);
}

static bool has_vr4300_cpu_state(const t64_machine *machine) {
  return machine != NULL &&
         machine->profile.machine_model_version == UINT32_C(3) &&
         machine->profile.state_serialization_version == UINT32_C(9);
}

static bool vr4300_cache_operation_is_defined(uint32_t operation) {
  switch (operation) {
  case T64_CACHE_INDEX_INVALIDATE_I:
  case T64_CACHE_INDEX_WRITEBACK_INVALIDATE_D:
  case T64_CACHE_INDEX_LOAD_TAG_I:
  case T64_CACHE_INDEX_LOAD_TAG_D:
  case T64_CACHE_INDEX_STORE_TAG_I:
  case T64_CACHE_INDEX_STORE_TAG_D:
  case T64_CACHE_CREATE_DIRTY_EXCLUSIVE_D:
  case T64_CACHE_HIT_INVALIDATE_I:
  case T64_CACHE_HIT_INVALIDATE_D:
  case T64_CACHE_FILL_I:
  case T64_CACHE_HIT_WRITEBACK_INVALIDATE_D:
  case T64_CACHE_HIT_WRITEBACK_I:
  case T64_CACHE_HIT_WRITEBACK_D:
    return true;
  default:
    return false;
  }
}

static t64_status cpu_enter_exception(t64_machine *machine, uint32_t code,
                                      uint64_t bad_virtual_address,
                                      bool branch_delay,
                                      bool sets_coprocessor,
                                      uint32_t coprocessor) {
  t64_cpu_exception_result exception = {0};
  uint64_t pc = machine->cpu.pc_full;

  exception.valid = true;
  exception.code = code;
  exception.bad_virtual_address = bad_virtual_address;
  exception.epc = branch_delay ? machine->cpu.delay_slot_pc_full - 4u : pc;
  exception.branch_delay = branch_delay;
  exception.sets_coprocessor = sets_coprocessor;
  exception.coprocessor = coprocessor;
  return t64_cpu_exception_enter(machine, &exception);
}

static void sync_vr4300_full_addresses(t64_cpu_state *cpu) {
  cpu->pc_full = t64_cpu_canonical_address(cpu->pc);
  cpu->last_pc_full = t64_cpu_canonical_address(cpu->last_pc);
  cpu->delay_slot_pc_full = t64_cpu_canonical_address(cpu->delay_slot_pc);
  cpu->delay_target_full = t64_cpu_canonical_address(cpu->delay_target);
  cpu->stop_pc_full = t64_cpu_canonical_address(cpu->stop_pc);
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

static uint64_t unsigned_magnitude_64(uint64_t value) {
  return (value & UINT64_C(0x8000000000000000)) != 0u
             ? ~value + UINT64_C(1)
             : value;
}

/* Portable 64x64 -> 128 multiplication using four 32-bit partial products. */
static void multiply_u64_full(uint64_t left, uint64_t right,
                              uint64_t *high_out, uint64_t *low_out) {
  uint64_t left_low = (uint64_t)(uint32_t)left;
  uint64_t left_high = left >> 32;
  uint64_t right_low = (uint64_t)(uint32_t)right;
  uint64_t right_high = right >> 32;
  uint64_t low_product = left_low * right_low;
  uint64_t middle = left_high * right_low + (low_product >> 32);
  uint64_t cross = left_low * right_high + (uint32_t)middle;

  *low_out = (cross << 32) | (uint32_t)low_product;
  *high_out = left_high * right_high + (middle >> 32) + (cross >> 32);
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

static bool cpu_translate_access(const t64_machine *machine,
                                 uint64_t virtual_address, uint32_t width,
                                 t64_mmu_access_kind kind,
                                 t64_mmu_result *result_out) {
  t64_mmu_request request;
  uint32_t physical_address;

  if (machine == NULL || result_out == NULL) {
    return false;
  }
  if (has_vr4300_cpu_state(machine)) {
    request.virtual_address = virtual_address;
    request.width = width;
    request.kind = kind;
    return t64_mmu_translate(machine, &request, result_out);
  }
  *result_out = (t64_mmu_result){0};
  result_out->virtual_address = virtual_address;
  result_out->width = width;
  result_out->physical_domain = T64_MMU_DOMAIN_BUS;
  if ((width != 1u && width != 2u && width != 4u && width != 8u) ||
      (kind == T64_MMU_ACCESS_FETCH &&
       (virtual_address & UINT64_C(3)) != 0u) ||
      (kind != T64_MMU_ACCESS_FETCH && width > 1u &&
       (virtual_address & ((uint64_t)width - 1u)) != 0u) ||
      !t64_cpu_translate(virtual_address, &physical_address) ||
      (uint64_t)physical_address + width > UINT64_C(0x100000000)) {
    result_out->fault = T64_MMU_FAULT_ADDRESS;
    return true;
  }
  result_out->physical_address = (uint64_t)physical_address;
  result_out->translated = true;
  result_out->cacheable = true;
  return true;
}

static bool cpu_read_bytes(t64_machine *machine, uint64_t virtual_address,
                           uint8_t *bytes_out, uint32_t width) {
  t64_mmu_result translation;
  uint32_t physical_address;
  uint32_t index;

  if (machine == NULL || bytes_out == NULL || width == 0u ||
      !cpu_translate_access(machine, virtual_address, width,
                            T64_MMU_ACCESS_READ, &translation) ||
      !translation.translated || translation.physical_address > UINT32_MAX) {
    return false;
  }
  physical_address = (uint32_t)translation.physical_address;
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
  t64_mmu_result translation;
  uint32_t physical_address;

  if (machine == NULL || value_out == NULL ||
      !cpu_translate_access(machine, virtual_address, width,
                            T64_MMU_ACCESS_READ, &translation) ||
      !translation.translated || translation.physical_address > UINT32_MAX) {
    return false;
  }
  physical_address = (uint32_t)translation.physical_address;
  if (width == 1u || width == 2u || width == 4u) {
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
  t64_mmu_result translation;
  uint32_t physical_address;
  uint32_t index;

  if (machine == NULL || bytes == NULL || width == 0u ||
      !cpu_translate_access(machine, virtual_address, width,
                            T64_MMU_ACCESS_WRITE, &translation) ||
      !translation.translated || translation.physical_address > UINT32_MAX) {
    return false;
  }
  physical_address = (uint32_t)translation.physical_address;
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
  t64_mmu_result translation;
  uint32_t physical_address;

  if (machine == NULL ||
      !cpu_translate_access(machine, virtual_address, width,
                            T64_MMU_ACCESS_WRITE, &translation) ||
      !translation.translated || translation.physical_address > UINT32_MAX) {
    return false;
  }
  physical_address = (uint32_t)translation.physical_address;
  if (width == 1u || width == 2u || width == 4u) {
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
  if (t64_cop1_is_branch(opcode)) {
    return t64_cop1_branch_condition(cpu, opcode, likely_out);
  }
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

static bool is_branch_instruction(uint32_t opcode, bool new_profile) {
  uint32_t operation = opcode >> 26;

  if (new_profile && t64_cop1_is_branch(opcode)) {
    return true;
  }

  if (operation == T64_CPU_REGIMM) {
    uint32_t suboperation = (opcode >> 16) & UINT32_C(0x1f);

    return suboperation <= 3u ||
           (suboperation >= 16u && suboperation <= 19u);
  }
  if (operation == T64_CPU_J || operation == T64_CPU_JAL ||
      operation == T64_CPU_BEQ ||
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

static bool execute_branch(uint64_t pc, bool full_width, uint32_t opcode,
                           const t64_cpu_state *cpu, bool *taken_out,
                           bool *likely_out, bool *link_out,
                           uint32_t *link_register_out,
                           uint64_t *target_out) {
  uint32_t operation = opcode >> 26;
  uint32_t rs = (opcode >> 21) & UINT32_C(0x1f);
  uint32_t immediate = opcode & UINT32_C(0xffff);
  uint64_t branch_pc = full_width ? pc : (uint64_t)(uint32_t)pc;
  uint64_t pc_after_delay = branch_pc + UINT64_C(8);
  uint64_t target;
  bool taken;

  *taken_out = false;
  *likely_out = false;
  *link_out = false;
  *link_register_out = 0u;
  *target_out = 0u;
  if (operation == T64_CPU_J || operation == T64_CPU_JAL) {
    uint64_t region_mask = full_width ? UINT64_C(0xfffffffff0000000)
                                      : UINT64_C(0xf0000000);

    target = ((branch_pc + UINT64_C(4)) & region_mask) |
             ((uint64_t)(opcode & UINT32_C(0x03ffffff)) << 2);
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
      *target_out = full_width ? cpu->gpr[rs]
                               : (uint64_t)(uint32_t)cpu->gpr[rs];
      return true;
    }
    if (function == 9u) {
      *taken_out = true;
      *target_out = full_width ? cpu->gpr[rs]
                               : (uint64_t)(uint32_t)cpu->gpr[rs];
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
  target = branch_pc + UINT64_C(4) +
           (sign_extend_16((uint16_t)immediate) << 2);
  *taken_out = taken;
  *target_out = taken ? target : pc_after_delay;
  return true;
}

static bool execute_memory(t64_machine *machine, uint32_t opcode,
                           bool *write_gpr_out, uint32_t *destination_out,
                           uint64_t *value_out,
                           t64_status *memory_status_out) {
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

  if (has_vr4300_cpu_state(machine)) {
    bool supported = false;
    *memory_status_out = t64_memory_execute_status(
        machine, opcode, &supported, write_gpr_out, destination_out,
        value_out);
    return supported && *memory_status_out == T64_STATUS_OK;
  }
  *memory_status_out = T64_STATUS_OK;

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

static void execute_word_multiply_unsigned(t64_machine *machine,
                                           uint32_t left, uint32_t right) {
  uint64_t product = (uint64_t)left * (uint64_t)right;

  machine->cpu.hi = sign_extend_32((uint32_t)(product >> 32));
  machine->cpu.lo = sign_extend_32((uint32_t)product);
}

static bool execute_word_divide(t64_machine *machine, uint32_t dividend,
                                uint32_t divisor, bool signed_operation) {
  uint32_t quotient;
  uint32_t remainder;

  if (signed_operation) {
    bool dividend_negative = (dividend & UINT32_C(0x80000000)) != 0u;
    bool divisor_negative = (divisor & UINT32_C(0x80000000)) != 0u;
    uint32_t dividend_magnitude;
    uint32_t divisor_magnitude;

    if (divisor == 0u) {
      machine->cpu.lo = sign_extend_32(
          dividend_negative ? UINT32_C(0x80000001) : UINT32_C(0x7fffffff));
      machine->cpu.hi = sign_extend_32(dividend);
      return true;
    }
    if (dividend == UINT32_C(0x80000000) && divisor == UINT32_MAX) {
      return false;
    }
    dividend_magnitude = dividend_negative ? ~dividend + UINT32_C(1)
                                           : dividend;
    divisor_magnitude = divisor_negative ? ~divisor + UINT32_C(1)
                                         : divisor;
    quotient = dividend_magnitude / divisor_magnitude;
    remainder = dividend_magnitude % divisor_magnitude;
    if (dividend_negative != divisor_negative) {
      quotient = ~quotient + UINT32_C(1);
    }
    if (dividend_negative) {
      remainder = ~remainder + UINT32_C(1);
    }
  } else {
    if (divisor == 0u) {
      return false;
    }
    quotient = dividend / divisor;
    remainder = dividend % divisor;
  }
  machine->cpu.lo = sign_extend_32(quotient);
  machine->cpu.hi = sign_extend_32(remainder);
  return true;
}

static void execute_doubleword_multiply(t64_machine *machine, uint64_t left,
                                        uint64_t right,
                                        bool signed_operation) {
  uint64_t high;
  uint64_t low;

  if (signed_operation) {
    bool negative =
        ((left ^ right) & UINT64_C(0x8000000000000000)) != 0u;

    multiply_u64_full(unsigned_magnitude_64(left),
                      unsigned_magnitude_64(right), &high, &low);
    if (negative) {
      bool carry = low == 0u;

      low = ~low + UINT64_C(1);
      high = ~high + (carry ? UINT64_C(1) : UINT64_C(0));
    }
  } else {
    multiply_u64_full(left, right, &high, &low);
  }
  machine->cpu.hi = high;
  machine->cpu.lo = low;
}

static bool execute_doubleword_divide(t64_machine *machine, uint64_t dividend,
                                      uint64_t divisor,
                                      bool signed_operation) {
  uint64_t quotient;
  uint64_t remainder;

  if (divisor == 0u) {
    return false;
  }
  if (signed_operation) {
    bool dividend_negative =
        (dividend & UINT64_C(0x8000000000000000)) != 0u;
    bool divisor_negative =
        (divisor & UINT64_C(0x8000000000000000)) != 0u;
    uint64_t dividend_magnitude;
    uint64_t divisor_magnitude;

    if (dividend == UINT64_C(0x8000000000000000) && divisor == UINT64_MAX) {
      return false;
    }
    dividend_magnitude = unsigned_magnitude_64(dividend);
    divisor_magnitude = unsigned_magnitude_64(divisor);
    quotient = dividend_magnitude / divisor_magnitude;
    remainder = dividend_magnitude % divisor_magnitude;
    if (dividend_negative != divisor_negative) {
      quotient = ~quotient + UINT64_C(1);
    }
    if (dividend_negative) {
      remainder = ~remainder + UINT64_C(1);
    }
  } else {
    quotient = dividend / divisor;
    remainder = dividend % divisor;
  }
  machine->cpu.lo = quotient;
  machine->cpu.hi = remainder;
  return true;
}

static bool execute_special(t64_machine *machine, uint32_t pc,
                            uint32_t opcode, bool new_profile,
                            bool *write_gpr_out,
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
  case 0x0fu:
    if (!new_profile) {
      return false;
    }
    return true;
  case 0x18u: {
    uint32_t left_magnitude;
    uint32_t right_magnitude;
    uint64_t product;
    bool negative;

    if ((opcode & UINT32_C(0x0000ffc0)) != 0u) {
      return false;
    }
    negative = ((left32 ^ right32) & UINT32_C(0x80000000)) != 0u;
    left_magnitude = (left32 & UINT32_C(0x80000000)) != 0u
                         ? ~left32 + UINT32_C(1)
                         : left32;
    right_magnitude = (right32 & UINT32_C(0x80000000)) != 0u
                          ? ~right32 + UINT32_C(1)
                          : right32;
    product = (uint64_t)left_magnitude * (uint64_t)right_magnitude;
    if (negative) {
      product = ~product + UINT64_C(1);
    }
    machine->cpu.hi = sign_extend_32((uint32_t)(product >> 32));
    machine->cpu.lo = sign_extend_32((uint32_t)product);
    return true;
  }
  case 0x19u: {
    if (!new_profile) {
      return false;
    }
    execute_word_multiply_unsigned(machine, left32, right32);
    return true;
  }
  case 0x1au:
  case 0x1bu:
    return new_profile &&
           execute_word_divide(machine, left32, right32, function == 0x1au);
  case 0x1cu:
  case 0x1du:
    if (!new_profile) {
      return false;
    }
    execute_doubleword_multiply(machine, left64, right64, function == 0x1cu);
    return true;
  case 0x1eu:
  case 0x1fu:
    return new_profile && execute_doubleword_divide(
                               machine, left64, right64, function == 0x1eu);
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
                            bool new_profile, bool *write_gpr_out,
                            uint32_t *destination_out,
                            uint64_t *value_out,
                            t64_status *memory_status_out) {
  uint32_t operation = opcode >> 26;
  uint32_t rs = (opcode >> 21) & UINT32_C(0x1f);
  uint32_t rt = (opcode >> 16) & UINT32_C(0x1f);
  uint32_t immediate = opcode & UINT32_C(0xffff);
  uint32_t left32 = (uint32_t)machine->cpu.gpr[rs];
  uint32_t result32;
  uint64_t left64 = machine->cpu.gpr[rs];
  uint64_t result64;

  *memory_status_out = T64_STATUS_OK;
  *write_gpr_out = false;
  *destination_out = 0u;
  *value_out = 0u;
  if (operation == 0u) {
    return execute_special(machine, machine->cpu.pc, opcode, new_profile,
                           write_gpr_out, destination_out, value_out);
  }
  if (operation >= 0x1au && operation <= 0x2fu) {
    return execute_memory(machine, opcode, write_gpr_out, destination_out,
                          value_out, memory_status_out);
  }
  if (operation == 0x37u || operation == 0x3fu) {
    return execute_memory(machine, opcode, write_gpr_out, destination_out,
                          value_out, memory_status_out);
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
                          value_out, memory_status_out);
  }
  *write_gpr_out = true;
  *destination_out = rt;
  *value_out = result64;
  return true;
}

static bool execute_cp0_warm(t64_machine *machine, uint32_t opcode,
                             bool *write_gpr_out,
                             uint32_t *destination_out,
                             uint64_t *value_out) {
  uint32_t suboperation = (opcode >> 21) & UINT32_C(0x1f);
  uint32_t rt = (opcode >> 16) & UINT32_C(0x1f);
  uint32_t rd = (opcode >> 11) & UINT32_C(0x1f);
  uint32_t select = opcode & UINT32_C(7);

  *write_gpr_out = false;
  *destination_out = 0u;
  *value_out = UINT64_C(0);
  if ((opcode & UINT32_C(0x7f8)) != 0u || select != 0u) {
    return false;
  }
  if (suboperation == 0u) { /* MFC0 */
    if (rd != T64_CP0_COUNT) {
      return false;
    }
    *write_gpr_out = true;
    *destination_out = rt;
    *value_out = sign_extend_32((uint32_t)machine->cpu.cp0[rd]);
    return true;
  }
  if (suboperation != 4u) { /* MTC0 */
    return false;
  }

  switch (rd) {
  case T64_CP0_COUNT:
  case T64_CP0_COMPARE:
    machine->cpu.cp0[rd] = (uint32_t)machine->cpu.gpr[rt];
    if (rd == T64_CP0_COMPARE) {
      machine->cpu.cp0[T64_CP0_CAUSE] =
          (uint32_t)machine->cpu.cp0[T64_CP0_CAUSE] &
          ~T64_CP0_CAUSE_TIMER_MASK;
    }
    return true;
  case T64_CP0_CAUSE:
    machine->cpu.cp0[rd] =
        ((uint32_t)machine->cpu.cp0[rd] &
         ~T64_CP0_CAUSE_SOFTWARE_MASK) |
        ((uint32_t)machine->cpu.gpr[rt] & T64_CP0_CAUSE_SOFTWARE_MASK);
    return true;
  case T64_CP0_WATCHLO:
    machine->cpu.cp0[rd] =
        (uint32_t)machine->cpu.gpr[rt] & T64_CP0_WATCHLO_MASK;
    return true;
  case T64_CP0_TAGLO:
    machine->cpu.cp0[rd] =
        (uint32_t)machine->cpu.gpr[rt] & T64_CP0_TAGLO_MASK;
    return true;
  case T64_CP0_TAGHI:
    /* VR4300 has a 32-bit physical address and no implemented TagHi bits. */
    machine->cpu.cp0[rd] = UINT32_C(0);
    return true;
  default:
    return false;
  }
}

static t64_cp0_transfer_result execute_cp0_vr4300(
    t64_machine *machine, uint32_t opcode, bool *write_gpr_out,
    uint32_t *destination_out, uint64_t *value_out,
    t64_cp0_effect *effect_out) {
  uint32_t operation = (opcode >> 21) & UINT32_C(0x1f);
  uint32_t function = opcode & UINT32_C(0x3f);
  t64_cp0_transfer_result result;

  if (operation == UINT32_C(0x10) &&
      (function == UINT32_C(1) || function == UINT32_C(2) ||
       function == UINT32_C(6) || function == UINT32_C(8))) {
    result =
        t64_cp0_prepare_tlb_instruction(machine, opcode, effect_out);
  } else {
    result = t64_cp0_prepare_transfer(machine, opcode, effect_out);
  }

  *write_gpr_out = false;
  *destination_out = 0u;
  *value_out = UINT64_C(0);
  if (result != T64_CP0_TRANSFER_OK) {
    return result;
  }
  *write_gpr_out = effect_out->write_gpr;
  *destination_out = effect_out->gpr_index;
  *value_out = effect_out->gpr_value;
  return T64_CP0_TRANSFER_OK;
}

static bool execute_cache(t64_machine *machine, uint32_t pc,
                          uint32_t opcode,
                          t64_status *operation_status_out) {
  uint32_t operation = (opcode >> 16) & UINT32_C(0x1f);
  uint32_t base = (opcode >> 21) & UINT32_C(0x1f);
  uint32_t physical_address;
  uint64_t virtual_address;
  t64_mmu_result translation;
  t64_status status;

  *operation_status_out = T64_STATUS_BUS_UNSUPPORTED;
  virtual_address =
      machine->cpu.gpr[base] + sign_extend_16((uint16_t)opcode);

  if (!has_vr4300_cpu_state(machine)) {
    /* Preserve the exact warm-v1 CACHE subset and effects. */
    if (operation != T64_CACHE_INDEX_STORE_TAG_I &&
        operation != T64_CACHE_INDEX_STORE_TAG_D &&
        operation != T64_CACHE_HIT_INVALIDATE_I &&
        operation != T64_CACHE_HIT_INVALIDATE_D &&
        operation != T64_CACHE_HIT_WRITEBACK_INVALIDATE_D) {
      return false;
    }
    if (operation == T64_CACHE_HIT_INVALIDATE_I) {
      return false;
    }
    if (!cpu_translate_access(machine, virtual_address, UINT32_C(1),
                              T64_MMU_ACCESS_READ, &translation) ||
        !translation.translated ||
        translation.physical_address > UINT32_MAX) {
      return false;
    }
    physical_address = (uint32_t)translation.physical_address;
    if (operation == T64_CACHE_HIT_INVALIDATE_D ||
        operation == T64_CACHE_HIT_WRITEBACK_INVALIDATE_D) {
      t64_cpu_link_invalidate_dcache_line(machine, physical_address);
    }
    machine->cache.last_operation = operation;
    machine->cache.last_pc = pc;
    machine->cache.last_address = physical_address;
    machine->cache.operation_seen = true;
    return true;
  }

  if (!vr4300_cache_operation_is_defined(operation)) {
    return false;
  }
  if (!cpu_translate_access(machine, virtual_address, UINT32_C(1),
                            T64_MMU_ACCESS_READ, &translation) ||
      !translation.translated || translation.physical_address > UINT32_MAX) {
    return false;
  }
  if (!translation.cacheable) {
    return false;
  }
  physical_address = (uint32_t)translation.physical_address;
  status = t64_cpu_cache_operation(machine, operation, virtual_address,
                                   physical_address);
  if (status != T64_STATUS_OK) {
    *operation_status_out = status;
    return false;
  }
  machine->cache.last_operation = operation;
  machine->cache.last_pc = pc;
  machine->cache.last_address = physical_address;
  machine->cache.operation_seen = true;
  return true;
}

static void cp0_retirement_tick(t64_machine *machine) {
  uint32_t count = (uint32_t)machine->cpu.cp0[T64_CP0_COUNT] + UINT32_C(1);
  uint32_t compare = (uint32_t)machine->cpu.cp0[T64_CP0_COMPARE];
  uint32_t cause = (uint32_t)machine->cpu.cp0[T64_CP0_CAUSE];

  machine->cpu.cp0[T64_CP0_COUNT] = count;
  if (count == compare) {
    cause |= T64_CP0_CAUSE_TIMER_MASK;
  }
  machine->cpu.cp0[T64_CP0_CAUSE] = cause;
}

static void cpu_stop_unsupported(t64_machine *machine, uint32_t pc,
                                 uint32_t opcode) {
  machine->cpu.stopped = true;
  machine->cpu.stop_reason = T64_STOP_REASON_CPU_UNSUPPORTED;
  machine->cpu.stop_pc = pc;
  machine->cpu.stop_opcode = opcode;
  if (has_vr4300_cpu_state(machine)) {
    machine->cpu.attempt_outcome = T64_CPU_ATTEMPT_MODEL_UNSUPPORTED;
    sync_vr4300_full_addresses(&machine->cpu);
  }
  t64_trace_emit(machine, machine->tick, T64_TRACE_DEVICE_MACHINE,
                 T64_TRACE_KIND_CPU_UNSUPPORTED,
                 ((uint64_t)pc << 32) | (uint64_t)opcode);
}

static void cpu_stop_completion(t64_machine *machine, uint32_t pc,
                                uint32_t opcode) {
  machine->cpu.stopped = true;
  machine->cpu.stop_reason = T64_STOP_REASON_GUEST_COMPLETION;
  machine->cpu.stop_pc = pc;
  machine->cpu.stop_opcode = opcode;
  if (has_vr4300_cpu_state(machine)) {
    machine->cpu.attempt_outcome = T64_CPU_ATTEMPT_SOFTWARE_STOP;
    sync_vr4300_full_addresses(&machine->cpu);
  }
  t64_trace_emit(machine, machine->tick, T64_TRACE_DEVICE_MACHINE,
                 T64_TRACE_KIND_CPU_COMPLETION,
                 ((uint64_t)pc << 32) | (uint64_t)opcode);
}

static t64_status cpu_enter_mmu_fault(t64_machine *machine,
                                      const t64_mmu_request *request,
                                      const t64_mmu_result *translation,
                                      bool in_delay_slot,
                                      uint32_t instruction) {
  t64_cpu_exception_result exception = {0};
  uint32_t code;

  if (translation->fault == T64_MMU_FAULT_DUPLICATE_MATCH) {
    machine->cpu.tlb_shutdown = true;
    machine->cpu.cp0[T64_CP0_STATUS] =
        (uint32_t)machine->cpu.cp0[T64_CP0_STATUS] | T64_CP0_STATUS_TS;
    cpu_stop_unsupported(machine, machine->cpu.pc, instruction);
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }
  switch (translation->fault) {
  case T64_MMU_FAULT_ADDRESS:
    code = request->kind == T64_MMU_ACCESS_WRITE ? UINT32_C(5)
                                                 : UINT32_C(4);
    break;
  case T64_MMU_FAULT_TLB_MODIFIED: code = UINT32_C(1); break;
  case T64_MMU_FAULT_TLB_REFILL:
  case T64_MMU_FAULT_TLB_INVALID:
    code = request->kind == T64_MMU_ACCESS_WRITE ? UINT32_C(3)
                                                 : UINT32_C(2);
    break;
  case T64_MMU_FAULT_NONE:
  case T64_MMU_FAULT_DUPLICATE_MATCH:
  default:
    machine->cpu.attempt_outcome = T64_CPU_ATTEMPT_HOST_ERROR;
    return T64_STATUS_INVALID_ARGUMENT;
  }
  exception.valid = true;
  exception.code = code;
  exception.bad_virtual_address = request->virtual_address;
  exception.epc = in_delay_slot
                      ? machine->cpu.delay_slot_pc_full - UINT64_C(4)
                      : machine->cpu.pc_full;
  exception.branch_delay = in_delay_slot;
  exception.sets_tlb_context = translation->tlb_context_valid;
  exception.tlb_refill = translation->tlb_refill_vector;
  exception.tlb_extended = translation->extended_address_mode;
  return t64_cpu_exception_enter(machine, &exception);
}

static bool memory_access_info(uint32_t opcode, uint32_t *width_out,
                               bool *store_out, bool *partial_out) {
  uint32_t operation = opcode >> 26;

  *store_out = false;
  *partial_out = false;
  switch (operation) {
  case 0x1au: *width_out = 8u; *partial_out = true; return true;
  case 0x1bu: *width_out = 8u; *partial_out = true; return true;
  case 0x20u: *width_out = 1u; return true;
  case 0x21u: *width_out = 2u; return true;
  case 0x22u: *width_out = 4u; *partial_out = true; return true;
  case 0x23u: *width_out = 4u; return true;
  case 0x24u: *width_out = 1u; return true;
  case 0x25u: *width_out = 2u; return true;
  case 0x26u: *width_out = 4u; *partial_out = true; return true;
  case 0x27u: *width_out = 4u; return true;
  case 0x28u: *width_out = 1u; *store_out = true; return true;
  case 0x29u: *width_out = 2u; *store_out = true; return true;
  case 0x2au:
    *width_out = 4u;
    *store_out = true;
    *partial_out = true;
    return true;
  case 0x2bu: *width_out = 4u; *store_out = true; return true;
  case 0x2cu:
  case 0x2du:
    *width_out = 8u;
    *store_out = true;
    *partial_out = true;
    return true;
  case 0x2eu:
    *width_out = 4u;
    *store_out = true;
    *partial_out = true;
    return true;
  case 0x30u: *width_out = 4u; return true;
  case 0x31u: *width_out = 4u; return true;
  case 0x35u: *width_out = 8u; return true;
  case 0x34u: *width_out = 8u; return true;
  case 0x38u: *width_out = 4u; *store_out = true; return true;
  case 0x39u: *width_out = 4u; *store_out = true; return true;
  case 0x3cu: *width_out = 8u; *store_out = true; return true;
  case 0x3du: *width_out = 8u; *store_out = true; return true;
  case 0x37u: *width_out = 8u; return true;
  case 0x3fu: *width_out = 8u; *store_out = true; return true;
  default: return false;
  }
}

static bool watch_matches(const t64_machine *machine, uint32_t physical,
                          bool store) {
  uint32_t watchlo = (uint32_t)machine->cpu.cp0[T64_CP0_WATCHLO];
  uint32_t status = (uint32_t)machine->cpu.cp0[T64_CP0_STATUS];
  uint32_t enabled = store ? T64_WATCHLO_W : T64_WATCHLO_R;

  return (status & T64_STATUS_EXL) == 0u && (watchlo & enabled) != 0u &&
         (physical & UINT32_C(0xfffffff8)) ==
             (watchlo & UINT32_C(0xfffffff8));
}

static bool cpu_reverse_endian_active(const t64_machine *machine) {
  uint32_t status = (uint32_t)machine->cpu.cp0[T64_CP0_STATUS];
  uint32_t config = (uint32_t)machine->cpu.cp0[16];

  return (status & (T64_STATUS_EXL | T64_STATUS_ERL)) == 0u &&
         (status & T64_STATUS_KSU) == T64_STATUS_USER &&
         (status & T64_STATUS_RE) != 0u &&
         (config & UINT32_C(0x00008000)) != 0u;
}

static uint8_t partial_access_mask(uint32_t width, uint32_t offset,
                                   bool left, bool little_endian) {
  bool low_bytes = little_endian == left;
  uint32_t all_bytes = (UINT32_C(1) << width) - 1u;

  if (little_endian && width == 4u) {
    uint32_t word_base = offset < 4u ? 4u : 0u;
    uint32_t within = offset & 3u;
    uint32_t count = left ? within + 1u : 4u - within;
    uint32_t start = left ? word_base + 4u - count : word_base;

    return (uint8_t)(((UINT32_C(1) << count) - 1u) << start);
  }
  if (little_endian && width == 8u) {
    uint32_t count = left ? offset + 1u : 8u - offset;
    uint32_t start = left ? 8u - count : 0u;

    return (uint8_t)(((UINT32_C(1) << count) - 1u) << start);
  }

  if (low_bytes) {
    return (uint8_t)((UINT32_C(1) << (offset + 1u)) - 1u);
  }
  return (uint8_t)(all_bytes & ~((UINT32_C(1) << offset) - 1u));
}

static bool memory_op_requires_64bit_mode(uint32_t operation) {
  switch (operation) {
  case 0x1au: case 0x1bu: case 0x2cu: case 0x2du:
  case 0x34u: case 0x3cu:
  case 0x37u: case 0x3fu:
    return true;
  default:
    return false;
  }
}

static bool memory_op_refused_in_32bit_user_or_supervisor(
    const t64_machine *machine) {
  uint32_t status = (uint32_t)machine->cpu.cp0[T64_CP0_STATUS];
  uint32_t privilege = status & T64_STATUS_KSU;

  if ((status & (T64_STATUS_EXL | T64_STATUS_ERL)) != 0u) {
    return false;
  }
  if (privilege == T64_STATUS_USER) {
    return (status & T64_STATUS_UX) == 0u;
  }
  if (privilege == T64_STATUS_SUPERVISOR) {
    return (status & T64_STATUS_SX) == 0u;
  }
  return false;
}

static t64_status cpu_preflight_memory(t64_machine *machine, uint32_t opcode,
                                       bool in_delay_slot, bool *handled_out) {
  uint32_t width = 0u;
  uint32_t physical = 0u;
  uint32_t probe_width;
  uint8_t touched;
  bool store = false;
  bool partial = false;
  bool reverse_endian;
  uint64_t address;
  t64_mmu_request request;
  t64_mmu_result translation;
  t64_status status;
  uint32_t offset;

  *handled_out = false;
  if (!memory_access_info(opcode, &width, &store, &partial)) {
    if ((opcode >> 26) == T64_CPU_CACHE) {
      t64_mmu_request cache_request;
      t64_mmu_result cache_translation;
      uint32_t cache_operation =
          (opcode >> 16) & UINT32_C(0x1f);

      /* Warm-v1 retains its legacy direct-only CACHE subset. */
      if (!has_vr4300_cpu_state(machine)) {
        return T64_STATUS_OK;
      }
      if (!vr4300_cache_operation_is_defined(cache_operation)) {
        return T64_STATUS_OK;
      }

      cache_request.virtual_address =
          machine->cpu.gpr[(opcode >> 21) & UINT32_C(0x1f)] +
          sign_extend_16((uint16_t)opcode);
      cache_request.width = UINT32_C(1);
      cache_request.kind = T64_MMU_ACCESS_READ;
      if (!t64_mmu_translate(machine, &cache_request,
                             &cache_translation)) {
        *handled_out = true;
        machine->cpu.attempt_outcome = T64_CPU_ATTEMPT_HOST_ERROR;
        return T64_STATUS_INVALID_ARGUMENT;
      }
      if (cache_translation.fault != T64_MMU_FAULT_NONE) {
        *handled_out = true;
        return cpu_enter_mmu_fault(machine, &cache_request,
                                   &cache_translation, in_delay_slot,
                                   opcode);
      }
      if (!cache_translation.translated ||
          cache_translation.physical_address > UINT32_MAX) {
        *handled_out = true;
        machine->cpu.attempt_outcome = T64_CPU_ATTEMPT_HOST_ERROR;
        return T64_STATUS_INVALID_ARGUMENT;
      }
    }
    return T64_STATUS_OK;
  }
  if (memory_op_requires_64bit_mode(opcode >> 26) &&
      memory_op_refused_in_32bit_user_or_supervisor(machine)) {
    *handled_out = true;
    return cpu_enter_exception(machine, UINT32_C(10), 0u, in_delay_slot,
                               false, 0u);
  }
  address = effective_address(&machine->cpu,
                              (opcode >> 21) & UINT32_C(0x1f),
                              (uint16_t)opcode);
  if ((opcode >> 26) == UINT32_C(0x31) ||
      (opcode >> 26) == UINT32_C(0x35) ||
      (opcode >> 26) == UINT32_C(0x39) ||
      (opcode >> 26) == UINT32_C(0x3d)) {
    if ((address & ((uint64_t)width - UINT64_C(1))) != 0u) {
      *handled_out = true;
      return cpu_enter_exception(machine, store ? UINT32_C(5) : UINT32_C(4),
                                 address, in_delay_slot, false, 0u);
    }
  }
  request.virtual_address = address;
  request.width = partial ? UINT32_C(1) : width;
  request.kind = store ? T64_MMU_ACCESS_WRITE : T64_MMU_ACCESS_READ;
  if (!cpu_translate_access(machine, request.virtual_address, request.width,
                            request.kind, &translation)) {
    *handled_out = true;
    machine->cpu.attempt_outcome = T64_CPU_ATTEMPT_HOST_ERROR;
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (translation.fault != T64_MMU_FAULT_NONE) {
    *handled_out = true;
    return cpu_enter_mmu_fault(machine, &request, &translation,
                               in_delay_slot, opcode);
  }
  if (!translation.translated || translation.physical_address > UINT32_MAX) {
    *handled_out = true;
    machine->cpu.attempt_outcome = T64_CPU_ATTEMPT_HOST_ERROR;
    return T64_STATUS_INVALID_ARGUMENT;
  }
  physical = (uint32_t)translation.physical_address;
  reverse_endian = cpu_reverse_endian_active(machine);
  probe_width = width;
  if (partial && reverse_endian) {
    probe_width = 8u;
  } else if (reverse_endian) {
    uint32_t lane_xor = 8u - width;

    physical = (physical & ~UINT32_C(7)) |
               ((physical & UINT32_C(7)) ^ lane_xor);
  }

  if (watch_matches(machine, physical, store)) {
    *handled_out = true;
    return cpu_enter_exception(machine, UINT32_C(23), address, in_delay_slot,
                               false, 0u);
  }

  if (partial) {
    physical &= ~(probe_width - 1u);
  }
  touched = partial
                ? partial_access_mask(
                      width,
                      (uint32_t)address &
                          (reverse_endian && width == 4u ? 7u : width - 1u),
                      ((opcode >> 26) == 0x22u ||
                       (opcode >> 26) == 0x2au ||
                       (opcode >> 26) == 0x2cu),
                      reverse_endian)
                : (uint8_t)((UINT32_C(1) << width) - 1u);

  for (offset = 0u; offset < probe_width; offset++) {
    if ((touched & (uint8_t)(UINT8_C(1) << offset)) == 0u) {
      continue;
    }
    if (physical > UINT32_MAX - offset) {
      *handled_out = true;
      return cpu_enter_exception(machine, UINT32_C(7), address,
                                 in_delay_slot, false, 0u);
    }
    status = t64_bus_probe_access(machine, physical + offset, UINT32_C(1));
    if (status == T64_STATUS_BUS_UNMAPPED || status == T64_STATUS_BUS_RANGE) {
      *handled_out = true;
      return cpu_enter_exception(machine, UINT32_C(7), address,
                                 in_delay_slot, false, 0u);
    }
    if (status == T64_STATUS_BUS_UNSUPPORTED) {
      return T64_STATUS_OK;
    }
    if (status != T64_STATUS_OK) {
      *handled_out = true;
      machine->cpu.attempt_outcome = T64_CPU_ATTEMPT_HOST_ERROR;
      return status;
    }
  }
  return T64_STATUS_OK;
}

static bool trap_condition(uint32_t operation, uint64_t left,
                           uint64_t right) {
  switch (operation) {
  case 0x30u: /* TGE */ return !signed_less_64(left, right);
  case 0x31u: /* TGEU */ return left >= right;
  case 0x32u: /* TLT */ return signed_less_64(left, right);
  case 0x33u: /* TLTU */ return left < right;
  case 0x34u: /* TEQ */ return left == right;
  case 0x36u: /* TNE */ return left != right;
  default: return false;
  }
}

static bool cpu_test_trap(const t64_cpu_state *cpu, uint32_t opcode,
                          bool *is_trap_out) {
  uint32_t operation = opcode >> 26;
  uint32_t function = opcode & UINT32_C(0x3f);
  uint32_t rs = (opcode >> 21) & UINT32_C(0x1f);
  uint32_t rt = (opcode >> 16) & UINT32_C(0x1f);
  uint64_t left = cpu->gpr[rs];
  uint64_t right = cpu->gpr[rt];
  uint32_t trap_operation = function;

  *is_trap_out = false;
  if (operation == 0u &&
      (function == 0x30u || function == 0x31u || function == 0x32u ||
       function == 0x33u || function == 0x34u || function == 0x36u)) {
    *is_trap_out = true;
    return trap_condition(trap_operation, left, right);
  }
  if (operation != T64_CPU_REGIMM) {
    return false;
  }
  switch (rt) {
  case 8u: trap_operation = 0x30u; break; /* TGEI */
  case 9u: trap_operation = 0x31u; break; /* TGEIU */
  case 10u: trap_operation = 0x32u; break; /* TLTI */
  case 11u: trap_operation = 0x33u; break; /* TLTIU */
  case 12u: trap_operation = 0x34u; break; /* TEQI */
  case 14u: trap_operation = 0x36u; break; /* TNEI */
  default: return false;
  }
  *is_trap_out = true;
  right = sign_extend_16((uint16_t)opcode);
  return trap_condition(trap_operation, left, right);
}

static bool cpu_instruction_overflows(const t64_machine *machine,
                                      uint32_t opcode) {
  uint32_t operation = opcode >> 26;
  uint32_t rs = (opcode >> 21) & UINT32_C(0x1f);
  uint32_t rt = (opcode >> 16) & UINT32_C(0x1f);
  uint32_t function = opcode & UINT32_C(0x3f);
  uint32_t left32 = (uint32_t)machine->cpu.gpr[rs];
  uint32_t right32 = (uint32_t)machine->cpu.gpr[rt];
  uint32_t result32;
  uint64_t left64 = machine->cpu.gpr[rs];
  uint64_t right64 = machine->cpu.gpr[rt];
  uint64_t result64;

  if (operation == 0x08u) {
    right32 = sign_extend_16_32((uint16_t)opcode);
    result32 = left32 + right32;
    return add_overflow_32(left32, right32, result32);
  }
  if (operation == 0x18u) {
    right64 = sign_extend_16((uint16_t)opcode);
    result64 = left64 + right64;
    return add_overflow_64(left64, right64, result64);
  }
  if (operation != 0u) {
    return false;
  }
  switch (function) {
  case 0x20u:
    result32 = left32 + right32;
    return add_overflow_32(left32, right32, result32);
  case 0x22u:
    result32 = left32 - right32;
    return subtract_overflow_32(left32, right32, result32);
  case 0x2cu:
    result64 = left64 + right64;
    return add_overflow_64(left64, right64, result64);
  case 0x2eu:
    result64 = left64 - right64;
    return subtract_overflow_64(left64, right64, result64);
  default: return false;
  }
}

static bool special_function_is_defined(uint32_t function) {
  switch (function) {
  case 0x00u: case 0x02u: case 0x03u: case 0x04u: case 0x06u: case 0x07u:
  case 0x08u: case 0x09u: case 0x0cu: case 0x0du: case 0x0fu:
  case 0x10u: case 0x11u: case 0x12u: case 0x13u: case 0x14u: case 0x16u:
  case 0x17u: case 0x18u: case 0x19u: case 0x1au: case 0x1bu: case 0x1cu:
  case 0x1du: case 0x1eu: case 0x1fu:
  case 0x20u: case 0x21u: case 0x22u: case 0x23u: case 0x24u: case 0x25u:
  case 0x26u: case 0x27u: case 0x2au: case 0x2bu: case 0x2cu: case 0x2du:
  case 0x2eu: case 0x2fu: case 0x30u: case 0x31u: case 0x32u: case 0x33u:
  case 0x34u: case 0x36u: case 0x38u: case 0x3au: case 0x3bu: case 0x3cu:
  case 0x3eu: case 0x3fu:
    return true;
  default: return false;
  }
}

static bool special_fields_are_reserved(uint32_t opcode) {
  uint32_t rs = (opcode >> 21) & UINT32_C(0x1f);
  uint32_t rt = (opcode >> 16) & UINT32_C(0x1f);
  uint32_t rd = (opcode >> 11) & UINT32_C(0x1f);
  uint32_t shift = (opcode >> 6) & UINT32_C(0x1f);
  uint32_t function = opcode & UINT32_C(0x3f);

  switch (function) {
  case 0x00u: case 0x02u: case 0x03u:
  case 0x38u: case 0x3au: case 0x3bu: case 0x3cu: case 0x3eu: case 0x3fu:
    return rs != 0u;
  case 0x04u: case 0x06u: case 0x07u:
  case 0x14u: case 0x16u: case 0x17u:
    return shift != 0u;
  case 0x08u: /* JR */
    return rt != 0u || rd != 0u || shift != 0u;
  case 0x09u: /* JALR */
    return rt != 0u || shift != 0u;
  case 0x10u: case 0x12u: /* MFHI/MFLO */
    return rs != 0u || rt != 0u || shift != 0u;
  case 0x11u: case 0x13u: /* MTHI/MTLO */
    return rt != 0u || rd != 0u || shift != 0u;
  case 0x18u: case 0x19u: case 0x1au: case 0x1bu:
  case 0x1cu: case 0x1du: case 0x1eu: case 0x1fu:
    return rd != 0u || shift != 0u;
  case 0x0cu: case 0x0du: /* SYSCALL/BREAK code fields */
  case 0x30u: case 0x31u: case 0x32u: case 0x33u: case 0x34u: case 0x36u:
    return false;
  case 0x0fu: /* SYNC: this profile defines only the stype-zero form. */
    return rs != 0u || rt != 0u || rd != 0u || shift != 0u;
  default:
    return shift != 0u;
  }
}

static bool cpu_instruction_is_reserved(uint32_t opcode) {
  uint32_t operation = opcode >> 26;
  uint32_t suboperation;

  if (t64_cop1_is_branch(opcode)) {
    return !t64_cop1_branch_encoding_valid(opcode);
  }

  if (operation == 0u) {
    return !special_function_is_defined(opcode & UINT32_C(0x3f)) ||
           special_fields_are_reserved(opcode);
  }
  if (operation == T64_CPU_REGIMM) {
    suboperation = (opcode >> 16) & UINT32_C(0x1f);
    return !((suboperation <= 3u) ||
             (suboperation >= 8u && suboperation <= 12u) ||
             suboperation == 14u ||
             (suboperation >= 16u && suboperation <= 19u));
  }
  if (operation == 0x06u || operation == 0x07u ||
      operation == 0x16u || operation == 0x17u) {
    return ((opcode >> 16) & UINT32_C(0x1f)) != 0u;
  }
  if (operation == 0x0fu) {
    return ((opcode >> 21) & UINT32_C(0x1f)) != 0u;
  }
  return operation == 0x1cu || operation == 0x1du ||
         operation == 0x1eu || operation == 0x1fu;
}

static bool cpu_coprocessor_unusable(const t64_machine *machine,
                                    uint32_t opcode, uint32_t *coprocessor_out) {
  uint32_t operation = opcode >> 26;
  uint32_t coprocessor = 0u;
  uint32_t status;
  uint32_t usable_mask;

  if (operation >= T64_CPU_COP0 && operation <= UINT32_C(0x13)) {
    coprocessor = operation - T64_CPU_COP0;
  } else if (operation == T64_CPU_CACHE) {
    coprocessor = 0u;
  } else {
    switch (operation) {
    case 0x31u: case 0x35u: case 0x39u: case 0x3du:
      coprocessor = 1u;
      break;
    case 0x32u: case 0x36u: case 0x3au: case 0x3eu:
      coprocessor = 2u;
      break;
    default: return false;
    }
  }
  status = (uint32_t)machine->cpu.cp0[T64_CP0_STATUS];
  usable_mask = UINT32_C(1) << (28u + coprocessor);
  if (coprocessor == 0u) {
    if (!t64_cp0_access_unusable(machine)) {
      return false;
    }
  } else if ((status & usable_mask) != 0u) {
    return false;
  }
  *coprocessor_out = coprocessor;
  return true;
}

static bool cpu_is_syscall(uint32_t opcode) {
  return (opcode >> 26) == 0u &&
         (opcode & UINT32_C(0x3f)) == UINT32_C(0x0c);
}

static bool cpu_is_break(uint32_t opcode) {
  return (opcode >> 26) == 0u &&
         (opcode & UINT32_C(0x3f)) == UINT32_C(0x0d);
}

static bool cpu_is_eret(uint32_t opcode) {
  return opcode == UINT32_C(0x42000018);
}

static bool cpu_instruction_exception(t64_machine *machine, uint32_t opcode,
                                      bool in_delay_slot,
                                      bool *trap_noop_out,
                                      t64_status *status_out) {
  uint32_t coprocessor = 0u;
  bool is_trap = false;

  *trap_noop_out = false;
  if (cpu_is_syscall(opcode)) {
    *status_out = cpu_enter_exception(machine, UINT32_C(8), 0u,
                                       in_delay_slot, false, 0u);
    return true;
  }
  if (cpu_is_break(opcode)) {
    *status_out = cpu_enter_exception(machine, UINT32_C(9), 0u,
                                       in_delay_slot, false, 0u);
    return true;
  }
  if (cpu_coprocessor_unusable(machine, opcode, &coprocessor)) {
    *status_out = cpu_enter_exception(machine, UINT32_C(11), 0u,
                                       in_delay_slot, true, coprocessor);
    return true;
  }
  if (cpu_instruction_is_reserved(opcode)) {
    *status_out = cpu_enter_exception(machine, UINT32_C(10), 0u,
                                       in_delay_slot, false, 0u);
    return true;
  }
  {
    bool trap_condition_is_true =
        cpu_test_trap(&machine->cpu, opcode, &is_trap);

    if (is_trap) {
      if (trap_condition_is_true) {
        *status_out = cpu_enter_exception(machine, UINT32_C(13), 0u,
                                           in_delay_slot, false, 0u);
        return true;
      }
      *trap_noop_out = true;
    }
  }
  if (cpu_instruction_overflows(machine, opcode)) {
    *status_out = cpu_enter_exception(machine, UINT32_C(12), 0u,
                                       in_delay_slot, false, 0u);
    return true;
  }
  return false;
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
  uint64_t pc_full;
  uint32_t physical_pc;
  uint32_t opcode = 0u;
  uint32_t next_pc;
  uint64_t next_pc_full;
  uint64_t target = 0u;
  uint32_t link_register = 0u;
  uint32_t destination = 0u;
  uint32_t fpr_destination = 0u;
  uint32_t fpr_width = 0u;
  uint64_t value = 0u;
  uint64_t fpr_value = 0u;
  t64_cp0_effect cp0_effect = {0};
  bool in_delay_slot;
  bool is_branch;
  bool branch_taken = false;
  bool branch_likely = false;
  bool link = false;
  bool write_gpr = false;
  bool write_fpr = false;
  bool clear_fcr31_causes = false;
  bool new_profile;
  bool trap_noop = false;
  bool memory_handled = false;
  bool is_eret = false;
  bool cp0_effect_prepared = false;
  bool supported;
  bool instruction_cacheable = false;
  uint64_t eret_target = 0u;
  t64_status status;
  t64_status memory_status = T64_STATUS_OK;

  (void)event;
  if (machine == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (machine->cpu.stopped) {
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }
  if (machine->cpu.tlb_shutdown) {
    cpu_stop_unsupported(machine, machine->cpu.pc,
                         machine->cpu.last_opcode);
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }
  new_profile = has_vr4300_cpu_state(machine);
  status = t64_cp0_synchronize_guest_time(machine, machine->tick);
  if (status != T64_STATUS_OK) {
    machine->cpu.attempt_outcome = T64_CPU_ATTEMPT_HOST_ERROR;
    return status;
  }
  pc_full = new_profile
                ? machine->cpu.pc_full
                : (uint64_t)machine->cpu.pc;
  pc = (uint32_t)pc_full;
  machine->cpu.pc = pc;
  in_delay_slot = machine->cpu.delay_slot_pending;
  machine->cpu.last_pc = pc;
  if (new_profile) {
    machine->cpu.last_pc_full = pc_full;
    if (t64_cp0_interrupt_pending(machine)) {
      machine->cpu.last_opcode = 0u;
      return cpu_enter_exception(machine, UINT32_C(0), 0u,
                                 in_delay_slot, false, 0u);
    }
    if ((pc_full & UINT64_C(3)) != 0u) {
      machine->cpu.last_opcode = 0u;
      return cpu_enter_exception(machine, UINT32_C(4), pc_full,
                                 in_delay_slot, false, 0u);
    }
  }
  {
    t64_mmu_result translation;
    uint64_t virtual_pc = new_profile ? pc_full : sign_extend_32(pc);

    if (!cpu_translate_access(machine, virtual_pc, UINT32_C(4),
                              T64_MMU_ACCESS_FETCH, &translation)) {
      machine->cpu.last_opcode = 0u;
      machine->cpu.attempt_outcome = T64_CPU_ATTEMPT_HOST_ERROR;
      return T64_STATUS_INVALID_ARGUMENT;
    }
    if (translation.fault != T64_MMU_FAULT_NONE ||
        !translation.translated ||
        translation.physical_address > UINT32_MAX) {
      machine->cpu.last_opcode = 0u;
      if (new_profile) {
        t64_mmu_request request = {
            virtual_pc, UINT32_C(4), T64_MMU_ACCESS_FETCH};

        return cpu_enter_mmu_fault(machine, &request, &translation,
                                   in_delay_slot, 0u);
      }
      cpu_stop_unsupported(machine, pc, 0u);
      return T64_STATUS_RESOURCE_EXHAUSTED;
    }
    physical_pc = (uint32_t)translation.physical_address;
    instruction_cacheable = translation.cacheable;
  }
  if (new_profile && instruction_cacheable) {
    status = t64_cpu_icache_fetch(machine, pc_full, physical_pc, &opcode);
  } else {
    status = t64_bus_read(machine, physical_pc, 4u, &opcode);
  }
  if (status != T64_STATUS_OK) {
    machine->cpu.last_opcode = 0u;
    if (new_profile && (status == T64_STATUS_BUS_UNMAPPED ||
                        status == T64_STATUS_BUS_RANGE)) {
      return cpu_enter_exception(machine, UINT32_C(6), pc_full,
                                 in_delay_slot, false, 0u);
    }
    if (new_profile && status == T64_STATUS_BUS_ALIGNMENT) {
      return cpu_enter_exception(machine, UINT32_C(4), pc_full,
                                 in_delay_slot, false, 0u);
    }
    if (status == T64_STATUS_BUS_UNSUPPORTED) {
      cpu_stop_unsupported(machine, pc, 0u);
      return T64_STATUS_RESOURCE_EXHAUSTED;
    }
    if (new_profile) {
      machine->cpu.attempt_outcome = T64_CPU_ATTEMPT_HOST_ERROR;
    }
    return status;
  }
  machine->cpu.last_opcode = opcode;
  if (machine->cpu.retired_instructions == UINT64_MAX ||
      machine->instruction_count == UINT64_MAX) {
    cpu_stop_unsupported(machine, pc, opcode);
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }

  /* Warm-v1 retains its qualified exact-encoding software stop. */
  if (!new_profile && opcode == T64_CPU_SOFTWARE_DEBUG_STOP_BREAK) {
    if (in_delay_slot) {
      cpu_stop_unsupported(machine, pc, opcode);
    } else {
      cpu_stop_completion(machine, pc, opcode);
    }
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }

  is_branch = is_branch_instruction(opcode, new_profile);

  if (new_profile) {
    if (cpu_instruction_exception(machine, opcode, in_delay_slot,
                                  &trap_noop, &status)) {
      return status;
    }
    if (cpu_is_eret(opcode)) {
      uint32_t cp0_status = (uint32_t)machine->cpu.cp0[T64_CP0_STATUS];

      if (in_delay_slot) {
        cpu_stop_unsupported(machine, pc, opcode);
        return T64_STATUS_RESOURCE_EXHAUSTED;
      }
      if ((cp0_status & T64_STATUS_ERL) != 0u) {
        eret_target = machine->cpu.cp0[T64_CP0_ERROR_EPC];
        machine->cpu.cp0[T64_CP0_STATUS] = cp0_status & ~T64_STATUS_ERL;
      } else {
        eret_target = machine->cpu.cp0[T64_CP0_EPC];
        machine->cpu.cp0[T64_CP0_STATUS] = cp0_status & ~T64_STATUS_EXL;
      }
      machine->cpu.linked_valid = false;
      machine->cpu.linked_width = 0u;
      machine->cpu.linked_physical_address = 0u;
      machine->cpu.pending_exception_valid = false;
      is_eret = true;
    } else {
      status = cpu_preflight_memory(machine, opcode, in_delay_slot,
                                    &memory_handled);
      if (memory_handled) {
        return status;
      }
    }
  }

  if (in_delay_slot && is_branch) {
    cpu_stop_unsupported(machine, pc, opcode);
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }
  if (is_branch) {
    supported = execute_branch(pc_full, new_profile, opcode, &machine->cpu,
                               &branch_taken, &branch_likely, &link,
                               &link_register, &target);
    if (!supported) {
      cpu_stop_unsupported(machine, pc, opcode);
      return T64_STATUS_RESOURCE_EXHAUSTED;
    }
  } else if (trap_noop) {
    supported = true;
  } else if (is_eret) {
    supported = true;
  } else if ((opcode >> 26) == T64_CPU_COP0) {
    if (new_profile) {
      t64_cp0_transfer_result cp0_result =
          execute_cp0_vr4300(machine, opcode, &write_gpr, &destination,
                             &value, &cp0_effect);

      if (cp0_result == T64_CP0_TRANSFER_RESERVED) {
        return cpu_enter_exception(machine, UINT32_C(10), 0u,
                                   in_delay_slot, false, 0u);
      }
      if (cp0_result == T64_CP0_TRANSFER_UNSUPPORTED) {
        cpu_stop_unsupported(machine, pc, opcode);
        return T64_STATUS_RESOURCE_EXHAUSTED;
      }
      cp0_effect_prepared = true;
      supported = true;
    } else {
      supported = execute_cp0_warm(machine, opcode, &write_gpr,
                                   &destination, &value);
    }
    if (!supported) {
      cpu_stop_unsupported(machine, pc, opcode);
      return T64_STATUS_RESOURCE_EXHAUSTED;
    }
  } else if (new_profile &&
             ((opcode >> 26) == T64_CPU_COP1 ||
              (opcode >> 26) == UINT32_C(0x31) ||
              (opcode >> 26) == UINT32_C(0x35) ||
              (opcode >> 26) == UINT32_C(0x39) ||
              (opcode >> 26) == UINT32_C(0x3d))) {
    t64_cpu_cop1_result cop1_result = {0};

    status = t64_cop1_execute(machine, opcode, in_delay_slot,
                              &cop1_result);
    if (status != T64_STATUS_OK) {
      uint32_t operation = opcode >> 26;
      bool store = operation == UINT32_C(0x39) ||
                   operation == UINT32_C(0x3d);

      if (status == T64_STATUS_BUS_ALIGNMENT) {
        uint64_t address = effective_address(
            &machine->cpu, (opcode >> 21) & UINT32_C(0x1f),
            (uint16_t)opcode);
        return cpu_enter_exception(machine, store ? UINT32_C(5) : UINT32_C(4),
                                   address, in_delay_slot, false, 0u);
      }
      if (status == T64_STATUS_BUS_UNMAPPED ||
          status == T64_STATUS_BUS_RANGE) {
        uint64_t address = effective_address(
            &machine->cpu, (opcode >> 21) & UINT32_C(0x1f),
            (uint16_t)opcode);
        return cpu_enter_exception(machine, UINT32_C(7), address,
                                   in_delay_slot, false, 0u);
      }
      if (status == T64_STATUS_BUS_UNSUPPORTED) {
        cpu_stop_unsupported(machine, pc, opcode);
        return T64_STATUS_RESOURCE_EXHAUSTED;
      }
      machine->cpu.attempt_outcome = T64_CPU_ATTEMPT_HOST_ERROR;
      return status;
    }
    if (cop1_result.trapped) {
      return T64_STATUS_OK;
    }
    if (!cop1_result.supported) {
      cpu_stop_unsupported(machine, pc, opcode);
      return T64_STATUS_RESOURCE_EXHAUSTED;
    }
    if (cop1_result.write_gpr) {
      write_gpr = true;
      destination = cop1_result.destination_gpr;
      value = cop1_result.gpr_value;
    }
    if (cop1_result.write_fpr) {
      write_fpr = true;
      fpr_destination = cop1_result.destination_fpr;
      fpr_width = cop1_result.fpr_width;
      fpr_value = cop1_result.result_bits;
    }
    clear_fcr31_causes = cop1_result.clear_fcr31_causes;
    supported = true;
  } else if ((opcode >> 26) == T64_CPU_CACHE) {
    t64_status cache_status = T64_STATUS_BUS_UNSUPPORTED;
    supported = execute_cache(machine, pc, opcode, &cache_status);
    if (!supported) {
      if (cache_status == T64_STATUS_BUS_UNMAPPED ||
          cache_status == T64_STATUS_BUS_RANGE) {
        uint64_t address = machine->cpu.gpr[(opcode >> 21) & UINT32_C(0x1f)] +
                           sign_extend_16((uint16_t)opcode);
        return cpu_enter_exception(machine, UINT32_C(7), address,
                                   in_delay_slot, false, 0u);
      }
      cpu_stop_unsupported(machine, pc, opcode);
      return T64_STATUS_RESOURCE_EXHAUSTED;
    }
  } else {
    supported = execute_integer(machine, opcode, new_profile, &write_gpr,
                                &destination, &value, &memory_status);
    if (!supported) {
      if (memory_status == T64_STATUS_BUS_UNMAPPED ||
          memory_status == T64_STATUS_BUS_RANGE) {
        uint64_t address = effective_address(
            &machine->cpu, (opcode >> 21) & UINT32_C(0x1f),
            (uint16_t)opcode);
        return cpu_enter_exception(machine, UINT32_C(7), address,
                                   in_delay_slot, false, 0u);
      }
      cpu_stop_unsupported(machine, pc, opcode);
      return T64_STATUS_RESOURCE_EXHAUSTED;
    }
  }

  if (write_gpr && destination != 0u) {
    machine->cpu.gpr[destination] = value;
  }
  if (write_fpr) {
    t64_cop1_commit_fpr(machine, fpr_destination, fpr_width, fpr_value);
  }
  if (clear_fcr31_causes) {
    machine->cpu.fcr31 &= ~T64_CPU_COP1_CAUSE_MASK;
  }
  if (link && (new_profile || !branch_likely || branch_taken) &&
      link_register != 0u) {
    machine->cpu.gpr[link_register] =
        new_profile ? pc_full + UINT64_C(8)
                    : sign_extend_32(pc + UINT32_C(8));
  }

  if (in_delay_slot) {
    next_pc_full = new_profile ? machine->cpu.delay_target_full
                               : (uint64_t)machine->cpu.delay_target;
    machine->cpu.delay_slot_pending = false;
    machine->cpu.delay_slot_pc = 0u;
    machine->cpu.delay_target = 0u;
    machine->cpu.delay_slot_pc_full = 0u;
    machine->cpu.delay_target_full = 0u;
  } else if (is_branch && branch_likely && !branch_taken) {
    next_pc_full = (new_profile ? pc_full : (uint64_t)pc) + UINT64_C(8);
    machine->cpu.delay_slot_pending = false;
    machine->cpu.delay_slot_pc = 0u;
    machine->cpu.delay_target = 0u;
    machine->cpu.delay_slot_pc_full = 0u;
    machine->cpu.delay_target_full = 0u;
  } else if (is_branch) {
    next_pc_full = (new_profile ? pc_full : (uint64_t)pc) + UINT64_C(4);
    machine->cpu.delay_slot_pending = true;
    machine->cpu.delay_slot_pc = (uint32_t)next_pc_full;
    machine->cpu.delay_target = (uint32_t)target;
    machine->cpu.delay_slot_pc_full = next_pc_full;
    machine->cpu.delay_target_full = target;
  } else {
    next_pc_full = is_eret ? eret_target
                           : (new_profile ? pc_full : (uint64_t)pc) +
                                 UINT64_C(4);
    machine->cpu.delay_slot_pending = false;
    machine->cpu.delay_slot_pc = 0u;
    machine->cpu.delay_target = 0u;
    machine->cpu.delay_slot_pc_full = 0u;
    machine->cpu.delay_target_full = 0u;
  }
  next_pc = (uint32_t)next_pc_full;
  machine->cpu.pc = next_pc;
  machine->cpu.gpr[0] = 0u;
  if (cp0_effect_prepared) {
    t64_cp0_commit_transfer(machine, &cp0_effect);
    if (machine->cpu.tlb_shutdown) {
      cpu_stop_unsupported(machine, pc, opcode);
      return T64_STATUS_RESOURCE_EXHAUSTED;
    }
  }
  machine->cpu.retired_instructions++;
  machine->instruction_count++;
  t64_cp0_instruction_retired(machine);
  if (!new_profile) {
    cp0_retirement_tick(machine);
  }
  if (has_vr4300_cpu_state(machine)) {
    machine->cpu.attempt_outcome = T64_CPU_ATTEMPT_RETIRED;
    machine->cpu.pc_full = next_pc_full;
    machine->cpu.last_pc_full = pc_full;
  }
  t64_trace_emit(machine, machine->tick, T64_TRACE_DEVICE_MACHINE,
                 T64_TRACE_KIND_CPU_RETIREMENT,
                 ((uint64_t)pc << 32) | (uint64_t)opcode);
  status = t64_cpu_schedule(machine);
  if (status != T64_STATUS_OK) {
    return status;
  }
  return T64_STATUS_OK;
}
