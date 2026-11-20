/*
 * Phase 9 CORE-02 ordering matrix (plan 09-06).
 *
 * One authored binary that orders the in-scope devices through the single
 * guest-time scheduler. The scheduler order is tick, then sequence, first in
 * first out (D-02). Every comparison here is either a literal expectation
 * authored independently of the code under test or a comparison of the code
 * with itself across schedules (whole, split at T-1, T and T+1, steps, slices,
 * event budgets, trace on and off). A tie that is consistently wrong in every
 * schedule would pass the self-comparisons; the literal tables and the
 * out-of-tree mutants (scripts/k5-mutants.py) are what make the pins bite.
 * Nothing here is hardware evidence.
 *
 * Modes: --list-cases, --list-ties, --print-sections, --suite matrix,
 * --case NAME, no argument runs every case.
 *
 * Failing comparisons print one stderr line `COMPARATOR <label> case=<name>`
 * so the mutant driver can tell which comparator was reached.
 *
 * Every static helper below is a local copy (test binaries cannot share
 * statics); each carries a provenance comment and none edits its source.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_internal.h"
#include "t64_sha256.h"
#include "t64_test.h"

#define IPL3_ASSET_BYTES 4096u
#define FAMILY_STEM "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1"
#define AI_PROFILE "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1"

#define VI_BASE UINT32_C(0x04400000)
#define AI_BASE UINT32_C(0x04500000)
#define AI_DRAM_ADDR (AI_BASE + UINT32_C(0x00))
#define AI_LEN (AI_BASE + UINT32_C(0x04))
#define AI_CONTROL (AI_BASE + UINT32_C(0x08))
#define AI_DACRATE (AI_BASE + UINT32_C(0x10))
#define AI_BITRATE (AI_BASE + UINT32_C(0x14))

#define CALL_CAP 200000u
#define QUEUE_SNAPSHOT_CAPACITY T64_EVENT_QUEUE_CAPACITY
#define LOG_CAPACITY 64u

/* ---- comparator labels ---------------------------------------------------- */

static const char *current_case = "none";

/* One stderr line per failing comparison, then the failure is counted. The
 * mutant driver reads the line to prove the intended comparator was reached. */
static void comparator_fail(const char *label) {
  fprintf(stderr, "COMPARATOR %s case=%s\n", label, current_case);
  T64_CHECK(false);
}

/* ---- allocator, asset and machine creation -------------------------------- */

/* Provenance: core/tests/test_sched_ledger.c (plan 09-01). */
static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  void *memory;

  (void)context;
  if (storage_out == NULL || size == 0u || alignment == 0u) return false;
  memory = malloc(size);
  if (memory == NULL || (uintptr_t)memory % alignment != 0u) {
    free(memory);
    return false;
  }
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  return true;
}

static void test_release(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static const t64_host_allocator allocator = {test_allocate, test_release,
                                             NULL};

/* Provenance: core/tests/test_sched_ledger.c read_openipl3_asset. */
static bool read_openipl3_asset(uint8_t bytes[IPL3_ASSET_BYTES]) {
  const char *path = getenv("T64_TEST_IPL3_PATH");
  FILE *file;
  size_t length;
  int extra;
  int read_error;
  int close_error;

  if (path == NULL || path[0] == '\0' || bytes == NULL) return false;
  file = fopen(path, "rb");
  if (file == NULL) return false;
  length = fread(bytes, 1u, IPL3_ASSET_BYTES, file);
  extra = fgetc(file);
  read_error = ferror(file);
  close_error = fclose(file);
  return read_error == 0 && close_error == 0 && length == IPL3_ASSET_BYTES &&
         extra == EOF;
}

/* A created machine with the cleared openIPL3 asset admitted. */
static t64_machine *create_machine(const char *profile_id) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t asset[IPL3_ASSET_BYTES];

  T64_CHECK_INT(t64_profile_lookup(profile_id, &profile), T64_STATUS_OK);
  if (profile == NULL) return NULL;
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  if (machine == NULL) return NULL;
  if (!read_openipl3_asset(asset)) {
    T64_CHECK(false);
    t64_machine_destroy(machine);
    return NULL;
  }
  T64_CHECK_INT(t64_machine_admit_rom(machine, asset, sizeof asset),
                T64_STATUS_OK);
  return machine;
}

