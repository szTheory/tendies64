#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define T64_VI_BASE UINT32_C(0x04400000)
#define T64_VI_END (T64_VI_BASE + UINT32_C(0x38))
#define T64_VI_REG_COUNT 14u
#define T64_VI_CONTROL 0u
#define T64_VI_ORIGIN 1u
#define T64_VI_WIDTH 2u
#define T64_VI_CURRENT 4u
#define T64_VI_BURST 5u
#define T64_VI_V_SYNC 6u
#define T64_VI_H_SYNC 7u
#define T64_VI_H_START 9u
#define T64_VI_V_START 10u
#define T64_VI_X_SCALE 12u
#define T64_VI_Y_SCALE 13u
#define T64_VI_SOURCE_CLOCK UINT64_C(48681812)
#define T64_VI_GUEST_CLOCK UINT64_C(62500000)
#define T64_MI_INTERRUPT_VI UINT32_C(0x08)

typedef struct {
  uint32_t width;
  uint32_t height;
  uint32_t source_width;
  uint32_t source_x_offset;
  uint32_t source_y_offset;
  uint32_t x_scale;
  uint32_t y_scale;
  uint32_t h_start;
  uint32_t h_end;
  uint32_t v_start;
  uint32_t v_end;
  uint32_t period_cycles;
  uint32_t source_bytes_per_pixel;
  uint64_t source_bytes;
} t64_vi_geometry;

static void copy_text(char *out, size_t capacity, const char *text) {
  size_t i = 0u;

  if (capacity == 0u) return;
  while (i + 1u < capacity && text[i] != '\0') {
    out[i] = text[i];
    i++;
  }
  out[i] = '\0';
}

