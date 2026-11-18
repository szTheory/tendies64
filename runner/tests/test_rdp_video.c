#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "t64_json.h"
#include "t64_video_observation.h"

#define FILL_FIXTURE_ID "rdp-vi-fill-diagnostic"
#define FILL_FIXTURE_SHA256 \
  "b469340e7caa0ac341a2a5b525aa7e3b39c8a47f295ecd10710ae695e46dfc3d"
#define FILL_PROFILE_ID "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v2"
#define FILL_PROFILE_SHA256 \
  "5612696247abb5dae1c2abb6c15d7fd6c5632136210be7b21a1085590af1c94e"
#define INPUT_SHA256 \
  "6d5593bc2d95465b44ec7ce7785e5ff7d7d53605de9b039a021bc697d226186d"
#define FILL_PROFILE_CLOCK_POLICY "rsp62500000-ntsc48681812-rational-v1"

typedef enum {
  TEST_ACCEPTED_FILL,
  TEST_WRONG_PIXEL,
  TEST_REPLACED_FIXTURE,
  TEST_DRAIN_CADENCE
} test_case_id;

typedef struct {
  const char *name;
  test_case_id id;
} test_case;

static const test_case cases[] = {
    {"accepted-cpu-fill-to-raw-frame", TEST_ACCEPTED_FILL},
    {"same-setup-wrong-pixel", TEST_WRONG_PIXEL},
    {"replacement-fixture-refused", TEST_REPLACED_FIXTURE},
    {"drain-cadence-real-runner", TEST_DRAIN_CADENCE},
};

static const uint8_t expected_source[T64_VIDEO_OBSERVATION_SOURCE_BYTES] = {
    0xf8, 0x01, 0x07, 0xc1, 0xf8, 0x01, 0x07, 0xc1,
    0xf8, 0x01, 0x07, 0xc1, 0xf8, 0x01, 0x07, 0xc1,
    0xf8, 0x01, 0x07, 0xc1, 0xf8, 0x01, 0x07, 0xc1,
};

static const uint8_t expected_packet[T64_VIDEO_OBSERVATION_PACKET_BYTES] = {
    255, 0, 0, 255, 0, 255, 0, 255, 255, 0, 0, 255, 0, 255, 0, 255,
    255, 0, 0, 255, 0, 255, 0, 255, 255, 0, 0, 255, 0, 255, 0, 255,
    255, 0, 0, 255, 0, 255, 0, 255, 255, 0, 0, 255, 0, 255, 0, 255,
};

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
      close(fd);
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
      close(fd);
      return false;
    }
  }
  if (close(fd) != 0) return false;
  *length_out = total;
  return true;
}

static bool read_pipe_record(int fd, t64_video_observation *record) {
  uint8_t *bytes = (uint8_t *)record;
  size_t total = 0u;
  while (total < sizeof *record) {
    ssize_t count = read(fd, bytes + total, sizeof *record - total);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) return false;
    total += (size_t)count;
  }
  {
    uint8_t extra;
    ssize_t count;
    do {
      count = read(fd, &extra, 1u);
    } while (count < 0 && errno == EINTR);
    return count == 0;
  }
}

