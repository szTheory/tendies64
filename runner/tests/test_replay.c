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
#include "t64_test_tree.h"

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

static const t64_host_allocator g_allocator =
    T64_HOST_ALLOCATOR_INIT(host_allocate, host_release, NULL);

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
    t64_stop_request request =
        T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, tick + UINT64_C(16));
    t64_stop_result stop = T64_STOP_RESULT_INIT;
    if (t64_machine_advance(machine, &request, &stop) != T64_STATUS_OK) break;
    tick = stop.tick;
    if (!drain_trace_all(machine, note_start, out, &out->dropped)) break;
    if (stop.reason == T64_STOP_REASON_GUEST_COMPLETION) {
      t64_output_drain_result drain = T64_OUTPUT_DRAIN_RESULT_INIT;
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
      t64_stop_request request =
          T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, next);
      t64_stop_result stop = T64_STOP_RESULT_INIT;
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

/* The just-in-time reference through the recorder wrapper: advance to the
 * completed boundary at event tick less 5,000, then submit (same-tick events in
 * array order), advance to stamp less 2,500, checkpoint; then the end. */
static bool record_jit(uint64_t step, t64_replay **replay_out) {
  t64_machine *machine = new_machine(PROFILE_STEM, false);
  t64_input_schedule *schedule = NULL;
  t64_replay_recorder *recorder = NULL;
  t64_replay_header header;
  t64_replay_error error;
  uint64_t tick = 0u;
  uint32_t reason = 0u;
  size_t i = 0u;
  size_t count;
  bool ok = false;
  if (machine == NULL || !load_script(&schedule) ||
      !compose_live_header(machine, &header)) {
    goto done;
  }
  recorder = t64_replay_recorder_create(&header, NULL);
  if (recorder == NULL) goto done;
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
      if (t64_replay_recorder_queue_input(recorder, machine, tick, event.tick,
                                          event.port, event.buttons,
                                          event.axis_x, event.axis_y) !=
          T64_STATUS_OK) {
        goto done;
      }
      i++;
    }
    if (!advance_to(machine, &tick, stamp - JIT_CHECKPOINT_LEAD, step,
                    &reason) ||
        reason != T64_STOP_REASON_BOUND ||
        t64_replay_recorder_checkpoint(recorder, machine, tick) !=
            T64_REPLAY_OK) {
      goto done;
    }
  }
  if (!advance_to(machine, &tick, RUN_BOUND, step, &reason) ||
      reason != T64_STOP_REASON_GUEST_COMPLETION ||
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
  t64_replay_destroy(recorded);
  /* Just-in-time mode: the reference path reproduces the JIT goldens and the
   * recorder-wrapped run produces the independent literal JIT op list. */
  {
    ref_run jit;
    t64_replay_op jit_want[MAX_GOLDEN_OPS];
    size_t jit_count = golden_jit_ops(jit_want);
    t64_replay *recorded_jit = NULL;
    CHECK(reference_jit(4096u, &jit));
    CHECK(jit.count == GOLDEN_JIT_CHECKPOINTS);
    for (i = 0u; i < jit.count && i < GOLDEN_JIT_CHECKPOINTS; i++) {
      CHECK(jit.ticks[i] == golden_jit_ticks[i]);
      CHECK(digest_equals_hex(jit.digests[i], golden_jit_digests[i]));
    }
    CHECK(jit.end_tick == GOLDEN_END_TICK &&
          jit.end_reason == T64_STOP_REASON_GUEST_COMPLETION &&
          digest_equals_hex(jit.end_digest, GOLDEN_JIT_END_DIGEST));
    CHECK(record_jit(4096u, &recorded_jit));
    if (recorded_jit != NULL) {
      CHECK(ops_equal(recorded_jit, jit_want, jit_count, "recorded-jit"));
    }
    t64_replay_destroy(recorded_jit);
  }
  return g_failures == 0 ? 0 : 1;
}

/* ---- mutation-matrix ---------------------------------------------------------------- */

#define OFF_NONE ((size_t)SIZE_MAX)
#define OFF_ANY ((size_t)SIZE_MAX - 1u)

static uint8_t g_base[8192];
static size_t g_base_length;
static uint8_t g_work[8192 + 1024];

static bool build_base(void) {
  t64_replay_header header;
  t64_replay_op ops[MAX_GOLDEN_OPS];
  size_t n = golden_prequeue_ops(ops);
  uint8_t *bytes = NULL;
  size_t length = 0u;
  fixed_header(&header);
  if (t64_replay_encode(&header, ops, n, &bytes, &length) != T64_REPLAY_OK ||
      length > sizeof g_base) {
    free(bytes);
    return false;
  }
  memcpy(g_base, bytes, length);
  g_base_length = length;
  free(bytes);
  return true;
}

static const uint8_t *find_text(const uint8_t *src, size_t n, const char *text) {
  size_t length = strlen(text);
  size_t i;
  if (length == 0u || length > n) return NULL;
  for (i = 0u; i + length <= n; i++) {
    if (memcmp(src + i, text, length) == 0) return src + i;
  }
  return NULL;
}

/* Replaces the first occurrence of `find` with `repl` in the base text. */
static bool replace_text(const char *find, const char *repl, size_t *length_out) {
  const uint8_t *at = find_text(g_base, g_base_length, find);
  size_t head;
  size_t flen = strlen(find);
  size_t rlen = strlen(repl);
  if (at == NULL) return false;
  head = (size_t)(at - g_base);
  if (g_base_length - flen + rlen > sizeof g_work) return false;
  memcpy(g_work, g_base, head);
  memcpy(g_work + head, repl, rlen);
  memcpy(g_work + head + rlen, g_base + head + flen, g_base_length - head - flen);
  *length_out = g_base_length - flen + rlen;
  return true;
}

/* The end of the value that starts at `start` (a top-level member value of the
 * canonical text: strings hold no braces or commas). */
static size_t value_end(const uint8_t *text, size_t start, size_t n) {
  int depth = 0;
  bool in_string = false;
  size_t i;
  for (i = start; i < n; i++) {
    uint8_t c = text[i];
    if (c == '"') in_string = !in_string;
    if (in_string) continue;
    if (c == '{' || c == '[') depth++;
    if (c == '}' || c == ']') {
      if (depth == 0) return i;
      depth--;
    }
    if (c == ',' && depth == 0) return i;
  }
  return n;
}

/* Removes ("remove") or rewrites the value of a root member. */
static bool edit_root_member(const char *key, const char *new_value,
                             size_t *length_out) {
  char pattern[64];
  const uint8_t *at;
  size_t start;
  size_t vstart;
  size_t vend;
  size_t tail;
  size_t out = 0u;
  (void)snprintf(pattern, sizeof pattern, "\"%s\":", key);
  at = find_text(g_base, g_base_length, pattern);
  if (at == NULL) return false;
  start = (size_t)(at - g_base);
  vstart = start + strlen(pattern);
  vend = value_end(g_base, vstart, g_base_length);
  if (new_value != NULL) {
    size_t rlen = strlen(new_value);
    if (g_base_length - (vend - vstart) + rlen > sizeof g_work) return false;
    memcpy(g_work, g_base, vstart);
    memcpy(g_work + vstart, new_value, rlen);
    out = vstart + rlen;
    tail = g_base_length - vend;
    memcpy(g_work + out, g_base + vend, tail);
    *length_out = out + tail;
    return true;
  }
  /* remove the member and one adjoining comma */
  if (g_base[vend] == ',') {
    vend++;
  } else if (start > 0u && g_base[start - 1u] == ',') {
    start--;
  }
  memcpy(g_work, g_base, start);
  tail = g_base_length - vend;
  memcpy(g_work + start, g_base + vend, tail);
  *length_out = start + tail;
  return true;
}

static t64_replay_status parse_with(const uint8_t *bytes, size_t length,
                                    const t64_replay_limits *limits,
                                    size_t *offset_out, bool *canonical_same) {
  t64_replay *replay = NULL;
  t64_replay_error error = {T64_REPLAY_OK, 0u};
  t64_replay_status status = limits == NULL
      ? t64_replay_parse(bytes, length, &replay, &error)
      : t64_replay_parse_limits(bytes, length, limits, &replay, &error);
  if (offset_out != NULL) *offset_out = error.offset;
  if (canonical_same != NULL) {
    size_t clen = 0u;
    const uint8_t *canonical = replay == NULL ? NULL
                                              : t64_replay_canonical(replay, &clen);
    *canonical_same = canonical != NULL && clen == g_base_length &&
                      memcmp(canonical, g_base, clen) == 0;
  }
  t64_replay_destroy(replay);
  return status;
}

typedef enum { M_REPLACE, M_REMOVE, M_VALUE, M_APPEND } mkind;

typedef struct {
  const char *name;
  mkind kind;
  const char *a;
  const char *b;
  t64_replay_status want;
  size_t want_offset;
} mrow;

#define DIGEST_A \
  "4a873dd8c04ab57e8f5aa64a95268d2f897269ee5a44c03bc498b8a05abcd452"
#define END_OP_TEXT \
  "{\"at\":\"1141425\",\"digest\":\"" GOLDEN_PRE_END_DIGEST \
  "\",\"op\":\"end\",\"stop\":\"guest-completion\"}"

