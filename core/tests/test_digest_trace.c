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
  size_t callback_calls;
} allocator_state;

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  allocator_state *state = (allocator_state *)context;
  void *memory;

  if (state == NULL || storage_out == NULL || alignment == 0u) {
    return false;
  }
  state->callback_calls++;
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
    state->callback_calls++;
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

static bool fixture_init(allocator_state *host_state,
                         const t64_host_allocator *allocator,
                         t64_machine **machine_out,
                         uint8_t image[TEST_ROM_SIZE]);

static void test_digest_outputs_reject_machine_and_image_aliases(void) {
  uint8_t image[TEST_ROM_SIZE];
  uint8_t image_prefix[T64_DIGEST_SIZE];
  uint8_t *machine_snapshot;
  allocator_state host_state = {0u};
  const t64_host_allocator allocator = {test_allocate, test_release,
                                        &host_state};
  t64_machine *machine = NULL;
  size_t machine_capacity;

  fill_authored_rom(image);
  T64_CHECK(fixture_init(&host_state, &allocator, &machine, image));
  if (machine == NULL) {
    return;
  }
  machine_capacity = machine->machine_storage.capacity;
  machine_snapshot = (uint8_t *)malloc(machine_capacity);
  T64_CHECK(machine_snapshot != NULL);
  if (machine_snapshot == NULL) {
    t64_machine_destroy(machine);
    return;
  }
  (void)memcpy(machine_snapshot, machine->machine_storage.data,
               machine_capacity);
  T64_CHECK_INT(t64_machine_digest(machine,
                                   (uint8_t *)machine->machine_storage.data),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK_INT(t64_machine_content_digest(
                    machine, (uint8_t *)machine->machine_storage.data),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK(memcmp(machine_snapshot, machine->machine_storage.data,
                   machine_capacity) == 0);

  (void)memcpy(image_prefix, machine->image_storage.data,
               sizeof image_prefix);
  T64_CHECK_INT(t64_machine_digest(machine, machine->image_storage.data),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK_INT(t64_machine_content_digest(machine,
                                           machine->image_storage.data),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK(memcmp(image_prefix, machine->image_storage.data,
                   sizeof image_prefix) == 0);

  free(machine_snapshot);
  t64_machine_destroy(machine);
  T64_CHECK_INT(host_state.outstanding, 0u);
}

static bool fixture_init(allocator_state *host_state,
                         const t64_host_allocator *allocator,
                         t64_machine **machine_out,
                         uint8_t image[TEST_ROM_SIZE]) {
  const t64_profile_descriptor *profile = NULL;

  *machine_out = NULL;
  fill_authored_rom(image);
  if (t64_profile_lookup("ntsc-cart-4m-skeleton", &profile) !=
          T64_STATUS_OK ||
      profile == NULL ||
      t64_machine_create(allocator, profile, machine_out) != T64_STATUS_OK ||
      *machine_out == NULL ||
      t64_machine_admit_rom(*machine_out, image, TEST_ROM_SIZE) !=
          T64_STATUS_OK) {
    t64_machine_destroy(*machine_out);
    *machine_out = NULL;
    return false;
  }
  (void)host_state;
  return true;
}

static bool fixture_init_with_trace(allocator_state *host_state,
                                    const t64_host_allocator *allocator,
                                    t64_machine **machine_out,
                                    uint8_t image[TEST_ROM_SIZE],
                                    bool enable_before_admission) {
  const t64_profile_descriptor *profile = NULL;

  *machine_out = NULL;
  fill_authored_rom(image);
  if (t64_profile_lookup("ntsc-cart-4m-skeleton", &profile) !=
          T64_STATUS_OK ||
      profile == NULL ||
      t64_machine_create(allocator, profile, machine_out) != T64_STATUS_OK ||
      *machine_out == NULL) {
    t64_machine_destroy(*machine_out);
    *machine_out = NULL;
    return false;
  }
  if (enable_before_admission &&
      t64_machine_trace_set_enabled(*machine_out, true) != T64_STATUS_OK) {
    t64_machine_destroy(*machine_out);
    *machine_out = NULL;
    return false;
  }
  if (t64_machine_admit_rom(*machine_out, image, TEST_ROM_SIZE) !=
      T64_STATUS_OK) {
    t64_machine_destroy(*machine_out);
    *machine_out = NULL;
    return false;
  }
  (void)host_state;
  return true;
}

static void check_digest_differs_after_byte_change(t64_machine *machine,
                                                   uint8_t *byte) {
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  uint8_t saved = *byte;

  T64_CHECK_INT(t64_machine_digest(machine, before), T64_STATUS_OK);
  *byte ^= UINT8_C(0x5a);
  T64_CHECK_INT(t64_machine_digest(machine, after), T64_STATUS_OK);
  T64_CHECK(memcmp(before, after, sizeof before) != 0);
  *byte = saved;
}

static void check_digest_differs_after_u32_change(t64_machine *machine,
                                                  uint32_t *field) {
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  uint32_t saved = *field;

  T64_CHECK_INT(t64_machine_digest(machine, before), T64_STATUS_OK);
  *field = saved + 1u;
  T64_CHECK_INT(t64_machine_digest(machine, after), T64_STATUS_OK);
  T64_CHECK(memcmp(before, after, sizeof before) != 0);
  *field = saved;
}

static void check_digest_differs_after_u64_change(t64_machine *machine,
                                                  uint64_t *field) {
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  uint64_t saved = *field;

  T64_CHECK_INT(t64_machine_digest(machine, before), T64_STATUS_OK);
  *field = saved + UINT64_C(1);
  T64_CHECK_INT(t64_machine_digest(machine, after), T64_STATUS_OK);
  T64_CHECK(memcmp(before, after, sizeof before) != 0);
  *field = saved;
}

static void check_digest_differs_after_bool_change(t64_machine *machine,
                                                   bool *field) {
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  bool saved = *field;

  T64_CHECK_INT(t64_machine_digest(machine, before), T64_STATUS_OK);
  *field = !saved;
  T64_CHECK_INT(t64_machine_digest(machine, after), T64_STATUS_OK);
  T64_CHECK(memcmp(before, after, sizeof before) != 0);
  *field = saved;
}

static void test_digest_golden_and_machine_fields(void) {
  uint8_t image[TEST_ROM_SIZE];
  uint8_t digest[T64_DIGEST_SIZE];
  /* Independently assembled SHA-256 of the documented state.v5 byte stream. */
  uint8_t expected[T64_DIGEST_SIZE] = {
      0x68u, 0x13u, 0x73u, 0x9fu, 0xf2u, 0xfcu, 0x1cu, 0xffu,
      0xacu, 0x6du, 0x85u, 0xfau, 0x86u, 0xdfu, 0x0cu, 0x59u,
      0x3eu, 0x30u, 0x35u, 0xf8u, 0xa6u, 0x40u, 0xc6u, 0x42u,
      0xe3u, 0x77u, 0x90u, 0xacu, 0xd9u, 0x6bu, 0x6eu, 0x77u};
  allocator_state host_state = {0u};
  const t64_host_allocator allocator = {test_allocate, test_release,
                                        &host_state};
  t64_machine *machine = NULL;
  size_t saved_image_length;
  size_t event_index;
  t64_event *event;

  T64_CHECK(fixture_init(&host_state, &allocator, &machine, image));
  if (machine == NULL) {
    return;
  }
  T64_CHECK_INT(t64_machine_digest(machine, digest), T64_STATUS_OK);
  T64_CHECK(memcmp(digest, expected, sizeof expected) == 0);

  check_digest_differs_after_byte_change(machine, &machine->profile_digest[0]);
  check_digest_differs_after_byte_change(machine, &machine->content_digest[0]);
  T64_CHECK_INT(t64_machine_digest(machine, digest), T64_STATUS_OK);
  saved_image_length = machine->image_length;
  machine->image_length++;
  T64_CHECK_INT(t64_machine_digest(machine, digest), T64_STATUS_OK);
  T64_CHECK(memcmp(digest, expected, sizeof expected) != 0);
  machine->image_length = saved_image_length;
  machine->has_image = false;
  T64_CHECK_INT(t64_machine_digest(machine, digest), T64_STATUS_NOT_READY);
  machine->has_image = true;
  check_digest_differs_after_byte_change(machine, &machine->rom_header[8]);
  check_digest_differs_after_byte_change(machine, &machine->rom_title[3]);
  check_digest_differs_after_byte_change(machine, &machine->rom_game_code[2]);
  check_digest_differs_after_byte_change(machine, &machine->rom_version);
  check_digest_differs_after_u64_change(machine, &machine->tick);
  check_digest_differs_after_u64_change(machine, &machine->instruction_count);
  check_digest_differs_after_u64_change(machine, &machine->frame_event_count);
  check_digest_differs_after_u32_change(machine,
                                        &machine->sticky_error_status);
  machine->halted = true;
  T64_CHECK_INT(t64_machine_digest(machine, digest), T64_STATUS_OK);
  T64_CHECK(memcmp(digest, expected, sizeof expected) != 0);
  machine->halted = false;
  check_digest_differs_after_byte_change(machine, &machine->ram[37]);
  check_digest_differs_after_u64_change(machine, &machine->next_sequence);
  machine->sequence_exhausted = true;
  T64_CHECK_INT(t64_machine_digest(machine, digest), T64_STATUS_OK);
  T64_CHECK(memcmp(digest, expected, sizeof expected) != 0);
  machine->sequence_exhausted = false;

  check_digest_differs_after_u32_change(machine, &machine->pi.dram_address);
  check_digest_differs_after_u32_change(machine, &machine->pi.cart_address);
  check_digest_differs_after_u32_change(machine, &machine->pi.read_length);
  check_digest_differs_after_u32_change(machine, &machine->pi.write_length);
  check_digest_differs_after_u32_change(machine, &machine->pi.domain1_latency);
  check_digest_differs_after_u32_change(machine,
                                        &machine->pi.domain1_pulse_width);
  check_digest_differs_after_u32_change(machine,
                                        &machine->pi.domain1_page_size);
  check_digest_differs_after_u32_change(machine,
                                        &machine->pi.domain1_release_duration);
  check_digest_differs_after_u32_change(machine, &machine->pi.domain2_latency);
  check_digest_differs_after_u32_change(machine,
                                        &machine->pi.domain2_pulse_width);
  check_digest_differs_after_u32_change(machine,
                                        &machine->pi.domain2_page_size);
  check_digest_differs_after_u32_change(machine,
                                        &machine->pi.domain2_release_duration);
  check_digest_differs_after_bool_change(machine, &machine->pi.error);
  check_digest_differs_after_bool_change(machine, &machine->pi.active_valid);
  check_digest_differs_after_bool_change(machine,
                                        &machine->pi.active_canceled);
  check_digest_differs_after_u32_change(machine,
                                        &machine->pi.active_dram_address);
  check_digest_differs_after_u32_change(machine,
                                        &machine->pi.active_cart_address);
  check_digest_differs_after_u32_change(machine, &machine->pi.active_length);
  check_digest_differs_after_u64_change(machine, &machine->pi.active_token);
  check_digest_differs_after_u64_change(machine, &machine->pi.next_token);
  check_digest_differs_after_bool_change(machine,
                                        &machine->pi.token_exhausted);

  T64_CHECK_INT(t64_test_schedule_synthetic(machine, 9u,
                                            T64_TEST_EVENT_NOOP, 17u),
                T64_STATUS_OK);
  event_index = machine->queue_head;
  event = &machine->events[event_index];
  check_digest_differs_after_u64_change(machine, &event->tick);
  check_digest_differs_after_u64_change(machine, &event->sequence);
  check_digest_differs_after_u32_change(machine, &event->device);
  check_digest_differs_after_u32_change(machine, &event->kind);
  check_digest_differs_after_u64_change(machine, &event->value);
  check_digest_differs_after_u64_change(machine, &event->token);
  {
    uint8_t before[T64_DIGEST_SIZE];
    uint8_t after[T64_DIGEST_SIZE];
    size_t saved_count = machine->queue_count;

    T64_CHECK_INT(t64_machine_digest(machine, before), T64_STATUS_OK);
    machine->queue_count++;
    T64_CHECK_INT(t64_machine_digest(machine, after), T64_STATUS_OK);
    T64_CHECK(memcmp(before, after, sizeof before) != 0);
    machine->queue_count = saved_count;
  }
  {
    uint8_t before[T64_DIGEST_SIZE];
    uint8_t after[T64_DIGEST_SIZE];
    t64_event saved_event = *event;
    size_t saved_head = machine->queue_head;
    size_t relocated = 13u;

    T64_CHECK_INT(t64_machine_digest(machine, before), T64_STATUS_OK);
    machine->events[relocated] = *event;
    machine->queue_head = relocated;
    T64_CHECK_INT(t64_machine_digest(machine, after), T64_STATUS_OK);
    T64_CHECK(memcmp(before, after, sizeof before) == 0);
    machine->queue_head = saved_head;
    machine->events[event_index] = saved_event;
  }
  t64_machine_destroy(machine);
  T64_CHECK_INT(host_state.outstanding, 0u);
}

static void test_digest_excludes_storage_and_event_handler(void) {
  uint8_t image[TEST_ROM_SIZE];
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  allocator_state host_state = {0u};
  const t64_host_allocator allocator = {test_allocate, test_release,
                                        &host_state};
  t64_machine *machine = NULL;
  void *saved_context;

  T64_CHECK(fixture_init(&host_state, &allocator, &machine, image));
  if (machine == NULL) {
    return;
  }
  T64_CHECK_INT(t64_test_schedule_synthetic(machine, 9u,
                                            T64_TEST_EVENT_NOOP, 17u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(machine, before), T64_STATUS_OK);
  saved_context = machine->allocator.context;
  {
    static uint8_t alternate_machine_storage;
    static uint8_t alternate_image_storage;
    t64_host_storage saved_machine_storage = machine->machine_storage;
    t64_host_storage saved_image_storage = machine->image_storage;
    t64_event_handler_fn saved_handler =
        machine->events[machine->queue_head].handler;
    size_t unused_index =
        (machine->queue_head + 1u) % T64_EVENT_QUEUE_CAPACITY;
    uint64_t saved_unused_value = machine->events[unused_index].value;

    machine->machine_storage.data = &alternate_machine_storage;
    machine->machine_storage.capacity = sizeof alternate_machine_storage;
    machine->image_storage.data = &alternate_image_storage;
    machine->image_storage.capacity = sizeof alternate_image_storage;
    machine->machine_storage.alignment++;
    machine->image_storage.alignment++;
    machine->events[machine->queue_head].handler = NULL;
    machine->events[(machine->queue_head + 1u) % T64_EVENT_QUEUE_CAPACITY]
        .value = UINT64_C(0xfeedface);
    T64_CHECK_INT(t64_machine_digest(machine, after), T64_STATUS_OK);
    T64_CHECK(memcmp(before, after, sizeof before) == 0);
    machine->machine_storage = saved_machine_storage;
    machine->image_storage = saved_image_storage;
    machine->events[machine->queue_head].handler = saved_handler;
    machine->events[unused_index].value = saved_unused_value;
  }
  machine->allocator.context = saved_context;
  t64_machine_destroy(machine);
  T64_CHECK_INT(host_state.outstanding, 0u);
}

static void test_partitioned_and_isolated_advances_have_same_digest(void) {
  uint8_t image_a[TEST_ROM_SIZE];
  uint8_t image_b[TEST_ROM_SIZE];
  uint8_t digest_a[T64_DIGEST_SIZE];
  uint8_t digest_b[T64_DIGEST_SIZE];
  allocator_state alloc_a = {0u};
  allocator_state alloc_b = {0u};
  const t64_host_allocator host_a = {test_allocate, test_release, &alloc_a};
  const t64_host_allocator host_b = {test_allocate, test_release, &alloc_b};
  t64_machine *machine_a = NULL;
  t64_machine *machine_b = NULL;
  t64_stop_request request_a = {T64_STOP_GUEST_TICK, 10u};
  t64_stop_request request_b = {T64_STOP_GUEST_TICK, 4u};
  t64_stop_result result = {0u, 0u};

  T64_CHECK(fixture_init(&alloc_a, &host_a, &machine_a, image_a));
  T64_CHECK(fixture_init(&alloc_b, &host_b, &machine_b, image_b));
  if (machine_a == NULL || machine_b == NULL) {
    t64_machine_destroy(machine_a);
    t64_machine_destroy(machine_b);
    return;
  }
  T64_CHECK_INT(t64_test_schedule_synthetic(machine_a, 3u,
                                            T64_TEST_EVENT_INSTRUCTION, 2u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_test_schedule_synthetic(machine_a, 8u,
                                            T64_TEST_EVENT_FRAME, 1u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_test_schedule_synthetic(machine_b, 3u,
                                            T64_TEST_EVENT_INSTRUCTION, 2u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_test_schedule_synthetic(machine_b, 8u,
                                            T64_TEST_EVENT_FRAME, 1u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_advance(machine_a, &request_a, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_advance(machine_b, &request_b, &result),
                T64_STATUS_OK);
  request_b.value = 10u;
  T64_CHECK_INT(t64_machine_advance(machine_b, &request_b, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(machine_a, digest_a), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(machine_b, digest_b), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_a, digest_b, sizeof digest_a) == 0);
  T64_CHECK_INT(machine_a->instruction_count, 2u);
  T64_CHECK_INT(machine_a->frame_event_count, 1u);
  T64_CHECK_INT(machine_a->tick, 10u);

  t64_machine_destroy(machine_b);
  t64_machine_destroy(machine_a);
  T64_CHECK_INT(alloc_a.outstanding, 0u);
  T64_CHECK_INT(alloc_b.outstanding, 0u);
}

static void test_scheduler_trace_records_boundary(void) {
  uint8_t image[TEST_ROM_SIZE];
  allocator_state host_state = {0u};
  const t64_host_allocator allocator = {test_allocate, test_release,
                                        &host_state};
  t64_machine *machine = NULL;
  t64_stop_request request = {T64_STOP_GUEST_TICK, 5u};
  t64_stop_result result = {0u, 0u};
  t64_trace_record record = {0u, 0u, 0u, 0u};
  size_t record_count = SIZE_MAX;
  size_t callbacks_before_advance;
  uint64_t dropped = UINT64_MAX;
  bool saturated = true;

  T64_CHECK(fixture_init(&host_state, &allocator, &machine, image));
  if (machine == NULL) {
    return;
  }
  T64_CHECK_INT(t64_machine_trace_set_enabled(machine, true), T64_STATUS_OK);
  T64_CHECK_INT(t64_test_schedule_synthetic(machine, 5u,
                                            T64_TEST_EVENT_NOOP, 17u),
                T64_STATUS_OK);
  callbacks_before_advance = host_state.callback_calls;
  T64_CHECK_INT(t64_machine_advance(machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(host_state.callback_calls, callbacks_before_advance);
  T64_CHECK_INT(t64_machine_trace_drain(machine, &record, 1u, &record_count,
                                        &dropped, &saturated),
                T64_STATUS_OK);
  T64_CHECK_INT(record_count, 1u);
  T64_CHECK_INT(record.tick, 5u);
  T64_CHECK_INT(record.device, T64_DEVICE_SYNTHETIC_TEST);
  T64_CHECK_INT(record.kind, T64_TEST_EVENT_NOOP);
  T64_CHECK_INT(record.value, 17u);
  T64_CHECK_INT(dropped, 0u);
  T64_CHECK(!saturated);

  t64_machine_destroy(machine);
  T64_CHECK_INT(host_state.outstanding, 0u);
}

static void test_trace_drain_validation_and_digest_exclusion(void) {
  uint8_t image[TEST_ROM_SIZE];
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  allocator_state host_state = {0u};
  const t64_host_allocator allocator = {test_allocate, test_release,
                                        &host_state};
  t64_machine *machine = NULL;
  t64_stop_request request = {T64_STOP_GUEST_TICK, 4u};
  t64_stop_result result = {0u, 0u};
  t64_trace_record record = {0u, 0u, 0u, 0u};
  size_t count;
  uint64_t dropped;
  bool saturated;

  T64_CHECK(fixture_init(&host_state, &allocator, &machine, image));
  if (machine == NULL) {
    return;
  }
  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_trace_set_enabled(machine, true), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);
  T64_CHECK_INT(t64_test_schedule_synthetic(machine, 4u,
                                            T64_TEST_EVENT_NOOP, 41u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_advance(machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);

  count = 77u;
  dropped = 88u;
  saturated = true;
  T64_CHECK_INT(t64_machine_trace_drain(machine, NULL, 1u, &count, &dropped,
                                        &saturated),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK_INT(count, 77u);
  T64_CHECK_INT(dropped, 88u);
  T64_CHECK(saturated);
  T64_CHECK_INT(t64_machine_trace_drain(machine, (t64_trace_record *)machine,
                                        1u, &count, &dropped, &saturated),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK_INT(count, 77u);

  T64_CHECK_INT(t64_machine_trace_drain(machine, NULL, 0u, &count, &dropped,
                                        &saturated),
                T64_STATUS_OK);
  T64_CHECK_INT(count, 0u);
  T64_CHECK_INT(dropped, 0u);
  T64_CHECK(!saturated);
  T64_CHECK_INT(t64_machine_trace_drain(machine, &record, 1u, &count, &dropped,
                                        &saturated),
                T64_STATUS_OK);
  T64_CHECK_INT(count, 1u);
  T64_CHECK_INT(record.tick, 4u);
  T64_CHECK_INT(record.value, 41u);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);
  t64_machine_destroy(machine);
  T64_CHECK_INT(host_state.outstanding, 0u);
}

static void test_trace_fifo_drain_and_same_digest_as_untraced(void) {
  uint8_t image_a[TEST_ROM_SIZE];
  uint8_t image_b[TEST_ROM_SIZE];
  uint8_t digest_a[T64_DIGEST_SIZE];
  uint8_t digest_b[T64_DIGEST_SIZE];
  allocator_state alloc_a = {0u};
  allocator_state alloc_b = {0u};
  const t64_host_allocator host_a = {test_allocate, test_release, &alloc_a};
  const t64_host_allocator host_b = {test_allocate, test_release, &alloc_b};
  t64_machine *machine_a = NULL;
  t64_machine *machine_b = NULL;
  t64_stop_request request = {T64_STOP_GUEST_TICK, 7u};
  t64_stop_result result = {0u, 0u};
  t64_trace_record records[2];
  size_t count = 0u;
  uint64_t dropped = 0u;
  bool saturated = false;

  T64_CHECK(fixture_init(&alloc_a, &host_a, &machine_a, image_a));
  T64_CHECK(fixture_init(&alloc_b, &host_b, &machine_b, image_b));
  if (machine_a == NULL || machine_b == NULL) {
    t64_machine_destroy(machine_a);
    t64_machine_destroy(machine_b);
    return;
  }
  T64_CHECK_INT(t64_machine_trace_set_enabled(machine_b, true), T64_STATUS_OK);
  T64_CHECK_INT(t64_test_schedule_synthetic(machine_a, 3u,
                                            T64_TEST_EVENT_NOOP, 31u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_test_schedule_synthetic(machine_a, 7u,
                                            T64_TEST_EVENT_NOOP, 72u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_test_schedule_synthetic(machine_b, 3u,
                                            T64_TEST_EVENT_NOOP, 31u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_test_schedule_synthetic(machine_b, 7u,
                                            T64_TEST_EVENT_NOOP, 72u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_advance(machine_a, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_advance(machine_b, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_trace_drain(machine_b, records, 1u, &count,
                                        &dropped, &saturated),
                T64_STATUS_OK);
  T64_CHECK_INT(count, 1u);
  T64_CHECK_INT(records[0].tick, 3u);
  T64_CHECK_INT(records[0].device, T64_DEVICE_SYNTHETIC_TEST);
  T64_CHECK_INT(records[0].kind, T64_TEST_EVENT_NOOP);
  T64_CHECK_INT(records[0].value, 31u);
  T64_CHECK_INT(dropped, 0u);
  T64_CHECK(!saturated);
  T64_CHECK_INT(t64_machine_trace_drain(machine_b, records, 2u, &count,
                                        &dropped, &saturated),
                T64_STATUS_OK);
  T64_CHECK_INT(count, 1u);
  T64_CHECK_INT(records[0].tick, 7u);
  T64_CHECK_INT(records[0].value, 72u);
  T64_CHECK_INT(t64_machine_digest(machine_a, digest_a), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(machine_b, digest_b), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_a, digest_b, sizeof digest_a) == 0);

  t64_machine_destroy(machine_b);
  t64_machine_destroy(machine_a);
  T64_CHECK_INT(alloc_a.outstanding, 0u);
  T64_CHECK_INT(alloc_b.outstanding, 0u);
}

static void test_trace_overflow_and_saturating_drop_counter(void) {
  uint8_t image_a[TEST_ROM_SIZE];
  uint8_t image_b[TEST_ROM_SIZE];
  uint8_t digest_a[T64_DIGEST_SIZE];
  uint8_t digest_b[T64_DIGEST_SIZE];
  allocator_state alloc_a = {0u};
  allocator_state alloc_b = {0u};
  const t64_host_allocator host_a = {test_allocate, test_release, &alloc_a};
  const t64_host_allocator host_b = {test_allocate, test_release, &alloc_b};
  t64_machine *machine_a = NULL;
  t64_machine *machine_b = NULL;
  t64_stop_request request = {T64_STOP_GUEST_TICK, 0u};
  t64_stop_result result = {0u, 0u};
  t64_trace_record records[T64_TRACE_RECORD_CAPACITY];
  size_t count = 0u;
  uint64_t dropped = 0u;
  bool saturated = false;
  size_t i;

  T64_CHECK(fixture_init(&alloc_a, &host_a, &machine_a, image_a));
  T64_CHECK(fixture_init_with_trace(&alloc_b, &host_b, &machine_b, image_b,
                                    true));
  if (machine_a == NULL || machine_b == NULL) {
    t64_machine_destroy(machine_a);
    t64_machine_destroy(machine_b);
    return;
  }
  for (i = 0u; i < T64_EVENT_QUEUE_CAPACITY; i++) {
    T64_CHECK_INT(t64_test_schedule_synthetic(machine_a, 0u,
                                              T64_TEST_EVENT_NOOP, i),
                  T64_STATUS_OK);
    T64_CHECK_INT(t64_test_schedule_synthetic(machine_b, 0u,
                                              T64_TEST_EVENT_NOOP, i),
                  T64_STATUS_OK);
  }
  T64_CHECK_INT(t64_machine_advance(machine_a, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_advance(machine_b, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_trace_drain(machine_b, records,
                                        T64_TRACE_RECORD_CAPACITY, &count,
                                        &dropped, &saturated),
                T64_STATUS_OK);
  T64_CHECK_INT(count, T64_TRACE_RECORD_CAPACITY);
  T64_CHECK_INT(records[0].device, T64_TRACE_DEVICE_MACHINE);
  T64_CHECK_INT(records[0].kind, T64_TRACE_KIND_ROM_ADMITTED);
  T64_CHECK_INT(records[0].value, TEST_ROM_SIZE);
  for (i = 1u; i < T64_TRACE_RECORD_CAPACITY; i++) {
    T64_CHECK_INT(records[i].tick, 0u);
    T64_CHECK_INT(records[i].device, T64_DEVICE_SYNTHETIC_TEST);
    T64_CHECK_INT(records[i].kind, T64_TEST_EVENT_NOOP);
    T64_CHECK_INT(records[i].value, i - 1u);
  }
  T64_CHECK_INT(dropped, 1u);
  T64_CHECK(!saturated);
  T64_CHECK_INT(t64_machine_digest(machine_a, digest_a), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(machine_b, digest_b), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_a, digest_b, sizeof digest_a) == 0);

  T64_CHECK_INT(t64_machine_trace_set_enabled(machine_a, true), T64_STATUS_OK);
  for (i = 0u; i < T64_TRACE_RECORD_CAPACITY; i++) {
    t64_trace_emit(machine_a, 2u, T64_TRACE_DEVICE_MACHINE,
                   T64_TRACE_KIND_ROM_ADMITTED, (uint64_t)i);
  }
  machine_a->trace_drop_count = UINT64_MAX - UINT64_C(1);
  t64_trace_emit(machine_a, 2u, T64_TRACE_DEVICE_MACHINE,
                 T64_TRACE_KIND_ROM_ADMITTED, 999u);
  T64_CHECK_INT(machine_a->trace_drop_count, UINT64_MAX);
  T64_CHECK(!machine_a->trace_drop_count_saturated);
  t64_trace_emit(machine_a, 2u, T64_TRACE_DEVICE_MACHINE,
                 T64_TRACE_KIND_ROM_ADMITTED, 1000u);
  T64_CHECK_INT(machine_a->trace_drop_count, UINT64_MAX);
  T64_CHECK(machine_a->trace_drop_count_saturated);
  T64_CHECK_INT(t64_machine_trace_drain(machine_a, NULL, 0u, &count, &dropped,
                                        &saturated),
                T64_STATUS_OK);
  T64_CHECK_INT(count, 0u);
  T64_CHECK_INT(dropped, UINT64_MAX);
  T64_CHECK(saturated);
  t64_machine_destroy(machine_b);
  t64_machine_destroy(machine_a);
  T64_CHECK_INT(alloc_a.outstanding, 0u);
  T64_CHECK_INT(alloc_b.outstanding, 0u);
}

static void test_admission_reset_and_refusal_preserve_scheduler_trace(void) {
  uint8_t image[TEST_ROM_SIZE];
  uint8_t replacement[TEST_ROM_SIZE];
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  allocator_state host_state = {0u};
  const t64_host_allocator allocator = {test_allocate, test_release,
                                        &host_state};
  t64_machine *machine = NULL;
  t64_stop_request request = {T64_STOP_GUEST_TICK, 0u};
  t64_stop_result result = {0u, 0u};
  size_t saved_trace_count;
  size_t saved_trace_head;
  uint64_t saved_drops;
  bool saved_saturation;
  t64_trace_record admission_before;
  t64_trace_record scheduler_before;
  t64_event pending_before;
  size_t saved_queue_count;
  uint64_t saved_next_sequence;
  t64_trace_record output;
  size_t count = 0u;
  uint64_t dropped = 0u;
  bool saturated = false;
  size_t i;

  T64_CHECK(fixture_init_with_trace(&host_state, &allocator, &machine, image,
                                    true));
  if (machine == NULL) {
    return;
  }
  T64_CHECK_INT(machine->trace_count, 1u);
  T64_CHECK_INT(t64_test_schedule_synthetic(machine, 0u,
                                            T64_TEST_EVENT_NOOP, 19u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_test_schedule_synthetic(machine, 5u,
                                            T64_TEST_EVENT_NOOP, 23u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_advance(machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(machine->trace_count, 2u);
  T64_CHECK_INT(machine->queue_count, 1u);
  saved_queue_count = machine->queue_count;
  saved_next_sequence = machine->next_sequence;
  pending_before = machine->events[machine->queue_head];
  saved_trace_count = machine->trace_count;
  saved_trace_head = machine->trace_head;
  saved_drops = machine->trace_drop_count;
  saved_saturation = machine->trace_drop_count_saturated;
  admission_before = machine->trace_records[machine->trace_head];
  scheduler_before = machine->trace_records[
      (machine->trace_head + machine->trace_count - 1u) %
      T64_TRACE_RECORD_CAPACITY];
  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);

  fill_authored_rom(replacement);
  replacement[0] = 0xdeu;
  replacement[1] = 0xadu;
  replacement[2] = 0xbeu;
  replacement[3] = 0xefu;
  T64_CHECK_INT(t64_machine_admit_rom(machine, replacement,
                                      sizeof replacement),
                T64_STATUS_INVALID_ROM);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);
  T64_CHECK_INT(machine->trace_count, saved_trace_count);
  T64_CHECK_INT(machine->trace_head, saved_trace_head);
  T64_CHECK_INT(machine->trace_drop_count, saved_drops);
  T64_CHECK(machine->trace_drop_count_saturated == saved_saturation);
  T64_CHECK_INT(machine->queue_count, saved_queue_count);
  T64_CHECK_INT(machine->next_sequence, saved_next_sequence);
  T64_CHECK_INT(machine->events[machine->queue_head].tick,
                pending_before.tick);
  T64_CHECK_INT(machine->events[machine->queue_head].sequence,
                pending_before.sequence);
  T64_CHECK_INT(machine->events[machine->queue_head].value,
                pending_before.value);
  T64_CHECK(t64_test_event_handler_matches(machine, 0u));
  T64_CHECK_INT(t64_machine_trace_drain(machine, &output, 1u, &count,
                                        &dropped, &saturated),
                T64_STATUS_OK);
  T64_CHECK_INT(count, 1u);
  T64_CHECK_INT(output.tick, admission_before.tick);
  T64_CHECK_INT(output.device, admission_before.device);
  T64_CHECK_INT(output.kind, admission_before.kind);
  T64_CHECK_INT(output.value, admission_before.value);
  T64_CHECK_INT(t64_machine_trace_drain(machine, &output, 1u, &count,
                                        &dropped, &saturated),
                T64_STATUS_OK);
  T64_CHECK_INT(count, 1u);
  T64_CHECK_INT(output.tick, scheduler_before.tick);
  T64_CHECK_INT(output.device, scheduler_before.device);
  T64_CHECK_INT(output.kind, scheduler_before.kind);
  T64_CHECK_INT(output.value, scheduler_before.value);
  T64_CHECK_INT(output.tick, 0u);
  T64_CHECK_INT(output.device, T64_DEVICE_SYNTHETIC_TEST);
  T64_CHECK_INT(output.kind, T64_TEST_EVENT_NOOP);
  T64_CHECK_INT(output.value, 19u);

  fill_authored_rom(replacement);
  replacement[0x100u] ^= 0x80u;
  T64_CHECK_INT(t64_machine_admit_rom(machine, replacement,
                                      sizeof replacement),
                T64_STATUS_OK);
  T64_CHECK_INT(machine->tick, 0u);
  T64_CHECK_INT(machine->instruction_count, 0u);
  T64_CHECK_INT(machine->frame_event_count, 0u);
  T64_CHECK_INT(machine->queue_count, 0u);
  T64_CHECK_INT(machine->trace_count, 1u);
  T64_CHECK_INT(machine->trace_head, 0u);
  T64_CHECK_INT(machine->trace_drop_count, 0u);
  T64_CHECK(!machine->trace_drop_count_saturated);
  T64_CHECK(machine->trace_enabled);
  for (i = 0u; i < T64_TRACE_RECORD_CAPACITY; i++) {
    T64_CHECK_INT(machine->trace_records[i].tick, 0u);
    T64_CHECK_INT(machine->trace_records[i].device, 0u);
    if (i != 0u) {
      T64_CHECK_INT(machine->trace_records[i].kind, 0u);
      T64_CHECK_INT(machine->trace_records[i].value, 0u);
    }
  }
  T64_CHECK_INT(t64_machine_trace_drain(machine, &output, 1u, &count,
                                        &dropped, &saturated),
                T64_STATUS_OK);
  T64_CHECK_INT(count, 1u);
  T64_CHECK_INT(output.tick, 0u);
  T64_CHECK_INT(output.device, T64_TRACE_DEVICE_MACHINE);
  T64_CHECK_INT(output.kind, T64_TRACE_KIND_ROM_ADMITTED);
  T64_CHECK_INT(output.value, TEST_ROM_SIZE);
  t64_machine_destroy(machine);
  T64_CHECK_INT(host_state.outstanding, 0u);
}

static void test_pi_state_reset_and_refused_replacement(void) {
  uint8_t image[TEST_ROM_SIZE];
  uint8_t replacement[TEST_ROM_SIZE];
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  allocator_state host_state = {0u};
  const t64_host_allocator allocator = {test_allocate, test_release,
                                        &host_state};
  t64_machine *machine = NULL;
  uint32_t value = 0u;

  T64_CHECK(fixture_init(&host_state, &allocator, &machine, image));
  if (machine == NULL) {
    return;
  }
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x04600000), 4u,
                                     UINT32_C(0x100)),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x04600004), 4u,
                                     UINT32_C(0x10000100)),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x04600014), 4u,
                                     UINT32_C(0xab)),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x0460000c), 4u,
                                     UINT32_C(15)),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x04600010), 4u,
                                     UINT32_C(1)),
                T64_STATUS_OK);
  T64_CHECK(machine->pi.active_valid);
  T64_CHECK(machine->pi.active_canceled);
  T64_CHECK(machine->pi.active_token != 0u);
  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);

  fill_authored_rom(replacement);
  replacement[0] = 0xdeu;
  replacement[1] = 0xadu;
  replacement[2] = 0xbeu;
  replacement[3] = 0xefu;
  T64_CHECK_INT(t64_machine_admit_rom(machine, replacement,
                                      sizeof replacement),
                T64_STATUS_INVALID_ROM);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);
  T64_CHECK(machine->pi.active_valid);
  T64_CHECK(machine->pi.active_canceled);
  T64_CHECK_INT(machine->queue_count, 1u);

  fill_authored_rom(replacement);
  replacement[0x100u] ^= 0x80u;
  T64_CHECK_INT(t64_machine_admit_rom(machine, replacement,
                                      sizeof replacement),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04600000), 4u,
                                     &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, 0u);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04600004), 4u,
                                     &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, 0u);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04600008), 4u,
                                     &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, 0u);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x0460000c), 4u,
                                     &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, 0u);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04600014), 4u,
                                     &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, 0u);
  T64_CHECK(!machine->pi.active_valid);
  T64_CHECK(!machine->pi.active_canceled);
  T64_CHECK(!machine->pi.error);
  T64_CHECK_INT(machine->pi.active_dram_address, 0u);
  T64_CHECK_INT(machine->pi.active_cart_address, 0u);
  T64_CHECK_INT(machine->pi.active_length, 0u);
  T64_CHECK_INT(machine->pi.active_token, 0u);
  T64_CHECK_INT(machine->pi.next_token, 1u);
  T64_CHECK(!machine->pi.token_exhausted);
  T64_CHECK_INT(machine->queue_count, 0u);
  T64_CHECK_INT(machine->mi_pending, 0u);
  T64_CHECK_INT(machine->mi_mask, 0u);
  t64_machine_destroy(machine);
  T64_CHECK_INT(host_state.outstanding, 0u);
}

int main(void) {
  (void)puts("evidence: synthetic scheduler metadata only; no CPU or device execution");
  test_queued_future_work_changes_digest();
  test_digest_outputs_reject_machine_and_image_aliases();
  test_digest_golden_and_machine_fields();
  test_digest_excludes_storage_and_event_handler();
  test_partitioned_and_isolated_advances_have_same_digest();
  test_scheduler_trace_records_boundary();
  test_trace_drain_validation_and_digest_exclusion();
  test_trace_fifo_drain_and_same_digest_as_untraced();
  test_trace_overflow_and_saturating_drop_counter();
  test_admission_reset_and_refusal_preserve_scheduler_trace();
  test_pi_state_reset_and_refused_replacement();
  return T64_TEST_FINISH("unit.core.digest-trace");
}
