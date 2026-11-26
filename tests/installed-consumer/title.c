/*
 * The title through the installed package (Phase 12, plan 12-05, API-06). The
 * authored title ROM (the single-screen homebrew of fixtures/title/title-spec.md)
 * runs on the installed library with the input scripts of the oracle, and every
 * output is compared with constants of t64c_title.h:
 *
 *   independent literals  video packet digests per checkpoint, audio packet and
 *                         stream digests, the ISViewer transcript, the 32,768-byte
 *                         SRAM save image (read through t64_machine_save_export:
 *                         the supervised runner cannot produce a save image of the
 *                         title, decision DR-34), and the refused-frame
 *                         expectations (from title.expected.json, the output of
 *                         title_oracle.py)
 *   recorded pins         completion tick, stop reason, final state digest and
 *                         video packet count of the short scenario
 *                         ("runner-recorded pin, not independent") and of the long
 *                         scenario ("consumer-recorded pin, not independent")
 *
 * The consumer derives no expected value from the core or the runner at run
 * time. Hashes are computed with the consumer's own SHA-256 (t64c_sha256.c),
 * never with the library's hash and never from captured_sha256.
 *
 *   t64c --case title         --rom PATH [--print] [--slice N]
 *   t64c --case title-persist --rom PATH [--print] [--slice N]
 *   t64c --case title-refused --rom PATH [--print] [--slice N]
 *
 * The cases of this file take the title ROM (title-refused: the refused-frame
 * ROM) as --rom; the composite cases take the composite ROM.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tendies64/t64_machine.h>

#include "t64c_cases.h"
#include "t64c_sha256.h"
#include "t64c_title.h"

#define T64C_TITLE_MAX_VIDEO 16u
#define T64C_TITLE_MAX_AUDIO 8u
#define T64C_TITLE_TEXT_MAX 512u
/* Ticks per advance (the default of the title cases; --slice N overrides it). */
#define T64C_TITLE_SLICE UINT64_C(65536)
/* The refused-frame ROM is advanced to this guest tick (plan 12-05, decision DR-34). */
#define T64C_TITLE_REFUSED_BOUND UINT64_C(5000000)
#define T64C_TITLE_REFUSED_MIN_PACKETS 3u

typedef struct {
  uint64_t tick;
  uint32_t port;
  uint32_t buttons;
  int32_t axis_x;
  int32_t axis_y;
} t64c_title_event;

typedef struct {
  uint32_t offset;
  uint8_t value;
} t64c_title_pair;

/* ---- the independent literals of the scenarios ---------------------------- */

static const t64c_title_event title_short_events[] = T64C_TLIT_SHORT_EVENTS_INIT;
static const char *const title_short_frames[] = T64C_TLIT_SHORT_FRAMES_INIT;
static const char *const title_short_audio[] = T64C_TLIT_SHORT_AUDIO_SHA256_INIT;
static const t64c_title_pair title_short_pairs[] = T64C_TLIT_SHORT_SAVE_NONZERO_INIT;

static const t64c_title_event title_long_events[] = T64C_TLIT_LONG_EVENTS_INIT;
static const char *const title_long_frames[] = T64C_TLIT_LONG_FRAMES_INIT;
static const char *const title_long_audio[] = T64C_TLIT_LONG_AUDIO_SHA256_INIT;
static const t64c_title_pair title_long_pairs[] = T64C_TLIT_LONG_SAVE_NONZERO_INIT;

static const t64c_title_event title_shifted_events[] = T64C_TLIT_SHIFTED_EVENTS_INIT;
static const char *const title_shifted_frames[] = T64C_TLIT_SHIFTED_FRAMES_INIT;
static const char *const title_shifted_audio[] = T64C_TLIT_SHIFTED_AUDIO_SHA256_INIT;
static const t64c_title_pair title_shifted_pairs[] = T64C_TLIT_SHIFTED_SAVE_NONZERO_INIT;

static const char *const title_persist_frames[] = T64C_TLIT_PERSIST_FRAMES_INIT;
static const char *const title_persist_audio[] = T64C_TLIT_PERSIST_AUDIO_SHA256_INIT;
static const t64c_title_pair title_persist_pairs[] = T64C_TLIT_PERSIST_SAVE_NONZERO_INIT;

