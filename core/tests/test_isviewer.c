#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define T64_IPL3_ASSET_BYTES 4096u
#define T64_MAX_FIXTURE_BYTES (1024u * 1024u)

static int tap_count;
static int tap_failures;

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  void *memory;

  (void)context;
  if (storage_out == NULL || size == 0u || alignment == 0u ||
      alignment > _Alignof(max_align_t)) {
    return false;
  }
  memory = malloc(size);
  if (memory == NULL || (uintptr_t)memory % alignment != 0u) {
    free(memory);
    return false;
  }
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = _Alignof(max_align_t);
  return true;
}

static void test_release(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static void check(bool passed, const char *name) {
  tap_count++;
  if (!passed) {
    tap_failures++;
  }
  (void)printf("%s %d - %s\n", passed ? "ok" : "not ok", tap_count, name);
}

static void check_status(t64_status actual, t64_status expected,
                         const char *name) {
  bool passed = actual == expected;

  check(passed, name);
  if (!passed) {
    (void)printf("# observed status=%u expected status=%u\n",
                 (unsigned)actual, (unsigned)expected);
  }
}

static bool read_file(const char *path, uint8_t *bytes, size_t capacity,
                      size_t *length_out) {
  FILE *file;
  long file_length;
  size_t length;
  int read_error;
  int close_error;

  if (path == NULL || path[0] == '\0' || bytes == NULL || length_out == NULL) {
    return false;
  }
  file = fopen(path, "rb");
  if (file == NULL) {
    return false;
  }
  if (fseek(file, 0L, SEEK_END) != 0 || (file_length = ftell(file)) < 0L ||
      (unsigned long)file_length > capacity || fseek(file, 0L, SEEK_SET) != 0) {
    (void)fclose(file);
    return false;
  }
  length = (size_t)file_length;
  if (fread(bytes, 1u, length, file) != length) {
    (void)fclose(file);
    return false;
  }
  read_error = ferror(file);
  close_error = fclose(file);
  if (read_error != 0 || close_error != 0) {
    return false;
  }
  *length_out = length;
  return true;
}

static bool read_named_fixture(const char *name, uint8_t *bytes,
                               size_t capacity, size_t *length_out) {
  const char *directory = getenv("T64_BOOT_FIXTURES_DIR");
  char path[512];
  int written;

  if (directory == NULL || directory[0] == '\0' || name == NULL) {
    return false;
  }
  written = snprintf(path, sizeof path, "%s/%s", directory, name);
  if (written < 0 || (size_t)written >= sizeof path) {
    return false;
  }
  return read_file(path, bytes, capacity, length_out);
}

static t64_machine *create_machine(const t64_host_allocator *allocator,
                                   const uint8_t *image, size_t image_length) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;

  if (t64_profile_lookup("ntsc-cart-4m-openipl3-warm-v1", &profile) !=
          T64_STATUS_OK ||
      profile == NULL ||
      t64_machine_create(allocator, profile, &machine) != T64_STATUS_OK ||
      machine == NULL ||
      t64_machine_admit_rom(machine, image, image_length) != T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return NULL;
  }
  return machine;
}

static bool bytes_are(const uint8_t *bytes, size_t length, uint8_t value) {
  size_t i;

  for (i = 0u; i < length; i++) {
    if (bytes[i] != value) {
      return false;
    }
  }
  return true;
}

static t64_status advance_guest_in_chunks(t64_machine *machine,
                                          uint64_t limit_tick,
                                          t64_stop_result *result_out) {
  t64_stop_request request = {T64_STOP_GUEST_TICK, 0u};

  if (machine == NULL || result_out == NULL || limit_tick < machine->tick) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  while (machine->tick < limit_tick && !machine->cpu.stopped) {
    uint64_t previous_tick = machine->tick;
    uint64_t remaining = limit_tick - machine->tick;
    t64_status advance_status;

    request.value = remaining > UINT64_C(3000)
                        ? machine->tick + UINT64_C(3000)
                        : limit_tick;
    advance_status = t64_machine_advance(machine, &request, result_out);
    if (advance_status != T64_STATUS_OK) {
      return advance_status;
    }
    if (result_out->reason == T64_STOP_REASON_CPU_UNSUPPORTED ||
        result_out->reason == T64_STOP_REASON_GUEST_COMPLETION ||
        result_out->reason == T64_STOP_REASON_HALT ||
        result_out->reason == T64_STOP_REASON_QUIESCENT) {
      return T64_STATUS_OK;
    }
    if (result_out->reason == T64_STOP_REASON_RESOURCE_LIMIT) {
      if (machine->tick <= previous_tick ||
          result_out->tick != machine->tick ||
          machine->tick > request.value) {
        return T64_STATUS_RESOURCE_EXHAUSTED;
      }
      continue;
    }
    if (result_out->reason != T64_STOP_REASON_BOUND ||
        result_out->tick != request.value || machine->tick != request.value) {
      return T64_STATUS_RESOURCE_EXHAUSTED;
    }
  }
  return T64_STATUS_OK;
}

