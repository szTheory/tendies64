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
                                       uint64_t x_delta_bound,
                                       uint64_t y_delta_bound) {
  unsigned channel;
  const uint64_t product_limit = (uint64_t)INT64_MAX / UINT64_C(4);

  for (channel = 0u; channel < 4u; channel++) {
    unsigned shift = 48u - channel * 16u;
    int64_t dx = pair_q16(words[5], words[7], shift);
    int64_t de = pair_q16(words[8], words[10], shift);
    int64_t dy = pair_q16(words[9], words[11], shift);
    if ((x_delta_bound != 0u &&
         (magnitude_i64(dx) > product_limit / x_delta_bound ||
          magnitude_i64(de) > product_limit / x_delta_bound)) ||
        (y_delta_bound != 0u &&
         magnitude_i64(dy) > product_limit / y_delta_bound)) return false;
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

  if (machine == NULL || words == NULL || y_begin_out == NULL ||
      y_end_out == NULL || (opcode != 0x08u && opcode != 0x0cu) ||
      machine->rdp_triangle_coverage == NULL ||
      word_count != t64_rdp_triangle_word_count(opcode)) return false;
  rdp = &machine->rdp;
  if (!rdp->color_image_valid || !rdp->scissor_valid ||
      !rdp->other_modes_valid || rdp->color_image_format != 0u ||
      rdp->color_image_size != 2u || rdp->color_image_width == 0u ||
      ((rdp->other_modes >> 52) & UINT64_C(3)) != 0u ||
      (rdp->color_image_address & UINT32_C(1)) != 0u) return false;

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
            2u;
  if (end > machine->profile.rdram_bytes) return false;

  first = floor_div_i64(yh16, INT64_C(65536));
  last = floor_div_i64(yl16 + INT64_C(65535), INT64_C(65536)) - 1;
  if (first < (int64_t)y0) first = (int64_t)y0;
  if (last >= (int64_t)y1) last = (int64_t)y1 - 1;
  if (first > last) {
    *y_begin_out = 1u;
    *y_end_out = 0u;
    return true;
  }
  if (opcode == 0x0cu) {
    uint64_t y_delta_bound = (uint64_t)(yl16 - yh16) + UINT64_C(65536);
    uint64_t maximum_slope = UINT64_C(0x80000000);
    uint64_t x_delta_bound = UINT64_C(0x80000000) +
        (maximum_slope * y_delta_bound) / UINT64_C(65536) +
        UINT64_C(1024) * UINT64_C(65536);
    if (!shade_products_are_bounded(words, x_delta_bound, y_delta_bound))
      return false;
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

void t64_rdp_triangle_render_row(t64_machine *machine) {
  static const int64_t sample[4] = {INT64_C(8192), INT64_C(24576),
                                    INT64_C(40960), INT64_C(57344)};
  t64_rdp_state *rdp = &machine->rdp;
  const uint64_t *words = rdp->pending_triangle_words;
  int64_t yh16, ym16, yl16, xh16, xm16, xl16, dxh, dxm, dxl;
  uint32_t x, x0 = rdp->scissor_xh >> 2, x1 = rdp->scissor_xl >> 2;
  uint32_t max_x = rdp->color_image_width;

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
      machine->rdp_triangle_coverage[coverage_index] |= coverage_mask;
      if (rdp->shade_triangle) {
        uint32_t channel;
        uint32_t rgba = 0u;
        int64_t xcenter = (int64_t)x * INT64_C(65536) + INT64_C(32768);
        for (channel = 0u; channel < 4u; channel++) {
          int64_t value = shade_channel(rdp, channel, xcenter, ycenter,
                                        xh16, yh16, major_center);
          if (value < 0) value = 0;
          if (value > 255) value = 255;
          rgba |= (uint32_t)value << (24u - channel * 8u);
        }
        color = rgba5551(rgba);
      } else {
        color = rgba5551(rdp->primitive_color);
      }
      {
        uint64_t pixel = (uint64_t)rdp->render_y * rdp->color_image_width + x;
        uint64_t address = (uint64_t)rdp->color_image_address + pixel * 2u;
        machine->ram[(size_t)address] = (uint8_t)(color >> 8);
        machine->ram[(size_t)address + 1u] = (uint8_t)color;
      }
    }
  }
}
