#include "t64_internal.h"

#include <limits.h>
#include <string.h>

#define T64_SP_DMEM_BASE UINT32_C(0x04000000)
#define T64_SP_IMEM_BASE UINT32_C(0x04001000)
#define T64_SP_MEM_ADDRESS_MASK UINT32_C(0x00001fff)
#define T64_SP_DRAM_ADDRESS_MASK UINT32_C(0x00ffffff)
#define T64_SP_LENGTH_MASK UINT32_C(0x00ffffff)
#define T64_SP_STATUS_DMA_BUSY UINT32_C(0x00000004)
#define T64_SP_STATUS_DMA_FULL UINT32_C(0x00000008)
#define T64_WARM_ZERO_SOURCE_SP_DRAM UINT32_C(0x00802000)

static bool warm_zero_source_sp_transfer(
    const t64_machine *machine, bool dram_to_sp, bool imem,
    uint32_t raw_mem_address, uint32_t raw_dram_address,
    uint32_t raw_length, uint32_t internal_start,
    uint32_t bytes_per_line, uint32_t line_count, uint32_t skip) {
  return machine != NULL && machine->has_image &&
         machine->profile.rdram_bytes == UINT32_C(0x00400000) &&
         machine->guest_devices.policy_version ==
             T64_WARM_DEVICE_POLICY_VERSION &&
         dram_to_sp &&
         ((imem && raw_mem_address == UINT32_C(0x00001000)) ||
          (!imem && raw_mem_address == UINT32_C(0x00000000))) &&
         raw_dram_address == T64_WARM_ZERO_SOURCE_SP_DRAM &&
         raw_length == UINT32_C(0x00000fff) && internal_start == 0u &&
         bytes_per_line == T64_SP_BANK_BYTES && line_count == 1u &&
         skip == 0u;
}

static bool dma_busy(const t64_sp_state *sp) {
  return sp->active.valid || sp->pending.valid;
}

static bool dma_full(const t64_sp_state *sp) {
  return sp->active.valid && sp->pending.valid;
}

static void trace_refusal(t64_machine *machine, uint32_t offset,
                          t64_status reason) {
  uint64_t value = ((uint64_t)reason << 32) | (uint64_t)offset;

  t64_trace_emit(machine, machine->tick, T64_TRACE_DEVICE_SP,
                 T64_TRACE_KIND_SP_REFUSED, value);
}

static t64_status refuse(t64_machine *machine, uint32_t offset,
                         t64_status reason) {
  trace_refusal(machine, offset, reason);
  return reason;
}

