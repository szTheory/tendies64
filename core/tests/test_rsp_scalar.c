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
} rsp_scalar_fixture;

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

static bool fixture_create(rsp_scalar_fixture *fixture) {
  const t64_profile_descriptor *profile = NULL;
  t64_host_storage image = {NULL, 0u, 0u};
  t64_status status;

  if (fixture == NULL) {
    return false;
  }
  (void)memset(fixture, 0, sizeof *fixture);
  fixture->allocator.allocate = fixture_allocate;
  fixture->allocator.release = fixture_release;
  status = t64_profile_lookup(
      "ntsc-cart-4m-openipl3-rsp-functional-v1", &profile);
  if (status != T64_STATUS_OK || profile == NULL ||
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

static void fixture_destroy(rsp_scalar_fixture *fixture) {
  if (fixture != NULL && fixture->machine != NULL) {
    t64_machine_destroy(fixture->machine);
    fixture->machine = NULL;
  }
}

static uint32_t encode_i(uint32_t operation, uint32_t rs, uint32_t rt,
                         uint16_t immediate) {
  return (operation << 26) | (rs << 21) | (rt << 16) | immediate;
}

static bool run_first_instruction(rsp_scalar_fixture *fixture,
                                  uint32_t instruction,
                                  uint32_t source_register,
                                  uint32_t source_value,
                                  uint32_t *outcome_out,
                                  uint32_t *pc_out,
                                  uint32_t *result_out) {
  t64_machine *machine;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(1)};
  t64_stop_result result = {0u, 0u};

  if (fixture == NULL || fixture->machine == NULL || outcome_out == NULL ||
      pc_out == NULL || result_out == NULL || source_register >= 32u) {
    return false;
  }
  machine = fixture->machine;
  t64_machine_reset_execution_state(machine);
  (void)memset(&machine->rsp, 0, sizeof machine->rsp);
  (void)memset(&machine->cpu, 0, sizeof machine->cpu);
  machine->rsp.sp_status = T64_RSP_STATUS_HALT;
  machine->rsp.gpr[source_register] = source_value;
  if (t64_machine_bus_write(machine, TEST_IMEM_BASE, 4u, instruction) !=
          T64_STATUS_OK ||
      t64_rsp_start(machine) != T64_STATUS_OK ||
      t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
      result.tick != UINT64_C(1)) {
    return false;
  }
  *outcome_out = machine->rsp_observed_outcome;
  *pc_out = machine->rsp.pc;
  *result_out = machine->rsp.gpr[2];
  return true;
}

static bool addi_sign_wrap_test(rsp_scalar_fixture *fixture) {
  const uint32_t expected = UINT32_C(0x80007fef);
  uint32_t outcome = T64_RSP_ATTEMPT_NONE;
  uint32_t pc = UINT32_MAX;
  uint32_t actual = 0u;
  bool setup_ok;

  setup_ok = run_first_instruction(fixture,
                                   encode_i(UINT32_C(0x08), UINT32_C(1),
                                            UINT32_C(2), UINT16_C(0x7fff)),
                                   UINT32_C(1), UINT32_C(0x7ffffff0),
                                   &outcome, &pc, &actual);
  if (!setup_ok) {
    fprintf(stderr, "RED setup failure: could not execute fetched ADDI\n");
    return false;
  }
  if (outcome != T64_RSP_ATTEMPT_RETIRED || pc != UINT32_C(4) ||
      actual != expected) {
    fprintf(stderr,
            "ADDI fetched semantic assertion failed at PC=0x000: "
            "opcode=0x%08x outcome=%u expected=(retired,pc=0x004,"
            "r2=0x%08x) actual=(%u,pc=0x%03x,r2=0x%08x)\n",
            encode_i(UINT32_C(0x08), UINT32_C(1), UINT32_C(2),
                     UINT16_C(0x7fff)),
            (unsigned)outcome, expected, (unsigned)outcome, (unsigned)pc,
            actual);
    return false;
  }
  return true;
}

int main(void) {
  rsp_scalar_fixture fixture;
  bool passed = fixture_create(&fixture);

  if (!passed) {
    fprintf(stderr, "fixture setup failure\n");
    fixture_destroy(&fixture);
    return 1;
  }
  passed = addi_sign_wrap_test(&fixture);
  fixture_destroy(&fixture);
  if (!passed) {
    return 1;
  }
  puts("RSP scalar ADDI fetched case PASS");
  return 0;
}
