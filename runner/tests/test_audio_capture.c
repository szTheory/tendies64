#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>

#include "t64_audio_observation.h"
#include "t64_json.h"
#include "t64_observation_pipe.h"
#include "t64_raw_capture.h"
#include "t64_video_observation.h"

#define AUDIO_PROFILE_ID \
  "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1"
#define AUDIO_PROFILE_SHA256 \
  "eaa400aa23bfa3d0c9ba6d475d8eba7a60302151e22b6278ff6a4991d08bb5bf"
#define AUDIO_FIXTURE_ID "audio-pcm-literal"
#define AUDIO_FIXTURE_SHA256 \
  "cd9556444480b8b02b043207013e7ce5bf5697a1c040058bcde1348e55e80b99"
#define WRONG_FIXTURE_ID "audio-pcm-wrong-sample"
#define WRONG_FIXTURE_SHA256 \
  "046f663adee4cb41dff2925cd8a62a938495798c2d6b7541da528afd5e1eeb46"
#define INPUT_SHA256 \
  "6d5593bc2d95465b44ec7ce7785e5ff7d7d53605de9b039a021bc697d226186d"
#define RDP_FIXTURE_ID "rdp-vi-fill-diagnostic"
#define RDP_FIXTURE_SHA256 \
  "b469340e7caa0ac341a2a5b525aa7e3b39c8a47f295ecd10710ae695e46dfc3d"
#define RDP_PROFILE_ID "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v2"
#define RDP_PROFILE_SHA256 \
  "5612696247abb5dae1c2abb6c15d7fd6c5632136210be7b21a1085590af1c94e"

static const uint8_t expected_pcm[32] = {
    0x00u, 0x00u, 0x7fu, 0xffu, 0x80u, 0x00u, 0xffu, 0xffu,
    0x12u, 0x34u, 0xedu, 0xccu, 0x00u, 0x01u, 0xffu, 0xfeu,
    0x80u, 0x00u, 0x7fu, 0xffu, 0xffu, 0xffu, 0x00u, 0x00u,
    0x55u, 0xaau, 0xaau, 0x55u, 0x00u, 0x00u, 0x80u, 0x00u};

/* This test-side table must stay aligned with the result checker's closed
 * capture host-error vocabulary. Each value is exercised as harness-error. */
static const char *const raw_capture_host_error_details[] = {
    "capture-audio-incomplete", "capture-audio-metadata",
    "capture-audio-name", "capture-audio-sequence", "capture-cap",
    "capture-close", "capture-content-size", "capture-directory-flush",
    "capture-error", "capture-file-create", "capture-file-flush",
    "capture-file-length", "capture-file-open", "capture-identity",
    "capture-incomplete-streams", "capture-incomplete-terminal",
    "capture-invalid", "capture-manifest-accounting",
    "capture-manifest-create", "capture-manifest-length",
    "capture-manifest-memory", "capture-manifest-open",
    "capture-manifest-size", "capture-manifest-write", "capture-memory",
    "capture-no-replace-commit", "capture-no-replace-unsupported",
    "capture-open", "capture-probe-collision", "capture-probe-identity",
    "capture-probe-name", "capture-stage-collision", "capture-stage-create",
    "capture-stage-identity", "capture-stage-name", "capture-stage-open",
    "capture-stage-replaced", "capture-stage-subdirectory",
    "capture-target-exists", "capture-target-parent", "capture-target-stat",
    "capture-test-barrier-timeout", "capture-test-fault-unreached",
    "capture-video-build",
    "capture-video-incomplete", "capture-video-length",
    "capture-video-metadata", "capture-video-metadata-missing",
    "capture-video-metadata-shape", "capture-video-name",
    "capture-video-overflow", "capture-video-profile",
    "capture-video-sequence", "capture-write"};

static bool save_evidence(const char *directory, const char *name,
                          const uint8_t *bytes, size_t length);

typedef struct {
  const char *fixture_id;
  const char *fixture_sha256;
  bool wrong_sample;
} audio_run_case;

static bool read_file(const char *path, uint8_t *bytes, size_t capacity,
                      size_t *length_out) {
  int fd;
  size_t total = 0u;
  if (path == NULL || bytes == NULL || length_out == NULL) return false;
  fd = open(path, O_RDONLY);
  if (fd < 0) return false;
  while (total < capacity) {
    ssize_t count = read(fd, bytes + total, capacity - total);
    if (count < 0 && errno == EINTR) continue;
    if (count < 0) {
      (void)close(fd);
      return false;
    }
    if (count == 0) break;
    total += (size_t)count;
  }
  if (total == capacity) {
    uint8_t extra;
    ssize_t count;
    do {
      count = read(fd, &extra, 1u);
    } while (count < 0 && errno == EINTR);
    if (count != 0) {
      (void)close(fd);
      return false;
    }
  }
  if (close(fd) != 0) return false;
  *length_out = total;
  return true;
}

static bool json_string_is(const t64_json_node *node, const char *expected) {
  size_t length;
  if (node == NULL || node->type != T64_JSON_STRING || expected == NULL) {
    return false;
  }
  length = strlen(expected);
  return node->len == length && memcmp(node->str, expected, length) == 0;
}

static bool json_u64_is(const t64_json_node *node, uint64_t expected) {
  uint64_t actual = 0u;
  return t64_json_u64_string(node, &actual) != 0 && actual == expected;
}

static const t64_json_node *array_item(const t64_json_node *array,
                                       size_t index) {
  const t64_json_node *item;
  if (array == NULL || array->type != T64_JSON_ARRAY || index >= array->count) {
    return NULL;
  }
  item = array->first;
  while (index != 0u && item != NULL) {
    item = item->next;
    index--;
  }
  return item;
}

static bool file_equals_bytes(const char *path, const uint8_t *expected,
                              size_t expected_length) {
  uint8_t bytes[65536];
  size_t length = 0u;
  return expected_length <= sizeof bytes &&
         read_file(path, bytes, sizeof bytes, &length) &&
         length == expected_length &&
         memcmp(bytes, expected, expected_length) == 0;
}

static bool files_equal(const char *left_path, const char *right_path) {
  uint8_t left[65536];
  uint8_t right[65536];
  size_t left_length = 0u;
  size_t right_length = 0u;
  return read_file(left_path, left, sizeof left, &left_length) &&
         read_file(right_path, right, sizeof right, &right_length) &&
         left_length == right_length &&
         memcmp(left, right, left_length) == 0;
}

static bool read_json(const char *path, uint8_t *bytes, size_t capacity,
                      size_t *length_out, t64_json_doc *doc_out) {
  if (!read_file(path, bytes, capacity, length_out) || *length_out == 0u ||
      t64_json_parse(bytes, *length_out, NULL, doc_out, NULL) != 0) {
    return false;
  }
  return true;
}

static bool replace_report_value(const char *source_path,
                                 const char *destination_directory,
                                 const char *destination_name,
                                 const char *old_value,
                                 const char *new_value) {
  uint8_t source[8192];
  uint8_t output[8192];
  size_t source_length = 0u;
  size_t old_length;
  size_t new_length;
  char *match;
  size_t prefix_length;
  size_t suffix_length;
  if (!read_file(source_path, source, sizeof source - 1u, &source_length) ||
      source_length == 0u || old_value == NULL || new_value == NULL) {
    return false;
  }
  source[source_length] = 0u;
  old_length = strlen(old_value);
  new_length = strlen(new_value);
  match = strstr((char *)source, old_value);
  if (match == NULL || strstr(match + old_length, old_value) != NULL) {
    return false;
  }
  prefix_length = (size_t)(match - (char *)source);
  suffix_length = source_length - prefix_length - old_length;
  if (prefix_length > sizeof output || new_length > sizeof output - prefix_length ||
      suffix_length > sizeof output - prefix_length - new_length) {
    return false;
  }
  memcpy(output, source, prefix_length);
  memcpy(output + prefix_length, new_value, new_length);
  memcpy(output + prefix_length + new_length, match + old_length, suffix_length);
  return save_evidence(destination_directory, destination_name, output,
                       prefix_length + new_length + suffix_length);
}

static bool preserve_supervisor_producer_report(const char *directory,
                                                const char *meta_path) {
  DIR *entries = opendir(directory);
  struct dirent *entry;
  char report_path[1200] = {0};
  uint8_t bytes[8192];
  size_t length = 0u;
  bool found = false;
  if (entries == NULL) return false;
  while ((entry = readdir(entries)) != NULL) {
    if (strncmp(entry->d_name, "t64real.", 8u) == 0) {
      int written = snprintf(report_path, sizeof report_path, "%s/%s/report",
                             directory, entry->d_name);
      struct stat status;
      if (written > 0 && (size_t)written < sizeof report_path &&
          lstat(report_path, &status) == 0 && S_ISREG(status.st_mode)) {
        found = true;
        break;
      }
    }
  }
  (void)closedir(entries);
  if (!found ||
      !read_file(report_path, bytes, sizeof bytes, &length) || length == 0u ||
      !save_evidence(directory, "producer-report.json", bytes, length)) {
    return false;
  }
  if (!read_file(meta_path, bytes, sizeof bytes, &length) || length == 0u ||
      !save_evidence(directory, "supervisor-meta.original.json", bytes,
                     length)) {
    return false;
  }
  return true;
}

static bool result_semantics_match(const char *left_path,
                                  const char *right_path) {
  uint8_t left_bytes[8192];
  uint8_t right_bytes[8192];
  size_t left_length = 0u;
  size_t right_length = 0u;
  t64_json_doc left_doc = {0};
  t64_json_doc right_doc = {0};
  const char *fields[] = {"outcome", "detail", "guest_tick", "state_digest"};
  size_t i;
  bool matches = false;
  if (!read_json(left_path, left_bytes, sizeof left_bytes, &left_length,
                 &left_doc) ||
      !read_json(right_path, right_bytes, sizeof right_bytes, &right_length,
                 &right_doc)) {
    t64_json_free(&left_doc);
    t64_json_free(&right_doc);
    return false;
  }
  matches = true;
  for (i = 0u; i < sizeof fields / sizeof fields[0]; i++) {
    const t64_json_node *left = t64_json_get(left_doc.root, fields[i]);
    const t64_json_node *right = t64_json_get(right_doc.root, fields[i]);
    if (left == NULL || right == NULL || left->type != right->type ||
        (left->type == T64_JSON_STRING &&
         (left->len != right->len ||
          memcmp(left->str, right->str, left->len) != 0)) ||
        (left->type != T64_JSON_STRING && left->type != T64_JSON_NULL)) {
      matches = false;
      break;
    }
  }
  if (matches) {
    const t64_json_node *left_stop =
        t64_json_get(t64_json_get(left_doc.root, "guest_stop"), "kind");
    const t64_json_node *right_stop =
        t64_json_get(t64_json_get(right_doc.root, "guest_stop"), "kind");
    matches = left_stop != NULL && right_stop != NULL &&
              left_stop->type == T64_JSON_STRING &&
              right_stop->type == T64_JSON_STRING &&
              left_stop->len == right_stop->len &&
              memcmp(left_stop->str, right_stop->str, left_stop->len) == 0;
  }
  t64_json_free(&left_doc);
  t64_json_free(&right_doc);
  return matches;
}

static bool save_evidence(const char *directory, const char *name,
                         const uint8_t *bytes, size_t length) {
  char path[4096];
  size_t offset = 0u;
  int fd;
  int path_length = snprintf(path, sizeof path, "%s/%s", directory, name);
  if (path_length <= 0 || (size_t)path_length >= sizeof path ||
      (bytes == NULL && length != 0u)) {
    return false;
  }
  fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
  if (fd < 0) return false;
  while (offset < length) {
    ssize_t count = write(fd, bytes + offset, length - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) {
      (void)close(fd);
      return false;
    }
    offset += (size_t)count;
  }
  return close(fd) == 0;
}

static bool save_command(const char *directory, char *const *args,
                         size_t count) {
  char text[4096];
  size_t offset = 0u;
  size_t i;
  for (i = 0u; i < count; i++) {
    size_t length = strlen(args[i]);
    if (length + 1u > sizeof text - offset) return false;
    memcpy(text + offset, args[i], length);
    offset += length;
    text[offset++] = '\n';
  }
  return save_evidence(directory, "command.args", (const uint8_t *)text,
                       offset);
}

