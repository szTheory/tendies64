/*
 * State container export tests (plan 09-08).
 *
 * The decoder in this file is written from the container layout only, with
 * literal offsets and its own big-endian readers. It never calls a production
 * helper and never includes the production section or container constants for
 * offsets, so a layout mistake in core/t64_state.c cannot be hidden by a
 * matching mistake here. The hashes are integrity checks, not authentication:
 * a blob whose hashes were recomputed after tampering still decodes.
 *
 * Every input is authored or the already cleared openIPL3 asset; no ROM, save,
 * firmware image, hardware measurement or oracle is used.
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
#define AUTHORED_ROM_BYTES 4096u
#define SI_STEM_ID "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1"

/* ---- machine construction (copied from core/tests/test_digest_v25.c) ---- */

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

/* Create the machine for an id and admit the cleared openIPL3 asset. */
static t64_machine *create_admitted(const char *profile_id) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t asset[IPL3_ASSET_BYTES];
  t64_status status;

  T64_CHECK_INT(t64_profile_lookup(profile_id, &profile), T64_STATUS_OK);
  if (profile == NULL) return NULL;
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
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

static void install_spin_loop(t64_machine *machine) {
  machine->ram[UINT32_C(0x1000)] = 0x08u;
  machine->ram[UINT32_C(0x1001)] = 0x00u;
  machine->ram[UINT32_C(0x1002)] = 0x04u;
  machine->ram[UINT32_C(0x1003)] = 0x00u;
  (void)memset(machine->ram + UINT32_C(0x1004), 0, 4u);
  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = t64_cpu_canonical_address(machine->cpu.pc);
}

/* The tick reported by the last stop result of advance_to, so the header tick
 * is compared with a value the machine reported through its public result. */
static uint64_t last_stop_tick;

static bool advance_to(t64_machine *machine, uint64_t tick) {
  t64_stop_request request = {T64_STOP_GUEST_TICK, tick};
  t64_stop_result stop = {0u, 0u};
  const t64_status status = t64_machine_advance(machine, &request, &stop);

  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(stop.reason, T64_STOP_REASON_BOUND);
  last_stop_tick = stop.tick;
  return status == T64_STATUS_OK && stop.reason == T64_STOP_REASON_BOUND;
}

/* Idle: admitted, a guest spin loop retiring in RDRAM, tick 64. */
static t64_machine *build_idle(const char *profile_id) {
  t64_machine *machine = create_admitted(profile_id);

  if (machine == NULL) return NULL;
  install_spin_loop(machine);
  T64_CHECK(advance_to(machine, 64u));
  T64_CHECK_INT(machine->tick, 64u);
  T64_CHECK(machine->cpu.retired_instructions > 0u);
  return machine;
}

/* ---- independent reference decoder: literal offsets only --------------- */

#define D_MAGIC 0x000u
#define D_CONTAINER_VERSION 0x008u
#define D_STATE_VERSION 0x00Cu
#define D_TOTAL 0x010u
#define D_COUNT 0x018u
#define D_FLAGS 0x01Cu
#define D_PROFILE 0x020u
#define D_CONTENT_SHA 0x040u
#define D_CONTENT_LEN 0x060u
#define D_TICK 0x068u
#define D_DECLARED 0x070u
#define D_CANON_OFF 0x090u
#define D_CANON_LEN 0x098u
#define D_BUILD 0x0A0u
#define D_BUILD_BYTES 128u
#define D_TABLE 0x120u
#define D_ENTRY 56u
#define E_ID 0u
#define E_FLAGS 4u
#define E_OFF 8u
#define E_LEN 16u
#define E_HASH 24u
#define D_OBSERVER_BYTES 40u
#define D_TRAILER_BYTES 32u
#define D_MAX_SECTIONS 32u
#define D_DOMAIN "t64.state.si.25"
#define D_DOMAIN_BYTES 15u
#define D_OBSERVER_ID 16u