static bool geometry_for(const t64_machine *machine, uint32_t control,
                         t64_vi_geometry *geometry_out) {
  const t64_vi_state *vi;
  uint32_t h_register;
  uint32_t v_register;
  uint32_t x_register;
  uint32_t y_register;
  uint64_t active_width;
  uint64_t active_height;
  uint64_t width;
  uint64_t height;
  uint64_t last_x;
  uint64_t last_y;
  uint64_t pixels;
  uint64_t source_end;

  if (machine == NULL || geometry_out == NULL || !machine->has_image ||
      !t64_has_rdp_vi_profile(machine)) {
    return false;
  }
  vi = &machine->vi;
  if (control == UINT32_C(2)) {
    geometry_out->source_bytes_per_pixel = 2u;
  } else if (control == UINT32_C(3) && t64_has_rdp_blender_profile(machine)) {
    geometry_out->source_bytes_per_pixel = 4u;
  } else {
    return false;
  }
  h_register = vi->registers[T64_VI_H_START];
  v_register = vi->registers[T64_VI_V_START];
  x_register = vi->registers[T64_VI_X_SCALE];
  y_register = vi->registers[T64_VI_Y_SCALE];
  if ((h_register & UINT32_C(0xfc00fc00)) != 0u ||
      (v_register & UINT32_C(0xfc00fc00)) != 0u ||
      (x_register & UINT32_C(0xf000f000)) != 0u ||
      (y_register & UINT32_C(0xf000f000)) != 0u ||
      (vi->registers[T64_VI_ORIGIN] & UINT32_C(0xfe000000)) != 0u ||
      (vi->registers[T64_VI_ORIGIN] &
       (geometry_out->source_bytes_per_pixel == 4u ? UINT32_C(3)
                                                  : UINT32_C(7))) != 0u) {
    return false;
  }
  geometry_out->source_width = vi->registers[T64_VI_WIDTH];
  geometry_out->h_start = (h_register >> 16) & UINT32_C(0x3ff);
  geometry_out->h_end = h_register & UINT32_C(0x3ff);
  geometry_out->v_start = (v_register >> 16) & UINT32_C(0x3ff);
  geometry_out->v_end = v_register & UINT32_C(0x3ff);
  geometry_out->x_scale = x_register & UINT32_C(0x0fff);
  geometry_out->y_scale = y_register & UINT32_C(0x0fff);
  geometry_out->source_x_offset = (x_register >> 16) & UINT32_C(0x0fff);
  geometry_out->source_y_offset = (y_register >> 16) & UINT32_C(0x0fff);
  active_width = geometry_out->h_end - geometry_out->h_start;
  active_height = geometry_out->v_end - geometry_out->v_start;
  if (geometry_out->h_end <= geometry_out->h_start ||
      geometry_out->v_end <= geometry_out->v_start ||
      geometry_out->source_width == 0u ||
      geometry_out->source_width > 1024u || geometry_out->x_scale == 0u ||
      geometry_out->y_scale == 0u) {
    return false;
  }
  width = (active_width * geometry_out->x_scale) >> 10;
  height = (active_height * geometry_out->y_scale) >> 10;
  if (width == 0u || height == 0u || width > T64_VIDEO_MAX_WIDTH ||
      height > T64_VIDEO_MAX_HEIGHT) {
    return false;
  }
  geometry_out->width = (uint32_t)width;
  geometry_out->height = (uint32_t)height;
  last_x = geometry_out->source_x_offset +
           (((width - 1u) * UINT64_C(1024)) / geometry_out->x_scale);
  last_y = geometry_out->source_y_offset +
           (((height - 1u) * UINT64_C(1024)) / geometry_out->y_scale);
  if (last_x >= geometry_out->source_width || last_y >= T64_VIDEO_MAX_HEIGHT) {
    return false;
  }
  pixels = last_y * geometry_out->source_width + last_x + 1u;
  if (pixels > UINT64_MAX / geometry_out->source_bytes_per_pixel)
    return false;
  geometry_out->source_bytes =
      pixels * geometry_out->source_bytes_per_pixel;
  source_end = (uint64_t)vi->registers[T64_VI_ORIGIN] +
               geometry_out->source_bytes;
  if (source_end > machine->profile.rdram_bytes) return false;
  if (vi->registers[T64_VI_H_SYNC] > UINT32_C(0x0fff) ||
      vi->registers[T64_VI_V_SYNC] > UINT32_C(0x03ff)) {
    return false;
  }
  geometry_out->period_cycles =
      (vi->registers[T64_VI_H_SYNC] + 1u) *
      (vi->registers[T64_VI_V_SYNC] + 1u);
  return geometry_out->period_cycles != 0u;
}

static bool delay_for_cycles(uint32_t cycles, uint64_t remainder,
                             uint64_t *delay_out) {
  uint64_t scaled = (uint64_t)cycles * T64_VI_GUEST_CLOCK;
  uint64_t needed;

  if (delay_out == NULL || remainder >= T64_VI_GUEST_CLOCK ||
      scaled <= remainder) {
    return false;
  }
  needed = scaled - remainder;
  *delay_out = needed / T64_VI_SOURCE_CLOCK;
  if (needed % T64_VI_SOURCE_CLOCK != 0u) (*delay_out)++;
  if (*delay_out == 0u) *delay_out = 1u;
  return true;
}

static bool phase_at_tick(const t64_vi_state *vi, uint64_t tick,
                          uint32_t period_cycles, uint64_t *cycles_out,
                          uint64_t *remainder_out) {
  uint64_t elapsed;
  uint64_t scaled;
  uint64_t whole_cycles;
  uint64_t remainder;

  if (tick < vi->last_guest_tick || cycles_out == NULL ||
      remainder_out == NULL) {
    return false;
  }
  elapsed = tick - vi->last_guest_tick;
  if (elapsed > (UINT64_MAX - vi->phase_remainder) / T64_VI_SOURCE_CLOCK)
    return false;
  scaled = elapsed * T64_VI_SOURCE_CLOCK + vi->phase_remainder;
  whole_cycles = scaled / T64_VI_GUEST_CLOCK;
  remainder = scaled % T64_VI_GUEST_CLOCK;
  if (whole_cycles > UINT64_MAX - vi->phase_cycles) return false;
  whole_cycles += vi->phase_cycles;
  if (whole_cycles < period_cycles) return false;
  whole_cycles -= period_cycles;
  if (whole_cycles >= period_cycles) return false;
  *cycles_out = whole_cycles;
  *remainder_out = remainder;
  return true;
}

