/* SPDX-License-Identifier: BSD-3-Clause */
/* SPDX-FileCopyrightText: 2011-2017 The Regents of the University of California */

/*============================================================================

This C header file is part of the SoftFloat IEEE Floating-Point Arithmetic
Package, Release 3e, by John R. Hauser.

Copyright 2011, 2012, 2013, 2014, 2015, 2016, 2017 The Regents of the
University of California.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions, and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions, and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

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

#ifndef internals_h
#define internals_h 1

#include <stdbool.h>
#include <stdint.h>
#include "primitiveTypes.h"
#include "primitives.h"
#include "softfloat.h"

union ui32_f32 { uint32_t ui; float32_t f; };
union ui64_f64 { uint64_t ui; float64_t f; };

#define signF32UI(a) ((bool)((uint32_t)(a) >> 31))
#define expF32UI(a) ((int_fast16_t)(((uint32_t)(a) >> 23) & 0xffu))
#define fracF32UI(a) ((uint32_t)(a) & UINT32_C(0x007fffff))
#define packToF32UI(sign, exp, sig) \
  (((uint32_t)(sign) << 31) + ((uint32_t)(exp) << 23) + (uint32_t)(sig))
#define isNaNF32UI(a) \
  ((((~(uint32_t)(a)) & UINT32_C(0x7f800000)) == 0u) && \
   (((uint32_t)(a) & UINT32_C(0x007fffff)) != 0u))

#define signF64UI(a) ((bool)((uint64_t)(a) >> 63))
#define expF64UI(a) ((int_fast16_t)(((uint64_t)(a) >> 52) & 0x7ffu))
#define fracF64UI(a) ((uint64_t)(a) & UINT64_C(0x000fffffffffffff))
#define packToF64UI(sign, exp, sig) \
  (((uint64_t)(sign) << 63) + ((uint64_t)(exp) << 52) + (uint64_t)(sig))
#define isNaNF64UI(a) \
  ((((~(uint64_t)(a)) & UINT64_C(0x7ff0000000000000)) == 0u) && \
   (((uint64_t)(a) & UINT64_C(0x000fffffffffffff)) != 0u))

struct exp16_sig32 { int_fast16_t exp; uint_fast32_t sig; };
struct exp16_sig64 { int_fast16_t exp; uint_fast64_t sig; };

struct exp16_sig32 softfloat_normSubnormalF32Sig(t64_softfloat_context *, uint_fast32_t);
struct exp16_sig64 softfloat_normSubnormalF64Sig(t64_softfloat_context *, uint_fast64_t);
float32_t softfloat_roundPackToF32(t64_softfloat_context *, bool, int_fast16_t, uint_fast32_t);
float64_t softfloat_roundPackToF64(t64_softfloat_context *, bool, int_fast16_t, uint_fast64_t);
float32_t softfloat_normRoundPackToF32(t64_softfloat_context *, bool, int_fast16_t, uint_fast32_t);
float64_t softfloat_normRoundPackToF64(t64_softfloat_context *, bool, int_fast16_t, uint_fast64_t);
int_fast32_t softfloat_roundToI32(t64_softfloat_context *, bool, uint_fast64_t, uint_fast8_t, bool);
int_fast64_t softfloat_roundToI64(t64_softfloat_context *, bool, uint_fast64_t, uint_fast64_t, uint_fast8_t, bool);
float32_t softfloat_addMagsF32(t64_softfloat_context *, uint_fast32_t, uint_fast32_t);
float32_t softfloat_subMagsF32(t64_softfloat_context *, uint_fast32_t, uint_fast32_t);
float64_t softfloat_addMagsF64(t64_softfloat_context *, uint_fast64_t, uint_fast64_t, bool);
float64_t softfloat_subMagsF64(t64_softfloat_context *, uint_fast64_t, uint_fast64_t, bool);

#endif
