/*
 * Phase 9 runner mechanism controls: per-fixture tick cap, transcript pins,
 * bounded supervisor argv, the admit ROMs, the artifact pair and the table
 * checks. Every case drives the real supervisor and runner binaries or the
 * production fixture table; nothing here is a hardware or timing claim.
 *
 * usage: test_phase09_runner CASE SUPERVISOR TEST_SUPERVISOR CHECKER ROOT
 *        test_phase09_runner --list-cases
 *        test_phase09_runner --print-profile-hashes
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
#include "t64_real_fixture.h"
#include "t64_sha256.h"
#include "tendies64/t64_machine.h"
#include "t64_test_tree.h"

#define LEGACY_HELLO_PROFILE "ntsc-cart-4m-openipl3-warm-v1"

/* The four composite family ids registered by 09-01 (final labels). */
#define PROFILE_STEM "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1"
#define PROFILE_E4K PROFILE_STEM "-e4k"
#define PROFILE_E16K PROFILE_STEM "-e16k"
#define PROFILE_S32K PROFILE_STEM "-s32k"

static const char *const family_ids[4] = {PROFILE_STEM, PROFILE_E4K,
                                          PROFILE_E16K, PROFILE_S32K};

static const char *g_supervisor;
static const char *g_test_supervisor;
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
    return stat(g_evidence, &info) == 0 && S_ISDIR(info.st_mode) &&
           t64_test_scope_open(g_evidence, sizeof g_evidence, false);
  }
  if (temp == NULL || temp[0] == '\0') temp = "/tmp";
  if (snprintf(g_evidence, sizeof g_evidence, "%s/t64-phase09.XXXXXX",
               temp) >= (int)sizeof g_evidence) {
    return false;
  }
  return mkdtemp(g_evidence) != NULL &&
         t64_test_scope_open(g_evidence, sizeof g_evidence, true);
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
  uint64_t bound;         /* 0 leaves the option out */
  uint64_t advance_step;  /* 0 leaves the option out (test twin only) */
  int video_fd;           /* 0 leaves the option out */
  int audio_fd;           /* 0 leaves the option out */
  const char *test_fault; /* NULL leaves the option out (test twin only) */
  const char *artifact_dir; /* NULL leaves the pair out */
  uint64_t artifact_max;
  const char *meta_path;  /* NULL uses the run directory's meta.json */
} run_spec;

#define MAX_ARGS 48u

typedef struct {
  char *items[MAX_ARGS];
  size_t count;
} arg_list;

static void arg_add(arg_list *list, char *value) {
  if (list->count < MAX_ARGS - 1u) list->items[list->count++] = value;
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
  pid_t pid;
  int status = 0;
  memset(out, 0, sizeof *out);
  if (!make_run_dir(out->dir) || !input_path(input) ||
      !join_path(result_path, sizeof result_path, out->dir, "result.json") ||
      !join_path(meta_path, sizeof meta_path, out->dir, "meta.json") ||
      !join_path(capture_path, sizeof capture_path, out->dir, "capture.bin")) {
    return false;
  }
  (void)snprintf(out->result_path, sizeof out->result_path, "%s",
                 result_path);
  (void)snprintf(bound_text, sizeof bound_text, "%llu",
                 (unsigned long long)spec->bound);
  (void)snprintf(advance_text, sizeof advance_text, "%llu",
                 (unsigned long long)spec->advance_step);
  (void)snprintf(video_text, sizeof video_text, "%d", spec->video_fd);
  (void)snprintf(audio_text, sizeof audio_text, "%d", spec->audio_fd);
  (void)snprintf(artifact_max_text, sizeof artifact_max_text, "%llu",
                 (unsigned long long)spec->artifact_max);
  pid = fork();
  if (pid < 0) return false;
  if (pid == 0) {
    arg_list list = {{NULL}, 0u};
    char real_option[] = "--real";
    char result_option[] = "--result";
    char meta_option[] = "--meta";
    char work_option[] = "--work-dir";
    char wall_option[] = "--wall-ms";
    char wall_value[] = "30000";
    char bound_option[] = "--bound";
    char fixture_option[] = "--fixture";
    char input_option[] = "--input";
    char capture_option[] = "--capture-out";
    char advance_option[] = "--advance-step";
    char video_option[] = "--video-observation-fd";
    char audio_option[] = "--audio-observation-fd";
    char fault_option[] = "--test-fault";
    char artifact_dir_option[] = "--artifact-dir";
    char artifact_max_option[] = "--artifact-max-bytes";
    arg_add(&list, (char *)(uintptr_t)spec->supervisor);
    arg_add(&list, real_option);
    arg_add(&list, result_option);
    arg_add(&list, result_path);
    arg_add(&list, meta_option);
    arg_add(&list, spec->meta_path != NULL ? (char *)(uintptr_t)spec->meta_path
                                            : meta_path);
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
    execv(spec->supervisor, list.items);
    _exit(127);
  }
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) return false;
  }
  out->supervisor_exit = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  out->parsed = parse_record(result_path, out);
  out->checker_ok = out->parsed && checker_accepts(result_path);
  return out->parsed;
}

/* The runner consumes verified inherited snapshots: read-only descriptors on
 * unlinked regular files. This builds one from a source file. */
static int open_snapshot(const char *source, const char *directory) {
  uint8_t bytes[8192];
  size_t length = 0u;
  char name[PATH_MAX];
  size_t written = 0u;
  int fd;
  if (!read_file(source, bytes, sizeof bytes, &length) || length == 0u ||
      length >= sizeof bytes || !join_path(name, sizeof name, directory,
                                           "snapshot.XXXXXX")) {
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

/* The runner binary runs directly with inherited descriptors. */
static bool run_runner_direct(const char *fixture, const char *profile,
                              const char *ticks_text,
                              const char *artifact_dir,
                              const char *artifact_max_text, run_out *out) {
  char runner[PATH_MAX];
  char input[PATH_MAX];
  char stdout_path[PATH_MAX];
  char fixture_fd_text[24];
  char input_fd_text[24];
  char *slash;
  int fixture_fd;
  int input_fd;
  pid_t pid;
  int status = 0;
  memset(out, 0, sizeof *out);
  if (snprintf(runner, sizeof runner, "%s", g_supervisor) >=
      (int)sizeof runner) {
    return false;
  }
  slash = strrchr(runner, '/');
  if (slash == NULL ||
      (size_t)(slash - runner) + sizeof "/t64-runner-real" > sizeof runner) {
    return false;
  }
  (void)snprintf(slash, sizeof runner - (size_t)(slash - runner),
                 "/t64-runner-real");
  if (!make_run_dir(out->dir) || !input_path(input) ||
      !join_path(stdout_path, sizeof stdout_path, out->dir, "runner.json")) {
    return false;
  }
  fixture_fd = open_snapshot(fixture, out->dir);
  input_fd = open_snapshot(input, out->dir);
  if (fixture_fd <= STDERR_FILENO || input_fd <= STDERR_FILENO) return false;
  (void)snprintf(fixture_fd_text, sizeof fixture_fd_text, "%d", fixture_fd);
  (void)snprintf(input_fd_text, sizeof input_fd_text, "%d", input_fd);
  pid = fork();
  if (pid < 0) return false;
  if (pid == 0) {
    char fixture_option[] = "--fixture-fd";
    char input_option[] = "--input-fd";
    char profile_option[] = "--profile";
    char ticks_option[] = "--guest-ticks";
    char artifact_dir_option[] = "--artifact-dir";
    char artifact_max_option[] = "--artifact-max-bytes";
    char *args[14];
    size_t count = 0u;
    int out_fd = open(stdout_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    int null_fd = open("/dev/null", O_WRONLY);
    if (out_fd < 0 || dup2(out_fd, STDOUT_FILENO) < 0) _exit(126);
    if (null_fd >= 0) (void)dup2(null_fd, STDERR_FILENO);
    args[count++] = runner;
    args[count++] = fixture_option;
    args[count++] = fixture_fd_text;
    args[count++] = input_option;
    args[count++] = input_fd_text;
    args[count++] = profile_option;
    args[count++] = (char *)(uintptr_t)profile;
    args[count++] = ticks_option;
    args[count++] = (char *)(uintptr_t)ticks_text;
    if (artifact_dir != NULL) {
      args[count++] = artifact_dir_option;
      args[count++] = (char *)(uintptr_t)artifact_dir;
      args[count++] = artifact_max_option;
      args[count++] = (char *)(uintptr_t)artifact_max_text;
    }
    args[count] = NULL;
    execv(runner, args);
    _exit(127);
  }
  (void)close(fixture_fd);
  (void)close(input_fd);
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) return false;
  }
  out->supervisor_exit = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  (void)snprintf(out->result_path, sizeof out->result_path, "%s", stdout_path);
  out->parsed = parse_record(stdout_path, out);
  return out->parsed;
}

/* ---- artifact directories ----------------------------------------------- */

static bool make_artifact_dir(char out[PATH_MAX], mode_t mode) {
  if (!make_run_dir(out)) return false;
  return chmod(out, mode) == 0;
}

/* Number of directory entries, and the first name found. */
static size_t list_directory(const char *path, char first[256]) {
  DIR *dir = opendir(path);
  struct dirent *entry;
  size_t count = 0u;
  first[0] = '\0';
  if (dir == NULL) return (size_t)-1;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    if (count == 0u) (void)snprintf(first, 256u, "%s", entry->d_name);
    count++;
  }
  (void)closedir(dir);
  return count;
}

