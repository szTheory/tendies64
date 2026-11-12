#include "t64_internal.h"

#include <string.h>

static bool access_is_aligned(uint32_t address, uint32_t width) {
  if (width == 1u) {
    return true;
  }
  if (width == 2u) {
    return (address & UINT32_C(1)) == 0u;
  }
  return width == 4u && (address & UINT32_C(3)) == 0u;
}

static bool width_is_supported(uint32_t width) {
  return width == 1u || width == 2u || width == 4u;
}

bool t64_isviewer_active(const t64_machine *machine) {
  static const char warm_profile_id[] = "ntsc-cart-4m-openipl3-warm-v1";
  static const char cpu_profile_id[] = "ntsc-cart-4m-openipl3-vr4300-v1";
  bool warm_profile;
  bool cpu_profile;

  if (machine == NULL ||
      machine->output.policy_version != T64_ISVIEWER_POLICY_VERSION) {
    return false;
  }
  warm_profile = machine->profile.descriptor_version == 2u &&
                 machine->profile.state_serialization_version == 8u &&
                 memcmp(machine->profile.profile_id, warm_profile_id,
                        sizeof warm_profile_id) == 0;
  cpu_profile = machine->profile.descriptor_version == 3u &&
                machine->profile.state_serialization_version == 9u &&
                memcmp(machine->profile.profile_id, cpu_profile_id,
                       sizeof cpu_profile_id) == 0;
  return warm_profile || cpu_profile;
}

static t64_status validate_isviewer_access(const t64_machine *machine,
                                           uint32_t address, uint32_t width) {
  uint32_t offset;

  if (!t64_isviewer_active(machine) || !width_is_supported(width)) {
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  if (!access_is_aligned(address, width)) {
    return T64_STATUS_BUS_ALIGNMENT;
  }
  if (address < T64_ISVIEWER_BASE) {
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  offset = address - T64_ISVIEWER_BASE;
  if (offset >= T64_ISVIEWER_DEVICE_BYTES ||
      width > T64_ISVIEWER_DEVICE_BYTES - offset) {
    return T64_STATUS_BUS_RANGE;
  }
  return T64_STATUS_OK;
}

static uint32_t read_be(const uint8_t *bytes, uint32_t width) {
  uint32_t value = 0u;
  uint32_t i;

  for (i = 0u; i < width; i++) {
    value = (value << 8) | bytes[i];
  }
  return value;
}

static void write_be(uint8_t *bytes, uint32_t width, uint32_t value) {
  uint32_t i;

  for (i = 0u; i < width; i++) {
    uint32_t shift = (width - i - 1u) * 8u;
    bytes[i] = (uint8_t)(value >> shift);
  }
}

static t64_status read_count_lane(const t64_output_state *output,
                                  uint32_t offset, uint32_t width,
                                  uint32_t *value_out) {
  uint32_t lane_offset = offset - T64_ISVIEWER_COUNT_OFFSET;
  uint32_t shift = (4u - lane_offset - width) * 8u;
  uint32_t mask = width == 4u ? UINT32_MAX
                              : (UINT32_C(1) << (width * 8u)) - 1u;

  *value_out = (output->byte_count >> shift) & mask;
  return T64_STATUS_OK;
}

t64_status t64_isviewer_read(const t64_machine *machine, uint32_t address,
                             uint32_t width, uint32_t *value_out) {
  uint32_t offset;
  uint32_t value;
  t64_status status;

  if (machine == NULL || value_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  status = validate_isviewer_access(machine, address, width);
  if (status != T64_STATUS_OK) {
    return status;
  }
  offset = address - T64_ISVIEWER_BASE;
  if (offset == T64_ISVIEWER_MAGIC_OFFSET && width == 4u) {
    value = machine->output.magic_register;
  } else if (offset >= T64_ISVIEWER_COUNT_OFFSET &&
             offset + width <= T64_ISVIEWER_COUNT_OFFSET + 4u) {
    (void)read_count_lane(&machine->output, offset, width, &value);
  } else if (offset >= T64_ISVIEWER_BUFFER_OFFSET &&
             offset + width <=
                 T64_ISVIEWER_BUFFER_OFFSET + T64_ISVIEWER_BUFFER_BYTES) {
    value = read_be(machine->output.buffer +
                        (offset - T64_ISVIEWER_BUFFER_OFFSET),
                    width);
  } else {
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  *value_out = value;
  return T64_STATUS_OK;
}

static t64_status flush_bytes(t64_output_state *output, uint32_t count,
                              uint32_t next_register_value) {
  if (count == 0u) {
    output->byte_count = next_register_value;
    return T64_STATUS_OK;
  }
  if (output->total_attempted_bytes_saturated ||
      UINT64_MAX - output->total_attempted_bytes < (uint64_t)count) {
    output->total_attempted_bytes = UINT64_MAX;
    output->total_attempted_bytes_saturated = true;
    output->lost_output = true;
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  output->total_attempted_bytes += (uint64_t)count;
  if (output->lost_output) {
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  if (count > T64_ISVIEWER_BUFFER_BYTES ||
      output->captured_bytes > T64_ISVIEWER_CAPTURE_BYTES ||
      count > T64_ISVIEWER_CAPTURE_BYTES - output->captured_bytes) {
    output->lost_output = true;
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }

  (void)memcpy(output->captured + output->captured_bytes, output->buffer,
               count);
  output->captured_bytes += count;
  output->byte_count = next_register_value;
  return T64_STATUS_OK;
}

t64_status t64_isviewer_write(t64_machine *machine, uint32_t address,
                              uint32_t width, uint32_t value) {
  uint32_t offset;
  t64_output_state *output;
  t64_status status;

  if (machine == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  status = validate_isviewer_access(machine, address, width);
  if (status != T64_STATUS_OK) {
    return status;
  }
  offset = address - T64_ISVIEWER_BASE;
  output = &machine->output;
  if (offset == T64_ISVIEWER_MAGIC_OFFSET && width == 4u) {
    output->magic_register = value;
    return T64_STATUS_OK;
  }
  if (offset == T64_ISVIEWER_COUNT_OFFSET && width == 4u) {
    return flush_bytes(output, value, value);
  }
  if (offset == T64_ISVIEWER_COUNT_OFFSET && width == 2u) {
    output->byte_count = (output->byte_count & UINT32_C(0x0000ffff)) |
                         ((value & UINT32_C(0x0000ffff)) << 16);
    return T64_STATUS_OK;
  }
  if (offset == T64_ISVIEWER_COUNT_OFFSET + 2u && width == 2u) {
    uint32_t count = value & UINT32_C(0x0000ffff);
    uint32_t next_register_value =
        (output->byte_count & UINT32_C(0xffff0000)) | count;

    return flush_bytes(output, count, next_register_value);
  }
  if (offset >= T64_ISVIEWER_BUFFER_OFFSET &&
      offset + width <=
          T64_ISVIEWER_BUFFER_OFFSET + T64_ISVIEWER_BUFFER_BYTES) {
    write_be(output->buffer + (offset - T64_ISVIEWER_BUFFER_OFFSET), width,
             value);
    return T64_STATUS_OK;
  }
  return T64_STATUS_BUS_UNSUPPORTED;
}
