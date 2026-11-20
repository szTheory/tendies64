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
#include "t64_save_flags.h"
#include "t64_sha256.h"
#include "t64_test_tree.h"

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
    /* A racing mkdir that finds the directory already present is success. */
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
  uint64_t advance_step;  /* the test twin only; 0 leaves it out */
  int video_fd;           /* the test twin only; 0 leaves it out */
  int audio_fd;           /* the test twin only; 0 leaves it out */
  const char *raw_capture_dir;
} run_spec;

#define MAX_ARGS 64u

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
  char advance_text[24];
  char video_text[24];
  char audio_text[24];
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
  arg_add_pair(&list, "--raw-capture-dir", spec->raw_capture_dir);
  if (spec->advance_step != 0u) {
    (void)snprintf(advance_text, sizeof advance_text, "%llu",
                   (unsigned long long)spec->advance_step);
    arg_add_pair(&list, "--advance-step", advance_text);
  }
  if (spec->video_fd != 0) {
    (void)snprintf(video_text, sizeof video_text, "%d", spec->video_fd);
    arg_add_pair(&list, "--video-observation-fd", video_text);
  }
  if (spec->audio_fd != 0) {
    (void)snprintf(audio_text, sizeof audio_text, "%d", spec->audio_fd);
    arg_add_pair(&list, "--audio-observation-fd", audio_text);
  }
  list.items[list.count] = NULL;
  pid = fork();
  if (pid < 0) return false;
  if (pid == 0) {
    /* What the supervisor prints is kept for the no-save-bytes scan. */
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

/* ---- more literals and helpers for the format, refusal and publication cases -- */

#define EEPROM_4K_BYTES 512u
#define EEPROM_16K_BYTES 2048u
#define ADMIT_E4K_ROM "phase09-admit-e4k"
#define ADMIT_E16K_ROM "phase09-admit-e16k"
#define ADMIT_STEM_ROM "phase09-admit-si-v1"
#define ADMIT_E4K_PROFILE "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e4k"
#define ADMIT_E16K_PROFILE "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e16k"
#define ADMIT_BYTES 20u
/* The admit transcripts (independently pinned in 09-02). */
#define ADMIT_E4K_SHA256 \
  "bd084623780adfd35591144d77f299f3107a3399fe536230c6c891ed7de4f015"
#define ADMIT_E16K_SHA256 \
  "c8ba071062fca5ba87baf198de9cf41bf387fa286754844410b644907eaa2bba"
/* Oracle scenario swap16-as-raw (fixtures/save/roundtrip_oracle.py --sha256),
 * also the independent shasum of the four hand-derived lines. */
#define SWAP16_AS_RAW_SHA256 \
  "3717361f6b106331021025e25a738598b01a52bffb68bd33507b0ec948369128"

/* The three selections: the token, the XOR mask of the offset mapping and the
 * oracle digests (--image-sha256 import and export) of each kind. The EEPROM
 * admit ROMs write nothing, so their import and export digests are equal. */
typedef struct {
  const char *token;
  unsigned mask;
  const char *sram_import;
  const char *sram_export;
  const char *eeprom4k;
  const char *eeprom16k;
} format_row;

static const format_row format_rows[3] = {
    {"raw-v1", 0u,
     "a4851725208bbacc91017a0663796acb58ad12a8b1061100894fc8df145a9c5d",
     "7ffc21818eb8b3f3802176937dfa7dbedb98cdb3f44afbfb467c95e4b9d3e588",
     "cd2e4a856f596727b039028313ca8d3556a46e82deb7980021dcdfac0840440b",
     "849ee493e7d9f2544d84fa3b7fcaa747a38c8d0d6ba79e57dfa005c58e1ebb39"},
    {"swap16-v1", 1u,
     "06677b257fe8f10fbc0c98e2df281bb88c207403b3a1d14aa3d77ece6bc1af75",
     "72a127590999f35722b42409f542226ec288d00316fecb4aa83ce528efec1b33",
     "e417bc4d4be5242179a7ffbbbb71f04e37cf5a0d1b32801c538c012474ca0f11",
     "55c8d93c9c6744ad62011b4fa3456a98730376a0f0cd212f832a53d1f2fb2d09"},
    {"wordrev32-v1", 3u,
     "c21e698fc968f0b2bde2d06b0b710f92d75adcd1f5c872d479b9c7a8515477e7",
     "3cc27bde2b0c8d2196e4c3a7bf8d4f11e74964717f7ae92b5bdec3cc0b3b5a41",
     "be5d23c803e41af29b81bd65a753c9ef596f5744ded227e6ab518b18f0f87c4f",
     "0210891e2c384c0a89a38d6ee94e630e095532514fb3f8f648fc3663bf1472d2"},
};

/* The offset mapping of a selection: out[i] = raw[i XOR mask]. */
static void encode_image(const uint8_t *raw, size_t length, unsigned mask,
                         uint8_t *out) {
  size_t i;
  for (i = 0u; i < length; i++) out[i] = raw[i ^ (size_t)mask];
}

static void hex_text(const uint8_t *bytes, size_t length, char *out) {
  static const char digits[] = "0123456789abcdef";
  size_t i;
  for (i = 0u; i < length; i++) {
    out[2u * i] = digits[bytes[i] >> 4];
    out[2u * i + 1u] = digits[bytes[i] & 0x0fu];
  }
  out[2u * length] = '\0';
}

static uint8_t image_raw[SRAM_BYTES + 8u];
static uint8_t image_encoded[SRAM_BYTES];
static uint8_t image_final[SRAM_BYTES];
static uint8_t published_bytes[SRAM_BYTES + 1u];
static uint8_t converted_bytes[SRAM_BYTES];

/* Stages an import file of `length` bytes in a fresh private directory and
 * fills in its path and SHA-256. */
static bool stage_import(const uint8_t *bytes, size_t length,
                         char dir[PATH_MAX], char path[PATH_MAX],
                         char sha[65]) {
  if (!make_private_dir(dir) || !join_path(path, PATH_MAX, dir, "import.dat") ||
      !write_new_file(path, bytes, length)) {
    return false;
  }
  sha_hex_of(bytes, length, sha);
  return true;
}

/* The published leaf: exactly this mode, size and digest. */
static void check_leaf(const char *directory, const char *leaf, size_t size,
                       const char *expected_sha) {
  char path[PATH_MAX];
  char sha[65];
  size_t length = 0u;
  struct stat info;
  CHECK(join_path(path, sizeof path, directory, leaf));
  if (stat(path, &info) != 0) {
    CHECK(false);
    return;
  }
  CHECK((info.st_mode & 0777) == 0600);
  CHECK((uint64_t)info.st_size == (uint64_t)size);
  CHECK(read_file(path, published_bytes, sizeof published_bytes, &length));
  CHECK(length == size);
  sha_hex_of(published_bytes, length, sha);
  CHECK(strcmp(sha, expected_sha) == 0);
}

static void expect_usage(const run_out *run) {
  CHECK(run->parsed);
  CHECK(run->checker_ok);
  CHECK(strcmp(run->outcome, "harness-error") == 0);
  CHECK(strcmp(run->detail, "usage") == 0);
  CHECK(strcmp(run->stop_kind, "not-started") == 0);
  CHECK(run->guest_tick == 0u);
}

/* The supervisor refuses before launch (a verified-input failure). */
static void expect_supervisor_refusal(const run_out *run) {
  CHECK(run->parsed);
  CHECK(run->checker_ok);
  CHECK(strcmp(run->outcome, "harness-error") == 0);
  CHECK(strcmp(run->detail, "refusal") == 0);
  CHECK(strcmp(run->stop_kind, "refused") == 0);
  CHECK(run->guest_tick == 0u);
}

/* The runner refuses before the guest starts: the supervised record is its
 * post-launch classification (report-error, capture-invalid, guest tick 0). */
static void expect_runner_refusal(const run_out *run) {
  CHECK(run->parsed);
  CHECK(run->checker_ok);
  CHECK(strcmp(run->outcome, "harness-error") == 0);
  CHECK(strcmp(run->stop_kind, "report-error") == 0);
  CHECK(strcmp(run->detail, "capture-invalid") == 0);
  CHECK(run->guest_tick == 0u);
}

static void expect_host_error(const run_out *run, const char *detail) {
  CHECK(run->parsed);
  CHECK(run->checker_ok);
  CHECK(strcmp(run->outcome, "harness-error") == 0);
  CHECK(strcmp(run->stop_kind, "host-error") == 0);
  CHECK(strcmp(run->detail, detail) == 0);
}

/* The roundtrip run of the SRAM ROM with the given tokens, import image and
 * leaf, into a fresh export directory; fills `import_dir` and `export_dir`. */
static bool roundtrip_run(const char *format, const uint8_t *import_bytes,
                          size_t import_length, const char *leaf,
                          char import_dir[PATH_MAX], char export_dir[PATH_MAX],
                          run_out *run) {
  char rom[PATH_MAX];
  char import_path[PATH_MAX];
  char sha[65];
  char size_text[24];
  run_spec spec = {0};
  if (!fixture_path(rom, ROUNDTRIP_ROM) ||
      !stage_import(import_bytes, import_length, import_dir, import_path,
                    sha) ||
      !make_private_dir(export_dir)) {
    return false;
  }
  (void)snprintf(size_text, sizeof size_text, "%zu", import_length);
  spec.supervisor = g_supervisor;
  spec.fixture = rom;
  spec.bound = SAVE_BOUND;
  spec.import_path = import_path;
  spec.import_size = size_text;
  spec.import_sha256 = sha;
  spec.kind = "sram32k";
  spec.format = format;
  spec.leaf = leaf;
  spec.artifact_dir = export_dir;
  spec.artifact_max = "32768";
  return run_supervised(&spec, run);
}

static void expect_roundtrip_pass(const run_out *run) {
  expect_pass(run);
  CHECK(strcmp(run->fixture_id, ROUNDTRIP_ROM) == 0);
  CHECK(strcmp(run->profile_id, ROUNDTRIP_PROFILE) == 0);
  CHECK(run->log_bytes == ROUNDTRIP_BYTES);
  CHECK(strcmp(run->log_sha256, ROUNDTRIP_SHA256) == 0);
}

/* ---- format-matrix ----------------------------------------------------------- */

static int case_format_matrix(void) {
  /* (import format, export format) as indexes of format_rows. The runner takes
   * one --save-format for the import and the export (the flag set of 09-05),
   * so a cross pair is run in its import format and the published file is
   * converted to the export format by the independent mapping before its
   * digest is compared with the oracle value of that format. */
  static const unsigned pairs[5][2] = {{0, 0}, {1, 1}, {2, 2}, {1, 0}, {2, 1}};
  char same_format_sha[3][65] = {{0}};
  size_t i;

  fill_image(image_raw, SRAM_BYTES);
  memcpy(image_final, image_raw, SRAM_BYTES);
  rewrite_image(image_final);
  for (i = 0u; i < 5u; i++) {
    const format_row *in = &format_rows[pairs[i][0]];
    const format_row *out = &format_rows[pairs[i][1]];
    char import_dir[PATH_MAX];
    char export_dir[PATH_MAX];
    char import_sha[65];
    char derived_sha[65];
    run_out run;

    encode_image(image_raw, SRAM_BYTES, in->mask, image_encoded);
    sha_hex_of(image_encoded, SRAM_BYTES, import_sha);
    CHECK(strcmp(import_sha, in->sram_import) == 0);
    CHECK(roundtrip_run(in->token, image_encoded, SRAM_BYTES, "roundtrip.sra",
                        import_dir, export_dir, &run));
    expect_roundtrip_pass(&run);
    /* The published file is the final image encoded in the run's format. */
    encode_image(image_final, SRAM_BYTES, in->mask, converted_bytes);
    sha_hex_of(converted_bytes, SRAM_BYTES, derived_sha);
    CHECK(strcmp(derived_sha, in->sram_export) == 0);
    check_leaf(export_dir, "roundtrip.sra", SRAM_BYTES, in->sram_export);
    if (pairs[i][0] == pairs[i][1]) {
      sha_hex_of(published_bytes, SRAM_BYTES, same_format_sha[pairs[i][0]]);
    } else {
      /* Convert to the export format: decode with the run's mapping, encode
         with the export mapping; the digest equals the oracle value. */
      char converted_sha[65];
      encode_image(published_bytes, SRAM_BYTES, in->mask, image_encoded);
      encode_image(image_encoded, SRAM_BYTES, out->mask, converted_bytes);
      sha_hex_of(converted_bytes, SRAM_BYTES, converted_sha);
      CHECK(strcmp(converted_sha, out->sram_export) == 0);
    }
    remove_dir_tree(export_dir);
    remove_dir_tree(import_dir);
  }
  /* The three same-format exports differ pairwise. */
  CHECK(strcmp(same_format_sha[0], same_format_sha[1]) != 0);
  CHECK(strcmp(same_format_sha[0], same_format_sha[2]) != 0);
  CHECK(strcmp(same_format_sha[1], same_format_sha[2]) != 0);
  return g_failures == 0 ? 0 : 1;
}

/* ---- eeprom-passthrough --------------------------------------------------- */

typedef struct {
  const char *rom;
  const char *profile;
  const char *kind;
  const char *transcript_sha;
  const char *leaf;
  size_t size;
} eeprom_row;

static const eeprom_row eeprom_rows[2] = {
    {ADMIT_E4K_ROM, ADMIT_E4K_PROFILE, "eeprom4k", ADMIT_E4K_SHA256, "e4k.eep",
     EEPROM_4K_BYTES},
    {ADMIT_E16K_ROM, ADMIT_E16K_PROFILE, "eeprom16k", ADMIT_E16K_SHA256,
     "e16k.eep", EEPROM_16K_BYTES},
};

static int case_eeprom_passthrough(void) {
  size_t row;
  size_t f;

  for (row = 0u; row < 2u; row++) {
    const eeprom_row *eeprom = &eeprom_rows[row];
    for (f = 0u; f < 3u; f++) {
      const format_row *selection = &format_rows[f];
      const char *oracle_import =
          row == 0u ? selection->eeprom4k : selection->eeprom16k;
      char rom[PATH_MAX];
      char import_dir[PATH_MAX];
      char import_path[PATH_MAX];
      char export_dir[PATH_MAX];
      char sha[65];
      char size_text[24];
      size_t other;
      run_spec spec = {0};
      run_out run;

      fill_image(image_raw, eeprom->size);
      encode_image(image_raw, eeprom->size, selection->mask, image_encoded);
      sha_hex_of(image_encoded, eeprom->size, sha);
      CHECK(strcmp(sha, oracle_import) == 0);
      CHECK(fixture_path(rom, eeprom->rom));
      CHECK(stage_import(image_encoded, eeprom->size, import_dir, import_path,
                         sha));
      CHECK(make_private_dir(export_dir));
      (void)snprintf(size_text, sizeof size_text, "%zu", eeprom->size);
      spec.supervisor = g_supervisor;
      spec.fixture = rom;
      spec.bound = SAVE_BOUND;
      spec.import_path = import_path;
      spec.import_size = size_text;
      spec.import_sha256 = sha;
      spec.kind = eeprom->kind;
      spec.format = selection->token;
      spec.leaf = eeprom->leaf;
      spec.artifact_dir = export_dir;
      spec.artifact_max = size_text;
      CHECK(run_supervised(&spec, &run));
      expect_pass(&run);
      CHECK(strcmp(run.fixture_id, eeprom->rom) == 0);
      CHECK(strcmp(run.profile_id, eeprom->profile) == 0);
      CHECK(run.log_bytes == ADMIT_BYTES);
      CHECK(strcmp(run.log_sha256, eeprom->transcript_sha) == 0);
      /* No guest write: the exported file is the imported file. */
      check_leaf(export_dir, eeprom->leaf, eeprom->size, oracle_import);
      /* An export in a different format equals the oracle digest of the image
         in that format (the file is converted by the independent mapping). */
      for (other = 0u; other < 3u; other++) {
        const char *want = row == 0u ? format_rows[other].eeprom4k
                                     : format_rows[other].eeprom16k;
        char converted_sha[65];
        encode_image(published_bytes, eeprom->size, selection->mask,
                     image_final);
        encode_image(image_final, eeprom->size, format_rows[other].mask,
                     converted_bytes);
        sha_hex_of(converted_bytes, eeprom->size, converted_sha);
        CHECK(strcmp(converted_sha, want) == 0);
      }
      remove_dir_tree(export_dir);
      remove_dir_tree(import_dir);
    }
  }
  return g_failures == 0 ? 0 : 1;
}

/* ---- same-setup-wrong-format ------------------------------------------------- */

static int case_same_setup_wrong_format(void) {
  char import_dir[PATH_MAX];
  char export_dir[PATH_MAX];
  run_out run;

  fill_image(image_raw, SRAM_BYTES);
  /* The accepted baseline of this setup. */
  CHECK(roundtrip_run("raw-v1", image_raw, SRAM_BYTES, "roundtrip.sra",
                      import_dir, export_dir, &run));
  expect_roundtrip_pass(&run);
  check_leaf(export_dir, "roundtrip.sra", SRAM_BYTES,
             format_rows[0].sram_export);
  remove_dir_tree(export_dir);
  remove_dir_tree(import_dir);

  /* An image encoded SWAP16 but declared raw-v1 reaches the transcript
     comparator: guest-fail, wrong-transcript, the oracle scenario digest, and
     nothing is published. */
  encode_image(image_raw, SRAM_BYTES, 1u, image_encoded);
  CHECK(roundtrip_run("raw-v1", image_encoded, SRAM_BYTES, "roundtrip.sra",
                      import_dir, export_dir, &run));
  CHECK(run.parsed);
  CHECK(run.checker_ok);
  CHECK(strcmp(run.outcome, "guest-fail") == 0);
  CHECK(strcmp(run.detail, "wrong-transcript") == 0);
  CHECK(strcmp(run.stop_kind, "authored-completion") == 0);
  CHECK(run.log_bytes == ROUNDTRIP_BYTES);
  CHECK(strcmp(run.log_sha256, SWAP16_AS_RAW_SHA256) == 0);
  CHECK(strcmp(run.log_sha256, ROUNDTRIP_SHA256) != 0);
  CHECK(count_entries(export_dir) == 0);
  remove_dir_tree(export_dir);
  remove_dir_tree(import_dir);

  /* A SWAP16 file declared swap16-v1 and read back: the published file is the
     SWAP16 encoding of the final image; read in the WORDREV32 selection (the
     independent mapping) its digest equals the oracle value, which differs
     from the oracle RAW value. */
  CHECK(roundtrip_run("swap16-v1", image_encoded, SRAM_BYTES, "roundtrip.sra",
                      import_dir, export_dir, &run));
  expect_roundtrip_pass(&run);
  check_leaf(export_dir, "roundtrip.sra", SRAM_BYTES,
             format_rows[1].sram_export);
  {
    char converted_sha[65];
    encode_image(published_bytes, SRAM_BYTES, 1u, image_final);
    encode_image(image_final, SRAM_BYTES, 3u, converted_bytes);
    sha_hex_of(converted_bytes, SRAM_BYTES, converted_sha);
    CHECK(strcmp(converted_sha, format_rows[2].sram_export) == 0);
    CHECK(strcmp(converted_sha, format_rows[0].sram_export) != 0);
  }
  remove_dir_tree(export_dir);
  remove_dir_tree(import_dir);
  return g_failures == 0 ? 0 : 1;
}

/* ---- import-refused ------------------------------------------------------------ */

typedef struct {
  const char *rom;
  const char *kind;
  const char *format;
  size_t length;       /* bytes of the import file */
  const char *size_text;   /* NULL: the decimal length */
  bool wrong_sha;
  bool omit_size;
  bool omit_sha;
  bool omit_kind;
  bool omit_format;
} refusal_row;

typedef enum {
  CLASS_USAGE,
  CLASS_SUPERVISOR_REFUSAL,
  CLASS_RUNNER_REFUSAL,
  CLASS_HOST_ERROR
} refusal_class;

static void run_refusal(const refusal_row *row, refusal_class want,
                        const char *label) {
  char rom[PATH_MAX];
  char import_dir[PATH_MAX];
  char import_path[PATH_MAX];
  char export_dir[PATH_MAX];
  char sha[65];
  char size_text[24];
  run_spec spec = {0};
  run_out run;

  fill_image(image_raw, SRAM_BYTES + 8u);
  CHECK(fixture_path(rom, row->rom));
  CHECK(stage_import(image_raw, row->length, import_dir, import_path, sha));
  CHECK(make_private_dir(export_dir));
  (void)snprintf(size_text, sizeof size_text, "%zu", row->length);
  if (row->wrong_sha) sha[0] = sha[0] == '0' ? '1' : '0';
  spec.supervisor = g_supervisor;
  spec.fixture = rom;
  spec.bound = SAVE_BOUND;
  spec.import_path = import_path;
  spec.import_size = row->omit_size ? NULL
                                    : (row->size_text != NULL ? row->size_text
                                                              : size_text);
  spec.import_sha256 = row->omit_sha ? NULL : sha;
  spec.kind = row->omit_kind ? NULL : row->kind;
  spec.format = row->omit_format ? NULL : row->format;
  spec.leaf = "roundtrip.sra";
  spec.artifact_dir = export_dir;
  spec.artifact_max = "32768";
  CHECK(run_supervised(&spec, &run));
  (void)fprintf(stderr, "import-refused '%s': outcome=%s detail=%s stop=%s "
                "tick=%llu\n", label, run.outcome, run.detail, run.stop_kind,
                (unsigned long long)run.guest_tick);
  if (want == CLASS_USAGE) expect_usage(&run);
  else if (want == CLASS_SUPERVISOR_REFUSAL) expect_supervisor_refusal(&run);
  else expect_runner_refusal(&run);
  CHECK(count_entries(export_dir) == 0);
  remove_dir_tree(export_dir);
  remove_dir_tree(import_dir);
}

/* ---- direct --save-import-fd (plan 09-10, FX-12; R-04, VG-07) ----------- */

/* The child's own descriptor option, run without the supervisor. Every other
 * child descriptor option has a direct-runner use in a test; this one was
 * reached only through the supervisor's forwarding. The runner accepts only
 * an unlinked, read-only regular file at offset 0 of exactly the kind size and
 * ends any other descriptor in the existing prelaunch refusal at guest tick 0;
 * the grammar rule of kind and format is a usage refusal. */
typedef enum {
  SAVE_FD_ACCEPTED,
  SAVE_FD_LINKED,
  SAVE_FD_WRITABLE,
  SAVE_FD_OFFSET,
  SAVE_FD_SHORT,
  SAVE_FD_PIPE,
  SAVE_FD_NO_KIND
} save_fd_mode;

/* An unlinked read-only descriptor on a private copy of `source` (the runner's
 * own fixture and input intake accept only such snapshots). */
static int snapshot_fd(const char *source, const uint8_t *literal,
                       size_t literal_length, const char *directory,
                       int open_flags, bool unlink_it) {
  static uint8_t bytes[SRAM_BYTES];
  size_t length = 0u;
  char name[PATH_MAX];
  int fd;
  if (source != NULL) {
    if (!read_file(source, bytes, sizeof bytes, &length) || length == 0u ||
        length >= sizeof bytes) {
      return -1;
    }
    literal = bytes;
    literal_length = length;
  }
  if (!join_path(name, sizeof name, directory, "snapshot.XXXXXX")) return -1;
  fd = mkstemp(name);
  if (fd < 0) return -1;
  {
    size_t written = 0u;
    while (written < literal_length) {
      ssize_t count = write(fd, literal + written, literal_length - written);
      if (count < 0 && errno == EINTR) continue;
      if (count <= 0) {
        (void)close(fd);
        (void)unlink(name);
        return -1;
      }
      written += (size_t)count;
    }
  }
  (void)close(fd);
  fd = open(name, open_flags);
  if (unlink_it) (void)unlink(name);
  return fd;
}

static bool run_save_direct(save_fd_mode mode, run_out *out) {
  char runner[PATH_MAX];
  char rom[PATH_MAX];
  char input[PATH_MAX];
  char stdout_path[PATH_MAX];
  char fixture_fd_text[24];
  char input_fd_text[24];
  char save_fd_text[24];
  char *slash;
  int fixture_fd;
  int input_fd;
  int save_fd = -1;
  int pipe_fds[2] = {-1, -1};
  pid_t pid;
  int status = 0;
  static uint8_t image[SRAM_BYTES];
  size_t image_length = SRAM_BYTES;
  memset(out, 0, sizeof *out);
  if (snprintf(runner, sizeof runner, "%s", g_supervisor) >= (int)sizeof runner ||
      (slash = strrchr(runner, '/')) == NULL ||
      (size_t)(slash - runner) + sizeof "/t64-runner-real" > sizeof runner) {
    return false;
  }
  (void)snprintf(slash, sizeof runner - (size_t)(slash - runner),
                 "/t64-runner-real");
  if (!make_run_dir(out->dir) || !fixture_path(rom, ROUNDTRIP_ROM) ||
      !input_path(input) ||
      !join_path(stdout_path, sizeof stdout_path, out->dir, "runner.json")) {
    return false;
  }
  fill_image(image, SRAM_BYTES);
  if (mode == SAVE_FD_SHORT) image_length = SRAM_BYTES - 1u;
  fixture_fd = snapshot_fd(rom, NULL, 0u, out->dir, O_RDONLY, true);
  input_fd = snapshot_fd(input, NULL, 0u, out->dir, O_RDONLY, true);
  switch (mode) {
    case SAVE_FD_LINKED:
      save_fd = snapshot_fd(NULL, image, image_length, out->dir, O_RDONLY,
                            false);
      break;
    case SAVE_FD_WRITABLE:
      save_fd = snapshot_fd(NULL, image, image_length, out->dir, O_RDWR, true);
      break;
    case SAVE_FD_OFFSET:
      save_fd = snapshot_fd(NULL, image, image_length, out->dir, O_RDONLY,
                            true);
      if (save_fd >= 0 && lseek(save_fd, 1, SEEK_SET) != 1) return false;
      break;
    case SAVE_FD_PIPE:
      if (pipe(pipe_fds) != 0) return false;
      save_fd = pipe_fds[0];
      break;
    default:
      save_fd = snapshot_fd(NULL, image, image_length, out->dir, O_RDONLY,
                            true);
      break;
  }
  if (fixture_fd <= STDERR_FILENO || input_fd <= STDERR_FILENO ||
      save_fd <= STDERR_FILENO) {
    return false;
  }
  (void)snprintf(fixture_fd_text, sizeof fixture_fd_text, "%d", fixture_fd);
  (void)snprintf(input_fd_text, sizeof input_fd_text, "%d", input_fd);
  (void)snprintf(save_fd_text, sizeof save_fd_text, "%d", save_fd);
  pid = fork();
  if (pid < 0) return false;
  if (pid == 0) {
    char *args[24];
    size_t n = 0u;
    int out_fd = open(stdout_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (out_fd < 0 || dup2(out_fd, STDOUT_FILENO) < 0) _exit(126);
    args[n++] = runner;
    args[n++] = (char *)(uintptr_t)"--fixture-fd";
    args[n++] = fixture_fd_text;
    args[n++] = (char *)(uintptr_t)"--input-fd";
    args[n++] = input_fd_text;
    args[n++] = (char *)(uintptr_t)"--profile";
    args[n++] = (char *)(uintptr_t)ROUNDTRIP_PROFILE;
    args[n++] = (char *)(uintptr_t)"--guest-ticks";
    args[n++] = (char *)(uintptr_t)"600000";
    args[n++] = (char *)(uintptr_t)"--save-import-fd";
    args[n++] = save_fd_text;
    if (mode != SAVE_FD_NO_KIND) {
      args[n++] = (char *)(uintptr_t)"--save-kind";
      args[n++] = (char *)(uintptr_t)"sram32k";
    }
    args[n++] = (char *)(uintptr_t)"--save-format";
    args[n++] = (char *)(uintptr_t)"raw-v1";
    args[n] = NULL;
    execv(runner, args);
    _exit(127);
  }
  (void)close(fixture_fd);
  (void)close(input_fd);
  (void)close(save_fd);
  if (pipe_fds[1] >= 0) (void)close(pipe_fds[1]);
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) return false;
  }
  out->supervisor_exit = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  (void)snprintf(out->result_path, sizeof out->result_path, "%s", stdout_path);
  out->parsed = parse_record(stdout_path, out);
  return out->parsed;
}

