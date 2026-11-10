/* Native model probe. Its output is t64.model-result, never a guest result. */
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "t64_jsonw.h"
#include "tendies64/t64_machine.h"

#define PROFILE_ID "ntsc-cart-4m-skeleton"
#define RECORD_CAPACITY 2048u
#define ROM_BYTES 4096u
#define PI_BASE UINT32_C(0x04600000)
#define SP_BASE UINT32_C(0x04040000)
#define MI_PENDING UINT32_C(0x04300008)
#define CART_BASE UINT32_C(0x10000000)
#define RDRAM_BASE UINT32_C(0x00000100)
#define PI_START_TICK UINT64_C(9)
#define SP_START_TICK UINT64_C(9)
#define SP_COMPLETE_TICK UINT64_C(22)

typedef struct {
  uint64_t checked;
  uint64_t failed;
  uint64_t first_failed_index;
  uint64_t first_actual;
  uint64_t first_expected;
} assertions;

static const uint8_t pi_expected[16] = {
    0x07u, 0x14u, 0x21u, 0x2eu, 0x3bu, 0x48u, 0x55u, 0x62u,
    0x6fu, 0x7cu, 0x89u, 0x96u, 0xa3u, 0xb0u, 0xbdu, 0xcau};
static const uint8_t sp_expected[8] = {
    0xa1u, 0xb3u, 0xc5u, 0xd7u, 0xe9u, 0xfbu, 0x0du, 0x1fu};

/* Duplicated here as an independent descriptor expectation for this probe. */
static const t64_profile_descriptor expected_profile = {
    .profile_id = PROFILE_ID,
    .console_target = "NUS-CPU-03",
    .console_qualification = "unqualified-target",
    .tv_standard = T64_TV_NTSC,
    .region = T64_REGION_US,
    .media_kind = T64_MEDIA_CARTRIDGE,
    .rdram_bytes = UINT32_C(4194304),
    .expansion_bytes = 0u,
    .peripheral_flags = 0u,
    .save_kind = T64_SAVE_NONE,
    .boot_route = "hle-pif+open-ipl3",
    .boot_status = "unimplemented",
    .guest_tick_unit = "model-rsp-cycle",
    .cpu_rsp_ratio_numerator = 1u,
    .cpu_rsp_ratio_denominator = 1u,
    .synchronization_quantum = 1u,
    .timing_tier = "skeleton-atomic",
    .scheduler_version = "fifo-v1",
    .pi_model_version = "pi_dma_skeleton_v1",
    .sp_model_version = "sp_dma_skeleton_v1",
    .rom_min_bytes = ROM_BYTES,
    .rom_max_bytes = UINT32_C(67108864),
    .event_queue_capacity = 256u,
    .sequence_width_bits = 64u,
    .advance_event_budget = 4096u,
    .pi_duration_base_ticks = 1u,
    .pi_duration_bytes_divisor = 2u,
    .sp_duration_base_rsp_cycles = 12u,
    .sp_duration_bytes_divisor = 8u,
    .rom_order_policy = "all-known-orders-to-big-endian-v1",
    .event_order_policy = "tick-sequence-fifo-v1",
    .queue_exhaustion_policy = "reject-before-mutation-v1",
    .sequence_exhaustion_policy = "reject-before-mutation-v1",
    .advance_budget_policy = "bounded-4096-events-v1",
    .guest_time_bound_policy = "inclusive-idle-advance-v1",
    .halt_precedence_policy = "halt-at-bound-wins-v1",
    .pi_register_policy = "pi-register-skeleton-v1",
    .sp_register_policy = "sp-register-skeleton-v1",
    .device_error_policy = "sticky-pi-error-v1",
    .dma_cancel_policy = "token-match-complete-once-v1",
    .dma_visibility_policy = "atomic-source-at-completion-v1",
    .descriptor_version = 1u,
    .machine_model_version = 1u,
    .state_serialization_version = 5u,
};

