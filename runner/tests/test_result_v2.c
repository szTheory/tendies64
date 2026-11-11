#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* Independently authored canonical real-v2 sample for the checker contract.
 * This JSON is structural test input, not evidence that a guest was run. */
static const char valid_pass_v2[] =
    "{\"build_id\":\"t64-build-v1-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\","
    "\"counts\":{\"log_bytes\":\"30\",\"log_bytes_total\":\"30\"},"
    "\"detail\":\"positive-match\","
    "\"evidence_kind\":\"guest-execution\","
    "\"evidence_tier\":\"smoke\","
    "\"fixture_id\":\"hello-openipl3\","
    "\"fixture_sha256\":\"fa276cc175a4e764b6fd5413193c44b51f38fd8135e33e400032827e02f27c7c\","
    "\"guest_stop\":{\"bound_tick\":\"600000\",\"kind\":\"authored-completion\"},"
    "\"guest_tick\":\"530438\","
    "\"input_digest\":\"6d5593bc2d95465b44ec7ce7785e5ff7d7d53605de9b039a021bc697d226186d\","
    "\"instruction_count\":\"530437\","
    "\"ipl3_payload_sha256\":\"f716a90b1d36ec4253d096790791256d3543455e54473a755ff8c3d1199b15d5\","
    "\"ipl3_sha256\":\"f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068\","
    "\"log_sha256\":\"86594505bea53c9843f9eaf07e4cf0121cbe0055670324674fd8385c88ffd18e\","
    "\"milestones\":["
    "{\"guest_tick\":\"0\",\"kind\":\"ipl3-entry\",\"pc\":\"ffffffffa4000040\"},"
    "{\"guest_tick\":\"100\",\"kind\":\"loader-dma\",\"pc\":\"ffffffffa4000100\"},"
    "{\"guest_tick\":\"530174\",\"kind\":\"hello-entry\",\"pc\":\"ffffffff80000400\"},"
    "{\"guest_tick\":\"530438\",\"kind\":\"hello-completion\",\"pc\":\"ffffffff80000478\"}],"
    "\"outcome\":\"pass\","
    "\"profile_digest\":\"a171241f68c377f7c324a995a08ff8bd8da60bbe70df14193d8f4b8ae7e9769f\","
    "\"profile_id\":\"ntsc-cart-4m-openipl3-warm-v1\","
    "\"schema\":\"t64.result\","
    "\"schema_version\":2,"
    "\"source_revision\":\"a69d215a06968d1552494222a8c9536c304ee537\","
    "\"state_digest\":\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\"}\n";

static const char prelaunch_missing_fixture[] =
    "{\"build_id\":null,"
    "\"counts\":{\"log_bytes\":\"0\",\"log_bytes_total\":\"0\"},"
    "\"detail\":\"fixture-absent\","
    "\"evidence_kind\":\"guest-execution\","
    "\"evidence_tier\":\"smoke\","
    "\"fixture_id\":null,\"fixture_sha256\":null,"
    "\"guest_stop\":{\"bound_tick\":\"600000\",\"kind\":\"not-started\"},"
    "\"guest_tick\":\"0\",\"input_digest\":null,"
    "\"instruction_count\":\"0\",\"ipl3_payload_sha256\":null,"
    "\"ipl3_sha256\":null,\"log_sha256\":null,\"milestones\":[],"
    "\"outcome\":\"missing-fixture\",\"profile_digest\":null,"
    "\"profile_id\":null,\"schema\":\"t64.result\",\"schema_version\":2,"
    "\"source_revision\":null,\"state_digest\":null}\n";

static const char model_result[] =
    "{\"assertions\":{\"checked\":3,\"failed\":0},"
    "\"build_id\":\"t64-build-v1-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\","
    "\"content_digest\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\","
    "\"evidence_tier\":\"unit-tested\",\"guest_tick\":\"0\","
    "\"model_scope\":\"native-model-probe\",\"outcome\":\"model-match\","
    "\"profile_hash\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\","
    "\"profile_id\":\"ntsc-cart-4m-skeleton\","
    "\"schema\":\"t64.model-result\",\"schema_version\":1,"
    "\"state_digest\":\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\"}\n";

