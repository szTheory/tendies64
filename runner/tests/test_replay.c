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

#include "replay_mix_golden.h"
#include "t64_input.h"
#include "t64_json.h"
#include "t64_replay.h"
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

/* ---- reference path (no replay module) --------------------------------------- */

#define MAX_YIELDS_WITHOUT_PROGRESS 64u
#define PREQUEUE_INTERVAL UINT64_C(160000)
#define JIT_LEAD UINT64_C(5000)
#define JIT_CHECKPOINT_LEAD UINT64_C(2500)
#define RUN_BOUND UINT64_C(1500000)

/* Advances to the absolute tick `target` in slices of `step` ticks with
 * inclusive tick bounds, resuming a resource-limit stop under a no-progress
 * rule. *reason_out is the reason of the last stop; the run ends early when it
 * is not a plain bound. */
static bool advance_to(t64_machine *machine, uint64_t *tick, uint64_t target,
                       uint64_t step, uint32_t *reason_out) {
  *reason_out = T64_STOP_REASON_BOUND;
  while (*tick < target) {
    uint64_t next = target - *tick > step ? *tick + step : target;
    unsigned stalled = 0u;
    for (;;) {
      t64_stop_request request = {T64_STOP_GUEST_TICK, next};
      t64_stop_result stop = {0u, 0u};
      uint64_t before = *tick;
      if (t64_machine_advance(machine, &request, &stop) != T64_STATUS_OK ||
          stop.tick < before || stop.tick > next) {
        return false;
      }
      *tick = stop.tick;
      *reason_out = stop.reason;
      if (stop.reason == T64_STOP_REASON_RESOURCE_LIMIT) {
        stalled = stop.tick > before ? 0u : stalled + 1u;
        if (stalled > MAX_YIELDS_WITHOUT_PROGRESS) return false;
        continue;
      }
      break;
    }
    if (*reason_out != T64_STOP_REASON_BOUND) return true;
  }
  return true;
}

static void hex_of(const uint8_t digest[T64_DIGEST_SIZE],
                   char out[T64_SHA256_HEX_SIZE]) {
  t64_sha256_hex(digest, out);
}

typedef struct {
  size_t count;
  uint64_t ticks[16];
  uint8_t digests[16][T64_DIGEST_SIZE];
  uint64_t end_tick;
  uint32_t end_reason;
  uint8_t end_digest[T64_DIGEST_SIZE];
} ref_run;

/* Pre-queue mode: the whole script is queued at tick 0 through the real API
 * and the run is digested at every multiple of the interval below the
 * completion tick and at the completion stop. */
static bool reference_prequeue(uint64_t step, ref_run *out) {
  t64_machine *machine = new_machine(PROFILE_STEM, false);
  t64_input_schedule *schedule = NULL;
  uint64_t tick = 0u;
  uint32_t reason = 0u;
  uint64_t target;
  size_t i;
  bool ok = false;
  memset(out, 0, sizeof *out);
  if (machine == NULL || !load_script(&schedule)) goto done;
  for (i = 0u; i < t64_input_schedule_event_count(schedule); i++) {
    t64_input_event event;
    if (!t64_input_schedule_event(schedule, i, &event) ||
        t64_machine_queue_input(machine, event.tick, event.port, event.buttons,
                                event.axis_x, event.axis_y) != T64_STATUS_OK) {
      goto done;
    }
  }
  for (target = PREQUEUE_INTERVAL; target <= RUN_BOUND;
       target += PREQUEUE_INTERVAL) {
    if (!advance_to(machine, &tick, target, step, &reason)) goto done;
    if (reason != T64_STOP_REASON_BOUND) break;
    if (out->count >= 16u) goto done;
    out->ticks[out->count] = tick;
    if (t64_machine_digest(machine, out->digests[out->count]) !=
        T64_STATUS_OK) {
      goto done;
    }
    out->count++;
  }
  if (reason == T64_STOP_REASON_BOUND &&
      !advance_to(machine, &tick, RUN_BOUND, step, &reason)) {
    goto done;
  }
  out->end_tick = tick;
  out->end_reason = reason;
  ok = t64_machine_digest(machine, out->end_digest) == T64_STATUS_OK;
done:
  t64_input_schedule_destroy(schedule);
  if (machine != NULL) t64_machine_destroy(machine);
  return ok;
}