static const mrow mutation_rows[] = {
    {"unknown-root-key", M_REPLACE, "{\"boot\":", "{\"aaa\":1,\"boot\":", T64_REPLAY_INVALID_SCHEMA, OFF_NONE},
    {"missing-boot", M_REMOVE, "boot", NULL, T64_REPLAY_INVALID_SCHEMA, OFF_NONE},
    {"missing-build_id", M_REMOVE, "build_id", NULL, T64_REPLAY_INVALID_SCHEMA, OFF_NONE},
    {"missing-content_sha256", M_REMOVE, "content_sha256", NULL, T64_REPLAY_INVALID_SCHEMA, OFF_NONE},
    {"missing-content_size", M_REMOVE, "content_size", NULL, T64_REPLAY_INVALID_SCHEMA, OFF_NONE},
    {"missing-descriptor_version", M_REMOVE, "descriptor_version", NULL, T64_REPLAY_INVALID_SCHEMA, OFF_NONE},
    {"missing-input_schema_version", M_REMOVE, "input_schema_version", NULL, T64_REPLAY_INVALID_SCHEMA, OFF_NONE},
    {"missing-machine_model_version", M_REMOVE, "machine_model_version", NULL, T64_REPLAY_INVALID_SCHEMA, OFF_NONE},
    {"missing-ops", M_REMOVE, "ops", NULL, T64_REPLAY_INVALID_SCHEMA, OFF_NONE},
    {"missing-profile_digest", M_REMOVE, "profile_digest", NULL, T64_REPLAY_INVALID_SCHEMA, OFF_NONE},
    {"missing-profile_id", M_REMOVE, "profile_id", NULL, T64_REPLAY_INVALID_SCHEMA, OFF_NONE},
    {"missing-save_init", M_REMOVE, "save_init", NULL, T64_REPLAY_INVALID_SCHEMA, OFF_NONE},
    {"missing-schema", M_REMOVE, "schema", NULL, T64_REPLAY_INVALID_SCHEMA, OFF_NONE},
    {"missing-schema_version", M_REMOVE, "schema_version", NULL, T64_REPLAY_INVALID_SCHEMA, OFF_NONE},
    {"missing-source_revision", M_REMOVE, "source_revision", NULL, T64_REPLAY_INVALID_SCHEMA, OFF_NONE},
    {"missing-state_serialization_version", M_REMOVE, "state_serialization_version", NULL, T64_REPLAY_INVALID_SCHEMA, OFF_NONE},
    {"duplicate-root-key", M_REPLACE, "\"boot\":\"cold-admit\",", "\"boot\":\"cold-admit\",\"boot\":\"cold-admit\",", T64_REPLAY_INVALID_JSON, OFF_ANY},
    {"duplicate-op-key", M_REPLACE, "{\"at\":\"0\",\"buttons\":32768,", "{\"at\":\"0\",\"at\":\"0\",\"buttons\":32768,", T64_REPLAY_INVALID_JSON, OFF_ANY},
    {"type-string-for-integer", M_VALUE, "descriptor_version", "\"12\"", T64_REPLAY_INVALID_SCHEMA, OFF_NONE},
    {"type-integer-for-string", M_VALUE, "profile_id", "5", T64_REPLAY_INVALID_SCHEMA, OFF_NONE},
    {"type-object-for-array", M_VALUE, "ops", "{}", T64_REPLAY_INVALID_SCHEMA, OFF_NONE},
    {"type-array-for-object", M_VALUE, "save_init", "[]", T64_REPLAY_INVALID_SCHEMA, OFF_NONE},
    {"schema-text", M_REPLACE, "\"schema\":\"t64.replay\"", "\"schema\":\"t64.replaz\"", T64_REPLAY_INVALID_SCHEMA, OFF_NONE},
    {"schema-version-2", M_REPLACE, "\"schema_version\":1", "\"schema_version\":2", T64_REPLAY_INVALID_SCHEMA, OFF_NONE},
    {"input-schema-version-2", M_REPLACE, "\"input_schema_version\":1", "\"input_schema_version\":2", T64_REPLAY_INVALID_SCHEMA, OFF_NONE},
    {"boot-state", M_REPLACE, "\"boot\":\"cold-admit\"", "\"boot\":\"state\"", T64_REPLAY_BOOT_UNSUPPORTED, OFF_NONE},
    {"boot-other", M_REPLACE, "\"boot\":\"cold-admit\"", "\"boot\":\"warm\"", T64_REPLAY_INVALID_HEADER, OFF_NONE},
    {"profile-digest-uppercase", M_REPLACE, "\"profile_digest\":\"661b", "\"profile_digest\":\"661B", T64_REPLAY_INVALID_HEADER, OFF_NONE},
    {"profile-digest-63", M_REPLACE, "dabb6d5\",\"profile_id\"", "dabb6d\",\"profile_id\"", T64_REPLAY_INVALID_HEADER, OFF_NONE},
    {"profile-digest-nonhex", M_REPLACE, "\"profile_digest\":\"661b", "\"profile_digest\":\"g61b", T64_REPLAY_INVALID_HEADER, OFF_NONE},
    {"content-sha-uppercase", M_REPLACE, "\"content_sha256\":\"79c5", "\"content_sha256\":\"79C5", T64_REPLAY_INVALID_HEADER, OFF_NONE},
    {"content-size-leading-zero", M_REPLACE, "\"content_size\":\"4616\"", "\"content_size\":\"04616\"", T64_REPLAY_INVALID_HEADER, OFF_NONE},
    {"content-size-above-u64", M_REPLACE, "\"content_size\":\"4616\"", "\"content_size\":\"18446744073709551616\"", T64_REPLAY_INVALID_HEADER, OFF_NONE},
    {"content-size-json-integer", M_REPLACE, "\"content_size\":\"4616\"", "\"content_size\":4616", T64_REPLAY_INVALID_HEADER, OFF_NONE},
    {"build-id-grammar", M_REPLACE, "\"build_id\":\"t64-build-v1-0", "\"build_id\":\"t64-build-v2-0", T64_REPLAY_INVALID_HEADER, OFF_NONE},
    {"source-revision-uppercase", M_REPLACE, "\"source_revision\":\"607b3ca", "\"source_revision\":\"607B3ca", T64_REPLAY_INVALID_HEADER, OFF_NONE},
    {"descriptor-version-zero", M_REPLACE, "\"descriptor_version\":12", "\"descriptor_version\":0", T64_REPLAY_INVALID_HEADER, OFF_NONE},
    {"save-init-kind-unknown", M_REPLACE, "\"kind\":\"blank\"", "\"kind\":\"blanc\"", T64_REPLAY_INVALID_HEADER, OFF_NONE},
    {"save-init-blank-extra-key", M_REPLACE, "\"save_init\":{\"kind\":\"blank\"}", "\"save_init\":{\"kind\":\"blank\",\"length\":\"5\"}", T64_REPLAY_INVALID_SCHEMA, OFF_NONE},
    {"save-init-image-accepted", M_REPLACE, "\"save_init\":{\"kind\":\"blank\"}", "\"save_init\":{\"kind\":\"image\",\"length\":\"512\",\"sha256\":\"" DIGEST_A "\"}", T64_REPLAY_OK, OFF_NONE},
    {"save-init-image-no-sha", M_REPLACE, "\"save_init\":{\"kind\":\"blank\"}", "\"save_init\":{\"kind\":\"image\",\"length\":\"512\"}", T64_REPLAY_INVALID_SCHEMA, OFF_NONE},
    {"save-init-image-length-zero", M_REPLACE, "\"save_init\":{\"kind\":\"blank\"}", "\"save_init\":{\"kind\":\"image\",\"length\":\"0\",\"sha256\":\"" DIGEST_A "\"}", T64_REPLAY_INVALID_HEADER, OFF_NONE},
    {"number-above-2-53", M_REPLACE, "\"descriptor_version\":12", "\"descriptor_version\":9007199254740993", T64_REPLAY_INVALID_JSON, OFF_ANY},
    {"number-fraction", M_REPLACE, "\"descriptor_version\":12", "\"descriptor_version\":12.5", T64_REPLAY_INVALID_JSON, OFF_ANY},
    {"number-exponent", M_REPLACE, "\"descriptor_version\":12", "\"descriptor_version\":1e3", T64_REPLAY_INVALID_JSON, OFF_ANY},
    {"port-1", M_REPLACE, "\"port\":0,\"x\":1", "\"port\":1,\"x\":1", T64_REPLAY_INVALID_OP, 0u},
    {"port-3", M_REPLACE, "\"port\":0,\"x\":1", "\"port\":3,\"x\":1", T64_REPLAY_INVALID_OP, 0u},
    {"buttons-65536", M_REPLACE, "\"buttons\":32768", "\"buttons\":65536", T64_REPLAY_INVALID_OP, 0u},
    {"x-128", M_REPLACE, "\"x\":1,\"y\":-1", "\"x\":128,\"y\":-1", T64_REPLAY_INVALID_OP, 0u},
    {"y-minus-129", M_REPLACE, "\"x\":1,\"y\":-1", "\"x\":1,\"y\":-129", T64_REPLAY_INVALID_OP, 0u},
    {"at-leading-zero", M_REPLACE, "{\"at\":\"0\",\"buttons\":32768", "{\"at\":\"00\",\"buttons\":32768", T64_REPLAY_INVALID_OP, 0u},
    {"event-before-at", M_REPLACE, "{\"at\":\"0\",\"buttons\":32768,\"event_tick\":\"602000\"", "{\"at\":\"700000\",\"buttons\":32768,\"event_tick\":\"602000\"", T64_REPLAY_EVENT_BEFORE_AT, 0u},
    {"at-reversed", M_REPLACE, "\"at\":\"320000\"", "\"at\":\"100000\"", T64_REPLAY_AT_REVERSED, 7u},
    {"end-missing", M_REPLACE, "," END_OP_TEXT, "", T64_REPLAY_END_MISSING, 13u},
    {"end-not-last", M_REPLACE, "\"stop\":\"guest-completion\"}]", "\"stop\":\"guest-completion\"},{\"at\":\"1141425\",\"digest\":\"" GOLDEN_PRE_END_DIGEST "\",\"op\":\"checkpoint\"}]", T64_REPLAY_END_NOT_LAST, 14u},
    {"second-end", M_REPLACE, "\"stop\":\"guest-completion\"}]", "\"stop\":\"guest-completion\"}," END_OP_TEXT "]", T64_REPLAY_END_NOT_LAST, 14u},
    {"unknown-op-name", M_REPLACE, "\"op\":\"input\"", "\"op\":\"inpuz\"", T64_REPLAY_INVALID_OP, 0u},
    {"input-with-digest", M_REPLACE, "{\"at\":\"0\",\"buttons\":32768,", "{\"at\":\"0\",\"buttons\":32768,\"digest\":\"" DIGEST_A "\",", T64_REPLAY_INVALID_OP, 0u},
    {"op-digest-uppercase", M_REPLACE, "\"digest\":\"d6a7", "\"digest\":\"d6A7", T64_REPLAY_INVALID_OP, 6u},
    {"end-stop-unknown", M_REPLACE, "\"stop\":\"guest-completion\"", "\"stop\":\"guest-completed\"", T64_REPLAY_INVALID_OP, 13u},
    {"depth-5", M_REPLACE, "\"save_init\":{\"kind\":\"blank\"}", "\"save_init\":{\"kind\":\"blank\",\"a\":{\"b\":{\"c\":{\"d\":1}}}}", T64_REPLAY_INVALID_JSON, OFF_ANY},
    {"trailing-bytes", M_APPEND, "{}", NULL, T64_REPLAY_INVALID_JSON, OFF_ANY},
};

static bool offset_ok(size_t got, size_t want, size_t length) {
  if (want == OFF_ANY) return got < length;
  return got == want;
}

/* A document of `ops` checkpoints (plus an end) under the fixed header. */
static t64_replay_status parse_checkpoint_doc(size_t checkpoints,
                                              size_t inputs,
                                              const t64_replay_limits *limits,
                                              size_t *offset_out) {
  t64_replay_header header;
  t64_replay_op *ops = (t64_replay_op *)calloc(checkpoints + inputs + 1u,
                                               sizeof *ops);
  uint8_t *bytes = NULL;
  size_t length = 0u;
  size_t i;
  size_t n = 0u;
  t64_replay_status status = T64_REPLAY_ALLOCATION_FAILED;
  t64_replay *replay = NULL;
  t64_replay_error error = {T64_REPLAY_OK, 0u};
  if (ops == NULL) return status;
  fixed_header(&header);
  for (i = 0u; i < inputs; i++) op_input(&ops[n++], 0u, &golden_script[i % 6u]);
  for (i = 0u; i < checkpoints; i++) {
    op_checkpoint(&ops[n++], 1000u + (uint64_t)i, golden_pre_digests[0]);
  }
  op_end(&ops[n++], 100000u, GOLDEN_PRE_END_DIGEST);
  if (t64_replay_encode(&header, ops, n, &bytes, &length) != T64_REPLAY_OK) {
    free(ops);
    return status;
  }
  status = limits == NULL
      ? t64_replay_parse(bytes, length, &replay, &error)
      : t64_replay_parse_limits(bytes, length, limits, &replay, &error);
  if (offset_out != NULL) *offset_out = error.offset;
  t64_replay_destroy(replay);
  free(bytes);
  free(ops);
  return status;
}

static int case_mutation_matrix(void) {
  size_t i;
  size_t length = 0u;
  size_t offset = 0u;
  size_t flips = 0u;
  size_t flip_accepted = 0u;
  size_t flip_same_canonical = 0u;
  uint8_t original_sha[T64_SHA256_DIGEST_SIZE];
  CHECK(build_base());
  if (g_base_length == 0u) return 1;
  CHECK(g_base_length == GOLDEN_PRE_CANONICAL_BYTES);
  /* The accepted baseline of the same bytes, once per row group below too. */
  CHECK(parse_with(g_base, g_base_length, NULL, &offset, NULL) == T64_REPLAY_OK);
  for (i = 0u; i < sizeof mutation_rows / sizeof mutation_rows[0]; i++) {
    const mrow *row = &mutation_rows[i];
    t64_replay_status status;
    bool built = false;
    CHECK(parse_with(g_base, g_base_length, NULL, &offset, NULL) ==
          T64_REPLAY_OK);
    if (row->kind == M_REPLACE) {
      built = replace_text(row->a, row->b, &length);
    } else if (row->kind == M_REMOVE) {
      built = edit_root_member(row->a, NULL, &length);
    } else if (row->kind == M_VALUE) {
      built = edit_root_member(row->a, row->b, &length);
    } else {
      memcpy(g_work, g_base, g_base_length);
      memcpy(g_work + g_base_length, row->a, strlen(row->a));
      length = g_base_length + strlen(row->a);
      built = true;
    }
    if (!built) {
      (void)fprintf(stderr, "row %s: mutation not applicable\n", row->name);
      CHECK(built);
      continue;
    }
    status = parse_with(g_work, length, NULL, &offset, NULL);
    if (status != row->want ||
        (row->want != T64_REPLAY_OK && row->want_offset != OFF_NONE &&
         !offset_ok(offset, row->want_offset, length)) ||
        (row->want != T64_REPLAY_OK && row->want_offset == OFF_NONE &&
         offset != OFF_NONE)) {
      (void)fprintf(stderr, "row %s: status %s offset %zu want %s offset %zu\n",
                    row->name, t64_replay_status_name(status), offset,
                    t64_replay_status_name(row->want), row->want_offset);
      g_failures++;
    }
  }
  /* A 161-byte string and a 160-byte string (the string limit is 160). */
  {
    char value[200];
    memset(value, 'a', sizeof value);
    value[0] = '"';
    memset(value + 1, 'a', 161u);
    value[162] = '"';
    value[163] = '\0';
    CHECK(edit_root_member("profile_id", value, &length));
    CHECK(parse_with(g_work, length, NULL, &offset, NULL) ==
              T64_REPLAY_INVALID_JSON && offset < length);
    memset(value + 1, 'a', 160u);
    value[161] = '"';
    value[162] = '\0';
    CHECK(edit_root_member("profile_id", value, &length));
    /* 160 characters pass the JSON limit and then fail the profile grammar. */
    CHECK(parse_with(g_work, length, NULL, &offset, NULL) ==
          T64_REPLAY_INVALID_HEADER);
  }
  /* The byte cap is checked before any parse, at the default limit. */
  {
    uint8_t *big = (uint8_t *)malloc((size_t)T64_REPLAY_MAX_BYTES + 1u);
    t64_replay *replay = NULL;
    t64_replay_error error = {T64_REPLAY_OK, 0u};
    CHECK(big != NULL);
    if (big != NULL) {
      memset(big, ' ', (size_t)T64_REPLAY_MAX_BYTES + 1u);
      memcpy(big, g_base, g_base_length);
      CHECK(t64_replay_parse(big, (size_t)T64_REPLAY_MAX_BYTES + 1u, &replay,
                             &error) == T64_REPLAY_SIZE_LIMIT);
      CHECK(error.offset == OFF_NONE && replay == NULL);
      free(big);
    }
  }
  /* Caps. The default constants are asserted by value. */
  CHECK(T64_REPLAY_MAX_BYTES == 4194304u && T64_REPLAY_MAX_OPS == 65536u &&
        T64_REPLAY_MAX_CHECKPOINTS == 4096u);
  {
    t64_replay_limits low = {T64_REPLAY_MAX_BYTES, 8u, 2u};
    /* ops at the cap (5 inputs, 2 checkpoints, the end = 8) and one above. */
    CHECK(parse_checkpoint_doc(2u, 5u, &low, &offset) == T64_REPLAY_OK);
    CHECK(parse_checkpoint_doc(2u, 6u, &low, &offset) == T64_REPLAY_OP_LIMIT);
    /* checkpoints at the cap and one above (the op cap is not reached). */
    CHECK(parse_checkpoint_doc(2u, 0u, &low, &offset) == T64_REPLAY_OK);
    CHECK(parse_checkpoint_doc(3u, 0u, &low, &offset) ==
          T64_REPLAY_CHECKPOINT_LIMIT);
    /* default limits: the 4,096th checkpoint is accepted, the 4,097th is not.
     * (The op cap of 65,536 is not reachable with input ops at the default byte
     * cap: a minimal canonical input op is about 75 bytes.) */
    CHECK(parse_checkpoint_doc(4096u, 0u, NULL, &offset) == T64_REPLAY_OK);
    CHECK(parse_checkpoint_doc(4097u, 0u, NULL, &offset) ==
          T64_REPLAY_CHECKPOINT_LIMIT);
  }
  /* Sampled single-bit-flip sweep: every 5th byte of the golden file. */
  {
    t64_replay *replay = NULL;
    t64_replay_error error;
    CHECK(t64_replay_parse(g_base, g_base_length, &replay, &error) ==
          T64_REPLAY_OK);
    if (replay != NULL) {
      memcpy(original_sha, t64_replay_sha256(replay), sizeof original_sha);
      t64_replay_destroy(replay);
    }
  }
  for (i = 0u; i < g_base_length; i += 5u) {
    t64_replay *replay = NULL;
    t64_replay_error error;
    memcpy(g_work, g_base, g_base_length);
    g_work[i] = (uint8_t)(g_work[i] ^ (uint8_t)(1u << (i % 8u)));
    flips++;
    if (t64_replay_parse(g_work, g_base_length, &replay, &error) ==
        T64_REPLAY_OK) {
      flip_accepted++;
      if (memcmp(t64_replay_sha256(replay), original_sha,
                 sizeof original_sha) == 0) {
        flip_same_canonical++;
        (void)fprintf(stderr, "flip at byte %zu kept the canonical identity\n", i);
      }
    }
    t64_replay_destroy(replay);
  }
  CHECK(flips >= 400u);
  CHECK(flip_same_canonical == 0u);
  /* The recorder refuses an append that would pass a cap: the op cap before the
   * machine call (the machine is unchanged) and the checkpoint cap; the replay
   * is not published (finish reports the first failure). */
  {
    t64_machine *machine = new_machine(PROFILE_STEM, false);
    t64_replay_header header;
    t64_replay_limits low_ops = {T64_REPLAY_MAX_BYTES, 3u, 2u};
    t64_replay_limits low_cp = {T64_REPLAY_MAX_BYTES, 8u, 1u};
    t64_replay_recorder *recorder = NULL;
    t64_replay *replay = NULL;
    t64_replay_error error;
    uint8_t before[T64_DIGEST_SIZE];
    uint8_t after[T64_DIGEST_SIZE];
    CHECK(machine != NULL && compose_live_header(machine, &header));
    if (machine != NULL) {
      recorder = t64_replay_recorder_create(&header, &low_ops);
      CHECK(recorder != NULL);
      CHECK(t64_replay_recorder_queue_input(recorder, machine, 0u, 1000u, 0u, 1u,
                                            0, 0) == T64_STATUS_OK);
      CHECK(t64_replay_recorder_queue_input(recorder, machine, 0u, 2000u, 0u, 2u,
                                            0, 0) == T64_STATUS_OK);
      CHECK(t64_machine_digest(machine, before) == T64_STATUS_OK);
      CHECK(t64_replay_recorder_queue_input(recorder, machine, 0u, 3000u, 0u, 3u,
                                            0, 0) ==
            T64_STATUS_RESOURCE_EXHAUSTED);
      CHECK(t64_machine_digest(machine, after) == T64_STATUS_OK &&
            memcmp(before, after, sizeof before) == 0);
      CHECK(t64_replay_recorder_error(recorder) == T64_REPLAY_OP_LIMIT);
      CHECK(t64_replay_recorder_end(recorder, machine, 0u,
                                    T64_REPLAY_STOP_GUEST_BOUND) ==
            T64_REPLAY_OP_LIMIT);
      CHECK(t64_replay_recorder_finish(recorder, &replay, &error) ==
                T64_REPLAY_OP_LIMIT &&
            replay == NULL);
      t64_replay_recorder_destroy(recorder);
      recorder = t64_replay_recorder_create(&header, &low_cp);
      CHECK(recorder != NULL);
      CHECK(t64_replay_recorder_checkpoint(recorder, machine, 0u) ==
            T64_REPLAY_OK);
      CHECK(t64_replay_recorder_checkpoint(recorder, machine, 0u) ==
            T64_REPLAY_CHECKPOINT_LIMIT);
      CHECK(t64_replay_recorder_finish(recorder, &replay, &error) ==
                T64_REPLAY_CHECKPOINT_LIMIT &&
            replay == NULL);
      t64_replay_recorder_destroy(recorder);
      t64_machine_destroy(machine);
    }
  }
  (void)fprintf(stderr, "bit-flip sweep: %zu flips, %zu accepted, %zu refused\n",
                flips, flip_accepted, flips - flip_accepted);
  return g_failures == 0 ? 0 : 1;
}

