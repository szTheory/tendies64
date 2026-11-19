/*
 * Phase 9 pad-poll tracer and controls (plan 09-03). The authored pad-poll ROM
 * programs the modeled SI and PIF with a two-channel joybus frame, polls four
 * times and prints one line per poll. Expected transcripts are literals
 * derived by fixtures/pad/poll_oracle.py outside the ROM and outside the core.
 * Every timing figure is a named software-profile policy value written out as
 * a literal here (never taken from a production helper); nothing is a
 * hardware or timing claim.
 *
 * usage: test_pad_poll CASE SUPERVISOR TEST_SUPERVISOR CHECKER ROOT
 *        test_pad_poll --suite NAME SUPERVISOR TEST_SUPERVISOR CHECKER ROOT
 *        test_pad_poll --list-cases
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

#include "t64_json.h"
#include "t64_sha256.h"
#include "tendies64/t64_machine.h"

#define PROFILE_STEM "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1"

static const char *g_supervisor;
static const char *g_checker;
static const char *g_root;
static char g_evidence[PATH_MAX];
static int g_failures;

static void check_impl(bool ok, const char *expr, int line) {
  if (!ok) {
    g_failures++;
    (void)fprintf(stderr, "FAIL line %d: %s\n", line, expr);
  }
}
#define CHECK(cond) check_impl((cond), #cond, __LINE__)

/* ---- files and evidence ------------------------------------------------- */

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

static bool join_path(char *out, size_t capacity, const char *a,
                      const char *b) {
  int length = snprintf(out, capacity, "%s/%s", a, b);
  return length > 0 && (size_t)length < capacity;
}

static bool prepare_evidence(void) {
  const char *configured = getenv("T64_PHASE09_EVIDENCE_DIR");
  const char *temp = getenv("TMPDIR");
  struct stat info;
  if (configured != NULL && configured[0] != '\0') {
    if (snprintf(g_evidence, sizeof g_evidence, "%s", configured) >=
        (int)sizeof g_evidence) {
      return false;
    }
    /* Concurrent CTest jobs share this directory: a racing mkdir that finds
     * the directory already present is success. */
    if (mkdir(g_evidence, 0700) != 0 && errno != EEXIST) return false;
    return stat(g_evidence, &info) == 0 && S_ISDIR(info.st_mode);
  }
  if (temp == NULL || temp[0] == '\0') temp = "/tmp";
  if (snprintf(g_evidence, sizeof g_evidence, "%s/t64-phase09.XXXXXX",
               temp) >= (int)sizeof g_evidence) {
    return false;
  }
  return mkdtemp(g_evidence) != NULL;
}

/* A fresh private working directory for one run. */
static bool make_run_dir(char out[PATH_MAX]) {
  if (snprintf(out, PATH_MAX, "%s/p09.XXXXXX", g_evidence) >= PATH_MAX) {
    return false;
  }
  return mkdtemp(out) != NULL;
}

static bool fixture_path(char out[PATH_MAX], const char *stem) {
  char name[128];
  char dir[PATH_MAX];
  if (snprintf(name, sizeof name, "%s.z64", stem) >= (int)sizeof name ||
      !join_path(dir, sizeof dir, g_root, "build/fixtures")) {
    return false;
  }
  return join_path(out, PATH_MAX, dir, name);
}

static bool input_path(char out[PATH_MAX]) {
  char dir[PATH_MAX];
  return join_path(dir, sizeof dir, g_root, "build/fixtures") &&
         join_path(out, PATH_MAX, dir, "hello.input.json");
}
/* ---- result records ----------------------------------------------------- */

