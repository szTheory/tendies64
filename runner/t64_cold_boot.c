#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "t64_internal.h"
#include "t64_sha256.h"
#include "tendies64/t64_machine.h"

#define COLD_HELLO_BYTES 4280u
#define COLD_ROM_LIMIT (1u << 20)
#define COLD_TICK_LIMIT UINT64_C(600000)
#define COLD_TRACE_STEP UINT64_C(16)
#define COLD_CAPTURE_CAPACITY 4096u
#define COLD_RI_BASE UINT32_C(0x04700000)
#define COLD_RI_LIMIT UINT32_C(0x04800000)
#define COLD_ABSENT_RDRAM_WORD UINT32_C(0x00400000)
#define COLD_PROFILE_A "ntsc-cart-4m-openipl3-rdram-a-t1-v1"
#define COLD_PROFILE_B "ntsc-cart-4m-openipl3-rdram-b-t1-v1"
#define COLD_HELLO_SHA256 \
  "fa276cc175a4e764b6fd5413193c44b51f38fd8135e33e400032827e02f27c7c"
#define COLD_IPL3_PAYLOAD_SHA256 \
  "f716a90b1d36ec4253d096790791256d3543455e54473a755ff8c3d1199b15d5"

typedef struct {
  uint8_t bytes[COLD_HELLO_BYTES];
  size_t length;
  char sha256[T64_SHA256_HEX_SIZE];
} cold_rom;

typedef struct {
  bool found;
  bool pending;
  bool write;
  uint32_t address;
  uint32_t width;
  uint32_t value;
  uint32_t pc;
  uint32_t opcode;
  uint64_t tick;
} cold_operation;

typedef struct {
  uint8_t bytes[COLD_CAPTURE_CAPACITY];
  size_t length;
  uint64_t attempted;
  bool exact;
  bool lost;
} cold_output;

static bool allocate_machine(void *context, size_t size, size_t alignment,
                             t64_host_storage *storage_out) {
  void *data = NULL;
  size_t actual_alignment = alignment < sizeof(void *) ? sizeof(void *) : alignment;
  (void)context;
  if (storage_out == NULL || posix_memalign(&data, actual_alignment, size) != 0) {
    return false;
  }
  storage_out->data = data;
  storage_out->capacity = size;
  storage_out->alignment = actual_alignment;
  return true;
}

