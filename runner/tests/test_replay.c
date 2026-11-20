/*
 * Phase 9 replay tests (plan 09-07). The authored replay-mix ROM polls the
 * modeled controller six times with a delay loop between polls; the committed
 * input script stamps one state in each gap. Expected transcripts, op lists and
 * identity literals are written out here (never taken from a production
 * helper); checkpoint digests are regression pins from a reference path that
 * does not use the replay module. Every timing figure is a named software
 * profile policy value; nothing is a hardware or timing claim.
 *
 * usage: test_replay CASE
 *        test_replay --suite unit
 *        test_replay --suite real SUPERVISOR TEST_SUPERVISOR CHECKER ROOT
 *        test_replay --list-cases
 *        test_replay --print-goldens [--mode prequeue|jit] [--step N]
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "t64_input.h"
#include "t64_json.h"
#include "t64_sha256.h"
#include "tendies64/t64_machine.h"

#define PROFILE_STEM "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1"
#define MIX_ROM_ID "phase09-replay-mix-si-v1"

static int g_failures;

static void check_impl(bool ok, const char *expr, int line) {
  if (!ok) {
    g_failures++;
    (void)fprintf(stderr, "FAIL line %d: %s\n", line, expr);
  }
}
#define CHECK(cond) check_impl((cond), #cond, __LINE__)

/* ---- files -------------------------------------------------------------- */

static bool read_file(const char *path, uint8_t *bytes, size_t capacity,
                      size_t *length_out) {
  int fd = open(path, O_RDONLY);
  size_t total = 0u;
  if (fd < 0) return false;
  while (total < capacity) {
    ssize_t count = read(fd, bytes + total, capacity - total);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) break;
    total += (size_t)count;
  }
  (void)close(fd);
  *length_out = total;
  return true;
}

static const char *env_or(const char *name, const char *fallback) {
  const char *value = getenv(name);
  return value != NULL && value[0] != '\0' ? value : fallback;
}

static const char *mix_rom_path(void) {
  return env_or("T64_REPLAY_MIX_ROM_PATH",
                "build/fixtures/" MIX_ROM_ID ".z64");
}

static const char *mix_input_path(void) {
  return env_or("T64_REPLAY_MIX_INPUT_PATH", "fixtures/replay/mix.input.json");
}

/* ---- in-process machines ------------------------------------------------ */

static bool host_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  void *memory = NULL;
  (void)context;
  if (storage_out == NULL || size == 0u || alignment == 0u) return false;
  if (posix_memalign(&memory, alignment < sizeof(void *) ? sizeof(void *)
                                                          : alignment,
                     size) != 0) {
    return false;
  }
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  return true;
}

static void host_release(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static const t64_host_allocator g_allocator = {host_allocate, host_release,
                                               NULL};

static uint8_t g_rom[8192];
static size_t g_rom_length;

static bool load_rom(void) {
  if (g_rom_length != 0u) return true;
  return read_file(mix_rom_path(), g_rom, sizeof g_rom, &g_rom_length) &&
         g_rom_length != 0u && g_rom_length < sizeof g_rom;
}

/* An admitted machine of the named profile holding the replay-mix ROM. */
static t64_machine *new_machine(const char *profile_id, bool trace) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  if (!load_rom() ||
      t64_profile_lookup(profile_id, &profile) != T64_STATUS_OK ||
      t64_machine_create(&g_allocator, profile, &machine) != T64_STATUS_OK ||
      machine == NULL) {
    return NULL;
  }
  if (t64_machine_admit_rom(machine, g_rom, g_rom_length) != T64_STATUS_OK ||
      t64_machine_trace_set_enabled(machine, trace) != T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return NULL;
  }
  return machine;
}

/* Drains the trace ring so it never overflows and hands every record to the
 * optional callback. */
typedef void (*trace_visit)(void *context, const t64_trace_record *record);

static bool drain_trace_all(t64_machine *machine, trace_visit visit,
                            void *context, uint64_t *dropped_out) {
  t64_trace_record records[64];
  size_t count;
  uint64_t dropped = 0u;
  bool saturated = false;
  do {
    size_t i;
    count = 0u;
    if (t64_machine_trace_drain(machine, records, 64u, &count, &dropped,
                                &saturated) != T64_STATUS_OK) {
      return false;
    }
    for (i = 0u; i < count && visit != NULL; i++) visit(context, &records[i]);
  } while (count == 64u);
  if (dropped_out != NULL) *dropped_out = dropped;
  return true;
}

/* ---- rom-timing ----------------------------------------------------------- */

/* Independent policy literals (never read from the core): the frame polls
 * channel 0 with the state command (one command byte, four response bytes):
 * 2000 * 1 + 188 + 438 + 2000 * 4 + 250 = 10,876 ticks from the RD64B start S
 * to the execute tick E, and no other channel (the end escape follows). */
#define HANDSHAKE_TICKS UINT64_C(10876)
#define SI_RD64B_ADDRESS UINT32_C(0x04800004)
#define POLL_COUNT 6u
#define STAMP_COUNT 6u
#define STAMP_MARGIN UINT64_C(8192)
/* Gap of each stamp in script order: S1 between E1 and E2, S2 between E2 and
 * E3, S3a and S3b between E3 and E4, S4 between E4 and E5, S5 between E5 and
 * E6 (the oracle literal, fixtures/replay/mix_oracle.py). */
static const unsigned stamp_gap[STAMP_COUNT] = {1u, 2u, 3u, 3u, 4u, 5u};

typedef struct {
  uint64_t starts[POLL_COUNT + 2u];
  size_t start_count;
  uint64_t completion_tick;
  uint64_t dropped;
  uint8_t log[256];
  size_t log_length;
  bool completed;
} poll_measure;

