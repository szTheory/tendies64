#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <dirent.h>
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
#include <time.h>
#include <unistd.h>

#include "t64_rsp_diag_protocol.h"
#include "t64_sha256.h"
#include "t64_verified_input.h"
#include "tendies64/t64_machine.h"
#include "t64_internal.h"

#define RSP_PROFILE_ID "ntsc-cart-4m-openipl3-rsp-functional-v1"
#define RSP_TICK_LIMIT UINT64_C(10000000)
#define RSP_STEP_TICKS UINT64_C(16)
#define RSP_CHILD_LIMIT_MS UINT64_C(10000)
#define RSP_CAPTURE_LIMIT 4096u
#define RSP_ROM_LIMIT UINT64_C(0x00401000)
#define RSP_PATH_LIMIT 4096u

typedef struct {
  uint32_t outcome;
  uint32_t status;
  uint32_t pc;
  uint32_t opcode;
  uint32_t refusal;
  uint32_t capture_length;
  uint32_t terminal_pc;
  uint32_t terminal_opcode;
  uint32_t terminal_delay_slot_pc;
  uint32_t terminal_delay_slot_opcode;
  uint32_t terminal_delay_slot_retired;
  uint32_t have_previous_cpu_retirement;
  uint32_t previous_cpu_pc;
  uint32_t previous_cpu_opcode;
  uint64_t tick;
  uint64_t retired;
  uint8_t capture[RSP_CAPTURE_LIMIT];
} rsp_child_report;

static bool allocate_machine(void *context, size_t size, size_t alignment,
                             t64_host_storage *storage_out) {
  void *data = NULL;
  size_t actual_alignment = alignment < sizeof(void *) ? sizeof(void *) : alignment;
  (void)context;
  if (storage_out == NULL || posix_memalign(&data, actual_alignment, size) != 0)
    return false;
  storage_out->data = data;
  storage_out->capacity = size;
  storage_out->alignment = actual_alignment;
  return true;
}

static void release_machine(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static bool valid_hex(const char *text, size_t digits) {
  size_t i;
  if (text == NULL || strlen(text) != digits) return false;
  for (i = 0u; i < digits; i++) {
    if (!((text[i] >= '0' && text[i] <= '9') ||
          (text[i] >= 'a' && text[i] <= 'f'))) return false;
  }
  return true;
}

static void digest_bytes(const void *bytes, size_t length,
                         uint8_t digest[T64_SHA256_DIGEST_SIZE]) {
  t64_sha256_ctx hash;
  t64_sha256_init(&hash);
  t64_sha256_update(&hash, bytes, length);
  t64_sha256_final(&hash, digest);
}

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

static bool read_bounded_file(const char *path, size_t maximum,
                              uint8_t **bytes_out, size_t *length_out) {
  struct stat info;
  uint8_t *bytes;
  size_t offset = 0u;
  int fd;
  if (path == NULL || bytes_out == NULL || length_out == NULL) return false;
  fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  if (fd < 0) return false;
  if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size <= 0 ||
      (uint64_t)info.st_size > maximum) {
    close(fd);
    return false;
  }
  bytes = (uint8_t *)malloc((size_t)info.st_size);
  if (bytes == NULL) { close(fd); return false; }
  while (offset < (size_t)info.st_size) {
    ssize_t count = read(fd, bytes + offset, (size_t)info.st_size - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) { free(bytes); close(fd); return false; }
    offset += (size_t)count;
  }
  if (close(fd) != 0) { free(bytes); return false; }
  *bytes_out = bytes;
  *length_out = offset;
  return true;
}

static bool parse_inventory_file(const char *path,
                                 t64_rsp_diag_inventory **inventory_out) {
  uint8_t *bytes = NULL;
  size_t length = 0u;
  t64_rsp_diag_status status;
  if (!read_bounded_file(path, T64_RSP_DIAG_MAX_INVENTORY_BYTES,
                         &bytes, &length)) return false;
  status = t64_rsp_diag_inventory_parse(bytes, length, inventory_out);
  free(bytes);
  return status == T64_RSP_DIAG_OK;
}

static bool local_execution_allowed(void) {
  const char *ci = getenv("CI");
  const char *fixtures = getenv("T64_REQUIRE_FIXTURES");
  return (ci == NULL || (strcmp(ci, "true") != 0 && strcmp(ci, "1") != 0)) &&
         (fixtures == NULL || strcmp(fixtures, "1") != 0);
}

