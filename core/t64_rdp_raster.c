#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>

static int64_t floor_div_i64(int64_t numerator, int64_t denominator) {
  int64_t quotient = numerator / denominator;
  int64_t remainder = numerator % denominator;

  if (remainder != 0 && numerator < 0) quotient--;
  return quotient;
}

static int64_t sign_extend_u14(uint32_t value) {
  value &= UINT32_C(0x3fff);
  return (value & UINT32_C(0x2000)) != 0u
             ? (int64_t)value - INT64_C(0x4000)
             : (int64_t)value;
}

static int64_t signed_u32(uint32_t value) {
  return (value & UINT32_C(0x80000000)) != 0u
             ? (int64_t)value - INT64_C(0x100000000)
             : (int64_t)value;
}

static int64_t pair_q16(uint64_t integer_word, uint64_t fraction_word,
                        unsigned shift) {
  uint32_t integer = (uint32_t)((integer_word >> shift) & UINT64_C(0xffff));
  uint32_t fraction =
      (uint32_t)((fraction_word >> shift) & UINT64_C(0xffff));
  return signed_u32((integer << 16) | fraction);
}

static uint64_t magnitude_i64(int64_t value) {
  return value < 0 ? (uint64_t)(-(value + INT64_C(1))) + UINT64_C(1)
                  : (uint64_t)value;
}

static bool shade_products_are_bounded(const uint64_t *words,
                                       unsigned shade_offset,
                                       uint64_t x_delta_bound,
                                       uint64_t y_delta_bound) {
  unsigned channel;
  const uint64_t product_limit = (uint64_t)INT64_MAX / UINT64_C(4);

  for (channel = 0u; channel < 4u; channel++) {
    unsigned shift = 48u - channel * 16u;
    int64_t dx = pair_q16(words[shade_offset + 1u],
                          words[shade_offset + 3u], shift);
    int64_t de = pair_q16(words[shade_offset + 4u],
                          words[shade_offset + 6u], shift);
    int64_t dy = pair_q16(words[shade_offset + 5u],
                          words[shade_offset + 7u], shift);
    if ((x_delta_bound != 0u &&
         (magnitude_i64(dx) > product_limit / x_delta_bound ||
          magnitude_i64(de) > product_limit / x_delta_bound)) ||
        (y_delta_bound != 0u &&
         magnitude_i64(dy) > product_limit / y_delta_bound)) return false;
  }
  return true;
}

static bool texture_products_are_bounded(const t64_machine *machine,
                                         const uint64_t *words,
                                         uint32_t opcode,
                                         uint32_t x0, uint32_t x1,
                                         uint32_t first_y,
                                         uint32_t last_y,
                                         int64_t yh16) {
  const unsigned texture_offset = opcode == UINT32_C(0x0e) ? 12u : 4u;
  const int64_t xh16 = signed_u32((uint32_t)(words[2] >> 32));
  const int64_t dxh = signed_u32((uint32_t)words[2]);
  const int64_t y0 = (int64_t)first_y * INT64_C(65536) + INT64_C(32768);
  const int64_t y1 = (int64_t)last_y * INT64_C(65536) + INT64_C(32768);
  const int64_t x_left = (int64_t)x0 * INT64_C(65536) + INT64_C(32768);
  const int64_t x_right = (int64_t)(x1 - 1u) * INT64_C(65536) +
                          INT64_C(32768);
  uint64_t y_bound = magnitude_i64(y0 - yh16);
  uint64_t current = magnitude_i64(y1 - yh16);
  uint64_t edge_product, edge_bound, x_bound;
  uint64_t max_s, max_t;
  uint64_t contribution[3] = {0u, 0u, 0u};
  int64_t base[3];
  unsigned lane;

  if (current > y_bound) y_bound = current;
  if (y_bound > UINT64_MAX - UINT64_C(65536)) return false;
  y_bound += UINT64_C(65536);
  if (magnitude_i64(dxh) != 0u &&
      y_bound > (uint64_t)INT64_MAX / magnitude_i64(dxh)) return false;
  edge_product = magnitude_i64(dxh) * y_bound;
  edge_bound = edge_product / UINT64_C(65536) + UINT64_C(1);
  x_bound = magnitude_i64(x_left - xh16);
  current = magnitude_i64(x_right - xh16);
  if (current > x_bound) x_bound = current;
  if (x_bound > UINT64_MAX - edge_bound - UINT64_C(65536)) return false;
  x_bound += edge_bound + UINT64_C(65536);

  for (lane = 0u; lane < 3u; lane++) {
    const unsigned shift = 48u - lane * 16u;
    const int64_t dx = pair_q16(words[texture_offset + 1u],
                                words[texture_offset + 3u], shift);
    const int64_t de = pair_q16(words[texture_offset + 4u],
                                words[texture_offset + 6u], shift);
    const int64_t dy = pair_q16(words[texture_offset + 5u],
                                words[texture_offset + 7u], shift);
    const uint64_t abs_dx = magnitude_i64(dx);
    const uint64_t abs_de = magnitude_i64(de);
    const uint64_t abs_dy = magnitude_i64(dy);
    uint64_t product_x, product_e, product_y, total;

    if ((x_bound != 0u &&
         (abs_dx > (uint64_t)INT64_MAX / x_bound ||
          abs_de > (uint64_t)INT64_MAX / edge_bound)) ||
        (y_bound != 0u && abs_dy > (uint64_t)INT64_MAX / y_bound))
      return false;
    product_x = abs_dx * x_bound;
    product_e = abs_de * edge_bound;
    product_y = abs_dy * y_bound;
    total = product_x / UINT64_C(65536) +
            product_e / UINT64_C(65536) +
            product_y / UINT64_C(65536) + UINT64_C(3);
    base[lane] = pair_q16(words[texture_offset],
                          words[texture_offset + 2u], shift);
    if (total > (uint64_t)INT32_MAX ||
        magnitude_i64(base[lane]) > (uint64_t)INT32_MAX - total)
      return false;
    contribution[lane] = total;
  }

  {
    int64_t min_w = base[2] - (int64_t)contribution[2];
    const int64_t max_w = base[2] + (int64_t)contribution[2];
    if (min_w <= 0 && max_w >= 0) return false;
    min_w = min_w > 0 ? min_w : (int64_t)magnitude_i64(max_w);
    max_s = magnitude_i64(base[0]) + contribution[0];
    max_t = magnitude_i64(base[1]) + contribution[1];
    if (((machine->rdp.other_modes >> 51) & UINT64_C(1)) != 0u) {
      uint64_t s_numerator = max_s * UINT64_C(65536);
      uint64_t t_numerator = max_t * UINT64_C(65536);
      if (s_numerator / (uint64_t)min_w > (uint64_t)INT32_MAX ||
          t_numerator / (uint64_t)min_w > (uint64_t)INT32_MAX) return false;
    }
  }
  return true;
}

