/*
 * Phase 9 state container export through the real supervised runner (plan
 * 09-08). Every case drives the production supervisor and runner binaries (or
 * the test twin for the file-size faults) on the authored admit ROM; the
 * published blob is read back by an independent Python decoder and the
 * expected values come from in-process public API calls and from SHA-256 of
 * the ROM file, never from the exporting process. Nothing here is a hardware,
 * timing or cross-build claim, and the container hashes are integrity checks,
 * not authentication.
 *
 * usage: test_state_runner CASE|all SUPERVISOR TEST_SUPERVISOR CHECKER ROOT
 *        test_state_runner --list-cases
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

static bool write_file(const char *path, const uint8_t *bytes,
                       size_t length) {
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  size_t done = 0u;
  if (fd < 0) return false;
  while (done < length) {
    ssize_t count = write(fd, bytes + done, length - done);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) {
      (void)close(fd);
      return false;
    }
    done += (size_t)count;
  }
  return close(fd) == 0;
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
    /* A racing mkdir that finds the directory already present is success. */
    if (mkdir(g_evidence, 0700) != 0 && errno != EEXIST) return false;
    return stat(g_evidence, &info) == 0 && S_ISDIR(info.st_mode);
  }
  if (temp == NULL || temp[0] == '\0') temp = "/tmp";
  if (snprintf(g_evidence, sizeof g_evidence, "%s/t64-state.XXXXXX", temp) >=
      (int)sizeof g_evidence) {
    return false;
  }
  return mkdtemp(g_evidence) != NULL;
}

/* A fresh private working directory for one run. */
static bool make_run_dir(char out[PATH_MAX]) {
  if (snprintf(out, PATH_MAX, "%s/st.XXXXXX", g_evidence) >= PATH_MAX) {
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
  int supervisor_exit;
  char outcome[48];
  char detail[64];
  char stop_kind[48];
  char fixture_id[96];
  char profile_id[96];
  char state_digest[65];
  char profile_digest[65];
  char fixture_sha256[65];
  uint64_t guest_tick;
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
  uint64_t value = 0u;
  if (!read_file(path, bytes, sizeof bytes, &length) || length == 0u ||
      t64_json_parse(bytes, length, NULL, &doc, NULL) != 0) {
    return false;
  }
  stop = t64_json_get(doc.root, "guest_stop");
  copy_string(out->outcome, sizeof out->outcome,
              t64_json_get(doc.root, "outcome"));
  copy_string(out->detail, sizeof out->detail,
              t64_json_get(doc.root, "detail"));
  copy_string(out->fixture_id, sizeof out->fixture_id,
              t64_json_get(doc.root, "fixture_id"));
  copy_string(out->profile_id, sizeof out->profile_id,
              t64_json_get(doc.root, "profile_id"));
  copy_string(out->state_digest, sizeof out->state_digest,
              t64_json_get(doc.root, "state_digest"));
  copy_string(out->profile_digest, sizeof out->profile_digest,
              t64_json_get(doc.root, "profile_digest"));
  copy_string(out->fixture_sha256, sizeof out->fixture_sha256,
              t64_json_get(doc.root, "fixture_sha256"));
  copy_string(out->stop_kind, sizeof out->stop_kind,
              stop == NULL ? NULL : t64_json_get(stop, "kind"));
  if (t64_json_u64_string(t64_json_get(doc.root, "guest_tick"), &value)) {
    out->guest_tick = value;
  }
  t64_json_free(&doc);
  return true;
}

/* Runs the strict result checker on a record. Returns its exit status (or -1)
 * and the first output line, which starts with the diagnostic code. */
static int run_checker(const char *result_path, char diagnostic[160]) {
  int pipe_fds[2];
  pid_t pid;
  int status = 0;
  size_t used = 0u;
  diagnostic[0] = '\0';
  if (pipe(pipe_fds) != 0) return -1;
  pid = fork();
  if (pid < 0) return -1;
  if (pid == 0) {
    char mode[] = "result";
    char *args[4];
    (void)close(pipe_fds[0]);
    (void)dup2(pipe_fds[1], STDOUT_FILENO);
    (void)dup2(pipe_fds[1], STDERR_FILENO);
    args[0] = (char *)(uintptr_t)g_checker;
    args[1] = mode;
    args[2] = (char *)(uintptr_t)result_path;
    args[3] = NULL;
    execv(g_checker, args);
    _exit(127);
  }
  (void)close(pipe_fds[1]);
  for (;;) {
    char chunk[256];
    ssize_t count = read(pipe_fds[0], chunk, sizeof chunk);
    size_t i;
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) break;
    for (i = 0u; i < (size_t)count && used + 1u < 160u; i++) {
      if (chunk[i] == '\n') break;
      diagnostic[used++] = chunk[i];
    }
    diagnostic[used] = '\0';
  }
  (void)close(pipe_fds[0]);
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) return -1;
  }
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* ---- supervised run ----------------------------------------------------- */

typedef struct {
  const char *supervisor; /* production or test twin */
  const char *fixture;
  uint64_t bound;         /* 0 leaves the option out */
  const char *test_fault; /* NULL leaves the option out (test twin only) */
  const char *artifact_dir; /* NULL leaves the pair out */
  uint64_t artifact_max;
  const char *const *extra; /* more option/value strings, or NULL */
  size_t extra_count;
} run_spec;

#define MAX_ARGS 64u

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
    char fault_option[] = "--test-fault";
    char artifact_dir_option[] = "--artifact-dir";
    char artifact_max_option[] = "--artifact-max-bytes";
    size_t i;
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
    for (i = 0u; i < spec->extra_count; i++) {
      arg_add(&list, (char *)(uintptr_t)spec->extra[i]);
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
  return out->parsed;
}