typedef struct {
  bool parsed;
  bool checker_ok;
  int supervisor_exit;
  char outcome[48];
  char detail[64];
  char stop_kind[48];
  char fixture_id[96];
  char profile_id[96];
  char log_sha256[65];
  bool has_log_sha256;
  uint64_t guest_tick;
  uint64_t bound_tick;
  uint64_t log_bytes;
  size_t milestone_count;
  char completion_pc[24]; /* pc of the last milestone, e.g. ffffffff80000470 */
  char dir[PATH_MAX];
  char result_path[PATH_MAX];
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
  const t64_json_node *digest;
  uint64_t value = 0u;
  if (!read_file(path, bytes, sizeof bytes, &length) || length == 0u ||
      t64_json_parse(bytes, length, NULL, &doc, NULL) != 0) {
    return false;
  }
  stop = t64_json_get(doc.root, "guest_stop");
  counts = t64_json_get(doc.root, "counts");
  digest = t64_json_get(doc.root, "log_sha256");
  copy_string(out->outcome, sizeof out->outcome,
              t64_json_get(doc.root, "outcome"));
  copy_string(out->detail, sizeof out->detail,
              t64_json_get(doc.root, "detail"));
  copy_string(out->fixture_id, sizeof out->fixture_id,
              t64_json_get(doc.root, "fixture_id"));
  copy_string(out->profile_id, sizeof out->profile_id,
              t64_json_get(doc.root, "profile_id"));
  copy_string(out->stop_kind, sizeof out->stop_kind,
              stop == NULL ? NULL : t64_json_get(stop, "kind"));
  copy_string(out->log_sha256, sizeof out->log_sha256, digest);
  out->has_log_sha256 = digest != NULL && digest->type == T64_JSON_STRING;
  if (t64_json_u64_string(t64_json_get(doc.root, "guest_tick"), &value)) {
    out->guest_tick = value;
  }
  if (stop != NULL &&
      t64_json_u64_string(t64_json_get(stop, "bound_tick"), &value)) {
    out->bound_tick = value;
  }
  if (counts != NULL &&
      t64_json_u64_string(t64_json_get(counts, "log_bytes"), &value)) {
    out->log_bytes = value;
  }
  {
    const t64_json_node *milestones = t64_json_get(doc.root, "milestones");
    const t64_json_node *item = milestones == NULL ? NULL : milestones->first;
    out->milestone_count = 0u;
    out->completion_pc[0] = '\0';
    while (item != NULL) {
      out->milestone_count++;
      copy_string(out->completion_pc, sizeof out->completion_pc,
                  t64_json_get(item, "pc"));
      item = item->next;
    }
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

/* ---- supervised run ----------------------------------------------------- */

typedef struct {
  const char *supervisor; /* production or test twin */
  const char *fixture;
  const char *input;      /* NULL uses the empty default schedule */
  uint64_t bound;         /* 0 leaves the option out */
  uint64_t advance_step;  /* 0 leaves the option out (test twin only) */
  int video_fd;           /* 0 leaves the option out */
  int audio_fd;           /* 0 leaves the option out */
  const char *test_fault; /* NULL leaves the option out (test twin only) */
  const char *artifact_dir; /* NULL leaves the pair out */
  uint64_t artifact_max;
  bool retain;            /* keep command.args and input.json for the collector */
} run_spec;

#define MAX_ARGS 48u

typedef struct {
  char *items[MAX_ARGS];
  size_t count;
} arg_list;

static void arg_add(arg_list *list, char *value) {
  if (list->count < MAX_ARGS - 1u) list->items[list->count++] = value;
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

/* Keeps the exact command (one argument per line, the supervisor first) and a
 * copy of the input script next to result.json in the private run directory. */
static bool retain_run_evidence(const char *dir, const arg_list *list,
                                const char *input) {
  char text[8192];
  char path[PATH_MAX];
  uint8_t script[8192];
  size_t script_length = 0u;
  size_t offset = 0u;
  size_t i;
  for (i = 0u; i < list->count; i++) {
    size_t length = strlen(list->items[i]);
    if (length + 1u > sizeof text - offset) return false;
    memcpy(text + offset, list->items[i], length);
    offset += length;
    text[offset++] = '\n';
  }
  if (!join_path(path, sizeof path, dir, "command.args") ||
      !write_new_file(path, (const uint8_t *)text, offset)) {
    return false;
  }
  if (!read_file(input, script, sizeof script, &script_length) ||
      script_length == 0u || script_length >= sizeof script ||
      !join_path(path, sizeof path, dir, "input.json")) {
    return false;
  }
  return write_new_file(path, script, script_length);
}

static bool run_supervised(const run_spec *spec, run_out *out) {
  char result_path[PATH_MAX];
  char meta_path[PATH_MAX];
  char capture_path[PATH_MAX];
  char input[PATH_MAX];
  char bound_text[24];
  char advance_text[24];
  char video_text[24];
  char audio_text[24];
  char artifact_max_text[24];
  char input_size_text[24];
  char input_sha_text[T64_SHA256_HEX_SIZE];
  pid_t pid;
  int status = 0;
  memset(out, 0, sizeof *out);
  input_size_text[0] = '\0';
  input_sha_text[0] = '\0';
  if (!make_run_dir(out->dir) ||
      (spec->input == NULL ? !input_path(input)
                           : snprintf(input, sizeof input, "%s",
                                      spec->input) >= (int)sizeof input) ||
      !join_path(result_path, sizeof result_path, out->dir, "result.json") ||
      !join_path(meta_path, sizeof meta_path, out->dir, "meta.json") ||
      !join_path(capture_path, sizeof capture_path, out->dir, "capture.bin")) {
    return false;
  }
  (void)snprintf(out->result_path, sizeof out->result_path, "%s",
                 result_path);
  if (spec->input != NULL) {
    /* A schedule other than the empty default is declared by size and digest. */
    uint8_t script[8192];
    uint8_t digest[T64_SHA256_DIGEST_SIZE];
    size_t script_length = 0u;
    t64_sha256_ctx ctx;
    if (!read_file(spec->input, script, sizeof script, &script_length) ||
        script_length == 0u || script_length >= sizeof script) {
      return false;
    }
    t64_sha256_init(&ctx);
    t64_sha256_update(&ctx, script, script_length);
    t64_sha256_final(&ctx, digest);
    t64_sha256_hex(digest, input_sha_text);
    (void)snprintf(input_size_text, sizeof input_size_text, "%zu",
                   script_length);
  }
  (void)snprintf(bound_text, sizeof bound_text, "%llu",
                 (unsigned long long)spec->bound);
  (void)snprintf(advance_text, sizeof advance_text, "%llu",
                 (unsigned long long)spec->advance_step);
  (void)snprintf(video_text, sizeof video_text, "%d", spec->video_fd);
  (void)snprintf(audio_text, sizeof audio_text, "%d", spec->audio_fd);
  (void)snprintf(artifact_max_text, sizeof artifact_max_text, "%llu",
                 (unsigned long long)spec->artifact_max);
  {
    char real_option[] = "--real";
    char result_option[] = "--result";
    char meta_option[] = "--meta";
    char work_option[] = "--work-dir";
    char wall_option[] = "--wall-ms";
    char wall_value[] = "30000";
    char bound_option[] = "--bound";
    char fixture_option[] = "--fixture";
    char input_option[] = "--input";
    char input_size_option[] = "--input-size";
    char input_sha_option[] = "--input-sha256";
    char capture_option[] = "--capture-out";
    char advance_option[] = "--advance-step";
    char video_option[] = "--video-observation-fd";
    char audio_option[] = "--audio-observation-fd";
    char fault_option[] = "--test-fault";
    char artifact_dir_option[] = "--artifact-dir";
    char artifact_max_option[] = "--artifact-max-bytes";
    arg_list list = {{NULL}, 0u};
    arg_add(&list, (char *)(uintptr_t)spec->supervisor);
    arg_add(&list, real_option);
    arg_add(&list, result_option);
    arg_add(&list, result_path);
    arg_add(&list, meta_option);
    arg_add(&list, meta_path);
    arg_add(&list, work_option);
    arg_add(&list, out->dir);
    arg_add(&list, wall_option);
    arg_add(&list, wall_value);
    if (spec->bound != 0u) {
      arg_add(&list, bound_option);
      arg_add(&list, bound_text);
    }
    arg_add(&list, fixture_option);
    arg_add(&list, (char *)(uintptr_t)spec->fixture);
    arg_add(&list, input_option);
    arg_add(&list, input);
    if (spec->input != NULL) {
      arg_add(&list, input_size_option);
      arg_add(&list, input_size_text);
      arg_add(&list, input_sha_option);
      arg_add(&list, input_sha_text);
    }
    arg_add(&list, capture_option);
    arg_add(&list, capture_path);
    if (spec->advance_step != 0u) {
      arg_add(&list, advance_option);
      arg_add(&list, advance_text);
    }
    if (spec->video_fd != 0) {
      arg_add(&list, video_option);
      arg_add(&list, video_text);
    }
    if (spec->audio_fd != 0) {
      arg_add(&list, audio_option);
      arg_add(&list, audio_text);
    }
    if (spec->test_fault != NULL) {
      arg_add(&list, fault_option);
      arg_add(&list, (char *)(uintptr_t)spec->test_fault);
    }
    if (spec->artifact_dir != NULL) {
      arg_add(&list, artifact_dir_option);
      arg_add(&list, (char *)(uintptr_t)spec->artifact_dir);
      arg_add(&list, artifact_max_option);
      arg_add(&list, artifact_max_text);
    }
    list.items[list.count] = NULL;
    pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
      execv(spec->supervisor, list.items);
      _exit(127);
    }
    while (waitpid(pid, &status, 0) < 0) {
      if (errno != EINTR) return false;
    }
    out->supervisor_exit = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    out->parsed = parse_record(result_path, out);
    out->checker_ok = out->parsed && checker_accepts(result_path);
    if (spec->retain && out->parsed && out->checker_ok) {
      /* Collector interface (plan 09-03.1): the validated record, the exact
       * command and the input script stay in the private run directory. */
      if (!retain_run_evidence(out->dir, &list, input)) return false;
      (void)fprintf(stderr, "private phase09 runner evidence retained at %s\n",
                    out->dir);
    }
    return out->parsed;
  }
}

/* ---- policy literals and expected transcripts --------------------------- */

/* Independent literals (policy, never read from the core): the frame polls
 * channel 0 (state command, one command byte and four response bytes:
 * 2000 + 188 + 438 + 8000 + 250 = 10,876 ticks) and channel 1 with no device
 * (2000 + 188 + 3907 = 6,095 ticks), so the handshake total is 16,971 ticks
 * and the read tail is 4,096 ticks. */
#define HANDSHAKE_TICKS UINT64_C(16971)
#define READ_TAIL_TICKS UINT64_C(4096)
#define MIN_POLL_PERIOD (HANDSHAKE_TICKS + READ_TAIL_TICKS)
#define POLL_COUNT 4u
#define SI_RD64B_ADDRESS UINT32_C(0x04800004)
#define BASELINE_ROM "phase09-pad-poll-si-v1"
#define BASELINE_SHA256 \
  "715aa3854b455dc9fd8e038e1aa27e5d200ea311acbfd032930cdc760498d3f6"
/* Oracle scenario neutral: no input at all. */
static const char neutral_transcript[] =
    "P1 00000000 84 -\n"
    "P2 00000000 84 +\n"
    "P3 00000000 84 +\n"
    "P4 00000000 84 +\n";

/* ---- in-process poll measurement ---------------------------------------- */

static bool pad_allocate(void *context, size_t size, size_t alignment,
                         t64_host_storage *storage_out) {
  void *memory;
  (void)context;
  if (storage_out == NULL || size == 0u || alignment == 0u) return false;
  memory = malloc(size);
  if (memory == NULL || (uintptr_t)memory % alignment != 0u) {
    free(memory);
    return false;
  }
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  return true;
}

static void pad_release(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static const t64_host_allocator allocator = {pad_allocate, pad_release, NULL};

typedef struct {
  uint64_t starts[POLL_COUNT];
  size_t start_count;
  uint64_t completion_tick;
  uint64_t dropped;
  uint8_t log[256];
  size_t log_length;
  bool completed;
} poll_measure;

/* Runs the ROM in process under the stem profile with the neutral schedule
 * (no input event), advancing in slices of 16 ticks like the real runner, and
 * records the tick of every 32-bit bus write to SI RD64B (the poll starts). */
static bool measure_polls(const char *rom_path, poll_measure *out) {
  static uint8_t rom[8192];
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  size_t rom_length = 0u;
  uint64_t tick = 0u;
  bool ok = false;

  memset(out, 0, sizeof *out);
  if (!read_file(rom_path, rom, sizeof rom, &rom_length) || rom_length == 0u ||
      rom_length >= sizeof rom ||
      t64_profile_lookup(PROFILE_STEM, &profile) != T64_STATUS_OK ||
      t64_machine_create(&allocator, profile, &machine) != T64_STATUS_OK ||
      machine == NULL) {
    return false;
  }
  if (t64_machine_admit_rom(machine, rom, rom_length) != T64_STATUS_OK ||
      t64_machine_trace_set_enabled(machine, true) != T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return false;
  }
  while (tick < UINT64_C(1000000)) {
    t64_stop_request request = {T64_STOP_GUEST_TICK, tick + UINT64_C(16)};
    t64_stop_result stop = {0u, 0u};
    t64_trace_record records[64];
    size_t count;
    uint64_t dropped = 0u;
    bool saturated = false;
    if (t64_machine_advance(machine, &request, &stop) != T64_STATUS_OK) break;
    tick = stop.tick;
    do {
      size_t i;
      count = 0u;
      if (t64_machine_trace_drain(machine, records, 64u, &count, &dropped,
                                  &saturated) != T64_STATUS_OK) {
        count = 0u;
        break;
      }
      for (i = 0u; i < count; i++) {
        if (records[i].device == T64_TRACE_DEVICE_BUS &&
            records[i].kind == T64_TRACE_KIND_BUS_WRITE32 &&
            (uint32_t)(records[i].value >> 32) == SI_RD64B_ADDRESS &&
            out->start_count < POLL_COUNT) {
          out->starts[out->start_count++] = records[i].tick;
        }
      }
    } while (count == 64u);
    out->dropped = dropped;
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

/* ---- input scripts ------------------------------------------------------- */

typedef struct {
  uint64_t tick;
  unsigned buttons;
  int x;
  int y;
} input_row;

static bool write_input_script(char out[PATH_MAX], const char *name,
                               const input_row *rows, size_t count) {
  char text[1024];
  size_t length;
  size_t i;
  size_t offset = 0u;
  int fd;
  if (snprintf(out, PATH_MAX, "%s/%s", g_evidence, name) >= PATH_MAX) {
    return false;
  }
  length = (size_t)snprintf(text, sizeof text, "{\"events\":[");
  for (i = 0u; i < count; i++) {
    int added = snprintf(text + length, sizeof text - length,
                         "%s{\"buttons\":%u,\"port\":0,\"tick\":\"%llu\","
                         "\"x\":%d,\"y\":%d}",
                         i == 0u ? "" : ",", rows[i].buttons,
                         (unsigned long long)rows[i].tick, rows[i].x,
                         rows[i].y);
    if (added < 0 || (size_t)added >= sizeof text - length) return false;
    length += (size_t)added;
  }
  {
    int added = snprintf(text + length, sizeof text - length,
                         "],\"schema\":\"t64.input\",\"schema_version\":1}\n");
    if (added < 0 || (size_t)added >= sizeof text - length) return false;
    length += (size_t)added;
  }
  fd = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd < 0) return false;
  while (offset < length) {
    ssize_t written = write(fd, text + offset, length - offset);
    if (written < 0 && errno == EINTR) continue;
    if (written <= 0) {
      (void)close(fd);
      return false;
    }
    offset += (size_t)written;
  }
  return close(fd) == 0;
}

/* ---- cases ---------------------------------------------------------------- */

static void expect_pass(const run_out *run) {
  CHECK(run->parsed);
  CHECK(run->checker_ok);
  CHECK(run->supervisor_exit == 0);
  CHECK(strcmp(run->outcome, "pass") == 0);
  CHECK(strcmp(run->detail, "positive-match") == 0);
  CHECK(strcmp(run->stop_kind, "authored-completion") == 0);
}

/* One marker line per control run for the collector (plan 09-03.1): the case
 * name, the observed outcome and detail and the guest tick of the run whose
 * comparator was just reached. The values are cross-checked against the
 * retained result record by scripts/qualify-phase09.py. */
static void report_control(const char *name, const run_out *run) {
  if (!run->parsed) return;
  (void)printf("case reached: %s outcome=%s detail=%s guest_tick=%llu\n", name,
               run->outcome, run->detail, (unsigned long long)run->guest_tick);
}

/* The ROM polls four times; consecutive RD64B starts are at least one full
 * handshake plus read tail apart (21,067 ticks), and E(n) is S(n) plus the
 * 16,971-tick handshake. The neutral run also prints the oracle's neutral
 * scenario, which witnesses the Count step and the modeled SI path together.
 * The absolute start ticks are measured, never pinned. */
static int case_poll_timing(void) {
  char rom[PATH_MAX];
  poll_measure measure;
  size_t i;
  CHECK(fixture_path(rom, BASELINE_ROM));
  CHECK(measure_polls(rom, &measure));
  CHECK(measure.completed);
  CHECK(measure.dropped == 0u);
  CHECK(measure.start_count == POLL_COUNT);
  for (i = 0u; i + 1u < measure.start_count; i++) {
    CHECK(measure.starts[i + 1u] - measure.starts[i] >= MIN_POLL_PERIOD);
  }
  CHECK(measure.log_length == sizeof neutral_transcript - 1u);
  CHECK(measure.log_length == sizeof neutral_transcript - 1u &&
        memcmp(measure.log, neutral_transcript, measure.log_length) == 0);
  for (i = 0u; i < measure.start_count; i++) {
    (void)fprintf(stderr, "poll %zu start tick %llu execute tick %llu\n",
                  i + 1u, (unsigned long long)measure.starts[i],
                  (unsigned long long)(measure.starts[i] + HANDSHAKE_TICKS));
  }
  (void)fprintf(stderr, "neutral completion tick %llu\n",
                (unsigned long long)measure.completion_tick);
  if (g_failures == 0 && measure.start_count == POLL_COUNT) {
    (void)printf("case reached: pad-poll-timing starts S1=%llu S2=%llu "
                 "S3=%llu S4=%llu\n",
                 (unsigned long long)measure.starts[0],
                 (unsigned long long)measure.starts[1],
                 (unsigned long long)measure.starts[2],
                 (unsigned long long)measure.starts[3]);
  }
  return g_failures == 0 ? 0 : 1;
}

/* The accepted run: input stamped at E1 - 1, E2 and E3 + 1 of consecutive
 * polls is seen exactly as the strictly-after rule requires, so the 68-byte
 * transcript equals the oracle's baseline scenario. */
static int case_accepted_poll_sequence(void) {
  char rom[PATH_MAX];
  char script[PATH_MAX];
  poll_measure measure;
  input_row rows[3];
  run_spec spec = {0};
  run_out run;
  CHECK(fixture_path(rom, BASELINE_ROM));
  CHECK(measure_polls(rom, &measure));
  CHECK(measure.start_count == POLL_COUNT);
  if (measure.start_count != POLL_COUNT) return 1;
  /* State A, B and C of the oracle: buttons 0x8000, 0x4021, 0x1802. */
  rows[0].tick = measure.starts[0] + HANDSHAKE_TICKS - 1u;
  rows[0].buttons = 0x8000u;
  rows[0].x = 1;
  rows[0].y = -1;
  rows[1].tick = measure.starts[1] + HANDSHAKE_TICKS;
  rows[1].buttons = 0x4021u;
  rows[1].x = 127;
  rows[1].y = -128;
  rows[2].tick = measure.starts[2] + HANDSHAKE_TICKS + 1u;
  rows[2].buttons = 0x1802u;
  rows[2].x = -1;
  rows[2].y = 1;
  CHECK(write_input_script(script, "pad-baseline.input.json", rows, 3u));
  spec.supervisor = g_supervisor;
  spec.fixture = rom;
  spec.input = script;
  spec.bound = 1000000u;
  spec.retain = true;
  CHECK(run_supervised(&spec, &run));
  expect_pass(&run);
  CHECK(strcmp(run.fixture_id, BASELINE_ROM) == 0);
  CHECK(strcmp(run.profile_id, PROFILE_STEM) == 0);
  CHECK(run.log_bytes == 68u);
  CHECK(strcmp(run.log_sha256, BASELINE_SHA256) == 0);
  CHECK(run.guest_tick < 1000000u);
  (void)fprintf(stderr, "accepted run completion tick %llu\n",
                (unsigned long long)run.guest_tick);
  if (g_failures == 0) {
    (void)printf("case reached: pad-poll-accepted completion_tick=%llu\n",
                 (unsigned long long)run.guest_tick);
  }
  return g_failures == 0 ? 0 : 1;
}

/* ---- controls (each runs the accepted baseline in the same setup) ----------- */

#define WRONG_ROM "phase09-pad-poll-wrong-si-v1"
/* Oracle scenarios, literals from fixtures/pad/poll_oracle.py: the transcript
 * of the shifted script (state B stamped one tick below E2), the neutral
 * script, and the wrong-port-flag ROM (byte 8 printed as 04). */
static const char shifted_transcript[] =
    "P1 800001ff 84 -\n"
    "P2 40217f80 84 +\n"
    "P3 40217f80 84 +\n"
    "P4 1802ff01 84 +\n";
#define SHIFTED_SHA256 \
  "4c78899114ba1e1d4111c02a84a059e3943d6c3d00208c533bbc3e7faeb07910"
#define NEUTRAL_SHA256 \
  "caf55c819271e372f4e4185520a49ed74c36d79c870bfa30d1dbc064c9f9f137"
static const char wrong_rom_transcript[] =
    "P1 800001ff 04 -\n"
    "P2 800001ff 04 +\n"
    "P3 40217f80 04 +\n"
    "P4 1802ff01 04 +\n";
#define WRONG_ROM_SHA256 \
  "07de0176dbe67a8c313b1871e019b588d00793e8a4002fae4ce3f6cecf333315"

static void expect_wrong_transcript(const run_out *run) {
  CHECK(run->parsed);
  CHECK(run->checker_ok);
  CHECK(strcmp(run->outcome, "guest-fail") == 0);
  CHECK(strcmp(run->detail, "wrong-transcript") == 0);
  CHECK(strcmp(run->stop_kind, "authored-completion") == 0);
  CHECK(run->log_bytes == 68u);
}

static bool capture_equals(const run_out *run, const char *literal,
                           size_t length) {
  char path[PATH_MAX];
  uint8_t bytes[256];
  size_t got = 0u;
  if (!join_path(path, sizeof path, run->dir, "capture.bin") ||
      !read_file(path, bytes, sizeof bytes, &got)) {
    return false;
  }
  return got == length && memcmp(bytes, literal, length) == 0;
}

/* Measures the poll starts and runs the accepted baseline; returns the rows
 * of the baseline script through `rows`. */
static bool accepted_baseline(const char *rom, poll_measure *measure,
                              input_row rows[3], run_spec *spec,
                              char script[PATH_MAX]) {
  run_out run;
  if (!measure_polls(rom, measure) || measure->start_count != POLL_COUNT) {
    return false;
  }
  rows[0].tick = measure->starts[0] + HANDSHAKE_TICKS - 1u;
  rows[0].buttons = 0x8000u;
  rows[0].x = 1;
  rows[0].y = -1;
  rows[1].tick = measure->starts[1] + HANDSHAKE_TICKS;
  rows[1].buttons = 0x4021u;
  rows[1].x = 127;
  rows[1].y = -128;
  rows[2].tick = measure->starts[2] + HANDSHAKE_TICKS + 1u;
  rows[2].buttons = 0x1802u;
  rows[2].x = -1;
  rows[2].y = 1;
  if (!write_input_script(script, "pad-control-baseline.input.json", rows,
                          3u)) {
    return false;
  }
  memset(spec, 0, sizeof *spec);
  spec->supervisor = g_supervisor;
  spec->fixture = rom;
  spec->input = script;
  spec->bound = 1000000u;
  if (!run_supervised(spec, &run)) return false;
  expect_pass(&run);
  CHECK(strcmp(run.log_sha256, BASELINE_SHA256) == 0);
  return true;
}

/* The same ROM and row, a script that crosses E (state B stamped one tick
 * below E2) and an all-neutral script each reach the transcript comparator
 * and fail it with the oracle's own transcripts. */
static int case_same_setup_wrong_script(void) {
  char rom[PATH_MAX];
  char script[PATH_MAX];
  char shifted_path[PATH_MAX];
  char neutral_path[PATH_MAX];
  poll_measure measure;
  input_row rows[3];
  run_spec spec;
  run_out run;
  CHECK(fixture_path(rom, BASELINE_ROM));
  CHECK(accepted_baseline(rom, &measure, rows, &spec, script));
  if (measure.start_count != POLL_COUNT) return 1;

  rows[1].tick = measure.starts[1] + HANDSHAKE_TICKS - 1u;
  CHECK(write_input_script(shifted_path, "pad-shifted.input.json", rows, 3u));
  spec.input = shifted_path;
  spec.retain = true;
  CHECK(run_supervised(&spec, &run));
  expect_wrong_transcript(&run);
  CHECK(strcmp(run.fixture_id, BASELINE_ROM) == 0);
  CHECK(strcmp(run.log_sha256, SHIFTED_SHA256) == 0);
  CHECK(strcmp(run.log_sha256, BASELINE_SHA256) != 0);
  CHECK(capture_equals(&run, shifted_transcript,
                       sizeof shifted_transcript - 1u));
  report_control("same-setup-wrong-script", &run);

  CHECK(write_input_script(neutral_path, "pad-neutral.input.json", rows, 0u));
  spec.input = neutral_path;
  CHECK(run_supervised(&spec, &run));
  expect_wrong_transcript(&run);
  CHECK(strcmp(run.log_sha256, NEUTRAL_SHA256) == 0);
  CHECK(capture_equals(&run, neutral_transcript,
                       sizeof neutral_transcript - 1u));
  report_control("same-setup-wrong-script", &run);
  return g_failures == 0 ? 0 : 1;
}

/* The wrong-port-flag ROM on its own row (same pin as the baseline, so its
 * text reaches the same comparator) fails with the oracle's wrong-rom
 * transcript under the baseline script. */
static int case_same_setup_wrong_rom(void) {
  char rom[PATH_MAX];
  char wrong[PATH_MAX];
  char script[PATH_MAX];
  poll_measure measure;
  input_row rows[3];
  run_spec spec;
  run_out run;
  CHECK(fixture_path(rom, BASELINE_ROM));
  CHECK(fixture_path(wrong, WRONG_ROM));
  CHECK(accepted_baseline(rom, &measure, rows, &spec, script));
  /* The variant has one more instruction per poll, so its polls start a few
     ticks later than the baseline's. Its inputs are stamped at E1 - 1, E2 and
     E3 + 1 of its own polls, which is the oracle's wrong-rom scenario. */
  CHECK(measure_polls(wrong, &measure));
  CHECK(measure.start_count == POLL_COUNT);
  if (measure.start_count != POLL_COUNT) return 1;
  rows[0].tick = measure.starts[0] + HANDSHAKE_TICKS - 1u;
  rows[1].tick = measure.starts[1] + HANDSHAKE_TICKS;
  rows[2].tick = measure.starts[2] + HANDSHAKE_TICKS + 1u;
  CHECK(write_input_script(script, "pad-wrong-rom.input.json", rows, 3u));
  spec.fixture = wrong;
  spec.input = script;
  spec.retain = true;
  CHECK(run_supervised(&spec, &run));
  expect_wrong_transcript(&run);
  CHECK(strcmp(run.fixture_id, WRONG_ROM) == 0);
  CHECK(strcmp(run.log_sha256, WRONG_ROM_SHA256) == 0);
  CHECK(strcmp(run.log_sha256, BASELINE_SHA256) != 0);
  CHECK(capture_equals(&run, wrong_rom_transcript,
                       sizeof wrong_rom_transcript - 1u));
  report_control("same-setup-wrong-rom", &run);
  return g_failures == 0 ? 0 : 1;
}

static bool write_altered_copy(const char *source, const char *destination) {
  uint8_t bytes[8192];
  size_t length = 0u;
  size_t offset = 0u;
  int fd;
  if (!read_file(source, bytes, sizeof bytes, &length) || length == 0u ||
      length >= sizeof bytes) {
    return false;
  }
  bytes[length - 1u] ^= UINT8_C(1);
  fd = open(destination, O_WRONLY | O_CREAT | O_TRUNC, 0600);
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

/* A one-byte altered copy of the baseline ROM is refused by the identity
 * check before the guest starts. */
static int case_replacement_fixture_refused(void) {
  char rom[PATH_MAX];
  char altered[PATH_MAX];
  char script[PATH_MAX];
  poll_measure measure;
  input_row rows[3];
  run_spec spec;
  run_out run;
  CHECK(fixture_path(rom, BASELINE_ROM));
  CHECK(accepted_baseline(rom, &measure, rows, &spec, script));
  CHECK(snprintf(altered, sizeof altered, "%s/pad-poll-altered.z64",
                 g_evidence) < (int)sizeof altered);
  CHECK(write_altered_copy(rom, altered));
  spec.fixture = altered;
  spec.retain = true;
  CHECK(run_supervised(&spec, &run));
  CHECK(run.parsed);
  CHECK(strcmp(run.outcome, "harness-error") == 0);
  CHECK(strcmp(run.detail, "refusal") == 0);
  CHECK(strcmp(run.stop_kind, "refused") == 0);
  CHECK(run.guest_tick == 0u);
  CHECK(run.fixture_id[0] == '\0');
  report_control("replacement-fixture-refused", &run);
  return g_failures == 0 ? 0 : 1;
}

/* ---- dispatch --------------------------------------------------------------- */

typedef struct {
  const char *name;
  int (*run)(void);
  const char *suite;
} case_entry;

static const case_entry cases[] = {
    {"poll-timing", case_poll_timing, "accepted"},
    {"accepted-poll-sequence", case_accepted_poll_sequence, "accepted"},
    {"same-setup-wrong-script", case_same_setup_wrong_script, "controls"},
    {"same-setup-wrong-rom", case_same_setup_wrong_rom, "controls"},
    {"replacement-fixture-refused", case_replacement_fixture_refused,
     "controls"},
};

int main(int argc, char **argv) {
  size_t i;
  size_t base;
  bool suite;
  int status = 0;
  bool matched = false;
  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
      (void)puts(cases[i].name);
    }
    return 0;
  }
  suite = argc == 7 && strcmp(argv[1], "--suite") == 0;
  if (!suite && argc != 6) {
    (void)fprintf(stderr,
                  "usage: %s CASE|--suite NAME SUPERVISOR TEST_SUPERVISOR "
                  "CHECKER ROOT\n",
                  argv[0]);
    return 2;
  }
  base = suite ? 3u : 2u;
  g_supervisor = argv[base];
  g_checker = argv[base + 2u];
  g_root = argv[base + 3u];
  if (!prepare_evidence()) {
    (void)fprintf(stderr, "evidence directory unavailable\n");
    return 2;
  }
  for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
    const char *name = suite ? argv[2] : argv[1];
    if (suite ? strcmp(cases[i].suite, name) == 0
              : strcmp(cases[i].name, name) == 0) {
      int case_status;
      matched = true;
      g_failures = 0;
      case_status = cases[i].run();
      (void)fprintf(stderr, "case %s: %s\n", cases[i].name,
                    case_status == 0 ? "ok" : "FAILED");
      if (case_status != 0) status = 1;
    }
  }
  if (!matched) {
    (void)fprintf(stderr, "unknown case or suite: %s\n",
                  suite ? argv[2] : argv[1]);
    return 2;
  }
  return status;
}