/* Just-in-time mode: every distinct stamp tick T is handled by advancing to
 * T - 5,000, submitting its events in script order (the submission tick is the
 * completed tick), advancing to T - 2,500 and taking a checkpoint; then the run
 * ends at the completion stop. The digests of the two modes differ by design. */
static bool reference_jit(uint64_t step, ref_run *out) {
  t64_machine *machine = new_machine(PROFILE_STEM, false);
  t64_input_schedule *schedule = NULL;
  uint64_t tick = 0u;
  uint32_t reason = 0u;
  size_t i = 0u;
  size_t count;
  bool ok = false;
  memset(out, 0, sizeof *out);
  if (machine == NULL || !load_script(&schedule)) goto done;
  count = t64_input_schedule_event_count(schedule);
  while (i < count) {
    t64_input_event event;
    uint64_t stamp;
    if (!t64_input_schedule_event(schedule, i, &event)) goto done;
    stamp = event.tick;
    if (!advance_to(machine, &tick, stamp - JIT_LEAD, step, &reason) ||
        reason != T64_STOP_REASON_BOUND) {
      goto done;
    }
    while (i < count && t64_input_schedule_event(schedule, i, &event) &&
           event.tick == stamp) {
      if (t64_machine_queue_input(machine, event.tick, event.port,
                                  event.buttons, event.axis_x, event.axis_y) !=
          T64_STATUS_OK) {
        goto done;
      }
      i++;
    }
    if (!advance_to(machine, &tick, stamp - JIT_CHECKPOINT_LEAD, step,
                    &reason) ||
        reason != T64_STOP_REASON_BOUND || out->count >= 16u) {
      goto done;
    }
    out->ticks[out->count] = tick;
    if (t64_machine_digest(machine, out->digests[out->count]) !=
        T64_STATUS_OK) {
      goto done;
    }
    out->count++;
  }
  if (!advance_to(machine, &tick, RUN_BOUND, step, &reason)) goto done;
  out->end_tick = tick;
  out->end_reason = reason;
  ok = t64_machine_digest(machine, out->end_digest) == T64_STATUS_OK;
done:
  t64_input_schedule_destroy(schedule);
  if (machine != NULL) t64_machine_destroy(machine);
  return ok;
}

static int print_goldens(const char *mode, uint64_t step) {
  ref_run run;
  size_t i;
  bool jit = mode != NULL && strcmp(mode, "jit") == 0;
  char hex[T64_SHA256_HEX_SIZE];
  if (!(jit ? reference_jit(step, &run) : reference_prequeue(step, &run))) {
    (void)fprintf(stderr, "reference run failed\n");
    return 1;
  }
  (void)printf("mode %s step %llu\n", jit ? "jit" : "prequeue",
               (unsigned long long)step);
  for (i = 0u; i < run.count; i++) {
    hex_of(run.digests[i], hex);
    (void)printf("checkpoint %llu %s\n", (unsigned long long)run.ticks[i], hex);
  }
  hex_of(run.end_digest, hex);
  (void)printf("end %llu %s %s\n", (unsigned long long)run.end_tick,
               run.end_reason == T64_STOP_REASON_GUEST_COMPLETION
                   ? "guest-completion"
                   : "other",
               hex);
  return 0;
}

/* ---- golden op lists and headers ------------------------------------------------ */