static void expect_direct_refusal(const run_out *run, const char *detail,
                                  const char *stop_kind) {
  CHECK(run->parsed);
  CHECK(strcmp(run->outcome, "harness-error") == 0);
  CHECK(strcmp(run->detail, detail) == 0);
  CHECK(strcmp(run->stop_kind, stop_kind) == 0);
  CHECK(run->guest_tick == 0u);
}

static void direct_save_import_fd_rows(void) {
  run_out run;
  /* The accepted baseline of the same setup: an unlinked read-only descriptor
   * of exactly the kind size is imported (the transcript digest of the guest
   * that reads the image proves the bytes arrived) and the run passes. */
  CHECK(run_save_direct(SAVE_FD_ACCEPTED, &run));
  CHECK(run.parsed);
  CHECK(strcmp(run.outcome, "pass") == 0);
  CHECK(strcmp(run.detail, "positive-match") == 0);
  CHECK(strcmp(run.stop_kind, "authored-completion") == 0);
  CHECK(run.log_bytes == ROUNDTRIP_BYTES);
  CHECK(strcmp(run.log_sha256, ROUNDTRIP_SHA256) == 0);
  CHECK(run.guest_tick != 0u);
  /* Every descriptor the runner must not accept: the existing prelaunch
   * refusal at guest tick 0. */
  CHECK(run_save_direct(SAVE_FD_LINKED, &run));
  expect_direct_refusal(&run, "refusal", "refused");
  CHECK(run_save_direct(SAVE_FD_WRITABLE, &run));
  expect_direct_refusal(&run, "refusal", "refused");
  CHECK(run_save_direct(SAVE_FD_OFFSET, &run));
  expect_direct_refusal(&run, "refusal", "refused");
  CHECK(run_save_direct(SAVE_FD_SHORT, &run));
  expect_direct_refusal(&run, "refusal", "refused");
  CHECK(run_save_direct(SAVE_FD_PIPE, &run));
  expect_direct_refusal(&run, "refusal", "refused");
  /* The grammar rule is repeated by the child: a descriptor without the kind
   * is a usage refusal, not a descriptor refusal. */
  CHECK(run_save_direct(SAVE_FD_NO_KIND, &run));
  expect_direct_refusal(&run, "usage", "not-started");
}

