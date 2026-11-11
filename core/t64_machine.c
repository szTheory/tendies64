#include "tendies64/t64_machine.h"

#include "t64_internal.h"
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
    .rom_order_policy = "all-known-orders-to-big-endian-v1",
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
    .state_serialization_version = 5u,
};

#define T64_OPENIPL3_ASSET_SHA256_BYTES                                      \
  {                                                                          \
    0xf5u, 0x22u, 0xdbu, 0x2eu, 0x31u, 0xa7u, 0x01u, 0xf8u,                  \
        0x25u, 0x97u, 0xf3u, 0x99u, 0xe7u, 0x6du, 0x55u, 0xc9u,              \
        0x48u, 0x77u, 0x60u, 0xd0u, 0x15u, 0xafu, 0xf9u, 0xd9u,              \
        0x01u, 0x76u, 0xb4u, 0x63u, 0xaeu, 0x39u, 0xa0u, 0x68u               \
  }
#define T64_OPENIPL3_PAYLOAD_SHA256_BYTES                                    \
  {                                                                          \
    0xf7u, 0x16u, 0xa9u, 0x0bu, 0x1du, 0x36u, 0xecu, 0x42u,                  \
        0x53u, 0xd0u, 0x96u, 0x79u, 0x07u, 0x91u, 0x25u, 0x6du,              \
        0x35u, 0x43u, 0x45u, 0x5eu, 0x54u, 0x47u, 0x3au, 0x75u,              \
        0x5fu, 0xf8u, 0xc3u, 0xd1u, 0x19u, 0x9bu, 0x15u, 0xd5u               \
  }
#define T64_OPENIPL3_FLAT_ENTRY_POLICY "exact-source-or-flat-payload-z64-v2"
#define T64_OPENIPL3_ROM_LIMIT UINT32_C(0x00100000)
#define T64_OPENIPL3_AUTHORED_PAYLOAD_OFFSET UINT32_C(0x00001000)
#define T64_OPENIPL3_POLICY_SHA256_BYTES                                     \
  {                                                                          \
    0xc6u, 0x4cu, 0xfcu, 0xfdu, 0xbdu, 0x3cu, 0xcdu, 0xd0u,                  \
        0x8fu, 0xdau, 0xfbu, 0xc6u, 0x35u, 0x39u, 0xafu, 0x5bu,              \
        0x80u, 0x26u, 0xcau, 0xebu, 0x70u, 0xfau, 0xd1u, 0x73u,              \
        0xbeu, 0xe9u, 0x33u, 0xa9u, 0x3eu, 0x96u, 0xb1u, 0x41u               \
  }

static const t64_profile_descriptor ntsc_cart_4m_openipl3_warm_profile = {
    .profile_id = "ntsc-cart-4m-openipl3-warm-v1",
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
    .boot_status = "prepared-warm-entry-partial",
    .guest_tick_unit = "model-rsp-cycle",
    .cpu_rsp_ratio_numerator = 1u,
    .cpu_rsp_ratio_denominator = 1u,
    .synchronization_quantum = 1u,
    .timing_tier = "partial-warm-entry",
    .scheduler_version = "tick-sequence-fifo-v1",
    .pi_model_version = "pi_dma_bounded_cart_v1",
    .sp_model_version = "sp_dma_bounded_warm_v1",
    .rom_min_bytes = 4096u,
    .rom_max_bytes = T64_OPENIPL3_ROM_LIMIT,
    .event_queue_capacity = 256u,
    .sequence_width_bits = 64u,
    .advance_event_budget = 4096u,
    .pi_duration_base_ticks = 1u,
    .pi_duration_bytes_divisor = 2u,
    .sp_duration_base_rsp_cycles = 12u,
    .sp_duration_bytes_divisor = 8u,
    .rom_order_policy = T64_OPENIPL3_FLAT_ENTRY_POLICY,
    .event_order_policy = "tick-sequence-fifo-v1",
    .queue_exhaustion_policy = "reject-before-mutation-v1",
    .sequence_exhaustion_policy = "reject-before-mutation-v1",
    .advance_budget_policy = "bounded-4096-events-v1",
    .guest_time_bound_policy = "inclusive-idle-advance-v1",
    .halt_precedence_policy = "halt-at-bound-wins-v1",
    .pi_register_policy = "bounded-pi-register-and-cart-dma-v1",
    .sp_register_policy = "bounded-sp-register-reset-and-dma-v1",
    .device_error_policy = "sticky-pi-error-v1",
    .dma_cancel_policy = "token-match-complete-once-v1",
    .dma_visibility_policy = "atomic-source-at-completion-v1",
    .descriptor_version = 2u,
    .machine_model_version = 2u,
    .state_serialization_version = 7u,
    .asset_sha256 = T64_OPENIPL3_ASSET_SHA256_BYTES,
    .payload_sha256 = T64_OPENIPL3_PAYLOAD_SHA256_BYTES,
    .boot_policy_sha256 = T64_OPENIPL3_POLICY_SHA256_BYTES,
    .hle_seed_reset_version = 1u,
};

