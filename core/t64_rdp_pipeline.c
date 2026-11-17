#include "t64_internal.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

static uint8_t color_channel(uint32_t color, unsigned channel) {
  return (uint8_t)(color >> (24u - channel * 8u));
}

static int32_t signed9(uint32_t value) {
  int32_t result = (int32_t)(value & UINT32_C(0x1ff));
  if ((result & INT32_C(0x100)) != 0) result -= INT32_C(0x200);
  return result;
}

static uint8_t clamp_u8(int64_t value) {
  if (value < 0) return 0u;
  if (value > UINT8_MAX) return UINT8_MAX;
  return (uint8_t)value;
}

static int64_t floor_div_255(int64_t value) {
  int64_t quotient = value / INT64_C(255);
  if (value % INT64_C(255) < 0) quotient--;
  return quotient;
}

static uint8_t next_noise(t64_rdp_state *rdp) {
  uint32_t value = rdp->random_state;
  if (value == 0u) value = 1u;
  value ^= value << 13;
  value ^= value >> 17;
  value ^= value << 5;
  rdp->random_state = value;
  return (uint8_t)value;
}

static uint8_t rgb_source(const t64_machine *machine,
                          const t64_rdp_combiner_inputs *inputs,
                          const uint8_t combined[4], unsigned slot,
                          unsigned code, unsigned channel, uint8_t noise) {
  const t64_rdp_state *rdp = &machine->rdp;
  switch (slot) {
  case 0u: /* RGB A */
    switch (code) {
    case 0u: return combined[channel];
    case 1u: return inputs->texel0[channel];
    case 2u: return inputs->texel1[channel];
    case 3u: return color_channel(rdp->primitive_color, channel);
    case 4u: return inputs->shade[channel];
    case 5u: return color_channel(rdp->environment_color, channel);
    case 6u: return UINT8_MAX;
    case 7u: return noise;
    default: return 0u;
    }
  case 1u: /* RGB B */
    switch (code) {
    case 0u: return combined[channel];
    case 1u: return inputs->texel0[channel];
    case 2u: return inputs->texel1[channel];
    case 3u: return color_channel(rdp->primitive_color, channel);
    case 4u: return inputs->shade[channel];
    case 5u: return color_channel(rdp->environment_color, channel);
    case 6u: return (uint8_t)rdp->key_center[channel];
    case 7u: return clamp_u8(signed9(rdp->convert_coefficients[4]));
    default: return 0u;
    }
  case 2u: /* RGB C */
    switch (code) {
    case 0u: return combined[channel];
    case 1u: return inputs->texel0[channel];
    case 2u: return inputs->texel1[channel];
    case 3u: return color_channel(rdp->primitive_color, channel);
    case 4u: return inputs->shade[channel];
    case 5u: return color_channel(rdp->environment_color, channel);
    case 6u: return (uint8_t)rdp->key_scale[channel];
    case 7u: return combined[3];
    case 8u: return inputs->texel0[3];
    case 9u: return inputs->texel1[3];
    case 10u: return color_channel(rdp->primitive_color, 3u);
    case 11u: return inputs->shade[3];
    case 12u: return color_channel(rdp->environment_color, 3u);
    case 13u: return inputs->lod_fraction;
    case 14u: return (uint8_t)rdp->primitive_lod_fraction;
    case 15u: return clamp_u8(signed9(rdp->convert_coefficients[5]));
    default: return 0u;
    }
  default: /* RGB D */
    switch (code) {
    case 0u: return combined[channel];
    case 1u: return inputs->texel0[channel];
    case 2u: return inputs->texel1[channel];
    case 3u: return color_channel(rdp->primitive_color, channel);
    case 4u: return inputs->shade[channel];
    case 5u: return color_channel(rdp->environment_color, channel);
    case 6u: return UINT8_MAX;
    default: return 0u;
    }
  }
}

static uint8_t alpha_source(const t64_machine *machine,
                            const t64_rdp_combiner_inputs *inputs,
                            const uint8_t combined[4], unsigned slot,
                            unsigned code) {
  const t64_rdp_state *rdp = &machine->rdp;
  if (slot == 2u) { /* Alpha C */
    switch (code) {
    case 0u: return inputs->lod_fraction;
    case 1u: return inputs->texel0[3];
    case 2u: return inputs->texel1[3];
    case 3u: return color_channel(rdp->primitive_color, 3u);
    case 4u: return inputs->shade[3];
    case 5u: return color_channel(rdp->environment_color, 3u);
    case 6u: return (uint8_t)rdp->primitive_lod_fraction;
    default: return 0u;
    }
  }
  switch (code) {
  case 0u: return combined[3];
  case 1u: return inputs->texel0[3];
  case 2u: return inputs->texel1[3];
  case 3u: return color_channel(rdp->primitive_color, 3u);
  case 4u: return inputs->shade[3];
  case 5u: return color_channel(rdp->environment_color, 3u);
  case 6u: return UINT8_MAX;
  default: return 0u;
  }
}

