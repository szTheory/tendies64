#line 1 "core/tests/test_rom_admission.c"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_test.h"
#include "t64_sha256.h"
#include "tendies64/t64_machine.h"

#define TEST_ROM_MIN_BYTES 4096u
#define TEST_ROM_MAX_BYTES 67108864u
#define MAX_TRACKED_ALLOCATIONS 8u

typedef struct {
  void *live[MAX_TRACKED_ALLOCATIONS];
  size_t sizes[MAX_TRACKED_ALLOCATIONS];
  size_t allocation_attempts;
  size_t allocation_count;
  size_t release_count;
  size_t outstanding_count;
  size_t release_errors;
  bool deny_next_allocation;
} allocator_state;

typedef enum {
  TEST_ORDER_BIG_ENDIAN,
  TEST_ORDER_BYTE_SWAPPED,
  TEST_ORDER_WORD_REVERSED
} test_byte_order;

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  allocator_state *state = (allocator_state *)context;
  void *memory;

  if (state == NULL || storage_out == NULL || alignment == 0u) {
    return false;
  }
  state->allocation_attempts++;
  if (state->deny_next_allocation) {
    state->deny_next_allocation = false;
    return false;
  }
  if (state->allocation_count >= MAX_TRACKED_ALLOCATIONS) {
    return false;
  }
  memory = malloc(size);
  if (memory == NULL || ((uintptr_t)memory % alignment) != 0u) {
    free(memory);
    return false;
  }
  state->live[state->allocation_count] = memory;
  state->sizes[state->allocation_count] = size;
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

static void fill_canonical_image(uint8_t *image, size_t length) {
  size_t i;

  for (i = 0u; i < length; i++) {
    image[i] = (uint8_t)((i * 37u + (i >> 4u) * 11u + 0x53u) & 0xffu);
  }
  image[0] = 0x80u;
  image[1] = 0x37u;
  image[2] = 0x12u;
  image[3] = 0x40u;
  for (i = 4u; i < 64u; i++) {
    image[i] = (uint8_t)(0xa0u + (i - 4u));
  }
}

static void encode_order(const uint8_t *canonical, uint8_t *raw,
                         size_t length, test_byte_order order) {
  size_t i;

  if (order == TEST_ORDER_BIG_ENDIAN) {
    (void)memcpy(raw, canonical, length);
  } else if (order == TEST_ORDER_BYTE_SWAPPED) {
    for (i = 0u; i < length; i += 2u) {
      raw[i] = canonical[i + 1u];
      raw[i + 1u] = canonical[i];
    }
  } else {
    for (i = 0u; i < length; i += 4u) {
      raw[i] = canonical[i + 3u];
      raw[i + 1u] = canonical[i + 2u];
      raw[i + 2u] = canonical[i + 1u];
      raw[i + 3u] = canonical[i];
    }
  }
}

static void sha256_bytes(const uint8_t *bytes, size_t length,
                         uint8_t digest[T64_DIGEST_SIZE]) {
  t64_sha256_ctx context;

  t64_sha256_init(&context);
  t64_sha256_update(&context, bytes, length);
  t64_sha256_final(&context, digest);
}

static void check_digest(const t64_machine *machine,
                         const uint8_t expected[T64_DIGEST_SIZE]) {
  uint8_t actual[T64_DIGEST_SIZE];

  T64_CHECK_INT(t64_machine_content_digest(machine, actual), T64_STATUS_OK);
  T64_CHECK(memcmp(actual, expected, T64_DIGEST_SIZE) == 0);
}

