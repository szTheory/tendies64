#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  t64_host_allocator allocator;
  t64_machine *machine;
} rsp_compare_fixture;

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

static bool fixture_create(rsp_compare_fixture *fixture) {
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

static void fixture_destroy(rsp_compare_fixture *fixture) {
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

static bool vlt_fetched_red(rsp_compare_fixture *fixture) {
  static const uint16_t left[8] = {
      0x8000, 0x0001, 0x7fff, 0x0000, 0x8000, 0x0002, 0xffff, 0x0005};
  static const uint16_t right[8] = {
      0x7fff, 0x0001, 0x8000, 0x0000, 0x8000, 0x0001, 0xffff, 0x0006};
  static const uint16_t expected[8] = {
      0x8000, 0x0001, 0x8000, 0x0000, 0x8000, 0x0001, 0xffff, 0x0005};
  const uint32_t instruction = UINT32_C(0x4a0208e0);
  t64_machine *machine = fixture->machine;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(1)};
  t64_stop_result result = {0u, 0u};
  uint8_t vector_before[32][16];
  uint64_t accumulator_before[8];
  uint32_t lane;

  t64_machine_reset_execution_state(machine);
  (void)memset(&machine->rsp, 0, sizeof machine->rsp);
  (void)memset(&machine->cpu, 0, sizeof machine->cpu);
  (void)memset(machine->sp_dmem, 0, sizeof machine->sp_dmem);
  (void)memset(machine->sp_imem, 0, sizeof machine->sp_imem);
  for (uint32_t reg = 0u; reg < 32u; reg++) {
    for (uint32_t byte = 0u; byte < 16u; byte++) {
      machine->rsp.vector[reg][byte] = (uint8_t)(reg * 13u + byte * 7u + 3u);
    }
  }
  for (lane = 0u; lane < 8u; lane++) {
    write_lane(machine->rsp.vector[1], lane, left[lane]);
    write_lane(machine->rsp.vector[2], lane, right[lane]);
    machine->rsp.accumulator[lane] = UINT64_C(0x123456789abc) + lane;
  }
  machine->rsp.vco = UINT16_C(0x0052);
  machine->rsp.vcc = UINT16_C(0xa55a);
  machine->rsp.vce = UINT8_C(0x10);
  if (encode_vector(0u, 2u, 1u, 3u, 0x20u) != instruction) {
    (void)fprintf(stderr, "VLT fixed raw instruction mismatch\n");
    return false;
  }
  (void)memcpy(vector_before, machine->rsp.vector, sizeof vector_before);
  (void)memcpy(accumulator_before, machine->rsp.accumulator,
               sizeof accumulator_before);
  machine->rsp.sp_status = UINT32_C(1);
  machine->sp_imem[0] = (uint8_t)(instruction >> 24);
  machine->sp_imem[1] = (uint8_t)(instruction >> 16);
  machine->sp_imem[2] = (uint8_t)(instruction >> 8);
  machine->sp_imem[3] = (uint8_t)instruction;
  if (t64_rsp_start(machine) != T64_STATUS_OK ||
      t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
      result.tick != UINT64_C(1)) {
    (void)fprintf(stderr, "VLT fetched setup/advance failure\n");
    return false;
  }
  if (machine->rsp_observed_outcome != T64_RSP_ATTEMPT_RETIRED ||
      machine->rsp_observed_opcode != instruction || machine->rsp.pc != 4u ||
      machine->rsp.retired_instructions != UINT64_C(1)) {
    (void)fprintf(stderr,
                  "VLT fetched raw word 0x%08x did not retire: outcome=%u "
                  "reason=%u actual=%08x pc=%03x retired=%llu\n",
                  instruction, (unsigned)machine->rsp_observed_outcome,
                  (unsigned)machine->rsp_observed_refusal_reason,
                  machine->rsp_observed_opcode, (unsigned)machine->rsp.pc,
                  (unsigned long long)machine->rsp.retired_instructions);
    return false;
  }
  for (lane = 0u; lane < 8u; lane++) {
    if (read_lane(machine->rsp.vector[3], lane) != expected[lane] ||
        machine->rsp.accumulator[lane] !=
            ((accumulator_before[lane] & ~UINT64_C(0xffff)) | expected[lane])) {
      (void)fprintf(stderr, "VLT lane %u result/ACC mismatch\n", lane);
      return false;
    }
  }
  if (machine->rsp.vco != 0u || machine->rsp.vce != 0u ||
      machine->rsp.vcc != UINT16_C(0x00c3) ||
      memcmp(machine->rsp.vector[0], vector_before[0], 16u) != 0) {
    (void)fprintf(stderr, "VLT result or flags mismatch\n");
    return false;
  }
  return true;
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
  machine->rsp.vco = UINT16_C(0x5aa5);
  machine->rsp.vcc = UINT16_C(0xa55a);
  machine->rsp.vce = UINT8_C(0xa5);
  machine->rsp.sp_status = UINT32_C(1);
}

static bool run_fetched(t64_machine *machine, uint32_t instruction,
                        uint32_t ticks, bool expect_retired) {
  t64_stop_request request = {T64_STOP_GUEST_TICK, ticks};
  t64_stop_result result = {0u, 0u};

  machine->sp_imem[0] = (uint8_t)(instruction >> 24);
  machine->sp_imem[1] = (uint8_t)(instruction >> 16);
  machine->sp_imem[2] = (uint8_t)(instruction >> 8);
  machine->sp_imem[3] = (uint8_t)instruction;
  if (ticks > 1u) {
    machine->sp_imem[4] = (uint8_t)(instruction >> 24);
    machine->sp_imem[5] = (uint8_t)(instruction >> 16);
    machine->sp_imem[6] = (uint8_t)(instruction >> 8);
    machine->sp_imem[7] = (uint8_t)instruction;
  }
  if (t64_rsp_start(machine) != T64_STATUS_OK ||
      t64_machine_advance(machine, &request, &result) != T64_STATUS_OK) {
    return false;
  }
  if (expect_retired) {
    return machine->rsp_observed_outcome == T64_RSP_ATTEMPT_RETIRED &&
           machine->rsp_observed_opcode == instruction &&
           machine->rsp.retired_instructions == ticks &&
           machine->rsp.pc == ticks * UINT32_C(4);
  }
  return machine->rsp_observed_outcome == T64_RSP_ATTEMPT_REFUSED &&
         machine->rsp_observed_refusal_reason ==
             T64_RSP_REFUSAL_UNSUPPORTED_OPCODE &&
         machine->rsp.retired_instructions == 0u && machine->rsp.pc == 0u;
}

static void set_compare_inputs(t64_machine *machine, const uint16_t left[8],
                              const uint16_t right[8]) {
  uint32_t lane;
  for (lane = 0u; lane < 8u; lane++) {
    write_lane(machine->rsp.vector[1], lane, left[lane]);
    write_lane(machine->rsp.vector[2], lane, right[lane]);
  }
}

static bool check_compare_result(t64_machine *machine, uint32_t destination,
                                 const uint16_t expected[8],
                                 uint16_t expected_vco,
                                 uint16_t expected_vcc,
                                 uint8_t expected_vce,
                                 const uint64_t accumulator_before[8]) {
  uint32_t lane;
  for (lane = 0u; lane < 8u; lane++) {
    if (read_lane(machine->rsp.vector[destination], lane) != expected[lane] ||
        machine->rsp.accumulator[lane] !=
            ((accumulator_before[lane] & ~UINT64_C(0xffff)) | expected[lane])) {
      (void)fprintf(stderr, "compare function result/ACC mismatch at lane %u\n",
                    lane);
      return false;
    }
  }
  if (machine->rsp.vco != expected_vco || machine->rsp.vcc != expected_vcc ||
      machine->rsp.vce != expected_vce) {
    (void)fprintf(stderr,
                  "compare function flags mismatch: got %04x/%04x/%02x "
                  "expected %04x/%04x/%02x\n",
                  machine->rsp.vco, machine->rsp.vcc, machine->rsp.vce,
                  expected_vco, expected_vcc, expected_vce);
    return false;
  }
  return true;
}

static bool compare_flag_matrix(rsp_compare_fixture *fixture) {
  static const uint16_t left[8] = {
      UINT16_C(0x8000), UINT16_C(0x0001), UINT16_C(0x7fff), UINT16_C(0),
      UINT16_C(0x8000), UINT16_C(2), UINT16_C(0xffff), UINT16_C(5)};
  static const uint16_t right[8] = {
      UINT16_C(0x7fff), UINT16_C(1), UINT16_C(0x8000), UINT16_C(0),
      UINT16_C(0x8000), UINT16_C(1), UINT16_C(0xffff), UINT16_C(6)};
  static const uint16_t veq_expected[8] = {
      UINT16_C(0x7fff), UINT16_C(1), UINT16_C(0x8000), UINT16_C(0),
      UINT16_C(0x8000), UINT16_C(1), UINT16_C(0xffff), UINT16_C(6)};
  static const uint16_t vne_expected[8] = {
      UINT16_C(0x8000), UINT16_C(1), UINT16_C(0x7fff), UINT16_C(0),
      UINT16_C(0x8000), UINT16_C(2), UINT16_C(0xffff), UINT16_C(5)};
  static const uint16_t vge_expected[8] = {
      UINT16_C(0x7fff), UINT16_C(1), UINT16_C(0x7fff), UINT16_C(0),
      UINT16_C(0x8000), UINT16_C(2), UINT16_C(0xffff), UINT16_C(6)};
  static const uint16_t vlt_expected[8] = {
      UINT16_C(0x8000), UINT16_C(1), UINT16_C(0x8000), UINT16_C(0),
      UINT16_C(0x8000), UINT16_C(1), UINT16_C(0xffff), UINT16_C(5)};
  struct compare_case {
    uint32_t function;
    uint32_t destination;
    uint16_t old_vco;
    uint16_t old_vcc;
    uint8_t old_vce;
    const uint16_t *expected;
    uint16_t vco;
    uint16_t vcc;
    uint8_t vce;
  };
  static const struct compare_case cases[] = {
      {0x20u, 3u, 0x0052u, 0xa55au, 0x10u, vlt_expected, 0u, 0x00c3u, 0u},
      {0x21u, 1u, 0xffffu, 0xa55au, 0xaau, veq_expected, 0u, 0x000au, 0u},
      {0x22u, 3u, 0xa55au, 0xffffu, 0xaau, vne_expected, 0xa500u, 0x00f5u, 0u},
      {0x23u, 3u, 0x0052u, 0xa55au, 0x10u, vge_expected, 0u, 0x003cu, 0u}};
  uint32_t case_index;

  for (case_index = 0u;
       case_index < (uint32_t)(sizeof cases / sizeof cases[0]);
       case_index++) {
    const struct compare_case *test_case = &cases[case_index];
    t64_machine *machine = fixture->machine;
    uint64_t accumulator_before[8];
    uint8_t vector_before[32][16];
    uint32_t lane;
    uint32_t instruction = encode_vector(0u, 2u, 1u,
                                         test_case->destination,
                                         test_case->function);

    prepare_machine(machine);
    set_compare_inputs(machine, left, right);
    machine->rsp.vco = test_case->old_vco;
    machine->rsp.vcc = test_case->old_vcc;
    machine->rsp.vce = test_case->old_vce;
    (void)memcpy(vector_before, machine->rsp.vector, sizeof vector_before);
    (void)memcpy(accumulator_before, machine->rsp.accumulator,
                 sizeof accumulator_before);
    if (!run_fetched(machine, instruction, 1u, true) ||
        !check_compare_result(machine, test_case->destination,
                              test_case->expected, test_case->vco,
                              test_case->vcc, test_case->vce,
                              accumulator_before)) {
      (void)fprintf(stderr, "compare matrix function %02x failed\n",
                    test_case->function);
      return false;
    }
    for (lane = 0u; lane < 32u; lane++) {
      if (lane != test_case->destination &&
          memcmp(machine->rsp.vector[lane], vector_before[lane], 16u) != 0) {
        (void)fprintf(stderr, "compare matrix changed V%u unexpectedly\n", lane);
        return false;
      }
    }
  }

  {
    static const uint16_t broadcast_left[8] = {
        0x8000, 0x0001, 0x7fff, 0x0000, 0x8000, 0x0002, 0xffff, 0x0005};
    static const uint16_t broadcast_right[8] = {
        0x0002, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};
    static const uint16_t broadcast_expected[8] = {
        0x0002, 0x0002, 0x7fff, 0x0002, 0x0002, 0x0002, 0x0002, 0x0005};
    t64_machine *machine = fixture->machine;
    uint64_t accumulator_before[8];
    uint32_t lane;
    uint32_t instruction = encode_vector(8u, 2u, 1u, 1u, 0x23u);

    prepare_machine(machine);
    set_compare_inputs(machine, broadcast_left, broadcast_right);
    machine->rsp.vco = UINT16_C(0xffff);
    machine->rsp.vce = 0u;
    for (lane = 0u; lane < 8u; lane++) {
      accumulator_before[lane] = machine->rsp.accumulator[lane];
    }
    if (instruction != UINT32_C(0x4b020863) ||
        !run_fetched(machine, instruction, 1u, true) ||
        !check_compare_result(machine, 1u, broadcast_expected, 0u,
                              UINT16_C(0x0084), 0u, accumulator_before)) {
      (void)fprintf(stderr, "VGE e=8 broadcast/source alias case failed\n");
      return false;
    }
  }
  return true;
}

static bool clip_chain_and_vcr(rsp_compare_fixture *fixture) {
  static const uint16_t left[8] = {
      UINT16_C(0xfffe), UINT16_C(3), UINT16_C(5), UINT16_C(1),
      UINT16_C(0xfffd), UINT16_C(0xffff), UINT16_C(1), UINT16_C(0xfffe)};
  static const uint16_t right[8] = {
      UINT16_C(1), UINT16_C(0xfffe), UINT16_C(2), UINT16_C(1),
      UINT16_C(0xffff), UINT16_C(1), UINT16_C(0xffff), UINT16_C(0xfffe)};
  static const uint16_t vch_expected[8] = {
      UINT16_C(0xffff), UINT16_C(3), UINT16_C(2), UINT16_C(1),
      UINT16_C(0xfffd), UINT16_C(0xffff), UINT16_C(1), UINT16_C(0xfffe)};
  static const uint16_t vcl_expected[8] = {
      UINT16_C(0xffff), UINT16_C(3), UINT16_C(2), UINT16_C(1),
      UINT16_C(0xfffd), UINT16_C(0xffff), UINT16_C(1), UINT16_C(0xfffe)};
  static const uint16_t vcr_expected[8] = {
      UINT16_C(0xfffe), UINT16_C(3), UINT16_C(2), UINT16_C(1),
      UINT16_C(0xfffd), UINT16_C(0xffff), UINT16_C(1), UINT16_C(0xfffe)};
  t64_machine *machine = fixture->machine;
  uint64_t accumulator_before[8];
  uint8_t vector_before[32][16];
  uint32_t lane;
  uint32_t vch = encode_vector(0u, 2u, 1u, 3u, 0x25u);
  uint32_t vcl = encode_vector(0u, 2u, 1u, 4u, 0x24u);

  if (vch != UINT32_C(0x4a0208e5) ||
      vcl != UINT32_C(0x4a020924)) {
    (void)fprintf(stderr, "VCH/VCL fixed fetched words mismatch\n");
    return false;
  }
  prepare_machine(machine);
  set_compare_inputs(machine, left, right);
  (void)memcpy(vector_before, machine->rsp.vector, sizeof vector_before);
  for (lane = 0u; lane < 8u; lane++) {
    accumulator_before[lane] = machine->rsp.accumulator[lane];
  }
  machine->rsp.vco = UINT16_C(0xffff);
  machine->rsp.vcc = UINT16_C(0xffff);
  machine->rsp.vce = UINT8_C(0xff);
  machine->sp_imem[0] = (uint8_t)(vch >> 24);
  machine->sp_imem[1] = (uint8_t)(vch >> 16);
  machine->sp_imem[2] = (uint8_t)(vch >> 8);
  machine->sp_imem[3] = (uint8_t)vch;
  machine->sp_imem[4] = (uint8_t)(vcl >> 24);
  machine->sp_imem[5] = (uint8_t)(vcl >> 16);
  machine->sp_imem[6] = (uint8_t)(vcl >> 8);
  machine->sp_imem[7] = (uint8_t)vcl;
  if (t64_rsp_start(machine) != T64_STATUS_OK) {
    return false;
  }
  {
    t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(1)};
    t64_stop_result result = {0u, 0u};
    if (t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
        machine->rsp.retired_instructions != UINT64_C(1) ||
        machine->rsp.pc != UINT32_C(4) ||
        machine->rsp_observed_opcode != vch ||
        machine->rsp_observed_outcome != T64_RSP_ATTEMPT_RETIRED) {
      (void)fprintf(stderr, "fetched VCH did not retire before VCL\n");
      return false;
    }
  }
  if (!check_compare_result(machine, 3u, vch_expected, UINT16_C(0x1763),
                            UINT16_C(0xcef1), UINT8_C(1),
                            accumulator_before)) {
    return false;
  }
  {
    t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(2)};
    t64_stop_result result = {0u, 0u};
    if (t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
        machine->rsp.retired_instructions != UINT64_C(2) ||
        machine->rsp.pc != UINT32_C(8) ||
        machine->rsp_observed_opcode != vcl ||
        machine->rsp_observed_outcome != T64_RSP_ATTEMPT_RETIRED) {
      (void)fprintf(stderr,
                    "fetched VCL did not consume VCH flags: retired=%llu "
                    "pc=%03x outcome=%u reason=%u opcode=%08x tick=%llu\n",
                    (unsigned long long)machine->rsp.retired_instructions,
                    machine->rsp.pc,
                    (unsigned)machine->rsp_observed_outcome,
                    (unsigned)machine->rsp_observed_refusal_reason,
                    machine->rsp_observed_opcode,
                    (unsigned long long)machine->tick);
      return false;
    }
  }
  for (lane = 0u; lane < 8u; lane++) {
    if (read_lane(machine->rsp.vector[4], lane) != vcl_expected[lane] ||
        machine->rsp.accumulator[lane] !=
            ((accumulator_before[lane] & ~UINT64_C(0xffff)) |
             vcl_expected[lane])) {
      (void)fprintf(stderr, "VCL chain result/ACC mismatch at lane %u\n", lane);
      return false;
    }
  }
  if (machine->rsp.vco != 0u || machine->rsp.vce != 0u ||
      machine->rsp.vcc != UINT16_C(0xce91)) {
    (void)fprintf(stderr, "VCL chain final flags mismatch\n");
    return false;
  }
  for (lane = 0u; lane < 32u; lane++) {
    if (lane != 3u && lane != 4u &&
        memcmp(machine->rsp.vector[lane], vector_before[lane], 16u) != 0) {
      (void)fprintf(stderr, "VCH/VCL chain changed V%u unexpectedly\n", lane);
      return false;
    }
  }

  prepare_machine(machine);
  set_compare_inputs(machine, left, right);
  for (lane = 0u; lane < 8u; lane++) {
    accumulator_before[lane] = machine->rsp.accumulator[lane];
  }
  {
    uint32_t vcr = encode_vector(0u, 2u, 1u, 5u, 0x26u);
    if (vcr != UINT32_C(0x4a020966) ||
        !run_fetched(machine, vcr, 1u, true) ||
      !check_compare_result(machine, 5u, vcr_expected, 0u, UINT16_C(0xce91),
                            0u, accumulator_before)) {
      (void)fprintf(stderr, "VCR complement/flag matrix failed\n");
      return false;
    }
  }
  return true;
}

