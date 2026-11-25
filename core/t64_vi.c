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
#define T64_VI_INTR 3u
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
  uint32_t source_pitch_bytes;
  uint32_t source_x_offset;
  uint32_t source_y_offset;
  uint32_t x_scale;
  uint32_t y_scale;
  uint32_t h_start;
  uint32_t h_end;
  uint32_t v_start;
  uint32_t v_end;
  uint32_t period_cycles;
  uint64_t period_units;
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
  const uint32_t *registers;
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
  registers = t64_has_vi_scanout_profile(machine) && vi->enabled
                  ? vi->active_registers
                  : vi->registers;
  if ((control & UINT32_C(3)) == UINT32_C(2)) {
    geometry_out->source_bytes_per_pixel = 2u;
  } else if ((control & UINT32_C(3)) == UINT32_C(3) &&
             t64_has_rdp_blender_profile(machine)) {
    geometry_out->source_bytes_per_pixel = 4u;
  } else {
    return false;
  }
  h_register = registers[T64_VI_H_START];
  v_register = registers[T64_VI_V_START];
  x_register = registers[T64_VI_X_SCALE];
  y_register = registers[T64_VI_Y_SCALE];
  if (t64_has_vi_scanout_profile(machine)) {
    uint64_t span;
    uint64_t source_index;
    uint64_t source_width;
    uint64_t source_height;
    uint64_t scale_x = x_register & UINT32_C(0x0fff);
    uint64_t scale_y = y_register & UINT32_C(0x0fff);
    uint64_t offset_x = (x_register >> 16) & UINT32_C(0x0fff);
    uint64_t offset_y = (y_register >> 16) & UINT32_C(0x0fff);

    geometry_out->source_width = registers[T64_VI_WIDTH] & UINT32_C(0x0fff);
    geometry_out->h_start = (h_register >> 16) & UINT32_C(0x03ff);
    geometry_out->h_end = h_register & UINT32_C(0x03ff);
    geometry_out->v_start = (v_register >> 16) & UINT32_C(0x03ff);
    geometry_out->v_end = v_register & UINT32_C(0x03ff);
    geometry_out->x_scale = (uint32_t)scale_x;
    geometry_out->y_scale = (uint32_t)scale_y;
    geometry_out->source_x_offset = (uint32_t)offset_x;
    geometry_out->source_y_offset = (uint32_t)offset_y;
    if (geometry_out->h_end <= geometry_out->h_start ||
        geometry_out->v_end <= geometry_out->v_start ||
        geometry_out->source_width == 0u || scale_x == 0u || scale_y == 0u) {
      return false;
    }
    span = (uint64_t)geometry_out->h_end - geometry_out->h_start;
    width = (span * UINT64_C(1024)) / scale_x;
    span = ((uint64_t)geometry_out->v_end - geometry_out->v_start) / 2u;
    height = (span * UINT64_C(1024)) / scale_y;
    if (width == 0u || height == 0u || width > T64_VIDEO_MAX_WIDTH ||
        height > T64_VIDEO_MAX_HEIGHT) {
      return false;
    }
    geometry_out->width = (uint32_t)width;
    geometry_out->height = (uint32_t)height;
    source_width = (offset_x + (width - 1u) * scale_x) >> 10;
    source_height = (offset_y + (height - 1u) * scale_y) >> 10;
    if ((registers[T64_VI_CONTROL] & UINT32_C(0x40)) != 0u) {
      source_height = source_height * 2u + (vi->field & UINT32_C(1));
    }
    if (source_width >= geometry_out->source_width ||
        geometry_out->source_width >
            UINT32_MAX / geometry_out->source_bytes_per_pixel) {
      return false;
    }
    geometry_out->source_pitch_bytes =
        geometry_out->source_width * geometry_out->source_bytes_per_pixel;
    if (source_height > UINT64_MAX / geometry_out->source_width) return false;
    source_index = source_height * geometry_out->source_width + source_width + 1u;
    if (source_index > UINT64_MAX / geometry_out->source_bytes_per_pixel)
      return false;
    geometry_out->source_bytes =
        source_index * geometry_out->source_bytes_per_pixel;
    source_end = (uint64_t)(registers[T64_VI_ORIGIN] & UINT32_C(0x00ffffff)) +
                 geometry_out->source_bytes;
    if (source_end > machine->profile.rdram_bytes) return false;
    geometry_out->period_cycles = 0u;
    geometry_out->period_units =
        (uint64_t)((registers[T64_VI_H_SYNC] & UINT32_C(0x0fff)) + 1u) *
        (registers[T64_VI_V_SYNC] & UINT32_C(0x03ff));
    return true;
  }
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
  geometry_out->source_pitch_bytes =
      geometry_out->source_width * geometry_out->source_bytes_per_pixel;
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
  geometry_out->period_units = geometry_out->period_cycles;
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

