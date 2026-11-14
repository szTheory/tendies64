#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  t64_host_allocator allocator;
  t64_machine *machine;
} rsp_divider_fixture;

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

static bool fixture_create(rsp_divider_fixture *fixture) {
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
          T64_STATUS_OK ||
      fixture->machine == NULL || !fixture_allocate(fixture, 2u, 1u, &image)) {
    return false;
  }
  ((uint8_t *)image.data)[0] = UINT8_C(0x12);
  ((uint8_t *)image.data)[1] = UINT8_C(0x34);
  fixture->machine->image_storage = image;
  fixture->machine->image_length = 2u;
  fixture->machine->has_image = true;
  return true;
}

static void fixture_destroy(rsp_divider_fixture *fixture) {
  if (fixture != NULL && fixture->machine != NULL) {
    t64_machine_destroy(fixture->machine);
    fixture->machine = NULL;
  }
}

static uint32_t encode_vector(uint32_t element, uint32_t vt, uint32_t de,
                              uint32_t vd, uint32_t function) {
  return (UINT32_C(0x12) << 26) | (UINT32_C(0x10) << 21) |
         ((element & UINT32_C(0x0f)) << 21) | (vt << 16) | (de << 11) |
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

static void prepare_machine(t64_machine *machine) {
  uint32_t reg;
  t64_machine_reset_execution_state(machine);
  (void)memset(&machine->rsp, 0, sizeof machine->rsp);
  (void)memset(&machine->cpu, 0, sizeof machine->cpu);
  (void)memset(machine->sp_dmem, 0, sizeof machine->sp_dmem);
  (void)memset(machine->sp_imem, 0, sizeof machine->sp_imem);
  for (reg = 0u; reg < 32u; reg++) {
    uint32_t byte;
    for (byte = 0u; byte < 16u; byte++) {
      machine->rsp.vector[reg][byte] =
          (uint8_t)(reg * 13u + byte * 7u + 3u);
    }
  }
  for (reg = 0u; reg < 8u; reg++) {
    machine->rsp.accumulator[reg] = UINT64_C(0x123456789abc) + reg;
  }
  machine->rsp.sp_status = UINT32_C(1);
}

static bool fetched_vrcph_returns_previous_high_and_latches_input(
    rsp_divider_fixture *fixture) {
  const uint32_t instruction =
      encode_vector(0u, 2u, 1u, 3u, UINT32_C(0x32));
  t64_machine *machine = fixture->machine;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(1)};
  t64_stop_result result = {0u, 0u};
  uint64_t accumulator_before[8];
  uint32_t lane;

  prepare_machine(machine);
  write_lane(machine->rsp.vector[2], 0u, UINT16_C(0x1234));
  write_lane(machine->rsp.vector[3], 1u, UINT16_C(0xfeed));
  machine->rsp.divider_input = UINT32_C(0x5678);
  machine->rsp.divider_result = UINT32_C(0x89abcdef);
  machine->rsp.divider_high_valid = false;
  (void)memcpy(accumulator_before, machine->rsp.accumulator,
               sizeof accumulator_before);
  if (instruction != UINT32_C(0x4a0208f2)) {
    (void)fprintf(stderr, "SETUP: VRCPH raw-word anchor changed\n");
    return false;
  }
  machine->sp_imem[0] = (uint8_t)(instruction >> 24);
  machine->sp_imem[1] = (uint8_t)(instruction >> 16);
  machine->sp_imem[2] = (uint8_t)(instruction >> 8);
  machine->sp_imem[3] = (uint8_t)instruction;
  if (t64_rsp_start(machine) != T64_STATUS_OK ||
      t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
      result.tick != UINT64_C(1)) {
    (void)fprintf(stderr, "SETUP: VRCPH fetched execution did not advance\n");
    return false;
  }
  if (machine->rsp_observed_outcome != T64_RSP_ATTEMPT_RETIRED ||
      machine->rsp_observed_opcode != instruction ||
      machine->rsp.pc != UINT32_C(4) ||
      machine->rsp.retired_instructions != UINT64_C(1)) {
    (void)fprintf(
        stderr,
        "FAIL: fetched VRCPH word=%08x pc=%03x expected=retired/pc004 "
        "actual=outcome%u/refusal%u/pc%03x retired=%llu\n",
        instruction, machine->rsp_observed_pc,
        (unsigned)machine->rsp_observed_outcome,
        (unsigned)machine->rsp_observed_refusal_reason,
        (unsigned)machine->rsp.pc,
        (unsigned long long)machine->rsp.retired_instructions);
    return false;
  }
  if (read_lane(machine->rsp.vector[3], 1u) != UINT16_C(0x89ab) ||
      machine->rsp.divider_input != UINT32_C(0x1234) ||
      machine->rsp.divider_result != UINT32_C(0x89abcdef) ||
      !machine->rsp.divider_high_valid) {
    (void)fprintf(stderr,
                  "FAIL: VRCPH destination or divider latch mismatch\n");
    return false;
  }
  for (lane = 0u; lane < 8u; lane++) {
    if ((machine->rsp.accumulator[lane] & UINT64_C(0xffff)) !=
            UINT64_C(0x1234) ||
        (machine->rsp.accumulator[lane] & ~UINT64_C(0xffff)) !=
            (accumulator_before[lane] & ~UINT64_C(0xffff))) {
      (void)fprintf(stderr, "FAIL: VRCPH ACC low slice lane %u mismatch\n",
                    lane);
      return false;
    }
  }
  return true;
}

int main(void) {
  rsp_divider_fixture fixture;
  bool passed;

  if (!fixture_create(&fixture)) {
    (void)fprintf(stderr, "SETUP: unable to create RSP divider fixture\n");
    fixture_destroy(&fixture);
    return 2;
  }
  passed = fetched_vrcph_returns_previous_high_and_latches_input(&fixture);
  fixture_destroy(&fixture);
  if (!passed) {
    return 1;
  }
  (void)puts("PASS: fetched VRCPH returns previous high result and latches input");
  return 0;
}
