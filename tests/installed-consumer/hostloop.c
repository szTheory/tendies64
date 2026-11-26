/*
 * A host-shaped loop through the installed package, no SDL (Phase 11, plan
 * 11-19, VER-05, API-01, DR-02). The host advances the machine in frame-sized
 * slices of guest ticks, queues each input event before the frame that contains
 * its tick, drains video, then audio, then ISViewer text in that order, treats a
 * RESOURCE_LIMIT stop that moved nothing as backpressure (drain, then resume),
 * and fails with "stall without progress" when the advance after a full drain
 * still moves nothing.
 *
 * What the cases show:
 *
 *   hostloop          a stalled host (no drain for eight frames) is forced into a
 *                     backpressure stop and resumes after a drain; the outputs
 *                     equal the independent literals; a host that never drains is
 *                     detected as "stall without progress" (the detector can fail)
 *   hostloop-cadence  drain every slice, every third slice, only on a stop, and
 *                     not at all until the first backpressure stop: four runs with
 *                     equal digests and equal outputs
 *   hostloop-delay    1 ms of host delay after every advance: the same digest as
 *                     the run without delay
 *   two-instance      two machines with disjoint counting allocators advance
 *                     alternately slice by slice with different slice sizes and
 *                     drain cadences; each equals the single-instance literals,
 *                     and destroying the first leaves the second untouched (the
 *                     second then meets its own backpressure stop and resumes)
 *
 * Host wall time is never an input: this file reads no clock and hands the
 * library nothing but guest ticks. The delay is a sleep, never a value.
 *
 *   t64c --case hostloop|hostloop-cadence|hostloop-delay|two-instance --rom PATH
 */
#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <tendies64/t64_machine.h>

#include "t64c_alloc.h"
#include "t64c_cases.h"
#include "t64c_check.h"
#include "t64c_composite.h"
#include "t64c_sha256.h"

/* The host's frame budget in guest ticks. It is a host policy value: a real 60 Hz
 * frame at the 62,500,000 ticks per second of the profile would be 1,041,666 ticks
 * and would span the whole composite run, so the loop uses a smaller slice that
 * gives the fixture about 74 frames to present. */
#define HL_FRAME_TICKS UINT64_C(8192)
/* The stalled host of the hostloop case drains nothing in frames 64 to 71 (guest
 * ticks 524288 to 589823), which holds the composite's first video packets. */
#define HL_STALL_FROM 64u
#define HL_STALL_TO 72u
#define HL_MAX_ADVANCES 4096u

typedef enum {
  HL_EVERY,            /* drain after every advance */
  HL_EVERY_THIRD,      /* drain after the last advance of every third frame */
  HL_ON_STOP,          /* no scheduled drain: only backpressure and completion */
  HL_SKIP_THEN_EVERY,  /* no drain until the first backpressure stop, then every advance */
  HL_STALLED_HOST,     /* every advance except in frames HL_STALL_FROM..HL_STALL_TO-1 */
  HL_NEVER             /* a broken host that never drains (the seeded failure) */
} hl_policy;

typedef struct {
  t64c_fixture *fixture;
  t64c_run *run;
  hl_policy policy;
  uint64_t slice;
  bool delay;
  unsigned frame;               /* host frames completed */
  unsigned advances;
  unsigned drains;
  unsigned sleeps;
  unsigned budget_stops;        /* RESOURCE_LIMIT stops that moved the tick */
  unsigned backpressure_stops;  /* RESOURCE_LIMIT stops that moved nothing */
  uint64_t first_backpressure_tick;
  bool must_progress;           /* the next advance must move or end differently */
  bool input_queued[2];
  bool done;
  const char *error;
} hl_host;

static const char *hl_policy_name(hl_policy policy) {
  switch (policy) {
    case HL_EVERY: return "every-slice";
    case HL_EVERY_THIRD: return "every-third-slice";
    case HL_ON_STOP: return "only-on-a-stop";
    case HL_SKIP_THEN_EVERY: return "skip-until-first-stop";
    case HL_STALLED_HOST: return "stalled-host";
    case HL_NEVER: return "never-drains";
    default: return "unknown";
  }
}

/* The name a run is printed under: the policy, and the injected delay when on. */
static const char *hl_run_name(const hl_host *host) {
  if (host->delay && host->policy == HL_EVERY) {
    return "every-slice-with-1ms-delay";
  }
  return hl_policy_name(host->policy);
}

