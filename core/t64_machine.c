#include "tendies64/t64_machine.h"

#include "t64_sha256.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define T64_BASE_RAM_BYTES UINT32_C(4194304)
#define T64_ROM_MIN_BYTES ((size_t)4096u)
#define T64_ROM_MAX_BYTES ((size_t)67108864u)
#define T64_PROFILE_HASH_VERSION UINT32_C(1)
#define T64_STATE_HASH_VERSION UINT32_C(1)

struct t64_machine {
  t64_host_allocator allocator;
  t64_host_storage machine_storage;
  t64_host_storage image_storage;
  t64_profile_descriptor profile;
  uint8_t profile_digest[T64_DIGEST_SIZE];
  uint8_t content_digest[T64_DIGEST_SIZE];
  uint8_t *ram;
  size_t image_length;
  uint64_t tick;
  bool has_image;
};

static const t64_profile_descriptor ntsc_cart_4m_profile = {
    .profile_id = "ntsc-cart-4m-skeleton",
    .console_target = "NUS-CPU-03",
    .tv_standard = T64_TV_NTSC,
    .region = T64_REGION_US,
    .media_kind = T64_MEDIA_CARTRIDGE,
    .rdram_bytes = T64_BASE_RAM_BYTES,
    .expansion_bytes = 0u,
    .peripheral_flags = 0u,
    .save_kind = T64_SAVE_NONE,
    .boot_route = "hle-pif+open-ipl3",
    .boot_status = "unimplemented",
};

typedef struct {
  uintptr_t begin;
  uintptr_t end;
} address_range;

static bool make_range(const void *pointer, size_t length,
                       address_range *range_out) {
  uintptr_t begin;

  if (pointer == NULL || range_out == NULL) {
    return false;
  }
  begin = (uintptr_t)pointer;
  if (length > (size_t)(UINTPTR_MAX - begin)) {
    return false;
  }
  range_out->begin = begin;
  range_out->end = begin + (uintptr_t)length;
  return true;
}

static bool ranges_overlap(address_range left, address_range right) {
  return left.begin < right.end && right.begin < left.end;
}

static bool storage_is_valid(t64_host_storage storage, size_t required,
                             size_t alignment, address_range *range_out) {
  if (storage.data == NULL || storage.capacity < required ||
      storage.alignment < alignment || alignment == 0u ||
      ((uintptr_t)storage.data % alignment) != 0u ||
      !make_range(storage.data, storage.capacity, range_out)) {
    return false;
  }
  return true;
}

static size_t bounded_text_length(const char *text, size_t capacity) {
  size_t length = 0u;

  while (length < capacity && text[length] != '\0') {
    length++;
  }
  return length;
}

static void hash_u32(t64_sha256_ctx *context, uint32_t value) {
  uint8_t bytes[4];

  bytes[0] = (uint8_t)(value >> 24);
  bytes[1] = (uint8_t)(value >> 16);
  bytes[2] = (uint8_t)(value >> 8);
  bytes[3] = (uint8_t)value;
  t64_sha256_update(context, bytes, sizeof bytes);
}

static void hash_u64(t64_sha256_ctx *context, uint64_t value) {
  uint8_t bytes[8];
  unsigned i;

  for (i = 0u; i < 8u; i++) {
    unsigned shift = 56u - (i * 8u);
    bytes[i] = (uint8_t)(value >> shift);
  }
  t64_sha256_update(context, bytes, sizeof bytes);
}

static bool hash_profile_text(t64_sha256_ctx *context, const char *text,
                              size_t capacity) {
  size_t length;

  if (text == NULL) {
    return false;
  }
  length = bounded_text_length(text, capacity);
  if (length == 0u || length >= capacity) {
    return false;
  }
  hash_u32(context, (uint32_t)length);
  t64_sha256_update(context, text, length);
  return true;
}

