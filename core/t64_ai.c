#include "t64_internal.h"

#include "t64_sha256.h"

#include <stdint.h>
#include <string.h>

#define T64_AI_DRAM_ADDR (T64_AI_REGISTER_BASE + UINT32_C(0x00))
#define T64_AI_LEN (T64_AI_REGISTER_BASE + UINT32_C(0x04))
#define T64_AI_CONTROL (T64_AI_REGISTER_BASE + UINT32_C(0x08))
#define T64_AI_STATUS (T64_AI_REGISTER_BASE + UINT32_C(0x0c))
#define T64_AI_DACRATE (T64_AI_REGISTER_BASE + UINT32_C(0x10))
#define T64_AI_BITRATE (T64_AI_REGISTER_BASE + UINT32_C(0x14))
#define T64_AI_CONTROL_ENABLE UINT32_C(1)
#define T64_AI_STATUS_BUSY UINT32_C(0x40000000)
#define T64_AI_STATUS_FULL UINT32_C(0x80000000)
#define T64_AI_ADDRESS_MASK UINT32_C(0x00ffffff)
#define T64_AI_DACRATE_MASK UINT32_C(0x00003fff)
#define T64_AI_BITRATE_MASK UINT32_C(0x0000000f)
#define T64_AI_SOURCE_CLOCK UINT64_C(48681812)
#define T64_AI_GUEST_CLOCK UINT64_C(62500000)

static void copy_text(char *out, size_t capacity, const char *text) {
  size_t i = 0u;

  if (capacity == 0u) return;
  while (i + 1u < capacity && text[i] != '\0') {
    out[i] = text[i];
    i++;
  }
  out[i] = '\0';
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
  size_t i;

  for (i = 0u; i < sizeof bytes; i++) {
    bytes[i] = (uint8_t)(value >> (56u - (uint32_t)i * 8u));
  }
  t64_sha256_update(context, bytes, sizeof bytes);
}

static bool sample_delay(uint32_t dacrate, uint64_t remainder,
                         uint64_t *delay_out, uint64_t *remainder_out) {
  uint64_t numerator;
  uint64_t needed;
  uint64_t delay;
  uint64_t scaled;

  if (delay_out == NULL || remainder_out == NULL ||
      dacrate > T64_AI_DACRATE_MASK || remainder >= T64_AI_SOURCE_CLOCK) {
    return false;
  }
  numerator = T64_AI_GUEST_CLOCK * ((uint64_t)dacrate + UINT64_C(1));
  if (numerator <= remainder) return false;
  needed = numerator - remainder;
  delay = needed / T64_AI_SOURCE_CLOCK;
  if (needed % T64_AI_SOURCE_CLOCK != 0u) delay++;
  if (delay == 0u || delay > UINT64_MAX / T64_AI_SOURCE_CLOCK) return false;
  scaled = delay * T64_AI_SOURCE_CLOCK;
  if (scaled > UINT64_MAX - remainder || scaled + remainder < numerator) {
    return false;
  }
  *delay_out = delay;
  *remainder_out = scaled + remainder - numerator;
  return *remainder_out < T64_AI_SOURCE_CLOCK;
}

static bool rate_pair_valid(uint32_t dacrate, uint32_t bitrate) {
  const uint64_t minimum_divisor =
      UINT64_C(66) * ((uint64_t)bitrate + UINT64_C(1));

  return dacrate <= T64_AI_DACRATE_MASK && bitrate > 0u &&
         bitrate <= T64_AI_BITRATE_MASK &&
         (uint64_t)dacrate + UINT64_C(1) >= minimum_divisor;
}

static t64_status reserve_sample_event(t64_machine *machine, uint32_t dacrate,
                                      uint64_t phase_remainder,
                                      uint64_t token,
                                      uint64_t *remainder_out) {
  t64_event_reservation reservation;
  uint64_t delay;
  uint64_t remainder;
  t64_status status;

  if (!sample_delay(dacrate, phase_remainder, &delay,
                    &remainder)) {
    return T64_STATUS_TICK_OVERFLOW;
  }
  status = t64_schedule_reserve_after(
      machine, delay, T64_AI_DEVICE, T64_AI_EVENT_SAMPLE, 0u, token,
      t64_ai_event, &reservation);
  if (status != T64_STATUS_OK) return status;
  status = t64_schedule_commit(machine, &reservation);
  if (status == T64_STATUS_OK) *remainder_out = remainder;
  return status;
}

