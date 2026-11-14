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

static const uint16_t kSelectorLeft[8] = {
    UINT16_C(0x0001), UINT16_C(0x0002), UINT16_C(0x0003),
    UINT16_C(0x0004), UINT16_C(0x0005), UINT16_C(0x0006),
    UINT16_C(0x0007), UINT16_C(0x0008)};
static const uint16_t kSelectorRight[8] = {
    UINT16_C(0x0011), UINT16_C(0x0012), UINT16_C(0x0013),
    UINT16_C(0x0014), UINT16_C(0x0015), UINT16_C(0x0016),
    UINT16_C(0x0017), UINT16_C(0x0018)};

/* Literal VADD results from Table 3-3's selector mapping on printed p.58 and
 * the VADD equation on printed pp.230-231.  VCO starts at 0xa5a5; these
 * small operands avoid saturation so each result is also the low ACC slice. */
static const uint16_t kSelectorVadd[16][8] = {
    {0x0013, 0x0014, 0x0017, 0x0018, 0x001a, 0x001d, 0x001e, 0x0021},
    {0, 0, 0, 0, 0, 0, 0, 0}, /* element 1 is reserved */
    {0x0013, 0x0013, 0x0017, 0x0017, 0x001a, 0x001c, 0x001e, 0x0020},
    {0x0014, 0x0014, 0x0018, 0x0018, 0x001b, 0x001d, 0x001f, 0x0021},
    {0x0013, 0x0013, 0x0015, 0x0015, 0x001a, 0x001c, 0x001c, 0x001e},
    {0x0014, 0x0014, 0x0016, 0x0016, 0x001b, 0x001d, 0x001d, 0x001f},
    {0x0015, 0x0015, 0x0017, 0x0017, 0x001c, 0x001e, 0x001e, 0x0020},
    {0x0016, 0x0016, 0x0018, 0x0018, 0x001d, 0x001f, 0x001f, 0x0021},
    {0x0013, 0x0013, 0x0015, 0x0015, 0x0016, 0x0018, 0x0018, 0x001a},
    {0x0014, 0x0014, 0x0016, 0x0016, 0x0017, 0x0019, 0x0019, 0x001b},
    {0x0015, 0x0015, 0x0017, 0x0017, 0x0018, 0x001a, 0x001a, 0x001c},
    {0x0016, 0x0016, 0x0018, 0x0018, 0x0019, 0x001b, 0x001b, 0x001d},
    {0x0017, 0x0017, 0x0019, 0x0019, 0x001a, 0x001c, 0x001c, 0x001e},
    {0x0018, 0x0018, 0x001a, 0x001a, 0x001b, 0x001d, 0x001d, 0x001f},
    {0x0019, 0x0019, 0x001b, 0x001b, 0x001c, 0x001e, 0x001e, 0x0020},
    {0x001a, 0x001a, 0x001c, 0x001c, 0x001d, 0x001f, 0x001f, 0x0021}};

