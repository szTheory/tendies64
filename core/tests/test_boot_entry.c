#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../t64_internal.h"
#include "../../tools/common/t64_sha256.h"
#include "t64_test.h"
#include "tendies64/t64_machine.h"

#define IPL3_ASSET_BYTES 4096u
#define IPL3_PAYLOAD_BYTES 4032u
#define IPL3_PAYLOAD_SOURCE_OFFSET 0x40u
#define IPL3_DMEM_OFFSET 0x40u
#define IPL3_ENTRY_PC UINT32_C(0xa4000040)

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
    free(storage.data);
    state->outstanding--;
  }
}

static bool read_qualified_asset(uint8_t bytes[IPL3_ASSET_BYTES]) {
  const char *path = getenv("T64_TEST_IPL3_PATH");
  FILE *file;
  size_t length;
  int extra;
  int read_error;
  int close_error;

  if (path == NULL || path[0] == '\0' || bytes == NULL) {
    return false;
  }
  file = fopen(path, "rb");
  if (file == NULL) {
    return false;
  }
  length = fread(bytes, 1u, IPL3_ASSET_BYTES, file);
  extra = fgetc(file);
  read_error = ferror(file);
  close_error = fclose(file);
  if (read_error != 0 || close_error != 0) {
    return false;
  }
  return length == IPL3_ASSET_BYTES && extra == EOF;
}

