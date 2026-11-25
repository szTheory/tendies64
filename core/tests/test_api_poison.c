/*
 * Defect class and entry guards of the public API (plan 11-03, API-02).
 *
 * White-box: this file includes core/t64_internal.h like test_k1_refusal.c so
 * it can force an internal invariant violation (t64_test_force_defect) and
 * corrupt members that the public surface cannot reach. The installed
 * consumer exercises the same postconditions through the shipped library with
 * a black-box trigger (plan 11-20).
 *
 *   poison           a forced defect poisons the instance; every guarded
 *                    entry then answers T64_STATUS_POISONED with every output
 *                    byte equal to its 0xA5 sentinel and the canonical state
 *                    unchanged; the poison query answers; destroy releases
 *                    every host allocation (allocation count equals release
 *                    count, zero outstanding)
 *   refusals-never-poison
 *                    forty refused calls of distinct shapes (NULL pointers,
 *                    bad sizes, reversed limits, past ticks, wrong save kinds,
 *                    hostile state blobs) leave the poison query at 0
 *   reentrant        a host allocator callback that calls all 23 guarded
 *                    entries on the same instance, during admission (the ROM
 *                    candidate allocation of core/t64_rom.c included),
 *                    replacement and destruction, gets T64_STATUS_REENTRANT
 *                    with every output untouched; admission then completes OK
 *                    and the digest equals a run without the callback calls
 *   bus-carve-out    t64_machine_bus_write is outside the refusal-is-a-no-op
 *                    domain: a write a device refuses changes exactly the
 *                    records the header names and nothing else; address,
 *                    width, alignment and range refusals change no byte
 *   null-machine     every machine-taking entry with a NULL machine answers
 *                    T64_STATUS_INVALID_ARGUMENT and leaves every sentinel
 *                    output untouched; destroy(NULL) is a no-op
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
#define SI_STEM "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1"
#define SAVE_PROFILE_ID SI_STEM "-e4k"
#define LEGACY_PROFILE_ID \
  "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1"
#define ENTRY_COUNT 24u
#define MAX_LIVE 16u
#define MIN_REFUSED_SHAPES 40u

/* A counting allocator that remembers every live block, so a test can state
 * "allocation count equals release count, zero outstanding" exactly. */
typedef struct {
  size_t allocations;
  size_t releases;
  size_t unknown_releases;
  void *live[MAX_LIVE];
  size_t live_count;
  /* Reentrancy hook: while hook_machine is set, every callback calls all 23
   * guarded entries on that instance and records what came back. */
  t64_machine *hook_machine;
  bool in_hook;
  unsigned hook_calls;
  unsigned hook_allocate_calls;
  unsigned hook_release_calls;
  unsigned hook_entries;
  unsigned hook_reentrant;
  unsigned hook_untouched;
  bool hook_saw_rom_size;
} counting_allocator;

static void run_reentrancy_hook(counting_allocator *state, bool is_allocate,
                                size_t size);

static bool counting_allocate(void *context, size_t size, size_t alignment,
                              t64_host_storage *storage_out) {
  counting_allocator *state = (counting_allocator *)context;
  void *memory;

  if (state == NULL || storage_out == NULL || size == 0u || alignment == 0u ||
      state->live_count >= MAX_LIVE) {
    return false;
  }
  run_reentrancy_hook(state, true, size);
  memory = malloc(size);
  if (memory == NULL || (uintptr_t)memory % alignment != 0u) {
    free(memory);
    return false;
  }
  state->allocations++;
  state->live[state->live_count++] = memory;
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  return true;
}

static void counting_release(void *context, t64_host_storage storage) {
  counting_allocator *state = (counting_allocator *)context;
  size_t index;

  if (state == NULL) {
    return;
  }
  run_reentrancy_hook(state, false, storage.capacity);
  state->releases++;
  for (index = 0u; index < state->live_count; index++) {
    if (state->live[index] == storage.data) {
      state->live[index] = state->live[state->live_count - 1u];
      state->live_count--;
      free(storage.data);
      return;
    }
  }
  state->unknown_releases++;
}

typedef struct {
  counting_allocator counts;
  t64_host_allocator allocator;
  t64_machine *machine;
} instance;

static bool read_openipl3_asset(uint8_t bytes[IPL3_ASSET_BYTES]) {
  const char *path = getenv("T64_TEST_IPL3_PATH");
  FILE *file;
  size_t length;
  int extra;
  int read_error;
  int close_error;

  if (path == NULL || path[0] == '\0') return false;
  file = fopen(path, "rb");
  if (file == NULL) return false;
  length = fread(bytes, 1u, IPL3_ASSET_BYTES, file);
  extra = fgetc(file);
  read_error = ferror(file);
  close_error = fclose(file);
  return read_error == 0 && close_error == 0 && length == IPL3_ASSET_BYTES &&
         extra == EOF;
}

static uint8_t asset_bytes[IPL3_ASSET_BYTES];

/* Create an instance for the id; admit the openIPL3 asset when asked. */
static bool make_instance(instance *item, const char *profile_id,
                          bool admit) {
  const t64_profile_descriptor *profile = NULL;

  (void)memset(item, 0, sizeof *item);
  item->allocator.allocate = counting_allocate;
  item->allocator.release = counting_release;
  item->allocator.context = &item->counts;
  T64_CHECK_INT(t64_profile_lookup(profile_id, &profile), T64_STATUS_OK);
  if (profile == NULL) return false;
  T64_CHECK_INT(t64_machine_create(&item->allocator, profile, &item->machine),
                T64_STATUS_OK);
  if (item->machine == NULL) return false;
  if (admit) {
    t64_status status = t64_machine_admit_rom(item->machine, asset_bytes,
                                              sizeof asset_bytes);

    T64_CHECK_INT(status, T64_STATUS_OK);
    if (status != T64_STATUS_OK) return false;
  }
  return true;
}

/* Destroy and prove the host allocator saw every block returned. */
static void destroy_and_check(instance *item) {
  t64_machine_destroy(item->machine);
  item->machine = NULL;
  T64_CHECK(item->counts.allocations > 0u);
  T64_CHECK_INT(item->counts.allocations, item->counts.releases);
  T64_CHECK_INT(item->counts.live_count, 0);
  T64_CHECK_INT(item->counts.unknown_releases, 0);
}

/* SHA-256 of the canonical state stream (the same bytes t64_machine_digest
 * hashes, through the internal stream so it works on a poisoned instance). */
static bool state_hash(const t64_machine *machine,
                       uint8_t out[T64_DIGEST_SIZE]) {
  t64_sha256_ctx context;
  t64_digest_sink sink;

  (void)memset(&sink, 0, sizeof sink);
  t64_sha256_init(&context);
  sink.hash = &context;
  if (t64_digest_stream(machine, &sink) != T64_STATUS_OK) return false;
  t64_sha256_final(&context, out);
  return true;
}

static bool not_poisoned(const t64_machine *machine) {
  uint32_t poisoned = 99u;

  return t64_machine_is_poisoned(machine, &poisoned) == T64_STATUS_OK &&
         poisoned == 0u;
}

static bool is_poisoned(const t64_machine *machine) {
  uint32_t poisoned = 99u;

  return t64_machine_is_poisoned(machine, &poisoned) == T64_STATUS_OK &&
         poisoned == 1u;
}

