#include "t64_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_SP_BASE UINT32_C(0x04040000)
#define TEST_DMEM_BASE UINT32_C(0x04000000)
#define TEST_IMEM_BASE UINT32_C(0x04001000)
#define TEST_PI_BASE UINT32_C(0x04600000)

typedef struct {
  t64_host_allocator allocator;
  t64_machine *machine;
  uint8_t image[4096];
  size_t allocations;
  size_t releases;
} fixture;

static t64_status observed_mem_status;
static t64_status observed_dram_status;
static t64_status observed_length_status;

static bool bytes_equal(t64_machine *machine, uint32_t address,
                        const uint8_t *expected, size_t count) {
  uint32_t actual = 0u;
  size_t i;

  for (i = 0u; i < count; i++) {
    if (t64_machine_bus_read(machine, address + (uint32_t)i, 1u, &actual) !=
            T64_STATUS_OK ||
        actual != (uint32_t)expected[i]) {
      return false;
    }
  }
  return true;
}

static bool bytes_are_zero(t64_machine *machine, uint32_t address,
                           size_t count) {
  uint32_t actual = 0u;
  size_t i;

  for (i = 0u; i < count; i++) {
    if (t64_machine_bus_read(machine, address + (uint32_t)i, 1u, &actual) !=
            T64_STATUS_OK ||
        actual != 0u) {
      return false;
    }
  }
  return true;
}

static bool advance_to(t64_machine *machine, uint64_t tick) {
  t64_stop_request request = {T64_STOP_GUEST_TICK, tick};
  t64_stop_result result = {0u, 0u};

  return t64_machine_advance(machine, &request, &result) == T64_STATUS_OK &&
         result.tick == tick;
}

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  fixture *value = (fixture *)context;
  void *memory;

  if (storage_out == NULL || size == 0u || alignment == 0u) {
    return false;
  }
  memory = malloc(size);
  if (memory == NULL || (uintptr_t)memory % alignment != 0u) {
    free(memory);
    return false;
  }
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  if (value != NULL) {
    value->allocations++;
  }
  return true;
}

static void test_release(void *context, t64_host_storage storage) {
  fixture *value = (fixture *)context;

  if (value != NULL) {
    value->releases++;
  }
  free(storage.data);
}

static bool fixture_init_profile(fixture *value, const char *profile_id) {
  const t64_profile_descriptor *profile = NULL;
  size_t i;

  memset(value, 0, sizeof *value);
  value->allocator.allocate = test_allocate;
  value->allocator.release = test_release;
  value->allocator.context = value;
  for (i = 0u; i < sizeof value->image; i++) {
    value->image[i] = (uint8_t)((i * 29u + 7u) & 0xffu);
  }
  value->image[0] = 0x80u;
  value->image[1] = 0x37u;
  value->image[2] = 0x12u;
  value->image[3] = 0x40u;
  value->image[0x3eu] = (uint8_t)'E';
  if (t64_profile_lookup(profile_id, &profile) !=
          T64_STATUS_OK ||
      profile == NULL ||
      t64_machine_create(&value->allocator, profile, &value->machine) !=
          T64_STATUS_OK ||
      value->machine == NULL) {
    return false;
  }
  if (t64_machine_admit_rom(value->machine, value->image,
                            sizeof value->image) != T64_STATUS_OK) {
    t64_machine_destroy(value->machine);
    value->machine = NULL;
    return false;
  }
  return true;
}

static bool fixture_init(fixture *value) {
  return fixture_init_profile(value, "ntsc-cart-4m-skeleton");
}

static bool fixture_create_profile(fixture *value, const char *profile_id) {
  const t64_profile_descriptor *profile = NULL;
  t64_status lookup_status;
  t64_status create_status;

  memset(value, 0, sizeof *value);
  value->allocator.allocate = test_allocate;
  value->allocator.release = test_release;
  value->allocator.context = value;
  lookup_status = t64_profile_lookup(profile_id, &profile);
  create_status = lookup_status == T64_STATUS_OK && profile != NULL
                      ? t64_machine_create(&value->allocator, profile,
                                           &value->machine)
                      : lookup_status;
  if (create_status != T64_STATUS_OK || value->machine == NULL) {
    fprintf(stderr, "RSP profile setup failed: lookup=%u create=%u\n",
            (unsigned)lookup_status, (unsigned)create_status);
    return false;
  }
  return true;
}

static bool fixture_enable_synthetic_image(fixture *value) {
  t64_host_storage storage = {NULL, 0u, 0u};

  if (value == NULL || value->machine == NULL ||
      !test_allocate(value, 2u, 1u, &storage)) {
    return false;
  }
  ((uint8_t *)storage.data)[0] = UINT8_C(0x5a);
  ((uint8_t *)storage.data)[1] = UINT8_C(0xa5);
  value->machine->image_storage = storage;
  value->machine->image_length = 2u;
  value->machine->has_image = true;
  return true;
}

static bool rsp_profile_admission_test(void) {
  fixture value;
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t changed[T64_DIGEST_SIZE];
  uint32_t saved_gpr = 0u;
  t64_status before_status;
  t64_status changed_status;
  bool passed = fixture_create_profile(
      &value, "ntsc-cart-4m-openipl3-rsp-functional-v1");

  if (passed) {
    passed = fixture_enable_synthetic_image(&value);
  }
  if (passed) {
    before_status = t64_machine_digest(value.machine, before);
    passed = value.machine->profile.machine_model_version == 6u &&
             value.machine->profile.state_serialization_version == 12u &&
             before_status == T64_STATUS_OK;
    saved_gpr = value.machine->rsp.gpr[1];
    value.machine->rsp.gpr[1] ^= UINT32_C(1);
    changed_status = t64_machine_digest(value.machine, changed);
    passed = passed && changed_status == T64_STATUS_OK &&
             memcmp(before, changed, sizeof before) != 0;
    if (!passed) {
      fprintf(stderr,
              "RSP profile state check failed: model=%u state=%u digest=%u/%u changed=%d\n",
              (unsigned)value.machine->profile.machine_model_version,
              (unsigned)value.machine->profile.state_serialization_version,
              (unsigned)before_status, (unsigned)changed_status,
              memcmp(before, changed, sizeof before) != 0);
    }
    value.machine->rsp.gpr[1] = saved_gpr;
    t64_machine_destroy(value.machine);
  }
  return passed;
}