static void digest_to_hex(const uint8_t digest[T64_DIGEST_SIZE],
                          char hex[T64_DIGEST_SIZE * 2u + 1u]) {
  static const char digits[] = "0123456789abcdef";
  size_t i;

  for (i = 0u; i < T64_DIGEST_SIZE; i++) {
    hex[i * 2u] = digits[digest[i] >> 4];
    hex[i * 2u + 1u] = digits[digest[i] & 0x0fu];
  }
  hex[T64_DIGEST_SIZE * 2u] = '\0';
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
                              uint8_t digest[T64_DIGEST_SIZE]) {
  size_t i;

  if (hex == NULL || digest == NULL || strlen(hex) != T64_DIGEST_SIZE * 2u) {
    return false;
  }
  for (i = 0u; i < T64_DIGEST_SIZE; i++) {
    int high = hex_nibble(hex[i * 2u]);
    int low = hex_nibble(hex[i * 2u + 1u]);

    if (high < 0 || low < 0) {
      return false;
    }
    digest[i] = (uint8_t)(((uint32_t)high << 4) | (uint32_t)low);
  }
  return true;
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

int main(void) {
  static const char expected_profile_sha256[] =
      "f67e9fb4ad2c065e9ec4dc3048f447a8755255a281c602d2ac8c6cc5bcf38faa";
  static const char expected_state_sha256[] =
      "21217cbc347896be9e97f7edcddd912a2fe4c9b0f151dd2e3809887477b6b42d";
  static const char expected_asset_sha256[] =
      "f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068";
  static const char expected_payload_sha256[] =
      "f716a90b1d36ec4253d096790791256d3543455e54473a755ff8c3d1199b15d5";
  static const char expected_policy_sha256[] =
      "a6eeba466ae8804a8dec79ce8c5dece40ea5d25b29724d53f7e1ebe46ffd5cc9";
  static const uint8_t zero_dmem_prefix[IPL3_DMEM_OFFSET] = {0u};
  const t64_profile_descriptor *profile = NULL;
  allocator_state allocator_state_value = {0u};
  const t64_host_allocator allocator = {
      test_allocate, test_release, &allocator_state_value};
  t64_profile_descriptor altered_profile;
  t64_machine *machine = NULL;
  t64_machine *rejected_machine = NULL;
  uint8_t asset[IPL3_ASSET_BYTES];
  uint8_t altered_asset[IPL3_ASSET_BYTES];
  uint8_t digest[T64_DIGEST_SIZE];
  uint8_t expected_asset_digest[T64_DIGEST_SIZE];
  uint8_t expected_payload_digest[T64_DIGEST_SIZE];
  uint8_t expected_policy_digest[T64_DIGEST_SIZE];
  char profile_hex[T64_DIGEST_SIZE * 2u + 1u];
  char state_hex[T64_DIGEST_SIZE * 2u + 1u];
  size_t observed_rom_size = SIZE_MAX;
  t64_status status;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(10)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_stop_result repeated_result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};
  bool fixture_ok;

  fixture_ok = read_qualified_asset(asset);
  T64_CHECK(fixture_ok);
  if (!fixture_ok) {
    goto done;
  }
  T64_CHECK(bytes_match_sha256(asset, sizeof asset, expected_asset_sha256));
  T64_CHECK(bytes_match_sha256(asset + IPL3_PAYLOAD_SOURCE_OFFSET,
                               IPL3_PAYLOAD_BYTES,
                               expected_payload_sha256));

  status = t64_profile_lookup("ntsc-cart-4m-openipl3-warm-v1", &profile);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK(profile != NULL);
  if (profile == NULL) {
    goto done;
  }
  T64_CHECK_STR(profile->profile_id, "ntsc-cart-4m-openipl3-warm-v1");
  T64_CHECK_STR(profile->console_target, "NUS-CPU-03");
  T64_CHECK_STR(profile->console_qualification, "unqualified-target");
  T64_CHECK_STR(profile->boot_status, "prepared-warm-entry-partial");
  T64_CHECK_INT(profile->tv_standard, 1u);
  T64_CHECK_INT(profile->region, 1u);
  T64_CHECK_INT(profile->media_kind, 1u);
  T64_CHECK_INT(profile->rdram_bytes, 4194304u);
  T64_CHECK_INT(profile->expansion_bytes, 0u);
  T64_CHECK_INT(profile->peripheral_flags, 0u);
  T64_CHECK_INT(profile->save_kind, 0u);
  T64_CHECK_INT(profile->rom_min_bytes, IPL3_ASSET_BYTES);
  T64_CHECK_INT(profile->rom_max_bytes, IPL3_ASSET_BYTES);
  T64_CHECK_INT(profile->descriptor_version, 2u);
  T64_CHECK_INT(profile->machine_model_version, 2u);
  T64_CHECK_INT(profile->state_serialization_version, 6u);
  T64_CHECK_INT(profile->hle_seed_reset_version, 1u);
  T64_CHECK(decode_sha256_hex(expected_asset_sha256, expected_asset_digest));
  T64_CHECK(decode_sha256_hex(expected_payload_sha256,
                              expected_payload_digest));
  T64_CHECK(decode_sha256_hex(expected_policy_sha256,
                              expected_policy_digest));
  T64_CHECK_INT(t64_profile_hash(profile, digest), T64_STATUS_OK);
  digest_to_hex(digest, profile_hex);
  T64_CHECK_STR(profile_hex, expected_profile_sha256);

  altered_profile = *profile;
  altered_profile.boot_policy_sha256[0] ^= UINT8_C(1);
  status = t64_machine_create(&allocator, &altered_profile,
                              &rejected_machine);
  T64_CHECK_INT(status, T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK(rejected_machine == NULL);
  T64_CHECK_INT(allocator_state_value.outstanding, 0u);

  status = t64_machine_create(&allocator, profile, &machine);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK(machine != NULL);
  if (machine == NULL) {
    goto done;
  }
  (void)memcpy(altered_asset, asset, sizeof altered_asset);
  altered_asset[0x100u] ^= UINT8_C(1);
  T64_CHECK_INT(t64_machine_admit_rom(machine, altered_asset,
                                      sizeof altered_asset),
                T64_STATUS_INVALID_ROM);
  T64_CHECK(!machine->has_image);
  T64_CHECK(!machine->boot.prepared);
  T64_CHECK_INT(machine->queue_count, 0u);
  T64_CHECK_INT(t64_machine_rom_size(machine, &observed_rom_size),
                T64_STATUS_NOT_READY);
  T64_CHECK_INT(observed_rom_size, SIZE_MAX);

  T64_CHECK_INT(t64_machine_admit_rom(machine, asset, sizeof asset),
                T64_STATUS_OK);
  T64_CHECK(machine->boot.prepared);
  T64_CHECK_INT(machine->boot.source_offset, IPL3_PAYLOAD_SOURCE_OFFSET);
  T64_CHECK_INT(machine->boot.payload_length, IPL3_PAYLOAD_BYTES);
  T64_CHECK_INT(machine->boot.dmem_offset, IPL3_DMEM_OFFSET);
  T64_CHECK(memcmp(machine->sp_dmem, zero_dmem_prefix,
                   sizeof zero_dmem_prefix) == 0);
  T64_CHECK_INT(machine->boot.entry_pc, IPL3_ENTRY_PC);
  T64_CHECK_INT(machine->cpu.pc, IPL3_ENTRY_PC);
  T64_CHECK_INT(machine->cpu.gpr[19], 0u);
  T64_CHECK_INT(machine->cpu.gpr[20], 1u);
  T64_CHECK_INT(machine->cpu.gpr[21], 1u);
  T64_CHECK_INT(machine->cpu.gpr[22], 0u);
  T64_CHECK_INT(machine->cpu.gpr[23], 1u);
  T64_CHECK_INT(machine->pi.next_token, 1u);
  T64_CHECK_INT(machine->sp.next_token, 1u);
  T64_CHECK(memcmp(machine->sp_dmem + IPL3_DMEM_OFFSET,
                   asset + IPL3_PAYLOAD_SOURCE_OFFSET,
                   IPL3_PAYLOAD_BYTES) == 0);
  T64_CHECK(memcmp(machine->boot.asset_sha256, expected_asset_digest,
                   sizeof expected_asset_digest) == 0);
  T64_CHECK(memcmp(machine->boot.payload_sha256, expected_payload_digest,
                   sizeof expected_payload_digest) == 0);
  T64_CHECK(memcmp(machine->boot.policy_sha256, expected_policy_digest,
                   sizeof expected_policy_digest) == 0);

  T64_CHECK_INT(t64_machine_digest(machine, digest), T64_STATUS_OK);
  digest_to_hex(digest, state_hex);
  T64_CHECK_STR(state_hex, expected_state_sha256);

  T64_CHECK_INT(t64_machine_advance(machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_CPU_UNSUPPORTED);
  T64_CHECK_INT(result.tick, 10u);
  T64_CHECK_INT(machine->instruction_count, 9u);
  T64_CHECK_INT(machine->cpu.retired_instructions, 9u);
  T64_CHECK_INT(t64_machine_cpu_stop_detail(machine, &detail), T64_STATUS_OK);
  T64_CHECK_INT(detail.retired_instructions, 9u);
  T64_CHECK_INT(detail.pc, UINT32_C(0xa4000064));
  T64_CHECK_INT(detail.opcode, UINT32_C(0x40044800));
  request.value = 10u;
  T64_CHECK_INT(t64_machine_advance(machine, &request, &repeated_result),
                T64_STATUS_OK);
  T64_CHECK_INT(repeated_result.reason, T64_STOP_REASON_CPU_UNSUPPORTED);
  T64_CHECK_INT(repeated_result.tick, 10u);

done:
  t64_machine_destroy(machine);
  t64_machine_destroy(rejected_machine);
  T64_CHECK_INT(allocator_state_value.outstanding, 0u);
  puts("TAP version 13");
  (void)printf("%s 1 - qualified warm entry identity and partial execution\n",
               t64_test_failures == 0 ? "ok" : "not ok");
  puts("1..1");
  return t64_test_failures == 0 ? 0 : 1;
}