typedef enum {
  DEC_OK = 0,
  DEC_MAGIC,
  DEC_CONTAINER_VERSION,
  DEC_TOTAL_LENGTH,
  DEC_SECTION_COUNT,
  DEC_FLAGS,
  DEC_TABLE_ORDER,
  DEC_SECTION_RANGE,
  DEC_MARKER,
  DEC_SECTION_HASH,
  DEC_CANONICAL_DIGEST,
  DEC_HEADER_IDENTITY,
  DEC_OBSERVER,
  DEC_TRAILER
} dec_kind;

static const char *const dec_names[] = {
    "ok",           "magic",        "container-version", "total-length",
    "section-count", "flags",       "table-order",       "section-range",
    "marker",       "section-hash", "canonical-digest",  "header-identity",
    "observer",     "trailer"};

typedef struct {
  uint32_t state_version;
  uint8_t profile_digest[T64_DIGEST_SIZE];
  uint8_t content_digest[T64_DIGEST_SIZE];
  uint64_t content_length;
  uint64_t tick;
  char build_id[D_BUILD_BYTES];
} dec_expect;

typedef struct {
  uint32_t container_version;
  uint32_t state_version;
  uint64_t total;
  uint32_t count;
  uint32_t flags;
  uint8_t profile_digest[T64_DIGEST_SIZE];
  uint8_t content_digest[T64_DIGEST_SIZE];
  uint64_t content_length;
  uint64_t tick;
  uint8_t declared[T64_DIGEST_SIZE];
  uint64_t canonical_offset;
  uint64_t canonical_length;
  uint8_t build_id[D_BUILD_BYTES];
  uint32_t ids[D_MAX_SECTIONS];
  uint32_t entry_flags[D_MAX_SECTIONS];
  uint64_t offsets[D_MAX_SECTIONS];
  uint64_t lengths[D_MAX_SECTIONS];
  uint64_t observer[5];
} dec_info;

static uint32_t rd32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint64_t rd64(const uint8_t *p) {
  return ((uint64_t)rd32(p) << 32) | (uint64_t)rd32(p + 4);
}

static void sha256_of(const uint8_t *bytes, size_t length,
                      uint8_t out[T64_DIGEST_SIZE]) {
  t64_sha256_ctx context;

  t64_sha256_init(&context);
  t64_sha256_update(&context, bytes, length);
  t64_sha256_final(&context, out);
}

static bool bytes_equal(const uint8_t *left, const uint8_t *right,
                        size_t length) {
  return memcmp(left, right, length) == 0;
}

/* Decode and verify one container; the first failing check names the kind.
 * The check order is the order the controls rely on: header sanity, table
 * order, observer entry shape, canonical ranges and markers, body hashes,
 * the declared digest, the header identity, then the trailer. */
