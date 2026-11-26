/*
 * The composite fixture through the installed package (Phase 11, plan 11-18,
 * VER-05). The authored Phase 10 composite ROM runs on the installed library
 * with the two input events of fixtures/composite/composite.input.json, and
 * every output is compared with constants of t64c_composite.h:
 *
 *   independent literals  video packets, audio packets, the 188-byte ISViewer
 *                         transcript, the polled words, the 32,768-byte SRAM
 *                         save (from composite.expected.json, the output of
 *                         composite_oracle.py)
 *   runner-recorded pins  completion tick, stop reason, final state digest and
 *                         video packet count (from composite.runner-pins.json):
 *                         "runner-recorded pin, not independent"
 *
 * The consumer derives no expected value from the core or the runner at run
 * time. Hashes are computed with the consumer's own SHA-256 (t64c_sha256.c),
 * never with the library's hash and never from captured_sha256.
 *
 *   t64c --case composite --rom PATH [--print] [--out DIR] [--slice N]
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tendies64/t64_machine.h>

#include "t64c_cases.h"
#include "t64c_composite.h"
#include "t64c_sha256.h"

#define T64C_MAX_ROM_BYTES (64u * 1024u * 1024u)

/* ---- fixture -------------------------------------------------------------- */

static uint8_t *t64c_read_file(const char *path, size_t *length_out) {
  FILE *file = fopen(path, "rb");
  long size;
  uint8_t *bytes;

  if (file == NULL) {
    (void)fprintf(stderr, "t64c: cannot open the ROM file\n");
    return NULL;
  }
  if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) <= 0 ||
      (unsigned long)size > T64C_MAX_ROM_BYTES || fseek(file, 0, SEEK_SET) != 0) {
    (void)fclose(file);
    (void)fprintf(stderr, "t64c: the ROM file has no usable size\n");
    return NULL;
  }
  bytes = (uint8_t *)malloc((size_t)size);
  if (bytes == NULL || fread(bytes, 1u, (size_t)size, file) != (size_t)size) {
    free(bytes);
    (void)fclose(file);
    (void)fprintf(stderr, "t64c: cannot read the ROM file\n");
    return NULL;
  }
  (void)fclose(file);
  *length_out = (size_t)size;
  return bytes;
}

