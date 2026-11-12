#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define T64_COP0_OPCODE UINT32_C(0x10)
#define T64_CP0_RANDOM UINT32_C(1)
#define T64_CP0_EPC UINT32_C(14)
#define T64_CP0_STATUS UINT32_C(12)
#define T64_STATUS_WRITABLE_MASK UINT32_C(0xf757ffff)
#define T64_STATUS_TS UINT32_C(0x00200000)

static uint64_t sign_extend_word(uint32_t value) {
  return t64_cpu_canonical_address(value);
}

static bool is_vr4300_profile(const t64_machine *machine) {
  return machine != NULL &&
         machine->profile.machine_model_version == UINT32_C(3) &&
         machine->profile.state_serialization_version == UINT32_C(9);
}

t64_cp0_transfer_result t64_cp0_prepare_transfer(
    const t64_machine *machine, uint32_t opcode, t64_cp0_effect *effect_out) {
  uint32_t operation;
  uint32_t rt;
  uint32_t rd;
  uint32_t select;
  uint32_t old_status;

  if (machine == NULL || effect_out == NULL || !is_vr4300_profile(machine) ||
      (opcode >> 26) != T64_COP0_OPCODE) {
    return T64_CP0_TRANSFER_UNSUPPORTED;
  }
  (void)memset(effect_out, 0, sizeof *effect_out);
  operation = (opcode >> 21) & UINT32_C(0x1f);
  rt = (opcode >> 16) & UINT32_C(0x1f);
  rd = (opcode >> 11) & UINT32_C(0x1f);
  select = opcode & UINT32_C(7);

  if ((opcode & UINT32_C(0x7f8)) != 0u || select != 0u ||
      (operation != 0u && operation != 1u && operation != 4u &&
       operation != 5u)) {
    return T64_CP0_TRANSFER_RESERVED;
  }
  if (operation == 0u || operation == 1u) {
    uint64_t value;

    if ((operation == 0u && rd != T64_CP0_STATUS) ||
        (operation == 1u && rd != T64_CP0_EPC)) {
      return T64_CP0_TRANSFER_UNSUPPORTED;
    }
    value = machine->cpu.cp0[rd];

    effect_out->write_gpr = true;
    effect_out->gpr_index = rt;
    effect_out->gpr_value = operation == 0u
                                ? sign_extend_word((uint32_t)value)
                                : value;
    return T64_CP0_TRANSFER_OK;
  }

  if (rd != T64_CP0_STATUS) {
    return T64_CP0_TRANSFER_UNSUPPORTED;
  }

  old_status = (uint32_t)machine->cpu.cp0[T64_CP0_STATUS];
  effect_out->write_cp0 = true;
  effect_out->cp0_index = rd;
  effect_out->cp0_value =
      ((uint32_t)machine->cpu.gpr[rt] & T64_STATUS_WRITABLE_MASK) |
      (old_status & T64_STATUS_TS);
  return T64_CP0_TRANSFER_OK;
}

void t64_cp0_commit_transfer(t64_machine *machine,
                             const t64_cp0_effect *effect) {
  if (machine == NULL || effect == NULL) {
    return;
  }
  if (effect->write_cp0 && effect->cp0_index < 32u) {
    machine->cpu.cp0[effect->cp0_index] = effect->cp0_value;
  }
  if (effect->clear_timer_pending) {
    machine->cpu.cp0[13] =
        (uint32_t)machine->cpu.cp0[13] & ~UINT32_C(0x00008000);
  }
  if (effect->reset_random) {
    machine->cpu.cp0[T64_CP0_RANDOM] = UINT32_C(31);
  }
}
