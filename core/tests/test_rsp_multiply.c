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

static void initialize_multiply_case(rsp_multiply_fixture *fixture,
                                     uint32_t source_a_register,
                                     uint32_t source_b_register,
                                     const uint16_t source_a[8],
                                     const uint16_t source_b[8],
                                     const uint64_t accumulator[8],
                                     uint8_t vectors[32][16]) {
  t64_machine *machine = fixture->machine;
  uint32_t reg;
  uint32_t lane;

  t64_machine_reset_execution_state(machine);
  (void)memset(&machine->rsp, 0, sizeof machine->rsp);
  (void)memset(&machine->cpu, 0, sizeof machine->cpu);
  (void)memset(machine->sp_dmem, 0, sizeof machine->sp_dmem);
  (void)memset(machine->sp_imem, 0, sizeof machine->sp_imem);
  for (reg = 0u; reg < 32u; reg++) {
    for (uint32_t byte = 0u; byte < 16u; byte++) {
      machine->rsp.vector[reg][byte] =
          (uint8_t)(reg * 23u + byte * 9u + 7u);
    }
  }
  for (lane = 0u; lane < 8u; lane++) {
    write_lane(machine->rsp.vector[source_a_register], lane, source_a[lane]);
    write_lane(machine->rsp.vector[source_b_register], lane, source_b[lane]);
    machine->rsp.accumulator[lane] = accumulator[lane];
  }
  machine->rsp.vco = UINT16_C(0xa55a);
  machine->rsp.vcc = UINT16_C(0x963c);
  machine->rsp.vce = UINT8_C(0xd5);
  (void)memcpy(vectors, machine->rsp.vector, sizeof machine->rsp.vector);
}

static bool execute_multiply_case(
    rsp_multiply_fixture *fixture, const char *label, uint32_t function,
    uint32_t element, uint32_t source_a_register,
    uint32_t source_b_register, uint32_t destination,
    const uint16_t source_a[8], const uint16_t source_b[8],
    const uint64_t accumulator_before[8], const uint16_t expected_vector[8],
    const uint64_t expected_accumulator[8]) {
  t64_machine *machine = fixture->machine;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(1)};
  t64_stop_result result = {0u, 0u};
  uint8_t vectors_expected[32][16];
  uint32_t instruction = encode_vector(element, source_b_register,
                                       source_a_register, destination,
                                       function);
  uint32_t lane;

  initialize_multiply_case(fixture, source_a_register, source_b_register,
                           source_a, source_b, accumulator_before,
                           vectors_expected);
  for (lane = 0u; lane < 8u; lane++) {
    write_lane(vectors_expected[destination], lane, expected_vector[lane]);
  }
  machine->rsp.sp_status = UINT32_C(1);
  machine->sp_imem[0] = (uint8_t)(instruction >> 24);
  machine->sp_imem[1] = (uint8_t)(instruction >> 16);
  machine->sp_imem[2] = (uint8_t)(instruction >> 8);
  machine->sp_imem[3] = (uint8_t)instruction;
  if (t64_rsp_start(machine) != T64_STATUS_OK ||
      t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
      result.tick != UINT64_C(1)) {
    (void)fprintf(stderr, "%s fetched setup/advance failure\n", label);
    return false;
  }
  if (machine->rsp_observed_outcome != T64_RSP_ATTEMPT_RETIRED ||
      machine->rsp_observed_opcode != instruction || machine->rsp.pc != 4u ||
      machine->rsp.retired_instructions != UINT64_C(1)) {
    (void)fprintf(stderr,
                  "%s fetched opcode assertion failed: raw=%08x outcome=%u "
                  "reason=%u actual=%08x pc=%03x retired=%llu\n",
                  label, instruction,
                  (unsigned)machine->rsp_observed_outcome,
                  (unsigned)machine->rsp_observed_refusal_reason,
                  machine->rsp_observed_opcode, (unsigned)machine->rsp.pc,
                  (unsigned long long)machine->rsp.retired_instructions);
    return false;
  }
  if (memcmp(machine->rsp.vector, vectors_expected,
             sizeof vectors_expected) != 0) {
    (void)fprintf(stderr, "%s full vector bank mismatch\n", label);
    for (lane = 0u; lane < 8u; lane++) {
      uint16_t expected = read_lane(vectors_expected[destination], lane);
      uint16_t actual = read_lane(machine->rsp.vector[destination], lane);
      if (expected != actual) {
        (void)fprintf(stderr, "  lane %u expected=%04x actual=%04x\n", lane,
                      expected, actual);
      }
    }
    return false;
  }
  for (lane = 0u; lane < 8u; lane++) {
    if (machine->rsp.accumulator[lane] != expected_accumulator[lane]) {
      (void)fprintf(stderr,
                    "%s lane %u accumulator mismatch: expected=%012llx "
                    "actual=%012llx\n",
                    label, lane,
                    (unsigned long long)expected_accumulator[lane],
                    (unsigned long long)machine->rsp.accumulator[lane]);
      return false;
    }
  }
  if (machine->rsp.vco != UINT16_C(0xa55a) ||
      machine->rsp.vcc != UINT16_C(0x963c) ||
      machine->rsp.vce != UINT8_C(0xd5)) {
    (void)fprintf(stderr, "%s changed vector flags\n", label);
    return false;
  }
  return true;
}