static dec_kind decode_container(const uint8_t *blob, size_t length,
                                 const dec_expect *expect, dec_info *info) {
  uint8_t digest[T64_DIGEST_SIZE];
  uint64_t canonical_end;
  uint64_t next_marker;
  uint32_t i;
  size_t j;

  (void)memset(info, 0, sizeof *info);
  if (length < D_TABLE) return DEC_TOTAL_LENGTH;
  if (memcmp(blob + D_MAGIC, "T64STATE", 8u) != 0) return DEC_MAGIC;
  info->container_version = rd32(blob + D_CONTAINER_VERSION);
  if (info->container_version != 1u) return DEC_CONTAINER_VERSION;
  info->total = rd64(blob + D_TOTAL);
  if (info->total != (uint64_t)length) return DEC_TOTAL_LENGTH;
  info->count = rd32(blob + D_COUNT);
  if (info->count < 2u || info->count > D_MAX_SECTIONS ||
      D_TABLE + (uint64_t)D_ENTRY * info->count > (uint64_t)length) {
    return DEC_SECTION_COUNT;
  }
  info->flags = rd32(blob + D_FLAGS);
  if (info->flags != 0u) return DEC_FLAGS;
  info->state_version = rd32(blob + D_STATE_VERSION);
  (void)memcpy(info->profile_digest, blob + D_PROFILE, T64_DIGEST_SIZE);
  (void)memcpy(info->content_digest, blob + D_CONTENT_SHA, T64_DIGEST_SIZE);
  info->content_length = rd64(blob + D_CONTENT_LEN);
  info->tick = rd64(blob + D_TICK);
  (void)memcpy(info->declared, blob + D_DECLARED, T64_DIGEST_SIZE);
  info->canonical_offset = rd64(blob + D_CANON_OFF);
  info->canonical_length = rd64(blob + D_CANON_LEN);
  (void)memcpy(info->build_id, blob + D_BUILD, D_BUILD_BYTES);
  for (i = 0u; i < info->count; i++) {
    const uint8_t *entry = blob + D_TABLE + (size_t)D_ENTRY * i;

    info->ids[i] = rd32(entry + E_ID);
    info->entry_flags[i] = rd32(entry + E_FLAGS);
    info->offsets[i] = rd64(entry + E_OFF);
    info->lengths[i] = rd64(entry + E_LEN);
  }
  for (i = 1u; i < info->count; i++) {
    if (info->ids[i] <= info->ids[i - 1u]) return DEC_TABLE_ORDER;
  }
  if (info->canonical_offset != D_TABLE + (uint64_t)D_ENTRY * info->count ||
      info->canonical_length >
          (uint64_t)length - info->canonical_offset ||
      info->canonical_offset + info->canonical_length + D_OBSERVER_BYTES +
              D_TRAILER_BYTES !=
          (uint64_t)length) {
    return DEC_SECTION_RANGE;
  }
  canonical_end = info->canonical_offset + info->canonical_length;
  i = info->count - 1u;
  if (info->ids[i] != D_OBSERVER_ID || info->entry_flags[i] != 1u ||
      info->offsets[i] != canonical_end ||
      info->lengths[i] != D_OBSERVER_BYTES) {
    return DEC_OBSERVER;
  }
  for (i = 0u; i + 1u < info->count; i++) {
    if (info->entry_flags[i] != 0u) return DEC_FLAGS;
  }
  if (info->canonical_length < D_DOMAIN_BYTES ||
      memcmp(blob + info->canonical_offset, D_DOMAIN, D_DOMAIN_BYTES) != 0) {
    return DEC_MARKER;
  }
  next_marker = info->canonical_offset + D_DOMAIN_BYTES;
  for (i = 0u; i + 1u < info->count; i++) {
    if (info->offsets[i] != next_marker + 4u ||
        info->lengths[i] > canonical_end - info->offsets[i]) {
      return DEC_SECTION_RANGE;
    }
    if (rd32(blob + info->offsets[i] - 4u) != info->ids[i]) return DEC_MARKER;
    next_marker = info->offsets[i] + info->lengths[i];
  }
  if (next_marker != canonical_end) return DEC_SECTION_RANGE;
  for (i = 0u; i < info->count; i++) {
    const uint8_t *entry = blob + D_TABLE + (size_t)D_ENTRY * i;

    sha256_of(blob + info->offsets[i], (size_t)info->lengths[i], digest);
    if (!bytes_equal(digest, entry + E_HASH, T64_DIGEST_SIZE)) {
      return DEC_SECTION_HASH;
    }
  }
  sha256_of(blob + info->canonical_offset, (size_t)info->canonical_length,
            digest);
  if (!bytes_equal(digest, info->declared, T64_DIGEST_SIZE)) {
    return DEC_CANONICAL_DIGEST;
  }
  /* The header agrees with the bodies it summarizes: the identity body starts
   * with the state serialization version, the profile digest and the content
   * digest; the machine-core body starts with the guest tick. */
  if (info->ids[0] != 1u || info->ids[1] != 2u || info->lengths[0] < 76u ||
      info->lengths[1] < 8u ||
      rd32(blob + info->offsets[0]) != info->state_version ||
      !bytes_equal(blob + info->offsets[0] + 4u, info->profile_digest,
                   T64_DIGEST_SIZE) ||
      !bytes_equal(blob + info->offsets[0] + 36u, info->content_digest,
                   T64_DIGEST_SIZE) ||
      rd32(blob + info->offsets[0] + 72u) != info->content_length ||
      rd64(blob + info->offsets[1]) != info->tick) {
    return DEC_HEADER_IDENTITY;
  }
  if (expect != NULL) {
    size_t build_length = 0u;

    while (build_length < D_BUILD_BYTES &&
           expect->build_id[build_length] != '\0') {
      build_length++;
    }
    if (info->state_version != expect->state_version ||
        !bytes_equal(info->profile_digest, expect->profile_digest,
                     T64_DIGEST_SIZE) ||
        !bytes_equal(info->content_digest, expect->content_digest,
                     T64_DIGEST_SIZE) ||
        info->content_length != expect->content_length ||
        info->tick != expect->tick || build_length >= D_BUILD_BYTES ||
        memcmp(info->build_id, expect->build_id, build_length) != 0) {
      return DEC_HEADER_IDENTITY;
    }
    /* NUL padded to the end, and no path character anywhere in the field. */
    for (j = build_length; j < D_BUILD_BYTES; j++) {
      if (info->build_id[j] != 0) return DEC_HEADER_IDENTITY;
    }
    for (j = 0u; j < D_BUILD_BYTES; j++) {
      if (info->build_id[j] == '/' || info->build_id[j] == '\\') {
        return DEC_HEADER_IDENTITY;
      }
    }
  }
  for (j = 0u; j < 5u; j++) {
    info->observer[j] = rd64(blob + canonical_end + 8u * j);
  }
  sha256_of(blob, length - D_TRAILER_BYTES, digest);
  if (!bytes_equal(digest, blob + length - D_TRAILER_BYTES,
                   D_TRAILER_BYTES)) {
    return DEC_TRAILER;
  }
  return DEC_OK;
}

