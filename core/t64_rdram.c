#include "t64_internal.h"

#include <stdint.h>
#include <string.h>

#define T64_RDRAM_REG_BASE UINT32_C(0x03f00000)
#define T64_RDRAM_BROADCAST_BASE UINT32_C(0x03f80000)
#define T64_RDRAM_REG_END UINT32_C(0x04000000)
#define T64_RDRAM_REGISTER_STRIDE UINT32_C(0x400)

#define T64_RDRAM_REG_DEVICE_TYPE UINT32_C(0)
#define T64_RDRAM_REG_DEVICE_ID UINT32_C(1)
#define T64_RDRAM_REG_DELAY UINT32_C(2)
#define T64_RDRAM_REG_MODE UINT32_C(3)
#define T64_RDRAM_REG_REF_ROW UINT32_C(5)
#define T64_RDRAM_REG_RAS_INTERVAL UINT32_C(6)
#define T64_RDRAM_REG_MANUFACTURER UINT32_C(9)

#define T64_RDRAM_INITIAL_DEVICE_ID UINT32_C(511)
#define T64_RDRAM_FIRST_DEVICE_ID UINT32_C(0)
#define T64_RDRAM_SECOND_DEVICE_ID UINT32_C(2)
#define T64_RDRAM_ABSENT_DEVICE_ID UINT32_C(4)
#define T64_RDRAM_DEVICE_ENABLE UINT32_C(0x00000002)
#define T64_RDRAM_AUTO_CURRENT UINT32_C(0x00000080)
#define T64_RDRAM_BROADCAST_MODE UINT32_C(0x00000044)
#define T64_RDRAM_DEVICE_MODE UINT32_C(0x00000046)
#define T64_RDRAM_MODEL_DEVICE_TYPE_2M UINT32_C(0x000019b4)
/* Functional feedback v1: source CCVALUE bit positions, not analog current. */
#define T64_RDRAM_CURRENT_FIELDS UINT32_C(0xc0c0c000)

#define T64_RI_MODE_ADDRESS UINT32_C(0x04700000)
#define T64_RI_CONFIG_ADDRESS UINT32_C(0x04700004)
#define T64_RI_CURRENT_LOAD_ADDRESS UINT32_C(0x04700008)
#define T64_RI_SELECT_ADDRESS UINT32_C(0x0470000c)
#define T64_RI_REFRESH_ADDRESS UINT32_C(0x04700010)
#define T64_RI_CONFIG_COLD_START UINT32_C(0x00000040)
#define T64_RI_SELECT_RX_TX UINT32_C(0x00000014)
#define T64_RI_MODE_RESET UINT32_C(0x00000000)
#define T64_RI_MODE_STANDARD UINT32_C(0x0000000e)
#define T64_RDRAM_MODEL_WAIT_TICKS UINT64_C(0x100)

typedef struct {
  bool broadcast;
  uint16_t device_id;
  uint32_t register_index;
} t64_rdram_register_address;

static uint32_t swap_u32(uint32_t value) {
  return ((value & UINT32_C(0x000000ff)) << 24) |
         ((value & UINT32_C(0x0000ff00)) << 8) |
         ((value & UINT32_C(0x00ff0000)) >> 8) |
         ((value & UINT32_C(0xff000000)) >> 24);
}

static uint16_t decode_device_id(uint32_t value) {
  uint32_t id = ((value >> 2) & UINT32_C(0x3f)) |
                (((value >> 15) & UINT32_C(0x1ff)) << 6) |
                (((value >> 31) & UINT32_C(1)) << 15);

  return id <= UINT32_C(511) ? (uint16_t)id : UINT16_MAX;
}

static uint32_t encode_device_id(uint16_t device_id) {
  uint32_t id = (uint32_t)device_id;

  return ((id & UINT32_C(0x3f)) << 2) |
         (((id >> 6) & UINT32_C(0x1ff)) << 15) |
         (((id >> 15) & UINT32_C(1)) << 31);
}

static uint16_t register_bit(uint32_t index) {
  return (uint16_t)(UINT16_C(1) << index);
}

