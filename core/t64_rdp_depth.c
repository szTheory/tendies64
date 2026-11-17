#include "t64_internal.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

static bool is_z_triangle(uint32_t opcode) {
  return opcode == UINT32_C(0x09) || opcode == UINT32_C(0x0b) ||
         opcode == UINT32_C(0x0d) || opcode == UINT32_C(0x0f);
}

static int64_t signed_i32(uint32_t value) {
  return (value & UINT32_C(0x80000000)) != 0u
             ? (int64_t)value - INT64_C(0x100000000)
             : (int64_t)value;
}

static int64_t signed_pair_q16(uint32_t value) {
  return signed_i32(value);
}

static uint64_t magnitude_i64(int64_t value) {
  return value < 0 ? (uint64_t)(-(value + INT64_C(1))) + UINT64_C(1)
                   : (uint64_t)value;
}

static int64_t floor_div_i64(int64_t numerator, int64_t denominator) {
  int64_t quotient = numerator / denominator;
  if (numerator % denominator < 0) quotient--;
  return quotient;
}

static int64_t pair_from_word(uint64_t word, unsigned shift) {
  uint32_t pair = (uint32_t)(word >> shift);
  return signed_pair_q16(pair);
}

static uint32_t z_delta_priority_code(uint32_t delta) {
  uint32_t code = 0u;
  if (delta == 0u) return 0u;
  while (delta > 1u) {
    delta >>= 1;
    code++;
  }
  return code > 15u ? 15u : code;
}

static uint32_t z_delta_decode(uint32_t code) {
  return code == 0u ? 1u : UINT32_C(1) << code;
}

static uint16_t z_compress(uint32_t linear_z, uint32_t delta_code) {
  static const uint32_t start[8] = {
      UINT32_C(0x00000), UINT32_C(0x20000), UINT32_C(0x30000),
      UINT32_C(0x38000), UINT32_C(0x3c000), UINT32_C(0x3e000),
      UINT32_C(0x3f000), UINT32_C(0x3f800)};
  static const uint8_t shift[8] = {6u, 5u, 4u, 3u, 2u, 1u, 0u, 0u};
  uint32_t exponent = 0u;
  uint32_t mantissa;

  while (exponent < 7u && linear_z >= start[exponent + 1u]) exponent++;
  mantissa = (linear_z - start[exponent]) >> shift[exponent];
  if (mantissa > UINT32_C(0x7ff)) mantissa = UINT32_C(0x7ff);
  return (uint16_t)((exponent << 13) | (mantissa << 2) |
                    ((delta_code >> 2) & UINT32_C(3)));
}

static uint32_t z_decompress(uint16_t visible_word) {
  static const uint32_t add[8] = {
      UINT32_C(0x00000), UINT32_C(0x20000), UINT32_C(0x30000),
      UINT32_C(0x38000), UINT32_C(0x3c000), UINT32_C(0x3e000),
      UINT32_C(0x3f000), UINT32_C(0x3f800)};
  static const uint8_t shift[8] = {6u, 5u, 4u, 3u, 2u, 1u, 0u, 0u};
  uint32_t exponent = (visible_word >> 13) & UINT16_C(7);
  uint32_t mantissa = (visible_word >> 2) & UINT16_C(0x7ff);
  return (mantissa << shift[exponent]) + add[exponent];
}

static uint32_t depth_word_read(const uint8_t *bytes) {
  return ((uint32_t)bytes[0] << 8) | (uint32_t)bytes[1];
}

