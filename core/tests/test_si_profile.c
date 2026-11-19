/*
 * Phase 9 composite-profile identity tests (plan 09-01).
 *
 * predicate-matrix: every registered pre-Phase-9 id keeps its version
 * predicate row. The expected rows below are derived by hand from the
 * definitions in core/t64_internal.h (version tuples are descriptor, model,
 * state), not from running the predicates.
 *
 * si-profile-identity, quantum-identity and family-devices cover the four
 * registered composite-family ids: tuple, labels, validated numeric fields,
 * quantum and ratio, every refusal leaving allocator counters unchanged, and
 * the inherited-device behavior of each id.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_internal.h"
#include "t64_sha256.h"
#include "t64_test.h"

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  void *memory;

  (void)context;
  if (storage_out == NULL || size == 0u || alignment == 0u) return false;
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

static void test_release(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static const t64_host_allocator allocator = {test_allocate, test_release,
                                             NULL};

/* Predicate bits, in the order of the plan's predicate list. */
#define P_AI (1u << 0)
#define P_V9 (1u << 1)
#define P_RDP_VI (1u << 2)
#define P_SCAN_V8 (1u << 3)
#define P_SCAN (1u << 4)
#define P_TRI (1u << 5)
#define P_TEX (1u << 6)
#define P_COMB (1u << 7)
#define P_BLEND (1u << 8)
#define P_DEPTH (1u << 9)
#define P_VR4300 (1u << 10)
#define P_T1 (1u << 11)
#define P_RDRAMP (1u << 12)
#define P_COMPAT_EXT (1u << 13)
#define P_RSP (1u << 14)
#define P_COMPAT (1u << 15)
/* Phase 9 family predicate; legacy rows never carry it. */
#define P_SI (1u << 16)

/* Every predicate reached through t64_has_rdp_vi_profile at descriptor 6+. */
#define CHAIN                                                                  \
  (P_VR4300 | P_T1 | P_RDRAMP | P_COMPAT_EXT | P_RSP | P_COMPAT)

typedef struct {
  const char *id;
  uint32_t expected;
} matrix_row;

static const matrix_row legacy_rows[] = {
    /* (1,1,5) */
    {"ntsc-cart-4m-skeleton", 0u},
    /* (2,2,8) */
    {"ntsc-cart-4m-openipl3-warm-v1", 0u},
    /* (3,3,9): VR4300 state only */
    {"ntsc-cart-4m-openipl3-vr4300-v1", P_VR4300},
    /* (3,4,10): compat policy by its own tuple */
    {"ntsc-cart-4m-openipl3-vr4300-compat-v1", P_VR4300 | P_COMPAT},
    /* (3,5,11): compat extension */
    {"ntsc-cart-4m-openipl3-vr4300-compat-v2",
     P_VR4300 | P_COMPAT_EXT | P_COMPAT},
    /* (3,6,12): RSP functional */
    {"ntsc-cart-4m-openipl3-rsp-functional-v1",
     P_VR4300 | P_COMPAT_EXT | P_RSP | P_COMPAT},
    /* (4,7,13): T1 policy */
    {"ntsc-cart-4m-openipl3-rdram-a-t1-v1",
     P_VR4300 | P_T1 | P_COMPAT_EXT | P_RSP | P_COMPAT},
    /* (4,8,13): RDRAM protocol */
    {"ntsc-cart-4m-openipl3-rdram-b-t1-v1", CHAIN},
    /* (6,11,16): first RDP/VI model */
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v2", P_RDP_VI | CHAIN},
    /* (7,12,17) */
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-triangle-v3",
     P_RDP_VI | P_TRI | CHAIN},
    /* (7,13,18) */
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-texture-v4",
     P_RDP_VI | P_TRI | P_TEX | CHAIN},
    /* (8,14,19) */
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-combiner-v5",
     P_RDP_VI | P_TRI | P_TEX | P_COMB | CHAIN},
    /* (9,15,20): depth needs descriptor 10 */
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-blender-v6",
     P_RDP_VI | P_TRI | P_TEX | P_COMB | P_BLEND | CHAIN},
    /* (10,16,21): first row with RDP depth */
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-depth-v7",
     P_RDP_VI | P_TRI | P_TEX | P_COMB | P_BLEND | P_DEPTH | CHAIN},
    /* (10,17,22): scanout v8 */
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v8",
     P_RDP_VI | P_SCAN_V8 | P_SCAN | P_TRI | P_TEX | P_COMB | P_BLEND |
         P_DEPTH | CHAIN},
    /* (10,18,23): filter v9 */
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9",
     P_V9 | P_RDP_VI | P_SCAN | P_TRI | P_TEX | P_COMB | P_BLEND | P_DEPTH |
         CHAIN},
    /* (11,19,24): audio; depth stays inactive (guarded by descriptor 10) */
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1",
     P_AI | P_V9 | P_RDP_VI | P_SCAN | P_TRI | P_TEX | P_COMB | P_BLEND |
         CHAIN},
};

#define LEGACY_ROW_COUNT (sizeof legacy_rows / sizeof legacy_rows[0])
#define AI_ROW_INDEX 16u

/*
 * Composite family stem: the AI row except the exact-tuple AI predicate is
 * false and the family predicate is true. RDP depth stays inactive (K1).
 */
static const matrix_row family_rows[] = {
    {"ntsc-cart-4m-openipl3-rdram-b-t1-si-v1",
     (P_V9 | P_RDP_VI | P_SCAN | P_TRI | P_TEX | P_COMB | P_BLEND | CHAIN |
      P_SI)},
};

#define FAMILY_ROW_COUNT (sizeof family_rows / sizeof family_rows[0])

