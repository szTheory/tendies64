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

static uint8_t coverage_entry(const t64_machine *machine, size_t granule) {
  uint8_t entry;
  if (machine == NULL || machine->rdp_memory_coverage == NULL ||
      granule >= T64_RDP_MEMORY_COVERAGE_BYTES(machine->profile.rdram_bytes))
    return 0u;
  entry = machine->rdp_memory_coverage[granule];
  return (entry & UINT8_C(0x80)) != 0u ? (uint8_t)(entry & UINT8_C(0x0f))
                                      : 0u;
}

uint8_t t64_rdp_memory_coverage_read(const t64_machine *machine,
                                    uint32_t address, uint32_t size_bytes) {
  size_t granule;
  uint8_t first;
  uint8_t second;
  if (!t64_has_rdp_blender_profile(machine) ||
      (size_bytes != 2u && size_bytes != 4u) ||
      (uint64_t)address + size_bytes > machine->profile.rdram_bytes)
    return 0u;
  granule = (size_t)(address / 2u);
  first = coverage_entry(machine, granule);
  if (size_bytes == 2u) return first;
  second = coverage_entry(machine, granule + 1u);
  return first < second ? first : second;
}

void t64_rdp_memory_coverage_write(t64_machine *machine, uint32_t address,
                                   uint32_t size_bytes, uint8_t coverage) {
  size_t granule;
  size_t count;
  size_t i;
  if (!t64_has_rdp_blender_profile(machine) ||
      (size_bytes != 2u && size_bytes != 4u) ||
      (uint64_t)address + size_bytes > machine->profile.rdram_bytes)
    return;
  if (coverage > 8u) coverage = 8u;
  granule = (size_t)(address / 2u);
  count = size_bytes / 2u;
  for (i = 0u; i < count; i++)
    machine->rdp_memory_coverage[granule + i] =
        (uint8_t)(UINT8_C(0x80) | coverage);
}

void t64_rdp_memory_coverage_invalidate(t64_machine *machine,
                                        uint32_t address,
                                        uint32_t size_bytes) {
  uint64_t end;
  size_t first;
  size_t last;
  size_t granule;
  if (!t64_has_rdp_blender_profile(machine) || size_bytes == 0u ||
      machine->rdp_memory_coverage == NULL)
    return;
  end = (uint64_t)address + size_bytes;
  if (end <= address || address >= machine->profile.rdram_bytes) return;
  if (end > machine->profile.rdram_bytes) end = machine->profile.rdram_bytes;
  first = (size_t)(address / 2u);
  last = (size_t)((end - 1u) / 2u);
  for (granule = first; granule <= last; granule++)
    machine->rdp_memory_coverage[granule] = 0u;
}

static uint8_t blend_rgb_source(const t64_machine *machine,
                                const uint8_t pixel[4],
                                const uint8_t prior[3],
                                const uint8_t memory[4], unsigned selector,
                                unsigned channel) {
  switch (selector) {
  case 0u: return prior == NULL ? pixel[channel] : prior[channel];
  case 1u: return memory[channel];
  case 2u: return color_channel(machine->rdp.blend_color, channel);
  default: return color_channel(machine->rdp.fog_color, channel);
  }
}

static uint8_t blend_alpha_factor(const t64_machine *machine,
                                 const t64_rdp_combiner_inputs *inputs,
                                 uint8_t pixel_alpha, unsigned selector) {
  uint8_t alpha;
  switch (selector) {
  case 0u: alpha = pixel_alpha; break;
  case 1u: alpha = color_channel(machine->rdp.fog_color, 3u); break;
  case 2u: alpha = inputs->shade[3]; break;
  default: alpha = 0u; break;
  }
  return (uint8_t)(alpha >> 3);
}

static uint8_t blend_b_factor(const t64_machine *machine,
                              const uint8_t memory[4], unsigned a,
                              unsigned selector) {
  switch (selector) {
  case 0u: return (uint8_t)(32u - a);
  case 1u: return (uint8_t)(memory[3] >> 3);
  case 2u: return 32u;
  default: (void)machine; return 0u;
  }
}

static uint8_t blend_coverage_destination(unsigned mode, uint8_t old_coverage,
                                          uint8_t new_coverage) {
  unsigned sum = (unsigned)old_coverage + (unsigned)new_coverage;
  switch (mode) {
  case 1u:
    return sum == 0u ? 0u : (uint8_t)(1u + ((sum - 1u) % 8u));
  case 2u: return 8u;
  case 3u: return old_coverage;
  default: return (uint8_t)(sum > 8u ? 8u : sum);
  }
}

static void read_blend_memory(const t64_machine *machine, uint32_t address,
                              uint32_t size_bytes, uint8_t color[4]) {
  const uint8_t *ram = machine->ram + address;
  if (size_bytes == 4u) {
    uint8_t alpha5 = (uint8_t)(ram[3] & UINT8_C(0x1f));
    color[0] = ram[0];
    color[1] = ram[1];
    color[2] = ram[2];
    color[3] = (uint8_t)((alpha5 << 3) | (alpha5 >> 2));
  } else {
    uint16_t value = (uint16_t)(((uint16_t)ram[0] << 8) | ram[1]);
    unsigned r = (value >> 11) & 31u;
    unsigned g = (value >> 6) & 31u;
    unsigned b = (value >> 1) & 31u;
    color[0] = (uint8_t)((r << 3) | (r >> 2));
    color[1] = (uint8_t)((g << 3) | (g >> 2));
    color[2] = (uint8_t)((b << 3) | (b >> 2));
    color[3] = (value & 1u) != 0u ? UINT8_MAX : 0u;
  }
}