static bool checker_accepts(char *checker, const char *result_path) {
  pid_t child = fork();
  int status = 0;
  if (child == 0) {
    char mode[] = "result";
    char result_path_copy[512];
    char *const args[] = {checker, mode, result_path_copy, NULL};
    int length = snprintf(result_path_copy, sizeof result_path_copy, "%s",
                          result_path);
    if (length <= 0 || (size_t)length >= sizeof result_path_copy) _exit(126);
    int null_fd = open("/dev/null", O_WRONLY);
    if (null_fd >= 0) {
      (void)dup2(null_fd, STDOUT_FILENO);
      (void)dup2(null_fd, STDERR_FILENO);
    }
    execv(checker, args);
    _exit(127);
  }
  if (child < 0) return false;
  while (waitpid(child, &status, 0) < 0) {
    if (errno == EINTR) continue;
    return false;
  }
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool result_is_pass(const char *result_path,
                           const audio_run_case *test,
                           char state_digest_out[65]) {
  uint8_t bytes[8192];
  size_t length = 0u;
  t64_json_doc doc = {0};
  const t64_json_node *outcome;
  const t64_json_node *profile;
  const t64_json_node *profile_digest;
  const t64_json_node *fixture;
  const t64_json_node *fixture_sha;
  const t64_json_node *state_digest;
  bool pass = false;
  if (!read_file(result_path, bytes, sizeof bytes, &length) || length == 0u ||
      t64_json_parse(bytes, length, NULL, &doc, NULL) != 0) {
    return false;
  }
  outcome = t64_json_get(doc.root, "outcome");
  profile = t64_json_get(doc.root, "profile_id");
  profile_digest = t64_json_get(doc.root, "profile_digest");
  fixture = t64_json_get(doc.root, "fixture_id");
  fixture_sha = t64_json_get(doc.root, "fixture_sha256");
  state_digest = t64_json_get(doc.root, "state_digest");
  pass = outcome != NULL && outcome->type == T64_JSON_STRING &&
         outcome->len == 4u && memcmp(outcome->str, "pass", 4u) == 0 &&
         profile != NULL && profile->type == T64_JSON_STRING &&
         profile->len == strlen(AUDIO_PROFILE_ID) &&
         memcmp(profile->str, AUDIO_PROFILE_ID, profile->len) == 0 &&
         profile_digest != NULL && profile_digest->type == T64_JSON_STRING &&
         profile_digest->len == 64u &&
         memcmp(profile_digest->str, AUDIO_PROFILE_SHA256, 64u) == 0 &&
         fixture != NULL && fixture->type == T64_JSON_STRING &&
         fixture->len == strlen(test->fixture_id) &&
         memcmp(fixture->str, test->fixture_id, fixture->len) == 0 &&
         fixture_sha != NULL && fixture_sha->type == T64_JSON_STRING &&
         fixture_sha->len == 64u &&
         memcmp(fixture_sha->str, test->fixture_sha256, 64u) == 0 &&
         state_digest != NULL && state_digest->type == T64_JSON_STRING &&
         state_digest->len == 64u;
  if (pass && state_digest_out != NULL) {
    (void)memcpy(state_digest_out, state_digest->str, 64u);
    state_digest_out[64] = '\0';
  }
  t64_json_free(&doc);
  return pass;
}

static bool result_is_capture_error(const char *result_path,
                                    const char *expected_detail,
                                    uint64_t minimum_guest_tick,
                                    char state_digest_out[65],
                                    uint64_t *guest_tick_out) {
  uint8_t bytes[8192];
  size_t length = 0u;
  t64_json_doc doc = {0};
  const t64_json_node *outcome;
  const t64_json_node *detail;
  const t64_json_node *stop;
  const t64_json_node *stop_kind;
  const t64_json_node *guest_tick;
  const t64_json_node *state_digest;
  uint64_t tick = 0u;
  bool valid = false;
  if (expected_detail == NULL ||
      !read_json(result_path, bytes, sizeof bytes, &length, &doc)) {
    return false;
  }
  outcome = t64_json_get(doc.root, "outcome");
  detail = t64_json_get(doc.root, "detail");
  stop = t64_json_get(doc.root, "guest_stop");
  stop_kind = t64_json_get(stop, "kind");
  guest_tick = t64_json_get(doc.root, "guest_tick");
  state_digest = t64_json_get(doc.root, "state_digest");
  valid = json_string_is(outcome, "harness-error") &&
          json_string_is(detail, expected_detail) && guest_tick != NULL &&
          t64_json_u64_string(guest_tick, &tick) != 0 &&
          tick >= minimum_guest_tick;
  if (strcmp(expected_detail, "usage") == 0) {
    valid = valid && json_string_is(stop_kind, "not-started") && tick == 0u &&
            state_digest != NULL && state_digest->type == T64_JSON_NULL;
  } else if (strcmp(expected_detail, "runner-signalled") == 0) {
    valid = valid && json_string_is(stop_kind, "host-error") && tick == 0u &&
            state_digest != NULL && state_digest->type == T64_JSON_NULL;
  } else {
    valid = valid && json_string_is(stop_kind, "host-error") &&
            state_digest != NULL && state_digest->type == T64_JSON_STRING &&
            state_digest->len == 64u;
    if (valid && state_digest_out != NULL) {
      memcpy(state_digest_out, state_digest->str, 64u);
      state_digest_out[64] = '\0';
    }
  }
  if (valid && guest_tick_out != NULL) *guest_tick_out = tick;
  t64_json_free(&doc);
  return valid;
}

static bool result_is_guest_bound(const char *result_path,
                                  const audio_run_case *test,
                                  uint64_t expected_tick,
                                  char state_digest_out[65],
                                  uint64_t *guest_tick_out) {
  uint8_t bytes[8192];
  size_t length = 0u;
  t64_json_doc doc = {0};
  const t64_json_node *stop;
  const t64_json_node *state_digest;
  const t64_json_node *guest_tick;
  const t64_json_node *profile_digest;
  const t64_json_node *fixture_sha;
  const t64_json_node *outcome;
  const t64_json_node *detail;
  uint64_t tick = 0u;
  bool valid = false;
  if (test == NULL ||
      !read_json(result_path, bytes, sizeof bytes, &length, &doc)) {
    return false;
  }
  stop = t64_json_get(t64_json_get(doc.root, "guest_stop"), "kind");
  state_digest = t64_json_get(doc.root, "state_digest");
  guest_tick = t64_json_get(doc.root, "guest_tick");
  profile_digest = t64_json_get(doc.root, "profile_digest");
  fixture_sha = t64_json_get(doc.root, "fixture_sha256");
  outcome = t64_json_get(doc.root, "outcome");
  detail = t64_json_get(doc.root, "detail");
  /* At tick 539440 the authored audio fixture has not completed its expected
   * A/V observations. The runner correctly refuses to call that a successful
   * observation and reports this exact harness error after computing the
   * canonical machine digest. Accept it only as an incomplete-bound state
   * witness; it is never a timeout/pass or accepted profile observation. */
  valid = ((json_string_is(outcome, "timeout") &&
            json_string_is(detail, "guest-bound") &&
            json_string_is(stop, "guest-bound")) ||
           (json_string_is(outcome, "harness-error") &&
            json_string_is(detail, "video-observation-invalid") &&
            json_string_is(stop, "host-error"))) &&
          guest_tick != NULL &&
          t64_json_u64_string(guest_tick, &tick) != 0 &&
          tick == expected_tick &&
          json_string_is(t64_json_get(doc.root, "profile_id"),
                         AUDIO_PROFILE_ID) &&
          json_string_is(profile_digest, AUDIO_PROFILE_SHA256) &&
          json_string_is(t64_json_get(doc.root, "fixture_id"),
                         test->fixture_id) &&
          json_string_is(fixture_sha, test->fixture_sha256) &&
          state_digest != NULL && state_digest->type == T64_JSON_STRING &&
          state_digest->len == 64u;
  if (valid) {
    for (size_t i = 0u; i < 64u; i++) {
      if (!((state_digest->str[i] >= '0' && state_digest->str[i] <= '9') ||
            (state_digest->str[i] >= 'a' && state_digest->str[i] <= 'f'))) {
        valid = false;
        break;
      }
    }
  }
  if (valid && state_digest_out != NULL) {
    memcpy(state_digest_out, state_digest->str, 64u);
    state_digest_out[64] = '\0';
  }
  if (valid && guest_tick_out != NULL) *guest_tick_out = tick;
  t64_json_free(&doc);
  return valid;
}

typedef struct {
  const char *raw_target_override;
  bool raw_cap_set;
  uint64_t raw_cap;
  const char *test_fault;
  const char *expected_error;
  uint64_t minimum_guest_tick;
  uint64_t *guest_tick_out;
  bool guest_bound_set;
  uint64_t guest_bound;
  bool expect_guest_bound;
  bool allow_commit_race;
  int *race_outcome_out;
} raw_run_options;

static bool run_supervised(char *supervisor, char *checker, char *fixture,
                           char *input, const audio_run_case *test,
                           t64_video_observation *video,
                           t64_audio_observation *audio,
                           uint64_t advance_step,
                           char raw_bundle_path_out[512],
                           char state_digest_out[65],
                           const raw_run_options *raw_options) {
  char directory[4096];
  char result_path[512] = {0};
  char meta_path[512] = {0};
  char capture_path[512] = {0};
  char raw_bundle_path[512] = {0};
  char work_dir[512] = {0};
  char video_fd_text[24];
  char audio_fd_text[24];
  char advance_step_text[24];
  char raw_cap_text[24];
  char test_fault_copy[96];
  int video_pipe[2] = {-1, -1};
  int audio_pipe[2] = {-1, -1};
  int status = 0;
  pid_t child = -1;
  int length;
  const char *evidence_root = getenv("T64_AUDIO_EVIDENCE_DIR");
  bool pipe_read_ok = false;
  bool producer_report_ok = false;
  bool result_ok = false;
  bool checker_ok = false;
  bool pass = false;
  bool directory_created = false;
  t64_test_observation_pipe pipes[2] = {
      {-1, (uint8_t *)video, sizeof *video, 0u, false, false},
      {-1, (uint8_t *)audio, sizeof *audio, 0u, false, false}};

  if (evidence_root == NULL || evidence_root[0] == '\0') {
    evidence_root = "/tmp";
  } else {
    struct stat evidence_stat;
    if (stat(evidence_root, &evidence_stat) != 0) {
      if (mkdir(evidence_root, 0700) != 0) goto cleanup;
    } else if (!S_ISDIR(evidence_stat.st_mode)) {
      goto cleanup;
    }
  }
  length = snprintf(directory, sizeof directory,
                    "%s/audio-capture.XXXXXX", evidence_root);
  if (length <= 0 || (size_t)length >= sizeof directory ||
      mkdtemp(directory) == NULL) {
    goto cleanup;
  }
  directory_created = true;
  if (pipe(video_pipe) != 0 || pipe(audio_pipe) != 0) goto cleanup;
  length = snprintf(result_path, sizeof result_path, "%s/result.json",
                    directory);
  if (length <= 0 || (size_t)length >= sizeof result_path) goto cleanup;
  length = snprintf(meta_path, sizeof meta_path, "%s/meta.json", directory);
  if (length <= 0 || (size_t)length >= sizeof meta_path) goto cleanup;
  length = snprintf(capture_path, sizeof capture_path, "%s/capture.bin",
                    directory);
  if (length <= 0 || (size_t)length >= sizeof capture_path) goto cleanup;
  length = snprintf(raw_bundle_path, sizeof raw_bundle_path,
                    "%s/raw-bundle", directory);
  if (length <= 0 || (size_t)length >= sizeof raw_bundle_path) goto cleanup;
  if (raw_options != NULL && raw_options->raw_target_override != NULL) {
    length = snprintf(raw_bundle_path, sizeof raw_bundle_path, "%s",
                      raw_options->raw_target_override);
    if (length <= 0 || (size_t)length >= sizeof raw_bundle_path) goto cleanup;
  }
  if (raw_bundle_path_out != NULL) {
    length = snprintf(raw_bundle_path_out, 512u, "%s", raw_bundle_path);
    if (length <= 0 || (size_t)length >= 512u) goto cleanup;
  }
  length = snprintf(work_dir, sizeof work_dir, "%s", directory);
  if (length <= 0 || (size_t)length >= sizeof work_dir) goto cleanup;
  (void)snprintf(video_fd_text, sizeof video_fd_text, "%d", video_pipe[1]);
  (void)snprintf(audio_fd_text, sizeof audio_fd_text, "%d", audio_pipe[1]);
  (void)snprintf(advance_step_text, sizeof advance_step_text, "%llu",
                 (unsigned long long)advance_step);

  child = fork();
  if (child == 0) {
    char real_option[] = "--real";
    char result_option[] = "--result";
    char meta_option[] = "--meta";
    char work_option[] = "--work-dir";
    char wall_option[] = "--wall-ms";
    char wall_limit[] = "30000";
    char bound_option[] = "--bound";
    char guest_bound[24];
    char log_option[] = "--log-limit";
    char log_limit[] = "67108864";
    char fixture_id_option[] = "--fixture-id";
    char fixture_option[] = "--fixture";
    char fixture_sha_option[] = "--fixture-sha256";
    char input_option[] = "--input";
    char input_size_option[] = "--input-size";
    char input_size[] = "54";
    char input_sha_option[] = "--input-sha256";
    char expect_pass_option[] = "--expect-pass";
    char expect_pass[] = "1";
    char capture_option[] = "--capture-out";
    char raw_capture_option[] = "--raw-capture-dir";
    char raw_cap_option[] = "--raw-capture-max-bytes";
    char test_fault_option[] = "--test-fault";
    char video_option[] = "--video-observation-fd";
    char audio_option[] = "--audio-observation-fd";
    char input_sha[] = INPUT_SHA256;
    char fixture_id[64];
    char fixture_sha256[65];
    char *args[48];
    char advance_step_option[] = "--advance-step";
    size_t count = 0u;
    const uint64_t selected_bound =
        raw_options != NULL && raw_options->guest_bound_set
            ? raw_options->guest_bound
            : UINT64_C(600000);
    int fixture_id_length = snprintf(fixture_id, sizeof fixture_id, "%s",
                                     test->fixture_id);
    int fixture_sha_length = snprintf(fixture_sha256, sizeof fixture_sha256,
                                      "%s", test->fixture_sha256);
    if (fixture_id_length <= 0 ||
        (size_t)fixture_id_length >= sizeof fixture_id ||
        fixture_sha_length != 64 ||
        (size_t)fixture_sha_length >= sizeof fixture_sha256 ||
        snprintf(guest_bound, sizeof guest_bound, "%llu",
                 (unsigned long long)selected_bound) <= 0) {
      _exit(126);
    }
    (void)close(video_pipe[0]);
    (void)close(audio_pipe[0]);
    args[count++] = supervisor;
    args[count++] = real_option;
    args[count++] = result_option;
    args[count++] = result_path;
    args[count++] = meta_option;
    args[count++] = meta_path;
    args[count++] = work_option;
    args[count++] = work_dir;
    args[count++] = wall_option;
    args[count++] = wall_limit;
    args[count++] = bound_option;
    args[count++] = guest_bound;
    args[count++] = log_option;
    args[count++] = log_limit;
    args[count++] = fixture_id_option;
    args[count++] = fixture_id;
    args[count++] = fixture_option;
    args[count++] = fixture;
    args[count++] = fixture_sha_option;
    args[count++] = fixture_sha256;
    args[count++] = input_option;
    args[count++] = input;
    args[count++] = input_size_option;
    args[count++] = input_size;
    args[count++] = input_sha_option;
    args[count++] = input_sha;
    args[count++] = expect_pass_option;
    args[count++] = expect_pass;
    args[count++] = capture_option;
    args[count++] = capture_path;
    if (raw_bundle_path_out != NULL) {
      args[count++] = raw_capture_option;
      args[count++] = raw_bundle_path;
      if (raw_options != NULL && raw_options->raw_cap_set) {
        (void)snprintf(raw_cap_text, sizeof raw_cap_text, "%llu",
                       (unsigned long long)raw_options->raw_cap);
        args[count++] = raw_cap_option;
        args[count++] = raw_cap_text;
      }
      if (raw_options != NULL && raw_options->test_fault != NULL) {
        int fault_length = snprintf(test_fault_copy, sizeof test_fault_copy,
                                    "%s", raw_options->test_fault);
        if (fault_length <= 0 ||
            (size_t)fault_length >= sizeof test_fault_copy) {
          _exit(126);
        }
        args[count++] = test_fault_option;
        args[count++] = test_fault_copy;
      }
    }
    args[count++] = video_option;
    args[count++] = video_fd_text;
    args[count++] = audio_option;
    args[count++] = audio_fd_text;
    if (advance_step != 0u) {
      args[count++] = advance_step_option;
      args[count++] = advance_step_text;
    }
    args[count] = NULL;
    {
      size_t i;
      size_t capture_targets = 0u;
      size_t capture_caps = 0u;
      size_t test_faults = 0u;
      bool capture_target_matches = true;
      bool capture_cap_matches = true;
      bool test_fault_matches = true;
      for (i = 0u; i < count; i++) {
        if (strcmp(args[i], "--raw-capture-dir") == 0) {
          capture_targets++;
          capture_target_matches = capture_target_matches && i + 1u < count &&
                                   strcmp(args[i + 1u], raw_bundle_path) == 0;
        } else if (strcmp(args[i], "--raw-capture-max-bytes") == 0) {
          capture_caps++;
          capture_cap_matches =
              capture_cap_matches && i + 1u < count && raw_options != NULL &&
              raw_options->raw_cap_set &&
              strcmp(args[i + 1u], raw_cap_text) == 0;
        } else if (strcmp(args[i], "--test-fault") == 0) {
          test_faults++;
          test_fault_matches =
              test_fault_matches && i + 1u < count && raw_options != NULL &&
              raw_options->test_fault != NULL &&
              strcmp(args[i + 1u], test_fault_copy) == 0;
        }
      }
      if (capture_targets != (raw_bundle_path_out != NULL ? 1u : 0u) ||
          capture_caps != (raw_bundle_path_out != NULL && raw_options != NULL &&
                                   raw_options->raw_cap_set
                               ? 1u
                               : 0u) ||
          test_faults != (raw_bundle_path_out != NULL && raw_options != NULL &&
                                  raw_options->test_fault != NULL
                              ? 1u
                              : 0u) ||
          !capture_target_matches || !capture_cap_matches ||
          !test_fault_matches) {
        (void)fprintf(stderr,
                      "raw capture child argv does not match one intended target/cap/fault\n");
        _exit(126);
      }
    }
    if (!save_command(directory, args, count)) _exit(126);
    execv(supervisor, args);
    _exit(127);
  }
  if (child < 0) goto cleanup;
  (void)close(video_pipe[1]);
  video_pipe[1] = -1;
  (void)close(audio_pipe[1]);
  audio_pipe[1] = -1;
  pipes[0].fd = video_pipe[0];
  pipes[1].fd = audio_pipe[0];
  if (raw_options != NULL && raw_options->expected_error != NULL &&
      (strcmp(raw_options->expected_error, "usage") == 0 ||
       strcmp(raw_options->expected_error, "runner-signalled") == 0)) {
    while (waitpid(child, &status, 0) < 0) {
      if (errno == EINTR) continue;
      goto cleanup;
    }
    pipe_read_ok = true;
  } else {
    pipe_read_ok = t64_test_observation_pipes_wait(child, pipes, 2u, &status);
  }
  child = -1;
  if (raw_bundle_path_out != NULL &&
      (raw_options == NULL || raw_options->expected_error == NULL ||
       (strcmp(raw_options->expected_error, "usage") != 0 &&
        strcmp(raw_options->expected_error, "runner-signalled") != 0))) {
    char producer_path[1024];
    length = snprintf(producer_path, sizeof producer_path,
                      "%s/producer-report.json", directory);
    producer_report_ok =
        length > 0 && (size_t)length < sizeof producer_path &&
        preserve_supervisor_producer_report(directory, meta_path) &&
        result_semantics_match(producer_path, result_path);
  } else if (raw_options != NULL && raw_options->expect_guest_bound) {
    char producer_path[1024];
    length = snprintf(producer_path, sizeof producer_path,
                      "%s/producer-report.json", directory);
    producer_report_ok =
        length > 0 && (size_t)length < sizeof producer_path &&
        preserve_supervisor_producer_report(directory, meta_path) &&
        result_semantics_match(producer_path, result_path);
  } else {
    producer_report_ok = true;
  }
  checker_ok = checker_accepts(checker, result_path);
  if (raw_options != NULL && raw_options->expected_error != NULL) {
    result_ok = result_is_capture_error(
        result_path, raw_options->expected_error,
        raw_options->minimum_guest_tick, state_digest_out,
        raw_options->guest_tick_out);
  } else if (raw_options != NULL && raw_options->allow_commit_race) {
    int race_outcome = 0;
    if (result_is_pass(result_path, test, state_digest_out)) {
      race_outcome = 1;
      result_ok = true;
    } else if (result_is_capture_error(
                   result_path, "capture-no-replace-commit",
                   UINT64_C(551001), state_digest_out, NULL)) {
      race_outcome = 2;
      result_ok = true;
    }
    if (raw_options->race_outcome_out != NULL) {
      *raw_options->race_outcome_out = race_outcome;
    }
  } else if (raw_options != NULL && raw_options->expect_guest_bound) {
    result_ok = result_is_guest_bound(
        result_path, test, raw_options->guest_bound, state_digest_out,
        raw_options->guest_tick_out);
  } else {
    result_ok = result_is_pass(result_path, test, state_digest_out);
  }
  if (!WIFEXITED(status) ||
      ((raw_options == NULL || raw_options->expected_error == NULL) &&
       (raw_options == NULL || !raw_options->allow_commit_race) &&
       WEXITSTATUS(status) != 0) ||
      !pipe_read_ok ||
      !producer_report_ok || !checker_ok || !result_ok) {
    uint8_t report[8192];
    size_t report_length = 0u;
    fprintf(stderr,
            "supervised raw capture setup failed: exit=%d pipe=%d producer=%d checker=%d result=%d\n",
            WIFEXITED(status) ? WEXITSTATUS(status) : -1, pipe_read_ok,
            producer_report_ok, checker_ok, result_ok);
    if (read_file(result_path, report, sizeof report, &report_length) &&
        report_length != 0u) {
      (void)fwrite(report, 1u, report_length, stderr);
      (void)fputc('\n', stderr);
    }
    goto cleanup;
  }
  pass = true;

cleanup:
  if (child > 0) {
    (void)kill(child, SIGTERM);
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
    }
  }
  if (video_pipe[0] >= 0) (void)close(video_pipe[0]);
  if (video_pipe[1] >= 0) (void)close(video_pipe[1]);
  if (audio_pipe[0] >= 0) (void)close(audio_pipe[0]);
  if (audio_pipe[1] >= 0) (void)close(audio_pipe[1]);
  if (directory_created) {
    char read_status[512];
    char original_meta_path[1024];
    uint8_t meta_bytes[4096];
    size_t meta_length = 0u;
    int read_status_length;
    length = snprintf(original_meta_path, sizeof original_meta_path,
                      "%s/supervisor-meta.original.json", directory);
    if (length > 0 && (size_t)length < sizeof original_meta_path &&
        access(original_meta_path, F_OK) != 0 &&
        read_file(meta_path, meta_bytes, sizeof meta_bytes, &meta_length) &&
        meta_length != 0u) {
      (void)save_evidence(directory, "supervisor-meta.original.json",
                          meta_bytes, meta_length);
    }
    (void)save_evidence(directory, "video-observation.bin",
                       (const uint8_t *)video, pipes[0].received);
    (void)save_evidence(directory, "audio-observation.bin",
                       (const uint8_t *)audio, pipes[1].received);
    read_status_length = snprintf(
        read_status, sizeof read_status,
        "wait_status=%d\npipe_read_ok=%d\nvideo_expected=%zu\nvideo_received=%zu\nvideo_eof=%d\nvideo_overflow=%d\naudio_expected=%zu\naudio_received=%zu\naudio_eof=%d\naudio_overflow=%d\nchecker_ok=%d\nresult_pass=%d\n",
        status, pipe_read_ok, pipes[0].expected, pipes[0].received,
        pipes[0].eof, pipes[0].overflow, pipes[1].expected,
        pipes[1].received, pipes[1].eof, pipes[1].overflow, checker_ok,
        result_ok);
    if (read_status_length > 0 &&
        (size_t)read_status_length < sizeof read_status) {
      (void)save_evidence(directory, "read-status.txt",
                          (const uint8_t *)read_status,
                          (size_t)read_status_length);
    }
    fprintf(stderr, "private audio runner evidence retained at %s\n",
            directory);
  }
  return pass;
}

