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
#include "tendies64/t64_machine.h"

typedef struct {
  const char *name;
  const char *fixture_name;
  const char *fixture_id;
  const char *fixture_sha256;
  const char *outcome;
  const char *detail;
  const char *stop_kind;
  const uint8_t *expected_log;
  size_t expected_log_length;
  size_t expected_log_total;
  uint8_t expected_log_fill_byte;
  uint32_t expected_entry_opcode;
  const char *expected_completion_pc;
  bool expect_unretired_stop;
  uint64_t expected_bound_tick;
  int expected_exit_code;
  bool expect_guest_progress;
} control_case;

static const uint8_t empty_log[] = "";
static const uint8_t wrong_log[] = "Wrong guest transcript\n";
static const uint8_t incomplete_log[] = "Tendies64 hello ";
static const uint8_t positive_log[] = "Tendies64 hello via open IPL3\n";

static const control_case positive_identity = {
    "positive identity source report is available for strict mutations",
    "hello.z64",
    "hello-openipl3",
    "fa276cc175a4e764b6fd5413193c44b51f38fd8135e33e400032827e02f27c7c",
    "pass",
    "positive-match",
    "authored-completion",
    positive_log,
    sizeof positive_log - 1u,
    sizeof positive_log - 1u,
    0u,
    0u,
    "ffffffff80000478",
    true,
    0u,
    0,
    true};

static const control_case empty_halt = {
    "empty BREAK returns boot-fail/empty-output after real IPL3 execution",
    "hello-empty-break.z64",
    "hello-empty-break",
    "5ae986bb263cafa28e2b06a17e3d3f3ee229c55f75cd1ab91df7a9698e35a652",
    "boot-fail",
    "empty-output",
    "authored-completion",
    empty_log,
    0u,
    0u,
    0u,
    UINT32_C(0x0000000d),
    "ffffffff80000400",
    true,
    0u,
    0,
    true};

static const control_case empty_loop = {
    "empty loop at the guest bound is boot-fail/empty-output",
    "hello-empty-loop.z64",
    "hello-empty-loop",
    "88edd612852e36897a979182db53bf7cb184c31d56128c74e9544ec797f3c815",
    "boot-fail",
    "empty-output",
    "guest-bound",
    empty_log,
    0u,
    0u,
    0u,
    UINT32_C(0x1000ffff),
    NULL,
    false,
    UINT64_C(530500),
    0,
    true};

static const control_case incomplete_loop = {
    "proper transcript prefix at the guest bound is timeout",
    "hello-incomplete-loop.z64",
    "hello-incomplete-loop",
    "ba62967b56a7317d9ca4e4c8ffa3acfce5856b3307f6ca67da1739ba394b1054",
    "timeout",
    "guest-bound",
    "guest-bound",
    incomplete_log,
    sizeof incomplete_log - 1u,
    sizeof incomplete_log - 1u,
    0u,
    0u,
    NULL,
    false,
    UINT64_C(530500),
    0,
    true};

static const control_case wrong_transcript = {
    "wrong complete transcript is guest-fail",
    "hello-wrong-text.z64",
    "hello-wrong-text",
    "7626a661296fd8f5a5939ceb3b1fa3321f96e23be0e0bbf6b06b7e57871d86b8",
    "guest-fail",
    "wrong-transcript",
    "authored-completion",
    wrong_log,
    sizeof wrong_log - 1u,
    sizeof wrong_log - 1u,
    0u,
    0u,
    "ffffffff80000478",
    false,
    0u,
    0,
    true};

static const control_case unsupported_opcode = {
    "unsupported entry opcode is boot-fail before retirement",
    "hello-unsupported.z64",
    "hello-unsupported",
    "7c89481f15137f24a7c7067a67ccd6929dad6a5f982ec459bf39114d2f9926e9",
    "boot-fail",
    "unsupported-execution",
    "unsupported-execution",
    empty_log,
    0u,
    0u,
    0u,
    UINT32_C(0xffffffff),
    NULL,
    true,
    0u,
    0,
    true};