typedef struct {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t alpha;
  uint8_t coverage;
} t64_vi_sample;

typedef struct {
  size_t source_bytes;
  uint32_t selected_filters;
} t64_vi_filter_context;

static bool vi_read_source_pixel(
    const t64_machine *machine, const t64_vi_geometry *geometry,
    uint32_t origin, uint32_t source_x, uint64_t source_y,
    const t64_vi_sample *fallback, t64_vi_filter_context *context,
    t64_vi_sample *sample_out) {
  uint64_t source_index;
  uint64_t source_address;
  uint64_t source_end;
  uint64_t consumed;
  uint32_t bytes_per_pixel;

  if (sample_out == NULL || machine == NULL || geometry == NULL ||
      context == NULL || geometry->source_width == 0u) {
    return false;
  }
  if (fallback != NULL) {
    *sample_out = *fallback;
  } else {
    (void)memset(sample_out, 0, sizeof *sample_out);
  }
  if (source_x >= geometry->source_width)
    source_x = geometry->source_width - 1u;
  bytes_per_pixel = geometry->source_bytes_per_pixel;
  if (bytes_per_pixel == 0u ||
      source_y > (UINT64_MAX - source_x) / geometry->source_width) {
    return false;
  }
  source_index = source_y * geometry->source_width + source_x;
  if (source_index > (UINT64_MAX - origin) / bytes_per_pixel) return false;
  source_address = (uint64_t)origin + source_index * bytes_per_pixel;
  if (source_address > UINT64_MAX - bytes_per_pixel) return false;
  source_end = source_address + bytes_per_pixel;
  if (source_end > machine->profile.rdram_bytes) return false;

  if (bytes_per_pixel == 4u) {
    const uint8_t alpha5 =
        (uint8_t)(machine->ram[source_address + 3u] & UINT8_C(31));
    sample_out->red = machine->ram[source_address];
    sample_out->green = machine->ram[source_address + 1u];
    sample_out->blue = machine->ram[source_address + 2u];
    sample_out->alpha = expand5(alpha5);
    sample_out->coverage = t64_rdp_memory_coverage_read(
        machine, (uint32_t)source_address, bytes_per_pixel);
  } else {
    const uint16_t pixel =
        (uint16_t)(((uint16_t)machine->ram[source_address] << 8) |
                   machine->ram[source_address + 1u]);
    sample_out->red = expand5((uint32_t)(pixel >> 11) & UINT32_C(31));
    sample_out->green = expand5((uint32_t)(pixel >> 6) & UINT32_C(31));
    sample_out->blue = expand5((uint32_t)(pixel >> 1) & UINT32_C(31));
    sample_out->alpha = (pixel & UINT16_C(1)) != 0u ? 255u : 0u;
    sample_out->coverage = t64_rdp_memory_coverage_read(
        machine, (uint32_t)source_address, bytes_per_pixel);
  }
  consumed = (source_address - origin) + bytes_per_pixel;
  if (consumed <= SIZE_MAX && (size_t)consumed > context->source_bytes)
    context->source_bytes = (size_t)consumed;
  return true;
}

static uint32_t vi_manhattan_distance(const t64_vi_sample *left,
                                     const t64_vi_sample *right) {
  const uint32_t red = left->red > right->red
                           ? (uint32_t)left->red - right->red
                           : (uint32_t)right->red - left->red;
  const uint32_t green = left->green > right->green
                             ? (uint32_t)left->green - right->green
                             : (uint32_t)right->green - left->green;
  const uint32_t blue = left->blue > right->blue
                            ? (uint32_t)left->blue - right->blue
                            : (uint32_t)right->blue - left->blue;
  return red + green + blue;
}

