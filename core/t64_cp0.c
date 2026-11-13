#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define T64_COP0_OPCODE UINT32_C(0x10)

#define T64_CP0_INDEX UINT32_C(0)
#define T64_CP0_RANDOM UINT32_C(1)
#define T64_CP0_ENTRYLO0 UINT32_C(2)
#define T64_CP0_ENTRYLO1 UINT32_C(3)
#define T64_CP0_CONTEXT UINT32_C(4)
#define T64_CP0_PAGEMASK UINT32_C(5)
#define T64_CP0_WIRED UINT32_C(6)
#define T64_CP0_BADVADDR UINT32_C(8)
#define T64_CP0_COUNT UINT32_C(9)
#define T64_CP0_ENTRYHI UINT32_C(10)
#define T64_CP0_COMPARE UINT32_C(11)
#define T64_CP0_STATUS UINT32_C(12)
#define T64_CP0_CAUSE UINT32_C(13)
#define T64_CP0_EPC UINT32_C(14)
#define T64_CP0_PRID UINT32_C(15)
#define T64_CP0_CONFIG UINT32_C(16)
#define T64_CP0_LLADDR UINT32_C(17)
#define T64_CP0_WATCHLO UINT32_C(18)
#define T64_CP0_WATCHHI UINT32_C(19)
#define T64_CP0_XCONTEXT UINT32_C(20)
#define T64_CP0_PERR UINT32_C(26)
#define T64_CP0_CACHEERR UINT32_C(27)
#define T64_CP0_TAGLO UINT32_C(28)
#define T64_CP0_TAGHI UINT32_C(29)
#define T64_CP0_ERROREPC UINT32_C(30)

#define T64_STATUS_WRITABLE_MASK UINT32_C(0xf757ffff)
#define T64_STATUS_TS UINT32_C(0x00200000)
#define T64_STATUS_CU0 UINT32_C(0x10000000)
#define T64_STATUS_EXL UINT32_C(0x00000002)
#define T64_STATUS_ERL UINT32_C(0x00000004)
#define T64_STATUS_KSU_MASK UINT32_C(0x00000018)
#define T64_STATUS_UX UINT32_C(0x00000020)
#define T64_STATUS_SX UINT32_C(0x00000040)

#define T64_INDEX_PROBE UINT32_C(0x80000000)
#define T64_INDEX_FIELD UINT32_C(0x0000003f)
#define T64_RANDOM_SOFTWARE_BIT UINT32_C(0x00000020)
#define T64_RANDOM_INDEX_MASK UINT32_C(0x0000001f)
#define T64_RANDOM_READ_MASK UINT32_C(0x0000003f)
#define T64_ENTRYLO_MASK UINT32_C(0x03ffffff)
#define T64_CONTEXT_HARDWARE_MASK UINT64_C(0x00000000007ffff0)
#define T64_CONTEXT_PTEBASE32_MASK UINT64_C(0x00000000ff800000)
#define T64_CONTEXT_PTEBASE64_MASK UINT64_C(0xffffffff80000000)
#define T64_PAGEMASK_FIELD UINT32_C(0x01ffe000)
#define T64_ENTRYHI_REGION_MASK UINT64_C(0xc000000000000000)
#define T64_ENTRYHI_VPN_MASK UINT64_C(0x000000ffffffe000)
#define T64_ENTRYHI_ASID_MASK UINT64_C(0x00000000000000ff)
#define T64_ENTRYHI32_MASK UINT32_C(0xffffe0ff)
#define T64_ENTRYHI64_MASK UINT64_C(0xc00000ffffffe0ff)
#define T64_ENTRYHI_HIGH32_MASK UINT64_C(0xc00000ff00000000)
#define T64_CAUSE_SOFTWARE_MASK UINT32_C(0x00000300)
#define T64_CAUSE_IP2_MASK UINT32_C(0x00000400)
#define T64_CAUSE_INTERRUPT_MASK UINT32_C(0x0000ff00)
#define T64_CAUSE_DEFINED_MASK UINT32_C(0xb000ff7c)
#define T64_CAUSE_TIMER_MASK UINT32_C(0x00008000)
#define T64_STATUS_IE UINT32_C(0x00000001)
#define T64_STATUS_EXL UINT32_C(0x00000002)
#define T64_STATUS_ERL UINT32_C(0x00000004)
#define T64_COUNT_MODULUS UINT64_C(0x100000000)
#define T64_CONFIG_EP_MASK UINT32_C(0x0f000000)
#define T64_CONFIG_BE_MASK UINT32_C(0x00008000)
#define T64_CONFIG_CU_MASK UINT32_C(0x00000008)
#define T64_CONFIG_K0_MASK UINT32_C(0x00000007)
#define T64_CONFIG_WRITABLE_MASK                                             \
  (T64_CONFIG_EP_MASK | T64_CONFIG_BE_MASK | T64_CONFIG_CU_MASK |           \
   T64_CONFIG_K0_MASK)