static bool run_expected_vector_case_with_acc(
    const char *name, uint32_t element, uint32_t function, uint32_t vs,
    uint32_t vt, uint32_t vd, const uint16_t left[8],
    const uint16_t right[8], const uint16_t expected_values[8],
    const uint16_t expected_accumulator_values[8],
    uint16_t expected_vco, uint16_t expected_vcc, uint8_t expected_vce,
    bool expect_retired) {
  rsp_vector_fixture fixture;
  t64_machine *machine;
  uint8_t vectors_expected[32][16];
  uint64_t accumulators_expected[8];
  uint32_t reg;
  uint32_t lane;
  uint32_t byte;
  uint32_t instruction = encode_vector(element, vt, vs, vd, function);
  bool passed = true;

  if (!fixture_create(&fixture)) {
    (void)fprintf(stderr, "%s fixture setup failed\n", name);
    fixture_destroy(&fixture);
    return false;
  }
  machine = fixture.machine;
  reset_machine(&fixture);
  for (reg = 0u; reg < 32u; reg++) {
    for (byte = 0u; byte < 16u; byte++) {
      machine->rsp.vector[reg][byte] =
          (uint8_t)(UINT32_C(0x83) + reg * UINT32_C(29) +
                    byte * UINT32_C(11));
    }
  }
  for (lane = 0u; lane < 8u; lane++) {
    machine->rsp.accumulator[lane] =
        UINT64_C(0x123456789abc) + (uint64_t)lane * UINT64_C(0x010203);
  }
  machine->rsp.vco = UINT16_C(0xa5a5);
  machine->rsp.vcc = UINT16_C(0x963c);
  machine->rsp.vce = UINT8_C(0xd5);
  for (lane = 0u; lane < 8u; lane++) {
    write_lane(machine->rsp.vector[vs], lane, left[lane]);
    write_lane(machine->rsp.vector[vt], lane, right[lane]);
  }
  (void)memcpy(vectors_expected, machine->rsp.vector,
               sizeof vectors_expected);
  (void)memcpy(accumulators_expected, machine->rsp.accumulator,
               sizeof accumulators_expected);
  if (expect_retired) {
    for (lane = 0u; lane < 8u; lane++) {
      write_lane(vectors_expected[vd], lane, expected_values[lane]);
      accumulators_expected[lane] =
          (accumulators_expected[lane] & ~UINT64_C(0xffff)) |
          (expected_accumulator_values == NULL
               ? expected_values[lane]
               : expected_accumulator_values[lane]);
    }
  }
  if (!execute_word(&fixture, instruction)) {
    (void)fprintf(stderr, "%s fetched execution setup failed\n", name);
    passed = false;
  } else if (expect_retired) {
    if (machine->rsp_observed_outcome != T64_RSP_ATTEMPT_RETIRED ||
        machine->rsp.pc != UINT32_C(4) ||
        machine->rsp.retired_instructions != UINT64_C(1)) {
      (void)fprintf(stderr,
                    "%s raw=%08x did not retire: outcome=%u reason=%u "
                    "pc=%03x retired=%llu\n",
                    name, instruction,
                    (unsigned)machine->rsp_observed_outcome,
                    (unsigned)machine->rsp_observed_refusal_reason,
                    (unsigned)machine->rsp.pc,
                    (unsigned long long)machine->rsp.retired_instructions);
      passed = false;
    }
  } else if (machine->rsp_observed_outcome != T64_RSP_ATTEMPT_REFUSED ||
             machine->rsp.pc != 0u ||
             machine->rsp.retired_instructions != 0u ||
             machine->rsp_observed_refusal_reason !=
                 T64_RSP_REFUSAL_UNSUPPORTED_OPCODE) {
    (void)fprintf(stderr,
                  "%s raw=%08x was not a pre-effect refusal: outcome=%u "
                  "reason=%u pc=%03x retired=%llu\n",
                  name, instruction, (unsigned)machine->rsp_observed_outcome,
                  (unsigned)machine->rsp_observed_refusal_reason,
                  (unsigned)machine->rsp.pc,
                  (unsigned long long)machine->rsp.retired_instructions);
    passed = false;
  }
  if (machine->rsp.vco != (expect_retired ? expected_vco : UINT16_C(0xa5a5)) ||
      machine->rsp.vcc != (expect_retired ? expected_vcc : UINT16_C(0x963c)) ||
      machine->rsp.vce != (expect_retired ? expected_vce : UINT8_C(0xd5))) {
    (void)fprintf(stderr,
                  "%s flags mismatch: got VCO=%04x VCC=%04x VCE=%02x\n",
                  name, machine->rsp.vco, machine->rsp.vcc, machine->rsp.vce);
    passed = false;
  }
  if (memcmp(machine->rsp.vector, vectors_expected,
             sizeof vectors_expected) != 0) {
    (void)fprintf(stderr, "%s complete vector bank mismatch\n", name);
    passed = false;
  }
  if (memcmp(machine->rsp.accumulator, accumulators_expected,
             sizeof accumulators_expected) != 0) {
    (void)fprintf(stderr, "%s complete 48-bit accumulator mismatch\n", name);
    passed = false;
  }
  fixture_destroy(&fixture);
  return passed;
}

