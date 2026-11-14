#include "t64_internal.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

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
  T64_BUS_REGION_SP_REG,
  T64_BUS_REGION_WARM,
  T64_BUS_REGION_ISVIEWER,
  T64_BUS_REGION_ABSENT_RDRAM_PROBE
} t64_bus_region;

bool t64_cpu_compat_zero_data_range(const t64_machine *machine,
                                   uint32_t address, size_t bytes) {
  uint64_t end;
  const uint64_t probe = T64_WARM_ABSENT_RDRAM_PROBE_ADDRESS;

  if (!t64_has_cpu_compat_extension_policy(machine) || bytes == 0u ||
      bytes > 32u) {
    return false;
  }
  end = (uint64_t)address + (uint64_t)bytes;
  if (end > UINT64_C(0x100000000)) {
    return false;
  }
  /* CPU data policy only: do not change bus decoding, fetch, MMIO, DMA,
   * other cartridge domains, or the modeled legacy absent-RDRAM probe. */
  if ((uint64_t)address >= machine->profile.rdram_bytes &&
      end <= T64_RDRAM_REG_BASE) {
    if (machine->guest_devices.policy_version ==
            T64_WARM_DEVICE_POLICY_VERSION &&
        (uint64_t)address < probe + UINT64_C(4) && end > probe) {
      return false;
    }
    return true;
  }
  return address >= T64_DOMAIN1_C_BASE && end <= T64_DOMAIN1_C_END;
}

