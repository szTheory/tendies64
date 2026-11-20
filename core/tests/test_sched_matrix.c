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
#define AUTHORED_ROM_BYTES 4096u
#define SKELETON_PROFILE "ntsc-cart-4m-skeleton"
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

/* Authored 4 KiB image the skeleton profile accepts. Provenance:
 * core/tests/test_sched_ledger.c fill_authored_rom. */
static void fill_authored_rom(uint8_t image[AUTHORED_ROM_BYTES]) {
  size_t i;

  for (i = 0u; i < AUTHORED_ROM_BYTES; i++) {
    image[i] = (uint8_t)((i * 13u + 7u) & 0xffu);
  }
  image[0] = 0x80u;
  image[1] = 0x37u;
  image[2] = 0x12u;
  image[3] = 0x40u;
  image[0x3eu] = (uint8_t)'E';
}

/* A created machine with an image admitted: the cleared openIPL3 asset for
 * the family and Phase 8 profiles, an authored flat image for the skeleton
 * profile (the legacy SP DMA completion event exists only there). */
static t64_machine *create_machine(const char *profile_id) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t asset[IPL3_ASSET_BYTES];

  T64_CHECK_INT(t64_profile_lookup(profile_id, &profile), T64_STATUS_OK);
  if (profile == NULL) return NULL;
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  if (machine == NULL) return NULL;
  if (strcmp(profile_id, SKELETON_PROFILE) == 0) {
    fill_authored_rom(asset);
  } else if (!read_openipl3_asset(asset)) {
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
  uint64_t find_address;       /* first bus write32 to this address, or 0 */
  uint64_t found_tick;         /* tick of that record, or UINT64_MAX */
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
  ctx->found_tick = UINT64_MAX;
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
    if (ctx->find_address != 0u && ctx->found_tick == UINT64_MAX &&
        records[i].device == T64_TRACE_DEVICE_BUS &&
        records[i].kind == T64_TRACE_KIND_BUS_WRITE32 &&
        (records[i].value >> 32) == ctx->find_address) {
      ctx->found_tick = records[i].tick;
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
 * tick is read back from the queue and returned), the trace disabled and no
 * guest time advanced beyond the setup the row needs. */
typedef t64_machine *(*pair_builder)(const pair_row *row, int order,
                                     uint64_t *tied_tick_out);

/* Called after every run of the row at its horizon, for the row's own
 * postcondition (destination bytes and the like). */
typedef void (*pair_post)(const pair_row *row, int order,
                          t64_machine *machine);

struct pair_row {
  const char *name;       /* PAIR-* scenario name */
  const char *tie_label;  /* tie.* label */
  const char *profile;    /* profile id or SKELETON_PROFILE */
  pair_builder build;
  int variant;            /* builder selector */
  uint64_t expected_tick; /* independent literal for the tied tick */
  uint64_t horizon_extra; /* horizon = tied tick + horizon_extra */
  uint64_t extra_split;   /* a second SPLIT3 centre, or 0 */
  /* Expected dispatch order of the watched pairs at the tied tick, per
   * insertion order (0 first inserted, 1 second inserted). */
  order_pair expected[2][MAX_ORDER];
  size_t expected_count[2];
  size_t order_count;     /* insertion orders the row has (1 or 2) */
  pair_post post;
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
  ok = run_schedule(&ctx, plan, tied + row->horizon_extra);
  observe(&ctx, obs_out);
  if (ok && row->post != NULL) row->post(row, order, machine);
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
static void check_pair_row(const pair_row *row) {
  size_t order_index;

  for (order_index = 0u; order_index < row->order_count; order_index++) {
    const int order = (int)order_index;

    static observation reference;
    static observation run;
    static observation trace_reference;
    order_pair actual[MAX_ORDER];
    size_t actual_count = 0u;
    uint64_t tied = 0u;
    schedule plans[9];
    size_t plan_count = 0u;
    size_t i;
    schedule whole = schedule_whole();

    /* Learn the tied tick (reached assertion inside the builder). */
    {
      t64_machine *probe = row->build(row, order, &tied);

      if (probe == NULL) return;
      t64_machine_destroy(probe);
    }
    if (row->expected_tick != 0u) T64_CHECK_INT(tied, row->expected_tick);
    plans[plan_count++] = schedule_split3(tied);
    if (row->extra_split != 0u) {
      plans[plan_count++] = schedule_split3(row->extra_split);
    }
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
     * observation only), the order equals the literal. The one-tick and
     * three-tick step schedules drain after every call, so a long row keeps
     * its trace records within the drain capacity. */
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
 * configure_ai_profile_vi_sync with h_sync 2207. */
static bool configure_vi_sync(t64_machine *machine, uint32_t v_sync) {
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
         write32(machine, VI_BASE + 0x18u, v_sync) &&
         write32(machine, VI_BASE + 0x1cu, UINT32_C(2207)) &&
         write32(machine, VI_BASE + 0x24u, UINT32_C(4)) &&
         write32(machine, VI_BASE + 0x28u, UINT32_C(0x00020008)) &&
         write32(machine, VI_BASE + 0x30u, UINT32_C(0x00000400)) &&
         write32(machine, VI_BASE + 0x34u, UINT32_C(0x00000400)) &&
         write32(machine, VI_BASE + 0x00u, UINT32_C(2));
}

/* The tracer uses v_sync 8 (core/tests/test_ai.c configure_ai_profile_vi). */
static bool configure_vi(t64_machine *machine) {
  return configure_vi_sync(machine, UINT32_C(8));
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

#define AI_SAMPLE_PAIR {T64_AI_DEVICE, T64_AI_EVENT_SAMPLE}
#define VI_FIELD_PAIR {T64_VI_DEVICE, T64_VI_EVENT_FIELD}

/* Order 0 starts the AI first (AI sample then VI field); order 1 starts the
 * VI first (VI field then AI sample). */
static const pair_row tracer_rows[2] = {
    {.name = "PAIR-VI-AI-STEM",
     .tie_label = "tie.vi-ai-tracer",
     .profile = FAMILY_STEM,
     .build = build_vi_ai,
     .expected_tick = TRACER_TIED_TICK,
     .horizon_extra = 1u,
     .expected = {{AI_SAMPLE_PAIR, VI_FIELD_PAIR}, {VI_FIELD_PAIR, AI_SAMPLE_PAIR}},
     .expected_count = {2u, 2u},
     .order_count = 2u},
    {.name = "PAIR-VI-AI-PROFILE",
     .tie_label = "tie.vi-ai-tracer",
     .profile = AI_PROFILE,
     .build = build_vi_ai,
     .expected_tick = TRACER_TIED_TICK,
     .horizon_extra = 1u,
     .expected = {{AI_SAMPLE_PAIR, VI_FIELD_PAIR}, {VI_FIELD_PAIR, AI_SAMPLE_PAIR}},
     .expected_count = {2u, 2u},
     .order_count = 2u},
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
    check_pair_row(&tracer_rows[row]);
  }
}

/* ---- shared device helpers ------------------------------------------------ */

#define PI_BASE UINT32_C(0x04600000)
#define SP_BASE UINT32_C(0x04040000)
#define DP_START_REGISTER UINT32_C(0x04100000)
#define DP_END_REGISTER UINT32_C(0x04100004)
#define SI_REG(offset) (T64_SI_BASE + (offset))

static void store_word(t64_machine *machine, uint32_t offset, uint32_t value) {
  machine->ram[offset] = (uint8_t)(value >> 24);
  machine->ram[offset + 1u] = (uint8_t)(value >> 16);
  machine->ram[offset + 2u] = (uint8_t)(value >> 8);
  machine->ram[offset + 3u] = (uint8_t)value;
}

/* Authored program at RDRAM 0x1000, the CPU pointed at it in KSEG0. The boot
 * CPU step event stays queued at tick 1, so instruction k retires at tick k.
 * Provenance: core/tests/test_si.c make_store_machine and
 * core/tests/test_sched_ledger.c install_spin_loop. */
static void install_program(t64_machine *machine, const uint32_t *words,
                            size_t count) {
  size_t i;

  for (i = 0u; i < count; i++) {
    store_word(machine, UINT32_C(0x1000) + (uint32_t)(4u * i), words[i]);
  }
  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = t64_cpu_canonical_address(machine->cpu.pc);
}

/* ---- tie.pi-sp: PI and legacy SP DMA completions on the skeleton profile --- */

/* Provenance: core/tests/test_sched_ledger.c arm_pi and arm_sp_dma (the same
 * lengths as core/tests/test_determinism.c start_cross_device_work). */
static bool arm_pi_dma(t64_machine *machine) {
  return write32(machine, PI_BASE, UINT32_C(0x200)) &&
         write32(machine, PI_BASE + 4u, UINT32_C(0x10000000)) &&
         write32(machine, PI_BASE + 12u, UINT32_C(23));
}

static bool arm_sp_dma_legacy(t64_machine *machine) {
  return write32(machine, SP_BASE, 0u) &&
         write32(machine, SP_BASE + 4u, UINT32_C(0x200)) &&
         write32(machine, SP_BASE + 8u, 0u);
}

#define PI_COMPLETE_PAIR {T64_DEVICE_PI, T64_PI_EVENT_COMPLETE}
#define SP_COMPLETE_PAIR {T64_DEVICE_SP, T64_SP_EVENT_COMPLETE}

/*
 * Independent literal for the tied tick: the PI transfer of 24 bytes lasts the
 * profile base of 1 tick plus 24 / 2 = 12 ticks, 13 in all; the eight-byte SP
 * transfer is chosen to land on the same tick (reached assertion below). The
 * RSP-functional profiles carry SP DMA on RSP issue events (device 4 kind 2)
 * instead, so the legacy completion event (device 4 kind 1) exists only on the
 * skeleton profile and this row runs there, as the 09-01 pair does.
 */
#define PI_SP_TIED_TICK UINT64_C(13)

static t64_machine *build_pi_sp(const pair_row *row, int order,
                                uint64_t *tied_tick_out) {
  t64_machine *machine = create_machine(row->profile);
  uint64_t pi_tick = UINT64_MAX;
  uint64_t sp_tick = UINT64_MAX;
  const order_pair pi = PI_COMPLETE_PAIR;
  const order_pair sp = SP_COMPLETE_PAIR;
  bool ok;

  if (machine == NULL) return NULL;
  if (order == 0) {
    ok = arm_pi_dma(machine) && arm_sp_dma_legacy(machine);
  } else {
    ok = arm_sp_dma_legacy(machine) && arm_pi_dma(machine);
  }
  if (!ok) {
    t64_machine_destroy(machine);
    return NULL;
  }
  T64_CHECK(queued_tick_of(machine, pi.device, pi.kind, &pi_tick));
  T64_CHECK(queued_tick_of(machine, sp.device, sp.kind, &sp_tick));
  T64_CHECK_INT(pi_tick, sp_tick);
  T64_CHECK(order == 0 ? queued_before(machine, pi_tick, pi, sp)
                       : queued_before(machine, pi_tick, sp, pi));
  *tied_tick_out = pi_tick;
  return machine;
}

/* ---- the SI rows ----------------------------------------------------------- */

/* Literal two-channel pad-poll frame (channel 0 one state command expecting
 * four bytes, channel 1 the same command on an absent device, the end escape,
 * and byte 63 requesting a parse). Provenance: core/tests/test_si.c
 * poll_frame. */
static const uint8_t poll_frame[64] = {
    0x01, 0x04, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
    0x01, 0x04, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFE, [63] = 0x01};

/* Independent timing literals from docs/spec/si-pif-controller.md: the WR64B
 * write completes 2,500 ticks after its start; the handshake of the poll frame
 * is 10,876 (one controller, 2000 + 188 + 438 + 4 * 2000 + 250) plus 6,095
 * (one absent channel, 2000 + 188 + 3,907) = 16,971; the read tail is 4,096. */
#define SI_STAGE_DRAM UINT32_C(0x3800)
#define SI_READ_DRAM UINT32_C(0x3900)
#define SI_S0 UINT64_C(2500)
#define SI_H_POLL UINT64_C(16971)
#define SI_TAIL UINT64_C(4096)
#define SI_E (SI_S0 + SI_H_POLL)
#define SI_C (SI_E + SI_TAIL)

enum {
  SI_VARIANT_INPUT_AT_E = 0,
  SI_VARIANT_INPUT_AT_E_MINUS_1 = 1,
  SI_VARIANT_SYNTHETIC_AT_E = 2,
  SI_VARIANT_SYNTHETIC_AT_C = 3
};

#define INPUT_PAIR {T64_TRACE_DEVICE_INPUT, T64_TRACE_KIND_INPUT_SUBMITTED}
#define SYNTHETIC_PAIR {T64_DEVICE_SYNTHETIC_TEST, T64_TEST_EVENT_NOOP}
#define SI_EXECUTE_PAIR {T64_SI_DEVICE, T64_SI_EVENT_EXECUTE}
#define SI_COMPLETE_PAIR {T64_SI_DEVICE, T64_SI_EVENT_COMPLETE}
#define CPU_STEP_PAIR {T64_DEVICE_CPU, T64_CPU_EVENT_STEP}

/* The state the host input carries: A pressed, x 1, y -1 (bytes 80 00 01 FF). */
static const uint8_t state_a_bytes[4] = {0x80u, 0x00u, 0x01u, 0xFFu};
static const uint8_t neutral_bytes[4] = {0x00u, 0x00u, 0x00u, 0x00u};

/* Stage the frame with a completed WR64B, then start the RD64B at tick 2,500
 * with the extra event (input or synthetic) queued before it (order 0) or
 * after it (order 1). The extra event sits at E, E - 1 or C by variant. */
static t64_machine *build_si(const pair_row *row, int order,
                             uint64_t *tied_tick_out) {
  t64_machine *machine = create_machine(row->profile);
  run_ctx ctx;
  bool ok = machine != NULL;
  uint64_t extra_tick;
  uint64_t tied;
  order_pair extra = SYNTHETIC_PAIR;
  order_pair si = SI_EXECUTE_PAIR;

  if (machine == NULL) return NULL;
  extra_tick = row->variant == SI_VARIANT_INPUT_AT_E_MINUS_1 ? SI_E - 1u
               : row->variant == SI_VARIANT_SYNTHETIC_AT_C   ? SI_C
                                                             : SI_E;
  tied = row->variant == SI_VARIANT_SYNTHETIC_AT_C ? SI_C : SI_E;
  if (row->variant == SI_VARIANT_INPUT_AT_E ||
      row->variant == SI_VARIANT_INPUT_AT_E_MINUS_1) {
    const order_pair input = INPUT_PAIR;

    extra = input;
  }
  if (row->variant == SI_VARIANT_SYNTHETIC_AT_C) {
    const order_pair complete = SI_COMPLETE_PAIR;

    si = complete;
  }
  (void)memcpy(machine->ram + SI_STAGE_DRAM, poll_frame, sizeof poll_frame);
  ok = ok && cancel_boot_cpu_event(machine) &&
       write32(machine, SI_REG(T64_SI_REG_DRAM_ADDR), SI_STAGE_DRAM) &&
       write32(machine, SI_REG(T64_SI_REG_WR64B), T64_SI_PIF_ADDRESS);
  if (ok) {
    ctx_start(&ctx, machine, false, 0u);
    ok = ctx_advance_to(&ctx, SI_S0);
  }
  /* Acknowledge the write interrupt, then aim the read at 0x3900. */
  ok = ok && write32(machine, SI_REG(T64_SI_REG_STATUS), 0u) &&
       write32(machine, SI_REG(T64_SI_REG_DRAM_ADDR), SI_READ_DRAM);
  if (!ok) {
    t64_machine_destroy(machine);
    return NULL;
  }
  T64_CHECK_INT(machine->tick, SI_S0);
  {
    int pass;

    for (pass = 0; pass < 2; pass++) {
      /* order 0: the extra event first; order 1: the read start first. */
      const bool do_extra = (pass == 0) == (order == 0);

      if (do_extra) {
        if (row->variant == SI_VARIANT_INPUT_AT_E ||
            row->variant == SI_VARIANT_INPUT_AT_E_MINUS_1) {
          T64_CHECK_INT(t64_machine_queue_input(machine, extra_tick, 0u,
                                                UINT16_C(0x8000), 1, -1),
                        T64_STATUS_OK);
        } else {
          T64_CHECK_INT(t64_test_schedule_synthetic(
                            machine, extra_tick, T64_TEST_EVENT_NOOP, 1u),
                        T64_STATUS_OK);
        }
      } else {
        T64_CHECK_INT(write32(machine, SI_REG(T64_SI_REG_RD64B),
                              T64_SI_PIF_ADDRESS),
                      true);
      }
    }
  }
  T64_CHECK_INT(machine->si.execute_tick, SI_E);
  T64_CHECK_INT(machine->si.complete_tick, SI_C);
  if (row->variant != SI_VARIANT_INPUT_AT_E_MINUS_1) {
    /* Reached: both events are queued at the tied tick in insertion order. */
    T64_CHECK(order == 0 ? queued_before(machine, tied, extra, si)
                         : queued_before(machine, tied, si, extra));
  }
  *tied_tick_out = tied;
  return machine;
}

/* Destination bytes of the executed read: the four state bytes of channel 0
 * sit at 0x3903..0x3906 once the read has completed. A wrong sample is the
 * si-sample comparator. */
static bool expect_state_bytes(const t64_machine *machine, const char *what,
                               const uint8_t expected[4]) {
  const uint8_t *actual = machine->ram + SI_READ_DRAM + 3u;

  if (memcmp(actual, expected, 4u) != 0) {
    fprintf(stderr, "%s: bytes %02x %02x %02x %02x want %02x %02x %02x %02x\n",
            what, (unsigned)actual[0], (unsigned)actual[1], (unsigned)actual[2],
            (unsigned)actual[3], (unsigned)expected[0], (unsigned)expected[1],
            (unsigned)expected[2], (unsigned)expected[3]);
    comparator_fail("si-sample");
    return false;
  }
  T64_CHECK(true);
  return true;
}

/* An input stamped E is not seen by the sample at E (strictly-after rule), an
 * input stamped E - 1 is. */
static void post_si_input(const pair_row *row, int order,
                          t64_machine *machine) {
  (void)order;
  if (machine->tick < SI_C) return;
  expect_state_bytes(machine, row->name,
                     row->variant == SI_VARIANT_INPUT_AT_E_MINUS_1
                         ? state_a_bytes
                         : neutral_bytes);
}

/* Same setup, the input stamped one tick before E: both orders see state A.
 * An accepted baseline for the E row above and the kill row of the
 * promote-then-sample mutant. */
static void check_si_input_control(void) {
  static const pair_row control = {
      .name = "PAIR-SI-INPUT-E-MINUS-1",
      .tie_label = "tie.si-execute-input",
      .profile = FAMILY_STEM,
      .build = build_si,
      .variant = SI_VARIANT_INPUT_AT_E_MINUS_1,
      .horizon_extra = SI_TAIL + 1u};
  int order;

  for (order = 0; order < 2; order++) {
    uint64_t tied = 0u;
    t64_machine *machine = build_si(&control, order, &tied);
    run_ctx ctx;

    if (machine == NULL) return;
    ctx_start(&ctx, machine, false, 0u);
    if (ctx_advance_to(&ctx, SI_C)) {
      expect_state_bytes(machine, control.name, state_a_bytes);
    }
    t64_machine_destroy(machine);
  }
}

/* ---- tie.cpu-si-store-sequence: a guest store that starts an SI read ------- */

/* lui t0,0xA480; lui t1,0x1FC0; ori t1,t1,0x07C0; sw t1,4(t0) (the RD64B
 * start); b self; nop. Provenance: core/tests/test_si.c OP_* words. */
static const uint32_t cpu_si_program[6] = {
    UINT32_C(0x3C08A480), UINT32_C(0x3C091FC0), UINT32_C(0x352907C0),
    UINT32_C(0xAD090004), UINT32_C(0x1000FFFF), UINT32_C(0x00000000)};

#define CPU_SI_STORE_TICK UINT64_C(4)

/* The tick of the bus write32 record at physical address 0x04800004: the CPU
 * program runs from tick 0 in one-tick calls with the trace drained after
 * each, so no record is dropped. UINT64_MAX when the store never happens. */
static uint64_t calibrate_store_tick(const uint32_t *program, size_t count,
                                     uint64_t horizon) {
  t64_machine *machine = create_machine(FAMILY_STEM);
  uint64_t at;
  uint64_t tick;
  run_ctx ctx;

  if (machine == NULL) return UINT64_MAX;
  install_program(machine, program, count);
  ctx_start(&ctx, machine, true, 0u);
  ctx.find_address = UINT64_C(0x04800004);
  for (at = 1u; at <= horizon && ctx.found_tick == UINT64_MAX; at++) {
    if (!ctx_advance_to(&ctx, at)) break;
  }
  T64_CHECK_INT(ctx.trace_dropped, 0u);
  tick = ctx.found_tick;
  t64_machine_destroy(machine);
  return tick;
}

static t64_machine *build_cpu_si(const pair_row *row, int order,
                                 uint64_t *tied_tick_out) {
  t64_machine *machine = create_machine(row->profile);

  (void)order;
  if (machine == NULL) return NULL;
  install_program(machine, cpu_si_program,
                  sizeof cpu_si_program / sizeof cpu_si_program[0]);
  /* No tie exists before the store retires: the execute event is committed by
   * the store itself, after the CPU step event that performs it. */
  *tied_tick_out = CPU_SI_STORE_TICK;
  return machine;
}

/*
 * Relative sequences after the step that retires the store. q is the sequence
 * of the dispatching CPU step event (read from the queue the tick before). The
 * reserve-then-commit design (K16) gives the SI execute event q + 1, the SI
 * complete event q + 2 and the CPU successor, re-reserved after the store's
 * events commit, q + 3, with next_sequence q + 4. The event budget is edited
 * in memory after create (N6) so the advance stops right after the store step.
 */
static void check_cpu_si_sequences(void) {
  t64_machine *machine = create_machine(FAMILY_STEM);
  run_ctx ctx;
  uint64_t q;
  uint32_t saved_budget;
  t64_stop_request request;
  t64_stop_result stop = {0u, 0u};
  const t64_event *front;

  if (machine == NULL) return;
  install_program(machine, cpu_si_program,
                  sizeof cpu_si_program / sizeof cpu_si_program[0]);
  ctx_start(&ctx, machine, false, 0u);
  if (!ctx_advance_to(&ctx, CPU_SI_STORE_TICK - 1u)) {
    t64_machine_destroy(machine);
    return;
  }
  /* Reached: the dispatching CPU step event is the only queued event. */
  T64_CHECK_INT(machine->queue_count, 1u);
  front = &machine->events[machine->queue_head];
  T64_CHECK_INT(front->device, T64_DEVICE_CPU);
  T64_CHECK_INT(front->kind, T64_CPU_EVENT_STEP);
  T64_CHECK_INT(front->tick, CPU_SI_STORE_TICK);
  q = front->sequence;
  saved_budget = machine->profile.advance_event_budget;
  machine->profile.advance_event_budget = 1u;
  request.kind = T64_STOP_GUEST_TICK;
  request.value = CPU_SI_STORE_TICK;
  T64_CHECK_INT(t64_machine_advance(machine, &request, &stop), T64_STATUS_OK);
  T64_CHECK_INT(stop.reason, T64_STOP_REASON_RESOURCE_LIMIT);
  machine->profile.advance_event_budget = saved_budget;
  {
    /* Sorted by tick then sequence: execute (S), successor (S + 1), complete
     * (S + 4,096). */
    static const struct {
      uint64_t tick_offset;
      uint64_t sequence_offset;
      uint32_t device;
      uint32_t kind;
    } expected[3] = {{0u, 1u, T64_SI_DEVICE, T64_SI_EVENT_EXECUTE},
                     {1u, 3u, T64_DEVICE_CPU, T64_CPU_EVENT_STEP},
                     {4096u, 2u, T64_SI_DEVICE, T64_SI_EVENT_COMPLETE}};
    bool equal = machine->queue_count == 3u && machine->next_sequence == q + 4u;
    size_t i;

    for (i = 0u; equal && i < 3u; i++) {
      const t64_event *event =
          &machine->events[(machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];

      equal = event->tick == CPU_SI_STORE_TICK + expected[i].tick_offset &&
              event->sequence == q + expected[i].sequence_offset &&
              event->device == expected[i].device &&
              event->kind == expected[i].kind;
    }
    if (!equal) {
      fprintf(stderr,
              "cpu-si store: queue_count %zu next_sequence %llu (q %llu)\n",
              machine->queue_count, (unsigned long long)machine->next_sequence,
              (unsigned long long)q);
      for (i = 0u; i < machine->queue_count; i++) {
        const t64_event *event =
            &machine->events[(machine->queue_head + i) %
                             T64_EVENT_QUEUE_CAPACITY];

        fprintf(stderr, "  tick %llu sequence %llu device %u kind %u\n",
                (unsigned long long)event->tick,
                (unsigned long long)event->sequence, (unsigned)event->device,
                (unsigned)event->kind);
      }
      comparator_fail("queue-snapshot");
    } else {
      T64_CHECK(true);
    }
  }
  t64_machine_destroy(machine);
}

/* ---- the all-devices workload W-ALL ---------------------------------------- */

/*
 * CPU program: set up the SI base and the PIF address, count 1,000 loop
 * iterations (three instructions each), acknowledge the SI interrupt, aim the
 * read at 0x3900 and start the RD64B, then branch to self. Instruction k
 * retires at tick k (the boot event at tick 1, interval one tick), so the
 * store is instruction 5 + 3 * 1,000 + 3 = 3,008 and S = 3,008. The staged
 * WR64B started at tick 0 completes at 2,500, before S.
 */
static const uint32_t w_program[13] = {
    UINT32_C(0x3C08A480), /* lui   t0,0xA480 */
    UINT32_C(0x3C091FC0), /* lui   t1,0x1FC0 */
    UINT32_C(0x352907C0), /* ori   t1,t1,0x07C0 */
    UINT32_C(0x340B3900), /* ori   t3,zero,0x3900 */
    UINT32_C(0x340A03E8), /* ori   t2,zero,1000 */
    UINT32_C(0x254AFFFF), /* addiu t2,t2,-1 */
    UINT32_C(0x1540FFFE), /* bne   t2,zero,-2 */
    UINT32_C(0x00000000), /* nop (delay slot) */
    UINT32_C(0xAD000018), /* sw    zero,0x18(t0) */
    UINT32_C(0xAD0B0000), /* sw    t3,0(t0) */
    UINT32_C(0xAD090004), /* sw    t1,4(t0) (the RD64B start) */
    UINT32_C(0x1000FFFF), /* b     self */
    UINT32_C(0x00000000)  /* nop */
};

#define W_STORE_TICK UINT64_C(3008)
#define W_E (W_STORE_TICK + SI_H_POLL)
#define W_C (W_E + SI_TAIL)
#define H_ALL UINT64_C(60000)

/*
 * The video queue holds two packets and nothing drains it, so a third field
 * boundary before the horizon would stall the VI (resource-limit with no
 * progress, which the resume loop reports as resume-liveness). The first VI
 * event is the first scan-line boundary at tick 1,418 whatever v_sync is, and
 * field boundaries fall every v_sync lines of 1,417.37 ticks: with v_sync 16
 * the third boundary is at line 48, tick 68,034, after H_ALL.
 */
#define W_VI_V_SYNC UINT32_C(16)

/* Provenance of the device starts: core/tests/test_sched_ledger.c arm_rsp,
 * arm_rdp (the RSP-functional profiles), core/tests/test_pi_dma.c and
 * core/tests/test_ai.c. */
static bool arm_w_devices(t64_machine *machine) {
  size_t i;
  bool ok;

  /* PI cartridge DMA of 24 bytes to RDRAM 0x200. */
  ok = arm_pi_dma(machine);
  /* SP DMA of eight bytes from RDRAM 0x600 to DMEM, then an RSP program that
   * retires an add and breaks. */
  ok = ok && write32(machine, SP_BASE, 0u) &&
       write32(machine, SP_BASE + 4u, UINT32_C(0x600)) &&
       write32(machine, SP_BASE + 8u, 0u) &&
       write32(machine, UINT32_C(0x04001000), UINT32_C(0x24020009)) &&
       write32(machine, UINT32_C(0x04001004), UINT32_C(0x0000000d));
  if (ok) T64_CHECK_INT(t64_rsp_start(machine), T64_STATUS_OK);
  /* RDP: four full-sync commands at RDRAM 0x100. */
  for (i = 0u; i < 4u; i++) {
    (void)memset(machine->ram + UINT32_C(0x100) + i * 8u, 0, 8u);
    machine->ram[UINT32_C(0x100) + i * 8u] = 0xe9u;
  }
  ok = ok && write32(machine, DP_START_REGISTER, UINT32_C(0x100)) &&
       write32(machine, DP_END_REGISTER, UINT32_C(0x120));
  /* AI: two frames, and the VI registers of the tracer. */
  ok = ok && configure_ai_rate_pair(machine) &&
       write32(machine, AI_CONTROL, 1u) &&
       write32(machine, AI_DRAM_ADDR, UINT32_C(0x400)) &&
       write32(machine, AI_LEN, 8u) &&
       configure_vi_sync(machine, W_VI_V_SYNC);
  return ok;
}

/* W-ALL on a profile id, optionally with the three inputs A at E - 1, B at E
 * and C at E + 1 (A buttons 0x8000 x 1 y -1; B 0x4021 x 127 y -128; C 0x1802
 * x -1 y 1). All host writes happen before the first advance. */
static t64_machine *build_w_all(const char *profile_id, bool with_inputs) {
  t64_machine *machine = create_machine(profile_id);
  bool ok;

  if (machine == NULL) return NULL;
  install_program(machine, w_program, sizeof w_program / sizeof w_program[0]);
  (void)memcpy(machine->ram + SI_STAGE_DRAM, poll_frame, sizeof poll_frame);
  ok = write32(machine, SI_REG(T64_SI_REG_DRAM_ADDR), SI_STAGE_DRAM) &&
       write32(machine, SI_REG(T64_SI_REG_WR64B), T64_SI_PIF_ADDRESS) &&
       arm_w_devices(machine);
  if (ok && with_inputs) {
    T64_CHECK_INT(t64_machine_queue_input(machine, W_E - 1u, 0u,
                                          UINT16_C(0x8000), 1, -1),
                  T64_STATUS_OK);
    T64_CHECK_INT(t64_machine_queue_input(machine, W_E, 0u, UINT16_C(0x4021),
                                          127, -128),
                  T64_STATUS_OK);
    T64_CHECK_INT(t64_machine_queue_input(machine, W_E + 1u, 0u,
                                          UINT16_C(0x1802), -1, 1),
                  T64_STATUS_OK);
  }
  if (!ok) {
    t64_machine_destroy(machine);
    return NULL;
  }
  return machine;
}

#define W_VI_AI_TICK TRACER_TIED_TICK

/* One run of W-ALL under a schedule to the horizon. */
static bool run_w_all(const char *profile_id, bool with_inputs,
                      const schedule *plan, bool trace, observation *obs_out,
                      bool check_bytes) {
  t64_machine *machine = build_w_all(profile_id, with_inputs);
  run_ctx ctx;
  bool ok;

  if (machine == NULL) return false;
  ctx_start(&ctx, machine, trace, 0u);
  ok = run_schedule(&ctx, plan, H_ALL);
  observe(&ctx, obs_out);
  if (trace) T64_CHECK_INT(ctx.trace_dropped, 0u);
  if (ok) {
    T64_CHECK_INT(obs_out->stop_reason, T64_STOP_REASON_BOUND);
    T64_CHECK_INT(obs_out->stop_tick, H_ALL);
  }
  if (ok && check_bytes) {
    /* Independent literal: state A was stamped at E - 1, so the sample at E
     * sees A and the executed read leaves 80 00 01 FF at 0x3903..0x3906. */
    (void)expect_state_bytes(machine, "W-ALL", state_a_bytes);
  }
  t64_machine_destroy(machine);
  return ok;
}

/* One row of the ledger: (device, kind) pairs the census must observe. */
typedef struct {
  uint32_t device;
  uint32_t kind;
  bool seen;
} census_row;

static void census_queue(const t64_machine *machine, census_row *rows,
                         size_t row_count) {
  size_t i;
  size_t j;

  for (i = 0u; i < machine->queue_count; i++) {
    const t64_event *event =
        &machine->events[(machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];

    for (j = 0u; j < row_count; j++) {
      if (rows[j].device == event->device && rows[j].kind == event->kind) {
        rows[j].seen = true;
      }
    }
  }
}

/* The census run: one-tick calls with the queue scanned before each call and
 * the trace drained after each call (every 16 ticks or fewer), no drops. The
 * RSP-functional profiles carry SP DMA on RSP issue events (4/2); the legacy
 * SP completion (4/1) is observed on the skeleton row of tie.pi-sp. */
static void check_w_all_census(void) {
  census_row rows[] = {
      {T64_DEVICE_CPU, T64_CPU_EVENT_STEP, false},
      {T64_DEVICE_PI, T64_PI_EVENT_COMPLETE, false},
      {T64_DEVICE_SP, T64_SP_EVENT_RSP_ISSUE, false},
      {T64_TRACE_DEVICE_INPUT, T64_TRACE_KIND_INPUT_SUBMITTED, false},
      {T64_RDP_DEVICE, T64_RDP_EVENT_WORK, false},
      {T64_VI_DEVICE, T64_VI_EVENT_FIELD, false},
      {T64_AI_DEVICE, T64_AI_EVENT_SAMPLE, false},
      {T64_SI_DEVICE, T64_SI_EVENT_EXECUTE, false},
      {T64_SI_DEVICE, T64_SI_EVENT_COMPLETE, false},
  };
  const size_t row_count = sizeof rows / sizeof rows[0];
  t64_machine *machine = build_w_all(FAMILY_STEM, true);
  run_ctx ctx;
  uint64_t tick;
  size_t i;

  if (machine == NULL) return;
  ctx_start(&ctx, machine, true, 0u);
  census_queue(machine, rows, row_count);
  for (tick = 1u; tick <= H_ALL; tick++) {
    if (!ctx_advance_to(&ctx, tick)) break;
    census_queue(machine, rows, row_count);
  }
  T64_CHECK_INT(ctx.trace_dropped, 0u);
  T64_CHECK_INT(machine->tick, H_ALL);
  for (i = 0u; i < row_count; i++) {
    if (!rows[i].seen) {
      fprintf(stderr, "census: ledger row %u/%u not observed\n",
              (unsigned)rows[i].device, (unsigned)rows[i].kind);
    }
    T64_CHECK(rows[i].seen);
  }
  t64_machine_destroy(machine);
}

/* Calibration of S without inputs: the bus write32 record at 0x04800004. */
static void check_w_all_calibration(void) {
  const uint64_t measured = calibrate_store_tick(
      w_program, sizeof w_program / sizeof w_program[0], W_STORE_TICK + 1u);

  /* The WR64B staging write is a host write at tick 0 and is not traced here
   * (the trace is enabled after the setup); the CPU store is the only record. */
  T64_CHECK_INT(measured, W_STORE_TICK);
}

static void check_w_all_partition(void) {
  static observation reference;
  static observation run;
  const char *const save_ids[3] = {
      "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e4k",
      "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e16k",
      "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-s32k"};
  schedule plans[8];
  size_t plan_count = 0u;
  schedule whole = schedule_whole();
  size_t i;

  /* Reached: the VI field and the AI sample of W-ALL are queued at one tick. */
  {
    t64_machine *probe = build_w_all(FAMILY_STEM, true);
    uint64_t vi_tick = UINT64_MAX;
    uint64_t ai_tick = UINT64_MAX;

    if (probe == NULL) return;
    T64_CHECK(queued_tick_of(probe, T64_VI_DEVICE, T64_VI_EVENT_FIELD,
                             &vi_tick));
    T64_CHECK(queued_tick_of(probe, T64_AI_DEVICE, T64_AI_EVENT_SAMPLE,
                             &ai_tick));
    T64_CHECK_INT(vi_tick, W_VI_AI_TICK);
    T64_CHECK_INT(ai_tick, W_VI_AI_TICK);
    t64_machine_destroy(probe);
  }
  check_w_all_calibration();
  check_w_all_census();

  plans[plan_count++] = schedule_steps(1u);
  plans[plan_count++] = schedule_steps(3u);
  plans[plan_count++] = schedule_steps(7u);
  plans[plan_count++] = schedule_steps(4096u);
  plans[plan_count++] = schedule_split3(W_STORE_TICK);
  plans[plan_count++] = schedule_split3(W_E);
  plans[plan_count++] = schedule_split3(W_C);
  plans[plan_count++] = schedule_split3(W_VI_AI_TICK);

  if (!run_w_all(FAMILY_STEM, true, &whole, false, &reference, true)) return;
  for (i = 0u; i < plan_count; i++) {
    const char *label;

    if (!run_w_all(FAMILY_STEM, true, &plans[i], false, &run, true)) return;
    label = compare_observation(&reference, &run);
    if (label != NULL) {
      fprintf(stderr, "W-ALL schedule %s %llu\n", plans[i].name,
              (unsigned long long)plans[i].arg);
      comparator_fail(label);
    }
  }
  /* Trace on, drained after every 16-tick call, against the trace-off
   * reference (the trace is observation only). */
  {
    schedule sixteen = schedule_steps(16u);
    const char *label;

    if (!run_w_all(FAMILY_STEM, true, &sixteen, true, &run, true)) return;
    label = compare_observation(&reference, &run);
    if (label != NULL) {
      fprintf(stderr, "W-ALL trace on, 16-tick steps\n");
      comparator_fail(label);
    }
  }
  /* The three save ids run the same workload and equal themselves across
   * schedules; their digests are not compared with the stem's (the
   * save-media section differs). */
  for (i = 0u; i < 3u; i++) {
    schedule steps7 = schedule_steps(7u);
    schedule steps4096 = schedule_steps(4096u);
    const char *label;

    if (!run_w_all(save_ids[i], true, &whole, false, &reference, true)) return;
    if (!run_w_all(save_ids[i], true, &steps7, false, &run, true)) return;
    label = compare_observation(&reference, &run);
    if (label != NULL) comparator_fail(label);
    if (!run_w_all(save_ids[i], true, &steps4096, false, &run, true)) return;
    label = compare_observation(&reference, &run);
    if (label != NULL) comparator_fail(label);
  }
}

/* ---- case cross-device ------------------------------------------------------ */

static const pair_row cross_rows[] = {
    {.name = "PAIR-VI-AI-STEM",
     .tie_label = "tie.vi-ai",
     .profile = FAMILY_STEM,
     .build = build_vi_ai,
     .expected_tick = TRACER_TIED_TICK,
     .horizon_extra = 1u,
     .expected = {{AI_SAMPLE_PAIR, VI_FIELD_PAIR}, {VI_FIELD_PAIR, AI_SAMPLE_PAIR}},
     .expected_count = {2u, 2u},
     .order_count = 2u},
    {.name = "PAIR-VI-AI-PROFILE",
     .tie_label = "tie.vi-ai",
     .profile = AI_PROFILE,
     .build = build_vi_ai,
     .expected_tick = TRACER_TIED_TICK,
     .horizon_extra = 1u,
     .expected = {{AI_SAMPLE_PAIR, VI_FIELD_PAIR}, {VI_FIELD_PAIR, AI_SAMPLE_PAIR}},
     .expected_count = {2u, 2u},
     .order_count = 2u},
    {.name = "PAIR-PI-SP-SKELETON",
     .tie_label = "tie.pi-sp",
     .profile = SKELETON_PROFILE,
     .build = build_pi_sp,
     .expected_tick = PI_SP_TIED_TICK,
     .horizon_extra = 1u,
     .expected = {{PI_COMPLETE_PAIR, SP_COMPLETE_PAIR},
                  {SP_COMPLETE_PAIR, PI_COMPLETE_PAIR}},
     .expected_count = {2u, 2u},
     .order_count = 2u},
    {.name = "PAIR-SI-EXECUTE-INPUT",
     .tie_label = "tie.si-execute-input",
     .profile = FAMILY_STEM,
     .build = build_si,
     .variant = SI_VARIANT_INPUT_AT_E,
     .expected_tick = SI_E,
     .horizon_extra = SI_TAIL + 1u,
     .extra_split = SI_S0,
     .expected = {{INPUT_PAIR, SI_EXECUTE_PAIR}, {SI_EXECUTE_PAIR, INPUT_PAIR}},
     .expected_count = {2u, 2u},
     .order_count = 2u,
     .post = post_si_input},
    {.name = "PAIR-SI-EXECUTE-SYNTHETIC",
     .tie_label = "tie.si-execute-synthetic",
     .profile = FAMILY_STEM,
     .build = build_si,
     .variant = SI_VARIANT_SYNTHETIC_AT_E,
     .expected_tick = SI_E,
     .horizon_extra = 1u,
     .extra_split = SI_S0,
     .expected = {{SYNTHETIC_PAIR, SI_EXECUTE_PAIR},
                  {SI_EXECUTE_PAIR, SYNTHETIC_PAIR}},
     .expected_count = {2u, 2u},
     .order_count = 2u},
    {.name = "PAIR-SI-COMPLETE-SYNTHETIC",
     .tie_label = "tie.si-complete-synthetic",
     .profile = FAMILY_STEM,
     .build = build_si,
     .variant = SI_VARIANT_SYNTHETIC_AT_C,
     .expected_tick = SI_C,
     .horizon_extra = 1u,
     .extra_split = SI_E,
     .expected = {{SYNTHETIC_PAIR, SI_COMPLETE_PAIR},
                  {SI_COMPLETE_PAIR, SYNTHETIC_PAIR}},
     .expected_count = {2u, 2u},
     .order_count = 2u},
    {.name = "PAIR-CPU-SI-STORE",
     .tie_label = "tie.cpu-si-store-sequence",
     .profile = FAMILY_STEM,
     .build = build_cpu_si,
     .expected_tick = CPU_SI_STORE_TICK,
     .horizon_extra = 1u,
     .expected = {{CPU_STEP_PAIR, SI_EXECUTE_PAIR}},
     .expected_count = {2u, 0u},
     .order_count = 1u},
};

static void case_cross_device(void) {
  size_t row;

  /* The tick of the store is measured by calibration, then compared with the
   * literal derived from four instructions at one tick each. */
  T64_CHECK_INT(calibrate_store_tick(cpu_si_program,
                                     sizeof cpu_si_program /
                                         sizeof cpu_si_program[0],
                                     CPU_SI_STORE_TICK + 2u),
                CPU_SI_STORE_TICK);
  for (row = 0u; row < sizeof cross_rows / sizeof cross_rows[0]; row++) {
    check_pair_row(&cross_rows[row]);
  }
  check_si_input_control();
  check_cpu_si_sequences();
}

/* ---- case partition and case interleave -------------------------------------- */

static void case_partition(void) { check_w_all_partition(); }

/* The rotation of uneven slices used by the interleave case. */
static const uint64_t slice_rotation[5] = {1u, 5u, 4096u, 3u, 977u};

typedef struct {
  const char *name;
  t64_machine *machine;
  run_ctx ctx;
  uint64_t horizon;
} interleaved;

/* The solo reference: the same builder advanced whole to the same horizon. */
static bool solo_observation(const pair_row *row, int order, uint64_t horizon,
                             bool w_all, observation *out) {
  t64_machine *machine;
  uint64_t tied = 0u;
  run_ctx ctx;
  bool ok;

  machine = w_all ? build_w_all(FAMILY_STEM, true)
                  : row->build(row, order, &tied);
  if (machine == NULL) return false;
  ctx_start(&ctx, machine, false, 0u);
  ok = ctx_advance_to(&ctx, horizon);
  observe(&ctx, out);
  t64_machine_destroy(machine);
  return ok;
}

/* The solo-reference comparator label, or NULL when the observations agree. */
static const char *solo_reference_label(const observation *a,
                                        const observation *b) {
  return compare_observation(a, b) == NULL ? NULL : "solo-reference";
}

static void check_interleave(size_t machine_count) {
  static observation solo[3];
  static observation got;
  interleaved set[3];
  const uint64_t horizons[3] = {H_ALL, TRACER_TIED_TICK + 1u,
                                TRACER_TIED_TICK + 1u};
  const char *const names[3] = {"w-all-stem", "vi-ai-stem", "vi-ai-profile"};
  size_t rotation = 0u;
  size_t i;
  bool pending = true;

  (void)memset(set, 0, sizeof set);
  for (i = 0u; i < machine_count; i++) {
    uint64_t tied = 0u;

    set[i].name = names[i];
    set[i].horizon = horizons[i];
    if (i == 0u) {
      set[i].machine = build_w_all(FAMILY_STEM, true);
    } else {
      set[i].machine = tracer_rows[i - 1u].build(&tracer_rows[i - 1u],
                                                 (int)(i - 1u), &tied);
    }
    if (set[i].machine == NULL) {
      for (i = 0u; i < machine_count; i++) {
        if (set[i].machine != NULL) t64_machine_destroy(set[i].machine);
      }
      return;
    }
    ctx_start(&set[i].ctx, set[i].machine, false, 0u);
    if (!solo_observation(i == 0u ? NULL : &tracer_rows[i - 1u],
                          (int)(i == 0u ? 0u : i - 1u), horizons[i], i == 0u,
                          &solo[i])) {
      return;
    }
  }
  /* Round robin in uneven slices, each machine to its own horizon. */
  while (pending) {
    pending = false;
    for (i = 0u; i < machine_count; i++) {
      uint64_t next;

      if (set[i].machine->tick >= set[i].horizon) continue;
      next = set[i].machine->tick + slice_rotation[rotation % 5u];
      rotation++;
      if (next > set[i].horizon) next = set[i].horizon;
      if (!ctx_advance_to(&set[i].ctx, next)) {
        pending = false;
        break;
      }
      if (set[i].machine->tick < set[i].horizon) pending = true;
    }
  }
  for (i = 0u; i < machine_count; i++) {
    const char *label;

    observe(&set[i].ctx, &got);
    label = solo_reference_label(&solo[i], &got);
    if (label != NULL) {
      fprintf(stderr, "interleave %zu machines: %s differs from its solo run\n",
              machine_count, set[i].name);
      comparator_fail(label);
    }
  }
  /* Control, after the accepted baseline: a solo run stopped one tick short of
   * the horizon is reported as a solo-reference difference. */
  {
    static observation short_solo;
    t64_machine *machine = build_w_all(FAMILY_STEM, true);
    run_ctx ctx;

    if (machine != NULL) {
      ctx_start(&ctx, machine, false, 0u);
      if (ctx_advance_to(&ctx, H_ALL - 1u)) {
        const char *label;

        observe(&ctx, &short_solo);
        observe(&set[0].ctx, &got);
        label = solo_reference_label(&short_solo, &got);
        T64_CHECK(label != NULL);
        if (label != NULL) T64_CHECK_STR(label, "solo-reference");
      }
      t64_machine_destroy(machine);
    }
  }
  for (i = 0u; i < machine_count; i++) t64_machine_destroy(set[i].machine);
}

static void case_interleave(void) {
  check_interleave(2u);
  check_interleave(3u);
}

/* ---- case table and entry point ------------------------------------------- */

typedef struct {
  const char *name;
  void (*run)(void);
} matrix_case;

static const matrix_case cases[] = {
    {"tracer-coincident", case_tracer_coincident},
    {"cross-device", case_cross_device},
    {"partition", case_partition},
    {"interleave", case_interleave},
};
#define CASE_COUNT (sizeof cases / sizeof cases[0])

/* `label owner-case` per tie label. */
static const char *const ties[][2] = {
    {"tie.vi-ai", "cross-device"},
    {"tie.pi-sp", "cross-device"},
    {"tie.si-execute-input", "cross-device"},
    {"tie.si-execute-synthetic", "cross-device"},
    {"tie.si-complete-synthetic", "cross-device"},
    {"tie.cpu-si-store-sequence", "cross-device"},
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