static bool fixture_observations_match(const t64_video_observation *video,
                                       const t64_audio_observation *audio,
                                       bool wrong_sample) {
  size_t i;
  size_t write_index = 0u;
  const uint32_t sample_words[8] = {
      UINT32_C(0x00007fff), UINT32_C(0x8000ffff), UINT32_C(0x1234edcc),
      UINT32_C(0x0001fffe), UINT32_C(0x80007fff), UINT32_C(0xffff0000),
      UINT32_C(0x55aaaa55), UINT32_C(0x00008000)};
  const uint32_t ai_addresses[7] = {
      UINT32_C(0x04500008), UINT32_C(0x04500010), UINT32_C(0x04500014),
      UINT32_C(0x04500000), UINT32_C(0x04500004), UINT32_C(0x04500000),
      UINT32_C(0x04500004)};
  const uint32_t ai_values[7] = {UINT32_C(1), UINT32_C(1103), UINT32_C(15),
                                 UINT32_C(0x3400), UINT32_C(16),
                                 UINT32_C(0x3410), UINT32_C(16)};

  if (video->magic != T64_VIDEO_OBSERVATION_MAGIC ||
      video->version != T64_VIDEO_OBSERVATION_VERSION || video->width != 12u ||
      video->height != 7u || video->stride_bytes != 48u ||
      video->source_address != UINT32_C(0x3000) ||
      video->source_bytes != 168u || video->packet_bytes != 336u ||
      video->packets_drained < 2u ||
      video->source_format != T64_VIDEO_SOURCE_RGBA5551 ||
      video->output_encoding != T64_VIDEO_ENCODING_RGBA8 ||
      strcmp(video->profile_id, AUDIO_PROFILE_ID) != 0 ||
      video->end_tick - video->start_tick != UINT64_C(411) ||
      video->mi_pending_before_clear != UINT32_C(0x24) ||
      video->mi_mask_before_clear != UINT32_C(0x20) ||
      video->mi_pending_after_clear != UINT32_C(0x04) ||
      video->commands_completed != UINT64_C(6) ||
      video->fullsync_completed != UINT64_C(1)) {
    return false;
  }
  for (i = 0u; i < video->source_bytes; i += 4u) {
    if (video->source[i] != 0xf8u || video->source[i + 1u] != 0x01u ||
        video->source[i + 2u] != 0x07u || video->source[i + 3u] != 0xc1u) {
      return false;
    }
  }
  for (i = 0u; i < video->packet_bytes; i += 8u) {
    const uint8_t first[4] = {255u, 0u, 0u, 255u};
    const uint8_t second[4] = {0u, 255u, 0u, 255u};
    if (memcmp(video->packet + i, first, sizeof first) != 0 ||
        memcmp(video->packet + i + 4u, second, sizeof second) != 0) {
      return false;
    }
  }
  if (audio->magic != T64_AUDIO_OBSERVATION_MAGIC ||
      audio->version != T64_AUDIO_OBSERVATION_VERSION ||
      audio->packet_count != T64_AUDIO_OBSERVATION_PACKET_COUNT ||
      audio->trace_overflow != 0u || audio->bus_write_count != 16u ||
      audio->mi_ai_read_seen == 0u || audio->guest_mi_snapshot_seen == 0u ||
      (audio->guest_mi_snapshot & UINT32_C(0x04)) == 0u) {
    return false;
  }
  for (i = 0u; i < T64_AUDIO_OBSERVATION_PACKET_COUNT; i++) {
    const t64_audio_packet_observation *packet = &audio->packets[i];
    if (packet->info.struct_size != (uint32_t)sizeof packet->info ||
        packet->info.sequence != (uint64_t)i + UINT64_C(1) ||
        packet->info.first_sample_index != (uint64_t)i * UINT64_C(4) ||
        packet->info.frame_count != 4u || packet->info.byte_length != 16u ||
        packet->info.rate_numerator != UINT64_C(48681812) ||
        packet->info.rate_denominator != UINT64_C(1104) ||
        packet->info.channels != 2u || packet->info.bits_per_sample != 16u ||
        packet->info.encoding != T64_AUDIO_ENCODING_S16BE ||
        strcmp(packet->info.profile_id, AUDIO_PROFILE_ID) != 0 ||
        packet->info.build_id[0] == '\0' ||
        strcmp(packet->info.build_id, audio->packets[0].info.build_id) != 0) {
      return false;
    }
    if (wrong_sample && i == 0u) {
      if (packet->bytes[0] != 0x00u || packet->bytes[1] != 0x01u ||
          memcmp(packet->bytes + 2u, expected_pcm + 2u, 14u) != 0) {
        return false;
      }
    } else if (memcmp(packet->bytes, expected_pcm + i * 16u, 16u) != 0) {
      return false;
    }
    if (packet->info.first_tick > packet->info.last_tick ||
        (i != 0u && packet->info.first_tick <=
                        audio->packets[i - 1u].info.last_tick)) {
      return false;
    }
  }
  if (audio->mi_ai_read_tick < audio->packets[1].info.last_tick) return false;
  for (i = 0u; i < 8u; i++) {
    const t64_audio_bus_write *write = &audio->bus_writes[write_index++];
    uint32_t expected_word = sample_words[i];
    if (wrong_sample && i == 0u) expected_word = UINT32_C(0x00017fff);
    if (write->kind != T64_TRACE_KIND_BUS_WRITE32 ||
        write->address != UINT32_C(0x3400) + (uint32_t)i * UINT32_C(4) ||
        write->value != expected_word) {
      return false;
    }
  }
  for (i = 0u; i < 7u; i++) {
    const t64_audio_bus_write *write = &audio->bus_writes[write_index++];
    if (write->kind != T64_TRACE_KIND_BUS_WRITE32 ||
        write->address != ai_addresses[i] || write->value != ai_values[i]) {
      return false;
    }
  }
  {
    const t64_audio_bus_write *write = &audio->bus_writes[write_index];
    if (write->kind != T64_TRACE_KIND_BUS_WRITE32 ||
        write->address != UINT32_C(0x3210) ||
        (write->value & UINT32_C(0x04)) == 0u) {
      return false;
    }
  }
  {
    size_t pcm_sw = 0u;
    bool ai_store_pcs[7] = {false, false, false, false,
                            false, false, false};
    const uint32_t ai_pcs[7] = {UINT32_C(0x8000047c), UINT32_C(0x80000484),
                                UINT32_C(0x8000048c), UINT32_C(0x80000494),
                                UINT32_C(0x8000049c), UINT32_C(0x800004a4),
                                UINT32_C(0x800004ac)};
    const uint32_t ai_opcodes[7] = {
        UINT32_C(0xad090008), UINT32_C(0xad090010), UINT32_C(0xad090014),
        UINT32_C(0xad090000), UINT32_C(0xad090004), UINT32_C(0xad090000),
        UINT32_C(0xad090004)};
    for (i = 0u; i < audio->sw_retirement_count; i++) {
      const t64_audio_sw_retirement *retirement = &audio->sw_retirements[i];
      if (retirement->pc == UINT32_C(0x8000045c) &&
          retirement->opcode == UINT32_C(0xad2b0000)) {
        pcm_sw++;
      }
      for (size_t j = 0u; j < 7u; j++) {
        if (retirement->pc == ai_pcs[j] &&
            retirement->opcode == ai_opcodes[j]) {
          ai_store_pcs[j] = true;
        }
      }
    }
    if (pcm_sw != 8u) return false;
    for (i = 0u; i < 7u; i++) {
      if (!ai_store_pcs[i]) return false;
    }
  }
  return true;
}

static int run_audio_case(const char *name, char *supervisor, char *checker,
                         char *fixture, char *input) {
  const audio_run_case test = strcmp(name, "runner-literal-pcm") == 0
                                  ? (audio_run_case){AUDIO_FIXTURE_ID,
                                                     AUDIO_FIXTURE_SHA256,
                                                     false}
                                  : (audio_run_case){WRONG_FIXTURE_ID,
                                                     WRONG_FIXTURE_SHA256,
                                                     true};
  t64_video_observation video = {0};
  t64_audio_observation audio = {0};
  if (!run_supervised(supervisor, checker, fixture, input, &test, &video,
                      &audio, 0u, NULL, NULL, NULL) ||
      !fixture_observations_match(&video, &audio, test.wrong_sample)) {
    fprintf(stderr, "actual CPU-to-AI-to-raw capture validation failed for %s\n",
            name);
    return 1;
  }
  if (test.wrong_sample) {
    printf("reached wrong-sample comparator after accepted setup; first packet differs by the authored left-sample bit\n");
  } else {
    printf("actual fetched CPU emitted two exact signed stereo packets across the DMA boundary through raw audio/video APIs\n");
  }
  return 0;
}