typedef struct {
  const char *name;
  const t64c_title_event *events;
  size_t event_count;
  uint64_t bound;
  bool completes;
  const char *const *frames;
  size_t frame_count;
  const char *const *audio;
  size_t audio_count;
  const char *audio_stream_sha256;
  const char *transcript;
  size_t transcript_bytes;
  const char *transcript_sha256;
  const char *save_sha256;
  const t64c_title_pair *save_pairs;
  size_t save_pair_count;
} t64c_title_expect;

#define T64C_TITLE_EXPECT(UPPER, lower, events)                                      \
  {                                                                                  \
    #lower, (events), T64C_TLIT_##UPPER##_EVENT_COUNT, T64C_TLIT_##UPPER##_BOUND_TICKS, \
        T64C_TLIT_##UPPER##_COMPLETES != 0u, title_##lower##_frames,                 \
        T64C_TLIT_##UPPER##_FRAME_COUNT, title_##lower##_audio,                      \
        T64C_TLIT_##UPPER##_AUDIO_COUNT, T64C_TLIT_##UPPER##_AUDIO_STREAM_SHA256_HEX, \
        T64C_TLIT_##UPPER##_TRANSCRIPT_TEXT, T64C_TLIT_##UPPER##_TRANSCRIPT_BYTES,   \
        T64C_TLIT_##UPPER##_TRANSCRIPT_SHA256_HEX, T64C_TLIT_##UPPER##_SAVE_SHA256_HEX, \
        title_##lower##_pairs, T64C_TLIT_##UPPER##_SAVE_NONZERO_COUNT                \
  }

enum { TITLE_SHORT = 0, TITLE_LONG = 1, TITLE_SHIFTED = 2, TITLE_PERSIST = 3 };

static const t64c_title_expect title_expect[4] = {
    T64C_TITLE_EXPECT(SHORT, short, title_short_events),
    T64C_TITLE_EXPECT(LONG, long, title_long_events),
    T64C_TITLE_EXPECT(SHIFTED, shifted, title_shifted_events),
    T64C_TITLE_EXPECT(PERSIST, persist, NULL),
};

/* ---- one run ----------------------------------------------------------------- */

/* Everything one run delivers to the host. Packets are hashed as they are
 * drained, so a 49,152-byte frame is never kept. */
typedef struct {
  uint64_t tick;
  uint32_t stop_reason;
  unsigned advances;
  unsigned resource_stops;
  uint8_t digest[T64_DIGEST_SIZE];
  size_t video_count;
  bool video_shape_ok;
  uint8_t video_sha[T64C_TITLE_MAX_VIDEO][T64C_SHA256_SIZE];
  size_t audio_count;
  uint8_t audio_sha[T64C_TITLE_MAX_AUDIO][T64C_SHA256_SIZE];
  uint8_t audio_stream[T64C_TITLE_MAX_AUDIO * T64C_TLIT_AUDIO_PACKET_BYTES];
  size_t audio_stream_length;
  uint8_t text[T64C_TITLE_TEXT_MAX];
  size_t text_length;
  uint8_t save[T64C_TLIT_SAVE_BYTES];
  size_t save_length;
  uint8_t frame[T64C_TLIT_FRAME_BYTES];
} t64c_title_run;

/* What one run is asked to do: the ROM, the events queued before the first
 * advance, an optional save image imported after admission, the guest tick the
 * run is bounded at (it stops earlier at an authored completion). */
typedef struct {
  const char *rom_path;
  const t64c_title_event *events;
  size_t event_count;
  const uint8_t *import_image;
  uint64_t bound;
  uint64_t slice;
} t64c_title_plan;