size_t t64_rdp_triangle_word_count(uint32_t opcode) {
  switch (opcode) {
  case 0x08u: return 4u;
  case 0x09u: return 6u;
  case 0x0au: return 12u;
  case 0x0bu: return 14u;
  case 0x0cu: return 12u;
  case 0x0du: return 14u;
  case 0x0eu: return 20u;
  case 0x0fu: return 22u;
  default: return 0u;
  }
}

bool t64_rdp_triangle_bounds(const t64_machine *machine, uint32_t opcode,
                             const uint64_t *words, size_t word_count,
                             uint32_t *y_begin_out,
                             uint32_t *y_end_out) {
  const t64_rdp_state *rdp;
  int64_t yh16, ym16, yl16, first, last;
  uint32_t x0, x1, y0, y1;
  uint64_t end;

  const bool texture_triangle = opcode == UINT32_C(0x0a) ||
                                opcode == UINT32_C(0x0b) ||
                                opcode == UINT32_C(0x0e) ||
                                opcode == UINT32_C(0x0f);
  const bool shade_triangle = opcode == UINT32_C(0x0c) ||
                              opcode == UINT32_C(0x0d) ||
                              opcode == UINT32_C(0x0e) ||
                              opcode == UINT32_C(0x0f);
  const bool z_triangle = opcode == UINT32_C(0x09) ||
                          opcode == UINT32_C(0x0b) ||
                          opcode == UINT32_C(0x0d) ||
                          opcode == UINT32_C(0x0f);

  if (machine == NULL || words == NULL || y_begin_out == NULL ||
      y_end_out == NULL ||
      (opcode != 0x08u && opcode != 0x09u && opcode != 0x0cu &&
       opcode != 0x0du && !texture_triangle) ||
      (z_triangle && !t64_has_rdp_depth_profile(machine)) ||
      (texture_triangle && !t64_has_rdp_texture_profile(machine)) ||
      machine->rdp_triangle_coverage == NULL ||
      word_count != t64_rdp_triangle_word_count(opcode)) return false;
  rdp = &machine->rdp;
  if (!rdp->color_image_valid || !rdp->scissor_valid ||
      !rdp->other_modes_valid || rdp->color_image_format != 0u ||
      (rdp->color_image_size != 2u &&
       !(t64_has_rdp_blender_profile(machine) &&
         rdp->color_image_size == 3u)) ||
      rdp->color_image_width == 0u ||
      (((rdp->other_modes >> 52) & UINT64_C(3)) != 0u &&
       (!t64_has_rdp_combiner_profile(machine) ||
        ((rdp->other_modes >> 52) & UINT64_C(3)) != 1u)) ||
      (rdp->color_image_address &
       (rdp->color_image_size == 3u ? UINT32_C(3) : UINT32_C(1))) != 0u)
    return false;

  yh16 = sign_extend_u14((uint32_t)(words[0] & UINT64_C(0x3fff))) *
         INT64_C(16384);
  ym16 = sign_extend_u14((uint32_t)((words[0] >> 16) & UINT64_C(0x3fff))) *
         INT64_C(16384);
  yl16 = sign_extend_u14((uint32_t)((words[0] >> 32) & UINT64_C(0x3fff))) *
         INT64_C(16384);
  if (yh16 > ym16 || ym16 > yl16) return false;

  x0 = rdp->scissor_xh >> 2;
  x1 = rdp->scissor_xl >> 2;
  y0 = rdp->scissor_yh >> 2;
  y1 = rdp->scissor_yl >> 2;
  if (x1 > rdp->color_image_width) x1 = rdp->color_image_width;
  if (x0 > x1) return false;
  if (x0 == x1 || y0 >= y1) {
    *y_begin_out = 1u;
    *y_end_out = 0u;
    return true;
  }
  if (y1 > 1024u) y1 = 1024u;
  if (y0 > y1) return false;
  end = (uint64_t)rdp->color_image_address +
        (((uint64_t)(y1 - 1u) * rdp->color_image_width + (x1 - 1u)) + 1u) *
            (rdp->color_image_size == 3u ? 4u : 2u);
  if (end > machine->profile.rdram_bytes) return false;
  if (z_triangle) {
    uint64_t depth_end;
    if (!rdp->depth_image_valid ||
        (rdp->depth_image_address & UINT32_C(7)) != 0u ||
        (((rdp->other_modes >> 2) & UINT64_C(1)) != 0u &&
         !rdp->primitive_depth_valid))
      return false;
    depth_end = (uint64_t)rdp->depth_image_address +
                ((uint64_t)(y1 - 1u) * rdp->color_image_width +
                 (x1 - 1u) + UINT64_C(1)) * UINT64_C(2);
    if (depth_end > machine->profile.rdram_bytes) return false;
  }

  first = floor_div_i64(yh16, INT64_C(65536));
  last = floor_div_i64(yl16 + INT64_C(65535), INT64_C(65536)) - 1;
  if (first < (int64_t)y0) first = (int64_t)y0;
  if (last >= (int64_t)y1) last = (int64_t)y1 - 1;
  if (first > last) {
    *y_begin_out = 1u;
    *y_end_out = 0u;
    return true;
  }
  if (shade_triangle) {
    uint64_t y_delta_bound = (uint64_t)(yl16 - yh16) + UINT64_C(65536);
    uint64_t maximum_slope = UINT64_C(0x80000000);
    uint64_t x_delta_bound = UINT64_C(0x80000000) +
        (maximum_slope * y_delta_bound) / UINT64_C(65536) +
        UINT64_C(1024) * UINT64_C(65536);
    if (!shade_products_are_bounded(words, 4u, x_delta_bound,
                                    y_delta_bound))
      return false;
  }
  if (texture_triangle &&
      (!t64_rdp_tmem_validate_triangle(
           machine, (uint32_t)((words[0] >> 48) & UINT64_C(7))) ||
       !texture_products_are_bounded(machine, words, opcode, x0, x1,
                                     (uint32_t)first, (uint32_t)last,
                                     yh16))) return false;
  if (z_triangle &&
      !t64_rdp_depth_triangle_preflight(machine, opcode, words, word_count,
                                        x0, x1, (uint32_t)first,
                                        (uint32_t)last))
    return false;
  if (texture_triangle) {
    uint32_t detail = (uint32_t)((rdp->other_modes >> 50) & UINT64_C(1));
    uint32_t sharpen = (uint32_t)((rdp->other_modes >> 49) & UINT64_C(1));
    uint32_t lod = (uint32_t)((rdp->other_modes >> 48) & UINT64_C(1));
    uint32_t primitive_tile = (uint32_t)((words[0] >> 48) & UINT64_C(7));
    uint32_t max_level = (uint32_t)((words[0] >> 51) & UINT64_C(7));
    uint32_t tile;
    if ((detail != 0u && sharpen != 0u) ||
        ((detail != 0u || sharpen != 0u) && lod == 0u)) return false;
    if (lod != 0u || detail != 0u || sharpen != 0u) {
      if (max_level >= T64_RDP_TILE_COUNT - primitive_tile) return false;
      for (tile = primitive_tile; tile <= primitive_tile + max_level; tile++) {
        if (!t64_rdp_tmem_validate_triangle(machine, tile)) return false;
      }
    }
  }
  *y_begin_out = (uint32_t)first;
  *y_end_out = (uint32_t)last;
  return true;
}

