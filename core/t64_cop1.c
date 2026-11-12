#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>

#define T64_STATUS_FR UINT32_C(0x04000000)
#define T64_STATUS_EXL UINT32_C(0x00000002)
#define T64_STATUS_ERL UINT32_C(0x00000004)
#define T64_STATUS_KSU UINT32_C(0x00000018)
#define T64_STATUS_USER UINT32_C(0x00000010)
#define T64_STATUS_SUPERVISOR UINT32_C(0x00000008)
#define T64_STATUS_UX UINT32_C(0x00000020)
#define T64_STATUS_SX UINT32_C(0x00000040)

#define T64_COP1_CAUSE_E UINT32_C(0x00020000)
#define T64_COP1_CAUSE_MASK UINT32_C(0x0003f000)
#define T64_COP1_CONTROL_MASK                                                \
  (T64_COP1_CAUSE_MASK | UINT32_C(0x00000f80) | UINT32_C(0x0000007c) |     \
   UINT32_C(0x01000000) | UINT32_C(0x00800000) | UINT32_C(0x00000003))

static int32_t cop1_i32_from_bits(uint32_t bits) {
  if (bits <= (uint32_t)INT32_MAX) {
    return (int32_t)bits;
  }
  return -1 - (int32_t)~bits;
}

static uint64_t cop1_sign_extend32(uint32_t bits) {
  return (bits & UINT32_C(0x80000000)) != 0u
             ? UINT64_C(0xffffffff00000000) | (uint64_t)bits
             : (uint64_t)bits;
}

static bool cop1_fr(const t64_machine *machine) {
  return ((uint32_t)machine->cpu.cp0[12] & T64_STATUS_FR) != 0u;
}

static bool cop1_mode_allows_doubleword(const t64_machine *machine) {
  uint32_t status = (uint32_t)machine->cpu.cp0[12];
  uint32_t privilege = status & T64_STATUS_KSU;

  if ((status & (T64_STATUS_EXL | T64_STATUS_ERL)) != 0u || privilege == 0u) {
    return true;
  }
  if (privilege == T64_STATUS_USER) {
    return (status & T64_STATUS_UX) != 0u;
  }
  if (privilege == T64_STATUS_SUPERVISOR) {
    return (status & T64_STATUS_SX) != 0u;
  }
  return false;
}

static uint32_t cop1_fgr_word(const t64_machine *machine, uint32_t reg) {
  return (uint32_t)machine->cpu.fpr[reg];
}

static bool cop1_fpr_read(const t64_machine *machine, uint32_t reg,
                          uint32_t width, uint64_t *bits_out) {
  if (machine == NULL || bits_out == NULL || reg >= 32u ||
      (width != 32u && width != 64u)) {
    return false;
  }
  if (width == 32u) {
    *bits_out = (uint64_t)cop1_fgr_word(machine, reg);
    return true;
  }
  if (cop1_fr(machine)) {
    *bits_out = machine->cpu.fpr[reg];
    return true;
  }
  if ((reg & 1u) != 0u || reg >= 31u) {
    return false;
  }
  *bits_out = ((uint64_t)cop1_fgr_word(machine, reg + 1u) << 32) |
              (uint64_t)cop1_fgr_word(machine, reg);
  return true;
}

void t64_cop1_commit_fpr(t64_machine *machine, uint32_t reg,
                         uint32_t width, uint64_t bits) {
  if (machine == NULL || reg >= 32u || (width != 32u && width != 64u)) {
    return;
  }
  if (cop1_fr(machine)) {
    if (width == 32u) {
      machine->cpu.fpr[reg] =
          (machine->cpu.fpr[reg] & UINT64_C(0xffffffff00000000)) |
          (uint64_t)(uint32_t)bits;
    } else {
      machine->cpu.fpr[reg] = bits;
    }
    return;
  }
  if (width == 32u) {
    machine->cpu.fpr[reg] =
        (machine->cpu.fpr[reg] & UINT64_C(0xffffffff00000000)) |
        (uint64_t)(uint32_t)bits;
  } else if ((reg & 1u) == 0u && reg < 31u) {
    machine->cpu.fpr[reg] =
        (machine->cpu.fpr[reg] & UINT64_C(0xffffffff00000000)) |
        (uint64_t)(uint32_t)bits;
    machine->cpu.fpr[reg + 1u] =
        (machine->cpu.fpr[reg + 1u] & UINT64_C(0xffffffff00000000)) |
        (uint64_t)(uint32_t)(bits >> 32);
  }
}