static bool rsp_fetched_vadd_test(void) {
  static const uint32_t program[] = {
      UINT32_C(0x24017fff), /* ADDIU r1,r0,32767 */
      UINT32_C(0x48810800), /* MTC2 r1,v1[0] */
      UINT32_C(0),           /* NOP: documented MTC2 destination latency */
      UINT32_C(0),
      UINT32_C(0),
      UINT32_C(0x4a0108d0), /* VADD v3,v1,v1, vector operand */
      UINT32_C(0),
      UINT32_C(0),
      UINT32_C(0),
      UINT32_C(0x48021800), /* MFC2 r2,v3[0] */
      UINT32_C(0),
      UINT32_C(0),
      UINT32_C(0xac020000), /* SW r2,0(r0) */
      UINT32_C(0x0000000d)  /* BREAK */
  };
  fixture value;
  t64_rsp_state before_vadd;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(5)};
  t64_stop_result result = {0u, 0u};
  static const uint8_t expected_dmem[] = {0u, 0u, 0x7fu, 0xffu};
  static const uint8_t expected_vector[] = {0x7fu, 0xffu, 0u, 0u,
                                            0u,    0u,    0u, 0u,
                                            0u,    0u,    0u, 0u,
                                            0u,    0u,    0u, 0u};
  size_t i;
  bool setup_ok = fixture_create_profile(
      &value, "ntsc-cart-4m-openipl3-rsp-functional-v1");
  bool passed = false;

  if (!setup_ok) {
    return false;
  }
  if (!fixture_enable_synthetic_image(&value)) {
    t64_machine_destroy(value.machine);
    return false;
  }
  t64_machine_reset_execution_state(value.machine);
  (void)memset(&value.machine->rsp, 0, sizeof value.machine->rsp);
  value.machine->rsp.vco = UINT16_C(1);
  value.machine->rsp.vcc = UINT16_C(0x5a5a);
  value.machine->rsp.vce = UINT8_C(0xa5);
  value.machine->rsp.sp_status = UINT32_C(0x40);
  for (i = 0u; i < sizeof program / sizeof program[0]; i++) {
    if (t64_machine_bus_write(value.machine,
                              TEST_IMEM_BASE + (uint32_t)(i * 4u), 4u,
                              program[i]) != T64_STATUS_OK) {
      t64_machine_destroy(value.machine);
      return false;
    }
  }
  if (t64_rsp_start(value.machine) != T64_STATUS_OK ||
      t64_machine_advance(value.machine, &request, &result) != T64_STATUS_OK ||
      result.tick != UINT64_C(5) || value.machine->rsp.pc != UINT32_C(0x14) ||
      value.machine->rsp.gpr[1] != UINT32_C(0x7fff) ||
      value.machine->rsp.retired_instructions != UINT64_C(5)) {
    t64_machine_destroy(value.machine);
    return false;
  }
  before_vadd = value.machine->rsp;
  request.value = UINT64_C(6);
  if (t64_machine_advance(value.machine, &request, &result) != T64_STATUS_OK ||
      result.tick != UINT64_C(6)) {
    t64_machine_destroy(value.machine);
    return false;
  }
  passed = value.machine->rsp_observation_valid &&
           value.machine->rsp_observed_pc == UINT32_C(0x14) &&
           value.machine->rsp_observed_opcode == UINT32_C(0x4a0108d0) &&
           value.machine->rsp_observed_outcome == T64_RSP_ATTEMPT_RETIRED &&
           value.machine->rsp.retired_instructions ==
               before_vadd.retired_instructions + UINT64_C(1) &&
           value.machine->rsp.pc == UINT32_C(0x18);
  if (!passed) {
    printf("# fetched VADD expected pc=0x14 word=0x4a0108d0 retired; ");
    printf("actual valid=%u pc=0x%03x word=0x%08x outcome=%u ",
           value.machine->rsp_observation_valid ? 1u : 0u,
           value.machine->rsp_observed_pc,
           value.machine->rsp_observed_opcode,
           value.machine->rsp_observed_outcome);
    printf("retired=%llu pc_after=0x%03x state_unchanged=%u\n",
           (unsigned long long)value.machine->rsp.retired_instructions,
           value.machine->rsp.pc,
           memcmp(&before_vadd, &value.machine->rsp,
                  sizeof before_vadd) == 0
               ? 1u
               : 0u);
  }
  request.value = UINT64_C(14);
  passed = passed &&
           t64_machine_advance(value.machine, &request, &result) ==
               T64_STATUS_OK &&
           result.tick == UINT64_C(14) &&
           result.reason == T64_STOP_REASON_BOUND &&
           !value.machine->halted &&
           value.machine->rsp.pc == UINT32_C(0x34) &&
           value.machine->rsp.retired_instructions == UINT64_C(14) &&
           value.machine->rsp.gpr[2] == UINT32_C(0x7fff) &&
           memcmp(value.machine->sp_dmem, expected_dmem,
                  sizeof expected_dmem) == 0 &&
           memcmp(value.machine->rsp.vector[3], expected_vector,
                  sizeof expected_vector) == 0 &&
           value.machine->rsp.accumulator[0] == UINT64_C(0xffff) &&
           value.machine->rsp.vco == 0u &&
           value.machine->rsp.vcc == UINT16_C(0x5a5a) &&
           value.machine->rsp.vce == UINT8_C(0xa5) &&
           (value.machine->rsp.sp_status & UINT32_C(0x43)) ==
               UINT32_C(0x43) &&
           (value.machine->mi_pending & UINT32_C(1)) != 0u &&
           value.machine->rsp_observation_valid &&
           value.machine->rsp_observed_pc == UINT32_C(0x34) &&
           value.machine->rsp_observed_opcode == UINT32_C(0x0000000d) &&
           value.machine->rsp_observed_outcome == T64_RSP_ATTEMPT_RETIRED;
  if (!passed) {
    fprintf(stderr,
            "RSP fetched scalar/VU/DMEM/BREAK path failed: reason=%u tick=%llu machine_halted=%u pc=0x%03x retired=%llu r2=0x%08x dmem=%02x%02x%02x%02x status=0x%08x mi=0x%08x\n",
            result.reason, (unsigned long long)result.tick,
            value.machine->halted ? 1u : 0u, value.machine->rsp.pc,
            (unsigned long long)value.machine->rsp.retired_instructions,
            value.machine->rsp.gpr[2], value.machine->sp_dmem[0],
            value.machine->sp_dmem[1], value.machine->sp_dmem[2],
            value.machine->sp_dmem[3], value.machine->rsp.sp_status,
            value.machine->mi_pending);
  }
  t64_machine_destroy(value.machine);
  return passed;
}

static bool rsp_break_allows_queued_pi_completion_test(void) {
  fixture value;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(3)};
  t64_stop_result result = {0u, 0u};
  uint32_t instruction = UINT32_C(0x0000000d);
  bool passed = fixture_create_profile(
      &value, "ntsc-cart-4m-openipl3-rsp-functional-v1");

  if (!passed) {
    return false;
  }
  if (!fixture_enable_synthetic_image(&value)) {
    t64_machine_destroy(value.machine);
    return false;
  }
  t64_machine_reset_execution_state(value.machine);
  (void)memset(&value.machine->rsp, 0, sizeof value.machine->rsp);
  if (t64_machine_bus_write(value.machine, TEST_IMEM_BASE, 4u, instruction) !=
          T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_PI_BASE, 4u,
                            UINT32_C(0x100)) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_PI_BASE + 4u, 4u,
                            UINT32_C(0x10000000)) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_PI_BASE + 12u, 4u,
                            UINT32_C(1)) != T64_STATUS_OK ||
      t64_rsp_start(value.machine) != T64_STATUS_OK) {
    t64_machine_destroy(value.machine);
    return false;
  }
  passed = t64_machine_advance(value.machine, &request, &result) ==
               T64_STATUS_OK &&
           result.tick == UINT64_C(3) &&
           result.reason == T64_STOP_REASON_BOUND && !value.machine->halted &&
           value.machine->rsp.pc == UINT32_C(0) &&
           value.machine->rsp.retired_instructions == UINT64_C(1) &&
           (value.machine->rsp.sp_status & UINT32_C(3)) == UINT32_C(3) &&
           !value.machine->pi.active_valid &&
           value.machine->ram[UINT32_C(0x100)] == UINT8_C(0x5a) &&
           value.machine->ram[UINT32_C(0x101)] == UINT8_C(0xa5) &&
           (value.machine->mi_pending & UINT32_C(0x10)) != 0u;
  if (!passed) {
    fprintf(stderr,
            "RSP BREAK queued-PI check failed: reason=%u tick=%llu halted=%u rsp_status=0x%08x pi_active=%u rdram=%02x%02x mi=0x%08x\n",
            result.reason, (unsigned long long)result.tick,
            value.machine->halted ? 1u : 0u, value.machine->rsp.sp_status,
            value.machine->pi.active_valid ? 1u : 0u,
            value.machine->ram[UINT32_C(0x100)],
            value.machine->ram[UINT32_C(0x101)], value.machine->mi_pending);
  }
  t64_machine_destroy(value.machine);
  return passed;
}

static bool rsp_store_preserves_only_nonoverlapping_seeded_links_test(void) {
  static const uint32_t program[] = {
      UINT32_C(0x24011234), /* ADDIU r1,r0,0x1234 */
      UINT32_C(0xac010100), /* SW r1,0x100(r0) */
      UINT32_C(0x0000000d)  /* BREAK */
  };
  static const struct {
    uint32_t linked_offset;
    uint32_t linked_width;
    bool expected_valid;
  } cases[] = {
      {UINT32_C(0x100), UINT32_C(4), false},
      {UINT32_C(0x100), UINT32_C(8), false},
      {UINT32_C(0x104), UINT32_C(4), true},
      {UINT32_C(0x108), UINT32_C(8), true},
  };
  size_t case_index;
  bool passed = true;

  for (case_index = 0u; case_index < sizeof cases / sizeof cases[0];
       case_index++) {
    fixture value;
    t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(2)};
    t64_stop_result result = {0u, 0u};
    size_t instruction_index;
    bool fixture_ok = fixture_create_profile(
        &value, "ntsc-cart-4m-openipl3-rsp-functional-v1");

    if (!fixture_ok) {
      return false;
    }
    if (!fixture_enable_synthetic_image(&value)) {
      t64_machine_destroy(value.machine);
      return false;
    }
    t64_machine_reset_execution_state(value.machine);
    (void)memset(&value.machine->rsp, 0, sizeof value.machine->rsp);
    for (instruction_index = 0u;
         instruction_index < sizeof program / sizeof program[0];
         instruction_index++) {
      if (t64_machine_bus_write(
              value.machine,
              TEST_IMEM_BASE + (uint32_t)(instruction_index * 4u), 4u,
              program[instruction_index]) != T64_STATUS_OK) {
        t64_machine_destroy(value.machine);
        return false;
      }
    }
    /* Native fixture seed only; no guest LL/LLD instruction is executed. */
    value.machine->cpu.linked_valid = true;
    value.machine->cpu.linked_physical_address =
        TEST_DMEM_BASE + cases[case_index].linked_offset;
    value.machine->cpu.linked_width = cases[case_index].linked_width;
    if (t64_rsp_start(value.machine) != T64_STATUS_OK ||
        t64_machine_advance(value.machine, &request, &result) !=
            T64_STATUS_OK) {
      t64_machine_destroy(value.machine);
      return false;
    }
    passed = passed && result.tick == UINT64_C(2) &&
             value.machine->rsp_observed_opcode == UINT32_C(0xac010100) &&
             value.machine->rsp_observed_outcome ==
                 T64_RSP_ATTEMPT_RETIRED &&
             value.machine->cpu.linked_valid ==
                 cases[case_index].expected_valid &&
             value.machine->sp_dmem[UINT32_C(0x100)] == 0u &&
             value.machine->sp_dmem[UINT32_C(0x101)] == 0u &&
             value.machine->sp_dmem[UINT32_C(0x102)] == UINT8_C(0x12) &&
             value.machine->sp_dmem[UINT32_C(0x103)] == UINT8_C(0x34);
    t64_machine_destroy(value.machine);
  }
  return passed;
}