/* Expected header values, all read through the public API. */
static bool make_expect(const t64_machine *machine, uint64_t tick,
                        dec_expect *expect) {
  const t64_profile_descriptor *profile = NULL;
  size_t rom_size = 0u;
  const char *build = t64_build_identity();

  (void)memset(expect, 0, sizeof *expect);
  T64_CHECK_INT(t64_machine_profile(machine, &profile), T64_STATUS_OK);
  if (profile == NULL) return false;
  T64_CHECK_INT(t64_profile_hash(profile, expect->profile_digest),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_content_digest(machine, expect->content_digest),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_rom_size(machine, &rom_size), T64_STATUS_OK);
  expect->state_version = profile->state_serialization_version;
  expect->content_length = (uint64_t)rom_size;
  expect->tick = tick;
  T64_CHECK(strlen(build) < D_BUILD_BYTES);
  if (strlen(build) < D_BUILD_BYTES) {
    (void)memcpy(expect->build_id, build, strlen(build));
  }
  return true;
}

/* ---- case export-layout (the tracer) ----------------------------------- */

#define CANARY_BYTES 64u
#define CANARY_FILL 0xc3u

/* Section ids of the stem id (the section 11 save-media section exists for
 * the three save ids only), then the observer section 16. */
static const uint32_t stem_ids[] = {1u, 2u, 3u, 4u, 5u,
                                    6u, 7u, 8u, 9u, 10u, 16u};

