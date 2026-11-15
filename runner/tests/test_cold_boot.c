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

static char profile_a[] = "ntsc-cart-4m-openipl3-rdram-a-t1-v1";
static char profile_b[] = "ntsc-cart-4m-openipl3-rdram-b-t1-v1";
static char warm_profile[] = "ntsc-cart-4m-openipl3-warm-v1";
static char arg_probe[] = "--probe";
static char arg_profile[] = "--profile";
static char arg_fixture[] = "--fixture";
static char arg_compare[] = "--compare";
static char arg_profile_a[] = "--profile-a";
static char arg_profile_b[] = "--profile-b";
#define FIXTURE_BYTES 4280u
#define CAPTURE_BYTES 16384u

typedef struct {
  char output[CAPTURE_BYTES];
  size_t length;
  int exit_code;
} child_result;

static bool run_child(char *runner, char *const args[],
                      child_result *result) {
  int descriptors[2];
  pid_t child;
  int wait_status;
  size_t offset = 0u;
  if (runner == NULL || args == NULL || result == NULL ||
      pipe(descriptors) != 0) return false;
  child = fork();
  if (child < 0) {
    (void)close(descriptors[0]);
    (void)close(descriptors[1]);
    return false;
  }
  if (child == 0) {
    (void)close(descriptors[0]);
    if (dup2(descriptors[1], STDOUT_FILENO) < 0 ||
        dup2(descriptors[1], STDERR_FILENO) < 0) _exit(126);
    (void)close(descriptors[1]);
    execv(runner, args);
    _exit(127);
  }
  (void)close(descriptors[1]);
  while (offset + 1u < sizeof result->output) {
    ssize_t count = read(descriptors[0], result->output + offset,
                         sizeof result->output - offset - 1u);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) break;
    offset += (size_t)count;
  }
  (void)close(descriptors[0]);
  while (waitpid(child, &wait_status, 0) < 0) {
    if (errno != EINTR) return false;
  }
  result->length = offset;
  result->output[offset] = '\0';
  result->exit_code = WIFEXITED(wait_status) ? WEXITSTATUS(wait_status) : 128;
  return true;
}

static bool expect(const char *name, bool condition) {
  (void)printf("%s %s\n", condition ? "ok" : "not ok", name);
  return condition;
}

