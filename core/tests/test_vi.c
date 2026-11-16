#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_test.h"
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

typedef struct {
  const char *name;
  void (*run)(void);
} test_case;

static const test_case cases[] = {
    {"cpu-authored-fill-packet-and-dp-lifecycle",
     case_cpu_authored_fill_packet_and_dp_lifecycle},
    {"copy-refusal-is-atomic", case_copy_refusal_is_atomic},
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
