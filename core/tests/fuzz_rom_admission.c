#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_internal.h"
#include "tendies64/t64_machine.h"

#define FUZZ_ROM_BYTES 4096u
#define FUZZ_ROM_CAP (64u * 1024u * 1024u)
#define FUZZ_MUTATIONS 250000u
#define ALLOCATION_SLOTS 4u

typedef enum {
  ALLOC_NORMAL,
  ALLOC_DENY,
  ALLOC_SHORT,
  ALLOC_MISALIGNED,
  ALLOC_ALIAS_INPUT,
  ALLOC_ALIAS_IMAGE
} allocation_mode;

typedef struct {
  void *exposed;
  void *base;
} allocation_entry;

typedef struct {
  allocation_entry entries[ALLOCATION_SLOTS];
  allocation_mode next_mode;
  void *alias_input;
  void *alias_image;
  size_t allocate_calls;
  size_t release_calls;
  size_t outstanding;
  size_t release_errors;
} allocator_state;

typedef struct {
  size_t fixed_cases;
  size_t total_calls;
  size_t mutations_completed;
  size_t accepted;
  size_t refused;
  size_t orders[3];
  size_t header_offsets[64];
  size_t branches[12];
  uint32_t first_random;
  t64_status first_expected_status;
  t64_status first_actual_status;
  bool has_status_mismatch;
  const char *first_state_mismatch_case;
  const char *first_failure;
} campaign_stats;

enum {
  BRANCH_NULL = 0,
  BRANCH_SHORT,
  BRANCH_TOO_SMALL,
  BRANCH_NON_WORD,
  BRANCH_TOO_LARGE,
  BRANCH_BAD_ORDER,
  BRANCH_ALLOC_DENIED,
  BRANCH_BAD_STORAGE_SHORT,
  BRANCH_BAD_STORAGE_ALIGNMENT,
  BRANCH_ALIAS_INPUT,
  BRANCH_ALIAS_IMAGE,
  BRANCH_INPUT_OVERLAP
};

typedef enum {
  ORDER_BIG_ENDIAN,
  ORDER_BYTE_SWAPPED,
  ORDER_WORD_REVERSED
} byte_order;

static campaign_stats stats;
static t64_machine *machine;
static allocator_state allocator;
static uint8_t *machine_snapshot;
static uint8_t *image_snapshot;
static size_t machine_snapshot_capacity;
static size_t image_snapshot_capacity;
static size_t baseline_machine_capacity;
static const void *baseline_machine_data;
static const void *baseline_image_data;
static bool mutation_in_progress;
static bool baseline_digest_valid;
static size_t baseline_image_capacity;
static uint8_t baseline_state_digest[T64_DIGEST_SIZE];
static uint8_t baseline_content_digest[T64_DIGEST_SIZE];

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  allocator_state *state = (allocator_state *)context;
  allocation_mode mode;
  void *base;
  void *exposed;
  size_t i;

  if (state == NULL || storage_out == NULL || alignment == 0u) {
    return false;
  }
  state->allocate_calls++;
  mode = state->next_mode;
  state->next_mode = ALLOC_NORMAL;
  if (mode == ALLOC_DENY) {
    return false;
  }
  if (mode == ALLOC_ALIAS_INPUT || mode == ALLOC_ALIAS_IMAGE) {
    exposed = mode == ALLOC_ALIAS_INPUT ? state->alias_input
                                        : state->alias_image;
    storage_out->data = exposed;
    storage_out->capacity = size;
    storage_out->alignment = alignment;
    return exposed != NULL;
  }

  base = malloc(size + (mode == ALLOC_MISALIGNED ? alignment : 0u) + 1u);
  if (base == NULL) {
    return false;
  }
  exposed = mode == ALLOC_MISALIGNED ? (uint8_t *)base + 1u : base;
  for (i = 0u; i < ALLOCATION_SLOTS; i++) {
    if (state->entries[i].exposed == NULL) {
      state->entries[i].exposed = exposed;
      state->entries[i].base = base;
      state->outstanding++;
      storage_out->data = exposed;
      storage_out->capacity = mode == ALLOC_SHORT ? size - 1u : size;
      storage_out->alignment = mode == ALLOC_MISALIGNED ? 4u : alignment;
      return true;
    }
  }
  free(base);
  return false;
}

