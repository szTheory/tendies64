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
    char *args[40];
    size_t count = 0u;
    size_t i;
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

/* ---- case export-accepted ----------------------------------------------- */

static int case_export_accepted(void) {
  char dir[PATH_MAX];
  run_out run;
  uint64_t size = 0u;
  CHECK(run_accepted(dir, ARTIFACT_MAX, &run, &size));
  /* Larger than the default child limit, within the raised one, and between
   * the independent arithmetic bounds (the major arrays alone, and 11 MiB). */
  CHECK(size > DEFAULT_CHILD_LIMIT);
  CHECK(size <= ARTIFACT_MAX + 65536u);
  CHECK(size > 6303756u);
  CHECK(size < 11534336u);
  g_state_size = size;
  g_accepted_tick = run.guest_tick;
  (void)fprintf(stderr, "export-accepted: state size %llu, guest tick %llu\n",
                (unsigned long long)size, (unsigned long long)run.guest_tick);
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
    {"export-accepted", case_export_accepted},
    {"export-size-cap", case_export_size_cap},
    {"export-fsize-at-state-size", case_export_fsize},
    {"export-refusals", case_export_refusals},
    {"export-nonterminal", case_export_nonterminal},
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