static bool rdram_access_ready(const t64_machine *machine) {
  const t64_rdram_protocol_state *protocol;

  if (machine == NULL) {
    return false;
  }
  protocol = &machine->guest_devices.rdram_protocol;
  return protocol->ri_mode_known && protocol->ri_mode == T64_RI_MODE_STANDARD &&
         machine->tick >= protocol->ri_mode_deadline_tick;
}

static void reset_device_protocol(t64_machine *machine) {
  t64_rdram_protocol_state *protocol =
      &machine->guest_devices.rdram_protocol;
  size_t index;

  (void)memset(protocol->devices, 0, sizeof protocol->devices);
  (void)memset(protocol->broadcast_register_values, 0,
               sizeof protocol->broadcast_register_values);
  protocol->broadcast_registers_known = 0u;
  protocol->mi_repeat_enabled = false;
  protocol->mi_repeat_known = false;
  protocol->mi_repeat_length = 0u;
  protocol->mi_upper_mode = false;
  protocol->logical_device_count = T64_RDRAM_PROTOCOL_SLOT_COUNT;
  protocol->capacity_scan_complete = false;
  protocol->logical_capacity_bytes = 0u;
  protocol->ri_refresh = 0u;
  protocol->ri_refresh_known = false;

  /* The v1 software model names two 2 MiB logical slots at IDs 0 and 2,
     following the pinned IPL3 discovery loop. This is not a physical topology
     claim derived from the profile's 4 MiB capacity. */
  for (index = 0u; index < T64_RDRAM_PROTOCOL_SLOT_COUNT; index++) {
    t64_rdram_device_state *device = &protocol->devices[index];

    device->present = true;
    device->device_id = (uint16_t)(index * 2u);
    device->device_id_known = true;
    device->register_values[T64_RDRAM_REG_DEVICE_ID] = device->device_id;
    device->registers_known |= register_bit(T64_RDRAM_REG_DEVICE_ID);
    device->register_values[T64_RDRAM_REG_MODE] = T64_RI_MODE_RESET;
    device->registers_known |= register_bit(T64_RDRAM_REG_MODE);
  }
}

static bool decode_register_address(uint32_t address,
                                    t64_rdram_register_address *decoded) {
  uint32_t offset;
  uint32_t byte_offset;

  if (decoded == NULL) {
    return false;
  }
  if (address >= T64_RDRAM_REG_BASE &&
      address < T64_RDRAM_BROADCAST_BASE) {
    offset = address - T64_RDRAM_REG_BASE;
    byte_offset = offset % T64_RDRAM_REGISTER_STRIDE;
    if ((byte_offset & UINT32_C(3)) != 0u ||
        (byte_offset >> 2) >= T64_RDRAM_PROTOCOL_REGISTER_COUNT) {
      return false;
    }
    decoded->broadcast = false;
    decoded->device_id = (uint16_t)(offset / T64_RDRAM_REGISTER_STRIDE);
    decoded->register_index = byte_offset >> 2;
    return true;
  }
  if (address >= T64_RDRAM_BROADCAST_BASE &&
      address < T64_RDRAM_REG_END) {
    offset = address - T64_RDRAM_BROADCAST_BASE;
    if ((offset & UINT32_C(3)) != 0u ||
        (offset >> 2) >= T64_RDRAM_PROTOCOL_REGISTER_COUNT) {
      return false;
    }
    decoded->broadcast = true;
    decoded->device_id = UINT16_MAX;
    decoded->register_index = offset >> 2;
    return true;
  }
  return false;
}

static t64_rdram_device_state *find_device(t64_rdram_protocol_state *protocol,
                                           uint16_t device_id) {
  size_t index;

  for (index = 0u; index < T64_RDRAM_PROTOCOL_SLOT_COUNT; index++) {
    t64_rdram_device_state *device = &protocol->devices[index];

    if (device->present && device->device_id_known &&
        device->device_id == device_id) {
      return device;
    }
  }
  return NULL;
}