static uint8_t combine_channel(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
  int64_t difference = (int64_t)a - (int64_t)b;
  int64_t product = difference * (int64_t)c;
  return clamp_u8(floor_div_255(product) + (int64_t)d);
}

static void combine_cycle(const t64_machine *machine,
                          const t64_rdp_combiner_inputs *inputs,
                          const uint8_t prior[4], uint8_t noise,
                          uint32_t cycle, uint8_t output[4]) {
  uint64_t mode = machine->rdp.combine_mode;
  unsigned r_a, r_b, r_c, r_d;
  unsigned a_a, a_b, a_c, a_d;
  unsigned channel;

  if (cycle == 0u) {
    r_a = (unsigned)((mode >> 52) & UINT64_C(0xf));
    r_c = (unsigned)((mode >> 47) & UINT64_C(0x1f));
    a_a = (unsigned)((mode >> 44) & UINT64_C(7));
    a_c = (unsigned)((mode >> 41) & UINT64_C(7));
    r_b = (unsigned)((mode >> 28) & UINT64_C(0xf));
    r_d = (unsigned)((mode >> 15) & UINT64_C(7));
    a_b = (unsigned)((mode >> 12) & UINT64_C(7));
    a_d = (unsigned)((mode >> 9) & UINT64_C(7));
  } else {
    r_a = (unsigned)((mode >> 37) & UINT64_C(0xf));
    r_c = (unsigned)((mode >> 32) & UINT64_C(0x1f));
    a_a = (unsigned)((mode >> 21) & UINT64_C(7));
    a_c = (unsigned)((mode >> 18) & UINT64_C(7));
    r_b = (unsigned)((mode >> 24) & UINT64_C(0xf));
    r_d = (unsigned)((mode >> 6) & UINT64_C(7));
    a_b = (unsigned)((mode >> 3) & UINT64_C(7));
    a_d = (unsigned)(mode & UINT64_C(7));
  }
  for (channel = 0u; channel < 3u; channel++) {
    uint8_t a = rgb_source(machine, inputs, prior, 0u, r_a, channel, noise);
    uint8_t b = rgb_source(machine, inputs, prior, 1u, r_b, channel, noise);
    uint8_t c = rgb_source(machine, inputs, prior, 2u, r_c, channel, noise);
    uint8_t d = rgb_source(machine, inputs, prior, 3u, r_d, channel, noise);
    output[channel] = combine_channel(a, b, c, d);
  }
  output[3] = combine_channel(
      alpha_source(machine, inputs, prior, 0u, a_a),
      alpha_source(machine, inputs, prior, 1u, a_b),
      alpha_source(machine, inputs, prior, 2u, a_c),
      alpha_source(machine, inputs, prior, 3u, a_d));
}

bool t64_rdp_combine_pixel(t64_machine *machine,
                           const t64_rdp_combiner_inputs *inputs,
                           uint32_t *rgba_out) {
  const uint8_t zero[4] = {0u, 0u, 0u, 0u};
  uint8_t combined[4] = {0u, 0u, 0u, 0u};
  uint8_t next[4];
  uint8_t noise;
  uint32_t cycle_type;

  if (machine == NULL || inputs == NULL || rgba_out == NULL ||
      !t64_has_rdp_combiner_profile(machine) ||
      !machine->rdp.combine_mode_valid) return false;
  cycle_type = (uint32_t)((machine->rdp.other_modes >> 52) & UINT64_C(3));
  if (cycle_type > 1u) return false;

  /* Policy: advance once for every accepted covered pixel and reuse its low
   * byte for every channel and cycle, whether or not a selector reads Noise. */
  noise = next_noise(&machine->rdp);
  combine_cycle(machine, inputs, zero, noise, 0u, combined);
  if (cycle_type == 1u) {
    combine_cycle(machine, inputs, combined, noise, 1u, next);
    (void)memcpy(combined, next, sizeof combined);
  }
  *rgba_out = ((uint32_t)combined[0] << 24) |
              ((uint32_t)combined[1] << 16) |
              ((uint32_t)combined[2] << 8) | (uint32_t)combined[3];
  return true;
}