static t64_status cop1_enter_exception(t64_machine *machine, uint32_t code,
                                       bool in_delay_slot) {
  t64_cpu_exception_result exception = {0};

  exception.valid = true;
  exception.code = code;
  exception.epc = in_delay_slot
                      ? machine->cpu.delay_slot_pc_full - UINT64_C(4)
                      : machine->cpu.pc_full;
  exception.branch_delay = in_delay_slot;
  return t64_cpu_exception_enter(machine, &exception);
}

static t64_status cop1_raise_unimplemented(t64_machine *machine,
                                           bool in_delay_slot) {
  machine->cpu.fcr31 =
      (machine->cpu.fcr31 & ~T64_COP1_CAUSE_MASK) | T64_COP1_CAUSE_E;
  return cop1_enter_exception(machine, UINT32_C(15), in_delay_slot);
}

static t64_status cop1_raise_reserved(t64_machine *machine,
                                     bool in_delay_slot) {
  return cop1_enter_exception(machine, UINT32_C(10), in_delay_slot);
}

static bool cop1_transfer_encoding_valid(uint32_t opcode) {
  return (opcode & UINT32_C(0x7ff)) == 0u;
}

static t64_status cop1_execute_memory(t64_machine *machine, uint32_t opcode,
                                      bool in_delay_slot,
                                      t64_cpu_cop1_result *result_out) {
  uint32_t operation = opcode >> 26;
  uint32_t reg = (opcode >> 16) & UINT32_C(0x1f);
  bool load = operation == UINT32_C(0x31) || operation == UINT32_C(0x35);
  uint32_t width = operation == UINT32_C(0x31) ||
                           operation == UINT32_C(0x39)
                       ? 32u
                       : 64u;
  uint64_t value = 0u;
  uint64_t store_value = 0u;
  bool supported = false;
  t64_status status;

  if (width == 64u && !cop1_fr(machine) && (reg & 1u) != 0u) {
    result_out->supported = true;
    result_out->trapped = true;
    return cop1_raise_unimplemented(machine, in_delay_slot);
  }
  if (!load && !cop1_fpr_read(machine, reg, width, &store_value)) {
    result_out->supported = true;
    result_out->trapped = true;
    return cop1_raise_unimplemented(machine, in_delay_slot);
  }
  status = t64_memory_execute_cop1_status(machine, opcode, store_value,
                                          &supported, &value);
  if (status != T64_STATUS_OK) {
    return status;
  }
  if (!supported) {
    return T64_STATUS_OK;
  }
  result_out->supported = true;
  if (load) {
    result_out->write_fpr = true;
    result_out->destination_fpr = reg;
    result_out->fpr_width = width;
    result_out->result_bits = value;
  }
  return T64_STATUS_OK;
}

