/* Local BASE inventory and authenticated one-member record adapter. */
#define _POSIX_C_SOURCE 200809L
#include "t64_base_diag_protocol.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "t64_cpu_diag_protocol.h"
#include "t64_diag_common.h"
#include "t64_jsonw.h"
#include "t64_sha256.h"
#include "t64_verified_input.h"
#include "tendies64/t64_machine.h"

#ifndef T64_BASE_PRIVATE_ROOT
#define T64_BASE_PRIVATE_ROOT "build/private-base-diagnostic"
#endif

#define BASE_FILE_LIMIT T64_BASE_DIAG_MAX_INVENTORY_BYTES
#define BASE_ROM_LIMIT UINT64_C(0x00401000)
#define BASE_TICK_LIMIT UINT64_C(10000000)
#define BASE_STEP_TICKS UINT64_C(16)
#define BASE_MAX_RESOURCE_YIELDS 64u
#define BASE_CAPTURE_CAPACITY 4096u
#define BASE_PARK_PC UINT32_C(0x80000420)
#define BASE_PARK_OPCODE UINT32_C(0x08000108)

typedef struct {
  uint8_t *bytes;
  size_t length;
} file_bytes;

static void print_refusal(const char *reason) {
  (void)printf("classification=harness-error status=%s\n", reason);
}

static bool local_execution_allowed(void) {
  const char *ci = getenv("CI");
  const char *require_fixtures = getenv("T64_REQUIRE_FIXTURES");
  return (ci == NULL || (strcmp(ci, "true") != 0 && strcmp(ci, "1") != 0)) &&
         (require_fixtures == NULL || strcmp(require_fixtures, "1") != 0);
}

static bool read_file(const char *path, size_t maximum, file_bytes *file) {
  struct stat before;
  struct stat opened;
  int descriptor;
  size_t offset = 0u;
  uint8_t *bytes;
  if (path == NULL || file == NULL || lstat(path, &before) != 0 ||
      !S_ISREG(before.st_mode) || S_ISLNK(before.st_mode) ||
      (before.st_mode & (S_IWGRP | S_IWOTH)) != 0 || before.st_size <= 0 ||
      (uintmax_t)before.st_size > (uintmax_t)maximum) {
    return false;
  }
  descriptor = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
  if (descriptor < 0 || fstat(descriptor, &opened) != 0 ||
      !S_ISREG(opened.st_mode) ||
      opened.st_dev != before.st_dev || opened.st_ino != before.st_ino ||
      opened.st_size != before.st_size) {
    if (descriptor >= 0) (void)close(descriptor);
    return false;
  }
  bytes = (uint8_t *)malloc((size_t)opened.st_size);
  if (bytes == NULL) {
    (void)close(descriptor);
    return false;
  }
  while (offset < (size_t)opened.st_size) {
    ssize_t count = read(descriptor, bytes + offset,
                         (size_t)opened.st_size - offset);
    if (count <= 0) {
      free(bytes);
      (void)close(descriptor);
      return false;
    }
    offset += (size_t)count;
  }
  if (close(descriptor) != 0) {
    free(bytes);
    return false;
  }
  file->bytes = bytes;
  file->length = offset;
  return true;
}

static bool read_file_allow_empty(const char *path, size_t maximum,
                                  file_bytes *file) {
  struct stat before;
  struct stat opened;
  int descriptor;
  size_t offset = 0u;
  size_t length;
  uint8_t *bytes;
  if (path == NULL || file == NULL || lstat(path, &before) != 0 ||
      !S_ISREG(before.st_mode) || S_ISLNK(before.st_mode) ||
      (before.st_mode & (S_IWGRP | S_IWOTH)) != 0 || before.st_size < 0 ||
      (uintmax_t)before.st_size > (uintmax_t)maximum) return false;
  descriptor = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
  if (descriptor < 0 || fstat(descriptor, &opened) != 0 ||
      !S_ISREG(opened.st_mode) || opened.st_dev != before.st_dev ||
      opened.st_ino != before.st_ino || opened.st_size != before.st_size) {
    if (descriptor >= 0) (void)close(descriptor);
    return false;
  }
  length = (size_t)opened.st_size;
  bytes = (uint8_t *)malloc(length == 0u ? 1u : length);
  if (bytes == NULL) {
    (void)close(descriptor);
    return false;
  }
  while (offset < length) {
    ssize_t count = read(descriptor, bytes + offset, length - offset);
    if (count <= 0) {
      free(bytes);
      (void)close(descriptor);
      return false;
    }
    offset += (size_t)count;
  }
  if (close(descriptor) != 0) {
    free(bytes);
    return false;
  }
  file->bytes = bytes;
  file->length = length;
  return true;
}

static void free_file(file_bytes *file) {
  if (file == NULL) return;
  free(file->bytes);
  file->bytes = NULL;
  file->length = 0u;
}

static bool parse_u32(const char *text, uint32_t *value_out) {
  uint64_t value = 0u;
  size_t i;
  if (text == NULL || text[0] == '\0' || value_out == NULL) return false;
  for (i = 0u; text[i] != '\0'; i++) {
    unsigned char digit = (unsigned char)text[i];
    if (digit < (unsigned char)'0' || digit > (unsigned char)'9' ||
        value > (UINT32_MAX - (uint64_t)(digit - (unsigned char)'0')) / 10u) {
      return false;
    }
    value = value * 10u + (uint64_t)(digit - (unsigned char)'0');
  }
  if (i > 1u && text[0] == '0') return false;
  *value_out = (uint32_t)value;
  return true;
}

