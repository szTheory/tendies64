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
#define S8_IDLE_LENGTH 4200260u
/* The tail of section 8 (plan 09-10): the combiner block (88 bytes: LOD fraction,
 * environment, fog and blend colors, combine mode, seven flags at +24, key
 * center, scale and width of three channels from +52), then the coverage table
 * size word at +88 and the table at +92. */
#define S8_TAIL (S8_IDLE_LENGTH - 2097244u)
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
    /* a report span inside live storage is refused and never written, for a
     * valid blob and for every argument refusal that precedes the span check
     * (plan 09-10 review L2 WR-01: a NULL blob, a zero or oversize length and
     * a bad flag word used to write the report into the machine); each
     * refusal runs first with a disjoint report span, so the status is the one
     * the aliasing call must keep */
    {
      uint8_t before[sizeof report];
      t64_state_import_report *inside =
          (t64_state_import_report *)(void *)(peer->ram + 64u);
      const struct {
        const char *name;
        const uint8_t *bytes;
        size_t length;
        uint32_t flags;
        uint32_t reason;
      } refusals[] = {
          {"valid blob", blob, length, 0u, 0u},
          {"NULL blob", NULL, length, 0u, T64_STATE_REASON_ARG_NULL},
          {"zero length", blob, 0u, 0u, T64_STATE_REASON_ARG_LENGTH},
          {"length above the ceiling", &byte,
           (size_t)T64_STATE_MAX_BYTES + 1u, 0u, T64_STATE_REASON_ARG_LENGTH},
          {"bad flags", blob, length, 2u, T64_STATE_REASON_ARG_FLAGS},
          {"blob in live storage", peer->ram, 4096u, 0u,
           T64_STATE_REASON_ARG_OVERLAP_LIVE},
      };
      size_t arm;

      for (arm = 0u; arm < sizeof refusals / sizeof refusals[0]; arm++) {
        t64_state_import_report disjoint;
        t64_status want;

        want = t64_machine_state_import(peer, refusals[arm].bytes,
                                        refusals[arm].length,
                                        refusals[arm].flags, &disjoint);
        if (refusals[arm].reason != 0u) {
          /* the accepted arm of the same setup: a usable report span records
           * the reason */
          T64_CHECK_INT(want, T64_STATUS_INVALID_ARGUMENT);
          T64_CHECK_INT(disjoint.reason, refusals[arm].reason);
        }
        (void)memset(inside, 0xA5, sizeof before); /* a canary per row */
        (void)memcpy(before, inside, sizeof before);
        T64_CHECK_INT(t64_machine_state_import(peer, refusals[arm].bytes,
                                               refusals[arm].length,
                                               refusals[arm].flags, inside),
                      refusals[arm].reason == 0u ? T64_STATUS_INVALID_ARGUMENT
                                                 : want);
        if (memcmp(before, inside, sizeof before) != 0) {
          fprintf(stderr, "aliasing report span written for: %s\n",
                  refusals[arm].name);
        }
        T64_CHECK(memcmp(before, inside, sizeof before) == 0);
      }
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
  void (*peer_edit)(t64_machine *peer);  /* applied to every fresh peer, or NULL */
  void (*extra)(size_t index);           /* row-specific continuation checks */
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

/* The RDP memory-coverage marks under the source pixels are part of the v25
 * state since plan 09-10, so the next field the VI scans out reads the same
 * marks on the peer (it retained 8 source bytes against 16 before the carry);
 * the rows B06 and B07 therefore build the frame with the marks. */
static t64_machine *build_b06(void) {
  return build_vi_frame_with(true);
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
  t64_machine *machine = build_vi_frame_with(true);
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

/* ---- rows B11 to B16 (plan 09-09.1 Task 3) -------------------------------- */

static void extra_b13(size_t index);
static void extra_b15(size_t index);
static void peer_edit_b13(t64_machine *peer);

/* B11 cpu-delay-dirty-tlb: a store through cached KSEG0 leaves a dirty D-cache
 * line, a TLB pair is installed by a direct test-only state edit (the pattern of
 * test_cpu_path.c install_tlb_pair) and the stop falls between a jump and its
 * delay slot (source: test_cpu_time.c delay-slot sequence). */
static t64_machine *build_b11(void) {
  static const uint32_t program[] = {
      UINT32_C(0x3c088000), /* lui t0,0x8000 */
      UINT32_C(0x3c09a5a5), /* lui t1,0xa5a5 */
      UINT32_C(0xad092000), /* sw  t1,0x2000(t0): a cached store */
      UINT32_C(0x08000403), /* j   0x8000100c (this jump) */
      UINT32_C(0x00000000)  /* nop: its delay slot */};
  t64_machine *machine = create_admitted(SI_STEM_ID);
  t64_vr4300_tlb_pair *entry;
  size_t i;

  if (machine == NULL) return NULL;
  for (i = 0u; i < sizeof program / sizeof program[0]; i++) {
    put32(machine->ram + 0x1000u + 4u * i, program[i]);
  }
  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = t64_cpu_canonical_address(machine->cpu.pc);
  entry = &machine->cpu.tlb[2];
  entry->page_mask = 0u;
  entry->vpn2 = UINT64_C(0x6000);
  entry->region = 0u;
  entry->asid = 0u;
  entry->entry_lo0 = (uint64_t)(((UINT32_C(0x6000) >> 12) << 6) |
                                (3u << 3) | 2u | 4u);
  entry->entry_lo1 = entry->entry_lo0;
  for (i = 0u; i < 64u; i++) {
    if (machine->cpu.delay_slot_pending && i > 4u) break;
    T64_CHECK(advance_to(machine, machine->tick + 1u));
  }
  return machine;
}

static size_t first_dirty_line(const t64_machine *machine) {
  size_t i;

  for (i = 0u; i < T64_VR4300_DCACHE_LINE_COUNT; i++) {
    if (machine->cpu.dcache[i].dirty) return i;
  }
  return T64_VR4300_DCACHE_LINE_COUNT;
}

static const char *reached_b11(const t64_machine *machine) {
  if (!machine->cpu.delay_slot_pending) return "cpu.delay_slot_pending";
  if (first_dirty_line(machine) == T64_VR4300_DCACHE_LINE_COUNT) {
    return "a dirty dcache line";
  }
  if ((machine->cpu.tlb[2].entry_lo0 & 2u) == 0u) return "tlb[2] valid";
  if (find_event(machine, T64_DEVICE_CPU, T64_CPU_EVENT_STEP) == NULL) {
    return "queue (CPU, STEP)";
  }
  return NULL;
}

static void perturb_b11(t64_machine *machine) {
  const size_t line = first_dirty_line(machine);

  if (line < T64_VR4300_DCACHE_LINE_COUNT) machine->cpu.dcache[line].data[0] ^= 1u;
}

/* B12 coincident-events: a PI completion and the CPU step of the same tick, two
 * production events of different devices (source: test_determinism.c same-tick
 * order). The transfer length is searched over 16 values for the first one whose
 * completion tick equals the next CPU step tick. */
static bool same_tick_pair(const t64_machine *machine) {
  const t64_event *pi = find_event(machine, T64_DEVICE_PI, T64_PI_EVENT_COMPLETE);
  const t64_event *cpu = find_event(machine, T64_DEVICE_CPU, T64_CPU_EVENT_STEP);

  return pi != NULL && cpu != NULL && pi->tick == cpu->tick;
}

static t64_machine *build_b12(void) {
  uint32_t length;

  for (length = 15u; length < 15u + 16u * 8u; length += 8u) {
    t64_machine *machine = create_admitted(SI_STEM_ID);
    const t64_event *pi;

    if (machine == NULL) return NULL;
    T64_CHECK(advance_to(machine, 2000u));
    bus_write32(machine, PI_BASE, UINT32_C(0x3A00));
    bus_write32(machine, PI_BASE + 4u, CART_BASE + UINT32_C(0x100));
    bus_write32(machine, PI_BASE + 12u, length);
    pi = find_event(machine, T64_DEVICE_PI, T64_PI_EVENT_COMPLETE);
    if (pi != NULL && pi->tick > machine->tick + 1u &&
        advance_to(machine, pi->tick - 1u) && same_tick_pair(machine)) {
      return machine;
    }
    t64_machine_destroy(machine);
  }
  return NULL;
}

static const char *reached_b12(const t64_machine *machine) {
  const t64_event *pi = find_event(machine, T64_DEVICE_PI, T64_PI_EVENT_COMPLETE);

  if (pi == NULL) return "queue (PI, COMPLETE)";
  if (!same_tick_pair(machine)) return "CPU step on the PI completion tick";
  if (pi->device == T64_DEVICE_CPU) return "distinct devices";
  return NULL;
}

/* The plan swaps the two sequence numbers; both events are popped before the
 * first continuation checkpoint, so that edit leaves no trace there. The PI
 * token persists (a stale completion never copies). */
static void perturb_b12(t64_machine *machine) {
  size_t i;

  for (i = 0u; i < machine->queue_count; i++) {
    t64_event *event =
        &machine->events[(machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];

    if (event->device == T64_DEVICE_PI && event->kind == T64_PI_EVENT_COMPLETE) {
      event->token += 1u;
    }
  }
}

/* B13 near-exhaustion: the profile copy of the queue capacity and the
 * sequence counter are edited in memory after create, identically on the
 * source and on every peer (never through a mutated descriptor): room for two
 * more events and four sequence numbers (source: test_scheduler.c transactional
 * exhaustion). */
static uint32_t b13_capacity;

static t64_machine *build_b13(void) {
  t64_machine *machine = build_quiet();

  if (machine == NULL) return NULL;
  b13_capacity = (uint32_t)machine->queue_count + 2u;
  machine->profile.event_queue_capacity = b13_capacity;
  machine->next_sequence = UINT64_MAX - 4u;
  return machine;
}

static void peer_edit_b13(t64_machine *peer) {
  peer->profile.event_queue_capacity = b13_capacity;
}

static const char *reached_b13(const t64_machine *machine) {
  if (machine->profile.sequence_width_bits != 64u) return "sequence_width_bits";
  if (machine->profile.event_queue_capacity - machine->queue_count != 2u) {
    return "queue room 2";
  }
  if (UINT64_MAX - machine->next_sequence != 4u) return "sequence headroom 4";
  return NULL;
}

static void perturb_b13(t64_machine *machine) {
  machine->next_sequence -= 1u;
}

/* B14 si-mid-transaction: the 09-09 S1 state, an RD64B in flight with its
 * execute event pending. */
static t64_machine *build_b14(void) {
  return build_s1(SI_STEM_ID);
}

static const char *reached_b14(const t64_machine *machine) {
  if (!machine->si.active_valid) return "si.active_valid";
  if (!machine->si.execute_pending) return "si.execute_pending";
  if (machine->si.active_direction != T64_SI_DIRECTION_RD64B) {
    return "si.active_direction";
  }
  if (find_event(machine, T64_SI_DEVICE, T64_SI_EVENT_EXECUTE) == NULL) {
    return "queue (SI, EXECUTE)";
  }
  if (find_event(machine, T64_SI_DEVICE, T64_SI_EVENT_COMPLETE) == NULL) {
    return "queue (SI, COMPLETE)";
  }
  return NULL;
}

static void perturb_b14(t64_machine *machine) {
  machine->si.execute_tick += 1u;
}

/* B15 eeprom-busy: on the 4 Kbit EEPROM id a block write through channel 4
 * (command 0x05) started by the SI frame path (a WR64B of the frame, then an
 * RD64B that executes it), stopped after the read completes while the busy
 * window is open (source: the 09-04 tracer of test_save.c). */
#define EEPROM_BUSY_WINDOW UINT64_C(375000)
#define H_READ_TICKS UINT64_C(20876)
#define H_WRITE_TICKS UINT64_C(22876)
#define SEND_DRAM UINT32_C(0x3800)
#define RECEIVE_DRAM UINT32_C(0x3900)
#define SI_STATUS_REG UINT32_C(0x04800018)

static const uint8_t eeprom_write_tx[10] = {0x05, 0x01, 0x11, 0x12, 0x13,
                                            0x14, 0x15, 0x16, 0x17, 0x18};
static const uint8_t eeprom_read_tx[2] = {0x04, 0x01};

static void eeprom_frame(uint8_t frame[64], uint32_t tx_count,
                         uint32_t rx_count, const uint8_t *tx) {
  (void)memset(frame, 0, 64u);
  frame[4] = (uint8_t)tx_count;
  frame[5] = (uint8_t)rx_count;
  (void)memcpy(frame + 6, tx, tx_count);
  (void)memset(frame + 6 + tx_count, 0xFF, rx_count);
  frame[6 + tx_count + rx_count] = 0xFE;
  frame[63] = 0x01;
}

/* A WR64B of the frame at the current tick, settled and acknowledged. */
static void eeprom_send(t64_machine *machine, const uint8_t frame[64]) {
  const uint64_t start = machine->tick;

  (void)memcpy(machine->ram + SEND_DRAM, frame, 64u);
  bus_write32(machine, SI_DRAM_ADDR, SEND_DRAM);
  bus_write32(machine, SI_WR64B, PIF_ADDRESS);
  T64_CHECK(advance_to(machine, start + 2500u));
  bus_write32(machine, SI_STATUS_REG, 0u);
}

/* An RD64B started at `start` (the destination pre-filled with 0xAA), settled
 * at the completion tick; the image the guest finds is returned. */
static void eeprom_receive(t64_machine *machine, uint64_t start,
                           uint8_t image[64]) {
  T64_CHECK(advance_to(machine, start));
  (void)memset(machine->ram + RECEIVE_DRAM, 0xAA, 64u);
  bus_write32(machine, SI_DRAM_ADDR, RECEIVE_DRAM);
  bus_write32(machine, SI_RD64B, PIF_ADDRESS);
  T64_CHECK(advance_to(machine, machine->si.complete_tick));
  bus_write32(machine, SI_STATUS_REG, 0u);
  (void)memcpy(image, machine->ram + RECEIVE_DRAM, 64u);
}

static t64_machine *build_b15(void) {
  t64_machine *machine = create_admitted(SAVE_E4K_ID);
  uint8_t frame[64];
  uint8_t image[64];

  if (machine == NULL) return NULL;
  machine->ram[0x1000] = 0x08u;
  machine->ram[0x1001] = 0x00u;
  machine->ram[0x1002] = 0x04u;
  machine->ram[0x1003] = 0x00u;
  (void)memset(machine->ram + 0x1004, 0, 4u);
  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = t64_cpu_canonical_address(machine->cpu.pc);
  T64_CHECK(advance_to(machine, 3000u));
  eeprom_frame(frame, 10u, 1u, eeprom_write_tx);
  eeprom_send(machine, frame);
  eeprom_receive(machine, 10000u, image);
  T64_CHECK_INT(image[5], 0x01u);
  return machine;
}

static const char *reached_b15(const t64_machine *machine) {
  if (machine->save.busy_until <= machine->tick) return "save.busy_until";
  if (machine->si.active_valid) return "si idle";
  return NULL;
}

static void perturb_b15(t64_machine *machine) {
  machine->save.busy_until += 1u;
}

/* B16 pending-input: an input event stamped 20,000 ticks ahead of the S1
 * state, queued and not dispatched (source: t64_machine_queue_input). */
static t64_machine *build_b16(void) {
  t64_machine *machine = build_s1(SI_STEM_ID);

  if (machine == NULL) return NULL;
  T64_CHECK_INT(t64_machine_queue_input(machine, machine->tick + 20000u, 0u,
                                        UINT16_C(0x8000), 1, -1),
                T64_STATUS_OK);
  return machine;
}

static const char *reached_b16(const t64_machine *machine) {
  const t64_event *event = find_event(machine, T64_TRACE_DEVICE_INPUT,
                                      T64_TRACE_KIND_INPUT_SUBMITTED);

  if (event == NULL) return "queue (INPUT, SUBMITTED)";
  if (event->tick <= machine->tick) return "input tick above the tick";
  return NULL;
}

static void perturb_b16(t64_machine *machine) {
  size_t i;

  for (i = 0u; i < machine->queue_count; i++) {
    t64_event *event =
        &machine->events[(machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];

    if (event->device == T64_TRACE_DEVICE_INPUT &&
        event->kind == T64_TRACE_KIND_INPUT_SUBMITTED) {
      event->value ^= UINT64_C(1);
    }
  }
}

/* ---- B17 and the carried RDP members (plan 09-10, STATE-CONTENT-F1) ------- */

#define KEY_GB_WORD                                                          \
  ((UINT64_C(0x2a) << 56) | (UINT64_C(0x123) << 44) |                        \
   (UINT64_C(0x234) << 32) | (UINT64_C(0x11) << 24) |                        \
   (UINT64_C(0x22) << 16) | (UINT64_C(0x33) << 8) | UINT64_C(0x44))
#define KEY_R_WORD                                                           \
  ((UINT64_C(0x2b) << 56) | (UINT64_C(0x345) << 16) | (UINT64_C(0x55) << 8) | \
   UINT64_C(0x66))

/* A command list through the real command path: an 8 by 8 fill (the rectangle
 * includes its last row, so it marks 8 pixels in each of 9 rows: 72 coverage
 * entries), then the one-cycle mode, the primitive, environment, fog,
 * blend and key registers and the combine mode, the members the family needs
 * for every triangle. */
static const uint64_t rdp_carry_setup[] = {
    UINT64_C(0x3f10000700005000), /* RGBA16 color image, width 8 */
    UINT64_C(0x2d00000000020020), /* scissor [0,8) x [0,8) */
    UINT64_C(0x2f30000000000000), /* fill cycle type */
    UINT64_C(0x37000000f80107c1), /* fill color */
    UINT64_C(0x3602002000000000), /* fill rectangle 8 x 8 */
    UINT64_C(0x2f00000000000000), /* one-cycle mode */
    UINT64_C(0x3a000000c8c8c8ff), /* primitive color */
    UINT64_C(0x3b000000505050ff), /* environment color */
    UINT64_C(0x38000000a1b2c3d4), /* fog color */
    UINT64_C(0x39000000e1f20304), /* blend color */
    KEY_GB_WORD,                  /* key green and blue */
    KEY_R_WORD,                   /* key red */
    UINT64_C(0xfc55566c8877ffff)};/* combine mode */
static const uint64_t rdp_carry_triangle[] = {
    UINT64_C(0x08000014000c0004), /* edge: rows 1 to 4 */
    UINT64_C(0x00010000fffe8000), UINT64_C(0x0001000000000000),
    UINT64_C(0x0004000000018000)};
#define RDP_CARRY_COUNT (sizeof rdp_carry_setup / sizeof rdp_carry_setup[0])

static void rdp_submit(t64_machine *machine, const uint64_t *words,
                       size_t count) {
  size_t i;

  for (i = 0u; i < count; i++) {
    put_be64(machine->ram + B05_COMMAND_ADDRESS + 8u * i, words[i]);
  }
  bus_write32(machine, DP_START_REG, B05_COMMAND_ADDRESS);
  bus_write32(machine, DP_START_REG + 4u,
              B05_COMMAND_ADDRESS + (uint32_t)(8u * count));
}

static bool rdp_is_idle(const t64_machine *machine) {
  return !machine->rdp.busy && !machine->rdp.render_active &&
         !machine->rdp.command_pending && !machine->rdp.fence_pending &&
         !machine->rdp.texrect_pending;
}

/* The machine advances one tick at a time (the RDP works in its own events)
 * until the command list is done and the RDP is idle. */
static void rdp_run_idle(t64_machine *machine) {
  size_t i;

  for (i = 0u; i < 3000u && !rdp_is_idle(machine); i++) {
    T64_CHECK(advance_to(machine, machine->tick + 1u));
  }
  T64_CHECK(rdp_is_idle(machine));
}

static size_t coverage_marks(const t64_machine *machine) {
  const size_t table = T64_RDP_MEMORY_COVERAGE_BYTES(machine->profile.rdram_bytes);
  size_t i;
  size_t marks = 0u;

  for (i = 0u; i < table; i++) {
    if (machine->rdp_memory_coverage[i] != 0u) marks++;
  }
  return marks;
}

/* B17 rdp-triangle-render: the setup above and a flat triangle after it, the
 * machine stopped after the first row of the triangle. */
static t64_machine *build_b17(void) {
  uint64_t words[RDP_CARRY_COUNT + 4u];
  t64_machine *machine = build_quiet();
  size_t i;

  if (machine == NULL) return NULL;
  (void)memcpy(words, rdp_carry_setup, sizeof rdp_carry_setup);
  (void)memcpy(words + RDP_CARRY_COUNT, rdp_carry_triangle,
               sizeof rdp_carry_triangle);
  rdp_submit(machine, words, RDP_CARRY_COUNT + 4u);
  for (i = 0u; i < 3000u && !(machine->rdp.render_active &&
                              machine->rdp.render_triangle &&
                              machine->rdp.render_y > machine->rdp.render_y_begin);
       i++) {
    T64_CHECK(advance_to(machine, machine->tick + 1u));
  }
  return machine;
}

static const char *reached_b17(const t64_machine *machine) {
  const t64_rdp_state *rdp = &machine->rdp;

  if (!rdp->render_active || !rdp->render_triangle) return "rdp.render_triangle";
  if (rdp->render_y <= rdp->render_y_begin) return "rdp.render_y";
  if (!rdp->combine_mode_valid || !rdp->environment_color_valid ||
      !rdp->fog_color_valid || !rdp->blend_color_valid ||
      !rdp->key_center_valid || !rdp->key_scale_valid ||
      !rdp->key_width_valid) {
    return "rdp combiner block";
  }
  if (coverage_marks(machine) != 72u) return "rdp_memory_coverage";
  if (find_event(machine, T64_RDP_DEVICE, T64_RDP_EVENT_WORK) == NULL) {
    return "queue (RDP, WORK)";
  }
  return NULL;
}

static void perturb_b17(t64_machine *machine) {
  machine->rdp.fog_color ^= UINT32_C(0x00000100);
}

static const boundary boundaries[] = {
    {"B01", "pi-active", "test_pi_dma.c busy start, stem id", SI_STEM_ID,
     build_b01, reached_b01, perturb_b01,
     {T64_DEVICE_PI, 0u}, {T64_PI_EVENT_COMPLETE, 0u}, true, NULL, NULL},
    {"B02", "pi-canceled", "test_pi_dma.c cancel (PI reset while active)",
     SI_STEM_ID, build_b02, reached_b02, perturb_b02,
     {T64_DEVICE_PI, 0u}, {T64_PI_EVENT_COMPLETE, 0u}, false, NULL, NULL},
    {"B03", "sp-active-pending", "test_sp_dma.c pending queue case",
     SI_STEM_ID, build_b03, reached_b03, perturb_b03,
     {T64_DEVICE_SP, 0u}, {T64_SP_EVENT_RSP_ISSUE, 0u}, false, NULL, NULL},
    {"B04", "rsp-mid-program", "test_rsp_control.c load-store bubble",
     SI_STEM_ID, build_b04, reached_b04, perturb_b04,
     {T64_DEVICE_SP, 0u}, {T64_SP_EVENT_RSP_ISSUE, 0u}, false, NULL, NULL},
    {"B05", "rdp-pending-words", "test_rdp_triangles.c raw words via DP regs",
     SI_STEM_ID, build_b05, reached_b05, perturb_b05,
     {T64_RDP_DEVICE, 0u}, {T64_RDP_EVENT_WORK, 0u}, false, NULL, NULL},
    {"B06", "vi-midfield-retained", "test_video_output.c advance_one_field",
     SI_STEM_ID, build_b06, reached_b06, perturb_b06,
     {T64_VI_DEVICE, 0u}, {T64_VI_EVENT_FIELD, 0u}, false, NULL, NULL},
    {"B07", "vi-stalled-event", "test_video_output.c output-full boundary",
     SI_STEM_ID, build_b07, reached_b07, perturb_b07,
     {T64_VI_DEVICE, 0u}, {T64_VI_EVENT_FIELD, 0u}, true, NULL, NULL},
    {"B08", "ai-two-descriptors", "test_ai.c prepare_two_descriptor_stream",
     SI_STEM_ID, build_b08, reached_b08, perturb_b08,
     {T64_AI_DEVICE, 0u}, {T64_AI_EVENT_SAMPLE, 0u}, false, NULL, NULL},
    {"B09", "ai-paused-partial", "test_ai.c pause phase resume",
     SI_STEM_ID, build_b09, reached_b09, perturb_b09,
     {0u, 0u}, {0u, 0u}, false, NULL, NULL},
    {"B10", "ai-output-full-stall", "test_ai.c output-full same event",
     SI_STEM_ID, build_b10, reached_b10, perturb_b10,
     {T64_AI_DEVICE, 0u}, {T64_AI_EVENT_SAMPLE, 0u}, true, NULL, NULL},
    {"B11", "cpu-delay-dirty-tlb", "test_cpu_time.c delay slot, test_cpu_path.c TLB pair",
     SI_STEM_ID, build_b11, reached_b11, perturb_b11,
     {T64_DEVICE_CPU, 0u}, {T64_CPU_EVENT_STEP, 0u}, false, NULL, NULL},
    {"B12", "coincident-events", "test_determinism.c same-tick order",
     SI_STEM_ID, build_b12, reached_b12, perturb_b12,
     {T64_DEVICE_PI, T64_DEVICE_CPU}, {T64_PI_EVENT_COMPLETE, T64_CPU_EVENT_STEP},
     false, NULL, NULL},
    {"B13", "near-exhaustion", "test_scheduler.c transactional exhaustion",
     SI_STEM_ID, build_b13, reached_b13, perturb_b13,
     {T64_DEVICE_CPU, 0u}, {T64_CPU_EVENT_STEP, 0u}, false, peer_edit_b13,
     extra_b13},
    {"B14", "si-mid-transaction", "the 09-09 S1 builder (RD64B before its execute)",
     SI_STEM_ID, build_b14, reached_b14, perturb_b14,
     {T64_SI_DEVICE, T64_SI_DEVICE}, {T64_SI_EVENT_EXECUTE, T64_SI_EVENT_COMPLETE},
     true, NULL, NULL},
    {"B15", "eeprom-busy", "test_save.c tracer (EEPROM write through channel 4)",
     SAVE_E4K_ID, build_b15, reached_b15, perturb_b15,
     {0u, 0u}, {0u, 0u}, false, NULL, extra_b15},
    {"B16", "pending-input", "t64_machine_queue_input 20,000 ticks ahead on S1",
     SI_STEM_ID, build_b16, reached_b16, perturb_b16,
     {T64_TRACE_DEVICE_INPUT, 0u}, {T64_TRACE_KIND_INPUT_SUBMITTED, 0u}, false,
     NULL, NULL},
    {"B17", "rdp-triangle-render", "test_rdp_triangles.c edge words after a fill and the combiner registers",
     SI_STEM_ID, build_b17, reached_b17, perturb_b17,
     {T64_RDP_DEVICE, 0u}, {T64_RDP_EVENT_WORK, 0u}, false, NULL, NULL},
};

#define BOUNDARY_COUNT (sizeof boundaries / sizeof boundaries[0])

static bool matrix_full(void) {
  const char *value = getenv("T64_STATE_MATRIX_FULL");

  return value != NULL && value[0] == '1';
}

/* The partition steps of a row. Every row compares the imported peer, advanced
 * in partitions of 7 ticks, with the one-shot source reference (so import
 * equivalence and partition invariance are checked by one run); the full rows
 * and T64_STATE_MATRIX_FULL add the one-shot peer and the partitions of 1, 31
 * and 4,096 ticks (D-40: the gate preset is a sample). */
static size_t partition_steps(const boundary *b, uint64_t steps[5]) {
  size_t count = 0u;

  if (b->full || matrix_full()) {
    steps[count++] = 0u;
    steps[count++] = 1u;
  }
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
      /* the digest of the build is the digest prepared before the export */
      (void)memcpy(p->ref_digest[0], p->digest, sizeof p->digest);
      continue;
    }
    run_to(machine, p->tick + checkpoint_offset[c], 0u, &p->ref_tuple[c]);
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
  if (b->peer_edit != NULL) b->peer_edit(peer);
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

/* The row-specific continuation checks (rows with an extra) -------------- */

/* B13: the identically edited peer is the generic path; here the other peers
 * of the same blob. Literals are recorded when T64_STATE_MATRIX_TRACE is set. */
static void extra_b13(size_t index) {
  const boundary *b = &boundaries[index];
  t64_state_import_report report;
  t64_machine *unedited = create_admitted(b->profile);
  t64_machine *shrunk = create_admitted(b->profile);
  t64_status status;

  if (unedited == NULL || shrunk == NULL) {
    T64_CHECK(false);
  } else {
    status = t64_machine_state_import(unedited, prepared[index].blob,
                                      prepared[index].length, 0u, &report);
    if (getenv("T64_STATE_MATRIX_TRACE") != NULL) {
      fprintf(stderr, "b13 unedited peer: status %u reason %u section %u rule %u\n",
              (unsigned)status, (unsigned)report.reason,
              (unsigned)report.section_id, (unsigned)report.rule);
    }
    /* the capacity is a profile member, not a blob member: a peer created
     * without the identical in-memory edit (room for 256 events) accepts the
     * blob and, because the exhaustion is the sequence counter's, continues
     * with the same result tuple */
    T64_CHECK_INT(status, T64_STATUS_OK);
    if (status == T64_STATUS_OK) {
      tuple got;

      run_to(unedited, prepared[index].tick + checkpoint_offset[1], 0u, &got);
      T64_CHECK(tuples_equal(&got, &prepared[index].ref_tuple[1]));
      /* the exhaustion is a resource-limit stop short of the checkpoint (the
       * literal on the gate build: tick 6 after five CPU steps) */
      T64_CHECK_INT(got.status, T64_STATUS_OK);
      T64_CHECK_INT(got.reason, T64_STOP_REASON_RESOURCE_LIMIT);
      T64_CHECK(got.tick < prepared[index].tick + checkpoint_offset[1]);
      T64_CHECK_INT(got.instructions, 5u);
    }
    /* a peer whose capacity is below the blob queue count is refused at the
     * scheduler section, leaving the machine untouched */
    shrunk->profile.event_queue_capacity = 0u;
    status = t64_machine_state_import(shrunk, prepared[index].blob,
                                      prepared[index].length, 0u, &report);
    if (getenv("T64_STATE_MATRIX_TRACE") != NULL) {
      fprintf(stderr, "b13 capacity 0 peer: status %u reason %u section %u rule %u\n",
              (unsigned)status, (unsigned)report.reason,
              (unsigned)report.section_id, (unsigned)report.rule);
    }
    T64_CHECK_INT(status, T64_STATUS_STATE_INVARIANT);
    T64_CHECK_INT(report.reason, T64_STATE_REASON_QUEUE_COUNT);
    T64_CHECK_INT(report.section_id, T64_STATE_SECTION_SCHEDULER);
    T64_CHECK_INT(report.rule, T64_STATE_RULE_COUNT_CAP);
    T64_CHECK(t64_state_summary_is_clear(shrunk));
  }
  if (unedited != NULL) t64_machine_destroy(unedited);
  if (shrunk != NULL) t64_machine_destroy(shrunk);
}

/* B15: the busy-window read across the boundary on the source and on an
 * imported peer: busy one tick before the deadline, idle at it, and the images
 * agree. The read executes at deadline + delta, so it starts H_READ before. */
static void eeprom_probe(t64_machine *machine, int64_t delta, uint8_t image[64]) {
  uint8_t frame[64];
  const uint64_t deadline = machine->save.busy_until;
  const uint64_t execute = (uint64_t)((int64_t)deadline + delta);
  const uint64_t start = execute - H_READ_TICKS;

  T64_CHECK(advance_to(machine, start - 2600u));
  eeprom_frame(frame, 2u, 8u, eeprom_read_tx);
  eeprom_send(machine, frame);
  eeprom_receive(machine, start, image);
}

static void extra_b15(size_t index) {
  const boundary *b = &boundaries[index];
  size_t k;

  for (k = 0u; k < 2u; k++) {
    const int64_t delta = k == 0u ? -1 : 0;
    t64_machine *source = b->build();
    t64_machine *peer = boundary_peer(index);
    uint8_t source_image[64];
    uint8_t peer_image[64];
    size_t i;

    if (source == NULL || peer == NULL) {
      T64_CHECK(false);
      if (source != NULL) t64_machine_destroy(source);
      if (peer != NULL) t64_machine_destroy(peer);
      continue;
    }
    T64_CHECK_INT(peer->save.busy_until, source->save.busy_until);
    eeprom_probe(source, delta, source_image);
    eeprom_probe(peer, delta, peer_image);
    T64_CHECK(memcmp(source_image, peer_image, sizeof source_image) == 0);
    if (delta < 0) {
      /* one tick before the deadline: the read times out */
      T64_CHECK((source_image[5] & 0x40u) != 0u);
      T64_CHECK((peer_image[5] & 0x40u) != 0u);
    } else {
      /* at the deadline: idle, the block written before the export */
      T64_CHECK_INT(source_image[5], 0x08u);
      T64_CHECK_INT(peer_image[5], 0x08u);
      for (i = 0u; i < 8u; i++) {
        T64_CHECK_INT(source_image[8u + i], 0x11u + i);
        T64_CHECK_INT(peer_image[8u + i], 0x11u + i);
      }
    }
    T64_CHECK(digests_equal(source, peer));
    t64_machine_destroy(source);
    t64_machine_destroy(peer);
  }
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
          /* the import checkpoint digest is the round-trip case (T1) */
          continue;
        }
        run_to(peer, prepared[i].tick + checkpoint_offset[c], steps[s], &got);
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
    if (b->extra != NULL) b->extra(i);
    matrix_trace("continuation", b, started);
  }
}

static void case_boundary_perturbation(void) {
  size_t i;

  for (i = 0u; i < BOUNDARY_COUNT; i++) {
    const boundary *b = &boundaries[i];
    const clock_t started = clock();
    t64_machine *peer;
    tuple got;
    uint8_t digest[T64_DIGEST_SIZE];

    if (!boundary_reference(i)) continue;
    peer = boundary_peer(i);
    if (peer == NULL) continue;
    /* the control arm in the same setup: before the edit the peer agrees with
     * the source at the import checkpoint (the first continuation checkpoint is
     * the unperturbed peer of the continuation case) */
    T64_CHECK(digest_of(peer, digest));
    T64_CHECK(memcmp(digest, prepared[i].ref_digest[0], sizeof digest) == 0);
    /* the perturbed arm: one field of the imported copy changes the digest at
     * the import checkpoint and at the first continuation checkpoint */
    b->perturb(peer);
    T64_CHECK(digest_of(peer, digest));
    if (memcmp(digest, prepared[i].ref_digest[0], sizeof digest) == 0) {
      fprintf(stderr, "boundary %s perturbation invisible at import\n", b->id);
    }
    T64_CHECK(memcmp(digest, prepared[i].ref_digest[0], sizeof digest) != 0);
    run_to(peer, prepared[i].tick + checkpoint_offset[1], 0u, &got);
    T64_CHECK(digest_of(peer, digest));
    if (memcmp(digest, prepared[i].ref_digest[1], sizeof digest) == 0) {
      fprintf(stderr, "boundary %s perturbation invisible at +310\n", b->id);
    }
    T64_CHECK(memcmp(digest, prepared[i].ref_digest[1], sizeof digest) != 0);
    t64_machine_destroy(peer);
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

/* True when an earlier row already owns the (device, kind) pair (the gate
 * preset checks each distinct pair once on its first row; every row runs under
 * T64_STATE_MATRIX_FULL). */
static bool pair_seen_before(size_t index, size_t pair) {
  size_t i;
  size_t p;

  for (i = 0u; i < index; i++) {
    for (p = 0u; p < 2u; p++) {
      if (boundaries[i].pair_device[p] == boundaries[index].pair_device[pair] &&
          boundaries[i].pair_kind[p] == boundaries[index].pair_kind[pair]) {
        return true;
      }
    }
  }
  return false;
}

static void case_boundary_event_rules(void) {
  size_t i;

  for (i = 0u; i < BOUNDARY_COUNT; i++) {
    const boundary *b = &boundaries[i];
    const clock_t started = clock();
    size_t pair;
    bool any = false;

    for (pair = 0u; pair < 2u; pair++) {
      if (b->pair_device[pair] != 0u &&
          (matrix_full() || !pair_seen_before(i, pair))) {
        any = true;
      }
    }
    if (!any) continue;
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
      if (!matrix_full() && pair_seen_before(i, pair)) continue;
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
        if (b->pair_device[pair] != T64_TRACE_DEVICE_INPUT) {
          refusal_expect(label, peer, skewed, skewed_length,
                         T64_STATE_REASON_DEVICE_TOKEN_MISMATCH);
        }
        (void)snprintf(label, sizeof label, "%s drop (%u,%u)", b->id,
                       (unsigned)b->pair_device[pair],
                       (unsigned)b->pair_kind[pair]);
        if (b->pair_device[pair] == T64_TRACE_DEVICE_INPUT) {
          /* A host-submitted input event owns no device continuation and its
           * token is never read (t64_input_event_handler uses the value), so
           * both mutants are accepted and bounded. */
          t64_state_import_report report;
          tuple after;

          T64_CHECK_INT(t64_machine_state_import(peer, skewed, skewed_length,
                                                 0u, &report),
                        T64_STATUS_OK);
          run_to(peer, peer->tick + 2000u, 0u, &after);
          T64_CHECK_INT(after.status, T64_STATUS_OK);
          t64_machine_destroy(peer);
          peer = create_admitted(b->profile);
          T64_CHECK(peer != NULL);
          if (peer != NULL) {
            T64_CHECK_INT(t64_machine_state_import(peer, removed,
                                                   removed_length, 0u, &report),
                          T64_STATUS_OK);
            run_to(peer, peer->tick + 2000u, 0u, &after);
            T64_CHECK_INT(after.status, T64_STATUS_OK);
          }
        } else if (b->pair_device[pair] == T64_DEVICE_SP &&
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


/* ---- case mid-tick-resume (D-36; plan 09-09.1 Task 3) --------------------- */

/* The all-devices workload W-ALL of core/tests/test_sched_matrix.c (copied: the
 * builders there are file-static), with the three inputs at E - 1, E and E + 1.
 * The CPU runs the authored program that starts the SI poll read, the PI and SP
 * DMA, the RSP program, the RDP full syncs, two AI frames and the VI registers
 * (v_sync 16, so no third field before the horizon) are all armed by host
 * writes before the first advance. Only production events are queued, so the
 * closed handler table of the importer accepts every state of the case. */
#define W_STORE_TICK UINT64_C(3008)
#define W_SI_H_POLL UINT64_C(16971)
#define W_E (W_STORE_TICK + W_SI_H_POLL)
#define W_H_ALL UINT64_C(60000)
#define W_VI_AI_TICK UINT64_C(1418)

static const uint32_t w_program[13] = {
    UINT32_C(0x3C08A480), UINT32_C(0x3C091FC0), UINT32_C(0x352907C0),
    UINT32_C(0x340B3900), UINT32_C(0x340A03E8), UINT32_C(0x254AFFFF),
    UINT32_C(0x1540FFFE), UINT32_C(0x00000000), UINT32_C(0xAD000018),
    UINT32_C(0xAD0B0000), UINT32_C(0xAD090004), UINT32_C(0x1000FFFF),
    UINT32_C(0x00000000)};

static void w_configure_vi(t64_machine *machine, uint32_t v_sync) {
  size_t pixel;

  for (pixel = 0u; pixel < 12u; pixel++) {
    T64_CHECK_INT(t64_machine_bus_write(machine,
                                        UINT32_C(0x3000) + (uint32_t)(pixel * 2u),
                                        2u, UINT32_C(0xf801)),
                  T64_STATUS_OK);
  }
  write_vi_register(machine, 0x04u, UINT32_C(0x3000));
  write_vi_register(machine, 0x08u, UINT32_C(4));
  write_vi_register(machine, 0x0cu, UINT32_C(2));
  write_vi_register(machine, 0x14u, UINT32_C(0));
  write_vi_register(machine, 0x18u, v_sync);
  write_vi_register(machine, 0x1cu, UINT32_C(2207));
  write_vi_register(machine, 0x24u, UINT32_C(4));
  write_vi_register(machine, 0x28u, UINT32_C(0x00020008));
  write_vi_register(machine, 0x30u, UINT32_C(0x00000400));
  write_vi_register(machine, 0x34u, UINT32_C(0x00000400));
  write_vi_register(machine, 0x00u, UINT32_C(2));
}

static t64_machine *build_w_all(void) {
  t64_machine *machine = create_admitted(SI_STEM_ID);
  size_t i;

  if (machine == NULL) return NULL;
  for (i = 0u; i < sizeof w_program / sizeof w_program[0]; i++) {
    put32(machine->ram + 0x1000u + 4u * i, w_program[i]);
  }
  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = t64_cpu_canonical_address(machine->cpu.pc);
  (void)memcpy(machine->ram + 0x3800u, poll_frame, sizeof poll_frame);
  bus_write32(machine, SI_DRAM_ADDR, UINT32_C(0x3800));
  bus_write32(machine, SI_WR64B, PIF_ADDRESS);
  /* PI cartridge DMA of 24 bytes to RDRAM 0x200 */
  bus_write32(machine, PI_BASE, UINT32_C(0x200));
  bus_write32(machine, PI_BASE + 4u, UINT32_C(0x10000000));
  bus_write32(machine, PI_BASE + 12u, UINT32_C(23));
  /* SP DMA of eight bytes, then an RSP program that adds and breaks */
  bus_write32(machine, SP_BASE, 0u);
  bus_write32(machine, SP_BASE + 4u, UINT32_C(0x600));
  bus_write32(machine, SP_BASE + 8u, 0u);
  bus_write32(machine, UINT32_C(0x04001000), UINT32_C(0x24020009));
  bus_write32(machine, UINT32_C(0x04001004), UINT32_C(0x0000000d));
  T64_CHECK_INT(t64_rsp_start(machine), T64_STATUS_OK);
  /* RDP: four full-sync commands at RDRAM 0x100 */
  for (i = 0u; i < 4u; i++) {
    (void)memset(machine->ram + 0x100u + i * 8u, 0, 8u);
    machine->ram[0x100u + i * 8u] = 0xe9u;
  }
  bus_write32(machine, DP_START_REG, UINT32_C(0x100));
  bus_write32(machine, DP_START_REG + 4u, UINT32_C(0x120));
  /* AI: two frames; VI: the tracer registers */
  ai_configure(machine);
  bus_write32(machine, AI_REG_CONTROL, 1u);
  bus_write32(machine, AI_BASE, UINT32_C(0x400));
  bus_write32(machine, AI_REG_LEN, 8u);
  w_configure_vi(machine, 16u);
  T64_CHECK_INT(t64_machine_queue_input(machine, W_E - 1u, 0u, UINT16_C(0x8000),
                                        1, -1),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_queue_input(machine, W_E, 0u, UINT16_C(0x4021), 127,
                                        -128),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_queue_input(machine, W_E + 1u, 0u, UINT16_C(0x1802),
                                        -1, 1),
                T64_STATUS_OK);
  return machine;
}

/* A stop is mid-tick when the queue front is still at the machine tick and an
 * event of another device is queued at that tick. */
static bool midtick_reached(const t64_machine *machine) {
  const t64_event *front;
  size_t i;

  if (machine->queue_count == 0u) return false;
  front = &machine->events[machine->queue_head];
  if (front->tick != machine->tick) return false;
  for (i = 1u; i < machine->queue_count; i++) {
    const t64_event *event =
        &machine->events[(machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];

    if (event->tick != machine->tick) break;
    if (event->device != front->device) return true;
  }
  return false;
}

static bool event_queued_at(const t64_machine *machine, uint32_t device,
                            uint32_t kind, uint64_t tick) {
  size_t i;

  for (i = 0u; i < machine->queue_count; i++) {
    const t64_event *event =
        &machine->events[(machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];

    if (event->device == device && event->kind == kind && event->tick == tick) {
      return true;
    }
  }
  return false;
}

typedef struct {
  size_t count;
  t64_event events[T64_EVENT_QUEUE_CAPACITY];
} queue_snapshot;

static void queue_snapshot_take(const t64_machine *machine, queue_snapshot *q) {
  size_t i;

  q->count = machine->queue_count;
  for (i = 0u; i < machine->queue_count; i++) {
    q->events[i] =
        machine->events[(machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];
  }
}

static bool queue_snapshot_equal(const t64_machine *machine,
                                 const queue_snapshot *q) {
  size_t i;

  if (machine->queue_count != q->count) return false;
  for (i = 0u; i < q->count; i++) {
    const t64_event *a =
        &machine->events[(machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];
    const t64_event *b = &q->events[i];

    if (a->tick != b->tick || a->sequence != b->sequence ||
        a->device != b->device || a->kind != b->kind || a->value != b->value ||
        a->token != b->token) {
      return false;
    }
  }
  return true;
}

/* The uninterrupted reference: digest, queue and result tuple at the horizon. */
typedef struct {
  bool ready;
  uint8_t digest[T64_DIGEST_SIZE];
  queue_snapshot queue;
  tuple final;
} w_reference;

static w_reference w_ref;

static void w_reference_build(void) {
  t64_machine *machine;

  if (w_ref.ready) return;
  w_ref.ready = true;
  machine = build_w_all();
  if (machine == NULL) {
    T64_CHECK(false);
    return;
  }
  /* reached: one VI field and one AI sample coincide on the recorded tick */
  T64_CHECK(advance_to(machine, W_VI_AI_TICK - 1u));
  T64_CHECK(event_queued_at(machine, T64_VI_DEVICE, T64_VI_EVENT_FIELD,
                            W_VI_AI_TICK));
  T64_CHECK(event_queued_at(machine, T64_AI_DEVICE, T64_AI_EVENT_SAMPLE,
                            W_VI_AI_TICK));
  run_to(machine, W_H_ALL, 0u, &w_ref.final);
  T64_CHECK_INT(w_ref.final.status, T64_STATUS_OK);
  T64_CHECK_INT(w_ref.final.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(w_ref.final.tick, W_H_ALL);
  (void)digest_of(machine, w_ref.digest);
  queue_snapshot_take(machine, &w_ref.queue);
  t64_machine_destroy(machine);
}

/* Continue a stopped source by repeated calls under its own event budget. */
static void run_budgeted(t64_machine *machine, uint64_t target, tuple *out) {
  t64_status status = T64_STATUS_OK;
  t64_stop_result stop = {0u, 0u};
  size_t calls;

  for (calls = 0u; calls < 400000u && machine->tick < target; calls++) {
    t64_stop_request request = {T64_STOP_GUEST_TICK, target};

    status = t64_machine_advance(machine, &request, &stop);
    if (status != T64_STATUS_OK) break;
    if (stop.reason != T64_STOP_REASON_RESOURCE_LIMIT &&
        stop.reason != T64_STOP_REASON_BOUND) {
      break;
    }
  }
  out->status = status;
  out->reason = stop.reason;
  out->stop_tick = stop.tick;
  out->tick = machine->tick;
  out->instructions = machine->instruction_count;
  out->frames = machine->frame_event_count;
}

typedef enum { STOP_BUDGET_1, STOP_BUDGET_2, STOP_INSTRUCTIONS, STOP_COINCIDENCE }
    stop_row;

static const char *const stop_row_name[] = {"s1 budget 1", "s2 budget 2",
                                            "s3 instruction count",
                                            "s4 VI and AI tick"};

/* Build W-ALL and stop it mid-tick; the literal (the counter target or the tick)
 * is returned. The machine keeps its edited budget for the source's own
 * continuation. */
static t64_machine *stop_w_all(stop_row which, uint32_t expected_reason,
                               uint64_t *literal) {
  t64_machine *machine = build_w_all();
  size_t i;

  *literal = 0u;
  if (machine == NULL) return NULL;
  if (which == STOP_BUDGET_1 || which == STOP_COINCIDENCE) {
    machine->profile.advance_event_budget = 1u;
  }
  if (which == STOP_BUDGET_2) machine->profile.advance_event_budget = 2u;
  for (i = 0u; i < 30000u; i++) {
    t64_stop_result stop = {0u, 0u};
    t64_status status;

    if (which == STOP_INSTRUCTIONS) {
      t64_stop_request request = {T64_STOP_INSTRUCTION_COUNT,
                                  machine->instruction_count + 1u};

      if (request.value > 4096u) break;
      status = t64_machine_advance(machine, &request, &stop);
    } else {
      t64_stop_request request = {T64_STOP_GUEST_TICK, W_H_ALL};

      status = t64_machine_advance(machine, &request, &stop);
    }
    T64_CHECK_INT(status, T64_STATUS_OK);
    if (status != T64_STATUS_OK || stop.reason != expected_reason) break;
    if (which == STOP_COINCIDENCE) {
      const bool vi = event_queued_at(machine, T64_VI_DEVICE, T64_VI_EVENT_FIELD,
                                      W_VI_AI_TICK);
      const bool ai = event_queued_at(machine, T64_AI_DEVICE, T64_AI_EVENT_SAMPLE,
                                      W_VI_AI_TICK);

      if (machine->tick == W_VI_AI_TICK && midtick_reached(machine) &&
          vi != ai) {
        *literal = machine->tick;
        return machine;
      }
      if (machine->tick > W_VI_AI_TICK) break;
    } else if (midtick_reached(machine)) {
      *literal = which == STOP_INSTRUCTIONS ? machine->instruction_count
                                          : machine->tick;
      return machine;
    }
  }
  fprintf(stderr, "mid-tick stop %s not reached\n", stop_row_name[which]);
  T64_CHECK(false);
  t64_machine_destroy(machine);
  return NULL;
}

static void midtick_row(stop_row which, uint32_t expected_reason) {
  t64_machine *source;
  t64_machine *peer;
  t64_state_import_report report;
  uint64_t literal = 0u;
  uint8_t *blob;
  size_t length = 0u;
  uint8_t digest[T64_DIGEST_SIZE];
  uint8_t source_digest[T64_DIGEST_SIZE];
  uint8_t *again;
  size_t again_length = 0u;
  tuple got;
  uint64_t steps[3];
  size_t step_count = 0u;
  size_t s;
  uint64_t stop_tick;
  const clock_t started = clock();

  source = stop_w_all(which, expected_reason, &literal);
  if (source == NULL) return;
  stop_tick = source->tick;
  if (getenv("T64_STATE_MATRIX_TRACE") != NULL) {
    fprintf(stderr, "mid-tick %s: stop tick %llu literal %llu queue %zu\n",
            stop_row_name[which], (unsigned long long)stop_tick,
            (unsigned long long)literal, source->queue_count);
  }
  /* reached before export: the stop reason is the budget or counter reason
   * (checked inside the search), the queue front is at the machine tick and an
   * event of another device is queued at that tick */
  T64_CHECK(midtick_reached(source));
  blob = export_blob(source, &length);
  if (blob == NULL) {
    t64_machine_destroy(source);
    return;
  }
  (void)digest_of(source, source_digest);
  /* T1 and T2 */
  peer = create_admitted(SI_STEM_ID);
  if (peer != NULL) {
    T64_CHECK_INT(t64_machine_state_import(peer, blob, length, 0u, &report),
                  T64_STATUS_OK);
    T64_CHECK(digest_of(peer, digest));
    T64_CHECK(memcmp(digest, source_digest, sizeof digest) == 0);
    T64_CHECK(memcmp(digest, blob + D_DECLARED, sizeof digest) == 0);
    again = export_blob(peer, &again_length);
    T64_CHECK(again != NULL && again_length == length &&
              memcmp(again, blob, length) == 0);
    free(again);
    /* T3: the source continues under its own budget, the peer in slices; both
     * end at the uninterrupted reference */
    run_budgeted(source, W_H_ALL, &got);
    T64_CHECK(tuples_equal(&got, &w_ref.final));
    T64_CHECK(digest_of(source, digest));
    T64_CHECK(memcmp(digest, w_ref.digest, sizeof digest) == 0);
    T64_CHECK(queue_snapshot_equal(source, &w_ref.queue));
    run_to(peer, W_H_ALL, 0u, &got);
    T64_CHECK(tuples_equal(&got, &w_ref.final));
    T64_CHECK(digest_of(peer, digest));
    T64_CHECK(memcmp(digest, w_ref.digest, sizeof digest) == 0);
    T64_CHECK(queue_snapshot_equal(peer, &w_ref.queue));
    t64_machine_destroy(peer);
  }
  /* partitions of the first 310 ticks, then the horizon */
  steps[step_count++] = 7u;
  if (matrix_full()) {
    steps[step_count++] = 1u;
    steps[step_count++] = 31u;
  }
  for (s = 0u; s < step_count; s++) {
    peer = create_admitted(SI_STEM_ID);
    if (peer == NULL) continue;
    T64_CHECK_INT(t64_machine_state_import(peer, blob, length, 0u, &report),
                  T64_STATUS_OK);
    run_to(peer, stop_tick + 310u, steps[s], &got);
    run_to(peer, W_H_ALL, 0u, &got);
    T64_CHECK(tuples_equal(&got, &w_ref.final));
    T64_CHECK(digest_of(peer, digest));
    T64_CHECK(memcmp(digest, w_ref.digest, sizeof digest) == 0);
    t64_machine_destroy(peer);
  }
  /* perturbation: the unperturbed peers above match in the same setup; one
   * field of the imported queue state changes the digest at the horizon (the
   * sequence of the front event is erased at its dispatch, so the counter that
   * every later reservation uses is edited) */
  peer = create_admitted(SI_STEM_ID);
  if (peer != NULL) {
    T64_CHECK_INT(t64_machine_state_import(peer, blob, length, 0u, &report),
                  T64_STATUS_OK);
    peer->next_sequence += 1u;
    run_to(peer, W_H_ALL, 0u, &got);
    T64_CHECK(digest_of(peer, digest));
    T64_CHECK(memcmp(digest, w_ref.digest, sizeof digest) != 0);
    t64_machine_destroy(peer);
  }
  free(blob);
  t64_machine_destroy(source);
  if (getenv("T64_STATE_MATRIX_TRACE") != NULL) {
    fprintf(stderr, "matrix-trace mid-tick %s %.2fs\n", stop_row_name[which],
            (double)(clock() - started) / (double)CLOCKS_PER_SEC);
  }
}

/* The 09-06 part b state: five synthetic instruction events and three synthetic
 * frame events queued at one tick. It exports, and the closed handler table
 * refuses it on import. */
static t64_machine *build_synthetic_midtick(void) {
  t64_machine *machine = create_admitted(SI_STEM_ID);
  uint64_t cancelled_tick = UINT64_MAX;
  static const uint32_t first[] = {T64_TEST_EVENT_INSTRUCTION,
                                   T64_TEST_EVENT_FRAME,
                                   T64_TEST_EVENT_INSTRUCTION};
  static const uint32_t second[] = {
      T64_TEST_EVENT_INSTRUCTION, T64_TEST_EVENT_FRAME,
      T64_TEST_EVENT_INSTRUCTION, T64_TEST_EVENT_INSTRUCTION,
      T64_TEST_EVENT_FRAME, T64_TEST_EVENT_NOOP};
  size_t i;

  if (machine == NULL) return NULL;
  T64_CHECK_INT(t64_schedule_cancel(machine, T64_DEVICE_CPU, T64_CPU_EVENT_STEP,
                                    0u, &cancelled_tick),
                T64_STATUS_OK);
  bus_write32(machine, SI_DRAM_ADDR, RECEIVE_DRAM);
  bus_write32(machine, SI_RD64B, PIF_ADDRESS);
  T64_CHECK_INT(machine->si.complete_tick, 4096u);
  for (i = 0u; i < 3u; i++) {
    T64_CHECK_INT(t64_test_schedule_synthetic(machine, 4096u, first[i], 1u),
                  T64_STATUS_OK);
  }
  T64_CHECK(advance_to(machine, 4096u - 13u));
  bus_write32(machine, PI_BASE, UINT32_C(0x200));
  bus_write32(machine, PI_BASE + 4u, UINT32_C(0x10000000));
  bus_write32(machine, PI_BASE + 12u, UINT32_C(23));
  for (i = 0u; i < 6u; i++) {
    T64_CHECK_INT(t64_test_schedule_synthetic(machine, 4096u, second[i], 1u),
                  T64_STATUS_OK);
  }
  return machine;
}

static void midtick_synthetic_control(const uint8_t *baseline, size_t baseline_length) {
  t64_machine *synthetic = build_synthetic_midtick();
  t64_machine *peer;
  t64_state_import_report report;
  uint8_t *blob;
  size_t length = 0u;
  snapshot before;

  if (synthetic == NULL) return;
  T64_CHECK_INT(synthetic->tick, 4096u - 13u);
  /* the state exports with OK (export does not apply the handler table) */
  blob = export_blob(synthetic, &length);
  t64_machine_destroy(synthetic);
  if (blob == NULL) return;
  /* an accepted baseline in the same setup, then the refusal */
  peer = create_admitted(SI_STEM_ID);
  if (peer != NULL) {
    T64_CHECK_INT(t64_machine_state_import(peer, baseline, baseline_length, 0u,
                                           &report),
                  T64_STATUS_OK);
    t64_machine_destroy(peer);
  }
  peer = create_admitted(SI_STEM_ID);
  if (peer != NULL) {
    const size_t allocs = allocate_calls;
    const size_t releases = release_calls;

    snapshot_take(peer, &before);
    T64_CHECK_INT(t64_machine_state_import(peer, blob, length, 0u, &report),
                  T64_STATUS_STATE_INVARIANT);
    T64_CHECK_INT(report.reason, T64_STATE_REASON_QUEUE_KIND_UNKNOWN);
    T64_CHECK_INT(report.section_id, T64_STATE_SECTION_SCHEDULER);
    T64_CHECK_INT(report.rule, T64_STATE_RULE_ENUM_CLOSED);
    T64_CHECK(snapshot_matches(peer, &before));
    T64_CHECK(t64_state_summary_is_clear(peer));
    T64_CHECK_INT(allocate_calls, allocs);
    T64_CHECK_INT(release_calls, releases);
    snapshot_free(&before);
    t64_machine_destroy(peer);
  }
  free(blob);
}

static void case_mid_tick_resume(void) {
  t64_machine *source;
  uint64_t literal = 0u;
  uint8_t *baseline;
  size_t baseline_length = 0u;

  w_reference_build();
  midtick_row(STOP_BUDGET_1, T64_STOP_REASON_RESOURCE_LIMIT);
  midtick_row(STOP_BUDGET_2, T64_STOP_REASON_RESOURCE_LIMIT);
  midtick_row(STOP_COINCIDENCE, T64_STOP_REASON_RESOURCE_LIMIT);
  if (matrix_full()) {
    midtick_row(STOP_INSTRUCTIONS, T64_STOP_REASON_INSTRUCTION_COUNT);
  }
  /* the accepted baseline of the synthetic control is the first stop row */
  source = stop_w_all(STOP_BUDGET_1, T64_STOP_REASON_RESOURCE_LIMIT, &literal);
  if (source == NULL) return;
  baseline = export_blob(source, &baseline_length);
  t64_machine_destroy(source);
  if (baseline != NULL) midtick_synthetic_control(baseline, baseline_length);
  free(baseline);
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

/* ======================================================================== */
/* Plan 09-09.1 Task 5: the structure-aware mutants, the sweep, the          */
/* sanitizer-scaled campaign and the liveness list (D-30, D-41)              */
/* ======================================================================== */

/* ---- the independently authored inventory (docs/spec/state-mutants.txt) --- */

typedef struct {
  const char *name;
  uint32_t value;
} nv;

#define NV_STATUS(x) {#x, (uint32_t)T64_STATUS_##x}
#define NV_REASON(x) {#x, (uint32_t)T64_STATE_REASON_##x}
#define NV_RULE(x) {#x, (uint32_t)T64_STATE_RULE_##x}

static const nv status_names[] = {
    NV_STATUS(STATE_FORMAT),   NV_STATUS(STATE_INTEGRITY),
    NV_STATUS(STATE_VERSION),  NV_STATUS(STATE_PROFILE),
    NV_STATUS(STATE_CONTENT),  NV_STATUS(STATE_BUILD),
    NV_STATUS(STATE_RANGE),    NV_STATUS(STATE_INVARIANT)};

static const nv reason_names[] = {
    NV_REASON(STATE_VERSION),        NV_REASON(PROFILE_DIGEST),
    NV_REASON(CONTENT_DIGEST),       NV_REASON(CONTENT_LENGTH),
    NV_REASON(BUILD_ID),             NV_REASON(TOTAL_LENGTH),
    NV_REASON(SECTION_COUNT),        NV_REASON(HEADER_FLAGS),
    NV_REASON(HEADER_RESERVED),      NV_REASON(SECTION_ORDER),
    NV_REASON(SECTION_ID_UNKNOWN),   NV_REASON(SECTION_BOUNDS),
    NV_REASON(SECTION_OVERLAP),      NV_REASON(SECTION_LENGTH_EXPECTED),
    NV_REASON(SECTION_SHA256),       NV_REASON(FIELD_RANGE),
    NV_REASON(FIELD_DERIVED),        NV_REASON(FIELD_PADDING),
    NV_REASON(FIELD_RESERVED),       NV_REASON(OBSERVER_RANGE),
    NV_REASON(QUEUE_COUNT),          NV_REASON(QUEUE_ORDER),
    NV_REASON(QUEUE_SEQUENCE),
    NV_REASON(QUEUE_TICK),           NV_REASON(QUEUE_KIND_UNKNOWN),
    NV_REASON(DEVICE_EVENT_MISSING), NV_REASON(DEVICE_EVENT_EXTRA),
    NV_REASON(DEVICE_TOKEN_MISMATCH), NV_REASON(DECLARED_DIGEST)};

static const nv rule_names[] = {
    NV_RULE(NONE),          NV_RULE(COUNT_CAP),    NV_RULE(INDEX_LT),
    NV_RULE(ENUM_CLOSED),   NV_RULE(BOOL_01),      NV_RULE(ALIGN),
    NV_RULE(ADDR_RANGE),    NV_RULE(TICK_GE),      NV_RULE(TICK_LE),
    NV_RULE(SEQ_LT),        NV_RULE(SHIFT_LT),     NV_RULE(LEN_LE),
    NV_RULE(PROFILE_DERIVED), NV_RULE(DERIVED_EQ), NV_RULE(PAD_ZERO),
    NV_RULE(RESERVED_ZERO)};

static bool nv_lookup(const nv *table, size_t count, const char *name,
                      uint32_t *out) {
  size_t i;

  for (i = 0u; i < count; i++) {
    if (strcmp(table[i].name, name) == 0) {
      *out = table[i].value;
      return true;
    }
  }
  return false;
}

#define NV_FIND(table, name, out) nv_lookup(table, sizeof table / sizeof table[0], name, out)

typedef struct {
  char name[56];
  char cls[12];
  uint32_t status;
  uint32_t reason;
  uint32_t section;
  uint32_t rule;
} inventory_row;

#define INVENTORY_MAX 96u

static size_t inventory_rows;
static inventory_row inventory[INVENTORY_MAX];
static bool inventory_ok;

static bool inventory_load(void) {
  const char *path = getenv("T64_STATE_MUTANT_INVENTORY");
  FILE *file;
  char line[256];
  char cls[12] = "";

  if (inventory_rows != 0u) return inventory_ok;
  if (path == NULL || path[0] == '\0') return false;
  file = fopen(path, "r");
  if (file == NULL) return false;
  inventory_ok = true;
  while (fgets(line, sizeof line, file) != NULL) {
    size_t length = strlen(line);
    char *field[5];
    size_t n = 0u;
    char *cursor = line;
    inventory_row *item;

    while (length > 0u && (line[length - 1u] == '\n' || line[length - 1u] == '\r')) {
      line[--length] = '\0';
    }
    if (length == 0u) continue;
    if (line[0] == '#') {
      static const char *const classes[] = {"header", "table",   "fields",
                                            "queue",  "devices", "digest"};
      size_t k;

      for (k = 0u; k < sizeof classes / sizeof classes[0]; k++) {
        const size_t cl = strlen(classes[k]);

        if (strncmp(line, "# ", 2u) == 0 &&
            strncmp(line + 2, classes[k], cl) == 0 &&
            (line[2 + cl] == '\0' || line[2 + cl] == ' ')) {
          (void)snprintf(cls, sizeof cls, "%s", classes[k]);
        }
      }
      continue;
    }
    while (n < 5u) {
      char *sep = strstr(cursor, " :: ");

      field[n++] = cursor;
      if (sep == NULL) break;
      *sep = '\0';
      cursor = sep + 4;
    }
    if (n != 5u || inventory_rows >= INVENTORY_MAX) {
      inventory_ok = false;
      break;
    }
    item = &inventory[inventory_rows++];
    (void)snprintf(item->name, sizeof item->name, "%s", field[0]);
    (void)snprintf(item->cls, sizeof item->cls, "%s", cls);
    item->section = (uint32_t)strtoul(field[3], NULL, 10);
    if (!NV_FIND(status_names, field[1], &item->status) ||
        !NV_FIND(reason_names, field[2], &item->reason) ||
        !NV_FIND(rule_names, field[4], &item->rule)) {
      fprintf(stderr, "inventory row %s: unknown name\n", item->name);
      inventory_ok = false;
    }
  }
  (void)fclose(file);
  return inventory_ok;
}

/* ---- the C mutant table --------------------------------------------------- */

typedef struct {
  const char *cls;
  row r;
} mutant;

static uint8_t *mut_total_plus_one(target_id target, size_t *length_out) {
  uint8_t *blob = dup_target(target, length_out);

  if (blob == NULL) return NULL;
  put64(blob + D_TOTAL, (uint64_t)*length_out + 1u);
  blob_resign(blob, *length_out, RS_TRAILER);
  return blob;
}

static size_t boundary_index(const char *id);

/* The token of one queue event of a boundary blob, skewed by one: the legitimate
 * boundary blob is imported first (an accepted baseline in the same setup). */
static bool boundary_baselined[BOUNDARY_COUNT];

static uint8_t *mut_boundary_token(const char *id, uint32_t device,
                                   uint32_t kind, size_t *length_out) {
  const size_t index = boundary_index(id);

  if (index == BOUNDARY_COUNT || !boundary_prepare(index)) return NULL;
  if (!boundary_baselined[index]) {
    t64_machine *baseline;

    boundary_baselined[index] = true;
    baseline = boundary_peer(index);
    if (baseline != NULL) t64_machine_destroy(baseline);
  }
  return blob_edit_pair(prepared[index].blob, prepared[index].length, device,
                        kind, false, length_out);
}

static uint8_t *mut_rsp_token(target_id target, size_t *length_out) {
  (void)target;
  return mut_boundary_token("B04", T64_DEVICE_SP, T64_SP_EVENT_RSP_ISSUE,
                            length_out);
}

static uint8_t *mut_vi_token(target_id target, size_t *length_out) {
  (void)target;
  return mut_boundary_token("B06", T64_VI_DEVICE, T64_VI_EVENT_FIELD, length_out);
}

static uint8_t *mut_ai_token(target_id target, size_t *length_out) {
  (void)target;
  return mut_boundary_token("B08", T64_AI_DEVICE, T64_AI_EVENT_SAMPLE,
                            length_out);
}

/* A render in progress (an 8 by 8 fill through the DP registers, stopped after
 * the first row): the accepted baseline of the render-extent mutant. */
static t64_machine *build_render(void) {
  static const uint64_t words[] = {
      UINT64_C(0x3f10000700005000), /* RGBA16 color image, width 8 */
      UINT64_C(0x2d00000000020020), /* scissor [0,8) x [0,8) */
      UINT64_C(0x2f30000000000000), /* fill cycle type */
      UINT64_C(0x37000000f80107c1), /* fill color */
      UINT64_C(0x3602002000000000)  /* fill rectangle 8 x 8 */};
  t64_machine *machine = build_quiet();
  size_t i;

  if (machine == NULL) return NULL;
  for (i = 0u; i < sizeof words / sizeof words[0]; i++) {
    put_be64(machine->ram + B05_COMMAND_ADDRESS + 8u * i, words[i]);
  }
  bus_write32(machine, DP_START_REG, B05_COMMAND_ADDRESS);
  bus_write32(machine, DP_START_REG + 4u,
              B05_COMMAND_ADDRESS + (uint32_t)sizeof words);
  for (i = 0u; i < 400u && !(machine->rdp.render_active &&
                             machine->rdp.render_y > machine->rdp.render_y_begin);
       i++) {
    T64_CHECK(advance_to(machine, machine->tick + 1u));
  }
  return machine;
}

static uint8_t *render_blob;
static size_t render_length;
static size_t render_field_offset; /* of render_x_begin inside section 8 */

static bool render_prepare(void) {
  t64_machine *machine;
  size_t body = 0u;
  size_t base = 0u;
  size_t i;
  size_t found = 0u;

  if (render_blob != NULL) return true;
  machine = build_render();
  if (machine == NULL) return false;
  T64_CHECK(machine->rdp.render_active);
  T64_CHECK(machine->rdp.render_x_end == 7u && machine->rdp.render_y_end == 8u);
  render_blob = export_blob(machine, &render_length);
  if (render_blob != NULL && find_section(render_blob, T64_STATE_SECTION_RDP_VI,
                                          &base, &body)) {
    /* the five consecutive render fields are located by value and the match
     * must be unique, so a layout error cannot patch the wrong bytes */
    for (i = 0u; i + 20u <= body; i += 4u) {
      if (rd32(render_blob + base + i) == machine->rdp.render_x_begin &&
          rd32(render_blob + base + i + 4u) == machine->rdp.render_x_end &&
          rd32(render_blob + base + i + 8u) == machine->rdp.render_y_begin &&
          rd32(render_blob + base + i + 12u) == machine->rdp.render_y_end &&
          rd32(render_blob + base + i + 16u) == machine->rdp.render_y) {
        render_field_offset = i;
        found++;
      }
    }
  }
  T64_CHECK_INT(found, 1u);
  if (found != 1u) {
    free(render_blob);
    render_blob = NULL;
  }
  t64_machine_destroy(machine);
  return render_blob != NULL;
}

static uint8_t *mut_render_extent(target_id target, size_t *length_out) {
  uint8_t *blob;
  size_t base = 0u;
  size_t body = 0u;

  (void)target;
  if (!render_prepare()) return NULL;
  blob = (uint8_t *)malloc(render_length);
  if (blob == NULL) return NULL;
  (void)memcpy(blob, render_blob, render_length);
  *length_out = render_length;
  (void)find_section(blob, T64_STATE_SECTION_RDP_VI, &base, &body);
  put32(blob + base + render_field_offset + 4u, UINT32_C(0x00ffffff));
  blob_resign_entry(blob, render_length, T64_STATE_SECTION_RDP_VI);
  blob_resign(blob, render_length, RS_DECLARED | RS_TRAILER);
  return blob;
}

/* A triangle render in progress (09-10 FX-01): a flat or a shaded triangle
 * submitted through the DP registers after the combine mode (the family needs
 * it for every triangle), stopped after its first row. The source builders of
 * the mutants below; the accepted baseline of each is the unmutated blob. */
typedef enum { TRI_FLAT = 0, TRI_SHADE = 1, TRI_COUNT = 2 } tri_kind;

static t64_machine *build_tri_render(tri_kind kind) {
  static const uint64_t flat[] = {
      UINT64_C(0x3f10000700005000), /* RGBA16 color image, width 8 */
      UINT64_C(0x2d00000000020020), /* scissor [0,8) x [0,8) */
      UINT64_C(0x2f00000000000000), /* one-cycle mode */
      UINT64_C(0x3a000000c8c8c8ff), /* primitive color */
      UINT64_C(0x3b000000505050ff), /* environment color */
      UINT64_C(0xfc55566c8877ffff), /* combine mode */
      UINT64_C(0x08000014000c0004), /* edge: rows 1 to 4 */
      UINT64_C(0x00010000fffe8000), UINT64_C(0x0001000000000000),
      UINT64_C(0x0004000000018000)};
  static const uint64_t shade[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
      UINT64_C(0x2f00000000000000), UINT64_C(0x3a000000c8c8c8ff),
      UINT64_C(0x3b000000505050ff), UINT64_C(0xfc55566c8877ffff),
      UINT64_C(0x0c000014000c0004), /* shade triangle, 12 words */
      UINT64_C(0x00010000fffe8000), UINT64_C(0x0001000000008000),
      UINT64_C(0x0004000000018000), UINT64_C(0x002000400060008f),
      UINT64_C(0x0008fffb000cfff7), UINT64_C(0x40008000c0004000),
      UINT64_C(0x8000c000c0008000), UINT64_C(0x00100008fffb0002),
      UINT64_C(0x0004000c0008fff9), UINT64_C(0x4000c00080004000),
      UINT64_C(0xc000400080004000)};
  const uint64_t *words = kind == TRI_SHADE ? shade : flat;
  const size_t count = kind == TRI_SHADE ? sizeof shade / sizeof shade[0]
                                         : sizeof flat / sizeof flat[0];
  t64_machine *machine = build_quiet();
  size_t i;

  if (machine == NULL) return NULL;
  for (i = 0u; i < count; i++) {
    put_be64(machine->ram + B05_COMMAND_ADDRESS + 8u * i, words[i]);
  }
  bus_write32(machine, DP_START_REG, B05_COMMAND_ADDRESS);
  bus_write32(machine, DP_START_REG + 4u,
              B05_COMMAND_ADDRESS + (uint32_t)(8u * count));
  for (i = 0u; i < 600u && !(machine->rdp.render_active &&
                             machine->rdp.render_triangle &&
                             machine->rdp.render_y > machine->rdp.render_y_begin);
       i++) {
    T64_CHECK(advance_to(machine, machine->tick + 1u));
  }
  return machine;
}

typedef struct {
  uint8_t *blob;
  size_t length;
  size_t field; /* render_x_begin inside section 8; x_end, y_begin, y_end and
                   render_y follow */
  size_t words; /* pending_triangle_words[0] inside section 8 */
  size_t count; /* pending_triangle_word_count */
  size_t render_flag; /* render_triangle; shade_triangle follows */
  size_t active_flag; /* render_active */
} tri_blob_info;

static tri_blob_info tri_blobs[TRI_COUNT];

/* The fields are located by value and by the layout offsets the importer
 * reads them at, and every located value is asserted, so a layout error cannot
 * patch the wrong bytes. */
static bool tri_prepare(tri_kind kind) {
  tri_blob_info *info = &tri_blobs[kind];
  t64_machine *machine;
  size_t body = 0u;
  size_t base = 0u;
  size_t i;
  size_t found = 0u;

  if (info->blob != NULL) return true;
  machine = build_tri_render(kind);
  if (machine == NULL) return false;
  T64_CHECK(machine->rdp.render_active && machine->rdp.render_triangle);
  T64_CHECK_INT(machine->rdp.shade_triangle, kind == TRI_SHADE ? 1u : 0u);
  T64_CHECK_INT(machine->rdp.pending_triangle_word_count,
                kind == TRI_SHADE ? 12u : 4u);
  T64_CHECK(machine->rdp.combine_mode_valid);
  info->blob = export_blob(machine, &info->length);
  if (info->blob != NULL &&
      find_section(info->blob, T64_STATE_SECTION_RDP_VI, &base, &body)) {
    for (i = 0u; i + 20u <= body; i += 4u) {
      if (rd32(info->blob + base + i) == machine->rdp.render_x_begin &&
          rd32(info->blob + base + i + 4u) == machine->rdp.render_x_end &&
          rd32(info->blob + base + i + 8u) == machine->rdp.render_y_begin &&
          rd32(info->blob + base + i + 12u) == machine->rdp.render_y_end &&
          rd32(info->blob + base + i + 16u) == machine->rdp.render_y) {
        info->field = i;
        found++;
      }
    }
    info->count = S8_TRIANGLE_WORD_COUNT;
    info->words = S8_TRIANGLE_WORD_COUNT + 4u;
    info->render_flag = info->words +
                        8u * machine->rdp.pending_triangle_word_count + 12u;
    info->active_flag = info->field + 36u;
    T64_CHECK_INT(rd32(info->blob + base + info->count),
                  machine->rdp.pending_triangle_word_count);
    T64_CHECK(rd64(info->blob + base + info->words) ==
              machine->rdp.pending_triangle_words[0]);
    T64_CHECK_INT(rd32(info->blob + base + info->render_flag), 1u);
    T64_CHECK_INT(rd32(info->blob + base + info->render_flag + 4u),
                  kind == TRI_SHADE ? 1u : 0u);
    T64_CHECK_INT(rd32(info->blob + base + info->active_flag), 1u);
  }
  T64_CHECK_INT(found, 1u);
  if (found != 1u) {
    free(info->blob);
    info->blob = NULL;
  }
  t64_machine_destroy(machine);
  return info->blob != NULL;
}

/* One edit of a triangle blob: a 32-bit or a 64-bit write at an offset of
 * section 8, the section entry, the declared digest and the trailer re-signed. */
static uint8_t *tri_edit(tri_kind kind, size_t offset, unsigned width,
                         uint64_t value, size_t *length_out) {
  tri_blob_info *info = &tri_blobs[kind];
  uint8_t *blob;
  size_t base = 0u;
  size_t body = 0u;

  if (!tri_prepare(kind)) return NULL;
  blob = (uint8_t *)malloc(info->length);
  if (blob == NULL) return NULL;
  (void)memcpy(blob, info->blob, info->length);
  *length_out = info->length;
  (void)find_section(blob, T64_STATE_SECTION_RDP_VI, &base, &body);
  if (width == 4u) put32(blob + base + offset, (uint32_t)value);
  if (width == 8u) put64(blob + base + offset, value);
  blob_resign_entry(blob, info->length, T64_STATE_SECTION_RDP_VI);
  blob_resign(blob, info->length, RS_DECLARED | RS_TRAILER);
  return blob;
}

/* rows at or above the 1,024 rows of the triangle coverage array */
static uint8_t *mut_tri_rows(target_id target, size_t *length_out) {
  uint8_t *blob;
  size_t base = 0u;
  size_t body = 0u;

  (void)target;
  blob = tri_edit(TRI_FLAT, tri_blobs[TRI_FLAT].field + 12u, 4u, 1500u,
                  length_out);
  if (blob == NULL) return NULL;
  (void)find_section(blob, T64_STATE_SECTION_RDP_VI, &base, &body);
  put32(blob + base + tri_blobs[TRI_FLAT].field + 16u, 1500u);
  blob_resign_entry(blob, *length_out, T64_STATE_SECTION_RDP_VI);
  blob_resign(blob, *length_out, RS_DECLARED | RS_TRAILER);
  return blob;
}

/* a first row that differs from the one the packet and the scissor give */
static uint8_t *mut_tri_begin(target_id target, size_t *length_out) {
  (void)target;
  if (!tri_prepare(TRI_FLAT)) return NULL;
  return tri_edit(TRI_FLAT, tri_blobs[TRI_FLAT].field + 8u, 4u, 0u, length_out);
}

/* the row being rendered past the last row of the packet */
static uint8_t *mut_tri_row_past_end(target_id target, size_t *length_out) {
  (void)target;
  if (!tri_prepare(TRI_FLAT)) return NULL;
  return tri_edit(TRI_FLAT, tri_blobs[TRI_FLAT].field + 16u, 4u, 700u,
                  length_out);
}

/* a shade slope whose products the rasterizer could not bound */
static uint8_t *mut_tri_shade_overflow(target_id target, size_t *length_out) {
  uint8_t *blob;
  size_t base = 0u;
  size_t body = 0u;

  (void)target;
  if (!tri_prepare(TRI_SHADE)) return NULL;
  blob = tri_edit(TRI_SHADE, tri_blobs[TRI_SHADE].words + 8u * 5u, 8u,
                  UINT64_C(0x7fff7fff7fff7fff), length_out);
  if (blob == NULL) return NULL;
  (void)find_section(blob, T64_STATE_SECTION_RDP_VI, &base, &body);
  put64(blob + base + tri_blobs[TRI_SHADE].words + 8u * 7u,
        UINT64_C(0xffffffffffffffff));
  blob_resign_entry(blob, *length_out, T64_STATE_SECTION_RDP_VI);
  blob_resign(blob, *length_out, RS_DECLARED | RS_TRAILER);
  return blob;
}

/* a packet word count that is not the count of the opcode: the count is 4 and
 * the body carries four words (a consistent layout, so the reader reaches the
 * rule and not a later misread field) */
static uint8_t *mut_tri_word_count(target_id target, size_t *length_out) {
  const tri_blob_info *info = &tri_blobs[TRI_SHADE];
  tblob t;
  size_t i;
  size_t cut;
  uint8_t *body;

  (void)target;
  if (!tri_prepare(TRI_SHADE)) return NULL;
  if (!tblob_decode(info->blob, info->length, &t)) return NULL;
  for (i = 0u; i < t.count && t.section[i].id != T64_STATE_SECTION_RDP_VI; i++) {
  }
  if (i == t.count) {
    tblob_free(&t);
    return NULL;
  }
  body = t.section[i].body;
  cut = info->words + 8u * 4u;
  put32(body + info->count, 4u);
  (void)memmove(body + cut, body + info->words + 8u * 12u,
                t.section[i].length - (info->words + 8u * 12u));
  t.section[i].length -= 8u * 8u;
  return encode_edited(&t, length_out);
}

/* an opcode the profile refuses at acceptance (a depth triangle) */
static uint8_t *mut_tri_opcode(target_id target, size_t *length_out) {
  tri_blob_info *info = &tri_blobs[TRI_SHADE];
  uint64_t word0;
  size_t base = 0u;
  size_t body = 0u;

  (void)target;
  if (!tri_prepare(TRI_SHADE)) return NULL;
  (void)find_section(info->blob, T64_STATE_SECTION_RDP_VI, &base, &body);
  word0 = rd64(info->blob + base + info->words);
  word0 = (word0 & ~(UINT64_C(0x3f) << 56)) | (UINT64_C(0x0d) << 56);
  return tri_edit(TRI_SHADE, info->words, 8u, word0, length_out);
}

/* the shade flag that does not follow the opcode */
static uint8_t *mut_tri_shade_flag(target_id target, size_t *length_out) {
  (void)target;
  if (!tri_prepare(TRI_SHADE)) return NULL;
  return tri_edit(TRI_SHADE, tri_blobs[TRI_SHADE].render_flag + 4u, 4u, 0u,
                  length_out);
}

/* a triangle render flag without a render in progress */
static uint8_t *mut_tri_flag_idle(target_id target, size_t *length_out) {
  (void)target;
  if (!tri_prepare(TRI_FLAT)) return NULL;
  return tri_edit(TRI_FLAT, tri_blobs[TRI_FLAT].active_flag, 4u, 0u,
                  length_out);
}

/* ---- rdp-state-carry (plan 09-10, STATE-CONTENT-F1) ----------------------- */

static bool rdp_carried_equal(const t64_machine *a, const t64_machine *b) {
  const t64_rdp_state *x = &a->rdp;
  const t64_rdp_state *y = &b->rdp;
  const size_t table = T64_RDP_MEMORY_COVERAGE_BYTES(a->profile.rdram_bytes);

  return x->primitive_lod_fraction == y->primitive_lod_fraction &&
         x->environment_color == y->environment_color &&
         x->fog_color == y->fog_color && x->blend_color == y->blend_color &&
         x->combine_mode == y->combine_mode &&
         x->combine_mode_valid == y->combine_mode_valid &&
         x->environment_color_valid == y->environment_color_valid &&
         x->fog_color_valid == y->fog_color_valid &&
         x->blend_color_valid == y->blend_color_valid &&
         x->key_center_valid == y->key_center_valid &&
         x->key_scale_valid == y->key_scale_valid &&
         x->key_width_valid == y->key_width_valid &&
         memcmp(x->key_center, y->key_center, sizeof x->key_center) == 0 &&
         memcmp(x->key_scale, y->key_scale, sizeof x->key_scale) == 0 &&
         memcmp(x->key_width, y->key_width, sizeof x->key_width) == 0 &&
         memcmp(a->rdp_memory_coverage, b->rdp_memory_coverage, table) == 0;
}

/* What the importer did before the carry: the combiner block and the coverage
 * table of the peer are zero (the old commit zeroed them and no reader set
 * them). The control arm of the continuation comparison. */
static void rdp_drop_carried(t64_machine *machine) {
  t64_rdp_state *rdp = &machine->rdp;

  rdp->primitive_lod_fraction = 0u;
  rdp->environment_color = rdp->fog_color = rdp->blend_color = 0u;
  rdp->combine_mode = 0u;
  rdp->combine_mode_valid = rdp->environment_color_valid = false;
  rdp->fog_color_valid = rdp->blend_color_valid = false;
  rdp->key_center_valid = rdp->key_scale_valid = rdp->key_width_valid = false;
  (void)memset(rdp->key_center, 0, sizeof rdp->key_center);
  (void)memset(rdp->key_scale, 0, sizeof rdp->key_scale);
  (void)memset(rdp->key_width, 0, sizeof rdp->key_width);
  (void)memset(machine->rdp_memory_coverage, 0,
               T64_RDP_MEMORY_COVERAGE_BYTES(machine->profile.rdram_bytes));
}

/* The authored fixture rdp-fill.z64 (built from authored sources, no external
 * asset) run by its own guest code on the stem id: its fills mark the memory
 * coverage table with no hand-set state. The machine at the end of the fills
 * exports, a fresh peer imports it, and the peer holds every mark and the
 * same digest (the review of plan 09-10 found the table the one member the old
 * import lost on exactly this fixture; the machine has reached its authored
 * completion, so the continuation is the job of the rows B17 and of the
 * register-driven triangle above). */
/* A host that drains every retained video and audio packet. */
static void drain_all_packets(t64_machine *machine) {
  static uint8_t packet[T64_VIDEO_MAX_PACKET_BYTES];
  size_t copied = 0u;

  while (machine->vi.packet_count > 0u &&
         t64_video_copy(machine, packet, sizeof packet, &copied) ==
             T64_STATUS_OK) {
  }
  while (machine->ai.packet_count > 0u &&
         t64_audio_copy(machine, packet, sizeof packet, &copied) ==
             T64_STATUS_OK) {
  }
}

static t64_machine *create_admitted_rom(const char *profile_id,
                                        const char *path) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  static uint8_t rom[65536];
  FILE *file = path == NULL ? NULL : fopen(path, "rb");
  size_t length = 0u;

  if (file == NULL) {
    T64_CHECK(false);
    return NULL;
  }
  length = fread(rom, 1u, sizeof rom, file);
  T64_CHECK(fclose(file) == 0);
  T64_CHECK(length > 0u && length < sizeof rom);
  T64_CHECK_INT(t64_profile_lookup(profile_id, &profile), T64_STATUS_OK);
  if (profile == NULL) return NULL;
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine), T64_STATUS_OK);
  if (machine == NULL) return NULL;
  if (t64_machine_admit_rom(machine, rom, length) != T64_STATUS_OK) {
    T64_CHECK(false);
    t64_machine_destroy(machine);
    return NULL;
  }
  return machine;
}

static void rdp_state_fixture(void) {
  const char *path = getenv("T64_TEST_RDP_FILL_PATH");
  t64_machine *source = create_admitted_rom(SI_STEM_ID, path);
  t64_machine *peer = create_admitted_rom(SI_STEM_ID, path);
  t64_state_import_report report;
  uint8_t *blob;
  size_t length = 0u;

  if (source == NULL || peer == NULL) {
    if (source != NULL) t64_machine_destroy(source);
    if (peer != NULL) t64_machine_destroy(peer);
    return;
  }
  /* the fixture programs short VI fields: a host that drains every 50 ticks
   * keeps the two-slot output ring from filling */
  while (source->tick < 900000u &&
         !(source->rdp.fill_count != 0u && rdp_is_idle(source))) {
    t64_stop_request request = {T64_STOP_GUEST_TICK, source->tick + 50u};
    t64_stop_result stop = {0u, 0u};

    T64_CHECK_INT(t64_machine_advance(source, &request, &stop), T64_STATUS_OK);
    drain_all_packets(source);
    if (stop.reason != T64_STOP_REASON_BOUND) break; /* authored completion */
  }
  /* reached: the guest ran its fills and left marks in the coverage table */
  T64_CHECK(source->rdp.fill_count != 0u && rdp_is_idle(source));
  T64_CHECK(coverage_marks(source) != 0u);
  blob = export_blob(source, &length);
  if (blob != NULL) {
    T64_CHECK_INT(t64_machine_state_import(peer, blob, length, 0u, &report),
                  T64_STATUS_OK);
    T64_CHECK(rdp_carried_equal(source, peer));
    T64_CHECK_INT(coverage_marks(peer), coverage_marks(source));
    T64_CHECK(digests_equal(source, peer));
    T64_CHECK_INT(peer->tick, source->tick);
    free(blob);
  }
  t64_machine_destroy(source);
  t64_machine_destroy(peer);
}

/* Source and imported peer receive the same triangle: they draw the same
 * pixels, count the triangle and agree on the digest. The control arm (the
 * peer of the old importer) refuses the triangle and diverges; the comparator
 * therefore sees the loss it was written for. */
static void case_rdp_state_carry(void) {
  const size_t framebuffer = 0x5000u;
  const size_t frame_bytes = 8u * 8u * 2u;
  t64_machine *source = build_quiet();
  t64_machine *peer = create_admitted(SI_STEM_ID);
  t64_machine *old_peer = create_admitted(SI_STEM_ID);
  t64_state_import_report report;
  uint8_t before[8u * 8u * 2u];
  uint8_t *blob;
  size_t length = 0u;

  if (source == NULL || peer == NULL || old_peer == NULL) {
    T64_CHECK(false);
    if (source != NULL) t64_machine_destroy(source);
    if (peer != NULL) t64_machine_destroy(peer);
    if (old_peer != NULL) t64_machine_destroy(old_peer);
    return;
  }
  rdp_submit(source, rdp_carry_setup, RDP_CARRY_COUNT);
  rdp_run_idle(source);
  /* reached: the combiner block and the coverage marks of the fill are set */
  T64_CHECK(source->rdp.combine_mode_valid && source->rdp.environment_color_valid);
  T64_CHECK(source->rdp.fog_color_valid && source->rdp.blend_color_valid);
  T64_CHECK(source->rdp.key_center_valid && source->rdp.key_scale_valid &&
            source->rdp.key_width_valid);
  T64_CHECK_INT(source->rdp.fog_color, UINT32_C(0xa1b2c3d4));
  T64_CHECK_INT(source->rdp.key_width[0], 0x345u);
  T64_CHECK_INT(coverage_marks(source), 72u); /* 8 pixels in each of 9 rows */
  (void)memcpy(before, source->ram + framebuffer, frame_bytes);

  blob = export_blob(source, &length);
  if (blob == NULL) {
    T64_CHECK(false);
    t64_machine_destroy(source);
    t64_machine_destroy(peer);
    t64_machine_destroy(old_peer);
    return;
  }
  T64_CHECK_INT(t64_machine_state_import(peer, blob, length, 0u, &report),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_state_import(old_peer, blob, length, 0u, &report),
                T64_STATUS_OK);
  /* the import carries every member and the digests agree */
  T64_CHECK(rdp_carried_equal(source, peer));
  T64_CHECK_INT(coverage_marks(peer), 72u);
  T64_CHECK(digests_equal(source, peer));
  T64_CHECK_INT(peer->tick, source->tick);
  /* the control arm: the members an old import left behind */
  rdp_drop_carried(old_peer);
  T64_CHECK(!rdp_carried_equal(source, old_peer));
  T64_CHECK(!digests_equal(source, old_peer));

  rdp_submit(source, rdp_carry_triangle, 4u);
  rdp_submit(peer, rdp_carry_triangle, 4u);
  rdp_submit(old_peer, rdp_carry_triangle, 4u);
  T64_CHECK(advance_to(source, source->tick + 1500u));
  T64_CHECK(advance_to(peer, peer->tick + 1500u));
  T64_CHECK(advance_to(old_peer, old_peer->tick + 1500u));
  /* the source drew the triangle; the peer drew the same pixels */
  T64_CHECK_INT(source->rdp.triangle_count, 1u);
  T64_CHECK_INT(source->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT(peer->rdp.triangle_count, source->rdp.triangle_count);
  T64_CHECK_INT(peer->rdp.refusal_reason, source->rdp.refusal_reason);
  T64_CHECK(memcmp(source->ram + framebuffer, before, frame_bytes) != 0);
  T64_CHECK(memcmp(source->ram + framebuffer, peer->ram + framebuffer,
                   frame_bytes) == 0);
  T64_CHECK(digests_equal(source, peer));
  /* the control arm refuses the triangle: no combine mode */
  T64_CHECK_INT(old_peer->rdp.triangle_count, 0u);
  T64_CHECK_INT(old_peer->rdp.refusal_reason, T64_RDP_REFUSAL_RENDER_RANGE);
  T64_CHECK(memcmp(source->ram + framebuffer, old_peer->ram + framebuffer,
                   frame_bytes) != 0);
  T64_CHECK(!digests_equal(source, old_peer));
  free(blob);
  t64_machine_destroy(source);
  t64_machine_destroy(peer);
  t64_machine_destroy(old_peer);
  rdp_state_fixture();
}

#define BF T64_STATUS_STATE_FORMAT
#define BR T64_STATUS_STATE_RANGE
#define SEC_AUDIO T64_STATE_SECTION_AUDIO
#define SEC_SI T64_STATE_SECTION_SI_PIF
#define SEC_PI T64_STATE_SECTION_INTERRUPT_PI_SP
#define SEC_RDPVI T64_STATE_SECTION_RDP_VI
#define SEC_SAVE T64_STATE_SECTION_SAVE_MEDIA
#define SEC_OBS T64_STATE_SECTION_OBSERVER_CONTINUITY

static const mutant mutants[] = {
    {"header", ROW("state-version", T_S1, 0u, D_STATE_VERSION, 4u, 24u, RS_ALL,
                   T64_STATUS_STATE_VERSION, T64_STATE_REASON_STATE_VERSION, 0u, NR)},
    {"header", ROW("profile-digest", T_S1, 0u, D_PROFILE + 3u, 1u, 0x5Au, RS_ALL,
                   T64_STATUS_STATE_PROFILE, T64_STATE_REASON_PROFILE_DIGEST, 0u, NR)},
    {"header", ROW("content-digest", T_S1, 0u, D_CONTENT_SHA + 7u, 1u, 0x5Au, RS_ALL,
                   T64_STATUS_STATE_CONTENT, T64_STATE_REASON_CONTENT_DIGEST, 0u, NR)},
    {"header", ROW("content-length", T_S1, 0u, D_CONTENT_LEN + 7u, 1u, 0x01u, RS_ALL,
                   T64_STATUS_STATE_CONTENT, T64_STATE_REASON_CONTENT_LENGTH, 0u, NR)},
    {"header", ROW("build-id", T_S1, 0u, D_BUILD, 1u, 0x7Eu, RS_ALL,
                   T64_STATUS_STATE_BUILD, T64_STATE_REASON_BUILD_ID, 0u, NR)},
    {"header", CROW("total-length-plus-one", T_S1, mut_total_plus_one, BF,
                    T64_STATE_REASON_TOTAL_LENGTH, 0u, NR)},
    {"header", ROW("section-count-zero", T_S1, 0u, 0x018u, 4u, 0u, 0u, BF,
                   T64_STATE_REASON_SECTION_COUNT, 0u, NR)},
    {"header", ROW("header-flags", T_S1, 0u, D_FLAGS, 4u, 1u, 0u, BF,
                   T64_STATE_REASON_HEADER_FLAGS, 0u, NR)},
    {"header", ROW("header-reserved", T_S1, 0u, 0x11Fu, 1u, 1u, 0u, BF,
                   T64_STATE_REASON_HEADER_RESERVED, 0u, NR)},
    {"table", CROW("id-order", T_S1, mut_entry_out_of_order, BF,
                   T64_STATE_REASON_SECTION_ORDER, 1u, NR)},
    {"table", CROW("id-unknown", T_S1, mut_entry_unknown_id, BF,
                   T64_STATE_REASON_SECTION_ID_UNKNOWN, 99u, NR)},
    {"table", CROW("offset-overflow", T_S1, mut_entry_overflow, BF,
                   T64_STATE_REASON_SECTION_BOUNDS, 1u, NR)},
    {"table", CROW("overlap", T_S1, mut_entry_overlap, BF,
                   T64_STATE_REASON_SECTION_OVERLAP, 2u, NR)},
    {"table", CROW("length-short", T_S1, mut_rdram_short, BF,
                   T64_STATE_REASON_SECTION_LENGTH_EXPECTED, 3u, NR)},
    {"table", CROW("section-sha-stale", T_S1, mut_section_hash_only,
                   T64_STATUS_STATE_INTEGRITY, T64_STATE_REASON_SECTION_SHA256,
                   1u, NR)},
    {"fields", ROW("count-cap-audio-descriptors", T_S1, SEC_AUDIO, 96u, 4u, 3u,
                   RS_ALL, BR, T64_STATE_REASON_FIELD_RANGE, SEC_AUDIO,
                   T64_STATE_RULE_COUNT_CAP)},
    {"fields", ROW("index-lt-audio-slot", T_S1, SEC_AUDIO, 120u, 4u, 2u, RS_ALL,
                   BR, T64_STATE_REASON_FIELD_RANGE, SEC_AUDIO,
                   T64_STATE_RULE_INDEX_LT)},
    {"fields", ROW("enum-closed-si-direction", T_S1, SEC_SI, 12u, 4u, 3u, RS_ALL,
                   BR, T64_STATE_REASON_FIELD_RANGE, SEC_SI,
                   T64_STATE_RULE_ENUM_CLOSED)},
    {"fields", ROW("bool-01-pi-error", T_S1, SEC_PI, 60u, 4u, 2u, RS_ALL, BR,
                   T64_STATE_REASON_FIELD_RANGE, SEC_PI, T64_STATE_RULE_BOOL_01)},
    {"fields", ROW("align-si-address", T_S1, SEC_SI, 16u, 4u, 0x3901u, RS_ALL, BR,
                   T64_STATE_REASON_FIELD_RANGE, SEC_SI, T64_STATE_RULE_ALIGN)},
    {"fields", ROW("addr-range-si-address", T_S1, SEC_SI, 16u, 4u, 0x400000u,
                   RS_ALL, BR, T64_STATE_REASON_FIELD_RANGE, SEC_SI,
                   T64_STATE_RULE_ADDR_RANGE)},
    {"fields", ROW("tick-ge-sp-descriptor-end", T_S1, SEC_PI, S5_SP_ACTIVE_END_TICK,
                   8u, 0u, RS_ALL, BR, T64_STATE_REASON_FIELD_RANGE, SEC_PI,
                   T64_STATE_RULE_TICK_GE)},
    {"fields", ROW("tick-le-sram-deadline", T_S32K, SEC_SAVE, 8u, 8u, 2001u, RS_ALL,
                   BR, T64_STATE_REASON_FIELD_RANGE, SEC_SAVE,
                   T64_STATE_RULE_TICK_LE)},
    {"fields", CROW("seq-lt-sequence-above-next", T_S1, mut_sequence_above, INV,
                    T64_STATE_REASON_QUEUE_SEQUENCE, SCHED,
                    T64_STATE_RULE_SEQ_LT)},
    {"fields", ROW("shift-lt-tile-shift", T_S1, SEC_RDPVI, S8_TILE0_SHIFT_S, 4u,
                   16u, RS_ALL, BR, T64_STATE_REASON_FIELD_RANGE, SEC_RDPVI,
                   T64_STATE_RULE_SHIFT_LT)},
    {"fields", ROW("len-le-video-packet", T_S1, SEC_RDPVI, S8_VI_SLOT0 + 4u, 4u,
                   0x00080000u, RS_ALL, BR, T64_STATE_REASON_FIELD_RANGE,
                   SEC_RDPVI, T64_STATE_RULE_LEN_LE)},
    {"fields", ROW("profile-derived-rdram-size", T_S1, T64_STATE_SECTION_RDRAM,
                   0u, 4u, 0x200000u, RS_ALL, BR, T64_STATE_REASON_FIELD_DERIVED,
                   T64_STATE_SECTION_RDRAM, T64_STATE_RULE_PROFILE_DERIVED)},
    {"fields", ROW("derived-eq-interrupt-line", T_S1, SEC_PI, 8u, 4u, 1u, RS_ALL,
                   BR, T64_STATE_REASON_FIELD_DERIVED, SEC_PI,
                   T64_STATE_RULE_DERIVED_EQ)},
    {"fields", ROW("pad-zero-clock-policy", T_S1, SEC_RDPVI,
                   S8_VI_SLOT0_CLOCK_POLICY + 10u, 1u, 0x78u, RS_ALL, BR,
                   T64_STATE_REASON_FIELD_PADDING, SEC_RDPVI,
                   T64_STATE_RULE_PAD_ZERO)},
    {"fields", ROW("reserved-zero-interrupt-pending", T_S1, SEC_PI, 3u, 1u, 0x80u,
                   RS_ALL, BR, T64_STATE_REASON_FIELD_RESERVED, SEC_PI,
                   T64_STATE_RULE_RESERVED_ZERO)},
    {"fields", ROW("observer-range", T_S1, SEC_OBS, 0u, 8u, UINT64_C(1) << 40,
                   RS_ALL, BR, T64_STATE_REASON_OBSERVER_RANGE, SEC_OBS,
                   T64_STATE_RULE_LEN_LE)},
    {"fields", ROW("busy-until-future", T_E4K, SEC_SAVE, 8u, 8u, 2000u + 375001u,
                   RS_ALL, BR, T64_STATE_REASON_FIELD_RANGE, SEC_SAVE,
                   T64_STATE_RULE_TICK_LE)},
    {"fields", CROW("render-extent-beyond-width", T_S1, mut_render_extent, BR,
                    T64_STATE_REASON_FIELD_RANGE, SEC_RDPVI,
                    T64_STATE_RULE_ADDR_RANGE)},
    {"fields", CROW("render-triangle-row-beyond-coverage", T_S1, mut_tri_rows,
                    BR, T64_STATE_REASON_FIELD_RANGE, SEC_RDPVI,
                    T64_STATE_RULE_INDEX_LT)},
    {"fields", CROW("render-triangle-flag-idle", T_S1, mut_tri_flag_idle, BR,
                    T64_STATE_REASON_FIELD_DERIVED, SEC_RDPVI,
                    T64_STATE_RULE_DERIVED_EQ)},
    {"fields", CROW("render-triangle-opcode", T_S1, mut_tri_opcode, BR,
                    T64_STATE_REASON_FIELD_RANGE, SEC_RDPVI,
                    T64_STATE_RULE_ENUM_CLOSED)},
    {"fields", CROW("render-triangle-word-count", T_S1, mut_tri_word_count, BR,
                    T64_STATE_REASON_FIELD_DERIVED, SEC_RDPVI,
                    T64_STATE_RULE_DERIVED_EQ)},
    {"fields", CROW("render-triangle-shade-flag", T_S1, mut_tri_shade_flag, BR,
                    T64_STATE_REASON_FIELD_DERIVED, SEC_RDPVI,
                    T64_STATE_RULE_DERIVED_EQ)},
    {"fields", CROW("render-triangle-begin", T_S1, mut_tri_begin, BR,
                    T64_STATE_REASON_FIELD_DERIVED, SEC_RDPVI,
                    T64_STATE_RULE_DERIVED_EQ)},
    {"fields", CROW("render-triangle-shade-products", T_S1,
                    mut_tri_shade_overflow, BR, T64_STATE_REASON_FIELD_RANGE,
                    SEC_RDPVI, T64_STATE_RULE_LEN_LE)},
    {"fields", CROW("render-triangle-row-past-end", T_S1, mut_tri_row_past_end,
                    BR, T64_STATE_REASON_FIELD_RANGE, SEC_RDPVI,
                    T64_STATE_RULE_INDEX_LT)},
    {"fields", ROW("rdp-carried-lod-range", T_S1, SEC_RDPVI, S8_TAIL, 4u,
                   0x100u, RS_ALL, BR, T64_STATE_REASON_FIELD_RANGE, SEC_RDPVI,
                   T64_STATE_RULE_INDEX_LT)},
    {"fields", ROW("rdp-carried-flag-bool", T_S1, SEC_RDPVI, S8_TAIL + 24u, 4u,
                   2u, RS_ALL, BR, T64_STATE_REASON_FIELD_RANGE, SEC_RDPVI,
                   T64_STATE_RULE_BOOL_01)},
    {"fields", ROW("rdp-carried-key-width-range", T_S1, SEC_RDPVI, S8_TAIL + 60u,
                   4u, 0x1000u, RS_ALL, BR, T64_STATE_REASON_FIELD_RANGE,
                   SEC_RDPVI, T64_STATE_RULE_INDEX_LT)},
    {"fields", ROW("rdp-carried-table-size", T_S1, SEC_RDPVI, S8_TAIL + 88u, 4u,
                   0x1000u, RS_ALL, BR, T64_STATE_REASON_FIELD_DERIVED, SEC_RDPVI,
                   T64_STATE_RULE_PROFILE_DERIVED)},
    {"fields", ROW("rdp-carried-coverage-entry", T_S1, SEC_RDPVI,
                   S8_TAIL + 92u + 100u, 1u, 0x89u, RS_ALL, BR,
                   T64_STATE_REASON_FIELD_RANGE, SEC_RDPVI,
                   T64_STATE_RULE_ENUM_CLOSED)},
    {"queue", ROW("queue-count-cap", T_S1, SCHED, 0u, 4u, 257u, RS_ALL, INV,
                  T64_STATE_REASON_QUEUE_COUNT, SCHED, T64_STATE_RULE_COUNT_CAP)},
    {"queue", CROW("queue-order", T_S1, mut_swap_events, INV,
                   T64_STATE_REASON_QUEUE_ORDER, SCHED, NR)},
    {"queue", CROW("equal-tuples", T_S1, mut_equal_tuples, INV,
                   T64_STATE_REASON_QUEUE_ORDER, SCHED, NR)},
    {"queue", CROW("sequence-at-next", T_S1, mut_sequence_equal, INV,
                   T64_STATE_REASON_QUEUE_SEQUENCE, SCHED,
                   T64_STATE_RULE_SEQ_LT)},
    {"queue", CROW("tick-below", T_S1, mut_tick_below, INV,
                   T64_STATE_REASON_QUEUE_TICK, SCHED, T64_STATE_RULE_TICK_GE)},
    {"queue", CROW("unknown-pair", T_S1, mut_pair_outside, INV,
                   T64_STATE_REASON_QUEUE_KIND_UNKNOWN, SCHED,
                   T64_STATE_RULE_ENUM_CLOSED)},
    {"queue", CROW("synthetic-device", T_S1, mut_pair_synthetic, INV,
                   T64_STATE_REASON_QUEUE_KIND_UNKNOWN, SCHED,
                   T64_STATE_RULE_ENUM_CLOSED)},
    {"queue", ROW("exhausted-flag", T_S1, SCHED, 12u, 4u, 1u, RS_ALL, INV,
                  T64_STATE_REASON_QUEUE_SEQUENCE, SCHED,
                  T64_STATE_RULE_SEQ_LT)},
    {"devices", CROW("cpu-no-step", T_S1, mut_remove_cpu, INV,
                     T64_STATE_REASON_DEVICE_EVENT_MISSING, SCHED, NR)},
    {"devices", CROW("cpu-two-steps", T_S1, mut_append_cpu, INV,
                     T64_STATE_REASON_DEVICE_EVENT_EXTRA, SCHED, NR)},
    {"devices", CROW("si-complete-missing", T_S1, mut_remove_si_complete, INV,
                     T64_STATE_REASON_DEVICE_EVENT_MISSING, SCHED, NR)},
    {"devices", CROW("si-token", T_S1, mut_si_token_skew, INV,
                     T64_STATE_REASON_DEVICE_TOKEN_MISMATCH, SCHED, NR)},
    {"devices", CROW("pi-extra", T_S1, mut_append_pi, INV,
                     T64_STATE_REASON_DEVICE_EVENT_EXTRA, SCHED, NR)},
    {"devices", CROW("sp-complete-extra", T_S1, mut_append_sp_complete, INV,
                     T64_STATE_REASON_DEVICE_EVENT_EXTRA, SCHED, NR)},
    {"devices", CROW("rsp-token", T_S1, mut_rsp_token, INV,
                     T64_STATE_REASON_DEVICE_TOKEN_MISMATCH, SCHED, NR)},
    {"devices", CROW("rdp-extra", T_S1, mut_append_rdp, INV,
                     T64_STATE_REASON_DEVICE_EVENT_EXTRA, SCHED, NR)},
    {"devices", CROW("vi-token", T_S1, mut_vi_token, INV,
                     T64_STATE_REASON_DEVICE_TOKEN_MISMATCH, SCHED, NR)},
    {"devices", CROW("ai-token", T_S1, mut_ai_token, INV,
                     T64_STATE_REASON_DEVICE_TOKEN_MISMATCH, SCHED, NR)},
    {"digest", ROW("declared-digest-stale", T_S1, T64_STATE_SECTION_MACHINE_CORE,
                   8u, 8u, 0x1234u, RS_SECTIONS | RS_TRAILER,
                   T64_STATUS_STATE_INTEGRITY, T64_STATE_REASON_DECLARED_DIGEST,
                   0u, NR)},
};

#define MUTANT_COUNT (sizeof mutants / sizeof mutants[0])

/* ---- the cases ------------------------------------------------------------- */

static uint64_t env_u64(const char *name, uint64_t fallback) {
  const char *value = getenv(name);

  return value != NULL && value[0] != '\0' ? strtoull(value, NULL, 10) : fallback;
}

static void case_mutant_inventory(void) {
  static const char *const classes[] = {"header", "table",   "fields",
                                        "queue",  "devices", "digest"};
  static const size_t minimum[] = {9u, 6u, 15u, 7u, 10u, 1u};
  size_t i;
  size_t k;

  T64_CHECK(inventory_load());
  T64_CHECK(MUTANT_COUNT >= 40u);
  T64_CHECK_INT(inventory_rows, MUTANT_COUNT);
  for (i = 0u; i < MUTANT_COUNT && i < inventory_rows; i++) {
    const mutant *m = &mutants[i];

    if (strcmp(m->r.name, inventory[i].name) != 0 ||
        strcmp(m->cls, inventory[i].cls) != 0 ||
        (uint32_t)m->r.status != inventory[i].status ||
        m->r.reason != inventory[i].reason ||
        m->r.section_id != inventory[i].section || m->r.rule != inventory[i].rule) {
      fprintf(stderr, "mutant %zu: table %s/%s differs from inventory %s/%s\n", i,
              m->r.name, m->cls, inventory[i].name, inventory[i].cls);
    }
    T64_CHECK_STR(m->r.name, inventory[i].name);
    T64_CHECK_STR(m->cls, inventory[i].cls);
    T64_CHECK_INT(m->r.status, inventory[i].status);
    T64_CHECK_INT(m->r.reason, inventory[i].reason);
    T64_CHECK_INT(m->r.section_id, inventory[i].section);
    T64_CHECK_INT(m->r.rule, inventory[i].rule);
  }
  for (k = 0u; k < sizeof classes / sizeof classes[0]; k++) {
    size_t count = 0u;

    for (i = 0u; i < MUTANT_COUNT; i++) {
      if (strcmp(mutants[i].cls, classes[k]) == 0) count++;
    }
    T64_CHECK(count >= minimum[k]);
  }
  /* the names are unique */
  for (i = 0u; i < MUTANT_COUNT; i++) {
    for (k = i + 1u; k < MUTANT_COUNT; k++) {
      T64_CHECK(strcmp(mutants[i].r.name, mutants[k].r.name) != 0);
    }
  }
}

static void case_mutant_refusals(void) {
  static row rows[MUTANT_COUNT];
  size_t i;

  if (!targets_prepare()) return;
  for (i = 0u; i < MUTANT_COUNT; i++) rows[i] = mutants[i].r;
  run_rows(rows, MUTANT_COUNT, CHECK_UNTOUCHED | CHECK_SCRATCH);
  /* the accepted baseline of the render-extent mutant: the unmutated render
   * state imports and its continuation equals the source */
  if (render_prepare()) {
    t64_machine *peer = create_admitted(SI_STEM_ID);
    t64_machine *source = build_render();
    t64_state_import_report report;

    if (peer != NULL && source != NULL) {
      T64_CHECK_INT(t64_machine_state_import(peer, render_blob, render_length,
                                             0u, &report),
                    T64_STATUS_OK);
      T64_CHECK(digests_equal(peer, source));
      T64_CHECK(advance_to(peer, peer->tick + 400u));
      T64_CHECK(advance_to(source, source->tick + 400u));
      T64_CHECK(digests_equal(peer, source));
    }
    if (peer != NULL) t64_machine_destroy(peer);
    if (source != NULL) t64_machine_destroy(source);
  }
  /* the accepted baselines of the triangle render mutants (FX-01): the
   * unmutated blobs import into a fresh peer in the same setup and the peer
   * continues like the source for 400 ticks */
  for (i = 0u; i < (size_t)TRI_COUNT; i++) {
    t64_machine *peer;
    t64_machine *source;
    t64_state_import_report report;

    if (!tri_prepare((tri_kind)i)) continue;
    peer = create_admitted(SI_STEM_ID);
    source = build_tri_render((tri_kind)i);
    if (peer != NULL && source != NULL) {
      T64_CHECK_INT(t64_machine_state_import(peer, tri_blobs[i].blob,
                                             tri_blobs[i].length, 0u, &report),
                    T64_STATUS_OK);
      T64_CHECK(advance_to(peer, peer->tick + 400u));
      T64_CHECK(advance_to(source, source->tick + 400u));
    }
    if (peer != NULL) t64_machine_destroy(peer);
    if (source != NULL) t64_machine_destroy(source);
  }
}

static void print_fuzz_stats(const char *label, const state_fuzz_stats *stats) {
  size_t i;

  fprintf(stderr,
          "%s: iterations %llu refusals %llu accepted %llu liveness-stops %llu "
          "violations %llu; by reason:",
          label, (unsigned long long)stats->iterations,
          (unsigned long long)stats->refusals,
          (unsigned long long)stats->accepted,
          (unsigned long long)stats->liveness_stops,
          (unsigned long long)stats->violations);
  for (i = 0u; i < 48u; i++) {
    if (stats->by_reason[i] != 0u) {
      fprintf(stderr, " %zu=%llu", i, (unsigned long long)stats->by_reason[i]);
    }
  }
  fputc('\n', stderr);
}

/* A bit flip at sampled header bytes and strided body offsets of one blob,
 * each in two integrity modes: every integrity field stale (the trailer check
 * is the first failure) and every integrity field re-signed (the deeper rules
 * run). */
/* The windows of the campaign and the sweep: the rule-bearing fields of section
 * 8 (the RDP scalars and the packet words at its start, the tile descriptors and
 * the VI block before the retained packet slots, and the carried tail) and the
 * small sections whole (the readers of sections 5, 6, 7, 9, 10, 11 and 16 are
 * reached through the fallback of the mutator, which skips RDRAM and SP memory). */
static const state_fuzz_window campaign_windows[] = {
    {T64_STATE_SECTION_RDP_VI, 0u, 400u},
    {T64_STATE_SECTION_RDP_VI, S8_TILE0_SHIFT_S - 160u, 1700u},
    {T64_STATE_SECTION_RDP_VI, S8_TAIL, 96u},
    {T64_STATE_SECTION_INTERRUPT_PI_SP, 0u, 276u},
    {T64_STATE_SECTION_SI_PIF, 0u, 404u},
    {T64_STATE_SECTION_AUDIO, 0u, 336u},
    {T64_STATE_SECTION_OBSERVER_CONTINUITY, 0u, 40u},
};

/* A body position of a sweep sample: inside a window of the blob when the
 * window fits the section, else the strided position of the first version. */
static size_t sweep_body_position(const uint8_t *blob, size_t length,
                                  size_t sample, size_t strided) {
  const state_fuzz_window *w =
      &campaign_windows[sample % (sizeof campaign_windows /
                                  sizeof campaign_windows[0])];
  size_t base = 0u;
  size_t body = 0u;

  if (find_section(blob, w->section, &base, &body) && w->length >= 8u &&
      w->offset <= body && w->length <= body - w->offset &&
      base + w->offset + w->length <= length) {
    return base + w->offset +
           ((sample * 40503u + sample / 7u) % (w->length - 4u) & ~(size_t)3u);
  }
  return strided;
}

static void sweep_blob(const uint8_t *blob, size_t length, size_t header_samples,
                       size_t body_samples, state_fuzz_stats *stats) {
  uint8_t *copy = (uint8_t *)malloc(length);
  t64_machine *peer = create_admitted(SI_STEM_ID);
  const size_t header_stride = header_samples == 0u ? D_TABLE : D_TABLE / header_samples;
  const size_t body_stride = body_samples == 0u ? length : (length - D_TABLE) / body_samples;
  size_t sample;
  uint64_t index = 0u;

  if (copy == NULL || peer == NULL) {
    T64_CHECK(false);
    free(copy);
    if (peer != NULL) t64_machine_destroy(peer);
    return;
  }
  for (sample = 0u; sample < header_samples + body_samples && peer != NULL; sample++) {
    const size_t offset =
        sample < header_samples
            ? sample * (header_stride == 0u ? 1u : header_stride)
            : sweep_body_position(
                  blob, length, sample - header_samples,
                  D_TABLE + (sample - header_samples) * body_stride +
                      (sample % 7u));
    unsigned mode;

    if (offset >= length) continue;
    for (mode = 0u; mode < 2u && peer != NULL; mode++) {
      (void)memcpy(copy, blob, length);
      copy[offset] ^= (uint8_t)(1u << (offset & 7u));
      if (mode == 1u) blob_resign(copy, length, RS_ALL);
      peer = state_fuzz_one(peer, copy, length, index++, stats);
    }
  }
  if (peer != NULL) t64_machine_destroy(peer);
  free(copy);
}

static const char *const sweep_boundaries[] = {"B05", "B07", "B10", "B04", "B14"};

static void case_mutation_sweep(void) {
  state_fuzz_stats total;
  state_fuzz_stats stats;
  const size_t header_samples = (size_t)env_u64("T64_STATE_SWEEP_HEADER", 6u);
  const size_t body_samples = (size_t)env_u64("T64_STATE_SWEEP_BODY", 6u);
  const size_t blobs = (size_t)env_u64("T64_STATE_SWEEP_BLOBS", 1u);
  size_t i;

  (void)memset(&total, 0, sizeof total);
  if (!targets_prepare()) return;
  state_fuzz_set_windows(campaign_windows,
                         sizeof campaign_windows / sizeof campaign_windows[0]);
  (void)memset(&stats, 0, sizeof stats);
  sweep_blob(target_blob[T_S1], target_length[T_S1], header_samples, body_samples,
             &stats);
  total.iterations += stats.iterations;
  total.refusals += stats.refusals;
  total.accepted += stats.accepted;
  total.violations += stats.violations;
  total.field_level += stats.field_level;
  for (i = 1u; i < blobs && i - 1u < sizeof sweep_boundaries / sizeof sweep_boundaries[0];
       i++) {
    const size_t index = boundary_index(sweep_boundaries[i - 1u]);

    if (index == BOUNDARY_COUNT || !boundary_prepare(index)) continue;
    (void)memset(&stats, 0, sizeof stats);
    sweep_blob(prepared[index].blob, prepared[index].length, header_samples,
               body_samples, &stats);
    total.iterations += stats.iterations;
    total.refusals += stats.refusals;
    total.accepted += stats.accepted;
    total.violations += stats.violations;
    total.field_level += stats.field_level;
  }
  if (getenv("T64_STATE_MATRIX_TRACE") != NULL) print_fuzz_stats("sweep", &total);
  T64_CHECK(total.iterations > 0u);
  T64_CHECK_INT(total.violations, 0u);
  /* reach, not a tautology: the re-signed arm of the sweep must be refused by
   * a reader or a later check or accepted for at least a quarter of its flips */
  T64_CHECK(total.field_level + total.accepted >= total.iterations / 8u);
}

static void case_fuzz_campaign(void) {
  state_fuzz_stats stats;
  const uint64_t iterations = env_u64("T64_STATE_FUZZ_ITERATIONS", 12u);

  if (!targets_prepare()) return;
  state_fuzz_set_windows(campaign_windows,
                         sizeof campaign_windows / sizeof campaign_windows[0]);
  state_fuzz_campaign(target_blob[T_S1], target_length[T_S1], iterations,
                      UINT64_C(0x5EED0911), &stats);
  print_fuzz_stats("fuzz-campaign", &stats);
  T64_CHECK_INT(stats.iterations, iterations);
  T64_CHECK_INT(stats.violations, 0u);
  /* reach: a campaign whose refusals all stop at the container level proves
   * nothing about the readers (the first version ended 12 of 12 iterations in
   * TOTAL_LENGTH or TRAILER_SHA256), so at least a quarter of the iterations
   * (and one) must be refused by a reader or a later check or be accepted */
  T64_CHECK(stats.field_level + stats.accepted >=
            (iterations / 4u > 1u ? iterations / 4u : 1u));
  /* a second base blob with a shaded triangle render in progress, so the
   * reader rules of the render fields (plan 09-10 FX-01) are in the reach of
   * the campaign; half the iterations, another seed */
  if (tri_prepare(TRI_SHADE)) {
    const uint64_t more = iterations / 2u > 6u ? iterations / 2u : 6u;

    state_fuzz_campaign(tri_blobs[TRI_SHADE].blob, tri_blobs[TRI_SHADE].length,
                        more, UINT64_C(0x7121A9E), &stats);
    print_fuzz_stats("fuzz-campaign-triangle", &stats);
    T64_CHECK_INT(stats.iterations, more);
    T64_CHECK_INT(stats.violations, 0u);
    T64_CHECK(stats.field_level + stats.accepted >=
              (more / 4u > 1u ? more / 4u : 1u));
  }
}

/* ---- liveness on hostile and semantically unreachable states --------------- */

typedef struct {
  const char *name;
  t64_machine *(*build)(void);
  void (*edit)(t64_machine *machine);
  int expect;           /* 1 accepted, -1 refused, 0 either */
  const char *profile;  /* the peer profile (the blob binds it) */
} hostile;

static void edit_vi_zero_period(t64_machine *m) {
  m->vi.registers[6] = 0u;
  m->vi.active_registers[6] = 0u;
}

static void edit_vi_zero_h_sync(t64_machine *m) {
  m->vi.registers[7] = 0u;
  m->vi.active_registers[7] = 0u;
}

static void edit_ai_zero_length(t64_machine *m) {
  m->ai.descriptors[m->ai.descriptor_head].length = 0u;
}

static void edit_ai_max_length(t64_machine *m) {
  m->ai.descriptors[m->ai.descriptor_head].length = UINT32_C(0x3fff8);
}

static void edit_tick_max(t64_machine *m) {
  size_t i;

  for (i = 0u; i < m->queue_count; i++) {
    t64_event *e = &m->events[(m->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];

    if (e->device == T64_DEVICE_PI) e->tick = UINT64_MAX;
  }
}

static void edit_tick_max_minus_one(t64_machine *m) {
  size_t i;

  for (i = 0u; i < m->queue_count; i++) {
    t64_event *e = &m->events[(m->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];

    if (e->device == T64_DEVICE_PI) e->tick = UINT64_MAX - 1u;
  }
}

static void edit_si_table_max(t64_machine *m) {
  size_t i;

  m->si.table.entry_count = 5u;
  for (i = 0u; i < 5u; i++) {
    m->si.table.entries[i].channel = (uint32_t)i;
    m->si.table.entries[i].offset = 0u;
    m->si.table.entries[i].tx_count = 61u;
    m->si.table.entries[i].rx_count = 61u;
  }
}

static void edit_pi_length_zero(t64_machine *m) {
  m->pi.active_length = 0u;
}

static void edit_sp_length_zero(t64_machine *m) {
  m->sp.active.raw_length = 0u;
  m->sp.active.payload_bytes = 0u;
  m->sp.active.line_count = 0u;
  m->sp.pending.valid = false;
}

static void edit_render_inverted(t64_machine *m) {
  m->rdp.render_x_begin = 6u;
  m->rdp.render_x_end = 2u;
}

static void edit_cpu_pc_top(t64_machine *m) {
  m->cpu.pc = UINT32_C(0xFFFFFFFC);
  m->cpu.pc_full = t64_cpu_canonical_address(m->cpu.pc);
}

static void edit_busy_window(t64_machine *m) {
  m->save.busy_until = m->tick + EEPROM_BUSY_WINDOW;
}

static void edit_busy_max(t64_machine *m) {
  m->save.busy_until = UINT64_MAX;
}

static void edit_sequence_max(t64_machine *m) {
  m->next_sequence = UINT64_MAX;
}

static t64_machine *build_quiet_hostile(void) {
  return build_quiet();
}

static t64_machine *build_h_b01(void) { return build_b01(); }
static t64_machine *build_h_b03(void) { return build_b03(); }
static t64_machine *build_h_b06(void) { return build_b06(); }
static t64_machine *build_h_b08(void) { return build_b08(); }
static t64_machine *build_h_b15(void) { return build_b15(); }
static t64_machine *build_h_s1(void) { return build_s1(SI_STEM_ID); }

static const hostile hostile_states[] = {
    {"vi zero vertical period", build_h_b06, edit_vi_zero_period, 1, SI_STEM_ID},
    {"vi zero horizontal sync", build_h_b06, edit_vi_zero_h_sync, 1, SI_STEM_ID},
    {"ai descriptor length 0", build_h_b08, edit_ai_zero_length, 0, SI_STEM_ID},
    {"ai descriptor length maximum", build_h_b08, edit_ai_max_length, 0, SI_STEM_ID},
    {"event tick 64-bit maximum", build_h_b01, edit_tick_max, 0, SI_STEM_ID},
    {"event tick maximum minus one", build_h_b01, edit_tick_max_minus_one, 0, SI_STEM_ID},
    {"si table five entries at their maximum", build_h_s1, edit_si_table_max, 0, SI_STEM_ID},
    {"pi transfer length 0", build_h_b01, edit_pi_length_zero, -1, SI_STEM_ID},
    {"sp transfer length 0", build_h_b03, edit_sp_length_zero, 0, SI_STEM_ID},
    {"rdp render range inverted", build_render, edit_render_inverted, 1, SI_STEM_ID},
    {"cpu pc at the top of the address space", build_quiet_hostile, edit_cpu_pc_top, 1, SI_STEM_ID},
    {"eeprom busy_until at the window", build_h_b15, edit_busy_window, 1, SAVE_E4K_ID},
    {"eeprom busy_until at the 64-bit maximum", build_h_b15, edit_busy_max, -1, SAVE_E4K_ID},
    {"next_sequence at the maximum", build_quiet_hostile, edit_sequence_max, 0, SI_STEM_ID},
};

static void case_liveness_bounded_advance(void) {
  state_fuzz_stats stats;
  size_t i;

  (void)memset(&stats, 0, sizeof stats);
  for (i = 0u; i < sizeof hostile_states / sizeof hostile_states[0]; i++) {
    const hostile *h = &hostile_states[i];
    t64_machine *source = h->build();
    t64_machine *peer = create_admitted(h->profile);
    uint8_t *blob;
    size_t length = 0u;
    const uint64_t accepted_before = stats.accepted;
    const uint64_t refusals_before = stats.refusals;

    if (source == NULL || peer == NULL) {
      fprintf(stderr, "hostile state %s not built\n", h->name);
      T64_CHECK(false);
      if (source != NULL) t64_machine_destroy(source);
      if (peer != NULL) t64_machine_destroy(peer);
      continue;
    }
    h->edit(source);
    blob = export_blob(source, &length);
    t64_machine_destroy(source);
    if (blob != NULL) {
      peer = state_fuzz_one(peer, blob, length, (uint64_t)i, &stats);
      if (getenv("T64_STATE_MATRIX_TRACE") != NULL) {
        fprintf(stderr, "liveness %s: %s\n", h->name,
                stats.accepted > accepted_before ? "accepted, advance returned"
                                                 : "refused");
      }
      if (h->expect > 0) T64_CHECK(stats.accepted > accepted_before);
      if (h->expect < 0) T64_CHECK(stats.refusals > refusals_before);
      free(blob);
    }
    if (peer != NULL) t64_machine_destroy(peer);
  }
  if (getenv("T64_STATE_MATRIX_TRACE") != NULL) print_fuzz_stats("liveness", &stats);
  T64_CHECK_INT(stats.violations, 0u);
  T64_CHECK_INT(stats.refusals + stats.accepted, stats.iterations);
}

static void mutants_release(void) {
  free(render_blob);
  render_blob = NULL;
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
      "dirty-peer",         "observer-flag",      "rdp-state-carry",
      "boundary-reached",   "boundary-roundtrip",   "boundary-continuation",
      "boundary-perturbation", "boundary-event-rules", "mid-tick-resume",
      "stalled-observer-stop", "drop-observers-outcomes", "mutant-inventory",
      "mutant-refusals", "mutation-sweep", "fuzz-campaign",
      "liveness-bounded-advance"};
  static const char *const suites[] = {
      "import",   "import",   "refusals", "refusals", "refusals",
      "refusals", "refusals", "refusals", "refusals", "ladder",
      "ladder",   "ladder",   "ladder",   "ladder",   "ladder",
      "boundaries", "boundaries", "boundaries", "boundaries", "boundaries",
      "boundaries", "observers", "observers", "adversarial", "adversarial",
      "adversarial", "adversarial", "adversarial"};
  static void (*const runners[])(void) = {
      case_roundtrip_all,        case_handler_table_ledger,
      case_binding_refusals,     case_container_refusals,
      case_field_range_refusals, case_invariant_accepts,
      case_invariant_refusals,   case_scratch_storage,
      case_refusal_untouched,    case_idempotence,
      case_save_media_all,       case_continuation_partitions,
      case_dirty_peer,           case_observer_flag,
      case_rdp_state_carry,
      case_boundary_reached,     case_boundary_roundtrip,
      case_boundary_continuation, case_boundary_perturbation,
      case_boundary_event_rules, case_mid_tick_resume,
      case_stalled_observer_stop,
      case_drop_observers_outcomes, case_mutant_inventory,
      case_mutant_refusals, case_mutation_sweep, case_fuzz_campaign,
      case_liveness_bounded_advance};
  const size_t count = sizeof cases / sizeof cases[0];
  size_t index;

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (index = 0u; index < count; index++) printf("%s\n", cases[index]);
    return 0;
  }
  if (argc == 2 && strcmp(argv[1], "--list-mutants") == 0) {
    for (index = 0u; index < MUTANT_COUNT; index++) {
      printf("%s\n", mutants[index].r.name);
    }
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
        mutants_release();
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
    mutants_release();
    (void)snprintf(name, sizeof name, "unit.core.state.import.%s", argv[2]);
    return T64_TEST_FINISH(name);
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--list-mutants|--case NAME|--suite NAME]\n",
            argv[0]);
    return 2;
  }
  for (index = 0u; index < count; index++) runners[index]();
  peers_release();
  targets_release();
  boundaries_release();
  mutants_release();
  return T64_TEST_FINISH("unit.core.state.import");
}