static const t64_rdram_device_state *find_const_device(
    const t64_rdram_protocol_state *protocol, uint16_t device_id) {
  size_t index;

  for (index = 0u; index < T64_RDRAM_PROTOCOL_SLOT_COUNT; index++) {
    const t64_rdram_device_state *device = &protocol->devices[index];

    if (device->present && device->device_id_known &&
        device->device_id == device_id) {
      return device;
    }
  }
  return NULL;
}

static bool device_scan_ready(const t64_rdram_protocol_state *protocol) {
  size_t index;

  if (protocol == NULL ||
      protocol->logical_device_count != T64_RDRAM_PROTOCOL_SLOT_COUNT) {
    return false;
  }
  for (index = 0u; index < T64_RDRAM_PROTOCOL_SLOT_COUNT; index++) {
    const t64_rdram_device_state *device = &protocol->devices[index];

    if (!device->present || !device->device_id_known ||
        device->device_id != (uint16_t)(index * 2u) ||
        device->calibration_state != T64_RDRAM_CALIBRATION_AUTO_COMMAND ||
        (device->register_values[T64_RDRAM_REG_MODE] &
         T64_RDRAM_DEVICE_ENABLE) == 0u) {
      return false;
    }
  }
  return true;
}

static t64_status write_broadcast(t64_machine *machine, uint32_t reg,
                                  uint32_t raw_value) {
  t64_rdram_protocol_state *protocol =
      &machine->guest_devices.rdram_protocol;
  uint32_t value = swap_u32(raw_value);
  size_t index;

  if (reg == T64_RDRAM_REG_DELAY) {
    if (!protocol->mi_repeat_known || !protocol->mi_repeat_enabled ||
        protocol->mi_repeat_length != UINT32_C(16)) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
  } else if (reg == T64_RDRAM_REG_DEVICE_ID) {
    if ((protocol->broadcast_registers_known &
         register_bit(T64_RDRAM_REG_DELAY)) == 0u ||
        decode_device_id(value) != T64_RDRAM_INITIAL_DEVICE_ID) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
  } else if (reg == T64_RDRAM_REG_MODE) {
    if ((protocol->broadcast_registers_known &
         register_bit(T64_RDRAM_REG_DEVICE_ID)) == 0u ||
        value != T64_RDRAM_BROADCAST_MODE) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
  } else if (reg == T64_RDRAM_REG_REF_ROW) {
    if ((protocol->broadcast_registers_known &
         register_bit(T64_RDRAM_REG_MODE)) == 0u || value != 0u) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
  } else {
    return T64_STATUS_BUS_UNSUPPORTED;
  }

  protocol->broadcast_register_values[reg] = value;
  protocol->broadcast_registers_known |= register_bit(reg);
  for (index = 0u; index < T64_RDRAM_PROTOCOL_SLOT_COUNT; index++) {
    t64_rdram_device_state *device = &protocol->devices[index];

    if (reg == T64_RDRAM_REG_DEVICE_ID) {
      device->device_id = (uint16_t)T64_RDRAM_INITIAL_DEVICE_ID;
      device->device_id_known = true;
    }
    if (reg == T64_RDRAM_REG_MODE) {
      device->calibration_state = T64_RDRAM_CALIBRATION_NONE;
    }
    device->register_values[reg] = value;
    device->registers_known |= register_bit(reg);
  }
  return T64_STATUS_OK;
}

