#include "t64_internal.h"

#include <limits.h>
#include <string.h>

#define T64_SP_REGISTER_BASE UINT32_C(0x04040000)
#define T64_SP_REGISTER_END UINT32_C(0x04040020)
#define T64_SP_DMEM_BASE UINT32_C(0x04000000)
#define T64_SP_IMEM_BASE UINT32_C(0x04001000)
#define T64_SP_MEM_ADDRESS_MASK UINT32_C(0x00001fff)
#define T64_SP_DRAM_ADDRESS_MASK UINT32_C(0x00ffffff)
#define T64_SP_LENGTH_MASK UINT32_C(0x00ffffff)
#define T64_SP_STATUS_DMA_BUSY UINT32_C(0x00000004)
#define T64_SP_STATUS_DMA_FULL UINT32_C(0x00000008)
#define T64_RSP_STATUS_HALT UINT32_C(0x00000001)
#define T64_RSP_STATUS_BROKE UINT32_C(0x00000002)
#define T64_RSP_STATUS_INTR_BREAK UINT32_C(0x00000040)
#define T64_WARM_ZERO_SOURCE_SP_DRAM UINT32_C(0x00802000)

static t64_status t64_rsp_issue_event(t64_machine *machine,
                                      const t64_event *event);

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
  uint32_t skip_field = (raw_length >> 20) & UINT32_C(0x0000000f);
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
      raw_length & T64_SP_LENGTH_MASK,
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
  descriptor_out->raw_length = raw_length & T64_SP_LENGTH_MASK;
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

