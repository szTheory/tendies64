#include "t64_internal.h"
#include "t64_test.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void check_operation(const char *label, t64_softfloat_operation op,
                            uint64_t a, uint64_t b, uint8_t rounding,
                            uint8_t tininess, uint8_t initial_flags,
                            t64_status expected_status, uint64_t expected_bits,
                            uint8_t expected_flags) {
  t64_softfloat_context context = {rounding, tininess, initial_flags};
  t64_softfloat_result result = {UINT64_C(0xa5a5a5a5a5a5a5a5), UINT8_C(0xff)};
  t64_status status =
      t64_softfloat_execute(op, &context, a, b, &result);

  if (status != expected_status || result.bits != expected_bits ||
      result.exception_flags != expected_flags) {
    (void)printf("# %s status=%u/%u bits=%016llx/%016llx flags=%02x/%02x\n",
                 label, (unsigned int)status, (unsigned int)expected_status,
                 (unsigned long long)result.bits,
                 (unsigned long long)expected_bits,
                 (unsigned int)result.exception_flags,
                 (unsigned int)expected_flags);
  }
  T64_CHECK_INT(status, expected_status);
  T64_CHECK(result.bits == expected_bits);
  T64_CHECK_INT(result.exception_flags, expected_flags);
}

enum {
  MATRIX_BASE = 1u << 0,
  MATRIX_TIE = 1u << 1,
  MATRIX_NEIGHBOR = 1u << 2,
  MATRIX_ZERO_SIGN = 1u << 3,
  MATRIX_SUBNORMAL = 1u << 4,
  MATRIX_INFINITY = 1u << 5,
  MATRIX_QNAN = 1u << 6,
  MATRIX_SNAN = 1u << 7,
  MATRIX_RANGE = 1u << 8,
  MATRIX_OVERFLOW = 1u << 9,
  MATRIX_UNDERFLOW = 1u << 10,
  MATRIX_INEXACT = 1u << 11,
  MATRIX_DIVZERO = 1u << 12,
  MATRIX_INVALID = 1u << 13,
  MATRIX_ROUNDING_SENSITIVE = 1u << 14
};

enum {
  MATRIX_SOURCE_BITS = 1u,
  MATRIX_SOURCE_RATIONAL = 2u,
  MATRIX_SOURCE_INTEGER_INTERVAL = 3u
};

typedef struct {
  const char *id;
  uint8_t format;
  t64_softfloat_operation operation;
  uint64_t a;
  uint64_t b;
  uint16_t categories;
  uint8_t expected_source;
  uint64_t expected[4];
  uint8_t flags[4];
} softfloat_matrix_vector;

#define MATRIX_SAME(id_, format_, operation_, a_, b_, categories_, source_,  \
                    bits_, flags_)                                           \
  {                                                                           \
    id_, format_, operation_, a_, b_, categories_, source_,                   \
        {bits_, bits_, bits_, bits_}, {flags_, flags_, flags_, flags_}        \
  }
#define MATRIX_MODES(id_, format_, operation_, a_, b_, categories_, source_, \
                     e0_, e1_, e2_, e3_, f0_, f1_, f2_, f3_)                  \
  {                                                                           \
    id_, format_, operation_, a_, b_, categories_, source_,                   \
        {e0_, e1_, e2_, e3_}, {f0_, f1_, f2_, f3_}                            \
  }

/* Each entry is a named vector family evaluated under RM0..RM3. Expected
   values are constants derived in the audit below the closure inventory. */