static bool decode_descriptor(const t64_machine *machine, bool dram_to_sp,
                              uint32_t raw_length, uint64_t start_tick,
                              t64_sp_dma_descriptor *descriptor_out,
                              t64_status *status_out) {
  uint64_t row_start;
  uint64_t duration_rsp_cycles;
  uint64_t duration_scaled;
  uint64_t duration_ticks;
  uint64_t row_advance;
  uint32_t length_field = raw_length & UINT32_C(0x00000fff);
  uint32_t count_field = (raw_length >> 12) & UINT32_C(0x000000ff);
  bool rsp_profile = t64_has_rsp_functional_profile(machine);
  uint32_t length_mask =
      rsp_profile ? UINT32_MAX : T64_SP_LENGTH_MASK;
  uint32_t skip_field =
      rsp_profile ? (raw_length >> 20) & UINT32_C(0x00000fff)
                  : (raw_length >> 20) & UINT32_C(0x0000000f);
  uint32_t bytes_per_line = (length_field | UINT32_C(7)) + UINT32_C(1);
  uint32_t line_count = count_field + UINT32_C(1);
  uint64_t payload_bytes = (uint64_t)bytes_per_line * (uint64_t)line_count;
  uint64_t dram_start =
      (uint64_t)(machine->sp.dram_address & T64_SP_DRAM_ADDRESS_MASK &
                 ~UINT32_C(7));
  uint64_t dram_end;
  uint32_t rsp_ratio_numerator = machine->profile.cpu_rsp_ratio_numerator;
  uint32_t rsp_ratio_denominator = machine->profile.cpu_rsp_ratio_denominator;
  uint32_t tick_unit_length = 0u;
  bool warm_zero_source;

  if (descriptor_out == NULL || status_out == NULL) {
    return false;
  }
  (void)memset(descriptor_out, 0, sizeof *descriptor_out);
  *status_out = T64_STATUS_RESOURCE_EXHAUSTED;
  if (payload_bytes > (uint64_t)UINT32_MAX ||
      machine->profile.sp_duration_bytes_divisor == 0u ||
      machine->profile.sp_duration_base_rsp_cycles > UINT32_MAX -
          (uint32_t)(payload_bytes /
                     (uint64_t)machine->profile.sp_duration_bytes_divisor)) {
    return false;
  }

  row_start = dram_start;
  row_advance = (uint64_t)bytes_per_line + (uint64_t)skip_field;
  warm_zero_source = warm_zero_source_sp_transfer(
      machine, dram_to_sp, (machine->sp.mem_address & UINT32_C(0x1000)) != 0u,
      machine->sp.mem_address & T64_SP_MEM_ADDRESS_MASK,
      machine->sp.dram_address & T64_SP_DRAM_ADDRESS_MASK,
      raw_length & length_mask,
      machine->sp.mem_address & UINT32_C(0x00000ff8), bytes_per_line,
      line_count, skip_field);
  for (uint32_t row = 0u; row < line_count; row++) {
    if (row_start > UINT64_MAX - (uint64_t)bytes_per_line) {
      *status_out = T64_STATUS_BUS_DEVICE_REFUSED;
      return false;
    }
    dram_end = row_start + (uint64_t)bytes_per_line;
    if (dram_end > (uint64_t)machine->profile.rdram_bytes &&
        !warm_zero_source) {
      *status_out = T64_STATUS_BUS_DEVICE_REFUSED;
      return false;
    }
    if (row + UINT32_C(1) < line_count) {
      if (row_start > UINT64_MAX - row_advance) {
        *status_out = T64_STATUS_BUS_DEVICE_REFUSED;
        return false;
      }
      row_start += row_advance;
    }
  }

  duration_rsp_cycles =
      (uint64_t)machine->profile.sp_duration_base_rsp_cycles +
      payload_bytes / (uint64_t)machine->profile.sp_duration_bytes_divisor;
  /* GuestTick is the profile's integer model-RSP-cycle unit. The recorded
     CPU:RSP ratio converts that duration to the profile's tick ratio. */
  while (tick_unit_length < sizeof "model-rsp-cycle" - 1u &&
         machine->profile.guest_tick_unit[tick_unit_length] != '\0') {
    tick_unit_length++;
  }
  if (tick_unit_length != sizeof "model-rsp-cycle" - 1u ||
      memcmp(machine->profile.guest_tick_unit, "model-rsp-cycle",
             sizeof "model-rsp-cycle" - 1u) != 0 ||
      rsp_ratio_numerator == 0u || rsp_ratio_denominator == 0u ||
      duration_rsp_cycles > UINT64_MAX / (uint64_t)rsp_ratio_numerator) {
    return false;
  }
  duration_scaled = duration_rsp_cycles * (uint64_t)rsp_ratio_numerator;
  if (duration_scaled % (uint64_t)rsp_ratio_denominator != 0u) {
    return false;
  }
  duration_ticks = duration_scaled / (uint64_t)rsp_ratio_denominator;
  if (duration_ticks == 0u || start_tick > UINT64_MAX - duration_ticks) {
    *status_out = T64_STATUS_TICK_OVERFLOW;
    return false;
  }

  descriptor_out->valid = true;
  descriptor_out->dram_to_sp = dram_to_sp;
  descriptor_out->imem = (machine->sp.mem_address & UINT32_C(0x1000)) != 0u;
  descriptor_out->raw_mem_address = machine->sp.mem_address;
  descriptor_out->raw_dram_address = machine->sp.dram_address;
  descriptor_out->raw_length = raw_length & length_mask;
  descriptor_out->aligned_dram_start = (uint32_t)dram_start;
  descriptor_out->internal_start =
      machine->sp.mem_address & UINT32_C(0x00000ff8);
  descriptor_out->bytes_per_line = bytes_per_line;
  descriptor_out->line_count = line_count;
  descriptor_out->skip = skip_field;
  descriptor_out->payload_bytes = (uint32_t)payload_bytes;
  descriptor_out->start_tick = start_tick;
  descriptor_out->end_tick = start_tick + duration_ticks;
  *status_out = T64_STATUS_OK;
  return true;
}