static bool private_output_path(const char *path, char *output,
                               size_t capacity) {
  static const char marker[] = "/build/private-rsp-diagnostic/";
  struct stat info;
  size_t length;
  if (path == NULL || output == NULL || strlen(path) >= capacity) return false;
  if (strstr(path, marker) == NULL) return false;
  length = strlen(path);
  if (length == 0u || path[0] != '/' || strstr(path, "/../") != NULL ||
      strcmp(path + (length > 3u ? length - 3u : 0u), "/..") == 0) return false;
  if (lstat("build/private-rsp-diagnostic", &info) != 0 ||
      !S_ISDIR(info.st_mode) || (info.st_mode & 077u) != 0u) return false;
  memcpy(output, path, length + 1u);
  return true;
}

static bool write_private_file(const char *path, const char *bytes,
                               size_t length) {
  int fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
                S_IRUSR | S_IWUSR);
  size_t offset = 0u;
  bool created = fd >= 0;
  bool ok = created;
  while (ok && offset < length) {
    ssize_t count = write(fd, bytes + offset, length - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) ok = false;
    else offset += (size_t)count;
  }
  if (ok && fsync(fd) != 0) ok = false;
  if (fd >= 0 && close(fd) != 0) ok = false;
  if (!ok && created) (void)unlink(path);
  return ok;
}

static t64_rsp_diag_status runtime_identity(
    const t64_rsp_diag_inventory *inventory,
    const t64_profile_descriptor **profile_out,
    char profile_hex[T64_SHA256_HEX_SIZE]) {
  t64_rsp_diag_identity identity;
  const t64_profile_descriptor *profile = NULL;
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  static const char build_prefix[] = "t64-build-v1-";
  const char *build = t64_build_identity();
  const char *revision = t64_source_revision();
  if (inventory == NULL || !t64_rsp_diag_inventory_identity(inventory, &identity) ||
      build == NULL || strncmp(build, build_prefix, sizeof build_prefix - 1u) != 0 ||
      !valid_hex(build + sizeof build_prefix - 1u, 64u) || revision == NULL ||
      !valid_hex(revision, 40u) || strcmp(identity.core_build_sha256,
                                            build + sizeof build_prefix - 1u) != 0 ||
      strcmp(identity.core_source_revision, revision) != 0 ||
      strcmp(identity.profile_id, RSP_PROFILE_ID) != 0 ||
      t64_profile_lookup(RSP_PROFILE_ID, &profile) != T64_STATUS_OK ||
      profile == NULL || t64_profile_hash(profile, digest) != T64_STATUS_OK ||
      profile->machine_model_version != 6u ||
      profile->state_serialization_version != 12u) {
    return T64_RSP_DIAG_IDENTITY_MISMATCH;
  }
  t64_sha256_hex(digest, profile_hex);
  if (strcmp(identity.profile_descriptor_sha256, profile_hex) != 0)
    return T64_RSP_DIAG_IDENTITY_MISMATCH;
  *profile_out = profile;
  return T64_RSP_DIAG_OK;
}

static int command_runtime_identity(void) {
  const t64_profile_descriptor *profile = NULL;
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  char profile_hex[T64_SHA256_HEX_SIZE];
  const char *build = t64_build_identity();
  const char *revision = t64_source_revision();
  if (build == NULL || strncmp(build, "t64-build-v1-", 13u) != 0 ||
      !valid_hex(build + 13u, 64u) || revision == NULL ||
      !valid_hex(revision, 40u) ||
      t64_profile_lookup(RSP_PROFILE_ID, &profile) != T64_STATUS_OK ||
      profile == NULL || t64_profile_hash(profile, digest) != T64_STATUS_OK ||
      profile->machine_model_version != 6u ||
      profile->state_serialization_version != 12u) {
    (void)puts("classification=harness-error status=runtime-identity-unavailable");
    return 4;
  }
  t64_sha256_hex(digest, profile_hex);
  (void)printf("{\"core_build_sha256\":\"%s\",\"core_source_revision\":\"%s\",\"profile_descriptor_sha256\":\"%s\",\"profile_id\":\"%s\"}\n",
               build + 13u, revision, profile_hex, RSP_PROFILE_ID);
  return 0;
}

static int command_validate_inventory(const char *path) {
  t64_rsp_diag_inventory *inventory = NULL;
  if (!parse_inventory_file(path, &inventory)) {
    (void)puts("classification=harness-error status=invalid-inventory");
    return 4;
  }
  (void)printf("classification=valid registrations=%zu required_values=%zu inventory_sha256=%s\n",
               t64_rsp_diag_inventory_registration_count(inventory),
               t64_rsp_diag_inventory_required_value_count(inventory),
               t64_rsp_diag_inventory_sha256(inventory));
  t64_rsp_diag_inventory_destroy(inventory);
  return 0;
}

