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
 *
 * Plan 10-12 adds the state 26 entry state_fuzz_campaign_v26 (called by
 * core/tests/test_state_import_v26.c with windows over the new section 8
 * bytes): the same mutators and the same contract, with a peer of the si-v2
 * stem id and the integrity fields re-signed under the domain t64.state.si.26.
 * The state 25 entry keeps its peer, its domain and its behavior; the phase09
 * case list of core/test_state_import is not touched.
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

static const state_fuzz_window *fuzz_windows;
static size_t fuzz_window_count;

/* The peer a campaign imports into and the domain text its re-signing uses:
 * the si-v1 stem and t64.state.si.25 unless state_fuzz_campaign_v26 runs. */
#define FUZZ_V26_PEER_ID "ntsc-cart-4m-openipl3-rdram-b-t1-si-v2"
#define FUZZ_V26_DOMAIN "t64.state.si.26"
static const char *fuzz_peer_id = SI_STEM_ID;
static bool fuzz_domain_v26;

/* blob_resign of the helper header with the domain text of the campaign: the
 * state 25 path calls the helper itself, so its behavior is the Phase 9 one. */
static void fuzz_resign(uint8_t *blob, size_t length, unsigned flags) {
  const uint32_t count = rd32(blob + 0x018u);
  uint32_t i;

  if (!fuzz_domain_v26) {
    blob_resign(blob, length, flags);
    return;
  }
  if (length < D_TABLE + D_TRAILER_BYTES || count > 32u) return;
  if ((flags & RS_SECTIONS) != 0u) {
    for (i = 0u; i < count; i++) {
      uint8_t *entry = blob + D_TABLE + (size_t)D_ENTRY * i;
      const uint64_t offset = rd64(entry + E_OFF);
      const uint64_t body = rd64(entry + E_LEN);

      if (offset <= length && body <= length - offset) {
        sha_of(blob + offset, (size_t)body, entry + E_HASH);
      }
    }
  }
  if ((flags & RS_DECLARED) != 0u) {
    t64_sha256_ctx context;
    uint8_t digest[T64_DIGEST_SIZE];

    t64_sha256_init(&context);
    t64_sha256_update(&context, (const uint8_t *)FUZZ_V26_DOMAIN,
                      D_DOMAIN_BYTES);
    for (i = 0u; i < count; i++) {
      const uint8_t *entry = blob + D_TABLE + (size_t)D_ENTRY * i;
      const uint32_t id = rd32(entry + E_ID);
      const uint64_t offset = rd64(entry + E_OFF);
      const uint64_t body = rd64(entry + E_LEN);
      uint8_t marker[4];

      if (id == 16u) continue;
      put32(marker, id);
      t64_sha256_update(&context, marker, sizeof marker);
      if (offset <= length && body <= length - offset) {
        t64_sha256_update(&context, blob + offset, (size_t)body);
      }
    }
    t64_sha256_final(&context, digest);
    (void)memcpy(blob + 0x070u, digest, sizeof digest);
  }
  if ((flags & RS_TRAILER) != 0u) {
    sha_of(blob, length - D_TRAILER_BYTES, blob + length - D_TRAILER_BYTES);
  }
}

void state_fuzz_set_windows(const state_fuzz_window *windows, size_t count) {
  fuzz_windows = windows;
  fuzz_window_count = count;
}

/* The body of a section by id: offset and length in the blob. */
static bool fuzz_section(const uint8_t *blob, size_t length, uint32_t id,
                         uint64_t *offset_out, uint64_t *body_out) {
  const uint32_t count = rd32(blob + 0x018u);
  uint32_t i;

  if (count == 0u || count > 32u) return false;
  for (i = 0u; i < count; i++) {
    const uint8_t *entry = blob + D_TABLE + (size_t)D_ENTRY * i;
    const uint64_t offset = rd64(entry + E_OFF);
    const uint64_t body = rd64(entry + E_LEN);

    if (rd32(entry + E_ID) == id && offset < length && body <= length - offset) {
      *offset_out = offset;
      *body_out = body;
      return true;
    }
  }
  return false;
}

/* A random aligned position where a rule is: three times in four inside a
 * window the caller named (the rule-bearing fields of a section whose bulk has
 * none), otherwise inside a section other than RDRAM and SP memory, so the
 * small sections are hit as often as the large ones and the 4 MiB RDRAM and the
 * 2 MiB coverage arrays do not take half of the mutations (plan 09-10 review
 * L2 WR-04). */
static size_t section_position(rng *r, const uint8_t *blob, size_t length,
                               size_t width) {
  const uint32_t count = rd32(blob + 0x018u);
  uint64_t offset = 0u;
  uint64_t body = 0u;
  unsigned attempt;

  if (fuzz_window_count != 0u && (rng_next(r) % 4u) != 0u) {
    const state_fuzz_window *w = &fuzz_windows[rng_next(r) % fuzz_window_count];

    if (fuzz_section(blob, length, w->section, &offset, &body) &&
        w->length >= width && w->offset <= body && w->length <= body - w->offset) {
      return (size_t)(offset + w->offset +
                      ((rng_next(r) % (w->length - width + 1u)) &
                       ~(uint64_t)3u));
    }
  }
  for (attempt = 0u; attempt < 8u; attempt++) {
    const uint32_t pick = count == 0u || count > 32u
                              ? 0u
                              : (uint32_t)(rng_next(r) % count);
    const uint8_t *entry = blob + D_TABLE + (size_t)D_ENTRY * pick;
    const uint32_t id = rd32(entry + E_ID);

    offset = rd64(entry + E_OFF);
    body = rd64(entry + E_LEN);
    if (count == 0u || count > 32u || offset >= length || body < width ||
        body > length - offset) {
      break;
    }
    if (id == T64_STATE_SECTION_RDRAM || id == T64_STATE_SECTION_SP_MEMORY) {
      continue;
    }
    return (size_t)(offset +
                    ((rng_next(r) % (body - width + 1u)) & ~(uint64_t)3u));
  }
  return (size_t)(rng_next(r) % (length - width)) & ~(size_t)3u;
}