static bool title_drain_video(const t64c_fixture *fixture, t64c_title_run *run) {
  for (;;) {
    t64_video_packet_info info = T64_VIDEO_PACKET_INFO_INIT;
    size_t copied = 0u;
    t64_status status = t64_video_info(fixture->machine, &info);

    if (status == T64_STATUS_NOT_READY) {
      return true;
    }
    if (status != T64_STATUS_OK || run->video_count >= T64C_TITLE_MAX_VIDEO ||
        info.packet_bytes != T64C_TLIT_FRAME_BYTES) {
      (void)fprintf(stderr, "t64c: video info refused or unexpected (%u)\n",
                    (unsigned)status);
      return false;
    }
    status = t64_video_copy(fixture->machine, run->frame, sizeof run->frame, &copied);
    if (status != T64_STATUS_OK || copied != T64C_TLIT_FRAME_BYTES) {
      (void)fprintf(stderr, "t64c: video copy failed (%u)\n", (unsigned)status);
      return false;
    }
    if (info.width != T64C_TLIT_WIDTH || info.height != T64C_TLIT_HEIGHT ||
        info.stride_bytes != T64C_TLIT_WIDTH * 4u) {
      run->video_shape_ok = false;
    }
    t64c_sha256(run->frame, T64C_TLIT_FRAME_BYTES, run->video_sha[run->video_count]);
    run->video_count++;
  }
}

static bool title_drain_audio(const t64c_fixture *fixture, t64c_title_run *run) {
  for (;;) {
    t64_audio_packet_info info = T64_AUDIO_PACKET_INFO_INIT;
    size_t copied = 0u;
    uint8_t *packet = run->audio_stream + run->audio_stream_length;
    t64_status status = t64_audio_info(fixture->machine, &info);

    if (status == T64_STATUS_NOT_READY) {
      return true;
    }
    if (status != T64_STATUS_OK || run->audio_count >= T64C_TITLE_MAX_AUDIO ||
        info.byte_length != T64C_TLIT_AUDIO_PACKET_BYTES) {
      (void)fprintf(stderr, "t64c: audio info refused or unexpected (%u)\n",
                    (unsigned)status);
      return false;
    }
    status = t64_audio_copy(fixture->machine, packet, T64C_TLIT_AUDIO_PACKET_BYTES,
                            &copied);
    if (status != T64_STATUS_OK || copied != T64C_TLIT_AUDIO_PACKET_BYTES) {
      (void)fprintf(stderr, "t64c: audio copy failed (%u)\n", (unsigned)status);
      return false;
    }
    t64c_sha256(packet, T64C_TLIT_AUDIO_PACKET_BYTES, run->audio_sha[run->audio_count]);
    run->audio_stream_length += T64C_TLIT_AUDIO_PACKET_BYTES;
    run->audio_count++;
  }
}

static bool title_drain_text(const t64c_fixture *fixture, t64c_title_run *run) {
  t64_output_drain_result result = T64_OUTPUT_DRAIN_RESULT_INIT;
  t64_status status;

  status = t64_machine_output_drain(fixture->machine, run->text + run->text_length,
                                    T64C_TITLE_TEXT_MAX - run->text_length, &result);
  if (status != T64_STATUS_OK ||
      result.copied_bytes > T64C_TITLE_TEXT_MAX - run->text_length) {
    (void)fprintf(stderr, "t64c: output drain failed (%u)\n", (unsigned)status);
    return false;
  }
  run->text_length += (size_t)result.copied_bytes;
  return true;
}

/* Host order: video, audio, then text. */
static bool title_drain_all(const t64c_fixture *fixture, t64c_title_run *run) {
  return title_drain_video(fixture, run) && title_drain_audio(fixture, run) &&
         title_drain_text(fixture, run);
}

/* Advance in slices up to the bound, draining after every advance, until the
 * guest completes or the bound is reached. A RESOURCE_LIMIT stop is resumable;
 * one that moved nothing is a stall only when the machine digest has not moved
 * either (a second such stop with an equal digest is permanent). */
