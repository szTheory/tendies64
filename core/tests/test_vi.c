#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_test.h"
#include "../t64_internal.h"
#include "tendies64/t64_machine.h"

#define TEST_ROM_CAPACITY 65536u
#define TEST_SOURCE_BYTES 24u
#define TEST_PROFILE_ID "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v1"
#define TEST_COMPLETION_TICK UINT64_C(600000)

static const uint8_t expected_packet[] = {
    255, 0, 0, 255, 0, 255, 0, 255, 255, 0, 0, 255, 0, 255, 0, 255,
    255, 0, 0, 255, 0, 255, 0, 255, 255, 0, 0, 255, 0, 255, 0, 255,
    255, 0, 0, 255, 0, 255, 0, 255, 255, 0, 0, 255, 0, 255, 0, 255,
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

static const t64_host_allocator test_allocator = {test_allocate, test_release,
                                                   NULL};

static bool load_authored_rom(uint8_t *bytes, size_t capacity,
                              size_t *length_out) {
  const char *path = getenv("T64_TEST_RDP_FILL_PATH");
  FILE *file;
  long length;
  if (path == NULL || path[0] == '\0' || bytes == NULL || length_out == NULL)
    return false;
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

static t64_machine *create_authored_machine(void) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t rom[TEST_ROM_CAPACITY];
  size_t rom_length = 0u;
  t64_status status;

  T64_CHECK(load_authored_rom(rom, sizeof rom, &rom_length));
  if (rom_length == 0u) return NULL;
  T64_CHECK_INT(t64_profile_lookup(TEST_PROFILE_ID, &profile), T64_STATUS_OK);
  if (profile == NULL) return NULL;
  T64_CHECK_INT(t64_machine_create(&test_allocator, profile, &machine),
                T64_STATUS_OK);
  if (machine == NULL) return NULL;
  status = t64_machine_admit_rom(machine, rom, rom_length);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return NULL;
  }
  return machine;
}

static const t64_event *find_pending_vi_event(const t64_machine *machine) {
  size_t offset;

  for (offset = 0u; offset < machine->queue_count; offset++) {
    size_t slot = (machine->queue_head + offset) % T64_EVENT_QUEUE_CAPACITY;
    const t64_event *event = &machine->events[slot];

    if (event->device == T64_VI_DEVICE &&
        event->kind == T64_VI_EVENT_FIELD &&
        event->token == machine->vi.event_generation) {
      return event;
    }
  }
  return NULL;
}

static bool advance_guest_to_tick(t64_machine *machine, uint64_t tick,
                                  t64_stop_result *stop_out) {
  size_t attempt;

  for (attempt = 0u; attempt < 512u; attempt++) {
    t64_status status = t64_machine_advance(
        machine, &(t64_stop_request){T64_STOP_GUEST_TICK, tick}, stop_out);

    T64_CHECK_INT(status, T64_STATUS_OK);
    if (status != T64_STATUS_OK) return false;
    if (stop_out->reason != T64_STOP_REASON_RESOURCE_LIMIT) {
      return stop_out->reason == T64_STOP_REASON_BOUND;
    }
    if (machine->tick == tick && machine->vi.packet_count >=
                                     T64_VIDEO_QUEUE_CAPACITY) {
      return false;
    }
  }
  T64_CHECK(false);
  return false;
}

static bool advance_until_video_full(t64_machine *machine, uint64_t tick,
                                     t64_stop_result *stop_out) {
  size_t attempt;

  for (attempt = 0u; attempt < 512u; attempt++) {
    t64_status status = t64_machine_advance(
        machine, &(t64_stop_request){T64_STOP_GUEST_TICK, tick}, stop_out);

    T64_CHECK_INT(status, T64_STATUS_OK);
    if (status != T64_STATUS_OK) return false;
    if (machine->vi.packet_count >= T64_VIDEO_QUEUE_CAPACITY) {
      return stop_out->reason == T64_STOP_REASON_RESOURCE_LIMIT;
    }
    if (stop_out->reason != T64_STOP_REASON_RESOURCE_LIMIT) return false;
  }
  T64_CHECK(false);
  return false;
}