static bool checker_accepts(char *checker, char *result_path) {
  pid_t pid = fork();
  int status = 0;
  if (pid == 0) {
    char mode[] = "result";
    char *const args[] = {checker, mode, result_path, NULL};
    int null_fd = open("/dev/null", O_WRONLY);
    if (null_fd >= 0) {
      (void)dup2(null_fd, STDOUT_FILENO);
      (void)dup2(null_fd, STDERR_FILENO);
    }
    execv(checker, args);
    _exit(127);
  }
  if (pid < 0) return false;
  while (waitpid(pid, &status, 0) < 0) {
    if (errno == EINTR) continue;
    return false;
  }
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool result_is_pass(const char *result_path,
                           char state_digest_out[65]) {
  uint8_t bytes[4097];
  size_t length = 0u;
  t64_json_doc doc = {0};
  const t64_json_node *outcome;
  const t64_json_node *profile;
  const t64_json_node *profile_digest;
  const t64_json_node *state_digest;
  bool pass = false;
  if (!read_file(result_path, bytes, sizeof bytes, &length) || length == 0u ||
      t64_json_parse(bytes, length, NULL, &doc, NULL) != 0) {
    return false;
  }
  outcome = t64_json_get(doc.root, "outcome");
  profile = t64_json_get(doc.root, "profile_id");
  profile_digest = t64_json_get(doc.root, "profile_digest");
  state_digest = t64_json_get(doc.root, "state_digest");
  if (outcome != NULL && outcome->type == T64_JSON_STRING &&
      outcome->len == 4u && memcmp(outcome->str, "pass", 4u) == 0) {
    pass = profile != NULL && profile->type == T64_JSON_STRING &&
           profile->len == strlen(FILL_PROFILE_ID) &&
           memcmp(profile->str, FILL_PROFILE_ID, profile->len) == 0 &&
           profile_digest != NULL &&
           profile_digest->type == T64_JSON_STRING &&
           profile_digest->len == 64u &&
           memcmp(profile_digest->str, FILL_PROFILE_SHA256, 64u) == 0 &&
           state_digest != NULL && state_digest->type == T64_JSON_STRING &&
           state_digest->len == 64u;
  }
  if (pass && state_digest_out != NULL) {
    memcpy(state_digest_out, state_digest->str, 64u);
    state_digest_out[64] = '\0';
  }
  t64_json_free(&doc);
  return pass;
}

static bool result_is_fixture_refusal(const char *result_path) {
  uint8_t bytes[4097];
  size_t length = 0u;
  t64_json_doc doc = {0};
  const t64_json_node *outcome;
  const t64_json_node *detail;
  const t64_json_node *fixture;
  const t64_json_node *fixture_sha;
  const t64_json_node *profile;
  const t64_json_node *source_revision;
  const t64_json_node *guest_stop;
  const t64_json_node *stop_kind;
  bool refused = false;

  if (!read_file(result_path, bytes, sizeof bytes, &length) || length == 0u ||
      t64_json_parse(bytes, length, NULL, &doc, NULL) != 0) {
    return false;
  }
  outcome = t64_json_get(doc.root, "outcome");
  detail = t64_json_get(doc.root, "detail");
  fixture = t64_json_get(doc.root, "fixture_id");
  fixture_sha = t64_json_get(doc.root, "fixture_sha256");
  profile = t64_json_get(doc.root, "profile_id");
  source_revision = t64_json_get(doc.root, "source_revision");
  guest_stop = t64_json_get(doc.root, "guest_stop");
  stop_kind = t64_json_get(guest_stop, "kind");
  if (outcome != NULL && outcome->type == T64_JSON_STRING &&
      outcome->len == strlen("harness-error") &&
      memcmp(outcome->str, "harness-error", outcome->len) == 0 &&
      detail != NULL && detail->type == T64_JSON_STRING &&
      detail->len == strlen("refusal") &&
      memcmp(detail->str, "refusal", detail->len) == 0 &&
      fixture != NULL && fixture->type == T64_JSON_NULL &&
      fixture_sha != NULL && fixture_sha->type == T64_JSON_NULL &&
      profile != NULL && profile->type == T64_JSON_NULL &&
      source_revision != NULL && source_revision->type == T64_JSON_NULL &&
      stop_kind != NULL && stop_kind->type == T64_JSON_STRING &&
      stop_kind->len == strlen("refused") &&
      memcmp(stop_kind->str, "refused", stop_kind->len) == 0) {
    refused = true;
  }
  t64_json_free(&doc);
  return refused;
}

static bool write_replacement_fixture(const char *source_path,
                                      const char *destination_path) {
  uint8_t bytes[65536];
  size_t length = 0u;
  size_t offset = 0u;
  int fd;

  if (!read_file(source_path, bytes, sizeof bytes, &length) || length == 0u)
    return false;
  bytes[0] ^= UINT8_C(1);
  fd = open(destination_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (fd < 0) return false;
  while (offset < length) {
    ssize_t count = write(fd, bytes + offset, length - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) {
      close(fd);
      return false;
    }
    offset += (size_t)count;
  }
  return close(fd) == 0;
}

static bool run_supervised(char *supervisor, char *checker,
                           char *fixture, char *input,
                           bool expect_refusal, uint64_t advance_step,
                           char state_digest_out[65],
                           t64_video_observation *record) {
  char directory[] = "/tmp/t64-rdp-video.XXXXXX";
  char result_path[512];
  char meta_path[512];
  char capture_path[512];
  char work_dir[512];
  char replacement_path[512];
  char *fixture_to_use = fixture;
  char fd_text[24];
  char advance_step_text[24];
  int pipe_fds[2] = {-1, -1};
  int status = 0;
  pid_t pid;
  int length;
  bool record_ok;
  bool check_ok;
  bool result_pass;
  bool pass = false;
  bool replacement_written = false;
  if (mkdtemp(directory) == NULL || pipe(pipe_fds) != 0) return false;
  length = snprintf(result_path, sizeof result_path, "%s/result.json",
                    directory);
  if (length <= 0 || (size_t)length >= sizeof result_path) goto cleanup;
  length = snprintf(meta_path, sizeof meta_path, "%s/meta.json", directory);
  if (length <= 0 || (size_t)length >= sizeof meta_path) goto cleanup;
  length = snprintf(capture_path, sizeof capture_path, "%s/capture.bin",
                    directory);
  if (length <= 0 || (size_t)length >= sizeof capture_path) goto cleanup;
  length = snprintf(work_dir, sizeof work_dir, "%s", directory);
  if (length <= 0 || (size_t)length >= sizeof work_dir) goto cleanup;
  replacement_path[0] = '\0';
  if (expect_refusal) {
    length = snprintf(replacement_path, sizeof replacement_path,
                      "%s/replaced-fixture.z64", directory);
    if (length <= 0 || (size_t)length >= sizeof replacement_path ||
        !write_replacement_fixture(fixture, replacement_path)) {
      goto cleanup;
    }
    replacement_written = true;
    fixture_to_use = replacement_path;
  }
  (void)snprintf(fd_text, sizeof fd_text, "%d", pipe_fds[1]);
  (void)snprintf(advance_step_text, sizeof advance_step_text, "%llu",
                 (unsigned long long)advance_step);

  pid = fork();
  if (pid == 0) {
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
    char advance_step_option[] = "--advance-step";
    char video_option[] = "--video-observation-fd";
    char fixture_id[] = FILL_FIXTURE_ID;
    char fixture_sha[] = FILL_FIXTURE_SHA256;
    char input_sha[] = INPUT_SHA256;
    char *args[40];
    size_t arg_count = 0u;
    args[arg_count++] = supervisor;
    args[arg_count++] = real_option;
    args[arg_count++] = result_option;
    args[arg_count++] = result_path;
    args[arg_count++] = meta_option;
    args[arg_count++] = meta_path;
    args[arg_count++] = work_option;
    args[arg_count++] = work_dir;
    args[arg_count++] = wall_option;
    args[arg_count++] = wall_limit;
    args[arg_count++] = bound_option;
    args[arg_count++] = guest_bound;
    args[arg_count++] = log_option;
    args[arg_count++] = log_limit;
    args[arg_count++] = fixture_id_option;
    args[arg_count++] = fixture_id;
    args[arg_count++] = fixture_option;
    args[arg_count++] = fixture_to_use;
    args[arg_count++] = fixture_sha_option;
    args[arg_count++] = fixture_sha;
    args[arg_count++] = input_option;
    args[arg_count++] = input;
    args[arg_count++] = input_size_option;
    args[arg_count++] = input_size;
    args[arg_count++] = input_sha_option;
    args[arg_count++] = input_sha;
    args[arg_count++] = expect_pass_option;
    args[arg_count++] = expect_pass;
    args[arg_count++] = capture_option;
    args[arg_count++] = capture_path;
    if (advance_step != 0u) {
      args[arg_count++] = advance_step_option;
      args[arg_count++] = advance_step_text;
    }
    args[arg_count++] = video_option;
    args[arg_count++] = fd_text;
    args[arg_count] = NULL;
    close(pipe_fds[0]);
    execv(supervisor, args);
    _exit(127);
  }
  if (pid < 0) goto cleanup;
  close(pipe_fds[1]);
  pipe_fds[1] = -1;
  while (waitpid(pid, &status, 0) < 0) {
    if (errno == EINTR) continue;
    goto cleanup;
  }
  if (expect_refusal) {
    bool refused = result_is_fixture_refusal(result_path);
    bool not_launched = access(meta_path, F_OK) != 0;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 1 || !refused ||
        !not_launched) {
      uint8_t report[4097];
      size_t report_length = 0u;
      fprintf(stderr,
              "replacement fixture was not refused before runner launch: exit=%d refused=%d meta-absent=%d\n",
              WIFEXITED(status) ? WEXITSTATUS(status) : -1, refused,
              not_launched);
      if (read_file(result_path, report, sizeof report, &report_length) &&
          report_length != 0u) {
        (void)fwrite(report, 1u, report_length, stderr);
        (void)fputc('\n', stderr);
      }
      goto cleanup;
    }
    pass = true;
    goto cleanup;
  }
  record_ok = read_pipe_record(pipe_fds[0], record);
  check_ok = checker_accepts(checker, result_path);
  result_pass = result_is_pass(result_path, state_digest_out);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0 || !record_ok ||
      !check_ok || !result_pass) {
    uint8_t report[4097];
    size_t report_length = 0u;
    fprintf(stderr,
            "supervised fill failed: exit=%d record=%d checker=%d pass=%d\n",
            WIFEXITED(status) ? WEXITSTATUS(status) : -1, record_ok,
            check_ok, result_pass);
    if (read_file(result_path, report, sizeof report, &report_length) &&
        report_length != 0u) {
      (void)fwrite(report, 1u, report_length, stderr);
      (void)fputc('\n', stderr);
    }
    goto cleanup;
  }
  pass = true;

cleanup:
  if (pipe_fds[0] >= 0) close(pipe_fds[0]);
  if (pipe_fds[1] >= 0) close(pipe_fds[1]);
  (void)unlink(result_path);
  (void)unlink(meta_path);
  (void)unlink(capture_path);
  if (replacement_written) (void)unlink(replacement_path);
  (void)rmdir(directory);
  return pass;
}

