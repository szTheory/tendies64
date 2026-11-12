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
#include "tendies64/t64_machine.h"

#define IPL3_BYTES 4096u
#define IMAGE_PAYLOAD_OFFSET 0x1000u
#define TEST_PAYLOAD_BYTES 40u
#define TEST_ROM_BYTES (IMAGE_PAYLOAD_OFFSET + TEST_PAYLOAD_BYTES)
#define TEST_TICK_BOUND UINT64_C(531000)
#define PARK_PC UINT32_C(0x80000420)
#define PARK_OPCODE UINT32_C(0x08000108)

static unsigned failures;

static void check(bool condition, const char *name) {
  if (!condition) {
    (void)fprintf(stderr, "FAIL %s\n", name);
    failures++;
  }
}

static void put_be32(uint8_t *out, uint32_t value) {
  out[0] = (uint8_t)(value >> 24);
  out[1] = (uint8_t)(value >> 16);
  out[2] = (uint8_t)(value >> 8);
  out[3] = (uint8_t)value;
}

static bool read_ipl3_asset(uint8_t bytes[IPL3_BYTES]) {
  const char *path = getenv("T64_TEST_IPL3_PATH");
  struct stat st;
  size_t offset = 0u;
  int fd;
  if (path == NULL || path[0] == '\0') return false;
  fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  if (fd < 0 || fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) ||
      st.st_size != (off_t)IPL3_BYTES) {
    if (fd >= 0) close(fd);
    return false;
  }
  while (offset < IPL3_BYTES) {
    ssize_t count = read(fd, bytes + offset, IPL3_BYTES - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) {
      close(fd);
      return false;
    }
    offset += (size_t)count;
  }
  close(fd);
  return true;
}

static bool allocate_machine(void *context, size_t size, size_t alignment,
                             t64_host_storage *storage_out) {
  void *data = NULL;
  size_t host_alignment = alignment;
  (void)context;
  if (host_alignment < sizeof(void *)) host_alignment = sizeof(void *);
  if (storage_out == NULL ||
      posix_memalign(&data, host_alignment, size) != 0) {
    return false;
  }
  storage_out->data = data;
  storage_out->capacity = size;
  storage_out->alignment = host_alignment;
  return true;
}

static void release_machine(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static void make_authored_rom(uint8_t image[TEST_ROM_BYTES]) {
  static const uint32_t program[TEST_PAYLOAD_BYTES / 4u] = {
      UINT32_C(0x3c08b3ff), /* lui t0,0xb3ff */
      UINT32_C(0x3c091234), /* lui t1,0x1234 */
      UINT32_C(0x35295678), /* ori t1,t1,0x5678 */
      UINT32_C(0xad090000), /* sw t1,0(t0): ISViewer magic */
      UINT32_C(0x3c0a4142), /* lui t2,0x4142 */
      UINT32_C(0xad0a0020), /* sw t2,0x20(t0): authored output */
      UINT32_C(0x240b0002), /* addiu t3,zero,2 */
      UINT32_C(0xa50b0016), /* sh t3,0x16(t0): flush two bytes */
      PARK_OPCODE,           /* j 0x80000420: traced park loop */
      UINT32_C(0x00000000)  /* nop */
  };
  uint8_t asset[IPL3_BYTES];
  size_t i;
  (void)memset(image, 0, TEST_ROM_BYTES);
  (void)memset(asset, 0, sizeof asset);
  if (!read_ipl3_asset(asset)) {
    check(false, "read exact cleared IPL3 asset");
    return;
  }
  put_be32(image + 0x00u, UINT32_C(0x80371240));
  put_be32(image + 0x04u, UINT32_C(0x0000000f));
  put_be32(image + 0x08u, UINT32_C(0x80000400));
  put_be32(image + 0x10u, TEST_PAYLOAD_BYTES);
  image[0x3au] = (uint8_t)'E';
  (void)memcpy(image + 0x40u, asset + 0x40u, IPL3_BYTES - 0x40u);
  for (i = 0u; i < sizeof program / sizeof program[0]; i++) {
    put_be32(image + IMAGE_PAYLOAD_OFFSET + i * 4u, program[i]);
  }
}

static bool observe_machine(t64_machine *machine, bool *saw_entry,
                            bool *saw_park, uint32_t *last_pc,
                            uint32_t *last_opcode) {
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
    if (records[i].device != T64_TRACE_DEVICE_MACHINE ||
        records[i].kind != T64_TRACE_KIND_CPU_RETIREMENT) {
      continue;
    }
    *last_pc = (uint32_t)(records[i].value >> 32);
    *last_opcode = (uint32_t)records[i].value;
    if (*last_pc == PARK_PC && *last_opcode == PARK_OPCODE) *saw_park = true;
    if (*last_pc == UINT32_C(0x80000400)) *saw_entry = true;
  }
  return true;
}

