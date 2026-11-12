#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>

#define T64_CP0_BAD_VADDR UINT32_C(8)
#define T64_CP0_STATUS UINT32_C(12)
#define T64_CP0_CAUSE UINT32_C(13)
#define T64_CP0_EPC UINT32_C(14)
#define T64_CP0_ENTRYHI UINT32_C(10)
#define T64_CP0_CONTEXT UINT32_C(4)
#define T64_CP0_XCONTEXT UINT32_C(20)

#define T64_STATUS_EXL UINT32_C(0x00000002)
#define T64_STATUS_BEV UINT32_C(0x00400000)
#define T64_CAUSE_BD UINT32_C(0x80000000)
#define T64_CAUSE_CE UINT32_C(0x30000000)
#define T64_CAUSE_EXCCODE UINT32_C(0x0000007c)
#define T64_ENTRYHI_ASID_MASK UINT64_C(0x00000000000000ff)
#define T64_ENTRYHI32_VPN_MASK UINT64_C(0x00000000ffffe000)
#define T64_ENTRYHI64_VPN_MASK UINT64_C(0xc00000ffffffe000)
#define T64_CONTEXT_BADVPN2_MASK UINT64_C(0x00000000007ffff0)
#define T64_XCONTEXT_BADVPN2_MASK UINT64_C(0x000000007ffffff0)
#define T64_XCONTEXT_REGION_MASK UINT64_C(0x0000000180000000)

static bool exception_updates_bad_vaddr(uint32_t code) {
  return code == UINT32_C(1) || code == UINT32_C(2) || code == UINT32_C(3) ||
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

static uint64_t tlb_refill_vector(uint32_t status, bool extended) {
  uint64_t base = (status & T64_STATUS_BEV) != 0u
                      ? UINT64_C(0xffffffffbfc00200)
                      : UINT64_C(0xffffffff80000000);

  return base + (extended ? UINT64_C(0x80) : UINT64_C(0));
}

static void update_tlb_fault_context(t64_machine *machine,
                                     const t64_cpu_exception_result *exception) {
  uint64_t address = exception->bad_virtual_address;
  uint64_t old_entry_hi = machine->cpu.cp0[T64_CP0_ENTRYHI];
  uint64_t entry_hi_mask = exception->tlb_extended
                               ? T64_ENTRYHI64_VPN_MASK
                               : T64_ENTRYHI32_VPN_MASK;
  uint64_t entry_hi = (old_entry_hi & T64_ENTRYHI_ASID_MASK) |
                      (address & entry_hi_mask);
  uint64_t context = machine->cpu.cp0[T64_CP0_CONTEXT];
  uint64_t xcontext = machine->cpu.cp0[T64_CP0_XCONTEXT];
  uint64_t xcontext_fields = T64_XCONTEXT_BADVPN2_MASK |
                             T64_XCONTEXT_REGION_MASK;
  uint64_t xcontext_bad_vpn2 = (address >> 9) &
                               T64_XCONTEXT_BADVPN2_MASK;
  uint64_t xcontext_region = (address >> 31) &
                             T64_XCONTEXT_REGION_MASK;

  machine->cpu.cp0[T64_CP0_ENTRYHI] = entry_hi;
  machine->cpu.cp0[T64_CP0_CONTEXT] =
      (context & ~T64_CONTEXT_BADVPN2_MASK) |
      ((address >> 9) & T64_CONTEXT_BADVPN2_MASK);
  machine->cpu.cp0[T64_CP0_XCONTEXT] =
      (xcontext & ~xcontext_fields) | xcontext_bad_vpn2 |
      xcontext_region;
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
  if (exception->sets_tlb_context) {
    update_tlb_fault_context(machine, exception);
  }
  machine->cpu.cp0[T64_CP0_STATUS] = old_status | T64_STATUS_EXL;

  vector = exception->tlb_refill && (old_status & T64_STATUS_EXL) == 0u
               ? tlb_refill_vector(old_status, exception->tlb_extended)
               : general_exception_vector(old_status);
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