static uint8_t expand5(uint32_t value) {
  return (uint8_t)((value << 3) | (value >> 2));
}

static void render_packet(t64_machine *machine, uint32_t slot,
                          const t64_vi_geometry *geometry,
                          t64_video_packet_info *info) {
  t64_vi_state *vi = &machine->vi;
  uint8_t *output = machine->video_packets +
                    (size_t)slot * T64_VIDEO_MAX_PACKET_BYTES;
  uint32_t origin = vi->registers[T64_VI_ORIGIN];
  uint32_t y;

  for (y = 0u; y < geometry->height; y++) {
    uint32_t source_y = geometry->source_y_offset +
                        (uint32_t)(((uint64_t)y * UINT64_C(1024)) /
                                   geometry->y_scale);
    uint32_t x;
    for (x = 0u; x < geometry->width; x++) {
      uint32_t source_x = geometry->source_x_offset +
                          (uint32_t)(((uint64_t)x * UINT64_C(1024)) /
                                     geometry->x_scale);
      uint64_t source_index =
          (uint64_t)source_y * geometry->source_width + source_x;
      uint64_t source_address = (uint64_t)origin +
                                source_index * geometry->source_bytes_per_pixel;
      size_t output_index = (size_t)y * info->stride_bytes + (size_t)x * 4u;
      if (geometry->source_bytes_per_pixel == 4u) {
        uint8_t alpha5 = (uint8_t)(machine->ram[source_address + 3u] & 31u);
        output[output_index] = machine->ram[source_address];
        output[output_index + 1u] = machine->ram[source_address + 1u];
        output[output_index + 2u] = machine->ram[source_address + 2u];
        output[output_index + 3u] =
            (uint8_t)((alpha5 << 3) | (alpha5 >> 2));
      } else {
        uint16_t pixel =
            (uint16_t)(((uint16_t)machine->ram[source_address] << 8) |
                       (uint16_t)machine->ram[source_address + 1u]);
        output[output_index] =
            expand5((uint32_t)(pixel >> 11) & UINT32_C(0x1f));
        output[output_index + 1u] =
            expand5((uint32_t)(pixel >> 6) & UINT32_C(0x1f));
        output[output_index + 2u] =
            expand5((uint32_t)(pixel >> 1) & UINT32_C(0x1f));
        output[output_index + 3u] =
            (pixel & UINT16_C(1)) != 0u ? 255u : 0u;
      }
    }
  }
}

void t64_vi_reset_machine_state(t64_machine *machine) {
  if (machine == NULL) return;
  (void)memset(&machine->vi, 0, sizeof machine->vi);
  if (machine->video_packets != NULL) {
    (void)memset(machine->video_packets, 0,
                 T64_VIDEO_QUEUE_STORAGE_BYTES);
  }
}