static void record_expectation(assertions *counts, uint64_t actual,
                               uint64_t expected) {
  counts->checked++;
  if (actual != expected) {
    counts->failed++;
    if (counts->first_failed_index == 0u) {
      counts->first_failed_index = counts->checked;
      counts->first_actual = actual;
      counts->first_expected = expected;
    }
  }
}

static bool allocate_storage(void *context, size_t size, size_t alignment,
                             t64_host_storage *storage_out) {
  void *memory;
  (void)context;

  if (storage_out == NULL || size == 0u || alignment == 0u) {
    return false;
  }
  memory = malloc(size);
  if (memory == NULL || (uintptr_t)memory % alignment != 0u) {
    free(memory);
    return false;
  }
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  return true;
}

static void release_storage(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static void fill_authored_image(uint8_t image[ROM_BYTES]) {
  size_t i;
  for (i = 0u; i < ROM_BYTES; i++) {
    image[i] = (uint8_t)((i * 13u + 7u) & 0xffu);
  }
  image[0] = 0x80u;
  image[1] = 0x37u;
  image[2] = 0x12u;
  image[3] = 0x40u;
  image[0x3eu] = (uint8_t)'E';
}

static bool advance_to(t64_machine *machine, uint64_t tick,
                       t64_stop_result *result) {
  const t64_stop_request request = {T64_STOP_GUEST_TICK, tick};
  return t64_machine_advance(machine, &request, result) == T64_STATUS_OK;
}

static bool read_byte(t64_machine *machine, uint32_t address,
                      uint8_t *value_out) {
  uint32_t value = 0u;
  if (t64_machine_bus_read(machine, address, 1u, &value) != T64_STATUS_OK) {
    return false;
  }
  *value_out = (uint8_t)value;
  return true;
}

static bool read_register(t64_machine *machine, uint32_t address,
                          uint32_t *value_out) {
  return t64_machine_bus_read(machine, address, 4u, value_out) ==
         T64_STATUS_OK;
}

static bool write_register(t64_machine *machine, uint32_t address,
                           uint32_t value) {
  return t64_machine_bus_write(machine, address, 4u, value) == T64_STATUS_OK;
}

static bool run_probe(const char *profile_id, assertions *counts,
                      uint8_t profile_hash[T64_DIGEST_SIZE],
                      uint8_t content_digest[T64_DIGEST_SIZE],
                      uint8_t state_digest[T64_DIGEST_SIZE],
                      uint64_t *tick_out) {
  const t64_host_allocator allocator = {allocate_storage, release_storage,
                                        NULL};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t image[ROM_BYTES];
  uint8_t expected_profile_hash[T64_DIGEST_SIZE];
  uint8_t value_byte = 0u;
  uint32_t value = 0u;
  t64_stop_result stop = {0u, 0u};
  size_t i;
  bool completed = false;

  if (t64_profile_lookup(profile_id, &profile) != T64_STATUS_OK ||
      profile == NULL ||
      t64_profile_hash(profile, profile_hash) != T64_STATUS_OK ||
      t64_profile_hash(&expected_profile, expected_profile_hash) !=
          T64_STATUS_OK) {
    return false;
  }
  record_expectation(counts,
                     strcmp(profile->profile_id, PROFILE_ID) == 0, 1u);
  record_expectation(counts, profile->tv_standard, T64_TV_NTSC);
  record_expectation(counts, profile->region, T64_REGION_US);
  record_expectation(counts, profile->media_kind, T64_MEDIA_CARTRIDGE);
  record_expectation(counts, profile->rdram_bytes, UINT64_C(4194304));
  record_expectation(counts, profile->expansion_bytes, 0u);
  record_expectation(
      counts, strcmp(profile->boot_route, "hle-pif+open-ipl3") == 0, 1u);
  record_expectation(counts,
                     strcmp(profile->boot_status, "unimplemented") == 0, 1u);
  record_expectation(counts,
                     memcmp(profile_hash, expected_profile_hash,
                            T64_DIGEST_SIZE) == 0,
                     1u);

  if (t64_machine_create(&allocator, profile, &machine) != T64_STATUS_OK ||
      machine == NULL) {
    return false;
  }
  fill_authored_image(image);
  if (t64_machine_admit_rom(machine, image, sizeof image) != T64_STATUS_OK ||
      t64_machine_content_digest(machine, content_digest) != T64_STATUS_OK) {
    goto done;
  }
  record_expectation(counts,
                     memcmp(content_digest, (uint8_t[T64_DIGEST_SIZE]){0},
                            T64_DIGEST_SIZE) != 0,
                     1u);

  /* PI: 16 bytes from authored ROM appear only at the inclusive tick 9. */
  if (!write_register(machine, PI_BASE, RDRAM_BASE) ||
      !write_register(machine, PI_BASE + 4u, CART_BASE + UINT32_C(0x100)) ||
      !write_register(machine, PI_BASE + 12u, UINT32_C(15)) ||
      !read_register(machine, PI_BASE + 16u, &value)) {
    goto done;
  }
  record_expectation(counts, value & 1u, 1u);
  for (i = 0u; i < sizeof pi_expected; i++) {
    if (!read_byte(machine, RDRAM_BASE + (uint32_t)i, &value_byte)) {
      goto done;
    }
    record_expectation(counts, value_byte, 0u);
  }
  if (!advance_to(machine, UINT64_C(8), &stop)) {
    goto done;
  }
  record_expectation(counts, stop.tick, UINT64_C(8));
  if (!read_register(machine, PI_BASE + 16u, &value)) {
    goto done;
  }
  record_expectation(counts, value & 1u, 1u);
  for (i = 0u; i < sizeof pi_expected; i++) {
    if (!read_byte(machine, RDRAM_BASE + (uint32_t)i, &value_byte)) {
      goto done;
    }
    record_expectation(counts, value_byte, 0u);
  }
  if (!advance_to(machine, PI_START_TICK, &stop)) {
    goto done;
  }
  record_expectation(counts, stop.tick, PI_START_TICK);
  if (!read_register(machine, PI_BASE + 16u, &value)) {
    goto done;
  }
  record_expectation(counts, value & 1u, 0u);
  if (!read_register(machine, MI_PENDING, &value)) {
    goto done;
  }
  record_expectation(counts, value & UINT32_C(0x10), UINT32_C(0x10));
  for (i = 0u; i < sizeof pi_expected; i++) {
    if (!read_byte(machine, RDRAM_BASE + (uint32_t)i, &value_byte)) {
      goto done;
    }
    record_expectation(counts, value_byte, pi_expected[i]);
  }

  /* SP: an authored eight-byte RDRAM row reaches DMEM at tick 22. */
  for (i = 0u; i < sizeof sp_expected; i++) {
    if (t64_machine_bus_write(machine, UINT32_C(0x200) + (uint32_t)i, 1u,
                              sp_expected[i]) != T64_STATUS_OK) {
      goto done;
    }
  }
  if (!write_register(machine, SP_BASE, 0u) ||
      !write_register(machine, SP_BASE + 4u, UINT32_C(0x200)) ||
      !write_register(machine, SP_BASE + 8u, 0u)) {
    goto done;
  }
  for (i = 0u; i < sizeof sp_expected; i++) {
    if (!read_byte(machine, UINT32_C(0x04000000) + (uint32_t)i,
                   &value_byte)) {
      goto done;
    }
    record_expectation(counts, value_byte, 0u);
  }
  if (!advance_to(machine, SP_COMPLETE_TICK - 1u, &stop)) {
    goto done;
  }
  record_expectation(counts, stop.tick, SP_COMPLETE_TICK - 1u);
  if (!read_register(machine, SP_BASE + 0x18u, &value)) {
    goto done;
  }
  record_expectation(counts, value & 1u, 1u);
  for (i = 0u; i < sizeof sp_expected; i++) {
    if (!read_byte(machine, UINT32_C(0x04000000) + (uint32_t)i,
                   &value_byte)) {
      goto done;
    }
    record_expectation(counts, value_byte, 0u);
  }
  if (!advance_to(machine, SP_COMPLETE_TICK, &stop)) {
    goto done;
  }
  record_expectation(counts, stop.tick, SP_COMPLETE_TICK);
  if (!read_register(machine, SP_BASE + 0x18u, &value)) {
    goto done;
  }
  record_expectation(counts, value & 1u, 0u);
  for (i = 0u; i < sizeof sp_expected; i++) {
    if (!read_byte(machine, UINT32_C(0x04000000) + (uint32_t)i,
                   &value_byte)) {
      goto done;
    }
    record_expectation(counts, value_byte, sp_expected[i]);
  }
  if (!read_register(machine, MI_PENDING, &value)) {
    goto done;
  }
  record_expectation(counts, value & UINT32_C(0x3f), UINT32_C(0x10));

  if (t64_machine_digest(machine, state_digest) != T64_STATUS_OK) {
    goto done;
  }
  record_expectation(counts,
                     memcmp(state_digest, (uint8_t[T64_DIGEST_SIZE]){0},
                            T64_DIGEST_SIZE) != 0,
                     1u);
  *tick_out = stop.tick;
  completed = true;

done:
  t64_machine_destroy(machine);
  return completed;
}

static void digest_to_hex(const uint8_t digest[T64_DIGEST_SIZE],
                          char hex[T64_DIGEST_SIZE * 2u + 1u]) {
  static const char digits[] = "0123456789abcdef";
  size_t i;
  for (i = 0u; i < T64_DIGEST_SIZE; i++) {
    hex[i * 2u] = digits[digest[i] >> 4];
    hex[i * 2u + 1u] = digits[digest[i] & 0x0fu];
  }
  hex[T64_DIGEST_SIZE * 2u] = '\0';
}

static bool emit_record(char *record, size_t capacity, size_t *length_out,
                        const assertions *counts, const uint8_t profile_hash[32],
                        const uint8_t content_digest[32],
                        const uint8_t state_digest[32], uint64_t tick) {
  char profile_hex[65];
  char content_hex[65];
  char state_hex[65];
  char tick_text[32];
  const char *outcome = counts->failed == 0u ? "model-match" : "model-mismatch";
  t64_jsonw writer;

  digest_to_hex(profile_hash, profile_hex);
  digest_to_hex(content_digest, content_hex);
  digest_to_hex(state_digest, state_hex);
  if (snprintf(tick_text, sizeof tick_text, "%llu",
               (unsigned long long)tick) < 0) {
    return false;
  }
  t64_jsonw_init(&writer, record, capacity);
#define W(call)                                                                \
  do {                                                                         \
    if ((call) != T64_JSONW_OK) {                                              \
      return false;                                                            \
    }                                                                          \
  } while (0)
  W(t64_jsonw_begin_object(&writer));
  W(t64_jsonw_key(&writer, "assertions", 10u));
  W(t64_jsonw_begin_object(&writer));
  W(t64_jsonw_key(&writer, "checked", 7u));
  W(t64_jsonw_int(&writer, (int64_t)counts->checked));
  W(t64_jsonw_key(&writer, "failed", 6u));
  W(t64_jsonw_int(&writer, (int64_t)counts->failed));
  W(t64_jsonw_end_object(&writer));
  W(t64_jsonw_key(&writer, "build_id", 8u));
  W(t64_jsonw_string(&writer, t64_build_identity(),
                     strlen(t64_build_identity())));
  W(t64_jsonw_key(&writer, "content_digest", 14u));
  W(t64_jsonw_string(&writer, content_hex, 64u));
  W(t64_jsonw_key(&writer, "evidence_tier", 13u));
  W(t64_jsonw_string(&writer, "unit-tested", 11u));
  W(t64_jsonw_key(&writer, "guest_tick", 10u));
  W(t64_jsonw_string(&writer, tick_text, strlen(tick_text)));
  W(t64_jsonw_key(&writer, "model_scope", 11u));
  W(t64_jsonw_string(&writer, "native-model-probe", 18u));
  W(t64_jsonw_key(&writer, "outcome", 7u));
  W(t64_jsonw_string(&writer, outcome, strlen(outcome)));
  W(t64_jsonw_key(&writer, "profile_hash", 12u));
  W(t64_jsonw_string(&writer, profile_hex, 64u));
  W(t64_jsonw_key(&writer, "profile_id", 10u));
  W(t64_jsonw_string(&writer, PROFILE_ID, sizeof PROFILE_ID - 1u));
  W(t64_jsonw_key(&writer, "schema", 6u));
  W(t64_jsonw_string(&writer, "t64.model-result", 16u));
  W(t64_jsonw_key(&writer, "schema_version", 14u));
  W(t64_jsonw_int(&writer, 1));
  W(t64_jsonw_key(&writer, "state_digest", 12u));
  W(t64_jsonw_string(&writer, state_hex, 64u));
  W(t64_jsonw_end_object(&writer));
#undef W
  return t64_jsonw_finish(&writer, length_out) == T64_JSONW_OK;
}

static bool write_all(int fd, const char *bytes, size_t length) {
  while (length > 0u) {
    ssize_t written = write(fd, bytes, length);
    if (written < 0 && errno == EINTR) {
      continue;
    }
    if (written <= 0) {
      return false;
    }
    bytes += (size_t)written;
    length -= (size_t)written;
  }
  return true;
}

static bool emit_output(const char *path, const char *record, size_t length) {
  int fd;
  struct stat info;
  bool ok;
  if (path == NULL) {
    size_t written = fwrite(record, 1u, length, stdout);
    return written == length && fflush(stdout) == 0;
  }
  fd = open(path, O_WRONLY | O_CREAT | O_NOFOLLOW | O_NONBLOCK,
            S_IRUSR | S_IWUSR);
  if (fd < 0) {
    return false;
  }
  if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) ||
      ftruncate(fd, 0) != 0) {
    (void)close(fd);
    return false;
  }
  ok = write_all(fd, record, length);
  if (close(fd) != 0) {
    ok = false;
  }
  return ok;
}