static uint32_t observed_row(const t64_machine *machine) {
  uint32_t row = 0u;

  row |= t64_has_ai_profile(machine) ? P_AI : 0u;
  row |= t64_has_vi_filter_v9_profile(machine) ? P_V9 : 0u;
  row |= t64_has_rdp_vi_profile(machine) ? P_RDP_VI : 0u;
  row |= t64_has_vi_scanout_v8_profile(machine) ? P_SCAN_V8 : 0u;
  row |= t64_has_vi_scanout_profile(machine) ? P_SCAN : 0u;
  row |= t64_has_rdp_triangle_profile(machine) ? P_TRI : 0u;
  row |= t64_has_rdp_texture_profile(machine) ? P_TEX : 0u;
  row |= t64_has_rdp_combiner_profile(machine) ? P_COMB : 0u;
  row |= t64_has_rdp_blender_profile(machine) ? P_BLEND : 0u;
  row |= t64_has_rdp_depth_profile(machine) ? P_DEPTH : 0u;
  row |= t64_has_vr4300_cpu_state(machine) ? P_VR4300 : 0u;
  row |= t64_has_t1_policy(machine) ? P_T1 : 0u;
  row |= t64_has_rdram_protocol_policy(machine) ? P_RDRAMP : 0u;
  row |= t64_has_cpu_compat_extension_policy(machine) ? P_COMPAT_EXT : 0u;
  row |= t64_has_rsp_functional_profile(machine) ? P_RSP : 0u;
  row |= t64_has_cpu_compat_policy(machine) ? P_COMPAT : 0u;
  row |= t64_has_si_profile(machine) ? P_SI : 0u;
  return row;
}

static t64_machine *create_machine(const char *profile_id) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;

  T64_CHECK_INT(t64_profile_lookup(profile_id, &profile), T64_STATUS_OK);
  if (profile == NULL) return NULL;
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  return machine;
}

static void case_predicate_matrix(void) {
  size_t index;

  for (index = 0u; index < LEGACY_ROW_COUNT; index++) {
    const matrix_row *row = &legacy_rows[index];
    t64_machine *machine = create_machine(row->id);
    uint32_t observed;

    T64_CHECK(machine != NULL);
    if (machine == NULL) continue;
    observed = observed_row(machine);
    if (observed != row->expected) {
      fprintf(stderr, "predicate row differs for %s: got 0x%04x want 0x%04x\n",
              row->id, (unsigned)observed, (unsigned)row->expected);
    }
    T64_CHECK_INT(observed, row->expected);
    t64_machine_destroy(machine);
  }
  T64_CHECK_INT(LEGACY_ROW_COUNT, 17);

  /* The family rows equal the AI row except the AI and family predicates. */
  for (index = 0u; index < FAMILY_ROW_COUNT; index++) {
    const matrix_row *row = &family_rows[index];
    t64_machine *machine = create_machine(row->id);
    uint32_t observed;
    uint32_t ai_row = legacy_rows[AI_ROW_INDEX].expected;

    T64_CHECK(machine != NULL);
    if (machine == NULL) continue;
    observed = observed_row(machine);
    if (observed != row->expected) {
      fprintf(stderr, "predicate row differs for %s: got 0x%05x want 0x%05x\n",
              row->id, (unsigned)observed, (unsigned)row->expected);
    }
    T64_CHECK_INT(observed, row->expected);
    T64_CHECK_INT(row->expected, (ai_row & ~(uint32_t)P_AI) | P_SI);
    T64_CHECK((observed & P_DEPTH) == 0u);
    T64_CHECK(t64_has_ai_device(machine));
    T64_CHECK(!t64_has_ai_profile(machine));
    t64_machine_destroy(machine);
  }
  for (index = 0u; index < LEGACY_ROW_COUNT; index++) {
    t64_machine *machine = create_machine(legacy_rows[index].id);

    T64_CHECK(machine != NULL);
    if (machine == NULL) continue;
    T64_CHECK(t64_has_ai_device(machine) ==
              ((legacy_rows[index].expected & P_AI) != 0u));
    t64_machine_destroy(machine);
  }

  /* K1: depth is active on the three descriptor 10 rows only. */
  T64_CHECK((legacy_rows[13].expected & P_DEPTH) != 0u);
  T64_CHECK((legacy_rows[14].expected & P_DEPTH) != 0u);
  T64_CHECK((legacy_rows[15].expected & P_DEPTH) != 0u);
  T64_CHECK((legacy_rows[16].expected & P_DEPTH) == 0u);
  T64_CHECK((family_rows[0].expected & P_DEPTH) == 0u);
}

/* ---- shared helpers for the family cases -------------------------------- */

#define IPL3_ASSET_BYTES 4096u
#define SI_STEM "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1"
#define AI_PROFILE_ID \
  "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1"
#define SI_FAMILY_COUNT 4u
#define SERIALIZED_CAPACITY T64_PROFILE_SERIALIZED_MAX_BYTES

static const char *const si_ids[SI_FAMILY_COUNT] = {
    SI_STEM, SI_STEM "-e4k", SI_STEM "-e16k", SI_STEM "-s32k"};
static const uint32_t si_save_kinds[SI_FAMILY_COUNT] = {
    T64_SAVE_NONE, T64_SAVE_EEPROM_4K, T64_SAVE_EEPROM_16K,
    T64_SAVE_SRAM_32K};
static const size_t si_id_lengths[SI_FAMILY_COUNT] = {38u, 42u, 43u, 43u};

/* Allocator that counts every call so a refusal can prove it allocated
 * nothing and released nothing. */
