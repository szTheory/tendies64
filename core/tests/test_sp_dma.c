#include "t64_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_SP_BASE UINT32_C(0x04040000)
#define TEST_DMEM_BASE UINT32_C(0x04000000)

typedef struct {
  t64_host_allocator allocator;
  t64_machine *machine;
} fixture;

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  void *memory;

  (void)context;
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
  return true;
}

static void test_release(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static bool fixture_init(fixture *value) {
  const t64_profile_descriptor *profile = NULL;

  memset(value, 0, sizeof *value);
  value->allocator.allocate = test_allocate;
  value->allocator.release = test_release;
  if (t64_profile_lookup("ntsc-cart-4m-skeleton", &profile) !=
          T64_STATUS_OK ||
      profile == NULL ||
      t64_machine_create(&value->allocator, profile, &value->machine) !=
          T64_STATUS_OK ||
      value->machine == NULL) {
    return false;
  }
  return true;
}

static bool source_at_completion_test(void) {
  fixture value;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(12)};
  t64_stop_result result = {0u, 0u};
  uint8_t expected[8];
  uint32_t byte = 0u;
  size_t i;
  bool passed = true;

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
  if (t64_machine_bus_write(value.machine, TEST_SP_BASE, 4u,
                            UINT32_C(0x00000003)) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 4u, 4u,
                            UINT32_C(0x00000107)) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 8u, 4u,
                            UINT32_C(0)) != T64_STATUS_OK) {
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

int main(void) {
  bool passed = source_at_completion_test();

  puts("1..1");
  printf("%s 1 - SP source is read at the 13-tick completion deadline\n",
         passed ? "ok" : "not ok");
  return passed ? 0 : 1;
}