#define T64_CONFIG_EP_DEFAULT UINT32_C(0x00000000)
#define T64_CONFIG_EP_BLOCK UINT32_C(0x06000000)
#define T64_PRID_VR4300_REV0 UINT32_C(0x00000b00)
#define T64_LLADDR_MASK UINT32_C(0x0fffffff)
#define T64_WATCHLO_MASK UINT32_C(0xfffffffb)
#define T64_WATCHHI_MASK UINT32_C(0x0000000f)
#define T64_XCONTEXT_HARDWARE_MASK UINT64_C(0x00000001fffffff0)
#define T64_XCONTEXT_PTEBASE_MASK UINT64_C(0xfffffffe00000000)
#define T64_TAGLO_MASK UINT32_C(0x0fffffc0)
#define T64_PERR_MASK UINT32_C(0x000000ff)

static uint64_t sign_extend_word(uint32_t value) {
  return t64_cpu_canonical_address(value);
}

static bool is_vr4300_profile(const t64_machine *machine) {
  return t64_has_vr4300_cpu_state(machine);
}

static bool is_cp0_kernel_mode(const t64_machine *machine) {
  uint32_t status = (uint32_t)machine->cpu.cp0[T64_CP0_STATUS];
  return (status & (T64_STATUS_EXL | T64_STATUS_ERL)) != 0u ||
         (status & T64_STATUS_KSU_MASK) == 0u;
}

bool t64_cp0_access_unusable(const t64_machine *machine) {
  uint32_t status;

  if (machine == NULL) {
    return true;
  }
  status = (uint32_t)machine->cpu.cp0[T64_CP0_STATUS];
  return !is_cp0_kernel_mode(machine) && (status & T64_STATUS_CU0) == 0u;
}

static bool doubleword_cp0_operation_is_reserved(const t64_machine *machine) {
  uint32_t status = (uint32_t)machine->cpu.cp0[T64_CP0_STATUS];
  uint32_t ksu = (status & T64_STATUS_KSU_MASK) >> 3;

  if (is_cp0_kernel_mode(machine)) {
    return false;
  }
  if (ksu == UINT32_C(2)) {
    return (status & T64_STATUS_UX) == 0u;
  }
  if (ksu == UINT32_C(1)) {
    return (status & T64_STATUS_SX) == 0u;
  }
  return true;
}

static bool cp0_register_is_reserved(uint32_t rd) {
  return rd == UINT32_C(7) || (rd >= UINT32_C(21) && rd <= UINT32_C(25)) ||
         rd == UINT32_C(31);
}

static bool cp0_register_is_64_bit(uint32_t rd) {
  return rd == T64_CP0_BADVADDR || rd == T64_CP0_CONTEXT ||
         rd == T64_CP0_ENTRYHI || rd == T64_CP0_EPC ||
         rd == T64_CP0_XCONTEXT || rd == T64_CP0_ERROREPC;
}

static uint64_t cp0_read_value(const t64_machine *machine, uint32_t rd) {
  uint64_t value = machine->cpu.cp0[rd];

  switch (rd) {
  case T64_CP0_INDEX:
    return value & (uint64_t)(T64_INDEX_PROBE | T64_INDEX_FIELD);
  case T64_CP0_RANDOM: return value & (uint64_t)T64_RANDOM_READ_MASK;
  case T64_CP0_ENTRYLO0:
  case T64_CP0_ENTRYLO1:
    return value & (uint64_t)T64_ENTRYLO_MASK;
  case T64_CP0_CONTEXT:
    return value & (T64_CONTEXT_PTEBASE64_MASK |
                    T64_CONTEXT_HARDWARE_MASK);
  case T64_CP0_PAGEMASK:
    return value & (uint64_t)T64_PAGEMASK_FIELD;
  case T64_CP0_WIRED:
    return value & (uint64_t)T64_INDEX_FIELD;
  case T64_CP0_BADVADDR: return value;
  case T64_CP0_COUNT:
  case T64_CP0_COMPARE:
    return value & UINT64_C(0xffffffff);
  case T64_CP0_ENTRYHI:
    return value & T64_ENTRYHI64_MASK;
  case T64_CP0_STATUS:
    return value & (uint64_t)(T64_STATUS_WRITABLE_MASK | T64_STATUS_TS);
  case T64_CP0_CAUSE:
    return value & (uint64_t)T64_CAUSE_DEFINED_MASK;
  case T64_CP0_EPC:
  case T64_CP0_ERROREPC: return value;
  case T64_CP0_PRID: return T64_PRID_VR4300_REV0;
  case T64_CP0_CONFIG:
    return value & UINT64_C(0xffffffff);
  case T64_CP0_LLADDR:
    return value & (uint64_t)T64_LLADDR_MASK;
  case T64_CP0_WATCHLO:
    return value & (uint64_t)T64_WATCHLO_MASK;
  case T64_CP0_WATCHHI:
    return value & (uint64_t)T64_WATCHHI_MASK;
  case T64_CP0_XCONTEXT:
    return value & (T64_XCONTEXT_PTEBASE_MASK |
                    T64_XCONTEXT_HARDWARE_MASK);
  case T64_CP0_PERR:
    return value & (uint64_t)T64_PERR_MASK;
  case T64_CP0_CACHEERR: return UINT64_C(0);
  case T64_CP0_TAGLO:
    return value & (uint64_t)T64_TAGLO_MASK;
  case T64_CP0_TAGHI: return UINT64_C(0);
  default: return UINT64_C(0);
  }
}

