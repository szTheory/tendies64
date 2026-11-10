#include "t64_internal.h"

#include <stddef.h>
#include <stdint.h>

#define T64_SP_DMEM_BASE UINT32_C(0x04000000)
#define T64_SP_IMEM_BASE UINT32_C(0x04001000)
#define T64_SP_MEMORY_END UINT32_C(0x04002000)
#define T64_SP_REGISTER_BASE UINT32_C(0x04040000)
#define T64_SP_REGISTER_END UINT32_C(0x04040020)
#define T64_RDRAM_REG_BASE UINT32_C(0x03f00000)
#define T64_RDRAM_REG_END UINT32_C(0x04000000)
#define T64_SP_APERTURE_END UINT32_C(0x04100000)
#define T64_DP_BASE UINT32_C(0x04100000)
#define T64_MI_BASE UINT32_C(0x04300000)
#define T64_VI_BASE UINT32_C(0x04400000)
#define T64_AI_BASE UINT32_C(0x04500000)
#define T64_PI_BASE UINT32_C(0x04600000)
#define T64_RI_BASE UINT32_C(0x04700000)
#define T64_SI_BASE UINT32_C(0x04800000)
#define T64_DEVICE_WINDOW_BYTES UINT32_C(0x00100000)
#define T64_MI_END UINT32_C(0x04400000)
#define T64_CART_BASE UINT32_C(0x10000000)
#define T64_CART_END UINT32_C(0x1fc00000)
#define T64_DOMAIN2_A_BASE UINT32_C(0x05000000)
#define T64_DOMAIN2_A_END UINT32_C(0x06000000)
#define T64_DOMAIN1_B_BASE UINT32_C(0x06000000)
#define T64_DOMAIN1_B_END UINT32_C(0x08000000)
#define T64_DOMAIN2_B_BASE UINT32_C(0x08000000)
#define T64_DOMAIN2_B_END UINT32_C(0x10000000)
#define T64_DOMAIN1_C_BASE UINT32_C(0x1fd00000)
#define T64_DOMAIN1_C_END UINT32_C(0x80000000)
#define T64_PIF_BASE UINT32_C(0x1fc00000)
#define T64_PIF_END UINT32_C(0x1fc00800)

typedef enum {
  T64_BUS_REGION_UNMAPPED = 0,
  T64_BUS_REGION_RDRAM,
  T64_BUS_REGION_SP_DMEM,
  T64_BUS_REGION_SP_IMEM,
  T64_BUS_REGION_CARTRIDGE,
  T64_BUS_REGION_UNSUPPORTED,
  T64_BUS_REGION_MI,
  T64_BUS_REGION_PI,
  T64_BUS_REGION_SP_REG
} t64_bus_region;

static t64_bus_region decode_region(const t64_machine *machine,
                                   uint32_t address) {
  uint64_t ram_end = machine->profile.rdram_bytes;

  if ((uint64_t)address < ram_end) {
    return T64_BUS_REGION_RDRAM;
  }
  if (address < T64_RDRAM_REG_BASE) {
    return T64_BUS_REGION_UNSUPPORTED;
  }
  if (address >= T64_RDRAM_REG_BASE && address < T64_RDRAM_REG_END) {
    return T64_BUS_REGION_UNSUPPORTED;
  }
  if ((address >= T64_DOMAIN2_A_BASE && address < T64_DOMAIN2_A_END) ||
      (address >= T64_DOMAIN1_B_BASE && address < T64_DOMAIN1_B_END) ||
      (address >= T64_DOMAIN2_B_BASE && address < T64_DOMAIN2_B_END) ||
      (address >= T64_DOMAIN1_C_BASE && address < T64_DOMAIN1_C_END)) {
    return T64_BUS_REGION_UNSUPPORTED;
  }
  if (address >= T64_SP_DMEM_BASE && address < T64_SP_IMEM_BASE) {
    return T64_BUS_REGION_SP_DMEM;
  }
  if (address >= T64_SP_IMEM_BASE && address < T64_SP_MEMORY_END) {
    return T64_BUS_REGION_SP_IMEM;
  }
  if (address >= T64_SP_REGISTER_BASE && address < T64_SP_REGISTER_END) {
    return T64_BUS_REGION_SP_REG;
  }
  if (address >= T64_SP_DMEM_BASE && address < T64_SP_APERTURE_END) {
    return T64_BUS_REGION_UNSUPPORTED;
  }
  if (address >= T64_PI_BASE &&
      address < T64_PI_BASE + T64_DEVICE_WINDOW_BYTES) {
    return T64_BUS_REGION_PI;
  }
  if ((address >= T64_DP_BASE && address < T64_MI_BASE) ||
      (address >= T64_VI_BASE &&
       address < T64_VI_BASE + T64_DEVICE_WINDOW_BYTES) ||
      (address >= T64_AI_BASE &&
       address < T64_AI_BASE + T64_DEVICE_WINDOW_BYTES) ||
      (address >= T64_RI_BASE &&
       address < T64_RI_BASE + T64_DEVICE_WINDOW_BYTES) ||
      (address >= T64_SI_BASE &&
       address < T64_SI_BASE + T64_DEVICE_WINDOW_BYTES) ||
      (address >= T64_PIF_BASE && address < T64_PIF_END)) {
    return T64_BUS_REGION_UNSUPPORTED;
  }
  if (address >= T64_MI_BASE && address < T64_MI_END) {
    return T64_BUS_REGION_MI;
  }
  if (address >= T64_CART_BASE && address < T64_CART_END) {
    return T64_BUS_REGION_CARTRIDGE;
  }
  return T64_BUS_REGION_UNMAPPED;
}