static void test_cpu_profile_authored_guest(void) {
  t64_host_allocator allocator = {allocate_machine, release_machine, NULL};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t image[TEST_ROM_BYTES];
  uint8_t output[16];
  size_t output_length = 0u;
  uint64_t output_total = 0u;
  bool output_lost = false;
  bool output_exact = false;
  bool saw_entry = false;
  bool saw_park = false;
  uint32_t last_pc = 0u;
  uint32_t last_opcode = 0u;
  uint64_t current_tick = 0u;
  uint32_t stop_reason = 0u;
  bool saw_first_store_fault = false;
  uint32_t first_store_cause = 0u;
  uint32_t first_store_opcode = 0u;
  uint32_t first_store_attempt = 0u;
  uint64_t first_store_epc = 0u;
  uint64_t first_store_address = 0u;
  uint64_t first_store_vector = 0u;
  t64_status status;

  make_authored_rom(image);
  check(t64_profile_lookup("ntsc-cart-4m-openipl3-vr4300-v1", &profile) ==
            T64_STATUS_OK,
        "lookup CPU profile");
  check(profile != NULL && profile->descriptor_version == 3u &&
            profile->state_serialization_version == 9u,
        "select descriptor-v3 CPU profile");
  if (profile == NULL) return;
  status = t64_machine_create(&allocator, profile, &machine);
  check(status == T64_STATUS_OK && machine != NULL, "create CPU machine");
  if (machine == NULL) return;
  check(t64_machine_trace_set_enabled(machine, true) == T64_STATUS_OK,
        "enable bounded execution trace");
  status = t64_machine_admit_rom(machine, image, sizeof image);
  if (status != T64_STATUS_OK) {
    (void)fprintf(stderr,
                  "authored ROM admission status=%u length=%zu entry=%08x load=%u\n",
                  status, sizeof image, (unsigned)image[8] << 24 |
                  (unsigned)image[9] << 16 | (unsigned)image[10] << 8 |
                  (unsigned)image[11],
                  (unsigned)image[0x10] << 24 | (unsigned)image[0x11] << 16 |
                  (unsigned)image[0x12] << 8 | (unsigned)image[0x13]);
  }
  check(status == T64_STATUS_OK, "admit authored open-IPL3 CPU ROM");

  while (current_tick < TEST_TICK_BOUND) {
    uint64_t step = current_tick >= UINT64_C(514000) ? 1u : 16u;
    uint64_t target = current_tick + step;
    t64_stop_result stop = {0u, 0u};
    t64_output_drain_result drained = {0u, 0u, 0u, {0u}, false, false};
    if (target > TEST_TICK_BOUND) target = TEST_TICK_BOUND;
    status = t64_machine_advance(
        machine, &(t64_stop_request){T64_STOP_GUEST_TICK, target}, &stop);
    if (status != T64_STATUS_OK) {
      (void)fprintf(stderr, "advance status=%u at tick=%llu\n", status,
                    (unsigned long long)current_tick);
      failures++;
      break;
    }
    current_tick = stop.tick;
    stop_reason = stop.reason;
    if (!observe_machine(machine, &saw_entry, &saw_park,
                         &last_pc, &last_opcode)) {
      check(false, "drain complete trace without loss");
      break;
    }
    if (t64_machine_output_drain(machine, output + output_length,
                                 sizeof output - output_length,
                                 &drained) != T64_STATUS_OK ||
        drained.copied_bytes > sizeof output - output_length) {
      check(false, "drain retained guest capture without loss");
      break;
    }
    output_length += drained.copied_bytes;
    output_total = drained.total_attempted_bytes;
    output_lost = drained.lost_output;
    output_exact = drained.total_attempted_bytes_exact;
    if (!saw_first_store_fault && machine->cpu.pending_exception_valid &&
        machine->cpu.pending_exception_epc ==
            UINT64_C(0xffffffff8000040c)) {
      saw_first_store_fault = true;
      first_store_cause = (uint32_t)machine->cpu.cp0[13];
      first_store_opcode = machine->cpu.last_opcode;
      first_store_attempt = machine->cpu.attempt_outcome;
      first_store_epc = machine->cpu.pending_exception_epc;
      first_store_address = machine->cpu.pending_exception_address;
      first_store_vector = machine->cpu.pc_full;
    }
    if (stop.reason == T64_STOP_REASON_CPU_UNSUPPORTED ||
        stop.reason == T64_STOP_REASON_GUEST_COMPLETION ||
        stop.reason == T64_STOP_REASON_HALT) {
      t64_cpu_stop_detail detail = {0u, 0u, 0u};
      if (t64_machine_cpu_stop_detail(machine, &detail) == T64_STATUS_OK) {
        (void)fprintf(stderr,
                      "guest stopped early reason=%u tick=%llu full_pc=%016llx opcode=%08x retired=%llu\n",
                      stop.reason, (unsigned long long)stop.tick,
                      (unsigned long long)(int64_t)(int32_t)detail.pc,
                      detail.opcode,
                      (unsigned long long)detail.retired_instructions);
      }
      break;
    }
  }
  check(stop_reason == T64_STOP_REASON_BOUND && current_tick == TEST_TICK_BOUND,
        "guest reaches finite guest-tick bound");
  check(saw_entry, "trace observes authored image entry");
  if (!saw_park) {
    (void)fprintf(stderr,
                  "guest trace park-miss full_pc=%016llx opcode=%08x tick=%llu stop=%u\n",
                  (unsigned long long)(int64_t)(int32_t)last_pc,
                  last_opcode, (unsigned long long)current_tick, stop_reason);
  }
  if (saw_first_store_fault) {
    (void)fprintf(stderr,
                  "first-store fault opcode=%08x full_epc=%016llx cause=%08x attempt=%u address=%016llx vector=%016llx\n",
                  first_store_opcode, (unsigned long long)first_store_epc,
                  first_store_cause, first_store_attempt,
                  (unsigned long long)first_store_address,
                  (unsigned long long)first_store_vector);
  }
  check(!saw_first_store_fault,
        "authored ISViewer store retires without a guest exception");
  check(saw_park && last_pc == PARK_PC && last_opcode == PARK_OPCODE,
        "trace observes qualified authored park PC and opcode");
  if (output_length != 2u || output_total != 2u || !output_exact ||
      output_lost || output[0] != (uint8_t)'A' || output[1] != (uint8_t)'B') {
    (void)fprintf(stderr,
                  "guest capture observed bytes=%zu total=%llu exact=%u lost=%u prefix=%02x%02x\n",
                  output_length, (unsigned long long)output_total,
                  output_exact ? 1u : 0u, output_lost ? 1u : 0u,
                  output_length > 0u ? output[0] : 0u,
                  output_length > 1u ? output[1] : 0u);
  }
  check(output_length == 2u && output_total == 2u && output_exact &&
            !output_lost && output[0] == (uint8_t)'A' &&
            output[1] == (uint8_t)'B',
        "capture contains exactly the authored AB output");
  t64_machine_destroy(machine);
}

int main(void) {
  test_cpu_profile_authored_guest();
  if (failures != 0u) {
    (void)fprintf(stderr, "runner.cpu-diagnostic: %u assertion(s) failed\n",
                  failures);
    return EXIT_FAILURE;
  }
  (void)puts("runner.cpu-diagnostic: authored CPU guest passed");
  return EXIT_SUCCESS;
}
