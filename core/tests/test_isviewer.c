#include "t64_internal.h"
#include "t64_sha256.h"

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

static t64_machine *create_unadmitted_machine(
    const t64_host_allocator *allocator) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;

  if (t64_profile_lookup("ntsc-cart-4m-openipl3-warm-v1", &profile) !=
          T64_STATUS_OK ||
      profile == NULL ||
      t64_machine_create(allocator, profile, &machine) != T64_STATUS_OK ||
      machine == NULL) {
    t64_machine_destroy(machine);
    return NULL;
  }
  return machine;
}

static bool capture_bytes(t64_machine *machine, const uint8_t *bytes,
                          size_t length) {
  uint32_t word = 0u;
  size_t i;

  if (machine == NULL || bytes == NULL || length == 0u || length > 4u) {
    return false;
  }
  for (i = 0u; i < length; i++) {
    word |= (uint32_t)bytes[i] << (24u - (uint32_t)(i * 8u));
  }
  return t64_machine_bus_write(machine, T64_ISVIEWER_BASE + 0x20u, 4u,
                               word) == T64_STATUS_OK &&
         t64_machine_bus_write(machine, T64_ISVIEWER_BASE + 0x16u, 2u,
                               (uint32_t)length) == T64_STATUS_OK;
}

static bool state_digest(const t64_machine *machine,
                         uint8_t digest_out[T64_DIGEST_SIZE]) {
  return t64_machine_digest(machine, digest_out) == T64_STATUS_OK;
}

static bool state_digests_equal(const t64_machine *left,
                                const t64_machine *right) {
  uint8_t left_digest[T64_DIGEST_SIZE];
  uint8_t right_digest[T64_DIGEST_SIZE];

  return state_digest(left, left_digest) && state_digest(right, right_digest) &&
         memcmp(left_digest, right_digest, sizeof left_digest) == 0;
}