static void expect_pass(const run_out *run) {
  char diagnostic[160];
  CHECK(run->parsed);
  CHECK(run_checker(run->result_path, diagnostic) == 0);
  CHECK(run->supervisor_exit == 0);
  CHECK(strcmp(run->outcome, "pass") == 0);
  CHECK(strcmp(run->detail, "positive-match") == 0);
  CHECK(strcmp(run->stop_kind, "authored-completion") == 0);
}

/* ---- case checker-vocabulary ------------------------------------------- */

/* Returns the text of a record with every occurrence of one fixed string
 * replaced; the copy is written to `path`. */
static bool write_replaced(const char *source, const char *path,
                           const char *from, const char *to) {
  uint8_t bytes[8192];
  size_t length = 0u;
  char text[8192];
  char out[8192];
  size_t used = 0u;
  const size_t from_length = strlen(from);
  const size_t to_length = strlen(to);
  const char *cursor;
  if (!read_file(source, bytes, sizeof bytes - 1u, &length)) return false;
  memcpy(text, bytes, length);
  text[length] = '\0';
  cursor = text;
  for (;;) {
    const char *found = strstr(cursor, from);
    size_t head = found == NULL ? strlen(cursor) : (size_t)(found - cursor);
    if (used + head + to_length + 1u > sizeof out) return false;
    memcpy(out + used, cursor, head);
    used += head;
    if (found == NULL) break;
    memcpy(out + used, to, to_length);
    used += to_length;
    cursor = found + from_length;
  }
  out[used] = '\0';
  return write_file(path, (const uint8_t *)out, used);
}

/* The record the runner writes at a state-export failure is the guest's own
 * pass record with only the outcome, the stop kind and the detail replaced.
 * The accepted baseline of this case is the real pass record. */
static int case_checker_vocabulary(void) {
  char rom[PATH_MAX];
  char cap_path[PATH_MAX];
  char bogus_path[PATH_MAX];
  char pass_path[PATH_MAX];
  char diagnostic[160];
  run_spec spec = {0};
  run_out run;
  spec.supervisor = g_supervisor;
  spec.bound = 600000u;
  CHECK(fixture_path(rom, "phase09-admit-si-v1"));
  spec.fixture = rom;
  CHECK(run_supervised(&spec, &run));
  expect_pass(&run);
  CHECK(join_path(cap_path, sizeof cap_path, run.dir, "cap.json"));
  CHECK(join_path(bogus_path, sizeof bogus_path, run.dir, "bogus.json"));
  CHECK(join_path(pass_path, sizeof pass_path, run.dir, "outcome-pass.json"));
  {
    char shaped_path[PATH_MAX];
    CHECK(join_path(shaped_path, sizeof shaped_path, run.dir, "shaped.json"));
    CHECK(write_replaced(run.result_path, shaped_path, "\"detail\":\"positive-match\"",
                         "\"detail\":\"state-export-cap\""));
    CHECK(write_replaced(shaped_path, cap_path, "\"kind\":\"authored-completion\"",
                         "\"kind\":\"host-error\""));
    CHECK(write_replaced(cap_path, shaped_path, "\"outcome\":\"pass\"",
                         "\"outcome\":\"harness-error\""));
    CHECK(rename(shaped_path, cap_path) == 0);
  }
  /* The state-export-cap record is accepted: harness-error, host-error stop. */
  CHECK(run_checker(cap_path, diagnostic) == 0);
  if (diagnostic[0] != '\0') {
    (void)fprintf(stderr, "checker (cap record): %s\n", diagnostic);
  }
  /* An unregistered state-export name is outside the closed enum. */
  CHECK(write_replaced(cap_path, bogus_path, "state-export-cap",
                       "state-export-bogus"));
  CHECK(run_checker(bogus_path, diagnostic) == 1);
  CHECK(strncmp(diagnostic, "R-ENUM ", 7u) == 0);
  /* The registered name with the outcome replaced by pass is inconsistent. */
  CHECK(write_replaced(cap_path, pass_path, "\"outcome\":\"harness-error\"",
                       "\"outcome\":\"pass\""));
  CHECK(run_checker(pass_path, diagnostic) == 1);
  CHECK(strncmp(diagnostic, "R-DETAIL-OUTCOME ", 17u) == 0);
  return g_failures == 0 ? 0 : 1;
}

/* ---- dispatch ----------------------------------------------------------- */

typedef struct {
  const char *name;
  int (*run)(void);
} case_entry;

static const case_entry cases[] = {
    {"checker-vocabulary", case_checker_vocabulary},
};

int main(int argc, char **argv) {
  size_t i;
  int worst = 0;
  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
      (void)puts(cases[i].name);
    }
    return 0;
  }
  if (argc != 6) {
    (void)fprintf(stderr,
                  "usage: %s CASE|all SUPERVISOR TEST_SUPERVISOR CHECKER ROOT\n",
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
    if (strcmp(argv[1], "all") == 0 || strcmp(argv[1], cases[i].name) == 0) {
      int status;
      g_failures = 0;
      status = cases[i].run();
      (void)fprintf(stderr, "case %s: %s\n", cases[i].name,
                    status == 0 ? "ok" : "FAILED");
      if (status != 0) worst = 1;
      if (strcmp(argv[1], "all") != 0) return status;
    }
  }
  if (strcmp(argv[1], "all") == 0) return worst;
  (void)fprintf(stderr, "unknown case: %s\n", argv[1]);
  return 2;
}