static int case_import_refused(void) {
  char import_dir[PATH_MAX];
  char export_dir[PATH_MAX];
  run_out run;

  /* The accepted baseline of the same setup. */
  fill_image(image_raw, SRAM_BYTES);
  CHECK(roundtrip_run("raw-v1", image_raw, SRAM_BYTES, "roundtrip.sra",
                      import_dir, export_dir, &run));
  expect_roundtrip_pass(&run);
  remove_dir_tree(export_dir);
  remove_dir_tree(import_dir);

  {
    static const struct {
      const char *label;
      refusal_class want;
      refusal_row row;
    } rows[] = {
        {"file of 32767 bytes", CLASS_USAGE,
         {ROUNDTRIP_ROM, "sram32k", "raw-v1", 32767u, NULL, false, false,
          false, false, false}},
        {"file of 32769 bytes", CLASS_USAGE,
         {ROUNDTRIP_ROM, "sram32k", "raw-v1", 32769u, NULL, false, false,
          false, false, false}},
        {"wrong sha256", CLASS_SUPERVISOR_REFUSAL,
         {ROUNDTRIP_ROM, "sram32k", "raw-v1", 32768u, NULL, true, false,
          false, false, false}},
        {"kind eeprom4k on the SRAM id", CLASS_RUNNER_REFUSAL,
         {ROUNDTRIP_ROM, "eeprom4k", "raw-v1", 512u, NULL, false, false,
          false, false, false}},
        {"kind sram32k on the 4 Kbit id", CLASS_RUNNER_REFUSAL,
         {ADMIT_E4K_ROM, "sram32k", "raw-v1", 32768u, NULL, false, false,
          false, false, false}},
        {"kind eeprom4k on the no-device id", CLASS_RUNNER_REFUSAL,
         {ADMIT_STEM_ROM, "eeprom4k", "raw-v1", 512u, NULL, false, false,
          false, false, false}},
        {"unknown format token", CLASS_USAGE,
         {ROUNDTRIP_ROM, "sram32k", "raw-v2", 32768u, NULL, false, false,
          false, false, false}},
        {"unversioned format token raw", CLASS_USAGE,
         {ROUNDTRIP_ROM, "sram32k", "raw", 32768u, NULL, false, false, false,
          false, false}},
        {"kind without format", CLASS_USAGE,
         {ROUNDTRIP_ROM, "sram32k", "raw-v1", 32768u, NULL, false, false,
          false, false, true}},
        {"format without kind", CLASS_USAGE,
         {ROUNDTRIP_ROM, "sram32k", "raw-v1", 32768u, NULL, false, false,
          false, true, false}},
        {"import trio missing its size", CLASS_USAGE,
         {ROUNDTRIP_ROM, "sram32k", "raw-v1", 32768u, NULL, false, true,
          false, false, false}},
        {"import trio missing its digest", CLASS_USAGE,
         {ROUNDTRIP_ROM, "sram32k", "raw-v1", 32768u, NULL, false, false,
          true, false, false}},
        {"declared size differs from the kind size", CLASS_USAGE,
         {ROUNDTRIP_ROM, "sram32k", "raw-v1", 32768u, "32767", false, false,
          false, false, false}},
    };
    size_t i;
    for (i = 0u; i < sizeof rows / sizeof rows[0]; i++) {
      run_refusal(&rows[i].row, rows[i].want, rows[i].label);
    }
  }
  direct_save_import_fd_rows();
  return g_failures == 0 ? 0 : 1;
}