static const softfloat_matrix_vector softfloat_matrix[] = {
    MATRIX_SAME("base-i32-f64", 64u, T64_SOFTFLOAT_OP_I32_TO_F64, 3u, 0u,
                MATRIX_BASE, MATRIX_SOURCE_BITS,
                UINT64_C(0x4008000000000000), 0u),
    MATRIX_SAME("base-f32-add", 32u, T64_SOFTFLOAT_OP_F32_ADD,
                UINT32_C(0x3f800000), UINT32_C(0x40000000), MATRIX_BASE,
                MATRIX_SOURCE_BITS, UINT32_C(0x40400000), 0u),
    MATRIX_SAME("base-f32-sub", 32u, T64_SOFTFLOAT_OP_F32_SUB,
                UINT32_C(0x40400000), UINT32_C(0x3f800000), MATRIX_BASE,
                MATRIX_SOURCE_BITS, UINT32_C(0x40000000), 0u),
    MATRIX_SAME("base-f32-mul", 32u, T64_SOFTFLOAT_OP_F32_MUL,
                UINT32_C(0x3fc00000), UINT32_C(0x40000000), MATRIX_BASE,
                MATRIX_SOURCE_BITS, UINT32_C(0x40400000), 0u),
    MATRIX_SAME("base-f32-div", 32u, T64_SOFTFLOAT_OP_F32_DIV,
                UINT32_C(0x40c00000), UINT32_C(0x40000000), MATRIX_BASE,
                MATRIX_SOURCE_RATIONAL, UINT32_C(0x40400000), 0u),
    MATRIX_SAME("base-f32-sqrt", 32u, T64_SOFTFLOAT_OP_F32_SQRT,
                UINT32_C(0x40800000), 0u, MATRIX_BASE, MATRIX_SOURCE_BITS,
                UINT32_C(0x40000000), 0u),
    MATRIX_SAME("base-f32-eq", 32u, T64_SOFTFLOAT_OP_F32_EQ,
                UINT32_C(0x3f800000), UINT32_C(0x3f800000), MATRIX_BASE,
                MATRIX_SOURCE_BITS, 1u, 0u),
    MATRIX_SAME("base-f32-le", 32u, T64_SOFTFLOAT_OP_F32_LE,
                UINT32_C(0x3f800000), UINT32_C(0x40000000), MATRIX_BASE,
                MATRIX_SOURCE_BITS, 1u, 0u),
    MATRIX_SAME("base-f32-lt", 32u, T64_SOFTFLOAT_OP_F32_LT,
                UINT32_C(0x3f800000), UINT32_C(0x40000000), MATRIX_BASE,
                MATRIX_SOURCE_BITS, 1u, 0u),
    MATRIX_SAME("base-f32-f64", 32u, T64_SOFTFLOAT_OP_F32_TO_F64,
                UINT32_C(0x3fc00000), 0u, MATRIX_BASE, MATRIX_SOURCE_BITS,
                UINT64_C(0x3ff8000000000000), 0u),
    MATRIX_SAME("base-f32-i32", 32u, T64_SOFTFLOAT_OP_F32_TO_I32,
                UINT32_C(0x40000000), 0u, MATRIX_BASE, MATRIX_SOURCE_BITS,
                2u, 0u),
    MATRIX_SAME("base-f32-i64", 32u, T64_SOFTFLOAT_OP_F32_TO_I64,
                UINT32_C(0x40000000), 0u, MATRIX_BASE, MATRIX_SOURCE_BITS,
                2u, 0u),
    MATRIX_SAME("base-f64-add", 64u, T64_SOFTFLOAT_OP_F64_ADD,
                UINT64_C(0x3ff0000000000000),
                UINT64_C(0x4000000000000000), MATRIX_BASE,
                MATRIX_SOURCE_BITS, UINT64_C(0x4008000000000000), 0u),
    MATRIX_SAME("base-f64-sub", 64u, T64_SOFTFLOAT_OP_F64_SUB,
                UINT64_C(0x4008000000000000),
                UINT64_C(0x3ff0000000000000), MATRIX_BASE,
                MATRIX_SOURCE_BITS, UINT64_C(0x4000000000000000), 0u),
    MATRIX_SAME("base-f64-mul", 64u, T64_SOFTFLOAT_OP_F64_MUL,
                UINT64_C(0x3ff8000000000000),
                UINT64_C(0x4000000000000000), MATRIX_BASE,
                MATRIX_SOURCE_BITS, UINT64_C(0x4008000000000000), 0u),
    MATRIX_SAME("base-f64-div", 64u, T64_SOFTFLOAT_OP_F64_DIV,
                UINT64_C(0x4018000000000000),
                UINT64_C(0x4000000000000000), MATRIX_BASE,
                MATRIX_SOURCE_RATIONAL, UINT64_C(0x4008000000000000), 0u),
    MATRIX_SAME("base-f64-sqrt", 64u, T64_SOFTFLOAT_OP_F64_SQRT,
                UINT64_C(0x4010000000000000), 0u, MATRIX_BASE,
                MATRIX_SOURCE_BITS, UINT64_C(0x4000000000000000), 0u),
    MATRIX_SAME("base-f64-eq", 64u, T64_SOFTFLOAT_OP_F64_EQ,
                UINT64_C(0x3ff0000000000000),
                UINT64_C(0x3ff0000000000000), MATRIX_BASE,
                MATRIX_SOURCE_BITS, 1u, 0u),
    MATRIX_SAME("base-f64-le", 64u, T64_SOFTFLOAT_OP_F64_LE,
                UINT64_C(0x3ff0000000000000),
                UINT64_C(0x4000000000000000), MATRIX_BASE,
                MATRIX_SOURCE_BITS, 1u, 0u),
    MATRIX_SAME("base-f64-lt", 64u, T64_SOFTFLOAT_OP_F64_LT,
                UINT64_C(0x3ff0000000000000),
                UINT64_C(0x4000000000000000), MATRIX_BASE,
                MATRIX_SOURCE_BITS, 1u, 0u),
    MATRIX_SAME("base-f64-f32", 64u, T64_SOFTFLOAT_OP_F64_TO_F32,
                UINT64_C(0x3ff8000000000000), 0u, MATRIX_BASE,
                MATRIX_SOURCE_BITS, UINT32_C(0x3fc00000), 0u),
    MATRIX_SAME("base-f64-i32", 64u, T64_SOFTFLOAT_OP_F64_TO_I32,
                UINT64_C(0x4000000000000000), 0u, MATRIX_BASE,
                MATRIX_SOURCE_BITS, 2u, 0u),
    MATRIX_SAME("base-f64-i64", 64u, T64_SOFTFLOAT_OP_F64_TO_I64,
                UINT64_C(0x4000000000000000), 0u, MATRIX_BASE,
                MATRIX_SOURCE_BITS, 2u, 0u),
    MATRIX_SAME("base-i32-f32", 32u, T64_SOFTFLOAT_OP_I32_TO_F32, 3u, 0u,
                MATRIX_BASE, MATRIX_SOURCE_BITS, UINT32_C(0x40400000), 0u),
    MATRIX_SAME("base-i64-f32", 32u, T64_SOFTFLOAT_OP_I64_TO_F32, 3u, 0u,
                MATRIX_BASE, MATRIX_SOURCE_BITS, UINT32_C(0x40400000), 0u),
    MATRIX_SAME("base-i64-f64", 64u, T64_SOFTFLOAT_OP_I64_TO_F64, 3u, 0u,
                MATRIX_BASE, MATRIX_SOURCE_BITS,
                UINT64_C(0x4008000000000000), 0u),

    MATRIX_MODES("f32-add-half-ulp-positive", 32u, T64_SOFTFLOAT_OP_F32_ADD,
                 UINT32_C(0x3f800000), UINT32_C(0x33800000),
                 MATRIX_TIE | MATRIX_INEXACT | MATRIX_ROUNDING_SENSITIVE,
                 MATRIX_SOURCE_RATIONAL,
                 UINT32_C(0x3f800000), UINT32_C(0x3f800000),
                 UINT32_C(0x3f800000), UINT32_C(0x3f800001), 1u, 1u, 1u, 1u),
    MATRIX_MODES("f32-add-half-ulp-below", 32u, T64_SOFTFLOAT_OP_F32_ADD,
                 UINT32_C(0x3f800000), UINT32_C(0x337fffff),
                 MATRIX_NEIGHBOR | MATRIX_INEXACT, MATRIX_SOURCE_RATIONAL,
                 UINT32_C(0x3f800000), UINT32_C(0x3f800000),
                 UINT32_C(0x3f800000), UINT32_C(0x3f800001), 1u, 1u, 1u, 1u),
    MATRIX_MODES("f32-add-half-ulp-above", 32u, T64_SOFTFLOAT_OP_F32_ADD,
                 UINT32_C(0x3f800000), UINT32_C(0x33800001),
                 MATRIX_NEIGHBOR | MATRIX_INEXACT, MATRIX_SOURCE_RATIONAL,
                 UINT32_C(0x3f800001), UINT32_C(0x3f800000),
                 UINT32_C(0x3f800000), UINT32_C(0x3f800001), 1u, 1u, 1u, 1u),
    MATRIX_MODES("f64-add-half-ulp-positive", 64u, T64_SOFTFLOAT_OP_F64_ADD,
                 UINT64_C(0x3ff0000000000000),
                 UINT64_C(0x3ca0000000000000),
                 MATRIX_TIE | MATRIX_INEXACT | MATRIX_ROUNDING_SENSITIVE,
                 MATRIX_SOURCE_RATIONAL, UINT64_C(0x3ff0000000000000),
                 UINT64_C(0x3ff0000000000000),
                 UINT64_C(0x3ff0000000000000),
                 UINT64_C(0x3ff0000000000001), 1u, 1u, 1u, 1u),
    MATRIX_MODES("f64-add-half-ulp-below", 64u, T64_SOFTFLOAT_OP_F64_ADD,
                 UINT64_C(0x3ff0000000000000),
                 UINT64_C(0x3c9fffffffffffff),
                 MATRIX_NEIGHBOR | MATRIX_INEXACT, MATRIX_SOURCE_RATIONAL,
                 UINT64_C(0x3ff0000000000000),
                 UINT64_C(0x3ff0000000000000),
                 UINT64_C(0x3ff0000000000000),
                 UINT64_C(0x3ff0000000000001), 1u, 1u, 1u, 1u),
    MATRIX_MODES("f64-add-half-ulp-above", 64u, T64_SOFTFLOAT_OP_F64_ADD,
                 UINT64_C(0x3ff0000000000000),
                 UINT64_C(0x3ca0000000000001),
                 MATRIX_NEIGHBOR | MATRIX_INEXACT, MATRIX_SOURCE_RATIONAL,
                 UINT64_C(0x3ff0000000000001),
                 UINT64_C(0x3ff0000000000000),
                 UINT64_C(0x3ff0000000000000),
                 UINT64_C(0x3ff0000000000001), 1u, 1u, 1u, 1u),

    MATRIX_MODES("f64-to-f32-halfway-positive", 64u,
                 T64_SOFTFLOAT_OP_F64_TO_F32,
                 UINT64_C(0x3ff0000010000000), 0u,
                 MATRIX_TIE | MATRIX_INEXACT, MATRIX_SOURCE_RATIONAL,
                 UINT32_C(0x3f800000), UINT32_C(0x3f800000),
                 UINT32_C(0x3f800000), UINT32_C(0x3f800001), 1u, 1u, 1u, 1u),
    MATRIX_MODES("f64-to-f32-neighbor-below", 64u,
                 T64_SOFTFLOAT_OP_F64_TO_F32,
                 UINT64_C(0x3ff000000fffffff), 0u,
                 MATRIX_NEIGHBOR | MATRIX_INEXACT, MATRIX_SOURCE_RATIONAL,
                 UINT32_C(0x3f800000), UINT32_C(0x3f800000),
                 UINT32_C(0x3f800000), UINT32_C(0x3f800001), 1u, 1u, 1u, 1u),
    MATRIX_MODES("f64-to-f32-neighbor-above", 64u,
                 T64_SOFTFLOAT_OP_F64_TO_F32,
                 UINT64_C(0x3ff0000010000001), 0u,
                 MATRIX_NEIGHBOR | MATRIX_INEXACT, MATRIX_SOURCE_RATIONAL,
                 UINT32_C(0x3f800001), UINT32_C(0x3f800000),
                 UINT32_C(0x3f800000), UINT32_C(0x3f800001), 1u, 1u, 1u, 1u),
    MATRIX_MODES("f64-to-f32-halfway-negative", 64u,
                 T64_SOFTFLOAT_OP_F64_TO_F32,
                 UINT64_C(0xbff0000010000000), 0u,
                 MATRIX_TIE | MATRIX_INEXACT, MATRIX_SOURCE_RATIONAL,
                 UINT32_C(0xbf800000), UINT32_C(0xbf800000),
                 UINT32_C(0xbf800001), UINT32_C(0xbf800000), 1u, 1u, 1u, 1u),
    MATRIX_MODES("f64-to-f32-neighbor-negative-below", 64u,
                 T64_SOFTFLOAT_OP_F64_TO_F32,
                 UINT64_C(0xbff000000fffffff), 0u,
                 MATRIX_NEIGHBOR | MATRIX_INEXACT, MATRIX_SOURCE_RATIONAL,
                 UINT32_C(0xbf800000), UINT32_C(0xbf800000),
                 UINT32_C(0xbf800001), UINT32_C(0xbf800000), 1u, 1u, 1u, 1u),
    MATRIX_MODES("f64-to-f32-neighbor-negative-above", 64u,
                 T64_SOFTFLOAT_OP_F64_TO_F32,
                 UINT64_C(0xbff0000010000001), 0u,
                 MATRIX_NEIGHBOR | MATRIX_INEXACT, MATRIX_SOURCE_RATIONAL,
                 UINT32_C(0xbf800001), UINT32_C(0xbf800000),
                 UINT32_C(0xbf800001), UINT32_C(0xbf800000), 1u, 1u, 1u, 1u),

    MATRIX_MODES("i32-to-f32-integer-tie-positive", 32u,
                 T64_SOFTFLOAT_OP_I32_TO_F32, UINT32_C(0x01000001), 0u,
                 MATRIX_TIE | MATRIX_INEXACT, MATRIX_SOURCE_INTEGER_INTERVAL,
                 UINT32_C(0x4b800000), UINT32_C(0x4b800000),
                 UINT32_C(0x4b800000), UINT32_C(0x4b800001), 1u, 1u, 1u, 1u),
    MATRIX_MODES("i32-to-f32-integer-tie-negative", 32u,
                 T64_SOFTFLOAT_OP_I32_TO_F32, UINT32_C(0xfeffffff), 0u,
                 MATRIX_TIE | MATRIX_INEXACT, MATRIX_SOURCE_INTEGER_INTERVAL,
                 UINT32_C(0xcb800000), UINT32_C(0xcb800000),
                 UINT32_C(0xcb800001), UINT32_C(0xcb800000), 1u, 1u, 1u, 1u),
    MATRIX_MODES("i64-to-f32-integer-tie-positive", 32u,
                 T64_SOFTFLOAT_OP_I64_TO_F32,
                 UINT64_C(0x0000000001000001), 0u,
                 MATRIX_TIE | MATRIX_INEXACT, MATRIX_SOURCE_INTEGER_INTERVAL,
                 UINT32_C(0x4b800000), UINT32_C(0x4b800000),
                 UINT32_C(0x4b800000), UINT32_C(0x4b800001), 1u, 1u, 1u, 1u),
    MATRIX_MODES("i64-to-f32-integer-tie-negative", 32u,
                 T64_SOFTFLOAT_OP_I64_TO_F32,
                 UINT64_C(0xfffffffffeffffff), 0u,
                 MATRIX_TIE | MATRIX_INEXACT, MATRIX_SOURCE_INTEGER_INTERVAL,
                 UINT32_C(0xcb800000), UINT32_C(0xcb800000),
                 UINT32_C(0xcb800001), UINT32_C(0xcb800000), 1u, 1u, 1u, 1u),
    MATRIX_MODES("i64-to-f64-integer-tie-positive", 64u,
                 T64_SOFTFLOAT_OP_I64_TO_F64,
                 UINT64_C(0x0020000000000001), 0u,
                 MATRIX_TIE | MATRIX_INEXACT, MATRIX_SOURCE_INTEGER_INTERVAL,
                 UINT64_C(0x4340000000000000),
                 UINT64_C(0x4340000000000000),
                 UINT64_C(0x4340000000000000),
                 UINT64_C(0x4340000000000001), 1u, 1u, 1u, 1u),
    MATRIX_MODES("i64-to-f64-integer-tie-negative", 64u,
                 T64_SOFTFLOAT_OP_I64_TO_F64,
                 UINT64_C(0xffdfffffffffffff), 0u,
                 MATRIX_TIE | MATRIX_INEXACT, MATRIX_SOURCE_INTEGER_INTERVAL,
                 UINT64_C(0xc340000000000000),
                 UINT64_C(0xc340000000000000),
                 UINT64_C(0xc340000000000001),
                 UINT64_C(0xc340000000000000), 1u, 1u, 1u, 1u),

    MATRIX_MODES("f32-zero-positive", 32u, T64_SOFTFLOAT_OP_F32_ADD,
                 0u, 0u, MATRIX_ZERO_SIGN, MATRIX_SOURCE_BITS,
                 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u),
    MATRIX_SAME("f32-zero-negative", 32u, T64_SOFTFLOAT_OP_F32_ADD,
                UINT32_C(0x80000000), UINT32_C(0x80000000),
                MATRIX_ZERO_SIGN, MATRIX_SOURCE_BITS,
                UINT32_C(0x80000000), 0u),
    MATRIX_MODES("f32-zero-opposite", 32u, T64_SOFTFLOAT_OP_F32_ADD,
                 0u, UINT32_C(0x80000000), MATRIX_ZERO_SIGN,
                 MATRIX_SOURCE_BITS, 0u, 0u, UINT32_C(0x80000000), 0u,
                 0u, 0u, 0u, 0u),
    MATRIX_MODES("f64-zero-positive", 64u, T64_SOFTFLOAT_OP_F64_ADD,
                 0u, 0u, MATRIX_ZERO_SIGN, MATRIX_SOURCE_BITS,
                 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u),
    MATRIX_SAME("f64-zero-negative", 64u, T64_SOFTFLOAT_OP_F64_ADD,
                UINT64_C(0x8000000000000000),
                UINT64_C(0x8000000000000000), MATRIX_ZERO_SIGN,
                MATRIX_SOURCE_BITS, UINT64_C(0x8000000000000000), 0u),
    MATRIX_MODES("f64-zero-opposite", 64u, T64_SOFTFLOAT_OP_F64_ADD,
                 0u, UINT64_C(0x8000000000000000), MATRIX_ZERO_SIGN,
                 MATRIX_SOURCE_BITS, 0u, 0u,
                 UINT64_C(0x8000000000000000), 0u, 0u, 0u, 0u, 0u),

    MATRIX_SAME("f32-minsub-plus-minsub", 32u, T64_SOFTFLOAT_OP_F32_ADD,
                1u, 1u, MATRIX_SUBNORMAL, MATRIX_SOURCE_RATIONAL, 2u, 0u),
    MATRIX_SAME("f32-maxsub-plus-minsub", 32u, T64_SOFTFLOAT_OP_F32_ADD,
                UINT32_C(0x007fffff), 1u, MATRIX_SUBNORMAL,
                MATRIX_SOURCE_RATIONAL, UINT32_C(0x00800000), 0u),
    MATRIX_SAME("f32-minnormal-minus-maxsub", 32u, T64_SOFTFLOAT_OP_F32_SUB,
                UINT32_C(0x00800000), UINT32_C(0x007fffff),
                MATRIX_SUBNORMAL, MATRIX_SOURCE_RATIONAL, 1u, 0u),
    MATRIX_SAME("f32-minnormal-times-one", 32u, T64_SOFTFLOAT_OP_F32_MUL,
                UINT32_C(0x00800000), UINT32_C(0x3f800000),
                MATRIX_SUBNORMAL, MATRIX_SOURCE_BITS,
                UINT32_C(0x00800000), 0u),
    MATRIX_SAME("f32-maxnormal-times-one", 32u, T64_SOFTFLOAT_OP_F32_MUL,
                UINT32_C(0x7f7fffff), UINT32_C(0x3f800000),
                MATRIX_SUBNORMAL, MATRIX_SOURCE_BITS,
                UINT32_C(0x7f7fffff), 0u),
    MATRIX_SAME("f64-minsub-plus-minsub", 64u, T64_SOFTFLOAT_OP_F64_ADD,
                1u, 1u, MATRIX_SUBNORMAL, MATRIX_SOURCE_RATIONAL, 2u, 0u),
    MATRIX_SAME("f64-maxsub-plus-minsub", 64u, T64_SOFTFLOAT_OP_F64_ADD,
                UINT64_C(0x000fffffffffffff), 1u, MATRIX_SUBNORMAL,
                MATRIX_SOURCE_RATIONAL, UINT64_C(0x0010000000000000), 0u),
    MATRIX_SAME("f64-minnormal-minus-maxsub", 64u, T64_SOFTFLOAT_OP_F64_SUB,
                UINT64_C(0x0010000000000000),
                UINT64_C(0x000fffffffffffff), MATRIX_SUBNORMAL,
                MATRIX_SOURCE_RATIONAL, 1u, 0u),
    MATRIX_SAME("f64-minnormal-times-one", 64u, T64_SOFTFLOAT_OP_F64_MUL,
                UINT64_C(0x0010000000000000),
                UINT64_C(0x3ff0000000000000), MATRIX_SUBNORMAL,
                MATRIX_SOURCE_BITS, UINT64_C(0x0010000000000000), 0u),
    MATRIX_SAME("f64-maxnormal-times-one", 64u, T64_SOFTFLOAT_OP_F64_MUL,
                UINT64_C(0x7fefffffffffffff),
                UINT64_C(0x3ff0000000000000), MATRIX_SUBNORMAL,
                MATRIX_SOURCE_BITS, UINT64_C(0x7fefffffffffffff), 0u),

    MATRIX_SAME("f32-positive-infinity-plus-one", 32u,
                T64_SOFTFLOAT_OP_F32_ADD, UINT32_C(0x7f800000),
                UINT32_C(0x3f800000), MATRIX_INFINITY, MATRIX_SOURCE_BITS,
                UINT32_C(0x7f800000), 0u),
    MATRIX_SAME("f32-negative-infinity-times-two", 32u,
                T64_SOFTFLOAT_OP_F32_MUL, UINT32_C(0xff800000),
                UINT32_C(0x40000000), MATRIX_INFINITY, MATRIX_SOURCE_BITS,
                UINT32_C(0xff800000), 0u),
    MATRIX_SAME("f64-positive-infinity-plus-one", 64u,
                T64_SOFTFLOAT_OP_F64_ADD, UINT64_C(0x7ff0000000000000),
                UINT64_C(0x3ff0000000000000), MATRIX_INFINITY,
                MATRIX_SOURCE_BITS, UINT64_C(0x7ff0000000000000), 0u),
    MATRIX_SAME("f64-negative-infinity-times-two", 64u,
                T64_SOFTFLOAT_OP_F64_MUL, UINT64_C(0xfff0000000000000),
                UINT64_C(0x4000000000000000), MATRIX_INFINITY,
                MATRIX_SOURCE_BITS, UINT64_C(0xfff0000000000000), 0u),

    MATRIX_SAME("f32-qnan-propagation", 32u, T64_SOFTFLOAT_OP_F32_ADD,
                UINT32_C(0x7fc12345), UINT32_C(0x7fc54321), MATRIX_QNAN,
                MATRIX_SOURCE_BITS, UINT32_C(0x7fc12345), 0u),
    MATRIX_SAME("f32-qnan-eq", 32u, T64_SOFTFLOAT_OP_F32_EQ,
                UINT32_C(0x7fc12345), UINT32_C(0x3f800000), MATRIX_QNAN,
                MATRIX_SOURCE_BITS, 0u, 0u),
    MATRIX_SAME("f32-qnan-le-invalid", 32u, T64_SOFTFLOAT_OP_F32_LE,
                UINT32_C(0x7fc12345), UINT32_C(0x3f800000),
                MATRIX_QNAN | MATRIX_INVALID, MATRIX_SOURCE_BITS, 0u,
                T64_SOFTFLOAT_FLAG_INVALID),
    MATRIX_SAME("f32-snan-propagation", 32u, T64_SOFTFLOAT_OP_F32_ADD,
                UINT32_C(0x7f812345), UINT32_C(0x7fc54321), MATRIX_SNAN,
                MATRIX_SOURCE_BITS, UINT32_C(0x7fc12345),
                T64_SOFTFLOAT_FLAG_INVALID),
    MATRIX_SAME("f32-snan-lt-invalid", 32u, T64_SOFTFLOAT_OP_F32_LT,
                UINT32_C(0x7f812345), UINT32_C(0x3f800000),
                MATRIX_SNAN | MATRIX_INVALID, MATRIX_SOURCE_BITS, 0u,
                T64_SOFTFLOAT_FLAG_INVALID),
    MATRIX_SAME("f64-qnan-propagation", 64u, T64_SOFTFLOAT_OP_F64_ADD,
                UINT64_C(0x7ff8123456789abc),
                UINT64_C(0x7ff8543212345678), MATRIX_QNAN,
                MATRIX_SOURCE_BITS, UINT64_C(0x7ff8123456789abc), 0u),
    MATRIX_SAME("f64-qnan-eq", 64u, T64_SOFTFLOAT_OP_F64_EQ,
                UINT64_C(0x7ff8123456789abc),
                UINT64_C(0x3ff0000000000000), MATRIX_QNAN,
                MATRIX_SOURCE_BITS, 0u, 0u),
    MATRIX_SAME("f64-qnan-le-invalid", 64u, T64_SOFTFLOAT_OP_F64_LE,
                UINT64_C(0x7ff8123456789abc),
                UINT64_C(0x3ff0000000000000), MATRIX_QNAN | MATRIX_INVALID,
                MATRIX_SOURCE_BITS, 0u, T64_SOFTFLOAT_FLAG_INVALID),
    MATRIX_SAME("f64-snan-propagation", 64u, T64_SOFTFLOAT_OP_F64_ADD,
                UINT64_C(0x7ff0123456789abc),
                UINT64_C(0x7ff8543212345678), MATRIX_SNAN,
                MATRIX_SOURCE_BITS, UINT64_C(0x7ff8123456789abc),
                T64_SOFTFLOAT_FLAG_INVALID),
    MATRIX_SAME("f64-snan-lt-invalid", 64u, T64_SOFTFLOAT_OP_F64_LT,
                UINT64_C(0x7ff0123456789abc),
                UINT64_C(0x3ff0000000000000), MATRIX_SNAN | MATRIX_INVALID,
                MATRIX_SOURCE_BITS, 0u, T64_SOFTFLOAT_FLAG_INVALID),

    MATRIX_SAME("f32-i32-min-endpoint", 32u,
                T64_SOFTFLOAT_OP_F32_TO_I32, UINT32_C(0xcf000000), 0u,
                MATRIX_RANGE, MATRIX_SOURCE_INTEGER_INTERVAL,
                UINT64_C(0xffffffff80000000), 0u),
    MATRIX_SAME("f32-i32-below-min", 32u, T64_SOFTFLOAT_OP_F32_TO_I32,
                UINT32_C(0xcf000001), 0u, MATRIX_RANGE | MATRIX_INVALID,
                MATRIX_SOURCE_INTEGER_INTERVAL,
                UINT64_C(0xffffffff80000000), T64_SOFTFLOAT_FLAG_INVALID),
    MATRIX_SAME("f32-i32-max-in-range", 32u,
                T64_SOFTFLOAT_OP_F32_TO_I32, UINT32_C(0x4effffff), 0u,
                MATRIX_RANGE, MATRIX_SOURCE_INTEGER_INTERVAL,
                UINT64_C(0x000000007fffff80), 0u),
    MATRIX_SAME("f32-i32-above-max", 32u, T64_SOFTFLOAT_OP_F32_TO_I32,
                UINT32_C(0x4f000000), 0u, MATRIX_RANGE | MATRIX_INVALID,
                MATRIX_SOURCE_INTEGER_INTERVAL,
                UINT64_C(0xffffffff80000000), T64_SOFTFLOAT_FLAG_INVALID),
    MATRIX_SAME("f32-i64-min-endpoint", 32u,
                T64_SOFTFLOAT_OP_F32_TO_I64, UINT32_C(0xdf000000), 0u,
                MATRIX_RANGE, MATRIX_SOURCE_INTEGER_INTERVAL,
                UINT64_C(0x8000000000000000), 0u),
    MATRIX_SAME("f32-i64-below-min", 32u, T64_SOFTFLOAT_OP_F32_TO_I64,
                UINT32_C(0xdf000001), 0u, MATRIX_RANGE | MATRIX_INVALID,
                MATRIX_SOURCE_INTEGER_INTERVAL,
                UINT64_C(0x8000000000000000), T64_SOFTFLOAT_FLAG_INVALID),
    MATRIX_SAME("f32-i64-max-in-range", 32u,
                T64_SOFTFLOAT_OP_F32_TO_I64, UINT32_C(0x5effffff), 0u,
                MATRIX_RANGE, MATRIX_SOURCE_INTEGER_INTERVAL,
                UINT64_C(0x7fffff8000000000), 0u),
    MATRIX_SAME("f32-i64-above-max", 32u, T64_SOFTFLOAT_OP_F32_TO_I64,
                UINT32_C(0x5f000000), 0u, MATRIX_RANGE | MATRIX_INVALID,
                MATRIX_SOURCE_INTEGER_INTERVAL,
                UINT64_C(0x8000000000000000), T64_SOFTFLOAT_FLAG_INVALID),
    MATRIX_SAME("f64-i32-min-endpoint", 64u,
                T64_SOFTFLOAT_OP_F64_TO_I32,
                UINT64_C(0xc1e0000000000000), 0u, MATRIX_RANGE,
                MATRIX_SOURCE_INTEGER_INTERVAL,
                UINT64_C(0xffffffff80000000), 0u),
    MATRIX_SAME("f64-i32-below-min", 64u, T64_SOFTFLOAT_OP_F64_TO_I32,
                UINT64_C(0xc1e0000000200000), 0u,
                MATRIX_RANGE | MATRIX_INVALID, MATRIX_SOURCE_INTEGER_INTERVAL,
                UINT64_C(0xffffffff80000000), T64_SOFTFLOAT_FLAG_INVALID),
    MATRIX_SAME("f64-i32-max-endpoint", 64u,
                T64_SOFTFLOAT_OP_F64_TO_I32,
                UINT64_C(0x41dfffffffc00000), 0u, MATRIX_RANGE,
                MATRIX_SOURCE_INTEGER_INTERVAL,
                UINT64_C(0x000000007fffffff), 0u),
    MATRIX_SAME("f64-i32-above-max", 64u, T64_SOFTFLOAT_OP_F64_TO_I32,
                UINT64_C(0x41e0000000000000), 0u,
                MATRIX_RANGE | MATRIX_INVALID, MATRIX_SOURCE_INTEGER_INTERVAL,
                UINT64_C(0xffffffff80000000), T64_SOFTFLOAT_FLAG_INVALID),
    MATRIX_SAME("f64-i64-min-endpoint", 64u,
                T64_SOFTFLOAT_OP_F64_TO_I64,
                UINT64_C(0xc3e0000000000000), 0u, MATRIX_RANGE,
                MATRIX_SOURCE_INTEGER_INTERVAL,
                UINT64_C(0x8000000000000000), 0u),
    MATRIX_SAME("f64-i64-below-min", 64u, T64_SOFTFLOAT_OP_F64_TO_I64,
                UINT64_C(0xc3e0000000000001), 0u,
                MATRIX_RANGE | MATRIX_INVALID, MATRIX_SOURCE_INTEGER_INTERVAL,
                UINT64_C(0x8000000000000000), T64_SOFTFLOAT_FLAG_INVALID),
    MATRIX_SAME("f64-i64-max-endpoint", 64u,
                T64_SOFTFLOAT_OP_F64_TO_I64,
                UINT64_C(0x43dfffffffffffff), 0u, MATRIX_RANGE,
                MATRIX_SOURCE_INTEGER_INTERVAL,
                UINT64_C(0x7ffffffffffffc00), 0u),
    MATRIX_SAME("f64-i64-above-max", 64u, T64_SOFTFLOAT_OP_F64_TO_I64,
                UINT64_C(0x43e0000000000000), 0u,
                MATRIX_RANGE | MATRIX_INVALID, MATRIX_SOURCE_INTEGER_INTERVAL,
                UINT64_C(0x8000000000000000), T64_SOFTFLOAT_FLAG_INVALID),

    MATRIX_MODES("f32-overflow-positive", 32u, T64_SOFTFLOAT_OP_F32_MUL,
                 UINT32_C(0x7f7fffff), UINT32_C(0x40000000),
                 MATRIX_OVERFLOW | MATRIX_INEXACT, MATRIX_SOURCE_RATIONAL,
                 UINT32_C(0x7f800000), UINT32_C(0x7f7fffff),
                 UINT32_C(0x7f7fffff), UINT32_C(0x7f800000), 5u, 5u, 5u, 5u),
    MATRIX_MODES("f32-overflow-negative", 32u, T64_SOFTFLOAT_OP_F32_MUL,
                 UINT32_C(0xff7fffff), UINT32_C(0x40000000),
                 MATRIX_OVERFLOW | MATRIX_INEXACT, MATRIX_SOURCE_RATIONAL,
                 UINT32_C(0xff800000), UINT32_C(0xff7fffff),
                 UINT32_C(0xff800000), UINT32_C(0xff7fffff), 5u, 5u, 5u, 5u),
    MATRIX_MODES("f64-overflow-positive", 64u, T64_SOFTFLOAT_OP_F64_MUL,
                 UINT64_C(0x7fefffffffffffff),
                 UINT64_C(0x4000000000000000),
                 MATRIX_OVERFLOW | MATRIX_INEXACT, MATRIX_SOURCE_RATIONAL,
                 UINT64_C(0x7ff0000000000000),
                 UINT64_C(0x7fefffffffffffff),
                 UINT64_C(0x7fefffffffffffff),
                 UINT64_C(0x7ff0000000000000), 5u, 5u, 5u, 5u),
    MATRIX_MODES("f64-overflow-negative", 64u, T64_SOFTFLOAT_OP_F64_MUL,
                 UINT64_C(0xffefffffffffffff),
                 UINT64_C(0x4000000000000000),
                 MATRIX_OVERFLOW | MATRIX_INEXACT, MATRIX_SOURCE_RATIONAL,
                 UINT64_C(0xfff0000000000000),
                 UINT64_C(0xffefffffffffffff),
                 UINT64_C(0xfff0000000000000),
                 UINT64_C(0xffefffffffffffff), 5u, 5u, 5u, 5u),
    MATRIX_MODES("f32-underflow-positive", 32u, T64_SOFTFLOAT_OP_F32_MUL,
                 1u, UINT32_C(0x3f000000),
                 MATRIX_UNDERFLOW | MATRIX_INEXACT,
                 MATRIX_SOURCE_RATIONAL, 0u, 0u, 0u, 1u, 3u, 3u, 3u, 3u),
    MATRIX_MODES("f32-underflow-negative", 32u, T64_SOFTFLOAT_OP_F32_MUL,
                 UINT32_C(0x80000001), UINT32_C(0x3f000000),
                 MATRIX_UNDERFLOW | MATRIX_INEXACT,
                 MATRIX_SOURCE_RATIONAL, UINT32_C(0x80000000),
                 UINT32_C(0x80000000), UINT32_C(0x80000001),
                 UINT32_C(0x80000000), 3u, 3u, 3u, 3u),
    MATRIX_MODES("f64-underflow-positive", 64u, T64_SOFTFLOAT_OP_F64_MUL,
                 1u, UINT64_C(0x3fe0000000000000),
                 MATRIX_UNDERFLOW | MATRIX_INEXACT,
                 MATRIX_SOURCE_RATIONAL, 0u, 0u, 0u, 1u, 3u, 3u, 3u, 3u),
    MATRIX_MODES("f64-underflow-negative", 64u, T64_SOFTFLOAT_OP_F64_MUL,
                 UINT64_C(0x8000000000000001),
                 UINT64_C(0x3fe0000000000000),
                 MATRIX_UNDERFLOW | MATRIX_INEXACT,
                 MATRIX_SOURCE_RATIONAL, UINT64_C(0x8000000000000000),
                 UINT64_C(0x8000000000000000),
                 UINT64_C(0x8000000000000001),
                 UINT64_C(0x8000000000000000), 3u, 3u, 3u, 3u),
    MATRIX_SAME("f32-divzero-positive", 32u, T64_SOFTFLOAT_OP_F32_DIV,
                UINT32_C(0x3f800000), 0u, MATRIX_DIVZERO,
                MATRIX_SOURCE_RATIONAL, UINT32_C(0x7f800000),
                T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO),
    MATRIX_SAME("f32-divzero-negative", 32u, T64_SOFTFLOAT_OP_F32_DIV,
                UINT32_C(0xbf800000), 0u, MATRIX_DIVZERO,
                MATRIX_SOURCE_RATIONAL, UINT32_C(0xff800000),
                T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO),
    MATRIX_SAME("f64-divzero-positive", 64u, T64_SOFTFLOAT_OP_F64_DIV,
                UINT64_C(0x3ff0000000000000), 0u, MATRIX_DIVZERO,
                MATRIX_SOURCE_RATIONAL, UINT64_C(0x7ff0000000000000),
                T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO),
    MATRIX_SAME("f64-divzero-negative", 64u, T64_SOFTFLOAT_OP_F64_DIV,
                UINT64_C(0xbff0000000000000), 0u, MATRIX_DIVZERO,
                MATRIX_SOURCE_RATIONAL, UINT64_C(0xfff0000000000000),
                T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO),
    MATRIX_SAME("f32-invalid-zero-over-zero", 32u,
                T64_SOFTFLOAT_OP_F32_DIV, 0u, 0u,
                MATRIX_INVALID, MATRIX_SOURCE_RATIONAL,
                UINT32_C(0x7fc00000), T64_SOFTFLOAT_FLAG_INVALID),
    MATRIX_SAME("f32-invalid-infinity-cancel", 32u,
                T64_SOFTFLOAT_OP_F32_SUB, UINT32_C(0x7f800000),
                UINT32_C(0x7f800000), MATRIX_INVALID, MATRIX_SOURCE_BITS,
                UINT32_C(0x7fc00000), T64_SOFTFLOAT_FLAG_INVALID),
    MATRIX_SAME("f32-invalid-infinity-times-zero", 32u,
                T64_SOFTFLOAT_OP_F32_MUL, UINT32_C(0x7f800000), 0u,
                MATRIX_INVALID, MATRIX_SOURCE_BITS, UINT32_C(0x7fc00000),
                T64_SOFTFLOAT_FLAG_INVALID),
    MATRIX_SAME("f32-invalid-negative-sqrt", 32u,
                T64_SOFTFLOAT_OP_F32_SQRT, UINT32_C(0xbf800000), 0u,
                MATRIX_INVALID, MATRIX_SOURCE_BITS, UINT32_C(0x7fc00000),
                T64_SOFTFLOAT_FLAG_INVALID),
    MATRIX_SAME("f64-invalid-zero-over-zero", 64u,
                T64_SOFTFLOAT_OP_F64_DIV, 0u, 0u, MATRIX_INVALID,
                MATRIX_SOURCE_RATIONAL, UINT64_C(0x7ff8000000000000),
                T64_SOFTFLOAT_FLAG_INVALID),
    MATRIX_SAME("f64-invalid-infinity-cancel", 64u,
                T64_SOFTFLOAT_OP_F64_SUB,
                UINT64_C(0x7ff0000000000000),
                UINT64_C(0x7ff0000000000000), MATRIX_INVALID,
                MATRIX_SOURCE_BITS, UINT64_C(0x7ff8000000000000),
                T64_SOFTFLOAT_FLAG_INVALID),
    MATRIX_SAME("f64-invalid-infinity-times-zero", 64u,
                T64_SOFTFLOAT_OP_F64_MUL,
                UINT64_C(0x7ff0000000000000), 0u, MATRIX_INVALID,
                MATRIX_SOURCE_BITS, UINT64_C(0x7ff8000000000000),
                T64_SOFTFLOAT_FLAG_INVALID),
    MATRIX_SAME("f64-invalid-negative-sqrt", 64u,
                T64_SOFTFLOAT_OP_F64_SQRT, UINT64_C(0xbff0000000000000), 0u,
                MATRIX_INVALID, MATRIX_SOURCE_BITS,
                UINT64_C(0x7ff8000000000000), T64_SOFTFLOAT_FLAG_INVALID),

    MATRIX_MODES("f32-sub-half-ulp-below-one", 32u,
                 T64_SOFTFLOAT_OP_F32_SUB, UINT32_C(0x3f800000),
                 UINT32_C(0x32ffffff),
                 MATRIX_NEIGHBOR | MATRIX_INEXACT |
                     MATRIX_ROUNDING_SENSITIVE,
                 MATRIX_SOURCE_RATIONAL, UINT32_C(0x3f800000),
                 UINT32_C(0x3f7fffff), UINT32_C(0x3f7fffff),
                 UINT32_C(0x3f800000), 1u, 1u, 1u, 1u),
    MATRIX_MODES("f32-sub-half-ulp-at-one", 32u,
                 T64_SOFTFLOAT_OP_F32_SUB, UINT32_C(0x3f800000),
                 UINT32_C(0x33000000),
                 MATRIX_TIE | MATRIX_INEXACT | MATRIX_ROUNDING_SENSITIVE,
                 MATRIX_SOURCE_RATIONAL, UINT32_C(0x3f800000),
                 UINT32_C(0x3f7fffff), UINT32_C(0x3f7fffff),
                 UINT32_C(0x3f800000), 1u, 1u, 1u, 1u),
    MATRIX_MODES("f32-sub-half-ulp-above-one", 32u,
                 T64_SOFTFLOAT_OP_F32_SUB, UINT32_C(0x3f800000),
                 UINT32_C(0x33000001),
                 MATRIX_NEIGHBOR | MATRIX_INEXACT |
                     MATRIX_ROUNDING_SENSITIVE,
                 MATRIX_SOURCE_RATIONAL, UINT32_C(0x3f7fffff),
                 UINT32_C(0x3f7fffff), UINT32_C(0x3f7fffff),
                 UINT32_C(0x3f800000), 1u, 1u, 1u, 1u),
    MATRIX_MODES("f64-sub-half-ulp-below-one", 64u,
                 T64_SOFTFLOAT_OP_F64_SUB,
                 UINT64_C(0x3ff0000000000000),
                 UINT64_C(0x3c8fffffffffffff),
                 MATRIX_NEIGHBOR | MATRIX_INEXACT |
                     MATRIX_ROUNDING_SENSITIVE,
                 MATRIX_SOURCE_RATIONAL,
                 UINT64_C(0x3ff0000000000000),
                 UINT64_C(0x3fefffffffffffff),
                 UINT64_C(0x3fefffffffffffff),
                 UINT64_C(0x3ff0000000000000), 1u, 1u, 1u, 1u),
    MATRIX_MODES("f64-sub-half-ulp-at-one", 64u,
                 T64_SOFTFLOAT_OP_F64_SUB,
                 UINT64_C(0x3ff0000000000000),
                 UINT64_C(0x3c90000000000000),
                 MATRIX_TIE | MATRIX_INEXACT | MATRIX_ROUNDING_SENSITIVE,
                 MATRIX_SOURCE_RATIONAL,
                 UINT64_C(0x3ff0000000000000),
                 UINT64_C(0x3fefffffffffffff),
                 UINT64_C(0x3fefffffffffffff),
                 UINT64_C(0x3ff0000000000000), 1u, 1u, 1u, 1u),
    MATRIX_MODES("f64-sub-half-ulp-above-one", 64u,
                 T64_SOFTFLOAT_OP_F64_SUB,
                 UINT64_C(0x3ff0000000000000),
                 UINT64_C(0x3c90000000000001),
                 MATRIX_NEIGHBOR | MATRIX_INEXACT |
                     MATRIX_ROUNDING_SENSITIVE,
                 MATRIX_SOURCE_RATIONAL,
                 UINT64_C(0x3fefffffffffffff),
                 UINT64_C(0x3fefffffffffffff),
                 UINT64_C(0x3fefffffffffffff),
                 UINT64_C(0x3ff0000000000000), 1u, 1u, 1u, 1u),

    MATRIX_MODES("f32-mul-near-one-positive", 32u,
                 T64_SOFTFLOAT_OP_F32_MUL, UINT32_C(0x3f800001),
                 UINT32_C(0x3f800001),
                 MATRIX_NEIGHBOR | MATRIX_INEXACT |
                     MATRIX_ROUNDING_SENSITIVE,
                 MATRIX_SOURCE_RATIONAL, UINT32_C(0x3f800002),
                 UINT32_C(0x3f800002), UINT32_C(0x3f800002),
                 UINT32_C(0x3f800003), 1u, 1u, 1u, 1u),
    MATRIX_MODES("f32-mul-near-one-negative", 32u,
                 T64_SOFTFLOAT_OP_F32_MUL, UINT32_C(0xbf800001),
                 UINT32_C(0x3f800001),
                 MATRIX_NEIGHBOR | MATRIX_INEXACT |
                     MATRIX_ROUNDING_SENSITIVE,
                 MATRIX_SOURCE_RATIONAL, UINT32_C(0xbf800002),
                 UINT32_C(0xbf800002), UINT32_C(0xbf800003),
                 UINT32_C(0xbf800002), 1u, 1u, 1u, 1u),
    MATRIX_MODES("f64-mul-near-one-positive", 64u,
                 T64_SOFTFLOAT_OP_F64_MUL,
                 UINT64_C(0x3ff0000000000001),
                 UINT64_C(0x3ff0000000000001),
                 MATRIX_NEIGHBOR | MATRIX_INEXACT |
                     MATRIX_ROUNDING_SENSITIVE,
                 MATRIX_SOURCE_RATIONAL,
                 UINT64_C(0x3ff0000000000002),
                 UINT64_C(0x3ff0000000000002),
                 UINT64_C(0x3ff0000000000002),
                 UINT64_C(0x3ff0000000000003), 1u, 1u, 1u, 1u),
    MATRIX_MODES("f64-mul-near-one-negative", 64u,
                 T64_SOFTFLOAT_OP_F64_MUL,
                 UINT64_C(0xbff0000000000001),
                 UINT64_C(0x3ff0000000000001),
                 MATRIX_NEIGHBOR | MATRIX_INEXACT |
                     MATRIX_ROUNDING_SENSITIVE,
                 MATRIX_SOURCE_RATIONAL,
                 UINT64_C(0xbff0000000000002),
                 UINT64_C(0xbff0000000000002),
                 UINT64_C(0xbff0000000000003),
                 UINT64_C(0xbff0000000000002), 1u, 1u, 1u, 1u),

    MATRIX_MODES("f32-div-one-third-positive", 32u,
                 T64_SOFTFLOAT_OP_F32_DIV, UINT32_C(0x3f800000),
                 UINT32_C(0x40400000),
                 MATRIX_INEXACT | MATRIX_ROUNDING_SENSITIVE,
                 MATRIX_SOURCE_RATIONAL, UINT32_C(0x3eaaaaab),
                 UINT32_C(0x3eaaaaaa), UINT32_C(0x3eaaaaaa),
                 UINT32_C(0x3eaaaaab), 1u, 1u, 1u, 1u),
    MATRIX_MODES("f32-div-one-third-negative", 32u,
                 T64_SOFTFLOAT_OP_F32_DIV, UINT32_C(0xbf800000),
                 UINT32_C(0x40400000),
                 MATRIX_INEXACT | MATRIX_ROUNDING_SENSITIVE,
                 MATRIX_SOURCE_RATIONAL, UINT32_C(0xbeaaaaab),
                 UINT32_C(0xbeaaaaaa), UINT32_C(0xbeaaaaab),
                 UINT32_C(0xbeaaaaaa), 1u, 1u, 1u, 1u),
    MATRIX_MODES("f64-div-one-third-positive", 64u,
                 T64_SOFTFLOAT_OP_F64_DIV,
                 UINT64_C(0x3ff0000000000000),
                 UINT64_C(0x4008000000000000),
                 MATRIX_INEXACT | MATRIX_ROUNDING_SENSITIVE,
                 MATRIX_SOURCE_RATIONAL,
                 UINT64_C(0x3fd5555555555555),
                 UINT64_C(0x3fd5555555555555),
                 UINT64_C(0x3fd5555555555555),
                 UINT64_C(0x3fd5555555555556), 1u, 1u, 1u, 1u),
    MATRIX_MODES("f64-div-one-third-negative", 64u,
                 T64_SOFTFLOAT_OP_F64_DIV,
                 UINT64_C(0xbff0000000000000),
                 UINT64_C(0x4008000000000000),
                 MATRIX_INEXACT | MATRIX_ROUNDING_SENSITIVE,
                 MATRIX_SOURCE_RATIONAL,
                 UINT64_C(0xbfd5555555555555),
                 UINT64_C(0xbfd5555555555555),
                 UINT64_C(0xbfd5555555555556),
                 UINT64_C(0xbfd5555555555555), 1u, 1u, 1u, 1u),

    MATRIX_MODES("f32-sqrt-two", 32u, T64_SOFTFLOAT_OP_F32_SQRT,
                 UINT32_C(0x40000000), 0u,
                 MATRIX_INEXACT | MATRIX_ROUNDING_SENSITIVE,
                 MATRIX_SOURCE_RATIONAL, UINT32_C(0x3fb504f3),
                 UINT32_C(0x3fb504f3), UINT32_C(0x3fb504f3),
                 UINT32_C(0x3fb504f4), 1u, 1u, 1u, 1u),
    MATRIX_MODES("f64-sqrt-two", 64u, T64_SOFTFLOAT_OP_F64_SQRT,
                 UINT64_C(0x4000000000000000), 0u,
                 MATRIX_INEXACT | MATRIX_ROUNDING_SENSITIVE,
                 MATRIX_SOURCE_RATIONAL,
                 UINT64_C(0x3ff6a09e667f3bcd),
                 UINT64_C(0x3ff6a09e667f3bcc),
                 UINT64_C(0x3ff6a09e667f3bcc),
                 UINT64_C(0x3ff6a09e667f3bcd), 1u, 1u, 1u, 1u),
};