/* Provenance: core/tests/test_sched_ledger.c cancel_boot_cpu_event. */
static bool cancel_boot_cpu_event(t64_machine *machine) {
  uint64_t cancelled_tick = UINT64_MAX;
  t64_status status = t64_schedule_cancel(
      machine, T64_DEVICE_CPU, T64_CPU_EVENT_STEP, 0u, &cancelled_tick);

  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(cancelled_tick, 1u);
  return status == T64_STATUS_OK && cancelled_tick == 1u;
}

static bool write32(t64_machine *machine, uint32_t address, uint32_t value) {
  t64_status status = t64_machine_bus_write(machine, address, 4u, value);

  T64_CHECK_INT(status, T64_STATUS_OK);
  return status == T64_STATUS_OK;
}

/* ---- expected order and its comparator ------------------------------------ */

typedef struct {
  uint32_t device;
  uint32_t kind;
} order_pair;

typedef enum { ORDER_OK = 0, ORDER_LENGTH = 1, ORDER_MISMATCH = 2 } order_kind;

/* Returns a kind, never fails a check, so a control can assert the kind. */
static order_kind compare_order(const order_pair *expected,
                                size_t expected_count,
                                const order_pair *actual,
                                size_t actual_count) {
  size_t i;

  if (expected_count != actual_count) return ORDER_LENGTH;
  for (i = 0u; i < expected_count; i++) {
    if (expected[i].device != actual[i].device ||
        expected[i].kind != actual[i].kind) {
      return ORDER_MISMATCH;
    }
  }
  return ORDER_OK;
}

/* ---- run context: advance with resume, trace drain and trace hash --------- */

typedef struct {
  t64_machine *machine;
  bool trace;                  /* drain after every advance call */
  t64_sha256_ctx trace_hash;
  uint64_t trace_count;
  uint64_t trace_dropped;
  uint64_t log_tick;           /* record the records at this tick, or 0 */
  t64_trace_record log[LOG_CAPACITY];
  size_t log_count;
  uint64_t calls;
  uint64_t resource_stops;
  uint32_t last_reason;
  uint64_t last_tick;
} run_ctx;

static void ctx_start(run_ctx *ctx, t64_machine *machine, bool trace,
                      uint64_t log_tick) {
  (void)memset(ctx, 0, sizeof *ctx);
  ctx->machine = machine;
  ctx->trace = trace;
  ctx->log_tick = log_tick;
  t64_sha256_init(&ctx->trace_hash);
  if (trace) {
    T64_CHECK_INT(t64_machine_trace_set_enabled(machine, true),
                  T64_STATUS_OK);
  }
}

static void hash_u64(t64_sha256_ctx *hash, uint64_t value) {
  uint8_t bytes[8];
  unsigned shift;

  for (shift = 0u; shift < 8u; shift++) {
    bytes[shift] = (uint8_t)(value >> (56u - 8u * shift));
  }
  t64_sha256_update(hash, bytes, sizeof bytes);
}

/* Drain every queued record into the per-run hash: (tick, device, kind,
 * value) of each record in order. A drop is a defect of the schedule, not of
 * the machine, and is checked by the caller. */
static void ctx_drain(run_ctx *ctx) {
  t64_trace_record records[T64_TRACE_RECORD_CAPACITY];
  size_t count = 0u;
  uint64_t dropped = 0u;
  bool saturated = false;
  size_t i;
  t64_status status;

  if (!ctx->trace) return;
  status = t64_machine_trace_drain(ctx->machine, records,
                                   T64_TRACE_RECORD_CAPACITY, &count,
                                   &dropped, &saturated);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) return;
  for (i = 0u; i < count; i++) {
    hash_u64(&ctx->trace_hash, records[i].tick);
    hash_u64(&ctx->trace_hash, (uint64_t)records[i].device);
    hash_u64(&ctx->trace_hash, (uint64_t)records[i].kind);
    hash_u64(&ctx->trace_hash, records[i].value);
    if (ctx->log_tick != 0u && records[i].tick == ctx->log_tick &&
        ctx->log_count < LOG_CAPACITY) {
      ctx->log[ctx->log_count++] = records[i];
    }
  }
  ctx->trace_count += count;
  ctx->trace_dropped = dropped;
  T64_CHECK(!saturated);
}

