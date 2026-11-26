/*
 * Unit cases of the host pump (Phase 12, plan 12-07, API-05, decisions DR-10 and
 * DR-11), run against the real core through the installed package with a fake
 * clock: the pump reads no host time, so a case decides what the clock says.
 *
 *   t64h_tests --case NAME --rom PATH [--expect HEX] [--event TICK:BUTTONS:X:Y]...
 *   t64h_tests --list-cases
 *
 *   drain-order   the title's short script through the pump with a fake clock: every
 *                 drain pass delivers video, then audio, then ISViewer text, one pass
 *                 holds all three kinds, the counts equal the oracle's and the first
 *                 video packet equals the expected frame digest (--expect)
 *   clock         the tick conversion (0, 15, 16 ns and the maximum), the carry kept from
 *                 the origin across steps, a backward clock refused without advancing,
 *                 the max_ticks cap and a bound that never falls behind the guest tick
 *   stall         a video sink that refuses everything ends a step with "stall without
 *                 progress" after one retry and loses nothing once it accepts again; a
 *                 sink that refuses once lets the step finish; a step after the guest
 *                 completed is TERMINAL and advances and queues nothing; a video buffer
 *                 smaller than the packet is an API error and drops nothing
 *   title-two-speeds  (plan 12-08) each --script PATH through the pump with the scripted
 *                 source and a fake clock at each --slice-hz speed (the clock advances
 *                 1e9 / hz ns per step): one JSON line per run with the per-packet video
 *                 digests, the audio packet and stream digests, the transcript digest, the
 *                 exported save digest, the completion tick, the stop reason and the final
 *                 state digest. The case holds no expected value: scripts/check-host-title.py
 *                 compares the lines with fixtures/title/title.expected.json and the pins
 *
 *   input-map     (plan 12-08) the logical input mapping table of t64h_input: axis scale,
 *                 dead zone and vertical inversion over every int16 value, each mapped
 *                 button bit, unmapped controls, the left trigger threshold, a removed pad
 *   frames        (plan 12-08) t64h_frame_copy honors stride_bytes and the destination pitch,
 *                 refuses a packet over 320 x 240, a short source or destination (leaving
 *                 the destination untouched) and a bad geometry or encoding; the S16BE swap
 *   script        (plan 12-08) the strict t64.input reader (a seed per rule, the size and
 *                 event limits, the committed scripts against the events python3 read from
 *                 the same files) and the scripted source against a model of a small queue
 *                 (lazy queueing, QUEUE_FULL deferral) and against the title (nothing is
 *                 queued after the guest completed)
 *
 * A --event argument that follows a --script argument belongs to that script (the events
 * the case expects the reader to find); before any --script it belongs to the case.
 *
 * The title ROM is the fixture of every case that runs a guest (build target
 * t64-phase12-title-fixture). The expected frame digest and the events come from
 * fixtures/title/ through scripts/check-host.sh, never from this file or the pump.
 */
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tendies64/t64_machine.h>

#include "t64h_frames.h"
#include "t64h_input.h"
#include "t64h_pump.h"
#include "t64h_script.h"
#include "t64h_sha256.h"

#define TITLE_PROFILE_ID "ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k"
#define MAX_ROM_BYTES (64u * 1024u * 1024u)
#define MAX_EVENTS 16u

/* The title's short scenario as the oracle states it (fixtures/title/title.expected.json,
 * scenario short): three video packets, two audio packets and a 20-byte transcript. */
#define SHORT_VIDEO_PACKETS 3u
#define SHORT_AUDIO_PACKETS 2u
#define SHORT_TEXT_BYTES 20u
#define TITLE_FRAME_BYTES 49152u
#define TITLE_FRAME_WIDTH 128u
#define TITLE_FRAME_HEIGHT 96u

/* ---- checks ---------------------------------------------------------------- */

typedef struct {
  unsigned long checks;
  unsigned long failures;
} tally;

#define CHECK(t, condition)                                                    \
  do {                                                                         \
    (t)->checks++;                                                             \
    if (!(condition)) {                                                        \
      (t)->failures++;                                                         \
      (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,            \
                    #condition);                                               \
    }                                                                          \
  } while (0)

#define CHECK_U64(t, actual, expected)                                         \
  do {                                                                         \
    const unsigned long long actual_value = (unsigned long long)(actual);      \
    const unsigned long long expected_value = (unsigned long long)(expected);  \
    (t)->checks++;                                                             \
    if (actual_value != expected_value) {                                      \
      (t)->failures++;                                                         \
      (void)fprintf(stderr, "FAIL %s:%d: %s is %llu, expected %llu\n",         \
                    __FILE__, __LINE__, #actual, actual_value, expected_value);\
    }                                                                          \
  } while (0)

static int report(const char *name, const tally *t) {
  (void)printf("%s: checks=%lu failures=%lu\n", name, t->checks, t->failures);
  return (t->checks == 0ul || t->failures != 0ul) ? 1 : 0;
}

/* ---- arguments --------------------------------------------------------------- */

typedef struct {
  uint64_t tick;
  uint16_t buttons;
  int32_t axis_x;
  int32_t axis_y;
} input_event;

#define MAX_SCRIPTS 4u
#define MAX_SPEEDS 4u

typedef struct {
  const char *path;
  input_event events[MAX_EVENTS];
  size_t event_count;
} script_arg;

typedef struct {
  const char *case_name;
  const char *rom_path;
  const char *expect_hex;
  input_event events[MAX_EVENTS];
  size_t event_count;
  script_arg scripts[MAX_SCRIPTS];
  size_t script_count;
  unsigned slice_hz[MAX_SPEEDS];
  size_t slice_count;
} args;

/* TICK:BUTTONS:X:Y, decimal, the whole string. */
static bool parse_event(const char *text, input_event *event) {
  char *end = NULL;
  unsigned long long tick;
  unsigned long buttons;
  long axis_x;
  long axis_y;

  tick = strtoull(text, &end, 10);
  if (end == text || *end != ':') {
    return false;
  }
  text = end + 1;
  buttons = strtoul(text, &end, 10);
  if (end == text || *end != ':' || buttons > 65535ul) {
    return false;
  }
  text = end + 1;
  axis_x = strtol(text, &end, 10);
  if (end == text || *end != ':' || axis_x < -128 || axis_x > 127) {
    return false;
  }
  text = end + 1;
  axis_y = strtol(text, &end, 10);
  if (end == text || *end != '\0' || axis_y < -128 || axis_y > 127) {
    return false;
  }
  event->tick = (uint64_t)tick;
  event->buttons = (uint16_t)buttons;
  event->axis_x = (int32_t)axis_x;
  event->axis_y = (int32_t)axis_y;
  return true;
}

/* HZ[,HZ]...: decimal speeds in 1..1000000 (the fake clock advances 1e9 / hz ns per step). */
static bool parse_speeds(const char *text, args *out) {
  out->slice_count = 0u;
  for (;;) {
    char *end = NULL;
    unsigned long hz = strtoul(text, &end, 10);

    if (end == text || hz < 1ul || hz > 1000000ul || out->slice_count >= MAX_SPEEDS) {
      return false;
    }
    out->slice_hz[out->slice_count++] = (unsigned)hz;
    if (*end == '\0') {
      return true;
    }
    if (*end != ',') {
      return false;
    }
    text = end + 1;
  }
}

static bool parse_args(int argc, char **argv, args *out) {
  int i;

  (void)memset(out, 0, sizeof *out);
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--case") == 0 && i + 1 < argc) {
      out->case_name = argv[++i];
    } else if (strcmp(argv[i], "--rom") == 0 && i + 1 < argc) {
      out->rom_path = argv[++i];
    } else if (strcmp(argv[i], "--expect") == 0 && i + 1 < argc) {
      out->expect_hex = argv[++i];
    } else if (strcmp(argv[i], "--script") == 0 && i + 1 < argc) {
      if (out->script_count >= MAX_SCRIPTS) {
        (void)fprintf(stderr, "t64h_tests: too many --script arguments\n");
        return false;
      }
      out->scripts[out->script_count++].path = argv[++i];
    } else if (strcmp(argv[i], "--slice-hz") == 0 && i + 1 < argc) {
      if (!parse_speeds(argv[++i], out)) {
        (void)fprintf(stderr, "t64h_tests: bad --slice-hz list\n");
        return false;
      }
    } else if (strcmp(argv[i], "--event") == 0 && i + 1 < argc) {
      script_arg *owner = out->script_count > 0u ? &out->scripts[out->script_count - 1u] : NULL;
      input_event *list = owner != NULL ? owner->events : out->events;
      size_t *count = owner != NULL ? &owner->event_count : &out->event_count;

      if (*count >= MAX_EVENTS || !parse_event(argv[++i], &list[*count])) {
        (void)fprintf(stderr, "t64h_tests: bad or too many --event arguments\n");
        return false;
      }
      (*count)++;
    } else {
      (void)fprintf(stderr, "t64h_tests: unknown argument %s\n", argv[i]);
      return false;
    }
  }
  return true;
}

/* ---- the rig: a counting allocator, a machine and its ROM -------------------- */

typedef struct {
  size_t allocations;
  size_t releases;
  size_t outstanding;
  size_t bad_releases;
} ledger;

/* Aligned by arithmetic over malloc (the public header compiles as C99): the raw
 * pointer is kept in the word before the block the core sees. */
static bool ledger_allocate(void *context, size_t size, size_t alignment,
                            t64_host_storage *storage_out) {
  ledger *counts = (ledger *)context;
  size_t align = alignment;
  uint8_t *raw;
  uintptr_t aligned;

  if (counts == NULL || storage_out == NULL || size == 0u || alignment == 0u ||
      (alignment & (alignment - 1u)) != 0u) {
    return false;
  }
  if (align < sizeof(void *)) {
    align = sizeof(void *);
  }
  raw = (uint8_t *)malloc(size + align + sizeof(void *));
  if (raw == NULL) {
    return false;
  }
  aligned = ((uintptr_t)raw + sizeof(void *) + align - 1u) / align * align;
  ((void **)aligned)[-1] = raw;
  counts->allocations++;
  counts->outstanding++;
  storage_out->data = (void *)aligned;
  storage_out->capacity = size;
  storage_out->alignment = align;
  return true;
}

static void ledger_release(void *context, t64_host_storage storage) {
  ledger *counts = (ledger *)context;

  if (counts == NULL || storage.data == NULL || counts->outstanding == 0u) {
    if (counts != NULL) {
      counts->bad_releases++;
    }
    return;
  }
  counts->releases++;
  counts->outstanding--;
  free(((void **)storage.data)[-1]);
}

typedef struct {
  ledger counts;
  t64_host_allocator allocator;
  const t64_profile_descriptor *profile;
  t64_machine *machine;
  uint8_t *rom;
  size_t rom_length;
} rig;

static uint8_t *read_file(const char *path, size_t *length_out) {
  FILE *file = fopen(path, "rb");
  long size;
  uint8_t *bytes;

  if (file == NULL) {
    (void)fprintf(stderr, "t64h_tests: cannot open the ROM file\n");
    return NULL;
  }
  if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) <= 0 ||
      (unsigned long)size > MAX_ROM_BYTES || fseek(file, 0, SEEK_SET) != 0) {
    (void)fclose(file);
    (void)fprintf(stderr, "t64h_tests: the ROM file has no usable size\n");
    return NULL;
  }
  bytes = (uint8_t *)malloc((size_t)size);
  if (bytes == NULL || fread(bytes, 1u, (size_t)size, file) != (size_t)size) {
    free(bytes);
    (void)fclose(file);
    (void)fprintf(stderr, "t64h_tests: cannot read the ROM file\n");
    return NULL;
  }
  (void)fclose(file);
  *length_out = (size_t)size;
  return bytes;
}