static const uint8_t openipl3_asset_sha256[T64_DIGEST_SIZE] =
    T64_OPENIPL3_ASSET_SHA256_BYTES;
static const uint8_t openipl3_payload_sha256[T64_DIGEST_SIZE] =
    T64_OPENIPL3_PAYLOAD_SHA256_BYTES;
static const uint8_t openipl3_policy_sha256[T64_DIGEST_SIZE] =
    T64_OPENIPL3_POLICY_SHA256_BYTES;

static size_t bounded_text_length(const char *text, size_t capacity) {
  size_t length = 0u;

  while (length < capacity && text[length] != '\0') {
    length++;
  }
  return length;
}

static bool has_openipl3_profile_id(const t64_profile_descriptor *profile) {
  static const char profile_id[] = "ntsc-cart-4m-openipl3-warm-v1";

  return profile != NULL &&
         bounded_text_length(profile->profile_id, sizeof profile->profile_id) ==
             sizeof profile_id - 1u &&
         memcmp(profile->profile_id, profile_id, sizeof profile_id - 1u) == 0;
}

static bool is_openipl3_profile(const t64_profile_descriptor *profile) {
  uint8_t candidate_digest[T64_DIGEST_SIZE];
  uint8_t canonical_digest[T64_DIGEST_SIZE];

  if (profile == NULL || profile->descriptor_version != 2u ||
      !has_openipl3_profile_id(profile) ||
      t64_profile_hash(profile, candidate_digest) != T64_STATUS_OK ||
      t64_profile_hash(&ntsc_cart_4m_openipl3_warm_profile,
                       canonical_digest) != T64_STATUS_OK) {
    return false;
  }
  return memcmp(candidate_digest, canonical_digest,
                sizeof candidate_digest) == 0;
}