static unsigned test_count;
static unsigned failure_count;

static int write_all(int fd, const char *bytes, size_t length) {
  while (length != 0u) {
    ssize_t wrote = write(fd, bytes, length);
    if (wrote < 0 && errno == EINTR) {
      continue;
    }
    if (wrote <= 0) {
      return 0;
    }
    bytes += (size_t)wrote;
    length -= (size_t)wrote;
  }
  return 1;
}

static int run_checker(const char *checker, const char *report, int *exit_code,
                       char *diagnostic, size_t diagnostic_size) {
  int error_pipe[2];
  pid_t child;
  int status;
  size_t used = 0;
  int null_fd;

  diagnostic[0] = '\0';
  if (pipe(error_pipe) != 0) {
    return 0;
  }
  child = fork();
  if (child < 0) {
    (void)close(error_pipe[0]);
    (void)close(error_pipe[1]);
    return 0;
  }
  if (child == 0) {
    (void)close(error_pipe[0]);
    null_fd = open("/dev/null", O_WRONLY);
    if (null_fd < 0 || dup2(null_fd, STDOUT_FILENO) < 0 ||
        dup2(error_pipe[1], STDERR_FILENO) < 0) {
      _exit(126);
    }
    (void)close(error_pipe[1]);
    if (null_fd != STDOUT_FILENO && null_fd != STDERR_FILENO) {
      (void)close(null_fd);
    }
    execl(checker, checker, "result", report, (char *)NULL);
    _exit(127);
  }

  (void)close(error_pipe[1]);
  while (used + 1u < diagnostic_size) {
    ssize_t got = read(error_pipe[0], diagnostic + used,
                       diagnostic_size - used - 1u);
    if (got < 0 && errno == EINTR) {
      continue;
    }
    if (got <= 0) {
      break;
    }
    used += (size_t)got;
  }
  diagnostic[used] = '\0';
  (void)close(error_pipe[0]);
  while (waitpid(child, &status, 0) < 0) {
    if (errno != EINTR) {
      return 0;
    }
  }
  if (!WIFEXITED(status)) {
    *exit_code = 128 + WTERMSIG(status);
  } else {
    *exit_code = WEXITSTATUS(status);
  }
  return 1;
}

static void extract_rule(const char *diagnostic, char *rule, size_t rule_size) {
  size_t i = 0u;
  while (diagnostic[i] != '\0' && diagnostic[i] != ' ' && i + 1u < rule_size) {
    rule[i] = diagnostic[i];
    i++;
  }
  rule[i] = '\0';
  if (i == 0u) {
    (void)strncpy(rule, "none", rule_size - 1u);
    rule[rule_size - 1u] = '\0';
  }
}

static int replace_once(char *report, size_t capacity, const char *before,
                        const char *after) {
  char changed[8192];
  char *at = strstr(report, before);
  size_t prefix;
  size_t before_len;
  size_t after_len;
  size_t suffix_len;

  if (at == NULL) {
    return 0;
  }
  prefix = (size_t)(at - report);
  before_len = strlen(before);
  after_len = strlen(after);
  suffix_len = strlen(at + before_len);
  if (prefix + after_len + suffix_len + 1u > sizeof changed ||
      prefix + after_len + suffix_len + 1u > capacity) {
    return 0;
  }
  memcpy(changed, report, prefix);
  memcpy(changed + prefix, after, after_len);
  memcpy(changed + prefix + after_len, at + before_len, suffix_len + 1u);
  memcpy(report, changed, prefix + after_len + suffix_len + 1u);
  return 1;
}

static int make_variant(char *out, size_t capacity, const char *before,
                        const char *after) {
  if (sizeof valid_pass_v2 > capacity) {
    return 0;
  }
  memcpy(out, valid_pass_v2, sizeof valid_pass_v2);
  return replace_once(out, capacity, before, after);
}