static void check_header_and_size(const t64_machine *machine,
                                  const uint8_t *canonical,
                                  size_t expected_image_size) {
  uint8_t header[T64_ROM_HEADER_SIZE];
  uint8_t untouched[T64_ROM_HEADER_SIZE];
  size_t image_size = SIZE_MAX;
  size_t required = 0u;
  size_t i;

  T64_CHECK_INT(t64_machine_rom_size(machine, &image_size), T64_STATUS_OK);
  T64_CHECK_INT(image_size, expected_image_size);

  T64_CHECK_INT(t64_machine_rom_header(machine, NULL, 0u, &required),
                T64_STATUS_BUFFER_TOO_SMALL);
  T64_CHECK_INT(required, T64_ROM_HEADER_SIZE);

  (void)memset(untouched, 0xc3, sizeof untouched);
  required = 0u;
  T64_CHECK_INT(t64_machine_rom_header(machine, untouched,
                                       T64_ROM_HEADER_SIZE - 1u, &required),
                T64_STATUS_BUFFER_TOO_SMALL);
  T64_CHECK_INT(required, T64_ROM_HEADER_SIZE);
  for (i = 0u; i < sizeof untouched; i++) {
    T64_CHECK_INT(untouched[i], 0xc3u);
  }

  required = 0u;
  T64_CHECK_INT(t64_machine_rom_header(machine, header, sizeof header,
                                       &required),
                T64_STATUS_OK);
  T64_CHECK_INT(required, T64_ROM_HEADER_SIZE);
  T64_CHECK(memcmp(header, canonical, T64_ROM_HEADER_SIZE) == 0);
  T64_CHECK(memcmp(header + 0x20u, canonical + 0x20u, 20u) == 0);
  T64_CHECK(memcmp(header + 0x3bu, canonical + 0x3bu, 4u) == 0);
  T64_CHECK_INT(header[0x3fu], canonical[0x3fu]);
}

static void check_refusal_unchanged(t64_machine *machine,
                                    allocator_state *allocator,
                                    const uint8_t *bytes, size_t length,
                                    t64_status expected_status) {
  uint8_t state_before[T64_DIGEST_SIZE];
  uint8_t state_after[T64_DIGEST_SIZE];
  uint8_t content_before[T64_DIGEST_SIZE];
  uint8_t content_after[T64_DIGEST_SIZE];
  uint8_t profile_before[T64_DIGEST_SIZE];
  uint8_t profile_after[T64_DIGEST_SIZE];
  uint8_t header_before[T64_ROM_HEADER_SIZE];
  uint8_t header_after[T64_ROM_HEADER_SIZE];
  const t64_profile_descriptor *profile = NULL;
  size_t allocations_before = allocator->allocation_count;
  size_t releases_before = allocator->release_count;
  size_t outstanding_before = allocator->outstanding_count;
  size_t image_size_before = 0u;
  size_t image_size_after = 0u;
  size_t header_length = 0u;

  T64_CHECK_INT(t64_machine_digest(machine, state_before), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_content_digest(machine, content_before),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_rom_size(machine, &image_size_before),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_rom_header(machine, header_before,
                                       sizeof header_before, &header_length),
                T64_STATUS_OK);
  T64_CHECK_INT(header_length, T64_ROM_HEADER_SIZE);
  T64_CHECK_INT(t64_machine_profile(machine, &profile), T64_STATUS_OK);
  T64_CHECK(profile != NULL);
  if (profile != NULL) {
    T64_CHECK_INT(t64_profile_hash(profile, profile_before), T64_STATUS_OK);
  }

  T64_CHECK_INT(t64_machine_admit_rom(machine, bytes, length),
                expected_status);

  T64_CHECK_INT(t64_machine_digest(machine, state_after), T64_STATUS_OK);
  T64_CHECK(memcmp(state_before, state_after, T64_DIGEST_SIZE) == 0);
  T64_CHECK_INT(t64_machine_content_digest(machine, content_after),
                T64_STATUS_OK);
  T64_CHECK(memcmp(content_before, content_after, T64_DIGEST_SIZE) == 0);
  T64_CHECK_INT(t64_machine_rom_size(machine, &image_size_after),
                T64_STATUS_OK);
  T64_CHECK_INT(image_size_after, image_size_before);
  T64_CHECK_INT(t64_machine_rom_header(machine, header_after,
                                       sizeof header_after, &header_length),
                T64_STATUS_OK);
  T64_CHECK_INT(header_length, T64_ROM_HEADER_SIZE);
  T64_CHECK(memcmp(header_before, header_after, T64_ROM_HEADER_SIZE) == 0);
  T64_CHECK_INT(t64_machine_profile(machine, &profile), T64_STATUS_OK);
  T64_CHECK(profile != NULL);
  if (profile != NULL) {
    T64_CHECK_INT(t64_profile_hash(profile, profile_after), T64_STATUS_OK);
    T64_CHECK(memcmp(profile_before, profile_after, T64_DIGEST_SIZE) == 0);
  }
  T64_CHECK_INT(allocator->allocation_count, allocations_before);
  T64_CHECK_INT(allocator->release_count, releases_before);
  T64_CHECK_INT(allocator->outstanding_count, outstanding_before);
}