static bool rsp_refuses_unknown_opcode_atomically_test(void) {
  fixture value;
  t64_rsp_state before_rsp;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(1)};
  t64_stop_result result = {0u, 0u};
  uint8_t before_digest[T64_DIGEST_SIZE];
  uint8_t after_digest[T64_DIGEST_SIZE];
  uint32_t unsupported = UINT32_C(0x4a0108d1);
  bool passed = fixture_create_profile(
      &value, "ntsc-cart-4m-openipl3-rsp-functional-v1");

  if (!passed) {
    return false;
  }
  if (!fixture_enable_synthetic_image(&value)) {
    t64_machine_destroy(value.machine);
    return false;
  }
  t64_machine_reset_execution_state(value.machine);
  (void)memset(&value.machine->rsp, 0, sizeof value.machine->rsp);
  if (t64_machine_bus_write(value.machine, TEST_IMEM_BASE, 4u, unsupported) !=
          T64_STATUS_OK ||
      t64_rsp_start(value.machine) != T64_STATUS_OK) {
    t64_machine_destroy(value.machine);
    return false;
  }
  before_rsp = value.machine->rsp;
  passed = t64_machine_advance(value.machine, &request, &result) ==
               T64_STATUS_OK &&
           result.tick == UINT64_C(1) &&
           memcmp(&before_rsp, &value.machine->rsp, sizeof before_rsp) == 0 &&
           value.machine->rsp_observation_valid &&
           value.machine->rsp_observed_pc == 0u &&
           value.machine->rsp_observed_opcode == unsupported &&
           value.machine->rsp_observed_outcome == T64_RSP_ATTEMPT_REFUSED &&
           value.machine->queue_count == 0u &&
           t64_machine_digest(value.machine, before_digest) == T64_STATUS_OK;
  value.machine->rsp_observed_pc = UINT32_C(0x1234);
  value.machine->rsp_observed_opcode = UINT32_C(0xffffffff);
  value.machine->rsp_observed_outcome = T64_RSP_ATTEMPT_RETIRED;
  value.machine->rsp_observation_valid = false;
  passed = passed &&
           t64_machine_digest(value.machine, after_digest) == T64_STATUS_OK &&
           memcmp(before_digest, after_digest, sizeof before_digest) == 0;
  t64_machine_destroy(value.machine);
  return passed;
}

static bool rsp_old_profile_refusal_test(void) {
  fixture value;
  bool passed = fixture_create_profile(&value, "ntsc-cart-4m-skeleton");

  if (passed) {
    passed = t64_rsp_start(value.machine) == T64_STATUS_BUS_DEVICE_REFUSED;
    t64_machine_destroy(value.machine);
  }
  return passed;
}

static bool source_at_completion_test(void) {
  fixture value;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(12)};
  t64_stop_result result = {0u, 0u};
  uint8_t expected[8];
  uint32_t byte = 0u;
  size_t i;
  bool passed = true;
  t64_status mem_status;
  t64_status dram_status;
  t64_status length_status;

  if (!fixture_init(&value)) {
    return false;
  }
  for (i = 0u; i < sizeof expected; i++) {
    expected[i] = (uint8_t)(UINT8_C(0xa0) + (uint8_t)i);
    if (t64_machine_bus_write(value.machine, UINT32_C(0x100) +
                                                 (uint32_t)i,
                              1u, (uint32_t)expected[i]) != T64_STATUS_OK) {
      passed = false;
    }
  }
  mem_status = t64_machine_bus_write(value.machine, TEST_SP_BASE, 4u,
                                     UINT32_C(0x00000003));
  dram_status = t64_machine_bus_write(value.machine, TEST_SP_BASE + 4u, 4u,
                                      UINT32_C(0x00000107));
  length_status = t64_machine_bus_write(value.machine, TEST_SP_BASE + 8u, 4u,
                                        UINT32_C(0));
  observed_mem_status = mem_status;
  observed_dram_status = dram_status;
  observed_length_status = length_status;
  if (mem_status != T64_STATUS_OK || dram_status != T64_STATUS_OK ||
      length_status != T64_STATUS_OK) {
    passed = false;
  }
  for (i = 0u; i < sizeof expected; i++) {
    if (t64_machine_bus_read(value.machine, TEST_DMEM_BASE + (uint32_t)i,
                             1u, &byte) != T64_STATUS_OK ||
        byte != 0u) {
      passed = false;
    }
  }
  for (i = 0u; i < sizeof expected; i++) {
    expected[i] = (uint8_t)(UINT8_C(0x50) + (uint8_t)i);
    if (t64_machine_bus_write(value.machine, UINT32_C(0x100) +
                                                 (uint32_t)i,
                              1u, (uint32_t)expected[i]) != T64_STATUS_OK) {
      passed = false;
    }
  }
  if (t64_machine_advance(value.machine, &request, &result) !=
          T64_STATUS_OK ||
      result.tick != UINT64_C(12)) {
    passed = false;
  }
  for (i = 0u; i < sizeof expected; i++) {
    if (t64_machine_bus_read(value.machine, TEST_DMEM_BASE + (uint32_t)i,
                             1u, &byte) != T64_STATUS_OK ||
        byte != 0u) {
      passed = false;
    }
  }
  request.value = UINT64_C(13);
  if (t64_machine_advance(value.machine, &request, &result) !=
          T64_STATUS_OK ||
      result.tick != UINT64_C(13)) {
    passed = false;
  }
  for (i = 0u; i < sizeof expected; i++) {
    if (t64_machine_bus_read(value.machine, TEST_DMEM_BASE + (uint32_t)i,
                             1u, &byte) != T64_STATUS_OK ||
        byte != (uint32_t)expected[i]) {
      passed = false;
    }
  }
  t64_machine_destroy(value.machine);
  return passed;
}

static bool rectangular_imem_rows_test(void) {
  fixture value;
  uint8_t expected[48];
  size_t row;
  size_t byte;
  bool passed = true;

  if (!fixture_init(&value)) {
    return false;
  }
  for (row = 0u; row < 3u; row++) {
    uint32_t row_start = UINT32_C(0x200) + (uint32_t)(row * 17u);

    for (byte = 0u; byte < 16u; byte++) {
      expected[row * 16u + byte] =
          (uint8_t)(UINT8_C(0x40) + (uint8_t)(row * 16u + byte));
      if (t64_machine_bus_write(value.machine,
                                row_start + (uint32_t)byte, 1u,
                                expected[row * 16u + byte]) != T64_STATUS_OK) {
        passed = false;
      }
    }
  }
  if (t64_machine_bus_write(value.machine, TEST_SP_BASE, 4u,
                            UINT32_C(0x00001027)) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 4u, 4u,
                            UINT32_C(0x00000207)) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 8u, 4u,
                            UINT32_C(0x0010200f)) != T64_STATUS_OK) {
    passed = false;
  }
  if (!bytes_are_zero(value.machine, UINT32_C(0x04001020), 48u) ||
      !advance_to(value.machine, UINT64_C(17)) ||
      !bytes_are_zero(value.machine, UINT32_C(0x04001020), 48u) ||
      !advance_to(value.machine, UINT64_C(18)) ||
      !bytes_equal(value.machine, UINT32_C(0x04001020), expected,
                   sizeof expected)) {
    passed = false;
  }
  t64_machine_destroy(value.machine);
  return passed;
}