static t64_status start_rsp_dma(t64_machine *machine, bool dram_to_sp,
                                uint32_t raw_length, uint32_t offset) {
  t64_sp_dma_descriptor descriptor;
  t64_status status = T64_STATUS_OK;
  uint64_t token;
  uint64_t start_tick;
  bool pending_slot;

  if (dma_full(&machine->sp)) {
    machine->rsp_observed_refusal_reason =
        T64_RSP_REFUSAL_DMA_QUEUE_FULL;
    return refuse(machine, offset, T64_STATUS_BUS_DEVICE_REFUSED);
  }
  if (machine->sp.token_exhausted) {
    return refuse(machine, offset, T64_STATUS_RESOURCE_EXHAUSTED);
  }
  pending_slot = machine->sp.active.valid;
  start_tick = pending_slot ? machine->sp.active.end_tick : machine->tick;
  if (!decode_descriptor(machine, dram_to_sp, raw_length, start_tick,
                         &descriptor, &status)) {
    if (status == T64_STATUS_BUS_DEVICE_REFUSED) {
      machine->rsp_observed_refusal_reason = T64_RSP_REFUSAL_DMA_SPAN;
    }
    return refuse(machine, offset, status);
  }
  token = machine->sp.next_token;
  descriptor.token = token;

  /* Reserve the shared model opportunity before making queue state visible. */
  status = t64_rsp_ensure_opportunity(machine);
  if (status != T64_STATUS_OK) {
    return refuse(machine, offset, status);
  }
  if (pending_slot) {
    machine->sp.pending = descriptor;
    machine->rsp.pending_dma_token = token;
  } else {
    machine->sp.active = descriptor;
    machine->rsp.dma_setup_remaining = UINT64_C(12);
    machine->rsp.dma_beat_cursor = 0u;
    machine->rsp.active_dma_token = token;
  }
  if (dram_to_sp) {
    machine->sp.read_length = raw_length;
  } else {
    machine->sp.write_length = raw_length;
  }
  if (token == UINT64_MAX) {
    machine->sp.token_exhausted = true;
  } else {
    machine->sp.next_token = token + UINT64_C(1);
  }
  machine->rsp_observed_refusal_reason = T64_RSP_REFUSAL_NONE;
  t64_trace_emit(machine, machine->tick, T64_TRACE_DEVICE_SP,
                 T64_TRACE_KIND_SP_ISSUE, token);
  return T64_STATUS_OK;
}