/* ---- export-publication --------------------------------------------------------- */

static void run_publication_row(const char *label, const char *leaf,
                                const char *artifact_max, bool with_pair,
                                const char *directory_override,
                                const char *want_detail, refusal_class want) {
  char rom[PATH_MAX];
  char import_dir[PATH_MAX];
  char import_path[PATH_MAX];
  char export_dir[PATH_MAX];
  char sha[65];
  run_spec spec = {0};
  run_out run;

  fill_image(image_raw, SRAM_BYTES);
  CHECK(fixture_path(rom, ROUNDTRIP_ROM));
  CHECK(stage_import(image_raw, SRAM_BYTES, import_dir, import_path, sha));
  CHECK(make_private_dir(export_dir));
  spec.supervisor = g_supervisor;
  spec.fixture = rom;
  spec.bound = SAVE_BOUND;
  spec.import_path = import_path;
  spec.import_size = "32768";
  spec.import_sha256 = sha;
  spec.kind = "sram32k";
  spec.format = "raw-v1";
  spec.leaf = leaf;
  if (with_pair) {
    spec.artifact_dir = directory_override != NULL ? directory_override
                                                   : export_dir;
    spec.artifact_max = artifact_max;
  }
  CHECK(run_supervised(&spec, &run));
  (void)fprintf(stderr, "export-publication '%s': outcome=%s detail=%s "
                "stop=%s tick=%llu\n", label, run.outcome, run.detail,
                run.stop_kind, (unsigned long long)run.guest_tick);
  if (want == CLASS_USAGE) {
    expect_usage(&run);
  } else if (want == CLASS_RUNNER_REFUSAL) {
    expect_runner_refusal(&run);
  } else if (want == CLASS_SUPERVISOR_REFUSAL) {
    expect_supervisor_refusal(&run);
  } else {
    expect_host_error(&run, want_detail);
    CHECK(run.guest_tick == 0u);
  }
  CHECK(count_entries(export_dir) == 0);
  remove_dir_tree(export_dir);
  remove_dir_tree(import_dir);
}

