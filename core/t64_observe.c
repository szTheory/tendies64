#include "t64_internal.h"

t64_status t64_machine_output_drain(t64_machine *machine, uint8_t *bytes_out,
                                   size_t capacity,
                                   t64_output_drain_result *result_out) {
  (void)machine;
  (void)bytes_out;
  (void)capacity;
  (void)result_out;
  return T64_STATUS_RESOURCE_EXHAUSTED;
}

t64_status t64_machine_queue_input(t64_machine *machine, uint64_t tick,
                                   uint32_t port, uint16_t buttons,
                                   int32_t axis_x, int32_t axis_y) {
  (void)machine;
  (void)tick;
  (void)port;
  (void)buttons;
  (void)axis_x;
  (void)axis_y;
  return T64_STATUS_RESOURCE_EXHAUSTED;
}
