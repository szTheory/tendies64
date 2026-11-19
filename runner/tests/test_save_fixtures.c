/*
 * Phase 9 save fixture tests (plan 09-04). The authored eeprom-rw and sram-rw
 * ROMs run through the real supervised runner on the explicit -e4k, -e16k and
 * -s32k ids and print transcripts that equal literals derived by
 * fixtures/save/save_oracle.py outside the ROMs and outside the core. Every
 * figure is a named software-profile policy value; nothing is a hardware or
 * timing claim.
 *
 * usage: test_save_fixtures CASE SUPERVISOR TEST_SUPERVISOR CHECKER ROOT RUNNER
 *        test_save_fixtures --suite NAME SUPERVISOR TEST_SUPERVISOR CHECKER ROOT
 *                           RUNNER
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

/* A file descriptor over an unlinked read-only copy of `path`, the way the
 * supervisor hands a fixture or an input script to the runner. */
static int open_unlinked_copy(const char *path) {
  uint8_t bytes[8192];
  size_t length = 0u;
  size_t offset = 0u;
  char name[PATH_MAX];
  int fd;
  if (!read_file(path, bytes, sizeof bytes, &length) || length == 0u ||
      length >= sizeof bytes ||
      snprintf(name, sizeof name, "%s/direct.XXXXXX", g_evidence) >=
          (int)sizeof name) {
    return -1;
  }
  fd = mkstemp(name);
  if (fd < 0) return -1;
  while (offset < length) {
    ssize_t written = write(fd, bytes + offset, length - offset);
    if (written < 0 && errno == EINTR) continue;
    if (written <= 0) {
      (void)close(fd);
      (void)unlink(name);
      return -1;
    }
    offset += (size_t)written;
  }
  (void)close(fd);
  fd = open(name, O_RDONLY);
  (void)unlink(name);
  return fd;
}

/* The real runner binary itself, with the fixture and the input script as
 * unlinked descriptors and an explicit profile id: proves the identity check
 * between a ROM and its one profile before the guest starts. */
static bool run_runner_direct(const char *rom_path, const char *profile_id,
                              uint64_t bound, run_out *out) {
  char input[PATH_MAX];
  char fixture_fd_text[16];
  char input_fd_text[16];
  char bound_text[24];
  char fixture_option[] = "--fixture-fd";
  char input_option[] = "--input-fd";
  char profile_option[] = "--profile";
  char ticks_option[] = "--guest-ticks";
  char *args[10];
  pid_t pid;
  int status = 0;
  int fixture_fd;
  int input_fd;
  int result_fd;
  memset(out, 0, sizeof *out);
  if (!make_run_dir(out->dir) || !input_path(input) ||
      !join_path(out->result_path, sizeof out->result_path, out->dir,
                 "result.json")) {
    return false;
  }
  fixture_fd = open_unlinked_copy(rom_path);
  input_fd = open_unlinked_copy(input);
  result_fd = open(out->result_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (fixture_fd < 0 || input_fd < 0 || result_fd < 0) return false;
  (void)snprintf(fixture_fd_text, sizeof fixture_fd_text, "%d", fixture_fd);
  (void)snprintf(input_fd_text, sizeof input_fd_text, "%d", input_fd);
  (void)snprintf(bound_text, sizeof bound_text, "%llu",
                 (unsigned long long)bound);
  args[0] = (char *)(uintptr_t)g_runner;
  args[1] = fixture_option;
  args[2] = fixture_fd_text;
  args[3] = input_option;
  args[4] = input_fd_text;
  args[5] = profile_option;
  args[6] = (char *)(uintptr_t)profile_id;
  args[7] = ticks_option;
  args[8] = bound_text;
  args[9] = NULL;
  pid = fork();
  if (pid < 0) return false;
  if (pid == 0) {
    if (dup2(result_fd, STDOUT_FILENO) < 0) _exit(126);
    execv(g_runner, args);
    _exit(127);
  }
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) return false;
  }
  (void)close(fixture_fd);
  (void)close(input_fd);
  (void)close(result_fd);
  out->supervisor_exit = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  out->parsed = parse_record(out->result_path, out);
  return out->parsed;
}