static bool read_fixture(const char *path, uint8_t bytes[FIXTURE_BYTES]) {
  struct stat metadata;
  size_t offset = 0u;
  int fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
  if (fd < 0) return false;
  if (fstat(fd, &metadata) != 0 || !S_ISREG(metadata.st_mode) ||
      metadata.st_size != (off_t)FIXTURE_BYTES) {
    (void)close(fd);
    return false;
  }
  while (offset < FIXTURE_BYTES) {
    ssize_t count = read(fd, bytes + offset, FIXTURE_BYTES - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) {
      (void)close(fd);
      return false;
    }
    offset += (size_t)count;
  }
  return close(fd) == 0;
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

static bool write_text(const char *path, const char *text) {
  FILE *file = fopen(path, "wb");
  bool ok;
  if (file == NULL) return false;
  ok = fputs(text, file) >= 0;
  if (fclose(file) != 0) ok = false;
  return ok;
}

static bool make_evidence_dir(const char *root, char *directory,
                              size_t capacity) {
  int count = snprintf(directory, capacity, "%s/build/gate/06-cold-startup",
                       root);
  if (count <= 0 || (size_t)count >= capacity) return false;
  if (mkdir("build", 0700) != 0 && errno != EEXIST) return false;
  if (mkdir("build/gate", 0700) != 0 && errno != EEXIST) return false;
  if (mkdir(directory, 0700) != 0 && errno != EEXIST) return false;
  return true;
}

static int controls(char *runner) {
  char *fixture = getenv("T64_TEST_HELLO_PATH");
  child_result child = {{0}, 0u, -1};
  char *compare_args[9];
  char *missing_args[] = {runner, arg_probe, arg_profile, profile_a, NULL};
  char *warm_args[7];
  char temporary[] = "/tmp/t64-cold-controls-XXXXXX";
  char mutation_path[512];
  uint8_t bytes[FIXTURE_BYTES];
  bool ok = true;
  int temporary_fd;

  (void)puts("TAP version 13");
  (void)puts("1..6");
  if (!expect("test fixture is supplied", fixture != NULL)) return 1;
  compare_args[0] = runner;
  compare_args[1] = arg_compare;
  compare_args[2] = arg_profile_a;
  compare_args[3] = profile_a;
  compare_args[4] = arg_profile_b;
  compare_args[5] = profile_b;
  compare_args[6] = arg_fixture;
  compare_args[7] = fixture;
  compare_args[8] = NULL;
  {
    bool compare_pass = run_child(runner, compare_args, &child) &&
        child.exit_code == 0 &&
        strstr(child.output, "classification=compare-pass") != NULL &&
        strstr(child.output, "comparison=equal") != NULL &&
        strstr(child.output, "terminal=0x80000478/0x0000000d") != NULL &&
        strstr(child.output, "last-retired=0x80000474/0xa50e0016") != NULL &&
        strstr(child.output, "terminal-in-delay-slot=0") != NULL &&
        strstr(child.output,
               "hello-hex=54656e6469657336342068656c6c6f20766961206f70656e2049504c330a") != NULL;
    if (!compare_pass) {
      (void)printf("# production compare response: %s", child.output);
    }
    ok &= expect("compare authenticates both terminal paths and literal output",
                 compare_pass);
  }
  ok &= expect("missing setup is refused before launch",
               run_child(runner, missing_args, &child) && child.exit_code == 2 &&
               strstr(child.output, "reason=usage") != NULL);

  warm_args[0] = runner;
  warm_args[1] = arg_probe;
  warm_args[2] = arg_profile;
  warm_args[3] = warm_profile;
  warm_args[4] = arg_fixture;
  warm_args[5] = fixture;
  warm_args[6] = NULL;
  ok &= expect("historical profile is refused",
               run_child(runner, warm_args, &child) && child.exit_code == 2 &&
               strstr(child.output, "reason=profile-identity") != NULL);

  if (!read_fixture(fixture, bytes) || mkdtemp(temporary) == NULL) {
    (void)expect("mutation control setup succeeds", false);
    return 1;
  }
  bytes[FIXTURE_BYTES - 1u] ^= 1u;
  if (snprintf(mutation_path, sizeof mutation_path, "%s/changed.z64",
               temporary) <= 0) {
    (void)expect("mutation path fits", false);
    return 1;
  }
  temporary_fd = open(mutation_path,
                       O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
  if (temporary_fd < 0 || !write_all(temporary_fd, bytes, sizeof bytes) ||
      close(temporary_fd) != 0) {
    if (temporary_fd >= 0) (void)close(temporary_fd);
    (void)expect("mutation fixture is created", false);
    return 1;
  }
  {
    char *mutation_args[] = {runner, arg_probe, arg_profile,
                             profile_a, arg_fixture, mutation_path,
                             NULL};
    ok &= expect("altered fixture is refused before machine admission",
                 run_child(runner, mutation_args, &child) &&
                 child.exit_code == 2 &&
                 strstr(child.output, "reason=fixture-identity") != NULL);
  }
  (void)unlink(mutation_path);
  (void)rmdir(temporary);

  {
    char *accepted_args[] = {runner, arg_probe, arg_profile,
                             profile_a, arg_fixture, fixture,
                             NULL};
    bool ran = run_child(runner, accepted_args, &child);
    bool exact = ran && child.exit_code == 0 &&
        strstr(child.output, "classification=observed") != NULL &&
        strstr(child.output, "operation=read address=0x0470000c width=4") != NULL &&
        strstr(child.output, "value=0x00000005 pc=0xa4000094 opcode=0x8c42000c") != NULL &&
        strstr(child.output, "tick=") != NULL && strstr(child.output, "stop=") != NULL &&
        strstr(child.output, "output_bytes=") != NULL;
    if (!exact && ran) (void)fputs(child.output, stderr);
    ok &= expect("profile A runs canonical IPL3 and reports exact RI read",
                 exact);
  }
  return ok ? 0 : 1;
}

static int integration(char *runner, char *root) {
  char *fixture = getenv("T64_TEST_HELLO_PATH");
  char command[2048];
  char evidence_dir[1024];
  char command_path[1200];
  char outcome_path[1200];
  char saved_outcome[CAPTURE_BYTES + 128u];
  child_result child = {{0}, 0u, -1};
  char *args[7];
  int count;
  bool saved;
  if (fixture == NULL || root == NULL) {
    (void)puts("not ok cold startup inputs supplied");
    return 1;
  }
  count = snprintf(command, sizeof command,
                   "%s --probe --profile %s --fixture %s\n", runner,
                   profile_b, fixture);
  if (count <= 0 || (size_t)count >= sizeof command ||
      !make_evidence_dir(root, evidence_dir, sizeof evidence_dir)) {
    (void)puts("not ok cold startup evidence directory prepared");
    return 1;
  }
  count = snprintf(command_path, sizeof command_path, "%s/command.txt",
                   evidence_dir);
  if (count <= 0 || (size_t)count >= sizeof command_path ||
      !write_text(command_path, command)) {
    (void)puts("not ok exact probe command saved");
    return 1;
  }
  args[0] = runner;
  args[1] = arg_probe;
  args[2] = arg_profile;
  args[3] = profile_b;
  args[4] = arg_fixture;
  args[5] = fixture;
  args[6] = NULL;
  if (!run_child(runner, args, &child)) {
    (void)puts("not ok single cold startup invocation completed");
    return 1;
  }
  count = snprintf(saved_outcome, sizeof saved_outcome,
                   "exit_code=%d\n%s", child.exit_code, child.output);
  if (count <= 0 || (size_t)count >= sizeof saved_outcome) return 1;
  count = snprintf(outcome_path, sizeof outcome_path, "%s/outcome.txt",
                   evidence_dir);
  saved = count > 0 && (size_t)count < sizeof outcome_path &&
          write_text(outcome_path, saved_outcome);
  (void)fputs(child.output, stdout);
  if (!expect("original bounded probe result retained before evaluation", saved))
    return 1;
  if (!expect("Tier B cold operation reached once",
              child.exit_code == 0 &&
              strstr(child.output, "classification=observed") != NULL &&
              strstr(child.output, "address=0x0470000c") != NULL &&
              strstr(child.output, "value=0x00000000") != NULL)) {
    return 1;
  }
  return 0;
}

int main(int argc, char **argv) {
  if (argc == 3 && strcmp(argv[1], "--controls") == 0) {
    return controls(argv[2]);
  }
  if (argc == 4 && strcmp(argv[1], "--integration") == 0) {
    return integration(argv[2], argv[3]);
  }
  (void)puts("not ok test invocation is valid");
  return 2;
}