t64_status t64_vi_read_register(const t64_machine *machine,
                                uint32_t address, uint32_t *value_out) {
  uint32_t offset;
  uint32_t index;
  uint32_t value;

  if (machine == NULL || value_out == NULL ||
      !t64_has_rdp_vi_profile(machine)) {
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  if ((address & UINT32_C(3)) != 0u) return T64_STATUS_BUS_ALIGNMENT;
  if (address < T64_VI_BASE || address >= T64_VI_END)
    return T64_STATUS_BUS_UNSUPPORTED;
  offset = address - T64_VI_BASE;
  index = offset / UINT32_C(4);
  if (index >= T64_VI_REG_COUNT) return T64_STATUS_BUS_UNSUPPORTED;
  value = machine->vi.registers[index];
  if (index == T64_VI_CURRENT) {
    uint32_t position = (uint32_t)(machine->vi.phase_cycles & UINT64_C(0x3ff));
    value = (position << 1) | (machine->vi.field & UINT32_C(1));
  }
  *value_out = value;
  return T64_STATUS_OK;
}

t64_status t64_vi_write_register(t64_machine *machine, uint32_t address,
                                 uint32_t value) {
  t64_vi_state *vi;
  uint32_t index;
  t64_status status;

  if (machine == NULL || !t64_has_rdp_vi_profile(machine))
    return T64_STATUS_BUS_UNSUPPORTED;
  if ((address & UINT32_C(3)) != 0u) return T64_STATUS_BUS_ALIGNMENT;
  if (address < T64_VI_BASE || address >= T64_VI_END)
    return T64_STATUS_BUS_UNSUPPORTED;
  index = (address - T64_VI_BASE) / UINT32_C(4);
  if (index >= T64_VI_REG_COUNT) return T64_STATUS_BUS_UNSUPPORTED;
  vi = &machine->vi;
  if (index == T64_VI_CONTROL) {
    uint64_t delay;
    uint64_t generation;
    t64_vi_geometry geometry;

    if ((value & UINT32_C(0xfffffffc)) != 0u)
      return T64_STATUS_BUS_DEVICE_REFUSED;
    if ((value & UINT32_C(3)) == 0u) {
      if (vi->event_generation == UINT64_MAX)
        return T64_STATUS_RESOURCE_EXHAUSTED;
      vi->event_generation++;
      vi->registers[index] = 0u;
      vi->enabled = false;
      return T64_STATUS_OK;
    }
    if (vi->enabled || !geometry_for(machine, value, &geometry) ||
        vi->event_generation == UINT64_MAX) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
    if (!delay_for_cycles(geometry.period_cycles, 0u, &delay))
      return T64_STATUS_RESOURCE_EXHAUSTED;
    generation = vi->event_generation + UINT64_C(1);
    {
      t64_event_reservation reservation;
      status = t64_schedule_reserve_after(
          machine, delay, T64_VI_DEVICE, T64_VI_EVENT_FIELD, 0u, generation,
          t64_vi_event, &reservation);
      if (status != T64_STATUS_OK) return status;
      status = t64_schedule_commit(machine, &reservation);
      if (status != T64_STATUS_OK) return status;
    }
    vi->registers[index] = value;
    vi->enabled = true;
    vi->event_generation = generation;
    vi->field = 0u;
    vi->phase_cycles = 0u;
    vi->phase_remainder = 0u;
    vi->last_guest_tick = machine->tick;
    vi->refusal_reason = T64_VI_REFUSAL_NONE;
    return T64_STATUS_OK;
  }
  if (index == T64_VI_CURRENT) {
    vi->registers[index] = value;
    t64_mi_clear_pending(machine, T64_MI_INTERRUPT_VI);
    return T64_STATUS_OK;
  }
  if (vi->enabled) return T64_STATUS_BUS_DEVICE_REFUSED;
  switch (index) {
  case T64_VI_ORIGIN:
    if ((value & UINT32_C(0xfe000007)) != 0u)
      return T64_STATUS_BUS_DEVICE_REFUSED;
    break;
  case T64_VI_WIDTH:
    if (value == 0u || value > 1024u) return T64_STATUS_BUS_DEVICE_REFUSED;
    break;
  case T64_VI_V_SYNC:
    if (value > UINT32_C(0x03ff)) return T64_STATUS_BUS_DEVICE_REFUSED;
    break;
  case T64_VI_H_SYNC:
    if (value > UINT32_C(0x0fff)) return T64_STATUS_BUS_DEVICE_REFUSED;
    break;
  case T64_VI_H_START:
  case T64_VI_V_START:
    if ((value & UINT32_C(0xfc00fc00)) != 0u)
      return T64_STATUS_BUS_DEVICE_REFUSED;
    break;
  case T64_VI_X_SCALE:
  case T64_VI_Y_SCALE:
    if ((value & UINT32_C(0xf000f000)) != 0u)
      return T64_STATUS_BUS_DEVICE_REFUSED;
    break;
  default:
    break;
  }
  vi->registers[index] = value;
  return T64_STATUS_OK;
}

t64_status t64_vi_event(t64_machine *machine, const t64_event *event) {
  t64_vi_state *vi;
  t64_vi_geometry geometry;
  t64_video_packet_info info;
  t64_event_reservation reservation;
  uint64_t phase_cycles;
  uint64_t phase_remainder;
  uint64_t delay = 0u;
  uint32_t slot;
  uint32_t remaining_cycles;
  t64_status status;

  if (machine == NULL || event == NULL || event->device != T64_VI_DEVICE ||
      event->kind != T64_VI_EVENT_FIELD || !t64_has_rdp_vi_profile(machine)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  vi = &machine->vi;
  if (!vi->enabled || event->token != vi->event_generation)
    return T64_STATUS_OK;
  if (vi->packet_count >= T64_VIDEO_QUEUE_CAPACITY)
    return T64_STATUS_QUEUE_FULL;
  if (vi->sequence_exhausted || vi->field == UINT32_MAX) {
    vi->refusal_reason = T64_VI_REFUSAL_SEQUENCE_EXHAUSTED;
    vi->enabled = false;
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }
  if (!geometry_for(machine, vi->registers[T64_VI_CONTROL], &geometry)) {
    vi->refusal_reason = T64_VI_REFUSAL_GEOMETRY;
    vi->enabled = false;
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  if (!phase_at_tick(vi, event->tick, geometry.period_cycles, &phase_cycles,
                     &phase_remainder)) {
    vi->refusal_reason = T64_VI_REFUSAL_PHASE_OVERFLOW;
    vi->enabled = false;
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }
  remaining_cycles = geometry.period_cycles - (uint32_t)phase_cycles;
  if (vi->next_sequence != UINT64_MAX) {
    if (!delay_for_cycles(remaining_cycles, phase_remainder, &delay)) {
      vi->refusal_reason = T64_VI_REFUSAL_PHASE_OVERFLOW;
      vi->enabled = false;
      return T64_STATUS_RESOURCE_EXHAUSTED;
    }
    status = t64_schedule_reserve_after(
        machine, delay, T64_VI_DEVICE, T64_VI_EVENT_FIELD, 0u,
        vi->event_generation, t64_vi_event, &reservation);
    if (status != T64_STATUS_OK) return status;
    status = t64_schedule_commit(machine, &reservation);
    if (status != T64_STATUS_OK) return status;
  }
  (void)memset(&info, 0, sizeof info);
  info.width = geometry.width;
  info.height = geometry.height;
  info.stride_bytes = geometry.width * UINT32_C(4);
  info.source_address = vi->registers[T64_VI_ORIGIN];
  info.source_format = geometry.source_bytes_per_pixel == 4u
                           ? T64_VIDEO_SOURCE_RGBA8888
                           : T64_VIDEO_SOURCE_RGBA5551;
  info.output_encoding = T64_VIDEO_ENCODING_RGBA8;
  info.field = vi->field;
  info.parity = vi->field & UINT32_C(1);
  info.control = vi->registers[T64_VI_CONTROL];
  info.burst = vi->registers[T64_VI_BURST];
  info.h_sync = vi->registers[T64_VI_H_SYNC];
  info.v_sync = vi->registers[T64_VI_V_SYNC];
  info.h_start = vi->registers[T64_VI_H_START];
  info.v_start = vi->registers[T64_VI_V_START];
  info.x_scale = vi->registers[T64_VI_X_SCALE];
  info.y_scale = vi->registers[T64_VI_Y_SCALE];
  info.start_tick = vi->last_guest_tick;
  info.end_tick = event->tick;
  info.sequence = vi->next_sequence;
  info.commands_completed = machine->rdp.command_count;
  info.fullsync_completed = machine->rdp.fullsync_count;
  info.last_command_word = machine->rdp.last_original_word;
  info.last_command_byte = machine->rdp.last_command_byte;
  info.last_opcode_field = machine->rdp.last_opcode_field;
  info.source_bytes = (size_t)geometry.source_bytes;
  info.packet_bytes = (size_t)geometry.width * (size_t)geometry.height * 4u;
  copy_text(info.profile_id, sizeof info.profile_id, machine->profile.profile_id);
  copy_text(info.build_id, sizeof info.build_id, t64_build_identity());
  copy_text(info.vi_clock_policy, sizeof info.vi_clock_policy,
            machine->profile.vi_clock_policy);
  slot = (uint32_t)((vi->packet_head + vi->packet_count) %
                    T64_VIDEO_QUEUE_CAPACITY);
  render_packet(machine, slot, &geometry, &info);
  vi->packet_lengths[slot] = info.packet_bytes;
  vi->packet_info[slot] = info;
  vi->packet_count++;
  vi->phase_cycles = phase_cycles;
  vi->phase_remainder = phase_remainder;
  vi->last_guest_tick = event->tick;
  vi->field++;
  vi->refusal_reason = T64_VI_REFUSAL_NONE;
  if (vi->next_sequence == UINT64_MAX) {
    vi->sequence_exhausted = true;
  } else {
    vi->next_sequence++;
  }
  t64_trace_emit(machine, machine->tick, T64_TRACE_DEVICE_VI,
                 T64_TRACE_KIND_VI_FRAME, info.sequence);
  return T64_STATUS_OK;
}

t64_status t64_video_info(const t64_machine *machine,
                          t64_video_packet_info *info_out) {
  t64_address_range output_range;

  if (machine == NULL || info_out == NULL ||
      !t64_internal_make_range(info_out, sizeof *info_out, &output_range) ||
      !t64_internal_observation_range_is_disjoint(machine, output_range)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!machine->has_image || !t64_has_rdp_vi_profile(machine))
    return T64_STATUS_NOT_READY;
  if (machine->vi.packet_count == 0u) return T64_STATUS_NOT_READY;
  *info_out = machine->vi.packet_info[machine->vi.packet_head];
  return T64_STATUS_OK;
}

t64_status t64_video_copy(t64_machine *machine, uint8_t *bytes_out,
                          size_t capacity, size_t *length_out) {
  t64_address_range length_range;
  t64_address_range bytes_range;
  size_t required;
  uint32_t slot;

  if (machine == NULL || length_out == NULL ||
      !t64_internal_make_range(length_out, sizeof *length_out, &length_range) ||
      !t64_internal_observation_range_is_disjoint(machine, length_range)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (bytes_out == NULL) {
    if (capacity != 0u) return T64_STATUS_INVALID_ARGUMENT;
  } else if (!t64_internal_make_range(bytes_out, capacity, &bytes_range) ||
             !t64_internal_observation_range_is_disjoint(machine, bytes_range) ||
             t64_internal_ranges_overlap(bytes_range, length_range)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!machine->has_image || !t64_has_rdp_vi_profile(machine))
    return T64_STATUS_NOT_READY;
  if (machine->vi.packet_count == 0u) return T64_STATUS_NOT_READY;
  slot = (uint32_t)machine->vi.packet_head;
  required = machine->vi.packet_lengths[slot];
  if (capacity < required) {
    *length_out = required;
    return T64_STATUS_BUFFER_TOO_SMALL;
  }
  if (bytes_out == NULL && required != 0u) return T64_STATUS_INVALID_ARGUMENT;
  (void)memcpy(bytes_out,
               machine->video_packets +
                   (size_t)slot * T64_VIDEO_MAX_PACKET_BYTES,
               required);
  *length_out = required;
  machine->vi.packet_head =
      (machine->vi.packet_head + 1u) % T64_VIDEO_QUEUE_CAPACITY;
  machine->vi.packet_count--;
  return T64_STATUS_OK;
}