static bool advance_to_next_vi_field(t64_machine *machine) {
  const t64_event *event = find_pending_vi_event(machine);
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  uint64_t tick;
  uint32_t field;
  size_t attempt;

  if (event == NULL) return false;
  tick = event->tick;
  field = machine->vi.field;
  for (attempt = 0u; attempt < 512u; attempt++) {
    t64_status status = t64_machine_advance(
        machine, &(t64_stop_request){T64_STOP_GUEST_TICK, tick}, &stop);

    T64_CHECK_INT(status, T64_STATUS_OK);
    if (status != T64_STATUS_OK) return false;
    if (machine->vi.field != field) break;
    if (stop.reason != T64_STOP_REASON_RESOURCE_LIMIT) return false;
  }
  if (attempt == 512u) T64_CHECK(false);
  T64_CHECK_INT(machine->vi.field, field + 1u);
  return machine->vi.field == field + 1u;
}

static bool copy_packet(t64_machine *machine, t64_video_packet_info *info_out,
                        uint8_t *bytes_out, size_t capacity,
                        size_t *length_out) {
  t64_status status;

  status = t64_video_info(machine, info_out);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) return false;
  status = t64_video_copy(machine, bytes_out, capacity, length_out);
  T64_CHECK_INT(status, T64_STATUS_OK);
  return status == T64_STATUS_OK;
}

static void check_video_copy_refusals(t64_machine *machine,
                                      const t64_video_packet_info *expected) {
  union {
    size_t length;
    uint8_t bytes[sizeof expected_packet];
  } overlapping;
  uint8_t destination[sizeof expected_packet];
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  t64_video_packet_info after = {0};
  size_t length;
  size_t i;

  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);

  length = (size_t)0x1234u;
  T64_CHECK_INT(t64_video_copy(machine, NULL, 1u, &length),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK_INT(length, (size_t)0x1234u);

  (void)memset(destination, 0xa5, sizeof destination);
  length = (size_t)0x5678u;
  T64_CHECK_INT(t64_video_copy(machine, destination,
                               sizeof destination - 1u, &length),
                T64_STATUS_BUFFER_TOO_SMALL);
  T64_CHECK_INT(length, sizeof expected_packet);
  for (i = 0u; i < sizeof destination; i++)
    T64_CHECK_INT(destination[i], UINT8_C(0xa5));

  (void)memset(overlapping.bytes, 0x5a, sizeof overlapping.bytes);
  T64_CHECK_INT(t64_video_copy(machine, overlapping.bytes,
                               sizeof overlapping.bytes,
                               &overlapping.length),
                T64_STATUS_INVALID_ARGUMENT);
  for (i = 0u; i < sizeof overlapping.bytes; i++)
    T64_CHECK_INT(overlapping.bytes[i], UINT8_C(0x5a));

  T64_CHECK_INT(t64_video_info(machine, &after), T64_STATUS_OK);
  T64_CHECK_INT(after.sequence, expected->sequence);
  T64_CHECK_INT(after.packet_bytes, expected->packet_bytes);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);
}