/* The probe pattern is derived here from its definition: byte i equals
 * (i * 7 + 3) modulo 256. */
static bool file_matches_pattern(const char *path, uint64_t size) {
  uint8_t chunk[4096];
  uint64_t offset = 0u;
  int fd = open(path, O_RDONLY);
  struct stat info;
  if (fd < 0) return false;
  if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) ||
      (uint64_t)info.st_size != size || info.st_nlink != 1 ||
      (info.st_mode & 0777) != 0600) {
    (void)close(fd);
    return false;
  }
  while (offset < size) {
    ssize_t count = read(fd, chunk, sizeof chunk);
    ssize_t i;
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) {
      (void)close(fd);
      return false;
    }
    for (i = 0; i < count; i++) {
      if (chunk[i] != (uint8_t)(((offset + (uint64_t)i) * 7u + 3u) % 256u)) {
        (void)close(fd);
        return false;
      }
    }
    offset += (uint64_t)count;
  }
  (void)close(fd);
  return true;
}

/* ---- cases -------------------------------------------------------------- */

static void expect_pass(const run_out *run) {
  CHECK(run->parsed);
  CHECK(run->checker_ok);
  CHECK(run->supervisor_exit == 0);
  CHECK(strcmp(run->outcome, "pass") == 0);
  CHECK(strcmp(run->detail, "positive-match") == 0);
  CHECK(strcmp(run->stop_kind, "authored-completion") == 0);
}

static void expect_refusal(const run_out *run) {
  CHECK(run->parsed);
  CHECK(strcmp(run->outcome, "harness-error") == 0);
  CHECK(strcmp(run->detail, "refusal") == 0);
  CHECK(strcmp(run->stop_kind, "refused") == 0);
  CHECK(run->guest_tick == 0u);
}

static void expect_usage(const run_out *run) {
  CHECK(run->parsed);
  CHECK(strcmp(run->outcome, "harness-error") == 0);
  CHECK(strcmp(run->detail, "usage") == 0);
  CHECK(strcmp(run->stop_kind, "not-started") == 0);
  CHECK(run->guest_tick == 0u);
}

/* K3: a bound above the default cap is refused after fixture identification;
 * a bound above the ceiling is refused at parse in both binaries. */
static int case_tick_cap(void) {
  char hello[PATH_MAX];
  run_spec spec = {0};
  run_out run;
  if (!fixture_path(hello, "hello")) return 2;

  /* Accepted baseline in the same setup: the legacy default bound passes. */
  spec.supervisor = g_supervisor;
  spec.fixture = hello;
  spec.bound = 600000u;
  CHECK(run_supervised(&spec, &run));
  expect_pass(&run);
  CHECK(run.bound_tick == 600000u);
  CHECK(strcmp(run.fixture_id, "hello-openipl3") == 0);

  /* One tick above the default cap on a default-cap row: refusal. */
  spec.bound = 600001u;
  CHECK(run_supervised(&spec, &run));
  expect_refusal(&run);
  CHECK(run.supervisor_exit != 0);

  /* Above the ceiling: parse refusal (usage class). */
  spec.bound = 4000001u;
  CHECK(run_supervised(&spec, &run));
  expect_usage(&run);

  /* The runner binary applies the same two checks on its own, after its own
   * accepted baseline in the same setup. */
  CHECK(run_runner_direct(hello, LEGACY_HELLO_PROFILE, "600000", NULL, NULL,
                          &run));
  CHECK(run.parsed);
  CHECK(strcmp(run.outcome, "pass") == 0);
  CHECK(run_runner_direct(hello, LEGACY_HELLO_PROFILE, "600001", NULL, NULL,
                          &run));
  expect_refusal(&run);
  CHECK(run_runner_direct(hello, LEGACY_HELLO_PROFILE, "4000001", NULL, NULL,
                          &run));
  expect_usage(&run);

  /* The named long row accepts a bound above the default cap and up to the
   * ceiling; the stem row, with the same bound, is refused. The completion
   * tick is asserted as a range, never as an exact number. */
  {
    char long_rom[PATH_MAX];
    char stem_rom[PATH_MAX];
    CHECK(fixture_path(long_rom, "phase09-admit-long-si-v1"));
    CHECK(fixture_path(stem_rom, "phase09-admit-si-v1"));
    spec.fixture = stem_rom;
    spec.bound = 600000u;
    CHECK(run_supervised(&spec, &run));
    expect_pass(&run);
    spec.bound = 1200000u;
    CHECK(run_supervised(&spec, &run));
    expect_refusal(&run);
    spec.fixture = long_rom;
    CHECK(run_supervised(&spec, &run));
    expect_pass(&run);
    CHECK(strcmp(run.fixture_id, "phase09-admit-long-si-v1") == 0);
    CHECK(run.guest_tick > 600000u);
    CHECK(run.guest_tick < 1500000u);
    CHECK(strcmp(run.completion_pc, "ffffffff80000484") == 0);
    (void)fprintf(stderr, "long row completion tick: %llu\n",
                  (unsigned long long)run.guest_tick);
    spec.bound = 4000000u;
    CHECK(run_supervised(&spec, &run));
    expect_pass(&run);
    CHECK(run.bound_tick == 4000000u);
    spec.bound = 4000001u;
    CHECK(run_supervised(&spec, &run));
    expect_usage(&run);
    spec.fixture = stem_rom;
    CHECK(run_supervised(&spec, &run));
    expect_usage(&run);
  }
  return g_failures == 0 ? 0 : 1;
}

static void expect_wrong_transcript(const run_out *run) {
  CHECK(run->parsed);
  CHECK(run->checker_ok);
  CHECK(strcmp(run->outcome, "guest-fail") == 0);
  CHECK(strcmp(run->detail, "wrong-transcript") == 0);
  CHECK(strcmp(run->stop_kind, "authored-completion") == 0);
}

/* K4: every legacy row is judged by the legacy pin exactly as before the
 * per-row pin existed. No pinned row exists yet when this case is first
 * written, so it starts green by design; it is the characterization that the
 * pin accessor did not move legacy behavior. */
/* Drift control (plan 09-10, L3 WR-03): the pass-rule sentence of
 * docs/spec/result-protocol.md names the pins of the fixture table, so it
 * cannot go back to describing the one 30-byte hello pin while the table holds
 * per-row pins. The control derives from the table how many rows carry their
 * own pin and the distinct pinned lengths, composes the two fragments the spec
 * must contain, and requires the legacy pin to stay named. */
static void check_pass_rule_spec(void) {
  static uint8_t spec[1u << 17];
  char path[PATH_MAX];
  char count_fragment[96];
  char length_fragment[192];
  size_t length = 0u;
  size_t pinned = 0u;
  size_t lengths[32];
  size_t distinct = 0u;
  size_t i;
  size_t k;
  size_t used;
  CHECK(join_path(path, sizeof path, g_root, "docs/spec/result-protocol.md"));
  CHECK(read_file(path, spec, sizeof spec - 1u, &length) && length != 0u);
  spec[length] = '\0';
  for (i = 0u; i < t64_real_fixture_count(); i++) {
    const t64_real_fixture *row = t64_real_fixture_at(i);
    size_t at = 0u;
    if (row == NULL || row->expected_log_sha256 == NULL) continue;
    pinned++;
    while (at < distinct && lengths[at] < row->expected_log_bytes) at++;
    if (at < distinct && lengths[at] == row->expected_log_bytes) continue;
    CHECK(distinct < sizeof lengths / sizeof lengths[0]);
    if (distinct >= sizeof lengths / sizeof lengths[0]) return;
    for (k = distinct; k > at; k--) lengths[k] = lengths[k - 1u];
    lengths[at] = row->expected_log_bytes;
    distinct++;
  }
  CHECK(pinned != 0u && distinct != 0u);
  (void)snprintf(count_fragment, sizeof count_fragment,
                 "%zu rows carry their own pinned length and SHA-256", pinned);
  used = (size_t)snprintf(length_fragment, sizeof length_fragment, "%s", "");
  for (i = 0u; i < distinct && used < sizeof length_fragment; i++) {
    used += (size_t)snprintf(length_fragment + used,
                             sizeof length_fragment - used, "%s%zu",
                             i == 0u ? "" : (i + 1u == distinct ? " and " : ", "),
                             lengths[i]);
  }
  if (used < sizeof length_fragment) {
    (void)snprintf(length_fragment + used, sizeof length_fragment - used,
                   " bytes");
  }
  if (strstr((const char *)spec, count_fragment) == NULL ||
      strstr((const char *)spec, length_fragment) == NULL) {
    (void)fprintf(stderr,
                  "result-protocol.md pass rule does not name the table: need "
                  "\"%s\" and \"%s\"\n",
                  count_fragment, length_fragment);
  }
  CHECK(strstr((const char *)spec, count_fragment) != NULL);
  CHECK(strstr((const char *)spec, length_fragment) != NULL);
  /* the legacy pin of the rows without their own stays named */
  CHECK(strstr((const char *)spec,
               "86594505bea53c9843f9eaf07e4cf0121cbe0055670324674fd8385c88ffd18e") !=
        NULL);
}

