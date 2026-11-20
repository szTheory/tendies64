/*
 * State import campaign (plan 09-09.1, decisions D-30 and D-41).
 *
 * A deterministic xorshift campaign over hash-valid and hash-stale mutants of
 * one exported state: byte flips, truncation, extension, whole-field
 * replacement with boundary values, swapped and duplicated section entries,
 * each re-signed with a random choice of which integrity fields stay valid.
 * Every iteration asserts the contract: the outcome is a closed refusal or an
 * acceptance, a refusal leaves the whole machine allocation byte identical with
 * the allocator counters unchanged, and an accepted state returns from a
 * bounded advance. The structure follows core/tests/fuzz_rom_admission.c. The
 * campaign holds no main: core/tests/test_state_import.c calls it with an
 * iteration count from the environment. No state is stored or logged; blobs
 * exist only in process memory.
 */
#include "state_import_helpers.h"

typedef struct {
  uint64_t state;
} rng;

static uint64_t rng_next(rng *r) {
  uint64_t x = r->state;

  x ^= x >> 12;
  x ^= x << 25;
  x ^= x >> 27;
  r->state = x;
  return x * UINT64_C(0x2545F4914F6CDD1D);
}

static const uint64_t wide_values[] = {
    UINT64_C(0),          UINT64_C(1),
    UINT64_MAX - 1u,      UINT64_MAX,
    UINT64_C(0x7FFFFFFF), UINT64_C(0x80000000),
    UINT64_C(0xFFFFFFFF), UINT64_C(0x100000000)};

static void violation(state_fuzz_stats *stats, uint64_t iteration,
                      const char *what) {
  stats->violations++;
  fprintf(stderr, "fuzz violation at iteration %llu: %s\n",
          (unsigned long long)iteration, what);
}

/* A random mutation of `copy`; returns the new length. */
static size_t mutate(rng *r, uint8_t *copy, size_t length) {
  const size_t table_end = D_TABLE + (size_t)D_ENTRY * 16u;
  size_t n = length;
  size_t position;
  size_t i;
  uint64_t value;

  switch (rng_next(r) % 6u) {
  case 0: {
    const size_t flips = 1u + (size_t)(rng_next(r) % 3u);

    for (i = 0u; i < flips; i++) {
      position = (size_t)(rng_next(r) % length);
      copy[position] ^= (uint8_t)(1u << (rng_next(r) % 8u));
    }
    break;
  }
  case 1:
    n = (size_t)(rng_next(r) % length);
    break;
  case 2:
    n = length + 1u + (size_t)(rng_next(r) % 64u);
    for (i = length; i < n; i++) copy[i] = (uint8_t)rng_next(r);
    break;
  case 3: {
    /* a 4 or 8 byte field at a random aligned position, biased to the header,
     * the table and the small sections that hold the rule-bearing fields */
    const bool narrow = (rng_next(r) & 1u) != 0u;
    const size_t window = (rng_next(r) % 3u) == 0u ? table_end : length - 16u;

    position = (size_t)(rng_next(r) % window) & ~(size_t)3u;
    value = wide_values[rng_next(r) % (sizeof wide_values / sizeof wide_values[0])];
    if (narrow) put32(copy + position, (uint32_t)value);
    else put64(copy + position, value);
    break;
  }
  case 4: {
    uint8_t scratch[D_ENTRY];
    const size_t a = (size_t)(rng_next(r) % 12u);
    const size_t b = (size_t)(rng_next(r) % 12u);

    (void)memcpy(scratch, copy + D_TABLE + (size_t)D_ENTRY * a, D_ENTRY);
    (void)memcpy(copy + D_TABLE + (size_t)D_ENTRY * a,
                 copy + D_TABLE + (size_t)D_ENTRY * b, D_ENTRY);
    (void)memcpy(copy + D_TABLE + (size_t)D_ENTRY * b, scratch, D_ENTRY);
    break;
  }
  default: {
    const size_t a = (size_t)(rng_next(r) % 12u);
    const size_t b = (size_t)(rng_next(r) % 12u);

    (void)memmove(copy + D_TABLE + (size_t)D_ENTRY * b,
                  copy + D_TABLE + (size_t)D_ENTRY * a, D_ENTRY);
    break;
  }
  }
  return n;
}

/* One import with the full contract checked; returns the peer to use next (a
 * fresh machine after an acceptance, the same one after a refusal), or NULL
 * when a replacement could not be created. */
t64_machine *state_fuzz_one(t64_machine *peer, const uint8_t *bytes, size_t n,
                            uint64_t iteration, state_fuzz_stats *stats) {
  t64_state_import_report report;
  snapshot before;
  size_t allocs;
  size_t releases;
  t64_status status;

  snapshot_take(peer, &before);
  allocs = allocate_calls;
  releases = release_calls;
  (void)memset(&report, 0xA5, sizeof report);
  status = t64_machine_state_import(peer, bytes, n, 0u, &report);
  stats->iterations++;
  if (allocate_calls != allocs || release_calls != releases) {
    violation(stats, iteration, "the import called the allocator");
  }
  if (status == T64_STATUS_OK) {
    t64_stop_request request;
    t64_stop_result stop = {0u, 0u};

    stats->accepted++;
    request.kind = T64_STOP_GUEST_TICK;
    request.value = peer->tick + 2000u;
    status = t64_machine_advance(peer, &request, &stop);
    if (stop.reason == T64_STOP_REASON_RESOURCE_LIMIT) stats->liveness_stops++;
    if (status != T64_STATUS_OK && status != T64_STATUS_SEQUENCE_EXHAUSTED &&
        status != T64_STATUS_QUEUE_FULL && status != T64_STATUS_TICK_OVERFLOW) {
      violation(stats, iteration,
                "an accepted state ended in an undefined advance status");
    }
    t64_machine_destroy(peer);
    peer = create_admitted(SI_STEM_ID);
  } else {
    stats->refusals++;
    if (report.reason < 48u) stats->by_reason[report.reason]++;
    if (report.status != status ||
        !state_reason_status_consistent(report.reason, (uint32_t)status)) {
      violation(stats, iteration, "the status and reason are not a closed pair");
    }
    if (!snapshot_matches(peer, &before)) {
      violation(stats, iteration, "a refusal changed the machine");
    }
    if (!t64_state_summary_is_clear(peer)) {
      violation(stats, iteration, "a refusal left the scratch dirty");
    }
  }
  snapshot_free(&before);
  return peer;
}

void state_fuzz_campaign(const uint8_t *blob, size_t length,
                         uint64_t iterations, uint64_t seed,
                         state_fuzz_stats *stats) {
  rng r = {seed == 0u ? UINT64_C(0x9E3779B97F4A7C15) : seed};
  uint8_t *copy = (uint8_t *)malloc(length + 128u);
  t64_machine *peer = create_admitted(SI_STEM_ID);
  uint64_t iteration;

  (void)memset(stats, 0, sizeof *stats);
  if (copy == NULL || peer == NULL || length < 4096u) {
    stats->violations++;
    free(copy);
    if (peer != NULL) t64_machine_destroy(peer);
    return;
  }
  for (iteration = 0u; iteration < iterations && peer != NULL; iteration++) {
    size_t n;

    (void)memcpy(copy, blob, length);
    n = mutate(&r, copy, length);
    blob_resign(copy, n, (unsigned)(rng_next(&r) % 8u));
    peer = state_fuzz_one(peer, copy, n, iteration, stats);
  }
  if (peer == NULL) {
    stats->violations++;
  } else {
    t64_machine_destroy(peer);
  }
  free(copy);
  stats->violations += (uint64_t)t64_test_failures;
}
