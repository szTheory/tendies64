#include "t64_internal.h"

#include "t64_sha256.h"

#include <string.h>

typedef enum {
  T64_ROM_ORDER_BIG_ENDIAN,
  T64_ROM_ORDER_BYTE_SWAPPED,
  T64_ROM_ORDER_WORD_REVERSED
} t64_rom_order;

static bool range_is_disjoint(t64_address_range candidate,
                              t64_host_storage storage, bool storage_present) {
  t64_address_range owned;

  if (!storage_present) {
    return true;
  }
  return t64_internal_make_range(storage.data, storage.capacity, &owned) &&
         !t64_internal_ranges_overlap(candidate, owned);
}

static bool candidate_is_disjoint(t64_address_range candidate,
                                  t64_host_storage machine_storage,
                                  t64_host_storage current_image_storage,
                                  bool has_current_image,
                                  t64_address_range input_range) {
  return range_is_disjoint(candidate, machine_storage, true) &&
         range_is_disjoint(candidate, current_image_storage,
                           has_current_image) &&
         !t64_internal_ranges_overlap(candidate, input_range);
}

static void release_invalid_distinct_storage(
    const t64_host_allocator *allocator, t64_host_storage storage,
    size_t required,
    t64_host_storage machine_storage, t64_host_storage current_image_storage,
    bool has_current_image, t64_address_range input_range) {
  t64_address_range returned_range;
  t64_address_range requested_range;

  if (storage.data != NULL && allocator != NULL && allocator->release != NULL &&
      t64_internal_make_range(storage.data, storage.capacity, &returned_range) &&
      t64_internal_make_range(storage.data, required, &requested_range) &&
      candidate_is_disjoint(returned_range, machine_storage,
                            current_image_storage, has_current_image,
                            input_range) &&
      candidate_is_disjoint(requested_range, machine_storage,
                            current_image_storage, has_current_image,
                            input_range)) {
    allocator->release(allocator->context, storage);
  }
}

static bool detect_order(const uint8_t *bytes, t64_rom_order *order_out) {
  static const uint8_t big_endian[4] = {0x80u, 0x37u, 0x12u, 0x40u};
  static const uint8_t byte_swapped[4] = {0x37u, 0x80u, 0x40u, 0x12u};
  static const uint8_t word_reversed[4] = {0x40u, 0x12u, 0x37u, 0x80u};

  if (memcmp(bytes, big_endian, sizeof big_endian) == 0) {
    *order_out = T64_ROM_ORDER_BIG_ENDIAN;
    return true;
  }
  if (memcmp(bytes, byte_swapped, sizeof byte_swapped) == 0) {
    *order_out = T64_ROM_ORDER_BYTE_SWAPPED;
    return true;
  }
  if (memcmp(bytes, word_reversed, sizeof word_reversed) == 0) {
    *order_out = T64_ROM_ORDER_WORD_REVERSED;
    return true;
  }
  return false;
}

static void normalize_image(const uint8_t *input, uint8_t *canonical,
                            size_t length, t64_rom_order order) {
  size_t i;

  if (order == T64_ROM_ORDER_BIG_ENDIAN) {
    (void)memcpy(canonical, input, length);
  } else if (order == T64_ROM_ORDER_BYTE_SWAPPED) {
    for (i = 0u; i < length; i += 2u) {
      canonical[i] = input[i + 1u];
      canonical[i + 1u] = input[i];
    }
  } else {
    for (i = 0u; i < length; i += 4u) {
      canonical[i] = input[i + 3u];
      canonical[i + 1u] = input[i + 2u];
      canonical[i + 2u] = input[i + 1u];
      canonical[i + 3u] = input[i];
    }
  }
}