static bool execute_multiply_matrix(rsp_multiply_fixture *fixture) {
  /* Fixed raw lanes: signed products include -32768 * -32768, negative
   * products, and an unsigned high-half product. These expected arrays are
   * literal bit patterns derived from Appendix A, not a runtime oracle. */
  static const uint16_t source_a[8] = {
      0x8000, 0x7fff, 0x4000, 0xc000, 0x0001, 0xffff, 0x1234, 0x8000};
  static const uint16_t source_b[8] = {
      0x8000, 0x7fff, 0xc000, 0x4000, 0xffff, 0x0002, 0x00ff, 0x7fff};
  static const uint64_t zero_accumulator[8] = {0u};
  static const uint16_t vmulu_result[8] = {
      0x8000, 0x7ffe, 0x0000, 0x0000, 0x0000, 0x0000, 0x0024, 0x0000};
  static const uint64_t vmulf_accumulator[8] = {
      UINT64_C(0x000080008000), UINT64_C(0x00007ffe8002),
      UINT64_C(0xffffe0008000), UINT64_C(0xffffe0008000),
      UINT64_C(0x000000007ffe), UINT64_C(0x000000007ffc),
      UINT64_C(0x00000024c398), UINT64_C(0xffff80018000)};
  static const uint64_t vmudl_accumulator[8] = {
      UINT64_C(0x000000004000), UINT64_C(0x000000003fff),
      UINT64_C(0x000000003000), UINT64_C(0x000000003000),
      UINT64_C(0x000000000000), UINT64_C(0x000000000001),
      UINT64_C(0x000000000012), UINT64_C(0x000000003fff)};
  static const uint16_t vmudl_result[8] = {
      0x4000, 0x3fff, 0x3000, 0x3000, 0x0000, 0x0001, 0x0012, 0x3fff};
  static const uint16_t vmudl_boundary_left[8] = {
      0xffff, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
  static const uint16_t vmudl_boundary_right[8] = {
      0xffff, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
  static const uint64_t vmudl_boundary_before[8] = {
      UINT64_C(0xffff00000000), 0u, 0u, 0u, 0u, 0u, 0u, 0u};
  static const uint16_t vmudl_boundary_result[8] = {
      0xfffe, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
  static const uint64_t vmudl_boundary_accumulator[8] = {
      UINT64_C(0xfffffffffffe), 0u, 0u, 0u, 0u, 0u, 0u, 0u};
  static const uint16_t vmudm_result[8] = {
      0x7fff, 0x3fff, 0x3000, 0x7fff, 0x0000, 0x7fff, 0x0012, 0x7fff};
  static const uint64_t vmudm_accumulator[8] = {
      UINT64_C(0x0000c0000000), UINT64_C(0x00003fff0001),
      UINT64_C(0x000030000000), UINT64_C(0x0000f0000000),
      UINT64_C(0x00000000ffff), UINT64_C(0x0000fffffffe),
      UINT64_C(0x0000001221cc), UINT64_C(0x0000c0008000)};
  static const uint16_t vmudn_result[8] = {
      0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff};
  static const uint64_t vmudn_accumulator[8] = {
      UINT64_C(0x0000c0000000), UINT64_C(0x00003fff0001),
      UINT64_C(0x0000f0000000), UINT64_C(0x000030000000),
      UINT64_C(0x0000ffffffff), UINT64_C(0x00000001fffe),
      UINT64_C(0x0000001221cc), UINT64_C(0x00003fff8000)};
  static const uint16_t vmudh_result[8] = {
      0x4000, 0x3fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x0012, 0x7fff};
  static const uint64_t vmudh_accumulator[8] = {
      UINT64_C(0x000040000000), UINT64_C(0x00003fff0000),
      UINT64_C(0x0000f0000000), UINT64_C(0x0000f0000000),
      UINT64_C(0x0000ffff0000), UINT64_C(0x0000ffff0000),
      UINT64_C(0x000000120000), UINT64_C(0x0000c0000000)};
  static const uint16_t vmulf_result[8] = {
      0x7fff, 0x7ffe, 0xe000, 0xe000, 0x0000, 0x0000, 0x0024, 0x8001};
  static const uint64_t accumulate_accumulator_before[8] = {
      UINT64_C(0x00000000ffff), UINT64_C(0x000000010000),
      UINT64_C(0x00007fffffff), UINT64_C(0xffff80000001),
      UINT64_C(0x000000120000), UINT64_C(0x000000300001),
      UINT64_C(0x0000ffffffff), UINT64_C(0xffffffffffff)};
  static const uint16_t vmacf_result[8] = {
      0x8000, 0x7fff, 0x8000, 0x8000, 0x0010, 0x002c, 0x7fff, 0x8000};
  static const uint16_t vmacu_result[8] = {
      0x0000, 0xffff, 0x0000, 0x0000, 0x0010, 0x002c, 0xffff, 0x0000};
  static const uint64_t vmac_accumulator[8] = {
      UINT64_C(0x80000000ffff), UINT64_C(0x7ffe00030000),
      UINT64_C(0xe0007fffffff), UINT64_C(0xdfff80000001),
      UINT64_C(0x000000100000), UINT64_C(0x0000002c0001),
      UINT64_C(0x00254397ffff), UINT64_C(0x8000ffffffff)};
  static const uint16_t vmadl_result[8] = {
      0x7fff, 0x7fff, 0x7fff, 0x8000, 0x7fff, 0x7fff, 0x0011, 0x8000};
  static const uint64_t vmadl_accumulator[8] = {
      UINT64_C(0x000000013fff), UINT64_C(0x000000013fff),
      UINT64_C(0x000080002fff), UINT64_C(0xffff80003001),
      UINT64_C(0x000000120000), UINT64_C(0x000000300002),
      UINT64_C(0x000000000011), UINT64_C(0xffff00003ffe)};
  static const uint16_t vmadm_result[8] = {
      0x7fff, 0x4000, 0x7fff, 0x8000, 0x0012, 0x002f, 0x0012, 0xc000};
  static const uint64_t vmadm_accumulator[8] = {
      UINT64_C(0x0000c000ffff), UINT64_C(0x000040000001),
      UINT64_C(0x0000afffffff), UINT64_C(0xffff70000001),
      UINT64_C(0x00000012ffff), UINT64_C(0x0000002fffff),
      UINT64_C(0x0000001221cb), UINT64_C(0xffffc0007fff)};
  static const uint16_t vmadn_result[8] = {
      0x7fff, 0x7fff, 0x7fff, 0x8000, 0x7fff, 0x7fff, 0x7fff, 0x8000};
  static const uint64_t vmadn_accumulator[8] = {
      UINT64_C(0x0000c000ffff), UINT64_C(0x000040000001),
      UINT64_C(0x00006fffffff), UINT64_C(0xffffb0000001),
      UINT64_C(0x00000011ffff), UINT64_C(0x00000031ffff),
      UINT64_C(0x0000001221cb), UINT64_C(0xffff3fff7fff)};
  static const uint16_t vmadh_result[8] = {
      0x4000, 0x4000, 0x6fff, 0x8000, 0x0011, 0x002f, 0x0011, 0xbfff};
  static const uint64_t vmadh_accumulator[8] = {
      UINT64_C(0x00004000ffff), UINT64_C(0x000040000000),
      UINT64_C(0x00006fffffff), UINT64_C(0xffff70000001),
      UINT64_C(0x000000110000), UINT64_C(0x0000002f0001),
      UINT64_C(0x00000011ffff), UINT64_C(0xffffbfffffff)};

  return execute_multiply_case(fixture, "VMULF", 0x00u, 0u, 1u, 2u, 3u,
                               source_a, source_b, zero_accumulator,
                               vmulf_result, vmulf_accumulator) &&
         execute_multiply_case(fixture, "VMULU", 0x01u, 0u, 1u, 2u, 3u,
                               source_a, source_b, zero_accumulator,
                               vmulu_result, vmulf_accumulator) &&
         execute_multiply_case(fixture, "VMUDL", 0x04u, 0u, 1u, 2u, 3u,
                               source_a, source_b, zero_accumulator,
                               vmudl_result, vmudl_accumulator) &&
         execute_multiply_case(
             fixture, "VMUDL signed high partial", 0x04u, 0u, 1u, 2u, 3u,
             vmudl_boundary_left, vmudl_boundary_right,
             vmudl_boundary_before, vmudl_boundary_result,
             vmudl_boundary_accumulator) &&
         execute_multiply_case(fixture, "VMUDM", 0x05u, 0u, 1u, 2u, 3u,
                               source_a, source_b, zero_accumulator,
                               vmudm_result, vmudm_accumulator) &&
         execute_multiply_case(fixture, "VMUDN", 0x06u, 0u, 1u, 2u, 3u,
                               source_a, source_b, zero_accumulator,
                               vmudn_result, vmudn_accumulator) &&
         execute_multiply_case(fixture, "VMUDH", 0x07u, 0u, 1u, 2u, 3u,
                               source_a, source_b, zero_accumulator,
                               vmudh_result, vmudh_accumulator) &&
         execute_multiply_case(fixture, "VMACF", 0x08u, 0u, 1u, 2u, 3u,
                               source_a, source_b,
                               accumulate_accumulator_before, vmacf_result,
                               vmac_accumulator) &&
         execute_multiply_case(fixture, "VMACU", 0x09u, 0u, 1u, 2u, 3u,
                               source_a, source_b,
                               accumulate_accumulator_before, vmacu_result,
                               vmac_accumulator) &&
         execute_multiply_case(fixture, "VMADL", 0x0cu, 0u, 1u, 2u, 3u,
                               source_a, source_b,
                               accumulate_accumulator_before, vmadl_result,
                               vmadl_accumulator) &&
         execute_multiply_case(fixture, "VMADM", 0x0du, 0u, 1u, 2u, 3u,
                               source_a, source_b,
                               accumulate_accumulator_before, vmadm_result,
                               vmadm_accumulator) &&
         execute_multiply_case(fixture, "VMADN", 0x0eu, 0u, 1u, 2u, 3u,
                               source_a, source_b,
                               accumulate_accumulator_before, vmadn_result,
                               vmadn_accumulator) &&
         execute_multiply_case(fixture, "VMADH", 0x0fu, 0u, 1u, 2u, 3u,
                               source_a, source_b,
                               accumulate_accumulator_before, vmadh_result,
                               vmadh_accumulator) &&
         execute_multiply_case(fixture, "VMULF vd=vs", 0x00u, 0u, 1u, 2u,
                               1u, source_a, source_b, zero_accumulator,
                               vmulf_result, vmulf_accumulator) &&
         execute_multiply_case(fixture, "VMULF vd=vt", 0x00u, 0u, 1u, 2u,
                               2u, source_a, source_b, zero_accumulator,
                               vmulf_result, vmulf_accumulator);
}

static bool execute_rounding_and_sar_matrix(
    rsp_multiply_fixture *fixture) {
  /* Rounding arrays include both signs, the ACC21 oddification boundary and
   * independent high/mid/low accumulator slice sentinels. */
  static const uint16_t zero_vectors[8] = {0u};
  static const uint16_t round_vector[8] = {
      0x0001, 0xffff, 0x0002, 0x8000, 0x7fff, 0x1234, 0xff00, 0x0100};
  static const uint64_t round_accumulator_before[8] = {
      UINT64_C(0x000000000001), UINT64_C(0xffffffffffff),
      UINT64_C(0x000000200000), UINT64_C(0xffff00000000),
      UINT64_C(0x000000210000), UINT64_C(0xffffffffff00),
      UINT64_C(0x000001000000), UINT64_C(0x800000000000)};
  static const uint16_t vrndp_result[8] = {
      0x0001, 0xffff, 0x0022, 0x8000, 0x7fff, 0xffff, 0x0000, 0x8000};
  static const uint64_t vrndp_accumulator[8] = {
      UINT64_C(0x000000010001), UINT64_C(0xffffffffffff),
      UINT64_C(0x000000220000), UINT64_C(0xffff00000000),
      UINT64_C(0x000080200000), UINT64_C(0xffffffffff00),
      UINT64_C(0x000000000000), UINT64_C(0x800000000000)};
  static const uint16_t vrndn_result[8] = {
      0x0000, 0xfffe, 0x0020, 0x8000, 0x0021, 0x1233, 0x0100, 0x8000};
  static const uint64_t vrndn_accumulator[8] = {
      UINT64_C(0x000000000001), UINT64_C(0xfffffffeffff),
      UINT64_C(0x000000200000), UINT64_C(0xfffe80000000),
      UINT64_C(0x000000210000), UINT64_C(0x00001233ff00),
      UINT64_C(0x000001000000), UINT64_C(0x800001000000)};
  static const uint16_t vrndp_even_result[8] = {
      0x0000, 0xffff, 0x0020, 0x8000, 0x0021, 0xffff, 0x00ff, 0x8000};
  static const uint64_t vrndp_even_accumulator[8] = {
      UINT64_C(0x000000000002), UINT64_C(0xffffffffffff),
      UINT64_C(0x000000200002), UINT64_C(0xffff00000000),
      UINT64_C(0x000000217fff), UINT64_C(0xffffffffff00),
      UINT64_C(0x000000ffff00), UINT64_C(0x800000000000)};
  static const uint16_t vrndn_even_result[8] = {
      0x0000, 0xffff, 0x0020, 0x8000, 0x0021, 0x0000, 0x0100, 0x8000};
  static const uint64_t vrndn_even_accumulator[8] = {
      UINT64_C(0x000000000001), UINT64_C(0xfffffffffffe),
      UINT64_C(0x000000200000), UINT64_C(0xfffeffff8000),
      UINT64_C(0x000000210000), UINT64_C(0x000000001134),
      UINT64_C(0x000001000000), UINT64_C(0x800000000100)};
  static const uint16_t vmulq_result[8] = {
      0x0000, 0x0000, 0x0010, 0x0010, 0x0000, 0x0000, 0x10e0, 0xc010};
  static const uint64_t vmulq_accumulator_before[8] = {
      UINT64_C(0x0123456789ab), UINT64_C(0xfedcba987654),
      UINT64_C(0x0000ffff0001), UINT64_C(0xffff80001234),
      UINT64_C(0x123456789abc), UINT64_C(0x0000aaaa0001),
      UINT64_C(0xffff0000cafe), UINT64_C(0x800000000001)};
  static const uint64_t vmulq_accumulator[8] = {
      UINT64_C(0x0000000089ab), UINT64_C(0x000000017654),
      UINT64_C(0x000000200001), UINT64_C(0x000000201234),
      UINT64_C(0x0000001f9abc), UINT64_C(0x0000001e0001),
      UINT64_C(0x000021cccafe), UINT64_C(0xffff80200001)};
  static const uint16_t vmacq_result[8] = {
      0xfff0, 0xfff0, 0x0010, 0x8010, 0x0010, 0xfff0, 0x0070, 0x8000};
  static const uint64_t vmacq_accumulator[8] = {
      UINT64_C(0xffffffe00001), UINT64_C(0xffffffffffff),
      UINT64_C(0x000000200000), UINT64_C(0xffff00200000),
      UINT64_C(0x000000210000), UINT64_C(0xffffffffff00),
      UINT64_C(0x000000e00000), UINT64_C(0x800000200000)};
  static const uint64_t sar_accumulator_before[8] = {
      UINT64_C(0x0123456789ab), UINT64_C(0xfedcba987654),
      UINT64_C(0x8000ffff0001), UINT64_C(0x0000abcd1234),
      UINT64_C(0x7fff8000ffff), UINT64_C(0xffff00008000),
      UINT64_C(0x5555aaaa5555), UINT64_C(0xaaaa5555aaaa)};
  static const uint16_t sar_source[8] = {
      0x8000, 0x7fff, 0x0001, 0xffff, 0x1234, 0xabcd, 0x00ff, 0xff00};
  static const uint16_t sar_upper_result[8] = {
      0x0123, 0xfedc, 0x8000, 0x0000, 0x7fff, 0xffff, 0x5555, 0xaaaa};
  static const uint64_t sar_upper_accumulator[8] = {
      UINT64_C(0x8000456789ab), UINT64_C(0x7fffba987654),
      UINT64_C(0x0001ffff0001), UINT64_C(0xffffabcd1234),
      UINT64_C(0x12348000ffff), UINT64_C(0xabcd00008000),
      UINT64_C(0x00ffaaaa5555), UINT64_C(0xff005555aaaa)};
  static const uint16_t sar_middle_result[8] = {
      0x4567, 0xba98, 0xffff, 0xabcd, 0x8000, 0x0000, 0xaaaa, 0x5555};
  static const uint64_t sar_middle_accumulator[8] = {
      UINT64_C(0x0123800089ab), UINT64_C(0xfedc7fff7654),
      UINT64_C(0x800000010001), UINT64_C(0x0000ffff1234),
      UINT64_C(0x7fff1234ffff), UINT64_C(0xffffabcd8000),
      UINT64_C(0x555500ff5555), UINT64_C(0xaaaaff00aaaa)};
  static const uint16_t sar_low_result[8] = {
      0x89ab, 0x7654, 0x0001, 0x1234, 0xffff, 0x8000, 0x5555, 0xaaaa};
  static const uint64_t sar_low_accumulator[8] = {
      UINT64_C(0x012345678000), UINT64_C(0xfedcba987fff),
      UINT64_C(0x8000ffff0001), UINT64_C(0x0000abcdffff),
      UINT64_C(0x7fff80001234), UINT64_C(0xffff0000abcd),
      UINT64_C(0x5555aaaa00ff), UINT64_C(0xaaaa5555ff00)};
  static const uint16_t multiply_source_a[8] = {
      0x8000, 0x7fff, 0x4000, 0xc000, 0x0001, 0xffff, 0x1234, 0x8000};
  static const uint16_t multiply_source_b[8] = {
      0x8000, 0x7fff, 0xc000, 0x4000, 0xffff, 0x0002, 0x00ff, 0x7fff};
  static const uint16_t ignored_vmacq_a[8] = {
      0x7fff, 0x8000, 0xffff, 0x0001, 0x1234, 0xabcd, 0x00ff, 0xff00};
  static const uint16_t ignored_vmacq_b[8] = {
      0x8000, 0x7fff, 0x0001, 0xffff, 0xabcd, 0x1234, 0xff00, 0x00ff};

  if (!execute_multiply_case(fixture, "VRNDP", 0x02u, 0u, 1u, 2u, 3u,
                             zero_vectors, round_vector,
                             round_accumulator_before, vrndp_result,
                             vrndp_accumulator) ||
      !execute_multiply_case(fixture, "VRNDN", 0x0au, 0u, 3u, 2u, 4u,
                             zero_vectors, round_vector,
                             round_accumulator_before, vrndn_result,
                             vrndn_accumulator) ||
      !execute_multiply_case(fixture, "VRNDP even vs field", 0x02u, 0u,
                             4u, 2u, 3u, zero_vectors, round_vector,
                             round_accumulator_before, vrndp_even_result,
                             vrndp_even_accumulator) ||
      !execute_multiply_case(fixture, "VRNDN even vs field", 0x0au, 0u,
                             4u, 2u, 3u, zero_vectors, round_vector,
                             round_accumulator_before, vrndn_even_result,
                             vrndn_even_accumulator) ||
      !execute_multiply_case(fixture, "VMULQ", 0x03u, 0u, 1u, 2u, 3u,
                             multiply_source_a, multiply_source_b,
                             vmulq_accumulator_before, vmulq_result,
                             vmulq_accumulator) ||
      !execute_multiply_case(fixture, "VMACQ", 0x0bu, 0u, 1u, 2u, 3u,
                             ignored_vmacq_a, ignored_vmacq_b,
                             round_accumulator_before, vmacq_result,
                             vmacq_accumulator)) {
    return false;
  }
  return execute_multiply_case(fixture, "VSAR upper", 0x1du, 0u, 1u, 2u,
                               3u, sar_source, zero_vectors,
                               sar_accumulator_before, sar_upper_result,
                               sar_upper_accumulator) &&
         execute_multiply_case(fixture, "VSAR middle", 0x1du, 1u, 1u, 2u,
                               3u, sar_source, zero_vectors,
                               sar_accumulator_before, sar_middle_result,
                               sar_middle_accumulator) &&
         execute_multiply_case(fixture, "VSAR low", 0x1du, 2u, 1u, 2u, 3u,
                               sar_source, zero_vectors,
                               sar_accumulator_before, sar_low_result,
                               sar_low_accumulator);
}

static bool execute_vmulq_rounding_anchor(rsp_multiply_fixture *fixture) {
  static const uint16_t source_a[8] = {
      0x8000, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
  static const uint16_t source_b[8] = {1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
  static const uint64_t accumulator_before[8] = {
      UINT64_C(0x123456789abc), UINT64_C(0x000000000001),
      UINT64_C(0x000000000002), UINT64_C(0x000000000003),
      UINT64_C(0x000000000004), UINT64_C(0x000000000005),
      UINT64_C(0x000000000006), UINT64_C(0x000000000007)};
  static const uint16_t expected_vector[8] = {
      0xc010, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
  static const uint64_t expected_accumulator[8] = {
      UINT64_C(0xffff80209abc), UINT64_C(0x000000000001),
      UINT64_C(0x000000000002), UINT64_C(0x000000000003),
      UINT64_C(0x000000000004), UINT64_C(0x000000000005),
      UINT64_C(0x000000000006), UINT64_C(0x000000000007)};

  if (encode_vector(0u, 2u, 1u, 3u, 0x03u) != UINT32_C(0x4a0208c3)) {
    (void)fprintf(stderr, "VMULQ negative rounding raw word mismatch\n");
    return false;
  }
  return execute_multiply_case(
      fixture, "VMULQ negative rounding anchor", 0x03u, 0u, 1u, 2u, 3u,
      source_a, source_b, accumulator_before, expected_vector,
      expected_accumulator);
}

static bool execute_multiply_selector_matrix(
    rsp_multiply_fixture *fixture) {
  /* The table records selected VT lane values from Table 3-3 directly; e=1
   * is checked separately as a pre-effect refusal. */
  static const uint16_t source_a[8] = {
      0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff};
  static const uint16_t source_b[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
  static const uint16_t selected_output[16][8] = {
      {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u},
      {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u},
      {1u, 1u, 3u, 3u, 5u, 5u, 7u, 7u},
      {2u, 2u, 4u, 4u, 6u, 6u, 8u, 8u},
      {1u, 1u, 1u, 1u, 5u, 5u, 5u, 5u},
      {2u, 2u, 2u, 2u, 6u, 6u, 6u, 6u},
      {3u, 3u, 3u, 3u, 7u, 7u, 7u, 7u},
      {4u, 4u, 4u, 4u, 8u, 8u, 8u, 8u},
      {1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u},
      {2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u},
      {3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u},
      {4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u},
      {5u, 5u, 5u, 5u, 5u, 5u, 5u, 5u},
      {6u, 6u, 6u, 6u, 6u, 6u, 6u, 6u},
      {7u, 7u, 7u, 7u, 7u, 7u, 7u, 7u},
      {8u, 8u, 8u, 8u, 8u, 8u, 8u, 8u}};
  static const uint64_t zero_accumulator[8] = {0u};
  static const uint64_t accumulator_for_result[9] = {
      UINT64_C(0), UINT64_C(0x000000017ffe), UINT64_C(0x000000027ffc),
      UINT64_C(0x000000037ffa), UINT64_C(0x000000047ff8),
      UINT64_C(0x000000057ff6), UINT64_C(0x000000067ff4),
      UINT64_C(0x000000077ff2), UINT64_C(0x000000087ff0)};
  uint16_t expected_vector[8];
  uint64_t expected_accumulator[8];
  uint32_t element;
  uint32_t lane;

  for (element = 0u; element < 16u; element++) {
    if (element == 1u) {
      continue;
    }
    for (lane = 0u; lane < 8u; lane++) {
      expected_vector[lane] = selected_output[element][lane];
      expected_accumulator[lane] =
          accumulator_for_result[selected_output[element][lane]];
    }
    if (!execute_multiply_case(fixture, "VMULF selector", 0x00u, element,
                               1u, 2u, 3u, source_a, source_b,
                               zero_accumulator, expected_vector,
                               expected_accumulator)) {
      return false;
    }
  }
  return true;
}

static bool execute_repeated_vmacf(rsp_multiply_fixture *fixture) {
  static const uint16_t one[8] = {1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u};
  static const uint64_t accumulator_before[8] = {
      UINT64_C(0x00000000ffff), UINT64_C(0x00007fffffff),
      UINT64_C(0xffffffffffff), UINT64_C(0x123456789abc),
      UINT64_C(0x000000000000), UINT64_C(0x000000ffff00),
      UINT64_C(0x800000000000), UINT64_C(0x000000000001)};
  static const uint16_t expected_vector[8] = {
      0x0004, 0x7fff, 0x0003, 0x7fff, 0x0004, 0x0103, 0x8000, 0x0004};
  static const uint64_t expected_accumulator[8] = {
      UINT64_C(0x00000004ffff), UINT64_C(0x00008003ffff),
      UINT64_C(0x00000003ffff), UINT64_C(0x1234567c9abc),
      UINT64_C(0x000000040000), UINT64_C(0x00000103ff00),
      UINT64_C(0x800000040000), UINT64_C(0x000000040001)};
  t64_machine *machine = fixture->machine;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(2)};
  t64_stop_result result = {0u, 0u};
  uint8_t vectors_expected[32][16];
  uint32_t instruction = encode_vector(0u, 2u, 1u, 3u, 0x08u);
  uint32_t lane;

  initialize_multiply_case(fixture, 1u, 2u, one, one, accumulator_before,
                           vectors_expected);
  for (lane = 0u; lane < 8u; lane++) {
    write_lane(vectors_expected[3], lane, expected_vector[lane]);
  }
  machine->rsp.sp_status = UINT32_C(1);
  machine->sp_imem[0] = (uint8_t)(instruction >> 24);
  machine->sp_imem[1] = (uint8_t)(instruction >> 16);
  machine->sp_imem[2] = (uint8_t)(instruction >> 8);
  machine->sp_imem[3] = (uint8_t)instruction;
  machine->sp_imem[4] = (uint8_t)(instruction >> 24);
  machine->sp_imem[5] = (uint8_t)(instruction >> 16);
  machine->sp_imem[6] = (uint8_t)(instruction >> 8);
  machine->sp_imem[7] = (uint8_t)instruction;
  if (t64_rsp_start(machine) != T64_STATUS_OK ||
      t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
      result.tick != UINT64_C(2) || machine->rsp.pc != 8u ||
      machine->rsp.retired_instructions != UINT64_C(2) ||
      machine->rsp_observed_outcome != T64_RSP_ATTEMPT_RETIRED ||
      machine->rsp_observed_opcode != instruction) {
    (void)fprintf(stderr, "repeated VMACF fetched sequence did not retire\n");
    return false;
  }
  if (memcmp(machine->rsp.vector, vectors_expected,
             sizeof vectors_expected) != 0) {
    (void)fprintf(stderr, "repeated VMACF full vector bank mismatch\n");
    return false;
  }
  for (lane = 0u; lane < 8u; lane++) {
    if (machine->rsp.accumulator[lane] != expected_accumulator[lane]) {
      (void)fprintf(stderr,
                    "repeated VMACF lane %u accumulator mismatch: "
                    "expected=%012llx actual=%012llx\n",
                    lane, (unsigned long long)expected_accumulator[lane],
                    (unsigned long long)machine->rsp.accumulator[lane]);
      return false;
    }
  }
  return machine->rsp.vco == UINT16_C(0xa55a) &&
         machine->rsp.vcc == UINT16_C(0x963c) &&
         machine->rsp.vce == UINT8_C(0xd5);
}

static bool execute_invalid_multiply_selector(
    rsp_multiply_fixture *fixture, uint32_t function, uint32_t element,
    const char *label) {
  static const uint16_t source_a[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
  static const uint16_t source_b[8] = {8u, 7u, 6u, 5u, 4u, 3u, 2u, 1u};
  static const uint64_t accumulator[8] = {
      UINT64_C(0x123456789abc), UINT64_C(0xfedcba987654),
      UINT64_C(0x800000000001), UINT64_C(0x0000ffffffff),
      UINT64_C(0x5555aaaa5555), UINT64_C(0xaaaa5555aaaa),
      UINT64_C(0x7fff8000ffff), UINT64_C(0xffff00008000)};
  t64_machine *machine = fixture->machine;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(1)};
  t64_stop_result result = {0u, 0u};
  uint8_t vectors_before[32][16];
  uint64_t accumulator_before[8];
  uint32_t instruction =
      encode_vector(element, 2u, 1u, 3u, function);

  initialize_multiply_case(fixture, 1u, 2u, source_a, source_b, accumulator,
                           vectors_before);
  (void)memcpy(accumulator_before, machine->rsp.accumulator,
               sizeof accumulator_before);
  machine->rsp.vco = UINT16_C(0xa5a5);
  machine->rsp.sp_status = UINT32_C(1);
  machine->sp_imem[0] = (uint8_t)(instruction >> 24);
  machine->sp_imem[1] = (uint8_t)(instruction >> 16);
  machine->sp_imem[2] = (uint8_t)(instruction >> 8);
  machine->sp_imem[3] = (uint8_t)instruction;
  if (t64_rsp_start(machine) != T64_STATUS_OK ||
      t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
      result.tick != UINT64_C(1) ||
      machine->rsp_observed_outcome != T64_RSP_ATTEMPT_REFUSED ||
      machine->rsp_observed_refusal_reason !=
          T64_RSP_REFUSAL_UNSUPPORTED_OPCODE ||
      machine->rsp.pc != 0u || machine->rsp.retired_instructions != 0u) {
    (void)fprintf(stderr, "%s invalid selector was not refused\n", label);
    return false;
  }
  return memcmp(machine->rsp.vector, vectors_before,
                sizeof vectors_before) == 0 &&
         memcmp(machine->rsp.accumulator, accumulator_before,
                sizeof accumulator_before) == 0 &&
         machine->rsp.vco == UINT16_C(0xa5a5) &&
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
  passed = execute_vmulf_red(&fixture) && execute_multiply_matrix(&fixture) &&
           execute_rounding_and_sar_matrix(&fixture) &&
           execute_vmulq_rounding_anchor(&fixture) &&
           execute_multiply_selector_matrix(&fixture) &&
           execute_repeated_vmacf(&fixture) &&
           execute_invalid_multiply_selector(&fixture, 0x00u, 1u,
                                             "VMULF e=1") &&
           execute_invalid_multiply_selector(&fixture, 0x1du, 3u,
                                             "VSAR e=3");
  fixture_destroy(&fixture);
  return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