static bool write_wraps_in_selected_dmem_test(void) {
  fixture value;
  uint8_t expected[16];
  size_t i;
  bool passed = true;

  if (!fixture_init(&value)) {
    return false;
  }
  for (i = 0u; i < sizeof expected; i++) {
    expected[i] = (uint8_t)(UINT8_C(0xc0) + (uint8_t)i);
    if (t64_machine_bus_write(
            value.machine,
            TEST_DMEM_BASE +
                (uint32_t)((UINT32_C(0x0ff8) + (uint32_t)i) &
                           UINT32_C(0x0fff)),
            1u, (uint32_t)expected[i]) != T64_STATUS_OK) {
      passed = false;
    }
  }
  if (t64_machine_bus_write(value.machine, TEST_SP_BASE, 4u,
                            UINT32_C(0x00000ffb)) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 4u, 4u,
                            UINT32_C(0x00000307)) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 12u, 4u,
                            UINT32_C(8)) != T64_STATUS_OK) {
    passed = false;
  }
  if (!bytes_are_zero(value.machine, UINT32_C(0x300), sizeof expected) ||
      !advance_to(value.machine, UINT64_C(13)) ||
      !bytes_are_zero(value.machine, UINT32_C(0x300), sizeof expected) ||
      !advance_to(value.machine, UINT64_C(14)) ||
      !bytes_equal(value.machine, UINT32_C(0x300), expected,
                   sizeof expected)) {
    passed = false;
  }
  t64_machine_destroy(value.machine);
  return passed;
}

static bool last_valid_and_first_invalid_row_test(void) {
  fixture value;
  uint8_t expected[8];
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  uint32_t length_latch = UINT32_MAX;
  size_t i;
  bool passed = true;

  if (!fixture_init(&value)) {
    return false;
  }
  for (i = 0u; i < sizeof expected; i++) {
    expected[i] = (uint8_t)(UINT8_C(0x70) + (uint8_t)i);
    if (t64_machine_bus_write(value.machine,
                              UINT32_C(0x003ffff8) + (uint32_t)i, 1u,
                              (uint32_t)expected[i]) != T64_STATUS_OK) {
      passed = false;
    }
  }
  if (t64_machine_bus_write(value.machine, TEST_SP_BASE, 4u, 0u) !=
          T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 4u, 4u,
                            UINT32_C(0x003fffff)) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 8u, 4u, 0u) !=
          T64_STATUS_OK ||
      !advance_to(value.machine, UINT64_C(13)) ||
      !bytes_equal(value.machine, TEST_DMEM_BASE, expected,
                   sizeof expected)) {
    passed = false;
  }
  if (t64_machine_bus_write(value.machine, TEST_SP_BASE + 4u, 4u,
                            UINT32_C(0x003fffff)) != T64_STATUS_OK ||
      t64_machine_digest(value.machine, before) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 8u, 4u,
                            UINT32_C(0x00001000)) !=
          T64_STATUS_BUS_DEVICE_REFUSED ||
      t64_machine_digest(value.machine, after) != T64_STATUS_OK ||
      memcmp(before, after, sizeof before) != 0 ||
      t64_machine_bus_read(value.machine, TEST_SP_BASE + 8u, 4u,
                           &length_latch) != T64_STATUS_OK ||
      length_latch != 0u) {
    passed = false;
  }
  t64_machine_destroy(value.machine);
  return passed;
}

static bool maximum_sp_fields_have_checked_deadline_test(void) {
  fixture value;
  bool passed = true;

  if (!fixture_init(&value)) {
    return false;
  }
  if (t64_machine_bus_write(value.machine, TEST_SP_BASE, 4u, 0u) !=
          T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 4u, 4u, 0u) !=
          T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 8u, 4u,
                            UINT32_C(0x00ffffff)) != T64_STATUS_OK ||
      !value.machine->sp.active.valid ||
      value.machine->sp.active.bytes_per_line != UINT32_C(4096) ||
      value.machine->sp.active.line_count != UINT32_C(256) ||
      value.machine->sp.active.skip != UINT32_C(15) ||
      value.machine->sp.active.payload_bytes != UINT32_C(1048576) ||
      value.machine->sp.active.start_tick != 0u ||
      value.machine->sp.active.end_tick != UINT64_C(131084) ||
      value.machine->queue_count != 1u) {
    passed = false;
  }
  t64_machine_destroy(value.machine);
  return passed;
}

