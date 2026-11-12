#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "t64_internal.h"
#include "t64_cpu_diag_protocol.h"
#include "t64_sha256.h"
#include "t64_verified_input.h"
#include "tendies64/t64_machine.h"

#ifndef __has_feature
#define __has_feature(feature) 0
#endif
#if defined(__SANITIZE_ADDRESS__) || __has_feature(address_sanitizer)
#define T64_CPU_DIAG_SANITIZED_TEST 1
#else
#define T64_CPU_DIAG_SANITIZED_TEST 0
#endif

#define IPL3_BYTES 4096u
#define IMAGE_PAYLOAD_OFFSET 0x1000u
#define TEST_PAYLOAD_BYTES 88u
#define TEST_ROM_BYTES 0x00100004u
#define TEST_TICK_BOUND UINT64_C(1100000)
#define TEST_BSS_PHYSICAL UINT32_C(0x00200000)
#define PARK_PC UINT32_C(0x80000448)
#define PARK_OPCODE UINT32_C(0x08000112)

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

static void make_authored_rom(uint8_t *image, size_t image_length) {
  static const uint32_t program[TEST_PAYLOAD_BYTES / 4u] = {
      UINT32_C(0x27a8fffc), /* addiu t0,sp,-4: authored stack local */
      UINT32_C(0x24091234), /* addiu t1,zero,0x1234 */
      UINT32_C(0xad090000), /* sw t1,0(t0): stack write */
      UINT32_C(0x8d0a0000), /* lw t2,0(t0): stack readback */
      UINT32_C(0x152a000f), /* bne t1,t2,fail */
      UINT32_C(0x00000000),
      UINT32_C(0x3c088020), /* lui t0,0x8020: authored BSS reservation */
      UINT32_C(0x8d090000), /* lw t1,0(t0): BSS must start zero */
      UINT32_C(0x1520000b), /* bne t1,zero,fail */
      UINT32_C(0x00000000),
      UINT32_C(0x3c08b3ff), /* lui t0,0xb3ff */
      UINT32_C(0x3c091234), /* lui t1,0x1234 */
      UINT32_C(0x35295678), /* ori t1,t1,0x5678 */
      UINT32_C(0xad090000), /* sw t1,0(t0): ISViewer magic */
      UINT32_C(0x3c0a4142), /* lui t2,0x4142 */
      UINT32_C(0xad0a0020), /* sw t2,0x20(t0): authored output */
      UINT32_C(0x240b0002), /* addiu t3,zero,2 */
      UINT32_C(0xa50b0016), /* sh t3,0x16(t0): flush two bytes */
      PARK_OPCODE,           /* j 0x80000448: traced park loop */
      UINT32_C(0x00000000), /* nop */
      UINT32_C(0x0000000d), /* break: stack/BSS validation failure */
      UINT32_C(0x00000000)  /* nop */
  };
  uint8_t asset[IPL3_BYTES];
  size_t i;
  (void)memset(image, 0, image_length);
  (void)memset(asset, 0, sizeof asset);
  if (!read_ipl3_asset(asset)) {
    check(false, "read exact cleared IPL3 asset");
    return;
  }
  put_be32(image + 0x00u, UINT32_C(0x80371240));
  put_be32(image + 0x04u, UINT32_C(0x0000000f));
  put_be32(image + 0x08u, UINT32_C(0x80000400));
  put_be32(image + 0x10u,
           (uint32_t)(image_length - IMAGE_PAYLOAD_OFFSET));
  image[0x3au] = (uint8_t)'E';
  (void)memcpy(image + 0x40u, asset + 0x40u, IPL3_BYTES - 0x40u);
  for (i = 0u; i < sizeof program / sizeof program[0]; i++) {
    put_be32(image + IMAGE_PAYLOAD_OFFSET + i * 4u, program[i]);
  }
}

static bool make_protocol_guest_rom_sized(const char *transcript,
                                          size_t minimum_image_length,
                                          uint8_t **image_out,
                                          size_t *image_length) {
  const size_t transcript_length = strlen(transcript);
  const size_t code_offset = 0x100u;
  const size_t chunk_count = (transcript_length + 511u) / 512u;
  size_t payload_length = code_offset + transcript_length * 8u +
                          chunk_count * 8u + 8u;
  size_t rom_length;
  uint8_t *image;
  uint8_t asset[IPL3_BYTES];
  size_t output_offset = code_offset;
  static const uint32_t entry_code[] = {
      UINT32_C(0x3c08b3ff), /* lui t0,0xb3ff */
      UINT32_C(0x3c091234), /* lui t1,0x1234 */
      UINT32_C(0x35295678), /* ori t1,t1,0x5678 */
      UINT32_C(0xad090000), /* sw t1,0(t0): ISViewer magic */
      UINT32_C(0x08000140), /* j 0x80000500 */
      UINT32_C(0x00000000),
      UINT32_C(0x00000000),
      UINT32_C(0x00000000),
      UINT32_C(0x08000108), /* j 0x80000420: park loop */
      UINT32_C(0x00000000)};
  size_t i;

  if (image_out == NULL || image_length == NULL || transcript == NULL ||
      transcript_length == 0u || transcript_length > 32767u ||
      !read_ipl3_asset(asset)) return false;
  if (minimum_image_length > IMAGE_PAYLOAD_OFFSET + payload_length) {
    if (minimum_image_length > UINT32_C(0x00401000) ||
        (minimum_image_length & 3u) != 0u) return false;
    payload_length = minimum_image_length - IMAGE_PAYLOAD_OFFSET;
  }
  rom_length = IMAGE_PAYLOAD_OFFSET + payload_length;
  if (payload_length > UINT32_MAX) return false;
  image = (uint8_t *)calloc(rom_length, 1u);
  if (image == NULL) return false;
  put_be32(image + 0x00u, UINT32_C(0x80371240));
  put_be32(image + 0x04u, UINT32_C(0x0000000f));
  put_be32(image + 0x08u, UINT32_C(0x80000400));
  put_be32(image + 0x10u, (uint32_t)payload_length);
  image[0x3au] = (uint8_t)'E';
  (void)memcpy(image + 0x40u, asset + 0x40u, IPL3_BYTES - 0x40u);
  for (i = 0u; i < sizeof entry_code / sizeof entry_code[0]; i++)
    put_be32(image + IMAGE_PAYLOAD_OFFSET + i * 4u, entry_code[i]);
  for (i = 0u; i < transcript_length; i++) {
    const size_t chunk_index = i % 512u;
    put_be32(image + IMAGE_PAYLOAD_OFFSET + output_offset,
             UINT32_C(0x340c0000) | (uint8_t)transcript[i]);
    output_offset += 4u;
    put_be32(image + IMAGE_PAYLOAD_OFFSET + output_offset,
             UINT32_C(0xa10c0000) | (UINT32_C(0x20) + (uint32_t)chunk_index));
    output_offset += 4u;
    if (chunk_index == 511u || i + 1u == transcript_length) {
      const size_t chunk_size = chunk_index + 1u;
      put_be32(image + IMAGE_PAYLOAD_OFFSET + output_offset,
               UINT32_C(0x240a0000) | (uint32_t)chunk_size);
      output_offset += 4u;
      put_be32(image + IMAGE_PAYLOAD_OFFSET + output_offset,
               UINT32_C(0xa50a0016));
      output_offset += 4u;
    }
  }
  put_be32(image + IMAGE_PAYLOAD_OFFSET + output_offset, UINT32_C(0x08000108));
  output_offset += 4u;
  put_be32(image + IMAGE_PAYLOAD_OFFSET + output_offset, UINT32_C(0));
  *image_out = image;
  *image_length = rom_length;
  return true;
}