static bool output_state_is_reset(const t64_output_state *output) {
  return output->policy_version == T64_ISVIEWER_POLICY_VERSION &&
         !output->video_output_ready && !output->audio_output_ready &&
         output->magic_register == 0u && output->byte_count == 0u &&
         bytes_are(output->buffer, sizeof output->buffer, 0u) &&
         output->captured_bytes == 0u &&
         bytes_are(output->captured, sizeof output->captured, 0u) &&
         !output->lost_output;
}

static void test_magic_register(const t64_host_allocator *allocator,
                                const uint8_t *asset, size_t asset_length) {
  static const char test_name[] =
      "libdragon magic register stores and reads back guest values";
  t64_machine *machine = create_machine(allocator, asset, asset_length);
  uint32_t value = UINT32_C(0xdecafbad);

  if (machine == NULL) {
    check(false, "magic-register test machine is admitted");
    return;
  }
  check(machine->output.magic_register == 0u,
        "magic register resets to zero before guest presence check");
  check_status(t64_machine_bus_write(machine, T64_ISVIEWER_BASE, 4u,
                                     UINT32_C(0xedcba987)),
               T64_STATUS_OK, "libdragon magic value write succeeds");
  check_status(t64_machine_bus_read(machine, T64_ISVIEWER_BASE, 4u, &value),
               T64_STATUS_OK, test_name);
  check(value == UINT32_C(0xedcba987),
        "magic register readback returns the guest-written complement");
  check_status(t64_machine_bus_write(machine, T64_ISVIEWER_BASE, 4u,
                                     UINT32_C(0x12345678)),
               T64_STATUS_OK, "second magic value write succeeds");
  check_status(t64_machine_bus_read(machine, T64_ISVIEWER_BASE, 4u, &value),
               T64_STATUS_OK, "second magic value reads back");
  check(value == UINT32_C(0x12345678),
        "magic register is writable storage rather than a fixed constant");
  t64_machine_destroy(machine);
}

