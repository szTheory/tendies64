/*
 * State import tests (plans 09-09 and 09-09.1; the shared helpers are in
 * state_import_helpers.h).
 *
 * The importer is exercised on authored machines of the composite family only:
 * every blob is produced in memory by the exporter of plan 09-08 from authored
 * inputs or the already cleared openIPL3 asset, and no test reads or writes a
 * file. The container offsets used here are literal copies of the reference
 * decoder of core/tests/test_state_export.c, so a layout mistake in the
 * production code cannot be hidden by a matching mistake in these tests. The
 * hashes inside a container are integrity checks, not authentication; the
 * contract under test is memory safety and liveness, and no case claims that an
 * imported state is reachable or hardware accurate.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "t64_internal.h"
#include "t64_sha256.h"
#include "t64_test.h"

#include "state_import_helpers.h"

static bool queues_equal(const t64_machine *left, const t64_machine *right) {
  size_t i;

  if (left->queue_count != right->queue_count) return false;
  for (i = 0u; i < left->queue_count; i++) {
    const t64_event *a =
        &left->events[(left->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];
    const t64_event *b =
        &right->events[(right->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];

    if (a->tick != b->tick || a->sequence != b->sequence ||
        a->device != b->device || a->kind != b->kind || a->value != b->value ||
        a->token != b->token) {
      return false;
    }
  }
  return true;
}

/* The ten production handlers by (device, kind), written out independently of
 * the importer's own table. */
static t64_event_handler_fn expected_handler(uint32_t device, uint32_t kind) {
  if (device == T64_DEVICE_CPU && kind == T64_CPU_EVENT_STEP) {
    return t64_cpu_step;
  }
  if (device == T64_DEVICE_PI && kind == T64_PI_EVENT_COMPLETE) {
    return t64_pi_complete;
  }
  if (device == T64_DEVICE_SP && kind == T64_SP_EVENT_COMPLETE) {
    return t64_sp_complete;
  }
  if (device == T64_DEVICE_SP && kind == T64_SP_EVENT_RSP_ISSUE) {
    return t64_rsp_issue_event_handler();
  }
  if (device == T64_RDP_DEVICE && kind == T64_RDP_EVENT_WORK) {
    return t64_rdp_event;
  }
  if (device == T64_VI_DEVICE && kind == T64_VI_EVENT_FIELD) {
    return t64_vi_event;
  }
  if (device == T64_AI_DEVICE && kind == T64_AI_EVENT_SAMPLE) {
    return t64_ai_event;
  }
  if (device == T64_TRACE_DEVICE_INPUT &&
      kind == T64_TRACE_KIND_INPUT_SUBMITTED) {
    return t64_input_event_handler;
  }
  if (device == T64_SI_DEVICE && kind == T64_SI_EVENT_EXECUTE) {
    return t64_si_execute_event;
  }
  if (device == T64_SI_DEVICE && kind == T64_SI_EVENT_COMPLETE) {
    return t64_si_complete_event;
  }
  return NULL;
}

static bool handlers_match_production(const t64_machine *machine) {
  size_t i;

  for (i = 0u; i < machine->queue_count; i++) {
    const t64_event *event =
        &machine->events[(machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];
    const t64_event_handler_fn want =
        expected_handler(event->device, event->kind);

    if (want == NULL || event->handler != want) return false;
  }
  return true;
}

static void print_digest(const char *label, const uint8_t digest[T64_DIGEST_SIZE]) {
  size_t i;

  fprintf(stderr, "%s: ", label);
  for (i = 0u; i < T64_DIGEST_SIZE; i++) fprintf(stderr, "%02x", digest[i]);
  fputc('\n', stderr);
}

/* ---- case roundtrip (T1) --------------------------------------------- */

static void case_roundtrip(void) {
  t64_machine *source = build_s1(SI_STEM_ID);
  t64_machine *peer;
  t64_state_import_report report;
  uint8_t source_digest[T64_DIGEST_SIZE];
  uint8_t peer_digest[T64_DIGEST_SIZE];
  uint8_t *blob;
  size_t length = 0u;
  size_t allocate_before;
  size_t release_before;
  t64_status status;

  if (source == NULL) return;
  blob = export_blob(source, &length);
  if (blob == NULL) {
    t64_machine_destroy(source);
    return;
  }
  (void)digest_of(source, source_digest);
  peer = create_admitted(SI_STEM_ID);
  if (peer == NULL) {
    free(blob);
    t64_machine_destroy(source);
    return;
  }
  T64_CHECK_INT(t64_machine_trace_set_enabled(peer, true), T64_STATUS_OK);
  /* reached: the peer holds trace records of its own before the import, so a
   * trace that survived the import would be seen */
  T64_CHECK(advance_to(peer, 100u));
  T64_CHECK(peer->trace_count > 0u);
  allocate_before = allocate_calls;
  release_before = release_calls;
  (void)memset(&report, 0xA5, sizeof report);
  status = t64_machine_state_import(peer, blob, length, 0u, &report);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(report.status, T64_STATUS_OK);
  T64_CHECK_INT(report.reason, T64_STATE_REASON_NONE);
  (void)digest_of(peer, peer_digest);
  if (memcmp(source_digest, peer_digest, sizeof source_digest) != 0) {
    print_digest("source digest", source_digest);
    print_digest("peer digest  ", peer_digest);
  }
  T64_CHECK(memcmp(source_digest, peer_digest, sizeof source_digest) == 0);
  T64_CHECK(memcmp(blob + D_DECLARED, peer_digest, sizeof peer_digest) == 0);
  T64_CHECK_INT(rd64(blob + D_TOTAL), length);
  T64_CHECK_INT(rd64(blob + D_TICK), peer->tick);
  T64_CHECK(queues_equal(source, peer));
  T64_CHECK(handlers_match_production(peer));
  T64_CHECK_INT(peer->queue_head, 0u);
  T64_CHECK_INT(peer->trace_count, 0u);
  T64_CHECK(peer->trace_enabled);
  T64_CHECK(t64_state_summary_is_clear(peer));
  T64_CHECK_INT(allocate_calls, allocate_before);
  T64_CHECK_INT(release_calls, release_before);
  free(blob);
  t64_machine_destroy(peer);
  t64_machine_destroy(source);
}

/* ---- case handler-table-ledger -------------------------------------- */

/* The ten production pairs, authored as literals from the 09-01 event ledger
 * (core/tests/test_sched_ledger.c) without the synthetic test device. */
static const struct {
  uint32_t device;
  uint32_t kind;
} ledger_pairs[] = {{5u, 1u}, {3u, 1u}, {4u, 1u},  {4u, 2u}, {6u, 1u},
                    {7u, 1u}, {8u, 1u}, {9u, 1u},  {10u, 1u}, {10u, 2u}};

static bool is_ledger_pair(uint32_t device, uint32_t kind) {
  size_t i;

  for (i = 0u; i < sizeof ledger_pairs / sizeof ledger_pairs[0]; i++) {
    if (ledger_pairs[i].device == device && ledger_pairs[i].kind == kind) {
      return true;
    }
  }
  return false;
}

static void case_handler_table_ledger(void) {
  uint32_t device;
  uint32_t kind;
  size_t non_null = 0u;
  static const uint32_t synthetic_devices[] = {UINT32_C(0xffff0001),
                                               UINT32_C(0xffff0002)};
  size_t i;

  for (device = 0u; device < 16u; device++) {
    for (kind = 0u; kind < 16u; kind++) {
      const t64_event_handler_fn handler =
          t64_state_handler_lookup(device, kind);

      if (handler != NULL) non_null++;
      T64_CHECK((handler != NULL) == is_ledger_pair(device, kind));
      T64_CHECK(handler == expected_handler(device, kind));
    }
  }
  T64_CHECK_INT(non_null, 10u);
  /* The synthetic test device and the MI test device are refused for every
   * kind: the closed table is the production ledger only. */
  for (i = 0u; i < sizeof synthetic_devices / sizeof synthetic_devices[0];
       i++) {
    for (kind = 0u; kind < 16u; kind++) {
      T64_CHECK(t64_state_handler_lookup(synthetic_devices[i], kind) == NULL);
    }
  }
  T64_CHECK(t64_state_handler_lookup(UINT32_MAX, UINT32_MAX) == NULL);
}

/* ---- continuation (T3) ------------------------------------------------- */


/* One source against one peer imported from it: equal digests and equal
 * drained ISViewer bytes at the import tick, at +310 and at +30,000. */
static void continuation_pair(uint64_t partition) {
  t64_machine *source = build_s1(SI_STEM_ID);
  t64_machine *peer;
  uint8_t *blob;
  size_t length = 0u;
  t64_state_import_report report;
  uint64_t base;
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t source_bytes[4096];
  uint8_t peer_bytes[4096];
  size_t source_copied = 0u;
  size_t peer_copied = 0u;

  if (source == NULL) return;
  blob = export_blob(source, &length);
  peer = create_admitted(SI_STEM_ID);
  if (blob == NULL || peer == NULL) {
    free(blob);
    if (peer != NULL) t64_machine_destroy(peer);
    t64_machine_destroy(source);
    return;
  }
  T64_CHECK_INT(t64_machine_state_import(peer, blob, length, 0u, &report),
                T64_STATUS_OK);
  base = peer->tick;
  T64_CHECK_INT(base, source->tick);
  T64_CHECK(digests_equal(source, peer));
  (void)digest_of(source, before);
  if (advance_pair(source, peer, base + 310u, partition)) {
    T64_CHECK(digests_equal(source, peer));
  }
  if (advance_pair(source, peer, base + 30000u, 0u)) {
    uint8_t after[T64_DIGEST_SIZE];

    T64_CHECK(digests_equal(source, peer));
    /* The comparison is not vacuous: the state moved, the read completed and
     * the source and the peer both show it. */
    (void)digest_of(source, after);
    T64_CHECK(memcmp(before, after, sizeof before) != 0);
    T64_CHECK(!source->si.active_valid && !peer->si.active_valid);
    T64_CHECK((source->mi_pending & T64_MI_INTERRUPT_SI) != 0u);
    T64_CHECK((peer->mi_pending & T64_MI_INTERRUPT_SI) != 0u);
  }
  if (drain_output(source, source_bytes, &source_copied) &&
      drain_output(peer, peer_bytes, &peer_copied)) {
    T64_CHECK_INT(source_copied, peer_copied);
    T64_CHECK(memcmp(source_bytes, peer_bytes, source_copied) == 0);
  }
  free(blob);
  t64_machine_destroy(peer);
  t64_machine_destroy(source);
}

static void case_continuation_roundtrip(void) {
  static const uint64_t partitions[] = {0u, 1u, 7u, 31u};
  size_t i;

  for (i = 0u; i < sizeof partitions / sizeof partitions[0]; i++) {
    continuation_pair(partitions[i]);
  }
}

/* ======================================================================== */
/* Task 2: refusals that leave the live machine bit-identical               */
/* ======================================================================== */


/* ---- the target machines and their accepted blobs ----------------------- */

typedef enum { T_S1 = 0, T_E4K, T_S32K, T_COUNT } target_id;

static const char *const target_profile[T_COUNT] = {SI_STEM_ID, SAVE_E4K_ID,
                                                    SAVE_S32K_ID};
static t64_machine *target_source[T_COUNT];
static uint8_t *target_blob[T_COUNT];
static size_t target_length[T_COUNT];

static bool targets_prepare(void) {
  size_t i;

  for (i = 0u; i < T_COUNT; i++) {
    if (target_blob[i] != NULL) continue;
    if (i == T_S1) {
      target_source[i] = build_s1(target_profile[i]);
    } else {
      target_source[i] = create_admitted(target_profile[i]);
      if (target_source[i] != NULL) T64_CHECK(advance_to(target_source[i], 2000u));
    }
    if (target_source[i] == NULL) return false;
    target_blob[i] = export_blob(target_source[i], &target_length[i]);
    if (target_blob[i] == NULL) return false;
    T64_CHECK(blob_consistent(target_blob[i], target_length[i]));
    {
      /* the structural encoder reproduces the exporter's bytes */
      tblob decoded;
      size_t again_length = 0u;
      uint8_t *again;

      T64_CHECK(tblob_decode(target_blob[i], target_length[i], &decoded));
      again = tblob_encode(&decoded, &again_length);
      T64_CHECK(again != NULL && again_length == target_length[i] &&
                memcmp(again, target_blob[i], again_length) == 0);
      free(again);
      tblob_free(&decoded);
    }
  }
  return true;
}

static void targets_release(void) {
  size_t i;

  for (i = 0u; i < T_COUNT; i++) {
    free(target_blob[i]);
    target_blob[i] = NULL;
    if (target_source[i] != NULL) t64_machine_destroy(target_source[i]);
    target_source[i] = NULL;
  }
}

static uint8_t *dup_target(target_id target, size_t *length_out) {
  uint8_t *copy = (uint8_t *)malloc(target_length[target]);

  if (copy == NULL) return NULL;
  (void)memcpy(copy, target_blob[target], target_length[target]);
  *length_out = target_length[target];
  return copy;
}

/* The body (offset and length) of a section in a blob by its id. */
static bool find_section(const uint8_t *blob, uint32_t id, size_t *offset,
                         size_t *length) {
  const uint32_t count = rd32(blob + 0x018u);
  uint32_t i;

  for (i = 0u; i < count; i++) {
    const uint8_t *entry = blob + D_TABLE + (size_t)D_ENTRY * i;

    if (rd32(entry + E_ID) == id) {
      *offset = (size_t)rd64(entry + E_OFF);
      *length = (size_t)rd64(entry + E_LEN);
      return true;
    }
  }
  return false;
}

/* ---- rows --------------------------------------------------------------- */

typedef uint8_t *(*mutator)(target_id target, size_t *length_out);

typedef struct {
  const char *name;
  target_id target;
  uint32_t section; /* 0: the offset is absolute in the blob */
  size_t offset;
  unsigned width;   /* 1, 4 or 8 bytes written (0: none) */
  uint64_t value;
  unsigned resign;  /* RS_* flags applied after the patch */
  mutator custom;   /* a structural edit instead of the patch, or NULL */
  t64_status status;
  uint32_t reason;
  uint32_t section_id;
  uint32_t rule;
} row;

static uint8_t *build_row_blob(const row *r, size_t *length_out) {
  uint8_t *blob;

  if (r->custom != NULL) return r->custom(r->target, length_out);
  blob = dup_target(r->target, length_out);
  if (blob != NULL && r->width != 0u) {
    size_t base = 0u;
    size_t body = 0u;

    if (r->section != 0u && !find_section(blob, r->section, &base, &body)) {
      T64_CHECK(false);
      free(blob);
      return NULL;
    }
    if (r->section != 0u && r->offset + r->width > body) {
      T64_CHECK(false);
      free(blob);
      return NULL;
    }
    if (r->width == 1u) blob[base + r->offset] = (uint8_t)r->value;
    if (r->width == 4u) put32(blob + base + r->offset, (uint32_t)r->value);
    if (r->width == 8u) put64(blob + base + r->offset, r->value);
    if (r->resign != 0u) {
      if (r->section == 0u) {
        /* a header edit changes no body: only the trailer covers it */
        blob_resign(blob, *length_out, r->resign & RS_TRAILER);
      } else {
        if ((r->resign & RS_SECTIONS) != 0u) {
          blob_resign_entry(blob, *length_out, r->section);
        }
        blob_resign(blob, *length_out, r->resign & (RS_DECLARED | RS_TRAILER));
      }
    }
  }
  return blob;
}

/* Scheduler section helpers for the structural rows. */
#define Q_HEADER 16u
#define Q_EVENT 40u

static uint8_t *scheduler_body(tblob *t, size_t *index_out) {
  size_t i;

  for (i = 0u; i < t->count; i++) {
    if (t->section[i].id == T64_STATE_SECTION_SCHEDULER) {
      *index_out = i;
      return t->section[i].body;
    }
  }
  return NULL;
}

/* Find the queue record of (device, kind); returns the event index or -1. */
static long queue_find(const uint8_t *body, uint32_t device, uint32_t kind) {
  const uint32_t count = rd32(body);
  uint32_t i;

  for (i = 0u; i < count; i++) {
    const uint8_t *event = body + Q_HEADER + (size_t)Q_EVENT * i;

    if (rd32(event + 16u) == device && rd32(event + 20u) == kind) {
      return (long)i;
    }
  }
  return -1;
}

static bool queue_remove(tblob *t, uint32_t device, uint32_t kind) {
  size_t index;
  uint8_t *body = scheduler_body(t, &index);
  long at;
  uint32_t count;

  if (body == NULL) return false;
  at = queue_find(body, device, kind);
  if (at < 0) return false;
  count = rd32(body);
  (void)memmove(body + Q_HEADER + (size_t)Q_EVENT * (size_t)at,
                body + Q_HEADER + (size_t)Q_EVENT * ((size_t)at + 1u),
                (size_t)Q_EVENT * (count - (uint32_t)at - 1u));
  put32(body, count - 1u);
  t->section[index].length -= Q_EVENT;
  return true;
}

/* Append an event after the last one: its tick is the last tick plus ten and
 * its sequence the last sequence, so the order stays strict. */
static bool queue_append(tblob *t, uint32_t device, uint32_t kind,
                         uint64_t value, uint64_t token) {
  size_t index;
  uint8_t *body = scheduler_body(t, &index);
  uint32_t count;
  uint8_t *grown;
  uint8_t *event;
  uint64_t tick = 0u;
  uint64_t sequence = 0u;

  if (body == NULL) return false;
  count = rd32(body);
  if (count > 0u) {
    const uint8_t *last = body + Q_HEADER + (size_t)Q_EVENT * (count - 1u);

    tick = rd64(last);
    sequence = rd64(last + 8u);
  }
  grown = (uint8_t *)realloc(body, t->section[index].length + Q_EVENT);
  if (grown == NULL) return false;
  t->section[index].body = grown;
  t->section[index].length += Q_EVENT;
  put32(grown, count + 1u);
  event = grown + Q_HEADER + (size_t)Q_EVENT * count;
  put64(event, tick + 10u);
  put64(event + 8u, sequence);
  put32(event + 16u, device);
  put32(event + 20u, kind);
  put64(event + 24u, value);
  put64(event + 32u, token);
  return true;
}

static uint8_t *encode_edited(tblob *t, size_t *length_out) {
  uint8_t *blob = tblob_encode(t, length_out);

  tblob_free(t);
  return blob;
}

static uint8_t *edit_queue_remove(target_id target, uint32_t device,
                                  uint32_t kind, size_t *length_out) {
  tblob t;

  if (!tblob_decode(target_blob[target], target_length[target], &t)) {
    return NULL;
  }
  if (!queue_remove(&t, device, kind)) {
    tblob_free(&t);
    return NULL;
  }
  return encode_edited(&t, length_out);
}

static uint8_t *edit_queue_append(target_id target, uint32_t device,
                                  uint32_t kind, uint64_t value,
                                  uint64_t token, size_t *length_out) {
  tblob t;

  if (!tblob_decode(target_blob[target], target_length[target], &t)) {
    return NULL;
  }
  if (!queue_append(&t, device, kind, value, token)) {
    tblob_free(&t);
    return NULL;
  }
  return encode_edited(&t, length_out);
}

/* One named structural mutator per row; each calls the shared editors. */
#define EDIT_REMOVE(name, device, kind)                                       \
  static uint8_t *name(target_id target, size_t *length_out) {                \
    return edit_queue_remove(target, device, kind, length_out);               \
  }
#define EDIT_APPEND(name, device, kind, value, token)                         \
  static uint8_t *name(target_id target, size_t *length_out) {                \
    return edit_queue_append(target, device, kind, value, token, length_out); \
  }

EDIT_REMOVE(mut_remove_cpu, 5u, 1u)
EDIT_REMOVE(mut_remove_si_complete, 10u, 2u)
EDIT_REMOVE(mut_remove_si_execute, 10u, 1u)
EDIT_APPEND(mut_append_cpu, 5u, 1u, 0u, 0u)
EDIT_APPEND(mut_append_pi, 3u, 1u, 0u, 0u)
EDIT_APPEND(mut_append_sp_complete, 4u, 1u, 0u, 0u)
EDIT_APPEND(mut_append_rsp, 4u, 2u, 0u, 0u)
EDIT_APPEND(mut_append_rdp, 7u, 1u, 0u, 0u)
EDIT_APPEND(mut_append_vi, 8u, 1u, 0u, 0u)
EDIT_APPEND(mut_append_ai, 9u, 1u, 0u, 0u)
EDIT_APPEND(mut_append_si_execute, 10u, 1u, 0u, 1u)
EDIT_APPEND(mut_append_input_port, 6u, 1u, UINT64_C(1) << 32, 0u)
EDIT_APPEND(mut_append_synthetic, 0xffff0001u, 1u, 0u, 0u)

/* Swap the first two queue records (the order becomes descending). */
static uint8_t *mut_swap_events(target_id target, size_t *length_out) {
  tblob t;
  size_t index;
  uint8_t *body;
  uint8_t tmp[Q_EVENT];

  if (!tblob_decode(target_blob[target], target_length[target], &t)) return NULL;
  body = scheduler_body(&t, &index);
  if (body == NULL || rd32(body) < 2u) {
    tblob_free(&t);
    return NULL;
  }
  (void)memcpy(tmp, body + Q_HEADER, Q_EVENT);
  (void)memcpy(body + Q_HEADER, body + Q_HEADER + Q_EVENT, Q_EVENT);
  (void)memcpy(body + Q_HEADER + Q_EVENT, tmp, Q_EVENT);
  return encode_edited(&t, length_out);
}

/* Two equal tuples: the second record becomes a copy of the first. */
static uint8_t *mut_equal_tuples(target_id target, size_t *length_out) {
  tblob t;
  size_t index;
  uint8_t *body;

  if (!tblob_decode(target_blob[target], target_length[target], &t)) return NULL;
  body = scheduler_body(&t, &index);
  if (body == NULL || rd32(body) < 2u) {
    tblob_free(&t);
    return NULL;
  }
  (void)memcpy(body + Q_HEADER + Q_EVENT, body + Q_HEADER, Q_EVENT);
  return encode_edited(&t, length_out);
}

/* The last queue record gets the given sequence relative to next_sequence. */
static uint8_t *set_last_sequence(target_id target, int64_t delta,
                                  size_t *length_out) {
  tblob t;
  size_t index;
  uint8_t *body;
  uint8_t *last;

  if (!tblob_decode(target_blob[target], target_length[target], &t)) return NULL;
  body = scheduler_body(&t, &index);
  if (body == NULL || rd32(body) < 1u) {
    tblob_free(&t);
    return NULL;
  }
  last = body + Q_HEADER + (size_t)Q_EVENT * (rd32(body) - 1u);
  put64(last + 8u, (uint64_t)((int64_t)rd64(body + 4u) + delta));
  return encode_edited(&t, length_out);
}

static uint8_t *mut_sequence_equal(target_id target, size_t *length_out) {
  return set_last_sequence(target, 0, length_out);
}

static uint8_t *mut_sequence_above(target_id target, size_t *length_out) {
  return set_last_sequence(target, 5, length_out);
}

static uint8_t *mut_tick_below(target_id target, size_t *length_out) {
  tblob t;
  size_t index;
  uint8_t *body;

  if (!tblob_decode(target_blob[target], target_length[target], &t)) return NULL;
  body = scheduler_body(&t, &index);
  if (body == NULL || rd32(body) < 1u) {
    tblob_free(&t);
    return NULL;
  }
  put64(body + Q_HEADER, rd64(t.header + 0x068u) - 1u);
  return encode_edited(&t, length_out);
}

static uint8_t *mut_event_kind(target_id target, uint32_t device,
                               uint32_t kind, size_t *length_out) {
  tblob t;
  size_t index;
  uint8_t *body;

  if (!tblob_decode(target_blob[target], target_length[target], &t)) return NULL;
  body = scheduler_body(&t, &index);
  if (body == NULL || rd32(body) < 1u) {
    tblob_free(&t);
    return NULL;
  }
  put32(body + Q_HEADER + 16u, device);
  put32(body + Q_HEADER + 20u, kind);
  return encode_edited(&t, length_out);
}

static uint8_t *mut_pair_outside(target_id target, size_t *length_out) {
  return mut_event_kind(target, 5u, 7u, length_out);
}

static uint8_t *mut_pair_synthetic(target_id target, size_t *length_out) {
  return mut_event_kind(target, 0xffff0001u, 1u, length_out);
}

/* Token edits on the SI complete record: skew the token, move the tick. */
static uint8_t *mut_si_complete_field(target_id target, bool token,
                                      size_t *length_out) {
  tblob t;
  size_t index;
  uint8_t *body;
  long at;
  uint8_t *event;

  if (!tblob_decode(target_blob[target], target_length[target], &t)) return NULL;
  body = scheduler_body(&t, &index);
  at = body == NULL ? -1 : queue_find(body, 10u, 2u);
  if (at < 0) {
    tblob_free(&t);
    return NULL;
  }
  event = body + Q_HEADER + (size_t)Q_EVENT * (size_t)at;
  if (token) {
    put64(event + 32u, rd64(event + 32u) + 1u);
  } else {
    put64(event, rd64(event) + 1u);
  }
  return encode_edited(&t, length_out);
}

static uint8_t *mut_si_token_skew(target_id target, size_t *length_out) {
  return mut_si_complete_field(target, true, length_out);
}

static uint8_t *mut_si_tick_skew(target_id target, size_t *length_out) {
  return mut_si_complete_field(target, false, length_out);
}

/* A section removed or an extra one added, with every hash recomputed. */
static uint8_t *mut_remove_audio(target_id target, size_t *length_out) {
  tblob t;
  size_t i;

  if (!tblob_decode(target_blob[target], target_length[target], &t)) return NULL;
  for (i = 0u; i < t.count; i++) {
    if (t.section[i].id == T64_STATE_SECTION_AUDIO) {
      free(t.section[i].body);
      (void)memmove(&t.section[i], &t.section[i + 1u],
                    sizeof t.section[0] * (t.count - i - 1u));
      t.count--;
      return encode_edited(&t, length_out);
    }
  }
  tblob_free(&t);
  return NULL;
}

static uint8_t *mut_extra_save_section(target_id target, size_t *length_out) {
  tblob t;
  tsec extra;

  if (!tblob_decode(target_blob[target], target_length[target], &t)) return NULL;
  extra.id = T64_STATE_SECTION_SAVE_MEDIA;
  extra.length = 20u;
  extra.body = (uint8_t *)calloc(extra.length, 1u);
  if (extra.body == NULL || t.count >= 32u) {
    free(extra.body);
    tblob_free(&t);
    return NULL;
  }
  /* before the observer entry, which stays last */
  t.section[t.count] = t.section[t.count - 1u];
  t.section[t.count - 1u] = extra;
  t.count++;
  return encode_edited(&t, length_out);
}

/* Table surgery on the encoded blob, with only the trailer re-signed. */
static uint8_t *table_patch(target_id target, size_t entry, size_t field,
                            uint64_t value, bool wide, size_t *length_out) {
  uint8_t *blob = dup_target(target, length_out);
  uint8_t *at;

  if (blob == NULL) return NULL;
  at = blob + D_TABLE + (size_t)D_ENTRY * entry + field;
  if (wide) put64(at, value);
  else put32(at, (uint32_t)value);
  blob_resign(blob, *length_out, RS_TRAILER);
  return blob;
}

static uint8_t *mut_entry_unknown_id(target_id t, size_t *n) {
  return table_patch(t, 0u, E_ID, 99u, false, n);
}

static uint8_t *mut_entry_out_of_order(target_id t, size_t *n) {
  /* the third entry (id 3) takes id 1, below its predecessor */
  return table_patch(t, 2u, E_ID, 1u, false, n);
}

static uint8_t *mut_entry_duplicate(target_id t, size_t *n) {
  return table_patch(t, 2u, E_ID, 2u, false, n);
}

static uint8_t *mut_entry_flags(target_id t, size_t *n) {
  return table_patch(t, 0u, E_FLAGS, 1u, false, n);
}

static uint8_t *mut_entry_overflow(target_id t, size_t *n) {
  uint8_t *blob = table_patch(t, 0u, E_OFF, UINT64_C(0xfffffffffffffff0), true,
                              n);

  if (blob != NULL) {
    put64(blob + D_TABLE + E_LEN, 0x20u);
    blob_resign(blob, *n, RS_TRAILER);
  }
  return blob;
}

static uint8_t *mut_entry_overlap(target_id t, size_t *n) {
  uint8_t *blob = dup_target(t, n);

  if (blob == NULL) return NULL;
  /* the second entry starts inside the first body */
  put64(blob + D_TABLE + D_ENTRY + E_OFF, rd64(blob + D_TABLE + E_OFF));
  blob_resign(blob, *n, RS_TRAILER);
  return blob;
}

static uint8_t *mut_rdram_short(target_id t, size_t *n) {
  uint8_t *blob = dup_target(t, n);
  size_t offset = 0u;
  size_t length = 0u;
  size_t i;

  if (blob == NULL) return NULL;
  for (i = 0u; i < rd32(blob + 0x018u); i++) {
    uint8_t *entry = blob + D_TABLE + (size_t)D_ENTRY * i;

    if (rd32(entry + E_ID) == 3u) {
      put64(entry + E_LEN, rd64(entry + E_LEN) - 1u);
      offset = 1u;
    }
  }
  (void)length;
  T64_CHECK(offset == 1u);
  blob_resign(blob, *n, RS_TRAILER);
  return blob;
}

static uint8_t *mut_section_hash_only(target_id t, size_t *n) {
  uint8_t *blob = dup_target(t, n);
  size_t offset = 0u;
  size_t length = 0u;

  if (blob == NULL) return NULL;
  T64_CHECK(find_section(blob, 1u, &offset, &length));
  blob[offset + 20u] ^= 0x01u; /* a body bit inside section 1 */
  blob_resign(blob, *n, RS_TRAILER);
  return blob;
}

static uint8_t *mut_trailer_body_flip(target_id t, size_t *n) {
  uint8_t *blob = dup_target(t, n);
  size_t offset = 0u;
  size_t length = 0u;

  if (blob == NULL) return NULL;
  T64_CHECK(find_section(blob, 4u, &offset, &length));
  blob[offset + 100u] ^= 0x01u; /* no re-signing: the trailer is first */
  return blob;
}

static uint8_t *mut_trailer_flip(target_id t, size_t *n) {
  uint8_t *blob = dup_target(t, n);

  if (blob == NULL) return NULL;
  blob[*n - 1u] ^= 0x01u;
  return blob;
}

/* ---- the row tables ----------------------------------------------------- */

#define ROW(name, target, section, offset, width, value, resign, status,      \
            reason, section_id, rule)                                         \
  {name, target, section, offset, width, value, resign, NULL, status, reason, \
   section_id, rule}
