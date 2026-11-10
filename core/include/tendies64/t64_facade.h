/*
 * Tendies64 core facade (PROVISIONAL).
 *
 * This header was one of the two placeholder paths allowed under core/ before
 * ADR-0002 was accepted (CONTEXT A-1). ADR-0002 is accepted; the header stays
 * PROVISIONAL and is NOT an ABI promise: the API freeze is Phase 10. The
 * synthetic runner guest implements it now; the real core implements it in
 * Phase 3.
 *
 * Semantics:
 *  - advance() runs guest time up to and including bound_tick. It returns
 *    T64_STOP_BOUND with tick == bound_tick, or T64_STOP_HALT with the halt
 *    tick.
 *  - drain_guest_log() returns queued guest-log bytes in emission order with
 *    their record tick, and returns 0 when empty.
 *  - No global state: everything lives in the instance pointer.
 *
 * Undefined today (PROVISIONAL; each case must be defined, with its refused-
 * operation effect, before the API freeze; callers must not rely on any of
 * them, and what the synthetic guest happens to do is not a promise):
 *  - advance() with bound_tick below the current tick. The synthetic guest
 *    clamps it to the current tick and reports T64_STOP_BOUND there.
 *  - advance() when the guest halts exactly at bound_tick: whether
 *    T64_STOP_HALT or T64_STOP_BOUND is reported. The synthetic guest reports
 *    T64_STOP_HALT.
 *  - drain_guest_log() with cap smaller than one record: whether a partial
 *    record is returned and which tick *tick_out carries on the next call.
 *    The synthetic guest returns the first cap bytes and the rest on later
 *    calls, each with the record's tick. cap == 0 returns 0, which cannot be
 *    told apart from an empty queue.
 *  - NULL inst, tick_out or buf. Nothing checks them.
 *  - The width of t64_stop_reason: an enum's size is implementation-defined.
 *    A fixed-width integer replaces it in t64_advance_result at the freeze.
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