static uint32_t read_be32(const uint8_t *bytes) {
  return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
         ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

static bool openipl3_source_asset_matches(const uint8_t *bytes,
                                          size_t length) {
  t64_sha256_ctx context;
  uint8_t digest[T64_DIGEST_SIZE];

  if (bytes == NULL || length != 4096u) {
    return false;
  }
  t64_sha256_init(&context);
  t64_sha256_update(&context, bytes, length);
  t64_sha256_final(&context, digest);
  return memcmp(digest, openipl3_asset_sha256, sizeof digest) == 0;
}

static bool openipl3_authored_flat_rom_matches(const uint8_t *bytes,
                                               size_t length) {
  static const uint8_t z64_magic[4] = {0x80u, 0x37u, 0x12u, 0x40u};
  t64_sha256_ctx context;
  uint8_t digest[T64_DIGEST_SIZE];
  uint32_t entry_pc;
  uint32_t load_bytes;
  uint32_t physical_entry;
  size_t flat_length;

  if (bytes == NULL || length <= T64_OPENIPL3_AUTHORED_PAYLOAD_OFFSET ||
      length > (size_t)T64_OPENIPL3_ROM_LIMIT || (length & 3u) != 0u ||
      memcmp(bytes, z64_magic, sizeof z64_magic) != 0) {
    return false;
  }
  t64_sha256_init(&context);
  t64_sha256_update(&context, bytes + UINT32_C(0x40), UINT32_C(0x0fc0));
  t64_sha256_final(&context, digest);
  if (memcmp(digest, openipl3_payload_sha256, sizeof digest) != 0) {
    return false;
  }

  entry_pc = read_be32(bytes + UINT32_C(0x08));
  load_bytes = read_be32(bytes + UINT32_C(0x10));
  physical_entry = entry_pc & UINT32_C(0x1fffffff);
  flat_length = length - (size_t)T64_OPENIPL3_AUTHORED_PAYLOAD_OFFSET;
  return (entry_pc & UINT32_C(0xe0000000)) == UINT32_C(0x80000000) &&
         (entry_pc & UINT32_C(3)) == 0u && load_bytes > 0u &&
         (size_t)load_bytes == flat_length &&
         physical_entry < T64_BASE_RAM_BYTES &&
         (uint64_t)physical_entry + (uint64_t)load_bytes <=
             (uint64_t)T64_BASE_RAM_BYTES;
}

static bool openipl3_asset_matches(const uint8_t *bytes, size_t length) {
  return openipl3_source_asset_matches(bytes, length) ||
         openipl3_authored_flat_rom_matches(bytes, length);
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
  if (profile->descriptor_version == 2u) {
    profile_writer_bytes(writer, profile->asset_sha256,
                         sizeof profile->asset_sha256);
    profile_writer_bytes(writer, profile->payload_sha256,
                         sizeof profile->payload_sha256);
    profile_writer_bytes(writer, profile->boot_policy_sha256,
                         sizeof profile->boot_policy_sha256);
    profile_writer_u32(writer, profile->hle_seed_reset_version);
  }
}

static size_t profile_serialized_size(const t64_profile_descriptor *profile) {
  profile_writer writer = {NULL, 0u, NULL};

  write_profile(&writer, profile);
  return writer.offset;
}

static bool range_disjoint_from_object(t64_address_range candidate,
                                       const void *object, size_t size) {
  t64_address_range object_range;

  return t64_internal_make_range(object, size, &object_range) &&
         !t64_internal_ranges_overlap(candidate, object_range);
}

static bool machine_storage_disjoint_from_inputs(
    t64_address_range storage_range,
    const t64_host_allocator *allocator,
    const t64_profile_descriptor *profile, t64_machine **machine_out) {
  return range_disjoint_from_object(storage_range, allocator,
                                    sizeof *allocator) &&
         range_disjoint_from_object(storage_range, profile, sizeof *profile) &&
         range_disjoint_from_object(storage_range, machine_out,
                                    sizeof *machine_out);
}

static void release_bad_machine_storage(
    const t64_host_allocator *allocator, t64_host_storage storage,
    size_t required, const t64_profile_descriptor *profile,
    t64_machine **machine_out) {
  t64_address_range returned_range;
  t64_address_range requested_range;

  if (storage.data != NULL && allocator->release != NULL &&
      t64_internal_make_range(storage.data, storage.capacity,
                              &returned_range) &&
      t64_internal_make_range(storage.data, required, &requested_range) &&
      machine_storage_disjoint_from_inputs(returned_range, allocator, profile,
                                           machine_out) &&
      machine_storage_disjoint_from_inputs(requested_range, allocator,
                                           profile, machine_out)) {
    allocator->release(allocator->context, storage);
  }
}

void t64_machine_reset_execution_state(t64_machine *machine) {
  t64_pi_reset_machine_state(machine);
  t64_sp_reset_machine_state(machine);
  machine->tick = 0u;
  machine->instruction_count = 0u;
  machine->frame_event_count = 0u;
  machine->sticky_error_status = T64_STATUS_OK;
  machine->halted = false;
  machine->queue_head = 0u;
  machine->queue_count = 0u;
  machine->next_sequence = 0u;
  machine->sequence_exhausted = false;
}

static t64_status prepare_openipl3_warm_entry(t64_machine *machine) {
  const uint8_t *image = (const uint8_t *)machine->image_storage.data;

  machine->boot.source_offset = UINT32_C(0x40);
  machine->boot.payload_length = UINT32_C(0xfc0);
  machine->boot.dmem_offset = UINT32_C(0x40);
  machine->boot.entry_pc = UINT32_C(0xa4000040);
  machine->boot.seed_reset_version = UINT32_C(1);
  machine->boot.seed_s3_rom_type = UINT32_C(0);
  machine->boot.seed_s4_tv_type = UINT32_C(1);
  machine->boot.seed_s5_reset_type = UINT32_C(1);
  machine->boot.seed_s6_entropy = UINT32_C(0);
  machine->boot.seed_s7_boot_info_version = UINT32_C(1);
  (void)memcpy(machine->boot.asset_sha256, openipl3_asset_sha256,
               sizeof machine->boot.asset_sha256);
  (void)memcpy(machine->boot.payload_sha256, openipl3_payload_sha256,
               sizeof machine->boot.payload_sha256);
  (void)memcpy(machine->boot.policy_sha256, openipl3_policy_sha256,
               sizeof machine->boot.policy_sha256);
  (void)memcpy(machine->sp_dmem + machine->boot.dmem_offset,
               image + machine->boot.source_offset,
               machine->boot.payload_length);
  machine->cpu.gpr[19] = machine->boot.seed_s3_rom_type;
  machine->cpu.gpr[20] = machine->boot.seed_s4_tv_type;
  machine->cpu.gpr[21] = machine->boot.seed_s5_reset_type;
  machine->cpu.gpr[22] = machine->boot.seed_s6_entropy;
  machine->cpu.gpr[23] = machine->boot.seed_s7_boot_info_version;
  machine->cpu.pc = machine->boot.entry_pc;
  machine->cache.policy_version = UINT32_C(1);
  machine->guest_devices.policy_version = T64_WARM_DEVICE_POLICY_VERSION;
  machine->guest_devices.mi_version = 0u;
  machine->guest_devices.ri_select = 1u;
  machine->guest_devices.mi_version_known = true;
  machine->guest_devices.ri_select_known = true;
  machine->output.policy_version = T64_ISVIEWER_POLICY_VERSION;
  machine->boot.prepared = true;
  t64_trace_emit(machine, machine->tick, T64_TRACE_DEVICE_MACHINE,
                 T64_TRACE_KIND_CPU_ENTRY, machine->cpu.pc);
  return t64_cpu_schedule(machine);
}

static t64_status reset_guest_state(t64_machine *machine) {
  t64_machine_reset_execution_state(machine);
  (void)memset(machine->ram, 0, machine->profile.rdram_bytes);
  (void)memset(machine->sp_dmem, 0, sizeof machine->sp_dmem);
  (void)memset(machine->sp_imem, 0, sizeof machine->sp_imem);
  (void)memset(&machine->cpu, 0, sizeof machine->cpu);
  (void)memset(&machine->boot, 0, sizeof machine->boot);
  (void)memset(&machine->cache, 0, sizeof machine->cache);
  (void)memset(&machine->guest_devices, 0, sizeof machine->guest_devices);
  (void)memset(&machine->output, 0, sizeof machine->output);
  machine->mi_pending = 0u;
  machine->mi_mask = 0u;
  machine->mi_irq_line = false;
  t64_trace_reset(machine);
  if (is_openipl3_profile(&machine->profile)) {
    return prepare_openipl3_warm_entry(machine);
  }
  return T64_STATUS_OK;
}

const char *t64_build_identity(void) { return T64_BUILD_ID; }

const char *t64_source_revision(void) { return T64_SOURCE_REVISION; }

t64_status t64_profile_lookup(const char *profile_id,
                              const t64_profile_descriptor **profile_out) {
  static const char skeleton_profile_id[] = "ntsc-cart-4m-skeleton";
  static const char openipl3_profile_id[] =
      "ntsc-cart-4m-openipl3-warm-v1";
  size_t i;

  if (profile_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  *profile_out = NULL;
  if (profile_id == NULL || profile_id[0] == '\0') {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  for (i = 0u;; i++) {
    if (profile_id[i] != skeleton_profile_id[i]) {
      break;
    }
    if (skeleton_profile_id[i] == '\0') {
      *profile_out = &ntsc_cart_4m_profile;
      return T64_STATUS_OK;
    }
  }
  for (i = 0u;; i++) {
    if (profile_id[i] != openipl3_profile_id[i]) {
      return T64_STATUS_UNKNOWN_PROFILE;
    }
    if (openipl3_profile_id[i] == '\0') {
      *profile_out = &ntsc_cart_4m_openipl3_warm_profile;
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
  t64_address_range profile_range;
  t64_address_range length_range;
  t64_address_range bytes_range;
  size_t required;

  if (length_out == NULL || !profile_fields_are_valid(profile) ||
      !t64_internal_make_range(profile, sizeof *profile, &profile_range) ||
      !t64_internal_make_range(length_out, sizeof *length_out,
                               &length_range) ||
      t64_internal_ranges_overlap(profile_range, length_range)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  required = profile_serialized_size(profile);
  if (required > T64_PROFILE_SERIALIZED_MAX_BYTES) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (bytes_out != NULL &&
      (!t64_internal_make_range(bytes_out, capacity, &bytes_range) ||
       t64_internal_ranges_overlap(bytes_range, profile_range) ||
       t64_internal_ranges_overlap(bytes_range, length_range))) {
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
  t64_address_range allocation_range;
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
      profile->event_queue_capacity > T64_EVENT_QUEUE_CAPACITY ||
      t64_profile_hash(profile, profile_digest) != T64_STATUS_OK) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (profile->descriptor_version == 2u &&
      has_openipl3_profile_id(profile) &&
      !is_openipl3_profile(profile)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  total_size = (size_t)profile->rdram_bytes;
  if (total_size > SIZE_MAX - sizeof(t64_machine)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  total_size += sizeof(t64_machine);
  if (!allocator->allocate(allocator->context, total_size, alignment,
                           &storage)) {
    return T64_STATUS_ALLOCATION_FAILED;
  }
  if (!t64_internal_storage_range(storage, total_size, alignment,
                                  &allocation_range) ||
      !machine_storage_disjoint_from_inputs(allocation_range, allocator,
                                            profile, machine_out)) {
    release_bad_machine_storage(allocator, storage, total_size, profile,
                                machine_out);
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
  t64_rom_candidate candidate;
  t64_host_storage previous;
  t64_status status;

  if (machine == NULL) {
    return T64_STATUS_INVALID_ROM;
  }
  if (is_openipl3_profile(&machine->profile) &&
      !openipl3_asset_matches(bytes, length)) {
    return T64_STATUS_INVALID_ROM;
  }
  status = t64_rom_candidate_prepare(
      &machine->allocator, &machine->profile, bytes, length,
      machine->machine_storage, machine->image_storage, machine->has_image,
      &candidate);
  if (status != T64_STATUS_OK) {
    return status;
  }

  previous = machine->image_storage;
  machine->image_storage = candidate.storage;
  machine->image_length = candidate.length;
  (void)memcpy(machine->content_digest, candidate.digest,
               sizeof machine->content_digest);
  (void)memcpy(machine->rom_header, candidate.header,
               sizeof machine->rom_header);
  (void)memcpy(machine->rom_title, candidate.title,
               sizeof machine->rom_title);
  (void)memcpy(machine->rom_game_code, candidate.game_code,
               sizeof machine->rom_game_code);
  machine->rom_version = candidate.version;
  machine->has_image = true;
  candidate.owns_storage = false;
  status = reset_guest_state(machine);
  t64_trace_emit(machine, machine->tick, T64_TRACE_DEVICE_MACHINE,
                 T64_TRACE_KIND_ROM_ADMITTED, (uint64_t)candidate.length);
  t64_rom_candidate_discard(&machine->allocator, &candidate);
  if (previous.data != NULL) {
    machine->allocator.release(machine->allocator.context, previous);
  }
  return status;
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
  return t64_scheduler_advance(machine, request, result_out);
}

t64_status t64_machine_content_digest(
    const t64_machine *machine, uint8_t digest_out[T64_DIGEST_SIZE]) {
  t64_address_range output_range;

  if (machine == NULL || digest_out == NULL ||
      !t64_internal_make_range(digest_out, T64_DIGEST_SIZE, &output_range) ||
      !t64_internal_observation_range_is_disjoint(machine, output_range)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!machine->has_image) {
    return T64_STATUS_NOT_READY;
  }
  (void)memcpy(digest_out, machine->content_digest,
               sizeof machine->content_digest);
  return T64_STATUS_OK;
}

t64_status t64_machine_rom_size(const t64_machine *machine,
                                size_t *size_out) {
  t64_address_range output_range;

  if (machine == NULL || size_out == NULL ||
      !t64_internal_make_range(size_out, sizeof *size_out, &output_range) ||
      !t64_internal_observation_range_is_disjoint(machine, output_range)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!machine->has_image) {
    return T64_STATUS_NOT_READY;
  }
  *size_out = machine->image_length;
  return T64_STATUS_OK;
}

t64_status t64_machine_rom_header(const t64_machine *machine,
                                  uint8_t *header_out, size_t capacity,
                                  size_t *length_out) {
  t64_address_range length_range;
  t64_address_range header_range;

  if (machine == NULL || length_out == NULL ||
      (header_out == NULL && capacity != 0u) ||
      !t64_internal_make_range(length_out, sizeof *length_out, &length_range) ||
      !t64_internal_observation_range_is_disjoint(machine, length_range)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (header_out != NULL) {
    if (!t64_internal_make_range(header_out, capacity, &header_range) ||
        t64_internal_ranges_overlap(header_range, length_range) ||
        !t64_internal_observation_range_is_disjoint(machine, header_range)) {
      return T64_STATUS_INVALID_ARGUMENT;
    }
  }
  if (!machine->has_image) {
    return T64_STATUS_NOT_READY;
  }
  if (header_out == NULL || capacity < T64_ROM_HEADER_BYTES) {
    *length_out = T64_ROM_HEADER_BYTES;
    return T64_STATUS_BUFFER_TOO_SMALL;
  }
  *length_out = T64_ROM_HEADER_BYTES;
  (void)memcpy(header_out, machine->rom_header, T64_ROM_HEADER_BYTES);
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

t64_status t64_machine_cpu_stop_detail(
    const t64_machine *machine, t64_cpu_stop_detail *detail_out) {
  t64_address_range output_range;
  t64_cpu_stop_detail detail;

  if (machine == NULL || detail_out == NULL ||
      !t64_internal_make_range(detail_out, sizeof *detail_out, &output_range) ||
      !t64_internal_observation_range_is_disjoint(machine, output_range)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!machine->has_image || !machine->cpu.stopped) {
    return T64_STATUS_NOT_READY;
  }
  detail.retired_instructions = machine->cpu.retired_instructions;
  detail.pc = machine->cpu.stop_pc;
  detail.opcode = machine->cpu.stop_opcode;
  (void)memcpy(detail_out, &detail, sizeof detail);
  return T64_STATUS_OK;
}