t64c_fixture *t64c_fixture_new(const char *rom_path) {
  t64c_fixture *fixture;
  t64_status status;

  if (rom_path == NULL) {
    (void)fprintf(stderr, "t64c: this case needs --rom PATH\n");
    return NULL;
  }
  fixture = (t64c_fixture *)calloc(1u, sizeof *fixture);
  if (fixture == NULL) {
    return NULL;
  }
  fixture->rom = t64c_read_file(rom_path, &fixture->rom_length);
  if (fixture->rom == NULL) {
    free(fixture);
    return NULL;
  }
  status = t64_profile_lookup(T64C_PROFILE_ID, &fixture->profile);
  if (status != T64_STATUS_OK || fixture->profile == NULL) {
    (void)fprintf(stderr, "t64c: profile lookup failed (%u)\n", (unsigned)status);
    t64c_fixture_free(fixture);
    return NULL;
  }
  fixture->allocator = (t64_host_allocator)T64_HOST_ALLOCATOR_INIT(
      t64c_allocate, t64c_release, &fixture->ledger);
  status = t64_machine_create(
      &(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(
          &fixture->allocator, fixture->profile),
      &fixture->machine);
  if (status != T64_STATUS_OK || fixture->machine == NULL) {
    (void)fprintf(stderr, "t64c: create failed (%u)\n", (unsigned)status);
    fixture->machine = NULL;
    t64c_fixture_free(fixture);
    return NULL;
  }
  return fixture;
}

void t64c_fixture_free(t64c_fixture *fixture) {
  if (fixture == NULL) {
    return;
  }
  if (fixture->machine != NULL) {
    t64_machine_destroy(fixture->machine);
  }
  free(fixture->rom);
  free(fixture);
}

t64_status t64c_fixture_admit(t64c_fixture *fixture) {
  return t64_machine_admit_rom(fixture->machine, fixture->rom,
                               fixture->rom_length);
}

bool t64c_queue_literal_inputs(t64c_fixture *fixture) {
  t64_status first = t64_machine_queue_input(
      fixture->machine, T64C_LIT_INPUT0_TICK, T64C_LIT_INPUT0_PORT,
      (uint16_t)T64C_LIT_INPUT0_BUTTONS, T64C_LIT_INPUT0_AXIS_X,
      T64C_LIT_INPUT0_AXIS_Y);
  t64_status second = t64_machine_queue_input(
      fixture->machine, T64C_LIT_INPUT1_TICK, T64C_LIT_INPUT1_PORT,
      (uint16_t)T64C_LIT_INPUT1_BUTTONS, T64C_LIT_INPUT1_AXIS_X,
      T64C_LIT_INPUT1_AXIS_Y);

  return first == T64_STATUS_OK && second == T64_STATUS_OK;
}

/* ---- run engine ----------------------------------------------------------- */

bool t64c_drain_video(t64c_fixture *fixture, t64c_run *run) {
  for (;;) {
    t64_video_packet_info info = T64_VIDEO_PACKET_INFO_INIT;
    size_t copied = 0u;
    t64_status status = t64_video_info(fixture->machine, &info);

    if (status == T64_STATUS_NOT_READY) {
      return true;
    }
    if (status != T64_STATUS_OK || run->video_count >= T64C_RUN_MAX_VIDEO ||
        info.packet_bytes != T64C_RUN_VIDEO_BYTES) {
      (void)fprintf(stderr, "t64c: video info refused or unexpected (%u)\n",
                    (unsigned)status);
      return false;
    }
    status = t64_video_copy(fixture->machine, run->video[run->video_count],
                            T64C_RUN_VIDEO_BYTES, &copied);
    if (status != T64_STATUS_OK || copied != T64C_RUN_VIDEO_BYTES) {
      (void)fprintf(stderr, "t64c: video copy failed (%u)\n", (unsigned)status);
      return false;
    }
    run->video_width[run->video_count] = info.width;
    run->video_height[run->video_count] = info.height;
    run->video_packet_bytes[run->video_count] = info.packet_bytes;
    run->video_count++;
  }
}

bool t64c_drain_audio(t64c_fixture *fixture, t64c_run *run) {
  for (;;) {
    t64_audio_packet_info info = T64_AUDIO_PACKET_INFO_INIT;
    size_t copied = 0u;
    t64_status status = t64_audio_info(fixture->machine, &info);

    if (status == T64_STATUS_NOT_READY) {
      return true;
    }
    if (status != T64_STATUS_OK || run->audio_count >= T64C_RUN_MAX_AUDIO ||
        info.byte_length != T64C_RUN_AUDIO_BYTES) {
      (void)fprintf(stderr, "t64c: audio info refused or unexpected (%u)\n",
                    (unsigned)status);
      return false;
    }
    status = t64_audio_copy(fixture->machine, run->audio[run->audio_count],
                            T64C_RUN_AUDIO_BYTES, &copied);
    if (status != T64_STATUS_OK || copied != T64C_RUN_AUDIO_BYTES) {
      (void)fprintf(stderr, "t64c: audio copy failed (%u)\n", (unsigned)status);
      return false;
    }
    run->audio_packet_bytes[run->audio_count] = info.byte_length;
    run->audio_count++;
  }
}

bool t64c_drain_text(t64c_fixture *fixture, t64c_run *run) {
  t64_output_drain_result result = T64_OUTPUT_DRAIN_RESULT_INIT;
  t64_status status;

  status = t64_machine_output_drain(fixture->machine, run->text + run->text_length,
                                    T64C_RUN_TEXT_MAX - run->text_length,
                                    &result);
  if (status != T64_STATUS_OK || result.copied_bytes > T64C_RUN_TEXT_MAX - run->text_length) {
    (void)fprintf(stderr, "t64c: output drain failed (%u)\n", (unsigned)status);
    return false;
  }
  run->text_length += (size_t)result.copied_bytes;
  return true;
}

bool t64c_drain_all(t64c_fixture *fixture, t64c_run *run) {
  return t64c_drain_video(fixture, run) && t64c_drain_audio(fixture, run) &&
         t64c_drain_text(fixture, run);
}

bool t64c_run_to_completion(t64c_fixture *fixture, uint64_t slice,
                            t64c_run *run) {
  if (slice == 0u) {
    slice = T64C_RUN_DEFAULT_SLICE;
  }
  while (run->tick < T64C_RUN_BOUND) {
    const uint64_t target = T64C_RUN_BOUND - run->tick > slice
                                ? run->tick + slice
                                : T64C_RUN_BOUND;
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
          &(t64_stop_request)T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, target),
          &stop);

      if (status != T64_STATUS_OK || stop.tick < before_tick || stop.tick > target) {
        (void)fprintf(stderr, "t64c: advance failed (%u)\n", (unsigned)status);
        return false;
      }
      run->advances++;
      if (!t64c_drain_all(fixture, run)) {
        return false;
      }
      run->tick = stop.tick;
      run->stop_reason = stop.reason;
      if (stop.reason != T64_STOP_REASON_RESOURCE_LIMIT) {
        break;
      }
      /* A RESOURCE_LIMIT stop is resumable: the per-advance event budget ends
       * an advance mid-slice and a full ring stops it until a drain. A stop
       * that moved nothing (same tick, nothing drained) is a stall only when
       * the machine digest has not moved either; a second such stop with an
       * equal digest is permanent. */
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
  (void)fprintf(stderr, "t64c: the bound was reached before completion\n");
  return false;
}