static bool title_advance(const t64c_fixture *fixture, uint64_t bound, uint64_t slice,
                          t64c_title_run *run) {
  while (run->tick < bound) {
    const uint64_t target = bound - run->tick > slice ? run->tick + slice : bound;
    unsigned consecutive = 0u;
    bool have_suspect = false;
    uint8_t suspect[T64_DIGEST_SIZE];

    for (;;) {
      t64_stop_result stop = T64_STOP_RESULT_INIT;
      const uint64_t before_tick = run->tick;
      const size_t before_packets = run->video_count + run->audio_count;
      const size_t before_text = run->text_length;
      t64_status status = t64_machine_advance(
          fixture->machine,
          &(t64_stop_request)T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, target), &stop);

      if (status != T64_STATUS_OK || stop.tick < before_tick || stop.tick > target) {
        (void)fprintf(stderr, "t64c: advance failed (%u)\n", (unsigned)status);
        return false;
      }
      run->advances++;
      if (!title_drain_all(fixture, run)) {
        return false;
      }
      run->tick = stop.tick;
      run->stop_reason = stop.reason;
      if (stop.reason != T64_STOP_REASON_RESOURCE_LIMIT) {
        break;
      }
      run->resource_stops++;
      consecutive++;
      if (consecutive > 64u) {
        (void)fprintf(stderr, "t64c: too many consecutive resource stops at tick %llu\n",
                      (unsigned long long)stop.tick);
        return false;
      }
      if (stop.tick == before_tick &&
          run->video_count + run->audio_count == before_packets &&
          run->text_length == before_text) {
        uint8_t now[T64_DIGEST_SIZE];

        if (t64_machine_digest(fixture->machine, now) != T64_STATUS_OK) {
          return false;
        }
        if (have_suspect && memcmp(now, suspect, sizeof now) == 0) {
          (void)fprintf(stderr, "t64c: stalled at tick %llu\n",
                        (unsigned long long)stop.tick);
          return false;
        }
        memcpy(suspect, now, sizeof suspect);
        have_suspect = true;
      } else {
        have_suspect = false;
      }
    }
    if (run->stop_reason == T64_STOP_REASON_GUEST_COMPLETION) {
      return true;
    }
    if (run->stop_reason != T64_STOP_REASON_BOUND) {
      (void)fprintf(stderr, "t64c: unexpected stop reason %u\n",
                    (unsigned)run->stop_reason);
      return false;
    }
  }
  return true;
}

/* After the run: the exported SRAM save and the machine digest. */
static bool title_finish(const t64c_fixture *fixture, t64c_title_run *run) {
  t64_status status = t64_machine_save_export(
      fixture->machine, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_RAW, run->save, sizeof run->save,
      &run->save_length);

  if (status != T64_STATUS_OK) {
    (void)fprintf(stderr, "t64c: save export failed (%u)\n", (unsigned)status);
    return false;
  }
  status = t64_machine_digest(fixture->machine, run->digest);
  if (status != T64_STATUS_OK) {
    (void)fprintf(stderr, "t64c: digest failed (%u)\n", (unsigned)status);
    return false;
  }
  return true;
}

/* ---- reporting ------------------------------------------------------------------ */

static void title_independent(t64c_tally *tally, bool equal, const char *what) {
  tally->checks++;
  if (!equal) {
    tally->failures++;
  }
  (void)printf("independent literal: %s: %s\n", what, equal ? "equal" : "DIFFERENT");
}

static void title_pinned(t64c_tally *tally, bool equal, const char *label,
                         const char *what) {
  tally->checks++;
  if (!equal) {
    tally->failures++;
  }
  (void)printf("%s: %s: %s\n", label, what, equal ? "equal" : "DIFFERENT");
}

/* One run on its own machine and allocator ledger: create, admit, import, queue,
 * advance, finish, destroy. Every step is a check of the tally; the run record
 * is returned (NULL when the machine could not be created). */
