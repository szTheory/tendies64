#include "tendies64/t64_machine.h"

#include "t64_sha256.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef T64_BUILD_ID
#define T64_BUILD_ID "unavailable"
#endif
#ifndef T64_SOURCE_REVISION
#define T64_SOURCE_REVISION "unrecorded"
#endif

#define T64_BASE_RAM_BYTES UINT32_C(4194304)
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
    .console_qualification = "unqualified-target",
    .tv_standard = T64_TV_NTSC,
    .region = T64_REGION_US,
    .media_kind = T64_MEDIA_CARTRIDGE,
    .rdram_bytes = T64_BASE_RAM_BYTES,
    .expansion_bytes = 0u,
    .peripheral_flags = 0u,
    .save_kind = T64_SAVE_NONE,
    .boot_route = "hle-pif+open-ipl3",
    .boot_status = "unimplemented",
    .guest_tick_unit = "model-rsp-cycle",
    .cpu_rsp_ratio_numerator = 1u,
    .cpu_rsp_ratio_denominator = 1u,
    .synchronization_quantum = 1u,
    .timing_tier = "skeleton-atomic",
    .scheduler_version = "fifo-v1",
    .pi_model_version = "pi_dma_skeleton_v1",
    .sp_model_version = "sp_dma_skeleton_v1",
    .rom_min_bytes = 4096u,
    .rom_max_bytes = 67108864u,
    .event_queue_capacity = 256u,
    .sequence_width_bits = 64u,
    .advance_event_budget = 4096u,
    .pi_duration_base_ticks = 1u,
    .pi_duration_bytes_divisor = 2u,
    .sp_duration_base_rsp_cycles = 12u,
    .sp_duration_bytes_divisor = 8u,
    .rom_order_policy = "canonical-only-v1",
    .event_order_policy = "tick-sequence-fifo-v1",
    .queue_exhaustion_policy = "reject-before-mutation-v1",
    .sequence_exhaustion_policy = "reject-before-mutation-v1",
    .advance_budget_policy = "bounded-4096-events-v1",
    .guest_time_bound_policy = "inclusive-idle-advance-v1",
    .halt_precedence_policy = "halt-at-bound-wins-v1",
    .pi_register_policy = "pi-register-skeleton-v1",
    .sp_register_policy = "sp-register-skeleton-v1",
    .device_error_policy = "sticky-pi-error-v1",
    .dma_cancel_policy = "token-match-complete-once-v1",
    .dma_visibility_policy = "atomic-source-at-completion-v1",
    .descriptor_version = 1u,
    .machine_model_version = 1u,
    .state_serialization_version = 1u,
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

typedef struct {
  uint8_t *bytes;
  size_t offset;
  t64_sha256_ctx *hash;
} profile_writer;

static bool profile_text_is_valid(const char *text, size_t capacity) {
  size_t length = bounded_text_length(text, capacity);

  return length > 0u && length < capacity;
}

