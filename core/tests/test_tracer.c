#line 1 "core/tests/test_tracer.c"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../t64_internal.h"
#include "../../tools/common/t64_sha256.h"
#include "t64_test.h"
#include "tendies64/t64_machine.h"

#define TEST_ROM_SIZE 4096u
#define MAX_TRACKED_ALLOCATIONS 8u
#define OPEN_IPL3_PROFILE_ID "ntsc-cart-4m-openipl3-warm-v1"
#define OPEN_IPL3_ASSET_SHA256                                               \
  "f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068"
#define OPEN_IPL3_PAYLOAD_SHA256                                             \
  "f716a90b1d36ec4253d096790791256d3543455e54473a755ff8c3d1199b15d5"

enum {
  TEST_TRACE_KIND_CPU_ENTRY = 2u,
  TEST_TRACE_KIND_CPU_RETIREMENT = 3u,
  TEST_TRACE_KIND_CPU_UNSUPPORTED = 4u
};

typedef struct {
  void *live[MAX_TRACKED_ALLOCATIONS];
  size_t capacity[MAX_TRACKED_ALLOCATIONS];
  size_t reported_capacity;
  size_t reported_alignment;
  size_t allocation_count;
  size_t release_count;
  size_t outstanding_count;
  size_t release_errors;
} allocator_state;

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  allocator_state *state = (allocator_state *)context;
  void *memory;
  size_t i;

  if (state == NULL || storage_out == NULL || alignment == 0u) {
    return false;
  }
  memory = malloc(size);
  if (memory == NULL || ((uintptr_t)memory % alignment) != 0u ||
      state->allocation_count >= MAX_TRACKED_ALLOCATIONS) {
    free(memory);
    return false;
  }
  for (i = 0; i < state->allocation_count; i++) {
    uintptr_t new_begin = (uintptr_t)memory;
    uintptr_t new_end = new_begin + size;
    uintptr_t old_begin = (uintptr_t)state->live[i];
    uintptr_t old_end = old_begin + state->capacity[i];

    if (state->live[i] != NULL && new_begin < old_end && old_begin < new_end) {
      free(memory);
      return false;
    }
  }
  state->live[state->allocation_count] = memory;
  storage_out->data = memory;
  storage_out->capacity = state->reported_capacity == 0u
                              ? size
                              : state->reported_capacity;
  storage_out->alignment = state->reported_alignment == 0u
                               ? alignment
                               : state->reported_alignment;
  state->capacity[state->allocation_count] = storage_out->capacity;
  state->allocation_count++;
  state->outstanding_count++;
  state->reported_capacity = 0u;
  state->reported_alignment = 0u;
  return true;
}

static void test_release(void *context, t64_host_storage storage) {
  allocator_state *state = (allocator_state *)context;
  size_t i;

  if (state == NULL) {
    return;
  }
  for (i = 0; i < state->allocation_count; i++) {
    if (state->live[i] == storage.data) {
      state->live[i] = NULL;
      state->release_count++;
      state->outstanding_count--;
      free(storage.data);
      return;
    }
  }
  state->release_errors++;
}

static void fill_authored_image(uint8_t image[TEST_ROM_SIZE]) {
  size_t i;

  for (i = 0; i < TEST_ROM_SIZE; i++) {
    image[i] = (uint8_t)((i * 13u + 7u) & 0xffu);
  }
  image[0] = 0x80u;
  image[1] = 0x37u;
  image[2] = 0x12u;
  image[3] = 0x40u;
  image[0x3eu] = (uint8_t)'E';
}

static void print_digest(const uint8_t digest[T64_DIGEST_SIZE]) {
  static const char digits[] = "0123456789abcdef";
  size_t i;

  for (i = 0; i < T64_DIGEST_SIZE; i++) {
    (void)putchar(digits[digest[i] >> 4]);
    (void)putchar(digits[digest[i] & 0x0fu]);
  }
}

static bool read_pinned_ipl3(uint8_t image[TEST_ROM_SIZE]) {
  const char *path = getenv("T64_TEST_IPL3_PATH");
  FILE *file;
  size_t length;
  int extra;

  if (path == NULL || path[0] == '\0' || image == NULL) {
    return false;
  }
  file = fopen(path, "rb");
  if (file == NULL) {
    return false;
  }
  length = fread(image, 1u, TEST_ROM_SIZE, file);
  extra = fgetc(file);
  if (fclose(file) != 0) {
    return false;
  }
  return length == TEST_ROM_SIZE && extra == EOF;
}