static bool config_write_is_supported(const t64_machine *machine,
                                      uint32_t source, uint32_t old_config) {
  uint32_t changed_init_fields = (source ^ old_config) &
                                (T64_CONFIG_EP_MASK | T64_CONFIG_BE_MASK);
  uint32_t ep = source & T64_CONFIG_EP_MASK;

  if (ep != T64_CONFIG_EP_DEFAULT && ep != T64_CONFIG_EP_BLOCK) {
    return false;
  }
  return changed_init_fields == 0u || machine->cpu.cp0_config_init_window;
}

static bool cp0_register_is_readable(uint32_t rd) {
  return rd <= T64_CP0_CONFIG || rd == T64_CP0_LLADDR ||
         rd == T64_CP0_WATCHLO || rd == T64_CP0_WATCHHI ||
         rd == T64_CP0_XCONTEXT || rd == T64_CP0_PERR ||
         rd == T64_CP0_CACHEERR || rd == T64_CP0_TAGLO ||
         rd == T64_CP0_TAGHI || rd == T64_CP0_ERROREPC;
}

static t64_cp0_transfer_result prepare_tlb_register_write(
    const t64_machine *machine, uint32_t operation, uint32_t rd,
    uint64_t source, t64_cp0_effect *effect_out) {
  uint64_t old_value = machine->cpu.cp0[rd];
  uint32_t word = (uint32_t)source;

  switch (rd) {
  case T64_CP0_INDEX:
    effect_out->write_cp0 = true;
    effect_out->cp0_value =
        (old_value & (uint64_t)T64_INDEX_PROBE) |
        (uint64_t)(word & T64_INDEX_FIELD);
    break;
  case T64_CP0_RANDOM:
    effect_out->write_cp0 = true;
    effect_out->cp0_value =
        (old_value & (uint64_t)T64_RANDOM_INDEX_MASK) |
        (uint64_t)(word & T64_RANDOM_SOFTWARE_BIT);
    break;
  case T64_CP0_ENTRYLO0:
  case T64_CP0_ENTRYLO1:
    effect_out->write_cp0 = true;
    effect_out->cp0_value = (uint64_t)(word & T64_ENTRYLO_MASK);
    break;
  case T64_CP0_CONTEXT:
    if (operation == UINT32_C(4)) {
      effect_out->cp0_value =
          (old_value & UINT64_C(0xffffffff00000000)) |
          (old_value & T64_CONTEXT_HARDWARE_MASK) |
          (uint64_t)(word & (uint32_t)T64_CONTEXT_PTEBASE32_MASK);
    } else {
      effect_out->cp0_value =
          (source & T64_CONTEXT_PTEBASE64_MASK) |
          (old_value & T64_CONTEXT_HARDWARE_MASK);
    }
    effect_out->write_cp0 = true;
    break;
  case T64_CP0_PAGEMASK:
    if (!t64_mmu_page_mask_is_defined((uint64_t)word) &&
        !t64_has_cpu_compat_extension_policy(machine)) {
      return T64_CP0_TRANSFER_UNSUPPORTED;
    }
    effect_out->write_cp0 = true;
    effect_out->cp0_value = (uint64_t)(word & T64_PAGEMASK_FIELD);
    break;
  case T64_CP0_WIRED:
    effect_out->write_cp0 = true;
    effect_out->cp0_value = (uint64_t)(word & T64_INDEX_FIELD);
    effect_out->reset_random = true;
    break;
  case T64_CP0_ENTRYHI:
    effect_out->write_cp0 = true;
    effect_out->cp0_value = operation == UINT32_C(4)
                                ? (old_value & T64_ENTRYHI_HIGH32_MASK) |
                                      (uint64_t)(word & T64_ENTRYHI32_MASK)
                                : source & T64_ENTRYHI64_MASK;
    break;
  case T64_CP0_XCONTEXT:
    effect_out->write_cp0 = true;
    effect_out->cp0_value = operation == UINT32_C(4)
                                ? old_value & ~UINT64_C(0x000000000000000f)
                                : (source & T64_XCONTEXT_PTEBASE_MASK) |
                                      (old_value & T64_XCONTEXT_HARDWARE_MASK);
    break;
  default: return T64_CP0_TRANSFER_RESERVED;
  }
  return T64_CP0_TRANSFER_OK;
}