bool t64_rdp_depth_triangle_preflight(const t64_machine *machine,
                                      uint32_t opcode, const uint64_t *words,
                                      size_t word_count, uint32_t x0,
                                      uint32_t x1, uint32_t y_begin,
                                      uint32_t y_end) {
  const t64_rdp_state *rdp;
  uint64_t y_bound, edge_bound, x_bound;
  uint64_t abs_dx, abs_de, abs_dy;
  uint64_t depth_end;
  uint32_t mode;
  uint32_t offset;
  int64_t yh16, yl16, xh16, dxh;
  int64_t z_base, dzdx, dzde, dzdy;
  const uint64_t product_limit = (uint64_t)INT64_MAX / UINT64_C(4);

  if (!is_z_triangle(opcode)) return true;
  if (machine == NULL || words == NULL ||
      !t64_has_rdp_depth_profile(machine) ||
      word_count != t64_rdp_triangle_word_count(opcode) ||
      machine->rdp_depth_hidden == NULL || machine->ram == NULL)
    return false;
  rdp = &machine->rdp;
  if (!rdp->depth_image_valid ||
      (rdp->depth_image_address & UINT32_C(7)) != 0u ||
      rdp->color_image_width == 0u)
    return false;
  mode = (uint32_t)((rdp->other_modes >> 2) & UINT64_C(1));
  if (mode != 0u && !rdp->primitive_depth_valid) return false;
  if (x0 >= x1 || y_begin > y_end) return true;
  depth_end = (uint64_t)rdp->depth_image_address +
              ((uint64_t)y_end * rdp->color_image_width + x1) * UINT64_C(2);
  if (depth_end > machine->profile.rdram_bytes) return false;

  yh16 = signed_i32((uint32_t)((words[0] & UINT64_C(0x3fff)) << 18)) /
         INT64_C(4);
  yl16 = signed_i32((uint32_t)(((words[0] >> 32) & UINT64_C(0x3fff)) << 18)) /
         INT64_C(4);
  xh16 = signed_i32((uint32_t)(words[2] >> 32));
  dxh = signed_i32((uint32_t)words[2]);
  y_bound = magnitude_i64(yl16 - yh16) + UINT64_C(65536);
  abs_dx = magnitude_i64(dxh);
  if (y_bound != 0u && abs_dx > (uint64_t)INT64_MAX / y_bound)
    return false;
  edge_bound = (abs_dx * y_bound) / UINT64_C(65536) + UINT64_C(1);
  x_bound = magnitude_i64((int64_t)x0 * INT64_C(65536) - xh16);
  if (magnitude_i64((int64_t)x1 * INT64_C(65536) - xh16) > x_bound)
    x_bound = magnitude_i64((int64_t)x1 * INT64_C(65536) - xh16);
  if (x_bound > UINT64_MAX - edge_bound - UINT64_C(65536)) return false;
  x_bound += edge_bound + UINT64_C(65536);

  offset = (uint32_t)word_count - 2u;
  z_base = pair_from_word(words[offset], 32u);
  dzdx = pair_from_word(words[offset], 0u);
  dzde = pair_from_word(words[offset + 1u], 32u);
  dzdy = pair_from_word(words[offset + 1u], 0u);
  abs_dx = magnitude_i64(dzdx);
  abs_de = magnitude_i64(dzde);
  abs_dy = magnitude_i64(dzdy);
  if ((x_bound != 0u && abs_dx > product_limit / x_bound) ||
      (edge_bound != 0u && abs_de > product_limit / edge_bound) ||
      (y_bound != 0u && abs_dy > product_limit / y_bound))
    return false;
  (void)z_base;
  return true;
}