/* A random mutation of `copy`; returns the new length. */
static size_t mutate(rng *r, uint8_t *copy, size_t length) {
  const size_t table_end = D_TABLE + (size_t)D_ENTRY * 16u;
  size_t n = length;
  size_t position;
  size_t i;
  uint64_t value;

  /* seven in ten mutations are field edits (a flip or a boundary value at a
   * rule-bearing position), the rest are structural */
  static const unsigned char kinds[10] = {0u, 0u, 3u, 3u, 3u, 3u, 3u, 1u, 2u, 4u};
  unsigned kind = kinds[rng_next(r) % 10u];

  if (kind == 4u && (rng_next(r) & 1u) != 0u) kind = 5u;
  switch (kind) {
  case 0: {
    const size_t flips = 1u + (size_t)(rng_next(r) % 3u);

    for (i = 0u; i < flips; i++) {
      position = (rng_next(r) & 1u) != 0u ? section_position(r, copy, length, 1u)
                                          : (size_t)(rng_next(r) % length);
      copy[position] ^= (uint8_t)(1u << (rng_next(r) % 8u));
    }
    break;
  }
  case 1:
    n = (size_t)(rng_next(r) % length);
    /* the header total follows the new length, so the check that sees it first
     * is not always TOTAL_LENGTH */
    put64(copy + D_TOTAL, (uint64_t)n);
    break;
  case 2:
    n = length + 1u + (size_t)(rng_next(r) % 64u);
    for (i = length; i < n; i++) copy[i] = (uint8_t)rng_next(r);
    put64(copy + D_TOTAL, (uint64_t)n);
    break;
  case 3: {
    /* a 4 or 8 byte field at a random aligned position, biased to the header,
     * the table and the small sections that hold the rule-bearing fields */
    const bool narrow = (rng_next(r) & 1u) != 0u;
    const size_t window = table_end;

    position = (rng_next(r) % 4u) == 0u
                   ? (size_t)(rng_next(r) % window) & ~(size_t)3u
                   : section_position(r, copy, length, narrow ? 4u : 8u);
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
  t64_state_import_report report = T64_STATE_IMPORT_REPORT_INIT;
  snapshot before;
  size_t allocs;
  size_t releases;
  t64_status status;

  snapshot_take(peer, &before);
  allocs = allocate_calls;
  releases = release_calls;
  poison_import_report(&report);
  status = t64_machine_state_import(peer, bytes, n, 0u, &report);
  stats->iterations++;
  if (allocate_calls != allocs || release_calls != releases) {
    violation(stats, iteration, "the import called the allocator");
  }
  if (status == T64_STATUS_OK) {
    t64_stop_request request = T64_STOP_REQUEST_INIT(0u, 0u);
    t64_stop_result stop = T64_STOP_RESULT_INIT;

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
    peer = create_admitted(fuzz_peer_id);
  } else {
    stats->refusals++;
    if (report.reason < 48u) stats->by_reason[report.reason]++;
    if (report.reason >= T64_STATE_REASON_FIELD_RANGE) stats->field_level++;
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

static void fuzz_campaign_run(const uint8_t *blob, size_t length,
                              uint64_t iterations, uint64_t seed,
                              state_fuzz_stats *stats) {
  rng r = {seed == 0u ? UINT64_C(0x9E3779B97F4A7C15) : seed};
  uint8_t *copy = (uint8_t *)malloc(length + 128u);
  t64_machine *peer = create_admitted(fuzz_peer_id);
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

    unsigned integrity;

    (void)memcpy(copy, blob, length);
    n = mutate(&r, copy, length);
    /* six draws in eight re-sign every integrity field (the readers and the
     * later checks run), one keeps the declared digest stale (the last check
     * fails for a state the readers accepted) and one leaves all of them stale
     * (the trailer fails first); the first version left the trailer stale in
     * half the draws */
    switch (rng_next(&r) % 8u) {
    case 0u: integrity = 0u; break;
    case 1u: integrity = RS_SECTIONS | RS_TRAILER; break;
    default: integrity = RS_ALL; break;
    }
    fuzz_resign(copy, n, integrity);
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

void state_fuzz_campaign(const uint8_t *blob, size_t length,
                         uint64_t iterations, uint64_t seed,
                         state_fuzz_stats *stats) {
  fuzz_campaign_run(blob, length, iterations, seed, stats);
}

/* The state 26 entry (declared by its caller, core/tests/test_state_import_v26.c):
 * a peer of the si-v2 stem id and the state 26 domain for the re-signing. */
void state_fuzz_campaign_v26(const uint8_t *blob, size_t length,
                             uint64_t iterations, uint64_t seed,
                             state_fuzz_stats *stats);
void state_fuzz_campaign_v26(const uint8_t *blob, size_t length,
                             uint64_t iterations, uint64_t seed,
                             state_fuzz_stats *stats) {
  fuzz_peer_id = FUZZ_V26_PEER_ID;
  fuzz_domain_v26 = true;
  fuzz_campaign_run(blob, length, iterations, seed, stats);
  fuzz_peer_id = SI_STEM_ID;
  fuzz_domain_v26 = false;
}
