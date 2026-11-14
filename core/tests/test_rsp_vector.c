#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_IMEM_BASE UINT32_C(0x04001000)

typedef struct {
  t64_host_allocator allocator;
  t64_machine *machine;
} rsp_vector_fixture;

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

static bool fixture_create(rsp_vector_fixture *fixture) {
  const t64_profile_descriptor *profile = NULL;
  t64_host_storage image = {NULL, 0u, 0u};
  t64_status status;

  (void)memset(fixture, 0, sizeof *fixture);
  fixture->allocator.allocate = fixture_allocate;
  fixture->allocator.release = fixture_release;
  status = t64_profile_lookup(
      "ntsc-cart-4m-openipl3-rsp-functional-v1", &profile);
  if (status != T64_STATUS_OK || profile == NULL ||
      t64_machine_create(&fixture->allocator, profile, &fixture->machine) !=
          T64_STATUS_OK || fixture->machine == NULL ||
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

static void fixture_destroy(rsp_vector_fixture *fixture) {
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

static void write_lane(uint8_t vector[16], uint32_t lane, uint16_t value) {
  vector[lane * 2u] = (uint8_t)(value >> 8);
  vector[lane * 2u + 1u] = (uint8_t)value;
}

static uint16_t read_lane(const uint8_t vector[16], uint32_t lane) {
  return (uint16_t)(((uint16_t)vector[lane * 2u] << 8) |
                    vector[lane * 2u + 1u]);
}

static void reset_machine(rsp_vector_fixture *fixture) {
  t64_machine *machine = fixture->machine;
  t64_machine_reset_execution_state(machine);
  (void)memset(&machine->rsp, 0, sizeof machine->rsp);
  (void)memset(&machine->cpu, 0, sizeof machine->cpu);
  (void)memset(machine->sp_dmem, 0, sizeof machine->sp_dmem);
  (void)memset(machine->sp_imem, 0, sizeof machine->sp_imem);
  machine->rsp.sp_status = UINT32_C(1);
}

static bool execute_word(rsp_vector_fixture *fixture, uint32_t instruction) {
  t64_machine *machine = fixture->machine;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(1)};
  t64_stop_result result = {0u, 0u};

  machine->sp_imem[0] = (uint8_t)(instruction >> 24);
  machine->sp_imem[1] = (uint8_t)(instruction >> 16);
  machine->sp_imem[2] = (uint8_t)(instruction >> 8);
  machine->sp_imem[3] = (uint8_t)instruction;
  return t64_rsp_start(machine) == T64_STATUS_OK &&
         t64_machine_advance(machine, &request, &result) == T64_STATUS_OK &&
         result.tick == UINT64_C(1);
}

static bool vadcc_fetched_carries_and_complete_state(void) {
  static const uint16_t left[8] = {
      UINT16_C(0xffff), UINT16_C(0x7fff), UINT16_C(0x0000),
      UINT16_C(0xffff), UINT16_C(0x8000), UINT16_C(0x0001),
      UINT16_C(0x1234), UINT16_C(0xfffe)};
  static const uint16_t right[8] = {
      UINT16_C(0x0001), UINT16_C(0x0001), UINT16_C(0x0000),
      UINT16_C(0xffff), UINT16_C(0x8000), UINT16_C(0xffff),
      UINT16_C(0x4321), UINT16_C(0x0002)};
  static const uint16_t expected[8] = {
      UINT16_C(0x0000), UINT16_C(0x8000), UINT16_C(0x0000),
      UINT16_C(0xfffe), UINT16_C(0x0000), UINT16_C(0x0000),
      UINT16_C(0x5555), UINT16_C(0x0000)};
  const uint64_t accumulator_before = UINT64_C(0x123456789abc);
  rsp_vector_fixture fixture;
  t64_machine *machine;
  uint32_t lane;
  bool passed = true;

  if (!fixture_create(&fixture)) {
    (void)fprintf(stderr, "VADDC fixture setup failed\n");
    fixture_destroy(&fixture);
    return false;
  }
  machine = fixture.machine;
  reset_machine(&fixture);
  for (lane = 0u; lane < 8u; lane++) {
    write_lane(machine->rsp.vector[1], lane, left[lane]);
    write_lane(machine->rsp.vector[2], lane, right[lane]);
    machine->rsp.accumulator[lane] = accumulator_before;
  }
  machine->rsp.vco = UINT16_C(0xa55a);
  machine->rsp.vcc = UINT16_C(0x963c);
  machine->rsp.vce = UINT8_C(0xd5);
  if (!execute_word(&fixture, encode_vector(0u, 2u, 1u, 3u, 0x14u))) {
    (void)fprintf(stderr, "VADDC fetched execution setup failed\n");
    passed = false;
  } else {
    if (machine->rsp_observed_outcome != T64_RSP_ATTEMPT_RETIRED ||
        machine->rsp.pc != UINT32_C(4) ||
        machine->rsp.retired_instructions != UINT64_C(1)) {
      (void)fprintf(stderr,
                    "VADDC fetched opcode assertion failed: expected "
                    "retired PC=4, got outcome=%u reason=%u pc=%u retired=%llu\n",
                    (unsigned)machine->rsp_observed_outcome,
                    (unsigned)machine->rsp_observed_refusal_reason,
                    (unsigned)machine->rsp.pc,
                    (unsigned long long)machine->rsp.retired_instructions);
      passed = false;
    } else {
      if (machine->rsp.vco != UINT16_C(0x0089) ||
          machine->rsp.vcc != UINT16_C(0x963c) ||
          machine->rsp.vce != UINT8_C(0xd5)) {
        (void)fprintf(stderr,
                      "VADDC fetched flags mismatch: VCO=%04x VCC=%04x VCE=%02x\n",
                      machine->rsp.vco, machine->rsp.vcc, machine->rsp.vce);
        passed = false;
      }
      for (lane = 0u; lane < 8u; lane++) {
        uint64_t accumulator_expected =
            (accumulator_before & ~UINT64_C(0xffff)) | expected[lane];
        if (read_lane(machine->rsp.vector[3], lane) != expected[lane] ||
            machine->rsp.accumulator[lane] != accumulator_expected) {
          (void)fprintf(stderr,
                        "VADDC lane %u mismatch: vector=%04x accumulator=%012llx\n",
                        lane, read_lane(machine->rsp.vector[3], lane),
                        (unsigned long long)machine->rsp.accumulator[lane]);
          passed = false;
        }
      }
    }
  }
  fixture_destroy(&fixture);
  return passed;
}

int main(void) {
  if (!vadcc_fetched_carries_and_complete_state()) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