static bool hl_host_open(hl_host *host, const char *rom_path, hl_policy policy,
                         uint64_t slice, bool delay) {
  (void)memset(host, 0, sizeof *host);
  host->fixture = t64c_fixture_new(rom_path);
  host->run = (t64c_run *)calloc(1u, sizeof *host->run);
  host->policy = policy;
  host->slice = slice;
  host->delay = delay;
  if (host->fixture == NULL || host->run == NULL) {
    free(host->run);
    t64c_fixture_free(host->fixture);
    host->run = NULL;
    host->fixture = NULL;
    return false;
  }
  if (t64c_fixture_admit(host->fixture) != T64_STATUS_OK) {
    (void)fprintf(stderr, "t64c: admission of the composite ROM failed\n");
    free(host->run);
    t64c_fixture_free(host->fixture);
    host->run = NULL;
    host->fixture = NULL;
    return false;
  }
  return true;
}

static void hl_host_close(hl_host *host) {
  free(host->run);
  host->run = NULL;
  t64c_fixture_free(host->fixture);
  host->fixture = NULL;
}

/* Queue every input event whose tick lies in the coming frame and was not queued
 * yet: an input is queued before the frame that holds its tick, never earlier. */
static bool hl_queue_due_inputs(hl_host *host, uint64_t frame_end) {
  static const uint64_t tick[2] = {T64C_LIT_INPUT0_TICK, T64C_LIT_INPUT1_TICK};
  static const uint32_t port[2] = {T64C_LIT_INPUT0_PORT, T64C_LIT_INPUT1_PORT};
  static const uint16_t buttons[2] = {(uint16_t)T64C_LIT_INPUT0_BUTTONS,
                                      (uint16_t)T64C_LIT_INPUT1_BUTTONS};
  static const int32_t axis_x[2] = {T64C_LIT_INPUT0_AXIS_X, T64C_LIT_INPUT1_AXIS_X};
  static const int32_t axis_y[2] = {T64C_LIT_INPUT0_AXIS_Y, T64C_LIT_INPUT1_AXIS_Y};
  size_t i;

  for (i = 0u; i < 2u; i++) {
    if (!host->input_queued[i] && tick[i] <= frame_end) {
      if (tick[i] < host->run->tick ||
          t64_machine_queue_input(host->fixture->machine, tick[i], port[i], buttons[i],
                                  axis_x[i], axis_y[i]) != T64_STATUS_OK) {
        host->error = "an input event was refused or already past";
        return false;
      }
      host->input_queued[i] = true;
    }
  }
  return true;
}

static void hl_sleep_one_millisecond(void) {
  struct timespec delay;

  delay.tv_sec = 0;
  delay.tv_nsec = 1000000L;
  (void)nanosleep(&delay, NULL);
}

/* The drain the cadence asks for after an advance that was not itself a stop. */
static bool hl_policy_drains(const hl_host *host, uint32_t reason) {
  switch (host->policy) {
    case HL_EVERY: return true;
    case HL_EVERY_THIRD:
      return host->frame % 3u == 2u && reason != T64_STOP_REASON_RESOURCE_LIMIT;
    case HL_ON_STOP: return false;
    case HL_SKIP_THEN_EVERY: return host->backpressure_stops > 0u;
    case HL_STALLED_HOST: return host->frame < HL_STALL_FROM || host->frame >= HL_STALL_TO;
    case HL_NEVER: return false;
    default: return false;
  }
}

/* One host frame: advance to the end of the frame's slice, re-advancing after a
 * resumable RESOURCE_LIMIT stop. False, with host->error set, on a failure. */