static t64_status start_dma(t64_machine *machine, bool dram_to_sp,
                            uint32_t raw_length, uint32_t offset) {
  t64_event_reservation reservation;
  t64_sp_dma_descriptor descriptor;
  t64_status status = T64_STATUS_OK;
  uint64_t token;
  uint64_t start_tick;
  bool pending_slot;

  if (t64_has_rsp_functional_profile(machine)) {
    return start_rsp_dma(machine, dram_to_sp, raw_length, offset);
  }
  if (dma_full(&machine->sp)) {
    if (dram_to_sp) {
      machine->sp.read_length = raw_length & T64_SP_LENGTH_MASK;
    } else {
      machine->sp.write_length = raw_length & T64_SP_LENGTH_MASK;
    }
    t64_trace_emit(machine, machine->tick, T64_TRACE_DEVICE_SP,
                   T64_TRACE_KIND_SP_DROP,
                   ((uint64_t)T64_STATUS_BUS_DEVICE_REFUSED << 32) |
                       (uint64_t)offset);
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  if (machine->sp.token_exhausted) {
    return refuse(machine, offset, T64_STATUS_RESOURCE_EXHAUSTED);
  }
  pending_slot = machine->sp.active.valid;
  start_tick = pending_slot ? machine->sp.active.end_tick : machine->tick;
  if (!decode_descriptor(machine, dram_to_sp, raw_length, start_tick,
                         &descriptor, &status)) {
    return refuse(machine, offset, status);
  }
  token = machine->sp.next_token;
  status = t64_schedule_reserve(machine, descriptor.end_tick, T64_DEVICE_SP,
                                T64_SP_EVENT_COMPLETE,
                                (uint64_t)descriptor.payload_bytes, token,
                                t64_sp_complete, &reservation);
  if (status != T64_STATUS_OK) {
    return refuse(machine, offset, status);
  }
  status = t64_schedule_commit(machine, &reservation);
  if (status != T64_STATUS_OK) {
    return refuse(machine, offset, status);
  }

  descriptor.token = token;
  if (pending_slot) {
    machine->sp.pending = descriptor;
  } else {
    machine->sp.active = descriptor;
  }
  if (dram_to_sp) {
    machine->sp.read_length = raw_length & T64_SP_LENGTH_MASK;
  } else {
    machine->sp.write_length = raw_length & T64_SP_LENGTH_MASK;
  }
  if (token == UINT64_MAX) {
    machine->sp.token_exhausted = true;
  } else {
    machine->sp.next_token = token + UINT64_C(1);
  }
  t64_trace_emit(machine, machine->tick, T64_TRACE_DEVICE_SP,
                 T64_TRACE_KIND_SP_ISSUE, token);
  return T64_STATUS_OK;
}

static bool rsp_pc_overlaps_dma_beat(uint32_t beat_start, uint32_t pc) {
  uint32_t beat_byte;
  uint32_t instruction_byte;

  for (beat_byte = 0u; beat_byte < UINT32_C(8); beat_byte++) {
    uint32_t beat_address =
        (beat_start + beat_byte) & UINT32_C(0x00000fff);

    for (instruction_byte = 0u; instruction_byte < UINT32_C(4);
         instruction_byte++) {
      uint32_t instruction_address =
          (pc + instruction_byte) & UINT32_C(0x00000fff);

      if (beat_address == instruction_address) {
        return true;
      }
    }
  }
  return false;
}

bool t64_sp_rsp_dma_active(const t64_machine *machine) {
  return machine != NULL && t64_has_rsp_functional_profile(machine) &&
         dma_busy(&machine->sp);
}

t64_status t64_sp_rsp_dma_preflight(t64_machine *machine, bool rsp_fetches,
                                    bool rsp_touches_dmem,
                                    t64_rsp_dma_step *step_out) {
  const t64_sp_dma_descriptor *descriptor;
  uint64_t cursor;
  uint64_t row;
  uint64_t within_row;
  uint64_t dram_advance;
  uint64_t dram_offset;
  uint32_t internal_offset;
  bool warm_zero_source;
  bool bank_conflict;

  if (machine == NULL || step_out == NULL ||
      !t64_has_rsp_functional_profile(machine)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  (void)memset(step_out, 0, sizeof *step_out);
  if (!machine->sp.active.valid) {
    return machine->sp.pending.valid ? T64_STATUS_BUS_DEVICE_REFUSED
                                     : T64_STATUS_OK;
  }
  descriptor = &machine->sp.active;
  if (descriptor->token == 0u ||
      machine->rsp.active_dma_token != descriptor->token) {
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  if (machine->rsp.dma_event_sequence == UINT64_MAX) {
    return T64_STATUS_SEQUENCE_EXHAUSTED;
  }
  step_out->token = descriptor->token;
  step_out->expected_cursor = machine->rsp.dma_beat_cursor;
  step_out->next_event_sequence =
      machine->rsp.dma_event_sequence + UINT64_C(1);
  step_out->dram_to_sp = descriptor->dram_to_sp;
  step_out->imem = descriptor->imem;
  if (machine->rsp.dma_setup_remaining > 0u) {
    step_out->kind = T64_RSP_DMA_STEP_SETUP;
    return T64_STATUS_OK;
  }

  cursor = machine->rsp.dma_beat_cursor;
  if (descriptor->bytes_per_line == 0u ||
      (descriptor->bytes_per_line & UINT32_C(7)) != 0u ||
      cursor >= (uint64_t)descriptor->payload_bytes ||
      cursor > (uint64_t)descriptor->payload_bytes - UINT64_C(8)) {
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  row = cursor / (uint64_t)descriptor->bytes_per_line;
  within_row = cursor % (uint64_t)descriptor->bytes_per_line;
  if (row > UINT64_MAX /
                ((uint64_t)descriptor->bytes_per_line +
                 (uint64_t)descriptor->skip) ||
      within_row > UINT64_MAX -
                       row * ((uint64_t)descriptor->bytes_per_line +
                              (uint64_t)descriptor->skip)) {
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  dram_advance =
      row * ((uint64_t)descriptor->bytes_per_line + (uint64_t)descriptor->skip) +
      within_row;
  dram_offset = (uint64_t)descriptor->aligned_dram_start + dram_advance;
  if (dram_offset > (uint64_t)UINT32_MAX - UINT64_C(7)) {
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  warm_zero_source = warm_zero_source_sp_transfer(
      machine, descriptor->dram_to_sp, descriptor->imem,
      descriptor->raw_mem_address, descriptor->raw_dram_address,
      descriptor->raw_length, descriptor->internal_start,
      descriptor->bytes_per_line, descriptor->line_count, descriptor->skip);
  if (dram_offset + UINT64_C(8) >
          (uint64_t)machine->profile.rdram_bytes &&
      !warm_zero_source) {
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  internal_offset =
      (descriptor->internal_start + (uint32_t)cursor) &
      (T64_SP_BANK_BYTES - UINT32_C(1));
  bank_conflict =
      (rsp_fetches && descriptor->imem) ||
      (rsp_touches_dmem && !descriptor->imem);
  if (!descriptor->dram_to_sp && warm_zero_source) {
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  if ((machine->rsp.sp_status & T64_RSP_STATUS_HALT) == 0u &&
      descriptor->imem && descriptor->dram_to_sp &&
      rsp_pc_overlaps_dma_beat(internal_offset,
                               machine->rsp.pc & UINT32_C(0x00000fff))) {
    machine->rsp_observed_refusal_reason =
        T64_RSP_REFUSAL_ACTIVE_IMEM_WRITE;
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  if (bank_conflict) {
    machine->rsp_observed_refusal_reason = T64_RSP_REFUSAL_BANK_CONFLICT;
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }

  step_out->kind = T64_RSP_DMA_STEP_BEAT;
  step_out->internal_offset = internal_offset;
  step_out->dram_offset = (uint32_t)dram_offset;
  return T64_STATUS_OK;
}

void t64_sp_rsp_dma_commit(t64_machine *machine,
                           const t64_rsp_dma_step *step) {
  t64_sp_dma_descriptor *descriptor;
  uint32_t byte;
  uint64_t next_cursor;

  if (machine == NULL || step == NULL ||
      step->kind == T64_RSP_DMA_STEP_NONE || !machine->sp.active.valid) {
    return;
  }
  descriptor = &machine->sp.active;
  if (descriptor->token != step->token ||
      machine->rsp.active_dma_token != step->token ||
      machine->rsp.dma_beat_cursor != step->expected_cursor ||
      machine->rsp.dma_event_sequence == UINT64_MAX ||
      machine->rsp.dma_event_sequence + UINT64_C(1) !=
          step->next_event_sequence) {
    return;
  }
  machine->rsp.dma_event_sequence = step->next_event_sequence;
  machine->rsp_observed_dma_cursor = machine->rsp.dma_beat_cursor;
  if (step->kind == T64_RSP_DMA_STEP_SETUP) {
    machine->rsp.dma_setup_remaining--;
    machine->rsp_observed_dma_action = T64_RSP_DMA_ACTION_SETUP;
    return;
  }

  {
    uint8_t *sp_bank = step->imem ? machine->sp_imem : machine->sp_dmem;
    bool warm_zero_source = warm_zero_source_sp_transfer(
        machine, descriptor->dram_to_sp, descriptor->imem,
        descriptor->raw_mem_address, descriptor->raw_dram_address,
        descriptor->raw_length, descriptor->internal_start,
        descriptor->bytes_per_line, descriptor->line_count, descriptor->skip);

    for (byte = 0u; byte < UINT32_C(8); byte++) {
      uint32_t internal_byte =
          (step->internal_offset + byte) & (T64_SP_BANK_BYTES - UINT32_C(1));
      uint32_t dram_byte = step->dram_offset + byte;

      if (step->dram_to_sp) {
        sp_bank[internal_byte] =
            warm_zero_source ? 0u : machine->ram[dram_byte];
        t64_cpu_link_invalidate_range(
            machine,
            (step->imem ? T64_SP_IMEM_BASE : T64_SP_DMEM_BASE) +
                internal_byte,
            1u);
      } else {
        machine->ram[dram_byte] = sp_bank[internal_byte];
        t64_cpu_link_invalidate_range(machine, dram_byte, 1u);
      }
    }
  }

  next_cursor = machine->rsp.dma_beat_cursor + UINT64_C(8);
  machine->rsp.dma_beat_cursor = next_cursor;
  machine->rsp_observed_dma_cursor = next_cursor;
  machine->rsp_observed_dma_action = T64_RSP_DMA_ACTION_BEAT;
  if (next_cursor >= (uint64_t)descriptor->payload_bytes) {
    uint64_t completed_token = descriptor->token;

    t64_trace_emit(machine, machine->tick, T64_TRACE_DEVICE_SP,
                   T64_TRACE_KIND_SP_COMPLETE, completed_token);
    (void)memset(descriptor, 0, sizeof *descriptor);
    machine->rsp_observed_dma_action |= T64_RSP_DMA_ACTION_COMPLETE;
    machine->rsp.dma_beat_cursor = 0u;
    machine->rsp.dma_setup_remaining = 0u;
    machine->rsp.active_dma_token = 0u;
    if (machine->sp.pending.valid) {
      uint64_t promoted_token = machine->sp.pending.token;

      machine->sp.active = machine->sp.pending;
      (void)memset(&machine->sp.pending, 0, sizeof machine->sp.pending);
      machine->rsp.active_dma_token = promoted_token;
      machine->rsp.pending_dma_token = 0u;
      machine->rsp.dma_setup_remaining = UINT64_C(12);
      machine->rsp_observed_dma_action |= T64_RSP_DMA_ACTION_PROMOTE;
      t64_trace_emit(machine, machine->tick, T64_TRACE_DEVICE_SP,
                     T64_TRACE_KIND_SP_PROMOTE, promoted_token);
    }
  }
}

t64_status t64_sp_peek_register(const t64_machine *machine, uint32_t address,
                                uint32_t *value_out) {
  uint32_t offset;
  uint32_t value;

  if (machine == NULL || value_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (t64_has_rsp_functional_profile(machine) &&
      address == T64_SP_PC_ADDRESS) {
    *value_out = machine->rsp.pc & UINT32_C(0x00000ffc);
    return T64_STATUS_OK;
  }
  if (address < T64_SP_REGISTER_BASE || address >= T64_SP_REGISTER_END) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  offset = address - T64_SP_REGISTER_BASE;
  if (t64_has_rsp_functional_profile(machine)) {
    switch (offset) {
    case UINT32_C(0x00):
      value = machine->sp.mem_address;
      break;
    case UINT32_C(0x04):
      value = machine->sp.dram_address;
      break;
    case UINT32_C(0x08):
      value = machine->sp.read_length;
      break;
    case UINT32_C(0x0c):
      value = machine->sp.write_length;
      break;
    case UINT32_C(0x10):
      value = (machine->rsp.sp_status & T64_RSP_STATUS_READ_MASK) |
              (dma_busy(&machine->sp) ? T64_RSP_STATUS_DMA_BUSY : 0u) |
              (dma_full(&machine->sp) ? T64_RSP_STATUS_DMA_FULL : 0u);
      break;
    case UINT32_C(0x14):
      value = dma_full(&machine->sp) ? UINT32_C(1) : UINT32_C(0);
      break;
    case UINT32_C(0x18):
      value = dma_busy(&machine->sp) ? UINT32_C(1) : UINT32_C(0);
      break;
    case UINT32_C(0x1c):
      value = machine->rsp.sp_semaphore ? UINT32_C(1) : UINT32_C(0);
      break;
    default:
      return T64_STATUS_BUS_UNSUPPORTED;
    }
    *value_out = value;
    return T64_STATUS_OK;
  }
  switch (offset) {
  case UINT32_C(0x00):
    value = machine->sp.mem_address;
    break;
  case UINT32_C(0x04):
    value = machine->sp.dram_address;
    break;
  case UINT32_C(0x08):
    value = machine->sp.read_length;
    break;
  case UINT32_C(0x0c):
    value = machine->sp.write_length;
    break;
  case UINT32_C(0x10):
    value = (dma_busy(&machine->sp) ? T64_SP_STATUS_DMA_BUSY : 0u) |
            (dma_full(&machine->sp) ? T64_SP_STATUS_DMA_FULL : 0u);
    break;
  case UINT32_C(0x14):
    value = dma_full(&machine->sp) ? UINT32_C(1) : UINT32_C(0);
    break;
  case UINT32_C(0x18):
    value = dma_busy(&machine->sp) ? UINT32_C(1) : UINT32_C(0);
    break;
  default:
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  *value_out = value;
  return T64_STATUS_OK;
}

t64_status t64_sp_read_register(t64_machine *machine, uint32_t address,
                                uint32_t *value_out) {
  t64_status status = t64_sp_peek_register(machine, address, value_out);

  if (status == T64_STATUS_OK && t64_has_rsp_functional_profile(machine) &&
      address == T64_SP_REGISTER_BASE + UINT32_C(0x1c)) {
    machine->rsp.sp_semaphore = true;
  }
  return status;
}

t64_status t64_rsp_validate_status_command(const t64_machine *machine,
                                           uint32_t command) {
  const uint32_t command_mask = UINT32_C(0x01ffffff);
  bool set_halt = (command & UINT32_C(0x2)) != 0u;
  bool set_single_step = (command & UINT32_C(0x40)) != 0u;

  if (machine == NULL || !t64_has_rsp_functional_profile(machine)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if ((command & ~command_mask) != 0u) {
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  if (set_halt && (machine->rsp.sp_status & T64_RSP_STATUS_HALT) == 0u) {
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  if (set_single_step && machine->rsp.next_issue_tick > machine->tick) {
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  return T64_STATUS_OK;
}

static t64_status write_rsp_status(t64_machine *machine, uint32_t command) {
  uint32_t bit;
  bool clear_halt = (command & UINT32_C(0x1)) != 0u;
  bool set_halt = (command & UINT32_C(0x2)) != 0u;
  bool start = clear_halt && !set_halt &&
               (machine->rsp.sp_status & T64_RSP_STATUS_HALT) != 0u;
  t64_status status = t64_rsp_validate_status_command(machine, command);

  if (status != T64_STATUS_OK) {
    if (status == T64_STATUS_BUS_DEVICE_REFUSED) {
      if (set_halt &&
          (machine->rsp.sp_status & T64_RSP_STATUS_HALT) == 0u) {
        machine->rsp_observed_refusal_reason =
            T64_RSP_REFUSAL_ASYNC_HALT;
      } else if ((command & UINT32_C(0x40)) != 0u &&
                 machine->rsp.next_issue_tick > machine->tick) {
        machine->rsp_observed_refusal_reason =
            T64_RSP_REFUSAL_SINGLE_STEP_IN_FLIGHT;
      }
    }
    return refuse(machine, UINT32_C(0x10), status);
  }
  if (start) {
    status = t64_rsp_ensure_opportunity(machine);
    if (status != T64_STATUS_OK) {
      return refuse(machine, UINT32_C(0x10), status);
    }
  }

  if ((command & UINT32_C(0x4)) != 0u) {
    machine->rsp.sp_status &= ~T64_RSP_STATUS_BROKE;
  }
  if ((command & UINT32_C(0x8)) != 0u) {
    t64_mi_clear_pending(machine, UINT32_C(1));
  }
  if ((command & UINT32_C(0x10)) != 0u) {
    t64_mi_set_pending(machine, UINT32_C(1));
  }
  if ((command & UINT32_C(0x20)) != 0u) {
    machine->rsp.sp_status &= ~T64_RSP_STATUS_SINGLE_STEP;
  }
  if ((command & UINT32_C(0x40)) != 0u) {
    machine->rsp.sp_status |= T64_RSP_STATUS_SINGLE_STEP;
  }
  if ((command & UINT32_C(0x80)) != 0u) {
    machine->rsp.sp_status &= ~T64_RSP_STATUS_INTR_BREAK;
  }
  if ((command & UINT32_C(0x100)) != 0u) {
    machine->rsp.sp_status |= T64_RSP_STATUS_INTR_BREAK;
  }
  for (bit = 0u; bit < 8u; bit++) {
    uint32_t status_bit = UINT32_C(7) + bit;
    uint32_t clear_command = UINT32_C(9) + bit * UINT32_C(2);
    uint32_t set_command = clear_command + UINT32_C(1);

    if ((command & (UINT32_C(1) << clear_command)) != 0u) {
      machine->rsp.sp_status &= ~(UINT32_C(1) << status_bit);
    }
    if ((command & (UINT32_C(1) << set_command)) != 0u) {
      machine->rsp.sp_status |= UINT32_C(1) << status_bit;
    }
  }
  if (clear_halt) {
    machine->rsp.sp_status &= ~T64_RSP_STATUS_HALT;
  }
  if (set_halt) {
    machine->rsp.sp_status |= T64_RSP_STATUS_HALT;
  }
  machine->rsp_observed_refusal_reason = T64_RSP_REFUSAL_NONE;
  return T64_STATUS_OK;
}

t64_status t64_sp_write_register(t64_machine *machine, uint32_t address,
                                 uint32_t value) {
  uint32_t offset;

  if (machine == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (t64_has_rsp_functional_profile(machine)) {
    if (address == T64_SP_PC_ADDRESS) {
      if ((machine->rsp.sp_status & T64_RSP_STATUS_HALT) == 0u) {
        machine->rsp_observed_refusal_reason =
            T64_RSP_REFUSAL_RUNNING_PC_WRITE;
        return refuse(machine, UINT32_C(0x20),
                      T64_STATUS_BUS_DEVICE_REFUSED);
      }
      machine->rsp.pc = value & UINT32_C(0x00000ffc);
      machine->rsp_observed_refusal_reason = T64_RSP_REFUSAL_NONE;
      return T64_STATUS_OK;
    }
  }
  if (address < T64_SP_REGISTER_BASE || address >= T64_SP_REGISTER_END) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (t64_has_rsp_functional_profile(machine)) {
    offset = address - T64_SP_REGISTER_BASE;
    switch (offset) {
    case UINT32_C(0x00):
      machine->sp.mem_address = value & T64_SP_MEM_ADDRESS_MASK;
      return T64_STATUS_OK;
    case UINT32_C(0x04):
      machine->sp.dram_address = value & T64_SP_DRAM_ADDRESS_MASK;
      return T64_STATUS_OK;
    case UINT32_C(0x08):
      return start_dma(machine, true, value, offset);
    case UINT32_C(0x0c):
      return start_dma(machine, false, value, offset);
    case UINT32_C(0x10):
      return write_rsp_status(machine, value);
    case UINT32_C(0x14):
    case UINT32_C(0x18):
      return refuse(machine, offset, T64_STATUS_BUS_DEVICE_REFUSED);
    case UINT32_C(0x1c):
      if (value != 0u) {
        return refuse(machine, offset, T64_STATUS_BUS_DEVICE_REFUSED);
      }
      machine->rsp.sp_semaphore = false;
      return T64_STATUS_OK;
    default:
      return T64_STATUS_BUS_UNSUPPORTED;
    }
  }
  offset = address - T64_SP_REGISTER_BASE;
  switch (offset) {
  case UINT32_C(0x00):
    machine->sp.mem_address = value & T64_SP_MEM_ADDRESS_MASK;
    return T64_STATUS_OK;
  case UINT32_C(0x04):
    machine->sp.dram_address = value & T64_SP_DRAM_ADDRESS_MASK;
    return T64_STATUS_OK;
  case UINT32_C(0x08):
    return start_dma(machine, true, value & T64_SP_LENGTH_MASK, offset);
  case UINT32_C(0x0c):
    return start_dma(machine, false, value & T64_SP_LENGTH_MASK, offset);
  default:
    return T64_STATUS_BUS_UNSUPPORTED;
  }
}

t64_status t64_sp_complete(t64_machine *machine, const t64_event *event) {
  t64_sp_dma_descriptor *descriptor;
  uint8_t *sp_bank;
  uint64_t dram_row;
  uint64_t completed_token;
  uint32_t payload_offset = 0u;
  uint32_t row;
  bool warm_zero_source;

  if (machine == NULL || event == NULL || event->device != T64_DEVICE_SP ||
      event->kind != T64_SP_EVENT_COMPLETE) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  descriptor = &machine->sp.active;
  if (!descriptor->valid || descriptor->token != event->token ||
      descriptor->end_tick != event->tick) {
    return T64_STATUS_OK;
  }
  if (machine->ram == NULL) {
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }
  warm_zero_source = warm_zero_source_sp_transfer(
      machine, descriptor->dram_to_sp, descriptor->imem,
      descriptor->raw_mem_address, descriptor->raw_dram_address,
      descriptor->raw_length, descriptor->internal_start,
      descriptor->bytes_per_line, descriptor->line_count, descriptor->skip);
  completed_token = descriptor->token;

  sp_bank = descriptor->imem ? machine->sp_imem : machine->sp_dmem;
  dram_row = (uint64_t)descriptor->aligned_dram_start;
  for (row = 0u; row < descriptor->line_count; row++) {
    uint32_t byte;

    for (byte = 0u; byte < descriptor->bytes_per_line; byte++) {
      uint32_t internal_offset =
          (descriptor->internal_start + payload_offset) &
          (T64_SP_BANK_BYTES - UINT32_C(1));
      size_t dram_offset = (size_t)(dram_row + (uint64_t)byte);

      if (dram_offset >= (size_t)machine->profile.rdram_bytes &&
          !warm_zero_source) {
        return T64_STATUS_RESOURCE_EXHAUSTED;
      }
      if (descriptor->dram_to_sp) {
        sp_bank[internal_offset] =
            warm_zero_source ? 0u : machine->ram[dram_offset];
        t64_cpu_link_invalidate_range(
            machine,
            (descriptor->imem ? T64_SP_IMEM_BASE : T64_SP_DMEM_BASE) +
                internal_offset,
            1u);
      } else {
        machine->ram[dram_offset] = sp_bank[internal_offset];
        t64_cpu_link_invalidate_range(machine, (uint32_t)dram_offset, 1u);
      }
      payload_offset++;
    }
    dram_row += (uint64_t)descriptor->bytes_per_line +
                (uint64_t)descriptor->skip;
  }

  t64_trace_emit(machine, event->tick, T64_TRACE_DEVICE_SP,
                 T64_TRACE_KIND_SP_COMPLETE, completed_token);
  (void)memset(descriptor, 0, sizeof *descriptor);
  if (machine->sp.pending.valid) {
    uint64_t promoted_token = machine->sp.pending.token;

    machine->sp.active = machine->sp.pending;
    (void)memset(&machine->sp.pending, 0, sizeof machine->sp.pending);
    t64_trace_emit(machine, event->tick, T64_TRACE_DEVICE_SP,
                   T64_TRACE_KIND_SP_PROMOTE, promoted_token);
  }
  return T64_STATUS_OK;
}

void t64_sp_reset_machine_state(t64_machine *machine) {
  if (machine == NULL) {
    return;
  }
  (void)memset(&machine->sp, 0, sizeof machine->sp);
  machine->sp.next_token = UINT64_C(1);
}
