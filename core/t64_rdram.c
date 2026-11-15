#include "t64_internal.h"

#include <stdint.h>

#define T64_RI_CONFIG_ADDRESS UINT32_C(0x04700004)
#define T64_RI_CURRENT_LOAD_ADDRESS UINT32_C(0x04700008)
#define T64_RI_SELECT_ADDRESS UINT32_C(0x0470000c)
#define T64_RI_CONFIG_COLD_START UINT32_C(0x00000040)
#define T64_RDRAM_MODEL_WAIT_TICKS UINT64_C(0x100)

t64_status t64_rdram_read(const t64_machine *machine, uint32_t address,
                          uint32_t width, uint32_t *value_out) {
  if (machine == NULL || value_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!t64_has_rdram_protocol_policy(machine)) {
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  if (width != 4u) {
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  if (address == T64_RI_CONFIG_ADDRESS) {
    if (!machine->guest_devices.rdram_protocol.ri_config_known) {
      return T64_STATUS_BUS_UNSUPPORTED;
    }
    *value_out = machine->guest_devices.rdram_protocol.ri_config;
    return T64_STATUS_OK;
  }
  if (address == T64_RI_CURRENT_LOAD_ADDRESS) {
    if (!machine->guest_devices.rdram_protocol.ri_current_load_known) {
      return T64_STATUS_BUS_UNSUPPORTED;
    }
    *value_out = machine->guest_devices.rdram_protocol.ri_current_load;
    return T64_STATUS_OK;
  }
  return t64_warm_device_read(machine, address, width, value_out);
}

t64_status t64_rdram_write(t64_machine *machine, uint32_t address,
                           uint32_t width, uint32_t value) {
  if (machine == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!t64_has_rdram_protocol_policy(machine)) {
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  if (width != 4u) {
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  if (address == T64_RI_CONFIG_ADDRESS) {
    if (machine->guest_devices.rdram_protocol.ri_config_known) {
      return machine->guest_devices.rdram_protocol.ri_config == value
                 ? T64_STATUS_OK
                 : T64_STATUS_BUS_DEVICE_REFUSED;
    }
    if (!machine->guest_devices.ri_select_known ||
        machine->guest_devices.ri_select != 0u ||
        value != T64_RI_CONFIG_COLD_START) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
    if (machine->tick > UINT64_MAX - T64_RDRAM_MODEL_WAIT_TICKS) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
    machine->guest_devices.rdram_protocol.ri_config = value;
    machine->guest_devices.rdram_protocol.ri_config_known = true;
    machine->guest_devices.rdram_protocol.ri_config_tick = machine->tick;
    machine->guest_devices.rdram_protocol.ri_current_load_deadline_tick =
        machine->guest_devices.rdram_protocol.ri_config_tick +
        T64_RDRAM_MODEL_WAIT_TICKS;
    return T64_STATUS_OK;
  }
  if (address == T64_RI_CURRENT_LOAD_ADDRESS) {
    if (!machine->guest_devices.rdram_protocol.ri_config_known ||
        value != 0u || machine->tick <
                           machine->guest_devices.rdram_protocol
                               .ri_current_load_deadline_tick) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
    if (machine->guest_devices.rdram_protocol.ri_current_load_known &&
        machine->guest_devices.rdram_protocol.ri_current_load != value) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
    machine->guest_devices.rdram_protocol.ri_current_load = value;
    machine->guest_devices.rdram_protocol.ri_current_load_known = true;
    return T64_STATUS_OK;
  }
  if (address == T64_RI_SELECT_ADDRESS) {
    return t64_warm_device_write(machine, address, width, value);
  }
  return T64_STATUS_BUS_UNSUPPORTED;
}
