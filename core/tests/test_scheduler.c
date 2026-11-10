#line 1 "core/tests/test_scheduler.c"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "t64_test.h"
#include "tendies64/t64_machine.h"

#define TEST_ROM_SIZE 4096u

typedef struct {
  size_t allocations;
  size_t releases;
  size_t outstanding;
} allocator_state;

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  allocator_state *state = (allocator_state *)context;
  void *memory;

  if (state == NULL || storage_out == NULL || alignment == 0u) {
    return false;
  }
  memory = malloc(size);
  if (memory == NULL || ((uintptr_t)memory % alignment) != 0u) {
    free(memory);
    return false;
  }
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  state->allocations++;
  state->outstanding++;
  return true;
}

static void test_release(void *context, t64_host_storage storage) {
  allocator_state *state = (allocator_state *)context;

  if (state != NULL && storage.data != NULL) {
    state->releases++;
    state->outstanding--;
    free(storage.data);
  }
}

static void fill_authored_rom(uint8_t image[TEST_ROM_SIZE]) {
  size_t i;

  for (i = 0u; i < TEST_ROM_SIZE; i++) {
    image[i] = (uint8_t)((i * 31u + 11u) & 0xffu);
  }
  image[0] = 0x80u;
  image[1] = 0x37u;
  image[2] = 0x12u;
  image[3] = 0x40u;
}

static void test_unattainable_instruction_bound_reports_quiescence(void) {
  static const uint32_t test_stop_instruction_count = UINT32_C(2);
  static const uint32_t test_reason_quiescent = UINT32_C(5);
  uint8_t image[TEST_ROM_SIZE];
  const t64_profile_descriptor *profile = NULL;
  allocator_state host_state = {0u, 0u, 0u};
  const t64_host_allocator allocator = {test_allocate, test_release,
                                        &host_state};
  t64_machine *machine = NULL;
  t64_stop_request request = {test_stop_instruction_count, UINT64_C(1)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  size_t allocations_before_advance;
  size_t releases_before_advance;
  t64_status status;

  fill_authored_rom(image);
  T64_CHECK_INT(t64_profile_lookup("ntsc-cart-4m-skeleton", &profile),
                T64_STATUS_OK);
  T64_CHECK(profile != NULL);
  if (profile == NULL) {
    return;
  }
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  T64_CHECK(machine != NULL);
  if (machine == NULL) {
    return;
  }
  T64_CHECK_INT(t64_machine_admit_rom(machine, image, sizeof image),
                T64_STATUS_OK);

  allocations_before_advance = host_state.allocations;
  releases_before_advance = host_state.releases;
  status = t64_machine_advance(machine, &request, &result);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(result.reason, test_reason_quiescent);
  T64_CHECK_INT(result.tick, UINT64_C(0));
  T64_CHECK_INT(host_state.allocations, allocations_before_advance);
  T64_CHECK_INT(host_state.releases, releases_before_advance);

  t64_machine_destroy(machine);
  T64_CHECK_INT(host_state.outstanding, 0u);
}

int main(void) {
  test_unattainable_instruction_bound_reports_quiescence();
  return T64_TEST_FINISH("unit.core.scheduler");
}