static t64c_title_run *title_play(const t64c_title_plan *plan, t64c_tally *tally,
                                  const char *name) {
  t64c_fixture *fixture = t64c_fixture_new(plan->rom_path);
  t64c_title_run *run = (t64c_title_run *)calloc(1u, sizeof *run);
  size_t i;
  bool ran;

  if (fixture == NULL || run == NULL) {
    free(run);
    t64c_fixture_free(fixture);
    return NULL;
  }
  run->video_shape_ok = true;
  T64C_CHECK(tally, strcmp(fixture->profile->profile_id, T64C_TPIN_PROFILE_ID) == 0);
  T64C_CHECK_U32(tally, t64c_fixture_admit(fixture), T64_STATUS_OK);
  if (plan->import_image != NULL) {
    T64C_CHECK_U32(tally,
                   t64_machine_save_import(fixture->machine, T64_SAVE_SRAM_32K,
                                           T64_SAVE_FORMAT_RAW, plan->import_image,
                                           T64C_TLIT_SAVE_BYTES),
                   T64_STATUS_OK);
  }
  for (i = 0u; i < plan->event_count; i++) {
    const t64c_title_event *event = &plan->events[i];

    T64C_CHECK_U32(tally,
                   t64_machine_queue_input(fixture->machine, event->tick, event->port,
                                           (uint16_t)event->buttons, event->axis_x,
                                           event->axis_y),
                   T64_STATUS_OK);
  }
  ran = title_advance(fixture, plan->bound, plan->slice != 0u ? plan->slice : T64C_TITLE_SLICE,
                      run);
  T64C_CHECK(tally, ran);
  T64C_CHECK(tally, ran && title_finish(fixture, run));
  (void)printf("%s: ran to tick %llu (stop reason %u), %u advances, %u resource stops, "
               "%u video packets, %u audio packets, %u text bytes\n",
               name, (unsigned long long)run->tick, (unsigned)run->stop_reason, run->advances,
               run->resource_stops, (unsigned)run->video_count, (unsigned)run->audio_count,
               (unsigned)run->text_length);
  t64_machine_destroy(fixture->machine);
  fixture->machine = NULL;
  T64C_CHECK(tally, fixture->ledger.outstanding == 0u);
  T64C_CHECK(tally, fixture->ledger.bad_releases == 0u);
  T64C_CHECK(tally, fixture->ledger.refusals == 0u);
  t64c_fixture_free(fixture);
  return run;
}

/* The frame digests of a run against the literals, one line per checkpoint
 * ("blank" marks a packet equal to the blank digest). Returns the index of the
 * first checkpoint that differs, or SIZE_MAX when every checkpoint is equal. */
static size_t title_compare_frames(t64c_tally *tally, const char *name,
                                   const t64c_title_run *run, const char *const *literal,
                                   size_t literal_count, size_t compare_count) {
  size_t first_different = SIZE_MAX;
  size_t i;
  char what[96];

  (void)snprintf(what, sizeof what, "%s video packet count is %u", name,
                 (unsigned)compare_count);
  title_independent(tally, run->video_count == compare_count && compare_count <= literal_count,
                    what);
  for (i = 0u; i < run->video_count && i < compare_count && i < literal_count; i++) {
    char hex[T64C_SHA256_HEX_SIZE];
    bool equal;
    bool blank;

    t64c_sha256_hex(run->video_sha[i], hex);
    equal = strcmp(hex, literal[i]) == 0;
    blank = strcmp(hex, T64C_TLIT_BLANK_SHA256_HEX) == 0;
    (void)snprintf(what, sizeof what, "%s checkpoint %u%s", name, (unsigned)i,
                   blank ? " (blank)" : "");
    title_independent(tally, equal, what);
    if (!equal && first_different == SIZE_MAX) {
      first_different = i;
    }
  }
  return first_different;
}

static void title_compare_audio(t64c_tally *tally, const char *name,
                                const t64c_title_run *run, const t64c_title_expect *want) {
  bool packets_equal = run->audio_count == want->audio_count;
  char hex[T64C_SHA256_HEX_SIZE];
  char what[96];
  size_t i;

  for (i = 0u; i < run->audio_count && i < want->audio_count; i++) {
    t64c_sha256_hex(run->audio_sha[i], hex);
    packets_equal = packets_equal && strcmp(hex, want->audio[i]) == 0;
  }
  (void)snprintf(what, sizeof what, "%s audio: %u packets of 2048 bytes, each digest", name,
                 (unsigned)want->audio_count);
  title_independent(tally, packets_equal, what);
  t64c_sha256_hex_of(run->audio_stream, run->audio_stream_length, hex);
  (void)snprintf(what, sizeof what, "%s audio stream SHA-256 (consumer's own implementation)",
                 name);
  title_independent(tally, strcmp(hex, want->audio_stream_sha256) == 0, what);
}

