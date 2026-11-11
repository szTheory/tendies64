#include "t64_internal.h"

#include <stdint.h>

#define T64_MI_INIT_MODE UINT32_C(0x04300000)
#define T64_MI_VERSION UINT32_C(0x04300004)
#define T64_MI_INTR UINT32_C(0x04300008)
#define T64_RI_SELECT UINT32_C(0x0470000c)
#define T64_SP_STATUS UINT32_C(0x04040010)
#define T64_SP_DMA_FULL UINT32_C(0x04040014)
#define T64_SP_DMA_BUSY UINT32_C(0x04040018)
#define T64_SP_SEMAPHORE UINT32_C(0x0404001c)
#define T64_SP_PC UINT32_C(0x04080000)
#define T64_AI_CONTROL UINT32_C(0x04500008)
#define T64_AI_STATUS UINT32_C(0x0450000c)
#define T64_SI_STATUS UINT32_C(0x04800018)
#define T64_SP_STATUS_DMA_BUSY UINT32_C(0x00000004)
#define T64_SP_STATUS_DMA_FULL UINT32_C(0x00000008)
#define T64_SP_STATUS_HALT UINT32_C(0x00000001)
#define T64_SP_STATUS_RESET_COMMAND UINT32_C(0x00aaaa0e)
#define T64_MI_INTERRUPT_SP UINT32_C(0x00000001)
#define T64_MI_INTERRUPT_SI UINT32_C(0x00000002)
#define T64_MI_INTERRUPT_AI UINT32_C(0x00000004)
#define T64_MI_INTERRUPT_DP UINT32_C(0x00000020)
#define T64_MI_INTR_WARM_IGNORED_WRITE UINT32_C(0x00000555)
#define T64_MI_INIT_CLEAR_DP_INTR UINT32_C(0x00000800)

static bool warm_policy_active(const t64_machine *machine) {
  return machine != NULL && machine->profile.rdram_bytes == UINT32_C(0x00400000) &&
         machine->guest_devices.policy_version ==
             T64_WARM_DEVICE_POLICY_VERSION;
}

static t64_status validate_warm_access(const t64_machine *machine,
                                       uint32_t width) {
  if (machine == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!warm_policy_active(machine) || width != 4u) {
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  return T64_STATUS_OK;
}

t64_status t64_warm_device_read(const t64_machine *machine, uint32_t address,
                                uint32_t width, uint32_t *value_out) {
  uint32_t value;
  t64_status status;

  if (machine == NULL || value_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  status = validate_warm_access(machine, width);
  if (status != T64_STATUS_OK) {
    return status;
  }
  switch (address) {
  case T64_MI_INIT_MODE:
    value = 0u;
    break;
  case T64_MI_VERSION:
    if (!machine->guest_devices.mi_version_known) {
      return T64_STATUS_BUS_UNSUPPORTED;
    }
    value = machine->guest_devices.mi_version;
    break;
  case T64_RI_SELECT:
    if (!machine->guest_devices.ri_select_known) {
      return T64_STATUS_BUS_UNSUPPORTED;
    }
    value = machine->guest_devices.ri_select;
    break;
  case T64_WARM_ABSENT_RDRAM_PROBE_ADDRESS:
    value = 0u;
    break;
  case T64_SP_STATUS:
    /* This qualified profile has no RSP engine: HALT remains set and the
       unmodeled status flags remain clear, while DMA flags stay live. */
    value = T64_SP_STATUS_HALT |
            (machine->sp.active.valid || machine->sp.pending.valid
                 ? T64_SP_STATUS_DMA_BUSY
                 : 0u) |
            (machine->sp.active.valid && machine->sp.pending.valid
                 ? T64_SP_STATUS_DMA_FULL
                 : 0u);
    break;
  case T64_SP_DMA_FULL:
    value = machine->sp.active.valid && machine->sp.pending.valid ? 1u : 0u;
    break;
  case T64_SP_DMA_BUSY:
    value = machine->sp.active.valid || machine->sp.pending.valid ? 1u : 0u;
    break;
  case T64_SP_SEMAPHORE:
  case T64_SP_PC:
  case T64_AI_STATUS:
  case T64_SI_STATUS:
    value = 0u;
    break;
  default:
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  *value_out = value;
  return T64_STATUS_OK;
}

t64_status t64_warm_device_write(t64_machine *machine, uint32_t address,
                                 uint32_t width, uint32_t value) {
  t64_status status = validate_warm_access(machine, width);

  if (status != T64_STATUS_OK) {
    return status;
  }
  switch (address) {
  case T64_MI_INIT_MODE:
    if (value == T64_MI_INIT_CLEAR_DP_INTR) {
      t64_mi_clear_pending(machine, T64_MI_INTERRUPT_DP);
      return T64_STATUS_OK;
    }
    return value == 0u || value == 1u ? T64_STATUS_OK
                                      : T64_STATUS_BUS_DEVICE_REFUSED;
  case T64_MI_VERSION:
    return T64_STATUS_BUS_DEVICE_REFUSED;
  case T64_MI_INTR:
    /* The SDK identifies MI_INTR as read-only. This exact ignored write is
       accepted only because the pinned warm loader emits it; it does not
       alter pending state, masks, DMA or device reset state. */
    return value == T64_MI_INTR_WARM_IGNORED_WRITE
               ? T64_STATUS_OK
               : T64_STATUS_BUS_DEVICE_REFUSED;
  case T64_RI_SELECT:
    if (value > UINT32_C(1)) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
    machine->guest_devices.ri_select = value;
    machine->guest_devices.ri_select_known = true;
    return T64_STATUS_OK;
  case T64_SP_STATUS:
    /* rcp.h documents 0x00aaaa0e as set HALT, clear BROKE/INTR and all
       signal bits. The named warm profile holds those flags at their reset
       values because no RSP engine can change them. Do not reset or cancel
       DMA here; the SP status command is separate from the machine reset. */
    if (value == 0u) {
      return T64_STATUS_OK;
    }
    if (value == T64_SP_STATUS_RESET_COMMAND) {
      t64_mi_clear_pending(machine, T64_MI_INTERRUPT_SP);
      return T64_STATUS_OK;
    }
    return T64_STATUS_BUS_DEVICE_REFUSED;
  case T64_SP_SEMAPHORE:
  case T64_SP_PC:
  case T64_AI_CONTROL:
    return value == 0u ? T64_STATUS_OK : T64_STATUS_BUS_DEVICE_REFUSED;
  case T64_AI_STATUS:
    /* The SDK documents writes as audio-interrupt clear. The warm profile
       admits only the reached zero write and uses canonical MI pending state. */
    if (value != 0u) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
    t64_mi_clear_pending(machine, T64_MI_INTERRUPT_AI);
    return T64_STATUS_OK;
  case T64_SI_STATUS:
    /* SI status writes clear SI interrupt; keep this profile's exact zero
       reset write and preserve all unrelated MI/DMA state. */
    if (value != 0u) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
    t64_mi_clear_pending(machine, T64_MI_INTERRUPT_SI);
    return T64_STATUS_OK;
  case T64_SP_DMA_FULL:
  case T64_SP_DMA_BUSY:
    return T64_STATUS_BUS_DEVICE_REFUSED;
  case T64_WARM_ABSENT_RDRAM_PROBE_ADDRESS:
    return T64_STATUS_OK;
  default:
    return T64_STATUS_BUS_UNSUPPORTED;
  }
}
