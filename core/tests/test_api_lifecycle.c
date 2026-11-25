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
 *   not-ready-table  one table over every machine-taking entry after unload
 *   output-status    t64_machine_output_status reports the ring occupancy
 *                    (added by task 3 of the plan)
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_test.h"
#include "tendies64/t64_machine.h"

#define MAX_FIXTURE_BYTES (64u * 1024u)
#define MAX_LIVE 16u
#define HELLO_PROFILE "ntsc-cart-4m-openipl3-warm-v1"
#define VI_PROFILE "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9"
#define HELLO_TRANSCRIPT "Tendies64 hello via open IPL3\n"
#define HELLO_BYTES 30u
#define HELLO_BOUND_TICK UINT64_C(700000)

/* A counting allocator that remembers every live block and its size, so a
 * test can state exactly which storage a call returned to the host. */
typedef struct {
  size_t allocations;
  size_t releases;
  size_t unknown_releases;
  size_t live_count;
  void *live[MAX_LIVE];
  size_t live_size[MAX_LIVE];
  size_t last_release_size;
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
  state->live[state->live_count] = memory;
  state->live_size[state->live_count] = size;
  state->live_count++;
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
      state->last_release_size = state->live_size[index];
      state->live[index] = state->live[state->live_count - 1u];
      state->live_size[index] = state->live_size[state->live_count - 1u];
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

static uint8_t hello_rom[MAX_FIXTURE_BYTES];
static size_t hello_length;
static uint8_t fill_rom[MAX_FIXTURE_BYTES];
static size_t fill_length;

static bool read_fixture(const char *name, uint8_t *bytes, size_t capacity,
                         size_t *length_out) {
  const char *directory = getenv("T64_BOOT_FIXTURES_DIR");
  char path[512];
  FILE *file;
  int written;
  size_t length;
  int extra;

  if (directory == NULL || directory[0] == '\0') return false;
  written = snprintf(path, sizeof path, "%s/%s", directory, name);
  if (written < 0 || (size_t)written >= sizeof path) return false;
  file = fopen(path, "rb");
  if (file == NULL) return false;
  length = fread(bytes, 1u, capacity, file);
  extra = fgetc(file);
  if (ferror(file) != 0 || extra != EOF || length == 0u) {
    (void)fclose(file);
    return false;
  }
  if (fclose(file) != 0) return false;
  *length_out = length;
  return true;
}

/* Create an instance for the profile id; admit the image when asked. */
static bool make_instance(instance *item, const char *profile_id,
                          const uint8_t *image, size_t image_length) {
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
    t64_stop_request request;
    t64_stop_result result;
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
  t64_output_drain_result drain;
  t64_cpu_stop_detail detail;

  if (!make_instance(&item, HELLO_PROFILE, hello_rom, hello_length)) return;
  T64_CHECK(digest_of(item.machine, first_admit));
  (void)memset(&drain, 0, sizeof drain);
  T64_CHECK_INT(t64_machine_output_drain(item.machine, NULL, 0u, &drain),
                T64_STATUS_OK);
  T64_CHECK_INT(drain.copied_bytes, 0);
  T64_CHECK_INT(drain.required_bytes, 0);

  /* Run to the authored BREAK and drain nothing. */
  T64_CHECK_INT(advance_to(item.machine, HELLO_BOUND_TICK),
                T64_STOP_REASON_GUEST_COMPLETION);
  T64_CHECK(digest_of(item.machine, after_run));
  T64_CHECK(!same_digest(first_admit, after_run));
  (void)memset(&drain, 0, sizeof drain);
  T64_CHECK_INT(t64_machine_output_drain(item.machine, NULL, 0u, &drain),
                T64_STATUS_BUFFER_TOO_SMALL);
  T64_CHECK_INT(drain.required_bytes, HELLO_BYTES);
  T64_CHECK_INT(t64_machine_cpu_stop_detail(item.machine, &detail),
                T64_STATUS_OK);

  /* Reset: the first-admit digest, an empty cursor and a running CPU. */
  T64_CHECK_INT(t64_machine_reset(item.machine), T64_STATUS_OK);
  T64_CHECK(digest_of(item.machine, after_reset));
  T64_CHECK(same_digest(first_admit, after_reset));
  (void)memset(&detail, 0xa5, sizeof detail);
  T64_CHECK_INT(t64_machine_cpu_stop_detail(item.machine, &detail),
                T64_STATUS_NOT_READY);
  (void)memset(&drain, 0xa5, sizeof drain);
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
  (void)memset(&drain, 0, sizeof drain);
  T64_CHECK_INT(
      t64_machine_output_drain(item.machine, drained, sizeof drained, &drain),
      T64_STATUS_OK);
  T64_CHECK_INT(drain.copied_bytes, HELLO_BYTES);
  T64_CHECK(memcmp(drained, HELLO_TRANSCRIPT, HELLO_BYTES) == 0);

  /* Reset again after a drain: the cursor does not survive. */
  T64_CHECK_INT(t64_machine_reset(item.machine), T64_STATUS_OK);
  T64_CHECK(digest_of(item.machine, after_reset));
  T64_CHECK(same_digest(first_admit, after_reset));
  (void)memset(&drain, 0xa5, sizeof drain);
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
  t64_video_packet_info first_info;
  t64_video_packet_info replay_info;
  t64_video_packet_info untouched;
  t64_audio_packet_info audio;
  size_t length = 0u;

  if (!make_instance(&item, VI_PROFILE, fill_rom, fill_length)) return;
  T64_CHECK(digest_of(item.machine, first_admit));
  T64_CHECK(enable_video(item.machine));
  (void)memset(&first_info, 0, sizeof first_info);
  T64_CHECK(run_until_video_packet(item.machine, &first_info));
  T64_CHECK(first_info.packet_bytes != 0u);

  T64_CHECK_INT(t64_machine_reset(item.machine), T64_STATUS_OK);
  T64_CHECK(digest_of(item.machine, after_reset));
  T64_CHECK(same_digest(first_admit, after_reset));
  (void)memset(&untouched, 0xa5, sizeof untouched);
  (void)memcpy(&replay_info, &untouched, sizeof replay_info);
  T64_CHECK_INT(t64_video_info(item.machine, &replay_info),
                T64_STATUS_NOT_READY);
  T64_CHECK(memcmp(&replay_info, &untouched, sizeof replay_info) == 0);
  length = 0xa5a5u;
  T64_CHECK_INT(t64_video_copy(item.machine, packet, sizeof packet, &length),
                T64_STATUS_NOT_READY);
  T64_CHECK_INT(length, 0xa5a5);
  (void)memset(&audio, 0xa5, sizeof audio);
  T64_CHECK_INT(t64_audio_info(item.machine, &audio), T64_STATUS_NOT_READY);

  /* The same authored setup after reset gives the same first packet. */
  T64_CHECK(enable_video(item.machine));
  (void)memset(&replay_info, 0, sizeof replay_info);
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

static bool load_fixtures(void) {
  return read_fixture("hello.z64", hello_rom, sizeof hello_rom,
                      &hello_length) &&
         read_fixture("rdp-fill.z64", fill_rom, sizeof fill_rom,
                      &fill_length);
}

int main(int argc, char **argv) {
  static const char *const cases[] = {"reset"};
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
  if (!ran) {
    fprintf(stderr, "unknown case: %s\n", selected);
    return 2;
  }
  if (selected == NULL) {
    return T64_TEST_FINISH("unit.core.api.lifecycle");
  }
  return T64_TEST_FINISH("unit.core.api.reset");
}
