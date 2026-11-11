#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "t64_input.h"
#include "t64_real_result.h"
#include "t64_sha256.h"
#include "tendies64/t64_machine.h"

#define T64_REAL_FIXTURE_SIZE 4280u
#define T64_REAL_FIXTURE_MAX_BYTES (1u << 20)
#define T64_REAL_OUTPUT_CAPACITY 4096u
#define T64_REAL_EXPECTED_LOG_BYTES 30u
#define T64_REAL_TRACE_STEP UINT64_C(16)
#define T64_REAL_MAX_RESOURCE_YIELDS 64u
#define T64_REAL_EXPECTED_PROFILE_ID "ntsc-cart-4m-openipl3-warm-v1"
#define T64_REAL_EXPECTED_FIXTURE_ID "hello-openipl3"
#define T64_REAL_EXPECTED_PROFILE_SHA256 \
  "a171241f68c377f7c324a995a08ff8bd8da60bbe70df14193d8f4b8ae7e9769f"
#define T64_REAL_EXPECTED_ASSET_SHA256 \
  "f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068"
#define T64_REAL_EXPECTED_PAYLOAD_SHA256 \
  "f716a90b1d36ec4253d096790791256d3543455e54473a755ff8c3d1199b15d5"
#define T64_REAL_EXPECTED_FIXTURE_SHA256 \
  "fa276cc175a4e764b6fd5413193c44b51f38fd8135e33e400032827e02f27c7c"
#define T64_REAL_EXPECTED_LOG_SHA256 \
  "86594505bea53c9843f9eaf07e4cf0121cbe0055670324674fd8385c88ffd18e"
#define T64_REAL_HELLO_PC UINT32_C(0x80000478)
#define T64_REAL_HELLO_ENTRY_PC UINT32_C(0x80000400)
#define T64_REAL_BREAK_OPCODE UINT32_C(0x0000000d)

static const uint8_t expected_log[T64_REAL_EXPECTED_LOG_BYTES + 1u] =
    "Tendies64 hello via open IPL3\n";

typedef struct {
  uint8_t *bytes;
  size_t length;
  uint8_t sha256[T64_SHA256_DIGEST_SIZE];
  char sha256_hex[T64_SHA256_HEX_SIZE];
} owned_bytes;

typedef struct {
  t64_real_result *result;
  uint64_t retired;
  uint32_t last_cpu_pc;
  bool have_last_cpu_pc;
  bool saw_ipl3_entry;
  bool saw_loader_dma;
  bool saw_hello_entry;
  bool saw_completion;
  uint64_t trace_records;
  uint64_t completion_tick;
  uint32_t completion_pc;
  uint32_t completion_opcode;
} trace_observation;

static bool parse_u64(const char *text, uint64_t *value_out) {
  uint64_t value = 0u;
  size_t i;

  if (text == NULL || text[0] == '\0' ||
      (text[0] == '0' && text[1] != '\0')) {
    return false;
  }
  for (i = 0u; text[i] != '\0'; i++) {
    unsigned digit;
    if (text[i] < '0' || text[i] > '9') {
      return false;
    }
    digit = (unsigned)(text[i] - '0');
    if (value > (UINT64_MAX - digit) / UINT64_C(10)) {
      return false;
    }
    value = value * UINT64_C(10) + digit;
  }
  *value_out = value;
  return true;
}

static bool parse_fd(const char *text, int *fd_out) {
  uint64_t value;
  if (!parse_u64(text, &value) || value > (uint64_t)INT_MAX ||
      value <= (uint64_t)STDERR_FILENO) {
    return false;
  }
  *fd_out = (int)value;
  return true;
}

