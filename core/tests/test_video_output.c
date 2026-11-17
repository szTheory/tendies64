#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../t64_internal.h"
#include "tendies64/t64_machine.h"
#include "t64_test.h"

#define TEST_ROM_CAPACITY 65536u
#define TEST_PROFILE_ID \
  "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9"
#define TEST_ORIGIN UINT32_C(0x3000)
#define TEST_VI_BASE UINT32_C(0x04400000)
#define TEST_PACKET_BYTES 16u

static const uint8_t expected_packet[TEST_PACKET_BYTES] = {
    255u, 0u,   0u,   255u, 0u,   255u, 0u,   255u,
    0u,   0u,   255u, 255u, 255u, 255u, 255u, 255u,
};

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  void *memory;
  (void)context;
  if (storage_out == NULL || alignment == 0u) return false;
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

static bool load_test_rom(uint8_t *bytes, size_t capacity,
                          size_t *length_out) {
  const char *path = getenv("T64_TEST_RDP_FILL_PATH");
  FILE *file;
  long length;
  if (path == NULL || path[0] == '\0' || bytes == NULL ||
      length_out == NULL) {
    return false;
  }
  file = fopen(path, "rb");
  if (file == NULL) return false;
  if (fseek(file, 0L, SEEK_END) != 0 || (length = ftell(file)) <= 0 ||
      (unsigned long)length > capacity || fseek(file, 0L, SEEK_SET) != 0) {
    (void)fclose(file);
    return false;
  }
  if (fread(bytes, 1u, (size_t)length, file) != (size_t)length ||
      fclose(file) != 0) {
    return false;
  }
  *length_out = (size_t)length;
  return true;
}

static bool write_vi(t64_machine *machine, uint32_t offset, uint32_t value) {
  const t64_status status =
      t64_machine_bus_write(machine, TEST_VI_BASE + offset, 4u, value);
  T64_CHECK_INT(status, T64_STATUS_OK);
  return status == T64_STATUS_OK;
}

static t64_machine *create_output_machine(void) {
  static const uint16_t source[4] = {
      UINT16_C(0xf801), UINT16_C(0x07c1), UINT16_C(0x003f),
      UINT16_C(0xffff),
  };
  const t64_host_allocator allocator = {test_allocate, test_release, NULL};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t rom[TEST_ROM_CAPACITY];
  size_t rom_length = 0u;
  size_t i;

  T64_CHECK(load_test_rom(rom, sizeof rom, &rom_length));
  if (rom_length == 0u) return NULL;
  T64_CHECK_INT(t64_profile_lookup(TEST_PROFILE_ID, &profile),
                T64_STATUS_OK);
  if (profile == NULL) return NULL;
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  if (machine == NULL) return NULL;
  T64_CHECK_INT(t64_machine_admit_rom(machine, rom, rom_length),
                T64_STATUS_OK);
  for (i = 0u; i < sizeof source / sizeof source[0]; i++) {
    const uint32_t address = TEST_ORIGIN + (uint32_t)(i * 2u);
    T64_CHECK_INT(t64_machine_bus_write(machine, address, 2u, source[i]),
                  T64_STATUS_OK);
    t64_rdp_memory_coverage_write(machine, address, 2u, 8u);
  }
  if (!write_vi(machine, UINT32_C(0x04), TEST_ORIGIN) ||
      !write_vi(machine, UINT32_C(0x08), UINT32_C(4)) ||
      !write_vi(machine, UINT32_C(0x18), UINT32_C(8)) ||
      !write_vi(machine, UINT32_C(0x1c), UINT32_C(15)) ||
      !write_vi(machine, UINT32_C(0x24), UINT32_C(4)) ||
      !write_vi(machine, UINT32_C(0x28), UINT32_C(2)) ||
      !write_vi(machine, UINT32_C(0x30), UINT32_C(0x400)) ||
      !write_vi(machine, UINT32_C(0x34), UINT32_C(0x400)) ||
      !write_vi(machine, UINT32_C(0x00), UINT32_C(2))) {
    t64_machine_destroy(machine);
    return NULL;
  }
  return machine;
}