typedef struct {
  size_t allocations;
  size_t outstanding;
  size_t release_errors;
} counting_state;

static bool counting_allocate(void *context, size_t size, size_t alignment,
                              t64_host_storage *storage_out) {
  counting_state *state = (counting_state *)context;
  void *memory;

  if (state == NULL || storage_out == NULL || size == 0u || alignment == 0u) {
    return false;
  }
  state->allocations++;
  memory = malloc(size);
  if (memory == NULL || (uintptr_t)memory % alignment != 0u) {
    free(memory);
    return false;
  }
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  state->outstanding++;
  return true;
}

static void counting_release(void *context, t64_host_storage storage) {
  counting_state *state = (counting_state *)context;

  if (state == NULL) return;
  if (storage.data == NULL || state->outstanding == 0u) {
    state->release_errors++;
    return;
  }
  state->outstanding--;
  free(storage.data);
}

static bool read_openipl3_asset(uint8_t bytes[IPL3_ASSET_BYTES]) {
  const char *path = getenv("T64_TEST_IPL3_PATH");
  FILE *file;
  size_t length;
  int extra;
  int read_error;
  int close_error;

  if (path == NULL || path[0] == '\0' || bytes == NULL) return false;
  file = fopen(path, "rb");
  if (file == NULL) return false;
  length = fread(bytes, 1u, IPL3_ASSET_BYTES, file);
  extra = fgetc(file);
  read_error = ferror(file);
  close_error = fclose(file);
  return read_error == 0 && close_error == 0 && length == IPL3_ASSET_BYTES &&
         extra == EOF;
}

