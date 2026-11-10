#include "t64_internal.h"

#include <limits.h>
#include <string.h>

static size_t event_index(const t64_machine *machine, size_t offset) {
  return (machine->queue_head + offset) % T64_EVENT_QUEUE_CAPACITY;
}

static bool event_precedes(const t64_event *left, const t64_event *right) {
  return left->tick < right->tick ||
         (left->tick == right->tick && left->sequence < right->sequence);
}

static size_t queue_limit(const t64_machine *machine) {
  size_t profile_limit = (size_t)machine->profile.event_queue_capacity;

  return profile_limit < T64_EVENT_QUEUE_CAPACITY ? profile_limit
                                                   : T64_EVENT_QUEUE_CAPACITY;
}

static uint64_t sequence_limit(const t64_machine *machine) {
  uint32_t width = machine->profile.sequence_width_bits;

  return width == 64u ? UINT64_MAX : (UINT64_C(1) << width) - UINT64_C(1);
}

static t64_status test_noop_handler(t64_machine *machine,
                                    const t64_event *event) {
  (void)machine;
  (void)event;
  return T64_STATUS_OK;
}

static t64_status test_instruction_handler(t64_machine *machine,
                                           const t64_event *event) {
  machine->instruction_count += event->value;
  return T64_STATUS_OK;
}

static t64_status test_frame_handler(t64_machine *machine,
                                     const t64_event *event) {
  machine->frame_event_count += event->value;
  return T64_STATUS_OK;
}

static t64_status test_halt_handler(t64_machine *machine,
                                    const t64_event *event) {
  (void)event;
  machine->halted = true;
  return T64_STATUS_OK;
}

static t64_status test_repeat_instruction_handler(t64_machine *machine,
                                                  const t64_event *event) {
  t64_event_reservation reservation;
  t64_status status;

  machine->instruction_count += event->value;
  status = t64_schedule_reserve(machine, event->tick,
                                T64_DEVICE_SYNTHETIC_TEST,
                                T64_TEST_EVENT_REPEAT_INSTRUCTION,
                                event->value, event->token,
                                test_repeat_instruction_handler, &reservation);
  if (status != T64_STATUS_OK) {
    return status;
  }
  return t64_schedule_commit(machine, &reservation);
}

static t64_event_handler_fn test_handler_for_kind(uint32_t kind) {
  switch (kind) {
  case T64_TEST_EVENT_NOOP:
    return test_noop_handler;
  case T64_TEST_EVENT_INSTRUCTION:
    return test_instruction_handler;
  case T64_TEST_EVENT_FRAME:
    return test_frame_handler;
  case T64_TEST_EVENT_HALT:
    return test_halt_handler;
  case T64_TEST_EVENT_REPEAT_INSTRUCTION:
    return test_repeat_instruction_handler;
  default:
    return NULL;
  }
}