static bool pending_queue_and_source_visibility_test(void) {
  fixture value;
  t64_sp_state expected_sp_state;
  t64_sp_dma_descriptor active_before;
  t64_sp_dma_descriptor pending_before;
  t64_trace_record records[T64_TRACE_RECORD_CAPACITY];
  uint8_t pending_bytes[16];
  uint32_t status = 0u;
  uint32_t length_latch = 0u;
  size_t allocations_before;
  size_t releases_before;
  size_t record_count = 0u;
  size_t completed_at_13 = SIZE_MAX;
  size_t promoted_at_13 = SIZE_MAX;
  size_t completed_at_27 = SIZE_MAX;
  uint64_t dropped_records = 0u;
  bool drop_count_saturated = false;
  size_t issue_records = 0u;
  bool drop_record = false;
  size_t i;
  bool passed = true;

  if (!fixture_init(&value)) {
    return false;
  }
  (void)t64_machine_trace_set_enabled(value.machine, true);
  for (i = 0u; i < 8u; i++) {
    if (t64_machine_bus_write(value.machine, UINT32_C(0x100) +
                                                 (uint32_t)i,
                              1u, UINT32_C(0x20) + (uint32_t)i) !=
        T64_STATUS_OK) {
      passed = false;
    }
    pending_bytes[i] = (uint8_t)(UINT8_C(0x90) + (uint8_t)i);
  }
  for (; i < sizeof pending_bytes; i++) {
    pending_bytes[i] = (uint8_t)(UINT8_C(0x90) + (uint8_t)i);
  }
  if (t64_machine_bus_write(value.machine, TEST_SP_BASE, 4u, 0u) !=
          T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 4u, 4u,
                            UINT32_C(0x100)) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 8u, 4u, 0u) !=
          T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE, 4u,
                            UINT32_C(0x1020)) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 4u, 4u,
                            UINT32_C(0x200)) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 8u, 4u,
                            UINT32_C(8)) != T64_STATUS_OK) {
    passed = false;
  }
  if (t64_machine_bus_read(value.machine, TEST_SP_BASE + 0x14u, 4u,
                           &status) != T64_STATUS_OK ||
      status != UINT32_C(1) ||
      t64_machine_bus_read(value.machine, TEST_SP_BASE + 0x18u, 4u,
                           &status) != T64_STATUS_OK ||
      status != UINT32_C(1) ||
      t64_machine_bus_read(value.machine, TEST_SP_BASE + 0x10u, 4u,
                           &status) != T64_STATUS_OK ||
      status != UINT32_C(0x0c)) {
    passed = false;
  }
  if (!passed || !value.machine->sp.pending.valid) {
    t64_machine_destroy(value.machine);
    return false;
  }

  active_before = value.machine->sp.active;
  pending_before = value.machine->sp.pending;
  while (value.machine->queue_count <
         (size_t)value.machine->profile.event_queue_capacity) {
    t64_status schedule_status = t64_test_schedule_synthetic(
        value.machine, UINT64_C(1000) + (uint64_t)value.machine->queue_count,
        T64_TEST_EVENT_NOOP, UINT64_C(0));

    if (schedule_status != T64_STATUS_OK) {
      passed = false;
      break;
    }
  }
  if (value.machine->queue_count !=
      (size_t)value.machine->profile.event_queue_capacity) {
    passed = false;
  }

  if (t64_machine_bus_write(value.machine, TEST_SP_BASE, 4u,
                            UINT32_C(0x0040)) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 4u, 4u,
                            UINT32_C(0x300)) != T64_STATUS_OK) {
    passed = false;
  }
  expected_sp_state = value.machine->sp;
  expected_sp_state.write_length = UINT32_C(8);
  if (t64_machine_bus_write(value.machine, TEST_SP_BASE + 12u, 4u,
                            UINT32_C(8)) !=
          T64_STATUS_BUS_DEVICE_REFUSED ||
      value.machine->queue_count !=
          (size_t)value.machine->profile.event_queue_capacity ||
      memcmp(&expected_sp_state, &value.machine->sp,
             sizeof expected_sp_state) != 0 ||
      memcmp(&active_before, &value.machine->sp.active,
             sizeof active_before) != 0 ||
      memcmp(&pending_before, &value.machine->sp.pending,
             sizeof pending_before) != 0 ||
      t64_machine_bus_read(value.machine, TEST_SP_BASE + 12u, 4u,
                           &length_latch) != T64_STATUS_OK ||
      length_latch != UINT32_C(8) ||
      t64_machine_bus_read(value.machine, UINT32_C(0x04300008), 4u,
                           &status) != T64_STATUS_OK ||
      status != 0u) {
    passed = false;
  }

  allocations_before = value.allocations;
  releases_before = value.releases;
  if (!advance_to(value.machine, UINT64_C(13))) {
    passed = false;
  }
  if (value.machine->sp.active.token != pending_before.token ||
      value.machine->sp.pending.valid ||
      !value.machine->sp.active.valid ||
      value.machine->queue_count >=
          (size_t)value.machine->profile.event_queue_capacity ||
      t64_machine_bus_read(value.machine, TEST_SP_BASE + 0x14u, 4u,
                           &status) != T64_STATUS_OK ||
      status != 0u ||
      t64_machine_bus_read(value.machine, TEST_SP_BASE + 0x18u, 4u,
                           &status) != T64_STATUS_OK ||
      status != UINT32_C(1) ||
      t64_machine_bus_read(value.machine, TEST_SP_BASE + 0x10u, 4u,
                           &status) != T64_STATUS_OK ||
      status != UINT32_C(0x04)) {
    passed = false;
  }
  for (i = 0u; i < sizeof pending_bytes; i++) {
    pending_bytes[i] = (uint8_t)(UINT8_C(0x40) + (uint8_t)i);
    if (t64_machine_bus_write(value.machine, UINT32_C(0x200) +
                                                 (uint32_t)i,
                              1u, (uint32_t)pending_bytes[i]) !=
        T64_STATUS_OK) {
      passed = false;
    }
  }
  if (!advance_to(value.machine, UINT64_C(26)) ||
      !bytes_are_zero(value.machine, UINT32_C(0x04001020), 16u) ||
      !advance_to(value.machine, UINT64_C(27)) ||
      !bytes_equal(value.machine, UINT32_C(0x04001020), pending_bytes,
                   sizeof pending_bytes) ||
      value.machine->sp.active.valid || value.machine->sp.pending.valid ||
      t64_machine_bus_read(value.machine, TEST_SP_BASE + 0x18u, 4u,
                           &status) != T64_STATUS_OK ||
      status != 0u ||
      t64_machine_bus_read(value.machine, TEST_SP_BASE + 0x10u, 4u,
                           &status) != T64_STATUS_OK ||
      status != 0u ||
      t64_machine_bus_read(value.machine, UINT32_C(0x04300008), 4u,
                           &status) != T64_STATUS_OK ||
      status != 0u || value.allocations != allocations_before ||
      value.releases != releases_before) {
    passed = false;
  }
  if (t64_machine_trace_drain(value.machine, records,
                              T64_TRACE_RECORD_CAPACITY, &record_count,
                              &dropped_records, &drop_count_saturated) !=
          T64_STATUS_OK ||
      dropped_records != 0u || drop_count_saturated) {
    passed = false;
  }
  for (i = 0u; i < record_count; i++) {
    if (records[i].device != T64_TRACE_DEVICE_SP) {
      continue;
    }
    if (records[i].kind == T64_TRACE_KIND_SP_ISSUE &&
        records[i].tick == 0u) {
      issue_records++;
    } else if (records[i].kind == T64_TRACE_KIND_SP_DROP) {
      drop_record = true;
    } else if (records[i].kind == T64_TRACE_KIND_SP_COMPLETE &&
               records[i].tick == UINT64_C(13)) {
      completed_at_13 = i;
    } else if (records[i].kind == T64_TRACE_KIND_SP_PROMOTE &&
               records[i].tick == UINT64_C(13)) {
      promoted_at_13 = i;
    } else if (records[i].kind == T64_TRACE_KIND_SP_COMPLETE &&
               records[i].tick == UINT64_C(27)) {
      completed_at_27 = i;
    }
  }
  if (issue_records != 2u || !drop_record || completed_at_13 == SIZE_MAX ||
      promoted_at_13 == SIZE_MAX || completed_at_27 == SIZE_MAX ||
      completed_at_13 >= promoted_at_13 ||
      promoted_at_13 >= completed_at_27) {
    passed = false;
  }
  t64_machine_destroy(value.machine);
  return passed;
}

static bool pending_capacity_refusal_test(void) {
  fixture value;
  t64_sp_dma_descriptor active_before;
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  uint32_t latch = UINT32_MAX;
  uint32_t busy = 0u;
  bool passed = true;

  if (!fixture_init(&value)) {
    return false;
  }
  if (t64_machine_bus_write(value.machine, TEST_SP_BASE, 4u, 0u) !=
          T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 4u, 4u,
                            UINT32_C(0x100)) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 8u, 4u, 0u) !=
          T64_STATUS_OK) {
    passed = false;
  }
  active_before = value.machine->sp.active;
  while (value.machine->queue_count <
         (size_t)value.machine->profile.event_queue_capacity) {
    if (t64_test_schedule_synthetic(
            value.machine,
            UINT64_C(1000) + (uint64_t)value.machine->queue_count,
            T64_TEST_EVENT_NOOP, UINT64_C(0)) != T64_STATUS_OK) {
      passed = false;
      break;
    }
  }
  if (t64_machine_bus_write(value.machine, TEST_SP_BASE, 4u,
                            UINT32_C(0x1020)) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 4u, 4u,
                            UINT32_C(0x200)) != T64_STATUS_OK ||
      t64_machine_digest(value.machine, digest_before) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 8u, 4u,
                            UINT32_C(8)) != T64_STATUS_QUEUE_FULL ||
      t64_machine_digest(value.machine, digest_after) != T64_STATUS_OK ||
      memcmp(digest_before, digest_after, sizeof digest_before) != 0 ||
      value.machine->queue_count !=
          (size_t)value.machine->profile.event_queue_capacity ||
      memcmp(&active_before, &value.machine->sp.active,
             sizeof active_before) != 0 ||
      value.machine->sp.pending.valid ||
      t64_machine_bus_read(value.machine, TEST_SP_BASE + 8u, 4u, &latch) !=
          T64_STATUS_OK ||
      latch != 0u ||
      t64_machine_bus_read(value.machine, TEST_SP_BASE + 0x18u, 4u, &busy) !=
          T64_STATUS_OK ||
      busy != UINT32_C(1)) {
    passed = false;
  }
  if (!advance_to(value.machine, UINT64_C(13)) ||
      value.machine->sp.active.valid || value.machine->sp.pending.valid ||
      t64_machine_bus_read(value.machine, TEST_SP_BASE + 0x18u, 4u, &busy) !=
          T64_STATUS_OK ||
      busy != 0u) {
    passed = false;
  }
  t64_machine_destroy(value.machine);
  return passed;
}

static bool token_sequence_and_deadline_refusal_test(void) {
  fixture value;
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  uint32_t latch = UINT32_MAX;
  bool passed = true;

  if (!fixture_init(&value)) {
    return false;
  }
  value.machine->sp.token_exhausted = true;
  if (t64_machine_bus_write(value.machine, TEST_SP_BASE, 4u, 0u) !=
          T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 4u, 4u,
                            UINT32_C(0x100)) != T64_STATUS_OK ||
      t64_machine_digest(value.machine, before) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 8u, 4u,
                            UINT32_C(8)) != T64_STATUS_RESOURCE_EXHAUSTED ||
      t64_machine_digest(value.machine, after) != T64_STATUS_OK ||
      memcmp(before, after, sizeof before) != 0 ||
      t64_machine_bus_read(value.machine, TEST_SP_BASE + 8u, 4u, &latch) !=
          T64_STATUS_OK ||
      latch != 0u || value.machine->queue_count != 0u ||
      value.machine->sp.active.valid) {
    passed = false;
  }
  t64_machine_destroy(value.machine);

  if (!fixture_init(&value)) {
    return false;
  }
  value.machine->sequence_exhausted = true;
  if (t64_machine_bus_write(value.machine, TEST_SP_BASE, 4u, 0u) !=
          T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 4u, 4u,
                            UINT32_C(0x100)) != T64_STATUS_OK ||
      t64_machine_digest(value.machine, before) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 8u, 4u,
                            UINT32_C(8)) != T64_STATUS_SEQUENCE_EXHAUSTED ||
      t64_machine_digest(value.machine, after) != T64_STATUS_OK ||
      memcmp(before, after, sizeof before) != 0 ||
      t64_machine_bus_read(value.machine, TEST_SP_BASE + 8u, 4u, &latch) !=
          T64_STATUS_OK ||
      latch != 0u || value.machine->queue_count != 0u ||
      value.machine->sp.active.valid) {
    passed = false;
  }
  t64_machine_destroy(value.machine);

  if (!fixture_init(&value)) {
    return false;
  }
  value.machine->tick = UINT64_MAX - UINT64_C(12);
  if (t64_machine_bus_write(value.machine, TEST_SP_BASE, 4u, 0u) !=
          T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 4u, 4u,
                            UINT32_C(0x100)) != T64_STATUS_OK ||
      t64_machine_digest(value.machine, before) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 8u, 4u, 0u) !=
          T64_STATUS_TICK_OVERFLOW ||
      t64_machine_digest(value.machine, after) != T64_STATUS_OK ||
      memcmp(before, after, sizeof before) != 0 ||
      t64_machine_bus_read(value.machine, TEST_SP_BASE + 8u, 4u, &latch) !=
          T64_STATUS_OK ||
      latch != 0u || value.machine->queue_count != 0u ||
      value.machine->sp.active.valid) {
    passed = false;
  }
  t64_machine_destroy(value.machine);
  return passed;
}

