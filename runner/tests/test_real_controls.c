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

typedef struct {
  const char *fixture_name;
  const char *fixture_id;
  const char *fixture_sha256;
  const char *outcome;
  const char *detail;
  const char *stop_kind;
  const uint8_t *expected_log;
  size_t expected_log_length;
} control_case;

static const uint8_t empty_log[] = "";

static const control_case empty_halt = {
    "hello-empty-break.z64",
    "hello-empty-break",
    "5ae986bb263cafa28e2b06a17e3d3f3ee229c55f75cd1ab91df7a9698e35a652",
    "boot-fail",
    "empty-output",
    "authored-completion",
    empty_log,
    0u};

static bool read_bounded(const char *path, uint8_t *bytes, size_t capacity,
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
      count = read(fd, &extra, sizeof extra);
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

static bool field_string_equals(const t64_json_node *object,
                                const char *key, const char *expected) {
  const t64_json_node *value = t64_json_get(object, key);
  size_t length;
  if (value == NULL || value->type != T64_JSON_STRING || expected == NULL) {
    return false;
  }
  length = strlen(expected);
  return value->len == length && memcmp(value->str, expected, length) == 0;
}

static bool field_u64_equals(const t64_json_node *object, const char *key,
                             uint64_t expected) {
  const t64_json_node *value = t64_json_get(object, key);
  uint64_t actual = 0u;
  size_t i;
  if (value == NULL || value->type != T64_JSON_STRING || value->len == 0u) {
    return false;
  }
  for (i = 0u; i < value->len; i++) {
    unsigned digit;
    if (value->str[i] < '0' || value->str[i] > '9') return false;
    digit = (unsigned)(value->str[i] - '0');
    if (actual > (UINT64_MAX - digit) / UINT64_C(10)) return false;
    actual = actual * UINT64_C(10) + digit;
  }
  return actual == expected;
}

static bool checker_accepts(const char *checker, const char *result_path) {
  char checker_arg[512];
  char result_arg[512];
  pid_t pid = fork();
  int status = 0;
  int n = snprintf(checker_arg, sizeof checker_arg, "%s", checker);
  if (n <= 0 || (size_t)n >= sizeof checker_arg) return false;
  n = snprintf(result_arg, sizeof result_arg, "%s", result_path);
  if (n <= 0 || (size_t)n >= sizeof result_arg) return false;
  if (pid == 0) {
    char mode[] = "result";
    char *const args[] = {checker_arg, mode, result_arg, NULL};
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

static bool run_guest_case(const char *supervisor, const char *checker,
                           const char *fixture_path, const char *input_path,
                           const control_case *test_case,
                           char observed[192]) {
  char directory[] = "/tmp/t64-real-controls.XXXXXX";
  char result_path[512] = {0};
  char meta_path[512] = {0};
  char work_dir[512] = {0};
  char capture_path[512] = {0};
  char supervisor_arg[512];
  char fixture_arg[1024];
  char input_arg[1024];
  uint8_t result_bytes[4097];
  uint8_t capture_bytes[8192];
  size_t result_length = 0u;
  size_t capture_length = 0u;
  int result_code;
  int status = 0;
  pid_t pid;
  t64_json_doc document = {0};
  const t64_json_node *root;
  const t64_json_node *stop;
  const t64_json_node *counts;
  bool passed = false;
  int n;

  if (mkdtemp(directory) == NULL) return false;
  n = snprintf(result_path, sizeof result_path, "%s/result.json", directory);
  if (n <= 0 || (size_t)n >= sizeof result_path) goto cleanup;
  n = snprintf(meta_path, sizeof meta_path, "%s/run.meta", directory);
  if (n <= 0 || (size_t)n >= sizeof meta_path) goto cleanup;
  n = snprintf(work_dir, sizeof work_dir, "%s", directory);
  if (n <= 0 || (size_t)n >= sizeof work_dir) goto cleanup;
  n = snprintf(capture_path, sizeof capture_path, "%s/capture.bin", directory);
  if (n <= 0 || (size_t)n >= sizeof capture_path) goto cleanup;
  n = snprintf(supervisor_arg, sizeof supervisor_arg, "%s", supervisor);
  if (n <= 0 || (size_t)n >= sizeof supervisor_arg) goto cleanup;
  n = snprintf(fixture_arg, sizeof fixture_arg, "%s", fixture_path);
  if (n <= 0 || (size_t)n >= sizeof fixture_arg) goto cleanup;
  n = snprintf(input_arg, sizeof input_arg, "%s", input_path);
  if (n <= 0 || (size_t)n >= sizeof input_arg) goto cleanup;

  pid = fork();
  if (pid == 0) {
    char real_option[] = "--real";
    char result_option[] = "--result";
    char meta_option[] = "--meta";
    char work_option[] = "--work-dir";
    char wall_option[] = "--wall-ms";
    char wall_ms[] = "30000";
    char fixture_option[] = "--fixture";
    char input_option[] = "--input";
    char capture_option[] = "--capture-out";
    char *const args[] = {
        supervisor_arg, real_option, result_option, result_path,
        meta_option, meta_path, work_option, work_dir, wall_option, wall_ms,
        fixture_option, fixture_arg, input_option, input_arg,
        capture_option, capture_path, NULL};
    execv(supervisor_arg, args);
    _exit(127);
  }
  if (pid < 0) goto cleanup;
  while (waitpid(pid, &status, 0) < 0) {
    if (errno == EINTR) continue;
    goto cleanup;
  }
  result_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  if ((result_code != 0 && result_code != 1) ||
      !read_bounded(result_path, result_bytes, sizeof result_bytes,
                    &result_length) ||
      !checker_accepts(checker, result_path) ||
      t64_json_parse(result_bytes, result_length, NULL, &document, NULL) != 0) {
    goto cleanup;
  }
  root = document.root;
  stop = t64_json_get(root, "guest_stop");
  counts = t64_json_get(root, "counts");
  if (!read_bounded(capture_path, capture_bytes, sizeof capture_bytes,
                    &capture_length)) {
    capture_length = 0u;
  }
  passed = result_code == 0 &&
           field_string_equals(root, "fixture_id", test_case->fixture_id) &&
           field_string_equals(root, "fixture_sha256",
                               test_case->fixture_sha256) &&
           field_string_equals(root, "outcome", test_case->outcome) &&
           field_string_equals(root, "detail", test_case->detail) &&
           field_string_equals(stop, "kind", test_case->stop_kind) &&
           field_u64_equals(counts, "log_bytes",
                            (uint64_t)test_case->expected_log_length) &&
           field_u64_equals(counts, "log_bytes_total",
                            (uint64_t)test_case->expected_log_length) &&
           capture_length == test_case->expected_log_length &&
           (capture_length == 0u ||
            memcmp(capture_bytes, test_case->expected_log,
                   capture_length) == 0);
  if (observed != NULL) {
    const t64_json_node *outcome = t64_json_get(root, "outcome");
    const t64_json_node *detail = t64_json_get(root, "detail");
    const t64_json_node *kind = stop == NULL ? NULL : t64_json_get(stop, "kind");
    const char *outcome_text = outcome != NULL && outcome->type == T64_JSON_STRING
                                   ? outcome->str : "invalid";
    const char *detail_text = detail != NULL && detail->type == T64_JSON_STRING
                                  ? detail->str : "invalid";
    const char *kind_text = kind != NULL && kind->type == T64_JSON_STRING
                                ? kind->str : "invalid";
    (void)snprintf(observed, 192u, "outcome=%s detail=%s stop=%s exit=%d",
                   outcome_text, detail_text, kind_text, result_code);
  }

cleanup:
  t64_json_free(&document);
  if (result_path[0] != '\0') (void)unlink(result_path);
  if (meta_path[0] != '\0') (void)unlink(meta_path);
  if (capture_path[0] != '\0') (void)unlink(capture_path);
  (void)rmdir(directory);
  return passed;
}

int main(int argc, char **argv) {
  char fixture_path[1024];
  char observed[192] = "no valid report";
  const char *fixture_dir = getenv("T64_TEST_FIXTURE_DIR");
  const char *input_path = getenv("T64_TEST_INPUT_PATH");
  int n;
  bool passed;

  (void)printf("TAP version 13\n1..1\n");
  if (argc != 4 || fixture_dir == NULL || input_path == NULL ||
      strcmp(argv[3], "empty-halt") != 0) {
    (void)printf("not ok 1 - empty BREAK control setup\n");
    return 1;
  }
  n = snprintf(fixture_path, sizeof fixture_path, "%s/%s", fixture_dir,
               empty_halt.fixture_name);
  if (n <= 0 || (size_t)n >= sizeof fixture_path) {
    (void)printf("not ok 1 - empty BREAK control setup\n");
    return 1;
  }
  passed = run_guest_case(argv[1], argv[2], fixture_path, input_path,
                          &empty_halt, observed);
  if (!passed) (void)printf("# observed %s\n", observed);
  (void)printf("%s 1 - empty BREAK returns boot-fail/empty-output after real IPL3 execution\n",
               passed ? "ok" : "not ok");
  return passed ? 0 : 1;
}
