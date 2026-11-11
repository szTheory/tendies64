#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "t64_input.h"
#include "t64_num.h"
#include "t64_sha256.h"
#include "t64_verified_input.h"
#include "tendies64/t64_machine.h"

#define FIXTURE_SIZE 4280u
#define FIXTURE_SHA256 \
  "fa276cc175a4e764b6fd5413193c44b51f38fd8135e33e400032827e02f27c7c"
#define FIXTURE_LIMIT T64_VERIFIED_FIXTURE_MAX_BYTES
#define INPUT_LIMIT T64_VERIFIED_SCHEDULE_MAX_BYTES
#define DEFAULT_TEST_COUNT 23u
#define REPLACEMENT_TEST_COUNT 13u

enum {
  TEST_FAULT_TEMP_OPEN = 1,
  TEST_FAULT_COPY_INTERRUPT = 2,
  TEST_FAULT_COPY_SHORT = 3,
  TEST_FAULT_FINALIZE = 4,
  TEST_FAULT_REOPEN = 5,
  TEST_FAULT_READBACK = 6,
  TEST_FAULT_READBACK_HASH = 7,
  TEST_FAULT_UNLINK = 8
};

extern void t64_verified_input_test_fail_once(int stage);

typedef struct {
  pid_t pid;
  int ready_fd;
  int gate_fd;
} child_control;

typedef struct {
  size_t outstanding;
} allocator_state;

static unsigned test_number;
static unsigned failures;

static void check(int condition, const char *name) {
  test_number++;
  if (condition) {
    printf("ok %u - %s\n", test_number, name);
  } else {
    printf("not ok %u - %s\n", test_number, name);
    failures++;
  }
}

static int count_open_fds(void) {
  int fd;
  int count = 0;
  for (fd = 0; fd < 1024; fd++) {
    if (fcntl(fd, F_GETFD) >= 0) count++;
  }
  return count;
}

static int count_entries(const char *directory) {
  DIR *dir = opendir(directory);
  struct dirent *entry;
  int count = 0;
  if (dir == NULL) return -1;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
      count++;
  }
  closedir(dir);
  return count;
}

static int hash_hex(const uint8_t *bytes, size_t length,
                    char hex[T64_SHA256_HEX_SIZE]) {
  t64_sha256_ctx hash;
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  t64_sha256_init(&hash);
  t64_sha256_update(&hash, bytes, length);
  t64_sha256_final(&hash, digest);
  t64_sha256_hex(digest, hex);
  return 0;
}

static int write_all(int fd, const uint8_t *bytes, size_t length) {
  size_t offset = 0u;
  while (offset < length) {
    ssize_t written = write(fd, bytes + offset, length - offset);
    if (written < 0 && errno == EINTR) continue;
    if (written <= 0) return -1;
    offset += (size_t)written;
  }
  return 0;
}

static int read_path(const char *path, size_t maximum, uint8_t **bytes_out,
                     size_t *length_out) {
  struct stat before;
  struct stat after;
  uint8_t *bytes = NULL;
  int fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
  size_t length;
  size_t offset = 0u;
  if (fd < 0) return -1;
  if (fstat(fd, &before) != 0 || !S_ISREG(before.st_mode) ||
      before.st_size < 0 || (uint64_t)before.st_size > maximum) {
    close(fd);
    return -1;
  }
  length = (size_t)before.st_size;
  bytes = (uint8_t *)malloc(length == 0u ? 1u : length);
  if (bytes == NULL) {
    close(fd);
    return -1;
  }
  while (offset < length) {
    ssize_t count = read(fd, bytes + offset, length - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) goto fail;
    offset += (size_t)count;
  }
  {
    uint8_t extra;
    ssize_t count;
    do {
      count = read(fd, &extra, 1u);
    } while (count < 0 && errno == EINTR);
    if (count != 0) goto fail;
  }
  if (fstat(fd, &after) != 0 || after.st_dev != before.st_dev ||
      after.st_ino != before.st_ino || after.st_size != before.st_size) {
    goto fail;
  }
  close(fd);
  *bytes_out = bytes;
  *length_out = length;
  return 0;
fail:
  close(fd);
  free(bytes);
  return -1;
}

static int copy_path(const char *source, const char *target, size_t maximum) {
  uint8_t *bytes = NULL;
  size_t length = 0u;
  int fd;
  if (source == NULL || read_path(source, maximum, &bytes, &length) != 0)
    return -1;
  fd = open(target, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
  if (fd < 0) {
    free(bytes);
    return -1;
  }
  if (write_all(fd, bytes, length) != 0 || fsync(fd) != 0) {
    close(fd);
    free(bytes);
    unlink(target);
    return -1;
  }
  close(fd);
  free(bytes);
  return 0;
}

static int atomic_replace(const char *path, const uint8_t *bytes, size_t length) {
  char temp[4096];
  int fd;
  int n = snprintf(temp, sizeof temp, "%s.replacement", path);
  if (n <= 0 || (size_t)n >= sizeof temp) return -1;
  fd = open(temp, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
  if (fd < 0) return -1;
  if (write_all(fd, bytes, length) != 0 || fsync(fd) != 0) {
    close(fd);
    unlink(temp);
    return -1;
  }
  if (close(fd) != 0 || rename(temp, path) != 0) {
    unlink(temp);
    return -1;
  }
  return 0;
}

static int create_source_directory(char directory[4096]) {
  char template_path[] = "/tmp/t64-descriptor-test.XXXXXX";
  if (mkdtemp(template_path) == NULL) return -1;
  return snprintf(directory, 4096u, "%s", template_path) < 4096 ? 0 : -1;
}

static int make_source_copies(char directory[4096], char fixture[4096],
                              char input[4096]) {
  const char *fixture_source = getenv("T64_TEST_HELLO_PATH");
  const char *input_source = getenv("T64_TEST_INPUT_PATH");
  if (fixture_source == NULL || input_source == NULL ||
      create_source_directory(directory) != 0 ||
      snprintf(fixture, 4096u, "%s/fixture.z64", directory) >= 4096 ||
      snprintf(input, 4096u, "%s/input.json", directory) >= 4096 ||
      copy_path(fixture_source, fixture, FIXTURE_LIMIT) != 0 ||
      copy_path(input_source, input, INPUT_LIMIT) != 0) {
    return -1;
  }
  return 0;
}

static void remove_source_directory(const char *directory, const char *fixture,
                                    const char *input) {
  if (fixture != NULL && fixture[0] != '\0') unlink(fixture);
  if (input != NULL && input[0] != '\0') unlink(input);
  if (directory != NULL && directory[0] != '\0') rmdir(directory);
}

static int descriptor_is_snapshot(int fd, uint64_t size,
                                  int want_close_on_exec) {
  struct stat st;
  int flags = fcntl(fd, F_GETFL);
  int descriptor_flags = fcntl(fd, F_GETFD);
  if (fd <= STDERR_FILENO || flags < 0 || descriptor_flags < 0 ||
      (flags & O_ACCMODE) != O_RDONLY ||
      ((descriptor_flags & FD_CLOEXEC) != 0) != (want_close_on_exec != 0) ||
      fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0 ||
      (uint64_t)st.st_size != size || st.st_nlink != 0 ||
      (st.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH)) != 0) {
    return 0;
  }
  return 1;
}

static int exact_descriptor_bytes(t64_verified_input *input) {
  uint8_t bytes[FIXTURE_SIZE];
  char hex[T64_SHA256_HEX_SIZE];
  ssize_t count = read(input->fd, bytes, sizeof bytes);
  if (count != (ssize_t)sizeof bytes) return 0;
  (void)hash_hex(bytes, sizeof bytes, hex);
  return strcmp(hex, FIXTURE_SHA256) == 0 &&
         strcmp(hex, input->sha256_hex) == 0 &&
         lseek(input->fd, 0, SEEK_SET) == 0;
}

static int fault_refuses_cleanly(const char *source, const char *directory,
                                 int stage) {
  t64_verified_input unchanged = {-1, 9u, {0u}, {0}, 0u};
  int before = count_open_fds();
  t64_verified_input_test_fail_once(stage);
  t64_verified_input_status status = t64_verified_input_prepare(
      source, FIXTURE_SIZE, FIXTURE_SHA256, FIXTURE_LIMIT, &unchanged);
  t64_verified_input_release(&unchanged);
  return status == T64_VERIFIED_INPUT_ERROR && unchanged.fd == -1 &&
         unchanged.size == 9u && count_open_fds() == before &&
         count_entries(directory) == 1;
}

static bool host_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  allocator_state *state = (allocator_state *)context;
  void *memory;
  if (state == NULL || storage_out == NULL || alignment == 0u) return false;
  memory = malloc(size);
  if (memory == NULL || (uintptr_t)memory % alignment != 0u) {
    free(memory);
    return false;
  }
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  state->outstanding++;
  return true;
}