static void test_release(void *context, t64_host_storage storage) {
  allocator_state *state = (allocator_state *)context;
  size_t i;

  if (state == NULL) {
    return;
  }
  state->release_calls++;
  for (i = 0u; i < ALLOCATION_SLOTS; i++) {
    if (state->entries[i].exposed == storage.data) {
      free(state->entries[i].base);
      state->entries[i].exposed = NULL;
      state->entries[i].base = NULL;
      state->outstanding--;
      return;
    }
  }
  state->release_errors++;
}

static void fail_once(const char *label) {
  if (stats.first_failure == NULL) {
    stats.first_failure = label;
  }
}

static void fill_canonical(uint8_t *image, size_t length) {
  size_t i;

  for (i = 0u; i < length; i++) {
    image[i] = (uint8_t)((i * 37u + (i >> 4u) * 11u + 0x53u) & 0xffu);
  }
  image[0] = 0x80u;
  image[1] = 0x37u;
  image[2] = 0x12u;
  image[3] = 0x40u;
  for (i = 4u; i < T64_ROM_HEADER_SIZE; i++) {
    image[i] = (uint8_t)(0x40u + i - 4u);
  }
}

static void encode_order(const uint8_t *canonical, uint8_t *raw,
                         size_t length, byte_order order) {
  size_t i;

  if (order == ORDER_BIG_ENDIAN) {
    (void)memcpy(raw, canonical, length);
  } else if (order == ORDER_BYTE_SWAPPED) {
    for (i = 0u; i < length; i += 2u) {
      raw[i] = canonical[i + 1u];
      raw[i + 1u] = canonical[i];
    }
  } else {
    for (i = 0u; i < length; i += 4u) {
      raw[i] = canonical[i + 3u];
      raw[i + 1u] = canonical[i + 2u];
      raw[i + 2u] = canonical[i + 1u];
      raw[i + 3u] = canonical[i];
    }
  }
}

static bool observed_content_matches(const uint8_t *canonical,
                                    size_t length) {
  uint8_t observed[T64_ROM_HEADER_SIZE];
  size_t required = 0u;

  if (machine->image_length != length || machine->image_storage.data == NULL ||
      machine->image_storage.capacity < length ||
      memcmp(machine->image_storage.data, canonical, length) != 0 ||
      memcmp(machine->rom_header, canonical, T64_ROM_HEADER_SIZE) != 0 ||
      memcmp(machine->rom_title, canonical + 0x20u, T64_ROM_TITLE_BYTES) != 0 ||
      memcmp(machine->rom_game_code, canonical + 0x3bu,
             T64_ROM_GAME_CODE_BYTES) != 0 ||
      machine->rom_version != canonical[0x3fu] ||
      t64_machine_rom_header(machine, observed, sizeof observed, &required) !=
          T64_STATUS_OK ||
      required != sizeof observed ||
      memcmp(observed, canonical, sizeof observed) != 0) {
    return false;
  }
  return true;
}

static bool admit_and_check(const uint8_t *raw, const uint8_t *canonical,
                            size_t length, const char *label) {
  t64_status status = t64_machine_admit_rom(machine, raw, length);

  stats.total_calls++;
  if (!mutation_in_progress) {
    stats.fixed_cases++;
  }
  if (status != T64_STATUS_OK) {
    stats.refused++;
    fail_once(label);
    return false;
  }
  stats.accepted++;
  if (!observed_content_matches(canonical, length)) {
    fail_once(label);
    return false;
  }
  return true;
}