static void decode_edges(const uint64_t *words, int64_t *yh16,
                         int64_t *ym16, int64_t *yl16, int64_t *xh16,
                         int64_t *xm16, int64_t *xl16, int64_t *dxh,
                         int64_t *dxm, int64_t *dxl) {
  *yl16 = sign_extend_u14((uint32_t)((words[0] >> 32) & UINT64_C(0x3fff))) *
          INT64_C(16384);
  *ym16 = sign_extend_u14((uint32_t)((words[0] >> 16) & UINT64_C(0x3fff))) *
          INT64_C(16384);
  *yh16 = sign_extend_u14((uint32_t)(words[0] & UINT64_C(0x3fff))) *
          INT64_C(16384);
  *xl16 = signed_u32((uint32_t)(words[1] >> 32));
  *dxl = signed_u32((uint32_t)words[1]);
  *xh16 = signed_u32((uint32_t)(words[2] >> 32));
  *dxh = signed_u32((uint32_t)words[2]);
  *xm16 = signed_u32((uint32_t)(words[3] >> 32));
  *dxm = signed_u32((uint32_t)words[3]);
}

static uint16_t rgba5551(uint32_t color) {
  uint32_t red = (color >> 24) & UINT32_C(0xff);
  uint32_t green = (color >> 16) & UINT32_C(0xff);
  uint32_t blue = (color >> 8) & UINT32_C(0xff);
  uint32_t alpha = color & UINT32_C(0xff);

  return (uint16_t)(((red >> 3) << 11) | ((green >> 3) << 6) |
                    ((blue >> 3) << 1) | (alpha >= 128u ? 1u : 0u));
}

