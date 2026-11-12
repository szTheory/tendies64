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

static bool make_protocol_guest_rom(const char *transcript,
                                    uint8_t **image_out, size_t *image_length) {
  const size_t transcript_length = strlen(transcript);
  const size_t code_offset = 0x100u;
  const size_t chunk_count = (transcript_length + 511u) / 512u;
  const size_t payload_length = code_offset + transcript_length * 8u +
                                chunk_count * 8u + 8u;
  const size_t rom_length = IMAGE_PAYLOAD_OFFSET + payload_length;
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
          "\"lane\":\"required-base\",\"level\":\"BasicFunctionality\","
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
                   "],\"schema\":\"t64.cpu-inventory\",\"schema_version\":2}\n");
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
                                   const char *transcript, const char *ticks,
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
  bool built = make_protocol_guest_rom(transcript, &rom, &rom_length) &&
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
                         "600000", "classification=incomplete",
                         "status=incomplete", 3, false);
  test_guest_record_case(production_executable, test_executable,
                         wrong_registration, "600000",
                         "classification=harness-error",
                         "status=identity-mismatch", 4, false);
  test_guest_record_case(production_executable, test_executable, guest_fail,
                         "600000", "classification=fail", NULL, 2, false);
  test_guest_record_case(production_executable, test_executable,
                         overflow_output, "1000000",
                         "classification=harness-error", "status=size-limit",
                         4, false);
  test_guest_record_case(production_executable, test_executable,
                         "{\"records\":[{\"event\":\"start\","
                         "\"registration_index\":0,\"value_index\":0},"
                         "{\"event\":\"outcome\",\"status\":\"pass\"},"
                         "{\"complete\":true,\"event\":\"done\","
                         "\"record_count\":\"3\"}],\"schema\":"
                         "\"t64.cpu-run\",\"schema_version\":1}\n",
                         "600000", "classification=pass", NULL, 0, true);
  free(overflow_output);
}

static void test_production_adapter(char *executable,
                                    char *test_executable) {
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

cleanup:
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
  if (argc == 3) test_production_adapter(argv[1], argv[2]);
  else check(false, "production and replacement-control executable paths");
  if (failures != 0u) {
    (void)fprintf(stderr, "runner.cpu-diagnostic: %u assertion(s) failed\n",
                  failures);
    return EXIT_FAILURE;
  }
  (void)puts("runner.cpu-diagnostic: authored CPU guest passed");
  return EXIT_SUCCESS;
}
