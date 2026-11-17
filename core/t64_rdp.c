#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define T64_DP_BASE UINT32_C(0x04100000)
#define T64_DP_START (T64_DP_BASE + UINT32_C(0x00))
#define T64_DP_END (T64_DP_BASE + UINT32_C(0x04))
#define T64_DP_CURRENT (T64_DP_BASE + UINT32_C(0x08))
#define T64_DP_STATUS (T64_DP_BASE + UINT32_C(0x0c))
#define T64_DP_CLOCK (T64_DP_BASE + UINT32_C(0x10))
#define T64_DP_BUFFER_BUSY (T64_DP_BASE + UINT32_C(0x14))
#define T64_DP_PIPE_BUSY (T64_DP_BASE + UINT32_C(0x18))
#define T64_DP_TMEM (T64_DP_BASE + UINT32_C(0x1c))
#define T64_SP_DMEM_BYTES UINT32_C(4096)
#define T64_DPC_COUNTER_MASK UINT32_C(0x00ffffff)

#define T64_DPC_CLR_XBUS UINT32_C(0x0001)
#define T64_DPC_SET_XBUS UINT32_C(0x0002)
#define T64_DPC_CLR_FREEZE UINT32_C(0x0004)
#define T64_DPC_SET_FREEZE UINT32_C(0x0008)
#define T64_DPC_CLR_FLUSH UINT32_C(0x0010)
#define T64_DPC_SET_FLUSH UINT32_C(0x0020)
#define T64_DPC_CLR_TMEM UINT32_C(0x0040)
#define T64_DPC_CLR_PIPE UINT32_C(0x0080)
#define T64_DPC_CLR_CMD UINT32_C(0x0100)
#define T64_DPC_CLR_CLOCK UINT32_C(0x0200)

#define RDP_OP_FILL_RECT UINT32_C(0x36)
#define RDP_OP_SET_FILL_COLOR UINT32_C(0x37)
#define RDP_OP_SET_PRIM_COLOR UINT32_C(0x3a)
#define RDP_OP_SET_ENV_COLOR UINT32_C(0x3b)
#define RDP_OP_SET_COMBINE UINT32_C(0x3c)
#define RDP_OP_SET_FOG_COLOR UINT32_C(0x38)
#define RDP_OP_SET_BLEND_COLOR UINT32_C(0x39)
#define RDP_OP_SET_KEY_GB UINT32_C(0x2a)
#define RDP_OP_SET_KEY_R UINT32_C(0x2b)
#define RDP_OP_SET_OTHER_MODES UINT32_C(0x2f)
#define RDP_OP_SET_SCISSOR UINT32_C(0x2d)
#define RDP_OP_LOAD_TLUT UINT32_C(0x30)
#define RDP_OP_SET_TILE_SIZE UINT32_C(0x32)
#define RDP_OP_LOAD_BLOCK UINT32_C(0x33)
#define RDP_OP_LOAD_TILE UINT32_C(0x34)
#define RDP_OP_SET_TILE UINT32_C(0x35)
#define RDP_OP_SET_COLOR_IMAGE UINT32_C(0x3f)
#define RDP_OP_TEX_RECT UINT32_C(0x24)
#define RDP_OP_TEX_RECT_FLIP UINT32_C(0x25)
#define RDP_OP_SET_CONVERT UINT32_C(0x2c)
#define RDP_OP_SET_TEXTURE_IMAGE UINT32_C(0x3d)
#define RDP_OP_SYNC_LOAD UINT32_C(0x26)
#define RDP_OP_SYNC_PIPE UINT32_C(0x27)
#define RDP_OP_SYNC_TILE UINT32_C(0x28)
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
  machine->rdp.dpc_start_valid = false;
  machine->rdp.dpc_end_valid = false;
  machine->rdp.render_active = false;
  machine->rdp.fence_pending = false;
  machine->rdp.command_pending = false;
  machine->rdp.texrect_pending = false;
  machine->rdp.render_texture_flip = false;
  machine->rdp.pending_command_address = 0u;
  machine->rdp.pending_command_byte = 0u;
  machine->rdp.pending_opcode_field = 0u;
  machine->rdp.pending_command_word = 0u;
  machine->rdp.pending_texrect_word = 0u;
  machine->rdp.pending_triangle_word_count = 0u;
  machine->rdp.render_triangle = false;
  machine->rdp.shade_triangle = false;
}