static bool parse_u64(const char *text, uint64_t *value_out) {
  uint64_t value = 0u;
  size_t i;
  if (text == NULL || text[0] == '\0' || value_out == NULL) return false;
  for (i = 0u; text[i] != '\0'; i++) {
    unsigned char digit = (unsigned char)text[i];
    if (digit < (unsigned char)'0' || digit > (unsigned char)'9' ||
        value > (UINT64_MAX - (uint64_t)(digit - (unsigned char)'0')) / 10u)
      return false;
    value = value * 10u + (uint64_t)(digit - (unsigned char)'0');
  }
  if (i > 1u && text[0] == '0') return false;
  *value_out = value;
  return true;
}

static bool private_output_path(const char *path) {
  const char *root = T64_BASE_PRIVATE_ROOT;
#ifdef T64_BASE_DIAGNOSTIC_TESTING
  const char *control_root = getenv("T64_BASE_CONTROL_ROOT");
  if (control_root != NULL && control_root[0] == '/') root = control_root;
#endif
  const size_t root_length = strlen(root);
  const char *cursor;
  struct stat info;
  if (lstat(root, &info) != 0 || S_ISLNK(info.st_mode) ||
      !S_ISDIR(info.st_mode) || (info.st_mode & 0077) != 0) {
    return false;
  }
  if (path == NULL || strncmp(path, root, root_length) != 0 ||
      path[root_length] != '/') {
    return false;
  }
  cursor = path + root_length + 1u;
  while (*cursor != '\0') {
    const char *slash = strchr(cursor, '/');
    size_t length = slash == NULL ? strlen(cursor) : (size_t)(slash - cursor);
    char component[PATH_MAX];
    size_t prefix_length = (size_t)(cursor - path);
    if (length == 0u ||
        (length == 1u && cursor[0] == '.') ||
        (length == 2u && cursor[0] == '.' && cursor[1] == '.') ||
        prefix_length + length >= sizeof component) return false;
    (void)memcpy(component, path, prefix_length + length);
    component[prefix_length + length] = '\0';
    if (slash == NULL) {
      if (lstat(component, &info) == 0) return false;
      return errno == ENOENT;
    }
    if (lstat(component, &info) != 0 || S_ISLNK(info.st_mode)) return false;
    if (!S_ISDIR(info.st_mode) || (info.st_mode & 0077) != 0) return false;
    cursor = slash + 1;
  }
  return false;
}

static bool current_target(const char *context_sha256,
                           t64_base_diag_target *target,
                           char profile_hex[T64_SHA256_HEX_SIZE]) {
  const t64_profile_descriptor *profile = NULL;
  uint8_t profile_hash[T64_DIGEST_SIZE];
  if (t64_profile_lookup("ntsc-cart-4m-openipl3-rdram-b-t1-v1", &profile) !=
          T64_STATUS_OK || profile == NULL ||
      t64_profile_hash(profile, profile_hash) != T64_STATUS_OK) return false;
  t64_sha256_hex(profile_hash, profile_hex);
  *target = (t64_base_diag_target){context_sha256, profile->profile_id,
                                  profile_hex, t64_build_identity()};
  return target->core_build_id != NULL;
}

static bool write_private_file(const char *path, const uint8_t *bytes,
                               size_t length) {
  int descriptor;
  size_t offset = 0u;
  if (!private_output_path(path)) return false;
  descriptor = open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
  if (descriptor < 0) return false;
  while (offset < length) {
    ssize_t count = write(descriptor, bytes + offset, length - offset);
    if (count <= 0) {
      (void)close(descriptor);
      (void)unlink(path);
      return false;
    }
    offset += (size_t)count;
  }
  if (fsync(descriptor) != 0) {
    (void)close(descriptor);
    (void)unlink(path);
    return false;
  }
  if (close(descriptor) != 0) {
    (void)unlink(path);
    return false;
  }
  return true;
}

static bool allocate_machine(void *context, size_t size, size_t alignment,
                             t64_host_storage *storage_out) {
  void *memory = NULL;
  const size_t host_alignment = alignment < sizeof(void *) ? sizeof(void *) : alignment;
  (void)context;
  if (storage_out == NULL || alignment == 0u || (alignment & (alignment - 1u)) != 0u ||
      posix_memalign(&memory, host_alignment, size) != 0) return false;
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = host_alignment;
  return true;
}