#undef MATRIX_SAME
#undef MATRIX_MODES

static void test_raw_edge_matrix(void) {
  static const uint16_t required_categories[] = {
      MATRIX_BASE,     MATRIX_TIE,       MATRIX_NEIGHBOR,
      MATRIX_ZERO_SIGN, MATRIX_SUBNORMAL, MATRIX_INFINITY,
      MATRIX_QNAN,     MATRIX_SNAN,      MATRIX_RANGE,
      MATRIX_OVERFLOW, MATRIX_UNDERFLOW, MATRIX_INEXACT,
      MATRIX_DIVZERO,  MATRIX_INVALID,   MATRIX_ROUNDING_SENSITIVE};
  uint8_t operation_modes[27][4] = {{0u}};
  uint8_t rounding_sensitive_modes[27][4] = {{0u}};
  uint16_t category_modes[2][4] = {{0u}};
  uint8_t source_modes[2][4] = {{0u}};
  uint8_t flags_seen[2] = {0u, 0u};
  const size_t vector_count =
      sizeof(softfloat_matrix) / sizeof(softfloat_matrix[0]);

  T64_CHECK_INT(vector_count, 126u);
  for (size_t index = 0u; index < vector_count; ++index) {
    const softfloat_matrix_vector *vector = &softfloat_matrix[index];
    const unsigned int format_index = vector->format == 32u ? 0u : 1u;

    T64_CHECK(vector->id != NULL && vector->id[0] != '\0');
    T64_CHECK(vector->format == 32u || vector->format == 64u);
    T64_CHECK(vector->operation >= T64_SOFTFLOAT_OP_I32_TO_F64 &&
              vector->operation <= T64_SOFTFLOAT_OP_I64_TO_F64);
    T64_CHECK(vector->categories != 0u);
    T64_CHECK(vector->expected_source >= MATRIX_SOURCE_BITS &&
              vector->expected_source <= MATRIX_SOURCE_INTEGER_INTERVAL);
    for (size_t previous = 0u; previous < index; ++previous) {
      T64_CHECK(strcmp(vector->id, softfloat_matrix[previous].id) != 0);
    }

    for (uint8_t rounding = 0u; rounding < 4u; ++rounding) {
      check_operation(vector->id, vector->operation, vector->a, vector->b,
                      rounding, T64_SOFTFLOAT_TININESS_AFTER_ROUNDING, 0u,
                      T64_STATUS_OK, vector->expected[rounding],
                      vector->flags[rounding]);
      category_modes[format_index][rounding] |= vector->categories;
      source_modes[format_index][rounding] |=
          (uint8_t)(1u << (vector->expected_source - 1u));
      flags_seen[format_index] |= vector->flags[rounding];
      if ((vector->categories & MATRIX_BASE) != 0u) {
        operation_modes[(unsigned int)vector->operation][rounding] = 1u;
      }
      if ((vector->categories & MATRIX_ROUNDING_SENSITIVE) != 0u) {
        rounding_sensitive_modes[(unsigned int)vector->operation][rounding] =
            1u;
      }
    }
  }

  /* Required inventory: every operation and every RM, then every required
     category and each derivation class for each result format and RM. */
  for (unsigned int operation = T64_SOFTFLOAT_OP_I32_TO_F64;
       operation <= T64_SOFTFLOAT_OP_I64_TO_F64; ++operation) {
    for (uint8_t rounding = 0u; rounding < 4u; ++rounding) {
      T64_CHECK_INT(operation_modes[operation][rounding], 1);
    }
  }
  for (unsigned int format = 0u; format < 2u; ++format) {
    for (uint8_t rounding = 0u; rounding < 4u; ++rounding) {
      for (size_t category = 0u;
           category < sizeof(required_categories) /
                          sizeof(required_categories[0]);
           ++category) {
        T64_CHECK((category_modes[format][rounding] &
                   required_categories[category]) != 0u);
      }
      T64_CHECK_INT(source_modes[format][rounding], 7u);
    }
    T64_CHECK((flags_seen[format] &
               (T64_SOFTFLOAT_FLAG_INEXACT | T64_SOFTFLOAT_FLAG_UNDERFLOW |
                T64_SOFTFLOAT_FLAG_OVERFLOW |
                T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO |
                T64_SOFTFLOAT_FLAG_INVALID)) ==
              (T64_SOFTFLOAT_FLAG_INEXACT | T64_SOFTFLOAT_FLAG_UNDERFLOW |
               T64_SOFTFLOAT_FLAG_OVERFLOW |
               T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO |
               T64_SOFTFLOAT_FLAG_INVALID));
  }
  for (unsigned int operation = T64_SOFTFLOAT_OP_F32_ADD;
       operation <= T64_SOFTFLOAT_OP_F32_SQRT; ++operation) {
    for (uint8_t rounding = 0u; rounding < 4u; ++rounding) {
      T64_CHECK_INT(rounding_sensitive_modes[operation][rounding], 1);
    }
  }
  for (unsigned int operation = T64_SOFTFLOAT_OP_F64_ADD;
       operation <= T64_SOFTFLOAT_OP_F64_SQRT; ++operation) {
    for (uint8_t rounding = 0u; rounding < 4u; ++rounding) {
      T64_CHECK_INT(rounding_sensitive_modes[operation][rounding], 1);
    }
  }
}