static bool hl_frame(hl_host *host) {
  const uint64_t frame_end = T64C_RUN_BOUND - host->run->tick > host->slice
                                 ? host->run->tick + host->slice
                                 : T64C_RUN_BOUND;

  if (!hl_queue_due_inputs(host, frame_end)) {
    return false;
  }
  for (;;) {
    t64_stop_result stop = T64_STOP_RESULT_INIT;
    const uint64_t before = host->run->tick;
    t64_status status;
    bool moved;
    bool drain;

    if (host->advances >= HL_MAX_ADVANCES) {
      host->error = "too many advances";
      return false;
    }
    status = t64_machine_advance(
        host->fixture->machine,
        &(t64_stop_request)T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, frame_end), &stop);
    if (status != T64_STATUS_OK || stop.tick < before || stop.tick > frame_end) {
      host->error = "advance failed or went outside its slice";
      return false;
    }
    host->advances++;
    moved = stop.tick > before;
    host->run->tick = stop.tick;
    host->run->stop_reason = stop.reason;
    if (host->must_progress) {
      /* The previous advance was a backpressure stop and the host drained: this
       * advance must move the guest or end for another reason. */
      if (!moved && stop.reason == T64_STOP_REASON_RESOURCE_LIMIT) {
        host->error = "stall without progress";
        return false;
      }
      host->must_progress = false;
    }
    drain = hl_policy_drains(host, stop.reason);
    if (stop.reason == T64_STOP_REASON_RESOURCE_LIMIT) {
      if (moved) {
        host->budget_stops++;
      } else {
        /* Nothing can run until the host drains: forced backpressure. A broken
         * host (HL_NEVER) does not drain even now. */
        if (host->backpressure_stops == 0u) {
          host->first_backpressure_tick = stop.tick;
        }
        host->backpressure_stops++;
        host->must_progress = true;
        drain = host->policy != HL_NEVER;
      }
    }
    if (stop.reason == T64_STOP_REASON_GUEST_COMPLETION && host->policy != HL_NEVER) {
      drain = true;
    }
    if (drain) {
      if (!t64c_drain_all(host->fixture, host->run)) {
        host->error = "a drain failed";
        return false;
      }
      host->drains++;
    }
    if (host->delay) {
      hl_sleep_one_millisecond();
      host->sleeps++;
    }
    if (stop.reason == T64_STOP_REASON_RESOURCE_LIMIT) {
      continue;
    }
    if (stop.reason == T64_STOP_REASON_GUEST_COMPLETION) {
      host->done = true;
      host->frame++;
      if (!t64c_finish_run(host->fixture, host->run)) {
        host->error = "the final save export or digest failed";
        return false;
      }
      return true;
    }
    if (stop.reason != T64_STOP_REASON_BOUND) {
      host->error = "unexpected stop reason";
      return false;
    }
    break;
  }
  host->frame++;
  if (host->run->tick >= T64C_RUN_BOUND) {
    host->error = "the run bound was reached before the guest completed";
    return false;
  }
  return true;
}

static bool hl_run_to_completion(hl_host *host) {
  while (!host->done) {
    if (!hl_frame(host)) {
      return false;
    }
  }
  return true;
}

static void hl_print_stats(const char *case_name, const hl_host *host) {
  char hex[T64C_SHA256_HEX_SIZE];

  t64c_sha256_hex(host->run->digest, hex);
  (void)printf("%s: run %s: frames=%u advances=%u drains=%u sleeps=%u budget-stops=%u "
               "backpressure-stops=%u first-backpressure-tick=%llu tick=%llu digest=%s\n",
               case_name, hl_run_name(host), host->frame, host->advances,
               host->drains, host->sleeps, host->budget_stops, host->backpressure_stops,
               (unsigned long long)host->first_backpressure_tick,
               (unsigned long long)host->run->tick, hex);
}

/* ---- the literals ----------------------------------------------------------- */

/* The outputs of a finished run against the independent literals of
 * composite.expected.json (video, audio, transcript, save). */
static bool hl_literals_equal(const t64c_run *run) {
  static const uint8_t video_literal[T64C_LIT_VIDEO_PACKET_BYTES] = T64C_LIT_VIDEO_PACKET_INIT;
  static const uint8_t audio_literal[T64C_LIT_AUDIO_PACKET_COUNT][T64C_LIT_AUDIO_PACKET_BYTES] = {
      T64C_LIT_AUDIO_PACKET0_INIT, T64C_LIT_AUDIO_PACKET1_INIT};
  static const uint8_t save_range[T64C_LIT_SAVE_RANGE0_BYTES] = T64C_LIT_SAVE_RANGE0_INIT;
  size_t i;

  if (run->video_count != T64C_PIN_VIDEO_PACKET_COUNT ||
      run->audio_count != T64C_LIT_AUDIO_PACKET_COUNT ||
      run->text_length != T64C_LIT_TRANSCRIPT_BYTES ||
      run->save_length != T64C_LIT_SAVE_BYTES) {
    return false;
  }
  for (i = 0u; i < run->video_count; i++) {
    if (run->video_width[i] != T64C_LIT_VIDEO_WIDTH ||
        run->video_height[i] != T64C_LIT_VIDEO_HEIGHT ||
        run->video_packet_bytes[i] != T64C_LIT_VIDEO_PACKET_BYTES ||
        memcmp(run->video[i], video_literal, sizeof video_literal) != 0) {
      return false;
    }
  }
  for (i = 0u; i < run->audio_count; i++) {
    if (run->audio_packet_bytes[i] != T64C_LIT_AUDIO_PACKET_BYTES ||
        memcmp(run->audio[i], audio_literal[i], T64C_LIT_AUDIO_PACKET_BYTES) != 0) {
      return false;
    }
  }
  if (memcmp(run->text, T64C_LIT_TRANSCRIPT_TEXT, T64C_LIT_TRANSCRIPT_BYTES) != 0) {
    return false;
  }
  for (i = 0u; i < T64C_LIT_SAVE_BYTES; i++) {
    const bool in_range = i >= T64C_LIT_SAVE_RANGE0_OFFSET &&
                          i < T64C_LIT_SAVE_RANGE0_OFFSET + T64C_LIT_SAVE_RANGE0_BYTES;
    const uint8_t expected = in_range ? save_range[i - T64C_LIT_SAVE_RANGE0_OFFSET] : 0u;

    if (run->save[i] != expected) {
      return false;
    }
  }
  return true;
}