static const control_case output_overflow = {
    "real guest output overflow is harness-error and cannot pass",
    "hello-output-overflow.z64",
    "hello-output-overflow",
    "4ae73021dbdc2465944887a356c8f9154b7f1b3109487b82c808e9b3cccde18e",
    "harness-error",
    "output-overflow",
    "output-overflow",
    NULL,
    4096u,
    4097u,
    (uint8_t)'x',
    0u,
    NULL,
    false,
    UINT64_C(600000),
    0,
    true};

static const control_case unknown_fixture = {
    "unknown ROM identity is refused before any guest starts",
    "unknown.z64",
    NULL,
    NULL,
    "harness-error",
    "refusal",
    "refused",
    empty_log,
    0u,
    0u,
    0u,
    0u,
    NULL,
    false,
    0u,
    1,
    false};

static const control_case missing_fixture = {
    "missing ROM identity is not-started, distinct from refusal",
    "missing.z64",
    NULL,
    NULL,
    "missing-fixture",
    "fixture-absent",
    "not-started",
    empty_log,
    0u,
    0u,
    0u,
    0u,
    NULL,
    false,
    0u,
    1,
    false};

static const control_case invalid_input = {
    "changed raw schedule identity is refused before guest execution",
    "hello.z64",
    NULL,
    NULL,
    "harness-error",
    "refusal",
    "refused",
    empty_log,
    0u,
    0u,
    0u,
    0u,
    NULL,
    false,
    0u,
    1,
    false};

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

static bool write_all(int fd, const uint8_t *bytes, size_t length) {
  size_t offset = 0u;
  while (offset < length) {
    ssize_t count = write(fd, bytes + offset, length - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) return false;
    offset += (size_t)count;
  }
  return true;
}

