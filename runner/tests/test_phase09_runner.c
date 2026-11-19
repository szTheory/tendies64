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

#define LEGACY_HELLO_PROFILE "ntsc-cart-4m-openipl3-warm-v1"

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
    if (stat(g_evidence, &info) != 0 && mkdir(g_evidence, 0700) != 0) {
      return false;
    }
    return true;
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
  char log_sha256[65];
  bool has_log_sha256;
  uint64_t guest_tick;
  uint64_t bound_tick;
  uint64_t log_bytes;
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
    arg_add(&list, capture_option);
    arg_add(&list, capture_path);
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

/* The runner binary runs directly with inherited descriptors. */
static bool run_runner_direct(const char *fixture, const char *profile,
                              const char *ticks_text, run_out *out) {
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
  fixture_fd = open(fixture, O_RDONLY);
  input_fd = open(input, O_RDONLY);
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
    char *args[10];
    int out_fd = open(stdout_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    int null_fd = open("/dev/null", O_WRONLY);
    if (out_fd < 0 || dup2(out_fd, STDOUT_FILENO) < 0) _exit(126);
    if (null_fd >= 0) (void)dup2(null_fd, STDERR_FILENO);
    args[0] = runner;
    args[1] = fixture_option;
    args[2] = fixture_fd_text;
    args[3] = input_option;
    args[4] = input_fd_text;
    args[5] = profile_option;
    args[6] = (char *)(uintptr_t)profile;
    args[7] = ticks_option;
    args[8] = (char *)(uintptr_t)ticks_text;
    args[9] = NULL;
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
  run_spec spec;
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

  /* The runner binary applies the same two checks on its own. */
  CHECK(run_runner_direct(hello, LEGACY_HELLO_PROFILE, "600001", &run));
  expect_refusal(&run);
  CHECK(run_runner_direct(hello, LEGACY_HELLO_PROFILE, "4000001", &run));
  expect_usage(&run);
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
static int case_transcript_pin(void) {
  char path[PATH_MAX];
  run_spec spec;
  run_out run;
  spec.supervisor = g_supervisor;
  spec.fixture = path;
  spec.bound = 600000u;

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

typedef struct {
  const char *name;
  int (*run)(void);
} case_entry;

static const case_entry cases[] = {
    {"tick-cap", case_tick_cap},
    {"transcript-pin", case_transcript_pin},
};

int main(int argc, char **argv) {
  size_t i;
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
