#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
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
#include <time.h>
#include <unistd.h>

#include "t64_cpu_diag_protocol.h"
#include "t64_jsonw.h"
#include "t64_sha256.h"
#include "t64_verified_input.h"
#include "tendies64/t64_machine.h"

#ifdef S_ISVTX
#define CPU_STICKY_DIRECTORY_MODE ((mode_t)S_ISVTX)
#else
/* POSIX sticky-bit value; some libc feature sets hide the S_ISVTX macro. */
#define CPU_STICKY_DIRECTORY_MODE ((mode_t)01000)
#endif

#define CPU_PROFILE_ID "ntsc-cart-4m-openipl3-vr4300-v1"
#define CPU_ROM_LIMIT UINT64_C(0x00401000)
#define CPU_TICK_LIMIT UINT64_C(10000000)
#define CPU_STEP_TICKS UINT64_C(16)
#define CPU_MAX_RESOURCE_YIELDS 64u
#define CPU_WALL_LIMIT_MS UINT64_C(10000)
#define CPU_PARK_PC UINT32_C(0x80000420)
#define CPU_PARK_OPCODE UINT32_C(0x08000108)
#define CPU_CAPTURE_CAPACITY 4096u
#define CPU_FILE_PATH_CAPACITY 4096u
#define CPU_GROUP_REPORT_LIMIT (64u * 1024u * 1024u)

typedef struct {
  uint32_t outcome;
  uint32_t harness_error;
  uint32_t protocol_status;
  uint32_t saw_entry;
  uint32_t saw_park;
  uint32_t reached_bound;
  uint32_t capture_length;
  uint64_t tick;
  uint64_t output_total;
  uint32_t output_exact;
  uint32_t output_lost;
  uint32_t last_pc;
  uint32_t last_opcode;
  uint8_t capture[CPU_CAPTURE_CAPACITY];
} child_report;

static bool parse_u64(const char *text, uint64_t *value_out) {
  uint64_t value = 0u;
  size_t i;
  if (text == NULL || text[0] == '\0' ||
      (text[0] == '0' && text[1] != '\0')) return false;
  for (i = 0u; text[i] != '\0'; i++) {
    unsigned digit;
    if (text[i] < '0' || text[i] > '9') return false;
    digit = (unsigned)(text[i] - '0');
    if (value > (UINT64_MAX - digit) / UINT64_C(10)) return false;
    value = value * UINT64_C(10) + digit;
  }
  *value_out = value;
  return true;
}

static bool local_execution_allowed(void) {
  const char *ci = getenv("CI");
  const char *require_fixtures = getenv("T64_REQUIRE_FIXTURES");
  return (ci == NULL || strcmp(ci, "true") != 0) &&
         (require_fixtures == NULL || strcmp(require_fixtures, "1") != 0);
}

static bool read_bounded_file(const char *path, size_t max_bytes,
                              uint8_t **bytes_out, size_t *length_out) {
  struct stat st;
  uint8_t *bytes;
  size_t offset = 0u;
  int fd;
  if (path == NULL || bytes_out == NULL || length_out == NULL) return false;
  fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  if (fd < 0) return false;
  if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0 ||
      (uint64_t)st.st_size > max_bytes) {
    close(fd);
    return false;
  }
  bytes = (uint8_t *)malloc((size_t)st.st_size);
  if (bytes == NULL) {
    close(fd);
    return false;
  }
  while (offset < (size_t)st.st_size) {
    ssize_t count = read(fd, bytes + offset, (size_t)st.st_size - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) {
      free(bytes);
      close(fd);
      return false;
    }
    offset += (size_t)count;
  }
  if (close(fd) != 0) {
    free(bytes);
    return false;
  }
  *bytes_out = bytes;
  *length_out = offset;
  return true;
}

static bool write_all(int fd, const void *bytes_in, size_t length) {
  const uint8_t *bytes = (const uint8_t *)bytes_in;
  size_t offset = 0u;
  while (offset < length) {
    ssize_t count = write(fd, bytes + offset, length - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) return false;
    offset += (size_t)count;
  }
  return true;
}

#ifdef T64_CPU_DIAGNOSTIC_TESTING
static bool test_snapshot_barrier(void) {
  const char *ready_text = getenv("T64_CPU_DIAG_TEST_READY_FD");
  const char *release_text = getenv("T64_CPU_DIAG_TEST_RELEASE_FD");
  char *ready_end = NULL;
  char *release_end = NULL;
  long ready_fd;
  long release_fd;
  uint8_t ready = 1u;
  uint8_t release = 0u;
  ssize_t count;
  if (ready_text == NULL && release_text == NULL) return true;
  if (ready_text == NULL || release_text == NULL ||
      ready_text[0] == '\0' || release_text[0] == '\0') return false;
  errno = 0;
  ready_fd = strtol(ready_text, &ready_end, 10);
  if (errno != 0 || ready_end == ready_text || *ready_end != '\0' ||
      ready_fd < 0 || ready_fd > INT_MAX) return false;
  errno = 0;
  release_fd = strtol(release_text, &release_end, 10);
  if (errno != 0 || release_end == release_text || *release_end != '\0' ||
      release_fd < 0 || release_fd > INT_MAX) return false;
  if (!write_all((int)ready_fd, &ready, sizeof ready)) return false;
  do {
    count = read((int)release_fd, &release, sizeof release);
  } while (count < 0 && errno == EINTR);
  return count == 1 && release == 1u;
}
#endif

static bool parse_inventory_file(const char *path,
                                 t64_cpu_diag_inventory **inventory_out) {
  uint8_t *bytes = NULL;
  size_t length = 0u;
  t64_cpu_diag_status status;
  if (!read_bounded_file(path, T64_CPU_DIAG_MAX_INVENTORY_BYTES,
                         &bytes, &length)) return false;
  status = t64_cpu_diag_inventory_parse(bytes, length, inventory_out);
  free(bytes);
  return status == T64_CPU_DIAG_OK;
}

static bool path_is_within(const char *path, const char *directory) {
  size_t directory_length;
  if (path == NULL || directory == NULL) return false;
  if (strcmp(path, directory) == 0) return true;
  directory_length = strlen(directory);
  if (directory_length == 1u && directory[0] == '/')
    return path[0] == '/';
  return strncmp(path, directory, directory_length) == 0 &&
         path[directory_length] == '/';
}

static bool directory_has_git_marker(const char *directory) {
  char current[CPU_FILE_PATH_CAPACITY];
  char marker[CPU_FILE_PATH_CAPACITY];
  struct stat info;
  size_t length;
  if (directory == NULL || strlen(directory) >= sizeof current) return true;
  memcpy(current, directory, strlen(directory) + 1u);
  for (;;) {
    int count = snprintf(marker, sizeof marker, "%s%s.git", current,
                         strcmp(current, "/") == 0 ? "" : "/");
    if (count <= 0 || (size_t)count >= sizeof marker) return true;
    if (lstat(marker, &info) == 0) return true;
    if (errno != ENOENT && errno != ENOTDIR) return true;
    if (strcmp(current, "/") == 0) return false;
    length = strlen(current);
    if (length == 0u) return true;
    {
      char *slash = strrchr(current, '/');
      if (slash == NULL) return true;
      if (slash == current) current[1] = '\0';
      else *slash = '\0';
    }
  }
}