static bool write_mutated_copy(const char *source_path,
                               const char *fixture_path,
                               size_t changed_offset, bool truncate_last) {
  uint8_t bytes[8192];
  size_t length = 0u;
  int fd = -1;
  bool ok = false;
  if (!read_bounded(source_path, bytes, sizeof bytes, &length) || length == 0u)
    return false;
  if (truncate_last) {
    length--;
  } else {
    if (changed_offset >= length) return false;
    bytes[changed_offset] ^= UINT8_C(1);
  }
  fd = open(fixture_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (fd < 0 || !write_all(fd, bytes, length) || close(fd) != 0) {
    if (fd >= 0) close(fd);
    fd = -1;
    goto cleanup;
  }
  fd = -1;
  ok = true;

cleanup:
  if (fd >= 0) close(fd);
  if (!ok) (void)unlink(fixture_path);
  return ok;
}

static bool create_identity_workspace(char directory[64]) {
  (void)snprintf(directory, 64u, "/tmp/t64-identity-mismatch.XXXXXX");
  return mkdtemp(directory) != NULL;
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

static bool field_is_null(const t64_json_node *object, const char *key) {
  const t64_json_node *value = t64_json_get(object, key);
  return value != NULL && value->type == T64_JSON_NULL;
}

static bool bytes_all_equal(const uint8_t *bytes, size_t length,
                            uint8_t expected) {
  size_t i;
  if (length > 0u && bytes == NULL) return false;
  for (i = 0u; i < length; i++) {
    if (bytes[i] != expected) return false;
  }
  return true;
}

static bool field_u64(const t64_json_node *object, const char *key,
                      uint64_t *actual_out) {
  const t64_json_node *value = t64_json_get(object, key);
  uint64_t actual = 0u;
  size_t i;
  if (value == NULL || value->type != T64_JSON_STRING || value->len == 0u ||
      actual_out == NULL) {
    return false;
  }
  for (i = 0u; i < value->len; i++) {
    unsigned digit;
    if (value->str[i] < '0' || value->str[i] > '9') return false;
    digit = (unsigned)(value->str[i] - '0');
    if (actual > (UINT64_MAX - digit) / UINT64_C(10)) return false;
    actual = actual * UINT64_C(10) + digit;
  }
  *actual_out = actual;
  return true;
}

static bool field_u64_equals(const t64_json_node *object, const char *key,
                             uint64_t expected) {
  uint64_t actual = 0u;
  return field_u64(object, key, &actual) && actual == expected;
}

static bool json_values_equal(const t64_json_node *left,
                              const t64_json_node *right) {
  const t64_json_node *left_child;
  const t64_json_node *right_child;

  if (left == NULL || right == NULL || left->type != right->type ||
      left->count != right->count) {
    return false;
  }
  switch (left->type) {
  case T64_JSON_NULL:
    return true;
  case T64_JSON_BOOL:
  case T64_JSON_INT:
    return left->num == right->num;
  case T64_JSON_STRING:
    return left->len == right->len &&
           memcmp(left->str, right->str, left->len) == 0;
  case T64_JSON_ARRAY:
  case T64_JSON_OBJECT:
    left_child = left->first;
    right_child = right->first;
    while (left_child != NULL && right_child != NULL) {
      if (left->type == T64_JSON_OBJECT &&
          (left_child->key_len != right_child->key_len ||
           memcmp(left_child->key, right_child->key,
                  left_child->key_len) != 0)) {
        return false;
      }
      if (!json_values_equal(left_child, right_child)) return false;
      left_child = left_child->next;
      right_child = right_child->next;
    }
    return left_child == NULL && right_child == NULL;
  }
  return false;
}

/* Compiler-specific build_id is deliberately excluded. These canonical
 * guest-evidence fields must match across repeated actual runs. */
static bool canonical_guest_identity_equal(const uint8_t *first_bytes,
                                           size_t first_length,
                                           const uint8_t *second_bytes,
                                           size_t second_length) {
  static const char *const fields[] = {
      "profile_id", "profile_digest", "fixture_id", "fixture_sha256",
      "ipl3_sha256", "ipl3_payload_sha256", "input_digest", "log_sha256",
      "state_digest", "guest_tick", "instruction_count", "counts",
      "guest_stop", "milestones", "outcome", "detail"};
  t64_json_doc first = {0};
  t64_json_doc second = {0};
  size_t i;
  bool equal = false;

  if (t64_json_parse(first_bytes, first_length, NULL, &first, NULL) != 0 ||
      t64_json_parse(second_bytes, second_length, NULL, &second, NULL) != 0) {
    goto cleanup;
  }
  equal = true;
  for (i = 0u; i < sizeof fields / sizeof fields[0]; i++) {
    if (!json_values_equal(t64_json_get(first.root, fields[i]),
                           t64_json_get(second.root, fields[i]))) {
      equal = false;
      break;
    }
  }

cleanup:
  t64_json_free(&first);
  t64_json_free(&second);
  return equal;
}

static bool milestone_pc_equals(const t64_json_node *root,
                                const char *milestone_kind,
                                const char *expected_pc) {
  const t64_json_node *milestones = t64_json_get(root, "milestones");
  const t64_json_node *item;
  if (milestones == NULL || milestones->type != T64_JSON_ARRAY) return false;
  for (item = milestones->first; item != NULL; item = item->next) {
    if (field_string_equals(item, "kind", milestone_kind)) {
      return field_string_equals(item, "pc", expected_pc);
    }
  }
  return false;
}

static bool fixture_entry_opcode_equals(const char *path,
                                        uint32_t expected_opcode) {
  uint8_t bytes[8192];
  size_t length = 0u;
  uint32_t opcode;
  if (!read_bounded(path, bytes, sizeof bytes, &length) || length < 4100u) {
    return false;
  }
  opcode = ((uint32_t)bytes[4096] << 24) |
           ((uint32_t)bytes[4097] << 16) |
           ((uint32_t)bytes[4098] << 8) | (uint32_t)bytes[4099];
  return opcode == expected_opcode;
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

static bool run_guest_case_copy(const char *supervisor, const char *checker,
                                const char *fixture_path,
                                const char *input_path,
                                const control_case *test_case,
                                char observed[192], uint8_t *copy_out,
                                size_t copy_capacity,
                                size_t *copy_length_out) {
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
  uint64_t guest_tick = 0u;
  uint64_t instruction_count = 0u;
  bool identity_ok;
  bool output_ok;
  bool progress_ok;
  bool opcode_ok;
  bool bound_ok;
  bool completion_ok;
  bool unretired_ok;
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
    char bound_option[] = "--bound";
    char bound_text[24];
    char fixture_option[] = "--fixture";
    char input_option[] = "--input";
    char capture_option[] = "--capture-out";
    uint64_t bound = test_case->expected_bound_tick != 0u
                         ? test_case->expected_bound_tick
                         : UINT64_C(530500);
    char *const args[] = {
        supervisor_arg, real_option, result_option, result_path,
        meta_option, meta_path, work_option, work_dir, wall_option, wall_ms,
        bound_option, bound_text, fixture_option, fixture_arg, input_option,
        input_arg, capture_option, capture_path, NULL};
    (void)snprintf(bound_text, sizeof bound_text, "%llu",
                   (unsigned long long)bound);
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
  if (copy_out != NULL) {
    if (copy_length_out == NULL || result_length > copy_capacity) goto cleanup;
    memcpy(copy_out, result_bytes, result_length);
    *copy_length_out = result_length;
  }
  root = document.root;
  stop = t64_json_get(root, "guest_stop");
  counts = t64_json_get(root, "counts");
  if (!read_bounded(capture_path, capture_bytes, sizeof capture_bytes,
                    &capture_length)) {
    capture_length = 0u;
  }
  identity_ok =
      (test_case->fixture_id == NULL
           ? field_is_null(root, "fixture_id")
           : field_string_equals(root, "fixture_id", test_case->fixture_id)) &&
      (test_case->fixture_sha256 == NULL
           ? field_is_null(root, "fixture_sha256")
           : field_string_equals(root, "fixture_sha256",
                                 test_case->fixture_sha256));
  output_ok = field_u64_equals(counts, "log_bytes",
                               (uint64_t)test_case->expected_log_length) &&
              field_u64_equals(counts, "log_bytes_total",
                               (uint64_t)test_case->expected_log_total) &&
              capture_length == test_case->expected_log_length &&
              (test_case->expected_log_fill_byte != 0u
                   ? bytes_all_equal(capture_bytes, capture_length,
                                     test_case->expected_log_fill_byte)
                   : (capture_length == 0u ||
                      memcmp(capture_bytes, test_case->expected_log,
                             capture_length) == 0));
  progress_ok = field_u64(root, "guest_tick", &guest_tick) &&
                field_u64(root, "instruction_count", &instruction_count) &&
                (test_case->expect_guest_progress
                     ? (guest_tick > 0u && instruction_count > 0u &&
                        milestone_pc_equals(root, "hello-entry",
                                            "ffffffff80000400"))
                     : (guest_tick == 0u && instruction_count == 0u));
  opcode_ok = test_case->expected_entry_opcode == 0u ||
              fixture_entry_opcode_equals(fixture_path,
                                          test_case->expected_entry_opcode);
  bound_ok = test_case->expected_bound_tick == 0u ||
             field_u64_equals(stop, "bound_tick",
                             test_case->expected_bound_tick);
  completion_ok = test_case->expected_completion_pc == NULL ||
                  milestone_pc_equals(root, "hello-completion",
                                      test_case->expected_completion_pc);
  unretired_ok = !test_case->expect_unretired_stop ||
                 (guest_tick == instruction_count + UINT64_C(1));
  passed = result_code == test_case->expected_exit_code && identity_ok &&
           field_string_equals(root, "outcome", test_case->outcome) &&
           field_string_equals(root, "detail", test_case->detail) &&
           field_string_equals(stop, "kind", test_case->stop_kind) &&
           output_ok && progress_ok && opcode_ok && bound_ok &&
           completion_ok && unretired_ok;
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

static bool run_guest_case(const char *supervisor, const char *checker,
                           const char *fixture_path, const char *input_path,
                           const control_case *test_case,
                           char observed[192]) {
  return run_guest_case_copy(supervisor, checker, fixture_path, input_path,
                             test_case, observed, NULL, 0u, NULL);
}

static uint8_t *find_bytes(uint8_t *bytes, size_t length,
                           const uint8_t *needle, size_t needle_length);

static bool replace_json_string(uint8_t *bytes, size_t *length,
                                size_t capacity, const char *key,
                                const char *replacement) {
  char prefix[128];
  uint8_t *field;
  uint8_t *value;
  uint8_t *end;
  size_t prefix_length;
  size_t old_length;
  size_t replacement_length;
  int written = snprintf(prefix, sizeof prefix, "\"%s\":\"", key);
  if (written <= 0 || (size_t)written >= sizeof prefix ||
      bytes == NULL || length == NULL || replacement == NULL) {
    return false;
  }
  prefix_length = (size_t)written;
  field = find_bytes(bytes, *length, (const uint8_t *)prefix, prefix_length);
  if (field == NULL) return false;
  value = field + prefix_length;
  end = (uint8_t *)memchr(value, '"', *length - (size_t)(value - bytes));
  if (end == NULL) return false;
  old_length = (size_t)(end - value);
  replacement_length = strlen(replacement);
  if (replacement_length > capacity ||
      *length - old_length > capacity - replacement_length) {
    return false;
  }
  memmove(value + replacement_length, end, *length - (size_t)(end - bytes));
  memcpy(value, replacement, replacement_length);
  *length = *length - old_length + replacement_length;
  return true;
}

static bool remove_first_milestone(uint8_t *bytes, size_t *length) {
  static const char marker[] = "\"milestones\":[{";
  uint8_t *field;
  uint8_t *first;
  uint8_t *end;
  size_t marker_length = sizeof marker - 1u;
  field = find_bytes(bytes, *length, (const uint8_t *)marker, marker_length);
  if (field == NULL) return false;
  first = field + marker_length - 1u;
  end = find_bytes(first, *length - (size_t)(first - bytes),
                   (const uint8_t *)"},", 2u);
  if (end == NULL) return false;
  memmove(first, end + 2u, *length - (size_t)(end + 2u - bytes));
  *length -= (size_t)(end + 2u - first);
  return true;
}

static bool write_bytes(const char *path, const uint8_t *bytes, size_t length) {
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  bool ok;
  if (fd < 0) return false;
  ok = write_all(fd, bytes, length);
  if (close(fd) != 0) ok = false;
  return ok;
}

static bool checker_rejects_mutation(const char *checker,
                                     const char *report_path,
                                     const uint8_t *baseline,
                                     size_t baseline_length,
                                     const char *key,
                                     const char *replacement) {
  uint8_t changed[4097];
  size_t changed_length = baseline_length;
  memcpy(changed, baseline, baseline_length);
  return replace_json_string(changed, &changed_length, sizeof changed,
                             key, replacement) &&
         write_bytes(report_path, changed, changed_length) &&
         !checker_accepts(checker, report_path);
}

static bool checker_rejects_skipped_ipl3_milestone(
    const char *checker, const char *report_path, const uint8_t *baseline,
    size_t baseline_length) {
  uint8_t changed[4097];
  size_t changed_length = baseline_length;
  memcpy(changed, baseline, baseline_length);
  return remove_first_milestone(changed, &changed_length) &&
         write_bytes(report_path, changed, changed_length) &&
         !checker_accepts(checker, report_path);
}

static bool changed_seed_profile_digest(char output[65]) {
  static const char digits[] = "0123456789abcdef";
  const t64_profile_descriptor *profile = NULL;
  t64_profile_descriptor changed;
  uint8_t digest[T64_DIGEST_SIZE];
  size_t i;

  if (output == NULL ||
      t64_profile_lookup("ntsc-cart-4m-openipl3-warm-v1", &profile) !=
          T64_STATUS_OK ||
      profile == NULL) {
    return false;
  }
  changed = *profile;
  changed.hle_seed_reset_version++;
  if (t64_profile_hash(&changed, digest) != T64_STATUS_OK) return false;
  for (i = 0u; i < T64_DIGEST_SIZE; i++) {
    output[i * 2u] = digits[digest[i] >> 4];
    output[i * 2u + 1u] = digits[digest[i] & 0x0fu];
  }
  output[T64_DIGEST_SIZE * 2u] = '\0';
  return true;
}

static uint8_t *find_bytes(uint8_t *bytes, size_t length,
                           const uint8_t *needle, size_t needle_length) {
  size_t i;
  if (needle_length == 0u) return bytes;
  if (bytes == NULL || needle == NULL || needle_length > length) return NULL;
  for (i = 0u; i <= length - needle_length; i++) {
    if (memcmp(bytes + i, needle, needle_length) == 0) return bytes + i;
  }
  return NULL;
}

static bool checker_identity_mutations(const char *supervisor,
                                       const char *checker,
                                       const char *fixture_dir,
                                       const char *input_path) {
  static const struct {
    const char *field;
    const char *replacement;
  } mutations[] = {
      {"build_id", "invalid-build-id"},
      {"source_revision", "invalid-revision"},
      {"profile_id", "ntsc-cart-4m-openipl3-warm-v2"},
      {"profile_digest",
       "0000000000000000000000000000000000000000000000000000000000000000"},
      {"fixture_id", "hello-unknown"},
      {"fixture_sha256",
       "0000000000000000000000000000000000000000000000000000000000000000"},
      {"ipl3_sha256",
       "0000000000000000000000000000000000000000000000000000000000000000"},
      {"ipl3_payload_sha256",
       "0000000000000000000000000000000000000000000000000000000000000000"},
      {"log_sha256",
       "0000000000000000000000000000000000000000000000000000000000000000"},
      {"state_digest", "not-a-sha256"},
  };
  char directory[] = "/tmp/t64-real-identity-report.XXXXXX";
  char positive_fixture[1024];
  char report_path[512] = {0};
  uint8_t baseline[4097];
  size_t baseline_length = 0u;
  uint8_t repeated[4097];
  size_t repeated_length = 0u;
  char changed_seed_digest[T64_DIGEST_SIZE * 2u + 1u];
  char observed[192] = "no valid positive identity report";
  size_t i;
  int n;
  bool ok = false;

  if (mkdtemp(directory) == NULL) return false;
  n = snprintf(positive_fixture, sizeof positive_fixture, "%s/%s",
               fixture_dir, positive_identity.fixture_name);
  if (n <= 0 || (size_t)n >= sizeof positive_fixture) goto cleanup;
  n = snprintf(report_path, sizeof report_path, "%s/mutated.json", directory);
  if (n <= 0 || (size_t)n >= sizeof report_path) goto cleanup;
  if (!run_guest_case_copy(supervisor, checker, positive_fixture, input_path,
                           &positive_identity, observed, baseline,
                           sizeof baseline, &baseline_length)) {
    (void)printf("# positive identity baseline failed: %s\n", observed);
    goto cleanup;
  }
  if (!run_guest_case_copy(supervisor, checker, positive_fixture, input_path,
                           &positive_identity, observed, repeated,
                           sizeof repeated, &repeated_length) ||
      !canonical_guest_identity_equal(baseline, baseline_length, repeated,
                                      repeated_length)) {
    (void)printf("# repeated positive runs differ in canonical guest identities, excluding build_id\n");
    goto cleanup;
  }
  ok = true;
  for (i = 0u; i < sizeof mutations / sizeof mutations[0]; i++) {
    if (!checker_rejects_mutation(checker, report_path, baseline,
                                  baseline_length, mutations[i].field,
                                  mutations[i].replacement)) {
      (void)printf("# checker mutation failed: %s\n", mutations[i].field);
      ok = false;
    }
  }
  if (!changed_seed_profile_digest(changed_seed_digest) ||
      !checker_rejects_mutation(checker, report_path, baseline,
                                baseline_length, "profile_digest",
                                changed_seed_digest)) {
    (void)printf("# checker mutation failed: altered HLE seed profile identity\n");
    ok = false;
  }
  if (!checker_rejects_skipped_ipl3_milestone(checker, report_path, baseline,
                                              baseline_length)) {
    (void)printf("# checker mutation failed: skipped IPL3 milestone\n");
    ok = false;
  }

cleanup:
  if (report_path[0] != '\0') (void)unlink(report_path);
  (void)rmdir(directory);
  return ok;
}

int main(int argc, char **argv) {
  char fixture_path[1024];
  char observed[192] = "no valid report";
  const char *fixture_dir = getenv("T64_TEST_FIXTURE_DIR");
  const char *input_path = getenv("T64_TEST_INPUT_PATH");
  const control_case *test_case = NULL;
  const char *description = NULL;
  int n;
  bool passed;

  (void)printf("TAP version 13\n1..1\n");
  if (argc != 4 || fixture_dir == NULL || input_path == NULL) {
    (void)printf("not ok 1 - empty BREAK control setup\n");
    return 1;
  }
  if (strcmp(argv[3], "empty-halt") == 0) {
    test_case = &empty_halt;
    description = empty_halt.name;
  } else if (strcmp(argv[3], "empty-bound") == 0) {
    description = "empty bound and proper-prefix bound use distinct guest outcomes";
  } else if (strcmp(argv[3], "wrong-transcript") == 0) {
    test_case = &wrong_transcript;
    description = wrong_transcript.name;
  } else if (strcmp(argv[3], "unsupported-opcode") == 0) {
    test_case = &unsupported_opcode;
    description = unsupported_opcode.name;
  } else if (strcmp(argv[3], "output-overflow") == 0) {
    test_case = &output_overflow;
    description = output_overflow.name;
  } else if (strcmp(argv[3], "identity-mismatch") == 0) {
    description = "real input identities and strict report evidence reject mutations";
  } else {
    (void)printf("not ok 1 - real control scenario is known\n");
    return 1;
  }
  if (strcmp(argv[3], "empty-bound") == 0) {
    char first_observed[192] = "no valid empty-bound report";
    char second_observed[192] = "no valid prefix report";
    char first_path[1024];
    char second_path[1024];
    n = snprintf(first_path, sizeof first_path, "%s/%s", fixture_dir,
                 empty_loop.fixture_name);
    if (n <= 0 || (size_t)n >= sizeof first_path) return 1;
    n = snprintf(second_path, sizeof second_path, "%s/%s", fixture_dir,
                 incomplete_loop.fixture_name);
    if (n <= 0 || (size_t)n >= sizeof second_path) return 1;
    passed = run_guest_case(argv[1], argv[2], first_path, input_path,
                            &empty_loop, first_observed) &&
             run_guest_case(argv[1], argv[2], second_path, input_path,
                            &incomplete_loop, second_observed);
    if (!passed) {
      (void)printf("# empty-loop %s\n# incomplete-loop %s\n",
                   first_observed, second_observed);
    }
  } else if (strcmp(argv[3], "identity-mismatch") == 0) {
    char source_path[1024];
    char positive_path[1024];
    char workspace[64] = {0};
    char unknown_path[512] = {0};
    char truncated_path[512] = {0};
    char changed_entry_path[512] = {0};
    char changed_input_path[512] = {0};
    char missing_path[512] = {0};
    char unknown_observed[192] = "no valid unknown-fixture report";
    char missing_observed[192] = "no valid missing-fixture report";
    char truncated_observed[192] = "no valid truncated-fixture report";
    char entry_observed[192] = "no valid changed-entry report";
    char input_observed[192] = "no valid changed-input report";
    bool input_cases;
    n = snprintf(source_path, sizeof source_path, "%s/%s", fixture_dir,
                 empty_halt.fixture_name);
    if (n <= 0 || (size_t)n >= sizeof source_path ||
        !create_identity_workspace(workspace)) {
      (void)printf("not ok 1 - identity control fixture preparation succeeds\n");
      return 1;
    }
    n = snprintf(positive_path, sizeof positive_path, "%s/%s", fixture_dir,
                 positive_identity.fixture_name);
    if (n <= 0 || (size_t)n >= sizeof positive_path) return 1;
    n = snprintf(unknown_path, sizeof unknown_path, "%s/unknown-title.z64",
                 workspace);
    if (n <= 0 || (size_t)n >= sizeof unknown_path ||
        !write_mutated_copy(source_path, unknown_path, 0x20u, false)) return 1;
    n = snprintf(truncated_path, sizeof truncated_path, "%s/truncated.z64",
                 workspace);
    if (n <= 0 || (size_t)n >= sizeof truncated_path ||
        !write_mutated_copy(source_path, truncated_path, 0u, true)) return 1;
    n = snprintf(changed_entry_path, sizeof changed_entry_path,
                 "%s/changed-entry.z64", workspace);
    if (n <= 0 || (size_t)n >= sizeof changed_entry_path ||
        !write_mutated_copy(source_path, changed_entry_path, 0x08u, false))
      return 1;
    n = snprintf(changed_input_path, sizeof changed_input_path,
                 "%s/changed.input.json", workspace);
    if (n <= 0 || (size_t)n >= sizeof changed_input_path ||
        !write_mutated_copy(input_path, changed_input_path, 1u, false)) return 1;
    n = snprintf(missing_path, sizeof missing_path, "%s/%s", workspace,
                 missing_fixture.fixture_name);
    if (n <= 0 || (size_t)n >= sizeof missing_path) return 1;
    input_cases = run_guest_case(argv[1], argv[2], unknown_path, input_path,
                                 &unknown_fixture, unknown_observed);
    input_cases = run_guest_case(argv[1], argv[2], missing_path, input_path,
                                 &missing_fixture, missing_observed) && input_cases;
    input_cases = run_guest_case(argv[1], argv[2], truncated_path, input_path,
                                 &unknown_fixture, truncated_observed) && input_cases;
    input_cases = run_guest_case(argv[1], argv[2], changed_entry_path,
                                 input_path, &unknown_fixture,
                                 entry_observed) && input_cases;
    input_cases = run_guest_case(argv[1], argv[2], positive_path,
                                 changed_input_path, &invalid_input,
                                 input_observed) && input_cases;
    passed = checker_identity_mutations(argv[1], argv[2], fixture_dir,
                                        input_path) && input_cases;
    (void)unlink(unknown_path);
    (void)unlink(truncated_path);
    (void)unlink(changed_entry_path);
    (void)unlink(changed_input_path);
    (void)rmdir(workspace);
    if (!passed) {
      (void)printf("# unknown %s\n# missing %s\n# truncated %s\n"
                   "# entry %s\n# input %s\n",
                   unknown_observed, missing_observed, truncated_observed,
                   entry_observed, input_observed);
    }
  } else {
    n = snprintf(fixture_path, sizeof fixture_path, "%s/%s", fixture_dir,
                 test_case->fixture_name);
    if (n <= 0 || (size_t)n >= sizeof fixture_path) {
      (void)printf("not ok 1 - real control fixture path fits\n");
      return 1;
    }
    passed = run_guest_case(argv[1], argv[2], fixture_path, input_path,
                            test_case, observed);
  }
  if (!passed && strcmp(argv[3], "empty-bound") != 0 &&
      strcmp(argv[3], "identity-mismatch") != 0) {
    (void)printf("# observed %s\n", observed);
  }
  (void)printf("%s 1 - %s\n", passed ? "ok" : "not ok", description);
  return passed ? 0 : 1;
}