static t64_status validate_access(uint32_t address, uint32_t width) {
  if (width != 1u && width != 4u) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if ((uint64_t)address + (uint64_t)width > UINT64_C(0x100000000)) {
    return T64_STATUS_BUS_RANGE;
  }
  if (width == 4u && (address & UINT32_C(3)) != 0u) {
    return T64_STATUS_BUS_ALIGNMENT;
  }
  return T64_STATUS_OK;
}

static uint32_t read_big_endian(const uint8_t *bytes, uint32_t width) {
  uint32_t value = bytes[0];

  if (width == 4u) {
    value = (value << 24) | ((uint32_t)bytes[1] << 16) |
            ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
  }
  return value;
}

static void write_big_endian(uint8_t *bytes, uint32_t width,
                             uint32_t value) {
  if (width == 1u) {
    bytes[0] = (uint8_t)value;
    return;
  }
  bytes[0] = (uint8_t)(value >> 24);
  bytes[1] = (uint8_t)(value >> 16);
  bytes[2] = (uint8_t)(value >> 8);
  bytes[3] = (uint8_t)value;
}

static const uint8_t *region_bytes(const t64_machine *machine,
                                   t64_bus_region region,
                                   uint32_t address, size_t *remaining_out) {
  size_t offset;

  switch (region) {
  case T64_BUS_REGION_RDRAM:
    offset = (size_t)address;
    *remaining_out = (size_t)machine->profile.rdram_bytes - offset;
    return machine->ram + offset;
  case T64_BUS_REGION_SP_DMEM:
    offset = (size_t)(address - T64_SP_DMEM_BASE);
    *remaining_out = sizeof machine->sp_dmem - offset;
    return machine->sp_dmem + offset;
  case T64_BUS_REGION_SP_IMEM:
    offset = (size_t)(address - T64_SP_IMEM_BASE);
    *remaining_out = sizeof machine->sp_imem - offset;
    return machine->sp_imem + offset;
  case T64_BUS_REGION_CARTRIDGE:
    offset = (size_t)(address - T64_CART_BASE);
    if (!machine->has_image || offset >= machine->image_length) {
      *remaining_out = 0u;
      return NULL;
    }
    *remaining_out = machine->image_length - offset;
    return (const uint8_t *)machine->image_storage.data + offset;
  case T64_BUS_REGION_UNMAPPED:
  case T64_BUS_REGION_UNSUPPORTED:
  case T64_BUS_REGION_MI:
  case T64_BUS_REGION_PI:
  case T64_BUS_REGION_SP_REG:
    break;
  }
  *remaining_out = 0u;
  return NULL;
}

static void trace_bus(t64_machine *machine, uint32_t address, uint32_t width,
                      uint32_t value, bool is_write) {
  uint32_t kind;
  uint64_t record_value = ((uint64_t)address << 32) | (uint64_t)value;

  if (width == 1u) {
    kind = is_write ? T64_TRACE_KIND_BUS_WRITE8 : T64_TRACE_KIND_BUS_READ8;
  } else {
    kind = is_write ? T64_TRACE_KIND_BUS_WRITE32 : T64_TRACE_KIND_BUS_READ32;
  }
  t64_trace_emit(machine, machine->tick, T64_TRACE_DEVICE_BUS, kind,
                 record_value);
}

