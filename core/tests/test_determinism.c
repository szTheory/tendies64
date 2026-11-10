#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_internal.h"
#include "t64_test.h"

#define ROM_BYTES 4096u
#define PI_BASE UINT32_C(0x04600000)
#define SP_BASE UINT32_C(0x04040000)
#define MI_PENDING UINT32_C(0x04300008)
#define CART_BASE UINT32_C(0x10000000)
#define PI_DEST UINT32_C(0x00000100)
#define PI_TICK UINT64_C(13)
#define SP_PENDING_DEST UINT32_C(0x04001020)
#define OVERFLOW_WRITES 300u

typedef struct {
  size_t allocations;
  size_t releases;
  size_t outstanding;
} allocator_state;

typedef struct {
  allocator_state host;
  t64_host_allocator allocator;
  t64_machine *machine;
  uint8_t image[ROM_BYTES];
} fixture;

static const uint8_t pi_expected_salt1[24] = {
    0x0bu, 0x2au, 0x49u, 0x68u, 0x87u, 0xa6u, 0xc5u, 0xe4u,
    0x03u, 0x22u, 0x41u, 0x60u, 0x7fu, 0x9eu, 0xbdu, 0xdcu,
    0xfbu, 0x1au, 0x39u, 0x58u, 0x77u, 0x96u, 0xb5u, 0xd4u};
static const uint8_t pi_expected_salt2[24] = {
    0x16u, 0x35u, 0x54u, 0x73u, 0x92u, 0xb1u, 0xd0u, 0xefu,
    0x0eu, 0x2du, 0x4cu, 0x6bu, 0x8au, 0xa9u, 0xc8u, 0xe7u,
    0x06u, 0x25u, 0x44u, 0x63u, 0x82u, 0xa1u, 0xc0u, 0xdfu};
static const uint8_t pi_expected_cancel[16] = {
    0x21u, 0x40u, 0x5fu, 0x7eu, 0x9du, 0xbcu, 0xdbu, 0xfau,
    0x19u, 0x38u, 0x57u, 0x76u, 0x95u, 0xb4u, 0xd3u, 0xf2u};
