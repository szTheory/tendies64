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
    "\"profile_digest\":\"08634481792748ade0e64073b9cd51d5972ec3a56421278b3203979087b4e05d\","
    "\"profile_id\":\"ntsc-cart-4m-openipl3-warm-v1\","
    "\"schema\":\"t64.result\","
    "\"schema_version\":2,"
    "\"source_revision\":\"a69d215a06968d1552494222a8c9536c304ee537\","
    "\"state_digest\":\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\"}\n";

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

int main(int argc, char **argv) {
  char report_path[] = "/tmp/t64-result-v2-XXXXXX";
  char diagnostic[1024];
  char rule[64] = "none";
  int report_fd;
  int checker_exit = -1;
  int good;

  if (argc != 2) {
    fprintf(stderr, "usage: test_result_v2 T64_RESULT_CHECK\n");
    return 2;
  }
  report_fd = mkstemp(report_path);
  if (report_fd < 0 ||
      !write_all(report_fd, valid_pass_v2, sizeof valid_pass_v2 - 1u) ||
      close(report_fd) != 0) {
    if (report_fd >= 0) {
      (void)close(report_fd);
    }
    (void)unlink(report_path);
    fprintf(stderr, "unable to prepare independent v2 report\n");
    return 2;
  }

  good = run_checker(argv[1], report_path, &checker_exit, diagnostic,
                     sizeof diagnostic);
  (void)unlink(report_path);
  if (!good) {
    fprintf(stderr, "unable to execute result checker\n");
    return 2;
  }
  if (diagnostic[0] != '\0') {
    size_t i = 0;
    while (diagnostic[i] != '\0' && diagnostic[i] != ' ' && i + 1u < sizeof rule) {
      rule[i] = diagnostic[i];
      i++;
    }
    rule[i] = '\0';
  }

  puts("TAP version 13");
  puts("1..1");
  if (checker_exit == 0) {
    puts("ok 1 - accepts independently authored canonical real v2 pass");
    return 0;
  }
  printf("not ok 1 - accepts independently authored canonical real v2 pass\n");
  printf("# expected: checker exit=0\n# actual: checker exit=%d rule=%s\n",
         checker_exit, rule);
  return 1;
}