static void seed_nondefault_state(void) {
  machine->ram[0x100u] = 0x91u;
  machine->ram[0x101u] = 0x27u;
  machine->sp_dmem[7u] = 0x42u;
  machine->sp_imem[9u] = 0x6du;
  machine->mi_pending = UINT32_C(0x05);
  machine->mi_mask = UINT32_C(0x0a);
  machine->mi_irq_line = true;
  machine->pi.dram_address = UINT32_C(0x1240);
  machine->pi.cart_address = UINT32_C(0x10002000);
  machine->pi.active_valid = true;
  machine->pi.active_canceled = true;
  machine->pi.active_dram_address = UINT32_C(0x2000);
  machine->pi.active_cart_address = UINT32_C(0x10004000);
  machine->pi.active_length = UINT32_C(16);
  machine->pi.active_token = UINT64_C(7);
  machine->pi.next_token = UINT64_C(8);
  machine->sp.mem_address = UINT32_C(0x123);
  machine->sp.dram_address = UINT32_C(0x456);
  machine->sp.active.valid = true;
  machine->sp.active.token = UINT64_C(9);
  machine->sp.pending.valid = true;
  machine->sp.pending.token = UINT64_C(10);
  machine->sp.next_token = UINT64_C(11);
  machine->tick = UINT64_C(31);
  machine->instruction_count = UINT64_C(4);
  machine->frame_event_count = UINT64_C(2);
  machine->sticky_error_status = T64_STATUS_RESOURCE_EXHAUSTED;
  machine->halted = true;
  machine->events[0].tick = UINT64_C(35);
  machine->events[0].sequence = UINT64_C(12);
  machine->events[0].device = UINT32_C(0x701);
  machine->events[0].kind = UINT32_C(0x702);
  machine->events[0].value = UINT64_C(13);
  machine->events[0].token = UINT64_C(14);
  machine->events[0].handler = NULL;
  machine->queue_head = 0u;
  machine->queue_count = 1u;
  machine->next_sequence = UINT64_C(13);
  machine->trace_enabled = true;
  machine->trace_records[0].tick = UINT64_C(31);
  machine->trace_records[0].device = UINT32_C(0x801);
  machine->trace_records[0].kind = UINT32_C(0x802);
  machine->trace_records[0].value = UINT64_C(15);
  machine->trace_head = 0u;
  machine->trace_count = 1u;
  machine->trace_drop_count = UINT64_C(3);
}

static bool snapshot_ranges_match_baseline(void) {
  return baseline_machine_data != NULL && baseline_image_data != NULL &&
         machine->machine_storage.data == baseline_machine_data &&
         machine->machine_storage.capacity == baseline_machine_capacity &&
         machine->image_storage.data == baseline_image_data &&
         machine->image_storage.capacity == baseline_image_capacity;
}

static bool capture_immutable_baseline(bool check_public_digest) {
  seed_nondefault_state();
  if (machine_snapshot == NULL || image_snapshot == NULL ||
      machine->machine_storage.data == NULL ||
      machine->image_storage.data == NULL ||
      machine->machine_storage.capacity != machine_snapshot_capacity ||
      machine->image_storage.capacity > image_snapshot_capacity) {
    fail_once("snapshot storage unavailable");
    return false;
  }
  baseline_machine_data = machine->machine_storage.data;
  baseline_image_data = machine->image_storage.data;
  baseline_machine_capacity = machine->machine_storage.capacity;
  baseline_image_capacity = machine->image_storage.capacity;
  (void)memcpy(machine_snapshot, machine->machine_storage.data,
               baseline_machine_capacity);
  (void)memcpy(image_snapshot, machine->image_storage.data,
               baseline_image_capacity);
  baseline_digest_valid = check_public_digest;
  if (check_public_digest &&
      (t64_machine_digest(machine, baseline_state_digest) != T64_STATUS_OK ||
       t64_machine_content_digest(machine, baseline_content_digest) !=
           T64_STATUS_OK)) {
    fail_once("digest setup");
    return false;
  }
  return true;
}

static bool snapshot_oracle_rejects_shrunken_capacity(void) {
  t64_host_storage saved_machine_storage = machine->machine_storage;
  bool rejects_zero_capacity;
  bool rejects_shrunken_capacity;

  machine->machine_storage.capacity = 0u;
  rejects_zero_capacity = !snapshot_ranges_match_baseline();
  machine->machine_storage = saved_machine_storage;
  machine->machine_storage.capacity = baseline_machine_capacity - 1u;
  rejects_shrunken_capacity = !snapshot_ranges_match_baseline();
  machine->machine_storage = saved_machine_storage;
  return rejects_zero_capacity && rejects_shrunken_capacity &&
         snapshot_ranges_match_baseline();
}