static void test_all_orders_and_refusal_state(void) {
  static const test_byte_order orders[] = {
      TEST_ORDER_BIG_ENDIAN,
      TEST_ORDER_BYTE_SWAPPED,
      TEST_ORDER_WORD_REVERSED,
  };
  static const size_t short_lengths[] = {
      0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u,
      16u, 17u, 18u, 19u, 20u, 21u, 22u, 23u, 24u, 25u, 26u, 27u, 28u, 29u,
      30u, 31u, 32u, 33u, 34u, 35u, 36u, 37u, 38u, 39u, 40u, 41u, 42u, 43u,
      44u, 45u, 46u, 47u, 48u, 49u, 50u, 51u, 52u, 53u, 54u, 55u, 56u, 57u,
      58u, 59u, 60u, 61u, 62u, 63u,
  };
  allocator_state allocator = {0};
  const t64_host_allocator host_allocator = {test_allocate, test_release,
                                              &allocator};
  uint8_t canonical[TEST_ROM_MIN_BYTES];
  uint8_t raw[TEST_ROM_MIN_BYTES];
  uint8_t malformed[TEST_ROM_MIN_BYTES];
  uint8_t expected_digest[T64_DIGEST_SIZE];
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  t64_stop_request tick_request = {T64_STOP_GUEST_TICK, UINT64_C(9)};
  t64_stop_result tick_result = {0u, 0u};
  size_t i;

  fill_canonical_image(canonical, sizeof canonical);
  sha256_bytes(canonical, sizeof canonical, expected_digest);
  T64_CHECK_INT(t64_profile_lookup("ntsc-cart-4m-skeleton", &profile),
                T64_STATUS_OK);
  T64_CHECK(profile != NULL);
  if (profile == NULL) {
    return;
  }
  T64_CHECK_INT(t64_machine_create(&host_allocator, profile, &machine),
                T64_STATUS_OK);
  T64_CHECK(machine != NULL);
  if (machine == NULL) {
    return;
  }

  for (i = 0u; i < sizeof orders / sizeof orders[0]; i++) {
    encode_order(canonical, raw, sizeof raw, orders[i]);
    T64_CHECK_INT(t64_machine_admit_rom(machine, raw, sizeof raw),
                  T64_STATUS_OK);
    check_header_and_size(machine, canonical, sizeof canonical);
    if (t64_machine_content_digest(machine, expected_digest) ==
        T64_STATUS_OK) {
      uint8_t independent_digest[T64_DIGEST_SIZE];

      sha256_bytes(canonical, sizeof canonical, independent_digest);
      T64_CHECK(memcmp(expected_digest, independent_digest,
                       T64_DIGEST_SIZE) == 0);
    } else {
      T64_CHECK(false);
    }
    sha256_bytes(canonical, sizeof canonical, expected_digest);
  }

  T64_CHECK_INT(t64_machine_advance(machine, &tick_request, &tick_result),
                T64_STATUS_OK);
  T64_CHECK_INT(tick_result.tick, 9u);
  encode_order(canonical, raw, sizeof raw, TEST_ORDER_BIG_ENDIAN);
  for (i = 0u; i < sizeof short_lengths / sizeof short_lengths[0]; i++) {
    check_refusal_unchanged(machine, &allocator, raw, short_lengths[i],
                            T64_STATUS_INVALID_ROM);
  }
  check_refusal_unchanged(machine, &allocator, raw, 4095u,
                          T64_STATUS_INVALID_ROM);
  check_refusal_unchanged(machine, &allocator, raw, 4097u,
                          T64_STATUS_INVALID_ROM);
  check_refusal_unchanged(machine, &allocator, NULL, sizeof raw,
                          T64_STATUS_INVALID_ROM);

  (void)memcpy(malformed, canonical, sizeof malformed);
  malformed[0] = 0u;
  check_refusal_unchanged(machine, &allocator, malformed, sizeof malformed,
                          T64_STATUS_INVALID_ROM);

  allocator.deny_next_allocation = true;
  check_refusal_unchanged(machine, &allocator, raw, sizeof raw,
                          T64_STATUS_ALLOCATION_FAILED);

  t64_machine_destroy(machine);
  T64_CHECK_INT(allocator.outstanding_count, 0u);
  T64_CHECK_INT(allocator.release_errors, 0u);
}