/* Drift control (plan 09-10, re-check R2 RC-02): the `detail` vocabulary of
 * docs/spec/result-protocol.md is closed, so every name the checker accepts in
 * its three tables of runner details (the host and process details, the artifact
 * publisher details and the state export details) must be named in the spec in
 * backticks. The raw capture names are specified with the raw capture. */
static void check_detail_vocabulary_spec(void) {
  static const char *const tables[] = {"real_detail_names",
                                       "artifact_detail_names",
                                       "state_export_detail_names"};
  static uint8_t spec[1u << 17];
  static uint8_t source[1u << 18];
  char path[PATH_MAX];
  size_t spec_length = 0u;
  size_t source_length = 0u;
  size_t table;
  size_t names_seen = 0u;
  CHECK(join_path(path, sizeof path, g_root, "docs/spec/result-protocol.md"));
  CHECK(read_file(path, spec, sizeof spec - 1u, &spec_length) &&
        spec_length != 0u);
  spec[spec_length] = '\0';
  CHECK(join_path(path, sizeof path, g_root, "runner/t64_result_check.c"));
  CHECK(read_file(path, source, sizeof source - 1u, &source_length) &&
        source_length != 0u);
  source[source_length] = '\0';
  for (table = 0u; table < sizeof tables / sizeof tables[0]; table++) {
    char header[96];
    const char *start;
    const char *end;
    const char *cursor;
    (void)snprintf(header, sizeof header, "static const char *const %s[] = {",
                   tables[table]);
    start = strstr((const char *)source, header);
    CHECK(start != NULL);
    if (start == NULL) continue;
    start += strlen(header);
    end = strstr(start, "};");
    CHECK(end != NULL);
    if (end == NULL) continue;
    for (cursor = start; cursor < end; cursor++) {
      char name[96];
      char wanted[100];
      const char *close;
      size_t name_length;
      if (*cursor != '"') continue;
      close = memchr(cursor + 1, '"', (size_t)(end - cursor - 1));
      CHECK(close != NULL);
      if (close == NULL) break;
      name_length = (size_t)(close - cursor - 1);
      CHECK(name_length != 0u && name_length < sizeof name);
      if (name_length == 0u || name_length >= sizeof name) break;
      (void)memcpy(name, cursor + 1, name_length);
      name[name_length] = '\0';
      (void)snprintf(wanted, sizeof wanted, "`%s`", name);
      names_seen++;
      if (strstr((const char *)spec, wanted) == NULL) {
        (void)fprintf(stderr,
                      "result-protocol.md does not name the detail %s of %s\n",
                      name, tables[table]);
        g_failures++;
      }
      cursor = close;
    }
  }
  /* the three tables hold 31, 9 and 6 names today; a parse that stopped
   * finding them would otherwise pass vacuously */
  CHECK(names_seen >= 40u);
}

static int case_transcript_pin(void) {
  char path[PATH_MAX];
  run_spec spec = {0};
  run_out run;
  spec.supervisor = g_supervisor;
  spec.fixture = path;
  spec.bound = 600000u;
  check_pass_rule_spec();
  check_detail_vocabulary_spec();

  CHECK(fixture_path(path, "hello"));
  CHECK(run_supervised(&spec, &run));
  expect_pass(&run);
  CHECK(run.log_bytes == T64_REAL_LEGACY_LOG_BYTES);
  CHECK(strcmp(run.log_sha256,
               "86594505bea53c9843f9eaf07e4cf0121cbe0055670324674fd8385c88ffd18e") == 0);

  CHECK(fixture_path(path, "hello-wrong-text"));
  CHECK(run_supervised(&spec, &run));
  expect_wrong_transcript(&run);
  CHECK(strcmp(run.fixture_id, "hello-wrong-text") == 0);

  CHECK(fixture_path(path, "audio-pcm"));
  CHECK(run_supervised(&spec, &run));
  expect_pass(&run);
  CHECK(strcmp(run.fixture_id, "audio-pcm-literal") == 0);
  CHECK(run.log_bytes == T64_REAL_LEGACY_LOG_BYTES);
  return g_failures == 0 ? 0 : 1;
}

/* ---- option-pair matrix (plan 09-10, FX-12; R-01 to R-04, VG-07, T-09-82) ---- */

/* Nine option families (docs/spec/state-container.md, Option-pair matrix) and
 * every pair of them. A pair is refused as usage at guest tick 0, before any
 * snapshot, work directory or child, or it is accepted by design; no pair is
 * undefined. Each family is complete in itself. The three families that import
 * a file (SAVE-IN, REPLAY-PLAY and STATE-IN) carry a path that does not exist,
 * so a supervisor that prepared a snapshot before the pair rule would report
 * the snapshot refusal (detail refusal) instead of the pair refusal (detail
 * usage) for every refused pair that holds one of them; for the other pairs the
 * order "pair rule before snapshot" has no observable effect and this test does
 * not pin it. The accepted baseline of every row is the family alone, which
 * must not be a usage refusal. @ART and @RAW stand for the private directories
 * of the run. */
static bool sha256_file_hex(const char *path, char out[65]);

#define PAIR_H64 \
  "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"

typedef enum {
  FAM_SAVE_IN,
  FAM_SAVE_OUT,
  FAM_REPLAY_REC,
  FAM_REPLAY_PLAY,
  FAM_STATE_OUT,
  FAM_STATE_IN,
  FAM_RAW,
  FAM_INPUT,
  FAM_PAIR,
  FAM_COUNT
} option_family;

typedef struct {
  const char *name;
  const char *args[14]; /* option and value strings, NULL terminated */
  bool needs_pair;
} family_def;

static const family_def families[FAM_COUNT] = {
    {"SAVE-IN",
     {"--save-import", "/nonexistent/save.bin", "--save-import-size", "512",
      "--save-import-sha256", PAIR_H64, "--save-kind", "eeprom4k",
      "--save-format", "raw-v1", NULL},
     false},
    {"SAVE-OUT",
     {"--save-kind", "eeprom4k", "--save-format", "raw-v1",
      "--save-export-leaf", "s.eep", "--artifact-dir", "@ART",
      "--artifact-max-bytes", "4096", NULL},
     true},
    {"REPLAY-REC",
     {"--replay-record", "r.rpl", "--replay-checkpoint-interval", "20000",
      "--artifact-dir", "@ART", "--artifact-max-bytes", "4096", NULL},
     true},
    {"REPLAY-PLAY",
     {"--replay-play", "/nonexistent/r.rpl", "--replay-play-size", "100",
      "--replay-play-sha256", PAIR_H64, "--replay-report", "r.rpr",
      "--artifact-dir", "@ART", "--artifact-max-bytes", "4096", NULL},
     true},
    {"STATE-OUT",
     {"--state-export", "s.sta", "--state-export-format",
      "t64.state-container.v1", "--artifact-dir", "@ART",
      "--artifact-max-bytes", "4096", NULL},
     true},
    {"STATE-IN",
     {"--state-import", "/nonexistent/state.sta", "--state-import-size", "100",
      "--state-import-sha256", PAIR_H64, NULL},
     false},
    {"RAW", {"--raw-capture-dir", "@RAW", NULL}, false},
    {"INPUT", {"--input-size", "100", "--input-sha256", PAIR_H64, NULL}, false},
    {"PAIR",
     {"--artifact-dir", "@ART", "--artifact-max-bytes", "4096", NULL},
     false},
};