static const uint8_t sp_active_expected[8] = {
    0x20u, 0x21u, 0x22u, 0x23u, 0x24u, 0x25u, 0x26u, 0x27u};

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  allocator_state *state = (allocator_state *)context;
  void *memory;

  if (state == NULL || storage_out == NULL || size == 0u || alignment == 0u) {
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

static void fill_authored_rom(uint8_t image[ROM_BYTES], uint8_t salt) {
  size_t i;
  for (i = 0u; i < ROM_BYTES; i++) {
    image[i] = (uint8_t)((i * 31u + (size_t)salt * 11u) & 0xffu);
  }
  image[0] = 0x80u;
  image[1] = 0x37u;
  image[2] = 0x12u;
  image[3] = 0x40u;
  image[0x3eu] = (uint8_t)'E';
}

static bool fixture_init(fixture *value, uint8_t salt) {
  const t64_profile_descriptor *profile = NULL;
  (void)memset(value, 0, sizeof *value);
  value->allocator.allocate = test_allocate;
  value->allocator.release = test_release;
  value->allocator.context = &value->host;
  fill_authored_rom(value->image, salt);
  if (t64_profile_lookup("ntsc-cart-4m-skeleton", &profile) !=
          T64_STATUS_OK ||
      profile == NULL ||
      t64_machine_create(&value->allocator, profile, &value->machine) !=
          T64_STATUS_OK ||
      value->machine == NULL ||
      t64_machine_admit_rom(value->machine, value->image,
                            sizeof value->image) != T64_STATUS_OK) {
    if (value->machine != NULL) {
      t64_machine_destroy(value->machine);
      value->machine = NULL;
    }
    return false;
  }
  return true;
}

static void fixture_destroy(fixture *value) {
  if (value->machine != NULL) {
    t64_machine_destroy(value->machine);
    value->machine = NULL;
  }
  T64_CHECK_INT(value->host.outstanding, 0u);
}

static t64_status bus_write8(t64_machine *machine, uint32_t address,
                             uint8_t value) {
  return t64_machine_bus_write(machine, address, 1u, (uint32_t)value);
}

static t64_status bus_read8(t64_machine *machine, uint32_t address,
                            uint8_t *value_out) {
  uint32_t value = 0u;
  t64_status status = t64_machine_bus_read(machine, address, 1u, &value);
  if (status == T64_STATUS_OK) {
    *value_out = (uint8_t)value;
  }
  return status;
}

static t64_status bus_read32(t64_machine *machine, uint32_t address,
                             uint32_t *value_out) {
  return t64_machine_bus_read(machine, address, 4u, value_out);
}

static t64_status bus_write32(t64_machine *machine, uint32_t address,
                              uint32_t value) {
  return t64_machine_bus_write(machine, address, 4u, value);
}

static bool advance_without_host_calls(fixture *value, uint32_t stop_kind,
                                       uint64_t limit,
                                       t64_stop_result *result_out) {
  t64_stop_request request = {stop_kind, limit};
  size_t allocations = value->host.allocations;
  size_t releases = value->host.releases;
  t64_status status = t64_machine_advance(value->machine, &request,
                                          result_out);

  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(value->host.allocations, allocations);
  T64_CHECK_INT(value->host.releases, releases);
  return status == T64_STATUS_OK;
}

static bool start_cross_device_work(fixture *value, bool trace_enabled) {
  if (t64_machine_trace_set_enabled(value->machine, trace_enabled) !=
      T64_STATUS_OK) {
    return false;
  }
  return bus_write32(value->machine, PI_BASE, PI_DEST) == T64_STATUS_OK &&
         bus_write32(value->machine, PI_BASE + 4u,
                     CART_BASE + UINT32_C(0x100)) == T64_STATUS_OK &&
         bus_write32(value->machine, PI_BASE + 12u, UINT32_C(23)) ==
             T64_STATUS_OK &&
         bus_write32(value->machine, SP_BASE, 0u) == T64_STATUS_OK &&
         bus_write32(value->machine, SP_BASE + 4u, PI_DEST) == T64_STATUS_OK &&
         bus_write32(value->machine, SP_BASE + 8u, 0u) == T64_STATUS_OK;
}

static void expect_zeros(t64_machine *machine, uint32_t address, size_t count) {
  uint8_t actual = UINT8_MAX;
  size_t i;
  for (i = 0u; i < count; i++) {
    T64_CHECK_INT(bus_read8(machine, address + (uint32_t)i, &actual),
                  T64_STATUS_OK);
    T64_CHECK_INT(actual, 0u);
  }
}

static void expect_bytes(t64_machine *machine, uint32_t address,
                         const uint8_t *expected, size_t count) {
  uint8_t actual = 0u;
  size_t i;
  for (i = 0u; i < count; i++) {
    T64_CHECK_INT(bus_read8(machine, address + (uint32_t)i, &actual),
                  T64_STATUS_OK);
    T64_CHECK_INT(actual, expected[i]);
  }
}

static void check_same_tick_trace_order(const t64_trace_record *records,
                                        size_t count) {
  size_t pi_complete = SIZE_MAX;
  size_t sp_complete = SIZE_MAX;
  size_t i;
  for (i = 0u; i < count; i++) {
    if (records[i].tick == PI_TICK &&
        records[i].device == T64_TRACE_DEVICE_PI &&
        records[i].kind == T64_TRACE_KIND_PI_COMPLETE) {
      pi_complete = i;
    }
    if (records[i].tick == PI_TICK &&
        records[i].device == T64_TRACE_DEVICE_SP &&
        records[i].kind == T64_TRACE_KIND_SP_COMPLETE) {
      sp_complete = i;
    }
  }
  T64_CHECK(pi_complete != SIZE_MAX);
  T64_CHECK(sp_complete != SIZE_MAX);
  T64_CHECK(pi_complete < sp_complete);
}

static bool complete_cross_device_work(fixture *value, bool partitioned,
                                       const uint8_t expected[24],
                                       uint8_t digest_out[T64_DIGEST_SIZE],
                                       bool drain_trace) {
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  uint32_t value32 = 0u;
  t64_trace_record records[T64_TRACE_RECORD_CAPACITY];
  size_t record_count = 0u;
  uint64_t dropped = UINT64_MAX;
  bool saturated = true;

  if (partitioned) {
    (void)advance_without_host_calls(value, T64_STOP_GUEST_TICK, 4u, &result);
    T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
    T64_CHECK_INT(result.tick, 4u);
    (void)advance_without_host_calls(value, T64_STOP_GUEST_TICK, 12u, &result);
  } else {
    (void)advance_without_host_calls(value, T64_STOP_GUEST_TICK, 12u, &result);
  }
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, 12u);
  T64_CHECK_INT(bus_read32(value->machine, PI_BASE + 16u, &value32),
                T64_STATUS_OK);
  T64_CHECK_INT(value32 & 1u, 1u);
  T64_CHECK_INT(bus_read32(value->machine, SP_BASE + 0x18u, &value32),
                T64_STATUS_OK);
  T64_CHECK_INT(value32, 1u);
  expect_zeros(value->machine, PI_DEST, sizeof pi_expected_salt1);
  expect_zeros(value->machine, UINT32_C(0x04000000), 8u);
  if (drain_trace) {
    t64_trace_reset(value->machine);
  }

  (void)advance_without_host_calls(value, T64_STOP_GUEST_TICK, PI_TICK,
                                   &result);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, PI_TICK);
  if (drain_trace) {
    T64_CHECK_INT(t64_machine_trace_drain(
                      value->machine, records, T64_TRACE_RECORD_CAPACITY,
                      &record_count, &dropped, &saturated),
                  T64_STATUS_OK);
    T64_CHECK_INT(record_count, 5u);
    T64_CHECK_INT(dropped, 0u);
    T64_CHECK(!saturated);
    check_same_tick_trace_order(records, record_count);
  }
  expect_bytes(value->machine, PI_DEST, expected, 24u);
  expect_bytes(value->machine, UINT32_C(0x04000000), expected, 8u);
  T64_CHECK_INT(bus_read32(value->machine, PI_BASE + 16u, &value32),
                T64_STATUS_OK);
  T64_CHECK_INT(value32 & 1u, 0u);
  T64_CHECK_INT(bus_read32(value->machine, SP_BASE + 0x18u, &value32),
                T64_STATUS_OK);
  T64_CHECK_INT(value32, 0u);
  T64_CHECK_INT(bus_read32(value->machine, MI_PENDING, &value32),
                T64_STATUS_OK);
  T64_CHECK_INT(value32, UINT32_C(0x10));
  T64_CHECK_INT(value->machine->instruction_count, 0u);
  T64_CHECK_INT(value->machine->frame_event_count, 0u);
  T64_CHECK_INT(value->machine->queue_count, 0u);
  T64_CHECK_INT(t64_machine_digest(value->machine, digest_out), T64_STATUS_OK);
  return true;
}

