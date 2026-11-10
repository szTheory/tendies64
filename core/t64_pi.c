#include "t64_internal.h"

#include <limits.h>
#include <string.h>

#define T64_PI_BASE UINT32_C(0x04600000)
#define T64_PI_END UINT32_C(0x04700000)
#define T64_PI_DRAM_ADDRESS_MASK UINT32_C(0x00ffffff)
#define T64_PI_LENGTH_MASK UINT32_C(0x00ffffff)
#define T64_PI_MAX_TRANSFER_BYTES UINT32_C(0x01000000)
#define T64_PI_TIMING_8BIT_MASK UINT32_C(0x000000ff)
#define T64_PI_TIMING_4BIT_MASK UINT32_C(0x0000000f)
#define T64_PI_TIMING_2BIT_MASK UINT32_C(0x00000003)
#define T64_PI_CART_BASE UINT32_C(0x10000000)
#define T64_PI_CART_END UINT32_C(0x1fc00000)
#define T64_MI_PI_INTERRUPT UINT32_C(0x10)

static void trace_refusal(t64_machine *machine, uint32_t offset,
                          t64_status reason) {
  uint64_t value = ((uint64_t)reason << 32) | (uint64_t)offset;

  t64_trace_emit(machine, machine->tick, T64_TRACE_DEVICE_PI,
                 T64_TRACE_KIND_PI_REFUSED, value);
}

static t64_status refuse_semantic(t64_machine *machine, uint32_t offset) {
  trace_refusal(machine, offset, T64_STATUS_BUS_DEVICE_REFUSED);
  return T64_STATUS_BUS_DEVICE_REFUSED;
}

static bool request_range_valid(const t64_machine *machine,
                                uint32_t dram_address, uint32_t cart_address,
                                uint32_t length) {
  uint64_t dram_end = (uint64_t)dram_address + (uint64_t)length;
  uint64_t cart_end = (uint64_t)cart_address + (uint64_t)length;
  uint64_t cart_offset;

  if (machine == NULL || !machine->has_image || length == 0u ||
      dram_end > machine->profile.rdram_bytes ||
      cart_address < T64_PI_CART_BASE || cart_end > T64_PI_CART_END) {
    return false;
  }
  cart_offset = (uint64_t)cart_address - (uint64_t)T64_PI_CART_BASE;
  return cart_offset <= (uint64_t)machine->image_length &&
         (uint64_t)length <= (uint64_t)machine->image_length - cart_offset;
}