static bool hex_to_digest(const char *hex, uint8_t out[T64_DIGEST_SIZE]) {
  size_t i;
  if (hex == NULL || strlen(hex) != 2u * T64_DIGEST_SIZE) return false;
  for (i = 0u; i < T64_DIGEST_SIZE; i++) {
    unsigned value = 0u;
    int k;
    for (k = 0; k < 2; k++) {
      char c = hex[2u * i + (unsigned)k];
      unsigned nibble;
      if (c >= '0' && c <= '9') nibble = (unsigned)(c - '0');
      else if (c >= 'a' && c <= 'f') nibble = (unsigned)(c - 'a') + 10u;
      else return false;
      value = value * 16u + nibble;
    }
    out[i] = (uint8_t)value;
  }
  return true;
}

static void op_input(t64_replay_op *op, uint64_t at, const golden_event *event) {
  memset(op, 0, sizeof *op);
  op->kind = T64_REPLAY_OP_INPUT;
  op->at = at;
  op->event_tick = event->event_tick;
  op->port = 0u;
  op->buttons = event->buttons;
  op->axis_x = event->x;
  op->axis_y = event->y;
}

static void op_checkpoint(t64_replay_op *op, uint64_t at, const char *digest) {
  memset(op, 0, sizeof *op);
  op->kind = T64_REPLAY_OP_CHECKPOINT;
  op->at = at;
  CHECK(hex_to_digest(digest, op->digest));
}

static void op_end(t64_replay_op *op, uint64_t at, const char *digest) {
  memset(op, 0, sizeof *op);
  op->kind = T64_REPLAY_OP_END;
  op->at = at;
  op->stop = T64_REPLAY_STOP_GUEST_COMPLETION;
  CHECK(hex_to_digest(digest, op->digest));
}

#define MAX_GOLDEN_OPS 32u

/* Pre-queue literal list: six inputs at 0 in script order, the seven
 * checkpoints and the end. */
static size_t golden_prequeue_ops(t64_replay_op *ops) {
  size_t n = 0u;
  size_t i;
  for (i = 0u; i < GOLDEN_SCRIPT_COUNT; i++) {
    op_input(&ops[n++], 0u, &golden_script[i]);
  }
  for (i = 0u; i < GOLDEN_PRE_CHECKPOINTS; i++) {
    op_checkpoint(&ops[n++], golden_pre_ticks[i], golden_pre_digests[i]);
  }
  op_end(&ops[n++], GOLDEN_END_TICK, GOLDEN_PRE_END_DIGEST);
  return n;
}

/* Just-in-time literal list: for each distinct stamp the inputs at stamp less
 * 5,000 in script order, then the checkpoint at stamp less 2,500; then the
 * end. */
static size_t golden_jit_ops(t64_replay_op *ops) {
  size_t n = 0u;
  size_t i;
  size_t k;
  for (k = 0u; k < GOLDEN_JIT_CHECKPOINTS; k++) {
    for (i = 0u; i < GOLDEN_SCRIPT_COUNT; i++) {
      if (golden_script[i].event_tick == golden_jit_stamps[k]) {
        op_input(&ops[n++], golden_script[i].event_tick - GOLDEN_JIT_LEAD,
                 &golden_script[i]);
      }
    }
    op_checkpoint(&ops[n++], golden_jit_ticks[k], golden_jit_digests[k]);
  }
  op_end(&ops[n++], GOLDEN_END_TICK, GOLDEN_JIT_END_DIGEST);
  return n;
}

/* The fixed header of the python-derived identity literals (format test only:
 * these values are not the live identity). */
