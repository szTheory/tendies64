#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_SP_BASE UINT32_C(0x04040000)
#define TEST_DMEM_BASE UINT32_C(0x04000000)
#define TEST_IMEM_BASE UINT32_C(0x04001000)

typedef struct {
  t64_host_allocator allocator;
  t64_machine *machine;
  size_t allocations;
  size_t releases;
} rsp_control_fixture;

typedef struct {
  bool setup_ok;
  bool unchanged;
  uint32_t pc;
  uint32_t word_before;
  uint32_t word_after;
  uint32_t refusal_reason;
  t64_status write_status;
} active_pc_write_observation;

static bool fixture_allocate(void *context, size_t size, size_t alignment,
                             t64_host_storage *storage_out) {
  rsp_control_fixture *fixture = (rsp_control_fixture *)context;
  void *data;

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
  if (fixture != NULL) {
    fixture->allocations++;
  }
  return true;
}

static void fixture_release(void *context, t64_host_storage storage) {
  rsp_control_fixture *fixture = (rsp_control_fixture *)context;

  if (fixture != NULL) {
    fixture->releases++;
  }
  free(storage.data);
}

static bool fixture_create(rsp_control_fixture *fixture) {
  const t64_profile_descriptor *profile = NULL;
  t64_host_storage image = {NULL, 0u, 0u};
  t64_status status;

  if (fixture == NULL) {
    return false;
  }
  (void)memset(fixture, 0, sizeof *fixture);
  fixture->allocator.allocate = fixture_allocate;
  fixture->allocator.release = fixture_release;
  fixture->allocator.context = fixture;
  status = t64_profile_lookup(
      "ntsc-cart-4m-openipl3-rsp-functional-v1", &profile);
  if (status != T64_STATUS_OK || profile == NULL) {
    fprintf(stderr, "fixture profile lookup failed: status=%u\n",
            (unsigned)status);
    return false;
  }
  status = t64_machine_create(&fixture->allocator, profile,
                              &fixture->machine);
  if (status != T64_STATUS_OK || fixture->machine == NULL) {
    fprintf(stderr, "fixture machine create failed: status=%u\n",
            (unsigned)status);
    return false;
  }
  if (!fixture_allocate(fixture, 2u, 1u, &image)) {
    fprintf(stderr, "fixture synthetic image allocation failed\n");
    t64_machine_destroy(fixture->machine);
    fixture->machine = NULL;
    return false;
  }
  ((uint8_t *)image.data)[0] = UINT8_C(0x5a);
  ((uint8_t *)image.data)[1] = UINT8_C(0xa5);
  fixture->machine->image_storage = image;
  fixture->machine->image_length = 2u;
  fixture->machine->has_image = true;
  return true;
}

static bool advance_to(t64_machine *machine, uint64_t tick) {
  t64_stop_request request = {T64_STOP_GUEST_TICK, tick};
  t64_stop_result result = {0u, 0u};

  return t64_machine_advance(machine, &request, &result) == T64_STATUS_OK &&
         result.tick == tick;
}

