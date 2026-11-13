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
#define T64_COP1_CAUSE_V UINT32_C(0x00010000)
#define T64_COP1_CAUSE_Z UINT32_C(0x00008000)
#define T64_COP1_CAUSE_O UINT32_C(0x00004000)
#define T64_COP1_CAUSE_U UINT32_C(0x00002000)
#define T64_COP1_CAUSE_I UINT32_C(0x00001000)
#define T64_COP1_ENABLE_MASK UINT32_C(0x00000f80)
#define T64_COP1_FLAG_MASK UINT32_C(0x0000007c)
#define T64_COP1_FLAG_V UINT32_C(0x00000040)
#define T64_COP1_FLAG_Z UINT32_C(0x00000020)
#define T64_COP1_FLAG_O UINT32_C(0x00000010)
#define T64_COP1_FLAG_U UINT32_C(0x00000008)
#define T64_COP1_FLAG_I UINT32_C(0x00000004)
#define T64_COP1_FS UINT32_C(0x01000000)
#define T64_COP1_CONDITION UINT32_C(0x00800000)
#define T64_COP1_CONTROL_MASK                                                \
  (T64_COP1_CAUSE_MASK | UINT32_C(0x00000f80) | UINT32_C(0x0000007c) |     \
   UINT32_C(0x01000000) | UINT32_C(0x00800000) | UINT32_C(0x00000003))

static uint64_t cop1_sign_extend32(uint32_t bits) {
  return (bits & UINT32_C(0x80000000)) != 0u
             ? UINT64_C(0xffffffff00000000) | (uint64_t)bits
             : (uint64_t)bits;
}

bool t64_cop1_is_branch(uint32_t opcode) {
  return (opcode >> 26) == UINT32_C(0x11) &&
         ((opcode >> 21) & UINT32_C(0x1f)) == UINT32_C(0x08);
}

bool t64_cop1_branch_encoding_valid(uint32_t opcode) {
  return t64_cop1_is_branch(opcode) &&
         ((opcode >> 16) & UINT32_C(0x1f)) <= UINT32_C(0x03);
}