static t64_cp0_transfer_result prepare_exception_register_write(
    const t64_machine *machine, uint32_t operation, uint32_t rd,
    uint64_t source, t64_cp0_effect *effect_out) {
  uint64_t old_value = machine->cpu.cp0[rd];
  uint32_t word = (uint32_t)source;

  switch (rd) {
  case T64_CP0_BADVADDR:
    /* Hardware-owned address diagnostic; writes have no architected effect. */
    break;
  case T64_CP0_COUNT:
    effect_out->write_cp0 = true;
    effect_out->cp0_value = (uint64_t)word;
    break;
  case T64_CP0_COMPARE:
    effect_out->write_cp0 = true;
    effect_out->cp0_value = (uint64_t)word;
    effect_out->clear_timer_pending = true;
    break;
  case T64_CP0_STATUS:
    effect_out->write_cp0 = true;
    effect_out->cp0_value =
        (uint64_t)((word & T64_STATUS_WRITABLE_MASK) |
                   ((uint32_t)old_value & T64_STATUS_TS));
    break;
  case T64_CP0_CAUSE:
    effect_out->write_cp0 = true;
    effect_out->cp0_value =
        (old_value & (uint64_t)(T64_CAUSE_DEFINED_MASK &
                                ~T64_CAUSE_SOFTWARE_MASK)) |
        (uint64_t)(word & T64_CAUSE_SOFTWARE_MASK);
    break;
  case T64_CP0_EPC:
  case T64_CP0_ERROREPC:
    effect_out->write_cp0 = true;
    effect_out->cp0_value = operation == UINT32_C(4)
                                ? (old_value & UINT64_C(0xffffffff00000000)) |
                                      (uint64_t)word
                                : source;
    break;
  default: return T64_CP0_TRANSFER_RESERVED;
  }
  return T64_CP0_TRANSFER_OK;
}

static t64_cp0_transfer_result prepare_system_register_write(
    const t64_machine *machine, uint32_t rd, uint64_t source,
    t64_cp0_effect *effect_out) {
  uint64_t old_value = machine->cpu.cp0[rd];
  uint32_t word = (uint32_t)source;

  switch (rd) {
  case T64_CP0_PRID:
  case T64_CP0_CACHEERR:
    /* Read-only implementation and diagnostic registers ignore writes. */
    break;
  case T64_CP0_CONFIG: {
    uint32_t old_config = (uint32_t)old_value;
    if (!config_write_is_supported(machine, word, old_config)) {
      return T64_CP0_TRANSFER_UNSUPPORTED;
    }
    effect_out->write_cp0 = true;
    effect_out->cp0_value = (uint64_t)(
        (old_config & ~T64_CONFIG_WRITABLE_MASK) |
        (word & T64_CONFIG_WRITABLE_MASK));
    break;
  }
  case T64_CP0_LLADDR:
    effect_out->write_cp0 = true;
    effect_out->cp0_value = (uint64_t)(word & T64_LLADDR_MASK);
    break;
  case T64_CP0_WATCHLO:
    effect_out->write_cp0 = true;
    effect_out->cp0_value = (uint64_t)(word & T64_WATCHLO_MASK);
    break;
  case T64_CP0_WATCHHI:
    effect_out->write_cp0 = true;
    effect_out->cp0_value = (uint64_t)(word & T64_WATCHHI_MASK);
    break;
  case T64_CP0_PERR:
    effect_out->write_cp0 = true;
    effect_out->cp0_value = (uint64_t)(word & T64_PERR_MASK);
    break;
  case T64_CP0_TAGLO:
    effect_out->write_cp0 = true;
    effect_out->cp0_value = (uint64_t)(word & T64_TAGLO_MASK);
    break;
  case T64_CP0_TAGHI:
    effect_out->write_cp0 = true;
    effect_out->cp0_value = UINT64_C(0);
    break;
  default: return T64_CP0_TRANSFER_RESERVED;
  }
  return T64_CP0_TRANSFER_OK;
}