/* What counts as progress between two stops of one resumed advance. */
typedef struct {
  uint64_t tick;
  uint64_t next_sequence;
  size_t queue_count;
  uint64_t front_sequence;
  uint64_t front_tick;
} progress_mark;

static progress_mark mark_of(const t64_machine *machine) {
  progress_mark mark;
  const t64_event *front = machine->queue_count > 0u
                               ? &machine->events[machine->queue_head]
                               : NULL;

  mark.tick = machine->tick;
  mark.next_sequence = machine->next_sequence;
  mark.queue_count = machine->queue_count;
  mark.front_sequence = front != NULL ? front->sequence : UINT64_MAX;
  mark.front_tick = front != NULL ? front->tick : UINT64_MAX;
  return mark;
}

static bool mark_equal(const progress_mark *a, const progress_mark *b) {
  return a->tick == b->tick && a->next_sequence == b->next_sequence &&
         a->queue_count == b->queue_count &&
         a->front_sequence == b->front_sequence &&
         a->front_tick == b->front_tick;
}

/*
 * Advance to an absolute guest tick. A resource-limit result is resumable:
 * the call repeats under a call cap, and every repeat must have made progress
 * (the tick or the queue changed) or the case fails with resume-liveness.
 */
static bool ctx_advance_to(run_ctx *ctx, uint64_t tick) {
  size_t calls = 0u;

  for (;;) {
    t64_stop_request request = {T64_STOP_GUEST_TICK, tick};
    t64_stop_result stop = {0u, 0u};
    progress_mark before = mark_of(ctx->machine);
    t64_status status = t64_machine_advance(ctx->machine, &request, &stop);

    ctx->calls++;
    T64_CHECK_INT(status, T64_STATUS_OK);
    if (status != T64_STATUS_OK) return false;
    ctx_drain(ctx);
    ctx->last_reason = stop.reason;
    ctx->last_tick = stop.tick;
    if (stop.reason == T64_STOP_REASON_BOUND) {
      T64_CHECK_INT(stop.tick, tick);
      return stop.tick == tick;
    }
    if (stop.reason != T64_STOP_REASON_RESOURCE_LIMIT) {
      fprintf(stderr, "unexpected stop reason %u at tick %llu\n",
              (unsigned)stop.reason, (unsigned long long)stop.tick);
      comparator_fail("stop-result");
      return false;
    }
    ctx->resource_stops++;
    T64_CHECK_INT(stop.tick, ctx->machine->tick);
    {
      progress_mark after = mark_of(ctx->machine);

      if (mark_equal(&before, &after)) {
        comparator_fail("resume-liveness");
        return false;
      }
    }
    if (++calls >= CALL_CAP) {
      comparator_fail("resume-liveness");
      return false;
    }
  }
}

/* ---- observation: what two runs of one scenario are compared on ----------- */

typedef struct {
  uint64_t tick;
  uint64_t sequence;
  uint32_t device;
  uint32_t kind;
  uint64_t value;
  uint64_t token;
} queue_entry;

typedef struct {
  uint8_t digest[T64_DIGEST_SIZE];
  queue_entry queue[QUEUE_SNAPSHOT_CAPACITY];
  size_t queue_count;
  uint64_t next_sequence;
  uint64_t tick;
  uint32_t stop_reason;
  uint64_t stop_tick;
  uint8_t trace_hash[T64_SHA256_DIGEST_SIZE];
  uint64_t trace_count;
  uint64_t trace_dropped;
  bool trace;
} observation;