#define CROW(name, target, fn, status, reason, section_id, rule)              \
  {name, target, 0u, 0u, 0u, 0u, 0u, fn, status, reason, section_id, rule}

#define NR T64_STATE_RULE_NONE

/* Identity binding: five single-field changes, each re-signed so integrity
 * passes and each with its own status or reason. */
static const row binding_rows[] = {
    ROW("state version", T_S1, 0u, D_STATE_VERSION, 4u, 24u, RS_ALL,
        T64_STATUS_STATE_VERSION, T64_STATE_REASON_STATE_VERSION, 0u, NR),
    ROW("profile digest", T_S1, 0u, D_PROFILE + 3u, 1u, 0x5Au, RS_ALL,
        T64_STATUS_STATE_PROFILE, T64_STATE_REASON_PROFILE_DIGEST, 0u, NR),
    ROW("content digest", T_S1, 0u, D_CONTENT_SHA + 7u, 1u, 0x5Au, RS_ALL,
        T64_STATUS_STATE_CONTENT, T64_STATE_REASON_CONTENT_DIGEST, 0u, NR),
    ROW("content length", T_S1, 0u, D_CONTENT_LEN + 7u, 1u, 0x01u, RS_ALL,
        T64_STATUS_STATE_CONTENT, T64_STATE_REASON_CONTENT_LENGTH, 0u, NR),
    ROW("build id", T_S1, 0u, D_BUILD, 1u, 0x7Eu, RS_ALL,
        T64_STATUS_STATE_BUILD, T64_STATE_REASON_BUILD_ID, 0u, NR),
};

/* Header, trailer and table rows (argument rows are separate calls). */
static const row container_rows[] = {
    ROW("magic", T_S1, 0u, D_MAGIC, 1u, 0x58u, 0u, T64_STATUS_STATE_FORMAT,
        T64_STATE_REASON_MAGIC, 0u, NR),
    ROW("container version", T_S1, 0u, D_CONTAINER_VERSION, 4u, 2u, 0u,
        T64_STATUS_STATE_FORMAT, T64_STATE_REASON_CONTAINER_VERSION, 0u, NR),
    ROW("section count zero", T_S1, 0u, 0x018u, 4u, 0u, 0u,
        T64_STATUS_STATE_FORMAT, T64_STATE_REASON_SECTION_COUNT, 0u, NR),
    ROW("section count 33", T_S1, 0u, 0x018u, 4u, 33u, 0u,
        T64_STATUS_STATE_FORMAT, T64_STATE_REASON_SECTION_COUNT, 0u, NR),
    ROW("section count max", T_S1, 0u, 0x018u, 4u, 0xFFFFFFFFu, 0u,
        T64_STATUS_STATE_FORMAT, T64_STATE_REASON_SECTION_COUNT, 0u, NR),
    ROW("header flags", T_S1, 0u, D_FLAGS, 4u, 1u, 0u,
        T64_STATUS_STATE_FORMAT, T64_STATE_REASON_HEADER_FLAGS, 0u, NR),
    ROW("header reserved byte", T_S1, 0u, 0x11Fu, 1u, 1u, 0u,
        T64_STATUS_STATE_FORMAT, T64_STATE_REASON_HEADER_RESERVED, 0u, NR),
    CROW("trailer after body flip", T_S1, mut_trailer_body_flip,
         T64_STATUS_STATE_INTEGRITY, T64_STATE_REASON_TRAILER_SHA256, 0u, NR),
    CROW("trailer bit flip", T_S1, mut_trailer_flip,
         T64_STATUS_STATE_INTEGRITY, T64_STATE_REASON_TRAILER_SHA256, 0u, NR),
    CROW("unknown section id", T_S1, mut_entry_unknown_id,
         T64_STATUS_STATE_FORMAT, T64_STATE_REASON_SECTION_ID_UNKNOWN, 99u, NR),
    CROW("ids out of order", T_S1, mut_entry_out_of_order,
         T64_STATUS_STATE_FORMAT, T64_STATE_REASON_SECTION_ORDER, 1u, NR),
    CROW("duplicate id", T_S1, mut_entry_duplicate, T64_STATUS_STATE_FORMAT,
         T64_STATE_REASON_SECTION_ORDER, 2u, NR),
    CROW("entry flags", T_S1, mut_entry_flags, T64_STATUS_STATE_FORMAT,
         T64_STATE_REASON_SECTION_FLAGS, 1u, NR),
    CROW("offset plus length overflow", T_S1, mut_entry_overflow,
         T64_STATUS_STATE_FORMAT, T64_STATE_REASON_SECTION_BOUNDS, 1u, NR),
    CROW("overlapping spans", T_S1, mut_entry_overlap, T64_STATUS_STATE_FORMAT,
         T64_STATE_REASON_SECTION_OVERLAP, 2u, NR),
    CROW("rdram length one short", T_S1, mut_rdram_short,
         T64_STATUS_STATE_FORMAT, T64_STATE_REASON_SECTION_LENGTH_EXPECTED, 3u,
         NR),
    CROW("section hash with re-signed trailer", T_S1, mut_section_hash_only,
         T64_STATUS_STATE_INTEGRITY, T64_STATE_REASON_SECTION_SHA256, 1u, NR),
    /* a body edit with the entry hashes and the trailer re-signed but the
     * declared digest left stale: the declared digest is the first failure */
    ROW("declared digest stale", T_S1, T64_STATE_SECTION_MACHINE_CORE, 8u, 8u,
        0x1234u, RS_SECTIONS | RS_TRAILER, T64_STATUS_STATE_INTEGRITY,
        T64_STATE_REASON_DECLARED_DIGEST, 0u, NR),
    CROW("mandatory section missing", T_S1, mut_remove_audio,
         T64_STATUS_STATE_FORMAT, T64_STATE_REASON_SECTION_MISSING, 9u, NR),
    CROW("save section on a profile without one", T_S1, mut_extra_save_section,
         T64_STATUS_STATE_FORMAT, T64_STATE_REASON_SECTION_ID_UNKNOWN, 11u, NR),
};

/* Offsets inside the section bodies (derived from the v25 walker field order
 * by hand and cross-checked below against the exported section lengths). */
#define S8_RENDER_TEXTURE_TILE 136u
#define S8_TRIANGLE_WORD_COUNT 196u
#define S8_TILE0_SHIFT_S 2101636u
#define S8_VI_SLOT0 2102416u
#define S8_VI_SLOT0_CLOCK_POLICY (S8_VI_SLOT0 + 144u)
#define S8_IDLE_LENGTH 2103016u
#define S5_SP_ACTIVE_END_TICK 176u
#define BLOB_TICK_S1 13000u

/* One re-signed mutant per rule id with a reader use; the declared digest is
 * recomputed, so the field rule, not an integrity check, is the first failure.
 * MASKED_AT_USE has no row: it names a consuming site and performs no check. */