static uint8_t color_dither_bias(const t64_machine *machine, uint32_t x,
                                uint32_t y) {
  static const uint8_t magic[4][4] = {
      {0u, 6u, 1u, 7u}, {4u, 2u, 5u, 3u},
      {1u, 7u, 0u, 6u}, {5u, 3u, 4u, 2u}};
  static const uint8_t bayer[4][4] = {
      {0u, 8u, 2u, 10u}, {12u, 4u, 14u, 6u},
      {3u, 11u, 1u, 9u}, {15u, 7u, 13u, 5u}};
  unsigned selector = (unsigned)((machine->rdp.other_modes >> 38) & 3u);
  switch (selector) {
  case 0u: return magic[y & 3u][x & 3u];
  case 1u: return (uint8_t)(bayer[y & 3u][x & 3u] >> 1);
  case 2u: return (uint8_t)(machine->rdp.random_state & 7u);
  default: return 0u;
  }
}

static uint8_t alpha_dither_bias(const t64_machine *machine, uint32_t x,
                                uint32_t y) {
  static const uint8_t pattern[4][4] = {
      {0u, 6u, 1u, 7u}, {4u, 2u, 5u, 3u},
      {1u, 7u, 0u, 6u}, {5u, 3u, 4u, 2u}};
  unsigned selector = (unsigned)((machine->rdp.other_modes >> 36) & 3u);
  switch (selector) {
  case 0u: return pattern[y & 3u][x & 3u];
  case 1u: return (uint8_t)(7u - pattern[y & 3u][x & 3u]);
  case 2u: return (uint8_t)(machine->rdp.random_state & 7u);
  default: return 0u;
  }
}

uint16_t t64_rdp_quantize_rgba5551_v6(const t64_machine *machine,
                                      uint32_t color, uint32_t x,
                                      uint32_t y) {
  int r = (int)((color >> 24) & 255u) + color_dither_bias(machine, x, y);
  int g = (int)((color >> 16) & 255u) + color_dither_bias(machine, x, y);
  int b = (int)((color >> 8) & 255u) + color_dither_bias(machine, x, y);
  int alpha_threshold = 128;
  unsigned alpha_mode = (unsigned)((machine->rdp.other_modes >> 36) & 3u);
  if (alpha_mode != 3u)
    alpha_threshold += (int)alpha_dither_bias(machine, x, y) - 4;
  if (r > 255) r = 255;
  if (g > 255) g = 255;
  if (b > 255) b = 255;
  return (uint16_t)(((unsigned)(r >> 3) << 11) |
                    ((unsigned)(g >> 3) << 6) |
                    ((unsigned)(b >> 3) << 1) |
                    (((int)(color & 255u) >= alpha_threshold) ? 1u : 0u));
}

uint8_t t64_rdp_quantize_alpha5_v6(const t64_machine *machine,
                                   uint32_t color, uint32_t x, uint32_t y) {
  int alpha = (int)(color & 255u);
  if (((machine->rdp.other_modes >> 36) & 3u) != 3u)
    alpha += (int)alpha_dither_bias(machine, x, y) - 4;
  if (alpha < 0) alpha = 0;
  if (alpha > 255) alpha = 255;
  return (uint8_t)((unsigned)alpha >> 3);
}

static int64_t shade_channel(const t64_rdp_state *rdp, unsigned channel,
                             int64_t x16, int64_t y16, int64_t xh16,
                             int64_t yh16, int64_t major_x16) {
  const uint64_t *words = rdp->pending_triangle_words;
  uint64_t shift = (uint64_t)(48u - channel * 16u);
  int64_t base = pair_q16(words[4], words[6], (unsigned)shift);
  int64_t dx = pair_q16(words[5], words[7], (unsigned)shift);
  int64_t de = pair_q16(words[8], words[10], (unsigned)shift);
  int64_t dy = pair_q16(words[9], words[11], (unsigned)shift);
  int64_t from_major = floor_div_i64(dx * (x16 - major_x16),
                                     INT64_C(65536));
  int64_t along_edge = floor_div_i64(de * (major_x16 - xh16),
                                     INT64_C(65536));
  int64_t vertical = floor_div_i64(dy * (y16 - yh16), INT64_C(65536));

  (void)rdp;
  return floor_div_i64(base + from_major + along_edge + vertical,
                       INT64_C(65536));
}