static int run_capture_case(char *supervisor, char *checker, char *fixture,
                            char *input, const raw_run_options *raw_options,
                            uint64_t *total_bytes_out) {
  const audio_run_case test = {AUDIO_FIXTURE_ID, AUDIO_FIXTURE_SHA256, false};
  t64_video_observation video = {0};
  t64_audio_observation audio = {0};
  char raw_bundle_path[512] = {0};
  struct stat bundle_stat;
  char manifest_path[1024];
  int length;

  if (!run_supervised(supervisor, checker, fixture, input, &test, &video,
                      &audio, 0u, raw_bundle_path, NULL, raw_options)) {
    fprintf(stderr,
            "actual --raw-capture-dir runner invocation did not produce a completed A/V bundle\n");
    return 1;
  }
  length = snprintf(manifest_path, sizeof manifest_path,
                    "%s/manifest.json", raw_bundle_path);
  if (length <= 0 || (size_t)length >= sizeof manifest_path ||
      lstat(raw_bundle_path, &bundle_stat) != 0 ||
      !S_ISDIR(bundle_stat.st_mode)) {
    fprintf(stderr, "actual --raw-capture-dir target is not a bundle directory\n");
    return 1;
  }
  if ((bundle_stat.st_mode & 0777) != 0700) {
    fprintf(stderr, "published raw bundle is not private mode 0700\n");
    return 1;
  }
  {
    struct stat manifest_stat;
    if (lstat(manifest_path, &manifest_stat) != 0 ||
        !S_ISREG(manifest_stat.st_mode) || manifest_stat.st_size <= 0 ||
        (manifest_stat.st_mode & 0777) != 0600) {
      fprintf(stderr, "actual --raw-capture-dir bundle has no manifest\n");
      return 1;
    }
  }
  if (!fixture_observations_match(&video, &audio, false)) {
    fprintf(stderr, "capture run diverged from actual producer observations\n");
    return 1;
  }
  {
    uint8_t manifest_bytes[1024u * 1024u];
    size_t manifest_length = 0u;
    t64_json_doc manifest_doc = {0};
    const t64_json_node *video_entries;
    const t64_json_node *audio_entries;
    uint64_t data_bytes = 0u;
    uint64_t data_bytes_declared = 0u;
    uint64_t manifest_bytes_declared = 0u;
    uint64_t last_video_tick = 0u;
    size_t i;
    bool valid = read_json(manifest_path, manifest_bytes,
                           sizeof manifest_bytes, &manifest_length,
                           &manifest_doc);
    if (valid) {
      const t64_json_node *root = manifest_doc.root;
      const t64_json_node *core_build_id = t64_json_get(root, "core_build_id");
      const t64_json_node *runner_build_id =
          t64_json_get(root, "runner_build_id");
      const t64_json_node *source_revision =
          t64_json_get(root, "source_revision");
      valid = json_string_is(t64_json_get(root, "schema"),
                             "t64.raw-av-capture") &&
              t64_json_get(root, "schema_version") != NULL &&
              t64_json_get(root, "schema_version")->type == T64_JSON_INT &&
              t64_json_get(root, "schema_version")->num == 1 &&
              json_string_is(t64_json_get(root, "profile_id"),
                             AUDIO_PROFILE_ID) &&
              json_string_is(t64_json_get(root, "profile_sha256"),
                             AUDIO_PROFILE_SHA256) &&
              json_u64_is(t64_json_get(root, "descriptor_version"), 11u) &&
              json_u64_is(t64_json_get(root, "machine_model_version"), 19u) &&
              json_u64_is(t64_json_get(root, "state_serialization_version"),
                          24u) &&
              core_build_id != NULL && core_build_id->type == T64_JSON_STRING &&
              strcmp(core_build_id->str,
                     audio.packets[0].info.build_id) == 0 &&
              runner_build_id != NULL &&
              runner_build_id->type == T64_JSON_STRING &&
              runner_build_id->len == strlen("t64-runner-v1-") + 64u &&
              memcmp(runner_build_id->str, "t64-runner-v1-",
                     strlen("t64-runner-v1-")) == 0 &&
              source_revision != NULL &&
              source_revision->type == T64_JSON_STRING &&
              source_revision->len == 40u &&
              json_string_is(t64_json_get(root, "content_sha256"),
                             AUDIO_FIXTURE_SHA256) &&
              json_u64_is(t64_json_get(root, "content_size"), 4728u) &&
              json_string_is(t64_json_get(root, "fixture_id"),
                             AUDIO_FIXTURE_ID) &&
              json_string_is(t64_json_get(root, "fixture_sha256"),
                             AUDIO_FIXTURE_SHA256) &&
              json_u64_is(t64_json_get(root, "fixture_size"), 4728u) &&
              json_string_is(t64_json_get(root, "input_file_sha256"),
                             INPUT_SHA256) &&
              json_string_is(t64_json_get(root, "input_digest"),
                             INPUT_SHA256) &&
              json_u64_is(t64_json_get(root, "input_size"), 54u) &&
              json_u64_is(t64_json_get(root, "terminal_guest_tick"),
                          audio.guest_stop_tick) &&
              json_string_is(t64_json_get(root, "terminal_outcome"), "pass") &&
              json_string_is(t64_json_get(root, "terminal_stop_kind"),
                             "authored-completion") &&
              json_u64_is(t64_json_get(root, "video_count"),
                          video.packets_drained) &&
              json_u64_is(t64_json_get(root, "audio_count"),
                          audio.packet_count) &&
              json_u64_is(t64_json_get(root, "manifest_bytes"),
                          (uint64_t)manifest_length);
      video_entries = t64_json_get(root, "video");
      audio_entries = t64_json_get(root, "audio");
      valid = valid && video_entries != NULL &&
              video_entries->type == T64_JSON_ARRAY &&
              video_entries->count == video.packets_drained &&
              video_entries->count >= 2u && audio_entries != NULL &&
              audio_entries->type == T64_JSON_ARRAY &&
              audio_entries->count == T64_AUDIO_OBSERVATION_PACKET_COUNT;
    } else {
      video_entries = NULL;
      audio_entries = NULL;
    }
    for (i = 0u; valid && i < video_entries->count; i++) {
      const t64_json_node *entry = array_item(video_entries, i);
      const t64_json_node *filename = t64_json_get(entry, "file");
      const t64_json_node *width = t64_json_get(entry, "width");
      const t64_json_node *height = t64_json_get(entry, "height");
      const t64_json_node *stride = t64_json_get(entry, "stride_bytes");
      uint64_t byte_length = 0u;
      uint64_t width_value = 0u;
      uint64_t height_value = 0u;
      uint64_t stride_value = 0u;
      uint64_t first_tick = 0u;
      uint64_t last_tick = 0u;
      char expected_name[48];
      char frame_path[1024];
      struct stat frame_stat;
      int name_length = snprintf(expected_name, sizeof expected_name,
                                 "frame-%06zu.rgba8", i);
      int path_length = snprintf(frame_path, sizeof frame_path,
                                 "%s/video/%s", raw_bundle_path,
                                 expected_name);
      valid = name_length > 0 && (size_t)name_length < sizeof expected_name &&
              path_length > 0 && (size_t)path_length < sizeof frame_path &&
              json_string_is(filename, expected_name) &&
              json_u64_is(t64_json_get(entry, "sequence"), (uint64_t)i) &&
              t64_json_u64_string(t64_json_get(entry, "first_tick"),
                                  &first_tick) != 0 &&
              t64_json_u64_string(t64_json_get(entry, "last_tick"),
                                  &last_tick) != 0 &&
              first_tick <= last_tick &&
              (i == 0u || first_tick >= last_video_tick) &&
              json_u64_is(width, video.width) &&
              json_u64_is(height, video.height) &&
              json_u64_is(stride, video.stride_bytes) &&
              json_string_is(t64_json_get(entry, "encoding"), "rgba8") &&
              t64_json_u64_string(t64_json_get(entry, "byte_length"),
                                  &byte_length) != 0 &&
              t64_json_u64_string(width, &width_value) != 0 &&
              t64_json_u64_string(height, &height_value) != 0 &&
              t64_json_u64_string(stride, &stride_value) != 0 &&
              width_value != 0u && height_value != 0u &&
              stride_value >= width_value * 4u &&
              byte_length == stride_value * height_value &&
              lstat(frame_path, &frame_stat) == 0 &&
              S_ISREG(frame_stat.st_mode) && frame_stat.st_size >= 0 &&
              (uint64_t)frame_stat.st_size == byte_length &&
              (frame_stat.st_mode & 0777) == 0600 &&
              byte_length != 0u && data_bytes <= UINT64_MAX - byte_length;
      if (valid) {
        data_bytes += byte_length;
        last_video_tick = last_tick;
        if (i == 0u) {
          valid = byte_length == video.packet_bytes &&
                  json_u64_is(t64_json_get(entry, "sequence"),
                              video.sequence) &&
                  json_u64_is(t64_json_get(entry, "first_tick"),
                              video.start_tick) &&
                  json_u64_is(t64_json_get(entry, "last_tick"),
                              video.end_tick) &&
                  file_equals_bytes(frame_path, video.packet,
                                   video.packet_bytes);
        }
      }
    }
    for (i = 0u; valid && i < audio_entries->count; i++) {
      const t64_audio_packet_observation *observed = &audio.packets[i];
      const t64_audio_packet_info *info = &observed->info;
      const t64_json_node *entry = array_item(audio_entries, i);
      const t64_json_node *filename = t64_json_get(entry, "file");
      uint64_t byte_length = 0u;
      char expected_name[48];
      char packet_path[1024];
      struct stat packet_stat;
      int name_length = snprintf(expected_name, sizeof expected_name,
                                 "packet-%06zu.s16be", i);
      int path_length = snprintf(packet_path, sizeof packet_path,
                                 "%s/audio/%s", raw_bundle_path,
                                 expected_name);
      valid = name_length > 0 && (size_t)name_length < sizeof expected_name &&
              path_length > 0 && (size_t)path_length < sizeof packet_path &&
              json_string_is(filename, expected_name) &&
              json_u64_is(t64_json_get(entry, "sequence"), info->sequence) &&
              json_u64_is(t64_json_get(entry, "first_sample_index"),
                          info->first_sample_index) &&
              json_u64_is(t64_json_get(entry, "frame_count"),
                          info->frame_count) &&
              json_u64_is(t64_json_get(entry, "rate_numerator"),
                          info->rate_numerator) &&
              json_u64_is(t64_json_get(entry, "rate_denominator"),
                          info->rate_denominator) &&
              json_u64_is(t64_json_get(entry, "first_tick"), info->first_tick) &&
              json_u64_is(t64_json_get(entry, "last_tick"), info->last_tick) &&
              json_string_is(t64_json_get(entry, "encoding"),
                             "s16be-interleaved-stereo") &&
              json_u64_is(t64_json_get(entry, "channels"), info->channels) &&
              json_u64_is(t64_json_get(entry, "bits_per_sample"),
                          info->bits_per_sample) &&
              t64_json_u64_string(t64_json_get(entry, "byte_length"),
                                  &byte_length) != 0 &&
              byte_length == info->byte_length && byte_length == 16u &&
              lstat(packet_path, &packet_stat) == 0 &&
              S_ISREG(packet_stat.st_mode) && packet_stat.st_size >= 0 &&
              (uint64_t)packet_stat.st_size == byte_length &&
              (packet_stat.st_mode & 0777) == 0600 &&
              data_bytes <= UINT64_MAX - byte_length &&
              file_equals_bytes(packet_path, observed->bytes,
                                (size_t)byte_length);
      if (valid) data_bytes += byte_length;
    }
    if (valid) {
      uint64_t total_bytes = 0u;
      valid =
              t64_json_u64_string(t64_json_get(manifest_doc.root,
                                                "data_bytes"),
                                  &data_bytes_declared) != 0 &&
              t64_json_u64_string(t64_json_get(manifest_doc.root,
                                                "manifest_bytes"),
                                  &manifest_bytes_declared) != 0 &&
              t64_json_u64_string(t64_json_get(manifest_doc.root,
                                                "total_bytes"),
                                  &total_bytes) != 0 &&
              data_bytes_declared == data_bytes &&
              manifest_bytes_declared == manifest_length &&
              total_bytes == data_bytes_declared + manifest_bytes_declared &&
              total_bytes <= T64_RAW_CAPTURE_DEFAULT_MAX_BYTES;
      if (valid && total_bytes_out != NULL) *total_bytes_out = total_bytes;
    }
    t64_json_free(&manifest_doc);
    if (!valid) {
      fprintf(stderr,
              "bundle manifest, identities, counts, metadata, or actual file bytes failed validation\n");
      return 1;
    }
  }
  puts("actual supervised runner published a complete raw A/V bundle");
  return 0;
}

static int run_geometry_case(char *supervisor, char *checker, char *fixture,
                             char *input) {
  char directory[4096];
  char result_path[512];
  char meta_path[512];
  char capture_path[512];
  char raw_bundle_path[512];
  char video_fd_text[24];
  char *args[40];
  int video_pipe[2] = {-1, -1};
  int status = 0;
  pid_t child = -1;
  int length;
  const char *evidence_root = getenv("T64_AUDIO_EVIDENCE_DIR");
  bool directory_created = false;
  bool pipe_read_ok = false;
  t64_video_observation video = {0};

  if (evidence_root == NULL || evidence_root[0] == '\0') {
    evidence_root = "/tmp";
  }
  length = snprintf(directory, sizeof directory,
                    "%s/audio-capture-geometry.XXXXXX", evidence_root);
  if (length <= 0 || (size_t)length >= sizeof directory ||
      mkdtemp(directory) == NULL) {
    return 1;
  }
  directory_created = true;
  if (pipe(video_pipe) != 0) goto cleanup;
#define PATH_IN_DIRECTORY(buffer, filename)                                  \
  do {                                                                        \
    length = snprintf((buffer), sizeof(buffer), "%s/%s", directory,          \
                      (filename));                                             \
    if (length <= 0 || (size_t)length >= sizeof(buffer)) goto cleanup;         \
  } while (0)
  PATH_IN_DIRECTORY(result_path, "result.json");
  PATH_IN_DIRECTORY(meta_path, "meta.json");
  PATH_IN_DIRECTORY(capture_path, "capture.bin");
  PATH_IN_DIRECTORY(raw_bundle_path, "raw-bundle");
#undef PATH_IN_DIRECTORY
  (void)snprintf(video_fd_text, sizeof video_fd_text, "%d", video_pipe[1]);
  {
    char real_option[] = "--real";
    char result_option[] = "--result";
    char meta_option[] = "--meta";
    char work_option[] = "--work-dir";
    char wall_option[] = "--wall-ms";
    char wall_limit[] = "30000";
    char bound_option[] = "--bound";
    char guest_bound[] = "600000";
    char log_option[] = "--log-limit";
    char log_limit[] = "67108864";
    char fixture_id_option[] = "--fixture-id";
    char fixture_id[] = RDP_FIXTURE_ID;
    char fixture_option[] = "--fixture";
    char fixture_sha_option[] = "--fixture-sha256";
    char fixture_sha[] = RDP_FIXTURE_SHA256;
    char input_option[] = "--input";
    char input_size_option[] = "--input-size";
    char input_size[] = "54";
    char input_sha_option[] = "--input-sha256";
    char input_sha[] = INPUT_SHA256;
    char expect_pass_option[] = "--expect-pass";
    char expect_pass[] = "1";
    char capture_option[] = "--capture-out";
    char test_fault_arg[] = "--test-fault";
    char preserve_stage_fault[] = "capture-retain-incomplete-stage";
    char video_option[] = "--video-observation-fd";
    char raw_capture_option[] = "--raw-capture-dir";
    size_t count = 0u;
    args[count++] = supervisor;
    args[count++] = real_option;
    args[count++] = result_option;
    args[count++] = result_path;
    args[count++] = meta_option;
    args[count++] = meta_path;
    args[count++] = work_option;
    args[count++] = directory;
    args[count++] = wall_option;
    args[count++] = wall_limit;
    args[count++] = bound_option;
    args[count++] = guest_bound;
    args[count++] = log_option;
    args[count++] = log_limit;
    args[count++] = fixture_id_option;
    args[count++] = fixture_id;
    args[count++] = fixture_option;
    args[count++] = fixture;
    args[count++] = fixture_sha_option;
    args[count++] = fixture_sha;
    args[count++] = input_option;
    args[count++] = input;
    args[count++] = input_size_option;
    args[count++] = input_size;
    args[count++] = input_sha_option;
    args[count++] = input_sha;
    args[count++] = expect_pass_option;
    args[count++] = expect_pass;
    args[count++] = capture_option;
    args[count++] = capture_path;
    args[count++] = test_fault_arg;
    args[count++] = preserve_stage_fault;
    args[count++] = video_option;
    args[count++] = video_fd_text;
    args[count++] = raw_capture_option;
    args[count++] = raw_bundle_path;
    args[count] = NULL;
    if (!save_command(directory, args, count)) goto cleanup;
    child = fork();
    if (child == 0) {
      (void)close(video_pipe[0]);
      execv(supervisor, args);
      _exit(127);
    }
  }
  if (child < 0) goto cleanup;
  (void)close(video_pipe[1]);
  video_pipe[1] = -1;
  {
    t64_test_observation_pipe pipe_record = {
        video_pipe[0], (uint8_t *)&video, sizeof video, 0u, false, false};
    pipe_read_ok = t64_test_observation_pipes_wait(child, &pipe_record, 1u,
                                                   &status);
    child = -1;
  }
  {
    uint8_t report[8192];
    size_t report_length = 0u;
    t64_json_doc report_doc = {0};
    const t64_json_node *guest_stop;
    const t64_json_node *state_digest;
    const t64_json_node *guest_tick;
    struct stat target_status;
    bool target_absent = lstat(raw_bundle_path, &target_status) != 0 &&
                         errno == ENOENT;
    bool report_valid = read_json(result_path, report, sizeof report,
                                  &report_length, &report_doc);
    if (report_valid) {
      guest_stop = t64_json_get(report_doc.root, "guest_stop");
      state_digest = t64_json_get(report_doc.root, "state_digest");
      guest_tick = t64_json_get(report_doc.root, "guest_tick");
      {
        uint64_t tick_value = 0u;
      report_valid = json_string_is(t64_json_get(report_doc.root, "outcome"),
                                    "harness-error") &&
                     json_string_is(t64_json_get(report_doc.root, "detail"),
                                    "capture-incomplete-streams") &&
                     json_string_is(t64_json_get(guest_stop, "kind"),
                                    "host-error") &&
                     state_digest != NULL &&
                     state_digest->type == T64_JSON_STRING &&
                     state_digest->len == 64u && guest_tick != NULL &&
                     t64_json_u64_string(guest_tick, &tick_value) != 0 &&
                     tick_value > 0u;
      }
    }
    if (pipe_read_ok) {
      (void)save_evidence(directory, "video-observation.bin",
                          (const uint8_t *)&video, sizeof video);
    }
    if (child > 0) {
      (void)kill(child, SIGTERM);
      while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
      }
      child = -1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0 || !pipe_read_ok ||
        !report_valid || !target_absent ||
        video.magic != T64_VIDEO_OBSERVATION_MAGIC ||
        video.width != 4u || video.height != 3u ||
        video.stride_bytes != 16u || video.packet_bytes != 48u ||
        video.packets_drained == 0u ||
        !checker_accepts(checker, result_path)) {
      t64_json_free(&report_doc);
      fprintf(stderr,
              "actual 4x3 VI capture ingestion did not refuse only the missing audio stream cleanly\n");
      goto cleanup;
    }
    if (!preserve_supervisor_producer_report(directory, meta_path)) {
      t64_json_free(&report_doc);
      fprintf(stderr, "could not retain producer report and original metadata\n");
      goto cleanup;
    }
    t64_json_free(&report_doc);
  }
  {
    char unknown_report[1024];
    char wrong_outcome_report[1024];
    size_t detail_index;
    for (detail_index = 0u;
         detail_index < sizeof raw_capture_host_error_details /
                            sizeof raw_capture_host_error_details[0];
         detail_index++) {
      char name[64];
      char known_report[1024];
      int name_length = snprintf(name, sizeof name,
                                 "raw-capture-known-%02zu.json", detail_index);
      int path_length = snprintf(known_report, sizeof known_report,
                                 "%s/%s", directory, name);
      if (name_length <= 0 || (size_t)name_length >= sizeof name ||
          path_length <= 0 || (size_t)path_length >= sizeof known_report ||
          !replace_report_value(result_path, directory, name,
                                "capture-incomplete-streams",
                                raw_capture_host_error_details[detail_index]) ||
          !checker_accepts(checker, known_report)) {
        fprintf(stderr,
                "result checker rejected known capture host-error detail %s\n",
                raw_capture_host_error_details[detail_index]);
        goto cleanup;
      }
    }
    puts("case reached: closed-capture-error-vocabulary");
    length = snprintf(unknown_report, sizeof unknown_report,
                      "%s/raw-capture-unknown-detail.json", directory);
    if (length <= 0 || (size_t)length >= sizeof unknown_report ||
        !replace_report_value(result_path, directory,
                              "raw-capture-unknown-detail.json",
                              "capture-incomplete-streams", "capture-unknown") ||
        checker_accepts(checker, unknown_report)) {
      fprintf(stderr, "result checker did not reject an unknown capture detail\n");
      goto cleanup;
    }
    length = snprintf(wrong_outcome_report, sizeof wrong_outcome_report,
                      "%s/raw-capture-wrong-outcome.json", directory);
    if (length <= 0 || (size_t)length >= sizeof wrong_outcome_report ||
        !replace_report_value(result_path, directory,
                              "raw-capture-wrong-outcome.json",
                              "harness-error", "pass") ||
        checker_accepts(checker, wrong_outcome_report)) {
      fprintf(stderr,
              "result checker accepted a raw-capture detail paired with the wrong outcome\n");
      goto cleanup;
    }
    puts("result checker accepted every listed capture host-error detail and rejected unknown/wrong-outcome mutations");
  }
  {
    DIR *entries = opendir(directory);
    struct dirent *entry;
    char stage_path[1024] = {0};
    bool found_stage = false;
    size_t stage_count = 0u;
    if (entries != NULL) {
      while ((entry = readdir(entries)) != NULL) {
        if (strncmp(entry->d_name, ".t64-probe-", 11u) == 0) {
          length = snprintf(stage_path, sizeof stage_path, "%s/%s", directory,
                            entry->d_name);
          if (length <= 0 || (size_t)length >= sizeof stage_path) {
            stage_count = 2u;
            break;
          }
          stage_count++;
          found_stage = true;
        }
      }
      (void)closedir(entries);
    }
    if (stage_count != 1u || !found_stage) {
      fprintf(stderr, "test-only pre-finalize stage was not retained exactly once\n");
      goto cleanup;
    }
    {
      char frame_path[1200];
      char manifest_path[1200];
      char audio_path[1200];
      struct stat stage_stat;
      struct stat frame_stat;
      struct stat audio_stat;
      int frame_length = snprintf(frame_path, sizeof frame_path,
                                  "%s/video/frame-000000.rgba8", stage_path);
      int manifest_length = snprintf(manifest_path, sizeof manifest_path,
                                     "%s/manifest.json", stage_path);
      int audio_length = snprintf(audio_path, sizeof audio_path,
                                  "%s/audio/packet-000000.s16be", stage_path);
      if (frame_length <= 0 ||
          (size_t)frame_length >= sizeof frame_path ||
          manifest_length <= 0 ||
          (size_t)manifest_length >= sizeof manifest_path ||
          audio_length <= 0 || (size_t)audio_length >= sizeof audio_path ||
          lstat(stage_path, &stage_stat) != 0 ||
          !S_ISDIR(stage_stat.st_mode) ||
          (stage_stat.st_mode & 0777) != 0700 ||
          lstat(frame_path, &frame_stat) != 0 ||
          !S_ISREG(frame_stat.st_mode) || frame_stat.st_size != 48 ||
          (frame_stat.st_mode & 0777) != 0600 ||
          !file_equals_bytes(frame_path, video.packet, video.packet_bytes) ||
          lstat(manifest_path, &stage_stat) == 0 || errno != ENOENT ||
          lstat(audio_path, &audio_stat) == 0 || errno != ENOENT) {
        fprintf(stderr,
                "retained stage did not contain the exact actual 4x3 stride-16 VI packet before manifest finalization\n");
        goto cleanup;
      }
    }
  }
  printf("general raw video consumer wrote and retained exact actual %ux%u stride-%u API packet bytes before rejecting the absent audio stream\n",
         video.width, video.height, video.stride_bytes);
  (void)directory_created;
  return 0;