static void case_export_layout(void) {
  t64_machine *machine = build_idle(SI_STEM_ID);
  size_t required = 0u;
  size_t written = 0u;
  uint8_t *buffer;
  uint8_t digest[T64_DIGEST_SIZE];
  uint8_t canonical[T64_DIGEST_SIZE];
  dec_expect expect;
  dec_info info;
  dec_kind kind;
  t64_status status;
  uint32_t i;
  size_t k;

  if (machine == NULL) return;
  status = t64_machine_state_export(machine, NULL, 0u, &required);
  T64_CHECK_INT(status, T64_STATUS_BUFFER_TOO_SMALL);
  T64_CHECK(required > 8400916u);
  T64_CHECK(required < 11534336u);
  if (status != T64_STATUS_BUFFER_TOO_SMALL || required < D_TABLE) {
    t64_machine_destroy(machine);
    return;
  }
  buffer = (uint8_t *)malloc(required + CANARY_BYTES);
  if (buffer == NULL) {
    T64_CHECK(false);
    t64_machine_destroy(machine);
    return;
  }
  (void)memset(buffer, 0, required);
  (void)memset(buffer + required, CANARY_FILL, CANARY_BYTES);
  status = t64_machine_state_export(machine, buffer, required, &written);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(written, required);
  for (k = 0u; k < CANARY_BYTES; k++) {
    T64_CHECK_INT(buffer[required + k], CANARY_FILL);
  }
  if (status == T64_STATUS_OK && written == required) {
    T64_CHECK(make_expect(machine, last_stop_tick, &expect));
    kind = decode_container(buffer, required, &expect, &info);
    if (kind != DEC_OK) {
      fprintf(stderr, "export-layout: decoder kind %s\n", dec_names[kind]);
    }
    T64_CHECK_INT(kind, DEC_OK);
    T64_CHECK_INT(info.container_version, 1);
    T64_CHECK_INT(info.state_version, 25);
    T64_CHECK_INT(info.flags, 0);
    T64_CHECK_INT(info.tick, 64);
    T64_CHECK_INT(info.count, sizeof stem_ids / sizeof stem_ids[0]);
    for (i = 0u; i < info.count && i < sizeof stem_ids / sizeof stem_ids[0];
         i++) {
      T64_CHECK_INT(info.ids[i], stem_ids[i]);
    }
    T64_CHECK_INT(info.entry_flags[info.count - 1u], 1);
    T64_CHECK_INT(info.lengths[info.count - 1u], 40);
    /* Literal body lengths: machine core 32, RDRAM 4,194,308, SP memory
     * 8,200 (ids 2, 3 and 4). */
    T64_CHECK_INT(info.lengths[1], 32);
    T64_CHECK_INT(info.lengths[2], 4194308);
    T64_CHECK_INT(info.lengths[3], 8200);
    /* The declared digest is the machine digest and the hash of the
     * canonical region. */
    T64_CHECK_INT(t64_machine_digest(machine, digest), T64_STATUS_OK);
    T64_CHECK(bytes_equal(info.declared, digest, T64_DIGEST_SIZE));
    sha256_of(buffer + info.canonical_offset, (size_t)info.canonical_length,
              canonical);
    T64_CHECK(bytes_equal(canonical, digest, T64_DIGEST_SIZE));
    /* Nothing was drained or dequeued: the observer values are zero. */
    for (k = 0u; k < 5u; k++) {
      T64_CHECK_INT(info.observer[k], 0);
    }
  }
  free(buffer);
  t64_machine_destroy(machine);
}

int main(int argc, char **argv) {
  static const char *const cases[] = {"export-layout"};

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    size_t index;

    for (index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
      printf("%s\n", cases[index]);
    }
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    if (strcmp(argv[2], "export-layout") == 0) {
      case_export_layout();
      return T64_TEST_FINISH("unit.core.state.export.export-layout");
    }
    fprintf(stderr, "unknown case: %s\n", argv[2]);
    return 2;
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--case NAME]\n", argv[0]);
    return 2;
  }
  case_export_layout();
  return T64_TEST_FINISH("unit.core.state.export");
}