bool t64c_finish_run(t64c_fixture *fixture, t64c_run *run) {
  t64_status status = t64_machine_save_export(
      fixture->machine, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_RAW, run->save,
      sizeof run->save, &run->save_length);

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

/* ---- the composite case --------------------------------------------------- */

static void t64c_independent(t64c_tally *tally, bool equal, const char *what) {
  tally->checks++;
  if (!equal) {
    tally->failures++;
  }
  (void)printf("independent literal: %s: %s\n", what,
               equal ? "equal" : "DIFFERENT");
}

static void t64c_pinned(t64c_tally *tally, bool equal, const char *what) {
  tally->checks++;
  if (!equal) {
    tally->failures++;
  }
  (void)printf("%s: %s: %s\n", T64C_PIN_LABEL, what,
               equal ? "equal" : "DIFFERENT");
}

/* The 8 hex digits after "<tag> " on the line of the transcript that starts
 * with the tag; false when there is no such line. */
static bool t64c_poll_word(const uint8_t *text, size_t length, const char *tag,
                           char word[9]) {
  const size_t tag_length = strlen(tag);
  size_t i;

  for (i = 0u; i + tag_length + 1u + 8u <= length; i++) {
    if ((i == 0u || text[i - 1u] == '\n') &&
        memcmp(text + i, tag, tag_length) == 0 && text[i + tag_length] == ' ') {
      memcpy(word, text + i + tag_length + 1u, 8u);
      word[8] = '\0';
      return true;
    }
  }
  return false;
}

static bool t64c_write_file(const char *dir, const char *name, size_t index,
                            const uint8_t *bytes, size_t length) {
  char path[512];
  FILE *file;
  int written;

  if (index == (size_t)-1) {
    written = snprintf(path, sizeof path, "%s/%s", dir, name);
  } else {
    written = snprintf(path, sizeof path, "%s/%s-%u", dir, name, (unsigned)index);
  }
  if (written <= 0 || (size_t)written >= sizeof path) {
    return false;
  }
  file = fopen(path, "wb");
  if (file == NULL) {
    return false;
  }
  if (fwrite(bytes, 1u, length, file) != length) {
    (void)fclose(file);
    return false;
  }
  return fclose(file) == 0;
}

/* The machine-readable lines of the three-way comparison and the output files
 * the in-tree runner's files are compared with byte for byte. */
static bool t64c_print_run(const t64c_args *args, const t64c_run *run) {
  char hex[T64C_SHA256_HEX_SIZE];
  size_t i;

  t64c_sha256_hex(run->digest, hex);
  (void)printf("print final_digest %s\n", hex);
  (void)printf("print completion_tick %llu\n", (unsigned long long)run->tick);
  (void)printf("print stop_reason %s\n",
               run->stop_reason == T64_STOP_REASON_GUEST_COMPLETION
                   ? "GUEST_COMPLETION"
                   : "OTHER");
  (void)printf("print video_packet_count %u\n", (unsigned)run->video_count);
  (void)printf("print audio_packet_count %u\n", (unsigned)run->audio_count);
  (void)printf("print save_bytes %u\n", (unsigned)run->save_length);
  (void)printf("print transcript_bytes %u\n", (unsigned)run->text_length);
  if (args->out_dir != NULL) {
    bool ok = true;

    for (i = 0u; i < run->video_count; i++) {
      ok = ok && t64c_write_file(args->out_dir, "video", i, run->video[i],
                                 T64C_RUN_VIDEO_BYTES);
    }
    for (i = 0u; i < run->audio_count; i++) {
      ok = ok && t64c_write_file(args->out_dir, "audio", i, run->audio[i],
                                 T64C_RUN_AUDIO_BYTES);
    }
    ok = ok && t64c_write_file(args->out_dir, "save", (size_t)-1, run->save,
                               run->save_length);
    ok = ok && t64c_write_file(args->out_dir, "transcript", (size_t)-1,
                               run->text, run->text_length);
    return ok;
  }
  return true;
}

int t64c_case_composite(const t64c_args *args) {
  static const uint8_t video_literal[T64C_LIT_VIDEO_PACKET_BYTES] =
      T64C_LIT_VIDEO_PACKET_INIT;
  static const uint8_t audio_literal[T64C_LIT_AUDIO_PACKET_COUNT]
                                    [T64C_LIT_AUDIO_PACKET_BYTES] = {
      T64C_LIT_AUDIO_PACKET0_INIT, T64C_LIT_AUDIO_PACKET1_INIT};
  static const uint8_t save_range[T64C_LIT_SAVE_RANGE0_BYTES] =
      T64C_LIT_SAVE_RANGE0_INIT;
  static const char *const polled_literal[3] = {
      T64C_LIT_POLLED0_HEX, T64C_LIT_POLLED1_HEX, T64C_LIT_POLLED2_HEX};
  static const char *const poll_tags[3] = {"I1", "I2", "I3"};
  t64c_tally tally = {0ul, 0ul};
  t64c_fixture *fixture = t64c_fixture_new(args->rom_path);
  t64c_run *run = (t64c_run *)calloc(1u, sizeof *run);
  char hex[T64C_SHA256_HEX_SIZE];
  char word[9];
  size_t i;
  bool video_equal = true;
  bool audio_equal = true;
  bool save_equal;

  if (fixture == NULL || run == NULL) {
    free(run);
    t64c_fixture_free(fixture);
    return 2;
  }
  T64C_CHECK_U32(&tally, t64c_sha256_selfcheck(), 0u);
  T64C_CHECK(&tally, strcmp(fixture->profile->profile_id, T64C_PIN_PROFILE_ID) == 0);
  T64C_CHECK_U32(&tally, t64c_fixture_admit(fixture), T64_STATUS_OK);
  T64C_CHECK(&tally, t64c_queue_literal_inputs(fixture));
  T64C_CHECK(&tally, t64c_run_to_completion(fixture, args->slice, run));
  T64C_CHECK(&tally, t64c_finish_run(fixture, run));

  /* Independent literals. */
  t64c_independent(&tally, run->video_count >= 1u, "at least one video packet");
  for (i = 0u; i < run->video_count; i++) {
    video_equal = video_equal && run->video_width[i] == T64C_LIT_VIDEO_WIDTH &&
                  run->video_height[i] == T64C_LIT_VIDEO_HEIGHT &&
                  run->video_packet_bytes[i] == T64C_LIT_VIDEO_PACKET_BYTES &&
                  memcmp(run->video[i], video_literal, sizeof video_literal) == 0;
  }
  t64c_independent(&tally, video_equal,
                   "every video packet is the 4x3 RGBA8 literal of 48 bytes");
  t64c_independent(&tally, run->audio_count == T64C_LIT_AUDIO_PACKET_COUNT,
                   "two audio packets");
  for (i = 0u; i < run->audio_count && i < T64C_LIT_AUDIO_PACKET_COUNT; i++) {
    audio_equal = audio_equal &&
                  run->audio_packet_bytes[i] == T64C_LIT_AUDIO_PACKET_BYTES &&
                  memcmp(run->audio[i], audio_literal[i], T64C_LIT_AUDIO_PACKET_BYTES) == 0;
  }
  t64c_independent(&tally, audio_equal && run->audio_count == T64C_LIT_AUDIO_PACKET_COUNT,
                   "both S16BE audio packets byte for byte");
  t64c_independent(&tally, run->text_length == T64C_LIT_TRANSCRIPT_BYTES,
                   "ISViewer transcript is 188 bytes");
  t64c_independent(&tally,
                   run->text_length == T64C_LIT_TRANSCRIPT_BYTES &&
                       memcmp(run->text, T64C_LIT_TRANSCRIPT_TEXT, T64C_LIT_TRANSCRIPT_BYTES) == 0,
                   "ISViewer transcript bytes");
  t64c_sha256_hex_of(run->text, run->text_length, hex);
  t64c_independent(&tally, strcmp(hex, T64C_LIT_TRANSCRIPT_SHA256_HEX) == 0,
                   "transcript SHA-256 (consumer's own implementation)");
  for (i = 0u; i < 3u; i++) {
    char what[48];

    (void)snprintf(what, sizeof what, "polled word %s", poll_tags[i]);
    t64c_independent(&tally,
                     t64c_poll_word(run->text, run->text_length, poll_tags[i], word) &&
                         strcmp(word, polled_literal[i]) == 0,
                     what);
  }
  save_equal = run->save_length == T64C_LIT_SAVE_BYTES;
  for (i = 0u; save_equal && i < T64C_LIT_SAVE_BYTES; i++) {
    const bool in_range = i >= T64C_LIT_SAVE_RANGE0_OFFSET &&
                          i < T64C_LIT_SAVE_RANGE0_OFFSET + T64C_LIT_SAVE_RANGE0_BYTES;
    const uint8_t expected = in_range ? save_range[i - T64C_LIT_SAVE_RANGE0_OFFSET] : 0u;

    save_equal = run->save[i] == expected;
  }
  t64c_independent(&tally, save_equal, "32768-byte SRAM save (blank plus the 16-byte record)");
  t64c_sha256_hex_of(run->save, run->save_length, hex);
  t64c_independent(&tally, strcmp(hex, T64C_LIT_SAVE_SHA256_HEX) == 0,
                   "save SHA-256 (consumer's own implementation)");

  /* Runner-recorded pins: agreement, not independent expectations. */
  (void)printf("%s: source: %s\n", T64C_PIN_LABEL, T64C_PIN_SOURCE);
  {
    char what[96];

    (void)snprintf(what, sizeof what, "completion tick %llu",
                   (unsigned long long)run->tick);
    t64c_pinned(&tally, run->tick == T64C_PIN_COMPLETION_TICK, what);
  }
  t64c_pinned(&tally, run->stop_reason == T64C_PIN_STOP_REASON,
              "stop reason " T64C_PIN_STOP_REASON_NAME);
  t64c_sha256_hex(run->digest, hex);
  {
    char what[128];

    (void)snprintf(what, sizeof what, "final state digest %s", hex);
    t64c_pinned(&tally, strcmp(hex, T64C_PIN_FINAL_DIGEST_HEX) == 0, what);
  }
  t64c_pinned(&tally, run->video_count == T64C_PIN_VIDEO_PACKET_COUNT,
              "video packet count");

  if (args->print || args->out_dir != NULL) {
    T64C_CHECK(&tally, t64c_print_run(args, run));
  }
  t64_machine_destroy(fixture->machine);
  fixture->machine = NULL;
  T64C_CHECK(&tally, fixture->ledger.outstanding == 0u);
  T64C_CHECK(&tally, fixture->ledger.bad_releases == 0u);
  T64C_CHECK(&tally, fixture->ledger.refusals == 0u);
  t64c_fixture_free(fixture);
  free(run);
  return t64c_report("composite", &tally);
}