static bool vi_filter_source_pixel(
    const t64_machine *machine, const t64_vi_geometry *geometry,
    const uint32_t *registers, uint32_t origin, uint32_t source_x,
    uint64_t source_y, uint32_t parity, t64_vi_filter_context *context,
    t64_vi_sample *sample_out) {
  t64_vi_sample center = {0u, 0u, 0u, 0u, 0u};
  uint32_t aa_mode = (registers[T64_VI_CONTROL] >> 8) & UINT32_C(3);
  bool has_aa_background = false;
  uint32_t best_distance = 0u;
  t64_vi_sample background = {0u, 0u, 0u, 0u, 0u};
  bool serrate = (registers[T64_VI_CONTROL] & UINT32_C(0x40)) != 0u;
  uint32_t row_step = serrate ? 2u : 1u;

  if (!vi_read_source_pixel(machine, geometry, origin, source_x, source_y,
                            NULL, context, &center)) {
    *sample_out = center;
    return false;
  }
  *sample_out = center;
  if (aa_mode < 2u && center.coverage < 8u) {
    int32_t dy;
    const uint64_t first_row = serrate ? (uint64_t)(parity & UINT32_C(1)) : 0u;
    for (dy = -1; dy <= 1; dy++) {
      int32_t dx;
      int64_t candidate_y = (int64_t)source_y +
                            (int64_t)dy * (int64_t)row_step;
      if (candidate_y < (int64_t)first_row)
        candidate_y = (int64_t)first_row;
      for (dx = -2; dx <= 2; dx++) {
        const int64_t candidate_x = (int64_t)source_x + (int64_t)dx;
        uint32_t clamped_x;
        t64_vi_sample candidate = center;
        uint32_t distance;
        if (candidate_x < 0) {
          clamped_x = 0u;
        } else if ((uint64_t)candidate_x >= geometry->source_width) {
          clamped_x = geometry->source_width - 1u;
        } else {
          clamped_x = (uint32_t)candidate_x;
        }
        (void)vi_read_source_pixel(machine, geometry, origin, clamped_x,
                                   (uint64_t)candidate_y, &center, context,
                                   &candidate);
        if (candidate.coverage != 8u) continue;
        distance = vi_manhattan_distance(&center, &candidate);
        if (!has_aa_background || distance > best_distance) {
          background = candidate;
          best_distance = distance;
          has_aa_background = true;
        }
      }
    }
    if (has_aa_background) {
      const uint32_t foreground_weight = center.coverage;
      const uint32_t background_weight = 8u - foreground_weight;
      sample_out->red = (uint8_t)((foreground_weight * center.red +
                                   background_weight * background.red) /
                                  8u);
      sample_out->green = (uint8_t)((foreground_weight * center.green +
                                     background_weight * background.green) /
                                    8u);
      sample_out->blue = (uint8_t)((foreground_weight * center.blue +
                                    background_weight * background.blue) /
                                   8u);
    }
    context->selected_filters |= T64_VIDEO_FILTER_AA;
  } else if ((registers[T64_VI_CONTROL] & UINT32_C(0x00010000)) != 0u &&
             geometry->source_bytes_per_pixel == 2u &&
             (aa_mode == 2u || center.coverage == 8u)) {
    uint32_t red_sum = 0u;
    uint32_t green_sum = 0u;
    uint32_t blue_sum = 0u;
    int32_t dy;
    const uint64_t first_row = serrate ? (uint64_t)(parity & UINT32_C(1)) : 0u;
    for (dy = -1; dy <= 1; dy++) {
      const int64_t candidate_y = (int64_t)source_y +
                                  (int64_t)dy * (int64_t)row_step;
      int32_t dx;
      const uint64_t clamped_y = candidate_y < (int64_t)first_row
                                     ? first_row
                                     : (uint64_t)candidate_y;
      for (dx = -1; dx <= 1; dx++) {
        const int64_t candidate_x = (int64_t)source_x + (int64_t)dx;
        uint32_t clamped_x;
        t64_vi_sample candidate = center;
        if (candidate_x < 0) {
          clamped_x = 0u;
        } else if ((uint64_t)candidate_x >= geometry->source_width) {
          clamped_x = geometry->source_width - 1u;
        } else {
          clamped_x = (uint32_t)candidate_x;
        }
        (void)vi_read_source_pixel(machine, geometry, origin, clamped_x,
                                   clamped_y, &center, context, &candidate);
        red_sum += candidate.red;
        green_sum += candidate.green;
        blue_sum += candidate.blue;
      }
    }
    sample_out->red = (uint8_t)(red_sum / 9u);
    sample_out->green = (uint8_t)(green_sum / 9u);
    sample_out->blue = (uint8_t)(blue_sum / 9u);
    context->selected_filters |= T64_VIDEO_FILTER_DITHER_SMOOTH;
  }
  return true;
}

static uint8_t vi_median_channel(uint8_t first, uint8_t center,
                                 uint8_t last) {
  uint8_t low = first < center ? first : center;
  uint8_t high = first > center ? first : center;
  if (last < low) return low;
  if (last > high) return high;
  return last;
}