typedef struct {
  option_family first;
  option_family second;
  bool refused; /* true: usage; false: accepted by the pair rule */
} pair_row;

/* All 36 unordered pairs. The six INPUT combinations (SAVE-IN, SAVE-OUT,
 * REPLAY-REC, REPLAY-PLAY and RAW with INPUT, and INPUT with PAIR) are accepted
 * by design: the custom schedule options are not an artifact producer. */
static const pair_row pair_rows[] = {
    {FAM_SAVE_IN, FAM_SAVE_OUT, false},
    {FAM_SAVE_IN, FAM_REPLAY_REC, true},
    {FAM_SAVE_IN, FAM_REPLAY_PLAY, true},
    {FAM_SAVE_IN, FAM_STATE_OUT, true},
    {FAM_SAVE_IN, FAM_STATE_IN, true},
    {FAM_SAVE_IN, FAM_RAW, true},
    {FAM_SAVE_IN, FAM_INPUT, false},
    {FAM_SAVE_IN, FAM_PAIR, false},
    {FAM_SAVE_OUT, FAM_REPLAY_REC, true},
    {FAM_SAVE_OUT, FAM_REPLAY_PLAY, true},
    {FAM_SAVE_OUT, FAM_STATE_OUT, true},
    {FAM_SAVE_OUT, FAM_STATE_IN, true},
    {FAM_SAVE_OUT, FAM_RAW, true},
    {FAM_SAVE_OUT, FAM_INPUT, false},
    {FAM_SAVE_OUT, FAM_PAIR, false},
    {FAM_REPLAY_REC, FAM_REPLAY_PLAY, true},
    {FAM_REPLAY_REC, FAM_STATE_OUT, true},
    {FAM_REPLAY_REC, FAM_STATE_IN, true},
    {FAM_REPLAY_REC, FAM_RAW, true},
    {FAM_REPLAY_REC, FAM_INPUT, false},
    {FAM_REPLAY_REC, FAM_PAIR, false},
    {FAM_REPLAY_PLAY, FAM_STATE_OUT, true},
    {FAM_REPLAY_PLAY, FAM_STATE_IN, true},
    {FAM_REPLAY_PLAY, FAM_RAW, true},
    {FAM_REPLAY_PLAY, FAM_INPUT, false},
    {FAM_REPLAY_PLAY, FAM_PAIR, false},
    {FAM_STATE_OUT, FAM_STATE_IN, true},
    {FAM_STATE_OUT, FAM_RAW, true},
    {FAM_STATE_OUT, FAM_INPUT, false},
    {FAM_STATE_OUT, FAM_PAIR, false},
    {FAM_STATE_IN, FAM_RAW, true},
    {FAM_STATE_IN, FAM_INPUT, true},
    {FAM_STATE_IN, FAM_PAIR, false},
    {FAM_RAW, FAM_INPUT, false},
    {FAM_RAW, FAM_PAIR, true},
    {FAM_INPUT, FAM_PAIR, false},
};

/* The 36 unordered pairs of nine families: a dropped or added row fails the
 * build (plan 09-10, re-check R2 RC-07). */
_Static_assert(sizeof pair_rows / sizeof pair_rows[0] ==
                   (size_t)FAM_COUNT * ((size_t)FAM_COUNT - 1u) / 2u,
               "pair_rows must hold every unordered pair of the families once");

/* Uniqueness, order and the spec: each unordered pair appears exactly once with
 * the lower family first, and the 9 by 9 table of docs/spec/state-container.md
 * (R refused, A accepted, a dot the family with itself) equals the rows, in both
 * triangles. The table and the code were compared by hand before. */
static void check_pair_table_against_spec(void) {
  static uint8_t spec[1u << 17];
  char path[PATH_MAX];
  size_t length = 0u;
  bool seen[FAM_COUNT][FAM_COUNT];
  bool refused[FAM_COUNT][FAM_COUNT];
  const char *cursor;
  size_t i;
  size_t j;
  size_t checked = 0u;
  memset(seen, 0, sizeof seen);
  memset(refused, 0, sizeof refused);
  for (i = 0u; i < sizeof pair_rows / sizeof pair_rows[0]; i++) {
    const pair_row *row = &pair_rows[i];
    CHECK((int)row->first < (int)row->second);
    CHECK(!seen[row->first][row->second]);
    seen[row->first][row->second] = true;
    refused[row->first][row->second] = row->refused;
    refused[row->second][row->first] = row->refused;
  }
  for (i = 0u; i < FAM_COUNT; i++) {
    for (j = i + 1u; j < FAM_COUNT; j++) CHECK(seen[i][j]);
  }
  CHECK(join_path(path, sizeof path, g_root, "docs/spec/state-container.md"));
  CHECK(read_file(path, spec, sizeof spec - 1u, &length) && length != 0u);
  spec[length] = '\0';
  cursor = strstr((const char *)spec,
                  "| | SAVE-IN | SAVE-OUT | REPLAY-REC | REPLAY-PLAY | STATE-OUT "
                  "| STATE-IN | RAW | INPUT | PAIR |");
  CHECK(cursor != NULL);
  if (cursor == NULL) return;
  cursor = strchr(cursor, '\n');   /* header */
  cursor = cursor != NULL ? strchr(cursor + 1, '\n') : NULL; /* separator */
  for (i = 0u; i < FAM_COUNT && cursor != NULL; i++) {
    char line[256];
    const char *end;
    char *cell;
    char *save = NULL;
    size_t column = 0u;
    cursor++;
    end = strchr(cursor, '\n');
    CHECK(end != NULL && (size_t)(end - cursor) < sizeof line);
    if (end == NULL || (size_t)(end - cursor) >= sizeof line) return;
    (void)memcpy(line, cursor, (size_t)(end - cursor));
    line[end - cursor] = '\0';
    cursor = end;
    for (cell = strtok_r(line, "|", &save); cell != NULL;
         cell = strtok_r(NULL, "|", &save), column++) {
      char text[32];
      size_t used = 0u;
      while (*cell == ' ') cell++;
      while (cell[used] != '\0' && cell[used] != ' ' &&
             used + 1u < sizeof text) {
        text[used] = cell[used];
        used++;
      }
      text[used] = '\0';
      if (column == 0u) {
        CHECK(strcmp(text, families[i].name) == 0);
      } else if (column <= FAM_COUNT) {
        const size_t family = column - 1u;
        const char *want = family == i ? "." : (refused[i][family] ? "R" : "A");
        if (strcmp(text, want) != 0) {
          (void)fprintf(stderr, "spec table %s x %s says %s, the rows say %s\n",
                        families[i].name, families[family].name, text, want);
        }
        CHECK(strcmp(text, want) == 0);
        checked++;
      }
    }
    CHECK(column == (size_t)FAM_COUNT + 1u);
  }
  CHECK(checked == (size_t)FAM_COUNT * (size_t)FAM_COUNT);
}

#define PAIR_MAX_ARGS 40u

typedef struct {
  const char *items[PAIR_MAX_ARGS];
  size_t count;
} pair_args;

static bool pair_args_has(const pair_args *list, const char *option) {
  size_t i;
  for (i = 0u; i + 1u < list->count; i += 2u) {
    if (strcmp(list->items[i], option) == 0) return true;
  }
  return false;
}

/* Appends a family's options; an option already present (the shared artifact
 * pair) is not repeated. `skip_pair` leaves the artifact pair out. */
static void pair_args_add_family(pair_args *list, option_family family,
                                 const char *artifact, const char *raw,
                                 bool skip_pair) {
  const char *const *args = families[family].args;
  size_t i;
  for (i = 0u; args[i] != NULL; i += 2u) {
    const char *value = args[i + 1u];
    if (skip_pair && (strcmp(args[i], "--artifact-dir") == 0 ||
                      strcmp(args[i], "--artifact-max-bytes") == 0)) {
      continue;
    }
    if (pair_args_has(list, args[i]) || list->count + 2u > PAIR_MAX_ARGS) {
      continue;
    }
    if (strcmp(value, "@ART") == 0) value = artifact;
    if (strcmp(value, "@RAW") == 0) value = raw;
    list->items[list->count++] = args[i];
    list->items[list->count++] = value;
  }
}

/* One supervisor run in the real or the synthetic mode with the given extra
 * options; the record is read from the result path. */
