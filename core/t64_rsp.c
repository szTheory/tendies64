#include "t64_internal.h"

#include <limits.h>
#include <string.h>

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

static uint32_t rsp_sign_extend16(uint16_t value) {
  return (value & UINT16_C(0x8000)) != 0u
             ? UINT32_C(0xffff0000) | (uint32_t)value
             : (uint32_t)value;
}

static bool rsp_signed_less32(uint32_t left, uint32_t right) {
  bool left_negative = (left & UINT32_C(0x80000000)) != 0u;
  bool right_negative = (right & UINT32_C(0x80000000)) != 0u;

  if (left_negative != right_negative) {
    return left_negative;
  }
  return left < right;
}

static uint32_t rsp_arithmetic_shift_right32(uint32_t value,
                                             uint32_t amount) {
  uint32_t shifted;

  if (amount == 0u) {
    return value;
  }
  shifted = value >> amount;
  if ((value & UINT32_C(0x80000000)) != 0u) {
    shifted |= UINT32_MAX << (32u - amount);
  }
  return shifted;
}

typedef struct {
  bool valid;
  bool load;
  uint32_t memory_count;
  uint16_t memory_address[16];
  uint8_t memory_value[16];
  uint32_t write_count;
  uint8_t write_register[16];
  uint8_t write_byte[16];
  uint8_t write_memory_index[16];
  uint8_t write_transform[16];
  uint32_t write_register_mask;
  uint32_t read_register_mask;
} rsp_vector_memory_plan;

enum {
  RSP_VECTOR_BYTE_DIRECT = 0,
  RSP_VECTOR_BYTE_PACKED_HIGH = 1,
  RSP_VECTOR_BYTE_UNSIGNED_HIGH = 2,
  RSP_VECTOR_BYTE_UNSIGNED_LOW = 3,
  RSP_VECTOR_BYTE_ZERO = 4
};

static bool rsp_select_vector_lane(uint32_t element, uint32_t lane,
                                   uint32_t *source_lane_out) {
  uint32_t source_lane;

  if (source_lane_out == NULL || element > UINT32_C(15) ||
      lane > UINT32_C(7)) {
    return false;
  }
  if (element == 0u) {
    source_lane = lane;
  } else if ((element & UINT32_C(0x0e)) == 2u) {
    source_lane = (element & UINT32_C(1)) + (lane & UINT32_C(6));
  } else if ((element & UINT32_C(0x0c)) == 4u) {
    source_lane = (element & UINT32_C(3)) + (lane & UINT32_C(4));
  } else if ((element & UINT32_C(0x08)) != 0u) {
    source_lane = element & UINT32_C(7);
  } else {
    return false;
  }
  *source_lane_out = source_lane;
  return true;
}

static bool rsp_vector_memory_add_address(rsp_vector_memory_plan *plan,
                                          uint32_t address,
                                          uint32_t *index_out) {
  uint32_t index;

  if (plan == NULL || index_out == NULL || plan->memory_count >= 16u) {
    return false;
  }
  index = plan->memory_count++;
  plan->memory_address[index] = (uint16_t)(address & UINT32_C(0x0fff));
  *index_out = index;
  return true;
}

static bool rsp_vector_memory_add_load_byte(rsp_vector_memory_plan *plan,
                                            uint32_t reg, uint32_t byte,
                                            uint32_t memory_index,
                                            uint32_t transform) {
  uint32_t index;

  if (plan == NULL || reg >= 32u || byte >= 16u || memory_index >= 16u ||
      transform > RSP_VECTOR_BYTE_ZERO || plan->write_count >= 16u) {
    return false;
  }
  index = plan->write_count++;
  plan->write_register[index] = (uint8_t)reg;
  plan->write_byte[index] = (uint8_t)byte;
  plan->write_memory_index[index] = (uint8_t)memory_index;
  plan->write_transform[index] = (uint8_t)transform;
  plan->write_register_mask |= UINT32_C(1) << reg;
  return true;
}

static bool rsp_vector_memory_add_store_byte(rsp_vector_memory_plan *plan,
                                             uint32_t address,
                                             uint32_t value) {
  uint32_t index;

  if (!rsp_vector_memory_add_address(plan, address, &index)) {
    return false;
  }
  plan->memory_value[index] = (uint8_t)value;
  return true;
}