static void release_machine(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static bool snapshot_bytes(const t64_verified_input *snapshot,
                           uint8_t **bytes_out) {
  uint8_t *bytes;
  size_t offset = 0u;
  if (snapshot == NULL || snapshot->fd < 0 || bytes_out == NULL ||
      snapshot->size == 0u || snapshot->size > BASE_ROM_LIMIT) return false;
  bytes = (uint8_t *)malloc((size_t)snapshot->size);
  if (bytes == NULL) return false;
  if (lseek(snapshot->fd, 0, SEEK_SET) != 0) {
    free(bytes);
    return false;
  }
  while (offset < (size_t)snapshot->size) {
    ssize_t count = read(snapshot->fd, bytes + offset,
                         (size_t)snapshot->size - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) {
      free(bytes);
      return false;
    }
    offset += (size_t)count;
  }
  *bytes_out = bytes;
  return true;
}

static bool save_capture(const char *path, const uint8_t *capture,
                         size_t length) {
  static const uint8_t empty_capture = 0u;
  return write_private_file(path, length == 0u ? &empty_capture : capture,
                            length);
}

static bool observe_machine(t64_machine *machine, uint8_t *capture,
                            size_t *capture_length, uint32_t *saw_entry,
                            uint32_t *saw_park, uint32_t *park_sequence,
                            uint32_t *last_pc, uint32_t *last_opcode,
                            uint64_t *output_total,
                            t64_base_diag_execution *proof) {
  t64_trace_record trace[T64_TRACE_RECORD_CAPACITY];
  size_t count = 0u;
  uint64_t dropped = 0u;
  bool saturated = false;
  size_t i;
  t64_output_drain_result output = T64_OUTPUT_DRAIN_RESULT_INIT;
  if (t64_machine_trace_drain(machine, trace,
                              (uint32_t)sizeof(t64_trace_record),
                              sizeof trace / sizeof trace[0], &count,
                              &dropped, &saturated) != T64_STATUS_OK ||
      dropped != 0u || saturated) return false;
  for (i = 0u; i < count; i++) {
    uint32_t pc;
    uint32_t opcode;
    if (trace[i].device != T64_TRACE_DEVICE_MACHINE) continue;
    if (trace[i].kind == T64_TRACE_KIND_CPU_UNSUPPORTED) {
      *last_pc = (uint32_t)(trace[i].value >> 32);
      *last_opcode = (uint32_t)trace[i].value;
      continue;
    }
    if (trace[i].kind != T64_TRACE_KIND_CPU_RETIREMENT) continue;
    pc = (uint32_t)(trace[i].value >> 32);
    opcode = (uint32_t)trace[i].value;
    *last_pc = pc;
    *last_opcode = opcode;
    if (pc == UINT32_C(0x80000400)) *saw_entry = 1u;
    if (pc == BASE_PARK_PC && opcode == BASE_PARK_OPCODE) *saw_park = 1u;
    if (pc == BASE_PARK_PC && opcode == BASE_PARK_OPCODE) {
      *park_sequence = *park_sequence >= 4u ? 5u :
                       *park_sequence == 2u ? 3u : 1u;
    } else if (pc == BASE_PARK_PC + 4u && opcode == 0u &&
               (*park_sequence == 1u || *park_sequence == 3u ||
                *park_sequence == 5u)) {
      *park_sequence = *park_sequence == 5u ? 6u : *park_sequence + 1u;
    } else {
      *park_sequence = 0u;
    }
  }
  if (*capture_length > BASE_CAPTURE_CAPACITY) return false;
  if (t64_machine_output_drain(machine, capture + *capture_length,
                               BASE_CAPTURE_CAPACITY - *capture_length,
                               &output) != T64_STATUS_OK) return false;
  proof->capture_exact = output.total_attempted_bytes_exact;
  proof->capture_lost = output.lost_output;
  proof->capture_total = output.total_attempted_bytes;
  if (output.copied_bytes > BASE_CAPTURE_CAPACITY - *capture_length ||
      output.lost_output || !output.total_attempted_bytes_exact) return false;
  *capture_length += output.copied_bytes;
  *output_total = output.total_attempted_bytes;
  return true;
}

static t64_base_diag_outcome execute_tier_b_t1(
    const t64_verified_input *snapshot, uint64_t tick_bound,
    uint32_t registration_index, uint32_t value_index,
    uint8_t capture[BASE_CAPTURE_CAPACITY], size_t *capture_length,
    uint64_t *tick_out, uint32_t *saw_entry, uint32_t *saw_park,
    uint32_t *last_pc, uint32_t *last_opcode, const char **stage_out,
    t64_base_diag_execution *proof) {
  const t64_profile_descriptor *profile = NULL;
  t64_host_allocator allocator =
      T64_HOST_ALLOCATOR_INIT(allocate_machine, release_machine, NULL);
  t64_machine *machine = NULL;
  uint8_t *rom = NULL;
  uint64_t current_tick = 0u;
  uint64_t output_total = 0u;
  uint64_t previous_resource_tick = UINT64_MAX;
  uint32_t resource_yields = 0u;
  uint32_t park_sequence = 0u;
  t64_base_diag_outcome outcome = T64_BASE_DIAG_OUTCOME_HARNESS_ERROR;
  *stage_out = "input-snapshot";
  *capture_length = 0u;
  *tick_out = 0u;
  *saw_entry = 0u;
  *saw_park = 0u;
  *last_pc = 0u;
  *last_opcode = 0u;
  if (tick_bound == 0u || tick_bound > BASE_TICK_LIMIT ||
      !snapshot_bytes(snapshot, &rom)) goto cleanup;
  *stage_out = "tier-b-t1-profile";
  if (t64_profile_lookup("ntsc-cart-4m-openipl3-rdram-b-t1-v1",
                          &profile) != T64_STATUS_OK || profile == NULL)
    goto cleanup;
  *stage_out = "canonical-machine-create";
  if (t64_machine_create(
          &(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(
              &allocator, profile),
          &machine) != T64_STATUS_OK ||
      machine == NULL ||
      t64_machine_trace_set_enabled(machine, true) != T64_STATUS_OK)
    goto cleanup;
  *stage_out = "canonical-rom-admission";
  if (t64_machine_admit_rom(machine, rom, (size_t)snapshot->size) !=
      T64_STATUS_OK) goto cleanup;
  while (current_tick < tick_bound) {
    uint64_t target = tick_bound - current_tick > BASE_STEP_TICKS
                          ? current_tick + BASE_STEP_TICKS : tick_bound;
    t64_stop_result stop = T64_STOP_RESULT_INIT;
    *stage_out = "canonical-core-advance";
    if (t64_machine_advance(
            machine,
            &(t64_stop_request)T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK,
                                                     target),
            &stop) != T64_STATUS_OK || stop.tick < current_tick ||
        stop.tick > target ||
        !observe_machine(machine, capture, capture_length, saw_entry,
                         saw_park, &park_sequence, last_pc, last_opcode,
                         &output_total, proof)) goto cleanup;
    current_tick = stop.tick;
    *tick_out = current_tick;
    if (stop.reason == T64_STOP_REASON_RESOURCE_LIMIT) {
      resource_yields++;
      if (resource_yields > BASE_MAX_RESOURCE_YIELDS ||
          (current_tick == previous_resource_tick && stop.tick != target))
        goto cleanup;
      previous_resource_tick = current_tick;
      continue;
    }
    if (stop.reason == T64_STOP_REASON_CPU_UNSUPPORTED) {
      *stage_out = "canonical-core-cpu-unsupported";
      outcome = T64_BASE_DIAG_OUTCOME_GUEST_STOP;
      goto cleanup;
    }
    if (stop.reason != T64_STOP_REASON_BOUND || current_tick != target)
      goto cleanup;
    resource_yields = 0u;
    previous_resource_tick = UINT64_MAX;
    if (park_sequence >= 4u && *saw_entry != 0u &&
        output_total == *capture_length) {
      t64_cpu_diag_guest_result result;
      if (t64_cpu_diag_guest_transcript_parse(
              capture, *capture_length, registration_index, value_index,
              &result) == T64_CPU_DIAG_OK) {
        outcome = result.outcome == T64_CPU_DIAG_OUTCOME_PASS
                      ? T64_BASE_DIAG_OUTCOME_PASS
                      : result.outcome == T64_CPU_DIAG_OUTCOME_FAIL
                            ? T64_BASE_DIAG_OUTCOME_FAIL
                            : T64_BASE_DIAG_OUTCOME_INCOMPLETE;
        *stage_out = "guest-transcript-terminal";
        goto cleanup;
      }
    }
  }
  *stage_out = "guest-tick-bound";
  outcome = *saw_entry != 0u && *saw_park != 0u
                ? T64_BASE_DIAG_OUTCOME_INCOMPLETE
                : T64_BASE_DIAG_OUTCOME_GUEST_STOP;
cleanup:
  proof->tick = *tick_out;
  proof->entry_reached = *saw_entry != 0u;
  proof->park_sequence = park_sequence;
  /* The sequence recognizes both real jump and NOP retirements. */
  if (park_sequence >= 4u) {
    proof->terminal_pc = BASE_PARK_PC;
    proof->terminal_opcode = BASE_PARK_OPCODE;
    proof->terminal_delay_opcode = 0u;
  }
  if (machine != NULL) t64_machine_destroy(machine);
  free(rom);
  return outcome;
}

static int command_inventory(int argc, char **argv) {
  file_bytes authority = {NULL, 0u};
  file_bytes inventory_bytes = {NULL, 0u};
  t64_base_diag_inventory *inventory = NULL;
  t64_base_diag_status status;
  int exit_code = 4;
  if (argc != 5 ||
      !read_file(argv[2], BASE_FILE_LIMIT, &authority) ||
      !read_file(argv[4], BASE_FILE_LIMIT, &inventory_bytes)) {
    print_refusal("inventory-input-refused");
    goto cleanup;
  }
  status = t64_base_diag_inventory_parse(
      authority.bytes, authority.length, argv[3], inventory_bytes.bytes,
      inventory_bytes.length, &inventory);
  if (status != T64_BASE_DIAG_OK) {
    print_refusal(t64_base_diag_status_name(status));
    goto cleanup;
  }
  (void)puts("classification=source-authority-accepted");
  exit_code = 0;
cleanup:
  t64_base_diag_inventory_destroy(inventory);
  free_file(&authority);
  free_file(&inventory_bytes);
  return exit_code;
}

static int command_aggregate(int argc, char **argv) {
  file_bytes authority = {NULL, 0u};
  file_bytes base_bytes = {NULL, 0u};
  file_bytes record = {NULL, 0u};
  file_bytes original = {NULL, 0u};
  t64_base_diag_inventory *base_inventory = NULL;
  t64_base_diag_aggregate *aggregate = NULL;
  t64_base_diag_status status;
  int exit_code = 4;
  if (argc != 7 ||
      !read_file(argv[2], BASE_FILE_LIMIT, &authority) ||
      !read_file(argv[4], BASE_FILE_LIMIT, &base_bytes) ||
      !read_file(argv[5], T64_BASE_DIAG_MAX_RECORD_BYTES, &record) ||
      !read_file_allow_empty(argv[6], BASE_CAPTURE_CAPACITY, &original)) {
    print_refusal("member-input-refused");
    goto cleanup;
  }
  status = t64_base_diag_inventory_parse(
      authority.bytes, authority.length, argv[3], base_bytes.bytes,
      base_bytes.length, &base_inventory);
  if (status != T64_BASE_DIAG_OK) {
    print_refusal(t64_base_diag_status_name(status));
    goto cleanup;
  }
  status = t64_base_diag_aggregate_create(base_inventory, &aggregate);
  if (status == T64_BASE_DIAG_OK)
    status = t64_base_diag_aggregate_add_original(
        aggregate, record.bytes, record.length, original.bytes, original.length);
  if (status != T64_BASE_DIAG_OK || aggregate == NULL ||
      t64_base_diag_aggregate_member_count(aggregate) != 1u ||
      t64_base_diag_aggregate_is_complete(aggregate)) {
    print_refusal(status == T64_BASE_DIAG_OK ? "unexpected-base-completion"
                                             : t64_base_diag_status_name(status));
    goto cleanup;
  }
  (void)puts("classification=member-record-authenticated base_complete=no");
  exit_code = 0;
cleanup:
  t64_base_diag_aggregate_destroy(aggregate);
  t64_base_diag_inventory_destroy(base_inventory);
  free_file(&authority);
  free_file(&base_bytes);
  free_file(&record);
  free_file(&original);
  return exit_code;
}

static int command_run_local(int argc, char **argv) {
  file_bytes authority = {NULL, 0u};
  file_bytes inventory_bytes = {NULL, 0u};
  t64_base_diag_inventory *inventory = NULL;
  t64_base_diag_aggregate *aggregate = NULL;
  t64_base_diag_member member;
  t64_verified_input snapshot = {-1, 0u, {0u}, {0}, 0u};
  t64_base_diag_status protocol_status;
  t64_base_diag_outcome outcome;
  uint64_t rom_size;
  uint64_t tick_bound;
  uint32_t registration_index;
  uint32_t value_index;
  bool member_found = false;
  uint8_t capture[BASE_CAPTURE_CAPACITY];
  uint8_t record[T64_BASE_DIAG_MAX_RECORD_BYTES];
  size_t capture_length = 0u;
  size_t record_length = 0u;
  uint64_t tick = 0u;
  uint32_t saw_entry = 0u;
  uint32_t saw_park = 0u;
  uint32_t last_pc = 0u;
  uint32_t last_opcode = 0u;
  const char *stage = "argument-validation";
  const bool bound = argc == 15 &&
                     strcmp(argv[1], "--run-bound-local") == 0;
  t64_base_diag_target target;
  char profile_hex[T64_SHA256_HEX_SIZE];
  t64_base_diag_execution proof = {
      .selected_rom_sha256 = "", .reason = "adapter-harness-error",
      .capture_exact = true};
  int exit_code = 4;
  if ((!bound && (argc != 14 || strcmp(argv[1], "--run-local") != 0)) ||
      strcmp(argv[2], "--local-only") != 0 ||
      !local_execution_allowed() || !parse_u64(argv[7], &rom_size) ||
      !parse_u64(argv[9], &tick_bound) ||
      !parse_u32(argv[10], &registration_index) ||
      !parse_u32(argv[11], &value_index) || rom_size == 0u ||
      rom_size > BASE_ROM_LIMIT || tick_bound == 0u ||
      tick_bound > BASE_TICK_LIMIT ||
      !read_file(argv[3], BASE_FILE_LIMIT, &authority) ||
      !read_file(argv[5], BASE_FILE_LIMIT, &inventory_bytes)) {
    print_refusal("local-member-arguments-refused");
    goto cleanup;
  }
  protocol_status = t64_base_diag_inventory_parse(
      authority.bytes, authority.length, argv[4], inventory_bytes.bytes,
      inventory_bytes.length, &inventory);
  if (protocol_status != T64_BASE_DIAG_OK) {
    print_refusal(t64_base_diag_status_name(protocol_status));
    goto cleanup;
  }
  for (size_t i = 0u; i < t64_base_diag_inventory_member_count(inventory); i++) {
    if (!t64_base_diag_inventory_member_at(inventory, i, &member)) goto cleanup;
    if (member.registration_index == registration_index &&
        member.value_index == value_index) {
      member_found = true;
      break;
    }
  }
  if (!member_found ||
      (!bound && (!member.required || strcmp(member.lane, "required-base") != 0)) ||
      (strcmp(member.execution, "once") != 0 &&
       strcmp(member.execution, "values") != 0) ||
      (strcmp(member.execution, "once") == 0 &&
       (value_index != 0u || member.value_id[0] != '\0'))) {
    print_refusal("member-not-dispatchable-on-tier-b-t1");
    goto cleanup;
  }
  if (t64_verified_input_prepare(argv[6], rom_size, argv[8],
                                BASE_ROM_LIMIT, &snapshot) !=
      T64_VERIFIED_INPUT_OK) {
    print_refusal("source-image-snapshot-refused");
    goto cleanup;
  }
  /* Validate the expected context before executing any guest instruction. */
  if (bound && (!current_target(argv[14], &target, profile_hex) ||
      t64_base_diag_aggregate_create_bound(inventory, &target, &aggregate) !=
          T64_BASE_DIAG_OK ||
      t64_base_diag_aggregate_bind_image(aggregate, registration_index,
                                        value_index, argv[8]) != T64_BASE_DIAG_OK)) {
    print_refusal("bound-target-refused");
    goto cleanup;
  }
  proof.selected_rom_sha256 = argv[8];
  /* The command carries the selector and value ordinals; the independent
     pre-use host has already matched them to the source projection. */
  outcome = execute_tier_b_t1(&snapshot, tick_bound, registration_index,
                              value_index, capture, &capture_length, &tick,
                              &saw_entry, &saw_park, &last_pc, &last_opcode,
                              &stage, &proof);
  proof.reason = outcome == T64_BASE_DIAG_OUTCOME_PASS ||
                         outcome == T64_BASE_DIAG_OUTCOME_FAIL ? "" :
                 outcome == T64_BASE_DIAG_OUTCOME_INCOMPLETE ?
                     "incomplete-transcript" :
                 outcome == T64_BASE_DIAG_OUTCOME_GUEST_STOP ?
                     (strcmp(stage, "canonical-core-cpu-unsupported") == 0 ?
                         "cpu-unsupported" : "guest-tick-bound") :
                     "adapter-harness-error";
  protocol_status = bound ? t64_base_diag_encode_bound_member_record(
          inventory, registration_index, value_index, outcome, &target,
          &proof, capture, capture_length, record, sizeof record, &record_length) :
      t64_base_diag_encode_member_record(
          inventory, registration_index, value_index, outcome, capture,
          capture_length, record, sizeof record, &record_length);
  if (protocol_status != T64_BASE_DIAG_OK ||
      !save_capture(argv[13], capture, capture_length) ||
      !write_private_file(argv[12], record, record_length)) {
    print_refusal("private-member-output-refused");
    goto cleanup;
  }
  protocol_status = bound ? T64_BASE_DIAG_OK :
      t64_base_diag_aggregate_create(inventory, &aggregate);
  if (protocol_status == T64_BASE_DIAG_OK)
    protocol_status = t64_base_diag_aggregate_add_original(
        aggregate, record, record_length, capture, capture_length);
  if (protocol_status != T64_BASE_DIAG_OK || aggregate == NULL ||
      t64_base_diag_aggregate_member_count(aggregate) != 1u ||
      (!bound && t64_base_diag_aggregate_is_complete(aggregate))) {
    print_refusal("single-member-base-aggregate-refused");
    goto cleanup;
  }
  (void)printf("classification=%s outcome=%s stage=%s tick=%llu entry=%u park=%u last_pc=%08x last_opcode=%08x capture_bytes=%zu base_complete=no\n",
               outcome == T64_BASE_DIAG_OUTCOME_PASS ? "pass" :
               outcome == T64_BASE_DIAG_OUTCOME_FAIL ? "fail" :
               outcome == T64_BASE_DIAG_OUTCOME_INCOMPLETE ? "incomplete" :
               outcome == T64_BASE_DIAG_OUTCOME_GUEST_STOP ? "guest-stop" :
               "harness-error",
               t64_base_diag_outcome_name(outcome), stage,
               (unsigned long long)tick, saw_entry, saw_park, last_pc,
               last_opcode, capture_length);
  exit_code = outcome == T64_BASE_DIAG_OUTCOME_PASS ? 0 :
              outcome == T64_BASE_DIAG_OUTCOME_FAIL ? 2 :
              outcome == T64_BASE_DIAG_OUTCOME_HARNESS_ERROR ? 4 : 3;
cleanup:
  t64_base_diag_aggregate_destroy(aggregate);
  t64_base_diag_inventory_destroy(inventory);
  free_file(&authority);
  free_file(&inventory_bytes);
  t64_verified_input_release(&snapshot);
  return exit_code;
}

static bool json_index(const t64_json_node *object, const char *key,
                        uint32_t *index) {
  const t64_json_node *node = t64_json_get(object, key);
  if (node == NULL || node->type != T64_JSON_INT || node->num < 0 ||
      (uint64_t)node->num > UINT32_MAX) return false;
  *index = (uint32_t)node->num;
  return true;
}

static const char *json_text(const t64_json_node *object, const char *key) {
  const t64_json_node *node = t64_json_get(object, key);
  return t64_diag_json_printable_text(node, PATH_MAX - 1u, true)
             ? node->str : NULL;
}

/* The pre-use host supplies an independently qualified image digest for each
 * selector. C owns membership, record validation and every completeness flag. */
static int command_classify(int argc, char **argv) {
  static const char *const root_keys[] = {"members", "schema", "schema_version"};
  static const char *const row_keys[] = {
      "expected_rom_sha256", "original", "outcome", "reason", "record",
      "registration_index", "value_index"};
  file_bytes authority = {0}, inventory_bytes = {0}, manifest = {0};
  file_bytes record = {0}, original = {0};
  t64_base_diag_inventory *inventory = NULL;
  t64_base_diag_aggregate *aggregate = NULL;
  t64_json_doc document = {0}, record_document = {0};
  t64_json_limits limits;
  t64_json_error error;
  t64_base_diag_target target;
  char profile_hex[T64_SHA256_HEX_SIZE];
  uint8_t *report = NULL;
  size_t report_length = 0u;
  int exit_code = 4;
  const t64_json_node *members, *row, *version;
  if (argc != 8 || !local_execution_allowed() ||
      !read_file(argv[2], BASE_FILE_LIMIT, &authority) ||
      !read_file(argv[4], BASE_FILE_LIMIT, &inventory_bytes) ||
      !read_file(argv[6], T64_BASE_DIAG_MAX_REPORT_BYTES, &manifest) ||
      !private_output_path(argv[7]) ||
      !current_target(argv[5], &target, profile_hex) ||
      t64_base_diag_inventory_parse(authority.bytes, authority.length,
          argv[3], inventory_bytes.bytes, inventory_bytes.length, &inventory) !=
          T64_BASE_DIAG_OK ||
      t64_base_diag_aggregate_create_bound(inventory, &target, &aggregate) !=
          T64_BASE_DIAG_OK) goto cleanup;
  t64_json_limits_default(&limits);
  limits.max_input = T64_BASE_DIAG_MAX_REPORT_BYTES;
  limits.max_nodes = T64_BASE_DIAG_MAX_MEMBERS * 9u + 8u;
  limits.max_string = PATH_MAX - 1u;
  if (t64_json_parse(manifest.bytes, manifest.length, &limits, &document, &error) != 0 ||
      !t64_diag_json_canonical_document(&document, manifest.bytes, manifest.length) ||
      !t64_diag_json_object_has_exact_keys(document.root, root_keys, 3u) ||
      !t64_diag_json_string_equals(t64_json_get(document.root, "schema"),
                                    "t64.base-classification-input") ||
      (version = t64_json_get(document.root, "schema_version")) == NULL ||
      version->type != T64_JSON_INT || version->num != 1 ||
      (members = t64_json_get(document.root, "members")) == NULL ||
      members->type != T64_JSON_ARRAY) goto cleanup;
  for (row = members->first; row != NULL; row = row->next) {
    uint32_t reg, val;
    const char *image, *record_path, *original_path, *outcome_name, *reason;
    t64_base_diag_status status;
    if (!t64_diag_json_object_has_exact_keys(row, row_keys, 7u) ||
        !json_index(row, "registration_index", &reg) ||
        !json_index(row, "value_index", &val) ||
        (image = json_text(row, "expected_rom_sha256")) == NULL ||
        (record_path = json_text(row, "record")) == NULL ||
        (original_path = json_text(row, "original")) == NULL ||
        (outcome_name = json_text(row, "outcome")) == NULL ||
        (reason = json_text(row, "reason")) == NULL) goto cleanup;
    if (image[0] != '\0' && t64_base_diag_aggregate_bind_image(
            aggregate, reg, val, image) != T64_BASE_DIAG_OK) goto cleanup;
    if (strcmp(outcome_name, "record") == 0) {
      uint32_t record_reg, record_val;
      if (reason[0] != '\0' ||
          !read_file(record_path, T64_BASE_DIAG_MAX_RECORD_BYTES, &record) ||
          !read_file_allow_empty(original_path, BASE_CAPTURE_CAPACITY, &original))
        goto cleanup;
      limits.max_input = T64_BASE_DIAG_MAX_RECORD_BYTES;
      limits.max_nodes = 64u;
      if (t64_json_parse(record.bytes, record.length, &limits,
                          &record_document, &error) != 0 ||
          !json_index(record_document.root, "registration_index", &record_reg) ||
          !json_index(record_document.root, "value_index", &record_val) ||
          reg != record_reg || val != record_val) goto cleanup;
      t64_json_free(&record_document);
    } else {
      uint8_t encoded[T64_BASE_DIAG_MAX_RECORD_BYTES];
      static const uint8_t empty = 0u;
      size_t encoded_length = 0u;
      t64_base_diag_outcome outcome;
      t64_base_diag_execution proof = {
          .selected_rom_sha256 = image, .reason = reason, .capture_exact = true};
      if (strcmp(outcome_name, "blocked") == 0) outcome = T64_BASE_DIAG_OUTCOME_BLOCKED;
      else if (strcmp(outcome_name, "unexecuted") == 0) outcome = T64_BASE_DIAG_OUTCOME_UNEXECUTED;
      else if (strcmp(outcome_name, "timeout") == 0) outcome = T64_BASE_DIAG_OUTCOME_TIMEOUT;
      else if (strcmp(outcome_name, "harness-error") == 0) outcome = T64_BASE_DIAG_OUTCOME_HARNESS_ERROR;
      else goto cleanup;
      if (outcome == T64_BASE_DIAG_OUTCOME_TIMEOUT ||
          outcome == T64_BASE_DIAG_OUTCOME_HARNESS_ERROR) {
        proof.capture_exact = false;
        proof.capture_lost = true;
      }
      status = t64_base_diag_encode_bound_member_record(inventory, reg, val,
          outcome, &target, &proof, &empty, 0u, encoded, sizeof encoded,
          &encoded_length);
      if (status != T64_BASE_DIAG_OK ||
          t64_base_diag_aggregate_add_original(aggregate, encoded, encoded_length,
                                               &empty, 0u) != T64_BASE_DIAG_OK ||
          !save_capture(original_path, &empty, 0u) ||
          !write_private_file(record_path, encoded, encoded_length)) goto cleanup;
      continue;
    }
    status = t64_base_diag_aggregate_add_original(aggregate, record.bytes,
        record.length, original.bytes, original.length);
    free_file(&record);
    free_file(&original);
    if (status != T64_BASE_DIAG_OK) goto cleanup;
  }
  report = (uint8_t *)malloc(T64_BASE_DIAG_MAX_REPORT_BYTES);
  if (report == NULL ||
      t64_base_diag_aggregate_report(aggregate, report,
          T64_BASE_DIAG_MAX_REPORT_BYTES, &report_length) != T64_BASE_DIAG_OK ||
      !write_private_file(argv[7], report, report_length)) goto cleanup;
  exit_code = t64_base_diag_aggregate_is_complete(aggregate) ? 0 : 3;
  (void)printf("classification=%s\n", exit_code == 0 ?
      "source-complete-see-private-report" : "source-incomplete-see-private-report");
cleanup:
  if (exit_code == 4) print_refusal("classification-input-refused");
  free(report);
  free_file(&authority); free_file(&inventory_bytes); free_file(&manifest);
  free_file(&record); free_file(&original);
  t64_json_free(&record_document);
  t64_json_free(&document);
  t64_base_diag_aggregate_destroy(aggregate);
  t64_base_diag_inventory_destroy(inventory);
  return exit_code;
}

static uint64_t monotonic_ns(void) {
  struct timespec now;
  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 || now.tv_sec < 0 ||
      (uint64_t)now.tv_sec > UINT64_MAX / UINT64_C(1000000000)) return 0u;
  return (uint64_t)now.tv_sec * UINT64_C(1000000000) + (uint64_t)now.tv_nsec;
}