static uint32_t read_u32_be(const uint8_t *bytes) {
  return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
         ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

static const t64_profile_descriptor *lookup_profile(const char *profile_id) {
  const t64_profile_descriptor *profile = NULL;

  T64_CHECK_INT(t64_profile_lookup(profile_id, &profile), T64_STATUS_OK);
  return profile;
}

static void hex_of(const uint8_t digest[T64_DIGEST_SIZE],
                   char hex[2u * T64_DIGEST_SIZE + 1u]) {
  static const char digits[] = "0123456789abcdef";
  size_t i;

  for (i = 0u; i < T64_DIGEST_SIZE; i++) {
    hex[2u * i] = digits[digest[i] >> 4];
    hex[2u * i + 1u] = digits[digest[i] & 0x0fu];
  }
  hex[2u * T64_DIGEST_SIZE] = '\0';
}

/* SHA-256 computed outside the core over the serialized descriptor bytes. */
static void external_digest(const uint8_t *bytes, size_t length,
                            uint8_t digest[T64_DIGEST_SIZE]) {
  t64_sha256_ctx context;

  t64_sha256_init(&context);
  t64_sha256_update(&context, bytes, length);
  t64_sha256_final(&context, digest);
}

/* A status check that names its case so the RED observation is readable. */
static void expect_status(const char *label, t64_status got,
                          t64_status want) {
  if (got != want) {
    fprintf(stderr, "%s: got status %u want %u\n", label, (unsigned)got,
            (unsigned)want);
  }
  T64_CHECK_INT(got, want);
}

/*
 * Hash, serialize and create must all refuse the descriptor with
 * INVALID_ARGUMENT, return no machine and leave every allocator counter
 * unchanged.
 */
static void expect_refused_everywhere(const char *label,
                                      const t64_profile_descriptor *profile) {
  counting_state state = {0u, 0u, 0u};
  const t64_host_allocator host = {counting_allocate, counting_release,
                                   &state};
  uint8_t digest[T64_DIGEST_SIZE];
  uint8_t serialized[SERIALIZED_CAPACITY];
  size_t length = 0u;
  t64_machine *machine = NULL;
  char text[160];

  (void)snprintf(text, sizeof text, "%s hash", label);
  expect_status(text, t64_profile_hash(profile, digest),
                T64_STATUS_INVALID_ARGUMENT);
  (void)snprintf(text, sizeof text, "%s serialize", label);
  expect_status(text,
                t64_profile_serialize(profile, serialized, sizeof serialized,
                                      &length),
                T64_STATUS_INVALID_ARGUMENT);
  (void)snprintf(text, sizeof text, "%s create", label);
  expect_status(text, t64_machine_create(&host, profile, &machine),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK(machine == NULL);
  T64_CHECK_INT(state.allocations, 0u);
  T64_CHECK_INT(state.outstanding, 0u);
  T64_CHECK_INT(state.release_errors, 0u);
  if (machine != NULL) t64_machine_destroy(machine);
}

/*
 * The descriptor hashes and serializes, differs from the original, and
 * machine creation still refuses it before any allocation because the id is
 * hash-pinned at create.
 */
static void expect_pinned_at_create(const char *label,
                                    const t64_profile_descriptor *original,
                                    const t64_profile_descriptor *changed,
                                    const uint8_t original_hash[T64_DIGEST_SIZE],
                                    const uint8_t *original_bytes,
                                    size_t original_length) {
  counting_state state = {0u, 0u, 0u};
  const t64_host_allocator host = {counting_allocate, counting_release,
                                   &state};
  uint8_t digest[T64_DIGEST_SIZE];
  uint8_t serialized[SERIALIZED_CAPACITY];
  size_t length = 0u;
  t64_machine *machine = NULL;
  char text[160];

  (void)original;
  (void)snprintf(text, sizeof text, "%s hash", label);
  expect_status(text, t64_profile_hash(changed, digest), T64_STATUS_OK);
  T64_CHECK(memcmp(digest, original_hash, T64_DIGEST_SIZE) != 0);
  (void)snprintf(text, sizeof text, "%s serialize", label);
  expect_status(text,
                t64_profile_serialize(changed, serialized, sizeof serialized,
                                      &length),
                T64_STATUS_OK);
  T64_CHECK(length != original_length ||
            memcmp(serialized, original_bytes, length) != 0);
  (void)snprintf(text, sizeof text, "%s create", label);
  expect_status(text, t64_machine_create(&host, changed, &machine),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK(machine == NULL);
  T64_CHECK_INT(state.allocations, 0u);
  T64_CHECK_INT(state.outstanding, 0u);
  T64_CHECK_INT(state.release_errors, 0u);
  if (machine != NULL) t64_machine_destroy(machine);
}

typedef struct {
  const char *name;
  size_t offset;
} text_field;

#define TEXT_FIELD(field) {#field, offsetof(t64_profile_descriptor, field)}

/* Every text field a descriptor-12 profile serializes, in layout order. */
static const text_field family_text_fields[] = {
    TEXT_FIELD(profile_id),
    TEXT_FIELD(console_target),
    TEXT_FIELD(console_qualification),
    TEXT_FIELD(boot_route),
    TEXT_FIELD(boot_status),
    TEXT_FIELD(guest_tick_unit),
    TEXT_FIELD(timing_tier),
    TEXT_FIELD(scheduler_version),
    TEXT_FIELD(pi_model_version),
    TEXT_FIELD(sp_model_version),
    TEXT_FIELD(rom_order_policy),
    TEXT_FIELD(event_order_policy),
    TEXT_FIELD(queue_exhaustion_policy),
    TEXT_FIELD(sequence_exhaustion_policy),
    TEXT_FIELD(advance_budget_policy),
    TEXT_FIELD(guest_time_bound_policy),
    TEXT_FIELD(halt_precedence_policy),
    TEXT_FIELD(pi_register_policy),
    TEXT_FIELD(sp_register_policy),
    TEXT_FIELD(device_error_policy),
    TEXT_FIELD(dma_cancel_policy),
    TEXT_FIELD(dma_visibility_policy),
    TEXT_FIELD(rdp_work_policy),
    TEXT_FIELD(vi_clock_policy),
    TEXT_FIELD(rdp_noise_policy),
    TEXT_FIELD(rdp_undefined_color_policy),
    TEXT_FIELD(vi_output_policy),
    TEXT_FIELD(rdp_triangle_policy),
    TEXT_FIELD(rdp_combiner_policy),
    TEXT_FIELD(rdp_depth_policy),
    TEXT_FIELD(ai_policy),
    TEXT_FIELD(si_policy),
    TEXT_FIELD(save_policy),
};

#define FAMILY_TEXT_FIELD_COUNT \
  (sizeof family_text_fields / sizeof family_text_fields[0])

/* Boot identity fields serialized from descriptor 2 onward. */
static const text_field family_boot_fields[] = {
    TEXT_FIELD(asset_sha256),
    TEXT_FIELD(payload_sha256),
    TEXT_FIELD(boot_policy_sha256),
    TEXT_FIELD(hle_seed_reset_version),
};

#define FAMILY_BOOT_FIELD_COUNT \
  (sizeof family_boot_fields / sizeof family_boot_fields[0])

/* The retained Phase 8 and earlier hash pins, same literals as
 * core/tests/test_profile.c, re-asserted so the new descriptor arm cannot
 * move an older byte. */
typedef struct {
  const char *id;
  const char *hash_hex;
  size_t length;
} legacy_pin;

static const legacy_pin legacy_pins[] = {
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-triangle-v3",
     "794710f767fce5f1bd37d1116a6067bac10026aa4c7d77fc9cfe7c3636d5830f",
     1116u},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-texture-v4",
     "3ce5b6c75ba85a8b1d5ed333820d70736c26c003ec16c76a7cd9f8ee686f3e18",
     1132u},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-combiner-v5",
     "0e9938d6b92159fe1f8c5dff0c03749979d5d10b11b458224f2dcca1a2345efa",
     1172u},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-blender-v6",
     "7f73b46e6c695fddb60d89d6351c1c099f56e76dbe109ab9f2cb9f372fc56d0e",
     1155u},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-depth-v7",
     "56f25e100d3c92792f836b0664e81697919487b4fdae49404476a85135ae5969",
     1231u},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v8",
     "4f929a757bc6c257729918ccd92e632256fc5ad991dab2317a272cba1d22dc1d",
     1243u},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9",
     "ebc4370b89cf52d0b2125bac2ab6b40b0fc9f5bef44c8bf9c28647c073514015",
     1256u},
    {AI_PROFILE_ID,
     "eaa400aa23bfa3d0c9ba6d475d8eba7a60302151e22b6278ff6a4991d08bb5bf",
     1319u},
};

static void check_legacy_pins(void) {
  size_t index;

  for (index = 0u; index < sizeof legacy_pins / sizeof legacy_pins[0];
       index++) {
    const t64_profile_descriptor *profile = lookup_profile(legacy_pins[index].id);
    uint8_t digest[T64_DIGEST_SIZE];
    uint8_t serialized[SERIALIZED_CAPACITY];
    char hex[2u * T64_DIGEST_SIZE + 1u];
    size_t length = 0u;

    if (profile == NULL) continue;
    T64_CHECK_INT(t64_profile_hash(profile, digest), T64_STATUS_OK);
    hex_of(digest, hex);
    T64_CHECK_STR(hex, legacy_pins[index].hash_hex);
    T64_CHECK_INT(t64_profile_serialize(profile, serialized,
                                        sizeof serialized, &length),
                  T64_STATUS_OK);
    T64_CHECK_INT(length, legacy_pins[index].length);
  }
}