static bool refusal_is_immutable(const uint8_t *bytes, size_t length,
                                 t64_status expected, const char *label,
                                 bool check_public_digest) {
  uint8_t state_digest_after[T64_DIGEST_SIZE];
  uint8_t content_digest_after[T64_DIGEST_SIZE];
  size_t outstanding_before = allocator.outstanding;
  bool unchanged = true;
  t64_status actual;

  /* machine_storage owns the full machine object and RDRAM; image_storage
   * owns the normalized content. Exact byte equality therefore preserves
   * every canonical serializer input and stored content digest without
   * rehashing 4 MiB for every early refusal. Reuse one non-default baseline
   * through consecutive refusals; successful admissions capture a new one.
   * Selected boundary controls compare both public digests directly. */
  if (machine_snapshot == NULL || image_snapshot == NULL ||
      !snapshot_ranges_match_baseline()) {
    fail_once("refusal baseline does not match owned storage");
    return false;
  }

  actual = t64_machine_admit_rom(machine, bytes, length);
  stats.total_calls++;
  if (!mutation_in_progress) {
    stats.fixed_cases++;
  }
  if (actual == T64_STATUS_OK) {
    stats.accepted++;
  } else {
    stats.refused++;
  }
  if (actual != expected) {
    if (!stats.has_status_mismatch) {
      stats.first_expected_status = expected;
      stats.first_actual_status = actual;
      stats.has_status_mismatch = true;
    }
    fail_once(label);
    unchanged = false;
  }
  if (!snapshot_ranges_match_baseline()) {
    if (stats.first_state_mismatch_case == NULL) {
      stats.first_state_mismatch_case = label;
    }
    fail_once("refusal changed owned storage pointer or capacity");
    unchanged = false;
  } else if (memcmp(machine_snapshot, baseline_machine_data,
                    baseline_machine_capacity) != 0 ||
             memcmp(image_snapshot, baseline_image_data,
                    baseline_image_capacity) != 0) {
    if (stats.first_state_mismatch_case == NULL) {
      stats.first_state_mismatch_case = label;
    }
    fail_once("refusal changed owned state or content bytes");
    unchanged = false;
  }
  if (allocator.outstanding != outstanding_before) {
    fail_once("refusal changed allocator ownership");
    unchanged = false;
  }
  if (check_public_digest && baseline_digest_valid &&
      (t64_machine_digest(machine, state_digest_after) != T64_STATUS_OK ||
       t64_machine_content_digest(machine, content_digest_after) !=
           T64_STATUS_OK ||
       memcmp(baseline_state_digest, state_digest_after, T64_DIGEST_SIZE) !=
           0 ||
       memcmp(baseline_content_digest, content_digest_after,
              T64_DIGEST_SIZE) != 0)) {
    fail_once("refusal changed public digest");
    unchanged = false;
  }
  return unchanged;
}

static uint32_t next_random(uint32_t *state) {
  uint32_t x = *state;

  x ^= x << 13u;
  x ^= x >> 17u;
  x ^= x << 5u;
  *state = x;
  return x;
}

static size_t encoded_offset(size_t canonical_offset, byte_order order) {
  if (order == ORDER_BYTE_SWAPPED) {
    return canonical_offset ^ 1u;
  }
  if (order == ORDER_WORD_REVERSED) {
    return (canonical_offset & ~(size_t)3u) +
           (3u - (canonical_offset & 3u));
  }
  return canonical_offset;
}