/* Match the existing CPU/RSP native ten-second limit inside the host's
 * twelve-second bound. The guest worker calls only the canonical core. */
static int supervised_run_local(int argc, char **argv) {
  const uint64_t start = monotonic_ns();
  uint64_t wall_limit = UINT64_C(10000000000);
  const struct timespec interval = {0, 10000000};
  pid_t child;
  int status = 0;
  bool expired = false;
#ifdef T64_BASE_DIAGNOSTIC_TESTING
  const bool deadline_control = argc == 2 &&
      strcmp(argv[1], "--control-deadline") == 0;
  if (deadline_control) wall_limit = UINT64_C(50000000);
#endif
  if (start == 0u || fflush(NULL) != 0) return 4;
  child = fork();
  if (child < 0) return 4;
  if (child == 0) {
#ifdef T64_BASE_DIAGNOSTIC_TESTING
    if (deadline_control) {
      const struct timespec delay = {0, 200000000};
      (void)nanosleep(&delay, NULL);
      _exit(0);
    }
#endif
    const int result = command_run_local(argc, argv);
    (void)fflush(NULL);
    _exit(result);
  }
  for (;;) {
    uint64_t now = monotonic_ns();
    pid_t observed;
    if (now == 0u || now < start ||
        now - start >= wall_limit) {
      expired = true;
      break;
    }
    observed = waitpid(child, &status, WNOHANG);
    if (observed == child) {
      (void)puts("native_worker_reaped=yes");
      now = monotonic_ns();
      /* A deadline remains authoritative if exit races its observation. */
      if (now == 0u || now < start ||
          now - start >= wall_limit) {
        (void)puts("classification=timeout status=native-wall-deadline");
        return 124;
      }
      return WIFEXITED(status) ? WEXITSTATUS(status) : 4;
    }
    if (observed < 0 && errno != EINTR) break;
    (void)nanosleep(&interval, NULL);
  }
  if (kill(child, SIGKILL) != 0 && errno != ESRCH) {
    print_refusal("native-worker-cleanup-unverified");
    return 4;
  }
  while (waitpid(child, &status, 0) < 0) {
    if (errno != EINTR) {
      print_refusal("native-worker-reap-unverified");
      return 4;
    }
  }
  if (expired) {
    (void)puts("native_worker_reaped=yes");
    (void)puts("classification=timeout status=native-wall-deadline");
    return 124;
  }
  return 4;
}