static void test_buffer_endianness_and_refusals(
    const t64_host_allocator *allocator, const uint8_t *asset,
    size_t asset_length) {
  t64_machine *machine = create_machine(allocator, asset, asset_length);
  t64_output_state before;
  uint32_t value = UINT32_C(0xdecafbad);
  t64_status status;
  size_t i;

  if (machine == NULL) {
    check(false, "buffer test machine is admitted");
    return;
  }
  check_status(t64_machine_bus_write(machine, T64_ISVIEWER_BASE + 0x20u, 1u,
                                     UINT32_C(0xa1)),
               T64_STATUS_OK, "buffer accepts aligned byte writes");
  check_status(t64_machine_bus_write(machine, T64_ISVIEWER_BASE + 0x21u, 1u,
                                     UINT32_C(0xb2)),
               T64_STATUS_OK, "buffer byte writes advance one byte");
  check_status(t64_machine_bus_write(machine, T64_ISVIEWER_BASE + 0x22u, 2u,
                                     UINT32_C(0xc3d4)),
               T64_STATUS_OK, "buffer accepts aligned halfword writes");
  check_status(t64_machine_bus_write(machine, T64_ISVIEWER_BASE + 0x24u, 4u,
                                     UINT32_C(0xe5f60718)),
               T64_STATUS_OK, "buffer accepts aligned word writes");
  check_status(t64_machine_bus_read(machine, T64_ISVIEWER_BASE + 0x20u, 4u,
                                    &value),
               T64_STATUS_OK, "buffer word read succeeds");
  check(value == UINT32_C(0xa1b2c3d4),
        "buffer reads preserve big-endian byte and halfword order");
  check_status(t64_machine_bus_read(machine, T64_ISVIEWER_BASE + 0x24u, 4u,
                                    &value),
               T64_STATUS_OK, "buffer second word read succeeds");
  check(value == UINT32_C(0xe5f60718),
        "buffer word writes store the exact big-endian bytes");

  check_status(t64_machine_bus_write(machine, T64_ISVIEWER_BASE + 0x21u, 2u,
                                     UINT32_C(0x1234)),
               T64_STATUS_BUS_ALIGNMENT,
               "misaligned halfword buffer write is refused");
  check_status(t64_machine_bus_write(machine, T64_ISVIEWER_BASE + 0x21u, 4u,
                                     UINT32_C(0x12345678)),
               T64_STATUS_BUS_ALIGNMENT,
               "misaligned word buffer write is refused");
  check_status(t64_machine_bus_write(machine, T64_ISVIEWER_BASE + 0x21cu, 4u,
                                     UINT32_C(0x89abcdef)),
               T64_STATUS_OK, "last aligned buffer word fits exactly");
  check_status(t64_machine_bus_read(machine, T64_ISVIEWER_BASE + 0x21cu, 4u,
                                    &value),
               T64_STATUS_OK, "last aligned buffer word reads back");
  check(value == UINT32_C(0x89abcdef),
        "buffer includes the complete final word at offset 0x21c");

  (void)memcpy(&before, &machine->output, sizeof before);
  status = t64_machine_bus_write(machine, T64_ISVIEWER_BASE, 2u, 0x1234u);
  check_status(status, T64_STATUS_BUS_UNSUPPORTED,
               "magic register rejects a halfword store");
  status = t64_machine_bus_write(machine, T64_ISVIEWER_BASE + 0x14u, 1u,
                                 0x12u);
  check_status(status, T64_STATUS_BUS_UNSUPPORTED,
               "count register refuses byte stores");
  status = t64_machine_bus_write(machine, T64_ISVIEWER_BASE + 0x15u, 2u,
                                 0x1234u);
  check_status(status, T64_STATUS_BUS_ALIGNMENT,
               "count register refuses an unaligned halfword lane");
  status = t64_machine_bus_read(machine, T64_ISVIEWER_BASE + 4u, 4u, &value);
  check_status(status, T64_STATUS_BUS_UNSUPPORTED,
               "unused register offsets are refused");
  check(value == UINT32_C(0x89abcdef),
        "refused register read leaves the caller output untouched");
  status = t64_machine_bus_write(machine, T64_ISVIEWER_BASE + 0x220u, 1u,
                                 0x77u);
  check_status(status, T64_STATUS_BUS_DEVICE_REFUSED,
               "write outside the defined device window is refused");
  check(memcmp(&before, &machine->output, sizeof before) == 0,
        "wrong width, alignment, reserved range and outside writes are transactional");

  for (i = 0u; i < T64_ISVIEWER_BUFFER_BYTES; i++) {
    status = t64_machine_bus_write(
        machine, T64_ISVIEWER_BASE + T64_ISVIEWER_BUFFER_OFFSET +
                    (uint32_t)i,
        1u, (uint32_t)(i & 0xffu));
    if (status != T64_STATUS_OK) {
      break;
    }
  }
  check(i == T64_ISVIEWER_BUFFER_BYTES,
        "all 512 buffer byte lanes accept independent values");
  check(machine->output.buffer[0] == 0u &&
            machine->output.buffer[255] == UINT8_C(0xff) &&
            machine->output.buffer[511] == UINT8_C(0xff),
        "buffer byte writes retain exact first, middle and final values");
  t64_machine_destroy(machine);
}

static void fill_pattern_buffer(t64_machine *machine) {
  size_t i;

  for (i = 0u; i < T64_ISVIEWER_BUFFER_BYTES; i++) {
    (void)t64_machine_bus_write(
        machine, T64_ISVIEWER_BASE + T64_ISVIEWER_BUFFER_OFFSET +
                    (uint32_t)i,
        1u, (uint32_t)(i & 0xffu));
  }
}