static void release_machine(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static void digest_hex(const void *bytes, size_t length,
                       char output[T64_SHA256_HEX_SIZE]) {
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  t64_sha256_ctx context;
  t64_sha256_init(&context);
  t64_sha256_update(&context, bytes, length);
  t64_sha256_final(&context, digest);
  t64_sha256_hex(digest, output);
}

static const char *read_cleared_hello(const char *path, cold_rom *rom) {
  struct stat metadata;
  size_t offset = 0u;
  int fd;
  if (path == NULL || rom == NULL) return "fixture-argument";
  fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
  if (fd < 0) return "fixture-open";
  if (fstat(fd, &metadata) != 0 || !S_ISREG(metadata.st_mode) ||
      metadata.st_size != (off_t)COLD_HELLO_BYTES ||
      (uint64_t)metadata.st_size > COLD_ROM_LIMIT) {
    (void)close(fd);
    return "fixture-metadata";
  }
  while (offset < sizeof rom->bytes) {
    ssize_t count = read(fd, rom->bytes + offset, sizeof rom->bytes - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) {
      (void)close(fd);
      return "fixture-read";
    }
    offset += (size_t)count;
  }
  if (close(fd) != 0) return "fixture-close";
  rom->length = offset;
  digest_hex(rom->bytes, rom->length, rom->sha256);
  if (strcmp(rom->sha256, COLD_HELLO_SHA256) != 0) {
    return "fixture-identity";
  }
  {
    char payload_digest[T64_SHA256_HEX_SIZE];
    digest_hex(rom->bytes + 0x40u, 4032u, payload_digest);
    if (strcmp(payload_digest, COLD_IPL3_PAYLOAD_SHA256) != 0) {
      return "fixture-ipl3-identity";
    }
  }
  return NULL;
}

static bool known_profile(const char *profile_id) {
  return profile_id != NULL &&
         (strcmp(profile_id, COLD_PROFILE_A) == 0 ||
          strcmp(profile_id, COLD_PROFILE_B) == 0);
}

static bool is_target_operation(uint32_t address) {
  return (address >= COLD_RI_BASE && address < COLD_RI_LIMIT) ||
         address == COLD_ABSENT_RDRAM_WORD;
}

static bool collect_trace(t64_machine *machine, cold_operation *operation,
                          uint64_t *dropped_total) {
  t64_trace_record records[T64_TRACE_RECORD_CAPACITY];
  size_t count = 0u;
  uint64_t dropped = 0u;
  bool saturated = false;
  size_t index;
  if (t64_machine_trace_drain(machine, records,
                              T64_TRACE_RECORD_CAPACITY, &count, &dropped,
                              &saturated) != T64_STATUS_OK || saturated ||
      dropped != 0u || count > T64_TRACE_RECORD_CAPACITY) {
    return false;
  }
  if (dropped > *dropped_total) *dropped_total = dropped;
  for (index = 0u; index < count; index++) {
    const t64_trace_record *record = &records[index];
    if (record->device == T64_TRACE_DEVICE_BUS &&
        (record->kind == T64_TRACE_KIND_BUS_READ8 ||
         record->kind == T64_TRACE_KIND_BUS_WRITE8 ||
         record->kind == T64_TRACE_KIND_BUS_READ16 ||
         record->kind == T64_TRACE_KIND_BUS_WRITE16 ||
         record->kind == T64_TRACE_KIND_BUS_READ32 ||
         record->kind == T64_TRACE_KIND_BUS_WRITE32)) {
      uint32_t address = (uint32_t)(record->value >> 32);
      if (!operation->found && is_target_operation(address)) {
        operation->pending = true;
        operation->address = address;
        operation->value = (uint32_t)record->value;
        operation->width = (record->kind == T64_TRACE_KIND_BUS_READ8 ||
                            record->kind == T64_TRACE_KIND_BUS_WRITE8)
                               ? 1u
                               : (record->kind == T64_TRACE_KIND_BUS_READ16 ||
                                  record->kind == T64_TRACE_KIND_BUS_WRITE16)
                                     ? 2u : 4u;
        operation->write = record->kind == T64_TRACE_KIND_BUS_WRITE8 ||
                           record->kind == T64_TRACE_KIND_BUS_WRITE16 ||
                           record->kind == T64_TRACE_KIND_BUS_WRITE32;
        operation->tick = record->tick;
      }
    } else if (record->device == T64_TRACE_DEVICE_MACHINE &&
               record->kind == T64_TRACE_KIND_CPU_RETIREMENT &&
               operation->pending && !operation->found) {
      operation->pc = (uint32_t)(record->value >> 32);
      operation->opcode = (uint32_t)record->value;
      operation->found = true;
      operation->pending = false;
    }
  }
  return true;
}

static bool drain_output(t64_machine *machine, cold_output *output) {
  t64_output_drain_result result = {0};
  size_t capacity = sizeof output->bytes - output->length;
  if (t64_machine_output_drain(machine, output->bytes + output->length,
                               capacity, &result) != T64_STATUS_OK ||
      result.copied_bytes > capacity) {
    return false;
  }
  output->length += result.copied_bytes;
  output->attempted = result.total_attempted_bytes;
  output->exact = result.total_attempted_bytes_exact;
  output->lost = result.lost_output;
  return output->exact && !output->lost;
}

static const char *stop_name(uint32_t reason) {
  switch (reason) {
  case T64_STOP_REASON_BOUND: return "guest-tick-bound";
  case T64_STOP_REASON_CPU_UNSUPPORTED: return "cpu-unsupported";
  case T64_STOP_REASON_GUEST_COMPLETION: return "software-stop";
  case T64_STOP_REASON_HALT: return "halt";
  case T64_STOP_REASON_QUIESCENT: return "quiescent";
  case T64_STOP_REASON_RESOURCE_LIMIT: return "resource-limit";
  default: return "other";
  }
}

static int run_probe(const char *profile_id, const char *fixture_path) {
  static const t64_host_allocator allocator = {
      allocate_machine, release_machine, NULL};
  cold_rom rom = {{0}, 0u, {0}};
  cold_operation operation = {0};
  cold_output output = {{0}, 0u, 0u, true, false};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  t64_stop_result stop = {T64_STOP_REASON_BOUND, 0u};
  uint64_t current_tick = 0u;
  uint64_t dropped_total = 0u;
  uint8_t content_digest[T64_DIGEST_SIZE];
  char content_hex[T64_SHA256_HEX_SIZE];
  uint8_t output_hash[T64_SHA256_DIGEST_SIZE];
  char output_hex[T64_SHA256_HEX_SIZE];
  t64_sha256_ctx output_context;
  const char *error;
  int result = 2;

  error = read_cleared_hello(fixture_path, &rom);
  if (error != NULL) {
    (void)printf("classification=refused reason=%s\n", error);
    return 2;
  }
  if (!known_profile(profile_id) ||
      t64_profile_lookup(profile_id, &profile) != T64_STATUS_OK ||
      profile == NULL || strcmp(profile->profile_id, profile_id) != 0 ||
      profile->rdram_bytes != UINT32_C(0x00400000) ||
      profile->expansion_bytes != 0u) {
    (void)puts("classification=refused reason=profile-identity");
    return 2;
  }
  if (t64_machine_create(&allocator, profile, &machine) != T64_STATUS_OK ||
      machine == NULL) {
    (void)puts("classification=refused reason=machine-create");
    goto cleanup;
  }
  if (t64_machine_trace_set_enabled(machine, true) != T64_STATUS_OK ||
      t64_machine_admit_rom(machine, rom.bytes, rom.length) != T64_STATUS_OK ||
      machine->cpu.pc != UINT32_C(0xa4000040) ||
      t64_machine_content_digest(machine, content_digest) != T64_STATUS_OK) {
    (void)puts("classification=refused reason=boot-entry");
    goto cleanup;
  }
  t64_sha256_hex(content_digest, content_hex);
  if (strcmp(content_hex, rom.sha256) != 0) {
    (void)puts("classification=harness-error reason=admitted-content-identity");
    goto cleanup;
  }
  if (!collect_trace(machine, &operation, &dropped_total) ||
      !drain_output(machine, &output)) {
    (void)puts("classification=harness-error reason=initial-observation");
    goto cleanup;
  }
  while (current_tick < COLD_TICK_LIMIT &&
         stop.reason == T64_STOP_REASON_BOUND) {
    uint64_t remaining = COLD_TICK_LIMIT - current_tick;
    uint64_t target = current_tick +
        (remaining < COLD_TRACE_STEP ? remaining : COLD_TRACE_STEP);
    t64_status status = t64_machine_advance(
        machine, &(t64_stop_request){T64_STOP_GUEST_TICK, target}, &stop);
    current_tick = stop.tick;
    if (status != T64_STATUS_OK) {
      if (machine->cpu.stopped &&
          machine->cpu.stop_reason == T64_STOP_REASON_CPU_UNSUPPORTED) {
        stop.reason = T64_STOP_REASON_CPU_UNSUPPORTED;
      } else {
        (void)printf("classification=harness-error reason=advance-status status=%u tick=%llu\n",
                     (unsigned int)status,
                     (unsigned long long)current_tick);
        goto cleanup;
      }
    }
    if (machine->cpu.stopped) {
      stop.reason = machine->cpu.stop_reason;
      current_tick = machine->tick;
    }
    if (!collect_trace(machine, &operation, &dropped_total) ||
        !drain_output(machine, &output)) {
      (void)puts("classification=harness-error reason=observation-overflow");
      goto cleanup;
    }
    if (stop.reason != T64_STOP_REASON_BOUND &&
        stop.reason != T64_STOP_REASON_CPU_UNSUPPORTED &&
        stop.reason != T64_STOP_REASON_GUEST_COMPLETION &&
        stop.reason != T64_STOP_REASON_HALT &&
        stop.reason != T64_STOP_REASON_QUIESCENT &&
        stop.reason != T64_STOP_REASON_RESOURCE_LIMIT) {
      (void)printf("classification=harness-error reason=unexpected-stop stop=%u\n",
                   stop.reason);
      goto cleanup;
    }
  }
  t64_sha256_init(&output_context);
  t64_sha256_update(&output_context, output.bytes, output.length);
  t64_sha256_final(&output_context, output_hash);
  t64_sha256_hex(output_hash, output_hex);
  if (!operation.found) {
    (void)printf("classification=incomplete reason=missing-ri-rdram-operation profile=%s stop=%s tick=%llu output_bytes=%zu output_sha256=%s\n",
                 profile_id, stop_name(stop.reason),
                 (unsigned long long)current_tick, output.length, output_hex);
    result = 1;
    goto cleanup;
  }
  (void)printf("classification=observed profile=%s operation=%s address=0x%08x width=%u value=0x%08x pc=0x%08x opcode=0x%08x tick=%llu stop=%s stop_tick=%llu output_bytes=%zu output_sha256=%s dropped=%llu\n",
               profile_id, operation.write ? "write" : "read",
               operation.address, operation.width, operation.value,
               operation.pc, operation.opcode,
               (unsigned long long)operation.tick, stop_name(stop.reason),
               (unsigned long long)current_tick, output.length, output_hex,
               (unsigned long long)dropped_total);
  result = 0;

cleanup:
  if (machine != NULL) t64_machine_destroy(machine);
  return result;
}

int main(int argc, char **argv) {
  if (argc >= 2 && strcmp(argv[1], "--compare") == 0) {
    (void)puts("classification=refused reason=compare-not-implemented");
    return 2;
  }
  if (argc != 6 || strcmp(argv[1], "--probe") != 0 ||
      strcmp(argv[2], "--profile") != 0 ||
      strcmp(argv[4], "--fixture") != 0) {
    (void)puts("classification=refused reason=usage");
    return 2;
  }
  return run_probe(argv[3], argv[5]);
}
