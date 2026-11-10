#line 1 "core/tests/test_scheduler.c"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "t64_internal.h"
#include "t64_test.h"

#define TEST_ROM_SIZE 4096u

typedef struct {
  size_t allocations;
  size_t releases;
  size_t outstanding;
} allocator_state;

typedef struct {
  allocator_state host_state;
  t64_host_allocator allocator;
  t64_machine *machine;
  uint8_t image[TEST_ROM_SIZE];
} machine_fixture;

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

static void fill_authored_rom(uint8_t image[TEST_ROM_SIZE], uint8_t salt) {
  size_t i;

  for (i = 0u; i < TEST_ROM_SIZE; i++) {
    image[i] = (uint8_t)((i * 31u + salt * 11u) & 0xffu);
  }
  image[0] = 0x80u;
  image[1] = 0x37u;
  image[2] = 0x12u;
  image[3] = 0x40u;
}

static bool fixture_init_with_profile(
    machine_fixture *fixture, uint8_t salt,
    const t64_profile_descriptor *profile) {
  (void)memset(fixture, 0, sizeof *fixture);
  fixture->allocator.allocate = test_allocate;
  fixture->allocator.release = test_release;
  fixture->allocator.context = &fixture->host_state;
  fill_authored_rom(fixture->image, salt);
  if (profile == NULL ||
      t64_machine_create(&fixture->allocator, profile, &fixture->machine) !=
          T64_STATUS_OK ||
      fixture->machine == NULL ||
      t64_machine_admit_rom(fixture->machine, fixture->image,
                            sizeof fixture->image) != T64_STATUS_OK) {
    if (fixture->machine != NULL) {
      t64_machine_destroy(fixture->machine);
      fixture->machine = NULL;
    }
    return false;
  }
  return true;
}

static bool fixture_init(machine_fixture *fixture, uint8_t salt) {
  const t64_profile_descriptor *profile = NULL;
  t64_status status =
      t64_profile_lookup("ntsc-cart-4m-skeleton", &profile);

  return fixture_init_with_profile(
      fixture, salt, status == T64_STATUS_OK ? profile : NULL);
}

static void fixture_destroy(machine_fixture *fixture) {
  if (fixture->machine != NULL) {
    t64_machine_destroy(fixture->machine);
    fixture->machine = NULL;
  }
  T64_CHECK_INT(fixture->host_state.outstanding, 0u);
}