static int case_export_publication(void) {
  char import_dir[PATH_MAX];
  char export_dir[PATH_MAX];
  char path[PATH_MAX];
  uint8_t original[64];
  uint8_t read_back[64];
  size_t length = 0u;
  run_out run;
  size_t i;

  /* The accepted baseline of this setup: the leaf is published once. */
  fill_image(image_raw, SRAM_BYTES);
  CHECK(roundtrip_run("raw-v1", image_raw, SRAM_BYTES, "roundtrip.sra",
                      import_dir, export_dir, &run));
  expect_roundtrip_pass(&run);
  check_leaf(export_dir, "roundtrip.sra", SRAM_BYTES,
             format_rows[0].sram_export);
  CHECK(count_entries(export_dir) == 1);
  remove_dir_tree(export_dir);
  remove_dir_tree(import_dir);

  /* An existing target: host-error stop, detail artifact-exists, the original
     file bytes unchanged and no stage directory left behind. */
  for (i = 0u; i < sizeof original; i++) original[i] = (uint8_t)(0x40u + i);
  {
    char rom[PATH_MAX];
    char import_path[PATH_MAX];
    char sha[65];
    run_spec spec = {0};

    CHECK(fixture_path(rom, ROUNDTRIP_ROM));
    CHECK(stage_import(image_raw, SRAM_BYTES, import_dir, import_path, sha));
    CHECK(make_private_dir(export_dir));
    CHECK(join_path(path, sizeof path, export_dir, "roundtrip.sra"));
    CHECK(write_new_file(path, original, sizeof original));
    spec.supervisor = g_supervisor;
    spec.fixture = rom;
    spec.bound = SAVE_BOUND;
    spec.import_path = import_path;
    spec.import_size = "32768";
    spec.import_sha256 = sha;
    spec.kind = "sram32k";
    spec.format = "raw-v1";
    spec.leaf = "roundtrip.sra";
    spec.artifact_dir = export_dir;
    spec.artifact_max = "32768";
    CHECK(run_supervised(&spec, &run));
    expect_host_error(&run, "artifact-exists");
    /* The publisher is opened before the guest runs, so the occupied target
       ends the run at guest tick 0 with no instruction retired. */
    CHECK(run.guest_tick == 0u);
    CHECK(read_file(path, read_back, sizeof read_back, &length));
    CHECK(length == sizeof original);
    CHECK(memcmp(read_back, original, sizeof original) == 0);
    CHECK(count_entries(export_dir) == 1);
    remove_dir_tree(export_dir);
    remove_dir_tree(import_dir);
  }

  /* An unsafe parent: a group-writable directory and a path through a
     symlink are refused before the guest starts and nothing is created. */
  {
    char link_path[PATH_MAX];
    char group_dir[PATH_MAX];
    char real_dir[PATH_MAX];

    CHECK(make_private_dir(group_dir));
    CHECK(chmod(group_dir, 0770) == 0);
    run_publication_row("group-writable directory", "roundtrip.sra", "32768",
                        true, group_dir, NULL, CLASS_RUNNER_REFUSAL);
    CHECK(count_entries(group_dir) == 0);
    remove_dir_tree(group_dir);
    CHECK(make_private_dir(real_dir));
    CHECK(snprintf(link_path, sizeof link_path, "%s.link", real_dir) <
          (int)sizeof link_path);
    CHECK(symlink(real_dir, link_path) == 0);
    run_publication_row("symlink component", "roundtrip.sra", "32768", true,
                        link_path, NULL, CLASS_RUNNER_REFUSAL);
    CHECK(count_entries(real_dir) == 0);
    (void)unlink(link_path);
    remove_dir_tree(real_dir);
  }

  /* Bad leaves: the extension rule is the usage class; the grammar belongs to
     the publisher and ends the run with its own error name before the guest
     starts. */
  run_publication_row("extension bin", "roundtrip.bin", "32768", true, NULL,
                      NULL, CLASS_USAGE);
  run_publication_row("uppercase stem", "Roundtrip.sra", "32768", true, NULL,
                      "artifact-leaf-name", CLASS_HOST_ERROR);
  run_publication_row("path separator", "a/b.sra", "32768", true, NULL,
                      "artifact-leaf-name", CLASS_HOST_ERROR);
  run_publication_row("37-character stem",
                      "abcdefghijklmnopqrstuvwxyz0123456789a.sra", "32768",
                      true, NULL, "artifact-leaf-name",
                      CLASS_HOST_ERROR);
  /* The artifact cap below the kind size and an export leaf without the
     artifact pair are usage. */
  run_publication_row("cap below the kind size", "roundtrip.sra", "32767", true,
                      NULL, NULL, CLASS_USAGE);
  run_publication_row("no artifact pair", "roundtrip.sra", NULL, false, NULL,
                      NULL, CLASS_USAGE);
  return g_failures == 0 ? 0 : 1;
}