static void observe(run_ctx *ctx, observation *out) {
  const t64_machine *machine = ctx->machine;
  size_t i;

  (void)memset(out, 0, sizeof *out);
  T64_CHECK_INT(t64_machine_digest(machine, out->digest), T64_STATUS_OK);
  out->queue_count = machine->queue_count;
  for (i = 0u; i < machine->queue_count; i++) {
    const t64_event *event =
        &machine->events[(machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];

    out->queue[i].tick = event->tick;
    out->queue[i].sequence = event->sequence;
    out->queue[i].device = event->device;
    out->queue[i].kind = event->kind;
    out->queue[i].value = event->value;
    out->queue[i].token = event->token;
  }
  out->next_sequence = machine->next_sequence;
  out->tick = machine->tick;
  out->stop_reason = ctx->last_reason;
  out->stop_tick = ctx->last_tick;
  out->trace = ctx->trace;
  if (ctx->trace) {
    t64_sha256_ctx hash = ctx->trace_hash;

    t64_sha256_final(&hash, out->trace_hash);
    out->trace_count = ctx->trace_count;
    out->trace_dropped = ctx->trace_dropped;
  }
}

/* The first differing comparator label between two observations, or NULL.
 * The trace is compared only when both runs carried it. */
static const char *compare_observation(const observation *a,
                                       const observation *b) {
  size_t i;

  if (memcmp(a->digest, b->digest, sizeof a->digest) != 0) {
    return "digest-schedule";
  }
  if (a->queue_count != b->queue_count || a->next_sequence != b->next_sequence ||
      a->tick != b->tick) {
    return "queue-snapshot";
  }
  for (i = 0u; i < a->queue_count; i++) {
    if (a->queue[i].tick != b->queue[i].tick ||
        a->queue[i].sequence != b->queue[i].sequence ||
        a->queue[i].device != b->queue[i].device ||
        a->queue[i].kind != b->queue[i].kind ||
        a->queue[i].value != b->queue[i].value ||
        a->queue[i].token != b->queue[i].token) {
      return "queue-snapshot";
    }
  }
  if (a->trace && b->trace &&
      (a->trace_count != b->trace_count ||
       memcmp(a->trace_hash, b->trace_hash, sizeof a->trace_hash) != 0)) {
    return "trace-order";
  }
  if (a->stop_reason != b->stop_reason || a->stop_tick != b->stop_tick) {
    return "stop-result";
  }
  return NULL;
}

/* ---- schedules ------------------------------------------------------------ */

typedef enum {
  SCHEDULE_WHOLE,   /* one request to the horizon */
  SCHEDULE_SPLIT3,  /* stops at t - 1, t and t + 1, then the horizon */
  SCHEDULE_STEPS,   /* steps of `arg` ticks */
  SCHEDULE_LEAD     /* whole to `arg`, then one-tick slices to the horizon */
} schedule_kind;

typedef struct {
  schedule_kind kind;
  uint64_t arg;
  const char *name;
} schedule;

/* The schedule family of the matrix: whole, SPLIT3(T), STEPS 1, 3, 7 and
 * 4,096 ticks, and one-tick slices from T - 3. */
static schedule schedule_whole(void) {
  schedule value = {SCHEDULE_WHOLE, 0u, "whole"};

  return value;
}

static schedule schedule_split3(uint64_t t) {
  schedule value = {SCHEDULE_SPLIT3, t, "split3"};

  return value;
}

static schedule schedule_steps(uint64_t step) {
  schedule value = {SCHEDULE_STEPS, step, "steps"};

  return value;
}

static schedule schedule_lead(uint64_t from) {
  schedule value = {SCHEDULE_LEAD, from, "lead"};

  return value;
}

static bool run_schedule(run_ctx *ctx, const schedule *plan,
                         uint64_t horizon) {
  t64_machine *machine = ctx->machine;

  switch (plan->kind) {
  case SCHEDULE_WHOLE:
    return ctx_advance_to(ctx, horizon);
  case SCHEDULE_SPLIT3: {
    uint64_t offset;

    for (offset = 0u; offset < 3u; offset++) {
      const uint64_t stop = plan->arg - 1u + offset;

      if (stop >= machine->tick && stop <= horizon) {
        if (!ctx_advance_to(ctx, stop)) return false;
      }
    }
    return ctx_advance_to(ctx, horizon);
  }
  case SCHEDULE_STEPS:
    while (machine->tick < horizon) {
      uint64_t next = machine->tick + plan->arg;

      if (next < machine->tick || next > horizon) next = horizon;
      if (!ctx_advance_to(ctx, next)) return false;
    }
    return ctx_advance_to(ctx, horizon);
  case SCHEDULE_LEAD:
    if (plan->arg > machine->tick && plan->arg <= horizon) {
      if (!ctx_advance_to(ctx, plan->arg)) return false;
    }
    while (machine->tick < horizon) {
      if (!ctx_advance_to(ctx, machine->tick + 1u)) return false;
    }
    return ctx_advance_to(ctx, horizon);
  }
  return false;
}

/* ---- pair table ----------------------------------------------------------- */

#define MAX_ORDER 4u

typedef struct pair_row pair_row;

/* A builder returns a machine with both events queued at one tick (the tied
 * tick is read back from the queue and returned), the boot CPU event
 * cancelled where the row says so, and the trace disabled. */
typedef t64_machine *(*pair_builder)(const pair_row *row, int order,
                                     uint64_t *tied_tick_out);

struct pair_row {
  const char *name;       /* PAIR-* scenario name */
  const char *tie_label;  /* tie.* label, owner case cross-device or tracer */
  const char *profile;
  pair_builder build;
  uint64_t expected_tick; /* independent literal for the tied tick, 0 = none */
  /* Expected dispatch order of the watched pairs at the tied tick, per
   * insertion order (0 first inserted, 1 second inserted). */
  order_pair expected[2][MAX_ORDER];
  size_t expected_count[2];
};

/* First record index at `tick` carrying the pair, or SIZE_MAX. */
static size_t first_log_index(const t64_trace_record *log, size_t count,
                              uint64_t tick, order_pair pair) {
  size_t i;

  for (i = 0u; i < count; i++) {
    if (log[i].tick == tick && log[i].device == pair.device &&
        log[i].kind == pair.kind) {
      return i;
    }
  }
  return SIZE_MAX;
}

/* The watched pairs in the order of their first trace record at the tied
 * tick; pairs with no record are omitted (the length comparison reports it). */
static size_t extract_order(const t64_trace_record *log, size_t count,
                            uint64_t tick, const order_pair *watch,
                            size_t watch_count, order_pair *out) {
  size_t index[MAX_ORDER];
  size_t found = 0u;
  size_t i;
  size_t j;

  for (i = 0u; i < watch_count && i < MAX_ORDER; i++) {
    const size_t at = first_log_index(log, count, tick, watch[i]);

    if (at == SIZE_MAX) continue;
    /* Insertion sort by trace index. */
    for (j = found; j > 0u && index[j - 1u] > at; j--) {
      index[j] = index[j - 1u];
      out[j] = out[j - 1u];
    }
    index[j] = at;
    out[j] = watch[i];
    found++;
  }
  return found;
}

/* Reached: the two watched events are queued at the tied tick and the one
 * the row says was inserted first has the smaller sequence. */
static bool queued_before(const t64_machine *machine, uint64_t tick,
                          order_pair first, order_pair second) {
  uint64_t first_sequence = UINT64_MAX;
  uint64_t second_sequence = UINT64_MAX;
  size_t i;

  for (i = 0u; i < machine->queue_count; i++) {
    const t64_event *event =
        &machine->events[(machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];

    if (event->tick != tick) continue;
    if (event->device == first.device && event->kind == first.kind) {
      first_sequence = event->sequence;
    }
    if (event->device == second.device && event->kind == second.kind) {
      second_sequence = event->sequence;
    }
  }
  return first_sequence != UINT64_MAX && second_sequence != UINT64_MAX &&
         first_sequence < second_sequence;
}

/* One schedule of one pair row and order. Returns the observation; the trace
 * variant also reports the order extracted at the tied tick. */
static bool run_pair_once(const pair_row *row, int order,
                          const schedule *plan, bool trace,
                          observation *obs_out, order_pair *order_out,
                          size_t *order_count_out) {
  uint64_t tied = 0u;
  t64_machine *machine = row->build(row, order, &tied);
  run_ctx ctx;
  bool ok;

  if (machine == NULL) return false;
  ctx_start(&ctx, machine, trace, tied);
  ok = run_schedule(&ctx, plan, tied + 1u);
  observe(&ctx, obs_out);
  if (trace) {
    T64_CHECK_INT(ctx.trace_dropped, 0u);
    if (order_out != NULL) {
      *order_count_out = extract_order(ctx.log, ctx.log_count, tied,
                                       row->expected[0], row->expected_count[0],
                                       order_out);
    }
  }
  t64_machine_destroy(machine);
  return ok;
}

static void check_order_literal(const pair_row *row, int order,
                                const order_pair *actual,
                                size_t actual_count) {
  const order_kind kind = compare_order(row->expected[order],
                                        row->expected_count[order], actual,
                                        actual_count);

  if (kind != ORDER_OK) {
    fprintf(stderr, "%s order %d: dispatch order differs from the literal (%s)\n",
            row->name, order, kind == ORDER_LENGTH ? "length" : "mismatch");
    comparator_fail("trace-order");
  }
}

/* Both insertion orders, every schedule, trace on and off. The reference is
 * the whole advance with the trace off; every other run must equal it, and
 * every trace-on run must carry the same trace hash as the whole trace-on run
 * and the literal dispatch order. */
static void check_pair_row(const pair_row *row, uint64_t tied_literal) {
  int order;

  for (order = 0; order < 2; order++) {
    static observation reference;
    static observation run;
    static observation trace_reference;
    order_pair actual[MAX_ORDER];
    size_t actual_count = 0u;
    uint64_t tied = 0u;
    schedule plans[8];
    size_t plan_count = 0u;
    size_t i;
    schedule whole = schedule_whole();

    /* Learn the tied tick (reached assertion inside the builder). */
    {
      t64_machine *probe = row->build(row, order, &tied);

      if (probe == NULL) return;
      t64_machine_destroy(probe);
    }
    if (tied_literal != 0u) T64_CHECK_INT(tied, tied_literal);
    plans[plan_count++] = schedule_split3(tied);
    plans[plan_count++] = schedule_lead(tied >= 3u ? tied - 3u : 0u);
    plans[plan_count++] = schedule_steps(1u);
    plans[plan_count++] = schedule_steps(3u);
    plans[plan_count++] = schedule_steps(7u);
    plans[plan_count++] = schedule_steps(4096u);

    /* Reference: whole, trace off. */
    if (!run_pair_once(row, order, &whole, false, &reference, NULL, NULL)) {
      return;
    }
    /* Whole, trace on: the digest equals the trace-off reference (trace is
     * observation only), the order equals the literal. */
    if (!run_pair_once(row, order, &whole, true, &trace_reference, actual,
                       &actual_count)) {
      return;
    }
    {
      const char *label = compare_observation(&reference, &trace_reference);

      if (label != NULL) {
        fprintf(stderr, "%s order %d: trace on differs from trace off\n",
                row->name, order);
        comparator_fail(label);
      }
    }
    check_order_literal(row, order, actual, actual_count);
    /* Transposed-literal control, after the accepted baseline in this setup:
     * the swapped expectation must report the mismatch kind. */
    if (row->expected_count[order] == 2u) {
      order_pair swapped[2];

      swapped[0] = row->expected[order][1];
      swapped[1] = row->expected[order][0];
      T64_CHECK_INT(compare_order(swapped, 2u, actual, actual_count),
                    ORDER_MISMATCH);
      T64_CHECK_INT(compare_order(row->expected[order], 1u, actual,
                                  actual_count),
                    ORDER_LENGTH);
    }
    for (i = 0u; i < plan_count; i++) {
      const char *label;

      if (!run_pair_once(row, order, &plans[i], false, &run, NULL, NULL)) {
        return;
      }
      label = compare_observation(&reference, &run);
      if (label != NULL) {
        fprintf(stderr, "%s order %d schedule %s %llu (trace off)\n", row->name,
                order, plans[i].name, (unsigned long long)plans[i].arg);
        comparator_fail(label);
      }
      if (!run_pair_once(row, order, &plans[i], true, &run, actual,
                         &actual_count)) {
        return;
      }
      label = compare_observation(&trace_reference, &run);
      if (label == NULL) label = compare_observation(&reference, &run);
      if (label != NULL) {
        fprintf(stderr, "%s order %d schedule %s %llu (trace on)\n", row->name,
                order, plans[i].name, (unsigned long long)plans[i].arg);
        comparator_fail(label);
      }
      check_order_literal(row, order, actual, actual_count);
    }
  }
}

/* ---- tracer: VI field and AI sample at one tick --------------------------- */

/* Provenance: core/tests/test_ai.c configure_ai_rate_pair. */
static bool configure_ai_rate_pair(t64_machine *machine) {
  return write32(machine, AI_DACRATE, 1103u) &&
         write32(machine, AI_BITRATE, 15u);
}

/* Provenance: core/tests/test_ai.c submit_ai_frames (two stereo frames,
 * eight bytes, at RDRAM 0x100). */
static bool submit_ai_two_frames(t64_machine *machine) {
  return write32(machine, AI_DRAM_ADDR, UINT32_C(0x100)) &&
         write32(machine, AI_LEN, 8u);
}

/* Provenance: core/tests/test_ai.c ai_profile_vi_write and
 * configure_ai_profile_vi_sync with v_sync 8 and h_sync 2207. */
static bool configure_vi(t64_machine *machine) {
  size_t pixel;

  for (pixel = 0u; pixel < 12u; pixel++) {
    t64_status status = t64_machine_bus_write(
        machine, UINT32_C(0x3000) + (uint32_t)(pixel * 2u), 2u,
        UINT32_C(0xf801));

    T64_CHECK_INT(status, T64_STATUS_OK);
    if (status != T64_STATUS_OK) return false;
  }
  return write32(machine, VI_BASE + 0x04u, UINT32_C(0x3000)) &&
         write32(machine, VI_BASE + 0x08u, UINT32_C(4)) &&
         write32(machine, VI_BASE + 0x0cu, UINT32_C(2)) &&
         write32(machine, VI_BASE + 0x14u, UINT32_C(0)) &&
         write32(machine, VI_BASE + 0x18u, UINT32_C(8)) &&
         write32(machine, VI_BASE + 0x1cu, UINT32_C(2207)) &&
         write32(machine, VI_BASE + 0x24u, UINT32_C(4)) &&
         write32(machine, VI_BASE + 0x28u, UINT32_C(0x00020008)) &&
         write32(machine, VI_BASE + 0x30u, UINT32_C(0x00000400)) &&
         write32(machine, VI_BASE + 0x34u, UINT32_C(0x00000400)) &&
         write32(machine, VI_BASE + 0x00u, UINT32_C(2));
}

/* Tick of the first queued event of a pair, or false. */
static bool queued_tick_of(const t64_machine *machine, uint32_t device,
                           uint32_t kind, uint64_t *tick_out) {
  size_t i;

  for (i = 0u; i < machine->queue_count; i++) {
    const t64_event *event =
        &machine->events[(machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];

    if (event->device == device && event->kind == kind) {
      *tick_out = event->tick;
      return true;
    }
  }
  return false;
}

/*
 * Independent literal for the tied tick: one sample at DACRATE 1103 lasts
 * ceil(62,500,000 * 1,104 / 48,681,812) = ceil(1417.36...) = 1,418 guest
 * ticks (69,000,000,000 divided by 48,681,812 leaves 17,872,396 after 1,417),
 * and the VI field of the registers below falls on the same tick.
 */
#define TRACER_TIED_TICK UINT64_C(1418)

/* Insertion order 0 starts the AI first, order 1 starts the VI first. The
 * register and DMA sequences are those of core/tests/test_ai.c
 * run_vi_ai_tie_order. */
static t64_machine *build_vi_ai(const pair_row *row, int order,
                                uint64_t *tied_tick_out) {
  t64_machine *machine = create_machine(row->profile);
  uint64_t ai_tick = UINT64_MAX;
  uint64_t vi_tick = UINT64_MAX;
  order_pair ai = {T64_AI_DEVICE, T64_AI_EVENT_SAMPLE};
  order_pair vi = {T64_VI_DEVICE, T64_VI_EVENT_FIELD};
  bool ok;

  if (machine == NULL) return NULL;
  ok = configure_ai_rate_pair(machine) && cancel_boot_cpu_event(machine);
  if (ok && order == 0) {
    ok = write32(machine, AI_CONTROL, 1u) && submit_ai_two_frames(machine) &&
         configure_vi(machine);
  } else if (ok) {
    ok = configure_vi(machine) && write32(machine, AI_CONTROL, 1u) &&
         submit_ai_two_frames(machine);
  }
  if (!ok) {
    t64_machine_destroy(machine);
    return NULL;
  }
  /* Reached: both events are queued at one tick, the first inserted first. */
  T64_CHECK(queued_tick_of(machine, ai.device, ai.kind, &ai_tick));
  T64_CHECK(queued_tick_of(machine, vi.device, vi.kind, &vi_tick));
  T64_CHECK_INT(ai_tick, vi_tick);
  T64_CHECK(order == 0 ? queued_before(machine, ai_tick, ai, vi)
                       : queued_before(machine, ai_tick, vi, ai));
  *tied_tick_out = ai_tick;
  return machine;
}

static const pair_row tracer_rows[2] = {
    {"PAIR-VI-AI-STEM", "tie.vi-ai-tracer", FAMILY_STEM, build_vi_ai,
     TRACER_TIED_TICK,
     {{{T64_AI_DEVICE, T64_AI_EVENT_SAMPLE}, {T64_VI_DEVICE, T64_VI_EVENT_FIELD}},
      {{T64_VI_DEVICE, T64_VI_EVENT_FIELD}, {T64_AI_DEVICE, T64_AI_EVENT_SAMPLE}}},
     {2u, 2u}},
    {"PAIR-VI-AI-PROFILE", "tie.vi-ai-tracer", AI_PROFILE, build_vi_ai,
     TRACER_TIED_TICK,
     {{{T64_AI_DEVICE, T64_AI_EVENT_SAMPLE}, {T64_VI_DEVICE, T64_VI_EVENT_FIELD}},
      {{T64_VI_DEVICE, T64_VI_EVENT_FIELD}, {T64_AI_DEVICE, T64_AI_EVENT_SAMPLE}}},
     {2u, 2u}},
};

/*
 * Case tracer-coincident: a VI field and an AI sample queued at one tick, in
 * each insertion order, dispatch in the literal order and give the same digest,
 * queue snapshot and trace across whole, split and sliced advances. The
 * results are regression pins and self-comparisons; a consistently wrong tie
 * would pass (D-36).
 */
static void case_tracer_coincident(void) {
  size_t row;

  for (row = 0u; row < sizeof tracer_rows / sizeof tracer_rows[0]; row++) {
    check_pair_row(&tracer_rows[row], tracer_rows[row].expected_tick);
  }
}

/* ---- case table and entry point ------------------------------------------- */

typedef struct {
  const char *name;
  void (*run)(void);
} matrix_case;

static const matrix_case cases[] = {
    {"tracer-coincident", case_tracer_coincident},
};
#define CASE_COUNT (sizeof cases / sizeof cases[0])

/* `label owner-case` per tie label. */
static const char *const ties[][2] = {
    {"tie.vi-ai-tracer", "tracer-coincident"},
};
#define TIE_COUNT (sizeof ties / sizeof ties[0])

static void run_case(const matrix_case *entry) {
  current_case = entry->name;
  entry->run();
  current_case = "none";
}

int main(int argc, char **argv) {
  size_t i;

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (i = 0u; i < CASE_COUNT; i++) printf("%s\n", cases[i].name);
    return 0;
  }
  if (argc == 2 && strcmp(argv[1], "--list-ties") == 0) {
    for (i = 0u; i < TIE_COUNT; i++) printf("%s %s\n", ties[i][0], ties[i][1]);
    return 0;
  }
  if (argc == 2 && strcmp(argv[1], "--print-sections") == 0) {
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    for (i = 0u; i < CASE_COUNT; i++) {
      if (strcmp(argv[2], cases[i].name) == 0) {
        run_case(&cases[i]);
        return T64_TEST_FINISH("unit.core.sched.matrix");
      }
    }
    fprintf(stderr, "unknown case: %s\n", argv[2]);
    return 2;
  }
  if (argc == 3 && strcmp(argv[1], "--suite") == 0 &&
      strcmp(argv[2], "matrix") == 0) {
    for (i = 0u; i < CASE_COUNT; i++) run_case(&cases[i]);
    return T64_TEST_FINISH("unit.core.sched.matrix");
  }
  if (argc != 1) {
    fprintf(stderr,
            "usage: %s [--list-cases|--list-ties|--print-sections|"
            "--suite matrix|--case NAME]\n",
            argv[0]);
    return 2;
  }
  for (i = 0u; i < CASE_COUNT; i++) run_case(&cases[i]);
  return T64_TEST_FINISH("unit.core.sched.matrix");
}