/* ---- report-record ------------------------------------------------------------------- */

typedef struct {
  const char *name;
  const char *find;
  const char *repl;
  t64_replay_report_status want;
} rrow;

static void sample_digest(uint8_t out[T64_DIGEST_SIZE], uint8_t seed) {
  size_t i;
  for (i = 0u; i < T64_DIGEST_SIZE; i++) out[i] = (uint8_t)(seed + i);
}

static void sample_report(t64_replay_report *r, t64_replay_result result) {
  memset(r, 0, sizeof *r);
  r->result = result;
  (void)snprintf(r->field, sizeof r->field, "none");
  r->has_replay_sha256 = true;
  sample_digest(r->replay_sha256, 0x10u);
  r->checkpoints_matched = 2u;
  r->ops_applied = 8u;
  r->live_tick = 480000u;
  if (result == T64_REPLAY_RESULT_DIVERGED) {
    r->detail = T64_REPLAY_DETAIL_DIGEST;
    r->has_op_index = true;
    r->op_index = 8u;
    r->has_at = true;
    r->at = 480000u;
    r->has_expected_digest = true;
    r->has_actual_digest = true;
    sample_digest(r->expected_digest, 0x20u);
    sample_digest(r->actual_digest, 0x30u);
  } else if (result == T64_REPLAY_RESULT_IDENTITY_MISMATCH) {
    (void)snprintf(r->field, sizeof r->field, "build-id");
    r->checkpoints_matched = 0u;
    r->ops_applied = 0u;
    r->live_tick = 0u;
  } else if (result == T64_REPLAY_RESULT_REFUSED) {
    r->detail = T64_REPLAY_DETAIL_QUEUE_FULL;
    r->has_op_index = true;
    r->op_index = 6u;
    r->has_at = true;
    r->at = 0u;
    r->checkpoints_matched = 0u;
    r->ops_applied = 6u;
    r->live_tick = 0u;
  } else {
    r->ops_applied = 14u;
    r->checkpoints_matched = 7u;
    r->live_tick = 1141425u;
  }
}

static const rrow report_rows_match[] = {
    {"match-field", "\"field\":\"none\"", "\"field\":\"build-id\"", T64_REPLAY_REPORT_MATCH_SHAPE},
    {"match-detail", "\"detail\":\"none\"", "\"detail\":\"digest\"", T64_REPLAY_REPORT_MATCH_SHAPE},
    {"match-op-index", "\"op_index\":null", "\"op_index\":3", T64_REPLAY_REPORT_MATCH_SHAPE},
    {"match-at", "\"at\":null", "\"at\":\"5\"", T64_REPLAY_REPORT_MATCH_SHAPE},
    {"count-order", "\"checkpoints_matched\":7", "\"checkpoints_matched\":15", T64_REPLAY_REPORT_COUNT_ORDER},
    {"schema-text", "\"schema\":\"t64.replay-report\"", "\"schema\":\"t64.replay-reporx\"", T64_REPLAY_REPORT_INVALID_SCHEMA},
    {"schema-version", "\"schema_version\":1", "\"schema_version\":2", T64_REPLAY_REPORT_INVALID_SCHEMA},
    {"unknown-key", "{\"actual_digest\"", "{\"aaa\":1,\"actual_digest\"", T64_REPLAY_REPORT_INVALID_SCHEMA},
    {"missing-key", "\"live_tick\":\"1141425\",", "", T64_REPLAY_REPORT_INVALID_SCHEMA},
    {"duplicate-key", "\"field\":\"none\",", "\"field\":\"none\",\"field\":\"none\",", T64_REPLAY_REPORT_INVALID_JSON},
    {"result-outside-set", "\"result\":\"match\"", "\"result\":\"maybe\"", T64_REPLAY_REPORT_BAD_RESULT},
    {"detail-outside-set", "\"detail\":\"none\"", "\"detail\":\"zzz\"", T64_REPLAY_REPORT_BAD_DETAIL},
    {"field-outside-set", "\"field\":\"none\"", "\"field\":\"zzz\"", T64_REPLAY_REPORT_BAD_FIELD},
};
static const rrow report_rows_diverged[] = {
    {"diverged-no-op-index", "\"op_index\":8", "\"op_index\":null", T64_REPLAY_REPORT_DIVERGED_SHAPE},
    {"diverged-no-at", "\"at\":\"480000\"", "\"at\":null", T64_REPLAY_REPORT_DIVERGED_SHAPE},
    {"diverged-no-expected", "\"expected_digest\":\"202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f\"", "\"expected_digest\":null", T64_REPLAY_REPORT_DIVERGED_SHAPE},
    {"diverged-no-actual", "\"actual_digest\":\"303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f\"", "\"actual_digest\":null", T64_REPLAY_REPORT_DIVERGED_SHAPE},
};
static const rrow report_rows_identity[] = {
    {"identity-field-none", "\"field\":\"build-id\"", "\"field\":\"none\"", T64_REPLAY_REPORT_IDENTITY_SHAPE},
    {"identity-detail", "\"detail\":\"none\"", "\"detail\":\"digest\"", T64_REPLAY_REPORT_IDENTITY_SHAPE},
    {"identity-field-unknown", "\"field\":\"build-id\"", "\"field\":\"zzz\"", T64_REPLAY_REPORT_BAD_FIELD},
};
static const rrow report_rows_refused[] = {
    {"refused-detail-none", "\"detail\":\"queue-full\"", "\"detail\":\"none\"", T64_REPLAY_REPORT_REFUSED_SHAPE},
    {"refused-with-digest", "\"expected_digest\":null", "\"expected_digest\":\"" DIGEST_A "\"", T64_REPLAY_REPORT_REFUSED_SHAPE},
};

static bool report_mutate(const uint8_t *src, size_t n, const char *find,
                          const char *repl, uint8_t *dst, size_t cap,
                          size_t *out) {
  const uint8_t *at = find_text(src, n, find);
  size_t head;
  size_t flen = strlen(find);
  size_t rlen = strlen(repl);
  if (at == NULL || n - flen + rlen > cap) return false;
  head = (size_t)(at - src);
  memcpy(dst, src, head);
  memcpy(dst + head, repl, rlen);
  memcpy(dst + head + rlen, src + head + flen, n - head - flen);
  *out = n - flen + rlen;
  return true;
}

static void run_report_rows(const t64_replay_report *base_report,
                            const rrow *rows, size_t count) {
  uint8_t bytes[T64_REPLAY_REPORT_MAX_BYTES + 64u];
  uint8_t mutated[T64_REPLAY_REPORT_MAX_BYTES + 64u];
  size_t length = 0u;
  size_t mlength = 0u;
  size_t i;
  CHECK(t64_replay_report_encode(base_report, bytes, sizeof bytes, &length) ==
        T64_REPLAY_REPORT_OK);
  for (i = 0u; i < count; i++) {
    t64_replay_report_status status;
    /* the accepted baseline of the same bytes first */
    CHECK(t64_replay_report_check(bytes, length, NULL) == T64_REPLAY_REPORT_OK);
    if (!report_mutate(bytes, length, rows[i].find, rows[i].repl, mutated,
                       sizeof mutated, &mlength)) {
      (void)fprintf(stderr, "report row %s: mutation not applicable\n",
                    rows[i].name);
      g_failures++;
      continue;
    }
    status = t64_replay_report_check(mutated, mlength, NULL);
    if (status != rows[i].want) {
      (void)fprintf(stderr, "report row %s: status %s want %s\n", rows[i].name,
                    t64_replay_report_status_name(status),
                    t64_replay_report_status_name(rows[i].want));
      g_failures++;
    }
  }
}

static int case_report_record(void) {
  t64_replay_report report;
  uint8_t bytes[T64_REPLAY_REPORT_MAX_BYTES + 64u];
  size_t length = 0u;
  t64_json_doc doc = {0};
  uint8_t big[T64_REPLAY_REPORT_MAX_BYTES + 2u];
  const t64_replay_result results[4] = {
      T64_REPLAY_RESULT_MATCH, T64_REPLAY_RESULT_DIVERGED,
      T64_REPLAY_RESULT_IDENTITY_MISMATCH, T64_REPLAY_RESULT_REFUSED};
  size_t i;
  /* Each result shape encodes to canonical sorted bytes the checker accepts. */
  for (i = 0u; i < 4u; i++) {
    t64_replay_report back;
    sample_report(&report, results[i]);
    CHECK(t64_replay_report_encode(&report, bytes, sizeof bytes, &length) ==
          T64_REPLAY_REPORT_OK);
    CHECK(length > 0u && length <= T64_REPLAY_REPORT_MAX_BYTES &&
          bytes[length - 1u] == '\n');
    CHECK(t64_json_parse(bytes, length, NULL, &doc, NULL) == 0);
    if (doc.root != NULL) CHECK(keys_ascending(doc.root));
    t64_json_free(&doc);
    CHECK(t64_replay_report_check(bytes, length, &back) ==
          T64_REPLAY_REPORT_OK);
    CHECK(back.result == report.result && back.detail == report.detail &&
          strcmp(back.field, report.field) == 0 &&
          back.checkpoints_matched == report.checkpoints_matched &&
          back.ops_applied == report.ops_applied &&
          back.live_tick == report.live_tick &&
          back.has_op_index == report.has_op_index &&
          back.op_index == report.op_index);
  }
  sample_report(&report, T64_REPLAY_RESULT_MATCH);
  run_report_rows(&report, report_rows_match,
                  sizeof report_rows_match / sizeof report_rows_match[0]);
  sample_report(&report, T64_REPLAY_RESULT_DIVERGED);
  run_report_rows(&report, report_rows_diverged,
                  sizeof report_rows_diverged / sizeof report_rows_diverged[0]);
  sample_report(&report, T64_REPLAY_RESULT_IDENTITY_MISMATCH);
  run_report_rows(&report, report_rows_identity,
                  sizeof report_rows_identity / sizeof report_rows_identity[0]);
  sample_report(&report, T64_REPLAY_RESULT_REFUSED);
  run_report_rows(&report, report_rows_refused,
                  sizeof report_rows_refused / sizeof report_rows_refused[0]);
  /* A report above 4,096 bytes is refused before any parse. */
  memset(big, ' ', sizeof big);
  CHECK(t64_replay_report_check(big, T64_REPLAY_REPORT_MAX_BYTES + 1u, NULL) ==
        T64_REPLAY_REPORT_SIZE_LIMIT);
  return g_failures == 0 ? 0 : 1;
}

