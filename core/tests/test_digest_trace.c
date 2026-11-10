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

static void test_digest_golden_and_machine_fields(void) {
  uint8_t image[TEST_ROM_SIZE];
  uint8_t digest[T64_DIGEST_SIZE];
  uint8_t expected[T64_DIGEST_SIZE] = {
      0x08u, 0x80u, 0x96u, 0xb1u, 0xecu, 0x1cu, 0xd7u, 0x36u,
      0x52u, 0xcfu, 0x1du, 0xdbu, 0x27u, 0x40u, 0x1au, 0x4eu,
      0xbau, 0x98u, 0x3cu, 0x61u, 0xa1u, 0xc2u, 0x9fu, 0x51u,
      0x41u, 0xfeu, 0x9fu, 0xe7u, 0x9cu, 0x28u, 0xaeu, 0xf7u};
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
    t64_host_storage saved_machine_storage = machine->machine_storage;
    t64_host_storage saved_image_storage = machine->image_storage;
    void *saved_machine_data = machine->machine_storage.data;
    void *saved_image_data = machine->image_storage.data;
    t64_event_handler_fn saved_handler =
        machine->events[machine->queue_head].handler;
    size_t unused_index =
        (machine->queue_head + 1u) % T64_EVENT_QUEUE_CAPACITY;
    uint64_t saved_unused_value = machine->events[unused_index].value;

    machine->machine_storage.data = NULL;
    machine->image_storage.data = NULL;
    machine->machine_storage.alignment++;
    machine->image_storage.alignment++;
    machine->events[machine->queue_head].handler = NULL;
    machine->events[(machine->queue_head + 1u) % T64_EVENT_QUEUE_CAPACITY]
        .value = UINT64_C(0xfeedface);
    T64_CHECK_INT(t64_machine_digest(machine, after), T64_STATUS_OK);
    T64_CHECK(memcmp(before, after, sizeof before) == 0);
    machine->machine_storage = saved_machine_storage;
    machine->image_storage = saved_image_storage;
    machine->machine_storage.data = saved_machine_data;
    machine->image_storage.data = saved_image_data;
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
  T64_CHECK_INT(t64_machine_advance(machine, &request, &result),
                T64_STATUS_OK);
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

int main(void) {
  (void)puts("evidence: synthetic scheduler metadata only; no CPU or device execution");
  test_queued_future_work_changes_digest();
  test_digest_golden_and_machine_fields();
  test_digest_excludes_storage_and_event_handler();
  test_partitioned_and_isolated_advances_have_same_digest();
  test_scheduler_trace_records_boundary();
  return T64_TEST_FINISH("unit.core.digest-trace");
}