static bool bytes_match_sha256(const uint8_t *bytes, size_t length,
                               const char *expected_hex) {
  t64_sha256_ctx context;
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  char actual_hex[T64_SHA256_HEX_SIZE];

  t64_sha256_init(&context);
  t64_sha256_update(&context, bytes, length);
  t64_sha256_final(&context, digest);
  t64_sha256_hex(digest, actual_hex);
  return strcmp(actual_hex, expected_hex) == 0;
}

static int tap_open_ipl3_profile_lookup(void) {
  const t64_profile_descriptor *profile = NULL;
  t64_status status =
      t64_profile_lookup(OPEN_IPL3_PROFILE_ID, &profile);

  (void)puts("TAP version 13");
  (void)printf("%s 1 - open IPL3 warm profile is discoverable\n",
               status == T64_STATUS_OK && profile != NULL ? "ok" : "not ok");
  (void)printf("# expected status=%u and descriptor; observed status=%u\n",
               T64_STATUS_OK, status);
  (void)puts("1..1");
  return status == T64_STATUS_OK && profile != NULL ? 0 : 1;
}

static int hex_nibble(char value) {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  if (value >= 'a' && value <= 'f') {
    return value - 'a' + 10;
  }
  if (value >= 'A' && value <= 'F') {
    return value - 'A' + 10;
  }
  return -1;
}

static bool decode_sha256_hex(const char *hex,
                              uint8_t digest[T64_SHA256_DIGEST_SIZE]) {
  size_t i;

  if (hex == NULL || strlen(hex) != 64u || digest == NULL) {
    return false;
  }
  for (i = 0u; i < T64_SHA256_DIGEST_SIZE; i++) {
    int high = hex_nibble(hex[i * 2u]);
    int low = hex_nibble(hex[i * 2u + 1u]);

    if (high < 0 || low < 0) {
      return false;
    }
    digest[i] = (uint8_t)(((uint32_t)high << 4) | (uint32_t)low);
  }
  return true;
}

static bool profile_v2_identity_is_serialized(
    const t64_profile_descriptor *profile) {
  uint8_t expected[3u * T64_SHA256_DIGEST_SIZE + 4u];
  uint8_t serialized[T64_PROFILE_SERIALIZED_MAX_BYTES];
  size_t serialized_length = 0u;
  t64_status status;

  if (!decode_sha256_hex(OPEN_IPL3_ASSET_SHA256, expected) ||
      !decode_sha256_hex(OPEN_IPL3_PAYLOAD_SHA256,
                         expected + T64_SHA256_DIGEST_SIZE) ||
      !decode_sha256_hex(
          "831a0a58ef8f82bc54f880ad5a1259bddf1b3d2e21680ac6637ea5994705a7fe",
          expected + 2u * T64_SHA256_DIGEST_SIZE)) {
    return false;
  }
  expected[96] = 0u;
  expected[97] = 0u;
  expected[98] = 0u;
  expected[99] = 1u;
  status = t64_profile_serialize(profile, NULL, 0u, &serialized_length);
  if (status != T64_STATUS_BUFFER_TOO_SMALL ||
      serialized_length > sizeof serialized ||
      serialized_length < sizeof expected) {
    return false;
  }
  status = t64_profile_serialize(profile, serialized, sizeof serialized,
                                 &serialized_length);
  return status == T64_STATUS_OK &&
         memcmp(serialized + serialized_length - sizeof expected, expected,
                sizeof expected) == 0;
}