static void check_family_identity(size_t index, const uint8_t ai_hash[T64_DIGEST_SIZE]) {
  const t64_profile_descriptor *profile = lookup_profile(si_ids[index]);
  counting_state state = {0u, 0u, 0u};
  const t64_host_allocator host = {counting_allocate, counting_release,
                                   &state};
  uint8_t digest[T64_DIGEST_SIZE];
  uint8_t reference[T64_DIGEST_SIZE];
  uint8_t serialized[SERIALIZED_CAPACITY];
  uint8_t asset[IPL3_ASSET_BYTES];
  size_t length = 0u;
  t64_machine *machine = NULL;

  if (profile == NULL) return;
  T64_CHECK_STR(profile->profile_id, si_ids[index]);
  T64_CHECK_INT(strlen(profile->profile_id), si_id_lengths[index]);
  T64_CHECK(strlen(profile->profile_id) <= T64_PROFILE_TEXT_CAPACITY - 1u);
  T64_CHECK_INT(profile->descriptor_version, 12u);
  T64_CHECK_INT(profile->machine_model_version, 20u);
  T64_CHECK_INT(profile->state_serialization_version, 25u);
  T64_CHECK_INT(profile->peripheral_flags, 1u);
  T64_CHECK_INT(profile->peripheral_flags,
                T64_PERIPHERAL_STANDARD_CONTROLLER_PORT0);
  T64_CHECK_INT(profile->save_kind, si_save_kinds[index]);
  T64_CHECK_INT(profile->synchronization_quantum, 1u);
  T64_CHECK_INT(profile->cpu_rsp_ratio_numerator, 1u);
  T64_CHECK_INT(profile->cpu_rsp_ratio_denominator, 1u);
  T64_CHECK_STR(profile->si_policy, "si-hle-joybus-wire-v1");
  T64_CHECK_STR(profile->save_policy,
                "save-explicit-eeprom4k16k-sram32k-busy6ms-v1");
  T64_CHECK_STR(profile->event_order_policy, "tick-sequence-fifo-v1");
  T64_CHECK_STR(profile->scheduler_version, "tick-sequence-fifo-v1");

  T64_CHECK_INT(t64_profile_hash(profile, digest), T64_STATUS_OK);
  T64_CHECK(memcmp(digest, ai_hash, T64_DIGEST_SIZE) != 0);
  T64_CHECK_INT(t64_profile_serialize(profile, serialized, sizeof serialized,
                                      &length),
                T64_STATUS_OK);
  /* AI serialization is 1319 bytes with a 55-character id; the family adds
   * the two labels (4-byte length word each) and has a shorter id. */
  T64_CHECK_INT(length, 1319u - 55u + si_id_lengths[index] + 4u + 21u + 4u +
                            44u);
  external_digest(serialized, length, reference);
  T64_CHECK(memcmp(digest, reference, sizeof digest) == 0);

  T64_CHECK_INT(t64_machine_create(&host, profile, &machine), T64_STATUS_OK);
  T64_CHECK(machine != NULL);
  if (machine != NULL) {
    T64_CHECK_INT(machine->profile.save_kind, si_save_kinds[index]);
    T64_CHECK(read_openipl3_asset(asset));
    T64_CHECK_INT(t64_machine_admit_rom(machine, asset, sizeof asset),
                  T64_STATUS_OK);
    T64_CHECK(t64_has_si_profile(machine));
    t64_machine_destroy(machine);
  }
  T64_CHECK_INT(state.outstanding, 0u);
  T64_CHECK_INT(state.release_errors, 0u);
}

static void mutate_text_and_boot(const t64_profile_descriptor *profile) {
  uint8_t original_hash[T64_DIGEST_SIZE];
  uint8_t original_bytes[SERIALIZED_CAPACITY];
  size_t original_length = 0u;
  size_t field;
  char label[160];

  T64_CHECK_INT(t64_profile_hash(profile, original_hash), T64_STATUS_OK);
  T64_CHECK_INT(t64_profile_serialize(profile, original_bytes,
                                      sizeof original_bytes, &original_length),
                T64_STATUS_OK);
  for (field = 0u; field < FAMILY_TEXT_FIELD_COUNT + FAMILY_BOOT_FIELD_COUNT;
       field++) {
    t64_profile_descriptor changed = *profile;
    const bool is_text = field < FAMILY_TEXT_FIELD_COUNT;
    const text_field *entry =
        is_text ? &family_text_fields[field]
                : &family_boot_fields[field - FAMILY_TEXT_FIELD_COUNT];
    uint8_t *bytes = (uint8_t *)&changed;

    bytes[entry->offset] ^= UINT8_C(1);
    (void)snprintf(label, sizeof label, "%s bit flip of %s",
                   profile->profile_id, entry->name);
    expect_pinned_at_create(label, profile, &changed, original_hash,
                            original_bytes, original_length);
  }
  for (field = 0u; field < FAMILY_TEXT_FIELD_COUNT; field++) {
    unsigned kind;

    for (kind = 0u; kind < 2u; kind++) {
      t64_profile_descriptor changed = *profile;
      char *text = (char *)&changed + family_text_fields[field].offset;

      /* kind 0: empty text; kind 1: no terminator within the capacity. */
      (void)memset(text, kind == 0u ? 0 : 'x', T64_PROFILE_TEXT_CAPACITY);
      (void)snprintf(label, sizeof label, "%s %s text of %s",
                     profile->profile_id, kind == 0u ? "empty" : "unterminated",
                     family_text_fields[field].name);
      expect_refused_everywhere(label, &changed);
    }
  }
}