static bool run_mutations(size_t count) {
  uint8_t canonical[FUZZ_ROM_BYTES];
  uint8_t expected[FUZZ_ROM_BYTES];
  uint8_t raw_orders[3][FUZZ_ROM_BYTES];
  uint32_t random = UINT32_C(0x7a64c0de);
  size_t i;

  fill_canonical(canonical, sizeof canonical);
  (void)memcpy(expected, canonical, sizeof expected);
  encode_order(canonical, raw_orders[ORDER_BIG_ENDIAN], sizeof canonical,
               ORDER_BIG_ENDIAN);
  encode_order(canonical, raw_orders[ORDER_BYTE_SWAPPED], sizeof canonical,
               ORDER_BYTE_SWAPPED);
  encode_order(canonical, raw_orders[ORDER_WORD_REVERSED], sizeof canonical,
               ORDER_WORD_REVERSED);
  for (i = 0u; i < count; i++) {
    uint32_t first = next_random(&random);
    uint32_t second = next_random(&random);
    size_t offset = first % T64_ROM_HEADER_SIZE;
    byte_order order = (byte_order)((i / 2u) % 3u);
    uint8_t mask = (uint8_t)((second >> 8u) | 1u);

    mutation_in_progress = true;
    if (i % 1000u == 0u) {
      size_t changed_offset = offset < 4u
                                  ? 4u + (size_t)(second % 60u)
                                  : offset;
      size_t raw_offset = encoded_offset(changed_offset, order);
      bool accepted;

      expected[changed_offset] ^= mask;
      raw_orders[(size_t)order][raw_offset] ^= mask;
      accepted = admit_and_check(raw_orders[(size_t)order], expected,
                                 sizeof expected,
                                 "seeded accepted field mutation");
      expected[changed_offset] ^= mask;
      raw_orders[(size_t)order][raw_offset] ^= mask;
      if (!accepted || !capture_immutable_baseline(false)) {
        mutation_in_progress = false;
        fail_once("seeded accepted field mutation");
        return false;
      }
    } else if ((i & 1u) == 0u) {
      size_t changed_offset = offset < 4u
                                  ? 4u + (size_t)(second % 60u)
                                  : offset;
      size_t raw_offset = encoded_offset(changed_offset, order);
      size_t short_length = (size_t)(second % 64u);
      bool refused;

      raw_orders[(size_t)order][raw_offset] ^= mask;
      refused = refusal_is_immutable(raw_orders[(size_t)order], short_length,
                                     T64_STATUS_INVALID_ROM,
                                     "seeded short-length mutation", false);
      raw_orders[(size_t)order][raw_offset] ^= mask;
      if (!refused) {
        mutation_in_progress = false;
        return false;
      }
    } else {
      size_t signature_offset = (size_t)(first % 4u);
      bool refused;

      raw_orders[(size_t)order][signature_offset] ^= mask;
      refused = refusal_is_immutable(raw_orders[(size_t)order],
                                     sizeof expected, T64_STATUS_INVALID_ROM,
                                     "seeded malformed-signature mutation",
                                     false);
      raw_orders[(size_t)order][signature_offset] ^= mask;
      if (!refused) {
        mutation_in_progress = false;
        return false;
      }
    }
    mutation_in_progress = false;
    stats.orders[(size_t)order]++;
    stats.header_offsets[offset]++;
    stats.mutations_completed++;
  }
  stats.first_random = UINT32_C(0x7a64c0de);
  return true;
}