static bool vi_divot_sample(const t64_machine *machine,
                            const t64_vi_geometry *geometry,
                            const uint32_t *registers, uint32_t origin,
                            uint32_t source_x, uint64_t source_y,
                            uint32_t parity,
                            t64_vi_filter_context *context,
                            t64_vi_sample *sample_out) {
  t64_vi_sample center;
  uint32_t aa_mode = (registers[T64_VI_CONTROL] >> 8) & UINT32_C(3);
  if (!vi_filter_source_pixel(machine, geometry, registers, origin, source_x,
                              source_y, parity, context, &center)) {
    *sample_out = center;
    return false;
  }
  if ((registers[T64_VI_CONTROL] & UINT32_C(0x10)) != 0u && aa_mode < 2u &&
      center.coverage < 8u) {
    const uint32_t left_x = source_x == 0u ? 0u : source_x - 1u;
    const uint32_t right_x = source_x + 1u >= geometry->source_width
                                 ? geometry->source_width - 1u
                                 : source_x + 1u;
    t64_vi_sample left;
    t64_vi_sample right;
    (void)vi_filter_source_pixel(machine, geometry, registers, origin, left_x,
                                 source_y, parity, context, &left);
    (void)vi_filter_source_pixel(machine, geometry, registers, origin, right_x,
                                 source_y, parity, context, &right);
    center.red = vi_median_channel(left.red, center.red, right.red);
    center.green = vi_median_channel(left.green, center.green, right.green);
    center.blue = vi_median_channel(left.blue, center.blue, right.blue);
    context->selected_filters |= T64_VIDEO_FILTER_DIVOT_MEDIAN;
  }
  *sample_out = center;
  return true;
}

static uint8_t vi_gamma_channel(uint8_t channel) {
  const uint32_t value = UINT32_C(255) * channel;
  uint32_t low = 0u;
  uint32_t high = 256u;
  while (high - low > 1u) {
    const uint32_t middle = (low + high) / 2u;
    if (middle * middle <= value)
      low = middle;
    else
      high = middle;
  }
  return (uint8_t)low;
}

static uint8_t vi_gamma_adjust(uint8_t channel, bool round_up) {
  if (round_up) return channel == UINT8_MAX ? UINT8_MAX : (uint8_t)(channel + 1u);
  return channel == 0u ? 0u : (uint8_t)(channel - 1u);
}

static void render_filter_packet(t64_machine *machine, uint32_t slot,
                                 const t64_vi_geometry *geometry,
                                 t64_video_packet_info *info) {
  static const uint8_t gamma_dither[4][4] = {
      {0u, 8u, 2u, 10u}, {12u, 4u, 14u, 6u},
      {3u, 11u, 1u, 9u}, {15u, 7u, 13u, 5u},
  };
  t64_vi_state *vi = &machine->vi;
  const uint32_t *registers = vi->active_registers;
  const uint32_t origin = registers[T64_VI_ORIGIN] & UINT32_C(0x00ffffff);
  const uint32_t control = registers[T64_VI_CONTROL];
  const uint32_t aa_mode = (control >> 8) & UINT32_C(3);
  const bool serrate = (control & UINT32_C(0x40)) != 0u;
  uint8_t *output = machine->video_packets +
                    (size_t)slot * T64_VIDEO_MAX_PACKET_BYTES;
  t64_vi_filter_context context = {0u, 0u};
  uint32_t y;

  for (y = 0u; y < geometry->height; y++) {
    uint32_t x;
    for (x = 0u; x < geometry->width; x++) {
      const uint64_t x_coordinate =
          (uint64_t)geometry->source_x_offset + (uint64_t)x * geometry->x_scale;
      const uint64_t y_coordinate =
          (uint64_t)geometry->source_y_offset + (uint64_t)y * geometry->y_scale;
      const uint32_t source_x0 = (uint32_t)(x_coordinate >> 10);
      const uint32_t fraction_x = (uint32_t)(x_coordinate & UINT64_C(1023));
      const uint64_t source_y_index = y_coordinate >> 10;
      const uint32_t fraction_y = (uint32_t)(y_coordinate & UINT64_C(1023));
      const uint64_t source_y0 = serrate
                                     ? source_y_index * 2u + (vi->field & 1u)
                                     : source_y_index;
      const uint64_t source_y1 = serrate ? source_y0 + 2u : source_y0 + 1u;
      const uint32_t source_x1 = source_x0 + 1u >= geometry->source_width
                                     ? geometry->source_width - 1u
                                     : source_x0 + 1u;
      const uint32_t nearest_x = source_x0 >= geometry->source_width
                                     ? geometry->source_width - 1u
                                     : source_x0;
      t64_vi_sample center = {0u, 0u, 0u, 0u, 0u};
      t64_vi_sample sampled = {0u, 0u, 0u, 0u, 0u};
      const size_t output_index =
          (size_t)y * info->stride_bytes + (size_t)x * 4u;
      uint32_t channel_values[3];
      uint32_t channel;

      (void)vi_read_source_pixel(machine, geometry, origin, nearest_x,
                                 source_y0, NULL, &context, &center);
      if (aa_mode == 3u) {
        (void)vi_divot_sample(machine, geometry, registers, origin, nearest_x,
                              source_y0, vi->field & UINT32_C(1), &context,
                              &sampled);
      } else {
        const uint32_t inverse_x = 1024u - fraction_x;
        const uint32_t inverse_y = 1024u - fraction_y;
        const uint32_t weights[4] = {
            inverse_x * inverse_y, fraction_x * inverse_y,
            inverse_x * fraction_y, fraction_x * fraction_y,
        };
        const uint32_t tap_x[4] = {nearest_x, source_x1, nearest_x, source_x1};
        const uint64_t tap_y[4] = {source_y0, source_y0, source_y1, source_y1};
        uint32_t sums[3] = {0u, 0u, 0u};
        size_t tap;
        context.selected_filters |= T64_VIDEO_FILTER_RESAMPLE_Q20;
        for (tap = 0u; tap < 4u; tap++) {
          t64_vi_sample sample;
          if (weights[tap] == 0u) continue;
          if (!vi_divot_sample(machine, geometry, registers, origin,
                               tap_x[tap], tap_y[tap],
                               vi->field & UINT32_C(1), &context, &sample)) {
            sample = center;
          }
          sums[0] += (uint32_t)sample.red * weights[tap];
          sums[1] += (uint32_t)sample.green * weights[tap];
          sums[2] += (uint32_t)sample.blue * weights[tap];
        }
        sampled.red = (uint8_t)(sums[0] >> 20);
        sampled.green = (uint8_t)(sums[1] >> 20);
        sampled.blue = (uint8_t)(sums[2] >> 20);
        sampled.alpha = center.alpha;
        sampled.coverage = center.coverage;
      }
      if ((control & UINT32_C(8)) != 0u) {
        if ((control & UINT32_C(4)) != 0u) {
          const bool round_up =
              gamma_dither[y & UINT32_C(3)][x & UINT32_C(3)] >= 8u;
          sampled.red = vi_gamma_adjust(sampled.red, round_up);
          sampled.green = vi_gamma_adjust(sampled.green, round_up);
          sampled.blue = vi_gamma_adjust(sampled.blue, round_up);
          context.selected_filters |= T64_VIDEO_FILTER_GAMMA_DITHER;
        }
        sampled.red = vi_gamma_channel(sampled.red);
        sampled.green = vi_gamma_channel(sampled.green);
        sampled.blue = vi_gamma_channel(sampled.blue);
        context.selected_filters |= T64_VIDEO_FILTER_GAMMA_SQRT255;
      }
      channel_values[0] = sampled.red;
      channel_values[1] = sampled.green;
      channel_values[2] = sampled.blue;
      for (channel = 0u; channel < 3u; channel++)
        output[output_index + channel] = (uint8_t)channel_values[channel];
      output[output_index + 3u] = center.alpha;
    }
  }
  info->filter = context.selected_filters;
  info->source_bytes = context.source_bytes;
}

