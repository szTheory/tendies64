/*
 * Phase 10 composite-profile tests for the descriptor 13 family (plan 10-09).
 *
 * v26-identity: the four registered ids ntsc-cart-4m-openipl3-rdram-b-t1-si-v2
 *     (+ -e4k, -e16k, -s32k) carry the tuple 13/21/26, their save kind, their
 *     id length and a profile SHA-256 pinned below as a literal (one-way: the
 *     ids and these hashes feed every later pin). The serialization differs
 *     from the descriptor 12 sibling in exactly the id digit, the three
 *     version words and the boot status text. Every text, boot and numeric
 *     field is mutated through the public hash, serialize and create calls.
 *     Machine creation under an unregistered descriptor 13 id refuses before
 *     any allocation. SetZImage, SetPrimDepth and the four Z triangle opcodes
 *     are accepted on each id (RDP depth is active there).
 * v26-isviewer: the ISViewer window is active on the eight SI family ids
 *     (four si-v1, four si-v2) and absent on an id outside the family.
 * v26-predicate-matrix (plan 10-10): over all 25 registered ids (17 before
 *     Phase 9, four si-v1, four si-v2) the predicates t64_has_rdp_depth_profile,
 *     t64_has_si_profile, t64_has_si_v25_profile, t64_has_si_v26_profile and
 *     t64_has_ai_device match an expected table written below as literals:
 *     depth is active on the three descriptor 10 depth ids and the four
 *     descriptor 13 ids only, inactive on the descriptor 11 id and the four
 *     descriptor 12 ids (K1 preserved).
 *
 * Expected values are literals written here or computed from the documented
 * layout, never read back from the production writer alone.
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

#define IPL3_ASSET_BYTES 4096u
#define SERIALIZED_CAPACITY T64_PROFILE_SERIALIZED_MAX_BYTES
#define V1_STEM "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1"
#define V2_STEM "ntsc-cart-4m-openipl3-rdram-b-t1-si-v2"
#define OUTSIDE_FAMILY_ID "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9"
#define FAMILY_COUNT 4u

typedef struct {
  const char *id;
  const char *v1_id;
  size_t id_length;
  uint32_t save_kind;
  const char *hash_hex;
  size_t serialized_length;
} v2_row;

/* The profile hashes are the one-way identity pins of the family. */
static const v2_row rows[FAMILY_COUNT] = {
    {V2_STEM, V1_STEM, 38u, T64_SAVE_NONE,
     "3bc028bdb4c6f9622e20ecfff828a26245adc04ea69ddeeefdbb08d5b27dbc47",
     1358u},
    {V2_STEM "-e4k", V1_STEM "-e4k", 42u, T64_SAVE_EEPROM_4K,
     "0a4bfc455adc8b66c4666087c71ff5c0ce3b2f46ab6c83784a37d862956c1fe8",
     1362u},
    {V2_STEM "-e16k", V1_STEM "-e16k", 43u, T64_SAVE_EEPROM_16K,
     "34f767b0a9495d604719288515951c2e2ae726ef661f6a7c5fc378397718536c",
     1363u},
    {V2_STEM "-s32k", V1_STEM "-s32k", 43u, T64_SAVE_SRAM_32K,
     "9dfb90d8055b822a1767be1c5594d1b2d904f042ef6fe82f2e7a3243e5756578",
     1363u},
};

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

static const t64_host_allocator allocator =
    T64_HOST_ALLOCATOR_INIT(test_allocate, test_release, NULL);

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

static const t64_profile_descriptor *lookup_profile(const char *profile_id) {
  const t64_profile_descriptor *profile = NULL;

  T64_CHECK_INT(t64_profile_lookup(profile_id, &profile), T64_STATUS_OK);
  return profile;
}