/* The runner-recorded pins: completion tick, stop reason and final state digest.
 * These are agreement with the in-tree runner, not independent expectations. */
static bool hl_pins_equal(const t64c_run *run) {
  char hex[T64C_SHA256_HEX_SIZE];

  t64c_sha256_hex(run->digest, hex);
  return run->tick == T64C_PIN_COMPLETION_TICK &&
         run->stop_reason == T64C_PIN_STOP_REASON &&
         strcmp(hex, T64C_PIN_FINAL_DIGEST_HEX) == 0;
}

static void hl_check_run(t64c_tally *tally, const char *case_name, const hl_host *host) {
  const bool literals = host->done && hl_literals_equal(host->run);
  const bool pins = host->done && hl_pins_equal(host->run);

  hl_print_stats(case_name, host);
  tally->checks += 2u;
  if (!literals) {
    tally->failures++;
  }
  if (!pins) {
    tally->failures++;
  }
  (void)printf("%s: run %s: independent literal: 3 video, 2 audio, transcript and save: %s\n",
               case_name, hl_run_name(host), literals ? "equal" : "DIFFERENT");
  (void)printf("%s: run %s: %s: completion tick, stop reason, final digest: %s\n", case_name,
               hl_run_name(host), T64C_PIN_LABEL, pins ? "equal" : "DIFFERENT");
}

/* Two finished runs hold identical outputs and an identical final state. */
static bool hl_runs_equal(const t64c_run *a, const t64c_run *b) {
  size_t i;

  if (a->tick != b->tick || a->stop_reason != b->stop_reason ||
      memcmp(a->digest, b->digest, sizeof a->digest) != 0 || a->video_count != b->video_count ||
      a->audio_count != b->audio_count || a->text_length != b->text_length ||
      a->save_length != b->save_length) {
    return false;
  }
  for (i = 0u; i < a->video_count; i++) {
    if (memcmp(a->video[i], b->video[i], T64C_RUN_VIDEO_BYTES) != 0) {
      return false;
    }
  }
  for (i = 0u; i < a->audio_count; i++) {
    if (memcmp(a->audio[i], b->audio[i], T64C_RUN_AUDIO_BYTES) != 0) {
      return false;
    }
  }
  return memcmp(a->text, b->text, a->text_length) == 0 &&
         memcmp(a->save, b->save, a->save_length) == 0;
}

/* One complete run: open, run to completion, check against the literals and the
 * pins, then keep the host for further checks. */
static bool hl_complete_run(t64c_tally *tally, const char *case_name, hl_host *host,
                            const t64c_args *args, hl_policy policy, uint64_t slice,
                            bool delay) {
  bool ok;

  if (!hl_host_open(host, args->rom_path, policy, slice, delay)) {
    tally->checks++;
    tally->failures++;
    return false;
  }
  ok = hl_run_to_completion(host);
  tally->checks++;
  if (!ok) {
    tally->failures++;
    (void)fprintf(stderr, "t64c: %s: run %s failed: %s\n", case_name, hl_policy_name(policy),
                  host->error != NULL ? host->error : "no reason recorded");
  }
  hl_check_run(tally, case_name, host);
  return ok;
}