static int64_t texture_channel_q16(const t64_rdp_state *rdp, unsigned lane,
                                   int64_t x16, int64_t y16, int64_t xh16,
                                   int64_t yh16, int64_t major_x16) {
  const uint64_t *words = rdp->pending_triangle_words;
  uint32_t opcode = (uint32_t)((words[0] >> 56) & UINT64_C(0x3f));
  unsigned offset = opcode == UINT32_C(0x0e) ? 12u : 4u;
  unsigned shift = 48u - lane * 16u;
  int64_t base = pair_q16(words[offset], words[offset + 2u], shift);
  int64_t dx = pair_q16(words[offset + 1u], words[offset + 3u], shift);
  int64_t de = pair_q16(words[offset + 4u], words[offset + 6u], shift);
  int64_t dy = pair_q16(words[offset + 5u], words[offset + 7u], shift);
  int64_t from_major = floor_div_i64(dx * (x16 - major_x16),
                                     INT64_C(65536));
  int64_t along_edge = floor_div_i64(de * (major_x16 - xh16),
                                     INT64_C(65536));
  int64_t vertical = floor_div_i64(dy * (y16 - yh16), INT64_C(65536));

  return base + from_major + along_edge + vertical;
}

static uint16_t modulate_texture_shade(const t64_rdp_state *rdp,
                                       uint16_t texel, int64_t x16,
                                       int64_t y16, int64_t xh16,
                                       int64_t yh16, int64_t major_x16) {
  uint32_t channels[4];
  uint32_t result[3];
  unsigned channel;
  const uint32_t texture_channels[3] = {
      (uint32_t)((texel >> 11) & UINT16_C(0x1f)),
      (uint32_t)((texel >> 6) & UINT16_C(0x1f)),
      (uint32_t)((texel >> 1) & UINT16_C(0x1f)),
  };

  for (channel = 0u; channel < 4u; channel++) {
    int64_t value = shade_channel(rdp, channel, x16, y16, xh16, yh16,
                                  major_x16);
    if (value < 0) value = 0;
    if (value > 255) value = 255;
    channels[channel] = (uint32_t)value;
  }
  for (channel = 0u; channel < 3u; channel++)
    result[channel] = (texture_channels[channel] * channels[channel]) / 255u;
  return (uint16_t)((result[0] << 11) | (result[1] << 6) |
                    (result[2] << 1) |
                    (((texel & UINT16_C(1)) != 0u &&
                      channels[3] >= 128u) ? 1u : 0u));
}

typedef struct {
  uint32_t base_tile;
  uint32_t next_tile;
  int32_t blend_q8;
  uint32_t lod_fraction;
} texture_lod_selection;

static bool project_texture_q16(int64_t s, int64_t t, int64_t w,
                                bool perspective, int64_t *s_out,
                                int64_t *t_out) {
  if (s_out == NULL || t_out == NULL || s < INT32_MIN || s > INT32_MAX ||
      t < INT32_MIN || t > INT32_MAX || w < INT32_MIN || w > INT32_MAX)
    return false;
  if (perspective) {
    int64_t s_numerator, t_numerator;
    int64_t s_quotient, t_quotient;
    int64_t s_remainder, t_remainder;
    if (w == 0) return false;
    s_numerator = s * INT64_C(65536);
    t_numerator = t * INT64_C(65536);
    s_quotient = s_numerator / w;
    t_quotient = t_numerator / w;
    s_remainder = s_numerator % w;
    t_remainder = t_numerator % w;
    if (s_remainder != 0 && ((s_numerator < 0) != (w < 0))) s_quotient--;
    if (t_remainder != 0 && ((t_numerator < 0) != (w < 0))) t_quotient--;
    s = s_quotient;
    t = t_quotient;
  }
  if (s < INT32_MIN || s > INT32_MAX || t < INT32_MIN || t > INT32_MAX)
    return false;
  *s_out = s;
  *t_out = t;
  return true;
}

static bool texture_values_at(const t64_rdp_state *rdp, int64_t xcenter,
                              int64_t ycenter, int64_t xh16, int64_t yh16,
                              int64_t major_x, bool perspective,
                              int64_t *s_out, int64_t *t_out) {
  int64_t s = texture_channel_q16(rdp, 0u, xcenter, ycenter, xh16, yh16,
                                  major_x);
  int64_t t = texture_channel_q16(rdp, 1u, xcenter, ycenter, xh16, yh16,
                                  major_x);
  int64_t w = texture_channel_q16(rdp, 2u, xcenter, ycenter, xh16, yh16,
                                  major_x);
  return project_texture_q16(s, t, w, perspective, s_out, t_out);
}