static void test_minimum_and_maximum_boundaries(void) {
  allocator_state allocator = {0};
  const t64_host_allocator host_allocator = {test_allocate, test_release,
                                              &allocator};
  uint8_t *maximum_span = NULL;
  uint8_t expected_digest[T64_DIGEST_SIZE];
  uint8_t before_rejection[T64_DIGEST_SIZE];
  uint8_t after_rejection[T64_DIGEST_SIZE];
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;

  maximum_span = (uint8_t *)malloc((size_t)TEST_ROM_MAX_BYTES + 1u);
  T64_CHECK(maximum_span != NULL);
  if (maximum_span == NULL) {
    return;
  }
  fill_canonical_image(maximum_span, (size_t)TEST_ROM_MAX_BYTES + 1u);
  T64_CHECK_INT(t64_profile_lookup("ntsc-cart-4m-skeleton", &profile),
                T64_STATUS_OK);
  if (profile == NULL) {
    free(maximum_span);
    return;
  }
  T64_CHECK_INT(t64_machine_create(&host_allocator, profile, &machine),
                T64_STATUS_OK);
  if (machine == NULL) {
    free(maximum_span);
    return;
  }
  T64_CHECK_INT(t64_machine_admit_rom(machine, maximum_span,
                                      (size_t)TEST_ROM_MIN_BYTES),
                T64_STATUS_OK);
  check_header_and_size(machine, maximum_span, TEST_ROM_MIN_BYTES);
  T64_CHECK_INT(t64_machine_admit_rom(machine, maximum_span,
                                      (size_t)TEST_ROM_MAX_BYTES),
                T64_STATUS_OK);
  check_header_and_size(machine, maximum_span, TEST_ROM_MAX_BYTES);
  sha256_bytes(maximum_span, (size_t)TEST_ROM_MAX_BYTES, expected_digest);
  check_digest(machine, expected_digest);
  T64_CHECK_INT(t64_machine_digest(machine, before_rejection), T64_STATUS_OK);
  check_refusal_unchanged(machine, &allocator, maximum_span,
                          (size_t)TEST_ROM_MAX_BYTES + 1u,
                          T64_STATUS_INVALID_ROM);
  T64_CHECK_INT(t64_machine_digest(machine, after_rejection), T64_STATUS_OK);
  T64_CHECK(memcmp(before_rejection, after_rejection, T64_DIGEST_SIZE) == 0);

  t64_machine_destroy(machine);
  free(maximum_span);
  T64_CHECK_INT(allocator.outstanding_count, 0u);
  T64_CHECK_INT(allocator.release_errors, 0u);
}

int main(void) {
  test_all_orders_and_refusal_state();
  test_minimum_and_maximum_boundaries();
  return T64_TEST_FINISH("unit.core.rom-admission");
}
