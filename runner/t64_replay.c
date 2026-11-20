/* t64.replay v1 module (plan 09-07). Tracer stub: every function reports an
 * allocation failure or an empty result so the first test compiles; the real
 * implementation replaces this file in the next commit. */
#include "t64_replay.h"

#include <stdlib.h>

const char *t64_replay_status_name(t64_replay_status status) {
  return status == T64_REPLAY_ALLOCATION_FAILED ? "allocation-failed" : "other";
}

t64_replay_status t64_replay_parse(const uint8_t *bytes, size_t length,
                                   t64_replay **replay_out,
                                   t64_replay_error *error_out) {
  (void)bytes;
  (void)length;
  (void)replay_out;
  if (error_out != NULL) {
    error_out->status = T64_REPLAY_ALLOCATION_FAILED;
    error_out->offset = SIZE_MAX;
  }
  return T64_REPLAY_ALLOCATION_FAILED;
}

void t64_replay_destroy(t64_replay *replay) { (void)replay; }

const t64_replay_header *t64_replay_header_get(const t64_replay *replay) {
  (void)replay;
  return NULL;
}

size_t t64_replay_op_count(const t64_replay *replay) {
  (void)replay;
  return 0u;
}

bool t64_replay_op_get(const t64_replay *replay, size_t index,
                       t64_replay_op *op_out) {
  (void)replay;
  (void)index;
  (void)op_out;
  return false;
}

const uint8_t *t64_replay_canonical(const t64_replay *replay,
                                    size_t *length_out) {
  (void)replay;
  if (length_out != NULL) *length_out = 0u;
  return NULL;
}

const uint8_t *t64_replay_sha256(const t64_replay *replay) {
  (void)replay;
  return NULL;
}

t64_replay_status t64_replay_encode(const t64_replay_header *header,
                                    const t64_replay_op *ops, size_t op_count,
                                    uint8_t **bytes_out, size_t *length_out) {
  (void)header;
  (void)ops;
  (void)op_count;
  if (bytes_out != NULL) *bytes_out = NULL;
  if (length_out != NULL) *length_out = 0u;
  return T64_REPLAY_ALLOCATION_FAILED;
}

t64_replay_recorder *t64_replay_recorder_create(
    const t64_replay_header *header, const t64_replay_limits *limits) {
  (void)header;
  (void)limits;
  return NULL;
}

void t64_replay_recorder_destroy(t64_replay_recorder *recorder) {
  (void)recorder;
}

t64_status t64_replay_recorder_queue_input(t64_replay_recorder *recorder,
                                           t64_machine *machine, uint64_t at,
                                           uint64_t event_tick, uint32_t port,
                                           uint16_t buttons, int32_t axis_x,
                                           int32_t axis_y) {
  (void)recorder;
  (void)at;
  return t64_machine_queue_input(machine, event_tick, port, buttons, axis_x,
                                 axis_y);
}

t64_replay_status t64_replay_recorder_checkpoint(
    t64_replay_recorder *recorder, const t64_machine *machine, uint64_t at) {
  (void)recorder;
  (void)machine;
  (void)at;
  return T64_REPLAY_ALLOCATION_FAILED;
}

t64_replay_status t64_replay_recorder_end(t64_replay_recorder *recorder,
                                          const t64_machine *machine,
                                          uint64_t at, t64_replay_stop stop) {
  (void)recorder;
  (void)machine;
  (void)at;
  (void)stop;
  return T64_REPLAY_ALLOCATION_FAILED;
}

t64_replay_status t64_replay_recorder_error(
    const t64_replay_recorder *recorder) {
  (void)recorder;
  return T64_REPLAY_ALLOCATION_FAILED;
}

t64_replay_status t64_replay_recorder_finish(t64_replay_recorder *recorder,
                                             t64_replay **replay_out,
                                             t64_replay_error *error_out) {
  (void)recorder;
  (void)replay_out;
  if (error_out != NULL) {
    error_out->status = T64_REPLAY_ALLOCATION_FAILED;
    error_out->offset = SIZE_MAX;
  }
  return T64_REPLAY_ALLOCATION_FAILED;
}