static void host_release(void *context, t64_host_storage storage) {
  allocator_state *state = (allocator_state *)context;
  if (state != NULL && storage.data != NULL) {
    free(storage.data);
    state->outstanding--;
  }
}

static int wait_for_release(int ready_fd, int gate_fd) {
  uint8_t ready = 'B';
  uint8_t release;
  if (write_all(ready_fd, &ready, 1u) != 0) return -1;
  for (;;) {
    ssize_t count = read(gate_fd, &release, 1u);
    if (count < 0 && errno == EINTR) continue;
    return count == 1 && release == 'R' ? 0 : -1;
  }
}

static int parse_fd_arg(const char *text, int *fd_out) {
  uint64_t value;
  if (!t64_parse_u64(text, strlen(text), &value) || value > INT32_MAX) return -1;
  *fd_out = (int)value;
  return 0;
}

static int parse_size_arg(const char *text, size_t maximum, size_t *size_out) {
  uint64_t value;
  if (!t64_parse_u64(text, strlen(text), &value) || value > maximum) return -1;
  *size_out = (size_t)value;
  return 0;
}

static int read_descriptor_bytes(int fd, size_t length, size_t maximum,
                                 uint8_t **bytes_out) {
  uint8_t *bytes;
  size_t offset = 0u;
  if (length > maximum) return -1;
  bytes = (uint8_t *)malloc(length == 0u ? 1u : length);
  if (bytes == NULL) return -1;
  while (offset < length) {
    ssize_t count = read(fd, bytes + offset, length - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) {
      free(bytes);
      return -1;
    }
    offset += (size_t)count;
  }
  {
    uint8_t extra;
    ssize_t count;
    do {
      count = read(fd, &extra, 1u);
    } while (count < 0 && errno == EINTR);
    if (count != 0) {
      free(bytes);
      return -1;
    }
  }
  *bytes_out = bytes;
  return 0;
}

static int core_admit_fixture(const uint8_t *bytes, size_t length,
                              const t64_input_schedule *schedule) {
  allocator_state state = {0u};
  t64_host_allocator allocator = {host_allocate, host_release, &state};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  size_t rom_size = 0u;
  size_t i;
  int ok = 0;
  if (t64_profile_lookup("ntsc-cart-4m-openipl3-warm-v1", &profile) !=
          T64_STATUS_OK || profile == NULL ||
      t64_machine_create(&allocator, profile, &machine) != T64_STATUS_OK ||
      machine == NULL ||
      t64_machine_admit_rom(machine, bytes, length) != T64_STATUS_OK ||
      t64_machine_rom_size(machine, &rom_size) != T64_STATUS_OK ||
      rom_size != length) {
    goto cleanup;
  }
  for (i = 0u; i < t64_input_schedule_event_count(schedule); i++) {
    t64_input_event event;
    if (!t64_input_schedule_event(schedule, i, &event) ||
        t64_machine_queue_input(machine, event.tick, event.port,
                                event.buttons, event.axis_x,
                                event.axis_y) != T64_STATUS_OK) {
      goto cleanup;
    }
  }
  ok = 1;
cleanup:
  if (machine != NULL) t64_machine_destroy(machine);
  return ok && state.outstanding == 0u;
}

static int helper_read_fds(int argc, char **argv) {
  int fixture_fd;
  int input_fd;
  int ready_fd;
  int gate_fd;
  size_t fixture_size;
  size_t input_size;
  uint8_t *fixture_bytes = NULL;
  uint8_t *input_bytes = NULL;
  char fixture_hex[T64_SHA256_HEX_SIZE];
  char input_hex[T64_SHA256_HEX_SIZE];
  char canonical_hex[T64_SHA256_HEX_SIZE];
  t64_input_schedule *schedule = NULL;
  t64_input_error input_error;
  int ok = 0;
  if (argc != 11 || parse_fd_arg(argv[2], &fixture_fd) != 0 ||
      parse_size_arg(argv[3], FIXTURE_LIMIT, &fixture_size) != 0 ||
      parse_fd_arg(argv[5], &input_fd) != 0 ||
      parse_size_arg(argv[6], INPUT_LIMIT, &input_size) != 0 ||
      parse_fd_arg(argv[9], &ready_fd) != 0 ||
      parse_fd_arg(argv[10], &gate_fd) != 0 ||
      !descriptor_is_snapshot(fixture_fd, fixture_size, 0) ||
      !descriptor_is_snapshot(input_fd, input_size, 0) ||
      (fcntl(ready_fd, F_GETFD) & FD_CLOEXEC) != 0 ||
      (fcntl(gate_fd, F_GETFD) & FD_CLOEXEC) != 0) {
    return 2;
  }
  if (wait_for_release(ready_fd, gate_fd) != 0 ||
      read_descriptor_bytes(fixture_fd, fixture_size, FIXTURE_LIMIT,
                            &fixture_bytes) != 0 ||
      read_descriptor_bytes(input_fd, input_size, INPUT_LIMIT,
                            &input_bytes) != 0) {
    goto cleanup;
  }
  (void)hash_hex(fixture_bytes, fixture_size, fixture_hex);
  (void)hash_hex(input_bytes, input_size, input_hex);
  if (strcmp(fixture_hex, argv[4]) != 0 || strcmp(input_hex, argv[7]) != 0 ||
      t64_input_parse(input_bytes, input_size, &schedule, &input_error) !=
          T64_INPUT_OK) {
    goto cleanup;
  }
  t64_sha256_hex(t64_input_schedule_sha256(schedule), canonical_hex);
  if (strcmp(canonical_hex, argv[8]) != 0 ||
      !core_admit_fixture(fixture_bytes, fixture_size, schedule)) {
    goto cleanup;
  }
  ok = 1;
cleanup:
  t64_input_schedule_destroy(schedule);
  free(fixture_bytes);
  free(input_bytes);
  return ok ? 0 : 3;
}

