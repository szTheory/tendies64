/* SPDX-License-Identifier: ISC */
/* SPDX-FileCopyrightText: 2004-2025 ares team, Near et al */

/*
 * Adapted integer divider equations from ares. See
 * docs/evidence/rsp-divider-provenance.md for exact paths, scope, and limits.
 */
#include "t64_rsp_divider.h"

uint16_t t64_rsp_divider_reciprocal_word(uint32_t index) {
  uint64_t denominator;
  uint64_t quotient;

  if (index == 0u) {
    return UINT16_MAX;
  }
  if (index >= UINT32_C(512)) {
    return UINT16_C(0);
  }
  denominator = (uint64_t)index + UINT64_C(512);
  quotient = (UINT64_C(1) << 34) / denominator;
  return (uint16_t)((quotient + UINT64_C(1)) >> 8);
}

uint16_t t64_rsp_divider_rsqrt_word(uint32_t index) {
  uint64_t a;
  uint64_t low = UINT64_C(0);
  uint64_t high = UINT64_C(1) << 19;
  const uint64_t limit = UINT64_C(1) << 44;

  if (index >= UINT32_C(512)) {
    return UINT16_C(0);
  }
  a = ((uint64_t)index + UINT64_C(512)) >> (index & UINT32_C(1));
  while (high - low > UINT64_C(1)) {
    uint64_t middle = low + (high - low) / UINT64_C(2);
    uint64_t square = middle * middle;

    if (a * square < limit) {
      low = middle;
    } else {
      high = middle;
    }
  }
  return (uint16_t)(low >> 1);
}

static uint32_t leading_zero_count(uint32_t value) {
  uint32_t count = 0u;
  uint32_t bit = UINT32_C(0x80000000);

  while ((value & bit) == 0u) {
    count++;
    bit >>= 1;
  }
  return count;
}

uint32_t t64_rsp_divider_result(bool inverse_square_root, uint32_t input) {
  uint32_t sign_mask =
      (input & UINT32_C(0x80000000)) != 0u ? UINT32_MAX : UINT32_C(0);
  uint32_t data = input ^ sign_mask;
  uint32_t shift;
  uint32_t index;
  uint32_t result;
  uint32_t right_shift;
  uint16_t table_word;

  if (input == 0u) {
    return UINT32_C(0x7fffffff);
  }
  if (input == UINT32_C(0xffff8000)) {
    return UINT32_C(0xffff0000);
  }
  if (sign_mask != 0u && input > UINT32_C(0xffff8000)) {
    data++;
  }
  shift = leading_zero_count(data);
  index = (uint32_t)(((uint64_t)data << shift & UINT64_C(0x7fc00000)) >> 22);
  if (inverse_square_root) {
    index = (index & UINT32_C(0x1fe)) | (shift & UINT32_C(1));
    table_word = t64_rsp_divider_rsqrt_word(index);
    right_shift = (UINT32_C(31) - shift) >> 1;
  } else {
    table_word = t64_rsp_divider_reciprocal_word(index);
    right_shift = UINT32_C(31) - shift;
  }
  result = (UINT32_C(0x10000) | (uint32_t)table_word) << 14;
  result >>= right_shift;
  return result ^ sign_mask;
}
