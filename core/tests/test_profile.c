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
  uint8_t digest[T64_DIGEST_SIZE];
  uint8_t serialized[T64_PROFILE_SERIALIZED_MAX_BYTES];
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
  digest_to_hex(profile_hash, profile_hash_hex);
  T64_CHECK_STR(profile_hash_hex,
                "34e61cf9555546a83aa81d856bb7b011a9247359aad005eb410382ded209d27a");
  T64_CHECK(strncmp(t64_build_identity(), "t64-build-v1-", 13u) == 0);
  T64_CHECK(strchr(t64_build_identity(), '/') == NULL);
  T64_CHECK(strlen(t64_source_revision()) > 0u);
  (void)printf("profile=%s profile_sha256=%s build_id=%s source_revision=%s\n",
               profile->profile_id, profile_hash_hex, t64_build_identity(),
               t64_source_revision());
  (void)printf("evidence=unit-tested synthetic counters; no guest execution\n");
}

int main(void) {
  test_profile_contract();
  return T64_TEST_FINISH("unit.core.profile");
}