static bool profile_fields_are_valid(const t64_profile_descriptor *profile) {
  return profile != NULL &&
         bounded_text_length(profile->profile_id,
                             sizeof profile->profile_id) > 0u &&
         bounded_text_length(profile->profile_id,
                             sizeof profile->profile_id) <
             sizeof profile->profile_id &&
         bounded_text_length(profile->console_target,
                             sizeof profile->console_target) > 0u &&
         bounded_text_length(profile->console_target,
                             sizeof profile->console_target) <
             sizeof profile->console_target &&
         bounded_text_length(profile->boot_route,
                             sizeof profile->boot_route) > 0u &&
         bounded_text_length(profile->boot_route,
                             sizeof profile->boot_route) <
             sizeof profile->boot_route &&
         bounded_text_length(profile->boot_status,
                             sizeof profile->boot_status) > 0u &&
         bounded_text_length(profile->boot_status,
                             sizeof profile->boot_status) <
             sizeof profile->boot_status &&
         (profile->tv_standard == T64_TV_NTSC ||
          profile->tv_standard == T64_TV_PAL) &&
         (profile->region == T64_REGION_US ||
          profile->region == T64_REGION_EUROPE) &&
         profile->media_kind == T64_MEDIA_CARTRIDGE &&
         profile->rdram_bytes > 0u;
}

static void hash_rom(const uint8_t *bytes, size_t length,
                     uint8_t digest_out[T64_DIGEST_SIZE]) {
  t64_sha256_ctx context;

  t64_sha256_init(&context);
  t64_sha256_update(&context, bytes, length);
  t64_sha256_final(&context, digest_out);
}

static bool storage_disjoint_from_machine(const t64_machine *machine,
                                          address_range candidate) {
  address_range owned;

  if (!make_range(machine->machine_storage.data,
                  machine->machine_storage.capacity, &owned) ||
      ranges_overlap(candidate, owned)) {
    return false;
  }
  if (machine->has_image &&
      (!make_range(machine->image_storage.data,
                   machine->image_storage.capacity, &owned) ||
       ranges_overlap(candidate, owned))) {
    return false;
  }
  return true;
}

static bool input_disjoint_from_machine(const t64_machine *machine,
                                        address_range input) {
  address_range owned;

  if (!make_range(machine->machine_storage.data,
                  machine->machine_storage.capacity, &owned) ||
      ranges_overlap(input, owned)) {
    return false;
  }
  if (machine->has_image &&
      (!make_range(machine->image_storage.data,
                   machine->image_storage.capacity, &owned) ||
       ranges_overlap(input, owned))) {
    return false;
  }
  return true;
}