int main(int argc, char **argv) {
#ifdef T64_BASE_DIAGNOSTIC_TESTING
  if (argc == 2 && strcmp(argv[1], "--control-deadline") == 0)
    return supervised_run_local(argc, argv);
#endif
  if (argc > 1 && strcmp(argv[1], "--identity") == 0 && argc == 2) {
    (void)puts("classification=BASE-member-adapter");
    return 0;
  }
  if (argc == 2 && strcmp(argv[1], "--runtime-identity-json") == 0) {
    const t64_profile_descriptor *profile = NULL;
    uint8_t profile_hash[T64_DIGEST_SIZE];
    char profile_hex[T64_SHA256_HEX_SIZE];
    const char *build = t64_build_identity();
    const char *revision = t64_source_revision();
    if (build == NULL || revision == NULL ||
        t64_profile_lookup("ntsc-cart-4m-openipl3-rdram-b-t1-v1", &profile) !=
            T64_STATUS_OK || profile == NULL ||
        t64_profile_hash(profile, profile_hash) != T64_STATUS_OK) return 4;
    t64_sha256_hex(profile_hash, profile_hex);
    (void)printf("{\"core_build_identity\":\"%s\",\"profile_id\":\"%s\",\"profile_sha256\":\"%s\",\"source_revision\":\"%s\"}\n",
                 build, profile->profile_id, profile_hex, revision);
    return 0;
  }
  if (argc > 1 && strcmp(argv[1], "--inventory") == 0)
    return command_inventory(argc, argv);
  if (argc > 1 && strcmp(argv[1], "--aggregate") == 0)
    return command_aggregate(argc, argv);
  if (argc > 1 && strcmp(argv[1], "--classify-manifest") == 0)
    return command_classify(argc, argv);
  if (argc > 1 && (strcmp(argv[1], "--run-local") == 0 ||
                  strcmp(argv[1], "--run-bound-local") == 0) && argc >= 3 &&
      strcmp(argv[2], "--local-only") == 0) {
    return supervised_run_local(argc, argv);
  }
  (void)fprintf(stderr,
      "usage: t64-base-diagnostic --identity | --runtime-identity-json | "
      "--inventory SOURCE SHA INVENTORY | --aggregate SOURCE SHA INVENTORY RECORD ORIGINAL | "
      "--run-local --local-only SOURCE SHA INVENTORY ROM SIZE ROM_SHA TICKS REG VALUE RECORD CAPTURE\n");
  return 64;
}