static uint32_t dma_slot(const t64_ai_state *ai, size_t offset) {
  return (uint32_t)((ai->descriptor_head + offset) %
                    T64_AUDIO_QUEUE_CAPACITY);
}

static uint32_t output_slot(uint64_t sequence) {
  return (uint32_t)((sequence - UINT64_C(1)) %
                    T64_AUDIO_QUEUE_CAPACITY);
}

static t64_status activate_queued_descriptor(t64_machine *machine) {
  t64_ai_state *ai = &machine->ai;
  t64_ai_dma_descriptor *descriptor;
  uint64_t token;
  uint64_t phase_remainder = ai->latched_dacrate == ai->dacrate
                                 ? ai->phase_remainder
                                 : UINT64_C(0);
  uint64_t remainder = phase_remainder;
  uint32_t slot;
  t64_status status;

  if (ai->active || ai->descriptor_count == 0u) return T64_STATUS_OK;
  if (ai->event_generation == UINT64_MAX) {
    return T64_STATUS_SEQUENCE_EXHAUSTED;
  }
  slot = dma_slot(ai, 0u);
  descriptor = &ai->descriptors[slot];
  token = ai->event_generation + UINT64_C(1);
  status = reserve_sample_event(machine, ai->dacrate, phase_remainder, token,
                                &remainder);
  if (status != T64_STATUS_OK) return status;

  descriptor->dacrate = ai->dacrate;
  descriptor->bitrate = ai->bitrate;
  ai->active = true;
  ai->paused = false;
  ai->paused_remaining_ticks = 0u;
  ai->active_byte_cursor = 0u;
  ai->latched_dacrate = ai->dacrate;
  ai->latched_bitrate = ai->bitrate;
  ai->event_generation = token;
  ai->event_token = token;
  ai->phase_remainder = remainder;
  return T64_STATUS_OK;
}

static void hash_completed_packet(t64_machine *machine, uint32_t slot,
                                   const t64_ai_packet_slot *info) {
  t64_sha256_ctx context;
  uint8_t digest[T64_DIGEST_SIZE];

  t64_sha256_init(&context);
  t64_sha256_update(&context, machine->ai.packet_history_sha256,
                    sizeof machine->ai.packet_history_sha256);
  hash_u64(&context, info->sequence);
  hash_u64(&context, info->first_sample_index);
  hash_u64(&context, info->first_tick);
  hash_u64(&context, info->last_tick);
  hash_u32(&context, info->frame_count);
  hash_u64(&context, info->rate_numerator);
  hash_u64(&context, info->rate_denominator);
  hash_u64(&context, (uint64_t)info->byte_length);
  t64_sha256_update(
      &context,
      machine->audio_packets + (size_t)slot * T64_AUDIO_MAX_PACKET_BYTES,
      info->byte_length);
  t64_sha256_final(&context, digest);
  (void)memcpy(machine->ai.packet_history_sha256, digest, sizeof digest);
  if (machine->ai.packet_history_count != UINT64_MAX) {
    machine->ai.packet_history_count++;
  }
}