static bool run_openipl3_to_tick_bound(t64_stop_result *first_result,
                                       t64_stop_result *repeat_result,
                                       uint64_t *retired_out,
                                       uint32_t *pc_out, uint32_t *last_pc_out,
                                       uint32_t *last_opcode_out,
                                       bool *unsupported_out) {
  uint8_t image[TEST_ROM_SIZE];
  const t64_profile_descriptor *profile = NULL;
  allocator_state allocator_state_value = {0};
  const t64_host_allocator allocator = {
      test_allocate, test_release, &allocator_state_value};
  t64_machine *machine = NULL;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(100)};
  t64_status status;
  bool okay = false;

  if (!read_pinned_ipl3(image) ||
      !bytes_match_sha256(image, sizeof image, OPEN_IPL3_ASSET_SHA256) ||
      t64_profile_lookup(OPEN_IPL3_PROFILE_ID, &profile) != T64_STATUS_OK ||
      profile == NULL) {
    return false;
  }
  status = t64_machine_create(&allocator, profile, &machine);
  if (status != T64_STATUS_OK || machine == NULL) {
    return false;
  }
  if (t64_machine_admit_rom(machine, image, sizeof image) == T64_STATUS_OK &&
      t64_machine_advance(machine, &request, first_result) == T64_STATUS_OK) {
    t64_cpu_stop_detail detail = {0u, 0u, 0u};
    *retired_out = machine->cpu.retired_instructions;
    *pc_out = machine->cpu.pc;
    *last_pc_out = machine->cpu.last_pc;
    *last_opcode_out = machine->cpu.last_opcode;
    *unsupported_out =
        t64_machine_cpu_stop_detail(machine, &detail) == T64_STATUS_OK;
    okay = first_result->reason == T64_STOP_REASON_BOUND &&
           first_result->tick == UINT64_C(100) &&
           *retired_out == UINT64_C(100) &&
           *pc_out == UINT32_C(0xa4000354) &&
           *last_pc_out == UINT32_C(0xa4000350) &&
           *last_opcode_out == UINT32_C(0x240af000) && !*unsupported_out;
    request.value = UINT64_C(100);
    okay = okay && t64_machine_advance(machine, &request, repeat_result) ==
                       T64_STATUS_OK;
  }
  t64_machine_destroy(machine);
  return okay;
}

static int tap_openipl3_public_stop_and_identity(void) {
  const t64_profile_descriptor *profile = NULL;
  t64_stop_result first_result = {0u, 0u};
  t64_stop_result repeat_result = {0u, 0u};
  uint64_t retired = 0u;
  uint32_t pc = 0u;
  uint32_t last_pc = 0u;
  uint32_t last_opcode = 0u;
  bool unsupported = true;
  bool asset_setup_ok;
  bool stop_setup_ok;
  bool identity_ok;
  t64_status lookup_status =
      t64_profile_lookup(OPEN_IPL3_PROFILE_ID, &profile);

  asset_setup_ok = lookup_status == T64_STATUS_OK && profile != NULL;
  identity_ok = asset_setup_ok && profile_v2_identity_is_serialized(profile);
  stop_setup_ok = run_openipl3_to_tick_bound(
      &first_result, &repeat_result, &retired, &pc, &last_pc, &last_opcode,
      &unsupported);
  (void)puts("TAP version 13");
  (void)printf("%s 1 - exact pinned IPL3 reaches the guest tick bound\n",
               stop_setup_ok ? "ok" : "not ok");
  (void)printf("%s 2 - CPU remains active at the bounded integration point\n",
               stop_setup_ok && first_result.reason == T64_STOP_REASON_BOUND &&
                       !unsupported
                   ? "ok"
                   : "not ok");
  (void)printf("# expected first reason=BOUND tick=100; observed=%u tick=%llu\n",
               first_result.reason, (unsigned long long)first_result.tick);
  (void)printf("%s 3 - repeated same-bound advance remains deterministic\n",
               stop_setup_ok && repeat_result.reason == T64_STOP_REASON_BOUND &&
                       repeat_result.tick == UINT64_C(100)
                   ? "ok"
                   : "not ok");
  (void)printf("# expected repeat reason=BOUND tick=100; observed reason=%u tick=%llu\n",
               repeat_result.reason, (unsigned long long)repeat_result.tick);
  (void)printf("%s 4 - observed instruction boundary is exact\n",
               stop_setup_ok && retired == UINT64_C(100) &&
                       pc == UINT32_C(0xa4000354) &&
                       last_pc == UINT32_C(0xa4000350) &&
                       last_opcode == UINT32_C(0x240af000)
                   ? "ok"
                   : "not ok");
  (void)printf("# observed retired=%llu next_pc=0x%08x last_pc=0x%08x last_opcode=0x%08x\n",
               (unsigned long long)retired, pc, last_pc, last_opcode);
  (void)printf("%s 5 - descriptor v2 serializes pinned identities and seed\n",
               identity_ok ? "ok" : "not ok");
  (void)printf("# expected 32-byte asset/payload/policy hashes and seed version 1; observed lookup status=%u\n",
               lookup_status);
  (void)puts("1..5");
  return stop_setup_ok && first_result.reason == T64_STOP_REASON_BOUND &&
                 repeat_result.reason == T64_STOP_REASON_BOUND &&
                 repeat_result.tick == UINT64_C(100) &&
                 retired == UINT64_C(100) &&
                 pc == UINT32_C(0xa4000354) &&
                 last_pc == UINT32_C(0xa4000350) &&
                 last_opcode == UINT32_C(0x240af000) && !unsupported &&
                 identity_ok
             ? 0
             : 1;
}