cleanup:
  if (child > 0) {
    (void)kill(child, SIGTERM);
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
    }
  }
  if (video_pipe[0] >= 0) (void)close(video_pipe[0]);
  if (video_pipe[1] >= 0) (void)close(video_pipe[1]);
  (void)directory_created;
  return 1;
}

static int run_capture_resume_case(char *supervisor, char *checker,
                                   char *fixture, char *input) {
  const audio_run_case test = {AUDIO_FIXTURE_ID, AUDIO_FIXTURE_SHA256, false};
  t64_video_observation video[2] = {{0}};
  t64_audio_observation audio[2] = {{0}};
  const uint64_t advances[2] = {UINT64_C(16), UINT64_C(32)};
  char bundle[2][512] = {{0}};
  char digest[2][65] = {{0}};
  char path_a[1024];
  char path_b[1024];
  size_t run;
  size_t i;
  int length;

  for (run = 0u; run < 2u; run++) {
    if (!run_supervised(supervisor, checker, fixture, input, &test,
                        &video[run], &audio[run], advances[run], bundle[run],
                        digest[run], NULL) ||
        !fixture_observations_match(&video[run], &audio[run], false)) {
      fprintf(stderr, "capture drain/resume failed at advance step %llu\n",
              (unsigned long long)advances[run]);
      return 1;
    }
  }
  length = snprintf(path_a, sizeof path_a, "%s/manifest.json", bundle[0]);
  if (length <= 0 || (size_t)length >= sizeof path_a) return 1;
  length = snprintf(path_b, sizeof path_b, "%s/manifest.json", bundle[1]);
  if (length <= 0 || (size_t)length >= sizeof path_b ||
      !files_equal(path_a, path_b) || strcmp(digest[0], digest[1]) != 0 ||
      audio[0].guest_stop_tick == 0u ||
      audio[0].guest_stop_tick != audio[1].guest_stop_tick ||
      video[0].packets_drained != video[1].packets_drained ||
      audio[0].packet_count != audio[1].packet_count) {
    fprintf(stderr, "capture drain schedules changed canonical manifest or terminal guest state\n");
    return 1;
  }
  for (i = 0u; i < video[0].packets_drained; i++) {
    char name[48];
    length = snprintf(name, sizeof name, "frame-%06zu.rgba8", i);
    if (length <= 0 || (size_t)length >= sizeof name) return 1;
    length = snprintf(path_a, sizeof path_a, "%s/video/%s", bundle[0], name);
    if (length <= 0 || (size_t)length >= sizeof path_a) return 1;
    length = snprintf(path_b, sizeof path_b, "%s/video/%s", bundle[1], name);
    if (length <= 0 || (size_t)length >= sizeof path_b ||
        !files_equal(path_a, path_b)) {
      fprintf(stderr, "capture video packet %zu changed with drain schedule\n", i);
      return 1;
    }
  }
  for (i = 0u; i < audio[0].packet_count; i++) {
    char name[48];
    length = snprintf(name, sizeof name, "packet-%06zu.s16be", i);
    if (length <= 0 || (size_t)length >= sizeof name) return 1;
    length = snprintf(path_a, sizeof path_a, "%s/audio/%s", bundle[0], name);
    if (length <= 0 || (size_t)length >= sizeof path_a) return 1;
    length = snprintf(path_b, sizeof path_b, "%s/audio/%s", bundle[1], name);
    if (length <= 0 || (size_t)length >= sizeof path_b ||
        !files_equal(path_a, path_b)) {
      fprintf(stderr, "capture audio packet %zu changed with drain schedule\n", i);
      return 1;
    }
  }
  puts("capture drain/resume steps 16 and 32 produced byte-identical manifests and packets");
  return 0;
}

static int run_capture_suite(char *supervisor, char *test_supervisor,
                             char *checker, char *fixture, char *input,
                             char *geometry_fixture) {
  if (run_capture_case(supervisor, checker, fixture, input, NULL, NULL) != 0) return 1;
  puts("case reached: actual-av-bundle");
  if (run_geometry_case(test_supervisor, checker, geometry_fixture, input) != 0)
    return 1;
  puts("case reached: general-frame-geometry");
  if (run_capture_case(supervisor, checker, fixture, input, NULL, NULL) != 0) return 1;
  puts("case reached: literal-pcm-files");
  if (run_capture_case(supervisor, checker, fixture, input, NULL, NULL) != 0) return 1;
  puts("case reached: manifest-exact-identities");
  if (run_capture_resume_case(test_supervisor, checker, fixture, input) != 0)
    return 1;
  puts("case reached: capture-drain-resume");
  return 0;
}

static int run_capture_short_write_case(char *supervisor, char *checker,
                                        char *fixture, char *input) {
  const audio_run_case test = {AUDIO_FIXTURE_ID, AUDIO_FIXTURE_SHA256, false};
  const raw_run_options options = {.test_fault = "capture-short-eintr"};
  t64_video_observation video = {0};
  t64_audio_observation audio = {0};
  char bundle[512] = {0};
  char path[1024];
  char digest[65] = {0};
  int length;

  if (!run_supervised(supervisor, checker, fixture, input, &test, &video,
                      &audio, 0u, bundle, digest, &options) ||
      !fixture_observations_match(&video, &audio, false)) {
    fprintf(stderr,
            "short-write/EINTR test control failed to produce the accepted actual A/V run\n");
    return 1;
  }
  length = snprintf(path, sizeof path, "%s/audio/packet-000000.s16be", bundle);
  if (length <= 0 || (size_t)length >= sizeof path ||
      !file_equals_bytes(path, expected_pcm, 16u)) {
    fprintf(stderr, "short-write/EINTR changed actual PCM file bytes\n");
    return 1;
  }
  length = snprintf(path, sizeof path, "%s/manifest.json", bundle);
  if (length <= 0 || (size_t)length >= sizeof path || access(path, R_OK) != 0) {
    fprintf(stderr, "short-write/EINTR did not publish its complete manifest\n");
    return 1;
  }
  puts("test-only writer completed a captured EINTR and short write without changing actual packet bytes");
  return 0;
}

static bool target_error_run(char *supervisor, char *checker, char *fixture,
                             char *input, const char *target,
                             const char *expected_detail) {
  const audio_run_case test = {AUDIO_FIXTURE_ID, AUDIO_FIXTURE_SHA256, false};
  const raw_run_options options = {
      .raw_target_override = target,
      .expected_error = expected_detail};
  t64_video_observation video = {0};
  t64_audio_observation audio = {0};
  char bundle[512] = {0};
  char digest[65] = {0};
  return run_supervised(supervisor, checker, fixture, input, &test, &video,
                        &audio, 0u, bundle, digest, &options);
}

static int run_parent_replacement_race(char *supervisor, char *checker,
                                       char *fixture, char *input);