static void case_cpu_authored_fill_packet_and_dp_lifecycle(void) {
  const t64_host_allocator allocator = {test_allocate, test_release, NULL};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t rom[TEST_ROM_CAPACITY];
  uint8_t packet[sizeof expected_packet];
  uint8_t discarded[64];
  size_t rom_length = 0u;
  size_t packet_length = 0u;
  t64_status status;
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {0u, 0u, 0u};
  t64_video_packet_info first_info = {0};
  uint32_t word = 0u;
  size_t i;
  bool packet_captured = false;

  T64_CHECK(load_authored_rom(rom, sizeof rom, &rom_length));
  if (rom_length == 0u) return;
  T64_CHECK_INT(t64_profile_lookup(TEST_PROFILE_ID, &profile), T64_STATUS_OK);
  if (profile == NULL) return;
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  if (machine == NULL) return;
  T64_CHECK_INT(t64_machine_admit_rom(machine, rom, rom_length),
                T64_STATUS_OK);

  status = t64_machine_advance(
      machine, &(t64_stop_request){T64_STOP_GUEST_TICK, TEST_COMPLETION_TICK},
      &stop);
  for (i = 0u; status == T64_STATUS_OK &&
               stop.reason == T64_STOP_REASON_RESOURCE_LIMIT && i < 512u;
       i++) {
    t64_video_packet_info queued;
    while (t64_video_info(machine, &queued) == T64_STATUS_OK) {
      size_t required = 0u;
      status = t64_video_copy(machine, NULL, 0u, &required);
      if (status != T64_STATUS_BUFFER_TOO_SMALL ||
          required > sizeof discarded) {
        status = T64_STATUS_RESOURCE_EXHAUSTED;
        break;
      }
      if (!packet_captured) check_video_copy_refusals(machine, &queued);
      status = t64_video_copy(machine, discarded, sizeof discarded,
                              &required);
      if (status != T64_STATUS_OK) break;
      if (!packet_captured) {
        first_info = queued;
        packet_length = required;
        (void)memcpy(packet, discarded, required);
        packet_captured = true;
      }
    }
    if (status != T64_STATUS_OK) break;
    status = t64_machine_advance(
        machine,
        &(t64_stop_request){T64_STOP_GUEST_TICK, TEST_COMPLETION_TICK},
        &stop);
  }
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(stop.reason, T64_STOP_REASON_GUEST_COMPLETION);
  if (status == T64_STATUS_OK &&
      stop.reason == T64_STOP_REASON_GUEST_COMPLETION) {
    T64_CHECK_INT(t64_machine_cpu_stop_detail(machine, &detail),
                  T64_STATUS_OK);
    T64_CHECK_INT(detail.pc, UINT32_C(0x80000558));
    T64_CHECK_INT(detail.opcode, UINT32_C(0x0000000d));
  }

  if (!packet_captured) {
    size_t required = 0u;
    T64_CHECK_INT(t64_video_info(machine, &first_info), T64_STATUS_OK);
    T64_CHECK_INT(t64_video_copy(machine, NULL, 0u, &required),
                  T64_STATUS_BUFFER_TOO_SMALL);
    T64_CHECK_INT(required, sizeof expected_packet);
    check_video_copy_refusals(machine, &first_info);
    T64_CHECK_INT(t64_video_copy(machine, packet, sizeof packet,
                                 &packet_length), T64_STATUS_OK);
    packet_captured = packet_length == sizeof packet;
  }
  T64_CHECK(packet_captured);
  T64_CHECK_INT(first_info.width, 4u);
  T64_CHECK_INT(first_info.height, 3u);
  T64_CHECK_INT(first_info.stride_bytes, 16u);
  T64_CHECK_INT(first_info.source_address, UINT32_C(0x3000));
  T64_CHECK_INT(first_info.source_format, T64_VIDEO_SOURCE_RGBA5551);
  T64_CHECK_INT(first_info.output_encoding, T64_VIDEO_ENCODING_RGBA8);
  T64_CHECK_INT(first_info.source_bytes, TEST_SOURCE_BYTES);
  T64_CHECK_INT(first_info.packet_bytes, sizeof expected_packet);
  T64_CHECK_INT(first_info.commands_completed, 6u);
  T64_CHECK_INT(first_info.fullsync_completed, 1u);
  T64_CHECK_INT(first_info.last_command_word,
                UINT64_C(0x2900000000000000));
  T64_CHECK_INT(first_info.start_tick, UINT64_C(538862));
  T64_CHECK_INT(first_info.end_tick, UINT64_C(538882));
  T64_CHECK_INT(first_info.end_tick - first_info.start_tick, 20u);
  T64_CHECK_INT(packet_length, sizeof expected_packet);
  T64_CHECK(memcmp(packet, expected_packet, sizeof packet) == 0);

  for (i = 0u; i < TEST_SOURCE_BYTES; i += 4u) {
    uint32_t actual = 0u;
    T64_CHECK_INT(t64_machine_bus_read(machine,
                                       UINT32_C(0x3000) + (uint32_t)i, 4u,
                                       &actual),
                  T64_STATUS_OK);
    T64_CHECK_INT(actual, UINT32_C(0xf80107c1));
  }
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04100008), 4u,
                                     &word),
                T64_STATUS_OK);
  T64_CHECK_INT(word, UINT32_C(0x3130));
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04100004), 4u,
                                     &word),
                T64_STATUS_OK);
  T64_CHECK_INT(word, UINT32_C(0x3130));
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04300008), 4u,
                                     &word),
                T64_STATUS_OK);
  T64_CHECK_INT(word, 0u);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x0430000c), 4u,
                                     &word),
                T64_STATUS_OK);
  T64_CHECK_INT(word, UINT32_C(0x20));
  t64_machine_destroy(machine);
}