/* Destroy the machine of a finished host and check that nothing leaked. */
static void hl_close_balanced(t64c_tally *tally, hl_host *host) {
  t64_machine_destroy(host->fixture->machine);
  host->fixture->machine = NULL;
  T64C_CHECK(tally, host->fixture->ledger.outstanding == 0u);
  T64C_CHECK(tally, host->fixture->ledger.bad_releases == 0u);
  T64C_CHECK(tally, host->fixture->ledger.refusals == 0u);
  hl_host_close(host);
}

/* ---- hostloop --------------------------------------------------------------- */

static int case_hostloop(const t64c_args *args) {
  t64c_tally tally = {0ul, 0ul};
  hl_host host;
  hl_host broken;
  bool ok;

  /* The stalled host: it presents every frame except eight in the middle of the
   * run, so the video ring fills, the library stops with RESOURCE_LIMIT, the host
   * drains and the guest resumes and completes. */
  ok = hl_complete_run(&tally, "hostloop", &host, args, HL_STALLED_HOST, HL_FRAME_TICKS, false);
  if (ok) {
    T64C_CHECK(&tally, host.backpressure_stops >= 1u);
    T64C_CHECK(&tally, host.first_backpressure_tick > 0u &&
                           host.first_backpressure_tick < T64C_PIN_COMPLETION_TICK);
    T64C_CHECK(&tally, host.done && host.run->stop_reason == T64_STOP_REASON_GUEST_COMPLETION);
    (void)printf("hostloop: forced backpressure: %u stop(s) with RESOURCE_LIMIT at guest tick "
                 "%llu, resumed after a drain, completed at tick %llu\n",
                 host.backpressure_stops, (unsigned long long)host.first_backpressure_tick,
                 (unsigned long long)host.run->tick);
  }
  if (host.fixture != NULL) {
    hl_close_balanced(&tally, &host);
  }

  /* The seeded failure: a host that never drains is stopped by the stall
   * detector, so a resumable-looking stop cannot loop forever. */
  if (hl_host_open(&broken, args->rom_path, HL_NEVER, HL_FRAME_TICKS, false)) {
    const bool finished = hl_run_to_completion(&broken);

    T64C_CHECK(&tally, !finished);
    T64C_CHECK(&tally, broken.error != NULL && strcmp(broken.error, "stall without progress") == 0);
    (void)printf("hostloop: seeded host that never drains: %s (%s at guest tick %llu after %u "
                 "backpressure stop(s))\n",
                 finished ? "NOT DETECTED" : "detected",
                 broken.error != NULL ? broken.error : "no error",
                 (unsigned long long)broken.run->tick, broken.backpressure_stops);
    hl_close_balanced(&tally, &broken);
  } else {
    tally.checks++;
    tally.failures++;
  }
  return t64c_report("hostloop", &tally);
}

/* ---- hostloop-cadence ------------------------------------------------------- */

static int case_hostloop_cadence(const t64c_args *args) {
  static const hl_policy policies[4] = {HL_EVERY, HL_EVERY_THIRD, HL_ON_STOP,
                                        HL_SKIP_THEN_EVERY};
  t64c_tally tally = {0ul, 0ul};
  hl_host hosts[4];
  size_t i;
  unsigned equal_digests = 0u;
  uint64_t stop_tick = 0u;
  bool stop_tick_known = false;
  bool stop_ticks_agree = true;
  char hex[T64C_SHA256_HEX_SIZE];

  for (i = 0u; i < 4u; i++) {
    (void)hl_complete_run(&tally, "hostloop-cadence", &hosts[i], args, policies[i],
                          HL_FRAME_TICKS, false);
  }
  for (i = 1u; i < 4u; i++) {
    if (hosts[0].done && hosts[i].done && hl_runs_equal(hosts[0].run, hosts[i].run)) {
      equal_digests++;
    }
  }
  /* Four equal digests: the first run equals itself and the other three equal it. */
  T64C_CHECK_U32(&tally, equal_digests, 3u);
  for (i = 1u; i < 4u; i++) {
    T64C_CHECK(&tally, hosts[0].done && hosts[i].done && hl_runs_equal(hosts[0].run, hosts[i].run));
  }
  if (hosts[0].done) {
    t64c_sha256_hex(hosts[0].run->digest, hex);
    (void)printf("hostloop-cadence: four runs, one digest %s, equal outputs\n", hex);
  }
  /* The runs that skip draining meet backpressure: at least one RESOURCE_LIMIT stop
   * that moved nothing, and every such first stop is at the same guest tick. */
  T64C_CHECK(&tally, hosts[2].backpressure_stops >= 1u);
  T64C_CHECK(&tally, hosts[3].backpressure_stops >= 1u);
  T64C_CHECK_U32(&tally, hosts[0].backpressure_stops, 0u);
  for (i = 0u; i < 4u; i++) {
    if (hosts[i].backpressure_stops > 0u) {
      if (!stop_tick_known) {
        stop_tick = hosts[i].first_backpressure_tick;
        stop_tick_known = true;
      } else if (hosts[i].first_backpressure_tick != stop_tick) {
        stop_ticks_agree = false;
      }
    }
  }
  T64C_CHECK(&tally, stop_tick_known && stop_ticks_agree);
  (void)printf("hostloop-cadence: backpressure stop at guest tick %llu in every run that met one "
               "(skip-until-first-stop: %u RESOURCE_LIMIT stop(s) that moved nothing, "
               "only-on-a-stop: %u)\n",
               (unsigned long long)stop_tick, hosts[3].backpressure_stops,
               hosts[2].backpressure_stops);
  for (i = 0u; i < 4u; i++) {
    if (hosts[i].fixture != NULL) {
      hl_close_balanced(&tally, &hosts[i]);
    }
  }
  return t64c_report("hostloop-cadence", &tally);
}

