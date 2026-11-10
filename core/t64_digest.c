#include "t64_internal.h"

#include "t64_sha256.h"

#include <stddef.h>
#include <stdint.h>

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

static void hash_event(t64_sha256_ctx *context, const t64_event *event) {
  hash_u64(context, event->tick);
  hash_u64(context, event->sequence);
  hash_u32(context, event->device);
  hash_u32(context, event->kind);
  hash_u64(context, event->value);
  hash_u64(context, event->token);
}

t64_status t64_machine_digest(const t64_machine *machine,
                              uint8_t digest_out[T64_DIGEST_SIZE]) {
  static const uint8_t domain[] = {'t', '6', '4', '.', 's', 't', 'a', 't',
                                   'e', '.', 'v', '2'};
  t64_sha256_ctx context;
  size_t i;

  if (machine == NULL || digest_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!machine->has_image) {
    return T64_STATUS_NOT_READY;
  }

  t64_sha256_init(&context);
  t64_sha256_update(&context, domain, sizeof domain);
  hash_u32(&context, machine->profile.state_serialization_version);
  t64_sha256_update(&context, machine->profile_digest,
                    sizeof machine->profile_digest);
  t64_sha256_update(&context, machine->content_digest,
                    sizeof machine->content_digest);
  hash_u32(&context, machine->has_image ? UINT32_C(1) : UINT32_C(0));
  hash_u32(&context, (uint32_t)machine->image_length);
  t64_sha256_update(&context, machine->rom_header,
                    sizeof machine->rom_header);
  t64_sha256_update(&context, machine->rom_title,
                    sizeof machine->rom_title);
  t64_sha256_update(&context, machine->rom_game_code,
                    sizeof machine->rom_game_code);
  hash_u32(&context, machine->rom_version);
  hash_u64(&context, machine->tick);
  hash_u64(&context, machine->instruction_count);
  hash_u64(&context, machine->frame_event_count);
  hash_u32(&context, machine->sticky_error_status);
  hash_u32(&context, machine->halted ? UINT32_C(1) : UINT32_C(0));
  hash_u32(&context, machine->profile.rdram_bytes);
  t64_sha256_update(&context, machine->ram, machine->profile.rdram_bytes);
  hash_u32(&context, (uint32_t)machine->queue_count);
  hash_u64(&context, machine->next_sequence);
  hash_u32(&context,
           machine->sequence_exhausted ? UINT32_C(1) : UINT32_C(0));
  for (i = 0u; i < machine->queue_count; i++) {
    size_t index = (machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY;

    hash_event(&context, &machine->events[index]);
  }
  t64_sha256_final(&context, digest_out);
  return T64_STATUS_OK;
}
