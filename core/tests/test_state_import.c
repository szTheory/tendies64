/*
 * State import tests (plan 09-09).
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

#include "t64_internal.h"
#include "t64_sha256.h"
#include "t64_test.h"

#define IPL3_ASSET_BYTES 4096u
#define SI_STEM_ID "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1"

/* ---- test allocator with counters (the shape of core/tests/test_profile.c) */

static size_t allocate_calls;
static size_t release_calls;

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
  allocate_calls++;
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  return true;
}

static void test_release(void *context, t64_host_storage storage) {
  (void)context;
  release_calls++;
  free(storage.data);
}

static const t64_host_allocator allocator = {test_allocate, test_release,
                                             NULL};

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

static t64_machine *create_admitted(const char *profile_id) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t asset[IPL3_ASSET_BYTES];
  t64_status status;

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
  status = t64_machine_admit_rom(machine, asset, sizeof asset);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return NULL;
  }
  return machine;
}

#define ADVANCE_SLICE_TICKS 2000u

/* One absolute guest-tick bound; a long distance is walked in slices because
 * every CPU step is one dispatched event and the profile caps the events of one
 * advance call. */
static bool advance_to(t64_machine *machine, uint64_t tick) {
  while (machine->tick < tick) {
    const uint64_t step = tick - machine->tick > ADVANCE_SLICE_TICKS
                              ? machine->tick + ADVANCE_SLICE_TICKS
                              : tick;
    t64_stop_request request = {T64_STOP_GUEST_TICK, step};
    t64_stop_result stop = {0u, 0u};
    const t64_status status = t64_machine_advance(machine, &request, &stop);

    T64_CHECK_INT(status, T64_STATUS_OK);
    T64_CHECK_INT(stop.reason, T64_STOP_REASON_BOUND);
    if (status != T64_STATUS_OK || stop.reason != T64_STOP_REASON_BOUND) {
      return false;
    }
  }
  return machine->tick == tick;
}

static bool digest_of(const t64_machine *machine,
                      uint8_t digest[T64_DIGEST_SIZE]) {
  const t64_status status = t64_machine_digest(machine, digest);

  T64_CHECK_INT(status, T64_STATUS_OK);
  return status == T64_STATUS_OK;
}

static bool digests_equal(const t64_machine *left, const t64_machine *right) {
  uint8_t a[T64_DIGEST_SIZE];
  uint8_t b[T64_DIGEST_SIZE];

  if (!digest_of(left, a) || !digest_of(right, b)) return false;
  return memcmp(a, b, sizeof a) == 0;
}

/* ---- the S1 state: an SI RD64B in flight with its execute still pending --- */

#define SI_DRAM_ADDR UINT32_C(0x04800000)
#define SI_RD64B UINT32_C(0x04800004)
#define SI_WR64B UINT32_C(0x04800010)
#define PIF_ADDRESS UINT32_C(0x1FC007C0)

/* The 09-03 tracer-poll frame: channel 0 and channel 1 each carry command 0x01
 * expecting four bytes, then the end escape, and byte 63 requests a parse. */
static const uint8_t poll_frame[64] = {
    0x01, 0x04, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
    0x01, 0x04, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFE, [63] = 0x01};

static void bus_write_bytes(t64_machine *machine, uint32_t address,
                            const uint8_t *bytes, size_t count) {
  size_t index;

  for (index = 0u; index < count; index++) {
    T64_CHECK_INT(t64_machine_bus_write(machine, address + (uint32_t)index,
                                        1u, bytes[index]),
                  T64_STATUS_OK);
  }
}