static void mutate_numeric(const t64_profile_descriptor *profile,
                           size_t index) {
  uint8_t original_hash[T64_DIGEST_SIZE];
  uint8_t original_bytes[SERIALIZED_CAPACITY];
  size_t original_length = 0u;
  t64_profile_descriptor changed;
  char label[160];
  size_t i;
  static const uint32_t bad_save_kinds[] = {4u, 5u, 0xffffffffu};
  static const uint32_t bad_peripheral_flags[] = {2u, 3u, 0x80000001u};
  static const uint32_t bad_counts[] = {0u, 2u, 3u};

  T64_CHECK_INT(t64_profile_hash(profile, original_hash), T64_STATUS_OK);
  T64_CHECK_INT(t64_profile_serialize(profile, original_bytes,
                                      sizeof original_bytes, &original_length),
                T64_STATUS_OK);

  /* In range: another valid save kind, no controller. The id is hash-pinned
   * at create, so the changed descriptor still refuses to create. */
  changed = *profile;
  changed.save_kind = (si_save_kinds[index] + 1u) % 4u;
  (void)snprintf(label, sizeof label, "%s in-range save_kind",
                 profile->profile_id);
  expect_pinned_at_create(label, profile, &changed, original_hash,
                          original_bytes, original_length);
  changed = *profile;
  changed.peripheral_flags = 0u;
  (void)snprintf(label, sizeof label, "%s peripheral_flags 0",
                 profile->profile_id);
  expect_pinned_at_create(label, profile, &changed, original_hash,
                          original_bytes, original_length);

  /* Out of range or unsupported: refused at hash, serialize and create. */
  for (i = 0u; i < sizeof bad_save_kinds / sizeof bad_save_kinds[0]; i++) {
    changed = *profile;
    changed.save_kind = bad_save_kinds[i];
    (void)snprintf(label, sizeof label, "%s save_kind %u",
                   profile->profile_id, (unsigned)bad_save_kinds[i]);
    expect_refused_everywhere(label, &changed);
  }
  for (i = 0u;
       i < sizeof bad_peripheral_flags / sizeof bad_peripheral_flags[0]; i++) {
    changed = *profile;
    changed.peripheral_flags = bad_peripheral_flags[i];
    (void)snprintf(label, sizeof label, "%s peripheral_flags %u",
                   profile->profile_id, (unsigned)bad_peripheral_flags[i]);
    expect_refused_everywhere(label, &changed);
  }
  for (i = 0u; i < sizeof bad_counts / sizeof bad_counts[0]; i++) {
    changed = *profile;
    changed.synchronization_quantum = bad_counts[i];
    (void)snprintf(label, sizeof label, "%s quantum %u", profile->profile_id,
                   (unsigned)bad_counts[i]);
    expect_refused_everywhere(label, &changed);
    changed = *profile;
    changed.cpu_rsp_ratio_numerator = bad_counts[i];
    (void)snprintf(label, sizeof label, "%s ratio %u:1", profile->profile_id,
                   (unsigned)bad_counts[i]);
    expect_refused_everywhere(label, &changed);
    changed = *profile;
    changed.cpu_rsp_ratio_denominator = bad_counts[i];
    (void)snprintf(label, sizeof label, "%s ratio 1:%u", profile->profile_id,
                   (unsigned)bad_counts[i]);
    expect_refused_everywhere(label, &changed);
  }
}

/* A descriptor-12 copy under an unregistered id must not create a machine:
 * the family tuple selects device storage by registered id. */
