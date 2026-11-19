/*
 * Phase 9 save fixture tests (plan 09-04). The authored eeprom-rw and sram-rw
 * ROMs run through the real supervised runner on the explicit -e4k, -e16k and
 * -s32k ids and print transcripts that equal literals derived by
 * fixtures/save/save_oracle.py outside the ROMs and outside the core. Every
 * figure is a named software-profile policy value; nothing is a hardware or
 * timing claim.
 *
 * usage: test_save_fixtures CASE SUPERVISOR TEST_SUPERVISOR CHECKER ROOT
 *        test_save_fixtures --suite NAME SUPERVISOR TEST_SUPERVISOR CHECKER ROOT
 *        test_save_fixtures --list-cases
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
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
  const char *supervisor;
  const char *fixture;
  uint64_t bound; /* 0 leaves the option out */
} run_spec;

#define MAX_ARGS 32u

typedef struct {
  char *items[MAX_ARGS];
  size_t count;
} arg_list;

static void arg_add(arg_list *list, char *value) {
  if (list->count < MAX_ARGS - 1u) list->items[list->count++] = value;
}

/* The authored ROMs read no input: the default empty schedule is used. */
static bool run_supervised(const run_spec *spec, run_out *out) {
  char result_path[PATH_MAX];
  char meta_path[PATH_MAX];
  char capture_path[PATH_MAX];
  char input[PATH_MAX];
  char bound_text[24];
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
  arg_list list = {{NULL}, 0u};
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
  return out->parsed;
}

/* ---- policy literals and expected transcripts --------------------------- */

#define SUPERVISED_BOUND UINT64_C(3000000)
#define EEPROM_4K_ROM "phase09-eeprom-rw-e4k-v1"
#define EEPROM_4K_PROFILE "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e4k"
/* Oracle scenario eeprom-4k (fixtures/save/save_oracle.py --sha256), also the
 * independent shasum of the hand-written 191-byte literal. */
#define EEPROM_4K_SHA256 \
  "03891120e98176bc11f8d9f6cde6278ff33ef0ebe9018d2f24ce083b3d2c6502"
#define EEPROM_4K_BYTES 191u

static void expect_pass(const run_out *run) {
  CHECK(run->parsed);
  CHECK(run->checker_ok);
  CHECK(run->supervisor_exit == 0);
  CHECK(strcmp(run->outcome, "pass") == 0);
  CHECK(strcmp(run->detail, "positive-match") == 0);
  CHECK(strcmp(run->stop_kind, "authored-completion") == 0);
}

/* ---- accepted cases ------------------------------------------------------- */

/* The baseline 4 Kbit ROM on the explicit -e4k id: write, read, the busy
 * window opening and closing in guest time, all through the modeled SI. */
static int case_eeprom_4k_accepted(void) {
  char rom[PATH_MAX];
  run_spec spec = {0};
  run_out run;
  CHECK(fixture_path(rom, EEPROM_4K_ROM));
  spec.supervisor = g_supervisor;
  spec.fixture = rom;
  spec.bound = SUPERVISED_BOUND;
  CHECK(run_supervised(&spec, &run));
  expect_pass(&run);
  CHECK(strcmp(run.fixture_id, EEPROM_4K_ROM) == 0);
  CHECK(strcmp(run.profile_id, EEPROM_4K_PROFILE) == 0);
  CHECK(run.log_bytes == EEPROM_4K_BYTES);
  CHECK(strcmp(run.log_sha256, EEPROM_4K_SHA256) == 0);
  CHECK(run.guest_tick < SUPERVISED_BOUND);
  (void)fprintf(stderr, "eeprom 4k completion tick %llu\n",
                (unsigned long long)run.guest_tick);
  if (g_failures == 0) {
    (void)printf("case reached: eeprom-4k-accepted completion_tick=%llu\n",
                 (unsigned long long)run.guest_tick);
  }
  return g_failures == 0 ? 0 : 1;
}

/* ---- dispatch --------------------------------------------------------------- */

typedef struct {
  const char *name;
  int (*run)(void);
  const char *suite;
} case_entry;

static const case_entry cases[] = {
    {"eeprom-4k-accepted", case_eeprom_4k_accepted, "accepted"},
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