static void test_unattainable_instruction_bound_and_counter_refusal(void) {
  machine_fixture fixture;
  t64_stop_request request = {T64_STOP_INSTRUCTION_COUNT, UINT64_C(1)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  size_t allocations_before;
  size_t releases_before;
  t64_status status;

  T64_CHECK(fixture_init(&fixture, 1u));
  if (fixture.machine == NULL) {
    return;
  }
  allocations_before = fixture.host_state.allocations;
  releases_before = fixture.host_state.releases;
  status = t64_machine_advance(fixture.machine, &request, &result);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_QUIESCENT);
  T64_CHECK_INT(result.tick, UINT64_C(0));
  T64_CHECK_INT(fixture.host_state.allocations, allocations_before);
  T64_CHECK_INT(fixture.host_state.releases, releases_before);

  request.value = 0u;
  result.reason = UINT32_MAX;
  result.tick = UINT64_MAX;
  T64_CHECK_INT(t64_test_schedule_synthetic(fixture.machine, 0u,
                                            T64_TEST_EVENT_INSTRUCTION, 1u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_advance(fixture.machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_INSTRUCTION_COUNT);
  T64_CHECK_INT(fixture.machine->instruction_count, 0u);
  T64_CHECK_INT(fixture.machine->queue_count, 1u);

  {
    uint8_t before[T64_DIGEST_SIZE];
    uint8_t after[T64_DIGEST_SIZE];

    fixture.machine->instruction_count = 1u;
    request.value = 0u;
    result.reason = UINT32_C(91);
    result.tick = UINT64_C(92);
    T64_CHECK_INT(t64_machine_digest(fixture.machine, before),
                  T64_STATUS_OK);
    T64_CHECK_INT(t64_machine_advance(fixture.machine, &request, &result),
                  T64_STATUS_LIMIT_REVERSED);
    T64_CHECK_INT(t64_machine_digest(fixture.machine, after), T64_STATUS_OK);
    T64_CHECK(memcmp(before, after, sizeof before) == 0);
    T64_CHECK_INT(result.reason, 91u);
    T64_CHECK_INT(result.tick, 92u);
    T64_CHECK_INT(fixture.machine->queue_count, 1u);
  }
  fixture_destroy(&fixture);
}

static void test_inclusive_time_bounds_idle_advance_and_order(void) {
  machine_fixture fixture;
  machine_fixture idle_fixture;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(10)};
  t64_stop_result result = {0u, 0u};
  size_t i;

  T64_CHECK(fixture_init(&fixture, 2u));
  if (fixture.machine == NULL) {
    return;
  }
  T64_CHECK_INT(t64_test_schedule_synthetic(fixture.machine, 11u,
                                            T64_TEST_EVENT_NOOP, 11u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_test_schedule_synthetic(fixture.machine, 9u,
                                            T64_TEST_EVENT_NOOP, 9u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_test_schedule_synthetic(fixture.machine, 10u,
                                            T64_TEST_EVENT_NOOP, 10u),
                T64_STATUS_OK);
  T64_CHECK_INT(fixture.machine->queue_count, 3u);
  for (i = 0u; i < fixture.machine->queue_count; i++) {
    T64_CHECK(t64_test_event_handler_matches(fixture.machine, i));
  }
  T64_CHECK_INT(fixture.machine->events[fixture.machine->queue_head].tick, 9u);
  T64_CHECK_INT(fixture.machine->events[(fixture.machine->queue_head + 1u) %
                                        T64_EVENT_QUEUE_CAPACITY]
                    .tick,
                10u);
  T64_CHECK_INT(t64_machine_advance(fixture.machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, 10u);
  T64_CHECK_INT(fixture.machine->tick, 10u);
  T64_CHECK_INT(fixture.machine->queue_count, 1u);
  T64_CHECK_INT(fixture.machine->events[fixture.machine->queue_head].tick, 11u);

  request.value = 9u;
  result.reason = 91u;
  result.tick = 92u;
  T64_CHECK_INT(t64_machine_advance(fixture.machine, &request, &result),
                T64_STATUS_TIME_REVERSED);
  T64_CHECK_INT(result.reason, 91u);
  T64_CHECK_INT(result.tick, 92u);
  fixture_destroy(&fixture);

  T64_CHECK(fixture_init(&idle_fixture, 3u));
  if (idle_fixture.machine != NULL) {
    request.value = 123u;
    T64_CHECK_INT(t64_machine_advance(idle_fixture.machine, &request, &result),
                  T64_STATUS_OK);
    T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
    T64_CHECK_INT(result.tick, 123u);
    T64_CHECK_INT(idle_fixture.machine->queue_count, 0u);
    T64_CHECK_INT(t64_test_schedule_synthetic(idle_fixture.machine, 123u,
                                              T64_TEST_EVENT_NOOP, 1u),
                  T64_STATUS_OK);
    T64_CHECK_INT(t64_machine_advance(idle_fixture.machine, &request, &result),
                  T64_STATUS_OK);
    T64_CHECK_INT(result.tick, 123u);
    T64_CHECK_INT(idle_fixture.machine->queue_count, 0u);
    fixture_destroy(&idle_fixture);
  }
}

static void test_fifo_counter_stop_leaves_later_same_tick_event(void) {
  machine_fixture first_fixture;
  machine_fixture reverse_fixture;
  t64_stop_request request = {T64_STOP_INSTRUCTION_COUNT, UINT64_C(1)};
  t64_stop_result result = {0u, 0u};

  T64_CHECK(fixture_init(&first_fixture, 4u));
  if (first_fixture.machine == NULL) {
    return;
  }
  T64_CHECK_INT(t64_test_schedule_synthetic(first_fixture.machine, 5u,
                                            T64_TEST_EVENT_INSTRUCTION, 1u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_test_schedule_synthetic(first_fixture.machine, 5u,
                                            T64_TEST_EVENT_FRAME, 1u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_advance(first_fixture.machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_INSTRUCTION_COUNT);
  T64_CHECK_INT(result.tick, 5u);
  T64_CHECK_INT(first_fixture.machine->instruction_count, 1u);
  T64_CHECK_INT(first_fixture.machine->frame_event_count, 0u);
  T64_CHECK_INT(first_fixture.machine->queue_count, 1u);
  T64_CHECK_INT(first_fixture.machine->events[first_fixture.machine->queue_head]
                    .kind,
                T64_TEST_EVENT_FRAME);
  request.kind = T64_STOP_FRAME_EVENT_COUNT;
  T64_CHECK_INT(t64_machine_advance(first_fixture.machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_FRAME_EVENT_COUNT);
  T64_CHECK_INT(first_fixture.machine->frame_event_count, 1u);
  T64_CHECK_INT(first_fixture.machine->queue_count, 0u);
  fixture_destroy(&first_fixture);

  T64_CHECK(fixture_init(&reverse_fixture, 5u));
  if (reverse_fixture.machine != NULL) {
    T64_CHECK_INT(t64_test_schedule_synthetic(reverse_fixture.machine, 5u,
                                              T64_TEST_EVENT_FRAME, 1u),
                  T64_STATUS_OK);
    T64_CHECK_INT(t64_test_schedule_synthetic(reverse_fixture.machine, 5u,
                                              T64_TEST_EVENT_INSTRUCTION, 1u),
                  T64_STATUS_OK);
    request.kind = T64_STOP_INSTRUCTION_COUNT;
    T64_CHECK_INT(t64_machine_advance(reverse_fixture.machine, &request,
                                      &result),
                  T64_STATUS_OK);
    T64_CHECK_INT(result.reason, T64_STOP_REASON_INSTRUCTION_COUNT);
    T64_CHECK_INT(reverse_fixture.machine->frame_event_count, 1u);
    T64_CHECK_INT(reverse_fixture.machine->queue_count, 0u);
    fixture_destroy(&reverse_fixture);
  }
}

static void test_halt_precedence_at_time_and_counter_bounds(void) {
  machine_fixture time_fixture;
  machine_fixture counter_fixture;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(7)};
  t64_stop_result result = {0u, 0u};

  T64_CHECK(fixture_init(&time_fixture, 6u));
  if (time_fixture.machine == NULL) {
    return;
  }
  T64_CHECK_INT(t64_test_schedule_synthetic(time_fixture.machine, 7u,
                                            T64_TEST_EVENT_HALT, 0u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_test_schedule_synthetic(time_fixture.machine, 7u,
                                            T64_TEST_EVENT_INSTRUCTION, 1u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_advance(time_fixture.machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_HALT);
  T64_CHECK_INT(result.tick, 7u);
  T64_CHECK(time_fixture.machine->halted);
  T64_CHECK_INT(time_fixture.machine->queue_count, 1u);
  T64_CHECK_INT(t64_machine_advance(time_fixture.machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_HALT);
  T64_CHECK_INT(time_fixture.machine->queue_count, 1u);
  fixture_destroy(&time_fixture);

  T64_CHECK(fixture_init(&counter_fixture, 7u));
  if (counter_fixture.machine != NULL) {
    T64_CHECK_INT(t64_test_schedule_synthetic(counter_fixture.machine, 8u,
                                              T64_TEST_EVENT_HALT, 0u),
                  T64_STATUS_OK);
    T64_CHECK_INT(t64_test_schedule_synthetic(
                      counter_fixture.machine, 9u,
                      T64_TEST_EVENT_INSTRUCTION, 1u),
                  T64_STATUS_OK);
    request.kind = T64_STOP_INSTRUCTION_COUNT;
    request.value = 1u;
    T64_CHECK_INT(t64_machine_advance(counter_fixture.machine, &request,
                                      &result),
                  T64_STATUS_OK);
    T64_CHECK_INT(result.reason, T64_STOP_REASON_HALT);
    T64_CHECK_INT(result.tick, 8u);
    T64_CHECK_INT(counter_fixture.machine->instruction_count, 0u);
    T64_CHECK_INT(counter_fixture.machine->queue_count, 1u);
    fixture_destroy(&counter_fixture);
  }
}

static void test_queue_sequence_and_tick_exhaustion_are_transactional(void) {
  machine_fixture queue_fixture;
  machine_fixture sequence_fixture;
  machine_fixture narrow_sequence_fixture;
  machine_fixture tick_fixture;
  machine_fixture max_advance_fixture;
  const t64_profile_descriptor *base_profile = NULL;
  t64_profile_descriptor narrow_profile;
  size_t i;
  t64_event_reservation reservation;

  T64_CHECK(fixture_init(&queue_fixture, 8u));
  if (queue_fixture.machine == NULL) {
    return;
  }
  for (i = 0u; i < queue_fixture.machine->profile.event_queue_capacity; i++) {
    T64_CHECK_INT(t64_test_schedule_synthetic(queue_fixture.machine, 0u,
                                              T64_TEST_EVENT_NOOP,
                                              (uint64_t)i),
                  T64_STATUS_OK);
  }
  T64_CHECK_INT(queue_fixture.machine->queue_count, 256u);
  T64_CHECK_INT(queue_fixture.machine->next_sequence, 256u);
  T64_CHECK_INT(t64_test_reserve_synthetic(queue_fixture.machine, 0u,
                                            T64_TEST_EVENT_NOOP, 257u,
                                            &reservation),
                T64_STATUS_QUEUE_FULL);
  T64_CHECK_INT(queue_fixture.machine->queue_count, 256u);
  T64_CHECK_INT(queue_fixture.machine->next_sequence, 256u);
  fixture_destroy(&queue_fixture);

  T64_CHECK_INT(t64_profile_lookup("ntsc-cart-4m-skeleton", &base_profile),
                T64_STATUS_OK);
  if (base_profile != NULL) {
    narrow_profile = *base_profile;
    narrow_profile.sequence_width_bits = 8u;
    T64_CHECK(fixture_init_with_profile(&narrow_sequence_fixture, 16u,
                                        &narrow_profile));
    if (narrow_sequence_fixture.machine != NULL) {
      uint8_t digest_before[T64_DIGEST_SIZE];
      uint8_t digest_after[T64_DIGEST_SIZE];

      narrow_sequence_fixture.machine->next_sequence = UINT64_C(254);
      T64_CHECK_INT(t64_test_schedule_synthetic(
                        narrow_sequence_fixture.machine, 0u,
                        T64_TEST_EVENT_NOOP, 254u),
                    T64_STATUS_OK);
      T64_CHECK_INT(narrow_sequence_fixture.machine->next_sequence, 255u);
      T64_CHECK_INT(t64_test_schedule_synthetic(
                        narrow_sequence_fixture.machine, 0u,
                        T64_TEST_EVENT_NOOP, 255u),
                    T64_STATUS_OK);
      T64_CHECK(narrow_sequence_fixture.machine->sequence_exhausted);
      T64_CHECK_INT(narrow_sequence_fixture.machine->queue_count, 2u);
      T64_CHECK_INT(t64_machine_digest(narrow_sequence_fixture.machine,
                                       digest_before),
                    T64_STATUS_OK);
      T64_CHECK_INT(t64_test_reserve_synthetic(
                        narrow_sequence_fixture.machine, 0u,
                        T64_TEST_EVENT_NOOP, 256u, &reservation),
                    T64_STATUS_SEQUENCE_EXHAUSTED);
      T64_CHECK_INT(t64_machine_digest(narrow_sequence_fixture.machine,
                                       digest_after),
                    T64_STATUS_OK);
      T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) ==
                0);
      T64_CHECK_INT(narrow_sequence_fixture.machine->queue_count, 2u);
      T64_CHECK_INT(narrow_sequence_fixture.machine->next_sequence, 255u);
      fixture_destroy(&narrow_sequence_fixture);
    }
  }

  T64_CHECK(fixture_init(&sequence_fixture, 9u));
  if (sequence_fixture.machine == NULL) {
    return;
  }
  sequence_fixture.machine->next_sequence = UINT64_MAX;
  T64_CHECK_INT(t64_test_reserve_synthetic(sequence_fixture.machine, 0u,
                                            T64_TEST_EVENT_NOOP, 1u,
                                            &reservation),
                T64_STATUS_OK);
  T64_CHECK_INT(sequence_fixture.machine->queue_count, 0u);
  T64_CHECK_INT(sequence_fixture.machine->next_sequence, UINT64_MAX);
  T64_CHECK_INT(t64_schedule_commit(sequence_fixture.machine, &reservation),
                T64_STATUS_OK);
  T64_CHECK(sequence_fixture.machine->sequence_exhausted);
  T64_CHECK_INT(sequence_fixture.machine->events[
                    sequence_fixture.machine->queue_head]
                    .sequence,
                UINT64_MAX);
  T64_CHECK_INT(t64_test_schedule_synthetic(sequence_fixture.machine, 0u,
                                            T64_TEST_EVENT_NOOP, 2u),
                T64_STATUS_SEQUENCE_EXHAUSTED);
  T64_CHECK_INT(sequence_fixture.machine->queue_count, 1u);
  T64_CHECK_INT(sequence_fixture.machine->next_sequence, UINT64_MAX);
  fixture_destroy(&sequence_fixture);

  T64_CHECK(fixture_init(&tick_fixture, 10u));
  if (tick_fixture.machine != NULL) {
    tick_fixture.machine->tick = 3u;
    T64_CHECK_INT(t64_test_schedule_synthetic(tick_fixture.machine, 2u,
                                              T64_TEST_EVENT_NOOP, 0u),
                  T64_STATUS_TIME_REVERSED);
    T64_CHECK_INT(tick_fixture.machine->queue_count, 0u);
    T64_CHECK_INT(tick_fixture.machine->next_sequence, 0u);
    tick_fixture.machine->tick = UINT64_MAX;
    T64_CHECK_INT(t64_test_schedule_synthetic_after(
                      tick_fixture.machine, 1u, T64_TEST_EVENT_NOOP, 0u),
                  T64_STATUS_TICK_OVERFLOW);
    T64_CHECK_INT(tick_fixture.machine->queue_count, 0u);
    T64_CHECK_INT(tick_fixture.machine->next_sequence, 0u);
    T64_CHECK_INT(t64_test_schedule_synthetic_after(
                      tick_fixture.machine, 0u, T64_TEST_EVENT_NOOP, 0u),
                  T64_STATUS_OK);
    T64_CHECK_INT(tick_fixture.machine->events[tick_fixture.machine->queue_head]
                      .tick,
                  UINT64_MAX);
    {
      t64_stop_request max_request = {T64_STOP_GUEST_TICK, UINT64_MAX};
      t64_stop_result max_result = {0u, 0u};

      T64_CHECK_INT(t64_machine_advance(tick_fixture.machine, &max_request,
                                        &max_result),
                    T64_STATUS_OK);
      T64_CHECK_INT(max_result.reason, T64_STOP_REASON_BOUND);
      T64_CHECK_INT(max_result.tick, UINT64_MAX);
      T64_CHECK_INT(tick_fixture.machine->queue_count, 0u);
    }
    fixture_destroy(&tick_fixture);
  }

  T64_CHECK(fixture_init(&max_advance_fixture, 14u));
  if (max_advance_fixture.machine != NULL) {
    t64_stop_request max_request = {T64_STOP_GUEST_TICK, UINT64_MAX};
    t64_stop_result max_result = {0u, 0u};

    T64_CHECK_INT(t64_machine_advance(max_advance_fixture.machine, &max_request,
                                      &max_result),
                  T64_STATUS_OK);
    T64_CHECK_INT(max_advance_fixture.machine->tick, UINT64_MAX);
    T64_CHECK_INT(max_result.tick, UINT64_MAX);
    max_request.value = UINT64_MAX - 1u;
    T64_CHECK_INT(t64_machine_advance(max_advance_fixture.machine, &max_request,
                                      &max_result),
                  T64_STATUS_TIME_REVERSED);
    T64_CHECK_INT(max_advance_fixture.machine->tick, UINT64_MAX);
    fixture_destroy(&max_advance_fixture);
  }
}

static void test_successful_rom_replacement_resets_scheduler_state(void) {
  machine_fixture fixture;
  uint8_t replacement[TEST_ROM_SIZE];
  t64_stop_request request = {T64_STOP_GUEST_TICK, 4u};
  t64_stop_result result = {0u, 0u};

  T64_CHECK(fixture_init(&fixture, 15u));
  if (fixture.machine == NULL) {
    return;
  }
  T64_CHECK_INT(t64_test_schedule_synthetic(fixture.machine, 8u,
                                            T64_TEST_EVENT_INSTRUCTION, 3u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_advance(fixture.machine, &request, &result),
                T64_STATUS_OK);
  fixture.machine->instruction_count = 7u;
  fixture.machine->frame_event_count = 5u;
  fixture.machine->sticky_error_status = T64_STATUS_INVALID_ROM;
  fixture.machine->sequence_exhausted = true;
  fill_authored_rom(replacement, 16u);
  T64_CHECK_INT(t64_machine_admit_rom(fixture.machine, replacement,
                                      sizeof replacement),
                T64_STATUS_OK);
  T64_CHECK_INT(fixture.machine->tick, 0u);
  T64_CHECK_INT(fixture.machine->instruction_count, 0u);
  T64_CHECK_INT(fixture.machine->frame_event_count, 0u);
  T64_CHECK_INT(fixture.machine->sticky_error_status, T64_STATUS_OK);
  T64_CHECK(!fixture.machine->halted);
  T64_CHECK_INT(fixture.machine->queue_count, 0u);
  T64_CHECK_INT(fixture.machine->queue_head, 0u);
  T64_CHECK_INT(fixture.machine->next_sequence, 0u);
  T64_CHECK(!fixture.machine->sequence_exhausted);
  fixture_destroy(&fixture);
}

static void test_counter_overflow_and_event_budget_preserve_pending_work(void) {
  machine_fixture overflow_fixture;
  machine_fixture budget_fixture;
  t64_stop_request request = {T64_STOP_INSTRUCTION_COUNT, UINT64_MAX};
  t64_stop_result result = {0u, 0u};

  T64_CHECK(fixture_init(&overflow_fixture, 11u));
  if (overflow_fixture.machine == NULL) {
    return;
  }
  overflow_fixture.machine->instruction_count = UINT64_MAX - 1u;
  T64_CHECK_INT(t64_test_schedule_synthetic(overflow_fixture.machine, 4u,
                                            T64_TEST_EVENT_INSTRUCTION, 2u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_advance(overflow_fixture.machine, &request,
                                    &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_RESOURCE_LIMIT);
  T64_CHECK_INT(result.tick, 0u);
  T64_CHECK_INT(overflow_fixture.machine->instruction_count, UINT64_MAX - 1u);
  T64_CHECK_INT(overflow_fixture.machine->queue_count, 1u);
  T64_CHECK_INT(overflow_fixture.machine->events[
                    overflow_fixture.machine->queue_head]
                    .tick,
                4u);
  fixture_destroy(&overflow_fixture);

  T64_CHECK(fixture_init(&budget_fixture, 12u));
  if (budget_fixture.machine != NULL) {
    T64_CHECK_INT(t64_test_schedule_synthetic(
                      budget_fixture.machine, 0u,
                      T64_TEST_EVENT_REPEAT_INSTRUCTION, 1u),
                  T64_STATUS_OK);
    T64_CHECK_INT(t64_machine_advance(budget_fixture.machine, &request,
                                      &result),
                  T64_STATUS_OK);
    T64_CHECK_INT(result.reason, T64_STOP_REASON_RESOURCE_LIMIT);
    T64_CHECK_INT(result.tick, 0u);
    T64_CHECK_INT(budget_fixture.machine->instruction_count, 4096u);
    T64_CHECK_INT(budget_fixture.machine->queue_count, 1u);
    T64_CHECK_INT(budget_fixture.machine->events[
                      budget_fixture.machine->queue_head]
                      .sequence,
                  4096u);
    fixture_destroy(&budget_fixture);
  }
}

static void test_one_shot_and_partitioned_time_advance_match(void) {
  machine_fixture one_shot;
  machine_fixture partitioned;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(10)};
  t64_stop_result result = {0u, 0u};
  uint8_t one_shot_digest[T64_DIGEST_SIZE];
  uint8_t partitioned_digest[T64_DIGEST_SIZE];

  T64_CHECK(fixture_init(&one_shot, 13u));
  T64_CHECK(fixture_init(&partitioned, 13u));
  if (one_shot.machine == NULL || partitioned.machine == NULL) {
    fixture_destroy(&one_shot);
    fixture_destroy(&partitioned);
    return;
  }
  T64_CHECK_INT(t64_test_schedule_synthetic(one_shot.machine, 3u,
                                            T64_TEST_EVENT_INSTRUCTION, 2u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_test_schedule_synthetic(one_shot.machine, 5u,
                                            T64_TEST_EVENT_FRAME, 1u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_test_schedule_synthetic(one_shot.machine, 7u,
                                            T64_TEST_EVENT_INSTRUCTION, 3u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_test_schedule_synthetic(partitioned.machine, 3u,
                                            T64_TEST_EVENT_INSTRUCTION, 2u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_test_schedule_synthetic(partitioned.machine, 5u,
                                            T64_TEST_EVENT_FRAME, 1u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_test_schedule_synthetic(partitioned.machine, 7u,
                                            T64_TEST_EVENT_INSTRUCTION, 3u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_advance(one_shot.machine, &request, &result),
                T64_STATUS_OK);
  request.value = 5u;
  T64_CHECK_INT(t64_machine_advance(partitioned.machine, &request, &result),
                T64_STATUS_OK);
  request.value = 10u;
  T64_CHECK_INT(t64_machine_advance(partitioned.machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(one_shot.machine->tick, partitioned.machine->tick);
  T64_CHECK_INT(one_shot.machine->instruction_count,
                partitioned.machine->instruction_count);
  T64_CHECK_INT(one_shot.machine->frame_event_count,
                partitioned.machine->frame_event_count);
  T64_CHECK_INT(one_shot.machine->next_sequence,
                partitioned.machine->next_sequence);
  T64_CHECK_INT(one_shot.machine->queue_count,
                partitioned.machine->queue_count);
  T64_CHECK_INT(t64_machine_digest(one_shot.machine, one_shot_digest),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(partitioned.machine, partitioned_digest),
                T64_STATUS_OK);
  T64_CHECK(memcmp(one_shot_digest, partitioned_digest,
                   T64_DIGEST_SIZE) == 0);
  fixture_destroy(&one_shot);
  fixture_destroy(&partitioned);
}

int main(void) {
  (void)puts("evidence: unit-tested synthetic counter events; no guest execution");
  test_unattainable_instruction_bound_and_counter_refusal();
  test_inclusive_time_bounds_idle_advance_and_order();
  test_fifo_counter_stop_leaves_later_same_tick_event();
  test_halt_precedence_at_time_and_counter_bounds();
  test_queue_sequence_and_tick_exhaustion_are_transactional();
  test_counter_overflow_and_event_budget_preserve_pending_work();
  test_one_shot_and_partitioned_time_advance_match();
  test_successful_rom_replacement_resets_scheduler_state();
  return T64_TEST_FINISH("unit.core.scheduler");
}
