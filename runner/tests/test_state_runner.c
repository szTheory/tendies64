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
 *        test_state_runner --run-mutant none|observer-range
 *                          (plan 09-10 tracer option, not a case; reads
 *                          T64_STATE_MUTANT_INVENTORY; provisional)
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
  char build_id[96];
  uint64_t guest_tick;
  uint64_t log_bytes;
  char dir[PATH_MAX];
  char result_path[PATH_MAX];
  char err_path[PATH_MAX]; /* the direct runner's diagnostic stream */
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
  {
    const t64_json_node *counts = t64_json_get(doc.root, "counts");
    if (counts != NULL &&
        t64_json_u64_string(t64_json_get(counts, "log_bytes"), &value)) {
      out->log_bytes = value;
    }
  }
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
  copy_string(out->build_id, sizeof out->build_id,
              t64_json_get(doc.root, "build_id"));
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

/* ---- artifact directories ----------------------------------------------- */

static bool make_artifact_dir(char out[PATH_MAX]) {
  if (!make_run_dir(out)) return false;
  return chmod(out, 0700) == 0;
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

/* Removes a directory tree (the evidence stays small: a blob is several MiB). */
static void remove_tree(const char *path) {
  DIR *dir = opendir(path);
  struct dirent *entry;
  if (dir != NULL) {
    while ((entry = readdir(dir)) != NULL) {
      char child[PATH_MAX];
      struct stat info;
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 ||
          !join_path(child, sizeof child, path, entry->d_name)) {
        continue;
      }
      if (lstat(child, &info) == 0 && S_ISDIR(info.st_mode)) {
        remove_tree(child);
      } else {
        (void)unlink(child);
      }
    }
    (void)closedir(dir);
  }
  (void)rmdir(path);
}

static bool sha256_file_hex(const char *path, char out[65]) {
  uint8_t chunk[8192];
  t64_sha256_ctx context;
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  int fd = open(path, O_RDONLY);
  if (fd < 0) return false;
  t64_sha256_init(&context);
  for (;;) {
    ssize_t count = read(fd, chunk, sizeof chunk);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) break;
    t64_sha256_update(&context, chunk, (size_t)count);
  }
  (void)close(fd);
  t64_sha256_final(&context, digest);
  t64_sha256_hex(digest, out);
  return true;
}

static void expect_usage(const run_out *run) {
  CHECK(run->parsed);
  CHECK(strcmp(run->outcome, "harness-error") == 0);
  CHECK(strcmp(run->detail, "usage") == 0);
  CHECK(strcmp(run->stop_kind, "not-started") == 0);
  CHECK(run->guest_tick == 0u);
}

/* The runner binary reports the refusal record itself. */
static void expect_refusal(const run_out *run) {
  CHECK(run->parsed);
  CHECK(strcmp(run->outcome, "harness-error") == 0);
  CHECK(strcmp(run->detail, "refusal") == 0);
  CHECK(strcmp(run->stop_kind, "refused") == 0);
  CHECK(run->guest_tick == 0u);
}

/* A runner prelaunch refusal reaches the supervised record as harness-error,
 * report-error stop, detail capture-invalid, guest tick 0 (the 09-02 rule). */
static void expect_supervised_preflight_refusal(const run_out *run) {
  char diagnostic[160];
  CHECK(run->parsed);
  CHECK(run_checker(run->result_path, diagnostic) == 0);
  CHECK(strcmp(run->outcome, "harness-error") == 0);
  CHECK(strcmp(run->stop_kind, "report-error") == 0);
  CHECK(strcmp(run->detail, "capture-invalid") == 0);
  CHECK(run->guest_tick == 0u);
}

/* A host-error record of the closed state-export vocabulary. */
static void expect_state_export(const run_out *run, const char *detail) {
  char diagnostic[160];
  CHECK(run->parsed);
  CHECK(run_checker(run->result_path, diagnostic) == 0);
  CHECK(strcmp(run->outcome, "harness-error") == 0);
  CHECK(strcmp(run->stop_kind, "host-error") == 0);
  CHECK(strcmp(run->detail, detail) == 0);
}

#define STATE_FORMAT "t64.state-container.v1"
#define ARTIFACT_MAX 33554432u
#define DEFAULT_CHILD_LIMIT 69632u
#define STATE_LEAF "state.sta"

static const char *const state_args[] = {"--state-export", STATE_LEAF,
                                         "--state-export-format",
                                         STATE_FORMAT};

/* Size and mode of the published file; false when it is not a private
 * regular file with one link. */
static bool stat_private_file(const char *path, uint64_t *size_out) {
  struct stat info;
  if (lstat(path, &info) != 0 || !S_ISREG(info.st_mode) ||
      info.st_nlink != 1 || (info.st_mode & 0777) != 0600) {
    return false;
  }
  *size_out = (uint64_t)info.st_size;
  return true;
}

/* The accepted production run: pass, one private state.sta in a fresh owner-only
 * directory. Returns the directory and the file size S. */
static bool run_accepted(char dir[PATH_MAX], uint64_t artifact_max,
                         run_out *run, uint64_t *size_out) {
  char rom[PATH_MAX];
  char leaf[PATH_MAX];
  char first[256];
  run_spec spec = {0};
  if (!fixture_path(rom, "phase09-admit-si-v1") || !make_artifact_dir(dir)) {
    return false;
  }
  spec.supervisor = g_supervisor;
  spec.fixture = rom;
  spec.bound = 600000u;
  spec.artifact_dir = dir;
  spec.artifact_max = artifact_max;
  spec.extra = state_args;
  spec.extra_count = sizeof state_args / sizeof state_args[0];
  if (!run_supervised(&spec, run)) return false;
  expect_pass(run);
  CHECK(list_directory(dir, first) == 1u);
  CHECK(strcmp(first, STATE_LEAF) == 0);
  CHECK(join_path(leaf, sizeof leaf, dir, STATE_LEAF));
  *size_out = 0u;
  CHECK(stat_private_file(leaf, size_out));
  return true;
}

/* The state size S of the accepted run, measured once per process. */
static uint64_t g_state_size;
static uint64_t g_accepted_tick;

static bool ensure_state_size(void) {
  char dir[PATH_MAX];
  run_out run;
  if (g_state_size != 0u) return true;
  if (!run_accepted(dir, ARTIFACT_MAX, &run, &g_state_size)) return false;
  g_accepted_tick = run.guest_tick;
  remove_tree(dir);
  return g_state_size != 0u;
}

/* The runner binary runs directly with inherited descriptors (the one-producer
 * rows of its own argument grammar). */