/* ---- driver helpers ------------------------------------------------------------------ */

/* A named assertion: out-of-tree mutants must fail at their named comparator. */
static void check_named(bool ok, const char *label) {
  if (!ok) {
    g_failures++;
    (void)fprintf(stderr, "FAIL assertion %s\n", label);
  }
}

static bool make_replay(const t64_replay_header *header,
                        const t64_replay_op *ops, size_t count,
                        t64_replay **replay_out) {
  uint8_t *bytes = NULL;
  size_t length = 0u;
  t64_replay_error error;
  bool ok = t64_replay_encode(header, ops, count, &bytes, &length) ==
                T64_REPLAY_OK &&
            t64_replay_parse(bytes, length, replay_out, &error) == T64_REPLAY_OK;
  free(bytes);
  return ok;
}

static t64_replay_step_result play_on(t64_machine *machine,
                                      const t64_replay *replay, uint64_t step,
                                      t64_replay_report *report,
                                      uint64_t *calls_out) {
  t64_replay_play_options options;
  t64_replay_step_result result;
  memset(&options, 0, sizeof options);
  options.advance_step = step;
  result = t64_replay_play(machine, replay, &options, report);
  if (calls_out != NULL) *calls_out = options.advance_calls;
  return result;
}

static bool report_is(const t64_replay_report *r, t64_replay_result result,
                      t64_replay_detail detail, const char *field) {
  return r->result == result && r->detail == detail &&
         strcmp(r->field, field) == 0;
}

/* ---- identity-binding ----------------------------------------------------------------- */

static void flip_hex(char *text, size_t index) {
  text[index] = text[index] == '0' ? '1' : '0';
}

static int case_identity_binding(void) {
  t64_replay_op ops[MAX_GOLDEN_OPS];
  const size_t count = golden_prequeue_ops(ops);
  t64_machine *probe = new_machine(PROFILE_STEM, false);
  t64_replay_header live;
  t64_replay_header altered;
  t64_replay *replay = NULL;
  t64_replay_report report;
  uint8_t admitted_digest[T64_DIGEST_SIZE];
  uint64_t calls = 0u;
  size_t i;
  static const char *const names[] = {"profile-digest", "build-id",
                                      "content-sha256", "content-size",
                                      "profile-id", "profile-and-build"};
  CHECK(probe != NULL && compose_live_header(probe, &live));
  if (probe == NULL) return 1;
  CHECK(t64_machine_digest(probe, admitted_digest) == T64_STATUS_OK);
  t64_machine_destroy(probe);
  /* The accepted baseline in the same setup: the unaltered replay plays to a
   * match on a fresh machine of the same ROM. */
  {
    t64_machine *machine = new_machine(PROFILE_STEM, false);
    t64_replay_op prefix[MAX_GOLDEN_OPS];
    size_t prefix_count = 0u;
    size_t k;
    /* The prefix through the first checkpoint plays the same live header. */
    for (k = 0u; k <= GOLDEN_SCRIPT_COUNT; k++) prefix[prefix_count++] = ops[k];
    op_end(&prefix[prefix_count], golden_pre_ticks[0], golden_pre_digests[0]);
    prefix[prefix_count].stop = T64_REPLAY_STOP_GUEST_BOUND;
    prefix_count++;
    CHECK(make_replay(&live, prefix, prefix_count, &replay));
    if (machine != NULL && replay != NULL) {
      CHECK(play_on(machine, replay, 4096u, &report, &calls) ==
            T64_REPLAY_STEP_MATCH);
      CHECK(report_is(&report, T64_REPLAY_RESULT_MATCH, T64_REPLAY_DETAIL_NONE,
                      "none"));
      CHECK(calls > 0u);
    }
    if (machine != NULL) t64_machine_destroy(machine);
    t64_replay_destroy(replay);
    replay = NULL;
  }
  /* Each header field family refuses with its own field before any advance. */
  for (i = 0u; i < sizeof names / sizeof names[0]; i++) {
    t64_machine *machine = new_machine(PROFILE_STEM, false);
    t64_replay_live_identity identity;
    const char *field = NULL;
    uint8_t after[T64_DIGEST_SIZE];
    const char *want_field = names[i];
    bool refused;
    altered = live;
    if (i == 0u) flip_hex(altered.identity.profile_digest, 3u);
    if (i == 1u) {
      size_t n = strlen(altered.identity.build_id);
      flip_hex(altered.identity.build_id, n - 1u);
    }
    if (i == 2u) flip_hex(altered.identity.content_sha256, 5u);
    if (i == 3u) altered.identity.content_size += 1u;
    if (i == 4u) {
      (void)snprintf(altered.identity.profile_id,
                     sizeof altered.identity.profile_id, "%s-e4k", PROFILE_STEM);
    }
    if (i == 5u) {
      /* The order row: profile and build both altered reports the profile. */
      flip_hex(altered.identity.profile_digest, 3u);
      flip_hex(altered.identity.build_id,
               strlen(altered.identity.build_id) - 1u);
      want_field = "profile-digest";
    }
    CHECK(machine != NULL && make_replay(&altered, ops, count, &replay));
    if (machine == NULL || replay == NULL) {
      if (machine != NULL) t64_machine_destroy(machine);
      t64_replay_destroy(replay);
      replay = NULL;
      continue;
    }
    /* Accepted baseline for the same setup: the live identity binds. */
    CHECK(t64_replay_live_identity_compose(machine, &identity));
    CHECK(t64_replay_bind(&live, &identity, &field) == T64_REPLAY_BIND_OK);
    refused = play_on(machine, replay, 4096u, &report, &calls) ==
              T64_REPLAY_STEP_FAILED;
    if (i == 1u) check_named(refused, "build-mismatch-refused");
    if (i == 0u) check_named(refused, "profile-mismatch-refused");
    CHECK(refused);
    CHECK(report_is(&report, T64_REPLAY_RESULT_IDENTITY_MISMATCH,
                    T64_REPLAY_DETAIL_NONE, want_field));
    CHECK(calls == 0u);
    CHECK(report.live_tick == 0u && report.ops_applied == 0u &&
          report.checkpoints_matched == 0u);
    CHECK(t64_machine_digest(machine, after) == T64_STATUS_OK &&
          memcmp(after, admitted_digest, sizeof after) == 0);
    t64_machine_destroy(machine);
    t64_replay_destroy(replay);
    replay = NULL;
  }
  /* save_init on an -e4k machine holding an authored 512-byte image. */
  {
    t64_machine *with_image = new_machine(PROFILE_STEM "-e4k", false);
    t64_machine *blank_device = new_machine(PROFILE_STEM "-e4k", false);
    t64_machine *no_device = new_machine(PROFILE_STEM, false);
    uint8_t image[512];
    uint8_t sha[T64_SHA256_DIGEST_SIZE];
    t64_replay_live_identity held;
    t64_replay_live_identity empty;
    t64_replay_live_identity stem;
    t64_replay_header recorded;
    const char *field = NULL;
    size_t k;
    for (k = 0u; k < sizeof image; k++) image[k] = (uint8_t)(k * 5u + 1u);
    CHECK(with_image != NULL && blank_device != NULL && no_device != NULL);
    if (with_image != NULL && blank_device != NULL && no_device != NULL) {
      CHECK(t64_machine_save_import(with_image, T64_SAVE_EEPROM_4K,
                                    T64_SAVE_FORMAT_RAW, image,
                                    sizeof image) == T64_STATUS_OK);
      CHECK(t64_replay_live_identity_compose(with_image, &held));
      CHECK(t64_replay_live_identity_compose(blank_device, &empty));
      CHECK(t64_replay_live_identity_compose(no_device, &stem));
      CHECK(held.save_kind == T64_REPLAY_SAVE_IMAGE && held.save_length == 512u);
      CHECK(empty.save_kind == T64_REPLAY_SAVE_BLANK);
      CHECK(stem.save_kind == T64_REPLAY_SAVE_BLANK);
      {
        t64_sha256_ctx ctx;
        t64_sha256_init(&ctx);
        t64_sha256_update(&ctx, image, sizeof image);
        t64_sha256_final(&ctx, sha);
        t64_sha256_hex(sha, recorded.identity.save_sha256);
      }
      CHECK(strcmp(held.save_sha256, recorded.identity.save_sha256) == 0);
      /* recorded blank: binds the blank device, refused on the held image */
      memset(&recorded, 0, sizeof recorded);
      recorded.identity = empty;
      recorded.identity.save_kind = T64_REPLAY_SAVE_BLANK;
      CHECK(t64_replay_bind(&recorded, &empty, &field) == T64_REPLAY_BIND_OK);
      CHECK(t64_replay_bind(&recorded, &held, &field) ==
                T64_REPLAY_BIND_SAVE_INIT &&
            strcmp(field, "save-init") == 0);
      /* recorded image: binds only the machine holding the same image */
      recorded.identity = held;
      CHECK(t64_replay_bind(&recorded, &held, &field) == T64_REPLAY_BIND_OK);
      CHECK(t64_replay_bind(&recorded, &empty, &field) ==
                T64_REPLAY_BIND_SAVE_INIT &&
            strcmp(field, "save-init") == 0);
      /* a stem machine has no device: only blank binds, image does not */
      recorded.identity = stem;
      recorded.identity.save_kind = T64_REPLAY_SAVE_BLANK;
      CHECK(t64_replay_bind(&recorded, &stem, &field) == T64_REPLAY_BIND_OK);
      recorded.identity.save_kind = T64_REPLAY_SAVE_IMAGE;
      recorded.identity.save_length = 512u;
      (void)snprintf(recorded.identity.save_sha256,
                     sizeof recorded.identity.save_sha256, "%s", held.save_sha256);
      CHECK(t64_replay_bind(&recorded, &stem, &field) ==
                T64_REPLAY_BIND_SAVE_INIT &&
            strcmp(field, "save-init") == 0);
      /* the full driver: a recorded blank refused on the held image before
       * any advance */
      {
        t64_replay_op one_end[1];
        t64_replay *blank_replay = NULL;
        recorded.identity = held;
        recorded.identity.save_kind = T64_REPLAY_SAVE_BLANK;
        (void)snprintf(recorded.source_revision, sizeof recorded.source_revision,
                       "%s", live.source_revision);
        op_end(&one_end[0], 10u, GOLDEN_PRE_END_DIGEST);
        one_end[0].stop = T64_REPLAY_STOP_GUEST_BOUND;
        CHECK(make_replay(&recorded, one_end, 1u, &blank_replay));
        if (blank_replay != NULL) {
          CHECK(play_on(with_image, blank_replay, 16u, &report, &calls) ==
                T64_REPLAY_STEP_FAILED);
          CHECK(report_is(&report, T64_REPLAY_RESULT_IDENTITY_MISMATCH,
                          T64_REPLAY_DETAIL_NONE, "save-init"));
          CHECK(calls == 0u && report.live_tick == 0u);
        }
        t64_replay_destroy(blank_replay);
      }
    }
    if (with_image != NULL) t64_machine_destroy(with_image);
    if (blank_device != NULL) t64_machine_destroy(blank_device);
    if (no_device != NULL) t64_machine_destroy(no_device);
  }
  return g_failures == 0 ? 0 : 1;
}

/* ---- at-matters ------------------------------------------------------------------------ */