static int helper_read_original(int argc, char **argv) {
  int ready_fd;
  int gate_fd;
  size_t fixture_size;
  size_t input_size;
  size_t observed_size;
  uint8_t *fixture_bytes = NULL;
  uint8_t *input_bytes = NULL;
  char fixture_hex[T64_SHA256_HEX_SIZE];
  char input_hex[T64_SHA256_HEX_SIZE];
  char canonical_hex[T64_SHA256_HEX_SIZE];
  t64_input_schedule *schedule = NULL;
  int ok = 0;
  if (argc != 11 || parse_size_arg(argv[3], FIXTURE_LIMIT, &fixture_size) != 0 ||
      parse_size_arg(argv[6], INPUT_LIMIT, &input_size) != 0 ||
      parse_fd_arg(argv[9], &ready_fd) != 0 ||
      parse_fd_arg(argv[10], &gate_fd) != 0 ||
      (fcntl(ready_fd, F_GETFD) & FD_CLOEXEC) != 0 ||
      (fcntl(gate_fd, F_GETFD) & FD_CLOEXEC) != 0) {
    return 2;
  }
  if (wait_for_release(ready_fd, gate_fd) != 0 ||
      read_path(argv[2], FIXTURE_LIMIT, &fixture_bytes, &observed_size) != 0 ||
      observed_size != fixture_size ||
      read_path(argv[5], INPUT_LIMIT, &input_bytes, &observed_size) != 0 ||
      observed_size != input_size) {
    goto cleanup;
  }
  (void)hash_hex(fixture_bytes, fixture_size, fixture_hex);
  (void)hash_hex(input_bytes, input_size, input_hex);
  if (strcmp(fixture_hex, argv[4]) != 0 || strcmp(input_hex, argv[7]) != 0 ||
      t64_input_parse(input_bytes, input_size, &schedule, NULL) != T64_INPUT_OK) {
    goto cleanup;
  }
  t64_sha256_hex(t64_input_schedule_sha256(schedule), canonical_hex);
  ok = strcmp(canonical_hex, argv[8]) == 0;
cleanup:
  t64_input_schedule_destroy(schedule);
  free(fixture_bytes);
  free(input_bytes);
  return ok ? 0 : 3;
}

