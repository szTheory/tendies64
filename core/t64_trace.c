#include "t64_internal.h"

t64_status t64_machine_trace_set_enabled(t64_machine *machine, bool enabled) {
  if (machine == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  machine->trace_enabled = enabled;
  return T64_STATUS_OK;
}

t64_status t64_machine_trace_drain(
    t64_machine *machine, t64_trace_record *records_out, size_t capacity,
    size_t *count_out, uint64_t *dropped_out,
    bool *drop_count_saturated_out) {
  if (machine == NULL || count_out == NULL || dropped_out == NULL ||
      drop_count_saturated_out == NULL ||
      (records_out == NULL && capacity != 0u)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  *count_out = 0u;
  *dropped_out = 0u;
  *drop_count_saturated_out = false;
  return T64_STATUS_OK;
}

void t64_trace_reset(t64_machine *machine) { (void)machine; }

void t64_trace_emit(t64_machine *machine, uint64_t tick, uint32_t device,
                    uint32_t kind, uint64_t value) {
  (void)machine;
  (void)tick;
  (void)device;
  (void)kind;
  (void)value;
}
