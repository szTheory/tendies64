#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static bool run_supervised(char *supervisor, char *checker, char *fixture,
                           char *input, const audio_run_case *test,
                           t64_video_observation *video,
                           t64_audio_observation *audio,
                           uint64_t advance_step,
                           char raw_bundle_path_out[512],
                           char state_digest_out[65]) {
  char directory[4096];
  char result_path[512] = {0};
  char meta_path[512] = {0};
  char capture_path[512] = {0};
  char raw_bundle_path[512] = {0};
  char work_dir[512] = {0};
  char video_fd_text[24];
  char audio_fd_text[24];
  char advance_step_text[24];
  int video_pipe[2] = {-1, -1};
  int audio_pipe[2] = {-1, -1};
  int status = 0;
  pid_t child = -1;
  int length;
  const char *evidence_root = getenv("T64_AUDIO_EVIDENCE_DIR");
  bool pipe_read_ok = false;
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
    char guest_bound[] = "600000";
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
    char video_option[] = "--video-observation-fd";
    char audio_option[] = "--audio-observation-fd";
    char input_sha[] = INPUT_SHA256;
    char fixture_id[64];
    char fixture_sha256[65];
    char *args[48];
    char advance_step_option[] = "--advance-step";
    size_t count = 0u;
    int fixture_id_length = snprintf(fixture_id, sizeof fixture_id, "%s",
                                     test->fixture_id);
    int fixture_sha_length = snprintf(fixture_sha256, sizeof fixture_sha256,
                                      "%s", test->fixture_sha256);
    if (fixture_id_length <= 0 ||
        (size_t)fixture_id_length >= sizeof fixture_id ||
        fixture_sha_length != 64 ||
        (size_t)fixture_sha_length >= sizeof fixture_sha256) {
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
      args[count++] = raw_bundle_path_out;
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
  pipe_read_ok = t64_test_observation_pipes_wait(child, pipes, 2u, &status);
  child = -1;
  checker_ok = checker_accepts(checker, result_path);
  result_ok = result_is_pass(result_path, test, state_digest_out);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0 || !pipe_read_ok ||
      !checker_ok || !result_ok) {
    uint8_t report[8192];
    size_t report_length = 0u;
    fprintf(stderr,
            "supervised audio setup failed: exit=%d pipe=%d checker=%d result=%d\n",
            WIFEXITED(status) ? WEXITSTATUS(status) : -1, pipe_read_ok,
            checker_ok, result_ok);
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
    int read_status_length;
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
                      &audio, 0u, NULL, NULL) ||
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
                            char *input) {
  const audio_run_case test = {AUDIO_FIXTURE_ID, AUDIO_FIXTURE_SHA256, false};
  t64_video_observation video = {0};
  t64_audio_observation audio = {0};
  char raw_bundle_path[512] = {0};
  struct stat bundle_stat;
  char manifest_path[1024];
  int length;

  if (!run_supervised(supervisor, checker, fixture, input, &test, &video,
                      &audio, 0u, raw_bundle_path, NULL)) {
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
    puts("result checker accepted the closed capture host-error detail and rejected unknown/wrong-outcome mutations");
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
                        digest[run]) ||
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
  if (run_capture_case(supervisor, checker, fixture, input) != 0) return 1;
  puts("case reached: actual-av-bundle");
  if (run_geometry_case(test_supervisor, checker, geometry_fixture, input) != 0)
    return 1;
  puts("case reached: general-frame-geometry");
  if (run_capture_case(supervisor, checker, fixture, input) != 0) return 1;
  puts("case reached: literal-pcm-files");
  if (run_capture_case(supervisor, checker, fixture, input) != 0) return 1;
  puts("case reached: manifest-exact-identities");
  if (run_capture_resume_case(test_supervisor, checker, fixture, input) != 0)
    return 1;
  puts("case reached: capture-drain-resume");
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
                        NULL, digest[run]) ||
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
    return 0;
  }
  if (argc == 8 && strcmp(argv[1], "actual-av-bundle") == 0) {
    return run_capture_suite(argv[2], argv[3], argv[4], argv[5], argv[6],
                             argv[7]);
  }
  if (argc == 6 && strcmp(argv[1], "runner-drain-pacing") == 0) {
    return run_drain_pacing_case(argv[2], argv[3], argv[4], argv[5]);
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