/* Create a machine of the title profile, admit the ROM and, on request, queue the
 * events of the arguments before the machine runs. */
static bool rig_open(rig *r, const args *a, bool with_events) {
  t64_status status;
  size_t i;

  (void)memset(r, 0, sizeof *r);
  if (a->rom_path == NULL) {
    (void)fprintf(stderr, "t64h_tests: this case needs --rom PATH\n");
    return false;
  }
  r->rom = read_file(a->rom_path, &r->rom_length);
  if (r->rom == NULL) {
    return false;
  }
  status = t64_profile_lookup(TITLE_PROFILE_ID, &r->profile);
  if (status != T64_STATUS_OK || r->profile == NULL) {
    (void)fprintf(stderr, "t64h_tests: profile lookup failed (%u)\n", (unsigned)status);
    return false;
  }
  r->allocator = (t64_host_allocator)T64_HOST_ALLOCATOR_INIT(ledger_allocate, ledger_release,
                                                             &r->counts);
  status = t64_machine_create(&(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(
                                  &r->allocator, r->profile),
                              &r->machine);
  if (status != T64_STATUS_OK || r->machine == NULL) {
    (void)fprintf(stderr, "t64h_tests: create failed (%u)\n", (unsigned)status);
    r->machine = NULL;
    return false;
  }
  status = t64_machine_admit_rom(r->machine, r->rom, r->rom_length);
  if (status != T64_STATUS_OK) {
    (void)fprintf(stderr, "t64h_tests: admission failed (%u)\n", (unsigned)status);
    return false;
  }
  for (i = 0u; with_events && i < a->event_count; i++) {
    status = t64_machine_queue_input(r->machine, a->events[i].tick, 0u, a->events[i].buttons,
                                     a->events[i].axis_x, a->events[i].axis_y);
    if (status != T64_STATUS_OK) {
      (void)fprintf(stderr, "t64h_tests: queue_input refused event %u (%u)\n", (unsigned)i,
                    (unsigned)status);
      return false;
    }
  }
  return true;
}

/* Destroy the machine and check the host allocator is balanced. */
static void rig_close(rig *r, tally *t) {
  if (r->machine != NULL) {
    t64_machine_destroy(r->machine);
    r->machine = NULL;
    CHECK_U64(t, r->counts.outstanding, 0u);
    CHECK_U64(t, r->counts.bad_releases, 0u);
  }
  free(r->rom);
  r->rom = NULL;
}

/* ---- the fake clock and the recording sinks ---------------------------------- */

typedef struct {
  uint64_t now;
  unsigned reads;
} fake_clock;

static uint64_t fake_clock_read(void *context) {
  fake_clock *clock = (fake_clock *)context;

  clock->reads++;
  return clock->now;
}

enum { KIND_VIDEO = 0, KIND_AUDIO = 1, KIND_TEXT = 2 };
#define LOG_MAX 128u
#define REFUSE_ALWAYS UINT_MAX

typedef struct {
  unsigned kind;
  uint64_t pass;
  uint64_t sequence;  /* of a video or audio packet, 0 for text */
  size_t length;
  char sha_prefix[17];
} log_entry;

typedef struct {
  const t64h_pump *pump;  /* its drain_passes counter names the pass of a call */
  log_entry log[LOG_MAX];
  size_t count;
  size_t overflow;
  unsigned packets[3];
  size_t text_bytes;
  unsigned refuse_video;  /* offers still to refuse, REFUSE_ALWAYS never counts down */
  unsigned refused_video;
  char first_video_hex[T64H_SHA256_HEX_SIZE];
  bool have_first_video;
  bool video_shape_ok;
} recorder;

static void record(recorder *rec, unsigned kind, uint64_t sequence, const uint8_t *bytes,
                   size_t length) {
  char hex[T64H_SHA256_HEX_SIZE];

  t64h_sha256_hex_of(bytes, length, hex);
  if (rec->count < LOG_MAX) {
    log_entry *entry = &rec->log[rec->count++];

    entry->kind = kind;
    entry->pass = rec->pump->counters.drain_passes;
    entry->sequence = sequence;
    entry->length = length;
    (void)memcpy(entry->sha_prefix, hex, 16u);
    entry->sha_prefix[16] = '\0';
  } else {
    rec->overflow++;
  }
  rec->packets[kind]++;
  if (kind == KIND_TEXT) {
    rec->text_bytes += length;
  }
  if (kind == KIND_VIDEO && !rec->have_first_video) {
    (void)memcpy(rec->first_video_hex, hex, sizeof hex);
    rec->have_first_video = true;
  }
}

static int record_video(void *context, const t64_video_packet_info *info,
                        const uint8_t *bytes, size_t length) {
  recorder *rec = (recorder *)context;

  if (rec->refuse_video != 0u) {
    if (rec->refuse_video != REFUSE_ALWAYS) {
      rec->refuse_video--;
    }
    rec->refused_video++;
    return 1;
  }
  if (info->width != TITLE_FRAME_WIDTH || info->height != TITLE_FRAME_HEIGHT ||
      info->stride_bytes != TITLE_FRAME_WIDTH * 4u || length != TITLE_FRAME_BYTES ||
      info->packet_bytes != (uint64_t)length) {
    rec->video_shape_ok = false;
  }
  record(rec, KIND_VIDEO, info->sequence, bytes, length);
  return 0;
}

static int record_audio(void *context, const t64_audio_packet_info *info,
                        const uint8_t *bytes, size_t length) {
  record((recorder *)context, KIND_AUDIO, info->sequence, bytes, length);
  return 0;
}

static int record_text(void *context, const uint8_t *bytes, size_t length) {
  record((recorder *)context, KIND_TEXT, 0u, bytes, length);
  return 0;
}

static const char *kind_name(unsigned kind) {
  return kind == KIND_VIDEO ? "video" : (kind == KIND_AUDIO ? "audio" : "text");
}

/* ---- the bench: a rig, a fake clock, a recorder and a pump -------------------- */

static uint8_t video_buffer[T64H_VIDEO_BUFFER_BYTES];
static uint8_t audio_buffer[T64H_AUDIO_BUFFER_BYTES];
static uint8_t text_buffer[T64H_TEXT_BUFFER_BYTES];

typedef struct {
  rig r;
  recorder rec;
  fake_clock clock;
  t64h_pump pump;
  t64h_config config;
  unsigned input_calls;
} bench;

/* The input hook of the bench only counts its calls: the events of a case are queued
 * before the machine runs, so the count shows whether the pump would have queued. */
static int count_input(void *context, t64_machine *machine, uint64_t guest_tick,
                       uint64_t *bound) {
  (void)machine;
  (void)guest_tick;
  (void)bound;
  ((bench *)context)->input_calls++;
  return 0;
}

static void clock_to_tick(bench *b, uint64_t tick) {
  b->clock.now = tick * T64H_TICK_NS;
}

/* A machine on the title ROM (the events of the arguments queued on request), a pump
 * on a fake clock at 0 ns with sinks that record. False, with a message, on failure. */
static bool bench_open(bench *b, const args *a, bool with_events, size_t video_capacity) {
  (void)memset(b, 0, sizeof *b);
  b->rec.pump = &b->pump;
  b->rec.video_shape_ok = true;
  if (!rig_open(&b->r, a, with_events)) {
    return false;
  }
  b->config.machine = b->r.machine;
  b->config.clock = fake_clock_read;
  b->config.clock_context = &b->clock;
  b->config.sinks.context = &b->rec;
  b->config.sinks.video = record_video;
  b->config.sinks.audio = record_audio;
  b->config.sinks.text = record_text;
  b->config.input = count_input;
  b->config.input_context = b;
  b->config.video_buffer = video_buffer;
  b->config.video_capacity = video_capacity;
  b->config.audio_buffer = audio_buffer;
  b->config.audio_capacity = sizeof audio_buffer;
  b->config.text_buffer = text_buffer;
  b->config.text_capacity = sizeof text_buffer;
  if (t64h_pump_init(&b->pump, &b->config) != T64H_OK) {
    (void)fprintf(stderr, "t64h_tests: pump init refused\n");
    return false;
  }
  return true;
}

static void bench_close(bench *b, tally *t) {
  rig_close(&b->r, t);
}

/* The video packets the recorder holds were delivered in sequence: each one follows
 * the one before by exactly one. */
static bool video_sequences_consecutive(const recorder *rec) {
  bool have_previous = false;
  uint64_t previous = 0u;
  size_t i;

  for (i = 0u; i < rec->count; i++) {
    if (rec->log[i].kind != KIND_VIDEO) {
      continue;
    }
    if (have_previous && rec->log[i].sequence != previous + 1u) {
      return false;
    }
    previous = rec->log[i].sequence;
    have_previous = true;
  }
  return true;
}

/* ---- drain-order -------------------------------------------------------------- */

#define SMALL_SLICE_TICKS UINT64_C(262144)
#define SMALL_PHASE_TICKS UINT64_C(1800000)
#define BIG_JUMP_TICKS UINT64_C(4000000)
#define MAX_STEPS 64u

static int case_drain_order(const args *a) {
  tally t = {0ul, 0ul};
  bench b;
  t64h_step_result result;
  unsigned steps = 0u;
  bool completed = false;
  bool all_ordered = true;
  bool full_pass = false;
  size_t i;

  CHECK(&t, a->expect_hex != NULL && a->event_count > 0u);
  if (!bench_open(&b, a, true, sizeof video_buffer)) {
    bench_close(&b, &t);
    return report("drain-order", &t) | 1;
  }

  /* Small slices first, one drain pass at the end of each, then one jump of host time
   * that carries the guest to its completion: the passes inside that step hold the
   * packets the small slices never met (video, audio and text together). */
  while (!completed && steps < MAX_STEPS) {
    const uint64_t slice = b.pump.tick < SMALL_PHASE_TICKS ? SMALL_SLICE_TICKS : BIG_JUMP_TICKS;
    t64h_status status;

    b.clock.now += slice * T64H_TICK_NS;
    status = t64h_pump_step(&b.pump, UINT64_MAX, &result);
    steps++;
    if (status != T64H_OK) {
      (void)fprintf(stderr, "t64h_tests: step %u answered %s (%s)\n", steps,
                    t64h_status_name(status), result.message != NULL ? result.message : "");
      t.checks++;
      t.failures++;
      break;
    }
    completed = result.stop_reason == T64_STOP_REASON_GUEST_COMPLETION;
  }
  CHECK(&t, completed);

  /* The log: one line per delivery, then the order check per pass. */
  for (i = 0u; i < b.rec.count; i++) {
    (void)printf("drain-order: pass %llu: %s %llu bytes sha256=%s...\n",
                 (unsigned long long)b.rec.log[i].pass, kind_name(b.rec.log[i].kind),
                 (unsigned long long)b.rec.log[i].length, b.rec.log[i].sha_prefix);
  }
  for (i = 0u; i + 1u < b.rec.count; i++) {
    if (b.rec.log[i + 1u].pass < b.rec.log[i].pass ||
        (b.rec.log[i + 1u].pass == b.rec.log[i].pass &&
         b.rec.log[i + 1u].kind < b.rec.log[i].kind)) {
      all_ordered = false;
    }
  }
  for (i = 0u; i < b.rec.count; i++) {
    unsigned seen = 0u;
    size_t j;

    for (j = 0u; j < b.rec.count; j++) {
      if (b.rec.log[j].pass == b.rec.log[i].pass) {
        seen |= 1u << b.rec.log[j].kind;
      }
    }
    if (seen == 7u) {
      full_pass = true;
    }
  }
  CHECK(&t, b.rec.overflow == 0u);
  CHECK(&t, all_ordered);
  CHECK(&t, full_pass);
  CHECK_U64(&t, b.rec.packets[KIND_VIDEO], SHORT_VIDEO_PACKETS);
  CHECK_U64(&t, b.rec.packets[KIND_AUDIO], SHORT_AUDIO_PACKETS);
  CHECK_U64(&t, b.rec.text_bytes, SHORT_TEXT_BYTES);
  CHECK_U64(&t, b.pump.counters.video_packets, SHORT_VIDEO_PACKETS);
  CHECK_U64(&t, b.pump.counters.audio_packets, SHORT_AUDIO_PACKETS);
  CHECK_U64(&t, b.pump.counters.text_bytes, SHORT_TEXT_BYTES);
  CHECK(&t, b.rec.video_shape_ok);
  CHECK(&t, video_sequences_consecutive(&b.rec));
  CHECK(&t, b.rec.have_first_video && a->expect_hex != NULL &&
                strcmp(b.rec.first_video_hex, a->expect_hex) == 0);
  (void)printf("drain-order: %u steps, %llu advances, %llu drain passes, first video packet "
               "sha256=%s (expected %s), order %s, one pass with video, audio and text: %s\n",
               steps, (unsigned long long)b.pump.counters.advances,
               (unsigned long long)b.pump.counters.drain_passes,
               b.rec.have_first_video ? b.rec.first_video_hex : "none",
               a->expect_hex != NULL ? a->expect_hex : "none", all_ordered ? "ok" : "WRONG",
               full_pass ? "yes" : "NO");
  bench_close(&b, &t);
  return report("drain-order", &t);
}

/* ---- clock -------------------------------------------------------------------- */

static bool digest_of(const bench *b, uint8_t digest[T64_DIGEST_SIZE]) {
  return t64_machine_digest(b->r.machine, digest) == T64_STATUS_OK;
}

static int case_clock(const args *a) {
  tally t = {0ul, 0ul};
  bench b;
  t64h_step_result result;
  uint64_t lcg = UINT64_C(0x9e3779b97f4a7c15);
  uint64_t before_tick;
  uint64_t before_advances;
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  uint64_t saved_now;
  unsigned i;

  /* The conversion is a division by 16, with no rounding up and no overflow. */
  CHECK_U64(&t, t64h_ticks_from_ns(0u), 0u);
  CHECK_U64(&t, t64h_ticks_from_ns(15u), 0u);
  CHECK_U64(&t, t64h_ticks_from_ns(16u), 1u);
  CHECK_U64(&t, t64h_ticks_from_ns(31u), 1u);
  CHECK_U64(&t, t64h_ticks_from_ns(UINT64_MAX), UINT64_MAX / 16u);
  if (!bench_open(&b, a, false, sizeof video_buffer)) {
    bench_close(&b, &t);
    return report("clock", &t) | 1;
  }

  /* The carry is kept from the origin, not per step: two steps of 8 ns are one tick. */
  b.clock.now = 8u;
  CHECK(&t, t64h_pump_step(&b.pump, UINT64_MAX, &result) == T64H_OK);
  CHECK_U64(&t, result.tick, 0u);
  b.clock.now = 16u;
  CHECK(&t, t64h_pump_step(&b.pump, UINT64_MAX, &result) == T64H_OK);
  CHECK_U64(&t, result.tick, 1u);
  /* Irregular steps: after every step the guest tick is the origin-relative time / 16. */
  for (i = 0u; i < 40u; i++) {
    lcg = lcg * UINT64_C(6364136223846793005) + UINT64_C(1442695040888963407);
    b.clock.now += (lcg >> 33) % 41u;
    CHECK(&t, t64h_pump_step(&b.pump, UINT64_MAX, &result) == T64H_OK);
    CHECK_U64(&t, result.tick, b.clock.now / T64H_TICK_NS);
  }
  CHECK_U64(&t, b.pump.counters.steps, 42u);

  /* A clock behind its last reading: refused and counted, nothing advances, and the
   * reading is not taken (the next good reading counts from the last accepted one). */
  saved_now = b.clock.now;
  before_tick = b.pump.tick;
  before_advances = b.pump.counters.advances;
  CHECK(&t, digest_of(&b, digest_before));
  b.clock.now = saved_now - 1u;
  CHECK(&t, t64h_pump_step(&b.pump, UINT64_MAX, &result) == T64H_CLOCK_BACKWARD);
  CHECK_U64(&t, b.pump.counters.backward_clocks, 1u);
  CHECK_U64(&t, result.tick, before_tick);
  CHECK_U64(&t, b.pump.counters.advances, before_advances);
  CHECK(&t, digest_of(&b, digest_after) &&
                memcmp(digest_before, digest_after, sizeof digest_before) == 0);
  b.clock.now = saved_now;  /* an equal reading is not backward */
  CHECK(&t, t64h_pump_step(&b.pump, UINT64_MAX, &result) == T64H_OK);
  CHECK_U64(&t, result.tick, before_tick);
  CHECK_U64(&t, b.pump.counters.backward_clocks, 1u);
  b.clock.now = saved_now + 4u * T64H_TICK_NS;
  CHECK(&t, t64h_pump_step(&b.pump, UINT64_MAX, &result) == T64H_OK);
  CHECK_U64(&t, result.tick, saved_now / T64H_TICK_NS + 4u);

  /* max_ticks caps one step's bound: the guest catches up a step at a time, and a cap of
   * 0 moves nothing (the bound never falls behind the guest tick: no TIME_REVERSED). */
  before_tick = b.pump.tick;
  clock_to_tick(&b, before_tick + 1000000u);
  CHECK(&t, t64h_pump_step(&b.pump, 1000u, &result) == T64H_OK);
  CHECK_U64(&t, result.tick, before_tick + 1000u);
  CHECK(&t, t64h_pump_step(&b.pump, 1000u, &result) == T64H_OK);
  CHECK_U64(&t, result.tick, before_tick + 2000u);
  CHECK(&t, t64h_pump_step(&b.pump, 0u, &result) == T64H_OK);
  CHECK_U64(&t, result.tick, before_tick + 2000u);
  CHECK_U64(&t, result.api_status, T64_STATUS_OK);
  (void)printf("clock: %llu steps, guest tick %llu, %llu backward clock(s) refused, "
               "%llu advances\n",
               (unsigned long long)b.pump.counters.steps, (unsigned long long)b.pump.tick,
               (unsigned long long)b.pump.counters.backward_clocks,
               (unsigned long long)b.pump.counters.advances);
  bench_close(&b, &t);
  return report("clock", &t);
}

/* ---- stall -------------------------------------------------------------------- */

#define STALL_JUMP_TICKS UINT64_C(8000000)

/* A sink that refuses every video packet: the step ends in "stall without progress"
 * after one retry, the refused packet is kept, and once the sink accepts again the pump
 * delivers it first, in sequence, with the machine moving on. */
static void stall_refused_everything(const args *a, tally *t) {
  bench b;
  t64h_step_result result;
  t64h_status status;
  uint64_t stall_tick;

  if (!bench_open(&b, a, false, sizeof video_buffer)) {
    t->checks++;
    t->failures++;
    bench_close(&b, t);
    return;
  }
  b.rec.refuse_video = REFUSE_ALWAYS;
  clock_to_tick(&b, STALL_JUMP_TICKS);
  status = t64h_pump_step(&b.pump, UINT64_MAX, &result);
  stall_tick = result.tick;
  (void)printf("stall: refused-everything: %s at guest tick %llu (%s), %llu backpressure "
               "stop(s), %llu retry, %u refusal(s)\n",
               t64h_status_name(status), (unsigned long long)result.tick,
               result.message != NULL ? result.message : "no message",
               (unsigned long long)b.pump.counters.backpressure_stops,
               (unsigned long long)b.pump.counters.stall_retries, b.rec.refused_video);
  CHECK(t, status == T64H_STALL);
  CHECK(t, result.message != NULL && strcmp(result.message, "stall without progress") == 0);
  CHECK_U64(t, b.pump.counters.stall_retries, 1u);
  CHECK_U64(t, b.pump.counters.backpressure_stops, 2u);
  CHECK(t, b.rec.refused_video >= 2u);
  CHECK_U64(t, b.rec.packets[KIND_VIDEO], 0u);
  CHECK(t, stall_tick > 0u && stall_tick < STALL_JUMP_TICKS);
  CHECK_U64(t, result.stop_reason, T64_STOP_REASON_RESOURCE_LIMIT);

  /* The host's sink accepts again: the next step drains the held packet and the rings in
   * order and the guest moves on (a stall is the end of a step, not of the pump). */
  b.rec.refuse_video = 0u;
  status = t64h_pump_step(&b.pump, 100000u, &result);
  CHECK(t, status == T64H_OK);
  CHECK(t, result.tick > stall_tick);
  CHECK(t, b.rec.packets[KIND_VIDEO] >= 3u);
  CHECK(t, video_sequences_consecutive(&b.rec));
  CHECK(t, b.rec.video_shape_ok);
  (void)printf("stall: refused-everything: after the sink accepts: %s at guest tick %llu, %u video "
               "packet(s) delivered in sequence\n",
               t64h_status_name(status), (unsigned long long)result.tick,
               b.rec.packets[KIND_VIDEO]);
  bench_close(&b, t);
}

/* A sink that refuses once lets the step finish; after the guest completes the pump is
 * terminal: the next step advances nothing, queues nothing and delivers nothing. */
static void stall_refused_once_and_terminal(const args *a, tally *t) {
  bench b;
  t64h_step_result result;
  t64h_status status;
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  uint64_t completion_tick;
  uint64_t advances;
  unsigned input_calls;
  unsigned i;

  (void)memset(&b, 0, sizeof b);
  if (a->event_count == 0u || !bench_open(&b, a, true, sizeof video_buffer)) {
    t->checks++;
    t->failures++;
    bench_close(&b, t);
    return;
  }
  b.rec.refuse_video = 1u;
  clock_to_tick(&b, BIG_JUMP_TICKS);
  status = t64h_pump_step(&b.pump, UINT64_MAX, &result);
  (void)printf("stall: refused-once: %s at guest tick %llu, stop reason %u, %u refusal(s), "
               "%llu video, %llu audio, %llu text byte(s)\n",
               t64h_status_name(status), (unsigned long long)result.tick,
               (unsigned)result.stop_reason, b.rec.refused_video,
               (unsigned long long)b.pump.counters.video_packets,
               (unsigned long long)b.pump.counters.audio_packets,
               (unsigned long long)b.pump.counters.text_bytes);
  CHECK(t, status == T64H_OK);
  CHECK_U64(t, result.stop_reason, T64_STOP_REASON_GUEST_COMPLETION);
  CHECK_U64(t, b.rec.refused_video, 1u);
  CHECK_U64(t, b.pump.counters.video_packets, SHORT_VIDEO_PACKETS);
  CHECK_U64(t, b.pump.counters.audio_packets, SHORT_AUDIO_PACKETS);
  CHECK_U64(t, b.pump.counters.text_bytes, SHORT_TEXT_BYTES);
  CHECK(t, video_sequences_consecutive(&b.rec));
  CHECK_U64(t, b.pump.counters.sink_refusals, 1u);

  /* Terminal: more host time, no further advance, no input hook call, no delivery. */
  completion_tick = result.tick;
  advances = b.pump.counters.advances;
  input_calls = b.input_calls;
  CHECK(t, digest_of(&b, digest_before));
  for (i = 0u; i < 2u; i++) {
    b.clock.now += 1000u * T64H_TICK_NS;
    status = t64h_pump_step(&b.pump, UINT64_MAX, &result);
    CHECK(t, status == T64H_TERMINAL);
    CHECK_U64(t, result.tick, completion_tick);
    CHECK_U64(t, result.stop_reason, T64_STOP_REASON_GUEST_COMPLETION);
    CHECK(t, result.message != NULL);
  }
  CHECK_U64(t, b.pump.counters.advances, advances);
  CHECK_U64(t, b.input_calls, input_calls);
  CHECK_U64(t, b.pump.counters.video_packets, SHORT_VIDEO_PACKETS);
  CHECK(t, digest_of(&b, digest_after) &&
               memcmp(digest_before, digest_after, sizeof digest_before) == 0);
  (void)printf("stall: terminal: two later steps answered %s at guest tick %llu, advances %llu "
               "unchanged, input hook calls %u unchanged\n",
               t64h_status_name(status), (unsigned long long)result.tick,
               (unsigned long long)b.pump.counters.advances, b.input_calls);
  bench_close(&b, t);
}

/* A video buffer smaller than the packet is an API error naming BUFFER_TOO_SMALL: the
 * packet stays in the ring (nothing was copied) and nothing reached the sink. */
static void stall_small_buffer(const args *a, tally *t) {
  bench b;
  t64h_step_result result;
  t64_output_status ring = T64_OUTPUT_STATUS_INIT;
  t64h_status status;

  if (!bench_open(&b, a, false, 1024u)) {
    t->checks++;
    t->failures++;
    bench_close(&b, t);
    return;
  }
  clock_to_tick(&b, BIG_JUMP_TICKS);
  status = t64h_pump_step(&b.pump, UINT64_MAX, &result);
  CHECK(t, status == T64H_API_ERROR);
  CHECK_U64(t, result.api_status, T64_STATUS_BUFFER_TOO_SMALL);
  CHECK(t, result.message != NULL);
  CHECK_U64(t, b.rec.packets[KIND_VIDEO], 0u);
  CHECK(t, t64_machine_output_status(b.r.machine, &ring) == T64_STATUS_OK);
  CHECK(t, ring.video_queued >= 1u);
  CHECK_U64(t, ring.video_next_bytes, TITLE_FRAME_BYTES);
  (void)printf("stall: small buffer: %s (%s), the %llu-byte packet is still queued (%u in the ring)\n",
               t64h_status_name(status), result.message != NULL ? result.message : "no message",
               (unsigned long long)ring.video_next_bytes, ring.video_queued);
  bench_close(&b, t);
}

static int case_stall(const args *a) {
  tally t = {0ul, 0ul};

  stall_refused_everything(a, &t);
  stall_refused_once_and_terminal(a, &t);
  stall_small_buffer(a, &t);
  return report("stall", &t);
}

/* ---- title-two-speeds ---------------------------------------------------------- */

#define CAPTURE_MAX_VIDEO 32u
#define CAPTURE_MAX_AUDIO 16u
#define CAPTURE_AUDIO_PACKET_BYTES 2048u
#define CAPTURE_TEXT_BYTES 8192u
#define SAVE_BYTES 32768u
#define SPEED_MAX_STEPS 1000000u

/* Everything one run delivered to its sinks, hashed with the host's own SHA-256. */
typedef struct {
  char video[CAPTURE_MAX_VIDEO][T64H_SHA256_HEX_SIZE];
  unsigned video_count;
  char audio[CAPTURE_MAX_AUDIO][T64H_SHA256_HEX_SIZE];
  unsigned audio_count;
  uint8_t audio_stream[CAPTURE_MAX_AUDIO * CAPTURE_AUDIO_PACKET_BYTES];
  size_t audio_stream_length;
  uint8_t text[CAPTURE_TEXT_BYTES];
  size_t text_length;
  bool overflow;
  bool shape_ok;
} capture;

static int capture_video(void *context, const t64_video_packet_info *info, const uint8_t *bytes,
                         size_t length) {
  capture *cap = (capture *)context;

  if (info->width != TITLE_FRAME_WIDTH || info->height != TITLE_FRAME_HEIGHT ||
      info->stride_bytes != TITLE_FRAME_WIDTH * 4u || length != TITLE_FRAME_BYTES ||
      info->packet_bytes != (uint64_t)length) {
    cap->shape_ok = false;
  }
  if (cap->video_count >= CAPTURE_MAX_VIDEO) {
    cap->overflow = true;
    return 0;
  }
  t64h_sha256_hex_of(bytes, length, cap->video[cap->video_count++]);
  return 0;
}

static int capture_audio(void *context, const t64_audio_packet_info *info, const uint8_t *bytes,
                         size_t length) {
  capture *cap = (capture *)context;

  (void)info;
  if (cap->audio_count >= CAPTURE_MAX_AUDIO || length != CAPTURE_AUDIO_PACKET_BYTES) {
    cap->overflow = true;
    return 0;
  }
  t64h_sha256_hex_of(bytes, length, cap->audio[cap->audio_count++]);
  (void)memcpy(cap->audio_stream + cap->audio_stream_length, bytes, length);
  cap->audio_stream_length += length;
  return 0;
}

static int capture_text(void *context, const uint8_t *bytes, size_t length) {
  capture *cap = (capture *)context;

  if (length > sizeof cap->text - cap->text_length) {
    cap->overflow = true;
    return 0;
  }
  (void)memcpy(cap->text + cap->text_length, bytes, length);
  cap->text_length += length;
  return 0;
}

static const char *stop_reason_name(uint32_t reason) {
  switch (reason) {
    case T64_STOP_REASON_BOUND: return "BOUND";
    case T64_STOP_REASON_INSTRUCTION_COUNT: return "INSTRUCTION_COUNT";
    case T64_STOP_REASON_FRAME_EVENT_COUNT: return "FRAME_EVENT_COUNT";
    case T64_STOP_REASON_HALT: return "HALT";
    case T64_STOP_REASON_QUIESCENT: return "QUIESCENT";
    case T64_STOP_REASON_RESOURCE_LIMIT: return "RESOURCE_LIMIT";
    case T64_STOP_REASON_CPU_UNSUPPORTED: return "CPU_UNSUPPORTED";
    case T64_STOP_REASON_GUEST_COMPLETION: return "GUEST_COMPLETION";
    default: return "UNKNOWN";
  }
}

static uint8_t script_bytes[T64H_SCRIPT_MAX_BYTES + 1u];
static t64h_script script_buffer;
static capture run_capture;
static uint8_t save_buffer[SAVE_BYTES];

/* Read at most limit + 1 bytes of a file into script_bytes, so a file over the limit is
 * seen as such by the reader. False, with a message, when it cannot be read. */
static bool load_script_bytes(const char *path, size_t *length_out) {
  FILE *file = fopen(path, "rb");
  size_t length;

  if (file == NULL) {
    (void)fprintf(stderr, "t64h_tests: cannot open a script file\n");
    return false;
  }
  length = fread(script_bytes, 1u, sizeof script_bytes, file);
  if (ferror(file) != 0) {
    (void)fclose(file);
    (void)fprintf(stderr, "t64h_tests: cannot read a script file\n");
    return false;
  }
  (void)fclose(file);
  *length_out = length;
  return true;
}

/* The last path component, so no directory is printed. */
static const char *base_name(const char *path) {
  const char *slash = strrchr(path, '/');

  return slash != NULL ? slash + 1 : path;
}

/* One run: the script through the pump at one speed, the fake clock advanced by 1e9 / hz
 * ns per step until the guest completes; then one JSON line. */
static void run_at_speed(const args *a, const char *script_name, const t64h_script *script,
                         unsigned hz, tally *t) {
  rig r;
  fake_clock clock = {0u, 0u};
  t64h_pump pump;
  t64h_config config;
  t64h_script_source source;
  t64h_step_result result = {0u, 0u, T64_STATUS_OK, NULL};
  t64_output_status ring = T64_OUTPUT_STATUS_INIT;
  uint8_t digest[T64_DIGEST_SIZE];
  char digest_hex[T64H_SHA256_HEX_SIZE];
  char stream_hex[T64H_SHA256_HEX_SIZE];
  char text_hex[T64H_SHA256_HEX_SIZE];
  char save_hex[T64H_SHA256_HEX_SIZE];
  size_t save_length = 0u;
  bool completed = false;
  unsigned steps = 0u;
  unsigned i;

  (void)memset(&run_capture, 0, sizeof run_capture);
  run_capture.shape_ok = true;
  if (!rig_open(&r, a, false)) {
    t->checks++;
    t->failures++;
    rig_close(&r, t);
    return;
  }
  t64h_script_source_init(&source, script, NULL, NULL);
  (void)memset(&config, 0, sizeof config);
  config.machine = r.machine;
  config.clock = fake_clock_read;
  config.clock_context = &clock;
  config.sinks.context = &run_capture;
  config.sinks.video = capture_video;
  config.sinks.audio = capture_audio;
  config.sinks.text = capture_text;
  config.input = t64h_script_feed;
  config.input_context = &source;
  config.video_buffer = video_buffer;
  config.video_capacity = sizeof video_buffer;
  config.audio_buffer = audio_buffer;
  config.audio_capacity = sizeof audio_buffer;
  config.text_buffer = text_buffer;
  config.text_capacity = sizeof text_buffer;
  if (t64h_pump_init(&pump, &config) != T64H_OK) {
    (void)fprintf(stderr, "t64h_tests: pump init refused\n");
    t->checks++;
    t->failures++;
    rig_close(&r, t);
    return;
  }

  while (!completed && steps < SPEED_MAX_STEPS) {
    t64h_status status;

    steps++;
    clock.now = (uint64_t)steps * UINT64_C(1000000000) / (uint64_t)hz;
    status = t64h_pump_step(&pump, UINT64_MAX, &result);
    if (status != T64H_OK) {
      (void)fprintf(stderr, "t64h_tests: %s at %u Hz: step %u answered %s (%s)\n", script_name, hz,
                    steps, t64h_status_name(status), result.message != NULL ? result.message : "");
      break;
    }
    completed = result.stop_reason == T64_STOP_REASON_GUEST_COMPLETION;
  }
  CHECK(t, completed);
  CHECK(t, !run_capture.overflow);
  CHECK(t, run_capture.shape_ok);
  CHECK(t, t64_machine_output_status(r.machine, &ring) == T64_STATUS_OK);
  CHECK_U64(t, ring.video_queued, 0u);
  CHECK_U64(t, ring.audio_queued, 0u);
  CHECK_U64(t, ring.text_pending_bytes, 0u);
  CHECK_U64(t, source.refusal, T64_STATUS_OK);
  CHECK(t, source.queued <= (uint64_t)script->count);
  CHECK(t, t64_machine_save_export(r.machine, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_RAW, save_buffer,
                                   sizeof save_buffer, &save_length) == T64_STATUS_OK);
  CHECK_U64(t, save_length, SAVE_BYTES);
  CHECK(t, t64_machine_digest(r.machine, digest) == T64_STATUS_OK);
  t64h_sha256_hex(digest, digest_hex);
  t64h_sha256_hex_of(run_capture.audio_stream, run_capture.audio_stream_length, stream_hex);
  t64h_sha256_hex_of(run_capture.text, run_capture.text_length, text_hex);
  t64h_sha256_hex_of(save_buffer, save_length, save_hex);

  (void)printf("{\"schema\":\"t64.host-pump-run\",\"script\":\"%s\",\"speed_hz\":%u,"
               "\"completed\":%s,\"stop_reason\":\"%s\",\"completion_tick\":%llu,"
               "\"final_digest\":\"%s\",\"video_packets\":%u,\"video\":[",
               script_name, hz, completed ? "true" : "false", stop_reason_name(result.stop_reason),
               (unsigned long long)result.tick, digest_hex, run_capture.video_count);
  for (i = 0u; i < run_capture.video_count; i++) {
    (void)printf("%s\"%s\"", i == 0u ? "" : ",", run_capture.video[i]);
  }
  (void)printf("],\"audio_packets\":%u,\"audio\":[", run_capture.audio_count);
  for (i = 0u; i < run_capture.audio_count; i++) {
    (void)printf("%s\"%s\"", i == 0u ? "" : ",", run_capture.audio[i]);
  }
  (void)printf("],\"audio_stream\":\"%s\",\"transcript_bytes\":%llu,\"transcript\":\"%s\","
               "\"save_bytes\":%llu,\"save\":\"%s\","
               "\"info\":{\"steps\":%llu,\"advances\":%llu,\"drain_passes\":%llu,"
               "\"backpressure_stops\":%llu,\"stall_retries\":%llu,\"events_queued\":%llu,"
               "\"events_deferred\":%llu}}\n",
               stream_hex, (unsigned long long)run_capture.text_length, text_hex,
               (unsigned long long)save_length, save_hex,
               (unsigned long long)pump.counters.steps, (unsigned long long)pump.counters.advances,
               (unsigned long long)pump.counters.drain_passes,
               (unsigned long long)pump.counters.backpressure_stops,
               (unsigned long long)pump.counters.stall_retries, (unsigned long long)source.queued,
               (unsigned long long)source.deferred);
  rig_close(&r, t);
}

static int case_title_two_speeds(const args *a) {
  tally t = {0ul, 0ul};
  size_t s;
  size_t k;

  CHECK(&t, a->script_count > 0u && a->slice_count >= 2u);
  for (s = 0u; s < a->script_count && t.failures == 0ul; s++) {
    t64h_script_error error;
    size_t length = 0u;

    if (!load_script_bytes(a->scripts[s].path, &length)) {
      t.checks++;
      t.failures++;
      break;
    }
    if (t64h_script_parse(script_bytes, length, NULL, &script_buffer, &error) != T64H_SCRIPT_OK) {
      (void)fprintf(stderr, "t64h_tests: the script %s is refused: rule %s at byte %llu\n",
                    base_name(a->scripts[s].path), t64h_script_status_name(error.status),
                    (unsigned long long)error.offset);
      t.checks++;
      t.failures++;
      break;
    }
    for (k = 0u; k < a->slice_count; k++) {
      run_at_speed(a, base_name(a->scripts[s].path), &script_buffer, a->slice_hz[k], &t);
    }
  }
  return report("title-two-speeds", &t);
}

/* ---- input-map ---------------------------------------------------------------- */

/* One raw pad with the pad present and everything else neutral. */
static t64h_raw_pad neutral_pad(void) {
  t64h_raw_pad raw;

  (void)memset(&raw, 0, sizeof raw);
  raw.present = 1;
  return raw;
}

/* The expected mapping of one host axis value, computed from sign and magnitude (a code
 * path that shares nothing with the table function): truncating division by 256, the
 * dead zone below 8, and for the vertical axis the complement -1 - value first. */
static int expected_axis(int value, bool vertical) {
  const int v = vertical ? -1 - value : value;
  const int magnitude = v < 0 ? -v : v;
  int scaled = magnitude / 256;

  if (v < 0) {
    scaled = -scaled;
  }
  return scaled > -8 && scaled < 8 ? 0 : scaled;
}

static void input_map_axes(tally *t) {
  t64h_raw_pad raw = neutral_pad();
  /* The pinned rows: host value, expected horizontal output, expected vertical output. */
  static const struct {
    int value;
    int horizontal;
    int vertical;
  } rows[] = {
      {-32768, -128, 127}, {32767, 127, -128}, {0, 0, 0},      {2046, 0, 0},    {2047, 0, -8},
      {2048, 8, -8},       {-2047, 0, 0},      {-2048, -8, 0}, {-2049, -8, 8},  {16384, 64, -64},
      {-16384, -64, 63},   {255, 0, 0},        {-256, 0, 0},   {4095, 15, -16}, {-4096, -16, 15},
  };
  size_t i;
  unsigned zeros_h = 0u;
  unsigned zeros_v = 0u;
  int previous_h = -128;
  int previous_v = 127;
  int value;

  for (i = 0u; i < sizeof rows / sizeof rows[0]; i++) {
    t64h_pad pad;

    raw = neutral_pad();
    raw.left_x = (int16_t)rows[i].value;
    raw.left_y = (int16_t)rows[i].value;
    pad = t64h_map_pad(&raw);
    CHECK_U64(t, (unsigned)(pad.x + 128), (unsigned)(rows[i].horizontal + 128));
    CHECK_U64(t, (unsigned)(pad.y + 128), (unsigned)(rows[i].vertical + 128));
  }
  /* Every int16 value: against the independent formula, the dead zone size, monotonic
   * outputs (the vertical axis falls as the host value rises) and the clamp. */
  for (value = -32768; value <= 32767; value++) {
    const int8_t h = t64h_map_axis((int16_t)value, 0);
    const int8_t v = t64h_map_axis((int16_t)value, 1);

    if (h != expected_axis(value, false) || v != expected_axis(value, true) || h < previous_h ||
        v > previous_v) {
      CHECK(t, h == expected_axis(value, false));
      CHECK(t, v == expected_axis(value, true));
      CHECK(t, h >= previous_h);
      CHECK(t, v <= previous_v);
      break;
    }
    previous_h = h;
    previous_v = v;
    zeros_h += h == 0 ? 1u : 0u;
    zeros_v += v == 0 ? 1u : 0u;
  }
  CHECK_U64(t, zeros_h, 4095u);
  CHECK_U64(t, zeros_v, 4095u);
}

static void input_map_buttons(tally *t) {
  /* The mapped controls and their bits, written here from the standard controller mask
   * (A 15, B 14, Z 13, Start 12, up 11, down 10, left 9, right 8). */
  static const struct {
    uint32_t raw;
    uint16_t logical;
  } mapped[] = {
      {T64H_RAW_A, UINT16_C(0x8000)},          {T64H_RAW_B, UINT16_C(0x4000)},
      {T64H_RAW_START, UINT16_C(0x1000)},      {T64H_RAW_DPAD_UP, UINT16_C(0x0800)},
      {T64H_RAW_DPAD_DOWN, UINT16_C(0x0400)},  {T64H_RAW_DPAD_LEFT, UINT16_C(0x0200)},
      {T64H_RAW_DPAD_RIGHT, UINT16_C(0x0100)},
  };
  static const uint32_t unmapped[] = {T64H_RAW_X,           T64H_RAW_Y,
                                      T64H_RAW_BACK,        T64H_RAW_GUIDE,
                                      T64H_RAW_LEFT_STICK,  T64H_RAW_RIGHT_STICK,
                                      T64H_RAW_LEFT_SHOULDER, T64H_RAW_RIGHT_SHOULDER,
                                      UINT32_C(0x80000000), UINT32_C(0x00008000)};
  t64h_raw_pad raw;
  t64h_pad pad;
  uint16_t all = 0u;
  size_t i;

  for (i = 0u; i < sizeof mapped / sizeof mapped[0]; i++) {
    raw = neutral_pad();
    raw.buttons = mapped[i].raw;
    pad = t64h_map_pad(&raw);
    CHECK_U64(t, pad.buttons, mapped[i].logical);
    CHECK_U64(t, pad.x, 0u);
    CHECK_U64(t, pad.y, 0u);
    all = (uint16_t)(all | mapped[i].logical);
  }
  raw = neutral_pad();
  for (i = 0u; i < sizeof mapped / sizeof mapped[0]; i++) {
    raw.buttons |= mapped[i].raw;
  }
  CHECK_U64(t, t64h_map_pad(&raw).buttons, all);
  CHECK_U64(t, all, UINT16_C(0xdf00));
  /* An unmapped control sets nothing, alone or with every mapped one. */
  for (i = 0u; i < sizeof unmapped / sizeof unmapped[0]; i++) {
    raw = neutral_pad();
    raw.buttons = unmapped[i];
    pad = t64h_map_pad(&raw);
    CHECK_U64(t, pad.buttons, 0u);
    raw.buttons |= T64H_RAW_A;
    CHECK_U64(t, t64h_map_pad(&raw).buttons, UINT16_C(0x8000));
  }
  raw = neutral_pad();
  raw.right_x = 32767;
  raw.right_y = -32768;
  raw.right_trigger = 32767;
  pad = t64h_map_pad(&raw);
  CHECK_U64(t, pad.buttons, 0u);
  CHECK_U64(t, pad.x, 0u);
  CHECK_U64(t, pad.y, 0u);
}

static void input_map_trigger_and_removal(tally *t) {
  t64h_raw_pad raw = neutral_pad();
  t64h_pad pad;
  int16_t level;

  /* The left trigger is Z from half travel on: 16383 leaves it clear, 16384 sets bit 13. */
  raw.left_trigger = 16383;
  CHECK_U64(t, t64h_map_pad(&raw).buttons, 0u);
  raw.left_trigger = 16384;
  CHECK_U64(t, t64h_map_pad(&raw).buttons, UINT16_C(0x2000));
  raw.left_trigger = 32767;
  CHECK_U64(t, t64h_map_pad(&raw).buttons, UINT16_C(0x2000));
  raw.left_trigger = 0;
  CHECK_U64(t, t64h_map_pad(&raw).buttons, 0u);
  for (level = -1; level > -32767; level = (int16_t)(level * 2)) {
    raw.left_trigger = level;  /* a malformed negative reading is never past half */
    CHECK_U64(t, t64h_map_pad(&raw).buttons, 0u);
  }
  raw.left_trigger = 20000;
  raw.buttons = T64H_RAW_A;
  CHECK_U64(t, t64h_map_pad(&raw).buttons, UINT16_C(0xa000));

  /* A removed pad is neutral whatever its last reading was; so is no pad at all. */
  (void)memset(&raw, 0xff, sizeof raw);
  raw.present = 0;
  raw.left_x = 32767;
  raw.left_y = -32768;
  raw.left_trigger = 32767;
  pad = t64h_map_pad(&raw);
  CHECK_U64(t, pad.buttons, 0u);
  CHECK_U64(t, pad.x, 0u);
  CHECK_U64(t, pad.y, 0u);
  pad = t64h_map_pad(NULL);
  CHECK_U64(t, pad.buttons, 0u);
  CHECK_U64(t, pad.x, 0u);
  CHECK_U64(t, pad.y, 0u);
  raw.present = 1;
  CHECK(t, t64h_map_pad(&raw).buttons != 0u);
}

static int case_input_map(const args *a) {
  tally t = {0ul, 0ul};

  (void)a;
  input_map_axes(&t);
  input_map_buttons(&t);
  input_map_trigger_and_removal(&t);
  return report("input-map", &t);
}

/* ---- script -------------------------------------------------------------------- */

/* Seed documents are written with ' for the double quote (no JSON text here holds an
 * apostrophe) and unquoted before the read. */
static void unquote(const char *text, char *out, size_t capacity) {
  size_t i = 0u;

  for (; text[i] != '\0' && i + 1u < capacity; i++) {
    out[i] = text[i] == '\'' ? '"' : text[i];
  }
  out[i] = '\0';
}

#define EV_OK "{'buttons':0,'port':0,'tick':'100','x':0,'y':0}"
#define DOC(events) "{'events':[" events "],'schema':'t64.input','schema_version':1}"

typedef struct {
  const char *name;
  const char *text;
  const char *rule;  /* the rule name the reader must give ("ok": accepted) */
} seed;

static const seed seeds[] = {
    {"base", DOC(EV_OK), "ok"},
    {"whitespace, key order and extreme values",
     " \n{ 'schema_version' : 1 ,\t'schema':'t64.input', 'events' : [ { 'y':-128,'x':127,"
     "'tick':'18446744073709551615','port':0,'buttons':65535 } ]}\r\n\n",
     "ok"},
    {"no events", DOC(""), "ok"},
    {"equal ticks", DOC(EV_OK "," EV_OK), "ok"},
    {"unknown key in an event", DOC("{'buttons':0,'port':0,'tick':'1','x':0,'y':0,'z':1}"),
     "unknown-key"},
    {"unknown key at the root", "{'events':[],'extra':1,'schema':'t64.input','schema_version':1}",
     "unknown-key"},
    {"event without y", DOC("{'buttons':0,'port':0,'tick':'1','x':0}"), "missing-key"},
    {"event without tick", DOC("{'buttons':0,'port':0,'x':0,'y':0}"), "missing-key"},
    {"root without events", "{'schema':'t64.input','schema_version':1}", "missing-key"},
    {"empty root", "{}", "missing-key"},
    {"empty event", DOC("{}"), "missing-key"},
    {"duplicate key in an event",
     DOC("{'buttons':0,'port':0,'tick':'1','x':0,'x':1,'y':0}"), "duplicate-key"},
    {"duplicate key at the root",
     "{'events':[],'schema':'t64.input','schema':'t64.input','schema_version':1}",
     "duplicate-key"},
    {"tick as a number", DOC("{'buttons':0,'port':0,'tick':100,'x':0,'y':0}"), "tick-type"},
    {"tick as null", DOC("{'buttons':0,'port':0,'tick':null,'x':0,'y':0}"), "tick-type"},
    {"tick with a plus sign", DOC("{'buttons':0,'port':0,'tick':'+100','x':0,'y':0}"),
     "tick-format"},
    {"tick with a minus sign", DOC("{'buttons':0,'port':0,'tick':'-1','x':0,'y':0}"),
     "tick-format"},
    {"tick with a leading zero", DOC("{'buttons':0,'port':0,'tick':'0100','x':0,'y':0}"),
     "tick-format"},
    {"empty tick", DOC("{'buttons':0,'port':0,'tick':'','x':0,'y':0}"), "tick-format"},
    {"tick with a letter", DOC("{'buttons':0,'port':0,'tick':'12a','x':0,'y':0}"), "tick-format"},
    {"tick of 21 digits",
     DOC("{'buttons':0,'port':0,'tick':'100000000000000000000','x':0,'y':0}"), "tick-format"},
    {"tick over 2^64 - 1",
     DOC("{'buttons':0,'port':0,'tick':'18446744073709551616','x':0,'y':0}"), "tick-format"},
    {"port 1", DOC("{'buttons':0,'port':1,'tick':'1','x':0,'y':0}"), "port"},
    {"port as a string", DOC("{'buttons':0,'port':'0','tick':'1','x':0,'y':0}"), "port"},
    {"x 128", DOC("{'buttons':0,'port':0,'tick':'1','x':128,'y':0}"), "axis-x"},
    {"x -129", DOC("{'buttons':0,'port':0,'tick':'1','x':-129,'y':0}"), "axis-x"},
    {"x as a fraction", DOC("{'buttons':0,'port':0,'tick':'1','x':1.5,'y':0}"), "axis-x"},
    {"x as a string", DOC("{'buttons':0,'port':0,'tick':'1','x':'5','y':0}"), "axis-x"},
    {"y 128", DOC("{'buttons':0,'port':0,'tick':'1','x':0,'y':128}"), "axis-y"},
    {"y -129", DOC("{'buttons':0,'port':0,'tick':'1','x':0,'y':-129}"), "axis-y"},
    {"buttons 65536", DOC("{'buttons':65536,'port':0,'tick':'1','x':0,'y':0}"), "buttons"},
    {"buttons -1", DOC("{'buttons':-1,'port':0,'tick':'1','x':0,'y':0}"), "buttons"},
    {"buttons as an object", DOC("{'buttons':{},'port':0,'tick':'1','x':0,'y':0}"), "buttons"},
    {"schema_version 2", "{'events':[],'schema':'t64.input','schema_version':2}",
     "schema-version"},
    {"schema_version as a string", "{'events':[],'schema':'t64.input','schema_version':'1'}",
     "schema-version"},
    {"another schema", "{'events':[],'schema':'t64.replay','schema_version':1}", "schema"},
    {"schema as a number", "{'events':[],'schema':5,'schema_version':1}", "schema"},
    {"trailing garbage", DOC(EV_OK) "x", "trailing-data"},
    {"a second document", DOC(EV_OK) "{}", "trailing-data"},
    {"trailing whitespace only", DOC(EV_OK) " \n\n", "ok"},
    {"ticks falling", DOC("{'buttons':0,'port':0,'tick':'200','x':0,'y':0},"
                          "{'buttons':0,'port':0,'tick':'100','x':0,'y':0}"),
     "time-reversed"},
    {"no bytes", "", "syntax"},
    {"truncated", "{'events':[{'buttons':0,'port'", "syntax"},
    {"array root", "[]", "syntax"},
    {"trailing comma in the events", DOC(EV_OK ","), "syntax"},
    {"missing colon", "{'events' [],'schema':'t64.input','schema_version':1}", "syntax"},
    {"unterminated string", "{'events':[],'schema':'t64.input", "syntax"},
    {"escape in a string", "{'events':[],'schema':'t64\\u002einput','schema_version':1}", "syntax"},
    {"leading zero in a number", DOC("{'buttons':00,'port':0,'tick':'1','x':0,'y':0}"), "syntax"},
    {"bare word", "{'events':[],'schema':'t64.input','schema_version':tru}", "syntax"},
};

#define SEED_TEXT_MAX 512u
#define BIG_DOC_MAX 320000u

static char big_doc[BIG_DOC_MAX];

/* An events document of the given count of minimal events, ticks ascending; its length. */
static size_t build_events_doc(unsigned count) {
  static const char head[] = "{\"events\":[";
  static const char tail[] = "],\"schema\":\"t64.input\",\"schema_version\":1}";
  size_t length = 0u;
  unsigned i;

  (void)memcpy(big_doc, head, sizeof head - 1u);
  length = sizeof head - 1u;
  for (i = 0u; i < count; i++) {
    const int n = snprintf(big_doc + length, BIG_DOC_MAX - length,
                           "%s{\"tick\":\"%u\",\"port\":0,\"buttons\":0,\"x\":0,\"y\":0}",
                           i == 0u ? "" : ",", i);

    length += (size_t)n;
  }
  (void)memcpy(big_doc + length, tail, sizeof tail - 1u);
  return length + sizeof tail - 1u;
}

/* One read of text with the limits; the rule name it gave. */
static const char *rule_of(const void *bytes, size_t length, const t64h_script_limits *limits,
                           t64h_script_error *error) {
  return t64h_script_status_name(
      t64h_script_parse((const uint8_t *)bytes, length, limits, &script_buffer, error));
}

static void script_seeds(tally *t) {
  char text[SEED_TEXT_MAX];
  size_t i;

  for (i = 0u; i < sizeof seeds / sizeof seeds[0]; i++) {
    t64h_script_error error;
    const char *rule;

    unquote(seeds[i].text, text, sizeof text);
    script_buffer.count = 99u;
    rule = rule_of(text, strlen(text), NULL, &error);
    if (strcmp(rule, seeds[i].rule) != 0) {
      (void)fprintf(stderr, "t64h_tests: seed \"%s\": rule %s, expected %s\n", seeds[i].name, rule,
                    seeds[i].rule);
    }
    CHECK(t, strcmp(rule, seeds[i].rule) == 0);
    CHECK(t, strcmp(seeds[i].rule, "ok") == 0 ? true : script_buffer.count == 0u);
    CHECK(t, error.offset <= strlen(text));
  }
}

static void script_limits(tally *t) {
  t64h_script_error error;
  t64h_script_limits wide = {1u << 20, 4096u};
  t64h_script_limits few = {T64H_SCRIPT_MAX_BYTES, 3u};
  t64h_script_limits huge = {1u << 20, 100000u};
  size_t length;
  size_t base_length;

  /* The default limits, by value. */
  CHECK_U64(t, T64H_SCRIPT_MAX_BYTES, 65536u);
  CHECK_U64(t, T64H_SCRIPT_MAX_EVENTS, 4096u);

  /* 65,536 bytes are accepted, 65,537 are refused by the size rule before any parse. */
  unquote(DOC(EV_OK), big_doc, sizeof big_doc);
  base_length = strlen(big_doc);
  (void)memset(big_doc + base_length, ' ', 65537u - base_length);
  CHECK(t, strcmp(rule_of(big_doc, 65536u, NULL, &error), "ok") == 0);
  CHECK(t, strcmp(rule_of(big_doc, 65537u, NULL, &error), "size") == 0);
  CHECK_U64(t, error.offset, 65537u);
  CHECK_U64(t, script_buffer.count, 0u);

  /* 4,096 events are accepted and the 4,097th is refused by the event rule; at the default
   * byte limit the same documents are over 64 KB (a minimal event is 46 bytes), so the size
   * rule is the one that answers there. */
  length = build_events_doc(4096u);
  CHECK(t, length > 65536u);
  CHECK(t, strcmp(rule_of(big_doc, length, NULL, &error), "size") == 0);
  CHECK(t, strcmp(rule_of(big_doc, length, &wide, &error), "ok") == 0);
  CHECK_U64(t, script_buffer.count, 4096u);
  CHECK_U64(t, script_buffer.events[4095].tick, 4095u);
  length = build_events_doc(4097u);
  CHECK(t, strcmp(rule_of(big_doc, length, &wide, &error), "event-limit") == 0);
  CHECK_U64(t, script_buffer.count, 0u);
  CHECK(t, error.offset > 0u && error.offset < length);
  CHECK(t, strcmp(rule_of(big_doc, length, &huge, &error), "event-limit") == 0);

  /* A lowered limit is the same code path: 4 events against a limit of 3. */
  length = build_events_doc(3u);
  CHECK(t, strcmp(rule_of(big_doc, length, &few, &error), "ok") == 0);
  length = build_events_doc(4u);
  CHECK(t, strcmp(rule_of(big_doc, length, &few, &error), "event-limit") == 0);

  /* NULL arguments. */
  CHECK(t, t64h_script_parse(NULL, 0u, NULL, &script_buffer, &error) == T64H_SCRIPT_BAD_ARGUMENT);
  CHECK(t, t64h_script_parse((const uint8_t *)"{}", 2u, NULL, NULL, &error) == T64H_SCRIPT_BAD_ARGUMENT);
}

/* The committed scripts: the reader finds exactly the events the case was told to expect
 * (read from the same file by another parser). */
static void script_committed(const args *a, tally *t) {
  size_t s;

  CHECK(t, a->script_count > 0u);
  for (s = 0u; s < a->script_count; s++) {
    t64h_script_error error;
    size_t length = 0u;
    size_t i;

    CHECK(t, a->scripts[s].event_count > 0u);
    if (!load_script_bytes(a->scripts[s].path, &length)) {
      t->checks++;
      t->failures++;
      continue;
    }
    CHECK(t, strcmp(rule_of(script_bytes, length, NULL, &error), "ok") == 0);
    CHECK_U64(t, script_buffer.count, a->scripts[s].event_count);
    for (i = 0u; i < script_buffer.count && i < a->scripts[s].event_count; i++) {
      CHECK_U64(t, script_buffer.events[i].tick, a->scripts[s].events[i].tick);
      CHECK_U64(t, script_buffer.events[i].buttons, a->scripts[s].events[i].buttons);
      CHECK_U64(t, script_buffer.events[i].x + 128, a->scripts[s].events[i].axis_x + 128);
      CHECK_U64(t, script_buffer.events[i].y + 128, a->scripts[s].events[i].axis_y + 128);
    }
    (void)printf("script: %s parses to %llu event(s)\n", base_name(a->scripts[s].path),
                 (unsigned long long)script_buffer.count);
  }
}

/* ---- the scripted source against a model of a small queue ------------------------ */

typedef struct {
  size_t capacity;
  uint64_t guest_tick;
  uint64_t held[8];
  size_t held_count;
  uint64_t accepted[16];
  size_t accepted_count;
  t64_status forced;  /* not OK: every call answers it */
  unsigned calls;
} queue_model;

/* The model guest reaches a tick: the events stamped before it are consumed. */
static void model_reach(queue_model *m, uint64_t tick) {
  size_t kept = 0u;
  size_t i;

  m->guest_tick = tick;
  for (i = 0u; i < m->held_count; i++) {
    if (m->held[i] >= tick) {
      m->held[kept++] = m->held[i];
    }
  }
  m->held_count = kept;
}

static t64_status model_queue(void *context, t64_machine *machine, uint64_t tick,
                              uint16_t buttons, int32_t x, int32_t y) {
  queue_model *m = (queue_model *)context;

  (void)machine;
  (void)buttons;
  (void)x;
  (void)y;
  m->calls++;
  if (m->forced != T64_STATUS_OK) {
    return m->forced;
  }
  if (tick < m->guest_tick) {
    return T64_STATUS_TIME_REVERSED;
  }
  if (m->held_count >= m->capacity) {
    return T64_STATUS_QUEUE_FULL;
  }
  m->held[m->held_count++] = tick;
  m->accepted[m->accepted_count++] = tick;
  return T64_STATUS_OK;
}

static void set_script_ticks(const uint64_t *ticks, size_t count) {
  size_t i;

  script_buffer.count = count;
  for (i = 0u; i < count; i++) {
    script_buffer.events[i].tick = ticks[i];
    script_buffer.events[i].buttons = (uint16_t)(i + 1u);
    script_buffer.events[i].x = 0;
    script_buffer.events[i].y = 0;
  }
}

static void source_model_cases(tally *t) {
  static const uint64_t ticks[] = {10u, 20u, 30u, 40u};
  queue_model m;
  t64h_script_source source;
  uint64_t bound;

  /* Lazily: only the events at or below the bound are queued, in file order. */
  (void)memset(&m, 0, sizeof m);
  m.capacity = 2u;
  set_script_ticks(ticks, 4u);
  t64h_script_source_init(&source, &script_buffer, model_queue, &m);
  bound = 25u;
  CHECK_U64(t, t64h_script_feed(&source, NULL, 0u, &bound), 0u);
  CHECK_U64(t, m.accepted_count, 2u);
  CHECK_U64(t, m.accepted[0], 10u);
  CHECK_U64(t, m.accepted[1], 20u);
  CHECK_U64(t, bound, 25u);
  CHECK_U64(t, source.deferred, 0u);

  /* A QUEUE_FULL answer defers the event: the bound drops to its tick, nothing after it
   * is tried, and the next call (the guest has consumed the older events) queues it. */
  bound = 100u;
  CHECK_U64(t, t64h_script_feed(&source, NULL, 25u, &bound), 0u);
  CHECK_U64(t, m.accepted_count, 2u);  /* the model still holds 10 and 20: full */
  CHECK_U64(t, bound, 30u);
  CHECK_U64(t, source.deferred, 1u);
  CHECK_U64(t, source.next, 2u);
  model_reach(&m, 30u);
  bound = 100u;
  CHECK_U64(t, t64h_script_feed(&source, NULL, 30u, &bound), 0u);
  CHECK_U64(t, m.accepted_count, 4u);  /* 30 and 40 now fit the freed queue */
  CHECK_U64(t, m.accepted[2], 30u);
  CHECK_U64(t, m.accepted[3], 40u);
  CHECK_U64(t, bound, 100u);
  CHECK_U64(t, source.queued, 4u);
  CHECK_U64(t, source.next, 4u);
  CHECK_U64(t, source.refusal, T64_STATUS_OK);
  /* Nothing is left: a later call queues nothing and touches the machine no more. */
  bound = 1000u;
  m.calls = 0u;
  CHECK_U64(t, t64h_script_feed(&source, NULL, 40u, &bound), 0u);
  CHECK_U64(t, m.calls, 0u);

  /* A deferred event that the guest has already reached can never be queued in time:
   * the feed fails with QUEUE_FULL instead of letting the guest pass it. */
  (void)memset(&m, 0, sizeof m);
  m.capacity = 1u;
  {
    static const uint64_t same[] = {20u, 20u};

    set_script_ticks(same, 2u);
  }
  t64h_script_source_init(&source, &script_buffer, model_queue, &m);
  bound = 100u;
  CHECK_U64(t, t64h_script_feed(&source, NULL, 0u, &bound), 0u);
  CHECK_U64(t, bound, 20u);
  model_reach(&m, 20u);  /* the stamp 20 is not below 20: still held */
  bound = 100u;
  CHECK_U64(t, t64h_script_feed(&source, NULL, 20u, &bound), 1u);
  CHECK_U64(t, source.refusal, T64_STATUS_QUEUE_FULL);
  CHECK_U64(t, source.deferred, 2u);

  /* Any other refusal ends the feed and names the core status. */
  (void)memset(&m, 0, sizeof m);
  m.capacity = 4u;
  m.forced = T64_STATUS_NOT_READY;
  set_script_ticks(ticks, 4u);
  t64h_script_source_init(&source, &script_buffer, model_queue, &m);
  bound = 100u;
  CHECK_U64(t, t64h_script_feed(&source, NULL, 0u, &bound), 1u);
  CHECK_U64(t, source.refusal, T64_STATUS_NOT_READY);
  CHECK_U64(t, source.queued, 0u);
  /* NULL context or bound is a failure, not a crash. */
  CHECK_U64(t, t64h_script_feed(NULL, NULL, 0u, &bound), 1u);
  CHECK_U64(t, t64h_script_feed(&source, NULL, 0u, NULL), 1u);
}

/* After the title completes the pump queues nothing: a script with one event beyond the
 * completion, stepped in small slices, is never fed that event, and later steps do not
 * call the source at all. */
static void source_terminal_case(const args *a, tally *t) {
  char doc[1024];
  size_t length;
  size_t i;
  rig r;
  fake_clock clock = {0u, 0u};
  t64h_pump pump;
  t64h_config config;
  t64h_script_source source;
  t64h_step_result result = {0u, 0u, T64_STATUS_OK, NULL};
  t64h_script_error error;
  uint64_t calls_at_completion;
  uint64_t queued_at_completion;
  unsigned steps = 0u;
  bool completed = false;

  CHECK(t, a->script_count > 0u && a->scripts[0].event_count > 0u);
  if (a->script_count == 0u || a->scripts[0].event_count == 0u) {
    return;
  }
  /* The committed events (as the arguments list them) and one more at tick 20,000,000. */
  length = (size_t)snprintf(doc, sizeof doc, "{\"events\":[");
  for (i = 0u; i < a->scripts[0].event_count; i++) {
    const input_event *e = &a->scripts[0].events[i];

    length += (size_t)snprintf(doc + length, sizeof doc - length,
                               "{\"tick\":\"%llu\",\"port\":0,\"buttons\":%u,\"x\":%d,\"y\":%d},",
                               (unsigned long long)e->tick, (unsigned)e->buttons, (int)e->axis_x,
                               (int)e->axis_y);
  }
  length += (size_t)snprintf(doc + length, sizeof doc - length,
                             "{\"tick\":\"20000000\",\"port\":0,\"buttons\":0,\"x\":0,\"y\":0}],"
                             "\"schema\":\"t64.input\",\"schema_version\":1}");
  CHECK(t, t64h_script_parse((const uint8_t *)doc, length, NULL, &script_buffer, &error) ==
               T64H_SCRIPT_OK);
  CHECK_U64(t, script_buffer.count, a->scripts[0].event_count + 1u);
  if (!rig_open(&r, a, false)) {
    t->checks++;
    t->failures++;
    rig_close(&r, t);
    return;
  }
  t64h_script_source_init(&source, &script_buffer, NULL, NULL);
  (void)memset(&config, 0, sizeof config);
  config.machine = r.machine;
  config.clock = fake_clock_read;
  config.clock_context = &clock;
  config.input = t64h_script_feed;
  config.input_context = &source;
  config.video_buffer = video_buffer;
  config.video_capacity = sizeof video_buffer;
  config.audio_buffer = audio_buffer;
  config.audio_capacity = sizeof audio_buffer;
  config.text_buffer = text_buffer;
  config.text_capacity = sizeof text_buffer;
  if (t64h_pump_init(&pump, &config) != T64H_OK) {
    t->checks++;
    t->failures++;
    rig_close(&r, t);
    return;
  }
  while (!completed && steps < MAX_STEPS * 4u) {
    clock.now += SMALL_SLICE_TICKS * T64H_TICK_NS;
    steps++;
    if (t64h_pump_step(&pump, UINT64_MAX, &result) != T64H_OK) {
      break;
    }
    completed = result.stop_reason == T64_STOP_REASON_GUEST_COMPLETION;
  }
  CHECK(t, completed);
  CHECK_U64(t, source.queued, a->scripts[0].event_count);
  CHECK_U64(t, source.next, a->scripts[0].event_count);
  calls_at_completion = source.calls;
  queued_at_completion = source.queued;
  for (i = 0u; i < 3u; i++) {
    clock.now += 100000u * T64H_TICK_NS;
    CHECK(t, t64h_pump_step(&pump, UINT64_MAX, &result) == T64H_TERMINAL);
  }
  CHECK_U64(t, source.calls, calls_at_completion);
  CHECK_U64(t, source.queued, queued_at_completion);
  CHECK_U64(t, source.refusal, T64_STATUS_OK);
  (void)printf("script: terminal: %llu of %llu events queued before the completion at tick %llu, "
               "%llu source call(s) then and after three more steps\n",
               (unsigned long long)queued_at_completion, (unsigned long long)script_buffer.count,
               (unsigned long long)result.tick, (unsigned long long)source.calls);
  rig_close(&r, t);
}

static int case_script(const args *a) {
  tally t = {0ul, 0ul};

  script_seeds(&t);
  script_limits(&t);
  script_committed(a, &t);
  source_model_cases(&t);
  source_terminal_case(a, &t);
  return report("script", &t);
}

/* ---- frames -------------------------------------------------------------------- */

#define FRAME_SENTINEL 0xa5u
#define SYNTH_MAX_BYTES (320u * 4u * 240u + 4096u)

static uint8_t synth_packet[SYNTH_MAX_BYTES];
static uint8_t synth_dest[SYNTH_MAX_BYTES];

/* A synthetic RGBA8 packet info of the given geometry. */
static t64_video_packet_info synth_info(uint32_t width, uint32_t height, uint32_t stride) {
  t64_video_packet_info info = T64_VIDEO_PACKET_INFO_INIT;

  info.width = width;
  info.height = height;
  info.stride_bytes = stride;
  info.output_encoding = T64_VIDEO_ENCODING_RGBA8;
  info.packet_bytes = (uint64_t)stride * height;
  return info;
}

/* Fill a packet whose pixel bytes follow a pattern and whose stride padding is 0xee. */
static size_t synth_fill(uint32_t width, uint32_t height, uint32_t stride) {
  size_t y;
  size_t x;

  (void)memset(synth_packet, 0xee, sizeof synth_packet);
  for (y = 0u; y < height; y++) {
    for (x = 0u; x < width * 4u; x++) {
      synth_packet[y * stride + x] = (uint8_t)((y * 31u + x * 7u + 3u) & 0xffu);
    }
  }
  return (size_t)stride * height;
}

static bool dest_untouched(size_t length) {
  size_t i;

  for (i = 0u; i < length; i++) {
    if (synth_dest[i] != FRAME_SENTINEL) {
      return false;
    }
  }
  return true;
}

static void frames_copy_cases(tally *t) {
  t64_video_packet_info info;
  size_t packet_length;
  size_t y;
  bool rows_equal;

  /* The title's geometry: 128 x 96, stride 512, to a tight buffer of pitch 512, byte for
   * byte. */
  info = synth_info(128u, 96u, 512u);
  packet_length = synth_fill(128u, 96u, 512u);
  (void)memset(synth_dest, FRAME_SENTINEL, sizeof synth_dest);
  CHECK(t, t64h_frame_copy(&info, synth_packet, packet_length, synth_dest, 512u * 96u, 512u) ==
               T64H_FRAME_OK);
  CHECK(t, memcmp(synth_dest, synth_packet, packet_length) == 0);
  CHECK_U64(t, synth_dest[512u * 96u], FRAME_SENTINEL);  /* nothing past the frame */

  /* A stride of 520: only 512 bytes of each row are copied, the 8 padding bytes are not. */
  info = synth_info(128u, 96u, 520u);
  packet_length = synth_fill(128u, 96u, 520u);
  (void)memset(synth_dest, FRAME_SENTINEL, sizeof synth_dest);
  CHECK(t, t64h_frame_copy(&info, synth_packet, packet_length, synth_dest, 512u * 96u, 512u) ==
               T64H_FRAME_OK);
  rows_equal = true;
  for (y = 0u; y < 96u; y++) {
    rows_equal = rows_equal && memcmp(synth_dest + y * 512u, synth_packet + y * 520u, 512u) == 0;
  }
  CHECK(t, rows_equal);
  CHECK_U64(t, synth_dest[512u * 96u], FRAME_SENTINEL);
  /* A wider destination pitch: rows land at the pitch and the gap is left alone. */
  info = synth_info(128u, 96u, 512u);
  packet_length = synth_fill(128u, 96u, 512u);
  (void)memset(synth_dest, FRAME_SENTINEL, sizeof synth_dest);
  CHECK(t, t64h_frame_copy(&info, synth_packet, packet_length, synth_dest, 600u * 95u + 512u,
                           600u) == T64H_FRAME_OK);
  rows_equal = true;
  for (y = 0u; y < 96u; y++) {
    rows_equal = rows_equal && memcmp(synth_dest + y * 600u, synth_packet + y * 512u, 512u) == 0;
    if (y < 95u) {
      rows_equal = rows_equal && synth_dest[y * 600u + 512u] == FRAME_SENTINEL &&
                   synth_dest[y * 600u + 599u] == FRAME_SENTINEL;
    }
  }
  CHECK(t, rows_equal);

  /* The largest packet the host supports is 320 x 240: accepted exactly, one more is not. */
  info = synth_info(320u, 240u, 1280u);
  packet_length = synth_fill(320u, 240u, 1280u);
  (void)memset(synth_dest, FRAME_SENTINEL, sizeof synth_dest);
  CHECK(t, t64h_frame_copy(&info, synth_packet, packet_length, synth_dest, 1280u * 240u, 1280u) ==
               T64H_FRAME_OK);
  CHECK(t, memcmp(synth_dest, synth_packet, packet_length) == 0);
  info = synth_info(321u, 96u, 1284u);
  packet_length = synth_fill(321u, 96u, 1284u);
  (void)memset(synth_dest, FRAME_SENTINEL, sizeof synth_dest);
  CHECK(t, t64h_frame_copy(&info, synth_packet, packet_length, synth_dest, sizeof synth_dest,
                           1284u) == T64H_FRAME_TOO_LARGE);
  CHECK(t, dest_untouched(sizeof synth_dest));
  info = synth_info(128u, 241u, 512u);
  packet_length = synth_fill(128u, 241u, 512u);
  CHECK(t, t64h_frame_copy(&info, synth_packet, packet_length, synth_dest, sizeof synth_dest,
                           512u) == T64H_FRAME_TOO_LARGE);
  CHECK(t, dest_untouched(sizeof synth_dest));
}

static void frames_refusal_cases(tally *t) {
  t64_video_packet_info info = synth_info(128u, 96u, 512u);
  const size_t packet_length = synth_fill(128u, 96u, 512u);
  t64_video_packet_info other;

  /* The padding after the last row is not needed: a stride 520 packet without it is whole. */
  other = synth_info(128u, 96u, 520u);
  (void)synth_fill(128u, 96u, 520u);
  CHECK(t, t64h_frame_copy(&other, synth_packet, 520u * 95u + 512u, synth_dest, 512u * 96u, 512u) ==
               T64H_FRAME_OK);
  (void)synth_fill(128u, 96u, 512u);

  /* A destination one row short is refused and left untouched; so is a packet one byte
   * short of its rows. */
  (void)memset(synth_dest, FRAME_SENTINEL, sizeof synth_dest);
  CHECK(t, t64h_frame_copy(&info, synth_packet, packet_length, synth_dest, 512u * 95u, 512u) ==
               T64H_FRAME_SHORT_DESTINATION);
  CHECK(t, dest_untouched(sizeof synth_dest));
  CHECK(t, t64h_frame_copy(&info, synth_packet, packet_length, synth_dest, 512u * 96u - 1u, 512u) ==
               T64H_FRAME_SHORT_DESTINATION);
  CHECK(t, dest_untouched(sizeof synth_dest));
  CHECK(t, t64h_frame_copy(&info, synth_packet, packet_length - 1u, synth_dest, 512u * 96u, 512u) ==
               T64H_FRAME_SHORT_SOURCE);
  CHECK(t, dest_untouched(sizeof synth_dest));
  /* A stride or pitch under one row, an empty frame, another encoding, NULL pointers. */
  other = synth_info(128u, 96u, 508u);
  CHECK(t, t64h_frame_copy(&other, synth_packet, packet_length, synth_dest, sizeof synth_dest,
                           512u) == T64H_FRAME_BAD_GEOMETRY);
  CHECK(t, t64h_frame_copy(&info, synth_packet, packet_length, synth_dest, sizeof synth_dest,
                           508u) == T64H_FRAME_BAD_GEOMETRY);
  other = synth_info(0u, 96u, 512u);
  CHECK(t, t64h_frame_copy(&other, synth_packet, packet_length, synth_dest, sizeof synth_dest,
                           512u) == T64H_FRAME_BAD_GEOMETRY);
  other = synth_info(128u, 0u, 512u);
  CHECK(t, t64h_frame_copy(&other, synth_packet, packet_length, synth_dest, sizeof synth_dest,
                           512u) == T64H_FRAME_BAD_GEOMETRY);
  other = info;
  other.output_encoding = 0u;
  CHECK(t, t64h_frame_copy(&other, synth_packet, packet_length, synth_dest, sizeof synth_dest,
                           512u) == T64H_FRAME_BAD_ENCODING);
  CHECK(t, t64h_frame_copy(NULL, synth_packet, packet_length, synth_dest, sizeof synth_dest,
                           512u) == T64H_FRAME_BAD_ARGUMENT);
  CHECK(t, t64h_frame_copy(&info, NULL, packet_length, synth_dest, sizeof synth_dest, 512u) ==
               T64H_FRAME_BAD_ARGUMENT);
  CHECK(t, t64h_frame_copy(&info, synth_packet, packet_length, NULL, sizeof synth_dest, 512u) ==
               T64H_FRAME_BAD_ARGUMENT);
  CHECK(t, dest_untouched(sizeof synth_dest));
}

static void frames_swap_cases(tally *t) {
  static const uint8_t bytes[] = {0x12u, 0x34u, 0x80u, 0x00u, 0x7fu, 0xffu, 0xffu, 0xffu,
                                  0x00u, 0x00u, 0x80u, 0x01u, 0xe0u, 0x00u, 0x20u, 0x00u};
  static const int expected[] = {0x1234, -32768, 32767, -1, 0, -32767, -8192, 8192};
  int16_t samples[8];
  size_t i;

  (void)memset(samples, 0x55, sizeof samples);
  t64h_swap_s16be(bytes, 8u, samples);
  for (i = 0u; i < 8u; i++) {
    CHECK_U64(t, (unsigned)(samples[i] + 32768), (unsigned)(expected[i] + 32768));
  }
  /* A count of zero writes nothing. */
  (void)memset(samples, 0x55, sizeof samples);
  t64h_swap_s16be(bytes, 0u, samples);
  CHECK_U64(t, samples[0], 0x5555u);
}

static int case_frames(const args *a) {
  tally t = {0ul, 0ul};

  (void)a;
  frames_copy_cases(&t);
  frames_refusal_cases(&t);
  frames_swap_cases(&t);
  return report("frames", &t);
}

/* ---- dispatch ----------------------------------------------------------------- */

typedef struct {
  const char *name;
  int (*run)(const args *a);
} test_case;

static const test_case cases[] = {
    {"drain-order", case_drain_order},
    {"clock", case_clock},
    {"stall", case_stall},
    {"title-two-speeds", case_title_two_speeds},
    {"input-map", case_input_map},
    {"script", case_script},
    {"frames", case_frames},
};

int main(int argc, char **argv) {
  args a;
  size_t i;

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
      (void)puts(cases[i].name);
    }
    return 0;
  }
  if (t64h_sha256_selfcheck() != 0u) {
    (void)fprintf(stderr, "t64h_tests: the SHA-256 self-check failed\n");
    return 1;
  }
  if (!parse_args(argc, argv, &a) || a.case_name == NULL) {
    (void)fprintf(stderr,
                  "usage: t64h_tests --case NAME --rom PATH [--expect HEX] [--event TICK:BUTTONS:X:Y]...\n"
                  "                  [--script PATH [--event ...]]... [--slice-hz HZ[,HZ]...]\n"
                  "       t64h_tests --list-cases\n");
    return 2;
  }
  for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
    if (strcmp(a.case_name, cases[i].name) == 0) {
      return cases[i].run(&a);
    }
  }
  (void)fprintf(stderr, "t64h_tests: unknown case %s\n", a.case_name);
  return 2;
}