static int helper_touch_marker(int argc, char **argv) {
  int fd;
  const uint8_t marker = 'x';
  if (argc < 3) return 2;
  fd = open(argv[2], O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
  if (fd < 0) return 3;
  if (write_all(fd, &marker, 1u) != 0 || close(fd) != 0) return 4;
  return 0;
}

static int helper_fail_with_descendant(int argc, char **argv) {
  int ready_fd;
  int gate_fd;
  int watch_fd;
  pid_t descendant;
  if (argc != 5 || parse_fd_arg(argv[2], &ready_fd) != 0 ||
      parse_fd_arg(argv[3], &gate_fd) != 0 ||
      parse_fd_arg(argv[4], &watch_fd) != 0) {
    return 2;
  }
  descendant = fork();
  if (descendant < 0) return 3;
  if (descendant == 0) {
    pid_t self = getpid();
    close(ready_fd);
    close(gate_fd);
    if (write_all(watch_fd, (const uint8_t *)&self, sizeof self) != 0)
      _exit(5);
    for (;;) pause();
  }
  close(watch_fd);
  if (wait_for_release(ready_fd, gate_fd) != 0) return 4;
  return 9;
}

static int clear_cloexec(int fd) {
  int flags = fcntl(fd, F_GETFD);
  return flags < 0 ? -1 : fcntl(fd, F_SETFD, flags & ~FD_CLOEXEC);
}

static int set_cloexec(int fd) {
  int flags = fcntl(fd, F_GETFD);
  return flags < 0 ? -1 : fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

static int push_arg(char storage[][4096], char *args[], size_t capacity,
                    size_t *count, const char *value) {
  int n;
  if (*count >= capacity) return -1;
  n = snprintf(storage[*count], 4096u, "%s", value);
  if (n < 0 || n >= 4096) return -1;
  args[*count] = storage[*count];
  (*count)++;
  return 0;
}

static void terminate_child(child_control *child) {
  if (child->pid > 0) {
    (void)kill(-child->pid, SIGKILL);
    (void)kill(child->pid, SIGKILL);
    (void)waitpid(child->pid, NULL, 0);
    child->pid = -1;
  }
  if (child->ready_fd >= 0) close(child->ready_fd);
  if (child->gate_fd >= 0) close(child->gate_fd);
  child->ready_fd = -1;
  child->gate_fd = -1;
}

static int setup_pipes(child_control *child, int ready[2], int gate[2]) {
  child->pid = -1;
  child->ready_fd = -1;
  child->gate_fd = -1;
  if (pipe(ready) != 0) return -1;
  if (pipe(gate) != 0) {
    close(ready[0]);
    close(ready[1]);
    return -1;
  }
  if (set_cloexec(ready[0]) != 0 || set_cloexec(ready[1]) != 0 ||
      set_cloexec(gate[0]) != 0 || set_cloexec(gate[1]) != 0) {
    close(ready[0]); close(ready[1]); close(gate[0]); close(gate[1]);
    return -1;
  }
  return 0;
}

static void close_pipe_pair(int pair[2]) {
  if (pair[0] >= 0) close(pair[0]);
  if (pair[1] >= 0) close(pair[1]);
  pair[0] = -1;
  pair[1] = -1;
}

static int spawn_fd_reader(const char *program, t64_verified_input *fixture,
                           t64_verified_input *input, const char *fixture_hex,
                           const char *input_hex, const char *canonical_hex,
                           const char *fixture_path, const char *input_path,
                           child_control *child, int *argv_clean) {
  int ready[2];
  int gate[2];
  char fixture_fd[24], fixture_size[24], input_fd[24], input_size[24];
  char ready_fd[24], gate_fd[24];
  char arg_storage[11][4096];
  char *args[12];
  size_t arg_count = 0u;
  pid_t pid;
  if (setup_pipes(child, ready, gate) != 0) return -1;
  snprintf(fixture_fd, sizeof fixture_fd, "%d", fixture->fd);
  snprintf(fixture_size, sizeof fixture_size, "%llu",
           (unsigned long long)fixture->size);
  snprintf(input_fd, sizeof input_fd, "%d", input->fd);
  snprintf(input_size, sizeof input_size, "%llu",
           (unsigned long long)input->size);
  snprintf(ready_fd, sizeof ready_fd, "%d", ready[1]);
  snprintf(gate_fd, sizeof gate_fd, "%d", gate[0]);
  if (push_arg(arg_storage, args, 11u, &arg_count, program) != 0 ||
      push_arg(arg_storage, args, 11u, &arg_count, "--read-fds") != 0 ||
      push_arg(arg_storage, args, 11u, &arg_count, fixture_fd) != 0 ||
      push_arg(arg_storage, args, 11u, &arg_count, fixture_size) != 0 ||
      push_arg(arg_storage, args, 11u, &arg_count, fixture_hex) != 0 ||
      push_arg(arg_storage, args, 11u, &arg_count, input_fd) != 0 ||
      push_arg(arg_storage, args, 11u, &arg_count, input_size) != 0 ||
      push_arg(arg_storage, args, 11u, &arg_count, input_hex) != 0 ||
      push_arg(arg_storage, args, 11u, &arg_count, canonical_hex) != 0 ||
      push_arg(arg_storage, args, 11u, &arg_count, ready_fd) != 0 ||
      push_arg(arg_storage, args, 11u, &arg_count, gate_fd) != 0) {
    close_pipe_pair(ready);
    close_pipe_pair(gate);
    return -1;
  }
  args[11] = NULL;
  *argv_clean = arg_count == 11u;
  for (int i = 1; i < 11; i++) {
    if (strstr(args[i], fixture_path) != NULL ||
        strstr(args[i], input_path) != NULL) *argv_clean = 0;
  }
  if (!*argv_clean) {
    close_pipe_pair(ready);
    close_pipe_pair(gate);
    return -1;
  }
  pid = fork();
  if (pid < 0) {
    close_pipe_pair(ready);
    close_pipe_pair(gate);
    return -1;
  }
  if (pid == 0) {
    setpgid(0, 0);
    close(ready[0]);
    close(gate[1]);
    if (clear_cloexec(fixture->fd) != 0 || clear_cloexec(input->fd) != 0 ||
        clear_cloexec(ready[1]) != 0 || clear_cloexec(gate[0]) != 0) _exit(126);
    execv(program, args);
    _exit(127);
  }
  setpgid(pid, pid);
  close(ready[1]);
  close(gate[0]);
  child->pid = pid;
  child->ready_fd = ready[0];
  child->gate_fd = gate[1];
  return 0;
}

static int spawn_path_reader(const char *program, const char *fixture_path,
                             size_t fixture_size, const char *fixture_hex,
                             const char *input_path, size_t input_size,
                             const char *input_hex, const char *canonical_hex,
                             child_control *child) {
  int ready[2];
  int gate[2];
  char fixture_size_s[24], input_size_s[24], ready_fd[24], gate_fd[24];
  char arg_storage[11][4096];
  char *args[12];
  size_t arg_count = 0u;
  pid_t pid;
  if (setup_pipes(child, ready, gate) != 0) return -1;
  snprintf(fixture_size_s, sizeof fixture_size_s, "%zu", fixture_size);
  snprintf(input_size_s, sizeof input_size_s, "%zu", input_size);
  snprintf(ready_fd, sizeof ready_fd, "%d", ready[1]);
  snprintf(gate_fd, sizeof gate_fd, "%d", gate[0]);
  if (push_arg(arg_storage, args, 11u, &arg_count, program) != 0 ||
      push_arg(arg_storage, args, 11u, &arg_count,
               "--read-original-after-barrier") != 0 ||
      push_arg(arg_storage, args, 11u, &arg_count, fixture_path) != 0 ||
      push_arg(arg_storage, args, 11u, &arg_count, fixture_size_s) != 0 ||
      push_arg(arg_storage, args, 11u, &arg_count, fixture_hex) != 0 ||
      push_arg(arg_storage, args, 11u, &arg_count, input_path) != 0 ||
      push_arg(arg_storage, args, 11u, &arg_count, input_size_s) != 0 ||
      push_arg(arg_storage, args, 11u, &arg_count, input_hex) != 0 ||
      push_arg(arg_storage, args, 11u, &arg_count, canonical_hex) != 0 ||
      push_arg(arg_storage, args, 11u, &arg_count, ready_fd) != 0 ||
      push_arg(arg_storage, args, 11u, &arg_count, gate_fd) != 0) {
    close_pipe_pair(ready);
    close_pipe_pair(gate);
    return -1;
  }
  args[11] = NULL;
  pid = fork();
  if (pid < 0) {
    close_pipe_pair(ready);
    close_pipe_pair(gate);
    return -1;
  }
  if (pid == 0) {
    setpgid(0, 0);
    close(ready[0]);
    close(gate[1]);
    if (clear_cloexec(ready[1]) != 0 || clear_cloexec(gate[0]) != 0) _exit(126);
    execv(program, args);
    _exit(127);
  }
  setpgid(pid, pid);
  close(ready[1]);
  close(gate[0]);
  child->pid = pid;
  child->ready_fd = ready[0];
  child->gate_fd = gate[1];
  return 0;
}

static int await_ready(child_control *child) {
  uint8_t ready;
  ssize_t count;
  struct pollfd watched = {child->ready_fd, POLLIN | POLLHUP, 0};
  int polled;
  do {
    polled = poll(&watched, 1u, 5000);
  } while (polled < 0 && errno == EINTR);
  if (polled <= 0 || (watched.revents & (POLLIN | POLLHUP)) == 0) return -1;
  do {
    count = read(child->ready_fd, &ready, 1u);
  } while (count < 0 && errno == EINTR);
  return count == 1 && ready == 'B' ? 0 : -1;
}

static int release_child(child_control *child) {
  uint8_t release = 'R';
  if (child->gate_fd < 0) return -1;
  int result = write_all(child->gate_fd, &release, 1u);
  close(child->gate_fd);
  child->gate_fd = -1;
  return result;
}

static int finish_child(child_control *child, int *group_gone) {
  siginfo_t info;
  int status = 0;
  int timed_out = 0;
  struct pollfd watched = {child->ready_fd, POLLHUP, 0};
  int polled;
  memset(&info, 0, sizeof info);
  do {
    polled = poll(&watched, 1u, 5000);
  } while (polled < 0 && errno == EINTR);
  if (polled <= 0 || (watched.revents & (POLLHUP | POLLERR)) == 0) {
    timed_out = 1;
    (void)kill(-child->pid, SIGKILL);
  }
  while (waitid(P_PID, (id_t)child->pid, &info, WEXITED | WNOWAIT) != 0) {
    if (errno != EINTR) return -1;
  }
  (void)kill(-child->pid, SIGKILL);
  while (waitpid(child->pid, &status, 0) < 0) {
    if (errno != EINTR) return -1;
  }
  child->pid = -1;
  close(child->ready_fd);
  child->ready_fd = -1;
  errno = 0;
  *group_gone = kill(-info.si_pid, 0) != 0 && errno == ESRCH;
  return timed_out ? 124 : (WIFEXITED(status) ? WEXITSTATUS(status) : 128);
}

static int spawn_failure_descendant(const char *program,
                                    child_control *child, int *watch_read) {
  int ready[2] = {-1, -1};
  int gate[2] = {-1, -1};
  int watch[2] = {-1, -1};
  char ready_fd[24], gate_fd[24], watch_fd[24];
  char arg_storage[5][4096];
  char *args[6];
  size_t arg_count = 0u;
  pid_t pid;
  child->pid = -1;
  child->ready_fd = -1;
  child->gate_fd = -1;
  *watch_read = -1;
  if (pipe(ready) != 0 || pipe(gate) != 0 || pipe(watch) != 0) goto fail;
  if (set_cloexec(ready[0]) != 0 || set_cloexec(ready[1]) != 0 ||
      set_cloexec(gate[0]) != 0 || set_cloexec(gate[1]) != 0 ||
      set_cloexec(watch[0]) != 0 || set_cloexec(watch[1]) != 0) goto fail;
  snprintf(ready_fd, sizeof ready_fd, "%d", ready[1]);
  snprintf(gate_fd, sizeof gate_fd, "%d", gate[0]);
  snprintf(watch_fd, sizeof watch_fd, "%d", watch[1]);
  if (push_arg(arg_storage, args, 5u, &arg_count, program) != 0 ||
      push_arg(arg_storage, args, 5u, &arg_count,
               "--fail-with-descendant") != 0 ||
      push_arg(arg_storage, args, 5u, &arg_count, ready_fd) != 0 ||
      push_arg(arg_storage, args, 5u, &arg_count, gate_fd) != 0 ||
      push_arg(arg_storage, args, 5u, &arg_count, watch_fd) != 0) goto fail;
  args[5] = NULL;
  pid = fork();
  if (pid < 0) goto fail;
  if (pid == 0) {
    setpgid(0, 0);
    close(ready[0]);
    close(gate[1]);
    close(watch[0]);
    if (clear_cloexec(ready[1]) != 0 || clear_cloexec(gate[0]) != 0 ||
        clear_cloexec(watch[1]) != 0) _exit(126);
    execv(program, args);
    _exit(127);
  }
  setpgid(pid, pid);
  close(ready[1]);
  ready[1] = -1;
  close(gate[0]);
  gate[0] = -1;
  close(watch[1]);
  watch[1] = -1;
  child->pid = pid;
  child->ready_fd = ready[0];
  ready[0] = -1;
  child->gate_fd = gate[1];
  gate[1] = -1;
  *watch_read = watch[0];
  watch[0] = -1;
  return 0;
fail:
  close_pipe_pair(ready);
  close_pipe_pair(gate);
  close_pipe_pair(watch);
  return -1;
}

static int descendant_pipe_closed(int fd, pid_t *descendant_pid) {
  struct pollfd watched = {fd, POLLHUP | POLLIN, 0};
  int polled;
  size_t offset = 0u;
  uint8_t *pid_bytes = (uint8_t *)descendant_pid;
  ssize_t count;
  if (descendant_pid == NULL) return 0;
  watched.events = POLLIN | POLLHUP;
  do {
    polled = poll(&watched, 1u, 5000);
  } while (polled < 0 && errno == EINTR);
  if (polled <= 0) return 0;
  while (offset < sizeof *descendant_pid) {
    count = read(fd, pid_bytes + offset, sizeof *descendant_pid - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) return 0;
    offset += (size_t)count;
  }
  watched.revents = 0;
  watched.events = POLLHUP | POLLIN;
  do {
    polled = poll(&watched, 1u, 5000);
  } while (polled < 0 && errno == EINTR);
  if (polled <= 0) return 0;
  do {
    uint8_t byte;
    count = read(fd, &byte, 1u);
  } while (count < 0 && errno == EINTR);
  return count == 0;
}

static int helper_touch_supervisor_marker(const char *supervisor,
                                          const char *program,
                                          const char *directory,
                                          const char *fixture_path,
                                          const char *marker_path,
                                          const char *result_path,
                                          const char *meta_path,
                                          int real_mode) {
  char (*storage)[4096] = (char (*)[4096])calloc(28u, sizeof *storage);
  char **args = (char **)calloc(29u, sizeof *args);
  char fixture_sha[] = FIXTURE_SHA256;
  size_t count = 0u;
  pid_t pid;
  int status = 0;
  int result = -1;
  if (storage == NULL || args == NULL) {
    free(storage);
    free(args);
    return -1;
  }
  (void)unlink(marker_path);
  (void)unlink(result_path);
  (void)unlink(meta_path);
  if (push_arg(storage, args, 28u, &count, supervisor) != 0) goto done;
  if (real_mode == 1 &&
      push_arg(storage, args, 28u, &count, "--real") != 0)
    goto done;
  if (push_arg(storage, args, 28u, &count, "--identity") != 0 ||
      push_arg(storage, args, 28u, &count, "synthetic") != 0 ||
      push_arg(storage, args, 28u, &count, "--result") != 0 ||
      push_arg(storage, args, 28u, &count, result_path) != 0 ||
      push_arg(storage, args, 28u, &count, "--meta") != 0 ||
      push_arg(storage, args, 28u, &count, meta_path) != 0 ||
      push_arg(storage, args, 28u, &count, "--work-dir") != 0 ||
      push_arg(storage, args, 28u, &count, directory) != 0 ||
      push_arg(storage, args, 28u, &count, "--wall-ms") != 0 ||
      push_arg(storage, args, 28u, &count, "1000") != 0 ||
      push_arg(storage, args, 28u, &count, "--bound") != 0 ||
      push_arg(storage, args, 28u, &count, "1") != 0 ||
      push_arg(storage, args, 28u, &count, "--log-limit") != 0 ||
      push_arg(storage, args, 28u, &count, "1024") != 0 ||
      push_arg(storage, args, 28u, &count, "--fixture-id") != 0 ||
      push_arg(storage, args, 28u, &count, "hello") != 0 ||
      push_arg(storage, args, 28u, &count, "--fixture") != 0 ||
      push_arg(storage, args, 28u, &count, fixture_path) != 0 ||
      push_arg(storage, args, 28u, &count, "--fixture-sha256") != 0 ||
      push_arg(storage, args, 28u, &count, fixture_sha) != 0 ||
      push_arg(storage, args, 28u, &count, "--expect-pass") != 0 ||
      push_arg(storage, args, 28u, &count, "0") != 0 ||
      push_arg(storage, args, 28u, &count, "--") != 0 ||
      push_arg(storage, args, 28u, &count, program) != 0 ||
      push_arg(storage, args, 28u, &count, "--touch-marker") != 0 ||
      push_arg(storage, args, 28u, &count, marker_path) != 0 ||
      (real_mode == 0 &&
       push_arg(storage, args, 28u, &count, "--real") != 0))
    goto done;
  args[count] = NULL;
  pid = fork();
  if (pid < 0) goto done;
  if (pid == 0) {
    if (real_mode == 2 && chdir(directory) != 0) _exit(126);
    execv(args[0], args);
    _exit(127);
  }
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) goto done;
  }
  if (real_mode == 1) {
    result = WIFEXITED(status) && WEXITSTATUS(status) == 2 &&
             access(marker_path, F_OK) != 0 && access(result_path, F_OK) != 0;
  } else if (real_mode == 2) {
    result = access(result_path, F_OK) == 0 && access(marker_path, F_OK) != 0;
  } else {
    result = access(marker_path, F_OK) == 0;
  }
done:
  free(storage);
  free(args);
  return result;
}

