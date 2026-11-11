#include "t64_internal.h"

#include "t64_sha256.h"

#include <string.h>

static bool observation_span_is_disjoint(const t64_machine *machine,
                                         t64_address_range range) {
  t64_address_range ram_range;

  return t64_internal_observation_range_is_disjoint(machine, range) &&
         t64_internal_make_range(machine->ram, machine->profile.rdram_bytes,
                                 &ram_range) &&
         !t64_internal_ranges_overlap(range, ram_range);
}

t64_status t64_machine_output_drain(t64_machine *machine, uint8_t *bytes_out,
                                   size_t capacity,
                                   t64_output_drain_result *result_out) {
  t64_address_range result_range;
  t64_address_range bytes_range;
  bool has_bytes_range = false;
  t64_output_drain_result result;
  t64_sha256_ctx digest_context;
  size_t pending_bytes;

  if (machine == NULL || result_out == NULL ||
      !t64_internal_make_range(result_out, sizeof *result_out, &result_range) ||
      !observation_span_is_disjoint(machine, result_range)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (bytes_out == NULL) {
    if (capacity != 0u) {
      return T64_STATUS_INVALID_ARGUMENT;
    }
  } else {
    if (!t64_internal_make_range(bytes_out, capacity, &bytes_range) ||
        !observation_span_is_disjoint(machine, bytes_range) ||
        t64_internal_ranges_overlap(result_range, bytes_range)) {
      return T64_STATUS_INVALID_ARGUMENT;
    }
    has_bytes_range = true;
  }
  if (!machine->has_image || !machine->boot.prepared ||
      machine->output.policy_version != T64_ISVIEWER_POLICY_VERSION) {
    return T64_STATUS_NOT_READY;
  }
  if (machine->output.captured_bytes > T64_ISVIEWER_CAPTURE_BYTES ||
      machine->output_drain_cursor > machine->output.captured_bytes) {
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }

  pending_bytes = (size_t)machine->output.captured_bytes -
                  machine->output_drain_cursor;
  t64_sha256_init(&digest_context);
  if (machine->output.captured_bytes > 0u) {
    t64_sha256_update(&digest_context, machine->output.captured,
                      machine->output.captured_bytes);
  }
  (void)memset(&result, 0, sizeof result);
  result.required_bytes = pending_bytes;
  result.total_attempted_bytes = machine->output.total_attempted_bytes;
  result.lost_output = machine->output.lost_output;
  result.total_attempted_bytes_exact =
      !machine->output.total_attempted_bytes_saturated;
  t64_sha256_final(&digest_context, result.captured_sha256);
  if (capacity < pending_bytes) {
    *result_out = result;
    return T64_STATUS_BUFFER_TOO_SMALL;
  }
  if (pending_bytes > 0u && (!has_bytes_range || bytes_out == NULL)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }

  if (pending_bytes > 0u) {
    (void)memcpy(bytes_out,
                 machine->output.captured + machine->output_drain_cursor,
                 pending_bytes);
  }
  result.copied_bytes = pending_bytes;
  machine->output_drain_cursor += pending_bytes;
  *result_out = result;
  return T64_STATUS_OK;
}

t64_status t64_input_event_handler(t64_machine *machine,
                                   const t64_event *event) {
  if (machine == NULL || event == NULL ||
      event->device != T64_TRACE_DEVICE_INPUT ||
      event->kind != T64_TRACE_KIND_INPUT_SUBMITTED) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  return T64_STATUS_OK;
}

t64_status t64_machine_queue_input(t64_machine *machine, uint64_t tick,
                                   uint32_t port, uint16_t buttons,
                                   int32_t axis_x, int32_t axis_y) {
  t64_event_reservation reservation;
  uint64_t value;
  t64_status status;

  if (machine == NULL || port != 0u || axis_x < T64_INPUT_AXIS_MIN ||
      axis_x > T64_INPUT_AXIS_MAX || axis_y < T64_INPUT_AXIS_MIN ||
      axis_y > T64_INPUT_AXIS_MAX) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!machine->has_image || !machine->boot.prepared ||
      machine->cpu.stopped || machine->halted) {
    return T64_STATUS_NOT_READY;
  }

  value = (uint64_t)buttons | ((uint64_t)(uint8_t)axis_x << 16) |
          ((uint64_t)(uint8_t)axis_y << 24);
  status = t64_schedule_reserve(machine, tick, T64_TRACE_DEVICE_INPUT,
                                T64_TRACE_KIND_INPUT_SUBMITTED, value, 0u,
                                t64_input_event_handler, &reservation);
  if (status != T64_STATUS_OK) {
    return status;
  }
  return t64_schedule_commit(machine, &reservation);
}
