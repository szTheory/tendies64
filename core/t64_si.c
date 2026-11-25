#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/*
 * Modeled serial interface (SI) with a 64-byte PIF RAM, the joybus transfer
 * events and the per-port input latch of the composite family. The PIF is a
 * modeled RAM with no PIF ROM; it is reachable only through the SI DMA start
 * registers. Every figure and every refusal rule in this file is named
 * software-profile policy; no hardware accuracy is claimed.
 */

_Static_assert(T64_TRACE_DEVICE_SI == T64_SI_DEVICE,
               "public SI trace device alias must equal the device id");
_Static_assert(T64_TRACE_KIND_SI_EXECUTE == T64_SI_EVENT_EXECUTE,
               "public SI execute kind alias must equal the event kind");
_Static_assert(T64_TRACE_KIND_SI_COMPLETE == T64_SI_EVENT_COMPLETE,
               "public SI complete kind alias must equal the event kind");
_Static_assert(T64_TRACE_KIND_SI_REFUSED == T64_SI_TRACE_KIND_REFUSED,
               "public SI refused kind alias must equal the trace kind");

#define SI_WINDOW_BYTES UINT32_C(0x00100000)
#define SI_DRAM_ALIGNMENT_MASK UINT32_C(7)
#define SI_COMMAND_BYTE_OFFSET (T64_PIF_RAM_BYTES - 1u)
#define SI_COMMAND_PARSE UINT8_C(0x01)

static void trace_refusal(t64_machine *machine, uint32_t offset,
                          t64_status reason) {
  uint64_t value = ((uint64_t)reason << 32) | (uint64_t)offset;

  t64_trace_emit(machine, machine->tick, T64_TRACE_DEVICE_SI,
                 T64_SI_TRACE_KIND_REFUSED, value);
}

static t64_status refuse(t64_machine *machine, uint32_t offset,
                         t64_status reason) {
  trace_refusal(machine, offset, reason);
  return reason;
}

static bool port0_present(const t64_machine *machine) {
  return (machine->profile.peripheral_flags &
          T64_PERIPHERAL_STANDARD_CONTROLLER_PORT0) != 0u;
}

/* Promote the pending value when it became visible at or before the tick. */
static void latch_promote(t64_si_port_latch *latch, uint64_t tick) {
  if (latch->next_valid && latch->next_visible_from <= tick) {
    latch->current_buttons = latch->next_buttons;
    latch->current_x = latch->next_x;
    latch->current_y = latch->next_y;
    latch->next_valid = false;
  }
}

void t64_si_input_latch_store(t64_machine *machine, uint32_t port,
                              uint64_t value, uint64_t tick) {
  t64_si_port_latch *latch;

  if (machine == NULL || port >= T64_SI_PORTS) {
    return;
  }
  latch = &machine->si.latch[port];
  /* A value stamped at tick T is visible to samples strictly after T, so the
     previous pending value is promoted first and the new value becomes
     visible from T + 1. A later call at an equal stamp replaces the pending
     value. */
  latch_promote(latch, tick);
  latch->next_buttons = (uint16_t)value;
  latch->next_x = (uint8_t)(value >> 16);
  latch->next_y = (uint8_t)(value >> 24);
  latch->next_visible_from = tick == UINT64_MAX ? UINT64_MAX : tick + 1u;
  latch->next_valid = true;
}

void t64_si_latch_sample(t64_machine *machine, uint32_t port, uint64_t tick,
                         uint16_t *buttons_out, uint8_t *x_out,
                         uint8_t *y_out) {
  const t64_si_port_latch *latch;

  *buttons_out = 0u;
  *x_out = 0u;
  *y_out = 0u;
  if (machine == NULL || port >= T64_SI_PORTS) {
    return;
  }
  latch_promote(&machine->si.latch[port], tick);
  latch = &machine->si.latch[port];
  *buttons_out = latch->current_buttons;
  *x_out = latch->current_x;
  *y_out = latch->current_y;
}

t64_status t64_si_read_register(const t64_machine *machine, uint32_t address,
                                uint32_t *value_out) {
  uint32_t offset;
  uint32_t value;

  if (machine == NULL || value_out == NULL || address < T64_SI_BASE ||
      address >= T64_SI_BASE + SI_WINDOW_BYTES) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!machine->has_image || !t64_has_si_profile(machine)) {
    return T64_STATUS_NOT_READY;
  }
  offset = address - T64_SI_BASE;
  switch (offset) {
  case T64_SI_REG_DRAM_ADDR:
    value = machine->si.dram_address;
    break;
  case T64_SI_REG_STATUS:
    value = (machine->si.active_valid ? T64_SI_STATUS_DMA_BUSY : 0u) |
            (machine->si.error ? T64_SI_STATUS_DMA_ERROR : 0u) |
            ((machine->mi_pending & T64_MI_INTERRUPT_SI) != 0u
                 ? T64_SI_STATUS_INTERRUPT
                 : 0u);
    break;
  default:
    /* RD64B and WR64B are write-only; every other offset is unmodeled. */
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  *value_out = value;
  return T64_STATUS_OK;
}