bool t64_rdp_blend_pixel(t64_machine *machine,
                         const t64_rdp_combiner_inputs *inputs,
                         uint32_t pixel_rgba, uint32_t address,
                         uint32_t size_bytes, uint8_t new_coverage,
                         uint32_t *rgba_out, uint8_t *coverage_out,
                         bool *write_color_out) {
  uint8_t pixel[4];
  uint8_t memory[4] = {0u, 0u, 0u, 0u};
  uint8_t feedback[3] = {0u, 0u, 0u};
  uint8_t old_coverage = 0u;
  uint8_t effective_coverage;
  uint8_t pixel_alpha;
  uint8_t output_alpha;
  uint8_t result[3];
  unsigned cycle_type;
  unsigned cycles;
  unsigned cycle;
  unsigned coverage_mode;
  unsigned sum;
  bool image_read;
  bool alpha_pass = true;
  bool overflow;
  bool blend_enabled;
  bool write_color = true;
  uint64_t modes;

  if (machine == NULL || inputs == NULL || rgba_out == NULL ||
      coverage_out == NULL || write_color_out == NULL ||
      !t64_has_rdp_blender_profile(machine) ||
      machine->rdp_memory_coverage == NULL ||
      (size_bytes != 2u && size_bytes != 4u) ||
      (uint64_t)address + size_bytes > machine->profile.rdram_bytes ||
      new_coverage > 8u || !machine->rdp.other_modes_valid)
    return false;
  modes = machine->rdp.other_modes;
  cycle_type = (unsigned)((modes >> 52) & UINT64_C(3));
  if (cycle_type > 1u) return false;
  cycles = cycle_type == 1u ? 2u : 1u;
  pixel[0] = (uint8_t)(pixel_rgba >> 24);
  pixel[1] = (uint8_t)(pixel_rgba >> 16);
  pixel[2] = (uint8_t)(pixel_rgba >> 8);
  pixel[3] = (uint8_t)pixel_rgba;
  image_read = ((modes >> 6) & UINT64_C(1)) != 0u;
  if (image_read) {
    read_blend_memory(machine, address, size_bytes, memory);
    old_coverage = t64_rdp_memory_coverage_read(machine, address,
                                                size_bytes);
  }
  pixel_alpha = pixel[3];
  effective_coverage = new_coverage;
  if (((modes >> 12) & UINT64_C(1)) != 0u) {
    effective_coverage = (uint8_t)(((unsigned)new_coverage * pixel_alpha +
                                    127u) / 255u);
  }
  if (((modes >> 13) & UINT64_C(1)) != 0u) {
    pixel_alpha = (uint8_t)(((unsigned)effective_coverage * 255u + 4u) / 8u);
  }
  sum = (unsigned)old_coverage + effective_coverage;
  overflow = sum > 8u;
  coverage_mode = (unsigned)((modes >> 8) & UINT64_C(3));
  *coverage_out = blend_coverage_destination(coverage_mode, old_coverage,
                                              effective_coverage);
  blend_enabled = ((modes >> 14) & UINT64_C(1)) != 0u ||
                  (((modes >> 3) & UINT64_C(1)) != 0u &&
                   effective_coverage < 8u);
  if (new_coverage == 0u) {
    *rgba_out = pixel_rgba;
    *write_color_out = false;
    return true;
  }
  if (((modes >> 0) & UINT64_C(1)) != 0u) {
    uint8_t threshold = color_channel(machine->rdp.blend_color, 3u);
    if (((modes >> 1) & UINT64_C(1)) != 0u)
      threshold = (uint8_t)machine->rdp.random_state;
    alpha_pass = pixel_alpha >= threshold;
  }
  if (((modes >> 7) & UINT64_C(1)) != 0u && !overflow) write_color = false;
  if (!alpha_pass) write_color = false;
  if (*coverage_out == 0u) write_color = false;
  if (blend_enabled) {
    for (cycle = 0u; cycle < cycles; cycle++) {
      unsigned shift = cycle == 0u ? 0u : 1u;
      unsigned p_selector = (unsigned)((modes >> (30u - shift * 2u)) & 3u);
      unsigned a_selector = (unsigned)((modes >> (26u - shift * 2u)) & 3u);
      unsigned m_selector = (unsigned)((modes >> (22u - shift * 2u)) & 3u);
      unsigned b_selector = (unsigned)((modes >> (18u - shift * 2u)) & 3u);
      unsigned a = blend_alpha_factor(machine, inputs, pixel_alpha,
                                      a_selector);
      unsigned b = blend_b_factor(machine, memory, a, b_selector);
      unsigned channel;
      for (channel = 0u; channel < 3u; channel++) {
        uint8_t p = blend_rgb_source(machine, pixel,
                                     cycle == 0u ? NULL : feedback,
                                     memory, p_selector, channel);
        uint8_t m = blend_rgb_source(machine, pixel, NULL, memory, m_selector,
                                     channel);
        unsigned denominator = a + b;
        unsigned numerator = a * (unsigned)p + b * (unsigned)m;
        feedback[channel] = denominator == 0u
                                ? 0u
                                : (uint8_t)(numerator / denominator);
      }
    }
    result[0] = feedback[0];
    result[1] = feedback[1];
    result[2] = feedback[2];
  } else {
    result[0] = pixel[0];
    result[1] = pixel[1];
    result[2] = pixel[2];
  }
  output_alpha = pixel[3];
  *rgba_out = ((uint32_t)result[0] << 24) |
              ((uint32_t)result[1] << 16) |
              ((uint32_t)result[2] << 8) | output_alpha;
  *write_color_out = write_color;
  return true;
}