static void fixed_header(t64_replay_header *header) {
  memset(header, 0, sizeof *header);
  (void)snprintf(header->identity.profile_id, sizeof header->identity.profile_id,
                 "%s", PROFILE_STEM);
  (void)snprintf(header->identity.profile_digest,
                 sizeof header->identity.profile_digest, "%s",
                 "661b161dfee9a10857efe3f98d840c09f05cbf0492dfde59e8ec44190dabb6"
                 "d5");
  header->identity.descriptor_version = 12u;
  header->identity.machine_model_version = 7u;
  header->identity.state_serialization_version = 25u;
  (void)snprintf(header->identity.build_id, sizeof header->identity.build_id,
                 "%s",
                 "t64-build-v1-0123456789abcdef0123456789abcdef0123456789abcdef"
                 "0123456789abcdef");
  (void)snprintf(header->identity.content_sha256,
                 sizeof header->identity.content_sha256, "%s",
                 "79c59149052719baa7080c22d44a21c8d924add1f45b16ced31bb9810bf37d"
                 "f0");
  header->identity.content_size = 4616u;
  header->identity.save_kind = T64_REPLAY_SAVE_BLANK;
  (void)snprintf(header->source_revision, sizeof header->source_revision, "%s",
                 "607b3ca087f97a2be307e196a75609cee07b3364");
}

/* The live identity of a machine composed here from the public API (never from
 * the module under test). */
static bool compose_live_header(const t64_machine *machine,
                                t64_replay_header *header) {
  const t64_profile_descriptor *profile = NULL;
  uint8_t digest[T64_DIGEST_SIZE];
  size_t size = 0u;
  memset(header, 0, sizeof *header);
  if (t64_machine_profile(machine, &profile) != T64_STATUS_OK ||
      t64_profile_hash(profile, digest) != T64_STATUS_OK) {
    return false;
  }
  (void)snprintf(header->identity.profile_id, sizeof header->identity.profile_id,
                 "%s", profile->profile_id);
  t64_sha256_hex(digest, header->identity.profile_digest);
  header->identity.descriptor_version = profile->descriptor_version;
  header->identity.machine_model_version = profile->machine_model_version;
  header->identity.state_serialization_version =
      profile->state_serialization_version;
  (void)snprintf(header->identity.build_id, sizeof header->identity.build_id,
                 "%s", t64_build_identity());
  if (t64_machine_content_digest(machine, digest) != T64_STATUS_OK ||
      t64_machine_rom_size(machine, &size) != T64_STATUS_OK) {
    return false;
  }
  t64_sha256_hex(digest, header->identity.content_sha256);
  header->identity.content_size = (uint64_t)size;
  header->identity.save_kind = T64_REPLAY_SAVE_BLANK;
  (void)snprintf(header->source_revision, sizeof header->source_revision, "%s",
                 t64_source_revision());
  return true;
}

/* The ops of a replay equal a literal list (kind, at, event tick, payload,
 * digest bytes and stop kind). */
static bool ops_equal(const t64_replay *replay, const t64_replay_op *want,
                      size_t count, const char *label) {
  size_t i;
  bool same = t64_replay_op_count(replay) == count;
  if (!same) {
    (void)fprintf(stderr, "%s: op count %zu want %zu\n", label,
                  t64_replay_op_count(replay), count);
    return false;
  }
  for (i = 0u; i < count; i++) {
    t64_replay_op got;
    if (!t64_replay_op_get(replay, i, &got) || got.kind != want[i].kind ||
        got.at != want[i].at ||
        (got.kind == T64_REPLAY_OP_INPUT &&
         (got.event_tick != want[i].event_tick || got.port != want[i].port ||
          got.buttons != want[i].buttons || got.axis_x != want[i].axis_x ||
          got.axis_y != want[i].axis_y)) ||
        (got.kind != T64_REPLAY_OP_INPUT &&
         memcmp(got.digest, want[i].digest, T64_DIGEST_SIZE) != 0) ||
        (got.kind == T64_REPLAY_OP_END && got.stop != want[i].stop)) {
      (void)fprintf(stderr, "%s: op %zu differs\n", label, i);
      same = false;
    }
  }
  return same;
}

static void sha_hex_of(const uint8_t *bytes, size_t length,
                       char out[T64_SHA256_HEX_SIZE]) {
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  t64_sha256_ctx ctx;
  t64_sha256_init(&ctx);
  t64_sha256_update(&ctx, bytes, length);
  t64_sha256_final(&ctx, digest);
  t64_sha256_hex(digest, out);
}

