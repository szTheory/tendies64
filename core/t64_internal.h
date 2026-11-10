#ifndef TENDIES64_T64_INTERNAL_H
#define TENDIES64_T64_INTERNAL_H

#include "tendies64/t64_machine.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define T64_ROM_HEADER_BYTES T64_ROM_HEADER_SIZE
#define T64_ROM_TITLE_BYTES 20u
#define T64_ROM_GAME_CODE_BYTES 4u

typedef struct {
  uintptr_t begin;
  uintptr_t end;
} t64_address_range;

typedef struct {
  t64_host_storage storage;
  size_t length;
  uint8_t header[T64_ROM_HEADER_BYTES];
  uint8_t title[T64_ROM_TITLE_BYTES];
  uint8_t game_code[T64_ROM_GAME_CODE_BYTES];
  uint8_t version;
  uint8_t digest[T64_DIGEST_SIZE];
  bool owns_storage;
} t64_rom_candidate;

static inline bool t64_internal_make_range(const void *pointer, size_t length,
                                           t64_address_range *range_out) {
  uintptr_t begin;

  if (pointer == NULL || range_out == NULL ||
      (uintmax_t)length > (uintmax_t)UINTPTR_MAX -
                              (uintmax_t)(uintptr_t)pointer) {
    return false;
  }
  begin = (uintptr_t)pointer;
  range_out->begin = begin;
  range_out->end = begin + (uintptr_t)length;
  return true;
}

static inline bool t64_internal_ranges_overlap(t64_address_range left,
                                               t64_address_range right) {
  return left.begin < right.end && right.begin < left.end;
}

static inline bool t64_internal_storage_range(t64_host_storage storage,
                                              size_t required,
                                              size_t alignment,
                                              t64_address_range *range_out) {
  return storage.data != NULL && storage.capacity >= required &&
         alignment > 0u && storage.alignment >= alignment &&
         ((uintptr_t)storage.data % alignment) == 0u &&
         t64_internal_make_range(storage.data, storage.capacity, range_out);
}

t64_status t64_rom_candidate_prepare(
    const t64_host_allocator *allocator,
    const t64_profile_descriptor *profile, const uint8_t *bytes,
    size_t length, t64_host_storage machine_storage,
    t64_host_storage current_image_storage, bool has_current_image,
    t64_rom_candidate *candidate_out);
void t64_rom_candidate_discard(const t64_host_allocator *allocator,
                               t64_rom_candidate *candidate);

#endif /* TENDIES64_T64_INTERNAL_H */