static bool merge_and_nop(rsp_compare_fixture *fixture) {
  static const uint16_t left[8] = {
      0x1000, 0x1001, 0x1002, 0x1003, 0x1004, 0x1005, 0x1006, 0x1007};
  static const uint16_t right[8] = {
      0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2007};
  static const uint16_t expected[8] = {
      0x2000, 0x1001, 0x2002, 0x1003, 0x1004, 0x2005, 0x1006, 0x2007};
  t64_machine *machine = fixture->machine;
  uint64_t accumulator_before[8];
  uint16_t old_vco;
  uint16_t old_vcc;
  uint8_t old_vce;
  uint8_t vectors_before[32][16];
  uint64_t accumulators_before[8];
  uint32_t lane;

  prepare_machine(machine);
  set_compare_inputs(machine, left, right);
  machine->rsp.vco = UINT16_C(0xa55a);
  machine->rsp.vcc = UINT16_C(0xa55a);
  machine->rsp.vce = UINT8_C(0x96);
  old_vco = machine->rsp.vco;
  old_vcc = machine->rsp.vcc;
  old_vce = machine->rsp.vce;
  for (lane = 0u; lane < 8u; lane++) {
    accumulator_before[lane] = machine->rsp.accumulator[lane];
  }
  if (!run_fetched(machine, encode_vector(0u, 2u, 1u, 1u, 0x27u), 1u,
                   true) ||
      !check_compare_result(machine, 1u, expected, old_vco, old_vcc, old_vce,
                            accumulator_before)) {
    (void)fprintf(stderr, "VMRG low-VCC selection or alias failed\n");
    return false;
  }

  prepare_machine(machine);
  (void)memcpy(vectors_before, machine->rsp.vector, sizeof vectors_before);
  (void)memcpy(accumulators_before, machine->rsp.accumulator,
               sizeof accumulators_before);
  old_vco = machine->rsp.vco;
  old_vcc = machine->rsp.vcc;
  old_vce = machine->rsp.vce;
  if (!run_fetched(machine, encode_vector(1u, 2u, 1u, 3u, 0x37u), 1u,
                   true) ||
      machine->rsp.vco != old_vco || machine->rsp.vcc != old_vcc ||
      machine->rsp.vce != old_vce ||
      memcmp(vectors_before, machine->rsp.vector, sizeof vectors_before) != 0 ||
      memcmp(accumulators_before, machine->rsp.accumulator,
             sizeof accumulators_before) != 0) {
    (void)fprintf(stderr, "VNOP changed vector state or did not retire\n");
    return false;
  }
  return true;
}