/* ---- no-save-bytes-in-result ----------------------------------------------------- */

static bool file_contains(const char *path, const char *needle) {
  static uint8_t text[65536];
  size_t length = 0u;
  size_t needle_length = strlen(needle);
  size_t i;
  if (!read_file(path, text, sizeof text - 1u, &length)) return false;
  for (i = 0u; i + needle_length <= length; i++) {
    if (memcmp(text + i, needle, needle_length) == 0) return true;
  }
  return false;
}

/* The closed t64.result v2 key set (22 keys); no key was added for saves. */
static const char *const result_keys[22] = {
    "build_id", "counts", "detail", "evidence_kind", "evidence_tier",
    "fixture_id", "fixture_sha256", "guest_stop", "guest_tick", "input_digest",
    "instruction_count", "ipl3_payload_sha256", "ipl3_sha256", "log_sha256",
    "milestones", "outcome", "profile_digest", "profile_id", "schema",
    "schema_version", "source_revision", "state_digest"};

static int case_no_save_bytes_in_result(void) {
  char import_dir[PATH_MAX];
  char export_dir[PATH_MAX];
  char names[4][PATH_MAX];
  char needles[3][33];
  char capture_path[PATH_MAX];
  uint8_t bytes[8192];
  size_t length = 0u;
  t64_json_doc doc = {0};
  const char *bad_key = NULL;
  run_out run;
  size_t file;
  size_t needle;

  fill_image(image_raw, SRAM_BYTES);
  CHECK(roundtrip_run("raw-v1", image_raw, SRAM_BYTES, "roundtrip.sra",
                      import_dir, export_dir, &run));
  expect_roundtrip_pass(&run);
  /* The lowercase hex of the first 16 imported bytes, the W block and the V
     block. */
  hex_text(image_raw, BLOCK_BYTES, needles[0]);
  hex_text(w_block, BLOCK_BYTES, needles[1]);
  hex_text(v_block, BLOCK_BYTES, needles[2]);
  CHECK(strcmp(needles[0], "0b30557a9fc4e90e33587da2c7ec1136") == 0);
  CHECK(join_path(names[0], PATH_MAX, run.dir, "result.json"));
  CHECK(join_path(names[1], PATH_MAX, run.dir, "meta.json"));
  CHECK(join_path(names[2], PATH_MAX, run.dir, "supervisor.out"));
  CHECK(join_path(names[3], PATH_MAX, run.dir, "supervisor.err"));
  /* The guest transcript prints the strings (the scan strings are the right
     ones); the record, the meta and everything the supervisor printed do not. */
  CHECK(join_path(capture_path, sizeof capture_path, run.dir, "capture.bin"));
  for (needle = 0u; needle < 3u; needle++) {
    CHECK(file_contains(capture_path, needles[needle]));
    for (file = 0u; file < 4u; file++) {
      CHECK(!file_contains(names[file], needles[needle]));
    }
  }
  /* The record keeps its closed key set. */
  CHECK(read_file(names[0], bytes, sizeof bytes, &length));
  CHECK(t64_json_parse(bytes, length, NULL, &doc, NULL) == 0);
  if (doc.root != NULL) {
    CHECK(t64_json_check_keys(doc.root, result_keys, 22u, result_keys, 22u,
                              &bad_key) == T64_JSON_KEYS_OK);
    t64_json_free(&doc);
  }
  CHECK(run.checker_ok);
  remove_dir_tree(export_dir);
  remove_dir_tree(import_dir);
  return g_failures == 0 ? 0 : 1;
}

