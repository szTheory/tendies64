#define _POSIX_C_SOURCE 200809L

#include <errno.h>
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
#include "t64_sha256.h"
#include "t64_verified_input.h"
#include "tendies64/t64_machine.h"

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

static bool write_batch_file(const char *path, const char *bytes,
                             size_t length) {
  int fd;
  bool ok;
  if (path == NULL || path[0] == '\0') return false;
  fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
  if (fd < 0) return false;
  ok = write_all(fd, bytes, length) && fsync(fd) == 0;
  if (close(fd) != 0) ok = false;
  if (!ok) unlink(path);
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

static bool drain_guest_observers(t64_machine *machine, child_report *report) {
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
    if (trace[i].device != T64_TRACE_DEVICE_MACHINE ||
        trace[i].kind != T64_TRACE_KIND_CPU_RETIREMENT) continue;
    pc = (uint32_t)(trace[i].value >> 32);
    opcode = (uint32_t)trace[i].value;
    report->last_pc = pc;
    report->last_opcode = opcode;
    if (pc == UINT32_C(0x80000400)) report->saw_entry = 1u;
    if (pc == CPU_PARK_PC && opcode == CPU_PARK_OPCODE)
      report->saw_park = 1u;
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
        stop.tick > target || !drain_guest_observers(machine, report)) {
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
  }
  report->reached_bound = 1u;
  if (report->saw_entry == 0u || report->saw_park == 0u ||
      report->output_total != report->capture_length ||
      report->output_exact == 0u || report->output_lost != 0u) {
    report->protocol_status = T64_CPU_DIAG_INVALID_BATCH;
    report->harness_error = report->saw_entry == 0u ? 8u :
                            report->saw_park == 0u ? 9u :
                            report->output_total != report->capture_length ? 10u :
                            11u;
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
    const char *ci = getenv("CI");
    const char *require_fixtures = getenv("T64_REQUIRE_FIXTURES");
    if (argc != 11 || strcmp(argv[2], "--local-only") != 0 ||
        (ci != NULL && strcmp(ci, "true") == 0) ||
        (require_fixtures != NULL && strcmp(require_fixtures, "1") == 0)) {
      (void)puts("classification=harness-error status=local-only-refused");
      return 4;
    }
    offset = 1;
  } else if (argc != 10) {
    (void)puts("classification=harness-error status=invalid-argument");
    return 4;
  }
  inventory_path = argv[2 + offset];
  rom_path = argv[3 + offset];
  rom_hash = argv[5 + offset];
  output_path = argv[9 + offset];
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
      (void)printf("classification=incomplete status=%s\n",
                   t64_cpu_diag_status_name(guest_status));
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
  if (!write_batch_file(output_path, batch, batch_length)) {
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
  if (argc == 3 && strcmp(argv[1], "--validate-inventory") == 0)
    return command_validate_inventory(argv[2]);
  if (argc == 6 && strcmp(argv[1], "--encode-inventory") == 0)
    return command_encode(argv[2], argv[3], argv[4], argv[5]);
  if (argc >= 3 && (strcmp(argv[1], "--aggregate") == 0 ||
                    strcmp(argv[1], "--verify-complete") == 0))
    return command_aggregate(argc, argv);
  if (argc > 1 && strcmp(argv[1], "--run-authored") == 0)
    return command_run(argc, argv, false);
  if (argc > 1 && strcmp(argv[1], "--run-local") == 0)
    return command_run(argc, argv, true);
  (void)fprintf(stderr,
                "usage: t64-cpu-diagnostic --validate-inventory FILE | "
                "--encode-inventory FILE REG VALUE OUTCOME | "
                "--aggregate INVENTORY BATCH... | --run-authored INVENTORY ROM SIZE SHA256 TICKS REG VALUE OUTPUT\n");
  return 64;
}