static int git_path_status(const char *directory, bool check_ignore,
                           const char *basename) {
  pid_t child = fork();
  int status = 0;
  if (child < 0) return -1;
  if (child == 0) {
    int null_fd = open("/dev/null", O_WRONLY | O_CLOEXEC);
    if (null_fd >= 0) {
      (void)dup2(null_fd, STDOUT_FILENO);
      (void)dup2(null_fd, STDERR_FILENO);
      close(null_fd);
    }
    if (check_ignore) {
      execlp("git", "git", "-C", directory, "check-ignore", "--quiet",
             "--no-index", "--", basename, (char *)NULL);
    } else {
      execlp("git", "git", "-C", directory, "rev-parse",
             "--show-toplevel", (char *)NULL);
    }
    _exit(127);
  }
  while (waitpid(child, &status, 0) < 0) {
    if (errno == EINTR) continue;
    return -1;
  }
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static bool private_temporary_directory(const char *directory,
                                        const char *temporary_root) {
  char canonical_root[CPU_FILE_PATH_CAPACITY];
  struct stat info;
  if (temporary_root == NULL || temporary_root[0] == '\0' ||
      realpath(temporary_root, canonical_root) == NULL ||
      !path_is_within(directory, canonical_root) ||
      stat(directory, &info) != 0 || !S_ISDIR(info.st_mode)) return false;
  if ((info.st_mode & CPU_STICKY_DIRECTORY_MODE) != 0) return true;
  return info.st_uid == geteuid() &&
         (info.st_mode & (S_IWGRP | S_IWOTH)) == 0;
}

static bool private_batch_path(const char *path, char *resolved_path,
                               size_t capacity) {
  char absolute_path[CPU_FILE_PATH_CAPACITY];
  char parent_path[CPU_FILE_PATH_CAPACITY];
  char canonical_parent[CPU_FILE_PATH_CAPACITY];
  char output_name[CPU_FILE_PATH_CAPACITY];
  char working_directory[CPU_FILE_PATH_CAPACITY];
  char *slash;
  const char *temporary_root = getenv("TMPDIR");
  int length;
  if (path == NULL || path[0] == '\0' || resolved_path == NULL ||
      capacity == 0u || strlen(path) >= sizeof absolute_path) return false;
  if (path[0] == '/') {
    memcpy(absolute_path, path, strlen(path) + 1u);
  } else {
    if (getcwd(working_directory, sizeof working_directory) == NULL)
      return false;
    length = snprintf(absolute_path, sizeof absolute_path, "%s/%s",
                      working_directory, path);
    if (length <= 0 || (size_t)length >= sizeof absolute_path) return false;
  }
  slash = strrchr(absolute_path, '/');
  if (slash == NULL) {
    memcpy(parent_path, ".", sizeof ".");
    memcpy(output_name, absolute_path, strlen(absolute_path) + 1u);
  } else {
    size_t name_length = strlen(slash + 1);
    if (name_length == 0u || name_length >= sizeof output_name) return false;
    memcpy(output_name, slash + 1, name_length + 1u);
    if (strcmp(output_name, ".") == 0 || strcmp(output_name, "..") == 0)
      return false;
    if (slash == absolute_path) {
      slash[1] = '\0';
    } else {
      *slash = '\0';
    }
    memcpy(parent_path, absolute_path, strlen(absolute_path) + 1u);
  }
  if (realpath(parent_path, canonical_parent) == NULL) return false;
  length = snprintf(resolved_path, capacity, "%s%s%s", canonical_parent,
                    strcmp(canonical_parent, "/") == 0 ? "" : "/",
                    output_name);
  if (length <= 0 || (size_t)length >= capacity) return false;

  if (git_path_status(canonical_parent, false, NULL) == 0)
    return git_path_status(canonical_parent, true, output_name) == 0;
  if (directory_has_git_marker(canonical_parent)) return false;

  if (private_temporary_directory(canonical_parent, temporary_root) ||
      private_temporary_directory(canonical_parent, "/tmp")) return true;
  return false;
}

static bool write_batch_file(const char *path, const char *bytes,
                             size_t length) {
  char private_path[CPU_FILE_PATH_CAPACITY];
  int fd;
  bool ok;
  if (!private_batch_path(path, private_path, sizeof private_path)) {
    errno = EPERM;
    return false;
  }
  fd = open(private_path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
            0600);
  if (fd < 0) return false;
  ok = write_all(fd, bytes, length) && fsync(fd) == 0;
  if (close(fd) != 0) ok = false;
  if (!ok) unlink(private_path);
  return ok;
}

static bool allocate_machine(void *context, size_t size, size_t alignment,
                             t64_host_storage *storage_out) {
  void *data = NULL;
  size_t host_alignment = alignment < sizeof(void *) ? sizeof(void *) : alignment;
  (void)context;
  if (storage_out == NULL || posix_memalign(&data, host_alignment, size) != 0)
    return false;
  storage_out->data = data;
  storage_out->capacity = size;
  storage_out->alignment = host_alignment;
  return true;
}

static void release_machine(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static bool read_owned_snapshot(int fd, size_t exact_size,
                                const char *expected_sha256,
                                uint8_t **bytes_out) {
  struct stat st;
  int flags;
  uint8_t *bytes;
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  char digest_hex[T64_SHA256_HEX_SIZE];
  size_t offset = 0u;
  if (fd < 0 || exact_size == 0u || exact_size > CPU_ROM_LIMIT ||
      expected_sha256 == NULL || bytes_out == NULL || fstat(fd, &st) != 0 ||
      !S_ISREG(st.st_mode) || st.st_size < 0 ||
      (size_t)st.st_size != exact_size || st.st_nlink != 0) return false;
  flags = fcntl(fd, F_GETFL);
  if (flags < 0 || (flags & O_ACCMODE) != O_RDONLY ||
      lseek(fd, 0, SEEK_SET) != 0) return false;
  bytes = (uint8_t *)malloc(exact_size);
  if (bytes == NULL) return false;
  while (offset < exact_size) {
    ssize_t count = read(fd, bytes + offset, exact_size - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) {
      free(bytes);
      return false;
    }
    offset += (size_t)count;
  }
  {
    t64_sha256_ctx hash;
    t64_sha256_init(&hash);
    t64_sha256_update(&hash, bytes, exact_size);
    t64_sha256_final(&hash, digest);
  }
  t64_sha256_hex(digest, digest_hex);
  if (strcmp(digest_hex, expected_sha256) != 0) {
    free(bytes);
    return false;
  }
  *bytes_out = bytes;
  return true;
}

static bool drain_guest_observers(t64_machine *machine, child_report *report,
                                  uint32_t *park_sequence) {
  t64_trace_record trace[T64_TRACE_RECORD_CAPACITY];
  size_t count = 0u;
  uint64_t dropped = 0u;
  bool saturated = false;
  size_t i;
  t64_output_drain_result output;
  if (t64_machine_trace_drain(machine, trace,
                              sizeof trace / sizeof trace[0], &count,
                              &dropped, &saturated) != T64_STATUS_OK ||
      dropped != 0u || saturated) {
    report->protocol_status = T64_CPU_DIAG_INVALID_BATCH;
    report->harness_error = 1u;
    return false;
  }
  for (i = 0u; i < count; i++) {
    uint32_t pc;
    uint32_t opcode;
    if (trace[i].device != T64_TRACE_DEVICE_MACHINE) continue;
    if (trace[i].kind == T64_TRACE_KIND_CPU_UNSUPPORTED) {
      report->last_pc = (uint32_t)(trace[i].value >> 32);
      report->last_opcode = (uint32_t)trace[i].value;
      continue;
    }
    if (trace[i].kind != T64_TRACE_KIND_CPU_RETIREMENT) continue;
    pc = (uint32_t)(trace[i].value >> 32);
    opcode = (uint32_t)trace[i].value;
    report->last_pc = pc;
    report->last_opcode = opcode;
    if (pc == UINT32_C(0x80000400)) report->saw_entry = 1u;
    if (pc == CPU_PARK_PC && opcode == CPU_PARK_OPCODE)
      report->saw_park = 1u;
    if (pc == CPU_PARK_PC && opcode == CPU_PARK_OPCODE) {
      *park_sequence = *park_sequence >= 4u ? 5u :
                         *park_sequence == 2u ? 3u : 1u;
    } else if (pc == CPU_PARK_PC + 4u && opcode == 0u &&
               (*park_sequence == 1u || *park_sequence == 3u ||
                *park_sequence == 5u)) {
      *park_sequence = *park_sequence == 5u ? 6u : *park_sequence + 1u;
    } else {
      *park_sequence = 0u;
    }
  }
  if (report->capture_length > CPU_CAPTURE_CAPACITY) return false;
  if (t64_machine_output_drain(machine,
                               report->capture + report->capture_length,
                               CPU_CAPTURE_CAPACITY - report->capture_length,
                               &output) != T64_STATUS_OK ||
      output.copied_bytes > CPU_CAPTURE_CAPACITY - report->capture_length) {
    report->protocol_status = T64_CPU_DIAG_INVALID_BATCH;
    report->harness_error = 1u;
    return false;
  }
  report->capture_length += (uint32_t)output.copied_bytes;
  report->output_total = output.total_attempted_bytes;
  report->output_exact = output.total_attempted_bytes_exact ? 1u : 0u;
  report->output_lost = output.lost_output ? 1u : 0u;
  if (report->output_lost != 0u) {
    report->protocol_status = T64_CPU_DIAG_SIZE_LIMIT;
    report->harness_error = 1u;
    return false;
  }
  if (report->output_exact == 0u) {
    report->protocol_status = T64_CPU_DIAG_INVALID_BATCH;
    report->harness_error = 1u;
    return false;
  }
  return true;
}

static void execute_guest(int snapshot_fd, size_t rom_size,
                          const char *rom_sha256, uint64_t tick_bound,
                          uint32_t registration_index, uint32_t value_index,
                          child_report *report) {
  const t64_profile_descriptor *profile = NULL;
  t64_host_allocator allocator = {allocate_machine, release_machine, NULL};
  t64_machine *machine = NULL;
  uint8_t *rom = NULL;
  uint64_t current_tick = 0u;
  uint32_t resource_yields = 0u;
  uint32_t park_sequence = 0u;
  uint64_t previous_resource_tick = UINT64_MAX;
  t64_status status;
  t64_cpu_diag_guest_result guest_result;
  t64_cpu_diag_status transcript_status;
  report->outcome = T64_CPU_DIAG_OUTCOME_HARNESS_ERROR;
  report->protocol_status = T64_CPU_DIAG_INVALID_ARGUMENT;
  if (tick_bound == 0u || tick_bound > CPU_TICK_LIMIT) {
    report->harness_error = 1u;
    goto cleanup;
  }
  if (!read_owned_snapshot(snapshot_fd, rom_size, rom_sha256, &rom)) {
    report->protocol_status = T64_CPU_DIAG_IDENTITY_MISMATCH;
    report->harness_error = 2u;
    goto cleanup;
  }
  if (t64_profile_lookup(CPU_PROFILE_ID, &profile) != T64_STATUS_OK ||
      profile == NULL) {
    report->harness_error = 3u;
    goto cleanup;
  }
  if (t64_machine_create(&allocator, profile, &machine) != T64_STATUS_OK ||
      machine == NULL) {
    report->harness_error = 4u;
    goto cleanup;
  }
  if (t64_machine_trace_set_enabled(machine, true) != T64_STATUS_OK) {
    report->harness_error = 5u;
    goto cleanup;
  }
  if (t64_machine_admit_rom(machine, rom, rom_size) != T64_STATUS_OK) {
    report->protocol_status = T64_CPU_DIAG_INVALID_BATCH;
    report->harness_error = 6u;
    goto cleanup;
  }
  while (current_tick < tick_bound) {
    uint64_t target = tick_bound - current_tick > CPU_STEP_TICKS
                          ? current_tick + CPU_STEP_TICKS
                          : tick_bound;
    t64_stop_result stop = {0u, 0u};
    status = t64_machine_advance(
        machine, &(t64_stop_request){T64_STOP_GUEST_TICK, target}, &stop);
    if (status != T64_STATUS_OK || stop.tick < current_tick ||
        stop.tick > target ||
        !drain_guest_observers(machine, report, &park_sequence)) {
      if (report->harness_error == 0u) {
        report->protocol_status = T64_CPU_DIAG_INVALID_BATCH;
        report->harness_error = 7u;
      }
      goto cleanup;
    }
    current_tick = stop.tick;
    report->tick = current_tick;
    if (stop.reason == T64_STOP_REASON_RESOURCE_LIMIT) {
      resource_yields++;
      if (resource_yields > CPU_MAX_RESOURCE_YIELDS ||
          (current_tick == previous_resource_tick && stop.tick != target))
        goto cleanup;
      previous_resource_tick = current_tick;
      continue;
    }
    if (stop.reason == T64_STOP_REASON_CPU_UNSUPPORTED) {
      report->outcome = T64_CPU_DIAG_OUTCOME_GUEST_STOP;
      report->protocol_status = T64_CPU_DIAG_INCOMPLETE;
      goto cleanup;
    }
    if (stop.reason != T64_STOP_REASON_BOUND || current_tick != target)
      goto cleanup;
    resource_yields = 0u;
    previous_resource_tick = UINT64_MAX;
    /* Completion is the validated guest record plus two observed jump/NOP
       cycles. The tick limit is an upper bound, not work after completion. */
    if (park_sequence >= 4u && report->saw_entry != 0u &&
        report->output_total == report->capture_length &&
        report->output_exact != 0u && report->output_lost == 0u &&
        t64_cpu_diag_guest_transcript_parse(
            report->capture, report->capture_length, registration_index,
            value_index, &guest_result) == T64_CPU_DIAG_OK) {
      report->protocol_status = T64_CPU_DIAG_OK;
      report->outcome = (uint32_t)guest_result.outcome;
      goto cleanup;
    }
  }
  report->reached_bound = 1u;
  if (report->output_total != report->capture_length ||
      report->output_exact == 0u || report->output_lost != 0u) {
    report->protocol_status = T64_CPU_DIAG_INVALID_BATCH;
    report->harness_error = report->output_total != report->capture_length ? 10u :
                            11u;
    goto cleanup;
  }
  if (report->saw_entry == 0u || report->saw_park == 0u) {
    report->protocol_status = T64_CPU_DIAG_INCOMPLETE;
    report->outcome = T64_CPU_DIAG_OUTCOME_INCOMPLETE;
    goto cleanup;
  }
  transcript_status = t64_cpu_diag_guest_transcript_parse(
      report->capture, report->capture_length, registration_index, value_index,
      &guest_result);
  report->protocol_status = (uint32_t)transcript_status;
  if (transcript_status == T64_CPU_DIAG_OK) {
    report->outcome = (uint32_t)guest_result.outcome;
  } else if (transcript_status == T64_CPU_DIAG_INCOMPLETE) {
    report->outcome = T64_CPU_DIAG_OUTCOME_INCOMPLETE;
  } else {
    report->outcome = T64_CPU_DIAG_OUTCOME_HARNESS_ERROR;
    report->harness_error = 1u;
  }
cleanup:
  if (machine != NULL) t64_machine_destroy(machine);
  free(rom);
}

static uint64_t monotonic_ms(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0u;
  return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static bool supervised_guest(t64_verified_input *snapshot, size_t rom_size,
                             const char *rom_sha256, uint64_t tick_bound,
                             uint32_t registration_index,
                             uint32_t value_index,
                             child_report *report) {
  int pipe_fds[2] = {-1, -1};
  pid_t child;
  int child_status = 0;
  size_t received = 0u;
  uint64_t start;
  bool child_done = false;
  bool pipe_closed = false;
  bool timed_out = false;
  bool pipe_ok = true;
  if (pipe(pipe_fds) != 0) return false;
  child = fork();
  if (child < 0) {
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    return false;
  }
  if (child == 0) {
    child_report child_result;
    (void)setpgid(0, 0);
    close(pipe_fds[0]);
    (void)memset(&child_result, 0, sizeof child_result);
    execute_guest(snapshot->fd, rom_size, rom_sha256, tick_bound,
                  registration_index, value_index,
                  &child_result);
    (void)write_all(pipe_fds[1], &child_result, sizeof child_result);
    close(pipe_fds[1]);
    _exit(0);
  }
  (void)setpgid(child, child);
  close(pipe_fds[1]);
  start = monotonic_ms();
  while (!child_done || received < sizeof *report) {
    struct pollfd fd = {pipe_closed ? -1 : pipe_fds[0], POLLIN | POLLHUP, 0};
    int poll_result = poll(&fd, 1u, 20);
    pid_t waited;
    if (!pipe_closed && poll_result > 0 &&
        (fd.revents & (POLLIN | POLLHUP)) != 0) {
      ssize_t count = read(pipe_fds[0], (uint8_t *)report + received,
                           sizeof *report - received);
      if (count < 0 && errno != EINTR) {
        pipe_ok = false;
        break;
      }
      if (count == 0) pipe_closed = true;
      if (count > 0) received += (size_t)count;
    }
    waited = waitpid(child, &child_status, WNOHANG);
    if (waited == child) child_done = true;
    if (waited < 0 && errno != EINTR) {
      pipe_ok = false;
      break;
    }
    if (monotonic_ms() - start > CPU_WALL_LIMIT_MS) {
      timed_out = true;
      (void)kill(-child, SIGKILL);
      break;
    }
    if (child_done && received < sizeof *report && pipe_closed) {
      /* The child closed its writer without sending a complete report. */
      break;
    }
  }
  close(pipe_fds[0]);
  if (timed_out) {
    while (waitpid(child, &child_status, 0) < 0 && errno == EINTR) {}
    return false;
  }
  if (!child_done) {
    while (waitpid(child, &child_status, 0) < 0) {
      if (errno != EINTR) return false;
    }
  }
  return pipe_ok && received == sizeof *report && WIFEXITED(child_status) &&
         WEXITSTATUS(child_status) == 0;
}

static int command_validate_inventory(const char *path) {
  t64_cpu_diag_inventory *inventory = NULL;
  if (!parse_inventory_file(path, &inventory)) {
    (void)puts("classification=harness-error status=invalid-inventory");
    return 4;
  }
  (void)printf("classification=valid registrations=%zu required_values=%zu sha256=%s\n",
               t64_cpu_diag_inventory_registration_count(inventory),
               t64_cpu_diag_inventory_required_value_count(inventory),
               t64_cpu_diag_inventory_sha256(inventory));
  t64_cpu_diag_inventory_destroy(inventory);
  return 0;
}

static bool parse_outcome(const char *text, t64_cpu_diag_outcome *outcome) {
  static const char *const names[] = {
      "pass", "fail", "incomplete", "not-run", "guest-stop", "harness-error"};
  size_t i;
  for (i = 0u; i < sizeof names / sizeof names[0]; i++) {
    if (strcmp(text, names[i]) == 0) {
      *outcome = (t64_cpu_diag_outcome)i;
      return true;
    }
  }
  return false;
}

static int command_encode(const char *path, const char *registration_text,
                          const char *value_text, const char *outcome_text) {
  t64_cpu_diag_inventory *inventory = NULL;
  uint64_t registration;
  uint64_t value;
  t64_cpu_diag_outcome outcome;
  char bytes[T64_CPU_DIAG_MAX_BATCH_BYTES];
  size_t length = 0u;
  t64_cpu_diag_status status;
  if (!parse_inventory_file(path, &inventory) ||
      !parse_u64(registration_text, &registration) || registration > UINT32_MAX ||
      !parse_u64(value_text, &value) || value > UINT32_MAX ||
      !parse_outcome(outcome_text, &outcome)) {
    t64_cpu_diag_inventory_destroy(inventory);
    (void)puts("classification=harness-error status=invalid-argument");
    return 4;
  }
  status = t64_cpu_diag_batch_encode(inventory, (uint32_t)registration,
                                     (uint32_t)value, outcome, bytes,
                                     sizeof bytes, &length);
  t64_cpu_diag_inventory_destroy(inventory);
  if (status != T64_CPU_DIAG_OK || !write_all(STDOUT_FILENO, bytes, length)) {
    (void)puts("classification=harness-error status=encode-failed");
    return 4;
  }
  return 0;
}

static int command_aggregate(int argc, char **argv) {
  t64_cpu_diag_inventory *inventory = NULL;
  t64_cpu_diag_aggregate *aggregate = NULL;
  int i;
  size_t groups = 0u;
  t64_cpu_diag_status status;
  if (argc < 3 || !parse_inventory_file(argv[2], &inventory) ||
      t64_cpu_diag_aggregate_create(inventory, &aggregate) != T64_CPU_DIAG_OK) {
    t64_cpu_diag_inventory_destroy(inventory);
    (void)puts("classification=harness-error status=invalid-inventory");
    return 4;
  }
  for (i = 3; i < argc; i++) {
    uint8_t *bytes = NULL;
    size_t length = 0u;
    if (!read_bounded_file(argv[i], T64_CPU_DIAG_MAX_BATCH_BYTES,
                           &bytes, &length)) {
      status = T64_CPU_DIAG_INVALID_BATCH;
    } else {
      status = t64_cpu_diag_aggregate_add_batch(aggregate, bytes, length);
    }
    free(bytes);
    if (status != T64_CPU_DIAG_OK) {
      (void)printf("classification=%s status=%s\n",
                   status == T64_CPU_DIAG_INCOMPLETE ? "incomplete" : "harness-error",
                   t64_cpu_diag_status_name(status));
      t64_cpu_diag_aggregate_destroy(aggregate);
      t64_cpu_diag_inventory_destroy(inventory);
      return status == T64_CPU_DIAG_INCOMPLETE ? 3 : 4;
    }
  }
  if (!t64_cpu_diag_aggregate_is_complete(aggregate)) {
    (void)printf("classification=incomplete members=%zu required=%zu\n",
                 t64_cpu_diag_aggregate_member_count(aggregate),
                 t64_cpu_diag_inventory_required_value_count(inventory));
    t64_cpu_diag_aggregate_destroy(aggregate);
    t64_cpu_diag_inventory_destroy(inventory);
    return 3;
  }
  for (i = 0; (size_t)i < t64_cpu_diag_required_group_count(); i++) {
    t64_cpu_diag_group_status group_status;
    size_t expected = 0u;
    size_t reached = 0u;
    size_t passed = 0u;
    size_t failed = 0u;
    if (t64_cpu_diag_aggregate_group(
            aggregate, t64_cpu_diag_required_group_name((size_t)i),
            &group_status, &expected, &reached, &passed, &failed) !=
        T64_CPU_DIAG_OK) {
      (void)puts("classification=harness-error status=aggregate-query-failed");
      t64_cpu_diag_aggregate_destroy(aggregate);
      t64_cpu_diag_inventory_destroy(inventory);
      return 4;
    }
    if (expected != 0u) groups++;
  }
  (void)printf("classification=complete groups=%zu members=%zu\n", groups,
               t64_cpu_diag_aggregate_member_count(aggregate));
  t64_cpu_diag_aggregate_destroy(aggregate);
  t64_cpu_diag_inventory_destroy(inventory);
  return 0;
}

static bool path_has_symlink_component(const char *path) {
  char absolute_path[CPU_FILE_PATH_CAPACITY];
  char cursor[CPU_FILE_PATH_CAPACITY];
  char *scan;
  size_t length;
  struct stat info;
  if (path == NULL || path[0] == '\0' || strlen(path) >= sizeof absolute_path)
    return false;
  if (path[0] == '/') {
    memcpy(absolute_path, path, strlen(path) + 1u);
  } else {
    char working_directory[CPU_FILE_PATH_CAPACITY];
    int count;
    if (getcwd(working_directory, sizeof working_directory) == NULL) return false;
    count = snprintf(absolute_path, sizeof absolute_path, "%s/%s",
                     working_directory, path);
    if (count <= 0 || (size_t)count >= sizeof absolute_path) return false;
  }
  length = strlen(absolute_path);
  if (length == 0u || absolute_path[0] != '/') return false;
  cursor[0] = '/';
  cursor[1] = '\0';
  scan = absolute_path + 1;
  while (*scan != '\0') {
    char *end = strchr(scan, '/');
    size_t component_length = end == NULL ? strlen(scan) : (size_t)(end - scan);
    size_t current_length = strlen(cursor);
    if (component_length == 0u) {
      scan = end == NULL ? scan + strlen(scan) : end + 1;
      continue;
    }
    if ((component_length == 1u && scan[0] == '.') ||
        (component_length == 2u && scan[0] == '.' && scan[1] == '.') ||
        current_length + component_length + 2u > sizeof cursor) return false;
    if (current_length > 1u) cursor[current_length++] = '/';
    memcpy(cursor + current_length, scan, component_length);
    cursor[current_length + component_length] = '\0';
    if (lstat(cursor, &info) != 0 || S_ISLNK(info.st_mode)) return false;
    scan = end == NULL ? scan + component_length : end + 1;
  }
  return true;
}

static bool private_batch_directory(const char *path, char *resolved,
                                    size_t capacity) {
  char probe[CPU_FILE_PATH_CAPACITY];
  char checked_probe[CPU_FILE_PATH_CAPACITY];
  char canonical_directory[CPU_FILE_PATH_CAPACITY];
  struct stat info;
  int length;
  if (path == NULL || strlen(path) >= CPU_FILE_PATH_CAPACITY - 24u ||
      !path_has_symlink_component(path) || realpath(path, canonical_directory) == NULL ||
      stat(canonical_directory, &info) != 0 || !S_ISDIR(info.st_mode) ||
      info.st_uid != geteuid() || (info.st_mode & (S_IWGRP | S_IWOTH)) != 0)
    return false;
  length = snprintf(probe, sizeof probe, "%s/.t64-private-probe", path);
  if (length <= 0 || (size_t)length >= sizeof probe ||
      !private_batch_path(probe, checked_probe, sizeof checked_probe))
    return false;
  length = snprintf(resolved, capacity, "%s", canonical_directory);
  return length > 0 && (size_t)length < capacity;
}

static const char *group_status_name(t64_cpu_diag_group_status status) {
  switch (status) {
    case T64_CPU_DIAG_GROUP_PASS: return "pass";
    case T64_CPU_DIAG_GROUP_FAIL: return "fail";
    case T64_CPU_DIAG_GROUP_INCOMPLETE: return "incomplete";
  }
  return "incomplete";
}

static int command_aggregate_json(int argc, char **argv) {
  t64_cpu_diag_inventory *inventory = NULL;
  t64_cpu_diag_aggregate *aggregate = NULL;
  t64_jsonw writer;
  t64_cpu_diag_identity identity;
  char batch_directory[CPU_FILE_PATH_CAPACITY];
  char *report_bytes = NULL;
  size_t report_length = 0u;
  DIR *directory = NULL;
  struct dirent *entry;
  size_t batch_count = 0u;
  size_t member_count;
  size_t i;
  bool complete;
  t64_cpu_diag_status status;
  int result = 4;
  if (argc != 4 || !local_execution_allowed() ||
      !parse_inventory_file(argv[2], &inventory) ||
      !t64_cpu_diag_inventory_identity(inventory, &identity) ||
      !private_batch_directory(argv[3], batch_directory, sizeof batch_directory) ||
      t64_cpu_diag_aggregate_create(inventory, &aggregate) != T64_CPU_DIAG_OK) {
    (void)puts("classification=harness-error status=aggregate-json-refused");
    goto cleanup;
  }
  report_bytes = (char *)malloc(CPU_GROUP_REPORT_LIMIT);
  if (report_bytes == NULL) {
    (void)puts("classification=harness-error status=group-report-allocation-failed");
    goto cleanup;
  }
  t64_jsonw_init(&writer, report_bytes, CPU_GROUP_REPORT_LIMIT);
  directory = opendir(batch_directory);
  if (directory == NULL) {
    (void)puts("classification=harness-error status=batch-directory-unreadable");
    goto cleanup;
  }
  while ((entry = readdir(directory)) != NULL) {
    char path[CPU_FILE_PATH_CAPACITY];
    struct stat info;
    uint8_t *bytes = NULL;
    size_t length = 0u;
    int count;
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;
    if (++batch_count > T64_CPU_DIAG_MAX_ROWS ||
        strchr(entry->d_name, '/') != NULL ||
        strcmp(entry->d_name, ".t64-private-probe") == 0) {
      (void)puts("classification=harness-error status=batch-directory-invalid");
      goto cleanup;
    }
    count = snprintf(path, sizeof path, "%s/%s", batch_directory,
                     entry->d_name);
    if (count <= 0 || (size_t)count >= sizeof path || lstat(path, &info) != 0 ||
        !S_ISREG(info.st_mode) || S_ISLNK(info.st_mode) ||
        !read_bounded_file(path, T64_CPU_DIAG_MAX_BATCH_BYTES, &bytes, &length)) {
      free(bytes);
      (void)puts("classification=harness-error status=batch-file-refused");
      goto cleanup;
    }
    status = t64_cpu_diag_aggregate_add_batch(aggregate, bytes, length);
    free(bytes);
    if (status != T64_CPU_DIAG_OK) {
      (void)printf("classification=harness-error status=%s\n",
                   t64_cpu_diag_status_name(status));
      goto cleanup;
    }
  }
  if (closedir(directory) != 0) {
    directory = NULL;
    (void)puts("classification=harness-error status=batch-directory-close-failed");
    goto cleanup;
  }
  directory = NULL;
  complete = t64_cpu_diag_aggregate_is_complete(aggregate);
  member_count = t64_cpu_diag_aggregate_member_count(aggregate);
#define WKEY(key) t64_jsonw_key(&writer, key, sizeof(key) - 1u)
#define WSTR(value) t64_jsonw_string(&writer, value, strlen(value))
#define WINT(value) t64_jsonw_int(&writer, (int64_t)(value))
#define WBOOL(value) t64_jsonw_bool(&writer, (value) ? 1 : 0)
#define WREQUIRE(expression)                                                   \
  do {                                                                         \
    if ((expression) != T64_JSONW_OK) goto report_error;                       \
  } while (0)
  WREQUIRE(t64_jsonw_begin_object(&writer));
  WREQUIRE(WKEY("classification"));
  WREQUIRE(WSTR(complete ? "complete" : "incomplete"));
  WREQUIRE(WKEY("groups"));
  WREQUIRE(t64_jsonw_begin_array(&writer));
  for (i = 0u; i < t64_cpu_diag_required_group_count(); i++) {
    t64_cpu_diag_group_status group_status;
    size_t expected = 0u, reached = 0u, passed = 0u, failed = 0u;
    if (t64_cpu_diag_aggregate_group(
            aggregate, t64_cpu_diag_required_group_name(i), &group_status,
            &expected, &reached, &passed, &failed) != T64_CPU_DIAG_OK) {
      goto report_error;
    }
    WREQUIRE(t64_jsonw_begin_object(&writer));
    WREQUIRE(WKEY("expected")); WREQUIRE(WINT(expected));
    WREQUIRE(WKEY("failed")); WREQUIRE(WINT(failed));
    WREQUIRE(WKEY("group"));
    WREQUIRE(WSTR(t64_cpu_diag_required_group_name(i)));
    WREQUIRE(WKEY("passed")); WREQUIRE(WINT(passed));
    WREQUIRE(WKEY("reached")); WREQUIRE(WINT(reached));
    WREQUIRE(WKEY("status")); WREQUIRE(WSTR(group_status_name(group_status)));
    WREQUIRE(t64_jsonw_end_object(&writer));
  }
  WREQUIRE(t64_jsonw_end_array(&writer));
  WREQUIRE(WKEY("identity"));
  WREQUIRE(t64_jsonw_begin_object(&writer));
  WREQUIRE(WKEY("base_rom_sha256")); WREQUIRE(WSTR(identity.base_rom_sha256));
  WREQUIRE(WKEY("core_build_sha256")); WREQUIRE(WSTR(identity.core_build_sha256));
  WREQUIRE(WKEY("overlay_sha256")); WREQUIRE(WSTR(identity.overlay_sha256));
  WREQUIRE(WKEY("profile_sha256")); WREQUIRE(WSTR(identity.profile_sha256));
  WREQUIRE(WKEY("source_revision")); WREQUIRE(WSTR(identity.source_revision));
  WREQUIRE(WKEY("tool_receipt_sha256")); WREQUIRE(WSTR(identity.tool_receipt_sha256));
  WREQUIRE(t64_jsonw_end_object(&writer));
  WREQUIRE(WKEY("inventory_sha256"));
  WREQUIRE(WSTR(t64_cpu_diag_inventory_sha256(inventory)));
  WREQUIRE(WKEY("members"));
  WREQUIRE(t64_jsonw_begin_array(&writer));
  for (i = 0u; i < member_count; i++) {
    t64_cpu_diag_member_result member;
    t64_cpu_diag_member_binding binding;
    if (!t64_cpu_diag_aggregate_member_at(aggregate, i, &member) ||
        !t64_cpu_diag_inventory_member_binding(
            inventory, member.registration_index, member.value_index, &binding)) {
      goto report_error;
    }
    WREQUIRE(t64_jsonw_begin_object(&writer));
    WREQUIRE(WKEY("group")); WREQUIRE(WSTR(member.group));
    WREQUIRE(WKEY("has_outcome")); WREQUIRE(WBOOL(member.has_outcome));
    WREQUIRE(WKEY("outcome"));
    WREQUIRE(WSTR(t64_cpu_diag_outcome_name(member.outcome)));
    WREQUIRE(WKEY("registration_index"));
    WREQUIRE(WINT(member.registration_index));
    WREQUIRE(WKEY("registration_name"));
    WREQUIRE(WSTR(member.registration_name));
    WREQUIRE(WKEY("selected_rom_sha256"));
    WREQUIRE(WSTR(binding.selected_rom_sha256));
    WREQUIRE(WKEY("selector")); WREQUIRE(WSTR(binding.selector));
    WREQUIRE(WKEY("value_id")); WREQUIRE(WSTR(member.value_id));
    WREQUIRE(WKEY("value_index")); WREQUIRE(WINT(member.value_index));
    WREQUIRE(t64_jsonw_end_object(&writer));
  }
  WREQUIRE(t64_jsonw_end_array(&writer));
  WREQUIRE(WKEY("schema")); WREQUIRE(WSTR("t64.cpu-group-status"));
  WREQUIRE(WKEY("schema_version")); WREQUIRE(WINT(1));
  WREQUIRE(t64_jsonw_end_object(&writer));
  WREQUIRE(t64_jsonw_finish(&writer, &report_length));
#undef WKEY
#undef WSTR
#undef WINT
#undef WBOOL
#undef WREQUIRE
  if (!write_all(STDOUT_FILENO, report_bytes, report_length)) {
    (void)puts("classification=harness-error status=group-report-failed");
    goto cleanup;
  }
  result = complete ? 0 : 3;
  goto cleanup;
report_error:
  (void)puts("classification=harness-error status=group-report-failed");
cleanup:
  if (directory != NULL) (void)closedir(directory);
  free(report_bytes);
  t64_cpu_diag_aggregate_destroy(aggregate);
  t64_cpu_diag_inventory_destroy(inventory);
  return result;
}

static bool valid_lower_hex(const char *text, size_t length) {
  size_t i;
  if (text == NULL || strlen(text) != length) return false;
  for (i = 0u; i < length; i++) {
    if (!((text[i] >= '0' && text[i] <= '9') ||
          (text[i] >= 'a' && text[i] <= 'f'))) return false;
  }
  return true;
}

static int command_runtime_identity_json(void) {
  static const char build_prefix[] = "t64-build-v1-";
  const char *build_identity = t64_build_identity();
  const char *source_revision = t64_source_revision();
  const t64_profile_descriptor *profile = NULL;
  uint8_t profile_digest[T64_SHA256_DIGEST_SIZE];
  char profile_hex[T64_SHA256_HEX_SIZE];
  char report[512];
  size_t report_length = 0u;
  t64_jsonw writer;
  if (build_identity == NULL ||
      strncmp(build_identity, build_prefix, sizeof build_prefix - 1u) != 0 ||
      !valid_lower_hex(build_identity + sizeof build_prefix - 1u, 64u) ||
      source_revision == NULL || !valid_lower_hex(source_revision, 40u) ||
      t64_profile_lookup(CPU_PROFILE_ID, &profile) != T64_STATUS_OK ||
      profile == NULL || t64_profile_hash(profile, profile_digest) != T64_STATUS_OK) {
    (void)puts("classification=harness-error status=runtime-identity-unavailable");
    return 4;
  }
  t64_sha256_hex(profile_digest, profile_hex);
  t64_jsonw_init(&writer, report, sizeof report);
#define WKEY(key) t64_jsonw_key(&writer, key, sizeof(key) - 1u)
#define WSTR(value) t64_jsonw_string(&writer, value, strlen(value))
#define WREQUIRE(expression)                                                   \
  do {                                                                         \
    if ((expression) != T64_JSONW_OK) goto identity_error;                     \
  } while (0)
  WREQUIRE(t64_jsonw_begin_object(&writer));
  WREQUIRE(WKEY("core_build_sha256"));
  WREQUIRE(WSTR(build_identity + sizeof build_prefix - 1u));
  WREQUIRE(WKEY("profile_sha256")); WREQUIRE(WSTR(profile_hex));
  WREQUIRE(WKEY("source_revision")); WREQUIRE(WSTR(source_revision));
  WREQUIRE(t64_jsonw_end_object(&writer));
  WREQUIRE(t64_jsonw_finish(&writer, &report_length));
#undef WKEY
#undef WSTR
#undef WREQUIRE
  if (!write_all(STDOUT_FILENO, report, report_length)) {
    (void)puts("classification=harness-error status=runtime-identity-write-failed");
    return 4;
  }
  return 0;
identity_error:
  (void)puts("classification=harness-error status=runtime-identity-write-failed");
  return 4;
}

static int command_run(int argc, char **argv, bool local_mode) {
  t64_cpu_diag_inventory *inventory = NULL;
  t64_cpu_diag_identity identity;
  t64_cpu_diag_member_binding member_binding;
  const t64_profile_descriptor *profile = NULL;
  t64_verified_input snapshot = {-1, 0u, {0u}, {0}, 0u};
  child_report report;
  uint64_t rom_size_value;
  uint64_t tick_bound;
  uint64_t registration;
  uint64_t value;
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  char profile_hex[T64_SHA256_HEX_SIZE];
  char batch[T64_CPU_DIAG_MAX_BATCH_BYTES];
  char private_output_path[CPU_FILE_PATH_CAPACITY];
  size_t batch_length = 0u;
  t64_cpu_diag_aggregate *aggregate = NULL;
  t64_cpu_diag_status protocol_status;
  const char *inventory_path;
  const char *rom_path;
  const char *rom_hash;
  const char *output_path;
  int offset = local_mode ? 1 : 0;
  int exit_code = 4;
  if (local_mode) {
    if (argc != 11 || strcmp(argv[2], "--local-only") != 0 ||
        !local_execution_allowed()) {
      (void)puts("classification=harness-error status=local-only-refused");
      return 4;
    }
    offset = 1;
  } else {
    if (argc != 10) {
      (void)puts("classification=harness-error status=invalid-argument");
      return 4;
    }
    if (!local_execution_allowed()) {
      (void)puts("classification=harness-error status=local-only-refused");
      return 4;
    }
  }
  inventory_path = argv[2 + offset];
  rom_path = argv[3 + offset];
  rom_hash = argv[5 + offset];
  output_path = argv[9 + offset];
  if (!private_batch_path(output_path, private_output_path,
                          sizeof private_output_path)) {
    (void)puts("classification=harness-error status=output-not-private");
    goto cleanup;
  }
  if (!parse_u64(argv[4 + offset], &rom_size_value) ||
      rom_size_value == 0u || rom_size_value > CPU_ROM_LIMIT ||
      !parse_u64(argv[6 + offset], &tick_bound) || tick_bound == 0u ||
      tick_bound > CPU_TICK_LIMIT ||
      !parse_u64(argv[7 + offset], &registration) || registration > UINT32_MAX ||
      !parse_u64(argv[8 + offset], &value) || value > UINT32_MAX) {
    (void)puts("classification=harness-error status=invalid-bound");
    return 4;
  }
  if (!parse_inventory_file(inventory_path, &inventory) ||
      !t64_cpu_diag_inventory_identity(inventory, &identity) ||
      t64_profile_lookup(CPU_PROFILE_ID, &profile) != T64_STATUS_OK ||
      profile == NULL || t64_profile_hash(profile, digest) != T64_STATUS_OK) {
    (void)puts("classification=harness-error status=invalid-inventory-or-profile");
    goto cleanup;
  }
  t64_sha256_hex(digest, profile_hex);
  if (strcmp(identity.profile_sha256, profile_hex) != 0) {
    (void)puts("classification=harness-error status=profile-identity-mismatch");
    goto cleanup;
  }
  if (local_mode &&
      (strncmp(t64_build_identity(), "t64-build-v1-", 13u) != 0 ||
       strcmp(t64_build_identity() + 13u, identity.core_build_sha256) != 0)) {
    (void)puts("classification=harness-error status=core-build-identity-mismatch");
    goto cleanup;
  }
  if (!t64_cpu_diag_inventory_member_binding(
          inventory, (uint32_t)registration, (uint32_t)value,
          &member_binding)) {
    (void)puts("classification=harness-error status=unregistered-member");
    goto cleanup;
  }
  if (strcmp(member_binding.selected_rom_sha256, rom_hash) != 0) {
    (void)puts("classification=harness-error status=rom-identity-mismatch");
    goto cleanup;
  }
  if (t64_verified_input_prepare(rom_path, rom_size_value, rom_hash,
                                 CPU_ROM_LIMIT, &snapshot) !=
      T64_VERIFIED_INPUT_OK) {
    (void)puts("classification=harness-error status=source-snapshot-refused");
    goto cleanup;
  }
#ifdef T64_CPU_DIAGNOSTIC_TESTING
  if (!test_snapshot_barrier()) {
    (void)puts("classification=harness-error status=test-snapshot-barrier-failed");
    goto cleanup;
  }
#endif
  (void)memset(&report, 0, sizeof report);
  if (!supervised_guest(&snapshot, (size_t)rom_size_value, rom_hash,
                        tick_bound, (uint32_t)registration,
                        (uint32_t)value, &report)) {
    (void)puts("classification=harness-error status=child-or-wall-bound");
    goto cleanup;
  }
  if (report.protocol_status != T64_CPU_DIAG_OK) {
    t64_cpu_diag_status guest_status =
        (t64_cpu_diag_status)report.protocol_status;
    if (guest_status == T64_CPU_DIAG_INCOMPLETE) {
      (void)printf("classification=incomplete status=%s outcome=%s stage=%u entry=%u park=%u tick=%llu capture=%u total=%llu last_pc=%08x last_opcode=%08x\n",
                   t64_cpu_diag_status_name(guest_status),
                   t64_cpu_diag_outcome_name((t64_cpu_diag_outcome)report.outcome),
                   report.harness_error, report.saw_entry, report.saw_park,
                   (unsigned long long)report.tick, report.capture_length,
                   (unsigned long long)report.output_total, report.last_pc,
                   report.last_opcode);
      exit_code = 3;
    } else {
      (void)printf("classification=harness-error status=%s stage=%u entry=%u park=%u tick=%llu capture=%u total=%llu last_pc=%08x last_opcode=%08x\n",
                   t64_cpu_diag_status_name(guest_status),
                   report.harness_error, report.saw_entry, report.saw_park,
                   (unsigned long long)report.tick, report.capture_length,
                   (unsigned long long)report.output_total, report.last_pc,
                   report.last_opcode);
    }
    goto cleanup;
  }
  protocol_status = t64_cpu_diag_batch_encode(
      inventory, (uint32_t)registration, (uint32_t)value,
      (t64_cpu_diag_outcome)report.outcome, batch, sizeof batch, &batch_length);
  if (protocol_status == T64_CPU_DIAG_OK)
    protocol_status = t64_cpu_diag_aggregate_create(inventory, &aggregate);
  if (protocol_status == T64_CPU_DIAG_OK)
    protocol_status = t64_cpu_diag_aggregate_add_batch(
        aggregate, (const uint8_t *)batch, batch_length);
  if (protocol_status != T64_CPU_DIAG_OK || aggregate == NULL) {
    (void)printf("classification=harness-error status=protocol-%s\n",
                 t64_cpu_diag_status_name(protocol_status));
    goto cleanup;
  }
  if (!write_batch_file(private_output_path, batch, batch_length)) {
    if (errno == EPERM) {
      (void)puts("classification=harness-error status=output-not-private");
      goto cleanup;
    }
    (void)printf("classification=harness-error status=output-failed errno=%d\n",
                 errno);
    goto cleanup;
  }
  (void)printf("classification=%s tick=%llu capture_bytes=%u park=%u\n",
               t64_cpu_diag_outcome_name((t64_cpu_diag_outcome)report.outcome),
               (unsigned long long)report.tick, report.capture_length,
               report.saw_park);
  exit_code = report.outcome == T64_CPU_DIAG_OUTCOME_PASS ? 0 :
              report.outcome == T64_CPU_DIAG_OUTCOME_INCOMPLETE ? 3 :
              report.outcome == T64_CPU_DIAG_OUTCOME_HARNESS_ERROR ? 4 : 2;
cleanup:
  if (aggregate != NULL) t64_cpu_diag_aggregate_destroy(aggregate);
  if (inventory != NULL) t64_cpu_diag_inventory_destroy(inventory);
  t64_verified_input_release(&snapshot);
  return exit_code;
}

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "--runtime-identity-json") == 0)
    return command_runtime_identity_json();
  if (argc == 3 && strcmp(argv[1], "--validate-inventory") == 0)
    return command_validate_inventory(argv[2]);
  if (argc == 6 && strcmp(argv[1], "--encode-inventory") == 0)
    return command_encode(argv[2], argv[3], argv[4], argv[5]);
  if (argc >= 3 && (strcmp(argv[1], "--aggregate") == 0 ||
                    strcmp(argv[1], "--verify-complete") == 0))
    return command_aggregate(argc, argv);
  if (argc == 4 && (strcmp(argv[1], "--aggregate-json") == 0 ||
                    strcmp(argv[1], "--verify-complete-json") == 0))
    return command_aggregate_json(argc, argv);
  if (argc > 1 && strcmp(argv[1], "--run-authored") == 0)
    return command_run(argc, argv, false);
  if (argc > 1 && strcmp(argv[1], "--run-local") == 0)
    return command_run(argc, argv, true);
  (void)fprintf(stderr,
                "usage: t64-cpu-diagnostic --validate-inventory FILE | "
                "--encode-inventory FILE REG VALUE OUTCOME | "
                "--aggregate INVENTORY BATCH... | --aggregate-json INVENTORY PRIVATE_BATCH_DIR | "
                "--run-authored INVENTORY ROM SIZE SHA256 TICKS REG VALUE OUTPUT\n");
  return 64;
}
