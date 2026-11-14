#include "t64_internal.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FUZZ_CASES_PER_OPCODE UINT32_C(256)
#define RSP_STRESS_MAX_ITERATIONS UINT32_C(100000)

static bool full_corpus_mode;
static uint32_t reported_opcode_count;

static void report_opcode(uint32_t local_index, const char *name,
                          uint32_t accepted, uint32_t seed) {
  uint32_t number = local_index;
  if (full_corpus_mode) number = ++reported_opcode_count;
  (void)printf("ok %u - %s accepted=%u refused=0 seed=%08x\n", number,
               name, accepted, seed);
}

typedef struct {
  t64_host_allocator allocator;
  t64_machine *machine;
} rsp_fuzz_fixture;

static bool fixture_allocate(void *context, size_t size, size_t alignment,
                             t64_host_storage *storage_out) {
  void *data;
  (void)context;
  if (storage_out == NULL || size == 0u || alignment == 0u) {
    return false;
  }
  data = malloc(size);
  if (data == NULL || (uintptr_t)data % alignment != 0u) {
    free(data);
    return false;
  }
  storage_out->data = data;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  return true;
}

static void fixture_release(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static bool fixture_create(rsp_fuzz_fixture *fixture) {
  const t64_profile_descriptor *profile = NULL;
  t64_host_storage image = {NULL, 0u, 0u};

  (void)memset(fixture, 0, sizeof *fixture);
  fixture->allocator.allocate = fixture_allocate;
  fixture->allocator.release = fixture_release;
  if (t64_profile_lookup("ntsc-cart-4m-openipl3-rsp-functional-v1", &profile) !=
          T64_STATUS_OK ||
      profile == NULL ||
      t64_machine_create(&fixture->allocator, profile, &fixture->machine) !=
          T64_STATUS_OK ||
      fixture->machine == NULL ||
      !fixture_allocate(fixture, 2u, 1u, &image)) {
    return false;
  }
  ((uint8_t *)image.data)[0] = UINT8_C(0x5a);
  ((uint8_t *)image.data)[1] = UINT8_C(0xa5);
  fixture->machine->image_storage = image;
  fixture->machine->image_length = 2u;
  fixture->machine->has_image = true;
  return true;
}

static void fixture_destroy(rsp_fuzz_fixture *fixture) {
  if (fixture != NULL && fixture->machine != NULL) {
    t64_machine_destroy(fixture->machine);
    fixture->machine = NULL;
  }
}

static uint32_t encode_vector(uint32_t element, uint32_t vt, uint32_t vs,
                              uint32_t vd, uint32_t function) {
  return (UINT32_C(0x12) << 26) | (UINT32_C(0x10) << 21) |
         ((element & UINT32_C(0x0f)) << 21) | (vt << 16) | (vs << 11) |
         (vd << 6) | function;
}

static uint32_t next_random(uint32_t *state) {
  uint32_t value = *state;
  value ^= value << 13;
  value ^= value >> 17;
  value ^= value << 5;
  *state = value;
  return value;
}

static uint16_t read_lane(const uint8_t vector[16], uint32_t lane) {
  return (uint16_t)(((uint16_t)vector[lane * 2u] << 8) |
                    vector[lane * 2u + 1u]);
}

static bool execute_fetched(t64_machine *machine, uint32_t instruction) {
  t64_stop_request request;
  t64_stop_result result = {0u, 0u};
  uint32_t pc = machine->rsp.pc & UINT32_C(0x0ffc);
  uint64_t retired_before = machine->rsp.retired_instructions;

  machine->sp_imem[pc] = (uint8_t)(instruction >> 24);
  machine->sp_imem[(pc + 1u) & UINT32_C(0x0fff)] =
      (uint8_t)(instruction >> 16);
  machine->sp_imem[(pc + 2u) & UINT32_C(0x0fff)] =
      (uint8_t)(instruction >> 8);
  machine->sp_imem[(pc + 3u) & UINT32_C(0x0fff)] = (uint8_t)instruction;
  if (t64_rsp_start(machine) != T64_STATUS_OK) {
    return false;
  }
  request.kind = T64_STOP_GUEST_TICK;
  request.value = machine->tick + UINT64_C(1);
  return t64_machine_advance(machine, &request, &result) == T64_STATUS_OK &&
         machine->rsp_observed_outcome == T64_RSP_ATTEMPT_RETIRED &&
         machine->rsp_observed_opcode == instruction &&
         machine->rsp.retired_instructions == retired_before + UINT64_C(1) &&
         result.tick == request.value;
}

static bool execute_fetched_until_retired(t64_machine *machine,
                                          uint32_t instruction,
                                          uint32_t maximum_ticks) {
  uint32_t pc = machine->rsp.pc & UINT32_C(0x0ffc);
  uint64_t retired_before = machine->rsp.retired_instructions;
  machine->sp_imem[pc] = (uint8_t)(instruction >> 24u);
  machine->sp_imem[(pc + 1u) & UINT32_C(0x0fff)] =
      (uint8_t)(instruction >> 16u);
  machine->sp_imem[(pc + 2u) & UINT32_C(0x0fff)] =
      (uint8_t)(instruction >> 8u);
  machine->sp_imem[(pc + 3u) & UINT32_C(0x0fff)] = (uint8_t)instruction;
  if (t64_rsp_start(machine) != T64_STATUS_OK) return false;
  for (uint32_t elapsed = 0u; elapsed < maximum_ticks; elapsed++) {
    t64_stop_request request;
    t64_stop_result result = {0u, 0u};
    request.kind = T64_STOP_GUEST_TICK;
    request.value = machine->tick + UINT64_C(1);
    if (t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
        result.tick != request.value ||
        machine->rsp_observed_opcode != instruction) {
      return false;
    }
    if (machine->rsp_observed_outcome == T64_RSP_ATTEMPT_RETIRED) {
      return machine->rsp.retired_instructions == retired_before + 1u;
    }
    if (machine->rsp_observed_outcome != T64_RSP_ATTEMPT_NONE ||
        machine->rsp_observed_ready_tick <= machine->tick) {
      return false;
    }
  }
  return false;
}

static bool execute_one_vand_tuple(rsp_fuzz_fixture *fixture,
                                   uint32_t *random_state) {
  t64_machine *machine = fixture->machine;
  uint8_t vectors_before[32][16];
  uint8_t dmem_before[T64_SP_BANK_BYTES];
  uint64_t accumulators_before[8];
  uint32_t gpr_before[32];
  uint32_t divider_input_before;
  uint32_t divider_result_before;
  bool divider_high_valid_before;
  uint16_t vco_before;
  uint16_t vcc_before;
  uint8_t vce_before;
  uint32_t instruction = encode_vector(0u, 2u, 1u, 3u, UINT32_C(0x28));
  uint32_t reg;
  uint32_t byte;
  uint32_t lane;
  bool valid = true;

  t64_machine_reset_execution_state(machine);
  (void)memset(&machine->rsp, 0, sizeof machine->rsp);
  (void)memset(&machine->cpu, 0, sizeof machine->cpu);
  (void)memset(machine->sp_dmem, 0, sizeof machine->sp_dmem);
  (void)memset(machine->sp_imem, 0, sizeof machine->sp_imem);
  machine->rsp.sp_status = UINT32_C(1);
  for (reg = 0u; reg < 32u; reg++) {
    machine->rsp.gpr[reg] = next_random(random_state);
    for (byte = 0u; byte < 16u; byte++) {
      machine->rsp.vector[reg][byte] = (uint8_t)next_random(random_state);
    }
  }
  machine->rsp.gpr[0] = 0u;
  for (lane = 0u; lane < 8u; lane++) {
    machine->rsp.accumulator[lane] =
        ((uint64_t)next_random(random_state) << 16) |
        (uint64_t)(next_random(random_state) & UINT32_C(0xffff));
  }
  machine->rsp.vco = (uint16_t)next_random(random_state);
  machine->rsp.vcc = (uint16_t)next_random(random_state);
  machine->rsp.vce = (uint8_t)next_random(random_state);
  machine->rsp.divider_input = next_random(random_state);
  machine->rsp.divider_result = next_random(random_state);
  machine->rsp.divider_high_valid = (next_random(random_state) & 1u) != 0u;
  for (byte = 0u; byte < T64_SP_BANK_BYTES; byte++) {
    machine->sp_dmem[byte] = (uint8_t)next_random(random_state);
  }
  vco_before = machine->rsp.vco;
  vcc_before = machine->rsp.vcc;
  vce_before = machine->rsp.vce;
  (void)memcpy(vectors_before, machine->rsp.vector, sizeof vectors_before);
  (void)memcpy(accumulators_before, machine->rsp.accumulator,
               sizeof accumulators_before);
  (void)memcpy(gpr_before, machine->rsp.gpr, sizeof gpr_before);
  (void)memcpy(dmem_before, machine->sp_dmem, sizeof dmem_before);
  divider_input_before = machine->rsp.divider_input;
  divider_result_before = machine->rsp.divider_result;
  divider_high_valid_before = machine->rsp.divider_high_valid;
  if (!execute_fetched(machine, instruction)) {
    return false;
  }
  for (lane = 0u; lane < 8u; lane++) {
    uint16_t expected = (uint16_t)(read_lane(vectors_before[1], lane) &
                                   read_lane(vectors_before[2], lane));
    uint64_t expected_accumulator =
        (accumulators_before[lane] & ~UINT64_C(0xffff)) | expected;
    if (read_lane(machine->rsp.vector[3], lane) != expected ||
        machine->rsp.accumulator[lane] != expected_accumulator) {
      valid = false;
    }
  }
  for (reg = 0u; reg < 32u; reg++) {
    if (reg != 3u &&
        memcmp(machine->rsp.vector[reg], vectors_before[reg], 16u) != 0) {
      valid = false;
    }
  }
  if (machine->rsp.vco != vco_before || machine->rsp.vcc != vcc_before ||
      machine->rsp.vce != vce_before ||
      memcmp(machine->rsp.gpr, gpr_before, sizeof gpr_before) != 0 ||
      memcmp(machine->sp_dmem, dmem_before, sizeof dmem_before) != 0 ||
      machine->rsp.divider_input != divider_input_before ||
      machine->rsp.divider_result != divider_result_before ||
      machine->rsp.divider_high_valid != divider_high_valid_before) {
    valid = false;
  }
  return valid;
}

static void write_expected_lane(uint8_t vector[16], uint32_t lane,
                                uint16_t value) {
  vector[lane * 2u] = (uint8_t)(value >> 8);
  vector[lane * 2u + 1u] = (uint8_t)value;
}

static void seed_machine(rsp_fuzz_fixture *fixture, uint32_t *random_state) {
  t64_machine *machine = fixture->machine;
  uint32_t reg;
  uint32_t byte;
  uint32_t lane;
  t64_machine_reset_execution_state(machine);
  (void)memset(&machine->rsp, 0, sizeof machine->rsp);
  (void)memset(&machine->cpu, 0, sizeof machine->cpu);
  (void)memset(machine->sp_dmem, 0, sizeof machine->sp_dmem);
  (void)memset(machine->sp_imem, 0, sizeof machine->sp_imem);
  machine->rsp.sp_status = UINT32_C(1);
  for (reg = 0u; reg < 32u; reg++) {
    machine->rsp.gpr[reg] = next_random(random_state);
    for (byte = 0u; byte < 16u; byte++) {
      machine->rsp.vector[reg][byte] = (uint8_t)next_random(random_state);
    }
  }
  machine->rsp.gpr[0] = 0u;
  for (lane = 0u; lane < 8u; lane++) {
    machine->rsp.accumulator[lane] =
        (((uint64_t)next_random(random_state) << 16) |
         (uint64_t)(next_random(random_state) & UINT32_C(0xffff))) &
        UINT64_C(0x0000ffffffffffff);
  }
  machine->rsp.vco = (uint16_t)next_random(random_state);
  machine->rsp.vcc = (uint16_t)next_random(random_state);
  machine->rsp.vce = (uint8_t)next_random(random_state);
  machine->rsp.divider_input = next_random(random_state);
  machine->rsp.divider_result = next_random(random_state);
  machine->rsp.divider_high_valid = (next_random(random_state) & 1u) != 0u;
  for (byte = 0u; byte < T64_SP_BANK_BYTES; byte++) {
    machine->sp_dmem[byte] = (uint8_t)next_random(random_state);
  }
}

static int32_t fuzz_s16(uint16_t value) {
  return (value & UINT16_C(0x8000)) != 0u
             ? (int32_t)value - INT32_C(65536)
             : (int32_t)value;
}

static int64_t fuzz_s48(uint64_t value) {
  uint64_t bits = value & UINT64_C(0x0000ffffffffffff);
  if ((bits & (UINT64_C(1) << 47)) == 0u) return (int64_t)bits;
  return -(int64_t)(((~bits) & UINT64_C(0x0000ffffffffffff)) + 1u);
}

static uint16_t fuzz_clamp_signed(uint64_t value, uint32_t shift) {
  uint32_t sign_bit = shift + 15u;
  uint32_t width = 48u - sign_bit;
  uint64_t mask = (UINT64_C(1) << width) - 1u;
  uint64_t upper = (value & UINT64_C(0x0000ffffffffffff)) >> sign_bit;
  if (upper == 0u || upper == mask) return (uint16_t)(value >> shift);
  return (value & (UINT64_C(1) << 47)) != 0u ? 0x8000u : 0x7fffu;
}

static uint16_t fuzz_clamp_unsigned(uint64_t value, uint32_t shift) {
  uint64_t upper = (value & UINT64_C(0x0000ffffffffffff)) >> (shift + 16u);
  if ((value & (UINT64_C(1) << 47)) != 0u) return 0u;
  return upper != 0u ? 0xffffu : (uint16_t)(value >> shift);
}

static bool fuzz_select_lane(uint32_t element, uint32_t lane,
                             uint32_t *selected) {
  if (element == 1u || element > 15u) return false;
  if (element == 0u) *selected = lane;
  else if (element <= 3u) *selected = (element & 1u) + (lane & 6u);
  else if (element <= 7u) *selected = (element & 3u) + (lane & 4u);
  else *selected = element & 7u;
  return true;
}

static bool reference_vu(uint32_t function, uint32_t element, uint32_t vs,
                         uint32_t vt, uint32_t vd,
                         uint8_t vectors[32][16], uint64_t accumulator[8],
                         uint16_t *vco, uint16_t *vcc, uint8_t *vce) {
  uint8_t old_vectors[32][16];
  uint64_t old_acc[8];
  uint16_t old_vco = *vco;
  uint16_t old_vcc = *vcc;
  uint8_t old_vce = *vce;
  uint16_t next_vco = old_vco;
  uint16_t next_vcc = old_vcc;
  uint8_t next_vce = old_vce;
  uint32_t lane;
  (void)memcpy(old_vectors, vectors, sizeof old_vectors);
  (void)memcpy(old_acc, accumulator, sizeof old_acc);
  if (function == 0x14u || function == 0x15u || function == 0x20u ||
      function == 0x21u || function == 0x23u || function == 0x25u ||
      function == 0x26u) {
    next_vco = 0u;
  }
  if (function == 0x20u || function == 0x21u || function == 0x22u ||
      function == 0x23u ||
      function == 0x25u || function == 0x26u) {
    next_vcc = 0u;
  }
  if (function == 0x25u) next_vce = 0u;
  for (lane = 0u; lane < 8u; lane++) {
    uint32_t right_lane = lane;
    uint16_t a = read_lane(old_vectors[vs], lane);
    uint16_t b;
    uint16_t result = 0u;
    uint64_t acc = old_acc[lane] & UINT64_C(0x0000ffffffffffff);
    int32_t sa = fuzz_s16(a);
    int32_t sb;
    int32_t raw = 0;
    int32_t product = 0;
    uint32_t product_u;
    bool select_a = false;
    if (function != 0x0bu && function != 0x1du && function != 0x37u &&
        !fuzz_select_lane(element, lane, &right_lane)) return false;
    b = read_lane(old_vectors[vt], right_lane);
    sb = fuzz_s16(b);
    product = sa * sb;
    product_u = (uint32_t)a * (uint32_t)b;
    switch (function) {
    case 0x00u: case 0x01u: {
      int64_t rounded = (int64_t)product * 2 + INT64_C(0x8000);
      acc = (uint64_t)rounded & UINT64_C(0x0000ffffffffffff);
      result = function == 0x00u ? fuzz_clamp_signed(acc, 16u)
                                 : fuzz_clamp_unsigned(acc, 16u);
      break;
    }
    case 0x02u: case 0x0au: {
      int64_t signed_acc = fuzz_s48(acc);
      bool add = function == 0x02u ? signed_acc >= 0 : signed_acc < 0;
      int64_t term = sb;
      if ((vs & 1u) != 0u) term *= INT64_C(65536);
      if (add) acc = (acc + (uint64_t)term) & UINT64_C(0x0000ffffffffffff);
      result = fuzz_clamp_signed(acc, 16u);
      break;
    }
    case 0x03u: {
      int32_t quantized = fuzz_s16((uint16_t)product);
      if (product < 0) quantized += 32;
      acc = (acc & 0xffffu) | ((uint64_t)(uint32_t)quantized << 16);
      result = (uint16_t)(fuzz_clamp_signed(acc, 17u) & 0xfff0u);
      break;
    }
    case 0x04u: {
      uint32_t high = product_u >> 16;
      uint32_t part = (high & 0x8000u) != 0u ? high | 0xffff0000u : high;
      acc = (acc & UINT64_C(0xffff00000000)) | part;
      result = fuzz_clamp_signed(acc, 0u);
      break;
    }
    case 0x05u: {
      int32_t part = sa * (int32_t)b;
      acc = (acc & UINT64_C(0xffff00000000)) | (uint32_t)part;
      result = fuzz_clamp_signed(acc, 16u);
      break;
    }
    case 0x06u: {
      int32_t part = (int32_t)a * sb;
      acc = (acc & UINT64_C(0xffff00000000)) | (uint32_t)part;
      result = fuzz_clamp_signed(acc, 0u);
      break;
    }
    case 0x07u: {
      uint32_t part = (uint32_t)product & 0xffff0000u;
      acc = (acc & UINT64_C(0xffff00000000)) | part;
      result = fuzz_clamp_signed(acc, 16u);
      break;
    }
    case 0x08u: case 0x09u: {
      uint32_t high = (uint32_t)(acc >> 16) + (uint32_t)product * 2u;
      acc = ((uint64_t)high << 16) | (acc & 0xffffu);
      result = function == 0x08u ? fuzz_clamp_signed(acc, 16u)
                                 : fuzz_clamp_unsigned(acc, 16u);
      break;
    }
    case 0x0bu: {
      int64_t signed_acc = fuzz_s48(acc);
      if ((acc & (UINT64_C(1) << 21)) == 0u && signed_acc < 0)
        acc = (acc + (UINT64_C(32) << 16)) & UINT64_C(0x0000ffffffffffff);
      else if ((acc & (UINT64_C(1) << 21)) == 0u && signed_acc > 0)
        acc = (acc - (UINT64_C(32) << 16)) & UINT64_C(0x0000ffffffffffff);
      result = (uint16_t)(fuzz_clamp_signed(acc, 17u) & 0xfff0u);
      break;
    }
    case 0x0cu: {
      uint32_t low = (uint32_t)acc + (product_u >> 16);
      acc = (acc & UINT64_C(0xffff00000000)) | low;
      result = fuzz_clamp_signed(acc, 0u);
      break;
    }
    case 0x0du: {
      uint32_t low = (uint32_t)acc + (uint32_t)(sa * (int32_t)b);
      acc = (acc & UINT64_C(0xffff00000000)) | low;
      result = fuzz_clamp_signed(acc, 16u);
      break;
    }
    case 0x0eu: {
      uint32_t low = (uint32_t)acc + (uint32_t)((int32_t)a * sb);
      acc = (acc & UINT64_C(0xffff00000000)) | low;
      result = fuzz_clamp_signed(acc, 0u);
      break;
    }
    case 0x0fu: {
      uint32_t low = (uint32_t)acc + ((uint32_t)product & 0xffff0000u);
      acc = (acc & UINT64_C(0xffff00000000)) | low;
      result = fuzz_clamp_signed(acc, 16u);
      break;
    }
    case 0x10u: case 0x11u: case 0x13u:
      if (function == 0x10u) {
        raw = sa + sb + (int32_t)((old_vco >> lane) & 1u);
        next_vco = 0u;
        result = raw > INT16_MAX ? 0x7fffu : raw < INT16_MIN ? 0x8000u : (uint16_t)raw;
      } else if (function == 0x11u) {
        raw = sa - sb - (int32_t)((old_vco >> lane) & 1u);
        next_vco = 0u;
        result = raw > INT16_MAX ? 0x7fffu : raw < INT16_MIN ? 0x8000u : (uint16_t)raw;
      } else {
        raw = sa < 0 ? -sb : sa > 0 ? sb : 0;
        result = raw > INT16_MAX ? 0x7fffu : raw < INT16_MIN ? 0x8000u : (uint16_t)raw;
      }
      acc = (acc & ~UINT64_C(0xffff)) | (uint16_t)raw;
      break;
    case 0x14u: {
      uint32_t sum = (uint32_t)a + b;
      result = (uint16_t)sum;
      acc = (acc & ~UINT64_C(0xffff)) | result;
      next_vco = (uint16_t)((next_vco & ~(UINT32_C(1) << lane)) |
                            (((sum >> 16) & 1u) << lane));
      break;
    }
    case 0x15u:
      result = (uint16_t)(a - b);
      acc = (acc & ~UINT64_C(0xffff)) | result;
      next_vco = (uint16_t)((next_vco & ~((UINT32_C(1) << lane) |
                                           (UINT32_C(1) << (lane + 8u)))) |
                            ((a < b ? 1u : 0u) << lane) |
                            ((a != b ? 1u : 0u) << (lane + 8u)));
      break;
    case 0x1du: {
      uint32_t shift = element == 0u ? 32u : element == 1u ? 16u : 0u;
      uint64_t mask = UINT64_C(0xffff) << shift;
      result = (uint16_t)(acc >> shift);
      acc = (acc & ~mask) | ((uint64_t)a << shift);
      break;
    }
    case 0x20u: case 0x21u: case 0x22u: case 0x23u: {
      bool equal = a == b;
      bool old_c = ((old_vco >> lane) & 1u) != 0u;
      bool old_e = ((old_vce >> lane) & 1u) != 0u;
      if (function == 0x20u) select_a = sa < sb || (equal && old_c && !old_e);
      if (function == 0x21u) select_a = equal && old_e;
      if (function == 0x22u) select_a = !equal || !old_e;
      if (function == 0x23u) select_a = sa > sb || (equal && (!old_c || old_e));
      result = select_a ? a : b;
      if (select_a) next_vcc |= (uint16_t)(1u << lane);
      else next_vcc &= (uint16_t)~(uint16_t)(1u << lane);
      if (function == 0x22u) next_vco &= (uint16_t)~(uint16_t)(1u << lane);
      else next_vco = 0u;
      next_vce = 0u;
      acc = (acc & ~UINT64_C(0xffff)) | result;
      break;
    }
    case 0x24u: {
      bool le = ((old_vcc >> lane) & 1u) != 0u;
      bool ge = ((old_vcc >> (lane + 8u)) & 1u) != 0u;
      bool clip_equal = ((old_vco >> (lane + 8u)) & 1u) == 0u;
      bool sign = ((old_vco >> lane) & 1u) != 0u;
      if (sign) {
        uint32_t sum = (uint32_t)a + b;
        bool carry = sum > 0xffffu;
        bool clip = ((old_vce >> lane) & 1u) != 0u;
        if (clip_equal) le = (!clip && (uint16_t)sum == 0u && !carry) ||
                             (clip && ((uint16_t)sum == 0u || !carry));
        result = le ? (uint16_t)(0u - b) : a;
      } else {
        if (clip_equal) ge = sa - sb >= 0;
        result = ge ? b : a;
      }
      next_vcc &= (uint16_t)~((UINT16_C(1) << lane) |
                              (UINT16_C(1) << (lane + 8u)));
      if (le) next_vcc |= (uint16_t)(UINT16_C(1) << lane);
      if (ge) next_vcc |= (uint16_t)(UINT16_C(1) << (lane + 8u));
      next_vco = 0u;
      next_vce = 0u;
      acc = (acc & ~UINT64_C(0xffff)) | result;
      break;
    }
    case 0x25u: {
      bool sign = ((a ^ b) & 0x8000u) != 0u;
      bool le, ge, equal, lane_vce = false;
      if (sign) {
        int32_t sum = sa + sb;
        ge = sb < 0; le = sum <= 0; lane_vce = sum == -1; equal = sum == 0;
        result = le ? (uint16_t)(0u - b) : a;
      } else {
        int32_t difference = sa - sb;
        le = sb < 0; ge = difference >= 0; equal = difference == 0;
        result = ge ? b : a;
      }
      if (le) next_vcc |= (uint16_t)(1u << lane);
      if (ge) next_vcc |= (uint16_t)(1u << (lane + 8u));
      if (!equal) next_vco |= (uint16_t)(1u << (lane + 8u));
      if (sign) next_vco |= (uint16_t)(1u << lane);
      if (lane_vce) next_vce |= (uint8_t)(1u << lane);
      acc = (acc & ~UINT64_C(0xffff)) | result;
      break;
    }
    case 0x26u: {
      bool sign = ((a ^ b) & 0x8000u) != 0u;
      bool le, ge;
      if (sign) {
        le = sa + sb + 1 <= 0; ge = sb < 0;
        result = le ? (uint16_t)~b : a;
      } else {
        le = sb < 0; ge = sa - sb >= 0; result = ge ? b : a;
      }
      if (le) next_vcc |= (uint16_t)(1u << lane);
      if (ge) next_vcc |= (uint16_t)(1u << (lane + 8u));
      next_vco = 0u; next_vce = 0u;
      acc = (acc & ~UINT64_C(0xffff)) | result;
      break;
    }
    case 0x27u:
      select_a = ((old_vcc >> lane) & 1u) != 0u;
      result = select_a ? a : b;
      acc = (acc & ~UINT64_C(0xffff)) | result;
      break;
    case 0x28u: result = (uint16_t)(a & b); acc = (acc & ~UINT64_C(0xffff)) | result; break;
    case 0x29u: result = (uint16_t)~(uint16_t)(a & b); acc = (acc & ~UINT64_C(0xffff)) | result; break;
    case 0x2au: result = (uint16_t)(a | b); acc = (acc & ~UINT64_C(0xffff)) | result; break;
    case 0x2bu: result = (uint16_t)~(uint16_t)(a | b); acc = (acc & ~UINT64_C(0xffff)) | result; break;
    case 0x2cu: result = (uint16_t)(a ^ b); acc = (acc & ~UINT64_C(0xffff)) | result; break;
    case 0x2du: result = (uint16_t)~(uint16_t)(a ^ b); acc = (acc & ~UINT64_C(0xffff)) | result; break;
    case 0x37u: continue;
    default: return false;
    }
    write_expected_lane(vectors[vd], lane, result);
    accumulator[lane] = acc & UINT64_C(0x0000ffffffffffff);
  }
  *vco = next_vco;
  *vcc = next_vcc;
  *vce = next_vce;
  return true;
}

typedef struct {
  const char *name;
  uint8_t function;
} vector_opcode;

static bool run_vmov_tuple(rsp_fuzz_fixture *fixture, uint32_t case_index,
                           uint32_t seed);
static bool run_divider_tuple(rsp_fuzz_fixture *fixture, uint32_t function,
                              uint32_t case_index, uint32_t seed);

static const vector_opcode kVectorOpcodes[] = {
    {"VMULF", 0x00u}, {"VMULU", 0x01u}, {"VRNDP", 0x02u},
    {"VMULQ", 0x03u}, {"VMUDL", 0x04u}, {"VMUDM", 0x05u},
    {"VMUDN", 0x06u}, {"VMUDH", 0x07u}, {"VMACF", 0x08u},
    {"VMACU", 0x09u}, {"VRNDN", 0x0au}, {"VMACQ", 0x0bu},
    {"VMADL", 0x0cu}, {"VMADM", 0x0du}, {"VMADN", 0x0eu},
    {"VMADH", 0x0fu}, {"VADD", 0x10u}, {"VSUB", 0x11u},
    {"VABS", 0x13u}, {"VADDC", 0x14u}, {"VSUBC", 0x15u},
    {"VSAR", 0x1du}, {"VLT", 0x20u}, {"VEQ", 0x21u},
    {"VNE", 0x22u}, {"VGE", 0x23u}, {"VCL", 0x24u},
    {"VCH", 0x25u}, {"VCR", 0x26u}, {"VMRG", 0x27u},
    {"VAND", 0x28u}, {"VNAND", 0x29u}, {"VOR", 0x2au},
    {"VNOR", 0x2bu}, {"VXOR", 0x2cu}, {"VNXOR", 0x2du},
    {"VRCP", 0x30u}, {"VRCPL", 0x31u}, {"VRCPH", 0x32u},
    {"VMOV", 0x33u}, {"VRSQ", 0x34u}, {"VRSQL", 0x35u},
    {"VRSQH", 0x36u}, {"VNOP", 0x37u}};

static bool vector_element_for_case(uint32_t function, uint32_t case_index,
                                    uint32_t *element_out) {
  static const uint8_t standard[] = {
      0u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u};
  if (function == 0x1du) {
    *element_out = case_index % 3u;
    return true;
  }
  if (function == 0x37u) {
    *element_out = case_index % 16u;
    return true;
  }
  *element_out = standard[case_index %
                          (uint32_t)(sizeof standard / sizeof standard[0])];
  return true;
}

static uint16_t rational_reciprocal_word(uint32_t index) {
  const uint64_t numerator = UINT64_C(1) << 34;
  uint64_t denominator = (uint64_t)index + UINT64_C(512);
  uint32_t low = 0u;
  uint32_t high = UINT32_C(131072);
  if (index == 0u) return UINT16_MAX;
  while (low < high) {
    uint32_t mid = low + (high - low) / 2u;
    uint64_t upper = (((uint64_t)(mid + 1u) << 8) - 1u) * denominator;
    if (numerator < upper) high = mid;
    else low = mid + 1u;
  }
  return (uint16_t)low;
}

static uint16_t rational_rsqrt_word(uint32_t index) {
  uint64_t a = ((uint64_t)index + UINT64_C(512)) >> (index & 1u);
  uint32_t low = 0u;
  uint32_t high = UINT32_C(1) << 20;
  const uint64_t limit = UINT64_C(1) << 44;
  while (low < high) {
    uint32_t mid = low + (high - low + 1u) / 2u;
    uint64_t square = (uint64_t)mid * mid;
    if (a * square < limit) low = mid;
    else high = mid - 1u;
  }
  return (uint16_t)(low >> 1);
}

static uint32_t rational_divider_result(bool rsqrt, uint32_t input) {
  uint32_t sign = (input & UINT32_C(0x80000000)) != 0u ? UINT32_MAX : 0u;
  uint32_t data = input ^ sign;
  uint32_t shift = 0u;
  uint32_t index;
  uint32_t right_shift;
  uint32_t result;
  uint16_t word;
  if (input == 0u) return UINT32_C(0x7fffffff);
  if (input == UINT32_C(0xffff8000)) return UINT32_C(0xffff0000);
  if (sign != 0u && input > UINT32_C(0xffff8000)) data++;
  while ((data & (UINT32_C(1) << (31u - shift))) == 0u) shift++;
  index = ((data << shift) & UINT32_C(0x7fc00000)) >> 22;
  if (rsqrt) {
    index = (index & UINT32_C(0x1fe)) | (shift & 1u);
    word = rational_rsqrt_word(index);
    right_shift = (31u - shift) >> 1;
  } else {
    word = rational_reciprocal_word(index);
    right_shift = 31u - shift;
  }
  result = (UINT32_C(0x10000) | word) << 14;
  result >>= right_shift;
  return result ^ sign;
}

static bool run_divider_tuple(rsp_fuzz_fixture *fixture, uint32_t function,
                              uint32_t case_index, uint32_t seed) {
  t64_machine *machine = fixture->machine;
  uint8_t expected_vectors[32][16];
  uint8_t dmem_before[T64_SP_BANK_BYTES];
  uint64_t expected_acc[8];
  uint32_t gpr_before[32];
  uint32_t random_state = seed == 0u ? 1u : seed;
  uint32_t element = case_index & 15u;
  uint32_t source_reg;
  uint32_t destination_reg;
  uint32_t destination_lane = (case_index >> 4u) & 7u;
  uint32_t source_lane = element & 7u;
  uint32_t instruction;
  uint32_t next_input;
  uint32_t next_result;
  bool next_high_valid;
  uint16_t scalar_input;
  uint16_t destination_value;
  uint16_t expected_vco;
  uint16_t expected_vcc;
  uint8_t expected_vce;
  uint32_t input;
  bool high = function == 0x32u || function == 0x36u;
  bool rsqrt = function >= 0x34u;

  seed_machine(fixture, &random_state);
  source_reg = 1u + ((case_index >> 2u) & 15u);
  destination_reg = (case_index & 3u) == 0u
                        ? source_reg
                        : 1u + ((case_index >> 7u) & 15u);
  scalar_input = read_lane(machine->rsp.vector[source_reg], source_lane);
  next_input = machine->rsp.divider_input;
  next_result = machine->rsp.divider_result;
  next_high_valid = machine->rsp.divider_high_valid;
  if (high) {
    destination_value = (uint16_t)(next_result >> 16);
    next_input = scalar_input;
    next_high_valid = true;
  } else {
    if ((function == 0x31u || function == 0x35u) && next_high_valid) {
      input = ((next_input & UINT32_C(0xffff)) << 16) | scalar_input;
    } else {
      input = (scalar_input & UINT16_C(0x8000)) != 0u
                  ? UINT32_C(0xffff0000) | scalar_input
                  : scalar_input;
    }
    next_result = rational_divider_result(rsqrt, input);
    next_high_valid = false;
    destination_value = (uint16_t)next_result;
  }
  (void)memcpy(expected_vectors, machine->rsp.vector,
               sizeof expected_vectors);
  (void)memcpy(expected_acc, machine->rsp.accumulator, sizeof expected_acc);
  (void)memcpy(gpr_before, machine->rsp.gpr, sizeof gpr_before);
  (void)memcpy(dmem_before, machine->sp_dmem, sizeof dmem_before);
  expected_vco = machine->rsp.vco;
  expected_vcc = machine->rsp.vcc;
  expected_vce = machine->rsp.vce;
  write_expected_lane(expected_vectors[destination_reg], destination_lane,
                      destination_value);
  for (uint32_t lane = 0u; lane < 8u; lane++) {
    expected_acc[lane] = (expected_acc[lane] & ~UINT64_C(0xffff)) |
                         scalar_input;
  }
  instruction = encode_vector(element, source_reg, destination_lane,
                              destination_reg, function);
  if (!execute_fetched(machine, instruction) || machine->rsp.pc != 4u ||
      memcmp(machine->rsp.vector, expected_vectors,
             sizeof expected_vectors) != 0 ||
      memcmp(machine->rsp.accumulator, expected_acc, sizeof expected_acc) !=
          0 ||
      machine->rsp.vco != expected_vco || machine->rsp.vcc != expected_vcc ||
      machine->rsp.vce != expected_vce ||
      machine->rsp.divider_input != next_input ||
      machine->rsp.divider_result != next_result ||
      machine->rsp.divider_high_valid != next_high_valid ||
      memcmp(machine->rsp.gpr, gpr_before, sizeof gpr_before) != 0 ||
      memcmp(machine->sp_dmem, dmem_before, sizeof dmem_before) != 0) {
    (void)fprintf(stderr,
                  "FAIL: fetched divider %02x case=%u seed=%08x e=%u lane=%u; complete-state/rational invariant failed\n",
                  function, case_index, seed, element, destination_lane);
    return false;
  }
  return true;
}

static bool run_divider_pair(rsp_fuzz_fixture *fixture, bool rsqrt,
                            uint32_t case_index, uint32_t seed) {
  t64_machine *machine = fixture->machine;
  uint8_t expected_vectors[32][16];
  uint8_t dmem_before[T64_SP_BANK_BYTES];
  uint64_t expected_acc[8];
  uint32_t gpr_before[32];
  uint32_t random_state = seed == 0u ? 1u : seed;
  uint32_t high_function = rsqrt ? 0x36u : 0x32u;
  uint32_t low_function = rsqrt ? 0x35u : 0x31u;
  uint32_t source_reg = 1u + (case_index % 31u);
  uint32_t destination_reg = (case_index & 3u) == 0u
                                 ? source_reg
                                 : 1u + ((case_index >> 5u) % 31u);
  uint32_t high_element = case_index & 15u;
  uint32_t low_element = (case_index >> 4u) & 15u;
  uint32_t destination_lane = (case_index >> 2u) & 7u;
  uint16_t high_source;
  uint16_t low_source;
  uint16_t high_destination;
  uint32_t expected_input;
  uint32_t expected_result;
  uint32_t previous_result;
  uint32_t high_instruction;
  uint32_t low_instruction;
  uint16_t expected_vco;
  uint16_t expected_vcc;
  uint8_t expected_vce;

  seed_machine(fixture, &random_state);
  high_source = read_lane(machine->rsp.vector[source_reg], high_element & 7u);
  previous_result = machine->rsp.divider_result;
  high_destination = (uint16_t)(previous_result >> 16u);
  (void)memcpy(expected_vectors, machine->rsp.vector,
               sizeof expected_vectors);
  (void)memcpy(expected_acc, machine->rsp.accumulator, sizeof expected_acc);
  (void)memcpy(gpr_before, machine->rsp.gpr, sizeof gpr_before);
  (void)memcpy(dmem_before, machine->sp_dmem, sizeof dmem_before);
  expected_vco = machine->rsp.vco;
  expected_vcc = machine->rsp.vcc;
  expected_vce = machine->rsp.vce;
  write_expected_lane(expected_vectors[destination_reg], destination_lane,
                      high_destination);
  for (uint32_t lane = 0u; lane < 8u; lane++) {
    expected_acc[lane] = (expected_acc[lane] & ~UINT64_C(0xffff)) |
                         high_source;
  }
  high_instruction = encode_vector(high_element, source_reg, destination_lane,
                                   destination_reg, high_function);
  if (!execute_fetched(machine, high_instruction) || machine->rsp.pc != 4u ||
      memcmp(machine->rsp.vector, expected_vectors,
             sizeof expected_vectors) != 0 ||
      memcmp(machine->rsp.accumulator, expected_acc, sizeof expected_acc) !=
          0 ||
      machine->rsp.vco != expected_vco || machine->rsp.vcc != expected_vcc ||
      machine->rsp.vce != expected_vce ||
      machine->rsp.divider_input != high_source ||
      machine->rsp.divider_result != previous_result ||
      !machine->rsp.divider_high_valid ||
      memcmp(machine->rsp.gpr, gpr_before, sizeof gpr_before) != 0 ||
      memcmp(machine->sp_dmem, dmem_before, sizeof dmem_before) != 0) {
    (void)fprintf(stderr,
                  "FAIL: fetched divider high/low pair high case=%u seed=%08x rsqrt=%u; high-state invariant failed\n",
                  case_index, seed, rsqrt ? 1u : 0u);
    return false;
  }

  low_source = read_lane(machine->rsp.vector[source_reg], low_element & 7u);
  expected_input = ((uint32_t)machine->rsp.divider_input << 16u) |
                   low_source;
  expected_result = rational_divider_result(rsqrt, expected_input);
  write_expected_lane(expected_vectors[destination_reg], destination_lane,
                      (uint16_t)expected_result);
  for (uint32_t lane = 0u; lane < 8u; lane++) {
    expected_acc[lane] = (expected_acc[lane] & ~UINT64_C(0xffff)) |
                         low_source;
  }
  low_instruction = encode_vector(low_element, source_reg, destination_lane,
                                  destination_reg, low_function);
  if (!execute_fetched_until_retired(machine, low_instruction, 16u) ||
      machine->rsp.pc != 8u ||
      machine->rsp.retired_instructions != UINT64_C(2) ||
      memcmp(machine->rsp.vector, expected_vectors,
             sizeof expected_vectors) != 0 ||
      memcmp(machine->rsp.accumulator, expected_acc, sizeof expected_acc) !=
          0 ||
      machine->rsp.vco != expected_vco || machine->rsp.vcc != expected_vcc ||
      machine->rsp.vce != expected_vce ||
      machine->rsp.divider_input != high_source ||
      machine->rsp.divider_result != expected_result ||
      machine->rsp.divider_high_valid ||
      memcmp(machine->rsp.gpr, gpr_before, sizeof gpr_before) != 0 ||
      memcmp(machine->sp_dmem, dmem_before, sizeof dmem_before) != 0) {
    (void)fprintf(stderr,
                  "FAIL: fetched divider 32-bit pair low case=%u seed=%08x rsqrt=%u input=%08x result=%08x actual=%08x lane=%04x pc=%03x retired=%llu latch=%u; complete-state/rational invariant failed\n",
                  case_index, seed, rsqrt ? 1u : 0u, expected_input,
                  expected_result, machine->rsp.divider_result,
                  read_lane(machine->rsp.vector[destination_reg],
                            destination_lane),
                  machine->rsp.pc,
                  (unsigned long long)machine->rsp.retired_instructions,
                  machine->rsp.divider_high_valid ? 1u : 0u);
    return false;
  }
  return true;
}

static bool run_vmov_tuple(rsp_fuzz_fixture *fixture, uint32_t case_index,
                           uint32_t seed) {
  t64_machine *machine = fixture->machine;
  uint8_t expected_vectors[32][16];
  uint8_t dmem_before[T64_SP_BANK_BYTES];
  uint64_t expected_acc[8];
  uint32_t gpr_before[32];
  uint32_t random_state = seed == 0u ? 1u : seed;
  uint32_t element;
  uint32_t source_reg;
  uint32_t destination_reg;
  uint32_t destination_lane = case_index & 7u;
  uint32_t source_lane;
  uint16_t scalar_input;
  uint16_t expected_vco;
  uint16_t expected_vcc;
  uint8_t expected_vce;
  uint32_t instruction;
  uint32_t divider_input_before;
  uint32_t divider_result_before;
  bool divider_high_valid_before;

  seed_machine(fixture, &random_state);
  (void)vector_element_for_case(0x33u, case_index, &element);
  source_reg = 1u + ((case_index >> 3u) & 15u);
  destination_reg = (case_index & 3u) == 0u
                        ? source_reg
                        : 1u + ((case_index >> 7u) & 15u);
  if (element == 0u) source_lane = destination_lane;
  else if (element <= 3u) source_lane = (element & 1u) + (destination_lane & 6u);
  else if (element <= 7u) source_lane = (element & 3u) + (destination_lane & 4u);
  else source_lane = element & 7u;
  scalar_input = read_lane(machine->rsp.vector[source_reg], source_lane);
  (void)memcpy(expected_vectors, machine->rsp.vector,
               sizeof expected_vectors);
  (void)memcpy(expected_acc, machine->rsp.accumulator, sizeof expected_acc);
  (void)memcpy(gpr_before, machine->rsp.gpr, sizeof gpr_before);
  (void)memcpy(dmem_before, machine->sp_dmem, sizeof dmem_before);
  divider_input_before = machine->rsp.divider_input;
  divider_result_before = machine->rsp.divider_result;
  divider_high_valid_before = machine->rsp.divider_high_valid;
  expected_vco = machine->rsp.vco;
  expected_vcc = machine->rsp.vcc;
  expected_vce = machine->rsp.vce;
  write_expected_lane(expected_vectors[destination_reg], destination_lane,
                      scalar_input);
  expected_acc[destination_lane] =
      (expected_acc[destination_lane] & ~UINT64_C(0xffff)) | scalar_input;
  instruction = encode_vector(element, source_reg, destination_lane,
                              destination_reg, UINT32_C(0x33));
  if (!execute_fetched(machine, instruction) || machine->rsp.pc != 4u ||
      memcmp(machine->rsp.vector, expected_vectors,
             sizeof expected_vectors) != 0 ||
      memcmp(machine->rsp.accumulator, expected_acc, sizeof expected_acc) !=
          0 ||
      machine->rsp.vco != expected_vco || machine->rsp.vcc != expected_vcc ||
      machine->rsp.vce != expected_vce ||
      memcmp(machine->rsp.gpr, gpr_before, sizeof gpr_before) != 0 ||
      memcmp(machine->sp_dmem, dmem_before, sizeof dmem_before) != 0 ||
      machine->rsp.divider_input != divider_input_before ||
      machine->rsp.divider_result != divider_result_before ||
      machine->rsp.divider_high_valid != divider_high_valid_before) {
    (void)fprintf(stderr,
                  "FAIL: fetched VMOV case=%u seed=%08x e=%u lane=%u; complete-state invariant failed\n",
                  case_index, seed, element, destination_lane);
    return false;
  }
  return true;
}

static bool run_vector_tuple(rsp_fuzz_fixture *fixture, uint32_t function,
                             uint32_t case_index, uint32_t seed) {
  t64_machine *machine = fixture->machine;
  uint8_t expected_vectors[32][16];
  uint8_t dmem_before[T64_SP_BANK_BYTES];
  uint64_t expected_acc[8];
  uint32_t gpr_before[32];
  uint32_t random_state = seed == 0u ? 1u : seed;
  uint32_t element;
  uint32_t vs;
  uint32_t vt;
  uint32_t vd;
  uint16_t expected_vco;
  uint16_t expected_vcc;
  uint8_t expected_vce;
  uint32_t divider_input;
  uint32_t divider_result;
  bool divider_high_valid;
  uint32_t instruction;

  if (function == 0x33u) {
    return run_vmov_tuple(fixture, case_index, seed);
  }
  if ((function >= 0x30u && function <= 0x32u) ||
      (function >= 0x34u && function <= 0x36u)) {
    return run_divider_tuple(fixture, function, case_index, seed);
  }

  seed_machine(fixture, &random_state);
  switch (case_index & 3u) {
  case 0u: vs = 1u; vt = 2u; vd = 3u; break;
  case 1u: vs = 3u; vt = 2u; vd = 3u; break;
  case 2u: vs = 1u; vt = 3u; vd = 3u; break;
  default: vs = 3u; vt = 3u; vd = 3u; break;
  }
  vs += (case_index >> 2u) & 7u;
  vt += (case_index >> 5u) & 7u;
  vd += (case_index >> 8u) & 7u;
  if (vs == 0u) vs = 1u;
  if (vt == 0u) vt = 1u;
  if (vd == 0u) vd = 1u;
  (void)vector_element_for_case(function, case_index, &element);
  (void)memcpy(expected_vectors, machine->rsp.vector,
               sizeof expected_vectors);
  (void)memcpy(expected_acc, machine->rsp.accumulator, sizeof expected_acc);
  (void)memcpy(gpr_before, machine->rsp.gpr, sizeof gpr_before);
  (void)memcpy(dmem_before, machine->sp_dmem, sizeof dmem_before);
  expected_vco = machine->rsp.vco;
  expected_vcc = machine->rsp.vcc;
  expected_vce = machine->rsp.vce;
  divider_input = machine->rsp.divider_input;
  divider_result = machine->rsp.divider_result;
  divider_high_valid = machine->rsp.divider_high_valid;
  if (!reference_vu(function, element, vs, vt, vd, expected_vectors,
                    expected_acc, &expected_vco, &expected_vcc,
                    &expected_vce)) {
    return false;
  }
  instruction = encode_vector(element, vt, vs, vd, function);
  if (!execute_fetched(machine, instruction) || machine->rsp.pc != 4u ||
      memcmp(machine->rsp.vector, expected_vectors,
             sizeof expected_vectors) != 0 ||
      memcmp(machine->rsp.accumulator, expected_acc, sizeof expected_acc) !=
          0 ||
      machine->rsp.vco != expected_vco || machine->rsp.vcc != expected_vcc ||
      machine->rsp.vce != expected_vce ||
      memcmp(machine->rsp.gpr, gpr_before, sizeof gpr_before) != 0 ||
      memcmp(machine->sp_dmem, dmem_before, sizeof dmem_before) != 0 ||
      machine->rsp.divider_input != divider_input ||
      machine->rsp.divider_result != divider_result ||
      machine->rsp.divider_high_valid != divider_high_valid) {
    (void)fprintf(stderr,
                  "FAIL: fetched %02x case=%u seed=%08x e=%u V%u,V%u->V%u; complete-state invariant failed\n",
                  function, case_index, seed, element, vs, vt, vd);
    return false;
  }
  if (machine->rsp.gpr[0] != 0u) return false;
  return true;
}

static bool vector_function_fuzz(rsp_fuzz_fixture *fixture) {
  size_t opcode_index;
  for (opcode_index = 0u;
       opcode_index < sizeof kVectorOpcodes / sizeof kVectorOpcodes[0];
       opcode_index++) {
    uint32_t case_index;
    uint32_t retired = 0u;
    for (case_index = 0u; case_index < FUZZ_CASES_PER_OPCODE; case_index++) {
      uint32_t seed = UINT32_C(0x05f00001) ^
                      ((uint32_t)opcode_index * UINT32_C(0x9e3779b9)) ^
                      (case_index * UINT32_C(0x85ebca6b));
      if (!run_vector_tuple(fixture, kVectorOpcodes[opcode_index].function,
                            case_index, seed)) {
        (void)fprintf(stderr, "FAIL: %s has %u completed tuples\n",
                      kVectorOpcodes[opcode_index].name, retired);
        return false;
      }
      retired++;
    }
    if (retired < FUZZ_CASES_PER_OPCODE) return false;
    report_opcode((uint32_t)(opcode_index + 1u),
                  kVectorOpcodes[opcode_index].name, retired,
                  UINT32_C(0x05f00001));
  }
  return true;
}

typedef struct {
  const char *name;
  uint8_t encoding;
  uint8_t operation;
} scalar_opcode;

enum {
  SCALAR_SPECIAL = 0,
  SCALAR_PRIMARY = 1
};

static const scalar_opcode kScalarAluOpcodes[] = {
    {"SLL", SCALAR_SPECIAL, 0x00u}, {"SRL", SCALAR_SPECIAL, 0x02u},
    {"SRA", SCALAR_SPECIAL, 0x03u}, {"SLLV", SCALAR_SPECIAL, 0x04u},
    {"SRLV", SCALAR_SPECIAL, 0x06u}, {"SRAV", SCALAR_SPECIAL, 0x07u},
    {"ADD", SCALAR_SPECIAL, 0x20u}, {"ADDU", SCALAR_SPECIAL, 0x21u},
    {"SUB", SCALAR_SPECIAL, 0x22u}, {"SUBU", SCALAR_SPECIAL, 0x23u},
    {"AND", SCALAR_SPECIAL, 0x24u}, {"OR", SCALAR_SPECIAL, 0x25u},
    {"XOR", SCALAR_SPECIAL, 0x26u}, {"NOR", SCALAR_SPECIAL, 0x27u},
    {"SLT", SCALAR_SPECIAL, 0x2au}, {"SLTU", SCALAR_SPECIAL, 0x2bu},
    {"ADDI", SCALAR_PRIMARY, 0x08u}, {"ADDIU", SCALAR_PRIMARY, 0x09u},
    {"SLTI", SCALAR_PRIMARY, 0x0au}, {"SLTIU", SCALAR_PRIMARY, 0x0bu},
    {"ANDI", SCALAR_PRIMARY, 0x0cu}, {"ORI", SCALAR_PRIMARY, 0x0du},
    {"XORI", SCALAR_PRIMARY, 0x0eu}, {"LUI", SCALAR_PRIMARY, 0x0fu}};

static uint32_t sign_extend_16(uint16_t value) {
  return (value & UINT16_C(0x8000)) != 0u
             ? UINT32_C(0xffff0000) | value
             : (uint32_t)value;
}

static uint32_t arithmetic_shift_right(uint32_t value, uint32_t shift) {
  if (shift == 0u) return value;
  if ((value & UINT32_C(0x80000000)) == 0u) return value >> shift;
  return (value >> shift) | (UINT32_MAX << (32u - shift));
}

static uint32_t scalar_expected(uint32_t operation, bool primary,
                                uint32_t left, uint32_t right,
                                uint32_t shift, uint16_t immediate) {
  uint32_t signed_immediate = sign_extend_16(immediate);
  if (primary) {
    switch (operation) {
    case 0x08u: case 0x09u: return left + signed_immediate;
    case 0x0au:
      return (left ^ UINT32_C(0x80000000)) <
                     (signed_immediate ^ UINT32_C(0x80000000))
                 ? 1u
                 : 0u;
    case 0x0bu: return left < signed_immediate ? 1u : 0u;
    case 0x0cu: return left & immediate;
    case 0x0du: return left | immediate;
    case 0x0eu: return left ^ immediate;
    case 0x0fu: return (uint32_t)immediate << 16;
    default: return 0u;
    }
  }
  switch (operation) {
  case 0x00u: return right << shift;
  case 0x02u: return right >> shift;
  case 0x03u: return arithmetic_shift_right(right, shift);
  case 0x04u: return right << (left & 31u);
  case 0x06u: return right >> (left & 31u);
  case 0x07u: return arithmetic_shift_right(right, left & 31u);
  case 0x20u: case 0x21u: return left + right;
  case 0x22u: case 0x23u: return left - right;
  case 0x24u: return left & right;
  case 0x25u: return left | right;
  case 0x26u: return left ^ right;
  case 0x27u: return ~(left | right);
  case 0x2au:
    return (left ^ UINT32_C(0x80000000)) <
                   (right ^ UINT32_C(0x80000000))
               ? 1u
               : 0u;
  case 0x2bu: return left < right ? 1u : 0u;
  default: return 0u;
  }
}

static uint32_t scalar_edge_value(uint32_t selector, uint32_t random) {
  static const uint32_t edges[] = {
      UINT32_C(0),          UINT32_C(1),          UINT32_C(0x7fffffff),
      UINT32_C(0x80000000), UINT32_MAX,           UINT32_C(0xffff8000),
      UINT32_C(0x0000ffff), UINT32_C(0x00008000)};
  return selector < (uint32_t)(sizeof edges / sizeof edges[0])
             ? edges[selector]
             : random;
}

static bool run_scalar_alu_tuple(rsp_fuzz_fixture *fixture,
                                 const scalar_opcode *opcode,
                                 uint32_t case_index, uint32_t seed) {
  t64_machine *machine = fixture->machine;
  uint8_t vectors_before[32][16];
  uint8_t dmem_before[T64_SP_BANK_BYTES];
  uint64_t accumulators_before[8];
  uint32_t gpr_before[32];
  uint32_t expected_gpr[32];
  uint32_t random_state = seed == 0u ? 1u : seed;
  uint32_t primary = opcode->encoding == SCALAR_PRIMARY ? 1u : 0u;
  uint32_t left_reg = 1u + ((case_index >> 1u) % 31u);
  uint32_t right_reg = 1u + ((case_index >> 6u) % 31u);
  uint32_t destination = case_index % 32u;
  uint32_t shift = (case_index * 13u) & 31u;
  uint16_t immediate = (uint16_t)(next_random(&random_state) >> 8u);
  uint32_t left;
  uint32_t right;
  uint32_t instruction;
  uint32_t expected;
  uint32_t reg;
  uint32_t divider_input_before;
  uint32_t divider_result_before;
  bool divider_high_valid_before;
  uint16_t vco_before;
  uint16_t vcc_before;
  uint8_t vce_before;

  seed_machine(fixture, &random_state);
  machine->rsp.gpr[left_reg] = scalar_edge_value(case_index % 12u,
                                                next_random(&random_state));
  machine->rsp.gpr[right_reg] = scalar_edge_value((case_index / 3u) % 12u,
                                                 next_random(&random_state));
  if (opcode->operation == 0x0fu) left_reg = 0u;
  if (!primary && opcode->operation <= 0x03u) left_reg = 0u;
  left = machine->rsp.gpr[left_reg];
  right = machine->rsp.gpr[right_reg];
  if (primary) {
    instruction = ((uint32_t)opcode->operation << 26u) | (left_reg << 21u) |
                  (destination << 16u) | immediate;
  } else if (opcode->operation <= 0x03u) {
    instruction = (right_reg << 16u) | (destination << 11u) |
                  (shift << 6u) | opcode->operation;
  } else {
    instruction = (left_reg << 21u) | (right_reg << 16u) |
                  (destination << 11u) |
                  opcode->operation;
  }
  expected = scalar_expected(opcode->operation, primary != 0u, left, right,
                             shift, immediate);
  (void)memcpy(vectors_before, machine->rsp.vector, sizeof vectors_before);
  (void)memcpy(accumulators_before, machine->rsp.accumulator,
               sizeof accumulators_before);
  (void)memcpy(gpr_before, machine->rsp.gpr, sizeof gpr_before);
  (void)memcpy(dmem_before, machine->sp_dmem, sizeof dmem_before);
  divider_input_before = machine->rsp.divider_input;
  divider_result_before = machine->rsp.divider_result;
  divider_high_valid_before = machine->rsp.divider_high_valid;
  vco_before = machine->rsp.vco;
  vcc_before = machine->rsp.vcc;
  vce_before = machine->rsp.vce;
  (void)memcpy(expected_gpr, gpr_before, sizeof expected_gpr);
  if (destination != 0u) expected_gpr[destination] = expected;
  if (!execute_fetched(machine, instruction) || machine->rsp.pc != 4u ||
      memcmp(machine->rsp.gpr, expected_gpr, sizeof expected_gpr) != 0 ||
      memcmp(machine->rsp.vector, vectors_before, sizeof vectors_before) != 0 ||
      memcmp(machine->rsp.accumulator, accumulators_before,
             sizeof accumulators_before) != 0 ||
      memcmp(machine->sp_dmem, dmem_before, sizeof dmem_before) != 0 ||
      machine->rsp.vco != vco_before || machine->rsp.vcc != vcc_before ||
      machine->rsp.vce != vce_before ||
      machine->rsp.divider_input != divider_input_before ||
      machine->rsp.divider_result != divider_result_before ||
      machine->rsp.divider_high_valid != divider_high_valid_before ||
      machine->rsp.gpr[0] != 0u) {
    (void)fprintf(stderr,
                  "FAIL: fetched scalar %s case=%u seed=%08x left=%08x right=%08x immediate=%04x; result/preservation invariant failed\n",
                  opcode->name, case_index, seed, left, right, immediate);
    return false;
  }
  for (reg = 0u; reg < 32u; reg++) {
    if (memcmp(machine->rsp.vector[reg], vectors_before[reg], 16u) != 0) {
      return false;
    }
  }
  return true;
}

static bool scalar_alu_fuzz(rsp_fuzz_fixture *fixture) {
  size_t opcode_index;
  for (opcode_index = 0u;
       opcode_index < sizeof kScalarAluOpcodes / sizeof kScalarAluOpcodes[0];
       opcode_index++) {
    uint32_t case_index;
    uint32_t accepted = 0u;
    for (case_index = 0u; case_index < FUZZ_CASES_PER_OPCODE; case_index++) {
      uint32_t seed = UINT32_C(0x05f10001) ^
                      ((uint32_t)opcode_index * UINT32_C(0x9e3779b9)) ^
                      (case_index * UINT32_C(0x85ebca6b));
      if (!run_scalar_alu_tuple(fixture, &kScalarAluOpcodes[opcode_index],
                                case_index, seed)) {
        (void)fprintf(stderr, "FAIL: scalar %s accepted=%u refused=0\n",
                      kScalarAluOpcodes[opcode_index].name, accepted);
        return false;
      }
      accepted++;
    }
    report_opcode((uint32_t)(opcode_index + 1u),
                  kScalarAluOpcodes[opcode_index].name, accepted,
                  UINT32_C(0x05f10001));
  }
  return true;
}

typedef struct {
  const char *name;
  uint8_t operation;
  uint8_t width;
  bool load;
  bool sign_extend;
} scalar_memory_opcode;

static const scalar_memory_opcode kScalarMemoryOpcodes[] = {
    {"LB", 0x20u, 1u, true, true}, {"LH", 0x21u, 2u, true, true},
    {"LW", 0x23u, 4u, true, false}, {"LBU", 0x24u, 1u, true, false},
    {"LHU", 0x25u, 2u, true, false}, {"SB", 0x28u, 1u, false, false},
    {"SH", 0x29u, 2u, false, false}, {"SW", 0x2bu, 4u, false, false}};

static bool run_scalar_memory_tuple(rsp_fuzz_fixture *fixture,
                                    const scalar_memory_opcode *opcode,
                                    uint32_t case_index, uint32_t seed) {
  t64_machine *machine = fixture->machine;
  uint8_t vectors_before[32][16];
  uint8_t dmem_before[T64_SP_BANK_BYTES];
  uint8_t expected_dmem[T64_SP_BANK_BYTES];
  uint64_t accumulators_before[8];
  uint32_t gpr_before[32];
  uint32_t expected_gpr[32];
  uint32_t random_state = seed == 0u ? 1u : seed;
  uint32_t base_reg = 1u + (case_index % 31u);
  uint32_t transfer_reg = (case_index & 3u) == 0u
                              ? base_reg
                              : 1u + ((case_index >> 2u) % 31u);
  uint32_t base_address = UINT32_C(0x00000ff8) + (case_index & 7u);
  uint16_t immediate = (uint16_t)(case_index * 7u);
  uint32_t effective_address;
  uint32_t address;
  uint32_t value = 0u;
  uint32_t instruction;
  uint32_t divider_input_before;
  uint32_t divider_result_before;
  bool divider_high_valid_before;
  uint16_t vco_before;
  uint16_t vcc_before;
  uint8_t vce_before;

  seed_machine(fixture, &random_state);
  machine->rsp.gpr[base_reg] = base_address;
  effective_address = (base_address + sign_extend_16(immediate)) &
                      UINT32_C(0x00000fff);
  (void)memcpy(vectors_before, machine->rsp.vector, sizeof vectors_before);
  (void)memcpy(accumulators_before, machine->rsp.accumulator,
               sizeof accumulators_before);
  (void)memcpy(gpr_before, machine->rsp.gpr, sizeof gpr_before);
  (void)memcpy(expected_gpr, gpr_before, sizeof expected_gpr);
  (void)memcpy(dmem_before, machine->sp_dmem, sizeof dmem_before);
  (void)memcpy(expected_dmem, dmem_before, sizeof expected_dmem);
  divider_input_before = machine->rsp.divider_input;
  divider_result_before = machine->rsp.divider_result;
  divider_high_valid_before = machine->rsp.divider_high_valid;
  vco_before = machine->rsp.vco;
  vcc_before = machine->rsp.vcc;
  vce_before = machine->rsp.vce;
  if (opcode->load) {
    for (uint32_t byte = 0u; byte < opcode->width; byte++) {
      address = (effective_address + byte) & UINT32_C(0x00000fff);
      value = (value << 8u) | dmem_before[address];
    }
    if (opcode->sign_extend && opcode->width == 1u &&
        (value & UINT32_C(0x80)) != 0u) {
      value |= UINT32_C(0xffffff00);
    } else if (opcode->sign_extend && opcode->width == 2u &&
               (value & UINT32_C(0x8000)) != 0u) {
      value |= UINT32_C(0xffff0000);
    }
    if (transfer_reg != 0u) expected_gpr[transfer_reg] = value;
  } else {
    value = machine->rsp.gpr[transfer_reg];
    for (uint32_t byte = 0u; byte < opcode->width; byte++) {
      uint32_t shift = (opcode->width - byte - 1u) * 8u;
      address = (effective_address + byte) & UINT32_C(0x00000fff);
      expected_dmem[address] = (uint8_t)(value >> shift);
    }
  }
  instruction = ((uint32_t)opcode->operation << 26u) |
                (base_reg << 21u) | (transfer_reg << 16u) | immediate;
  if (!execute_fetched(machine, instruction) || machine->rsp.pc != 4u ||
      memcmp(machine->rsp.gpr, expected_gpr, sizeof expected_gpr) != 0 ||
      memcmp(machine->rsp.vector, vectors_before, sizeof vectors_before) != 0 ||
      memcmp(machine->rsp.accumulator, accumulators_before,
             sizeof accumulators_before) != 0 ||
      memcmp(machine->sp_dmem, expected_dmem, sizeof expected_dmem) != 0 ||
      machine->rsp.vco != vco_before || machine->rsp.vcc != vcc_before ||
      machine->rsp.vce != vce_before ||
      machine->rsp.divider_input != divider_input_before ||
      machine->rsp.divider_result != divider_result_before ||
      machine->rsp.divider_high_valid != divider_high_valid_before ||
      machine->rsp.gpr[0] != 0u) {
    (void)fprintf(stderr,
                  "FAIL: fetched scalar %s case=%u seed=%08x address=%03x width=%u; result/preservation invariant failed\n",
                  opcode->name, case_index, seed, effective_address,
                  opcode->width);
    return false;
  }
  return true;
}

static bool scalar_memory_fuzz(rsp_fuzz_fixture *fixture) {
  size_t opcode_index;
  for (opcode_index = 0u;
       opcode_index < sizeof kScalarMemoryOpcodes /
                          sizeof kScalarMemoryOpcodes[0];
       opcode_index++) {
    uint32_t case_index;
    uint32_t accepted = 0u;
    for (case_index = 0u; case_index < FUZZ_CASES_PER_OPCODE; case_index++) {
      uint32_t seed = UINT32_C(0x05f20001) ^
                      ((uint32_t)opcode_index * UINT32_C(0x9e3779b9)) ^
                      (case_index * UINT32_C(0x85ebca6b));
      if (!run_scalar_memory_tuple(fixture,
                                   &kScalarMemoryOpcodes[opcode_index],
                                   case_index, seed)) {
        (void)fprintf(stderr, "FAIL: scalar memory %s accepted=%u refused=0\n",
                      kScalarMemoryOpcodes[opcode_index].name, accepted);
        return false;
      }
      accepted++;
    }
    report_opcode((uint32_t)(opcode_index + 1u),
                  kScalarMemoryOpcodes[opcode_index].name, accepted,
                  UINT32_C(0x05f20001));
  }
  return true;
}

typedef struct {
  const char *name;
  uint8_t coprocessor_rs;
} cop2_transfer_opcode;

static const cop2_transfer_opcode kCop2TransferOpcodes[] = {
    {"MFC2", 0u}, {"MTC2", 4u}, {"CFC2", 2u}, {"CTC2", 6u}};

static bool run_cop2_transfer_tuple(rsp_fuzz_fixture *fixture,
                                    const cop2_transfer_opcode *opcode,
                                    uint32_t case_index, uint32_t seed) {
  t64_machine *machine = fixture->machine;
  uint8_t expected_vectors[32][16];
  uint8_t dmem_before[T64_SP_BANK_BYTES];
  uint64_t expected_acc[8];
  uint32_t gpr_before[32];
  uint32_t expected_gpr[32];
  uint32_t random_state = seed == 0u ? 1u : seed;
  uint32_t vector_reg = case_index & 31u;
  uint32_t scalar_reg = (case_index >> 3u) & 31u;
  uint32_t control_reg = case_index % 3u;
  uint32_t element = opcode->coprocessor_rs == 0u ||
                             opcode->coprocessor_rs == 4u
                         ? (case_index % 8u) * 2u
                         : case_index % 16u;
  uint16_t expected_vco;
  uint16_t expected_vcc;
  uint8_t expected_vce;
  uint32_t divider_input_before;
  uint32_t divider_result_before;
  bool divider_high_valid_before;
  uint32_t rd = (opcode->coprocessor_rs == 2u ||
                 opcode->coprocessor_rs == 6u)
                    ? control_reg
                    : vector_reg;
  uint32_t rs = opcode->coprocessor_rs;
  uint32_t instruction;
  uint32_t value;

  seed_machine(fixture, &random_state);
  (void)memcpy(expected_vectors, machine->rsp.vector,
               sizeof expected_vectors);
  (void)memcpy(expected_acc, machine->rsp.accumulator, sizeof expected_acc);
  (void)memcpy(gpr_before, machine->rsp.gpr, sizeof gpr_before);
  (void)memcpy(expected_gpr, gpr_before, sizeof expected_gpr);
  (void)memcpy(dmem_before, machine->sp_dmem, sizeof dmem_before);
  expected_vco = machine->rsp.vco;
  expected_vcc = machine->rsp.vcc;
  expected_vce = machine->rsp.vce;
  divider_input_before = machine->rsp.divider_input;
  divider_result_before = machine->rsp.divider_result;
  divider_high_valid_before = machine->rsp.divider_high_valid;
  if (rs == 0u) {
    uint16_t half = read_lane(machine->rsp.vector[rd], element / 2u);
    value = (half & UINT16_C(0x8000)) != 0u
                ? UINT32_C(0xffff0000) | half
                : (uint32_t)half;
    if (scalar_reg != 0u) expected_gpr[scalar_reg] = value;
  } else if (rs == 4u) {
    value = (uint16_t)machine->rsp.gpr[scalar_reg];
    write_expected_lane(expected_vectors[rd], element / 2u,
                        (uint16_t)value);
  } else if (rs == 2u) {
    uint16_t control_value = rd == 0u ? expected_vco :
                             rd == 1u ? expected_vcc : expected_vce;
    value = (control_value & UINT16_C(0x8000)) != 0u
                ? UINT32_C(0xffff0000) | control_value
                : control_value;
    if (scalar_reg != 0u) expected_gpr[scalar_reg] = value;
  } else {
    value = machine->rsp.gpr[scalar_reg];
    if (rd == 0u) expected_vco = (uint16_t)value;
    else if (rd == 1u) expected_vcc = (uint16_t)value;
    else expected_vce = (uint8_t)value;
  }
  instruction = (UINT32_C(0x12) << 26u) | (rs << 21u) |
                (scalar_reg << 16u) | (rd << 11u) | (element << 7u);
  if (!execute_fetched(machine, instruction) || machine->rsp.pc != 4u ||
      memcmp(machine->rsp.gpr, expected_gpr, sizeof expected_gpr) != 0 ||
      memcmp(machine->rsp.vector, expected_vectors,
             sizeof expected_vectors) != 0 ||
      memcmp(machine->rsp.accumulator, expected_acc, sizeof expected_acc) !=
          0 ||
      memcmp(machine->sp_dmem, dmem_before, sizeof dmem_before) != 0 ||
      machine->rsp.vco != expected_vco || machine->rsp.vcc != expected_vcc ||
      machine->rsp.vce != expected_vce ||
      machine->rsp.divider_input != divider_input_before ||
      machine->rsp.divider_result != divider_result_before ||
      machine->rsp.divider_high_valid != divider_high_valid_before ||
      machine->rsp.gpr[0] != 0u) {
    (void)fprintf(stderr,
                  "FAIL: fetched COP2 %s case=%u seed=%08x rs=%u rd=%u e=%u scalar=%u; complete-state invariant failed\n",
                  opcode->name, case_index, seed, rs, rd, element,
                  scalar_reg);
    return false;
  }
  return true;
}

static bool cop2_transfer_fuzz(rsp_fuzz_fixture *fixture) {
  size_t opcode_index;
  for (opcode_index = 0u;
       opcode_index < sizeof kCop2TransferOpcodes /
                          sizeof kCop2TransferOpcodes[0];
       opcode_index++) {
    uint32_t case_index;
    uint32_t accepted = 0u;
    for (case_index = 0u; case_index < FUZZ_CASES_PER_OPCODE; case_index++) {
      uint32_t seed = UINT32_C(0x05f30001) ^
                      ((uint32_t)opcode_index * UINT32_C(0x9e3779b9)) ^
                      (case_index * UINT32_C(0x85ebca6b));
      if (!run_cop2_transfer_tuple(fixture,
                                   &kCop2TransferOpcodes[opcode_index],
                                   case_index, seed)) {
        (void)fprintf(stderr, "FAIL: COP2 %s accepted=%u refused=0\n",
                      kCop2TransferOpcodes[opcode_index].name, accepted);
        return false;
      }
      accepted++;
    }
    report_opcode((uint32_t)(opcode_index + 1u),
                  kCop2TransferOpcodes[opcode_index].name, accepted,
                  UINT32_C(0x05f30001));
  }
  return true;
}

static uint32_t expected_cp0_read(const t64_machine *machine, uint32_t reg) {
  switch (reg) {
  case 0u: return machine->sp.mem_address;
  case 1u: return machine->sp.dram_address;
  case 2u: return machine->sp.read_length;
  case 3u: return machine->sp.write_length;
  case 4u: return machine->rsp.sp_status & T64_RSP_STATUS_READ_MASK;
  case 5u: case 6u: return 0u;
  case 7u: return machine->rsp.sp_semaphore ? 1u : 0u;
  default: return 0u;
  }
}

static bool run_cp0_read_tuple(rsp_fuzz_fixture *fixture, uint32_t cp0_reg,
                               uint32_t case_index, uint32_t seed) {
  t64_machine *machine = fixture->machine;
  uint8_t vectors_before[32][16];
  uint8_t dmem_before[T64_SP_BANK_BYTES];
  uint64_t accumulators_before[8];
  uint32_t gpr_before[32];
  uint32_t expected_gpr[32];
  uint32_t random_state = seed == 0u ? 1u : seed;
  uint32_t scalar_reg = case_index % 32u;
  uint32_t expected_value;
  uint32_t instruction;
  bool expected_semaphore;
  uint16_t vco_before;
  uint16_t vcc_before;
  uint8_t vce_before;
  uint32_t divider_input_before;
  uint32_t divider_result_before;
  bool divider_high_valid_before;

  seed_machine(fixture, &random_state);
  machine->sp.mem_address = (case_index * 11u) & UINT32_C(0x1fff);
  machine->sp.dram_address = (case_index * 257u) & UINT32_C(0x00ffffff);
  machine->sp.read_length = case_index * 3u;
  machine->sp.write_length = case_index * 5u;
  machine->rsp.sp_status = T64_RSP_STATUS_INTR_BREAK |
                           ((case_index & 1u) != 0u
                                ? T64_RSP_STATUS_SINGLE_STEP
                                : 0u) |
                           ((case_index & 2u) != 0u
                                ? UINT32_C(1) << 8u
                                : 0u);
  machine->rsp.sp_semaphore = (case_index & 1u) != 0u;
  expected_semaphore = cp0_reg == 7u ? true : machine->rsp.sp_semaphore;
  expected_value = expected_cp0_read(machine, cp0_reg);
  (void)memcpy(vectors_before, machine->rsp.vector, sizeof vectors_before);
  (void)memcpy(accumulators_before, machine->rsp.accumulator,
               sizeof accumulators_before);
  (void)memcpy(gpr_before, machine->rsp.gpr, sizeof gpr_before);
  (void)memcpy(expected_gpr, gpr_before, sizeof expected_gpr);
  if (scalar_reg != 0u) expected_gpr[scalar_reg] = expected_value;
  (void)memcpy(dmem_before, machine->sp_dmem, sizeof dmem_before);
  vco_before = machine->rsp.vco;
  vcc_before = machine->rsp.vcc;
  vce_before = machine->rsp.vce;
  divider_input_before = machine->rsp.divider_input;
  divider_result_before = machine->rsp.divider_result;
  divider_high_valid_before = machine->rsp.divider_high_valid;
  instruction = (UINT32_C(0x10) << 26u) | (scalar_reg << 16u) |
                (cp0_reg << 11u);
  if (!execute_fetched(machine, instruction) || machine->rsp.pc != 4u ||
      memcmp(machine->rsp.gpr, expected_gpr, sizeof expected_gpr) != 0 ||
      memcmp(machine->rsp.vector, vectors_before, sizeof vectors_before) != 0 ||
      memcmp(machine->rsp.accumulator, accumulators_before,
             sizeof accumulators_before) != 0 ||
      memcmp(machine->sp_dmem, dmem_before, sizeof dmem_before) != 0 ||
      machine->rsp.vco != vco_before || machine->rsp.vcc != vcc_before ||
      machine->rsp.vce != vce_before ||
      machine->rsp.divider_input != divider_input_before ||
      machine->rsp.divider_result != divider_result_before ||
      machine->rsp.divider_high_valid != divider_high_valid_before ||
      machine->rsp.sp_semaphore != expected_semaphore ||
      machine->rsp.gpr[0] != 0u) {
    (void)fprintf(stderr,
                  "FAIL: fetched MFC0 c%u case=%u seed=%08x expected=%08x r%u; complete-state invariant failed\n",
                  cp0_reg, case_index, seed, expected_value, scalar_reg);
    return false;
  }
  return true;
}

static bool cp0_read_fuzz(rsp_fuzz_fixture *fixture) {
  for (uint32_t cp0_reg = 0u; cp0_reg < 8u; cp0_reg++) {
    uint32_t case_index;
    uint32_t accepted = 0u;
    for (case_index = 0u; case_index < FUZZ_CASES_PER_OPCODE; case_index++) {
      uint32_t seed = UINT32_C(0x05f40001) ^
                      (cp0_reg * UINT32_C(0x9e3779b9)) ^
                      (case_index * UINT32_C(0x85ebca6b));
      if (!run_cp0_read_tuple(fixture, cp0_reg, case_index, seed)) {
        (void)fprintf(stderr, "FAIL: MFC0 c%u accepted=%u refused=0\n",
                      cp0_reg, accepted);
        return false;
      }
      accepted++;
    }
    {
      char name[24];
      (void)snprintf(name, sizeof name, "MFC0 c%u", cp0_reg);
      report_opcode(cp0_reg + 1u, name, accepted, UINT32_C(0x05f40001));
    }
  }
  return true;
}

static bool run_cp0_write_tuple(rsp_fuzz_fixture *fixture, uint32_t cp0_reg,
                                uint32_t case_index, uint32_t seed) {
  t64_machine *machine = fixture->machine;
  uint8_t vectors_before[32][16];
  uint8_t dmem_before[T64_SP_BANK_BYTES];
  uint64_t accumulators_before[8];
  uint32_t gpr_before[32];
  uint32_t random_state = seed == 0u ? 1u : seed;
  uint32_t scalar_reg = 1u + (case_index % 31u);
  uint32_t value = cp0_reg == 4u || cp0_reg == 7u ? 0u :
                   cp0_reg == 2u || cp0_reg == 3u ? 0u :
                   next_random(&random_state);
  uint32_t instruction;
  uint32_t expected_mem_address = 0u;
  uint32_t expected_dram_address = 0u;
  uint16_t vco_before;
  uint16_t vcc_before;
  uint8_t vce_before;
  uint32_t divider_input_before;
  uint32_t divider_result_before;
  bool divider_high_valid_before;
  bool semaphore_before;

  seed_machine(fixture, &random_state);
  machine->sp.mem_address = UINT32_C(0x0100);
  machine->sp.dram_address = UINT32_C(0);
  machine->rsp.gpr[scalar_reg] = value;
  machine->rsp.sp_semaphore = true;
  semaphore_before = machine->rsp.sp_semaphore;
  (void)memcpy(vectors_before, machine->rsp.vector, sizeof vectors_before);
  (void)memcpy(accumulators_before, machine->rsp.accumulator,
               sizeof accumulators_before);
  (void)memcpy(gpr_before, machine->rsp.gpr, sizeof gpr_before);
  (void)memcpy(dmem_before, machine->sp_dmem, sizeof dmem_before);
  vco_before = machine->rsp.vco;
  vcc_before = machine->rsp.vcc;
  vce_before = machine->rsp.vce;
  divider_input_before = machine->rsp.divider_input;
  divider_result_before = machine->rsp.divider_result;
  divider_high_valid_before = machine->rsp.divider_high_valid;
  if (cp0_reg == 0u) expected_mem_address = value & UINT32_C(0x1fff);
  if (cp0_reg == 1u) expected_dram_address = value & UINT32_C(0x00ffffff);
  instruction = (UINT32_C(0x10) << 26u) | (UINT32_C(4) << 21u) |
                (scalar_reg << 16u) | (cp0_reg << 11u);
  if (!execute_fetched(machine, instruction) || machine->rsp.pc != 4u ||
      memcmp(machine->rsp.gpr, gpr_before, sizeof gpr_before) != 0 ||
      memcmp(machine->rsp.vector, vectors_before, sizeof vectors_before) != 0 ||
      memcmp(machine->rsp.accumulator, accumulators_before,
             sizeof accumulators_before) != 0 ||
      memcmp(machine->sp_dmem, dmem_before, sizeof dmem_before) != 0 ||
      machine->rsp.vco != vco_before || machine->rsp.vcc != vcc_before ||
      machine->rsp.vce != vce_before ||
      machine->rsp.divider_input != divider_input_before ||
      machine->rsp.divider_result != divider_result_before ||
      machine->rsp.divider_high_valid != divider_high_valid_before ||
      machine->rsp.sp_semaphore != (cp0_reg == 7u ? false : semaphore_before) ||
      (cp0_reg == 0u && machine->sp.mem_address != expected_mem_address) ||
      (cp0_reg == 1u && machine->sp.dram_address != expected_dram_address) ||
      ((cp0_reg == 2u || cp0_reg == 3u) && !machine->sp.active.valid) ||
      machine->rsp.gpr[0] != 0u) {
    (void)fprintf(stderr,
                  "FAIL: fetched MTC0 c%u case=%u seed=%08x value=%08x; result/preservation invariant failed\n",
                  cp0_reg, case_index, seed, value);
    return false;
  }
  return true;
}

static bool cp0_write_fuzz(rsp_fuzz_fixture *fixture) {
  static const uint8_t kWritableRegisters[] = {0u, 1u, 2u, 3u, 4u, 7u};
  size_t index;
  for (index = 0u;
       index < sizeof kWritableRegisters / sizeof kWritableRegisters[0];
       index++) {
    uint32_t case_index;
    uint32_t accepted = 0u;
    uint32_t cp0_reg = kWritableRegisters[index];
    for (case_index = 0u; case_index < FUZZ_CASES_PER_OPCODE; case_index++) {
      uint32_t seed = UINT32_C(0x05f50001) ^
                      (cp0_reg * UINT32_C(0x9e3779b9)) ^
                      (case_index * UINT32_C(0x85ebca6b));
      if (!run_cp0_write_tuple(fixture, cp0_reg, case_index, seed)) {
        (void)fprintf(stderr, "FAIL: MTC0 c%u accepted=%u refused=0\n",
                      cp0_reg, accepted);
        return false;
      }
      accepted++;
    }
    {
      char name[24];
      (void)snprintf(name, sizeof name, "MTC0 c%u", cp0_reg);
      report_opcode((uint32_t)(index + 1u), name, accepted,
                    UINT32_C(0x05f50001));
    }
  }
  return true;
}

static uint32_t vector_memory_scale(uint32_t subopcode) {
  if (subopcode == 1u) return 1u;
  if (subopcode == 2u) return 2u;
  if (subopcode == 3u || subopcode == 6u || subopcode == 7u) return 3u;
  if (subopcode >= 4u && subopcode <= 11u) return 4u;
  return 0u;
}

static bool vector_memory_element(bool load, uint32_t subopcode,
                                 uint32_t case_index, uint32_t *element) {
  switch (subopcode) {
  case 0u: *element = case_index & 15u; return true;
  case 1u: *element = (case_index & 7u) * 2u; return true;
  case 2u: *element = (case_index & 3u) * 4u; return true;
  case 3u: *element = (case_index & 1u) * 8u; return true;
  case 4u: case 5u: case 6u: case 7u: case 8u:
    *element = 0u; return true;
  case 9u: *element = (case_index & 1u) * 8u; return true;
  case 10u:
    if (load) return false;
    *element = (case_index & 7u) * 2u; return true;
  case 11u: *element = (case_index & 7u) * 2u; return true;
  default: return false;
  }
}

static bool vector_memory_supported(bool load, uint32_t subopcode) {
  return subopcode <= 9u || (!load && subopcode == 10u) || subopcode == 11u;
}

static void expected_vector_memory(bool load, uint32_t subopcode,
                                   uint32_t vector_register,
                                   uint32_t element, uint32_t address,
                                   uint8_t vectors[32][16], uint8_t dmem[4096]) {
  uint32_t index;
  if (subopcode <= 3u) {
    uint32_t size = subopcode == 0u ? 1u : subopcode == 1u ? 2u :
                    subopcode == 2u ? 4u : 8u;
    for (index = 0u; index < size; index++) {
      uint32_t vector_byte = element + index;
      uint32_t memory_address = (address + index) & UINT32_C(0x0fff);
      if (load) vectors[vector_register][vector_byte] = dmem[memory_address];
      else dmem[memory_address] = vectors[vector_register][vector_byte];
    }
  } else if (subopcode == 4u || subopcode == 5u) {
    uint32_t count = subopcode == 4u ? 16u - (address & 15u)
                                     : address & 15u;
    uint32_t aligned = address & UINT32_C(0x0ff0);
    uint32_t vector_start = subopcode == 4u ? 0u : 16u - count;
    for (index = 0u; index < count; index++) {
      uint32_t vector_byte = vector_start + index;
      uint32_t memory_address =
          (subopcode == 4u ? address + index : aligned + index) &
          UINT32_C(0x0fff);
      if (load) vectors[vector_register][vector_byte] = dmem[memory_address];
      else dmem[memory_address] = vectors[vector_register][vector_byte];
    }
  } else if (subopcode >= 6u && subopcode <= 9u) {
    uint32_t stride = subopcode == 8u ? 2u : subopcode == 9u ? 4u : 1u;
    uint32_t count = subopcode == 9u ? 4u : 8u;
    uint32_t lane_start = subopcode == 9u ? element / 2u : 0u;
    for (index = 0u; index < count; index++) {
      uint32_t lane = lane_start + index;
      uint32_t memory_address =
          (address + index * stride) & UINT32_C(0x0fff);
      if (load) {
        uint8_t source = dmem[memory_address];
        uint16_t value = subopcode == 6u
                             ? (uint16_t)((uint16_t)source << 8u)
                             : (uint16_t)((uint16_t)source << 7u);
        write_expected_lane(vectors[vector_register], lane, value);
      } else {
        uint16_t value = read_lane(vectors[vector_register], lane);
        dmem[memory_address] = subopcode == 6u
                                   ? (uint8_t)(value >> 8u)
                                   : (uint8_t)((value >> 7u) & 0xffu);
      }
    }
  } else {
    uint32_t register_base = vector_register & UINT32_C(0x18);
    uint32_t rotation = element / 2u;
    uint32_t aligned = address & UINT32_C(0x0ff0);
    for (index = 0u; index < 8u; index++) {
      uint32_t vector = register_base | ((index + rotation) & 7u);
      uint32_t lane = subopcode == 10u ? (index + rotation) & 7u : index;
      uint32_t memory_address = (aligned + index * 2u) & UINT32_C(0x0fff);
      if (load) {
        uint16_t value =
            (uint16_t)(((uint16_t)dmem[memory_address] << 8u) |
                       dmem[(memory_address + 1u) & UINT32_C(0x0fff)]);
        write_expected_lane(vectors[vector], lane, value);
      } else {
        uint16_t value = read_lane(vectors[vector], lane);
        dmem[memory_address] = (uint8_t)(value >> 8u);
        dmem[(memory_address + 1u) & UINT32_C(0x0fff)] = (uint8_t)value;
      }
    }
  }
}

static bool run_vector_memory_tuple(rsp_fuzz_fixture *fixture, bool load,
                                    uint32_t subopcode,
                                    uint32_t case_index, uint32_t seed) {
  t64_machine *machine = fixture->machine;
  uint8_t expected_vectors[32][16];
  uint8_t expected_dmem[T64_SP_BANK_BYTES];
  uint64_t accumulators_before[8];
  uint32_t gpr_before[32];
  uint32_t random_state = seed == 0u ? 1u : seed;
  uint32_t base_reg = 1u + (case_index % 31u);
  uint32_t vector_reg = case_index & 31u;
  uint32_t element;
  uint32_t scale = vector_memory_scale(subopcode);
  uint32_t raw_offset = (case_index * 37u) & 0x7fu;
  uint32_t signed_offset = (raw_offset & 0x40u) != 0u
                               ? raw_offset | UINT32_C(0xffffff80)
                               : raw_offset;
  uint32_t address = (case_index * 59u) & UINT32_C(0x0fff);
  uint32_t effective;
  uint32_t base;
  uint32_t instruction;
  uint16_t vco_before;
  uint16_t vcc_before;
  uint8_t vce_before;
  uint32_t divider_input_before;
  uint32_t divider_result_before;
  bool divider_high_valid_before;

  if (!vector_memory_element(load, subopcode, case_index, &element)) {
    return false;
  }
  seed_machine(fixture, &random_state);
  if (subopcode == 8u) address = (address & UINT32_C(0x0ff0)) |
                                         (case_index & 1u);
  if (subopcode == 9u) address = (address & UINT32_C(0x0ff0)) |
                                         (case_index & 3u);
  if (subopcode >= 10u) address &= UINT32_C(0x0ff0);
  base = (address - (signed_offset << scale)) & UINT32_C(0x0fff);
  machine->rsp.gpr[base_reg] = base;
  effective = (base + (signed_offset << scale)) & UINT32_C(0x0fff);
  (void)memcpy(expected_vectors, machine->rsp.vector,
               sizeof expected_vectors);
  (void)memcpy(expected_dmem, machine->sp_dmem, sizeof expected_dmem);
  (void)memcpy(accumulators_before, machine->rsp.accumulator,
               sizeof accumulators_before);
  (void)memcpy(gpr_before, machine->rsp.gpr, sizeof gpr_before);
  vco_before = machine->rsp.vco;
  vcc_before = machine->rsp.vcc;
  vce_before = machine->rsp.vce;
  divider_input_before = machine->rsp.divider_input;
  divider_result_before = machine->rsp.divider_result;
  divider_high_valid_before = machine->rsp.divider_high_valid;
  expected_vector_memory(load, subopcode, vector_reg, element, effective,
                        expected_vectors, expected_dmem);
  instruction = ((load ? UINT32_C(0x32) : UINT32_C(0x3a)) << 26u) |
                (base_reg << 21u) | (vector_reg << 16u) |
                (subopcode << 11u) | (element << 7u) | raw_offset;
  if (!execute_fetched(machine, instruction) || machine->rsp.pc != 4u ||
      memcmp(machine->rsp.gpr, gpr_before, sizeof gpr_before) != 0 ||
      memcmp(machine->rsp.vector, expected_vectors,
             sizeof expected_vectors) != 0 ||
      memcmp(machine->rsp.accumulator, accumulators_before,
             sizeof accumulators_before) != 0 ||
      memcmp(machine->sp_dmem, expected_dmem, sizeof expected_dmem) != 0 ||
      machine->rsp.vco != vco_before || machine->rsp.vcc != vcc_before ||
      machine->rsp.vce != vce_before ||
      machine->rsp.divider_input != divider_input_before ||
      machine->rsp.divider_result != divider_result_before ||
      machine->rsp.divider_high_valid != divider_high_valid_before ||
      machine->rsp.gpr[0] != 0u) {
    (void)fprintf(stderr,
                  "FAIL: fetched vector-memory %s sub=%u case=%u seed=%08x e=%u address=%03x; complete-state invariant failed\n",
                  load ? "load" : "store", subopcode, case_index, seed,
                  element, effective);
    return false;
  }
  return true;
}

static bool vector_memory_fuzz(rsp_fuzz_fixture *fixture) {
  uint32_t direction;
  uint32_t subopcode;
  uint32_t emitted = 0u;
  for (direction = 0u; direction < 2u; direction++) {
    bool load = direction == 0u;
    for (subopcode = 0u; subopcode <= 11u; subopcode++) {
      uint32_t case_index;
      uint32_t accepted = 0u;
      if (!vector_memory_supported(load, subopcode)) continue;
      for (case_index = 0u; case_index < FUZZ_CASES_PER_OPCODE; case_index++) {
        uint32_t seed = UINT32_C(0x05f60001) ^
                        (direction * UINT32_C(0x9e3779b9)) ^
                        (subopcode * UINT32_C(0x85ebca6b)) ^ case_index;
        if (!run_vector_memory_tuple(fixture, load, subopcode, case_index,
                                     seed)) {
          (void)fprintf(stderr,
                        "FAIL: vector-memory %s sub=%u accepted=%u refused=0\n",
                        load ? "load" : "store", subopcode, accepted);
          return false;
        }
        accepted++;
      }
      emitted++;
      {
        char name[24];
        (void)snprintf(name, sizeof name, "%s sub=%u",
                       load ? "LWC2" : "SWC2", subopcode);
        report_opcode(emitted, name, accepted, UINT32_C(0x05f60001));
      }
    }
  }
  return true;
}

enum {
  CONTROL_I_BRANCH = 0,
  CONTROL_REGIMM = 1,
  CONTROL_JUMP = 2,
  CONTROL_REGISTER_JUMP = 3,
  CONTROL_BREAK = 4
};

typedef struct {
  const char *name;
  uint8_t kind;
  uint8_t operation;
  uint8_t subcode;
} control_opcode;

static const control_opcode kControlOpcodes[] = {
    {"BEQ", CONTROL_I_BRANCH, 0x04u, 0u},
    {"BNE", CONTROL_I_BRANCH, 0x05u, 0u},
    {"BLEZ", CONTROL_I_BRANCH, 0x06u, 0u},
    {"BGTZ", CONTROL_I_BRANCH, 0x07u, 0u},
    {"BLTZ", CONTROL_REGIMM, 0x01u, 0u},
    {"BGEZ", CONTROL_REGIMM, 0x01u, 1u},
    {"BLTZAL", CONTROL_REGIMM, 0x01u, 16u},
    {"BGEZAL", CONTROL_REGIMM, 0x01u, 17u},
    {"J", CONTROL_JUMP, 0x02u, 0u},
    {"JAL", CONTROL_JUMP, 0x03u, 0u},
    {"JR", CONTROL_REGISTER_JUMP, 0x08u, 0u},
    {"JALR", CONTROL_REGISTER_JUMP, 0x09u, 0u},
    {"BREAK", CONTROL_BREAK, 0x0du, 0u}};

static bool run_control_tuple(rsp_fuzz_fixture *fixture,
                              const control_opcode *opcode,
                              uint32_t case_index, uint32_t seed) {
  t64_machine *machine = fixture->machine;
  uint8_t vectors_before[32][16];
  uint8_t dmem_before[T64_SP_BANK_BYTES];
  uint64_t accumulators_before[8];
  uint32_t gpr_before[32];
  uint32_t expected_gpr[32];
  uint32_t random_state = seed == 0u ? 1u : seed;
  uint32_t rs = 1u + (case_index % 31u);
  uint32_t rt = 1u + ((case_index >> 3u) % 31u);
  uint32_t rd = 1u + ((case_index >> 5u) % 31u);
  uint16_t immediate = (uint16_t)(case_index * 521u);
  uint32_t instruction;
  uint32_t target = 0u;
  uint32_t source_a;
  uint32_t source_b;
  uint32_t link_destination = 0u;
  uint32_t divider_input_before;
  uint32_t divider_result_before;
  bool divider_high_valid_before;
  uint16_t vco_before;
  uint16_t vcc_before;
  uint8_t vce_before;
  bool taken = false;

  seed_machine(fixture, &random_state);
  if (opcode->kind == CONTROL_REGIMM &&
      (opcode->subcode == 16u || opcode->subcode == 17u)) {
    rs = 1u + (case_index % 30u);
  }
  if (opcode->kind == CONTROL_REGISTER_JUMP) {
    rd = opcode->operation == 8u ? 0u : 1u + ((case_index >> 5u) % 31u);
    if (rd == rs) rd = (rd % 31u) + 1u;
  }
  source_a = scalar_edge_value(case_index % 12u,
                               next_random(&random_state));
  source_b = scalar_edge_value((case_index / 5u) % 12u,
                               next_random(&random_state));
  machine->rsp.gpr[rs] = source_a;
  machine->rsp.gpr[rt] = source_b;
  source_a = machine->rsp.gpr[rs];
  source_b = machine->rsp.gpr[rt];
  if (opcode->kind == CONTROL_I_BRANCH) {
    if (opcode->operation == 0x04u) taken = source_a == source_b;
    else if (opcode->operation == 0x05u) taken = source_a != source_b;
    else if (opcode->operation == 0x06u) {
      taken = (source_a & UINT32_C(0x80000000)) != 0u || source_a == 0u;
      rt = 0u;
    } else {
      taken = (source_a & UINT32_C(0x80000000)) == 0u && source_a != 0u;
      rt = 0u;
    }
    target = taken ? (UINT32_C(4) + (sign_extend_16(immediate) << 2u)) &
                         UINT32_C(0x0ffc)
                   : UINT32_C(8);
    instruction = ((uint32_t)opcode->operation << 26u) | (rs << 21u) |
                  (rt << 16u) | immediate;
  } else if (opcode->kind == CONTROL_REGIMM) {
    taken = opcode->subcode == 0u || opcode->subcode == 16u
                ? (source_a & UINT32_C(0x80000000)) != 0u
                : (source_a & UINT32_C(0x80000000)) == 0u;
    target = taken ? (UINT32_C(4) + (sign_extend_16(immediate) << 2u)) &
                         UINT32_C(0x0ffc)
                   : UINT32_C(8);
    instruction = (UINT32_C(1) << 26u) | (rs << 21u) |
                  ((uint32_t)opcode->subcode << 16u) | immediate;
    if (opcode->subcode == 16u || opcode->subcode == 17u) {
      link_destination = 31u;
    }
  } else if (opcode->kind == CONTROL_JUMP) {
    uint32_t jump_index = (case_index * UINT32_C(7919)) &
                          UINT32_C(0x03ffffff);
    taken = true;
    target = (jump_index << 2u) & UINT32_C(0x0ffc);
    instruction = ((uint32_t)opcode->operation << 26u) | jump_index;
    if (opcode->operation == 3u) link_destination = 31u;
  } else if (opcode->kind == CONTROL_REGISTER_JUMP) {
    source_a = (case_index * 4u) & UINT32_C(0x0ffc);
    machine->rsp.gpr[rs] = source_a;
    taken = true;
    target = source_a & UINT32_C(0x0ffc);
    instruction = (rs << 21u) | (rd << 11u) | opcode->operation;
    if (opcode->operation == 9u) link_destination = rd;
  } else {
    uint32_t code = (case_index * UINT32_C(0x04e5a2d7)) &
                    UINT32_C(0x03ffffc0);
    instruction = code | opcode->operation;
  }
  (void)memcpy(vectors_before, machine->rsp.vector, sizeof vectors_before);
  (void)memcpy(accumulators_before, machine->rsp.accumulator,
               sizeof accumulators_before);
  (void)memcpy(gpr_before, machine->rsp.gpr, sizeof gpr_before);
  (void)memcpy(expected_gpr, gpr_before, sizeof expected_gpr);
  (void)memcpy(dmem_before, machine->sp_dmem, sizeof dmem_before);
  divider_input_before = machine->rsp.divider_input;
  divider_result_before = machine->rsp.divider_result;
  divider_high_valid_before = machine->rsp.divider_high_valid;
  vco_before = machine->rsp.vco;
  vcc_before = machine->rsp.vcc;
  vce_before = machine->rsp.vce;
  if (link_destination != 0u) expected_gpr[link_destination] = 8u;
  if (!execute_fetched(machine, instruction)) {
    (void)fprintf(stderr,
                  "FAIL: fetched control %s case=%u seed=%08x did not retire\n",
                  opcode->name, case_index, seed);
    return false;
  }
  if (opcode->kind == CONTROL_BREAK) {
    if (machine->rsp.pc != 0u ||
        (machine->rsp.sp_status & (T64_RSP_STATUS_HALT |
                                   T64_RSP_STATUS_BROKE)) !=
            (T64_RSP_STATUS_HALT | T64_RSP_STATUS_BROKE)) {
      (void)fprintf(stderr, "FAIL: BREAK status/PC mismatch case=%u\n",
                    case_index);
      return false;
    }
  } else if (machine->rsp.pc != 4u || !machine->rsp.delay_slot_pending ||
             machine->rsp.delay_target != target ||
             machine->rsp.branch_bubble_pending != taken) {
    (void)fprintf(stderr,
                  "FAIL: control %s case=%u target=%03x taken=%u; pc=%03x delay=%u bubble=%u actual-target=%03x\n",
                  opcode->name, case_index, target, taken ? 1u : 0u,
                  machine->rsp.pc, machine->rsp.delay_slot_pending ? 1u : 0u,
                  machine->rsp.branch_bubble_pending ? 1u : 0u,
                  machine->rsp.delay_target);
    return false;
  }
  if (memcmp(machine->rsp.gpr, expected_gpr, sizeof expected_gpr) != 0 ||
      memcmp(machine->rsp.vector, vectors_before, sizeof vectors_before) != 0 ||
      memcmp(machine->rsp.accumulator, accumulators_before,
             sizeof accumulators_before) != 0 ||
      memcmp(machine->sp_dmem, dmem_before, sizeof dmem_before) != 0 ||
      machine->rsp.vco != vco_before || machine->rsp.vcc != vcc_before ||
      machine->rsp.vce != vce_before ||
      machine->rsp.divider_input != divider_input_before ||
      machine->rsp.divider_result != divider_result_before ||
      machine->rsp.divider_high_valid != divider_high_valid_before ||
      machine->rsp.gpr[0] != 0u ||
      machine->rsp.retired_instructions != UINT64_C(1)) {
    (void)fprintf(stderr,
                  "FAIL: fetched control %s case=%u seed=%08x; complete-state invariant failed\n",
                  opcode->name, case_index, seed);
    return false;
  }
  return true;
}

static bool control_fuzz(rsp_fuzz_fixture *fixture) {
  size_t opcode_index;
  for (opcode_index = 0u;
       opcode_index < sizeof kControlOpcodes / sizeof kControlOpcodes[0];
       opcode_index++) {
    uint32_t case_index;
    uint32_t accepted = 0u;
    for (case_index = 0u; case_index < FUZZ_CASES_PER_OPCODE; case_index++) {
      uint32_t seed = UINT32_C(0x05f70001) ^
                      ((uint32_t)opcode_index * UINT32_C(0x9e3779b9)) ^
                      (case_index * UINT32_C(0x85ebca6b));
      if (!run_control_tuple(fixture, &kControlOpcodes[opcode_index],
                             case_index, seed)) {
        (void)fprintf(stderr, "FAIL: control %s accepted=%u refused=0\n",
                      kControlOpcodes[opcode_index].name, accepted);
        return false;
      }
      accepted++;
    }
    report_opcode((uint32_t)(opcode_index + 1u),
                  kControlOpcodes[opcode_index].name, accepted,
                  UINT32_C(0x05f70001));
  }
  return true;
}

static uint32_t full_corpus_opcode_count(void) {
  return (uint32_t)(sizeof kVectorOpcodes / sizeof kVectorOpcodes[0]) +
         (uint32_t)(sizeof kScalarAluOpcodes / sizeof kScalarAluOpcodes[0]) +
         (uint32_t)(sizeof kScalarMemoryOpcodes /
                    sizeof kScalarMemoryOpcodes[0]) +
         (uint32_t)(sizeof kCop2TransferOpcodes /
                    sizeof kCop2TransferOpcodes[0]) +
         UINT32_C(8) + UINT32_C(6) + UINT32_C(23) +
         (uint32_t)(sizeof kControlOpcodes / sizeof kControlOpcodes[0]);
}

static bool refusal_control_fuzz(rsp_fuzz_fixture *fixture);

static bool stress_authored_family(rsp_fuzz_fixture *fixture, uint32_t seed,
                                   uint32_t iterations) {
  static const uint8_t kStressFunctions[] = {
      0x00u, 0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u,
      0x08u, 0x09u, 0x0au, 0x0bu, 0x0cu, 0x0du, 0x0eu, 0x0fu,
      0x30u, 0x31u, 0x32u, 0x33u, 0x34u, 0x35u, 0x36u};
  uint64_t completed = 0u;
  for (uint32_t iteration = 0u; iteration < iterations; iteration++) {
    for (size_t function_index = 0u;
         function_index < sizeof kStressFunctions /
                              sizeof kStressFunctions[0];
         function_index++) {
      uint32_t tuple_seed = seed ^
                            ((uint32_t)function_index *
                             UINT32_C(0x9e3779b9)) ^
                            (iteration * UINT32_C(0x85ebca6b));
      if (!run_vector_tuple(fixture, kStressFunctions[function_index],
                            iteration, tuple_seed)) {
        (void)fprintf(stderr,
                      "FAIL: authored stress function=%02x iteration=%u seed=%08x\n",
                      kStressFunctions[function_index], iteration,
                      tuple_seed);
        return false;
      }
      completed++;
    }
    {
      uint32_t tuple_seed = seed ^ (iteration * UINT32_C(0xc2b2ae35));
      if (!run_divider_pair(fixture, false, iteration, tuple_seed) ||
          !run_divider_pair(fixture, true, iteration,
                            tuple_seed ^ UINT32_C(0x27d4eb2f))) {
        return false;
      }
      completed += UINT64_C(2);
    }
    if ((iteration + 1u) % UINT32_C(10000) == 0u) {
      (void)printf("# stress progress iteration=%u/%u fetched-cases=%llu\n",
                   iteration + 1u, iterations,
                   (unsigned long long)completed);
    }
  }
  (void)printf("ok 1 - authored VMULF-through-32-bit-VRCP/VRSQ stress seed=%08x iterations=%u opcodes=23 plus reciprocal/rsqrt high-low sequences=%u fetched-cases=%llu\n",
               seed, iterations, iterations * 2u,
               (unsigned long long)completed);
  return true;
}

static bool parse_u32_argument(const char *text, uint32_t *value_out) {
  unsigned long long parsed;
  char *end = NULL;
  if (text == NULL || value_out == NULL || text[0] == '\0' ||
      text[0] == '-' || text[0] == '+') {
    return false;
  }
  errno = 0;
  parsed = strtoull(text, &end, 0);
  if (errno != 0 || end == text || *end != '\0' ||
      parsed > (unsigned long long)UINT32_MAX) {
    return false;
  }
  *value_out = (uint32_t)parsed;
  return true;
}

static bool full_corpus_fuzz(rsp_fuzz_fixture *fixture) {
  uint32_t expected_count = full_corpus_opcode_count();
  bool passed;

  full_corpus_mode = true;
  reported_opcode_count = 0u;
  (void)printf("1..%u\n", expected_count);
  passed = vector_function_fuzz(fixture) && scalar_alu_fuzz(fixture) &&
           scalar_memory_fuzz(fixture) && control_fuzz(fixture) &&
           cp0_read_fuzz(fixture) && cp0_write_fuzz(fixture) &&
           cop2_transfer_fuzz(fixture) && vector_memory_fuzz(fixture);
  if (!passed) return false;
  if (reported_opcode_count != expected_count) {
    (void)fprintf(stderr,
                  "not ok - supported opcode denominator mismatch: expected=%u observed=%u\n",
                  expected_count, reported_opcode_count);
    return false;
  }
  if (!refusal_control_fuzz(fixture)) return false;
  (void)printf("# complete: %u supported opcode forms each retired 256 deterministic fetched tuples; refusals are tracked separately\n",
               expected_count);
  (void)printf("# evidence: authored reference vectors and per-opcode invariants; optimized implementation absent, differential not run\n");
  return true;
}

static bool expect_fetched_refusal(rsp_fuzz_fixture *fixture,
                                   uint32_t instruction,
                                   uint32_t expected_reason,
                                   uint32_t seed) {
  t64_machine *machine = fixture->machine;
  t64_stop_request request;
  t64_stop_result result = {0u, 0u};
  uint8_t vectors_before[32][16];
  uint8_t dmem_before[T64_SP_BANK_BYTES];
  uint64_t accumulators_before[8];
  uint32_t gpr_before[32];
  t64_sp_state sp_before;
  uint32_t divider_input_before;
  uint32_t divider_result_before;
  bool divider_high_valid_before;
  uint16_t vco_before;
  uint16_t vcc_before;
  uint8_t vce_before;
  uint64_t retired_before;
  uint32_t pc_before;
  uint32_t status_before;
  uint32_t pc = machine->rsp.pc & UINT32_C(0x0ffc);

  machine->sp_imem[pc] = (uint8_t)(instruction >> 24u);
  machine->sp_imem[(pc + 1u) & UINT32_C(0x0fff)] =
      (uint8_t)(instruction >> 16u);
  machine->sp_imem[(pc + 2u) & UINT32_C(0x0fff)] =
      (uint8_t)(instruction >> 8u);
  machine->sp_imem[(pc + 3u) & UINT32_C(0x0fff)] = (uint8_t)instruction;
  if (t64_rsp_start(machine) != T64_STATUS_OK) return false;
  (void)memcpy(vectors_before, machine->rsp.vector, sizeof vectors_before);
  (void)memcpy(accumulators_before, machine->rsp.accumulator,
               sizeof accumulators_before);
  (void)memcpy(gpr_before, machine->rsp.gpr, sizeof gpr_before);
  (void)memcpy(dmem_before, machine->sp_dmem, sizeof dmem_before);
  sp_before = machine->sp;
  divider_input_before = machine->rsp.divider_input;
  divider_result_before = machine->rsp.divider_result;
  divider_high_valid_before = machine->rsp.divider_high_valid;
  vco_before = machine->rsp.vco;
  vcc_before = machine->rsp.vcc;
  vce_before = machine->rsp.vce;
  retired_before = machine->rsp.retired_instructions;
  pc_before = machine->rsp.pc;
  status_before = machine->rsp.sp_status;
  request.kind = T64_STOP_GUEST_TICK;
  request.value = machine->tick + UINT64_C(1);
  if (t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
      result.tick != request.value ||
      machine->rsp_observed_outcome != T64_RSP_ATTEMPT_REFUSED ||
      machine->rsp_observed_opcode != instruction ||
      machine->rsp_observed_refusal_reason != expected_reason ||
      machine->rsp.retired_instructions != retired_before ||
      machine->rsp.pc != pc_before || machine->rsp.sp_status != status_before ||
      memcmp(machine->rsp.gpr, gpr_before, sizeof gpr_before) != 0 ||
      memcmp(machine->rsp.vector, vectors_before, sizeof vectors_before) != 0 ||
      memcmp(machine->rsp.accumulator, accumulators_before,
             sizeof accumulators_before) != 0 ||
      memcmp(machine->sp_dmem, dmem_before, sizeof dmem_before) != 0 ||
      memcmp(&machine->sp, &sp_before, sizeof sp_before) != 0 ||
      machine->rsp.vco != vco_before || machine->rsp.vcc != vcc_before ||
      machine->rsp.vce != vce_before ||
      machine->rsp.divider_input != divider_input_before ||
      machine->rsp.divider_result != divider_result_before ||
      machine->rsp.divider_high_valid != divider_high_valid_before) {
    (void)fprintf(stderr,
                  "FAIL: fetched refusal opcode=%08x seed=%08x expected-reason=%u actual=%u outcome=%u; state-preservation invariant failed\n",
                  instruction, seed, expected_reason,
                  machine->rsp_observed_refusal_reason,
                  machine->rsp_observed_outcome);
    return false;
  }
  return true;
}

static bool no_hle_controls(rsp_fuzz_fixture *fixture) {
  t64_machine *machine = fixture->machine;
  uint8_t first_vectors[32][16];
  uint8_t first_dmem[T64_SP_BANK_BYTES];
  uint64_t first_accumulators[8];
  uint32_t first_gpr[32];
  t64_sp_state first_sp;
  uint32_t first_divider_input;
  uint32_t first_divider_result;
  uint32_t first_pc;
  uint32_t first_sp_status;
  uint64_t first_tick;
  uint64_t first_retired;
  uint16_t first_vco;
  uint16_t first_vcc;
  uint8_t first_vce;
  bool first_divider_high_valid;
  uint32_t random_state = UINT32_C(0x05f80a11);
  uint32_t first_instruction;
  uint32_t second_instruction;
  uint32_t first_result;
  uint32_t second_result;

  seed_machine(fixture, &random_state);
  machine->rsp.gpr[1] = UINT32_C(0x100);
  machine->sp_dmem[0x100] = 0x10u;
  machine->sp_dmem[0x101] = 0x20u;
  machine->sp_dmem[0x102] = 0x30u;
  machine->sp_dmem[0x103] = 0x41u;
  first_instruction = (UINT32_C(0x23) << 26u) | (UINT32_C(1) << 21u) |
                      (UINT32_C(3) << 16u);
  if (!execute_fetched(machine, first_instruction) ||
      machine->rsp_observed_opcode != first_instruction ||
      machine->rsp.gpr[3] != UINT32_C(0x10203041)) {
    (void)fprintf(stderr,
                  "FAIL: same-program/different-content control first fetched LW result\n");
    return false;
  }
  first_result = machine->rsp.gpr[3];
  (void)memcpy(first_vectors, machine->rsp.vector, sizeof first_vectors);
  (void)memcpy(first_dmem, machine->sp_dmem, sizeof first_dmem);
  (void)memcpy(first_accumulators, machine->rsp.accumulator,
               sizeof first_accumulators);
  (void)memcpy(first_gpr, machine->rsp.gpr, sizeof first_gpr);
  first_sp = machine->sp;
  first_divider_input = machine->rsp.divider_input;
  first_divider_result = machine->rsp.divider_result;
  first_divider_high_valid = machine->rsp.divider_high_valid;
  first_pc = machine->rsp.pc;
  first_sp_status = machine->rsp.sp_status;
  first_tick = machine->tick;
  first_retired = machine->rsp.retired_instructions;
  first_vco = machine->rsp.vco;
  first_vcc = machine->rsp.vcc;
  first_vce = machine->rsp.vce;

  random_state = UINT32_C(0x05f80a11);
  seed_machine(fixture, &random_state);
  machine->rsp.gpr[1] = UINT32_C(0x100);
  machine->sp_dmem[0x100] = 0x10u;
  machine->sp_dmem[0x101] = 0x20u;
  machine->sp_dmem[0x102] = 0x30u;
  machine->sp_dmem[0x103] = 0x42u;
  second_instruction = first_instruction;
  first_dmem[0x103] = 0x42u; /* Only the authored input byte differs. */
  if (!execute_fetched(machine, second_instruction) ||
      machine->rsp_observed_opcode != second_instruction ||
      machine->rsp.gpr[3] != UINT32_C(0x10203042)) {
    (void)fprintf(stderr,
                  "FAIL: same-program/different-content control second fetched LW result\n");
    return false;
  }
  second_result = machine->rsp.gpr[3];
  for (uint32_t reg = 0u; reg < 32u; reg++) {
    uint32_t expected = first_gpr[reg];
    if (reg == 3u) expected = second_result;
    if (machine->rsp.gpr[reg] != expected) return false;
  }
  if (first_result == second_result ||
      memcmp(machine->rsp.vector, first_vectors, sizeof first_vectors) != 0 ||
      memcmp(machine->sp_dmem, first_dmem, sizeof first_dmem) != 0 ||
      memcmp(machine->rsp.accumulator, first_accumulators,
             sizeof first_accumulators) != 0 ||
      memcmp(&machine->sp, &first_sp, sizeof first_sp) != 0 ||
      machine->rsp.vco != first_vco || machine->rsp.vcc != first_vcc ||
      machine->rsp.vce != first_vce ||
      machine->rsp.divider_input != first_divider_input ||
      machine->rsp.divider_result != first_divider_result ||
      machine->rsp.divider_high_valid != first_divider_high_valid ||
      machine->rsp.pc != first_pc ||
      machine->rsp.sp_status != first_sp_status ||
      machine->tick != first_tick ||
      machine->rsp.retired_instructions != first_retired) {
    (void)fprintf(stderr,
                  "FAIL: same-program/different-content changed unrelated RSP state\n");
    return false;
  }
  (void)printf("ok 1 - same fetched LW program and initial RSP state produced data-dependent results %08x/%08x\n",
               first_result, second_result);

  random_state = UINT32_C(0x05f80a12);
  seed_machine(fixture, &random_state);
  if (!expect_fetched_refusal(
          fixture, encode_vector(0u, 1u, 2u, 3u, UINT32_C(0x12)),
          T64_RSP_REFUSAL_UNSUPPORTED_OPCODE, UINT32_C(0x05f80a12))) {
    return false;
  }
  (void)printf("ok 2 - unsupported fetched opcode refused with complete RSP/SP state preserved\n");
  return true;
}

static bool refusal_control_fuzz(rsp_fuzz_fixture *fixture) {
  static const uint8_t kReservedFunctions[] = {
      0x12u, 0x16u, 0x17u, 0x18u, 0x19u, 0x1au, 0x1bu, 0x1cu, 0x1eu,
      0x1fu, 0x2eu, 0x2fu, 0x38u, 0x39u, 0x3au, 0x3bu, 0x3cu, 0x3du,
      0x3eu, 0x3fu};
  uint32_t refused = 0u;

  for (size_t function_index = 0u;
       function_index < sizeof kReservedFunctions /
                            sizeof kReservedFunctions[0];
       function_index++) {
    for (uint32_t case_index = 0u; case_index < FUZZ_CASES_PER_OPCODE;
         case_index++) {
      uint32_t seed = UINT32_C(0x05f80001) ^
                      ((uint32_t)function_index * UINT32_C(0x9e3779b9)) ^
                      (case_index * UINT32_C(0x85ebca6b));
      uint32_t instruction = encode_vector(
          case_index & 15u, case_index & 31u,
          (case_index >> 3u) & 31u, (case_index >> 5u) & 31u,
          kReservedFunctions[function_index]);
      uint32_t random_state = seed == 0u ? 1u : seed;
      seed_machine(fixture, &random_state);
      if (!expect_fetched_refusal(fixture, instruction,
                                  T64_RSP_REFUSAL_UNSUPPORTED_OPCODE, seed)) {
        return false;
      }
      refused++;
    }
  }
  if (refused != (uint32_t)(sizeof kReservedFunctions /
                            sizeof kReservedFunctions[0]) *
                      FUZZ_CASES_PER_OPCODE) {
    return false;
  }
  (void)printf("# refusal-control: reserved vector-function slots refused=%u accepted=0\n",
               refused);

  for (uint32_t cp0_reg = 8u; cp0_reg <= 15u; cp0_reg++) {
    for (uint32_t direction = 0u; direction < 2u; direction++) {
      for (uint32_t case_index = 0u; case_index < FUZZ_CASES_PER_OPCODE;
           case_index++) {
        uint32_t seed = UINT32_C(0x05f90001) ^
                        (cp0_reg * UINT32_C(0x9e3779b9)) ^
                        (direction * UINT32_C(0x85ebca6b)) ^ case_index;
        uint32_t instruction = (UINT32_C(0x10) << 26u) |
                               ((direction != 0u ? UINT32_C(4) : 0u) << 21u) |
                               ((case_index & 31u) << 16u) |
                               (cp0_reg << 11u);
        uint32_t random_state = seed == 0u ? 1u : seed;
        seed_machine(fixture, &random_state);
        if (!expect_fetched_refusal(fixture, instruction,
                                    T64_RSP_REFUSAL_DP_UNAVAILABLE, seed)) {
          return false;
        }
        refused++;
      }
    }
  }
  (void)printf("# refusal-control: DP c8-c15 read/write refused=%u accepted=0\n",
               UINT32_C(16) * FUZZ_CASES_PER_OPCODE);
  return true;
}

static bool vand_pilot(rsp_fuzz_fixture *fixture) {
  uint32_t case_index;
  for (case_index = 0u; case_index < FUZZ_CASES_PER_OPCODE; case_index++) {
    uint32_t random_state = UINT32_C(0x05080001) ^
                            (case_index * UINT32_C(0x9e3779b9));
    if (random_state == 0u) {
      random_state = UINT32_C(1);
    }
    if (!execute_one_vand_tuple(fixture, &random_state)) {
      (void)fprintf(stderr,
                    "FAIL: fetched VAND case=%u seed=%08x failed its full-state invariant\n",
                    case_index, random_state);
      return false;
    }
  }
  return true;
}

int main(int argc, char **argv) {
  rsp_fuzz_fixture fixture;
  bool full_mode = argc == 1;
  bool stress_mode = argc == 4 && strcmp(argv[1], "--stress") == 0;
  bool pilot_only = argc == 2 && strcmp(argv[1], "--pilot") == 0;
  bool vector_mode = argc == 2 && strcmp(argv[1], "--vectors") == 0;
  bool scalar_mode = argc == 2 && strcmp(argv[1], "--scalar") == 0;
  bool scalar_memory_mode = argc == 2 && strcmp(argv[1], "--scalar-memory") == 0;
  bool cop2_mode = argc == 2 && strcmp(argv[1], "--cop2") == 0;
  bool cp0_mode = argc == 2 && strcmp(argv[1], "--cp0") == 0;
  bool vector_memory_mode = argc == 2 && strcmp(argv[1], "--vector-memory") == 0;
  bool control_mode = argc == 2 && strcmp(argv[1], "--control") == 0;
  bool no_hle_control_mode = argc == 2 &&
                             strcmp(argv[1], "--no-hle-controls") == 0;
  bool pilot_valid;
  uint32_t stress_seed = 0u;
  uint32_t stress_iterations = 0u;

  if ((!full_mode && !stress_mode && argc != 2) ||
      (argc == 2 && !pilot_only && !vector_mode && !scalar_mode &&
       !scalar_memory_mode && !cop2_mode && !cp0_mode &&
       !vector_memory_mode && !control_mode && !no_hle_control_mode) ||
      (argc == 4 &&
       (!stress_mode || !parse_u32_argument(argv[2], &stress_seed) ||
        !parse_u32_argument(argv[3], &stress_iterations) ||
        stress_seed == 0u || stress_iterations == 0u ||
        stress_iterations > RSP_STRESS_MAX_ITERATIONS)) ||
      (argc != 1 && argc != 2 && argc != 4)) {
    (void)fprintf(stderr,
                  "usage: test_rsp_fuzz [--pilot|--vectors|--scalar|--scalar-memory|--cop2|--cp0|--vector-memory|--control] | --stress SEED ITERATIONS (1..100000)\n");
    return 2;
  }

  if (!fixture_create(&fixture)) {
    (void)fprintf(stderr, "fuzz fixture setup failed\n");
    fixture_destroy(&fixture);
    return 2;
  }
  if (no_hle_control_mode) {
    (void)printf("1..2\n");
    pilot_valid = no_hle_controls(&fixture);
  } else if (full_mode) {
    pilot_valid = full_corpus_fuzz(&fixture);
  } else if (stress_mode) {
    (void)printf("1..1\n");
    pilot_valid = stress_authored_family(&fixture, stress_seed,
                                         stress_iterations);
  } else if (vector_mode) {
    (void)printf("1..%zu\n",
                 sizeof kVectorOpcodes / sizeof kVectorOpcodes[0]);
    pilot_valid = vector_function_fuzz(&fixture);
  } else if (scalar_mode) {
    (void)printf("1..%zu\n",
                 sizeof kScalarAluOpcodes / sizeof kScalarAluOpcodes[0]);
    pilot_valid = scalar_alu_fuzz(&fixture);
  } else if (scalar_memory_mode) {
    (void)printf("1..%zu\n", sizeof kScalarMemoryOpcodes /
                                   sizeof kScalarMemoryOpcodes[0]);
    pilot_valid = scalar_memory_fuzz(&fixture);
  } else if (cop2_mode) {
    (void)printf("1..%zu\n", sizeof kCop2TransferOpcodes /
                                   sizeof kCop2TransferOpcodes[0]);
    pilot_valid = cop2_transfer_fuzz(&fixture);
  } else if (cp0_mode) {
    (void)printf("1..14\n");
    pilot_valid = cp0_read_fuzz(&fixture) && cp0_write_fuzz(&fixture);
  } else if (vector_memory_mode) {
    (void)printf("1..23\n");
    pilot_valid = vector_memory_fuzz(&fixture);
  } else if (control_mode) {
    (void)printf("1..%zu\n",
                 sizeof kControlOpcodes / sizeof kControlOpcodes[0]);
    pilot_valid = control_fuzz(&fixture);
  } else {
    (void)printf("1..1\n");
    pilot_valid = vand_pilot(&fixture);
  }
  fixture_destroy(&fixture);
  if (!pilot_valid) {
    return 1;
  }
  if (no_hle_control_mode) return 0;
  if (pilot_only) {
    (void)printf("ok 1 - COP2 pilot: VAND retired 256 deterministic tuples with complete vector, ACC, flag, GPR, DMEM, and divider-state checks\n");
    return 0;
  }
  if (stress_mode) return pilot_valid ? 0 : 1;
  if (full_mode) {
    if (!pilot_valid || reported_opcode_count != full_corpus_opcode_count()) {
      (void)fprintf(stderr,
                    "not ok - full supported-opcode denominator incomplete: expected=%u observed=%u\n",
                    full_corpus_opcode_count(), reported_opcode_count);
      return 1;
    }
    return 0;
  }
  if (scalar_mode) {
    (void)printf("# incomplete: scalar ALU subset does not cover scalar control, CP0, transfers, or vector memory\n");
    return 0;
  }
  if (scalar_memory_mode) {
    (void)printf("# incomplete: scalar memory subset does not cover control, CP0, COP2, or vector memory\n");
    return 0;
  }
  if (cop2_mode) {
    (void)printf("# incomplete: COP2 scalar-transfer subset does not cover COP2 vector functions or vector memory\n");
    return 0;
  }
  if (cp0_mode) {
    (void)printf("# incomplete: CP0 subset does not include refused register/DPC requests or scalar control\n");
    return 0;
  }
  if (vector_memory_mode) {
    (void)printf("# incomplete: vector-memory subset does not cover vector functions, scalar control, or refused encodings\n");
    return 0;
  }
  if (control_mode) {
    (void)printf("# incomplete: control subset does not cover other supported opcode families or refusal controls\n");
    return 0;
  }
  if (vector_mode) {
    (void)printf("# incomplete: vector-function pilot is not the full supported-opcode ledger denominator\n");
    return 0;
  } else {
    (void)printf("not ok 2 - full supported-opcode denominator is not yet covered (pilot is incomplete)\n");
  }
  return 1;
}
