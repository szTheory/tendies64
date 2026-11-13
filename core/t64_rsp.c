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
    t64_cpu_link_invalidate_range(machine,
                                  T64_RSP_DMEM_BASE + store_address,
                                  UINT32_C(4));
  }
  if (is_break) {
    machine->rsp.sp_status |= T64_RSP_STATUS_HALT | T64_RSP_STATUS_BROKE;
    if ((machine->rsp.sp_status & T64_RSP_STATUS_INTR_BREAK) != 0u) {
      t64_mi_set_pending(machine, UINT32_C(1));
    }
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