static void test_flush_boundaries(const t64_host_allocator *allocator,
                                  const uint8_t *asset,
                                  size_t asset_length) {
  t64_machine *machine = create_machine(allocator, asset, asset_length);
  const t64_output_state *output;
  bool exact;

  if (machine == NULL) {
    check(false, "flush boundary test machine is admitted");
    return;
  }
  output = &machine->output;
  fill_pattern_buffer(machine);
  check_status(t64_machine_bus_write(machine, T64_ISVIEWER_BASE + 0x14u, 4u,
                                     0u),
               T64_STATUS_OK, "zero-byte word flush is accepted");
  check(output->captured_bytes == 0u && output->byte_count == 0u,
        "zero-byte flush appends nothing");

  check_status(t64_machine_bus_write(machine, T64_ISVIEWER_BASE + 0x14u, 4u,
                                     1u),
               T64_STATUS_OK, "one-byte word flush succeeds");
  exact = output->captured_bytes == 1u && output->captured[0] == 0u;
  check(exact, "one-byte flush copies exactly one byte");

  check_status(t64_machine_bus_write(machine, T64_ISVIEWER_BASE + 0x16u, 2u,
                                     3u),
               T64_STATUS_OK, "three-byte low-halfword flush succeeds");
  exact = output->captured_bytes == 4u && output->captured[1] == 0u &&
          output->captured[2] == 1u && output->captured[3] == 2u;
  check(exact, "three-byte flush excludes the fourth byte of its word");

  check_status(t64_machine_bus_write(machine, T64_ISVIEWER_BASE + 0x14u, 4u,
                                     4u),
               T64_STATUS_OK, "four-byte word flush succeeds");
  exact = output->captured_bytes == 8u &&
          memcmp(output->captured + 4u, output->buffer, 4u) == 0;
  check(exact, "four-byte flush copies exactly one complete word");

  check_status(t64_machine_bus_write(machine, T64_ISVIEWER_BASE + 0x16u, 2u,
                                     T64_ISVIEWER_BUFFER_BYTES),
               T64_STATUS_OK, "512-byte low-halfword flush succeeds");
  exact = output->captured_bytes == 520u &&
          memcmp(output->captured + 8u, output->buffer,
                 T64_ISVIEWER_BUFFER_BYTES) == 0;
  check(exact, "512-byte flush copies the complete buffer without padding");
  check(!output->lost_output,
        "valid boundary flushes do not set lost-output state");
  t64_machine_destroy(machine);
}

static void test_count_lanes_and_repeat(const t64_host_allocator *allocator,
                                        const uint8_t *asset,
                                        size_t asset_length) {
  t64_machine *machine = create_machine(allocator, asset, asset_length);
  uint32_t value = 0u;
  bool correct;

  if (machine == NULL) {
    check(false, "count lane test machine is admitted");
    return;
  }
  check_status(t64_machine_bus_write(machine, T64_ISVIEWER_BASE + 0x20u, 4u,
                                     UINT32_C(0x41424344)),
               T64_STATUS_OK, "partial-word fixture bytes are prepared");
  check_status(t64_machine_bus_write(machine, T64_ISVIEWER_BASE + 0x14u, 2u,
                                     UINT32_C(0x1234)),
               T64_STATUS_OK, "high count halfword updates without flushing");
  check(machine->output.byte_count == UINT32_C(0x12340000) &&
            machine->output.captured_bytes == 0u,
        "high count lane is retained and does not flush");
  check_status(t64_machine_bus_read(machine, T64_ISVIEWER_BASE + 0x14u, 4u,
                                    &value),
               T64_STATUS_OK, "count word reads back both lanes");
  check(value == UINT32_C(0x12340000),
        "count word read is big-endian across its halfword lanes");
  check_status(t64_machine_bus_read(machine, T64_ISVIEWER_BASE + 0x14u, 2u,
                                    &value),
               T64_STATUS_OK, "count high halfword reads back");
  check(value == UINT32_C(0x1234), "high count lane read is exact");

  check_status(t64_machine_bus_write(machine, T64_ISVIEWER_BASE + 0x16u, 2u,
                                     3u),
               T64_STATUS_OK, "low count halfword flushes its unsigned value");
  correct = machine->output.byte_count == UINT32_C(0x12340003) &&
            machine->output.captured_bytes == 3u &&
            memcmp(machine->output.captured, "ABC", 3u) == 0;
  check(correct, "low count lane flushes three exact bytes and preserves high lane");
  check_status(t64_machine_bus_write(machine, T64_ISVIEWER_BASE + 0x16u, 2u,
                                     3u),
               T64_STATUS_OK, "repeated low-lane flush succeeds");
  correct = machine->output.captured_bytes == 6u &&
            memcmp(machine->output.captured, "ABCABC", 6u) == 0;
  check(correct, "repeated flush appends the same bytes again");
  check_status(t64_machine_bus_write(machine, T64_ISVIEWER_BASE + 0x14u, 4u,
                                     4u),
               T64_STATUS_OK, "full word count flush succeeds");
  correct = machine->output.captured_bytes == 10u &&
            memcmp(machine->output.captured + 6u, "ABCD", 4u) == 0 &&
            machine->output.byte_count == 4u;
  check(correct, "word count flushes all 32 count bits and exact buffer bytes");
  t64_machine_destroy(machine);
}