static bool select_texture_lod(const t64_rdp_state *rdp, int64_t s,
                               int64_t t, int64_t w, int64_t xcenter,
                               int64_t ycenter, int64_t xh16, int64_t yh16,
                               int64_t major_x, int64_t dxh,
                               texture_lod_selection *selection) {
  const bool perspective = ((rdp->other_modes >> 51) & UINT64_C(1)) != 0u;
  const uint32_t lod_enabled =
      (uint32_t)((rdp->other_modes >> 48) & UINT64_C(1));
  const uint32_t detail =
      (uint32_t)((rdp->other_modes >> 50) & UINT64_C(1));
  const uint32_t sharpen =
      (uint32_t)((rdp->other_modes >> 49) & UINT64_C(1));
  int64_t s_now, t_now, s_x, t_x, s_y, t_y;
  int64_t next_major;
  uint64_t rho, candidate, lod;
  uint32_t level = 0u, fraction;
  const uint32_t primitive_tile =
      (uint32_t)((rdp->pending_triangle_words[0] >> 48) & UINT64_C(7));
  const uint32_t max_level =
      (uint32_t)((rdp->pending_triangle_words[0] >> 51) & UINT64_C(7));

  if (selection == NULL) return false;
  selection->base_tile = primitive_tile;
  selection->next_tile = primitive_tile;
  selection->blend_q8 = 0;
  selection->lod_fraction = 0u;
  if (lod_enabled == 0u) return true;
  if (!project_texture_q16(s, t, w, perspective, &s_now, &t_now) ||
      !texture_values_at(rdp, xcenter + INT64_C(65536), ycenter, xh16, yh16,
                         major_x, perspective, &s_x, &t_x)) return false;
  next_major = xh16 + floor_div_i64(
      dxh * (ycenter + INT64_C(65536) - yh16), INT64_C(65536));
  if (!texture_values_at(rdp, xcenter, ycenter + INT64_C(65536), xh16, yh16,
                         next_major, perspective, &s_y, &t_y)) return false;

  rho = magnitude_i64(s_x - s_now);
  candidate = magnitude_i64(t_x - t_now);
  if (candidate > rho) rho = candidate;
  candidate = magnitude_i64(s_y - s_now);
  if (candidate > rho) rho = candidate;
  candidate = magnitude_i64(t_y - t_now);
  if (candidate > rho) rho = candidate;
  lod = rho < UINT64_C(32768) ? UINT64_C(32768) : rho;
  while (lod >= UINT64_C(131072) && level + 1u < T64_RDP_TILE_COUNT) {
    lod >>= 1;
    level++;
  }
  if (level > max_level) {
    level = max_level;
    lod = rho >> level;
  }
  fraction = (uint32_t)((lod & UINT64_C(0xffff)) >> 8);
  selection->lod_fraction = fraction;
  if (sharpen != 0u) {
    selection->base_tile = primitive_tile + level;
    selection->next_tile = primitive_tile + (level < max_level ? level + 1u : level);
    selection->blend_q8 = (int32_t)fraction - 256;
  } else if (detail != 0u) {
    selection->base_tile = primitive_tile + (level < max_level ? level + 1u : level);
    selection->next_tile = primitive_tile + level;
    selection->blend_q8 = 256 - (int32_t)fraction;
  } else {
    selection->base_tile = primitive_tile + level;
    selection->next_tile = primitive_tile + level;
  }
  return true;
}

static uint32_t texture_channel5(uint16_t pixel, unsigned channel) {
  if (channel == 0u) return (pixel >> 11) & UINT16_C(0x1f);
  if (channel == 1u) return (pixel >> 6) & UINT16_C(0x1f);
  if (channel == 2u) return (pixel >> 1) & UINT16_C(0x1f);
  return pixel & UINT16_C(1);
}

static uint16_t blend_texture_levels(uint16_t base, uint16_t next,
                                     int32_t blend_q8) {
  uint32_t output[4];
  unsigned channel;
  for (channel = 0u; channel < 4u; channel++) {
    int64_t a = (int64_t)texture_channel5(base, channel);
    int64_t b = (int64_t)texture_channel5(next, channel);
    int64_t value = a + floor_div_i64((b - a) * blend_q8, INT64_C(256));
    if (value < 0) value = 0;
    if (value > (channel == 3u ? 1 : 31))
      value = channel == 3u ? 1 : 31;
    output[channel] = (uint32_t)value;
  }
  return (uint16_t)((output[0] << 11) | (output[1] << 6) |
                    (output[2] << 1) | output[3]);
}

