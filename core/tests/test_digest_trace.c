#line 1 "core/tests/test_digest_trace.c"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "t64_internal.h"
#include "t64_test.h"

#define TEST_ROM_SIZE 4096u

typedef struct {
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
  state->outstanding++;
  return true;
}

static void test_release(void *context, t64_host_storage storage) {
  allocator_state *state = (allocator_state *)context;

  if (state != NULL && storage.data != NULL) {
    state->outstanding--;
    free(storage.data);
  }
}

static void fill_authored_rom(uint8_t image[TEST_ROM_SIZE]) {
  size_t i;

  for (i = 0u; i < TEST_ROM_SIZE; i++) {
    image[i] = (uint8_t)((i * 13u + 7u) & 0xffu);
  }
  image[0] = 0x80u;
  image[1] = 0x37u;
  image[2] = 0x12u;
  image[3] = 0x40u;
  image[0x3eu] = (uint8_t)'E';
}

static void test_queued_future_work_changes_digest(void) {
  uint8_t image[TEST_ROM_SIZE];
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  const t64_profile_descriptor *profile = NULL;
  allocator_state host_state = {0u};
  const t64_host_allocator allocator = {test_allocate, test_release,
                                        &host_state};
  t64_machine *machine = NULL;

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
  T64_CHECK_INT(t64_machine_digest(machine, before), T64_STATUS_OK);
  T64_CHECK_INT(t64_test_schedule_synthetic(machine, 9u,
                                            T64_TEST_EVENT_NOOP, 17u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(machine, after), T64_STATUS_OK);
  T64_CHECK(memcmp(before, after, sizeof before) != 0);

  t64_machine_destroy(machine);
  T64_CHECK_INT(host_state.outstanding, 0u);
}

int main(void) {
  (void)puts("evidence: synthetic scheduler metadata only; no CPU or device execution");
  test_queued_future_work_changes_digest();
  return T64_TEST_FINISH("unit.core.digest-trace");
}