static int command_validate_batch(const char *inventory_path,
                                 const char *batch_path) {
  t64_rsp_diag_inventory *inventory = NULL;
  t64_rsp_diag_aggregate *aggregate = NULL;
  t64_rsp_diag_member_result member_result;
  uint8_t *bytes = NULL;
  size_t length = 0u;
  char checked_inventory[RSP_PATH_LIMIT];
  char checked_batch[RSP_PATH_LIMIT];
  int result = 4;
  if (!local_execution_allowed() ||
      !private_output_path(inventory_path, checked_inventory,
                           sizeof checked_inventory) ||
      !private_output_path(batch_path, checked_batch, sizeof checked_batch) ||
      !parse_inventory_file(checked_inventory, &inventory) ||
      !read_bounded_file(checked_batch, T64_RSP_DIAG_MAX_BATCH_BYTES,
                         &bytes, &length)) {
    (void)puts("classification=harness-error status=private-input-or-inventory-refused");
    goto cleanup;
  }
  if (t64_rsp_diag_aggregate_create(inventory, &aggregate) !=
          T64_RSP_DIAG_OK ||
      t64_rsp_diag_aggregate_add_batch(aggregate, bytes, length) !=
          T64_RSP_DIAG_OK ||
      t64_rsp_diag_aggregate_member_count(aggregate) != 1u ||
      !t64_rsp_diag_aggregate_member_at(aggregate, 0u, &member_result)) {
    (void)puts("classification=harness-error status=invalid-or-unregistered-batch");
    goto cleanup;
  }
  (void)printf("classification=validated outcome=%s members=1\n",
               t64_rsp_diag_outcome_name(member_result.outcome));
  result = 0;
cleanup:
  free(bytes);
  t64_rsp_diag_aggregate_destroy(aggregate);
  t64_rsp_diag_inventory_destroy(inventory);
  return result;
}

static bool read_at_bounded_file(int directory_fd, const char *name,
                                 uint8_t **bytes_out, size_t *length_out) {
  struct stat info;
  uint8_t *bytes;
  size_t offset = 0u;
  int fd = openat(directory_fd, name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  if (fd < 0) return false;
  if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size <= 0 ||
      (uint64_t)info.st_size > T64_RSP_DIAG_MAX_BATCH_BYTES) {
    close(fd);
    return false;
  }
  bytes = (uint8_t *)malloc((size_t)info.st_size);
  if (bytes == NULL) { close(fd); return false; }
  while (offset < (size_t)info.st_size) {
    ssize_t count = read(fd, bytes + offset, (size_t)info.st_size - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) { free(bytes); close(fd); return false; }
    offset += (size_t)count;
  }
  if (close(fd) != 0) { free(bytes); return false; }
  *bytes_out = bytes;
  *length_out = offset;
  return true;
}