/* ---- format-roundtrip ------------------------------------------------------------- */

static bool keys_ascending(const t64_json_node *node) {
  const t64_json_node *child;
  const t64_json_node *previous = NULL;
  if (node == NULL) return true;
  if (node->type == T64_JSON_OBJECT) {
    for (child = node->first; child != NULL; child = child->next) {
      if (previous != NULL) {
        size_t common = previous->key_len < child->key_len ? previous->key_len
                                                          : child->key_len;
        int order = memcmp(previous->key, child->key, common);
        if (order > 0 || (order == 0 && previous->key_len >= child->key_len)) {
          return false;
        }
      }
      previous = child;
    }
  }
  for (child = node->first; child != NULL; child = child->next) {
    if (!keys_ascending(child)) return false;
  }
  return true;
}

/* Every u64 field of the document is a decimal string. */
static bool u64_fields_are_strings(const t64_json_node *root) {
  static const char *const root_u64[] = {"content_size"};
  static const char *const op_u64[] = {"at", "event_tick"};
  const t64_json_node *ops = t64_json_get(root, "ops");
  const t64_json_node *op;
  size_t i;
  uint64_t value;
  for (i = 0u; i < sizeof root_u64 / sizeof root_u64[0]; i++) {
    if (!t64_json_u64_string(t64_json_get(root, root_u64[i]), &value)) {
      return false;
    }
  }
  if (ops == NULL) return false;
  for (op = ops->first; op != NULL; op = op->next) {
    for (i = 0u; i < sizeof op_u64 / sizeof op_u64[0]; i++) {
      const t64_json_node *field = t64_json_get(op, op_u64[i]);
      if (field != NULL && !t64_json_u64_string(field, &value)) return false;
    }
    if (!t64_json_u64_string(t64_json_get(op, "at"), &value)) return false;
  }
  return true;
}

static int case_format_roundtrip(void) {
  t64_replay_header header;
  t64_replay_op ops[MAX_GOLDEN_OPS];
  uint8_t *bytes = NULL;
  size_t length = 0u;
  char hex[T64_SHA256_HEX_SIZE];
  size_t n;
  int pass;
  fixed_header(&header);
  for (pass = 0; pass < 2; pass++) {
    const bool jit = pass == 1;
    t64_replay *replay = NULL;
    t64_replay_error error = {T64_REPLAY_OK, 0u};
    t64_replay_status status;
    t64_json_doc doc = {0};
    n = jit ? golden_jit_ops(ops) : golden_prequeue_ops(ops);
    status = t64_replay_encode(&header, ops, n, &bytes, &length);
    CHECK(status == T64_REPLAY_OK);
    if (status != T64_REPLAY_OK) {
      (void)fprintf(stderr, "format-roundtrip: encode status %s\n",
                    t64_replay_status_name(status));
      return 1;
    }
    sha_hex_of(bytes, length, hex);
    CHECK(length == (jit ? GOLDEN_JIT_CANONICAL_BYTES
                         : GOLDEN_PRE_CANONICAL_BYTES));
    CHECK(strcmp(hex, jit ? GOLDEN_JIT_CANONICAL_SHA256
                          : GOLDEN_PRE_CANONICAL_SHA256) == 0);
    CHECK(length > 0u && bytes[length - 1u] == '\n');
    CHECK(t64_json_parse(bytes, length, NULL, &doc, NULL) == 0);
    if (doc.root != NULL) {
      CHECK(keys_ascending(doc.root));
      CHECK(u64_fields_are_strings(doc.root));
    }
    t64_json_free(&doc);
    /* Parse back, compare the ops with the literal list, re-encode to the
     * same bytes and the same identity. */
    status = t64_replay_parse(bytes, length, &replay, &error);
    CHECK(status == T64_REPLAY_OK);
    if (status == T64_REPLAY_OK) {
      size_t canonical_length = 0u;
      const uint8_t *canonical = t64_replay_canonical(replay, &canonical_length);
      uint8_t *again = NULL;
      size_t again_length = 0u;
      const uint8_t *identity = t64_replay_sha256(replay);
      uint8_t want[T64_SHA256_DIGEST_SIZE];
      CHECK(ops_equal(replay, ops, n, jit ? "format-jit" : "format-pre"));
      CHECK(canonical != NULL && canonical_length == length &&
            memcmp(canonical, bytes, length) == 0);
      CHECK(t64_replay_encode(t64_replay_header_get(replay), ops, n, &again,
                              &again_length) == T64_REPLAY_OK &&
            again_length == length && memcmp(again, bytes, length) == 0);
      free(again);
      {
        t64_sha256_ctx ctx;
        t64_sha256_init(&ctx);
        t64_sha256_update(&ctx, bytes, length);
        t64_sha256_final(&ctx, want);
      }
      CHECK(identity != NULL && memcmp(identity, want, sizeof want) == 0);
    } else {
      (void)fprintf(stderr, "format-roundtrip: parse status %s\n",
                    t64_replay_status_name(status));
    }
    t64_replay_destroy(replay);
    free(bytes);
    bytes = NULL;
  }
  return g_failures == 0 ? 0 : 1;
}