static t64_status start_rom_dma(t64_machine *machine, uint32_t raw_length) {
  t64_event_reservation reservation;
  t64_pi_state *pi = &machine->pi;
  uint32_t length = (raw_length & T64_PI_LENGTH_MASK) + UINT32_C(1);
  uint64_t delay;
  uint64_t token;
  t64_status status;

  if (pi->active_valid) {
    /* The sticky busy-start ERROR effect is the selected profile policy. */
    pi->write_length = raw_length & T64_PI_LENGTH_MASK;
    pi->error = true;
    trace_refusal(machine, UINT32_C(0x0c), T64_STATUS_BUS_DEVICE_REFUSED);
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }

  if (length == 0u || length > T64_PI_MAX_TRANSFER_BYTES ||
      (length & UINT32_C(1)) != 0u ||
      (pi->dram_address & UINT32_C(7)) != 0u ||
      (pi->cart_address & UINT32_C(1)) != 0u ||
      !request_range_valid(machine, pi->dram_address, pi->cart_address,
                           length)) {
    pi->write_length = raw_length & T64_PI_LENGTH_MASK;
    trace_refusal(machine, UINT32_C(0x0c), T64_STATUS_BUS_DEVICE_REFUSED);
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  if (pi->token_exhausted) {
    trace_refusal(machine, UINT32_C(0x0c), T64_STATUS_RESOURCE_EXHAUSTED);
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }
  if ((uint64_t)(length / UINT32_C(2)) >= UINT64_MAX - UINT64_C(1)) {
    trace_refusal(machine, UINT32_C(0x0c), T64_STATUS_TICK_OVERFLOW);
    return T64_STATUS_TICK_OVERFLOW;
  }
  delay = UINT64_C(1) + (uint64_t)(length / UINT32_C(2));
  token = pi->next_token;
  status = t64_schedule_reserve_after(machine, delay, T64_DEVICE_PI,
                                      T64_PI_EVENT_COMPLETE,
                                      (uint64_t)length, token,
                                      t64_pi_complete, &reservation);
  if (status != T64_STATUS_OK) {
    trace_refusal(machine, UINT32_C(0x0c), status);
    return status;
  }
  status = t64_schedule_commit(machine, &reservation);
  if (status != T64_STATUS_OK) {
    trace_refusal(machine, UINT32_C(0x0c), status);
    return status;
  }

  pi->active_dram_address = pi->dram_address;
  pi->active_cart_address = pi->cart_address;
  pi->active_length = length;
  pi->write_length = raw_length & T64_PI_LENGTH_MASK;
  pi->active_token = token;
  pi->active_canceled = false;
  pi->active_valid = true;
  if (token == UINT64_MAX) {
    pi->token_exhausted = true;
  } else {
    pi->next_token = token + UINT64_C(1);
  }
  t64_trace_emit(machine, machine->tick, T64_TRACE_DEVICE_PI,
                 T64_TRACE_KIND_PI_ISSUE, token);
  return T64_STATUS_OK;
}

static void reset_controller(t64_machine *machine) {
  t64_pi_state *pi = &machine->pi;

  pi->error = false;
  if (pi->active_valid && !pi->active_canceled) {
    pi->active_canceled = true;
    t64_trace_emit(machine, machine->tick, T64_TRACE_DEVICE_PI,
                   T64_TRACE_KIND_PI_CANCEL, pi->active_token);
  }
}

t64_status t64_pi_read_register(const t64_machine *machine, uint32_t address,
                                uint32_t *value_out) {
  uint32_t offset;
  uint32_t value;

  if (machine == NULL || value_out == NULL || address < T64_PI_BASE ||
      address >= T64_PI_END) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  offset = address - T64_PI_BASE;
  switch (offset) {
  case UINT32_C(0x00):
    value = machine->pi.dram_address;
    break;
  case UINT32_C(0x04):
    value = machine->pi.cart_address;
    break;
  case UINT32_C(0x08):
    value = machine->pi.read_length;
    break;
  case UINT32_C(0x0c):
    value = machine->pi.write_length;
    break;
  case UINT32_C(0x10):
    value = machine->pi.active_valid ? UINT32_C(1) : UINT32_C(0);
    if (machine->pi.error) {
      value |= UINT32_C(4);
    }
    break;
  case UINT32_C(0x14):
    value = machine->pi.domain1_latency;
    break;
  case UINT32_C(0x18):
    value = machine->pi.domain1_pulse_width;
    break;
  case UINT32_C(0x1c):
    value = machine->pi.domain1_page_size;
    break;
  case UINT32_C(0x20):
    value = machine->pi.domain1_release_duration;
    break;
  case UINT32_C(0x24):
    value = machine->pi.domain2_latency;
    break;
  case UINT32_C(0x28):
    value = machine->pi.domain2_pulse_width;
    break;
  case UINT32_C(0x2c):
    value = machine->pi.domain2_page_size;
    break;
  case UINT32_C(0x30):
    value = machine->pi.domain2_release_duration;
    break;
  default:
    return T64_STATUS_BUS_UNSUPPORTED;
  }
  *value_out = value;
  return T64_STATUS_OK;
}

t64_status t64_pi_write_register(t64_machine *machine, uint32_t address,
                                 uint32_t value) {
  uint32_t offset;
  t64_pi_state *pi;

  if (machine == NULL || address < T64_PI_BASE || address >= T64_PI_END) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  pi = &machine->pi;
  offset = address - T64_PI_BASE;
  switch (offset) {
  case UINT32_C(0x00):
    pi->dram_address = value & T64_PI_DRAM_ADDRESS_MASK;
    return T64_STATUS_OK;
  case UINT32_C(0x04):
    pi->cart_address = value;
    return T64_STATUS_OK;
  case UINT32_C(0x08):
    pi->read_length = value & T64_PI_LENGTH_MASK;
    return refuse_semantic(machine, offset);
  case UINT32_C(0x0c):
    return start_rom_dma(machine, value & T64_PI_LENGTH_MASK);
  case UINT32_C(0x10):
    if ((value & UINT32_C(1)) != 0u) {
      reset_controller(machine);
    }
    if ((value & UINT32_C(2)) != 0u) {
      t64_mi_clear_pending(machine, T64_MI_PI_INTERRUPT);
    }
    return T64_STATUS_OK;
  case UINT32_C(0x14):
    pi->domain1_latency = value & T64_PI_TIMING_8BIT_MASK;
    return T64_STATUS_OK;
  case UINT32_C(0x18):
    pi->domain1_pulse_width = value & T64_PI_TIMING_8BIT_MASK;
    return T64_STATUS_OK;
  case UINT32_C(0x1c):
    pi->domain1_page_size = value & T64_PI_TIMING_4BIT_MASK;
    return T64_STATUS_OK;
  case UINT32_C(0x20):
    pi->domain1_release_duration = value & T64_PI_TIMING_2BIT_MASK;
    return T64_STATUS_OK;
  case UINT32_C(0x24):
    pi->domain2_latency = value & T64_PI_TIMING_8BIT_MASK;
    return T64_STATUS_OK;
  case UINT32_C(0x28):
    pi->domain2_pulse_width = value & T64_PI_TIMING_8BIT_MASK;
    return T64_STATUS_OK;
  case UINT32_C(0x2c):
    pi->domain2_page_size = value & T64_PI_TIMING_4BIT_MASK;
    return T64_STATUS_OK;
  case UINT32_C(0x30):
    pi->domain2_release_duration = value & T64_PI_TIMING_2BIT_MASK;
    return T64_STATUS_OK;
  default:
    return T64_STATUS_BUS_UNSUPPORTED;
  }
}

t64_status t64_pi_complete(t64_machine *machine, const t64_event *event) {
  uint64_t cart_offset;
  uint64_t destination_end;
  uint64_t source_end;

  if (machine == NULL || event == NULL || event->device != T64_DEVICE_PI ||
      event->kind != T64_PI_EVENT_COMPLETE) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!machine->pi.active_valid ||
      machine->pi.active_token != event->token) {
    return T64_STATUS_OK;
  }

  cart_offset = (uint64_t)machine->pi.active_cart_address -
                (uint64_t)T64_PI_CART_BASE;
  destination_end = (uint64_t)machine->pi.active_dram_address +
                    (uint64_t)machine->pi.active_length;
  source_end = cart_offset + (uint64_t)machine->pi.active_length;
  if (!machine->has_image || machine->image_storage.data == NULL ||
      machine->ram == NULL ||
      destination_end > machine->profile.rdram_bytes ||
      source_end > (uint64_t)machine->image_length) {
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }
  if (!machine->pi.active_canceled) {
    (void)memcpy(machine->ram + machine->pi.active_dram_address,
                 (const uint8_t *)machine->image_storage.data +
                     (size_t)cart_offset,
                 (size_t)machine->pi.active_length);
  }
  machine->pi.active_valid = false;
  machine->pi.active_canceled = false;
  machine->pi.active_dram_address = 0u;
  machine->pi.active_cart_address = 0u;
  machine->pi.active_length = 0u;
  machine->pi.active_token = 0u;
  t64_mi_set_pending(machine, T64_MI_PI_INTERRUPT);
  t64_trace_emit(machine, event->tick, T64_TRACE_DEVICE_PI,
                 T64_TRACE_KIND_PI_COMPLETE, event->token);
  return T64_STATUS_OK;
}

void t64_pi_reset_machine_state(t64_machine *machine) {
  if (machine == NULL) {
    return;
  }
  (void)memset(&machine->pi, 0, sizeof machine->pi);
  machine->pi.next_token = UINT64_C(1);
}