bool t64_cop1_branch_condition(const t64_cpu_state *cpu, uint32_t opcode,
                               bool *likely_out) {
  uint32_t suboperation;
  bool condition;

  if (cpu == NULL || likely_out == NULL ||
      !t64_cop1_branch_encoding_valid(opcode)) {
    return false;
  }
  suboperation = (opcode >> 16) & UINT32_C(0x1f);
  condition = (cpu->fcr31 & UINT32_C(0x00800000)) != 0u;
  *likely_out = (suboperation & UINT32_C(0x02)) != 0u;
  return condition == ((suboperation & UINT32_C(0x01)) != 0u);
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
                          uint32_t width, bool compat_fr0_odd_alias,
                          uint64_t *bits_out) {
  if (machine == NULL || bits_out == NULL || reg >= 32u ||
      (width != 32u && width != 64u)) {
    return false;
  }
  if (compat_fr0_odd_alias && !cop1_fr(machine)) {
    reg &= ~UINT32_C(1);
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
                      ? t64_cpu_delay_branch_epc(machine)
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

static uint64_t cop1_float_exponent_mask(uint32_t format) {
  return format == UINT32_C(16) ? UINT64_C(0x7f800000)
                                : UINT64_C(0x7ff0000000000000);
}

static uint64_t cop1_float_fraction_mask(uint32_t format) {
  return format == UINT32_C(16) ? UINT64_C(0x007fffff)
                                : UINT64_C(0x000fffffffffffff);
}

static uint64_t cop1_float_sign_mask(uint32_t format) {
  return format == UINT32_C(16) ? UINT64_C(0x80000000)
                                : UINT64_C(0x8000000000000000);
}

static uint64_t cop1_float_signaling_bit(uint32_t format) {
  /* The NEC manual's VR4300 NaN polarity is the inverse of SoftFloat's. */
  return format == UINT32_C(16) ? UINT64_C(0x00400000)
                                : UINT64_C(0x0008000000000000);
}

static bool cop1_float_is_nan(uint32_t format, uint64_t bits) {
  return (bits & cop1_float_exponent_mask(format)) ==
             cop1_float_exponent_mask(format) &&
         (bits & cop1_float_fraction_mask(format)) != 0u;
}

static bool cop1_float_is_signaling_nan(uint32_t format, uint64_t bits) {
  return cop1_float_is_nan(format, bits) &&
         (bits & cop1_float_signaling_bit(format)) != 0u;
}

static bool cop1_float_is_quiet_nan(uint32_t format, uint64_t bits) {
  return cop1_float_is_nan(format, bits) &&
         (bits & cop1_float_signaling_bit(format)) == 0u;
}

static bool cop1_float_is_infinite(uint32_t format, uint64_t bits) {
  return (bits & cop1_float_exponent_mask(format)) ==
             cop1_float_exponent_mask(format) &&
         (bits & cop1_float_fraction_mask(format)) == 0u;
}

static bool cop1_float_is_subnormal(uint32_t format, uint64_t bits) {
  return (bits & cop1_float_exponent_mask(format)) == 0u &&
         (bits & cop1_float_fraction_mask(format)) != 0u;
}

static uint8_t cop1_softfloat_rounding(uint32_t fcr31_rounding) {
  if (fcr31_rounding == UINT32_C(2)) return 3u; /* RP -> round maximum */
  if (fcr31_rounding == UINT32_C(3)) return 2u; /* RM -> round minimum */
  return (uint8_t)fcr31_rounding;
}

static uint64_t cop1_canonical_quiet_nan(uint32_t format) {
  /* A deterministic positive payload with the VR4300 quiet bit clear. */
  return format == UINT32_C(16) ? UINT64_C(0x7f800001)
                                : UINT64_C(0x7ff0000000000001);
}

static uint32_t cop1_raw_flags_to_causes(uint8_t flags) {
  uint32_t causes = 0u;

  if ((flags & T64_SOFTFLOAT_FLAG_INVALID) != 0u) {
    causes |= T64_COP1_CAUSE_V;
  }
  if ((flags & T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO) != 0u) {
    causes |= T64_COP1_CAUSE_Z;
  }
  if ((flags & T64_SOFTFLOAT_FLAG_OVERFLOW) != 0u) {
    causes |= T64_COP1_CAUSE_O;
  }
  if ((flags & T64_SOFTFLOAT_FLAG_UNDERFLOW) != 0u) {
    causes |= T64_COP1_CAUSE_U;
  }
  if ((flags & T64_SOFTFLOAT_FLAG_INEXACT) != 0u) {
    causes |= T64_COP1_CAUSE_I;
  }
  return causes;
}

static uint32_t cop1_flags_for_disabled_causes(uint32_t causes,
                                                uint32_t fcr31) {
  uint32_t enabled_causes =
      (fcr31 & T64_COP1_ENABLE_MASK) << UINT32_C(5);
  return ((causes & ~enabled_causes) >> UINT32_C(10)) &
         T64_COP1_FLAG_MASK;
}

static t64_status cop1_raise_unimplemented_result(
    t64_machine *machine, bool in_delay_slot,
    t64_cpu_cop1_result *result_out) {
  result_out->supported = true;
  result_out->trapped = true;
  return cop1_raise_unimplemented(machine, in_delay_slot);
}

static t64_status cop1_raise_ieee(t64_machine *machine, bool in_delay_slot,
                                  uint32_t causes,
                                  t64_cpu_cop1_result *result_out) {
  uint32_t flags = cop1_flags_for_disabled_causes(causes,
                                                   machine->cpu.fcr31);
  uint32_t fcr31 = machine->cpu.fcr31;
  uint32_t enabled_causes = (fcr31 & T64_COP1_ENABLE_MASK) << UINT32_C(5);

  result_out->supported = true;
  result_out->trapped = true;
  machine->cpu.fcr31 = (fcr31 & ~T64_COP1_CAUSE_MASK) | causes | flags;
  if ((causes & enabled_causes) == 0u) {
    return T64_STATUS_OK;
  }
  return cop1_enter_exception(machine, UINT32_C(15), in_delay_slot);
}

static uint64_t cop1_flush_subnormal(uint32_t format, uint64_t bits,
                                     uint32_t rounding_mode) {
  bool negative = (bits & cop1_float_sign_mask(format)) != 0u;
  uint64_t sign = negative ? cop1_float_sign_mask(format) : 0u;
  bool directed_outward = (!negative && rounding_mode == UINT32_C(2)) ||
                          (negative && rounding_mode == UINT32_C(3));
  uint64_t minimum_normal = format == UINT32_C(16)
                                ? UINT64_C(0x00800000)
                                : UINT64_C(0x0010000000000000);

  return sign | (directed_outward ? minimum_normal : 0u);
}

static bool cop1_compare_numeric(uint32_t format, uint64_t left,
                                 uint64_t right, bool *less_out,
                                 bool *equal_out) {
  uint64_t sign_mask = cop1_float_sign_mask(format);
  uint64_t left_magnitude = left & ~sign_mask;
  uint64_t right_magnitude = right & ~sign_mask;
  bool left_negative = (left & sign_mask) != 0u;
  bool right_negative = (right & sign_mask) != 0u;

  if (less_out == NULL || equal_out == NULL ||
      cop1_float_is_nan(format, left) || cop1_float_is_nan(format, right)) {
    return false;
  }
  if (left_magnitude == 0u && right_magnitude == 0u) {
    *less_out = false;
    *equal_out = true;
    return true;
  }
  if (left_negative != right_negative) {
    *less_out = left_negative;
  } else if (left_negative) {
    *less_out = left_magnitude > right_magnitude;
  } else {
    *less_out = left_magnitude < right_magnitude;
  }
  *equal_out = left == right;
  return true;
}

static t64_status cop1_finish_ieee_result(
    t64_machine *machine, bool in_delay_slot, uint32_t format,
    bool floating_result, t64_softfloat_result *raw_result,
    t64_cpu_cop1_result *result_out) {
  uint32_t causes = cop1_raw_flags_to_causes(raw_result->exception_flags);
  uint32_t rounding_mode = machine->cpu.fcr31 & UINT32_C(3);
  uint32_t enabled_underflow = UINT32_C(0x00000180);
  bool tiny = floating_result &&
              ((causes & T64_COP1_CAUSE_U) != 0u ||
               cop1_float_is_subnormal(format, raw_result->bits));

  if (tiny) {
    if ((machine->cpu.fcr31 & T64_COP1_FS) == 0u ||
        (machine->cpu.fcr31 & enabled_underflow) != 0u) {
      return cop1_raise_unimplemented_result(machine, in_delay_slot,
                                              result_out);
    }
    raw_result->bits = cop1_flush_subnormal(format, raw_result->bits,
                                            rounding_mode);
    causes |= T64_COP1_CAUSE_U | T64_COP1_CAUSE_I;
  }
  if (floating_result && (causes & T64_COP1_CAUSE_V) != 0u) {
    raw_result->bits = cop1_canonical_quiet_nan(format);
  }
  result_out->supported = true;
  result_out->clear_fcr31_causes = true;
  result_out->fcr31_causes = causes;
  result_out->fcr31_flags =
      cop1_flags_for_disabled_causes(causes, machine->cpu.fcr31);
  if ((causes & ((machine->cpu.fcr31 & T64_COP1_ENABLE_MASK) << 5)) != 0u) {
    return cop1_raise_ieee(machine, in_delay_slot, causes, result_out);
  }
  result_out->result_bits = raw_result->bits;
  return T64_STATUS_OK;
}

static t64_status cop1_execute_compare(
    t64_machine *machine, uint32_t format, uint32_t function,
    uint32_t left_register, uint32_t right_register, bool in_delay_slot,
    t64_cpu_cop1_result *result_out) {
  uint64_t left;
  uint64_t right;
  bool unordered;
  bool less = false;
  bool equal = false;
  bool invalid;
  bool condition;
  t64_status status;

  if (!cop1_fpr_read(machine, left_register, format == UINT32_C(16) ? 32u
                                                                    : 64u,
                     false, &left) ||
      !cop1_fpr_read(machine, right_register, format == UINT32_C(16) ? 32u
                                                                      : 64u,
                     false, &right)) {
    return cop1_raise_unimplemented_result(machine, in_delay_slot,
                                            result_out);
  }
  unordered = cop1_float_is_nan(format, left) ||
              cop1_float_is_nan(format, right);
  if (!unordered && !cop1_compare_numeric(format, left, right, &less, &equal)) {
    return cop1_raise_unimplemented_result(machine, in_delay_slot,
                                            result_out);
  }
  invalid = cop1_float_is_signaling_nan(format, left) ||
            cop1_float_is_signaling_nan(format, right) ||
            (unordered && (function & UINT32_C(8)) != 0u);
  condition = (((function & UINT32_C(4)) != 0u) && less) ||
              (((function & UINT32_C(2)) != 0u) && equal) ||
              (((function & UINT32_C(1)) != 0u) && unordered);
  status = cop1_finish_ieee_result(
      machine, in_delay_slot, format, false,
      &(t64_softfloat_result){condition ? 1u : 0u,
                              invalid ? T64_SOFTFLOAT_FLAG_INVALID : 0u},
      result_out);
  if (status != T64_STATUS_OK || result_out->trapped) {
    return status;
  }
  result_out->write_fcr31_condition = true;
  result_out->fcr31_condition = condition;
  return T64_STATUS_OK;
}

static bool cop1_floating_registers_valid(const t64_machine *machine,
                                          uint32_t source_format,
                                          uint32_t destination_width,
                                          uint32_t fs, uint32_t fd,
                                          bool compat_fr0_odd_alias) {
  uint64_t ignored;

  if (source_format == UINT32_C(17) || source_format == UINT32_C(21)) {
    if (!cop1_fpr_read(machine, fs, 64u, compat_fr0_odd_alias, &ignored)) {
      return false;
    }
  }
  if (destination_width == 64u && !cop1_fr(machine) &&
      (((fd & 1u) != 0u) || fd >= 31u) && !compat_fr0_odd_alias) {
    return false;
  }
  return true;
}

static t64_status cop1_execute_floating(
    t64_machine *machine, uint32_t format, uint32_t rt, uint32_t fs,
    uint32_t fd, uint32_t function, bool in_delay_slot,
    t64_cpu_cop1_result *result_out) {
  t64_softfloat_context context;
  t64_softfloat_result raw_result = {0u, 0u};
  t64_softfloat_operation operation = (t64_softfloat_operation)0;
  uint32_t source_width = format == UINT32_C(17) ||
                                  format == UINT32_C(21)
                              ? 64u
                              : 32u;
  uint32_t destination_width = 0u;
  uint64_t source_a = 0u;
  uint64_t source_b = 0u;
  bool floating_result = false;
  bool comparison = function >= UINT32_C(0x30) &&
                     function <= UINT32_C(0x3f);
  bool unary = function == UINT32_C(0x04) || function == UINT32_C(0x05) ||
               function == UINT32_C(0x06) || function == UINT32_C(0x07);
  bool arithmetic = function <= UINT32_C(0x07);
  bool conversion = function == UINT32_C(0x20) ||
                    function == UINT32_C(0x21) ||
                    function == UINT32_C(0x24) ||
                    function == UINT32_C(0x25) ||
                    (function >= UINT32_C(0x08) &&
                     function <= UINT32_C(0x0f));
  /* The manual leaves odd FR=0 double operands undefined; compat-v2 follows
   * the pinned software's even-pair alias for covered double ops/conversions. */
  bool compat_fr0_odd_alias =
      t64_has_cpu_compat_extension_policy(machine) && !cop1_fr(machine) &&
      !comparison &&
      (format == UINT32_C(17) || conversion);
  uint32_t source_format = format;
  bool explicit_rounding = function >= UINT32_C(0x08) &&
                           function <= UINT32_C(0x0f);
  uint32_t raw_rounding = cop1_softfloat_rounding(
      machine->cpu.fcr31 & UINT32_C(3));
  t64_status status;

  result_out->supported = true;
  if (format != UINT32_C(16) && format != UINT32_C(17) &&
      format != UINT32_C(20) && format != UINT32_C(21)) {
    return cop1_raise_unimplemented_result(machine, in_delay_slot,
                                            result_out);
  }
  if ((format == UINT32_C(20) || format == UINT32_C(21)) &&
      function != UINT32_C(0x20) && function != UINT32_C(0x21)) {
    return cop1_raise_unimplemented_result(machine, in_delay_slot,
                                            result_out);
  }
  if (comparison) {
    if (format != UINT32_C(16) && format != UINT32_C(17)) {
      return cop1_raise_unimplemented(machine, in_delay_slot);
    }
    return cop1_execute_compare(machine, format, function, fs, rt,
                                in_delay_slot, result_out);
  }
  if ((unary && rt != 0u) || (conversion && rt != 0u) ||
      (!unary && !conversion && !arithmetic)) {
    return cop1_raise_unimplemented(machine, in_delay_slot);
  }
  if (function == UINT32_C(0x06)) { /* MOV is a bit transfer. */
    if ((format != UINT32_C(16) && format != UINT32_C(17)) || rt != 0u ||
        !cop1_fpr_read(machine, fs, source_width, compat_fr0_odd_alias,
                       &source_a)) {
      return cop1_raise_unimplemented_result(machine, in_delay_slot,
                                              result_out);
    }
    result_out->write_fpr = true;
    result_out->destination_fpr =
        compat_fr0_odd_alias && source_width == 64u
            ? fd & ~UINT32_C(1)
            : fd;
    result_out->fpr_width = source_width;
    result_out->result_bits = source_a;
    return T64_STATUS_OK;
  }
  if (arithmetic && format != UINT32_C(16) && format != UINT32_C(17)) {
    return cop1_raise_unimplemented_result(machine, in_delay_slot,
                                            result_out);
  }
  if (function == UINT32_C(0x05) || function == UINT32_C(0x07) ||
      function == UINT32_C(0x04)) {
    destination_width = source_width;
    floating_result = true;
  } else if (arithmetic) {
    destination_width = source_width;
    floating_result = true;
  } else if (function == UINT32_C(0x20)) {
    destination_width = 32u;
    floating_result = true;
    if (format == UINT32_C(16)) return cop1_raise_unimplemented_result(
        machine, in_delay_slot, result_out);
  } else if (function == UINT32_C(0x21)) {
    destination_width = 64u;
    floating_result = true;
    if (format == UINT32_C(17)) return cop1_raise_unimplemented_result(
        machine, in_delay_slot, result_out);
  } else if (function == UINT32_C(0x24) ||
             (function >= UINT32_C(0x0c) &&
              function <= UINT32_C(0x0f))) {
    destination_width = 32u;
  } else if (function == UINT32_C(0x25) ||
             (function >= UINT32_C(0x08) &&
              function <= UINT32_C(0x0b))) {
    destination_width = 64u;
  }
  if (destination_width == 0u ||
      !cop1_floating_registers_valid(machine, source_format,
                                     destination_width, fs, fd,
                                     compat_fr0_odd_alias)) {
    return cop1_raise_unimplemented_result(machine, in_delay_slot,
                                            result_out);
  }

  if (format == UINT32_C(16) || format == UINT32_C(17)) {
    if (!cop1_fpr_read(machine, fs, source_width, compat_fr0_odd_alias,
                       &source_a)) {
      return cop1_raise_unimplemented_result(machine, in_delay_slot,
                                              result_out);
    }
    if (arithmetic && !unary) {
      if (!cop1_fpr_read(machine, rt, source_width, compat_fr0_odd_alias,
                         &source_b)) {
        return cop1_raise_unimplemented_result(machine, in_delay_slot,
                                                result_out);
      }
    }
  } else if (!cop1_fpr_read(machine, fs, source_width,
                            compat_fr0_odd_alias, &source_a)) {
    return cop1_raise_unimplemented_result(machine, in_delay_slot,
                                            result_out);
  }

  if ((function == UINT32_C(0x05) || function == UINT32_C(0x07) ||
       function == UINT32_C(0x04) || arithmetic || conversion) &&
      format <= UINT32_C(17)) {
    if (cop1_float_is_quiet_nan(format, source_a) ||
        (arithmetic && !unary &&
         cop1_float_is_quiet_nan(format, source_b)) ||
        (format <= UINT32_C(17) &&
         (cop1_float_is_subnormal(format, source_a) ||
          (arithmetic && !unary &&
           cop1_float_is_subnormal(format, source_b))))) {
      return cop1_raise_unimplemented_result(machine, in_delay_slot,
                                              result_out);
    }
    if (function == UINT32_C(0x24) || function == UINT32_C(0x25) ||
        (function >= UINT32_C(0x08) && function <= UINT32_C(0x0f))) {
      if (cop1_float_is_nan(format, source_a) ||
          cop1_float_is_infinite(format, source_a)) {
        return cop1_raise_unimplemented_result(machine, in_delay_slot,
                                                result_out);
      }
    }
  }

  if (function == UINT32_C(0x05)) { /* ABS is arithmetic for S-NaN. */
    raw_result.bits = source_a & ~cop1_float_sign_mask(format);
    raw_result.exception_flags =
        cop1_float_is_signaling_nan(format, source_a)
            ? T64_SOFTFLOAT_FLAG_INVALID
            : 0u;
  } else if (function == UINT32_C(0x07)) { /* NEG */
    raw_result.bits = source_a ^ cop1_float_sign_mask(format);
    raw_result.exception_flags =
        cop1_float_is_signaling_nan(format, source_a)
            ? T64_SOFTFLOAT_FLAG_INVALID
            : 0u;
  } else {
    context = (t64_softfloat_context){(uint8_t)raw_rounding,
                                      T64_SOFTFLOAT_TININESS_AFTER_ROUNDING,
                                      0u};
    if (format == UINT32_C(16)) {
      switch (function) {
      case 0x00: operation = T64_SOFTFLOAT_OP_F32_ADD; break;
      case 0x01: operation = T64_SOFTFLOAT_OP_F32_SUB; break;
      case 0x02: operation = T64_SOFTFLOAT_OP_F32_MUL; break;
      case 0x03: operation = T64_SOFTFLOAT_OP_F32_DIV; break;
      case 0x04: operation = T64_SOFTFLOAT_OP_F32_SQRT; break;
      case 0x20:
        if (format == UINT32_C(17)) operation = T64_SOFTFLOAT_OP_F64_TO_F32;
        else if (format == UINT32_C(20)) operation = T64_SOFTFLOAT_OP_I32_TO_F32;
        else if (format == UINT32_C(21)) operation = T64_SOFTFLOAT_OP_I64_TO_F32;
        break;
      case 0x21:
        if (format == UINT32_C(16)) operation = T64_SOFTFLOAT_OP_F32_TO_F64;
        else if (format == UINT32_C(20)) operation = T64_SOFTFLOAT_OP_I32_TO_F64;
        else if (format == UINT32_C(21)) operation = T64_SOFTFLOAT_OP_I64_TO_F64;
        break;
      case 0x24:
      case 0x0c: case 0x0d: case 0x0e: case 0x0f:
        operation = T64_SOFTFLOAT_OP_F32_TO_I32;
        break;
      case 0x25:
      case 0x08: case 0x09: case 0x0a: case 0x0b:
        operation = T64_SOFTFLOAT_OP_F32_TO_I64;
        break;
      default: break;
      }
    } else {
      switch (function) {
      case 0x00: operation = T64_SOFTFLOAT_OP_F64_ADD; break;
      case 0x01: operation = T64_SOFTFLOAT_OP_F64_SUB; break;
      case 0x02: operation = T64_SOFTFLOAT_OP_F64_MUL; break;
      case 0x03: operation = T64_SOFTFLOAT_OP_F64_DIV; break;
      case 0x04: operation = T64_SOFTFLOAT_OP_F64_SQRT; break;
      case 0x20: operation = T64_SOFTFLOAT_OP_F64_TO_F32; break;
      case 0x21: operation = T64_SOFTFLOAT_OP_F32_TO_F64; break;
      case 0x24: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
        operation = T64_SOFTFLOAT_OP_F64_TO_I32;
        break;
      case 0x25: case 0x08: case 0x09: case 0x0a: case 0x0b:
        operation = T64_SOFTFLOAT_OP_F64_TO_I64;
        break;
      default: break;
      }
    }
    if (function == UINT32_C(0x20) || function == UINT32_C(0x21)) {
      if (format == UINT32_C(20)) {
        operation = function == UINT32_C(0x20)
                        ? T64_SOFTFLOAT_OP_I32_TO_F32
                        : T64_SOFTFLOAT_OP_I32_TO_F64;
      } else if (format == UINT32_C(21)) {
        operation = function == UINT32_C(0x20)
                        ? T64_SOFTFLOAT_OP_I64_TO_F32
                        : T64_SOFTFLOAT_OP_I64_TO_F64;
      }
      if (format == UINT32_C(17) && function == UINT32_C(0x20)) {
        operation = T64_SOFTFLOAT_OP_F64_TO_F32;
      }
      if (format == UINT32_C(16) && function == UINT32_C(0x21)) {
        operation = T64_SOFTFLOAT_OP_F32_TO_F64;
      }
    }
    if (operation == (t64_softfloat_operation)0) {
      return cop1_raise_unimplemented_result(machine, in_delay_slot,
                                              result_out);
    }
    if (explicit_rounding) {
      raw_rounding = function == UINT32_C(0x08) ||
                             function == UINT32_C(0x0c)
                         ? 0u
                         : function == UINT32_C(0x09) ||
                                   function == UINT32_C(0x0d)
                               ? 1u
                               : function == UINT32_C(0x0a) ||
                                         function == UINT32_C(0x0e)
                                     ? 3u
                                     : 2u;
    }
    context.rounding_mode = (uint8_t)raw_rounding;
    status = t64_softfloat_execute(operation, &context, source_a, source_b,
                                   &raw_result);
    if (status != T64_STATUS_OK) {
      return status;
    }
    if (!floating_result &&
        (raw_result.exception_flags & T64_SOFTFLOAT_FLAG_INVALID) != 0u) {
      return cop1_raise_unimplemented_result(machine, in_delay_slot,
                                              result_out);
    }
    if (format <= UINT32_C(17) &&
        cop1_float_is_signaling_nan(format, source_a) &&
        function != UINT32_C(0x06)) {
      raw_result.exception_flags |= T64_SOFTFLOAT_FLAG_INVALID;
      if (floating_result) {
        raw_result.bits = cop1_canonical_quiet_nan(
            destination_width == 32u ? UINT32_C(16) : UINT32_C(17));
      }
    }
    if (format <= UINT32_C(17) && arithmetic && !unary &&
        cop1_float_is_signaling_nan(format, source_b)) {
      raw_result.exception_flags |= T64_SOFTFLOAT_FLAG_INVALID;
      raw_result.bits = cop1_canonical_quiet_nan(
          destination_width == 32u ? UINT32_C(16) : UINT32_C(17));
    }
  }
  status = cop1_finish_ieee_result(machine, in_delay_slot,
                                   floating_result && destination_width == 32u
                                       ? UINT32_C(16)
                                       : UINT32_C(17),
                                   floating_result, &raw_result, result_out);
  if (status != T64_STATUS_OK || result_out->trapped) {
    return status;
  }
  result_out->write_fpr = true;
  result_out->destination_fpr =
      compat_fr0_odd_alias && destination_width == 64u
          ? fd & ~UINT32_C(1)
          : fd;
  result_out->fpr_width = destination_width;
  result_out->result_bits = raw_result.bits;
  return T64_STATUS_OK;
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
  if (!load && !cop1_fpr_read(machine, reg, width, false, &store_value)) {
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
      if (!cop1_fpr_read(machine, fs, 64u, false, &fpr_value)) {
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

  return cop1_execute_floating(machine, format, rt, fs, fd, function,
                               in_delay_slot, result_out);
}