static t64_cp0_transfer_result prepare_cp0_write(
    const t64_machine *machine, uint32_t operation, uint32_t rt, uint32_t rd,
    t64_cp0_effect *effect_out) {
  uint64_t source = machine->cpu.gpr[rt];

  effect_out->write_cp0 = false;
  effect_out->cp0_index = rd;
  switch (rd) {
  case T64_CP0_INDEX:
  case T64_CP0_RANDOM:
  case T64_CP0_ENTRYLO0:
  case T64_CP0_ENTRYLO1:
  case T64_CP0_CONTEXT:
  case T64_CP0_PAGEMASK:
  case T64_CP0_WIRED:
  case T64_CP0_ENTRYHI:
  case T64_CP0_XCONTEXT:
    return prepare_tlb_register_write(machine, operation, rd, source,
                                      effect_out);
  case T64_CP0_BADVADDR:
  case T64_CP0_COUNT:
  case T64_CP0_COMPARE:
  case T64_CP0_STATUS:
  case T64_CP0_CAUSE:
  case T64_CP0_EPC:
  case T64_CP0_ERROREPC:
    return prepare_exception_register_write(machine, operation, rd, source,
                                            effect_out);
  case T64_CP0_PRID:
  case T64_CP0_CONFIG:
  case T64_CP0_LLADDR:
  case T64_CP0_WATCHLO:
  case T64_CP0_WATCHHI:
  case T64_CP0_PERR:
  case T64_CP0_CACHEERR:
  case T64_CP0_TAGLO:
  case T64_CP0_TAGHI:
    return prepare_system_register_write(machine, rd, source, effect_out);
  default: return T64_CP0_TRANSFER_RESERVED;
  }
}

t64_cp0_transfer_result t64_cp0_prepare_transfer(
    const t64_machine *machine, uint32_t opcode, t64_cp0_effect *effect_out) {
  uint32_t operation;
  uint32_t rt;
  uint32_t rd;
  bool is_read;
  bool is_doubleword;

  if (machine == NULL || effect_out == NULL || !is_vr4300_profile(machine) ||
      (opcode >> 26) != T64_COP0_OPCODE) {
    return T64_CP0_TRANSFER_UNSUPPORTED;
  }
  (void)memset(effect_out, 0, sizeof *effect_out);
  operation = (opcode >> 21) & UINT32_C(0x1f);
  rt = (opcode >> 16) & UINT32_C(0x1f);
  rd = (opcode >> 11) & UINT32_C(0x1f);
  if ((opcode & UINT32_C(0x7ff)) != 0u) {
    return T64_CP0_TRANSFER_RESERVED;
  }
  is_read = operation == UINT32_C(0) || operation == UINT32_C(1);
  is_doubleword = operation == UINT32_C(1) || operation == UINT32_C(5);
  if (!is_read && operation != UINT32_C(4) && operation != UINT32_C(5)) {
    return T64_CP0_TRANSFER_RESERVED;
  }
  if (cp0_register_is_reserved(rd)) {
    return T64_CP0_TRANSFER_RESERVED;
  }
  if (is_doubleword && doubleword_cp0_operation_is_reserved(machine)) {
    return T64_CP0_TRANSFER_RESERVED;
  }
  if (is_doubleword && !cp0_register_is_64_bit(rd) &&
      !t64_has_cpu_compat_policy(machine)) {
    return T64_CP0_TRANSFER_UNSUPPORTED;
  }

  if (is_read) {
    uint64_t value;
    if (!cp0_register_is_readable(rd)) {
      return T64_CP0_TRANSFER_RESERVED;
    }
    value = cp0_read_value(machine, rd);
    effect_out->write_gpr = true;
    effect_out->gpr_index = rt;
    effect_out->gpr_value = is_doubleword
                                ? (cp0_register_is_64_bit(rd)
                                       ? value
                                       : (uint64_t)(uint32_t)value)
                                : sign_extend_word((uint32_t)value);
    return T64_CP0_TRANSFER_OK;
  }
  return prepare_cp0_write(machine, operation, rt, rd, effect_out);
}

