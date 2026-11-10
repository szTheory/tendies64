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

static bool fixture_init(fixture *value) {
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
  if (t64_profile_lookup("ntsc-cart-4m-skeleton", &profile) !=
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

static bool pending_queue_and_source_visibility_test(void) {
  fixture value;
  t64_sp_dma_descriptor active_before;
  t64_sp_dma_descriptor pending_before;
  uint8_t pending_bytes[16];
  uint32_t status = 0u;
  uint32_t length_latch = 0u;
  size_t allocations_before;
  size_t releases_before;
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
      status != UINT32_C(1)) {
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
                            UINT32_C(0x300)) != T64_STATUS_OK ||
      t64_machine_bus_write(value.machine, TEST_SP_BASE + 12u, 4u,
                            UINT32_C(8)) !=
          T64_STATUS_BUS_DEVICE_REFUSED ||
      value.machine->queue_count !=
          (size_t)value.machine->profile.event_queue_capacity ||
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
      status != UINT32_C(1)) {
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
      t64_machine_bus_read(value.machine, UINT32_C(0x04300008), 4u,
                           &status) != T64_STATUS_OK ||
      status != 0u || value.allocations != allocations_before ||
      value.releases != releases_before) {
    passed = false;
  }
  t64_machine_destroy(value.machine);
  return passed;
}

static bool sp_latches_participate_in_digest_test(void) {
  fixture value;
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  bool passed;

  if (!fixture_init(&value)) {
    return false;
  }
  passed = t64_machine_digest(value.machine, before) == T64_STATUS_OK &&
           t64_machine_bus_write(value.machine, TEST_SP_BASE, 4u,
                                 UINT32_C(0x00001237)) == T64_STATUS_OK &&
           t64_machine_digest(value.machine, after) == T64_STATUS_OK &&
           memcmp(before, after, sizeof before) != 0;
  t64_machine_destroy(value.machine);
  return passed;
}

int main(void) {
  static const char *const names[] = {
      "SP source is read at the 13-tick completion deadline",
      "SP rectangular rows honor skip, alignment, and IMEM selection",
      "SP write wraps within the selected DMEM bank",
      "SP validates the final valid and first invalid DRAM rows",
      "SP pending requests survive full queue pressure and source mutation",
      "SP latches participate in the canonical digest schema"};
  bool results[sizeof names / sizeof names[0]];
  size_t i;
  bool all_passed = true;

  results[0] = source_at_completion_test();
  results[1] = rectangular_imem_rows_test();
  results[2] = write_wraps_in_selected_dmem_test();
  results[3] = last_valid_and_first_invalid_row_test();
  results[4] = pending_queue_and_source_visibility_test();
  results[5] = sp_latches_participate_in_digest_test();
  puts("1..6");
  for (i = 0u; i < sizeof names / sizeof names[0]; i++) {
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
