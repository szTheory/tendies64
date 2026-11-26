/*
 * Lifecycle and output surface of the public API (plan 11-04, API-01, API-02).
 *
 * This file includes only the public header and the shared test header, so it
 * sees exactly what an installed consumer sees. The poisoned, reentrant and
 * NULL-machine answers of the three entries added here are proved with the
 * other 23 entries in test_api_poison.c.
 *
 *   reset            public reset returns the admitted image to its first-admit
 *                    state (the digest equals the digest taken right after the
 *                    first admit; both rings and the ISViewer cursor are
 *                    empty); a machine without an image refuses with
 *                    T64_STATUS_NOT_READY and a NULL machine with
 *                    T64_STATUS_INVALID_ARGUMENT, state unchanged
 *   unload           public unload releases the image storage through the
 *                    host allocator; afterwards admission equals a fresh
 *                    admission by digest, and destroy leaves nothing live
 *   not-ready-table  one table over every machine-taking entry after unload,
 *                    and the advance span rule (plan 11-27): a request or a
 *                    result placed inside the instance's own blocks is refused
 *                    with INVALID_ARGUMENT before struct_size and admission
 *   output-status    t64_machine_output_status reports the ring occupancy
 *                    (added by task 3 of the plan)
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api_test_support.h"
#include "t64_test.h"
#include "tendies64/t64_machine.h"

#define MAX_FIXTURE_BYTES (64u * 1024u)
#define HELLO_PROFILE "ntsc-cart-4m-openipl3-warm-v1"
#define VI_PROFILE "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9"
#define HELLO_TRANSCRIPT "Tendies64 hello via open IPL3\n"
#define HELLO_BYTES 30u
#define HELLO_BOUND_TICK UINT64_C(700000)

typedef struct {
  counting_allocator counts;
  t64_host_allocator allocator;
  t64_machine *machine;
} instance;

static uint8_t hello_rom[MAX_FIXTURE_BYTES];
static size_t hello_length;
static uint8_t fill_rom[MAX_FIXTURE_BYTES];
static size_t fill_length;
static uint8_t audio_rom[MAX_FIXTURE_BYTES];
static size_t audio_length;

/* Create an instance for the profile id; admit the image when asked. */
static bool make_instance(instance *item, const char *profile_id,
                          const uint8_t *image, size_t image_length) {
  const t64_profile_descriptor *profile = NULL;

  (void)memset(item, 0, sizeof *item);
  item->allocator = (t64_host_allocator)T64_HOST_ALLOCATOR_INIT(
      counting_allocate, counting_release, &item->counts);
  T64_CHECK_INT(t64_profile_lookup(profile_id, &profile), T64_STATUS_OK);
  if (profile == NULL) return false;
  T64_CHECK_INT(t64_machine_create(
                    &(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(
                        &item->allocator, profile),
                    &item->machine),
                T64_STATUS_OK);
  if (item->machine == NULL) return false;
  if (image != NULL) {
    const t64_status status =
        t64_machine_admit_rom(item->machine, image, image_length);

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

/* Advance to a guest tick, resuming after the resumable per-advance event
 * budget stop, until the bound is reached or the machine stops for good;
 * returns the last stop reason (0 when an advance was refused or stalled). */
static uint32_t advance_to(t64_machine *machine, uint64_t tick) {
  uint32_t reason = 0u;
  uint64_t previous_tick = UINT64_MAX;
  unsigned attempt;

  for (attempt = 0u; attempt < 200000u; attempt++) {
    t64_stop_request request = T64_STOP_REQUEST_INIT(0u, 0u);
    t64_stop_result result = T64_STOP_RESULT_INIT;
    t64_status status;

    request.kind = T64_STOP_GUEST_TICK;
    request.value = tick;
    result.reason = 0u;
    result.tick = 0u;
    status = t64_machine_advance(machine, &request, &result);
    T64_CHECK_INT(status, T64_STATUS_OK);
    if (status != T64_STATUS_OK) return 0u;
    reason = result.reason;
    if (reason != T64_STOP_REASON_RESOURCE_LIMIT) break;
    if (result.tick == previous_tick) return 0u;
    previous_tick = result.tick;
  }
  return reason;
}

static bool digest_of(const t64_machine *machine,
                      uint8_t digest[T64_DIGEST_SIZE]) {
  const t64_status status = t64_machine_digest(machine, digest);

  T64_CHECK_INT(status, T64_STATUS_OK);
  return status == T64_STATUS_OK;
}

static bool same_digest(const uint8_t a[T64_DIGEST_SIZE],
                        const uint8_t b[T64_DIGEST_SIZE]) {
  return memcmp(a, b, T64_DIGEST_SIZE) == 0;
}

/* ISViewer: the hello guest prints a 30-byte transcript, left undrained; reset
 * must bring back the first-admit state and an empty drain cursor. */
static void reset_isviewer_scenario(void) {
  instance item;
  uint8_t first_admit[T64_DIGEST_SIZE];
  uint8_t after_run[T64_DIGEST_SIZE];
  uint8_t after_reset[T64_DIGEST_SIZE];
  uint8_t after_second_run[T64_DIGEST_SIZE];
  uint8_t drained[HELLO_BYTES + 2u];
  t64_output_drain_result drain = T64_OUTPUT_DRAIN_RESULT_INIT;
  t64_cpu_stop_detail detail = T64_CPU_STOP_DETAIL_INIT;

  if (!make_instance(&item, HELLO_PROFILE, hello_rom, hello_length)) return;
  T64_CHECK(digest_of(item.machine, first_admit));
  drain = (t64_output_drain_result)T64_OUTPUT_DRAIN_RESULT_INIT;
  T64_CHECK_INT(t64_machine_output_drain(item.machine, NULL, 0u, &drain),
                T64_STATUS_OK);
  T64_CHECK_INT(drain.copied_bytes, 0);
  T64_CHECK_INT(drain.required_bytes, 0);

  /* Run to the authored BREAK and drain nothing. */
  T64_CHECK_INT(advance_to(item.machine, HELLO_BOUND_TICK),
                T64_STOP_REASON_GUEST_COMPLETION);
  T64_CHECK(digest_of(item.machine, after_run));
  T64_CHECK(!same_digest(first_admit, after_run));
  drain = (t64_output_drain_result)T64_OUTPUT_DRAIN_RESULT_INIT;
  T64_CHECK_INT(t64_machine_output_drain(item.machine, NULL, 0u, &drain),
                T64_STATUS_BUFFER_TOO_SMALL);
  T64_CHECK_INT(drain.required_bytes, HELLO_BYTES);
  T64_CHECK_INT(t64_machine_cpu_stop_detail(item.machine, &detail),
                T64_STATUS_OK);

  /* Reset: the first-admit digest, an empty cursor and a running CPU. */
  T64_CHECK_INT(t64_machine_reset(item.machine), T64_STATUS_OK);
  T64_CHECK(digest_of(item.machine, after_reset));
  T64_CHECK(same_digest(first_admit, after_reset));
  poison_cpu_stop_detail(&detail);
  T64_CHECK_INT(t64_machine_cpu_stop_detail(item.machine, &detail),
                T64_STATUS_NOT_READY);
  poison_drain_result(&drain);
  T64_CHECK_INT(
      t64_machine_output_drain(item.machine, drained, sizeof drained, &drain),
      T64_STATUS_OK);
  T64_CHECK_INT(drain.copied_bytes, 0);
  T64_CHECK_INT(drain.required_bytes, 0);
  T64_CHECK_INT(drain.total_attempted_bytes, 0);

  /* The replay is the same run: same stop, same digest, same transcript. */
  T64_CHECK_INT(advance_to(item.machine, HELLO_BOUND_TICK),
                T64_STOP_REASON_GUEST_COMPLETION);
  T64_CHECK(digest_of(item.machine, after_second_run));
  T64_CHECK(same_digest(after_run, after_second_run));
  (void)memset(drained, 0, sizeof drained);
  drain = (t64_output_drain_result)T64_OUTPUT_DRAIN_RESULT_INIT;
  T64_CHECK_INT(
      t64_machine_output_drain(item.machine, drained, sizeof drained, &drain),
      T64_STATUS_OK);
  T64_CHECK_INT(drain.copied_bytes, HELLO_BYTES);
  T64_CHECK(memcmp(drained, HELLO_TRANSCRIPT, HELLO_BYTES) == 0);

  /* Reset again after a drain: the cursor does not survive. */
  T64_CHECK_INT(t64_machine_reset(item.machine), T64_STATUS_OK);
  T64_CHECK(digest_of(item.machine, after_reset));
  T64_CHECK(same_digest(first_admit, after_reset));
  poison_drain_result(&drain);
  T64_CHECK_INT(
      t64_machine_output_drain(item.machine, drained, sizeof drained, &drain),
      T64_STATUS_OK);
  T64_CHECK_INT(drain.copied_bytes, 0);

  /* Reset allocates and releases nothing: one image, one video, one audio
   * block (and the instance) stay live until destroy. */
  T64_CHECK_INT(item.counts.releases, 0);
  destroy_and_check(&item);
}

#define VI_BASE UINT32_C(0x04400000)
#define VI_ORIGIN UINT32_C(0x3000)

static bool write_bus(t64_machine *machine, uint32_t address, uint32_t width,
                      uint32_t value) {
  const t64_status status = t64_machine_bus_write(machine, address, width,
                                                  value);

  T64_CHECK_INT(status, T64_STATUS_OK);
  return status == T64_STATUS_OK;
}

/* Author a four-pixel frame in RDRAM and enable the VI, exactly as the
 * existing video output tests do through the public bus calls. */
static bool enable_video(t64_machine *machine) {
  static const uint16_t source[4] = {UINT16_C(0xf801), UINT16_C(0x07c1),
                                     UINT16_C(0x003f), UINT16_C(0xffff)};
  size_t index;

  for (index = 0u; index < sizeof source / sizeof source[0]; index++) {
    if (!write_bus(machine, VI_ORIGIN + (uint32_t)(index * 2u), 2u,
                   source[index])) {
      return false;
    }
  }
  return write_bus(machine, VI_BASE + UINT32_C(0x04), 4u, VI_ORIGIN) &&
         write_bus(machine, VI_BASE + UINT32_C(0x08), 4u, UINT32_C(4)) &&
         write_bus(machine, VI_BASE + UINT32_C(0x18), 4u, UINT32_C(8)) &&
         write_bus(machine, VI_BASE + UINT32_C(0x1c), 4u, UINT32_C(15)) &&
         write_bus(machine, VI_BASE + UINT32_C(0x24), 4u, UINT32_C(4)) &&
         write_bus(machine, VI_BASE + UINT32_C(0x28), 4u, UINT32_C(2)) &&
         write_bus(machine, VI_BASE + UINT32_C(0x30), 4u, UINT32_C(0x400)) &&
         write_bus(machine, VI_BASE + UINT32_C(0x34), 4u, UINT32_C(0x400)) &&
         write_bus(machine, VI_BASE + UINT32_C(0x00), 4u, UINT32_C(2));
}

/* Advance in large guest-tick strides until the first raw VI packet exists. */
static bool run_until_video_packet(t64_machine *machine,
                                   t64_video_packet_info *info_out) {
  uint64_t tick = UINT64_C(0);
  unsigned stride;

  for (stride = 0u; stride < 64u; stride++) {
    tick += UINT64_C(500000);
    (void)advance_to(machine, tick);
    if (t64_video_info(machine, info_out) == T64_STATUS_OK) return true;
  }
  return false;
}

/* Rings: a queued VI packet and authored RDRAM and VI state do not survive
 * reset; the replay produces the same first packet. */
static void reset_rings_scenario(void) {
  instance item;
  uint8_t first_admit[T64_DIGEST_SIZE];
  uint8_t after_reset[T64_DIGEST_SIZE];
  uint8_t packet[320u * 240u * 4u];
  t64_video_packet_info first_info = T64_VIDEO_PACKET_INFO_INIT;
  t64_video_packet_info replay_info = T64_VIDEO_PACKET_INFO_INIT;
  t64_video_packet_info untouched = T64_VIDEO_PACKET_INFO_INIT;
  t64_audio_packet_info audio = T64_AUDIO_PACKET_INFO_INIT;
  size_t length = 0u;

  if (!make_instance(&item, VI_PROFILE, fill_rom, fill_length)) return;
  T64_CHECK(digest_of(item.machine, first_admit));
  T64_CHECK(enable_video(item.machine));
  first_info = (t64_video_packet_info)T64_VIDEO_PACKET_INFO_INIT;
  T64_CHECK(run_until_video_packet(item.machine, &first_info));
  T64_CHECK(first_info.packet_bytes != 0u);

  T64_CHECK_INT(t64_machine_reset(item.machine), T64_STATUS_OK);
  T64_CHECK(digest_of(item.machine, after_reset));
  T64_CHECK(same_digest(first_admit, after_reset));
  poison_video_info(&untouched);
  (void)memcpy(&replay_info, &untouched, sizeof replay_info);
  T64_CHECK_INT(t64_video_info(item.machine, &replay_info),
                T64_STATUS_NOT_READY);
  T64_CHECK(memcmp(&replay_info, &untouched, sizeof replay_info) == 0);
  length = 0xa5a5u;
  T64_CHECK_INT(t64_video_copy(item.machine, packet, sizeof packet, &length),
                T64_STATUS_NOT_READY);
  T64_CHECK_INT(length, 0xa5a5);
  poison_audio_info(&audio);
  T64_CHECK_INT(t64_audio_info(item.machine, &audio), T64_STATUS_NOT_READY);

  /* The same authored setup after reset gives the same first packet. */
  T64_CHECK(enable_video(item.machine));
  replay_info = (t64_video_packet_info)T64_VIDEO_PACKET_INFO_INIT;
  T64_CHECK(run_until_video_packet(item.machine, &replay_info));
  T64_CHECK_INT(replay_info.sequence, first_info.sequence);
  T64_CHECK_INT(replay_info.start_tick, first_info.start_tick);
  T64_CHECK_INT(replay_info.end_tick, first_info.end_tick);
  T64_CHECK_INT(replay_info.packet_bytes, first_info.packet_bytes);
  destroy_and_check(&item);
}

/* Refusals: no image, NULL machine; each leaves allocator counters unchanged
 * and a later admission still works. */
static void reset_refusals(void) {
  instance item;
  size_t allocations;
  size_t releases;
  uint32_t poisoned = 99u;
  uint8_t admitted[T64_DIGEST_SIZE];
  uint8_t fresh[T64_DIGEST_SIZE];
  instance other;

  T64_CHECK_INT(t64_machine_reset(NULL), T64_STATUS_INVALID_ARGUMENT);
  if (!make_instance(&item, HELLO_PROFILE, NULL, 0u)) return;
  allocations = item.counts.allocations;
  releases = item.counts.releases;
  T64_CHECK_INT(t64_machine_reset(item.machine), T64_STATUS_NOT_READY);
  T64_CHECK_INT(item.counts.allocations, allocations);
  T64_CHECK_INT(item.counts.releases, releases);
  T64_CHECK_INT(t64_machine_is_poisoned(item.machine, &poisoned),
                T64_STATUS_OK);
  T64_CHECK_INT(poisoned, 0);
  T64_CHECK_INT(t64_machine_admit_rom(item.machine, hello_rom, hello_length),
                T64_STATUS_OK);
  T64_CHECK(digest_of(item.machine, admitted));
  if (make_instance(&other, HELLO_PROFILE, hello_rom, hello_length)) {
    T64_CHECK(digest_of(other.machine, fresh));
    T64_CHECK(same_digest(admitted, fresh));
    destroy_and_check(&other);
  }
  destroy_and_check(&item);
}

static void case_reset(void) {
  reset_isviewer_scenario();
  reset_rings_scenario();
  reset_refusals();
}

/* ---- unload ------------------------------------------------------------ */

/* The digest of an instance that admitted `image` and ran to `tick` (0 for no
 * run), taken from a fresh instance of the profile. */
static bool fresh_digest(const char *profile_id, const uint8_t *image,
                         size_t image_length, uint64_t tick,
                         uint8_t digest[T64_DIGEST_SIZE]) {
  instance fresh;
  bool ok;

  if (!make_instance(&fresh, profile_id, image, image_length)) return false;
  if (tick != 0u) (void)advance_to(fresh.machine, tick);
  ok = digest_of(fresh.machine, digest);
  destroy_and_check(&fresh);
  return ok;
}

static void unload_accepted(const char *profile_id, const uint8_t *image,
                            size_t image_length, const uint8_t *other_image,
                            size_t other_length) {
  instance item;
  size_t allocations;
  size_t releases;
  size_t live;
  size_t image_block_size;
  uint8_t fresh[T64_DIGEST_SIZE];
  uint8_t again[T64_DIGEST_SIZE];

  if (!make_instance(&item, profile_id, image, image_length)) return;
  /* The image block is the allocation the admission made last. */
  image_block_size = item.counts.live_size[item.counts.live_count - 1u];
  T64_CHECK(image_block_size >= image_length);
  (void)advance_to(item.machine, UINT64_C(60000));
  allocations = item.counts.allocations;
  releases = item.counts.releases;
  live = item.counts.live_count;

  T64_CHECK_INT(t64_machine_unload(item.machine), T64_STATUS_OK);
  /* Exactly the image storage went back to the host, and nothing was taken. */
  T64_CHECK_INT(item.counts.allocations, allocations);
  T64_CHECK_INT(item.counts.releases, releases + 1u);
  T64_CHECK_INT(item.counts.live_count, live - 1u);
  T64_CHECK_INT(item.counts.last_release_size, image_block_size);
  T64_CHECK_INT(item.counts.unknown_releases, 0);

  /* Admission after an unload equals a fresh admission, now and after a run. */
  T64_CHECK_INT(t64_machine_admit_rom(item.machine, image, image_length),
                T64_STATUS_OK);
  T64_CHECK(digest_of(item.machine, again));
  T64_CHECK(fresh_digest(profile_id, image, image_length, 0u, fresh));
  T64_CHECK(same_digest(fresh, again));
  (void)advance_to(item.machine, UINT64_C(60000));
  T64_CHECK(digest_of(item.machine, again));
  T64_CHECK(fresh_digest(profile_id, image, image_length, UINT64_C(60000),
                         fresh));
  T64_CHECK(same_digest(fresh, again));

  /* Unload, then a different image: no state of the first one survives. */
  T64_CHECK_INT(t64_machine_unload(item.machine), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_admit_rom(item.machine, other_image, other_length),
                T64_STATUS_OK);
  T64_CHECK(digest_of(item.machine, again));
  T64_CHECK(fresh_digest(profile_id, other_image, other_length, 0u, fresh));
  T64_CHECK_INT(item.counts.unknown_releases, 0);
  T64_CHECK(same_digest(fresh, again));

  /* Unload then destroy: every block is back, none twice. */
  T64_CHECK_INT(t64_machine_unload(item.machine), T64_STATUS_OK);
  destroy_and_check(&item);
}

static void unload_refused(void) {
  instance item;
  size_t allocations;
  size_t releases;
  size_t live;
  uint32_t poisoned = 99u;

  T64_CHECK_INT(t64_machine_unload(NULL), T64_STATUS_INVALID_ARGUMENT);
  /* No image yet. */
  if (!make_instance(&item, HELLO_PROFILE, NULL, 0u)) return;
  allocations = item.counts.allocations;
  releases = item.counts.releases;
  live = item.counts.live_count;
  T64_CHECK_INT(t64_machine_unload(item.machine), T64_STATUS_NOT_READY);
  T64_CHECK_INT(item.counts.allocations, allocations);
  T64_CHECK_INT(item.counts.releases, releases);
  T64_CHECK_INT(item.counts.live_count, live);
  T64_CHECK_INT(t64_machine_is_poisoned(item.machine, &poisoned),
                T64_STATUS_OK);
  T64_CHECK_INT(poisoned, 0);
  /* Unloaded twice: the second call is the no-image refusal. */
  T64_CHECK_INT(t64_machine_admit_rom(item.machine, hello_rom, hello_length),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_unload(item.machine), T64_STATUS_OK);
  releases = item.counts.releases;
  live = item.counts.live_count;
  T64_CHECK_INT(t64_machine_unload(item.machine), T64_STATUS_NOT_READY);
  T64_CHECK_INT(item.counts.releases, releases);
  T64_CHECK_INT(item.counts.live_count, live);
  destroy_and_check(&item);
}

static void case_unload(void) {
  unload_accepted(HELLO_PROFILE, hello_rom, hello_length, fill_rom,
                  fill_length);
  unload_accepted(VI_PROFILE, fill_rom, fill_length, hello_rom, hello_length);
  unload_refused();
}

/* ---- the NOT_READY table ------------------------------------------------ */

/* Every out parameter of every machine-taking entry, one scratch record, filled
 * with 0xA5 before a call and compared byte for byte afterwards. */
typedef struct {
  t64_stop_result stop_result;
  uint8_t digest[T64_DIGEST_SIZE];
  size_t size;
  uint8_t header[T64_ROM_HEADER_SIZE];
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
  t64_output_status output_status;
} scratch;

/* The whole record at 0xA5, then every in/out member of a public struct type
 * re-poisoned from its INIT form (see the sentinels above), so a leading
 * struct_size stays valid and the members still hold a non-zero sentinel. */
static void scratch_poison(scratch *s) {
  (void)memset(s, 0xa5, sizeof *s);
  poison_stop_result(&s->stop_result);
  poison_import_report(&s->report);
  poison_cpu_stop_detail(&s->detail);
  poison_drain_result(&s->drain);
  poison_video_info(&s->video_info);
  poison_save_info(&s->save_info);
  poison_audio_info(&s->audio_info);
  poison_output_status(&s->output_status);
}

enum {
  E_ADMIT_ROM,
  E_ADVANCE,
  E_CONTENT_DIGEST,
  E_ROM_SIZE,
  E_ROM_HEADER,
  E_PROFILE,
  E_DIGEST,
  E_STATE_EXPORT,
  E_STATE_IMPORT,
  E_CPU_STOP_DETAIL,
  E_OUTPUT_DRAIN,
  E_VIDEO_INFO,
  E_VIDEO_COPY,
  E_SAVE_INFO,
  E_SAVE_EXPORT,
  E_SAVE_IMPORT,
  E_AUDIO_INFO,
  E_AUDIO_COPY,
  E_QUEUE_INPUT,
  E_BUS_READ_RAM,
  E_BUS_READ_CART,
  E_BUS_WRITE_RAM,
  E_BUS_WRITE_CART,
  E_TRACE_SET_ENABLED,
  E_TRACE_DRAIN,
  E_RESET,
  E_UNLOAD,
  E_OUTPUT_STATUS,
  E_IS_POISONED,
  E_COUNT
};

/* The decided answer of every entry on an instance whose image was unloaded.
 * Entries that need an image answer NOT_READY with every output untouched;
 * the others do not depend on an image (decided from the code of each entry,
 * recorded in PIN-IMPACT.md "Shape steps", row 11-04). */
typedef struct {
  unsigned entry;
  const char *name;
  t64_status after_unload;
  bool outputs_untouched;
} table_row;

static const table_row table[E_COUNT] = {
    {E_ADMIT_ROM, "admit_rom", T64_STATUS_OK, false},
    {E_ADVANCE, "advance", T64_STATUS_NOT_READY, true},
    {E_CONTENT_DIGEST, "content_digest", T64_STATUS_NOT_READY, true},
    {E_ROM_SIZE, "rom_size", T64_STATUS_NOT_READY, true},
    {E_ROM_HEADER, "rom_header", T64_STATUS_NOT_READY, true},
    {E_PROFILE, "profile", T64_STATUS_OK, false},
    {E_DIGEST, "digest", T64_STATUS_NOT_READY, true},
    {E_STATE_EXPORT, "state_export", T64_STATUS_NOT_READY, true},
    /* The import report is the caller record of the attempt, so a refusal
     * fills it (reason NOT_ADMITTED); no other byte is written. */
    {E_STATE_IMPORT, "state_import", T64_STATUS_NOT_READY, false},
    {E_CPU_STOP_DETAIL, "cpu_stop_detail", T64_STATUS_NOT_READY, true},
    {E_OUTPUT_DRAIN, "output_drain", T64_STATUS_NOT_READY, true},
    {E_VIDEO_INFO, "video_info", T64_STATUS_NOT_READY, true},
    {E_VIDEO_COPY, "video_copy", T64_STATUS_NOT_READY, true},
    {E_SAVE_INFO, "save_info", T64_STATUS_NOT_READY, true},
    {E_SAVE_EXPORT, "save_export", T64_STATUS_NOT_READY, true},
    {E_SAVE_IMPORT, "save_import", T64_STATUS_NOT_READY, true},
    {E_AUDIO_INFO, "audio_info", T64_STATUS_NOT_READY, true},
    {E_AUDIO_COPY, "audio_copy", T64_STATUS_NOT_READY, true},
    {E_QUEUE_INPUT, "queue_input", T64_STATUS_NOT_READY, true},
    {E_BUS_READ_RAM, "bus_read (RDRAM)", T64_STATUS_OK, false},
    /* Not gated on an image: the window has no bytes while none is admitted,
     * and the bus answers BUS_UNSUPPORTED as it does on a new instance. */
    {E_BUS_READ_CART, "bus_read (cartridge window)", T64_STATUS_BUS_UNSUPPORTED,
     true},
    {E_BUS_WRITE_RAM, "bus_write (RDRAM)", T64_STATUS_OK, false},
    {E_BUS_WRITE_CART, "bus_write (cartridge window)",
     T64_STATUS_BUS_DEVICE_REFUSED, false},
    {E_TRACE_SET_ENABLED, "trace_set_enabled", T64_STATUS_OK, false},
    {E_TRACE_DRAIN, "trace_drain", T64_STATUS_OK, false},
    {E_RESET, "reset", T64_STATUS_NOT_READY, true},
    {E_UNLOAD, "unload", T64_STATUS_NOT_READY, true},
    /* Image-independent: the rings are empty and the text is 0 bytes, so the
     * answer is OK with zero counts (checked by the output-status case). */
    {E_OUTPUT_STATUS, "output_status", T64_STATUS_OK, false},
    {E_IS_POISONED, "is_poisoned", T64_STATUS_OK, false},
};

static uint32_t table_save_kind = 1u;

static t64_status call_table_entry(unsigned entry, t64_machine *machine,
                                   scratch *s) {
  t64_stop_request request = T64_STOP_REQUEST_INIT(0u, 0u);
  uint8_t blob[64];

  request.kind = T64_STOP_GUEST_TICK;
  request.value = UINT64_C(100000);
  (void)memset(blob, 0x5a, sizeof blob);
  switch (entry) {
  case E_ADMIT_ROM:
    return t64_machine_admit_rom(machine, hello_rom, hello_length);
  case E_ADVANCE:
    return t64_machine_advance(machine, &request, &s->stop_result);
  case E_CONTENT_DIGEST:
    return t64_machine_content_digest(machine, s->digest);
  case E_ROM_SIZE:
    return t64_machine_rom_size(machine, &s->size);
  case E_ROM_HEADER:
    return t64_machine_rom_header(machine, s->header, sizeof s->header,
                                  &s->header_length);
  case E_PROFILE:
    return t64_machine_profile(machine, &s->profile);
  case E_DIGEST:
    return t64_machine_digest(machine, s->digest);
  case E_STATE_EXPORT:
    return t64_machine_state_export(machine, s->export_bytes,
                                    sizeof s->export_bytes, &s->export_length);
  case E_STATE_IMPORT:
    return t64_machine_state_import(machine, blob, sizeof blob, 0u,
                                    &s->report);
  case E_CPU_STOP_DETAIL:
    return t64_machine_cpu_stop_detail(machine, &s->detail);
  case E_OUTPUT_DRAIN:
    return t64_machine_output_drain(machine, s->drain_bytes,
                                    sizeof s->drain_bytes, &s->drain);
  case E_VIDEO_INFO:
    return t64_video_info(machine, &s->video_info);
  case E_VIDEO_COPY:
    return t64_video_copy(machine, s->video_bytes, sizeof s->video_bytes,
                          &s->video_length);
  case E_SAVE_INFO:
    return t64_machine_save_info(machine, table_save_kind, &s->save_info);
  case E_SAVE_EXPORT:
    return t64_machine_save_export(machine, table_save_kind,
                                   T64_SAVE_FORMAT_RAW, s->save_bytes,
                                   sizeof s->save_bytes, &s->save_length);
  case E_SAVE_IMPORT:
    return t64_machine_save_import(machine, table_save_kind,
                                   T64_SAVE_FORMAT_RAW, blob, sizeof blob);
  case E_AUDIO_INFO:
    return t64_audio_info(machine, &s->audio_info);
  case E_AUDIO_COPY:
    return t64_audio_copy(machine, s->audio_bytes, sizeof s->audio_bytes,
                          &s->audio_length);
  case E_QUEUE_INPUT:
    return t64_machine_queue_input(machine, UINT64_C(200000), 0u, 0u, 0, 0);
  case E_BUS_READ_RAM:
    return t64_machine_bus_read(machine, UINT32_C(0x100), 4u, &s->bus_value);
  case E_BUS_READ_CART:
    return t64_machine_bus_read(machine, UINT32_C(0x10000000), 4u,
                                &s->bus_value);
  case E_BUS_WRITE_RAM:
    return t64_machine_bus_write(machine, UINT32_C(0x100), 4u,
                                 UINT32_C(0xdeadbeef));
  case E_BUS_WRITE_CART:
    return t64_machine_bus_write(machine, UINT32_C(0x10000000), 4u,
                                 UINT32_C(0xdeadbeef));
  case E_TRACE_SET_ENABLED:
    return t64_machine_trace_set_enabled(machine, true);
  case E_TRACE_DRAIN:
    return t64_machine_trace_drain(machine, s->records,
                                   (uint32_t)sizeof(t64_trace_record), 2u, &s->trace_count,
                                   &s->trace_dropped, &s->trace_saturated);
  case E_RESET:
    return t64_machine_reset(machine);
  case E_UNLOAD:
    return t64_machine_unload(machine);
  case E_OUTPUT_STATUS:
    s->output_status.struct_size = T64_OUTPUT_STATUS_V1_SIZE;
    return t64_machine_output_status(machine, &s->output_status);
  case E_IS_POISONED: {
    uint32_t poisoned = 99u;
    const t64_status status = t64_machine_is_poisoned(machine, &poisoned);

    return status == T64_STATUS_OK && poisoned == 0u
               ? T64_STATUS_OK
               : T64_STATUS_INTERNAL_DEFECT;
  }
  default:
    return T64_STATUS_INVALID_ARGUMENT;
  }
}

/* One instance of the profile, unloaded; run every entry once against it and
 * require the table's class. Entries that change the instance (admit_rom,
 * advance, reset) are not required to leave it unloaded, so each row gets its
 * own instance. */
static void table_for_profile(const char *profile_id) {
  unsigned row;

  for (row = 0u; row < E_COUNT; row++) {
    instance item;
    scratch actual;
    scratch expected;
    scratch fresh_scratch;
    t64_status status;
    size_t allocations;
    size_t releases;
    const t64_profile_descriptor *profile = NULL;

    if (!make_instance(&item, profile_id, hello_rom, hello_length)) return;
    T64_CHECK_INT(t64_machine_profile(item.machine, &profile), T64_STATUS_OK);
    table_save_kind =
        profile != NULL && profile->save_kind != 0u ? profile->save_kind : 1u;
    T64_CHECK_INT(t64_machine_unload(item.machine), T64_STATUS_OK);
    allocations = item.counts.allocations;
    releases = item.counts.releases;
    scratch_poison(&actual);
    (void)memcpy(&expected, &actual, sizeof expected);
    status = call_table_entry(table[row].entry, item.machine, &actual);
    if (status != table[row].after_unload) {
      fprintf(stderr, "%s unloaded %s: status %u, table says %u\n", profile_id,
              table[row].name, (unsigned)status,
              (unsigned)table[row].after_unload);
    }
    T64_CHECK_INT(table[row].entry, row);
    T64_CHECK_INT(status, table[row].after_unload);
    if (table[row].entry == E_STATE_IMPORT) {
      T64_CHECK_INT(actual.report.status, T64_STATUS_NOT_READY);
      T64_CHECK_INT(actual.report.reason, T64_STATE_REASON_NOT_ADMITTED);
    }
    if (table[row].outputs_untouched) {
      if (memcmp(&actual, &expected, sizeof actual) != 0) {
        fprintf(stderr, "%s unloaded %s: an output byte changed\n", profile_id,
                table[row].name);
      }
      T64_CHECK(memcmp(&actual, &expected, sizeof actual) == 0);
      /* A refusal takes nothing from and gives nothing to the host. */
      T64_CHECK_INT(item.counts.allocations, allocations);
      T64_CHECK_INT(item.counts.releases, releases);
    }
    destroy_and_check(&item);

    /* Unload leaves no image-derived state: an instance that never admitted
     * an image gives the same answer to the same call. */
    if (make_instance(&item, profile_id, NULL, 0u)) {
      scratch_poison(&fresh_scratch);
      T64_CHECK_INT(
          call_table_entry(table[row].entry, item.machine, &fresh_scratch),
          table[row].after_unload);
      destroy_and_check(&item);
    }
  }
}

/* ---- the advance span rule (plan 11-27, finding F-20-A) ------------------------
 *
 * The header's check order for every entry that takes a struct puts a span that
 * wraps or overlaps the instance before the struct is read, before the
 * struct_size answer and before the admission answer, and names
 * t64_machine_advance among the entries that judge their request and their
 * result that way. The instance owns the blocks its allocator returned (the
 * machine storage, the admitted image, the packet storages); the counting
 * allocator lists them, so the test can put a request or a result inside each of
 * them with the public API alone.
 *
 * Four stop objects are written into the block, 16 bytes apart (a valid
 * request, a request whose struct_size is 0, a poisoned result and a result
 * whose struct_size is 0), the original bytes are saved first and written back
 * last, and the machine digest is taken after the placement: the placement is
 * the test's own deliberate write, and what must stay equal is everything the
 * refused calls could have changed. A refused call must answer
 * INVALID_ARGUMENT, write no placed byte, leave a result outside the instance
 * as the caller left it, leave the digest unchanged and not poison the
 * instance. */
#define SPAN_PLACED_BYTES 64u

static size_t span_offset(size_t block_size) {
  return ((block_size - SPAN_PLACED_BYTES) / 2u) & ~(size_t)15u;
}

static bool is_poisoned_now(const t64_machine *machine) {
  uint32_t poisoned = 99u;

  T64_CHECK_INT(t64_machine_is_poisoned(machine, &poisoned), T64_STATUS_OK);
  return poisoned != 0u;
}

static void advance_span_block(instance *item, size_t block, bool admitted) {
  uint8_t *base = (uint8_t *)item->counts.live[block];
  const size_t size = item->counts.live_size[block];
  uint8_t *where;
  uint8_t saved[SPAN_PLACED_BYTES];
  uint8_t placed[SPAN_PLACED_BYTES];
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  t64_stop_request request_valid =
      T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, 1000u);
  t64_stop_request request_zero =
      T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, 1000u);
  t64_stop_result result_poisoned = T64_STOP_RESULT_INIT;
  t64_stop_result result_zero = T64_STOP_RESULT_INIT;
  t64_stop_request outside_request =
      T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, 1000u);
  t64_stop_request outside_zero_request =
      T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, 1000u);
  t64_stop_result outside_result = T64_STOP_RESULT_INIT;
  t64_stop_result outside_expected = T64_STOP_RESULT_INIT;

  T64_CHECK(base != NULL);
  T64_CHECK(size >= 2u * SPAN_PLACED_BYTES);
  if (base == NULL || size < 2u * SPAN_PLACED_BYTES) return;
  where = base + span_offset(size);
  (void)memcpy(saved, where, sizeof saved);

  request_zero = request_valid;
  request_zero.struct_size = 0u;
  poison_stop_result(&result_poisoned);
  result_zero = result_poisoned;
  result_zero.struct_size = 0u;
  (void)memcpy(placed + 0u, &request_valid, sizeof request_valid);
  (void)memcpy(placed + 16u, &request_zero, sizeof request_zero);
  (void)memcpy(placed + 32u, &result_poisoned, sizeof result_poisoned);
  (void)memcpy(placed + 48u, &result_zero, sizeof result_zero);
  (void)memcpy(where, placed, sizeof placed);
  outside_zero_request.struct_size = 0u;
  poison_stop_result(&outside_result);
  outside_expected = outside_result;

  if (admitted) {
    T64_CHECK(digest_of(item->machine, before));
  }

  /* A request inside the block: the span answer wins over a valid struct_size
   * and over a struct_size of 0 that would otherwise be too small. */
  T64_CHECK_INT(t64_machine_advance(
                    item->machine, (const t64_stop_request *)(const void *)where,
                    &outside_result),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK(memcmp(&outside_result, &outside_expected,
                   sizeof outside_result) == 0);
  T64_CHECK_INT(t64_machine_advance(
                    item->machine,
                    (const t64_stop_request *)(const void *)(where + 16u),
                    &outside_result),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK(memcmp(&outside_result, &outside_expected,
                   sizeof outside_result) == 0);
  /* A result inside the block, the request outside (valid, then too small). */
  T64_CHECK_INT(t64_machine_advance(
                    item->machine, &outside_request,
                    (t64_stop_result *)(void *)(where + 32u)),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK_INT(t64_machine_advance(
                    item->machine, &outside_zero_request,
                    (t64_stop_result *)(void *)(where + 48u)),
                T64_STATUS_INVALID_ARGUMENT);
  /* Both inside. */
  T64_CHECK_INT(t64_machine_advance(
                    item->machine, (const t64_stop_request *)(const void *)where,
                    (t64_stop_result *)(void *)(where + 32u)),
                T64_STATUS_INVALID_ARGUMENT);
  /* The other side of the order: with both spans outside the instance, a
   * request below the v1 size is STRUCT_TOO_SMALL, before the admission
   * answer. */
  T64_CHECK_INT(t64_machine_advance(item->machine, &outside_zero_request,
                                    &outside_result),
                T64_STATUS_STRUCT_TOO_SMALL);
  T64_CHECK(memcmp(&outside_result, &outside_expected,
                   sizeof outside_result) == 0);

  /* No placed byte was written, the instance is not poisoned and (when it has
   * an image to hash) the digest is the one taken after the placement. */
  T64_CHECK(memcmp(where, placed, sizeof placed) == 0);
  T64_CHECK(!is_poisoned_now(item->machine));
  if (admitted) {
    T64_CHECK(digest_of(item->machine, after));
    T64_CHECK(same_digest(before, after));
  }
  (void)memcpy(where, saved, sizeof saved);
}

static void advance_span(const char *profile_id, bool admit) {
  instance item;
  size_t block;
  size_t blocks;

  if (!make_instance(&item, profile_id, admit ? hello_rom : NULL,
                     admit ? hello_length : 0u)) {
    return;
  }
  blocks = item.counts.live_count;
  /* The machine storage, and with an image the admitted image; a profile with
   * video or audio output also owns the packet storages. */
  T64_CHECK(blocks >= (admit ? 2u : 1u));
  for (block = 0u; block < blocks; block++) {
    advance_span_block(&item, block, admit);
  }
  destroy_and_check(&item);
}

static void case_not_ready_table(void) {
  /* The legacy skeleton-class profile (no VI, AI or SI), the composite family
   * profile with the 4 KiB EEPROM, and the VI and AI profiles. */
  table_for_profile(HELLO_PROFILE);
  table_for_profile("ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e4k");
  table_for_profile(VI_PROFILE);
  table_for_profile("ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1");
  /* The advance span rule: before the struct_size and the admission answers,
   * with and without an image, with and without packet storages. */
  advance_span(HELLO_PROFILE, true);
  advance_span(HELLO_PROFILE, false);
  advance_span("ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1", true);
}

/* ---- output status ------------------------------------------------------ */

/* The ring capacity, from the public header: raw video and audio are each "a
 * bounded, lossless, two-packet queue". */
#define RING_CAPACITY 2u

static bool read_status(const t64_machine *machine, t64_output_status *out) {
  t64_output_status status = T64_OUTPUT_STATUS_INIT;
  const t64_status result = t64_machine_output_status(machine, &status);

  T64_CHECK_INT(result, T64_STATUS_OK);
  /* The callee never writes struct_size and writes the pad member as 0. */
  T64_CHECK_INT(status.struct_size, T64_OUTPUT_STATUS_V1_SIZE);
  T64_CHECK_INT(status.reserved0, 0);
  *out = status;
  return result == T64_STATUS_OK;
}

/* The status agrees with the oldest packet the info and copy calls report, and
 * the two rings account for their full capacity. */
static void check_status_against_rings(t64_machine *machine,
                                       const t64_output_status *status) {
  t64_video_packet_info video = T64_VIDEO_PACKET_INFO_INIT;
  t64_audio_packet_info audio = T64_AUDIO_PACKET_INFO_INIT;
  size_t required = 0u;

  T64_CHECK_INT(status->video_queued + status->video_free, RING_CAPACITY);
  T64_CHECK_INT(status->audio_queued + status->audio_free, RING_CAPACITY);
  video = (t64_video_packet_info)T64_VIDEO_PACKET_INFO_INIT;
  audio = (t64_audio_packet_info)T64_AUDIO_PACKET_INFO_INIT;
  if (status->video_queued != 0u) {
    T64_CHECK_INT(t64_video_info(machine, &video), T64_STATUS_OK);
    T64_CHECK_INT(status->video_next_bytes, video.packet_bytes);
    T64_CHECK_INT(t64_video_copy(machine, NULL, 0u, &required),
                  T64_STATUS_BUFFER_TOO_SMALL);
    T64_CHECK_INT(status->video_next_bytes, required);
  } else {
    T64_CHECK_INT(t64_video_info(machine, &video), T64_STATUS_NOT_READY);
    T64_CHECK_INT(status->video_next_bytes, 0);
  }
  if (status->audio_queued != 0u) {
    T64_CHECK_INT(t64_audio_info(machine, &audio), T64_STATUS_OK);
    T64_CHECK_INT(status->audio_next_bytes, audio.byte_length);
    required = 0u;
    T64_CHECK_INT(t64_audio_copy(machine, NULL, 0u, &required),
                  T64_STATUS_BUFFER_TOO_SMALL);
    T64_CHECK_INT(status->audio_next_bytes, required);
  } else {
    T64_CHECK_INT(t64_audio_info(machine, &audio), T64_STATUS_NOT_READY);
    T64_CHECK_INT(status->audio_next_bytes, 0);
  }
}

static void status_empty(const char *profile_id, const uint8_t *image,
                         size_t image_length) {
  instance item;
  t64_output_status status = T64_OUTPUT_STATUS_INIT;
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];

  if (!make_instance(&item, profile_id, image, image_length)) return;
  if (image != NULL) T64_CHECK(digest_of(item.machine, before));
  if (read_status(item.machine, &status)) {
    T64_CHECK_INT(status.video_queued, 0);
    T64_CHECK_INT(status.video_free, RING_CAPACITY);
    T64_CHECK_INT(status.video_next_bytes, 0);
    T64_CHECK_INT(status.audio_queued, 0);
    T64_CHECK_INT(status.audio_free, RING_CAPACITY);
    T64_CHECK_INT(status.audio_next_bytes, 0);
    T64_CHECK_INT(status.text_pending_bytes, 0);
  }
  if (image != NULL) {
    T64_CHECK(digest_of(item.machine, after));
    T64_CHECK(same_digest(before, after));
  }
  destroy_and_check(&item);
}

/* The ring counts follow a real run: the audio fixture drives both rings of
 * the VI and AI profile. */
static void status_tracks_rings(void) {
  instance item;
  t64_output_status status = T64_OUTPUT_STATUS_INIT;
  t64_output_status again = T64_OUTPUT_STATUS_INIT;
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  uint8_t bytes[320u * 240u * 4u];
  size_t length = 0u;
  unsigned most_video = 0u;
  unsigned most_audio = 0u;
  unsigned stride;
  uint64_t tick = 0u;

  if (!make_instance(&item,
                     "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1",
                     audio_rom, audio_length)) {
    return;
  }
  for (stride = 0u; stride < 24u; stride++) {
    tick += UINT64_C(100000);
    (void)advance_to(item.machine, tick);
    T64_CHECK(digest_of(item.machine, before));
    if (!read_status(item.machine, &status)) break;
    check_status_against_rings(item.machine, &status);
    /* The call changed nothing, and a second call agrees with the first. */
    T64_CHECK(digest_of(item.machine, after));
    T64_CHECK(same_digest(before, after));
    T64_CHECK(read_status(item.machine, &again));
    T64_CHECK(memcmp(&status, &again, sizeof status) == 0);
    if (status.video_queued > most_video) most_video = status.video_queued;
    if (status.audio_queued > most_audio) most_audio = status.audio_queued;
    if (status.video_queued != 0u) {
      /* One copy moves the video ring by exactly one packet. */
      const unsigned queued = status.video_queued;
      const uint64_t next = status.video_next_bytes;

      T64_CHECK_INT(t64_video_copy(item.machine, bytes, sizeof bytes, &length),
                    T64_STATUS_OK);
      T64_CHECK_INT(length, next);
      if (read_status(item.machine, &again)) {
        T64_CHECK_INT(again.video_queued, queued - 1u);
        T64_CHECK_INT(again.video_free, status.video_free + 1u);
        T64_CHECK_INT(again.audio_queued, status.audio_queued);
        check_status_against_rings(item.machine, &again);
      }
    }
    if (status.audio_queued != 0u) {
      const unsigned queued = status.audio_queued;
      const uint64_t next = status.audio_next_bytes;

      T64_CHECK_INT(t64_audio_copy(item.machine, bytes, sizeof bytes, &length),
                    T64_STATUS_OK);
      T64_CHECK_INT(length, next);
      if (read_status(item.machine, &again)) {
        T64_CHECK_INT(again.audio_queued, queued - 1u);
        T64_CHECK_INT(again.audio_free, status.audio_free + 1u);
        check_status_against_rings(item.machine, &again);
      }
    }
  }
  /* The run really filled both rings (otherwise the counts above were all 0). */
  T64_CHECK(most_video >= 1u);
  T64_CHECK(most_audio >= 1u);
  destroy_and_check(&item);
}

/* ISViewer: the pending text counts down to 0 on a drain. */
static void status_text_pending(void) {
  instance item;
  t64_output_status status = T64_OUTPUT_STATUS_INIT;
  uint8_t drained[HELLO_BYTES + 2u];
  t64_output_drain_result drain = T64_OUTPUT_DRAIN_RESULT_INIT;

  if (!make_instance(&item, HELLO_PROFILE, hello_rom, hello_length)) return;
  if (read_status(item.machine, &status)) {
    T64_CHECK_INT(status.text_pending_bytes, 0);
  }
  T64_CHECK_INT(advance_to(item.machine, HELLO_BOUND_TICK),
                T64_STOP_REASON_GUEST_COMPLETION);
  if (read_status(item.machine, &status)) {
    T64_CHECK_INT(status.text_pending_bytes, HELLO_BYTES);
  }
  drain = (t64_output_drain_result)T64_OUTPUT_DRAIN_RESULT_INIT;
  T64_CHECK_INT(
      t64_machine_output_drain(item.machine, drained, sizeof drained, &drain),
      T64_STATUS_OK);
  T64_CHECK_INT(drain.copied_bytes, HELLO_BYTES);
  if (read_status(item.machine, &status)) {
    T64_CHECK_INT(status.text_pending_bytes, 0);
  }
  destroy_and_check(&item);
}

/* Refusals: a struct_size below the v1 size, a NULL output and a NULL machine
 * leave the caller struct byte for byte as it was and the digest unchanged;
 * a larger caller struct is written only up to the known size. */
static void status_refused(void) {
  instance item;
  t64_output_status status = T64_OUTPUT_STATUS_INIT;
  t64_output_status sentinel = T64_OUTPUT_STATUS_INIT;
  struct {
    t64_output_status known;
    uint8_t tail[24];
  } larger;
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  static const uint32_t sizes[] = {0u, 1u, 4u, 47u};
  size_t index;

  T64_CHECK_INT(T64_OUTPUT_STATUS_V1_SIZE, sizeof(t64_output_status));
  T64_CHECK(offsetof(t64_output_status, struct_size) == 0u);
  if (!make_instance(&item, VI_PROFILE, fill_rom, fill_length)) return;
  T64_CHECK(digest_of(item.machine, before));

  for (index = 0u; index < sizeof sizes / sizeof sizes[0]; index++) {
    poison_output_status(&status);
    status.struct_size = sizes[index];
    (void)memcpy(&sentinel, &status, sizeof sentinel);
    T64_CHECK_INT(t64_machine_output_status(item.machine, &status),
                  T64_STATUS_STRUCT_TOO_SMALL);
    T64_CHECK(memcmp(&status, &sentinel, sizeof status) == 0);
  }
  poison_output_status(&status);
  status.struct_size = T64_OUTPUT_STATUS_V1_SIZE;
  (void)memcpy(&sentinel, &status, sizeof sentinel);
  T64_CHECK_INT(t64_machine_output_status(item.machine, NULL),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK_INT(t64_machine_output_status(NULL, &status),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK(memcmp(&status, &sentinel, sizeof status) == 0);

  /* A caller struct larger than v1: the members it knows are filled, the
   * struct_size and the bytes past the known size are not touched. */
  poison_output_status(&larger.known);
  (void)memset(larger.tail, 0xa5, sizeof larger.tail);
  larger.known.struct_size = (uint32_t)sizeof larger;
  T64_CHECK_INT(t64_machine_output_status(item.machine, &larger.known),
                T64_STATUS_OK);
  T64_CHECK_INT(larger.known.struct_size, sizeof larger);
  T64_CHECK_INT(larger.known.video_free, RING_CAPACITY);
  T64_CHECK_INT(larger.known.audio_free, RING_CAPACITY);
  for (index = 0u; index < sizeof larger.tail; index++) {
    T64_CHECK_INT(larger.tail[index], 0xa5);
  }

  /* A caller that knows only the first 48 bytes of a hypothetical larger v2
   * struct sees the same v1 answer; a caller struct of exactly v1 size is the
   * one the other cases use. None of the above changed the machine. */
  T64_CHECK(digest_of(item.machine, after));
  T64_CHECK(same_digest(before, after));
  destroy_and_check(&item);
}

static void case_output_status(void) {
  status_empty(HELLO_PROFILE, NULL, 0u);
  status_empty(HELLO_PROFILE, hello_rom, hello_length);
  status_empty(VI_PROFILE, fill_rom, fill_length);
  status_tracks_rings();
  status_text_pending();
  status_refused();
}

static bool load_fixtures(void) {
  return read_fixture("hello.z64", hello_rom, sizeof hello_rom,
                      &hello_length) &&
         read_fixture("rdp-fill.z64", fill_rom, sizeof fill_rom,
                      &fill_length) &&
         read_fixture("audio-pcm.z64", audio_rom, sizeof audio_rom,
                      &audio_length);
}

int main(int argc, char **argv) {
  static const char *const cases[] = {"reset", "unload", "not-ready-table",
                                      "output-status"};
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
  if (!load_fixtures()) {
    fprintf(stderr, "T64_BOOT_FIXTURES_DIR does not name the fixtures\n");
    return 2;
  }
  if (selected == NULL || strcmp(selected, "reset") == 0) {
    case_reset();
    ran = true;
  }
  if (selected == NULL || strcmp(selected, "unload") == 0) {
    case_unload();
    ran = true;
  }
  if (selected == NULL || strcmp(selected, "not-ready-table") == 0) {
    case_not_ready_table();
    ran = true;
  }
  if (selected == NULL || strcmp(selected, "output-status") == 0) {
    case_output_status();
    ran = true;
  }
  if (!ran) {
    fprintf(stderr, "unknown case: %s\n", selected);
    return 2;
  }
  if (selected == NULL) {
    return T64_TEST_FINISH("unit.core.api.lifecycle");
  }
  if (strcmp(selected, "unload") == 0) {
    return T64_TEST_FINISH("unit.core.api.unload");
  }
  if (strcmp(selected, "not-ready-table") == 0) {
    return T64_TEST_FINISH("unit.core.api.not-ready-table");
  }
  if (strcmp(selected, "output-status") == 0) {
    return T64_TEST_FINISH("unit.core.api.output-status");
  }
  return T64_TEST_FINISH("unit.core.api.reset");
}