static int expect_case(const char *checker, const char *name, const char *report,
                       int expected_exit, const char *expected_rule) {
  char report_path[] = "/tmp/t64-result-v2-XXXXXX";
  char diagnostic[1024];
  char rule[64] = "none";
  int report_fd;
  int checker_exit = -1;
  int good;
  int passed;
  unsigned id = ++test_count;

  report_fd = mkstemp(report_path);
  if (report_fd < 0 ||
      !write_all(report_fd, report, strlen(report))) {
    if (report_fd >= 0) {
      (void)close(report_fd);
    }
    (void)unlink(report_path);
    printf("not ok %u - %s\n# expected: test report can be prepared\n", id, name);
    failure_count++;
    return 0;
  }
  if (close(report_fd) != 0) {
    (void)unlink(report_path);
    printf("not ok %u - %s\n# expected: test report closes cleanly\n", id, name);
    failure_count++;
    return 0;
  }

  good = run_checker(checker, report_path, &checker_exit, diagnostic,
                     sizeof diagnostic);
  (void)unlink(report_path);
  if (!good) {
    printf("not ok %u - %s\n# expected: checker executes\n", id, name);
    failure_count++;
    return 0;
  }
  extract_rule(diagnostic, rule, sizeof rule);
  passed = checker_exit == expected_exit &&
           (expected_rule == NULL || strcmp(rule, expected_rule) == 0);
  if (passed) {
    printf("ok %u - %s\n", id, name);
  } else {
    printf("not ok %u - %s\n# expected: exit=%d rule=%s\n"
           "# actual: exit=%d rule=%s\n",
           id, name, expected_exit,
           expected_rule != NULL ? expected_rule : "none", checker_exit, rule);
    failure_count++;
  }
  return passed;
}

static int expect_file_case(const char *checker, const char *name,
                            const char *report, int expected_exit,
                            const char *expected_rule) {
  char diagnostic[1024];
  char rule[64];
  int checker_exit = -1;
  int passed;
  unsigned id = ++test_count;

  if (!run_checker(checker, report, &checker_exit, diagnostic,
                   sizeof diagnostic)) {
    printf("not ok %u - %s\n# expected: checker executes\n", id, name);
    failure_count++;
    return 0;
  }
  extract_rule(diagnostic, rule, sizeof rule);
  passed = checker_exit == expected_exit &&
           (expected_rule == NULL || strcmp(rule, expected_rule) == 0);
  if (passed) {
    printf("ok %u - %s\n", id, name);
  } else {
    printf("not ok %u - %s\n# expected: exit=%d rule=%s\n"
           "# actual: exit=%d rule=%s\n",
           id, name, expected_exit,
           expected_rule != NULL ? expected_rule : "none", checker_exit, rule);
    failure_count++;
  }
  return passed;
}

