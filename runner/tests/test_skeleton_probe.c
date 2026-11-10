#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "t64_test.h"

static int run(const char *tool, const char *profile, const char *record_path,
               const char *output_path) {
  pid_t child = fork();
  int status = 0;
  pid_t waited;

  if (child < 0) {
    return 127;
  }
  if (child == 0) {
    int output_fd = open(output_path, O_WRONLY | O_TRUNC);
    if (output_fd < 0 || dup2(output_fd, STDOUT_FILENO) < 0 ||
        dup2(output_fd, STDERR_FILENO) < 0) {
      _exit(127);
    }
    (void)close(output_fd);
    execl(tool, tool, "--profile", profile, "--out", record_path,
          (char *)NULL);
    _exit(127);
  }
  do {
    waited = waitpid(child, &status, 0);
  } while (waited < 0 && errno == EINTR);
  if (waited != child || !WIFEXITED(status)) {
    return 127;
  }
  return WEXITSTATUS(status);
}

static int check(const char *tool, const char *record_path,
                 const char *output_path) {
  pid_t child = fork();
  int status = 0;
  pid_t waited;

  if (child < 0) {
    return 127;
  }
  if (child == 0) {
    int output_fd = open(output_path, O_WRONLY | O_TRUNC);
    if (output_fd < 0 || dup2(output_fd, STDOUT_FILENO) < 0 ||
        dup2(output_fd, STDERR_FILENO) < 0) {
      _exit(127);
    }
    (void)close(output_fd);
    execl(tool, tool, "result", record_path, (char *)NULL);
    _exit(127);
  }
  do {
    waited = waitpid(child, &status, 0);
  } while (waited < 0 && errno == EINTR);
  if (waited != child || !WIFEXITED(status)) {
    return 127;
  }
  return WEXITSTATUS(status);
}

int main(int argc, char **argv) {
  char record_path[] = "/tmp/t64-model-probe-record-XXXXXX";
  char output_path[] = "/tmp/t64-model-probe-output-XXXXXX";
  int record_fd;
  int output_fd;

  if (argc != 3) {
    return 2;
  }
  record_fd = mkstemp(record_path);
  output_fd = mkstemp(output_path);
  T64_CHECK(record_fd >= 0);
  T64_CHECK(output_fd >= 0);
  if (record_fd < 0 || output_fd < 0) {
    return T64_TEST_FINISH("unit.core.profile-result");
  }
  (void)close(record_fd);
  (void)close(output_fd);

  T64_CHECK_INT(run(argv[1], "ntsc-cart-4m-skeleton", record_path,
                    output_path),
                0);
  T64_CHECK_INT(check(argv[2], record_path, output_path), 0);

  (void)unlink(record_path);
  (void)unlink(output_path);
  return T64_TEST_FINISH("unit.core.profile-result");
}