static bool replacement_invalidates_pending_sp_events_test(void) {
  fixture value;
  t64_event stale = {0};
  uint8_t replacement[4096];
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  bool found_pending = false;
  bool passed = true;
  size_t i;

  if (!fixture_init(&value)) {
    return false;
  }
  if (t64_machine_bus_write(value.machine, TEST_SP_BASE, 4u, 0u) !=
          T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 4u, 4u,
                            UINT32_C(0x100)) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 8u, 4u, 0u) !=
          T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE, 4u,
                            UINT32_C(0x1020)) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 4u, 4u,
                            UINT32_C(0x200)) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 8u, 4u,
                            UINT32_C(8)) != T64_STATUS_OK) {
    passed = false;
  }
  for (i = 0u; i < value.machine->queue_count; i++) {
    size_t index = (value.machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY;
    const t64_event *event = &value.machine->events[index];

    if (event->device == T64_DEVICE_SP &&
        event->token == value.machine->sp.pending.token) {
      stale = *event;
      found_pending = true;
    }
  }
  if (!found_pending || !value.machine->sp.pending.valid) {
    passed = false;
  }
  for (i = 0u; i < sizeof replacement; i++) {
    replacement[i] = (uint8_t)((i * 17u + 23u) & 0xffu);
  }
  replacement[0] = 0x80u;
  replacement[1] = 0x37u;
  replacement[2] = 0x12u;
  replacement[3] = 0x40u;
  replacement[0x3eu] = (uint8_t)'E';
  if (t64_machine_admit_rom(value.machine, replacement,
                            sizeof replacement) != T64_STATUS_OK ||
      value.machine->queue_count != 0u ||
      value.machine->sp.active.valid || value.machine->sp.pending.valid ||
      value.machine->sp.next_token != UINT64_C(1) ||
      t64_machine_digest(value.machine, before) != T64_STATUS_OK ||
      t64_sp_complete(value.machine, &stale) != T64_STATUS_OK ||
      t64_machine_digest(value.machine, after) != T64_STATUS_OK ||
      memcmp(before, after, sizeof before) != 0 ||
      !advance_to(value.machine, UINT64_C(27)) ||
      !bytes_are_zero(value.machine, TEST_DMEM_BASE, 8u) ||
      !bytes_are_zero(value.machine, UINT32_C(0x04001020), 16u)) {
    passed = false;
  }
  t64_machine_destroy(value.machine);
  return passed;
}

static bool equal_tick_pi_sp_fifo_test(void) {
  fixture value;
  uint8_t copied_sp[8];
  t64_trace_record records[T64_TRACE_RECORD_CAPACITY];
  uint32_t pending = 0u;
  size_t record_count = 0u;
  size_t pi_completion = SIZE_MAX;
  size_t sp_completion = SIZE_MAX;
  uint64_t dropped = 0u;
  bool saturated = false;
  bool passed = true;
  size_t i;

  if (!fixture_init(&value)) {
    return false;
  }
  (void)t64_machine_trace_set_enabled(value.machine, true);
  for (i = 0u; i < sizeof copied_sp; i++) {
    copied_sp[i] = (uint8_t)(UINT8_C(0xb0) + (uint8_t)i);
    if (t64_machine_bus_write(value.machine, UINT32_C(0x200) +
                                                 (uint32_t)i,
                              1u, (uint32_t)copied_sp[i]) != T64_STATUS_OK) {
      passed = false;
    }
  }
  if (t64_machine_bus_write(value.machine, UINT32_C(0x04600000), 4u,
                            UINT32_C(0x100)) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, UINT32_C(0x04600004), 4u,
                            UINT32_C(0x10000000)) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, UINT32_C(0x0460000c), 4u,
                            UINT32_C(23)) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE, 4u, 0u) !=
          T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 4u, 4u,
                            UINT32_C(0x200)) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 8u, 4u, 0u) !=
          T64_STATUS_OK ||
      !advance_to(value.machine, UINT64_C(13)) ||
      !bytes_equal(value.machine, TEST_DMEM_BASE, copied_sp,
                   sizeof copied_sp) ||
      t64_machine_bus_read(value.machine, UINT32_C(0x04300008), 4u,
                           &pending) != T64_STATUS_OK ||
      pending != UINT32_C(0x10) ||
      t64_machine_trace_drain(value.machine, records,
                              T64_TRACE_RECORD_CAPACITY, &record_count,
                              &dropped, &saturated) != T64_STATUS_OK ||
      dropped != 0u || saturated) {
    passed = false;
  }
  for (i = 0u; i < record_count; i++) {
    if (records[i].tick == UINT64_C(13) &&
        records[i].device == T64_TRACE_DEVICE_PI &&
        records[i].kind == T64_TRACE_KIND_PI_COMPLETE) {
      pi_completion = i;
    }
    if (records[i].tick == UINT64_C(13) &&
        records[i].device == T64_TRACE_DEVICE_SP &&
        records[i].kind == T64_TRACE_KIND_SP_COMPLETE) {
      sp_completion = i;
    }
  }
  if (pi_completion == SIZE_MAX || sp_completion == SIZE_MAX ||
      pi_completion >= sp_completion || (pending & UINT32_C(1)) != 0u) {
    passed = false;
  }
  t64_machine_destroy(value.machine);
  return passed;
}

static bool digest_changes_after_u32(t64_machine *machine, uint32_t *field) {
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  uint8_t restored[T64_DIGEST_SIZE];
  uint32_t saved = *field;
  bool passed = t64_machine_digest(machine, before) == T64_STATUS_OK;

  *field = saved ^ UINT32_C(1);
  passed = passed &&
           t64_machine_digest(machine, after) == T64_STATUS_OK &&
           memcmp(before, after, sizeof before) != 0;
  *field = saved;
  passed = passed &&
           t64_machine_digest(machine, restored) == T64_STATUS_OK &&
           memcmp(before, restored, sizeof before) == 0;
  return passed;
}

static bool digest_changes_after_u64(t64_machine *machine, uint64_t *field) {
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  uint8_t restored[T64_DIGEST_SIZE];
  uint64_t saved = *field;
  bool passed = t64_machine_digest(machine, before) == T64_STATUS_OK;

  *field = saved ^ UINT64_C(1);
  passed = passed &&
           t64_machine_digest(machine, after) == T64_STATUS_OK &&
           memcmp(before, after, sizeof before) != 0;
  *field = saved;
  passed = passed &&
           t64_machine_digest(machine, restored) == T64_STATUS_OK &&
           memcmp(before, restored, sizeof before) == 0;
  return passed;
}

static bool digest_changes_after_bool(t64_machine *machine, bool *field) {
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  uint8_t restored[T64_DIGEST_SIZE];
  bool saved = *field;
  bool passed = t64_machine_digest(machine, before) == T64_STATUS_OK;

  *field = !saved;
  passed = passed &&
           t64_machine_digest(machine, after) == T64_STATUS_OK &&
           memcmp(before, after, sizeof before) != 0;
  *field = saved;
  passed = passed &&
           t64_machine_digest(machine, restored) == T64_STATUS_OK &&
           memcmp(before, restored, sizeof before) == 0;
  return passed;
}

static bool digest_changes_after_u8(t64_machine *machine, uint8_t *field) {
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  uint8_t restored[T64_DIGEST_SIZE];
  uint8_t saved = *field;
  bool passed = t64_machine_digest(machine, before) == T64_STATUS_OK;

  *field = (uint8_t)(saved ^ UINT8_C(1));
  passed = passed &&
           t64_machine_digest(machine, after) == T64_STATUS_OK &&
           memcmp(before, after, sizeof before) != 0;
  *field = saved;
  passed = passed &&
           t64_machine_digest(machine, restored) == T64_STATUS_OK &&
           memcmp(before, restored, sizeof before) == 0;
  return passed;
}