/* Create the machine for an id and admit the cleared openIPL3 asset. */
static t64_machine *create_admitted(const char *profile_id) {
  const t64_profile_descriptor *profile = lookup_profile(profile_id);
  t64_machine *machine = NULL;
  uint8_t asset[IPL3_ASSET_BYTES];
  t64_status status;

  if (profile == NULL) return NULL;
  T64_CHECK_INT(t64_machine_create(
                    &(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(
                        &allocator, profile),
                    &machine),
                T64_STATUS_OK);
  if (machine == NULL) return NULL;
  if (!read_openipl3_asset(asset)) {
    T64_CHECK(false);
    t64_machine_destroy(machine);
    return NULL;
  }
  status = t64_machine_admit_rom(machine, asset, sizeof asset);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return NULL;
  }
  return machine;
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

static void expect_status(const char *label, t64_status got,
                          t64_status want) {
  if (got != want) {
    fprintf(stderr, "%s: got status %u want %u\n", label, (unsigned)got,
            (unsigned)want);
  }
  T64_CHECK_INT(got, want);
}

/* Hash, serialize and create must all refuse the descriptor with
 * INVALID_ARGUMENT, return no machine and leave every counter unchanged. */
static void expect_refused_everywhere(const char *label,
                                      const t64_profile_descriptor *profile) {
  counting_state state = {0u, 0u, 0u};
  const t64_host_allocator host =
      T64_HOST_ALLOCATOR_INIT(counting_allocate, counting_release, &state);
  uint8_t digest[T64_DIGEST_SIZE];
  uint8_t serialized[SERIALIZED_CAPACITY];
  size_t length = 0u;
  t64_machine *machine = NULL;
  char text[200];

  (void)snprintf(text, sizeof text, "%s hash", label);
  expect_status(text, t64_profile_hash(profile, digest),
                T64_STATUS_INVALID_ARGUMENT);
  (void)snprintf(text, sizeof text, "%s serialize", label);
  expect_status(text,
                t64_profile_serialize(profile, serialized, sizeof serialized,
                                      &length),
                T64_STATUS_INVALID_ARGUMENT);
  (void)snprintf(text, sizeof text, "%s create", label);
  expect_status(text, t64_machine_create(
                          &(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(
                              &host, profile),
                          &machine),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK(machine == NULL);
  T64_CHECK_INT(state.allocations, 0u);
  T64_CHECK_INT(state.outstanding, 0u);
  T64_CHECK_INT(state.release_errors, 0u);
  if (machine != NULL) t64_machine_destroy(machine);
}

/* The descriptor hashes and serializes, differs from the original, and
 * machine creation still refuses it before any allocation. */
static void expect_pinned_at_create(const char *label,
                                    const t64_profile_descriptor *changed,
                                    const uint8_t original_hash[T64_DIGEST_SIZE],
                                    const uint8_t *original_bytes,
                                    size_t original_length) {
  counting_state state = {0u, 0u, 0u};
  const t64_host_allocator host =
      T64_HOST_ALLOCATOR_INIT(counting_allocate, counting_release, &state);
  uint8_t digest[T64_DIGEST_SIZE];
  uint8_t serialized[SERIALIZED_CAPACITY];
  size_t length = 0u;
  t64_machine *machine = NULL;
  char text[200];

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
  expect_status(text, t64_machine_create(
                          &(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(
                              &host, changed),
                          &machine),
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

/* Every text field a descriptor 13 profile serializes, in layout order. */
static const text_field text_fields[] = {
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

#define TEXT_FIELD_COUNT (sizeof text_fields / sizeof text_fields[0])

/* Boot identity fields serialized from descriptor 2 onward. */
static const text_field boot_fields[] = {
    TEXT_FIELD(asset_sha256),
    TEXT_FIELD(payload_sha256),
    TEXT_FIELD(boot_policy_sha256),
    TEXT_FIELD(hle_seed_reset_version),
};

#define BOOT_FIELD_COUNT (sizeof boot_fields / sizeof boot_fields[0])

/* Mutate every text and boot field (bit flip: the hash and bytes change and
 * create refuses) and every text field (empty and unterminated: refused
 * everywhere). A descriptor falling through a version branch that stopped at
 * the previous descriptor would leave a field unhashed and fail here. */
static void mutate_text_and_boot(const t64_profile_descriptor *profile) {
  uint8_t original_hash[T64_DIGEST_SIZE];
  uint8_t original_bytes[SERIALIZED_CAPACITY];
  size_t original_length = 0u;
  size_t field;
  char label[200];

  T64_CHECK_INT(t64_profile_hash(profile, original_hash), T64_STATUS_OK);
  T64_CHECK_INT(t64_profile_serialize(profile, original_bytes,
                                      sizeof original_bytes, &original_length),
                T64_STATUS_OK);
  for (field = 0u; field < TEXT_FIELD_COUNT + BOOT_FIELD_COUNT; field++) {
    t64_profile_descriptor changed = *profile;
    const bool is_text = field < TEXT_FIELD_COUNT;
    const text_field *entry =
        is_text ? &text_fields[field] : &boot_fields[field - TEXT_FIELD_COUNT];
    uint8_t *bytes = (uint8_t *)&changed;

    bytes[entry->offset] ^= UINT8_C(1);
    (void)snprintf(label, sizeof label, "%s bit flip of %s",
                   profile->profile_id, entry->name);
    expect_pinned_at_create(label, &changed, original_hash, original_bytes,
                            original_length);
  }
  for (field = 0u; field < TEXT_FIELD_COUNT; field++) {
    unsigned kind;

    for (kind = 0u; kind < 2u; kind++) {
      t64_profile_descriptor changed = *profile;
      char *text = (char *)&changed + text_fields[field].offset;

      /* kind 0: empty text; kind 1: no terminator within the capacity. */
      (void)memset(text, kind == 0u ? 0 : 'x', T64_PROFILE_TEXT_CAPACITY);
      (void)snprintf(label, sizeof label, "%s %s text of %s",
                     profile->profile_id, kind == 0u ? "empty" : "unterminated",
                     text_fields[field].name);
      expect_refused_everywhere(label, &changed);
    }
  }
}

/* Numeric fields: another valid save kind, another version word, a missing
 * controller and every out-of-range value. Each changed descriptor is pinned
 * by the registered id and its canonical hash at create. */
static void mutate_numeric(const t64_profile_descriptor *profile,
                           size_t index) {
  uint8_t original_hash[T64_DIGEST_SIZE];
  uint8_t original_bytes[SERIALIZED_CAPACITY];
  size_t original_length = 0u;
  t64_profile_descriptor changed;
  char label[200];
  size_t i;
  static const uint32_t bad_save_kinds[] = {4u, 5u, 0xffffffffu};
  static const uint32_t bad_peripheral_flags[] = {2u, 3u, 0x80000001u};
  static const uint32_t bad_counts[] = {0u, 2u, 3u};
  static const uint32_t other_versions[] = {0u, 12u, 14u, 21u, 26u};

  T64_CHECK_INT(t64_profile_hash(profile, original_hash), T64_STATUS_OK);
  T64_CHECK_INT(t64_profile_serialize(profile, original_bytes,
                                      sizeof original_bytes, &original_length),
                T64_STATUS_OK);

  changed = *profile;
  changed.save_kind = (rows[index].save_kind + 1u) % 4u;
  (void)snprintf(label, sizeof label, "%s in-range save_kind",
                 profile->profile_id);
  expect_pinned_at_create(label, &changed, original_hash, original_bytes,
                          original_length);
  changed = *profile;
  changed.peripheral_flags = 0u;
  (void)snprintf(label, sizeof label, "%s peripheral_flags 0",
                 profile->profile_id);
  expect_pinned_at_create(label, &changed, original_hash, original_bytes,
                          original_length);
  /* Another version word: zero is refused by the field check; any other
   * value hashes to a different identity and the registered id refuses it at
   * create. */
  for (i = 0u; i < sizeof other_versions / sizeof other_versions[0]; i++) {
    unsigned word;

    for (word = 0u; word < 3u; word++) {
      static const char *const names[3] = {
          "descriptor_version", "machine_model_version",
          "state_serialization_version"};
      static const uint32_t own[3] = {13u, 21u, 26u};

      if (other_versions[i] == own[word]) continue;
      changed = *profile;
      if (word == 0u) changed.descriptor_version = other_versions[i];
      if (word == 1u) changed.machine_model_version = other_versions[i];
      if (word == 2u) changed.state_serialization_version = other_versions[i];
      (void)snprintf(label, sizeof label, "%s %s %u", profile->profile_id,
                     names[word], (unsigned)other_versions[i]);
      if (other_versions[i] == 0u) {
        expect_refused_everywhere(label, &changed);
      } else {
        expect_pinned_at_create(label, &changed, original_hash, original_bytes,
                                original_length);
      }
    }
  }
  for (i = 0u; i < sizeof bad_save_kinds / sizeof bad_save_kinds[0]; i++) {
    changed = *profile;
    changed.save_kind = bad_save_kinds[i];
    (void)snprintf(label, sizeof label, "%s save_kind %u", profile->profile_id,
                   (unsigned)bad_save_kinds[i]);
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

static uint32_t read_u32_be(const uint8_t *bytes) {
  return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
         ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

/* Remove the boot_status text (length word and bytes) from a serialization;
 * the offset is computed from the documented layout. Returns the new length
 * and the removed text length in text_out. */
static size_t strip_boot_status(const t64_profile_descriptor *profile,
                                const uint8_t *bytes, size_t length,
                                uint8_t *out, size_t *text_out) {
  size_t offset = 8u + 4u;  /* domain, serialization version */
  size_t text_length;

  offset += 4u + strlen(profile->profile_id);
  offset += 4u + strlen(profile->console_target);
  offset += 4u + strlen(profile->console_qualification);
  offset += 4u * 3u;  /* tv standard, region, media kind */
  offset += 4u * 2u;  /* rdram bytes, expansion bytes */
  offset += 4u * 2u;  /* peripheral flags, save kind */
  offset += 4u + strlen(profile->boot_route);
  text_length = read_u32_be(bytes + offset);
  *text_out = text_length;
  (void)memcpy(out, bytes, offset);
  (void)memcpy(out + offset, bytes + offset + 4u + text_length,
               length - offset - 4u - text_length);
  return length - 4u - text_length;
}

/* The serialization of each si-v2 id differs from its si-v1 sibling only in
 * the id digit, the boot status text and the low byte of the three version
 * words (12 to 13, 20 to 21, 25 to 26). */
static void check_differs_from_v1_only_by_identity(size_t index,
                                                   const uint8_t *v2_bytes,
                                                   size_t v2_length) {
  const t64_profile_descriptor *v1 = lookup_profile(rows[index].v1_id);
  const t64_profile_descriptor *v2 = lookup_profile(rows[index].id);
  uint8_t v1_bytes[SERIALIZED_CAPACITY];
  uint8_t v1_stripped[SERIALIZED_CAPACITY];
  uint8_t v2_stripped[SERIALIZED_CAPACITY];
  size_t v1_length = 0u;
  size_t v1_text = 0u;
  size_t v2_text = 0u;
  size_t v1_stripped_length;
  size_t v2_stripped_length;
  size_t differing = 0u;
  size_t i;

  if (v1 == NULL || v2 == NULL) return;
  T64_CHECK_INT(t64_profile_serialize(v1, v1_bytes, sizeof v1_bytes,
                                      &v1_length),
                T64_STATUS_OK);
  v1_stripped_length =
      strip_boot_status(v1, v1_bytes, v1_length, v1_stripped, &v1_text);
  v2_stripped_length =
      strip_boot_status(v2, v2_bytes, v2_length, v2_stripped, &v2_text);
  T64_CHECK_INT(v1_text, strlen("prepared-rdram-b-t1-rdp-vi-filter-v9-ai-v1"));
  T64_CHECK_INT(v2_text, strlen("prepared-rdram-b-t1-si-v2"));
  T64_CHECK_INT(v1_stripped_length, v2_stripped_length);
  if (v1_stripped_length != v2_stripped_length) return;
  for (i = 0u; i < v1_stripped_length; i++) {
    if (v1_stripped[i] != v2_stripped[i]) {
      differing++;
    }
  }
  /* id digit, descriptor low byte, model low byte, state low byte */
  T64_CHECK_INT(differing, 4u);
}

static void check_depth_commands(const char *profile_id);

static void check_family_row(size_t index) {
  const v2_row *row = &rows[index];
  const t64_profile_descriptor *profile = lookup_profile(row->id);
  counting_state state = {0u, 0u, 0u};
  const t64_host_allocator host =
      T64_HOST_ALLOCATOR_INIT(counting_allocate, counting_release, &state);
  uint8_t digest[T64_DIGEST_SIZE];
  uint8_t reference[T64_DIGEST_SIZE];
  uint8_t serialized[SERIALIZED_CAPACITY];
  uint8_t asset[IPL3_ASSET_BYTES];
  char hex[2u * T64_DIGEST_SIZE + 1u];
  size_t length = 0u;
  t64_machine *machine = NULL;

  if (profile == NULL) return;
  T64_CHECK_STR(profile->profile_id, row->id);
  T64_CHECK_INT(strlen(profile->profile_id), row->id_length);
  T64_CHECK(strlen(profile->profile_id) <= T64_PROFILE_TEXT_CAPACITY - 1u);
  T64_CHECK_INT(profile->descriptor_version, 13u);
  T64_CHECK_INT(profile->machine_model_version, 21u);
  T64_CHECK_INT(profile->state_serialization_version, 26u);
  T64_CHECK_INT(profile->save_kind, row->save_kind);
  T64_CHECK_INT(profile->peripheral_flags,
                T64_PERIPHERAL_STANDARD_CONTROLLER_PORT0);
  T64_CHECK_INT(profile->synchronization_quantum, 1u);
  T64_CHECK_INT(profile->cpu_rsp_ratio_numerator, 1u);
  T64_CHECK_INT(profile->cpu_rsp_ratio_denominator, 1u);
  T64_CHECK_STR(profile->si_policy, "si-hle-joybus-wire-v1");
  T64_CHECK_STR(profile->save_policy,
                "save-explicit-eeprom4k16k-sram32k-busy6ms-v1");
  T64_CHECK_STR(profile->boot_status, "prepared-rdram-b-t1-si-v2");
  T64_CHECK_STR(profile->rdp_depth_policy,
                "z-q16-floor8192-cfloor-p15s3-dzmax-eq-icvg-xlu-decal-h2-v1");

  T64_CHECK_INT(t64_profile_hash(profile, digest), T64_STATUS_OK);
  hex_of(digest, hex);
  printf("v26-identity: id=%s hash=%s\n", row->id, hex);
  T64_CHECK_STR(hex, row->hash_hex);
  T64_CHECK_INT(t64_profile_serialize(profile, serialized, sizeof serialized,
                                      &length),
                T64_STATUS_OK);
  printf("v26-identity: id=%s serialized_length=%zu\n", row->id, length);
  T64_CHECK_INT(length, row->serialized_length);
  external_digest(serialized, length, reference);
  T64_CHECK(memcmp(digest, reference, sizeof digest) == 0);
  check_differs_from_v1_only_by_identity(index, serialized, length);

  T64_CHECK_INT(t64_machine_create(
                    &(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(
                        &host, profile),
                    &machine), T64_STATUS_OK);
  T64_CHECK(machine != NULL);
  if (machine != NULL) {
    T64_CHECK_INT(machine->profile.save_kind, row->save_kind);
    T64_CHECK(read_openipl3_asset(asset));
    T64_CHECK_INT(t64_machine_admit_rom(machine, asset, sizeof asset),
                  T64_STATUS_OK);
    T64_CHECK(t64_has_si_v26_profile(machine));
    T64_CHECK(!t64_has_si_v25_profile(machine));
    T64_CHECK(t64_has_si_profile(machine));
    T64_CHECK(!t64_has_ai_profile(machine));
    T64_CHECK(t64_has_ai_device(machine));
    T64_CHECK(t64_has_vi_filter_v9_profile(machine));
    T64_CHECK(t64_has_rdp_blender_profile(machine));
    T64_CHECK(machine->rdp_memory_coverage != NULL);
    T64_CHECK(machine->rdp_depth_hidden != NULL);
    T64_CHECK(machine->video_packets != NULL);
    T64_CHECK(machine->audio_packets != NULL);
    T64_CHECK(t64_has_rdp_depth_profile(machine));
    t64_machine_destroy(machine);
  }
  T64_CHECK_INT(state.outstanding, 0u);
  T64_CHECK_INT(state.release_errors, 0u);
  check_depth_commands(row->id);
}

/* A descriptor 13 copy under an unregistered id, and the registered ids under
 * the wrong descriptor, must not create a machine: ring storage and the depth
 * sidecar are selected by the registered id. */
static void check_unregistered_ids(void) {
  const t64_profile_descriptor *v2 = lookup_profile(V2_STEM);
  const t64_profile_descriptor *v1 = lookup_profile(V1_STEM);
  static const char *const names[] = {
      "test-only-unregistered-family",
      V1_STEM,
      V2_STEM "-e8k",
      V2_STEM "-",
      "ntsc-cart-4m-openipl3-rdram-b-t1-si-v3",
  };
  size_t i;

  if (v2 == NULL || v1 == NULL) return;
  for (i = 0u; i < sizeof names / sizeof names[0]; i++) {
    t64_profile_descriptor changed = *v2;
    counting_state state = {0u, 0u, 0u};
    const t64_host_allocator host =
        T64_HOST_ALLOCATOR_INIT(counting_allocate, counting_release, &state);
    t64_machine *machine = NULL;
    uint8_t digest[T64_DIGEST_SIZE];

    (void)memset(changed.profile_id, 0, sizeof changed.profile_id);
    (void)memcpy(changed.profile_id, names[i], strlen(names[i]) + 1u);
    T64_CHECK_INT(t64_profile_hash(&changed, digest), T64_STATUS_OK);
    expect_status(names[i], t64_machine_create(
                                &(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(
                                    &host, &changed),
                                &machine),
                  T64_STATUS_INVALID_ARGUMENT);
    T64_CHECK(machine == NULL);
    T64_CHECK_INT(state.allocations, 0u);
    T64_CHECK_INT(state.outstanding, 0u);
    if (machine != NULL) t64_machine_destroy(machine);
  }
  /* The si-v1 body under the si-v2 id with descriptor 12, and the si-v2 body
   * under the si-v1 id with descriptor 13, are not the registered
   * descriptors. */
  {
    t64_profile_descriptor changed = *v1;
    counting_state state = {0u, 0u, 0u};
    const t64_host_allocator host =
        T64_HOST_ALLOCATOR_INIT(counting_allocate, counting_release, &state);
    t64_machine *machine = NULL;

    (void)memset(changed.profile_id, 0, sizeof changed.profile_id);
    (void)memcpy(changed.profile_id, V2_STEM, sizeof V2_STEM);
    expect_status("si-v1 body under the si-v2 id",
                  t64_machine_create(
                      &(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(
                          &host, &changed),
                      &machine),
                  T64_STATUS_INVALID_ARGUMENT);
    T64_CHECK(machine == NULL);
    T64_CHECK_INT(state.allocations, 0u);
  }
  {
    t64_profile_descriptor changed = *v2;
    counting_state state = {0u, 0u, 0u};
    const t64_host_allocator host =
        T64_HOST_ALLOCATOR_INIT(counting_allocate, counting_release, &state);
    t64_machine *machine = NULL;

    (void)memset(changed.profile_id, 0, sizeof changed.profile_id);
    (void)memcpy(changed.profile_id, V1_STEM, sizeof V1_STEM);
    expect_status("si-v2 body under the si-v1 id",
                  t64_machine_create(
                      &(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(
                          &host, &changed),
                      &machine),
                  T64_STATUS_INVALID_ARGUMENT);
    T64_CHECK(machine == NULL);
    T64_CHECK_INT(state.allocations, 0u);
  }
}

/* ---- RDP depth commands on the new ids ---------------------------------- */

#define COMMAND_ADDRESS UINT32_C(0x100)
#define DP_START UINT32_C(0x04100000)
#define DP_END (DP_START + UINT32_C(4))
#define FRAMEBUFFER_ADDRESS UINT32_C(0x5000)
#define DEPTH_IMAGE_ADDRESS UINT32_C(0x6000)
#define TEXTURE_ADDRESS UINT32_C(0x4000)

static void write_be16(uint8_t *bytes, uint16_t value) {
  bytes[0] = (uint8_t)(value >> 8);
  bytes[1] = (uint8_t)value;
}

static void write_be64(uint8_t *bytes, uint64_t value) {
  uint32_t i;

  for (i = 0u; i < 8u; i++) bytes[i] = (uint8_t)(value >> (56u - i * 8u));
}

/* Same event pump as core/tests/test_rdp_depth.c dispatch_all. */
static void dispatch_all(t64_machine *machine) {
  size_t step;

  for (step = 0u; step < 4096u && machine->queue_count != 0u; step++) {
    t64_event event = machine->events[machine->queue_head];
    machine->queue_head = (machine->queue_head + 1u) % T64_EVENT_QUEUE_CAPACITY;
    machine->queue_count--;
    machine->tick = event.tick;
    T64_CHECK(event.handler != NULL);
    if (event.handler != NULL) {
      T64_CHECK_INT(event.handler(machine, &event), T64_STATUS_OK);
    }
  }
  T64_CHECK_INT(machine->queue_count, 0u);
}

static void run_commands(t64_machine *machine, const uint64_t *words,
                         size_t count) {
  size_t i;
  uint32_t end = COMMAND_ADDRESS + (uint32_t)(count * 8u);

  machine->queue_head = 0u;
  machine->queue_count = 0u;
  for (i = 0u; i < count; i++) {
    write_be64(machine->ram + COMMAND_ADDRESS + i * 8u, words[i]);
  }
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_START, 4u, COMMAND_ADDRESS),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_END, 4u, end),
                T64_STATUS_OK);
  dispatch_all(machine);
}

/* Command words of core/tests/test_rdp_depth.c
 * case_all_z_triangle_opcodes_reach_pixel (that file is oracle-bound and is
 * never edited): the RGBA16 color image at 0x5000 of width 8, the scissor, the
 * primitive Z mode, SetZImage at 0x6000, SetPrimDepth Z=0x1357 dZ=9, the
 * primitive color and the combiner that selects it. */
static const uint64_t head_words[] = {
    UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
    UINT64_C(0x2f00000000000034), UINT64_C(0x3e00000000006000),
    UINT64_C(0x2e00000013570009), UINT64_C(0x3a00000000ff00ff),
    UINT64_C(0xfc00000000018600),
};
static const uint64_t texture_setup[] = {
    UINT64_C(0x3d10000700004000), UINT64_C(0x3510040000080200),
    UINT64_C(0x320000000001c01c), UINT64_C(0x340000000001c01c),
};
static const uint64_t edge_words[] = {
    UINT64_C(0x00010000fffe8000), UINT64_C(0x0001000000000000),
    UINT64_C(0x0004000000018000),
};

/* SetZImage and SetPrimDepth are accepted and latch their operands. */
static void check_depth_state_commands(const char *profile_id) {
  t64_machine *machine = create_admitted(profile_id);

  if (machine == NULL) return;
  run_commands(machine, head_words, 3u);
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK(!machine->rdp.depth_image_valid);
  T64_CHECK(!machine->rdp.primitive_depth_valid);
  run_commands(machine, head_words + 3u, 1u);
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK((machine->rdp.dpc_status & T64_RDP_STATUS_ERROR) == 0u);
  T64_CHECK(machine->rdp.depth_image_valid);
  T64_CHECK_INT(machine->rdp.depth_image_address, DEPTH_IMAGE_ADDRESS);
  run_commands(machine, head_words + 4u, 1u);
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK(machine->rdp.primitive_depth_valid);
  T64_CHECK_INT(machine->rdp.primitive_z, 0x1357u);
  T64_CHECK_INT(machine->rdp.primitive_dz, 9u);
  t64_machine_destroy(machine);
}

/* One Z triangle opcode runs through to its full sync on one id: no refusal,
 * and the depth word at pixel (1,2) and its hidden low bits are the literals
 * of the depth profile test (0x09a8 and 0x83; 0x0004 and 0x80 for opcode 0x0f
 * with Z base 8.0). */
static void check_z_triangle(const char *profile_id, uint32_t opcode) {
  const bool textured = opcode == 0x0bu || opcode == 0x0fu;
  const bool shaded = opcode == 0x0du || opcode == 0x0fu;
  uint64_t commands[40];
  size_t count = 0u;
  size_t triangle_at;
  uint32_t i;
  t64_machine *machine = create_admitted(profile_id);
  const uint32_t depth_address = DEPTH_IMAGE_ADDRESS + (2u * 8u + 1u) * 2u;

  if (machine == NULL) return;
  (void)memcpy(commands + count, head_words, sizeof head_words);
  count += sizeof head_words / sizeof head_words[0];
  if (textured) {
    (void)memcpy(commands + count, texture_setup, sizeof texture_setup);
    count += sizeof texture_setup / sizeof texture_setup[0];
    for (i = 0u; i < 8u * 8u; i++) {
      write_be16(machine->ram + TEXTURE_ADDRESS + i * 2u,
                 opcode == 0x0fu && i != 0u ? 0u : UINT16_C(0xffff));
    }
  }
  triangle_at = count;
  commands[count++] = (UINT64_C(0x0000000014000c0004) |
                       ((uint64_t)opcode << 56));
  (void)memcpy(commands + count, edge_words, sizeof edge_words);
  count += sizeof edge_words / sizeof edge_words[0];
  if (shaded && textured) {
    commands[count++] = UINT64_C(0x0080004000c000ff);
    for (i = 0u; i < 7u; i++) commands[count++] = 0u;
  }
  if (textured) {
    commands[count++] = UINT64_C(0x0000000000010000);
    for (i = 0u; i < 7u; i++) commands[count++] = 0u;
  }
  if (shaded && !textured) {
    for (i = 0u; i < 8u; i++) commands[count++] = 0u;
  }
  commands[count++] = UINT64_C(0x0000000000000000);
  commands[count++] = UINT64_C(0x0000000000000000);
  if (opcode == 0x0fu) {
    commands[6] = UINT64_C(0xfc12180000000000);
    commands[2] = UINT64_C(0x2f00000000000030);
    commands[count - 2u] = UINT64_C(0x0008000000000000);
  }
  commands[count++] = UINT64_C(0x2900000000000000);
  T64_CHECK_INT((commands[triangle_at] >> 56) & UINT64_C(0x3f), opcode);
  (void)memset(machine->ram + FRAMEBUFFER_ADDRESS, 0xa5, 8u * 8u * 2u);
  for (i = 0u; i < 8u * 8u; i++) {
    write_be16(machine->ram + DEPTH_IMAGE_ADDRESS + i * 2u, UINT16_C(0xfffc));
  }
  run_commands(machine, commands, count);
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT(machine->rdp.last_opcode_field, UINT32_C(0x29));
  T64_CHECK((machine->rdp.dpc_status & T64_RDP_STATUS_ERROR) == 0u);
  T64_CHECK(machine->rdp.fullsync_count > 0u);
  T64_CHECK_INT((machine->ram[depth_address] << 8) |
                    machine->ram[depth_address + 1u],
                opcode == 0x0fu ? 0x0004u : 0x09a8u);
  T64_CHECK_INT(machine->rdp_depth_hidden[depth_address / 2u],
                opcode == 0x0fu ? 0x80u : 0x83u);
  t64_machine_destroy(machine);
}

static void check_depth_commands(const char *profile_id) {
  static const uint32_t opcodes[] = {0x09u, 0x0bu, 0x0du, 0x0fu};
  size_t i;

  check_depth_state_commands(profile_id);
  for (i = 0u; i < sizeof opcodes / sizeof opcodes[0]; i++) {
    check_z_triangle(profile_id, opcodes[i]);
  }
}

static void case_v26_identity(void) {
  size_t i;
  size_t j;
  uint8_t hashes[FAMILY_COUNT][T64_DIGEST_SIZE];

  for (i = 0u; i < FAMILY_COUNT; i++) {
    const t64_profile_descriptor *profile = lookup_profile(rows[i].id);

    check_family_row(i);
    if (profile == NULL) {
      (void)memset(hashes[i], 0, T64_DIGEST_SIZE);
      continue;
    }
    T64_CHECK_INT(t64_profile_hash(profile, hashes[i]), T64_STATUS_OK);
    mutate_text_and_boot(profile);
    mutate_numeric(profile, i);
  }
  for (i = 0u; i < FAMILY_COUNT; i++) {
    for (j = i + 1u; j < FAMILY_COUNT; j++) {
      T64_CHECK(memcmp(hashes[i], hashes[j], T64_DIGEST_SIZE) != 0);
    }
  }
  check_unregistered_ids();
}

/* ---- ISViewer activity on the SI family --------------------------------- */

#define ISVIEWER_ID_COUNT 8u

static const char *const isviewer_accept_ids[ISVIEWER_ID_COUNT] = {
    V1_STEM,           V1_STEM "-e4k",    V1_STEM "-e16k",  V1_STEM "-s32k",
    V2_STEM,           V2_STEM "-e4k",    V2_STEM "-e16k",  V2_STEM "-s32k"};

static unsigned accepted_ids;
static unsigned refused_ids;

static void check_isviewer_accepts(const char *profile_id) {
  t64_machine *machine = create_admitted(profile_id);
  uint32_t value = 0u;
  t64_status status;
  bool active;

  if (machine == NULL) return;
  active = t64_isviewer_active(machine);
  if (!active) {
    fprintf(stderr, "v26-isviewer: ISViewer inactive on %s\n", profile_id);
  }
  T64_CHECK(active);
  status = t64_machine_bus_write(machine, T64_ISVIEWER_BASE + 0x24u, 4u,
                                 UINT32_C(0xe5f60718));
  if (status != T64_STATUS_OK) {
    fprintf(stderr, "v26-isviewer: word write on %s returned status %u\n",
            profile_id, (unsigned)status);
  }
  T64_CHECK_INT(status, T64_STATUS_OK);
  status = t64_machine_bus_read(machine, T64_ISVIEWER_BASE + 0x24u, 4u, &value);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK(value == UINT32_C(0xe5f60718));
  if (active && status == T64_STATUS_OK && value == UINT32_C(0xe5f60718)) {
    accepted_ids++;
  }
  t64_machine_destroy(machine);
}

static void check_isviewer_refuses(const char *profile_id) {
  t64_machine *machine = create_admitted(profile_id);
  t64_status status;

  if (machine == NULL) return;
  T64_CHECK(!t64_isviewer_active(machine));
  status = t64_machine_bus_write(machine, T64_ISVIEWER_BASE, 4u,
                                 UINT32_C(0x12345678));
  T64_CHECK_INT(status, T64_STATUS_BUS_DEVICE_REFUSED);
  if (!t64_isviewer_active(machine) &&
      status == T64_STATUS_BUS_DEVICE_REFUSED) {
    refused_ids++;
  }
  t64_machine_destroy(machine);
}

static void case_v26_isviewer(void) {
  size_t i;

  accepted_ids = 0u;
  refused_ids = 0u;
  for (i = 0u; i < ISVIEWER_ID_COUNT; i++) {
    check_isviewer_accepts(isviewer_accept_ids[i]);
  }
  check_isviewer_refuses(OUTSIDE_FAMILY_ID);
  printf("v26-isviewer: accepted ids=%u refused ids=%u\n", accepted_ids,
         refused_ids);
  T64_CHECK_INT(accepted_ids, ISVIEWER_ID_COUNT);
  T64_CHECK_INT(refused_ids, 1u);
}

/* ---- predicate matrix over all 25 registered ids ------------------------ */

typedef struct {
  const char *id;
  bool depth;
  bool si;
  bool v25;
  bool v26;
  bool ai_device;
} matrix_row;

/* Written by hand from the descriptor and tuple of each id, not read from the
 * predicates. Tuples (descriptor, model, state): the 17 earlier ids run from
 * (1,1,5) to (11,19,24); depth needs descriptor 10 with model 16, 17 or 18
 * (rows depth-v7, vi-v8, vi-filter-v9); the AI row (11,19,24) has the AI
 * device but no depth; the si-v1 family is (12,20,25); the si-v2 family is
 * (13,21,26) with depth. */
#define PRE_V9 "ntsc-cart-4m-openipl3-rdram-b-t1-"
static const matrix_row matrix_rows[] = {
    /* id, depth, si family, exact 12/20/25, exact 13/21/26, ai device */
    {"ntsc-cart-4m-skeleton", false, false, false, false, false},
    {"ntsc-cart-4m-openipl3-warm-v1", false, false, false, false, false},
    {"ntsc-cart-4m-openipl3-vr4300-v1", false, false, false, false, false},
    {"ntsc-cart-4m-openipl3-vr4300-compat-v1", false, false, false, false,
     false},
    {"ntsc-cart-4m-openipl3-vr4300-compat-v2", false, false, false, false,
     false},
    {"ntsc-cart-4m-openipl3-rsp-functional-v1", false, false, false, false,
     false},
    {"ntsc-cart-4m-openipl3-rdram-a-t1-v1", false, false, false, false,
     false},
    {PRE_V9 "v1", false, false, false, false, false},
    {PRE_V9 "rdp-vi-v2", false, false, false, false, false},
    {PRE_V9 "rdp-triangle-v3", false, false, false, false, false},
    {PRE_V9 "rdp-texture-v4", false, false, false, false, false},
    {PRE_V9 "rdp-combiner-v5", false, false, false, false, false},
    {PRE_V9 "rdp-blender-v6", false, false, false, false, false},
    {PRE_V9 "rdp-depth-v7", true, false, false, false, false},
    {PRE_V9 "rdp-vi-v8", true, false, false, false, false},
    {PRE_V9 "rdp-vi-filter-v9", true, false, false, false, false},
    {PRE_V9 "rdp-vi-filter-v9-ai-v1", false, false, false, false, true},
    {V1_STEM, false, true, true, false, true},
    {V1_STEM "-e4k", false, true, true, false, true},
    {V1_STEM "-e16k", false, true, true, false, true},
    {V1_STEM "-s32k", false, true, true, false, true},
    {V2_STEM, true, true, false, true, true},
    {V2_STEM "-e4k", true, true, false, true, true},
    {V2_STEM "-e16k", true, true, false, true, true},
    {V2_STEM "-s32k", true, true, false, true, true},
};

#define MATRIX_ROW_COUNT (sizeof matrix_rows / sizeof matrix_rows[0])

static void expect_flag(const char *id, const char *name, bool got,
                        bool want) {
  if (got != want) {
    fprintf(stderr, "v26-predicate-matrix: %s: %s is %s, expected %s\n", id,
            name, got ? "true" : "false", want ? "true" : "false");
  }
  T64_CHECK(got == want);
}

static void case_v26_predicate_matrix(void) {
  size_t i;
  size_t depth_rows = 0u;
  size_t checked = 0u;

  T64_CHECK_INT(MATRIX_ROW_COUNT, 25);
  for (i = 0u; i < MATRIX_ROW_COUNT; i++) {
    const matrix_row *row = &matrix_rows[i];
    const t64_profile_descriptor *profile = lookup_profile(row->id);
    t64_machine *machine = NULL;

    if (profile == NULL) continue;
    T64_CHECK_INT(t64_machine_create(
                      &(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(
                          &allocator, profile),
                      &machine),
                  T64_STATUS_OK);
    if (machine == NULL) continue;
    expect_flag(row->id, "t64_has_rdp_depth_profile",
                t64_has_rdp_depth_profile(machine), row->depth);
    expect_flag(row->id, "t64_has_si_profile", t64_has_si_profile(machine),
                row->si);
    expect_flag(row->id, "t64_has_si_v25_profile",
                t64_has_si_v25_profile(machine), row->v25);
    expect_flag(row->id, "t64_has_si_v26_profile",
                t64_has_si_v26_profile(machine), row->v26);
    expect_flag(row->id, "t64_has_ai_device", t64_has_ai_device(machine),
                row->ai_device);
    /* The exact predicates are disjoint and the family is their union. */
    T64_CHECK(!(t64_has_si_v25_profile(machine) &&
                t64_has_si_v26_profile(machine)));
    T64_CHECK(t64_has_si_profile(machine) ==
              (t64_has_si_v25_profile(machine) ||
               t64_has_si_v26_profile(machine)));
    if (row->depth) depth_rows++;
    checked++;
    t64_machine_destroy(machine);
  }
  /* Depth: three descriptor 10 ids plus the four descriptor 13 ids. */
  T64_CHECK_INT(depth_rows, 7);
  printf("v26-predicate-matrix: rows=%zu\n", checked);
  T64_CHECK_INT(checked, 25);
}

int main(int argc, char **argv) {
  static const char *const cases[] = {"v26-identity", "v26-isviewer",
                                      "v26-predicate-matrix"};

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    size_t index;

    for (index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
      printf("%s\n", cases[index]);
    }
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    if (strcmp(argv[2], "v26-identity") == 0) {
      case_v26_identity();
      return T64_TEST_FINISH("unit.core.profile.v26-identity");
    }
    if (strcmp(argv[2], "v26-isviewer") == 0) {
      case_v26_isviewer();
      return T64_TEST_FINISH("unit.core.profile.v26-isviewer");
    }
    if (strcmp(argv[2], "v26-predicate-matrix") == 0) {
      case_v26_predicate_matrix();
      return T64_TEST_FINISH("unit.core.profile.v26-predicate-matrix");
    }
    fprintf(stderr, "unknown case: %s\n", argv[2]);
    return 2;
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--case NAME]\n", argv[0]);
    return 2;
  }
  case_v26_identity();
  case_v26_isviewer();
  case_v26_predicate_matrix();
  return T64_TEST_FINISH("unit.core.profile.v26");
}
