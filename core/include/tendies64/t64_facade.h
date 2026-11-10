/*
 * Tendies64 core facade (PROVISIONAL).
 *
 * This header is one of exactly two placeholder paths allowed under core/
 * before ADR-0002 is accepted (CONTEXT A-1). It is NOT an ABI promise: the
 * API freeze is Phase 10. The synthetic runner guest implements it now; the
 * real core implements it in Phase 3.
 *
 * Semantics:
 *  - advance() runs guest time up to and including bound_tick. It returns
 *    T64_STOP_BOUND with tick == bound_tick, or T64_STOP_HALT with the halt
 *    tick.
 *  - drain_guest_log() returns queued guest-log bytes in emission order with
 *    their record tick, and returns 0 when empty.
 *  - No global state: everything lives in the instance pointer.
 */
#ifndef TENDIES64_T64_FACADE_H
#define TENDIES64_T64_FACADE_H

#include <stddef.h>
#include <stdint.h>

typedef enum { T64_STOP_BOUND = 1, T64_STOP_HALT = 2 } t64_stop_reason;

typedef struct {
  t64_stop_reason reason;
  uint64_t tick;
} t64_advance_result;

typedef struct t64_facade_ops {
  t64_advance_result (*advance)(void *inst, uint64_t bound_tick);
  size_t (*drain_guest_log)(void *inst, uint64_t *tick_out, uint8_t *buf,
                            size_t cap);
} t64_facade_ops;

/* Defined by the stub; returns the literal "tendies64-core-stub". */
const char *t64_core_identity(void);

#endif /* TENDIES64_T64_FACADE_H */