static t64_status write_device_register(t64_machine *machine, uint16_t id,
                                        uint32_t reg, uint32_t raw_value) {
  t64_rdram_protocol_state *protocol =
      &machine->guest_devices.rdram_protocol;
  t64_rdram_device_state *device = find_device(protocol, id);
  uint32_t value = swap_u32(raw_value);

  if (reg == T64_RDRAM_REG_DEVICE_ID) {
    uint16_t new_id = decode_device_id(value);
    t64_rdram_device_state *existing;

    if (new_id == UINT16_MAX) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
    if (device == NULL) {
      if (id == T64_RDRAM_INITIAL_DEVICE_ID &&
          new_id == T64_RDRAM_ABSENT_DEVICE_ID &&
          device_scan_ready(protocol)) {
        return T64_STATUS_OK;
      }
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
    if ((protocol->broadcast_registers_known &
         register_bit(T64_RDRAM_REG_REF_ROW)) == 0u) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
    if (id == T64_RDRAM_INITIAL_DEVICE_ID) {
      if ((new_id != T64_RDRAM_FIRST_DEVICE_ID &&
           new_id != T64_RDRAM_SECOND_DEVICE_ID) ||
          (device == &protocol->devices[0] &&
           new_id != T64_RDRAM_FIRST_DEVICE_ID) ||
          (device == &protocol->devices[1] &&
           new_id != T64_RDRAM_SECOND_DEVICE_ID)) {
        return T64_STATUS_BUS_DEVICE_REFUSED;
      }
    } else {
      existing = find_device(protocol, new_id);
      if (existing != NULL && existing != device) {
        return T64_STATUS_BUS_DEVICE_REFUSED;
      }
    }
    device->device_id = new_id;
    device->device_id_known = true;
    device->register_values[T64_RDRAM_REG_DEVICE_ID] = new_id;
    device->registers_known |= register_bit(T64_RDRAM_REG_DEVICE_ID);
    return T64_STATUS_OK;
  }

  if (device == NULL) {
    if (id == T64_RDRAM_ABSENT_DEVICE_ID &&
        reg == T64_RDRAM_REG_MODE &&
        value == T64_RDRAM_DEVICE_MODE && device_scan_ready(protocol)) {
      protocol->capacity_scan_complete = true;
      protocol->logical_capacity_bytes = machine->profile.rdram_bytes;
      return T64_STATUS_OK;
    }
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }

  if (reg == T64_RDRAM_REG_MODE) {
    if ((value & T64_RDRAM_DEVICE_ENABLE) == 0u ||
        (value & T64_RDRAM_DEVICE_MODE) != T64_RDRAM_DEVICE_MODE ||
        (protocol->broadcast_registers_known &
         register_bit(T64_RDRAM_REG_REF_ROW)) == 0u) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
    if ((value & T64_RDRAM_AUTO_CURRENT) != 0u) {
      device->calibration_state = T64_RDRAM_CALIBRATION_AUTO_COMMAND;
    } else {
      device->calibration_state = T64_RDRAM_CALIBRATION_MANUAL_COMMAND;
    }
  } else if (reg == T64_RDRAM_REG_DELAY) {
    if (!protocol->mi_repeat_known || !protocol->mi_repeat_enabled) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
  } else if (reg == T64_RDRAM_REG_RAS_INTERVAL) {
    if (device->calibration_state !=
        T64_RDRAM_CALIBRATION_AUTO_COMMAND) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
  } else if (reg == T64_RDRAM_REG_REF_ROW) {
    /* The source touches row state only as a named register operation; this
       model stores the command without simulating analog refresh behavior. */
  } else {
    return T64_STATUS_BUS_UNSUPPORTED;
  }

  device->register_values[reg] = value;
  device->registers_known |= register_bit(reg);
  return T64_STATUS_OK;
}

static t64_status read_device_register(const t64_rdram_protocol_state *protocol,
                                       uint16_t id, uint32_t reg,
                                       uint32_t *value_out) {
  const t64_rdram_device_state *device;
  uint32_t value;

  if (protocol == NULL || value_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if ((reg & UINT32_C(1)) != 0u && !protocol->mi_upper_mode) {
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  device = find_const_device(protocol, id);
  if (device == NULL) {
    if (id == T64_RDRAM_ABSENT_DEVICE_ID && reg == T64_RDRAM_REG_MODE &&
        device_scan_ready(protocol)) {
      *value_out = 0u;
      return T64_STATUS_OK;
    }
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  if (reg == T64_RDRAM_REG_DEVICE_ID) {
    if (!device->device_id_known) {
      return T64_STATUS_BUS_UNSUPPORTED;
    }
    value = encode_device_id(device->device_id);
  } else if (reg == T64_RDRAM_REG_DEVICE_TYPE) {
    if (device->calibration_state != T64_RDRAM_CALIBRATION_AUTO_COMMAND) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
    value = T64_RDRAM_MODEL_DEVICE_TYPE_2M;
  } else if (reg == T64_RDRAM_REG_MANUFACTURER) {
    if (device->calibration_state != T64_RDRAM_CALIBRATION_AUTO_COMMAND) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
    /* Zero is a versioned unknown-manufacturer sentinel, not a vendor ID. */
    value = 0u;
  } else if (reg == T64_RDRAM_REG_MODE &&
             device->calibration_state == T64_RDRAM_CALIBRATION_AUTO_COMMAND) {
    /* Report logical requested CCI. The source writes its complement into
     * these fields. This deterministic feedback is not a measured current. */
    value = device->register_values[reg] ^ T64_RDRAM_CURRENT_FIELDS;
  } else if ((device->registers_known & register_bit(reg)) != 0u) {
    value = device->register_values[reg];
  } else {
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  *value_out = swap_u32(value);
  return T64_STATUS_OK;
}

uint64_t t64_rdram_filter_read(const t64_machine *machine, uint32_t address,
                               uint32_t width, uint64_t value) {
  uint32_t byte;
  if (!t64_has_rdram_protocol_policy(machine) || width == 0u || width > 8u) {
    return value;
  }
  for (byte = 0u; byte < width; byte++) {
    uint64_t location = (uint64_t)address + byte;
    uint64_t slot = location / T64_RDRAM_PROTOCOL_SLOT_BYTES;
    const t64_rdram_device_state *device;
    if (location >= machine->profile.rdram_bytes ||
        slot >= T64_RDRAM_PROTOCOL_SLOT_COUNT) continue;
    device = &machine->guest_devices.rdram_protocol.devices[(size_t)slot];
    /* The permitted algorithm starts its manual sweep at logical CCI0.
     * Versioned software feedback makes that origin unready, and nonzero
     * codes ready. Backing/writes remain intact; no read-side counter or
     * host state exists. Later mode changes obey the same rule. */
    if (device->calibration_state == T64_RDRAM_CALIBRATION_MANUAL_COMMAND &&
        (device->register_values[T64_RDRAM_REG_MODE] &
         T64_RDRAM_CURRENT_FIELDS) == T64_RDRAM_CURRENT_FIELDS) {
      value &= ~(UINT64_C(0xff) << ((width - 1u - byte) * 8u));
    }
  }
  return value;
}

t64_status t64_rdram_read(const t64_machine *machine, uint32_t address,
                          uint32_t width, uint32_t *value_out) {
  const t64_rdram_protocol_state *protocol;
  t64_rdram_register_address decoded;

  if (machine == NULL || value_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!t64_has_rdram_protocol_policy(machine)) {
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  if (width != 4u) {
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  protocol = &machine->guest_devices.rdram_protocol;
  if (address == T64_RI_CONFIG_ADDRESS) {
    if (!protocol->ri_config_known) {
      return T64_STATUS_BUS_UNSUPPORTED;
    }
    *value_out = protocol->ri_config;
    return T64_STATUS_OK;
  }
  if (address == T64_RI_CURRENT_LOAD_ADDRESS) {
    if (!protocol->ri_current_load_known) {
      return T64_STATUS_BUS_UNSUPPORTED;
    }
    *value_out = protocol->ri_current_load;
    return T64_STATUS_OK;
  }
  if (address == T64_RI_MODE_ADDRESS) {
    if (!protocol->ri_mode_known) {
      return T64_STATUS_BUS_UNSUPPORTED;
    }
    *value_out = protocol->ri_mode;
    return T64_STATUS_OK;
  }
  if (address == T64_RI_REFRESH_ADDRESS) {
    if (!protocol->ri_refresh_known) {
      return T64_STATUS_BUS_UNSUPPORTED;
    }
    *value_out = protocol->ri_refresh;
    return T64_STATUS_OK;
  }
  if (!decode_register_address(address, &decoded)) {
    return t64_warm_device_read(machine, address, width, value_out);
  }
  if (!rdram_access_ready(machine)) {
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  if (decoded.broadcast) {
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  return read_device_register(protocol, decoded.device_id,
                              decoded.register_index, value_out);
}

t64_status t64_rdram_write(t64_machine *machine, uint32_t address,
                           uint32_t width, uint32_t value) {
  t64_rdram_protocol_state *protocol;
  t64_rdram_register_address decoded;

  if (machine == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!t64_has_rdram_protocol_policy(machine)) {
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  if (width != 4u) {
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  protocol = &machine->guest_devices.rdram_protocol;
  if (address == T64_RI_CONFIG_ADDRESS) {
    if (protocol->ri_config_known) {
      return protocol->ri_config == value ? T64_STATUS_OK
                                          : T64_STATUS_BUS_DEVICE_REFUSED;
    }
    if (!machine->guest_devices.ri_select_known ||
        machine->guest_devices.ri_select != 0u ||
        value != T64_RI_CONFIG_COLD_START ||
        machine->tick > UINT64_MAX - T64_RDRAM_MODEL_WAIT_TICKS) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
    protocol->ri_config = value;
    protocol->ri_config_known = true;
    protocol->ri_config_tick = machine->tick;
    protocol->ri_current_load_deadline_tick =
        machine->tick + T64_RDRAM_MODEL_WAIT_TICKS;
    return T64_STATUS_OK;
  }
  if (address == T64_RI_CURRENT_LOAD_ADDRESS) {
    if (!protocol->ri_config_known || value != 0u ||
        machine->tick < protocol->ri_current_load_deadline_tick) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
    if (protocol->ri_current_load_known && protocol->ri_current_load != value) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
    protocol->ri_current_load = value;
    protocol->ri_current_load_known = true;
    return T64_STATUS_OK;
  }
  if (address == T64_RI_SELECT_ADDRESS) {
    if (!protocol->ri_current_load_known || value != T64_RI_SELECT_RX_TX) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
    machine->guest_devices.ri_select = value;
    machine->guest_devices.ri_select_known = true;
    return T64_STATUS_OK;
  }
  if (address == T64_RI_MODE_ADDRESS) {
    if (value == T64_RI_MODE_RESET) {
      if (!machine->guest_devices.ri_select_known ||
          machine->guest_devices.ri_select != T64_RI_SELECT_RX_TX ||
          !protocol->ri_current_load_known ||
          machine->tick > UINT64_MAX - T64_RDRAM_MODEL_WAIT_TICKS ||
          machine->profile.rdram_bytes != UINT32_C(0x00400000)) {
        return T64_STATUS_BUS_DEVICE_REFUSED;
      }
      protocol->ri_mode = value;
      protocol->ri_mode_known = true;
      protocol->ri_mode_tick = machine->tick;
      protocol->ri_mode_deadline_tick =
          machine->tick + T64_RDRAM_MODEL_WAIT_TICKS;
      reset_device_protocol(machine);
      return T64_STATUS_OK;
    }
    if (value == T64_RI_MODE_STANDARD) {
      if (protocol->ri_mode_known &&
          protocol->ri_mode == T64_RI_MODE_STANDARD) {
        return T64_STATUS_OK;
      }
      if (!protocol->ri_mode_known ||
          protocol->ri_mode != T64_RI_MODE_RESET ||
          machine->tick < protocol->ri_mode_deadline_tick ||
          machine->tick > UINT64_MAX - T64_RDRAM_MODEL_WAIT_TICKS) {
        return T64_STATUS_BUS_DEVICE_REFUSED;
      }
      protocol->ri_mode = value;
      protocol->ri_mode_tick = machine->tick;
      protocol->ri_mode_deadline_tick =
          machine->tick + T64_RDRAM_MODEL_WAIT_TICKS;
      return T64_STATUS_OK;
    }
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  if (address == T64_RI_REFRESH_ADDRESS) {
    protocol->ri_refresh = value;
    protocol->ri_refresh_known = true;
    return T64_STATUS_OK;
  }
  if (!decode_register_address(address, &decoded)) {
    if (address == T64_RI_SELECT_ADDRESS) {
      return t64_warm_device_write(machine, address, width, value);
    }
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  if (!rdram_access_ready(machine)) {
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  if (decoded.broadcast) {
    return write_broadcast(machine, decoded.register_index, value);
  }
  return write_device_register(machine, decoded.device_id,
                               decoded.register_index, value);
}