/* ---- hostloop-delay --------------------------------------------------------- */

static int case_hostloop_delay(const t64c_args *args) {
  t64c_tally tally = {0ul, 0ul};
  hl_host plain;
  hl_host slow;
  bool plain_ok;
  bool slow_ok;

  plain_ok = hl_complete_run(&tally, "hostloop-delay", &plain, args, HL_EVERY, HL_FRAME_TICKS, false);
  slow_ok = hl_complete_run(&tally, "hostloop-delay", &slow, args, HL_EVERY, HL_FRAME_TICKS, true);
  T64C_CHECK(&tally, plain_ok && slow_ok);
  T64C_CHECK(&tally, slow.sleeps == slow.advances && slow.sleeps > 1u);
  T64C_CHECK(&tally, plain_ok && slow_ok && hl_runs_equal(plain.run, slow.run));
  (void)printf("hostloop-delay: %u host delays of 1 ms between advances; digest and outputs equal "
               "the run without delay: %s\n",
               slow.sleeps,
               plain_ok && slow_ok && hl_runs_equal(plain.run, slow.run) ? "yes" : "NO");
  if (plain.fixture != NULL) {
    hl_close_balanced(&tally, &plain);
  }
  if (slow.fixture != NULL) {
    hl_close_balanced(&tally, &slow);
  }
  return t64c_report("hostloop-delay", &tally);
}

/* ---- two-instance ----------------------------------------------------------- */

/* No storage block is held by both ledgers. */
static bool hl_ledgers_disjoint(const t64c_ledger *a, const t64c_ledger *b) {
  size_t i;
  size_t j;

  for (i = 0u; i < a->slot_count; i++) {
    for (j = 0u; j < b->slot_count; j++) {
      if (a->slots[i].data == b->slots[j].data) {
        return false;
      }
    }
  }
  return true;
}