t64_status t64_bus_read(t64_machine *machine, uint32_t address,
                        uint32_t width, uint32_t *value_out) {
  t64_bus_region region;
  const uint8_t *bytes;
  uint32_t value;
  size_t remaining = 0u;
  t64_status status;

  if (machine == NULL || value_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  status = validate_access(address, width);
  if (status != T64_STATUS_OK) {
    return status;
  }
  region = decode_region(machine, address);
  if (region == T64_BUS_REGION_UNMAPPED) {
    return T64_STATUS_BUS_UNMAPPED;
  }
  if (region == T64_BUS_REGION_UNSUPPORTED) {
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  if (region == T64_BUS_REGION_MI) {
    if (width != 4u || !t64_mi_register_read(machine, address, &value)) {
      return T64_STATUS_BUS_UNSUPPORTED;
    }
    *value_out = value;
    trace_bus(machine, address, width, value, false);
    return T64_STATUS_OK;
  }
  if (region == T64_BUS_REGION_PI) {
    if (width != 4u) {
      return T64_STATUS_BUS_UNSUPPORTED;
    }
    status = t64_pi_read_register(machine, address, &value);
    if (status != T64_STATUS_OK) {
      return status;
    }
    *value_out = value;
    trace_bus(machine, address, width, value, false);
    return T64_STATUS_OK;
  }
  if (region == T64_BUS_REGION_SP_REG) {
    if (width != 4u) {
      return T64_STATUS_BUS_UNSUPPORTED;
    }
    status = t64_sp_read_register(machine, address, &value);
    if (status != T64_STATUS_OK) {
      return status;
    }
    *value_out = value;
    trace_bus(machine, address, width, value, false);
    return T64_STATUS_OK;
  }
  bytes = region_bytes(machine, region, address, &remaining);
  if (bytes == NULL) {
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  if ((size_t)width > remaining) {
    return T64_STATUS_BUS_RANGE;
  }
  value = read_big_endian(bytes, width);
  *value_out = value;
  trace_bus(machine, address, width, value, false);
  return T64_STATUS_OK;
}

t64_status t64_bus_write(t64_machine *machine, uint32_t address,
                         uint32_t width, uint32_t value) {
  t64_bus_region region;
  uint8_t *bytes = NULL;
  size_t offset;
  size_t remaining = 0u;
  t64_status status;

  if (machine == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  status = validate_access(address, width);
  if (status != T64_STATUS_OK) {
    return status;
  }
  region = decode_region(machine, address);
  if (region == T64_BUS_REGION_UNMAPPED) {
    return T64_STATUS_BUS_UNMAPPED;
  }
  if (region == T64_BUS_REGION_CARTRIDGE) {
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  if (region == T64_BUS_REGION_UNSUPPORTED) {
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  if (region == T64_BUS_REGION_MI) {
    if (width != 4u) {
      return T64_STATUS_BUS_UNSUPPORTED;
    }
    status = t64_mi_register_write(machine, address, value);
    if (status == T64_STATUS_OK) {
      trace_bus(machine, address, width, value, true);
    }
    return status;
  }
  if (region == T64_BUS_REGION_PI) {
    if (width != 4u) {
      return T64_STATUS_BUS_UNSUPPORTED;
    }
    status = t64_pi_write_register(machine, address, value);
    if (status == T64_STATUS_OK) {
      trace_bus(machine, address, width, value, true);
    }
    return status;
  }
  if (region == T64_BUS_REGION_SP_REG) {
    if (width != 4u) {
      return T64_STATUS_BUS_UNSUPPORTED;
    }
    status = t64_sp_write_register(machine, address, value);
    if (status == T64_STATUS_OK) {
      trace_bus(machine, address, width, value, true);
    }
    return status;
  }
  switch (region) {
  case T64_BUS_REGION_RDRAM:
    offset = (size_t)address;
    remaining = (size_t)machine->profile.rdram_bytes - offset;
    bytes = machine->ram + offset;
    break;
  case T64_BUS_REGION_SP_DMEM:
    offset = (size_t)(address - T64_SP_DMEM_BASE);
    remaining = sizeof machine->sp_dmem - offset;
    bytes = machine->sp_dmem + offset;
    break;
  case T64_BUS_REGION_SP_IMEM:
    offset = (size_t)(address - T64_SP_IMEM_BASE);
    remaining = sizeof machine->sp_imem - offset;
    bytes = machine->sp_imem + offset;
    break;
  case T64_BUS_REGION_CARTRIDGE:
  case T64_BUS_REGION_UNMAPPED:
  case T64_BUS_REGION_UNSUPPORTED:
  case T64_BUS_REGION_MI:
  case T64_BUS_REGION_PI:
  case T64_BUS_REGION_SP_REG:
    break;
  }
  if (bytes == NULL || (size_t)width > remaining) {
    return T64_STATUS_BUS_RANGE;
  }
  write_big_endian(bytes, width, value);
  trace_bus(machine, address, width, value, true);
  return T64_STATUS_OK;
}

t64_status t64_machine_bus_read(t64_machine *machine, uint32_t address,
                                uint32_t width, uint32_t *value_out) {
  return t64_bus_read(machine, address, width, value_out);
}

t64_status t64_machine_bus_write(t64_machine *machine, uint32_t address,
                                 uint32_t width, uint32_t value) {
  return t64_bus_write(machine, address, width, value);
}
