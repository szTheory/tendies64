#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define T64_DP_BASE UINT32_C(0x04100000)
#define T64_DP_START (T64_DP_BASE + UINT32_C(0x00))
#define T64_DP_END (T64_DP_BASE + UINT32_C(0x04))
#define T64_DP_CURRENT (T64_DP_BASE + UINT32_C(0x08))
#define T64_DP_STATUS (T64_DP_BASE + UINT32_C(0x0c))

#define RDP_OP_FILL_RECT UINT32_C(0x36)
#define RDP_OP_SET_FILL_COLOR UINT32_C(0x37)
#define RDP_OP_SET_OTHER_MODES UINT32_C(0x2f)
#define RDP_OP_SET_SCISSOR UINT32_C(0x2d)
#define RDP_OP_SET_COLOR_IMAGE UINT32_C(0x3f)
#define RDP_OP_SYNC_FULL UINT32_C(0x29)

static uint64_t read_be64(const uint8_t *bytes) {
  uint64_t value = 0u;
  uint32_t i;

  for (i = 0u; i < 8u; i++) value = (value << 8) | (uint64_t)bytes[i];
  return value;
}

static void write_be16(uint8_t *bytes, uint16_t value) {
  bytes[0] = (uint8_t)(value >> 8);
  bytes[1] = (uint8_t)value;
}

static void set_refusal(t64_machine *machine, uint32_t reason) {
  machine->rdp.refusal_reason = reason;
  machine->rdp.dpc_status |= T64_RDP_STATUS_ERROR;
  machine->rdp.dpc_status &= ~T64_RDP_STATUS_BUSY;
  machine->rdp.busy = false;
  machine->rdp.render_active = false;
  machine->rdp.fence_pending = false;
}

static void finish_if_idle(t64_rdp_state *rdp, uint32_t next) {
  if (next >= rdp->dpc_end && !rdp->render_active && !rdp->fence_pending) {
    rdp->busy = false;
    rdp->dpc_status &= ~T64_RDP_STATUS_BUSY;
  }
}

static t64_status schedule_next_committed(t64_machine *machine,
                                          uint32_t next) {
  t64_event_reservation reservation;
  t64_status status;

  if (next >= machine->rdp.dpc_end) return T64_STATUS_OK;
  status = t64_schedule_reserve_after(
      machine, UINT64_C(1), T64_RDP_DEVICE, T64_RDP_EVENT_WORK, 0u, 0u,
      t64_rdp_event, &reservation);
  if (status != T64_STATUS_OK) return status;
  return t64_schedule_commit(machine, &reservation);
}

static t64_status schedule_work_after_committed(t64_machine *machine) {
  t64_event_reservation reservation;
  t64_status status = t64_schedule_reserve_after(
      machine, UINT64_C(1), T64_RDP_DEVICE, T64_RDP_EVENT_WORK, 0u, 0u,
      t64_rdp_event, &reservation);

  if (status != T64_STATUS_OK) return status;
  return t64_schedule_commit(machine, &reservation);
}

static bool command_span_valid(const t64_machine *machine, uint32_t address,
                               uint32_t *reason_out) {
  uint64_t end = (uint64_t)address + UINT64_C(8);

  if ((address & UINT32_C(7)) != 0u || end > machine->profile.rdram_bytes) {
    *reason_out = T64_RDP_REFUSAL_SPAN;
    return false;
  }
  if (end > machine->rdp.dpc_end) {
    *reason_out = T64_RDP_REFUSAL_TRUNCATED_WORD;
    return false;
  }
  return true;
}