static int case_at_matters(void) {
  t64_replay_op jit[MAX_GOLDEN_OPS];
  t64_replay_op pre[MAX_GOLDEN_OPS];
  const size_t jit_count = golden_jit_ops(jit);
  (void)golden_prequeue_ops(pre);
  t64_machine *probe = new_machine(PROFILE_STEM, false);
  t64_replay_header live;
  t64_replay *replay = NULL;
  t64_replay_report report;
  uint64_t calls = 0u;
  CHECK(probe != NULL && compose_live_header(probe, &live));
  if (probe == NULL) return 1;
  t64_machine_destroy(probe);
  /* The baseline just-in-time replay matches its goldens. */
  {
    t64_machine *machine = new_machine(PROFILE_STEM, false);
    bool matched = false;
    CHECK(machine != NULL && make_replay(&live, jit, jit_count, &replay));
    if (machine != NULL && replay != NULL) {
      matched = play_on(machine, replay, 4096u, &report, &calls) ==
                T64_REPLAY_STEP_MATCH;
    }
    check_named(matched, "jit-golden-match");
    if (machine != NULL) t64_machine_destroy(machine);
    t64_replay_destroy(replay);
    replay = NULL;
  }
  /* The first input op moved to at 0 (still nondecreasing) diverges at its
   * first following checkpoint (op index 1, tick 599,500). */
  {
    t64_replay_op moved[MAX_GOLDEN_OPS];
    t64_machine *machine = new_machine(PROFILE_STEM, false);
    memcpy(moved, jit, sizeof moved);
    moved[0].at = 0u;
    CHECK(machine != NULL && make_replay(&live, moved, jit_count, &replay));
    if (machine != NULL && replay != NULL) {
      CHECK(play_on(machine, replay, 4096u, &report, &calls) ==
            T64_REPLAY_STEP_FAILED);
      CHECK(report_is(&report, T64_REPLAY_RESULT_DIVERGED,
                      T64_REPLAY_DETAIL_DIGEST, "none"));
      CHECK(report.has_op_index && report.op_index == 1u);
      CHECK(report.has_at && report.at == golden_jit_ticks[0]);
      CHECK(report.live_tick == golden_jit_ticks[0] && report.live_tick != 0u);
      CHECK(report.checkpoints_matched == 0u);
      CHECK(report.has_expected_digest && report.has_actual_digest &&
            memcmp(report.expected_digest, report.actual_digest,
                   T64_DIGEST_SIZE) != 0);
    }
    if (machine != NULL) t64_machine_destroy(machine);
    t64_replay_destroy(replay);
    replay = NULL;
  }
  /* The pre-queue golden with its last input op at 100 instead of 0 diverges
   * at the first checkpoint (160,000, op index 6). */
  {
    t64_replay_op moved[MAX_GOLDEN_OPS];
    t64_replay_op prefix[MAX_GOLDEN_OPS];
    t64_machine *machine = new_machine(PROFILE_STEM, false);
    t64_machine *baseline = new_machine(PROFILE_STEM, false);
    t64_replay *unmoved = NULL;
    size_t prefix_count = 0u;
    size_t k;
    /* The prefix through the first checkpoint (the inputs, the checkpoint at
     * 160,000 and an end at the same tick) is the cheap same-setup baseline. */
    for (k = 0u; k <= GOLDEN_SCRIPT_COUNT; k++) prefix[prefix_count++] = pre[k];
    op_end(&prefix[prefix_count], golden_pre_ticks[0], golden_pre_digests[0]);
    prefix[prefix_count].stop = T64_REPLAY_STOP_GUEST_BOUND;
    prefix_count++;
    CHECK(baseline != NULL && make_replay(&live, prefix, prefix_count, &unmoved));
    if (baseline != NULL && unmoved != NULL) {
      CHECK(play_on(baseline, unmoved, 4096u, &report, &calls) ==
            T64_REPLAY_STEP_MATCH);
    }
    if (baseline != NULL) t64_machine_destroy(baseline);
    t64_replay_destroy(unmoved);
    memcpy(moved, prefix, sizeof moved);
    moved[GOLDEN_SCRIPT_COUNT - 1u].at = 100u;
    CHECK(machine != NULL && make_replay(&live, moved, prefix_count, &replay));
    if (machine != NULL && replay != NULL) {
      CHECK(play_on(machine, replay, 4096u, &report, &calls) ==
            T64_REPLAY_STEP_FAILED);
      CHECK(report_is(&report, T64_REPLAY_RESULT_DIVERGED,
                      T64_REPLAY_DETAIL_DIGEST, "none"));
      CHECK(report.has_op_index && report.op_index == GOLDEN_SCRIPT_COUNT);
      CHECK(report.live_tick == GOLDEN_PRE_INTERVAL);
    }
    if (machine != NULL) t64_machine_destroy(machine);
    t64_replay_destroy(replay);
    replay = NULL;
  }
  /* Witness only (a non-claim): pre-queue and just-in-time submission give
   * different digests at the first checkpoint where the first event is still
   * pending (tick 599,500). No equality across submission schedules is
   * claimed anywhere. */
  {
    t64_machine *machine = new_machine(PROFILE_STEM, false);
    t64_input_schedule *schedule = NULL;
    uint8_t digest[T64_DIGEST_SIZE];
    uint64_t tick = 0u;
    uint32_t reason = 0u;
    size_t i;
    CHECK(machine != NULL && load_script(&schedule));
    if (machine != NULL && schedule != NULL) {
      for (i = 0u; i < t64_input_schedule_event_count(schedule); i++) {
        t64_input_event event;
        CHECK(t64_input_schedule_event(schedule, i, &event) &&
              t64_machine_queue_input(machine, event.tick, event.port,
                                      event.buttons, event.axis_x,
                                      event.axis_y) == T64_STATUS_OK);
      }
      CHECK(advance_to(machine, &tick, golden_jit_ticks[0], 4096u, &reason));
      CHECK(tick == golden_jit_ticks[0] &&
            t64_machine_digest(machine, digest) == T64_STATUS_OK);
      CHECK(!digest_equals_hex(digest, golden_jit_digests[0]));
    }
    t64_input_schedule_destroy(schedule);
    if (machine != NULL) t64_machine_destroy(machine);
  }
  return g_failures == 0 ? 0 : 1;
}

/* ---- partition ---------------------------------------------------------------------------- */

typedef struct {
  t64_machine *other;
  uint64_t tick;
} interleave;

static void advance_other(void *context, uint64_t tick) {
  interleave *state = (interleave *)context;
  uint32_t reason = 0u;
  if (state->other != NULL && tick > state->tick) {
    (void)advance_to(state->other, &state->tick, tick, 4096u, &reason);
  }
}

/* One full play of a golden at an advance step with the trace on or off and an
 * optional second machine advanced between the slices. */
static bool play_golden(const t64_replay_header *live, const t64_replay_op *ops,
                        size_t count, uint64_t step, bool trace,
                        bool second_machine) {
  t64_machine *machine = new_machine(PROFILE_STEM, trace);
  t64_machine *other = second_machine ? new_machine(PROFILE_STEM, false) : NULL;
  t64_replay *replay = NULL;
  t64_replay_report report;
  t64_replay_play_options options;
  interleave state = {other, 0u};
  bool ok = false;
  memset(&options, 0, sizeof options);
  options.advance_step = step;
  options.after_advance = advance_other;
  options.context = &state;
  if (machine != NULL && (!second_machine || other != NULL) &&
      make_replay(live, ops, count, &replay)) {
    ok = t64_replay_play(machine, replay, &options, &report) ==
         T64_REPLAY_STEP_MATCH;
  }
  if (!ok) {
    (void)fprintf(stderr,
                  "partition: step %llu trace %d second %d did not match "
                  "(result %d detail %d op %llu)\n",
                  (unsigned long long)step, trace, second_machine,
                  (int)report.result, (int)report.detail,
                  (unsigned long long)report.op_index);
  }
  t64_replay_destroy(replay);
  if (machine != NULL) t64_machine_destroy(machine);
  if (other != NULL) t64_machine_destroy(other);
  return ok;
}

static int case_partition(void) {
  t64_replay_op pre[MAX_GOLDEN_OPS];
  t64_replay_op jit[MAX_GOLDEN_OPS];
  t64_replay_op prefix[MAX_GOLDEN_OPS];
  const size_t pre_count = golden_prequeue_ops(pre);
  const size_t jit_count = golden_jit_ops(jit);
  t64_machine *probe = new_machine(PROFILE_STEM, false);
  t64_replay_header live;
  size_t n = 0u;
  size_t i;
  CHECK(probe != NULL && compose_live_header(probe, &live));
  if (probe == NULL) return 1;
  t64_machine_destroy(probe);
  /* Step 1 over the prefix through the first checkpoint: the inputs, the
   * checkpoint at 160,000 and an end at the same tick (guest-bound). */
  for (i = 0u; i < GOLDEN_SCRIPT_COUNT; i++) prefix[n++] = pre[i];
  prefix[n++] = pre[GOLDEN_SCRIPT_COUNT];
  op_end(&prefix[n], golden_pre_ticks[0], golden_pre_digests[0]);
  prefix[n].stop = T64_REPLAY_STOP_GUEST_BOUND;
  n++;
  CHECK(play_golden(&live, prefix, n, 1u, false, false));
  CHECK(play_golden(&live, pre, pre_count, 7u, false, false));
  CHECK(play_golden(&live, pre, pre_count, 4096u, false, false));
  CHECK(play_golden(&live, pre, pre_count, 4096u, true, false));
  CHECK(play_golden(&live, pre, pre_count, 4096u, false, true));
  CHECK(play_golden(&live, jit, jit_count, 7u, false, false));
  CHECK(play_golden(&live, jit, jit_count, 4096u, true, true));
  return g_failures == 0 ? 0 : 1;
}

/* ---- queue-full-divergence --------------------------------------------------------------- */

static int case_queue_full_divergence(void) {
  enum { INPUTS = 300 };
  t64_replay_op *ops = (t64_replay_op *)calloc(INPUTS + 1u, sizeof *ops);
  t64_machine *counter = new_machine(PROFILE_STEM, false);
  t64_machine *machine = new_machine(PROFILE_STEM, false);
  t64_machine *reference = new_machine(PROFILE_STEM, false);
  t64_replay_header live;
  t64_replay *replay = NULL;
  t64_replay_report report;
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  uint64_t calls = 0u;
  size_t accepted = 0u;
  size_t i;
  CHECK(ops != NULL && counter != NULL && machine != NULL && reference != NULL);
  if (ops == NULL || counter == NULL || machine == NULL || reference == NULL) {
    free(ops);
    return 1;
  }
  CHECK(compose_live_header(machine, &live));
  /* An independent reference counts k: the accepted calls on a fresh admitted
   * machine before the first QUEUE_FULL. */
  for (i = 0u; i < INPUTS; i++) {
    t64_status status = t64_machine_queue_input(
        counter, 1000u + 1000u * (uint64_t)i, 0u, (uint16_t)(i + 1u),
        (int32_t)(i % 100u) - 50, 7);
    if (status == T64_STATUS_QUEUE_FULL) break;
    CHECK(status == T64_STATUS_OK);
    accepted++;
  }
  CHECK(accepted > 0u && accepted < INPUTS);
  for (i = 0u; i < INPUTS; i++) {
    golden_event event;
    event.event_tick = 1000u + 1000u * (uint64_t)i;
    event.buttons = (uint16_t)(i + 1u);
    event.x = (int32_t)(i % 100u) - 50;
    event.y = 7;
    op_input(&ops[i], 0u, &event);
  }
  op_end(&ops[INPUTS], 100u, GOLDEN_PRE_END_DIGEST);
  ops[INPUTS].stop = T64_REPLAY_STOP_GUEST_BOUND;
  /* The accepted baseline: exactly k inputs and an end whose digest comes from
   * the reference run to tick 100. */
  {
    uint64_t tick = 0u;
    uint32_t reason = 0u;
    t64_replay_op baseline[INPUTS + 1u];
    t64_replay *good = NULL;
    t64_machine *runner = new_machine(PROFILE_STEM, false);
    for (i = 0u; i < accepted; i++) {
      CHECK(t64_machine_queue_input(
                reference, 1000u + 1000u * (uint64_t)i, 0u,
                (uint16_t)(i + 1u), (int32_t)(i % 100u) - 50, 7) ==
            T64_STATUS_OK);
    }
    CHECK(advance_to(reference, &tick, 100u, 16u, &reason));
    CHECK(t64_machine_digest(reference, after) == T64_STATUS_OK);
    for (i = 0u; i < accepted; i++) baseline[i] = ops[i];
    memset(&baseline[accepted], 0, sizeof baseline[accepted]);
    baseline[accepted].kind = T64_REPLAY_OP_END;
    baseline[accepted].at = 100u;
    baseline[accepted].stop = T64_REPLAY_STOP_GUEST_BOUND;
    memcpy(baseline[accepted].digest, after, T64_DIGEST_SIZE);
    CHECK(make_replay(&live, baseline, accepted + 1u, &good));
    if (runner != NULL && good != NULL) {
      CHECK(play_on(runner, good, 16u, &report, &calls) ==
            T64_REPLAY_STEP_MATCH);
    }
    if (runner != NULL) t64_machine_destroy(runner);
    t64_replay_destroy(good);
  }
  /* The authored replay with 300 inputs parses; the player refuses op k. */
  CHECK(make_replay(&live, ops, INPUTS + 1u, &replay));
  if (replay != NULL) {
    CHECK(t64_machine_digest(machine, before) == T64_STATUS_OK);
    (void)before;
    CHECK(play_on(machine, replay, 16u, &report, &calls) ==
          T64_REPLAY_STEP_FAILED);
    CHECK(report_is(&report, T64_REPLAY_RESULT_REFUSED,
                    T64_REPLAY_DETAIL_QUEUE_FULL, "none"));
    CHECK(report.has_op_index && report.op_index == accepted);
    CHECK(report.ops_applied == accepted && report.live_tick == 0u);
    CHECK(calls == 0u);
    /* The refused op changed nothing: the machine equals the one holding the
     * k accepted calls. */
    {
      uint8_t reference_digest[T64_DIGEST_SIZE];
      t64_machine *held = new_machine(PROFILE_STEM, false);
      CHECK(held != NULL);
      for (i = 0u; i < accepted && held != NULL; i++) {
        CHECK(t64_machine_queue_input(
                  held, 1000u + 1000u * (uint64_t)i, 0u, (uint16_t)(i + 1u),
                  (int32_t)(i % 100u) - 50, 7) == T64_STATUS_OK);
      }
      CHECK(held != NULL &&
            t64_machine_digest(held, reference_digest) == T64_STATUS_OK &&
            t64_machine_digest(machine, after) == T64_STATUS_OK &&
            memcmp(reference_digest, after, T64_DIGEST_SIZE) == 0);
      if (held != NULL) t64_machine_destroy(held);
    }
  }
  t64_replay_destroy(replay);
  t64_machine_destroy(counter);
  t64_machine_destroy(machine);
  t64_machine_destroy(reference);
  free(ops);
  return g_failures == 0 ? 0 : 1;
}

