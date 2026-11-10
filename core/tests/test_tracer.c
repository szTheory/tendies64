#line 1 "core/tests/test_tracer.c"
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
  image_bad_order[0] = 0x37u;
  image_bad_order[1] = 0x80u;
  image_bad_order[2] = 0x40u;
  image_bad_order[3] = 0x12u;
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
                T64_STATUS_UNSUPPORTED_ROM_ORDER);
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
  test_machine_tracer();
  test_allocator_storage_validation();
  return T64_TEST_FINISH("unit.core.tracer");
}