static bool digest_changes_after_u16(t64_machine *machine, uint16_t *field) {
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  uint8_t restored[T64_DIGEST_SIZE];
  uint16_t saved = *field;
  bool passed = t64_machine_digest(machine, before) == T64_STATUS_OK;

  *field = (uint16_t)(saved ^ UINT16_C(1));
  passed = passed &&
           t64_machine_digest(machine, after) == T64_STATUS_OK &&
           memcmp(before, after, sizeof before) != 0;
  *field = saved;
  passed = passed &&
           t64_machine_digest(machine, restored) == T64_STATUS_OK &&
           memcmp(before, restored, sizeof before) == 0;
  return passed;
}

static bool rsp_fields_participate_in_digest_test(void) {
  fixture value;
  uint32_t *u32_fields[6];
  uint64_t *u64_fields[13];
  uint16_t *u16_fields[2];
  uint8_t *u8_fields[2];
  bool *bool_fields[3];
  size_t u32_count = 0u;
  size_t u64_count = 0u;
  size_t u16_count = 0u;
  size_t u8_count = 0u;
  size_t bool_count = 0u;
  size_t i;
  bool passed = fixture_create_profile(
      &value, "ntsc-cart-4m-openipl3-rsp-functional-v1");

  if (!passed) {
    return false;
  }
  if (!fixture_enable_synthetic_image(&value)) {
    t64_machine_destroy(value.machine);
    return false;
  }
  u32_fields[u32_count++] = &value.machine->rsp.gpr[0];
  u32_fields[u32_count++] = &value.machine->rsp.pc;
  u32_fields[u32_count++] = &value.machine->rsp.delay_target;
  u32_fields[u32_count++] = &value.machine->rsp.divider_input;
  u32_fields[u32_count++] = &value.machine->rsp.divider_result;
  u32_fields[u32_count++] = &value.machine->rsp.sp_status;
  u16_fields[u16_count++] = &value.machine->rsp.vco;
  u16_fields[u16_count++] = &value.machine->rsp.vcc;
  u64_fields[u64_count++] = &value.machine->rsp.accumulator[0];
  u64_fields[u64_count++] = &value.machine->rsp.scalar_ready_tick[0];
  u64_fields[u64_count++] = &value.machine->rsp.vector_ready_tick[0];
  u64_fields[u64_count++] = &value.machine->rsp.accumulator_ready_tick[0];
  u64_fields[u64_count++] = &value.machine->rsp.next_issue_tick;
  u64_fields[u64_count++] = &value.machine->rsp.issue_sequence;
  u64_fields[u64_count++] = &value.machine->rsp.retired_instructions;
  u64_fields[u64_count++] = &value.machine->rsp.stall_slots;
  u64_fields[u64_count++] = &value.machine->rsp.dma_beat_cursor;
  u64_fields[u64_count++] = &value.machine->rsp.dma_setup_remaining;
  u64_fields[u64_count++] = &value.machine->rsp.active_dma_token;
  u64_fields[u64_count++] = &value.machine->rsp.pending_dma_token;
  u64_fields[u64_count++] = &value.machine->rsp.dma_event_sequence;
  u8_fields[u8_count++] = &value.machine->rsp.vector[0][0];
  u8_fields[u8_count++] = &value.machine->rsp.vce;
  bool_fields[bool_count++] = &value.machine->rsp.delay_slot_pending;
  bool_fields[bool_count++] = &value.machine->rsp.divider_high_valid;
  bool_fields[bool_count++] = &value.machine->rsp.sp_semaphore;
  for (i = 0u; i < u32_count; i++) {
    passed = digest_changes_after_u32(value.machine, u32_fields[i]) && passed;
  }
  for (i = 0u; i < u64_count; i++) {
    passed = digest_changes_after_u64(value.machine, u64_fields[i]) && passed;
  }
  for (i = 0u; i < u16_count; i++) {
    passed = digest_changes_after_u16(value.machine, u16_fields[i]) && passed;
  }
  for (i = 0u; i < u8_count; i++) {
    passed = digest_changes_after_u8(value.machine, u8_fields[i]) && passed;
  }
  for (i = 0u; i < bool_count; i++) {
    passed = digest_changes_after_bool(value.machine, bool_fields[i]) &&
             passed;
  }
  t64_machine_destroy(value.machine);
  return passed;
}

static bool sp_latches_participate_in_digest_test(void) {
  fixture value;
  uint32_t *u32_fields[22];
  uint64_t *u64_fields[7];
  bool *bool_fields[8];
  size_t u32_count = 0u;
  size_t u64_count = 0u;
  size_t bool_count = 0u;
  size_t i;
  bool passed = true;

  if (!fixture_init(&value)) {
    return false;
  }
  if (t64_machine_bus_write(value.machine, TEST_SP_BASE, 4u, 0u) !=
          T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 4u, 4u,
                            UINT32_C(0x100)) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 8u, 4u, 0u) !=
          T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE, 4u,
                            UINT32_C(0x1021)) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 4u, 4u,
                            UINT32_C(0x200)) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 12u, 4u,
                            UINT32_C(8)) != T64_STATUS_OK ||
      !value.machine->sp.active.valid || !value.machine->sp.pending.valid) {
    t64_machine_destroy(value.machine);
    return false;
  }

  u32_fields[u32_count++] = &value.machine->sp.mem_address;
  u32_fields[u32_count++] = &value.machine->sp.dram_address;
  u32_fields[u32_count++] = &value.machine->sp.read_length;
  u32_fields[u32_count++] = &value.machine->sp.write_length;
  u32_fields[u32_count++] = &value.machine->sp.active.raw_mem_address;
  u32_fields[u32_count++] = &value.machine->sp.active.raw_dram_address;
  u32_fields[u32_count++] = &value.machine->sp.active.raw_length;
  u32_fields[u32_count++] = &value.machine->sp.active.aligned_dram_start;
  u32_fields[u32_count++] = &value.machine->sp.active.internal_start;
  u32_fields[u32_count++] = &value.machine->sp.active.bytes_per_line;
  u32_fields[u32_count++] = &value.machine->sp.active.line_count;
  u32_fields[u32_count++] = &value.machine->sp.active.skip;
  u32_fields[u32_count++] = &value.machine->sp.active.payload_bytes;
  u32_fields[u32_count++] = &value.machine->sp.pending.raw_mem_address;
  u32_fields[u32_count++] = &value.machine->sp.pending.raw_dram_address;
  u32_fields[u32_count++] = &value.machine->sp.pending.raw_length;
  u32_fields[u32_count++] = &value.machine->sp.pending.aligned_dram_start;
  u32_fields[u32_count++] = &value.machine->sp.pending.internal_start;
  u32_fields[u32_count++] = &value.machine->sp.pending.bytes_per_line;
  u32_fields[u32_count++] = &value.machine->sp.pending.line_count;
  u32_fields[u32_count++] = &value.machine->sp.pending.skip;
  u32_fields[u32_count++] = &value.machine->sp.pending.payload_bytes;
  u64_fields[u64_count++] = &value.machine->sp.active.start_tick;
  u64_fields[u64_count++] = &value.machine->sp.active.end_tick;
  u64_fields[u64_count++] = &value.machine->sp.active.token;
  u64_fields[u64_count++] = &value.machine->sp.pending.start_tick;
  u64_fields[u64_count++] = &value.machine->sp.pending.end_tick;
  u64_fields[u64_count++] = &value.machine->sp.pending.token;
  u64_fields[u64_count++] = &value.machine->sp.next_token;
  bool_fields[bool_count++] = &value.machine->sp.active.valid;
  bool_fields[bool_count++] = &value.machine->sp.active.dram_to_sp;
  bool_fields[bool_count++] = &value.machine->sp.active.imem;
  bool_fields[bool_count++] = &value.machine->sp.pending.valid;
  bool_fields[bool_count++] = &value.machine->sp.pending.dram_to_sp;
  bool_fields[bool_count++] = &value.machine->sp.pending.imem;
  bool_fields[bool_count++] = &value.machine->sp.token_exhausted;
  for (i = 0u; i < u32_count; i++) {
    passed = digest_changes_after_u32(value.machine, u32_fields[i]) && passed;
  }
  for (i = 0u; i < u64_count; i++) {
    passed = digest_changes_after_u64(value.machine, u64_fields[i]) && passed;
  }
  for (i = 0u; i < bool_count; i++) {
    passed = digest_changes_after_bool(value.machine, bool_fields[i]) &&
             passed;
  }
  t64_machine_destroy(value.machine);
  return passed;
}