static t64_status start_dma(t64_machine *machine, bool dram_to_sp,
                            uint32_t raw_length, uint32_t offset) {
  t64_event_reservation reservation;
  t64_sp_dma_descriptor descriptor;
  t64_status status = T64_STATUS_OK;
  uint64_t token;
  uint64_t start_tick;
  bool pending_slot;

  if (t64_has_rsp_functional_profile(machine)) {
    return refuse(machine, offset, T64_STATUS_BUS_DEVICE_REFUSED);
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

t64_status t64_sp_read_register(const t64_machine *machine, uint32_t address,
                                uint32_t *value_out) {
  uint32_t offset;
  uint32_t value;

  if (machine == NULL || value_out == NULL || address < T64_SP_REGISTER_BASE ||
      address >= T64_SP_REGISTER_END) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  offset = address - T64_SP_REGISTER_BASE;
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

t64_status t64_sp_write_register(t64_machine *machine, uint32_t address,
                                 uint32_t value) {
  uint32_t offset;

  if (machine == NULL || address < T64_SP_REGISTER_BASE ||
      address >= T64_SP_REGISTER_END) {
    return T64_STATUS_INVALID_ARGUMENT;
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

static t64_status t64_rsp_schedule_next(t64_machine *machine) {
  t64_event_reservation reservation;
  uint64_t token;
  t64_status status;

  if (machine->rsp.issue_sequence == UINT64_MAX) {
    return T64_STATUS_SEQUENCE_EXHAUSTED;
  }
  token = machine->rsp.issue_sequence + UINT64_C(1);
  status = t64_schedule_reserve_after(
      machine, UINT64_C(1), T64_DEVICE_SP, T64_SP_EVENT_RSP_ISSUE,
      UINT64_C(0), token, t64_rsp_issue_event, &reservation);
  if (status != T64_STATUS_OK) {
    return status;
  }
  status = t64_schedule_commit(machine, &reservation);
  if (status != T64_STATUS_OK) {
    return status;
  }
  machine->rsp.issue_sequence = token;
  machine->rsp.next_issue_tick = reservation.event.tick;
  return T64_STATUS_OK;
}

static uint32_t rsp_read_imem_word(const t64_machine *machine,
                                   uint32_t address) {
  const uint8_t *bytes = machine->sp_imem + (size_t)address;

  return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
         ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

static uint16_t rsp_read_vector_element(const t64_machine *machine,
                                       uint32_t vector, uint32_t lane) {
  const uint8_t *bytes = machine->rsp.vector[vector] + (size_t)(lane * 2u);

  return (uint16_t)(((uint16_t)bytes[0] << 8) | (uint16_t)bytes[1]);
}

static void rsp_write_vector_element(t64_machine *machine, uint32_t vector,
                                     uint32_t lane, uint16_t value) {
  uint8_t *bytes = machine->rsp.vector[vector] + (size_t)(lane * 2u);

  bytes[0] = (uint8_t)(value >> 8);
  bytes[1] = (uint8_t)value;
}

static int32_t rsp_signed16(uint16_t value) {
  return (value & UINT16_C(0x8000)) != 0u
             ? (int32_t)value - INT32_C(65536)
             : (int32_t)value;
}

static t64_status t64_rsp_issue_event(t64_machine *machine,
                                      const t64_event *event) {
  uint32_t pc;
  uint32_t instruction;
  uint32_t operation;
  uint32_t rs;
  uint32_t rt;
  uint32_t rd;
  uint32_t element;
  uint32_t vector_function;
  uint32_t vector_source;
  uint32_t vector_target;
  uint32_t vector_result;
  uint32_t store_address = 0u;
  uint32_t store_value = 0u;
  uint16_t vector_results[8];
  uint64_t accumulator_results[8];
  bool write_gpr = false;
  bool write_vector = false;
  bool write_vector_result = false;
  bool write_dmem_word = false;
  bool is_break = false;
  uint32_t gpr_destination = 0u;
  uint32_t gpr_value = 0u;
  uint32_t vector_destination = 0u;
  uint32_t vector_element = 0u;
  uint16_t vector_value = 0u;
  bool supported = false;
  t64_status status;

  if (machine == NULL || event == NULL || event->device != T64_DEVICE_SP ||
      event->kind != T64_SP_EVENT_RSP_ISSUE) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!t64_has_rsp_functional_profile(machine) ||
      event->token != machine->rsp.issue_sequence) {
    return T64_STATUS_OK;
  }
  pc = machine->rsp.pc & UINT32_C(0x00000fff);
  if ((pc & UINT32_C(3)) != 0u || pc > T64_SP_BANK_BYTES - 4u) {
    machine->rsp_observation_valid = false;
    machine->rsp_observed_outcome = T64_RSP_ATTEMPT_REFUSED;
    return T64_STATUS_OK;
  }
  instruction = rsp_read_imem_word(machine, pc);
  operation = instruction >> 26;
  rs = (instruction >> 21) & UINT32_C(0x1f);
  rt = (instruction >> 16) & UINT32_C(0x1f);
  rd = (instruction >> 11) & UINT32_C(0x1f);
  element = (instruction >> 7) & UINT32_C(0x0f);
  vector_function = instruction & UINT32_C(0x3f);
  vector_source = (instruction >> 11) & UINT32_C(0x1f);
  vector_target = (instruction >> 16) & UINT32_C(0x1f);
  vector_result = (instruction >> 6) & UINT32_C(0x1f);
  machine->rsp_observed_pc = pc;
  machine->rsp_observed_opcode = instruction;
  machine->rsp_observed_outcome = T64_RSP_ATTEMPT_NONE;
  machine->rsp_observation_valid = true;

  if (instruction == 0u) {
    supported = true;
  } else if (operation == UINT32_C(0x09)) {
    uint32_t immediate = instruction & UINT32_C(0xffff);
    uint32_t signed_immediate = (immediate & UINT32_C(0x8000)) != 0u
                                    ? immediate | UINT32_C(0xffff0000)
                                    : immediate;

    write_gpr = rt != 0u;
    gpr_destination = rt;
    gpr_value = machine->rsp.gpr[rs] + signed_immediate;
    supported = true;
  } else if (operation == UINT32_C(0x12) && rs == UINT32_C(4) &&
             (element & UINT32_C(1)) == 0u && element <= UINT32_C(14) &&
             (instruction & UINT32_C(0x7f)) == 0u) {
    write_vector = true;
    vector_destination = rd;
    vector_element = element;
    vector_value = (uint16_t)machine->rsp.gpr[rt];
    supported = true;
  } else if (operation == UINT32_C(0x12) && rs == 0u &&
             (element & UINT32_C(1)) == 0u && element <= UINT32_C(14) &&
             (instruction & UINT32_C(0x7f)) == 0u) {
    int32_t signed_value =
        rsp_signed16(rsp_read_vector_element(machine, rd, element / 2u));

    write_gpr = rt != 0u;
    gpr_destination = rt;
    gpr_value = (uint32_t)signed_value;
    supported = true;
  } else if (operation == UINT32_C(0x12) && (rs & UINT32_C(0x10)) != 0u &&
             vector_function == UINT32_C(0x10)) {
    uint32_t vector_element_code = (instruction >> 21) & UINT32_C(0x0f);
    uint32_t lane;
    bool legal_element = vector_element_code == 0u ||
                         (vector_element_code & UINT32_C(0x0e)) == 2u ||
                         (vector_element_code & UINT32_C(0x0c)) == 4u ||
                         (vector_element_code & UINT32_C(0x08)) != 0u;

    if (legal_element) {
      for (lane = 0u; lane < 8u; lane++) {
        uint32_t target_lane = lane;
        int32_t left;
        int32_t right;
        int32_t sum;

        if (vector_element_code != 0u) {
          if ((vector_element_code & UINT32_C(0x0e)) == 2u) {
            target_lane = (vector_element_code & UINT32_C(1)) + (lane & 6u);
          } else if ((vector_element_code & UINT32_C(0x0c)) == 4u) {
            target_lane = (vector_element_code & 3u) + (lane & 4u);
          } else {
            target_lane = vector_element_code & 7u;
          }
        }
        left = rsp_signed16(
            rsp_read_vector_element(machine, vector_source, lane));
        right = rsp_signed16(
            rsp_read_vector_element(machine, vector_target, target_lane));
        sum = left + right + (int32_t)((machine->rsp.vco >> lane) & 1u);
        if (sum > INT32_C(32767)) {
          vector_results[lane] = UINT16_C(0x7fff);
        } else if (sum < -INT32_C(32768)) {
          vector_results[lane] = UINT16_C(0x8000);
        } else {
          vector_results[lane] = (uint16_t)sum;
        }
        accumulator_results[lane] =
            (machine->rsp.accumulator[lane] & ~UINT64_C(0xffff)) |
            (uint64_t)(uint16_t)sum;
      }
      write_vector_result = true;
      supported = true;
    }
  } else if (operation == UINT32_C(0x2b)) {
    uint32_t immediate = instruction & UINT32_C(0xffff);
    uint32_t signed_immediate = (immediate & UINT32_C(0x8000)) != 0u
                                    ? immediate | UINT32_C(0xffff0000)
                                    : immediate;
    uint32_t effective_address = machine->rsp.gpr[rs] + signed_immediate;

    if ((effective_address & UINT32_C(3)) == 0u) {
      store_address = effective_address & UINT32_C(0x00000fff);
      store_value = machine->rsp.gpr[rt];
      write_dmem_word = true;
      supported = true;
    }
  } else if (instruction == UINT32_C(0x0000000d)) {
    is_break = true;
    supported = true;
  }
  if (!supported) {
    machine->rsp_observed_outcome = T64_RSP_ATTEMPT_REFUSED;
    return T64_STATUS_OK;
  }
  if (!is_break) {
    status = t64_rsp_schedule_next(machine);
    if (status != T64_STATUS_OK) {
      machine->rsp_observed_outcome = T64_RSP_ATTEMPT_REFUSED;
      return status;
    }
  }

  if (write_gpr) {
    machine->rsp.gpr[gpr_destination] = gpr_value;
  }
  if (write_vector) {
    rsp_write_vector_element(machine, vector_destination,
                             vector_element / 2u, vector_value);
  }
  if (write_vector_result) {
    uint32_t lane;

    for (lane = 0u; lane < 8u; lane++) {
      rsp_write_vector_element(machine, vector_result, lane,
                               vector_results[lane]);
      machine->rsp.accumulator[lane] = accumulator_results[lane];
    }
    machine->rsp.vco = 0u;
  }
  if (write_dmem_word) {
    uint8_t *bytes = machine->sp_dmem + (size_t)store_address;

    bytes[0] = (uint8_t)(store_value >> 24);
    bytes[1] = (uint8_t)(store_value >> 16);
    bytes[2] = (uint8_t)(store_value >> 8);
    bytes[3] = (uint8_t)store_value;
  }
  if (is_break) {
    machine->rsp.sp_status |= T64_RSP_STATUS_HALT | T64_RSP_STATUS_BROKE;
    if ((machine->rsp.sp_status & T64_RSP_STATUS_INTR_BREAK) != 0u) {
      t64_mi_set_pending(machine, UINT32_C(1));
    }
    machine->halted = true;
  } else {
    machine->rsp.pc = (pc + UINT32_C(4)) & UINT32_C(0x00000fff);
  }
  machine->rsp.retired_instructions++;
  machine->rsp_observed_outcome = T64_RSP_ATTEMPT_RETIRED;
  return T64_STATUS_OK;
}

t64_status t64_rsp_start(t64_machine *machine) {
  t64_event_reservation reservation;
  uint64_t token;
  t64_status status;

  if (machine == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!t64_has_rsp_functional_profile(machine)) {
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  if (!machine->has_image) {
    return T64_STATUS_NOT_READY;
  }
  if (machine->rsp.issue_sequence == UINT64_MAX) {
    return T64_STATUS_SEQUENCE_EXHAUSTED;
  }
  token = machine->rsp.issue_sequence + UINT64_C(1);
  status = t64_schedule_reserve_after(
      machine, UINT64_C(1), T64_DEVICE_SP, T64_SP_EVENT_RSP_ISSUE,
      UINT64_C(0), token, t64_rsp_issue_event, &reservation);
  if (status != T64_STATUS_OK) {
    return status;
  }
  status = t64_schedule_commit(machine, &reservation);
  if (status != T64_STATUS_OK) {
    return status;
  }
  machine->rsp.issue_sequence = token;
  machine->rsp.next_issue_tick = reservation.event.tick;
  return T64_STATUS_OK;
}