static const t64_event *find_vi_event(const t64_machine *machine) {
  size_t offset;
  for (offset = 0u; offset < machine->queue_count; offset++) {
    const size_t slot =
        (machine->queue_head + offset) % T64_EVENT_QUEUE_CAPACITY;
    const t64_event *event = &machine->events[slot];
    if (event->device == T64_VI_DEVICE &&
        event->kind == T64_VI_EVENT_FIELD &&
        event->token == machine->vi.event_generation) {
      return event;
    }
  }
  return NULL;
}

static bool advance_one_field(t64_machine *machine) {
  const uint32_t field = machine->vi.field;
  size_t attempt;
  for (attempt = 0u; attempt < 512u; attempt++) {
    const t64_event *event = find_vi_event(machine);
    t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
    t64_status status;
    if (event == NULL) return false;
    status = t64_machine_advance(
        machine, &(t64_stop_request){T64_STOP_GUEST_TICK, event->tick}, &stop);
    if (status != T64_STATUS_OK) return false;
    if (machine->vi.field != field) return true;
    if (stop.reason == T64_STOP_REASON_RESOURCE_LIMIT &&
        machine->vi.packet_count >= T64_VIDEO_QUEUE_CAPACITY) {
      return false;
    }
    if (stop.reason != T64_STOP_REASON_RESOURCE_LIMIT &&
        stop.reason != T64_STOP_REASON_BOUND) {
      return false;
    }
  }
  return false;
}

static bool advance_two_fields(t64_machine *machine) {
  return advance_one_field(machine) && advance_one_field(machine);
}