static int open_snapshot(const char *source, const char *directory) {
  uint8_t bytes[8192];
  size_t length = 0u;
  char name[PATH_MAX];
  size_t written = 0u;
  int fd;
  if (!read_file(source, bytes, sizeof bytes, &length) || length == 0u ||
      length >= sizeof bytes ||
      !join_path(name, sizeof name, directory, "snapshot.XXXXXX")) {
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

static bool run_runner_direct(const char *fixture, const char *profile,
                              const char *ticks_text, const char *artifact_dir,
                              const char *artifact_max_text,
                              const char *const *extra, size_t extra_count,
                              run_out *out) {
  char runner[PATH_MAX];
  char input[PATH_MAX];
  char stdout_path[PATH_MAX];
  char stderr_path[PATH_MAX];
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
      !join_path(stdout_path, sizeof stdout_path, out->dir, "runner.json") ||
      !join_path(stderr_path, sizeof stderr_path, out->dir, "runner.err")) {
    return false;
  }
  (void)snprintf(out->err_path, sizeof out->err_path, "%s", stderr_path);
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
    char *args[40];
    size_t count = 0u;
    size_t i;
    int out_fd = open(stdout_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    int err_fd = open(stderr_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (out_fd < 0 || err_fd < 0 || dup2(out_fd, STDOUT_FILENO) < 0 ||
        dup2(err_fd, STDERR_FILENO) < 0) {
      _exit(126);
    }
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
    for (i = 0u; i < extra_count && count + 1u < sizeof args / sizeof args[0];
         i++) {
      args[count++] = (char *)(uintptr_t)extra[i];
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

/* ---- the independent Python decoder ------------------------------------- */

typedef struct {
  char status[16];
  char kind[32];
  char total_length[24];
  char version[8];
  char canonical_digest[65];
  char guest_tick[24];
  char profile_digest[65];
  char content_sha256[65];
  char content_length[24];
  char build_id[96];
  char ids[96];
  char observer[96];
} decoded_blob;

static void join_array(char *out, size_t capacity, const t64_json_node *array) {
  const t64_json_node *item = array == NULL ? NULL : array->first;
  size_t used = 0u;
  out[0] = '\0';
  while (item != NULL && item->type == T64_JSON_STRING) {
    int written = snprintf(out + used, capacity - used, "%s%.*s",
                           used == 0u ? "" : ",", (int)item->len, item->str);
    if (written < 0 || (size_t)written >= capacity - used) return;
    used += (size_t)written;
    item = item->next;
  }
}

/* Runs `python3 -B -I decode_container.py --blob PATH`; returns the decoder
 * exit status (0 accepted, 1 refused, 2 usage) or -1, and parses its one JSON
 * line. */
static int decode_blob(const char *path, decoded_blob *out) {
  const char *script = getenv("T64_STATE_DECODER");
  int pipe_fds[2];
  pid_t pid;
  int status = 0;
  uint8_t line[4096];
  size_t used = 0u;
  t64_json_doc doc = {0};
  memset(out, 0, sizeof *out);
  if (script == NULL || script[0] == '\0' || pipe(pipe_fds) != 0) return -1;
  pid = fork();
  if (pid < 0) return -1;
  if (pid == 0) {
    char python[] = "python3";
    char no_bytecode[] = "-B";
    char isolated[] = "-I";
    char blob_option[] = "--blob";
    char *args[7];
    (void)close(pipe_fds[0]);
    (void)dup2(pipe_fds[1], STDOUT_FILENO);
    args[0] = python;
    args[1] = no_bytecode;
    args[2] = isolated;
    args[3] = (char *)(uintptr_t)script;
    args[4] = blob_option;
    args[5] = (char *)(uintptr_t)path;
    args[6] = NULL;
    execvp(python, args);
    _exit(127);
  }
  (void)close(pipe_fds[1]);
  for (;;) {
    ssize_t count = read(pipe_fds[0], line + used, sizeof line - 1u - used);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) break;
    used += (size_t)count;
    if (used + 1u >= sizeof line) break;
  }
  (void)close(pipe_fds[0]);
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) return -1;
  }
  if (!WIFEXITED(status)) return -1;
  if (used > 0u && t64_json_parse(line, used, NULL, &doc, NULL) == 0) {
    copy_string(out->status, sizeof out->status,
                t64_json_get(doc.root, "status"));
    copy_string(out->kind, sizeof out->kind, t64_json_get(doc.root, "kind"));
    copy_string(out->total_length, sizeof out->total_length,
                t64_json_get(doc.root, "total_length"));
    copy_string(out->version, sizeof out->version,
                t64_json_get(doc.root, "state_serialization_version"));
    copy_string(out->canonical_digest, sizeof out->canonical_digest,
                t64_json_get(doc.root, "canonical_digest"));
    copy_string(out->guest_tick, sizeof out->guest_tick,
                t64_json_get(doc.root, "guest_tick"));
    copy_string(out->profile_digest, sizeof out->profile_digest,
                t64_json_get(doc.root, "profile_digest"));
    copy_string(out->content_sha256, sizeof out->content_sha256,
                t64_json_get(doc.root, "content_sha256"));
    copy_string(out->content_length, sizeof out->content_length,
                t64_json_get(doc.root, "content_length"));
    copy_string(out->build_id, sizeof out->build_id,
                t64_json_get(doc.root, "build_id"));
    join_array(out->ids, sizeof out->ids,
               t64_json_get(doc.root, "section_ids"));
    join_array(out->observer, sizeof out->observer,
               t64_json_get(doc.root, "observer"));
    t64_json_free(&doc);
  }
  return WEXITSTATUS(status);
}

/* The decoded values of the real published blob against values the exporting
 * process did not produce: the supervisor's result record, an in-process
 * public API call for the profile digest, and SHA-256 and the size of the ROM
 * file. */
static void check_decoded_against(const char *blob_path, const char *rom,
                                  const run_out *run, uint64_t size) {
  decoded_blob decoded;
  const t64_profile_descriptor *profile = NULL;
  uint8_t digest[T64_DIGEST_SIZE];
  char profile_hex[T64_SHA256_HEX_SIZE];
  char rom_hex[65];
  char expected[32];
  struct stat info;
  CHECK(decode_blob(blob_path, &decoded) == 0);
  CHECK(strcmp(decoded.status, "accepted") == 0);
  CHECK(strcmp(decoded.kind, "ok") == 0);
  (void)snprintf(expected, sizeof expected, "%llu", (unsigned long long)size);
  CHECK(strcmp(decoded.total_length, expected) == 0);
  CHECK(strcmp(decoded.version, "25") == 0);
  CHECK(strcmp(decoded.ids, "1,2,3,4,5,6,7,8,9,10,16") == 0);
  CHECK(strcmp(decoded.canonical_digest, run->state_digest) == 0);
  (void)snprintf(expected, sizeof expected, "%llu",
                 (unsigned long long)run->guest_tick);
  CHECK(strcmp(decoded.guest_tick, expected) == 0);
  CHECK(t64_profile_lookup(PROFILE_STEM, &profile) == T64_STATUS_OK &&
        profile != NULL && t64_profile_hash(profile, digest) == T64_STATUS_OK);
  t64_sha256_hex(digest, profile_hex);
  CHECK(strcmp(decoded.profile_digest, profile_hex) == 0);
  CHECK(strcmp(decoded.profile_digest, run->profile_digest) == 0);
  CHECK(sha256_file_hex(rom, rom_hex));
  CHECK(strcmp(decoded.content_sha256, rom_hex) == 0);
  CHECK(stat(rom, &info) == 0);
  (void)snprintf(expected, sizeof expected, "%llu",
                 (unsigned long long)info.st_size);
  CHECK(strcmp(decoded.content_length, expected) == 0);
  /* No host path in the build identity of the header, and it equals the
   * record's. */
  CHECK(strcmp(decoded.build_id, run->build_id) == 0);
  CHECK(strchr(decoded.build_id, '/') == NULL);
  /* The observer section carries the runner's output drain cursor: the bytes
   * it drained are the record's log bytes; no video or audio was produced. */
  (void)snprintf(expected, sizeof expected, "%llu,0,0,0,0",
                 (unsigned long long)run->log_bytes);
  CHECK(run->log_bytes > 0u);
  CHECK(strcmp(decoded.observer, expected) == 0);
}

/* ---- case export-accepted ----------------------------------------------- */

static int case_export_accepted(void) {
  char dir[PATH_MAX];
  run_out run;
  uint64_t size = 0u;
  CHECK(run_accepted(dir, ARTIFACT_MAX, &run, &size));
  /* Larger than the default child limit, within the raised one, and between
   * the independent arithmetic bounds (the major arrays alone: RDRAM 4,194,308,
   * two 2,097,152-byte coverage tables, TMEM 4,096 and the SP memory 8,200 are
   * 8,400,908; and 11 MiB). */
  CHECK(size > DEFAULT_CHILD_LIMIT);
  CHECK(size <= ARTIFACT_MAX + 65536u);
  CHECK(size > 8400908u);
  CHECK(size < 11534336u);
  g_state_size = size;
  g_accepted_tick = run.guest_tick;
  (void)fprintf(stderr, "export-accepted: state size %llu, guest tick %llu\n",
                (unsigned long long)size, (unsigned long long)run.guest_tick);
  {
    char rom[PATH_MAX];
    char blob[PATH_MAX];
    CHECK(fixture_path(rom, "phase09-admit-si-v1"));
    CHECK(join_path(blob, sizeof blob, dir, STATE_LEAF));
    check_decoded_against(blob, rom, &run, size);
  }
  remove_tree(dir);
  return g_failures == 0 ? 0 : 1;
}

/* ---- case export-size-cap ----------------------------------------------- */

static int case_export_size_cap(void) {
  char dir[PATH_MAX];
  char rom[PATH_MAX];
  char first[256];
  run_spec spec = {0};
  run_out run;
  uint64_t size = 0u;
  CHECK(ensure_state_size());
  /* The maximum equal to the size publishes exactly that many bytes. */
  CHECK(run_accepted(dir, g_state_size, &run, &size));
  CHECK(size == g_state_size);
  remove_tree(dir);
  /* One byte below: refused with the named detail after the guest ran, with
   * the accepted run's guest tick and nothing published or left behind. */
  CHECK(fixture_path(rom, "phase09-admit-si-v1"));
  CHECK(make_artifact_dir(dir));
  spec.supervisor = g_supervisor;
  spec.fixture = rom;
  spec.bound = 600000u;
  spec.artifact_dir = dir;
  spec.artifact_max = g_state_size - 1u;
  spec.extra = state_args;
  spec.extra_count = sizeof state_args / sizeof state_args[0];
  CHECK(run_supervised(&spec, &run));
  expect_state_export(&run, "state-export-cap");
  CHECK(run.guest_tick == g_accepted_tick);
  CHECK(list_directory(dir, first) == 0u);
  remove_tree(dir);
  return g_failures == 0 ? 0 : 1;
}

/* ---- case export-fsize-at-state-size (K9) -------------------------------- */

static bool is_stage_name(const char *name) {
  return strncmp(name, ".t64-art-stage-", 15u) == 0;
}

static int case_export_fsize(void) {
  char rom[PATH_MAX];
  char dir[PATH_MAX];
  char first[256];
  run_spec spec = {0};
  run_out run;
  CHECK(ensure_state_size());
  CHECK(fixture_path(rom, "phase09-admit-si-v1"));
  spec.supervisor = g_test_supervisor;
  spec.fixture = rom;
  spec.bound = 600000u;
  spec.artifact_max = g_state_size;

  /* Accepted baseline: with the maximum equal to the state size the probe
   * publishes exactly that many bytes and the run passes. */
  CHECK(make_artifact_dir(dir));
  spec.artifact_dir = dir;
  spec.test_fault = "artifact-publish-probe";
  CHECK(run_supervised(&spec, &run));
  expect_pass(&run);
  CHECK(list_directory(dir, first) == 1u);
  CHECK(strcmp(first, "probe.sta") == 0);
  remove_tree(dir);

  /* Exactly S plus 65,536 bytes through the staged descriptor: not signalled,
   * the run passes and nothing remains published. */
  CHECK(make_artifact_dir(dir));
  spec.artifact_dir = dir;
  spec.test_fault = "artifact-write-limit-exact";
  CHECK(run_supervised(&spec, &run));
  expect_pass(&run);
  CHECK(list_directory(dir, first) == 0u);
  remove_tree(dir);

  /* One byte more: the kernel ends the child, the supervisor reports
   * runner-signalled and nothing is published; only the private stage stays,
   * cut at the limit exactly. */
  CHECK(make_artifact_dir(dir));
  spec.artifact_dir = dir;
  spec.test_fault = "artifact-write-limit-over";
  CHECK(run_supervised(&spec, &run));
  {
    char diagnostic[160];
    CHECK(run.parsed);
    CHECK(run_checker(run.result_path, diagnostic) == 0);
  }
  CHECK(strcmp(run.outcome, "harness-error") == 0);
  CHECK(strcmp(run.detail, "runner-signalled") == 0);
  CHECK(strcmp(run.stop_kind, "host-error") == 0);
  CHECK(list_directory(dir, first) == 1u);
  CHECK(is_stage_name(first));
  {
    char stage_path[PATH_MAX];
    char inner[256];
    char staged_file[PATH_MAX];
    struct stat info;
    CHECK(join_path(stage_path, sizeof stage_path, dir, first));
    CHECK(list_directory(stage_path, inner) == 1u);
    CHECK(strcmp(inner, "probe.sta") == 0);
    CHECK(join_path(staged_file, sizeof staged_file, stage_path, inner));
    CHECK(lstat(staged_file, &info) == 0);
    CHECK((uint64_t)info.st_size == g_state_size + 65536u);
    CHECK((info.st_mode & 0777) == 0600);
  }
  remove_tree(dir);

  /* FX-10 (L3 WR-01, VG-10): a failed publication publishes nothing. The
   * accepted baseline of the same setup (test twin, state export, no fault)
   * publishes the leaf and passes. The directory sync that follows the commit
   * then fails (test fault): the record names the closed state-export-publish
   * detail, the leaf is withdrawn and no stage is left, so the directory is
   * empty. A directory that cannot sync is refused by the preflight before
   * the guest starts (guest tick 0), also with an empty directory. */
  spec.extra = state_args;
  spec.extra_count = sizeof state_args / sizeof state_args[0];
  spec.artifact_max = ARTIFACT_MAX;
  CHECK(make_artifact_dir(dir));
  spec.artifact_dir = dir;
  spec.test_fault = NULL;
  CHECK(run_supervised(&spec, &run));
  expect_pass(&run);
  CHECK(list_directory(dir, first) == 1u);
  CHECK(strcmp(first, STATE_LEAF) == 0);
  remove_tree(dir);

  CHECK(make_artifact_dir(dir));
  spec.artifact_dir = dir;
  spec.test_fault = "artifact-sync-after-rename";
  CHECK(run_supervised(&spec, &run));
  expect_state_export(&run, "state-export-publish");
  CHECK(run.guest_tick == g_accepted_tick);
  CHECK(list_directory(dir, first) == 0u);
  remove_tree(dir);

  CHECK(make_artifact_dir(dir));
  spec.artifact_dir = dir;
  spec.test_fault = "artifact-sync-preflight";
  CHECK(run_supervised(&spec, &run));
  expect_supervised_preflight_refusal(&run);
  CHECK(list_directory(dir, first) == 0u);
  remove_tree(dir);
  return g_failures == 0 ? 0 : 1;
}

/* ---- case export-refusals ------------------------------------------------ */

/* The save and replay option families, each complete in itself so the
 * baseline without the state flags is not a usage refusal. */
static const char *const hex64 =
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

typedef struct {
  const char *label;
  const char *args[8];
  size_t count;
} family_row;

static const family_row producer_rows[] = {
    {"save-import trio",
     {"--save-import", "/nonexistent/save.bin", "--save-import-size", "512",
      "--save-import-sha256", hex64},
     6u},
    {"save-kind", {"--save-kind", "eeprom4k"}, 2u},
    {"save-format", {"--save-format", "raw-v1"}, 2u},
    {"save-export-leaf", {"--save-export-leaf", "s.eep"}, 2u},
    {"replay-record", {"--replay-record", "r.rpl"}, 2u},
    {"replay-checkpoint-interval",
     {"--replay-checkpoint-interval", "160000"},
     2u},
    {"replay-play trio",
     {"--replay-play", "/nonexistent/r.rpl", "--replay-play-size", "100",
      "--replay-play-sha256", hex64},
     6u},
    {"replay-report", {"--replay-report", "r.rpr"}, 2u},
    /* Complete families: the same row without the state flags is not a usage
     * refusal. */
    {"save family",
     {"--save-kind", "eeprom4k", "--save-format", "raw-v1",
      "--save-export-leaf", "s.eep"},
     6u},
    {"replay record family",
     {"--replay-record", "r.rpl", "--replay-checkpoint-interval", "160000"},
     4u},
    {"replay play family",
     {"--replay-play", "/nonexistent/r.rpl", "--replay-play-size", "100",
      "--replay-play-sha256", hex64, "--replay-report", "r.rpr"},
     8u},
};

#define COMPLETE_FAMILY_FIRST 8u

/* One run: the state flags plus a row, in a fresh directory that must stay
 * empty, refused with usage before the publisher ever opens. */
static void producer_row_refused(const char *rom, const family_row *row) {
  char dir[PATH_MAX];
  char first[256];
  const char *args[16];
  size_t count = 0u;
  size_t i;
  run_spec spec = {0};
  run_out run;
  for (i = 0u; i < sizeof state_args / sizeof state_args[0]; i++) {
    args[count++] = state_args[i];
  }
  for (i = 0u; i < row->count; i++) args[count++] = row->args[i];
  CHECK(make_artifact_dir(dir));
  spec.supervisor = g_supervisor;
  spec.fixture = rom;
  spec.bound = 600000u;
  spec.artifact_dir = dir;
  spec.artifact_max = ARTIFACT_MAX;
  spec.extra = args;
  spec.extra_count = count;
  CHECK(run_supervised(&spec, &run));
  if (strcmp(run.detail, "usage") != 0) {
    (void)fprintf(stderr, "one-producer row %s: detail %s\n", row->label,
                  run.detail);
  }
  expect_usage(&run);
  CHECK(list_directory(dir, first) == 0u);
  remove_tree(dir);
}

/* The same family row without the state flags is not a usage refusal. */
static void producer_row_baseline(const char *rom, const family_row *row) {
  char dir[PATH_MAX];
  run_spec spec = {0};
  run_out run;
  CHECK(make_artifact_dir(dir));
  spec.supervisor = g_supervisor;
  spec.fixture = rom;
  spec.bound = 600000u;
  spec.artifact_dir = dir;
  spec.artifact_max = ARTIFACT_MAX;
  spec.extra = row->args;
  spec.extra_count = row->count;
  CHECK(run_supervised(&spec, &run));
  if (strcmp(run.detail, "usage") == 0) {
    (void)fprintf(stderr, "baseline row %s is a usage refusal\n", row->label);
  }
  CHECK(strcmp(run.detail, "usage") != 0);
  remove_tree(dir);
}

static int case_export_refusals(void) {
  char dir[PATH_MAX];
  char rom[PATH_MAX];
  char hello[PATH_MAX];
  char first[256];
  char leaf[PATH_MAX];
  char other[PATH_MAX];
  run_spec spec = {0};
  run_out run;
  uint64_t size = 0u;
  size_t i;
  CHECK(fixture_path(rom, "phase09-admit-si-v1"));
  CHECK(fixture_path(hello, "hello"));
  /* The accepted baseline of this setup. */
  CHECK(run_accepted(dir, ARTIFACT_MAX, &run, &size));
  remove_tree(dir);

  spec.supervisor = g_supervisor;
  spec.fixture = rom;
  spec.bound = 600000u;
  spec.artifact_max = ARTIFACT_MAX;

  /* Without the artifact pair. */
  spec.artifact_dir = NULL;
  spec.extra = state_args;
  spec.extra_count = sizeof state_args / sizeof state_args[0];
  CHECK(run_supervised(&spec, &run));
  expect_usage(&run);

  /* Without the format flag, and with a wrong format string. */
  CHECK(make_artifact_dir(dir));
  spec.artifact_dir = dir;
  spec.extra_count = 2u;
  CHECK(run_supervised(&spec, &run));
  expect_usage(&run);
  CHECK(list_directory(dir, first) == 0u);
  {
    const char *wrong[] = {"--state-export", STATE_LEAF,
                           "--state-export-format", "t64.state-container.v2"};
    spec.extra = wrong;
    spec.extra_count = 4u;
    CHECK(run_supervised(&spec, &run));
    expect_usage(&run);
    CHECK(list_directory(dir, first) == 0u);
  }
  /* With raw capture. */
  {
    char capture_dir[PATH_MAX];
    const char *with_raw[6];
    CHECK(make_artifact_dir(capture_dir));
    with_raw[0] = state_args[0];
    with_raw[1] = state_args[1];
    with_raw[2] = state_args[2];
    with_raw[3] = state_args[3];
    with_raw[4] = "--raw-capture-dir";
    with_raw[5] = capture_dir;
    spec.extra = with_raw;
    spec.extra_count = 6u;
    CHECK(run_supervised(&spec, &run));
    expect_usage(&run);
    CHECK(list_directory(dir, first) == 0u);
    CHECK(list_directory(capture_dir, first) == 0u);
    remove_tree(capture_dir);
  }
  remove_tree(dir);

  /* A leaf with another extension: the publisher is never opened for it and
   * the run ends at tick 0 with the named detail. */
  {
    const char *other_extension[] = {"--state-export", "x.stx",
                                     "--state-export-format", STATE_FORMAT};
    CHECK(make_artifact_dir(dir));
    spec.artifact_dir = dir;
    spec.extra = other_extension;
    spec.extra_count = 4u;
    CHECK(run_supervised(&spec, &run));
    expect_state_export(&run, "state-export-open");
    CHECK(run.guest_tick == 0u);
    CHECK(list_directory(dir, first) == 0u);
    remove_tree(dir);
  }
  /* An existing target leaf keeps its original bytes. */
  {
    static const uint8_t original[] = "private bytes that must survive";
    uint8_t after[64];
    size_t after_length = 0u;
    CHECK(make_artifact_dir(dir));
    CHECK(join_path(leaf, sizeof leaf, dir, STATE_LEAF));
    CHECK(write_file(leaf, original, sizeof original - 1u));
    spec.artifact_dir = dir;
    spec.extra = state_args;
    spec.extra_count = sizeof state_args / sizeof state_args[0];
    CHECK(run_supervised(&spec, &run));
    expect_state_export(&run, "state-export-open");
    CHECK(run.guest_tick == 0u);
    CHECK(list_directory(dir, first) == 1u);
    CHECK(strcmp(first, STATE_LEAF) == 0);
    CHECK(read_file(leaf, after, sizeof after, &after_length));
    CHECK(after_length == sizeof original - 1u);
    CHECK(memcmp(after, original, sizeof original - 1u) == 0);
    remove_tree(dir);
  }
  /* A nonexistent directory is the existing artifact preflight refusal (the
   * shared prelaunch path), before the state publisher is reached. */
  {
    CHECK(make_artifact_dir(dir));
    CHECK(join_path(other, sizeof other, dir, "absent"));
    spec.artifact_dir = other;
    CHECK(run_supervised(&spec, &run));
    expect_supervised_preflight_refusal(&run);
    CHECK(list_directory(dir, first) == 0u);
    CHECK(run_runner_direct(rom, PROFILE_STEM, "600000", other, "4096",
                            state_args,
                            sizeof state_args / sizeof state_args[0], &run));
    expect_refusal(&run);
    remove_tree(dir);
  }
  /* A profile whose state serialization version is not 25: the existing
   * prelaunch refusal (supervised: the report-error classification; the
   * runner binary reports the refusal record itself). */
  {
    CHECK(make_artifact_dir(dir));
    spec.fixture = hello;
    spec.artifact_dir = dir;
    CHECK(run_supervised(&spec, &run));
    expect_supervised_preflight_refusal(&run);
    CHECK(list_directory(dir, first) == 0u);
    CHECK(run_runner_direct(hello, "ntsc-cart-4m-openipl3-warm-v1", "600000",
                            dir, "4096", state_args,
                            sizeof state_args / sizeof state_args[0], &run));
    expect_refusal(&run);
    CHECK(list_directory(dir, first) == 0u);
    remove_tree(dir);
    spec.fixture = rom;
  }
  /* One artifact producer per run: the state flags are refused together with
   * each save and replay flag family before the publisher opens. */
  for (i = 0u; i < sizeof producer_rows / sizeof producer_rows[0]; i++) {
    if (i >= COMPLETE_FAMILY_FIRST) producer_row_baseline(rom, &producer_rows[i]);
    producer_row_refused(rom, &producer_rows[i]);
  }
  /* The runner binary repeats the rule for its forwarded child options. */
  {
    static const char *const child_save[] = {"--save-kind", "eeprom4k",
                                             "--save-format", "raw-v1",
                                             "--save-export-leaf", "s.eep"};
    static const char *const child_replay[] = {"--replay-record", "r.rpl",
                                               "--replay-checkpoint-interval",
                                               "160000"};
    const char *const *families[2] = {child_save, child_replay};
    const size_t counts[2] = {6u, 4u};
    size_t f;
    for (f = 0u; f < 2u; f++) {
      const char *args[16];
      size_t count = 0u;
      CHECK(make_artifact_dir(dir));
      /* Baseline: the family alone is not a usage refusal. */
      CHECK(run_runner_direct(rom, PROFILE_STEM, "600000", dir, "33554432",
                              families[f], counts[f], &run));
      CHECK(strcmp(run.detail, "usage") != 0);
      remove_tree(dir);
      CHECK(make_artifact_dir(dir));
      for (i = 0u; i < sizeof state_args / sizeof state_args[0]; i++) {
        args[count++] = state_args[i];
      }
      for (i = 0u; i < counts[f]; i++) args[count++] = families[f][i];
      CHECK(run_runner_direct(rom, PROFILE_STEM, "600000", dir, "33554432",
                              args, count, &run));
      CHECK(run.parsed);
      CHECK(strcmp(run.detail, "usage") == 0);
      CHECK(run.guest_tick == 0u);
      CHECK(list_directory(dir, first) == 0u);
      remove_tree(dir);
    }
  }
  /* The input triple is not an artifact producer: with it the same run is
   * accepted and publishes the state file. */
  {
    char input[PATH_MAX];
    char input_hex[65];
    char input_size[24];
    struct stat info;
    const char *args[10];
    size_t count = 0u;
    uint64_t input_blob = 0u;
    CHECK(input_path(input));
    CHECK(sha256_file_hex(input, input_hex));
    CHECK(stat(input, &info) == 0);
    (void)snprintf(input_size, sizeof input_size, "%llu",
                   (unsigned long long)info.st_size);
    for (i = 0u; i < sizeof state_args / sizeof state_args[0]; i++) {
      args[count++] = state_args[i];
    }
    args[count++] = "--input-size";
    args[count++] = input_size;
    args[count++] = "--input-sha256";
    args[count++] = input_hex;
    CHECK(make_artifact_dir(dir));
    spec.artifact_dir = dir;
    spec.extra = args;
    spec.extra_count = count;
    CHECK(run_supervised(&spec, &run));
    expect_pass(&run);
    CHECK(list_directory(dir, first) == 1u);
    CHECK(join_path(leaf, sizeof leaf, dir, STATE_LEAF));
    CHECK(stat_private_file(leaf, &input_blob));
    CHECK(input_blob == g_state_size || g_state_size == 0u);
    remove_tree(dir);
  }
  return g_failures == 0 ? 0 : 1;
}

/* ---- case export-nonterminal ---------------------------------------------- */

static int case_export_nonterminal(void) {
  char dir[PATH_MAX];
  char rom[PATH_MAX];
  char first[256];
  char leaf[PATH_MAX];
  uint64_t size = 0u;
  run_spec spec = {0};
  run_out run;
  CHECK(fixture_path(rom, "phase09-admit-long-si-v1"));
  spec.supervisor = g_supervisor;
  spec.fixture = rom;
  spec.artifact_max = ARTIFACT_MAX;
  spec.extra = state_args;
  spec.extra_count = sizeof state_args / sizeof state_args[0];

  /* Baseline: with room to finish, the long ROM exports and passes. */
  CHECK(make_artifact_dir(dir));
  spec.artifact_dir = dir;
  spec.bound = 1200000u;
  CHECK(run_supervised(&spec, &run));
  expect_pass(&run);
  CHECK(list_directory(dir, first) == 1u);
  CHECK(join_path(leaf, sizeof leaf, dir, STATE_LEAF));
  CHECK(stat_private_file(leaf, &size));
  CHECK(size > DEFAULT_CHILD_LIMIT);
  remove_tree(dir);

  /* The same ROM stopped by the bound before authored completion: nothing is
   * published and the directory stays empty. */
  CHECK(make_artifact_dir(dir));
  spec.artifact_dir = dir;
  spec.bound = 600000u;
  CHECK(run_supervised(&spec, &run));
  expect_state_export(&run, "state-export-incomplete-terminal");
  CHECK(list_directory(dir, first) == 0u);
  remove_tree(dir);
  return g_failures == 0 ? 0 : 1;
}

/* ---- case export-decode ---------------------------------------------------- */

/* Writes a copy of the blob with one byte changed. */
static bool write_mutated_copy(const char *blob_path, const char *copy_path,
                               size_t offset) {
  struct stat info;
  uint8_t *bytes;
  size_t length = 0u;
  bool ok;
  if (stat(blob_path, &info) != 0 || info.st_size <= 0) return false;
  bytes = (uint8_t *)malloc((size_t)info.st_size);
  if (bytes == NULL) return false;
  ok = read_file(blob_path, bytes, (size_t)info.st_size, &length) &&
       length == (size_t)info.st_size && offset < length;
  if (ok) {
    bytes[offset] = (uint8_t)(bytes[offset] ^ 1u);
    ok = write_file(copy_path, bytes, length);
  }
  free(bytes);
  return ok;
}

/* The first u64 body offset of table entry `index` (literal layout: the
 * table at 0x120, 56 bytes an entry, the body offset at +8). */
static bool entry_body_offset(const char *blob_path, unsigned index,
                              size_t *offset_out) {
  uint8_t head[0x120 + 56u * 12u];
  size_t length = 0u;
  const size_t at = 0x120u + 56u * index + 8u;
  uint64_t value = 0u;
  unsigned i;
  if (!read_file(blob_path, head, sizeof head, &length) || length < at + 8u) {
    return false;
  }
  for (i = 0u; i < 8u; i++) value = (value << 8) | head[at + i];
  *offset_out = (size_t)value;
  return true;
}

static int case_export_decode(void) {
  char dir[PATH_MAX];
  char rom[PATH_MAX];
  char blob[PATH_MAX];
  char copy[PATH_MAX];
  run_out run;
  uint64_t size = 0u;
  decoded_blob decoded;
  size_t body = 0u;
  CHECK(fixture_path(rom, "phase09-admit-si-v1"));
  /* The accepted baseline of this setup: the real blob decodes. */
  CHECK(run_accepted(dir, ARTIFACT_MAX, &run, &size));
  CHECK(join_path(blob, sizeof blob, dir, STATE_LEAF));
  CHECK(join_path(copy, sizeof copy, dir, "mutated.sta"));
  check_decoded_against(blob, rom, &run, size);
  /* One RDRAM body byte flipped (index 2 is section 3): its own kind. */
  CHECK(entry_body_offset(blob, 2u, &body));
  CHECK(write_mutated_copy(blob, copy, body + 100u));
  CHECK(decode_blob(copy, &decoded) == 1);
  CHECK(strcmp(decoded.status, "refused") == 0);
  CHECK(strcmp(decoded.kind, "section-hash") == 0);
  /* The last trailer byte flipped. */
  CHECK(write_mutated_copy(blob, copy, (size_t)size - 1u));
  CHECK(decode_blob(copy, &decoded) == 1);
  CHECK(strcmp(decoded.kind, "trailer") == 0);
  /* A magic byte flipped. */
  CHECK(write_mutated_copy(blob, copy, 0u));
  CHECK(decode_blob(copy, &decoded) == 1);
  CHECK(strcmp(decoded.kind, "magic") == 0);
  /* A missing file is a usage error. */
  CHECK(decode_blob("/nonexistent/blob.sta", &decoded) == 2);
  remove_tree(dir);
  return g_failures == 0 ? 0 : 1;
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

/* The accepted baseline of this case is the real pass record; the record
 * under test is the real state-export-cap record of the supervised run whose
 * artifact maximum is one byte below the state size. */
static int case_checker_vocabulary(void) {
  char dir[PATH_MAX];
  char rom[PATH_MAX];
  char bogus_path[PATH_MAX];
  char pass_path[PATH_MAX];
  char diagnostic[160];
  run_spec spec = {0};
  run_out pass_run;
  run_out cap_run;
  uint64_t size = 0u;
  CHECK(ensure_state_size());
  CHECK(run_accepted(dir, g_state_size, &pass_run, &size));
  CHECK(run_checker(pass_run.result_path, diagnostic) == 0);
  remove_tree(dir);
  CHECK(fixture_path(rom, "phase09-admit-si-v1"));
  CHECK(make_artifact_dir(dir));
  spec.supervisor = g_supervisor;
  spec.fixture = rom;
  spec.bound = 600000u;
  spec.artifact_dir = dir;
  spec.artifact_max = g_state_size - 1u;
  spec.extra = state_args;
  spec.extra_count = sizeof state_args / sizeof state_args[0];
  CHECK(run_supervised(&spec, &cap_run));
  expect_state_export(&cap_run, "state-export-cap");
  CHECK(run_checker(cap_run.result_path, diagnostic) == 0);
  CHECK(join_path(bogus_path, sizeof bogus_path, cap_run.dir, "bogus.json"));
  CHECK(join_path(pass_path, sizeof pass_path, cap_run.dir,
                  "outcome-pass.json"));
  /* An unregistered state-export name is outside the closed enum. */
  CHECK(write_replaced(cap_run.result_path, bogus_path, "state-export-cap",
                       "state-export-bogus"));
  CHECK(run_checker(bogus_path, diagnostic) == 1);
  CHECK(strncmp(diagnostic, "R-ENUM ", 7u) == 0);
  /* The registered name with the outcome replaced by pass is inconsistent. */
  CHECK(write_replaced(cap_run.result_path, pass_path,
                       "\"outcome\":\"harness-error\"",
                       "\"outcome\":\"pass\""));
  CHECK(run_checker(pass_path, diagnostic) == 1);
  CHECK(strncmp(diagnostic, "R-DETAIL-OUTCOME ", 17u) == 0);
  remove_tree(dir);
  return g_failures == 0 ? 0 : 1;
}

/* ---- dispatch ----------------------------------------------------------- */

/* ======================================================================= */
/* State import through the verified-input path (plan 09-09, suite import)  */
/* ======================================================================= */

#define LONG_ROM "phase09-admit-long-si-v1"
#define LONG_BOUND 1200000u
#define IMPORT_TICK 700000u
#define BUILD_ID_OFFSET 0xA0u
#define TRAILER_BYTES 32u

static bool host_allocate(void *context, size_t size, size_t alignment,
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

static void host_release(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

/* One state file on disk: the private path, the exact size and the SHA-256 the
 * supervisor verifies, and the bytes for the test-side edits. */
typedef struct {
  char path[PATH_MAX];
  char hex[65];
  char size_text[24];
  size_t size;
  uint8_t *bytes;
} state_file;

static void state_file_free(state_file *file) {
  free(file->bytes);
  file->bytes = NULL;
}

/* Re-sign the container trailer (SHA-256 of every byte before it; the layout
 * offset is the literal of docs/spec/state-container.md). */
static void resign_trailer(uint8_t *bytes, size_t size) {
  t64_sha256_ctx context;
  uint8_t digest[32];
  t64_sha256_init(&context);
  t64_sha256_update(&context, bytes, size - TRAILER_BYTES);
  t64_sha256_final(&context, digest);
  memcpy(bytes + size - TRAILER_BYTES, digest, TRAILER_BYTES);
}

/* Write the bytes 0600 into the directory and compute the supervisor triple. */
static bool state_file_commit(const char *dir, const char *name,
                              uint8_t *bytes, size_t size,
                              state_file *out) {
  t64_sha256_ctx context;
  uint8_t digest[32];
  memset(out, 0, sizeof *out);
  out->bytes = bytes;
  out->size = size;
  if (!join_path(out->path, sizeof out->path, dir, name) ||
      !write_file(out->path, bytes, size)) {
    return false;
  }
  t64_sha256_init(&context);
  t64_sha256_update(&context, bytes, size);
  t64_sha256_final(&context, digest);
  t64_sha256_hex(digest, out->hex);
  (void)snprintf(out->size_text, sizeof out->size_text, "%llu",
                 (unsigned long long)size);
  return true;
}

/* A state exported in process from the same core library at a tick inside the
 * long spin. (The runner publishes a state only at authored completion, so a
 * mid-run state comes from the core API.) */
static bool native_state_file(const char *dir, uint64_t tick,
                              state_file *out) {
  static const t64_host_allocator host = {host_allocate, host_release, NULL};
  char rom_path[PATH_MAX];
  uint8_t *rom = (uint8_t *)malloc(1u << 20);
  size_t rom_length = 0u;
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  size_t required = 0u;
  size_t written = 0u;
  uint8_t *blob = NULL;
  bool ok = false;
  memset(out, 0, sizeof *out);
  if (rom == NULL || !fixture_path(rom_path, LONG_ROM) ||
      !read_file(rom_path, rom, 1u << 20, &rom_length) || rom_length == 0u ||
      t64_profile_lookup(PROFILE_STEM, &profile) != T64_STATUS_OK ||
      t64_machine_create(&host, profile, &machine) != T64_STATUS_OK ||
      t64_machine_admit_rom(machine, rom, rom_length) != T64_STATUS_OK) {
    goto done;
  }
  {
    uint64_t reached = 0u;
    while (reached < tick) {
      t64_stop_request request;
      t64_stop_result stop = {0u, 0u};
      request.kind = T64_STOP_GUEST_TICK;
      request.value = tick - reached > 2000u ? reached + 2000u : tick;
      if (t64_machine_advance(machine, &request, &stop) != T64_STATUS_OK ||
          stop.reason != T64_STOP_REASON_BOUND) {
        goto done;
      }
      reached = stop.tick;
    }
  }
  if (t64_machine_state_export(machine, NULL, 0u, &required) !=
          T64_STATUS_BUFFER_TOO_SMALL ||
      required == 0u || (blob = (uint8_t *)malloc(required)) == NULL ||
      t64_machine_state_export(machine, blob, required, &written) !=
          T64_STATUS_OK ||
      written != required) {
    goto done;
  }
  ok = state_file_commit(dir, "native.sta", blob, written, out);
  if (ok) blob = NULL;
done:
  free(blob);
  free(rom);
  if (machine != NULL) t64_machine_destroy(machine);
  return ok;
}

/* The supervised run with the state import triple of a file. */
static bool run_with_state(const state_file *file, const char *rom,
                           uint64_t bound, const char *const *more,
                           size_t more_count, run_out *run) {
  const char *args[MAX_ARGS];
  size_t count = 0u;
  size_t i;
  run_spec spec = {0};
  args[count++] = "--state-import";
  args[count++] = file->path;
  args[count++] = "--state-import-size";
  args[count++] = file->size_text;
  args[count++] = "--state-import-sha256";
  args[count++] = file->hex;
  for (i = 0u; i < more_count && count < MAX_ARGS - 8u; i++) {
    args[count++] = more[i];
  }
  spec.supervisor = g_supervisor;
  spec.fixture = rom;
  spec.bound = bound;
  spec.extra = args;
  spec.extra_count = count;
  return run_supervised(&spec, run);
}

/* An unlinked read-only descriptor of a copy of the file (the form of the
 * verified snapshot the child reads) for the direct runner. */
static int open_state_descriptor(const state_file *file, const char *dir) {
  char copy[PATH_MAX];
  int fd;
  if (!join_path(copy, sizeof copy, dir, "direct.sta") ||
      !write_file(copy, file->bytes, file->size)) {
    return -1;
  }
  fd = open(copy, O_RDONLY);
  (void)unlink(copy);
  return fd;
}

/* The direct runner with a state descriptor and more options. */
static bool run_direct_with_state(const state_file *file, const char *rom,
                                  const char *profile, const char *dir,
                                  const char *const *more, size_t more_count,
                                  run_out *run) {
  const char *args[24];
  char fd_text[24];
  size_t count = 0u;
  size_t i;
  int fd = open_state_descriptor(file, dir);
  if (fd <= STDERR_FILENO) return false;
  (void)snprintf(fd_text, sizeof fd_text, "%d", fd);
  args[count++] = "--state-import-fd";
  args[count++] = fd_text;
  for (i = 0u; i < more_count && count < 22u; i++) args[count++] = more[i];
  {
    bool ok = run_runner_direct(rom, profile, "600000", NULL, NULL, args,
                                count, run);
    (void)close(fd);
    return ok;
  }
}

/* The diagnostic stream of a direct run holds exactly the closed reason line:
 * no blob byte, no path. */
static void expect_diagnostic(const run_out *run, const char *want,
                              const state_file *file) {
  uint8_t bytes[512];
  size_t length = 0u;
  char expected[128];
  CHECK(read_file(run->err_path, bytes, sizeof bytes - 1u, &length));
  bytes[length] = 0u;
  (void)snprintf(expected, sizeof expected, "state-import refused: %s\n", want);
  CHECK(length == strlen(expected) && memcmp(bytes, expected, length) == 0);
  CHECK(strstr((const char *)bytes, file->path) == NULL);
  CHECK(strstr((const char *)bytes, "T64STATE") == NULL);
}

/* Guest ticks executed: the run reached the guest (an accepted import). */
static void expect_import_ran(const run_out *run) {
  CHECK(run->parsed);
  CHECK(run->guest_tick > 0u);
}

/* Load a published state file for the import triple. */
static bool state_file_load(const char *path, state_file *out) {
  struct stat info;
  uint8_t *bytes;
  size_t length = 0u;
  t64_sha256_ctx context;
  uint8_t digest[32];
  memset(out, 0, sizeof *out);
  if (stat(path, &info) != 0 || info.st_size <= 0) return false;
  bytes = (uint8_t *)malloc((size_t)info.st_size);
  if (bytes == NULL ||
      !read_file(path, bytes, (size_t)info.st_size, &length) ||
      length != (size_t)info.st_size) {
    free(bytes);
    return false;
  }
  out->bytes = bytes;
  out->size = length;
  (void)snprintf(out->path, sizeof out->path, "%s", path);
  t64_sha256_init(&context);
  t64_sha256_update(&context, bytes, length);
  t64_sha256_final(&context, digest);
  t64_sha256_hex(digest, out->hex);
  (void)snprintf(out->size_text, sizeof out->size_text, "%llu",
                 (unsigned long long)length);
  return true;
}

/* The evidence limit of a resumed run, asserted: its verdict is derived from
 * milestones and a retirement count observed through the run's own trace, which
 * a resumed run does not have, so it can never report pass; it ends in the
 * closed harness-error host-error record, while the guest tick, the final state
 * digest and the log bytes continue exactly as the uninterrupted run. */
static void expect_resumed_like(const run_out *resumed, const run_out *a) {
  char diagnostic[160];
  CHECK(resumed->parsed);
  CHECK(run_checker(resumed->result_path, diagnostic) == 0);
  CHECK(strcmp(resumed->outcome, "harness-error") == 0);
  CHECK(strcmp(resumed->stop_kind, "host-error") == 0);
  CHECK(strcmp(resumed->detail, "host-error") == 0);
  CHECK(resumed->guest_tick == a->guest_tick);
  CHECK(resumed->guest_tick > IMPORT_TICK);
  CHECK(strcmp(resumed->state_digest, a->state_digest) == 0);
  CHECK(resumed->log_bytes == a->log_bytes);
}

static int case_import_accepted_continues(void) {
  char rom[PATH_MAX];
  char dir[PATH_MAX];
  char artifact[PATH_MAX];
  char first[256];
  char leaf[PATH_MAX];
  state_file file;
  state_file published;
  run_spec spec = {0};
  run_out a;
  run_out b;
  run_out c;
  CHECK(fixture_path(rom, LONG_ROM));
  CHECK(make_run_dir(dir));
  /* run A: uninterrupted to authored completion */
  spec.supervisor = g_supervisor;
  spec.fixture = rom;
  spec.bound = LONG_BOUND;
  CHECK(run_supervised(&spec, &a));
  expect_pass(&a);
  CHECK(a.guest_tick > IMPORT_TICK);
  /* run C: a mid-run state (tick 700,000, inside the spin) through the
   * verified-input path resumes to the same completion tick, final state digest
   * and log bytes */
  CHECK(native_state_file(dir, IMPORT_TICK, &file));
  CHECK(run_with_state(&file, rom, LONG_BOUND, NULL, 0u, &c));
  expect_resumed_like(&c, &a);
  /* run B publishes the state at authored completion through the runner. That
   * state was exported after the runner drained the whole log (observer cursor
   * at 20 bytes), so a resumed run delivers no earlier byte and its record
   * (total 20, captured 0) disagrees with its own log: the supervisor ends it
   * in the closed report-error record. The limit is the default observer
   * restore of the import, asserted here by name and not hidden. */
  CHECK(make_artifact_dir(artifact));
  spec.artifact_dir = artifact;
  spec.artifact_max = ARTIFACT_MAX;
  spec.extra = state_args;
  spec.extra_count = sizeof state_args / sizeof state_args[0];
  CHECK(run_supervised(&spec, &b));
  expect_pass(&b);
  CHECK(list_directory(artifact, first) == 1u);
  CHECK(join_path(leaf, sizeof leaf, artifact, STATE_LEAF));
  CHECK(state_file_load(leaf, &published));
  CHECK(run_with_state(&published, rom, LONG_BOUND, NULL, 0u, &c));
  CHECK(c.parsed);
  CHECK(strcmp(c.outcome, "harness-error") == 0);
  CHECK(strcmp(c.stop_kind, "report-error") == 0);
  CHECK(strcmp(c.detail, "report-invalid") == 0);
  CHECK(c.guest_tick == 0u);
  state_file_free(&published);
  state_file_free(&file);
  remove_tree(artifact);
  remove_tree(dir);
  return g_failures == 0 ? 0 : 1;
}

/* ---- case import-refused-integrity -------------------------------------- */

static int case_import_refused_integrity(void) {
  char rom[PATH_MAX];
  char dir[PATH_MAX];
  state_file base;
  state_file flipped;
  uint8_t *copy;
  run_out run;
  CHECK(fixture_path(rom, LONG_ROM));
  CHECK(make_run_dir(dir));
  CHECK(native_state_file(dir, IMPORT_TICK, &base));
  /* accepted baseline in the same setup */
  CHECK(run_with_state(&base, rom, LONG_BOUND, NULL, 0u, &run));
  expect_import_ran(&run);
  /* one bit flipped in a section body; size and SHA-256 are recomputed over the
   * flipped file so the verified-input check passes and the importer's trailer
   * check is the refusal */
  copy = (uint8_t *)malloc(base.size);
  CHECK(copy != NULL);
  memcpy(copy, base.bytes, base.size);
  copy[5000] ^= 0x01u;
  CHECK(state_file_commit(dir, "flipped.sta", copy, base.size, &flipped));
  CHECK(run_with_state(&flipped, rom, LONG_BOUND, NULL, 0u, &run));
  expect_supervised_preflight_refusal(&run);
  /* the direct runner names the closed reason on its diagnostic stream */
  CHECK(run_direct_with_state(&flipped, rom, PROFILE_STEM, dir, NULL, 0u,
                              &run));
  expect_refusal(&run);
  expect_diagnostic(&run, "trailer-sha256", &flipped);
  state_file_free(&flipped);
  state_file_free(&base);
  remove_tree(dir);
  return g_failures == 0 ? 0 : 1;
}

/* ---- case import-refused-build ------------------------------------------ */

static int case_import_refused_build(void) {
  char rom[PATH_MAX];
  char dir[PATH_MAX];
  state_file base;
  state_file foreign;
  uint8_t *copy;
  run_out run;
  CHECK(fixture_path(rom, LONG_ROM));
  CHECK(make_run_dir(dir));
  CHECK(native_state_file(dir, IMPORT_TICK, &base));
  CHECK(run_with_state(&base, rom, LONG_BOUND, NULL, 0u, &run));
  expect_import_ran(&run);
  /* one byte of the header build identity changed, the trailer re-signed with
   * the literal layout offsets, so the build binding is the refusal */
  copy = (uint8_t *)malloc(base.size);
  CHECK(copy != NULL);
  memcpy(copy, base.bytes, base.size);
  copy[BUILD_ID_OFFSET] ^= 0x20u;
  resign_trailer(copy, base.size);
  CHECK(state_file_commit(dir, "foreign.sta", copy, base.size, &foreign));
  CHECK(run_with_state(&foreign, rom, LONG_BOUND, NULL, 0u, &run));
  expect_supervised_preflight_refusal(&run);
  CHECK(run_direct_with_state(&foreign, rom, PROFILE_STEM, dir, NULL, 0u,
                              &run));
  expect_refusal(&run);
  expect_diagnostic(&run, "build-id", &foreign);
  state_file_free(&foreign);
  state_file_free(&base);
  remove_tree(dir);
  return g_failures == 0 ? 0 : 1;
}

/* ---- case import-option-refusals ---------------------------------------- */

static int case_import_option_refusals(void) {
  char rom[PATH_MAX];
  char dir[PATH_MAX];
  char artifact[PATH_MAX];
  char input_hex[65];
  char input_path_text[PATH_MAX];
  state_file base;
  state_file ghost;
  run_out run;
  size_t i;
  static const char zero_hex[] =
      "0000000000000000000000000000000000000000000000000000000000000000";
  const char *const save_import[] = {"--save-import", "/nonexistent/save.bin",
                                     "--save-import-size", "512",
                                     "--save-import-sha256", zero_hex};
  const char *const save_kind[] = {"--save-kind", "eeprom-4k"};
  const char *const save_format[] = {"--save-format", "raw"};
  const char *const save_leaf[] = {"--save-export-leaf", "save.eep"};
  const char *const replay_record[] = {"--replay-record", "r.rpl"};
  const char *const replay_interval[] = {"--replay-checkpoint-interval", "160"};
  const char *const replay_play[] = {"--replay-play", "/nonexistent/r.rpl",
                                     "--replay-play-size", "100",
                                     "--replay-play-sha256", zero_hex};
  const char *const replay_report[] = {"--replay-report", "r.rpr"};
  const char *const state_out[] = {"--state-export", STATE_LEAF,
                                   "--state-export-format", STATE_FORMAT};
  const struct {
    const char *name;
    const char *const *args;
    size_t count;
  } producers[] = {
      {"save import trio", save_import, 6u},
      {"save kind", save_kind, 2u},
      {"save format", save_format, 2u},
      {"save export leaf", save_leaf, 2u},
      {"replay record", replay_record, 2u},
      {"replay interval", replay_interval, 2u},
      {"replay play", replay_play, 6u},
      {"replay report", replay_report, 2u},
      {"state export", state_out, 4u},
  };
  CHECK(fixture_path(rom, LONG_ROM));
  CHECK(make_run_dir(dir));
  CHECK(native_state_file(dir, IMPORT_TICK, &base));
  /* the triple alone is accepted: the baseline of every row below */
  CHECK(run_with_state(&base, rom, LONG_BOUND, NULL, 0u, &run));
  expect_import_ran(&run);
  /* the ordering baseline: a nonexistent path with no conflicting flag reports
   * the snapshot refusal, so a pair refusal reported ahead of it was decided
   * before the snapshot was prepared, before any child and any guest tick */
  ghost = base;
  (void)snprintf(ghost.path, sizeof ghost.path, "%s",
                 "/nonexistent/import/state.sta");
  CHECK(run_with_state(&ghost, rom, LONG_BOUND, NULL, 0u, &run));
  expect_refusal(&run);
  /* one artifact producer per run: each earlier family with the triple */
  CHECK(make_artifact_dir(artifact));
  for (i = 0u; i < sizeof producers / sizeof producers[0]; i++) {
    const char *more[16];
    size_t count = 0u;
    size_t k;
    for (k = 0u; k < producers[i].count; k++) more[count++] = producers[i].args[k];
    more[count++] = "--artifact-dir";
    more[count++] = artifact;
    more[count++] = "--artifact-max-bytes";
    more[count++] = "33554432";
    CHECK(run_with_state(&ghost, rom, LONG_BOUND, more, count, &run));
    if (run.guest_tick != 0u || strcmp(run.detail, "usage") != 0) {
      (void)fprintf(stderr, "one-producer row %s: detail %s\n",
                    producers[i].name, run.detail);
    }
    expect_usage(&run);
  }
  /* a custom input schedule is not combined with a state import */
  CHECK(input_path(input_path_text) && sha256_file_hex(input_path_text,
                                                       input_hex));
  {
    const char *more[] = {"--input-size", "54", "--input-sha256", input_hex};
    CHECK(run_with_state(&ghost, rom, LONG_BOUND, more, 4u, &run));
    expect_usage(&run);
  }
  /* raw capture, the incomplete triples, the size and the hash grammar */
  {
    const char *raw[] = {"--raw-capture-dir", artifact};
    CHECK(run_with_state(&ghost, rom, LONG_BOUND, raw, 2u, &run));
    expect_usage(&run);
  }
  {
    run_spec spec = {0};
    const char *only_path[] = {"--state-import", "/nonexistent/x.sta"};
    const char *no_hash[] = {"--state-import", "/nonexistent/x.sta",
                             "--state-import-size", "100"};
    const char *no_size[] = {"--state-import", "/nonexistent/x.sta",
                             "--state-import-sha256", zero_hex};
    const char *zero_size[] = {"--state-import", "/nonexistent/x.sta",
                               "--state-import-size", "0",
                               "--state-import-sha256", zero_hex};
    const char *huge_size[] = {"--state-import", "/nonexistent/x.sta",
                               "--state-import-size", "67108865",
                               "--state-import-sha256", zero_hex};
    const char *upper_hash[] = {"--state-import", "/nonexistent/x.sta",
                                "--state-import-size", "100",
                                "--state-import-sha256",
                                "ABCDEF0000000000000000000000000000000000000000000000000000000000"};
    const char *short_hash[] = {"--state-import", "/nonexistent/x.sta",
                                "--state-import-size", "100",
                                "--state-import-sha256", "00ff"};
    const struct {
      const char *const *args;
      size_t count;
    } grammar[] = {{only_path, 2u}, {no_hash, 4u}, {no_size, 4u},
                   {zero_size, 6u}, {huge_size, 6u}, {upper_hash, 6u},
                   {short_hash, 6u}};
    spec.supervisor = g_supervisor;
    spec.fixture = rom;
    spec.bound = LONG_BOUND;
    for (i = 0u; i < sizeof grammar / sizeof grammar[0]; i++) {
      spec.extra = grammar[i].args;
      spec.extra_count = grammar[i].count;
      CHECK(run_supervised(&spec, &run));
      expect_usage(&run);
    }
  }
  /* the child repeats the family check: started directly with the descriptor
   * and a conflicting child option it refuses with guest tick 0 and no import
   * call (no reason line on the diagnostic stream) */
  {
    const char *conflict[] = {"--save-kind", "eeprom-4k"};
    CHECK(run_direct_with_state(&base, rom, PROFILE_STEM, dir, conflict, 2u,
                                &run));
    expect_usage(&run);
    {
      uint8_t bytes[64];
      size_t length = 1u;
      CHECK(read_file(run.err_path, bytes, sizeof bytes, &length));
      CHECK(length == 0u);
    }
  }
  /* a profile outside the composite family takes the existing refusal */
  {
    char audio_rom[PATH_MAX];
    CHECK(fixture_path(audio_rom, "audio-pcm"));
    CHECK(run_direct_with_state(
        &base, audio_rom,
        "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1", dir, NULL,
        0u, &run));
    expect_refusal(&run);
  }
  remove_tree(artifact);
  state_file_free(&base);
  remove_tree(dir);
  return g_failures == 0 ? 0 : 1;
}

/* ---- case import-bound-below-tick --------------------------------------- */

static int case_import_bound_below_tick(void) {
  char rom[PATH_MAX];
  char dir[PATH_MAX];
  state_file base;
  run_out run;
  CHECK(fixture_path(rom, LONG_ROM));
  CHECK(make_run_dir(dir));
  CHECK(native_state_file(dir, IMPORT_TICK, &base));
  /* accepted baseline: the same state with a bound above its tick */
  CHECK(run_with_state(&base, rom, LONG_BOUND, NULL, 0u, &run));
  expect_import_ran(&run);
  /* a bound below the state tick ends in the existing prelaunch refusal before
   * any advance (the observed supervised record: harness-error, report-error,
   * capture-invalid, guest tick 0), and the direct runner names the closed
   * reason */
  CHECK(run_with_state(&base, rom, 600000u, NULL, 0u, &run));
  expect_supervised_preflight_refusal(&run);
  CHECK(run_direct_with_state(&base, rom, PROFILE_STEM, dir, NULL, 0u, &run));
  expect_refusal(&run);
  expect_diagnostic(&run, "tick-above-bound", &base);
  state_file_free(&base);
  remove_tree(dir);
  return g_failures == 0 ? 0 : 1;
}

/* ======================================================================= */
/* Plan 09-10 tracer: one real mutated state blob through the real runner   */
/* (option --run-mutant ROW, not a case; provisional until Phase 10)        */
/* ======================================================================= */

#define TRACER_PRIVATE "build/gate/09-10-private"
#define TABLE_OFFSET 0x120u
#define TABLE_ENTRY_BYTES 56u
#define OBSERVER_SECTION_ID 16u
#define OBSERVER_BODY_BYTES 40u
#define OBSERVER_DRAIN_CURSOR_AT 0u /* the first of the five u64 values */

static uint32_t be32_at(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint64_t be64_at(const uint8_t *p) {
  return ((uint64_t)be32_at(p) << 32) | (uint64_t)be32_at(p + 4);
}

static void put_be64_at(uint8_t *p, uint64_t value) {
  unsigned i;
  for (i = 0u; i < 8u; i++) p[i] = (uint8_t)(value >> (56u - 8u * i));
}

/* The observer-range mutation with literal layout offsets (the table at 0x120,
 * 56 bytes an entry: id u32, flags u32, body offset u64, body length u64, body
 * SHA-256 at +24; section count u32 at 0x18): the output drain cursor, the
 * first of the five u64 values of the observer body, becomes the 64-bit
 * maximum, the member the inventory row observer-range edits (rule LEN_LE; the
 * review of plan 09-10 found the first version of this tracer editing the
 * video head, which reaches the rule INDEX_LT of another row); the observer
 * entry hash and the trailer are recomputed; the declared digest is untouched
 * (the observer body is outside the canonical region).
 * Returns the id of the entry that was changed, or 0 when the layout does not
 * match. */
static unsigned mutate_observer_range(uint8_t *bytes, size_t size) {
  uint32_t count;
  uint32_t i;
  if (size < TABLE_OFFSET + TRAILER_BYTES) return 0u;
  count = be32_at(bytes + 0x18u);
  for (i = 0u; i < count; i++) {
    const size_t at = TABLE_OFFSET + (size_t)TABLE_ENTRY_BYTES * i;
    uint64_t body;
    uint64_t length;
    t64_sha256_ctx context;
    if (at + TABLE_ENTRY_BYTES > size) return 0u;
    if (be32_at(bytes + at) != OBSERVER_SECTION_ID) continue;
    body = be64_at(bytes + at + 8u);
    length = be64_at(bytes + at + 16u);
    if (be32_at(bytes + at + 4u) != 1u || length != OBSERVER_BODY_BYTES ||
        body + length + TRAILER_BYTES != (uint64_t)size) {
      return 0u;
    }
    put_be64_at(bytes + body + OBSERVER_DRAIN_CURSOR_AT, UINT64_MAX);
    t64_sha256_init(&context);
    t64_sha256_update(&context, bytes + body, (size_t)length);
    t64_sha256_final(&context, bytes + at + 24u);
    resign_trailer(bytes, size);
    return OBSERVER_SECTION_ID;
  }
  return 0u;
}

/* The body offset of the observer entry (0 when the layout does not match). */
static uint64_t observer_body_offset(const uint8_t *bytes, size_t size) {
  uint32_t count;
  uint32_t i;
  if (size < TABLE_OFFSET + TRAILER_BYTES) return 0u;
  count = be32_at(bytes + 0x18u);
  for (i = 0u; i < count; i++) {
    const size_t at = TABLE_OFFSET + (size_t)TABLE_ENTRY_BYTES * i;
    if (at + TABLE_ENTRY_BYTES > size) return 0u;
    if (be32_at(bytes + at) == OBSERVER_SECTION_ID) {
      return be64_at(bytes + at + 8u);
    }
  }
  return 0u;
}

/* One inventory row: name :: status :: reason :: section id :: rule. The
 * expectation comes from the file named by T64_STATE_MUTANT_INVENTORY, never
 * from the production reader. */
typedef struct {
  char status[48];
  char reason[48];
  char rule[48];
  unsigned section;
} inventory_row;

static bool inventory_load(const char *name, inventory_row *out) {
  const char *path = getenv("T64_STATE_MUTANT_INVENTORY");
  FILE *file;
  char line[256];
  const size_t name_length = strlen(name);
  bool found = false;
  if (path == NULL || path[0] == '\0') return false;
  file = fopen(path, "r");
  if (file == NULL) return false;
  while (!found && fgets(line, sizeof line, file) != NULL) {
    char *fields[5];
    char *cursor = line;
    size_t n = 0u;
    size_t length = strlen(line);
    while (length > 0u && (line[length - 1u] == '\n' || line[length - 1u] == '\r')) {
      line[--length] = '\0';
    }
    if (strncmp(line, name, name_length) != 0 ||
        strncmp(line + name_length, " :: ", 4u) != 0) {
      continue;
    }
    while (n < 5u) {
      char *sep = strstr(cursor, " :: ");
      fields[n++] = cursor;
      if (sep == NULL) break;
      *sep = '\0';
      cursor = sep + 4;
    }
    if (n == 5u && strlen(fields[1]) < sizeof out->status &&
        strlen(fields[2]) < sizeof out->reason &&
        strlen(fields[4]) < sizeof out->rule) {
      (void)snprintf(out->status, sizeof out->status, "%s", fields[1]);
      (void)snprintf(out->reason, sizeof out->reason, "%s", fields[2]);
      (void)snprintf(out->rule, sizeof out->rule, "%s", fields[4]);
      out->section = (unsigned)strtoul(fields[3], NULL, 10);
      found = true;
    }
  }
  (void)fclose(file);
  return found;
}

/* The closed reason name of the inventory spelling (OBSERVER_RANGE) in the
 * diagnostic spelling of the runner (observer-range). */
static void reason_to_diagnostic(const char *reason, char *out, size_t capacity) {
  size_t i;
  for (i = 0u; reason[i] != '\0' && i + 1u < capacity; i++) {
    char c = reason[i];
    out[i] = c == '_' ? '-' : (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
  }
  out[i] = '\0';
}

/* The reason the child printed (inventory spelling), or "none". */
static void diagnostic_reason(const run_out *run, char *out, size_t capacity) {
  uint8_t bytes[512];
  size_t length = 0u;
  const char prefix[] = "state-import refused: ";
  const char *at;
  size_t i = 0u;
  (void)snprintf(out, capacity, "none");
  if (!read_file(run->err_path, bytes, sizeof bytes - 1u, &length)) return;
  bytes[length] = 0u;
  at = strstr((const char *)bytes, prefix);
  if (at == NULL) return;
  at += sizeof prefix - 1u;
  while (*at != '\0' && *at != '\n' && i + 1u < capacity) {
    char c = *at++;
    out[i++] = c == '-' ? '_' : (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
  }
  out[i] = '\0';
}

/* A new private file: 0600, no-replace. */
static bool write_new_private(const char *path, const uint8_t *bytes,
                              size_t length) {
  int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
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

/* The untracked private directory of the tracer (owner only). */
static bool tracer_private_dir(char out[PATH_MAX]) {
  struct stat info;
  if (!join_path(out, PATH_MAX, g_root, TRACER_PRIVATE)) return false;
  if (mkdir(out, 0700) != 0 && errno != EEXIST) return false;
  return lstat(out, &info) == 0 && S_ISDIR(info.st_mode) &&
         info.st_uid == geteuid() && (info.st_mode & 077) == 0;
}

/* The direct runner with a state descriptor at an explicit guest-tick bound. */
static bool run_direct_state_at(const state_file *file, const char *rom,
                                const char *dir, const char *bound_text,
                                run_out *run) {
  const char *args[4];
  char fd_text[24];
  int fd = open_state_descriptor(file, dir);
  bool ok;
  if (fd <= STDERR_FILENO) return false;
  (void)snprintf(fd_text, sizeof fd_text, "%d", fd);
  args[0] = "--state-import-fd";
  args[1] = fd_text;
  ok = run_runner_direct(rom, PROFILE_STEM, bound_text, NULL, NULL, args, 2u,
                         run);
  (void)close(fd);
  return ok;
}

/* The closed names of the tuple a tracer row can carry, in the spelling of the
 * inventory (docs/spec/state-mutants.txt). The tracer calls the importer of the
 * same core library in process on the exact blob the runner child imports, so
 * the full tuple (status, reason, section, rule) is observed and compared with
 * the row; the child's diagnostic line carries only the reason. */
typedef struct {
  const char *name;
  uint32_t value;
} tuple_name;

static const tuple_name tuple_statuses[] = {
    {"OK", (uint32_t)T64_STATUS_OK},
    {"STATE_RANGE", (uint32_t)T64_STATUS_STATE_RANGE}};
static const tuple_name tuple_reasons[] = {
    {"NONE", T64_STATE_REASON_NONE},
    {"OBSERVER_RANGE", T64_STATE_REASON_OBSERVER_RANGE}};
static const tuple_name tuple_rules[] = {
    {"NONE", T64_STATE_RULE_NONE},
    {"COUNT_CAP", T64_STATE_RULE_COUNT_CAP},
    {"INDEX_LT", T64_STATE_RULE_INDEX_LT},
    {"LEN_LE", T64_STATE_RULE_LEN_LE}};

static bool tuple_value(const tuple_name *table, size_t count, const char *name,
                        uint32_t *out) {
  size_t i;
  for (i = 0u; i < count; i++) {
    if (strcmp(table[i].name, name) == 0) {
      *out = table[i].value;
      return true;
    }
  }
  return false;
}

static const char *tuple_label(const tuple_name *table, size_t count,
                               uint32_t value) {
  size_t i;
  for (i = 0u; i < count; i++) {
    if (table[i].value == value) return table[i].name;
  }
  return "other";
}

/* Import the blob into a fresh machine of the runner row (the stem profile and
 * the long admit ROM) and return the closed tuple. */
static bool inprocess_import(const uint8_t *blob, size_t size,
                             t64_status *status_out,
                             t64_state_import_report *report_out) {
  static const t64_host_allocator host = {host_allocate, host_release, NULL};
  char rom_path[PATH_MAX];
  uint8_t *rom = (uint8_t *)malloc(1u << 20);
  size_t rom_length = 0u;
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  bool ok = false;
  memset(report_out, 0, sizeof *report_out);
  if (rom != NULL && fixture_path(rom_path, LONG_ROM) &&
      read_file(rom_path, rom, 1u << 20, &rom_length) && rom_length != 0u &&
      t64_profile_lookup(PROFILE_STEM, &profile) == T64_STATUS_OK &&
      t64_machine_create(&host, profile, &machine) == T64_STATUS_OK &&
      t64_machine_admit_rom(machine, rom, rom_length) == T64_STATUS_OK) {
    *status_out = t64_machine_state_import(machine, blob, size, 0u, report_out);
    ok = true;
  }
  free(rom);
  if (machine != NULL) t64_machine_destroy(machine);
  return ok;
}

/* ROW none or observer-range. The setup is the one of import-accepted-continues
 * (run A uninterrupted, run B exporting a state through the real supervised
 * runner); the blob of run B is copied into the private directory, mutated by
 * the independent re-signer for a mutant row, and imported through the real
 * runner (the supervised import triple and the direct runner, which prints the
 * closed reason on its diagnostic stream). One result line is printed; the
 * exit status is 0 only when observed equals expected. */
static int tracer_run(const char *row_name) {
  const bool mutant = strcmp(row_name, "observer-range") == 0;
  char rom[PATH_MAX];
  char dir[PATH_MAX];
  char artifact[PATH_MAX];
  char leaf[PATH_MAX];
  char first[256];
  char private_dir[PATH_MAX];
  char subject_path[PATH_MAX];
  char name[64];
  char expected[128];
  char reason[64];
  char want_diag[64];
  char hex[65];
  char observed_tuple[128] = "unobserved";
  const char *outcome_token;
  inventory_row row = {{0}, {0}, {0}, 0u};
  state_file published;
  state_file subject;
  run_spec spec = {0};
  run_out a;
  run_out b;
  run_out supervised;
  run_out direct;
  bool have_subject = false;
  memset(&subject, 0, sizeof subject);
  memset(&published, 0, sizeof published);
  if (mutant) {
    CHECK(inventory_load(row_name, &row));
    (void)snprintf(expected, sizeof expected, "%s/%s/%u/%s", row.status,
                   row.reason, row.section, row.rule);
  } else {
    (void)snprintf(expected, sizeof expected, "OK/NONE/0/NONE");
  }
  CHECK(fixture_path(rom, LONG_ROM));
  CHECK(make_run_dir(dir));
  CHECK(tracer_private_dir(private_dir));
  /* run A: uninterrupted to authored completion */
  spec.supervisor = g_supervisor;
  spec.fixture = rom;
  spec.bound = LONG_BOUND;
  CHECK(run_supervised(&spec, &a));
  expect_pass(&a);
  /* run B: the same ROM exporting a state through the real supervised runner */
  CHECK(make_artifact_dir(artifact));
  spec.artifact_dir = artifact;
  spec.artifact_max = ARTIFACT_MAX;
  spec.extra = state_args;
  spec.extra_count = sizeof state_args / sizeof state_args[0];
  CHECK(run_supervised(&spec, &b));
  expect_pass(&b);
  CHECK(list_directory(artifact, first) == 1u);
  CHECK(join_path(leaf, sizeof leaf, artifact, STATE_LEAF));
  CHECK(state_file_load(leaf, &published));
  /* the private copy (no-replace), mutated for a mutant row */
  if (published.bytes != NULL) {
    uint8_t *copy = (uint8_t *)malloc(published.size);
    CHECK(copy != NULL);
    if (copy != NULL) {
      memcpy(copy, published.bytes, published.size);
      if (mutant) {
        CHECK(mutate_observer_range(copy, published.size) == row.section);
      }
      (void)snprintf(name, sizeof name, "tracer-%ld-%s.sta", (long)getpid(),
                     row_name);
      CHECK(join_path(subject_path, sizeof subject_path, private_dir, name));
      CHECK(write_new_private(subject_path, copy, published.size));
      have_subject = state_file_load(subject_path, &subject);
      CHECK(have_subject);
      free(copy);
    }
  }
  memset(&supervised, 0, sizeof supervised);
  memset(&direct, 0, sizeof direct);
  if (have_subject) {
    /* the real import triple through the supervisor, then the direct runner for
     * the closed reason on the diagnostic stream */
    CHECK(run_with_state(&subject, rom, LONG_BOUND, NULL, 0u, &supervised));
    CHECK(run_direct_state_at(&subject, rom, dir, "1200000", &direct));
  }
  diagnostic_reason(&direct, reason, sizeof reason);
  if (direct.parsed && strcmp(direct.outcome, "harness-error") == 0 &&
      strcmp(direct.detail, "refusal") == 0 &&
      strcmp(direct.stop_kind, "refused") == 0) {
    outcome_token = "refusal";
  } else if (direct.parsed && strcmp(direct.outcome, "harness-error") == 0 &&
             strcmp(direct.stop_kind, "host-error") == 0 &&
             strcmp(direct.detail, "host-error") == 0 &&
             direct.guest_tick == a.guest_tick &&
             strcmp(direct.state_digest, a.state_digest) == 0 &&
             direct.log_bytes == 0u) {
    /* An accepted import resumes to the uninterrupted run's completion tick and
     * final state digest. The runner exported this state after it drained the
     * whole 20-byte log (observer cursor at the log length), so the resumed run
     * delivers no earlier byte (log bytes 0). Its verdict can never be pass (the
     * 09-09 evidence limit of a resumed run), so the closed record is the
     * harness-error host-error one. */
    outcome_token = "resumed";
  } else {
    outcome_token = direct.parsed ? direct.outcome : "unparsed";
  }
  if (have_subject) {
    CHECK(sha256_file_hex(subject_path, hex));
  } else {
    (void)snprintf(hex, sizeof hex, "none");
  }
  /* the full tuple of the same blob from the importer of this core library:
   * the status, the reason, the section and the rule, never only the reason */
  {
    t64_status status = T64_STATUS_OK;
    t64_state_import_report report;
    uint32_t want_status = 0u;
    uint32_t want_reason = 0u;
    uint32_t want_rule = 0u;
    const char *want_status_name = mutant ? row.status : "OK";
    const char *want_reason_name = mutant ? row.reason : "NONE";
    const char *want_rule_name = mutant ? row.rule : "NONE";
    const unsigned want_section = mutant ? row.section : 0u;
    const bool ran = have_subject &&
                     inprocess_import(subject.bytes, subject.size, &status,
                                      &report);

    CHECK(ran);
    CHECK(tuple_value(tuple_statuses,
                      sizeof tuple_statuses / sizeof tuple_statuses[0],
                      want_status_name, &want_status));
    CHECK(tuple_value(tuple_reasons,
                      sizeof tuple_reasons / sizeof tuple_reasons[0],
                      want_reason_name, &want_reason));
    CHECK(tuple_value(tuple_rules, sizeof tuple_rules / sizeof tuple_rules[0],
                      want_rule_name, &want_rule));
    if (ran) {
      (void)snprintf(
          observed_tuple, sizeof observed_tuple, "%s/%s/%u/%s",
          tuple_label(tuple_statuses,
                      sizeof tuple_statuses / sizeof tuple_statuses[0],
                      (uint32_t)status),
          tuple_label(tuple_reasons,
                      sizeof tuple_reasons / sizeof tuple_reasons[0],
                      report.reason),
          (unsigned)report.section_id,
          tuple_label(tuple_rules, sizeof tuple_rules / sizeof tuple_rules[0],
                      report.rule));
      CHECK((uint32_t)status == want_status);
      CHECK(report.reason == want_reason);
      CHECK(report.section_id == want_section);
      CHECK(report.rule == want_rule);
      if (mutant) {
        /* the refusal is raised right after the drain cursor is read, so its
         * position is the byte after the first value of the observer body */
        CHECK(report.position ==
              observer_body_offset(subject.bytes, subject.size) +
                  OBSERVER_DRAIN_CURSOR_AT + 8u);
      }
    }
  }
  (void)printf(
      "run-mutant %s expected=%s observed-tuple=%s outcome=%s detail=%s "
      "guest_tick=%llu reason=%s blob_bytes=%zu blob_sha256=%s\n",
      row_name, expected, observed_tuple, outcome_token,
      direct.parsed ? direct.stop_kind : "unparsed",
      (unsigned long long)direct.guest_tick, reason, subject.size, hex);
  if (mutant) {
    reason_to_diagnostic(row.reason, want_diag, sizeof want_diag);
    CHECK(strcmp(outcome_token, "refusal") == 0);
    CHECK(direct.guest_tick == 0u);
    CHECK(strcmp(reason, row.reason) == 0);
    expect_diagnostic(&direct, want_diag, &subject);
    expect_supervised_preflight_refusal(&supervised);
  } else {
    /* the accepted baseline of the same setup: the importer accepted (no
     * reason line), the direct record resumed equal, and the supervised record
     * is the closed report-invalid one of a runner-published state (not the
     * preflight refusal record of the mutant row) */
    CHECK(strcmp(outcome_token, "resumed") == 0);
    CHECK(strcmp(reason, "none") == 0);
    CHECK(supervised.parsed);
    CHECK(strcmp(supervised.outcome, "harness-error") == 0);
    CHECK(strcmp(supervised.stop_kind, "report-error") == 0);
    CHECK(strcmp(supervised.detail, "report-invalid") == 0);
    CHECK(supervised.guest_tick == 0u);
  }
  if (have_subject) (void)unlink(subject_path);
  state_file_free(&subject);
  state_file_free(&published);
  remove_tree(artifact);
  remove_tree(dir);
  /* the private directories go when empty (no blob may remain) */
  if (strncmp(g_evidence, private_dir, strlen(private_dir)) == 0) {
    remove_tree(g_evidence); /* every run directory of this run is under it */
  }
  (void)rmdir(private_dir);
  return g_failures == 0 ? 0 : 1;
}

/* --run-mutant ROW [SUPERVISOR TEST_SUPERVISOR CHECKER ROOT]. Without the four
 * paths they default to the siblings of this binary and the repository root
 * three levels above it (build/PRESET/runner/); every blob and working
 * directory of the run lives under the untracked private directory. */
static int tracer_main(int argc, char **argv) {
  static char supervisor[PATH_MAX];
  static char test_supervisor[PATH_MAX];
  static char checker[PATH_MAX];
  static char root[PATH_MAX];
  char private_dir[PATH_MAX];
  char evidence[PATH_MAX];
  if (strcmp(argv[2], "none") != 0 && strcmp(argv[2], "observer-range") != 0) {
    (void)fprintf(stderr, "unknown mutant row: %s\n", argv[2]);
    return 2;
  }
  if (argc == 7) {
    g_supervisor = argv[3];
    g_test_supervisor = argv[4];
    g_checker = argv[5];
    g_root = argv[6];
  } else if (argc == 3) {
    char here[PATH_MAX];
    char resolved[PATH_MAX];
    char *slash;
    if (snprintf(here, sizeof here, "%s", argv[0]) >= (int)sizeof here ||
        (slash = strrchr(here, '/')) == NULL) {
      (void)fprintf(stderr, "run-mutant needs a path to this binary\n");
      return 2;
    }
    *slash = '\0';
    if (realpath(here, resolved) == NULL ||
        snprintf(supervisor, sizeof supervisor, "%s/t64-supervisor", resolved) >=
            (int)sizeof supervisor ||
        snprintf(test_supervisor, sizeof test_supervisor,
                 "%s/t64-supervisor-test", resolved) >=
            (int)sizeof test_supervisor ||
        snprintf(checker, sizeof checker, "%s/t64-result-check", resolved) >=
            (int)sizeof checker ||
        snprintf(here, sizeof here, "%s/../../..", resolved) >=
            (int)sizeof here ||
        realpath(here, root) == NULL) {
      (void)fprintf(stderr, "run-mutant: cannot resolve the sibling paths\n");
      return 2;
    }
    g_supervisor = supervisor;
    g_test_supervisor = test_supervisor;
    g_checker = checker;
    g_root = root;
  } else {
    (void)fprintf(stderr,
                  "usage: %s --run-mutant ROW [SUPERVISOR TEST_SUPERVISOR "
                  "CHECKER ROOT]\n",
                  argv[0]);
    return 2;
  }
  if (!tracer_private_dir(private_dir) ||
      snprintf(evidence, sizeof evidence, "%s/ev", private_dir) >=
          (int)sizeof evidence ||
      (getenv("T64_PHASE09_EVIDENCE_DIR") == NULL &&
       setenv("T64_PHASE09_EVIDENCE_DIR", evidence, 0) != 0) ||
      !prepare_evidence()) {
    (void)fprintf(stderr, "run-mutant: private directory unavailable\n");
    return 2;
  }
  return tracer_run(argv[2]);
}

typedef struct {
  const char *name;
  int (*run)(void);
} case_entry;

static const case_entry cases[] = {
    {"export-accepted", case_export_accepted},
    {"export-size-cap", case_export_size_cap},
    {"export-fsize-at-state-size", case_export_fsize},
    {"export-refusals", case_export_refusals},
    {"export-nonterminal", case_export_nonterminal},
    {"export-decode", case_export_decode},
    {"checker-vocabulary", case_checker_vocabulary},
    {"import-accepted-continues", case_import_accepted_continues},
    {"import-refused-integrity", case_import_refused_integrity},
    {"import-refused-build", case_import_refused_build},
    {"import-option-refusals", case_import_option_refusals},
    {"import-bound-below-tick", case_import_bound_below_tick},
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
  if (argc >= 3 && strcmp(argv[1], "--run-mutant") == 0) {
    return tracer_main(argc, argv);
  }
  if (argc != 6) {
    (void)fprintf(stderr,
                  "usage: %s CASE|all SUPERVISOR TEST_SUPERVISOR CHECKER ROOT\n"
                  "       %s --run-mutant ROW\n",
                  argv[0], argv[0]);
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
