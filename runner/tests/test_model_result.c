#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define DIGEST_A "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
#define DIGEST_B "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
#define DIGEST_C "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
#define DIGEST_D "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"

static const char canonical_match[] =
    "{\"assertions\":{\"checked\":3,\"failed\":0},"
    "\"build_id\":\"t64-build-v1-" DIGEST_A "\","
    "\"content_digest\":\"" DIGEST_B "\","
    "\"evidence_tier\":\"unit-tested\",\"guest_tick\":\"0\","
    "\"model_scope\":\"native-model-probe\","
    "\"outcome\":\"model-match\",\"profile_hash\":\"" DIGEST_C "\","
    "\"profile_id\":\"ntsc-cart-4m-skeleton\","
    "\"schema\":\"t64.model-result\",\"schema_version\":1,"
    "\"state_digest\":\"" DIGEST_D "\"}\n";

static const char canonical_suite[] =
    "{\"case_count\":1,\"inventory_sha256\":\"" DIGEST_A
    "\",\"schema\":\"t64.suite\",\"schema_version\":1}\n";
static char oversized_record[65537];

static size_t test_count;
static size_t failures;
static const char *test_executable_path;

#define CHILD_DEADLINE_MS 10000u

static size_t replace_once(const char *source, const char *needle,
                           const char *replacement, char *out, size_t capacity) {
  const char *found = strstr(source, needle);
  size_t prefix;
  size_t replacement_len;
  size_t suffix_len;
  size_t total;

  if (found == NULL) {
    return 0u;
  }
  prefix = (size_t)(found - source);
  replacement_len = strlen(replacement);
  suffix_len = strlen(found + strlen(needle));
  total = prefix + replacement_len + suffix_len;
  if (total + 1u > capacity) {
    return 0u;
  }
  (void)memcpy(out, source, prefix);
  (void)memcpy(out + prefix, replacement, replacement_len);
  (void)memcpy(out + prefix + replacement_len, found + strlen(needle),
               suffix_len + 1u);
  return total;
}

static size_t append_member(const char *source, const char *member,
                            char *out, size_t capacity) {
  const char *end = strrchr(source, '}');
  size_t prefix;
  int written;

  if (end == NULL) {
    return 0u;
  }
  prefix = (size_t)(end - source);
  if (prefix >= capacity) {
    return 0u;
  }
  (void)memcpy(out, source, prefix);
  written = snprintf(out + prefix, capacity - prefix, ",%s}\n", member);
  return written < 0 || (size_t)written >= capacity - prefix
             ? 0u
             : prefix + (size_t)written;
}

static int write_all(int fd, const char *bytes, size_t length) {
  while (length > 0u) {
    ssize_t written = write(fd, bytes, length);
    if (written < 0 && errno == EINTR) {
      continue;
    }
    if (written <= 0) {
      return 0;
    }
    bytes += (size_t)written;
    length -= (size_t)written;
  }
  return 1;
}

static int write_record(const char *path, const char *record, size_t length) {
  int fd = open(path, O_WRONLY | O_TRUNC);
  int ok;

  if (fd < 0) {
    return 0;
  }
  ok = write_all(fd, record, length);
  if (close(fd) != 0) {
    ok = 0;
  }
  return ok;
}

static bool process_group_exists(pid_t group) {
  if (kill(-group, 0) == 0) {
    return true;
  }
  return errno == EPERM;
}

static bool deadline_reached(const struct timespec *now,
                             const struct timespec *deadline) {
  return now->tv_sec > deadline->tv_sec ||
         (now->tv_sec == deadline->tv_sec &&
          now->tv_nsec >= deadline->tv_nsec);
}

static void terminate_process_group(pid_t group, bool child_reaped) {
  struct timespec cleanup_deadline;
  struct timespec now;
  struct timespec pause_time = {0, 1000000L};
  int status;

  (void)kill(-group, SIGKILL);
  if (!child_reaped) {
    pid_t waited;

    (void)kill(group, SIGKILL);
    do {
      waited = waitpid(group, &status, 0);
    } while (waited < 0 && errno == EINTR);
  }
  while (waitpid(-group, &status, WNOHANG) > 0) {
  }
  if (clock_gettime(CLOCK_MONOTONIC, &cleanup_deadline) != 0) {
    return;
  }
  cleanup_deadline.tv_sec++;
  while (process_group_exists(group) &&
         clock_gettime(CLOCK_MONOTONIC, &now) == 0 &&
         !deadline_reached(&now, &cleanup_deadline)) {
    (void)nanosleep(&pause_time, NULL);
  }
}