static int command_verify_complete(const char *inventory_path,
                                   const char *batch_directory) {
  t64_rsp_diag_inventory *inventory = NULL;
  t64_rsp_diag_aggregate *aggregate = NULL;
  struct stat info;
  DIR *directory = NULL;
  struct dirent *entry;
  int directory_fd;
  size_t batches = 0u;
  bool invalid = false;
  int result = 4;
  if (!local_execution_allowed() || !parse_inventory_file(inventory_path, &inventory)) {
    (void)puts("classification=harness-error status=invalid-inventory-or-local-only-refused");
    goto cleanup;
  }
  if (batch_directory == NULL ||
      strstr(batch_directory, "/build/private-rsp-diagnostic/") == NULL ||
      lstat(batch_directory, &info) != 0 || !S_ISDIR(info.st_mode) ||
      (info.st_mode & 077u) != 0u) {
    (void)puts("classification=harness-error status=batch-directory-not-private");
    goto cleanup;
  }
  if (t64_rsp_diag_aggregate_create(inventory, &aggregate) != T64_RSP_DIAG_OK) {
    (void)puts("classification=harness-error status=aggregate-allocation-failed");
    goto cleanup;
  }
  directory_fd = open(batch_directory, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  if (directory_fd < 0) {
    (void)puts("classification=harness-error status=batch-directory-open-failed");
    goto cleanup;
  }
  directory = fdopendir(directory_fd);
  if (directory == NULL) {
    close(directory_fd);
    (void)puts("classification=harness-error status=batch-directory-read-failed");
    goto cleanup;
  }
  errno = 0;
  while ((entry = readdir(directory)) != NULL) {
    uint8_t *bytes = NULL;
    size_t length = 0u;
    size_t name_length = strlen(entry->d_name);
    struct stat child_info;
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;
    if (name_length <= 5u || strcmp(entry->d_name + name_length - 5u, ".json") != 0 ||
        fstatat(dirfd(directory), entry->d_name, &child_info,
                AT_SYMLINK_NOFOLLOW) != 0 || !S_ISREG(child_info.st_mode) ||
        !read_at_bounded_file(dirfd(directory), entry->d_name, &bytes, &length)) {
      invalid = true;
      break;
    }
    if (t64_rsp_diag_aggregate_add_batch(aggregate, bytes, length) !=
        T64_RSP_DIAG_OK) invalid = true;
    free(bytes);
    batches++;
    if (invalid) break;
    errno = 0;
  }
  if (entry == NULL && errno != 0) invalid = true;
  closedir(directory);
  directory = NULL;
  if (invalid) {
    (void)printf("classification=harness-error status=invalid-batch batches=%zu\n",
                 batches);
    goto cleanup;
  }
  if (!t64_rsp_diag_aggregate_is_complete(aggregate)) {
    (void)printf("classification=incomplete status=missing-or-nonpassing-member batches=%zu members=%zu\n",
                 batches, t64_rsp_diag_aggregate_member_count(aggregate));
    result = 3;
    goto cleanup;
  }
  (void)printf("classification=complete members=%zu\n",
               t64_rsp_diag_aggregate_member_count(aggregate));
  result = 0;
cleanup:
  if (directory != NULL) closedir(directory);
  t64_rsp_diag_aggregate_destroy(aggregate);
  t64_rsp_diag_inventory_destroy(inventory);
  return result;
}

static bool read_snapshot(int fd, size_t length, const char *expected_sha,
                          uint8_t **bytes_out) {
  uint8_t *bytes;
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  char digest_hex[T64_SHA256_HEX_SIZE];
  size_t offset = 0u;
  if (lseek(fd, 0, SEEK_SET) < 0) return false;
  bytes = (uint8_t *)malloc(length);
  if (bytes == NULL) return false;
  while (offset < length) {
    ssize_t count = read(fd, bytes + offset, length - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) { free(bytes); return false; }
    offset += (size_t)count;
  }
  digest_bytes(bytes, length, digest);
  t64_sha256_hex(digest, digest_hex);
  if (strcmp(digest_hex, expected_sha) != 0) { free(bytes); return false; }
  *bytes_out = bytes;
  return true;
}

static bool drain_guest_observers(t64_machine *machine,
                                  rsp_child_report *report) {
  t64_trace_record trace[T64_TRACE_RECORD_CAPACITY];
  size_t count = 0u;
  uint64_t dropped = 0u;
  bool saturated = false;
  size_t i;
  t64_output_drain_result output;
  size_t available = RSP_CAPTURE_LIMIT - report->capture_length;
  if (t64_machine_trace_drain(machine, trace,
                              sizeof trace / sizeof trace[0], &count,
                              &dropped, &saturated) != T64_STATUS_OK ||
      dropped != 0u || saturated) {
    report->status = T64_RSP_DIAG_INVALID_BATCH;
    report->outcome = T64_RSP_DIAG_OUTCOME_HARNESS_ERROR;
    return false;
  }
  for (i = 0u; i < count; i++) {
    uint32_t pc;
    uint32_t opcode;
    if (trace[i].device != T64_TRACE_DEVICE_MACHINE) continue;
    if (trace[i].kind == T64_TRACE_KIND_CPU_UNSUPPORTED) {
      report->pc = (uint32_t)(trace[i].value >> 32);
      report->opcode = (uint32_t)trace[i].value;
      continue;
    }
    if (trace[i].kind != T64_TRACE_KIND_CPU_RETIREMENT) continue;
    pc = (uint32_t)(trace[i].value >> 32);
    opcode = (uint32_t)trace[i].value;
    report->retired++;
    report->pc = pc;
    report->opcode = opcode;
    if (report->have_previous_cpu_retirement != 0u &&
        report->previous_cpu_pc == T64_RSP_DIAG_TERMINAL_PC &&
        report->previous_cpu_opcode == T64_RSP_DIAG_TERMINAL_OPCODE &&
        pc == T64_RSP_DIAG_TERMINAL_PC + 4u &&
        opcode == T64_RSP_DIAG_TERMINAL_DELAY_SLOT_OPCODE) {
      report->terminal_pc = report->previous_cpu_pc;
      report->terminal_opcode = report->previous_cpu_opcode;
      report->terminal_delay_slot_pc = pc;
      report->terminal_delay_slot_opcode = opcode;
      report->terminal_delay_slot_retired = 1u;
    }
    report->have_previous_cpu_retirement = 1u;
    report->previous_cpu_pc = pc;
    report->previous_cpu_opcode = opcode;
  }
  if (t64_machine_output_drain(
          machine, available == 0u ? NULL : report->capture + report->capture_length,
          available, &output) != T64_STATUS_OK || output.lost_output ||
      !output.total_attempted_bytes_exact ||
      output.total_attempted_bytes > RSP_CAPTURE_LIMIT ||
      output.copied_bytes > available) {
    report->status = T64_RSP_DIAG_OUTPUT_OVERFLOW;
    report->outcome = T64_RSP_DIAG_OUTCOME_HARNESS_ERROR;
    return false;
  }
  report->capture_length += (uint32_t)output.copied_bytes;
  if (output.total_attempted_bytes != report->capture_length) {
    report->status = T64_RSP_DIAG_INVALID_BATCH;
    report->outcome = T64_RSP_DIAG_OUTCOME_HARNESS_ERROR;
    return false;
  }
  return true;
}

static void execute_guest(int snapshot_fd, size_t rom_size,
                          const char *rom_sha256, uint64_t tick_bound,
                          uint32_t registration_index, uint32_t value_index,
                          rsp_child_report *report) {
  const t64_profile_descriptor *profile = NULL;
  t64_host_allocator allocator = {allocate_machine, release_machine, NULL};
  t64_machine *machine = NULL;
  uint8_t *rom = NULL;
  uint64_t current_tick = 0u;
  uint64_t total_captured = 0u;
  t64_rsp_diag_guest_result guest_result = {0u, 0u, T64_RSP_DIAG_OUTCOME_INCOMPLETE};
  memset(report, 0, sizeof *report);
  report->outcome = T64_RSP_DIAG_OUTCOME_HARNESS_ERROR;
  report->status = T64_RSP_DIAG_INVALID_ARGUMENT;
  if (tick_bound == 0u || tick_bound > RSP_TICK_LIMIT ||
      !read_snapshot(snapshot_fd, rom_size, rom_sha256, &rom)) goto done;
  if (t64_profile_lookup(RSP_PROFILE_ID, &profile) != T64_STATUS_OK ||
      profile == NULL ||
      t64_machine_create(&allocator, profile, &machine) != T64_STATUS_OK ||
      machine == NULL ||
      t64_machine_trace_set_enabled(machine, true) != T64_STATUS_OK ||
      t64_machine_admit_rom(machine, rom, rom_size) !=
          T64_STATUS_OK) {
    report->status = T64_RSP_DIAG_IDENTITY_MISMATCH;
    goto done;
  }
  report->status = T64_RSP_DIAG_INCOMPLETE;
  while (current_tick < tick_bound) {
    uint64_t target = tick_bound - current_tick > RSP_STEP_TICKS
                          ? current_tick + RSP_STEP_TICKS : tick_bound;
    t64_stop_result stop = {0u, 0u};
    t64_status status = t64_machine_advance(
        machine, &(t64_stop_request){T64_STOP_GUEST_TICK, target}, &stop);
    if (status != T64_STATUS_OK || stop.tick < current_tick ||
        stop.tick > target) {
      report->status = T64_RSP_DIAG_INVALID_ARGUMENT;
      report->outcome = T64_RSP_DIAG_OUTCOME_HARNESS_ERROR;
      break;
    }
    if (!drain_guest_observers(machine, report)) break;
    total_captured = report->capture_length;
    if (machine->rsp_observation_valid) {
      report->pc = machine->rsp_observed_pc;
      report->opcode = machine->rsp_observed_opcode;
      report->refusal = machine->rsp_observed_refusal_reason;
      if (machine->rsp_observed_outcome == T64_RSP_ATTEMPT_REFUSED) {
        report->outcome = T64_RSP_DIAG_OUTCOME_REFUSED;
        report->status = T64_RSP_DIAG_INVALID_BATCH;
        report->tick = stop.tick;
        break;
      }
    }
    current_tick = stop.tick;
    report->tick = current_tick;
    if (stop.reason == T64_STOP_REASON_CPU_UNSUPPORTED) {
      t64_cpu_stop_detail detail = {0u, 0u, 0u};
      if (t64_machine_cpu_stop_detail(machine, &detail) == T64_STATUS_OK) {
        report->pc = detail.pc;
        report->opcode = detail.opcode;
      }
      report->outcome = T64_RSP_DIAG_OUTCOME_REFUSED;
      report->status = T64_RSP_DIAG_INVALID_BATCH;
      report->refusal = UINT32_C(0x100);
      break;
    }
    if (stop.reason != T64_STOP_REASON_BOUND || current_tick != target) {
      report->outcome = T64_RSP_DIAG_OUTCOME_INCOMPLETE;
      report->status = T64_RSP_DIAG_INCOMPLETE;
      break;
    }
    if (report->terminal_delay_slot_retired != 0u) {
      t64_rsp_diag_status transcript_status =
          t64_rsp_diag_guest_transcript_parse(
              report->capture, report->capture_length,
              registration_index, value_index, &guest_result);
      if (transcript_status == T64_RSP_DIAG_OK) {
        report->outcome = guest_result.outcome;
        report->status = T64_RSP_DIAG_OK;
      } else if (transcript_status == T64_RSP_DIAG_INCOMPLETE) {
        report->outcome = T64_RSP_DIAG_OUTCOME_INCOMPLETE;
        report->status = T64_RSP_DIAG_INCOMPLETE;
      } else {
        report->outcome = T64_RSP_DIAG_OUTCOME_HARNESS_ERROR;
        report->status = T64_RSP_DIAG_INVALID_BATCH;
      }
      break;
    }
  }
  if (report->outcome != T64_RSP_DIAG_OUTCOME_REFUSED &&
      report->outcome != T64_RSP_DIAG_OUTCOME_PASS &&
      report->outcome != T64_RSP_DIAG_OUTCOME_GUEST_FAIL &&
      report->outcome != T64_RSP_DIAG_OUTCOME_HARNESS_ERROR) {
    report->outcome = T64_RSP_DIAG_OUTCOME_INCOMPLETE;
    report->status = T64_RSP_DIAG_INCOMPLETE;
  }
  if (total_captured != report->capture_length) {
    report->outcome = T64_RSP_DIAG_OUTCOME_HARNESS_ERROR;
    report->status = T64_RSP_DIAG_INVALID_BATCH;
  }
done:
  if (machine != NULL) t64_machine_destroy(machine);
  free(rom);
}

static uint64_t monotonic_ms(void) {
  struct timespec value;
  if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) return 0u;
  return (uint64_t)value.tv_sec * 1000u + (uint64_t)value.tv_nsec / 1000000u;
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

static bool supervised_guest(t64_verified_input *snapshot, size_t rom_size,
                             const char *rom_sha256, uint64_t tick_bound,
                             uint32_t registration_index, uint32_t value_index,
                             rsp_child_report *report, bool *timed_out) {
  int pipe_fds[2] = {-1, -1};
  pid_t child;
  int child_status = 0;
  size_t received = 0u;
  bool child_done = false;
  bool pipe_closed = false;
  uint64_t start;
  *timed_out = false;
  if (pipe(pipe_fds) != 0) return false;
  child = fork();
  if (child < 0) { close(pipe_fds[0]); close(pipe_fds[1]); return false; }
  if (child == 0) {
    rsp_child_report child_report;
    (void)setpgid(0, 0);
    close(pipe_fds[0]);
    execute_guest(snapshot->fd, rom_size, rom_sha256, tick_bound,
                  registration_index, value_index, &child_report);
    (void)write_all(pipe_fds[1], &child_report, sizeof child_report);
    close(pipe_fds[1]);
    _exit(0);
  }
  (void)setpgid(child, child);
  close(pipe_fds[1]);
  start = monotonic_ms();
  while (!child_done || received < sizeof *report) {
    struct pollfd event = {pipe_closed ? -1 : pipe_fds[0], POLLIN | POLLHUP, 0};
    int polled = poll(&event, 1u, 20);
    if (!pipe_closed && polled > 0 &&
        (event.revents & (POLLIN | POLLHUP)) != 0) {
      ssize_t count = read(pipe_fds[0], (uint8_t *)report + received,
                           sizeof *report - received);
      if (count < 0 && errno != EINTR) break;
      if (count == 0) pipe_closed = true;
      if (count > 0) received += (size_t)count;
    }
    if (waitpid(child, &child_status, WNOHANG) == child) child_done = true;
    if (monotonic_ms() - start >= RSP_CHILD_LIMIT_MS) {
      *timed_out = true;
      (void)kill(-child, SIGKILL);
      break;
    }
    if (child_done && pipe_closed && received < sizeof *report) break;
  }
  close(pipe_fds[0]);
  if (*timed_out) {
    while (waitpid(child, &child_status, 0) < 0 && errno == EINTR) {}
    return false;
  }
  if (!child_done) {
    while (waitpid(child, &child_status, 0) < 0) {
      if (errno != EINTR) return false;
    }
  }
  return received == sizeof *report && WIFEXITED(child_status) &&
         WEXITSTATUS(child_status) == 0;
}

static int command_run_local(int argc, char **argv) {
  t64_rsp_diag_inventory *inventory = NULL;
  t64_rsp_diag_member member;
  const t64_profile_descriptor *profile = NULL;
  t64_verified_input snapshot = {-1, 0u, {0u}, {0}, 0u};
  rsp_child_report report;
  t64_rsp_diag_member_result result = {0};
  uint64_t rom_size, ticks, registration, value;
  char profile_hex[T64_SHA256_HEX_SIZE];
  char output_path[RSP_PATH_LIMIT];
  char batch[T64_RSP_DIAG_MAX_BATCH_BYTES];
  size_t batch_length = 0u;
  bool timed_out = false;
  int exit_code = 4;
  if (argc != 11 || strcmp(argv[2], "--local-only") != 0 ||
      !local_execution_allowed()) {
    (void)puts("classification=harness-error status=local-only-refused");
    return 4;
  }
  if (!private_output_path(argv[10], output_path, sizeof output_path)) {
    (void)puts("classification=harness-error status=output-not-private");
    return 4;
  }
  if (!parse_u64(argv[5], &rom_size) || rom_size == 0u || rom_size > RSP_ROM_LIMIT ||
      !valid_hex(argv[6], 64u) || !parse_u64(argv[7], &ticks) || ticks == 0u ||
      ticks != RSP_TICK_LIMIT || !parse_u64(argv[8], &registration) ||
      registration > UINT32_MAX || !parse_u64(argv[9], &value) ||
      value > UINT32_MAX) {
    (void)puts("classification=harness-error status=invalid-bound");
    return 4;
  }
  if (!parse_inventory_file(argv[3], &inventory) ||
      t64_rsp_diag_inventory_required_value_count(inventory) == 0u) {
    (void)puts("classification=harness-error status=invalid-inventory");
    goto cleanup;
  }
  if (runtime_identity(inventory, &profile, profile_hex) != T64_RSP_DIAG_OK) {
    (void)puts("classification=harness-error status=runtime-identity-mismatch");
    goto cleanup;
  }
  {
    size_t ordinal;
    bool found = false;
    for (ordinal = 0u;
         ordinal < t64_rsp_diag_inventory_required_value_count(inventory);
         ordinal++) {
      if (t64_rsp_diag_inventory_member_at(inventory, ordinal, &member) &&
          member.registration_index == (uint32_t)registration &&
          member.value_index == (uint32_t)value) {
        found = true;
        break;
      }
    }
    if (!found || strcmp(member.selected_rom_sha256, argv[6]) != 0) {
      (void)puts("classification=harness-error status=unregistered-member-or-rom-mismatch");
      goto cleanup;
    }
  }
  if (t64_verified_input_prepare(argv[4], rom_size, argv[6], RSP_ROM_LIMIT,
                                 &snapshot) != T64_VERIFIED_INPUT_OK) {
    (void)puts("classification=harness-error status=source-snapshot-refused");
    goto cleanup;
  }
  if (!supervised_guest(&snapshot, (size_t)rom_size, argv[6], ticks,
                        (uint32_t)registration, (uint32_t)value,
                        &report, &timed_out)) {
    if (timed_out) {
      report = (rsp_child_report){0};
      report.outcome = T64_RSP_DIAG_OUTCOME_TIMEOUT;
      report.status = T64_RSP_DIAG_INCOMPLETE;
      report.tick = 0u;
      report.refusal = 0u;
    } else {
      (void)puts("classification=harness-error status=child-report-or-cleanup-failed");
      goto cleanup;
    }
  }
  {
    uint8_t result_digest[T64_SHA256_DIGEST_SIZE];
    char result_hex[T64_SHA256_HEX_SIZE];
    uint8_t capture_digest[T64_SHA256_DIGEST_SIZE];
    char capture_hex[T64_SHA256_HEX_SIZE];
    char material[256];
    int material_length = snprintf(material, sizeof material,
        "%u:%u:%08x:%08x:%u:%llu:%u", report.outcome, report.status,
        report.pc, report.opcode, report.refusal,
        (unsigned long long)report.tick, report.capture_length);
    digest_bytes(report.capture, report.capture_length, capture_digest);
    t64_sha256_hex(capture_digest, capture_hex);
    if (material_length < 0 || (size_t)material_length >= sizeof material) {
      (void)puts("classification=harness-error status=result-hash-failed");
      goto cleanup;
    }
    digest_bytes(material, (size_t)material_length, result_digest);
    t64_sha256_hex(result_digest, result_hex);
    result.outcome = (t64_rsp_diag_outcome)report.outcome;
    result.operation = report.refusal == UINT32_C(0x100) ? "cpu-opcode" :
                       report.outcome == T64_RSP_DIAG_OUTCOME_REFUSED
                           ? "rsp-opcode" : "guest-run";
    if (report.outcome == T64_RSP_DIAG_OUTCOME_REFUSED) {
      static char refusal_reason[96];
      (void)snprintf(refusal_reason, sizeof refusal_reason,
                     "refusal-code-%u", report.refusal);
      result.reason = refusal_reason;
    } else if (timed_out) result.reason = "child-wall-deadline-expired";
    else if (report.outcome == T64_RSP_DIAG_OUTCOME_HARNESS_ERROR)
      result.reason = "guest-capture-or-machine-error";
    else result.reason = "guest-tick-bound-before-authenticated-terminal";
    result.retired_instructions = report.retired;
    result.terminal_pc = report.terminal_pc;
    result.terminal_opcode = report.terminal_opcode;
    result.terminal_delay_slot_pc = report.terminal_delay_slot_pc;
    result.terminal_delay_slot_opcode = report.terminal_delay_slot_opcode;
    result.terminal_delay_slot_retired =
        report.terminal_delay_slot_retired != 0u;
    if (report.outcome == T64_RSP_DIAG_OUTCOME_PASS) {
      result.operation = "";
      result.reason = "";
    } else if (report.outcome == T64_RSP_DIAG_OUTCOME_GUEST_FAIL) {
      result.operation = "guest-assertion";
      result.reason = "rsp-test-reported-failure";
    }
    result.result_sha256 = result_hex;
    result.capture_sha256 = capture_hex;
    if (t64_rsp_diag_batch_encode(inventory, (uint32_t)registration,
                                  (uint32_t)value, &result, batch,
                                  sizeof batch, &batch_length) != T64_RSP_DIAG_OK ||
        !write_private_file(output_path, batch, batch_length)) {
      (void)puts("classification=harness-error status=batch-encoding-or-write-failed");
      goto cleanup;
    }
  }
  if (report.outcome == T64_RSP_DIAG_OUTCOME_REFUSED) {
    (void)printf("classification=refused pc=%03x opcode=%08x operation=%s reason=refusal-code-%u tick=%llu retired=%llu capture=%u\n",
        report.pc, report.opcode,
        report.refusal == UINT32_C(0x100) ? "cpu-opcode" : "rsp-opcode",
        report.refusal, (unsigned long long)report.tick,
        (unsigned long long)report.retired, report.capture_length);
    exit_code = 2;
  } else if (report.outcome == T64_RSP_DIAG_OUTCOME_PASS) {
    (void)printf("classification=pass terminal_pc=%08x terminal_opcode=%08x delay_slot_pc=%08x delay_slot_opcode=%08x tick=%llu retired=%llu capture=%u\n",
        report.terminal_pc, report.terminal_opcode,
        report.terminal_delay_slot_pc, report.terminal_delay_slot_opcode,
        (unsigned long long)report.tick, (unsigned long long)report.retired,
        report.capture_length);
    exit_code = 0;
  } else if (report.outcome == T64_RSP_DIAG_OUTCOME_GUEST_FAIL) {
    (void)printf("classification=guest-fail terminal_pc=%08x terminal_opcode=%08x delay_slot_pc=%08x delay_slot_opcode=%08x tick=%llu retired=%llu capture=%u\n",
        report.terminal_pc, report.terminal_opcode,
        report.terminal_delay_slot_pc, report.terminal_delay_slot_opcode,
        (unsigned long long)report.tick, (unsigned long long)report.retired,
        report.capture_length);
    exit_code = 2;
  } else if (timed_out) {
    (void)puts("classification=timeout stage=bounded-child");
    exit_code = 3;
  } else if (report.outcome == T64_RSP_DIAG_OUTCOME_HARNESS_ERROR) {
    (void)printf("classification=harness-error status=%s pc=%08x opcode=%08x tick=%llu retired=%llu capture=%u\n",
        t64_rsp_diag_status_name((t64_rsp_diag_status)report.status), report.pc,
        report.opcode, (unsigned long long)report.tick,
        (unsigned long long)report.retired, report.capture_length);
    exit_code = 4;
  } else {
    (void)printf("classification=incomplete pc=%03x opcode=%08x tick=%llu retired=%llu capture=%u\n",
        report.pc, report.opcode, (unsigned long long)report.tick,
        (unsigned long long)report.retired, report.capture_length);
    exit_code = 3;
  }
cleanup:
  if (inventory != NULL) t64_rsp_diag_inventory_destroy(inventory);
  t64_verified_input_release(&snapshot);
  return exit_code;
}

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "--runtime-identity-json") == 0)
    return command_runtime_identity();
  if (argc == 3 && strcmp(argv[1], "--validate-inventory") == 0)
    return command_validate_inventory(argv[2]);
  if (argc == 4 && strcmp(argv[1], "--validate-batch-json") == 0)
    return command_validate_batch(argv[2], argv[3]);
  if (argc == 4 && strcmp(argv[1], "--verify-complete-json") == 0)
    return command_verify_complete(argv[2], argv[3]);
  if (argc > 1 && strcmp(argv[1], "--run-local") == 0)
    return command_run_local(argc, argv);
  (void)fprintf(stderr,
      "usage: t64-rsp-diagnostic --runtime-identity-json | --validate-inventory FILE | --validate-batch-json INVENTORY BATCH | --verify-complete-json INVENTORY PRIVATE_BATCH_DIR | --run-local --local-only INVENTORY ROM SIZE SHA256 TICKS REG VALUE OUTPUT\n");
  return 64;
}