t64_cp0_transfer_result t64_cp0_prepare_tlb_instruction(
    const t64_machine *machine, uint32_t opcode,
    t64_cp0_effect *effect_out) {
  uint32_t function;
  uint32_t index = UINT32_MAX;
  bool duplicate = false;

  if (machine == NULL || effect_out == NULL || !is_vr4300_profile(machine) ||
      (opcode >> 26) != T64_COP0_OPCODE ||
      ((opcode >> 21) & UINT32_C(0x1f)) != UINT32_C(0x10) ||
      ((opcode >> 16) & UINT32_C(0x1f)) != 0u ||
      ((opcode >> 11) & UINT32_C(0x1f)) != 0u ||
      ((opcode >> 6) & UINT32_C(0x1f)) != 0u ||
      (opcode & UINT32_C(0x000007c0)) != 0u) {
    return T64_CP0_TRANSFER_RESERVED;
  }
  (void)memset(effect_out, 0, sizeof *effect_out);
  function = opcode & UINT32_C(0x3f);
  switch (function) {
  case UINT32_C(0x01):
    effect_out->tlb_operation = T64_CP0_TLB_READ;
    effect_out->tlb_index =
        (uint32_t)machine->cpu.cp0[T64_CP0_INDEX] & T64_RANDOM_INDEX_MASK;
    return T64_CP0_TRANSFER_OK;
  case UINT32_C(0x02):
    if (!t64_mmu_page_mask_is_defined(
            machine->cpu.cp0[T64_CP0_PAGEMASK]) &&
        !t64_has_cpu_compat_extension_policy(machine)) {
      return T64_CP0_TRANSFER_UNSUPPORTED;
    }
    effect_out->tlb_operation = T64_CP0_TLB_WRITE_INDEXED;
    effect_out->tlb_index =
        (uint32_t)machine->cpu.cp0[T64_CP0_INDEX] & T64_RANDOM_INDEX_MASK;
    return T64_CP0_TRANSFER_OK;
  case UINT32_C(0x06):
    if (!t64_mmu_page_mask_is_defined(
            machine->cpu.cp0[T64_CP0_PAGEMASK]) &&
        !t64_has_cpu_compat_extension_policy(machine)) {
      return T64_CP0_TRANSFER_UNSUPPORTED;
    }
    effect_out->tlb_operation = T64_CP0_TLB_WRITE_RANDOM;
    effect_out->tlb_index = t64_cp0_random_tlb_index(machine);
    return T64_CP0_TRANSFER_OK;
  case UINT32_C(0x08):
    if (!t64_mmu_probe(machine, machine->cpu.cp0[T64_CP0_ENTRYHI], &index,
                       &duplicate)) {
      return T64_CP0_TRANSFER_UNSUPPORTED;
    }
    effect_out->tlb_operation = T64_CP0_TLB_PROBE;
    effect_out->tlb_index = index;
    effect_out->tlb_duplicate = duplicate;
    effect_out->write_cp0 = true;
    effect_out->cp0_index = T64_CP0_INDEX;
    if (duplicate || index == UINT32_MAX) {
      effect_out->cp0_value =
          (machine->cpu.cp0[T64_CP0_INDEX] &
           (uint64_t)(T64_INDEX_FIELD & ~UINT32_C(0x1f))) |
          (uint64_t)T64_INDEX_PROBE;
    } else {
      effect_out->cp0_value =
          (machine->cpu.cp0[T64_CP0_INDEX] &
           (uint64_t)(T64_INDEX_FIELD & ~UINT32_C(0x1f))) |
          (uint64_t)index;
    }
    return T64_CP0_TRANSFER_OK;
  default: return T64_CP0_TRANSFER_RESERVED;
  }
}

static void refresh_count_compare_deadline(t64_machine *machine) {
  uint64_t count_distance;
  uint64_t ticks_until_equality;
  uint32_t count = (uint32_t)machine->cpu.cp0[T64_CP0_COUNT];
  uint32_t compare = (uint32_t)machine->cpu.cp0[T64_CP0_COMPARE];

  count_distance = (uint64_t)(uint32_t)(compare - count);
  if (count_distance == 0u) {
    count_distance = T64_COUNT_MODULUS;
  }
  ticks_until_equality = count_distance * UINT64_C(2) -
                         (uint64_t)machine->cpu.count_half_clock_phase;
  if (ticks_until_equality > UINT64_MAX - machine->cpu.count_sync_tick) {
    machine->cpu.count_compare_deadline = UINT64_MAX;
  } else {
    machine->cpu.count_compare_deadline =
        machine->cpu.count_sync_tick + ticks_until_equality;
  }
}

