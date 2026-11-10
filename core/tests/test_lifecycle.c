#line 1 "core/tests/test_lifecycle.c"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_test.h"
#include "t64_sha256.h"
#include "tendies64/t64_machine.h"

#define TEST_ROM_BYTES 4096u
#define MAX_STORAGE_SLOTS 16u

typedef enum {
  ALLOCATE_NORMAL,
  ALLOCATE_DENY,
  ALLOCATE_SHORT_CAPACITY,
  ALLOCATE_BAD_ALIGNMENT,
  ALLOCATE_ALIAS
} allocation_mode;

typedef struct {
  void *data;
  void *base;
  size_t actual_size;
  size_t reported_capacity;
  size_t reported_alignment;
  bool active;
  bool external;
} storage_slot;

typedef struct {
  storage_slot slots[MAX_STORAGE_SLOTS];
  size_t allocation_attempts;
  size_t allocation_count;
  size_t release_count;
  size_t outstanding_count;
  size_t bad_release_attempts;
  size_t protected_release_attempts;
  size_t replacement_checks;
  size_t replacement_check_failures;
  size_t last_slot;
  allocation_mode next_mode;
  void *alias_pointer;
  void *protected_pointer;
  void *old_image_pointer;
  const uint8_t *replacement_bytes;
  size_t replacement_length;
  bool inspect_replacement_release;
  bool use_adjacent_region;
  void *adjacent_region;
  size_t adjacent_region_size;
} allocator_state;

static size_t record_slot(allocator_state *state, void *data, void *base,
                          size_t actual_size, size_t reported_capacity,
                          size_t reported_alignment, bool external) {
  size_t slot = state->allocation_count;

  if (slot >= MAX_STORAGE_SLOTS) {
    return MAX_STORAGE_SLOTS;
  }
  state->slots[slot].data = data;
  state->slots[slot].base = base;
  state->slots[slot].actual_size = actual_size;
  state->slots[slot].reported_capacity = reported_capacity;
  state->slots[slot].reported_alignment = reported_alignment;
  state->slots[slot].active = true;
  state->slots[slot].external = external;
  state->allocation_count++;
  state->outstanding_count++;
  state->last_slot = slot;
  return slot;
}

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  allocator_state *state = (allocator_state *)context;
  allocation_mode mode;
  void *memory;
  size_t capacity;
  size_t reported_alignment;

  if (state == NULL || storage_out == NULL || alignment == 0u) {
    return false;
  }
  state->allocation_attempts++;
  mode = state->next_mode;
  state->next_mode = ALLOCATE_NORMAL;
  if (mode == ALLOCATE_DENY) {
    return false;
  }
  if (mode == ALLOCATE_ALIAS) {
    storage_out->data = state->alias_pointer;
    storage_out->capacity = 0u;
    storage_out->alignment = 0u;
    return state->alias_pointer != NULL;
  }
  if (state->allocation_count >= MAX_STORAGE_SLOTS) {
    return false;
  }

  if (state->use_adjacent_region) {
    if (size != state->adjacent_region_size || state->adjacent_region == NULL) {
      return false;
    }
    memory = state->adjacent_region;
    capacity = size;
    reported_alignment = alignment;
    if (record_slot(state, memory, memory, size, capacity,
                    reported_alignment, true) >= MAX_STORAGE_SLOTS) {
      return false;
    }
    state->use_adjacent_region = false;
  } else {
    memory = malloc(size);
    if (memory == NULL || ((uintptr_t)memory % alignment) != 0u) {
      free(memory);
      return false;
    }
    capacity = mode == ALLOCATE_SHORT_CAPACITY ? size - 1u : size;
    reported_alignment = mode == ALLOCATE_BAD_ALIGNMENT ? 0u : alignment;
    if (record_slot(state, memory, memory, size, capacity,
                    reported_alignment, false) >= MAX_STORAGE_SLOTS) {
      free(memory);
      return false;
    }
  }
  storage_out->data = memory;
  storage_out->capacity = capacity;
  storage_out->alignment = reported_alignment;
  return true;
}

