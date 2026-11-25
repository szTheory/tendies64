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
#define ENTRY_COUNT 23u
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
} counting_allocator;

static bool counting_allocate(void *context, size_t size, size_t alignment,
                              t64_host_storage *storage_out) {
  counting_allocator *state = (counting_allocator *)context;
  void *memory;

  if (state == NULL || storage_out == NULL || size == 0u || alignment == 0u ||
      state->live_count >= MAX_LIVE) {
    return false;
  }
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

/* The four public observers whose bodies are sealed by the no-HLE checker
 * (t64_video_info, t64_video_copy, t64_audio_info, t64_audio_copy; entries 11,
 * 12, 16 and 17) get their guard in task 2 of plan 11-03, together with the
 * isolated refresh of their pins. Until then the poison sweep skips them. */
static const bool sealed_observers_guarded = false;

static bool is_sealed_observer(unsigned entry) {
  return entry == 11u || entry == 12u || entry == 16u || entry == 17u;
}

static const char *const entry_names[ENTRY_COUNT] = {
    "admit_rom",     "advance",        "content_digest", "rom_size",
    "rom_header",    "profile",        "digest",         "state_export",
    "state_import",  "cpu_stop_detail", "output_drain",  "video_info",
    "video_copy",    "save_info",      "save_export",    "save_import",
    "audio_info",    "audio_copy",     "queue_input",    "bus_read",
    "bus_write",     "trace_set_enabled", "trace_drain"};

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
    return t64_machine_save_info(machine, machine->profile.save_kind,
                                 &s->save_info);
  case 14:
    return t64_machine_save_export(machine, machine->profile.save_kind, 1u,
                                   s->save_bytes, sizeof s->save_bytes,
                                   &s->save_length);
  case 15:
    return t64_machine_save_import(machine, machine->profile.save_kind, 1u,
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
  default:
    return T64_STATUS_INVALID_ARGUMENT;
  }
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

    if (!sealed_observers_guarded && is_sealed_observer(entry)) continue;
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

int main(int argc, char **argv) {
  static const char *const cases[] = {"poison", "refusals-never-poison"};
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
  if (!ran) {
    fprintf(stderr, "unknown case: %s\n", selected);
    return 2;
  }
  if (selected == NULL) {
    return T64_TEST_FINISH("unit.core.api");
  }
  return T64_TEST_FINISH(strcmp(selected, "poison") == 0
                             ? "unit.core.api.poison"
                             : "unit.core.api.refusals-never-poison");
}