static size_t count_events(const t64_machine *machine, uint32_t device,
                           uint32_t kind) {
  size_t count = 0u;
  size_t i;

  for (i = 0u; i < machine->queue_count; i++) {
    const t64_event *event =
        &machine->events[(machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];

    if (event->device == device && event->kind == kind) count++;
  }
  return count;
}

/* Admit, run the boot code to tick 2,000, write the poll frame, start WR64B,
 * run to 5,000, start RD64B at 0x3900 and run 8,000 more ticks: the read is
 * active and its execute event is still pending. */
static t64_machine *build_s1(const char *profile_id) {
  t64_machine *machine = create_admitted(profile_id);

  if (machine == NULL) return NULL;
  T64_CHECK(advance_to(machine, 2000u));
  bus_write_bytes(machine, UINT32_C(0x3800), poll_frame, 64u);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_DRAM_ADDR, 4u, 0x3800u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_WR64B, 4u, PIF_ADDRESS),
                T64_STATUS_OK);
  T64_CHECK(advance_to(machine, 5000u));
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_DRAM_ADDR, 4u, 0x3900u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_RD64B, 4u, PIF_ADDRESS),
                T64_STATUS_OK);
  T64_CHECK(advance_to(machine, 13000u));
  /* Reached: the read is in flight with the execute event pending, both SI
   * events and one CPU step event are queued. */
  T64_CHECK(machine->si.active_valid);
  T64_CHECK(machine->si.execute_pending);
  T64_CHECK_INT(machine->si.active_direction, T64_SI_DIRECTION_RD64B);
  T64_CHECK_INT(count_events(machine, T64_SI_DEVICE, T64_SI_EVENT_EXECUTE),
                1u);
  T64_CHECK_INT(count_events(machine, T64_SI_DEVICE, T64_SI_EVENT_COMPLETE),
                1u);
  T64_CHECK_INT(count_events(machine, T64_DEVICE_CPU, T64_CPU_EVENT_STEP), 1u);
  return machine;
}

/* ---- container access with literal offsets (reference decoder values) ---- */

#define D_TOTAL 0x010u
#define D_COUNT 0x018u
#define D_TICK 0x068u
#define D_DECLARED 0x070u

static uint32_t rd32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint64_t rd64(const uint8_t *p) {
  return ((uint64_t)rd32(p) << 32) | (uint64_t)rd32(p + 4);
}

/* One export by size query then copy-out; the caller frees the blob. */
static uint8_t *export_blob(const t64_machine *machine, size_t *length_out) {
  size_t required = 0u;
  size_t written = 0u;
  uint8_t *blob;
  t64_status status;

  *length_out = 0u;
  status = t64_machine_state_export(machine, NULL, 0u, &required);
  T64_CHECK_INT(status, T64_STATUS_BUFFER_TOO_SMALL);
  T64_CHECK(required > 0u);
  if (status != T64_STATUS_BUFFER_TOO_SMALL || required == 0u) return NULL;
  blob = (uint8_t *)malloc(required);
  if (blob == NULL) {
    T64_CHECK(false);
    return NULL;
  }
  status = t64_machine_state_export(machine, blob, required, &written);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(written, required);
  if (status != T64_STATUS_OK || written != required) {
    free(blob);
    return NULL;
  }
  *length_out = written;
  return blob;
}

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

static bool drain_output(t64_machine *machine, uint8_t bytes[4096],
                         size_t *copied_out) {
  t64_output_drain_result result;
  const t64_status status =
      t64_machine_output_drain(machine, bytes, 4096u, &result);

  T64_CHECK_INT(status, T64_STATUS_OK);
  *copied_out = result.copied_bytes;
  return status == T64_STATUS_OK;
}

/* Advance both machines to the same absolute bound in steps of `step` ticks
 * (a step of 0 is one call per slice). */
static bool advance_pair(t64_machine *left, t64_machine *right,
                         uint64_t target, uint64_t step) {
  while (left->tick < target) {
    const uint64_t next = step == 0u || target - left->tick <= step
                              ? target
                              : left->tick + step;

    if (!advance_to(left, next) || !advance_to(right, next)) return false;
  }
  return left->tick == right->tick;
}

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

#define SAVE_E4K_ID SI_STEM_ID "-e4k"
#define SAVE_S32K_ID SI_STEM_ID "-s32k"

