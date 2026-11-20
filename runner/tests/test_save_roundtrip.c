/*
 * Phase 9 save import and export through the real supervised runner (plan
 * 09-05). The authored save-roundtrip ROM reads an SRAM image the host
 * imported through a verified descriptor, rewrites two blocks and prints four
 * lines; the runner then exports the rewritten image through the no-clobber
 * publisher after a pass outcome. Expected transcripts and file digests are
 * literals taken from fixtures/save/roundtrip_oracle.py and asserted equal to
 * the independent in-test derivation below; nothing is taken from a production
 * helper. Every image is an index-derived authored pattern generated at test
 * time into a private temporary directory and removed afterwards; no ROM,
 * save or firmware exists anywhere in this test. The byte-order selections are
 * authored policy (assumption A1), not a hardware fact.
 *
 * usage: test_save_roundtrip SUPERVISOR TEST_SUPERVISOR CHECKER ROOT RUNNER
 *        test_save_roundtrip --case NAME SUPERVISOR TEST_SUPERVISOR CHECKER
 *                            ROOT RUNNER
 *        test_save_roundtrip --list-cases
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
    /* A racing mkdir that finds the directory already present is success. */
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

/* Every save option is passed as text, so a control can hand the supervisor a
 * malformed value; NULL leaves the option out. */
typedef struct {
  const char *supervisor;
  const char *fixture;
  uint64_t bound; /* 0 leaves the option out */
  const char *import_path;
  const char *import_size;
  const char *import_sha256;
  const char *kind;
  const char *format;
  const char *leaf;
  const char *artifact_dir;
  const char *artifact_max;
} run_spec;

#define MAX_ARGS 40u

typedef struct {
  char *items[MAX_ARGS];
  size_t count;
} arg_list;

static void arg_add(arg_list *list, char *value) {
  if (list->count < MAX_ARGS - 1u) list->items[list->count++] = value;
}

static void arg_add_pair(arg_list *list, const char *option,
                         const char *value) {
  if (value == NULL) return;
  arg_add(list, (char *)(uintptr_t)option);
  arg_add(list, (char *)(uintptr_t)value);
}

/* The authored ROM reads no input schedule: the default empty one is used. */
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
  arg_add_pair(&list, "--save-import", spec->import_path);
  arg_add_pair(&list, "--save-import-size", spec->import_size);
  arg_add_pair(&list, "--save-import-sha256", spec->import_sha256);
  arg_add_pair(&list, "--save-kind", spec->kind);
  arg_add_pair(&list, "--save-format", spec->format);
  arg_add_pair(&list, "--save-export-leaf", spec->leaf);
  arg_add_pair(&list, "--artifact-dir", spec->artifact_dir);
  arg_add_pair(&list, "--artifact-max-bytes", spec->artifact_max);
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

/* ---- policy literals ---------------------------------------------------- */

#define SAVE_BOUND UINT64_C(600000)
#define ROUNDTRIP_ROM "phase09-save-roundtrip-s32k"
#define ROUNDTRIP_PROFILE "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-s32k"
#define ROUNDTRIP_BYTES 144u
/* Oracle scenario baseline (fixtures/save/roundtrip_oracle.py --sha256), also
 * the independent shasum of the four hand-written lines. */
#define ROUNDTRIP_SHA256 \
  "649b62940ace25d9e4c31e12381f38df779a5bbef90dcf0baf1be1458e6b75c4"

static void expect_pass(const run_out *run) {
  CHECK(run->parsed);
  CHECK(run->checker_ok);
  CHECK(run->supervisor_exit == 0);
  CHECK(strcmp(run->outcome, "pass") == 0);
  CHECK(strcmp(run->detail, "positive-match") == 0);
  CHECK(strcmp(run->stop_kind, "authored-completion") == 0);
}

/* ---- authored images and files (independent of the oracle and the core) ---- */

#define SRAM_BYTES 32768u
#define W_OFFSET 0x0100u
#define V_OFFSET 0x7FF0u
#define BLOCK_BYTES 16u

