/* SPDX-License-Identifier: BSD-3-Clause */
/* SPDX-FileCopyrightText: 2011-2016 The Regents of the University of California */
/*============================================================================

This C source file is part of the SoftFloat IEEE Floating-Point Arithmetic
Package, Release 3e, by John R. Hauser.

Copyright 2011, 2012, 2013, 2014, 2015, 2016 The Regents of the University of
California.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions, and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice
    this list of conditions, and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 3. Neither the name of the University nor the names of its contributors may
    be used to endorse or promote products derived from this software without
    specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS", AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ARE
DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=============================================================================*/

#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>

static uint_fast8_t t64_count_leading_zeros32(uint32_t value) {
  uint_fast8_t count = 0u;

  while (value != 0u && (value & UINT32_C(0x80000000)) == 0u) {
    value <<= 1;
    count++;
  }
  return count;
}

t64_status t64_softfloat_i32_to_f64(int32_t input,
                                    t64_softfloat_context *context,
                                    t64_softfloat_result *result_out) {
  t64_softfloat_result result = {0u, 0u};
  uint32_t input_bits;
  uint32_t magnitude;
  uint_fast8_t shift_distance;
  uint_fast32_t exponent;
  bool sign;

  if (context == NULL || result_out == NULL || context->rounding_mode > 3u ||
      context->tininess_mode > 1u) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (input != 0) {
    input_bits = (uint32_t)input;
    sign = input < 0;
    magnitude = sign ? UINT32_C(0) - input_bits : input_bits;
    shift_distance = t64_count_leading_zeros32(magnitude) + 21u;
    exponent = UINT32_C(0x432) - shift_distance;
    result.bits = (sign ? UINT64_C(0x8000000000000000) : UINT64_C(0)) +
                  ((uint64_t)exponent << 52) +
                  ((uint64_t)magnitude << shift_distance);
  }
  result.exception_flags = context->exception_flags;
  *result_out = result;
  return T64_STATUS_OK;
}