static bool profile_fields_are_valid(const t64_profile_descriptor *profile) {
#define T64_PROFILE_TEXT_OK(field)                                            \
  profile_text_is_valid(profile->field, sizeof profile->field)
  return profile != NULL && T64_PROFILE_TEXT_OK(profile_id) &&
         T64_PROFILE_TEXT_OK(console_target) &&
         T64_PROFILE_TEXT_OK(console_qualification) &&
         T64_PROFILE_TEXT_OK(boot_route) && T64_PROFILE_TEXT_OK(boot_status) &&
         T64_PROFILE_TEXT_OK(guest_tick_unit) &&
         T64_PROFILE_TEXT_OK(timing_tier) &&
         T64_PROFILE_TEXT_OK(scheduler_version) &&
         T64_PROFILE_TEXT_OK(pi_model_version) &&
         T64_PROFILE_TEXT_OK(sp_model_version) &&
         T64_PROFILE_TEXT_OK(rom_order_policy) &&
         T64_PROFILE_TEXT_OK(event_order_policy) &&
         T64_PROFILE_TEXT_OK(queue_exhaustion_policy) &&
         T64_PROFILE_TEXT_OK(sequence_exhaustion_policy) &&
         T64_PROFILE_TEXT_OK(advance_budget_policy) &&
         T64_PROFILE_TEXT_OK(guest_time_bound_policy) &&
         T64_PROFILE_TEXT_OK(halt_precedence_policy) &&
         T64_PROFILE_TEXT_OK(pi_register_policy) &&
         T64_PROFILE_TEXT_OK(sp_register_policy) &&
         T64_PROFILE_TEXT_OK(device_error_policy) &&
         T64_PROFILE_TEXT_OK(dma_cancel_policy) &&
         T64_PROFILE_TEXT_OK(dma_visibility_policy) &&
         (profile->tv_standard == T64_TV_NTSC ||
          profile->tv_standard == T64_TV_PAL) &&
         (profile->region == T64_REGION_US ||
          profile->region == T64_REGION_EUROPE) &&
         profile->media_kind != 0u && profile->rdram_bytes > 0u &&
         profile->cpu_rsp_ratio_numerator > 0u &&
         profile->cpu_rsp_ratio_denominator > 0u &&
         profile->synchronization_quantum > 0u &&
         profile->rom_min_bytes >= 4096u &&
         profile->rom_max_bytes >= profile->rom_min_bytes &&
         profile->event_queue_capacity > 0u &&
         profile->sequence_width_bits > 0u &&
         profile->sequence_width_bits <= 64u &&
         profile->advance_event_budget > 0u &&
         profile->pi_duration_bytes_divisor > 0u &&
         profile->sp_duration_bytes_divisor > 0u &&
         profile->descriptor_version > 0u &&
         profile->machine_model_version > 0u &&
         profile->state_serialization_version > 0u;
#undef T64_PROFILE_TEXT_OK
}

static void profile_writer_bytes(profile_writer *writer, const uint8_t *bytes,
                                 size_t length) {
  if (writer->bytes != NULL) {
    (void)memcpy(writer->bytes + writer->offset, bytes, length);
  }
  if (writer->hash != NULL) {
    t64_sha256_update(writer->hash, bytes, length);
  }
  writer->offset += length;
}

static void profile_writer_u32(profile_writer *writer, uint32_t value) {
  uint8_t bytes[4];

  bytes[0] = (uint8_t)(value >> 24);
  bytes[1] = (uint8_t)(value >> 16);
  bytes[2] = (uint8_t)(value >> 8);
  bytes[3] = (uint8_t)value;
  profile_writer_bytes(writer, bytes, sizeof bytes);
}

static void profile_writer_text(profile_writer *writer, const char *text,
                                size_t capacity) {
  size_t length = bounded_text_length(text, capacity);

  profile_writer_u32(writer, (uint32_t)length);
  profile_writer_bytes(writer, (const uint8_t *)text, length);
}