static t64_bus_region decode_region(const t64_machine *machine,
                                   uint32_t address) {
  uint64_t ram_end = machine->profile.rdram_bytes;

  if ((uint64_t)address < ram_end) {
    return T64_BUS_REGION_RDRAM;
  }
  if (address == T64_WARM_ABSENT_RDRAM_PROBE_ADDRESS &&
      ram_end == (uint64_t)T64_WARM_ABSENT_RDRAM_PROBE_ADDRESS &&
      machine->guest_devices.policy_version ==
          T64_WARM_DEVICE_POLICY_VERSION) {
    return T64_BUS_REGION_ABSENT_RDRAM_PROBE;
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
  if (address == T64_SP_PC_ADDRESS &&
      t64_has_rsp_functional_profile(machine)) {
    return T64_BUS_REGION_SP_REG;
  }
  if (address >= T64_ISVIEWER_BASE &&
      address - T64_ISVIEWER_BASE < T64_ISVIEWER_DEVICE_BYTES &&
      t64_isviewer_active(machine)) {
    return T64_BUS_REGION_ISVIEWER;
  }
  if (address == T64_SP_PC_ADDRESS ||
      (address >= T64_AI_BASE &&
       address < T64_AI_BASE + T64_DEVICE_WINDOW_BYTES) ||
      (address >= T64_RI_BASE &&
       address < T64_RI_BASE + T64_DEVICE_WINDOW_BYTES) ||
      (address >= T64_SI_BASE &&
       address < T64_SI_BASE + T64_DEVICE_WINDOW_BYTES)) {
    return T64_BUS_REGION_WARM;
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
  if (width != 1u && width != 2u && width != 4u) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if ((uint64_t)address + (uint64_t)width > UINT64_C(0x100000000)) {
    return T64_STATUS_BUS_RANGE;
  }
  if (width == 4u && (address & UINT32_C(3)) != 0u) {
    return T64_STATUS_BUS_ALIGNMENT;
  }
  if (width == 2u && (address & UINT32_C(1)) != 0u) {
    return T64_STATUS_BUS_ALIGNMENT;
  }
  return T64_STATUS_OK;
}

static uint32_t read_big_endian(const uint8_t *bytes, uint32_t width) {
  uint32_t value = bytes[0];

  if (width == 2u) {
    value = (value << 8) | (uint32_t)bytes[1];
  } else if (width == 4u) {
    value = (value << 24) | ((uint32_t)bytes[1] << 16) |
            ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
  }
  return value;
}

static bool rsp_imem_write_hits_active_pc(t64_machine *machine,
                                          uint32_t address, uint32_t width,
                                          uint8_t byte_mask) {
  uint32_t active_start;
  uint32_t index;

  if (machine == NULL || !t64_has_rsp_functional_profile(machine) ||
      (machine->rsp.sp_status & T64_RSP_STATUS_HALT) != 0u ||
      address < T64_SP_IMEM_BASE || address >= T64_SP_MEMORY_END) {
    return false;
  }
  active_start = T64_SP_IMEM_BASE |
                 (machine->rsp.pc & UINT32_C(0x00000ffc));
  for (index = 0u; index < width; index++) {
    uint32_t byte_address;

    if ((byte_mask & (uint8_t)(UINT8_C(1) << index)) == 0u) {
      continue;
    }
    byte_address = address + index;
    if (byte_address >= active_start && byte_address < active_start + 4u) {
      machine->rsp_observed_refusal_reason =
          T64_RSP_REFUSAL_ACTIVE_IMEM_WRITE;
      return true;
    }
  }
  return false;
}

static void write_big_endian(uint8_t *bytes, uint32_t width,
                             uint32_t value) {
  if (width == 1u) {
    bytes[0] = (uint8_t)value;
    return;
  }
  if (width == 2u) {
    bytes[0] = (uint8_t)(value >> 8);
    bytes[1] = (uint8_t)value;
    return;
  }
  bytes[0] = (uint8_t)(value >> 24);
  bytes[1] = (uint8_t)(value >> 16);
  bytes[2] = (uint8_t)(value >> 8);
  bytes[3] = (uint8_t)value;
}

static uint64_t read_big_endian_64(const uint8_t *bytes) {
  uint64_t value = 0u;
  uint32_t index;

  for (index = 0u; index < 8u; index++) {
    value = (value << 8) | (uint64_t)bytes[index];
  }
  return value;
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
  case T64_BUS_REGION_WARM:
  case T64_BUS_REGION_ISVIEWER:
  case T64_BUS_REGION_ABSENT_RDRAM_PROBE:
    break;
  }
  *remaining_out = 0u;
  return NULL;
}

/* Side-effect-free address classification for precise CPU preflight. This
 * reports only address-map/range faults; device-specific refusals remain the
 * owning bus operation's responsibility. */
t64_status t64_bus_probe_access(const t64_machine *machine,
                                uint32_t physical_address, uint32_t width) {
  t64_bus_region region;
  const uint8_t *bytes;
  size_t remaining = 0u;
  t64_status status;

  if (machine == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  status = validate_access(physical_address, width);
  if (status != T64_STATUS_OK) {
    return status;
  }
  region = decode_region(machine, physical_address);
  if (region == T64_BUS_REGION_UNMAPPED) {
    return T64_STATUS_BUS_UNMAPPED;
  }
  if (region == T64_BUS_REGION_UNSUPPORTED) {
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  bytes = region_bytes(machine, region, physical_address, &remaining);
  if (bytes != NULL) {
    return (size_t)width <= remaining ? T64_STATUS_OK : T64_STATUS_BUS_RANGE;
  }
  /* Mapped device windows are addressable even where the current profile has
   * no operation implementation. Do not turn that model limitation into a
   * guest bus exception. */
  return region == T64_BUS_REGION_MI || region == T64_BUS_REGION_PI ||
                 region == T64_BUS_REGION_SP_REG ||
                 region == T64_BUS_REGION_WARM ||
                 region == T64_BUS_REGION_ISVIEWER ||
                 region == T64_BUS_REGION_ABSENT_RDRAM_PROBE
             ? T64_STATUS_OK
             : T64_STATUS_BUS_UNMAPPED;
}

static void trace_bus(t64_machine *machine, uint32_t address, uint32_t width,
                      uint32_t value, bool is_write) {
  uint32_t kind;
  uint64_t record_value = ((uint64_t)address << 32) | (uint64_t)value;

  if (width == 1u) {
    kind = is_write ? T64_TRACE_KIND_BUS_WRITE8 : T64_TRACE_KIND_BUS_READ8;
  } else if (width == 2u) {
    kind = is_write ? T64_TRACE_KIND_BUS_WRITE16 : T64_TRACE_KIND_BUS_READ16;
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
  if (region == T64_BUS_REGION_ISVIEWER) {
    status = t64_isviewer_read(machine, address, width, &value);
    if (status != T64_STATUS_OK) {
      return status;
    }
    *value_out = value;
    trace_bus(machine, address, width, value, false);
    return T64_STATUS_OK;
  }
  if (region == T64_BUS_REGION_MI) {
    status = t64_warm_device_read(machine, address, width, &value);
    if (status == T64_STATUS_OK) {
      *value_out = value;
      trace_bus(machine, address, width, value, false);
      return T64_STATUS_OK;
    }
    if (status != T64_STATUS_BUS_UNSUPPORTED || width != 4u ||
        !t64_mi_register_read(machine, address, &value)) {
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
    if (!t64_has_rsp_functional_profile(machine)) {
      status = t64_warm_device_read(machine, address, width, &value);
      if (status == T64_STATUS_OK) {
        *value_out = value;
        trace_bus(machine, address, width, value, false);
        return T64_STATUS_OK;
      }
      if (status != T64_STATUS_BUS_UNSUPPORTED) {
        return status;
      }
    }
    status = t64_sp_read_register(machine, address, &value);
    if (status != T64_STATUS_OK) {
      return status;
    }
    *value_out = value;
    trace_bus(machine, address, width, value, false);
    return T64_STATUS_OK;
  }
  if (region == T64_BUS_REGION_WARM ||
      region == T64_BUS_REGION_ABSENT_RDRAM_PROBE) {
    status = t64_warm_device_read(machine, address, width, &value);
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

t64_status t64_bus_read_wide(t64_machine *machine, uint32_t address,
                             uint64_t *value_out) {
  t64_bus_region region;
  const uint8_t *bytes;
  size_t remaining = 0u;

  if (machine == NULL || value_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if ((address & UINT32_C(7)) != 0u) {
    return T64_STATUS_BUS_ALIGNMENT;
  }
  if ((uint64_t)address + UINT64_C(8) > UINT64_C(0x100000000)) {
    return T64_STATUS_BUS_RANGE;
  }
  region = decode_region(machine, address);
  if (region == T64_BUS_REGION_UNMAPPED) {
    return T64_STATUS_BUS_UNMAPPED;
  }
  if (region == T64_BUS_REGION_UNSUPPORTED) {
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  if (region == T64_BUS_REGION_MI || region == T64_BUS_REGION_PI ||
      region == T64_BUS_REGION_SP_REG || region == T64_BUS_REGION_WARM ||
      region == T64_BUS_REGION_ISVIEWER ||
      region == T64_BUS_REGION_ABSENT_RDRAM_PROBE) {
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  bytes = region_bytes(machine, region, address, &remaining);
  if (bytes == NULL) {
    return region == T64_BUS_REGION_CARTRIDGE
               ? T64_STATUS_BUS_RANGE
               : T64_STATUS_BUS_DEVICE_REFUSED;
  }
  if (remaining < 8u) {
    return T64_STATUS_BUS_RANGE;
  }
  *value_out = read_big_endian_64(bytes);
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
  if (region == T64_BUS_REGION_ISVIEWER) {
    status = t64_isviewer_write(machine, address, width, value);
    if (status == T64_STATUS_OK) {
      trace_bus(machine, address, width, value, true);
    }
    return status;
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
    status = t64_warm_device_write(machine, address, width, value);
    if (status != T64_STATUS_BUS_UNSUPPORTED) {
      if (status == T64_STATUS_OK) {
        trace_bus(machine, address, width, value, true);
      }
      return status;
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
    if (!t64_has_rsp_functional_profile(machine)) {
      status = t64_warm_device_write(machine, address, width, value);
      if (status != T64_STATUS_BUS_UNSUPPORTED) {
        if (status == T64_STATUS_OK) {
          trace_bus(machine, address, width, value, true);
        }
        return status;
      }
    }
    status = t64_sp_write_register(machine, address, value);
    if (status == T64_STATUS_OK) {
      trace_bus(machine, address, width, value, true);
    }
    return status;
  }
  if (region == T64_BUS_REGION_WARM ||
      region == T64_BUS_REGION_ABSENT_RDRAM_PROBE) {
    status = t64_warm_device_write(machine, address, width, value);
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
  case T64_BUS_REGION_WARM:
  case T64_BUS_REGION_ISVIEWER:
  case T64_BUS_REGION_ABSENT_RDRAM_PROBE:
    break;
  }
  if (bytes == NULL || (size_t)width > remaining) {
    return T64_STATUS_BUS_RANGE;
  }
  if (region == T64_BUS_REGION_SP_IMEM &&
      rsp_imem_write_hits_active_pc(
          machine, address, width,
          (uint8_t)((UINT32_C(1) << width) - UINT32_C(1)))) {
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  write_big_endian(bytes, width, value);
  if (region == T64_BUS_REGION_RDRAM ||
      region == T64_BUS_REGION_SP_DMEM ||
      region == T64_BUS_REGION_SP_IMEM) {
    t64_cpu_link_invalidate_range(machine, address, width);
  }
  trace_bus(machine, address, width, value, true);
  return T64_STATUS_OK;
}

t64_status t64_bus_write_masked(t64_machine *machine, uint32_t address,
                                uint32_t width, uint8_t byte_mask,
                                uint64_t value) {
  t64_bus_region region;
  uint8_t *bytes = NULL;
  size_t offset = 0u;
  size_t remaining = 0u;
  uint32_t index;
  uint32_t byte_count;
  uint32_t valid_mask;

  if (machine == NULL || (width != 4u && width != 8u)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  byte_count = width;
  valid_mask = (UINT32_C(1) << byte_count) - 1u;
  if (byte_mask == 0u || ((uint32_t)byte_mask & ~valid_mask) != 0u) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if ((address & (width - 1u)) != 0u) {
    return T64_STATUS_BUS_ALIGNMENT;
  }
  if ((uint64_t)address + width > UINT64_C(0x100000000)) {
    return T64_STATUS_BUS_RANGE;
  }
  if (width == 4u && byte_mask == UINT8_C(0x0f)) {
    return t64_bus_write(machine, address, 4u, (uint32_t)value);
  }

  region = decode_region(machine, address);
  if (region == T64_BUS_REGION_UNMAPPED) {
    return T64_STATUS_BUS_UNMAPPED;
  }
  if (region == T64_BUS_REGION_UNSUPPORTED) {
    return T64_STATUS_BUS_UNSUPPORTED;
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
  case T64_BUS_REGION_WARM:
  case T64_BUS_REGION_ISVIEWER:
  case T64_BUS_REGION_ABSENT_RDRAM_PROBE:
    break;
  }
  if (bytes == NULL) {
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  if ((size_t)width > remaining) {
    return T64_STATUS_BUS_RANGE;
  }
  if (region == T64_BUS_REGION_SP_IMEM &&
      rsp_imem_write_hits_active_pc(machine, address, width, byte_mask)) {
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  for (index = 0u; index < byte_count; index++) {
    if (((uint32_t)byte_mask & (UINT32_C(1) << index)) != 0u) {
      uint32_t shift = (byte_count - index - 1u) * 8u;
      bytes[index] = (uint8_t)(value >> shift);
      if (region == T64_BUS_REGION_RDRAM ||
          region == T64_BUS_REGION_SP_DMEM ||
          region == T64_BUS_REGION_SP_IMEM) {
        t64_cpu_link_invalidate_range(machine, address + index, 1u);
      }
    }
  }
  if (width == 4u) {
    trace_bus(machine, address, 4u, (uint32_t)value, true);
  } else {
    trace_bus(machine, address, 4u, (uint32_t)(value >> 32), true);
    trace_bus(machine, address + 4u, 4u, (uint32_t)value, true);
  }
  return T64_STATUS_OK;
}

static t64_status cache_writeback_destination(
    t64_machine *machine, uint32_t address, uint32_t line_bytes,
    uint8_t **bytes_out) {
  const uint64_t end = (uint64_t)address + line_bytes;
  t64_bus_region region;
  uint32_t offset;
  uint8_t *bytes = NULL;

  if (machine == NULL || bytes_out == NULL ||
      (line_bytes != T64_VR4300_DCACHE_LINE_BYTES &&
       line_bytes != T64_VR4300_ICACHE_LINE_BYTES)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if ((address & (line_bytes - UINT32_C(1))) != 0u) {
    return T64_STATUS_BUS_ALIGNMENT;
  }
  if (end > UINT64_C(0x100000000)) {
    return T64_STATUS_BUS_RANGE;
  }
  region = decode_region(machine, address);
  switch (region) {
  case T64_BUS_REGION_RDRAM:
    if (machine->ram == NULL || end > machine->profile.rdram_bytes) {
      return T64_STATUS_BUS_RANGE;
    }
    bytes = machine->ram + (size_t)address;
    break;
  case T64_BUS_REGION_SP_DMEM:
    if (address < T64_SP_DMEM_BASE ||
        end > (uint64_t)T64_SP_IMEM_BASE) {
      return T64_STATUS_BUS_RANGE;
    }
    bytes = machine->sp_dmem + (size_t)(address - T64_SP_DMEM_BASE);
    break;
  case T64_BUS_REGION_SP_IMEM:
    if (address < T64_SP_IMEM_BASE || end > (uint64_t)T64_SP_MEMORY_END) {
      return T64_STATUS_BUS_RANGE;
    }
    bytes = machine->sp_imem + (size_t)(address - T64_SP_IMEM_BASE);
    break;
  case T64_BUS_REGION_CARTRIDGE:
    return T64_STATUS_BUS_DEVICE_REFUSED;
  case T64_BUS_REGION_UNMAPPED:
    return T64_STATUS_BUS_UNMAPPED;
  case T64_BUS_REGION_UNSUPPORTED:
    return T64_STATUS_BUS_UNSUPPORTED;
  case T64_BUS_REGION_MI:
  case T64_BUS_REGION_PI:
  case T64_BUS_REGION_SP_REG:
  case T64_BUS_REGION_WARM:
  case T64_BUS_REGION_ISVIEWER:
  case T64_BUS_REGION_ABSENT_RDRAM_PROBE:
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  for (offset = 1u; offset < line_bytes; offset++) {
    if (decode_region(machine, address + offset) != region) {
      return T64_STATUS_BUS_RANGE;
    }
  }
  *bytes_out = bytes;
  return T64_STATUS_OK;
}

t64_status t64_bus_validate_cache_writeback(t64_machine *machine,
                                            uint32_t physical_address,
                                            uint32_t line_bytes) {
  uint8_t *ignored = NULL;

  return cache_writeback_destination(machine, physical_address, line_bytes,
                                     &ignored);
}

t64_status t64_bus_writeback_cache_line(t64_machine *machine,
                                        uint32_t physical_address,
                                        const uint8_t *data,
                                        uint32_t line_bytes) {
  uint8_t *destination = NULL;
  uint32_t offset;
  t64_status status;

  if (data == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  status = cache_writeback_destination(machine, physical_address, line_bytes,
                                       &destination);
  if (status != T64_STATUS_OK) {
    return status;
  }
  (void)memcpy(destination, data, line_bytes);
  /* Cache writeback exposes already committed CPU data. Reservation effects
   * belong to the original store or explicit D-cache invalidation, not to an
   * unrelated backing write caused by line maintenance. */
  for (offset = 0u; offset < line_bytes; offset += 4u) {
    trace_bus(machine, physical_address + offset, 4u,
              read_big_endian(data + offset, 4u), true);
  }
  return T64_STATUS_OK;
}

t64_status t64_bus_validate_dcache_line_writeback(
    t64_machine *machine, uint32_t physical_address) {
  return t64_bus_validate_cache_writeback(
      machine, physical_address, T64_VR4300_DCACHE_LINE_BYTES);
}

t64_status t64_bus_writeback_dcache_line(
    t64_machine *machine, uint32_t physical_address,
    const uint8_t data[T64_VR4300_DCACHE_LINE_BYTES]) {
  return t64_bus_writeback_cache_line(
      machine, physical_address, data, T64_VR4300_DCACHE_LINE_BYTES);
}

t64_status t64_machine_bus_read(t64_machine *machine, uint32_t address,
                                uint32_t width, uint32_t *value_out) {
  return t64_bus_read(machine, address, width, value_out);
}

t64_status t64_machine_bus_write(t64_machine *machine, uint32_t address,
                                 uint32_t width, uint32_t value) {
  return t64_bus_write(machine, address, width, value);
}
