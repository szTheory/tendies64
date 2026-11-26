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
 *
 * The title ROM is the fixture of every case that runs a guest (build target
 * t64-phase12-title-fixture). The expected frame digest and the events come from
 * fixtures/title/ through scripts/check-host.sh, never from this file or the pump.
 */
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

/* Create a machine of the title profile, admit the ROM and queue the events. */
static bool rig_open(rig *r, const args *a) {
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
  for (i = 0u; i < a->event_count; i++) {
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

typedef struct {
  unsigned kind;
  uint64_t pass;
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
  char first_video_hex[T64H_SHA256_HEX_SIZE];
  bool have_first_video;
  bool video_shape_ok;
} recorder;

static void record(recorder *rec, unsigned kind, const uint8_t *bytes, size_t length) {
  char hex[T64H_SHA256_HEX_SIZE];

  t64h_sha256_hex_of(bytes, length, hex);
  if (rec->count < LOG_MAX) {
    log_entry *entry = &rec->log[rec->count++];

    entry->kind = kind;
    entry->pass = rec->pump->counters.drain_passes;
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

  if (info->width != TITLE_FRAME_WIDTH || info->height != TITLE_FRAME_HEIGHT ||
      info->stride_bytes != TITLE_FRAME_WIDTH * 4u || length != TITLE_FRAME_BYTES ||
      info->packet_bytes != (uint64_t)length) {
    rec->video_shape_ok = false;
  }
  record(rec, KIND_VIDEO, bytes, length);
  return 0;
}

static int record_audio(void *context, const t64_audio_packet_info *info,
                        const uint8_t *bytes, size_t length) {
  recorder *rec = (recorder *)context;

  (void)info;
  record(rec, KIND_AUDIO, bytes, length);
  return 0;
}

static int record_text(void *context, const uint8_t *bytes, size_t length) {
  record((recorder *)context, KIND_TEXT, bytes, length);
  return 0;
}

static const char *kind_name(unsigned kind) {
  return kind == KIND_VIDEO ? "video" : (kind == KIND_AUDIO ? "audio" : "text");
}

/* ---- drain-order -------------------------------------------------------------- */

#define SMALL_SLICE_TICKS UINT64_C(262144)
#define SMALL_PHASE_TICKS UINT64_C(1800000)
#define BIG_JUMP_TICKS UINT64_C(4000000)
#define MAX_STEPS 64u

static int case_drain_order(const args *a) {
  static uint8_t video_buffer[T64H_VIDEO_BUFFER_BYTES];
  static uint8_t audio_buffer[T64H_AUDIO_BUFFER_BYTES];
  static uint8_t text_buffer[T64H_TEXT_BUFFER_BYTES];
  tally t = {0ul, 0ul};
  rig r;
  recorder rec;
  fake_clock clock;
  t64h_pump pump;
  t64h_config config;
  t64h_step_result result;
  unsigned steps = 0u;
  bool completed = false;
  bool all_ordered = true;
  bool full_pass = false;
  size_t i;

  (void)memset(&rec, 0, sizeof rec);
  (void)memset(&clock, 0, sizeof clock);
  rec.pump = &pump;
  rec.video_shape_ok = true;
  CHECK(&t, a->expect_hex != NULL && a->event_count > 0u);
  if (!rig_open(&r, a)) {
    rig_close(&r, &t);
    return report("drain-order", &t) | 1;
  }
  (void)memset(&config, 0, sizeof config);
  config.machine = r.machine;
  config.clock = fake_clock_read;
  config.clock_context = &clock;
  config.sinks.context = &rec;
  config.sinks.video = record_video;
  config.sinks.audio = record_audio;
  config.sinks.text = record_text;
  config.video_buffer = video_buffer;
  config.video_capacity = sizeof video_buffer;
  config.audio_buffer = audio_buffer;
  config.audio_capacity = sizeof audio_buffer;
  config.text_buffer = text_buffer;
  config.text_capacity = sizeof text_buffer;
  CHECK(&t, t64h_pump_init(&pump, &config) == T64H_OK);

  /* Small slices first, one drain pass at the end of each, then one jump of host time
   * that carries the guest to its completion: the passes inside that step hold the
   * packets the small slices never met (video, audio and text together). */
  while (!completed && steps < MAX_STEPS) {
    const uint64_t slice = pump.tick < SMALL_PHASE_TICKS ? SMALL_SLICE_TICKS : BIG_JUMP_TICKS;
    t64h_status status;

    clock.now += slice * T64H_TICK_NS;
    status = t64h_pump_step(&pump, UINT64_MAX, &result);
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
  for (i = 0u; i < rec.count; i++) {
    (void)printf("drain-order: pass %llu: %s %llu bytes sha256=%s...\n",
                 (unsigned long long)rec.log[i].pass, kind_name(rec.log[i].kind),
                 (unsigned long long)rec.log[i].length, rec.log[i].sha_prefix);
  }
  for (i = 0u; i + 1u < rec.count; i++) {
    if (rec.log[i + 1u].pass < rec.log[i].pass ||
        (rec.log[i + 1u].pass == rec.log[i].pass && rec.log[i + 1u].kind < rec.log[i].kind)) {
      all_ordered = false;
    }
  }
  for (i = 0u; i < rec.count; i++) {
    unsigned seen = 0u;
    size_t j;

    for (j = 0u; j < rec.count; j++) {
      if (rec.log[j].pass == rec.log[i].pass) {
        seen |= 1u << rec.log[j].kind;
      }
    }
    if (seen == 7u) {
      full_pass = true;
    }
  }
  CHECK(&t, rec.overflow == 0u);
  CHECK(&t, all_ordered);
  CHECK(&t, full_pass);
  CHECK_U64(&t, rec.packets[KIND_VIDEO], SHORT_VIDEO_PACKETS);
  CHECK_U64(&t, rec.packets[KIND_AUDIO], SHORT_AUDIO_PACKETS);
  CHECK_U64(&t, rec.text_bytes, SHORT_TEXT_BYTES);
  CHECK_U64(&t, pump.counters.video_packets, SHORT_VIDEO_PACKETS);
  CHECK_U64(&t, pump.counters.audio_packets, SHORT_AUDIO_PACKETS);
  CHECK_U64(&t, pump.counters.text_bytes, SHORT_TEXT_BYTES);
  CHECK(&t, rec.video_shape_ok);
  CHECK(&t, rec.have_first_video && a->expect_hex != NULL &&
                strcmp(rec.first_video_hex, a->expect_hex) == 0);
  (void)printf("drain-order: %u steps, %llu advances, %llu drain passes, first video packet "
               "sha256=%s (expected %s), order %s, one pass with video, audio and text: %s\n",
               steps, (unsigned long long)pump.counters.advances,
               (unsigned long long)pump.counters.drain_passes,
               rec.have_first_video ? rec.first_video_hex : "none",
               a->expect_hex != NULL ? a->expect_hex : "none", all_ordered ? "ok" : "WRONG",
               full_pass ? "yes" : "NO");
  rig_close(&r, &t);
  return report("drain-order", &t);
}

/* ---- dispatch ----------------------------------------------------------------- */

typedef struct {
  const char *name;
  int (*run)(const args *a);
} test_case;

static const test_case cases[] = {
    {"drain-order", case_drain_order},
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