static void test_oversize_and_total_limit(
    const t64_host_allocator *allocator, const uint8_t *asset,
    size_t asset_length) {
  t64_machine *oversize = create_machine(allocator, asset, asset_length);
  t64_machine *total = create_machine(allocator, asset, asset_length);
  uint32_t previous_count;
  bool full_capture;
  size_t i;

  if (oversize == NULL || total == NULL) {
    check(false, "overflow test machines are admitted");
    t64_machine_destroy(oversize);
    t64_machine_destroy(total);
    return;
  }
  fill_pattern_buffer(oversize);
  check_status(t64_machine_bus_write(oversize, T64_ISVIEWER_BASE + 0x14u, 4u,
                                     513u),
               T64_STATUS_BUS_DEVICE_REFUSED,
               "513-byte flush is refused");
  check(oversize->output.lost_output,
        "oversize flush sets sticky lost-output state");
  check(oversize->output.captured_bytes == 0u &&
            oversize->output.byte_count == 0u,
        "oversize flush changes no capture bytes or count register");
  check_status(t64_machine_bus_write(oversize, T64_ISVIEWER_BASE + 0x16u, 2u,
                                     1u),
               T64_STATUS_BUS_DEVICE_REFUSED,
               "sticky loss refuses a later nonzero flush");
  check(oversize->output.captured_bytes == 0u &&
            oversize->output.lost_output,
        "later refused flush cannot hide or partially capture output");

  for (i = 0u; i < T64_ISVIEWER_BUFFER_BYTES; i += 4u) {
    (void)t64_machine_bus_write(
        total, T64_ISVIEWER_BASE + T64_ISVIEWER_BUFFER_OFFSET +
                   (uint32_t)i,
        4u, UINT32_C(0x78787878));
  }
  for (i = 0u; i < T64_ISVIEWER_CAPTURE_BYTES / T64_ISVIEWER_BUFFER_BYTES;
       i++) {
    check_status(t64_machine_bus_write(total, T64_ISVIEWER_BASE + 0x16u, 2u,
                                       T64_ISVIEWER_BUFFER_BYTES),
                 T64_STATUS_OK, "capture-limit chunk is accepted in full");
  }
  previous_count = total->output.byte_count;
  check(total->output.captured_bytes == T64_ISVIEWER_CAPTURE_BYTES,
        "eight chunks fill the 4096-byte lifetime capture exactly");
  check(bytes_are(total->output.captured, T64_ISVIEWER_CAPTURE_BYTES,
                  (uint8_t)'x'),
        "lifetime capture contains every accepted guest byte");
  check_status(t64_machine_bus_write(total, T64_ISVIEWER_BASE + 0x16u, 2u,
                                     1u),
               T64_STATUS_BUS_DEVICE_REFUSED,
               "byte beyond lifetime capture cap is refused");
  full_capture = total->output.captured_bytes == T64_ISVIEWER_CAPTURE_BYTES &&
                 total->output.byte_count == previous_count &&
                 total->output.lost_output &&
                 bytes_are(total->output.captured,
                           T64_ISVIEWER_CAPTURE_BYTES, (uint8_t)'x');
  check(full_capture,
        "total overflow is sticky and leaves the full capture unchanged");
  t64_machine_destroy(oversize);
  t64_machine_destroy(total);
}

static bool digest_changes_and_restores(t64_machine *machine, void *field,
                                        const char *name) {
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t changed[T64_DIGEST_SIZE];
  uint8_t restored[T64_DIGEST_SIZE];
  uint8_t *field_byte = (uint8_t *)field;
  t64_status before_status;
  t64_status changed_status;
  t64_status restored_status;
  bool passed;

  before_status = t64_machine_digest(machine, before);
  field_byte[0] ^= UINT8_C(1);
  changed_status = t64_machine_digest(machine, changed);
  field_byte[0] ^= UINT8_C(1);
  restored_status = t64_machine_digest(machine, restored);
  passed = before_status == T64_STATUS_OK &&
           changed_status == T64_STATUS_OK &&
           restored_status == T64_STATUS_OK &&
           memcmp(before, changed, sizeof before) != 0 &&
           memcmp(before, restored, sizeof before) == 0;
  check(passed, name);
  return passed;
}

