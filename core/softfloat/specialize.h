#ifndef T64_SOFTFLOAT_SPECIALIZE_H
#define T64_SOFTFLOAT_SPECIALIZE_H

#include <stdbool.h>
#include <stdint.h>

#include "softfloat.h"

#define i32_fromPosOverflow (-INT32_C(0x7fffffff) - 1)
#define i32_fromNegOverflow (-INT32_C(0x7fffffff) - 1)
#define i32_fromNaN (-INT32_C(0x7fffffff) - 1)
#define i64_fromPosOverflow (-INT64_C(0x7fffffffffffffff) - 1)
#define i64_fromNegOverflow (-INT64_C(0x7fffffffffffffff) - 1)
#define i64_fromNaN (-INT64_C(0x7fffffffffffffff) - 1)

#define defaultNaNF32UI UINT32_C(0x7fc00000)
#define defaultNaNF64UI UINT64_C(0x7ff8000000000000)
#define softfloat_isSigNaNF32UI(uiA) \
  ((((uiA) & UINT32_C(0x7fc00000)) == UINT32_C(0x7f800000)) && \
   (((uiA) & UINT32_C(0x003fffff)) != 0u))
#define softfloat_isSigNaNF64UI(uiA) \
  ((((uiA) & UINT64_C(0x7ff8000000000000)) == \
    UINT64_C(0x7ff0000000000000)) && \
   (((uiA) & UINT64_C(0x0007ffffffffffff)) != 0u))

struct commonNaN {
  bool sign;
  uint64_t payload;
};

static inline uint_fast32_t softfloat_propagateNaNF32UI(
    t64_softfloat_context *context, uint_fast32_t uiA, uint_fast32_t uiB) {
  uint32_t selected = (uint32_t)uiA;

  if (softfloat_isSigNaNF32UI(uiA) || softfloat_isSigNaNF32UI(uiB)) {
    softfloat_raiseFlags(context, softfloat_flag_invalid);
  }
  if ((((uint32_t)uiA & UINT32_C(0x7f800000)) != UINT32_C(0x7f800000)) ||
      (((uint32_t)uiA & UINT32_C(0x007fffff)) == 0u)) {
    selected = (uint32_t)uiB;
  }
  if (((selected & UINT32_C(0x7f800000)) != UINT32_C(0x7f800000)) ||
      ((selected & UINT32_C(0x007fffff)) == 0u)) {
    return defaultNaNF32UI;
  }
  return selected | UINT32_C(0x00400000);
}

static inline uint_fast64_t softfloat_propagateNaNF64UI(
    t64_softfloat_context *context, uint_fast64_t uiA, uint_fast64_t uiB) {
  uint64_t selected = (uint64_t)uiA;

  if (softfloat_isSigNaNF64UI(uiA) || softfloat_isSigNaNF64UI(uiB)) {
    softfloat_raiseFlags(context, softfloat_flag_invalid);
  }
  if ((((uint64_t)uiA & UINT64_C(0x7ff0000000000000)) !=
       UINT64_C(0x7ff0000000000000)) ||
      (((uint64_t)uiA & UINT64_C(0x000fffffffffffff)) == 0u)) {
    selected = (uint64_t)uiB;
  }
  if (((selected & UINT64_C(0x7ff0000000000000)) !=
       UINT64_C(0x7ff0000000000000)) ||
      ((selected & UINT64_C(0x000fffffffffffff)) == 0u)) {
    return defaultNaNF64UI;
  }
  return selected | UINT64_C(0x0008000000000000);
}

static inline void softfloat_f32UIToCommonNaN(
    t64_softfloat_context *context, uint_fast32_t uiA,
    struct commonNaN *common) {
  uint32_t bits = (uint32_t)uiA;

  if (softfloat_isSigNaNF32UI(bits)) {
    softfloat_raiseFlags(context, softfloat_flag_invalid);
  }
  common->sign = (bits >> 31) != 0u;
  common->payload = (uint64_t)((bits & UINT32_C(0x007fffff)) |
                               UINT32_C(0x00400000)) << 41;
}

static inline void softfloat_f64UIToCommonNaN(
    t64_softfloat_context *context, uint_fast64_t uiA,
    struct commonNaN *common) {
  uint64_t bits = (uint64_t)uiA;

  if (softfloat_isSigNaNF64UI(bits)) {
    softfloat_raiseFlags(context, softfloat_flag_invalid);
  }
  common->sign = (bits >> 63) != 0u;
  common->payload = ((bits & UINT64_C(0x000fffffffffffff)) |
                     UINT64_C(0x0008000000000000)) << 12;
}

static inline uint_fast32_t softfloat_commonNaNToF32UI(
    const struct commonNaN *common) {
  return (common->sign ? UINT32_C(0x80000000) : 0u) |
         UINT32_C(0x7f800000) | (uint32_t)(common->payload >> 41) |
         UINT32_C(0x00400000);
}

static inline uint_fast64_t softfloat_commonNaNToF64UI(
    const struct commonNaN *common) {
  return (common->sign ? UINT64_C(0x8000000000000000) : 0u) |
         UINT64_C(0x7ff0000000000000) | (common->payload >> 12) |
         UINT64_C(0x0008000000000000);
}

#endif