static bool observation_baseline(const t64_video_observation *actual) {
  const uint32_t semantic_flags = T64_VIDEO_OBSERVATION_FLAG_DP_MASK |
                                  T64_VIDEO_OBSERVATION_FLAG_DP_PENDING |
                                  T64_VIDEO_OBSERVATION_FLAG_DP_CLEARED;
  if (actual->magic != T64_VIDEO_OBSERVATION_MAGIC ||
      actual->version != T64_VIDEO_OBSERVATION_VERSION ||
      actual->width != 4u || actual->height != 3u ||
      actual->stride_bytes != 16u || actual->source_address != 0x3000u ||
      actual->source_format != 2u || actual->output_encoding != 1u ||
      actual->control != 2u || actual->h_sync != 4u || actual->v_sync != 2u ||
      actual->h_start != 4u || actual->v_start != 3u ||
      actual->x_scale != 0x400u || actual->y_scale != 0x400u ||
      actual->source_bytes != T64_VIDEO_OBSERVATION_SOURCE_BYTES ||
      actual->packet_bytes != T64_VIDEO_OBSERVATION_PACKET_BYTES ||
      actual->end_tick < actual->start_tick ||
      actual->end_tick - actual->start_tick != 20u ||
      actual->commands_completed != 6u || actual->fullsync_completed != 1u ||
      actual->last_command_word != UINT64_C(0x2900000000000000) ||
      actual->last_command_byte != 0x29u || actual->last_opcode_field != 0x29u ||
      actual->fill_command_word != UINT64_C(0x37000000f80107c1) ||
      actual->fill_command_byte != 0x37u || actual->fill_opcode_field != 0x37u ||
      actual->dpc_current != 0x3130u || actual->dpc_end != 0x3130u ||
      (actual->semantic_flags & semantic_flags) != semantic_flags ||
      actual->mi_pending_before_clear != 0x20u ||
      actual->mi_mask_before_clear != 0x20u ||
      actual->mi_pending_after_clear != 0u ||
      strcmp(actual->profile_id, FILL_PROFILE_ID) != 0 ||
      strcmp(actual->vi_clock_policy, FILL_PROFILE_CLOCK_POLICY) != 0 ||
      strncmp(actual->build_id, "t64-build-v1-", 13u) != 0) {
    return false;
  }
  return true;
}

