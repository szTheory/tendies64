#include "t64_internal.h"

#include <stdint.h>

#define T64_MI_INIT_MODE UINT32_C(0x04300000)
#define T64_MI_VERSION UINT32_C(0x04300004)
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
    value = (machine->sp.active.valid || machine->sp.pending.valid
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
    return value == 0u || value == 1u ? T64_STATUS_OK
                                      : T64_STATUS_BUS_DEVICE_REFUSED;
  case T64_MI_VERSION:
    return T64_STATUS_BUS_DEVICE_REFUSED;
  case T64_RI_SELECT:
    if (value > UINT32_C(1)) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
    machine->guest_devices.ri_select = value;
    machine->guest_devices.ri_select_known = true;
    return T64_STATUS_OK;
  case T64_SP_STATUS:
    /* RSP execution is out of scope. Only the warm reset's zero command is
       accepted; it changes no unmodeled RSP state. */
    return value == 0u ? T64_STATUS_OK : T64_STATUS_BUS_DEVICE_REFUSED;
  case T64_SP_SEMAPHORE:
  case T64_SP_PC:
  case T64_AI_CONTROL:
  case T64_SI_STATUS:
    return value == 0u ? T64_STATUS_OK : T64_STATUS_BUS_DEVICE_REFUSED;
  case T64_SP_DMA_FULL:
  case T64_SP_DMA_BUSY:
  case T64_AI_STATUS:
    return T64_STATUS_BUS_DEVICE_REFUSED;
  case T64_WARM_ABSENT_RDRAM_PROBE_ADDRESS:
    return T64_STATUS_OK;
  default:
    return T64_STATUS_BUS_UNSUPPORTED;
  }
}