/* ---- real suite: supervised runs ----------------------------------------------------- */

static const char *g_supervisor;
static const char *g_test_supervisor;
static const char *g_checker;
static const char *g_root;
static char g_evidence[PATH_MAX];

static bool join_path(char *out, size_t capacity, const char *a,
                      const char *b) {
  int length = snprintf(out, capacity, "%s/%s", a, b);
  return length > 0 && (size_t)length < capacity;
}

static bool prepare_evidence(void) {
  const char *configured = getenv("T64_REPLAY_EVIDENCE_DIR");
  const char *temp = getenv("TMPDIR");
  struct stat info;
  if (configured != NULL && configured[0] != '\0') {
    if (snprintf(g_evidence, sizeof g_evidence, "%s", configured) >=
        (int)sizeof g_evidence) {
      return false;
    }
    if (mkdir(g_evidence, 0700) != 0 && errno != EEXIST) return false;
    return stat(g_evidence, &info) == 0 && S_ISDIR(info.st_mode) &&
           t64_test_scope_open(g_evidence, sizeof g_evidence, false);
  }
  if (temp == NULL || temp[0] == '\0') temp = "/tmp";
  if (snprintf(g_evidence, sizeof g_evidence, "%s/t64-replay.XXXXXX", temp) >=
      (int)sizeof g_evidence) {
    return false;
  }
  return mkdtemp(g_evidence) != NULL &&
         t64_test_scope_open(g_evidence, sizeof g_evidence, true);
}

static bool make_run_dir(char out[PATH_MAX]) {
  if (snprintf(out, PATH_MAX, "%s/r07.XXXXXX", g_evidence) >= PATH_MAX) {
    return false;
  }
  return mkdtemp(out) != NULL;
}

/* A fresh owner-only artifact directory inside a fresh run directory. */
static bool make_artifact_dir(char out[PATH_MAX]) {
  char run[PATH_MAX];
  if (!make_run_dir(run) || !join_path(out, PATH_MAX, run, "art") ||
      mkdir(out, 0700) != 0) {
    return false;
  }
  return chmod(out, 0700) == 0;
}

/* Number of entries (hidden ones included); the first name is stored. */
static size_t list_directory(const char *dir, char first[256]) {
  DIR *handle = opendir(dir);
  struct dirent *entry;
  size_t count = 0u;
  first[0] = '\0';
  if (handle == NULL) return (size_t)-1;
  while ((entry = readdir(handle)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    if (count == 0u) (void)snprintf(first, 256u, "%s", entry->d_name);
    count++;
  }
  (void)closedir(handle);
  return count;
}

static bool file_mode_is(const char *path, mode_t want) {
  struct stat info;
  return lstat(path, &info) == 0 && S_ISREG(info.st_mode) &&
         (info.st_mode & 07777) == want;
}

static bool write_new_file(const char *path, const uint8_t *bytes,
                           size_t length) {
  size_t offset = 0u;
  int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (fd < 0) return false;
  while (offset < length) {
    ssize_t written = write(fd, bytes + offset, length - offset);
    if (written < 0 && errno == EINTR) continue;
    if (written <= 0) {
      (void)close(fd);
      return false;
    }
    offset += (size_t)written;
  }
  return close(fd) == 0;
}

static bool sha256_file_hex(const char *path, size_t *size_out,
                            char out[T64_SHA256_HEX_SIZE]) {
  static uint8_t bytes[T64_REPLAY_MAX_BYTES + 1u];
  size_t length = 0u;
  if (!read_file(path, bytes, sizeof bytes, &length) || length == 0u ||
      length > T64_REPLAY_MAX_BYTES) {
    return false;
  }
  sha_hex_of(bytes, length, out);
  *size_out = length;
  return true;
}

typedef struct {
  bool parsed;
  bool checker_ok;
  int supervisor_exit;
  char outcome[48];
  char detail[64];
  char stop_kind[48];
  char log_sha256[65];
  char state_digest[65];
  uint64_t guest_tick;
  uint64_t log_bytes;
  char dir[PATH_MAX];
} run_out;

static void copy_string(char *out, size_t capacity, const t64_json_node *node) {
  out[0] = '\0';
  if (node != NULL && node->type == T64_JSON_STRING && node->len < capacity) {
    memcpy(out, node->str, node->len);
    out[node->len] = '\0';
  }
}

static bool parse_record(const char *path, run_out *out) {
  uint8_t bytes[8192];
  size_t length = 0u;
  t64_json_doc doc = {0};
  const t64_json_node *stop;
  const t64_json_node *counts;
  uint64_t value = 0u;
  if (!read_file(path, bytes, sizeof bytes, &length) || length == 0u ||
      t64_json_parse(bytes, length, NULL, &doc, NULL) != 0) {
    return false;
  }
  stop = t64_json_get(doc.root, "guest_stop");
  counts = t64_json_get(doc.root, "counts");
  copy_string(out->outcome, sizeof out->outcome,
              t64_json_get(doc.root, "outcome"));
  copy_string(out->detail, sizeof out->detail,
              t64_json_get(doc.root, "detail"));
  copy_string(out->stop_kind, sizeof out->stop_kind,
              stop == NULL ? NULL : t64_json_get(stop, "kind"));
  copy_string(out->log_sha256, sizeof out->log_sha256,
              t64_json_get(doc.root, "log_sha256"));
  copy_string(out->state_digest, sizeof out->state_digest,
              t64_json_get(doc.root, "state_digest"));
  if (t64_json_u64_string(t64_json_get(doc.root, "guest_tick"), &value)) {
    out->guest_tick = value;
  }
  if (counts != NULL &&
      t64_json_u64_string(t64_json_get(counts, "log_bytes"), &value)) {
    out->log_bytes = value;
  }
  t64_json_free(&doc);
  return true;
}

static bool checker_accepts(const char *result_path) {
  pid_t pid = fork();
  int status = 0;
  if (pid < 0) return false;
  if (pid == 0) {
    char mode[] = "result";
    char *args[4];
    int null_fd = open("/dev/null", O_WRONLY);
    args[0] = (char *)(uintptr_t)g_checker;
    args[1] = mode;
    args[2] = (char *)(uintptr_t)result_path;
    args[3] = NULL;
    if (null_fd >= 0) {
      (void)dup2(null_fd, STDOUT_FILENO);
      (void)dup2(null_fd, STDERR_FILENO);
    }
    execv(g_checker, args);
    _exit(127);
  }
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) return false;
  }
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

/* A supervised real run. Every optional field left NULL (or 0) is omitted. */
typedef struct {
  const char *supervisor;
  const char *fixture;
  const char *input;        /* NULL: the canonical empty schedule */
  uint64_t bound;
  uint64_t advance_step;    /* test twin only */
  const char *artifact_dir;
  const char *artifact_max; /* text so refusal rows can pass odd values */
  const char *record_leaf;
  const char *interval;
  const char *play_path;
  const char *play_size;
  const char *play_sha;
  const char *report_leaf;
  const char *raw_capture_dir;
} run_spec;

#define MAX_ARGS 48u

static bool write_empty_schedule(char out[PATH_MAX]) {
  static const char text[] =
      "{\"events\":[],\"schema\":\"t64.input\",\"schema_version\":1}\n";
  if (!join_path(out, PATH_MAX, g_evidence, "empty.input.json")) return false;
  if (access(out, F_OK) == 0) return true;
  return write_new_file(out, (const uint8_t *)text, sizeof text - 1u) ||
         access(out, F_OK) == 0;
}

static bool run_supervised(const run_spec *spec, run_out *out) {
  char result_path[PATH_MAX];
  char meta_path[PATH_MAX];
  char capture_path[PATH_MAX];
  char input[PATH_MAX];
  char bound_text[24];
  char advance_text[24];
  char input_size_text[24];
  char input_sha_text[T64_SHA256_HEX_SIZE];
  char *list[MAX_ARGS];
  size_t count = 0u;
  uint8_t script[8192];
  size_t script_length = 0u;
  pid_t pid;
  int status = 0;
  memset(out, 0, sizeof *out);
  if (!make_run_dir(out->dir) ||
      !join_path(result_path, sizeof result_path, out->dir, "result.json") ||
      !join_path(meta_path, sizeof meta_path, out->dir, "meta.json") ||
      !join_path(capture_path, sizeof capture_path, out->dir, "capture.bin")) {
    return false;
  }
  if (spec->input != NULL) {
    (void)snprintf(input, sizeof input, "%s", spec->input);
  } else if (!write_empty_schedule(input)) {
    return false;
  }
  if (!read_file(input, script, sizeof script, &script_length) ||
      script_length == 0u || script_length >= sizeof script) {
    return false;
  }
  sha_hex_of(script, script_length, input_sha_text);
  (void)snprintf(input_size_text, sizeof input_size_text, "%zu", script_length);
  (void)snprintf(bound_text, sizeof bound_text, "%llu",
                 (unsigned long long)spec->bound);
  (void)snprintf(advance_text, sizeof advance_text, "%llu",
                 (unsigned long long)spec->advance_step);
#define ARG(text) \
  do { if (count < MAX_ARGS - 1u) list[count++] = (char *)(uintptr_t)(text); } while (0)
  ARG(spec->supervisor); ARG("--real");
  ARG("--result"); ARG(result_path);
  ARG("--meta"); ARG(meta_path);
  ARG("--work-dir"); ARG(out->dir);
  ARG("--wall-ms"); ARG("60000");
  if (spec->bound != 0u) { ARG("--bound"); ARG(bound_text); }
  ARG("--fixture"); ARG(spec->fixture);
  ARG("--input"); ARG(input);
  ARG("--input-size"); ARG(input_size_text);
  ARG("--input-sha256"); ARG(input_sha_text);
  ARG("--capture-out"); ARG(capture_path);
  if (spec->advance_step != 0u) { ARG("--advance-step"); ARG(advance_text); }
  if (spec->artifact_dir != NULL) {
    ARG("--artifact-dir"); ARG(spec->artifact_dir);
    ARG("--artifact-max-bytes"); ARG(spec->artifact_max);
  }
  if (spec->record_leaf != NULL) { ARG("--replay-record"); ARG(spec->record_leaf); }
  if (spec->interval != NULL) { ARG("--replay-checkpoint-interval"); ARG(spec->interval); }
  if (spec->play_path != NULL) { ARG("--replay-play"); ARG(spec->play_path); }
  if (spec->play_size != NULL) { ARG("--replay-play-size"); ARG(spec->play_size); }
  if (spec->play_sha != NULL) { ARG("--replay-play-sha256"); ARG(spec->play_sha); }
  if (spec->report_leaf != NULL) { ARG("--replay-report"); ARG(spec->report_leaf); }
  if (spec->raw_capture_dir != NULL) { ARG("--raw-capture-dir"); ARG(spec->raw_capture_dir); }
#undef ARG
  list[count] = NULL;
  pid = fork();
  if (pid < 0) return false;
  if (pid == 0) {
    int null_fd = open("/dev/null", O_WRONLY);
    if (null_fd >= 0) (void)dup2(null_fd, STDERR_FILENO);
    execv(spec->supervisor, list);
    _exit(127);
  }
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) return false;
  }
  out->supervisor_exit = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  out->parsed = parse_record(result_path, out);
  out->checker_ok = out->parsed && checker_accepts(result_path);
  return true;
}

static void expect_pass(const run_out *run) {
  CHECK(run->parsed);
  CHECK(run->checker_ok);
  CHECK(run->supervisor_exit == 0);
  CHECK(strcmp(run->outcome, "pass") == 0);
  CHECK(strcmp(run->detail, "positive-match") == 0);
  CHECK(strcmp(run->stop_kind, "authored-completion") == 0);
}

#define MIX_ROM_SHA256 \
  "79c59149052719baa7080c22d44a21c8d924add1f45b16ced31bb9810bf37df0"
/* The oracle value (fixtures/replay/mix_oracle.py --sha256) and the
 * independent shasum of the hand-derived text. */
#define MIX_TRANSCRIPT_SHA256 \
  "b16f6575b5f94bee4018f7ef12ad0ca0e1c8134a033479b78e7d8527d20eabe2"

static void expect_mix_pass(const run_out *run) {
  expect_pass(run);
  CHECK(run->log_bytes == 72u);
  CHECK(strcmp(run->log_sha256, MIX_TRANSCRIPT_SHA256) == 0);
}

/* The report fields read with the plain JSON parser (independent of the module's
 * checker). */
typedef struct {
  bool ok;
  char result[32];
  char detail[32];
  char field[32];
  char at[24];
  char live_tick[24];
  char actual[65];
  char expected[65];
  char replay_sha256[65];
  int64_t checkpoints_matched;
  int64_t ops_applied;
  int64_t op_index; /* -1 when null */
} report_view;

static bool read_report(const char *dir, const char *leaf, report_view *view,
                        uint8_t *raw, size_t raw_capacity, size_t *raw_length) {
  char path[PATH_MAX];
  size_t length = 0u;
  t64_json_doc doc = {0};
  const t64_json_node *node;
  memset(view, 0, sizeof *view);
  view->op_index = -1;
  if (!join_path(path, sizeof path, dir, leaf) ||
      !read_file(path, raw, raw_capacity, &length) || length == 0u ||
      t64_json_parse(raw, length, NULL, &doc, NULL) != 0) {
    return false;
  }
  if (raw_length != NULL) *raw_length = length;
  copy_string(view->result, sizeof view->result, t64_json_get(doc.root, "result"));
  copy_string(view->detail, sizeof view->detail, t64_json_get(doc.root, "detail"));
  copy_string(view->field, sizeof view->field, t64_json_get(doc.root, "field"));
  copy_string(view->at, sizeof view->at, t64_json_get(doc.root, "at"));
  copy_string(view->live_tick, sizeof view->live_tick,
              t64_json_get(doc.root, "live_tick"));
  copy_string(view->actual, sizeof view->actual,
              t64_json_get(doc.root, "actual_digest"));
  copy_string(view->expected, sizeof view->expected,
              t64_json_get(doc.root, "expected_digest"));
  copy_string(view->replay_sha256, sizeof view->replay_sha256,
              t64_json_get(doc.root, "replay_sha256"));
  node = t64_json_get(doc.root, "checkpoints_matched");
  if (node != NULL && node->type == T64_JSON_INT) view->checkpoints_matched = node->num;
  node = t64_json_get(doc.root, "ops_applied");
  if (node != NULL && node->type == T64_JSON_INT) view->ops_applied = node->num;
  node = t64_json_get(doc.root, "op_index");
  if (node != NULL && node->type == T64_JSON_INT) view->op_index = node->num;
  t64_json_free(&doc);
  view->ok = true;
  return true;
}

