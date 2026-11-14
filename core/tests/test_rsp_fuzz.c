#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_IMEM_BASE UINT32_C(0x04001000)
#define FUZZ_CASES_PER_OPCODE UINT32_C(256)

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

  machine->sp_imem[0] = (uint8_t)(instruction >> 24);
  machine->sp_imem[1] = (uint8_t)(instruction >> 16);
  machine->sp_imem[2] = (uint8_t)(instruction >> 8);
  machine->sp_imem[3] = (uint8_t)instruction;
  if (t64_rsp_start(machine) != T64_STATUS_OK) {
    return false;
  }
  request.kind = T64_STOP_GUEST_TICK;
  request.value = machine->tick + UINT64_C(1);
  return t64_machine_advance(machine, &request, &result) == T64_STATUS_OK &&
         machine->rsp_observed_outcome == T64_RSP_ATTEMPT_RETIRED &&
         machine->rsp_observed_opcode == instruction &&
         machine->rsp.retired_instructions == UINT64_C(1) &&
         result.tick == request.value;
}

static bool execute_one_vand_tuple(rsp_fuzz_fixture *fixture,
                                   uint32_t *random_state) {
  t64_machine *machine = fixture->machine;
  uint8_t vectors_before[32][16];
  uint64_t accumulators_before[8];
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
    for (byte = 0u; byte < 16u; byte++) {
      machine->rsp.vector[reg][byte] = (uint8_t)next_random(random_state);
    }
  }
  for (lane = 0u; lane < 8u; lane++) {
    machine->rsp.accumulator[lane] =
        ((uint64_t)next_random(random_state) << 16) |
        (uint64_t)(next_random(random_state) & UINT32_C(0xffff));
  }
  machine->rsp.vco = (uint16_t)next_random(random_state);
  machine->rsp.vcc = (uint16_t)next_random(random_state);
  machine->rsp.vce = (uint8_t)next_random(random_state);
  vco_before = machine->rsp.vco;
  vcc_before = machine->rsp.vcc;
  vce_before = machine->rsp.vce;
  (void)memcpy(vectors_before, machine->rsp.vector, sizeof vectors_before);
  (void)memcpy(accumulators_before, machine->rsp.accumulator,
               sizeof accumulators_before);
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
      machine->rsp.vce != vce_before) {
    valid = false;
  }
  return valid;
}

int main(void) {
  rsp_fuzz_fixture fixture;
  uint32_t random_state = UINT32_C(0x05080001);
  uint32_t cases_for_opcode = 0u;
  bool tuple_valid;

  if (!fixture_create(&fixture)) {
    (void)fprintf(stderr, "fuzz fixture setup failed\n");
    fixture_destroy(&fixture);
    return 2;
  }
  tuple_valid = execute_one_vand_tuple(&fixture, &random_state);
  if (tuple_valid) {
    cases_for_opcode++;
  }
  fixture_destroy(&fixture);
  if (!tuple_valid) {
    (void)fprintf(stderr, "1..1\nnot ok 1 - fetched VAND scalar algebra\n");
    return 1;
  }
  (void)printf("1..1\n%s 1 - VAND has at least 256 deterministic fetched tuples (observed %u)\n",
               cases_for_opcode >= FUZZ_CASES_PER_OPCODE ? "ok" : "not ok",
               cases_for_opcode);
  return cases_for_opcode >= FUZZ_CASES_PER_OPCODE ? 0 : 1;
}
