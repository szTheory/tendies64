#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "t64_sha256.h"
#include "t64_verified_input.h"
#include "t64_json.h"

#define FIXTURE_SIZE 4280u
#define INPUT_SIZE 54u
#define FIXTURE_SHA256 \
  "fa276cc175a4e764b6fd5413193c44b51f38fd8135e33e400032827e02f27c7c"
#define INPUT_SHA256 \
  "6d5593bc2d95465b44ec7ce7785e5ff7d7d53605de9b039a021bc697d226186d"
#define INPUT_CANONICAL_SHA256 INPUT_SHA256
#define LOG_SHA256 \
  "86594505bea53c9843f9eaf07e4cf0121cbe0055670324674fd8385c88ffd18e"
#define PROFILE_SHA256 \
  "a171241f68c377f7c324a995a08ff8bd8da60bbe70df14193d8f4b8ae7e9769f"

static unsigned checks;
static unsigned failures;

static void check(bool pass, const char *description) {
  checks++;
  printf("%s %u - %s\n", pass ? "ok" : "not ok", checks, description);
  if (!pass) failures++;
}

static void hash_file(const char *path, uint64_t size,
                      char output[T64_VERIFIED_INPUT_SHA256_HEX_SIZE]) {
  uint8_t buffer[65536];
  t64_sha256_ctx hash;
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  int fd = open(path, O_RDONLY);
  size_t total = 0u;
  if (fd < 0 || size > sizeof buffer) {
    output[0] = '\0';
    if (fd >= 0) close(fd);
    return;
  }
  while (total < (size_t)size) {
    ssize_t count = read(fd, buffer + total, (size_t)size - total);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) {
      output[0] = '\0';
      close(fd);
      return;
    }
    total += (size_t)count;
  }
  close(fd);
  t64_sha256_init(&hash);
  t64_sha256_update(&hash, buffer, total);
  t64_sha256_final(&hash, digest);
  t64_sha256_hex(digest, output);
}

static bool clear_cloexec(int fd) {
  int flags = fcntl(fd, F_GETFD);
  return flags >= 0 && fcntl(fd, F_SETFD, flags & ~FD_CLOEXEC) == 0;
}