static void check_unregistered_family_id(void) {
  const t64_profile_descriptor *profile = lookup_profile(SI_STEM);
  t64_profile_descriptor changed;
  counting_state state = {0u, 0u, 0u};
  const t64_host_allocator host = {counting_allocate, counting_release,
                                   &state};
  t64_machine *machine = NULL;
  uint8_t digest[T64_DIGEST_SIZE];

  if (profile == NULL) return;
  changed = *profile;
  (void)memset(changed.profile_id, 0, sizeof changed.profile_id);
  (void)memcpy(changed.profile_id, "test-only-unregistered-family",
               sizeof "test-only-unregistered-family");
  T64_CHECK_INT(t64_profile_hash(&changed, digest), T64_STATUS_OK);
  expect_status("unregistered descriptor-12 id create",
                t64_machine_create(&host, &changed, &machine),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK(machine == NULL);
  T64_CHECK_INT(state.allocations, 0u);
  if (machine != NULL) t64_machine_destroy(machine);
}

/* Older descriptor versions keep their permissive hashing but refuse a
 * non-zero save kind or peripheral set at create (D-01). */
static void check_legacy_refuses_numeric_at_create(void) {
  const t64_profile_descriptor *profile = lookup_profile("ntsc-cart-4m-skeleton");
  counting_state state = {0u, 0u, 0u};
  const t64_host_allocator host = {counting_allocate, counting_release,
                                   &state};
  t64_profile_descriptor changed;
  uint8_t digest[T64_DIGEST_SIZE];
  uint8_t baseline[T64_DIGEST_SIZE];
  t64_machine *machine = NULL;
  size_t i;
  static const uint32_t save_kinds[] = {1u, 2u, 3u, 7u};

  if (profile == NULL) return;
  changed = *profile;
  (void)memset(changed.profile_id, 0, sizeof changed.profile_id);
  (void)memcpy(changed.profile_id, "test-only-legacy-numeric",
               sizeof "test-only-legacy-numeric");
  T64_CHECK_INT(t64_profile_hash(&changed, baseline), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_create(&host, &changed, &machine), T64_STATUS_OK);
  T64_CHECK(machine != NULL);
  if (machine != NULL) t64_machine_destroy(machine);
  T64_CHECK_INT(state.outstanding, 0u);

  for (i = 0u; i < sizeof save_kinds / sizeof save_kinds[0]; i++) {
    const size_t before = state.allocations;

    machine = NULL;
    changed.save_kind = save_kinds[i];
    T64_CHECK_INT(t64_profile_hash(&changed, digest), T64_STATUS_OK);
    T64_CHECK(memcmp(digest, baseline, sizeof digest) != 0);
    expect_status("legacy create with save_kind",
                  t64_machine_create(&host, &changed, &machine),
                  T64_STATUS_INVALID_ARGUMENT);
    T64_CHECK(machine == NULL);
    T64_CHECK_INT(state.allocations, before);
    if (machine != NULL) t64_machine_destroy(machine);
  }
  changed.save_kind = 0u;
  changed.peripheral_flags = T64_PERIPHERAL_STANDARD_CONTROLLER_PORT0;
  T64_CHECK_INT(t64_profile_hash(&changed, digest), T64_STATUS_OK);
  {
    const size_t before = state.allocations;

    machine = NULL;
    expect_status("legacy create with peripheral_flags",
                  t64_machine_create(&host, &changed, &machine),
                  T64_STATUS_INVALID_ARGUMENT);
    T64_CHECK(machine == NULL);
    T64_CHECK_INT(state.allocations, before);
    if (machine != NULL) t64_machine_destroy(machine);
  }
  T64_CHECK_INT(state.outstanding, 0u);
  T64_CHECK_INT(state.release_errors, 0u);
}

static void case_si_profile_identity(void) {
  const t64_profile_descriptor *ai = lookup_profile(AI_PROFILE_ID);
  uint8_t ai_hash[T64_DIGEST_SIZE];
  uint8_t hashes[SI_FAMILY_COUNT][T64_DIGEST_SIZE];
  size_t i;
  size_t j;

  if (ai == NULL) return;
  T64_CHECK_INT(t64_profile_hash(ai, ai_hash), T64_STATUS_OK);
  check_legacy_pins();
  for (i = 0u; i < SI_FAMILY_COUNT; i++) {
    const t64_profile_descriptor *profile = lookup_profile(si_ids[i]);

    check_family_identity(i, ai_hash);
    if (profile == NULL) {
      (void)memset(hashes[i], 0, T64_DIGEST_SIZE);
      continue;
    }
    T64_CHECK_INT(t64_profile_hash(profile, hashes[i]), T64_STATUS_OK);
    mutate_text_and_boot(profile);
    mutate_numeric(profile, i);
  }
  for (i = 0u; i < SI_FAMILY_COUNT; i++) {
    for (j = i + 1u; j < SI_FAMILY_COUNT; j++) {
      T64_CHECK(memcmp(hashes[i], hashes[j], T64_DIGEST_SIZE) != 0);
    }
  }
  check_unregistered_family_id();
  check_legacy_refuses_numeric_at_create();
}

/* Offset of the quantum word in the serialized descriptor, derived from the
 * documented layout (not from the production writer). */
static size_t expected_quantum_offset(const t64_profile_descriptor *profile) {
  size_t offset = 8u + 4u;  /* domain, serialization version */

  offset += 4u + strlen(profile->profile_id);
  offset += 4u + strlen(profile->console_target);
  offset += 4u + strlen(profile->console_qualification);
  offset += 4u * 3u;  /* tv standard, region, media kind */
  offset += 4u * 2u;  /* rdram bytes, expansion bytes */
  offset += 4u * 2u;  /* peripheral flags, save kind */
  offset += 4u + strlen(profile->boot_route);
  offset += 4u + strlen(profile->boot_status);
  offset += 4u + strlen(profile->guest_tick_unit);
  offset += 4u * 2u;  /* cpu:rsp ratio */
  return offset;
}

static void case_quantum_identity(void) {
  size_t index;
  const t64_profile_descriptor *ai = lookup_profile(AI_PROFILE_ID);

  /* Descriptor 12 refuses any other quantum or ratio at hash, serialize and
   * create; the canonical Q = 1 is accepted. */
  for (index = 0u; index < SI_FAMILY_COUNT; index++) {
    const t64_profile_descriptor *profile = lookup_profile(si_ids[index]);
    counting_state state = {0u, 0u, 0u};
    const t64_host_allocator host = {counting_allocate, counting_release,
                                     &state};
    t64_profile_descriptor changed;
    t64_machine *machine = NULL;
    char label[160];
    size_t k;
    static const uint32_t bad_quanta[] = {0u, 2u};
    static const uint32_t bad_ratios[][2] = {{2u, 1u}, {1u, 2u}, {0u, 1u},
                                             {1u, 0u}, {0u, 0u}};

    if (profile == NULL) continue;
    T64_CHECK_INT(profile->synchronization_quantum, 1u);
    T64_CHECK_INT(t64_machine_create(&host, profile, &machine), T64_STATUS_OK);
    T64_CHECK(machine != NULL);
    if (machine != NULL) t64_machine_destroy(machine);
    T64_CHECK_INT(state.outstanding, 0u);
    for (k = 0u; k < sizeof bad_quanta / sizeof bad_quanta[0]; k++) {
      changed = *profile;
      changed.synchronization_quantum = bad_quanta[k];
      (void)snprintf(label, sizeof label, "%s quantum %u", si_ids[index],
                     (unsigned)bad_quanta[k]);
      expect_refused_everywhere(label, &changed);
    }
    for (k = 0u; k < sizeof bad_ratios / sizeof bad_ratios[0]; k++) {
      changed = *profile;
      changed.cpu_rsp_ratio_numerator = bad_ratios[k][0];
      changed.cpu_rsp_ratio_denominator = bad_ratios[k][1];
      (void)snprintf(label, sizeof label, "%s ratio %u:%u", si_ids[index],
                     (unsigned)bad_ratios[k][0], (unsigned)bad_ratios[k][1]);
      expect_refused_everywhere(label, &changed);
    }
  }

  /* Hash sensitivity: on a descriptor 11 copy only the quantum changes, the
   * serialized bytes differ in exactly one 32-bit window, and the externally
   * computed SHA-256 of each byte string equals the core hash. */
  if (ai != NULL) {
    t64_profile_descriptor changed = *ai;
    uint8_t first[SERIALIZED_CAPACITY];
    uint8_t second[SERIALIZED_CAPACITY];
    uint8_t first_hash[T64_DIGEST_SIZE];
    uint8_t second_hash[T64_DIGEST_SIZE];
    uint8_t reference[T64_DIGEST_SIZE];
    size_t first_length = 0u;
    size_t second_length = 0u;
    size_t differing = 0u;
    size_t window = 0u;
    size_t i;

    changed.synchronization_quantum = 2u;
    T64_CHECK_INT(t64_profile_hash(ai, first_hash), T64_STATUS_OK);
    T64_CHECK_INT(t64_profile_hash(&changed, second_hash), T64_STATUS_OK);
    T64_CHECK(memcmp(first_hash, second_hash, T64_DIGEST_SIZE) != 0);
    T64_CHECK_INT(t64_profile_serialize(ai, first, sizeof first,
                                        &first_length),
                  T64_STATUS_OK);
    T64_CHECK_INT(t64_profile_serialize(&changed, second, sizeof second,
                                        &second_length),
                  T64_STATUS_OK);
    T64_CHECK_INT(first_length, second_length);
    for (i = 0u; i < first_length && i < second_length; i++) {
      if (first[i] != second[i]) {
        if (differing == 0u) window = i & ~(size_t)3u;
        T64_CHECK_INT(i & ~(size_t)3u, window);
        differing++;
      }
    }
    T64_CHECK_INT(differing, 1u);
    window = expected_quantum_offset(ai);
    T64_CHECK_INT(read_u32_be(first + window), 1u);
    T64_CHECK_INT(read_u32_be(second + window), 2u);
    external_digest(first, first_length, reference);
    T64_CHECK(memcmp(reference, first_hash, T64_DIGEST_SIZE) == 0);
    external_digest(second, second_length, reference);
    T64_CHECK(memcmp(reference, second_hash, T64_DIGEST_SIZE) == 0);
  }
}

static void case_family_devices(void) {
  size_t index;

  /* ISViewer: every family id inherits the admitted ISViewer window. */
  for (index = 0u; index < SI_FAMILY_COUNT; index++) {
    const t64_profile_descriptor *profile = lookup_profile(si_ids[index]);
    counting_state state = {0u, 0u, 0u};
    const t64_host_allocator host = {counting_allocate, counting_release,
                                     &state};
    t64_machine *machine = NULL;
    uint8_t asset[IPL3_ASSET_BYTES];
    uint32_t value = 0u;

    if (profile == NULL) continue;
    T64_CHECK_INT(t64_machine_create(&host, profile, &machine), T64_STATUS_OK);
    T64_CHECK(machine != NULL);
    if (machine == NULL) continue;
    T64_CHECK(read_openipl3_asset(asset));
    T64_CHECK_INT(t64_machine_admit_rom(machine, asset, sizeof asset),
                  T64_STATUS_OK);
    expect_status("family ISViewer window active",
                  t64_isviewer_active(machine) ? T64_STATUS_OK
                                               : T64_STATUS_BUS_DEVICE_REFUSED,
                  T64_STATUS_OK);
    T64_CHECK_INT(t64_machine_bus_write(machine, T64_ISVIEWER_BASE, 4u,
                                        UINT32_C(0x12345678)),
                  T64_STATUS_OK);
    T64_CHECK_INT(t64_machine_bus_read(machine, T64_ISVIEWER_BASE, 4u, &value),
                  T64_STATUS_OK);
    T64_CHECK_INT(value, UINT32_C(0x12345678));
    t64_machine_destroy(machine);
    T64_CHECK_INT(state.outstanding, 0u);
  }
}

int main(int argc, char **argv) {
  static const char *const cases[] = {"predicate-matrix", "si-profile-identity",
                                      "quantum-identity", "family-devices"};

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    size_t index;

    for (index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
      printf("%s\n", cases[index]);
    }
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    if (strcmp(argv[2], "predicate-matrix") == 0) {
      case_predicate_matrix();
      return T64_TEST_FINISH("unit.core.profile.predicate-matrix");
    }
    if (strcmp(argv[2], "si-profile-identity") == 0) {
      case_si_profile_identity();
      return T64_TEST_FINISH("unit.core.profile.v12-identity");
    }
    if (strcmp(argv[2], "quantum-identity") == 0) {
      case_quantum_identity();
      return T64_TEST_FINISH("unit.core.profile.quantum-identity");
    }
    if (strcmp(argv[2], "family-devices") == 0) {
      case_family_devices();
      return T64_TEST_FINISH("unit.core.profile.family-devices");
    }
    fprintf(stderr, "unknown case: %s\n", argv[2]);
    return 2;
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--case NAME]\n", argv[0]);
    return 2;
  }
  case_predicate_matrix();
  case_si_profile_identity();
  case_quantum_identity();
  case_family_devices();
  return T64_TEST_FINISH("unit.core.profile.si");
}