static bool run_pair_probe(const pair_args *extra, bool synthetic,
                           run_out *out) {
  char result_path[PATH_MAX];
  char meta_path[PATH_MAX];
  char capture_path[PATH_MAX];
  char input[PATH_MAX];
  char fixture[PATH_MAX];
  char hello_sha[65];
  const char *true_program = access("/usr/bin/true", X_OK) == 0
                                 ? "/usr/bin/true"
                                 : "/bin/true";
  const char *argv_list[PAIR_MAX_ARGS + 40u];
  size_t count = 0u;
  size_t i;
  pid_t pid;
  int status = 0;
  memset(out, 0, sizeof *out);
  if (!make_run_dir(out->dir) || !input_path(input) ||
      !join_path(result_path, sizeof result_path, out->dir, "result.json") ||
      !join_path(meta_path, sizeof meta_path, out->dir, "meta.json") ||
      !join_path(capture_path, sizeof capture_path, out->dir, "capture.bin") ||
      !fixture_path(fixture, synthetic ? "hello" : "phase09-admit-si-v1")) {
    return false;
  }
  (void)snprintf(out->result_path, sizeof out->result_path, "%s",
                 result_path);
  argv_list[count++] = g_supervisor;
  if (synthetic) {
    if (!sha256_file_hex(fixture, hello_sha)) return false;
    argv_list[count++] = "--identity";
    argv_list[count++] = "synthetic";
    argv_list[count++] = "--result";
    argv_list[count++] = result_path;
    argv_list[count++] = "--meta";
    argv_list[count++] = meta_path;
    argv_list[count++] = "--work-dir";
    argv_list[count++] = out->dir;
    argv_list[count++] = "--wall-ms";
    argv_list[count++] = "5000";
    argv_list[count++] = "--bound";
    argv_list[count++] = "100";
    argv_list[count++] = "--log-limit";
    argv_list[count++] = "1024";
    argv_list[count++] = "--fixture-id";
    argv_list[count++] = "hello";
    argv_list[count++] = "--fixture";
    argv_list[count++] = fixture;
    argv_list[count++] = "--fixture-sha256";
    argv_list[count++] = hello_sha;
    argv_list[count++] = "--expect-pass";
    argv_list[count++] = "0";
  } else {
    argv_list[count++] = "--real";
    argv_list[count++] = "--result";
    argv_list[count++] = result_path;
    argv_list[count++] = "--meta";
    argv_list[count++] = meta_path;
    argv_list[count++] = "--work-dir";
    argv_list[count++] = out->dir;
    argv_list[count++] = "--wall-ms";
    argv_list[count++] = "30000";
    argv_list[count++] = "--bound";
    argv_list[count++] = "20000";
    argv_list[count++] = "--fixture";
    argv_list[count++] = fixture;
    argv_list[count++] = "--input";
    argv_list[count++] = input;
    argv_list[count++] = "--capture-out";
    argv_list[count++] = capture_path;
  }
  for (i = 0u; i < extra->count; i++) argv_list[count++] = extra->items[i];
  if (synthetic) {
    argv_list[count++] = "--";
    argv_list[count++] = true_program;
  }
  argv_list[count] = NULL;
  pid = fork();
  if (pid < 0) return false;
  if (pid == 0) {
    execv(g_supervisor, (char *const *)(uintptr_t)argv_list);
    _exit(127);
  }
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) return false;
  }
  out->supervisor_exit = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  out->parsed = parse_record(result_path, out);
  out->checker_ok = out->parsed && checker_accepts(result_path);
  return out->parsed;
}

/* The synthetic mode records the v1 result schema (no guest stop object), so
 * its usage refusal is read by outcome, detail and tick only. */
static bool is_synthetic_usage_record(const run_out *run) {
  return run->parsed && strcmp(run->outcome, "harness-error") == 0 &&
         strcmp(run->detail, "usage") == 0 && run->guest_tick == 0u;
}

static bool is_usage_record(const run_out *run) {
  return run->parsed && strcmp(run->outcome, "harness-error") == 0 &&
         strcmp(run->detail, "usage") == 0 &&
         strcmp(run->stop_kind, "not-started") == 0 && run->guest_tick == 0u;
}

/* The accepted side of a row: a record that is not the usage refusal, so the
 * arguments passed the supervisor's pair rules and reached the snapshot or the
 * child. */
static bool is_not_usage_record(const run_out *run) {
  return run->parsed && strcmp(run->detail, "usage") != 0;
}

static const char *const synthetic_flags[][2] = {
    {"--artifact-dir", "@ART"},
    {"--artifact-max-bytes", "4096"},
    {"--save-import", "/nonexistent/save.bin"},
    {"--save-import-size", "512"},
    {"--save-import-sha256", PAIR_H64},
    {"--save-kind", "eeprom4k"},
    {"--save-format", "raw-v1"},
    {"--save-export-leaf", "s.eep"},
    {"--replay-record", "r.rpl"},
    {"--replay-checkpoint-interval", "20000"},
    {"--replay-play", "/nonexistent/r.rpl"},
    {"--replay-play-size", "100"},
    {"--replay-play-sha256", PAIR_H64},
    {"--replay-report", "r.rpr"},
    {"--state-export", "s.sta"},
    {"--state-export-format", "t64.state-container.v1"},
    {"--state-import", "/nonexistent/state.sta"},
    {"--state-import-size", "100"},
    {"--state-import-sha256", PAIR_H64},
    /* the two Phase 8 families the same rule covers */
    {"--raw-capture-dir", "@RAW"},
    {"--input-size", "100"},
};

/* Fresh private directories for one row, so a row that runs the guest and
 * publishes (an accepted pair) cannot influence the next. */
static void fresh_pair_dirs(char artifact[PATH_MAX], char raw[PATH_MAX]) {
  CHECK(make_artifact_dir(artifact, 0700));
  CHECK(make_artifact_dir(raw, 0700));
}

static void option_pair_matrix(void) {
  char artifact[PATH_MAX];
  char raw[PATH_MAX];
  char first[256];
  run_out run;
  pair_args args;
  size_t i;

  check_pair_table_against_spec();
  /* The accepted baseline of every family: the family alone is not a usage
   * refusal (the pair rows below can start from it). */
  for (i = 0u; i < FAM_COUNT; i++) {
    fresh_pair_dirs(artifact, raw);
    memset(&args, 0, sizeof args);
    pair_args_add_family(&args, (option_family)i, artifact, raw, false);
    CHECK(run_pair_probe(&args, false, &run));
    if (!is_not_usage_record(&run)) {
      (void)fprintf(stderr, "family %s alone: detail %s\n", families[i].name,
                    run.detail);
    }
    CHECK(is_not_usage_record(&run));
  }

  /* Every pair: refused as usage at guest tick 0 with nothing created, or
   * accepted. */
  for (i = 0u; i < sizeof pair_rows / sizeof pair_rows[0]; i++) {
    const pair_row *row = &pair_rows[i];
    fresh_pair_dirs(artifact, raw);
    memset(&args, 0, sizeof args);
    pair_args_add_family(&args, row->first, artifact, raw, false);
    pair_args_add_family(&args, row->second, artifact, raw, false);
    CHECK(run_pair_probe(&args, false, &run));
    if (row->refused ? !is_usage_record(&run) : !is_not_usage_record(&run)) {
      (void)fprintf(stderr, "pair %s x %s: expected %s, detail %s\n",
                    families[row->first].name, families[row->second].name,
                    row->refused ? "usage" : "accepted", run.detail);
    }
    if (row->refused) {
      CHECK(is_usage_record(&run));
      CHECK(list_directory(artifact, first) == 0u);
      CHECK(list_directory(raw, first) == 0u);
    } else {
      CHECK(is_not_usage_record(&run));
    }
  }

  /* A producer that needs the artifact pair without it is refused as usage. */
  for (i = 0u; i < FAM_COUNT; i++) {
    if (!families[i].needs_pair) continue;
    fresh_pair_dirs(artifact, raw);
    memset(&args, 0, sizeof args);
    pair_args_add_family(&args, (option_family)i, artifact, raw, true);
    CHECK(run_pair_probe(&args, false, &run));
    if (!is_usage_record(&run)) {
      (void)fprintf(stderr, "family %s without the pair: detail %s\n",
                    families[i].name, run.detail);
    }
    CHECK(is_usage_record(&run));
  }

  /* Synthetic mode: the accepted baseline runs the stand-in child and ends in
   * its own record (no result), and every Phase 9 flag (19) plus the two
   * Phase 8 families the rule also covers is refused as usage. */
  fresh_pair_dirs(artifact, raw);
  memset(&args, 0, sizeof args);
  CHECK(run_pair_probe(&args, true, &run));
  CHECK(run.parsed);
  CHECK(strcmp(run.detail, "usage") != 0);
  CHECK(strcmp(run.detail, "no-result") == 0);
  for (i = 0u; i < sizeof synthetic_flags / sizeof synthetic_flags[0]; i++) {
    const char *value = synthetic_flags[i][1];
    memset(&args, 0, sizeof args);
    if (strcmp(value, "@ART") == 0) value = artifact;
    if (strcmp(value, "@RAW") == 0) value = raw;
    args.items[args.count++] = synthetic_flags[i][0];
    args.items[args.count++] = value;
    CHECK(run_pair_probe(&args, true, &run));
    if (!is_synthetic_usage_record(&run)) {
      (void)fprintf(stderr, "synthetic mode with %s: detail %s\n",
                    synthetic_flags[i][0], run.detail);
    }
    CHECK(is_synthetic_usage_record(&run));
  }
  CHECK(list_directory(artifact, first) == 0u);
  CHECK(list_directory(raw, first) == 0u);
}

