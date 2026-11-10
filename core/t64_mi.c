#include "t64_internal.h"

#include <stdint.h>

#define T64_MI_BASE UINT32_C(0x04300000)
#define T64_MI_INTR_REG (T64_MI_BASE + UINT32_C(0x08))
#define T64_MI_INTR_MASK_REG (T64_MI_BASE + UINT32_C(0x0c))

static void update_irq_line(t64_machine *machine) {
  machine->mi_irq_line =
      (machine->mi_pending & machine->mi_mask & T64_MI_INTERRUPT_BITS) != 0u;
}

static uint64_t pack_mi_state(const t64_machine *machine) {
  return (uint64_t)(machine->mi_pending & T64_MI_INTERRUPT_BITS) |
         ((uint64_t)(machine->mi_mask & T64_MI_INTERRUPT_BITS) << 8) |
         ((uint64_t)(machine->mi_irq_line ? 1u : 0u) << 16);
}

void t64_mi_set_pending(t64_machine *machine, uint32_t bits) {
  uint32_t previous;

  if (machine == NULL) {
    return;
  }
  previous = machine->mi_pending;
  machine->mi_pending =
      (previous | (bits & T64_MI_INTERRUPT_BITS)) & T64_MI_INTERRUPT_BITS;
  if (machine->mi_pending == previous) {
    return;
  }
  update_irq_line(machine);
  t64_trace_emit(machine, machine->tick, T64_TRACE_DEVICE_MI,
                 T64_TRACE_KIND_MI_PENDING, pack_mi_state(machine));
}

void t64_mi_clear_pending(t64_machine *machine, uint32_t bits) {
  uint32_t previous;

  if (machine == NULL) {
    return;
  }
  previous = machine->mi_pending;
  machine->mi_pending &= ~(bits & T64_MI_INTERRUPT_BITS);
  machine->mi_pending &= T64_MI_INTERRUPT_BITS;
  if (machine->mi_pending == previous) {
    return;
  }
  update_irq_line(machine);
  t64_trace_emit(machine, machine->tick, T64_TRACE_DEVICE_MI,
                 T64_TRACE_KIND_MI_PENDING, pack_mi_state(machine));
}

void t64_mi_write_mask(t64_machine *machine, uint32_t command) {
  uint32_t previous;
  unsigned source;

  if (machine == NULL) {
    return;
  }
  previous = machine->mi_mask;
  for (source = 0u; source < 6u; source++) {
    uint32_t source_bit = UINT32_C(1) << source;
    uint32_t clear_command = UINT32_C(1) << (source * 2u);
    uint32_t set_command = clear_command << 1;

    if ((command & clear_command) != 0u) {
      machine->mi_mask &= ~source_bit;
    }
    /* Paired commands use the documented model rule: set wins. */
    if ((command & set_command) != 0u) {
      machine->mi_mask |= source_bit;
    }
  }
  machine->mi_mask &= T64_MI_INTERRUPT_BITS;
  if (machine->mi_mask == previous) {
    return;
  }
  update_irq_line(machine);
  t64_trace_emit(machine, machine->tick, T64_TRACE_DEVICE_MI,
                 T64_TRACE_KIND_MI_MASK, pack_mi_state(machine));
}

bool t64_mi_register_read(const t64_machine *machine, uint32_t address,
                          uint32_t *value_out) {
  uint32_t value;

  if (machine == NULL || value_out == NULL) {
    return false;
  }
  if (address == T64_MI_INTR_REG) {
    value = machine->mi_pending & T64_MI_INTERRUPT_BITS;
  } else if (address == T64_MI_INTR_MASK_REG) {
    value = machine->mi_mask & T64_MI_INTERRUPT_BITS;
  } else {
    return false;
  }
  *value_out = value;
  return true;
}

t64_status t64_mi_register_write(t64_machine *machine, uint32_t address,
                                uint32_t value) {
  if (machine == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (address == T64_MI_INTR_REG) {
    return T64_STATUS_BUS_DEVICE_REFUSED;
  }
  if (address == T64_MI_INTR_MASK_REG) {
    t64_mi_write_mask(machine, value);
    return T64_STATUS_OK;
  }
  return T64_STATUS_BUS_UNSUPPORTED;
}