/* ---- argv-max-save ----------------------------------------------------------------- */

/* The supervisor test twin with every accepted option together (advance step,
 * video and audio observation descriptors, the artifact pair and the save
 * flags). The observation validators belong to the audio and VI fixtures, so
 * on a family id their descriptors end every run as video-observation-invalid:
 * that is the accepted baseline outcome of this setup (the same twin run with
 * the descriptors and the pair but no save flag). The full set must end with
 * the same outcome, with the imported image visible in the transcript digest
 * (the flags were parsed, forwarded and used) and nothing published (a run
 * that is not a pass publishes nothing). The same set plus a raw capture
 * directory is refused as usage. The compile-time slot sum is the build's own
 * _Static_assert in the supervisor. */
static int case_argv_max_save(void) {
  char rom[PATH_MAX];
  char import_dir[PATH_MAX];
  char import_path[PATH_MAX];
  char export_dir[PATH_MAX];
  char capture_dir[PATH_MAX];
  char sha[65];
  char baseline_sha[65];
  int video_pipe[2] = {-1, -1};
  int audio_pipe[2] = {-1, -1};
  run_spec spec = {0};
  run_out run;

  fill_image(image_raw, SRAM_BYTES);
  CHECK(fixture_path(rom, ROUNDTRIP_ROM));
  CHECK(stage_import(image_raw, SRAM_BYTES, import_dir, import_path, sha));
  CHECK(make_private_dir(export_dir));
  CHECK(make_private_dir(capture_dir));
  CHECK(pipe(video_pipe) == 0);
  CHECK(pipe(audio_pipe) == 0);
  spec.supervisor = g_test_supervisor;
  spec.fixture = rom;
  spec.bound = SAVE_BOUND;
  spec.advance_step = 16u;
  spec.video_fd = video_pipe[1];
  spec.audio_fd = audio_pipe[1];
  spec.artifact_dir = export_dir;
  spec.artifact_max = "32768";

  /* The accepted baseline of this setup: no save flag. */
  CHECK(run_supervised(&spec, &run));
  (void)fprintf(stderr, "argv-max-save baseline: outcome=%s detail=%s stop=%s\n",
                run.outcome, run.detail, run.stop_kind);
  CHECK(run.parsed);
  CHECK(run.checker_ok);
  CHECK(strcmp(run.outcome, "harness-error") == 0);
  CHECK(strcmp(run.stop_kind, "host-error") == 0);
  CHECK(strcmp(run.detail, "video-observation-invalid") == 0);
  CHECK(strcmp(run.log_sha256, ROUNDTRIP_SHA256) != 0);
  (void)snprintf(baseline_sha, sizeof baseline_sha, "%s", run.log_sha256);

  /* Every option together. */
  spec.import_path = import_path;
  spec.import_size = "32768";
  spec.import_sha256 = sha;
  spec.kind = "sram32k";
  spec.format = "raw-v1";
  spec.leaf = "roundtrip.sra";
  CHECK(run_supervised(&spec, &run));
  (void)fprintf(stderr, "argv-max-save: outcome=%s detail=%s stop=%s\n",
                run.outcome, run.detail, run.stop_kind);
  CHECK(run.parsed);
  CHECK(run.checker_ok);
  CHECK(strcmp(run.outcome, "harness-error") == 0);
  CHECK(strcmp(run.stop_kind, "host-error") == 0);
  CHECK(strcmp(run.detail, "video-observation-invalid") == 0);
  CHECK(strcmp(run.log_sha256, ROUNDTRIP_SHA256) == 0);
  CHECK(strcmp(run.log_sha256, baseline_sha) != 0);
  CHECK(count_entries(export_dir) == 0);

  /* The same set plus a raw capture directory is refused as usage. */
  spec.raw_capture_dir = capture_dir;
  CHECK(run_supervised(&spec, &run));
  expect_usage(&run);
  (void)close(video_pipe[0]);
  (void)close(video_pipe[1]);
  (void)close(audio_pipe[0]);
  (void)close(audio_pipe[1]);
  remove_dir_tree(export_dir);
  remove_dir_tree(capture_dir);
  remove_dir_tree(import_dir);
  return g_failures == 0 ? 0 : 1;
}

/* ---- upload-allowlist-save ------------------------------------------------------------ */

/* Runs argv in `directory` with T64_BUILD_DIR set, collecting stdout and
 * stderr into `out`; returns the exit status (or -1). */
static int run_capture(char *const argv[], const char *directory,
                       const char *build_dir, char *out, size_t capacity) {
  int fds[2];
  pid_t pid;
  size_t used = 0u;
  int status = 0;
  if (pipe(fds) != 0) return -1;
  pid = fork();
  if (pid < 0) return -1;
  if (pid == 0) {
    (void)close(fds[0]);
    if (dup2(fds[1], STDOUT_FILENO) < 0 || dup2(fds[1], STDERR_FILENO) < 0 ||
        chdir(directory) != 0 || setenv("T64_BUILD_DIR", build_dir, 1) != 0) {
      _exit(126);
    }
    execvp(argv[0], argv);
    _exit(127);
  }
  (void)close(fds[1]);
  for (;;) {
    ssize_t count = read(fds[0], out + used, capacity - 1u - used);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) break;
    used += (size_t)count;
    if (used >= capacity - 1u) break;
  }
  out[used] = '\0';
  (void)close(fds[0]);
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) return -1;
  }
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* The public member scan of scripts/qualify-phase09.py, run in isolated mode
 * (-I) with the qualifier loaded by path: exit 0 and ACCEPTED, or exit 3 and
 * the refusal message. */
static const char public_scan_program[] =
    "import importlib.util, sys\n"
    "from pathlib import Path\n"
    "spec = importlib.util.spec_from_file_location('q', sys.argv[1])\n"
    "mod = importlib.util.module_from_spec(spec)\n"
    "spec.loader.exec_module(mod)\n"
    "try:\n"
    "    mod.scan_public_members(Path(sys.argv[2]))\n"
    "except mod.Refusal as error:\n"
    "    print('REFUSED ' + str(error))\n"
    "    sys.exit(3)\n"
    "print('ACCEPTED')\n";

static char g_build_dir[PATH_MAX];

static int public_scan(const char *directory, char *out, size_t capacity) {
  char qualifier[PATH_MAX];
  char *argv[8];
  if (!join_path(qualifier, sizeof qualifier, g_root,
                 "scripts/qualify-phase09.py")) {
    return -1;
  }
  argv[0] = (char *)(uintptr_t)"python3";
  argv[1] = (char *)(uintptr_t)"-B";
  argv[2] = (char *)(uintptr_t)"-I";
  argv[3] = (char *)(uintptr_t)"-c";
  argv[4] = (char *)(uintptr_t)public_scan_program;
  argv[5] = qualifier;
  argv[6] = (char *)(uintptr_t)directory;
  argv[7] = NULL;
  return run_capture(argv, g_root, g_build_dir, out, capacity);
}

static int rights_staging_scan(const char *directory, char *out,
                               size_t capacity) {
  char script[PATH_MAX];
  char *argv[5];
  if (!join_path(script, sizeof script, g_root, "scripts/rights-scan.sh")) {
    return -1;
  }
  argv[0] = (char *)(uintptr_t)"sh";
  argv[1] = script;
  argv[2] = (char *)(uintptr_t)"--staging";
  argv[3] = (char *)(uintptr_t)directory;
  argv[4] = NULL;
  return run_capture(argv, g_root, g_build_dir, out, capacity);
}