static void test_release(void *context, t64_host_storage storage) {
  allocator_state *state = (allocator_state *)context;
  size_t i;

  if (state == NULL) {
    return;
  }
  if (storage.data == state->protected_pointer) {
    state->protected_release_attempts++;
    return;
  }
  for (i = 0u; i < state->allocation_count; i++) {
    storage_slot *slot = &state->slots[i];

    if (slot->active && slot->data == storage.data) {
      if (state->inspect_replacement_release &&
          storage.data == state->old_image_pointer) {
        if (state->replacement_length > 0u &&
            state->slots[state->last_slot].active &&
            state->slots[state->last_slot].data != storage.data &&
            state->slots[state->last_slot].reported_capacity >=
                state->replacement_length &&
            memcmp(state->slots[state->last_slot].data,
                   state->replacement_bytes,
                   state->replacement_length) == 0) {
          state->replacement_checks++;
        } else {
          state->replacement_check_failures++;
        }
      }
      slot->active = false;
      state->release_count++;
      state->outstanding_count--;
      if (!slot->external) {
        free(slot->base);
      }
      return;
    }
  }
  state->bad_release_attempts++;
}

static void fill_image(uint8_t image[TEST_ROM_BYTES], uint8_t salt) {
  size_t i;

  for (i = 0u; i < TEST_ROM_BYTES; i++) {
    image[i] = (uint8_t)((i * 29u + salt * 17u + 0x39u) & 0xffu);
  }
  image[0] = 0x80u;
  image[1] = 0x37u;
  image[2] = 0x12u;
  image[3] = 0x40u;
  for (i = 4u; i < 64u; i++) {
    image[i] = (uint8_t)(0x90u + i - 4u);
  }
}

static void hash_bytes(const uint8_t *bytes, size_t length,
                       uint8_t digest[T64_DIGEST_SIZE]) {
  t64_sha256_ctx context;

  t64_sha256_init(&context);
  t64_sha256_update(&context, bytes, length);
  t64_sha256_final(&context, digest);
}

static void check_machine_snapshot(t64_machine *machine,
                                   const t64_profile_descriptor *profile,
                                   const uint8_t *input, size_t input_length,
                                   allocator_state *allocator,
                                   allocation_mode next_mode,
                                   void *alias_pointer,
                                   t64_status expected_status,
                                   bool expect_no_allocation,
                                   size_t expected_allocation_delta,
                                   size_t expected_release_delta) {
  uint8_t state_before[T64_DIGEST_SIZE];
  uint8_t state_after[T64_DIGEST_SIZE];
  uint8_t content_before[T64_DIGEST_SIZE];
  uint8_t content_after[T64_DIGEST_SIZE];
  uint8_t profile_before[T64_DIGEST_SIZE];
  uint8_t profile_after[T64_DIGEST_SIZE];
  const t64_profile_descriptor *observed_profile = NULL;
  size_t attempts_before = allocator->allocation_attempts;
  size_t allocations_before = allocator->allocation_count;
  size_t releases_before = allocator->release_count;
  size_t outstanding_before = allocator->outstanding_count;

  T64_CHECK_INT(t64_machine_digest(machine, state_before), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_content_digest(machine, content_before),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_profile_hash(profile, profile_before), T64_STATUS_OK);
  allocator->next_mode = next_mode;
  allocator->alias_pointer = alias_pointer;
  T64_CHECK_INT(t64_machine_admit_rom(machine, input, input_length),
                expected_status);
  T64_CHECK_INT(t64_machine_digest(machine, state_after), T64_STATUS_OK);
  T64_CHECK(memcmp(state_before, state_after, T64_DIGEST_SIZE) == 0);
  T64_CHECK_INT(t64_machine_content_digest(machine, content_after),
                T64_STATUS_OK);
  T64_CHECK(memcmp(content_before, content_after, T64_DIGEST_SIZE) == 0);
  T64_CHECK_INT(t64_machine_profile(machine, &observed_profile),
                T64_STATUS_OK);
  T64_CHECK(observed_profile != NULL);
  if (observed_profile != NULL) {
    T64_CHECK_INT(t64_profile_hash(observed_profile, profile_after),
                  T64_STATUS_OK);
    T64_CHECK(memcmp(profile_before, profile_after, T64_DIGEST_SIZE) == 0);
  }
  T64_CHECK_INT(allocator->allocation_count,
                allocations_before + expected_allocation_delta);
  T64_CHECK_INT(allocator->release_count,
                releases_before + expected_release_delta);
  T64_CHECK_INT(allocator->outstanding_count,
                outstanding_before + expected_allocation_delta -
                    expected_release_delta);
  if (expect_no_allocation) {
    T64_CHECK_INT(allocator->allocation_attempts, attempts_before);
  } else {
    T64_CHECK_INT(allocator->allocation_attempts, attempts_before + 1u);
  }
}