static bool selector_refusal_matrix(rsp_compare_fixture *fixture) {
  t64_machine *machine = fixture->machine;
  uint32_t function;

  for (function = 0x20u; function <= 0x27u; function++) {
    uint8_t vectors_before[32][16];
    uint64_t accumulators_before[8];
    uint16_t old_vco;
    uint16_t old_vcc;
    uint8_t old_vce;
    uint32_t instruction;

    prepare_machine(machine);
    (void)memcpy(vectors_before, machine->rsp.vector, sizeof vectors_before);
    (void)memcpy(accumulators_before, machine->rsp.accumulator,
                 sizeof accumulators_before);
    old_vco = machine->rsp.vco;
    old_vcc = machine->rsp.vcc;
    old_vce = machine->rsp.vce;
    instruction = encode_vector(1u, 2u, 1u, 3u, function);
    if (!run_fetched(machine, instruction, 1u, false) ||
        machine->rsp.vco != old_vco || machine->rsp.vcc != old_vcc ||
        machine->rsp.vce != old_vce ||
        memcmp(vectors_before, machine->rsp.vector, sizeof vectors_before) !=
            0 ||
        memcmp(accumulators_before, machine->rsp.accumulator,
               sizeof accumulators_before) != 0) {
      (void)fprintf(stderr, "invalid compare selector e=1 function %02x failed\n",
                    function);
      return false;
    }
  }
  return true;
}