/* A real exported leaf of an EEPROM admit ROM (the image imported RAW and
 * exported RAW) in `export_dir`. */
static bool eeprom_export(const eeprom_row *eeprom, char import_dir[PATH_MAX],
                          char export_dir[PATH_MAX], run_out *run) {
  char rom[PATH_MAX];
  char import_path[PATH_MAX];
  char sha[65];
  char size_text[24];
  run_spec spec = {0};
  fill_image(image_raw, eeprom->size);
  if (!fixture_path(rom, eeprom->rom) ||
      !stage_import(image_raw, eeprom->size, import_dir, import_path, sha) ||
      !make_private_dir(export_dir)) {
    return false;
  }
  (void)snprintf(size_text, sizeof size_text, "%zu", eeprom->size);
  spec.supervisor = g_supervisor;
  spec.fixture = rom;
  spec.bound = SAVE_BOUND;
  spec.import_path = import_path;
  spec.import_size = size_text;
  spec.import_sha256 = sha;
  spec.kind = eeprom->kind;
  spec.format = "raw-v1";
  spec.leaf = eeprom->leaf;
  spec.artifact_dir = export_dir;
  spec.artifact_max = size_text;
  return run_supervised(&spec, run);
}

/* Copies one exported leaf into a fresh candidate directory and runs both
 * scanners on it. */
static void check_leaf_refused(const char *export_dir, const char *leaf) {
  char candidate[PATH_MAX];
  char source[PATH_MAX];
  char target[PATH_MAX];
  char output[2048];
  size_t length = 0u;
  int status;
  CHECK(make_private_dir(candidate));
  CHECK(join_path(source, sizeof source, export_dir, leaf));
  CHECK(join_path(target, sizeof target, candidate, leaf));
  CHECK(read_file(source, published_bytes, sizeof published_bytes, &length));
  CHECK(length != 0u);
  CHECK(write_new_file(target, published_bytes, length));
  status = public_scan(candidate, output, sizeof output);
  (void)fprintf(stderr, "upload-allowlist-save '%s': public scan exit %d\n",
                leaf, status);
  CHECK(status == 3);
  CHECK(strstr(output, "REFUSED public upload contains a private-suffix member")
        != NULL);
  CHECK(strstr(output, leaf) != NULL);
  status = rights_staging_scan(candidate, output, sizeof output);
  CHECK(status == 1);
  {
    char expected[128];
    (void)snprintf(expected, sizeof expected, "RULE R-DENIED-EXT %s", leaf);
    CHECK(strstr(output, expected) != NULL);
  }
  remove_dir_tree(candidate);
}

static int case_upload_allowlist_save(void) {
  static const char *const runner_extensions[3] = {"eep", "sra", "sav"};
  static const char manifest_text[] =
      "{\"schema\":\"t64.phase09-upload\",\"entries\":[]}\n";
  char baseline[PATH_MAX];
  char manifest_path[PATH_MAX];
  char output[2048];
  char import_dir[PATH_MAX];
  char export_dir[PATH_MAX];
  run_out run;
  size_t i;

  /* The accepted baseline of this setup: a directory holding only an authored
     manifest.json is accepted by both scanners. */
  CHECK(make_private_dir(baseline));
  CHECK(join_path(manifest_path, sizeof manifest_path, baseline,
                  "manifest.json"));
  CHECK(write_new_file(manifest_path, (const uint8_t *)manifest_text,
                       sizeof manifest_text - 1u));
  CHECK(public_scan(baseline, output, sizeof output) == 0);
  CHECK(strstr(output, "ACCEPTED") != NULL);
  CHECK(rights_staging_scan(baseline, output, sizeof output) == 0);
  CHECK(strstr(output, "findings=0") != NULL);
  remove_dir_tree(baseline);

  /* Real exported leaves: the SRAM leaf in two of the allowed extensions and
     both EEPROM leaves are refused by the public scan (private suffix) and by
     the staging scan (R-DENIED-EXT). */
  fill_image(image_raw, SRAM_BYTES);
  CHECK(roundtrip_run("raw-v1", image_raw, SRAM_BYTES, "roundtrip.sra",
                      import_dir, export_dir, &run));
  expect_roundtrip_pass(&run);
  check_leaf_refused(export_dir, "roundtrip.sra");
  remove_dir_tree(export_dir);
  remove_dir_tree(import_dir);
  CHECK(roundtrip_run("raw-v1", image_raw, SRAM_BYTES, "slot.sav", import_dir,
                      export_dir, &run));
  expect_roundtrip_pass(&run);
  check_leaf_refused(export_dir, "slot.sav");
  remove_dir_tree(export_dir);
  remove_dir_tree(import_dir);
  for (i = 0u; i < 2u; i++) {
    CHECK(eeprom_export(&eeprom_rows[i], import_dir, export_dir, &run));
    expect_pass(&run);
    check_leaf_refused(export_dir, eeprom_rows[i].leaf);
    remove_dir_tree(export_dir);
    remove_dir_tree(import_dir);
  }

  /* The three extensions the runner allows for an export are a subset of the
     suffixes the public scan refuses: each is accepted by the flag rule and
     refused as a private suffix, and a non-allowed extension is not allowed by
     the flag rule. */
  for (i = 0u; i < 3u; i++) {
    char leaf[32];
    char candidate[PATH_MAX];
    char target[PATH_MAX];
    (void)snprintf(leaf, sizeof leaf, "probe.%s", runner_extensions[i]);
    CHECK(t64_save_flags_leaf_extension_ok(leaf));
    CHECK(make_private_dir(candidate));
    CHECK(join_path(target, sizeof target, candidate, leaf));
    CHECK(write_new_file(target, (const uint8_t *)"x", 1u));
    CHECK(public_scan(candidate, output, sizeof output) == 3);
    CHECK(strstr(output, "private-suffix member") != NULL);
    remove_dir_tree(candidate);
  }
  CHECK(!t64_save_flags_leaf_extension_ok("probe.bin"));
  CHECK(!t64_save_flags_leaf_extension_ok("probe.z64"));
  CHECK(!t64_save_flags_leaf_extension_ok("probe.json"));
  return g_failures == 0 ? 0 : 1;
}

/* ---- dispatch --------------------------------------------------------------- */

typedef struct {
  const char *name;
  int (*run)(void);
} case_entry;

static const case_entry cases[] = {
    {"sram-roundtrip", case_sram_roundtrip},
    {"format-matrix", case_format_matrix},
    {"eeprom-passthrough", case_eeprom_passthrough},
    {"same-setup-wrong-format", case_same_setup_wrong_format},
    {"import-refused", case_import_refused},
    {"export-publication", case_export_publication},
    {"no-save-bytes-in-result", case_no_save_bytes_in_result},
    {"argv-max-save", case_argv_max_save},
    {"upload-allowlist-save", case_upload_allowlist_save},
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
  {
    /* The build directory is two levels above the checker binary. */
    char checker_copy[PATH_MAX];
    char *slash;
    (void)snprintf(checker_copy, sizeof checker_copy, "%s", g_checker);
    slash = strrchr(checker_copy, '/');
    if (slash != NULL) *slash = '\0';
    slash = strrchr(checker_copy, '/');
    if (slash != NULL) *slash = '\0';
    (void)snprintf(g_build_dir, sizeof g_build_dir, "%s", checker_copy);
  }
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