static bool rsp_plan_vector_memory(const t64_machine *machine,
                                   uint32_t instruction,
                                   rsp_vector_memory_plan *plan) {
  uint32_t operation = instruction >> 26;
  uint32_t base_register = (instruction >> 21) & UINT32_C(0x1f);
  uint32_t vector_register = (instruction >> 16) & UINT32_C(0x1f);
  uint32_t subopcode = (instruction >> 11) & UINT32_C(0x1f);
  uint32_t element = (instruction >> 7) & UINT32_C(0x0f);
  uint32_t raw_offset = instruction & UINT32_C(0x7f);
  uint32_t offset = (raw_offset & UINT32_C(0x40)) != 0u
                        ? raw_offset | UINT32_C(0xffffff80)
                        : raw_offset;
  uint32_t offset_scale;
  uint32_t address;
  uint32_t size;
  uint32_t index;
  uint32_t register_base;
  uint32_t rotation;
  bool load;

  if (machine == NULL || plan == NULL ||
      (operation != UINT32_C(0x32) && operation != UINT32_C(0x3a))) {
    return false;
  }
  (void)memset(plan, 0, sizeof *plan);
  load = operation == UINT32_C(0x32);
  plan->load = load;
  switch (subopcode) {
  case 0u:
    offset_scale = 0u;
    size = 1u;
    break;
  case 1u:
    offset_scale = 1u;
    size = 2u;
    if ((element & UINT32_C(1)) != 0u || element > 14u) {
      return false;
    }
    break;
  case 2u:
    offset_scale = 2u;
    size = 4u;
    if ((element & UINT32_C(3)) != 0u || element > 12u) {
      return false;
    }
    break;
  case 3u:
    offset_scale = 3u;
    size = 8u;
    if (element != 0u && element != 8u) {
      return false;
    }
    break;
  case 4u:
  case 5u:
    offset_scale = 4u;
    size = 0u;
    if (element != 0u) {
      return false;
    }
    break;
  case 6u:
  case 7u:
    offset_scale = 3u;
    size = 8u;
    if (element != 0u) {
      return false;
    }
    break;
  case 8u:
    offset_scale = 4u;
    size = 8u;
    if (element != 0u) {
      return false;
    }
    break;
  case 9u:
    offset_scale = 4u;
    size = 4u;
    if (element != 0u && element != 8u) {
      return false;
    }
    break;
  case 10u:
    /* LWC2 subopcode 10 is reserved; SWC2 subopcode 10 is SWV. */
    if (load) {
      return false;
    }
    offset_scale = 4u;
    size = 16u;
    if ((element & UINT32_C(1)) != 0u || element > 14u) {
      return false;
    }
    break;
  case 11u:
    offset_scale = 4u;
    size = 16u;
    if ((element & UINT32_C(1)) != 0u || element > 14u) {
      return false;
    }
    break;
  default:
    return false;
  }
  address = (machine->rsp.gpr[base_register] + (offset << offset_scale)) &
            UINT32_C(0x0fff);
  /* Guide Table3-1 leaves other packed/transpose alignments undefined.
   * This profile refuses them rather than inventing their memory effects. */
  if ((subopcode == 8u && (address & UINT32_C(15)) > 1u) ||
      (subopcode == 9u && (address & UINT32_C(15)) > 3u) ||
      (subopcode >= 10u && (address & UINT32_C(15)) != 0u)) {
    return false;
  }

  if (subopcode <= 3u) {
    for (index = 0u; index < size; index++) {
      uint32_t memory_index;
      uint32_t vector_byte = element + index;
      uint32_t memory_address = address + index;

      if (load) {
        if (!rsp_vector_memory_add_address(plan, memory_address,
                                          &memory_index) ||
            !rsp_vector_memory_add_load_byte(
                plan, vector_register, vector_byte, memory_index,
                RSP_VECTOR_BYTE_DIRECT)) {
          return false;
        }
      } else if (!rsp_vector_memory_add_store_byte(
                     plan, memory_address,
                     machine->rsp.vector[vector_register][vector_byte])) {
        return false;
      }
    }
    if (!load) {
      plan->read_register_mask |= UINT32_C(1) << vector_register;
    }
  } else if (subopcode == 4u || subopcode == 5u) {
    uint32_t count = subopcode == 4u
                         ? UINT32_C(16) - (address & UINT32_C(15))
                         : address & UINT32_C(15);
    uint32_t aligned_address = address & UINT32_C(0x0ff0);
    uint32_t vector_start = subopcode == 4u ? 0u : UINT32_C(16) - count;

    for (index = 0u; index < count; index++) {
      uint32_t memory_index;
      uint32_t memory_address = subopcode == 4u
                                    ? address + index
                                    : aligned_address + index;
      uint32_t vector_byte = vector_start + index;

      if (load) {
        if (!rsp_vector_memory_add_address(plan, memory_address,
                                          &memory_index) ||
            !rsp_vector_memory_add_load_byte(
                plan, vector_register, vector_byte, memory_index,
                RSP_VECTOR_BYTE_DIRECT)) {
          return false;
        }
      } else if (!rsp_vector_memory_add_store_byte(
                     plan, memory_address,
                     machine->rsp.vector[vector_register][vector_byte])) {
        return false;
      }
    }
    if (!load) {
      plan->read_register_mask |= UINT32_C(1) << vector_register;
    }
  } else if (subopcode >= 6u && subopcode <= 9u) {
    uint32_t source_step = subopcode == 8u ? 2u :
                           subopcode == 9u ? 4u : 1u;
    uint32_t lane_count = subopcode == 9u ? 4u : 8u;
    uint32_t destination_lane = subopcode == 9u ? element / 2u : 0u;
    uint32_t transform = subopcode == 6u
                             ? RSP_VECTOR_BYTE_PACKED_HIGH
                             : RSP_VECTOR_BYTE_UNSIGNED_HIGH;

    for (index = 0u; index < lane_count; index++) {
      uint32_t memory_index;
      uint32_t memory_address = address + index * source_step;
      uint32_t lane = destination_lane + index;

      if (load) {
        if (!rsp_vector_memory_add_address(plan, memory_address,
                                          &memory_index) ||
            !rsp_vector_memory_add_load_byte(
                plan, vector_register, lane * 2u, memory_index, transform) ||
            !rsp_vector_memory_add_load_byte(
                plan, vector_register, lane * 2u + 1u, memory_index,
                subopcode == 6u ? RSP_VECTOR_BYTE_ZERO
                                : RSP_VECTOR_BYTE_UNSIGNED_LOW)) {
          return false;
        }
      } else {
        uint16_t value = rsp_read_vector_element(machine, vector_register,
                                                lane);
        uint32_t byte_value;

        if (subopcode == 6u) {
          byte_value = value >> 8;
        } else if (subopcode == 7u) {
          byte_value = (value >> 7) & UINT32_C(0xff);
        } else {
          byte_value = (value >> 7) & UINT32_C(0xff);
        }
        if (!rsp_vector_memory_add_store_byte(plan, memory_address,
                                             byte_value)) {
          return false;
        }
      }
    }
    if (!load) {
      plan->read_register_mask |= UINT32_C(1) << vector_register;
    }
  } else {
    register_base = vector_register & UINT32_C(0x18);
    rotation = element / 2u;
    if (subopcode == 10u || subopcode == 11u) {
      uint32_t aligned_address = address & UINT32_C(0x0ff0);

      for (index = 0u; index < 8u; index++) {
        uint32_t selected_register =
            register_base | ((index + rotation) & UINT32_C(7));
        uint32_t lane = subopcode == 10u
                            ? (index + rotation) & UINT32_C(7)
                            : index;

        if (load) {
          uint32_t high_index;
          uint32_t low_index;

          if (!rsp_vector_memory_add_address(plan,
                                             aligned_address + index * 2u,
                                             &high_index) ||
              !rsp_vector_memory_add_address(plan,
                                             aligned_address + index * 2u + 1u,
                                             &low_index) ||
              !rsp_vector_memory_add_load_byte(
                  plan, selected_register, lane * 2u, high_index,
                  RSP_VECTOR_BYTE_DIRECT) ||
              !rsp_vector_memory_add_load_byte(
                  plan, selected_register, lane * 2u + 1u, low_index,
                  RSP_VECTOR_BYTE_DIRECT)) {
            return false;
          }
        } else {
          uint16_t value = rsp_read_vector_element(machine,
                                                  selected_register, lane);

          if (!rsp_vector_memory_add_store_byte(
                  plan, aligned_address + index * 2u, value >> 8) ||
              !rsp_vector_memory_add_store_byte(
                  plan, aligned_address + index * 2u + 1u, value)) {
            return false;
          }
        }
        plan->read_register_mask |= UINT32_C(1) << selected_register;
      }
    }
  }
  if (load) {
    plan->read_register_mask |= plan->write_register_mask;
  }
  plan->valid = true;
  return true;
}