static bool fill_bounds(const t64_rdp_state *rdp, uint64_t word,
                        uint32_t *x0_out, uint32_t *y0_out,
                        uint32_t *x1_out, uint32_t *y1_out) {
  uint32_t xl = (uint32_t)((word >> 44) & UINT64_C(0xfff)) >> 2;
  uint32_t yl = (uint32_t)((word >> 32) & UINT64_C(0xfff)) >> 2;
  uint32_t xh = (uint32_t)((word >> 12) & UINT64_C(0xfff)) >> 2;
  uint32_t yh = (uint32_t)(word & UINT64_C(0xfff)) >> 2;
  uint32_t sx0 = rdp->scissor_xh >> 2;
  uint32_t sy0 = rdp->scissor_yh >> 2;
  uint32_t sx1 = rdp->scissor_xl >> 2;
  uint32_t sy1 = rdp->scissor_yl >> 2;

  if (xh > xl || yh > yl || rdp->color_image_width == 0u) return false;
  if (sx0 > xh) xh = sx0;
  if (sy0 > yh) yh = sy0;
  if (sx1 < xl) xl = sx1;
  if (sy1 < yl) yl = sy1;
  if (xh >= rdp->color_image_width) {
    xl = 0u;
    xh = 1u;
  } else if (xl >= rdp->color_image_width) {
    xl = rdp->color_image_width - 1u;
  }
  *x0_out = xh;
  *x1_out = xl;
  *y0_out = yh;
  *y1_out = yl;
  return true;
}

static bool validate_fill_store(const t64_machine *machine, uint32_t x0,
                                uint32_t y0, uint32_t x1, uint32_t y1) {
  const t64_rdp_state *rdp = &machine->rdp;
  uint64_t last_pixel;
  uint64_t end;

  if (y0 > y1 || x0 > x1) return true;
  if (!rdp->color_image_valid || rdp->color_image_width == 0u ||
      rdp->color_image_size != 2u || rdp->color_image_format != 0u ||
      (rdp->color_image_address & UINT32_C(1)) != 0u) {
    return false;
  }
  last_pixel = (uint64_t)y1 * rdp->color_image_width + x1;
  end = (uint64_t)rdp->color_image_address + (last_pixel + 1u) * 2u;
  return end <= machine->profile.rdram_bytes;
}

static void fill_one_row(t64_machine *machine) {
  t64_rdp_state *rdp = &machine->rdp;
  uint32_t x;

  for (x = rdp->render_x_begin; x <= rdp->render_x_end; x++) {
    uint64_t pixel = (uint64_t)rdp->render_y * rdp->color_image_width + x;
    uint64_t address = (uint64_t)rdp->color_image_address + pixel * 2u;
    uint16_t value = (x & 1u) == 0u
                         ? (uint16_t)(rdp->fill_color >> 16)
                         : (uint16_t)rdp->fill_color;

    write_be16(machine->ram + (size_t)address, value);
  }
}

static t64_status handle_row(t64_machine *machine) {
  t64_rdp_state *rdp = &machine->rdp;
  bool last_row = rdp->render_y >= rdp->render_y_end;
  uint32_t next_command = rdp->dpc_current;
  t64_status status = schedule_next_committed(machine, next_command);

  if (status != T64_STATUS_OK) return status;
  fill_one_row(machine);
  if (last_row) {
    rdp->render_active = false;
    rdp->fill_count++;
    finish_if_idle(rdp, next_command);
  } else {
    rdp->render_y++;
  }
  return T64_STATUS_OK;
}

static void record_command(t64_machine *machine, uint64_t word,
                           uint8_t command_byte, uint32_t opcode) {
  t64_rdp_state *rdp = &machine->rdp;

  rdp->last_original_word = word;
  rdp->last_command_byte = command_byte;
  rdp->last_opcode_field = opcode;
  t64_trace_emit(machine, machine->tick, T64_TRACE_DEVICE_RDP,
                 T64_TRACE_KIND_RDP_COMMAND, word);
}

void t64_rdp_reset_machine_state(t64_machine *machine) {
  if (machine == NULL) return;
  (void)memset(&machine->rdp, 0, sizeof machine->rdp);
  machine->rdp.random_state = UINT32_C(1);
  machine->rdp.work_policy_version = UINT32_C(1);
}

t64_status t64_rdp_read_register(const t64_machine *machine,
                                 uint32_t address, uint32_t *value_out) {
  uint32_t value;

  if (machine == NULL || value_out == NULL || !t64_has_rdp_vi_profile(machine))
    return T64_STATUS_BUS_UNSUPPORTED;
  switch (address) {
  case T64_DP_START: value = machine->rdp.dpc_start; break;
  case T64_DP_END: value = machine->rdp.dpc_end; break;
  case T64_DP_CURRENT: value = machine->rdp.dpc_current; break;
  case T64_DP_STATUS: value = machine->rdp.dpc_status; break;
  default: return T64_STATUS_BUS_UNSUPPORTED;
  }
  *value_out = value;
  return T64_STATUS_OK;
}