/* ---- independent-original-run ------------------------------------------------------- */

static bool digest_equals_hex(const uint8_t digest[T64_DIGEST_SIZE],
                              const char *hex) {
  char text[T64_SHA256_HEX_SIZE];
  hex_of(digest, text);
  return strcmp(text, hex) == 0;
}

/* The reference run (no replay module) reproduces the golden literals. */
static void check_reference_against_golden(const ref_run *run) {
  size_t i;
  CHECK(run->count == GOLDEN_PRE_CHECKPOINTS);
  for (i = 0u; i < run->count && i < GOLDEN_PRE_CHECKPOINTS; i++) {
    CHECK(run->ticks[i] == golden_pre_ticks[i]);
    CHECK(digest_equals_hex(run->digests[i], golden_pre_digests[i]));
  }
  CHECK(run->end_tick == GOLDEN_END_TICK);
  CHECK(run->end_reason == T64_STOP_REASON_GUEST_COMPLETION);
  CHECK(digest_equals_hex(run->end_digest, GOLDEN_PRE_END_DIGEST));
}

/* Records the pre-queue run through the recorder wrapper at the given advance
 * step; *replay_out is the finished replay. */
static bool record_prequeue(uint64_t step, t64_replay **replay_out) {
  t64_machine *machine = new_machine(PROFILE_STEM, false);
  t64_input_schedule *schedule = NULL;
  t64_replay_recorder *recorder = NULL;
  t64_replay_header header;
  t64_replay_error error;
  uint64_t tick = 0u;
  uint32_t reason = 0u;
  uint64_t target;
  size_t i;
  bool ok = false;
  if (machine == NULL || !load_script(&schedule) ||
      !compose_live_header(machine, &header)) {
    goto done;
  }
  recorder = t64_replay_recorder_create(&header, NULL);
  if (recorder == NULL) goto done;
  for (i = 0u; i < t64_input_schedule_event_count(schedule); i++) {
    t64_input_event event;
    if (!t64_input_schedule_event(schedule, i, &event) ||
        t64_replay_recorder_queue_input(recorder, machine, 0u, event.tick,
                                        event.port, event.buttons,
                                        event.axis_x, event.axis_y) !=
            T64_STATUS_OK) {
      goto done;
    }
  }
  for (target = PREQUEUE_INTERVAL; target <= RUN_BOUND;
       target += PREQUEUE_INTERVAL) {
    if (!advance_to(machine, &tick, target, step, &reason)) goto done;
    if (reason != T64_STOP_REASON_BOUND) break;
    if (t64_replay_recorder_checkpoint(recorder, machine, tick) !=
        T64_REPLAY_OK) {
      goto done;
    }
  }
  if (reason != T64_STOP_REASON_GUEST_COMPLETION &&
      !advance_to(machine, &tick, RUN_BOUND, step, &reason)) {
    goto done;
  }
  if (reason != T64_STOP_REASON_GUEST_COMPLETION ||
      t64_replay_recorder_end(recorder, machine, tick,
                              T64_REPLAY_STOP_GUEST_COMPLETION) !=
          T64_REPLAY_OK) {
    goto done;
  }
  ok = t64_replay_recorder_finish(recorder, replay_out, &error) ==
       T64_REPLAY_OK;
done:
  t64_replay_recorder_destroy(recorder);
  t64_input_schedule_destroy(schedule);
  if (machine != NULL) t64_machine_destroy(machine);
  return ok;
}

