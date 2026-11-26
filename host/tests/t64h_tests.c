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

#include "t64h_pump.h"
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

typedef struct {
  const char *case_name;
  const char *rom_path;
  const char *expect_hex;
  input_event events[MAX_EVENTS];
  size_t event_count;
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
    } else if (strcmp(argv[i], "--event") == 0 && i + 1 < argc) {
      if (out->event_count >= MAX_EVENTS ||
          !parse_event(argv[++i], &out->events[out->event_count])) {
        (void)fprintf(stderr, "t64h_tests: bad or too many --event arguments\n");
        return false;
      }
      out->event_count++;
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
                       uint64_t bound) {
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

/* ---- dispatch ----------------------------------------------------------------- */

typedef struct {
  const char *name;
  int (*run)(const args *a);
} test_case;

static const test_case cases[] = {
    {"drain-order", case_drain_order},
    {"clock", case_clock},
    {"stall", case_stall},
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
