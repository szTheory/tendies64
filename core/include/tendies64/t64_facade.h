/*
 * Tendies64 synthetic-guest runner header (PROVISIONAL, in-tree only).
 *
 * This header was one of the two placeholder paths allowed under core/ before
 * ADR-0002 was accepted (CONTEXT A-1). It declares the facade the synthetic
 * runner guest implements. It is not installed, is not part of the ABI and is
 * not in the freeze record: the public C API is tendies64/t64_machine.h, a 0.x
 * freeze candidate bound by Phase 11 (final ABI bind at the end of Phase 12).
 *
 * Semantics:
 *  - advance() runs guest time up to and including bound_tick. It returns
 *    T64_STOP_BOUND with tick == bound_tick, or T64_STOP_HALT with the halt
 *    tick.
 *  - drain_guest_log() returns queued guest-log bytes in emission order with
 *    their record tick, and returns 0 when empty.
 *  - No global state: everything lives in the instance pointer.
 *
 * Undefined (PROVISIONAL; this header is outside the freeze record, so no case
 * is defined here; callers must not rely on any of them, and what the
 * synthetic guest happens to do is not a promise):
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
 *    The public header uses fixed-width integers instead.
 */
#ifndef TENDIES64_T64_FACADE_H
#define TENDIES64_T64_FACADE_H

#include <stddef.h>
#include <stdint.h>

/* Renames t64_core_identity to t64i_core_identity (DR-17); the stub defines
 * and the runner calls the same renamed symbol. */
#include "t64_prefix.h"

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