static void run_fixed_inventory(uint8_t *base, uint8_t *raw,
                                uint8_t *large, bool sample) {
  const t64_profile_descriptor *profile = NULL;
  static const byte_order orders[3] = {
      ORDER_BIG_ENDIAN, ORDER_BYTE_SWAPPED, ORDER_WORD_REVERSED};
  static const size_t boundaries[] = {
      64u, 4095u, FUZZ_ROM_BYTES, FUZZ_ROM_BYTES + 1u,
      FUZZ_ROM_CAP - 1u, FUZZ_ROM_CAP, FUZZ_ROM_CAP + 1u};
  uint8_t changed[FUZZ_ROM_BYTES];
  size_t i;

  fill_canonical(base, FUZZ_ROM_BYTES);
  if (t64_profile_lookup("ntsc-cart-4m-skeleton", &profile) !=
          T64_STATUS_OK ||
      profile == NULL ||
      t64_machine_create(&(t64_host_allocator){test_allocate, test_release,
                                                &allocator},
                         profile, &machine) != T64_STATUS_OK) {
    fail_once("machine setup");
    return;
  }
  encode_order(base, raw, FUZZ_ROM_BYTES, ORDER_BIG_ENDIAN);
  if (!admit_and_check(raw, base, FUZZ_ROM_BYTES, "baseline admission")) {
    return;
  }
  machine_snapshot = (uint8_t *)malloc(machine->machine_storage.capacity);
  image_snapshot = (uint8_t *)malloc(machine->image_storage.capacity);
  machine_snapshot_capacity = machine->machine_storage.capacity;
  image_snapshot_capacity = machine->image_storage.capacity;
  if (machine_snapshot == NULL || image_snapshot == NULL) {
    fail_once("snapshot allocation");
    return;
  }
  if (sample) {
    if (!capture_immutable_baseline(true) ||
        !snapshot_oracle_rejects_shrunken_capacity()) {
      fail_once("refusal snapshot oracle control");
    }
    return;
  }
  for (i = 0u; i < 3u; i++) {
    encode_order(base, raw, FUZZ_ROM_BYTES, orders[i]);
    if (!admit_and_check(raw, base, FUZZ_ROM_BYTES, "accepted byte order")) {
      return;
    }
  }
  for (i = 4u; i < T64_ROM_HEADER_SIZE; i++) {
    (void)memcpy(changed, base, sizeof changed);
    changed[i] ^= UINT8_C(0x5a);
    encode_order(changed, raw, sizeof changed, ORDER_BIG_ENDIAN);
    if (!admit_and_check(raw, changed, sizeof changed,
                         "independent header field mutation")) {
      return;
    }
    stats.header_offsets[i]++;
  }
  if (!capture_immutable_baseline(true)) {
    return;
  }
  if (!snapshot_oracle_rejects_shrunken_capacity()) {
    fail_once("refusal snapshot oracle missed zero or shrunken capacity");
    return;
  }
  for (i = 0u; i < 64u; i++) {
    (void)refusal_is_immutable(raw, i, T64_STATUS_INVALID_ROM, "short prefix",
                               i == 0u || i == 63u);
    stats.branches[BRANCH_SHORT]++;
  }
  for (i = 0u; i < sizeof boundaries / sizeof boundaries[0]; i++) {
    size_t length = boundaries[i];
    const uint8_t *input = length <= FUZZ_ROM_BYTES ? raw : large;
    if (length == FUZZ_ROM_BYTES) {
      encode_order(base, raw, FUZZ_ROM_BYTES, ORDER_BIG_ENDIAN);
      if (!admit_and_check(raw, base, length, "minimum accepted length")) {
        return;
      }
      if (!capture_immutable_baseline(true)) {
        return;
      }
      continue;
    }
    if (length == FUZZ_ROM_CAP) {
      continue;
    }
    if (length == FUZZ_ROM_CAP + 1u) {
      (void)refusal_is_immutable(input, length, T64_STATUS_INVALID_ROM,
                                 "cap plus one", true);
      stats.branches[BRANCH_TOO_LARGE]++;
    } else {
      (void)refusal_is_immutable(input, length, T64_STATUS_INVALID_ROM,
                                 "length boundary", true);
      if (length < 64u) {
        stats.branches[BRANCH_SHORT]++;
      } else if (length < 4096u) {
        stats.branches[BRANCH_TOO_SMALL]++;
      } else if (length > FUZZ_ROM_CAP) {
        stats.branches[BRANCH_TOO_LARGE]++;
      } else if ((length & 3u) != 0u) {
        stats.branches[BRANCH_NON_WORD]++;
      }
    }
  }

  (void)refusal_is_immutable(NULL, FUZZ_ROM_BYTES, T64_STATUS_INVALID_ROM,
                             "null input", true);
  stats.branches[BRANCH_NULL]++;
  (void)memcpy(changed, base, sizeof changed);
  for (i = 0u; i < 4u; i++) {
    (void)memcpy(changed, base, sizeof changed);
    changed[i] ^= UINT8_C(0x01);
    encode_order(changed, raw, sizeof changed, ORDER_BIG_ENDIAN);
    (void)refusal_is_immutable(raw, sizeof changed, T64_STATUS_INVALID_ROM,
                               "malformed signature offset", false);
    stats.branches[BRANCH_BAD_ORDER]++;
    stats.header_offsets[i]++;
  }
  (void)memcpy(changed, base, sizeof changed);
  changed[17] ^= UINT8_C(0x37);
  encode_order(changed, raw, sizeof changed, ORDER_BIG_ENDIAN);

  allocator.next_mode = ALLOC_DENY;
  (void)refusal_is_immutable(raw, sizeof changed,
                             T64_STATUS_ALLOCATION_FAILED,
                             "allocation denial", true);
  stats.branches[BRANCH_ALLOC_DENIED]++;
  allocator.next_mode = ALLOC_SHORT;
  (void)refusal_is_immutable(raw, sizeof changed, T64_STATUS_BAD_STORAGE,
                             "short reported allocation", false);
  stats.branches[BRANCH_BAD_STORAGE_SHORT]++;
  allocator.next_mode = ALLOC_MISALIGNED;
  (void)refusal_is_immutable(raw, sizeof changed, T64_STATUS_BAD_STORAGE,
                             "misaligned reported allocation", true);
  stats.branches[BRANCH_BAD_STORAGE_ALIGNMENT]++;
  allocator.alias_input = raw;
  allocator.next_mode = ALLOC_ALIAS_INPUT;
  (void)refusal_is_immutable(raw, sizeof changed, T64_STATUS_BAD_STORAGE,
                             "allocator aliases input", false);
  stats.branches[BRANCH_ALIAS_INPUT]++;
  allocator.alias_image = machine->image_storage.data;
  allocator.next_mode = ALLOC_ALIAS_IMAGE;
  (void)refusal_is_immutable(raw, sizeof changed, T64_STATUS_BAD_STORAGE,
                             "allocator aliases live image", false);
  stats.branches[BRANCH_ALIAS_IMAGE]++;
  allocator.alias_input = NULL;
  allocator.alias_image = NULL;
  (void)refusal_is_immutable(machine->image_storage.data,
                             machine->image_length, T64_STATUS_INVALID_ROM,
                             "input overlaps live image", true);
  stats.branches[BRANCH_INPUT_OVERLAP]++;
  (void)refusal_is_immutable((const uint8_t *)machine->machine_storage.data,
                             sizeof(t64_machine), T64_STATUS_INVALID_ROM,
                             "input overlaps live machine", false);
  stats.branches[BRANCH_INPUT_OVERLAP]++;

  if (t64_machine_admit_rom(NULL, raw, FUZZ_ROM_BYTES) !=
      T64_STATUS_INVALID_ROM) {
    fail_once("null machine refusal");
  }
  stats.branches[BRANCH_NULL]++;
  stats.total_calls++;
  stats.fixed_cases++;
  stats.refused++;

  /* The cap-sized acceptance runs after the mutation loop, so later refusal
   * snapshots stay bounded to the 4 KiB synthetic image. */
}