static bool mix_fixture_path(char out[PATH_MAX]) {
  (void)g_root;
  return snprintf(out, PATH_MAX, "%s", mix_rom_path()) < PATH_MAX;
}

/* One supervised record run of the mix ROM into a fresh artifact directory:
 * returns the published replay path in `replay_path`. */
static bool record_run(const char *supervisor, const char *interval,
                       char dir_out[PATH_MAX], char replay_path[PATH_MAX],
                       run_out *run) {
  char rom[PATH_MAX];
  run_spec spec = {0};
  if (!mix_fixture_path(rom) || !make_artifact_dir(dir_out) ||
      !join_path(replay_path, PATH_MAX, dir_out, "mix.rpl")) {
    return false;
  }
  spec.supervisor = supervisor;
  spec.fixture = rom;
  spec.input = mix_input_path();
  spec.bound = RUN_BOUND;
  spec.artifact_dir = dir_out;
  spec.artifact_max = "65536";
  spec.record_leaf = "mix.rpl";
  spec.interval = interval;
  return run_supervised(&spec, run);
}

/* One record shared by the controls that need only a recorded file (the cases
 * record-play-match and the first record of this process still run their own);
 * the cached run is the accepted record baseline of the same setup. */
static bool g_cached_valid;
static char g_cached_dir[PATH_MAX];
static char g_cached_path[PATH_MAX];
static run_out g_cached_run;

static bool record_run_cached(char dir_out[PATH_MAX], char replay_path[PATH_MAX],
                              run_out *run) {
  if (!g_cached_valid) {
    if (!record_run(g_supervisor, "160000", g_cached_dir, g_cached_path,
                    &g_cached_run)) {
      return false;
    }
    g_cached_valid = true;
  }
  (void)snprintf(dir_out, PATH_MAX, "%s", g_cached_dir);
  (void)snprintf(replay_path, PATH_MAX, "%s", g_cached_path);
  *run = g_cached_run;
  return true;
}

static bool play_run(const char *supervisor, uint64_t advance_step,
                     const char *replay_path, char dir_out[PATH_MAX],
                     run_out *run) {
  char rom[PATH_MAX];
  char sha[T64_SHA256_HEX_SIZE];
  char size_text[24];
  size_t size = 0u;
  run_spec spec = {0};
  if (!mix_fixture_path(rom) || !make_artifact_dir(dir_out) ||
      !sha256_file_hex(replay_path, &size, sha)) {
    return false;
  }
  (void)snprintf(size_text, sizeof size_text, "%zu", size);
  spec.supervisor = supervisor;
  spec.fixture = rom;
  spec.bound = RUN_BOUND;
  spec.advance_step = advance_step;
  spec.artifact_dir = dir_out;
  spec.artifact_max = "65536";
  spec.play_path = replay_path;
  spec.play_size = size_text;
  spec.play_sha = sha;
  spec.report_leaf = "mix.rpr";
  return run_supervised(&spec, run);
}

static int case_record_play_match(void) {
  char record_dir[PATH_MAX];
  char replay_path[PATH_MAX];
  char first[256];
  char play_dir[PATH_MAX];
  char twin_dir[PATH_MAX];
  run_out run;
  t64_replay_op want[MAX_GOLDEN_OPS];
  size_t want_count = golden_prequeue_ops(want);
  t64_replay *replay = NULL;
  t64_replay_error error;
  static uint8_t bytes[T64_REPLAY_MAX_BYTES];
  size_t length = 0u;
  uint8_t report_a[T64_REPLAY_REPORT_MAX_BYTES + 1u];
  uint8_t report_b[T64_REPLAY_REPORT_MAX_BYTES + 1u];
  size_t report_a_length = 0u;
  size_t report_b_length = 0u;
  report_view view;

  CHECK(record_run(g_supervisor, "160000", record_dir, replay_path, &run));
  expect_mix_pass(&run);
  CHECK(strcmp(run.state_digest, GOLDEN_PRE_END_DIGEST) == 0);
  /* Exactly one published file: private, with the replay extension. */
  CHECK(list_directory(record_dir, first) == 1u);
  CHECK(strcmp(first, "mix.rpl") == 0);
  CHECK(file_mode_is(replay_path, 0600));
  CHECK(read_file(replay_path, bytes, sizeof bytes, &length) && length > 0u);
  CHECK(t64_replay_parse(bytes, length, &replay, &error) == T64_REPLAY_OK);
  if (replay != NULL) {
    t64_machine *machine = new_machine(PROFILE_STEM, false);
    t64_replay_header live;
    CHECK(ops_equal(replay, want, want_count, "published"));
    CHECK(machine != NULL && compose_live_header(machine, &live));
    if (machine != NULL) {
      const t64_replay_header *got = t64_replay_header_get(replay);
      CHECK(strcmp(got->identity.profile_id, live.identity.profile_id) == 0);
      CHECK(strcmp(got->identity.profile_digest, live.identity.profile_digest) == 0);
      CHECK(got->identity.descriptor_version == live.identity.descriptor_version);
      CHECK(got->identity.machine_model_version ==
            live.identity.machine_model_version);
      CHECK(got->identity.state_serialization_version ==
            live.identity.state_serialization_version);
      CHECK(strcmp(got->identity.build_id, live.identity.build_id) == 0);
      CHECK(strcmp(got->identity.content_sha256, live.identity.content_sha256) == 0);
      CHECK(strcmp(got->identity.content_sha256, MIX_ROM_SHA256) == 0);
      CHECK(got->identity.content_size == live.identity.content_size);
      CHECK(got->identity.save_kind == T64_REPLAY_SAVE_BLANK);
      CHECK(strcmp(got->source_revision, live.source_revision) == 0);
      t64_machine_destroy(machine);
    }
  }
  /* Play: an empty schedule, the replay supplies every input. */
  CHECK(play_run(g_supervisor, 0u, replay_path, play_dir, &run));
  expect_mix_pass(&run);
  CHECK(list_directory(play_dir, first) == 1u);
  CHECK(strcmp(first, "mix.rpr") == 0);
  {
    char report_path[PATH_MAX];
    CHECK(join_path(report_path, sizeof report_path, play_dir, "mix.rpr"));
    CHECK(file_mode_is(report_path, 0600));
  }
  CHECK(read_report(play_dir, "mix.rpr", &view, report_a, sizeof report_a,
                    &report_a_length));
  CHECK(strcmp(view.result, "match") == 0);
  CHECK(strcmp(view.detail, "none") == 0);
  CHECK(strcmp(view.field, "none") == 0);
  CHECK(view.checkpoints_matched == (int64_t)GOLDEN_PRE_CHECKPOINTS);
  CHECK(view.ops_applied == (int64_t)want_count);
  CHECK(view.op_index == -1);
  if (replay != NULL) {
    char hex[T64_SHA256_HEX_SIZE];
    hex_of(t64_replay_sha256(replay), hex);
    CHECK(strcmp(view.replay_sha256, hex) == 0);
  }
  /* The test twin at advance step 7 gives the same report bytes. */
  CHECK(play_run(g_test_supervisor, 7u, replay_path, twin_dir, &run));
  expect_mix_pass(&run);
  CHECK(read_report(twin_dir, "mix.rpr", &view, report_b, sizeof report_b,
                    &report_b_length));
  CHECK(report_a_length == report_b_length &&
        memcmp(report_a, report_b, report_a_length) == 0);
  t64_replay_destroy(replay);
  return g_failures == 0 ? 0 : 1;
}

/* ---- real controls ------------------------------------------------------------------- */

/* Rewrites a recorded replay with one change and returns the new path. `which`:
 * 0 profile digest, 1 build id, 2 content sha256, 3 one checkpoint digest. */
static bool write_altered_replay(const char *source, int which,
                                 char dest[PATH_MAX]) {
  static uint8_t bytes[T64_REPLAY_MAX_BYTES];
  size_t length = 0u;
  t64_replay *replay = NULL;
  t64_replay_error error;
  t64_replay_header header;
  t64_replay_op *ops = NULL;
  uint8_t *encoded = NULL;
  size_t encoded_length = 0u;
  size_t count;
  size_t i;
  bool ok = false;
  char name[64];
  if (!read_file(source, bytes, sizeof bytes, &length) ||
      t64_replay_parse(bytes, length, &replay, &error) != T64_REPLAY_OK) {
    return false;
  }
  header = *t64_replay_header_get(replay);
  count = t64_replay_op_count(replay);
  ops = (t64_replay_op *)calloc(count, sizeof *ops);
  if (ops == NULL) goto done;
  for (i = 0u; i < count; i++) {
    if (!t64_replay_op_get(replay, i, &ops[i])) goto done;
  }
  if (which == 0) flip_hex(header.identity.profile_digest, 3u);
  if (which == 1) {
    flip_hex(header.identity.build_id, strlen(header.identity.build_id) - 1u);
  }
  if (which == 2) flip_hex(header.identity.content_sha256, 5u);
  if (which == 3) {
    /* the third checkpoint (op index 8, tick 480,000) */
    if (ops[GOLDEN_SCRIPT_COUNT + 2u].kind != T64_REPLAY_OP_CHECKPOINT) goto done;
    ops[GOLDEN_SCRIPT_COUNT + 2u].digest[0] ^= 0x10u;
  }
  (void)snprintf(name, sizeof name, "altered-%d.rpl", which);
  if (t64_replay_encode(&header, ops, count, &encoded, &encoded_length) !=
          T64_REPLAY_OK ||
      !make_run_dir(dest)) {
    goto done;
  }
  {
    char leaf[PATH_MAX];
    if (!join_path(leaf, sizeof leaf, dest, name)) goto done;
    (void)snprintf(dest, PATH_MAX, "%s", leaf);
  }
  ok = write_new_file(dest, encoded, encoded_length);
done:
  free(encoded);
  free(ops);
  t64_replay_destroy(replay);
  return ok;
}

/* An unlinked read-only private copy of a file: the runner accepts only such
 * snapshots (the supervisor makes the same kind). */
static int open_snapshot(const char *source, const char *directory) {
  static uint8_t bytes[T64_REPLAY_MAX_BYTES];
  size_t length = 0u;
  char name[PATH_MAX];
  size_t written = 0u;
  int fd;
  if (!read_file(source, bytes, sizeof bytes, &length) || length == 0u ||
      !join_path(name, sizeof name, directory, "snapshot.XXXXXX")) {
    return -1;
  }
  fd = mkstemp(name);
  if (fd < 0) return -1;
  while (written < length) {
    ssize_t count = write(fd, bytes + written, length - written);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) {
      (void)close(fd);
      (void)unlink(name);
      return -1;
    }
    written += (size_t)count;
  }
  (void)close(fd);
  fd = open(name, O_RDONLY);
  (void)unlink(name);
  return fd;
}

/* The runner binary run directly with a replay descriptor (the refusal record
 * reaches the checker only through the supervisor, so the runner's own
 * prelaunch refusal is read here). */
static bool run_runner_direct_play(const char *replay_path,
                                   const char *artifact_dir, run_out *out) {
  char runner[PATH_MAX];
  char stdout_path[PATH_MAX];
  char rom[PATH_MAX];
  char input[PATH_MAX];
  char fixture_fd_text[24];
  char input_fd_text[24];
  char replay_fd_text[24];
  char *slash;
  int fixture_fd;
  int input_fd;
  int replay_fd;
  pid_t pid;
  int status = 0;
  memset(out, 0, sizeof *out);
  if (snprintf(runner, sizeof runner, "%s", g_supervisor) >= (int)sizeof runner ||
      (slash = strrchr(runner, '/')) == NULL ||
      (size_t)(slash - runner) + sizeof "/t64-runner-real" > sizeof runner) {
    return false;
  }
  (void)snprintf(slash, sizeof runner - (size_t)(slash - runner),
                 "/t64-runner-real");
  if (!make_run_dir(out->dir) || !mix_fixture_path(rom) ||
      !write_empty_schedule(input) ||
      !join_path(stdout_path, sizeof stdout_path, out->dir, "runner.json")) {
    return false;
  }
  fixture_fd = open_snapshot(rom, out->dir);
  input_fd = open_snapshot(input, out->dir);
  replay_fd = open_snapshot(replay_path, out->dir);
  if (fixture_fd <= STDERR_FILENO || input_fd <= STDERR_FILENO ||
      replay_fd <= STDERR_FILENO) {
    return false;
  }
  (void)snprintf(fixture_fd_text, sizeof fixture_fd_text, "%d", fixture_fd);
  (void)snprintf(input_fd_text, sizeof input_fd_text, "%d", input_fd);
  (void)snprintf(replay_fd_text, sizeof replay_fd_text, "%d", replay_fd);
  pid = fork();
  if (pid < 0) return false;
  if (pid == 0) {
    char *args[24];
    size_t n = 0u;
    int out_fd = open(stdout_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    int null_fd = open("/dev/null", O_WRONLY);
    if (out_fd < 0 || dup2(out_fd, STDOUT_FILENO) < 0) _exit(126);
    if (null_fd >= 0) (void)dup2(null_fd, STDERR_FILENO);
    args[n++] = runner;
    args[n++] = (char *)(uintptr_t)"--fixture-fd"; args[n++] = fixture_fd_text;
    args[n++] = (char *)(uintptr_t)"--input-fd"; args[n++] = input_fd_text;
    args[n++] = (char *)(uintptr_t)"--profile"; args[n++] = (char *)(uintptr_t)PROFILE_STEM;
    args[n++] = (char *)(uintptr_t)"--guest-ticks"; args[n++] = (char *)(uintptr_t)"1500000";
    args[n++] = (char *)(uintptr_t)"--artifact-dir"; args[n++] = (char *)(uintptr_t)artifact_dir;
    args[n++] = (char *)(uintptr_t)"--artifact-max-bytes"; args[n++] = (char *)(uintptr_t)"65536";
    args[n++] = (char *)(uintptr_t)"--replay-fd"; args[n++] = replay_fd_text;
    args[n++] = (char *)(uintptr_t)"--replay-report"; args[n++] = (char *)(uintptr_t)"mix.rpr";
    args[n] = NULL;
    execv(runner, args);
    _exit(127);
  }
  (void)close(fixture_fd);
  (void)close(input_fd);
  (void)close(replay_fd);
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) return false;
  }
  out->supervisor_exit = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  out->parsed = parse_record(stdout_path, out);
  return out->parsed;
}