static bool unsupported_function_dispositions(rsp_compare_fixture *fixture) {
  static const uint8_t unsupported[] = {
      0x12u, 0x16u, 0x17u, 0x18u, 0x19u, 0x1au, 0x1bu, 0x1cu,
      0x1eu, 0x1fu, 0x2eu, 0x2fu, 0x38u, 0x39u, 0x3au, 0x3bu,
      0x3cu, 0x3du,
      0x3eu, 0x3fu};
  t64_machine *machine = fixture->machine;
  uint32_t index;

  for (index = 0u; index < (uint32_t)(sizeof unsupported); index++) {
    uint8_t vectors_before[32][16];
    uint64_t accumulators_before[8];
    uint16_t old_vco;
    uint16_t old_vcc;
    uint8_t old_vce;
    uint32_t instruction;

    prepare_machine(machine);
    (void)memcpy(vectors_before, machine->rsp.vector, sizeof vectors_before);
    (void)memcpy(accumulators_before, machine->rsp.accumulator,
                 sizeof accumulators_before);
    old_vco = machine->rsp.vco;
    old_vcc = machine->rsp.vcc;
    old_vce = machine->rsp.vce;
    instruction = encode_vector(0u, 2u, 1u, 3u, unsupported[index]);
    if (!run_fetched(machine, instruction, 1u, false) ||
        machine->rsp.vco != old_vco || machine->rsp.vcc != old_vcc ||
        machine->rsp.vce != old_vce ||
        memcmp(vectors_before, machine->rsp.vector, sizeof vectors_before) !=
            0 ||
        memcmp(accumulators_before, machine->rsp.accumulator,
               sizeof accumulators_before) != 0) {
      (void)fprintf(stderr,
                    "function slot %02x did not refuse without state changes\n",
                    unsupported[index]);
      return false;
    }
  }
  return true;
}