static void case_copy_refusal_is_atomic(void) {
  const t64_host_allocator allocator = {test_allocate, test_release, NULL};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t rom[TEST_ROM_CAPACITY];
  size_t rom_length = 0u;
  t64_status status;
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  t64_video_packet_info queued = {0};
  size_t i;

  T64_CHECK(load_authored_rom(rom, sizeof rom, &rom_length));
  if (rom_length == 0u) return;
  T64_CHECK_INT(t64_profile_lookup(TEST_PROFILE_ID, &profile), T64_STATUS_OK);
  if (profile == NULL) return;
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  if (machine == NULL) return;
  T64_CHECK_INT(t64_machine_admit_rom(machine, rom, rom_length),
                T64_STATUS_OK);
  status = t64_machine_advance(
      machine, &(t64_stop_request){T64_STOP_GUEST_TICK, TEST_COMPLETION_TICK},
      &stop);
  for (i = 0u; status == T64_STATUS_OK && i < 512u &&
               t64_video_info(machine, &queued) == T64_STATUS_NOT_READY;
       i++) {
    status = t64_machine_advance(
        machine,
        &(t64_stop_request){T64_STOP_GUEST_TICK, TEST_COMPLETION_TICK},
        &stop);
  }
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK_INT(t64_video_info(machine, &queued), T64_STATUS_OK);
  if (t64_video_info(machine, &queued) != T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK_INT(queued.packet_bytes, sizeof expected_packet);
  check_video_copy_refusals(machine, &queued);
  t64_machine_destroy(machine);
}

static void case_v15_digest_covers_vi_and_inherited_state(void) {
  t64_machine *machine = create_authored_machine();
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after_vi[T64_DIGEST_SIZE];
  uint8_t after_warm[T64_DIGEST_SIZE];
  t64_status status;

  if (machine == NULL) return;
  T64_CHECK(t64_has_rdp_vi_profile(machine));
  T64_CHECK(t64_has_t1_policy(machine));
  T64_CHECK_INT(machine->profile.machine_model_version, 10u);
  T64_CHECK_INT(machine->profile.state_serialization_version, 15u);
  T64_CHECK_STR(machine->profile.rdp_work_policy,
                "dpc-fetch-effect-fill-row-event-1tick-v2");
  T64_CHECK_INT(t64_machine_digest(machine, before), T64_STATUS_OK);
  status = t64_machine_bus_write(machine, UINT32_C(0x04400004), 4u,
                                 UINT32_C(0x3000));
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(machine, after_vi), T64_STATUS_OK);
  T64_CHECK(memcmp(before, after_vi, sizeof before) != 0);

  machine->boot.seed_reset_version++;
  T64_CHECK_INT(t64_machine_digest(machine, after_warm), T64_STATUS_OK);
  T64_CHECK(memcmp(after_vi, after_warm, sizeof after_vi) != 0);
  t64_machine_destroy(machine);
}

