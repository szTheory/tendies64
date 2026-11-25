/*
 * Phase 10 runner contract of the state container format strings (plan 10-15,
 * CORE-06). The v1 string names container version 1 (state serialization
 * version 25) and the v2 string names container version 2 (state 26). The
 * supervisor and the runner argument parser accept only that closed set; the
 * runner pairs the string with the profile at prelaunch, so every other pair is
 * refused at guest tick 0 with nothing published, and a state import stays
 * defined for state 25 only. Each refusal runs next to an accepted baseline of
 * the same setup that differs only in the one option under test, so the refusal
 * is attributable to the pairing and not to the fixture, the paths or the
 * artifact limits. No ROM, save or state blob is committed: every blob is
 * written by the run into a private directory under the evidence scope and
 * removed with it.
 *
 * usage: test_state_format_v26 --list-cases
 *        test_state_format_v26 --case NAME SUPERVISOR TEST_SUPERVISOR CHECKER
 *                              ROOT RUNNER
 *        test_state_format_v26 SUPERVISOR TEST_SUPERVISOR CHECKER ROOT RUNNER
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <dirent.h>
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
#include "t64_sha256.h"
#include "t64_test_tree.h"

static const char *g_supervisor;
static const char *g_test_supervisor;
static const char *g_checker;
static const char *g_root;
static const char *g_runner;
static char g_evidence[PATH_MAX];
static int g_failures;

static void check_impl(bool ok, const char *expr, int line) {
  if (!ok) {
    g_failures++;
    (void)fprintf(stderr, "FAIL line %d: %s\n", line, expr);
  }
}
#define CHECK(cond) check_impl((cond), #cond, __LINE__)

/* ---- policy literals ---------------------------------------------------- */

#define FORMAT_V1 "t64.state-container.v1"
#define FORMAT_V2 "t64.state-container.v2"
#define FORMAT_V3 "t64.state-container.v3"
#define STATE_LEAF "state.sta"
#define ARTIFACT_MAX "16777216"
#define GUEST_BOUND "600000"
#define SCRIPT_REL "fixtures/composite/composite.input.json"
/* state 25 (the si-v1 family, container v1) */
#define ROM_25 "phase09-admit-si-v1"
#define PROFILE_25 "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1"
/* state 26 (the si-v2 family, container v2) */
#define ROM_26 "phase10-composite-s32k"
#define PROFILE_26 "ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k"
/* state 24 (the ai-v1 audio profile, no state export) */
#define ROM_24 "audio-pcm"

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

/* The evidence directory may be a child of a directory another test of the
 * same family creates, so its parent is created first; both are private and a
 * racing mkdir that finds either present is success. */