/* ---- supervisor hardening rows (plan 09-10, FX-18; L3 IN-06, meta write) ---- */

static void supervisor_hardening_rows(void) {
  char rom[PATH_MAX];
  run_spec spec = {0};
  run_out run;
  CHECK(fixture_path(rom, "phase09-admit-si-v1"));

  /* A failed --meta write in real mode fails closed: the result record is
   * still the valid pass record, the exit status is not 0, so a caller never
   * takes a missing or stale run-meta for a complete run. The accepted
   * baseline is the same run with a writable meta path. */
  spec.fixture = rom;
  spec.bound = 600000u;
  spec.supervisor = g_supervisor;
  CHECK(run_supervised(&spec, &run));
  expect_pass(&run);
  spec.meta_path = "/nonexistent-t64-directory/meta.json";
  CHECK(run_supervised(&spec, &run));
  CHECK(run.parsed);
  CHECK(run.checker_ok);
  CHECK(strcmp(run.outcome, "pass") == 0);
  CHECK(run.supervisor_exit != 0);
}

/* K6: the maximum accepted option set runs to a pass through the supervisor
 * test twin; the capacity-exhausted fault ends the child before exec with a
 * harness error and no guest run. */
static int case_argv_bound(void) {
  char audio[PATH_MAX];
  char artifact[PATH_MAX];
  run_spec spec = {0};
  run_out run;
  int video_pipe[2] = {-1, -1};
  int audio_pipe[2] = {-1, -1};
  CHECK(fixture_path(audio, "audio-pcm"));
  CHECK(pipe(video_pipe) == 0);
  CHECK(pipe(audio_pipe) == 0);
  spec.supervisor = g_test_supervisor;
  spec.fixture = audio;
  spec.bound = 600000u;
  spec.advance_step = 16u;
  spec.video_fd = video_pipe[1];
  spec.audio_fd = audio_pipe[1];
  CHECK(make_artifact_dir(artifact, 0700));
  spec.artifact_dir = artifact;
  spec.artifact_max = 4096u;

  /* Accepted baseline with every option: advance step, video fd, audio fd and
   * the artifact pair. */
  CHECK(run_supervised(&spec, &run));
  expect_pass(&run);
  CHECK(strcmp(run.fixture_id, "audio-pcm-literal") == 0);
  CHECK(run.guest_tick != 0u);

  /* Same setup with the capacity-exhausted fault: no guest run. */
  spec.test_fault = "argv-capacity-exhausted";
  CHECK(run_supervised(&spec, &run));
  (void)fprintf(stderr, "argv-capacity-exhausted record: exit=%d outcome=%s "
                "detail=%s stop=%s guest_tick=%llu\n", run.supervisor_exit,
                run.outcome, run.detail, run.stop_kind,
                (unsigned long long)run.guest_tick);
  CHECK(strcmp(run.outcome, "harness-error") == 0);
  CHECK(strcmp(run.detail, "runner-exit-nonzero") == 0);
  CHECK(run.guest_tick == 0u);
  CHECK(run.supervisor_exit != 0);
  (void)close(video_pipe[0]);
  (void)close(video_pipe[1]);
  (void)close(audio_pipe[0]);
  (void)close(audio_pipe[1]);
  option_pair_matrix();
  supervisor_hardening_rows();
  return g_failures == 0 ? 0 : 1;
}

/* A runner prelaunch refusal reaches the supervisor as a record that does not
 * carry the identities the supervisor already holds, so the supervised result
 * is its post-launch classification: harness-error, report-error stop, detail
 * capture-invalid, guest tick 0 (no guest ran). The runner binary itself
 * reports the refusal record, asserted separately. */
static void expect_supervised_preflight_refusal(const run_out *run) {
  CHECK(run->parsed);
  CHECK(run->checker_ok);
  CHECK(strcmp(run->outcome, "harness-error") == 0);
  CHECK(strcmp(run->stop_kind, "report-error") == 0);
  CHECK(strcmp(run->detail, "capture-invalid") == 0);
  CHECK(run->guest_tick == 0u);
  CHECK(run->log_bytes == 0u);
}

/* The four transcripts are pinned from literal text with an independent
 * tool (printf piped to shasum), never from a run. */
#define PIN_ADMIT_0 \
  "5fc4d0d36f6eb18aeaa37457e14d3020e14614b97b9e94c6c219f6036624fbc9"
#define PIN_ADMIT_1 \
  "bd084623780adfd35591144d77f299f3107a3399fe536230c6c891ed7de4f015"
#define PIN_ADMIT_2 \
  "c8ba071062fca5ba87baf198de9cf41bf387fa286754844410b644907eaa2bba"
#define PIN_ADMIT_3 \
  "6a0845a8f00f0078e2dc05dddb9daf4ff66430dd2c74010054759107ab25130f"
/* The wrong variant prints the letter x in place of the digit. */
#define LITERAL_ADMIT_X \
  "c1997caafcdcaa870097c2a8745e5286e77f115c99b76d05aaedebe4c8cb40b7"
#define ADMIT_STEM_COMPLETION_PC "ffffffff80000470"
#define ADMIT_LONG_COMPLETION_PC "ffffffff80000484"

/* Tracer: the stem admit ROM on the stem id is admitted, ticked and passed
 * through the real supervised runner under its per-row pin. */
static int case_tracer_artifact(void) {
  char path[PATH_MAX];
  run_spec spec = {0};
  run_out run;
  spec.supervisor = g_supervisor;
  spec.fixture = path;
  spec.bound = 600000u;

  CHECK(fixture_path(path, "phase09-admit-si-v1"));
  CHECK(run_supervised(&spec, &run));
  expect_pass(&run);
  CHECK(strcmp(run.fixture_id, "phase09-admit-si-v1") == 0);
  CHECK(run.log_bytes == 20u);
  CHECK(strcmp(run.log_sha256, PIN_ADMIT_0) == 0);
  CHECK(run.milestone_count == 4u);
  CHECK(strcmp(run.completion_pc, ADMIT_STEM_COMPLETION_PC) == 0);

  /* Part two: the artifact pair. The test twin with the probe fault publishes
   * exactly one private file of the requested size. */
  {
    char dir[PATH_MAX];
    char first[256];
    char leaf_path[PATH_MAX];
    const uint64_t size = 100000u;
    CHECK(make_artifact_dir(dir, 0700));
    spec.supervisor = g_test_supervisor;
    spec.test_fault = "artifact-publish-probe";
    spec.artifact_dir = dir;
    spec.artifact_max = size;
    CHECK(run_supervised(&spec, &run));
    expect_pass(&run);
    CHECK(list_directory(dir, first) == 1u);
    CHECK(strcmp(first, "probe.sta") == 0);
    CHECK(join_path(leaf_path, sizeof leaf_path, dir, "probe.sta"));
    CHECK(file_matches_pattern(leaf_path, size));

    /* The production runner with the pair only runs the preflight: nothing is
     * published and the directory stays empty. */
    CHECK(make_artifact_dir(dir, 0700));
    spec.supervisor = g_supervisor;
    spec.test_fault = NULL;
    spec.artifact_dir = dir;
    CHECK(run_supervised(&spec, &run));
    expect_pass(&run);
    CHECK(list_directory(dir, first) == 0u);

    /* A missing and a group-writable directory are refused before the guest
     * starts (guest tick 0, refusal). */
    {
      char missing[PATH_MAX];
      char writable[PATH_MAX];
      CHECK(make_artifact_dir(dir, 0700));
      CHECK(join_path(missing, sizeof missing, dir, "absent"));
      spec.artifact_dir = missing;
      CHECK(run_supervised(&spec, &run));
      expect_supervised_preflight_refusal(&run);
      CHECK(make_artifact_dir(writable, 0770));
      CHECK(chmod(writable, 0775) == 0);
      spec.artifact_dir = writable;
      CHECK(run_supervised(&spec, &run));
      expect_supervised_preflight_refusal(&run);
      CHECK(list_directory(writable, first) == 0u);
      /* The runner binary reports the refusal record itself. Its accepted
       * baseline in the same setup: an owner-only directory runs the
       * preflight, the guest passes and nothing is left behind. */
      CHECK(make_artifact_dir(dir, 0700));
      CHECK(run_runner_direct(path, PROFILE_STEM, "600000", dir, "4096",
                              &run));
      CHECK(run.parsed);
      CHECK(strcmp(run.outcome, "pass") == 0);
      CHECK(list_directory(dir, first) == 0u);
      CHECK(run_runner_direct(path, PROFILE_STEM, "600000", missing, "4096",
                              &run));
      expect_refusal(&run);
      CHECK(run_runner_direct(path, PROFILE_STEM, "600000", writable, "4096",
                              &run));
      expect_refusal(&run);
      CHECK(list_directory(writable, first) == 0u);
    }
  }
  return g_failures == 0 ? 0 : 1;
}