static t64_status start_dma(t64_machine *machine, uint32_t direction,
                            uint32_t offset, uint32_t pif_address) {
  t64_si_state *si = &machine->si;
  t64_event_reservation reservation;
  uint64_t handshake_ticks = 0u;
  uint64_t execute_delay = 0u;
  uint64_t complete_delay;
  uint64_t token;
  uint8_t command_byte = 0u;
  t64_status status;

  /* Validation order: overlap, PIF address, alignment, span, command byte,
     token, then the shared capacity rule. A refusal changes nothing except
     the sticky error where it is named. */
  if (si->active_valid) {
    si->error = true;
    return refuse(machine, offset, T64_STATUS_BUS_DEVICE_REFUSED);
  }
  if (pif_address != T64_SI_PIF_ADDRESS) {
    return refuse(machine, offset, T64_STATUS_BUS_DEVICE_REFUSED);
  }
  if ((si->dram_address & SI_DRAM_ALIGNMENT_MASK) != 0u) {
    si->error = true;
    return refuse(machine, offset, T64_STATUS_BUS_DEVICE_REFUSED);
  }
  if (machine->ram == NULL ||
      (uint64_t)si->dram_address + (uint64_t)T64_PIF_RAM_BYTES >
          (uint64_t)machine->profile.rdram_bytes) {
    return refuse(machine, offset, T64_STATUS_BUS_DEVICE_REFUSED);
  }
  if (direction == T64_SI_DIRECTION_WR64B) {
    command_byte = machine->ram[si->dram_address + SI_COMMAND_BYTE_OFFSET];
    if ((command_byte & (uint8_t)~SI_COMMAND_PARSE) != 0u) {
      return refuse(machine, offset, T64_STATUS_BUS_DEVICE_REFUSED);
    }
  }
  if (si->token_exhausted) {
    return refuse(machine, offset, T64_STATUS_RESOURCE_EXHAUSTED);
  }
  if (direction == T64_SI_DIRECTION_WR64B) {
    complete_delay = T64_SI_WRITE_DMA_TICKS;
    status = t64_schedule_check_capacity(machine, 1u, complete_delay);
  } else {
    handshake_ticks = t64_joybus_plan_machine(machine, si->pif_ram,
                                              &si->table,
                                              port0_present(machine));
    execute_delay = handshake_ticks;
    complete_delay = handshake_ticks + T64_SI_READ_TAIL_TICKS;
    status = t64_schedule_check_capacity(machine, 2u, complete_delay);
  }
  if (status != T64_STATUS_OK) {
    return refuse(machine, offset, status);
  }

  /* The capacity rule held, so the reserve and commit calls below cannot
     fail; they run in order (K16: a commit invalidates other reservations,
     so the second event is reserved only after the first is committed). */
  token = si->next_token;
  if (direction == T64_SI_DIRECTION_RD64B) {
    status = t64_schedule_reserve_after(
        machine, execute_delay, T64_SI_DEVICE, T64_SI_EVENT_EXECUTE,
        UINT64_C(0), token, t64_si_execute_event, &reservation);
    if (status != T64_STATUS_OK) {
      return refuse(machine, offset, status);
    }
    status = t64_schedule_commit(machine, &reservation);
    if (status != T64_STATUS_OK) {
      return refuse(machine, offset, status);
    }
  }
  status = t64_schedule_reserve_after(
      machine, complete_delay, T64_SI_DEVICE, T64_SI_EVENT_COMPLETE,
      UINT64_C(0), token, t64_si_complete_event, &reservation);
  if (status != T64_STATUS_OK) {
    return refuse(machine, offset, status);
  }
  status = t64_schedule_commit(machine, &reservation);
  if (status != T64_STATUS_OK) {
    return refuse(machine, offset, status);
  }

  si->active_valid = true;
  si->active_direction = direction;
  si->active_dram_address = si->dram_address;
  si->start_tick = machine->tick;
  si->complete_tick = machine->tick + complete_delay;
  si->active_token = token;
  if (token == UINT64_MAX) {
    si->token_exhausted = true;
  } else {
    si->next_token = token + UINT64_C(1);
  }
  if (direction == T64_SI_DIRECTION_WR64B) {
    /* The source is read at the start tick (a named difference from the PI
       model, which reads at completion). */
    (void)memcpy(si->pif_ram, machine->ram + si->dram_address,
                 T64_PIF_RAM_BYTES);
    if ((command_byte & SI_COMMAND_PARSE) != 0u) {
      t64_joybus_parse(si->pif_ram, &si->table);
    }
    si->pif_ram[SI_COMMAND_BYTE_OFFSET] = 0u;
    si->execute_tick = 0u;
    si->execute_pending = false;
  } else {
    si->execute_tick = machine->tick + execute_delay;
    si->execute_pending = true;
  }
  return T64_STATUS_OK;
}