static t64_status submit_dma(t64_machine *machine, uint32_t raw_length) {
  t64_ai_state *ai = &machine->ai;
  t64_ai_dma_descriptor descriptor;
  uint32_t slot;
  uint32_t length = raw_length & UINT32_C(0x0003fff8);
  uint64_t token = UINT64_C(0);
  uint64_t phase_remainder = ai->latched_dacrate == ai->dacrate
                                 ? ai->phase_remainder
                                 : UINT64_C(0);
  uint64_t remainder = phase_remainder;
  bool activate;
  t64_status status;

  if (length == 0u) return T64_STATUS_OK;
  if (!ai->address_staged || (ai->staged_address & UINT32_C(7)) != 0u ||
      length > T64_AUDIO_MAX_PACKET_BYTES || (length & UINT32_C(7)) != 0u ||
      (uint64_t)ai->staged_address + (uint64_t)length >
          (uint64_t)machine->profile.rdram_bytes ||
      !rate_pair_valid(ai->dacrate, ai->bitrate)) {
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  if (ai->descriptor_count >= T64_AUDIO_QUEUE_CAPACITY) {
    return T64_STATUS_QUEUE_FULL;
  }
  if (ai->descriptor_sequence == UINT64_MAX) {
    return T64_STATUS_SEQUENCE_EXHAUSTED;
  }
  if (!ai->active && ai->descriptor_count != 0u &&
      (ai->control & T64_AI_CONTROL_ENABLE) != 0u) {
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }

  descriptor.address = ai->staged_address;
  descriptor.length = length;
  descriptor.sequence = ai->descriptor_sequence + UINT64_C(1);
  descriptor.dacrate = ai->dacrate;
  descriptor.bitrate = ai->bitrate;
  activate = !ai->active &&
             (ai->control & T64_AI_CONTROL_ENABLE) != 0u;
  if (activate) {
    if (ai->event_generation == UINT64_MAX) {
      return T64_STATUS_SEQUENCE_EXHAUSTED;
    }
    token = ai->event_generation + UINT64_C(1);
    status = reserve_sample_event(machine, ai->dacrate, phase_remainder,
                                  token, &remainder);
    if (status != T64_STATUS_OK) return status;
  }
  slot = dma_slot(ai, ai->descriptor_count);
  ai->descriptors[slot] = descriptor;
  ai->descriptor_count++;
  ai->descriptor_sequence = descriptor.sequence;
  ai->address_staged = false;
  if (activate) {
    ai->active = true;
    ai->paused = false;
    ai->paused_remaining_ticks = 0u;
    ai->active_byte_cursor = 0u;
    ai->latched_dacrate = descriptor.dacrate;
    ai->latched_bitrate = descriptor.bitrate;
    ai->event_generation = token;
    ai->event_token = token;
    ai->phase_remainder = remainder;
  }
  return T64_STATUS_OK;
}

t64_status t64_ai_read_register(const t64_machine *machine, uint32_t address,
                                uint32_t *value_out) {
  uint32_t value;
  const t64_ai_state *ai;

  if (machine == NULL || value_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!machine->has_image || !t64_has_ai_device(machine)) {
    return T64_STATUS_NOT_READY;
  }
  ai = &machine->ai;
  switch (address) {
  case T64_AI_DRAM_ADDR:
  case T64_AI_CONTROL:
    /* Both registers are write-only in the documented AI interface. */
    return T64_STATUS_BUS_UNSUPPORTED;
  case T64_AI_LEN:
    value = ai->active && ai->descriptor_count > 0u
                ? ai->descriptors[ai->descriptor_head].length -
                      ai->active_byte_cursor
                : 0u;
    break;
  case T64_AI_STATUS:
    value = (ai->descriptor_count > 0u ? T64_AI_STATUS_BUSY : 0u) |
            (ai->descriptor_count >= T64_AUDIO_QUEUE_CAPACITY
                 ? T64_AI_STATUS_FULL
                 : 0u);
    break;
  case T64_AI_DACRATE:
    value = ai->dacrate;
    break;
  case T64_AI_BITRATE:
    value = ai->bitrate;
    break;
  default:
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  *value_out = value;
  return T64_STATUS_OK;
}

t64_status t64_ai_write_register(t64_machine *machine, uint32_t address,
                                 uint32_t value) {
  t64_ai_state *ai;
  uint32_t masked;
  uint64_t event_tick;
  uint64_t remaining_ticks;
  t64_event_reservation reservation;
  t64_status status;

  if (machine == NULL) return T64_STATUS_INVALID_ARGUMENT;
  if (!machine->has_image || !t64_has_ai_device(machine)) {
    return T64_STATUS_NOT_READY;
  }
  ai = &machine->ai;
  switch (address) {
  case T64_AI_DRAM_ADDR:
    masked = value & T64_AI_ADDRESS_MASK & UINT32_C(0x00fffff8);
    if (masked >= machine->profile.rdram_bytes) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
    ai->staged_address = masked;
    ai->address_staged = true;
    return T64_STATUS_OK;
  case T64_AI_LEN:
    return submit_dma(machine, value);
  case T64_AI_CONTROL:
    if ((value & ~T64_AI_CONTROL_ENABLE) != 0u) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
    if (value == 0u) {
      if (ai->active && !ai->paused) {
        status = t64_schedule_cancel(machine, T64_AI_DEVICE,
                                     T64_AI_EVENT_SAMPLE, ai->event_token,
                                     &event_tick);
        if (status != T64_STATUS_OK) return status;
        remaining_ticks = event_tick - machine->tick;
        ai->paused = true;
        ai->paused_remaining_ticks = remaining_ticks;
      }
      ai->control = 0u;
      return T64_STATUS_OK;
    }
    if (ai->active && ai->paused) {
      status = t64_schedule_reserve_after(
          machine, ai->paused_remaining_ticks, T64_AI_DEVICE,
          T64_AI_EVENT_SAMPLE, 0u, ai->event_token, t64_ai_event,
          &reservation);
      if (status != T64_STATUS_OK) return status;
      status = t64_schedule_commit(machine, &reservation);
      if (status != T64_STATUS_OK) return status;
      ai->paused = false;
      ai->paused_remaining_ticks = 0u;
      ai->control = T64_AI_CONTROL_ENABLE;
      return T64_STATUS_OK;
    }
    if (!ai->active && ai->descriptor_count > 0u) {
      status = activate_queued_descriptor(machine);
      if (status != T64_STATUS_OK) return status;
    }
    ai->control = T64_AI_CONTROL_ENABLE;
    return T64_STATUS_OK;
  case T64_AI_STATUS:
    t64_mi_clear_pending(machine, T64_MI_INTERRUPT_AI);
    return T64_STATUS_OK;
  case T64_AI_DACRATE:
    masked = value & T64_AI_DACRATE_MASK;
    if (ai->bitrate != 0u && !rate_pair_valid(masked, ai->bitrate)) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
    ai->dacrate = masked;
    return T64_STATUS_OK;
  case T64_AI_BITRATE:
    masked = value & T64_AI_BITRATE_MASK;
    if (!rate_pair_valid(ai->dacrate, masked)) {
      return T64_STATUS_BUS_DEVICE_REFUSED;
    }
    ai->bitrate = masked;
    return T64_STATUS_OK;
  default:
    return T64_STATUS_BUS_UNSUPPORTED;
  }
}

bool t64_ai_event_waits_for_output_capacity(const t64_machine *machine,
                                            const t64_event *event) {
  return machine != NULL && event != NULL &&
         event->device == T64_AI_DEVICE &&
         event->kind == T64_AI_EVENT_SAMPLE && t64_has_ai_device(machine) &&
         machine->ai.active && !machine->ai.paused &&
         event->token == machine->ai.event_token &&
         !machine->ai.packet_reserved &&
         machine->ai.packet_count >= T64_AUDIO_QUEUE_CAPACITY;
}

bool t64_ai_event_can_dispatch(const t64_machine *machine,
                               const t64_event *event) {
  const t64_ai_state *ai;
  const t64_ai_dma_descriptor *descriptor;
  uint64_t delay;
  uint64_t remainder;
  uint64_t sequence_limit;
  size_t queue_limit;
  bool final_frame;
  bool next_descriptor;
  uint32_t next_dacrate;
  uint64_t next_phase_remainder;
  uint32_t sequence_width;

  if (machine == NULL || event == NULL) return false;
  if (event->device != T64_AI_DEVICE || event->kind != T64_AI_EVENT_SAMPLE ||
      !t64_has_ai_device(machine)) {
    return true;
  }
  ai = &machine->ai;
  if (!ai->active || ai->paused || ai->descriptor_count == 0u ||
      event->token != ai->event_token) {
    return true;
  }
  descriptor = &ai->descriptors[ai->descriptor_head];
  if (machine->audio_packets == NULL || descriptor->length < 4u ||
      ai->active_byte_cursor > descriptor->length - 4u ||
      ai->sample_index == UINT64_MAX ||
      (!ai->packet_reserved &&
       ai->packet_count >= T64_AUDIO_QUEUE_CAPACITY)) {
    return false;
  }
  final_frame = ai->active_byte_cursor + 4u == descriptor->length;
  next_descriptor = final_frame && ai->descriptor_count > 1u;
  if ((final_frame && ai->packet_history_count == UINT64_MAX) ||
      (next_descriptor && ai->event_generation == UINT64_MAX)) {
    return false;
  }
  if (final_frame && !next_descriptor) return true;

  next_dacrate = ai->latched_dacrate;
  next_phase_remainder = ai->phase_remainder;
  if (next_descriptor) {
    next_dacrate = ai->dacrate;
    if (next_dacrate != ai->latched_dacrate) next_phase_remainder = 0u;
  }
  if (!sample_delay(next_dacrate, next_phase_remainder, &delay, &remainder) ||
      delay > UINT64_MAX - event->tick) {
    return false;
  }

  queue_limit = (size_t)machine->profile.event_queue_capacity;
  if (queue_limit > T64_EVENT_QUEUE_CAPACITY) {
    queue_limit = T64_EVENT_QUEUE_CAPACITY;
  }
  if (machine->queue_count == 0u ||
      machine->queue_count - 1u >= queue_limit) {
    return false;
  }
  sequence_width = machine->profile.sequence_width_bits;
  if (sequence_width == 0u || sequence_width > 64u ||
      machine->sequence_exhausted) {
    return false;
  }
  sequence_limit = sequence_width == 64u
                       ? UINT64_MAX
                       : (UINT64_C(1) << sequence_width) - UINT64_C(1);
  return machine->next_sequence <= sequence_limit;
}

t64_status t64_ai_event(t64_machine *machine, const t64_event *event) {
  t64_ai_state *ai;
  t64_ai_dma_descriptor *descriptor;
  t64_ai_packet_slot info;
  uint8_t *packet;
  uint64_t next_remainder = 0u;
  uint64_t next_token = UINT64_C(0);
  uint64_t next_phase_remainder;
  uint32_t slot;
  uint32_t output;
  bool final_frame;
  bool next_descriptor;
  t64_status status;

  if (machine == NULL || event == NULL || event->device != T64_AI_DEVICE ||
      event->kind != T64_AI_EVENT_SAMPLE || !t64_has_ai_device(machine)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  ai = &machine->ai;
  if (!ai->active || ai->paused || ai->descriptor_count == 0u ||
      event->token != ai->event_token) {
    return T64_STATUS_OK;
  }
  descriptor = &ai->descriptors[ai->descriptor_head];
  if (machine->audio_packets == NULL || ai->active_byte_cursor >
                                            descriptor->length - 4u ||
      ai->sample_index == UINT64_MAX) {
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }
  if (!ai->packet_reserved &&
      ai->packet_count >= T64_AUDIO_QUEUE_CAPACITY) {
    return T64_STATUS_QUEUE_FULL;
  }
  final_frame = ai->active_byte_cursor + 4u == descriptor->length;
  next_descriptor = final_frame && ai->descriptor_count > 1u;
  if (final_frame && ai->packet_history_count == UINT64_MAX) {
    return T64_STATUS_SEQUENCE_EXHAUSTED;
  }
  if (!final_frame || next_descriptor) {
    uint32_t next_dacrate = ai->latched_dacrate;

    next_phase_remainder = ai->phase_remainder;

    if (next_descriptor) {
      if (ai->event_generation == UINT64_MAX) {
        return T64_STATUS_SEQUENCE_EXHAUSTED;
      }
      next_token = ai->event_generation + UINT64_C(1);
      next_dacrate = ai->dacrate;
      if (next_dacrate != ai->latched_dacrate) {
        next_phase_remainder = 0u;
      }
    } else {
      next_token = ai->event_token;
    }
    status = reserve_sample_event(machine, next_dacrate,
                                  next_phase_remainder, next_token,
                                  &next_remainder);
    if (status != T64_STATUS_OK) return status;
  }

  if (!ai->packet_reserved) {
    ai->output_reservation_slot = output_slot(descriptor->sequence);
    ai->packet_reserved = true;
    ai->packet_length = 0u;
    (void)memset(&ai->packet_info[ai->output_reservation_slot], 0,
                 sizeof ai->packet_info[ai->output_reservation_slot]);
    (void)memset(machine->audio_packets +
                     ai->output_reservation_slot * T64_AUDIO_MAX_PACKET_BYTES,
                 0, T64_AUDIO_MAX_PACKET_BYTES);
  }
  slot = (uint32_t)ai->output_reservation_slot;
  output = slot * (uint32_t)T64_AUDIO_MAX_PACKET_BYTES;
  packet = machine->audio_packets + output;
  info = ai->packet_info[slot];
  if (ai->packet_length == 0u) {
    (void)memset(&info, 0, sizeof info);
    ai->packet_start_tick = event->tick;
    ai->packet_first_sample = ai->sample_index;
    info.struct_size = (uint32_t)sizeof info;
    info.sequence = descriptor->sequence;
    info.first_sample_index = ai->sample_index;
    info.first_tick = event->tick;
    info.rate_numerator = T64_AI_SOURCE_CLOCK;
    info.rate_denominator = (uint64_t)ai->latched_dacrate + UINT64_C(1);
    info.channels = 2u;
    info.bits_per_sample = 16u;
    info.encoding = T64_AUDIO_ENCODING_S16BE;
    copy_text(info.profile_id, sizeof info.profile_id,
              machine->profile.profile_id);
    copy_text(info.build_id, sizeof info.build_id, t64_build_identity());
  }
  (void)memcpy(packet + ai->packet_length,
               machine->ram + descriptor->address + ai->active_byte_cursor,
               4u);
  ai->packet_length += 4u;
  ai->active_byte_cursor += 4u;
  ai->sample_index++;
  info.last_tick = event->tick;
  info.frame_count++;
  info.byte_length = ai->packet_length;
  ai->packet_info[slot] = info;
  ai->packet_lengths[slot] = ai->packet_length;

  if (final_frame) {
    const size_t count_before = ai->descriptor_count;

    hash_completed_packet(machine, slot, &info);
    ai->packet_count++;
    ai->packet_reserved = false;
    ai->packet_length = 0u;
    ai->packet_start_tick = 0u;
    ai->packet_first_sample = 0u;
    ai->descriptor_head = (ai->descriptor_head + 1u) %
                          T64_AUDIO_QUEUE_CAPACITY;
    ai->descriptor_count--;
    ai->active_byte_cursor = 0u;
    if (count_before == T64_AUDIO_QUEUE_CAPACITY) {
      t64_mi_set_pending(machine, T64_MI_INTERRUPT_AI);
    }
    if (ai->descriptor_count == 0u) {
      ai->active = false;
      ai->paused = false;
      ai->paused_remaining_ticks = 0u;
    } else {
      descriptor = &ai->descriptors[ai->descriptor_head];
      descriptor->dacrate = ai->dacrate;
      descriptor->bitrate = ai->bitrate;
      ai->latched_dacrate = ai->dacrate;
      ai->latched_bitrate = ai->bitrate;
      ai->event_generation = next_token;
      ai->event_token = next_token;
      ai->phase_remainder = next_remainder;
    }
  } else {
    ai->phase_remainder = next_remainder;
  }
  return T64_STATUS_OK;
}

void t64_ai_reset_machine_state(t64_machine *machine) {
  size_t offset;

  if (machine == NULL) return;
  offset = 0u;
  while (offset < machine->queue_count) {
    const t64_event event =
        machine->events[(machine->queue_head + offset) %
                        T64_EVENT_QUEUE_CAPACITY];
    if (event.device == T64_AI_DEVICE) {
      uint64_t ignored_tick;
      if (t64_schedule_cancel(machine, event.device, event.kind, event.token,
                              &ignored_tick) != T64_STATUS_OK) {
        offset++;
      }
    } else {
      offset++;
    }
  }
  t64_mi_clear_pending(machine, T64_MI_INTERRUPT_AI);
  (void)memset(&machine->ai, 0, sizeof machine->ai);
  if (machine->audio_packets != NULL) {
    (void)memset(machine->audio_packets, 0, T64_AUDIO_QUEUE_STORAGE_BYTES);
  }
}

t64_status t64_audio_info(const t64_machine *machine,
                          t64_audio_packet_info *info_out) {
  t64_address_range output_range;

  T64_ENTRY_GUARD(machine);
  if (machine == NULL || info_out == NULL ||
      !t64_internal_make_range(info_out, sizeof *info_out, &output_range) ||
      !t64_internal_observation_range_is_disjoint(machine, output_range)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!machine->has_image || !t64_has_ai_device(machine)) {
    return T64_STATUS_NOT_READY;
  }
  if (machine->ai.packet_count == 0u) return T64_STATUS_NOT_READY;
  /* Copy-out of the oldest retained slot into the public struct, member by
   * member (plan 11-04, DR-08): the retained slot is an internal type, so the
   * public struct can change its layout without moving a retained byte. The
   * destination is cleared first, which keeps the bytes (padding included)
   * exactly those of the slot copy this replaces while the layouts are equal. */
  {
    const t64_ai_packet_slot *slot =
        &machine->ai.packet_info[machine->ai.packet_head];

    (void)memset(info_out, 0, sizeof *info_out);
    info_out->struct_size = slot->struct_size;
    info_out->sequence = slot->sequence;
    info_out->first_sample_index = slot->first_sample_index;
    info_out->first_tick = slot->first_tick;
    info_out->last_tick = slot->last_tick;
    info_out->frame_count = slot->frame_count;
    info_out->rate_numerator = slot->rate_numerator;
    info_out->rate_denominator = slot->rate_denominator;
    info_out->channels = slot->channels;
    info_out->bits_per_sample = slot->bits_per_sample;
    info_out->encoding = slot->encoding;
    info_out->byte_length = slot->byte_length;
    (void)memcpy(info_out->profile_id, slot->profile_id,
                 sizeof info_out->profile_id);
    (void)memcpy(info_out->build_id, slot->build_id,
                 sizeof info_out->build_id);
  }
  return T64_STATUS_OK;
}

t64_status t64_audio_copy(t64_machine *machine, uint8_t *bytes_out,
                          size_t capacity, size_t *length_out) {
  t64_address_range length_range;
  t64_address_range bytes_range;
  size_t required;
  uint32_t slot;

  T64_ENTRY_GUARD(machine);
  if (machine == NULL || length_out == NULL ||
      !t64_internal_make_range(length_out, sizeof *length_out, &length_range) ||
      !t64_internal_observation_range_is_disjoint(machine, length_range)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (bytes_out == NULL) {
    if (capacity != 0u) return T64_STATUS_INVALID_ARGUMENT;
  } else if (!t64_internal_make_range(bytes_out, capacity, &bytes_range) ||
             !t64_internal_observation_range_is_disjoint(machine,
                                                         bytes_range) ||
             t64_internal_ranges_overlap(bytes_range, length_range)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!machine->has_image || !t64_has_ai_device(machine)) {
    return T64_STATUS_NOT_READY;
  }
  if (machine->ai.packet_count == 0u) return T64_STATUS_NOT_READY;
  slot = (uint32_t)machine->ai.packet_head;
  required = machine->ai.packet_lengths[slot];
  if (capacity < required) {
    *length_out = required;
    return T64_STATUS_BUFFER_TOO_SMALL;
  }
  if (bytes_out == NULL && required != 0u) return T64_STATUS_INVALID_ARGUMENT;
  (void)memcpy(bytes_out,
               machine->audio_packets +
                   (size_t)slot * T64_AUDIO_MAX_PACKET_BYTES,
               required);
  *length_out = required;
  machine->ai.packet_head =
      (machine->ai.packet_head + 1u) % T64_AUDIO_QUEUE_CAPACITY;
  machine->ai.packet_count--;
  return T64_STATUS_OK;
}