static void print_inventory(bool sample) {
  printf("# rom admission: seed=0x7a64c0de mutations=%lu fixed_cases=%lu ",
         (unsigned long)stats.mutations_completed,
         (unsigned long)stats.fixed_cases);
  printf("accepted=%lu refused=%lu orders=%lu/%lu/%lu fields=%lu ",
         (unsigned long)stats.accepted, (unsigned long)stats.refused,
         (unsigned long)stats.orders[0], (unsigned long)stats.orders[1],
         (unsigned long)stats.orders[2],
         (unsigned long)(T64_ROM_HEADER_SIZE - 4u));
  printf("inventory=%s sanitizer=asan+ubsan", sample ? "sample" :
                                                   "fixed-v1");
  printf(" total_calls=%lu branches=%lu/%lu/%lu/%lu/%lu/%lu/%lu/%lu/%lu/%lu/%lu/%lu",
         (unsigned long)stats.total_calls,
         (unsigned long)stats.branches[BRANCH_NULL],
         (unsigned long)stats.branches[BRANCH_SHORT],
         (unsigned long)stats.branches[BRANCH_TOO_SMALL],
         (unsigned long)stats.branches[BRANCH_NON_WORD],
         (unsigned long)stats.branches[BRANCH_TOO_LARGE],
         (unsigned long)stats.branches[BRANCH_BAD_ORDER],
         (unsigned long)stats.branches[BRANCH_ALLOC_DENIED],
         (unsigned long)stats.branches[BRANCH_BAD_STORAGE_SHORT],
         (unsigned long)stats.branches[BRANCH_BAD_STORAGE_ALIGNMENT],
         (unsigned long)stats.branches[BRANCH_ALIAS_INPUT],
         (unsigned long)stats.branches[BRANCH_ALIAS_IMAGE],
         (unsigned long)stats.branches[BRANCH_INPUT_OVERLAP]);
  if (stats.first_failure != NULL) {
    printf(" failure=%s", stats.first_failure);
  }
  if (stats.has_status_mismatch) {
    printf(" status_expected=%u status_actual=%u",
           (unsigned)stats.first_expected_status,
           (unsigned)stats.first_actual_status);
  }
  if (stats.first_state_mismatch_case != NULL) {
    printf(" state_case=%s", stats.first_state_mismatch_case);
  }
  putchar('\n');
}