t64_status t64_cp0_synchronize_guest_time(t64_machine *machine,
                                          uint64_t tick) {
  uint64_t elapsed_ticks;
  uint64_t count_increments;
  uint64_t equality_distance;
  uint32_t count;
  uint32_t compare;
  uint32_t next_count;
  uint32_t next_phase;
  uint32_t cause;
  uint32_t phase;

  if (machine == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!is_vr4300_profile(machine)) {
    return T64_STATUS_OK;
  }
  if (tick < machine->cpu.count_sync_tick) {
    return T64_STATUS_TIME_REVERSED;
  }
  phase = machine->cpu.count_half_clock_phase;
  if (phase > 1u) {
    return T64_STATUS_INVALID_ARGUMENT;
  }

  elapsed_ticks = tick - machine->cpu.count_sync_tick;
  if (elapsed_ticks == 0u) {
    return T64_STATUS_OK;
  }
  count_increments = elapsed_ticks / UINT64_C(2) +
                     ((elapsed_ticks % UINT64_C(2) + (uint64_t)phase) /
                      UINT64_C(2));
  next_phase = (uint32_t)((elapsed_ticks % UINT64_C(2) +
                           (uint64_t)phase) % UINT64_C(2));
  count = (uint32_t)machine->cpu.cp0[T64_CP0_COUNT];
  compare = (uint32_t)machine->cpu.cp0[T64_CP0_COMPARE];
  next_count = count + (uint32_t)count_increments;
  if (count_increments != 0u) {
    equality_distance = (uint64_t)(uint32_t)(compare - count);
    if (equality_distance == 0u) {
      equality_distance = T64_COUNT_MODULUS;
    }
    if (count_increments >= equality_distance) {
      uint64_t equality_count =
          UINT64_C(1) +
          (count_increments - equality_distance) / T64_COUNT_MODULUS;

      cause = (uint32_t)machine->cpu.cp0[T64_CP0_CAUSE];
      machine->cpu.cp0[T64_CP0_CAUSE] = cause | T64_CAUSE_TIMER_MASK;
      machine->cpu.count_equality_latch_generation += equality_count;
      machine->cpu.count_equality_latched = next_count == compare;
    } else if (next_count != compare) {
      machine->cpu.count_equality_latched = false;
    }
  }
  machine->cpu.cp0[T64_CP0_COUNT] = (uint64_t)next_count;
  machine->cpu.count_half_clock_phase = next_phase;
  machine->cpu.count_sync_tick = tick;
  refresh_count_compare_deadline(machine);
  return T64_STATUS_OK;
}

bool t64_cp0_interrupt_pending(t64_machine *machine) {
  uint32_t cause;
  uint32_t status;

  if (machine == NULL || !is_vr4300_profile(machine)) {
    return false;
  }
  cause = (uint32_t)machine->cpu.cp0[T64_CP0_CAUSE];
  if (machine->mi_irq_line) {
    cause |= T64_CAUSE_IP2_MASK;
  } else {
    cause &= ~T64_CAUSE_IP2_MASK;
  }
  machine->cpu.cp0[T64_CP0_CAUSE] = cause;
  status = (uint32_t)machine->cpu.cp0[T64_CP0_STATUS];
  return (status & T64_STATUS_IE) != 0u &&
         (status & (T64_STATUS_EXL | T64_STATUS_ERL)) == 0u &&
         (status & cause & T64_CAUSE_INTERRUPT_MASK) != 0u;
}