static bool active_pc_imem_write_refusal_test(
    active_pc_write_observation *observation) {
  rsp_control_fixture fixture;
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  uint32_t imem_before = 0u;
  uint32_t imem_after = 0u;
  uint32_t refusal_reason = T64_RSP_REFUSAL_NONE;
  t64_status setup_status = T64_STATUS_INVALID_ARGUMENT;
  t64_status write_status = T64_STATUS_INVALID_ARGUMENT;
  t64_status digest_before_status = T64_STATUS_INVALID_ARGUMENT;
  t64_status digest_after_status = T64_STATUS_INVALID_ARGUMENT;
  bool unchanged = false;
  bool setup_ok;

  if (observation == NULL) {
    return false;
  }
  (void)memset(observation, 0, sizeof *observation);
  observation->write_status = T64_STATUS_INVALID_ARGUMENT;
  setup_ok = fixture_create(&fixture);
  if (setup_ok) {
    t64_machine_reset_execution_state(fixture.machine);
    fixture.machine->rsp.sp_status = T64_RSP_STATUS_HALT;
    setup_status =
        t64_machine_bus_write(fixture.machine, TEST_IMEM_BASE, 4u, 0u);
    setup_ok = setup_status == T64_STATUS_OK &&
               t64_machine_bus_write(fixture.machine, TEST_IMEM_BASE + 4u,
                                     4u, UINT32_C(0x24020009)) ==
                   T64_STATUS_OK &&
               t64_machine_bus_read(fixture.machine, TEST_IMEM_BASE + 4u,
                                    4u, &imem_before) == T64_STATUS_OK &&
               t64_rsp_start(fixture.machine) == T64_STATUS_OK &&
               advance_to(fixture.machine, UINT64_C(1)) &&
               fixture.machine->rsp.pc == UINT32_C(4) &&
               (fixture.machine->rsp.sp_status & T64_RSP_STATUS_HALT) == 0u;
  }
  if (setup_ok) {
    observation->pc = fixture.machine->rsp.pc;
    digest_before_status =
        t64_machine_digest(fixture.machine, digest_before);
    write_status = t64_machine_bus_write(
        fixture.machine, TEST_IMEM_BASE + UINT32_C(4), 4u,
        UINT32_C(0x0000000d));
    digest_after_status =
        t64_machine_digest(fixture.machine, digest_after);
    refusal_reason = fixture.machine->rsp_observed_refusal_reason;
    if (t64_machine_bus_read(fixture.machine, TEST_IMEM_BASE + 4u, 4u,
                             &imem_after) == T64_STATUS_OK &&
        digest_before_status == T64_STATUS_OK &&
        digest_after_status == T64_STATUS_OK) {
      unchanged = imem_before == imem_after &&
                  memcmp(digest_before, digest_after, sizeof digest_before) ==
                      0;
    }
  }
  observation->setup_ok = setup_ok;
  observation->word_before = imem_before;
  observation->word_after = imem_after;
  observation->write_status = write_status;
  observation->refusal_reason = refusal_reason;
  observation->unchanged = unchanged;
  if (fixture.machine != NULL) {
    t64_machine_destroy(fixture.machine);
  }
  return setup_ok && write_status == T64_STATUS_BUS_DEVICE_REFUSED &&
         refusal_reason == T64_RSP_REFUSAL_ACTIVE_IMEM_WRITE && unchanged;
}

int main(int argc, char **argv) {
  active_pc_write_observation observation;
  bool passed;

  if (argc == 2 &&
      strcmp(argv[1], "--tdd-active-pc-imem-refusal") == 0) {
    passed = active_pc_imem_write_refusal_test(&observation);
    puts("TAP version 13");
    printf("%s 1 - CPU write to active RSP PC in IMEM refuses atomically\n",
           passed ? "ok" : "not ok");
    if (!passed && observation.setup_ok) {
      printf("# active PC=0x%03x write_status=%u refusal=%u unchanged=%u "
             "word_before=0x%08x word_after=0x%08x\n",
             observation.pc, (unsigned)observation.write_status,
             observation.refusal_reason, observation.unchanged ? 1u : 0u,
             observation.word_before, observation.word_after);
    } else if (!passed) {
      puts("# active-PC refusal fixture setup failed before the write assertion");
    }
    puts("1..1");
    return passed ? 0 : 1;
  }
  if (argc != 1) {
    fputs("usage: test_rsp_control [--tdd-active-pc-imem-refusal]\n",
          stderr);
    return 2;
  }
  passed = active_pc_imem_write_refusal_test(&observation);
  puts("TAP version 13");
  printf("%s 1 - CPU write to active RSP PC in IMEM refuses atomically\n",
         passed ? "ok" : "not ok");
  if (!passed && observation.setup_ok) {
    printf("# active PC=0x%03x write_status=%u refusal=%u unchanged=%u "
           "word_before=0x%08x word_after=0x%08x\n",
           observation.pc, (unsigned)observation.write_status,
           observation.refusal_reason, observation.unchanged ? 1u : 0u,
           observation.word_before, observation.word_after);
  } else if (!passed) {
    puts("# active-PC refusal fixture setup failed before the write assertion");
  }
  puts("1..1");
  return passed ? 0 : 1;
}