/* Authored formula: byte i is (37 i + 11 + 101 (i >> 8)) modulo 256. */
static uint8_t image_byte(size_t index) {
  return (uint8_t)((37u * (unsigned)index + 11u +
                    101u * (unsigned)(index >> 8)) &
                   0xFFu);
}

static void fill_image(uint8_t *bytes, size_t length) {
  size_t i;
  for (i = 0u; i < length; i++) bytes[i] = image_byte(i);
}

/* The guest literals of fixtures/save/roundtrip.S. */
static const uint8_t w_block[BLOCK_BYTES] = {
    0xc3, 0x5a, 0x96, 0xe1, 0x0f, 0x78, 0xb2, 0x4d,
    0x21, 0x8e, 0xf6, 0x34, 0x69, 0xd0, 0x17, 0xab};
static const uint8_t v_block[BLOCK_BYTES] = {
    0x18, 0xe5, 0x7c, 0xb9, 0x03, 0xd6, 0x4a, 0x91,
    0xff, 0x2e, 0xa7, 0x60, 0xc8, 0x35, 0x8b, 0x5e};

/* The image the guest leaves behind: the pattern with W and V rewritten. */
static void rewrite_image(uint8_t *image) {
  memcpy(image + W_OFFSET, w_block, BLOCK_BYTES);
  memcpy(image + V_OFFSET, v_block, BLOCK_BYTES);
}

static void sha_hex_of(const uint8_t *data, size_t length, char out[65]) {
  t64_sha256_ctx ctx;
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  t64_sha256_init(&ctx);
  t64_sha256_update(&ctx, data, length);
  t64_sha256_final(&ctx, digest);
  t64_sha256_hex(digest, out);
}

/* A new 0600 file; fails when it exists. */
static bool write_new_file(const char *path, const uint8_t *data,
                           size_t length) {
  size_t done = 0u;
  int fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
  if (fd < 0) return false;
  while (done < length) {
    ssize_t count = write(fd, data + done, length - done);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) {
      (void)close(fd);
      return false;
    }
    done += (size_t)count;
  }
  return close(fd) == 0;
}

/* A private 0700 directory for exported leaves (the publisher requires an
 * owner-only directory), created under the evidence directory. */
static bool make_private_dir(char out[PATH_MAX]) {
  if (snprintf(out, PATH_MAX, "%s/exp.XXXXXX", g_evidence) >= PATH_MAX) {
    return false;
  }
  return mkdtemp(out) != NULL;
}

/* Counts the directory entries other than the dot entries (a published leaf
 * is one; a left-over stage directory would be another). */
static int count_entries(const char *directory) {
  DIR *dir = opendir(directory);
  struct dirent *entry;
  int count = 0;
  if (dir == NULL) return -1;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
      count++;
    }
  }
  (void)closedir(dir);
  return count;
}

/* Removes the single level of files in a private directory, then the
 * directory (the images are removed after each case). */
static void remove_dir_tree(const char *directory) {
  DIR *dir = opendir(directory);
  struct dirent *entry;
  char path[PATH_MAX];
  if (dir != NULL) {
    while ((entry = readdir(dir)) != NULL) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
        continue;
      }
      if (join_path(path, sizeof path, directory, entry->d_name)) {
        (void)unlink(path);
      }
    }
    (void)closedir(dir);
  }
  (void)rmdir(directory);
}

/* ---- literal digests (fixtures/save/roundtrip_oracle.py --image-sha256) ---- */

#define SRAM_IMPORT_RAW_SHA256 \
  "a4851725208bbacc91017a0663796acb58ad12a8b1061100894fc8df145a9c5d"
#define SRAM_EXPORT_RAW_SHA256 \
  "7ffc21818eb8b3f3802176937dfa7dbedb98cdb3f44afbfb467c95e4b9d3e588"

/* ---- cases ---------------------------------------------------------------- */

/* The baseline: the authored image imported RAW through a verified descriptor,
 * rewritten by the guest and exported RAW through the publisher. */