static void test_interleaved_partitioned_cross_device_determinism(void) {
  fixture isolated_a;
  fixture isolated_b;
  fixture partitioned_a;
  fixture interleaved_a;
  fixture interleaved_b;
  uint8_t digest_a[T64_DIGEST_SIZE];
  uint8_t digest_b[T64_DIGEST_SIZE];
  uint8_t digest_partitioned[T64_DIGEST_SIZE];
  uint8_t digest_interleaved_a[T64_DIGEST_SIZE];
  uint8_t digest_interleaved_b[T64_DIGEST_SIZE];
  t64_stop_result result = {0u, 0u};

  T64_CHECK(fixture_init(&isolated_a, 1u));
  T64_CHECK(fixture_init(&isolated_b, 2u));
  T64_CHECK(fixture_init(&partitioned_a, 1u));
  T64_CHECK(fixture_init(&interleaved_a, 1u));
  T64_CHECK(fixture_init(&interleaved_b, 2u));
  if (isolated_a.machine == NULL || isolated_b.machine == NULL ||
      partitioned_a.machine == NULL || interleaved_a.machine == NULL ||
      interleaved_b.machine == NULL) {
    fixture_destroy(&isolated_a);
    fixture_destroy(&isolated_b);
    fixture_destroy(&partitioned_a);
    fixture_destroy(&interleaved_a);
    fixture_destroy(&interleaved_b);
    return;
  }
  T64_CHECK(start_cross_device_work(&isolated_a, false));
  T64_CHECK(start_cross_device_work(&isolated_b, false));
  T64_CHECK(start_cross_device_work(&partitioned_a, false));
  T64_CHECK(start_cross_device_work(&interleaved_a, false));
  T64_CHECK(start_cross_device_work(&interleaved_b, false));
  (void)complete_cross_device_work(&isolated_a, false, pi_expected_salt1,
                                   digest_a, false);
  (void)complete_cross_device_work(&isolated_b, false, pi_expected_salt2,
                                   digest_b, false);
  (void)complete_cross_device_work(&partitioned_a, true, pi_expected_salt1,
                                   digest_partitioned, false);

  (void)advance_without_host_calls(&interleaved_a, T64_STOP_GUEST_TICK, 5u,
                                   &result);
  (void)advance_without_host_calls(&interleaved_b, T64_STOP_GUEST_TICK, 9u,
                                   &result);
  (void)advance_without_host_calls(&interleaved_a, T64_STOP_GUEST_TICK, 12u,
                                   &result);
  (void)advance_without_host_calls(&interleaved_b, T64_STOP_GUEST_TICK, 12u,
                                   &result);
  (void)advance_without_host_calls(&interleaved_a, T64_STOP_GUEST_TICK, 13u,
                                   &result);
  (void)advance_without_host_calls(&interleaved_b, T64_STOP_GUEST_TICK, 13u,
                                   &result);
  expect_bytes(interleaved_a.machine, PI_DEST, pi_expected_salt1, 24u);
  expect_bytes(interleaved_a.machine, UINT32_C(0x04000000),
               pi_expected_salt1, 8u);
  expect_bytes(interleaved_b.machine, PI_DEST, pi_expected_salt2, 24u);
  expect_bytes(interleaved_b.machine, UINT32_C(0x04000000),
               pi_expected_salt2, 8u);
  T64_CHECK_INT(t64_machine_digest(interleaved_a.machine, digest_interleaved_a),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(interleaved_b.machine, digest_interleaved_b),
                T64_STATUS_OK);
  T64_CHECK(memcmp(digest_a, digest_partitioned, T64_DIGEST_SIZE) == 0);
  T64_CHECK(memcmp(digest_a, digest_interleaved_a, T64_DIGEST_SIZE) == 0);
  T64_CHECK(memcmp(digest_b, digest_interleaved_b, T64_DIGEST_SIZE) == 0);

  fixture_destroy(&isolated_a);
  fixture_destroy(&isolated_b);
  fixture_destroy(&partitioned_a);
  fixture_destroy(&interleaved_a);
  fixture_destroy(&interleaved_b);
}