static void parse_header(const uint8_t canonical[T64_ROM_HEADER_BYTES],
                         t64_rom_candidate *candidate) {
  size_t i;

  for (i = 0u; i < T64_ROM_HEADER_BYTES; i++) {
    candidate->header[i] = canonical[i];
  }
  for (i = 0u; i < T64_ROM_TITLE_BYTES; i++) {
    candidate->title[i] = canonical[0x20u + i];
  }
  for (i = 0u; i < T64_ROM_GAME_CODE_BYTES; i++) {
    candidate->game_code[i] = canonical[0x3bu + i];
  }
  candidate->version = canonical[0x3fu];
}

static void hash_image(const uint8_t *canonical, size_t length,
                       uint8_t digest_out[T64_DIGEST_SIZE]) {
  t64_sha256_ctx context;

  t64_sha256_init(&context);
  t64_sha256_update(&context, canonical, length);
  t64_sha256_final(&context, digest_out);
}

t64_status t64_rom_input_preflight(
    const t64_profile_descriptor *profile, const uint8_t *bytes,
    size_t length, t64_host_storage machine_storage,
    t64_host_storage current_image_storage, bool has_current_image,
    t64_address_range *input_range_out) {
  t64_address_range input_range;

  if (profile == NULL || bytes == NULL ||
      length < (size_t)profile->rom_min_bytes ||
      length > (size_t)profile->rom_max_bytes || (length & 3u) != 0u ||
      !t64_internal_make_range(bytes, length, &input_range)) {
    return T64_STATUS_INVALID_ROM;
  }
  if (!range_is_disjoint(input_range, machine_storage, true) ||
      !range_is_disjoint(input_range, current_image_storage,
                         has_current_image)) {
    return T64_STATUS_INVALID_ROM;
  }
  if (input_range_out != NULL) {
    *input_range_out = input_range;
  }
  return T64_STATUS_OK;
}

t64_status t64_rom_candidate_prepare(
    const t64_host_allocator *allocator,
    const t64_profile_descriptor *profile, const uint8_t *bytes,
    size_t length, t64_host_storage machine_storage,
    t64_host_storage current_image_storage, bool has_current_image,
    t64_rom_candidate *candidate_out) {
  t64_address_range input_range;
  t64_address_range candidate_range;
  t64_rom_order order;
  t64_host_storage storage = {NULL, 0u, 0u};

  if (candidate_out == NULL || allocator == NULL || allocator->allocate == NULL ||
      allocator->release == NULL || profile == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  (void)memset(candidate_out, 0, sizeof *candidate_out);
  if (t64_rom_input_preflight(profile, bytes, length, machine_storage,
                              current_image_storage, has_current_image,
                              &input_range) != T64_STATUS_OK) {
    return T64_STATUS_INVALID_ROM;
  }
  if (!detect_order(bytes, &order)) {
    return T64_STATUS_INVALID_ROM;
  }

  if (!allocator->allocate(allocator->context, length, 1u, &storage)) {
    return T64_STATUS_ALLOCATION_FAILED;
  }
  if (!t64_internal_storage_range(storage, length, 1u, &candidate_range) ||
      (storage.alignment > 0u &&
       ((uintptr_t)storage.data % storage.alignment) != 0u) ||
      !candidate_is_disjoint(candidate_range, machine_storage,
                             current_image_storage, has_current_image,
                             input_range)) {
    release_invalid_distinct_storage(allocator, storage, length,
                                     machine_storage,
                                     current_image_storage, has_current_image,
                                     input_range);
    return T64_STATUS_BAD_STORAGE;
  }

  normalize_image(bytes, (uint8_t *)storage.data, length, order);
  candidate_out->storage = storage;
  candidate_out->length = length;
  candidate_out->owns_storage = true;
  parse_header((const uint8_t *)storage.data, candidate_out);
  hash_image((const uint8_t *)storage.data, length, candidate_out->digest);
  return T64_STATUS_OK;
}

void t64_rom_candidate_discard(const t64_host_allocator *allocator,
                               t64_rom_candidate *candidate) {
  if (allocator == NULL || allocator->release == NULL || candidate == NULL) {
    return;
  }
  if (candidate->owns_storage) {
    allocator->release(allocator->context, candidate->storage);
  }
  (void)memset(candidate, 0, sizeof *candidate);
}
