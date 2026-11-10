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

#include "t64_test.h"
#include "tendies64/t64_machine.h"

#define RECORD_CAPACITY 4096u

static const char expected_profile_hash[] =
    "b7d1600ad960b4e04fdc3ec372675edc47b26b15be8e9cd84cbab4fd75835647";
static const char expected_content_digest[] =
    "658c40a86a711053f3a36d29993181811cce283a623a6e7b1fee4003649cfe3b";
static const char expected_state_digest[] =
    "63afb49d9669dea05112953b01c369def27d242787825823d8a310815cf7dfb6";

static int run_process(const char *const input_args[], const char *capture_path,
                       const char *build_dir) {
  char *args[16];
  size_t argument_count = 0u;
  pid_t child;
  int status = 0;
  pid_t waited;

  while (input_args[argument_count] != NULL) {
    if (argument_count + 1u >= sizeof args / sizeof args[0]) {
      return 127;
    }
    args[argument_count] = strdup(input_args[argument_count]);
    if (args[argument_count] == NULL) {
      while (argument_count > 0u) {
        free(args[--argument_count]);
      }
      return 127;
    }
    argument_count++;
  }
  args[argument_count] = NULL;
  child = fork();

  if (child < 0) {
    while (argument_count > 0u) {
      free(args[--argument_count]);
    }
    return 127;
  }
  if (child == 0) {
    int capture_fd = open(capture_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (capture_fd < 0 || dup2(capture_fd, STDOUT_FILENO) < 0 ||
        dup2(capture_fd, STDERR_FILENO) < 0) {
      _exit(127);
    }
    (void)close(capture_fd);
    if (build_dir != NULL && setenv("T64_BUILD_DIR", build_dir, 1) != 0) {
      _exit(127);
    }
    execv(args[0], args);
    _exit(127);
  }
  do {
    waited = waitpid(child, &status, 0);
  } while (waited < 0 && errno == EINTR);
  if (waited != child || !WIFEXITED(status)) {
    while (argument_count > 0u) {
      free(args[--argument_count]);
    }
    return 127;
  }
  status = WEXITSTATUS(status);
  while (argument_count > 0u) {
    free(args[--argument_count]);
  }
  return status;
}

static bool read_file(const char *path, char *buffer, size_t capacity,
                      size_t *length_out) {
  FILE *file = fopen(path, "rb");
  size_t length;

  if (file == NULL) {
    return false;
  }
  length = fread(buffer, 1u, capacity - 1u, file);
  if (ferror(file) || fclose(file) != 0) {
    return false;
  }
  buffer[length] = '\0';
  *length_out = length;
  return true;
}

static bool write_file(const char *path, const char *bytes, size_t length) {
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  size_t offset = 0u;

  if (fd < 0) {
    return false;
  }
  while (offset < length) {
    ssize_t written = write(fd, bytes + offset, length - offset);
    if (written < 0 && errno == EINTR) {
      continue;
    }
    if (written <= 0) {
      (void)close(fd);
      return false;
    }
    offset += (size_t)written;
  }
  return close(fd) == 0;
}

static bool file_is_empty(const char *path) {
  struct stat info;
  return stat(path, &info) == 0 && info.st_size == 0;
}

static bool output_contains(const char *path, const char *needle) {
  char output[4096];
  size_t length = 0u;
  return read_file(path, output, sizeof output, &length) &&
         strstr(output, needle) != NULL;
}

static size_t replace_once(const char *source, const char *needle,
                           const char *replacement, char *output,
                           size_t capacity) {
  const char *found = strstr(source, needle);
  size_t prefix;
  size_t replacement_length;
  size_t suffix_length;
  size_t total;

  if (found == NULL) {
    return 0u;
  }
  prefix = (size_t)(found - source);
  replacement_length = strlen(replacement);
  suffix_length = strlen(found + strlen(needle));
  total = prefix + replacement_length + suffix_length;
  if (total + 1u > capacity) {
    return 0u;
  }
  (void)memcpy(output, source, prefix);
  (void)memcpy(output + prefix, replacement, replacement_length);
  (void)memcpy(output + prefix + replacement_length,
               found + strlen(needle), suffix_length + 1u);
  return total;
}

static int probe(const char *tool, const char *profile,
                 const char *record_path, const char *output_path) {
  const char *args[] = {tool, "--profile", profile, "--out", record_path,
                        NULL};
  return run_process(args, output_path, NULL);
}

static int model_check(const char *checker, const char *record_path,
                       const char *output_path) {
  const char *args[] = {checker, "result", record_path, NULL};
  return run_process(args, output_path, NULL);
}

static int result_check(const char *checker, const char *const args_tail[],
                        const char *output_path) {
  const char *args[10];
  size_t i = 0u;
  args[i++] = checker;
  while (args_tail[i - 1u] != NULL && i < sizeof args / sizeof args[0]) {
    args[i] = args_tail[i - 1u];
    i++;
  }
  if (i >= sizeof args / sizeof args[0]) {
    return 127;
  }
  args[i] = NULL;
  return run_process(args, output_path, NULL);
}

static bool write_claim(const char *path, const char *evidence_path) {
  static const char template_text[] =
      "# Test evidence\n\n"
      "```t64-claim\n"
      "build_id: 0.1.0\n"
      "profile_id: ntsc-cart-4m-skeleton\n"
      "tier: unit-tested\n"
      "fixture_set: model-probe-test\n"
      "manifest_digest: sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
      "pass: 0\n"
      "total: 0\n"
      "guest_fail: 0\n"
      "boot_fail: 0\n"
      "timeout: 0\n"
      "missing: 0\n"
      "harness_error: 0\n"
      "skipped: 0\n"
      "oracle_kind: native-model-probe\n"
      "oracle_revision: v1\n"
      "not_claimed: guest outcomes, hardware behavior or commercial software\n"
      "statement: Tendies64 0.1.0 on profile ntsc-cart-4m-skeleton: unit-tested for model-probe-test @ sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef (0/0; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: native-model-probe v1. Not claimed: guest outcomes, hardware behavior or commercial software.\n"
      "evidence: %s\n"
      "```\n";
  char text[2048];
  int length = snprintf(text, sizeof text, template_text, evidence_path);
  return length > 0 && (size_t)length < sizeof text &&
         write_file(path, text, (size_t)length);
}

static void test_expected_result(const char *record, const char *record_path,
                                 const char *model_checker,
                                 const char *capture_path) {
  char expected_record[RECORD_CAPACITY];
  char mismatched[RECORD_CAPACITY];
  char changed[RECORD_CAPACITY];
  size_t mismatched_length;
  int expected_length;

  T64_CHECK(strncmp(t64_build_identity(), "t64-build-v1-", 13u) == 0);
  T64_CHECK(strlen(t64_build_identity()) == 77u);
  expected_length = snprintf(
      expected_record, sizeof expected_record,
      "{\"assertions\":{\"checked\":94,\"failed\":0},"
      "\"build_id\":\"%s\",\"content_digest\":\"%s\","
      "\"evidence_tier\":\"unit-tested\",\"guest_tick\":\"22\","
      "\"model_scope\":\"native-model-probe\","
      "\"outcome\":\"model-match\",\"profile_hash\":\"%s\","
      "\"profile_id\":\"ntsc-cart-4m-skeleton\","
      "\"schema\":\"t64.model-result\",\"schema_version\":1,"
      "\"state_digest\":\"%s\"}\n",
      t64_build_identity(), expected_content_digest, expected_profile_hash,
      expected_state_digest);
  T64_CHECK(expected_length > 0 &&
            (size_t)expected_length < sizeof expected_record);
  if (expected_length > 0 && (size_t)expected_length < sizeof expected_record) {
    T64_CHECK_STR(record, expected_record);
  }
  T64_CHECK_INT(model_check(model_checker, record_path, capture_path), 0);
  T64_CHECK(output_contains(capture_path,
                            "valid schema=t64.model-result/1 model_scope=native-model-probe"));

  /* Test-owned expected-value mutation proves a completed mismatch is scoped. */
  mismatched_length = replace_once(record, "\"failed\":0", "\"failed\":1",
                                   changed, sizeof changed);
  T64_CHECK(mismatched_length > 0u);
  if (mismatched_length > 0u) {
    mismatched_length = replace_once(changed, "\"outcome\":\"model-match\"",
                                     "\"outcome\":\"model-mismatch\"",
                                     mismatched, sizeof mismatched);
  }
  T64_CHECK(mismatched_length > 0u);
  if (mismatched_length > 0u) {
    T64_CHECK(write_file(record_path, mismatched, mismatched_length));
    T64_CHECK_INT(model_check(model_checker, record_path, capture_path), 0);
    T64_CHECK(output_contains(capture_path, "outcome=model-mismatch"));
    T64_CHECK(write_file(record_path, record, strlen(record)));
  }
}

static void test_unknown_profiles(const char *tool, const char *record_path,
                                  const char *capture_path) {
  T64_CHECK(write_file(record_path, "", 0u));
  T64_CHECK_INT(probe(tool, "", record_path, capture_path), 2);
  T64_CHECK(file_is_empty(record_path));
  T64_CHECK(write_file(record_path, "", 0u));
  T64_CHECK_INT(probe(tool, "unknown-profile", record_path, capture_path), 3);
  T64_CHECK(file_is_empty(record_path));
}

static void test_legacy_rejection(const char *result_checker,
                                  const char *model_checker,
                                  const char *record_path,
                                  const char *record,
                                  const char *positive_result,
                                  const char *good_suite,
                                  const char *inventory,
                                  const char *capture_path,
                                  const char *sealed_path,
                                  const char *suite_path,
                                  const char *suite_with_footer,
                                  size_t suite_with_footer_length) {
  const char *result_args[] = {"result", record_path, NULL};
  const char *identity_args[] = {"identity", record_path, NULL};
  const char *seal_args[] = {"seal", "--inventory", inventory, "--out",
                             sealed_path, record_path, NULL};
  const char *identity_suite_args[] = {"identity", suite_path, NULL};
  const char *suite_args[] = {"suite", "--inventory", inventory, suite_path,
                              NULL};
  const char *positive_result_args[] = {"result", positive_result, NULL};
  const char *positive_identity_args[] = {"identity", good_suite, NULL};
  const char *positive_suite_args[] = {"suite", "--inventory", inventory,
                                       good_suite, NULL};
  char *changed = NULL;
  size_t changed_length;
  char retagged_path[] = "/tmp/t64-model-retagged-XXXXXX";
  char version2_path[] = "/tmp/t64-model-version2-XXXXXX";
  char version2[RECORD_CAPACITY];
  size_t version2_length;
  int retagged_fd;
  int version2_fd;

  T64_CHECK_INT(result_check(result_checker, result_args, capture_path) != 0,
                1);
  T64_CHECK(!output_contains(capture_path, "valid schema=t64.result/1"));
  T64_CHECK_INT(result_check(result_checker, identity_args, capture_path) != 0,
                1);
  T64_CHECK_INT(result_check(result_checker, seal_args, capture_path) != 0, 1);
  T64_CHECK(file_is_empty(sealed_path) || access(sealed_path, F_OK) != 0);

  T64_CHECK(write_file(suite_path, suite_with_footer,
                       suite_with_footer_length));
  T64_CHECK_INT(result_check(result_checker, identity_suite_args,
                             capture_path) != 0,
                1);
  T64_CHECK_INT(result_check(result_checker, suite_args, capture_path) != 0,
                1);

  version2_fd = mkstemp(version2_path);
  T64_CHECK(version2_fd >= 0);
  if (version2_fd >= 0) {
    (void)close(version2_fd);
    version2_length = replace_once(record, "\"schema_version\":1",
                                   "\"schema_version\":2", version2,
                                   sizeof version2);
    T64_CHECK(version2_length > 0u);
    if (version2_length > 0u) {
      T64_CHECK(write_file(version2_path, version2, version2_length));
      T64_CHECK_INT(model_check(model_checker, version2_path, capture_path) !=
                        0,
                    1);
    }
    (void)unlink(version2_path);
  }

  retagged_fd = mkstemp(retagged_path);
  T64_CHECK(retagged_fd >= 0);
  if (retagged_fd >= 0) {
    (void)close(retagged_fd);
    changed = (char *)malloc(RECORD_CAPACITY);
    T64_CHECK(changed != NULL);
    if (changed != NULL) {
      changed_length = replace_once(record, "t64.model-result", "t64.result",
                                    changed, RECORD_CAPACITY);
      T64_CHECK(changed_length > 0u);
      if (changed_length > 0u) {
        T64_CHECK(write_file(retagged_path, changed, changed_length));
        result_args[1] = retagged_path;
        T64_CHECK_INT(result_check(result_checker, result_args, capture_path) !=
                          0,
                      1);
      }
      free(changed);
    }
    (void)unlink(retagged_path);
  }

  T64_CHECK_INT(result_check(result_checker, positive_result_args,
                             capture_path),
                0);
  T64_CHECK_INT(result_check(result_checker, positive_identity_args,
                             capture_path),
                0);
  T64_CHECK_INT(result_check(result_checker, positive_suite_args,
                             capture_path),
                0);
}

static void test_claims_rejection(const char *claims_script,
                                  const char *source_root,
                                  const char *build_root,
                                  const char *record,
                                  const char *footer, size_t footer_length,
                                  const char *capture_path) {
  char root_template[] = "/tmp/t64-claims-model-XXXXXX";
  char *root = mkdtemp(root_template);
  char docs_dir[512];
  char evidence_dir[512];
  char record_path[512];
  char footer_path[512];
  char direct_claim_path[512];
  char footer_claim_path[512];
  const char *direct_args[8];
  const char *footer_args[8];
  char combined[RECORD_CAPACITY * 2u];
  size_t record_length = strlen(record);
  size_t combined_length;

  T64_CHECK(root != NULL);
  if (root == NULL) {
    return;
  }
  (void)snprintf(docs_dir, sizeof docs_dir, "%s/docs", root);
  (void)snprintf(evidence_dir, sizeof evidence_dir, "%s/evidence", root);
  (void)snprintf(record_path, sizeof record_path, "%s/model.jsonl", evidence_dir);
  (void)snprintf(footer_path, sizeof footer_path, "%s/model-footer.jsonl",
                 evidence_dir);
  (void)snprintf(direct_claim_path, sizeof direct_claim_path,
                 "%s/model-direct.md", docs_dir);
  (void)snprintf(footer_claim_path, sizeof footer_claim_path,
                 "%s/model-footer.md", docs_dir);
  T64_CHECK_INT(mkdir(docs_dir, 0700), 0);
  T64_CHECK_INT(mkdir(evidence_dir, 0700), 0);
  T64_CHECK(write_file(record_path, record, record_length));
  combined_length = record_length + footer_length;
  T64_CHECK(combined_length < sizeof combined);
  if (combined_length < sizeof combined) {
    (void)memcpy(combined, record, record_length);
    (void)memcpy(combined + record_length, footer, footer_length);
    T64_CHECK(write_file(footer_path, combined, combined_length));
  }
  T64_CHECK(write_claim(direct_claim_path, "evidence/model.jsonl"));
  T64_CHECK(write_claim(footer_claim_path, "evidence/model-footer.jsonl"));

  direct_args[0] = "/bin/sh";
  direct_args[1] = claims_script;
  direct_args[2] = "--root";
  direct_args[3] = root;
  direct_args[4] = "--files";
  direct_args[5] = direct_claim_path;
  direct_args[6] = NULL;
  T64_CHECK_INT(run_process(direct_args, capture_path, build_root) != 0, 1);
  T64_CHECK(output_contains(capture_path, "C-EVIDENCE-INVALID"));
  T64_CHECK(!output_contains(capture_path, "ok\n"));

  footer_args[0] = "/bin/sh";
  footer_args[1] = claims_script;
  footer_args[2] = "--root";
  footer_args[3] = root;
  footer_args[4] = "--files";
  footer_args[5] = footer_claim_path;
  footer_args[6] = NULL;
  T64_CHECK_INT(run_process(footer_args, capture_path, build_root) != 0, 1);
  T64_CHECK(output_contains(capture_path, "C-EVIDENCE-INVALID"));
  T64_CHECK(!output_contains(capture_path, "ok\n"));
  (void)source_root;
  (void)unlink(record_path);
  (void)unlink(footer_path);
  (void)unlink(direct_claim_path);
  (void)unlink(footer_claim_path);
  (void)rmdir(evidence_dir);
  (void)rmdir(docs_dir);
  (void)rmdir(root);
}

int main(int argc, char **argv) {
  char record_path[] = "/tmp/t64-model-probe-record-XXXXXX";
  char capture_path[] = "/tmp/t64-model-probe-output-XXXXXX";
  char sealed_path[] = "/tmp/t64-model-probe-seal-XXXXXX";
  char suite_path[] = "/tmp/t64-model-probe-suite-XXXXXX";
  char record[RECORD_CAPACITY];
  char suite[RECORD_CAPACITY];
  char footer[512];
  size_t record_length = 0u;
  size_t suite_length = 0u;
  size_t footer_start;
  size_t footer_end;
  size_t footer_length;
  size_t suite_with_footer_length;
  char suite_with_footer[RECORD_CAPACITY];
  int record_fd;
  int capture_fd;
  int sealed_fd;
  int suite_fd;

  if (argc != 10) {
    return 2;
  }
  record_fd = mkstemp(record_path);
  capture_fd = mkstemp(capture_path);
  sealed_fd = mkstemp(sealed_path);
  suite_fd = mkstemp(suite_path);
  T64_CHECK(record_fd >= 0);
  T64_CHECK(capture_fd >= 0);
  T64_CHECK(sealed_fd >= 0);
  T64_CHECK(suite_fd >= 0);
  if (record_fd < 0 || capture_fd < 0 || sealed_fd < 0 || suite_fd < 0) {
    return T64_TEST_FINISH("unit.core.profile-result");
  }
  (void)close(record_fd);
  (void)close(capture_fd);
  (void)close(sealed_fd);
  (void)close(suite_fd);

  T64_CHECK_INT(probe(argv[1], "ntsc-cart-4m-skeleton", record_path,
                      capture_path),
                0);
  T64_CHECK(read_file(record_path, record, sizeof record, &record_length));
  if (record_length > 0u) {
    test_expected_result(record, record_path, argv[2], capture_path);
  }
  test_unknown_profiles(argv[1], record_path, capture_path);
  T64_CHECK(write_file(record_path, record, record_length));

  T64_CHECK(read_file(argv[5], suite, sizeof suite, &suite_length));
  footer_end = suite_length;
  while (footer_end > 0u && suite[footer_end - 1u] == '\n') {
    footer_end--;
  }
  footer_start = footer_end;
  while (footer_start > 0u && suite[footer_start - 1u] != '\n') {
    footer_start--;
  }
  footer_length = footer_end - footer_start;
  T64_CHECK(footer_length > 0u && footer_length < sizeof footer - 1u);
  if (footer_length > 0u && footer_length < sizeof footer - 1u) {
    (void)memcpy(footer, suite + footer_start, footer_length);
    footer[footer_length] = '\n';
    footer_length++;
    suite_with_footer_length = record_length + footer_length;
    T64_CHECK(suite_with_footer_length < sizeof suite_with_footer);
    if (suite_with_footer_length < sizeof suite_with_footer) {
      (void)memcpy(suite_with_footer, record, record_length);
      (void)memcpy(suite_with_footer + record_length, footer, footer_length);
      test_legacy_rejection(argv[3], argv[2], record_path, record, argv[4],
                            argv[5], argv[6], capture_path, sealed_path,
                            suite_path, suite_with_footer,
                            suite_with_footer_length);
      test_claims_rejection(argv[7], argv[8], argv[9], record, footer,
                            footer_length, capture_path);
    }
  }

  (void)unlink(record_path);
  (void)unlink(capture_path);
  (void)unlink(sealed_path);
  (void)unlink(suite_path);
  return T64_TEST_FINISH("unit.core.profile-result");
}