/* Every out parameter of the 23 machine-taking entries, one scratch record.
 * The record is filled with 0xA5 before a call and compared byte for byte
 * afterwards (padding included), so any write by a guarded refusal fails. */
typedef struct {
  t64_stop_result stop_result;
  uint8_t digest[T64_DIGEST_SIZE];
  size_t size;
  uint8_t header[T64_ROM_HEADER_BYTES];
  size_t header_length;
  const t64_profile_descriptor *profile;
  uint8_t export_bytes[64];
  size_t export_length;
  t64_state_import_report report;
  t64_cpu_stop_detail detail;
  uint8_t drain_bytes[64];
  t64_output_drain_result drain;
  t64_video_packet_info video_info;
  uint8_t video_bytes[64];
  size_t video_length;
  t64_save_info save_info;
  uint8_t save_bytes[64];
  size_t save_length;
  t64_audio_packet_info audio_info;
  uint8_t audio_bytes[64];
  size_t audio_length;
  uint32_t bus_value;
  t64_trace_record records[2];
  size_t trace_count;
  uint64_t trace_dropped;
  bool trace_saturated;
} scratch;

static const char *const entry_names[ENTRY_COUNT] = {
    "admit_rom",     "advance",        "content_digest", "rom_size",
    "rom_header",    "profile",        "digest",         "state_export",
    "state_import",  "cpu_stop_detail", "output_drain",  "video_info",
    "video_copy",    "save_info",      "save_export",    "save_import",
    "audio_info",    "audio_copy",     "queue_input",    "bus_read",
    "bus_write",     "trace_set_enabled", "trace_drain", "reset"};

/* The save device kind of the profile under test; a plain value so an entry
 * can be called with a NULL machine. */
static uint32_t entry_save_kind = 1u;

/* Call guarded entry number `entry` once with arguments that would be valid
 * on a healthy admitted instance. */
static t64_status call_entry(unsigned entry, t64_machine *machine,
                             scratch *s) {
  t64_stop_request request;

  request.kind = T64_STOP_GUEST_TICK;
  request.value = UINT64_C(100000);
  switch (entry) {
  case 0:
    return t64_machine_admit_rom(machine, asset_bytes, sizeof asset_bytes);
  case 1:
    return t64_machine_advance(machine, &request, &s->stop_result);
  case 2:
    return t64_machine_content_digest(machine, s->digest);
  case 3:
    return t64_machine_rom_size(machine, &s->size);
  case 4:
    return t64_machine_rom_header(machine, s->header, sizeof s->header,
                                  &s->header_length);
  case 5:
    return t64_machine_profile(machine, &s->profile);
  case 6:
    return t64_machine_digest(machine, s->digest);
  case 7:
    return t64_machine_state_export(machine, s->export_bytes,
                                    sizeof s->export_bytes,
                                    &s->export_length);
  case 8:
    return t64_machine_state_import(machine, asset_bytes, 64u, 0u,
                                    &s->report);
  case 9:
    return t64_machine_cpu_stop_detail(machine, &s->detail);
  case 10:
    return t64_machine_output_drain(machine, s->drain_bytes,
                                    sizeof s->drain_bytes, &s->drain);
  case 11:
    return t64_video_info(machine, &s->video_info);
  case 12:
    return t64_video_copy(machine, s->video_bytes, sizeof s->video_bytes,
                          &s->video_length);
  case 13:
    return t64_machine_save_info(machine, entry_save_kind,
                                 &s->save_info);
  case 14:
    return t64_machine_save_export(machine, entry_save_kind, 1u,
                                   s->save_bytes, sizeof s->save_bytes,
                                   &s->save_length);
  case 15:
    return t64_machine_save_import(machine, entry_save_kind, 1u,
                                   asset_bytes, 512u);
  case 16:
    return t64_audio_info(machine, &s->audio_info);
  case 17:
    return t64_audio_copy(machine, s->audio_bytes, sizeof s->audio_bytes,
                          &s->audio_length);
  case 18:
    return t64_machine_queue_input(machine, UINT64_C(200000), 0u, 0u, 0, 0);
  case 19:
    return t64_machine_bus_read(machine, UINT32_C(0x100), 4u, &s->bus_value);
  case 20:
    return t64_machine_bus_write(machine, UINT32_C(0x100), 4u,
                                 UINT32_C(0xdeadbeef));
  case 21:
    return t64_machine_trace_set_enabled(machine, true);
  case 22:
    return t64_machine_trace_drain(machine, s->records, 2u, &s->trace_count,
                                   &s->trace_dropped, &s->trace_saturated);
  case 23:
    return t64_machine_reset(machine);
  default:
    return T64_STATUS_INVALID_ARGUMENT;
  }
}

static void run_reentrancy_hook(counting_allocator *state, bool is_allocate,
                                size_t size) {
  unsigned entry;

  if (state->hook_machine == NULL || state->in_hook) {
    return;
  }
  state->in_hook = true;
  state->hook_calls++;
  if (is_allocate) {
    state->hook_allocate_calls++;
    if (size == IPL3_ASSET_BYTES) {
      state->hook_saw_rom_size = true;
    }
  } else {
    state->hook_release_calls++;
  }
  for (entry = 0u; entry < ENTRY_COUNT; entry++) {
    scratch actual;
    scratch expected;
    t64_status status;

    (void)memset(&actual, 0xa5, sizeof actual);
    (void)memset(&expected, 0xa5, sizeof expected);
    status = call_entry(entry, state->hook_machine, &actual);
    state->hook_entries++;
    if (status == T64_STATUS_REENTRANT) {
      state->hook_reentrant++;
    }
    if (memcmp(&actual, &expected, sizeof actual) == 0) {
      state->hook_untouched++;
    }
  }
  state->in_hook = false;
}

/* A forced defect poisons the instance. Every guarded entry then answers
 * POISONED, every output byte stays at its sentinel, the canonical state does
 * not move, the query answers, and destroy releases everything. */