static void write_profile(profile_writer *writer,
                          const t64_profile_descriptor *profile) {
  static const uint8_t domain[] = {'T', '6', '4', 'P', 'D', 'E', 'S', 'C'};

  profile_writer_bytes(writer, domain, sizeof domain);
  profile_writer_u32(writer, T64_PROFILE_SERIALIZATION_VERSION);
  profile_writer_text(writer, profile->profile_id,
                      sizeof profile->profile_id);
  profile_writer_text(writer, profile->console_target,
                      sizeof profile->console_target);
  profile_writer_text(writer, profile->console_qualification,
                      sizeof profile->console_qualification);
  profile_writer_u32(writer, profile->tv_standard);
  profile_writer_u32(writer, profile->region);
  profile_writer_u32(writer, profile->media_kind);
  profile_writer_u32(writer, profile->rdram_bytes);
  profile_writer_u32(writer, profile->expansion_bytes);
  profile_writer_u32(writer, profile->peripheral_flags);
  profile_writer_u32(writer, profile->save_kind);
  profile_writer_text(writer, profile->boot_route, sizeof profile->boot_route);
  profile_writer_text(writer, profile->boot_status,
                      sizeof profile->boot_status);
  profile_writer_text(writer, profile->guest_tick_unit,
                      sizeof profile->guest_tick_unit);
  profile_writer_u32(writer, profile->cpu_rsp_ratio_numerator);
  profile_writer_u32(writer, profile->cpu_rsp_ratio_denominator);
  profile_writer_u32(writer, profile->synchronization_quantum);
  profile_writer_text(writer, profile->timing_tier,
                      sizeof profile->timing_tier);
  profile_writer_text(writer, profile->scheduler_version,
                      sizeof profile->scheduler_version);
  profile_writer_text(writer, profile->pi_model_version,
                      sizeof profile->pi_model_version);
  profile_writer_text(writer, profile->sp_model_version,
                      sizeof profile->sp_model_version);
  profile_writer_u32(writer, profile->rom_min_bytes);
  profile_writer_u32(writer, profile->rom_max_bytes);
  profile_writer_u32(writer, profile->event_queue_capacity);
  profile_writer_u32(writer, profile->sequence_width_bits);
  profile_writer_u32(writer, profile->advance_event_budget);
  profile_writer_u32(writer, profile->pi_duration_base_ticks);
  profile_writer_u32(writer, profile->pi_duration_bytes_divisor);
  profile_writer_u32(writer, profile->sp_duration_base_rsp_cycles);
  profile_writer_u32(writer, profile->sp_duration_bytes_divisor);
  profile_writer_text(writer, profile->rom_order_policy,
                      sizeof profile->rom_order_policy);
  profile_writer_text(writer, profile->event_order_policy,
                      sizeof profile->event_order_policy);
  profile_writer_text(writer, profile->queue_exhaustion_policy,
                      sizeof profile->queue_exhaustion_policy);
  profile_writer_text(writer, profile->sequence_exhaustion_policy,
                      sizeof profile->sequence_exhaustion_policy);
  profile_writer_text(writer, profile->advance_budget_policy,
                      sizeof profile->advance_budget_policy);
  profile_writer_text(writer, profile->guest_time_bound_policy,
                      sizeof profile->guest_time_bound_policy);
  profile_writer_text(writer, profile->halt_precedence_policy,
                      sizeof profile->halt_precedence_policy);
  profile_writer_text(writer, profile->pi_register_policy,
                      sizeof profile->pi_register_policy);
  profile_writer_text(writer, profile->sp_register_policy,
                      sizeof profile->sp_register_policy);
  profile_writer_text(writer, profile->device_error_policy,
                      sizeof profile->device_error_policy);
  profile_writer_text(writer, profile->dma_cancel_policy,
                      sizeof profile->dma_cancel_policy);
  profile_writer_text(writer, profile->dma_visibility_policy,
                      sizeof profile->dma_visibility_policy);
  profile_writer_u32(writer, profile->descriptor_version);
  profile_writer_u32(writer, profile->machine_model_version);
  profile_writer_u32(writer, profile->state_serialization_version);
}

static size_t profile_serialized_size(const t64_profile_descriptor *profile) {
  profile_writer writer = {NULL, 0u, NULL};

  write_profile(&writer, profile);
  return writer.offset;
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

const char *t64_build_identity(void) { return T64_BUILD_ID; }

const char *t64_source_revision(void) { return T64_SOURCE_REVISION; }

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
  profile_writer writer = {NULL, 0u, NULL};
  t64_sha256_ctx context;

  if (digest_out == NULL || !profile_fields_are_valid(profile)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  t64_sha256_init(&context);
  writer.hash = &context;
  write_profile(&writer, profile);
  t64_sha256_final(&context, digest_out);
  return T64_STATUS_OK;
}

t64_status t64_profile_serialize(const t64_profile_descriptor *profile,
                                 uint8_t *bytes_out, size_t capacity,
                                 size_t *length_out) {
  profile_writer writer;
  size_t required;

  if (length_out == NULL || !profile_fields_are_valid(profile)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  required = profile_serialized_size(profile);
  if (required > T64_PROFILE_SERIALIZED_MAX_BYTES) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (bytes_out == NULL || capacity < required) {
    *length_out = required;
    return T64_STATUS_BUFFER_TOO_SMALL;
  }
  writer.bytes = bytes_out;
  writer.offset = 0u;
  writer.hash = NULL;
  write_profile(&writer, profile);
  *length_out = writer.offset;
  return T64_STATUS_OK;
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

  if (machine == NULL || bytes == NULL ||
      length < (size_t)machine->profile.rom_min_bytes ||
      length > (size_t)machine->profile.rom_max_bytes ||
      (length & 3u) != 0u ||
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

t64_status t64_machine_profile(
    const t64_machine *machine, const t64_profile_descriptor **profile_out) {
  if (profile_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  *profile_out = NULL;
  if (machine == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  *profile_out = &machine->profile;
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