static bool write_export_file(const char *path, const uint8_t *bytes,
                              size_t length) {
  int fd;
  size_t offset = 0u;
  if (path == NULL || bytes == NULL || length == 0u) return false;
  fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
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

/* This export is test-only and can only run after the normal accepted case has
 * exercised the real supervisor, guest, bus reads, t64_video_copy and pixel
 * comparator. Public output contains no local paths or private guest data. */
static bool emit_actual_raw_export(const t64_video_observation *actual) {
  const char *directory = getenv("T64_RDP_VI_EXPORT_DIR");
  char source_path[1024];
  char packet_path[1024];
  char metadata_path[1024];
  char metadata[4096];
  int source_path_length;
  int packet_path_length;
  int metadata_path_length;
  int metadata_length;
  size_t build_id_length;
  size_t build_id_offset;
  if (directory == NULL || directory[0] == '\0') return true;
  build_id_length = strlen(actual->build_id);
  if (build_id_length != 77u ||
      memcmp(actual->build_id, "t64-build-v1-", 13u) != 0) {
    return false;
  }
  for (build_id_offset = 13u; build_id_offset < build_id_length;
       build_id_offset++) {
    char digit = actual->build_id[build_id_offset];
    if (!((digit >= '0' && digit <= '9') ||
          (digit >= 'a' && digit <= 'f'))) {
      return false;
    }
  }
  source_path_length = snprintf(source_path, sizeof source_path,
                                "%s/rdram.bin", directory);
  packet_path_length = snprintf(packet_path, sizeof packet_path,
                                "%s/vi-packet.bin", directory);
  metadata_path_length = snprintf(metadata_path, sizeof metadata_path,
                                  "%s/observation.json", directory);
  if (source_path_length <= 0 || (size_t)source_path_length >= sizeof source_path ||
      packet_path_length <= 0 || (size_t)packet_path_length >= sizeof packet_path ||
      metadata_path_length <= 0 || (size_t)metadata_path_length >= sizeof metadata_path) {
    return false;
  }
  metadata_length = snprintf(
      metadata, sizeof metadata,
      "{\"schema\":\"t64.rdp-vi-observation\",\"schema_version\":1,"
      "\"fixture_id\":\"%s\",\"fixture_sha256\":\"%s\","
      "\"input_sha256\":\"%s\",\"profile_id\":\"%s\","
      "\"profile_sha256\":\"%s\",\"build_id\":\"%s\","
      "\"vi_clock_policy\":\"%s\",\"width\":%u,\"height\":%u,"
      "\"stride_bytes\":%u,\"source_address\":%u,\"source_format\":%u,"
      "\"output_encoding\":%u,\"field\":%u,\"parity\":%u,"
      "\"control\":%u,\"burst\":%u,\"h_sync\":%u,\"v_sync\":%u,"
      "\"h_start\":%u,\"v_start\":%u,\"x_scale\":%u,\"y_scale\":%u,"
      "\"source_bytes\":%u,\"packet_bytes\":%u,\"start_tick\":%llu,"
      "\"end_tick\":%llu,\"sequence\":%llu,\"commands_completed\":%llu,"
      "\"fullsync_completed\":%llu,\"last_command_word\":%llu,"
      "\"fill_command_word\":%llu,\"last_command_byte\":%u,"
      "\"last_opcode_field\":%u,\"fill_command_byte\":%u,"
      "\"fill_opcode_field\":%u,\"dpc_current\":%u,\"dpc_end\":%u,"
      "\"semantic_flags\":%u,\"mi_pending_before_clear\":%u,"
      "\"mi_mask_before_clear\":%u,\"mi_pending_after_clear\":%u}\n",
      FILL_FIXTURE_ID, FILL_FIXTURE_SHA256, INPUT_SHA256,
      actual->profile_id, FILL_PROFILE_SHA256, actual->build_id,
      actual->vi_clock_policy, actual->width, actual->height,
      actual->stride_bytes, actual->source_address, actual->source_format,
      actual->output_encoding, actual->field, actual->parity, actual->control,
      actual->burst, actual->h_sync, actual->v_sync, actual->h_start,
      actual->v_start, actual->x_scale, actual->y_scale,
      actual->source_bytes, actual->packet_bytes,
      (unsigned long long)actual->start_tick,
      (unsigned long long)actual->end_tick,
      (unsigned long long)actual->sequence,
      (unsigned long long)actual->commands_completed,
      (unsigned long long)actual->fullsync_completed,
      (unsigned long long)actual->last_command_word,
      (unsigned long long)actual->fill_command_word,
      actual->last_command_byte, actual->last_opcode_field,
      actual->fill_command_byte, actual->fill_opcode_field, actual->dpc_current,
      actual->dpc_end, actual->semantic_flags,
      actual->mi_pending_before_clear, actual->mi_mask_before_clear,
      actual->mi_pending_after_clear);
  if (metadata_length <= 0 || (size_t)metadata_length >= sizeof metadata ||
      !write_export_file(source_path, actual->source, actual->source_bytes) ||
      !write_export_file(packet_path, actual->packet, actual->packet_bytes) ||
      !write_export_file(metadata_path, (const uint8_t *)metadata,
                         (size_t)metadata_length)) {
    return false;
  }
  return true;
}

static int run_case(test_case_id id, char *supervisor,
                    char *checker, char *fixture, char *input) {
  t64_video_observation actual = {0};
  uint8_t expected[T64_VIDEO_OBSERVATION_PACKET_BYTES];
  uint32_t i;
  if (id == TEST_REPLACED_FIXTURE) {
    if (!run_supervised(supervisor, checker, fixture, input, true, 0u,
                        NULL, &actual)) {
      fprintf(stderr, "replacement fixture was accepted or reached the runner\n");
      return 1;
    }
    printf("replacement fixture refused before guest runner launch\n");
    return 0;
  }
  if (id == TEST_DRAIN_CADENCE) {
    t64_video_observation coarse = {0};
    t64_video_observation fine = {0};
    char coarse_digest[65] = {0};
    char fine_digest[65] = {0};
    if (!run_supervised(supervisor, checker, fixture, input, false, 16u,
                        coarse_digest, &coarse) ||
        !run_supervised(supervisor, checker, fixture, input, false, 32u,
                        fine_digest, &fine) ||
        !observation_baseline(&coarse) || !observation_baseline(&fine) ||
        memcmp(&coarse, &fine, sizeof coarse) != 0 ||
        strcmp(coarse_digest, fine_digest) != 0) {
      fprintf(stderr,
              "actual runner drain cadence changed packet bytes, metadata, or final state digest\n");
      return 1;
    }
    printf("actual supervised runner: 16-tick and 32-tick advances produced identical packet record and state digest %s\n",
           coarse_digest);
    return 0;
  }
  if (!run_supervised(supervisor, checker, fixture, input, false, 0u,
                      NULL, &actual) ||
      !observation_baseline(&actual)) {
    fprintf(stderr,
            "accepted fill setup/semantics failed: magic=%08x size=%ux%u stride=%u source=%u packet=%u tick=%llu..%llu sync=%u/%u start=%u/%u scale=%x/%x fmt=%u/%u commands=%llu fullsync=%llu mi=%x/%x/%x dpc=%x/%x fill=%016llx/%02x/%x\n",
            actual.magic, actual.width, actual.height, actual.stride_bytes,
            actual.source_bytes, actual.packet_bytes,
            (unsigned long long)actual.start_tick,
            (unsigned long long)actual.end_tick, actual.h_sync,
            actual.v_sync, actual.h_start, actual.v_start, actual.x_scale,
            actual.y_scale, actual.source_format, actual.output_encoding,
            (unsigned long long)actual.commands_completed,
            (unsigned long long)actual.fullsync_completed,
            actual.mi_pending_before_clear, actual.mi_mask_before_clear,
            actual.mi_pending_after_clear, actual.dpc_current, actual.dpc_end,
            (unsigned long long)actual.fill_command_word,
            actual.fill_command_byte, actual.fill_opcode_field);
    return 1;
  }
  for (i = 0u; i < T64_VIDEO_OBSERVATION_SOURCE_BYTES; i++) {
    if (actual.source[i] != expected_source[i]) {
      fprintf(stderr,
              "RDRAM first difference at address %08x: expected %02x actual %02x\n",
              0x3000u + i, expected_source[i], actual.source[i]);
      return 1;
    }
  }
  memcpy(expected, expected_packet, sizeof expected);
  if (id == TEST_ACCEPTED_FILL) {
    for (i = 0u; i < sizeof expected; i++) {
      if (actual.packet[i] != expected[i]) {
        fprintf(stderr,
                "raw VI first difference at byte %u (x=%u y=%u channel=%u): expected %02x actual %02x\n",
                i, (i / 4u) % 4u, i / 16u, i % 4u, expected[i],
                actual.packet[i]);
        return 1;
      }
    }
    puts("accepted raw pixel comparator reached: authored bytes match t64_video_copy output");
    if (!emit_actual_raw_export(&actual)) {
      fprintf(stderr, "could not export actual raw RDRAM/VI observation\n");
      return 1;
    }
    return 0;
  }

  /* The negative control changes only one independently-authored expected
   * output literal, after the same fixture has passed all setup assertions. */
  expected[0] = 254u;
  for (i = 0u; i < sizeof expected; i++) {
    if (actual.packet[i] != expected[i]) {
      if (i != 0u || actual.packet[i] != 255u || expected[i] != 254u) {
        fprintf(stderr, "wrong-pixel control diverged at unexpected byte %u\n",
                i);
        return 1;
      }
      printf("wrong-pixel comparator reached at (x=0,y=0,channel=0): expected fe actual ff\n");
      return 0;
    }
  }
  fprintf(stderr, "wrong-pixel comparator did not detect the changed literal\n");
  return 1;
}

int main(int argc, char **argv) {
  char *selector = NULL;
  char *supervisor = NULL;
  char *checker = NULL;
  char *fixture = NULL;
  char *input = NULL;
  test_case_id selected = TEST_ACCEPTED_FILL;
  size_t i;
  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
      puts(cases[i].name);
    }
    return 0;
  }
  if (argc == 6) {
    selector = argv[1];
    supervisor = argv[2];
    checker = argv[3];
    fixture = argv[4];
    input = argv[5];
  } else if (argc == 5) {
    supervisor = argv[1];
    checker = argv[2];
    fixture = argv[3];
    input = argv[4];
  } else {
    return 2;
  }
  if (selector != NULL) {
    bool found = false;
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
      if (strcmp(selector, cases[i].name) == 0) {
        selected = cases[i].id;
        found = true;
        break;
      }
    }
    if (!found) return 2;
  }
  return run_case(selected, supervisor, checker, fixture, input);
}