static bool spawn_runner(char *runner, char *report_path, char *capture_path,
                         t64_verified_input *fixture,
                         t64_verified_input *input, uint64_t guest_ticks,
                         int *status_out) {
  char fixture_option[] = "--fixture-fd";
  char input_option[] = "--input-fd";
  char profile_option[] = "--profile";
  char profile_id[] = "ntsc-cart-4m-openipl3-warm-v1";
  char ticks_option[] = "--guest-ticks";
  char ticks[24];
  char fixture_fd[24];
  char input_fd[24];
  pid_t pid;
  int report_fd;
  int capture_fd;
  char *const args[] = {runner,
                        fixture_option,
                        fixture_fd,
                        input_option,
                        input_fd,
                        profile_option,
                        profile_id,
                        ticks_option,
                        ticks,
                        NULL};

  (void)snprintf(fixture_fd, sizeof fixture_fd, "%d", fixture->fd);
  (void)snprintf(input_fd, sizeof input_fd, "%d", input->fd);
  (void)snprintf(ticks, sizeof ticks, "%llu",
                 (unsigned long long)guest_ticks);
  if (lseek(fixture->fd, 0, SEEK_SET) != 0 ||
      lseek(input->fd, 0, SEEK_SET) != 0) {
    return false;
  }
  report_fd = open(report_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  capture_fd = open(capture_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (report_fd < 0 || capture_fd < 0) {
    if (report_fd >= 0) close(report_fd);
    if (capture_fd >= 0) close(capture_fd);
    return false;
  }
  pid = fork();
  if (pid == 0) {
    if (!clear_cloexec(fixture->fd) || !clear_cloexec(input->fd) ||
        dup2(report_fd, STDOUT_FILENO) < 0 ||
        dup2(capture_fd, STDERR_FILENO) < 0) {
      _exit(126);
    }
    close(report_fd);
    close(capture_fd);
    execv(runner, args);
    _exit(127);
  }
  close(report_fd);
  close(capture_fd);
  if (pid < 0) return false;
  while (waitpid(pid, status_out, 0) < 0) {
    if (errno == EINTR) continue;
    return false;
  }
  return true;
}

static bool spawn_supervisor_real(char *supervisor, char *result_path,
                                  char *meta_path, char *work_dir,
                                  char *capture_path, char *fixture_path,
                                  char *input_path, int *status_out) {
  char real_option[] = "--real";
  char result_option[] = "--result";
  char meta_option[] = "--meta";
  char work_option[] = "--work-dir";
  char wall_option[] = "--wall-ms";
  char wall_ms[] = "30000";
  char fixture_option[] = "--fixture";
  char input_option[] = "--input";
  char capture_option[] = "--capture-out";
  char *const args[] = {supervisor, real_option, result_option, result_path,
                        meta_option, meta_path, work_option, work_dir,
                        wall_option, wall_ms, fixture_option, fixture_path,
                        input_option, input_path, capture_option, capture_path,
                        NULL};
  pid_t pid = fork();
  if (pid == 0) {
    execv(supervisor, args);
    _exit(127);
  }
  if (pid < 0) return false;
  while (waitpid(pid, status_out, 0) < 0) {
    if (errno == EINTR) continue;
    return false;
  }
  return true;
}

static bool read_file(const char *path, uint8_t *bytes, size_t capacity,
                      size_t *length_out) {
  int fd = open(path, O_RDONLY);
  size_t total = 0u;
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
  close(fd);
  *length_out = total;
  return true;
}

static bool checker_accepts(char *checker, char *report_path) {
  pid_t pid = fork();
  int status;
  if (pid == 0) {
    char mode[] = "result";
    char *const args[] = {checker, mode, report_path, NULL};
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

static bool contains(const char *text, const char *needle) {
  return strstr(text, needle) != NULL;
}

static bool report_string_equals(const uint8_t *bytes, size_t length,
                                 const char *parent_key, const char *key,
                                 const char *expected) {
  t64_json_doc document = {0};
  const t64_json_node *container;
  const t64_json_node *value;
  size_t expected_length = strlen(expected);
  bool equal;
  if (t64_json_parse(bytes, length, NULL, &document, NULL) != 0) {
    return false;
  }
  container = parent_key == NULL ? document.root
                                 : t64_json_get(document.root, parent_key);
  value = container == NULL ? NULL : t64_json_get(container, key);
  equal = value != NULL && value->type == T64_JSON_STRING &&
          value->len == expected_length &&
          memcmp(value->str, expected, expected_length) == 0;
  t64_json_free(&document);
  return equal;
}

static bool report_field_is_null(const uint8_t *bytes, size_t length,
                                 const char *key) {
  t64_json_doc document = {0};
  const t64_json_node *value;
  bool is_null;
  if (t64_json_parse(bytes, length, NULL, &document, NULL) != 0) {
    return false;
  }
  value = t64_json_get(document.root, key);
  is_null = value != NULL && value->type == T64_JSON_NULL;
  t64_json_free(&document);
  return is_null;
}

int main(int argc, char **argv) {
  static const uint8_t expected_log[] = "Tendies64 hello via open IPL3\n";
  char *fixture_path = getenv("T64_TEST_HELLO_PATH");
  char *input_path = getenv("T64_TEST_INPUT_PATH");
  char fixture_hash[T64_VERIFIED_INPUT_SHA256_HEX_SIZE];
  char input_hash[T64_VERIFIED_INPUT_SHA256_HEX_SIZE];
  char directory[] = "/tmp/t64-real-runner.XXXXXX";
  char report_path[256];
  char capture_path[256];
  char bounded_report_path[256];
  char bounded_capture_path[256];
  char invalid_input_path[256];
  char invalid_report_path[256];
  char invalid_capture_path[256];
  char supervisor_report_path[256];
  char supervisor_meta_path[256];
  char supervisor_capture_path[256];
  char report[4097] = {0};
  uint8_t capture[128];
  char invalid_schedule[] =
      "{\"events\":[],\"schema\":\"t64.input\",\"schema_version\":2}";
  size_t report_length = 0u;
  size_t capture_length = 0u;
  t64_verified_input fixture = {-1, 0u, {0u}, {0}, 0u};
  t64_verified_input input = {-1, 0u, {0u}, {0}, 0u};
  t64_verified_input invalid_input = {-1, 0u, {0u}, {0}, 0u};
  char invalid_input_hash[T64_VERIFIED_INPUT_SHA256_HEX_SIZE];
  int runner_status = 0;
  bool setup_ok = argc == 4 && fixture_path != NULL && input_path != NULL;
  bool spawned = false;

  (void)printf("TAP version 13\n");
  if (setup_ok) {
    hash_file(fixture_path, FIXTURE_SIZE, fixture_hash);
    hash_file(input_path, INPUT_SIZE, input_hash);
    setup_ok = strcmp(fixture_hash, FIXTURE_SHA256) == 0 &&
               strcmp(input_hash, INPUT_SHA256) == 0 &&
               mkdtemp(directory) != NULL;
  }
  check(setup_ok, "uses the pinned generated fixture and canonical schedule");
  if (setup_ok) {
    (void)snprintf(report_path, sizeof report_path, "%s/result.json", directory);
    (void)snprintf(capture_path, sizeof capture_path, "%s/guest.log", directory);
    (void)snprintf(bounded_report_path, sizeof bounded_report_path,
                   "%s/bounded.json", directory);
    (void)snprintf(bounded_capture_path, sizeof bounded_capture_path,
                   "%s/bounded.log", directory);
    (void)snprintf(invalid_input_path, sizeof invalid_input_path,
                   "%s/invalid.input.json", directory);
    (void)snprintf(invalid_report_path, sizeof invalid_report_path,
                   "%s/invalid.json", directory);
    (void)snprintf(invalid_capture_path, sizeof invalid_capture_path,
                   "%s/invalid.log", directory);
    (void)snprintf(supervisor_report_path, sizeof supervisor_report_path,
                   "%s/supervisor.json", directory);
    (void)snprintf(supervisor_meta_path, sizeof supervisor_meta_path,
                   "%s/supervisor.meta", directory);
    (void)snprintf(supervisor_capture_path, sizeof supervisor_capture_path,
                   "%s/supervisor.log", directory);
    setup_ok = t64_verified_input_prepare(
                   fixture_path, FIXTURE_SIZE, FIXTURE_SHA256,
                   T64_VERIFIED_FIXTURE_MAX_BYTES, &fixture) ==
                   T64_VERIFIED_INPUT_OK &&
               t64_verified_input_prepare(
                   input_path, INPUT_SIZE, INPUT_SHA256,
                   T64_VERIFIED_SCHEDULE_MAX_BYTES, &input) ==
                   T64_VERIFIED_INPUT_OK;
  }
  check(setup_ok, "prepares verified unlinked read-only runner descriptors");
  if (setup_ok) {
    spawned = spawn_runner(argv[1], report_path, capture_path, &fixture, &input,
                           UINT64_C(530500), &runner_status);
  }
  check(spawned && WIFEXITED(runner_status) && WEXITSTATUS(runner_status) == 0,
        "actual producer exits successfully after guest completion");
  if (spawned) {
    (void)read_file(capture_path, capture, sizeof capture, &capture_length);
    (void)read_file(report_path, (uint8_t *)report, sizeof report - 1u,
                    &report_length);
    report[report_length] = '\0';
  }
  check(capture_length == sizeof expected_log - 1u &&
            memcmp(capture, expected_log, sizeof expected_log - 1u) == 0,
        "captures the exact authored guest transcript");
  check(spawned && checker_accepts(argv[2], report_path),
        "strict real-result checker accepts the producer report");
  check(spawned && report_string_equals((const uint8_t *)report,
                                        report_length, NULL, "outcome", "pass"),
        "parsed real-result outcome is pass");
  check(contains(report, "\"fixture_sha256\":\"" FIXTURE_SHA256 "\"") &&
            contains(report, "\"input_digest\":\"" INPUT_CANONICAL_SHA256 "\"") &&
            contains(report, "\"log_sha256\":\"" LOG_SHA256 "\"") &&
            contains(report, "\"profile_digest\":\"" PROFILE_SHA256 "\"") &&
            contains(report, "\"guest_tick\":\"530438\"") &&
            contains(report, "\"instruction_count\":\"530437\"") &&
            contains(report, "\"kind\":\"hello-completion\"") &&
            contains(report, "\"kind\":\"loader-dma\"") &&
            contains(report, "\"kind\":\"ipl3-entry\"") &&
            contains(report, "\"kind\":\"hello-entry\""),
        "report binds descriptor identities, real milestones and stop progress");

  {
    bool bounded_started = spawn_runner(
        argv[1], bounded_report_path, bounded_capture_path, &fixture, &input,
        UINT64_C(100), &runner_status);
    uint8_t bounded_report[4097] = {0};
    uint8_t bounded_capture[128];
    size_t bounded_report_length = 0u;
    size_t bounded_capture_length = 0u;
    bool bounded_files_read = bounded_started &&
        read_file(bounded_report_path, bounded_report,
                  sizeof bounded_report - 1u, &bounded_report_length) &&
        read_file(bounded_capture_path, bounded_capture,
                  sizeof bounded_capture, &bounded_capture_length);
    check(bounded_started && WIFEXITED(runner_status) &&
              WEXITSTATUS(runner_status) == 0,
          "bounded producer exits without claiming guest completion");
    check(bounded_files_read && checker_accepts(argv[2], bounded_report_path) &&
              report_string_equals(bounded_report, bounded_report_length,
                                   NULL, "outcome", "boot-fail") &&
              report_string_equals(bounded_report, bounded_report_length,
                                   NULL, "detail", "empty-output") &&
              report_string_equals(bounded_report, bounded_report_length,
                                   "guest_stop", "kind", "guest-bound") &&
              report_string_equals(bounded_report, bounded_report_length,
                                   NULL, "guest_tick", "100") &&
              bounded_capture_length == 0u,
          "bounded execution validates as timeout with no guest output or pass");
  }

  if (setup_ok) {
    int invalid_fd = open(invalid_input_path,
                          O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (invalid_fd >= 0) {
      size_t length = sizeof invalid_schedule - 1u;
      size_t offset = 0u;
      while (offset < length) {
        ssize_t count = write(invalid_fd, invalid_schedule + offset,
                              length - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        offset += (size_t)count;
      }
      if (close(invalid_fd) != 0 || offset != length) setup_ok = false;
    } else {
      setup_ok = false;
    }
  }
  if (setup_ok) {
    hash_file(invalid_input_path, sizeof invalid_schedule - 1u,
              invalid_input_hash);
    setup_ok = invalid_input_hash[0] != '\0' &&
        t64_verified_input_prepare(
            invalid_input_path, sizeof invalid_schedule - 1u,
            invalid_input_hash, T64_VERIFIED_SCHEDULE_MAX_BYTES,
            &invalid_input) == T64_VERIFIED_INPUT_OK;
  }
  check(setup_ok, "prepares a separately hashed malformed input descriptor");
  if (setup_ok) {
    bool invalid_started = spawn_runner(
        argv[1], invalid_report_path, invalid_capture_path, &fixture,
        &invalid_input, UINT64_C(530500), &runner_status);
    uint8_t invalid_report[4097] = {0};
    uint8_t invalid_capture[128];
    size_t invalid_report_length = 0u;
    size_t invalid_capture_length = 0u;
    bool invalid_files_read = invalid_started &&
        read_file(invalid_report_path, invalid_report,
                  sizeof invalid_report - 1u, &invalid_report_length) &&
        read_file(invalid_capture_path, invalid_capture,
                  sizeof invalid_capture, &invalid_capture_length);
    check(invalid_files_read && WIFEXITED(runner_status) &&
              WEXITSTATUS(runner_status) == 0 &&
              checker_accepts(argv[2], invalid_report_path) &&
              report_string_equals(invalid_report, invalid_report_length,
                                   NULL, "outcome", "harness-error") &&
              report_string_equals(invalid_report, invalid_report_length,
                                   "guest_stop", "kind", "refused") &&
              report_field_is_null(invalid_report, invalid_report_length,
                                   "input_digest") &&
              invalid_capture_length == 0u,
          "malformed canonical schedule refuses before launch and cannot pass");
  }

  {
    int supervisor_status = 0;
    bool supervisor_started = setup_ok && spawn_supervisor_real(
        argv[3], supervisor_report_path, supervisor_meta_path, directory,
        supervisor_capture_path, fixture_path, input_path,
        &supervisor_status);
    uint8_t supervisor_report[4097] = {0};
    uint8_t supervisor_capture[128];
    size_t supervisor_report_length = 0u;
    size_t supervisor_capture_length = 0u;
    bool supervisor_files_read = supervisor_started &&
        read_file(supervisor_report_path, supervisor_report,
                  sizeof supervisor_report - 1u, &supervisor_report_length) &&
        read_file(supervisor_capture_path, supervisor_capture,
                  sizeof supervisor_capture, &supervisor_capture_length);
    check(supervisor_files_read && WIFEXITED(supervisor_status) &&
              WEXITSTATUS(supervisor_status) == 0 &&
              checker_accepts(argv[2], supervisor_report_path) &&
              report_string_equals(supervisor_report,
                                   supervisor_report_length, NULL, "outcome",
                                   "pass") &&
              supervisor_capture_length == sizeof expected_log - 1u &&
              memcmp(supervisor_capture, expected_log,
                     sizeof expected_log - 1u) == 0,
          "real supervisor emits a checked v2 pass and exact captured guest bytes");
  }

  t64_verified_input_release(&fixture);
  t64_verified_input_release(&input);
  t64_verified_input_release(&invalid_input);
  if (setup_ok && failures == 0u) {
    (void)unlink(report_path);
    (void)unlink(capture_path);
    (void)unlink(bounded_report_path);
    (void)unlink(bounded_capture_path);
    (void)unlink(invalid_input_path);
    (void)unlink(invalid_report_path);
    (void)unlink(invalid_capture_path);
    (void)unlink(supervisor_report_path);
    (void)unlink(supervisor_meta_path);
    (void)unlink(supervisor_capture_path);
    (void)rmdir(directory);
  } else if (setup_ok) {
    fprintf(stderr, "local failure artifacts: %s\n", directory);
  }
  printf("1..%u\n", checks);
  return failures == 0u ? 0 : 1;
}