static void expect_supervised_refusal(const run_out *run) {
  /* A runner prelaunch refusal reaches the supervisor as a record that does
   * not carry the identities it already holds, so the supervised result is its
   * post-launch classification (09-02): harness-error, report-error stop,
   * detail capture-invalid, guest tick 0. */
  CHECK(run->parsed);
  CHECK(run->checker_ok);
  CHECK(strcmp(run->outcome, "harness-error") == 0);
  CHECK(strcmp(run->stop_kind, "report-error") == 0);
  CHECK(strcmp(run->detail, "capture-invalid") == 0);
  CHECK(run->guest_tick == 0u);
  CHECK(run->log_bytes == 0u);
}

static void expect_runner_refusal(const run_out *run) {
  CHECK(run->parsed);
  CHECK(strcmp(run->outcome, "harness-error") == 0);
  CHECK(strcmp(run->stop_kind, "refused") == 0);
  CHECK(strcmp(run->detail, "refusal") == 0);
  CHECK(run->guest_tick == 0u);
}

static int mismatch_control(int which, const char *want_field) {
  char record_dir[PATH_MAX];
  char replay_path[PATH_MAX];
  char altered[PATH_MAX];
  char play_dir[PATH_MAX];
  char direct_dir[PATH_MAX];
  char first[256];
  run_out run;
  uint8_t raw[T64_REPLAY_REPORT_MAX_BYTES + 1u];
  report_view view;
  CHECK(record_run_cached(record_dir, replay_path, &run));
  expect_mix_pass(&run);
  /* The accepted baseline in the same setup: the unaltered file plays. */
  CHECK(play_run(g_supervisor, 0u, replay_path, play_dir, &run));
  expect_mix_pass(&run);
  CHECK(read_report(play_dir, "mix.rpr", &view, raw, sizeof raw, NULL));
  CHECK(strcmp(view.result, "match") == 0);
  /* Exactly one header field altered: refused at guest tick 0. */
  CHECK(write_altered_replay(replay_path, which, altered));
  CHECK(play_run(g_supervisor, 0u, altered, play_dir, &run));
  expect_supervised_refusal(&run);
  CHECK(list_directory(play_dir, first) == 1u && strcmp(first, "mix.rpr") == 0);
  CHECK(read_report(play_dir, "mix.rpr", &view, raw, sizeof raw, NULL));
  CHECK(strcmp(view.result, "identity-mismatch") == 0);
  CHECK(strcmp(view.detail, "none") == 0);
  CHECK(strcmp(view.field, want_field) == 0);
  CHECK(strcmp(view.live_tick, "0") == 0);
  CHECK(view.checkpoints_matched == 0 && view.ops_applied == 0 &&
        view.op_index == -1);
  /* The runner binary reports its own prelaunch refusal record. */
  CHECK(make_artifact_dir(direct_dir));
  CHECK(run_runner_direct_play(replay_path, direct_dir, &run));
  if (strcmp(run.outcome, "pass") != 0) {
    (void)fprintf(stderr, "direct baseline: outcome %s detail %s stop %s\n",
                  run.outcome, run.detail, run.stop_kind);
  }
  CHECK(strcmp(run.outcome, "pass") == 0);
  CHECK(run_runner_direct_play(altered, direct_dir, &run));
  expect_runner_refusal(&run);
  return g_failures == 0 ? 0 : 1;
}

static int case_mismatch_profile(void) { return mismatch_control(0, "profile-digest"); }
static int case_mismatch_build(void) { return mismatch_control(1, "build-id"); }
static int case_mismatch_content(void) { return mismatch_control(2, "content-sha256"); }

static int case_play_diverged(void) {
  char record_dir[PATH_MAX];
  char replay_path[PATH_MAX];
  char altered[PATH_MAX];
  char play_dir[PATH_MAX];
  run_out run;
  uint8_t raw[T64_REPLAY_REPORT_MAX_BYTES + 1u];
  report_view view;
  uint8_t expected_altered[T64_DIGEST_SIZE];
  char want_expected[T64_SHA256_HEX_SIZE];
  CHECK(record_run_cached(record_dir, replay_path, &run));
  expect_mix_pass(&run);
  CHECK(play_run(g_supervisor, 0u, replay_path, play_dir, &run));
  expect_mix_pass(&run);
  /* One checkpoint digest altered (the third, op index 8, tick 480,000). */
  CHECK(write_altered_replay(replay_path, 3, altered));
  CHECK(play_run(g_supervisor, 0u, altered, play_dir, &run));
  CHECK(run.parsed && run.checker_ok);
  CHECK(strcmp(run.outcome, "harness-error") == 0);
  CHECK(strcmp(run.detail, "host-error") == 0);
  CHECK(strcmp(run.stop_kind, "host-error") == 0);
  CHECK(read_report(play_dir, "mix.rpr", &view, raw, sizeof raw, NULL));
  CHECK(strcmp(view.result, "diverged") == 0);
  CHECK(strcmp(view.detail, "digest") == 0);
  CHECK(strcmp(view.field, "none") == 0);
  CHECK(view.op_index == (int64_t)(GOLDEN_SCRIPT_COUNT + 2u));
  CHECK(strcmp(view.at, "480000") == 0 && strcmp(view.live_tick, "480000") == 0);
  CHECK(view.checkpoints_matched == 2);
  CHECK(view.ops_applied == (int64_t)(GOLDEN_SCRIPT_COUNT + 2u));
  CHECK(hex_to_digest(golden_pre_digests[2], expected_altered));
  expected_altered[0] ^= 0x10u;
  hex_of(expected_altered, want_expected);
  CHECK(strcmp(view.expected, want_expected) == 0);
  CHECK(strcmp(view.actual, golden_pre_digests[2]) == 0);
  return g_failures == 0 ? 0 : 1;
}

/* Refused before the guest starts: no zero exit, guest tick 0 when a record
 * exists and nothing published. */
static void expect_refused_before_guest(const char *name, const run_out *run,
                                        const char *dir, bool published_ok) {
  char first[256];
  size_t entries = list_directory(dir, first);
  if (run->supervisor_exit == 0) {
    (void)fprintf(stderr, "row %s: supervisor exit 0\n", name);
    g_failures++;
  }
  if (run->parsed &&
      (strcmp(run->outcome, "harness-error") != 0 || run->guest_tick != 0u)) {
    (void)fprintf(stderr, "row %s: outcome %s tick %llu\n", name, run->outcome,
                  (unsigned long long)run->guest_tick);
    g_failures++;
  }
  if (!published_ok && entries != 0u) {
    (void)fprintf(stderr, "row %s: %zu files published\n", name, entries);
    g_failures++;
  }
}

static int case_flag_combinations(void) {
  char record_dir[PATH_MAX];
  char replay_path[PATH_MAX];
  char dir[PATH_MAX];
  char rom[PATH_MAX];
  char sha[T64_SHA256_HEX_SIZE];
  char size_text[24];
  size_t size = 0u;
  run_out run;
  run_spec spec;
  size_t i;
  /* The accepted record baseline. */
  CHECK(record_run_cached(record_dir, replay_path, &run));
  expect_mix_pass(&run);
  CHECK(mix_fixture_path(rom) && sha256_file_hex(replay_path, &size, sha));
  (void)snprintf(size_text, sizeof size_text, "%zu", size);
#define BASE_RECORD(spec_name)                          \
  memset(&(spec_name), 0, sizeof(spec_name));           \
  (spec_name).supervisor = g_supervisor;                \
  (spec_name).fixture = rom;                            \
  (spec_name).input = mix_input_path();                 \
  (spec_name).bound = RUN_BOUND;                        \
  (spec_name).artifact_dir = dir;                       \
  (spec_name).artifact_max = "65536";                   \
  (spec_name).record_leaf = "mix.rpl";                  \
  (spec_name).interval = "160000"
#define BASE_PLAY(spec_name)                            \
  memset(&(spec_name), 0, sizeof(spec_name));           \
  (spec_name).supervisor = g_supervisor;                \
  (spec_name).fixture = rom;                            \
  (spec_name).bound = RUN_BOUND;                        \
  (spec_name).artifact_dir = dir;                       \
  (spec_name).artifact_max = "65536";                   \
  (spec_name).play_path = replay_path;                  \
  (spec_name).play_size = size_text;                    \
  (spec_name).play_sha = sha;                           \
  (spec_name).report_leaf = "mix.rpr"
  /* record together with play */
  CHECK(make_artifact_dir(dir));
  BASE_RECORD(spec);
  spec.play_path = replay_path; spec.play_size = size_text;
  spec.play_sha = sha; spec.report_leaf = "mix.rpr";
  CHECK(run_supervised(&spec, &run));
  expect_refused_before_guest("record-with-play", &run, dir, false);
  /* record without the artifact pair */
  CHECK(make_artifact_dir(dir));
  BASE_RECORD(spec);
  spec.artifact_dir = NULL;
  CHECK(run_supervised(&spec, &run));
  expect_refused_before_guest("record-no-pair", &run, dir, false);
  /* play without a report leaf */
  CHECK(make_artifact_dir(dir));
  BASE_PLAY(spec);
  spec.report_leaf = NULL;
  CHECK(run_supervised(&spec, &run));
  expect_refused_before_guest("play-no-report", &run, dir, false);
  /* play with a non-empty schedule: the runner refuses after the pair is
   * accepted; the published report says refused with input-refused. */
  CHECK(make_artifact_dir(dir));
  BASE_PLAY(spec);
  spec.input = mix_input_path();
  CHECK(run_supervised(&spec, &run));
  expect_refused_before_guest("play-non-empty-schedule", &run, dir, true);
  {
    report_view view;
    uint8_t raw[T64_REPLAY_REPORT_MAX_BYTES + 1u];
    CHECK(read_report(dir, "mix.rpr", &view, raw, sizeof raw, NULL));
    CHECK(strcmp(view.result, "refused") == 0 &&
          strcmp(view.detail, "input-refused") == 0);
  }
  /* intervals: 0, 17 (not a multiple of 16) and one above the bound */
  {
    static const char *const intervals[] = {"0", "17", "1500016"};
    for (i = 0u; i < sizeof intervals / sizeof intervals[0]; i++) {
      CHECK(make_artifact_dir(dir));
      BASE_RECORD(spec);
      spec.interval = intervals[i];
      CHECK(run_supervised(&spec, &run));
      expect_refused_before_guest(intervals[i], &run, dir, false);
    }
  }
  /* leaves: a slash, an uppercase letter and a four-letter extension */
  {
    static const char *const leaves[] = {"a/b.rpl", "MIX.rpl", "mix.rplx"};
    for (i = 0u; i < sizeof leaves / sizeof leaves[0]; i++) {
      CHECK(make_artifact_dir(dir));
      BASE_RECORD(spec);
      spec.record_leaf = leaves[i];
      CHECK(run_supervised(&spec, &run));
      expect_refused_before_guest(leaves[i], &run, dir, false);
    }
  }
  /* replay with raw capture */
  {
    char raw_dir[PATH_MAX];
    CHECK(make_artifact_dir(dir));
    CHECK(make_artifact_dir(raw_dir));
    BASE_RECORD(spec);
    spec.raw_capture_dir = raw_dir;
    CHECK(run_supervised(&spec, &run));
    expect_refused_before_guest("record-with-raw-capture", &run, dir, false);
  }
#undef BASE_RECORD
#undef BASE_PLAY
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
    {"mutation-matrix", "unit", case_mutation_matrix},
    {"report-record", "unit", case_report_record},
    {"independent-original-run", "unit", case_independent_original_run},
    {"partition", "unit", case_partition},
    {"at-matters", "unit", case_at_matters},
    {"queue-full-divergence", "unit", case_queue_full_divergence},
    {"identity-binding", "unit", case_identity_binding},
    {"record-play-match", "real", case_record_play_match},
    {"mismatch-profile", "real", case_mismatch_profile},
    {"mismatch-build", "real", case_mismatch_build},
    {"mismatch-content", "real", case_mismatch_content},
    {"play-diverged", "real", case_play_diverged},
    {"flag-combinations", "real", case_flag_combinations},
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
    if (strcmp(argv[2], "real") == 0) {
      if (argc != 7) return 2;
      g_supervisor = argv[3];
      g_test_supervisor = argv[4];
      g_checker = argv[5];
      g_root = argv[6];
      if (!prepare_evidence()) return 2;
    }
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