static int case_independent_original_run(void) {
  ref_run run;
  ref_run run_step7;
  t64_replay_op want[MAX_GOLDEN_OPS];
  size_t want_count = golden_prequeue_ops(want);
  t64_replay *recorded = NULL;
  t64_replay *recorded7 = NULL;
  size_t i;
  /* The reference path (the queue calls, the advance bounds and the digests
   * written here, no replay module) reproduces the golden literals at two
   * advance steps. */
  CHECK(reference_prequeue(4096u, &run));
  check_reference_against_golden(&run);
  CHECK(reference_prequeue(7u, &run_step7));
  CHECK(run_step7.count == run.count && run_step7.end_tick == run.end_tick &&
        memcmp(run_step7.end_digest, run.end_digest, T64_DIGEST_SIZE) == 0);
  for (i = 0u; i < run.count && i < run_step7.count; i++) {
    CHECK(memcmp(run_step7.digests[i], run.digests[i], T64_DIGEST_SIZE) == 0);
  }
  /* The recorder-wrapped run produces the independent literal op list. */
  CHECK(record_prequeue(4096u, &recorded));
  if (recorded != NULL) {
    CHECK(ops_equal(recorded, want, want_count, "recorded-4096"));
    for (i = 0u; i < run.count && i < GOLDEN_PRE_CHECKPOINTS; i++) {
      t64_replay_op got;
      CHECK(t64_replay_op_get(recorded, GOLDEN_SCRIPT_COUNT + i, &got) &&
            memcmp(got.digest, run.digests[i], T64_DIGEST_SIZE) == 0);
    }
  }
  CHECK(record_prequeue(7u, &recorded7));
  if (recorded7 != NULL) {
    CHECK(ops_equal(recorded7, want, want_count, "recorded-7"));
  }
  /* The recorded file of two different advance steps is the same file. */
  if (recorded != NULL && recorded7 != NULL) {
    size_t a = 0u;
    size_t b = 0u;
    const uint8_t *first = t64_replay_canonical(recorded, &a);
    const uint8_t *second = t64_replay_canonical(recorded7, &b);
    CHECK(first != NULL && second != NULL && a == b &&
          memcmp(first, second, a) == 0);
  }
  t64_replay_destroy(recorded);
  t64_replay_destroy(recorded7);
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
    {"format-roundtrip", "unit", case_format_roundtrip},
    {"independent-original-run", "unit", case_independent_original_run},
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
  if (argc >= 2 && strcmp(argv[1], "--print-goldens") == 0) {
    const char *mode = "prequeue";
    uint64_t step = 4096u;
    int k;
    for (k = 2; k + 1 < argc; k += 2) {
      if (strcmp(argv[k], "--mode") == 0) mode = argv[k + 1];
      else if (strcmp(argv[k], "--step") == 0) step = strtoull(argv[k + 1], NULL, 10);
      else return 2;
    }
    return step == 0u ? 2 : print_goldens(mode, step);
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