static void test_trace_modes_and_overflow_are_observational(void) {
  fixture untraced;
  fixture traced;
  fixture overflow_untraced;
  fixture overflow_traced;
  uint8_t digest_untraced[T64_DIGEST_SIZE];
  uint8_t digest_traced[T64_DIGEST_SIZE];
  uint8_t digest_overflow_untraced[T64_DIGEST_SIZE];
  uint8_t digest_overflow_traced[T64_DIGEST_SIZE];
  t64_trace_record records[T64_TRACE_RECORD_CAPACITY];
  t64_stop_result result = {0u, 0u};
  size_t record_count = 0u;
  uint64_t dropped = 0u;
  bool saturated = false;
  size_t i;

  T64_CHECK(fixture_init(&untraced, 1u));
  T64_CHECK(fixture_init(&traced, 1u));
  if (untraced.machine == NULL || traced.machine == NULL) {
    fixture_destroy(&untraced);
    fixture_destroy(&traced);
    return;
  }
  T64_CHECK(start_cross_device_work(&untraced, false));
  T64_CHECK(start_cross_device_work(&traced, true));
  (void)complete_cross_device_work(&untraced, false, pi_expected_salt1,
                                   digest_untraced, false);
  (void)complete_cross_device_work(&traced, false, pi_expected_salt1,
                                   digest_traced, true);
  T64_CHECK(memcmp(digest_untraced, digest_traced, T64_DIGEST_SIZE) == 0);
  fixture_destroy(&untraced);
  fixture_destroy(&traced);

  T64_CHECK(fixture_init(&overflow_untraced, 1u));
  T64_CHECK(fixture_init(&overflow_traced, 1u));
  if (overflow_untraced.machine == NULL || overflow_traced.machine == NULL) {
    fixture_destroy(&overflow_untraced);
    fixture_destroy(&overflow_traced);
    return;
  }
  T64_CHECK(start_cross_device_work(&overflow_untraced, false));
  T64_CHECK(start_cross_device_work(&overflow_traced, true));
  t64_trace_reset(overflow_traced.machine);
  for (i = 0u; i < OVERFLOW_WRITES; i++) {
    uint8_t value = (uint8_t)(i * 7u + 3u);
    T64_CHECK_INT(bus_write8(overflow_untraced.machine,
                             UINT32_C(0x1000) + (uint32_t)i, value),
                  T64_STATUS_OK);
    T64_CHECK_INT(bus_write8(overflow_traced.machine,
                             UINT32_C(0x1000) + (uint32_t)i, value),
                  T64_STATUS_OK);
  }
  (void)advance_without_host_calls(&overflow_untraced, T64_STOP_GUEST_TICK,
                                   PI_TICK, &result);
  (void)advance_without_host_calls(&overflow_traced, T64_STOP_GUEST_TICK,
                                   PI_TICK, &result);
  T64_CHECK_INT(t64_machine_trace_drain(
                    overflow_traced.machine, records, T64_TRACE_RECORD_CAPACITY,
                    &record_count, &dropped, &saturated),
                T64_STATUS_OK);
  T64_CHECK_INT(record_count, T64_TRACE_RECORD_CAPACITY);
  T64_CHECK_INT(dropped, UINT64_C(49));
  T64_CHECK(!saturated);
  T64_CHECK_INT(t64_machine_digest(overflow_untraced.machine,
                                   digest_overflow_untraced),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(overflow_traced.machine,
                                   digest_overflow_traced),
                T64_STATUS_OK);
  T64_CHECK(memcmp(digest_overflow_untraced, digest_overflow_traced,
                   T64_DIGEST_SIZE) == 0);
  expect_bytes(overflow_traced.machine, PI_DEST, pi_expected_salt1, 24u);
  expect_bytes(overflow_traced.machine, UINT32_C(0x04000000),
               pi_expected_salt1, 8u);
  {
    uint8_t actual = 0u;
    T64_CHECK_INT(bus_read8(overflow_traced.machine, UINT32_C(0x1000),
                            &actual),
                  T64_STATUS_OK);
    T64_CHECK_INT(actual, 3u);
    T64_CHECK_INT(bus_read8(overflow_traced.machine,
                            UINT32_C(0x1000) + OVERFLOW_WRITES - 1u,
                            &actual),
                  T64_STATUS_OK);
    T64_CHECK_INT(actual, (OVERFLOW_WRITES - 1u) * 7u + 3u & 0xffu);
  }
  fixture_destroy(&overflow_untraced);
  fixture_destroy(&overflow_traced);
}