static bool advance_guest_bound(t64_machine *machine, uint64_t target) {
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  t64_status status;
  if (machine == NULL) {
    T64_CHECK(false);
    return false;
  }
  status = t64_machine_advance(
      machine, &(t64_stop_request){T64_STOP_GUEST_TICK, target}, &stop);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(stop.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(stop.tick, target);
  return status == T64_STATUS_OK && stop.reason == T64_STOP_REASON_BOUND &&
         stop.tick == target;
}

static bool copy_oldest(t64_machine *machine, t64_video_packet_info *info,
                        uint8_t packet[TEST_PACKET_BYTES]) {
  size_t length = 0u;
  if (t64_video_info(machine, info) != T64_STATUS_OK) return false;
  if (t64_video_copy(machine, packet, TEST_PACKET_BYTES, &length) !=
          T64_STATUS_OK ||
      length != TEST_PACKET_BYTES) {
    return false;
  }
  return true;
}

static bool fill_queue_and_block(t64_machine *machine, t64_event *pending) {
  uint32_t field;
  uint64_t period;
  size_t attempt;
  if (!advance_two_fields(machine) ||
      machine->vi.packet_count != T64_VIDEO_QUEUE_CAPACITY) {
    return false;
  }
  field = machine->vi.field;
  period =
      (uint64_t)((machine->vi.active_registers[7] & UINT32_C(0x0fff)) + 1u) *
      (machine->vi.active_registers[6] & UINT32_C(0x03ff));
  if (period == 0u) return false;
  for (attempt = 0u; attempt < 512u; attempt++) {
    const t64_event *event = find_vi_event(machine);
    t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
    t64_event before;
    t64_status status;
    const bool is_field_boundary =
        machine->vi.next_boundary_units == period;
    if (event == NULL) return false;
    before = *event;
    status = t64_machine_advance(
        machine, &(t64_stop_request){T64_STOP_GUEST_TICK, before.tick},
        &stop);
    if (status != T64_STATUS_OK || machine->vi.field != field) return false;
    if (is_field_boundary &&
        stop.reason == T64_STOP_REASON_RESOURCE_LIMIT) {
      event = find_vi_event(machine);
      if (machine->vi.packet_count != T64_VIDEO_QUEUE_CAPACITY ||
          event == NULL || event->tick != before.tick ||
          event->sequence != before.sequence || event->token != before.token ||
          machine->tick > event->tick) {
        return false;
      }
      *pending = *event;
      return true;
    }
    if (is_field_boundary ||
        (stop.reason != T64_STOP_REASON_BOUND &&
         stop.reason != T64_STOP_REASON_RESOURCE_LIMIT)) {
      return false;
    }
  }
  return false;
}

static bool digest_of(const t64_machine *machine,
                      uint8_t digest[T64_DIGEST_SIZE]) {
  return t64_machine_digest(machine, digest) == T64_STATUS_OK;
}

static void case_output_full_pending_boundary(void) {
  t64_machine *machine = create_output_machine();
  t64_event pending = {0};
  if (machine == NULL) return;
  if (!fill_queue_and_block(machine, &pending)) {
    T64_CHECK(false);
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK_INT(machine->vi.next_sequence, UINT64_C(2));
  T64_CHECK_INT(machine->vi.field, 2u);
  T64_CHECK_INT(machine->vi.packet_count, T64_VIDEO_QUEUE_CAPACITY);
  {
    const t64_event *resumed = find_vi_event(machine);
    T64_CHECK(resumed != NULL);
    if (resumed != NULL) {
      T64_CHECK_INT(resumed->tick, pending.tick);
      T64_CHECK_INT(resumed->sequence, pending.sequence);
      T64_CHECK_INT(resumed->token, pending.token);
    }
  }
  t64_machine_destroy(machine);
}

static void case_output_resume_exactly_once(void) {
  t64_machine *machine = create_output_machine();
  t64_event pending = {0};
  t64_video_packet_info info = {0};
  uint8_t packet[TEST_PACKET_BYTES];
  if (machine == NULL) return;
  if (!fill_queue_and_block(machine, &pending) ||
      !copy_oldest(machine, &info, packet)) {
    T64_CHECK(false);
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK_INT(info.sequence, UINT64_C(0));
  T64_CHECK(memcmp(packet, expected_packet, sizeof packet) == 0);
  if (!advance_one_field(machine)) {
    T64_CHECK(false);
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK_INT(machine->vi.field, 3u);
  T64_CHECK_INT(machine->vi.next_sequence, UINT64_C(3));
  T64_CHECK_INT(machine->vi.packet_count, T64_VIDEO_QUEUE_CAPACITY);
  if (!copy_oldest(machine, &info, packet)) {
    T64_CHECK(false);
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK_INT(info.sequence, UINT64_C(1));
  T64_CHECK(memcmp(packet, expected_packet, sizeof packet) == 0);
  if (!copy_oldest(machine, &info, packet)) {
    T64_CHECK(false);
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK_INT(info.sequence, UINT64_C(2));
  T64_CHECK_INT(info.end_tick, pending.tick);
  T64_CHECK(memcmp(packet, expected_packet, sizeof packet) == 0);
  T64_CHECK_INT(t64_video_info(machine, &info), T64_STATUS_NOT_READY);
  t64_machine_destroy(machine);
}

static void case_output_no_overwrite_loss(void) {
  t64_machine *machine = create_output_machine();
  t64_event pending = {0};
  t64_video_packet_info info = {0};
  uint8_t packet[TEST_PACKET_BYTES];
  size_t copy_count = 0u;
  if (machine == NULL) return;
  if (!fill_queue_and_block(machine, &pending) ||
      !copy_oldest(machine, &info, packet)) {
    T64_CHECK(false);
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK_INT(info.sequence, UINT64_C(0));
  T64_CHECK(memcmp(packet, expected_packet, sizeof packet) == 0);
  if (!advance_one_field(machine)) {
    T64_CHECK(false);
    t64_machine_destroy(machine);
    return;
  }
  while (t64_video_info(machine, &info) == T64_STATUS_OK) {
    size_t length = 0u;
    t64_status status =
        t64_video_copy(machine, packet, sizeof packet, &length);
    T64_CHECK_INT(status, T64_STATUS_OK);
    if (status != T64_STATUS_OK) break;
    T64_CHECK_INT(length, TEST_PACKET_BYTES);
    T64_CHECK_INT(info.sequence, UINT64_C(1) + copy_count);
    T64_CHECK(memcmp(packet, expected_packet, sizeof packet) == 0);
    copy_count++;
  }
  T64_CHECK_INT(copy_count, 2u);
  t64_machine_destroy(machine);
}

static void case_output_required_size_no_partial_copy(void) {
  t64_machine *machine = create_output_machine();
  t64_event pending = {0};
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  uint8_t destination[TEST_PACKET_BYTES];
  size_t length = SIZE_MAX;
  size_t i;
  if (machine == NULL) return;
  if (!fill_queue_and_block(machine, &pending)) {
    T64_CHECK(false);
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK(digest_of(machine, before));
  T64_CHECK_INT(t64_video_copy(machine, NULL, 0u, &length),
                T64_STATUS_BUFFER_TOO_SMALL);
  T64_CHECK_INT(length, TEST_PACKET_BYTES);
  (void)memset(destination, UINT8_C(0xa5), sizeof destination);
  length = 7u;
  T64_CHECK_INT(t64_video_copy(machine, destination, sizeof destination - 1u,
                               &length), T64_STATUS_BUFFER_TOO_SMALL);
  T64_CHECK_INT(length, TEST_PACKET_BYTES);
  for (i = 0u; i < sizeof destination; i++)
    T64_CHECK_INT(destination[i], UINT8_C(0xa5));
  T64_CHECK(digest_of(machine, after));
  T64_CHECK(memcmp(before, after, sizeof before) == 0);
  t64_machine_destroy(machine);
}

static void case_output_invalid_overlap_span(void) {
  union {
    size_t length;
    uint8_t bytes[TEST_PACKET_BYTES];
  } overlapping;
  t64_machine *machine = create_output_machine();
  t64_event pending = {0};
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  size_t i;
  if (machine == NULL) return;
  if (!fill_queue_and_block(machine, &pending)) {
    T64_CHECK(false);
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK(digest_of(machine, before));
  (void)memset(overlapping.bytes, UINT8_C(0x5a), sizeof overlapping.bytes);
  T64_CHECK_INT(t64_video_copy(machine, overlapping.bytes,
                               sizeof overlapping.bytes,
                               &overlapping.length),
                T64_STATUS_INVALID_ARGUMENT);
  for (i = 0u; i < sizeof overlapping.bytes; i++)
    T64_CHECK_INT(overlapping.bytes[i], UINT8_C(0x5a));
  overlapping.length = 17u;
  T64_CHECK_INT(t64_video_copy(machine, NULL, 1u, &overlapping.length),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK_INT(overlapping.length, 17u);
  T64_CHECK(digest_of(machine, after));
  T64_CHECK(memcmp(before, after, sizeof before) == 0);
  t64_machine_destroy(machine);
}

static void case_output_drain_cadence_equal(void) {
  t64_machine *drain_each = create_output_machine();
  t64_machine *batch = create_output_machine();
  t64_video_packet_info info = {0};
  uint8_t packet[TEST_PACKET_BYTES];
  uint8_t each_digest[T64_DIGEST_SIZE];
  uint8_t batch_digest[T64_DIGEST_SIZE];
  size_t i;
  if (drain_each == NULL || batch == NULL) goto cleanup;
  for (i = 0u; i < 2u; i++) {
    if (!advance_one_field(drain_each) ||
        !copy_oldest(drain_each, &info, packet) ||
        !advance_one_field(batch)) {
      T64_CHECK(false);
      goto cleanup;
    }
    T64_CHECK_INT(info.sequence, i);
    T64_CHECK(memcmp(packet, expected_packet, sizeof packet) == 0);
  }
  T64_CHECK_INT(drain_each->tick, batch->tick);
  T64_CHECK_INT(drain_each->vi.field, batch->vi.field);
  T64_CHECK_INT(drain_each->vi.next_sequence, batch->vi.next_sequence);
  T64_CHECK_INT(drain_each->vi.packet_count, 0u);
  T64_CHECK_INT(batch->vi.packet_count, 2u);
  if (!digest_of(drain_each, each_digest) ||
      !digest_of(batch, batch_digest)) {
    T64_CHECK(false);
    goto cleanup;
  }
  T64_CHECK(memcmp(each_digest, batch_digest, sizeof each_digest) == 0);
  for (i = 0u; i < 2u; i++) {
    if (!copy_oldest(batch, &info, packet)) {
      T64_CHECK(false);
      goto cleanup;
    }
    T64_CHECK_INT(info.sequence, i);
    T64_CHECK(memcmp(packet, expected_packet, sizeof packet) == 0);
  }
  if (!digest_of(batch, batch_digest)) {
    T64_CHECK(false);
    goto cleanup;
  }
  T64_CHECK(memcmp(each_digest, batch_digest, sizeof each_digest) == 0);
cleanup:
  if (drain_each != NULL) t64_machine_destroy(drain_each);
  if (batch != NULL) t64_machine_destroy(batch);
}

static void case_output_whole_split(void) {
  t64_machine *whole = create_output_machine();
  t64_machine *split = create_output_machine();
  uint8_t whole_digest[T64_DIGEST_SIZE];
  uint8_t split_digest[T64_DIGEST_SIZE];
  uint8_t whole_after_copy[T64_DIGEST_SIZE];
  uint8_t split_after_copy[T64_DIGEST_SIZE];
  size_t i;
  if (whole == NULL || split == NULL) {
    T64_CHECK(false);
    goto cleanup;
  }
  /* The whole machine crosses both field deadlines in one public request. */
  if (!advance_guest_bound(whole, UINT64_C(166))) goto cleanup;
  /* The split machine stops at each deadline, then reaches the same tick. */
  if (!advance_guest_bound(split, UINT64_C(83)) ||
      !advance_guest_bound(split, UINT64_C(165)) ||
      !advance_guest_bound(split, UINT64_C(166))) {
    goto cleanup;
  }
  T64_CHECK_INT(whole->tick, UINT64_C(166));
  T64_CHECK_INT(split->tick, UINT64_C(166));
  T64_CHECK_INT(whole->tick, split->tick);
  T64_CHECK_INT(whole->vi.field, split->vi.field);
  T64_CHECK_INT(whole->vi.field, 2u);
  T64_CHECK_INT(whole->vi.next_sequence, split->vi.next_sequence);
  T64_CHECK_INT(whole->vi.next_sequence, 2u);
  T64_CHECK_INT(whole->vi.packet_count, 2u);
  T64_CHECK_INT(split->vi.packet_count, 2u);
  if (!digest_of(whole, whole_digest) || !digest_of(split, split_digest)) {
    T64_CHECK(false);
    goto cleanup;
  }
  T64_CHECK(memcmp(whole_digest, split_digest, sizeof whole_digest) == 0);
  for (i = 0u; i < 2u; i++) {
    t64_video_packet_info whole_info = {0};
    t64_video_packet_info split_info = {0};
    uint8_t whole_packet[TEST_PACKET_BYTES];
    uint8_t split_packet[TEST_PACKET_BYTES];
    if (!copy_oldest(whole, &whole_info, whole_packet) ||
        !copy_oldest(split, &split_info, split_packet)) {
      T64_CHECK(false);
      goto cleanup;
    }
    T64_CHECK_INT(whole_info.sequence, i);
    T64_CHECK_INT(split_info.sequence, i);
    T64_CHECK_INT(whole_info.field, i);
    T64_CHECK_INT(split_info.field, i);
    T64_CHECK_INT(whole_info.parity, 0u);
    T64_CHECK_INT(split_info.parity, whole_info.parity);
    T64_CHECK_INT(whole_info.start_tick, i == 0u ? 0u : 83u);
    T64_CHECK_INT(split_info.start_tick, whole_info.start_tick);
    T64_CHECK_INT(whole_info.end_tick, i == 0u ? 83u : 165u);
    T64_CHECK_INT(split_info.end_tick, whole_info.end_tick);
    T64_CHECK_INT(whole_info.width, 4u);
    T64_CHECK_INT(split_info.width, whole_info.width);
    T64_CHECK_INT(whole_info.height, 1u);
    T64_CHECK_INT(split_info.height, whole_info.height);
    T64_CHECK_INT(whole_info.stride_bytes, 16u);
    T64_CHECK_INT(split_info.stride_bytes, whole_info.stride_bytes);
    T64_CHECK_INT(whole_info.source_address, TEST_ORIGIN);
    T64_CHECK_INT(split_info.source_address, whole_info.source_address);
    T64_CHECK_INT(whole_info.source_format, T64_VIDEO_SOURCE_RGBA5551);
    T64_CHECK_INT(split_info.source_format, whole_info.source_format);
    T64_CHECK_INT(whole_info.output_encoding, T64_VIDEO_ENCODING_RGBA8);
    T64_CHECK_INT(split_info.output_encoding, whole_info.output_encoding);
    T64_CHECK_INT(whole_info.source_bytes, 8u);
    T64_CHECK_INT(split_info.source_bytes, whole_info.source_bytes);
    T64_CHECK_INT(whole_info.packet_bytes, TEST_PACKET_BYTES);
    T64_CHECK_INT(split_info.packet_bytes, whole_info.packet_bytes);
    T64_CHECK_INT(whole_info.filter, T64_VIDEO_FILTER_RESAMPLE_Q20);
    T64_CHECK_INT(split_info.filter, whole_info.filter);
    T64_CHECK_STR(whole_info.profile_id, TEST_PROFILE_ID);
    T64_CHECK_STR(split_info.profile_id, whole_info.profile_id);
    T64_CHECK_STR(split_info.build_id, whole_info.build_id);
    T64_CHECK_STR(split_info.vi_clock_policy, whole_info.vi_clock_policy);
    T64_CHECK_STR(split_info.vi_output_policy, whole_info.vi_output_policy);
    T64_CHECK(memcmp(whole_packet, expected_packet,
                     sizeof whole_packet) == 0);
    T64_CHECK(memcmp(split_packet, whole_packet,
                     sizeof whole_packet) == 0);
  }
  T64_CHECK_INT(whole->tick, UINT64_C(166));
  T64_CHECK_INT(split->tick, UINT64_C(166));
  T64_CHECK_INT(whole->vi.packet_count, 0u);
  T64_CHECK_INT(split->vi.packet_count, 0u);
  if (!digest_of(whole, whole_after_copy) ||
      !digest_of(split, split_after_copy)) {
    T64_CHECK(false);
    goto cleanup;
  }
  T64_CHECK(memcmp(whole_digest, whole_after_copy,
                   sizeof whole_digest) == 0);
  T64_CHECK(memcmp(split_digest, split_after_copy,
                   sizeof split_digest) == 0);
  T64_CHECK(memcmp(whole_after_copy, split_after_copy,
                   sizeof whole_after_copy) == 0);
cleanup:
  if (whole != NULL) t64_machine_destroy(whole);
  if (split != NULL) t64_machine_destroy(split);
}

static void case_output_interleaved_instances(void) {
  t64_machine *first = create_output_machine();
  t64_machine *second = create_output_machine();
  uint8_t first_digest[T64_DIGEST_SIZE];
  uint8_t second_digest[T64_DIGEST_SIZE];
  size_t i;
  if (first == NULL || second == NULL) goto cleanup;
  for (i = 0u; i < 2u; i++) {
    if (!advance_one_field(first) || !advance_one_field(second)) {
      T64_CHECK(false);
      goto cleanup;
    }
    T64_CHECK_INT(first->tick, second->tick);
    T64_CHECK_INT(first->vi.field, second->vi.field);
    if (!digest_of(first, first_digest) || !digest_of(second, second_digest)) {
      T64_CHECK(false);
      goto cleanup;
    }
    T64_CHECK(memcmp(first_digest, second_digest, sizeof first_digest) == 0);
  }
cleanup:
  if (first != NULL) t64_machine_destroy(first);
  if (second != NULL) t64_machine_destroy(second);
}

typedef struct {
  const char *name;
  void (*run)(void);
} test_case;

static const test_case cases[] = {
    {"output-full-pending-boundary", case_output_full_pending_boundary},
    {"output-resume-exactly-once", case_output_resume_exactly_once},
    {"output-no-overwrite-loss", case_output_no_overwrite_loss},
    {"output-required-size-no-partial-copy",
     case_output_required_size_no_partial_copy},
    {"output-invalid-overlap-span", case_output_invalid_overlap_span},
    {"output-drain-cadence-equal", case_output_drain_cadence_equal},
    {"output-whole-split", case_output_whole_split},
    {"output-interleaved-instances", case_output_interleaved_instances},
};

int main(int argc, char **argv) {
  size_t i;
  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) puts(cases[i].name);
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
      if (strcmp(argv[2], cases[i].name) == 0) {
        cases[i].run();
        return T64_TEST_FINISH(cases[i].name);
      }
    }
    return 2;
  }
  if (argc == 3 && strcmp(argv[1], "--suite") == 0 &&
      strcmp(argv[2], "video-output") == 0) {
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) cases[i].run();
    return T64_TEST_FINISH("video-output");
  }
  return 2;
}