static void test_existing_conversion_and_binary32(void) {
  /* -2^31 is exact; its binary64 sign/exponent/significand encode C1E0... . */
  check_operation("i32 -2^31 to f64", T64_SOFTFLOAT_OP_I32_TO_F64,
                  UINT32_C(0x80000000), 0u, 0u,
                  T64_SOFTFLOAT_TININESS_AFTER_ROUNDING, 0u, T64_STATUS_OK,
                  UINT64_C(0xc1e0000000000000), 0u);
  check_operation("i32 positive to f64", T64_SOFTFLOAT_OP_I32_TO_F64, 3u,
                  0u, 0u, T64_SOFTFLOAT_TININESS_AFTER_ROUNDING, 0u,
                  T64_STATUS_OK, UINT64_C(0x4008000000000000), 0u);

  /* Exact dyadic arithmetic: 1 + 2 = 3 and (-1) + (-2) = -3. */
  check_operation("f32 add positive", T64_SOFTFLOAT_OP_F32_ADD,
                  UINT32_C(0x3f800000), UINT32_C(0x40000000), 0u, 1u, 0u,
                  T64_STATUS_OK, UINT32_C(0x40400000), 0u);
  check_operation("f32 add negative", T64_SOFTFLOAT_OP_F32_ADD,
                  UINT32_C(0xbf800000), UINT32_C(0xc0000000), 0u, 1u, 0u,
                  T64_STATUS_OK, UINT32_C(0xc0400000), 0u);
  /* Exact dyadic arithmetic: 3 - 1 = 2 and -3 - (-1) = -2. */
  check_operation("f32 sub positive", T64_SOFTFLOAT_OP_F32_SUB,
                  UINT32_C(0x40400000), UINT32_C(0x3f800000), 0u, 1u, 0u,
                  T64_STATUS_OK, UINT32_C(0x40000000), 0u);
  check_operation("f32 sub negative", T64_SOFTFLOAT_OP_F32_SUB,
                  UINT32_C(0xc0400000), UINT32_C(0xbf800000), 0u, 1u, 0u,
                  T64_STATUS_OK, UINT32_C(0xc0000000), 0u);
  /* 1.5*2=3; (-1.5)*2=-3. */
  check_operation("f32 mul positive", T64_SOFTFLOAT_OP_F32_MUL,
                  UINT32_C(0x3fc00000), UINT32_C(0x40000000), 0u, 1u, 0u,
                  T64_STATUS_OK, UINT32_C(0x40400000), 0u);
  check_operation("f32 mul negative", T64_SOFTFLOAT_OP_F32_MUL,
                  UINT32_C(0xbfc00000), UINT32_C(0x40000000), 0u, 1u, 0u,
                  T64_STATUS_OK, UINT32_C(0xc0400000), 0u);
  /* 6/2=3 and -6/2=-3, both exactly representable. */
  check_operation("f32 div positive", T64_SOFTFLOAT_OP_F32_DIV,
                  UINT32_C(0x40c00000), UINT32_C(0x40000000), 0u, 1u, 0u,
                  T64_STATUS_OK, UINT32_C(0x40400000), 0u);
  check_operation("f32 div negative", T64_SOFTFLOAT_OP_F32_DIV,
                  UINT32_C(0xc0c00000), UINT32_C(0x40000000), 0u, 1u, 0u,
                  T64_STATUS_OK, UINT32_C(0xc0400000), 0u);
  /* sqrt(4)=2. A negative finite operand raises invalid and returns qNaN. */
  check_operation("f32 sqrt positive", T64_SOFTFLOAT_OP_F32_SQRT,
                  UINT32_C(0x40800000), 0u, 0u, 1u, 0u, T64_STATUS_OK,
                  UINT32_C(0x40000000), 0u);
  check_operation("f32 sqrt negative", T64_SOFTFLOAT_OP_F32_SQRT,
                  UINT32_C(0xc0800000), 0u, 0u, 1u, 0u, T64_STATUS_OK,
                  UINT32_C(0x7fc00000), T64_SOFTFLOAT_FLAG_INVALID);

  check_operation("f32 eq true", T64_SOFTFLOAT_OP_F32_EQ,
                  UINT32_C(0xbf800000), UINT32_C(0xbf800000), 0u, 1u, 0u,
                  T64_STATUS_OK, 1u, 0u);
  check_operation("f32 eq false", T64_SOFTFLOAT_OP_F32_EQ,
                  UINT32_C(0xbf800000), UINT32_C(0x3f800000), 0u, 1u, 0u,
                  T64_STATUS_OK, 0u, 0u);
  check_operation("f32 le true", T64_SOFTFLOAT_OP_F32_LE,
                  UINT32_C(0xbf800000), UINT32_C(0x3f800000), 0u, 1u, 0u,
                  T64_STATUS_OK, 1u, 0u);
  check_operation("f32 le false", T64_SOFTFLOAT_OP_F32_LE,
                  UINT32_C(0x3f800000), UINT32_C(0xbf800000), 0u, 1u, 0u,
                  T64_STATUS_OK, 0u, 0u);
  check_operation("f32 lt true", T64_SOFTFLOAT_OP_F32_LT,
                  UINT32_C(0xbf800000), UINT32_C(0x3f800000), 0u, 1u, 0u,
                  T64_STATUS_OK, 1u, 0u);
  check_operation("f32 lt false", T64_SOFTFLOAT_OP_F32_LT,
                  UINT32_C(0x3f800000), UINT32_C(0xbf800000), 0u, 1u, 0u,
                  T64_STATUS_OK, 0u, 0u);
  /* 1.5 is exactly 3*2^-1 in both binary32 and binary64. */
  check_operation("f32 to f64 positive", T64_SOFTFLOAT_OP_F32_TO_F64,
                  UINT32_C(0x3fc00000), 0u, 0u, 1u, 0u, T64_STATUS_OK,
                  UINT64_C(0x3ff8000000000000), 0u);
  check_operation("f32 to f64 negative", T64_SOFTFLOAT_OP_F32_TO_F64,
                  UINT32_C(0xbfc00000), 0u, 0u, 1u, 0u, T64_STATUS_OK,
                  UINT64_C(0xbff8000000000000), 0u);
  /* Ties at 3.5 round to even 4 and -3.5 to even -4 in nearest mode. */
  check_operation("f32 to i32 positive", T64_SOFTFLOAT_OP_F32_TO_I32,
                  UINT32_C(0x40600000), 0u, 0u, 1u, 0u, T64_STATUS_OK, 4u,
                  T64_SOFTFLOAT_FLAG_INEXACT);
  check_operation("f32 to i32 negative", T64_SOFTFLOAT_OP_F32_TO_I32,
                  UINT32_C(0xc0600000), 0u, 0u, 1u, 0u, T64_STATUS_OK,
                  UINT64_C(0xfffffffffffffffc), T64_SOFTFLOAT_FLAG_INEXACT);
  check_operation("f32 to i64 positive", T64_SOFTFLOAT_OP_F32_TO_I64,
                  UINT32_C(0x40600000), 0u, 0u, 1u, 0u, T64_STATUS_OK, 4u,
                  T64_SOFTFLOAT_FLAG_INEXACT);
  check_operation("f32 to i64 negative", T64_SOFTFLOAT_OP_F32_TO_I64,
                  UINT32_C(0xc0600000), 0u, 0u, 1u, 0u, T64_STATUS_OK,
                  UINT64_C(0xfffffffffffffffc), T64_SOFTFLOAT_FLAG_INEXACT);
}