static bool read_snapshot(int fd, size_t max_bytes, size_t exact_size,
                          owned_bytes *output) {
  struct stat st;
  int file_flags;
  off_t offset;
  size_t total = 0u;
  t64_sha256_ctx hash;
  uint8_t extra;

  if (fd < 0 || output == NULL || fstat(fd, &st) != 0 ||
      !S_ISREG(st.st_mode) || st.st_size < 0 ||
      (uint64_t)st.st_size > (uint64_t)max_bytes ||
      (exact_size != 0u && (size_t)st.st_size != exact_size) ||
      st.st_nlink != 0) {
    return false;
  }
  file_flags = fcntl(fd, F_GETFL);
  offset = lseek(fd, 0, SEEK_CUR);
  if (file_flags < 0 || (file_flags & O_ACCMODE) != O_RDONLY || offset != 0) {
    return false;
  }
  output->length = (size_t)st.st_size;
  if (output->length == 0u) {
    return false;
  }
  output->bytes = (uint8_t *)malloc(output->length);
  if (output->bytes == NULL) {
    return false;
  }
  t64_sha256_init(&hash);
  while (total < output->length) {
    ssize_t count = read(fd, output->bytes + total, output->length - total);
    if (count < 0 && errno == EINTR) {
      continue;
    }
    if (count <= 0) {
      free(output->bytes);
      output->bytes = NULL;
      return false;
    }
    t64_sha256_update(&hash, output->bytes + total, (size_t)count);
    total += (size_t)count;
  }
  for (;;) {
    ssize_t count = read(fd, &extra, 1u);
    if (count < 0 && errno == EINTR) {
      continue;
    }
    if (count != 0) {
      free(output->bytes);
      output->bytes = NULL;
      return false;
    }
    break;
  }
  t64_sha256_final(&hash, output->sha256);
  t64_sha256_hex(output->sha256, output->sha256_hex);
  return true;
}

static bool digest_hex(const uint8_t *bytes, size_t length,
                       char output[T64_SHA256_HEX_SIZE]) {
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  t64_sha256_ctx hash;
  if (bytes == NULL || output == NULL) {
    return false;
  }
  t64_sha256_init(&hash);
  t64_sha256_update(&hash, bytes, length);
  t64_sha256_final(&hash, digest);
  t64_sha256_hex(digest, output);
  return true;
}

static bool bytes_equal_hex(const uint8_t *bytes, size_t length,
                            const char *expected) {
  char actual[T64_SHA256_HEX_SIZE];
  return digest_hex(bytes, length, actual) &&
         strcmp(actual, expected) == 0;
}

static bool allocator_allocate(void *context, size_t size, size_t alignment,
                               t64_host_storage *storage_out) {
  void *data = NULL;
  size_t host_alignment = alignment;
  (void)context;
  if (alignment < sizeof(void *)) {
    host_alignment = sizeof(void *);
  }
  if (storage_out == NULL || size == 0u || alignment == 0u ||
      posix_memalign(&data, host_alignment, size) != 0) {
    return false;
  }
  storage_out->data = data;
  storage_out->capacity = size;
  storage_out->alignment = host_alignment;
  return true;
}