static void case_drain_cadence_preserves_digest_and_packet_history(void) {
  t64_machine *early = create_authored_machine();
  t64_machine *delayed = create_authored_machine();
  t64_stop_result early_stop = {UINT32_MAX, UINT64_MAX};
  t64_stop_result delayed_stop = {UINT32_MAX, UINT64_MAX};
  t64_video_packet_info early_info[2] = {{0}};
  t64_video_packet_info delayed_info[2] = {{0}};
  uint8_t early_packets[2][sizeof expected_packet] = {{0}};
  uint8_t delayed_packets[2][sizeof expected_packet] = {{0}};
  uint8_t before_copy[T64_DIGEST_SIZE];
  uint8_t after_copy[T64_DIGEST_SIZE];
  uint8_t early_digest[T64_DIGEST_SIZE];
  uint8_t delayed_digest[T64_DIGEST_SIZE];
  size_t early_lengths[2] = {0u, 0u};
  size_t delayed_lengths[2] = {0u, 0u};
  size_t i;

  if (early == NULL || delayed == NULL) {
    t64_machine_destroy(early);
    t64_machine_destroy(delayed);
    return;
  }
  T64_CHECK(advance_guest_to_tick(early, UINT64_C(538881), &early_stop));
  T64_CHECK(
      advance_guest_to_tick(delayed, UINT64_C(538881), &delayed_stop));
  T64_CHECK(early->vi.enabled);
  T64_CHECK(delayed->vi.enabled);

  T64_CHECK(advance_to_next_vi_field(early));
  T64_CHECK_INT(t64_video_info(early, &early_info[0]), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(early, before_copy), T64_STATUS_OK);
  T64_CHECK_INT(t64_video_copy(early, early_packets[0],
                               sizeof early_packets[0], &early_lengths[0]),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(early, after_copy), T64_STATUS_OK);
  T64_CHECK(memcmp(before_copy, after_copy, sizeof before_copy) == 0);

  T64_CHECK(advance_to_next_vi_field(delayed));
  T64_CHECK(advance_to_next_vi_field(early));
  T64_CHECK(copy_packet(early, &early_info[1], early_packets[1],
                        sizeof early_packets[1], &early_lengths[1]));
  T64_CHECK(advance_to_next_vi_field(delayed));

  T64_CHECK_INT(early->tick, delayed->tick);
  T64_CHECK_INT(early->vi.field, delayed->vi.field);
  T64_CHECK_INT(early->vi.next_sequence, delayed->vi.next_sequence);
  T64_CHECK_INT(early->vi.packet_count, 0u);
  T64_CHECK_INT(delayed->vi.packet_count, 2u);
  T64_CHECK_INT(t64_machine_digest(early, early_digest), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(delayed, delayed_digest), T64_STATUS_OK);
  T64_CHECK(memcmp(early_digest, delayed_digest, sizeof early_digest) == 0);

  for (i = 0u; i < 2u; i++) {
    T64_CHECK(copy_packet(delayed, &delayed_info[i], delayed_packets[i],
                          sizeof delayed_packets[i], &delayed_lengths[i]));
    T64_CHECK_INT(delayed_info[i].sequence, early_info[i].sequence);
    T64_CHECK_INT(delayed_lengths[i], early_lengths[i]);
    T64_CHECK(memcmp(delayed_packets[i], early_packets[i],
                     early_lengths[i]) == 0);
  }
  T64_CHECK_INT(t64_machine_digest(delayed, after_copy), T64_STATUS_OK);
  T64_CHECK(memcmp(delayed_digest, after_copy, sizeof delayed_digest) == 0);
  t64_machine_destroy(early);
  t64_machine_destroy(delayed);
}