static void test_binary64(void) {
  /* Exact dyadic arithmetic: 1+2=3, 3-1=2, 1.5*2=3 and 6/2=3. */
  check_operation("f64 add positive", T64_SOFTFLOAT_OP_F64_ADD,
                  UINT64_C(0x3ff0000000000000), UINT64_C(0x4000000000000000),
                  0u, 1u, 0u, T64_STATUS_OK, UINT64_C(0x4008000000000000),
                  0u);
  check_operation("f64 add negative", T64_SOFTFLOAT_OP_F64_ADD,
                  UINT64_C(0xbff0000000000000), UINT64_C(0xc000000000000000),
                  0u, 1u, 0u, T64_STATUS_OK, UINT64_C(0xc008000000000000),
                  0u);
  check_operation("f64 sub positive", T64_SOFTFLOAT_OP_F64_SUB,
                  UINT64_C(0x4008000000000000), UINT64_C(0x3ff0000000000000),
                  0u, 1u, 0u, T64_STATUS_OK, UINT64_C(0x4000000000000000),
                  0u);
  check_operation("f64 sub negative", T64_SOFTFLOAT_OP_F64_SUB,
                  UINT64_C(0xc008000000000000), UINT64_C(0xbff0000000000000),
                  0u, 1u, 0u, T64_STATUS_OK, UINT64_C(0xc000000000000000),
                  0u);
  check_operation("f64 mul positive", T64_SOFTFLOAT_OP_F64_MUL,
                  UINT64_C(0x3ff8000000000000), UINT64_C(0x4000000000000000),
                  0u, 1u, 0u, T64_STATUS_OK, UINT64_C(0x4008000000000000),
                  0u);
  check_operation("f64 mul negative", T64_SOFTFLOAT_OP_F64_MUL,
                  UINT64_C(0xbff8000000000000), UINT64_C(0x4000000000000000),
                  0u, 1u, 0u, T64_STATUS_OK, UINT64_C(0xc008000000000000),
                  0u);
  check_operation("f64 div positive", T64_SOFTFLOAT_OP_F64_DIV,
                  UINT64_C(0x4018000000000000), UINT64_C(0x4000000000000000),
                  0u, 1u, 0u, T64_STATUS_OK, UINT64_C(0x4008000000000000),
                  0u);
  check_operation("f64 div negative", T64_SOFTFLOAT_OP_F64_DIV,
                  UINT64_C(0xc018000000000000), UINT64_C(0x4000000000000000),
                  0u, 1u, 0u, T64_STATUS_OK, UINT64_C(0xc008000000000000),
                  0u);
  /* sqrt(4)=2; negative finite sqrt is invalid and returns default qNaN. */
  check_operation("f64 sqrt positive", T64_SOFTFLOAT_OP_F64_SQRT,
                  UINT64_C(0x4010000000000000), 0u, 0u, 1u, 0u,
                  T64_STATUS_OK, UINT64_C(0x4000000000000000), 0u);
  check_operation("f64 sqrt negative", T64_SOFTFLOAT_OP_F64_SQRT,
                  UINT64_C(0xc010000000000000), 0u, 0u, 1u, 0u,
                  T64_STATUS_OK, UINT64_C(0x7ff8000000000000),
                  T64_SOFTFLOAT_FLAG_INVALID);
  check_operation("f64 eq true", T64_SOFTFLOAT_OP_F64_EQ,
                  UINT64_C(0xbff0000000000000), UINT64_C(0xbff0000000000000),
                  0u, 1u, 0u, T64_STATUS_OK, 1u, 0u);
  check_operation("f64 eq false", T64_SOFTFLOAT_OP_F64_EQ,
                  UINT64_C(0xbff0000000000000), UINT64_C(0x3ff0000000000000),
                  0u, 1u, 0u, T64_STATUS_OK, 0u, 0u);
  check_operation("f64 le true", T64_SOFTFLOAT_OP_F64_LE,
                  UINT64_C(0xbff0000000000000), UINT64_C(0x3ff0000000000000),
                  0u, 1u, 0u, T64_STATUS_OK, 1u, 0u);
  check_operation("f64 le false", T64_SOFTFLOAT_OP_F64_LE,
                  UINT64_C(0x3ff0000000000000), UINT64_C(0xbff0000000000000),
                  0u, 1u, 0u, T64_STATUS_OK, 0u, 0u);
  check_operation("f64 lt true", T64_SOFTFLOAT_OP_F64_LT,
                  UINT64_C(0xbff0000000000000), UINT64_C(0x3ff0000000000000),
                  0u, 1u, 0u, T64_STATUS_OK, 1u, 0u);
  check_operation("f64 lt false", T64_SOFTFLOAT_OP_F64_LT,
                  UINT64_C(0x3ff0000000000000), UINT64_C(0xbff0000000000000),
                  0u, 1u, 0u, T64_STATUS_OK, 0u, 0u);
  /* 1.5 is exactly representable in binary32 and binary64. */
  check_operation("f64 to f32 positive", T64_SOFTFLOAT_OP_F64_TO_F32,
                  UINT64_C(0x3ff8000000000000), 0u, 0u, 1u, 0u,
                  T64_STATUS_OK, UINT32_C(0x3fc00000), 0u);
  check_operation("f64 to f32 negative", T64_SOFTFLOAT_OP_F64_TO_F32,
                  UINT64_C(0xbff8000000000000), 0u, 0u, 1u, 0u,
                  T64_STATUS_OK, UINT32_C(0xbfc00000), 0u);
  /* 3.5 and -3.5 are halfway cases: nearest-even gives 4 and -4. */
  check_operation("f64 to i32 positive", T64_SOFTFLOAT_OP_F64_TO_I32,
                  UINT64_C(0x400c000000000000), 0u, 0u, 1u, 0u,
                  T64_STATUS_OK, 4u, T64_SOFTFLOAT_FLAG_INEXACT);
  check_operation("f64 to i32 negative", T64_SOFTFLOAT_OP_F64_TO_I32,
                  UINT64_C(0xc00c000000000000), 0u, 0u, 1u, 0u,
                  T64_STATUS_OK, UINT64_C(0xfffffffffffffffc),
                  T64_SOFTFLOAT_FLAG_INEXACT);
  check_operation("f64 to i64 positive", T64_SOFTFLOAT_OP_F64_TO_I64,
                  UINT64_C(0x400c000000000000), 0u, 0u, 1u, 0u,
                  T64_STATUS_OK, 4u, T64_SOFTFLOAT_FLAG_INEXACT);
  check_operation("f64 to i64 negative", T64_SOFTFLOAT_OP_F64_TO_I64,
                  UINT64_C(0xc00c000000000000), 0u, 0u, 1u, 0u,
                  T64_STATUS_OK, UINT64_C(0xfffffffffffffffc),
                  T64_SOFTFLOAT_FLAG_INEXACT);
  /* Integers ±3 are exact in both binary formats. */
  check_operation("i32 to f32 positive", T64_SOFTFLOAT_OP_I32_TO_F32, 3u,
                  0u, 0u, 1u, 0u, T64_STATUS_OK, UINT32_C(0x40400000), 0u);
  check_operation("i32 to f32 negative", T64_SOFTFLOAT_OP_I32_TO_F32,
                  UINT32_C(0xfffffffd), 0u, 0u, 1u, 0u, T64_STATUS_OK,
                  UINT32_C(0xc0400000), 0u);
  check_operation("i64 to f32 positive", T64_SOFTFLOAT_OP_I64_TO_F32, 3u,
                  0u, 0u, 1u, 0u, T64_STATUS_OK, UINT32_C(0x40400000), 0u);
  check_operation("i64 to f32 negative", T64_SOFTFLOAT_OP_I64_TO_F32,
                  UINT64_C(0xfffffffffffffffd), 0u, 0u, 1u, 0u,
                  T64_STATUS_OK, UINT32_C(0xc0400000), 0u);
  check_operation("i64 to f64 positive", T64_SOFTFLOAT_OP_I64_TO_F64, 3u,
                  0u, 0u, 1u, 0u, T64_STATUS_OK,
                  UINT64_C(0x4008000000000000), 0u);
  check_operation("i64 to f64 negative", T64_SOFTFLOAT_OP_I64_TO_F64,
                  UINT64_C(0xfffffffffffffffd), 0u, 0u, 1u, 0u,
                  T64_STATUS_OK, UINT64_C(0xc008000000000000), 0u);
}