static void case_poison(void) {
  instance item;
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  uint32_t poisoned_out = UINT32_C(0xa5a5a5a5);
  unsigned entry;

  if (!make_instance(&item, SAVE_PROFILE_ID, true)) return;
  entry_save_kind = item.machine->profile.save_kind;
  T64_CHECK(not_poisoned(item.machine));
  T64_CHECK(state_hash(item.machine, before));
  t64_test_force_defect(item.machine);
  T64_CHECK(is_poisoned(item.machine));
  /* The flag is outside the digest and the state container. */
  T64_CHECK(state_hash(item.machine, after));
  T64_CHECK(memcmp(before, after, sizeof before) == 0);

  /* The poison query itself: NULL machine, NULL output and a span inside the
   * instance are refused with the output untouched. */
  T64_CHECK_INT(t64_machine_is_poisoned(NULL, &poisoned_out),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK_INT(poisoned_out, UINT32_C(0xa5a5a5a5));
  T64_CHECK_INT(t64_machine_is_poisoned(item.machine, NULL),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK_INT(
      t64_machine_is_poisoned(item.machine,
                              (uint32_t *)(void *)&item.machine->tick),
      T64_STATUS_INVALID_ARGUMENT);

  for (entry = 0u; entry < ENTRY_COUNT; entry++) {
    scratch actual;
    scratch expected;
    t64_status status;

    (void)memset(&actual, 0xa5, sizeof actual);
    (void)memset(&expected, 0xa5, sizeof expected);
    status = call_entry(entry, item.machine, &actual);
    if (status != T64_STATUS_POISONED ||
        memcmp(&actual, &expected, sizeof actual) != 0) {
      fprintf(stderr, "poisoned entry %s: status %u\n", entry_names[entry],
              (unsigned)status);
    }
    T64_CHECK_INT(status, T64_STATUS_POISONED);
    T64_CHECK(memcmp(&actual, &expected, sizeof actual) == 0);
    T64_CHECK(is_poisoned(item.machine));
  }
  T64_CHECK(state_hash(item.machine, after));
  T64_CHECK(memcmp(before, after, sizeof before) == 0);
  T64_CHECK_INT(item.machine->trace_enabled, 0);
  destroy_and_check(&item);

  /* Destroy of a poisoned instance that never admitted an image. */
  if (!make_instance(&item, SAVE_PROFILE_ID, false)) return;
  t64_test_force_defect(item.machine);
  T64_CHECK(is_poisoned(item.machine));
  destroy_and_check(&item);

  /* A healthy instance reports 0 and 1 is sticky. */
  if (!make_instance(&item, LEGACY_PROFILE_ID, true)) return;
  T64_CHECK(not_poisoned(item.machine));
  t64_test_force_defect(item.machine);
  t64_test_force_defect(item.machine);
  T64_CHECK(is_poisoned(item.machine));
  destroy_and_check(&item);
}

/* Counts the refused shapes and fails any shape that returns OK, a defect
 * status or REENTRANT, or that poisons any instance under test. */
static unsigned shapes_run;
static const t64_machine *shape_machines[3];

#define REFUSED(call_)                                                       \
  do {                                                                       \
    const t64_status refused_status_ = (call_);                              \
    size_t machine_index_;                                                   \
    shapes_run++;                                                            \
    if (refused_status_ == T64_STATUS_OK) {                                  \
      fprintf(stderr, "shape %u at line %d returned OK\n", shapes_run,       \
              __LINE__);                                                     \
    }                                                                        \
    T64_CHECK(refused_status_ != T64_STATUS_OK);                             \
    T64_CHECK(refused_status_ != T64_STATUS_POISONED &&                      \
              refused_status_ != T64_STATUS_INTERNAL_DEFECT &&               \
              refused_status_ != T64_STATUS_REENTRANT);                      \
    for (machine_index_ = 0u; machine_index_ < 3u; machine_index_++) {       \
      T64_CHECK(not_poisoned(shape_machines[machine_index_]));               \
    }                                                                        \
  } while (0)

/* An exported container of the main instance, used to build hostile blobs. */
static uint8_t *export_blob(const t64_machine *machine, size_t *length_out) {
  size_t length = 0u;
  uint8_t *blob;

  if (t64_machine_state_export(machine, NULL, 0u, &length) !=
          T64_STATUS_BUFFER_TOO_SMALL ||
      length == 0u) {
    T64_CHECK(false);
    return NULL;
  }
  blob = (uint8_t *)malloc(length);
  if (blob == NULL) {
    T64_CHECK(false);
    return NULL;
  }
  T64_CHECK_INT(t64_machine_state_export(machine, blob, length, length_out),
                T64_STATUS_OK);
  T64_CHECK_INT(*length_out, length);
  return blob;
}

static void case_refusals_never_poison(void) {
  instance main_item;
  instance fresh_item;
  instance legacy_item;
  t64_machine *m;
  t64_machine *fresh;
  t64_machine *legacy;
  uint8_t digest[T64_DIGEST_SIZE];
  uint8_t small[64];
  size_t length = 0u;
  size_t size = 0u;
  uint8_t *blob;
  size_t blob_length = 0u;
  uint32_t save_kind;
  uint32_t value = 0u;
  const t64_profile_descriptor *profile = NULL;
  t64_stop_request request;
  t64_stop_result result;
  t64_state_import_report report;
  t64_cpu_stop_detail detail;
  t64_output_drain_result drain;
  t64_video_packet_info video_info;
  t64_audio_packet_info audio_info;
  t64_save_info save_info;
  t64_trace_record records[2];
  size_t trace_count = 0u;
  uint64_t trace_dropped = 0u;
  bool trace_saturated = false;

  shapes_run = 0u;
  if (!make_instance(&main_item, SAVE_PROFILE_ID, true)) return;
  if (!make_instance(&fresh_item, SAVE_PROFILE_ID, false)) return;
  if (!make_instance(&legacy_item, LEGACY_PROFILE_ID, true)) return;
  m = main_item.machine;
  fresh = fresh_item.machine;
  legacy = legacy_item.machine;
  shape_machines[0] = m;
  shape_machines[1] = fresh;
  shape_machines[2] = legacy;
  save_kind = m->profile.save_kind;

  /* A little guest time so tick and instruction counts are non-zero. */
  request.kind = T64_STOP_GUEST_TICK;
  request.value = UINT64_C(20000);
  T64_CHECK_INT(t64_machine_advance(m, &request, &result), T64_STATUS_OK);
  T64_CHECK(m->tick > 0u);
  T64_CHECK(m->instruction_count > 0u);
  blob = export_blob(m, &blob_length);
  if (blob == NULL) return;

  /* Admission. */
  REFUSED(t64_machine_admit_rom(m, NULL, 0u));
  REFUSED(t64_machine_admit_rom(m, asset_bytes, 0u));
  REFUSED(t64_machine_admit_rom(m, asset_bytes, IPL3_ASSET_BYTES - 1u));
  REFUSED(t64_machine_admit_rom(m, NULL, IPL3_ASSET_BYTES));
  /* Advance: NULL pointers, an unknown kind, reversed limits, no image. */
  REFUSED(t64_machine_advance(m, NULL, &result));
  REFUSED(t64_machine_advance(m, &request, NULL));
  request.kind = UINT32_C(99);
  REFUSED(t64_machine_advance(m, &request, &result));
  request.kind = T64_STOP_GUEST_TICK;
  request.value = 0u;
  REFUSED(t64_machine_advance(m, &request, &result));
  request.kind = T64_STOP_INSTRUCTION_COUNT;
  request.value = 0u;
  REFUSED(t64_machine_advance(m, &request, &result));
  request.kind = T64_STOP_GUEST_TICK;
  request.value = UINT64_C(50000);
  REFUSED(t64_machine_advance(fresh, &request, &result));
  /* Content digest, size and header. */
  REFUSED(t64_machine_content_digest(m, NULL));
  REFUSED(t64_machine_content_digest(fresh, digest));
  REFUSED(t64_machine_rom_size(m, NULL));
  REFUSED(t64_machine_rom_size(fresh, &size));
  REFUSED(t64_machine_rom_header(m, NULL, 0u, NULL));
  REFUSED(t64_machine_rom_header(m, small, 4u, &length));
  REFUSED(t64_machine_rom_header(fresh, small, sizeof small, &length));
  /* Profile and digest. */
  REFUSED(t64_machine_profile(m, NULL));
  REFUSED(t64_machine_digest(m, NULL));
  REFUSED(t64_machine_digest(fresh, digest));
  /* State export: NULL out, too small, a profile outside the family, no image. */
  REFUSED(t64_machine_state_export(m, NULL, 0u, NULL));
  REFUSED(t64_machine_state_export(m, small, sizeof small, &length));
  REFUSED(t64_machine_state_export(legacy, small, sizeof small, &length));
  REFUSED(t64_machine_state_export(fresh, small, sizeof small, &length));
  /* State import: NULL, empty, short, truncated and three flipped bytes. */
  REFUSED(t64_machine_state_import(m, NULL, 0u, 0u, &report));
  REFUSED(t64_machine_state_import(m, blob, 0u, 0u, &report));
  REFUSED(t64_machine_state_import(m, blob, 16u, 0u, &report));
  REFUSED(t64_machine_state_import(m, blob, blob_length - 1u, 0u, &report));
  blob[0] ^= 0x01u;
  REFUSED(t64_machine_state_import(m, blob, blob_length, 0u, &report));
  blob[0] ^= 0x01u;
  blob[blob_length / 2u] ^= 0x01u;
  REFUSED(t64_machine_state_import(m, blob, blob_length, 0u, &report));
  blob[blob_length / 2u] ^= 0x01u;
  blob[blob_length - 1u] ^= 0x01u;
  REFUSED(t64_machine_state_import(m, blob, blob_length, 0u, &report));
  blob[blob_length - 1u] ^= 0x01u;
  /* CPU stop detail, output drain, video and audio observations. */
  REFUSED(t64_machine_cpu_stop_detail(m, NULL));
  REFUSED(t64_machine_cpu_stop_detail(fresh, &detail));
  REFUSED(t64_machine_output_drain(m, NULL, 5u, &drain));
  REFUSED(t64_machine_output_drain(m, small, sizeof small, NULL));
  REFUSED(t64_video_info(m, NULL));
  REFUSED(t64_video_info(fresh, &video_info));
  REFUSED(t64_video_copy(m, NULL, 4u, &length));
  REFUSED(t64_video_copy(m, small, sizeof small, NULL));
  REFUSED(t64_audio_info(m, NULL));
  REFUSED(t64_audio_info(fresh, &audio_info));
  REFUSED(t64_audio_copy(m, NULL, 4u, &length));
  REFUSED(t64_audio_copy(m, small, sizeof small, NULL));
  /* Save device: unknown kind, unsupported kind, bad format, short buffer,
   * wrong size, NULL out. */
  REFUSED(t64_machine_save_info(m, UINT32_C(99), &save_info));
  REFUSED(t64_machine_save_info(m, UINT32_C(5), &save_info));
  REFUSED(t64_machine_save_info(m, save_kind, NULL));
  REFUSED(t64_machine_save_export(m, UINT32_C(99), 1u, small, sizeof small,
                                  &length));
  REFUSED(t64_machine_save_export(m, save_kind, 0u, small, sizeof small,
                                  &length));
  REFUSED(t64_machine_save_export(m, save_kind, 1u, small, 4u, &length));
  REFUSED(t64_machine_save_import(m, UINT32_C(99), 1u, asset_bytes, 512u));
  REFUSED(t64_machine_save_import(m, save_kind, 1u, asset_bytes, 1u));
  REFUSED(t64_machine_save_import(m, save_kind, 0u, asset_bytes, 512u));
  /* Timed input: a past tick, a bad port, an out-of-range axis. */
  REFUSED(t64_machine_queue_input(m, 0u, 0u, 0u, 0, 0));
  REFUSED(t64_machine_queue_input(m, UINT64_C(900000), 1u, 0u, 0, 0));
  REFUSED(t64_machine_queue_input(m, UINT64_C(900000), 0u, 0u, 1000000, 0));
  /* Debug bus: bad width, misaligned address, NULL out. */
  REFUSED(t64_machine_bus_read(m, 0u, 3u, &value));
  REFUSED(t64_machine_bus_read(m, UINT32_C(2), 4u, &value));
  REFUSED(t64_machine_bus_read(m, 0u, 4u, NULL));
  REFUSED(t64_machine_bus_write(m, 0u, 3u, 0u));
  REFUSED(t64_machine_bus_write(m, UINT32_C(1), 4u, 0u));
  /* Trace drain: NULL outputs. */
  REFUSED(t64_machine_trace_drain(m, NULL, 4u, &trace_count, &trace_dropped,
                                  &trace_saturated));
  REFUSED(t64_machine_trace_drain(m, records, 1u, NULL, &trace_dropped,
                                  &trace_saturated));
  /* Creation refusals leave no instance to poison but must refuse cleanly. */
  {
    t64_machine *created = (t64_machine *)(uintptr_t)1u;

    T64_CHECK_INT(t64_machine_create(&main_item.allocator, NULL, &created),
                  T64_STATUS_INVALID_ARGUMENT);
    T64_CHECK(created == NULL);
    T64_CHECK_INT(t64_profile_lookup(SAVE_PROFILE_ID, &profile),
                  T64_STATUS_OK);
  }

  T64_CHECK(shapes_run >= MIN_REFUSED_SHAPES);
  printf("refused shapes run: %u\n", shapes_run);
  /* After every refusal the instance still works and is not poisoned. */
  request.kind = T64_STOP_GUEST_TICK;
  request.value = UINT64_C(60000);
  T64_CHECK_INT(t64_machine_advance(m, &request, &result), T64_STATUS_OK);
  T64_CHECK(not_poisoned(m));
  free(blob);
  destroy_and_check(&main_item);
  destroy_and_check(&fresh_item);
  destroy_and_check(&legacy_item);
}

/* Host allocator callbacks that call back into the same instance. */
static void case_reentrant(void) {
  instance hooked;
  instance control;
  uint8_t hooked_digest[T64_DIGEST_SIZE];
  uint8_t control_digest[T64_DIGEST_SIZE];

  if (!make_instance(&hooked, SAVE_PROFILE_ID, false)) return;
  entry_save_kind = hooked.machine->profile.save_kind;
  hooked.counts.hook_machine = hooked.machine;
  /* Admission: the ROM candidate allocation of core/t64_rom.c (an allocator
   * call that receives the allocator, not the instance) and the replacement
   * that releases the previous image are both reached from inside the host
   * callbacks of this instance. */
  T64_CHECK_INT(t64_machine_admit_rom(hooked.machine, asset_bytes,
                                      sizeof asset_bytes),
                T64_STATUS_OK);
  T64_CHECK(hooked.counts.hook_allocate_calls >= 1u);
  T64_CHECK(hooked.counts.hook_saw_rom_size);
  T64_CHECK_INT(t64_machine_admit_rom(hooked.machine, asset_bytes,
                                      sizeof asset_bytes),
                T64_STATUS_OK);
  T64_CHECK(hooked.counts.hook_allocate_calls >= 2u);
  T64_CHECK(hooked.counts.hook_release_calls >= 1u);
  T64_CHECK_INT(hooked.counts.hook_entries,
                hooked.counts.hook_calls * ENTRY_COUNT);
  T64_CHECK_INT(hooked.counts.hook_reentrant, hooked.counts.hook_entries);
  T64_CHECK_INT(hooked.counts.hook_untouched, hooked.counts.hook_entries);
  T64_CHECK(not_poisoned(hooked.machine));
  /* The calls made from the callbacks changed nothing: the digest equals the
   * digest of an instance that never had a callback calling in. */
  hooked.counts.hook_machine = NULL;
  T64_CHECK_INT(t64_machine_digest(hooked.machine, hooked_digest),
                T64_STATUS_OK);
  if (make_instance(&control, SAVE_PROFILE_ID, true)) {
    T64_CHECK_INT(t64_machine_admit_rom(control.machine, asset_bytes,
                                        sizeof asset_bytes),
                  T64_STATUS_OK);
    T64_CHECK_INT(t64_machine_digest(control.machine, control_digest),
                  T64_STATUS_OK);
    T64_CHECK(memcmp(hooked_digest, control_digest, sizeof hooked_digest) ==
              0);
    T64_CHECK_INT(control.counts.hook_calls, 0);
    destroy_and_check(&control);
  }
  /* Destruction calls the host too: the release callbacks see REENTRANT. */
  hooked.counts.hook_machine = hooked.machine;
  {
    const unsigned calls_before = hooked.counts.hook_calls;
    const unsigned entries_before = hooked.counts.hook_entries;

    destroy_and_check(&hooked);
    T64_CHECK(hooked.counts.hook_calls > calls_before);
    T64_CHECK_INT(hooked.counts.hook_entries - entries_before,
                  (hooked.counts.hook_calls - calls_before) * ENTRY_COUNT);
  }
  T64_CHECK_INT(hooked.counts.hook_reentrant, hooked.counts.hook_entries);
  T64_CHECK_INT(hooked.counts.hook_untouched, hooked.counts.hook_entries);
}

/* A NULL machine: INVALID_ARGUMENT and untouched outputs on every entry. */
static void case_null_machine(void) {
  unsigned entry;
  uint32_t poisoned_out = UINT32_C(0xa5a5a5a5);

  entry_save_kind = 1u;
  for (entry = 0u; entry < ENTRY_COUNT; entry++) {
    scratch actual;
    scratch expected;
    t64_status status;

    (void)memset(&actual, 0xa5, sizeof actual);
    (void)memset(&expected, 0xa5, sizeof expected);
    status = call_entry(entry, NULL, &actual);
    if (status != T64_STATUS_INVALID_ARGUMENT ||
        memcmp(&actual, &expected, sizeof actual) != 0) {
      fprintf(stderr, "null machine entry %s: status %u\n",
              entry_names[entry], (unsigned)status);
    }
    T64_CHECK_INT(status, T64_STATUS_INVALID_ARGUMENT);
    T64_CHECK(memcmp(&actual, &expected, sizeof actual) == 0);
  }
  T64_CHECK_INT(t64_machine_is_poisoned(NULL, &poisoned_out),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK_INT(poisoned_out, UINT32_C(0xa5a5a5a5));
  t64_machine_destroy(NULL);
  T64_CHECK(true);
}

/* ---- defect sites (AMBIG-A, AMBIG-B, AMBIG-C/E) ---------------------- */

#define SI_V2_PROFILE_ID "ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-e4k"

static t64_status advance_to(t64_machine *machine, uint32_t kind,
                             uint64_t value, t64_stop_result *result_out) {
  t64_stop_request request;

  request.kind = kind;
  request.value = value;
  return t64_machine_advance(machine, &request, result_out);
}

/* AMBIG-A: an internal phase that is neither 0 nor 1 is a defect, not an
 * argument fault. */
static void defect_cp0_phase(void) {
  instance item;
  t64_stop_result result;
  t64_stop_result expected;

  if (!make_instance(&item, SAVE_PROFILE_ID, true)) return;
  (void)memset(&result, 0xa5, sizeof result);
  (void)memset(&expected, 0xa5, sizeof expected);
  item.machine->cpu.count_half_clock_phase = 7u;
  T64_CHECK_INT(advance_to(item.machine, T64_STOP_GUEST_TICK, 100000u, &result),
                T64_STATUS_INTERNAL_DEFECT);
  T64_CHECK(memcmp(&result, &expected, sizeof result) == 0);
  T64_CHECK(is_poisoned(item.machine));
  T64_CHECK_INT(advance_to(item.machine, T64_STOP_GUEST_TICK, 200000u, &result),
                T64_STATUS_POISONED);
  T64_CHECK(memcmp(&result, &expected, sizeof result) == 0);
  destroy_and_check(&item);
}

/* AMBIG-A: a stored profile digest that no longer matches the profile bytes
 * is a defect at admission; nothing is allocated and the image is kept. */
static void defect_profile_digest(void) {
  instance item;
  uint8_t content_before[T64_DIGEST_SIZE];
  uint8_t content_after[T64_DIGEST_SIZE];
  size_t allocations_before;

  if (!make_instance(&item, SAVE_PROFILE_ID, true)) return;
  T64_CHECK_INT(t64_machine_content_digest(item.machine, content_before),
                T64_STATUS_OK);
  allocations_before = item.counts.allocations;
  item.machine->profile_digest[3] ^= 0x10u;
  T64_CHECK_INT(t64_machine_admit_rom(item.machine, asset_bytes,
                                      sizeof asset_bytes),
                T64_STATUS_INTERNAL_DEFECT);
  T64_CHECK(is_poisoned(item.machine));
  T64_CHECK_INT(item.counts.allocations, allocations_before);
  T64_CHECK(item.machine->has_image);
  T64_CHECK(memcmp(content_before, item.machine->content_digest,
                   sizeof content_before) == 0);
  T64_CHECK_INT(t64_machine_content_digest(item.machine, content_after),
                T64_STATUS_POISONED);
  destroy_and_check(&item);

  /* An instance with no image yet takes the same path. */
  if (!make_instance(&item, SAVE_PROFILE_ID, false)) return;
  item.machine->profile_digest[31] ^= 0x01u;
  T64_CHECK_INT(t64_machine_admit_rom(item.machine, asset_bytes,
                                      sizeof asset_bytes),
                T64_STATUS_INTERNAL_DEFECT);
  T64_CHECK(is_poisoned(item.machine));
  T64_CHECK(!item.machine->has_image);
  destroy_and_check(&item);
}

/* AMBIG-B: the internal inconsistency sites of the digest stream (the
 * coverage table and the hidden depth table every family machine owns)
 * answer INTERNAL_DEFECT and poison; state export finds them before it
 * writes a byte of the caller buffer. */
static void defect_digest_tables(void) {
  static const char *const ids[2] = {SAVE_PROFILE_ID, SI_V2_PROFILE_ID};
  unsigned which;

  for (which = 0u; which < 2u; which++) {
    unsigned entry_point;

    for (entry_point = 0u; entry_point < 2u; entry_point++) {
      instance item;
      uint8_t digest[T64_DIGEST_SIZE];
      uint8_t digest_expected[T64_DIGEST_SIZE];
      uint8_t export_bytes[256];
      uint8_t export_expected[256];
      size_t length = 0x5a5a5a5au;

      if (!make_instance(&item, ids[which], true)) return;
      if (which == 0u) {
        item.machine->rdp_memory_coverage = NULL;
      } else {
        item.machine->rdp_depth_hidden = NULL;
      }
      (void)memset(digest, 0xa5, sizeof digest);
      (void)memset(digest_expected, 0xa5, sizeof digest_expected);
      (void)memset(export_bytes, 0xa5, sizeof export_bytes);
      (void)memset(export_expected, 0xa5, sizeof export_expected);
      if (entry_point == 0u) {
        T64_CHECK_INT(t64_machine_digest(item.machine, digest),
                      T64_STATUS_INTERNAL_DEFECT);
        T64_CHECK(memcmp(digest, digest_expected, sizeof digest) == 0);
      } else {
        T64_CHECK_INT(t64_machine_state_export(item.machine, export_bytes,
                                               sizeof export_bytes, &length),
                      T64_STATUS_INTERNAL_DEFECT);
        T64_CHECK(memcmp(export_bytes, export_expected,
                         sizeof export_bytes) == 0);
        T64_CHECK_INT(length, 0x5a5a5a5au);
      }
      T64_CHECK(is_poisoned(item.machine));
      T64_CHECK_INT(t64_machine_digest(item.machine, digest),
                    T64_STATUS_POISONED);
      destroy_and_check(&item);
    }
  }
}

/* AMBIG-B: state export no longer touches the buffer on a refusal. The
 * buffer is larger than any header or table so a stray clear would show. */
static void export_refusal_untouched(void) {
  instance main_item;
  instance fresh_item;
  instance legacy_item;
  enum { BUFFER_BYTES = 4096 };
  uint8_t bytes[BUFFER_BYTES];
  uint8_t expected[BUFFER_BYTES];
  size_t length;
  size_t total = 0u;

  if (!make_instance(&main_item, SAVE_PROFILE_ID, true)) return;
  if (!make_instance(&fresh_item, SAVE_PROFILE_ID, false)) return;
  if (!make_instance(&legacy_item, LEGACY_PROFILE_ID, true)) return;
  (void)memset(expected, 0xa5, sizeof expected);

  (void)memset(bytes, 0xa5, sizeof bytes);
  length = 0x5a5a5a5au;
  T64_CHECK_INT(t64_machine_state_export(main_item.machine, bytes,
                                         sizeof bytes, &length),
                T64_STATUS_BUFFER_TOO_SMALL);
  total = length;
  T64_CHECK(total > sizeof bytes);
  T64_CHECK(memcmp(bytes, expected, sizeof bytes) == 0);

  (void)memset(bytes, 0xa5, sizeof bytes);
  length = 0x5a5a5a5au;
  T64_CHECK_INT(t64_machine_state_export(main_item.machine, NULL, 0u,
                                         &length),
                T64_STATUS_BUFFER_TOO_SMALL);
  T64_CHECK_INT(length, total);

  (void)memset(bytes, 0xa5, sizeof bytes);
  length = 0x5a5a5a5au;
  T64_CHECK_INT(t64_machine_state_export(fresh_item.machine, bytes,
                                         sizeof bytes, &length),
                T64_STATUS_NOT_READY);
  T64_CHECK(memcmp(bytes, expected, sizeof bytes) == 0);
  T64_CHECK_INT(length, 0x5a5a5a5au);

  (void)memset(bytes, 0xa5, sizeof bytes);
  length = 0x5a5a5a5au;
  T64_CHECK_INT(t64_machine_state_export(legacy_item.machine, bytes,
                                         sizeof bytes, &length),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK(memcmp(bytes, expected, sizeof bytes) == 0);
  T64_CHECK_INT(length, 0x5a5a5a5au);

  T64_CHECK(not_poisoned(main_item.machine));
  T64_CHECK(not_poisoned(fresh_item.machine));
  T64_CHECK(not_poisoned(legacy_item.machine));
  destroy_and_check(&main_item);
  destroy_and_check(&fresh_item);
  destroy_and_check(&legacy_item);
}

/* AMBIG-C/E: every status a real handler returns by design is masked into
 * today's RESOURCE_LIMIT stop (or the CPU stop reason) and never poisons.
 * Each scenario sets the state that makes one real handler return one
 * whitelisted status through dispatch_front. */
typedef enum {
  SCENARIO_CPU_STOP,
  SCENARIO_SEQUENCE_EXHAUSTED,
  SCENARIO_QUEUE_FULL,
  SCENARIO_TICK_OVERFLOW,
  SCENARIO_TIME_REVERSED,
  SCENARIO_PI_REFUSAL,
  SCENARIO_COUNT
} handler_scenario;

static const char *const scenario_names[SCENARIO_COUNT] = {
    "cpu-stop", "sequence-exhausted", "queue-full", "tick-overflow",
    "time-reversed", "pi-refusal"};

static void guest_driven_case(handler_scenario scenario) {
  instance item;
  t64_stop_result result;
  t64_status status;
  uint32_t reason_wanted = T64_STOP_REASON_RESOURCE_LIMIT;
  uint64_t bound = UINT64_C(4000000);
  uint64_t tick_before = 0u;
  size_t queue_before = 0u;
  unsigned index;

  if (!make_instance(&item, SAVE_PROFILE_ID, true)) return;
  (void)memset(&result, 0xa5, sizeof result);
  queue_before = item.machine->queue_count;
  switch (scenario) {
  case SCENARIO_CPU_STOP:
    /* The cleared boot asset runs to a terminal CPU stop; the step that stops
     * the CPU returns a status the scheduler masks into the CPU stop reason.
     * Several advances are needed because each is bounded by the event
     * budget (a RESOURCE_LIMIT stop that is not a handler error). */
    bound = UINT64_C(2000000000);
    reason_wanted = 0u;
    for (index = 0u; index < 400u && !item.machine->cpu.stopped; index++) {
      status = advance_to(item.machine, T64_STOP_GUEST_TICK, bound, &result);
      T64_CHECK_INT(status, T64_STATUS_OK);
    }
    T64_CHECK(item.machine->cpu.stopped);
    break;
  case SCENARIO_SEQUENCE_EXHAUSTED:
    item.machine->profile.sequence_width_bits = 8u;
    item.machine->next_sequence = UINT64_C(253);
    break;
  case SCENARIO_QUEUE_FULL:
    for (index = 0u; index < 2u; index++) {
      T64_CHECK_INT(t64_test_schedule_synthetic(item.machine,
                                                UINT64_C(900000000),
                                                T64_TEST_EVENT_NOOP, index),
                    T64_STATUS_OK);
    }
    item.machine->profile.event_queue_capacity = 2u;
    break;
  case SCENARIO_TICK_OVERFLOW:
    item.machine->tick = UINT64_MAX - 1u;
    item.machine->events[item.machine->queue_head].tick = UINT64_MAX - 1u;
    item.machine->cpu.count_sync_tick = UINT64_MAX - 1u;
    bound = UINT64_MAX;
    break;
  case SCENARIO_TIME_REVERSED:
    item.machine->cpu.count_sync_tick = UINT64_C(0x7fffffff00000000);
    break;
  case SCENARIO_PI_REFUSAL:
    /* Authored register stores: a cart-to-RDRAM request with a source beyond
     * the cart window is refused by the device (a device refusal is a bus
     * outcome that never poisons); the second request is accepted and runs. */
    T64_CHECK_INT(t64_machine_bus_write(item.machine, UINT32_C(0x04600000), 4u,
                                        UINT32_C(0x1000)),
                  T64_STATUS_OK);
    T64_CHECK_INT(t64_machine_bus_write(item.machine, UINT32_C(0x04600004), 4u,
                                        UINT32_C(0x10100000)),
                  T64_STATUS_OK);
    T64_CHECK_INT(t64_machine_bus_write(item.machine, UINT32_C(0x0460000c), 4u,
                                        UINT32_C(0x3f)),
                  T64_STATUS_BUS_DEVICE_REFUSED);
    T64_CHECK(not_poisoned(item.machine));
    break;
  default:
    break;
  }
  tick_before = item.machine->tick;
  queue_before = item.machine->queue_count;
  status = advance_to(item.machine, T64_STOP_GUEST_TICK, bound, &result);
  printf("scenario %s: status %u reason %u tick %llu->%llu queue %zu->%zu "
         "sequence_exhausted %d stopped %d\n",
         scenario_names[scenario], (unsigned)status, (unsigned)result.reason,
         (unsigned long long)tick_before, (unsigned long long)result.tick,
         queue_before, item.machine->queue_count,
         (int)item.machine->sequence_exhausted, (int)item.machine->cpu.stopped);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (reason_wanted != 0u) {
    T64_CHECK_INT(result.reason, reason_wanted);
  } else {
    T64_CHECK(result.reason == T64_STOP_REASON_GUEST_COMPLETION ||
              result.reason == T64_STOP_REASON_CPU_UNSUPPORTED);
  }
  switch (scenario) {
  case SCENARIO_SEQUENCE_EXHAUSTED:
    T64_CHECK(item.machine->sequence_exhausted);
    break;
  case SCENARIO_QUEUE_FULL:
    /* The step event ran and its successor could not be queued. */
    T64_CHECK(item.machine->tick > tick_before ||
              item.machine->instruction_count > 0u);
    break;
  case SCENARIO_TIME_REVERSED:
    /* The time synchronization refused before the event was dequeued. */
    T64_CHECK_INT(item.machine->queue_count, queue_before);
    T64_CHECK_INT(item.machine->tick, tick_before);
    break;
  default:
    break;
  }
  T64_CHECK(not_poisoned(item.machine));
  destroy_and_check(&item);
}

static void case_defect_sites(void) {
  unsigned scenario;

  defect_cp0_phase();
  defect_profile_digest();
  defect_digest_tables();
  export_refusal_untouched();
  for (scenario = 0u; scenario < (unsigned)SCENARIO_COUNT; scenario++) {
    guest_driven_case((handler_scenario)scenario);
  }
}

/* ---- t64_machine_bus_write carve-out (AMBIG-D) ----------------------- */

/* A byte range of the instance that a refused write is allowed to change. */
typedef struct {
  size_t offset;
  size_t length;
} allowed_span;

#define ALLOWED(member_)                                                     \
  {offsetof(t64_machine, member_), sizeof(((t64_machine *)0)->member_)}

/* The members the header names for a write that a device refuses: the
 * latched PI length registers and the sticky PI and SI error flags, plus the
 * trace buffer when tracing is enabled (one record). */
static const allowed_span pi_latches[] = {ALLOWED(pi.read_length),
                                          ALLOWED(pi.write_length),
                                          ALLOWED(pi.error)};
static const allowed_span si_latches[] = {ALLOWED(si.error)};
static const allowed_span trace_members[] = {
    ALLOWED(trace_records), ALLOWED(trace_head), ALLOWED(trace_count),
    ALLOWED(trace_drop_count), ALLOWED(trace_drop_count_saturated)};

static bool span_allowed(size_t index, const allowed_span *spans,
                         size_t count) {
  size_t i;

  for (i = 0u; i < count; i++) {
    if (index >= spans[i].offset && index - spans[i].offset < spans[i].length) {
      return true;
    }
  }
  return false;
}

/* Bytes of the instance that differ and are not inside an allowed span. */
static size_t unexpected_changes(const uint8_t *before, const uint8_t *after,
                                 const allowed_span *first, size_t first_count,
                                 const allowed_span *second,
                                 size_t second_count, size_t *changed_out) {
  size_t index;
  size_t unexpected = 0u;

  *changed_out = 0u;
  for (index = 0u; index < sizeof(t64_machine); index++) {
    if (before[index] != after[index]) {
      (*changed_out)++;
      if (!span_allowed(index, first, first_count) &&
          !span_allowed(index, second, second_count)) {
        unexpected++;
      }
    }
  }
  return unexpected;
}

/* Every public observation that must not move on any refused write. */
typedef struct {
  t64_status status[7];
  t64_save_info save;
  t64_video_packet_info video;
  t64_audio_packet_info audio;
  t64_cpu_stop_detail detail;
  size_t rom_size;
  uint8_t header[T64_ROM_HEADER_BYTES];
  size_t header_length;
  uint8_t content[T64_DIGEST_SIZE];
} observations;

static void observe(t64_machine *machine, observations *o) {
  (void)memset(o, 0xa5, sizeof *o);
  o->status[0] = t64_machine_save_info(machine, entry_save_kind, &o->save);
  o->status[1] = t64_video_info(machine, &o->video);
  o->status[2] = t64_audio_info(machine, &o->audio);
  o->status[3] = t64_machine_cpu_stop_detail(machine, &o->detail);
  o->status[4] = t64_machine_rom_size(machine, &o->rom_size);
  o->status[5] = t64_machine_rom_header(machine, o->header, sizeof o->header,
                                        &o->header_length);
  o->status[6] = t64_machine_content_digest(machine, o->content);
}

typedef struct {
  instance item;
  uint8_t *before;
  uint8_t *after;
  observations seen_before;
  observations seen_after;
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
} carve_fixture;

static bool carve_begin(carve_fixture *f, bool tracing) {
  (void)memset(f, 0, sizeof *f);
  if (!make_instance(&f->item, SAVE_PROFILE_ID, true)) return false;
  entry_save_kind = f->item.machine->profile.save_kind;
  f->before = (uint8_t *)malloc(sizeof(t64_machine));
  f->after = (uint8_t *)malloc(sizeof(t64_machine));
  if (f->before == NULL || f->after == NULL) {
    T64_CHECK(false);
    return false;
  }
  T64_CHECK_INT(t64_machine_trace_set_enabled(f->item.machine, tracing),
                T64_STATUS_OK);
  return true;
}

/* Take the before-snapshot, make the refused write, take the after-snapshot
 * and check what moved. */
static void carve_write(carve_fixture *f, uint32_t address, uint32_t width,
                        uint32_t value, t64_status expected,
                        const allowed_span *latches, size_t latch_count,
                        bool tracing, size_t *changed_out) {
  t64_machine *machine = f->item.machine;
  size_t unexpected;
  size_t trace_before = machine->trace_count;

  (void)memcpy(f->before, machine, sizeof(t64_machine));
  observe(machine, &f->seen_before);
  T64_CHECK_INT(t64_machine_digest(machine, f->digest_before), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, address, width, value),
                expected);
  (void)memcpy(f->after, machine, sizeof(t64_machine));
  observe(machine, &f->seen_after);
  T64_CHECK_INT(t64_machine_digest(machine, f->digest_after), T64_STATUS_OK);
  unexpected = unexpected_changes(
      f->before, f->after, latches, latch_count,
      tracing ? trace_members : NULL,
      tracing ? sizeof trace_members / sizeof trace_members[0] : 0u,
      changed_out);
  T64_CHECK_INT(unexpected, 0);
  T64_CHECK(memcmp(&f->seen_before, &f->seen_after, sizeof f->seen_before) ==
            0);
  if (tracing && latch_count != 0u) {
    /* A device refusal writes exactly one refusal record. */
    T64_CHECK_INT(machine->trace_count - trace_before, 1);
  } else {
    T64_CHECK_INT(machine->trace_count, trace_before);
  }
  T64_CHECK(not_poisoned(machine));
}

static void carve_end(carve_fixture *f) {
  free(f->before);
  free(f->after);
  destroy_and_check(&f->item);
}

static void carve_pi(bool tracing) {
  carve_fixture f;
  size_t changed = 0u;
  t64_machine *machine;

  if (!carve_begin(&f, tracing)) return;
  machine = f.item.machine;
  /* A request made while a transfer is active: the length register latches
   * the refused request and the sticky error is set. */
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x04600000), 4u,
                                      UINT32_C(0x1000)), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x04600004), 4u,
                                      UINT32_C(0x10000000)), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x0460000c), 4u,
                                      UINT32_C(0x3f)), T64_STATUS_OK);
  T64_CHECK(!machine->pi.error);
  carve_write(&f, UINT32_C(0x0460000c), 4u, UINT32_C(0x1f),
              T64_STATUS_BUS_DEVICE_REFUSED, pi_latches,
              sizeof pi_latches / sizeof pi_latches[0], tracing, &changed);
  T64_CHECK(machine->pi.error);
  T64_CHECK_INT(machine->pi.write_length, 0x1f);
  T64_CHECK(changed > 0u);
  carve_end(&f);

  /* A request the device rejects while idle (source beyond the cart window)
   * latches the length register only. */
  if (!carve_begin(&f, tracing)) return;
  machine = f.item.machine;
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x04600000), 4u,
                                      UINT32_C(0x1000)), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x04600004), 4u,
                                      UINT32_C(0x10100000)), T64_STATUS_OK);
  carve_write(&f, UINT32_C(0x0460000c), 4u, UINT32_C(0x3f),
              T64_STATUS_BUS_DEVICE_REFUSED, pi_latches,
              sizeof pi_latches / sizeof pi_latches[0], tracing, &changed);
  T64_CHECK(!machine->pi.error);
  T64_CHECK_INT(machine->pi.write_length, 0x3f);
  carve_end(&f);
}