static int usage(void) {
  (void)fprintf(stderr,
                "usage: t64-skeleton-probe --profile PROFILE [--out FILE]\n");
  return 2;
}

int main(int argc, char **argv) {
  const char *profile_id = NULL;
  const char *output_path = NULL;
  assertions counts = {0u, 0u, 0u, 0u, 0u};
  uint8_t profile_hash[T64_DIGEST_SIZE];
  uint8_t content_digest[T64_DIGEST_SIZE];
  uint8_t state_digest[T64_DIGEST_SIZE];
  char record[RECORD_CAPACITY];
  size_t record_length = 0u;
  uint64_t tick = 0u;
  int i;

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--profile") == 0 && i + 1 < argc &&
        profile_id == NULL) {
      profile_id = argv[++i];
    } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc &&
               output_path == NULL) {
      output_path = argv[++i];
    } else {
      return usage();
    }
  }
  if (profile_id == NULL || profile_id[0] == '\0') {
    return usage();
  }
  if (!run_probe(profile_id, &counts, profile_hash, content_digest,
                 state_digest, &tick)) {
    (void)fprintf(stderr, "t64-skeleton-probe: profile admission or model operation failed\n");
    return 3;
  }
  if (!emit_record(record, sizeof record, &record_length, &counts, profile_hash,
                   content_digest, state_digest, tick) ||
      !emit_output(output_path, record, record_length)) {
    (void)fprintf(stderr, "t64-skeleton-probe: could not create or write model record\n");
    return 3;
  }
  if (counts.failed != 0u) {
    (void)fprintf(stderr,
                  "t64-skeleton-probe: assertion %llu observed %llu, expected %llu (%llu total mismatch)\n",
                  (unsigned long long)counts.first_failed_index,
                  (unsigned long long)counts.first_actual,
                  (unsigned long long)counts.first_expected,
                  (unsigned long long)counts.failed);
  }
  return counts.failed == 0u ? 0 : 1;
}
