/* Linkable RED scaffold; the closed schedule behavior is implemented next. */
#include "t64_input.h"

#include <stdlib.h>

struct t64_input_schedule {
  size_t event_count;
  t64_input_event events[T64_INPUT_MAX_EVENTS];
  uint8_t *canonical_json;
  size_t canonical_length;
  uint8_t sha256[T64_INPUT_SHA256_BYTES];
};

t64_input_status t64_input_parse(const uint8_t *bytes, size_t length,
                                 t64_input_schedule **schedule_out,
                                 t64_input_error *error_out) {
  (void)bytes;
  (void)length;
  if (error_out != NULL) {
    error_out->status = T64_INPUT_INVALID_SCHEMA;
    error_out->input_offset = 0u;
  }
  if (schedule_out == NULL) return T64_INPUT_INVALID_ARGUMENT;
  return T64_INPUT_INVALID_SCHEMA;
}

void t64_input_schedule_destroy(t64_input_schedule *schedule) {
  if (schedule != NULL) {
    free(schedule->canonical_json);
    free(schedule);
  }
}

size_t t64_input_schedule_event_count(const t64_input_schedule *schedule) {
  return schedule == NULL ? 0u : schedule->event_count;
}

bool t64_input_schedule_event(const t64_input_schedule *schedule,
                              size_t index, t64_input_event *event_out) {
  if (schedule == NULL || event_out == NULL || index >= schedule->event_count) {
    return false;
  }
  *event_out = schedule->events[index];
  return true;
}

const uint8_t *t64_input_schedule_canonical_json(
    const t64_input_schedule *schedule, size_t *length_out) {
  if (schedule == NULL || length_out == NULL) return NULL;
  *length_out = schedule->canonical_length;
  return schedule->canonical_json;
}

const uint8_t *t64_input_schedule_sha256(
    const t64_input_schedule *schedule) {
  return schedule == NULL ? NULL : schedule->sha256;
}