static int wait_for_process_group(pid_t child, unsigned timeout_ms,
                                  int *status_out) {
  struct timespec deadline;
  struct timespec now;
  struct timespec pause_time = {0, 1000000L};
  int child_status = 0;
  bool child_reaped = false;

  if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) {
    terminate_process_group(child, false);
    return 127;
  }
  deadline.tv_sec += (time_t)(timeout_ms / 1000u);
  deadline.tv_nsec += (long)(timeout_ms % 1000u) * 1000000L;
  if (deadline.tv_nsec >= 1000000000L) {
    deadline.tv_sec++;
    deadline.tv_nsec -= 1000000000L;
  }

  for (;;) {
    if (!child_reaped) {
      pid_t waited = waitpid(child, &child_status, WNOHANG);

      if (waited == child) {
        child_reaped = true;
      } else if (waited < 0 && errno == ECHILD) {
        child_reaped = true;
      } else if (waited < 0 && errno != EINTR) {
        terminate_process_group(child, false);
        return 127;
      }
    }
    if (child_reaped && !process_group_exists(child)) {
      *status_out = child_status;
      return 0;
    }
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 ||
        deadline_reached(&now, &deadline)) {
      terminate_process_group(child, child_reaped);
      *status_out = child_status;
      return 1;
    }
    (void)nanosleep(&pause_time, NULL);
  }
}

static int process_status_with_timeout(const char *checker,
                                       const char *record_path,
                                       const char *output_path,
                                       unsigned timeout_ms) {
  pid_t child = fork();
  int status = 0;
  int wait_result;

  if (child < 0) {
    return 127;
  }
  if (child == 0) {
    if (setpgid(0, 0) != 0) {
      _exit(127);
    }
    int output_fd = open(output_path, O_WRONLY | O_TRUNC);
    if (output_fd < 0 || dup2(output_fd, STDOUT_FILENO) < 0 ||
        dup2(output_fd, STDERR_FILENO) < 0) {
      _exit(127);
    }
    (void)close(output_fd);
    execl(checker, checker, "result", record_path, (char *)NULL);
    _exit(127);
  }
  wait_result = wait_for_process_group(child, timeout_ms, &status);
  if (wait_result == 1) {
    return 124;
  }
  if (wait_result != 0) {
    return 127;
  }
  if (!WIFEXITED(status)) {
    return 128;
  }
  return WEXITSTATUS(status);
}

static int process_status(const char *checker, const char *record_path,
                          const char *output_path) {
  return process_status_with_timeout(checker, record_path, output_path,
                                     CHILD_DEADLINE_MS);
}

static int output_contains(const char *path, const char *needle) {
  char output[2048];
  FILE *file = fopen(path, "rb");
  size_t length;

  if (file == NULL) {
    return 0;
  }
  length = fread(output, 1u, sizeof output - 1u, file);
  (void)fclose(file);
  output[length] = '\0';
  return strstr(output, needle) != NULL;
}

static void expect_case(const char *checker, const char *record_path,
                        const char *output_path, const char *record,
                        size_t record_len, const char *name, int expected_exit,
                        const char *required_output) {
  int actual_exit = -1;
  bool passed = write_record(record_path, record, record_len) != 0;

  if (passed) {
    actual_exit = process_status(checker, record_path, output_path);
    passed = actual_exit == expected_exit;
  }
  if (passed && required_output != NULL) {
    passed = output_contains(output_path, required_output) != 0;
  }
  test_count++;
  if (passed) {
    printf("ok %lu - %s\n", (unsigned long)test_count, name);
  } else {
    failures++;
    printf("not ok %lu - %s\n", (unsigned long)test_count, name);
  }
}

static void expect_file_case(const char *checker, const char *record_path,
                             const char *output_path, const char *name,
                             int expected_exit) {
  int actual_exit = process_status(checker, record_path, output_path);
  bool passed = actual_exit == expected_exit;

  test_count++;
  if (passed) {
    printf("ok %lu - %s\n", (unsigned long)test_count, name);
  } else {
    failures++;
    printf("not ok %lu - %s\n", (unsigned long)test_count, name);
  }
}

static void expect_oversized_and_fifo_rejection(const char *checker,
                                                const char *record_path,
                                                const char *output_path) {
  int actual_exit = -1;
  bool passed = write_record(record_path, oversized_record,
                             sizeof oversized_record) != 0;

  if (passed) {
    actual_exit = process_status(checker, record_path, output_path);
    passed = actual_exit == 2;
  }
  if (passed) {
    passed = unlink(record_path) == 0 && mkfifo(record_path, 0600) == 0;
  }
  if (passed) {
    actual_exit = process_status(checker, record_path, output_path);
    passed = actual_exit == 2;
  }
  if (passed) {
    actual_exit = process_status_with_timeout(
        test_executable_path, "--timeout-descendants", output_path, 100u);
    passed = actual_exit == 124;
  }
  (void)unlink(record_path);
  test_count++;
  if (passed) {
    printf("ok %lu - reject input above configured bound\n",
           (unsigned long)test_count);
  } else {
    failures++;
    printf("not ok %lu - reject input above configured bound\n",
           (unsigned long)test_count);
  }
}

