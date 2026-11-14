#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  t64_host_allocator allocator;
  t64_machine *machine;
} rsp_multiply_fixture;

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

static bool fixture_create(rsp_multiply_fixture *fixture) {
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
  ((uint8_t *)image.data)[0] = UINT8_C(0x12);
  ((uint8_t *)image.data)[1] = UINT8_C(0x34);
  fixture->machine->image_storage = image;
  fixture->machine->image_length = 2u;
  fixture->machine->has_image = true;
  return true;
}

static void fixture_destroy(rsp_multiply_fixture *fixture) {
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

static bool execute_vmulf_red(rsp_multiply_fixture *fixture) {
  static const uint16_t left[8] = {
      0x4000, 0x4000, 0xc000, 0x8000, 0x7fff, 0x8000, 0x0001, 0xffff};
  static const uint16_t right[8] = {
      0x4000, 0xc000, 0x4000, 0x8000, 0x7fff, 0x0001, 0x7fff, 0x8000};
  static const uint16_t expected_vector[8] = {
      0x2000, 0xe000, 0xe000, 0x7fff, 0x7ffe, 0xffff, 0x0001, 0x0001};
  static const uint64_t expected_accumulator[8] = {
      UINT64_C(0x000020008000), UINT64_C(0xffffe0008000),
      UINT64_C(0xffffe0008000), UINT64_C(0x000080008000),
      UINT64_C(0x00007ffe8002), UINT64_C(0xffffffff8000),
      UINT64_C(0x000000017ffe), UINT64_C(0x000000018000)};
  const uint32_t instruction = UINT32_C(0x4a0208c0);
  t64_machine *machine = fixture->machine;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(1)};
  t64_stop_result result = {0u, 0u};
  uint8_t vectors_before[32][16];
  uint8_t vectors_expected[32][16];
  uint64_t accumulator_before[8];
  uint32_t reg;
  uint32_t lane;

  t64_machine_reset_execution_state(machine);
  (void)memset(&machine->rsp, 0, sizeof machine->rsp);
  (void)memset(&machine->cpu, 0, sizeof machine->cpu);
  (void)memset(machine->sp_dmem, 0, sizeof machine->sp_dmem);
  (void)memset(machine->sp_imem, 0, sizeof machine->sp_imem);
  for (reg = 0u; reg < 32u; reg++) {
    for (uint32_t byte = 0u; byte < 16u; byte++) {
      machine->rsp.vector[reg][byte] = (uint8_t)(reg * 23u + byte * 9u + 7u);
    }
  }
  for (lane = 0u; lane < 8u; lane++) {
    write_lane(machine->rsp.vector[1], lane, left[lane]);
    write_lane(machine->rsp.vector[2], lane, right[lane]);
    machine->rsp.accumulator[lane] = UINT64_C(0x123456789abc) + lane;
  }
  machine->rsp.vco = UINT16_C(0xa55a);
  machine->rsp.vcc = UINT16_C(0x963c);
  machine->rsp.vce = UINT8_C(0xd5);
  if (encode_vector(0u, 2u, 1u, 3u, 0u) != instruction) {
    (void)fprintf(stderr, "VMULF fixed raw instruction word mismatch\n");
    return false;
  }
  (void)memcpy(vectors_before, machine->rsp.vector, sizeof vectors_before);
  (void)memcpy(vectors_expected, vectors_before, sizeof vectors_expected);
  (void)memcpy(accumulator_before, machine->rsp.accumulator,
               sizeof accumulator_before);
  for (lane = 0u; lane < 8u; lane++) {
    write_lane(vectors_expected[3], lane, expected_vector[lane]);
  }
  machine->rsp.sp_status = UINT32_C(1);
  machine->sp_imem[0] = (uint8_t)(instruction >> 24);
  machine->sp_imem[1] = (uint8_t)(instruction >> 16);
  machine->sp_imem[2] = (uint8_t)(instruction >> 8);
  machine->sp_imem[3] = (uint8_t)instruction;
  if (t64_rsp_start(machine) != T64_STATUS_OK ||
      t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
      result.tick != UINT64_C(1)) {
    (void)fprintf(stderr, "VMULF fetched setup/advance failure\n");
    return false;
  }
  if (machine->rsp_observed_outcome != T64_RSP_ATTEMPT_RETIRED ||
      machine->rsp_observed_opcode != instruction || machine->rsp.pc != 4u ||
      machine->rsp.retired_instructions != UINT64_C(1)) {
    (void)fprintf(stderr,
                  "VMULF fetched opcode assertion failed: expected raw "
                  "0x%08x at PC 0; outcome=%u reason=%u actual=%08x pc=%03x "
                  "retired=%llu\n",
                  instruction, (unsigned)machine->rsp_observed_outcome,
                  (unsigned)machine->rsp_observed_refusal_reason,
                  machine->rsp_observed_opcode, (unsigned)machine->rsp.pc,
                  (unsigned long long)machine->rsp.retired_instructions);
    return false;
  }
  for (lane = 0u; lane < 8u; lane++) {
    if (read_lane(machine->rsp.vector[3], lane) != expected_vector[lane] ||
        machine->rsp.accumulator[lane] != expected_accumulator[lane]) {
      (void)fprintf(stderr,
                    "VMULF lane %u mismatch: vd=%04x ACC=%012llx\n", lane,
                    read_lane(machine->rsp.vector[3], lane),
                    (unsigned long long)machine->rsp.accumulator[lane]);
      return false;
    }
  }
  (void)accumulator_before;
  (void)vectors_before;
  (void)vectors_expected;
  return machine->rsp.vco == UINT16_C(0xa55a) &&
         machine->rsp.vcc == UINT16_C(0x963c) &&
         machine->rsp.vce == UINT8_C(0xd5);
}

int main(void) {
  rsp_multiply_fixture fixture;
  bool passed;

  if (!fixture_create(&fixture)) {
    (void)fprintf(stderr, "VMULF fixture creation failed\n");
    fixture_destroy(&fixture);
    return EXIT_FAILURE;
  }
  passed = execute_vmulf_red(&fixture);
  fixture_destroy(&fixture);
  return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