t64_status t64_rdp_write_register(t64_machine *machine, uint32_t address,
                                  uint32_t value) {
  t64_rdp_state *rdp;
  t64_status status;

  if (machine == NULL || !t64_has_rdp_vi_profile(machine))
    return T64_STATUS_BUS_UNSUPPORTED;
  if ((address & UINT32_C(3)) != 0u)
    return T64_STATUS_BUS_ALIGNMENT;
  if (address < T64_DP_BASE || address >= T64_DP_BASE + UINT32_C(0x10))
    return T64_STATUS_BUS_UNSUPPORTED;
  rdp = &machine->rdp;
  switch (address) {
  case T64_DP_START:
    if (rdp->busy || (value & UINT32_C(7)) != 0u ||
        (uint64_t)value >= machine->profile.rdram_bytes)
      return T64_STATUS_BUS_DEVICE_REFUSED;
    rdp->dpc_start = value;
    rdp->dpc_current = value;
    return T64_STATUS_OK;
  case T64_DP_END:
    if (rdp->busy || (value & UINT32_C(7)) != 0u || value < rdp->dpc_current ||
        (uint64_t)value > machine->profile.rdram_bytes)
      return T64_STATUS_BUS_DEVICE_REFUSED;
    if (value == rdp->dpc_current) {
      rdp->dpc_end = value;
      return T64_STATUS_OK;
    }
    {
      t64_event_reservation reservation;
      status = t64_schedule_reserve_after(
          machine, UINT64_C(1), T64_RDP_DEVICE, T64_RDP_EVENT_WORK, 0u, 0u,
          t64_rdp_event, &reservation);
      if (status != T64_STATUS_OK) return status;
      status = t64_schedule_commit(machine, &reservation);
      if (status != T64_STATUS_OK) return status;
    }
    rdp->dpc_end = value;
    rdp->busy = true;
    rdp->dpc_status |= T64_RDP_STATUS_BUSY;
    rdp->refusal_reason = T64_RDP_REFUSAL_NONE;
    return T64_STATUS_OK;
  case T64_DP_STATUS:
    if ((value & UINT32_C(1)) != 0u) {
      rdp->dpc_status &= ~T64_RDP_STATUS_ERROR;
      rdp->refusal_reason = T64_RDP_REFUSAL_NONE;
    }
    return T64_STATUS_OK;
  default:
    return T64_STATUS_BUS_UNSUPPORTED;
  }
}