static void test_rounding_and_flags(void) {
  static const uint32_t f32_positive[4] = {
      UINT32_C(0x3eaaaaab), UINT32_C(0x3eaaaaaa), UINT32_C(0x3eaaaaaa),
      UINT32_C(0x3eaaaaab)};
  static const uint32_t f32_negative[4] = {
      UINT32_C(0xbeaaaaab), UINT32_C(0xbeaaaaaa), UINT32_C(0xbeaaaaab),
      UINT32_C(0xbeaaaaaa)};
  static const uint64_t f64_positive[4] = {
      UINT64_C(0x3fd5555555555555), UINT64_C(0x3fd5555555555555),
      UINT64_C(0x3fd5555555555555), UINT64_C(0x3fd5555555555556)};
  static const uint64_t f64_negative[4] = {
      UINT64_C(0xbfd5555555555555), UINT64_C(0xbfd5555555555555),
      UINT64_C(0xbfd5555555555556), UINT64_C(0xbfd5555555555555)};

  /* 1/3 is between adjacent encodings; each mode selects the directed bound. */
  for (uint8_t mode = 0u; mode < 4u; ++mode) {
    check_operation("f32 one-third positive rounding",
                    T64_SOFTFLOAT_OP_F32_DIV, UINT32_C(0x3f800000),
                    UINT32_C(0x40400000), mode, 1u, 0u, T64_STATUS_OK,
                    f32_positive[mode], T64_SOFTFLOAT_FLAG_INEXACT);
    check_operation("f32 one-third negative rounding",
                    T64_SOFTFLOAT_OP_F32_DIV, UINT32_C(0xbf800000),
                    UINT32_C(0x40400000), mode, 1u, 0u, T64_STATUS_OK,
                    f32_negative[mode], T64_SOFTFLOAT_FLAG_INEXACT);
    check_operation("f64 one-third positive rounding",
                    T64_SOFTFLOAT_OP_F64_DIV,
                    UINT64_C(0x3ff0000000000000),
                    UINT64_C(0x4008000000000000), mode, 1u, 0u,
                    T64_STATUS_OK, f64_positive[mode],
                    T64_SOFTFLOAT_FLAG_INEXACT);
    check_operation("f64 one-third negative rounding",
                    T64_SOFTFLOAT_OP_F64_DIV,
                    UINT64_C(0xbff0000000000000),
                    UINT64_C(0x4008000000000000), mode, 1u, 0u,
                    T64_STATUS_OK, f64_negative[mode],
                    T64_SOFTFLOAT_FLAG_INEXACT);
  }

  /* Five IEEE status categories: inexact, underflow, overflow, divide-zero,
     and invalid. Expected results follow exact powers/rational values. */
  check_operation("f32 inexact 1/3", T64_SOFTFLOAT_OP_F32_DIV,
                  UINT32_C(0x3f800000), UINT32_C(0x40400000), 0u, 1u, 0u,
                  T64_STATUS_OK, UINT32_C(0x3eaaaaab),
                  T64_SOFTFLOAT_FLAG_INEXACT);
  check_operation("f64 inexact 1/3", T64_SOFTFLOAT_OP_F64_DIV,
                  UINT64_C(0x3ff0000000000000),
                  UINT64_C(0x4008000000000000), 0u, 1u, 0u,
                  T64_STATUS_OK, UINT64_C(0x3fd5555555555555),
                  T64_SOFTFLOAT_FLAG_INEXACT);
  /* 2^-149 * 2^-1 is halfway between 0 and 2^-149; ties-to-even gives 0. */
  check_operation("f32 underflow", T64_SOFTFLOAT_OP_F32_MUL,
                  UINT32_C(0x00000001), UINT32_C(0x3f000000), 0u, 1u, 0u,
                  T64_STATUS_OK, 0u,
                  T64_SOFTFLOAT_FLAG_UNDERFLOW | T64_SOFTFLOAT_FLAG_INEXACT);
  /* 2^-1074 * 2^-1 is the corresponding binary64 halfway value. */
  check_operation("f64 underflow", T64_SOFTFLOAT_OP_F64_MUL,
                  UINT64_C(0x0000000000000001),
                  UINT64_C(0x3fe0000000000000), 0u, 1u, 0u,
                  T64_STATUS_OK, 0u,
                  T64_SOFTFLOAT_FLAG_UNDERFLOW | T64_SOFTFLOAT_FLAG_INEXACT);
  /* Largest finite times two overflows to positive infinity. */
  check_operation("f32 overflow", T64_SOFTFLOAT_OP_F32_MUL,
                  UINT32_C(0x7f7fffff), UINT32_C(0x40000000), 0u, 1u, 0u,
                  T64_STATUS_OK, UINT32_C(0x7f800000),
                  T64_SOFTFLOAT_FLAG_OVERFLOW | T64_SOFTFLOAT_FLAG_INEXACT);
  check_operation("f64 overflow", T64_SOFTFLOAT_OP_F64_MUL,
                  UINT64_C(0x7fefffffffffffff),
                  UINT64_C(0x4000000000000000), 0u, 1u, 0u,
                  T64_STATUS_OK, UINT64_C(0x7ff0000000000000),
                  T64_SOFTFLOAT_FLAG_OVERFLOW | T64_SOFTFLOAT_FLAG_INEXACT);
  check_operation("f32 divide by zero", T64_SOFTFLOAT_OP_F32_DIV,
                  UINT32_C(0x3f800000), 0u, 0u, 1u, 0u, T64_STATUS_OK,
                  UINT32_C(0x7f800000), T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO);
  check_operation("f64 divide by zero", T64_SOFTFLOAT_OP_F64_DIV,
                  UINT64_C(0x3ff0000000000000), 0u, 0u, 1u, 0u,
                  T64_STATUS_OK, UINT64_C(0x7ff0000000000000),
                  T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO);
  check_operation("f32 invalid zero/zero", T64_SOFTFLOAT_OP_F32_DIV, 0u, 0u,
                  0u, 1u, 0u, T64_STATUS_OK, UINT32_C(0x7fc00000),
                  T64_SOFTFLOAT_FLAG_INVALID);
  check_operation("f64 invalid zero/zero", T64_SOFTFLOAT_OP_F64_DIV, 0u, 0u,
                  0u, 1u, 0u, T64_STATUS_OK,
                  UINT64_C(0x7ff8000000000000), T64_SOFTFLOAT_FLAG_INVALID);
  /* Authored raw NaN policy keeps the first NaN payload and quiets it; an
     input signaling NaN also raises invalid. */
  check_operation("f32 first NaN payload", T64_SOFTFLOAT_OP_F32_ADD,
                  UINT32_C(0x7fc12345), UINT32_C(0x7fc54321), 0u, 1u, 0u,
                  T64_STATUS_OK, UINT32_C(0x7fc12345), 0u);
  check_operation("f64 first NaN payload", T64_SOFTFLOAT_OP_F64_ADD,
                  UINT64_C(0x7ff8123456789abc),
                  UINT64_C(0x7ff8543212345678), 0u, 1u, 0u, T64_STATUS_OK,
                  UINT64_C(0x7ff8123456789abc), 0u);
  check_operation("f32 quiet signaling NaN", T64_SOFTFLOAT_OP_F32_ADD,
                  UINT32_C(0x7f812345), UINT32_C(0x7fc54321), 0u, 1u, 0u,
                  T64_STATUS_OK, UINT32_C(0x7fc12345),
                  T64_SOFTFLOAT_FLAG_INVALID);
  /* Invalid raw integer conversion returns the selected signed minimum
     sentinel and raises invalid; VR4300 COP1 prechecks remain CPU-owned. */
  check_operation("f32 NaN to i32 sentinel", T64_SOFTFLOAT_OP_F32_TO_I32,
                  UINT32_C(0x7fc00000), 0u, 0u, 1u, 0u, T64_STATUS_OK,
                  UINT64_C(0xffffffff80000000), T64_SOFTFLOAT_FLAG_INVALID);

  /* maxSubnormal*(1+2^-23) = minNormal-2^-172, just below minNormal but
     within half an ulp. It rounds to minNormal, exercising both tininess
     modes while remaining inexact. */
  check_operation("f32 tininess before", T64_SOFTFLOAT_OP_F32_MUL,
                  UINT32_C(0x007fffff), UINT32_C(0x3f800001), 0u, 0u, 0u,
                  T64_STATUS_OK, UINT32_C(0x00800000),
                  T64_SOFTFLOAT_FLAG_UNDERFLOW | T64_SOFTFLOAT_FLAG_INEXACT);
  check_operation("f32 tininess after", T64_SOFTFLOAT_OP_F32_MUL,
                  UINT32_C(0x007fffff), UINT32_C(0x3f800001), 0u, 1u, 0u,
                  T64_STATUS_OK, UINT32_C(0x00800000),
                  T64_SOFTFLOAT_FLAG_INEXACT);
  /* Same exact derivation for binary64: minNormal-2^-1126 rounds upward. */
  check_operation("f64 tininess before", T64_SOFTFLOAT_OP_F64_MUL,
                  UINT64_C(0x000fffffffffffff),
                  UINT64_C(0x3ff0000000000001), 0u, 0u, 0u, T64_STATUS_OK,
                  UINT64_C(0x0010000000000000),
                  T64_SOFTFLOAT_FLAG_UNDERFLOW | T64_SOFTFLOAT_FLAG_INEXACT);
  check_operation("f64 tininess after", T64_SOFTFLOAT_OP_F64_MUL,
                  UINT64_C(0x000fffffffffffff),
                  UINT64_C(0x3ff0000000000001), 0u, 1u, 0u, T64_STATUS_OK,
                  UINT64_C(0x0010000000000000), T64_SOFTFLOAT_FLAG_INEXACT);
}