static int run_replacement_control(const char *program, const char *supervisor) {
  static const uint8_t source_schedule[] =
      "{ \"schema_version\":1, \"events\":[{\"y\":0,\"tick\":\"1\","
      "\"x\":0,\"port\":0,\"buttons\":1}], \"schema\":\"t64.input\" }\n";
  static const uint8_t replacement_schedule[] =
      "{ \"schema_version\":1, \"events\":[{\"y\":0,\"tick\":\"2\","
      "\"x\":0,\"port\":0,\"buttons\":2}], \"schema\":\"t64.input\" }\n";
  char directory[4096] = {0};
  char fixture_path[4096] = {0};
  char input_path[4096] = {0};
  char marker_path[4096];
  char result_path[4096];
  char meta_path[4096];
  uint8_t *fixture_bytes = NULL;
  uint8_t *replacement_fixture = NULL;
  uint8_t *input_bytes = NULL;
  size_t fixture_size = 0u;
  size_t input_size = 0u;
  size_t observed_size = 0u;
  char original_fixture_hex[T64_SHA256_HEX_SIZE];
  char replacement_fixture_hex[T64_SHA256_HEX_SIZE];
  char original_input_hex[T64_SHA256_HEX_SIZE];
  char replacement_input_hex[T64_SHA256_HEX_SIZE];
  char original_canonical_hex[T64_SHA256_HEX_SIZE];
  char replacement_canonical_hex[T64_SHA256_HEX_SIZE];
  t64_input_schedule *original_schedule = NULL;
  t64_input_schedule *replacement_parsed = NULL;
  t64_verified_input fixture = {-1, 0u, {0u}, {0}, 0u};
  t64_verified_input input = {-1, 0u, {0u}, {0}, 0u};
  t64_input_error input_error;
  t64_verified_input_status fixture_status = T64_VERIFIED_INPUT_ERROR;
  t64_verified_input_status input_status = T64_VERIFIED_INPUT_ERROR;
  child_control descriptor_child = {-1, -1, -1};
  child_control path_child = {-1, -1, -1};
  child_control failure_child = {-1, -1, -1};
  int descriptor_group_gone = 0;
  int path_group_gone = 0;
  int failure_group_gone = 0;
  int descriptor_exit = -1;
  int path_exit = -1;
  int failure_exit = -1;
  int launches = 0;
  int failure_watch = -1;
  int failure_pipe_closed = 0;
  int failure_fds_clean = 0;
  int failure_spawned = 0;
  pid_t failure_pgid = -1;
  pid_t failure_descendant_pid = -1;
  int replacements_ready = 0;
  int argv_clean = 0;
  int real_refused = 0;
  int real_after_separator = 0;
  int setup_ok = make_source_copies(directory, fixture_path, input_path) == 0;
  if (setup_ok) {
    (void)setenv("TMPDIR", directory, 1);
    setup_ok = atomic_replace(input_path, source_schedule,
                              sizeof source_schedule - 1u) == 0 &&
               read_path(fixture_path, FIXTURE_LIMIT, &fixture_bytes,
                         &fixture_size) == 0 &&
               read_path(input_path, INPUT_LIMIT, &input_bytes, &input_size) == 0 &&
               t64_input_parse(input_bytes, input_size, &original_schedule,
                               &input_error) == T64_INPUT_OK;
  }
  if (setup_ok) {
    (void)hash_hex(fixture_bytes, fixture_size, original_fixture_hex);
    (void)hash_hex(input_bytes, input_size, original_input_hex);
    t64_sha256_hex(t64_input_schedule_sha256(original_schedule),
                   original_canonical_hex);
    setup_ok = fixture_size == FIXTURE_SIZE &&
               strcmp(original_fixture_hex, FIXTURE_SHA256) == 0 &&
               strcmp(original_input_hex, original_canonical_hex) != 0 &&
               sizeof source_schedule == sizeof replacement_schedule;
  }
  replacement_fixture = setup_ok ? (uint8_t *)malloc(fixture_size) : NULL;
  if (replacement_fixture != NULL) {
    memcpy(replacement_fixture, fixture_bytes, fixture_size);
    replacement_fixture[fixture_size / 2u] ^= 1u;
    (void)hash_hex(replacement_fixture, fixture_size, replacement_fixture_hex);
    (void)hash_hex(replacement_schedule, sizeof replacement_schedule - 1u,
                   replacement_input_hex);
    setup_ok = t64_input_parse(replacement_schedule,
                               sizeof replacement_schedule - 1u,
                               &replacement_parsed, &input_error) == T64_INPUT_OK;
    if (setup_ok) {
      t64_sha256_hex(t64_input_schedule_sha256(replacement_parsed),
                     replacement_canonical_hex);
      setup_ok = strcmp(original_fixture_hex, replacement_fixture_hex) != 0 &&
                 strcmp(original_input_hex, replacement_input_hex) != 0 &&
                 strcmp(original_canonical_hex, replacement_canonical_hex) != 0;
    }
  } else {
    setup_ok = 0;
  }
  if (setup_ok) {
    fixture_status = t64_verified_input_prepare(
        fixture_path, fixture_size, original_fixture_hex, FIXTURE_LIMIT, &fixture);
    input_status = t64_verified_input_prepare(
        input_path, input_size, original_input_hex, INPUT_LIMIT, &input);
    setup_ok = fixture_status == T64_VERIFIED_INPUT_OK &&
               input_status == T64_VERIFIED_INPUT_OK &&
               descriptor_is_snapshot(fixture.fd, fixture.size, 1) &&
               descriptor_is_snapshot(input.fd, input.size, 1);
  }
  check(setup_ok, "prepares read-only unlinked fixture and schedule descriptors");
  check(setup_ok && strcmp(original_input_hex, original_canonical_hex) != 0,
        "keeps raw schedule file SHA separate from canonical schedule SHA");
  if (setup_ok) {
    (void)snprintf(marker_path, sizeof marker_path, "%s/marker", directory);
    (void)snprintf(result_path, sizeof result_path, "%s/result.json", directory);
    (void)snprintf(meta_path, sizeof meta_path, "%s/meta.json", directory);
    if (spawn_fd_reader(program, &fixture, &input, original_fixture_hex,
                        original_input_hex, original_canonical_hex,
                        fixture_path, input_path, &descriptor_child,
                        &argv_clean) == 0) {
      launches++;
    } else {
      setup_ok = 0;
    }
    if (spawn_path_reader(program, fixture_path, fixture_size,
                          replacement_fixture_hex, input_path,
                          sizeof replacement_schedule - 1u,
                          replacement_input_hex, replacement_canonical_hex,
                          &path_child) == 0) {
      launches++;
    } else {
      setup_ok = 0;
    }
  }
  if (setup_ok) {
    setup_ok = await_ready(&descriptor_child) == 0 && await_ready(&path_child) == 0;
  }
  check(setup_ok, "both consumers block at explicit barriers before first input read");
  if (setup_ok) {
    replacements_ready = atomic_replace(fixture_path, replacement_fixture,
                                        fixture_size) == 0 &&
                         atomic_replace(input_path, replacement_schedule,
                                        sizeof replacement_schedule - 1u) == 0;
    if (replacements_ready) {
      uint8_t *check_bytes = NULL;
      replacements_ready = read_path(fixture_path, FIXTURE_LIMIT, &check_bytes,
                                     &observed_size) == 0 &&
                           observed_size == fixture_size;
      if (replacements_ready) {
        char current[T64_SHA256_HEX_SIZE];
        (void)hash_hex(check_bytes, observed_size, current);
        replacements_ready = strcmp(current, replacement_fixture_hex) == 0;
      }
      free(check_bytes);
    }
    if (replacements_ready) {
      uint8_t *check_input = NULL;
      replacements_ready = read_path(input_path, INPUT_LIMIT, &check_input,
                                     &observed_size) == 0 &&
                           observed_size == sizeof replacement_schedule - 1u;
      if (replacements_ready) {
        char current[T64_SHA256_HEX_SIZE];
        (void)hash_hex(check_input, observed_size, current);
        replacements_ready = strcmp(current, replacement_input_hex) == 0;
      }
      free(check_input);
    }
  }
  check(replacements_ready && argv_clean && fixture_size == FIXTURE_SIZE &&
            input_size == sizeof replacement_schedule - 1u,
        "replaces same-size originals with changed bytes; FD argv has no source paths");
  if (setup_ok) {
    (void)release_child(&descriptor_child);
    (void)release_child(&path_child);
    descriptor_exit = finish_child(&descriptor_child, &descriptor_group_gone);
    path_exit = finish_child(&path_child, &path_group_gone);
  }
  check(descriptor_exit == 0,
        "inherited-FD reader hashes, parses and core-admits the verified originals");
  check(path_exit == 0,
        "negative control: original-path reopen detects both replacements");
  check(launches == 2 && descriptor_group_gone && path_group_gone,
        "both child processes were launched once and their groups were reaped");
  check(setup_ok && count_entries(directory) == 2,
        "replacement run leaves only its two private source files");
  {
    int before_failure_fds = count_open_fds();
    if (spawn_failure_descendant(program, &failure_child, &failure_watch) == 0) {
      failure_spawned = 1;
      failure_pgid = failure_child.pid;
      if (await_ready(&failure_child) == 0)
        (void)release_child(&failure_child);
      failure_exit = finish_child(&failure_child, &failure_group_gone);
      failure_pipe_closed = descendant_pipe_closed(failure_watch,
                                                   &failure_descendant_pid);
      close(failure_watch);
      failure_watch = -1;
      failure_fds_clean = count_open_fds() == before_failure_fds;
    }
  }
  check(failure_spawned && failure_exit == 9,
        "blocked child failure is observed as nonzero, never as pass");
  printf("# failure cleanup: leader_pid=%ld descendant_pid=%ld pgid=%ld "
         "group_gone_at_reap=%d descendant_pipe_closed=%d fds_clean=%d\n",
         (long)failure_pgid, (long)failure_descendant_pid,
         (long)failure_pgid, failure_group_gone, failure_pipe_closed,
         failure_fds_clean);
  check(failure_spawned && failure_descendant_pid > 0 && failure_pipe_closed &&
            failure_fds_clean,
        "group kill terminates descendant and closes owned pipes");
  if (supervisor != NULL) {
    real_refused = helper_touch_supervisor_marker(supervisor, program, directory,
        fixture_path, marker_path, result_path, meta_path, 1);
    real_after_separator = helper_touch_supervisor_marker(supervisor, program,
        directory, getenv("T64_TEST_HELLO_PATH"), marker_path, result_path,
        meta_path, 0);
  }
  check(real_refused,
        "--real refuses before launch and never forwards original paths to a child");
  check(real_after_separator,
        "runner-owned --real after the separator does not select supervisor real mode");
  {
    int option_value_is_not_real = helper_touch_supervisor_marker(
        supervisor, program, directory, "--real", marker_path, result_path,
        meta_path, 2);
    check(option_value_is_not_real,
          "fixture value --real is consumed as a value, not supervisor mode");
  }

  terminate_child(&descriptor_child);
  terminate_child(&path_child);
  terminate_child(&failure_child);
  if (failure_watch >= 0) close(failure_watch);
  t64_verified_input_release(&fixture);
  t64_verified_input_release(&input);
  t64_input_schedule_destroy(original_schedule);
  t64_input_schedule_destroy(replacement_parsed);
  free(fixture_bytes);
  free(input_bytes);
  free(replacement_fixture);
  if (directory[0] != '\0') {
    (void)snprintf(marker_path, sizeof marker_path, "%s/marker", directory);
    (void)snprintf(result_path, sizeof result_path, "%s/result.json", directory);
    (void)snprintf(meta_path, sizeof meta_path, "%s/meta.json", directory);
    unlink(marker_path);
    unlink(result_path);
    unlink(meta_path);
  }
  remove_source_directory(directory, fixture_path, input_path);
  return test_number == REPLACEMENT_TEST_COUNT && failures == 0u ? 0 : 1;
}