static bool sha256_matches(const uint8_t *bytes, size_t length,
                           const uint8_t expected_bytes[T64_DIGEST_SIZE]) {
  t64_sha256_ctx context;
  uint8_t digest[T64_DIGEST_SIZE];

  t64_sha256_init(&context);
  if (length > 0u) {
    t64_sha256_update(&context, bytes, length);
  }
  t64_sha256_final(&context, digest);
  return memcmp(digest, expected_bytes, sizeof digest) == 0;
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
         !output->lost_output && output->total_attempted_bytes == 0u &&
         !output->total_attempted_bytes_saturated;
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

static void test_public_output_and_input_seams(
    const t64_host_allocator *allocator, const uint8_t *asset,
    size_t asset_length) {
  static const uint8_t expected_bytes[] = {'A', 'B', 'C'};
  static const uint8_t next_bytes[] = {'D', 'E', 'F'};
  static const uint8_t reset_bytes[] = {'N', 'E', 'W'};
  t64_machine *machine = create_machine(allocator, asset, asset_length);
  t64_output_drain_result drain = {0};
  uint8_t bytes[sizeof expected_bytes] = {0};
  uint8_t too_small[2] = {UINT8_C(0x5a), UINT8_C(0x5a)};
  uint8_t state_before[T64_DIGEST_SIZE];
  uint8_t state_after[T64_DIGEST_SIZE];
  uint8_t expected_digest[T64_DIGEST_SIZE];
  t64_sha256_ctx sha_context;
  t64_status status;
  size_t i;
  bool input_event_found = false;

  if (machine == NULL) {
    check(false, "public observation seam machine is admitted");
    return;
  }
  check(capture_bytes(machine, expected_bytes, sizeof expected_bytes),
        "public output test seeds exact guest-owned bytes");
  check(state_digest(machine, state_before),
        "state digest is available before host drain");

  status = t64_machine_output_drain(machine, bytes, sizeof bytes, &drain);
  check_status(status, T64_STATUS_OK,
               "ready output drain returns the captured guest bytes");
  check(drain.copied_bytes == sizeof expected_bytes &&
            drain.required_bytes == sizeof expected_bytes &&
            memcmp(bytes, expected_bytes, sizeof expected_bytes) == 0,
        "output drain copies the exact pending capture prefix");
  t64_sha256_init(&sha_context);
  t64_sha256_update(&sha_context, expected_bytes, sizeof expected_bytes);
  t64_sha256_final(&sha_context, expected_digest);
  check(drain.total_attempted_bytes == sizeof expected_bytes &&
            !drain.lost_output &&
            memcmp(drain.captured_sha256, expected_digest,
                   sizeof expected_digest) == 0,
        "drain reports lifetime total, prefix hash and no loss");
  check(state_digest(machine, state_after) &&
            memcmp(state_before, state_after, sizeof state_before) == 0,
        "copying output does not change the canonical machine digest");

  (void)memset(bytes, 0x5a, sizeof bytes);
  drain = (t64_output_drain_result){0};
  check_status(t64_machine_output_drain(machine, bytes, sizeof bytes, &drain),
               T64_STATUS_OK, "repeated empty drain succeeds");
  check(drain.copied_bytes == 0u && drain.required_bytes == 0u &&
            drain.total_attempted_bytes == sizeof expected_bytes &&
            bytes_are(bytes, sizeof bytes, UINT8_C(0x5a)),
        "repeated empty drain copies nothing and preserves lifetime total");

  check(capture_bytes(machine, next_bytes, sizeof next_bytes),
        "next output chunk is captured independently");
  check(state_digest(machine, state_before),
        "state digest is available before short-buffer query");
  (void)memset(&drain, 0xa5, sizeof drain);
  check_status(t64_machine_output_drain(machine, too_small, sizeof too_small,
                                        &drain),
               T64_STATUS_BUFFER_TOO_SMALL,
               "short output buffer reports the required pending length");
  check(drain.copied_bytes == 0u &&
            drain.required_bytes == sizeof next_bytes &&
            drain.total_attempted_bytes == 2u * sizeof expected_bytes &&
            bytes_are(too_small, sizeof too_small, UINT8_C(0x5a)),
        "short output query leaves bytes and observer cursor unchanged");
  check(state_digest(machine, state_after) &&
            memcmp(state_before, state_after, sizeof state_before) == 0,
        "short output query leaves canonical state unchanged");
  (void)memset(bytes, 0, sizeof bytes);
  check_status(t64_machine_output_drain(machine, bytes, sizeof bytes, &drain),
               T64_STATUS_OK, "retry after short buffer copies the same chunk");
  check(drain.copied_bytes == sizeof next_bytes &&
            memcmp(bytes, next_bytes, sizeof next_bytes) == 0 &&
            sha256_matches((const uint8_t *)"ABCDEF", 6u,
                           drain.captured_sha256),
        "captured-prefix hash includes all lifetime output in order");

  check_status(t64_machine_queue_input(machine, machine->tick, 0u,
                                       UINT16_C(0x8001), -128, 127),
               T64_STATUS_OK,
               "port-zero logical input is accepted for trace scheduling");
  for (i = 0u; i < machine->queue_count; i++) {
    size_t index = (machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY;
    const t64_event *event = &machine->events[index];

    if (event->device == T64_TRACE_DEVICE_INPUT &&
        event->kind == T64_TRACE_KIND_INPUT_SUBMITTED) {
      input_event_found = event->tick == machine->tick &&
                          event->value == UINT64_C(0x7f808001) &&
                          event->token == 0u;
      break;
    }
  }
  check(input_event_found,
        "queued input encodes buttons and signed axes in the v7 event tuple");

  check_status(t64_machine_admit_rom(machine, asset, asset_length),
               T64_STATUS_OK, "successful image replacement resets output");
  check(output_state_is_reset(&machine->output),
        "image replacement clears guest capture and loss state");
  check(capture_bytes(machine, reset_bytes, sizeof reset_bytes),
        "replacement image produces a fresh capture prefix");
  (void)memset(bytes, 0, sizeof bytes);
  check_status(t64_machine_output_drain(machine, bytes, sizeof bytes, &drain),
               T64_STATUS_OK, "drain works after output lifecycle reset");
  check(drain.copied_bytes == sizeof reset_bytes &&
            drain.total_attempted_bytes == sizeof reset_bytes &&
            memcmp(bytes, reset_bytes, sizeof reset_bytes) == 0,
        "reset observer cursor begins at the new image prefix");
  t64_machine_destroy(machine);
}

static void test_output_drain_refusals_and_isolation(
    const t64_host_allocator *allocator, const uint8_t *asset,
    size_t asset_length) {
  static const uint8_t seeded[] = {'X', 'Y', 'Z'};
  t64_machine *machine = create_machine(allocator, asset, asset_length);
  t64_machine *other = create_machine(allocator, asset, asset_length);
  t64_machine *unadmitted = create_unadmitted_machine(allocator);
  t64_output_drain_result result;
  t64_output_drain_result before_result;
  uint8_t output[sizeof seeded];
  uint8_t before_output[sizeof seeded];
  uint8_t state_before[T64_DIGEST_SIZE];
  uint8_t state_after[T64_DIGEST_SIZE];
  uint8_t owned_before[sizeof result];
  uint8_t owned_after[sizeof result];
  union {
    t64_output_drain_result result;
    uint8_t bytes[sizeof(t64_output_drain_result)];
  } overlap;
  t64_status status;

  if (machine == NULL || other == NULL || unadmitted == NULL) {
    check(false, "output refusal and isolation machines are available");
    t64_machine_destroy(machine);
    t64_machine_destroy(other);
    t64_machine_destroy(unadmitted);
    return;
  }
  check(capture_bytes(machine, seeded, sizeof seeded),
        "refusal test seeds a pending output chunk");
  check(state_digest(machine, state_before),
        "refusal test captures its pre-call state digest");
  (void)memset(&result, 0xa5, sizeof result);
  (void)memcpy(&before_result, &result, sizeof result);
  (void)memset(output, 0x5a, sizeof output);
  (void)memcpy(before_output, output, sizeof output);
  status = t64_machine_output_drain(machine, output, SIZE_MAX, &result);
  check_status(status, T64_STATUS_INVALID_ARGUMENT,
               "overflowing data span is refused before copying");
  check(memcmp(&result, &before_result, sizeof result) == 0 &&
            memcmp(output, before_output, sizeof output) == 0,
        "overflow refusal leaves caller result and bytes unchanged");

  (void)memset(&result, 0xa5, sizeof result);
  (void)memcpy(&before_result, &result, sizeof result);
  status = t64_machine_output_drain(machine, NULL, sizeof output, &result);
  check_status(status, T64_STATUS_INVALID_ARGUMENT,
               "null nonempty data span is refused");
  check(memcmp(&result, &before_result, sizeof result) == 0,
        "null data refusal leaves result unchanged");
  check_status(t64_machine_output_drain(machine, output, sizeof output, NULL),
               T64_STATUS_INVALID_ARGUMENT,
               "null result span is refused");

  (void)memset(&overlap, 0xa5, sizeof overlap);
  (void)memcpy(owned_before, overlap.bytes, sizeof owned_before);
  status = t64_machine_output_drain(machine, overlap.bytes, sizeof seeded,
                                    &overlap.result);
  check_status(status, T64_STATUS_INVALID_ARGUMENT,
               "pairwise-overlapping result and data spans are refused");
  (void)memcpy(owned_after, overlap.bytes, sizeof owned_after);
  check(memcmp(owned_before, owned_after, sizeof owned_before) == 0,
        "pairwise alias refusal leaves the shared caller bytes unchanged");

  (void)memset(&result, 0xa5, sizeof result);
  (void)memcpy(&before_result, &result, sizeof result);
  (void)memcpy(owned_before, machine->ram, sizeof owned_before);
  check_status(t64_machine_output_drain(machine, machine->ram, sizeof seeded,
                                        &result),
               T64_STATUS_INVALID_ARGUMENT,
               "RDRAM alias is refused");
  (void)memcpy(owned_after, machine->ram, sizeof owned_after);
  check(memcmp(owned_before, owned_after, sizeof owned_before) == 0 &&
            memcmp(&result, &before_result, sizeof result) == 0,
        "RDRAM alias refusal preserves both caller and machine bytes");
  check_status(t64_machine_output_drain(
                   machine, (uint8_t *)machine->image_storage.data,
                   sizeof seeded, &result),
               T64_STATUS_INVALID_ARGUMENT, "admitted-image alias is refused");
  check_status(t64_machine_output_drain(machine, (uint8_t *)machine,
                                        sizeof seeded, &result),
               T64_STATUS_INVALID_ARGUMENT, "machine-storage alias is refused");

  (void)memcpy(owned_before, machine->ram, sizeof owned_before);
  check_status(t64_machine_output_drain(
                   machine, output, sizeof output,
                   (t64_output_drain_result *)machine->ram),
               T64_STATUS_INVALID_ARGUMENT,
               "result span inside RDRAM is refused");
  (void)memcpy(owned_after, machine->ram, sizeof owned_after);
  check(memcmp(owned_before, owned_after, sizeof owned_before) == 0,
        "result alias refusal preserves RDRAM bytes");
  check(state_digest(machine, state_after) &&
            memcmp(state_before, state_after, sizeof state_before) == 0,
        "all invalid output spans leave guest future state unchanged");

  (void)memset(&result, 0xa5, sizeof result);
  (void)memcpy(&before_result, &result, sizeof result);
  (void)memset(output, 0x5a, sizeof output);
  (void)memcpy(before_output, output, sizeof output);
  check_status(t64_machine_output_drain(unadmitted, output, sizeof output,
                                        &result),
               T64_STATUS_NOT_READY,
               "drain before image admission is not ready");
  check(memcmp(&result, &before_result, sizeof result) == 0 &&
            memcmp(output, before_output, sizeof output) == 0,
        "not-ready drain leaves caller outputs unchanged");

  check_status(t64_machine_admit_rom(machine, asset, asset_length),
               T64_STATUS_OK,
               "interleaving control starts a fresh first-instance capture");
  check(capture_bytes(other, (const uint8_t *)"ABC", 3u),
        "second instance captures its independent bytes");
  check(capture_bytes(machine, (const uint8_t *)"XYZ", 3u),
        "first instance captures its independent bytes");
  check_status(t64_machine_output_drain(machine, output, sizeof output,
                                        &result),
               T64_STATUS_OK, "first interleaved instance drains");
  check(memcmp(output, "XYZ", sizeof output) == 0,
        "first interleaved instance returns only its own bytes");
  check_status(t64_machine_output_drain(other, output, sizeof output,
                                        &result),
               T64_STATUS_OK, "second interleaved instance drains");
  check(memcmp(output, "ABC", sizeof output) == 0,
        "second interleaved instance returns only its own bytes");
  t64_machine_destroy(machine);
  t64_machine_destroy(other);
  t64_machine_destroy(unadmitted);
}

static void test_output_loss_observation(
    const t64_host_allocator *allocator, const uint8_t *asset,
    size_t asset_length) {
  t64_machine *machine = create_machine(allocator, asset, asset_length);
  t64_output_drain_result result = {0};
  uint8_t full_capture[T64_ISVIEWER_CAPTURE_BYTES];
  size_t i;

  if (machine == NULL) {
    check(false, "loss observation test machine is admitted");
    return;
  }
  check_status(t64_machine_bus_write(machine, T64_ISVIEWER_BASE + 0x14u, 4u,
                                     513u),
               T64_STATUS_BUS_DEVICE_REFUSED,
               "overflow setup sets sticky output loss");
  check_status(t64_machine_output_drain(machine, NULL, 0u, &result),
               T64_STATUS_OK, "empty lost-output observation succeeds");
  check(result.copied_bytes == 0u && result.required_bytes == 0u &&
            result.total_attempted_bytes == UINT64_C(513) &&
            result.lost_output && result.total_attempted_bytes_exact,
        "drain reports sticky loss even when no output was retained");

  check_status(t64_machine_admit_rom(machine, asset, asset_length),
               T64_STATUS_OK, "new image clears prior output loss");
  for (i = 0u; i < T64_ISVIEWER_BUFFER_BYTES; i += 4u) {
    (void)t64_machine_bus_write(
        machine, T64_ISVIEWER_BASE + T64_ISVIEWER_BUFFER_OFFSET +
                     (uint32_t)i,
        4u, UINT32_C(0x78787878));
  }
  for (i = 0u; i < T64_ISVIEWER_CAPTURE_BYTES / T64_ISVIEWER_BUFFER_BYTES;
       i++) {
    (void)t64_machine_bus_write(machine, T64_ISVIEWER_BASE + 0x16u, 2u,
                                T64_ISVIEWER_BUFFER_BYTES);
  }
  (void)t64_machine_bus_write(machine, T64_ISVIEWER_BASE + 0x16u, 2u, 1u);
  check(machine->output.lost_output &&
            machine->output.captured_bytes == T64_ISVIEWER_CAPTURE_BYTES,
        "lifetime cap retains its prefix and records the refused next byte");
  check_status(t64_machine_output_drain(machine, full_capture,
                                        sizeof full_capture, &result),
               T64_STATUS_OK, "full retained prefix remains drainable after loss");
  check(result.copied_bytes == T64_ISVIEWER_CAPTURE_BYTES &&
            result.total_attempted_bytes == T64_ISVIEWER_CAPTURE_BYTES + 1u &&
            result.lost_output && result.total_attempted_bytes_exact &&
            bytes_are(full_capture, sizeof full_capture, (uint8_t)'x'),
        "drain preserves all retained bytes and reports sticky loss");
  t64_machine_destroy(machine);
}

static void test_input_refusals_and_identity(
    const t64_host_allocator *allocator, const uint8_t *asset,
    size_t asset_length) {
  t64_machine *first = create_machine(allocator, asset, asset_length);
  t64_machine *same = create_machine(allocator, asset, asset_length);
  t64_machine *different = create_machine(allocator, asset, asset_length);
  t64_machine *unadmitted = create_unadmitted_machine(allocator);
  t64_machine *past = create_machine(allocator, asset, asset_length);
  t64_machine *full = create_machine(allocator, asset, asset_length);
  t64_machine *exhausted = create_machine(allocator, asset, asset_length);
  t64_machine *terminal = create_machine(allocator, asset, asset_length);
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  t64_stop_request request = {T64_STOP_GUEST_TICK, 1u};
  t64_stop_result result;
  t64_status status;

  if (first == NULL || same == NULL || different == NULL || unadmitted == NULL ||
      past == NULL || full == NULL || exhausted == NULL || terminal == NULL) {
    check(false, "input refusal test machines are available");
    t64_machine_destroy(first);
    t64_machine_destroy(same);
    t64_machine_destroy(different);
    t64_machine_destroy(unadmitted);
    t64_machine_destroy(past);
    t64_machine_destroy(full);
    t64_machine_destroy(exhausted);
    t64_machine_destroy(terminal);
    return;
  }

  check_status(t64_machine_queue_input(first, 0u, 0u, UINT16_C(0x1234), -1, 1),
               T64_STATUS_OK, "first logical input submission succeeds");
  check_status(t64_machine_queue_input(same, 0u, 0u, UINT16_C(0x1234), -1, 1),
               T64_STATUS_OK, "identical logical input submission succeeds");
  check(state_digests_equal(first, same),
        "identical pending input tuples produce identical future digests");
  check_status(t64_machine_queue_input(different, 0u, 0u,
                                       UINT16_C(0x1235), -1, 1),
               T64_STATUS_OK, "different logical input submission succeeds");
  check(!state_digests_equal(first, different),
        "pending input payload changes the canonical future digest");

  check(state_digest(first, before), "input refusal baseline digest is available");
  check_status(t64_machine_queue_input(first, 0u, 1u, 0u, 0, 0),
               T64_STATUS_INVALID_ARGUMENT, "nonzero input port is refused");
  check(state_digest(first, after) && memcmp(before, after, sizeof before) == 0,
        "unsupported port refusal leaves pending queue and digest unchanged");
  check_status(t64_machine_queue_input(first, 0u, 0u, 0u, -129, 0),
               T64_STATUS_INVALID_ARGUMENT, "axis below signed-eight range is refused");
  check_status(t64_machine_queue_input(first, 0u, 0u, 0u, 0, 128),
               T64_STATUS_INVALID_ARGUMENT, "axis above signed-eight range is refused");
  check(state_digest(first, after) && memcmp(before, after, sizeof before) == 0,
        "axis refusals preserve queue and sequence state");

  check_status(t64_machine_queue_input(unadmitted, 0u, 0u, 0u, 0, 0),
               T64_STATUS_NOT_READY, "input before ROM admission is refused");
  check_status(t64_machine_advance(past, &request, &result), T64_STATUS_OK,
               "past-time control advances a ready guest");
  check(result.tick == 1u, "past-time control reaches the requested tick");
  check(state_digest(past, before), "past-time baseline digest is available");
  check_status(t64_machine_queue_input(past, 0u, 0u, 0u, 0, 0),
               T64_STATUS_TIME_REVERSED, "input at a past guest tick is refused");
  check(state_digest(past, after) && memcmp(before, after, sizeof before) == 0,
        "past-time refusal leaves digest and queue unchanged");

  while (full->queue_count < full->profile.event_queue_capacity) {
    status = t64_test_schedule_synthetic(full, UINT64_C(10),
                                         T64_TEST_EVENT_NOOP, 0u);
    if (status != T64_STATUS_OK) {
      break;
    }
  }
  check(full->queue_count == full->profile.event_queue_capacity,
        "queue-full control fills the bounded per-instance scheduler");
  check(state_digest(full, before), "queue-full baseline digest is available");
  check_status(t64_machine_queue_input(full, 0u, 0u, 1u, 0, 0),
               T64_STATUS_QUEUE_FULL, "full scheduler refuses input atomically");
  check(state_digest(full, after) && memcmp(before, after, sizeof before) == 0,
        "queue-full refusal leaves event tuples and sequence unchanged");

  exhausted->sequence_exhausted = true;
  check(state_digest(exhausted, before),
        "sequence-exhaustion baseline digest is available");
  check_status(t64_machine_queue_input(exhausted, 0u, 0u, 1u, 0, 0),
               T64_STATUS_SEQUENCE_EXHAUSTED,
               "exhausted scheduler sequence refuses input");
  check(state_digest(exhausted, after) &&
            memcmp(before, after, sizeof before) == 0,
        "sequence refusal leaves next sequence and queue unchanged");

  terminal->cpu.stopped = true;
  check(state_digest(terminal, before),
        "terminal-state baseline digest is available");
  check_status(t64_machine_queue_input(terminal, 0u, 0u, 1u, 0, 0),
               T64_STATUS_NOT_READY,
               "terminal CPU state refuses new input events");
  check(state_digest(terminal, after) &&
            memcmp(before, after, sizeof before) == 0,
        "terminal refusal leaves canonical future state unchanged");

  t64_machine_destroy(first);
  t64_machine_destroy(same);
  t64_machine_destroy(different);
  t64_machine_destroy(unadmitted);
  t64_machine_destroy(past);
  t64_machine_destroy(full);
  t64_machine_destroy(exhausted);
  t64_machine_destroy(terminal);
}

static void test_input_trace_order_and_observer_invariance(
    const t64_host_allocator *allocator, const uint8_t *asset,
    size_t asset_length) {
  static const uint8_t output_seed[] = {'A', 'B', 'C'};
  t64_machine *traced = create_machine(allocator, asset, asset_length);
  t64_machine *untraced = create_machine(allocator, asset, asset_length);
  t64_trace_record records[T64_TRACE_RECORD_CAPACITY];
  t64_output_drain_result drain = {0};
  t64_stop_result result;
  size_t record_count = 0u;
  uint64_t dropped = 0u;
  bool saturated = false;
  size_t input_count = 0u;
  size_t i;
  bool ordered = true;
  uint8_t bytes[sizeof output_seed];

  if (traced == NULL || untraced == NULL) {
    check(false, "trace invariance machines are admitted");
    t64_machine_destroy(traced);
    t64_machine_destroy(untraced);
    return;
  }
  check(capture_bytes(traced, output_seed, sizeof output_seed) &&
            capture_bytes(untraced, output_seed, sizeof output_seed),
        "both machines produce identical retained guest output");
  check_status(t64_machine_trace_set_enabled(traced, true), T64_STATUS_OK,
               "trace is enabled for the first instance");
  check_status(t64_machine_trace_set_enabled(untraced, false), T64_STATUS_OK,
               "trace remains disabled for the second instance");
  check_status(t64_machine_queue_input(traced, 1u, 0u, UINT16_C(0x1234), -1, 1),
               T64_STATUS_OK, "first same-tick traced input is queued");
  check_status(t64_machine_queue_input(traced, 1u, 0u, UINT16_C(0xabcd), 2, -2),
               T64_STATUS_OK, "second same-tick traced input is queued");
  check_status(t64_machine_queue_input(untraced, 1u, 0u, UINT16_C(0x1234), -1, 1),
               T64_STATUS_OK, "first same-tick untraced input is queued");
  check_status(t64_machine_queue_input(untraced, 1u, 0u,
                                       UINT16_C(0xabcd), 2, -2),
               T64_STATUS_OK, "second same-tick untraced input is queued");
  check(state_digests_equal(traced, untraced),
        "same input schedule has equal digest with different trace settings");

  check_status(t64_machine_output_drain(traced, bytes, sizeof bytes, &drain),
               T64_STATUS_OK, "first instance drains output before advance");
  check(state_digests_equal(traced, untraced),
        "different output-drain timing leaves equal canonical digests");
  check_status(advance_guest_in_chunks(traced, 1u, &result), T64_STATUS_OK,
               "traced guest advances through its scheduled input tick");
  check_status(advance_guest_in_chunks(untraced, 1u, &result), T64_STATUS_OK,
               "untraced guest advances through its scheduled input tick");
  check(state_digests_equal(traced, untraced),
        "trace and drain choices do not change guest future state");
  check_status(t64_machine_output_drain(untraced, bytes, sizeof bytes, &drain),
               T64_STATUS_OK, "second instance drains output after advance");
  check(state_digests_equal(traced, untraced),
        "post-advance drain also leaves the canonical digest unchanged");

  check_status(t64_machine_trace_drain(traced, records,
                                       T64_TRACE_RECORD_CAPACITY,
                                       &record_count, &dropped, &saturated),
               T64_STATUS_OK, "traced input records drain successfully");
  for (i = 0u; i < record_count; i++) {
    if (records[i].device == T64_TRACE_DEVICE_INPUT &&
        records[i].kind == T64_TRACE_KIND_INPUT_SUBMITTED) {
      if (input_count == 0u) {
        ordered = records[i].tick == 1u &&
                  records[i].value == UINT64_C(0x01ff1234);
      } else if (input_count == 1u) {
        ordered = ordered && records[i].tick == 1u &&
                  records[i].value == UINT64_C(0xfe02abcd);
      } else {
        ordered = false;
      }
      input_count++;
    }
  }
  check(input_count == 2u && ordered && dropped == 0u && !saturated,
        "trace identifies input source and kind at tick in submission order");
  record_count = 99u;
  check_status(t64_machine_trace_drain(untraced, records,
                                       T64_TRACE_RECORD_CAPACITY,
                                       &record_count, &dropped, &saturated),
               T64_STATUS_OK, "disabled trace drain succeeds");
  check(record_count == 0u,
        "trace-disabled input events do not create observer records");
  t64_machine_destroy(traced);
  t64_machine_destroy(untraced);
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
  t64_machine *saturated = create_machine(allocator, asset, asset_length);
  t64_output_drain_result saturation_result = {0};
  uint32_t previous_count;
  bool full_capture;
  size_t i;

  if (oversize == NULL || total == NULL || saturated == NULL) {
    check(false, "overflow test machines are admitted");
    t64_machine_destroy(oversize);
    t64_machine_destroy(total);
    t64_machine_destroy(saturated);
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
  check(oversize->output.total_attempted_bytes == UINT64_C(513) &&
            !oversize->output.total_attempted_bytes_saturated,
        "oversize rejection records the complete requested flush length");
  check_status(t64_machine_bus_write(oversize, T64_ISVIEWER_BASE + 0x16u, 2u,
                                     1u),
               T64_STATUS_BUS_DEVICE_REFUSED,
               "sticky loss refuses a later nonzero flush");
  check(oversize->output.captured_bytes == 0u &&
            oversize->output.lost_output &&
            oversize->output.total_attempted_bytes == UINT64_C(514),
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
                 total->output.total_attempted_bytes == UINT64_C(4097) &&
                 bytes_are(total->output.captured,
                           T64_ISVIEWER_CAPTURE_BYTES, (uint8_t)'x');
  check(full_capture,
        "total overflow is sticky and leaves the full capture unchanged");
  saturated->output.total_attempted_bytes = UINT64_MAX - UINT64_C(1);
  check_status(t64_machine_bus_write(saturated, T64_ISVIEWER_BASE + 0x16u,
                                     2u, 2u),
               T64_STATUS_BUS_DEVICE_REFUSED,
               "attempted-byte counter overflow refuses before appending");
  check(saturated->output.captured_bytes == 0u &&
            saturated->output.total_attempted_bytes == UINT64_MAX &&
            saturated->output.total_attempted_bytes_saturated &&
            saturated->output.lost_output,
        "counter overflow saturates, records inexactness, and appends nothing");
  check_status(t64_machine_output_drain(saturated, NULL, 0u,
                                        &saturation_result),
               T64_STATUS_OK,
               "saturated attempted-byte total remains observable");
  check(saturation_result.total_attempted_bytes == UINT64_MAX &&
            !saturation_result.total_attempted_bytes_exact &&
            saturation_result.lost_output,
        "drain marks the saturated attempted-byte total as inexact");
  t64_machine_destroy(oversize);
  t64_machine_destroy(total);
  t64_machine_destroy(saturated);
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

static void test_v8_digest_covers_output_state(
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
                                    "v8 digest changes with sticky overflow state");
  (void)digest_changes_and_restores(
      machine, &machine->output.total_attempted_bytes,
      "v8 digest changes with attempted output bytes");
  (void)digest_changes_and_restores(
      machine, &machine->output.total_attempted_bytes_saturated,
      "v8 digest changes with attempted output total exactness");
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
  t64_output_drain_result drain = {0};
  uint8_t host_output[sizeof expected - 1u] = {0};
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
  check(result.reason == T64_STOP_REASON_GUEST_COMPLETION &&
            machine->tick == UINT64_C(530438) &&
            machine->cpu.retired_instructions == UINT64_C(530437) &&
            machine->cpu.pc == UINT32_C(0x80000478),
        "exact authored BREAK convention stops before instruction retirement");
  exact = machine->output.captured_bytes == sizeof expected - 1u &&
          memcmp(machine->output.captured, expected, sizeof expected - 1u) == 0;
  check(exact, "actual hello captures the independent exact 30-byte transcript");
  check(machine->output.magic_register == UINT32_C(0x12345678) &&
            machine->output.byte_count == sizeof expected - 1u &&
            !machine->output.lost_output,
        "actual hello completes the libdragon magic handshake and exact flush");
  check_status(t64_machine_output_drain(machine, host_output,
                                        sizeof host_output, &drain),
               T64_STATUS_OK,
               "public output drain remains available after terminal BREAK");
  check(drain.copied_bytes == sizeof host_output &&
            drain.total_attempted_bytes == sizeof host_output &&
            !drain.lost_output &&
            memcmp(host_output, expected, sizeof host_output) == 0,
        "public drain copies the exact actual guest transcript");
  check_status(t64_machine_cpu_stop_detail(machine, &detail), T64_STATUS_OK,
               "software completion stop detail is available");
  check(detail.retired_instructions == UINT64_C(530437) &&
            detail.pc == UINT32_C(0x80000478) &&
            detail.opcode == UINT32_C(0x0000000d),
        "captured output does not independently establish a guest pass");
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
          machine->output.lost_output && machine->output.byte_count == 512u &&
          machine->output.total_attempted_bytes == UINT64_C(4097) &&
          !machine->output.total_attempted_bytes_saturated;
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
    test_v8_digest_covers_output_state(&allocator, asset, asset_length);
    test_reset_and_instance_isolation(&allocator, asset, asset_length);
    test_public_output_and_input_seams(&allocator, asset, asset_length);
    test_output_drain_refusals_and_isolation(&allocator, asset, asset_length);
    test_output_loss_observation(&allocator, asset, asset_length);
    test_input_refusals_and_identity(&allocator, asset, asset_length);
    test_input_trace_order_and_observer_invariance(&allocator, asset,
                                                   asset_length);
    test_actual_hello(&allocator, hello, hello_length);
    test_actual_overflow_guest(&allocator, overflow, overflow_length);
  }
  (void)printf("1..%d\n", tap_count);
  free(hello);
  free(overflow);
  return tap_failures == 0 && tap_count > 0 ? 0 : 1;
}