static void test_signed_conversion_boundaries(void) {
  for (uint8_t rounding = 0u; rounding < 4u; ++rounding) {
    t64_softfloat_context first = {
        rounding, T64_SOFTFLOAT_TININESS_AFTER_ROUNDING, 0u};
    t64_softfloat_context second = {
        (uint8_t)(3u - rounding), T64_SOFTFLOAT_TININESS_BEFORE_ROUNDING,
        T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO};
    t64_softfloat_result result = {UINT64_C(0xa5a5a5a5a5a5a5a5), 0xffu};

    /* 2^31 is an exact binary32 integer, one above INT32_MAX. */
    T64_CHECK_INT(t64_softfloat_execute(T64_SOFTFLOAT_OP_F32_TO_I32, &first,
                                        UINT32_C(0x4f000000), 0u, &result),
                  T64_STATUS_OK);
    T64_CHECK(result.bits == UINT64_C(0xffffffff80000000));
    T64_CHECK_INT(result.exception_flags, T64_SOFTFLOAT_FLAG_INVALID);
    T64_CHECK_INT(first.exception_flags, T64_SOFTFLOAT_FLAG_INVALID);

    /* An exact operation on a differently configured context is isolated. */
    T64_CHECK_INT(t64_softfloat_execute(T64_SOFTFLOAT_OP_F64_ADD, &second,
                                        UINT64_C(0x3ff0000000000000),
                                        UINT64_C(0x4000000000000000), &result),
                  T64_STATUS_OK);
    T64_CHECK(result.bits == UINT64_C(0x4008000000000000));
    T64_CHECK_INT(result.exception_flags,
                  T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO);
    T64_CHECK_INT(second.rounding_mode, (uint8_t)(3u - rounding));
    T64_CHECK_INT(second.tininess_mode,
                  T64_SOFTFLOAT_TININESS_BEFORE_ROUNDING);
    T64_CHECK_INT(second.exception_flags,
                  T64_SOFTFLOAT_FLAG_DIVIDE_BY_ZERO);
    T64_CHECK_INT(first.rounding_mode, rounding);
    T64_CHECK_INT(first.tininess_mode,
                  T64_SOFTFLOAT_TININESS_AFTER_ROUNDING);
    T64_CHECK_INT(first.exception_flags, T64_SOFTFLOAT_FLAG_INVALID);

    /* Reset only the first context to make the next operation's flags local. */
    first.exception_flags = 0u;
    /* 2^63 is an exact binary64 integer, one above INT64_MAX. */
    T64_CHECK_INT(t64_softfloat_execute(T64_SOFTFLOAT_OP_F64_TO_I64, &first,
                                        UINT64_C(0x43e0000000000000), 0u,
                                        &result),
                  T64_STATUS_OK);
    T64_CHECK(result.bits == UINT64_C(0x8000000000000000));
    T64_CHECK_INT(result.exception_flags, T64_SOFTFLOAT_FLAG_INVALID);
    T64_CHECK_INT(first.exception_flags, T64_SOFTFLOAT_FLAG_INVALID);
  }
}