int main(int argc, char **argv) {
  size_t mutation_count = FUZZ_MUTATIONS;
  bool sample = false;
  uint8_t base[FUZZ_ROM_BYTES];
  uint8_t raw[FUZZ_ROM_BYTES];
  uint8_t *large = NULL;
  size_t i;

  if (argc == 3 && strcmp(argv[1], "--sample") == 0) {
    char *end = NULL;
    unsigned long parsed = strtoul(argv[2], &end, 10);
    if (end == argv[2] || *end != '\0' || parsed == 0u ||
        parsed > (unsigned long)FUZZ_MUTATIONS) {
      return 2;
    }
    mutation_count = (size_t)parsed;
    sample = true;
  } else if (argc != 1) {
    return 2;
  }

  large = (uint8_t *)calloc(sample ? FUZZ_ROM_BYTES + 1u : FUZZ_ROM_CAP + 1u,
                           1u);
  if (large == NULL) {
    return 2;
  }
  fill_canonical(base, sizeof base);
  (void)memcpy(large, base, T64_ROM_HEADER_SIZE);
  run_fixed_inventory(base, raw, large, sample);
  if (stats.first_failure == NULL && !run_mutations(mutation_count)) {
    fail_once("mutation loop");
  }
  if (!sample && machine != NULL) {
    fill_canonical(large, T64_ROM_HEADER_SIZE);
    allocator.next_mode = ALLOC_NORMAL;
    (void)admit_and_check(large, large, FUZZ_ROM_CAP, "cap boundary");
  }

  if (machine != NULL) {
    t64_machine_destroy(machine);
    machine = NULL;
  }
  for (i = 0u; i < ALLOCATION_SLOTS; i++) {
    if (allocator.entries[i].exposed != NULL) {
      fail_once("allocator leaked owned storage");
      free(allocator.entries[i].base);
      allocator.entries[i].exposed = NULL;
      allocator.entries[i].base = NULL;
      allocator.outstanding--;
    }
  }
  if (allocator.outstanding != 0u || allocator.release_errors != 0u) {
    fail_once("allocator ownership accounting");
  }
  if (!sample) {
    if (mutation_count != FUZZ_MUTATIONS ||
        stats.mutations_completed != FUZZ_MUTATIONS ||
        stats.fixed_cases != 148u) {
      fail_once("exact seed, mutation, and fixed inventory");
    }
    for (i = 0u; i < T64_ROM_HEADER_SIZE; i++) {
      if (stats.header_offsets[i] == 0u) {
        fail_once("header offset inventory");
      }
    }
    for (i = 0u; i < 12u; i++) {
      if (stats.branches[i] == 0u) {
        fail_once("required refusal branch inventory");
      }
    }
  }
  printf("1..1\n");
  print_inventory(sample);
  printf("%s 1 - production admission campaign has exact mutations, inventory, and refusal invariants\n",
         stats.first_failure == NULL ? "ok" : "not ok");
  free(machine_snapshot);
  free(image_snapshot);
  free(large);
  return stats.first_failure == NULL ? 0 : 1;
}