void t64_cp0_commit_transfer(t64_machine *machine,
                             const t64_cp0_effect *effect) {
  if (machine == NULL || effect == NULL) {
    return;
  }
  if (effect->write_cp0 && effect->cp0_index < 32u) {
    if (effect->cp0_index == T64_CP0_COUNT ||
        effect->cp0_index == T64_CP0_COMPARE) {
      if (t64_cp0_synchronize_guest_time(machine, machine->tick) !=
          T64_STATUS_OK) {
        return;
      }
    }
    machine->cpu.cp0[effect->cp0_index] = effect->cp0_value;
    if (effect->cp0_index == T64_CP0_COUNT) {
      uint32_t count = (uint32_t)effect->cp0_value;
      uint32_t compare = (uint32_t)machine->cpu.cp0[T64_CP0_COMPARE];

      machine->cpu.count_equality_latched = count == compare;
      if (count == compare) {
        machine->cpu.cp0[T64_CP0_CAUSE] =
            (uint32_t)machine->cpu.cp0[T64_CP0_CAUSE] |
            T64_CAUSE_TIMER_MASK;
        machine->cpu.count_equality_latch_generation++;
      }
      refresh_count_compare_deadline(machine);
    } else if (effect->cp0_index == T64_CP0_COMPARE) {
      machine->cpu.count_equality_latched =
          (uint32_t)machine->cpu.cp0[T64_CP0_COUNT] ==
          (uint32_t)effect->cp0_value;
      refresh_count_compare_deadline(machine);
    }
  }
  if (effect->clear_timer_pending) {
    machine->cpu.cp0[T64_CP0_CAUSE] =
        (uint32_t)machine->cpu.cp0[T64_CP0_CAUSE] & ~T64_CAUSE_TIMER_MASK;
  }
  if (effect->reset_random) {
    machine->cpu.cp0[T64_CP0_RANDOM] = UINT32_C(31);
  }
  if (effect->tlb_operation == T64_CP0_TLB_READ) {
    const t64_vr4300_tlb_pair *entry =
        &machine->cpu.tlb[effect->tlb_index & T64_RANDOM_INDEX_MASK];

    machine->cpu.cp0[T64_CP0_PAGEMASK] =
        t64_has_cpu_compat_extension_policy(machine)
            ? t64_mmu_compat_normalize_page_mask(entry->page_mask)
            : entry->page_mask;
    machine->cpu.cp0[T64_CP0_ENTRYHI] =
        ((entry->region << 62) & T64_ENTRYHI_REGION_MASK) |
        (entry->vpn2 & T64_ENTRYHI_VPN_MASK) |
        ((uint64_t)entry->asid & T64_ENTRYHI_ASID_MASK);
    machine->cpu.cp0[T64_CP0_ENTRYLO0] =
        entry->entry_lo0 & (uint64_t)T64_ENTRYLO_MASK;
    machine->cpu.cp0[T64_CP0_ENTRYLO1] =
        entry->entry_lo1 & (uint64_t)T64_ENTRYLO_MASK;
  } else if (effect->tlb_operation == T64_CP0_TLB_WRITE_INDEXED ||
             effect->tlb_operation == T64_CP0_TLB_WRITE_RANDOM) {
    t64_vr4300_tlb_pair *entry =
        &machine->cpu.tlb[effect->tlb_index & T64_RANDOM_INDEX_MASK];
    uint64_t entry_hi = machine->cpu.cp0[T64_CP0_ENTRYHI];

    entry->page_mask = machine->cpu.cp0[T64_CP0_PAGEMASK] &
                       (uint64_t)T64_PAGEMASK_FIELD;
    entry->region = (entry_hi >> 62) & UINT64_C(3);
    entry->vpn2 = entry_hi & T64_ENTRYHI_VPN_MASK;
    entry->asid = (uint32_t)(entry_hi & T64_ENTRYHI_ASID_MASK);
    entry->entry_lo0 = machine->cpu.cp0[T64_CP0_ENTRYLO0] &
                       (uint64_t)T64_ENTRYLO_MASK;
    entry->entry_lo1 = machine->cpu.cp0[T64_CP0_ENTRYLO1] &
                       (uint64_t)T64_ENTRYLO_MASK;
  }
  if (effect->tlb_duplicate) {
    machine->cpu.tlb_shutdown = true;
    machine->cpu.cp0[T64_CP0_STATUS] =
        (uint32_t)machine->cpu.cp0[T64_CP0_STATUS] | T64_STATUS_TS;
  }
}

uint32_t t64_cp0_random_tlb_index(const t64_machine *machine) {
  if (machine == NULL) {
    return UINT32_C(0);
  }
  return (uint32_t)machine->cpu.cp0[T64_CP0_RANDOM] & T64_RANDOM_INDEX_MASK;
}

void t64_cp0_instruction_retired(t64_machine *machine) {
  uint32_t wired;
  uint32_t random_index;
  uint32_t software_bit;

  if (!is_vr4300_profile(machine)) {
    return;
  }
  wired = (uint32_t)machine->cpu.cp0[T64_CP0_WIRED] & T64_INDEX_FIELD;
  random_index = t64_cp0_random_tlb_index(machine);
  if (t64_has_cpu_compat_extension_policy(machine) &&
      (wired & UINT32_C(0x20)) != 0u) {
    random_index =
        (((uint32_t)machine->cpu.cp0[T64_CP0_RANDOM] - UINT32_C(1)) &
         T64_RANDOM_READ_MASK);
    machine->cpu.cp0[T64_CP0_RANDOM] = (uint64_t)random_index;
    return;
  }
  wired &= T64_RANDOM_INDEX_MASK;
  software_bit = (uint32_t)machine->cpu.cp0[T64_CP0_RANDOM] &
                 T64_RANDOM_SOFTWARE_BIT;
  if (random_index <= wired) {
    random_index = T64_RANDOM_INDEX_MASK;
  } else {
    random_index--;
  }
  machine->cpu.cp0[T64_CP0_RANDOM] = software_bit | random_index;
}