t64_status t64_rdp_event(t64_machine *machine, const t64_event *event) {
  t64_rdp_state *rdp;
  uint32_t address;
  uint32_t next;
  uint32_t reason = T64_RDP_REFUSAL_NONE;
  uint32_t opcode;
  uint32_t x0, x1, y0, y1;
  uint64_t word;
  uint8_t command_byte;
  t64_status status;

  if (machine == NULL || event == NULL || event->device != T64_RDP_DEVICE ||
      event->kind != T64_RDP_EVENT_WORK || !t64_has_rdp_vi_profile(machine))
    return T64_STATUS_INVALID_ARGUMENT;
  rdp = &machine->rdp;
  if (rdp->render_active) return handle_row(machine);
  if (rdp->fence_pending) {
    rdp->fence_pending = false;
    rdp->fullsync_count++;
    rdp->busy = false;
    rdp->dpc_status &= ~T64_RDP_STATUS_BUSY;
    t64_mi_set_pending(machine, UINT32_C(0x20));
    t64_trace_emit(machine, machine->tick, T64_TRACE_DEVICE_RDP,
                   T64_TRACE_KIND_RDP_COMPLETE, rdp->fullsync_count);
    return T64_STATUS_OK;
  }
  address = rdp->dpc_current;
  if (!command_span_valid(machine, address, &reason)) {
    set_refusal(machine, reason);
    return T64_STATUS_OK;
  }
  word = read_be64(machine->ram + address);
  command_byte = (uint8_t)(word >> 56);
  opcode = (uint32_t)(command_byte & UINT8_C(0x3f));
  next = address + UINT32_C(8);

  if (opcode == RDP_OP_SET_COLOR_IMAGE) {
    uint32_t format = (uint32_t)((word >> 53) & UINT64_C(7));
    uint32_t size = (uint32_t)((word >> 51) & UINT64_C(3));
    uint32_t width = (uint32_t)((word >> 32) & UINT64_C(0x3ff)) + 1u;
    uint32_t base = (uint32_t)(word & UINT64_C(0x03ffffff));
    if (format != 0u || size != 2u || width > 1024u ||
        (base & UINT32_C(1)) != 0u) {
      record_command(machine, word, command_byte, opcode);
      set_refusal(machine, T64_RDP_REFUSAL_INVALID_IMAGE);
      return T64_STATUS_OK;
    }
    status = schedule_next_committed(machine, next);
    if (status != T64_STATUS_OK) return status;
    rdp->color_image_format = format;
    rdp->color_image_size = size;
    rdp->color_image_width = width;
    rdp->color_image_address = base;
    rdp->color_image_valid = true;
  } else if (opcode == RDP_OP_SET_SCISSOR) {
    status = schedule_next_committed(machine, next);
    if (status != T64_STATUS_OK) return status;
    rdp->scissor_xh = (uint32_t)((word >> 44) & UINT64_C(0xfff));
    rdp->scissor_yh = (uint32_t)((word >> 32) & UINT64_C(0xfff));
    rdp->scissor_xl = (uint32_t)((word >> 12) & UINT64_C(0xfff));
    rdp->scissor_yl = (uint32_t)(word & UINT64_C(0xfff));
    rdp->scissor_valid = true;
  } else if (opcode == RDP_OP_SET_OTHER_MODES) {
    status = schedule_next_committed(machine, next);
    if (status != T64_STATUS_OK) return status;
    rdp->other_modes = word;
    rdp->other_modes_valid = true;
  } else if (opcode == RDP_OP_SET_FILL_COLOR) {
    status = schedule_next_committed(machine, next);
    if (status != T64_STATUS_OK) return status;
    rdp->fill_color = (uint32_t)word;
    rdp->fill_color_valid = true;
  } else if (opcode == RDP_OP_FILL_RECT) {
    if (!rdp->color_image_valid || !rdp->scissor_valid ||
        !rdp->other_modes_valid || !rdp->fill_color_valid ||
        ((rdp->other_modes >> 52) & UINT64_C(3)) != UINT64_C(3) ||
        !fill_bounds(rdp, word, &x0, &y0, &x1, &y1) ||
        !validate_fill_store(machine, x0, y0, x1, y1)) {
      record_command(machine, word, command_byte, opcode);
      set_refusal(machine, T64_RDP_REFUSAL_RENDER_RANGE);
      return T64_STATUS_OK;
    }
    if (x0 <= x1 && y0 <= y1) {
      status = schedule_work_after_committed(machine);
      if (status != T64_STATUS_OK) return status;
      rdp->render_active = true;
      rdp->render_x_begin = x0;
      rdp->render_x_end = x1;
      rdp->render_y_begin = y0;
      rdp->render_y_end = y1;
      rdp->render_y = y0;
    } else {
      status = schedule_next_committed(machine, next);
      if (status != T64_STATUS_OK) return status;
    }
  } else if (opcode == RDP_OP_SYNC_FULL) {
    {
      t64_event_reservation reservation;
      status = t64_schedule_reserve_after(
          machine, UINT64_C(1), T64_RDP_DEVICE, T64_RDP_EVENT_WORK, 0u, 0u,
          t64_rdp_event, &reservation);
      if (status != T64_STATUS_OK) return status;
      status = t64_schedule_commit(machine, &reservation);
      if (status != T64_STATUS_OK) return status;
    }
    rdp->fence_pending = true;
  } else {
    record_command(machine, word, command_byte, opcode);
    set_refusal(machine, T64_RDP_REFUSAL_UNSUPPORTED_OPCODE);
    return T64_STATUS_OK;
  }

  record_command(machine, word, command_byte, opcode);
  rdp->dpc_current = next;
  rdp->command_count++;
  finish_if_idle(rdp, next);
  return T64_STATUS_OK;
}