/* Reference decoder offsets (literal copies of core/tests/test_state_export.c) */
#define D_MAGIC 0x000u
#define D_CONTAINER_VERSION 0x008u
#define D_STATE_VERSION 0x00Cu
#define D_FLAGS 0x01Cu
#define D_PROFILE 0x020u
#define D_CONTENT_SHA 0x040u
#define D_CONTENT_LEN 0x060u
#define D_CANON_OFF 0x090u
#define D_CANON_LEN 0x098u
#define D_BUILD 0x0A0u
#define D_TABLE 0x120u
#define D_ENTRY 56u
#define E_ID 0u
#define E_FLAGS 4u
#define E_OFF 8u
#define E_LEN 16u
#define E_HASH 24u
#define D_OBSERVER_BYTES 40u
#define D_TRAILER_BYTES 32u
#define D_DOMAIN "t64.state.si.25"
#define D_DOMAIN_BYTES 15u

static void put32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)v;
}

static void put64(uint8_t *p, uint64_t v) {
  put32(p, (uint32_t)(v >> 32));
  put32(p + 4, (uint32_t)v);
}

static void sha_of(const uint8_t *bytes, size_t length,
                   uint8_t out[T64_DIGEST_SIZE]) {
  t64_sha256_ctx context;

  t64_sha256_init(&context);
  t64_sha256_update(&context, bytes, length);
  t64_sha256_final(&context, out);
}

/* The independent re-signing helper. Every field it recomputes is written out
 * here from the container definition (docs/spec/state-container.md), never by
 * calling a production helper. */
#define RS_SECTIONS 1u
#define RS_DECLARED 2u
#define RS_TRAILER 4u
#define RS_ALL (RS_SECTIONS | RS_DECLARED | RS_TRAILER)