static void finish_if_idle(t64_rdp_state *rdp, uint32_t next) {
  if (next >= rdp->dpc_end && !rdp->render_active && !rdp->fence_pending &&
      !rdp->command_pending && !rdp->texrect_pending) {
    rdp->busy = false;
    rdp->dpc_start_valid = false;
    rdp->dpc_end_valid = false;
    rdp->dpc_status &= ~T64_RDP_STATUS_BUSY;
  }
}

static bool rdp_event_queued(const t64_machine *machine) {
  size_t i;

  for (i = 0u; i < machine->queue_count; i++) {
    size_t index = (machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY;
    const t64_event *event = &machine->events[index];
    if (event->device == T64_RDP_DEVICE &&
        event->kind == T64_RDP_EVENT_WORK) {
      return true;
    }
  }
  return false;
}

static uint32_t dpc_status_value(const t64_rdp_state *rdp) {
  uint32_t value = T64_DPC_STATUS_CBUF_READY;

  if (rdp->dpc_xbus) value |= T64_DPC_STATUS_XBUS;
  if (rdp->dpc_freeze) value |= T64_DPC_STATUS_FREEZE;
  if (rdp->dpc_flush) value |= T64_DPC_STATUS_FLUSH;
  if (rdp->busy && !rdp->dpc_freeze && !rdp->dpc_flush)
    value |= T64_DPC_STATUS_START_GCLK;
  if (rdp->render_active) value |= T64_DPC_STATUS_PIPE_BUSY;
  if (rdp->busy && (rdp->command_pending ||
                    rdp->dpc_current < rdp->dpc_end) &&
      !rdp->dpc_freeze && !rdp->dpc_flush) {
    value |= T64_DPC_STATUS_CMD_BUSY;
  }
  if (rdp->busy && rdp->dpc_current < rdp->dpc_end && !rdp->dpc_freeze &&
      !rdp->dpc_flush)
    value |= T64_DPC_STATUS_DMA_BUSY;
  if (rdp->dpc_end_valid) value |= T64_DPC_STATUS_END_VALID;
  if (rdp->dpc_start_valid) value |= T64_DPC_STATUS_START_VALID;
  return value;
}

static void increment_counter(uint32_t *counter) {
  *counter = (*counter + UINT32_C(1)) & T64_DPC_COUNTER_MASK;
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

  if ((address & UINT32_C(7)) != 0u ||
      (machine->rdp.dpc_xbus ? end > T64_SP_DMEM_BYTES
                             : end > machine->profile.rdram_bytes)) {
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

static bool texture_rect_bounds(const t64_rdp_state *rdp, uint64_t word,
                                uint32_t *x0_out, uint32_t *y0_out,
                                uint32_t *x1_out, uint32_t *y1_out) {
  uint32_t xh_raw = (uint32_t)((word >> 44) & UINT64_C(0xfff));
  uint32_t yh_raw = (uint32_t)((word >> 32) & UINT64_C(0xfff));
  uint32_t xl_raw = (uint32_t)((word >> 12) & UINT64_C(0xfff));
  uint32_t yl_raw = (uint32_t)(word & UINT64_C(0xfff));
  uint32_t xh = xh_raw >> 2;
  uint32_t yh = yh_raw >> 2;
  uint32_t xl = xl_raw >> 2;
  uint32_t yl = yl_raw >> 2;
  uint32_t sx0 = rdp->scissor_xh >> 2;
  uint32_t sy0 = rdp->scissor_yh >> 2;
  uint32_t sx1 = rdp->scissor_xl == 0u
                     ? 0u
                     : (rdp->scissor_xl - 1u) >> 2;
  uint32_t sy1 = rdp->scissor_yl == 0u
                     ? 0u
                     : (rdp->scissor_yl - 1u) >> 2;
  if (xl_raw < xh_raw || yl_raw < yh_raw ||
      rdp->color_image_width == 0u) return false;
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
  t64_status status = last_row
                          ? schedule_next_committed(machine, next_command)
                          : schedule_work_after_committed(machine);

  if (status != T64_STATUS_OK) return status;
  increment_counter(&rdp->dpc_pipe_busy_counter);
  if (rdp->render_triangle) {
    t64_rdp_triangle_render_row(machine);
  } else if (rdp->render_kind == 2u) {
    uint32_t x;
    for (x = rdp->render_x_begin; x <= rdp->render_x_end; x++) {
      uint64_t pixel = (uint64_t)rdp->render_y * rdp->color_image_width + x;
      uint64_t address = (uint64_t)rdp->color_image_address + pixel * 2u;
      uint16_t color = 0u;
      (void)t64_rdp_tmem_copy_pixel(machine, x, rdp->render_y, &color);
      write_be16(machine->ram + (size_t)address, color);
    }
  } else {
    fill_one_row(machine);
  }
  if (last_row) {
    rdp->render_active = false;
    if (rdp->render_triangle) {
      rdp->render_triangle = false;
      rdp->shade_triangle = false;
      rdp->pending_triangle_word_count = 0u;
      rdp->triangle_count++;
    } else if (rdp->render_kind == 2u)
      rdp->copy_count++;
    else
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

static bool supported_opcode(const t64_machine *machine, uint32_t opcode) {
  switch (opcode) {
  case RDP_OP_FILL_RECT:
  case RDP_OP_SET_FILL_COLOR:
  case RDP_OP_SET_OTHER_MODES:
  case RDP_OP_SET_SCISSOR:
  case RDP_OP_SET_COLOR_IMAGE:
  case RDP_OP_SET_TEXTURE_IMAGE:
  case RDP_OP_SET_TILE:
  case RDP_OP_SET_TILE_SIZE:
  case RDP_OP_LOAD_TILE:
  case RDP_OP_LOAD_BLOCK:
  case RDP_OP_LOAD_TLUT:
  case RDP_OP_SET_CONVERT:
  case RDP_OP_TEX_RECT:
  case RDP_OP_TEX_RECT_FLIP:
  case RDP_OP_SYNC_LOAD:
  case RDP_OP_SYNC_PIPE:
  case RDP_OP_SYNC_TILE:
  case RDP_OP_SYNC_FULL:
    return true;
  default:
    return (t64_has_rdp_triangle_profile(machine) &&
            (t64_rdp_triangle_word_count(opcode) != 0u ||
             opcode == RDP_OP_SET_PRIM_COLOR)) ||
           (t64_has_rdp_combiner_profile(machine) &&
            (opcode == RDP_OP_SET_ENV_COLOR ||
             opcode == RDP_OP_SET_COMBINE ||
             opcode == RDP_OP_SET_FOG_COLOR ||
             opcode == RDP_OP_SET_BLEND_COLOR ||
             opcode == RDP_OP_SET_KEY_GB ||
             opcode == RDP_OP_SET_KEY_R));
  }
}

static t64_status apply_pending_command(t64_machine *machine) {
  t64_rdp_state *rdp = &machine->rdp;
  uint32_t address = rdp->pending_command_address;
  uint32_t next = rdp->dpc_current;
  uint32_t opcode = rdp->pending_opcode_field;
  uint32_t x0 = 0u, x1 = 0u, y0 = 0u, y1 = 0u;
  uint32_t format = 0u, size = 0u, width = 0u, base = 0u;
  uint64_t word = rdp->pending_command_word;
  uint8_t command_byte = (uint8_t)rdp->pending_command_byte;
  bool valid_fill = true;
  bool draw_fill = false;
  bool draw_triangle = false;
  bool texture_command;
  uint32_t triangle_y_begin = 1u, triangle_y_end = 0u;
  size_t triangle_words;
  uint32_t reason = T64_RDP_REFUSAL_INVALID_TEXTURE;
  t64_status status;

  texture_command = t64_rdp_tmem_handles_opcode(opcode);
  triangle_words = t64_rdp_triangle_word_count(opcode);

  if (triangle_words != 0u) {
    bool texture_triangle = opcode == UINT32_C(0x0a) ||
                            opcode == UINT32_C(0x0e);
    if (!t64_has_rdp_triangle_profile(machine) ||
        (opcode != UINT32_C(0x08) && opcode != UINT32_C(0x0c) &&
         (!texture_triangle ||
          !t64_has_rdp_texture_profile(machine)))) {
      rdp->dpc_current = address;
      record_command(machine, word, command_byte, opcode);
      set_refusal(machine, T64_RDP_REFUSAL_UNSUPPORTED_OPCODE);
      return T64_STATUS_OK;
    }
    if (rdp->pending_triangle_word_count != triangle_words ||
        !t64_rdp_triangle_bounds(machine, opcode,
                                 rdp->pending_triangle_words,
                                 rdp->pending_triangle_word_count,
                                 &triangle_y_begin, &triangle_y_end)) {
      rdp->dpc_current = address;
      record_command(machine, word, command_byte, opcode);
      set_refusal(machine, T64_RDP_REFUSAL_RENDER_RANGE);
      return T64_STATUS_OK;
    }
    draw_triangle = triangle_y_begin <= triangle_y_end;
  }

  if (opcode == RDP_OP_SET_COLOR_IMAGE) {
    format = (uint32_t)((word >> 53) & UINT64_C(7));
    size = (uint32_t)((word >> 51) & UINT64_C(3));
    width = (uint32_t)((word >> 32) & UINT64_C(0x3ff)) + 1u;
    base = (uint32_t)(word & UINT64_C(0x03ffffff));
    valid_fill = format == 0u && size == 2u && width <= 1024u &&
                 (base & UINT32_C(1)) == 0u;
    if (!valid_fill) {
      rdp->dpc_current = address;
      record_command(machine, word, command_byte, opcode);
      set_refusal(machine, T64_RDP_REFUSAL_INVALID_IMAGE);
      return T64_STATUS_OK;
    }
  } else if (opcode == RDP_OP_FILL_RECT) {
    valid_fill = rdp->color_image_valid && rdp->scissor_valid &&
                 rdp->other_modes_valid && rdp->fill_color_valid &&
                 ((rdp->other_modes >> 52) & UINT64_C(3)) == UINT64_C(3) &&
                 fill_bounds(rdp, word, &x0, &y0, &x1, &y1) &&
                 validate_fill_store(machine, x0, y0, x1, y1);
    if (!valid_fill) {
      rdp->dpc_current = address;
      record_command(machine, word, command_byte, opcode);
      set_refusal(machine, T64_RDP_REFUSAL_RENDER_RANGE);
      return T64_STATUS_OK;
    }
    draw_fill = x0 <= x1 && y0 <= y1;
  }

  if (texture_command &&
      !t64_rdp_tmem_validate_command(machine, opcode, word, &reason)) {
    rdp->dpc_current = address;
    record_command(machine, word, command_byte, opcode);
    set_refusal(machine, reason);
    return T64_STATUS_OK;
  }

  if ((opcode == RDP_OP_SYNC_FULL) || draw_fill ||
      (triangle_words != 0u && draw_triangle)) {
    status = schedule_work_after_committed(machine);
  } else {
    status = schedule_next_committed(machine, next);
  }
  if (status != T64_STATUS_OK) return status;

  increment_counter(&rdp->dpc_clock_counter);
  if (opcode == RDP_OP_TEX_RECT || opcode == RDP_OP_TEX_RECT_FLIP) {
    rdp->pending_texrect_word = word;
    rdp->texrect_pending = true;
    rdp->command_pending = false;
    rdp->pending_command_byte = 0u;
    rdp->pending_opcode_field = 0u;
    rdp->pending_command_word = 0u;
    record_command(machine, word, command_byte, opcode);
    return T64_STATUS_OK;
  }
  if (opcode == RDP_OP_SET_COLOR_IMAGE) {
    rdp->color_image_format = format;
    rdp->color_image_size = size;
    rdp->color_image_width = width;
    rdp->color_image_address = base;
    rdp->color_image_valid = true;
  } else if (opcode == RDP_OP_SET_SCISSOR) {
    rdp->scissor_xh = (uint32_t)((word >> 44) & UINT64_C(0xfff));
    rdp->scissor_yh = (uint32_t)((word >> 32) & UINT64_C(0xfff));
    rdp->scissor_xl = (uint32_t)((word >> 12) & UINT64_C(0xfff));
    rdp->scissor_yl = (uint32_t)(word & UINT64_C(0xfff));
    rdp->scissor_valid = true;
  } else if (opcode == RDP_OP_SET_OTHER_MODES) {
    uint32_t tlut_enabled = (uint32_t)((word >> 47) & UINT64_C(1));
    uint32_t tlut_type = (uint32_t)((word >> 46) & UINT64_C(1));
    rdp->other_modes = word;
    rdp->tlut_format = tlut_enabled != 0u
                           ? (tlut_type != 0u ? 3u : 2u)
                           : 0u;
    rdp->other_modes_valid = true;
  } else if (opcode == RDP_OP_SET_FILL_COLOR) {
    rdp->fill_color = (uint32_t)word;
    rdp->fill_color_valid = true;
  } else if (opcode == RDP_OP_SET_PRIM_COLOR &&
             t64_has_rdp_triangle_profile(machine)) {
    rdp->primitive_color = (uint32_t)word;
    rdp->primitive_lod_fraction =
        (uint32_t)((word >> 40) & UINT64_C(0xff));
    rdp->primitive_color_valid = true;
  } else if (opcode == RDP_OP_SET_ENV_COLOR &&
             t64_has_rdp_combiner_profile(machine)) {
    rdp->environment_color = (uint32_t)word;
    rdp->environment_color_valid = true;
  } else if (opcode == RDP_OP_SET_COMBINE &&
             t64_has_rdp_combiner_profile(machine)) {
    rdp->combine_mode = word;
    rdp->combine_mode_valid = true;
  } else if (opcode == RDP_OP_SET_FOG_COLOR &&
             t64_has_rdp_combiner_profile(machine)) {
    rdp->fog_color = (uint32_t)word;
    rdp->fog_color_valid = true;
  } else if (opcode == RDP_OP_SET_BLEND_COLOR &&
             t64_has_rdp_combiner_profile(machine)) {
    rdp->blend_color = (uint32_t)word;
    rdp->blend_color_valid = true;
  } else if (opcode == RDP_OP_SET_KEY_GB &&
             t64_has_rdp_combiner_profile(machine)) {
    rdp->key_width[1] = (uint32_t)((word >> 44) & UINT64_C(0xfff));
    rdp->key_width[2] = (uint32_t)((word >> 32) & UINT64_C(0xfff));
    rdp->key_center[1] = (uint32_t)((word >> 24) & UINT64_C(0xff));
    rdp->key_scale[1] = (uint32_t)((word >> 16) & UINT64_C(0xff));
    rdp->key_center[2] = (uint32_t)((word >> 8) & UINT64_C(0xff));
    rdp->key_scale[2] = (uint32_t)(word & UINT64_C(0xff));
    rdp->key_center_valid = true;
    rdp->key_scale_valid = true;
    rdp->key_width_valid = true;
  } else if (opcode == RDP_OP_SET_KEY_R &&
             t64_has_rdp_combiner_profile(machine)) {
    rdp->key_width[0] = (uint32_t)((word >> 16) & UINT64_C(0xfff));
    rdp->key_center[0] = (uint32_t)((word >> 8) & UINT64_C(0xff));
    rdp->key_scale[0] = (uint32_t)(word & UINT64_C(0xff));
    rdp->key_center_valid = true;
    rdp->key_scale_valid = true;
    rdp->key_width_valid = true;
  } else if (triangle_words != 0u && draw_triangle) {
    rdp->render_kind = 3u;
    rdp->render_active = true;
    rdp->render_triangle = true;
    rdp->shade_triangle = opcode == UINT32_C(0x0c) ||
                          opcode == UINT32_C(0x0e);
    rdp->render_x_begin = rdp->scissor_xh >> 2;
    rdp->render_x_end = (rdp->scissor_xl >> 2) - 1u;
    if (rdp->render_x_end >= rdp->color_image_width)
      rdp->render_x_end = rdp->color_image_width - 1u;
    rdp->render_y_begin = triangle_y_begin;
    rdp->render_y_end = triangle_y_end;
    rdp->render_y = triangle_y_begin;
  } else if (triangle_words != 0u) {
    rdp->pending_triangle_word_count = 0u;
    rdp->triangle_count++;
  } else if (opcode == RDP_OP_FILL_RECT && draw_fill) {
    rdp->render_kind = 1u;
    rdp->render_active = true;
    rdp->render_x_begin = x0;
    rdp->render_x_end = x1;
    rdp->render_y_begin = y0;
    rdp->render_y_end = y1;
    rdp->render_y = y0;
  } else if (opcode == RDP_OP_SYNC_FULL) {
    rdp->fence_pending = true;
    rdp->fence_generation++;
  }

  if (texture_command) t64_rdp_tmem_apply_command(machine, opcode, word);

  record_command(machine, word, command_byte, opcode);
  rdp->command_pending = false;
  rdp->pending_command_address = 0u;
  rdp->pending_command_byte = 0u;
  rdp->pending_opcode_field = 0u;
  rdp->pending_command_word = 0u;
  rdp->command_count++;
  finish_if_idle(rdp, next);
  return T64_STATUS_OK;
}

void t64_rdp_reset_machine_state(t64_machine *machine) {
  if (machine == NULL) return;
  (void)memset(&machine->rdp, 0, sizeof machine->rdp);
  machine->rdp.random_state = UINT32_C(1);
  machine->rdp.work_policy_version = UINT32_C(2);
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
  case T64_DP_STATUS: value = dpc_status_value(&machine->rdp); break;
  case T64_DP_CLOCK: value = machine->rdp.dpc_clock_counter; break;
  case T64_DP_BUFFER_BUSY: value = machine->rdp.dpc_buffer_busy_counter; break;
  case T64_DP_PIPE_BUSY: value = machine->rdp.dpc_pipe_busy_counter; break;
  case T64_DP_TMEM: value = machine->rdp.dpc_tmem_counter; break;
  default: return T64_STATUS_BUS_UNSUPPORTED;
  }
  *value_out = value;
  return T64_STATUS_OK;
}

t64_status t64_rdp_write_register(t64_machine *machine, uint32_t address,
                                  uint32_t value) {
  t64_rdp_state *rdp;
  t64_status status;
  bool xbus;
  bool freeze_after;
  bool flush_after;

  if (machine == NULL || !t64_has_rdp_vi_profile(machine))
    return T64_STATUS_BUS_UNSUPPORTED;
  if ((address & UINT32_C(3)) != 0u)
    return T64_STATUS_BUS_ALIGNMENT;
  if (address < T64_DP_BASE || address >= T64_DP_BASE + UINT32_C(0x20))
    return T64_STATUS_BUS_UNSUPPORTED;
  rdp = &machine->rdp;
  if (address >= T64_DP_CLOCK) return T64_STATUS_BUS_DEVICE_REFUSED;
  switch (address) {
  case T64_DP_START:
    xbus = rdp->dpc_xbus;
    if (rdp->busy || (value & UINT32_C(7)) != 0u ||
        (xbus ? value >= T64_SP_DMEM_BYTES
              : (uint64_t)value >= machine->profile.rdram_bytes))
      return T64_STATUS_BUS_DEVICE_REFUSED;
    rdp->dpc_start = value;
    rdp->dpc_current = value;
    rdp->dpc_start_valid = true;
    rdp->refusal_reason = T64_RDP_REFUSAL_NONE;
    rdp->dpc_status &= ~T64_RDP_STATUS_ERROR;
    return T64_STATUS_OK;
  case T64_DP_END:
    xbus = rdp->dpc_xbus;
    if ((value & UINT32_C(7)) != 0u || value < rdp->dpc_current ||
        (xbus ? value > T64_SP_DMEM_BYTES
              : (uint64_t)value > machine->profile.rdram_bytes) ||
        (!rdp->busy && !rdp->dpc_start_valid && value > rdp->dpc_current))
      return T64_STATUS_BUS_DEVICE_REFUSED;
    if (value > rdp->dpc_current && !rdp->dpc_freeze && !rdp->dpc_flush &&
        !rdp_event_queued(machine)) {
      status = schedule_work_after_committed(machine);
      if (status != T64_STATUS_OK) return status;
    }
    rdp->dpc_end = value;
    rdp->dpc_end_valid = value > rdp->dpc_current;
    rdp->refusal_reason = T64_RDP_REFUSAL_NONE;
    if (value > rdp->dpc_current) {
      rdp->busy = true;
      rdp->dpc_status |= T64_RDP_STATUS_BUSY;
    } else if (!rdp->render_active && !rdp->fence_pending &&
               !rdp->command_pending) {
      rdp->busy = false;
      rdp->dpc_start_valid = false;
      rdp->dpc_status &= ~T64_RDP_STATUS_BUSY;
    }
    return T64_STATUS_OK;
  case T64_DP_STATUS:
    freeze_after = rdp->dpc_freeze;
    flush_after = rdp->dpc_flush;
    if ((value & T64_DPC_CLR_FREEZE) != 0u) freeze_after = false;
    if ((value & T64_DPC_SET_FREEZE) != 0u) freeze_after = true;
    if ((value & T64_DPC_CLR_FLUSH) != 0u) flush_after = false;
    if ((value & T64_DPC_SET_FLUSH) != 0u) flush_after = true;
    if ((!freeze_after && !flush_after) &&
        (rdp->dpc_freeze || rdp->dpc_flush) && rdp->busy &&
        !rdp_event_queued(machine) &&
        (rdp->command_pending || rdp->render_active || rdp->fence_pending ||
         rdp->dpc_current < rdp->dpc_end)) {
      status = schedule_work_after_committed(machine);
      if (status != T64_STATUS_OK) return status;
    }
    if ((value & T64_DPC_CLR_XBUS) != 0u) rdp->dpc_xbus = false;
    if ((value & T64_DPC_SET_XBUS) != 0u) rdp->dpc_xbus = true;
    rdp->dpc_freeze = freeze_after;
    rdp->dpc_flush = flush_after;
    if ((value & T64_DPC_CLR_TMEM) != 0u) rdp->dpc_tmem_counter = 0u;
    if ((value & T64_DPC_CLR_PIPE) != 0u) rdp->dpc_pipe_busy_counter = 0u;
    if ((value & T64_DPC_CLR_CMD) != 0u)
      rdp->dpc_buffer_busy_counter = 0u;
    if ((value & T64_DPC_CLR_CLOCK) != 0u) rdp->dpc_clock_counter = 0u;
    return T64_STATUS_OK;
  default:
    return T64_STATUS_BUS_UNSUPPORTED;
  }
}

t64_status t64_rdp_event(t64_machine *machine, const t64_event *event) {
  t64_rdp_state *rdp;
  uint32_t address;
  uint32_t reason = T64_RDP_REFUSAL_NONE;
  uint32_t opcode;
  uint64_t word;
  uint64_t triangle_packet[T64_RDP_TRIANGLE_MAX_WORDS] = {0u};
  uint8_t command_byte;
  uint32_t x0 = 0u, y0 = 0u, x1 = 0u, y1 = 0u;
  t64_status status;

  if (machine == NULL || event == NULL || event->device != T64_RDP_DEVICE ||
      event->kind != T64_RDP_EVENT_WORK || !t64_has_rdp_vi_profile(machine))
    return T64_STATUS_INVALID_ARGUMENT;
  rdp = &machine->rdp;
  if (rdp->dpc_freeze || rdp->dpc_flush) return T64_STATUS_OK;
  if (rdp->render_active) return handle_row(machine);
  if (rdp->fence_pending) {
    status = schedule_next_committed(machine, rdp->dpc_current);
    if (status != T64_STATUS_OK) return status;
    increment_counter(&rdp->dpc_clock_counter);
    rdp->fence_pending = false;
    rdp->fullsync_count++;
    t64_mi_set_pending(machine, UINT32_C(0x20));
    t64_trace_emit(machine, machine->tick, T64_TRACE_DEVICE_RDP,
                   T64_TRACE_KIND_RDP_COMPLETE, rdp->fullsync_count);
    finish_if_idle(rdp, rdp->dpc_current);
    return T64_STATUS_OK;
  }
  if (rdp->command_pending) return apply_pending_command(machine);

  if (rdp->texrect_pending) {
    uint32_t first_address = rdp->pending_command_address;
    uint64_t first_word = rdp->pending_texrect_word;
    uint64_t second_word;
    uint32_t rect_opcode = (uint32_t)((first_word >> 56) & UINT64_C(0x3f));
    uint32_t copy_reason = T64_RDP_REFUSAL_RENDER_RANGE;

    address = rdp->dpc_current;
    if (!command_span_valid(machine, address, &reason)) {
      rdp->dpc_current = first_address;
      increment_counter(&rdp->dpc_clock_counter);
      set_refusal(machine, reason);
      return T64_STATUS_OK;
    }
    second_word = read_be64(rdp->dpc_xbus ? machine->sp_dmem + address
                                         : machine->ram + address);
    if (!texture_rect_bounds(rdp, first_word, &x0, &y0, &x1, &y1) ||
        !t64_rdp_tmem_validate_copy(machine, first_word, second_word,
                                    x0, y0, x1, y1, &copy_reason)) {
      rdp->dpc_current = first_address;
      record_command(machine, first_word, (uint8_t)(first_word >> 56),
                     rect_opcode);
      set_refusal(machine, copy_reason);
      return T64_STATUS_OK;
    }
    status = (x0 <= x1 && y0 <= y1)
                 ? schedule_work_after_committed(machine)
                 : schedule_next_committed(machine, address + UINT32_C(8));
    if (status != T64_STATUS_OK) return status;
    increment_counter(&rdp->dpc_clock_counter);
    rdp->dpc_current = address + UINT32_C(8);
    rdp->texrect_pending = false;
    rdp->pending_texrect_word = 0u;
    rdp->pending_command_address = 0u;
    t64_rdp_tmem_begin_copy(machine, first_word, second_word, x0, y0, x1,
                            y1);
    record_command(machine, first_word, (uint8_t)(first_word >> 56),
                   rect_opcode);
    rdp->command_count++;
    finish_if_idle(rdp, rdp->dpc_current);
    return T64_STATUS_OK;
  }
  address = rdp->dpc_current;
  if (address >= rdp->dpc_end) {
    finish_if_idle(rdp, address);
    return T64_STATUS_OK;
  }
  if (!command_span_valid(machine, address, &reason)) {
    increment_counter(&rdp->dpc_clock_counter);
    set_refusal(machine, reason);
    return T64_STATUS_OK;
  }
  word = read_be64(rdp->dpc_xbus ? machine->sp_dmem + address
                                 : machine->ram + address);
  command_byte = (uint8_t)(word >> 56);
  opcode = (uint32_t)(command_byte & UINT8_C(0x3f));
  if (!supported_opcode(machine, opcode)) {
    increment_counter(&rdp->dpc_clock_counter);
    increment_counter(&rdp->dpc_buffer_busy_counter);
    record_command(machine, word, command_byte, opcode);
    set_refusal(machine, T64_RDP_REFUSAL_UNSUPPORTED_OPCODE);
    return T64_STATUS_OK;
  }
  {
    size_t packet_words = t64_rdp_triangle_word_count(opcode);
    if (packet_words != 0u) {
      uint64_t packet_end = (uint64_t)address + (uint64_t)packet_words * 8u;
      uint64_t memory_end = rdp->dpc_xbus ? T64_SP_DMEM_BYTES
                                          : machine->profile.rdram_bytes;
      size_t i;
      if (packet_end > memory_end) {
        increment_counter(&rdp->dpc_clock_counter);
        record_command(machine, word, command_byte, opcode);
        set_refusal(machine, T64_RDP_REFUSAL_SPAN);
        return T64_STATUS_OK;
      }
      if (packet_end > rdp->dpc_end) {
        increment_counter(&rdp->dpc_clock_counter);
        record_command(machine, word, command_byte, opcode);
        set_refusal(machine, T64_RDP_REFUSAL_TRUNCATED_WORD);
        return T64_STATUS_OK;
      }
      for (i = 0u; i < packet_words; i++) {
        const uint8_t *source = rdp->dpc_xbus
                                    ? machine->sp_dmem + address + i * 8u
                                    : machine->ram + address + i * 8u;
        triangle_packet[i] = read_be64(source);
      }
    }
  }
  if ((opcode == RDP_OP_TEX_RECT || opcode == RDP_OP_TEX_RECT_FLIP) &&
      (uint64_t)address + UINT64_C(16) > rdp->dpc_end) {
    increment_counter(&rdp->dpc_clock_counter);
    record_command(machine, word, command_byte, opcode);
    set_refusal(machine, T64_RDP_REFUSAL_TRUNCATED_WORD);
    return T64_STATUS_OK;
  }
  status = schedule_work_after_committed(machine);
  if (status != T64_STATUS_OK) return status;
  increment_counter(&rdp->dpc_clock_counter);
  increment_counter(&rdp->dpc_buffer_busy_counter);
  if (t64_rdp_triangle_word_count(opcode) != 0u) {
    size_t packet_words = t64_rdp_triangle_word_count(opcode);
    (void)memcpy(rdp->pending_triangle_words, triangle_packet,
                 packet_words * sizeof triangle_packet[0]);
  }
  rdp->pending_command_address = address;
  rdp->pending_command_byte = command_byte;
  rdp->pending_opcode_field = opcode;
  rdp->pending_command_word = word;
  rdp->pending_triangle_word_count =
      (uint32_t)t64_rdp_triangle_word_count(opcode);
  rdp->command_pending = true;
  rdp->dpc_current = address +
      (uint32_t)(rdp->pending_triangle_word_count != 0u
                     ? rdp->pending_triangle_word_count * 8u
                     : 8u);
  rdp->dpc_start_valid = false;
  rdp->dpc_end_valid = false;
  return T64_STATUS_OK;
}