static bool capture_equals(const run_out *run, const char *literal,
                           size_t length) {
  char path[PATH_MAX];
  uint8_t bytes[512];
  size_t got = 0u;
  if (!join_path(path, sizeof path, run->dir, "capture.bin") ||
      !read_file(path, bytes, sizeof bytes, &got)) {
    return false;
  }
  return got == length && memcmp(bytes, literal, length) == 0;
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
#define EEPROM_16K_ROM "phase09-eeprom-rw-e16k-v1"
#define EEPROM_16K_PROFILE "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e16k"
#define EEPROM_16K_SHA256 \
  "d9a5bf956cce18ea6bc3fd563ae060d7dda3f8554a881b62214ccf89a7f4ca95"
#define EEPROM_16K_BYTES 192u
#define EEPROM_WRONG_ROM "phase09-eeprom-rw-wrong-e4k-v1"
/* Oracle scenario eeprom-wrong-4k: line R2 ends in 19 and nothing else
 * differs. */
#define EEPROM_WRONG_SHA256 \
  "87609f3fa10ce472755b3b501cfb80a457c16be2c20e536b190b8e2002598a7c"
static const char eeprom_wrong_transcript[] =
    "EEPROM 4k\n"
    "I1 03 008000\n"
    "W1 01 00\n"
    "I2 03 008080\n"
    "R1 48 ffffffffffffffff\n"
    "W2 01 80\n"
    "I3 03 008000\n"
    "R2 08 1112131415161719\n"
    "R5 08 ffffffffffffffff\n"
    "W3 01 00\n"
    "R3 08 4142434445464748\n"
    "R4 08 4142434445464748\n";
#define SRAM_ROM "phase09-sram-rw-s32k-v1"
#define SRAM_PROFILE "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-s32k"
#define SRAM_SHA256 \
  "52b0f1caee3ec470f4217236ce6f3da94e6ff1c15fd93143a68f7364b732a026"
#define SRAM_BYTES 101u
#define SRAM_WRONG_ROM "phase09-sram-rw-wrong-s32k-v1"
/* Oracle scenario sram-wrong-32k: lines D and M end in fe. */
#define SRAM_WRONG_SHA256 \
  "29174dfa593b7e9b6cc9e979ff7312e1fc39b5fc41ef9b280e8833dfe049f06c"
static const char sram_wrong_transcript[] =
    "SRAM 32k\n"
    "Z 00000000\n"
    "D 00112233445566778899aabbccddeefe\n"
    "C 12345678\n"
    "M 00112233123456788899aabbccddeefe\n";

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

/* One accepted run of a row: pass, positive-match, the exact transcript
 * length and digest, completion before the bound; prints the completion tick
 * for the case record. */
static int accepted_run(const char *name, const char *rom_id,
                        const char *profile, const char *sha256,
                        uint64_t bytes) {
  char rom[PATH_MAX];
  run_spec spec = {0};
  run_out run;
  CHECK(fixture_path(rom, rom_id));
  spec.supervisor = g_supervisor;
  spec.fixture = rom;
  spec.bound = SUPERVISED_BOUND;
  CHECK(run_supervised(&spec, &run));
  expect_pass(&run);
  CHECK(strcmp(run.fixture_id, rom_id) == 0);
  CHECK(strcmp(run.profile_id, profile) == 0);
  CHECK(run.log_bytes == bytes);
  CHECK(strcmp(run.log_sha256, sha256) == 0);
  CHECK(run.guest_tick < SUPERVISED_BOUND);
  (void)fprintf(stderr, "%s completion tick %llu\n", name,
                (unsigned long long)run.guest_tick);
  if (g_failures == 0) {
    (void)printf("case reached: %s completion_tick=%llu\n", name,
                 (unsigned long long)run.guest_tick);
  }
  return g_failures == 0 ? 0 : 1;
}

static int case_eeprom_16k_accepted(void) {
  return accepted_run("eeprom-16k-accepted", EEPROM_16K_ROM, EEPROM_16K_PROFILE,
                      EEPROM_16K_SHA256, EEPROM_16K_BYTES);
}

static int case_sram_accepted(void) {
  return accepted_run("sram-accepted", SRAM_ROM, SRAM_PROFILE, SRAM_SHA256,
                      SRAM_BYTES);
}

/* ---- controls (each runs the accepted baseline in the same setup) ----------- */

static void expect_wrong_transcript(const run_out *run, uint64_t bytes) {
  CHECK(run->parsed);
  CHECK(run->checker_ok);
  CHECK(strcmp(run->outcome, "guest-fail") == 0);
  CHECK(strcmp(run->detail, "wrong-transcript") == 0);
  CHECK(strcmp(run->stop_kind, "authored-completion") == 0);
  CHECK(run->log_bytes == bytes);
}

static void report_control(const char *name, const run_out *run) {
  if (!run->parsed) return;
  (void)printf("case reached: %s outcome=%s detail=%s guest_tick=%llu\n", name,
               run->outcome, run->detail,
               (unsigned long long)run->guest_tick);
}

/* The wrong-byte ROM on its own row carries the same pin as its baseline, so
 * its text reaches the same comparator and fails it with the oracle's own
 * transcript. */
static int wrong_byte_control(const char *name, const char *base_id,
                              const char *base_sha, uint64_t base_bytes,
                              const char *wrong_id, const char *wrong_sha,
                              const char *wrong_text, size_t wrong_length) {
  char base[PATH_MAX];
  char wrong[PATH_MAX];
  run_spec spec = {0};
  run_out run;
  CHECK(fixture_path(base, base_id));
  CHECK(fixture_path(wrong, wrong_id));
  spec.supervisor = g_supervisor;
  spec.fixture = base;
  spec.bound = SUPERVISED_BOUND;
  CHECK(run_supervised(&spec, &run));
  expect_pass(&run);
  CHECK(strcmp(run.log_sha256, base_sha) == 0);
  CHECK(run.log_bytes == base_bytes);
  spec.fixture = wrong;
  CHECK(run_supervised(&spec, &run));
  expect_wrong_transcript(&run, base_bytes);
  CHECK(strcmp(run.fixture_id, wrong_id) == 0);
  CHECK(strcmp(run.log_sha256, wrong_sha) == 0);
  CHECK(strcmp(run.log_sha256, base_sha) != 0);
  CHECK(capture_equals(&run, wrong_text, wrong_length));
  report_control(name, &run);
  return g_failures == 0 ? 0 : 1;
}

static int case_same_setup_wrong_byte_eeprom(void) {
  return wrong_byte_control("same-setup-wrong-byte-eeprom", EEPROM_4K_ROM,
                            EEPROM_4K_SHA256, EEPROM_4K_BYTES,
                            EEPROM_WRONG_ROM, EEPROM_WRONG_SHA256,
                            eeprom_wrong_transcript,
                            sizeof eeprom_wrong_transcript - 1u);
}

static int case_same_setup_wrong_byte_sram(void) {
  return wrong_byte_control("same-setup-wrong-byte-sram", SRAM_ROM, SRAM_SHA256,
                            SRAM_BYTES, SRAM_WRONG_ROM, SRAM_WRONG_SHA256,
                            sram_wrong_transcript,
                            sizeof sram_wrong_transcript - 1u);
}

/* One ROM binds exactly one profile id (K12): after the accepted baselines
 * (the supervised run of each ROM, and the real runner binary with the
 * matching profile), the 4 Kbit ROM bytes under the -s32k profile id and the
 * SRAM ROM bytes under the -e4k id are refused by the identity check before
 * the guest starts (guest tick 0). */
static int case_rom_profile_mismatch_refused(void) {
  char eeprom_rom[PATH_MAX];
  char sram_rom[PATH_MAX];
  run_spec spec = {0};
  run_out run;
  CHECK(fixture_path(eeprom_rom, EEPROM_4K_ROM));
  CHECK(fixture_path(sram_rom, SRAM_ROM));
  spec.supervisor = g_supervisor;
  spec.fixture = eeprom_rom;
  spec.bound = SUPERVISED_BOUND;
  CHECK(run_supervised(&spec, &run));
  expect_pass(&run);
  spec.fixture = sram_rom;
  CHECK(run_supervised(&spec, &run));
  expect_pass(&run);

  CHECK(run_runner_direct(eeprom_rom, EEPROM_4K_PROFILE, SUPERVISED_BOUND, &run));
  CHECK(strcmp(run.outcome, "pass") == 0);
  CHECK(strcmp(run.fixture_id, EEPROM_4K_ROM) == 0);
  CHECK(run_runner_direct(sram_rom, SRAM_PROFILE, SUPERVISED_BOUND, &run));
  CHECK(strcmp(run.outcome, "pass") == 0);
  CHECK(strcmp(run.fixture_id, SRAM_ROM) == 0);

  CHECK(run_runner_direct(eeprom_rom, SRAM_PROFILE, SUPERVISED_BOUND, &run));
  CHECK(run.parsed);
  CHECK(strcmp(run.outcome, "harness-error") == 0);
  CHECK(strcmp(run.detail, "refusal") == 0);
  CHECK(strcmp(run.stop_kind, "refused") == 0);
  CHECK(run.guest_tick == 0u);
  CHECK(run.fixture_id[0] == '\0');
  report_control("rom-profile-mismatch-refused", &run);
  CHECK(run_runner_direct(sram_rom, EEPROM_4K_PROFILE, SUPERVISED_BOUND, &run));
  CHECK(run.parsed);
  CHECK(strcmp(run.outcome, "harness-error") == 0);
  CHECK(strcmp(run.detail, "refusal") == 0);
  CHECK(strcmp(run.stop_kind, "refused") == 0);
  CHECK(run.guest_tick == 0u);
  CHECK(run.fixture_id[0] == '\0');
  report_control("rom-profile-mismatch-refused", &run);
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
    {"eeprom-16k-accepted", case_eeprom_16k_accepted, "accepted"},
    {"sram-accepted", case_sram_accepted, "accepted"},
    {"same-setup-wrong-byte-eeprom", case_same_setup_wrong_byte_eeprom,
     "controls"},
    {"same-setup-wrong-byte-sram", case_same_setup_wrong_byte_sram, "controls"},
    {"rom-profile-mismatch-refused", case_rom_profile_mismatch_refused,
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
  suite = argc == 8 && strcmp(argv[1], "--suite") == 0;
  if (!suite && argc != 7) {
    (void)fprintf(stderr,
                  "usage: %s CASE|--suite NAME SUPERVISOR TEST_SUPERVISOR "
                  "CHECKER ROOT RUNNER\n",
                  argv[0]);
    return 2;
  }
  base = suite ? 3u : 2u;
  g_supervisor = argv[base];
  g_checker = argv[base + 2u];
  g_root = argv[base + 3u];
  g_runner = argv[base + 4u];
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