int main(int argc, char **argv) {
  char source_path[4096] = {0};
  char directory[4096] = {0};
  char bad_path[4096];
  char link_path[4096];
  char fifo_path[4096];
  char large_path[4096];
  t64_verified_input input = {-1, 0u, {0u}, {0}, 0u};
  t64_verified_input unchanged = {-1, 9u, {0u}, {0}, 0u};
  t64_verified_input_status status;
  int unrelated = -1;
  int before_fds;
  struct stat st;
  int copied;

  if (argc > 1 && strcmp(argv[1], "--read-fds") == 0)
    return helper_read_fds(argc, argv);
  if (argc > 1 && strcmp(argv[1], "--read-original-after-barrier") == 0)
    return helper_read_original(argc, argv);
  if (argc > 1 && strcmp(argv[1], "--touch-marker") == 0)
    return helper_touch_marker(argc, argv);
  if (argc > 1 && strcmp(argv[1], "--fail-with-descendant") == 0)
    return helper_fail_with_descendant(argc, argv);
  if (argc > 1 && strcmp(argv[1], "--replacement") == 0) {
    if (argc < 2 || argc > 3) return 2;
    test_number = 0u;
    failures = 0u;
    return run_replacement_control(argv[0], argc == 3 ? argv[2] : NULL);
  }

  copied = create_source_directory(directory) == 0 &&
           snprintf(source_path, sizeof source_path, "%s/fixture.bin", directory) < 4096 &&
           copy_path(getenv("T64_TEST_HELLO_PATH"), source_path, FIXTURE_LIMIT) == 0;
  if (copied) (void)setenv("TMPDIR", directory, 1);
  printf("1..%u\n", DEFAULT_TEST_COUNT);

  status = copied
               ? t64_verified_input_prepare(source_path, FIXTURE_SIZE,
                     FIXTURE_SHA256, FIXTURE_LIMIT, &input)
               : T64_VERIFIED_INPUT_ERROR;
  check(status == T64_VERIFIED_INPUT_OK && input.fd >= 0,
        "prepares a verified immutable descriptor");
  check(status == T64_VERIFIED_INPUT_OK &&
            descriptor_is_snapshot(input.fd, FIXTURE_SIZE, 1),
        "descriptor is read-only, regular, unlinked and close-on-exec");
  check(status == T64_VERIFIED_INPUT_OK && input.size == FIXTURE_SIZE &&
            strcmp(input.sha256_hex, FIXTURE_SHA256) == 0 &&
            fstat(input.fd, &st) == 0 && st.st_nlink == 0 &&
            st.st_size == FIXTURE_SIZE,
        "descriptor carries exact raw size and SHA-256");
  check(status == T64_VERIFIED_INPUT_OK && exact_descriptor_bytes(&input),
        "descriptor starts at byte zero and yields the pinned fixture bytes");
  check(status == T64_VERIFIED_INPUT_OK && count_entries(directory) == 1,
        "successful preparation leaves no named private snapshot");
  if (status == T64_VERIFIED_INPUT_OK) t64_verified_input_release(&input);
  errno = 0;
  check(fcntl(input.fd, F_GETFD) < 0 && errno == EBADF,
        "release closes the owned descriptor");

  before_fds = count_open_fds();
  (void)snprintf(bad_path, sizeof bad_path, "%s/missing", directory);
  status = t64_verified_input_prepare(bad_path, FIXTURE_SIZE, FIXTURE_SHA256,
                                     FIXTURE_LIMIT, &unchanged);
  check(status == T64_VERIFIED_INPUT_MISSING && unchanged.fd == -1 &&
            unchanged.size == 9u && count_open_fds() == before_fds &&
            count_entries(directory) == 1,
        "missing source refuses without changing output or leaking resources");

  (void)snprintf(link_path, sizeof link_path, "%s/symlink", directory);
  (void)symlink(source_path, link_path);
  before_fds = count_open_fds();
  status = t64_verified_input_prepare(link_path, FIXTURE_SIZE, FIXTURE_SHA256,
                                     FIXTURE_LIMIT, &unchanged);
  unlink(link_path);
  check(status == T64_VERIFIED_INPUT_ERROR && count_open_fds() == before_fds &&
            count_entries(directory) == 1,
        "symlink source refuses without following or leaking resources");

  (void)snprintf(fifo_path, sizeof fifo_path, "%s/fifo", directory);
  (void)mkfifo(fifo_path, 0600);
  before_fds = count_open_fds();
  status = t64_verified_input_prepare(fifo_path, FIXTURE_SIZE, FIXTURE_SHA256,
                                     FIXTURE_LIMIT, &unchanged);
  unlink(fifo_path);
  check(status == T64_VERIFIED_INPUT_ERROR && count_open_fds() == before_fds &&
            count_entries(directory) == 1,
        "non-regular FIFO source refuses without blocking or leaking resources");

  (void)snprintf(large_path, sizeof large_path, "%s/oversize", directory);
  unrelated = open(large_path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
  if (unrelated >= 0) {
    (void)ftruncate(unrelated, FIXTURE_LIMIT + 1u);
    close(unrelated);
  }
  before_fds = count_open_fds();
  status = t64_verified_input_prepare(large_path, FIXTURE_LIMIT + 1u,
                                     FIXTURE_SHA256, FIXTURE_LIMIT, &unchanged);
  unlink(large_path);
  check(status == T64_VERIFIED_INPUT_ERROR && count_open_fds() == before_fds &&
            count_entries(directory) == 1,
        "profile ceiling rejects oversize source before copying");

  unrelated = open("/dev/null", O_RDONLY);
  before_fds = count_open_fds();
  status = t64_verified_input_prepare(source_path, FIXTURE_SIZE,
      "0000000000000000000000000000000000000000000000000000000000000000",
      FIXTURE_LIMIT, &unchanged);
  t64_verified_input_release(&unchanged);
  check(status == T64_VERIFIED_INPUT_ERROR && unrelated >= 0 &&
            fcntl(unrelated, F_GETFD) >= 0 && count_open_fds() == before_fds &&
            count_entries(directory) == 1,
        "digest mismatch and failed release preserve caller descriptors");
  if (unrelated >= 0) close(unrelated);

  before_fds = count_open_fds();
  t64_verified_input_test_fail_once(TEST_FAULT_COPY_INTERRUPT);
  status = t64_verified_input_prepare(source_path, FIXTURE_SIZE, FIXTURE_SHA256,
                                      FIXTURE_LIMIT, &input);
  if (status == T64_VERIFIED_INPUT_OK) t64_verified_input_release(&input);
  check(status == T64_VERIFIED_INPUT_OK && count_open_fds() == before_fds &&
            count_entries(directory) == 1,
        "interrupted source read retries and completes the bounded copy");
  check(fault_refuses_cleanly(source_path, directory, TEST_FAULT_COPY_SHORT),
        "short copy failure removes partial snapshot and closes descriptors");
  check(fault_refuses_cleanly(source_path, directory, TEST_FAULT_TEMP_OPEN),
        "temporary open failure preserves output and cleans owned resources");
  check(fault_refuses_cleanly(source_path, directory, TEST_FAULT_FINALIZE),
        "finalize failure removes snapshot and closes descriptors");
  check(fault_refuses_cleanly(source_path, directory, TEST_FAULT_REOPEN),
        "read-only reopen failure removes snapshot and closes descriptors");
  check(fault_refuses_cleanly(source_path, directory, TEST_FAULT_READBACK),
        "snapshot read failure refuses and closes descriptors");
  check(fault_refuses_cleanly(source_path, directory, TEST_FAULT_READBACK_HASH),
        "reopened digest mismatch refuses and removes snapshot");
  check(fault_refuses_cleanly(source_path, directory, TEST_FAULT_UNLINK),
        "unlink failure refuses and removes the owned snapshot");

  check(!descriptor_is_snapshot(-1, FIXTURE_SIZE, 1),
        "invalid descriptor is refused by inherited-FD admission");
  unrelated = open("/dev/null", O_RDONLY);
  if (unrelated >= 0) close(unrelated);
  check(unrelated >= 0 && !descriptor_is_snapshot(unrelated, FIXTURE_SIZE, 0),
        "closed descriptor is refused by inherited-FD admission");
  unrelated = open(source_path, O_RDWR | O_NOFOLLOW | O_CLOEXEC);
  check(unrelated >= 0 && !descriptor_is_snapshot(unrelated, FIXTURE_SIZE, 1),
        "writable named descriptor is refused by inherited-FD admission");
  if (unrelated >= 0) close(unrelated);
  unrelated = open(source_path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  check(unrelated >= 0 && !descriptor_is_snapshot(unrelated, FIXTURE_SIZE, 1),
        "read-only but named descriptor is refused by inherited-FD admission");
  if (unrelated >= 0) close(unrelated);

  remove_source_directory(directory, source_path, NULL);
  return failures == 0u && test_number == DEFAULT_TEST_COUNT ? 0 : 1;
}