static void carve_si(bool tracing) {
  carve_fixture f;
  size_t changed = 0u;
  t64_machine *machine;

  if (!carve_begin(&f, tracing)) return;
  machine = f.item.machine;
  /* A misaligned RDRAM address: the sticky SI DMA error latches. */
  T64_CHECK_INT(t64_machine_bus_write(machine, T64_SI_BASE + T64_SI_REG_DRAM_ADDR,
                                      4u, UINT32_C(0x1004)), T64_STATUS_OK);
  T64_CHECK(!machine->si.error);
  carve_write(&f, T64_SI_BASE + T64_SI_REG_WR64B, 4u, T64_SI_PIF_ADDRESS,
              T64_STATUS_BUS_DEVICE_REFUSED, si_latches,
              sizeof si_latches / sizeof si_latches[0], tracing, &changed);
  T64_CHECK(machine->si.error);
  T64_CHECK(changed > 0u);
  carve_end(&f);
}

/* Refusals for address, width, alignment and range change nothing at all
 * (every byte of the instance), and a device refusal with no latch (the
 * cartridge window is read-only) changes nothing either. */
static void carve_plain_refusals(void) {
  static const struct {
    uint32_t address;
    uint32_t width;
    t64_status expected;
  } probes[] = {
      {UINT32_C(0x80000000), 4u, T64_STATUS_BUS_UNMAPPED},
      {UINT32_C(0x04900000), 4u, T64_STATUS_BUS_UNMAPPED},
      {UINT32_C(0x00800000), 4u, T64_STATUS_BUS_UNSUPPORTED},
      {UINT32_C(0x04600034), 4u, T64_STATUS_BUS_UNSUPPORTED},
      {UINT32_C(0x00000001), 4u, T64_STATUS_BUS_ALIGNMENT},
      {UINT32_C(0x00000001), 2u, T64_STATUS_BUS_ALIGNMENT},
      {UINT32_C(0xfffffffe), 4u, T64_STATUS_BUS_RANGE},
      {UINT32_C(0xffffffff), 2u, T64_STATUS_BUS_RANGE},
      {UINT32_C(0x10000000), 4u, T64_STATUS_BUS_DEVICE_REFUSED}};
  size_t index;

  for (index = 0u; index < sizeof probes / sizeof probes[0]; index++) {
    unsigned tracing;

    for (tracing = 0u; tracing < 2u; tracing++) {
      carve_fixture f;
      size_t changed = 99u;

      if (!carve_begin(&f, tracing != 0u)) return;
      carve_write(&f, probes[index].address, probes[index].width,
                  UINT32_C(0xdeadbeef), probes[index].expected, NULL, 0u,
                  false, &changed);
      T64_CHECK_INT(changed, 0);
      T64_CHECK(memcmp(f.digest_before, f.digest_after,
                       sizeof f.digest_before) == 0);
      carve_end(&f);
    }
  }
}