static void case_full_queue_copy_preserves_event_order(void) {
  t64_machine *machine = create_authored_machine();
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  const t64_event *pending_event;
  const t64_event *resumed_event;
  uint64_t pending_tick;
  uint64_t pending_sequence = UINT64_MAX;
  uint64_t machine_sequence_before_copy;
  uint64_t tick_before_copy;
  uint64_t field_before_resume;
  uint64_t frames_before_resume;
  size_t scheduler_count_before_copy;
  size_t length = 0u;
  uint8_t packet[sizeof expected_packet];
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  size_t vi_trace_index = SIZE_MAX;
  size_t synthetic_trace_index = SIZE_MAX;
  size_t i;
  t64_status status;

  if (machine == NULL) return;
  T64_CHECK(advance_until_video_full(machine, TEST_COMPLETION_TICK, &stop));
  status = T64_STATUS_OK;
  T64_CHECK_INT(stop.reason, T64_STOP_REASON_RESOURCE_LIMIT);
  T64_CHECK_INT(machine->vi.packet_count, T64_VIDEO_QUEUE_CAPACITY);

  pending_event = find_pending_vi_event(machine);
  T64_CHECK(pending_event != NULL);
  pending_tick = pending_event == NULL ? stop.tick : pending_event->tick;
  if (pending_event != NULL) pending_sequence = pending_event->sequence;
  T64_CHECK(machine->tick <= pending_tick);
  T64_CHECK_INT(t64_test_schedule_synthetic(
                    machine, pending_tick, T64_TEST_EVENT_FRAME, 1u),
                T64_STATUS_OK);
  machine_sequence_before_copy = machine->next_sequence;
  scheduler_count_before_copy = machine->queue_count;
  tick_before_copy = machine->tick;
  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);

  machine->trace_enabled = true;
  t64_trace_reset(machine);
  T64_CHECK_INT(t64_video_copy(machine, packet, sizeof packet, &length),
                T64_STATUS_OK);
  T64_CHECK_INT(length, sizeof expected_packet);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);
  T64_CHECK_INT(machine->next_sequence, machine_sequence_before_copy);
  T64_CHECK_INT(machine->queue_count, scheduler_count_before_copy);
  T64_CHECK_INT(machine->tick, tick_before_copy);

  resumed_event = find_pending_vi_event(machine);
  T64_CHECK(resumed_event != NULL);
  if (pending_sequence != UINT64_MAX && resumed_event != NULL) {
    T64_CHECK_INT(resumed_event->tick, pending_tick);
    T64_CHECK_INT(resumed_event->sequence, pending_sequence);
  }

  field_before_resume = machine->vi.field;
  frames_before_resume = machine->frame_event_count;
  status = t64_machine_advance(
      machine, &(t64_stop_request){T64_STOP_GUEST_TICK, pending_tick}, &stop);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(machine->vi.field, field_before_resume + 1u);
  T64_CHECK_INT(machine->frame_event_count, frames_before_resume + 1u);
  for (i = 0u; i < machine->trace_count; i++) {
    size_t slot = (machine->trace_head + i) % T64_TRACE_RECORD_CAPACITY;
    const t64_trace_record *record = &machine->trace_records[slot];

    if (record->tick != pending_tick) continue;
    if (record->device == T64_VI_DEVICE &&
        record->kind == T64_VI_EVENT_FIELD && record->value == 0u &&
        vi_trace_index == SIZE_MAX) {
      vi_trace_index = i;
    }
    if (record->device == T64_DEVICE_SYNTHETIC_TEST &&
        record->kind == T64_TEST_EVENT_FRAME &&
        synthetic_trace_index == SIZE_MAX) {
      synthetic_trace_index = i;
    }
  }
  T64_CHECK(vi_trace_index != SIZE_MAX);
  T64_CHECK(synthetic_trace_index != SIZE_MAX);
  T64_CHECK(vi_trace_index < synthetic_trace_index);
  t64_machine_destroy(machine);
}

typedef struct {
  const char *name;
  void (*run)(void);
} test_case;

static const test_case cases[] = {
    {"cpu-authored-fill-packet-and-dp-lifecycle",
     case_cpu_authored_fill_packet_and_dp_lifecycle},
    {"copy-refusal-is-atomic", case_copy_refusal_is_atomic},
    {"v15-digest-covers-vi-and-inherited-state",
     case_v15_digest_covers_vi_and_inherited_state},
    {"drain-cadence-preserves-digest-and-packet-history",
     case_drain_cadence_preserves_digest_and_packet_history},
    {"full-queue-copy-preserves-event-order",
     case_full_queue_copy_preserves_event_order},
};

int main(int argc, char **argv) {
  size_t i;
  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) puts(cases[i].name);
    return 0;
  }
  if (argc != 1) return 2;
  for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) cases[i].run();
  return T64_TEST_FINISH("unit.core.vi.fill");
}