static void title_compare_transcript(t64c_tally *tally, const char *name,
                                     const t64c_title_run *run,
                                     const t64c_title_expect *want) {
  char hex[T64C_SHA256_HEX_SIZE];
  char what[96];

  (void)snprintf(what, sizeof what, "%s transcript is %u bytes", name,
                 (unsigned)want->transcript_bytes);
  title_independent(tally, run->text_length == want->transcript_bytes, what);
  (void)snprintf(what, sizeof what, "%s transcript bytes", name);
  title_independent(tally,
                    run->text_length == want->transcript_bytes &&
                        memcmp(run->text, want->transcript, want->transcript_bytes) == 0,
                    what);
  t64c_sha256_hex_of(run->text, run->text_length, hex);
  (void)snprintf(what, sizeof what, "%s transcript SHA-256 (consumer's own implementation)",
                 name);
  title_independent(tally, strcmp(hex, want->transcript_sha256) == 0, what);
}

/* The save image read through t64_machine_save_export: zero except the oracle's
 * (offset, value) pairs, and the oracle's SHA-256. */
static void title_compare_save(t64c_tally *tally, const char *name, const t64c_title_run *run,
                               const t64c_title_pair *pairs, size_t pair_count,
                               const char *sha256_hex) {
  bool image_equal = run->save_length == T64C_TLIT_SAVE_BYTES;
  char hex[T64C_SHA256_HEX_SIZE];
  char what[96];
  size_t i;

  for (i = 0u; image_equal && i < T64C_TLIT_SAVE_BYTES; i++) {
    uint8_t expected = 0u;
    size_t k;

    for (k = 0u; k < pair_count; k++) {
      if (pairs[k].offset == i) {
        expected = pairs[k].value;
      }
    }
    image_equal = run->save[i] == expected;
  }
  (void)snprintf(what, sizeof what, "%s 32768-byte SRAM save image (save export)", name);
  title_independent(tally, image_equal, what);
  t64c_sha256_hex_of(run->save, run->save_length, hex);
  (void)snprintf(what, sizeof what, "%s save SHA-256 (consumer's own implementation)", name);
  title_independent(tally, strcmp(hex, sha256_hex) == 0, what);
}

/* No packet of an accepted scenario is the blank frame: a blank screen is
 * distinguishable from every checkpoint. */
static void title_compare_not_blank(t64c_tally *tally, const char *name,
                                    const t64c_title_run *run) {
  bool any_blank = false;
  char hex[T64C_SHA256_HEX_SIZE];
  char what[96];
  size_t i;

  for (i = 0u; i < run->video_count; i++) {
    t64c_sha256_hex(run->video_sha[i], hex);
    any_blank = any_blank || strcmp(hex, T64C_TLIT_BLANK_SHA256_HEX) == 0;
  }
  (void)snprintf(what, sizeof what, "%s: no packet is the blank frame", name);
  title_independent(tally, !any_blank, what);
}

/* Frames, audio, transcript, save, packet shape and the stop of one scenario. */
static void title_compare_scenario(t64c_tally *tally, const t64c_title_expect *want,
                                   const t64c_title_run *run) {
  char what[96];

  (void)title_compare_frames(tally, want->name, run, want->frames, want->frame_count,
                             want->frame_count);
  (void)snprintf(what, sizeof what, "%s every packet is %ux%u RGBA8 of %u bytes", want->name,
                 (unsigned)T64C_TLIT_WIDTH, (unsigned)T64C_TLIT_HEIGHT,
                 (unsigned)T64C_TLIT_FRAME_BYTES);
  title_independent(tally, run->video_shape_ok, what);
  title_compare_not_blank(tally, want->name, run);
  title_compare_audio(tally, want->name, run, want);
  title_compare_transcript(tally, want->name, run, want);
  title_compare_save(tally, want->name, run, want->save_pairs, want->save_pair_count,
                     want->save_sha256);
  if (want->completes) {
    (void)snprintf(what, sizeof what, "%s ends in an authored completion before tick %llu",
                   want->name, (unsigned long long)want->bound);
    title_independent(tally,
                      run->stop_reason == T64_STOP_REASON_GUEST_COMPLETION &&
                          run->tick < want->bound,
                      what);
  } else {
    (void)snprintf(what, sizeof what, "%s does not complete (stops at the bound %llu)",
                   want->name, (unsigned long long)want->bound);
    title_independent(tally,
                      run->stop_reason == T64_STOP_REASON_BOUND && run->tick == want->bound,
                      what);
  }
}