static void note_start(void *context, const t64_trace_record *record) {
  poll_measure *measure = (poll_measure *)context;
  if (record->device == T64_TRACE_DEVICE_BUS &&
      record->kind == T64_TRACE_KIND_BUS_WRITE32 &&
      (uint32_t)(record->value >> 32) == SI_RD64B_ADDRESS &&
      measure->start_count < POLL_COUNT + 2u) {
    measure->starts[measure->start_count++] = record->tick;
  }
}

/* Runs the ROM in process with the neutral schedule (no input), advancing in
 * slices of 16 ticks like the real runner, and records the tick of every
 * 32-bit bus write to SI RD64B. */
static bool measure_polls(poll_measure *out) {
  t64_machine *machine = new_machine(PROFILE_STEM, true);
  uint64_t tick = 0u;
  bool ok = false;
  memset(out, 0, sizeof *out);
  if (machine == NULL) return false;
  while (tick < UINT64_C(2000000)) {
    t64_stop_request request = {T64_STOP_GUEST_TICK, tick + UINT64_C(16)};
    t64_stop_result stop = {0u, 0u};
    if (t64_machine_advance(machine, &request, &stop) != T64_STATUS_OK) break;
    tick = stop.tick;
    if (!drain_trace_all(machine, note_start, out, &out->dropped)) break;
    if (stop.reason == T64_STOP_REASON_GUEST_COMPLETION) {
      t64_output_drain_result drain;
      memset(&drain, 0, sizeof drain);
      out->completed = true;
      out->completion_tick = stop.tick;
      ok = t64_machine_output_drain(machine, out->log, sizeof out->log,
                                    &drain) == T64_STATUS_OK;
      out->log_length = drain.copied_bytes;
      break;
    }
    if (stop.reason != T64_STOP_REASON_BOUND) break;
  }
  t64_machine_destroy(machine);
  return ok;
}

static bool load_script(t64_input_schedule **schedule_out) {
  uint8_t bytes[8192];
  size_t length = 0u;
  t64_input_error error;
  if (!read_file(mix_input_path(), bytes, sizeof bytes, &length) ||
      length == 0u || length >= sizeof bytes) {
    return false;
  }
  return t64_input_parse(bytes, length, schedule_out, &error) == T64_INPUT_OK;
}

static int case_rom_timing(void) {
  poll_measure measure;
  t64_input_schedule *schedule = NULL;
  uint64_t execute[POLL_COUNT];
  size_t i;
  size_t j;
  CHECK(measure_polls(&measure));
  CHECK(measure.completed);
  CHECK(measure.dropped == 0u);
  CHECK(measure.start_count == POLL_COUNT);
  CHECK(measure.completion_tick >= UINT64_C(1000000) &&
        measure.completion_tick <= UINT64_C(1400000));
  CHECK(measure.log_length == 72u);
  CHECK(load_script(&schedule));
  CHECK(schedule != NULL &&
        t64_input_schedule_event_count(schedule) == STAMP_COUNT);
  if (measure.start_count != POLL_COUNT || schedule == NULL ||
      t64_input_schedule_event_count(schedule) != STAMP_COUNT) {
    t64_input_schedule_destroy(schedule);
    return 1;
  }
  for (i = 0u; i < POLL_COUNT; i++) {
    execute[i] = measure.starts[i] + HANDSHAKE_TICKS;
    if (i > 0u) CHECK(measure.starts[i] > measure.starts[i - 1u]);
  }
  for (i = 0u; i < STAMP_COUNT; i++) {
    t64_input_event event;
    unsigned gap = stamp_gap[i];
    CHECK(t64_input_schedule_event(schedule, i, &event));
    for (j = 0u; j < POLL_COUNT; j++) {
      uint64_t distance = event.tick > execute[j] ? event.tick - execute[j]
                                                  : execute[j] - event.tick;
      CHECK(distance >= STAMP_MARGIN);
    }
    /* Strictly inside the gap between E(gap) and E(gap + 1) (1-based). */
    CHECK(event.tick > execute[gap - 1u] && event.tick < execute[gap]);
  }
  for (i = 0u; i < measure.start_count; i++) {
    (void)fprintf(stderr, "poll %zu start tick %llu execute tick %llu\n",
                  i + 1u, (unsigned long long)measure.starts[i],
                  (unsigned long long)execute[i]);
  }
  (void)fprintf(stderr, "neutral completion tick %llu\n",
                (unsigned long long)measure.completion_tick);
  t64_input_schedule_destroy(schedule);
  return g_failures == 0 ? 0 : 1;
}

/* ---- dispatch ------------------------------------------------------------- */

typedef int (*case_fn)(void);
typedef struct {
  const char *name;
  const char *suite;
  case_fn run;
} case_entry;

static const case_entry cases[] = {
    {"rom-timing", "unit", case_rom_timing},
};

int main(int argc, char **argv) {
  size_t i;
  int rc = 0;
  size_t ran = 0u;
  if (argc >= 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
      (void)printf("%s\n", cases[i].name);
    }
    return 0;
  }
  if (argc >= 3 && strcmp(argv[1], "--suite") == 0) {
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
      if (strcmp(cases[i].suite, argv[2]) == 0) {
        int status;
        g_failures = 0;
        status = cases[i].run();
        (void)fprintf(stderr, "case %s: %s\n", cases[i].name,
                      status == 0 && g_failures == 0 ? "pass" : "FAIL");
        if (status != 0 || g_failures != 0) rc = 1;
        ran++;
      }
    }
    return ran == 0u ? 2 : rc;
  }
  if (argc >= 2) {
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
      if (strcmp(cases[i].name, argv[1]) == 0) {
        int status = cases[i].run();
        return status == 0 && g_failures == 0 ? 0 : 1;
      }
    }
  }
  return 2;
}