static void case_bus_carve_out(void) {
  carve_pi(false);
  carve_pi(true);
  carve_si(false);
  carve_si(true);
  carve_plain_refusals();
}

int main(int argc, char **argv) {
  static const char *const cases[] = {"poison", "refusals-never-poison",
                                      "reentrant", "null-machine",
                                      "defect-sites", "bus-carve-out"};
  const char *selected = NULL;
  size_t index;
  bool ran = false;

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
      printf("%s\n", cases[index]);
    }
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    selected = argv[2];
  } else if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--case NAME]\n", argv[0]);
    return 2;
  }
  if (!read_openipl3_asset(asset_bytes)) {
    fprintf(stderr, "T64_TEST_IPL3_PATH is not set to the cleared asset\n");
    return 2;
  }
  if (selected == NULL || strcmp(selected, "poison") == 0) {
    case_poison();
    ran = true;
  }
  if (selected == NULL || strcmp(selected, "refusals-never-poison") == 0) {
    case_refusals_never_poison();
    ran = true;
  }
  if (selected == NULL || strcmp(selected, "reentrant") == 0) {
    case_reentrant();
    ran = true;
  }
  if (selected == NULL || strcmp(selected, "null-machine") == 0) {
    case_null_machine();
    ran = true;
  }
  if (selected == NULL || strcmp(selected, "defect-sites") == 0) {
    case_defect_sites();
    ran = true;
  }
  if (selected == NULL || strcmp(selected, "bus-carve-out") == 0) {
    case_bus_carve_out();
    ran = true;
  }
  if (!ran) {
    fprintf(stderr, "unknown case: %s\n", selected);
    return 2;
  }
  if (selected == NULL) {
    return T64_TEST_FINISH("unit.core.api");
  }
  if (strcmp(selected, "poison") == 0) {
    return T64_TEST_FINISH("unit.core.api.poison");
  }
  if (strcmp(selected, "reentrant") == 0) {
    return T64_TEST_FINISH("unit.core.api.reentrant");
  }
  if (strcmp(selected, "null-machine") == 0) {
    return T64_TEST_FINISH("unit.core.api.null-machine");
  }
  if (strcmp(selected, "defect-sites") == 0) {
    return T64_TEST_FINISH("unit.core.api.defect-sites");
  }
  if (strcmp(selected, "bus-carve-out") == 0) {
    return T64_TEST_FINISH("unit.core.api.bus-carve-out");
  }
  return T64_TEST_FINISH("unit.core.api.refusals-never-poison");
}