t64_status t64_schedule_reserve(t64_machine *machine, uint64_t tick,
                                uint32_t device, uint32_t kind,
                                uint64_t value, uint64_t token,
                                t64_event_handler_fn handler,
                                t64_event_reservation *reservation_out) {
  if (reservation_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  (void)memset(reservation_out, 0, sizeof *reservation_out);
  if (machine == NULL || handler == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!machine->has_image) {
    return T64_STATUS_NOT_READY;
  }
  if (tick < machine->tick) {
    return T64_STATUS_TIME_REVERSED;
  }
  if (machine->queue_count >= queue_limit(machine)) {
    return T64_STATUS_QUEUE_FULL;
  }
  if (machine->sequence_exhausted) {
    return T64_STATUS_SEQUENCE_EXHAUSTED;
  }
  if (machine->next_sequence > sequence_limit(machine)) {
    return T64_STATUS_SEQUENCE_EXHAUSTED;
  }

  reservation_out->machine = machine;
  reservation_out->expected_queue_count = machine->queue_count;
  reservation_out->expected_tick = machine->tick;
  reservation_out->event.tick = tick;
  reservation_out->event.sequence = machine->next_sequence;
  reservation_out->event.device = device;
  reservation_out->event.kind = kind;
  reservation_out->event.value = value;
  reservation_out->event.token = token;
  reservation_out->event.handler = handler;
  reservation_out->valid = true;
  return T64_STATUS_OK;
}

t64_status t64_schedule_reserve_after(t64_machine *machine, uint64_t delay,
                                      uint32_t device, uint32_t kind,
                                      uint64_t value, uint64_t token,
                                      t64_event_handler_fn handler,
                                      t64_event_reservation *reservation_out) {
  uint64_t tick;

  if (machine == NULL || reservation_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (delay > UINT64_MAX - machine->tick) {
    (void)memset(reservation_out, 0, sizeof *reservation_out);
    return T64_STATUS_TICK_OVERFLOW;
  }
  tick = machine->tick + delay;
  return t64_schedule_reserve(machine, tick, device, kind, value, token,
                              handler, reservation_out);
}

t64_status t64_schedule_commit(t64_machine *machine,
                               t64_event_reservation *reservation) {
  size_t insert_at;
  size_t i;
  t64_event event;
  uint64_t maximum_sequence;

  maximum_sequence = machine == NULL ? 0u : sequence_limit(machine);
  if (machine == NULL || reservation == NULL || !reservation->valid ||
      reservation->machine != machine ||
      reservation->expected_tick != machine->tick ||
      reservation->expected_queue_count != machine->queue_count ||
      reservation->event.sequence != machine->next_sequence ||
      machine->sequence_exhausted ||
      reservation->event.sequence > maximum_sequence ||
      reservation->event.tick < machine->tick ||
      machine->queue_count >= queue_limit(machine)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }

  event = reservation->event;
  insert_at = machine->queue_count;
  for (i = machine->queue_count; i > 0u; i--) {
    size_t previous_index = event_index(machine, i - 1u);

    if (!event_precedes(&event, &machine->events[previous_index])) {
      break;
    }
    machine->events[event_index(machine, i)] =
        machine->events[previous_index];
    insert_at = i - 1u;
  }
  machine->events[event_index(machine, insert_at)] = event;
  machine->queue_count++;
  if (event.sequence == maximum_sequence) {
    machine->sequence_exhausted = true;
  } else {
    machine->next_sequence = event.sequence + UINT64_C(1);
  }
  reservation->valid = false;
  return T64_STATUS_OK;
}

t64_status t64_test_schedule_synthetic(t64_machine *machine, uint64_t tick,
                                       uint32_t kind, uint64_t value) {
  t64_event_reservation reservation;
  t64_status status;

  status = t64_test_reserve_synthetic(machine, tick, kind, value,
                                       &reservation);
  if (status != T64_STATUS_OK) {
    return status;
  }
  return t64_schedule_commit(machine, &reservation);
}

t64_status t64_test_schedule_synthetic_after(t64_machine *machine,
                                             uint64_t delay, uint32_t kind,
                                             uint64_t value) {
  t64_event_handler_fn handler = test_handler_for_kind(kind);
  t64_event_reservation reservation;
  t64_status status;

  if (handler == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  status = t64_schedule_reserve_after(machine, delay,
                                      T64_DEVICE_SYNTHETIC_TEST, kind, value,
                                      UINT64_C(0), handler, &reservation);
  if (status != T64_STATUS_OK) {
    return status;
  }
  return t64_schedule_commit(machine, &reservation);
}

t64_status t64_test_reserve_synthetic(t64_machine *machine, uint64_t tick,
                                      uint32_t kind, uint64_t value,
                                      t64_event_reservation *reservation_out) {
  t64_event_handler_fn handler = test_handler_for_kind(kind);

  if (handler == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  return t64_schedule_reserve(machine, tick, T64_DEVICE_SYNTHETIC_TEST,
                              kind, value, UINT64_C(0), handler,
                              reservation_out);
}

bool t64_test_event_handler_matches(const t64_machine *machine,
                                    size_t logical_offset) {
  const t64_event *event;

  if (machine == NULL || logical_offset >= machine->queue_count) {
    return false;
  }
  event = &machine->events[event_index(machine, logical_offset)];
  return event->device == T64_DEVICE_SYNTHETIC_TEST &&
         event->handler == test_handler_for_kind(event->kind);
}

static const t64_event *queue_front(const t64_machine *machine) {
  return &machine->events[machine->queue_head];
}

static void queue_pop_front(t64_machine *machine) {
  machine->queue_head = (machine->queue_head + 1u) % T64_EVENT_QUEUE_CAPACITY;
  machine->queue_count--;
  if (machine->queue_count == 0u) {
    machine->queue_head = 0u;
  }
}

static bool event_counter_would_overflow(const t64_machine *machine,
                                         const t64_event *event) {
  if (event->device != T64_DEVICE_SYNTHETIC_TEST) {
    return false;
  }
  if (event->kind == T64_TEST_EVENT_INSTRUCTION ||
      event->kind == T64_TEST_EVENT_REPEAT_INSTRUCTION) {
    return event->value > UINT64_MAX - machine->instruction_count;
  }
  if (event->kind == T64_TEST_EVENT_FRAME) {
    return event->value > UINT64_MAX - machine->frame_event_count;
  }
  return false;
}

static bool event_can_dispatch(const t64_machine *machine,
                               const t64_event *event) {
  return event->handler != NULL &&
         !event_counter_would_overflow(machine, event);
}

static t64_status dispatch_front(t64_machine *machine) {
  t64_event event = *queue_front(machine);
  t64_status status;

  if (!event_can_dispatch(machine, &event)) {
    return T64_STATUS_RESOURCE_EXHAUSTED;
  }
  queue_pop_front(machine);
  machine->tick = event.tick;
  t64_trace_emit(machine, event.tick, event.device, event.kind, event.value);
  status = event.handler(machine, &event);
  return status;
}

static void set_stop_result(t64_stop_result *result_out, uint32_t reason,
                            uint64_t tick) {
  result_out->reason = reason;
  result_out->tick = tick;
}

static uint64_t selected_counter(const t64_machine *machine,
                                 uint32_t stop_kind) {
  return stop_kind == T64_STOP_INSTRUCTION_COUNT
             ? machine->instruction_count
             : machine->frame_event_count;
}

static uint32_t reached_reason(uint32_t stop_kind) {
  return stop_kind == T64_STOP_INSTRUCTION_COUNT
             ? T64_STOP_REASON_INSTRUCTION_COUNT
             : T64_STOP_REASON_FRAME_EVENT_COUNT;
}

static t64_status advance_time_bound(t64_machine *machine,
                                     const t64_stop_request *request,
                                     t64_stop_result *result_out) {
  size_t dispatched = 0u;

  if (request->value < machine->tick) {
    return T64_STATUS_TIME_REVERSED;
  }
  if (machine->halted) {
    set_stop_result(result_out, T64_STOP_REASON_HALT, machine->tick);
    return T64_STATUS_OK;
  }
  while (machine->queue_count > 0u && queue_front(machine)->tick <= request->value) {
    t64_status status;

    if (dispatched >= machine->profile.advance_event_budget) {
      set_stop_result(result_out, T64_STOP_REASON_RESOURCE_LIMIT, machine->tick);
      return T64_STATUS_OK;
    }
    if (!event_can_dispatch(machine, queue_front(machine))) {
      set_stop_result(result_out, T64_STOP_REASON_RESOURCE_LIMIT, machine->tick);
      return T64_STATUS_OK;
    }
    status = dispatch_front(machine);
    dispatched++;
    if (status != T64_STATUS_OK) {
      set_stop_result(result_out, T64_STOP_REASON_RESOURCE_LIMIT, machine->tick);
      return T64_STATUS_OK;
    }
    if (machine->halted) {
      set_stop_result(result_out, T64_STOP_REASON_HALT, machine->tick);
      return T64_STATUS_OK;
    }
  }
  machine->tick = request->value;
  set_stop_result(result_out, T64_STOP_REASON_BOUND, machine->tick);
  return T64_STATUS_OK;
}

static t64_status advance_counter_bound(t64_machine *machine,
                                       const t64_stop_request *request,
                                       t64_stop_result *result_out) {
  uint64_t current = selected_counter(machine, request->kind);
  size_t dispatched = 0u;

  if (request->value < current) {
    return T64_STATUS_LIMIT_REVERSED;
  }
  if (machine->halted) {
    set_stop_result(result_out, T64_STOP_REASON_HALT, machine->tick);
    return T64_STATUS_OK;
  }
  if (request->value == current) {
    set_stop_result(result_out, reached_reason(request->kind), machine->tick);
    return T64_STATUS_OK;
  }
  while (current < request->value) {
    t64_status status;

    if (machine->queue_count == 0u) {
      set_stop_result(result_out, T64_STOP_REASON_QUIESCENT, machine->tick);
      return T64_STATUS_OK;
    }
    if (dispatched >= machine->profile.advance_event_budget) {
      set_stop_result(result_out, T64_STOP_REASON_RESOURCE_LIMIT, machine->tick);
      return T64_STATUS_OK;
    }
    if (!event_can_dispatch(machine, queue_front(machine))) {
      set_stop_result(result_out, T64_STOP_REASON_RESOURCE_LIMIT, machine->tick);
      return T64_STATUS_OK;
    }
    status = dispatch_front(machine);
    dispatched++;
    if (status != T64_STATUS_OK) {
      set_stop_result(result_out, T64_STOP_REASON_RESOURCE_LIMIT, machine->tick);
      return T64_STATUS_OK;
    }
    if (machine->halted) {
      set_stop_result(result_out, T64_STOP_REASON_HALT, machine->tick);
      return T64_STATUS_OK;
    }
    current = selected_counter(machine, request->kind);
  }
  set_stop_result(result_out, reached_reason(request->kind), machine->tick);
  return T64_STATUS_OK;
}

t64_status t64_scheduler_advance(t64_machine *machine,
                                 const t64_stop_request *request,
                                 t64_stop_result *result_out) {
  if (machine == NULL || request == NULL || result_out == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (!machine->has_image) {
    return T64_STATUS_NOT_READY;
  }
  if (request->kind == T64_STOP_GUEST_TICK) {
    return advance_time_bound(machine, request, result_out);
  }
  if (request->kind == T64_STOP_INSTRUCTION_COUNT ||
      request->kind == T64_STOP_FRAME_EVENT_COUNT) {
    return advance_counter_bound(machine, request, result_out);
  }
  return T64_STATUS_INVALID_ARGUMENT;
}