static void test_invalid_creation_does_not_allocate(void) {
  allocator_state allocator = {0};
  t64_host_allocator valid_allocator = {test_allocate, test_release,
                                        &allocator};
  t64_host_allocator missing_allocate = {NULL, test_release, &allocator};
  t64_host_allocator missing_release = {test_allocate, NULL, &allocator};
  t64_profile_descriptor invalid_profile;
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = (t64_machine *)(uintptr_t)1u;
  size_t attempts_before;

  T64_CHECK_INT(t64_profile_lookup("ntsc-cart-4m-skeleton", &profile),
                T64_STATUS_OK);
  if (profile == NULL) {
    return;
  }
  attempts_before = allocator.allocation_attempts;
  T64_CHECK_INT(t64_machine_create(&valid_allocator, profile, NULL),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK_INT(allocator.allocation_attempts, attempts_before);
  T64_CHECK_INT(t64_machine_create(&missing_allocate, profile, &machine),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK(machine == NULL);
  T64_CHECK_INT(t64_machine_create(&missing_release, profile, &machine),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK(machine == NULL);
  (void)memcpy(&invalid_profile, profile, sizeof invalid_profile);
  invalid_profile.rdram_bytes = 0u;
  T64_CHECK_INT(t64_machine_create(&valid_allocator, &invalid_profile,
                                  &machine),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK(machine == NULL);
  T64_CHECK_INT(allocator.allocation_attempts, attempts_before);

  allocator.next_mode = ALLOCATE_DENY;
  T64_CHECK_INT(t64_machine_create(&valid_allocator, profile, &machine),
                T64_STATUS_ALLOCATION_FAILED);
  T64_CHECK(machine == NULL);
  T64_CHECK_INT(allocator.allocation_attempts, attempts_before + 1u);
  T64_CHECK_INT(allocator.release_count, 0u);
  T64_CHECK_INT(allocator.outstanding_count, 0u);

  allocator.alias_pointer = &machine;
  allocator.next_mode = ALLOCATE_ALIAS;
  T64_CHECK_INT(t64_machine_create(&valid_allocator, profile, &machine),
                T64_STATUS_BAD_STORAGE);
  T64_CHECK(machine == NULL);
  T64_CHECK_INT(allocator.allocation_attempts, attempts_before + 2u);
  T64_CHECK_INT(allocator.bad_release_attempts, 0u);
  T64_CHECK_INT(allocator.release_count, 0u);

  allocator.next_mode = ALLOCATE_SHORT_CAPACITY;
  T64_CHECK_INT(t64_machine_create(&valid_allocator, profile, &machine),
                T64_STATUS_BAD_STORAGE);
  T64_CHECK(machine == NULL);
  T64_CHECK_INT(allocator.release_count, 1u);
  T64_CHECK_INT(allocator.outstanding_count, 0u);

  allocator.next_mode = ALLOCATE_BAD_ALIGNMENT;
  T64_CHECK_INT(t64_machine_create(&valid_allocator, profile, &machine),
                T64_STATUS_BAD_STORAGE);
  T64_CHECK(machine == NULL);
  T64_CHECK_INT(allocator.release_count, 2u);
  T64_CHECK_INT(allocator.outstanding_count, 0u);
  T64_CHECK_INT(allocator.bad_release_attempts, 0u);
}

static void test_admission_alias_and_descriptor_refusals(void) {
  allocator_state allocator = {0};
  const t64_host_allocator host_allocator = {test_allocate, test_release,
                                              &allocator};
  uint8_t image[TEST_ROM_BYTES];
  uint8_t replacement[TEST_ROM_BYTES];
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  t64_stop_request tick_request = {T64_STOP_GUEST_TICK, UINT64_C(41)};
  t64_stop_result tick_result = {0u, 0u};
  void *machine_storage;
  size_t machine_storage_size;
  void *image_storage;
  size_t attempts_before;
  size_t releases_before;
  size_t protected_attempts_before;
  uint8_t *overlap_input;

  fill_image(image, 1u);
  fill_image(replacement, 2u);
  T64_CHECK_INT(t64_profile_lookup("ntsc-cart-4m-skeleton", &profile),
                T64_STATUS_OK);
  if (profile == NULL) {
    return;
  }
  T64_CHECK_INT(t64_machine_create(&host_allocator, profile, &machine),
                T64_STATUS_OK);
  T64_CHECK(machine != NULL);
  if (machine == NULL) {
    return;
  }
  {
    uint8_t header[T64_ROM_HEADER_SIZE];
    size_t image_size = 123u;
    size_t header_length = 321u;

    (void)memset(header, 0xacu, sizeof header);
    T64_CHECK_INT(t64_machine_rom_size(machine, &image_size),
                  T64_STATUS_NOT_READY);
    T64_CHECK_INT(image_size, 123u);
    T64_CHECK_INT(t64_machine_rom_header(machine, NULL, 0u, &header_length),
                  T64_STATUS_NOT_READY);
    T64_CHECK_INT(header_length, 321u);
    T64_CHECK_INT(t64_machine_rom_header(machine, header, sizeof header,
                                         &header_length),
                  T64_STATUS_NOT_READY);
    T64_CHECK_INT(header_length, 321u);
    for (size_t i = 0u; i < sizeof header; i++) {
      T64_CHECK_INT(header[i], 0xacu);
    }
  }
  T64_CHECK_INT(t64_machine_admit_rom(machine, image, sizeof image),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_advance(machine, &tick_request, &tick_result),
                T64_STATUS_OK);
  machine_storage = allocator.slots[0].data;
  machine_storage_size = allocator.slots[0].actual_size;
  image_storage = allocator.slots[1].data;

  check_machine_snapshot(machine, profile, replacement, sizeof replacement,
                         &allocator, ALLOCATE_DENY, NULL,
                         T64_STATUS_ALLOCATION_FAILED, false, 0u, 0u);

  releases_before = allocator.release_count;
  check_machine_snapshot(machine, profile, replacement, sizeof replacement,
                         &allocator, ALLOCATE_SHORT_CAPACITY, NULL,
                         T64_STATUS_BAD_STORAGE, false, 1u, 1u);
  T64_CHECK_INT(allocator.release_count, releases_before + 1u);

  releases_before = allocator.release_count;
  check_machine_snapshot(machine, profile, replacement, sizeof replacement,
                         &allocator, ALLOCATE_BAD_ALIGNMENT, NULL,
                         T64_STATUS_BAD_STORAGE, false, 1u, 1u);
  T64_CHECK_INT(allocator.release_count, releases_before + 1u);

  releases_before = allocator.release_count;
  protected_attempts_before = allocator.protected_release_attempts;
  allocator.protected_pointer = image_storage;
  check_machine_snapshot(machine, profile, replacement, sizeof replacement,
                         &allocator, ALLOCATE_ALIAS, image_storage,
                         T64_STATUS_BAD_STORAGE, false, 0u, 0u);
  T64_CHECK_INT(allocator.protected_release_attempts,
                protected_attempts_before);
  T64_CHECK_INT(allocator.release_count, releases_before);
  allocator.protected_pointer = NULL;

  attempts_before = allocator.allocation_attempts;
  releases_before = allocator.release_count;
  check_machine_snapshot(machine, profile, image_storage, sizeof image,
                         &allocator, ALLOCATE_NORMAL, NULL,
                         T64_STATUS_INVALID_ROM, true, 0u, 0u);
  T64_CHECK_INT(allocator.allocation_attempts, attempts_before);
  T64_CHECK_INT(allocator.release_count, releases_before);

  overlap_input = (uint8_t *)machine_storage + machine_storage_size -
                  sizeof image;
  (void)memcpy(overlap_input, image, sizeof image);
  attempts_before = allocator.allocation_attempts;
  check_machine_snapshot(machine, profile, overlap_input, sizeof image,
                         &allocator, ALLOCATE_NORMAL, NULL,
                         T64_STATUS_INVALID_ROM, true, 0u, 0u);
  T64_CHECK_INT(allocator.allocation_attempts, attempts_before);

  t64_machine_destroy(machine);
  T64_CHECK_INT(allocator.outstanding_count, 0u);
  T64_CHECK_INT(allocator.release_count, allocator.allocation_count);
  T64_CHECK_INT(allocator.bad_release_attempts, 0u);
}

static void test_zero_capacity_aliases_are_not_released(void) {
  allocator_state allocator = {0};
  const t64_host_allocator host_allocator = {test_allocate, test_release,
                                              &allocator};
  uint8_t image[TEST_ROM_BYTES];
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  size_t bad_releases_before;
  size_t protected_releases_before;

  fill_image(image, 3u);
  T64_CHECK_INT(t64_profile_lookup("ntsc-cart-4m-skeleton", &profile),
                T64_STATUS_OK);
  if (profile == NULL) {
    return;
  }
  T64_CHECK_INT(t64_machine_create(&host_allocator, profile, &machine),
                T64_STATUS_OK);
  if (machine == NULL) {
    return;
  }
  T64_CHECK_INT(t64_machine_admit_rom(machine, image, sizeof image),
                T64_STATUS_OK);
  bad_releases_before = allocator.bad_release_attempts;
  check_machine_snapshot(machine, profile, image, sizeof image, &allocator,
                         ALLOCATE_ALIAS, image, T64_STATUS_BAD_STORAGE, false,
                         0u, 0u);
  T64_CHECK_INT(allocator.bad_release_attempts, bad_releases_before);

  protected_releases_before = allocator.protected_release_attempts;
  allocator.alias_pointer = allocator.slots[1].data;
  allocator.protected_pointer = allocator.alias_pointer;
  check_machine_snapshot(machine, profile, image, sizeof image, &allocator,
                         ALLOCATE_ALIAS, allocator.alias_pointer,
                         T64_STATUS_BAD_STORAGE, false, 0u, 0u);
  T64_CHECK_INT(allocator.protected_release_attempts,
                protected_releases_before);

  allocator.protected_pointer = NULL;
  t64_machine_destroy(machine);
  T64_CHECK_INT(allocator.outstanding_count, 0u);
  T64_CHECK_INT(allocator.release_count, allocator.allocation_count);
  T64_CHECK_INT(allocator.bad_release_attempts, 0u);
}

static void test_adjacent_candidate_and_replacement_order(void) {
  allocator_state allocator = {0};
  const t64_host_allocator host_allocator = {test_allocate, test_release,
                                              &allocator};
  uint8_t *arena = (uint8_t *)malloc(TEST_ROM_BYTES * 2u);
  uint8_t replacement[TEST_ROM_BYTES];
  uint8_t expected_digest[T64_DIGEST_SIZE];
  uint8_t actual_digest[T64_DIGEST_SIZE];
  uint8_t profile_before[T64_DIGEST_SIZE];
  uint8_t profile_after[T64_DIGEST_SIZE];
  const t64_profile_descriptor *profile = NULL;
  const t64_profile_descriptor *observed = NULL;
  t64_machine *machine = NULL;
  t64_stop_request later_tick = {T64_STOP_GUEST_TICK, UINT64_C(99)};
  t64_stop_request reset_tick = {T64_STOP_GUEST_TICK, UINT64_C(0)};
  t64_stop_result result = {0u, 0u};
  void *old_image;

  T64_CHECK(arena != NULL);
  if (arena == NULL) {
    return;
  }
  fill_image(arena, 4u);
  fill_image(replacement, 5u);
  T64_CHECK_INT(t64_profile_lookup("ntsc-cart-4m-skeleton", &profile),
                T64_STATUS_OK);
  if (profile == NULL) {
    free(arena);
    return;
  }
  T64_CHECK_INT(t64_machine_create(&host_allocator, profile, &machine),
                T64_STATUS_OK);
  if (machine == NULL) {
    free(arena);
    return;
  }
  allocator.use_adjacent_region = true;
  allocator.adjacent_region = arena + TEST_ROM_BYTES;
  allocator.adjacent_region_size = TEST_ROM_BYTES;
  T64_CHECK_INT(t64_machine_admit_rom(machine, arena, TEST_ROM_BYTES),
                T64_STATUS_OK);
  T64_CHECK(memcmp(arena + TEST_ROM_BYTES, arena, TEST_ROM_BYTES) == 0);
  hash_bytes(arena, TEST_ROM_BYTES, expected_digest);
  T64_CHECK_INT(t64_machine_content_digest(machine, actual_digest),
                T64_STATUS_OK);
  T64_CHECK(memcmp(actual_digest, expected_digest, T64_DIGEST_SIZE) == 0);
  {
    uint8_t header[T64_ROM_HEADER_SIZE];
    size_t image_size = 0u;
    size_t header_length = 0u;

    T64_CHECK_INT(t64_machine_rom_size(machine, &image_size), T64_STATUS_OK);
    T64_CHECK_INT(image_size, TEST_ROM_BYTES);
    T64_CHECK_INT(t64_machine_rom_header(machine, header, sizeof header,
                                         &header_length),
                  T64_STATUS_OK);
    T64_CHECK_INT(header_length, T64_ROM_HEADER_SIZE);
    T64_CHECK(memcmp(header, arena, T64_ROM_HEADER_SIZE) == 0);
  }

  old_image = allocator.slots[1].data;
  T64_CHECK_INT(t64_machine_profile(machine, &observed), T64_STATUS_OK);
  T64_CHECK(observed != NULL);
  if (observed != NULL) {
    T64_CHECK_INT(t64_profile_hash(observed, profile_before), T64_STATUS_OK);
  }
  T64_CHECK_INT(t64_machine_advance(machine, &later_tick, &result),
                T64_STATUS_OK);
  allocator.old_image_pointer = old_image;
  allocator.replacement_bytes = replacement;
  allocator.replacement_length = sizeof replacement;
  allocator.inspect_replacement_release = true;
  T64_CHECK_INT(t64_machine_admit_rom(machine, replacement,
                                      sizeof replacement),
                T64_STATUS_OK);
  allocator.inspect_replacement_release = false;
  T64_CHECK_INT(allocator.replacement_checks, 1u);
  T64_CHECK_INT(allocator.replacement_check_failures, 0u);
  hash_bytes(replacement, sizeof replacement, expected_digest);
  T64_CHECK_INT(t64_machine_content_digest(machine, actual_digest),
                T64_STATUS_OK);
  T64_CHECK(memcmp(actual_digest, expected_digest, T64_DIGEST_SIZE) == 0);
  {
    uint8_t header[T64_ROM_HEADER_SIZE];
    size_t image_size = 0u;
    size_t header_length = 0u;

    T64_CHECK_INT(t64_machine_rom_size(machine, &image_size), T64_STATUS_OK);
    T64_CHECK_INT(image_size, sizeof replacement);
    T64_CHECK_INT(t64_machine_rom_header(machine, header, sizeof header,
                                         &header_length),
                  T64_STATUS_OK);
    T64_CHECK_INT(header_length, T64_ROM_HEADER_SIZE);
    T64_CHECK(memcmp(header, replacement, T64_ROM_HEADER_SIZE) == 0);
  }
  T64_CHECK_INT(t64_machine_profile(machine, &observed), T64_STATUS_OK);
  T64_CHECK(observed != NULL);
  if (observed != NULL) {
    T64_CHECK_INT(t64_profile_hash(observed, profile_after), T64_STATUS_OK);
    T64_CHECK(memcmp(profile_before, profile_after, T64_DIGEST_SIZE) == 0);
  }
  T64_CHECK_INT(t64_machine_advance(machine, &reset_tick, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.tick, 0u);

  {
    uint8_t state_before[T64_DIGEST_SIZE];
    uint8_t state_after[T64_DIGEST_SIZE];
    uint8_t too_small[T64_ROM_HEADER_SIZE];
    uint8_t too_small_before[T64_ROM_HEADER_SIZE];
    size_t output_length = 777u;
    size_t output_size = 888u;
    size_t attempts_before = allocator.allocation_attempts;
    union {
      size_t alignment;
      uint8_t bytes[T64_ROM_HEADER_SIZE];
    } overlapping_outputs;

    T64_CHECK_INT(t64_machine_digest(machine, state_before), T64_STATUS_OK);
    T64_CHECK_INT(t64_machine_rom_header(machine, NULL, 0u, &output_length),
                  T64_STATUS_BUFFER_TOO_SMALL);
    T64_CHECK_INT(output_length, T64_ROM_HEADER_SIZE);

    (void)memset(too_small, 0x5au, sizeof too_small);
    (void)memcpy(too_small_before, too_small, sizeof too_small);
    output_length = 0u;
    T64_CHECK_INT(t64_machine_rom_header(machine, too_small,
                                         T64_ROM_HEADER_SIZE - 1u,
                                         &output_length),
                  T64_STATUS_BUFFER_TOO_SMALL);
    T64_CHECK_INT(output_length, T64_ROM_HEADER_SIZE);
    T64_CHECK(memcmp(too_small, too_small_before, sizeof too_small) == 0);

    output_length = 777u;
    T64_CHECK_INT(t64_machine_rom_header(machine, NULL, 1u, &output_length),
                  T64_STATUS_INVALID_ARGUMENT);
    T64_CHECK_INT(output_length, 777u);
    T64_CHECK_INT(t64_machine_rom_header(machine, NULL, 0u, NULL),
                  T64_STATUS_INVALID_ARGUMENT);
    T64_CHECK_INT(t64_machine_rom_size(machine, NULL),
                  T64_STATUS_INVALID_ARGUMENT);
    T64_CHECK_INT(t64_machine_rom_size(NULL, &output_size),
                  T64_STATUS_INVALID_ARGUMENT);
    T64_CHECK_INT(t64_machine_rom_size(
                      machine, (size_t *)allocator.slots[0].data),
                  T64_STATUS_INVALID_ARGUMENT);
    T64_CHECK_INT(t64_machine_rom_header(
                      machine, (uint8_t *)allocator.slots[0].data,
                      T64_ROM_HEADER_SIZE, &output_length),
                  T64_STATUS_INVALID_ARGUMENT);
    T64_CHECK_INT(t64_machine_rom_header(
                      machine, (uint8_t *)allocator.slots[2].data,
                      T64_ROM_HEADER_SIZE, &output_length),
                  T64_STATUS_INVALID_ARGUMENT);
    overlapping_outputs.alignment = 0u;
    T64_CHECK_INT(t64_machine_rom_header(
                      machine, overlapping_outputs.bytes,
                      T64_ROM_HEADER_SIZE, &overlapping_outputs.alignment),
                  T64_STATUS_INVALID_ARGUMENT);
    T64_CHECK_INT(overlapping_outputs.alignment, 0u);

    T64_CHECK_INT(allocator.allocation_attempts, attempts_before);
    T64_CHECK_INT(t64_machine_digest(machine, state_after), T64_STATUS_OK);
    T64_CHECK(memcmp(state_before, state_after, T64_DIGEST_SIZE) == 0);
  }

  t64_machine_destroy(machine);
  free(arena);
  T64_CHECK_INT(allocator.outstanding_count, 0u);
  T64_CHECK_INT(allocator.release_count, allocator.allocation_count);
  T64_CHECK_INT(allocator.bad_release_attempts, 0u);
}

static void test_two_instances_have_disjoint_storage(void) {
  allocator_state first_allocator = {0};
  allocator_state second_allocator = {0};
  const t64_host_allocator first_host = {test_allocate, test_release,
                                         &first_allocator};
  const t64_host_allocator second_host = {test_allocate, test_release,
                                          &second_allocator};
  uint8_t image[TEST_ROM_BYTES];
  const t64_profile_descriptor *profile = NULL;
  t64_machine *first = NULL;
  t64_machine *second = NULL;
  size_t i, j;

  fill_image(image, 6u);
  T64_CHECK_INT(t64_profile_lookup("ntsc-cart-4m-skeleton", &profile),
                T64_STATUS_OK);
  if (profile == NULL) {
    return;
  }
  T64_CHECK_INT(t64_machine_create(&first_host, profile, &first),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_create(&second_host, profile, &second),
                T64_STATUS_OK);
  if (first == NULL || second == NULL) {
    t64_machine_destroy(second);
    t64_machine_destroy(first);
    return;
  }
  T64_CHECK_INT(t64_machine_admit_rom(first, image, sizeof image),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_admit_rom(second, image, sizeof image),
                T64_STATUS_OK);
  for (i = 0u; i < first_allocator.allocation_count; i++) {
    uintptr_t first_begin;
    uintptr_t first_end;

    if (!first_allocator.slots[i].active) {
      continue;
    }
    first_begin = (uintptr_t)first_allocator.slots[i].data;
    first_end = first_begin + first_allocator.slots[i].reported_capacity;
    for (j = 0u; j < second_allocator.allocation_count; j++) {
      uintptr_t second_begin;
      uintptr_t second_end;

      if (!second_allocator.slots[j].active) {
        continue;
      }
      second_begin = (uintptr_t)second_allocator.slots[j].data;
      second_end = second_begin + second_allocator.slots[j].reported_capacity;
      T64_CHECK(first_end <= second_begin || second_end <= first_begin);
    }
  }
  t64_machine_destroy(second);
  t64_machine_destroy(first);
  T64_CHECK_INT(first_allocator.release_count, 2u);
  T64_CHECK_INT(second_allocator.release_count, 2u);
  T64_CHECK_INT(first_allocator.outstanding_count, 0u);
  T64_CHECK_INT(second_allocator.outstanding_count, 0u);
}

int main(void) {
  test_invalid_creation_does_not_allocate();
  test_admission_alias_and_descriptor_refusals();
  test_zero_capacity_aliases_are_not_released();
  test_adjacent_candidate_and_replacement_order();
  test_two_instances_have_disjoint_storage();
  return T64_TEST_FINISH("unit.core.lifecycle");
}