void t64_rdp_triangle_render_row(t64_machine *machine) {
  static const int64_t sample[4] = {INT64_C(8192), INT64_C(24576),
                                    INT64_C(40960), INT64_C(57344)};
  t64_rdp_state *rdp = &machine->rdp;
  const uint64_t *words = rdp->pending_triangle_words;
  int64_t yh16, ym16, yl16, xh16, xm16, xl16, dxh, dxm, dxl;
  uint32_t x, x0 = rdp->scissor_xh >> 2, x1 = rdp->scissor_xl >> 2;
  uint32_t max_x = rdp->color_image_width;
  uint32_t opcode = (uint32_t)((words[0] >> 56) & UINT64_C(0x3f));
  bool texture_triangle = opcode == UINT32_C(0x0a) ||
                          opcode == UINT32_C(0x0b) ||
                          opcode == UINT32_C(0x0e) ||
                          opcode == UINT32_C(0x0f);
  bool z_triangle = opcode == UINT32_C(0x09) ||
                    opcode == UINT32_C(0x0b) ||
                    opcode == UINT32_C(0x0d) ||
                    opcode == UINT32_C(0x0f);

  decode_edges(words, &yh16, &ym16, &yl16, &xh16, &xm16, &xl16,
               &dxh, &dxm, &dxl);
  if (x1 > max_x) x1 = max_x;
  for (x = x0; x < x1; x++) {
    unsigned covered = 0u;
    uint16_t coverage_mask = 0u;
    unsigned sy;
    unsigned sx;
    int64_t ycenter = (int64_t)rdp->render_y * INT64_C(65536) +
                      INT64_C(32768);
    int64_t major_center = xh16 + floor_div_i64(
        dxh * (ycenter - yh16), INT64_C(65536));

    for (sy = 0u; sy < 4u; sy++) {
      int64_t ysample = (int64_t)rdp->render_y * INT64_C(65536) + sample[sy];
      int64_t major = xh16 + floor_div_i64(
          dxh * (ysample - yh16), INT64_C(65536));
      int64_t minor;
      int64_t left;
      int64_t right;
      if (ysample < ym16) {
        minor = xh16 + floor_div_i64(dxm * (ysample - yh16),
                                     INT64_C(65536));
      } else {
        minor = xm16 + floor_div_i64(dxl * (ysample - ym16),
                                     INT64_C(65536));
      }
      if (((words[0] >> 55) & UINT64_C(1)) == 0u) {
        left = major;
        right = minor;
      } else {
        left = minor;
        right = major;
      }
      for (sx = 0u; sx < 4u; sx++) {
        int64_t xsample = (int64_t)x * INT64_C(65536) + sample[sx];
        if (ysample >= yh16 && ysample < yl16 && xsample >= left &&
            xsample < right) {
          unsigned sample_index = sy * 4u + sx;
          covered++;
          coverage_mask |= (uint16_t)(UINT16_C(1) << sample_index);
        }
      }
    }
    rdp->triangle_last_coverage_x = x;
    rdp->triangle_last_coverage_y = rdp->render_y;
    rdp->triangle_last_coverage_mask = coverage_mask;
    if (covered != 0u) {
      size_t coverage_index =
          (size_t)rdp->render_y * T64_RDP_TRIANGLE_COVERAGE_WIDTH + x;
      uint16_t color;
      uint32_t rgba = 0u;
      uint8_t logical_coverage = (uint8_t)((covered + 1u) / 2u);
      uint8_t destination_coverage = logical_coverage;
      bool write_color = true;
      bool have_depth_result = false;
      t64_rdp_depth_pixel_result depth_result = {0u, 0u, 0u, false, false};
      int64_t xcenter = (int64_t)x * INT64_C(65536) + INT64_C(32768);
      if (t64_has_rdp_combiner_profile(machine)) {
        t64_rdp_combiner_inputs inputs = {{0u}, {0u}, {0u}, 0u};
        if (texture_triangle) {
          int64_t s = texture_channel_q16(rdp, 0u, xcenter, ycenter, xh16,
                                          yh16, major_center);
          int64_t t = texture_channel_q16(rdp, 1u, xcenter, ycenter, xh16,
                                          yh16, major_center);
          int64_t w = texture_channel_q16(rdp, 2u, xcenter, ycenter, xh16,
                                          yh16, major_center);
          texture_lod_selection selection;
          uint32_t texel0, texel1;
          if (!select_texture_lod(rdp, s, t, w, xcenter, ycenter, xh16, yh16,
                                  major_center, dxh, &selection) ||
              !t64_rdp_tmem_triangle_pixel_rgba8(
                  machine, selection.base_tile, s, t, w,
                  ((rdp->other_modes >> 51) & UINT64_C(1)) != 0u,
                  &texel0)) {
            rdp->refusal_reason = T64_RDP_REFUSAL_INVALID_TEXTURE;
            continue;
          }
          texel1 = texel0;
          if (selection.next_tile != selection.base_tile &&
              !t64_rdp_tmem_triangle_pixel_rgba8(
                  machine, selection.next_tile, s, t, w,
                  ((rdp->other_modes >> 51) & UINT64_C(1)) != 0u,
                  &texel1)) {
            rdp->refusal_reason = T64_RDP_REFUSAL_INVALID_TEXTURE;
            continue;
          }
          inputs.texel0[0] = (uint8_t)(texel0 >> 24);
          inputs.texel0[1] = (uint8_t)(texel0 >> 16);
          inputs.texel0[2] = (uint8_t)(texel0 >> 8);
          inputs.texel0[3] = (uint8_t)texel0;
          inputs.texel1[0] = (uint8_t)(texel1 >> 24);
          inputs.texel1[1] = (uint8_t)(texel1 >> 16);
          inputs.texel1[2] = (uint8_t)(texel1 >> 8);
          inputs.texel1[3] = (uint8_t)texel1;
          inputs.lod_fraction = (uint8_t)selection.lod_fraction;
        }
        if (rdp->shade_triangle) {
          uint32_t channel;
          for (channel = 0u; channel < 4u; channel++) {
            int64_t value = shade_channel(rdp, channel, xcenter, ycenter,
                                          xh16, yh16, major_center);
            if (value < 0) value = 0;
            if (value > 255) value = 255;
            inputs.shade[channel] = (uint8_t)value;
          }
        }
        if (!t64_rdp_combine_pixel(machine, &inputs, &rgba)) {
          rdp->refusal_reason = T64_RDP_REFUSAL_UNSUPPORTED_OPCODE;
          continue;
        }
        if (t64_has_rdp_blender_profile(machine)) {
          uint64_t pixel =
              (uint64_t)rdp->render_y * rdp->color_image_width + x;
          uint32_t size_bytes = rdp->color_image_size == 3u ? 4u : 2u;
          uint32_t address = rdp->color_image_address +
                             (uint32_t)(pixel * size_bytes);
          if (!t64_rdp_blend_pixel(machine, &inputs, rgba, address, size_bytes,
                                   logical_coverage, &rgba,
                                   &destination_coverage, &write_color)) {
            rdp->refusal_reason = T64_RDP_REFUSAL_RENDER_RANGE;
            continue;
          }
          color = size_bytes == 2u
                      ? t64_rdp_quantize_rgba5551_v6(machine, rgba, x,
                                                     rdp->render_y)
                      : 0u;
        } else {
          color = rgba5551(rgba);
        }
      } else if (texture_triangle) {
        int64_t s = texture_channel_q16(rdp, 0u, xcenter, ycenter, xh16,
                                        yh16, major_center);
        int64_t t = texture_channel_q16(rdp, 1u, xcenter, ycenter, xh16,
                                        yh16, major_center);
        int64_t w = texture_channel_q16(rdp, 2u, xcenter, ycenter, xh16,
                                        yh16, major_center);
        texture_lod_selection selection;
        if (!select_texture_lod(rdp, s, t, w, xcenter, ycenter, xh16, yh16,
                                 major_center, dxh, &selection)) {
          rdp->refusal_reason = T64_RDP_REFUSAL_INVALID_TEXTURE;
          continue;
        }
        if (!t64_rdp_tmem_triangle_pixel(
                machine, selection.base_tile, s, t, w,
                ((rdp->other_modes >> 51) & UINT64_C(1)) != 0u, &color)) {
          rdp->refusal_reason = T64_RDP_REFUSAL_INVALID_TEXTURE;
          continue;
        }
        if (selection.next_tile != selection.base_tile) {
          uint16_t next_color;
          if (!t64_rdp_tmem_triangle_pixel(
                  machine, selection.next_tile, s, t, w,
                  ((rdp->other_modes >> 51) & UINT64_C(1)) != 0u,
                  &next_color)) {
            rdp->refusal_reason = T64_RDP_REFUSAL_INVALID_TEXTURE;
            continue;
          }
          color = blend_texture_levels(color, next_color,
                                       selection.blend_q8);
        }
        if (opcode == UINT32_C(0x0e) || opcode == UINT32_C(0x0f))
          color = modulate_texture_shade(rdp, color, xcenter, ycenter,
                                         xh16, yh16, major_center);
      } else if (rdp->shade_triangle) {
        uint32_t channel;
        uint32_t shade_rgba = 0u;
        for (channel = 0u; channel < 4u; channel++) {
          int64_t value = shade_channel(rdp, channel, xcenter, ycenter,
                                        xh16, yh16, major_center);
          if (value < 0) value = 0;
          if (value > 255) value = 255;
          shade_rgba |= (uint32_t)value << (24u - channel * 8u);
        }
        color = rgba5551(shade_rgba);
      } else {
        color = rgba5551(rdp->primitive_color);
      }
      {
        uint64_t pixel = (uint64_t)rdp->render_y * rdp->color_image_width + x;
        uint32_t size_bytes = t64_has_rdp_blender_profile(machine) &&
                                      rdp->color_image_size == 3u
                                  ? 4u
                                  : 2u;
        uint64_t address = (uint64_t)rdp->color_image_address +
                           pixel * size_bytes;
        if (z_triangle) {
          if (address > UINT32_MAX ||
              !t64_rdp_depth_evaluate_pixel(
                  machine, opcode, words, xcenter, ycenter, xh16, yh16,
                  major_center, x, rdp->render_y, (uint32_t)address,
                  size_bytes, logical_coverage, write_color, &depth_result)) {
            rdp->refusal_reason = T64_RDP_REFUSAL_RENDER_RANGE;
            continue;
          }
          have_depth_result = true;
          if (!depth_result.passed) continue;
        }
        machine->rdp_triangle_coverage[coverage_index] |= coverage_mask;
        if (t64_has_rdp_blender_profile(machine)) {
          t64_rdp_memory_coverage_invalidate(machine, (uint32_t)address,
                                             size_bytes);
          t64_rdp_memory_coverage_write(machine, (uint32_t)address,
                                        size_bytes, destination_coverage);
          if (write_color) {
            if (size_bytes == 4u) {
              unsigned alpha5 = t64_rdp_quantize_alpha5_v6(
                  machine, rgba, x, rdp->render_y);
              uint8_t alpha = (uint8_t)(((destination_coverage - 1u) << 5) |
                                        (alpha5 & 31u));
              machine->ram[(size_t)address] = (uint8_t)(rgba >> 24);
              machine->ram[(size_t)address + 1u] = (uint8_t)(rgba >> 16);
              machine->ram[(size_t)address + 2u] = (uint8_t)(rgba >> 8);
              machine->ram[(size_t)address + 3u] = alpha;
            } else {
              machine->ram[(size_t)address] = (uint8_t)(color >> 8);
              machine->ram[(size_t)address + 1u] = (uint8_t)color;
            }
          }
        } else {
          t64_rdp_memory_coverage_invalidate(machine, (uint32_t)address,
                                             size_bytes);
          machine->ram[(size_t)address] = (uint8_t)(color >> 8);
          machine->ram[(size_t)address + 1u] = (uint8_t)color;
        }
        if (have_depth_result)
          t64_rdp_depth_commit_pixel(machine, &depth_result);
      }
    }
  }
}