int main(int argc, char **argv) {
  char changed[8192];
  char *report;

  if (argc != 3) {
    fprintf(stderr, "usage: test_result_v2 T64_RESULT_CHECK V1_GOLDEN\n");
    return 2;
  }
  puts("TAP version 13");
  (void)expect_case(argv[1], "accepts independently authored canonical real v2 pass",
                    valid_pass_v2, 0, NULL);
  if (make_variant(changed, sizeof changed,
                   "\"state_digest\":\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\"",
                   "\"state_digest\":null")) {
    (void)replace_once(changed, sizeof changed,
                       "\"guest_stop\":{\"bound_tick\":\"600000\",\"kind\":\"authored-completion\"}",
                       "\"guest_stop\":{\"bound_tick\":\"600000\",\"kind\":\"report-error\"}");
    (void)replace_once(changed, sizeof changed, "\"guest_tick\":\"530438\"",
                       "\"guest_tick\":\"0\"");
    (void)replace_once(changed, sizeof changed,
                       "\"instruction_count\":\"530437\"",
                       "\"instruction_count\":\"0\"");
    (void)replace_once(
        changed, sizeof changed,
        "\"milestones\":[{\"guest_tick\":\"0\",\"kind\":\"ipl3-entry\",\"pc\":\"ffffffffa4000040\"},{\"guest_tick\":\"100\",\"kind\":\"loader-dma\",\"pc\":\"ffffffffa4000100\"},{\"guest_tick\":\"530174\",\"kind\":\"hello-entry\",\"pc\":\"ffffffff80000400\"},{\"guest_tick\":\"530438\",\"kind\":\"hello-completion\",\"pc\":\"ffffffff80000478\"}],",
        "\"milestones\":[],");
    (void)replace_once(changed, sizeof changed, "\"outcome\":\"pass\"",
                       "\"outcome\":\"harness-error\"");
    (void)replace_once(changed, sizeof changed,
                       "\"detail\":\"positive-match\"",
                       "\"detail\":\"report-invalid\"");
    (void)expect_case(argv[1],
                      "accepts untrusted postlaunch report with null state and zero verified progress",
                      changed, 0, NULL);
  }
  if (make_variant(changed, sizeof changed,
                   "\"state_digest\":\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\"",
                   "\"state_digest\":null")) {
    (void)expect_case(argv[1], "rejects mixed-null identities on a guest pass",
                      changed, 1, "R-NULL-RULES");
  }
  if (make_variant(changed, sizeof changed, "\"outcome\":\"pass\"",
                   "\"outcome\":\"harness-error\"")) {
    (void)replace_once(changed, sizeof changed,
                       "\"detail\":\"positive-match\"",
                       "\"detail\":\"runner-exit-nonzero\"");
    (void)replace_once(changed, sizeof changed,
                       "\"kind\":\"authored-completion\"",
                       "\"kind\":\"host-error\"");
    (void)expect_case(argv[1],
                      "harness error overrides correct captured transcript after nonzero child exit",
                      changed, 0, NULL);
  }
  (void)expect_file_case(argv[1], "continues to accept legacy v1 positive golden",
                         argv[2], 0, NULL);
  (void)expect_case(argv[1], "refuses native model result as guest evidence",
                    model_result, 1, "R-SCHEMA");

  if (make_variant(changed, sizeof changed, "\"schema_version\":2",
                   "\"schema_version\":3")) {
    (void)expect_case(argv[1], "rejects unsupported result schema version",
                      changed, 1, "R-VERSION");
  }
  if (make_variant(changed, sizeof changed, "\"schema_version\":2",
                   "\"schema_version\":\"2\"")) {
    (void)expect_case(argv[1], "rejects string schema version", changed, 1,
                      "R-VERSION");
  }
  if (make_variant(changed, sizeof changed,
                   "\"schema_version\":2,\"source_revision\":\"a69d215a06968d1552494222a8c9536c304ee537\",",
                   "\"schema_version\":2,")) {
    (void)expect_case(argv[1], "rejects missing source revision",
                      changed, 1, "R-MISSING-FIELD");
  }
  if (make_variant(changed, sizeof changed,
                   "\"state_digest\":\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\"}",
                   "\"state_digest\":\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\",\"zz_extra\":0}")) {
    (void)expect_case(argv[1], "rejects unknown result field", changed, 1,
                      "R-UNKNOWN-FIELD");
  }
  if (make_variant(changed, sizeof changed, "\"counts\":{",
                   "\"counts\":{\"extra\":0,")) {
    (void)expect_case(argv[1], "rejects unknown nested count field", changed,
                      1, "R-UNKNOWN-FIELD");
  }
  if (make_variant(changed, sizeof changed, "\"schema_version\":2",
                   "\"schema_version\":2,\"schema_version\":2")) {
    (void)expect_case(argv[1], "rejects duplicate raw JSON key", changed, 1,
                      "R-JSON");
  }
  if (make_variant(changed, sizeof changed, "{\"build_id\":",
                   "{ \"build_id\":")) {
    (void)expect_case(argv[1], "rejects noncanonical whitespace", changed, 1,
                      "R-NOT-CANONICAL");
  }
  if (make_variant(changed, sizeof changed, "\"evidence_kind\":\"guest-execution\"",
                   "\"evidence_kind\":\"native-model-probe\"")) {
    (void)expect_case(argv[1], "rejects non-guest evidence kind", changed, 1,
                      "R-ENUM");
  }
  if (make_variant(changed, sizeof changed, "\"evidence_tier\":\"smoke\"",
                   "\"evidence_tier\":\"synthetic\"")) {
    (void)expect_case(argv[1], "rejects non-smoke real evidence tier", changed, 1,
                      "R-ENUM");
  }
  if (make_variant(changed, sizeof changed,
                   "t64-build-v1-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                   "t64-build-v1-AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA")) {
    (void)expect_case(argv[1], "rejects malformed build identity", changed, 1,
                      "R-TYPE");
  }
  if (make_variant(changed, sizeof changed,
                   "a171241f68c377f7c324a995a08ff8bd8da60bbe70df14193d8f4b8ae7e9769f",
                   "08634481792748ade0e64073b9cd51d5972ec3a56421278b3203979087b4e05d")) {
    (void)expect_case(argv[1], "rejects boot-policy digest substituted for full profile identity",
                      changed, 1, "R-IDENTITY");
  }
  if (make_variant(changed, sizeof changed,
                   "f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068",
                   "0522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068")) {
    (void)expect_case(argv[1], "rejects mismatched full IPL3 source identity",
                      changed, 1, "R-IDENTITY");
  }
  if (make_variant(changed, sizeof changed,
                   "f716a90b1d36ec4253d096790791256d3543455e54473a755ff8c3d1199b15d5",
                   "0716a90b1d36ec4253d096790791256d3543455e54473a755ff8c3d1199b15d5")) {
    (void)expect_case(argv[1], "rejects mismatched IPL3 payload identity",
                      changed, 1, "R-IDENTITY");
  }
  if (make_variant(changed, sizeof changed,
                   "a69d215a06968d1552494222a8c9536c304ee537",
                   "A69D215A06968D1552494222A8C9536C304EE537")) {
    (void)expect_case(argv[1], "rejects non-lowercase source revision",
                      changed, 1, "R-TYPE");
  }
  if (make_variant(changed, sizeof changed,
                   "\"guest_tick\":\"530438\"",
                   "\"guest_tick\":530438")) {
    (void)expect_case(argv[1], "rejects numeric guest tick", changed, 1,
                      "R-TYPE");
  }
  if (make_variant(changed, sizeof changed,
                   "\"guest_tick\":\"530438\"",
                   "\"guest_tick\":\"18446744073709551616\"")) {
    (void)expect_case(argv[1], "rejects guest tick above u64", changed, 1,
                      "R-TYPE");
  }
  if (make_variant(changed, sizeof changed,
                   "\"log_bytes\":\"30\"",
                   "\"log_bytes\":30")) {
    (void)expect_case(argv[1], "requires log counts to be decimal strings",
                      changed, 1, "R-TYPE");
  }
  if (make_variant(changed, sizeof changed,
                   "\"bound_tick\":\"600000\"",
                   "\"bound_tick\":\"530000\"")) {
  (void)expect_case(argv[1], "rejects guest tick beyond declared guest bound",
                    changed, 1, "R-CONSISTENCY");
  }
  if (make_variant(changed, sizeof changed,
                   "\"guest_stop\":{\"bound_tick\":\"600000\",\"kind\":\"authored-completion\"}",
                   "\"guest_stop\":{\"bound_tick\":\"600000\",\"kind\":\"authored-completion\",\"extra\":0}")) {
    (void)expect_case(argv[1], "rejects unknown guest stop field", changed, 1,
                      "R-UNKNOWN-FIELD");
  }
  if (make_variant(changed, sizeof changed,
                   "\"pc\":\"ffffffffa4000040\"",
                   "\"pc\":\"FFFFFFFFA4000040\"")) {
    (void)expect_case(argv[1], "rejects noncanonical milestone PC", changed,
                      1, "R-TYPE");
  }
  if (make_variant(changed, sizeof changed,
                   "\"kind\":\"hello-entry\"",
                   "\"kind\":\"loader-dma\"")) {
    (void)expect_case(argv[1], "rejects duplicate or reordered milestones",
                      changed, 1, "R-CONSISTENCY");
  }
  if (make_variant(changed, sizeof changed,
                   "{\"guest_tick\":\"100\",\"kind\":\"loader-dma\",\"pc\":\"ffffffffa4000100\"},",
                   "")) {
    (void)expect_case(argv[1], "pass requires loader DMA milestone", changed,
                      1, "R-CONSISTENCY");
  }
  if (make_variant(changed, sizeof changed, "\"instruction_count\":\"530437\"",
                   "\"instruction_count\":\"0\"")) {
    (void)expect_case(argv[1], "pass requires nonzero retired instructions",
                      changed, 1, "R-CONSISTENCY");
  }

  if (make_variant(changed, sizeof changed,
                   "\"log_sha256\":\"86594505bea53c9843f9eaf07e4cf0121cbe0055670324674fd8385c88ffd18e\"",
                   "\"log_sha256\":\"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc\"")) {
    (void)replace_once(changed, sizeof changed, "\"outcome\":\"pass\"",
                       "\"outcome\":\"guest-fail\"");
    (void)replace_once(changed, sizeof changed,
                       "\"detail\":\"positive-match\"",
                       "\"detail\":\"wrong-transcript\"");
    (void)expect_case(argv[1], "classifies wrong completed text as guest-fail",
                      changed, 0, NULL);
  }

  if (make_variant(changed, sizeof changed,
                   "\"log_sha256\":\"86594505bea53c9843f9eaf07e4cf0121cbe0055670324674fd8385c88ffd18e\"",
                   "\"log_sha256\":\"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855\"")) {
    (void)replace_once(changed, sizeof changed, "\"log_bytes\":\"30\"",
                       "\"log_bytes\":\"0\"");
    (void)replace_once(changed, sizeof changed, "\"log_bytes_total\":\"30\"",
                       "\"log_bytes_total\":\"0\"");
    (void)replace_once(changed, sizeof changed, "\"outcome\":\"pass\"",
                       "\"outcome\":\"boot-fail\"");
    (void)replace_once(changed, sizeof changed,
                       "\"detail\":\"positive-match\"",
                       "\"detail\":\"empty-output\"");
    (void)expect_case(argv[1], "empty output at authored completion is boot-fail",
                      changed, 0, NULL);
  }

  report = changed;
  if (make_variant(report, sizeof changed,
                   ",{\"guest_tick\":\"530438\",\"kind\":\"hello-completion\",\"pc\":\"ffffffff80000478\"}",
                   "")) {
    (void)replace_once(report, sizeof changed,
                       "\"guest_stop\":{\"bound_tick\":\"600000\",\"kind\":\"authored-completion\"}",
                       "\"guest_stop\":{\"bound_tick\":\"600000\",\"kind\":\"guest-bound\"}");
    (void)replace_once(report, sizeof changed, "\"guest_tick\":\"530438\"",
                       "\"guest_tick\":\"600000\"");
    (void)replace_once(report, sizeof changed,
                       "\"log_sha256\":\"86594505bea53c9843f9eaf07e4cf0121cbe0055670324674fd8385c88ffd18e\"",
                       "\"log_sha256\":\"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855\"");
    (void)replace_once(report, sizeof changed, "\"log_bytes\":\"30\"",
                       "\"log_bytes\":\"0\"");
    (void)replace_once(report, sizeof changed, "\"log_bytes_total\":\"30\"",
                       "\"log_bytes_total\":\"0\"");
    (void)replace_once(report, sizeof changed, "\"outcome\":\"pass\"",
                       "\"outcome\":\"boot-fail\"");
    (void)replace_once(report, sizeof changed,
                       "\"detail\":\"positive-match\"",
                       "\"detail\":\"empty-output\"");
    (void)expect_case(argv[1], "empty output at guest bound is boot-fail",
                      report, 0, NULL);
  }

  if (make_variant(changed, sizeof changed,
                   ",{\"guest_tick\":\"530438\",\"kind\":\"hello-completion\",\"pc\":\"ffffffff80000478\"}",
                   "")) {
    (void)replace_once(changed, sizeof changed,
                       "\"guest_stop\":{\"bound_tick\":\"600000\",\"kind\":\"authored-completion\"}",
                       "\"guest_stop\":{\"bound_tick\":\"600000\",\"kind\":\"unsupported-execution\"}");
    (void)replace_once(changed, sizeof changed, "\"guest_tick\":\"530438\"",
                       "\"guest_tick\":\"530174\"");
    (void)replace_once(changed, sizeof changed,
                       "\"instruction_count\":\"530437\"",
                       "\"instruction_count\":\"530173\"");
    (void)replace_once(changed, sizeof changed, "\"outcome\":\"pass\"",
                       "\"outcome\":\"boot-fail\"");
    (void)replace_once(changed, sizeof changed,
                       "\"detail\":\"positive-match\"",
                       "\"detail\":\"unsupported-execution\"");
    (void)expect_case(argv[1], "unsupported execution before completion is boot-fail",
                      changed, 0, NULL);
  }

  if (make_variant(changed, sizeof changed,
                   ",{\"guest_tick\":\"530438\",\"kind\":\"hello-completion\",\"pc\":\"ffffffff80000478\"}",
                   "")) {
    (void)replace_once(changed, sizeof changed,
                       "\"guest_stop\":{\"bound_tick\":\"600000\",\"kind\":\"authored-completion\"}",
                       "\"guest_stop\":{\"bound_tick\":\"600000\",\"kind\":\"guest-bound\"}");
    (void)replace_once(changed, sizeof changed, "\"guest_tick\":\"530438\"",
                       "\"guest_tick\":\"600000\"");
    (void)replace_once(changed, sizeof changed,
                       "\"log_sha256\":\"86594505bea53c9843f9eaf07e4cf0121cbe0055670324674fd8385c88ffd18e\"",
                       "\"log_sha256\":\"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc\"");
    (void)replace_once(changed, sizeof changed, "\"log_bytes\":\"30\"",
                       "\"log_bytes\":\"5\"");
    (void)replace_once(changed, sizeof changed, "\"log_bytes_total\":\"30\"",
                       "\"log_bytes_total\":\"5\"");
    (void)replace_once(changed, sizeof changed, "\"outcome\":\"pass\"",
                       "\"outcome\":\"timeout\"");
    (void)replace_once(changed, sizeof changed,
                       "\"detail\":\"positive-match\"",
                       "\"detail\":\"guest-bound\"");
    (void)expect_case(argv[1], "non-empty incomplete output at guest bound is timeout",
                      changed, 0, NULL);
  }

  if (make_variant(changed, sizeof changed,
                   "\"log_bytes\":\"30\"",
                   "\"log_bytes\":\"4096\"")) {
    (void)replace_once(changed, sizeof changed,
                       "\"log_bytes_total\":\"30\"",
                       "\"log_bytes_total\":\"4097\"");
    (void)replace_once(changed, sizeof changed,
                       "\"guest_stop\":{\"bound_tick\":\"600000\",\"kind\":\"authored-completion\"}",
                       "\"guest_stop\":{\"bound_tick\":\"600000\",\"kind\":\"output-overflow\"}");
    (void)replace_once(changed, sizeof changed, "\"outcome\":\"pass\"",
                       "\"outcome\":\"harness-error\"");
    (void)replace_once(changed, sizeof changed,
                       "\"detail\":\"positive-match\"",
                       "\"detail\":\"output-overflow\"");
    (void)expect_case(argv[1], "output overflow is a harness error",
                      changed, 0, NULL);
  }

  (void)expect_case(argv[1], "accepts null-only prelaunch missing-fixture record",
                    prelaunch_missing_fixture, 0, NULL);
  if (make_variant(changed, sizeof changed,
                   "\"state_digest\":\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\"",
                   "\"state_digest\":null")) {
    (void)expect_case(argv[1], "rejects partially null launched identities",
                      changed, 1, "R-NULL-RULES");
  }
  if (make_variant(changed, sizeof changed, "{\"build_id\":",
                   "{ \"build_id\":")) {
    (void)expect_case(argv[1], "rejects noncanonical whitespace after parsing",
                      changed, 1, "R-NOT-CANONICAL");
  }

  printf("1..%u\n", test_count);
  return failure_count == 0u ? 0 : 1;
}