static const row field_rows[] = {
    ROW("COUNT_CAP audio descriptors", T_S1, T64_STATE_SECTION_AUDIO, 96u, 4u,
        3u, RS_ALL, T64_STATUS_STATE_RANGE, T64_STATE_REASON_FIELD_RANGE,
        T64_STATE_SECTION_AUDIO, T64_STATE_RULE_COUNT_CAP),
    ROW("COUNT_CAP triangle words", T_S1, T64_STATE_SECTION_RDP_VI,
        S8_TRIANGLE_WORD_COUNT, 4u, 23u, RS_ALL, T64_STATUS_STATE_RANGE,
        T64_STATE_REASON_FIELD_RANGE, T64_STATE_SECTION_RDP_VI,
        T64_STATE_RULE_COUNT_CAP),
    ROW("COUNT_CAP joybus entries", T_S1, T64_STATE_SECTION_SI_PIF, 136u, 4u,
        6u, RS_ALL, T64_STATUS_STATE_RANGE, T64_STATE_REASON_FIELD_RANGE,
        T64_STATE_SECTION_SI_PIF, T64_STATE_RULE_COUNT_CAP),
    ROW("BOOL_01 through the shared reader", T_S1,
        T64_STATE_SECTION_INTERRUPT_PI_SP, 64u, 4u, 2u, RS_ALL,
        T64_STATUS_STATE_RANGE, T64_STATE_REASON_FIELD_RANGE,
        T64_STATE_SECTION_INTERRUPT_PI_SP, T64_STATE_RULE_BOOL_01),
    ROW("observer drain cursor above the capture", T_S1,
        T64_STATE_SECTION_OBSERVER_CONTINUITY, 0u, 8u, UINT64_C(1) << 40,
        RS_ALL, T64_STATUS_STATE_RANGE, T64_STATE_REASON_OBSERVER_RANGE,
        T64_STATE_SECTION_OBSERVER_CONTINUITY, T64_STATE_RULE_LEN_LE),
    ROW("observer video head at the capacity", T_S1,
        T64_STATE_SECTION_OBSERVER_CONTINUITY, 8u, 8u, 2u, RS_ALL,
        T64_STATUS_STATE_RANGE, T64_STATE_REASON_OBSERVER_RANGE,
        T64_STATE_SECTION_OBSERVER_CONTINUITY, T64_STATE_RULE_INDEX_LT),
    ROW("observer video count above the capacity", T_S1,
        T64_STATE_SECTION_OBSERVER_CONTINUITY, 16u, 8u, 3u, RS_ALL,
        T64_STATUS_STATE_RANGE, T64_STATE_REASON_OBSERVER_RANGE,
        T64_STATE_SECTION_OBSERVER_CONTINUITY, T64_STATE_RULE_COUNT_CAP),
    ROW("observer audio head at the capacity", T_S1,
        T64_STATE_SECTION_OBSERVER_CONTINUITY, 24u, 8u, 2u, RS_ALL,
        T64_STATUS_STATE_RANGE, T64_STATE_REASON_OBSERVER_RANGE,
        T64_STATE_SECTION_OBSERVER_CONTINUITY, T64_STATE_RULE_INDEX_LT),
    ROW("observer audio count above the capacity", T_S1,
        T64_STATE_SECTION_OBSERVER_CONTINUITY, 32u, 8u, 3u, RS_ALL,
        T64_STATUS_STATE_RANGE, T64_STATE_REASON_OBSERVER_RANGE,
        T64_STATE_SECTION_OBSERVER_CONTINUITY, T64_STATE_RULE_COUNT_CAP),
    ROW("INDEX_LT audio reservation slot", T_S1, T64_STATE_SECTION_AUDIO, 120u,
        4u, 2u, RS_ALL, T64_STATUS_STATE_RANGE, T64_STATE_REASON_FIELD_RANGE,
        T64_STATE_SECTION_AUDIO, T64_STATE_RULE_INDEX_LT),
    ROW("INDEX_LT texture tile", T_S1, T64_STATE_SECTION_RDP_VI,
        S8_RENDER_TEXTURE_TILE, 4u, 8u, RS_ALL, T64_STATUS_STATE_RANGE,
        T64_STATE_REASON_FIELD_RANGE, T64_STATE_SECTION_RDP_VI,
        T64_STATE_RULE_INDEX_LT),
    ROW("INDEX_LT joybus channel", T_S1, T64_STATE_SECTION_SI_PIF, 144u, 4u, 5u,
        RS_ALL, T64_STATUS_STATE_RANGE, T64_STATE_REASON_FIELD_RANGE,
        T64_STATE_SECTION_SI_PIF, T64_STATE_RULE_INDEX_LT),
    ROW("ENUM_CLOSED SI direction", T_S1, T64_STATE_SECTION_SI_PIF, 12u, 4u, 3u,
        RS_ALL, T64_STATUS_STATE_RANGE, T64_STATE_REASON_FIELD_RANGE,
        T64_STATE_SECTION_SI_PIF, T64_STATE_RULE_ENUM_CLOSED),
    ROW("BOOL_01 PI error", T_S1, T64_STATE_SECTION_INTERRUPT_PI_SP, 60u, 4u,
        2u, RS_ALL, T64_STATUS_STATE_RANGE, T64_STATE_REASON_FIELD_RANGE,
        T64_STATE_SECTION_INTERRUPT_PI_SP, T64_STATE_RULE_BOOL_01),
    ROW("ALIGN SI active address", T_S1, T64_STATE_SECTION_SI_PIF, 16u, 4u,
        0x3901u, RS_ALL, T64_STATUS_STATE_RANGE, T64_STATE_REASON_FIELD_RANGE,
        T64_STATE_SECTION_SI_PIF, T64_STATE_RULE_ALIGN),
    ROW("ALIGN AI staged address", T_S1, T64_STATE_SECTION_AUDIO, 0u, 4u, 4u,
        RS_ALL, T64_STATUS_STATE_RANGE, T64_STATE_REASON_FIELD_RANGE,
        T64_STATE_SECTION_AUDIO, T64_STATE_RULE_ALIGN),
    ROW("ADDR_RANGE SI active address", T_S1, T64_STATE_SECTION_SI_PIF, 16u,
        4u, 0x400000u, RS_ALL, T64_STATUS_STATE_RANGE,
        T64_STATE_REASON_FIELD_RANGE, T64_STATE_SECTION_SI_PIF,
        T64_STATE_RULE_ADDR_RANGE),
    /* the boot code has an SP DMA in flight at tick 13,000: its descriptor
     * is valid, and an end tick of zero is not after its start */
    ROW("TICK_GE SP descriptor end", T_S1, T64_STATE_SECTION_INTERRUPT_PI_SP,
        S5_SP_ACTIVE_END_TICK, 8u, 0u, RS_ALL, T64_STATUS_STATE_RANGE,
        T64_STATE_REASON_FIELD_RANGE, T64_STATE_SECTION_INTERRUPT_PI_SP,
        T64_STATE_RULE_TICK_GE),
    ROW("TICK_LE EEPROM deadline above the window", T_E4K,
        T64_STATE_SECTION_SAVE_MEDIA, 8u, 8u, 2000u + 375001u, RS_ALL,
        T64_STATUS_STATE_RANGE, T64_STATE_REASON_FIELD_RANGE,
        T64_STATE_SECTION_SAVE_MEDIA, T64_STATE_RULE_TICK_LE),
    ROW("TICK_LE EEPROM deadline maximum", T_E4K,
        T64_STATE_SECTION_SAVE_MEDIA, 8u, 8u, UINT64_MAX, RS_ALL,
        T64_STATUS_STATE_RANGE, T64_STATE_REASON_FIELD_RANGE,
        T64_STATE_SECTION_SAVE_MEDIA, T64_STATE_RULE_TICK_LE),
    ROW("TICK_LE SRAM deadline above the tick", T_S32K,
        T64_STATE_SECTION_SAVE_MEDIA, 8u, 8u, 2001u, RS_ALL,
        T64_STATUS_STATE_RANGE, T64_STATE_REASON_FIELD_RANGE,
        T64_STATE_SECTION_SAVE_MEDIA, T64_STATE_RULE_TICK_LE),
    ROW("SHIFT_LT tile shift", T_S1, T64_STATE_SECTION_RDP_VI, S8_TILE0_SHIFT_S,
        4u, 16u, RS_ALL, T64_STATUS_STATE_RANGE, T64_STATE_REASON_FIELD_RANGE,
        T64_STATE_SECTION_RDP_VI, T64_STATE_RULE_SHIFT_LT),
    ROW("LEN_LE video packet length", T_S1, T64_STATE_SECTION_RDP_VI,
        S8_VI_SLOT0 + 4u, 4u, 0x00080000u, RS_ALL, T64_STATUS_STATE_RANGE,
        T64_STATE_REASON_FIELD_RANGE, T64_STATE_SECTION_RDP_VI,
        T64_STATE_RULE_LEN_LE),
    ROW("LEN_LE audio packet length", T_S1, T64_STATE_SECTION_AUDIO, 88u, 8u,
        0x40000u, RS_ALL, T64_STATUS_STATE_RANGE, T64_STATE_REASON_FIELD_RANGE,
        T64_STATE_SECTION_AUDIO, T64_STATE_RULE_LEN_LE),
    ROW("PROFILE_DERIVED rdram size", T_S1, T64_STATE_SECTION_RDRAM, 0u, 4u,
        0x200000u, RS_ALL, T64_STATUS_STATE_RANGE,
        T64_STATE_REASON_FIELD_DERIVED, T64_STATE_SECTION_RDRAM,
        T64_STATE_RULE_PROFILE_DERIVED),
    ROW("PROFILE_DERIVED save kind", T_S32K, T64_STATE_SECTION_SAVE_MEDIA, 0u,
        4u, 1u, RS_ALL, T64_STATUS_STATE_RANGE, T64_STATE_REASON_FIELD_DERIVED,
        T64_STATE_SECTION_SAVE_MEDIA, T64_STATE_RULE_PROFILE_DERIVED),
    ROW("DERIVED_EQ interrupt line", T_S1, T64_STATE_SECTION_INTERRUPT_PI_SP,
        8u, 4u, 1u, RS_ALL, T64_STATUS_STATE_RANGE,
        T64_STATE_REASON_FIELD_DERIVED, T64_STATE_SECTION_INTERRUPT_PI_SP,
        T64_STATE_RULE_DERIVED_EQ),
    ROW("DERIVED_EQ header tick", T_S1, T64_STATE_SECTION_MACHINE_CORE, 0u, 8u,
        BLOB_TICK_S1 + 1u, RS_ALL, T64_STATUS_STATE_RANGE,
        T64_STATE_REASON_FIELD_DERIVED, T64_STATE_SECTION_MACHINE_CORE,
        T64_STATE_RULE_DERIVED_EQ),
    ROW("PAD_ZERO text after the terminator", T_S1, T64_STATE_SECTION_RDP_VI,
        S8_VI_SLOT0_CLOCK_POLICY + 10u, 1u, 0x78u, RS_ALL,
        T64_STATUS_STATE_RANGE, T64_STATE_REASON_FIELD_PADDING,
        T64_STATE_SECTION_RDP_VI, T64_STATE_RULE_PAD_ZERO),
    ROW("RESERVED_ZERO interrupt pending", T_S1,
        T64_STATE_SECTION_INTERRUPT_PI_SP, 3u, 1u, 0x80u, RS_ALL,
        T64_STATUS_STATE_RANGE, T64_STATE_REASON_FIELD_RESERVED,
        T64_STATE_SECTION_INTERRUPT_PI_SP, T64_STATE_RULE_RESERVED_ZERO),
};

/* Rows accepted next to the TICK_LE refusals: the boundary, a stale value
 * below the blob tick and zero. */
static const row deadline_accept_rows[] = {
    ROW("deadline at the window", T_E4K, T64_STATE_SECTION_SAVE_MEDIA, 8u, 8u,
        2000u + 375000u, RS_ALL, T64_STATUS_OK, T64_STATE_REASON_NONE, 0u, NR),
    ROW("deadline stale below the tick", T_E4K, T64_STATE_SECTION_SAVE_MEDIA,
        8u, 8u, 1000u, RS_ALL, T64_STATUS_OK, T64_STATE_REASON_NONE, 0u, NR),
    ROW("deadline zero", T_E4K, T64_STATE_SECTION_SAVE_MEDIA, 8u, 8u, 0u,
        RS_ALL, T64_STATUS_OK, T64_STATE_REASON_NONE, 0u, NR),
    ROW("sram deadline at the tick", T_S32K, T64_STATE_SECTION_SAVE_MEDIA, 8u,
        8u, 2000u, RS_ALL, T64_STATUS_OK, T64_STATE_REASON_NONE, 0u, NR),
};

#define INV T64_STATUS_STATE_INVARIANT

#define SCHED T64_STATE_SECTION_SCHEDULER
#define IROW(name, fn, reason, rule) \
  CROW(name, T_S1, fn, INV, reason, SCHED, rule)

static const row invariant_rows[] = {
    IROW("queue not ordered", mut_swap_events, T64_STATE_REASON_QUEUE_ORDER,
         NR),
    IROW("queue equal tuples", mut_equal_tuples, T64_STATE_REASON_QUEUE_ORDER,
         NR),
    IROW("sequence equal to next", mut_sequence_equal,
         T64_STATE_REASON_QUEUE_SEQUENCE, T64_STATE_RULE_SEQ_LT),
    IROW("sequence above next", mut_sequence_above,
         T64_STATE_REASON_QUEUE_SEQUENCE, T64_STATE_RULE_SEQ_LT),
    IROW("tick one below the machine tick", mut_tick_below,
         T64_STATE_REASON_QUEUE_TICK, T64_STATE_RULE_TICK_GE),
    ROW("exhausted flag away from the maximum", T_S1, SCHED, 12u, 4u, 1u,
        RS_ALL, INV, T64_STATE_REASON_QUEUE_SEQUENCE, SCHED,
        T64_STATE_RULE_SEQ_LT),
    ROW("queue count above the capacity", T_S1, SCHED, 0u, 4u, 257u, RS_ALL,
        INV, T64_STATE_REASON_QUEUE_COUNT, SCHED, T64_STATE_RULE_COUNT_CAP),
    IROW("pair outside the closed table", mut_pair_outside,
         T64_STATE_REASON_QUEUE_KIND_UNKNOWN, T64_STATE_RULE_ENUM_CLOSED),
    IROW("synthetic test device", mut_pair_synthetic,
         T64_STATE_REASON_QUEUE_KIND_UNKNOWN, T64_STATE_RULE_ENUM_CLOSED),
    IROW("synthetic event appended", mut_append_synthetic,
         T64_STATE_REASON_QUEUE_KIND_UNKNOWN, T64_STATE_RULE_ENUM_CLOSED),
    IROW("no CPU step event", mut_remove_cpu,
         T64_STATE_REASON_DEVICE_EVENT_MISSING, NR),
    IROW("two CPU step events", mut_append_cpu,
         T64_STATE_REASON_DEVICE_EVENT_EXTRA, NR),
    IROW("SI read without its complete event", mut_remove_si_complete,
         T64_STATE_REASON_DEVICE_EVENT_MISSING, NR),
    IROW("SI complete token skewed", mut_si_token_skew,
         T64_STATE_REASON_DEVICE_TOKEN_MISMATCH, NR),
    IROW("SI complete tick skewed", mut_si_tick_skew,
         T64_STATE_REASON_DEVICE_STATE_INCONSISTENT, NR),
    IROW("SI execute pending without its event", mut_remove_si_execute,
         T64_STATE_REASON_DEVICE_EVENT_MISSING, NR),
    ROW("SI execute event while not pending", T_S1, T64_STATE_SECTION_SI_PIF,
        64u, 4u, 0u, RS_ALL, INV, T64_STATE_REASON_DEVICE_EVENT_EXTRA, SCHED,
        NR),
    IROW("extra PI event on an idle PI", mut_append_pi,
         T64_STATE_REASON_DEVICE_EVENT_EXTRA, NR),
    IROW("SP completion event on the RSP profile", mut_append_sp_complete,
         T64_STATE_REASON_DEVICE_EVENT_EXTRA, NR),
    IROW("extra RSP event on an idle RSP", mut_append_rsp,
         T64_STATE_REASON_DEVICE_EVENT_EXTRA, NR),
    IROW("extra RDP event on an idle RDP", mut_append_rdp,
         T64_STATE_REASON_DEVICE_EVENT_EXTRA, NR),
    IROW("extra VI event on a disabled VI", mut_append_vi,
         T64_STATE_REASON_DEVICE_EVENT_EXTRA, NR),
    IROW("extra AI event on an idle AI", mut_append_ai,
         T64_STATE_REASON_DEVICE_EVENT_EXTRA, NR),
    IROW("second SI execute event", mut_append_si_execute,
         T64_STATE_REASON_DEVICE_EVENT_EXTRA, NR),
    IROW("input event with a port field", mut_append_input_port,
         T64_STATE_REASON_DEVICE_STATE_INCONSISTENT, NR),
};

/* ---- the row runner ------------------------------------------------------ */

typedef struct {
  uint8_t *machine_bytes;
  size_t machine_size;
  uint8_t *video_bytes;
  size_t video_size;
  uint8_t *audio_bytes;
  size_t audio_size;
  uint8_t digest[T64_DIGEST_SIZE];
  size_t trace_count;
  uint64_t trace_drops;
  size_t queue_count;
  uint64_t next_sequence;
  uint64_t tick;
  size_t allocs;
  size_t releases;
} snapshot;

static uint8_t *copy_region(const void *data, size_t size) {
  uint8_t *copy;

  if (data == NULL || size == 0u) return NULL;
  copy = (uint8_t *)malloc(size);
  if (copy != NULL) (void)memcpy(copy, data, size);
  return copy;
}

static void snapshot_take(const t64_machine *machine, snapshot *s) {
  (void)memset(s, 0, sizeof *s);
  s->machine_size = machine->machine_storage.capacity;
  s->machine_bytes = copy_region(machine->machine_storage.data, s->machine_size);
  s->video_size = machine->video_storage.capacity;
  s->video_bytes = copy_region(machine->video_storage.data, s->video_size);
  s->audio_size = machine->audio_storage.capacity;
  s->audio_bytes = copy_region(machine->audio_storage.data, s->audio_size);
  (void)digest_of(machine, s->digest);
  s->trace_count = machine->trace_count;
  s->trace_drops = machine->trace_drop_count;
  s->queue_count = machine->queue_count;
  s->next_sequence = machine->next_sequence;
  s->tick = machine->tick;
  s->allocs = allocate_calls;
  s->releases = release_calls;
}

static void snapshot_free(snapshot *s) {
  free(s->machine_bytes);
  free(s->video_bytes);
  free(s->audio_bytes);
}

/* Whole-machine comparison: the machine allocation (the structure, RDRAM, the
 * coverage tables and the save bytes), both packet storages, the digest, the
 * trace observations, the queue, the tick and the allocator counters. */
static bool snapshot_matches(const t64_machine *machine, const snapshot *s) {
  uint8_t digest[T64_DIGEST_SIZE];
  bool same = true;

  same = same && s->machine_bytes != NULL &&
         memcmp(machine->machine_storage.data, s->machine_bytes,
                s->machine_size) == 0;
  same = same && (s->video_bytes == NULL ||
                  memcmp(machine->video_storage.data, s->video_bytes,
                         s->video_size) == 0);
  same = same && (s->audio_bytes == NULL ||
                  memcmp(machine->audio_storage.data, s->audio_bytes,
                         s->audio_size) == 0);
  same = same && digest_of(machine, digest) &&
         memcmp(digest, s->digest, sizeof digest) == 0;
  same = same && machine->trace_count == s->trace_count &&
         machine->trace_drop_count == s->trace_drops &&
         machine->queue_count == s->queue_count &&
         machine->next_sequence == s->next_sequence &&
         machine->tick == s->tick && allocate_calls == s->allocs &&
         release_calls == s->releases;
  return same;
}

#define CHECK_UNTOUCHED 1u
#define CHECK_SCRATCH 2u

static t64_machine *refusal_peer[T_COUNT];

static t64_machine *peer_for(target_id target) {
  if (refusal_peer[target] == NULL) {
    refusal_peer[target] = create_admitted(target_profile[target]);
    if (refusal_peer[target] != NULL) {
      T64_CHECK_INT(t64_machine_trace_set_enabled(refusal_peer[target], true),
                    T64_STATUS_OK);
    }
  }
  return refusal_peer[target];
}

static void peers_release(void) {
  size_t i;

  for (i = 0u; i < T_COUNT; i++) {
    if (refusal_peer[i] != NULL) t64_machine_destroy(refusal_peer[i]);
    refusal_peer[i] = NULL;
  }
}

/* One accepted baseline per target in the same setup: the unchanged blob into
 * a fresh peer, OK, equal digests. */