static void allocator_release(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static void format_pc(uint32_t pc, char output[17]) {
  uint64_t address = (uint64_t)(int64_t)(int32_t)pc;
  (void)snprintf(output, 17u, "%016" PRIx64, address);
}

static bool add_milestone(t64_real_result *result, const char *kind,
                          uint64_t tick, uint32_t pc) {
  t64_real_milestone *milestone;
  size_t length;
  if (result->milestone_count >= T64_REAL_RESULT_MAX_MILESTONES) {
    return false;
  }
  milestone = &result->milestones[result->milestone_count];
  length = strlen(kind);
  if (length >= sizeof milestone->kind) {
    return false;
  }
  (void)memcpy(milestone->kind, kind, length + 1u);
  milestone->guest_tick = tick;
  format_pc(pc, milestone->pc);
  result->milestone_count++;
  return true;
}

static bool observe_trace_record(trace_observation *observation,
                                 const t64_trace_record *record) {
  uint32_t pc = (uint32_t)(record->value >> 32);
  uint32_t opcode = (uint32_t)record->value;

  observation->trace_records++;

  if (record->device == T64_TRACE_DEVICE_MACHINE &&
      record->kind == T64_TRACE_KIND_CPU_ENTRY &&
      (uint32_t)record->value == UINT32_C(0xa4000040) &&
      !observation->saw_ipl3_entry) {
    if (!add_milestone(observation->result, "ipl3-entry", record->tick,
                       UINT32_C(0xa4000040))) {
      return false;
    }
    observation->saw_ipl3_entry = true;
  }
  if (record->device == T64_TRACE_DEVICE_MACHINE &&
      record->kind == T64_TRACE_KIND_CPU_RETIREMENT) {
    observation->retired++;
    observation->last_cpu_pc = pc;
    observation->have_last_cpu_pc = true;
    if (pc == UINT32_C(0xa4000040) && !observation->saw_ipl3_entry) {
      if (!add_milestone(observation->result, "ipl3-entry", UINT64_C(0),
                         pc)) {
        return false;
      }
      observation->saw_ipl3_entry = true;
    }
    if (pc == T64_REAL_HELLO_ENTRY_PC && !observation->saw_hello_entry) {
      if (!add_milestone(observation->result, "hello-entry", record->tick,
                         pc)) {
        return false;
      }
      observation->saw_hello_entry = true;
    }
  }
  if (record->device == T64_TRACE_DEVICE_SP &&
      record->kind == T64_TRACE_KIND_SP_COMPLETE &&
      !observation->saw_loader_dma && observation->saw_ipl3_entry &&
      observation->have_last_cpu_pc) {
    if (!add_milestone(observation->result, "loader-dma", record->tick,
                       observation->last_cpu_pc)) {
      return false;
    }
    observation->saw_loader_dma = true;
  }
  if (record->device == T64_TRACE_DEVICE_PI &&
      record->kind == T64_TRACE_KIND_PI_COMPLETE &&
      !observation->saw_loader_dma && observation->saw_ipl3_entry &&
      observation->have_last_cpu_pc) {
    if (!add_milestone(observation->result, "loader-dma", record->tick,
                       observation->last_cpu_pc)) {
      return false;
    }
    observation->saw_loader_dma = true;
  }
  if (record->device == T64_TRACE_DEVICE_MACHINE &&
      record->kind == T64_TRACE_KIND_CPU_COMPLETION) {
    observation->saw_completion = true;
    observation->completion_tick = record->tick;
    observation->completion_pc = pc;
    observation->completion_opcode = opcode;
  }
  return true;
}

static bool drain_trace(t64_machine *machine,
                        trace_observation *observation) {
  t64_trace_record records[T64_TRACE_RECORD_CAPACITY];
  size_t count = 0u;
  uint64_t dropped = 0u;
  bool saturated = false;
  size_t i;

  if (t64_machine_trace_drain(machine, records,
                              sizeof records / sizeof records[0], &count,
                              &dropped, &saturated) != T64_STATUS_OK ||
      dropped != 0u || saturated) {
    return false;
  }
  for (i = 0u; i < count; i++) {
    if (!observe_trace_record(observation, &records[i])) {
      return false;
    }
  }
  return true;
}

static bool drain_output(t64_machine *machine, uint8_t output[4096],
                         size_t *length_io, uint64_t *total_out,
                         bool *lost_out) {
  t64_output_drain_result result;
  size_t capacity;
  t64_status status;

  if (length_io == NULL || total_out == NULL || lost_out == NULL ||
      *length_io > T64_REAL_OUTPUT_CAPACITY) {
    return false;
  }
  capacity = *length_io < T64_REAL_OUTPUT_CAPACITY
                 ? T64_REAL_OUTPUT_CAPACITY - *length_io
                 : 0u;
  status = t64_machine_output_drain(machine, output + *length_io, capacity,
                                    &result);
  if (status != T64_STATUS_OK || result.copied_bytes > capacity ||
      result.copied_bytes > SIZE_MAX - *length_io) {
    return false;
  }
  *length_io += result.copied_bytes;
  *total_out = result.total_produced_bytes;
  *lost_out = result.lost_output;
  return true;
}

static bool write_all(int fd, const uint8_t *bytes, size_t length) {
  size_t offset = 0u;
  while (offset < length) {
    ssize_t count = write(fd, bytes + offset, length - offset);
    if (count < 0 && errno == EINTR) {
      continue;
    }
    if (count <= 0) {
      return false;
    }
    offset += (size_t)count;
  }
  return true;
}

static void set_prelaunch_result(t64_real_result *result, const char *detail,
                                 const char *stop_kind) {
  (void)memset(result, 0, sizeof *result);
  result->prelaunch = true;
  result->guest_stop_kind[0] = '\0';
  (void)snprintf(result->guest_stop_kind, sizeof result->guest_stop_kind,
                 "%s", stop_kind);
  (void)snprintf(result->outcome, sizeof result->outcome, "harness-error");
  (void)snprintf(result->detail, sizeof result->detail, "%s", detail);
}

static int emit_prelaunch(const char *detail, const char *stop_kind,
                          uint64_t bound_tick, int exit_code) {
  t64_real_result result;
  set_prelaunch_result(&result, detail, stop_kind);
  result.bound_tick = bound_tick;
  if (!t64_real_result_write_fd(&result, STDOUT_FILENO)) {
    return 2;
  }
  return exit_code;
}

static bool valid_build_identity(const char *build_id) {
  const char prefix[] = "t64-build-v1-";
  size_t i;
  if (build_id == NULL || strlen(build_id) != sizeof prefix - 1u + 64u ||
      memcmp(build_id, prefix, sizeof prefix - 1u) != 0) {
    return false;
  }
  for (i = sizeof prefix - 1u; build_id[i] != '\0'; i++) {
    if (!((build_id[i] >= '0' && build_id[i] <= '9') ||
          (build_id[i] >= 'a' && build_id[i] <= 'f'))) {
      return false;
    }
  }
  return true;
}

static bool valid_source_revision(const char *revision) {
  size_t i;
  if (revision == NULL || strlen(revision) != 40u) {
    return false;
  }
  for (i = 0u; revision[i] != '\0'; i++) {
    if (!((revision[i] >= '0' && revision[i] <= '9') ||
          (revision[i] >= 'a' && revision[i] <= 'f'))) {
      return false;
    }
  }
  return true;
}

static bool initialize_launched_result(t64_real_result *result,
                                       const t64_profile_descriptor *profile,
                                       const char *fixture_sha,
                                       const char *input_sha) {
  uint8_t profile_digest[T64_DIGEST_SIZE];
  char profile_hex[T64_SHA256_HEX_SIZE];
  const char *build_id = t64_build_identity();
  const char *source_revision = t64_source_revision();
  char asset_hex[T64_SHA256_HEX_SIZE];
  char payload_hex[T64_SHA256_HEX_SIZE];

  if (profile == NULL || !valid_build_identity(build_id) ||
      !valid_source_revision(source_revision) ||
      t64_profile_hash(profile, profile_digest) != T64_STATUS_OK) {
    return false;
  }
  t64_sha256_hex(profile_digest, profile_hex);
  t64_sha256_hex(profile->asset_sha256, asset_hex);
  t64_sha256_hex(profile->payload_sha256, payload_hex);
  if (strcmp(profile_hex, T64_REAL_EXPECTED_PROFILE_SHA256) != 0 ||
      strcmp(asset_hex, T64_REAL_EXPECTED_ASSET_SHA256) != 0 ||
      strcmp(payload_hex, T64_REAL_EXPECTED_PAYLOAD_SHA256) != 0) {
    return false;
  }

  (void)memset(result, 0, sizeof *result);
  (void)snprintf(result->build_id, sizeof result->build_id, "%s", build_id);
  (void)snprintf(result->source_revision, sizeof result->source_revision, "%s",
                 source_revision);
  (void)snprintf(result->profile_id, sizeof result->profile_id, "%s",
                 profile->profile_id);
  (void)snprintf(result->profile_digest, sizeof result->profile_digest, "%s",
                 profile_hex);
  (void)snprintf(result->fixture_id, sizeof result->fixture_id, "%s",
                 T64_REAL_EXPECTED_FIXTURE_ID);
  (void)snprintf(result->fixture_sha256, sizeof result->fixture_sha256, "%s",
                 fixture_sha);
  (void)snprintf(result->ipl3_sha256, sizeof result->ipl3_sha256, "%s",
                 asset_hex);
  (void)snprintf(result->ipl3_payload_sha256,
                 sizeof result->ipl3_payload_sha256, "%s", payload_hex);
  (void)snprintf(result->input_digest, sizeof result->input_digest, "%s",
                 input_sha);
  result->has_input_digest = true;
  result->has_log_sha256 = false;
  result->has_state_digest = false;
  result->bound_tick = 0u;
  (void)snprintf(result->guest_stop_kind, sizeof result->guest_stop_kind,
                 "host-error");
  (void)snprintf(result->outcome, sizeof result->outcome, "harness-error");
  (void)snprintf(result->detail, sizeof result->detail, "host-error");
  return true;
}

static void set_digest(char output[T64_REAL_RESULT_HEX_SIZE],
                       const uint8_t digest[T64_DIGEST_SIZE]) {
  t64_sha256_hex(digest, output);
}

static int run_real(int fixture_fd, int input_fd, uint64_t guest_ticks) {
  static const t64_host_allocator allocator = {
      allocator_allocate, allocator_release, NULL};
  owned_bytes fixture = {0};
  owned_bytes input = {0};
  t64_input_schedule *schedule = NULL;
  t64_input_error input_error;
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  t64_real_result result;
  trace_observation observations = {0};
  uint8_t output[T64_REAL_OUTPUT_CAPACITY] = {0};
  size_t output_length = 0u;
  uint64_t output_total = 0u;
  bool output_lost = false;
  uint64_t current_tick = 0u;
  uint64_t previous_yield_records = 0u;
  uint64_t resource_yields = 0u;
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail stop_detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};
  t64_status status;
  int exit_code = 0;
  char input_digest[T64_SHA256_HEX_SIZE];
  uint8_t state_digest[T64_DIGEST_SIZE];
  char output_digest[T64_SHA256_HEX_SIZE];
  bool qualified_completion;

  if (!read_snapshot(fixture_fd, T64_REAL_FIXTURE_MAX_BYTES,
                     T64_REAL_FIXTURE_SIZE, &fixture)) {
    exit_code = emit_prelaunch("refusal", "refused", guest_ticks, 0);
    goto cleanup;
  }
  if (strcmp(fixture.sha256_hex, T64_REAL_EXPECTED_FIXTURE_SHA256) != 0) {
    exit_code = emit_prelaunch("refusal", "refused", guest_ticks, 0);
    goto cleanup;
  }
  if (!read_snapshot(input_fd, T64_INPUT_MAX_BYTES, 0u, &input)) {
    exit_code = emit_prelaunch("refusal", "refused", guest_ticks, 0);
    goto cleanup;
  }
  if (t64_input_parse(input.bytes, input.length, &schedule, &input_error) !=
      T64_INPUT_OK) {
    exit_code = emit_prelaunch("refusal", "refused", guest_ticks, 0);
    goto cleanup;
  }
  {
    size_t canonical_length = 0u;
    const uint8_t *canonical =
        t64_input_schedule_canonical_json(schedule, &canonical_length);
    if (canonical == NULL ||
        !digest_hex(canonical, canonical_length, input_digest)) {
      exit_code = emit_prelaunch("refusal", "refused", guest_ticks, 0);
      goto cleanup;
    }
  }
  if (t64_profile_lookup(T64_REAL_EXPECTED_PROFILE_ID, &profile) !=
          T64_STATUS_OK ||
      profile == NULL ||
      !bytes_equal_hex(fixture.bytes + 0x40u, 4032u,
                       T64_REAL_EXPECTED_PAYLOAD_SHA256)) {
    exit_code = emit_prelaunch("refusal", "refused", guest_ticks, 0);
    goto cleanup;
  }
  {
    size_t i;
    t64_input_event event;
    for (i = 0u; i < t64_input_schedule_event_count(schedule); i++) {
      if (!t64_input_schedule_event(schedule, i, &event) ||
          event.tick > guest_ticks) {
        exit_code = emit_prelaunch("refusal", "refused", guest_ticks, 0);
        goto cleanup;
      }
    }
  }
  if (!initialize_launched_result(&result, profile, fixture.sha256_hex,
                                  input_digest)) {
    exit_code = emit_prelaunch("refusal", "refused", guest_ticks, 0);
    goto cleanup;
  }
  result.bound_tick = guest_ticks;
  status = t64_machine_create(&allocator, profile, &machine);
  if (status != T64_STATUS_OK || machine == NULL) {
    exit_code = emit_prelaunch("refusal", "refused", guest_ticks, 0);
    goto cleanup;
  }
  status = t64_machine_trace_set_enabled(machine, true);
  if (status != T64_STATUS_OK) {
    exit_code = emit_prelaunch("refusal", "refused", guest_ticks, 0);
    goto cleanup;
  }
  status = t64_machine_admit_rom(machine, fixture.bytes, fixture.length);
  if (status != T64_STATUS_OK) {
    exit_code = emit_prelaunch("refusal", "refused", guest_ticks, 0);
    goto cleanup;
  }
  observations.result = &result;
  if (!drain_trace(machine, &observations)) {
    (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                   "host-error");
    (void)snprintf(result.outcome, sizeof result.outcome, "harness-error");
    (void)snprintf(result.detail, sizeof result.detail, "host-error");
    goto finalize;
  }
  {
    size_t i;
    t64_input_event event;
    for (i = 0u; i < t64_input_schedule_event_count(schedule); i++) {
      if (!t64_input_schedule_event(schedule, i, &event) ||
          t64_machine_queue_input(machine, event.tick, event.port,
                                  event.buttons, event.axis_x,
                                  event.axis_y) != T64_STATUS_OK) {
        (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                       "host-error");
        (void)snprintf(result.outcome, sizeof result.outcome,
                       "harness-error");
        (void)snprintf(result.detail, sizeof result.detail, "host-error");
        goto finalize;
      }
    }
  }

  while (current_tick < guest_ticks) {
    uint64_t target =
        guest_ticks - current_tick > T64_REAL_TRACE_STEP
            ? current_tick + T64_REAL_TRACE_STEP
            : guest_ticks;
    for (;;) {
      uint64_t before_records = observations.trace_records;
      status = t64_machine_advance(
          machine,
          &(t64_stop_request){T64_STOP_GUEST_TICK, target}, &stop);
      if (status != T64_STATUS_OK || stop.tick < current_tick ||
          stop.tick > target || !drain_trace(machine, &observations) ||
          !drain_output(machine, output, &output_length, &output_total,
                        &output_lost)) {
        (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                       "host-error");
        (void)snprintf(result.outcome, sizeof result.outcome,
                       "harness-error");
        (void)snprintf(result.detail, sizeof result.detail, "host-error");
        goto finalize;
      }
      current_tick = stop.tick;
      if (output_lost) {
        (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                       "output-overflow");
        (void)snprintf(result.outcome, sizeof result.outcome,
                       "harness-error");
        (void)snprintf(result.detail, sizeof result.detail, "output-overflow");
        goto finalize;
      }
      if (stop.reason == T64_STOP_REASON_RESOURCE_LIMIT) {
        resource_yields++;
        if (resource_yields > T64_REAL_MAX_RESOURCE_YIELDS ||
            (observations.trace_records == before_records &&
             observations.trace_records == previous_yield_records)) {
          (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                         "host-error");
          (void)snprintf(result.outcome, sizeof result.outcome,
                         "harness-error");
          (void)snprintf(result.detail, sizeof result.detail, "host-error");
          goto finalize;
        }
        previous_yield_records = observations.trace_records;
        continue;
      }
      break;
    }
    if (stop.reason == T64_STOP_REASON_BOUND) {
      if (current_tick != target) {
        (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                       "host-error");
        (void)snprintf(result.outcome, sizeof result.outcome,
                       "harness-error");
        (void)snprintf(result.detail, sizeof result.detail, "host-error");
        goto finalize;
      }
      if (current_tick == guest_ticks) {
        (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                       "guest-bound");
        if (output_length == 0u) {
          (void)snprintf(result.outcome, sizeof result.outcome, "boot-fail");
          (void)snprintf(result.detail, sizeof result.detail, "empty-output");
        } else {
          (void)snprintf(result.outcome, sizeof result.outcome, "timeout");
          (void)snprintf(result.detail, sizeof result.detail, "guest-bound");
        }
        break;
      }
      continue;
    }
    if (stop.reason == T64_STOP_REASON_GUEST_COMPLETION ||
        stop.reason == T64_STOP_REASON_CPU_UNSUPPORTED) {
      if (t64_machine_cpu_stop_detail(machine, &stop_detail) !=
          T64_STATUS_OK ||
          stop_detail.retired_instructions != observations.retired) {
        (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                       "host-error");
        (void)snprintf(result.outcome, sizeof result.outcome,
                       "harness-error");
        (void)snprintf(result.detail, sizeof result.detail, "host-error");
        goto finalize;
      }
      break;
    }
    (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                   "host-error");
    (void)snprintf(result.outcome, sizeof result.outcome, "harness-error");
    (void)snprintf(result.detail, sizeof result.detail, "host-error");
    goto finalize;
  }

  if (stop.reason == T64_STOP_REASON_GUEST_COMPLETION) {
    qualified_completion =
        strcmp(fixture.sha256_hex, T64_REAL_EXPECTED_FIXTURE_SHA256) == 0 &&
        observations.saw_completion &&
        observations.completion_pc == T64_REAL_HELLO_PC &&
        observations.completion_opcode == T64_REAL_BREAK_OPCODE &&
        stop_detail.pc == T64_REAL_HELLO_PC &&
        stop_detail.opcode == T64_REAL_BREAK_OPCODE &&
        observations.saw_ipl3_entry && observations.saw_loader_dma &&
        observations.saw_hello_entry;
    if (qualified_completion &&
        !add_milestone(&result, "hello-completion",
                       observations.completion_tick,
                       observations.completion_pc)) {
      (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                     "host-error");
      (void)snprintf(result.outcome, sizeof result.outcome, "harness-error");
      (void)snprintf(result.detail, sizeof result.detail, "host-error");
      goto finalize;
    }
    if (qualified_completion) {
      (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                     "authored-completion");
      if (output_length == 0u) {
        (void)snprintf(result.outcome, sizeof result.outcome, "boot-fail");
        (void)snprintf(result.detail, sizeof result.detail, "empty-output");
      } else if (output_length == sizeof expected_log - 1u &&
                 memcmp(output, expected_log, sizeof expected_log - 1u) == 0 &&
                 bytes_equal_hex(output, output_length,
                                 T64_REAL_EXPECTED_LOG_SHA256)) {
        (void)snprintf(result.outcome, sizeof result.outcome, "pass");
        (void)snprintf(result.detail, sizeof result.detail, "positive-match");
      } else {
        (void)snprintf(result.outcome, sizeof result.outcome, "guest-fail");
        (void)snprintf(result.detail, sizeof result.detail, "wrong-transcript");
      }
    } else {
      (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                     "unsupported-execution");
      (void)snprintf(result.outcome, sizeof result.outcome, "boot-fail");
      (void)snprintf(result.detail, sizeof result.detail,
                     "unsupported-execution");
    }
  } else if (stop.reason == T64_STOP_REASON_CPU_UNSUPPORTED) {
    (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                   "unsupported-execution");
    (void)snprintf(result.outcome, sizeof result.outcome, "boot-fail");
    (void)snprintf(result.detail, sizeof result.detail,
                   "unsupported-execution");
  }

finalize:
  result.guest_tick = stop.tick <= guest_ticks ? stop.tick : current_tick;
  result.instruction_count = observations.retired;
  result.log_bytes = (uint64_t)output_length;
  result.log_bytes_total = output_total;
  if (result.log_bytes == result.log_bytes_total) {
    if (digest_hex(output, output_length, output_digest)) {
      (void)snprintf(result.log_sha256, sizeof result.log_sha256, "%s",
                     output_digest);
      result.has_log_sha256 = true;
    }
  } else if (strcmp(result.detail, "output-overflow") == 0) {
    if (digest_hex(output, output_length, output_digest)) {
      (void)snprintf(result.log_sha256, sizeof result.log_sha256, "%s",
                     output_digest);
      result.has_log_sha256 = true;
    }
  }
  if (machine != NULL &&
      t64_machine_digest(machine, state_digest) == T64_STATUS_OK) {
    set_digest(result.state_digest, state_digest);
    result.has_state_digest = true;
  }
  if (!write_all(STDERR_FILENO, output, output_length)) {
    result.has_log_sha256 = false;
    result.log_bytes = 0u;
    result.log_bytes_total = 0u;
    result.has_state_digest = false;
    (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                   "host-error");
    (void)snprintf(result.outcome, sizeof result.outcome, "harness-error");
    (void)snprintf(result.detail, sizeof result.detail, "host-error");
  }
  if (!t64_real_result_write_fd(&result, STDOUT_FILENO)) {
    exit_code = 2;
  }

cleanup:
  if (machine != NULL) {
    t64_machine_destroy(machine);
  }
  t64_input_schedule_destroy(schedule);
  free(fixture.bytes);
  free(input.bytes);
  return exit_code;
}

