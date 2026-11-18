#line 1 "core/tests/test_profile.c"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_test.h"
#include "tendies64/t64_machine.h"

#define TEST_ROM_SIZE 4096u
#define MAX_TRACKED_ALLOCATIONS 8u

typedef struct {
  void *live[MAX_TRACKED_ALLOCATIONS];
  size_t capacity[MAX_TRACKED_ALLOCATIONS];
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
  for (i = 0u; i < state->allocation_count; i++) {
    if (state->live[i] != NULL) {
      uintptr_t new_begin = (uintptr_t)memory;
      uintptr_t new_end = new_begin + size;
      uintptr_t old_begin = (uintptr_t)state->live[i];
      uintptr_t old_end = old_begin + state->capacity[i];

      if (new_begin < old_end && old_begin < new_end) {
        free(memory);
        return false;
      }
    }
  }
  state->live[state->allocation_count] = memory;
  state->capacity[state->allocation_count] = size;
  state->allocation_count++;
  state->outstanding_count++;
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  return true;
}

static void test_release(void *context, t64_host_storage storage) {
  allocator_state *state = (allocator_state *)context;
  size_t i;

  if (state == NULL) {
    return;
  }
  for (i = 0u; i < state->allocation_count; i++) {
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

static void fill_authored_image(uint8_t image[TEST_ROM_SIZE], uint8_t country) {
  size_t i;

  for (i = 0u; i < TEST_ROM_SIZE; i++) {
    image[i] = (uint8_t)((i * 13u + 7u) & 0xffu);
  }
  image[0] = 0x80u;
  image[1] = 0x37u;
  image[2] = 0x12u;
  image[3] = 0x40u;
  image[0x3eu] = country;
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

static bool hex_decode(const char *hex, uint8_t *bytes, size_t byte_count) {
  size_t i;

  for (i = 0u; i < byte_count; i++) {
    unsigned high, low;
    char h = hex[i * 2u];
    char l = hex[i * 2u + 1u];

    if (h >= '0' && h <= '9') {
      high = (unsigned)(h - '0');
    } else if (h >= 'a' && h <= 'f') {
      high = (unsigned)(h - 'a') + 10u;
    } else {
      return false;
    }
    if (l >= '0' && l <= '9') {
      low = (unsigned)(l - '0');
    } else if (l >= 'a' && l <= 'f') {
      low = (unsigned)(l - 'a') + 10u;
    } else {
      return false;
    }
    bytes[i] = (uint8_t)((high << 4) | low);
  }
  return hex[byte_count * 2u] == '\0';
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

static const t64_profile_descriptor serialization_vector = {
    .profile_id = "A",
    .console_target = "B",
    .console_qualification = "C",
    .tv_standard = 1u,
    .region = 2u,
    .media_kind = 3u,
    .rdram_bytes = 4u,
    .expansion_bytes = 5u,
    .peripheral_flags = 6u,
    .save_kind = 7u,
    .boot_route = "D",
    .boot_status = "E",
    .guest_tick_unit = "F",
    .cpu_rsp_ratio_numerator = 8u,
    .cpu_rsp_ratio_denominator = 9u,
    .synchronization_quantum = 10u,
    .timing_tier = "G",
    .scheduler_version = "H",
    .pi_model_version = "I",
    .sp_model_version = "J",
    .rom_min_bytes = 4096u,
    .rom_max_bytes = 8192u,
    .event_queue_capacity = 11u,
    .sequence_width_bits = 12u,
    .advance_event_budget = 13u,
    .pi_duration_base_ticks = 14u,
    .pi_duration_bytes_divisor = 15u,
    .sp_duration_base_rsp_cycles = 16u,
    .sp_duration_bytes_divisor = 17u,
    .rom_order_policy = "K",
    .event_order_policy = "L",
    .queue_exhaustion_policy = "M",
    .sequence_exhaustion_policy = "N",
    .advance_budget_policy = "O",
    .guest_time_bound_policy = "P",
    .halt_precedence_policy = "Q",
    .pi_register_policy = "R",
    .sp_register_policy = "S",
    .device_error_policy = "T",
    .dma_cancel_policy = "U",
    .dma_visibility_policy = "V",
    .descriptor_version = 18u,
    .machine_model_version = 19u,
    .state_serialization_version = 20u,
};

static void test_serialization_golden(void) {
  /* Independently authored canonical bytes for serialization_vector. */
  static const char golden_hex[] =
      "5436345044455343000000010000000141000000014200000001430000000100000002"
      "0000000300000004000000050000000600000007000000014400000001450000000146"
      "00000008000000090000000a000000014700000001480000000149000000014a000010"
      "00000020000000000b0000000c0000000d0000000e0000000f00000010000000110000"
      "00014b000000014c000000014d000000014e000000014f000000015000000001510000"
      "0001520000000153000000015400000001550000000156000000120000001300000014";
  uint8_t serialized[T64_PROFILE_SERIALIZED_MAX_BYTES];
  uint8_t expected[T64_PROFILE_SERIALIZED_MAX_BYTES];
  size_t serialized_length = 0u;
  size_t expected_length = (sizeof golden_hex - 1u) / 2u;
  char hash_hex[T64_DIGEST_SIZE * 2u + 1u];
  uint8_t digest[T64_DIGEST_SIZE];
  t64_status status;

  T64_CHECK(hex_decode(golden_hex, expected, expected_length));
  status = t64_profile_serialize(&serialization_vector, serialized,
                                sizeof serialized, &serialized_length);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(serialized_length, expected_length);
  if (status == T64_STATUS_OK && serialized_length == expected_length) {
    T64_CHECK(memcmp(serialized, expected, expected_length) == 0);
  }
  T64_CHECK_INT(t64_profile_hash(&serialization_vector, digest),
                T64_STATUS_OK);
  digest_to_hex(digest, hash_hex);
  T64_CHECK_STR(hash_hex,
                "690b194461ea06f48ac2ad7fb0073b0fa6e0550d02979f53a7e7b7389ca09856");
}

static void test_profile_lookup_and_equal_hash(
    const t64_profile_descriptor *profile, uint8_t profile_hash[32]) {
  const t64_profile_descriptor *selected = profile;
  t64_profile_descriptor copy = *profile;
  uint8_t copy_hash[T64_DIGEST_SIZE];

  T64_CHECK(profile != NULL);
  if (profile == NULL) {
    return;
  }
  T64_CHECK_STR(profile->profile_id, "ntsc-cart-4m-skeleton");
  T64_CHECK_INT(profile->tv_standard, T64_TV_NTSC);
  T64_CHECK_INT(profile->region, T64_REGION_US);
  T64_CHECK_INT(profile->rdram_bytes, 4194304u);
  T64_CHECK_INT(t64_profile_hash(profile, profile_hash), T64_STATUS_OK);
  T64_CHECK(digest_is_nonzero(profile_hash));
  T64_CHECK_INT(t64_profile_hash(&copy, copy_hash), T64_STATUS_OK);
  T64_CHECK(memcmp(profile_hash, copy_hash, T64_DIGEST_SIZE) == 0);

  T64_CHECK_INT(t64_profile_lookup(NULL, &selected),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK(selected == NULL);
  selected = profile;
  T64_CHECK_INT(t64_profile_lookup("", &selected),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK(selected == NULL);
  selected = profile;
  T64_CHECK_INT(t64_profile_lookup("unknown-profile", &selected),
                T64_STATUS_UNKNOWN_PROFILE);
  T64_CHECK(selected == NULL);
  T64_CHECK_INT(t64_profile_lookup("ntsc-cart-4m-skeleton", &selected),
                T64_STATUS_OK);
  T64_CHECK(selected != NULL);
  T64_CHECK_INT(t64_profile_lookup("ntsc-cart-4m-skeleton", NULL),
                T64_STATUS_INVALID_ARGUMENT);
}

static void test_each_descriptor_field_changes_hash(
    const t64_profile_descriptor *profile,
    const uint8_t original_hash[T64_DIGEST_SIZE]) {
  t64_profile_descriptor changed;
  uint8_t changed_hash[T64_DIGEST_SIZE];
  t64_status status;

#define CHECK_TEXT_FIELD(field)                                               \
  do {                                                                         \
    changed = *profile;                                                        \
    changed.field[0] = changed.field[0] == 'X' ? 'Y' : 'X';                   \
    status = t64_profile_hash(&changed, changed_hash);                         \
    T64_CHECK_INT(status, T64_STATUS_OK);                                      \
    T64_CHECK(memcmp(changed_hash, original_hash, T64_DIGEST_SIZE) != 0);      \
  } while (0)
#define CHECK_NUMERIC_FIELD(field)                                            \
  do {                                                                         \
    changed = *profile;                                                        \
    changed.field++;                                                           \
    status = t64_profile_hash(&changed, changed_hash);                         \
    T64_CHECK_INT(status, T64_STATUS_OK);                                      \
    T64_CHECK(memcmp(changed_hash, original_hash, T64_DIGEST_SIZE) != 0);      \
  } while (0)

  CHECK_TEXT_FIELD(profile_id);
  CHECK_TEXT_FIELD(console_target);
  CHECK_TEXT_FIELD(console_qualification);
  CHECK_TEXT_FIELD(boot_route);
  CHECK_TEXT_FIELD(boot_status);
  CHECK_TEXT_FIELD(guest_tick_unit);
  CHECK_TEXT_FIELD(timing_tier);
  CHECK_TEXT_FIELD(scheduler_version);
  CHECK_TEXT_FIELD(pi_model_version);
  CHECK_TEXT_FIELD(sp_model_version);
  CHECK_TEXT_FIELD(rom_order_policy);
  CHECK_TEXT_FIELD(event_order_policy);
  CHECK_TEXT_FIELD(queue_exhaustion_policy);
  CHECK_TEXT_FIELD(sequence_exhaustion_policy);
  CHECK_TEXT_FIELD(advance_budget_policy);
  CHECK_TEXT_FIELD(guest_time_bound_policy);
  CHECK_TEXT_FIELD(halt_precedence_policy);
  CHECK_TEXT_FIELD(pi_register_policy);
  CHECK_TEXT_FIELD(sp_register_policy);
  CHECK_TEXT_FIELD(device_error_policy);
  CHECK_TEXT_FIELD(dma_cancel_policy);
  CHECK_TEXT_FIELD(dma_visibility_policy);

  CHECK_NUMERIC_FIELD(tv_standard);
  CHECK_NUMERIC_FIELD(region);
  CHECK_NUMERIC_FIELD(media_kind);
  CHECK_NUMERIC_FIELD(rdram_bytes);
  CHECK_NUMERIC_FIELD(expansion_bytes);
  CHECK_NUMERIC_FIELD(peripheral_flags);
  CHECK_NUMERIC_FIELD(save_kind);
  CHECK_NUMERIC_FIELD(cpu_rsp_ratio_numerator);
  CHECK_NUMERIC_FIELD(cpu_rsp_ratio_denominator);
  CHECK_NUMERIC_FIELD(synchronization_quantum);
  CHECK_NUMERIC_FIELD(rom_min_bytes);
  CHECK_NUMERIC_FIELD(rom_max_bytes);
  CHECK_NUMERIC_FIELD(event_queue_capacity);
  CHECK_NUMERIC_FIELD(advance_event_budget);
  CHECK_NUMERIC_FIELD(pi_duration_base_ticks);
  CHECK_NUMERIC_FIELD(pi_duration_bytes_divisor);
  CHECK_NUMERIC_FIELD(sp_duration_base_rsp_cycles);
  CHECK_NUMERIC_FIELD(sp_duration_bytes_divisor);
  CHECK_NUMERIC_FIELD(descriptor_version);
  CHECK_NUMERIC_FIELD(machine_model_version);
  CHECK_NUMERIC_FIELD(state_serialization_version);

  changed = *profile;
  changed.sequence_width_bits--;
  status = t64_profile_hash(&changed, changed_hash);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK(memcmp(changed_hash, original_hash, T64_DIGEST_SIZE) != 0);

#undef CHECK_TEXT_FIELD
#undef CHECK_NUMERIC_FIELD
}

static void test_rejected_descriptors(
    const t64_profile_descriptor *profile) {
  t64_profile_descriptor empty = {0};
  t64_profile_descriptor changed = *profile;
  t64_profile_descriptor valid_mutable = *profile;
  union {
    t64_profile_descriptor profile;
    size_t length;
  } profile_length_alias;
  union {
    t64_profile_descriptor profile;
    uint8_t bytes[T64_PROFILE_SERIALIZED_MAX_BYTES];
  } source_alias;
  union {
    size_t alignment;
    uint8_t bytes[T64_PROFILE_SERIALIZED_MAX_BYTES];
  } overlapping_outputs;
  uint8_t digest[T64_DIGEST_SIZE];
  uint8_t serialized[T64_PROFILE_SERIALIZED_MAX_BYTES];
  uint8_t serialized_before[T64_PROFILE_SERIALIZED_MAX_BYTES];
  uint8_t source_alias_before[T64_PROFILE_SERIALIZED_MAX_BYTES];
  size_t serialized_length = 0u;
  size_t required_length = 0u;

  T64_CHECK_INT(t64_profile_hash(NULL, digest), T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK_INT(t64_profile_hash(&empty, digest), T64_STATUS_INVALID_ARGUMENT);
  changed.pi_model_version[0] = '\0';
  T64_CHECK_INT(t64_profile_hash(&changed, digest), T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK_INT(t64_profile_serialize(profile, NULL, 0u, &required_length),
                T64_STATUS_BUFFER_TOO_SMALL);
  T64_CHECK(required_length > 0u);
  memset(serialized, 0xa5, sizeof serialized);
  T64_CHECK_INT(t64_profile_serialize(profile, serialized,
                                      required_length - 1u,
                                      &serialized_length),
                T64_STATUS_BUFFER_TOO_SMALL);
  T64_CHECK_INT(serialized_length, required_length);
  T64_CHECK(serialized[0] == 0xa5u);

  T64_CHECK_INT(t64_profile_serialize(profile, NULL, 0u, NULL),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK_INT(t64_profile_serialize(
                    profile, NULL, 0u, (size_t *)(uintptr_t)(UINTPTR_MAX - 1u)),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK_INT(t64_profile_serialize(
                    profile, (uint8_t *)(uintptr_t)(UINTPTR_MAX - 4u),
                    required_length, &serialized_length),
                T64_STATUS_INVALID_ARGUMENT);

  T64_CHECK(required_length <= sizeof source_alias.bytes);
  T64_CHECK_INT(t64_profile_serialize(&valid_mutable, serialized,
                                      sizeof serialized, &serialized_length),
                T64_STATUS_OK);
  T64_CHECK_INT(serialized_length, required_length);
  memset(source_alias.bytes, 0x6bu, sizeof source_alias.bytes);
  source_alias.profile = valid_mutable;
  (void)memcpy(source_alias_before, source_alias.bytes,
               sizeof source_alias_before);
  T64_CHECK_INT(t64_profile_serialize(
                    &source_alias.profile, source_alias.bytes,
                    sizeof source_alias.bytes, &serialized_length),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK(memcmp(source_alias.bytes, source_alias_before,
                   sizeof source_alias_before) == 0);
  profile_length_alias.profile = *profile;
  T64_CHECK_INT(t64_profile_serialize(&profile_length_alias.profile, NULL, 0u,
                                      &profile_length_alias.length),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK(memcmp(&profile_length_alias.profile, profile,
                   sizeof *profile) == 0);

  memset(overlapping_outputs.bytes, 0xa5,
         sizeof overlapping_outputs.bytes);
  (void)memcpy(serialized_before, overlapping_outputs.bytes,
               sizeof serialized_before);
  T64_CHECK_INT(t64_profile_serialize(
                    profile, overlapping_outputs.bytes, required_length,
                    (size_t *)(void *)overlapping_outputs.bytes),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK(memcmp(overlapping_outputs.bytes, serialized_before,
                   sizeof serialized_before) == 0);
  T64_CHECK_INT(t64_profile_serialize(
                    profile, overlapping_outputs.bytes, required_length - 1u,
                    (size_t *)(void *)overlapping_outputs.bytes),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK(memcmp(overlapping_outputs.bytes, serialized_before,
                   sizeof serialized_before) == 0);
}

static void test_country_metadata_and_test_contrast(
    const t64_profile_descriptor *profile,
    const uint8_t default_profile_hash[T64_DIGEST_SIZE]) {
  uint8_t image_us[TEST_ROM_SIZE];
  uint8_t image_country_changed[TEST_ROM_SIZE];
  uint8_t profile_hash_default[T64_DIGEST_SIZE];
  uint8_t profile_hash_contrast[T64_DIGEST_SIZE];
  uint8_t content_default[T64_DIGEST_SIZE];
  uint8_t content_contrast[T64_DIGEST_SIZE];
  uint8_t content_country_changed[T64_DIGEST_SIZE];
  uint8_t state_default[T64_DIGEST_SIZE];
  uint8_t state_contrast[T64_DIGEST_SIZE];
  uint8_t state_country_changed[T64_DIGEST_SIZE];
  t64_profile_descriptor contrast = *profile;
  allocator_state alloc = {0};
  const t64_host_allocator host = {test_allocate, test_release, &alloc};
  const t64_profile_descriptor *selected_default = NULL;
  const t64_profile_descriptor *selected_contrast = NULL;
  const t64_profile_descriptor *selected_country_changed = NULL;
  t64_machine *machine_default = NULL;
  t64_machine *machine_contrast = NULL;
  t64_machine *machine_country_changed = NULL;

  memset(contrast.profile_id, 0, sizeof contrast.profile_id);
  memcpy(contrast.profile_id, "test-only-pal-contrast",
         sizeof "test-only-pal-contrast");
  contrast.tv_standard = T64_TV_PAL;
  contrast.region = T64_REGION_EUROPE;
  contrast.synchronization_quantum = 2u;
  fill_authored_image(image_us, (uint8_t)'E');
  fill_authored_image(image_country_changed, (uint8_t)'P');

  T64_CHECK_INT(t64_profile_hash(profile, profile_hash_default), T64_STATUS_OK);
  T64_CHECK(memcmp(profile_hash_default, default_profile_hash,
                   T64_DIGEST_SIZE) == 0);
  T64_CHECK_INT(t64_profile_hash(&contrast, profile_hash_contrast),
                T64_STATUS_OK);
  T64_CHECK(memcmp(profile_hash_default, profile_hash_contrast,
                   T64_DIGEST_SIZE) != 0);

  T64_CHECK_INT(t64_machine_create(&host, profile, &machine_default),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_create(&host, &contrast, &machine_contrast),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_create(&host, profile, &machine_country_changed),
                T64_STATUS_OK);
  if (machine_default == NULL || machine_contrast == NULL ||
      machine_country_changed == NULL) {
    t64_machine_destroy(machine_country_changed);
    t64_machine_destroy(machine_contrast);
    t64_machine_destroy(machine_default);
    return;
  }

  T64_CHECK_INT(t64_machine_admit_rom(machine_default, image_us,
                                      sizeof image_us),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_admit_rom(machine_contrast, image_us,
                                      sizeof image_us),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_admit_rom(machine_country_changed,
                                      image_country_changed,
                                      sizeof image_country_changed),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_profile(machine_default, &selected_default),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_profile(machine_contrast, &selected_contrast),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_profile(machine_country_changed,
                                    &selected_country_changed),
                T64_STATUS_OK);
  T64_CHECK(selected_default != NULL);
  T64_CHECK(selected_contrast != NULL);
  T64_CHECK(selected_country_changed != NULL);
  if (selected_default != NULL && selected_contrast != NULL &&
      selected_country_changed != NULL) {
    T64_CHECK_INT(selected_default->tv_standard, T64_TV_NTSC);
    T64_CHECK_INT(selected_default->region, T64_REGION_US);
    T64_CHECK_INT(selected_contrast->tv_standard, T64_TV_PAL);
    T64_CHECK_INT(selected_contrast->region, T64_REGION_EUROPE);
    T64_CHECK_INT(selected_country_changed->tv_standard, T64_TV_NTSC);
    T64_CHECK_INT(selected_country_changed->region, T64_REGION_US);
  }
  T64_CHECK_INT(t64_machine_content_digest(machine_default, content_default),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_content_digest(machine_contrast, content_contrast),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_content_digest(machine_country_changed,
                                          content_country_changed),
                T64_STATUS_OK);
  T64_CHECK(memcmp(content_default, content_contrast, T64_DIGEST_SIZE) == 0);
  T64_CHECK(memcmp(content_default, content_country_changed,
                   T64_DIGEST_SIZE) != 0);
  T64_CHECK_INT(t64_machine_digest(machine_default, state_default),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(machine_contrast, state_contrast),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(machine_country_changed,
                                   state_country_changed),
                T64_STATUS_OK);
  T64_CHECK(memcmp(state_default, state_contrast, T64_DIGEST_SIZE) != 0);
  T64_CHECK(memcmp(state_default, state_country_changed, T64_DIGEST_SIZE) != 0);

  t64_machine_destroy(machine_country_changed);
  t64_machine_destroy(machine_contrast);
  t64_machine_destroy(machine_default);
  T64_CHECK_INT(alloc.allocation_count, 6u);
  T64_CHECK_INT(alloc.release_count, 6u);
  T64_CHECK_INT(alloc.outstanding_count, 0u);
  T64_CHECK_INT(alloc.release_errors, 0u);
}

static void test_vr4300_profile_contract(void) {
  static const char asset_sha256[] =
      "f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068";
  static const char payload_sha256[] =
      "f716a90b1d36ec4253d096790791256d3543455e54473a755ff8c3d1199b15d5";
  static const char policy_sha256[] =
      "8ff4eb70b307f974982a1a499b3db1cdb2709e50cacaff75fb10666b4c5fca49";
  const t64_profile_descriptor *profile = NULL;
  t64_profile_descriptor changed;
  uint8_t expected_asset[T64_DIGEST_SIZE];
  uint8_t expected_payload[T64_DIGEST_SIZE];
  uint8_t expected_policy[T64_DIGEST_SIZE];
  uint8_t profile_hash[T64_DIGEST_SIZE];
  uint8_t changed_hash[T64_DIGEST_SIZE];
  uint8_t serialized[T64_PROFILE_SERIALIZED_MAX_BYTES];
  size_t serialized_length = 0u;
  size_t metadata_length = (T64_DIGEST_SIZE * 3u) + sizeof(uint32_t);
  char profile_hash_hex[T64_DIGEST_SIZE * 2u + 1u];

  T64_CHECK_INT(
      t64_profile_lookup("ntsc-cart-4m-openipl3-vr4300-v1", &profile),
      T64_STATUS_OK);
  T64_CHECK(profile != NULL);
  if (profile == NULL) {
    return;
  }
  T64_CHECK_INT(profile->descriptor_version, 3u);
  T64_CHECK_INT(profile->machine_model_version, 3u);
  T64_CHECK_INT(profile->state_serialization_version, 9u);
  T64_CHECK_INT(profile->hle_seed_reset_version, 2u);
  T64_CHECK_INT(profile->rom_max_bytes, UINT32_C(0x00401000));
  T64_CHECK_STR(profile->rom_order_policy,
                "exact-source-or-flat-payload-z64-cpu-4m-v1");
  T64_CHECK_STR(profile->boot_route, "hle-pif+open-ipl3");
  T64_CHECK_STR(profile->boot_status, "prepared-vr4300-entry-partial");
  T64_CHECK(hex_decode(asset_sha256, expected_asset, sizeof expected_asset));
  T64_CHECK(hex_decode(payload_sha256, expected_payload,
                       sizeof expected_payload));
  T64_CHECK(hex_decode(policy_sha256, expected_policy, sizeof expected_policy));
  T64_CHECK(memcmp(profile->asset_sha256, expected_asset,
                   sizeof expected_asset) == 0);
  T64_CHECK(memcmp(profile->payload_sha256, expected_payload,
                   sizeof expected_payload) == 0);
  T64_CHECK(memcmp(profile->boot_policy_sha256, expected_policy,
                   sizeof expected_policy) == 0);
  T64_CHECK_INT(t64_profile_serialize(profile, serialized,
                                      sizeof serialized, &serialized_length),
                T64_STATUS_OK);
  T64_CHECK(serialized_length >= metadata_length);
  if (serialized_length >= metadata_length) {
    size_t metadata_offset = serialized_length - metadata_length;

    T64_CHECK(memcmp(serialized + metadata_offset, expected_asset,
                     sizeof expected_asset) == 0);
    T64_CHECK(memcmp(serialized + metadata_offset + T64_DIGEST_SIZE,
                     expected_payload, sizeof expected_payload) == 0);
    T64_CHECK(memcmp(serialized + metadata_offset + (T64_DIGEST_SIZE * 2u),
                     expected_policy, sizeof expected_policy) == 0);
    T64_CHECK_INT(serialized[serialized_length - 4u], 0u);
    T64_CHECK_INT(serialized[serialized_length - 3u], 0u);
    T64_CHECK_INT(serialized[serialized_length - 2u], 0u);
    T64_CHECK_INT(serialized[serialized_length - 1u], 2u);
  }
  T64_CHECK_INT(t64_profile_hash(profile, profile_hash), T64_STATUS_OK);
  digest_to_hex(profile_hash, profile_hash_hex);
  T64_CHECK_STR(profile_hash_hex,
                "f37861d8911ad9d3c0e54b070514f79dffb9ecb3a52f3f345e3aaf0dfc0083c4");

#define CHECK_VR4300_PROFILE_MUTATION(statement)                              \
  do {                                                                         \
    changed = *profile;                                                        \
    statement                                                                 \
    T64_CHECK_INT(t64_profile_hash(&changed, changed_hash), T64_STATUS_OK);    \
    T64_CHECK(memcmp(profile_hash, changed_hash, T64_DIGEST_SIZE) != 0);       \
  } while (0)

  CHECK_VR4300_PROFILE_MUTATION(changed.asset_sha256[0] ^= UINT8_C(1););
  CHECK_VR4300_PROFILE_MUTATION(changed.payload_sha256[31] ^= UINT8_C(1););
  CHECK_VR4300_PROFILE_MUTATION(changed.boot_policy_sha256[0] ^= UINT8_C(1););
  CHECK_VR4300_PROFILE_MUTATION(changed.hle_seed_reset_version++;);
  CHECK_VR4300_PROFILE_MUTATION(changed.descriptor_version++;);
  CHECK_VR4300_PROFILE_MUTATION(changed.machine_model_version++;);
  CHECK_VR4300_PROFILE_MUTATION(changed.state_serialization_version++;);

#undef CHECK_VR4300_PROFILE_MUTATION
}

static void test_vr4300_compatibility_profile_contract(void) {
  static const char expected_compat_hash[] =
      "5955f3556c0653602c7c2da9faaee49dd2a647f0692d46f823ccb152011fa8d7";
  const t64_profile_descriptor *strict_profile = NULL;
  const t64_profile_descriptor *profile = NULL;
  uint8_t profile_hash[T64_DIGEST_SIZE];
  uint8_t strict_hash[T64_DIGEST_SIZE];
  char profile_hash_hex[T64_DIGEST_SIZE * 2u + 1u];
  t64_status status = t64_profile_lookup(
      "ntsc-cart-4m-openipl3-vr4300-compat-v1", &profile);

  if (status != T64_STATUS_OK) {
    (void)fprintf(stderr,
                  "RED observation: compatibility profile lookup expected "
                  "status %u, observed status %u\n",
                  (unsigned)T64_STATUS_OK, (unsigned)status);
  }
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK(profile != NULL);
  if (profile == NULL) {
    return;
  }
  T64_CHECK_STR(profile->profile_id,
                "ntsc-cart-4m-openipl3-vr4300-compat-v1");
  T64_CHECK_INT(profile->descriptor_version, 3u);
  T64_CHECK_INT(profile->machine_model_version, 4u);
  T64_CHECK_INT(profile->state_serialization_version, 10u);
  T64_CHECK_INT(profile->rom_min_bytes, 4096u);
  T64_CHECK_INT(profile->rom_max_bytes, UINT32_C(0x00401000));
  T64_CHECK_STR(profile->rom_order_policy,
                "exact-source-or-flat-payload-z64-cpu-4m-v1");
  T64_CHECK_STR(profile->boot_status,
                "prepared-vr4300-compat-entry-partial");
  T64_CHECK_STR(profile->timing_tier,
                "versioned-vr4300-software-compatibility");
  T64_CHECK_INT(t64_profile_lookup("ntsc-cart-4m-openipl3-vr4300-v1",
                                   &strict_profile),
                T64_STATUS_OK);
  T64_CHECK(strict_profile != NULL);
  if (strict_profile == NULL) {
    return;
  }
  T64_CHECK(memcmp(profile->asset_sha256, strict_profile->asset_sha256,
                   T64_DIGEST_SIZE) == 0);
  T64_CHECK(memcmp(profile->payload_sha256, strict_profile->payload_sha256,
                   T64_DIGEST_SIZE) == 0);
  T64_CHECK(memcmp(profile->boot_policy_sha256,
                   strict_profile->boot_policy_sha256,
                   T64_DIGEST_SIZE) == 0);
  T64_CHECK_INT(profile->hle_seed_reset_version,
                strict_profile->hle_seed_reset_version);
  T64_CHECK_INT(t64_profile_hash(profile, profile_hash), T64_STATUS_OK);
  T64_CHECK_INT(t64_profile_hash(strict_profile, strict_hash), T64_STATUS_OK);
  T64_CHECK(memcmp(profile_hash, strict_hash, sizeof profile_hash) != 0);
  digest_to_hex(profile_hash, profile_hash_hex);
  T64_CHECK_STR(profile_hash_hex, expected_compat_hash);
}

static void test_profile_contract(void) {
  const t64_profile_descriptor *profile = NULL;
  uint8_t profile_hash[T64_DIGEST_SIZE];
  char profile_hash_hex[T64_DIGEST_SIZE * 2u + 1u];

  T64_CHECK_INT(t64_profile_lookup("ntsc-cart-4m-skeleton", &profile),
                T64_STATUS_OK);
  if (profile == NULL) {
    return;
  }
  test_profile_lookup_and_equal_hash(profile, profile_hash);
  test_serialization_golden();
  test_each_descriptor_field_changes_hash(profile, profile_hash);
  test_rejected_descriptors(profile);
  test_country_metadata_and_test_contrast(profile, profile_hash);
  test_vr4300_profile_contract();
  test_vr4300_compatibility_profile_contract();
  digest_to_hex(profile_hash, profile_hash_hex);
  T64_CHECK_STR(profile_hash_hex,
                "b7d1600ad960b4e04fdc3ec372675edc47b26b15be8e9cd84cbab4fd75835647");
  T64_CHECK(strncmp(t64_build_identity(), "t64-build-v1-", 13u) == 0);
  T64_CHECK(strchr(t64_build_identity(), '/') == NULL);
  T64_CHECK(strlen(t64_source_revision()) > 0u);
  (void)printf("profile=%s profile_sha256=%s build_id=%s source_revision=%s\n",
               profile->profile_id, profile_hash_hex, t64_build_identity(),
               t64_source_revision());
  (void)printf("evidence=unit-tested synthetic counters; no guest execution\n");
}

static void test_compat_v2_profile_identity(void) {
  const t64_profile_descriptor *profile = NULL;
  const t64_profile_descriptor *v1 = NULL;
  uint8_t digest[T64_DIGEST_SIZE];
  uint8_t v1_digest[T64_DIGEST_SIZE];
  char digest_hex[T64_DIGEST_SIZE * 2u + 1u];
  t64_status status = t64_profile_lookup(
      "ntsc-cart-4m-openipl3-vr4300-compat-v2", &profile);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK(profile != NULL);
  if (profile == NULL) return;
  T64_CHECK_INT(profile->descriptor_version, 3u);
  T64_CHECK_INT(profile->machine_model_version, 5u);
  T64_CHECK_INT(profile->state_serialization_version, 11u);
  T64_CHECK_INT(t64_profile_lookup(
      "ntsc-cart-4m-openipl3-vr4300-compat-v1", &v1), T64_STATUS_OK);
  if (v1 == NULL) return;
  T64_CHECK_INT(t64_profile_hash(profile, digest), T64_STATUS_OK);
  T64_CHECK_INT(t64_profile_hash(v1, v1_digest), T64_STATUS_OK);
  T64_CHECK(memcmp(digest, v1_digest, sizeof digest) != 0);
  digest_to_hex(digest, digest_hex);
  T64_CHECK_STR(digest_hex,
      "1086516cd466bf48133533a8edf33a12cc8fd9e27dcd2b493e92b2bcf21a7634");
  T64_CHECK_INT(profile->rom_max_bytes, v1->rom_max_bytes);
  T64_CHECK(memcmp(profile->asset_sha256, v1->asset_sha256,
                   T64_DIGEST_SIZE) == 0);
}

static void test_cold_t1_profile_identities(void) {
  static const char *const profile_ids[] = {
      "ntsc-cart-4m-openipl3-rdram-a-t1-v1",
      "ntsc-cart-4m-openipl3-rdram-b-t1-v1",
  };
  size_t index;

  for (index = 0u; index < sizeof profile_ids / sizeof profile_ids[0];
       index++) {
    const t64_profile_descriptor *profile = NULL;
    t64_status status = t64_profile_lookup(profile_ids[index], &profile);

    if (status != T64_STATUS_OK) {
      (void)fprintf(stderr,
                    "RED observation: cold T1 profile %s lookup returned %u\n",
                    profile_ids[index], (unsigned)status);
    }
    T64_CHECK_INT(status, T64_STATUS_OK);
    T64_CHECK(profile != NULL);
    if (profile == NULL) {
      continue;
    }
    T64_CHECK_STR(profile->profile_id, profile_ids[index]);
    T64_CHECK_INT(profile->descriptor_version, 4u);
    T64_CHECK_INT(profile->machine_model_version, 7u + (uint32_t)index);
    T64_CHECK_INT(profile->state_serialization_version, 13u);
  }
}

/* Test the complete modern descriptor contract, including accepted public
 * serialization and known-ID canonical admission, rather than only lookup. */
static void test_rdp_profile_metadata_binding(bool run_mutations) {
  static const char *const ids[] = {
      "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-triangle-v3",
      "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-texture-v4",
      "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-combiner-v5",
      "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-blender-v6",
      "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-depth-v7",
      "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v8",
      "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9",
      "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1",
  };
  /* Earlier-profile bytes are pinned to the retained pre-repair artifact. */
  static const char *const legacy_hashes[] = {
      "794710f767fce5f1bd37d1116a6067bac10026aa4c7d77fc9cfe7c3636d5830f",
      "3ce5b6c75ba85a8b1d5ed333820d70736c26c003ec16c76a7cd9f8ee686f3e18",
      "0e9938d6b92159fe1f8c5dff0c03749979d5d10b11b458224f2dcca1a2345efa",
      "7f73b46e6c695fddb60d89d6351c1c099f56e76dbe109ab9f2cb9f372fc56d0e",
      "56f25e100d3c92792f836b0664e81697919487b4fdae49404476a85135ae5969",
      "4f929a757bc6c257729918ccd92e632256fc5ad991dab2317a272cba1d22dc1d",
      "ebc4370b89cf52d0b2125bac2ab6b40b0fc9f5bef44c8bf9c28647c073514015",
      "eaa400aa23bfa3d0c9ba6d475d8eba7a60302151e22b6278ff6a4991d08bb5bf",
  };
  static const size_t legacy_lengths[] = {1116u, 1132u, 1172u, 1155u,
                                          1231u, 1243u, 1256u, 1319u};
  static const uint32_t descriptor_versions[] = {7u, 7u, 8u, 9u,
                                                  10u, 10u, 10u, 11u};
  static const uint32_t model_versions[] = {12u, 13u, 14u, 15u,
                                             16u, 17u, 18u, 19u};
  static const uint32_t state_versions[] = {17u, 18u, 19u, 20u,
                                             21u, 22u, 23u, 24u};
  static const size_t text_offsets[] = {
      offsetof(t64_profile_descriptor, rdp_work_policy),
      offsetof(t64_profile_descriptor, vi_clock_policy),
      offsetof(t64_profile_descriptor, rdp_noise_policy),
      offsetof(t64_profile_descriptor, rdp_undefined_color_policy),
      offsetof(t64_profile_descriptor, vi_output_policy),
      offsetof(t64_profile_descriptor, rdp_triangle_policy),
      offsetof(t64_profile_descriptor, rdp_combiner_policy),
      offsetof(t64_profile_descriptor, rdp_depth_policy),
      offsetof(t64_profile_descriptor, ai_policy),
  };
  static const size_t boot_offsets[] = {
      offsetof(t64_profile_descriptor, asset_sha256),
      offsetof(t64_profile_descriptor, payload_sha256),
      offsetof(t64_profile_descriptor, boot_policy_sha256),
      offsetof(t64_profile_descriptor, hle_seed_reset_version),
  };
  size_t index;
  for (index = 0u; index < sizeof ids / sizeof ids[0]; index++) {
    const t64_profile_descriptor *profile = NULL;
    uint8_t original_hash[T64_DIGEST_SIZE];
    uint8_t original_bytes[8192];
    size_t original_length = 0u;
    size_t field;
    size_t text_count = index < 2u ? 6u : (index < 4u ? 7u :
                         (index < 7u ? 8u : 9u));
    allocator_state state = {0};
    t64_host_allocator allocator = {test_allocate, test_release, &state};
    T64_CHECK_INT(t64_profile_lookup(ids[index], &profile), T64_STATUS_OK);
    if (profile == NULL) continue;
    T64_CHECK_STR(profile->profile_id, ids[index]);
    T64_CHECK_INT(profile->descriptor_version, descriptor_versions[index]);
    T64_CHECK_INT(profile->machine_model_version, model_versions[index]);
    T64_CHECK_INT(profile->state_serialization_version, state_versions[index]);
    if (index == 6u) {
      T64_CHECK_STR(profile->vi_output_policy,
                    "5551-8888-aa53mdtl-q8f-df3-dvmed-bq20f-gd16r8-sqrt255-pcspan-v3");
    }
    if (index == 7u) {
      T64_CHECK_STR(profile->ai_policy,
                    "fetched-rdram-stereo-s16be-rational-62500000-v1");
    }
    T64_CHECK_INT(t64_profile_hash(profile, original_hash), T64_STATUS_OK);
    T64_CHECK_INT(t64_profile_serialize(profile, original_bytes,
                                        sizeof original_bytes,
                                        &original_length), T64_STATUS_OK);
    {
      t64_machine *canonical_machine = NULL;
      T64_CHECK_INT(t64_machine_create(&allocator, profile,
                                       &canonical_machine), T64_STATUS_OK);
      T64_CHECK(canonical_machine != NULL);
      if (canonical_machine != NULL)
        t64_machine_destroy(canonical_machine);
    }
    T64_CHECK(original_length > 0u);
    {
      char original_hex[T64_DIGEST_SIZE * 2u + 1u];
      digest_to_hex(original_hash, original_hex);
      T64_CHECK_STR(original_hex, legacy_hashes[index]);
      T64_CHECK_INT(original_length, legacy_lengths[index]);
    }
    if (run_mutations) {
    for (field = 0u; field < text_count + 4u; field++) {
      t64_profile_descriptor changed = *profile;
      state = (allocator_state){0};
      uint8_t changed_hash[T64_DIGEST_SIZE];
      uint8_t changed_bytes[8192];
      size_t changed_length = 0u;
      t64_machine *machine = NULL;
      uint8_t *bytes = (uint8_t *)&changed;
      size_t offset = field < text_count ? text_offsets[field]
                                          : boot_offsets[field - text_count];
      bytes[offset] ^= UINT8_C(1);
      T64_CHECK_INT(t64_profile_hash(&changed, changed_hash), T64_STATUS_OK);
      T64_CHECK(memcmp(original_hash, changed_hash, sizeof changed_hash) != 0);
      T64_CHECK_INT(t64_profile_serialize(&changed, changed_bytes,
                                          sizeof changed_bytes,
                                          &changed_length), T64_STATUS_OK);
      T64_CHECK_INT(changed_length, original_length);
      T64_CHECK(memcmp(original_bytes, changed_bytes, original_length) != 0);
      T64_CHECK_INT(t64_machine_create(&allocator, &changed, &machine),
                    T64_STATUS_INVALID_ARGUMENT);
      T64_CHECK(machine == NULL);
      if (machine != NULL) t64_machine_destroy(machine);
    }
    for (field = 0u; field < text_count; field++) {
      unsigned invalid_kind;
      for (invalid_kind = 0u; invalid_kind < 2u; invalid_kind++) {
        t64_profile_descriptor changed = *profile;
        state = (allocator_state){0};
        uint8_t digest[T64_DIGEST_SIZE];
        uint8_t serialized[8192];
        size_t length = 0u;
        t64_machine *machine = NULL;
        char *text = (char *)&changed + text_offsets[field];
        (void)memset(text, invalid_kind == 0u ? 0 : 'x',
                     T64_PROFILE_TEXT_CAPACITY);
        T64_CHECK_INT(t64_profile_hash(&changed, digest),
                      T64_STATUS_INVALID_ARGUMENT);
        T64_CHECK_INT(t64_profile_serialize(&changed, serialized,
                                            sizeof serialized, &length),
                      T64_STATUS_INVALID_ARGUMENT);
        T64_CHECK_INT(t64_machine_create(&allocator, &changed, &machine),
                      T64_STATUS_INVALID_ARGUMENT);
        T64_CHECK(machine == NULL);
        if (machine != NULL) t64_machine_destroy(machine);
      }
    }
    }
    T64_CHECK_INT(state.outstanding_count, 0u);
    T64_CHECK_INT(state.release_errors, 0u);
  }
}

static void test_legacy_vi_v2_profile_bytes(void) {
  const t64_profile_descriptor *profile = NULL;
  uint8_t digest[T64_DIGEST_SIZE];
  uint8_t serialized[8192];
  char digest_hex[T64_DIGEST_SIZE * 2u + 1u];
  size_t serialized_length = 0u;
  const t64_status lookup_status = t64_profile_lookup(
      "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v2", &profile);

  T64_CHECK_INT(lookup_status, T64_STATUS_OK);
  if (lookup_status != T64_STATUS_OK || profile == NULL) return;
  T64_CHECK_INT(t64_profile_hash(profile, digest), T64_STATUS_OK);
  digest_to_hex(digest, digest_hex);
  T64_CHECK_STR(digest_hex,
                "5612696247abb5dae1c2abb6c15d7fd6c5632136210be7b21a1085590af1c94e");
  T64_CHECK_INT(t64_profile_serialize(profile, serialized,
                                      sizeof serialized,
                                      &serialized_length), T64_STATUS_OK);
  T64_CHECK_INT(serialized_length, (size_t)1038u);
}

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    puts("old-profile-pins");
    puts("descriptor-policy-mutations");
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    if (strcmp(argv[2], "old-profile-pins") == 0) {
      test_cold_t1_profile_identities();
      test_rdp_profile_metadata_binding(false);
      test_legacy_vi_v2_profile_bytes();
      return T64_TEST_FINISH("unit.core.ai.profile.old-profile-pins");
    }
    if (strcmp(argv[2], "descriptor-policy-mutations") == 0) {
      test_rdp_profile_metadata_binding(true);
      return T64_TEST_FINISH("unit.core.ai.profile.descriptor-policy-mutations");
    }
    fprintf(stderr, "unknown profile case: %s\n", argv[2]);
    return 2;
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--case NAME]\n", argv[0]);
    return 2;
  }
  test_profile_contract();
  test_compat_v2_profile_identity();
  test_cold_t1_profile_identities();
  test_rdp_profile_metadata_binding(true);
  test_legacy_vi_v2_profile_bytes();
  return T64_TEST_FINISH("unit.core.profile");
}