static void test_v7_digest_covers_output_state(
    const t64_host_allocator *allocator, const uint8_t *asset,
    size_t asset_length) {
  t64_machine *machine = create_machine(allocator, asset, asset_length);

  if (machine == NULL) {
    check(false, "state digest test machine is admitted");
    return;
  }
  (void)digest_changes_and_restores(machine, &machine->output.magic_register,
                                    "v7 digest changes with magic register");
  (void)digest_changes_and_restores(machine, &machine->output.byte_count,
                                    "v7 digest changes with pending byte count");
  (void)digest_changes_and_restores(machine, &machine->output.buffer[0],
                                    "v7 digest changes with pending buffer data");
  (void)digest_changes_and_restores(machine, &machine->output.captured_bytes,
                                    "v7 digest changes with capture length");
  (void)digest_changes_and_restores(machine, &machine->output.captured[0],
                                    "v7 digest changes with captured bytes");
  (void)digest_changes_and_restores(machine, &machine->output.lost_output,
                                    "v7 digest changes with sticky overflow state");
  t64_machine_destroy(machine);
}

static void test_reset_and_instance_isolation(
    const t64_host_allocator *allocator, const uint8_t *asset,
    size_t asset_length) {
  t64_machine *first = create_machine(allocator, asset, asset_length);
  t64_machine *second = create_machine(allocator, asset, asset_length);
  uint32_t value = 0u;

  if (first == NULL || second == NULL) {
    check(false, "reset and isolation machines are admitted");
    t64_machine_destroy(first);
    t64_machine_destroy(second);
    return;
  }
  check_status(t64_machine_bus_write(first, T64_ISVIEWER_BASE, 4u,
                                     UINT32_C(0xedcba987)),
               T64_STATUS_OK, "first instance accepts its own magic write");
  check_status(t64_machine_bus_read(second, T64_ISVIEWER_BASE, 4u, &value),
               T64_STATUS_OK, "second instance magic register remains readable");
  check(value == 0u && second->output.captured_bytes == 0u,
        "ISViewer state is isolated per machine instance");
  check_status(t64_machine_bus_write(first, T64_ISVIEWER_BASE + 0x20u, 4u,
                                     UINT32_C(0x41424344)),
               T64_STATUS_OK, "first instance buffer is writable");
  check_status(t64_machine_bus_write(first, T64_ISVIEWER_BASE + 0x14u, 4u,
                                     4u),
               T64_STATUS_OK, "first instance captures its own bytes");
  check(first->output.captured_bytes == 4u &&
            second->output.captured_bytes == 0u,
        "capture in one machine does not affect another instance");
  check_status(t64_machine_admit_rom(first, asset, asset_length),
               T64_STATUS_OK, "re-admission resets guest output state");
  check(output_state_is_reset(&first->output),
        "reset clears magic, count, buffer, capture and sticky loss");
  t64_machine_destroy(first);
  t64_machine_destroy(second);
}

static void test_actual_hello(const t64_host_allocator *allocator,
                              const uint8_t *hello, size_t hello_length) {
  static const uint8_t expected[] = "Tendies64 hello via open IPL3\n";
  t64_machine *machine = create_machine(allocator, hello, hello_length);
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(700000)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};
  t64_status status;
  bool exact;

  if (machine == NULL) {
    check(false, "actual hello machine is admitted");
    return;
  }
  status = advance_guest_in_chunks(machine, request.value, &result);
  check_status(status, T64_STATUS_OK,
               "pinned IPL3 advances the actual authored hello guest");
  (void)printf("# hello stop=%u tick=%llu retired=%llu pc=0x%08x last_pc=0x%08x opcode=0x%08x output=(magic=0x%08x count=%u captured=%u lost=%u)\n",
               result.reason, (unsigned long long)machine->tick,
               (unsigned long long)machine->cpu.retired_instructions,
               machine->cpu.pc, machine->cpu.last_pc,
               machine->cpu.last_opcode, machine->output.magic_register,
               machine->output.byte_count, machine->output.captured_bytes,
               (unsigned)machine->output.lost_output);
  check(result.reason == T64_STOP_REASON_CPU_UNSUPPORTED,
        "hello retains its explicit unsupported BREAK boundary after output");
  exact = machine->output.captured_bytes == sizeof expected - 1u &&
          memcmp(machine->output.captured, expected, sizeof expected - 1u) == 0;
  check(exact, "actual hello captures the independent exact 30-byte transcript");
  check(machine->output.magic_register == UINT32_C(0x12345678) &&
            machine->output.byte_count == sizeof expected - 1u &&
            !machine->output.lost_output,
        "actual hello completes the libdragon magic handshake and exact flush");
  check_status(t64_machine_cpu_stop_detail(machine, &detail), T64_STATUS_OK,
               "unsupported post-output stop detail is available");
  check(detail.opcode == UINT32_C(0x0000000d),
        "captured output does not turn the unsupported BREAK into guest pass");
  t64_machine_destroy(machine);
}