static bool sha256_file_hex(const char *path, char out[65]) {
  uint8_t bytes[8192];
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  size_t length = 0u;
  t64_sha256_ctx ctx;
  if (!read_file(path, bytes, sizeof bytes, &length) || length == 0u ||
      length >= sizeof bytes) {
    return false;
  }
  t64_sha256_init(&ctx);
  t64_sha256_update(&ctx, bytes, length);
  t64_sha256_final(&ctx, digest);
  t64_sha256_hex(digest, out);
  return true;
}

typedef struct {
  const char *stem;
  const char *profile;
  const char *pin;
} admit_row;

static const admit_row admit_rows[4] = {
    {"phase09-admit-si-v1", PROFILE_STEM, PIN_ADMIT_0},
    {"phase09-admit-e4k", PROFILE_E4K, PIN_ADMIT_1},
    {"phase09-admit-e16k", PROFILE_E16K, PIN_ADMIT_2},
    {"phase09-admit-s32k", PROFILE_S32K, PIN_ADMIT_3},
};

/* Each of the four ids admits its own authored ROM and passes under its own
 * pinned transcript; a ROM under another row's profile id is refused. */
static int case_admit_ids(void) {
  char path[PATH_MAX];
  char other[PATH_MAX];
  char hashes[4][65];
  run_spec spec = {0};
  run_out run;
  size_t i;
  size_t j;
  spec.supervisor = g_supervisor;
  spec.fixture = path;
  spec.bound = 600000u;
  for (i = 0u; i < 4u; i++) {
    CHECK(fixture_path(path, admit_rows[i].stem));
    CHECK(sha256_file_hex(path, hashes[i]));
    CHECK(run_supervised(&spec, &run));
    expect_pass(&run);
    CHECK(strcmp(run.fixture_id, admit_rows[i].stem) == 0);
    CHECK(strcmp(run.profile_id, admit_rows[i].profile) == 0);
    CHECK(run.log_bytes == 20u);
    CHECK(strcmp(run.log_sha256, admit_rows[i].pin) == 0);
    CHECK(run.milestone_count == 4u);
    CHECK(strcmp(run.completion_pc, ADMIT_STEM_COMPLETION_PC) == 0);
  }
  for (i = 0u; i < 4u; i++) {
    for (j = i + 1u; j < 4u; j++) {
      CHECK(strcmp(hashes[i], hashes[j]) != 0);
      CHECK(strcmp(admit_rows[i].pin, admit_rows[j].pin) != 0);
    }
  }
  /* Identity check on the runner binary: the ROM of row 1 under its own
   * profile id passes in the same setup, under row 0's id it is refused. */
  CHECK(fixture_path(other, admit_rows[1].stem));
  CHECK(run_runner_direct(other, admit_rows[1].profile, "600000", NULL, NULL,
                          &run));
  CHECK(run.parsed);
  CHECK(strcmp(run.outcome, "pass") == 0);
  CHECK(run_runner_direct(other, admit_rows[0].profile, "600000", NULL, NULL,
                          &run));
  expect_refusal(&run);
  return g_failures == 0 ? 0 : 1;
}

/* The wrong variant reaches the same comparator: it completes at the break
 * with a 20-byte transcript that differs from the pin. */
static int case_admit_wrong_transcript(void) {
  char path[PATH_MAX];
  run_spec spec = {0};
  run_out run;
  spec.supervisor = g_supervisor;
  spec.fixture = path;
  spec.bound = 600000u;
  CHECK(fixture_path(path, "phase09-admit-si-v1"));
  CHECK(run_supervised(&spec, &run));
  expect_pass(&run);
  CHECK(strcmp(run.log_sha256, PIN_ADMIT_0) == 0);
  CHECK(fixture_path(path, "phase09-admit-wrong-si-v1"));
  CHECK(run_supervised(&spec, &run));
  expect_wrong_transcript(&run);
  CHECK(strcmp(run.fixture_id, "phase09-admit-wrong-si-v1") == 0);
  CHECK(run.log_bytes == 20u);
  CHECK(strcmp(run.log_sha256, LITERAL_ADMIT_X) == 0);
  CHECK(strcmp(run.log_sha256, PIN_ADMIT_0) != 0);
  CHECK(run.milestone_count == 4u);
  CHECK(strcmp(run.completion_pc, ADMIT_STEM_COMPLETION_PC) == 0);
  return g_failures == 0 ? 0 : 1;
}