/* The machine-readable lines of a recorded run. */
static void title_print_run(const char *name, const t64c_title_run *run) {
  char hex[T64C_SHA256_HEX_SIZE];

  t64c_sha256_hex(run->digest, hex);
  (void)printf("print %s.final_digest %s\n", name, hex);
  (void)printf("print %s.completion_tick %llu\n", name, (unsigned long long)run->tick);
  (void)printf("print %s.stop_reason %s\n", name,
               run->stop_reason == T64_STOP_REASON_GUEST_COMPLETION ? "GUEST_COMPLETION"
                                                                    : "OTHER");
  (void)printf("print %s.video_packet_count %u\n", name, (unsigned)run->video_count);
  (void)printf("print %s.audio_packet_count %u\n", name, (unsigned)run->audio_count);
  (void)printf("print %s.transcript_bytes %u\n", name, (unsigned)run->text_length);
}

/* ---- the title case -------------------------------------------------------------- */

/* The runner-recorded pins of the short scenario: agreement, not independence. */
static void title_compare_runner_pins(t64c_tally *tally, const t64c_title_run *run) {
  char hex[T64C_SHA256_HEX_SIZE];
  char what[128];

  (void)printf("%s: source: %s\n", T64C_TPIN_RUNNER_LABEL, T64C_TPIN_RUNNER_SOURCE);
  (void)snprintf(what, sizeof what, "short completion tick %llu",
                 (unsigned long long)run->tick);
  title_pinned(tally, run->tick == T64C_TPIN_RUNNER_COMPLETION_TICK, T64C_TPIN_RUNNER_LABEL,
               what);
  title_pinned(tally, run->stop_reason == T64C_TPIN_RUNNER_STOP_REASON, T64C_TPIN_RUNNER_LABEL,
               "short stop reason " T64C_TPIN_RUNNER_STOP_REASON_NAME);
  t64c_sha256_hex(run->digest, hex);
  (void)snprintf(what, sizeof what, "short final state digest %s", hex);
  title_pinned(tally, strcmp(hex, T64C_TPIN_RUNNER_FINAL_DIGEST_HEX) == 0,
               T64C_TPIN_RUNNER_LABEL, what);
  title_pinned(tally, run->video_count == T64C_TPIN_RUNNER_VIDEO_PACKET_COUNT,
               T64C_TPIN_RUNNER_LABEL, "short video packet count");
}

int t64c_case_title(const t64c_args *args) {
  t64c_tally tally = {0ul, 0ul};
  t64c_title_plan plan;
  t64c_title_run *run;

  T64C_CHECK_U32(&tally, t64c_sha256_selfcheck(), 0u);
  memset(&plan, 0, sizeof plan);
  plan.rom_path = args->rom_path;
  plan.slice = args->slice;

  /* The short scenario: the independent literals and the runner-recorded pins. */
  plan.events = title_expect[TITLE_SHORT].events;
  plan.event_count = title_expect[TITLE_SHORT].event_count;
  plan.bound = title_expect[TITLE_SHORT].bound;
  run = title_play(&plan, &tally, "short");
  if (run == NULL) {
    return 2;
  }
  title_compare_scenario(&tally, &title_expect[TITLE_SHORT], run);
  title_compare_runner_pins(&tally, run);
  if (args->print) {
    title_print_run("short", run);
  }
  free(run);
  return t64c_report("title", &tally);
}