static void test_actual_overflow_guest(const t64_host_allocator *allocator,
                                       const uint8_t *overflow_rom,
                                       size_t overflow_length) {
  t64_machine *machine =
      create_machine(allocator, overflow_rom, overflow_length);
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(700000)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  bool exact;

  if (machine == NULL) {
    check(false, "overflow-control machine is admitted");
    return;
  }
  check_status(advance_guest_in_chunks(machine, request.value, &result),
               T64_STATUS_OK,
               "pinned IPL3 advances the authored overflow-control guest");
  (void)printf("# overflow stop=%u tick=%llu retired=%llu pc=0x%08x last_pc=0x%08x opcode=0x%08x output=(count=%u captured=%u lost=%u)\n",
               result.reason, (unsigned long long)machine->tick,
               (unsigned long long)machine->cpu.retired_instructions,
               machine->cpu.pc, machine->cpu.last_pc,
               machine->cpu.last_opcode, machine->output.byte_count,
               machine->output.captured_bytes,
               (unsigned)machine->output.lost_output);
  check(result.reason == T64_STOP_REASON_CPU_UNSUPPORTED,
        "overflow control stops explicitly on the refused output flush");
  exact = machine->output.captured_bytes == T64_ISVIEWER_CAPTURE_BYTES &&
          bytes_are(machine->output.captured, T64_ISVIEWER_CAPTURE_BYTES,
                    (uint8_t)'x') &&
          machine->output.lost_output && machine->output.byte_count == 512u;
  check(exact,
        "actual guest fills 4096 bytes then cannot append its 4097th byte");
  t64_machine_destroy(machine);
}

int main(void) {
  const t64_host_allocator allocator = {test_allocate, test_release, NULL};
  uint8_t asset[T64_IPL3_ASSET_BYTES];
  uint8_t *hello = (uint8_t *)malloc(T64_MAX_FIXTURE_BYTES);
  uint8_t *overflow = (uint8_t *)malloc(T64_MAX_FIXTURE_BYTES);
  size_t asset_length = 0u;
  size_t hello_length = 0u;
  size_t overflow_length = 0u;
  const char *asset_path = getenv("T64_TEST_IPL3_PATH");
  bool inputs_ok = hello != NULL && overflow != NULL &&
                   read_file(asset_path, asset, sizeof asset,
                             &asset_length) &&
                   asset_length == sizeof asset &&
                   read_named_fixture("hello.z64", hello,
                                      T64_MAX_FIXTURE_BYTES, &hello_length) &&
                   read_named_fixture("hello-output-overflow.z64", overflow,
                                      T64_MAX_FIXTURE_BYTES,
                                      &overflow_length);

  puts("TAP version 13");
  if (!inputs_ok) {
    check(false, "qualified IPL3 and authored guest fixtures are available");
  } else {
    test_magic_register(&allocator, asset, asset_length);
    test_buffer_endianness_and_refusals(&allocator, asset, asset_length);
    test_flush_boundaries(&allocator, asset, asset_length);
    test_count_lanes_and_repeat(&allocator, asset, asset_length);
    test_oversize_and_total_limit(&allocator, asset, asset_length);
    test_v7_digest_covers_output_state(&allocator, asset, asset_length);
    test_reset_and_instance_isolation(&allocator, asset, asset_length);
    test_actual_hello(&allocator, hello, hello_length);
    test_actual_overflow_guest(&allocator, overflow, overflow_length);
  }
  (void)printf("1..%d\n", tap_count);
  free(hello);
  free(overflow);
  return tap_failures == 0 && tap_count > 0 ? 0 : 1;
}