static bool make_protocol_guest_rom(const char *transcript,
                                    uint8_t **image_out,
                                    size_t *image_length) {
  return make_protocol_guest_rom_sized(transcript, 0u, image_out,
                                       image_length);
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
  const t64_profile_descriptor *warm_profile = NULL;
  t64_machine *machine = NULL;
  t64_machine *warm_machine = NULL;
  uint8_t *image = (uint8_t *)calloc(TEST_ROM_BYTES, 1u);
  uint8_t output[16];
  size_t output_length = 0u;
  uint64_t output_total = 0u;
  bool output_lost = false;
  bool output_exact = false;
  bool saw_entry = false;
  bool entry_before_output = false;
  bool entry_code_layout = false;
  bool entry_bss_zero = false;
  bool guest_stack_and_bss_passed = false;
  uint64_t entry_stack_pointer = UINT64_MAX;
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

  check(image != NULL, "allocate above-warm-limit startup ROM");
  if (image == NULL) return;
  make_authored_rom(image, TEST_ROM_BYTES);
  check(t64_profile_lookup("ntsc-cart-4m-openipl3-vr4300-v1", &profile) ==
            T64_STATUS_OK,
        "lookup CPU profile");
  check(profile != NULL && profile->descriptor_version == 3u &&
            profile->state_serialization_version == 9u,
        "select descriptor-v3 CPU profile");
  check(t64_profile_lookup("ntsc-cart-4m-openipl3-warm-v1", &warm_profile) ==
                T64_STATUS_OK &&
            warm_profile != NULL &&
            warm_profile->rom_max_bytes == UINT32_C(0x00100000),
        "warm-v1 keeps its one-MiB image ceiling");
  if (profile == NULL) {
    free(image);
    return;
  }
  if (warm_profile != NULL &&
      t64_machine_create(&allocator, warm_profile, &warm_machine) ==
          T64_STATUS_OK &&
      warm_machine != NULL) {
    check(t64_machine_admit_rom(warm_machine, image, TEST_ROM_BYTES) !=
                  T64_STATUS_OK &&
              !warm_machine->has_image,
          "warm-v1 refuses the CPU-only above-one-MiB image");
    t64_machine_destroy(warm_machine);
  } else {
    check(false, "create warm-v1 machine for expansion refusal control");
  }
  status = t64_machine_create(&allocator, profile, &machine);
  check(status == T64_STATUS_OK && machine != NULL, "create CPU machine");
  if (machine == NULL) {
    free(image);
    return;
  }
  check(t64_machine_trace_set_enabled(machine, true) == T64_STATUS_OK,
        "enable bounded execution trace");
  status = t64_machine_admit_rom(machine, image, TEST_ROM_BYTES);
  if (status != T64_STATUS_OK) {
    (void)fprintf(stderr,
                  "authored ROM admission status=%u length=%zu entry=%08x load=%u\n",
                  status, (size_t)TEST_ROM_BYTES, (unsigned)image[8] << 24 |
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
    if (saw_entry && entry_stack_pointer == UINT64_MAX) {
      static const uint8_t zero_bss[16] = {0u};
      entry_stack_pointer = machine->cpu.gpr[29];
      entry_before_output = machine->output.total_attempted_bytes == 0u &&
                            machine->output.captured_bytes == 0u &&
                            machine->output.magic_register == 0u;
      guest_stack_and_bss_passed = true;
      entry_code_layout =
          memcmp(machine->ram + UINT32_C(0x400),
                 image + IMAGE_PAYLOAD_OFFSET, TEST_PAYLOAD_BYTES) == 0;
      entry_bss_zero = memcmp(machine->ram + TEST_BSS_PHYSICAL, zero_bss,
                              sizeof zero_bss) == 0;
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
            UINT64_C(0xffffffff80000434)) {
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
  check(entry_stack_pointer == UINT64_C(0xffffffff803ffee0) &&
            (entry_stack_pointer & UINT64_C(0xf)) == 0u,
        "cleared IPL3 establishes the aligned authored entry stack");
  check(entry_code_layout,
        "large flat image loads authored code at the linked app address");
  check(entry_bss_zero,
        "reserved BSS bytes remain zero at the authored entry boundary");
  check(guest_stack_and_bss_passed,
        "authored guest validates stack write/read and zero BSS before output");
  check(entry_before_output,
        "startup stack and memory layout are visible before first output");
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
  free(image);
}

static void test_cpu_profile_exact_image_limit(void) {
  const size_t image_length = 0x00401000u;
  const uint32_t payload_length = 0x00400000u;
  t64_host_allocator allocator = {allocate_machine, release_machine, NULL};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t *image = (uint8_t *)calloc(image_length, 1u);
  uint8_t asset[IPL3_BYTES];
  t64_status status;
  bool asset_ready;

  check(image != NULL, "allocate exact four-MiB authored flat image");
  asset_ready = image != NULL && read_ipl3_asset(asset);
  check(asset_ready, "load exact cleared IPL3 bytes for flat admission");
  if (image == NULL || !asset_ready) goto cleanup;
  put_be32(image + 0x00u, UINT32_C(0x80371240));
  put_be32(image + 0x04u, UINT32_C(0x0000000f));
  put_be32(image + 0x08u, UINT32_C(0x80000000));
  put_be32(image + 0x10u, payload_length);
  image[0x3au] = (uint8_t)'E';
  (void)memcpy(image + 0x40u, asset + 0x40u, IPL3_BYTES - 0x40u);
  check(t64_profile_lookup("ntsc-cart-4m-openipl3-vr4300-v1", &profile) ==
            T64_STATUS_OK &&
            profile != NULL && profile->descriptor_version == 3u &&
            profile->rom_max_bytes == UINT32_C(0x00401000) &&
            profile->state_serialization_version == 9u,
        "select the descriptor-v3 CPU profile for its flat-loader boundary");
  if (profile == NULL) goto cleanup;
  status = t64_machine_create(&allocator, profile, &machine);
  check(status == T64_STATUS_OK && machine != NULL,
        "create canonical machine for exact-limit flat admission");
  if (machine == NULL) goto cleanup;
  status = t64_machine_admit_rom(machine, image, image_length);
  if (status != T64_STATUS_OK) {
    (void)fprintf(stderr,
                  "exact-limit admission expected status=0 observed=%u image=%zu entry=%08x payload=%u\n",
                  status, image_length,
                  (unsigned)image[8] << 24 | (unsigned)image[9] << 16 |
                      (unsigned)image[10] << 8 | (unsigned)image[11],
                  (unsigned)image[0x10] << 24 |
                      (unsigned)image[0x11] << 16 |
                      (unsigned)image[0x12] << 8 | (unsigned)image[0x13]);
  }
  check(status == T64_STATUS_OK,
        "admits exact 0x00401000 image with a 4 MiB KSEG0 payload");
cleanup:
  if (machine != NULL) t64_machine_destroy(machine);
  free(image);
}

static void check_cpu_profile_rejects_transactionally(
    const t64_profile_descriptor *profile, const uint8_t *image,
    size_t image_length, const char *name) {
  t64_host_allocator allocator = {allocate_machine, release_machine, NULL};
  t64_machine *machine = NULL;
  uint8_t boot_asset[IPL3_BYTES];
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  t64_status status;

  status = t64_machine_create(&allocator, profile, &machine);
  check(status == T64_STATUS_OK && machine != NULL,
        "create machine for malformed flat image control");
  if (machine == NULL) return;
  if (!read_ipl3_asset(boot_asset)) {
    check(false, "read pinned source asset for transactional baseline");
    t64_machine_destroy(machine);
    return;
  }
  status = t64_machine_admit_rom(machine, boot_asset, sizeof boot_asset);
  check(status == T64_STATUS_OK,
        "admit pinned source asset for transactional baseline");
  status = t64_machine_digest(machine, before);
  check(status == T64_STATUS_OK, "digest machine before flat refusal");
  status = t64_machine_admit_rom(machine, image, image_length);
  check(status != T64_STATUS_OK, name);
  status = t64_machine_digest(machine, after);
  check(status == T64_STATUS_OK && memcmp(before, after, sizeof before) == 0,
        "malformed flat image refusal leaves canonical state unchanged");
  t64_machine_destroy(machine);
}

static void test_cpu_profile_malformed_flat_images(void) {
  const size_t image_length = 0x00401000u;
  const t64_profile_descriptor *profile = NULL;
  uint8_t *image = (uint8_t *)calloc(image_length + 4u, 1u);
  uint8_t asset[IPL3_BYTES];
  if (image == NULL || !read_ipl3_asset(asset) ||
      t64_profile_lookup("ntsc-cart-4m-openipl3-vr4300-v1", &profile) !=
          T64_STATUS_OK || profile == NULL) {
    check(false, "prepare malformed large flat-image controls");
    free(image);
    return;
  }
  put_be32(image + 0x00u, UINT32_C(0x80371240));
  put_be32(image + 0x04u, UINT32_C(0x0000000f));
  put_be32(image + 0x08u, UINT32_C(0x80000000));
  put_be32(image + 0x10u, UINT32_C(0x00400000));
  image[0x3au] = (uint8_t)'E';
  (void)memcpy(image + 0x40u, asset + 0x40u, IPL3_BYTES - 0x40u);

  image[0] = UINT8_C(0x37);
  check_cpu_profile_rejects_transactionally(
      profile, image, image_length, "rejects byte-swapped flat image");
  image[0] = UINT8_C(0x80);

  put_be32(image + 0x08u, UINT32_C(0x80001000));
  check_cpu_profile_rejects_transactionally(
      profile, image, image_length, "rejects entry-plus-payload overflow");
  put_be32(image + 0x08u, UINT32_C(0x80000000));

  image[0x50u] ^= UINT8_C(1);
  check_cpu_profile_rejects_transactionally(
      profile, image, image_length, "rejects malformed embedded IPL3 bytes");
  image[0x50u] ^= UINT8_C(1);

  check_cpu_profile_rejects_transactionally(
      profile, image, image_length + 4u, "rejects image above CPU profile max");
  free(image);
}

static bool write_file(const char *path, const void *bytes, size_t length) {
  size_t offset = 0u;
  int fd = open(path, O_WRONLY | O_TRUNC | O_CLOEXEC);
  if (fd < 0) return false;
  while (offset < length) {
    ssize_t count = write(fd, (const uint8_t *)bytes + offset,
                          length - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) {
      close(fd);
      return false;
    }
    offset += (size_t)count;
  }
  return close(fd) == 0;
}

static bool create_file(const char *path, const void *bytes, size_t length,
                        mode_t mode) {
  size_t offset = 0u;
  int fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, mode);
  if (fd < 0) return false;
  while (offset < length) {
    ssize_t count = write(fd, (const uint8_t *)bytes + offset,
                          length - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) {
      close(fd);
      unlink(path);
      return false;
    }
    offset += (size_t)count;
  }
  if (close(fd) != 0) {
    unlink(path);
    return false;
  }
  return true;
}

static bool replace_test_text(char *bytes, size_t *length, size_t capacity,
                              const char *from, const char *to) {
  char *match = strstr(bytes, from);
  size_t from_length = strlen(from);
  size_t to_length = strlen(to);
  size_t tail_length;
  if (match == NULL || *length + to_length < from_length ||
      *length + to_length - from_length + 1u > capacity) return false;
  tail_length = *length - (size_t)(match - bytes) - from_length;
  memmove(match + to_length, match + from_length, tail_length + 1u);
  memcpy(match, to, to_length);
  *length = *length + to_length - from_length;
  return true;
}

static bool temp_path(const char *kind, char *path, size_t capacity,
                      const void *bytes, size_t length) {
  int fd;
  int count = snprintf(path, capacity, "/tmp/t64-%s.XXXXXX", kind);
  if (count <= 0 || (size_t)count >= capacity) return false;
  fd = mkstemp(path);
  if (fd < 0) return false;
  if (fchmod(fd, 0600) != 0 || close(fd) != 0) {
    unlink(path);
    return false;
  }
  return write_file(path, bytes, length);
}

static bool public_test_path(const char *source_root, char *path,
                             size_t capacity) {
  int fd;
  int count;
  if (source_root == NULL || path == NULL || capacity == 0u) return false;
  count = snprintf(path, capacity,
                   "%s/core/tests/t64-cpu-diagnostic-public.XXXXXX",
                   source_root);
  if (count <= 0 || (size_t)count >= capacity) return false;
  fd = mkstemp(path);
  if (fd < 0) return false;
  if (close(fd) != 0 || unlink(path) != 0) {
    unlink(path);
    return false;
  }
  return true;
}

static void hex_digest(const uint8_t *bytes, size_t length,
                       char output[T64_SHA256_HEX_SIZE]) {
  t64_sha256_ctx hash;
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  t64_sha256_init(&hash);
  t64_sha256_update(&hash, bytes, length);
  t64_sha256_final(&hash, digest);
  t64_sha256_hex(digest, output);
}

static int run_tool(const char *executable, const char *const argv[], size_t argc,
                    const char *env_name, const char *env_value,
                    char *output, size_t output_capacity) {
  int output_pipe[2];
  pid_t child;
  int status;
  size_t length = 0u;
  if (pipe(output_pipe) != 0) return -1;
  child = fork();
  if (child < 0) {
    close(output_pipe[0]);
    close(output_pipe[1]);
    return -1;
  }
  if (child == 0) {
    char *child_argv[64];
    size_t i;
    if (argc >= sizeof child_argv / sizeof child_argv[0]) _exit(126);
    close(output_pipe[0]);
    (void)dup2(output_pipe[1], STDOUT_FILENO);
    close(output_pipe[1]);
    if (setenv("CI", "", 1) != 0 ||
        setenv("T64_REQUIRE_FIXTURES", "0", 1) != 0) _exit(126);
    if (env_name != NULL && env_value != NULL)
      (void)setenv(env_name, env_value, 1);
    for (i = 0u; i < argc; i++) {
      child_argv[i] = strdup(argv[i]);
      if (child_argv[i] == NULL) _exit(126);
    }
    child_argv[argc] = NULL;
    execv(executable, child_argv);
    _exit(127);
  }
  close(output_pipe[1]);
  (void)argc;
  while (length + 1u < output_capacity) {
    ssize_t count = read(output_pipe[0], output + length,
                         output_capacity - length - 1u);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) break;
    length += (size_t)count;
  }
  output[length] = '\0';
  close(output_pipe[0]);
  while (waitpid(child, &status, 0) < 0) {
    if (errno != EINTR) return -1;
  }
  return WIFEXITED(status) ? WEXITSTATUS(status) : 128;
}

static bool build_inventory(const uint8_t *rom, size_t rom_length,
                            char *output, size_t capacity) {
  static const char *const groups[] = {
      "address_error_exception", "arithmetic", "cache", "cop0", "cop1",
      "cop_unusable", "endian_re", "exception_instructions", "jumps",
      "overflow_exception", "privilege", "tlb", "tlb64", "traps"};
  const t64_profile_descriptor *profile = NULL;
  uint8_t profile_digest[T64_DIGEST_SIZE];
  char profile_hex[T64_SHA256_HEX_SIZE];
  char rom_hex[T64_SHA256_HEX_SIZE];
  char second_rom_hex[T64_SHA256_HEX_SIZE];
  uint8_t *second_rom = NULL;
  size_t offset = 0u;
  size_t i;
  int count;
  if (t64_profile_lookup("ntsc-cart-4m-openipl3-vr4300-v1", &profile) !=
          T64_STATUS_OK ||
      profile == NULL ||
      t64_profile_hash(profile, profile_digest) != T64_STATUS_OK) return false;
  second_rom = (uint8_t *)malloc(rom_length);
  if (second_rom == NULL) return false;
  memcpy(second_rom, rom, rom_length);
  second_rom[rom_length - 1u] ^= 1u;
  t64_sha256_hex(profile_digest, profile_hex);
  hex_digest(rom, rom_length, rom_hex);
  hex_digest(second_rom, rom_length, second_rom_hex);
  free(second_rom);
  count = snprintf(
      output + offset, capacity - offset,
      "{\"declared_counts\":{\"registrations\":\"14\",\"required_values\":\"15\",\"values\":\"15\"},"
      "\"identity\":{\"base_rom_sha256\":\"0000000000000000000000000000000000000000000000000000000000000000\","
      "\"core_build_sha256\":\"1111111111111111111111111111111111111111111111111111111111111111\","
      "\"overlay_sha256\":\"2222222222222222222222222222222222222222222222222222222222222222\","
      "\"profile_sha256\":\"%s\","
      "\"source_revision\":\"5555555555555555555555555555555555555555\","
      "\"tool_receipt_sha256\":\"6666666666666666666666666666666666666666666666666666666666666666\"},"
      "\"registrations\":[",
      profile_hex);
  if (count <= 0 || (size_t)count >= capacity) return false;
  offset += (size_t)count;
  for (i = 0u; i < sizeof groups / sizeof groups[0]; i++) {
    if (i == 0u) {
      count = snprintf(
          output + offset, capacity - offset,
          "{\"execution\":\"values\",\"group\":\"%s\",\"index\":%zu,"
          "\"lane\":\"required-base\",\"level\":\"Weird\","
          "\"name\":\"authored.cpu-profile.%02zu\",\"reason\":\"required\","
          "\"values\":[{\"id\":\"park-output\",\"index\":0,"
          "\"selected_rom_sha256\":\"%s\",\"selector\":\"authored-selector-a\"},"
          "{\"id\":\"park-output-second\",\"index\":1,"
          "\"selected_rom_sha256\":\"%s\",\"selector\":\"authored-selector-b\"}]}",
          groups[i], i, i, rom_hex, second_rom_hex);
    } else {
      count = snprintf(
          output + offset, capacity - offset,
          ",{\"execution\":\"values\",\"group\":\"%s\",\"index\":%zu,"
          "\"lane\":\"required-base\",\"level\":\"BasicFunctionality\","
          "\"name\":\"authored.cpu-profile.%02zu\",\"reason\":\"required\","
          "\"values\":[{\"id\":\"park-output\",\"index\":0,"
          "\"selected_rom_sha256\":\"%s\",\"selector\":\"authored-selector\"}]}",
          groups[i], i, i, rom_hex);
    }
    if (count <= 0 || (size_t)count >= capacity - offset) return false;
    offset += (size_t)count;
  }
  count = snprintf(output + offset, capacity - offset,
                   "],\"schema\":\"t64.cpu-inventory\",\"schema_version\":3}\n");
  return count > 0 && (size_t)count < capacity - offset;
}

static bool atomic_replace_file(const char *path, const void *bytes,
                                size_t length) {
  char replacement[256];
  int count = snprintf(replacement, sizeof replacement, "%s.replacement", path);
  int fd;
  bool ok;
  const uint8_t *cursor = (const uint8_t *)bytes;
  size_t offset = 0u;
  if (count <= 0 || (size_t)count >= sizeof replacement) return false;
  fd = open(replacement, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
  if (fd < 0) return false;
  ok = true;
  while (offset < length) {
    ssize_t written = write(fd, cursor + offset, length - offset);
    if (written < 0 && errno == EINTR) continue;
    if (written <= 0) {
      ok = false;
      break;
    }
    offset += (size_t)written;
  }
  if (ok) ok = fsync(fd) == 0;
  if (close(fd) != 0) ok = false;
  if (!ok || rename(replacement, path) != 0) {
    unlink(replacement);
    return false;
  }
  return true;
}

static int run_tool_with_snapshot_replacement(
    const char *executable, const char *const argv[], size_t argc,
    const char *source_path, const void *replacement, size_t replacement_len,
    char *output, size_t output_capacity) {
  int ready_pipe[2];
  int release_pipe[2];
  int output_pipe[2];
  pid_t child;
  int status;
  size_t length = 0u;
  uint8_t ready = 0u;
  uint8_t release = 1u;
  struct pollfd ready_event;
  if (pipe(ready_pipe) != 0) return -1;
  if (pipe(release_pipe) != 0) {
    close(ready_pipe[0]);
    close(ready_pipe[1]);
    return -1;
  }
  if (pipe(output_pipe) != 0) {
    close(ready_pipe[0]);
    close(ready_pipe[1]);
    close(release_pipe[0]);
    close(release_pipe[1]);
    return -1;
  }
  child = fork();
  if (child < 0) return -1;
  if (child == 0) {
    char *child_argv[64];
    char ready_text[24];
    char release_text[24];
    size_t i;
    (void)setpgid(0, 0);
    close(ready_pipe[0]);
    close(release_pipe[1]);
    close(output_pipe[0]);
    if (dup2(output_pipe[1], STDOUT_FILENO) < 0 ||
        snprintf(ready_text, sizeof ready_text, "%d", ready_pipe[1]) <= 0 ||
        snprintf(release_text, sizeof release_text, "%d", release_pipe[0]) <= 0 ||
        setenv("T64_CPU_DIAG_TEST_READY_FD", ready_text, 1) != 0 ||
        setenv("T64_CPU_DIAG_TEST_RELEASE_FD", release_text, 1) != 0 ||
        setenv("CI", "", 1) != 0 ||
        setenv("T64_REQUIRE_FIXTURES", "0", 1) != 0 ||
        argc >= sizeof child_argv / sizeof child_argv[0]) _exit(126);
    for (i = 0u; i < argc; i++) {
      child_argv[i] = strdup(argv[i]);
      if (child_argv[i] == NULL) _exit(126);
    }
    child_argv[argc] = NULL;
    execv(executable, child_argv);
    _exit(127);
  }
  (void)setpgid(child, child);
  close(ready_pipe[1]);
  close(release_pipe[0]);
  close(output_pipe[1]);
  ready_event.fd = ready_pipe[0];
  ready_event.events = POLLIN | POLLHUP;
  ready_event.revents = 0;
  if (poll(&ready_event, 1u, 5000) <= 0 ||
      read(ready_pipe[0], &ready, sizeof ready) != 1 || ready != 1u ||
      !atomic_replace_file(source_path, replacement, replacement_len) ||
      write(release_pipe[1], &release, sizeof release) != 1) {
    (void)kill(-child, SIGKILL);
    close(ready_pipe[0]);
    close(release_pipe[1]);
    close(output_pipe[0]);
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
    return -1;
  }
  close(ready_pipe[0]);
  close(release_pipe[1]);
  while (length + 1u < output_capacity) {
    ssize_t count = read(output_pipe[0], output + length,
                         output_capacity - length - 1u);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) break;
    length += (size_t)count;
  }
  output[length] = '\0';
  close(output_pipe[0]);
  while (waitpid(child, &status, 0) < 0) {
    if (errno != EINTR) return -1;
  }
  return WIFEXITED(status) ? WEXITSTATUS(status) : 128;
}

static void test_guest_record_case(const char *production_executable,
                                   const char *test_executable,
                                   const char *transcript,
                                   size_t minimum_image_length,
                                   const char *ticks,
                                   const char *classification,
                                   const char *status_text, int expected_exit,
                                   bool replace_source) {
  uint8_t *rom = NULL;
  size_t rom_length = 0u;
  char inventory[8192];
  char output[1024];
  char rom_path[128] = {0};
  char inventory_path[128] = {0};
  char batch_path[128] = {0};
  char size_text[32];
  char hash_text[T64_SHA256_HEX_SIZE];
  uint8_t *replacement = NULL;
  size_t replacement_length = 0u;
  const char *run_argv[11];
  int result;
  output[0] = '\0';
  bool built = make_protocol_guest_rom_sized(
                   transcript, minimum_image_length, &rom, &rom_length) &&
               build_inventory(rom, rom_length, inventory, sizeof inventory);
  check(built, "build actual guest record control ROM and canonical inventory");
  if (!built) goto cleanup;
  hex_digest(rom, rom_length, hash_text);
  (void)snprintf(size_text, sizeof size_text, "%zu", rom_length);
  if (!temp_path("cpu-control-rom", rom_path, sizeof rom_path, rom,
                 rom_length) ||
      !temp_path("cpu-control-inventory", inventory_path,
                 sizeof inventory_path, inventory, strlen(inventory)) ||
      !temp_path("cpu-control-batch", batch_path, sizeof batch_path, NULL, 0u)) {
    check(false, "create private guest record control paths");
    goto cleanup;
  }
  (void)unlink(batch_path);
  if (replace_source) {
    replacement = (uint8_t *)malloc(rom_length);
    if (replacement == NULL) {
      check(false, "allocate post-snapshot replacement bytes");
      goto cleanup;
    }
    memcpy(replacement, rom, rom_length);
    replacement[rom_length - 1u] ^= 1u;
    replacement_length = rom_length;
  }
  run_argv[0] = production_executable;
  run_argv[1] = "--run-authored";
  run_argv[2] = inventory_path;
  run_argv[3] = rom_path;
  run_argv[4] = size_text;
  run_argv[5] = hash_text;
  run_argv[6] = ticks;
  run_argv[7] = "0";
  run_argv[8] = "0";
  run_argv[9] = batch_path;
  if (replace_source) {
    run_argv[0] = test_executable;
    result = run_tool_with_snapshot_replacement(
        test_executable, run_argv, 10u, rom_path, replacement,
        replacement_length, output, sizeof output);
  } else {
    result = run_tool(production_executable, run_argv, 10u, NULL, NULL,
                      output, sizeof output);
  }
  if (result != expected_exit || strstr(output, classification) == NULL ||
      (status_text != NULL && strstr(output, status_text) == NULL)) {
    (void)fprintf(stderr, "guest record control exit=%d output=%s\n", result,
                  output);
  }
  check(result == expected_exit && strstr(output, classification) != NULL &&
            (status_text == NULL || strstr(output, status_text) != NULL),
        "actual adapter preserves the guest record control classification");
  if (replace_source) {
    struct stat st;
    check(access(rom_path, F_OK) == 0 && stat(rom_path, &st) == 0 &&
              (size_t)st.st_size == replacement_length,
          "atomically replaces source only after verified snapshot is sealed");
    check(result == 0 && strstr(output, "classification=pass") != NULL,
          "adapter consumes immutable snapshot after source replacement");
  }
cleanup:
  free(rom);
  free(replacement);
  if (rom_path[0] != '\0') unlink(rom_path);
  if (inventory_path[0] != '\0') unlink(inventory_path);
  if (batch_path[0] != '\0') unlink(batch_path);
}

static void test_guest_record_controls(const char *production_executable,
                                       const char *test_executable) {
  static const char missing_done[] =
      "{\"records\":[{\"event\":\"start\",\"registration_index\":0,"
      "\"value_index\":0},{\"event\":\"outcome\",\"status\":\"pass\"}],"
      "\"schema\":\"t64.cpu-run\",\"schema_version\":1}\n";
  static const char wrong_registration[] =
      "{\"records\":[{\"event\":\"start\",\"registration_index\":1,"
      "\"value_index\":0},{\"event\":\"outcome\",\"status\":\"pass\"},"
      "{\"complete\":true,\"event\":\"done\",\"record_count\":\"3\"}],"
      "\"schema\":\"t64.cpu-run\",\"schema_version\":1}\n";
  static const char guest_fail[] =
      "{\"records\":[{\"event\":\"start\",\"registration_index\":0,"
      "\"value_index\":0},{\"event\":\"outcome\",\"status\":\"fail\"},"
      "{\"complete\":true,\"event\":\"done\",\"record_count\":\"3\"}],"
      "\"schema\":\"t64.cpu-run\",\"schema_version\":1}\n";
  char *overflow_output = (char *)malloc(4098u);
  if (overflow_output == NULL) {
    check(false, "allocate output-loss guest transcript");
    return;
  }
  memset(overflow_output, 'A', 4097u);
  overflow_output[4097] = '\0';
  test_guest_record_case(production_executable, test_executable, missing_done,
                         0u, "600000", "classification=incomplete",
                         "status=incomplete", 3, false);
  test_guest_record_case(production_executable, test_executable,
                         wrong_registration, 0u, "600000",
                         "classification=harness-error",
                         "status=identity-mismatch", 4, false);
  test_guest_record_case(production_executable, test_executable, guest_fail,
                         0u, "600000", "classification=fail", NULL, 2,
                         false);
  test_guest_record_case(production_executable, test_executable,
                         overflow_output, 0u, "1000000",
                         "classification=harness-error", "status=size-limit",
                         4, false);
  test_guest_record_case(production_executable, test_executable,
                         "{\"records\":[{\"event\":\"start\","
                         "\"registration_index\":0,\"value_index\":0},"
                         "{\"event\":\"outcome\",\"status\":\"pass\"},"
                         "{\"complete\":true,\"event\":\"done\","
                         "\"record_count\":\"3\"}],\"schema\":"
                         "\"t64.cpu-run\",\"schema_version\":1}\n",
                         0u, "600000", "classification=pass", NULL, 0,
                         true);
#if !T64_CPU_DIAG_SANITIZED_TEST
  test_guest_record_case(
      production_executable, test_executable,
      "{\"records\":[{\"event\":\"start\","
      "\"registration_index\":0,\"value_index\":0},"
      "{\"event\":\"outcome\",\"status\":\"pass\"},"
      "{\"complete\":true,\"event\":\"done\","
      "\"record_count\":\"3\"}],\"schema\":"
      "\"t64.cpu-run\",\"schema_version\":1}\n",
      0x00100004u, "10000000", "classification=pass", NULL, 0, false);
#endif
  free(overflow_output);
}

static void test_build_script_preuse_refusal(const char *source_root) {
  static const char fake_compiler[] =
      "#!/bin/sh\n"
      "printf '%s\\n' FAKE_CC_INVOKED\n"
      ": > \"$T64_PREUSE_MARKER\"\n"
      "exit 73\n";
  static const char cargo_toml[] =
      "[package]\nname = \"authored-check\"\nversion = \"1.0.0\"\nedition = \"2021\"\n";
  static const char crate_bytes[] = "authored registry archive bytes\n";
  static const char closure_report[] =
      "{\"excluded_before_compile\":[\"font\",\"framebuffer\",\"graphics\",\"non_cpu_rsp_startup\"],"
      "\"source_revision\":\"9a8b9f7d94ee2f6f57d7feed70c98c22cdc30e6c\","
      "\"status\":\"PASS_PREBUILD\"}\n";
  static const char projection_report[] =
      "{\"prebuild_gate\":\"ready-for-qualified-build\","
      "\"projection_sha256\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\","
      "\"schema\":\"t64.cpu-source-projection\",\"schema_version\":2,"
      "\"source_revision\":\"9a8b9f7d94ee2f6f57d7feed70c98c22cdc30e6c\"}\n";
  static const char overlay_bytes[] = "authored overlay directives\n";
  static const char *const case_names[] = {
      "qualification status", "wrong Rust archive SHA", "missing Cargo.lock",
      "changed Cargo resolution", "unreviewed build hook", "uncleared font"};
  static const char *const reasons[] = {
      "status is not qualified",
      "rustc archive SHA-256 differs from the dated manifest",
      "packaged Cargo.lock is missing",
      "resolved dependency graph differs from the packaged Cargo.lock",
      "a registry dependency is unverified or has an unreviewed hook",
      "uncleared font, graphics, framebuffer, or non-CPU RSP startup remains"};
  static const char *const rustc_sha =
      "bd38aa1f0248f66a5682366b6deea6247f3d6c4bdd4024195a4bf37a17e2b45a";
  static const char *const cargo_sha =
      "2aef38157428f63fe9bde388f27baf79336fcffc0c6e4683035e43e6b0ca26df";
  static const char *const host_std_sha =
      "1fa77a688ee001c791401416019ee58c4470825d753030321247e8752c0a3239";
  static const char *const rust_src_sha =
      "b8bb98533e65cb2468548b20b3575518d01b44e7738045a12de2dbb71a0d6cc2";
  static const char *const manifest_sha =
      "808b3fb16540d28b08fc8f9db812a129a13129d5413cdb30607a12ec7fa09632";
  char private_template[4096] = {0};
  char private_root[4096] = {0};
  char receipt_path[4096] = {0};
  char compiler_path[4096] = {0};
  char marker_path[4096] = {0};
  char script_path[4096] = {0};
  char manifest_path[4096] = {0};
  char rustc_path[4096] = {0};
  char cargo_path[4096] = {0};
  char host_std_path[4096] = {0};
  char rust_src_path[4096] = {0};
  char cargo_manifest_path[4096] = {0};
  char lock_path[4096] = {0};
  char missing_lock_path[4096] = {0};
  char crate_path[4096] = {0};
  char closure_path[4096] = {0};
  char projection_path[4096] = {0};
  char overlay_path[4096] = {0};
  char output[8192] = {0};
  char lock_bytes[1024] = {0};
  char receipt[20000] = {0};
  char variant[20000] = {0};
  char cargo_manifest_sha[65] = {0};
  char lock_sha[65] = {0};
  char crate_sha[65] = {0};
  char closure_sha[65] = {0};
  char projection_sha[65] = {0};
  char overlay_sha[65] = {0};
  char *saved_cc = NULL;
  char *saved_marker = NULL;
  const char *old_cc = getenv("CC");
  const char *old_marker = getenv("T64_PREUSE_MARKER");
  const char *argv[4];
  const char *fault;
  const char *replacement;
  bool had_cc = old_cc != NULL;
  bool had_marker = old_marker != NULL;
  int result;
  int count;
  int length;
  size_t lock_length;
  size_t index;
  const uint8_t *lock_bytes_ptr;

  hex_digest((const uint8_t *)crate_bytes, sizeof crate_bytes - 1u, crate_sha);
  count = snprintf(lock_bytes, sizeof lock_bytes,
                   "version = 4\n\n[[package]]\nname = \"authored-dep\"\n"
                   "version = \"1.0.0\"\n"
                   "source = \"registry+https://github.com/rust-lang/crates.io-index\"\n"
                   "checksum = \"%s\"\n",
                   crate_sha);
  if (count <= 0 || (size_t)count >= sizeof lock_bytes) {
    check(false, "construct authored locked dependency control");
    return;
  }
  lock_length = (size_t)count;
  lock_bytes_ptr = (const uint8_t *)lock_bytes;
  hex_digest((const uint8_t *)cargo_toml, sizeof cargo_toml - 1u,
             cargo_manifest_sha);
  hex_digest(lock_bytes_ptr, (size_t)count, lock_sha);
  hex_digest((const uint8_t *)closure_report, sizeof closure_report - 1u,
             closure_sha);
  hex_digest((const uint8_t *)projection_report, sizeof projection_report - 1u,
             projection_sha);
  hex_digest((const uint8_t *)overlay_bytes, sizeof overlay_bytes - 1u,
             overlay_sha);

  if (had_cc) saved_cc = strdup(old_cc);
  if (had_marker) saved_marker = strdup(old_marker);
  count = snprintf(private_template, sizeof private_template,
                   "%s/build/private-cpu-diagnostic/preuse-control.XXXXXX",
                   source_root);
  if (saved_cc == NULL && had_cc) goto cleanup;
  if (saved_marker == NULL && had_marker) goto cleanup;
  if (count <= 0 || (size_t)count >= sizeof private_template ||
      mkdtemp(private_template) == NULL) goto cleanup;
  count = snprintf(private_root, sizeof private_root, "%s", private_template);
  if (count <= 0 || (size_t)count >= sizeof private_root) goto cleanup;
  count = snprintf(receipt_path, sizeof receipt_path, "%s/qualification.json",
                   private_root);
  if (count <= 0 || (size_t)count >= sizeof receipt_path) goto cleanup;
  count = snprintf(compiler_path, sizeof compiler_path, "%s/fake-cc",
                   private_root);
  if (count <= 0 || (size_t)count >= sizeof compiler_path ||
      !create_file(compiler_path, fake_compiler, sizeof fake_compiler - 1u,
                   S_IRUSR | S_IWUSR | S_IXUSR))
    goto cleanup;
  count = snprintf(marker_path, sizeof marker_path, "%s/compiler-ran",
                   private_root);
  if (count <= 0 || (size_t)count >= sizeof marker_path) goto cleanup;
  count = snprintf(script_path, sizeof script_path,
                   "%s/scripts/build-cpu-diagnostic.sh", source_root);
  if (count <= 0 || (size_t)count >= sizeof script_path) goto cleanup;
  count = snprintf(manifest_path, sizeof manifest_path,
                   "%s/build/private-cpu-diagnostic/rust-channel/channel-rust-nightly.toml",
                   source_root);
  if (count <= 0 || (size_t)count >= sizeof manifest_path) goto cleanup;
  count = snprintf(rustc_path, sizeof rustc_path,
                   "%s/build/private-cpu-diagnostic/root-host-tool-audit/rustc-nightly-aarch64-apple-darwin.tar.xz",
                   source_root);
  if (count <= 0 || (size_t)count >= sizeof rustc_path) goto cleanup;
  count = snprintf(cargo_path, sizeof cargo_path,
                   "%s/build/private-cpu-diagnostic/root-host-tool-audit/cargo-nightly-aarch64-apple-darwin.tar.xz",
                   source_root);
  if (count <= 0 || (size_t)count >= sizeof cargo_path) goto cleanup;
  count = snprintf(host_std_path, sizeof host_std_path,
                   "%s/build/private-cpu-diagnostic/root-host-tool-audit/rust-std-nightly-aarch64-apple-darwin.tar.xz",
                   source_root);
  if (count <= 0 || (size_t)count >= sizeof host_std_path) goto cleanup;
  count = snprintf(rust_src_path, sizeof rust_src_path,
                   "%s/build/private-cpu-diagnostic/root-rust-src-audit/rust-src-nightly.tar.xz",
                   source_root);
  if (count <= 0 || (size_t)count >= sizeof rust_src_path) goto cleanup;
  count = snprintf(cargo_manifest_path, sizeof cargo_manifest_path,
                   "%s/Cargo.toml", private_root);
  if (count <= 0 || (size_t)count >= sizeof cargo_manifest_path) goto cleanup;
  count = snprintf(lock_path, sizeof lock_path, "%s/Cargo.lock", private_root);
  if (count <= 0 || (size_t)count >= sizeof lock_path) goto cleanup;
  count = snprintf(missing_lock_path, sizeof missing_lock_path,
                   "%s/Missing.lock", private_root);
  if (count <= 0 || (size_t)count >= sizeof missing_lock_path) goto cleanup;
  count = snprintf(crate_path, sizeof crate_path, "%s/authored-dep.crate",
                   private_root);
  if (count <= 0 || (size_t)count >= sizeof crate_path) goto cleanup;
  count = snprintf(closure_path, sizeof closure_path, "%s/closure.json",
                   private_root);
  if (count <= 0 || (size_t)count >= sizeof closure_path) goto cleanup;
  count = snprintf(projection_path, sizeof projection_path, "%s/projection.json",
                   private_root);
  if (count <= 0 || (size_t)count >= sizeof projection_path) goto cleanup;
  count = snprintf(overlay_path, sizeof overlay_path, "%s/overlay.patch",
                   private_root);
  if (count <= 0 || (size_t)count >= sizeof overlay_path) goto cleanup;
  if (!create_file(cargo_manifest_path, cargo_toml, sizeof cargo_toml - 1u,
                   S_IRUSR | S_IWUSR) ||
      !create_file(lock_path, lock_bytes, lock_length, S_IRUSR | S_IWUSR) ||
      !create_file(crate_path, crate_bytes, sizeof crate_bytes - 1u,
                   S_IRUSR | S_IWUSR) ||
      !create_file(closure_path, closure_report, sizeof closure_report - 1u,
                   S_IRUSR | S_IWUSR) ||
      !create_file(projection_path, projection_report,
                   sizeof projection_report - 1u, S_IRUSR | S_IWUSR) ||
      !create_file(overlay_path, overlay_bytes, sizeof overlay_bytes - 1u,
                   S_IRUSR | S_IWUSR))
    goto cleanup;
  if (setenv("CC", compiler_path, 1) != 0 ||
      setenv("T64_PREUSE_MARKER", marker_path, 1) != 0)
    goto cleanup;

  length = snprintf(
      receipt, sizeof receipt,
      "{\"assets\":{\"closure_report\":{\"path\":\"%s\",\"sha256\":\"%s\"},"
      "\"excluded_before_compile\":[\"font\",\"framebuffer\",\"graphics\",\"non_cpu_rsp_startup\"],"
      "\"prebuild_closure_reviewed\":true},"
      "\"checks\":{\"build_hooks\":\"pass\",\"host_toolchain\":\"pass\","
      "\"prebuild_asset_closure\":\"pass\",\"project_lock\":\"pass\","
      "\"registry_dependencies\":\"pass\",\"rust_source\":\"pass\","
      "\"source_membership\":\"pass\",\"target_tools\":\"pass\"},"
      "\"dependencies\":[{\"archive\":{\"path\":\"%s\",\"sha256\":\"%s\"},"
      "\"build_hook_reviewed\":true,\"checksum\":\"%s\",\"legitimacy\":\"verified\","
      "\"license_reviewed\":true,\"name\":\"authored-dep\",\"proc_macro_reviewed\":true,"
      "\"source_reviewed\":true,\"version\":\"1.0.0\"}],"
      "\"project\":{\"locked\":true,\"lockfile\":{\"path\":\"%s\",\"sha256\":\"%s\"},"
      "\"manifest\":{\"path\":\"%s\",\"sha256\":\"%s\"},\"resolution_sha256\":\"%s\"},"
      "\"schema\":\"t64.cpu-qualification\",\"schema_version\":1,"
      "\"source\":{\"membership_sealed\":true,\"overlay\":{\"path\":\"%s\",\"sha256\":\"%s\"},"
      "\"overlay_sha256\":\"%s\",\"projection\":{\"path\":\"%s\",\"projection_sha256\":"
      "\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\"sha256\":\"%s\"}},"
      "\"source_revision\":\"9a8b9f7d94ee2f6f57d7feed70c98c22cdc30e6c\","
      "\"status\":\"qualified\",\"toolchain\":{\"archives\":["
      "{\"archive\":{\"path\":\"%s\",\"sha256\":\"%s\"},\"manifest_sha256\":\"%s\",\"name\":\"rustc\",\"target\":\"aarch64-apple-darwin\"},"
      "{\"archive\":{\"path\":\"%s\",\"sha256\":\"%s\"},\"manifest_sha256\":\"%s\",\"name\":\"cargo\",\"target\":\"aarch64-apple-darwin\"},"
      "{\"archive\":{\"path\":\"%s\",\"sha256\":\"%s\"},\"manifest_sha256\":\"%s\",\"name\":\"rust-std\",\"target\":\"aarch64-apple-darwin\"},"
      "{\"archive\":{\"path\":\"%s\",\"sha256\":\"%s\"},\"manifest_sha256\":\"%s\",\"name\":\"rust-src\",\"target\":\"*\"}],"
      "\"channel\":\"nightly-2026-07-16\",\"host_target\":\"aarch64-apple-darwin\","
      "\"manifest\":{\"path\":\"%s\",\"sha256\":\"%s\"}}}\n",
      closure_path, closure_sha, crate_path, crate_sha, crate_sha, lock_path,
      lock_sha, cargo_manifest_path, cargo_manifest_sha, lock_sha, overlay_path,
      overlay_sha, overlay_sha, projection_path, projection_sha, rustc_path,
      rustc_sha, rustc_sha, cargo_path, cargo_sha, cargo_sha, host_std_path,
      host_std_sha, host_std_sha, rust_src_path, rust_src_sha, rust_src_sha,
      manifest_path, manifest_sha);
  if (length <= 0 || (size_t)length >= sizeof receipt) goto cleanup;

  argv[0] = "/bin/sh";
  argv[1] = script_path;
  argv[2] = "--qualify";
  argv[3] = NULL;
  for (index = 0u; index < sizeof case_names / sizeof case_names[0]; index++) {
    size_t variant_length;
    memcpy(variant, receipt, (size_t)length + 1u);
    variant_length = (size_t)length;
    fault = NULL;
    replacement = NULL;
    switch (index) {
      case 0u:
        fault = "\"status\":\"qualified\"";
        replacement = "\"status\":\"unqualified\"";
        break;
      case 1u:
        fault = "\"manifest_sha256\":\"bd38aa1f0248f66a5682366b6deea6247f3d6c4bdd4024195a4bf37a17e2b45a\"";
        replacement = "\"manifest_sha256\":\"0000000000000000000000000000000000000000000000000000000000000000\"";
        break;
      case 2u:
        fault = lock_path;
        replacement = missing_lock_path;
        break;
      case 3u:
        fault = NULL;
        break;
      case 4u:
        fault = "\"build_hook_reviewed\":true";
        replacement = "\"build_hook_reviewed\":false";
        break;
      default:
        fault = "[\"font\",\"framebuffer\",\"graphics\",\"non_cpu_rsp_startup\"]";
        replacement = "[\"framebuffer\",\"graphics\",\"non_cpu_rsp_startup\"]";
        break;
    }
    if (index == 3u) {
      char needle[160];
      char changed[160];
      count = snprintf(needle, sizeof needle, "\"resolution_sha256\":\"%s\"", lock_sha);
      if (count <= 0 || (size_t)count >= sizeof needle) goto cleanup;
      count = snprintf(changed, sizeof changed,
                       "\"resolution_sha256\":\"0000000000000000000000000000000000000000000000000000000000000000\"");
      if (count <= 0 || (size_t)count >= sizeof changed ||
          !replace_test_text(variant, &variant_length, sizeof variant, needle,
                             changed))
        goto cleanup;
    } else if (fault == NULL || replacement == NULL ||
               !replace_test_text(variant, &variant_length, sizeof variant,
                                  fault, replacement)) {
      goto cleanup;
    }
    if (!write_file(receipt_path, variant, variant_length)) goto cleanup;
    (void)unlink(marker_path);
    output[0] = '\0';
    result = run_tool("/bin/sh", argv, 3u,
                      "T64_CPU_DIAGNOSTIC_QUALIFICATION_ROOT", private_root,
                      output, sizeof output);
    check(result == 4 && access(marker_path, F_OK) != 0 &&
              strstr(output, "pre-use qualification refused") != NULL &&
              strstr(output, reasons[index]) != NULL,
          case_names[index]);
  }

cleanup:
  if (had_cc) (void)setenv("CC", saved_cc, 1);
  else (void)unsetenv("CC");
  if (had_marker) (void)setenv("T64_PREUSE_MARKER", saved_marker, 1);
  else (void)unsetenv("T64_PREUSE_MARKER");
  if (compiler_path[0] != '\0') (void)unlink(compiler_path);
  if (receipt_path[0] != '\0') (void)unlink(receipt_path);
  if (cargo_manifest_path[0] != '\0') (void)unlink(cargo_manifest_path);
  if (lock_path[0] != '\0') (void)unlink(lock_path);
  if (crate_path[0] != '\0') (void)unlink(crate_path);
  if (closure_path[0] != '\0') (void)unlink(closure_path);
  if (projection_path[0] != '\0') (void)unlink(projection_path);
  if (overlay_path[0] != '\0') (void)unlink(overlay_path);
  if (private_root[0] != '\0') (void)rmdir(private_root);
  free(saved_cc);
  free(saved_marker);
}

static void test_production_adapter(char *executable, char *test_executable,
                                   const char *source_root) {
  static const char guest_transcript[] =
      "{\"records\":[{\"event\":\"start\",\"registration_index\":0,"
      "\"value_index\":0},{\"event\":\"outcome\",\"status\":\"pass\"},"
      "{\"complete\":true,\"event\":\"done\",\"record_count\":\"3\"}],"
      "\"schema\":\"t64.cpu-run\",\"schema_version\":1}\n";
  uint8_t *rom = NULL;
  size_t rom_length = 0u;
  char inventory[8192];
  char altered_inventory[8192];
  char tool_output[8192];
  char rom_path[128] = {0};
  char inventory_path[128] = {0};
  char altered_path[128] = {0};
  char batch_path[128] = {0};
  char encoded_path[128] = {0};
  char truncated_path[128] = {0};
  char malformed_path[128] = {0};
  char swapped_binding_path[128] = {0};
  char unregistered_binding_path[128] = {0};
  char public_batch_path[4096] = {0};
  char batch_paths[15][128] = {{0}};
  char size_text[32];
  char hash_text[T64_SHA256_HEX_SIZE];
  char second_hash_text[T64_SHA256_HEX_SIZE];
  uint8_t *second_rom = NULL;
  uint8_t *batch_bytes = NULL;
  size_t batch_length = 0u;
  size_t i;
  struct stat st;
  int fd;
  int result;
  const char *validate_argv[4] = {executable, "--validate-inventory", NULL, NULL};
  const char *run_argv[12] = {executable, "--run-authored", NULL, NULL,
                              NULL, NULL, "600000", "0", "0", NULL, NULL, NULL};
  const char *verify_argv[5] = {executable, "--verify-complete", NULL, NULL, NULL};
  const char *bad_bound_argv[12] = {executable, "--run-authored", NULL, NULL,
                              NULL, NULL, "0", "0", "0", NULL, NULL, NULL};
  const char *local_refusal_argv[12] = {executable, "--run-local",
                                  "--local-only", NULL, NULL, NULL, NULL,
                                        "600000", "0", "0", NULL, NULL};
  const char *authored_refusal_argv[11] = {
      executable, "--run-authored", NULL, NULL, NULL, NULL,
      "600000", "0", "0", NULL, NULL};

  if (executable == NULL || executable[0] == '\0') {
    check(false, "receive production adapter executable path");
    return;
  }
  check(make_protocol_guest_rom(guest_transcript, &rom, &rom_length),
        "build guest that emits canonical compact run records");
  check(rom != NULL &&
            build_inventory(rom, rom_length, inventory, sizeof inventory),
        "build canonical authored inventory covering all required groups");
  hex_digest(rom, rom_length, hash_text);
  second_rom = (uint8_t *)malloc(rom_length);
  if (second_rom != NULL) {
    memcpy(second_rom, rom, rom_length);
    second_rom[rom_length - 1u] ^= 1u;
    hex_digest(second_rom, rom_length, second_hash_text);
  }
  check(second_rom != NULL && strcmp(hash_text, second_hash_text) != 0,
        "derive a distinct authored selected-ROM identity for value two");
  (void)snprintf(size_text, sizeof size_text, "%zu", rom_length);
  check(temp_path("cpu-rom", rom_path, sizeof rom_path, rom, rom_length),
        "create private authored ROM file");
  check(temp_path("cpu-inventory", inventory_path, sizeof inventory_path,
                  inventory, strlen(inventory)),
        "create private canonical inventory file");
  check(temp_path("cpu-batch", batch_path, sizeof batch_path, NULL, 0u),
        "reserve private batch output");
  (void)unlink(batch_path);
  check(temp_path("cpu-encoded", encoded_path, sizeof encoded_path, NULL, 0u),
        "reserve private encoded output");
  check(temp_path("cpu-truncated", truncated_path, sizeof truncated_path,
                  NULL, 0u),
        "reserve private truncated batch");
  check(temp_path("cpu-malformed", malformed_path, sizeof malformed_path,
                  "{\"schema\":", 9u),
        "write malformed batch bytes");
  check(temp_path("cpu-swapped-binding", swapped_binding_path,
                  sizeof swapped_binding_path, NULL, 0u) &&
            temp_path("cpu-unregistered-binding", unregistered_binding_path,
                      sizeof unregistered_binding_path, NULL, 0u),
        "reserve private member-binding controls");
  if (failures != 0u) goto cleanup;

  validate_argv[2] = inventory_path;
  result = run_tool(executable, validate_argv, 3u, NULL, NULL, tool_output,
                    sizeof tool_output);
  check(result == 0 && strstr(tool_output, "classification=valid") != NULL,
        "production CLI validates canonical inventory");

  run_argv[2] = inventory_path;
  run_argv[3] = rom_path;
  run_argv[4] = size_text;
  run_argv[5] = hash_text;
  run_argv[9] = batch_path;
  result = run_tool(executable, run_argv, 10u, NULL, NULL, tool_output,
                    sizeof tool_output);
  if (result != 0) (void)fprintf(stderr, "adapter-run exit=%d output=%s\n", result, tool_output);
  check(result == 0 && strstr(tool_output, "classification=pass") != NULL &&
            strstr(tool_output, "park=1") != NULL,
        "production adapter derives pass from guest records and traced park");
  if (!public_test_path(source_root, public_batch_path,
                        sizeof public_batch_path)) {
    check(false, "prepare absent public-repository output path");
  } else {
    run_argv[9] = public_batch_path;
    result = run_tool(executable, run_argv, 10u, NULL, NULL, tool_output,
                      sizeof tool_output);
    check(result == 4 &&
              strstr(tool_output, "status=output-not-private") != NULL &&
              strstr(tool_output, "classification=pass") == NULL &&
              access(public_batch_path, F_OK) != 0,
          "production adapter refuses tracked public output before guest run");
    result = run_tool(executable, run_argv, 10u, "PATH", "", tool_output,
                      sizeof tool_output);
    check(result == 4 &&
              strstr(tool_output, "status=output-not-private") != NULL &&
              access(public_batch_path, F_OK) != 0,
          "production adapter fails closed on public path when git is unavailable");
    if (access(public_batch_path, F_OK) == 0) unlink(public_batch_path);
    run_argv[9] = batch_path;
  }
  {
    const char *wrong_member_rom_argv[11] = {
        executable, "--run-authored", inventory_path, rom_path, size_text,
        hash_text, "600000", "0", "1", batch_path, NULL};
    result = run_tool(executable, wrong_member_rom_argv, 10u, NULL, NULL,
                      tool_output, sizeof tool_output);
    check(result == 4 && strstr(tool_output, "rom-identity-mismatch") != NULL,
          "production adapter refuses ROM bytes bound to a different value");
  }
  test_guest_record_controls(executable, test_executable);

  verify_argv[2] = inventory_path;
  verify_argv[3] = batch_path;
  result = run_tool(executable, verify_argv, 4u, NULL, NULL, tool_output,
                    sizeof tool_output);
  if (result != 3) (void)fprintf(stderr, "single verify exit=%d output=%s\n", result, tool_output);
  check(result == 3 && strstr(tool_output, "classification=incomplete") != NULL,
        "one actual machine batch remains incomplete for the whole inventory");

  for (i = 0u; i < 15u; i++) {
    char registration_text[16];
    char value_text[16];
    size_t registration_index = i < 2u ? 0u : i - 1u;
    size_t value_index = i == 1u ? 1u : 0u;
    const char *member_encode_argv[7] = {
        executable, "--encode-inventory", inventory_path, registration_text,
        value_text, "pass", NULL};
    (void)snprintf(registration_text, sizeof registration_text, "%zu",
                   registration_index);
    (void)snprintf(value_text, sizeof value_text, "%zu", value_index);
    check(temp_path("cpu-protocol-batch", batch_paths[i],
                    sizeof batch_paths[i], NULL, 0u),
          "reserve private required-member batch");
    result = run_tool(executable, member_encode_argv, 6u, NULL, NULL,
                      tool_output, sizeof tool_output);
    check(result == 0 && strstr(tool_output, "\"event\":\"done\"") != NULL &&
              write_file(batch_paths[i], tool_output, strlen(tool_output)),
          "production encoder writes canonical required-member transcript");
    if (i == 1u) {
      char swapped_batch[T64_CPU_DIAG_MAX_BATCH_BYTES + 1u];
      char unregistered_batch[T64_CPU_DIAG_MAX_BATCH_BYTES + 1u];
      size_t swapped_length = strlen(tool_output);
      size_t unregistered_length = swapped_length;
      (void)memcpy(swapped_batch, tool_output, swapped_length + 1u);
      (void)memcpy(unregistered_batch, tool_output, swapped_length + 1u);
      if (!replace_test_text(
              swapped_batch, &swapped_length, sizeof swapped_batch,
              second_hash_text, hash_text) ||
          !replace_test_text(
              swapped_batch, &swapped_length, sizeof swapped_batch,
              "authored-selector-b", "authored-selector-a") ||
          !replace_test_text(
              unregistered_batch, &unregistered_length,
              sizeof unregistered_batch, second_hash_text,
              "8888888888888888888888888888888888888888888888888888888888888888") ||
          !replace_test_text(
              unregistered_batch, &unregistered_length,
              sizeof unregistered_batch, "authored-selector-b",
              "unregistered-selector") ||
          !write_file(swapped_binding_path, swapped_batch, swapped_length) ||
          !write_file(unregistered_binding_path, unregistered_batch,
                      unregistered_length)) {
        check(false, "construct canonical swapped and unregistered bindings");
      }
    }
  }
  {
    const char *swapped_argv[5] = {executable, "--aggregate", inventory_path,
                                  swapped_binding_path, NULL};
    result = run_tool(executable, swapped_argv, 4u, NULL, NULL, tool_output,
                      sizeof tool_output);
    check(result == 4 && strstr(tool_output, "identity-mismatch") != NULL,
          "production aggregator rejects swapped registered member binding");
  }
  {
    const char *unregistered_argv[5] = {executable, "--aggregate", inventory_path,
                                       unregistered_binding_path, NULL};
    result = run_tool(executable, unregistered_argv, 4u, NULL, NULL,
                      tool_output, sizeof tool_output);
    check(result == 4 && strstr(tool_output, "identity-mismatch") != NULL,
          "production aggregator rejects unregistered member binding");
  }
  {
    const char *complete_argv[18] = {executable, "--verify-complete", inventory_path};
    for (i = 0u; i < 15u; i++) complete_argv[3u + i] = batch_paths[i];
    result = run_tool(executable, complete_argv, 18u, NULL, NULL, tool_output,
                      sizeof tool_output);
    if (result != 0) (void)fprintf(stderr, "full aggregate exit=%d output=%s\n", result, tool_output);
    check(result == 0 && strstr(tool_output, "classification=complete") != NULL,
          "production consumer completes exact authored required set");
  }
  {
    const char *omitted_argv[17] = {executable, "--aggregate", inventory_path};
    for (i = 0u; i < 14u; i++)
      omitted_argv[3u + i] = batch_paths[i == 0u ? 0u : i + 1u];
    result = run_tool(executable, omitted_argv, 17u, NULL, NULL, tool_output,
                      sizeof tool_output);
    if (result != 3) (void)fprintf(stderr, "omitted aggregate exit=%d output=%s\n", result, tool_output);
    check(result == 3 && strstr(tool_output, "classification=incomplete") != NULL,
          "production aggregate marks an omitted required value incomplete");
  }
  {
    const char *duplicate_argv[20] = {executable, "--aggregate", inventory_path};
    for (i = 0u; i < 15u; i++) duplicate_argv[3u + i] = batch_paths[i];
    duplicate_argv[18] = batch_paths[0];
    result = run_tool(executable, duplicate_argv, 19u, NULL, NULL, tool_output,
                      sizeof tool_output);
    if (result != 4) (void)fprintf(stderr, "duplicate aggregate exit=%d output=%s\n", result, tool_output);
    check(result == 4 && strstr(tool_output, "duplicate-member") != NULL,
          "production aggregate rejects duplicate adjacent batch");
  }

  fd = open(batch_path, O_RDONLY | O_CLOEXEC);
  if (fd < 0) (void)fprintf(stderr, "actual batch open error=%d path-set=%s\n", errno, batch_path);
  if (fd >= 0 && fstat(fd, &st) == 0 && st.st_size > 12) {
    batch_length = (size_t)st.st_size;
    batch_bytes = (uint8_t *)malloc(batch_length);
    if (batch_bytes != NULL) {
      size_t offset = 0u;
      while (offset < batch_length) {
        ssize_t count = read(fd, batch_bytes + offset, batch_length - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        offset += (size_t)count;
      }
      batch_length = offset;
    }
    close(fd);
  }
  if (batch_bytes != NULL && batch_length > 12u &&
      write_file(truncated_path, batch_bytes, batch_length - 12u)) {
    const char *truncated_argv[5] = {executable, "--aggregate",
                               inventory_path, truncated_path, NULL};
    result = run_tool(executable, truncated_argv, 4u, NULL, NULL, tool_output,
                      sizeof tool_output);
    check((result == 3 || result == 4) &&
              (strstr(tool_output, "classification=incomplete") != NULL ||
               strstr(tool_output, "classification=harness-error") != NULL),
          "production consumer rejects truncated batch without completing it");
  } else {
    check(false, "prepare truncated batch control");
  }
  {
    const char *malformed_argv[5] = {executable, "--aggregate",
                               inventory_path, malformed_path, NULL};
    result = run_tool(executable, malformed_argv, 4u, NULL, NULL, tool_output,
                      sizeof tool_output);
    check(result == 4 && strstr(tool_output, "classification=harness-error") != NULL,
          "production consumer classifies malformed batch as harness error");
  }

  (void)memcpy(altered_inventory, inventory, strlen(inventory) + 1u);
  {
    char *selected = strstr(altered_inventory, "selected_rom_sha256\":\"");
    char *profile = strstr(altered_inventory, "profile_sha256\":\"");
    char *profile_value;
    char *selected_value;
    if (selected == NULL || profile == NULL) {
      check(false, "locate canonical identity values in authored test vector");
      goto cleanup;
    }
    selected_value = selected + strlen("selected_rom_sha256\":\"");
    selected_value[0] = selected_value[0] == 'a' ? 'b' : 'a';
    profile_value = profile + strlen("profile_sha256\":\"");
    profile_value[0] = profile_value[0] == 'a' ? 'b' : 'a';
  }
  check(temp_path("cpu-wrong-identity", altered_path, sizeof altered_path,
                  altered_inventory, strlen(altered_inventory)),
        "write canonical wrong-identity inventory control");
  {
    const char *wrong_argv[5] = {executable, "--aggregate", altered_path,
                                 batch_paths[0], NULL};
    result = run_tool(executable, wrong_argv, 4u, NULL, NULL, tool_output,
                      sizeof tool_output);
  }
  check(result == 4 && strstr(tool_output, "identity-mismatch") != NULL,
        "production aggregate rejects wrong ROM/profile inventory identity");

  bad_bound_argv[2] = inventory_path;
  bad_bound_argv[3] = rom_path;
  bad_bound_argv[4] = size_text;
  bad_bound_argv[5] = hash_text;
  bad_bound_argv[9] = batch_path;
  result = run_tool(executable, bad_bound_argv, 10u, NULL, NULL, tool_output,
                    sizeof tool_output);
  check(result == 4 && strstr(tool_output, "invalid-bound") != NULL,
        "production adapter classifies missing tick bound as parameter refusal");

  local_refusal_argv[3] = inventory_path;
  local_refusal_argv[4] = rom_path;
  local_refusal_argv[5] = size_text;
  local_refusal_argv[6] = hash_text;
  local_refusal_argv[10] = batch_path;
  result = run_tool(executable, local_refusal_argv, 11u, "CI", "true",
                    tool_output, sizeof tool_output);
  check(result == 4 && strstr(tool_output, "local-only-refused") != NULL,
        "external mode refuses CI even with an explicit local-only flag");

  authored_refusal_argv[2] = inventory_path;
  authored_refusal_argv[3] = rom_path;
  authored_refusal_argv[4] = size_text;
  authored_refusal_argv[5] = hash_text;
  authored_refusal_argv[9] = batch_path;
  result = run_tool(executable, authored_refusal_argv, 10u, "CI", "true",
                    tool_output, sizeof tool_output);
  check(result == 4 && strstr(tool_output, "local-only-refused") != NULL,
        "authored adapter refuses CI before accepting caller-supplied ROMs");
  result = run_tool(executable, authored_refusal_argv, 10u,
                    "T64_REQUIRE_FIXTURES", "1", tool_output,
                    sizeof tool_output);
  check(result == 4 && strstr(tool_output, "local-only-refused") != NULL,
        "authored adapter refuses required-fixture mode before guest execution");

cleanup:
  if (public_batch_path[0] != '\0') unlink(public_batch_path);
  free(batch_bytes);
  free(rom);
  free(second_rom);
  if (rom_path[0] != '\0') unlink(rom_path);
  if (inventory_path[0] != '\0') unlink(inventory_path);
  if (altered_path[0] != '\0') unlink(altered_path);
  if (batch_path[0] != '\0') unlink(batch_path);
  if (encoded_path[0] != '\0') unlink(encoded_path);
  if (truncated_path[0] != '\0') unlink(truncated_path);
  if (malformed_path[0] != '\0') unlink(malformed_path);
  if (swapped_binding_path[0] != '\0') unlink(swapped_binding_path);
  if (unregistered_binding_path[0] != '\0') unlink(unregistered_binding_path);
  for (i = 0u; i < 15u; i++)
    if (batch_paths[i][0] != '\0') unlink(batch_paths[i]);
}

int main(int argc, char **argv) {
  test_cpu_profile_authored_guest();
  test_cpu_profile_exact_image_limit();
  test_cpu_profile_malformed_flat_images();
  if (argc == 4) {
    test_build_script_preuse_refusal(argv[3]);
    test_production_adapter(argv[1], argv[2], argv[3]);
  }
  else check(false, "production and replacement-control executable paths");
  if (failures != 0u) {
    (void)fprintf(stderr, "runner.cpu-diagnostic: %u assertion(s) failed\n",
                  failures);
    return EXIT_FAILURE;
  }
  (void)puts("runner.cpu-diagnostic: authored CPU guest passed");
  return EXIT_SUCCESS;
}
