#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../t64_internal.h"
#include "../../tools/common/t64_sha256.h"
#include "t64_test.h"
#include "tendies64/t64_machine.h"

#define IPL3_ASSET_BYTES 4096u
#define IPL3_PAYLOAD_BYTES 4032u
#define IPL3_PAYLOAD_SOURCE_OFFSET 0x40u
#define IPL3_DMEM_OFFSET 0x40u
#define IPL3_ENTRY_PC UINT32_C(0xa4000040)

typedef struct {
  size_t outstanding;
} allocator_state;

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  allocator_state *state = (allocator_state *)context;
  void *memory;

  if (state == NULL || storage_out == NULL || alignment == 0u) {
    return false;
  }
  memory = malloc(size);
  if (memory == NULL || ((uintptr_t)memory % alignment) != 0u) {
    free(memory);
    return false;
  }
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  state->outstanding++;
  return true;
}

static void test_release(void *context, t64_host_storage storage) {
  allocator_state *state = (allocator_state *)context;

  if (state != NULL && storage.data != NULL) {
    free(storage.data);
    state->outstanding--;
  }
}

static bool read_qualified_asset(uint8_t bytes[IPL3_ASSET_BYTES]) {
  const char *path = getenv("T64_TEST_IPL3_PATH");
  FILE *file;
  size_t length;
  int extra;
  int read_error;
  int close_error;

  if (path == NULL || path[0] == '\0' || bytes == NULL) {
    return false;
  }
  file = fopen(path, "rb");
  if (file == NULL) {
    return false;
  }
  length = fread(bytes, 1u, IPL3_ASSET_BYTES, file);
  extra = fgetc(file);
  read_error = ferror(file);
  close_error = fclose(file);
  if (read_error != 0 || close_error != 0) {
    return false;
  }
  return length == IPL3_ASSET_BYTES && extra == EOF;
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

static int hex_nibble(char value) {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  if (value >= 'a' && value <= 'f') {
    return value - 'a' + 10;
  }
  if (value >= 'A' && value <= 'F') {
    return value - 'A' + 10;
  }
  return -1;
}

static bool decode_sha256_hex(const char *hex,
                              uint8_t digest[T64_DIGEST_SIZE]) {
  size_t i;

  if (hex == NULL || digest == NULL || strlen(hex) != T64_DIGEST_SIZE * 2u) {
    return false;
  }
  for (i = 0u; i < T64_DIGEST_SIZE; i++) {
    int high = hex_nibble(hex[i * 2u]);
    int low = hex_nibble(hex[i * 2u + 1u]);

    if (high < 0 || low < 0) {
      return false;
    }
    digest[i] = (uint8_t)(((uint32_t)high << 4) | (uint32_t)low);
  }
  return true;
}

static bool bytes_match_sha256(const uint8_t *bytes, size_t length,
                               const char *expected_hex) {
  t64_sha256_ctx context;
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  char actual_hex[T64_SHA256_HEX_SIZE];

  t64_sha256_init(&context);
  t64_sha256_update(&context, bytes, length);
  t64_sha256_final(&context, digest);
  t64_sha256_hex(digest, actual_hex);
  return strcmp(actual_hex, expected_hex) == 0;
}

/* This encoder intentionally lives in the test and does not call the
 * production profile or machine serializers. It pins the wire field order
 * independently, including the legacy v6 meaning. */
static void reference_hash_u32(t64_sha256_ctx *context, uint32_t value) {
  uint8_t bytes[4];

  bytes[0] = (uint8_t)(value >> 24);
  bytes[1] = (uint8_t)(value >> 16);
  bytes[2] = (uint8_t)(value >> 8);
  bytes[3] = (uint8_t)value;
  t64_sha256_update(context, bytes, sizeof bytes);
}

static void reference_hash_u64(t64_sha256_ctx *context, uint64_t value) {
  uint8_t bytes[8];
  unsigned i;

  for (i = 0u; i < 8u; i++) {
    bytes[i] = (uint8_t)(value >> (56u - (i * 8u)));
  }
  t64_sha256_update(context, bytes, sizeof bytes);
}

static size_t reference_text_length(const char *text, size_t capacity) {
  size_t length = 0u;

  while (length < capacity && text[length] != '\0') {
    length++;
  }
  return length;
}

static void reference_profile_text(t64_sha256_ctx *context, const char *text,
                                   size_t capacity) {
  size_t length = reference_text_length(text, capacity);

  reference_hash_u32(context, (uint32_t)length);
  t64_sha256_update(context, text, length);
}

static void reference_profile_digest(const t64_profile_descriptor *profile,
                                     uint8_t digest[T64_DIGEST_SIZE]) {
  static const uint8_t domain[] = {'T', '6', '4', 'P', 'D', 'E', 'S', 'C'};
  t64_sha256_ctx context;

  t64_sha256_init(&context);
  t64_sha256_update(&context, domain, sizeof domain);
  reference_hash_u32(&context, 1u);
  reference_profile_text(&context, profile->profile_id,
                         sizeof profile->profile_id);
  reference_profile_text(&context, profile->console_target,
                         sizeof profile->console_target);
  reference_profile_text(&context, profile->console_qualification,
                         sizeof profile->console_qualification);
  reference_hash_u32(&context, profile->tv_standard);
  reference_hash_u32(&context, profile->region);
  reference_hash_u32(&context, profile->media_kind);
  reference_hash_u32(&context, profile->rdram_bytes);
  reference_hash_u32(&context, profile->expansion_bytes);
  reference_hash_u32(&context, profile->peripheral_flags);
  reference_hash_u32(&context, profile->save_kind);
  reference_profile_text(&context, profile->boot_route,
                         sizeof profile->boot_route);
  reference_profile_text(&context, profile->boot_status,
                         sizeof profile->boot_status);
  reference_profile_text(&context, profile->guest_tick_unit,
                         sizeof profile->guest_tick_unit);
  reference_hash_u32(&context, profile->cpu_rsp_ratio_numerator);
  reference_hash_u32(&context, profile->cpu_rsp_ratio_denominator);
  reference_hash_u32(&context, profile->synchronization_quantum);
  reference_profile_text(&context, profile->timing_tier,
                         sizeof profile->timing_tier);
  reference_profile_text(&context, profile->scheduler_version,
                         sizeof profile->scheduler_version);
  reference_profile_text(&context, profile->pi_model_version,
                         sizeof profile->pi_model_version);
  reference_profile_text(&context, profile->sp_model_version,
                         sizeof profile->sp_model_version);
  reference_hash_u32(&context, profile->rom_min_bytes);
  reference_hash_u32(&context, profile->rom_max_bytes);
  reference_hash_u32(&context, profile->event_queue_capacity);
  reference_hash_u32(&context, profile->sequence_width_bits);
  reference_hash_u32(&context, profile->advance_event_budget);
  reference_hash_u32(&context, profile->pi_duration_base_ticks);
  reference_hash_u32(&context, profile->pi_duration_bytes_divisor);
  reference_hash_u32(&context, profile->sp_duration_base_rsp_cycles);
  reference_hash_u32(&context, profile->sp_duration_bytes_divisor);
  reference_profile_text(&context, profile->rom_order_policy,
                         sizeof profile->rom_order_policy);
  reference_profile_text(&context, profile->event_order_policy,
                         sizeof profile->event_order_policy);
  reference_profile_text(&context, profile->queue_exhaustion_policy,
                         sizeof profile->queue_exhaustion_policy);
  reference_profile_text(&context, profile->sequence_exhaustion_policy,
                         sizeof profile->sequence_exhaustion_policy);
  reference_profile_text(&context, profile->advance_budget_policy,
                         sizeof profile->advance_budget_policy);
  reference_profile_text(&context, profile->guest_time_bound_policy,
                         sizeof profile->guest_time_bound_policy);
  reference_profile_text(&context, profile->halt_precedence_policy,
                         sizeof profile->halt_precedence_policy);
  reference_profile_text(&context, profile->pi_register_policy,
                         sizeof profile->pi_register_policy);
  reference_profile_text(&context, profile->sp_register_policy,
                         sizeof profile->sp_register_policy);
  reference_profile_text(&context, profile->device_error_policy,
                         sizeof profile->device_error_policy);
  reference_profile_text(&context, profile->dma_cancel_policy,
                         sizeof profile->dma_cancel_policy);
  reference_profile_text(&context, profile->dma_visibility_policy,
                         sizeof profile->dma_visibility_policy);
  reference_hash_u32(&context, profile->descriptor_version);
  reference_hash_u32(&context, profile->machine_model_version);
  reference_hash_u32(&context, profile->state_serialization_version);
  if (profile->descriptor_version == 2u) {
    t64_sha256_update(&context, profile->asset_sha256,
                      sizeof profile->asset_sha256);
    t64_sha256_update(&context, profile->payload_sha256,
                      sizeof profile->payload_sha256);
    t64_sha256_update(&context, profile->boot_policy_sha256,
                      sizeof profile->boot_policy_sha256);
    reference_hash_u32(&context, profile->hle_seed_reset_version);
  }
  t64_sha256_final(&context, digest);
}

static void reference_hash_sp_descriptor(
    t64_sha256_ctx *context, const t64_sp_dma_descriptor *descriptor) {
  reference_hash_u32(context, descriptor->valid ? 1u : 0u);
  reference_hash_u32(context, descriptor->dram_to_sp ? 1u : 0u);
  reference_hash_u32(context, descriptor->imem ? 1u : 0u);
  reference_hash_u32(context, descriptor->raw_mem_address);
  reference_hash_u32(context, descriptor->raw_dram_address);
  reference_hash_u32(context, descriptor->raw_length);
  reference_hash_u32(context, descriptor->aligned_dram_start);
  reference_hash_u32(context, descriptor->internal_start);
  reference_hash_u32(context, descriptor->bytes_per_line);
  reference_hash_u32(context, descriptor->line_count);
  reference_hash_u32(context, descriptor->skip);
  reference_hash_u32(context, descriptor->payload_bytes);
  reference_hash_u64(context, descriptor->start_tick);
  reference_hash_u64(context, descriptor->end_tick);
  reference_hash_u64(context, descriptor->token);
}

static void reference_machine_digest(const t64_machine *machine,
                                    uint8_t digest[T64_DIGEST_SIZE]) {
  static const uint8_t domain_v6[] = {
      't', '6', '4', '.', 's', 't', 'a', 't', 'e', '.', 'v', '6'};
  static const uint8_t domain_v7[] = {
      't', '6', '4', '.', 's', 't', 'a', 't', 'e', '.', 'v', '7'};
  const bool v7 = machine->profile.state_serialization_version == 7u;
  t64_sha256_ctx context;
  size_t i;

  t64_sha256_init(&context);
  t64_sha256_update(&context, v7 ? domain_v7 : domain_v6,
                    sizeof domain_v6);
  reference_hash_u32(&context, machine->profile.state_serialization_version);
  t64_sha256_update(&context, machine->profile_digest,
                    sizeof machine->profile_digest);
  t64_sha256_update(&context, machine->content_digest,
                    sizeof machine->content_digest);
  reference_hash_u32(&context, machine->has_image ? 1u : 0u);
  reference_hash_u32(&context, (uint32_t)machine->image_length);
  t64_sha256_update(&context, machine->rom_header, sizeof machine->rom_header);
  t64_sha256_update(&context, machine->rom_title, sizeof machine->rom_title);
  t64_sha256_update(&context, machine->rom_game_code,
                    sizeof machine->rom_game_code);
  reference_hash_u32(&context, machine->rom_version);
  reference_hash_u64(&context, machine->tick);
  reference_hash_u64(&context, machine->instruction_count);
  reference_hash_u64(&context, machine->frame_event_count);
  reference_hash_u32(&context, machine->sticky_error_status);
  reference_hash_u32(&context, machine->halted ? 1u : 0u);
  reference_hash_u32(&context, machine->profile.rdram_bytes);
  t64_sha256_update(&context, machine->ram, machine->profile.rdram_bytes);
  reference_hash_u32(&context, (uint32_t)sizeof machine->sp_dmem);
  t64_sha256_update(&context, machine->sp_dmem, sizeof machine->sp_dmem);
  reference_hash_u32(&context, (uint32_t)sizeof machine->sp_imem);
  t64_sha256_update(&context, machine->sp_imem, sizeof machine->sp_imem);
  reference_hash_u32(&context, machine->mi_pending & T64_MI_INTERRUPT_BITS);
  reference_hash_u32(&context, machine->mi_mask & T64_MI_INTERRUPT_BITS);
  reference_hash_u32(&context, machine->mi_irq_line ? 1u : 0u);
  reference_hash_u32(&context, machine->pi.dram_address);
  reference_hash_u32(&context, machine->pi.cart_address);
  reference_hash_u32(&context, machine->pi.read_length);
  reference_hash_u32(&context, machine->pi.write_length);
  reference_hash_u32(&context, machine->pi.domain1_latency);
  reference_hash_u32(&context, machine->pi.domain1_pulse_width);
  reference_hash_u32(&context, machine->pi.domain1_page_size);
  reference_hash_u32(&context, machine->pi.domain1_release_duration);
  reference_hash_u32(&context, machine->pi.domain2_latency);
  reference_hash_u32(&context, machine->pi.domain2_pulse_width);
  reference_hash_u32(&context, machine->pi.domain2_page_size);
  reference_hash_u32(&context, machine->pi.domain2_release_duration);
  reference_hash_u32(&context, machine->pi.error ? 1u : 0u);
  reference_hash_u32(&context, machine->pi.active_valid ? 1u : 0u);
  reference_hash_u32(&context, machine->pi.active_canceled ? 1u : 0u);
  reference_hash_u32(&context, machine->pi.active_dram_address);
  reference_hash_u32(&context, machine->pi.active_cart_address);
  reference_hash_u32(&context, machine->pi.active_length);
  reference_hash_u64(&context, machine->pi.active_token);
  reference_hash_u64(&context, machine->pi.next_token);
  reference_hash_u32(&context, machine->pi.token_exhausted ? 1u : 0u);
  reference_hash_u32(&context, machine->sp.mem_address);
  reference_hash_u32(&context, machine->sp.dram_address);
  reference_hash_u32(&context, machine->sp.read_length);
  reference_hash_u32(&context, machine->sp.write_length);
  reference_hash_sp_descriptor(&context, &machine->sp.active);
  reference_hash_sp_descriptor(&context, &machine->sp.pending);
  reference_hash_u64(&context, machine->sp.next_token);
  reference_hash_u32(&context, machine->sp.token_exhausted ? 1u : 0u);
  reference_hash_u32(&context, (uint32_t)machine->queue_count);
  reference_hash_u64(&context, machine->next_sequence);
  reference_hash_u32(&context, machine->sequence_exhausted ? 1u : 0u);
  for (i = 0u; i < machine->queue_count; i++) {
    size_t index = (machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY;
    const t64_event *event = &machine->events[index];

    reference_hash_u64(&context, event->tick);
    reference_hash_u64(&context, event->sequence);
    reference_hash_u32(&context, event->device);
    reference_hash_u32(&context, event->kind);
    reference_hash_u64(&context, event->value);
    reference_hash_u64(&context, event->token);
  }
  for (i = 0u; i < sizeof machine->boot.asset_sha256; i++) {
    t64_sha256_update(&context, &machine->boot.asset_sha256[i], 1u);
  }
  for (i = 0u; i < sizeof machine->boot.payload_sha256; i++) {
    t64_sha256_update(&context, &machine->boot.payload_sha256[i], 1u);
  }
  for (i = 0u; i < sizeof machine->boot.policy_sha256; i++) {
    t64_sha256_update(&context, &machine->boot.policy_sha256[i], 1u);
  }
  reference_hash_u32(&context, machine->boot.source_offset);
  reference_hash_u32(&context, machine->boot.payload_length);
  reference_hash_u32(&context, machine->boot.dmem_offset);
  reference_hash_u32(&context, machine->boot.entry_pc);
  reference_hash_u32(&context, machine->boot.seed_reset_version);
  reference_hash_u32(&context, machine->boot.seed_s3_rom_type);
  reference_hash_u32(&context, machine->boot.seed_s4_tv_type);
  reference_hash_u32(&context, machine->boot.seed_s5_reset_type);
  reference_hash_u32(&context, machine->boot.seed_s6_entropy);
  reference_hash_u32(&context, machine->boot.seed_s7_boot_info_version);
  reference_hash_u32(&context, machine->boot.prepared ? 1u : 0u);
  for (i = 0u; i < 32u; i++) {
    reference_hash_u64(&context, machine->cpu.gpr[i]);
  }
  reference_hash_u64(&context, machine->cpu.hi);
  reference_hash_u64(&context, machine->cpu.lo);
  for (i = 0u; i < 32u; i++) {
    reference_hash_u64(&context, machine->cpu.cp0[i]);
  }
  reference_hash_u64(&context, machine->cpu.retired_instructions);
  reference_hash_u32(&context, machine->cpu.pc);
  reference_hash_u32(&context, machine->cpu.last_pc);
  reference_hash_u32(&context, machine->cpu.last_opcode);
  reference_hash_u32(&context, machine->cpu.delay_slot_pc);
  reference_hash_u32(&context, machine->cpu.delay_target);
  reference_hash_u32(&context, machine->cpu.delay_slot_pending ? 1u : 0u);
  reference_hash_u32(&context, machine->cpu.stopped ? 1u : 0u);
  reference_hash_u32(&context, machine->cpu.stop_reason);
  reference_hash_u32(&context, machine->cpu.stop_pc);
  reference_hash_u32(&context, machine->cpu.stop_opcode);
  reference_hash_u32(&context, machine->cache.policy_version);
  reference_hash_u32(&context, machine->cache.last_operation);
  reference_hash_u32(&context, machine->cache.last_pc);
  reference_hash_u32(&context, machine->cache.last_address);
  reference_hash_u32(&context, machine->cache.operation_seen ? 1u : 0u);
  reference_hash_u32(&context, machine->guest_devices.policy_version);
  reference_hash_u32(&context, machine->guest_devices.mi_version);
  reference_hash_u32(&context, machine->guest_devices.ri_select);
  reference_hash_u32(&context, machine->guest_devices.mi_version_known ? 1u
                                                                        : 0u);
  reference_hash_u32(&context, machine->guest_devices.ri_select_known ? 1u
                                                                       : 0u);
  reference_hash_u32(&context, machine->output.policy_version);
  reference_hash_u32(&context, machine->output.video_output_ready ? 1u : 0u);
  reference_hash_u32(&context, machine->output.audio_output_ready ? 1u : 0u);
  if (v7) {
    reference_hash_u32(&context, machine->output.magic_register);
    reference_hash_u32(&context, machine->output.byte_count);
    reference_hash_u32(&context, T64_ISVIEWER_BUFFER_BYTES);
    t64_sha256_update(&context, machine->output.buffer,
                      sizeof machine->output.buffer);
    reference_hash_u32(&context, machine->output.captured_bytes);
    reference_hash_u32(&context, T64_ISVIEWER_CAPTURE_BYTES);
    t64_sha256_update(&context, machine->output.captured,
                      sizeof machine->output.captured);
    reference_hash_u32(&context, machine->output.lost_output ? 1u : 0u);
  }
  t64_sha256_final(&context, digest);
}

int main(void) {
  static const char expected_v6_profile_sha256[] =
      "5775f19766dc0ae070178dc87f2e6b0f11a9e5dc587fa380588f3b82b7d768d2";
  static const char expected_v6_state_sha256[] =
      "a3820a5bf34b713b5903e42f455a1c579a57799a48411d3d33ed59786c3e93bf";
  static const char expected_v7_profile_sha256[] =
      "6e6c4f86beb81c08023dfdf3a7b04e5d74ab0d4038ef5a95ea1a83f3eb8bba97";
  static const char expected_v7_state_sha256[] =
      "10232c080e55a88dfadfbfa7d467b9a911a9cdb909638336bd0fa713b7d95a56";
  static const char expected_asset_sha256[] =
      "f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068";
  static const char expected_payload_sha256[] =
      "f716a90b1d36ec4253d096790791256d3543455e54473a755ff8c3d1199b15d5";
  static const char expected_policy_sha256[] =
      "08634481792748ade0e64073b9cd51d5972ec3a56421278b3203979087b4e05d";
  static const char expected_v6_policy_sha256[] =
      "831a0a58ef8f82bc54f880ad5a1259bddf1b3d2e21680ac6637ea5994705a7fe";
  static const uint8_t zero_dmem_prefix[IPL3_DMEM_OFFSET] = {0u};
  const t64_profile_descriptor *profile = NULL;
  allocator_state allocator_state_value = {0u};
  const t64_host_allocator allocator = {
      test_allocate, test_release, &allocator_state_value};
  t64_profile_descriptor altered_profile;
  t64_profile_descriptor legacy_profile;
  t64_machine *machine = NULL;
  t64_machine *rejected_machine = NULL;
  t64_machine legacy_machine;
  uint8_t asset[IPL3_ASSET_BYTES];
  uint8_t altered_asset[IPL3_ASSET_BYTES];
  uint8_t digest[T64_DIGEST_SIZE];
  uint8_t expected_asset_digest[T64_DIGEST_SIZE];
  uint8_t expected_payload_digest[T64_DIGEST_SIZE];
  uint8_t expected_policy_digest[T64_DIGEST_SIZE];
  uint8_t expected_v6_policy_digest[T64_DIGEST_SIZE];
  uint8_t reference_digest[T64_DIGEST_SIZE];
  uint8_t reference_profile[T64_DIGEST_SIZE];
  uint8_t production_digest[T64_DIGEST_SIZE];
  char profile_hex[T64_DIGEST_SIZE * 2u + 1u];
  char state_hex[T64_DIGEST_SIZE * 2u + 1u];
  size_t observed_rom_size = SIZE_MAX;
  t64_status status;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(100)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_stop_result repeated_result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};
  bool fixture_ok;

  fixture_ok = read_qualified_asset(asset);
  T64_CHECK(fixture_ok);
  if (!fixture_ok) {
    goto done;
  }
  T64_CHECK(bytes_match_sha256(asset, sizeof asset, expected_asset_sha256));
  T64_CHECK(bytes_match_sha256(asset + IPL3_PAYLOAD_SOURCE_OFFSET,
                               IPL3_PAYLOAD_BYTES,
                               expected_payload_sha256));

  status = t64_profile_lookup("ntsc-cart-4m-openipl3-warm-v1", &profile);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK(profile != NULL);
  if (profile == NULL) {
    goto done;
  }
  T64_CHECK_STR(profile->profile_id, "ntsc-cart-4m-openipl3-warm-v1");
  T64_CHECK_STR(profile->console_target, "NUS-CPU-03");
  T64_CHECK_STR(profile->console_qualification, "unqualified-target");
  T64_CHECK_STR(profile->boot_status, "prepared-warm-entry-partial");
  T64_CHECK_INT(profile->tv_standard, 1u);
  T64_CHECK_INT(profile->region, 1u);
  T64_CHECK_INT(profile->media_kind, 1u);
  T64_CHECK_INT(profile->rdram_bytes, 4194304u);
  T64_CHECK_INT(profile->expansion_bytes, 0u);
  T64_CHECK_INT(profile->peripheral_flags, 0u);
  T64_CHECK_INT(profile->save_kind, 0u);
  T64_CHECK_INT(profile->rom_min_bytes, IPL3_ASSET_BYTES);
  T64_CHECK_INT(profile->rom_max_bytes, UINT32_C(1048576));
  T64_CHECK_INT(profile->descriptor_version, 2u);
  T64_CHECK_INT(profile->machine_model_version, 2u);
  T64_CHECK_INT(profile->state_serialization_version, 7u);
  T64_CHECK_INT(profile->hle_seed_reset_version, 1u);
  T64_CHECK(decode_sha256_hex(expected_asset_sha256, expected_asset_digest));
  T64_CHECK(decode_sha256_hex(expected_payload_sha256,
                              expected_payload_digest));
  T64_CHECK(decode_sha256_hex(expected_policy_sha256,
                              expected_policy_digest));
  T64_CHECK(decode_sha256_hex(expected_v6_policy_sha256,
                              expected_v6_policy_digest));
  legacy_profile = *profile;
  legacy_profile.state_serialization_version = 6u;
  (void)memcpy(legacy_profile.boot_policy_sha256, expected_v6_policy_digest,
               sizeof legacy_profile.boot_policy_sha256);
  reference_profile_digest(&legacy_profile, reference_profile);
  digest_to_hex(reference_profile, profile_hex);
  T64_CHECK_STR(profile_hex, expected_v6_profile_sha256);
  T64_CHECK_INT(t64_profile_hash(&legacy_profile, digest), T64_STATUS_OK);
  T64_CHECK(memcmp(digest, reference_profile, sizeof digest) == 0);

  reference_profile_digest(profile, reference_profile);
  digest_to_hex(reference_profile, profile_hex);
  T64_CHECK_INT(t64_profile_hash(profile, digest), T64_STATUS_OK);
  T64_CHECK(memcmp(digest, reference_profile, sizeof digest) == 0);
  T64_CHECK_STR(profile_hex, expected_v7_profile_sha256);

  altered_profile = *profile;
  altered_profile.boot_policy_sha256[0] ^= UINT8_C(1);
  status = t64_machine_create(&allocator, &altered_profile,
                              &rejected_machine);
  T64_CHECK_INT(status, T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK(rejected_machine == NULL);
  T64_CHECK_INT(allocator_state_value.outstanding, 0u);

  status = t64_machine_create(&allocator, profile, &machine);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK(machine != NULL);
  if (machine == NULL) {
    goto done;
  }
  (void)memcpy(altered_asset, asset, sizeof altered_asset);
  altered_asset[0x100u] ^= UINT8_C(1);
  T64_CHECK_INT(t64_machine_admit_rom(machine, altered_asset,
                                      sizeof altered_asset),
                T64_STATUS_INVALID_ROM);
  T64_CHECK(!machine->has_image);
  T64_CHECK(!machine->boot.prepared);
  T64_CHECK_INT(machine->queue_count, 0u);
  T64_CHECK_INT(t64_machine_rom_size(machine, &observed_rom_size),
                T64_STATUS_NOT_READY);
  T64_CHECK_INT(observed_rom_size, SIZE_MAX);

  T64_CHECK_INT(t64_machine_admit_rom(machine, asset, sizeof asset),
                T64_STATUS_OK);
  T64_CHECK(machine->boot.prepared);
  T64_CHECK_INT(machine->boot.source_offset, IPL3_PAYLOAD_SOURCE_OFFSET);
  T64_CHECK_INT(machine->boot.payload_length, IPL3_PAYLOAD_BYTES);
  T64_CHECK_INT(machine->boot.dmem_offset, IPL3_DMEM_OFFSET);
  T64_CHECK(memcmp(machine->sp_dmem, zero_dmem_prefix,
                   sizeof zero_dmem_prefix) == 0);
  T64_CHECK_INT(machine->boot.entry_pc, IPL3_ENTRY_PC);
  T64_CHECK_INT(machine->cpu.pc, IPL3_ENTRY_PC);
  T64_CHECK_INT(machine->cpu.gpr[19], 0u);
  T64_CHECK_INT(machine->cpu.gpr[20], 1u);
  T64_CHECK_INT(machine->cpu.gpr[21], 1u);
  T64_CHECK_INT(machine->cpu.gpr[22], 0u);
  T64_CHECK_INT(machine->cpu.gpr[23], 1u);
  T64_CHECK_INT(machine->pi.next_token, 1u);
  T64_CHECK_INT(machine->sp.next_token, 1u);
  T64_CHECK(memcmp(machine->sp_dmem + IPL3_DMEM_OFFSET,
                   asset + IPL3_PAYLOAD_SOURCE_OFFSET,
                   IPL3_PAYLOAD_BYTES) == 0);
  T64_CHECK(memcmp(machine->boot.asset_sha256, expected_asset_digest,
                   sizeof expected_asset_digest) == 0);
  T64_CHECK(memcmp(machine->boot.payload_sha256, expected_payload_digest,
                   sizeof expected_payload_digest) == 0);
  T64_CHECK(memcmp(machine->boot.policy_sha256, expected_policy_digest,
                   sizeof expected_policy_digest) == 0);

  legacy_machine = *machine;
  legacy_machine.profile = legacy_profile;
  reference_profile_digest(&legacy_profile, legacy_machine.profile_digest);
  (void)memcpy(legacy_machine.boot.policy_sha256,
               expected_v6_policy_digest,
               sizeof legacy_machine.boot.policy_sha256);
  legacy_machine.output.policy_version = 1u;
  reference_machine_digest(&legacy_machine, reference_digest);
  digest_to_hex(reference_digest, state_hex);
  T64_CHECK_STR(state_hex, expected_v6_state_sha256);
  T64_CHECK_INT(t64_machine_digest(&legacy_machine, production_digest),
                T64_STATUS_OK);
  T64_CHECK(memcmp(production_digest, reference_digest,
                   sizeof reference_digest) == 0);

  T64_CHECK_INT(t64_machine_digest(machine, digest), T64_STATUS_OK);
  reference_machine_digest(machine, reference_digest);
  T64_CHECK(memcmp(digest, reference_digest, sizeof digest) == 0);
  digest_to_hex(reference_digest, state_hex);
  T64_CHECK_STR(state_hex, expected_v7_state_sha256);

  T64_CHECK_INT(t64_machine_advance(machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, 100u);
  T64_CHECK_INT(machine->instruction_count, 100u);
  T64_CHECK_INT(machine->cpu.retired_instructions, 100u);
  T64_CHECK_INT(machine->cpu.pc, UINT32_C(0xa4000354));
  T64_CHECK_INT(machine->cpu.last_pc, UINT32_C(0xa4000350));
  T64_CHECK_INT(machine->cpu.last_opcode, UINT32_C(0x240af000));
  T64_CHECK_INT(t64_machine_cpu_stop_detail(machine, &detail),
                T64_STATUS_NOT_READY);
  request.value = 100u;
  T64_CHECK_INT(t64_machine_advance(machine, &request, &repeated_result),
                T64_STATUS_OK);
  T64_CHECK_INT(repeated_result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(repeated_result.tick, 100u);

done:
  t64_machine_destroy(machine);
  t64_machine_destroy(rejected_machine);
  T64_CHECK_INT(allocator_state_value.outstanding, 0u);
  puts("TAP version 13");
  (void)printf("%s 1 - qualified warm entry identity and partial execution\n",
               t64_test_failures == 0 ? "ok" : "not ok");
  puts("1..1");
  return t64_test_failures == 0 ? 0 : 1;
}
