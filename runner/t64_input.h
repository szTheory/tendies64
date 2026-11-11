/* Closed, bounded host-side t64.input v1 schedule parser. */
#ifndef T64_INPUT_H
#define T64_INPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define T64_INPUT_MAX_BYTES 65536u
#define T64_INPUT_MAX_EVENTS 128u
#define T64_INPUT_SCHEMA_VERSION 1u
#define T64_INPUT_SHA256_BYTES 32u

typedef enum {
  T64_INPUT_OK = 0,
  T64_INPUT_INVALID_ARGUMENT,
  T64_INPUT_SIZE_LIMIT,
  T64_INPUT_INVALID_JSON,
  T64_INPUT_INVALID_SCHEMA,
  T64_INPUT_EVENT_LIMIT,
  T64_INPUT_INVALID_EVENT,
  T64_INPUT_TIME_REVERSED,
  T64_INPUT_ALLOCATION_FAILED,
  T64_INPUT_CANONICALIZATION_FAILED
} t64_input_status;

typedef struct {
  t64_input_status status;
  size_t input_offset;
} t64_input_error;

typedef struct {
  uint64_t tick;
  uint32_t port;
  uint16_t buttons;
  int32_t axis_x;
  int32_t axis_y;
} t64_input_event;

typedef struct t64_input_schedule t64_input_schedule;

/*
 * Parses and canonicalizes one complete t64.input v1 document. On success,
 * *schedule_out receives a new owned schedule. On refusal, the caller's
 * existing *schedule_out value is unchanged. JSON tick strings are canonical
 * unsigned decimal and events remain in source-array order at equal ticks.
 * These host-side schedules do not run in or call back from core advancement.
 */
t64_input_status t64_input_parse(const uint8_t *bytes, size_t length,
                                 t64_input_schedule **schedule_out,
                                 t64_input_error *error_out);

void t64_input_schedule_destroy(t64_input_schedule *schedule);
size_t t64_input_schedule_event_count(const t64_input_schedule *schedule);
bool t64_input_schedule_event(const t64_input_schedule *schedule,
                              size_t index, t64_input_event *event_out);
/* Returned canonical bytes and digest are borrowed until schedule destruction. */
const uint8_t *t64_input_schedule_canonical_json(
    const t64_input_schedule *schedule, size_t *length_out);
const uint8_t *t64_input_schedule_sha256(
    const t64_input_schedule *schedule);

#endif /* T64_INPUT_H */