t64_status t64_cop1_execute(t64_machine *machine, uint32_t opcode,
                            bool in_delay_slot,
                            t64_cpu_cop1_result *result_out) {
  uint32_t operation;
  uint32_t format;
  uint32_t rt;
  uint32_t fs;
  uint32_t fd;
  uint32_t function;
  uint64_t fpr_value;
  t64_softfloat_context context;
  t64_softfloat_result conversion;
  t64_status status;

  if (machine == NULL || result_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  *result_out = (t64_cpu_cop1_result){0};
  operation = opcode >> 26;
  if (operation == UINT32_C(0x31) || operation == UINT32_C(0x35) ||
      operation == UINT32_C(0x39) || operation == UINT32_C(0x3d)) {
    return cop1_execute_memory(machine, opcode, in_delay_slot, result_out);
  }
  if (operation != UINT32_C(0x11)) {
    return T64_STATUS_OK;
  }

  format = (opcode >> 21) & UINT32_C(0x1f);
  rt = (opcode >> 16) & UINT32_C(0x1f);
  fs = (opcode >> 11) & UINT32_C(0x1f);
  fd = (opcode >> 6) & UINT32_C(0x1f);
  function = opcode & UINT32_C(0x3f);

  if (format == 0u || format == 1u || format == 2u || format == 4u ||
      format == 5u || format == 6u) {
    if (!cop1_transfer_encoding_valid(opcode)) {
      result_out->supported = true;
      result_out->trapped = true;
      return cop1_raise_reserved(machine, in_delay_slot);
    }
    switch (format) {
    case 0u: /* MFC1 */
      result_out->supported = true;
      result_out->write_gpr = true;
      result_out->destination_gpr = rt;
      result_out->gpr_value =
          cop1_sign_extend32(cop1_fgr_word(machine, fs));
      return T64_STATUS_OK;
    case 1u: /* DMFC1 */
      if (!cop1_mode_allows_doubleword(machine)) {
        result_out->supported = true;
        result_out->trapped = true;
        return cop1_raise_reserved(machine, in_delay_slot);
      }
      if (!cop1_fpr_read(machine, fs, 64u, &fpr_value)) {
        result_out->supported = true;
        result_out->trapped = true;
        return cop1_raise_unimplemented(machine, in_delay_slot);
      }
      result_out->supported = true;
      result_out->write_gpr = true;
      result_out->destination_gpr = rt;
      result_out->gpr_value = fpr_value;
      return T64_STATUS_OK;
    case 2u: /* CFC1 */
      if (fs != 0u && fs != 31u) {
        result_out->supported = true;
        result_out->trapped = true;
        return cop1_raise_reserved(machine, in_delay_slot);
      }
      result_out->supported = true;
      result_out->write_gpr = true;
      result_out->destination_gpr = rt;
      result_out->gpr_value = cop1_sign_extend32(
          fs == 0u ? machine->cpu.fcr0 : machine->cpu.fcr31);
      return T64_STATUS_OK;
    case 4u: /* MTC1 */
      result_out->supported = true;
      result_out->write_fpr = true;
      result_out->destination_fpr = fs;
      result_out->fpr_width = 32u;
      result_out->result_bits = (uint32_t)machine->cpu.gpr[rt];
      return T64_STATUS_OK;
    case 5u: /* DMTC1 */
      if (!cop1_mode_allows_doubleword(machine)) {
        result_out->supported = true;
        result_out->trapped = true;
        return cop1_raise_reserved(machine, in_delay_slot);
      }
      if (!cop1_fr(machine) && (fs & 1u) != 0u) {
        result_out->supported = true;
        result_out->trapped = true;
        return cop1_raise_unimplemented(machine, in_delay_slot);
      }
      result_out->supported = true;
      result_out->write_fpr = true;
      result_out->destination_fpr = fs;
      result_out->fpr_width = 64u;
      result_out->result_bits = machine->cpu.gpr[rt];
      return T64_STATUS_OK;
    case 6u: /* CTC1 */
      if (fs != 0u && fs != 31u) {
        result_out->supported = true;
        result_out->trapped = true;
        return cop1_raise_reserved(machine, in_delay_slot);
      }
      result_out->supported = true;
      if (fs == 31u) {
        uint32_t next = (uint32_t)machine->cpu.gpr[rt] &
                        T64_COP1_CONTROL_MASK;
        uint32_t causes = (next >> 12) & UINT32_C(0x1f);
        uint32_t enables = (next >> 7) & UINT32_C(0x1f);
        machine->cpu.fcr31 = next;
        if ((next & T64_COP1_CAUSE_E) != 0u ||
            (causes & enables) != 0u) {
          result_out->trapped = true;
          return cop1_enter_exception(machine, UINT32_C(15),
                                      in_delay_slot);
        }
      }
      return T64_STATUS_OK;
    default: break;
    }
  }

  if (format == UINT32_C(0x14) && rt == 0u && function == UINT32_C(0x21)) {
    if ((!cop1_fr(machine) && ((fs & 1u) != 0u || (fd & 1u) != 0u)) ||
        !cop1_fpr_read(machine, fs, 32u, &fpr_value)) {
      result_out->supported = true;
      result_out->trapped = true;
      return cop1_raise_unimplemented(machine, in_delay_slot);
    }
    context = (t64_softfloat_context){
        (uint8_t)(machine->cpu.fcr31 & UINT32_C(3)),
        T64_SOFTFLOAT_TININESS_AFTER_ROUNDING, 0u};
    status = t64_softfloat_i32_to_f64(cop1_i32_from_bits((uint32_t)fpr_value),
                                      &context, &conversion);
    if (status != T64_STATUS_OK) {
      machine->cpu.attempt_outcome = T64_CPU_ATTEMPT_HOST_ERROR;
      return status;
    }
    result_out->supported = true;
    result_out->write_fpr = true;
    result_out->destination_fpr = fd;
    result_out->fpr_width = 64u;
    result_out->result_bits = conversion.bits;
    result_out->fcr31_flags = conversion.exception_flags;
    return T64_STATUS_OK;
  }

  result_out->supported = true;
  result_out->trapped = true;
  return cop1_raise_reserved(machine, in_delay_slot);
}