static void blob_resign(uint8_t *blob, size_t length, unsigned flags) {
  const uint32_t count = rd32(blob + 0x018u);
  uint32_t i;

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
    t64_sha256_update(&context, (const uint8_t *)D_DOMAIN, D_DOMAIN_BYTES);
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

/* Re-sign the table hash of one section only (a body edit that keeps every
 * length leaves the other entry hashes valid). */
static void blob_resign_entry(uint8_t *blob, size_t length, uint32_t id) {
  const uint32_t count = rd32(blob + 0x018u);
  uint32_t i;

  for (i = 0u; i < count && i < 32u; i++) {
    uint8_t *entry = blob + D_TABLE + (size_t)D_ENTRY * i;
    const uint64_t offset = rd64(entry + E_OFF);
    const uint64_t body = rd64(entry + E_LEN);

    if (rd32(entry + E_ID) == id && offset <= length && body <= length - offset) {
      sha_of(blob + offset, (size_t)body, entry + E_HASH);
    }
  }
}

/* True when every integrity field of the blob agrees with its content, from
 * the reference definition: the entry hashes, the declared digest and the
 * trailer. The accepted baseline must satisfy it; a field row asserts it
 * before the call so the rule, not an integrity check, is the first failure. */
static bool blob_consistent(const uint8_t *blob, size_t length) {
  uint8_t *copy = (uint8_t *)malloc(length);
  bool same;

  if (copy == NULL) return false;
  (void)memcpy(copy, blob, length);
  blob_resign(copy, length, RS_ALL);
  same = memcmp(copy, blob, length) == 0;
  free(copy);
  return same;
}

/* Decoded sections for edits that change a length: each body is a heap copy. */
typedef struct {
  uint32_t id;
  uint8_t *body;
  size_t length;
} tsec;

typedef struct {
  tsec section[32];
  size_t count; /* the observer section included, last */
  uint8_t header[D_TABLE];
} tblob;

static void tblob_free(tblob *t) {
  size_t i;

  for (i = 0u; i < t->count; i++) free(t->section[i].body);
  t->count = 0u;
}

static bool tblob_decode(const uint8_t *blob, size_t length, tblob *t) {
  const uint32_t count = rd32(blob + 0x018u);
  uint32_t i;

  (void)memset(t, 0, sizeof *t);
  if (count == 0u || count > 32u) return false;
  (void)memcpy(t->header, blob, D_TABLE);
  for (i = 0u; i < count; i++) {
    const uint8_t *entry = blob + D_TABLE + (size_t)D_ENTRY * i;
    const uint64_t offset = rd64(entry + E_OFF);
    const uint64_t body = rd64(entry + E_LEN);

    if (offset > length || body > length - offset) {
      tblob_free(t);
      return false;
    }
    t->section[i].id = rd32(entry + E_ID);
    t->section[i].length = (size_t)body;
    t->section[i].body = (uint8_t *)malloc(body == 0u ? 1u : (size_t)body);
    if (t->section[i].body == NULL) {
      t->count = i;
      tblob_free(t);
      return false;
    }
    (void)memcpy(t->section[i].body, blob + offset, (size_t)body);
    t->count = i + 1u;
  }
  return true;
}

/* Rebuild a whole container from the decoded sections with the layout of the
 * definition: header, table, domain bytes, id and body per canonical section,
 * the observer body, the trailer. Every hash is recomputed. */
static uint8_t *tblob_encode(const tblob *t, size_t *length_out) {
  size_t canonical = D_DOMAIN_BYTES;
  size_t total;
  size_t i;
  size_t cursor;
  uint8_t *blob;

  for (i = 0u; i + 1u < t->count; i++) canonical += 4u + t->section[i].length;
  total = (size_t)D_TABLE + (size_t)D_ENTRY * t->count + canonical +
          D_OBSERVER_BYTES + D_TRAILER_BYTES;
  blob = (uint8_t *)calloc(total, 1u);
  if (blob == NULL) return NULL;
  (void)memcpy(blob, t->header, D_TABLE);
  put64(blob + 0x010u, total);
  put32(blob + 0x018u, (uint32_t)t->count);
  put64(blob + D_CANON_OFF, (uint64_t)D_TABLE + (uint64_t)D_ENTRY * t->count);
  put64(blob + D_CANON_LEN, canonical);
  cursor = (size_t)(D_TABLE + (size_t)D_ENTRY * t->count);
  (void)memcpy(blob + cursor, D_DOMAIN, D_DOMAIN_BYTES);
  cursor += D_DOMAIN_BYTES;
  for (i = 0u; i < t->count; i++) {
    uint8_t *entry = blob + D_TABLE + (size_t)D_ENTRY * i;
    const bool observer = i + 1u == t->count;

    if (!observer) {
      put32(blob + cursor, t->section[i].id);
      cursor += 4u;
    }
    put32(entry + E_ID, t->section[i].id);
    put32(entry + E_FLAGS, observer ? 1u : 0u);
    put64(entry + E_OFF, cursor);
    put64(entry + E_LEN, t->section[i].length);
    (void)memcpy(blob + cursor, t->section[i].body, t->section[i].length);
    cursor += t->section[i].length;
  }
  *length_out = total;
  blob_resign(blob, total, RS_ALL);
  return blob;
}

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


/* The case roundtrip: the T1 round trip of the S1 state, then the
 * continuation of the source and of the peer under one-shot and the 1, 7 and
 * 31 tick partitions. */
static void case_roundtrip_all(void) {
  case_roundtrip();
  case_continuation_roundtrip();
}

/* ---- dispatch ---------------------------------------------------------- */

int main(int argc, char **argv) {
  static const char *const cases[] = {
      "roundtrip",          "handler-table-ledger", "binding-refusals",
      "container-refusals", "field-range-refusals", "invariant-accepts",
      "invariant-refusals", "scratch-storage",      "refusal-untouched"};
  static const char *const suites[] = {"import",   "import",   "refusals",
                                       "refusals", "refusals", "refusals",
                                       "refusals", "refusals", "refusals"};
  static void (*const runners[])(void) = {
      case_roundtrip_all,        case_handler_table_ledger,
      case_binding_refusals,     case_container_refusals,
      case_field_range_refusals, case_invariant_accepts,
      case_invariant_refusals,   case_scratch_storage,
      case_refusal_untouched};
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
  return T64_TEST_FINISH("unit.core.state.import");
}