static bool ensure_directory(const char *path) {
  struct stat info;
  if (mkdir(path, 0700) != 0 && errno != EEXIST) return false;
  return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

static bool prepare_evidence(void) {
  const char *configured = getenv("T64_PHASE10_EVIDENCE_DIR");
  const char *temp = getenv("TMPDIR");
  if (configured != NULL && configured[0] != '\0') {
    char parent[PATH_MAX];
    char *slash;
    if (snprintf(g_evidence, sizeof g_evidence, "%s", configured) >=
            (int)sizeof g_evidence ||
        snprintf(parent, sizeof parent, "%s", configured) >=
            (int)sizeof parent) {
      return false;
    }
    slash = strrchr(parent, '/');
    if (slash != NULL && slash != parent) {
      *slash = '\0';
      if (!ensure_directory(parent)) return false;
    }
    return ensure_directory(g_evidence) &&
           t64_test_scope_open(g_evidence, sizeof g_evidence, false);
  }
  if (temp == NULL || temp[0] == '\0') temp = "/tmp";
  if (snprintf(g_evidence, sizeof g_evidence, "%s/t64-phase10.XXXXXX", temp) >=
      (int)sizeof g_evidence) {
    return false;
  }
  return mkdtemp(g_evidence) != NULL &&
         t64_test_scope_open(g_evidence, sizeof g_evidence, true);
}

static bool make_private_dir(char out[PATH_MAX]) {
  if (snprintf(out, PATH_MAX, "%s/p10.XXXXXX", g_evidence) >= PATH_MAX) {
    return false;
  }
  return mkdtemp(out) != NULL && chmod(out, 0700) == 0;
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

static bool default_input_path(char out[PATH_MAX]) {
  char dir[PATH_MAX];
  return join_path(dir, sizeof dir, g_root, "build/fixtures") &&
         join_path(out, PATH_MAX, dir, "hello.input.json");
}

/* The composite input script and the size and SHA-256 the supervisor needs for
 * any script other than the empty default. */
static char g_script_path[PATH_MAX];
static char g_script_size[24];
static char g_script_sha[65];

static void sha256_hex_of(const uint8_t *bytes, size_t length, char out[65]) {
  t64_sha256_ctx context;
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  t64_sha256_init(&context);
  t64_sha256_update(&context, bytes, length);
  t64_sha256_final(&context, digest);
  t64_sha256_hex(digest, out);
}

static bool load_script_identity(void) {
  static uint8_t bytes[4096];
  size_t length = 0u;
  if (!join_path(g_script_path, sizeof g_script_path, g_root, SCRIPT_REL) ||
      !read_file(g_script_path, bytes, sizeof bytes, &length) || length == 0u ||
      length >= sizeof bytes) {
    return false;
  }
  sha256_hex_of(bytes, length, g_script_sha);
  (void)snprintf(g_script_size, sizeof g_script_size, "%zu", length);
  return true;
}

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
  char state_digest[65];
  uint64_t guest_tick;
  char dir[PATH_MAX];
  char aux_dir[PATH_MAX]; /* the artifact directory */
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
  copy_string(out->stop_kind, sizeof out->stop_kind,
              stop == NULL ? NULL : t64_json_get(stop, "kind"));
  copy_string(out->state_digest, sizeof out->state_digest,
              t64_json_get(doc.root, "state_digest"));
  if (t64_json_u64_string(t64_json_get(doc.root, "guest_tick"), &value)) {
    out->guest_tick = value;
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
  const char *stem;          /* fixture stem under build/fixtures */
  bool script;               /* the composite input script with size and hash */
  const char *export_format; /* --state-export-format; NULL: no state export */
  const char *import_path;   /* --state-import triple; NULL: no import */
  const char *import_size;
  const char *import_sha256;
} run_spec;

#define MAX_ARGS 48u

typedef struct {
  char *items[MAX_ARGS];
  size_t count;
} arg_list;

static void arg_add(arg_list *list, const char *value) {
  if (list->count < MAX_ARGS - 1u) {
    list->items[list->count++] = (char *)(uintptr_t)value;
  }
}

static void arg_add_pair(arg_list *list, const char *option,
                         const char *value) {
  if (value == NULL) return;
  arg_add(list, option);
  arg_add(list, value);
}

static bool run_supervised(const run_spec *spec, run_out *out) {
  char rom[PATH_MAX];
  char default_input[PATH_MAX];
  char result_path[PATH_MAX];
  char meta_path[PATH_MAX];
  arg_list list = {{NULL}, 0u};
  pid_t pid;
  int status = 0;
  memset(out, 0, sizeof *out);
  if (!make_private_dir(out->dir) || !fixture_path(rom, spec->stem) ||
      !default_input_path(default_input) ||
      !join_path(result_path, sizeof result_path, out->dir, "result.json") ||
      !join_path(meta_path, sizeof meta_path, out->dir, "meta.json")) {
    return false;
  }
  if (spec->export_format != NULL && !make_private_dir(out->aux_dir)) {
    return false;
  }
  (void)snprintf(out->result_path, sizeof out->result_path, "%s", result_path);
  arg_add(&list, g_supervisor);
  arg_add(&list, "--real");
  arg_add_pair(&list, "--result", result_path);
  arg_add_pair(&list, "--meta", meta_path);
  arg_add_pair(&list, "--work-dir", out->dir);
  arg_add_pair(&list, "--wall-ms", "30000");
  arg_add_pair(&list, "--bound", GUEST_BOUND);
  arg_add_pair(&list, "--fixture", rom);
  if (spec->script) {
    arg_add_pair(&list, "--input", g_script_path);
    arg_add_pair(&list, "--input-size", g_script_size);
    arg_add_pair(&list, "--input-sha256", g_script_sha);
  } else {
    arg_add_pair(&list, "--input", default_input);
  }
  if (spec->export_format != NULL) {
    arg_add_pair(&list, "--artifact-dir", out->aux_dir);
    arg_add_pair(&list, "--artifact-max-bytes", ARTIFACT_MAX);
    arg_add_pair(&list, "--state-export", STATE_LEAF);
    arg_add_pair(&list, "--state-export-format", spec->export_format);
  }
  arg_add_pair(&list, "--state-import", spec->import_path);
  arg_add_pair(&list, "--state-import-size", spec->import_size);
  arg_add_pair(&list, "--state-import-sha256", spec->import_sha256);
  list.items[list.count] = NULL;
  pid = fork();
  if (pid < 0) return false;
  if (pid == 0) {
    char out_path[PATH_MAX];
    char err_path[PATH_MAX];
    int out_fd;
    int err_fd;
    if (!join_path(out_path, sizeof out_path, out->dir, "supervisor.out") ||
        !join_path(err_path, sizeof err_path, out->dir, "supervisor.err")) {
      _exit(126);
    }
    out_fd = open(out_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    err_fd = open(err_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (out_fd < 0 || err_fd < 0 || dup2(out_fd, STDOUT_FILENO) < 0 ||
        dup2(err_fd, STDERR_FILENO) < 0) {
      _exit(126);
    }
    execv(g_supervisor, list.items);
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

/* A directory the run is done with: its blob is several MiB. */
static void drop_run(run_out *run) {
  if (run->aux_dir[0] != '\0') t64_test_remove_tree(run->aux_dir);
  if (run->dir[0] != '\0') t64_test_remove_tree(run->dir);
}

/* ---- expectations ------------------------------------------------------- */

static void expect_pass(const run_out *run, const char *rom,
                        const char *profile) {
  CHECK(run->parsed);
  CHECK(run->checker_ok);
  CHECK(run->supervisor_exit == 0);
  CHECK(strcmp(run->outcome, "pass") == 0);
  CHECK(strcmp(run->detail, "positive-match") == 0);
  CHECK(strcmp(run->stop_kind, "authored-completion") == 0);
  CHECK(strcmp(run->fixture_id, rom) == 0);
  CHECK(strcmp(run->profile_id, profile) == 0);
}

/* The state export is published: one private file of the leaf name. */
static uint64_t expect_published(const run_out *run) {
  char first[256];
  char path[PATH_MAX];
  struct stat info;
  CHECK(list_directory(run->aux_dir, first) == 1u);
  CHECK(strcmp(first, STATE_LEAF) == 0);
  CHECK(join_path(path, sizeof path, run->aux_dir, STATE_LEAF));
  CHECK(lstat(path, &info) == 0 && S_ISREG(info.st_mode) &&
        (info.st_mode & 0777) == 0600 && info.st_size > 0);
  return lstat(path, &info) == 0 ? (uint64_t)info.st_size : 0u;
}

/* The prelaunch refusal at guest tick 0: the runner's own refusal, seen through
 * the supervisor as the report-error classification (docs/spec/result-protocol.md);
 * nothing is published. */
static void expect_prelaunch_refusal(const run_out *run) {
  char first[256];
  CHECK(run->parsed);
  CHECK(run->checker_ok);
  CHECK(strcmp(run->outcome, "harness-error") == 0);
  CHECK(strcmp(run->detail, "capture-invalid") == 0);
  CHECK(strcmp(run->stop_kind, "report-error") == 0);
  CHECK(run->guest_tick == 0u);
  CHECK(run->supervisor_exit == 1);
  CHECK(run->aux_dir[0] == '\0' || list_directory(run->aux_dir, first) == 0u);
}

/* The supervisor's usage refusal: the guest is never started. */
static void expect_usage_refusal(const run_out *run) {
  char first[256];
  CHECK(run->parsed);
  CHECK(strcmp(run->outcome, "harness-error") == 0);
  CHECK(strcmp(run->detail, "usage") == 0);
  CHECK(strcmp(run->stop_kind, "not-started") == 0);
  CHECK(run->guest_tick == 0u);
  CHECK(run->supervisor_exit == 2);
  CHECK(run->aux_dir[0] == '\0' || list_directory(run->aux_dir, first) == 0u);
}

static void report_pair(const char *name, const char *kind, const run_out *run) {
  (void)printf("pairing %s: %s (%s/%s/%s) at guest tick %llu\n", name, kind,
               run->outcome, run->detail, run->stop_kind,
               (unsigned long long)run->guest_tick);
}

/* One accepted export of state 25 with the v1 string: the baseline of every
 * refusal that uses the si-v1 ROM, and the blob the import case feeds back. */
static bool export_v1_on_25(run_out *run, uint64_t *size_out) {
  run_spec spec = {0};
  spec.stem = ROM_25;
  spec.export_format = FORMAT_V1;
  if (!run_supervised(&spec, run)) return false;
  expect_pass(run, ROM_25, PROFILE_25);
  *size_out = expect_published(run);
  return true;
}

/* ---- cases ---------------------------------------------------------------- */

static int case_v1_on_25_accepted(void) {
  run_out run;
  uint64_t size = 0u;
  CHECK(export_v1_on_25(&run, &size));
  CHECK(size > 0u);
  report_pair("v1-on-25-accepted", "accepted", &run);
  (void)printf("pairing v1-on-25-accepted: state blob %llu bytes published\n",
               (unsigned long long)size);
  drop_run(&run);
  return g_failures == 0 ? 0 : 1;
}

static int case_v2_on_25_refused(void) {
  run_out base;
  run_out run;
  run_spec spec = {0};
  uint64_t size = 0u;
  CHECK(export_v1_on_25(&base, &size));
  drop_run(&base);
  spec.stem = ROM_25;
  spec.export_format = FORMAT_V2;
  CHECK(run_supervised(&spec, &run));
  expect_prelaunch_refusal(&run);
  report_pair("v2-on-25-refused", "prelaunch refusal", &run);
  drop_run(&run);
  return g_failures == 0 ? 0 : 1;
}

/* The baseline of the 26 rows is the v2 string on the same ROM, script and
 * options: admitted and published (the positive path). */
static int case_v1_on_26_refused(void) {
  run_out base;
  run_out run;
  run_spec spec = {0};
  spec.stem = ROM_26;
  spec.script = true;
  spec.export_format = FORMAT_V2;
  CHECK(run_supervised(&spec, &base));
  expect_pass(&base, ROM_26, PROFILE_26);
  CHECK(expect_published(&base) > 0u);
  drop_run(&base);
  spec.export_format = FORMAT_V1;
  CHECK(run_supervised(&spec, &run));
  expect_prelaunch_refusal(&run);
  report_pair("v1-on-26-refused", "prelaunch refusal", &run);
  drop_run(&run);
  return g_failures == 0 ? 0 : 1;
}

/* The baseline of the 24 rows is the same ROM without a state option: it
 * passes, so the refusal is the state request on a profile with no state
 * export. */
static int refused_on_24(const char *name, const char *format) {
  run_out base;
  run_out run;
  run_spec spec = {0};
  spec.stem = ROM_24;
  CHECK(run_supervised(&spec, &base));
  CHECK(base.parsed);
  CHECK(strcmp(base.outcome, "pass") == 0);
  CHECK(strcmp(base.detail, "positive-match") == 0);
  drop_run(&base);
  spec.export_format = format;
  CHECK(run_supervised(&spec, &run));
  expect_prelaunch_refusal(&run);
  report_pair(name, "prelaunch refusal", &run);
  drop_run(&run);
  return g_failures == 0 ? 0 : 1;
}

static int case_v1_on_24_refused(void) {
  return refused_on_24("v1-on-24-refused", FORMAT_V1);
}

static int case_v2_on_24_refused(void) {
  return refused_on_24("v2-on-24-refused", FORMAT_V2);
}

/* A state import on a state-26 profile. The blob is the one the v1 export of
 * state 25 wrote; the supervisor checks its size and SHA-256 and hands the
 * child a descriptor, so the refusal is the runner's prelaunch pairing, not an
 * integrity failure. The baseline imports the same blob on state 25: the guest
 * resumes at the exported tick and ends at the same state digest. */
static int case_import_on_26_refused(void) {
  run_out exported;
  run_out base;
  run_out run;
  run_spec spec = {0};
  static uint8_t blob[16u * 1024u * 1024u];
  size_t length = 0u;
  char path[PATH_MAX];
  char size_text[24];
  char sha[65];
  uint64_t size = 0u;
  CHECK(export_v1_on_25(&exported, &size));
  CHECK(join_path(path, sizeof path, exported.aux_dir, STATE_LEAF));
  CHECK(read_file(path, blob, sizeof blob, &length) && length == size &&
        length > 0u);
  sha256_hex_of(blob, length, sha);
  (void)snprintf(size_text, sizeof size_text, "%zu", length);

  spec.stem = ROM_25;
  spec.import_path = path;
  spec.import_size = size_text;
  spec.import_sha256 = sha;
  CHECK(run_supervised(&spec, &base));
  CHECK(base.parsed);
  CHECK(base.checker_ok);
  CHECK(base.guest_tick == exported.guest_tick);
  CHECK(strcmp(base.state_digest, exported.state_digest) == 0);
  CHECK(strcmp(base.profile_id, PROFILE_25) == 0);
  drop_run(&base);

  spec.stem = ROM_26;
  CHECK(run_supervised(&spec, &run));
  expect_prelaunch_refusal(&run);
  report_pair("import-on-26-refused", "prelaunch refusal", &run);
  drop_run(&run);
  drop_run(&exported);
  return g_failures == 0 ? 0 : 1;
}

/* A string outside the closed set is the supervisor's usage refusal: the guest
 * never starts. */
static int case_unknown_string_refused(void) {
  run_out base;
  run_out run;
  run_spec spec = {0};
  uint64_t size = 0u;
  CHECK(export_v1_on_25(&base, &size));
  drop_run(&base);
  spec.stem = ROM_25;
  spec.export_format = FORMAT_V3;
  CHECK(run_supervised(&spec, &run));
  expect_usage_refusal(&run);
  report_pair("unknown-string-refused", "usage refusal", &run);
  drop_run(&run);
  return g_failures == 0 ? 0 : 1;
}

/* ---- dispatch ----------------------------------------------------------- */

typedef struct {
  const char *name;
  int (*run)(void);
} case_entry;

static const case_entry cases[] = {
    {"v1-on-25-accepted", case_v1_on_25_accepted},
    {"v2-on-25-refused", case_v2_on_25_refused},
    {"v1-on-26-refused", case_v1_on_26_refused},
    {"v1-on-24-refused", case_v1_on_24_refused},
    {"v2-on-24-refused", case_v2_on_24_refused},
    {"import-on-26-refused", case_import_on_26_refused},
    {"unknown-string-refused", case_unknown_string_refused},
};

int main(int argc, char **argv) {
  size_t i;
  size_t base;
  bool single;
  int status = 0;
  bool matched = false;
  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
      (void)puts(cases[i].name);
    }
    return 0;
  }
  single = argc == 8 && strcmp(argv[1], "--case") == 0;
  if (!single && argc != 6) {
    (void)fprintf(stderr,
                  "usage: %s [--case NAME] SUPERVISOR TEST_SUPERVISOR "
                  "CHECKER ROOT RUNNER\n",
                  argv[0]);
    return 2;
  }
  base = single ? 3u : 1u;
  g_supervisor = argv[base];
  g_test_supervisor = argv[base + 1u];
  g_checker = argv[base + 2u];
  g_root = argv[base + 3u];
  g_runner = argv[base + 4u];
  (void)g_test_supervisor;
  (void)g_runner;
  if (!prepare_evidence()) {
    (void)fprintf(stderr, "evidence directory unavailable\n");
    return 2;
  }
  if (!load_script_identity()) {
    (void)fprintf(stderr, "composite input script unavailable\n");
    return 2;
  }
  for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
    int case_status;
    if (single && strcmp(cases[i].name, argv[2]) != 0) continue;
    matched = true;
    g_failures = 0;
    case_status = cases[i].run();
    (void)fprintf(stderr, "case %s: %s\n", cases[i].name,
                  case_status == 0 ? "ok" : "FAILED");
    if (case_status != 0) status = 1;
  }
  if (!matched) {
    (void)fprintf(stderr, "unknown case: %s\n", single ? argv[2] : "");
    return 2;
  }
  return status;
}
