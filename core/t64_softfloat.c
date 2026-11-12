#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>

#include "softfloat/softfloat.h"

void softfloat_raiseFlags(t64_softfloat_context *context, uint_fast8_t flags) {
  context->exception_flags =
      (uint8_t)(context->exception_flags | (uint8_t)flags);
}

static bool t64_softfloat_context_valid(const t64_softfloat_context *context) {
  return context != NULL && context->rounding_mode <= 3u &&
         context->tininess_mode <= 1u;
}

static int32_t t64_i32_from_bits(uint32_t bits) {
  if (bits < UINT32_C(0x80000000)) {
    return (int32_t)bits;
  }
  return -1 - (int32_t)~bits;
}

static int64_t t64_i64_from_bits(uint64_t bits) {
  if (bits <= (uint64_t)INT64_MAX) {
    return (int64_t)bits;
  }
  return -1 - (int64_t)~bits;
}

static void t64_softfloat_store(t64_softfloat_context *context,
                                uint64_t bits,
                                t64_softfloat_result *result_out) {
  t64_softfloat_result result = {bits, context->exception_flags};
  *result_out = result;
}

t64_status t64_softfloat_i32_to_f64(int32_t input,
                                    t64_softfloat_context *context,
                                    t64_softfloat_result *result_out) {
  if (!t64_softfloat_context_valid(context) || result_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  return t64_softfloat_execute(T64_SOFTFLOAT_OP_I32_TO_F64, context,
                               (uint32_t)input, 0u, result_out);
}

t64_status t64_softfloat_execute(t64_softfloat_operation operation,
                                 t64_softfloat_context *context,
                                 uint64_t a_bits, uint64_t b_bits,
                                 t64_softfloat_result *result_out) {
  uint64_t bits;
  float32_t f32_a = {(uint32_t)a_bits};
  float32_t f32_b = {(uint32_t)b_bits};
  float64_t f64_a = {a_bits};
  float64_t f64_b = {b_bits};

  if (!t64_softfloat_context_valid(context) || result_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  switch (operation) {
    case T64_SOFTFLOAT_OP_I32_TO_F64:
      bits = i32_to_f64(context, t64_i32_from_bits((uint32_t)a_bits)).v;
      break;
    case T64_SOFTFLOAT_OP_F32_ADD:
      bits = f32_add(context, f32_a, f32_b).v;
      break;
    case T64_SOFTFLOAT_OP_F32_SUB:
      bits = f32_sub(context, f32_a, f32_b).v;
      break;
    case T64_SOFTFLOAT_OP_F32_MUL:
      bits = f32_mul(context, f32_a, f32_b).v;
      break;
    case T64_SOFTFLOAT_OP_F32_DIV:
      bits = f32_div(context, f32_a, f32_b).v;
      break;
    case T64_SOFTFLOAT_OP_F32_SQRT:
      bits = f32_sqrt(context, f32_a).v;
      break;
    case T64_SOFTFLOAT_OP_F32_EQ:
      bits = f32_eq(context, f32_a, f32_b) ? 1u : 0u;
      break;
    case T64_SOFTFLOAT_OP_F32_LE:
      bits = f32_le(context, f32_a, f32_b) ? 1u : 0u;
      break;
    case T64_SOFTFLOAT_OP_F32_LT:
      bits = f32_lt(context, f32_a, f32_b) ? 1u : 0u;
      break;
    case T64_SOFTFLOAT_OP_F32_TO_F64:
      bits = f32_to_f64(context, f32_a).v;
      break;
    case T64_SOFTFLOAT_OP_F32_TO_I32:
      bits = (uint64_t)(int64_t)f32_to_i32(
          context, f32_a, context->rounding_mode, true);
      break;
    case T64_SOFTFLOAT_OP_F32_TO_I64:
      bits = (uint64_t)f32_to_i64(
          context, f32_a, context->rounding_mode, true);
      break;
    case T64_SOFTFLOAT_OP_F64_ADD:
      bits = f64_add(context, f64_a, f64_b).v;
      break;
    case T64_SOFTFLOAT_OP_F64_SUB:
      bits = f64_sub(context, f64_a, f64_b).v;
      break;
    case T64_SOFTFLOAT_OP_F64_MUL:
      bits = f64_mul(context, f64_a, f64_b).v;
      break;
    case T64_SOFTFLOAT_OP_F64_DIV:
      bits = f64_div(context, f64_a, f64_b).v;
      break;
    case T64_SOFTFLOAT_OP_F64_SQRT:
      bits = f64_sqrt(context, f64_a).v;
      break;
    case T64_SOFTFLOAT_OP_F64_EQ:
      bits = f64_eq(context, f64_a, f64_b) ? 1u : 0u;
      break;
    case T64_SOFTFLOAT_OP_F64_LE:
      bits = f64_le(context, f64_a, f64_b) ? 1u : 0u;
      break;
    case T64_SOFTFLOAT_OP_F64_LT:
      bits = f64_lt(context, f64_a, f64_b) ? 1u : 0u;
      break;
    case T64_SOFTFLOAT_OP_F64_TO_F32:
      bits = f64_to_f32(context, f64_a).v;
      break;
    case T64_SOFTFLOAT_OP_F64_TO_I32:
      bits = (uint64_t)(int64_t)f64_to_i32(
          context, f64_a, context->rounding_mode, true);
      break;
    case T64_SOFTFLOAT_OP_F64_TO_I64:
      bits = (uint64_t)f64_to_i64(
          context, f64_a, context->rounding_mode, true);
      break;
    case T64_SOFTFLOAT_OP_I32_TO_F32:
      bits = i32_to_f32(context, t64_i32_from_bits((uint32_t)a_bits)).v;
      break;
    case T64_SOFTFLOAT_OP_I64_TO_F32:
      bits = i64_to_f32(context, t64_i64_from_bits(a_bits)).v;
      break;
    case T64_SOFTFLOAT_OP_I64_TO_F64:
      bits = i64_to_f64(context, t64_i64_from_bits(a_bits)).v;
      break;
    default:
      return T64_STATUS_NOT_READY;
  }
  t64_softfloat_store(context, bits, result_out);
  return T64_STATUS_OK;
}