t64_status t64_profile_lookup(const char *profile_id,
                              const t64_profile_descriptor **profile_out) {
  static const char known_profile_id[] = "ntsc-cart-4m-skeleton";
  size_t i;

  if (profile_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  *profile_out = NULL;
  if (profile_id == NULL || profile_id[0] == '\0') {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  for (i = 0u;; i++) {
    if (profile_id[i] != known_profile_id[i]) {
      return T64_STATUS_UNKNOWN_PROFILE;
    }
    if (known_profile_id[i] == '\0') {
      *profile_out = &ntsc_cart_4m_profile;
      return T64_STATUS_OK;
    }
  }
}

t64_status t64_profile_hash(const t64_profile_descriptor *profile,
                            uint8_t digest_out[T64_DIGEST_SIZE]) {
  static const uint8_t domain[] = {'t', '6', '4', '.', 'p', 'r', 'o', 'f',
                                   'i', 'l', 'e', '.', 'v', '1'};
  t64_sha256_ctx context;

  if (digest_out == NULL || !profile_fields_are_valid(profile)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  t64_sha256_init(&context);
  t64_sha256_update(&context, domain, sizeof domain);
  hash_u32(&context, T64_PROFILE_HASH_VERSION);
  (void)hash_profile_text(&context, profile->profile_id,
                          sizeof profile->profile_id);
  (void)hash_profile_text(&context, profile->console_target,
                          sizeof profile->console_target);
  hash_u32(&context, profile->tv_standard);
  hash_u32(&context, profile->region);
  hash_u32(&context, profile->media_kind);
  hash_u32(&context, profile->rdram_bytes);
  hash_u32(&context, profile->expansion_bytes);
  hash_u32(&context, profile->peripheral_flags);
  hash_u32(&context, profile->save_kind);
  (void)hash_profile_text(&context, profile->boot_route,
                          sizeof profile->boot_route);
  (void)hash_profile_text(&context, profile->boot_status,
                          sizeof profile->boot_status);
  t64_sha256_final(&context, digest_out);
  return T64_STATUS_OK;
}

t64_status t64_profile_serialize(const t64_profile_descriptor *profile,
                                 uint8_t *bytes_out, size_t capacity,
                                 size_t *length_out) {
  (void)profile;
  (void)bytes_out;
  (void)capacity;
  (void)length_out;
  return T64_STATUS_INVALID_ARGUMENT;
}

t64_status t64_machine_create(const t64_host_allocator *allocator,
                              const t64_profile_descriptor *profile,
                              t64_machine **machine_out) {
  t64_host_storage storage = {NULL, 0u, 0u};
  address_range allocation_range;
  uint8_t profile_digest[T64_DIGEST_SIZE];
  size_t total_size;
  size_t alignment = _Alignof(max_align_t);
  t64_machine *machine;

  if (machine_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  *machine_out = NULL;
  if (allocator == NULL || allocator->allocate == NULL ||
      allocator->release == NULL || !profile_fields_are_valid(profile) ||
      t64_profile_hash(profile, profile_digest) != T64_STATUS_OK ||
      profile->rdram_bytes > (uint32_t)(SIZE_MAX - sizeof(t64_machine))) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  total_size = sizeof(t64_machine) + (size_t)profile->rdram_bytes;
  if (!allocator->allocate(allocator->context, total_size, alignment,
                           &storage)) {
    return T64_STATUS_ALLOCATION_FAILED;
  }
  if (!storage_is_valid(storage, total_size, alignment, &allocation_range)) {
    if (storage.data != NULL &&
        make_range(storage.data, storage.capacity, &allocation_range)) {
      allocator->release(allocator->context, storage);
    }
    return T64_STATUS_BAD_STORAGE;
  }
  machine = (t64_machine *)storage.data;
  (void)memset(machine, 0, sizeof *machine);
  machine->allocator = *allocator;
  machine->machine_storage = storage;
  (void)memcpy(&machine->profile, profile, sizeof machine->profile);
  (void)memcpy(machine->profile_digest, profile_digest,
               sizeof machine->profile_digest);
  machine->ram = (uint8_t *)storage.data + sizeof *machine;
  (void)memset(machine->ram, 0, profile->rdram_bytes);
  *machine_out = machine;
  return T64_STATUS_OK;
}

void t64_machine_destroy(t64_machine *machine) {
  t64_host_allocator allocator;
  t64_host_storage machine_storage;

  if (machine == NULL) {
    return;
  }
  allocator = machine->allocator;
  machine_storage = machine->machine_storage;
  if (machine->has_image) {
    allocator.release(allocator.context, machine->image_storage);
  }
  allocator.release(allocator.context, machine_storage);
}

t64_status t64_machine_admit_rom(t64_machine *machine, const uint8_t *bytes,
                                 size_t length) {
  static const uint8_t canonical_magic[4] = {0x80u, 0x37u, 0x12u, 0x40u};
  static const uint8_t byte_swapped_magic[4] = {0x37u, 0x80u, 0x40u, 0x12u};
  static const uint8_t little_endian_magic[4] = {0x40u, 0x12u, 0x37u, 0x80u};
  t64_host_storage candidate = {NULL, 0u, 0u};
  t64_host_storage previous;
  address_range input_range;
  address_range candidate_range;
  size_t i;
  bool canonical = true;

  if (machine == NULL || bytes == NULL || length < T64_ROM_MIN_BYTES ||
      length > T64_ROM_MAX_BYTES || (length & 3u) != 0u ||
      !make_range(bytes, length, &input_range)) {
    return T64_STATUS_INVALID_ROM;
  }
  if (!input_disjoint_from_machine(machine, input_range)) {
    return T64_STATUS_INVALID_ROM;
  }
  for (i = 0u; i < sizeof canonical_magic; i++) {
    if (bytes[i] != canonical_magic[i]) {
      canonical = false;
      break;
    }
  }
  if (!canonical) {
    bool known_other_order = true;
    bool known_little_order = true;

    for (i = 0u; i < sizeof byte_swapped_magic; i++) {
      if (bytes[i] != byte_swapped_magic[i]) {
        known_other_order = false;
      }
      if (bytes[i] != little_endian_magic[i]) {
        known_little_order = false;
      }
    }
    return (known_other_order || known_little_order)
               ? T64_STATUS_UNSUPPORTED_ROM_ORDER
               : T64_STATUS_INVALID_ROM;
  }
  if (!machine->allocator.allocate(machine->allocator.context, length, 1u,
                                   &candidate)) {
    return T64_STATUS_ALLOCATION_FAILED;
  }
  if (!storage_is_valid(candidate, length, 1u, &candidate_range) ||
      !storage_disjoint_from_machine(machine, candidate_range) ||
      ranges_overlap(candidate_range, input_range)) {
    address_range release_range;
    bool safe_to_release =
        candidate.data != NULL &&
        make_range(candidate.data, candidate.capacity, &release_range) &&
        input_disjoint_from_machine(machine, release_range) &&
        !ranges_overlap(release_range, input_range);

    if (safe_to_release) {
      machine->allocator.release(machine->allocator.context, candidate);
    }
    return T64_STATUS_BAD_STORAGE;
  }

  (void)memcpy(candidate.data, bytes, length);
  hash_rom((const uint8_t *)candidate.data, length,
           machine->content_digest);
  previous = machine->image_storage;
  machine->image_storage = candidate;
  machine->image_length = length;
  machine->has_image = true;
  machine->tick = 0u;
  (void)memset(machine->ram, 0, machine->profile.rdram_bytes);
  if (previous.data != NULL) {
    machine->allocator.release(machine->allocator.context, previous);
  }
  return T64_STATUS_OK;
}

t64_status t64_machine_advance(t64_machine *machine,
                               const t64_stop_request *request,
                               t64_stop_result *result_out) {
  if (machine == NULL || request == NULL || result_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!machine->has_image) {
    return T64_STATUS_NOT_READY;
  }
  if (request->kind != T64_STOP_GUEST_TICK) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (request->value < machine->tick) {
    return T64_STATUS_TIME_REVERSED;
  }
  machine->tick = request->value;
  result_out->reason = T64_STOP_REASON_BOUND;
  result_out->tick = machine->tick;
  return T64_STATUS_OK;
}

t64_status t64_machine_content_digest(
    const t64_machine *machine, uint8_t digest_out[T64_DIGEST_SIZE]) {
  if (machine == NULL || digest_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!machine->has_image) {
    return T64_STATUS_NOT_READY;
  }
  (void)memcpy(digest_out, machine->content_digest,
               sizeof machine->content_digest);
  return T64_STATUS_OK;
}

t64_status t64_machine_digest(const t64_machine *machine,
                              uint8_t digest_out[T64_DIGEST_SIZE]) {
  static const uint8_t domain[] = {'t', '6', '4', '.', 's', 't', 'a', 't',
                                   'e', '.', 'v', '1'};
  t64_sha256_ctx context;

  if (machine == NULL || digest_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!machine->has_image) {
    return T64_STATUS_NOT_READY;
  }
  t64_sha256_init(&context);
  t64_sha256_update(&context, domain, sizeof domain);
  hash_u32(&context, T64_STATE_HASH_VERSION);
  t64_sha256_update(&context, machine->profile_digest,
                    sizeof machine->profile_digest);
  t64_sha256_update(&context, machine->content_digest,
                    sizeof machine->content_digest);
  hash_u64(&context, machine->tick);
  hash_u32(&context, (uint32_t)machine->image_length);
  hash_u32(&context, machine->profile.rdram_bytes);
  t64_sha256_update(&context, machine->ram, machine->profile.rdram_bytes);
  t64_sha256_final(&context, digest_out);
  return T64_STATUS_OK;
}