static void render_packet(t64_machine *machine, uint32_t slot,
                          const t64_vi_geometry *geometry,
                          t64_video_packet_info *info) {
  if (t64_has_vi_filter_v9_profile(machine)) {
    render_filter_packet(machine, slot, geometry, info);
    return;
  }
  t64_vi_state *vi = &machine->vi;
  const uint32_t *registers = t64_has_vi_scanout_profile(machine)
                                  ? vi->active_registers
                                  : vi->registers;
  uint8_t *output = machine->video_packets +
                    (size_t)slot * T64_VIDEO_MAX_PACKET_BYTES;
  uint32_t origin = registers[T64_VI_ORIGIN] & UINT32_C(0x00ffffff);
  uint32_t y;

  for (y = 0u; y < geometry->height; y++) {
    uint32_t source_y;
    if (t64_has_vi_scanout_profile(machine)) {
      source_y = (geometry->source_y_offset + y * geometry->y_scale) >> 10;
      if ((registers[T64_VI_CONTROL] & UINT32_C(0x40)) != 0u)
        source_y = source_y * 2u + (vi->field & UINT32_C(1));
    } else {
      source_y = geometry->source_y_offset +
                 (uint32_t)(((uint64_t)y * UINT64_C(1024)) /
                            geometry->y_scale);
    }
    uint32_t x;
    for (x = 0u; x < geometry->width; x++) {
      uint32_t source_x = t64_has_vi_scanout_profile(machine)
                              ? (geometry->source_x_offset +
                                 x * geometry->x_scale) >> 10
                              : geometry->source_x_offset +
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
    if (t64_has_vi_scanout_profile(machine)) {
      value = machine->vi.current_readback;
    } else {
      uint32_t position =
          (uint32_t)(machine->vi.phase_cycles & UINT64_C(0x3ff));
      value = (position << 1) | (machine->vi.field & UINT32_C(1));
    }
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
  if (t64_has_vi_scanout_profile(machine)) {
    if (index == T64_VI_CURRENT) {
      vi->registers[index] = value;
      t64_mi_clear_pending(machine, T64_MI_INTERRUPT_VI);
      return T64_STATUS_OK;
    }
    if (index == T64_VI_INTR) {
      vi->registers[index] = value;
      vi->active_registers[index] = value;
      return T64_STATUS_OK;
    }
    if (index == T64_VI_CONTROL) {
      uint32_t mode = value & UINT32_C(3);
      uint32_t prior_control = vi->registers[index];
      uint64_t generation;
      uint64_t delay = 0u;
      t64_vi_geometry geometry;

      if (mode == 1u) return T64_STATUS_BUS_DEVICE_REFUSED;
      if (vi->enabled) {
        vi->registers[index] = value;
        vi->latch_pending = true;
        return T64_STATUS_OK;
      }
      if (mode == 0u) {
        if (vi->event_generation == UINT64_MAX)
          return T64_STATUS_RESOURCE_EXHAUSTED;
        vi->event_generation++;
        vi->registers[index] = value;
        vi->active_registers[index] = value;
        vi->latch_pending = false;
        return T64_STATUS_OK;
      }
      vi->registers[index] = value;
      if (!geometry_for(machine, value, &geometry)) {
        vi->registers[index] = prior_control;
        return T64_STATUS_BUS_DEVICE_REFUSED;
      }
      if (vi->event_generation == UINT64_MAX) {
        vi->registers[index] = prior_control;
        return T64_STATUS_RESOURCE_EXHAUSTED;
      }
      if (geometry.period_units != 0u) {
        uint64_t halfline_units =
            (uint64_t)((vi->registers[T64_VI_H_SYNC] & UINT32_C(0x0fff)) +
                       1u);
        uint64_t scaled = halfline_units * T64_VI_GUEST_CLOCK;
        uint64_t denominator = UINT64_C(2) * T64_VI_SOURCE_CLOCK;
        delay = scaled / denominator;
        if (scaled % denominator != 0u) delay++;
        if (delay == 0u) delay = 1u;
        generation = vi->event_generation + UINT64_C(1);
        {
          t64_event_reservation reservation;
          status = t64_schedule_reserve_after(
              machine, delay, T64_VI_DEVICE, T64_VI_EVENT_FIELD, 0u,
              generation, t64_vi_event, &reservation);
          if (status != T64_STATUS_OK) {
            vi->registers[index] = prior_control;
            return status;
          }
          status = t64_schedule_commit(machine, &reservation);
          if (status != T64_STATUS_OK) {
            vi->registers[index] = prior_control;
            return status;
          }
        }
      }
      vi->active_registers[index] = value;
      (void)memcpy(vi->active_registers, vi->registers,
                   sizeof vi->active_registers);
      vi->enabled = geometry.period_units != 0u;
      vi->event_generation++;
      vi->field = 0u;
      vi->current_readback = 0u;
      vi->phase_units = 0u;
      vi->phase_cycles = 0u;
      vi->phase_remainder = 0u;
      vi->next_boundary_units =
          (uint64_t)((vi->active_registers[T64_VI_H_SYNC] &
                      UINT32_C(0x0fff)) + 1u);
      vi->last_guest_tick = machine->tick;
      vi->field_start_tick = machine->tick;
      vi->latch_pending = false;
      vi->refusal_reason = T64_VI_REFUSAL_NONE;
      return T64_STATUS_OK;
    }
    vi->registers[index] = value;
    if (vi->enabled) {
      vi->latch_pending = true;
    } else {
      vi->active_registers[index] = value;
    }
    return T64_STATUS_OK;
  }
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

static t64_status vi_v8_event(t64_machine *machine, const t64_event *event) {
  const uint64_t numerator = UINT64_C(2) * T64_VI_SOURCE_CLOCK;
  const uint64_t denominator = T64_VI_GUEST_CLOCK;
  t64_vi_state *vi = &machine->vi;
  const uint32_t *active = vi->active_registers;
  uint64_t hline =
      (uint64_t)((active[T64_VI_H_SYNC] & UINT32_C(0x0fff)) + 1u);
  uint64_t period = hline *
                    (active[T64_VI_V_SYNC] & UINT32_C(0x03ff));
  uint64_t elapsed;
  uint64_t quotient;
  uint64_t tick_remainder;
  uint64_t scaled;
  uint64_t tail;
  uint64_t added_units;
  uint64_t phase;
  uint64_t remainder;
  uint64_t boundary = vi->next_boundary_units;
  uint64_t delay = 0u;
  uint32_t boundary_index;
  uint32_t current_value = vi->current_readback;
  uint32_t field_parity =
      (active[T64_VI_CONTROL] & UINT32_C(0x40)) != 0u
          ? (vi->field & UINT32_C(1))
          : 0u;
  bool line_sample;
  bool field_boundary;
  bool set_interrupt = false;
  t64_vi_geometry geometry;
  t64_event_reservation reservation;
  t64_status status;

  if (period == 0u || hline == 0u || boundary == 0u || boundary > period ||
      event->tick < vi->last_guest_tick ||
      vi->phase_remainder >= denominator) {
    vi->refusal_reason = T64_VI_REFUSAL_PHASE_OVERFLOW;
    vi->enabled = false;
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }
  elapsed = event->tick - vi->last_guest_tick;
  quotient = elapsed / denominator;
  tick_remainder = elapsed % denominator;
  if (quotient > UINT64_MAX / numerator) {
    vi->refusal_reason = T64_VI_REFUSAL_PHASE_OVERFLOW;
    vi->enabled = false;
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }
  scaled = quotient * numerator;
  tail = tick_remainder * numerator + vi->phase_remainder;
  if (scaled > UINT64_MAX - tail / denominator) {
    vi->refusal_reason = T64_VI_REFUSAL_PHASE_OVERFLOW;
    vi->enabled = false;
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }
  added_units = scaled + tail / denominator;
  if (vi->phase_units > UINT64_MAX - added_units) {
    vi->refusal_reason = T64_VI_REFUSAL_PHASE_OVERFLOW;
    vi->enabled = false;
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }
  phase = vi->phase_units + added_units;
  remainder = tail % denominator;
  if (phase < boundary) {
    vi->refusal_reason = T64_VI_REFUSAL_PHASE_OVERFLOW;
    vi->enabled = false;
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }
  field_boundary = boundary == period;
  if (field_boundary && vi->packet_count >= T64_VIDEO_QUEUE_CAPACITY)
    return T64_STATUS_QUEUE_FULL;
  if (field_boundary && vi->field == UINT32_MAX) {
    vi->refusal_reason = T64_VI_REFUSAL_SEQUENCE_EXHAUSTED;
    vi->enabled = false;
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }
  if (field_boundary && vi->sequence_exhausted) {
    vi->refusal_reason = T64_VI_REFUSAL_SEQUENCE_EXHAUSTED;
    vi->enabled = false;
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }

  boundary_index = (uint32_t)(boundary / hline);
  line_sample = (boundary_index & UINT32_C(1)) == 0u;
  if (line_sample) {
    current_value = (boundary_index & UINT32_C(0x03fe)) |
                    field_parity;
    set_interrupt =
        current_value == (active[T64_VI_INTR] & UINT32_C(0x03ff));
  }
  if (field_boundary) {
    if (!geometry_for(machine, active[T64_VI_CONTROL], &geometry)) {
      vi->refusal_reason = T64_VI_REFUSAL_GEOMETRY;
      vi->enabled = false;
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
  }

  vi->phase_units = phase;
  vi->phase_remainder = remainder;
  vi->last_guest_tick = event->tick;
  if (line_sample) {
    vi->current_readback = current_value;
    if (set_interrupt) t64_mi_set_pending(machine, T64_MI_INTERRUPT_VI);
  }
  if (field_boundary) {
    t64_video_packet_info info;
    uint32_t slot;
    (void)memset(&info, 0, sizeof info);
    info.width = geometry.width;
    info.height = geometry.height;
    info.stride_bytes = geometry.width * UINT32_C(4);
    info.source_address = active[T64_VI_ORIGIN] & UINT32_C(0x00ffffff);
    info.source_format = geometry.source_bytes_per_pixel == 4u
                             ? T64_VIDEO_SOURCE_RGBA8888
                             : T64_VIDEO_SOURCE_RGBA5551;
    info.output_encoding = T64_VIDEO_ENCODING_RGBA8;
    info.field = vi->field;
    info.parity = field_parity;
    info.control = active[T64_VI_CONTROL];
    info.burst = active[T64_VI_BURST];
    info.h_sync = active[T64_VI_H_SYNC];
    info.v_sync = active[T64_VI_V_SYNC];
    info.h_start = geometry.h_start;
    info.h_end = geometry.h_end;
    info.v_start = geometry.v_start;
    info.v_end = geometry.v_end;
    info.source_width = geometry.source_width;
    info.source_pitch_bytes = geometry.source_pitch_bytes;
    info.source_x_offset = geometry.source_x_offset;
    info.source_y_offset = geometry.source_y_offset;
    info.x_scale = geometry.x_scale;
    info.y_scale = geometry.y_scale;
    info.filter = 0u;
    info.start_tick = vi->field_start_tick;
    info.end_tick = event->tick;
    info.sequence = vi->next_sequence;
    info.commands_completed = machine->rdp.command_count;
    info.fullsync_completed = machine->rdp.fullsync_count;
    info.last_command_word = machine->rdp.last_original_word;
    info.last_command_byte = machine->rdp.last_command_byte;
    info.last_opcode_field = machine->rdp.last_opcode_field;
    info.source_bytes = (size_t)geometry.source_bytes;
    info.packet_bytes = (size_t)geometry.width *
                        (size_t)geometry.height * 4u;
    copy_text(info.profile_id, sizeof info.profile_id,
              machine->profile.profile_id);
    copy_text(info.build_id, sizeof info.build_id, t64_build_identity());
    copy_text(info.vi_clock_policy, sizeof info.vi_clock_policy,
              machine->profile.vi_clock_policy);
    copy_text(info.vi_output_policy, sizeof info.vi_output_policy,
              machine->profile.vi_output_policy);
    slot = (uint32_t)((vi->packet_head + vi->packet_count) %
                      T64_VIDEO_QUEUE_CAPACITY);
    render_packet(machine, slot, &geometry, &info);
    vi->packet_lengths[slot] = info.packet_bytes;
    vi->packet_info[slot] = info;
    vi->packet_count++;
    if (vi->next_sequence == UINT64_MAX) {
      vi->sequence_exhausted = true;
    } else {
      vi->next_sequence++;
    }
    t64_trace_emit(machine, machine->tick, T64_TRACE_DEVICE_VI,
                   T64_TRACE_KIND_VI_FRAME, info.sequence);

    vi->phase_units -= period;
    vi->field++;
    vi->field_start_tick = event->tick;
    if (vi->latch_pending) {
      (void)memcpy(vi->active_registers, vi->registers,
                   sizeof vi->active_registers);
      active = vi->active_registers;
    }
    vi->current_readback =
        (active[T64_VI_CONTROL] & UINT32_C(0x40)) != 0u
            ? (vi->field & UINT32_C(1))
            : 0u;
    hline = (uint64_t)((active[T64_VI_H_SYNC] & UINT32_C(0x0fff)) + 1u);
    period = hline * (active[T64_VI_V_SYNC] & UINT32_C(0x03ff));
    vi->next_boundary_units = hline;
    if (vi->latch_pending) {
      vi->latch_pending = false;
      if ((active[T64_VI_CONTROL] & UINT32_C(3)) == 0u) {
        if (vi->event_generation == UINT64_MAX) {
          vi->refusal_reason = T64_VI_REFUSAL_SEQUENCE_EXHAUSTED;
          vi->enabled = false;
          return T64_STATUS_RESOURCE_EXHAUSTED;
        }
        vi->event_generation++;
        vi->enabled = false;
        vi->refusal_reason = T64_VI_REFUSAL_NONE;
        return T64_STATUS_OK;
      }
      if (!geometry_for(machine, active[T64_VI_CONTROL], &geometry)) {
        vi->refusal_reason = T64_VI_REFUSAL_GEOMETRY;
        vi->enabled = false;
        return T64_STATUS_OK;
      }
    }
    if ((active[T64_VI_CONTROL] & UINT32_C(3)) < UINT32_C(2) ||
        period == 0u) {
      vi->enabled = false;
      vi->event_generation++;
      vi->refusal_reason = T64_VI_REFUSAL_NONE;
      return T64_STATUS_OK;
    }
    boundary = vi->next_boundary_units;
  } else {
    vi->next_boundary_units = boundary + hline;
    boundary = vi->next_boundary_units;
  }

  if (vi->sequence_exhausted || vi->next_sequence == UINT64_MAX) {
    vi->sequence_exhausted = true;
    vi->enabled = false;
    return T64_STATUS_OK;
  }
  if (vi->phase_units < boundary) {
    uint64_t needed = (boundary - vi->phase_units) * denominator;
    if (needed < vi->phase_remainder) {
      vi->refusal_reason = T64_VI_REFUSAL_PHASE_OVERFLOW;
      vi->enabled = false;
      return T64_STATUS_RESOURCE_EXHAUSTED;
    }
    needed -= vi->phase_remainder;
    delay = needed / numerator;
    if (needed % numerator != 0u) delay++;
    if (delay == 0u) delay = 1u;
  }
  status = t64_schedule_reserve_after(
      machine, delay, T64_VI_DEVICE, T64_VI_EVENT_FIELD, 0u,
      vi->event_generation, t64_vi_event, &reservation);
  if (status == T64_STATUS_OK)
    status = t64_schedule_commit(machine, &reservation);
  if (status != T64_STATUS_OK) {
    vi->refusal_reason = T64_VI_REFUSAL_PHASE_OVERFLOW;
    vi->enabled = false;
    return status;
  }
  vi->refusal_reason = T64_VI_REFUSAL_NONE;
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
  if (t64_has_vi_scanout_profile(machine))
    return vi_v8_event(machine, event);
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

  T64_ENTRY_GUARD(machine);
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

  T64_ENTRY_GUARD(machine);
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