int main(int argc, char **argv) {
  char record_path[] = "/tmp/t64-model-record-XXXXXX";
  char output_path[] = "/tmp/t64-model-output-XXXXXX";
  char changed[2048];
  char replacement[320];
  char long_profile[258];
  char suite_path[] = "/tmp/t64-model-suite-XXXXXX";
  int record_fd;
  int output_fd;
  int suite_fd;
  size_t length;

  if (argc == 3 && strcmp(argv[1], "result") == 0 &&
      strcmp(argv[2], "--timeout-descendants") == 0) {
    pid_t descendant = fork();

    if (descendant < 0) {
      return 127;
    }
    for (;;) {
      (void)pause();
    }
  }
  if (argc != 3) {
    return 2;
  }
  test_executable_path = argv[0];
  record_fd = mkstemp(record_path);
  output_fd = mkstemp(output_path);
  suite_fd = mkstemp(suite_path);
  if (record_fd < 0 || output_fd < 0 || suite_fd < 0) {
    return 2;
  }
  (void)close(record_fd);
  (void)close(output_fd);
  (void)close(suite_fd);

  printf("1..37\n");
  expect_case(argv[1], record_path, output_path, canonical_match,
              sizeof canonical_match - 1u, "accept canonical model-match", 0,
              "model_scope=native-model-probe");

  length = replace_once(canonical_match, "\"failed\":0", "\"failed\":1",
                        changed, sizeof changed);
  if (length > 0u) {
    char mismatch[2048];
    length = replace_once(changed, "\"outcome\":\"model-match\"",
                          "\"outcome\":\"model-mismatch\"", mismatch,
                          sizeof mismatch);
    expect_case(argv[1], record_path, output_path, mismatch, length,
                "accept canonical model-mismatch", 0,
                "outcome=model-mismatch");
  } else {
    expect_case(argv[1], record_path, output_path, "", 0u,
                "accept canonical model-mismatch", 1, NULL);
  }

  length = replace_once(canonical_match, "\"model_scope\":\"native-model-probe\"",
                        "\"model_scope\":\"guest-result\"", changed,
                        sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject changed model scope", 1, NULL);

  length = replace_once(canonical_match, "\"model_scope\":\"native-model-probe\",",
                        "", changed, sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject missing model scope", 1, NULL);

  length = replace_once(canonical_match, "\"outcome\":\"model-match\"",
                        "\"outcome\":\"pass\"", changed, sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject guest outcome", 1, NULL);

  length = replace_once(canonical_match, "\"evidence_tier\":\"unit-tested\"",
                        "\"evidence_tier\":\"hardware-derived\"", changed,
                        sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject false hardware evidence", 1, NULL);

  length = replace_once(canonical_match, "\"content_digest\":\"" DIGEST_B "\"",
                        "\"content_digest\":null", changed, sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject null content digest", 1, NULL);

  length = replace_once(canonical_match, "\"profile_hash\":\"" DIGEST_C "\"",
                        "\"profile_hash\":\"abc\"", changed, sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject malformed profile digest", 1, NULL);

  length = replace_once(canonical_match,
                        "\"profile_id\":\"ntsc-cart-4m-skeleton\"",
                        "\"profile_id\":\"unknown-profile\"", changed,
                        sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject unknown profile identity", 1, NULL);

  length = replace_once(canonical_match,
                        "\"build_id\":\"t64-build-v1-" DIGEST_A "\"",
                        "\"build_id\":\"0.0.0+synthetic\"", changed,
                        sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject non-production build identity", 1, NULL);

  length = replace_once(canonical_match, "\"schema_version\":1",
                        "\"schema_version\":2", changed, sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject unsupported schema version", 1, NULL);

  length = replace_once(canonical_match, "\"checked\":3", "\"checked\":0",
                        changed, sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject empty assertion count", 1, NULL);

  length = replace_once(canonical_match, "\"failed\":0", "\"failed\":4",
                        changed, sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject failed count above checked", 1, NULL);

  length = replace_once(canonical_match, "\"outcome\":\"model-match\"",
                        "\"outcome\":\"model-mismatch\"", changed,
                        sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject inconsistent model outcome", 1, NULL);

  length = append_member(canonical_match, "\"unknown\":0", changed,
                         sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject unknown top-level field", 1, NULL);

  length = append_member(canonical_match,
                         "\"state_digest\":\"" DIGEST_D "\"", changed,
                         sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject duplicate top-level field", 1, NULL);

  length = replace_once(canonical_match, "\"checked\":3", "\"extra\":3",
                        changed, sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject missing nested assertion field", 1, NULL);

  length = append_member(canonical_match, "\"checked\":4", changed,
                         sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject duplicate nested assertion field", 1, NULL);

  length = replace_once(canonical_match,
                        "\"assertions\":{\"checked\":3,\"failed\":0}",
                        "\"assertions\":{\"checked\":3,\"failed\":0,\"other\":0}",
                        changed, sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject unknown nested assertion field", 1, NULL);

  length = replace_once(canonical_match, "\"checked\":3",
                        "\"checked\":9007199254740992", changed,
                        sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject assertion count above canonical integer range", 1,
              NULL);

  length = replace_once(canonical_match, "\"checked\":3", "\"checked\":-1",
                        changed, sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject negative assertion count", 1, NULL);

  length = replace_once(canonical_match, "\"state_digest\":\"" DIGEST_D "\"",
                        "\"state_digest\":null", changed, sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject null state digest", 1, NULL);

  length = replace_once(canonical_match, "\"guest_tick\":\"0\"",
                        "\"guest_tick\":\"01\"", changed, sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject noncanonical guest tick", 1, NULL);

  length = replace_once(canonical_match, "\"guest_tick\":\"0\"",
                        "\"guest_tick\":\"18446744073709551616\"",
                        changed, sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject guest tick above u64", 1, NULL);

  length = replace_once(canonical_match, "{", "{ ", changed, sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject noncanonical whitespace", 1, NULL);

  length = replace_once(canonical_match, "}\n", "}\n{}", changed,
                        sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject trailing data", 1, NULL);

  expect_file_case(argv[1], argv[2], output_path,
                   "reject legacy t64.result v1 record", 1);
  expect_case(argv[1], suite_path, output_path, canonical_suite,
              sizeof canonical_suite - 1u, "reject legacy t64.suite v1 record",
              1, NULL);

  length = replace_once(canonical_match, "}\n", "}", changed,
                        sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject missing canonical final LF", 1, NULL);

  length = replace_once(canonical_match, "\"assertions\":{\"checked\":3,\"failed\":0}",
                        "\"assertions\":[]", changed, sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject wrong assertions shape", 1, NULL);

  length = replace_once(canonical_match, "\"failed\":0", "\"failed\":\"0\"",
                        changed, sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject string assertion count", 1, NULL);

  length = replace_once(canonical_match, "\"guest_tick\":\"0\"",
                        "\"guest_tick\":0", changed, sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject numeric guest tick", 1, NULL);

  length = replace_once(canonical_match,
                        "\"schema\":\"t64.model-result\"",
                        "\"schema\":\"t64.result\"", changed,
                        sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject guest result schema", 1, NULL);

  length = replace_once(canonical_match, "\"model_scope\":\"native-model-probe\",",
                        "\"model_scope\":\"native-model-probe\", ",
                        changed, sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject noncanonical internal whitespace", 1, NULL);

  length = replace_once(
      canonical_match,
      "\"schema\":\"t64.model-result\",\"schema_version\":1",
      "\"schema_version\":1,\"schema\":\"t64.model-result\"", changed,
      sizeof changed);
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject noncanonical key order", 1, NULL);

  (void)memset(long_profile, 'x', sizeof long_profile - 1u);
  long_profile[sizeof long_profile - 1u] = '\0';
  {
    int written = snprintf(replacement, sizeof replacement,
                           "\"profile_id\":\"%s\"", long_profile);
    length = written < 0 || (size_t)written >= sizeof replacement
                 ? 0u
                 : replace_once(canonical_match,
                                "\"profile_id\":\"ntsc-cart-4m-skeleton\"",
                                replacement, changed, sizeof changed);
  }
  expect_case(argv[1], record_path, output_path, changed, length,
              "reject string above configured bound", 1, NULL);

  (void)memset(oversized_record, ' ', sizeof oversized_record);
  expect_oversized_and_fifo_rejection(argv[1], record_path, output_path);

  (void)unlink(record_path);
  (void)unlink(output_path);
  (void)unlink(suite_path);
  return failures == 0u && test_count == 37u ? 0 : 1;
}