static void schedule_counter_work(fixture *value) {
  T64_CHECK_INT(t64_test_schedule_synthetic(value->machine, 3u,
                                            T64_TEST_EVENT_INSTRUCTION, 2u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_test_schedule_synthetic(value->machine, 5u,
                                            T64_TEST_EVENT_FRAME, 1u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_test_schedule_synthetic(value->machine, 7u,
                                            T64_TEST_EVENT_INSTRUCTION, 3u),
                T64_STATUS_OK);
}

static void test_guest_instruction_and_frame_bounds(void) {
  fixture one_shot;
  fixture partitioned;
  fixture instruction_bound;
  fixture frame_bound;
  t64_stop_request request;
  t64_stop_result result = {0u, 0u};
  uint8_t digest_a[T64_DIGEST_SIZE];
  uint8_t digest_b[T64_DIGEST_SIZE];

  T64_CHECK(fixture_init(&one_shot, 3u));
  T64_CHECK(fixture_init(&partitioned, 3u));
  T64_CHECK(fixture_init(&instruction_bound, 3u));
  T64_CHECK(fixture_init(&frame_bound, 3u));
  if (one_shot.machine == NULL || partitioned.machine == NULL ||
      instruction_bound.machine == NULL || frame_bound.machine == NULL) {
    fixture_destroy(&one_shot);
    fixture_destroy(&partitioned);
    fixture_destroy(&instruction_bound);
    fixture_destroy(&frame_bound);
    return;
  }
  schedule_counter_work(&one_shot);
  schedule_counter_work(&partitioned);
  (void)advance_without_host_calls(&one_shot, T64_STOP_GUEST_TICK, 10u,
                                   &result);
  (void)advance_without_host_calls(&partitioned, T64_STOP_GUEST_TICK, 4u,
                                   &result);
  (void)advance_without_host_calls(&partitioned, T64_STOP_GUEST_TICK, 6u,
                                   &result);
  (void)advance_without_host_calls(&partitioned, T64_STOP_GUEST_TICK, 10u,
                                   &result);
  T64_CHECK_INT(one_shot.machine->tick, 10u);
  T64_CHECK_INT(one_shot.machine->instruction_count, 5u);
  T64_CHECK_INT(one_shot.machine->frame_event_count, 1u);
  T64_CHECK_INT(partitioned.machine->instruction_count, 5u);
  T64_CHECK_INT(partitioned.machine->frame_event_count, 1u);
  T64_CHECK_INT(t64_machine_digest(one_shot.machine, digest_a), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(partitioned.machine, digest_b),
                T64_STATUS_OK);
  T64_CHECK(memcmp(digest_a, digest_b, T64_DIGEST_SIZE) == 0);

  T64_CHECK_INT(t64_test_schedule_synthetic(
                    instruction_bound.machine, 2u,
                    T64_TEST_EVENT_INSTRUCTION, 2u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_test_schedule_synthetic(instruction_bound.machine, 4u,
                                            T64_TEST_EVENT_FRAME, 1u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_test_schedule_synthetic(
                    instruction_bound.machine, 6u,
                    T64_TEST_EVENT_INSTRUCTION, 3u),
                T64_STATUS_OK);
  request.kind = T64_STOP_INSTRUCTION_COUNT;
  request.value = 3u;
  (void)advance_without_host_calls(&instruction_bound, request.kind,
                                   request.value, &result);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_INSTRUCTION_COUNT);
  T64_CHECK_INT(result.tick, 6u);
  T64_CHECK_INT(instruction_bound.machine->instruction_count, 5u);
  T64_CHECK_INT(instruction_bound.machine->frame_event_count, 1u);

  T64_CHECK_INT(t64_test_schedule_synthetic(frame_bound.machine, 3u,
                                            T64_TEST_EVENT_FRAME, 2u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_test_schedule_synthetic(
                    frame_bound.machine, 5u,
                    T64_TEST_EVENT_INSTRUCTION, 7u),
                T64_STATUS_OK);
  request.kind = T64_STOP_FRAME_EVENT_COUNT;
  request.value = 2u;
  (void)advance_without_host_calls(&frame_bound, request.kind, request.value,
                                   &result);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_FRAME_EVENT_COUNT);
  T64_CHECK_INT(result.tick, 3u);
  T64_CHECK_INT(frame_bound.machine->instruction_count, 0u);
  T64_CHECK_INT(frame_bound.machine->frame_event_count, 2u);
  T64_CHECK_INT(frame_bound.machine->queue_count, 1u);

  fixture_destroy(&one_shot);
  fixture_destroy(&partitioned);
  fixture_destroy(&instruction_bound);
  fixture_destroy(&frame_bound);
}

static void test_pi_cancel_and_sp_promotion_after_refused_replacement(void) {
  fixture canceled;
  fixture pending;
  uint8_t invalid_image[ROM_BYTES];
  uint8_t content_before[T64_DIGEST_SIZE];
  uint8_t content_after[T64_DIGEST_SIZE];
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  t64_sp_dma_descriptor active_before;
  t64_sp_dma_descriptor pending_before;
  t64_stop_result result = {0u, 0u};
  uint32_t value = 0u;
  size_t trace_count_before;
  size_t trace_head_before;
  uint64_t trace_drops_before;
  size_t i;

  T64_CHECK(fixture_init(&canceled, 3u));
  if (canceled.machine == NULL) {
    fixture_destroy(&canceled);
    return;
  }
  T64_CHECK_INT(bus_write32(canceled.machine, PI_BASE, PI_DEST), T64_STATUS_OK);
  T64_CHECK_INT(bus_write32(canceled.machine, PI_BASE + 4u,
                            CART_BASE + UINT32_C(0x100)),
                T64_STATUS_OK);
  T64_CHECK_INT(bus_write32(canceled.machine, PI_BASE + 12u, UINT32_C(15)),
                T64_STATUS_OK);
  T64_CHECK_INT(bus_write32(canceled.machine, PI_BASE + 16u, 1u),
                T64_STATUS_OK);
  (void)advance_without_host_calls(&canceled, T64_STOP_GUEST_TICK, 9u,
                                   &result);
  expect_zeros(canceled.machine, PI_DEST, sizeof pi_expected_cancel);
  T64_CHECK_INT(bus_read32(canceled.machine, MI_PENDING, &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, UINT32_C(0x10));
  fixture_destroy(&canceled);

  T64_CHECK(fixture_init(&pending, 3u));
  if (pending.machine == NULL) {
    fixture_destroy(&pending);
    return;
  }
  T64_CHECK_INT(t64_machine_trace_set_enabled(pending.machine, true),
                T64_STATUS_OK);
  T64_CHECK_INT(bus_write32(pending.machine, PI_BASE, PI_DEST), T64_STATUS_OK);
  T64_CHECK_INT(bus_write32(pending.machine, PI_BASE + 4u,
                            CART_BASE + UINT32_C(0x100)),
                T64_STATUS_OK);
  T64_CHECK_INT(bus_write32(pending.machine, PI_BASE + 12u, UINT32_C(15)),
                T64_STATUS_OK);
  for (i = 0u; i < sizeof sp_active_expected; i++) {
    T64_CHECK_INT(bus_write8(pending.machine, UINT32_C(0x200) + (uint32_t)i,
                             sp_active_expected[i]),
                  T64_STATUS_OK);
  }
  for (i = 0u; i < 16u; i++) {
    T64_CHECK_INT(bus_write8(pending.machine, UINT32_C(0x300) + (uint32_t)i,
                             (uint8_t)(UINT8_C(0x90) + (uint8_t)i)),
                  T64_STATUS_OK);
  }
  T64_CHECK_INT(bus_write32(pending.machine, SP_BASE, 0u), T64_STATUS_OK);
  T64_CHECK_INT(bus_write32(pending.machine, SP_BASE + 4u,
                            UINT32_C(0x200)),
                T64_STATUS_OK);
  T64_CHECK_INT(bus_write32(pending.machine, SP_BASE + 8u, 0u),
                T64_STATUS_OK);
  T64_CHECK_INT(bus_write32(pending.machine, SP_BASE, UINT32_C(0x1020)),
                T64_STATUS_OK);
  T64_CHECK_INT(bus_write32(pending.machine, SP_BASE + 4u,
                            UINT32_C(0x300)),
                T64_STATUS_OK);
  T64_CHECK_INT(bus_write32(pending.machine, SP_BASE + 8u, UINT32_C(8)),
                T64_STATUS_OK);
  T64_CHECK(pending.machine->pi.active_valid);
  T64_CHECK(pending.machine->sp.active.valid);
  T64_CHECK(pending.machine->sp.pending.valid);
  T64_CHECK_INT(pending.machine->queue_count, 3u);
  active_before = pending.machine->sp.active;
  pending_before = pending.machine->sp.pending;
  trace_count_before = pending.machine->trace_count;
  trace_head_before = pending.machine->trace_head;
  trace_drops_before = pending.machine->trace_drop_count;
  T64_CHECK_INT(t64_machine_content_digest(pending.machine, content_before),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(pending.machine, digest_before),
                T64_STATUS_OK);

  (void)memcpy(invalid_image, pending.image, sizeof invalid_image);
  invalid_image[0] = 0xdeu;
  invalid_image[1] = 0xadu;
  invalid_image[2] = 0xbeu;
  invalid_image[3] = 0xefu;
  T64_CHECK_INT(t64_machine_admit_rom(pending.machine, invalid_image,
                                      sizeof invalid_image),
                T64_STATUS_INVALID_ROM);
  T64_CHECK_INT(t64_machine_content_digest(pending.machine, content_after),
                T64_STATUS_OK);
  T64_CHECK(memcmp(content_before, content_after, T64_DIGEST_SIZE) == 0);
  T64_CHECK_INT(t64_machine_digest(pending.machine, digest_after),
                T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, T64_DIGEST_SIZE) == 0);
  T64_CHECK_INT(pending.machine->queue_count, 3u);
  T64_CHECK(memcmp(&active_before, &pending.machine->sp.active,
                   sizeof active_before) == 0);
  T64_CHECK(memcmp(&pending_before, &pending.machine->sp.pending,
                   sizeof pending_before) == 0);
  T64_CHECK(pending.machine->pi.active_valid);
  T64_CHECK_INT(pending.machine->trace_count, trace_count_before);
  T64_CHECK_INT(pending.machine->trace_head, trace_head_before);
  T64_CHECK_INT(pending.machine->trace_drop_count, trace_drops_before);
  T64_CHECK_INT(bus_read32(pending.machine, PI_BASE + 16u, &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value & 1u, 1u);
  T64_CHECK_INT(bus_read32(pending.machine, SP_BASE + 0x14u, &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, 1u);
  T64_CHECK_INT(bus_read32(pending.machine, SP_BASE + 0x18u, &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, 1u);

  (void)advance_without_host_calls(&pending, T64_STOP_GUEST_TICK, 13u,
                                   &result);
  expect_bytes(pending.machine, PI_DEST, pi_expected_cancel, 16u);
  expect_bytes(pending.machine, UINT32_C(0x04000000), sp_active_expected,
               sizeof sp_active_expected);
  T64_CHECK(pending.machine->sp.active.valid);
  T64_CHECK(!pending.machine->sp.pending.valid);
  T64_CHECK_INT(pending.machine->sp.active.token, pending_before.token);
  for (i = 0u; i < 16u; i++) {
    T64_CHECK_INT(bus_write8(pending.machine, UINT32_C(0x300) + (uint32_t)i,
                             (uint8_t)(UINT8_C(0x40) + (uint8_t)i)),
                  T64_STATUS_OK);
  }
  (void)advance_without_host_calls(&pending, T64_STOP_GUEST_TICK, 26u,
                                   &result);
  expect_zeros(pending.machine, SP_PENDING_DEST, 16u);
  (void)advance_without_host_calls(&pending, T64_STOP_GUEST_TICK, 27u,
                                   &result);
  for (i = 0u; i < 16u; i++) {
    uint8_t actual = 0u;
    T64_CHECK_INT(bus_read8(pending.machine, SP_PENDING_DEST + (uint32_t)i,
                            &actual),
                  T64_STATUS_OK);
    T64_CHECK_INT(actual, UINT8_C(0x40) + (uint8_t)i);
  }
  T64_CHECK(!pending.machine->sp.active.valid);
  T64_CHECK(!pending.machine->sp.pending.valid);
  fixture_destroy(&pending);
}

static void print_evidence_identity(void) {
  const t64_profile_descriptor *profile = NULL;
  uint8_t digest[T64_DIGEST_SIZE];
  static const char hex_digits[] = "0123456789abcdef";
  char hex[T64_DIGEST_SIZE * 2u + 1u];
  size_t i;

  T64_CHECK_INT(t64_profile_lookup("ntsc-cart-4m-skeleton", &profile),
                T64_STATUS_OK);
  T64_CHECK(profile != NULL);
  if (profile == NULL ||
      t64_profile_hash(profile, digest) != T64_STATUS_OK) {
    return;
  }
  for (i = 0u; i < T64_DIGEST_SIZE; i++) {
    hex[i * 2u] = hex_digits[digest[i] >> 4];
    hex[i * 2u + 1u] = hex_digits[digest[i] & 0x0fu];
  }
  hex[T64_DIGEST_SIZE * 2u] = '\0';
  (void)printf("profile=%s profile_sha256=%s build_id=%s source_revision=%s\n",
               profile->profile_id, hex, t64_build_identity(),
               t64_source_revision());
  (void)puts("evidence=unit-tested synthetic counter events; no guest execution");
}

int main(void) {
  print_evidence_identity();
  test_interleaved_partitioned_cross_device_determinism();
  test_trace_modes_and_overflow_are_observational();
  test_guest_instruction_and_frame_bounds();
  test_pi_cancel_and_sp_promotion_after_refused_replacement();
  return T64_TEST_FINISH("unit.core.determinism");
}