static void check_baselines(const bool used[T_COUNT]) {
  static bool done[T_COUNT];
  size_t i;

  for (i = 0u; i < T_COUNT; i++) {
    t64_machine *peer;
    t64_state_import_report report;

    if (!used[i] || done[i]) continue;
    done[i] = true;
    peer = create_admitted(target_profile[i]);
    if (peer == NULL) continue;
    T64_CHECK_INT(t64_machine_state_import(peer, target_blob[i],
                                           target_length[i], 0u, &report),
                  T64_STATUS_OK);
    T64_CHECK_INT(report.reason, T64_STATE_REASON_NONE);
    T64_CHECK(digests_equal(peer, target_source[i]));
    t64_machine_destroy(peer);
  }
}

static void run_row(const row *r, unsigned checks) {
  size_t length = 0u;
  uint8_t *blob = build_row_blob(r, &length);
  uint8_t *pristine;
  t64_machine *peer = peer_for(r->target);
  t64_state_import_report report;
  snapshot before;
  size_t allocs_before;
  size_t releases_before;
  t64_status status;

  if (blob == NULL || peer == NULL) {
    fprintf(stderr, "row not built: %s\n", r->name);
    T64_CHECK(false);
    free(blob);
    return;
  }
  pristine = copy_region(blob, length);
  T64_CHECK(t64_state_summary_is_clear(peer));
  if ((checks & CHECK_UNTOUCHED) != 0u) snapshot_take(peer, &before);
  (void)memset(&report, 0xA5, sizeof report);
  allocs_before = allocate_calls;
  releases_before = release_calls;
  status = t64_machine_state_import(peer, blob, length, 0u, &report);
  /* no import allocates or releases, whatever its outcome */
  T64_CHECK_INT(allocate_calls, allocs_before);
  T64_CHECK_INT(release_calls, releases_before);
  if (status != r->status || report.reason != r->reason ||
      report.section_id != r->section_id || report.rule != r->rule) {
    fprintf(stderr,
            "row %s: got status %u reason %u section %u rule %u at %llu, want "
            "status %u reason %u section %u rule %u\n",
            r->name, (unsigned)status, (unsigned)report.reason,
            (unsigned)report.section_id, (unsigned)report.rule,
            (unsigned long long)report.position, (unsigned)r->status,
            (unsigned)r->reason, (unsigned)r->section_id, (unsigned)r->rule);
  }
  T64_CHECK_INT(status, r->status);
  T64_CHECK_INT(report.status, r->status);
  T64_CHECK_INT(report.reason, r->reason);
  T64_CHECK_INT(report.section_id, r->section_id);
  T64_CHECK_INT(report.rule, r->rule);
  T64_CHECK(t64_state_summary_is_clear(peer));
  if ((checks & CHECK_UNTOUCHED) != 0u) {
    if (status != T64_STATUS_OK) {
      T64_CHECK(snapshot_matches(peer, &before));
    }
    snapshot_free(&before);
  }
  T64_CHECK(pristine != NULL && memcmp(pristine, blob, length) == 0);
  if (status == T64_STATUS_OK) {
    uint8_t digest[T64_DIGEST_SIZE];

    /* an accepted state reproduces the digest the blob declares */
    T64_CHECK(digest_of(peer, digest) &&
              memcmp(digest, blob + 0x070u, sizeof digest) == 0);
    /* an accepted row changed the peer; give the next row a fresh one */
    t64_machine_destroy(refusal_peer[r->target]);
    refusal_peer[r->target] = NULL;
  }
  free(pristine);
  free(blob);
}

static void run_rows(const row *rows, size_t count, unsigned checks) {
  bool used[T_COUNT] = {false, false, false};
  size_t i;

  for (i = 0u; i < count; i++) used[rows[i].target] = true;
  check_baselines(used);
  for (i = 0u; i < count; i++) run_row(&rows[i], checks);
}

#define ROWS(table) table, sizeof table / sizeof table[0]

/* ---- the cases ----------------------------------------------------------- */

static void case_binding_refusals(void) {
  size_t i;

  if (!targets_prepare()) return;
  run_rows(ROWS(binding_rows), CHECK_SCRATCH);
  /* five rows, five distinct (status, reason) pairs, none of them a status the
   * importer reports for another family */
  for (i = 0u; i < sizeof binding_rows / sizeof binding_rows[0]; i++) {
    size_t j;

    for (j = i + 1u; j < sizeof binding_rows / sizeof binding_rows[0]; j++) {
      T64_CHECK(binding_rows[i].reason != binding_rows[j].reason);
    }
    T64_CHECK(binding_rows[i].status > T64_STATUS_SAVE_BUSY);
  }
  peers_release();
}

static void case_container_refusals(void) {
  t64_machine *peer;
  t64_machine *bare = NULL;
  const t64_profile_descriptor *profile = NULL;
  t64_state_import_report report;
  uint8_t byte = 0u;
  const uint8_t *blob;
  size_t length;

  if (!targets_prepare()) return;
  blob = target_blob[T_S1];
  length = target_length[T_S1];
  run_rows(ROWS(container_rows), CHECK_SCRATCH);
  /* argument rows */
  peer = peer_for(T_S1);
  if (peer != NULL) {
    T64_CHECK_INT(t64_machine_state_import(NULL, blob, length, 0u, &report),
                  T64_STATUS_INVALID_ARGUMENT);
    T64_CHECK_INT(report.reason, T64_STATE_REASON_ARG_NULL);
    T64_CHECK_INT(t64_machine_state_import(peer, NULL, length, 0u, &report),
                  T64_STATUS_INVALID_ARGUMENT);
    T64_CHECK_INT(report.reason, T64_STATE_REASON_ARG_NULL);
    T64_CHECK_INT(t64_machine_state_import(peer, blob, 0u, 0u, &report),
                  T64_STATUS_INVALID_ARGUMENT);
    T64_CHECK_INT(report.reason, T64_STATE_REASON_ARG_LENGTH);
    /* one above the ceiling: the refusal comes before any byte is read */
    T64_CHECK_INT(t64_machine_state_import(peer, &byte,
                                           (size_t)T64_STATE_MAX_BYTES + 1u,
                                           0u, &report),
                  T64_STATUS_INVALID_ARGUMENT);
    T64_CHECK_INT(report.reason, T64_STATE_REASON_ARG_LENGTH);
    T64_CHECK_INT(t64_machine_state_import(peer, blob, length, 2u, &report),
                  T64_STATUS_INVALID_ARGUMENT);
    T64_CHECK_INT(report.reason, T64_STATE_REASON_ARG_FLAGS);
    /* a span inside the live RDRAM and inside the admitted image */
    T64_CHECK_INT(t64_machine_state_import(peer, peer->ram, 4096u, 0u, &report),
                  T64_STATUS_INVALID_ARGUMENT);
    T64_CHECK_INT(report.reason, T64_STATE_REASON_ARG_OVERLAP_LIVE);
    T64_CHECK_INT(t64_machine_state_import(
                      peer, (const uint8_t *)peer->image_storage.data, 4096u,
                      0u, &report),
                  T64_STATUS_INVALID_ARGUMENT);
    T64_CHECK_INT(report.reason, T64_STATE_REASON_ARG_OVERLAP_LIVE);
    /* a report span inside live storage is refused and never written */
    {
      uint8_t before[sizeof report];
      t64_state_import_report *inside =
          (t64_state_import_report *)(void *)(peer->ram + 64u);

      (void)memcpy(before, inside, sizeof before);
      T64_CHECK_INT(t64_machine_state_import(peer, blob, length, 0u, inside),
                    T64_STATUS_INVALID_ARGUMENT);
      T64_CHECK(memcmp(before, inside, sizeof before) == 0);
    }
    /* a NULL report is allowed: the call still refuses by status */
    T64_CHECK_INT(t64_machine_state_import(peer, blob, 0u, 0u, NULL),
                  T64_STATUS_INVALID_ARGUMENT);
  }
  /* no admitted image */
  T64_CHECK_INT(t64_profile_lookup(SI_STEM_ID, &profile), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &bare), T64_STATUS_OK);
  if (bare != NULL) {
    T64_CHECK_INT(t64_machine_state_import(bare, blob, length, 0u, &report),
                  T64_STATUS_NOT_READY);
    T64_CHECK_INT(report.reason, T64_STATE_REASON_NOT_ADMITTED);
    T64_CHECK(t64_state_summary_is_clear(bare));
    t64_machine_destroy(bare);
  }
  peers_release();
}

static void case_field_range_refusals(void) {
  size_t offset = 0u;
  size_t length = 0u;

  if (!targets_prepare()) return;
  /* the hand-derived offsets must agree with the exported layout */
  T64_CHECK(find_section(target_blob[T_S1], T64_STATE_SECTION_RDP_VI, &offset,
                         &length));
  T64_CHECK_INT(length, S8_IDLE_LENGTH);
  T64_CHECK(find_section(target_blob[T_S1], T64_STATE_SECTION_INTERRUPT_PI_SP,
                         &offset, &length));
  T64_CHECK_INT(length, 276u);
  T64_CHECK(find_section(target_blob[T_S1], T64_STATE_SECTION_SI_PIF, &offset,
                         &length));
  T64_CHECK_INT(length, 404u);
  T64_CHECK(find_section(target_blob[T_S1], T64_STATE_SECTION_AUDIO, &offset,
                         &length));
  T64_CHECK_INT(length, 336u);
  T64_CHECK_INT(rd64(target_blob[T_S1] + 0x068u), BLOB_TICK_S1);
  run_rows(ROWS(field_rows), CHECK_SCRATCH);
  run_rows(ROWS(deadline_accept_rows), CHECK_SCRATCH);
  peers_release();
}

static void case_invariant_accepts(void) {
  static const uint64_t points[] = {0u, 1u, 2u, 3u};
  size_t i;

  for (i = 0u; i < sizeof points / sizeof points[0]; i++) {
    t64_machine *source = NULL;
    t64_machine *peer;
    t64_state_import_report report;
    uint8_t *blob;
    size_t length = 0u;

    switch (points[i]) {
    case 0u: /* idle at tick 2,000 */
      source = create_admitted(SI_STEM_ID);
      if (source != NULL) T64_CHECK(advance_to(source, 2000u));
      break;
    case 1u: /* WR64B in flight */
      source = create_admitted(SI_STEM_ID);
      if (source != NULL) {
        T64_CHECK(advance_to(source, 2000u));
        bus_write_bytes(source, UINT32_C(0x3800), poll_frame, 64u);
        T64_CHECK_INT(t64_machine_bus_write(source, SI_DRAM_ADDR, 4u, 0x3800u),
                      T64_STATUS_OK);
        T64_CHECK_INT(t64_machine_bus_write(source, SI_WR64B, 4u, PIF_ADDRESS),
                      T64_STATUS_OK);
        T64_CHECK(advance_to(source, 3000u));
        T64_CHECK(source->si.active_valid);
        T64_CHECK_INT(source->si.active_direction, T64_SI_DIRECTION_WR64B);
      }
      break;
    case 2u: /* RD64B before E */
      source = build_s1(SI_STEM_ID);
      break;
    default: /* RD64B between E and C */
      source = build_s1(SI_STEM_ID);
      if (source != NULL) {
        T64_CHECK(advance_to(source, 24000u));
        T64_CHECK(source->si.active_valid && !source->si.execute_pending);
        T64_CHECK_INT(source->si.active_direction, T64_SI_DIRECTION_RD64B);
      }
      break;
    }
    if (source == NULL) continue;
    blob = export_blob(source, &length);
    peer = create_admitted(SI_STEM_ID);
    if (blob != NULL && peer != NULL) {
      T64_CHECK(blob_consistent(blob, length));
      T64_CHECK_INT(t64_machine_state_import(peer, blob, length, 0u, &report),
                    T64_STATUS_OK);
      T64_CHECK_INT(report.reason, T64_STATE_REASON_NONE);
      T64_CHECK(digests_equal(peer, source));
      T64_CHECK(t64_state_summary_is_clear(peer));
    }
    free(blob);
    if (peer != NULL) t64_machine_destroy(peer);
    t64_machine_destroy(source);
  }
}

static void case_invariant_refusals(void) {
  if (!targets_prepare()) return;
  run_rows(ROWS(invariant_rows), CHECK_SCRATCH);
  peers_release();
}

/* The scratch is storage of the machine, never a frame: its size is bounded,
 * it is clear before and after every accepted and refused import above, and no
 * import calls the allocator or releases anything. */
static void case_scratch_storage(void) {
  t64_machine *peer;
  t64_state_import_report report;
  size_t allocs;
  size_t releases;

  T64_CHECK(sizeof(t64_state_summary) <= T64_STATE_SUMMARY_MAX_BYTES);
  T64_CHECK(sizeof(t64_state_summary) <= 8192u);
  T64_CHECK_INT(sizeof(((t64_machine *)0)->state_summary),
                sizeof(t64_state_summary));
  if (!targets_prepare()) return;
  peer = create_admitted(SI_STEM_ID);
  if (peer == NULL) return;
  T64_CHECK(t64_state_summary_is_clear(peer));
  allocs = allocate_calls;
  releases = release_calls;
  T64_CHECK_INT(t64_machine_state_import(peer, target_blob[T_S1],
                                         target_length[T_S1], 0u, &report),
                T64_STATUS_OK);
  T64_CHECK(t64_state_summary_is_clear(peer));
  T64_CHECK_INT(allocate_calls, allocs);
  T64_CHECK_INT(release_calls, releases);
  t64_machine_destroy(peer);
  /* Every row of the four refusal cases asserts the scratch and the allocator
   * counters around its own call (run_row); these representatives cover each
   * refusal family once more in this case: a binding row, a trailer row, a
   * table row, a field row and an invariant row. */
  run_rows(binding_rows, 2u, CHECK_SCRATCH);
  run_rows(&container_rows[7], 3u, CHECK_SCRATCH);
  run_rows(&field_rows[0], 1u, CHECK_SCRATCH);
  run_rows(&invariant_rows[0], 1u, CHECK_SCRATCH);
  peers_release();
}

/* Every refusal row of the four refusal tables: the whole machine allocation,
 * both packet storages, the digest, the trace observations, the queue, the tick
 * and the allocator counters are equal before and after, and the caller blob
 * is unchanged. */
static void case_refusal_untouched(void) {
  if (!targets_prepare()) return;
  run_rows(ROWS(binding_rows), CHECK_UNTOUCHED | CHECK_SCRATCH);
  run_rows(ROWS(container_rows), CHECK_UNTOUCHED | CHECK_SCRATCH);
  run_rows(ROWS(field_rows), CHECK_UNTOUCHED | CHECK_SCRATCH);
  run_rows(ROWS(invariant_rows), CHECK_UNTOUCHED | CHECK_SCRATCH);
  peers_release();
}


/* ======================================================================== */
/* Task 3: the same-build ladder                                            */
/* ======================================================================== */

#define ISVIEWER_BASE UINT32_C(0x13ff0000)
#define SRAM_WORD_A UINT32_C(0x08000000)
#define SRAM_WORD_B UINT32_C(0x08007FFC)
#define VI_BASE UINT32_C(0x04400000)
#define VI_ORIGIN UINT32_C(0x3000)

/* Host writes into the ISViewer buffer window (the observable guest output). */
static void isviewer_emit(t64_machine *machine, const uint8_t *bytes,
                          size_t count) {
  size_t i;

  for (i = 0u; i + 4u <= count; i += 4u) {
    T64_CHECK_INT(t64_machine_bus_write(machine,
                                        ISVIEWER_BASE + UINT32_C(0x20) +
                                            (uint32_t)i,
                                        4u, rd32(bytes + i)),
                  T64_STATUS_OK);
  }
  T64_CHECK_INT(t64_machine_bus_write(machine, ISVIEWER_BASE + UINT32_C(0x14),
                                      4u, (uint32_t)count),
                T64_STATUS_OK);
}

static uint64_t save_generation(t64_machine *machine, uint32_t kind) {
  t64_save_info info;

  if (kind == 0u) return machine->save.write_generation;
  T64_CHECK_INT(t64_machine_save_info(machine, kind, &info), T64_STATUS_OK);
  return info.write_generation;
}

/* ---- case idempotence (T2) --------------------------------------------- */

static void idempotence_of(t64_machine *source, const char *profile_id) {
  t64_machine *peer = create_admitted(profile_id);
  t64_state_import_report report;
  uint8_t *first;
  uint8_t *second;
  size_t first_length = 0u;
  size_t second_length = 0u;

  if (source == NULL || peer == NULL) {
    if (peer != NULL) t64_machine_destroy(peer);
    return;
  }
  first = export_blob(source, &first_length);
  if (first != NULL) {
    T64_CHECK_INT(
        t64_machine_state_import(peer, first, first_length, 0u, &report),
        T64_STATUS_OK);
    second = export_blob(peer, &second_length);
    T64_CHECK_INT(second_length, first_length);
    T64_CHECK(second != NULL && second_length == first_length &&
              memcmp(first, second, first_length) == 0);
    free(second);
  }
  free(first);
  t64_machine_destroy(peer);
}

static t64_machine *build_s32k_with_words(void) {
  t64_machine *machine = create_admitted(SAVE_S32K_ID);

  if (machine == NULL) return NULL;
  T64_CHECK(advance_to(machine, 2000u));
  T64_CHECK_INT(t64_machine_bus_write(machine, SRAM_WORD_A, 4u, 0x11223344u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, SRAM_WORD_B, 4u, 0xA5A5A5A5u),
                T64_STATUS_OK);
  return machine;
}

static void case_idempotence(void) {
  t64_machine *idle = create_admitted(SI_STEM_ID);
  t64_machine *s1 = build_s1(SI_STEM_ID);
  t64_machine *sram = build_s32k_with_words();

  if (idle != NULL) T64_CHECK(advance_to(idle, 2000u));
  idempotence_of(idle, SI_STEM_ID);
  idempotence_of(s1, SI_STEM_ID);
  idempotence_of(sram, SAVE_S32K_ID);
  if (idle != NULL) t64_machine_destroy(idle);
  if (s1 != NULL) t64_machine_destroy(s1);
  if (sram != NULL) t64_machine_destroy(sram);
}

/* ---- case save-media-roundtrip ------------------------------------------ */

/* The 09-05 save import of a patterned image into the machine. */
static void save_import_pattern(t64_machine *machine, uint32_t kind,
                                size_t size) {
  uint8_t *image = (uint8_t *)malloc(size);
  size_t i;

  if (image == NULL) {
    T64_CHECK(false);
    return;
  }
  for (i = 0u; i < size; i++) image[i] = (uint8_t)(i * 7u + 3u);
  T64_CHECK_INT(t64_machine_save_import(machine, kind, T64_SAVE_FORMAT_RAW,
                                        image, size),
                T64_STATUS_OK);
  free(image);
}

