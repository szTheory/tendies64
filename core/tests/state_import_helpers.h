/*
 * Test-side helpers shared by the state import tests and the fuzz campaign of
 * plan 09-09.1 (extracted from core/tests/test_state_import.c without a
 * behavior change). Every function is static inline so a translation unit that
 * does not use one compiles clean under -Werror. The container offsets are the
 * literal copies of the reference decoder of core/tests/test_state_export.c and
 * the re-signing helper is written from the container definition, never by
 * calling a production helper. Each translation unit has its own allocator
 * counters and its own T64_CHECK counters.
 */
#ifndef T64_STATE_IMPORT_HELPERS_H
#define T64_STATE_IMPORT_HELPERS_H

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

static inline bool test_allocate(void *context, size_t size, size_t alignment,
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

static inline void test_release(void *context, t64_host_storage storage) {
  (void)context;
  release_calls++;
  free(storage.data);
}

static const t64_host_allocator allocator =
    T64_HOST_ALLOCATOR_INIT(test_allocate, test_release, NULL);

/* Poisons an import report before a call: the INIT form first, so a leading
 * struct_size of the flip stays valid, then a non-zero value in every other
 * member by name. A byte-wise fill would also clobber a leading struct_size,
 * and a plain INIT assignment would let a callee that writes zeros or leaves a
 * member alone pass the checks of the fields it must overwrite. */
static inline void poison_import_report(t64_state_import_report *report) {
  *report = (t64_state_import_report)T64_STATE_IMPORT_REPORT_INIT;
  report->status = UINT32_C(0xa5a5a5a5);
  report->reason = UINT32_C(0xa5a5a5a5);
  report->section_id = UINT32_C(0xa5a5a5a5);
  report->rule = UINT32_C(0xa5a5a5a5);
  report->position = UINT64_C(0xa5a5a5a5a5a5a5a5);
}

/* True when every member still carries the poison of poison_import_report. */
static inline bool import_report_is_poisoned(
    const t64_state_import_report *report) {
  return report->status == UINT32_C(0xa5a5a5a5) &&
         report->reason == UINT32_C(0xa5a5a5a5) &&
         report->section_id == UINT32_C(0xa5a5a5a5) &&
         report->rule == UINT32_C(0xa5a5a5a5) &&
         report->position == UINT64_C(0xa5a5a5a5a5a5a5a5);
}

static inline bool read_openipl3_asset(uint8_t bytes[IPL3_ASSET_BYTES]) {
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

static inline t64_machine *create_admitted(const char *profile_id) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t asset[IPL3_ASSET_BYTES];
  t64_status status;

  T64_CHECK_INT(t64_profile_lookup(profile_id, &profile), T64_STATUS_OK);
  if (profile == NULL) return NULL;
  T64_CHECK_INT(t64_machine_create(
                    &(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(
                        &allocator, profile),
                    &machine),
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
static inline bool advance_to(t64_machine *machine, uint64_t tick) {
  while (machine->tick < tick) {
    const uint64_t step = tick - machine->tick > ADVANCE_SLICE_TICKS
                              ? machine->tick + ADVANCE_SLICE_TICKS
                              : tick;
    t64_stop_request request = T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, step);
    t64_stop_result stop = T64_STOP_RESULT_INIT;
    const t64_status status = t64_machine_advance(machine, &request, &stop);

    T64_CHECK_INT(status, T64_STATUS_OK);
    T64_CHECK_INT(stop.reason, T64_STOP_REASON_BOUND);
    if (status != T64_STATUS_OK || stop.reason != T64_STOP_REASON_BOUND) {
      return false;
    }
  }
  return machine->tick == tick;
}

static inline bool digest_of(const t64_machine *machine,
                      uint8_t digest[T64_DIGEST_SIZE]) {
  const t64_status status = t64_machine_digest(machine, digest);

  T64_CHECK_INT(status, T64_STATUS_OK);
  return status == T64_STATUS_OK;
}

static inline bool digests_equal(const t64_machine *left, const t64_machine *right) {
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

static inline void bus_write_bytes(t64_machine *machine, uint32_t address,
                            const uint8_t *bytes, size_t count) {
  size_t index;

  for (index = 0u; index < count; index++) {
    T64_CHECK_INT(t64_machine_bus_write(machine, address + (uint32_t)index,
                                        1u, bytes[index]),
                  T64_STATUS_OK);
  }
}

static inline size_t count_events(const t64_machine *machine, uint32_t device,
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
static inline t64_machine *build_s1(const char *profile_id) {
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

static inline uint32_t rd32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static inline uint64_t rd64(const uint8_t *p) {
  return ((uint64_t)rd32(p) << 32) | (uint64_t)rd32(p + 4);
}

/* One export by size query then copy-out; the caller frees the blob. */
static inline uint8_t *export_blob(const t64_machine *machine, size_t *length_out) {
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

static inline bool drain_output(t64_machine *machine, uint8_t bytes[4096],
                         size_t *copied_out) {
  t64_output_drain_result result = T64_OUTPUT_DRAIN_RESULT_INIT;
  const t64_status status =
      t64_machine_output_drain(machine, bytes, 4096u, &result);

  T64_CHECK_INT(status, T64_STATUS_OK);
  *copied_out = result.copied_bytes;
  return status == T64_STATUS_OK;
}

/* Advance both machines to the same absolute bound in steps of `step` ticks
 * (a step of 0 is one call per slice). */
static inline bool advance_pair(t64_machine *left, t64_machine *right,
                         uint64_t target, uint64_t step) {
  while (left->tick < target) {
    const uint64_t next = step == 0u || target - left->tick <= step
                              ? target
                              : left->tick + step;

    if (!advance_to(left, next) || !advance_to(right, next)) return false;
  }
  return left->tick == right->tick;
}

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

static inline void put32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)v;
}

static inline void put64(uint8_t *p, uint64_t v) {
  put32(p, (uint32_t)(v >> 32));
  put32(p + 4, (uint32_t)v);
}

static inline void sha_of(const uint8_t *bytes, size_t length,
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

static inline void blob_resign(uint8_t *blob, size_t length, unsigned flags) {
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
static inline void blob_resign_entry(uint8_t *blob, size_t length, uint32_t id) {
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
static inline bool blob_consistent(const uint8_t *blob, size_t length) {
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

static inline void tblob_free(tblob *t) {
  size_t i;

  for (i = 0u; i < t->count; i++) free(t->section[i].body);
  t->count = 0u;
}

static inline bool tblob_decode(const uint8_t *blob, size_t length, tblob *t) {
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
static inline uint8_t *tblob_encode(const tblob *t, size_t *length_out) {
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

/* ---- whole-machine snapshot (moved from test_state_import.c) ---- */

typedef struct {
  uint8_t *machine_bytes;
  size_t machine_size;
  uint8_t *video_bytes;
  size_t video_size;
  uint8_t *audio_bytes;
  size_t audio_size;
  uint8_t *image_bytes;
  size_t image_size;
  size_t trace_count;
  uint64_t trace_drops;
  size_t queue_count;
  uint64_t next_sequence;
  uint64_t tick;
  size_t allocs;
  size_t releases;
} snapshot;

static inline uint8_t *copy_region(const void *data, size_t size) {
  uint8_t *copy;

  if (data == NULL || size == 0u) return NULL;
  copy = (uint8_t *)malloc(size);
  if (copy != NULL) (void)memcpy(copy, data, size);
  return copy;
}

static inline void snapshot_take(const t64_machine *machine, snapshot *s) {
  (void)memset(s, 0, sizeof *s);
  s->machine_size = machine->machine_storage.capacity;
  s->machine_bytes = copy_region(machine->machine_storage.data, s->machine_size);
  s->video_size = machine->video_storage.capacity;
  s->video_bytes = copy_region(machine->video_storage.data, s->video_size);
  s->audio_size = machine->audio_storage.capacity;
  s->audio_bytes = copy_region(machine->audio_storage.data, s->audio_size);
  /* The digest is a pure function of the machine allocation, the packet storages
   * and the admitted image, all compared byte for byte below, so no digest pass
   * over the 6.3 MB state is needed (plan 09-09.1 cost control: two of the six
   * hashing passes of every refusal row). */
  s->image_size = machine->image_storage.capacity;
  s->image_bytes = copy_region(machine->image_storage.data, s->image_size);
  s->trace_count = machine->trace_count;
  s->trace_drops = machine->trace_drop_count;
  s->queue_count = machine->queue_count;
  s->next_sequence = machine->next_sequence;
  s->tick = machine->tick;
  s->allocs = allocate_calls;
  s->releases = release_calls;
}

static inline void snapshot_free(snapshot *s) {
  free(s->machine_bytes);
  free(s->video_bytes);
  free(s->audio_bytes);
  free(s->image_bytes);
}

/* Whole-machine comparison: the machine allocation (the structure, RDRAM, the
 * coverage tables and the save bytes), both packet storages, the image, the
 * trace observations, the queue, the tick and the allocator counters. */
static inline bool snapshot_matches(const t64_machine *machine, const snapshot *s) {
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
  same = same && (s->image_bytes == NULL ||
                  memcmp(machine->image_storage.data, s->image_bytes,
                         s->image_size) == 0);
  same = same && machine->trace_count == s->trace_count &&
         machine->trace_drop_count == s->trace_drops &&
         machine->queue_count == s->queue_count &&
         machine->next_sequence == s->next_sequence &&
         machine->tick == s->tick && allocate_calls == s->allocs &&
         release_calls == s->releases;
  return same;
}

/* ---- the closed outcome table and the fuzz campaign interface (09-09.1) --- */

/* The reason to status table, authored from the "Statuses and reasons" table
 * of docs/spec/state-container.md (not from the importer). True when the pair
 * is one the contract allows. */
static inline bool state_reason_status_consistent(uint32_t reason,
                                                  uint32_t status) {
  switch (reason) {
  case 1u: case 2u: case 3u: case 4u:
    return status == (uint32_t)T64_STATUS_INVALID_ARGUMENT;
  case 5u:
    return status == (uint32_t)T64_STATUS_NOT_READY;
  case 6u: case 7u: case 8u: case 9u: case 10u: case 11u: case 18u: case 19u:
  case 20u: case 21u: case 22u: case 23u: case 25u:
    return status == 26u;
  case 12u: case 24u: case 39u:
    return status == 27u;
  case 13u:
    return status == 28u;
  case 14u:
    return status == 29u;
  case 15u: case 16u:
    return status == 30u;
  case 17u:
    return status == 31u;
  case 26u: case 27u: case 28u: case 29u: case 40u:
    return status == 32u;
  case 30u: case 31u: case 32u: case 33u: case 34u: case 35u: case 36u:
  case 37u: case 38u:
    return status == 33u;
  default:
    return false;
  }
}

typedef struct {
  uint64_t iterations;
  uint64_t refusals;
  uint64_t accepted;
  uint64_t liveness_stops;
  uint64_t violations;
  /* Refusals that reached a section reader or a later check (reason
   * FIELD_RANGE and above); the others stopped at the container level (plan
   * 09-10 review L2 WR-04). */
  uint64_t field_level;
  uint64_t by_reason[48];
} state_fuzz_stats;

/* A window of a section body where the mutators aim: the rule-bearing fields of
 * a section whose bulk (RDRAM, SP memory, the coverage arrays) has no rule. Set
 * by the caller before a campaign or a sweep; without windows the mutators pick
 * a section other than RDRAM and SP memory uniformly. */
typedef struct {
  uint32_t section;
  size_t offset;
  size_t length;
} state_fuzz_window;

void state_fuzz_set_windows(const state_fuzz_window *windows, size_t count);

/* One import with the whole contract checked (core/tests/fuzz_state_import.c);
 * returns the peer to use next. */
t64_machine *state_fuzz_one(t64_machine *peer, const uint8_t *bytes, size_t n,
                            uint64_t iteration, state_fuzz_stats *stats);

/* Defined in core/tests/fuzz_state_import.c; holds no main. */
void state_fuzz_campaign(const uint8_t *blob, size_t length,
                         uint64_t iterations, uint64_t seed,
                         state_fuzz_stats *stats);

#endif