bool t64_rdp_depth_evaluate_pixel(
    const t64_machine *machine, uint32_t opcode, const uint64_t *words,
    int64_t xcenter, int64_t ycenter, int64_t xh16, int64_t yh16,
    int64_t major_center, uint32_t x, uint32_t y, uint32_t color_address,
    uint32_t color_size_bytes, uint8_t incoming_coverage, bool color_write,
    t64_rdp_depth_pixel_result *result_out) {
  const t64_rdp_state *rdp;
  uint64_t pixel_index;
  uint64_t depth_address;
  uint32_t old_delta_code;
  uint32_t old_delta;
  uint32_t new_delta;
  uint32_t tolerance;
  uint32_t z_mode;
  uint32_t linear_z;
  uint32_t delta_code;
  uint16_t old_visible;
  uint8_t old_hidden;
  uint32_t old_z;
  bool passed = true;
  bool update;

  if (machine == NULL || words == NULL || result_out == NULL ||
      !is_z_triangle(opcode) || !t64_has_rdp_depth_profile(machine) ||
      machine->ram == NULL || machine->rdp_depth_hidden == NULL ||
      incoming_coverage > 8u ||
      (color_size_bytes != 2u && color_size_bytes != 4u))
    return false;
  rdp = &machine->rdp;
  pixel_index = (uint64_t)y * rdp->color_image_width + x;
  depth_address = (uint64_t)rdp->depth_image_address + pixel_index * 2u;
  if (!rdp->depth_image_valid ||
      (uint64_t)depth_address + 2u > machine->profile.rdram_bytes ||
      depth_address > UINT32_MAX || !rdp->other_modes_valid)
    return false;

  if (((rdp->other_modes >> 2) & UINT64_C(1)) != 0u) {
    uint32_t raw_z = rdp->primitive_z;
    if (!rdp->primitive_depth_valid) return false;
    if (raw_z > UINT32_C(0x7fff)) raw_z = UINT32_C(0x7fff);
    linear_z = raw_z << 3;
    new_delta = rdp->primitive_dz;
  } else {
    uint32_t offset = (uint32_t)t64_rdp_triangle_word_count(opcode) - 2u;
    int64_t z_base = pair_from_word(words[offset], 32u);
    int64_t dzdx = pair_from_word(words[offset], 0u);
    int64_t dzde = pair_from_word(words[offset + 1u], 32u);
    int64_t dzdy = pair_from_word(words[offset + 1u], 0u);
    int64_t dx_delta = xcenter - major_center;
    int64_t de_delta = major_center - xh16;
    int64_t dy_delta = ycenter - yh16;
    int64_t q16_z;
    int64_t product_x = dzdx * dx_delta;
    int64_t product_e = dzde * de_delta;
    int64_t product_y = dzdy * dy_delta;
    uint64_t delta_sum = magnitude_i64(dzdx) + magnitude_i64(dzdy);
    int64_t converted;

    q16_z = z_base + floor_div_i64(product_x, INT64_C(65536)) +
            floor_div_i64(product_e, INT64_C(65536)) +
            floor_div_i64(product_y, INT64_C(65536));
    converted = floor_div_i64(q16_z, INT64_C(8192));
    if (converted < 0) converted = 0;
    if (converted > INT64_C(0x3ffff)) converted = INT64_C(0x3ffff);
    linear_z = (uint32_t)converted;
    delta_sum = (delta_sum + UINT64_C(8191)) / UINT64_C(8192);
    if (delta_sum > UINT16_MAX) delta_sum = UINT16_MAX;
    new_delta = (uint32_t)delta_sum;
  }
  delta_code = z_delta_priority_code(new_delta);
  old_visible = (uint16_t)depth_word_read(machine->ram + (size_t)depth_address);
  old_hidden = machine->rdp_depth_hidden[(size_t)depth_address / 2u];
  old_delta_code = ((uint32_t)old_visible & UINT32_C(3)) << 2;
  if ((old_hidden & UINT8_C(0x80)) != 0u)
    old_delta_code |= (uint32_t)(old_hidden & UINT8_C(3));
  old_delta = z_delta_decode(old_delta_code);
  tolerance = new_delta > old_delta ? new_delta : old_delta;
  old_z = z_decompress(old_visible);
  z_mode = (uint32_t)((rdp->other_modes >> 10) & UINT64_C(3));
  if (((rdp->other_modes >> 4) & UINT64_C(1)) != 0u) {
    if (z_mode == 3u) {
      uint32_t difference = linear_z > old_z ? linear_z - old_z
                                             : old_z - linear_z;
      passed = difference <= tolerance;
    } else {
      uint32_t threshold = old_z > UINT32_C(0x3ffff) - tolerance
                               ? UINT32_C(0x3ffff)
                               : old_z + tolerance;
      passed = linear_z <= threshold;
      if (z_mode == 1u &&
          incoming_coverage > t64_rdp_memory_coverage_read(
                                  machine, color_address, color_size_bytes))
        passed = true;
    }
  }
  update = passed && color_write &&
           ((rdp->other_modes >> 5) & UINT64_C(1)) != 0u && z_mode < 2u;
  result_out->address = (uint32_t)depth_address;
  result_out->visible_word = z_compress(linear_z, delta_code);
  result_out->hidden_value =
      (uint8_t)(UINT8_C(0x80) | (uint8_t)(delta_code & UINT32_C(3)));
  result_out->passed = passed;
  result_out->update = update;
  (void)x;
  return true;
}

void t64_rdp_depth_commit_pixel(t64_machine *machine,
                                const t64_rdp_depth_pixel_result *result) {
  size_t granule;

  if (machine == NULL || result == NULL || !result->passed ||
      !result->update || machine->ram == NULL ||
      machine->rdp_depth_hidden == NULL ||
      (uint64_t)result->address + 2u > machine->profile.rdram_bytes)
    return;
  granule = (size_t)result->address / 2u;
  t64_rdp_memory_coverage_invalidate(machine, result->address, 2u);
  machine->ram[result->address] = (uint8_t)(result->visible_word >> 8);
  machine->ram[result->address + 1u] = (uint8_t)result->visible_word;
  machine->rdp_depth_hidden[granule] = result->hidden_value;
}
