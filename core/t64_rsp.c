#include "t64_internal.h"

#include <limits.h>

#define T64_RSP_STATUS_HALT UINT32_C(0x00000001)
#define T64_RSP_STATUS_BROKE UINT32_C(0x00000002)
#define T64_RSP_STATUS_INTR_BREAK UINT32_C(0x00000040)
#define T64_RSP_DMEM_BASE UINT32_C(0x04000000)

static t64_status t64_rsp_issue_event(t64_machine *machine,
                                      const t64_event *event);

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

t64_status t64_rsp_ensure_opportunity(t64_machine *machine) {
  if (machine == NULL || !t64_has_rsp_functional_profile(machine)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!machine->has_image) {
    return T64_STATUS_NOT_READY;
  }
  if (machine->rsp.next_issue_tick > machine->tick) {
    return T64_STATUS_OK;
  }
  return t64_rsp_schedule_next(machine);
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

static uint64_t rsp_ready_tick_after(uint64_t tick, uint64_t delay) {
  return tick > UINT64_MAX - delay ? UINT64_MAX : tick + delay;
}

static bool rsp_decode_vadd(const t64_machine *machine, uint32_t instruction,
                            uint32_t *source_a_out,
                            uint32_t *source_b_out,
                            uint32_t *destination_out,
                            uint16_t result_out[8],
                            uint64_t accumulator_out[8]) {
  uint32_t rs = (instruction >> 21) & UINT32_C(0x1f);
  uint32_t function = instruction & UINT32_C(0x3f);
  uint32_t operation = instruction >> 26;
  uint32_t element_code = rs & UINT32_C(0x0f);
  uint32_t source_a = (instruction >> 11) & UINT32_C(0x1f);
  uint32_t source_b = (instruction >> 16) & UINT32_C(0x1f);
  uint32_t destination = (instruction >> 6) & UINT32_C(0x1f);
  uint32_t lane;

  if (machine == NULL || source_a_out == NULL || source_b_out == NULL ||
      destination_out == NULL || result_out == NULL ||
      accumulator_out == NULL || operation != UINT32_C(0x12) ||
      (rs & UINT32_C(0x10)) == 0u || function != UINT32_C(0x10)) {
    return false;
  }
  for (lane = 0u; lane < 8u; lane++) {
    uint32_t source_lane = lane;
    int32_t left;
    int32_t right;
    int32_t sum;

    if (element_code != 0u) {
      if ((element_code & UINT32_C(0x0e)) == 2u) {
        source_lane = (element_code & UINT32_C(1)) + (lane & 6u);
      } else if ((element_code & UINT32_C(0x0c)) == 4u) {
        source_lane = (element_code & UINT32_C(3)) + (lane & 4u);
      } else if ((element_code & UINT32_C(0x08)) != 0u) {
        source_lane = element_code & UINT32_C(7);
      } else {
        return false;
      }
    }
    left = rsp_signed16(
        rsp_read_vector_element(machine, source_a, lane));
    right = rsp_signed16(
        rsp_read_vector_element(machine, source_b, source_lane));
    sum = left + right +
          (int32_t)((machine->rsp.vco >> lane) & UINT16_C(1));
    if (sum > INT32_C(32767)) {
      result_out[lane] = UINT16_C(0x7fff);
    } else if (sum < -INT32_C(32768)) {
      result_out[lane] = UINT16_C(0x8000);
    } else {
      result_out[lane] = (uint16_t)sum;
    }
    accumulator_out[lane] =
        (machine->rsp.accumulator[lane] & ~UINT64_C(0xffff)) |
        (uint64_t)(uint16_t)sum;
  }
  *source_a_out = source_a;
  *source_b_out = source_b;
  *destination_out = destination;
  return true;
}

static void rsp_commit_vadd(t64_machine *machine, uint32_t destination,
                            const uint16_t results[8],
                            const uint64_t accumulator_results[8],
                            uint64_t ready_tick) {
  uint32_t lane;

  for (lane = 0u; lane < 8u; lane++) {
    rsp_write_vector_element(machine, destination, lane, results[lane]);
    machine->rsp.accumulator[lane] = accumulator_results[lane];
    machine->rsp.accumulator_ready_tick[lane] = ready_tick;
  }
  machine->rsp.vector_ready_tick[destination] = ready_tick;
  machine->rsp.vco = 0u;
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
  bool is_branch = false;
  bool branch_taken = false;
  bool load_dmem_word = false;
  bool is_store = false;
  bool is_load_or_cop_move = false;
  bool was_delay_slot = false;
  bool pair_vadd = false;
  uint32_t gpr_destination = 0u;
  uint32_t gpr_value = 0u;
  uint32_t vector_destination = 0u;
  uint32_t vector_element = 0u;
  uint16_t vector_value = 0u;
  uint32_t scalar_read_mask = 0u;
  uint32_t vector_read_mask = 0u;
  uint32_t scalar_write_delay = 0u;
  uint32_t vector_write_delay = 0u;
  uint32_t load_address = 0u;
  uint32_t load_value = 0u;
  uint32_t branch_target = 0u;
  uint32_t pair_pc = 0u;
  uint32_t pair_instruction = 0u;
  uint32_t pair_source_a = 0u;
  uint32_t pair_source_b = 0u;
  uint32_t pair_destination = 0u;
  uint16_t pair_vector_results[8];
  uint64_t pair_accumulator_results[8];
  bool supported = false;
  bool dma_active;
  bool dma_conflict;
  t64_rsp_dma_step dma_step;
  t64_status dma_status;
  t64_status status;

  if (machine == NULL || event == NULL || event->device != T64_DEVICE_SP ||
      event->kind != T64_SP_EVENT_RSP_ISSUE) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!t64_has_rsp_functional_profile(machine) ||
      event->token != machine->rsp.issue_sequence) {
    return T64_STATUS_OK;
  }
  machine->rsp_observed_tick = machine->tick;
  machine->rsp_observation_valid = false;
  machine->rsp_observed_pc = 0u;
  machine->rsp_observed_opcode = 0u;
  machine->rsp_observed_outcome = T64_RSP_ATTEMPT_NONE;
  machine->rsp_observed_ready_tick = 0u;
  machine->rsp_observed_second_pc = 0u;
  machine->rsp_observed_second_opcode = 0u;
  machine->rsp_observed_stall_reason = 0u;
  machine->rsp_observed_refusal_reason = T64_RSP_REFUSAL_NONE;
  machine->rsp_observed_dma_action = 0u;
  dma_active = t64_sp_rsp_dma_active(machine);
  if ((machine->rsp.sp_status & T64_RSP_STATUS_HALT) != 0u) {
    if (!dma_active) {
      return T64_STATUS_OK;
    }
    dma_status =
        t64_sp_rsp_dma_preflight(machine, false, false, &dma_step);
    if (dma_status != T64_STATUS_OK) {
      return dma_status;
    }
    status = t64_rsp_schedule_next(machine);
    if (status != T64_STATUS_OK) {
      return status;
    }
    t64_sp_rsp_dma_commit(machine, &dma_step);
    machine->rsp_observed_outcome = T64_RSP_ATTEMPT_NONE;
    return T64_STATUS_OK;
  }
  was_delay_slot = machine->rsp.delay_slot_pending;
  if (machine->rsp.branch_bubble_pending && !was_delay_slot) {
    dma_status =
        t64_sp_rsp_dma_preflight(machine, false, false, &dma_step);
    if (dma_status != T64_STATUS_OK &&
        dma_status != T64_STATUS_BUS_DEVICE_REFUSED) {
      return dma_status;
    }
    dma_conflict = dma_status == T64_STATUS_BUS_DEVICE_REFUSED;
    status = t64_rsp_schedule_next(machine);
    if (status != T64_STATUS_OK) {
      return status;
    }
    if (dma_conflict) {
      if (machine->rsp_observed_refusal_reason == T64_RSP_REFUSAL_NONE) {
        machine->rsp_observed_refusal_reason = T64_RSP_REFUSAL_BANK_CONFLICT;
      }
      machine->rsp_observed_outcome = T64_RSP_ATTEMPT_REFUSED;
      machine->rsp.stall_slots++;
      return T64_STATUS_OK;
    }
    t64_sp_rsp_dma_commit(machine, &dma_step);
    machine->rsp.branch_bubble_pending = false;
    machine->rsp.stall_slots++;
    machine->rsp_observed_pc = machine->rsp.pc & UINT32_C(0x00000fff);
    machine->rsp_observed_stall_reason = T64_RSP_STALL_BRANCH_BUBBLE;
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
    scalar_read_mask |= UINT32_C(1) << rs;
    scalar_write_delay = UINT32_C(1);
    supported = true;
  } else if (operation == UINT32_C(0x12) && rs == UINT32_C(4) &&
             (element & UINT32_C(1)) == 0u && element <= UINT32_C(14) &&
             (instruction & UINT32_C(0x7f)) == 0u) {
    write_vector = true;
    vector_destination = rd;
    vector_element = element;
    vector_value = (uint16_t)machine->rsp.gpr[rt];
    scalar_read_mask |= UINT32_C(1) << rt;
    vector_write_delay = UINT32_C(3);
    is_load_or_cop_move = true;
    supported = true;
  } else if (operation == UINT32_C(0x12) && rs == 0u &&
             (element & UINT32_C(1)) == 0u && element <= UINT32_C(14) &&
             (instruction & UINT32_C(0x7f)) == 0u) {
    int32_t signed_value =
        rsp_signed16(rsp_read_vector_element(machine, rd, element / 2u));

    write_gpr = rt != 0u;
    gpr_destination = rt;
    gpr_value = (uint32_t)signed_value;
    vector_read_mask |= UINT32_C(1) << rd;
    scalar_write_delay = UINT32_C(3);
    is_load_or_cop_move = true;
    supported = true;
  } else if (rsp_decode_vadd(machine, instruction, &vector_source,
                             &vector_target, &vector_result,
                             vector_results, accumulator_results)) {
    write_vector_result = true;
    vector_read_mask |= (UINT32_C(1) << vector_source) |
                        (UINT32_C(1) << vector_target);
    vector_destination = vector_result;
    vector_write_delay = UINT32_C(4);
    supported = true;
  } else if (operation == UINT32_C(0x04) ||
             operation == UINT32_C(0x05)) {
    uint32_t immediate = instruction & UINT32_C(0xffff);
    uint32_t signed_immediate = (immediate & UINT32_C(0x8000)) != 0u
                                    ? immediate | UINT32_C(0xffff0000)
                                    : immediate;
    uint32_t offset = signed_immediate << 2;

    is_branch = true;
    scalar_read_mask |= (UINT32_C(1) << rs) | (UINT32_C(1) << rt);
    branch_taken = operation == UINT32_C(0x04)
                       ? machine->rsp.gpr[rs] == machine->rsp.gpr[rt]
                       : machine->rsp.gpr[rs] != machine->rsp.gpr[rt];
    branch_target = branch_taken
                        ? (pc + UINT32_C(4) + offset) &
                              UINT32_C(0x00000ffc)
                        : (pc + UINT32_C(8)) & UINT32_C(0x00000ffc);
    supported = !was_delay_slot;
  } else if (operation == UINT32_C(0x23)) {
    uint32_t immediate = instruction & UINT32_C(0xffff);
    uint32_t signed_immediate = (immediate & UINT32_C(0x8000)) != 0u
                                    ? immediate | UINT32_C(0xffff0000)
                                    : immediate;
    uint32_t effective_address = machine->rsp.gpr[rs] + signed_immediate;

    scalar_read_mask |= UINT32_C(1) << rs;
    if ((effective_address & UINT32_C(3)) == 0u) {
      load_address = effective_address & UINT32_C(0x00000fff);
      load_value = ((uint32_t)machine->sp_dmem[load_address] << 24) |
                   ((uint32_t)machine->sp_dmem[load_address + 1u] << 16) |
                   ((uint32_t)machine->sp_dmem[load_address + 2u] << 8) |
                   (uint32_t)machine->sp_dmem[load_address + 3u];
      write_gpr = rt != 0u;
      gpr_destination = rt;
      gpr_value = load_value;
      scalar_write_delay = UINT32_C(3);
      load_dmem_word = true;
      is_load_or_cop_move = true;
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
      scalar_read_mask |= (UINT32_C(1) << rs) | (UINT32_C(1) << rt);
      is_store = true;
      supported = true;
    }
  } else if (instruction == UINT32_C(0x0000000d)) {
    is_break = true;
    supported = true;
  }
  if (supported && !is_break && !is_branch && !write_vector_result &&
      !was_delay_slot) {
    uint32_t pair_vector_mask;
    bool vector_register_conflict = false;

    pair_pc = (pc + UINT32_C(4)) & UINT32_C(0x00000fff);
    pair_instruction = rsp_read_imem_word(machine, pair_pc);
    if (rsp_decode_vadd(machine, pair_instruction, &pair_source_a,
                        &pair_source_b, &pair_destination,
                        pair_vector_results, pair_accumulator_results)) {
      pair_vector_mask = (UINT32_C(1) << pair_source_a) |
                         (UINT32_C(1) << pair_source_b) |
                         (UINT32_C(1) << pair_destination);
      if (write_vector &&
          (pair_vector_mask & (UINT32_C(1) << vector_destination)) != 0u) {
        vector_register_conflict = true;
      }
      if ((vector_read_mask & (UINT32_C(1) << pair_destination)) != 0u) {
        vector_register_conflict = true;
      }
      if (!vector_register_conflict) {
        pair_vadd = true;
        machine->rsp_observed_second_pc = pair_pc;
        machine->rsp_observed_second_opcode = pair_instruction;
      }
    }
  }
  if (!supported) {
    machine->rsp_observed_outcome = T64_RSP_ATTEMPT_REFUSED;
    machine->rsp_observed_refusal_reason =
        T64_RSP_REFUSAL_UNSUPPORTED_OPCODE;
    if (!dma_active) {
      return T64_STATUS_OK;
    }
    dma_status =
        t64_sp_rsp_dma_preflight(machine, true, false, &dma_step);
    if (dma_status != T64_STATUS_OK &&
        dma_status != T64_STATUS_BUS_DEVICE_REFUSED) {
      return dma_status;
    }
    dma_conflict = dma_status == T64_STATUS_BUS_DEVICE_REFUSED;
    status = t64_rsp_schedule_next(machine);
    if (status != T64_STATUS_OK) {
      return status;
    }
    if (!dma_conflict) {
      t64_sp_rsp_dma_commit(machine, &dma_step);
    } else {
      machine->rsp.stall_slots++;
    }
    return T64_STATUS_OK;
  }

  {
    uint64_t ready_tick = 0u;
    uint32_t stall_reason = 0u;
    uint32_t reg;

    for (reg = 0u; reg < UINT32_C(32); reg++) {
      if ((scalar_read_mask & (UINT32_C(1) << reg)) != 0u &&
          machine->rsp.scalar_ready_tick[reg] > ready_tick) {
        ready_tick = machine->rsp.scalar_ready_tick[reg];
        stall_reason = T64_RSP_STALL_SCALAR_READY;
      }
      if ((vector_read_mask & (UINT32_C(1) << reg)) != 0u &&
          machine->rsp.vector_ready_tick[reg] > ready_tick) {
        ready_tick = machine->rsp.vector_ready_tick[reg];
        stall_reason = T64_RSP_STALL_VECTOR_READY;
      }
      if (pair_vadd &&
          ((reg == pair_source_a || reg == pair_source_b) &&
           machine->rsp.vector_ready_tick[reg] > ready_tick)) {
        ready_tick = machine->rsp.vector_ready_tick[reg];
        stall_reason = T64_RSP_STALL_VECTOR_READY;
      }
    }
    if (is_store && machine->rsp.memory_access_ready_tick > ready_tick) {
      ready_tick = machine->rsp.memory_access_ready_tick;
      stall_reason = T64_RSP_STALL_LOAD_STORE;
    }
    if (ready_tick > machine->tick) {
      dma_status = t64_sp_rsp_dma_preflight(
          machine, true, write_dmem_word || load_dmem_word, &dma_step);
      if (dma_status != T64_STATUS_OK &&
          dma_status != T64_STATUS_BUS_DEVICE_REFUSED) {
        return dma_status;
      }
      dma_conflict = dma_status == T64_STATUS_BUS_DEVICE_REFUSED;
      status = t64_rsp_schedule_next(machine);
      if (status != T64_STATUS_OK) {
        return status;
      }
      if (!dma_conflict) {
        t64_sp_rsp_dma_commit(machine, &dma_step);
      } else {
        machine->rsp_observed_refusal_reason =
            T64_RSP_REFUSAL_BANK_CONFLICT;
        machine->rsp_observed_outcome = T64_RSP_ATTEMPT_REFUSED;
      }
      machine->rsp_observed_ready_tick = ready_tick;
      machine->rsp_observed_stall_reason =
          dma_conflict ? T64_RSP_STALL_VECTOR_READY : stall_reason;
      machine->rsp.stall_slots++;
      return T64_STATUS_OK;
    }
  }

  dma_status = t64_sp_rsp_dma_preflight(
      machine, true, write_dmem_word || load_dmem_word, &dma_step);
  if (dma_status != T64_STATUS_OK &&
      dma_status != T64_STATUS_BUS_DEVICE_REFUSED) {
    return dma_status;
  }
  dma_conflict = dma_status == T64_STATUS_BUS_DEVICE_REFUSED;
  if (!is_break || dma_active) {
    status = t64_rsp_schedule_next(machine);
    if (status != T64_STATUS_OK) {
      machine->rsp_observed_outcome = T64_RSP_ATTEMPT_REFUSED;
      return status;
    }
  }
  if (dma_conflict) {
    if (machine->rsp_observed_refusal_reason == T64_RSP_REFUSAL_NONE) {
      machine->rsp_observed_refusal_reason = T64_RSP_REFUSAL_BANK_CONFLICT;
    }
    machine->rsp_observed_outcome = T64_RSP_ATTEMPT_REFUSED;
    machine->rsp.stall_slots++;
    return T64_STATUS_OK;
  }
  t64_sp_rsp_dma_commit(machine, &dma_step);

  if (write_gpr) {
    machine->rsp.gpr[gpr_destination] = gpr_value;
    if (scalar_write_delay != 0u) {
      machine->rsp.scalar_ready_tick[gpr_destination] =
          rsp_ready_tick_after(machine->tick, scalar_write_delay);
    }
  }
  if (write_vector) {
    rsp_write_vector_element(machine, vector_destination,
                             vector_element / 2u, vector_value);
    if (vector_write_delay != 0u) {
      machine->rsp.vector_ready_tick[vector_destination] =
          rsp_ready_tick_after(machine->tick, vector_write_delay);
    }
  }
  if (write_vector_result) {
    rsp_commit_vadd(machine, vector_result, vector_results,
                    accumulator_results,
                    rsp_ready_tick_after(machine->tick, vector_write_delay));
  }
  if (pair_vadd) {
    rsp_commit_vadd(machine, pair_destination, pair_vector_results,
                    pair_accumulator_results,
                    rsp_ready_tick_after(machine->tick, UINT64_C(4)));
  }
  if (write_dmem_word) {
    uint8_t *bytes = machine->sp_dmem + (size_t)store_address;

    bytes[0] = (uint8_t)(store_value >> 24);
    bytes[1] = (uint8_t)(store_value >> 16);
    bytes[2] = (uint8_t)(store_value >> 8);
    bytes[3] = (uint8_t)store_value;
    t64_cpu_link_invalidate_range(machine,
                                  T64_RSP_DMEM_BASE + store_address,
                                  UINT32_C(4));
  }
  if (is_load_or_cop_move) {
    machine->rsp.memory_access_ready_tick =
        rsp_ready_tick_after(machine->tick, UINT64_C(3));
  }
  if (is_break) {
    machine->rsp.sp_status |= T64_RSP_STATUS_HALT | T64_RSP_STATUS_BROKE;
    if (was_delay_slot) {
      machine->rsp.delay_slot_pending = false;
      machine->rsp.branch_bubble_pending = false;
    }
    if ((machine->rsp.sp_status & T64_RSP_STATUS_INTR_BREAK) != 0u) {
      t64_mi_set_pending(machine, UINT32_C(1));
    }
  } else if (is_branch) {
    machine->rsp.delay_slot_pending = true;
    machine->rsp.branch_bubble_pending = branch_taken;
    machine->rsp.delay_target = branch_target;
    machine->rsp.pc = (pc + UINT32_C(4)) & UINT32_C(0x00000ffc);
  } else if (was_delay_slot) {
    machine->rsp.delay_slot_pending = false;
    machine->rsp.pc = machine->rsp.delay_target & UINT32_C(0x00000ffc);
  } else {
    machine->rsp.pc = (pc + (pair_vadd ? UINT32_C(8) : UINT32_C(4))) &
                      UINT32_C(0x00000ffc);
  }
  machine->rsp.retired_instructions += pair_vadd ? UINT64_C(2) : UINT64_C(1);
  machine->rsp_observed_outcome = T64_RSP_ATTEMPT_RETIRED;
  if (!is_break &&
      (machine->rsp.sp_status & T64_RSP_STATUS_SINGLE_STEP) != 0u) {
    machine->rsp.sp_status |= T64_RSP_STATUS_HALT;
  }
  return T64_STATUS_OK;
}

t64_status t64_rsp_start(t64_machine *machine) {
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
  status = t64_rsp_ensure_opportunity(machine);
  if (status == T64_STATUS_OK) {
    machine->rsp.sp_status &= ~T64_RSP_STATUS_HALT;
  }
  return status;
}