static int run_existing_target_case(char *supervisor, char *checker,
                                    char *fixture, char *input) {
  const char *evidence_root = getenv("T64_AUDIO_EVIDENCE_DIR");
  char directory[512];
  char file_path[1024];
  char directory_path[1024];
  char nested_path[1024];
  char symlink_path[1024];
  char pattern[1024];
  char symlink_value[1024];
  const uint8_t sentinel[] = {0x53u, 0x45u, 0x4eu, 0x54u, 0x49u, 0x4eu, 0x45u, 0x4cu};
  struct stat file_before;
  struct stat file_after;
  struct stat directory_before;
  struct stat directory_after;
  struct stat nested_before;
  struct stat nested_after;
  struct stat link_before;
  struct stat link_after;
  ssize_t link_length;
  int fd;
  int length;

  if (evidence_root == NULL || evidence_root[0] == '\0') evidence_root = "/tmp";
  length = snprintf(pattern, sizeof pattern, "%s/capture-targets.XXXXXX",
                    evidence_root);
  if (length <= 0 || (size_t)length >= sizeof pattern ||
      mkdtemp(pattern) == NULL) {
    fprintf(stderr, "could not create private existing-target evidence directory\n");
    return 1;
  }
  (void)snprintf(directory, sizeof directory, "%s", pattern);
  length = snprintf(file_path, sizeof file_path, "%s/existing-file", directory);
  if (length <= 0 || (size_t)length >= sizeof file_path) return 1;
  length = snprintf(directory_path, sizeof directory_path, "%s/existing-dir", directory);
  if (length <= 0 || (size_t)length >= sizeof directory_path) return 1;
  length = snprintf(nested_path, sizeof nested_path, "%s/sentinel", directory_path);
  if (length <= 0 || (size_t)length >= sizeof nested_path) return 1;
  length = snprintf(symlink_path, sizeof symlink_path, "%s/existing-link", directory);
  if (length <= 0 || (size_t)length >= sizeof symlink_path) return 1;
  fd = open(file_path, O_WRONLY | O_CREAT | O_EXCL, 0640);
  if (fd < 0 || write(fd, sentinel, sizeof sentinel) != (ssize_t)sizeof sentinel ||
      fstat(fd, &file_before) != 0 || close(fd) != 0 ||
      mkdir(directory_path, 0700) != 0) {
    if (fd >= 0) (void)close(fd);
    fprintf(stderr, "could not prepare existing file/directory targets\n");
    return 1;
  }
  fd = open(nested_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (fd < 0 || write(fd, sentinel, sizeof sentinel) != (ssize_t)sizeof sentinel ||
      fstat(fd, &nested_before) != 0 || close(fd) != 0 ||
      lstat(directory_path, &directory_before) != 0 ||
      symlink("existing-file", symlink_path) != 0 ||
      lstat(symlink_path, &link_before) != 0) {
    if (fd >= 0) (void)close(fd);
    fprintf(stderr, "could not prepare existing directory/link targets\n");
    return 1;
  }
  if (!target_error_run(supervisor, checker, fixture, input, file_path,
                        "capture-target-exists") ||
      !target_error_run(supervisor, checker, fixture, input, directory_path,
                        "capture-target-exists") ||
      !target_error_run(supervisor, checker, fixture, input, symlink_path,
                        "capture-target-exists") ||
      lstat(file_path, &file_after) != 0 ||
      lstat(directory_path, &directory_after) != 0 ||
      lstat(nested_path, &nested_after) != 0 ||
      lstat(symlink_path, &link_after) != 0 ||
      file_before.st_dev != file_after.st_dev ||
      file_before.st_ino != file_after.st_ino ||
      file_before.st_mode != file_after.st_mode ||
      directory_before.st_dev != directory_after.st_dev ||
      directory_before.st_ino != directory_after.st_ino ||
      nested_before.st_dev != nested_after.st_dev ||
      nested_before.st_ino != nested_after.st_ino ||
      link_before.st_dev != link_after.st_dev ||
      link_before.st_ino != link_after.st_ino ||
      !file_equals_bytes(file_path, sentinel, sizeof sentinel) ||
      !file_equals_bytes(nested_path, sentinel, sizeof sentinel)) {
    fprintf(stderr,
            "existing file/directory/symlink target was not refused intact\n");
    return 1;
  }
  link_length = readlink(symlink_path, symlink_value, sizeof symlink_value - 1u);
  if (link_length != (ssize_t)strlen("existing-file") ||
      memcmp(symlink_value, "existing-file", (size_t)link_length) != 0) {
    fprintf(stderr, "existing symlink target text changed\n");
    return 1;
  }
  puts("existing file, directory, and symlink targets were refused without changing their identities or contents");
  return 0;
}

static int run_unsafe_target_parent_case(char *supervisor, char *checker,
                                         char *fixture, char *input) {
  const char *evidence_root = getenv("T64_AUDIO_EVIDENCE_DIR");
  char root[512];
  char target[1024];
  char symlink_path[1024];
  char outside[1024];
  char sentinel_path[1200];
  const uint8_t sentinel[] = {0x50u, 0x41u, 0x52u, 0x45u, 0x4eu, 0x54u};
  struct stat sentinel_before;
  struct stat sentinel_after;
  struct stat status;
  int length;
  if (evidence_root == NULL || evidence_root[0] == '\0') evidence_root = "/tmp";
  length = snprintf(root, sizeof root, "%s", evidence_root);
  if (length <= 0 || (size_t)length >= sizeof root) return 1;
  length = snprintf(target, sizeof target, "%s/../raw-capture-unsafe-%ld",
                    root, (long)getpid());
  if (length <= 0 || (size_t)length >= sizeof target ||
      (lstat(target, &status) == 0 || errno != ENOENT)) {
    fprintf(stderr, "unsafe-parent sentinel target was unexpectedly present\n");
    return 1;
  }
  if (!target_error_run(supervisor, checker, fixture, input, target,
                        "capture-target-parent") ||
      lstat(target, &status) == 0 || errno != ENOENT) {
    fprintf(stderr, "unsafe parent target escaped rejection\n");
    return 1;
  }
  length = snprintf(outside, sizeof outside, "%s/unsafe-outside-%ld",
                    root, (long)getpid());
  if (length <= 0 || (size_t)length >= sizeof outside ||
      mkdir(outside, 0700) != 0) return 1;
  length = snprintf(symlink_path, sizeof symlink_path, "%s/unsafe-link-%ld",
                    root, (long)getpid());
  if (length <= 0 || (size_t)length >= sizeof symlink_path ||
      symlink(outside, symlink_path) != 0) return 1;
  length = snprintf(sentinel_path, sizeof sentinel_path, "%s/sentinel", outside);
  if (length <= 0 || (size_t)length >= sizeof sentinel_path) return 1;
  {
    int fd = open(sentinel_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0 || write(fd, sentinel, sizeof sentinel) != (ssize_t)sizeof sentinel ||
        fstat(fd, &sentinel_before) != 0 || close(fd) != 0) {
      if (fd >= 0) (void)close(fd);
      return 1;
    }
  }
  length = snprintf(target, sizeof target, "%s/final.bundle", symlink_path);
  if (length <= 0 || (size_t)length >= sizeof target ||
      !target_error_run(supervisor, checker, fixture, input, target,
                        "capture-target-parent") ||
      lstat(sentinel_path, &sentinel_after) != 0 ||
      sentinel_before.st_dev != sentinel_after.st_dev ||
      sentinel_before.st_ino != sentinel_after.st_ino ||
      sentinel_before.st_mode != sentinel_after.st_mode ||
      !file_equals_bytes(sentinel_path, sentinel, sizeof sentinel)) {
    fprintf(stderr, "intermediate symlink parent was followed or changed its outside sentinel\n");
    return 1;
  }
  length = snprintf(target, sizeof target, "%s/final.bundle", outside);
  if (length <= 0 || (size_t)length >= sizeof target ||
      (lstat(target, &status) == 0 || errno != ENOENT) ||
      run_parent_replacement_race(supervisor, checker, fixture, input) != 0) {
    fprintf(stderr, "unsafe parent race redirected output or changed the outside target\n");
    return 1;
  }
  puts("raw capture refused dot-dot and intermediate symlink parents, then finalized through the retained parent FD after its pathname was replaced with a symlink");
  return 0;
}

static int run_capture_target_refusals(char *supervisor, char *checker,
                                       char *fixture, char *input) {
  if (run_existing_target_case(supervisor, checker, fixture, input) != 0)
    return 1;
  puts("case reached: existing-targets-untouched");
  puts("case reached: existing-file-directory-untouched");
  if (run_unsafe_target_parent_case(supervisor, checker, fixture, input) != 0)
    return 1;
  puts("case reached: unsafe-target-parent");
  return 0;
}

static bool run_capture_fault(char *supervisor, char *checker, char *fixture,
                              char *input, const char *fault,
                              const char *expected_detail,
                              uint64_t minimum_guest_tick,
                              char state_digest_out[65],
                              uint64_t *guest_tick_out) {
  const audio_run_case test = {AUDIO_FIXTURE_ID, AUDIO_FIXTURE_SHA256, false};
  const raw_run_options options = {
      .test_fault = fault,
      .expected_error = expected_detail,
      .minimum_guest_tick = minimum_guest_tick,
      .guest_tick_out = guest_tick_out};
  t64_video_observation video = {0};
  t64_audio_observation audio = {0};
  char bundle[512] = {0};
  char digest[65] = {0};
  struct stat status;
  if (!run_supervised(supervisor, checker, fixture, input, &test, &video,
                      &audio, 0u, bundle,
                      state_digest_out != NULL ? state_digest_out : digest,
                      &options)) {
    return false;
  }
  return lstat(bundle, &status) != 0 && errno == ENOENT;
}

static bool run_guest_bound_baseline(char *supervisor, char *checker,
                                    char *fixture, char *input,
                                    uint64_t guest_tick,
                                    char state_digest_out[65],
                                    uint64_t *guest_tick_out) {
  const audio_run_case test = {AUDIO_FIXTURE_ID, AUDIO_FIXTURE_SHA256, false};
  const raw_run_options options = {
      .guest_bound_set = true,
      .guest_bound = guest_tick,
      .guest_tick_out = guest_tick_out,
      .expect_guest_bound = true};
  t64_video_observation video = {0};
  t64_audio_observation audio = {0};
  return run_supervised(supervisor, checker, fixture, input, &test, &video,
                       &audio, 0u, NULL, state_digest_out, &options);
}

static int run_capture_io_errors(char *production_supervisor,
                                 char *test_supervisor, char *checker,
                                 char *fixture, char *input) {
  static const char *const faults[] = {
      "capture-fail-write", "capture-fail-close", "capture-fail-fsync",
      "capture-fail-rename"};
  static const char *const details[] = {
      "capture-write", "capture-close", "capture-file-flush",
      "capture-no-replace-commit"};
  char error_digests[4][65] = {{0}};
  char first_io_baseline_digest[65] = {0};
  char completed_baseline_digest[65] = {0};
  uint64_t first_io_baseline_tick = 0u;
  size_t i;
  for (i = 0u; i < sizeof faults / sizeof faults[0]; i++) {
    uint64_t tick = 0u;
    uint64_t minimum_tick = i == 3u ? UINT64_C(551001) : UINT64_C(1);
    if (!run_capture_fault(test_supervisor, checker, fixture, input,
                           faults[i], details[i], minimum_tick,
                           error_digests[i], &tick) ||
        (i < 3u && tick != UINT64_C(539440)) ||
        (i == 3u && tick != UINT64_C(551001))) {
      fprintf(stderr, "test-only %s did not report %s at actual guest progress\n",
              faults[i], details[i]);
      return 1;
    }
    printf("case reached: %s -> %s at guest tick %llu with producer/public report agreement\n",
           faults[i], details[i], (unsigned long long)tick);
    if (i == 0u) {
      if (!run_guest_bound_baseline(
              production_supervisor, checker, fixture, input, tick,
              first_io_baseline_digest, &first_io_baseline_tick) ||
          first_io_baseline_tick != tick) {
        fprintf(stderr,
                "independent no-capture baseline did not stop at first I/O error tick %llu\n",
                (unsigned long long)tick);
        return 1;
      }
    }
    if (i < 3u &&
        (tick != first_io_baseline_tick ||
         strcmp(error_digests[i], first_io_baseline_digest) != 0)) {
      fprintf(stderr,
              "%s changed canonical guest state versus the no-capture baseline at tick %llu\n",
              faults[i], (unsigned long long)tick);
      return 1;
    }
    if (i == 3u) {
      const audio_run_case test = {
          AUDIO_FIXTURE_ID, AUDIO_FIXTURE_SHA256, false};
      t64_video_observation video = {0};
      t64_audio_observation audio = {0};
      if (!run_supervised(production_supervisor, checker, fixture, input,
                          &test, &video, &audio, 0u, NULL,
                          completed_baseline_digest, NULL) ||
          strcmp(error_digests[i], completed_baseline_digest) != 0) {
        fprintf(stderr,
                "final rename failure changed completed canonical guest state\n");
        return 1;
      }
    }
  }
  printf("case reached: host-error-guest-state; write/close/fsync errors matched an independent no-capture digest at tick %llu and rename failure matched the completed no-capture digest at tick 551001\n",
         (unsigned long long)first_io_baseline_tick);
  {
    const audio_run_case test = {AUDIO_FIXTURE_ID, AUDIO_FIXTURE_SHA256, false};
    const raw_run_options options = {
        .test_fault = "capture-fail-write", .expected_error = "usage"};
    t64_video_observation video = {0};
    t64_audio_observation audio = {0};
    char bundle[512] = {0};
    char digest[65] = {0};
    if (!run_supervised(production_supervisor, checker, fixture, input, &test,
                        &video, &audio, 0u, bundle, digest, &options)) {
      fprintf(stderr,
              "production supervisor accepted a test-only raw-capture fault control\n");
      return 1;
    }
    puts("production supervisor rejected --test-fault before guest execution");
  }
  return 0;
}

static bool run_capture_cap_error(char *supervisor, char *checker,
                                  char *fixture, char *input, uint64_t cap,
                                  const char *expected_detail,
                                  uint64_t minimum_guest_tick,
                                  uint64_t *guest_tick_out) {
  const audio_run_case test = {AUDIO_FIXTURE_ID, AUDIO_FIXTURE_SHA256, false};
  const raw_run_options options = {
      .raw_cap_set = true,
      .raw_cap = cap,
      .expected_error = expected_detail,
      .minimum_guest_tick = minimum_guest_tick,
      .guest_tick_out = guest_tick_out};
  t64_video_observation video = {0};
  t64_audio_observation audio = {0};
  char bundle[512] = {0};
  char digest[65] = {0};
  struct stat status;
  return run_supervised(supervisor, checker, fixture, input, &test, &video,
                        &audio, 0u, bundle, digest, &options) &&
         lstat(bundle, &status) != 0 && errno == ENOENT;
}

static int run_capture_output_caps(char *supervisor, char *checker,
                                   char *fixture, char *input) {
  uint64_t baseline_total = 0u;
  uint64_t actual_total = 0u;
  uint64_t boundary_caps[3];
  size_t i;
  if (run_capture_case(supervisor, checker, fixture, input, NULL,
                       &baseline_total) != 0 ||
      baseline_total < 2u) {
    fprintf(stderr, "could not measure a complete accepted raw bundle baseline\n");
    return 1;
  }
  printf("accepted capture baseline accounts for %llu bytes including its manifest\n",
         (unsigned long long)baseline_total);
  if (!run_capture_cap_error(supervisor, checker, fixture, input,
                             baseline_total - 1u, "capture-cap",
                             UINT64_C(551001), NULL)) {
    fprintf(stderr, "cap one byte below the complete bundle was not refused\n");
    return 1;
  }
  puts("case reached: cap immediately below full stream-plus-manifest total -> capture-cap");
  boundary_caps[0] = baseline_total;
  boundary_caps[1] = baseline_total + 1u;
  boundary_caps[2] = T64_RAW_CAPTURE_MAX_BYTES;
  for (i = 0u; i < sizeof boundary_caps / sizeof boundary_caps[0]; i++) {
    const raw_run_options options = {
        .raw_cap_set = true, .raw_cap = boundary_caps[i]};
    if (run_capture_case(supervisor, checker, fixture, input, &options,
                         &actual_total) != 0 ||
        actual_total != baseline_total) {
      fprintf(stderr,
              "accepted raw cap %llu changed or misaccounted the complete bundle\n",
              (unsigned long long)boundary_caps[i]);
      return 1;
    }
    printf("case reached: accepted cap %llu produced the same %llu-byte full bundle\n",
           (unsigned long long)boundary_caps[i],
           (unsigned long long)actual_total);
  }
  if (!run_capture_cap_error(supervisor, checker, fixture, input, 1u,
                             "capture-cap", UINT64_C(1), NULL) ||
      !run_capture_cap_error(supervisor, checker, fixture, input, 0u,
                             "usage", UINT64_C(0), NULL) ||
      !run_capture_cap_error(supervisor, checker, fixture, input,
                             T64_RAW_CAPTURE_MAX_BYTES + 1u, "usage",
                             UINT64_C(0), NULL)) {
    fprintf(stderr,
            "minimum or malformed raw output cap was not classified correctly\n");
    return 1;
  }
  puts("case reached: 1-byte cap refused output, zero and over-256-MiB caps refused before startup");
  return 0;
}

static int run_capture_incomplete_outputs(char *supervisor, char *checker,
                                         char *fixture, char *input) {
  static const char *const faults[] = {
      "capture-missing-video", "capture-zero-video",
      "capture-truncate-video"};
  static const char *const details[] = {
      "capture-incomplete-streams", "capture-video-incomplete",
      "capture-video-incomplete"};
  const audio_run_case test = {AUDIO_FIXTURE_ID, AUDIO_FIXTURE_SHA256, false};
  size_t i;
  for (i = 0u; i < sizeof faults / sizeof faults[0]; i++) {
    const raw_run_options options = {
        .test_fault = faults[i],
        .expected_error = details[i],
        .minimum_guest_tick = UINT64_C(551001)};
    t64_video_observation video = {0};
    t64_audio_observation audio = {0};
    char bundle[512] = {0};
    char digest[65] = {0};
    struct stat status;
    if (!run_supervised(supervisor, checker, fixture, input, &test, &video,
                        &audio, 0u, bundle, digest, &options) ||
        video.magic != T64_VIDEO_OBSERVATION_MAGIC ||
        video.packets_drained < 2u || audio.packet_count != 2u ||
        lstat(bundle, &status) == 0 || errno != ENOENT) {
      fprintf(stderr,
              "%s did not reject the incomplete actual stream at the reached guest completion boundary\n",
              faults[i]);
      return 1;
    }
    printf("case reached: %s -> %s at tick %llu after actual video=%u/audio=%u packets; no final bundle\n",
           faults[i], details[i],
           (unsigned long long)UINT64_C(551001), video.packets_drained,
           audio.packet_count);
  }
  return 0;
}

static bool count_private_packets(const char *directory, const char *prefix,
                                 const char *suffix, size_t *count_out) {
  DIR *entries = opendir(directory);
  struct dirent *entry;
  size_t count = 0u;
  bool valid = entries != NULL;
  while (valid && (entry = readdir(entries)) != NULL) {
    char path[1024];
    struct stat status;
    size_t name_length = strlen(entry->d_name);
    size_t prefix_length = strlen(prefix);
    size_t suffix_length = strlen(suffix);
    int path_length;
    if (name_length < prefix_length + suffix_length ||
        memcmp(entry->d_name, prefix, prefix_length) != 0 ||
        memcmp(entry->d_name + name_length - suffix_length, suffix,
               suffix_length) != 0) {
      continue;
    }
    path_length = snprintf(path, sizeof path, "%s/%s", directory,
                           entry->d_name);
    if (path_length <= 0 || (size_t)path_length >= sizeof path ||
        lstat(path, &status) != 0 || !S_ISREG(status.st_mode) ||
        status.st_nlink != 1 || status.st_size <= 0 ||
        (status.st_mode & 0777) != 0600) {
      valid = false;
      break;
    }
    count++;
  }
  if (entries != NULL) (void)closedir(entries);
  if (!valid || count == 0u) return false;
  *count_out = count;
  return true;
}

static bool make_capture_test_root(char root[512]) {
  const char *evidence = getenv("T64_AUDIO_EVIDENCE_DIR");
  int length;
  if (evidence == NULL || evidence[0] == '\0') evidence = "/tmp";
  length = snprintf(root, 512u, "%s/raw-transaction.XXXXXX", evidence);
  if (length <= 0 || (size_t)length >= 512u || mkdtemp(root) == NULL ||
      chmod(root, 0700) != 0) {
    return false;
  }
  return true;
}

static bool make_barrier_paths(const char *root, char base[1024],
                               char target[1024]) {
  int base_length = snprintf(base, 1024u, "%s/commit-barrier", root);
  int target_length = snprintf(target, 1024u, "%s/raced-target", root);
  return base_length > 0 && (size_t)base_length < 1024u &&
         target_length > 0 && (size_t)target_length < 1024u;
}

static bool barrier_ready_pids(const char *base, pid_t *pids, size_t capacity,
                               size_t *count_out) {
  char root[1024];
  char prefix[1024];
  char *slash;
  DIR *entries;
  struct dirent *entry;
  size_t count = 0u;
  int length = snprintf(prefix, sizeof prefix, "%s.ready.", base);
  if (length <= 0 || (size_t)length >= sizeof prefix) return false;
  memcpy(root, base, strlen(base) + 1u);
  slash = strrchr(root, '/');
  if (slash == NULL) return false;
  *slash = '\0';
  entries = opendir(root);
  if (entries == NULL) return false;
  while ((entry = readdir(entries)) != NULL) {
    char *end = NULL;
    long parsed;
    char path[1200];
    struct stat status;
    if (strncmp(entry->d_name, strrchr(prefix, '/') + 1,
                strlen(strrchr(prefix, '/') + 1)) != 0) continue;
    errno = 0;
    parsed = strtol(entry->d_name + strlen(strrchr(prefix, '/') + 1), &end, 10);
    length = snprintf(path, sizeof path, "%s/%s", root, entry->d_name);
    if (errno != 0 || end == entry->d_name + strlen(strrchr(prefix, '/') + 1) ||
        *end != '\0' || parsed <= 1 || parsed > INT32_MAX ||
        length <= 0 || (size_t)length >= sizeof path ||
        lstat(path, &status) != 0 || !S_ISREG(status.st_mode) ||
        status.st_uid != geteuid() || (status.st_mode & 0777) != 0600 ||
        count >= capacity) {
      (void)closedir(entries);
      return false;
    }
    pids[count++] = (pid_t)parsed;
  }
  (void)closedir(entries);
  *count_out = count;
  return true;
}

static bool wait_for_barrier_ready(const char *base, size_t wanted,
                                   pid_t *pids) {
  struct timespec delay = {0, 10000000L};
  unsigned attempt;
  for (attempt = 0u; attempt < 3000u; attempt++) {
    size_t count = 0u;
    if (!barrier_ready_pids(base, pids, 4u, &count)) return false;
    if (count == wanted) return true;
    if (count > wanted) return false;
    (void)nanosleep(&delay, NULL);
  }
  return false;
}

static bool release_capture_barrier(const char *base) {
  char path[1200];
  int length = snprintf(path, sizeof path, "%s.go", base);
  int fd;
  if (length <= 0 || (size_t)length >= sizeof path) return false;
  fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
  if (fd < 0) return false;
  return close(fd) == 0;
}

static bool packet_set_matches(const char *left, const char *right,
                               const char *prefix, const char *suffix,
                               size_t packet_count) {
  size_t index;
  char left_dir[1200];
  char right_dir[1200];
  struct stat status;
  size_t left_count = 0u;
  size_t right_count = 0u;
  int left_length = snprintf(left_dir, sizeof left_dir, "%s/%s", left,
                             strcmp(prefix, "frame-") == 0 ? "video" : "audio");
  int right_length = snprintf(right_dir, sizeof right_dir, "%s/%s", right,
                              strcmp(prefix, "frame-") == 0 ? "video" : "audio");
  if (left_length <= 0 || (size_t)left_length >= sizeof left_dir ||
      right_length <= 0 || (size_t)right_length >= sizeof right_dir ||
      lstat(left_dir, &status) != 0 || !S_ISDIR(status.st_mode) ||
      (status.st_mode & 0777) != 0700 ||
      lstat(right_dir, &status) != 0 || !S_ISDIR(status.st_mode) ||
      (status.st_mode & 0777) != 0700 ||
      !count_private_packets(left_dir, prefix, suffix, &left_count) ||
      !count_private_packets(right_dir, prefix, suffix, &right_count) ||
      left_count != packet_count || right_count != packet_count) {
    return false;
  }
  for (index = 0u; index < packet_count; index++) {
    char name[64];
    char left_path[1280];
    char right_path[1280];
    int name_length = snprintf(name, sizeof name, "%s%06zu%s", prefix,
                               index, suffix);
    int lp = snprintf(left_path, sizeof left_path, "%s/%s", left_dir, name);
    int rp = snprintf(right_path, sizeof right_path, "%s/%s", right_dir,
                      name);
    if (name_length <= 0 || (size_t)name_length >= sizeof name ||
        lp <= 0 || (size_t)lp >= sizeof left_path || rp <= 0 ||
        (size_t)rp >= sizeof right_path ||
        !files_equal(left_path, right_path)) return false;
  }
  return true;
}

static bool capture_bundles_equal(const char *expected, const char *actual,
                                  size_t video_count, size_t audio_count) {
  char expected_manifest[1200];
  char actual_manifest[1200];
  struct stat expected_status;
  struct stat actual_status;
  int ep = snprintf(expected_manifest, sizeof expected_manifest,
                    "%s/manifest.json", expected);
  int ap = snprintf(actual_manifest, sizeof actual_manifest,
                    "%s/manifest.json", actual);
  if (ep <= 0 || (size_t)ep >= sizeof expected_manifest || ap <= 0 ||
      (size_t)ap >= sizeof actual_manifest ||
      lstat(expected, &expected_status) != 0 || !S_ISDIR(expected_status.st_mode) ||
      (expected_status.st_mode & 0777) != 0700 ||
      lstat(actual, &actual_status) != 0 || !S_ISDIR(actual_status.st_mode) ||
      (actual_status.st_mode & 0777) != 0700 ||
      lstat(expected_manifest, &expected_status) != 0 ||
      !S_ISREG(expected_status.st_mode) ||
      (expected_status.st_mode & 0777) != 0600 ||
      lstat(actual_manifest, &actual_status) != 0 ||
      !S_ISREG(actual_status.st_mode) ||
      (actual_status.st_mode & 0777) != 0600 ||
      !files_equal(expected_manifest, actual_manifest)) return false;
  return packet_set_matches(expected, actual, "frame-", ".rgba8", video_count) &&
         packet_set_matches(expected, actual, "packet-", ".s16be", audio_count);
}

static bool result_detail_is(const char *path, const char *detail) {
  uint8_t bytes[8192];
  size_t length = 0u;
  t64_json_doc doc = {0};
  bool matches = read_json(path, bytes, sizeof bytes, &length, &doc) &&
                 json_string_is(t64_json_get(doc.root, "detail"), detail);
  t64_json_free(&doc);
  return matches;
}

static bool result_outcome_is(const char *path, const char *outcome) {
  uint8_t bytes[8192];
  size_t length = 0u;
  t64_json_doc doc = {0};
  bool matches = read_json(path, bytes, sizeof bytes, &length, &doc) &&
                 json_string_is(t64_json_get(doc.root, "outcome"), outcome);
  t64_json_free(&doc);
  return matches;
}

static bool command_targets_bundle(const char *directory, const char *target) {
  char path[1200];
  char expected[1200];
  uint8_t bytes[4096];
  size_t length = 0u;
  char *first;
  int path_length = snprintf(path, sizeof path, "%s/command.args", directory);
  int expected_length = snprintf(expected, sizeof expected,
                                 "--raw-capture-dir\n%s\n", target);
  if (path_length <= 0 || (size_t)path_length >= sizeof path ||
      expected_length <= 0 || (size_t)expected_length >= sizeof expected ||
      !read_file(path, bytes, sizeof bytes - 1u, &length) || length == 0u) {
    return false;
  }
  bytes[length] = 0u;
  first = strstr((char *)bytes, expected);
  return first != NULL && strstr(first + expected_length, expected) == NULL;
}

static bool find_result_for_detail(const char *root, const char *detail,
                                   const char *target,
                                   char directory_out[1024],
                                   size_t *matches_out) {
  DIR *entries = opendir(root);
  struct dirent *entry;
  size_t matches = 0u;
  if (entries == NULL) return false;
  while ((entry = readdir(entries)) != NULL) {
    char directory[1024];
    char result[1200];
    int dp;
    int rp;
    if (strncmp(entry->d_name, "audio-capture.", 14u) != 0) continue;
    dp = snprintf(directory, sizeof directory, "%s/%s", root, entry->d_name);
    rp = snprintf(result, sizeof result, "%s/result.json", directory);
    if (dp <= 0 || (size_t)dp >= sizeof directory || rp <= 0 ||
        (size_t)rp >= sizeof result) {
      (void)closedir(entries);
      return false;
    }
    if (result_detail_is(result, detail) &&
        (target == NULL || command_targets_bundle(directory, target))) {
      if (matches == 0u) memcpy(directory_out, directory, strlen(directory) + 1u);
      matches++;
    }
  }
  (void)closedir(entries);
  *matches_out = matches;
  return true;
}

static bool find_result_for_outcome(const char *root, const char *outcome,
                                    const char *target,
                                    char directory_out[1024],
                                    size_t *matches_out) {
  DIR *entries = opendir(root);
  struct dirent *entry;
  size_t matches = 0u;
  if (entries == NULL) return false;
  while ((entry = readdir(entries)) != NULL) {
    char directory[1024];
    char result[1200];
    int dp;
    int rp;
    if (strncmp(entry->d_name, "audio-capture.", 14u) != 0) continue;
    dp = snprintf(directory, sizeof directory, "%s/%s", root, entry->d_name);
    rp = snprintf(result, sizeof result, "%s/result.json", directory);
    if (dp <= 0 || (size_t)dp >= sizeof directory || rp <= 0 ||
        (size_t)rp >= sizeof result) {
      (void)closedir(entries);
      return false;
    }
    if (result_outcome_is(result, outcome) &&
        command_targets_bundle(directory, target)) {
      if (matches == 0u) memcpy(directory_out, directory, strlen(directory) + 1u);
      matches++;
    }
  }
  (void)closedir(entries);
  *matches_out = matches;
  return true;
}

enum {
  CAPTURE_WORKER_INTERRUPT = 1,
  CAPTURE_WORKER_RACE = 2,
  CAPTURE_WORKER_PARENT_REPLACE = 3
};

static int capture_worker(char *supervisor, char *checker, char *fixture,
                          char *input, const char *target, int mode) {
  const bool race = mode == CAPTURE_WORKER_RACE;
  const audio_run_case test = {AUDIO_FIXTURE_ID, AUDIO_FIXTURE_SHA256, false};
  const raw_run_options options = {
      .raw_target_override = target,
      .test_fault = "capture-precommit-barrier",
      .expected_error = mode == CAPTURE_WORKER_INTERRUPT
                            ? "runner-signalled"
                            : NULL,
      .minimum_guest_tick = 0u,
      .allow_commit_race = race};
  t64_video_observation video = {0};
  t64_audio_observation audio = {0};
  char bundle[512] = {0};
  char digest[65] = {0};
  int race_outcome = 0;
  raw_run_options worker_options = options;
  worker_options.race_outcome_out = &race_outcome;
  if (!run_supervised(supervisor, checker, fixture, input, &test, &video,
                      &audio, 0u, bundle, digest, &worker_options)) {
    _exit(22);
  }
  if (race) _exit(race_outcome == 1 ? 20 : race_outcome == 2 ? 21 : 22);
  if (mode != CAPTURE_WORKER_INTERRUPT &&
      mode != CAPTURE_WORKER_PARENT_REPLACE) _exit(22);
  _exit(0);
}

static bool wait_worker(pid_t worker, int *status_out) {
  struct timespec delay = {0, 10000000L};
  unsigned attempt;
  for (attempt = 0u; attempt < 3000u; attempt++) {
    pid_t waited = waitpid(worker, status_out, WNOHANG);
    if (waited == worker) return true;
    if (waited < 0 && errno != EINTR) return false;
    (void)nanosleep(&delay, NULL);
  }
  return false;
}

static int run_parent_replacement_race(char *supervisor, char *checker,
                                      char *fixture, char *input) {
  const audio_run_case test = {AUDIO_FIXTURE_ID, AUDIO_FIXTURE_SHA256, false};
  const uint8_t sentinel[] = {0x4fu, 0x55u, 0x54u, 0x53u, 0x49u, 0x44u, 0x45u};
  t64_video_observation baseline_video = {0};
  t64_audio_observation baseline_audio = {0};
  raw_run_options baseline_options = {0};
  char root[512];
  char baseline[512];
  char baseline_capture[512] = {0};
  char barrier[1024];
  char parent[1024];
  char moved_parent[1024];
  char outside[1024];
  char target[1200];
  char final_target[1200];
  char sentinel_path[1200];
  char evidence[1024];
  char result[1200];
  char producer[1200];
  char meta[1200];
  char original_meta[1200];
  char digest[65] = {0};
  char actual_digest[65] = {0};
  char link_text[1200];
  pid_t worker;
  pid_t ready[4];
  int worker_status = 0;
  size_t matching_results = 0u;
  ssize_t link_length;
  struct stat parent_before;
  struct stat moved_after;
  struct stat outside_before;
  struct stat outside_after;
  struct stat sentinel_before;
  struct stat sentinel_after;
  struct stat link_after;
  struct stat status;
  int length;
  if (!make_capture_test_root(root) ||
      setenv("T64_AUDIO_EVIDENCE_DIR", root, 1) != 0) return 1;
  length = snprintf(baseline, sizeof baseline, "%s/accepted-baseline", root);
  if (length <= 0 || (size_t)length >= sizeof baseline ||
      !make_barrier_paths(root, barrier, target)) return 1;
  length = snprintf(parent, sizeof parent, "%s/opened-parent", root);
  if (length <= 0 || (size_t)length >= sizeof parent) return 1;
  length = snprintf(moved_parent, sizeof moved_parent, "%s/renamed-parent", root);
  if (length <= 0 || (size_t)length >= sizeof moved_parent) return 1;
  length = snprintf(outside, sizeof outside, "%s/outside-sentinel-dir", root);
  if (length <= 0 || (size_t)length >= sizeof outside ||
      mkdir(parent, 0700) != 0 || mkdir(outside, 0700) != 0 ||
      lstat(parent, &parent_before) != 0 || lstat(outside, &outside_before) != 0)
    return 1;
  length = snprintf(target, sizeof target, "%s/final.bundle", parent);
  if (length <= 0 || (size_t)length >= sizeof target) return 1;
  length = snprintf(final_target, sizeof final_target, "%s/final.bundle",
                    moved_parent);
  if (length <= 0 || (size_t)length >= sizeof final_target) return 1;
  length = snprintf(sentinel_path, sizeof sentinel_path, "%s/sentinel.bin",
                    outside);
  if (length <= 0 || (size_t)length >= sizeof sentinel_path) return 1;
  {
    int fd = open(sentinel_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0 || write(fd, sentinel, sizeof sentinel) != (ssize_t)sizeof sentinel ||
        fstat(fd, &sentinel_before) != 0 || close(fd) != 0) {
      if (fd >= 0) (void)close(fd);
      return 1;
    }
  }
  baseline_options.raw_target_override = baseline;
  if (!run_supervised(supervisor, checker, fixture, input, &test,
                      &baseline_video, &baseline_audio, 0u, baseline_capture,
                      digest, &baseline_options) ||
      !fixture_observations_match(&baseline_video, &baseline_audio, false)) {
    fprintf(stderr, "parent replacement baseline was not a complete actual bundle\n");
    return 1;
  }
  if (setenv("T64_RAW_CAPTURE_TEST_BARRIER", barrier, 1) != 0) return 1;
  worker = fork();
  if (worker == 0) capture_worker(supervisor, checker, fixture, input, target,
                                 CAPTURE_WORKER_PARENT_REPLACE);
  if (worker < 0 || !wait_for_barrier_ready(barrier, 1u, ready)) {
    (void)release_capture_barrier(barrier);
    if (worker > 0) (void)wait_worker(worker, &worker_status);
    fprintf(stderr, "parent replacement runner did not reach precommit barrier\n");
    return 1;
  }
  if (rename(parent, moved_parent) != 0 || symlink(outside, parent) != 0 ||
      !release_capture_barrier(barrier) ||
      !wait_worker(worker, &worker_status) || !WIFEXITED(worker_status) ||
      WEXITSTATUS(worker_status) != 0) {
    (void)release_capture_barrier(barrier);
    fprintf(stderr, "parent replacement did not complete through the opened directory FD\n");
    return 1;
  }
  if (!capture_bundles_equal(baseline, final_target,
                             baseline_video.packets_drained,
                             baseline_audio.packet_count) ||
      (lstat(outside, &outside_after) != 0 ||
       outside_before.st_dev != outside_after.st_dev ||
       outside_before.st_ino != outside_after.st_ino ||
       lstat(sentinel_path, &sentinel_after) != 0 ||
       sentinel_before.st_dev != sentinel_after.st_dev ||
       sentinel_before.st_ino != sentinel_after.st_ino ||
       sentinel_before.st_mode != sentinel_after.st_mode ||
       !file_equals_bytes(sentinel_path, sentinel, sizeof sentinel)) ||
      lstat(moved_parent, &moved_after) != 0 || !S_ISDIR(moved_after.st_mode) ||
      moved_after.st_dev != parent_before.st_dev ||
      moved_after.st_ino != parent_before.st_ino ||
      lstat(parent, &link_after) != 0 || !S_ISLNK(link_after.st_mode) ||
      (lstat(target, &status) == 0 || errno != ENOENT) ||
      (lstat(final_target, &status) != 0 || !S_ISDIR(status.st_mode))) {
    fprintf(stderr, "parent replacement escaped the retained FD or altered outside state\n");
    return 1;
  }
  link_length = readlink(parent, link_text, sizeof link_text - 1u);
  if (link_length < 0 || (size_t)link_length >= sizeof link_text) return 1;
  link_text[link_length] = '\0';
  if (strcmp(link_text, outside) != 0 ||
      !find_result_for_outcome(root, "pass", target, evidence,
                               &matching_results) || matching_results != 1u) {
    fprintf(stderr, "parent path replacement or argv evidence was not retained\n");
    return 1;
  }
  {
    int result_length = snprintf(result, sizeof result, "%s/result.json", evidence);
    int producer_length = snprintf(producer, sizeof producer,
                                   "%s/producer-report.json", evidence);
    int meta_length = snprintf(meta, sizeof meta, "%s/meta.json", evidence);
    int original_length = snprintf(original_meta, sizeof original_meta,
                                   "%s/supervisor-meta.original.json", evidence);
    if (result_length <= 0 || (size_t)result_length >= sizeof result ||
        producer_length <= 0 || (size_t)producer_length >= sizeof producer ||
        meta_length <= 0 || (size_t)meta_length >= sizeof meta ||
        original_length <= 0 || (size_t)original_length >= sizeof original_meta ||
        !result_is_pass(result, &test, actual_digest) ||
      strcmp(actual_digest, digest) != 0 || access(producer, F_OK) != 0 ||
        !files_equal(meta, original_meta)) {
      fprintf(stderr, "parent replacement run lost its actual guest result evidence\n");
      return 1;
    }
  }
  {
    uint8_t result_bytes[8192];
    size_t result_length = 0u;
    t64_json_doc result_doc = {0};
    uint64_t actual_tick = 0u;
    bool tick_matches = read_json(result, result_bytes, sizeof result_bytes,
                                  &result_length, &result_doc) &&
                        t64_json_u64_string(
                            t64_json_get(result_doc.root, "guest_tick"),
                            &actual_tick) != 0 &&
                        actual_tick == UINT64_C(551001);
    t64_json_free(&result_doc);
    if (!tick_matches) return 1;
  }
  printf("case reached: opened parent directory was renamed and its old path replaced by a symlink; bundle committed only under the retained directory FD at tick 551001, outside sentinel unchanged\n");
  return 0;
}

static int run_capture_interrupted_stage(char *supervisor, char *checker,
                                         char *fixture, char *input) {
  const audio_run_case test = {AUDIO_FIXTURE_ID, AUDIO_FIXTURE_SHA256, false};
  t64_video_observation baseline_video = {0};
  t64_audio_observation baseline_audio = {0};
  raw_run_options baseline_options = {0};
  char root[512];
  char baseline[512];
  char baseline_capture[512] = {0};
  char barrier[1024];
  char target[1024];
  char digest[65] = {0};
  char evidence[1024];
  char path[1200];
  char stage[1200] = {0};
  char original_meta[1200];
  char meta[1200];
  char args[1200];
  pid_t worker;
  pid_t ready[4];
  pid_t runner;
  int worker_status = 0;
  size_t stage_count = 0u;
  size_t matching_results = 0u;
  struct stat status;
  DIR *entries;
  struct dirent *entry;
  if (!make_capture_test_root(root)) return 1;
  if (setenv("T64_AUDIO_EVIDENCE_DIR", root, 1) != 0) return 1;
  if (snprintf(baseline, sizeof baseline, "%s/accepted-baseline", root) <= 0 ||
      !make_barrier_paths(root, barrier, target)) return 1;
  baseline_options.raw_target_override = baseline;
  if (!run_supervised(supervisor, checker, fixture, input, &test,
                      &baseline_video, &baseline_audio, 0u, baseline_capture, digest,
                      &baseline_options) ||
      !fixture_observations_match(&baseline_video, &baseline_audio, false) ||
      baseline_video.packets_drained < 2u || baseline_audio.packet_count != 2u) {
    fprintf(stderr, "interruption baseline was not a complete actual A/V capture\n");
    return 1;
  }
  if (setenv("T64_RAW_CAPTURE_TEST_BARRIER", barrier, 1) != 0) return 1;
  worker = fork();
  if (worker == 0) capture_worker(supervisor, checker, fixture, input,
                                 target, CAPTURE_WORKER_INTERRUPT);
  if (worker < 0 || !wait_for_barrier_ready(barrier, 1u, ready)) {
    (void)release_capture_barrier(barrier);
    if (worker > 0) (void)wait_worker(worker, &worker_status);
    fprintf(stderr, "interruption runner did not reach the actual precommit barrier\n");
    return 1;
  }
  runner = ready[0];
  if (kill(runner, SIGKILL) != 0 || !wait_worker(worker, &worker_status) ||
      !WIFEXITED(worker_status) || WEXITSTATUS(worker_status) != 0) {
    (void)release_capture_barrier(barrier);
    fprintf(stderr, "precommit runner kill was not recorded by the supervisor\n");
    return 1;
  }
  if (!find_result_for_detail(root, "runner-signalled", target, evidence,
                              &matching_results) || matching_results != 1u) return 1;
  {
    int result_length = snprintf(path, sizeof path, "%s/result.json", evidence);
    if (result_length <= 0 || (size_t)result_length >= sizeof path ||
        !result_detail_is(path, "runner-signalled")) return 1;
  }
  (void)snprintf(original_meta, sizeof original_meta,
                 "%s/supervisor-meta.original.json", evidence);
  (void)snprintf(meta, sizeof meta, "%s/meta.json", evidence);
  (void)snprintf(args, sizeof args, "%s/command.args", evidence);
  {
    int report_length = snprintf(path, sizeof path, "%s/producer-report.json",
                                 evidence);
    if (access(original_meta, F_OK) != 0 || access(meta, F_OK) != 0 ||
        !files_equal(original_meta, meta) || access(args, F_OK) != 0 ||
        report_length <= 0 || (size_t)report_length >= sizeof path ||
        access(path, F_OK) == 0) {
      fprintf(stderr, "interruption did not preserve actual supervisor evidence\n");
      return 1;
    }
  }
  {
    uint8_t meta_bytes[4096];
    uint8_t args_bytes[4096];
    size_t meta_length = 0u;
    size_t args_length = 0u;
    (void)snprintf(original_meta, sizeof original_meta,
                   "%s/supervisor-meta.original.json", evidence);
    (void)snprintf(args, sizeof args, "%s/command.args", evidence);
    if (!read_file(original_meta, meta_bytes, sizeof meta_bytes, &meta_length) ||
        meta_length == 0u || meta_length >= sizeof meta_bytes) {
      fprintf(stderr, "interruption metadata could not be read\n");
      return 1;
    }
    meta_bytes[meta_length] = 0u;
    if (strstr((char *)meta_bytes, "\"wait_status\":9") == NULL ||
        !read_file(args, args_bytes, sizeof args_bytes - 1u, &args_length) ||
        args_length == 0u) {
      fprintf(stderr, "interruption metadata does not show SIGKILL or argv\n");
      return 1;
    }
    args_bytes[args_length] = 0u;
    if (strstr((char *)args_bytes, "--raw-capture-dir") == NULL ||
        strstr((char *)args_bytes, target) == NULL ||
        strstr((char *)args_bytes, "--test-fault") == NULL ||
        strstr((char *)args_bytes, "capture-precommit-barrier") == NULL) {
      fprintf(stderr, "interruption command evidence lacks target or barrier fault\n");
      return 1;
    }
  }
  if (lstat(target, &status) == 0 || errno != ENOENT) {
    fprintf(stderr, "interrupted capture published a final target\n");
    return 1;
  }
  entries = opendir(root);
  if (entries == NULL) return 1;
  while ((entry = readdir(entries)) != NULL) {
    if (strncmp(entry->d_name, ".t64-probe-", 11u) == 0) {
      int length = snprintf(stage, sizeof stage, "%s/%s", root, entry->d_name);
      if (length <= 0 || (size_t)length >= sizeof stage) {
        (void)closedir(entries);
        return 1;
      }
      stage_count++;
    }
  }
  (void)closedir(entries);
  if (stage_count != 1u ||
      !capture_bundles_equal(baseline, stage,
                             baseline_video.packets_drained,
                             baseline_audio.packet_count)) {
    fprintf(stderr, "killed process left no intact private precommit stage\n");
    return 1;
  }
  printf("case reached: SIGKILL at the actual precommit barrier left an intact private stage and no final target; baseline tick 551001, digest %s\n",
         digest);
  return 0;
}

static int run_capture_commit_race(char *supervisor, char *checker,
                                   char *fixture, char *input) {
  const audio_run_case test = {AUDIO_FIXTURE_ID, AUDIO_FIXTURE_SHA256, false};
  t64_video_observation baseline_video = {0};
  t64_audio_observation baseline_audio = {0};
  raw_run_options baseline_options = {0};
  char root[512];
  char baseline[512];
  char baseline_capture[512] = {0};
  char barrier[1024];
  char target[1024];
  char digest[65] = {0};
  char pass_evidence[1024];
  char loser_evidence[1024];
  char result[1200];
  pid_t workers[2] = {-1, -1};
  pid_t ready[4];
  int statuses[2] = {0, 0};
  size_t pass_count = 0u;
  size_t loser_count = 0u;
  uint64_t guest_tick = 0u;
  if (!make_capture_test_root(root) ||
      setenv("T64_AUDIO_EVIDENCE_DIR", root, 1) != 0 ||
      snprintf(baseline, sizeof baseline, "%s/accepted-baseline", root) <= 0 ||
      !make_barrier_paths(root, barrier, target)) return 1;
  baseline_options.raw_target_override = baseline;
  if (!run_supervised(supervisor, checker, fixture, input, &test,
                      &baseline_video, &baseline_audio, 0u, baseline_capture, digest,
                      &baseline_options) ||
      !fixture_observations_match(&baseline_video, &baseline_audio, false)) {
    fprintf(stderr, "race baseline was not a complete actual A/V capture\n");
    return 1;
  }
  if (setenv("T64_RAW_CAPTURE_TEST_BARRIER", barrier, 1) != 0) return 1;
  workers[0] = fork();
  if (workers[0] == 0) capture_worker(supervisor, checker, fixture, input,
                                      target, CAPTURE_WORKER_RACE);
  workers[1] = fork();
  if (workers[1] == 0) capture_worker(supervisor, checker, fixture, input,
                                      target, CAPTURE_WORKER_RACE);
  if (workers[0] < 0 || workers[1] < 0 ||
      !wait_for_barrier_ready(barrier, 2u, ready) ||
      !release_capture_barrier(barrier) ||
      !wait_worker(workers[0], &statuses[0]) ||
      !wait_worker(workers[1], &statuses[1])) {
    (void)release_capture_barrier(barrier);
    if (workers[0] > 0 && waitpid(workers[0], &statuses[0], WNOHANG) == 0)
      (void)wait_worker(workers[0], &statuses[0]);
    if (workers[1] > 0 && waitpid(workers[1], &statuses[1], WNOHANG) == 0)
      (void)wait_worker(workers[1], &statuses[1]);
    fprintf(stderr, "two actual capture contenders did not reach and leave the shared barrier\n");
    return 1;
  }
  if (!WIFEXITED(statuses[0]) || !WIFEXITED(statuses[1]) ||
      !((WEXITSTATUS(statuses[0]) == 20 && WEXITSTATUS(statuses[1]) == 21) ||
        (WEXITSTATUS(statuses[0]) == 21 && WEXITSTATUS(statuses[1]) == 20))) {
    fprintf(stderr, "no-replace race did not have exactly one winner and one loser\n");
    return 1;
  }
  if (!capture_bundles_equal(baseline, target,
                             baseline_video.packets_drained,
                             baseline_audio.packet_count) ||
      !find_result_for_outcome(root, "pass", target, pass_evidence,
                                &pass_count) ||
      !find_result_for_detail(root, "capture-no-replace-commit", target,
                              loser_evidence, &loser_count) ||
      pass_count != 1u || loser_count != 1u) {
    fprintf(stderr, "race did not publish one intact bundle with one commit refusal\n");
    return 1;
  }
  {
    char loser_digest[65] = {0};
    int result_length = snprintf(result, sizeof result, "%s/result.json",
                                 loser_evidence);
    if (result_length <= 0 || (size_t)result_length >= sizeof result ||
        !result_is_capture_error(result, "capture-no-replace-commit",
                                 UINT64_C(551001), loser_digest,
                                 &guest_tick) ||
        strcmp(loser_digest, digest) != 0) return 1;
    {
      char producer[1200];
      char original_meta_path[1200];
      int producer_length = snprintf(producer, sizeof producer,
                                     "%s/producer-report.json", loser_evidence);
      int meta_length = snprintf(original_meta_path, sizeof original_meta_path,
                                 "%s/supervisor-meta.original.json",
                                 loser_evidence);
      if (producer_length <= 0 || (size_t)producer_length >= sizeof producer ||
          meta_length <= 0 || (size_t)meta_length >= sizeof original_meta_path ||
          access(producer, F_OK) != 0 || access(original_meta_path, F_OK) != 0) {
        fprintf(stderr, "race loser producer report or original metadata was not preserved\n");
        return 1;
      }
    }
  }
  if (guest_tick != UINT64_C(551001) ||
      !capture_bundles_equal(baseline, target,
                             baseline_video.packets_drained,
                             baseline_audio.packet_count)) {
    fprintf(stderr, "race loser report or published bytes differ from the actual baseline\n");
    return 1;
  }
  printf("case reached: two actual runners crossed the shared precommit barrier; one published the baseline bundle and one reported capture-no-replace-commit at tick %llu with digest %s\n",
         (unsigned long long)guest_tick, digest);
  return 0;
}

static int run_drain_pacing_case(char *supervisor, char *checker,
                                char *fixture, char *input) {
  const audio_run_case test = {AUDIO_FIXTURE_ID, AUDIO_FIXTURE_SHA256, false};
  t64_video_observation video[2] = {{0}};
  t64_audio_observation audio[2] = {{0}};
  char digest[2][65] = {{0}};
  uint64_t advance_steps[2] = {UINT64_C(16), UINT64_C(32)};
  size_t packet;
  size_t run;

  for (run = 0u; run < 2u; run++) {
    if (!run_supervised(supervisor, checker, fixture, input, &test,
                        &video[run], &audio[run], advance_steps[run],
                        NULL, digest[run], NULL) ||
        !fixture_observations_match(&video[run], &audio[run], false)) {
      fprintf(stderr,
              "accepted runner setup or literal packet validation failed at advance step %llu\n",
              (unsigned long long)advance_steps[run]);
      return 1;
    }
  }
  if (audio[0].packet_count != T64_AUDIO_OBSERVATION_PACKET_COUNT ||
      audio[1].packet_count != T64_AUDIO_OBSERVATION_PACKET_COUNT ||
      audio[0].guest_stop_tick == 0u ||
      audio[0].guest_stop_tick != audio[1].guest_stop_tick ||
      audio[0].guest_stop_reason != T64_STOP_REASON_GUEST_COMPLETION ||
      audio[1].guest_stop_reason != T64_STOP_REASON_GUEST_COMPLETION ||
      audio[0].guest_last_cpu_tick != audio[1].guest_last_cpu_tick ||
      strcmp(digest[0], digest[1]) != 0) {
    fprintf(stderr,
            "runner pacing changed guest completion boundary or canonical digest\n");
    return 1;
  }
  for (packet = 0u; packet < T64_AUDIO_OBSERVATION_PACKET_COUNT; packet++) {
    if (audio[0].packets[packet].info.byte_length == 0u ||
        audio[1].packets[packet].info.byte_length == 0u ||
        memcmp(&audio[0].packets[packet].info,
               &audio[1].packets[packet].info,
               sizeof audio[0].packets[packet].info) != 0 ||
        memcmp(audio[0].packets[packet].bytes,
               audio[1].packets[packet].bytes,
               T64_AUDIO_OBSERVATION_PACKET_BYTES) != 0) {
      fprintf(stderr,
              "runner pacing changed packet %zu metadata or raw PCM bytes\n",
              packet);
      return 1;
    }
  }
  printf("actual runner drain steps 16 and 32 matched two exact packets at guest tick %llu and the same canonical digest\n",
         (unsigned long long)audio[0].guest_stop_tick);
  return 0;
}

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    puts("runner-literal-pcm");
    puts("reached-wrong-sample");
    puts("runner-drain-pacing");
    puts("actual-av-bundle");
    puts("general-frame-geometry");
    puts("literal-pcm-files");
    puts("manifest-exact-identities");
    puts("capture-drain-resume");
    puts("short-write-and-eintr");
    puts("existing-targets-untouched");
    puts("existing-file-directory-untouched");
    puts("unsafe-target-parent");
    puts("io-write-close-fsync-rename-failures");
    puts("host-error-guest-state");
    puts("normal-cli-rejects-test-fault");
    puts("output-cap-boundaries");
    puts("closed-capture-error-vocabulary");
    puts("missing-zero-truncated-output");
    puts("interrupted-stage");
    puts("racing-finalization");
    return 0;
  }
  if (argc == 8 && strcmp(argv[1], "actual-av-bundle") == 0) {
    return run_capture_suite(argv[2], argv[3], argv[4], argv[5], argv[6],
                             argv[7]);
  }
  if (argc == 6 && strcmp(argv[1], "runner-drain-pacing") == 0) {
    return run_drain_pacing_case(argv[2], argv[3], argv[4], argv[5]);
  }
  if (argc == 6 && strcmp(argv[1], "short-write-and-eintr") == 0) {
    return run_capture_short_write_case(argv[2], argv[3], argv[4], argv[5]);
  }
  if (argc == 6 && strcmp(argv[1], "capture-target-refusals") == 0) {
    return run_capture_target_refusals(argv[2], argv[3], argv[4], argv[5]);
  }
  if (argc == 7 && strcmp(argv[1], "io-errors") == 0) {
    return run_capture_io_errors(argv[2], argv[3], argv[4], argv[5], argv[6]);
  }
  if (argc == 6 && strcmp(argv[1], "cap-boundaries") == 0) {
    return run_capture_output_caps(argv[2], argv[3], argv[4], argv[5]);
  }
  if (argc == 6 && strcmp(argv[1], "capture-incomplete") == 0) {
    return run_capture_incomplete_outputs(argv[2], argv[3], argv[4], argv[5]);
  }
  if (argc == 6 && strcmp(argv[1], "capture-interrupted") == 0) {
    return run_capture_interrupted_stage(argv[2], argv[3], argv[4], argv[5]);
  }
  if (argc == 6 && strcmp(argv[1], "capture-race") == 0) {
    return run_capture_commit_race(argv[2], argv[3], argv[4], argv[5]);
  }
  if (argc != 6 ||
      (strcmp(argv[1], "runner-literal-pcm") != 0 &&
       strcmp(argv[1], "reached-wrong-sample") != 0)) {
    fprintf(stderr,
            "usage: %s runner-literal-pcm|reached-wrong-sample|runner-drain-pacing <supervisor> <checker> <fixture> <input>\n",
            argv[0]);
    return 2;
  }
  return run_audio_case(argv[1], argv[2], argv[3], argv[4], argv[5]);
}