static bool is_lower_hex64(const char *text) {
  size_t i;
  if (text == NULL || strlen(text) != 64u) return false;
  for (i = 0u; i < 64u; i++) {
    const char c = text[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  }
  return true;
}

/* N5: the table is read through its accessor. Every id fits the supervisor's
 * 63-character child buffer, every profile resolves and hashes to its literal,
 * no two rows share an identity, and pinned rows are well formed. */
static int case_profile_table(void) {
  const size_t count = t64_real_fixture_count();
  size_t i;
  size_t j;
  size_t pinned = 0u;
  size_t seen_family[4] = {0u, 0u, 0u, 0u};
  char mutated[65];
  CHECK(count >= 11u);
  CHECK(t64_real_fixture_at(count) == NULL);
  for (i = 0u; i < count; i++) {
    const t64_real_fixture *row = t64_real_fixture_at(i);
    const t64_profile_descriptor *profile = NULL;
    uint8_t digest[T64_DIGEST_SIZE];
    char hex[T64_SHA256_HEX_SIZE];
    size_t k;
    CHECK(row != NULL);
    if (row == NULL) continue;
    CHECK(strlen(row->id) >= 1u && strlen(row->id) <= 63u);
    CHECK(strlen(row->profile_id) >= 1u && strlen(row->profile_id) <= 63u);
    CHECK(t64_profile_lookup(row->profile_id, &profile) == T64_STATUS_OK);
    CHECK(profile != NULL);
    if (profile != NULL) {
      CHECK(t64_profile_hash(profile, digest) == T64_STATUS_OK);
      t64_sha256_hex(digest, hex);
      CHECK(strcmp(hex, row->profile_sha256) == 0);
    }
    CHECK(is_lower_hex64(row->sha256));
    CHECK(t64_real_fixture_profile_allowed(row->profile_id));
    for (k = 0u; k < 4u; k++) {
      if (strcmp(row->profile_id, family_ids[k]) == 0) seen_family[k]++;
    }
    if (row->expected_log_sha256 != NULL) {
      pinned++;
      CHECK(row->pass_eligible);
      CHECK(row->expected_log_bytes >= 1u && row->expected_log_bytes <= 4096u);
      CHECK(is_lower_hex64(row->expected_log_sha256));
    } else {
      CHECK(row->expected_log_bytes == 0u);
    }
    CHECK(row->guest_tick_cap == 0u ||
          row->guest_tick_cap <= T64_REAL_GUEST_TICKS_CEILING);
    for (j = i + 1u; j < count; j++) {
      const t64_real_fixture *other = t64_real_fixture_at(j);
      CHECK(other != NULL);
      if (other == NULL) continue;
      CHECK(strcmp(row->id, other->id) != 0);
      CHECK(!(row->size == other->size &&
              strcmp(row->sha256, other->sha256) == 0));
    }
  }
  CHECK(pinned >= 6u);
  /* The four new ids have lengths 38, 42, 43 and 43 and appear in rows. */
  CHECK(strlen(family_ids[0]) == 38u);
  CHECK(strlen(family_ids[1]) == 42u);
  CHECK(strlen(family_ids[2]) == 43u);
  CHECK(strlen(family_ids[3]) == 43u);
  for (i = 0u; i < 4u; i++) CHECK(seen_family[i] >= 1u);
  /* The profile gate accepts registered ids only: an unknown id and a
   * 64-character mutation of a valid id are refused. */
  CHECK(!t64_real_fixture_profile_allowed("no-such-profile"));
  CHECK(!t64_real_fixture_profile_allowed(NULL));
  (void)memset(mutated, 'x', 64u);
  mutated[64] = '\0';
  (void)memcpy(mutated, PROFILE_STEM, strlen(PROFILE_STEM));
  CHECK(strlen(mutated) == 64u);
  CHECK(!t64_real_fixture_profile_allowed(mutated));
  return g_failures == 0 ? 0 : 1;
}

/* K9: the child write limit is exactly the declared artifact maximum plus
 * 65,536 bytes when the pair is present; one byte more ends the child. */
#define FSIZE_SLACK_BYTES 65536u

static bool is_stage_name(const char *name) {
  return strncmp(name, ".t64-art-stage-", 15u) == 0;
}

static int case_fsize_artifact(void) {
  char stem[PATH_MAX];
  char dir[PATH_MAX];
  char first[256];
  const uint64_t size = 100000u;
  run_spec spec = {0};
  run_out run;
  CHECK(fixture_path(stem, "phase09-admit-si-v1"));
  spec.supervisor = g_test_supervisor;
  spec.fixture = stem;
  spec.bound = 600000u;
  spec.artifact_max = size;

  /* Accepted baseline: the probe publishes exactly M bytes and passes. */
  CHECK(make_artifact_dir(dir, 0700));
  spec.artifact_dir = dir;
  spec.test_fault = "artifact-publish-probe";
  CHECK(run_supervised(&spec, &run));
  expect_pass(&run);
  CHECK(list_directory(dir, first) == 1u);
  CHECK(strcmp(first, "probe.sta") == 0);

  /* Exactly M plus 65,536 bytes written to the staged descriptor: the child
   * is not signalled, the run still passes and nothing remains published. */
  CHECK(make_artifact_dir(dir, 0700));
  spec.test_fault = "artifact-write-limit-exact";
  CHECK(run_supervised(&spec, &run));
  expect_pass(&run);
  CHECK(list_directory(dir, first) == 0u);

  /* One more byte: the kernel ends the child, the supervisor reports
   * runner-signalled, nothing is published and the stage stays private. */
  CHECK(make_artifact_dir(dir, 0700));
  spec.test_fault = "artifact-write-limit-over";
  CHECK(run_supervised(&spec, &run));
  CHECK(run.parsed);
  CHECK(run.checker_ok);
  CHECK(strcmp(run.outcome, "harness-error") == 0);
  CHECK(strcmp(run.detail, "runner-signalled") == 0);
  CHECK(strcmp(run.stop_kind, "host-error") == 0);
  CHECK(list_directory(dir, first) == 1u);
  CHECK(is_stage_name(first));
  {
    char stage_path[PATH_MAX];
    char staged_file[PATH_MAX];
    char inner[256];
    struct stat info;
    CHECK(join_path(stage_path, sizeof stage_path, dir, first));
    CHECK(lstat(stage_path, &info) == 0);
    CHECK(S_ISDIR(info.st_mode));
    CHECK((info.st_mode & 0777) == 0700);
    CHECK(list_directory(stage_path, inner) == 1u);
    CHECK(strcmp(inner, "probe.sta") == 0);
    CHECK(join_path(staged_file, sizeof staged_file, stage_path, inner));
    CHECK(lstat(staged_file, &info) == 0);
    /* The write that crossed the limit was cut at the limit exactly. */
    CHECK((uint64_t)info.st_size == size + FSIZE_SLACK_BYTES);
    CHECK((info.st_mode & 0777) == 0600);
  }
  return g_failures == 0 ? 0 : 1;
}

/* With the pair absent the limit is unchanged at the report cap plus the
 * slack (69,632 bytes); the oversize report is still refused in both
 * configurations. */
static int case_fsize_report_capture(void) {
  char stem[PATH_MAX];
  char dir[PATH_MAX];
  run_spec spec = {0};
  run_out run;
  run_out absent;
  run_out present;
  CHECK(fixture_path(stem, "phase09-admit-si-v1"));
  spec.supervisor = g_test_supervisor;
  spec.fixture = stem;
  spec.bound = 600000u;

  /* Accepted baseline in the same setup. */
  CHECK(run_supervised(&spec, &run));
  expect_pass(&run);

  spec.test_fault = "capture-write-limit-exact";
  CHECK(run_supervised(&spec, &run));
  CHECK(run.parsed);
  CHECK(strcmp(run.detail, "runner-signalled") != 0);
  (void)fprintf(stderr, "capture-write-limit-exact record: outcome=%s "
                "detail=%s stop=%s\n", run.outcome, run.detail,
                run.stop_kind);
  CHECK(strcmp(run.outcome, "harness-error") == 0);

  spec.test_fault = "capture-write-limit-over";
  CHECK(run_supervised(&spec, &run));
  CHECK(run.parsed);
  CHECK(strcmp(run.outcome, "harness-error") == 0);
  CHECK(strcmp(run.detail, "runner-signalled") == 0);
  CHECK(strcmp(run.stop_kind, "host-error") == 0);

  /* The oversize report fault is refused the same way with and without the
   * pair; the pair does not loosen the report bound. */
  spec.test_fault = "report-oversize";
  CHECK(run_supervised(&spec, &absent));
  CHECK(absent.parsed);
  CHECK(strcmp(absent.outcome, "harness-error") == 0);
  CHECK(strcmp(absent.detail, "pass") != 0);
  CHECK(make_artifact_dir(dir, 0700));
  spec.artifact_dir = dir;
  spec.artifact_max = 100000u;
  CHECK(run_supervised(&spec, &present));
  CHECK(present.parsed);
  CHECK(strcmp(present.outcome, absent.outcome) == 0);
  CHECK(strcmp(present.detail, absent.detail) == 0);
  CHECK(strcmp(present.stop_kind, absent.stop_kind) == 0);
  (void)fprintf(stderr, "report-oversize record: outcome=%s detail=%s "
                "stop=%s\n", present.outcome, present.detail,
                present.stop_kind);
  return g_failures == 0 ? 0 : 1;
}

typedef struct {
  const char *name;
  int (*run)(void);
} case_entry;

static const case_entry cases[] = {
    {"tracer-artifact", case_tracer_artifact},
    {"admit-ids", case_admit_ids},
    {"admit-wrong-transcript", case_admit_wrong_transcript},
    {"tick-cap", case_tick_cap},
    {"transcript-pin", case_transcript_pin},
    {"argv-bound", case_argv_bound},
    {"fsize-artifact", case_fsize_artifact},
    {"fsize-report-capture", case_fsize_report_capture},
    {"profile-table", case_profile_table},
};

/* Not a case: prints the id, the profile hash and the id length of the four
 * family ids so fixture rows are re-pinned from output, never by hand. */
static int print_profile_hashes(void) {
  size_t i;
  for (i = 0u; i < sizeof family_ids / sizeof family_ids[0]; i++) {
    const t64_profile_descriptor *profile = NULL;
    uint8_t digest[T64_DIGEST_SIZE];
    char hex[T64_SHA256_HEX_SIZE];
    if (t64_profile_lookup(family_ids[i], &profile) != T64_STATUS_OK ||
        profile == NULL ||
        t64_profile_hash(profile, digest) != T64_STATUS_OK) {
      (void)fprintf(stderr, "profile unavailable: %s\n", family_ids[i]);
      return 1;
    }
    t64_sha256_hex(digest, hex);
    (void)printf("%s %s %zu\n", family_ids[i], hex, strlen(family_ids[i]));
  }
  return 0;
}

int main(int argc, char **argv) {
  size_t i;
  if (argc == 2 && strcmp(argv[1], "--print-profile-hashes") == 0) {
    return print_profile_hashes();
  }
  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
      (void)puts(cases[i].name);
    }
    return 0;
  }
  if (argc != 6) {
    (void)fprintf(stderr,
                  "usage: %s CASE SUPERVISOR TEST_SUPERVISOR CHECKER ROOT\n",
                  argv[0]);
    return 2;
  }
  g_supervisor = argv[2];
  g_test_supervisor = argv[3];
  g_checker = argv[4];
  g_root = argv[5];
  if (!prepare_evidence()) {
    (void)fprintf(stderr, "evidence directory unavailable\n");
    return 2;
  }
  for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
    if (strcmp(argv[1], cases[i].name) == 0) {
      int status = cases[i].run();
      (void)fprintf(stderr, "case %s: %s\n", cases[i].name,
                    status == 0 ? "ok" : "FAILED");
      return status;
    }
  }
  (void)fprintf(stderr, "unknown case: %s\n", argv[1]);
  return 2;
}