static void test_actual_open_ipl3_entry(void) {
  uint8_t image[TEST_ROM_SIZE];
  uint8_t altered[TEST_ROM_SIZE];
  uint8_t state_before_refusal[T64_DIGEST_SIZE];
  uint8_t state_after_refusal[T64_DIGEST_SIZE];
  uint8_t state_after_first_retirement[T64_DIGEST_SIZE];
  uint8_t state_before_reversed[T64_DIGEST_SIZE];
  uint8_t state_after_reversed[T64_DIGEST_SIZE];
  uint8_t expected_asset_sha256[T64_SHA256_DIGEST_SIZE];
  uint8_t expected_payload_sha256[T64_SHA256_DIGEST_SIZE];
  uint8_t expected_policy_sha256[T64_SHA256_DIGEST_SIZE];
  t64_trace_record records[T64_TRACE_RECORD_CAPACITY];
  t64_cpu_stop_detail stop_detail = {0u, 0u, 0u};
  const t64_profile_descriptor *profile = NULL;
  allocator_state allocator_state_value = {0};
  const t64_host_allocator allocator = {
      test_allocate, test_release, &allocator_state_value};
  t64_machine *machine = NULL;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(1)};
  t64_stop_result result = {0u, 0u};
  t64_status status;
  size_t count = 0u;
  uint64_t dropped = 0u;
  bool saturated = false;
  bool entry_seen = false;
  bool first_instruction_seen = false;
  bool unsupported_seen = false;
  uint64_t trace_bound;
  size_t i;

  {
    const bool read_ok = read_pinned_ipl3(image);
    T64_CHECK(read_ok);
    if (!read_ok) {
      return;
    }
  }
  T64_CHECK(bytes_match_sha256(image, sizeof image, OPEN_IPL3_ASSET_SHA256));
  T64_CHECK(bytes_match_sha256(image + 0x40u, sizeof image - 0x40u,
                               OPEN_IPL3_PAYLOAD_SHA256));
  T64_CHECK_INT(t64_profile_lookup(OPEN_IPL3_PROFILE_ID, &profile),
                T64_STATUS_OK);
  T64_CHECK(profile != NULL);
  if (profile == NULL) {
    return;
  }
  T64_CHECK_STR(profile->profile_id, OPEN_IPL3_PROFILE_ID);
  T64_CHECK_INT(profile->descriptor_version, 2u);
  T64_CHECK_INT(profile->machine_model_version, 2u);
  T64_CHECK_INT(profile->state_serialization_version, 6u);
  T64_CHECK_STR(profile->boot_status, "prepared-warm-entry-partial");
  T64_CHECK(decode_sha256_hex(OPEN_IPL3_ASSET_SHA256,
                              expected_asset_sha256));
  T64_CHECK(decode_sha256_hex(OPEN_IPL3_PAYLOAD_SHA256,
                              expected_payload_sha256));
  T64_CHECK(decode_sha256_hex(
      "831a0a58ef8f82bc54f880ad5a1259bddf1b3d2e21680ac6637ea5994705a7fe",
      expected_policy_sha256));
  T64_CHECK(memcmp(profile->asset_sha256, expected_asset_sha256,
                   sizeof expected_asset_sha256) == 0);
  T64_CHECK(memcmp(profile->payload_sha256, expected_payload_sha256,
                   sizeof expected_payload_sha256) == 0);
  T64_CHECK(memcmp(profile->boot_policy_sha256, expected_policy_sha256,
                   sizeof expected_policy_sha256) == 0);
  T64_CHECK_INT(profile->hle_seed_reset_version, 1u);

  status = t64_machine_create(&allocator, profile, &machine);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK(machine != NULL);
  if (status != T64_STATUS_OK || machine == NULL) {
    return;
  }
  T64_CHECK_INT(t64_machine_trace_set_enabled(machine, true), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_admit_rom(machine, image, sizeof image),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(machine, state_before_refusal),
                T64_STATUS_OK);
  (void)memcpy(altered, image, sizeof altered);
  altered[0x100u] ^= 0x01u;
  T64_CHECK_INT(t64_machine_admit_rom(machine, altered, sizeof altered),
                T64_STATUS_INVALID_ROM);
  T64_CHECK_INT(t64_machine_digest(machine, state_after_refusal),
                T64_STATUS_OK);
  T64_CHECK(memcmp(state_before_refusal, state_after_refusal,
                   sizeof state_before_refusal) == 0);

  T64_CHECK_INT(t64_machine_advance(machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, UINT64_C(1));
  T64_CHECK_INT(t64_machine_digest(machine, state_after_first_retirement),
                T64_STATUS_OK);
  T64_CHECK(memcmp(state_before_refusal, state_after_first_retirement,
                   sizeof state_before_refusal) != 0);
  T64_CHECK_INT(t64_machine_trace_drain(machine, records,
                                        sizeof records / sizeof records[0],
                                        &count, &dropped, &saturated),
                T64_STATUS_OK);
  for (i = 0u; i < count; i++) {
    if (records[i].device != T64_TRACE_DEVICE_MACHINE) {
      continue;
    }
    if (records[i].kind == TEST_TRACE_KIND_CPU_ENTRY &&
        records[i].value == UINT64_C(0xa4000040)) {
      entry_seen = true;
    }
    if (records[i].kind == TEST_TRACE_KIND_CPU_RETIREMENT &&
        records[i].value == UINT64_C(0xa40000403044d236)) {
      first_instruction_seen = true;
    }
  }
  T64_CHECK(entry_seen);
  T64_CHECK(first_instruction_seen);
  T64_CHECK_INT(dropped, 0u);
  T64_CHECK(!saturated);

  request.value = UINT64_C(2);
  T64_CHECK_INT(t64_machine_advance(machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, UINT64_C(2));
  T64_CHECK_INT(t64_machine_trace_drain(machine, records,
                                        sizeof records / sizeof records[0],
                                        &count, &dropped, &saturated),
                T64_STATUS_OK);
  T64_CHECK_INT(dropped, 0u);
  T64_CHECK(!saturated);
  for (trace_bound = UINT64_C(16); trace_bound <= UINT64_C(100);
       trace_bound += UINT64_C(16)) {
    request.value = trace_bound > UINT64_C(100) ? UINT64_C(100) : trace_bound;
    T64_CHECK_INT(t64_machine_advance(machine, &request, &result),
                  T64_STATUS_OK);
    T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
    T64_CHECK_INT(result.tick, request.value);
    T64_CHECK_INT(t64_machine_trace_drain(
                      machine, records, sizeof records / sizeof records[0],
                      &count, &dropped, &saturated),
                  T64_STATUS_OK);
    T64_CHECK_INT(dropped, 0u);
    T64_CHECK(!saturated);
    for (i = 0u; i < count; i++) {
      if (records[i].device == T64_TRACE_DEVICE_MACHINE &&
          records[i].kind == TEST_TRACE_KIND_CPU_UNSUPPORTED) {
        unsupported_seen = true;
      }
    }
    if (trace_bound >= UINT64_C(100)) {
      break;
    }
  }
  if (result.tick < UINT64_C(100)) {
    request.value = UINT64_C(100);
    T64_CHECK_INT(t64_machine_advance(machine, &request, &result),
                  T64_STATUS_OK);
    T64_CHECK_INT(t64_machine_trace_drain(
                      machine, records, sizeof records / sizeof records[0],
                      &count, &dropped, &saturated),
                  T64_STATUS_OK);
    T64_CHECK_INT(dropped, 0u);
    T64_CHECK(!saturated);
    for (i = 0u; i < count; i++) {
      if (records[i].device == T64_TRACE_DEVICE_MACHINE &&
          records[i].kind == TEST_TRACE_KIND_CPU_UNSUPPORTED) {
        unsupported_seen = true;
      }
    }
  }
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, UINT64_C(100));
  T64_CHECK_INT(machine->cpu.retired_instructions, UINT64_C(100));
  T64_CHECK_INT(machine->cpu.pc, UINT32_C(0xa4000354));
  T64_CHECK_INT(machine->cpu.last_pc, UINT32_C(0xa4000350));
  T64_CHECK_INT(machine->cpu.last_opcode, UINT32_C(0x240af000));
  T64_CHECK_INT(t64_machine_cpu_stop_detail(machine, &stop_detail),
                T64_STATUS_NOT_READY);
  T64_CHECK(!unsupported_seen);

  T64_CHECK_INT(t64_machine_digest(machine, state_before_reversed),
                T64_STATUS_OK);
  request.value = UINT64_C(100);
  T64_CHECK_INT(t64_machine_advance(machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, UINT64_C(100));
  T64_CHECK_INT(t64_machine_digest(machine, state_after_reversed),
                T64_STATUS_OK);
  T64_CHECK(memcmp(state_before_reversed, state_after_reversed,
                   sizeof state_before_reversed) == 0);

  T64_CHECK_INT(t64_machine_digest(machine, state_before_reversed),
                T64_STATUS_OK);
  request.value = UINT64_C(1);
  T64_CHECK_INT(t64_machine_advance(machine, &request, &result),
                T64_STATUS_TIME_REVERSED);
  T64_CHECK_INT(t64_machine_digest(machine, state_after_reversed),
                T64_STATUS_OK);
  T64_CHECK(memcmp(state_before_reversed, state_after_reversed,
                   sizeof state_before_reversed) == 0);

  (void)printf("profile=%s asset_sha256=%s payload_sha256=%s\n",
               profile->profile_id, OPEN_IPL3_ASSET_SHA256,
               OPEN_IPL3_PAYLOAD_SHA256);
  (void)printf("entry_pc=0xa4000040 retired_opcode=0x3044d236 ");
  (void)printf("tick_bound=100 next_pc=0xa4000354 last_pc=0xa4000350 ");
  (void)printf("last_opcode=0x240af000 retired=100 evidence=bounded real CPU path\n");
  t64_machine_destroy(machine);
  T64_CHECK_INT(allocator_state_value.outstanding_count, 0u);
  T64_CHECK_INT(allocator_state_value.release_errors, 0u);
}

static void test_openipl3_instruction_count_boundary(void) {
  uint8_t image[TEST_ROM_SIZE];
  const t64_profile_descriptor *profile = NULL;
  allocator_state allocator_state_value = {0};
  const t64_host_allocator allocator = {
      test_allocate, test_release, &allocator_state_value};
  t64_machine *machine = NULL;
  t64_stop_request request = {T64_STOP_INSTRUCTION_COUNT, UINT64_C(1)};
  t64_stop_result result = {0u, 0u};
  t64_cpu_stop_detail detail = {UINT64_C(99), UINT32_C(99), UINT32_C(99)};
  t64_status status;
  bool read_ok = read_pinned_ipl3(image);

  T64_CHECK(read_ok);
  if (!read_ok) {
    return;
  }
  T64_CHECK_INT(t64_profile_lookup(OPEN_IPL3_PROFILE_ID, &profile),
                T64_STATUS_OK);
  if (profile == NULL) {
    return;
  }
  status = t64_machine_create(&allocator, profile, &machine);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK || machine == NULL) {
    return;
  }
  T64_CHECK_INT(t64_machine_admit_rom(machine, image, sizeof image),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_advance(machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_INSTRUCTION_COUNT);
  T64_CHECK_INT(result.tick, UINT64_C(1));
  T64_CHECK_INT(t64_machine_cpu_stop_detail(machine, &detail),
                T64_STATUS_NOT_READY);
  T64_CHECK_INT(detail.retired_instructions, UINT64_C(99));
  T64_CHECK_INT(detail.pc, UINT32_C(99));
  T64_CHECK_INT(detail.opcode, UINT32_C(99));

  request.value = UINT64_C(2);
  T64_CHECK_INT(t64_machine_advance(machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_INSTRUCTION_COUNT);
  T64_CHECK_INT(result.tick, UINT64_C(2));
  T64_CHECK_INT(t64_machine_cpu_stop_detail(machine, &detail),
                T64_STATUS_NOT_READY);
  T64_CHECK_INT(detail.retired_instructions, UINT64_C(99));
  T64_CHECK_INT(detail.pc, UINT32_C(99));
  T64_CHECK_INT(detail.opcode, UINT32_C(99));

  request.value = UINT64_C(100);
  T64_CHECK_INT(t64_machine_advance(machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_INSTRUCTION_COUNT);
  T64_CHECK_INT(result.tick, UINT64_C(100));
  T64_CHECK_INT(machine->cpu.retired_instructions, UINT64_C(100));
  T64_CHECK_INT(t64_machine_cpu_stop_detail(machine, &detail),
                T64_STATUS_NOT_READY);
  t64_machine_destroy(machine);
  T64_CHECK_INT(allocator_state_value.outstanding_count, 0u);
  T64_CHECK_INT(allocator_state_value.release_errors, 0u);
}

static bool digest_is_nonzero(const uint8_t digest[T64_DIGEST_SIZE]) {
  size_t i;

  for (i = 0u; i < T64_DIGEST_SIZE; i++) {
    if (digest[i] != 0u) {
      return true;
    }
  }
  return false;
}

static bool allocator_states_are_disjoint(const allocator_state *left,
                                          const allocator_state *right) {
  size_t i, j;

  for (i = 0u; i < left->allocation_count; i++) {
    uintptr_t left_begin;
    uintptr_t left_end;

    if (left->live[i] == NULL) {
      continue;
    }
    left_begin = (uintptr_t)left->live[i];
    left_end = left_begin + left->capacity[i];
    for (j = 0u; j < right->allocation_count; j++) {
      uintptr_t right_begin;
      uintptr_t right_end;

      if (right->live[j] == NULL) {
        continue;
      }
      right_begin = (uintptr_t)right->live[j];
      right_end = right_begin + right->capacity[j];
      if (left_begin < right_end && right_begin < left_end) {
        return false;
      }
    }
  }
  return true;
}

static void test_machine_tracer(void) {
  static const char default_profile_id[] = "ntsc-cart-4m-skeleton";
  uint8_t image_a[TEST_ROM_SIZE];
  uint8_t image_b[TEST_ROM_SIZE];
  uint8_t image_bad_order[TEST_ROM_SIZE];
  uint8_t profile_digest_a[T64_DIGEST_SIZE];
  uint8_t profile_digest_b[T64_DIGEST_SIZE];
  uint8_t content_digest_a[T64_DIGEST_SIZE];
  uint8_t content_digest_after_mutation[T64_DIGEST_SIZE];
  uint8_t state_digest_before_refusal[T64_DIGEST_SIZE];
  uint8_t state_digest_after_refusal[T64_DIGEST_SIZE];
  uint8_t state_digest_a[T64_DIGEST_SIZE];
  uint8_t state_digest_b[T64_DIGEST_SIZE];
  const t64_profile_descriptor *profile = NULL;
  allocator_state alloc_a = {0};
  allocator_state alloc_b = {0};
  const t64_host_allocator host_a = {test_allocate, test_release, &alloc_a};
  const t64_host_allocator host_b = {test_allocate, test_release, &alloc_b};
  t64_machine *machine_a = NULL;
  t64_machine *machine_b = NULL;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(9)};
  t64_stop_result result = {0u, 0u};
  t64_status status;

  fill_authored_image(image_a);
  fill_authored_image(image_b);
  fill_authored_image(image_bad_order);
  image_bad_order[0] = 0xdeu;
  image_bad_order[1] = 0xadu;
  image_bad_order[2] = 0xbeu;
  image_bad_order[3] = 0xefu;
  T64_CHECK_INT(t64_profile_lookup(default_profile_id, &profile),
                T64_STATUS_OK);
  T64_CHECK(profile != NULL);
  if (profile == NULL) {
    return;
  }
  T64_CHECK_INT(t64_profile_hash(profile, profile_digest_a), T64_STATUS_OK);
  T64_CHECK_INT(t64_profile_hash(profile, profile_digest_b), T64_STATUS_OK);
  T64_CHECK(memcmp(profile_digest_a, profile_digest_b, T64_DIGEST_SIZE) == 0);
  T64_CHECK(digest_is_nonzero(profile_digest_a));

  status = t64_machine_create(&host_a, profile, &machine_a);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK(machine_a != NULL);
  if (status != T64_STATUS_OK || machine_a == NULL) {
    return;
  }
  T64_CHECK_INT(t64_machine_admit_rom(machine_a, image_a, sizeof image_a),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_content_digest(machine_a, content_digest_a),
                T64_STATUS_OK);
  T64_CHECK(digest_is_nonzero(content_digest_a));
  T64_CHECK_INT(t64_machine_digest(machine_a, state_digest_before_refusal),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_admit_rom(machine_a, image_bad_order,
                                      sizeof image_bad_order),
                T64_STATUS_INVALID_ROM);
  T64_CHECK_INT(t64_machine_admit_rom(machine_a, image_bad_order,
                                      sizeof image_bad_order - 1u),
                T64_STATUS_INVALID_ROM);
  {
    uint8_t unchanged_digest[T64_DIGEST_SIZE];
    T64_CHECK_INT(t64_machine_content_digest(machine_a, unchanged_digest),
                  T64_STATUS_OK);
    T64_CHECK(memcmp(content_digest_a, unchanged_digest,
                     T64_DIGEST_SIZE) == 0);
  }
  T64_CHECK_INT(t64_machine_digest(machine_a, state_digest_after_refusal),
                T64_STATUS_OK);
  T64_CHECK(memcmp(state_digest_before_refusal, state_digest_after_refusal,
                   T64_DIGEST_SIZE) == 0);
  image_a[0x100u] ^= 0xffu;
  T64_CHECK_INT(t64_machine_content_digest(
                    machine_a, content_digest_after_mutation),
                T64_STATUS_OK);
  T64_CHECK(memcmp(content_digest_a, content_digest_after_mutation,
                   T64_DIGEST_SIZE) == 0);
  T64_CHECK_INT(t64_machine_advance(machine_a, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, UINT64_C(9));
  T64_CHECK_INT(t64_machine_digest(machine_a, state_digest_a),
                T64_STATUS_OK);
  T64_CHECK(digest_is_nonzero(state_digest_a));

  status = t64_machine_create(&host_b, profile, &machine_b);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK(machine_b != NULL);
  if (status == T64_STATUS_OK && machine_b != NULL) {
    T64_CHECK_INT(t64_machine_admit_rom(machine_b, image_b, sizeof image_b),
                  T64_STATUS_OK);
    T64_CHECK_INT(t64_machine_advance(machine_b, &request, &result),
                  T64_STATUS_OK);
    T64_CHECK_INT(t64_machine_digest(machine_b, state_digest_b),
                  T64_STATUS_OK);
    T64_CHECK(memcmp(state_digest_a, state_digest_b, T64_DIGEST_SIZE) == 0);
    T64_CHECK(allocator_states_are_disjoint(&alloc_a, &alloc_b));
  }

  (void)printf("profile=%s profile_sha256=", profile->profile_id);
  print_digest(profile_digest_a);
  (void)printf(" content_sha256=");
  print_digest(content_digest_a);
  (void)printf(" state_sha256=");
  print_digest(state_digest_a);
  (void)printf(" build_id=%s source_revision=%s", t64_build_identity(),
               t64_source_revision());
  (void)printf(" evidence=unit-tested synthetic counters; no guest execution\n");

  t64_machine_destroy(machine_b);
  t64_machine_destroy(machine_a);
  T64_CHECK_INT(alloc_a.allocation_count, 2u);
  T64_CHECK_INT(alloc_a.release_count, 2u);
  T64_CHECK_INT(alloc_a.outstanding_count, 0u);
  T64_CHECK_INT(alloc_a.release_errors, 0u);
  T64_CHECK_INT(alloc_b.allocation_count, 2u);
  T64_CHECK_INT(alloc_b.release_count, 2u);
  T64_CHECK_INT(alloc_b.outstanding_count, 0u);
  T64_CHECK_INT(alloc_b.release_errors, 0u);
}

static void test_allocator_storage_validation(void) {
  const t64_profile_descriptor *profile = NULL;
  allocator_state short_storage = {0};
  allocator_state misreported_alignment = {0};
  const t64_host_allocator short_allocator = {
      test_allocate, test_release, &short_storage};
  const t64_host_allocator alignment_allocator = {
      test_allocate, test_release, &misreported_alignment};
  t64_machine *machine = NULL;

  T64_CHECK_INT(t64_profile_lookup("ntsc-cart-4m-skeleton", &profile),
                T64_STATUS_OK);
  if (profile == NULL) {
    return;
  }
  short_storage.reported_capacity = 1u;
  T64_CHECK_INT(t64_machine_create(&short_allocator, profile, &machine),
                T64_STATUS_BAD_STORAGE);
  T64_CHECK(machine == NULL);
  T64_CHECK_INT(short_storage.allocation_count, 1u);
  T64_CHECK_INT(short_storage.release_count, 1u);
  T64_CHECK_INT(short_storage.outstanding_count, 0u);

  misreported_alignment.reported_alignment = 1u;
  T64_CHECK_INT(t64_machine_create(&alignment_allocator, profile, &machine),
                T64_STATUS_BAD_STORAGE);
  T64_CHECK(machine == NULL);
  T64_CHECK_INT(misreported_alignment.allocation_count, 1u);
  T64_CHECK_INT(misreported_alignment.release_count, 1u);
  T64_CHECK_INT(misreported_alignment.outstanding_count, 0u);
}

int main(void) {
  const char *tap_mode = getenv("T64_TDD_TAP");

  if (tap_mode != NULL && strcmp(tap_mode, "profile") == 0) {
    return tap_open_ipl3_profile_lookup();
  }
  if (tap_mode != NULL && strcmp(tap_mode, "cpu-stop") == 0) {
    return tap_openipl3_public_stop_and_identity();
  }
  test_actual_open_ipl3_entry();
  test_openipl3_instruction_count_boundary();
  test_machine_tracer();
  test_allocator_storage_validation();
  return T64_TEST_FINISH("unit.core.tracer");
}