static void test_context_isolation(void) {
  test_signed_conversion_boundaries();

  t64_softfloat_context nearest = {0u, 0u, T64_SOFTFLOAT_FLAG_INVALID};
  t64_softfloat_context upward = {3u, 1u, T64_SOFTFLOAT_FLAG_OVERFLOW};
  t64_softfloat_result nearest_result = {0u, 0u};
  t64_softfloat_result upward_result = {0u, 0u};
  t64_status nearest_status;
  t64_status upward_status;

  /* -1/3 distinguishes nearest-even from rounding toward +infinity. */
  nearest_status = t64_softfloat_execute(
      T64_SOFTFLOAT_OP_F32_DIV, &nearest, UINT32_C(0xbf800000),
      UINT32_C(0x40400000), &nearest_result);
  upward_status = t64_softfloat_execute(
      T64_SOFTFLOAT_OP_F32_DIV, &upward, UINT32_C(0xbf800000),
      UINT32_C(0x40400000), &upward_result);
  T64_CHECK_INT(nearest_status, T64_STATUS_OK);
  T64_CHECK_INT(upward_status, T64_STATUS_OK);
  T64_CHECK(nearest_result.bits == UINT32_C(0xbeaaaaab));
  T64_CHECK(upward_result.bits == UINT32_C(0xbeaaaaaa));
  T64_CHECK_INT(nearest.exception_flags,
                T64_SOFTFLOAT_FLAG_INVALID | T64_SOFTFLOAT_FLAG_INEXACT);
  T64_CHECK_INT(upward.exception_flags,
                T64_SOFTFLOAT_FLAG_OVERFLOW | T64_SOFTFLOAT_FLAG_INEXACT);
  T64_CHECK_INT(nearest_result.exception_flags, nearest.exception_flags);
  T64_CHECK_INT(upward_result.exception_flags, upward.exception_flags);

  /* Change both to nearest-even, then interleave maxSubnormal*(1+ulp). This
     exact product is just below minNormal but rounds to it; each context's
     independent tininess rule is observed. */
  nearest.rounding_mode = 0u;
  nearest.tininess_mode = T64_SOFTFLOAT_TININESS_BEFORE_ROUNDING;
  nearest.exception_flags = 0u;
  upward.rounding_mode = 0u;
  upward.tininess_mode = T64_SOFTFLOAT_TININESS_AFTER_ROUNDING;
  upward.exception_flags = 0u;
  nearest_status = t64_softfloat_execute(
      T64_SOFTFLOAT_OP_F32_MUL, &nearest, UINT32_C(0x007fffff),
      UINT32_C(0x3f800001), &nearest_result);
  upward_status = t64_softfloat_execute(
      T64_SOFTFLOAT_OP_F32_MUL, &upward, UINT32_C(0x007fffff),
      UINT32_C(0x3f800001), &upward_result);
  T64_CHECK_INT(nearest_status, T64_STATUS_OK);
  T64_CHECK_INT(upward_status, T64_STATUS_OK);
  T64_CHECK(nearest_result.bits == UINT32_C(0x00800000));
  T64_CHECK(upward_result.bits == UINT32_C(0x00800000));
  T64_CHECK_INT(nearest.exception_flags,
                T64_SOFTFLOAT_FLAG_UNDERFLOW | T64_SOFTFLOAT_FLAG_INEXACT);
  T64_CHECK_INT(upward.exception_flags, T64_SOFTFLOAT_FLAG_INEXACT);
  T64_CHECK_INT(nearest_result.exception_flags, nearest.exception_flags);
  T64_CHECK_INT(upward_result.exception_flags, upward.exception_flags);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    (void)fprintf(stderr,
                  "usage: test_softfloat --softfloat32|--softfloat64|--isolation\n");
    return 2;
  }
  if (strcmp(argv[1], "--softfloat32") == 0) {
    test_existing_conversion_and_binary32();
    test_rounding_and_flags();
    test_raw_edge_matrix();
    return T64_TEST_FINISH("unit.core.softfloat32");
  }
  if (strcmp(argv[1], "--softfloat64") == 0) {
    test_binary64();
    test_rounding_and_flags();
    test_raw_edge_matrix();
    return T64_TEST_FINISH("unit.core.softfloat64");
  }
  if (strcmp(argv[1], "--isolation") == 0) {
    test_context_isolation();
    return T64_TEST_FINISH("unit.core.softfloat-isolation");
  }
  (void)fprintf(stderr,
                "usage: test_softfloat --softfloat32|--softfloat64|--isolation\n");
  return 2;
}