static int rsp_sp_pc_resume_tap_test(void) {
  static const uint32_t program[] = {
      UINT32_C(0x24010007), /* ADDIU r1,r0,7 */
      UINT32_C(0x0000000d), /* BREAK */
      UINT32_C(0x24020009), /* ADDIU r2,r0,9 */
      UINT32_C(0x0000000d)  /* BREAK */
  };
  fixture value;
  t64_status status;
  uint32_t pc = 0u;
  size_t i;
  bool setup_ok = fixture_create_profile(
      &value, "ntsc-cart-4m-openipl3-rsp-functional-v1");

  if (setup_ok) {
    setup_ok = fixture_enable_synthetic_image(&value);
  }
  for (i = 0u; setup_ok && i < sizeof program / sizeof program[0]; i++) {
    setup_ok = t64_machine_bus_write(
                   value.machine, TEST_IMEM_BASE + (uint32_t)(i * 4u), 4u,
                   program[i]) == T64_STATUS_OK;
  }
  if (!setup_ok || t64_rsp_start(value.machine) != T64_STATUS_OK ||
      !advance_to(value.machine, UINT64_C(2)) ||
      !value.machine->rsp_observation_valid ||
      value.machine->rsp_observed_pc != UINT32_C(4) ||
      value.machine->rsp_observed_opcode != UINT32_C(0x0000000d) ||
      value.machine->rsp_observed_outcome != T64_RSP_ATTEMPT_RETIRED ||
      (value.machine->rsp.sp_status & UINT32_C(3)) != UINT32_C(3)) {
    if (value.machine != NULL) {
      t64_machine_destroy(value.machine);
    }
    fputs("TAP version 13\nBail out! fetched BREAK precondition failed\n",
          stdout);
    return 2;
  }

  status = t64_machine_bus_write(value.machine, UINT32_C(0x04080000), 4u,
                                 UINT32_C(0x00001008));
  if (status == T64_STATUS_OK) {
    status = t64_machine_bus_read(value.machine, UINT32_C(0x04080000), 4u,
                                  &pc);
  }
  if (status == T64_STATUS_OK && pc == UINT32_C(8)) {
    status = t64_machine_bus_write(value.machine,
                                   TEST_SP_BASE + UINT32_C(0x10), 4u,
                                   UINT32_C(0x5));
  }
  if (status == T64_STATUS_OK) {
    setup_ok = advance_to(value.machine, UINT64_C(3)) &&
               value.machine->rsp_observation_valid &&
               value.machine->rsp_observed_pc == UINT32_C(8) &&
               value.machine->rsp_observed_opcode == UINT32_C(0x24020009) &&
               value.machine->rsp_observed_outcome ==
                   T64_RSP_ATTEMPT_RETIRED &&
               value.machine->rsp.gpr[2] == UINT32_C(9);
  } else {
    setup_ok = false;
  }

  fputs("TAP version 13\n", stdout);
  if (setup_ok) {
    fputs("ok 1 - SP_PC masks writes and resumes fetched IMEM after BREAK\n",
          stdout);
  } else {
    printf("not ok 1 - SP_PC masks writes and resumes fetched IMEM after BREAK\n"
           "# expected status=0 pc=0x00000008 fetched=(pc=0x008 opcode=0x24020009) r2=9\n"
           "# actual status=%u pc=0x%08x fetched=(pc=0x%03x opcode=0x%08x outcome=%u) r2=%u\n",
           (unsigned)status, pc, value.machine->rsp_observed_pc,
           value.machine->rsp_observed_opcode,
           value.machine->rsp_observed_outcome, value.machine->rsp.gpr[2]);
  }
  fputs("1..1\n", stdout);
  t64_machine_destroy(value.machine);
  return setup_ok ? 0 : 1;
}

int main(int argc, char **argv) {
  static const char *const names[] = {
      "SP source is read at the 13-tick completion deadline",
      "SP rectangular rows honor skip, alignment, and IMEM selection",
      "SP write wraps within the selected DMEM bank",
      "SP validates the final valid and first invalid DRAM rows",
      "SP maximum row/count fields produce a checked profile deadline",
      "SP pending requests survive full queue pressure and source mutation",
      "SP latches participate in the canonical digest schema",
      "SP refuses pending work without capacity or sequence reservation",
      "SP token, sequence, and deadline exhaustion refuse atomically",
      "ROM replacement invalidates active and pending SP completions",
      "same-tick PI and SP completions retain scheduler FIFO order",
      "new RSP profile is admitted with its distinct model and state domain",
      "RSP future state fields participate in state v12 digest",
      "fetched scalar/VU/DMEM path retires through BREAK",
      "unsupported fetched opcode refuses atomically with excluded diagnostics",
      "historical profile refuses RSP startup",
      "RSP BREAK leaves queued PI DMA and machine advancement running",
      "SP DMEM SW invalidates overlapping seeded LL/LLD links only"};
  bool results[sizeof names / sizeof names[0]];
  size_t i;
  bool all_passed = true;
  if (argc == 2 && strcmp(argv[1], "--rsp-tdd-control") == 0) {
    return rsp_sp_pc_resume_tap_test();
  }
  bool rsp_only = argc == 2 && strcmp(argv[1], "--rsp-tracer") == 0;
  size_t test_count = sizeof names / sizeof names[0];

  if (argc > 1 && !rsp_only) {
    fputs("usage: test_sp_dma [--rsp-tracer]\n", stderr);
    return 2;
  }

  if (rsp_only) {
    bool admission_passed = rsp_profile_admission_test();
    bool digest_passed = rsp_fields_participate_in_digest_test();
    bool tracer_passed = rsp_fetched_vadd_test();
    bool refusal_passed = rsp_refuses_unknown_opcode_atomically_test();
    bool old_profile_passed = rsp_old_profile_refusal_test();
    bool break_passed = rsp_break_allows_queued_pi_completion_test();
    bool link_passed =
        rsp_store_preserves_only_nonoverlapping_seeded_links_test();
    puts("1..7");
    printf("%s 1 - %s\n", admission_passed ? "ok" : "not ok", names[11]);
    printf("%s 2 - %s\n", digest_passed ? "ok" : "not ok", names[12]);
    printf("%s 3 - %s\n", tracer_passed ? "ok" : "not ok", names[13]);
    printf("%s 4 - %s\n", refusal_passed ? "ok" : "not ok", names[14]);
    printf("%s 5 - %s\n", old_profile_passed ? "ok" : "not ok", names[15]);
    printf("%s 6 - %s\n", break_passed ? "ok" : "not ok", names[16]);
    printf("%s 7 - %s\n", link_passed ? "ok" : "not ok", names[17]);
    all_passed = admission_passed && digest_passed && tracer_passed &&
                 refusal_passed && old_profile_passed && break_passed &&
                 link_passed;
    return all_passed ? 0 : 1;
  }

  results[0] = source_at_completion_test();
  results[1] = rectangular_imem_rows_test();
  results[2] = write_wraps_in_selected_dmem_test();
  results[3] = last_valid_and_first_invalid_row_test();
  results[4] = maximum_sp_fields_have_checked_deadline_test();
  results[5] = pending_queue_and_source_visibility_test();
  results[6] = sp_latches_participate_in_digest_test();
  results[7] = pending_capacity_refusal_test();
  results[8] = token_sequence_and_deadline_refusal_test();
  results[9] = replacement_invalidates_pending_sp_events_test();
  results[10] = equal_tick_pi_sp_fifo_test();
  results[11] = rsp_profile_admission_test();
  results[12] = rsp_fields_participate_in_digest_test();
  results[13] = rsp_fetched_vadd_test();
  results[14] = rsp_refuses_unknown_opcode_atomically_test();
  results[15] = rsp_old_profile_refusal_test();
  results[16] = rsp_break_allows_queued_pi_completion_test();
  results[17] = rsp_store_preserves_only_nonoverlapping_seeded_links_test();
  printf("1..%zu\n", test_count);
  for (i = 0u; i < test_count; i++) {
    printf("%s %zu - %s\n", results[i] ? "ok" : "not ok", i + 1u,
           names[i]);
    all_passed = all_passed && results[i];
  }
  if (!results[0]) {
    printf("# SP register statuses: mem=%u dram=%u length=%u\n",
           (unsigned)observed_mem_status, (unsigned)observed_dram_status,
           (unsigned)observed_length_status);
  }
  return all_passed ? 0 : 1;
}
