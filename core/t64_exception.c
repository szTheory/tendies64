#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>

#define T64_CP0_BAD_VADDR UINT32_C(8)
#define T64_CP0_STATUS UINT32_C(12)
#define T64_CP0_CAUSE UINT32_C(13)
#define T64_CP0_EPC UINT32_C(14)

#define T64_STATUS_EXL UINT32_C(0x00000002)
#define T64_STATUS_BEV UINT32_C(0x00400000)
#define T64_CAUSE_BD UINT32_C(0x80000000)
#define T64_CAUSE_CE UINT32_C(0x30000000)
#define T64_CAUSE_EXCCODE UINT32_C(0x0000007c)

static bool exception_updates_bad_vaddr(uint32_t code) {
  return code == UINT32_C(2) || code == UINT32_C(3) ||
         code == UINT32_C(4) || code == UINT32_C(5);
}

static uint64_t general_exception_vector(uint32_t status) {
  if ((status & T64_STATUS_BEV) != 0u) {
    /* The table on p.181 selects BFC0_0380 in 64-bit BEV mode. The adjacent
     * prose gives a conflicting 32-bit address; the profile records the table
     * choice as policy, not as a resolved hardware fact. */
    return UINT64_C(0xffffffffbfc00380);
  }
  return UINT64_C(0xffffffff80000180);
}

t64_status t64_cpu_exception_enter(
    t64_machine *machine, const t64_cpu_exception_result *exception) {
  t64_event_reservation reservation;
  t64_status status;
  uint32_t old_status;
  uint32_t cause;
  uint64_t vector;

  if (machine == NULL || exception == NULL || !exception->valid ||
      exception->code > UINT32_C(31) ||
      (exception->sets_coprocessor && exception->coprocessor > UINT32_C(3))) {
    return T64_STATUS_INVALID_ARGUMENT;
  }

  old_status = (uint32_t)machine->cpu.cp0[T64_CP0_STATUS];
  status = t64_schedule_reserve_after(
      machine, UINT64_C(1), T64_DEVICE_CPU, T64_CPU_EVENT_STEP, UINT64_C(0),
      UINT64_C(0), t64_cpu_step, &reservation);
  if (status != T64_STATUS_OK) {
    machine->cpu.attempt_outcome = T64_CPU_ATTEMPT_HOST_ERROR;
    return status;
  }
  status = t64_schedule_commit(machine, &reservation);
  if (status != T64_STATUS_OK) {
    machine->cpu.attempt_outcome = T64_CPU_ATTEMPT_HOST_ERROR;
    return status;
  }

  cause = (uint32_t)machine->cpu.cp0[T64_CP0_CAUSE];
  cause = (cause & ~T64_CAUSE_EXCCODE) |
          ((exception->code << 2) & T64_CAUSE_EXCCODE);
  if ((old_status & T64_STATUS_EXL) == 0u) {
    cause &= ~T64_CAUSE_BD;
    if (exception->branch_delay) {
      cause |= T64_CAUSE_BD;
    }
    machine->cpu.cp0[T64_CP0_EPC] = exception->epc;
  }
  if (exception->sets_coprocessor) {
    cause = (cause & ~T64_CAUSE_CE) |
            ((exception->coprocessor << 28) & T64_CAUSE_CE);
  }
  machine->cpu.cp0[T64_CP0_CAUSE] = cause;
  if (exception_updates_bad_vaddr(exception->code)) {
    machine->cpu.cp0[T64_CP0_BAD_VADDR] = exception->bad_virtual_address;
  }
  machine->cpu.cp0[T64_CP0_STATUS] = old_status | T64_STATUS_EXL;

  vector = general_exception_vector(old_status);
  machine->cpu.pc_full = vector;
  machine->cpu.pc = (uint32_t)vector;
  machine->cpu.delay_slot_pending = false;
  machine->cpu.delay_slot_pc = 0u;
  machine->cpu.delay_target = 0u;
  machine->cpu.delay_slot_pc_full = 0u;
  machine->cpu.delay_target_full = 0u;
  machine->cpu.pending_exception_valid = true;
  machine->cpu.pending_exception_code = exception->code;
  machine->cpu.pending_exception_address = exception->bad_virtual_address;
  machine->cpu.pending_exception_epc = exception->epc;
  machine->cpu.pending_exception_bd = exception->branch_delay;
  machine->cpu.attempt_outcome = T64_CPU_ATTEMPT_GUEST_EXCEPTION;
  return T64_STATUS_OK;
}