static void case_save_media_roundtrip(void) {
  t64_machine *source = build_s32k_with_words();
  t64_machine *peer = create_admitted(SAVE_S32K_ID);
  t64_machine *wrong = create_admitted(SAVE_E4K_ID);
  t64_machine *stem = create_admitted(SI_STEM_ID);
  t64_state_import_report report;
  uint8_t *blob = NULL;
  size_t length = 0u;
  uint32_t value = 0u;
  uint8_t digest[T64_DIGEST_SIZE];

  if (source == NULL || peer == NULL || wrong == NULL || stem == NULL) {
    T64_CHECK(false);
    goto done;
  }
  /* Two host save imports give the source a generation of 2; the guest-side
   * words are written afterwards (guest writes never move the generation). */
  save_import_pattern(source, T64_SAVE_SRAM_32K, 32768u);
  save_import_pattern(source, T64_SAVE_SRAM_32K, 32768u);
  T64_CHECK_INT(t64_machine_bus_write(source, SRAM_WORD_A, 4u, 0x11223344u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(source, SRAM_WORD_B, 4u, 0xA5A5A5A5u),
                T64_STATUS_OK);
  T64_CHECK_INT(save_generation(source, T64_SAVE_SRAM_32K), 2u);
  blob = export_blob(source, &length);
  if (blob == NULL) goto done;

  /* the round trip, with the generation rule */
  T64_CHECK_INT(save_generation(peer, T64_SAVE_SRAM_32K), 0u);
  T64_CHECK_INT(t64_machine_state_import(peer, blob, length, 0u, &report),
                T64_STATUS_OK);
  T64_CHECK(digests_equal(peer, source));
  T64_CHECK_INT(t64_machine_bus_read(peer, SRAM_WORD_A, 4u, &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, 0x11223344u);
  T64_CHECK_INT(t64_machine_bus_read(peer, SRAM_WORD_B, 4u, &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, 0xA5A5A5A5u);
  /* one successful import increments the counter by exactly one */
  T64_CHECK_INT(save_generation(peer, T64_SAVE_SRAM_32K), 1u);
  T64_CHECK_INT(t64_machine_state_import(peer, blob, length, 0u, &report),
                T64_STATUS_OK);
  T64_CHECK_INT(save_generation(peer, T64_SAVE_SRAM_32K), 2u);
  /* the counter is outside the digest and the container: equal digests across
   * differing generations (the source holds 2, a third import makes it 3 on
   * the peer) and the exported bytes do not depend on it */
  T64_CHECK_INT(t64_machine_state_import(peer, blob, length, 0u, &report),
                T64_STATUS_OK);
  T64_CHECK_INT(save_generation(peer, T64_SAVE_SRAM_32K), 3u);
  T64_CHECK_INT(save_generation(source, T64_SAVE_SRAM_32K), 2u);
  T64_CHECK(digests_equal(peer, source));

  /* a refused import never changes it: another profile, a flipped trailer and
   * one row per refusal family */
  T64_CHECK_INT(t64_machine_state_import(wrong, blob, length, 0u, &report),
                T64_STATUS_STATE_PROFILE);
  T64_CHECK_INT(report.reason, T64_STATE_REASON_PROFILE_DIGEST);
  T64_CHECK_INT(save_generation(wrong, T64_SAVE_EEPROM_4K), 0u);
  {
    uint8_t *bad = (uint8_t *)malloc(length);
    size_t offset = 0u;
    size_t body = 0u;

    if (bad != NULL) {
      (void)memcpy(bad, blob, length);
      bad[length - 1u] ^= 1u;
      T64_CHECK_INT(t64_machine_state_import(peer, bad, length, 0u, &report),
                    T64_STATUS_STATE_INTEGRITY);
      T64_CHECK_INT(save_generation(peer, T64_SAVE_SRAM_32K), 3u);
      (void)memcpy(bad, blob, length);
      bad[0] ^= 1u; /* the magic */
      T64_CHECK_INT(t64_machine_state_import(peer, bad, length, 0u, &report),
                    T64_STATUS_STATE_FORMAT);
      T64_CHECK_INT(save_generation(peer, T64_SAVE_SRAM_32K), 3u);
      (void)memcpy(bad, blob, length);
      put32(bad + D_STATE_VERSION, 24u);
      blob_resign(bad, length, RS_ALL);
      T64_CHECK_INT(t64_machine_state_import(peer, bad, length, 0u, &report),
                    T64_STATUS_STATE_VERSION);
      T64_CHECK_INT(save_generation(peer, T64_SAVE_SRAM_32K), 3u);
      /* a re-signed save kind byte: the section 11 field rule, recorded */
      (void)memcpy(bad, blob, length);
      T64_CHECK(find_section(bad, T64_STATE_SECTION_SAVE_MEDIA, &offset, &body));
      put32(bad + offset, 1u);
      blob_resign(bad, length, RS_ALL);
      T64_CHECK_INT(t64_machine_state_import(peer, bad, length, 0u, &report),
                    T64_STATUS_STATE_RANGE);
      T64_CHECK_INT(report.reason, T64_STATE_REASON_FIELD_DERIVED);
      T64_CHECK_INT(report.section_id, T64_STATE_SECTION_SAVE_MEDIA);
      T64_CHECK_INT(report.rule, T64_STATE_RULE_PROFILE_DERIVED);
      T64_CHECK_INT(save_generation(peer, T64_SAVE_SRAM_32K), 3u);
      (void)memcpy(bad, blob, length);
      T64_CHECK(find_section(bad, T64_STATE_SECTION_SAVE_MEDIA, &offset, &body));
      put64(bad + offset + 8u, UINT64_MAX); /* a deadline above the window */
      blob_resign(bad, length, RS_ALL);
      T64_CHECK_INT(t64_machine_state_import(peer, bad, length, 0u, &report),
                    T64_STATUS_STATE_RANGE);
      T64_CHECK_INT(report.rule, T64_STATE_RULE_TICK_LE);
      T64_CHECK_INT(save_generation(peer, T64_SAVE_SRAM_32K), 3u);
      free(bad);
    }
  }

  /* the stem id has no save device: the field stays zero */
  {
    size_t stem_length = 0u;
    uint8_t *stem_blob = export_blob(stem, &stem_length);

    if (stem_blob != NULL) {
      T64_CHECK_INT(
          t64_machine_state_import(stem, stem_blob, stem_length, 0u, &report),
          T64_STATUS_OK);
      T64_CHECK_INT(save_generation(stem, 0u), 0u);
      free(stem_blob);
    }
  }

  /* replacement with the same authored asset resets the counter and the digest
   * to the fresh-machine digest */
  {
    t64_machine *fresh = create_admitted(SAVE_S32K_ID);
    uint8_t asset[IPL3_ASSET_BYTES];

    if (fresh != NULL && read_openipl3_asset(asset)) {
      T64_CHECK_INT(t64_machine_admit_rom(peer, asset, sizeof asset),
                    T64_STATUS_OK);
      T64_CHECK_INT(save_generation(peer, T64_SAVE_SRAM_32K), 0u);
      (void)digest_of(fresh, digest);
      T64_CHECK(digests_equal(peer, fresh));
    }
    if (fresh != NULL) t64_machine_destroy(fresh);
  }

done:
  free(blob);
  if (source != NULL) t64_machine_destroy(source);
  if (peer != NULL) t64_machine_destroy(peer);
  if (wrong != NULL) t64_machine_destroy(wrong);
  if (stem != NULL) t64_machine_destroy(stem);
}

/* The hostile-deadline consequence: an accepted deadline of the blob tick plus
 * the window blocks a host save import for exactly the window and never longer. */
static void case_save_deadline_window(void) {
  size_t length = 0u;
  uint8_t *blob;
  t64_machine *peer;
  t64_state_import_report report;
  uint8_t image[512];
  size_t offset = 0u;
  size_t body = 0u;
  uint64_t tick_out = 0u;
  const uint64_t deadline = 2000u + T64_SAVE_EEPROM_BUSY_TICKS;

  if (!targets_prepare()) return;
  blob = dup_target(T_E4K, &length);
  peer = create_admitted(SAVE_E4K_ID);
  if (blob == NULL || peer == NULL) goto done;
  T64_CHECK(find_section(blob, T64_STATE_SECTION_SAVE_MEDIA, &offset, &body));
  put64(blob + offset + 8u, deadline);
  blob_resign(blob, length, RS_ALL);
  T64_CHECK_INT(t64_machine_state_import(peer, blob, length, 0u, &report),
                T64_STATUS_OK);
  /* park the CPU so the clock can jump across the window */
  T64_CHECK_INT(t64_schedule_cancel(peer, T64_DEVICE_CPU, T64_CPU_EVENT_STEP,
                                    0u, &tick_out),
                T64_STATUS_OK);
  (void)memset(image, 0x5A, sizeof image);
  T64_CHECK_INT(t64_machine_save_import(peer, T64_SAVE_EEPROM_4K,
                                        T64_SAVE_FORMAT_RAW, image,
                                        sizeof image),
                T64_STATUS_SAVE_BUSY);
  T64_CHECK(advance_to(peer, deadline - 1u));
  T64_CHECK_INT(t64_machine_save_import(peer, T64_SAVE_EEPROM_4K,
                                        T64_SAVE_FORMAT_RAW, image,
                                        sizeof image),
                T64_STATUS_SAVE_BUSY);
  T64_CHECK(advance_to(peer, deadline));
  T64_CHECK_INT(t64_machine_save_import(peer, T64_SAVE_EEPROM_4K,
                                        T64_SAVE_FORMAT_RAW, image,
                                        sizeof image),
                T64_STATUS_OK);
done:
  free(blob);
  if (peer != NULL) t64_machine_destroy(peer);
}

/* ---- case continuation-partitions (T3) --------------------------------- */

/* One source against one imported peer with the given advance partitions and
 * trace setting: equal digests at +310, +10,000 and +30,000 and equal drained
 * ISViewer bytes after an identical host write on both. */
static void partition_run(uint64_t first_step, uint64_t later_step,
                          bool trace) {
  t64_machine *source = build_s1(SI_STEM_ID);
  t64_machine *peer = create_admitted(SI_STEM_ID);
  t64_state_import_report report;
  uint8_t *blob = NULL;
  size_t length = 0u;
  uint64_t base;
  static const uint8_t output_text[8] = {'s', 't', 'a', 't', 'e', '0', '9', '!'};
  uint8_t source_bytes[4096];
  uint8_t peer_bytes[4096];
  size_t source_copied = 0u;
  size_t peer_copied = 0u;

  if (source == NULL || peer == NULL) goto done;
  T64_CHECK_INT(t64_machine_trace_set_enabled(source, trace), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_trace_set_enabled(peer, trace), T64_STATUS_OK);
  blob = export_blob(source, &length);
  if (blob == NULL) goto done;
  T64_CHECK_INT(t64_machine_state_import(peer, blob, length, 0u, &report),
                T64_STATUS_OK);
  base = peer->tick;
  isviewer_emit(source, output_text, sizeof output_text);
  isviewer_emit(peer, output_text, sizeof output_text);
  T64_CHECK(digests_equal(source, peer));
  if (advance_pair(source, peer, base + 310u, first_step)) {
    T64_CHECK(digests_equal(source, peer));
  }
  if (advance_pair(source, peer, base + 10000u, later_step)) {
    T64_CHECK(digests_equal(source, peer));
  }
  if (advance_pair(source, peer, base + 30000u, later_step)) {
    T64_CHECK(digests_equal(source, peer));
  }
  if (drain_output(source, source_bytes, &source_copied) &&
      drain_output(peer, peer_bytes, &peer_copied)) {
    T64_CHECK_INT(source_copied, 8u);
    T64_CHECK_INT(peer_copied, source_copied);
    T64_CHECK(memcmp(source_bytes, peer_bytes, source_copied) == 0);
  }
done:
  free(blob);
  if (source != NULL) t64_machine_destroy(source);
  if (peer != NULL) t64_machine_destroy(peer);
}

/* The control: one field of the peer is changed after the import; the
 * comparison that passes unperturbed must now see a difference. */
static void perturbation_control(bool execute_tick) {
  t64_machine *source = build_s1(SI_STEM_ID);
  t64_machine *peer = create_admitted(SI_STEM_ID);
  t64_state_import_report report;
  uint8_t *blob = NULL;
  size_t length = 0u;
  uint64_t base;

  if (source == NULL || peer == NULL) goto done;
  blob = export_blob(source, &length);
  if (blob == NULL) goto done;
  T64_CHECK_INT(t64_machine_state_import(peer, blob, length, 0u, &report),
                T64_STATUS_OK);
  base = peer->tick;
  if (execute_tick) {
    peer->si.execute_tick += 1u;
    /* the execute tick is pending state until the read executes: visible at
     * the first checkpoint */
    if (advance_pair(source, peer, base + 310u, 0u)) {
      T64_CHECK(!digests_equal(source, peer));
    }
  } else {
    peer->si.next_token += 1u;
    /* the token counter persists: visible at the last checkpoint */
    if (advance_pair(source, peer, base + 30000u, 0u)) {
      T64_CHECK(!digests_equal(source, peer));
    }
  }
done:
  free(blob);
  if (source != NULL) t64_machine_destroy(source);
  if (peer != NULL) t64_machine_destroy(peer);
}

static void case_continuation_partitions(void) {
  partition_run(0u, 0u, false);
  partition_run(1u, 0u, false);
  partition_run(7u, 0u, false);
  partition_run(31u, 0u, false);
  partition_run(4096u, 4096u, false);
  partition_run(0u, 0u, true);
  partition_run(7u, 4096u, true);
  perturbation_control(true);
  perturbation_control(false);
}

/* ---- case dirty-peer ---------------------------------------------------- */

static void case_dirty_peer(void) {
  t64_machine *source = build_s1(SI_STEM_ID);
  t64_machine *dirty = build_s1(SI_STEM_ID);
  t64_machine *rewound = build_s1(SI_STEM_ID);
  t64_machine *clone = create_admitted(SI_STEM_ID);
  t64_state_import_report report;
  uint8_t *blob = NULL;
  uint8_t *rewound_blob = NULL;
  size_t length = 0u;
  size_t rewound_length = 0u;

  if (source == NULL || dirty == NULL || rewound == NULL || clone == NULL) {
    goto done;
  }
  blob = export_blob(source, &length);
  rewound_blob = export_blob(rewound, &rewound_length);
  if (blob == NULL || rewound_blob == NULL) goto done;
  /* a peer that ran 40,000 further ticks of its own SI transaction */
  T64_CHECK(advance_to(dirty, source->tick + 40000u));
  T64_CHECK(!digests_equal(source, dirty));
  T64_CHECK_INT(t64_machine_state_import(dirty, blob, length, 0u, &report),
                T64_STATUS_OK);
  T64_CHECK(digests_equal(source, dirty));
  T64_CHECK_INT(dirty->tick, source->tick);
  if (advance_pair(source, dirty, source->tick + 30000u, 0u)) {
    T64_CHECK(digests_equal(source, dirty));
  }
  /* rewind: the source advanced 10,000 ticks beyond the export point imports
   * its own earlier blob and equals a never-advanced clone */
  T64_CHECK_INT(t64_machine_state_import(clone, rewound_blob, rewound_length,
                                         0u, &report),
                T64_STATUS_OK);
  T64_CHECK(advance_to(rewound, rewound->tick + 10000u));
  T64_CHECK(!digests_equal(rewound, clone));
  T64_CHECK_INT(
      t64_machine_state_import(rewound, rewound_blob, rewound_length, 0u,
                               &report),
      T64_STATUS_OK);
  T64_CHECK(digests_equal(rewound, clone));
  T64_CHECK_INT(rewound->tick, clone->tick);
  if (advance_pair(rewound, clone, rewound->tick + 30000u, 0u)) {
    T64_CHECK(digests_equal(rewound, clone));
  }
done:
  free(blob);
  free(rewound_blob);
  if (source != NULL) t64_machine_destroy(source);
  if (dirty != NULL) t64_machine_destroy(dirty);
  if (rewound != NULL) t64_machine_destroy(rewound);
  if (clone != NULL) t64_machine_destroy(clone);
}

/* ---- case observer-flag ------------------------------------------------- */

static void write_vi_register(t64_machine *machine, uint32_t offset,
                              uint32_t value) {
  T64_CHECK_INT(t64_machine_bus_write(machine, VI_BASE + offset, 4u, value),
                T64_STATUS_OK);
}

/* A VI field with authored 5551 source pixels: one retained undrained packet
 * (the setup of core/tests/test_video_output.c). */
static t64_machine *build_vi_frame_with(bool coverage) {
  static const uint16_t source[4] = {UINT16_C(0xf801), UINT16_C(0x07c1),
                                     UINT16_C(0x003f), UINT16_C(0xffff)};
  t64_machine *machine = create_admitted(SI_STEM_ID);
  size_t i;

  if (machine == NULL) return NULL;
  for (i = 0u; i < sizeof source / sizeof source[0]; i++) {
    const uint32_t address = VI_ORIGIN + (uint32_t)(i * 2u);

    T64_CHECK_INT(t64_machine_bus_write(machine, address, 2u, source[i]),
                  T64_STATUS_OK);
    if (coverage) t64_rdp_memory_coverage_write(machine, address, 2u, 8u);
  }
  write_vi_register(machine, UINT32_C(0x04), VI_ORIGIN);
  write_vi_register(machine, UINT32_C(0x08), UINT32_C(4));
  write_vi_register(machine, UINT32_C(0x18), UINT32_C(8));
  write_vi_register(machine, UINT32_C(0x1c), UINT32_C(15));
  write_vi_register(machine, UINT32_C(0x24), UINT32_C(4));
  write_vi_register(machine, UINT32_C(0x28), UINT32_C(2));
  write_vi_register(machine, UINT32_C(0x30), UINT32_C(0x400));
  write_vi_register(machine, UINT32_C(0x34), UINT32_C(0x400));
  write_vi_register(machine, UINT32_C(0x00), UINT32_C(2));
  for (i = 0u; i < 512u && machine->vi.packet_count == 0u; i++) {
    size_t q;
    uint64_t tick = UINT64_MAX;

    for (q = 0u; q < machine->queue_count; q++) {
      const t64_event *event =
          &machine->events[(machine->queue_head + q) % T64_EVENT_QUEUE_CAPACITY];

      if (event->device == T64_VI_DEVICE && event->kind == T64_VI_EVENT_FIELD &&
          event->token == machine->vi.event_generation) {
        tick = event->tick;
        break;
      }
    }
    if (tick == UINT64_MAX || !advance_to(machine, tick)) break;
  }
  /* Reached: a retained, undrained packet exists. */
  T64_CHECK(machine->vi.packet_count >= 1u);
  T64_CHECK(machine->vi.packet_lengths[machine->vi.packet_head] > 0u);
  return machine;
}

/* The 09-09 observer-flag setup, with the hidden memory-coverage marks. */
static t64_machine *build_vi_frame(void) {
  return build_vi_frame_with(true);
}

static void case_observer_flag(void) {
  t64_machine *source = build_vi_frame();
  t64_machine *keep = create_admitted(SI_STEM_ID);
  t64_machine *drop = create_admitted(SI_STEM_ID);
  t64_machine *refused = create_admitted(SI_STEM_ID);
  t64_state_import_report report;
  t64_video_packet_info source_info;
  t64_video_packet_info keep_info;
  static const uint8_t first[8] = {'o', 'n', 'e', '.', 'o', 'n', 'e', '.'};
  static const uint8_t second[8] = {'t', 'w', 'o', '.', 't', 'w', 'o', '.'};
  uint8_t drained[4096];
  size_t copied = 0u;
  uint8_t *blob = NULL;
  size_t length = 0u;
  snapshot before;

  if (source == NULL || keep == NULL || drop == NULL || refused == NULL) {
    goto done;
  }
  /* observer progress: 16 captured bytes, the first 8 already drained */
  isviewer_emit(source, first, sizeof first);
  T64_CHECK(drain_output(source, drained, &copied));
  T64_CHECK_INT(copied, 8u);
  isviewer_emit(source, second, sizeof second);
  T64_CHECK_INT(source->output.captured_bytes, 16u);
  T64_CHECK_INT(source->output_drain_cursor, 8u);
  blob = export_blob(source, &length);
  if (blob == NULL) goto done;
  T64_CHECK_INT(t64_video_info(source, &source_info), T64_STATUS_OK);

  /* default import: head, count, cursor and the oldest packet are restored */
  T64_CHECK_INT(t64_machine_state_import(keep, blob, length, 0u, &report),
                T64_STATUS_OK);
  T64_CHECK_INT(keep->vi.packet_head, source->vi.packet_head);
  T64_CHECK_INT(keep->vi.packet_count, source->vi.packet_count);
  T64_CHECK_INT(keep->ai.packet_head, source->ai.packet_head);
  T64_CHECK_INT(keep->ai.packet_count, source->ai.packet_count);
  T64_CHECK_INT(keep->output_drain_cursor, source->output_drain_cursor);
  T64_CHECK_INT(t64_video_info(keep, &keep_info), T64_STATUS_OK);
  if (memcmp(&source_info, &keep_info, sizeof source_info) != 0) {
    size_t i;
    const uint8_t *a = (const uint8_t *)&source_info;
    const uint8_t *b = (const uint8_t *)&keep_info;

    for (i = 0u; i < sizeof source_info; i++) {
      if (a[i] != b[i]) {
        fprintf(stderr, "video info byte %zu: source %02x peer %02x\n", i, a[i], b[i]);
      }
    }
  }
  T64_CHECK(memcmp(&source_info, &keep_info, sizeof source_info) == 0);
  T64_CHECK(digests_equal(source, keep));
  T64_CHECK(drain_output(keep, drained, &copied));
  T64_CHECK_INT(copied, 8u);
  T64_CHECK(memcmp(drained, second, sizeof second) == 0);

  /* drop-observers: both rings empty, the cursor at the retained length, the
   * canonical digest the same */
  T64_CHECK_INT(t64_machine_state_import(drop, blob, length,
                                         T64_STATE_IMPORT_DROP_OBSERVERS,
                                         &report),
                T64_STATUS_OK);
  T64_CHECK_INT(drop->vi.packet_head, 0u);
  T64_CHECK_INT(drop->vi.packet_count, 0u);
  T64_CHECK_INT(drop->ai.packet_head, 0u);
  T64_CHECK_INT(drop->ai.packet_count, 0u);
  T64_CHECK_INT(drop->output_drain_cursor, drop->output.captured_bytes);
  T64_CHECK_INT(t64_video_info(drop, &keep_info), T64_STATUS_NOT_READY);
  T64_CHECK(digests_equal(source, drop));
  T64_CHECK(drain_output(drop, drained, &copied));
  T64_CHECK_INT(copied, 0u);

  /* any other flag bit is refused with a bit-identical peer */
  snapshot_take(refused, &before);
  T64_CHECK_INT(t64_machine_state_import(refused, blob, length, 0x2u, &report),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK_INT(report.reason, T64_STATE_REASON_ARG_FLAGS);
  T64_CHECK_INT(t64_machine_state_import(refused, blob, length,
                                         T64_STATE_IMPORT_DROP_OBSERVERS | 0x4u,
                                         &report),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK_INT(report.reason, T64_STATE_REASON_ARG_FLAGS);
  T64_CHECK(snapshot_matches(refused, &before));
  snapshot_free(&before);
done:
  free(blob);
  if (source != NULL) t64_machine_destroy(source);
  if (keep != NULL) t64_machine_destroy(keep);
  if (drop != NULL) t64_machine_destroy(drop);
  if (refused != NULL) t64_machine_destroy(refused);
}


/* ======================================================================== */
/* Plan 09-09.1: the boundary matrix (D-32, capped at the sixteen rows of    */
/* the table below; a seventeenth row is a recorded deviation)               */
/* ======================================================================== */

#define PI_BASE UINT32_C(0x04600000)
#define CART_BASE UINT32_C(0x10000000)
#define CHECKPOINT_COUNT 4u

/* Offsets from the import tick of the four compared checkpoints. */
static const uint64_t checkpoint_offset[CHECKPOINT_COUNT] = {0u, 310u, 10000u,
                                                             30000u};

/* The full advance result tuple: the status, the stop reason and tick of the
 * last advance call, the machine tick and the three progress counters. */
typedef struct {
  t64_status status;
  uint32_t reason;
  uint64_t stop_tick;
  uint64_t tick;
  uint64_t instructions;
  uint64_t frames;
} tuple;

static bool tuples_equal(const tuple *a, const tuple *b) {
  return a->status == b->status && a->reason == b->reason &&
         a->stop_tick == b->stop_tick && a->tick == b->tick &&
         a->instructions == b->instructions && a->frames == b->frames;
}

static void tuple_print(const char *label, const tuple *t) {
  fprintf(stderr,
          "  %s: status %u reason %u stop %llu tick %llu insn %llu frames %llu\n",
          label, (unsigned)t->status, (unsigned)t->reason,
          (unsigned long long)t->stop_tick, (unsigned long long)t->tick,
          (unsigned long long)t->instructions, (unsigned long long)t->frames);
}

/* Advance to an absolute tick in partitions of `step` ticks (0: one partition)
 * each walked in slices of ADVANCE_SLICE_TICKS (the event budget of one call).
 * A stop other than the bound ends the walk, as the same stop ends it on both
 * sides of a comparison. */
static void run_to(t64_machine *machine, uint64_t target, uint64_t step,
                   tuple *out) {
  t64_status status = T64_STATUS_OK;
  t64_stop_result stop = {0u, 0u};
  bool stopped = false;

  while (machine->tick < target && !stopped) {
    const uint64_t part =
        step == 0u || target - machine->tick <= step ? target
                                                     : machine->tick + step;

    while (machine->tick < part) {
      const uint64_t next = part - machine->tick > ADVANCE_SLICE_TICKS
                                ? machine->tick + ADVANCE_SLICE_TICKS
                                : part;
      t64_stop_request request = {T64_STOP_GUEST_TICK, next};

      status = t64_machine_advance(machine, &request, &stop);
      if (status != T64_STATUS_OK || stop.reason != T64_STOP_REASON_BOUND) {
        stopped = true;
        break;
      }
    }
  }
  out->status = status;
  out->reason = stop.reason;
  out->stop_tick = stop.tick;
  out->tick = machine->tick;
  out->instructions = machine->instruction_count;
  out->frames = machine->frame_event_count;
}

static const t64_event *find_event(const t64_machine *machine, uint32_t device,
                                   uint32_t kind) {
  size_t i;

  for (i = 0u; i < machine->queue_count; i++) {
    const t64_event *event =
        &machine->events[(machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];

    if (event->device == device && event->kind == kind) return event;
  }
  return NULL;
}

static void bus_write32(t64_machine *machine, uint32_t address, uint32_t value) {
  T64_CHECK_INT(t64_machine_bus_write(machine, address, 4u, value),
                T64_STATUS_OK);
}


/* Diagnostic: the first differing section and offset of two machines' exports
 * (printed only when T64_STATE_MATRIX_TRACE is set). */
static void debug_diff(const char *label, const t64_machine *a,
                       const t64_machine *b) {
  size_t la = 0u;
  size_t lb = 0u;
  uint8_t *ba;
  uint8_t *bb;
  uint32_t count;
  uint32_t i;

  if (getenv("T64_STATE_MATRIX_TRACE") == NULL) return;
  ba = export_blob(a, &la);
  bb = export_blob(b, &lb);
  if (ba != NULL && bb != NULL) {
    count = rd32(ba + 0x018u);
    for (i = 0u; i < count; i++) {
      size_t oa = 0u, na = 0u, ob = 0u, nb = 0u;
      const uint32_t id = rd32(ba + D_TABLE + (size_t)D_ENTRY * i);

      if (!find_section(ba, id, &oa, &na) || !find_section(bb, id, &ob, &nb) ||
          na != nb) {
        fprintf(stderr, "%s: section %u length differs\n", label, (unsigned)id);
        continue;
      }
      if (memcmp(ba + oa, bb + ob, na) != 0) {
        size_t k = 0u;

        while (ba[oa + k] == bb[ob + k]) k++;
        fprintf(stderr, "%s: section %u first difference at offset %zu (%02x vs %02x)\n",
                label, (unsigned)id, k, ba[oa + k], bb[ob + k]);
      }
    }
  }
  {
    size_t slot;

    for (slot = 0u; slot < T64_VIDEO_QUEUE_CAPACITY; slot++) {
      const t64_video_packet_info *x = &a->vi.packet_info[slot];
      const t64_video_packet_info *y = &b->vi.packet_info[slot];

      fprintf(stderr, "%s: vi slot %zu seq %llu/%llu start %llu/%llu end %llu/%llu count %zu/%zu head %zu/%zu len %zu/%zu\n",
              label, slot, (unsigned long long)x->sequence,
              (unsigned long long)y->sequence, (unsigned long long)x->start_tick,
              (unsigned long long)y->start_tick, (unsigned long long)x->end_tick,
              (unsigned long long)y->end_tick, a->vi.packet_count,
              b->vi.packet_count, a->vi.packet_head, b->vi.packet_head,
              a->vi.packet_lengths[slot], b->vi.packet_lengths[slot]);
    }
  }
  free(ba);
  free(bb);
}

/* ---- the table ----------------------------------------------------------- */

typedef struct {
  const char *id;
  const char *name;
  const char *source; /* the construction source, confirmed by `reached` */
  const char *profile;
  t64_machine *(*build)(void);
  /* NULL when the boundary is reached, else the name of the unreached field */
  const char *(*reached)(const t64_machine *machine);
  void (*perturb)(t64_machine *machine); /* one field of an imported copy */
  uint32_t pair_device[2];               /* the queue events the device owns */
  uint32_t pair_kind[2];
  bool full; /* the full partition set runs on this row in every preset */
} boundary;

/* B01 pi-active: a PI DMA read started through the host bus and stopped one
 * tick later, before its completion tick (source: test_pi_dma.c busy start). */
static t64_machine *build_b01(void) {
  t64_machine *machine = create_admitted(SI_STEM_ID);

  if (machine == NULL) return NULL;
  T64_CHECK(advance_to(machine, 2000u));
  T64_CHECK(!machine->pi.active_valid);
  bus_write32(machine, PI_BASE, UINT32_C(0x3A00));
  bus_write32(machine, PI_BASE + 4u, CART_BASE + UINT32_C(0x100));
  bus_write32(machine, PI_BASE + 12u, 63u);
  T64_CHECK(advance_to(machine, 2001u));
  return machine;
}

static const char *reached_b01(const t64_machine *machine) {
  const t64_event *event = find_event(machine, T64_DEVICE_PI,
                                      T64_PI_EVENT_COMPLETE);

  if (!machine->pi.active_valid) return "pi.active_valid";
  if (machine->pi.active_canceled) return "pi.active_canceled";
  if (event == NULL) return "queue (PI, COMPLETE)";
  if (event->token != machine->pi.active_token) return "event token";
  if (event->tick <= machine->tick) return "completion tick above the tick";
  return NULL;
}

static void perturb_b01(t64_machine *machine) {
  machine->pi.active_dram_address += 8u;
}

/* ---- rows B02 to B10: the device-queue boundaries (plan 09-09.1 Task 2) -- */

#define SP_BASE UINT32_C(0x04040000)
#define IMEM_BASE UINT32_C(0x04001000)
#define DP_START_REG UINT32_C(0x04100000)
#define AI_BASE UINT32_C(0x04500000)
#define AI_REG_LEN (AI_BASE + UINT32_C(4))
#define AI_REG_CONTROL (AI_BASE + UINT32_C(8))
#define AI_REG_DACRATE (AI_BASE + UINT32_C(0x10))
#define AI_REG_BITRATE (AI_BASE + UINT32_C(0x14))

/* The stop of the stalling advance a builder ends with (rows B07 and B10). */
static uint32_t build_stop_reason;
static uint64_t build_stop_tick;

/* A machine whose CPU runs a two-instruction spin loop at 0x80001000 (the setup
 * of core/tests/test_ai.c start_ai_spin_loop), so the boundary state is not
 * disturbed by the boot code. */
static t64_machine *build_quiet(void) {
  t64_machine *machine = create_admitted(SI_STEM_ID);

  if (machine == NULL) return NULL;
  machine->ram[0x1000] = 0x08u;
  machine->ram[0x1001] = 0x00u;
  machine->ram[0x1002] = 0x04u;
  machine->ram[0x1003] = 0x00u;
  (void)memset(machine->ram + 0x1004, 0, 4u);
  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = t64_cpu_canonical_address(machine->cpu.pc);
  return machine;
}

/* B02 pi-canceled: the B01 transfer reset through the PI status register; the
 * request stays active, marked canceled, with its completion event retained
 * (source: test_pi_dma.c cancel and stale completion). */
static t64_machine *build_b02(void) {
  t64_machine *machine = build_b01();

  if (machine == NULL) return NULL;
  bus_write32(machine, PI_BASE + 16u, 1u);
  return machine;
}

static const char *reached_b02(const t64_machine *machine) {
  const t64_event *event =
      find_event(machine, T64_DEVICE_PI, T64_PI_EVENT_COMPLETE);

  if (!machine->pi.active_valid) return "pi.active_valid";
  if (!machine->pi.active_canceled) return "pi.active_canceled";
  if (event == NULL) return "queue (PI, COMPLETE)";
  if (event->token != machine->pi.active_token) return "event token";
  return NULL;
}

static void perturb_b02(t64_machine *machine) {
  machine->pi.active_canceled = false;
}

/* B03 sp-active-pending: an SP DMA in flight with a second descriptor in the
 * pending slot (source: test_sp_dma.c pending queue case). */
static t64_machine *build_b03(void) {
  t64_machine *machine = build_quiet();
  size_t i;

  if (machine == NULL) return NULL;
  for (i = 0u; i < 0x200u; i++) {
    machine->ram[0x100u + i] = (uint8_t)(0x31u + i);
  }
  bus_write32(machine, SP_BASE, 0u);
  bus_write32(machine, SP_BASE + 4u, UINT32_C(0x100));
  bus_write32(machine, SP_BASE + 8u, 0u);
  bus_write32(machine, SP_BASE, UINT32_C(0x1020));
  bus_write32(machine, SP_BASE + 4u, UINT32_C(0x200));
  bus_write32(machine, SP_BASE + 8u, 8u);
  return machine;
}

static const char *reached_b03(const t64_machine *machine) {
  if (!machine->sp.active.valid) return "sp.active.valid";
  if (!machine->sp.pending.valid) return "sp.pending.valid";
  return NULL;
}

/* The plan names the pending length; it is consumed when the slot is
 * promoted, so the difference would vanish before the first continuation
 * checkpoint (recorded as a deviation). The DRAM source address of the pending
 * descriptor changes which bytes the promoted transfer copies. */
static void perturb_b03(t64_machine *machine) {
  machine->sp.pending.aligned_dram_start += 8u;
}

/* B04 rsp-mid-program: a program with a load-store bubble that loops, stopped
 * at tick 4 after three retired instructions (source: test_rsp_control.c
 * load_store_bubble_test). */
static t64_machine *build_b04(void) {
  static const uint32_t program[] = {
      UINT32_C(0x24010000), /* ADDIU r1,r0,0 */
      UINT32_C(0x8c220000), /* LW r2,0(r1) */
      UINT32_C(0x00000000), /* one intervening issue slot */
      UINT32_C(0xac230004), /* SW r3,4(r1), independent of the loaded r2 */
      UINT32_C(0x08000000), /* J 0 */
      UINT32_C(0x00000000)};
  t64_machine *machine = build_quiet();
  size_t i;

  if (machine == NULL) return NULL;
  machine->rsp.gpr[3] = UINT32_C(0x89abcdef);
  for (i = 0u; i < sizeof program / sizeof program[0]; i++) {
    T64_CHECK_INT(t64_machine_bus_write(machine, IMEM_BASE + (uint32_t)(4u * i),
                                        4u, program[i]),
                  T64_STATUS_OK);
  }
  T64_CHECK_INT(t64_rsp_start(machine), T64_STATUS_OK);
  T64_CHECK(advance_to(machine, 4u));
  return machine;
}

static const char *reached_b04(const t64_machine *machine) {
  const t64_event *event =
      find_event(machine, T64_DEVICE_SP, T64_SP_EVENT_RSP_ISSUE);

  if ((machine->rsp.sp_status & T64_RSP_STATUS_HALT) != 0u) return "rsp halted";
  if (machine->rsp.memory_access_ready_tick <= machine->tick) {
    return "rsp.memory_access_ready_tick";
  }
  if (machine->rsp.next_issue_tick <= machine->tick) {
    return "rsp.next_issue_tick";
  }
  if (event == NULL) return "queue (SP, RSP_ISSUE)";
  return NULL;
}

/* The plan names next_issue_tick; the next issue overwrites it, so the
 * difference would vanish before the first continuation checkpoint (recorded as
 * a deviation). The stored register persists in the data memory. */
static void perturb_b04(t64_machine *machine) {
  machine->rsp.gpr[3] ^= UINT32_C(1);
}

/* B05 rdp-pending-words: a command word fetched and waiting for its apply
 * event with triangle coverage present (source: test_rdp_triangles.c raw
 * words submitted through the RDP registers). A pending triangle packet cannot
 * be the boundary: triangles need the combine mode of the filter-v9 family and
 * that register block is outside the v25 state, so the nearest builder that
 * reaches the boundary class holds a plain command word pending (recorded as a
 * deviation). The coverage word is a direct test-only state edit. */
#define B05_COMMAND_ADDRESS UINT32_C(0x4000)

static void put_be64(uint8_t *bytes, uint64_t value) {
  size_t i;

  for (i = 0u; i < 8u; i++) bytes[i] = (uint8_t)(value >> (56u - 8u * i));
}

static bool coverage_nonzero(const t64_machine *machine) {
  size_t i;

  for (i = 0u; i < T64_RDP_TRIANGLE_COVERAGE_BYTES / 2u; i++) {
    if (machine->rdp_triangle_coverage[i] != 0u) return true;
  }
  return false;
}

static t64_machine *build_b05(void) {
  static const uint64_t words[] = {
      UINT64_C(0x3f10000700005000), /* RGBA16 color image, width 8 */
      UINT64_C(0x2d00000000020020), /* scissor [0,8) x [0,8) */
      UINT64_C(0x2f00000000000000)};/* 1-cycle mode */
  t64_machine *machine = build_quiet();
  size_t i;

  if (machine == NULL) return NULL;
  for (i = 0u; i < sizeof words / sizeof words[0]; i++) {
    put_be64(machine->ram + B05_COMMAND_ADDRESS + 8u * i, words[i]);
  }
  machine->rdp_triangle_coverage[1234] = UINT16_C(0x8001);
  bus_write32(machine, DP_START_REG, B05_COMMAND_ADDRESS);
  bus_write32(machine, DP_START_REG + 4u,
              B05_COMMAND_ADDRESS + (uint32_t)sizeof words);
  for (i = 0u; i < 64u && !machine->rdp.command_pending; i++) {
    T64_CHECK(advance_to(machine, machine->tick + 1u));
  }
  return machine;
}

static const char *reached_b05(const t64_machine *machine) {
  if (!machine->rdp.command_pending) return "rdp.command_pending";
  if (machine->rdp.pending_command_word != UINT64_C(0x3f10000700005000)) {
    return "rdp.pending_command_word";
  }
  if (machine->rdp.pending_triangle_word_count != 0u) {
    return "rdp.pending_triangle_word_count";
  }
  if (find_event(machine, T64_RDP_DEVICE, T64_RDP_EVENT_WORK) == NULL) {
    return "queue (RDP, WORK)";
  }
  if (!coverage_nonzero(machine)) return "rdp_triangle_coverage";
  return NULL;
}

static void perturb_b05(t64_machine *machine) {
  machine->rdp.pending_command_word ^= UINT64_C(1) << 32;
}

/* B06 vi-midfield-retained and B07 vi-stalled-event: the VI frame of the
 * observer-flag case (authored 5551 source pixels), then, for B07, fields
 * advanced until the two-packet queue is full and the next field event waits
 * (sources: test_video_output.c advance_one_field and the output-full pending
 * boundary; test_vi.c advance_until_video_full). */
static const t64_event *current_vi_event(const t64_machine *machine) {
  size_t i;

  for (i = 0u; i < machine->queue_count; i++) {
    const t64_event *event =
        &machine->events[(machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];

    if (event->device == T64_VI_DEVICE && event->kind == T64_VI_EVENT_FIELD &&
        event->token == machine->vi.event_generation) {
      return event;
    }
  }
  return NULL;
}

static const t64_event *stalled_front_event(const t64_machine *machine) {
  return machine->queue_count == 0u ? NULL
                                    : &machine->events[machine->queue_head];
}

/* The matrix rows leave the hidden RDP memory-coverage table untouched: that
 * table is outside the v25 state (it is zeroed by the import), so a boundary
 * that depended on it could not continue equal; see docs/spec/state-container.md
 * "Limits of the matrix". */
static t64_machine *build_b06(void) {
  return build_vi_frame_with(false);
}

static const char *reached_b06(const t64_machine *machine) {
  if (!machine->vi.enabled) return "vi.enabled";
  if (machine->vi.packet_count < 1u) return "vi.packet_count";
  if (current_vi_event(machine) == NULL) return "queue (VI, FIELD)";
  return NULL;
}

static void perturb_b06(t64_machine *machine) {
  machine->vi.phase_remainder += 1u;
}

/* Advance toward a far bound until the output queue is full and two further
 * calls return the same resource-limit stop at the same tick with the queue
 * unchanged: a stall of the occupancy-gated field event, not the event budget of
 * one call (the loop of core/tests/test_vi.c advance_until_video_full). */
static t64_machine *build_b07(void) {
  t64_machine *machine = build_vi_frame_with(false);
  size_t attempt;

  build_stop_reason = 0u;
  build_stop_tick = 0u;
  if (machine == NULL) return NULL;
  for (attempt = 0u; attempt < 512u; attempt++) {
    t64_stop_request request = {T64_STOP_GUEST_TICK,
                                machine->tick + UINT64_C(4000000)};
    t64_stop_result stop = {0u, 0u};

    T64_CHECK_INT(t64_machine_advance(machine, &request, &stop), T64_STATUS_OK);
    if (machine->vi.packet_count >= T64_VIDEO_QUEUE_CAPACITY) {
      const size_t queue_before = machine->queue_count;
      const uint64_t tick_before = machine->tick;

      T64_CHECK_INT(t64_machine_advance(machine, &request, &stop),
                    T64_STATUS_OK);
      if (stop.reason == T64_STOP_REASON_RESOURCE_LIMIT &&
          machine->tick == tick_before && machine->queue_count == queue_before) {
        build_stop_reason = stop.reason;
        build_stop_tick = stop.tick;
      }
      break;
    }
    if (stop.reason != T64_STOP_REASON_RESOURCE_LIMIT) break;
  }
  return machine;
}

static const char *reached_b07(const t64_machine *machine) {
  const t64_event *front = stalled_front_event(machine);

  if (machine->vi.packet_count != T64_VIDEO_QUEUE_CAPACITY) {
    return "vi.packet_count";
  }
  if (front == NULL || front->device != T64_VI_DEVICE ||
      front->kind != T64_VI_EVENT_FIELD ||
      front->token != machine->vi.event_generation) {
    return "queue front (VI, FIELD)";
  }
  if (build_stop_reason != T64_STOP_REASON_RESOURCE_LIMIT) {
    return "resource-limit stop";
  }
  if (machine->tick != build_stop_tick) return "stalled tick";
  return NULL;
}

static void perturb_b07(t64_machine *machine) {
  machine->vi.next_boundary_units += 1u;
}

/* AI rows: the rate pair, the control enable and the descriptor submissions of
 * test_ai.c on the quiet machine. */
static void ai_configure(t64_machine *machine) {
  bus_write32(machine, AI_REG_DACRATE, 1103u);
  bus_write32(machine, AI_REG_BITRATE, 15u);
}

static void ai_submit(t64_machine *machine, uint32_t address, uint32_t bytes) {
  bus_write32(machine, AI_BASE, address);
  bus_write32(machine, AI_REG_LEN, bytes);
}

static const t64_event *current_ai_event(const t64_machine *machine) {
  size_t i;

  for (i = 0u; i < machine->queue_count; i++) {
    const t64_event *event =
        &machine->events[(machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];

    if (event->device == T64_AI_DEVICE && event->kind == T64_AI_EVENT_SAMPLE &&
        event->token == machine->ai.event_token) {
      return event;
    }
  }
  return NULL;
}

static void advance_to_next_ai_sample(t64_machine *machine) {
  const t64_event *event = current_ai_event(machine);

  T64_CHECK(event != NULL);
  if (event != NULL) T64_CHECK(advance_to(machine, event->tick));
}

static void ai_load_pcm(t64_machine *machine) {
  static const uint8_t pcm[24] = {
      0x00u, 0x01u, 0x00u, 0x02u, 0x00u, 0x03u, 0x00u, 0x04u,
      0x10u, 0x01u, 0x10u, 0x02u, 0x10u, 0x03u, 0x10u, 0x04u,
      0x20u, 0x01u, 0x20u, 0x02u, 0x20u, 0x03u, 0x20u, 0x04u};

  (void)memcpy(machine->ram + 0x100, pcm, sizeof pcm);
}

/* B08 ai-two-descriptors (source: test_ai.c prepare_two_descriptor_stream). */
static t64_machine *build_b08(void) {
  t64_machine *machine = build_quiet();

  if (machine == NULL) return NULL;
  ai_load_pcm(machine);
  ai_configure(machine);
  bus_write32(machine, AI_REG_CONTROL, 1u);
  ai_submit(machine, UINT32_C(0x100), 8u);
  ai_submit(machine, UINT32_C(0x108), 8u);
  return machine;
}

static const char *reached_b08(const t64_machine *machine) {
  if (machine->ai.descriptor_count != 2u) return "ai.descriptor_count";
  if (current_ai_event(machine) == NULL) return "queue (AI, SAMPLE)";
  return NULL;
}

static void perturb_b08(t64_machine *machine) {
  machine->ai.descriptors[(machine->ai.descriptor_head + 1u) %
                          T64_AUDIO_QUEUE_CAPACITY].length -= 8u;
}

/* B09 ai-paused-partial: one sample into a four-frame descriptor, paused 17
 * ticks before the next sample (source: test_ai.c case_pause_phase_resume). */
static t64_machine *build_b09(void) {
  t64_machine *machine = build_quiet();
  const t64_event *event;

  if (machine == NULL) return NULL;
  ai_load_pcm(machine);
  ai_configure(machine);
  bus_write32(machine, AI_REG_CONTROL, 1u);
  ai_submit(machine, UINT32_C(0x100), 16u);
  advance_to_next_ai_sample(machine);
  event = current_ai_event(machine);
  T64_CHECK(event != NULL);
  if (event == NULL) return machine;
  T64_CHECK(advance_to(machine, event->tick - 17u));
  bus_write32(machine, AI_REG_CONTROL, 0u);
  return machine;
}

static const char *reached_b09(const t64_machine *machine) {
  if (!machine->ai.paused) return "ai.paused";
  if (machine->ai.paused_remaining_ticks == 0u) {
    return "ai.paused_remaining_ticks";
  }
  if (machine->ai.packet_length == 0u) return "ai.packet_length";
  if (current_ai_event(machine) != NULL) return "queue (AI, SAMPLE) present";
  return NULL;
}

static void perturb_b09(t64_machine *machine) {
  machine->ai.paused_remaining_ticks += 1u;
}

/* B10 ai-output-full-stall: two completed packets, a third descriptor whose
 * sample event waits for output capacity (source: test_ai.c
 * prepare_blocked_ai_event and case_output_full_same_event). */
static t64_machine *build_b10(void) {
  t64_machine *machine = build_quiet();
  size_t i;

  build_stop_reason = 0u;
  build_stop_tick = 0u;
  if (machine == NULL) return NULL;
  ai_load_pcm(machine);
  ai_configure(machine);
  bus_write32(machine, AI_REG_CONTROL, 1u);
  ai_submit(machine, UINT32_C(0x100), 8u);
  ai_submit(machine, UINT32_C(0x108), 8u);
  for (i = 0u; i < 4u; i++) advance_to_next_ai_sample(machine);
  ai_submit(machine, UINT32_C(0x110), 8u);
  if (current_ai_event(machine) != NULL) {
    t64_stop_request request = {T64_STOP_GUEST_TICK,
                                current_ai_event(machine)->tick};
    t64_stop_result stop = {0u, 0u};

    T64_CHECK_INT(t64_machine_advance(machine, &request, &stop), T64_STATUS_OK);
    build_stop_reason = stop.reason;
    build_stop_tick = stop.tick;
  }
  return machine;
}

static const char *reached_b10(const t64_machine *machine) {
  const t64_event *event = current_ai_event(machine);

  if (machine->ai.packet_count != T64_AUDIO_QUEUE_CAPACITY) {
    return "ai.packet_count";
  }
  if (event == NULL) return "queue (AI, SAMPLE)";
  if (!t64_ai_event_waits_for_output_capacity(machine, event)) {
    return "event waits for output capacity";
  }
  if (build_stop_reason != T64_STOP_REASON_RESOURCE_LIMIT) {
    return "resource-limit stop";
  }
  if (machine->tick != build_stop_tick || machine->tick >= event->tick) {
    return "stalled tick below the event";
  }
  return NULL;
}

static void perturb_b10(t64_machine *machine) {
  machine->ai.phase_remainder += 1u;
}

static const boundary boundaries[] = {
    {"B01", "pi-active", "test_pi_dma.c busy start, stem id", SI_STEM_ID,
     build_b01, reached_b01, perturb_b01,
     {T64_DEVICE_PI, 0u}, {T64_PI_EVENT_COMPLETE, 0u}, true},
    {"B02", "pi-canceled", "test_pi_dma.c cancel (PI reset while active)",
     SI_STEM_ID, build_b02, reached_b02, perturb_b02,
     {T64_DEVICE_PI, 0u}, {T64_PI_EVENT_COMPLETE, 0u}, false},
    {"B03", "sp-active-pending", "test_sp_dma.c pending queue case",
     SI_STEM_ID, build_b03, reached_b03, perturb_b03,
     {T64_DEVICE_SP, 0u}, {T64_SP_EVENT_RSP_ISSUE, 0u}, false},
    {"B04", "rsp-mid-program", "test_rsp_control.c load-store bubble",
     SI_STEM_ID, build_b04, reached_b04, perturb_b04,
     {T64_DEVICE_SP, 0u}, {T64_SP_EVENT_RSP_ISSUE, 0u}, false},
    {"B05", "rdp-pending-words", "test_rdp_triangles.c raw words via DP regs",
     SI_STEM_ID, build_b05, reached_b05, perturb_b05,
     {T64_RDP_DEVICE, 0u}, {T64_RDP_EVENT_WORK, 0u}, false},
    {"B06", "vi-midfield-retained", "test_video_output.c advance_one_field",
     SI_STEM_ID, build_b06, reached_b06, perturb_b06,
     {T64_VI_DEVICE, 0u}, {T64_VI_EVENT_FIELD, 0u}, false},
    {"B07", "vi-stalled-event", "test_video_output.c output-full boundary",
     SI_STEM_ID, build_b07, reached_b07, perturb_b07,
     {T64_VI_DEVICE, 0u}, {T64_VI_EVENT_FIELD, 0u}, true},
    {"B08", "ai-two-descriptors", "test_ai.c prepare_two_descriptor_stream",
     SI_STEM_ID, build_b08, reached_b08, perturb_b08,
     {T64_AI_DEVICE, 0u}, {T64_AI_EVENT_SAMPLE, 0u}, false},
    {"B09", "ai-paused-partial", "test_ai.c pause phase resume",
     SI_STEM_ID, build_b09, reached_b09, perturb_b09,
     {0u, 0u}, {0u, 0u}, false},
    {"B10", "ai-output-full-stall", "test_ai.c output-full same event",
     SI_STEM_ID, build_b10, reached_b10, perturb_b10,
     {T64_AI_DEVICE, 0u}, {T64_AI_EVENT_SAMPLE, 0u}, true},
};

#define BOUNDARY_COUNT (sizeof boundaries / sizeof boundaries[0])

static bool matrix_full(void) {
  const char *value = getenv("T64_STATE_MATRIX_FULL");

  return value != NULL && value[0] == '1';
}

/* The partition steps of a row: one-shot and 7 always, 1, 31 and 4,096 on the
 * full rows and under T64_STATE_MATRIX_FULL (D-40: sampled in the gate preset). */
static size_t partition_steps(const boundary *b, uint64_t steps[5]) {
  size_t count = 0u;

  steps[count++] = 0u;
  if (b->full || matrix_full()) steps[count++] = 1u;
  steps[count++] = 7u;
  if (b->full || matrix_full()) {
    steps[count++] = 31u;
    steps[count++] = 4096u;
  }
  return count;
}

static void matrix_trace(const char *what, const boundary *b, clock_t since) {
  if (getenv("T64_STATE_MATRIX_TRACE") != NULL) {
    fprintf(stderr, "matrix-trace %s %s %.2fs\n", what, b->id,
            (double)(clock() - since) / (double)CLOCKS_PER_SEC);
  }
}

/* ---- the per-row prepared state ----------------------------------------- */

typedef struct {
  bool built;
  bool referenced;
  uint8_t *blob;
  size_t length;
  uint64_t tick;
  uint8_t digest[T64_DIGEST_SIZE];
  uint8_t ref_digest[CHECKPOINT_COUNT][T64_DIGEST_SIZE];
  tuple ref_tuple[CHECKPOINT_COUNT];
} bprep;

static bprep prepared[BOUNDARY_COUNT];

/* Build the boundary machine, assert its reached predicate on the machine
 * before export, export it and keep the blob and the source digest. */
static bool boundary_prepare(size_t index) {
  const boundary *b = &boundaries[index];
  bprep *p = &prepared[index];
  t64_machine *machine;
  const clock_t started = clock();
  const char *unreached;

  if (p->built) return p->blob != NULL;
  p->built = true;
  machine = b->build();
  if (machine == NULL) {
    T64_CHECK(false);
    return false;
  }
  unreached = b->reached(machine);
  if (unreached != NULL) {
    fprintf(stderr, "boundary %s %s not reached: %s\n", b->id, b->name,
            unreached);
    T64_CHECK(false);
  }
  p->blob = export_blob(machine, &p->length);
  p->tick = machine->tick;
  if (p->blob != NULL) (void)digest_of(machine, p->digest);
  t64_machine_destroy(machine);
  matrix_trace("prepare", b, started);
  return p->blob != NULL;
}

/* The source reference: a fresh build advanced one-shot through the four
 * checkpoints, with the tuple and digest at each. */
static bool boundary_reference(size_t index) {
  const boundary *b = &boundaries[index];
  bprep *p = &prepared[index];
  t64_machine *machine;
  const clock_t started = clock();
  size_t c;

  if (!boundary_prepare(index)) return false;
  if (p->referenced) return true;
  p->referenced = true;
  machine = b->build();
  if (machine == NULL) {
    T64_CHECK(false);
    return false;
  }
  T64_CHECK_INT(machine->tick, p->tick);
  for (c = 0u; c < CHECKPOINT_COUNT; c++) {
    if (c == 0u) {
      (void)memset(&p->ref_tuple[0], 0, sizeof p->ref_tuple[0]);
      p->ref_tuple[0].tick = machine->tick;
      p->ref_tuple[0].instructions = machine->instruction_count;
      p->ref_tuple[0].frames = machine->frame_event_count;
    } else {
      run_to(machine, p->tick + checkpoint_offset[c], 0u, &p->ref_tuple[c]);
    }
    (void)digest_of(machine, p->ref_digest[c]);
  }
  t64_machine_destroy(machine);
  matrix_trace("reference", b, started);
  return true;
}

static void boundaries_release(void) {
  size_t i;

  for (i = 0u; i < BOUNDARY_COUNT; i++) {
    free(prepared[i].blob);
    (void)memset(&prepared[i], 0, sizeof prepared[i]);
  }
}

/* An imported peer of the row, or NULL after recording the failure. */
static t64_machine *boundary_peer(size_t index) {
  const boundary *b = &boundaries[index];
  const bprep *p = &prepared[index];
  t64_machine *peer = create_admitted(b->profile);
  t64_state_import_report report;
  t64_status status;

  if (peer == NULL) return NULL;
  status = t64_machine_state_import(peer, p->blob, p->length, 0u, &report);
  if (status != T64_STATUS_OK) {
    fprintf(stderr, "boundary %s import: status %u reason %u section %u\n",
            b->id, (unsigned)status, (unsigned)report.reason,
            (unsigned)report.section_id);
    T64_CHECK(false);
    t64_machine_destroy(peer);
    return NULL;
  }
  return peer;
}

/* ---- the cases ------------------------------------------------------------ */

static void case_boundary_reached(void) {
  size_t i;

  /* the predicate is asserted inside the preparation, on the machine before
   * export; the preparation also leaves a blob that carries the machine tick */
  for (i = 0u; i < BOUNDARY_COUNT; i++) {
    if (boundary_prepare(i)) {
      T64_CHECK_INT(rd64(prepared[i].blob + D_TICK), prepared[i].tick);
      T64_CHECK_INT(rd64(prepared[i].blob + D_TOTAL), prepared[i].length);
    }
  }
}

static void case_boundary_roundtrip(void) {
  size_t i;

  for (i = 0u; i < BOUNDARY_COUNT; i++) {
    const boundary *b = &boundaries[i];
    const clock_t started = clock();
    t64_machine *peer;
    uint8_t digest[T64_DIGEST_SIZE];
    uint8_t *again;
    size_t again_length = 0u;

    if (!boundary_prepare(i)) continue;
    peer = boundary_peer(i);
    if (peer == NULL) continue;
    /* T1: the peer digest equals the source digest and the declared digest */
    T64_CHECK(digest_of(peer, digest));
    T64_CHECK(memcmp(digest, prepared[i].digest, sizeof digest) == 0);
    T64_CHECK(memcmp(digest, prepared[i].blob + D_DECLARED, sizeof digest) == 0);
    T64_CHECK_INT(peer->tick, prepared[i].tick);
    T64_CHECK(handlers_match_production(peer));
    /* T2: export of the import is the source blob, byte for byte */
    again = export_blob(peer, &again_length);
    T64_CHECK_INT(again_length, prepared[i].length);
    T64_CHECK(again != NULL && again_length == prepared[i].length &&
              memcmp(again, prepared[i].blob, again_length) == 0);
    free(again);
    t64_machine_destroy(peer);
    matrix_trace("roundtrip", b, started);
  }
}

static void case_boundary_continuation(void) {
  size_t i;

  for (i = 0u; i < BOUNDARY_COUNT; i++) {
    const boundary *b = &boundaries[i];
    const clock_t started = clock();
    uint64_t steps[5];
    const size_t step_count = partition_steps(b, steps);
    size_t s;

    if (!boundary_reference(i)) continue;
    for (s = 0u; s < step_count; s++) {
      t64_machine *peer = boundary_peer(i);
      size_t c;

      if (peer == NULL) break;
      for (c = 0u; c < CHECKPOINT_COUNT; c++) {
        tuple got;
        uint8_t digest[T64_DIGEST_SIZE];

        if (c == 0u) {
          (void)memset(&got, 0, sizeof got);
          got.tick = peer->tick;
          got.instructions = peer->instruction_count;
          got.frames = peer->frame_event_count;
        } else {
          run_to(peer, prepared[i].tick + checkpoint_offset[c], steps[s],
                 &got);
        }
        (void)digest_of(peer, digest);
        if (!tuples_equal(&got, &prepared[i].ref_tuple[c]) ||
            memcmp(digest, prepared[i].ref_digest[c], sizeof digest) != 0) {
          fprintf(stderr, "boundary %s partition %llu checkpoint %u differs\n",
                  b->id, (unsigned long long)steps[s], (unsigned)c);
          tuple_print("source", &prepared[i].ref_tuple[c]);
          tuple_print("peer  ", &got);
          if (getenv("T64_STATE_MATRIX_TRACE") != NULL) {
            t64_machine *again = b->build();
            tuple ignored;

            if (again != NULL) {
              run_to(again, prepared[i].tick + checkpoint_offset[c], 0u,
                     &ignored);
              debug_diff(b->id, again, peer);
              t64_machine_destroy(again);
            }
          }
        }
        T64_CHECK(tuples_equal(&got, &prepared[i].ref_tuple[c]));
        T64_CHECK(memcmp(digest, prepared[i].ref_digest[c], sizeof digest) == 0);
      }
      t64_machine_destroy(peer);
    }
    matrix_trace("continuation", b, started);
  }
}

static void case_boundary_perturbation(void) {
  size_t i;

  for (i = 0u; i < BOUNDARY_COUNT; i++) {
    const boundary *b = &boundaries[i];
    const clock_t started = clock();
    t64_machine *clean;
    t64_machine *edited;
    tuple got;
    uint8_t digest[T64_DIGEST_SIZE];

    if (!boundary_reference(i)) continue;
    /* the control arm in the same setup: the unperturbed peer agrees at the
     * import checkpoint and at the first continuation checkpoint */
    clean = boundary_peer(i);
    edited = boundary_peer(i);
    if (clean == NULL || edited == NULL) {
      if (clean != NULL) t64_machine_destroy(clean);
      if (edited != NULL) t64_machine_destroy(edited);
      continue;
    }
    T64_CHECK(digest_of(clean, digest));
    T64_CHECK(memcmp(digest, prepared[i].ref_digest[0], sizeof digest) == 0);
    run_to(clean, prepared[i].tick + checkpoint_offset[1], 0u, &got);
    T64_CHECK(digest_of(clean, digest));
    T64_CHECK(memcmp(digest, prepared[i].ref_digest[1], sizeof digest) == 0);
    T64_CHECK(tuples_equal(&got, &prepared[i].ref_tuple[1]));
    /* the perturbed arm: one field of the imported copy changes the digest at
     * the import checkpoint and at the first continuation checkpoint */
    b->perturb(edited);
    T64_CHECK(digest_of(edited, digest));
    if (memcmp(digest, prepared[i].ref_digest[0], sizeof digest) == 0) {
      fprintf(stderr, "boundary %s perturbation invisible at import\n", b->id);
    }
    T64_CHECK(memcmp(digest, prepared[i].ref_digest[0], sizeof digest) != 0);
    run_to(edited, prepared[i].tick + checkpoint_offset[1], 0u, &got);
    T64_CHECK(digest_of(edited, digest));
    if (memcmp(digest, prepared[i].ref_digest[1], sizeof digest) == 0) {
      fprintf(stderr, "boundary %s perturbation invisible at +310\n", b->id);
    }
    T64_CHECK(memcmp(digest, prepared[i].ref_digest[1], sizeof digest) != 0);
    t64_machine_destroy(clean);
    t64_machine_destroy(edited);
    matrix_trace("perturbation", b, started);
  }
}

/* The blob with the first (device, kind) event removed, or with its token
 * increased by one, every integrity field recomputed. */
static uint8_t *blob_edit_pair(const uint8_t *blob, size_t length,
                               uint32_t device, uint32_t kind, bool remove_it,
                               size_t *length_out) {
  tblob t;
  size_t index;
  uint8_t *body;
  long at;

  if (!tblob_decode(blob, length, &t)) return NULL;
  body = scheduler_body(&t, &index);
  at = body == NULL ? -1 : queue_find(body, device, kind);
  if (at < 0) {
    tblob_free(&t);
    return NULL;
  }
  if (remove_it) {
    (void)queue_remove(&t, device, kind);
  } else {
    uint8_t *event = body + Q_HEADER + (size_t)Q_EVENT * (size_t)at;

    put64(event + 32u, rd64(event + 32u) + 1u);
  }
  return encode_edited(&t, length_out);
}

static void refusal_expect(const char *label, t64_machine *peer,
                           const uint8_t *blob, size_t length,
                           uint32_t reason) {
  t64_state_import_report report;
  const size_t allocs = allocate_calls;
  const size_t releases = release_calls;
  const t64_status status =
      t64_machine_state_import(peer, blob, length, 0u, &report);

  if (status != T64_STATUS_STATE_INVARIANT || report.reason != reason ||
      report.section_id != T64_STATE_SECTION_SCHEDULER) {
    fprintf(stderr, "%s: got status %u reason %u section %u, want reason %u\n",
            label, (unsigned)status, (unsigned)report.reason,
            (unsigned)report.section_id, (unsigned)reason);
  }
  T64_CHECK_INT(status, T64_STATUS_STATE_INVARIANT);
  T64_CHECK_INT(report.reason, reason);
  T64_CHECK_INT(report.section_id, T64_STATE_SECTION_SCHEDULER);
  T64_CHECK_INT(report.rule, T64_STATE_RULE_NONE);
  T64_CHECK_INT(allocate_calls, allocs);
  T64_CHECK_INT(release_calls, releases);
  T64_CHECK(t64_state_summary_is_clear(peer));
}

static void case_boundary_event_rules(void) {
  size_t i;

  for (i = 0u; i < BOUNDARY_COUNT; i++) {
    const boundary *b = &boundaries[i];
    const clock_t started = clock();
    size_t pair;

    if (!boundary_prepare(i)) continue;
    if (b->pair_device[0] != 0u) {
      /* the accepted legitimate state first, in the same setup */
      t64_machine *baseline = boundary_peer(i);

      if (baseline != NULL) {
        uint8_t digest[T64_DIGEST_SIZE];

        T64_CHECK(digest_of(baseline, digest));
        T64_CHECK(memcmp(digest, prepared[i].digest, sizeof digest) == 0);
        t64_machine_destroy(baseline);
      }
    }
    for (pair = 0u; pair < 2u; pair++) {
      t64_machine *peer;
      uint8_t *removed;
      uint8_t *skewed;
      size_t removed_length = 0u;
      size_t skewed_length = 0u;
      char label[96];

      if (b->pair_device[pair] == 0u) continue;
      removed = blob_edit_pair(prepared[i].blob, prepared[i].length,
                               b->pair_device[pair], b->pair_kind[pair], true,
                               &removed_length);
      skewed = blob_edit_pair(prepared[i].blob, prepared[i].length,
                              b->pair_device[pair], b->pair_kind[pair], false,
                              &skewed_length);
      peer = create_admitted(b->profile);
      if (removed == NULL || skewed == NULL || peer == NULL) {
        fprintf(stderr, "boundary %s event mutants not built\n", b->id);
        T64_CHECK(false);
      } else {
        (void)snprintf(label, sizeof label, "%s skew (%u,%u)", b->id,
                       (unsigned)b->pair_device[pair],
                       (unsigned)b->pair_kind[pair]);
        refusal_expect(label, peer, skewed, skewed_length,
                       T64_STATE_REASON_DEVICE_TOKEN_MISMATCH);
        (void)snprintf(label, sizeof label, "%s drop (%u,%u)", b->id,
                       (unsigned)b->pair_device[pair],
                       (unsigned)b->pair_kind[pair]);
        if (b->pair_device[pair] == T64_DEVICE_SP &&
            b->pair_kind[pair] == T64_SP_EVENT_RSP_ISSUE) {
          /* The source permits a running RSP without its issue event (an
           * unsupported opcode or an out-of-bank pc leaves it running and not
           * rescheduled, core/t64_rsp.c t64_rsp_issue_event), so the drop
           * mutant is accepted and the machine still returns from a bounded
           * advance: a finding recorded against the plan text, which expected
           * DEVICE_EVENT_MISSING. */
          t64_state_import_report report;
          tuple after;

          T64_CHECK_INT(t64_machine_state_import(peer, removed, removed_length,
                                                 0u, &report),
                        T64_STATUS_OK);
          run_to(peer, peer->tick + 2000u, 0u, &after);
          T64_CHECK_INT(after.status, T64_STATUS_OK);
        } else {
          refusal_expect(label, peer, removed, removed_length,
                         T64_STATE_REASON_DEVICE_EVENT_MISSING);
        }
      }
      free(removed);
      free(skewed);
      if (peer != NULL) t64_machine_destroy(peer);
    }
    matrix_trace("event-rules", b, started);
  }
}


/* ---- the stalled-observer cases (D-29, plan 09-09.1 Task 2) ------------- */

static size_t boundary_index(const char *id) {
  size_t i;

  for (i = 0u; i < BOUNDARY_COUNT; i++) {
    if (strcmp(boundaries[i].id, id) == 0) return i;
  }
  return BOUNDARY_COUNT;
}

static void drain_packets(t64_machine *machine, bool video) {
  uint8_t *buffer = (uint8_t *)malloc(video ? T64_VIDEO_MAX_PACKET_BYTES
                                            : T64_AUDIO_MAX_PACKET_BYTES);
  size_t i;

  if (buffer == NULL) {
    T64_CHECK(false);
    return;
  }
  for (i = 0u; i < 4u && (video ? machine->vi.packet_count
                                : machine->ai.packet_count) > 0u;
       i++) {
    size_t copied = 0u;
    const t64_status status =
        video ? t64_video_copy(machine, buffer, T64_VIDEO_MAX_PACKET_BYTES,
                               &copied)
              : t64_audio_copy(machine, buffer, T64_AUDIO_MAX_PACKET_BYTES,
                               &copied);

    T64_CHECK_INT(status, T64_STATUS_OK);
    if (status != T64_STATUS_OK) break;
  }
  T64_CHECK_INT(video ? machine->vi.packet_count : machine->ai.packet_count, 0u);
  free(buffer);
}

/* The observed stop literals are recorded by the case when T64_STATE_MATRIX_TRACE
 * is set; the expectations below are authored from the cited device source:
 * an occupancy-gated event keeps the machine at the stop tick it reached (the
 * front event is not dispatchable), a default import restores the occupancy and
 * therefore the same stop, and the drop flag opens the gate. */
static const char *const stalled_rows[] = {"B07", "B10"};

static void case_stalled_observer_stop(void) {
  size_t r;

  for (r = 0u; r < 2u; r++) {
    const size_t index = boundary_index(stalled_rows[r]);
    const boundary *b = &boundaries[index];
    t64_machine *source;
    t64_machine *peer;
    tuple expected;
    tuple got;
    uint64_t y;
    uint64_t x0;

    if (index == BOUNDARY_COUNT || !boundary_prepare(index)) continue;
    source = b->build();
    peer = boundary_peer(index);
    if (source == NULL || peer == NULL) {
      if (source != NULL) t64_machine_destroy(source);
      if (peer != NULL) t64_machine_destroy(peer);
      T64_CHECK(false);
      continue;
    }
    T64_CHECK(stalled_front_event(source) != NULL);
    y = stalled_front_event(source)->tick + 1u;
    x0 = source->tick;
    run_to(source, y, 0u, &expected);
    run_to(peer, y, 0u, &got);
    if (getenv("T64_STATE_MATRIX_TRACE") != NULL) {
      fprintf(stderr, "observer-literal %s stalled-at %llu target %llu\n", b->id,
              (unsigned long long)x0, (unsigned long long)y);
      tuple_print("source", &expected);
      tuple_print("default import", &got);
    }
    /* authored literals first: the source stops with the resource-limit reason
     * at the tick of the stall and does not move past it */
    T64_CHECK_INT(expected.status, T64_STATUS_OK);
    T64_CHECK_INT(expected.reason, T64_STOP_REASON_RESOURCE_LIMIT);
    T64_CHECK_INT(expected.stop_tick, x0);
    T64_CHECK_INT(expected.tick, x0);
    /* the default import restores observer continuity: the same stop reason at
     * the same tick with the full tuple equal, and equal digests */
    T64_CHECK(tuples_equal(&expected, &got));
    T64_CHECK_INT(got.reason, T64_STOP_REASON_RESOURCE_LIMIT);
    T64_CHECK_INT(got.stop_tick, x0);
    T64_CHECK(digests_equal(source, peer));
    t64_machine_destroy(source);
    t64_machine_destroy(peer);
  }
}

static void case_drop_observers_outcomes(void) {
  size_t r;

  for (r = 0u; r < 2u; r++) {
    const size_t index = boundary_index(stalled_rows[r]);
    const boundary *b = &boundaries[index];
    const bool video = strcmp(b->id, "B07") == 0;
    t64_machine *source;
    t64_machine *dropped;
    t64_machine *defaulted;
    t64_state_import_report report;
    tuple got;
    uint8_t digest[T64_DIGEST_SIZE];
    uint64_t x0;
    uint64_t y;

    if (index == BOUNDARY_COUNT || !boundary_prepare(index)) continue;
    source = b->build();
    dropped = create_admitted(b->profile);
    defaulted = boundary_peer(index);
    if (source == NULL || dropped == NULL || defaulted == NULL) {
      if (source != NULL) t64_machine_destroy(source);
      if (dropped != NULL) t64_machine_destroy(dropped);
      if (defaulted != NULL) t64_machine_destroy(defaulted);
      T64_CHECK(false);
      continue;
    }
    x0 = source->tick;
    y = stalled_front_event(source)->tick + 1u;
    T64_CHECK_INT(t64_machine_state_import(dropped, prepared[index].blob,
                                           prepared[index].length,
                                           T64_STATE_IMPORT_DROP_OBSERVERS,
                                           &report),
                  T64_STATUS_OK);
    /* the canonical digest is equal at once: observer state is outside it */
    T64_CHECK(digest_of(dropped, digest));
    T64_CHECK(memcmp(digest, prepared[index].digest, sizeof digest) == 0);
    T64_CHECK_INT(video ? dropped->vi.packet_count : dropped->ai.packet_count,
                  0u);
    T64_CHECK(video ? source->vi.packet_count > 0u
                    : source->ai.packet_count > 0u);
    run_to(dropped, y, 0u, &got);
    if (getenv("T64_STATE_MATRIX_TRACE") != NULL) {
      fprintf(stderr, "observer-literal %s drop import, stalled-at %llu\n",
              b->id, (unsigned long long)x0);
      tuple_print("drop import", &got);
    }
    /* the occupancy gate is open: the dispatchable event runs, so the peer is
     * not stopped at the source's stall tick and reports a later point */
    T64_CHECK_INT(got.status, T64_STATUS_OK);
    T64_CHECK(got.tick > x0);
    T64_CHECK(got.reason != T64_STOP_REASON_RESOURCE_LIMIT ||
              got.stop_tick > x0);
    /* after the source drains the stalled packets and both reach the same tick
     * the canonical digests agree across the three machines */
    drain_packets(source, video);
    drain_packets(defaulted, video);
    run_to(source, y, 0u, &got);
    run_to(defaulted, y, 0u, &got);
    T64_CHECK_INT(source->tick, y);
    T64_CHECK_INT(dropped->tick, y);
    T64_CHECK(digests_equal(source, dropped));
    T64_CHECK(digests_equal(source, defaulted));
    t64_machine_destroy(source);
    t64_machine_destroy(dropped);
    t64_machine_destroy(defaulted);
  }
}

/* The case roundtrip: the T1 round trip of the S1 state, then the
 * continuation of the source and of the peer under one-shot and the 1, 7 and
 * 31 tick partitions. */
static void case_roundtrip_all(void) {
  case_roundtrip();
  case_continuation_roundtrip();
}

/* ---- dispatch ---------------------------------------------------------- */

/* The case save-media-roundtrip: the round trip, the generation rule and the
 * hostile-deadline window of the save device. */
static void case_save_media_all(void) {
  case_save_media_roundtrip();
  case_save_deadline_window();
}

int main(int argc, char **argv) {
  static const char *const cases[] = {
      "roundtrip",          "handler-table-ledger", "binding-refusals",
      "container-refusals", "field-range-refusals", "invariant-accepts",
      "invariant-refusals", "scratch-storage",      "refusal-untouched",
      "idempotence",        "save-media-roundtrip", "continuation-partitions",
      "dirty-peer",         "observer-flag",
      "boundary-reached",   "boundary-roundtrip",   "boundary-continuation",
      "boundary-perturbation", "boundary-event-rules",
      "stalled-observer-stop", "drop-observers-outcomes"};
  static const char *const suites[] = {
      "import",   "import",   "refusals", "refusals", "refusals",
      "refusals", "refusals", "refusals", "refusals", "ladder",
      "ladder",   "ladder",   "ladder",   "ladder",
      "boundaries", "boundaries", "boundaries", "boundaries", "boundaries",
      "observers", "observers"};
  static void (*const runners[])(void) = {
      case_roundtrip_all,        case_handler_table_ledger,
      case_binding_refusals,     case_container_refusals,
      case_field_range_refusals, case_invariant_accepts,
      case_invariant_refusals,   case_scratch_storage,
      case_refusal_untouched,    case_idempotence,
      case_save_media_all,       case_continuation_partitions,
      case_dirty_peer,           case_observer_flag,
      case_boundary_reached,     case_boundary_roundtrip,
      case_boundary_continuation, case_boundary_perturbation,
      case_boundary_event_rules, case_stalled_observer_stop,
      case_drop_observers_outcomes};
  const size_t count = sizeof cases / sizeof cases[0];
  size_t index;

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (index = 0u; index < count; index++) printf("%s\n", cases[index]);
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    for (index = 0u; index < count; index++) {
      if (strcmp(argv[2], cases[index]) == 0) {
        char name[96];

        runners[index]();
        peers_release();
        targets_release();
        boundaries_release();
        (void)snprintf(name, sizeof name, "unit.core.state.import.%s",
                       cases[index]);
        return T64_TEST_FINISH(name);
      }
    }
    fprintf(stderr, "unknown case: %s\n", argv[2]);
    return 2;
  }
  if (argc == 3 && strcmp(argv[1], "--suite") == 0) {
    bool found = false;
    char name[96];

    for (index = 0u; index < count; index++) {
      if (strcmp(argv[2], suites[index]) == 0) {
        runners[index]();
        peers_release();
        found = true;
      }
    }
    if (!found) {
      fprintf(stderr, "unknown suite: %s\n", argv[2]);
      return 2;
    }
    targets_release();
    boundaries_release();
    (void)snprintf(name, sizeof name, "unit.core.state.import.%s", argv[2]);
    return T64_TEST_FINISH(name);
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--case NAME|--suite NAME]\n",
            argv[0]);
    return 2;
  }
  for (index = 0u; index < count; index++) runners[index]();
  peers_release();
  targets_release();
  boundaries_release();
  return T64_TEST_FINISH("unit.core.state.import");
}