static int case_two_instance(const t64c_args *args) {
  t64c_tally tally = {0ul, 0ul};
  hl_host first;
  hl_host second;
  uint8_t second_before[T64_DIGEST_SIZE];
  uint8_t second_after[T64_DIGEST_SIZE];
  size_t second_outstanding;
  size_t second_allocations;
  uint64_t second_tick_at_destroy;
  bool opened_first;
  bool opened_second;
  bool failed = false;

  /* Both machines exist before either advances: distinct allocators, distinct
   * storage. The first presents big frames and drains every slice; the second
   * presents half-size frames and drains only when the library stops it, so it
   * meets backpressure after the first is gone. */
  opened_first = hl_host_open(&first, args->rom_path, HL_EVERY, HL_FRAME_TICKS, false);
  opened_second = hl_host_open(&second, args->rom_path, HL_ON_STOP, HL_FRAME_TICKS / 2u, false);
  T64C_CHECK(&tally, opened_first && opened_second);
  if (!opened_first || !opened_second) {
    if (opened_first) {
      hl_host_close(&first);
    }
    if (opened_second) {
      hl_host_close(&second);
    }
    return t64c_report("two-instance", &tally);
  }
  T64C_CHECK(&tally, first.fixture->machine != second.fixture->machine);
  T64C_CHECK(&tally, first.fixture->ledger.allocations > 0u &&
                         second.fixture->ledger.allocations > 0u);
  T64C_CHECK(&tally, hl_ledgers_disjoint(&first.fixture->ledger, &second.fixture->ledger));
  T64C_CHECK(&tally, first.fixture->allocator.context == &first.fixture->ledger);
  T64C_CHECK(&tally, second.fixture->allocator.context == &second.fixture->ledger);
  T64C_CHECK(&tally, first.fixture->allocator.context != second.fixture->allocator.context);

  /* Alternate slice by slice until the first finishes. */
  while (!first.done && !failed) {
    if (!hl_frame(&first) || (!second.done && !hl_frame(&second))) {
      failed = true;
    }
  }
  T64C_CHECK(&tally, !failed);
  if (failed) {
    (void)fprintf(stderr, "t64c: two-instance: %s\n",
                  first.error != NULL ? first.error : (second.error != NULL ? second.error : "?"));
    hl_close_balanced(&tally, &first);
    hl_close_balanced(&tally, &second);
    return t64c_report("two-instance", &tally);
  }
  hl_print_stats("two-instance first", &first);
  T64C_CHECK(&tally, first.done && !second.done);
  T64C_CHECK(&tally, first.done && hl_literals_equal(first.run) && hl_pins_equal(first.run));
  (void)printf("two-instance: first instance finished at tick %llu while the second is at tick "
               "%llu (%u of its frames)\n",
               (unsigned long long)first.run->tick, (unsigned long long)second.run->tick,
               second.frame);

  /* Destroy the first while the second is in the middle of its run. */
  T64C_CHECK(&tally, t64_machine_digest(second.fixture->machine, second_before) == T64_STATUS_OK);
  second_outstanding = second.fixture->ledger.outstanding;
  second_allocations = second.fixture->ledger.allocations;
  second_tick_at_destroy = second.run->tick;
  T64C_CHECK(&tally, hl_ledgers_disjoint(&first.fixture->ledger, &second.fixture->ledger));
  hl_close_balanced(&tally, &first);
  T64C_CHECK(&tally, t64_machine_digest(second.fixture->machine, second_after) == T64_STATUS_OK);
  T64C_CHECK(&tally, memcmp(second_before, second_after, sizeof second_before) == 0);
  T64C_CHECK(&tally, second.fixture->ledger.outstanding == second_outstanding);
  T64C_CHECK(&tally, second.fixture->ledger.allocations == second_allocations);
  T64C_CHECK(&tally, second.fixture->ledger.bad_releases == 0u);
  (void)printf("two-instance: destroying the first left the second's digest and ledger unchanged "
               "at tick %llu\n",
               (unsigned long long)second_tick_at_destroy);

  /* The second runs alone to the end and equals the same literals. */
  T64C_CHECK(&tally, hl_run_to_completion(&second));
  hl_check_run(&tally, "two-instance second", &second);
  T64C_CHECK(&tally, second.backpressure_stops >= 1u);
  hl_close_balanced(&tally, &second);
  return t64c_report("two-instance", &tally);
}

/* ---- dispatch --------------------------------------------------------------- */

typedef struct {
  const char *name;
  int (*run)(const t64c_args *args);
} hl_case;

static const hl_case hl_cases[] = {
    {"hostloop", case_hostloop},
    {"hostloop-cadence", case_hostloop_cadence},
    {"hostloop-delay", case_hostloop_delay},
    {"two-instance", case_two_instance},
};

bool t64c_hostloop_known(const char *name) {
  size_t i;

  for (i = 0u; i < sizeof hl_cases / sizeof hl_cases[0]; i++) {
    if (strcmp(name, hl_cases[i].name) == 0) {
      return true;
    }
  }
  return false;
}

void t64c_hostloop_list(void) {
  size_t i;

  for (i = 0u; i < sizeof hl_cases / sizeof hl_cases[0]; i++) {
    (void)puts(hl_cases[i].name);
  }
}

int t64c_hostloop_run(const char *name, const t64c_args *args) {
  size_t i;

  for (i = 0u; i < sizeof hl_cases / sizeof hl_cases[0]; i++) {
    if (strcmp(name, hl_cases[i].name) == 0) {
      return hl_cases[i].run(args);
    }
  }
  return 2;
}