t64_status t64_si_write_register(t64_machine *machine, uint32_t address,
                                 uint32_t value) {
  uint32_t offset;

  if (machine == NULL || address < T64_SI_BASE ||
      address >= T64_SI_BASE + SI_WINDOW_BYTES) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!machine->has_image || !t64_has_si_profile(machine)) {
    return T64_STATUS_NOT_READY;
  }
  offset = address - T64_SI_BASE;
  switch (offset) {
  case T64_SI_REG_DRAM_ADDR:
    machine->si.dram_address = value & T64_SI_DRAM_ADDRESS_MASK;
    return T64_STATUS_OK;
  case T64_SI_REG_RD64B:
    return start_dma(machine, T64_SI_DIRECTION_RD64B, offset, value);
  case T64_SI_REG_WR64B:
    return start_dma(machine, T64_SI_DIRECTION_WR64B, offset, value);
  case T64_SI_REG_STATUS:
    /* Any write acknowledges the SI interrupt and nothing else; the sticky
       error is cleared only by reset or replacement. */
    t64_mi_clear_pending(machine, T64_MI_INTERRUPT_SI);
    return T64_STATUS_OK;
  default:
    return T64_STATUS_BUS_UNSUPPORTED;
  }
}

t64_status t64_si_execute_event(t64_machine *machine, const t64_event *event) {
  t64_si_state *si;
  t64_joybus_controller port0;

  if (machine == NULL || event == NULL || event->device != T64_SI_DEVICE ||
      event->kind != T64_SI_EVENT_EXECUTE) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  si = &machine->si;
  if (!si->active_valid || si->active_direction != T64_SI_DIRECTION_RD64B ||
      si->active_token != event->token || !si->execute_pending) {
    return T64_STATUS_OK;
  }
  (void)memset(&port0, 0, sizeof port0);
  port0.present = port0_present(machine);
  /* Controllers are sampled at the execute tick of the event itself, never
     at the machine tick of an advance request. */
  t64_si_latch_sample(machine, 0u, event->tick, &port0.buttons, &port0.x,
                      &port0.y);
  t64_joybus_execute_machine(machine, event->tick, si->pif_ram, &si->table,
                             &port0);
  si->execute_pending = false;
  return T64_STATUS_OK;
}

t64_status t64_si_complete_event(t64_machine *machine,
                                 const t64_event *event) {
  t64_si_state *si;

  if (machine == NULL || event == NULL || event->device != T64_SI_DEVICE ||
      event->kind != T64_SI_EVENT_COMPLETE) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  si = &machine->si;
  if (!si->active_valid || si->active_token != event->token) {
    return T64_STATUS_OK;
  }
  if (si->active_direction == T64_SI_DIRECTION_RD64B) {
    if (machine->ram == NULL ||
        (uint64_t)si->active_dram_address + (uint64_t)T64_PIF_RAM_BYTES >
            (uint64_t)machine->profile.rdram_bytes) {
      return T64_STATUS_RESOURCE_EXHAUSTED;
    }
    /* The whole PIF image reaches RDRAM at the completion tick or not at
       all; a CPU or bus read before then sees the old bytes. */
    (void)memcpy(machine->ram + si->active_dram_address, si->pif_ram,
                 T64_PIF_RAM_BYTES);
    t64_rdp_memory_coverage_invalidate(machine, si->active_dram_address,
                                       T64_PIF_RAM_BYTES);
    t64_cpu_link_invalidate_range(machine, si->active_dram_address,
                                  T64_PIF_RAM_BYTES);
  }
  si->active_valid = false;
  si->active_direction = T64_SI_DIRECTION_NONE;
  si->active_dram_address = 0u;
  si->start_tick = 0u;
  si->execute_tick = 0u;
  si->complete_tick = 0u;
  si->active_token = 0u;
  si->execute_pending = false;
  t64_mi_set_pending(machine, T64_MI_INTERRUPT_SI);
  return T64_STATUS_OK;
}

void t64_si_reset_machine_state(t64_machine *machine) {
  size_t offset;

  if (machine == NULL) {
    return;
  }
  offset = 0u;
  while (offset < machine->queue_count) {
    const t64_event event =
        machine->events[(machine->queue_head + offset) %
                        T64_EVENT_QUEUE_CAPACITY];

    if (event.device == T64_SI_DEVICE) {
      uint64_t ignored_tick;

      if (t64_schedule_cancel(machine, event.device, event.kind, event.token,
                              &ignored_tick) != T64_STATUS_OK) {
        offset++;
      }
    } else {
      offset++;
    }
  }
  if (t64_has_si_profile(machine)) {
    t64_mi_clear_pending(machine, T64_MI_INTERRUPT_SI);
  }
  (void)memset(&machine->si, 0, sizeof machine->si);
  machine->si.next_token = UINT64_C(1);
}