static bool run_expected_vector_case(
    const char *name, uint32_t element, uint32_t function, uint32_t vs,
    uint32_t vt, uint32_t vd, const uint16_t left[8],
    const uint16_t right[8], const uint16_t expected_values[8],
    uint16_t expected_vco, uint16_t expected_vcc, uint8_t expected_vce,
    bool expect_retired) {
  return run_expected_vector_case_with_acc(
      name, element, function, vs, vt, vd, left, right, expected_values, NULL,
      expected_vco, expected_vcc, expected_vce, expect_retired);
}

typedef struct {
  const char *name;
  uint32_t function;
  const uint16_t *expected;
  uint16_t expected_vco;
} vector_basic_case;

static const uint16_t kVsubExpected[8] = {
    0xffef, 0xfff0, 0xffef, 0xfff0, 0xfff0, 0xffef, 0xfff0, 0xffef};
static const uint16_t kVabsExpected[8] = {
    0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017, 0x0018};
static const uint16_t kVaddcExpected[8] = {
    0x0012, 0x0014, 0x0016, 0x0018, 0x001a, 0x001c, 0x001e, 0x0020};
static const uint16_t kVsubcExpected[8] = {
    0xfff0, 0xfff0, 0xfff0, 0xfff0, 0xfff0, 0xfff0, 0xfff0, 0xfff0};
static const uint16_t kVandExpected[8] = {
    0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008};
static const uint16_t kVnandExpected[8] = {
    0xfffe, 0xfffd, 0xfffc, 0xfffb, 0xfffa, 0xfff9, 0xfff8, 0xfff7};
static const uint16_t kVorExpected[8] = {
    0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017, 0x0018};
static const uint16_t kVnorExpected[8] = {
    0xffee, 0xffed, 0xffec, 0xffeb, 0xffea, 0xffe9, 0xffe8, 0xffe7};
static const uint16_t kVxorExpected[8] = {
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010};
static const uint16_t kVnxorExpected[8] = {
    0xffef, 0xffef, 0xffef, 0xffef, 0xffef, 0xffef, 0xffef, 0xffef};

static bool selector_and_opcode_matrix(void) {
  static const vector_basic_case cases[] = {
      {"VSUB", 0x11u, kVsubExpected, 0u},
      {"VABS", 0x13u, kVabsExpected, UINT16_C(0xa5a5)},
      {"VADDC", 0x14u, kVaddcExpected, 0u},
      {"VSUBC", 0x15u, kVsubcExpected, UINT16_C(0xffff)},
      {"VAND", 0x28u, kVandExpected, UINT16_C(0xa5a5)},
      {"VNAND", 0x29u, kVnandExpected, UINT16_C(0xa5a5)},
      {"VOR", 0x2au, kVorExpected, UINT16_C(0xa5a5)},
      {"VNOR", 0x2bu, kVnorExpected, UINT16_C(0xa5a5)},
      {"VXOR", 0x2cu, kVxorExpected, UINT16_C(0xa5a5)},
      {"VNXOR", 0x2du, kVnxorExpected, UINT16_C(0xa5a5)}};
  size_t index;
  uint32_t element;
  bool passed = true;

  for (element = 0u; element < 16u; element++) {
    char name[48];
    (void)snprintf(name, sizeof name, "VADD selector %u", element);
    passed = run_expected_vector_case(
                 name, element, 0x10u, 1u, 2u, 3u, kSelectorLeft,
                 kSelectorRight, kSelectorVadd[element], 0u,
                 UINT16_C(0x963c), UINT8_C(0xd5), element != 1u) &&
             passed;
  }

  for (index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
    char name[64];
    (void)snprintf(name, sizeof name, "%s element 0", cases[index].name);
    passed = run_expected_vector_case(
                 name, 0u, cases[index].function, 1u, 2u, 3u,
                 kSelectorLeft, kSelectorRight, cases[index].expected,
                 cases[index].expected_vco, UINT16_C(0x963c), UINT8_C(0xd5),
                 true) &&
             passed;

    (void)snprintf(name, sizeof name, "%s vd aliases vs", cases[index].name);
    passed = run_expected_vector_case(
                 name, 0u, cases[index].function, 1u, 2u, 1u,
                 kSelectorLeft, kSelectorRight, cases[index].expected,
                 cases[index].expected_vco, UINT16_C(0x963c), UINT8_C(0xd5),
                 true) &&
             passed;
    (void)snprintf(name, sizeof name, "%s vd aliases vt", cases[index].name);
    passed = run_expected_vector_case(
                 name, 0u, cases[index].function, 1u, 2u, 2u,
                 kSelectorLeft, kSelectorRight, cases[index].expected,
                 cases[index].expected_vco, UINT16_C(0x963c), UINT8_C(0xd5),
                 true) &&
             passed;

    (void)snprintf(name, sizeof name, "%s rejects selector 1", cases[index].name);
    passed = run_expected_vector_case(
                 name, 1u, cases[index].function, 1u, 2u, 3u,
                 kSelectorLeft, kSelectorRight, cases[index].expected,
                 cases[index].expected_vco, UINT16_C(0x963c), UINT8_C(0xd5),
                 false) &&
             passed;
  }
  return passed;
}