static uint8_t rsp_vector_memory_loaded_byte(
    const rsp_vector_memory_plan *plan, uint32_t write_index) {
  uint32_t memory_index = plan->write_memory_index[write_index];
  uint32_t value = plan->memory_value[memory_index];

  switch (plan->write_transform[write_index]) {
  case RSP_VECTOR_BYTE_PACKED_HIGH:
    return (uint8_t)value;
  case RSP_VECTOR_BYTE_UNSIGNED_HIGH:
    return (uint8_t)(value >> 1);
  case RSP_VECTOR_BYTE_UNSIGNED_LOW:
    return (uint8_t)((value & UINT32_C(1)) << 7);
  case RSP_VECTOR_BYTE_ZERO:
    return 0u;
  default:
    return (uint8_t)value;
  }
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
    uint32_t source_lane;
    int32_t left;
    int32_t right;
    int32_t sum;

    if (!rsp_select_vector_lane(element_code, lane, &source_lane)) {
      return false;
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
  uint32_t store_width = 0u;
  uint16_t vector_results[8];
  uint64_t accumulator_results[8];
  bool write_gpr = false;
  bool write_vector = false;
  bool write_vmov = false;
  bool write_vector_result = false;
  bool vector_memory_transfer = false;
  bool write_vu_control = false;
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
  uint32_t vmov_destination = 0u;
  uint32_t vmov_destination_lane = 0u;
  uint16_t vmov_value = 0u;
  uint16_t vector_value = 0u;
  uint32_t scalar_read_mask = 0u;
  uint32_t vector_read_mask = 0u;
  uint32_t scalar_write_delay = 0u;
  uint32_t vector_write_delay = 0u;
  uint32_t load_address = 0u;
  uint32_t load_value = 0u;
  uint32_t load_width = 0u;
  uint32_t cp0_register = 0u;
  uint32_t cp0_value = 0u;
  uint32_t vu_control_register = 0u;
  uint32_t vu_control_value = 0u;
  rsp_vector_memory_plan vector_memory_plan = {0};
  bool read_cp0 = false;
  bool write_cp0 = false;
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
      machine->rsp_observed_stall_reason = T64_RSP_STALL_BANK_CONFLICT;
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
  } else if (operation == UINT32_C(0x08) ||
             operation == UINT32_C(0x09)) {
    uint32_t immediate = rsp_sign_extend16((uint16_t)instruction);

    write_gpr = rt != 0u;
    gpr_destination = rt;
    gpr_value = machine->rsp.gpr[rs] + immediate;
    scalar_read_mask |= UINT32_C(1) << rs;
    scalar_write_delay = UINT32_C(1);
    supported = true;
  } else if (operation == UINT32_C(0x00)) {
    uint32_t function = instruction & UINT32_C(0x3f);
    uint32_t shift = (instruction >> 6) & UINT32_C(0x1f);
    uint32_t left = machine->rsp.gpr[rs];
    uint32_t right = machine->rsp.gpr[rt];
    bool valid = false;

    if (function == UINT32_C(0x0d)) {
      is_break = true;
      supported = true;
    } else if (function == UINT32_C(0x08) ||
               function == UINT32_C(0x09)) {
      bool is_link = function == UINT32_C(0x09);

      valid = rt == 0u && shift == 0u &&
              (is_link || rd == 0u);
      if (is_link && rd == rs) {
        valid = false;
      }
      if ((left & UINT32_C(3)) != 0u) {
        valid = false;
      }
      if (valid) {
        is_branch = true;
        branch_taken = true;
        branch_target = left & UINT32_C(0x00000ffc);
        scalar_read_mask |= UINT32_C(1) << rs;
        if (is_link) {
          write_gpr = rd != 0u;
          gpr_destination = rd;
          gpr_value = (pc + UINT32_C(8)) & UINT32_C(0x00000ffc);
        }
        supported = !was_delay_slot;
      }
    } else if (function == UINT32_C(0x00) ||
               function == UINT32_C(0x02) ||
               function == UINT32_C(0x03)) {
      valid = rs == 0u;
      if (valid) {
        if (function == UINT32_C(0x00)) {
          gpr_value = right << shift;
        } else if (function == UINT32_C(0x02)) {
          gpr_value = right >> shift;
        } else {
          gpr_value = rsp_arithmetic_shift_right32(right, shift);
        }
      }
    } else if (function == UINT32_C(0x04) ||
               function == UINT32_C(0x06) ||
               function == UINT32_C(0x07)) {
      valid = shift == 0u;
      if (valid) {
        uint32_t amount = left & UINT32_C(0x1f);

        if (function == UINT32_C(0x04)) {
          gpr_value = right << amount;
        } else if (function == UINT32_C(0x06)) {
          gpr_value = right >> amount;
        } else {
          gpr_value = rsp_arithmetic_shift_right32(right, amount);
        }
      }
    } else if (function == UINT32_C(0x20) ||
               function == UINT32_C(0x21) ||
               function == UINT32_C(0x22) ||
               function == UINT32_C(0x23) ||
               function == UINT32_C(0x24) ||
               function == UINT32_C(0x25) ||
               function == UINT32_C(0x26) ||
               function == UINT32_C(0x27) ||
               function == UINT32_C(0x2a) ||
               function == UINT32_C(0x2b)) {
      valid = shift == 0u;
      if (valid) {
        switch (function) {
        case UINT32_C(0x20):
        case UINT32_C(0x21):
          gpr_value = left + right;
          break;
        case UINT32_C(0x22):
        case UINT32_C(0x23):
          gpr_value = left - right;
          break;
        case UINT32_C(0x24):
          gpr_value = left & right;
          break;
        case UINT32_C(0x25):
          gpr_value = left | right;
          break;
        case UINT32_C(0x26):
          gpr_value = left ^ right;
          break;
        case UINT32_C(0x27):
          gpr_value = ~(left | right);
          break;
        case UINT32_C(0x2a):
          gpr_value = rsp_signed_less32(left, right) ? 1u : 0u;
          break;
        case UINT32_C(0x2b):
          gpr_value = left < right ? 1u : 0u;
          break;
        default:
          break;
        }
      }
    }
    if (valid && function != UINT32_C(0x08) &&
        function != UINT32_C(0x09) && function != UINT32_C(0x0d)) {
      write_gpr = rd != 0u;
      gpr_destination = rd;
      scalar_read_mask |= UINT32_C(1) << rt;
      if (function != UINT32_C(0x00) && function != UINT32_C(0x02) &&
          function != UINT32_C(0x03)) {
        scalar_read_mask |= UINT32_C(1) << rs;
      }
      scalar_write_delay = UINT32_C(1);
      supported = true;
    }
  } else if (operation == UINT32_C(0x0c) ||
             operation == UINT32_C(0x0d) ||
             operation == UINT32_C(0x0e)) {
    uint32_t immediate = instruction & UINT32_C(0xffff);
    uint32_t left = machine->rsp.gpr[rs];

    if (operation == UINT32_C(0x0c)) {
      gpr_value = left & immediate;
    } else if (operation == UINT32_C(0x0d)) {
      gpr_value = left | immediate;
    } else {
      gpr_value = left ^ immediate;
    }
    write_gpr = rt != 0u;
    gpr_destination = rt;
    scalar_read_mask |= UINT32_C(1) << rs;
    scalar_write_delay = UINT32_C(1);
    supported = true;
  } else if (operation == UINT32_C(0x0f) && rs == 0u) {
    write_gpr = rt != 0u;
    gpr_destination = rt;
    gpr_value = (instruction & UINT32_C(0xffff)) << 16;
    scalar_write_delay = UINT32_C(1);
    supported = true;
  } else if (operation == UINT32_C(0x0a) ||
             operation == UINT32_C(0x0b)) {
    uint32_t left = machine->rsp.gpr[rs];
    uint32_t immediate = rsp_sign_extend16((uint16_t)instruction);

    gpr_value = (operation == UINT32_C(0x0a)
                     ? rsp_signed_less32(left, immediate)
                     : left < immediate)
                    ? 1u
                    : 0u;
    write_gpr = rt != 0u;
    gpr_destination = rt;
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
    vector_read_mask |= UINT32_C(1) << rd;
    scalar_read_mask |= UINT32_C(1) << rt;
    vector_write_delay = UINT32_C(4);
    is_load_or_cop_move = true;
    is_store = true;
    supported = true;
  } else if (operation == UINT32_C(0x12) &&
             (rs == UINT32_C(2) || rs == UINT32_C(6)) &&
             (instruction & UINT32_C(0x7f)) == 0u && rd <= 2u) {
    vu_control_register = rd;
    if (rs == UINT32_C(2)) {
      if (rd == 0u) {
        gpr_value = machine->rsp.vco;
      } else if (rd == 1u) {
        gpr_value = machine->rsp.vcc;
      } else {
        gpr_value = machine->rsp.vce;
      }
      /* VU moves sign-extend their 16-bit result into the scalar GPR. */
      gpr_value = rsp_sign_extend16((uint16_t)gpr_value);
      write_gpr = rt != 0u;
      gpr_destination = rt;
      scalar_write_delay = UINT32_C(3);
      is_load_or_cop_move = true;
      is_store = true;
    } else {
      write_vu_control = true;
      vu_control_value = machine->rsp.gpr[rt];
      scalar_read_mask |= UINT32_C(1) << rt;
      is_load_or_cop_move = true;
      is_store = true;
    }
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
    is_store = true;
    supported = true;
  } else if (operation == UINT32_C(0x12) && (rs & UINT32_C(0x10)) != 0u &&
             (instruction & UINT32_C(0x3f)) == UINT32_C(0x33)) {
    uint32_t element_code = rs & UINT32_C(0x0f);
    uint32_t selected_source_lane;
    uint32_t destination_vector = (instruction >> 6) & UINT32_C(0x1f);

    if (rd <= 7u && rsp_select_vector_lane(element_code, rd,
                                           &selected_source_lane)) {
      write_vmov = true;
      vmov_destination = destination_vector;
      vmov_destination_lane = rd;
      vmov_value = rsp_read_vector_element(machine, rt, selected_source_lane);
      vector_read_mask |= (UINT32_C(1) << rt) |
                          (UINT32_C(1) << destination_vector);
      vector_write_delay = UINT32_C(4);
      is_store = true;
      supported = true;
    }
  } else if (rsp_decode_vadd(machine, instruction, &vector_source,
                             &vector_target, &vector_result,
                             vector_results, accumulator_results)) {
    write_vector_result = true;
    vector_read_mask |= (UINT32_C(1) << vector_source) |
                        (UINT32_C(1) << vector_target);
    vector_destination = vector_result;
    vector_write_delay = UINT32_C(4);
    supported = true;
  } else if ((operation == UINT32_C(0x32) ||
              operation == UINT32_C(0x3a)) &&
             rsp_plan_vector_memory(machine, instruction,
                                    &vector_memory_plan)) {
    vector_memory_transfer = true;
    vector_read_mask |= vector_memory_plan.read_register_mask;
    vector_write_delay = UINT32_C(4);
    is_load_or_cop_move = true;
    is_store = true;
    supported = true;
  } else if (operation == UINT32_C(0x10) &&
             (rs == UINT32_C(0) || rs == UINT32_C(4))) {
    uint32_t control_bits = instruction & UINT32_C(0x7ff);

    cp0_register = rd;
    if (control_bits == 0u && cp0_register >= UINT32_C(8) &&
        cp0_register <= UINT32_C(15)) {
      machine->rsp_observed_refusal_reason =
          T64_RSP_REFUSAL_DP_UNAVAILABLE;
    } else if (control_bits == 0u && cp0_register <= UINT32_C(7)) {
      if (rs == UINT32_C(0)) {
        status = t64_sp_peek_register(
            machine, T64_SP_REGISTER_BASE + cp0_register * UINT32_C(4),
            &cp0_value);
        if (status == T64_STATUS_OK) {
          read_cp0 = true;
          write_gpr = rt != 0u;
          gpr_destination = rt;
          gpr_value = cp0_value;
          scalar_write_delay = UINT32_C(3);
          is_load_or_cop_move = true;
          is_store = true;
          supported = true;
        }
      } else if (cp0_register == UINT32_C(5) ||
                 cp0_register == UINT32_C(6)) {
        machine->rsp_observed_refusal_reason =
            T64_RSP_REFUSAL_CP0_READ_ONLY;
      } else if (cp0_register == UINT32_C(7) &&
                 machine->rsp.gpr[rt] != 0u) {
        machine->rsp_observed_refusal_reason =
            T64_RSP_REFUSAL_CP0_INVALID_VALUE;
      } else {
        write_cp0 = true;
        cp0_value = machine->rsp.gpr[rt];
        if (cp0_register == UINT32_C(4) &&
            t64_rsp_validate_status_command(machine, cp0_value) !=
                T64_STATUS_OK) {
          write_cp0 = false;
          machine->rsp_observed_refusal_reason =
              T64_RSP_REFUSAL_CP0_INVALID_VALUE;
        } else {
          scalar_read_mask |= UINT32_C(1) << rt;
          is_load_or_cop_move = true;
          is_store = true;
          supported = true;
        }
      }
    }
  } else if (operation == UINT32_C(0x04) ||
             operation == UINT32_C(0x05) ||
             operation == UINT32_C(0x06) ||
             operation == UINT32_C(0x07) ||
             operation == UINT32_C(0x01)) {
    uint32_t immediate = instruction & UINT32_C(0xffff);
    uint32_t offset = rsp_sign_extend16((uint16_t)immediate) << 2;
    uint32_t left = machine->rsp.gpr[rs];
    uint32_t right = machine->rsp.gpr[rt];
    bool valid = true;

    is_branch = true;
    if (operation == UINT32_C(0x04)) {
      branch_taken = left == right;
      scalar_read_mask |= (UINT32_C(1) << rs) | (UINT32_C(1) << rt);
    } else if (operation == UINT32_C(0x05)) {
      branch_taken = left != right;
      scalar_read_mask |= (UINT32_C(1) << rs) | (UINT32_C(1) << rt);
    } else if (operation == UINT32_C(0x06)) {
      valid = rt == 0u;
      branch_taken = rsp_signed_less32(left, 0u) || left == 0u;
      scalar_read_mask |= UINT32_C(1) << rs;
    } else if (operation == UINT32_C(0x07)) {
      valid = rt == 0u;
      branch_taken = !rsp_signed_less32(left, 0u) && left != 0u;
      scalar_read_mask |= UINT32_C(1) << rs;
    } else {
      switch (rt) {
      case UINT32_C(0):
        branch_taken = rsp_signed_less32(left, 0u);
        break;
      case UINT32_C(1):
        branch_taken = !rsp_signed_less32(left, 0u);
        break;
      case UINT32_C(16):
        branch_taken = rsp_signed_less32(left, 0u);
        write_gpr = true;
        gpr_destination = UINT32_C(31);
        gpr_value = (pc + UINT32_C(8)) & UINT32_C(0x00000ffc);
        break;
      case UINT32_C(17):
        branch_taken = !rsp_signed_less32(left, 0u);
        write_gpr = true;
        gpr_destination = UINT32_C(31);
        gpr_value = (pc + UINT32_C(8)) & UINT32_C(0x00000ffc);
        break;
      default:
        valid = false;
        break;
      }
      if ((rt == UINT32_C(16) || rt == UINT32_C(17)) &&
          rs == UINT32_C(31)) {
        valid = false;
      }
      scalar_read_mask |= UINT32_C(1) << rs;
    }
    branch_target = branch_taken
                        ? (pc + UINT32_C(4) + offset) &
                              UINT32_C(0x00000ffc)
                        : (pc + UINT32_C(8)) & UINT32_C(0x00000ffc);
    supported = valid && !was_delay_slot;
  } else if (operation == UINT32_C(0x02) ||
             operation == UINT32_C(0x03)) {
    uint32_t target = instruction & UINT32_C(0x03ffffff);

    is_branch = true;
    branch_taken = true;
    /* MIPS high address bits are above the 12-bit IMEM PC domain. */
    branch_target = (target << 2) & UINT32_C(0x00000ffc);
    if (operation == UINT32_C(0x03)) {
      write_gpr = true;
      gpr_destination = UINT32_C(31);
      gpr_value = (pc + UINT32_C(8)) & UINT32_C(0x00000ffc);
    }
    supported = !was_delay_slot;
  } else if (operation == UINT32_C(0x20) ||
             operation == UINT32_C(0x21) ||
             operation == UINT32_C(0x23) ||
             operation == UINT32_C(0x24) ||
             operation == UINT32_C(0x25)) {
    uint32_t immediate = rsp_sign_extend16((uint16_t)instruction);
    uint32_t effective_address = machine->rsp.gpr[rs] + immediate;
    bool sign_extend = false;

    if (operation == UINT32_C(0x20) || operation == UINT32_C(0x24)) {
      load_width = 1u;
      sign_extend = operation == UINT32_C(0x20);
    } else if (operation == UINT32_C(0x21) ||
               operation == UINT32_C(0x25)) {
      load_width = 2u;
      sign_extend = operation == UINT32_C(0x21);
    } else {
      load_width = 4u;
    }
    load_address = effective_address & UINT32_C(0x00000fff);
    load_value = 0u;
    for (uint32_t byte = 0u; byte < load_width; byte++) {
      uint32_t address = (load_address + byte) & UINT32_C(0x00000fff);

      load_value = (load_value << 8) | (uint32_t)machine->sp_dmem[address];
    }
    if (sign_extend && load_width == 1u &&
        (load_value & UINT32_C(0x80)) != 0u) {
      load_value |= UINT32_C(0xffffff00);
    } else if (sign_extend && load_width == 2u &&
               (load_value & UINT32_C(0x8000)) != 0u) {
      load_value |= UINT32_C(0xffff0000);
    }
    write_gpr = rt != 0u;
    gpr_destination = rt;
    gpr_value = load_value;
    scalar_read_mask |= UINT32_C(1) << rs;
    scalar_write_delay = UINT32_C(3);
    load_dmem_word = true;
    is_load_or_cop_move = true;
    is_store = true;
    supported = true;
  } else if (operation == UINT32_C(0x28) ||
             operation == UINT32_C(0x29) ||
             operation == UINT32_C(0x2b)) {
    uint32_t immediate = rsp_sign_extend16((uint16_t)instruction);
    uint32_t effective_address = machine->rsp.gpr[rs] + immediate;

    store_width = operation == UINT32_C(0x28)
                      ? 1u
                      : (operation == UINT32_C(0x29) ? 2u : 4u);
    store_address = effective_address & UINT32_C(0x00000fff);
    store_value = machine->rsp.gpr[rt];
    write_dmem_word = true;
    scalar_read_mask |= (UINT32_C(1) << rs) | (UINT32_C(1) << rt);
    is_store = true;
    is_load_or_cop_move = true;
    supported = true;
  }
  if (supported && !is_break && !is_branch && !write_vector_result &&
      !write_vmov && !was_delay_slot) {
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
      if (vector_memory_transfer && vector_memory_plan.load &&
          (pair_vector_mask & vector_memory_plan.write_register_mask) != 0u) {
        vector_register_conflict = true;
      }
      /* VADD consumes and clears VCO; do not pair it across a VCO write. */
      if (write_vu_control && vu_control_register == 0u) {
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
    if (machine->rsp_observed_refusal_reason == T64_RSP_REFUSAL_NONE) {
      machine->rsp_observed_refusal_reason =
          T64_RSP_REFUSAL_UNSUPPORTED_OPCODE;
    }
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
          machine, true,
          write_dmem_word || load_dmem_word || vector_memory_transfer,
          &dma_step);
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
          dma_conflict ? T64_RSP_STALL_BANK_CONFLICT : stall_reason;
      machine->rsp.stall_slots++;
      return T64_STATUS_OK;
    }
  }

  dma_status = t64_sp_rsp_dma_preflight(
      machine, true,
      write_dmem_word || load_dmem_word || vector_memory_transfer,
      &dma_step);
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
    machine->rsp_observed_stall_reason = T64_RSP_STALL_BANK_CONFLICT;
    machine->rsp.stall_slots++;
    return T64_STATUS_OK;
  }
  t64_sp_rsp_dma_commit(machine, &dma_step);

  if (read_cp0) {
    status = t64_machine_bus_read(
        machine, T64_SP_REGISTER_BASE + cp0_register * UINT32_C(4), 4u,
        &cp0_value);
    if (status != T64_STATUS_OK) {
      machine->rsp_observed_outcome = T64_RSP_ATTEMPT_REFUSED;
      return status;
    }
    gpr_value = cp0_value;
  }
  if (vector_memory_transfer && vector_memory_plan.load) {
    uint32_t byte;

    for (byte = 0u; byte < vector_memory_plan.memory_count; byte++) {
      uint32_t byte_value = 0u;

      status = t64_machine_bus_read(
          machine,
          T64_RSP_DMEM_BASE +
              (uint32_t)vector_memory_plan.memory_address[byte],
          1u, &byte_value);
      if (status != T64_STATUS_OK) {
        machine->rsp_observed_outcome = T64_RSP_ATTEMPT_REFUSED;
        return status;
      }
      vector_memory_plan.memory_value[byte] = (uint8_t)byte_value;
    }
  }
  if (load_dmem_word) {
    load_value = 0u;
    for (uint32_t byte = 0u; byte < load_width; byte++) {
      uint32_t address = (load_address + byte) & UINT32_C(0x00000fff);
      uint32_t byte_value = 0u;

      status = t64_machine_bus_read(machine, T64_RSP_DMEM_BASE + address, 1u,
                                    &byte_value);
      if (status != T64_STATUS_OK) {
        machine->rsp_observed_outcome = T64_RSP_ATTEMPT_REFUSED;
        return status;
      }
      load_value = (load_value << 8) | (byte_value & UINT32_C(0xff));
    }
    if (load_width == 1u && (load_value & UINT32_C(0x80)) != 0u &&
        (operation == UINT32_C(0x20))) {
      load_value |= UINT32_C(0xffffff00);
    } else if (load_width == 2u &&
               (load_value & UINT32_C(0x8000)) != 0u &&
               operation == UINT32_C(0x21)) {
      load_value |= UINT32_C(0xffff0000);
    }
    gpr_value = load_value;
  }
  if (write_cp0) {
    status = t64_machine_bus_write(
        machine, T64_SP_REGISTER_BASE + cp0_register * UINT32_C(4), 4u,
        cp0_value);
    if (status != T64_STATUS_OK) {
      machine->rsp_observed_outcome = T64_RSP_ATTEMPT_REFUSED;
      if (machine->rsp_observed_refusal_reason == T64_RSP_REFUSAL_NONE) {
        machine->rsp_observed_refusal_reason =
            T64_RSP_REFUSAL_UNSUPPORTED_OPCODE;
      }
      return status == T64_STATUS_BUS_DEVICE_REFUSED ? T64_STATUS_OK : status;
    }
  }
  if (write_vu_control) {
    if (vu_control_register == 0u) {
      machine->rsp.vco = (uint16_t)vu_control_value;
    } else if (vu_control_register == 1u) {
      machine->rsp.vcc = (uint16_t)vu_control_value;
    } else {
      machine->rsp.vce = (uint8_t)vu_control_value;
    }
  }

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
  if (write_vmov) {
    rsp_write_vector_element(machine, vmov_destination,
                             vmov_destination_lane, vmov_value);
    machine->rsp.vector_ready_tick[vmov_destination] =
        rsp_ready_tick_after(machine->tick, vector_write_delay);
    machine->rsp.accumulator[vmov_destination_lane] =
        (machine->rsp.accumulator[vmov_destination_lane] &
         ~UINT64_C(0xffff)) |
        (uint64_t)vmov_value;
    machine->rsp.accumulator_ready_tick[vmov_destination_lane] =
        rsp_ready_tick_after(machine->tick, vector_write_delay);
  }
  if (vector_memory_transfer && vector_memory_plan.load) {
    uint32_t index;

    for (index = 0u; index < vector_memory_plan.write_count; index++) {
      uint32_t reg = vector_memory_plan.write_register[index];
      uint32_t byte = vector_memory_plan.write_byte[index];

      machine->rsp.vector[reg][byte] =
          rsp_vector_memory_loaded_byte(&vector_memory_plan, index);
    }
    for (index = 0u; index < 32u; index++) {
      if ((vector_memory_plan.write_register_mask &
           (UINT32_C(1) << index)) != 0u) {
        machine->rsp.vector_ready_tick[index] =
            rsp_ready_tick_after(machine->tick, vector_write_delay);
      }
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
    for (uint32_t byte = 0u; byte < store_width; byte++) {
      uint32_t address = (store_address + byte) & UINT32_C(0x00000fff);
      uint32_t shift = (store_width - byte - 1u) * 8u;
      uint32_t byte_value = (store_value >> shift) & UINT32_C(0xff);

      status = t64_machine_bus_write(machine, T64_RSP_DMEM_BASE + address, 1u,
                                     byte_value);
      if (status != T64_STATUS_OK) {
        machine->rsp_observed_outcome = T64_RSP_ATTEMPT_REFUSED;
        return status;
      }
    }
  }
  if (vector_memory_transfer && !vector_memory_plan.load) {
    uint32_t byte;

    for (byte = 0u; byte < vector_memory_plan.memory_count; byte++) {
      status = t64_machine_bus_write(
          machine,
          T64_RSP_DMEM_BASE +
              (uint32_t)vector_memory_plan.memory_address[byte],
          1u, vector_memory_plan.memory_value[byte]);
      if (status != T64_STATUS_OK) {
        machine->rsp_observed_outcome = T64_RSP_ATTEMPT_REFUSED;
        return status;
      }
    }
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