/* VNOP needs the vector-format bit and occupies the sole VU issue slot. */
static bool vnop_encoding_and_issue_test(rsp_compare_fixture *fixture) {
  t64_machine *m = fixture->machine;
  bool passed = true;
  for (uint32_t rs = 0u; rs < 16u; rs++) {
    prepare_machine(m);
    if (!run_fetched(m, UINT32_C(0x48000037) | (rs << 21), 1u, false)) {
      fprintf(stderr, "non-vector COP2 accepted as VNOP: rs=%u\n", rs);
      passed = false;
    }
  }
  prepare_machine(m);
  m->rsp.vco = 0u;
  for (uint32_t lane = 0u; lane < 8u; lane++) {
    write_lane(m->rsp.vector[1], lane, 1u);
    write_lane(m->rsp.vector[2], lane, 2u);
  }
  /* Raw VNOP then VADD v3,v1,v2; both require the VU. */
  if (t64_machine_bus_write(m, UINT32_C(0x04001000), 4u,
                             UINT32_C(0x4a000037)) != T64_STATUS_OK ||
      t64_machine_bus_write(m, UINT32_C(0x04001004), 4u,
                             UINT32_C(0x4a0208d0)) != T64_STATUS_OK) {
    return false;
  }
  {
    uint8_t vectors[32][16];
    uint64_t accumulators[8];
    t64_stop_request request = {T64_STOP_GUEST_TICK, 1u};
    t64_stop_result result = {0u, 0u};
    memcpy(vectors, m->rsp.vector, sizeof vectors);
    memcpy(accumulators, m->rsp.accumulator, sizeof accumulators);
    if (t64_rsp_start(m) != T64_STATUS_OK ||
        t64_machine_advance(m, &request, &result) != T64_STATUS_OK ||
        m->rsp.retired_instructions != 1u || m->rsp.pc != 4u ||
        memcmp(vectors, m->rsp.vector, sizeof vectors) != 0 ||
        memcmp(accumulators, m->rsp.accumulator, sizeof accumulators) != 0) {
      fprintf(stderr, "VNOP/VADD incorrectly shared a VU issue slot\n");
      passed = false;
    }
    request.value = 2u;
    if (t64_machine_advance(m, &request, &result) != T64_STATUS_OK ||
        m->rsp.retired_instructions != 2u || read_lane(m->rsp.vector[3], 0u) != 3u) {
      fprintf(stderr, "VNOP/VADD sequential result failed\n");
      passed = false;
    }
  }
  return passed;
}

int main(void) {
  rsp_compare_fixture fixture;
  bool passed;

  if (!fixture_create(&fixture)) {
    (void)fprintf(stderr, "VLT fixture creation failed\n");
    fixture_destroy(&fixture);
    return EXIT_FAILURE;
  }
  passed = vlt_fetched_red(&fixture) && compare_flag_matrix(&fixture) &&
           clip_chain_and_vcr(&fixture) && merge_and_nop(&fixture) &&
           selector_refusal_matrix(&fixture) &&
           unsupported_function_dispositions(&fixture) &&
           vnop_encoding_and_issue_test(&fixture);
  fixture_destroy(&fixture);
  return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