static const uint16_t kAliasVadd[8] = {
    0x0003, 0x0004, 0x0007, 0x0008, 0x000a, 0x000d, 0x000e, 0x0011};
static const uint16_t kAliasVsub[8] = {
    0xffff, 0x0000, 0xffff, 0x0000, 0x0000, 0xffff, 0x0000, 0xffff};
static const uint16_t kAliasVaddc[8] = {
    0x0002, 0x0004, 0x0006, 0x0008, 0x000a, 0x000c, 0x000e, 0x0010};
static const uint16_t kAliasZero[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static const uint16_t kAliasNot[8] = {
    0xfffe, 0xfffd, 0xfffc, 0xfffb, 0xfffa, 0xfff9, 0xfff8, 0xfff7};
static const uint16_t kAliasOnes[8] = {
    0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff};

static bool operand_alias_matrix(void) {
  static const vector_basic_case cases[] = {
      {"VADD", 0x10u, kAliasVadd, 0u},
      {"VSUB", 0x11u, kAliasVsub, 0u},
      {"VABS", 0x13u, kSelectorLeft, UINT16_C(0xa5a5)},
      {"VADDC", 0x14u, kAliasVaddc, 0u},
      {"VSUBC", 0x15u, kAliasZero, 0u},
      {"VAND", 0x28u, kSelectorLeft, UINT16_C(0xa5a5)},
      {"VNAND", 0x29u, kAliasNot, UINT16_C(0xa5a5)},
      {"VOR", 0x2au, kSelectorLeft, UINT16_C(0xa5a5)},
      {"VNOR", 0x2bu, kAliasNot, UINT16_C(0xa5a5)},
      {"VXOR", 0x2cu, kAliasZero, UINT16_C(0xa5a5)},
      {"VNXOR", 0x2du, kAliasOnes, UINT16_C(0xa5a5)}};
  size_t index;
  bool passed = true;

  for (index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
    char name[64];
    (void)snprintf(name, sizeof name, "%s vs equals vt equals vd",
                   cases[index].name);
    passed = run_expected_vector_case(
                 name, 0u, cases[index].function, 1u, 1u, 1u,
                 kSelectorLeft, kSelectorLeft, cases[index].expected,
                 cases[index].expected_vco, UINT16_C(0x963c), UINT8_C(0xd5),
                 true) &&
             passed;
  }
  return passed;
}

static bool arithmetic_signed_boundaries(void) {
  static const uint16_t add_left[8] = {
      0x7fff, 0x8000, 0x8000, 0x7fff, 0x0000, 0x0001, 0xffff, 0x7fff};
  static const uint16_t add_right[8] = {
      0x0001, 0xffff, 0x7fff, 0x7fff, 0x8000, 0x8000, 0x0001, 0x8001};
  static const uint16_t add_expected[8] = {
      0x7fff, 0x8000, 0x0000, 0x7fff, 0x8000, 0x8002, 0x0000, 0x0001};
  static const uint16_t add_accumulator_expected[8] = {
      0x8001, 0x7fff, 0x0000, 0xfffe, 0x8000, 0x8002, 0x0000, 0x0001};
  static const uint16_t sub_left[8] = {
      0x8000, 0x7fff, 0x8000, 0x7fff, 0x0000, 0x0001, 0xffff, 0x0000};
  static const uint16_t sub_right[8] = {
      0x0001, 0xffff, 0x7fff, 0x8000, 0x8000, 0x0002, 0x0001, 0xffff};
  static const uint16_t sub_expected[8] = {
      0x8000, 0x7fff, 0x8000, 0x7fff, 0x7fff, 0xfffe, 0xfffe, 0x0000};
  static const uint16_t sub_accumulator_expected[8] = {
      0x7ffe, 0x8000, 0x0000, 0xffff, 0x8000, 0xfffe, 0xfffe, 0x0000};
  static const uint16_t abs_sign[8] = {
      0xffff, 0x0000, 0x0001, 0x8000, 0x7fff, 0x8000, 0x0000, 0xffff};
  static const uint16_t abs_value[8] = {
      0x8000, 0x1234, 0x8000, 0x8000, 0x7fff, 0x0001, 0x8000, 0x7fff};
  static const uint16_t abs_expected[8] = {
      0x7fff, 0x0000, 0x8000, 0x7fff, 0x7fff, 0xffff, 0x0000, 0x8001};
  static const uint16_t abs_accumulator_expected[8] = {
      0x8000, 0x0000, 0x8000, 0x8000, 0x7fff, 0xffff, 0x0000, 0x8001};
  bool passed = true;

  passed = run_expected_vector_case_with_acc(
               "VADD signed saturation and raw accumulator sums", 0u, 0x10u,
               1u, 2u, 3u, add_left, add_right, add_expected,
               add_accumulator_expected, 0u,
               UINT16_C(0x963c), UINT8_C(0xd5), true) &&
           passed;
  passed = run_expected_vector_case_with_acc(
               "VSUB signed saturation and raw accumulator differences", 0u,
               0x11u, 1u, 2u, 3u, sub_left, sub_right, sub_expected,
               sub_accumulator_expected, 0u,
               UINT16_C(0x963c), UINT8_C(0xd5), true) &&
           passed;
  passed = run_expected_vector_case_with_acc(
               "VABS negative minimum and zero sign", 0u, 0x13u, 1u, 2u, 3u,
               abs_sign, abs_value, abs_expected, abs_accumulator_expected,
               UINT16_C(0xa5a5),
               UINT16_C(0x963c), UINT8_C(0xd5), true) &&
           passed;
  return passed;
}

static bool subc_carry_boundaries(void) {
  static const uint16_t left[8] = {
      0x0000, 0xffff, 0x8000, 0x7fff, 0x0000, 0x8000, 0x7fff, 0xffff};
  static const uint16_t right[8] = {
      0xffff, 0x0000, 0x7fff, 0x8000, 0x0000, 0x7fff, 0xffff, 0xffff};
  static const uint16_t expected[8] = {
      0x0001, 0xffff, 0x0001, 0xffff, 0x0000, 0x0001, 0x8000, 0x0000};

  return run_expected_vector_case(
      "VSUBC unsigned borrow and not-equal lanes", 0u, 0x15u, 1u, 2u, 3u,
      left, right, expected, UINT16_C(0x6f49), UINT16_C(0x963c),
      UINT8_C(0xd5), true);
}

static bool logical_selector_edges(void) {
  const struct {
    const char *name;
    uint32_t function;
    const uint16_t *e2;
    const uint16_t *e7;
    const uint16_t *e15;
  } cases[] = {
      {"VAND", 0x28u,
       (const uint16_t[]){1, 0, 3, 0, 5, 4, 7, 0},
       (const uint16_t[]){0, 0, 0, 4, 0, 0, 0, 8},
       (const uint16_t[]){0, 0, 0, 0, 0, 0, 0, 8}},
      {"VNAND", 0x29u,
       (const uint16_t[]){0xfffe, 0xffff, 0xfffc, 0xffff, 0xfffa, 0xfffb,
                          0xfff8, 0xffff},
       (const uint16_t[]){0xffff, 0xffff, 0xffff, 0xfffb, 0xffff, 0xffff,
                          0xffff, 0xfff7},
       (const uint16_t[]){0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
                          0xffff, 0xfff7}},
      {"VOR", 0x2au,
       (const uint16_t[]){0x11, 0x13, 0x13, 0x17, 0x15, 0x17, 0x17, 0x1f},
       (const uint16_t[]){0x15, 0x16, 0x17, 0x14, 0x1d, 0x1e, 0x1f, 0x18},
       (const uint16_t[]){0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x18}},
      {"VNOR", 0x2bu,
       (const uint16_t[]){0xffee, 0xffec, 0xffec, 0xffe8, 0xffea, 0xffe8,
                          0xffe8, 0xffe0},
       (const uint16_t[]){0xffea, 0xffe9, 0xffe8, 0xffeb, 0xffe2, 0xffe1,
                          0xffe0, 0xffe7},
       (const uint16_t[]){0xffe6, 0xffe5, 0xffe4, 0xffe3, 0xffe2, 0xffe1,
                          0xffe0, 0xffe7}},
      {"VXOR", 0x2cu,
       (const uint16_t[]){0x10, 0x13, 0x10, 0x17, 0x10, 0x13, 0x10, 0x1f},
       (const uint16_t[]){0x15, 0x16, 0x17, 0x10, 0x1d, 0x1e, 0x1f, 0x10},
       (const uint16_t[]){0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x10}},
      {"VNXOR", 0x2du,
       (const uint16_t[]){0xffef, 0xffec, 0xffef, 0xffe8, 0xffef, 0xffec,
                          0xffef, 0xffe0},
       (const uint16_t[]){0xffea, 0xffe9, 0xffe8, 0xffef, 0xffe2, 0xffe1,
                          0xffe0, 0xffef},
       (const uint16_t[]){0xffe6, 0xffe5, 0xffe4, 0xffe3, 0xffe2, 0xffe1,
                          0xffe0, 0xffef}}};
  size_t index;
  bool passed = true;

  for (index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
    char name[64];
    (void)snprintf(name, sizeof name, "%s quarter selector 2",
                   cases[index].name);
    passed = run_expected_vector_case(
                 name, 2u, cases[index].function, 1u, 2u, 3u,
                 kSelectorLeft, kSelectorRight, cases[index].e2,
                 UINT16_C(0xa5a5), UINT16_C(0x963c), UINT8_C(0xd5), true) &&
             passed;
    (void)snprintf(name, sizeof name, "%s half selector 7",
                   cases[index].name);
    passed = run_expected_vector_case(
                 name, 7u, cases[index].function, 1u, 2u, 3u,
                 kSelectorLeft, kSelectorRight, cases[index].e7,
                 UINT16_C(0xa5a5), UINT16_C(0x963c), UINT8_C(0xd5), true) &&
             passed;
    (void)snprintf(name, sizeof name, "%s scalar selector 15",
                   cases[index].name);
    passed = run_expected_vector_case(
                 name, 15u, cases[index].function, 1u, 2u, 3u,
                 kSelectorLeft, kSelectorRight, cases[index].e15,
                 UINT16_C(0xa5a5), UINT16_C(0x963c), UINT8_C(0xd5), true) &&
             passed;
  }
  return passed;
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
  if (encode_vector(0u, 2u, 1u, 3u, 0x14u) != UINT32_C(0x4a0208d4)) {
    (void)fprintf(stderr, "VADDC fixed raw instruction word mismatch\n");
    passed = false;
  } else if (!execute_word(&fixture, UINT32_C(0x4a0208d4))) {
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
      if (machine->rsp.vco != UINT16_C(0x00b9) ||
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
  bool passed = selector_and_opcode_matrix();
  passed = operand_alias_matrix() && passed;
  passed = logical_selector_edges() && passed;
  passed = arithmetic_signed_boundaries() && passed;
  passed = subc_carry_boundaries() && passed;
  passed = vadcc_fetched_carries_and_complete_state() && passed;
  return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