int main(int argc, char **argv) {
  int fixture_fd = -1;
  int input_fd = -1;
  uint64_t guest_ticks = 0u;
  bool seen_fixture = false;
  bool seen_input = false;
  bool seen_profile = false;
  bool seen_ticks = false;
  int i;

  if (argc != 9) {
    return emit_prelaunch("usage", "not-started", 0u, 2);
  }
  for (i = 1; i < argc; i += 2) {
    const char *option = argv[i];
    const char *value = argv[i + 1];
    if (strcmp(option, "--fixture-fd") == 0 && !seen_fixture &&
        parse_fd(value, &fixture_fd)) {
      seen_fixture = true;
    } else if (strcmp(option, "--input-fd") == 0 && !seen_input &&
               parse_fd(value, &input_fd)) {
      seen_input = true;
    } else if (strcmp(option, "--profile") == 0 && !seen_profile &&
               strcmp(value, T64_REAL_EXPECTED_PROFILE_ID) == 0) {
      seen_profile = true;
    } else if (strcmp(option, "--guest-ticks") == 0 && !seen_ticks &&
               parse_u64(value, &guest_ticks) && guest_ticks > 0u &&
               guest_ticks <= UINT64_C(600000)) {
      seen_ticks = true;
    } else {
      return emit_prelaunch("usage", "not-started", guest_ticks, 2);
    }
  }
  if (!seen_fixture || !seen_input || !seen_profile || !seen_ticks) {
    return emit_prelaunch("usage", "not-started", guest_ticks, 2);
  }
  return run_real(fixture_fd, input_fd, guest_ticks);
}
