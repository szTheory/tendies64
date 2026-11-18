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
#include <sys/stat.h>
#include <unistd.h>

#include "t64_audio_observation.h"
#include "t64_json.h"
#include "t64_observation_pipe.h"
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

static const uint8_t expected_pcm[32] = {
    0x00u, 0x00u, 0x7fu, 0xffu, 0x80u, 0x00u, 0xffu, 0xffu,
    0x12u, 0x34u, 0xedu, 0xccu, 0x00u, 0x01u, 0xffu, 0xfeu,
    0x80u, 0x00u, 0x7fu, 0xffu, 0xffu, 0xffu, 0x00u, 0x00u,
    0x55u, 0xaau, 0xaau, 0x55u, 0x00u, 0x00u, 0x80u, 0x00u};

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
                           const audio_run_case *test) {
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
  t64_json_free(&doc);
  return pass;
}

static bool run_supervised(char *supervisor, char *checker, char *fixture,
                           char *input, const audio_run_case *test,
                           t64_video_observation *video,
                           t64_audio_observation *audio) {
  char directory[4096];
  char result_path[512] = {0};
  char meta_path[512] = {0};
  char capture_path[512] = {0};
  char work_dir[512] = {0};
  char video_fd_text[24];
  char audio_fd_text[24];
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
  length = snprintf(work_dir, sizeof work_dir, "%s", directory);
  if (length <= 0 || (size_t)length >= sizeof work_dir) goto cleanup;
  (void)snprintf(video_fd_text, sizeof video_fd_text, "%d", video_pipe[1]);
  (void)snprintf(audio_fd_text, sizeof audio_fd_text, "%d", audio_pipe[1]);

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
    char video_option[] = "--video-observation-fd";
    char audio_option[] = "--audio-observation-fd";
    char input_sha[] = INPUT_SHA256;
    char fixture_id[64];
    char fixture_sha256[65];
    char *args[48];
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
    args[count++] = video_option;
    args[count++] = video_fd_text;
    args[count++] = audio_option;
    args[count++] = audio_fd_text;
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
  result_ok = result_is_pass(result_path, test);
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
                      &audio) ||
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

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    puts("runner-literal-pcm");
    puts("reached-wrong-sample");
    return 0;
  }
  if (argc != 6 ||
      (strcmp(argv[1], "runner-literal-pcm") != 0 &&
       strcmp(argv[1], "reached-wrong-sample") != 0)) {
    fprintf(stderr,
            "usage: %s runner-literal-pcm|reached-wrong-sample <supervisor> <checker> <fixture> <input>\n",
            argv[0]);
    return 2;
  }
  return run_audio_case(argv[1], argv[2], argv[3], argv[4], argv[5]);
}