static int case_sram_roundtrip(void) {
  static uint8_t image[SRAM_BYTES];
  static uint8_t expected[SRAM_BYTES];
  static uint8_t published[SRAM_BYTES + 1u];
  char rom[PATH_MAX];
  char dir[PATH_MAX];
  char import_path[PATH_MAX];
  char leaf_path[PATH_MAX];
  char import_sha[65];
  char expected_sha[65];
  char published_sha[65];
  size_t published_length = 0u;
  struct stat info;
  run_spec spec = {0};
  run_out run;

  fill_image(image, sizeof image);
  sha_hex_of(image, sizeof image, import_sha);
  CHECK(strcmp(import_sha, SRAM_IMPORT_RAW_SHA256) == 0);
  memcpy(expected, image, sizeof image);
  rewrite_image(expected);
  sha_hex_of(expected, sizeof expected, expected_sha);
  CHECK(strcmp(expected_sha, SRAM_EXPORT_RAW_SHA256) == 0);

  CHECK(fixture_path(rom, ROUNDTRIP_ROM));
  CHECK(make_private_dir(dir));
  CHECK(join_path(import_path, sizeof import_path, dir, "import.sav"));
  CHECK(write_new_file(import_path, image, sizeof image));
  {
    char export_dir[PATH_MAX];
    CHECK(make_private_dir(export_dir));
    spec.supervisor = g_supervisor;
    spec.fixture = rom;
    spec.bound = SAVE_BOUND;
    spec.import_path = import_path;
    spec.import_size = "32768";
    spec.import_sha256 = import_sha;
    spec.kind = "sram32k";
    spec.format = "raw-v1";
    spec.leaf = "roundtrip.sra";
    spec.artifact_dir = export_dir;
    spec.artifact_max = "32768";
    CHECK(run_supervised(&spec, &run));
    (void)fprintf(stderr,
                  "roundtrip: outcome=%s detail=%s stop=%s log_bytes=%llu "
                  "tick=%llu\n",
                  run.outcome, run.detail, run.stop_kind,
                  (unsigned long long)run.log_bytes,
                  (unsigned long long)run.guest_tick);
    CHECK(join_path(leaf_path, sizeof leaf_path, export_dir, "roundtrip.sra"));
    if (stat(leaf_path, &info) == 0) {
      CHECK((info.st_mode & 0777) == 0600);
      CHECK((uint64_t)info.st_size == SRAM_BYTES);
    } else {
      CHECK(false);
    }
    CHECK(read_file(leaf_path, published, sizeof published, &published_length));
    CHECK(published_length == SRAM_BYTES);
    sha_hex_of(published, published_length, published_sha);
    CHECK(strcmp(published_sha, SRAM_EXPORT_RAW_SHA256) == 0);
    CHECK(published_length == SRAM_BYTES &&
          memcmp(published, expected, SRAM_BYTES) == 0);
    /* The leaf is the only entry: no stage directory is left behind. */
    CHECK(count_entries(export_dir) == 1);
    remove_dir_tree(export_dir);
  }
  remove_dir_tree(dir);
  expect_pass(&run);
  CHECK(strcmp(run.fixture_id, ROUNDTRIP_ROM) == 0);
  CHECK(strcmp(run.profile_id, ROUNDTRIP_PROFILE) == 0);
  CHECK(run.log_bytes == ROUNDTRIP_BYTES);
  CHECK(strcmp(run.log_sha256, ROUNDTRIP_SHA256) == 0);
  CHECK(run.guest_tick < SAVE_BOUND);
  if (g_failures == 0) {
    (void)printf("case reached: sram-roundtrip completion_tick=%llu\n",
                 (unsigned long long)run.guest_tick);
  }
  return g_failures == 0 ? 0 : 1;
}

/* ---- dispatch --------------------------------------------------------------- */

typedef struct {
  const char *name;
  int (*run)(void);
} case_entry;

static const case_entry cases[] = {
    {"sram-roundtrip", case_sram_roundtrip},
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
  g_checker = argv[base + 2u];
  g_root = argv[base + 3u];
  if (!prepare_evidence()) {
    (void)fprintf(stderr, "evidence directory unavailable\n");
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
