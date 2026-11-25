/*
 * Phase 9 save import and export tests (plan 09-05): the D-24 check order, the
 * three byte-order selections, the success and refusal postconditions and the
 * replacement rule of the provisional core save API.
 *
 * Every expected value below is written out here from the specification text:
 * the check order as the table D24_ORDER, the transforms as 8-byte literal
 * vectors and as an offset mapping, the image as a closed formula. Nothing is
 * read from a production helper. The transform definitions are authored policy
 * (assumption A1), not a hardware fact, and no other emulator's convention is
 * asserted. All contents are authored byte patterns; no ROM, save or firmware
 * image is used (the only external input is the already cleared openIPL3 asset
 * named by T64_TEST_IPL3_PATH).
 *
 * usage: test_save_transfer [--list-cases|--case NAME]
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
/* MAP_ANON (the guard mapping of no-caller-read) is a feature-macro name on
 * both hosts under -std=c17. */
#if defined(__APPLE__)
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif
#elif !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#endif

#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "t64_internal.h"
#include "t64_test.h"

#define STEM_ID "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1"
#define E4K_ID "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e4k"
#define E16K_ID "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e16k"
#define S32K_ID "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-s32k"
#define IPL3_ASSET_BYTES 4096u

/* ---- machine setup (copied helpers; nothing shared with other tests) ------ */

static bool read_openipl3_asset(uint8_t bytes[IPL3_ASSET_BYTES]) {
  const char *path = getenv("T64_TEST_IPL3_PATH");
  FILE *file;
  size_t length;
  int extra;
  int read_error;
  int close_error;

  /* CTest sets the path; a direct run from the repository root (the verify
   * commands of the plan) finds the built asset at its fixed relative place. */
  if (path == NULL || path[0] == '\0') path = "build/fixtures/ipl3_compat.z64";
  if (bytes == NULL) return false;
  file = fopen(path, "rb");
  if (file == NULL) return false;
  length = fread(bytes, 1u, IPL3_ASSET_BYTES, file);
  extra = fgetc(file);
  read_error = ferror(file);
  close_error = fclose(file);
  return read_error == 0 && close_error == 0 &&
         length == IPL3_ASSET_BYTES && extra == EOF;
}

/* A counting allocator: a refusal and a success must make no allocator call
 * after the machine exists (D-24: the core never allocates here). */
static size_t allocator_allocations;
static size_t allocator_releases;

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
  allocator_allocations++;
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  return true;
}

static void test_release(void *context, t64_host_storage storage) {
  (void)context;
  allocator_releases++;
  free(storage.data);
}

static const t64_host_allocator allocator =
    T64_HOST_ALLOCATOR_INIT(test_allocate, test_release, NULL);

/* Poisons a save info before a call: the INIT form first, so a leading
 * struct_size of the flip stays valid, then a non-zero value in every other
 * member by name, so a callee that leaves a member alone or writes zeros still
 * shows in the checks of the fields it must report. */
static void poison_save_info(t64_save_info *info) {
  *info = (t64_save_info)T64_SAVE_INFO_INIT;
  info->kind = UINT32_C(0xa5a5a5a5);
  info->size_bytes = UINT32_C(0xa5a5a5a5);
  info->flags = UINT32_C(0xa5a5a5a5);
  info->write_generation = UINT64_C(0xa5a5a5a5a5a5a5a5);
}

static t64_machine *create_admitted(const char *profile_id) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t asset[IPL3_ASSET_BYTES];

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
  T64_CHECK_INT(t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES),
                T64_STATUS_OK);
  return machine;
}

static void digest_of(const t64_machine *machine,
                      uint8_t digest[T64_DIGEST_SIZE]) {
  T64_CHECK_INT(t64_machine_digest(machine, digest), T64_STATUS_OK);
}

/* ---- literal tables ------------------------------------------------------- */

/*
 * The D-24 check order, authored from the specification text before any
 * implementation: row, condition, status. R8 differs between import and export
 * (SIZE_MISMATCH versus BUFFER_TOO_SMALL); the table holds the import status
 * and the export status of that row is named separately below.
 */
typedef struct {
  const char *row;
  const char *condition;
  t64_status status;
} d24_row;

static const d24_row D24_ORDER[] = {
    {"R0", "machine NULL", T64_STATUS_INVALID_ARGUMENT},
    {"R1", "no admitted image", T64_STATUS_NOT_READY},
    {"R2", "pointer or span", T64_STATUS_INVALID_ARGUMENT},
    {"R3", "profile has no save device", T64_STATUS_SAVE_DEVICE_MISMATCH},
    {"R4", "kind not in 1 to 7", T64_STATUS_SAVE_UNKNOWN_DEVICE},
    {"R5", "kind in 4 to 7", T64_STATUS_SAVE_UNSUPPORTED_DEVICE},
    {"R6", "kind in 1 to 3 but not the profile kind",
     T64_STATUS_SAVE_DEVICE_MISMATCH},
    {"R7", "format not in 1 to 3", T64_STATUS_SAVE_BAD_FORMAT},
    {"R8", "import length not the kind size",
     T64_STATUS_SAVE_SIZE_MISMATCH},
    {"R9", "import while in flight", T64_STATUS_SAVE_BUSY},
};
#define D24_ROWS (sizeof D24_ORDER / sizeof D24_ORDER[0])

/* The statuses written as numbers (the public values are provisional until
 * Phase 10); the table above must agree with them row by row. */
static const uint32_t d24_numbers[D24_ROWS] = {1u,  7u,  1u,  22u, 20u,
                                               21u, 22u, 23u, 24u, 25u};

static void check_order_table(void) {
  size_t row;

  T64_CHECK_INT(D24_ROWS, 10u);
  for (row = 0u; row < D24_ROWS && row < 10u; row++) {
    char expected_name[3];

    expected_name[0] = 'R';
    expected_name[1] = (char)('0' + (int)row);
    expected_name[2] = '\0';
    T64_CHECK_STR(D24_ORDER[row].row, expected_name);
    T64_CHECK_INT(D24_ORDER[row].status, d24_numbers[row]);
  }
  /* The export status of row R8. */
  T64_CHECK_INT(T64_STATUS_BUFFER_TOO_SMALL, 9);
}

/* Authored image formula: byte i is (37 i + 11 + 101 (i >> 8)) modulo 256;
 * every 4-byte word holds four distinct bytes, so SWAP16 and WORDREV32 differ. */
static uint8_t image_byte(size_t index) {
  return (uint8_t)((37u * (unsigned)index + 11u +
                    101u * (unsigned)(index >> 8)) &
                   0xFFu);
}

static void fill_image(uint8_t *bytes, size_t length) {
  size_t index;

  for (index = 0u; index < length; index++) bytes[index] = image_byte(index);
}

/* The three 8-byte vectors, written out as literals (assumption A1). */
static const uint8_t vector_raw[8] = {0x00, 0x01, 0x02, 0x03,
                                      0x04, 0x05, 0x06, 0x07};
static const uint8_t vector_swap16[8] = {0x01, 0x00, 0x03, 0x02,
                                         0x05, 0x04, 0x07, 0x06};
static const uint8_t vector_wordrev32[8] = {0x03, 0x02, 0x01, 0x00,
                                            0x07, 0x06, 0x05, 0x04};
/* A SWAP16 vector read as WORDREV32, and the reverse, give the same bytes. */
static const uint8_t vector_crossed[8] = {0x02, 0x03, 0x00, 0x01,
                                          0x06, 0x07, 0x04, 0x05};

/* ---- shared helpers ------------------------------------------------------- */

typedef struct {
  const char *id;
  uint32_t kind;
  size_t size;
  uint8_t blank;
} profile_row;

/* The four family ids: the stem has no save device, the other three carry the
 * three save kinds (512, 2048 and 32,768 bytes). */
static const profile_row profile_rows[] = {
    {STEM_ID, 0u, 0u, 0x00u},
    {E4K_ID, 1u, 512u, 0xFFu},
    {E16K_ID, 2u, 2048u, 0xFFu},
    {S32K_ID, 3u, 32768u, 0x00u},
};
#define PROFILE_ROWS (sizeof profile_rows / sizeof profile_rows[0])

/* A machine that was created and never given an image (row R1). */
static t64_machine *create_unadmitted(const char *profile_id) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;

  T64_CHECK_INT(t64_profile_lookup(profile_id, &profile), T64_STATUS_OK);
  if (profile == NULL) return NULL;
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  return machine;
}

/* An admitted machine whose boot CPU event is cancelled, so the devices are
 * driven by host bus writes and guest-tick advances alone. */
static t64_machine *make_idle_machine(const char *profile_id) {
  t64_machine *machine = create_admitted(profile_id);
  uint64_t cancelled_tick = UINT64_MAX;
  t64_status status;

  if (machine == NULL) return NULL;
  status = t64_schedule_cancel(machine, T64_DEVICE_CPU, T64_CPU_EVENT_STEP,
                               0u, &cancelled_tick);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(cancelled_tick, 1u);
  if (status != T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return NULL;
  }
  return machine;
}

static bool advance_guest_to(t64_machine *machine, uint64_t tick) {
  t64_stop_request request = T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, tick);
  t64_stop_result result = T64_STOP_RESULT_INIT;
  t64_status status = t64_machine_advance(machine, &request, &result);

  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) return false;
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, tick);
  return result.reason == T64_STOP_REASON_BOUND && result.tick == tick;
}

/* The comparison state of one machine: every item D-24 names for a refusal
 * (digest, device bytes, deadline, scheduler queue and next sequence, tick,
 * trace, allocator counters, write generation) and a checksum over the whole
 * machine structure (every device field outside the RDRAM and the image). The
 * full digest costs about 14 ms, so it is taken when `full` is set. */
typedef struct {
  bool has_digest;
  uint8_t digest[T64_DIGEST_SIZE];
  uint64_t structure_checksum;
  uint64_t deadline;
  uint64_t tick;
  uint64_t next_sequence;
  uint64_t generation;
  size_t queue_head;
  size_t queue_count;
  size_t trace_count;
  size_t allocations;
  size_t releases;
  size_t storage_size;
  uint8_t storage[T64_SAVE_SRAM_BYTES];
} snapshot;

/* FNV-1a over 8-byte words (the structure is about 79 KB; the campaign takes
 * two of these per request), then over the remaining bytes. */
static uint64_t fnv1a64(const uint8_t *bytes, size_t length) {
  uint64_t hash = UINT64_C(0xcbf29ce484222325);
  size_t index = 0u;

  for (; index + 8u <= length; index += 8u) {
    uint64_t word;

    (void)memcpy(&word, bytes + index, 8u);
    hash ^= word;
    hash *= UINT64_C(0x100000001b3);
  }
  for (; index < length; index++) {
    hash ^= (uint64_t)bytes[index];
    hash *= UINT64_C(0x100000001b3);
  }
  return hash;
}

static void take_snapshot(const t64_machine *machine, bool full,
                          snapshot *out) {
  out->has_digest = full;
  if (full) digest_of(machine, out->digest);
  out->structure_checksum =
      fnv1a64((const uint8_t *)machine, sizeof *machine);
  out->deadline = machine->save.busy_until;
  out->tick = machine->tick;
  out->next_sequence = machine->next_sequence;
  out->generation = machine->save.write_generation;
  out->queue_head = machine->queue_head;
  out->queue_count = machine->queue_count;
  out->trace_count = machine->trace_count;
  out->allocations = allocator_allocations;
  out->releases = allocator_releases;
  out->storage_size = machine->save.bytes == NULL
                          ? 0u
                          : t64_save_storage_bytes(machine->profile.save_kind);
  if (out->storage_size != 0u) {
    (void)memcpy(out->storage, machine->save.bytes, out->storage_size);
  }
}

/* True when every compared item equals the snapshot `before`. */
static bool snapshot_unchanged(const char *label, const t64_machine *machine,
                               bool full, const snapshot *before) {
  static snapshot after;
  bool same;

  take_snapshot(machine, full && before->has_digest, &after);
  same = after.structure_checksum == before->structure_checksum &&
         after.deadline == before->deadline && after.tick == before->tick &&
         after.next_sequence == before->next_sequence &&
         after.generation == before->generation &&
         after.queue_head == before->queue_head &&
         after.queue_count == before->queue_count &&
         after.trace_count == before->trace_count &&
         after.allocations == before->allocations &&
         after.releases == before->releases &&
         after.storage_size == before->storage_size &&
         (before->storage_size == 0u ||
          memcmp(after.storage, before->storage, before->storage_size) == 0) &&
         (!after.has_digest ||
          memcmp(after.digest, before->digest, T64_DIGEST_SIZE) == 0);
  if (!same) {
    fprintf(stderr,
            "state changed by '%s': structure %d deadline %d tick %d seq %d "
            "generation %d queue %d trace %d alloc %d storage %d digest %d\n",
            label, after.structure_checksum == before->structure_checksum,
            after.deadline == before->deadline, after.tick == before->tick,
            after.next_sequence == before->next_sequence,
            after.generation == before->generation,
            after.queue_head == before->queue_head &&
                after.queue_count == before->queue_count,
            after.trace_count == before->trace_count,
            after.allocations == before->allocations &&
                after.releases == before->releases,
            before->storage_size == 0u ||
                memcmp(after.storage, before->storage,
                       before->storage_size) == 0,
            !after.has_digest ||
                memcmp(after.digest, before->digest, T64_DIGEST_SIZE) == 0);
  }
  return same;
}

/* The offset mapping of a selection (assumption A1): the byte at offset i of
 * the RAW image is the byte at offset i XOR 1 (SWAP16) or i XOR 3 (WORDREV32)
 * of the encoded image; RAW is the identity. Written here, not taken from the
 * core. */
static size_t mapped_offset(size_t index, uint32_t format) {
  if (format == 2u) return index ^ 1u;
  if (format == 3u) return index ^ 3u;
  return index;
}

static void map_image(const uint8_t *source, size_t length, uint32_t format,
                      uint8_t *out) {
  size_t index;

  for (index = 0u; index < length; index++) {
    out[index] = source[mapped_offset(index, format)];
  }
}

/* ---- the independent ordered rule function ----------------------------------- */

/* A request as the rule function sees it: what the caller supplies and the
 * state the machine is in, never anything computed by the core. */
typedef enum { POINTER_VALID = 0, POINTER_NULL = 1, POINTER_OVERLAP = 2 } pointer_class;

typedef struct {
  bool is_export;
  bool admitted;
  pointer_class pointer;
  uintptr_t address;    /* the caller pointer value, for the span arithmetic */
  size_t length;        /* import length or export capacity */
  uint32_t profile_kind;
  size_t size;          /* storage size of the profile kind */
  uint32_t kind;
  uint32_t format;
  bool busy;
} request;

/* Returns the index of the first violated row of D24_ORDER (1 to 9; row R0 is
 * the NULL machine, handled by the callers), or -1 when every row passes. A
 * zero-length span is never constructed (an empty span cannot overlap or
 * overflow), so with a zero length only a NULL import pointer violates R2. */
static int first_violated_row(const request *r) {
  const bool span_overflows =
      r->pointer != POINTER_NULL &&
      (uintmax_t)r->length > (uintmax_t)UINTPTR_MAX - (uintmax_t)r->address;

  if (!r->admitted) return 1;
  if (r->pointer == POINTER_NULL) {
    if (!r->is_export || r->length != 0u) return 2;
  } else if (r->length != 0u &&
             (span_overflows || r->pointer == POINTER_OVERLAP)) {
    return 2;
  }
  if (r->profile_kind == 0u) return 3;
  if (r->kind == 0u || r->kind > 7u) return 4;
  if (r->kind >= 4u) return 5;
  if (r->kind != r->profile_kind) return 6;
  if (r->format < 1u || r->format > 3u) return 7;
  if (r->is_export ? r->length < r->size : r->length != r->size) return 8;
  if (!r->is_export && r->busy) return 9;
  return -1;
}

/* The status of a row: the D24_ORDER literal, except the export form of R8. */
static t64_status expected_status(const request *r, int row) {
  if (row < 0) return T64_STATUS_OK;
  if (row == 8 && r->is_export) return T64_STATUS_BUFFER_TOO_SMALL;
  return D24_ORDER[row].status;
}

/* ------------------------------------------------------------------------ */
/* tracer-sram-raw                                                           */
/* ------------------------------------------------------------------------ */

static uint8_t image_a[T64_SAVE_SRAM_BYTES];
static uint8_t image_b[T64_SAVE_SRAM_BYTES];
static uint8_t image_c[T64_SAVE_SRAM_BYTES];

static void case_tracer_sram_raw(void) {
  t64_machine *machine = create_admitted(S32K_ID);
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  t64_save_info info = T64_SAVE_INFO_INIT;
  size_t length = 0u;

  check_order_table();
  if (machine == NULL) return;
  fill_image(image_a, sizeof image_a);

  poison_save_info(&info);
  T64_CHECK_INT(t64_machine_save_info(machine, T64_SAVE_SRAM_32K, &info),
                T64_STATUS_OK);
  T64_CHECK_INT(info.kind, 3u);
  T64_CHECK_INT(info.size_bytes, 32768u);
  T64_CHECK_INT(info.flags & T64_SAVE_INFO_BUSY, 0u);
  T64_CHECK_INT(info.write_generation, 0u);
  digest_of(machine, digest_before);

  T64_CHECK_INT(t64_machine_save_import(machine, T64_SAVE_SRAM_32K,
                                        T64_SAVE_FORMAT_RAW, image_a,
                                        sizeof image_a),
                T64_STATUS_OK);
  poison_save_info(&info);
  T64_CHECK_INT(t64_machine_save_info(machine, T64_SAVE_SRAM_32K, &info),
                T64_STATUS_OK);
  T64_CHECK_INT(info.write_generation, 1u);

  (void)memset(image_b, 0x5A, sizeof image_b);
  T64_CHECK_INT(t64_machine_save_export(machine, T64_SAVE_SRAM_32K,
                                        T64_SAVE_FORMAT_RAW, image_b,
                                        sizeof image_b, &length),
                T64_STATUS_OK);
  T64_CHECK_INT(length, 32768u);
  T64_CHECK(memcmp(image_a, image_b, sizeof image_a) == 0);

  digest_of(machine, digest_after);
  T64_CHECK(memcmp(digest_before, digest_after, T64_DIGEST_SIZE) != 0);
  t64_machine_destroy(machine);
}

/* ------------------------------------------------------------------------ */
/* transform-vectors                                                         */
/* ------------------------------------------------------------------------ */

static void import_all(t64_machine *machine, uint32_t format,
                       const uint8_t *bytes) {
  T64_CHECK_INT(t64_machine_save_import(machine, T64_SAVE_SRAM_32K, format,
                                        bytes, T64_SAVE_SRAM_BYTES),
                T64_STATUS_OK);
}

static void export_all(t64_machine *machine, uint32_t format, uint8_t *bytes) {
  size_t length = 0u;

  (void)memset(bytes, 0xC3, T64_SAVE_SRAM_BYTES);
  T64_CHECK_INT(t64_machine_save_export(machine, T64_SAVE_SRAM_32K, format,
                                        bytes, T64_SAVE_SRAM_BYTES, &length),
                T64_STATUS_OK);
  T64_CHECK_INT(length, T64_SAVE_SRAM_BYTES);
}

static void check_full_size_transforms(void);

static void case_transform_vectors(void) {
  t64_machine *machine = create_admitted(S32K_ID);

  if (machine == NULL) return;

  /* Import direction: the encoded vector stores the RAW vector. */
  fill_image(image_a, sizeof image_a);
  (void)memcpy(image_a, vector_swap16, 8u);
  import_all(machine, T64_SAVE_FORMAT_SWAP16, image_a);
  export_all(machine, T64_SAVE_FORMAT_RAW, image_b);
  T64_CHECK(memcmp(image_b, vector_raw, 8u) == 0);

  (void)memcpy(image_a, vector_wordrev32, 8u);
  import_all(machine, T64_SAVE_FORMAT_WORDREV32, image_a);
  export_all(machine, T64_SAVE_FORMAT_RAW, image_b);
  T64_CHECK(memcmp(image_b, vector_raw, 8u) == 0);

  /* The swapped expectation is rejected: a SWAP16 vector declared as
   * WORDREV32 (and the reverse) stores the crossed bytes, not the RAW vector. */
  (void)memcpy(image_a, vector_swap16, 8u);
  import_all(machine, T64_SAVE_FORMAT_WORDREV32, image_a);
  export_all(machine, T64_SAVE_FORMAT_RAW, image_b);
  T64_CHECK(memcmp(image_b, vector_raw, 8u) != 0);
  T64_CHECK(memcmp(image_b, vector_crossed, 8u) == 0);
  (void)memcpy(image_a, vector_wordrev32, 8u);
  import_all(machine, T64_SAVE_FORMAT_SWAP16, image_a);
  export_all(machine, T64_SAVE_FORMAT_RAW, image_b);
  T64_CHECK(memcmp(image_b, vector_raw, 8u) != 0);
  T64_CHECK(memcmp(image_b, vector_crossed, 8u) == 0);

  /* Export direction: the stored RAW vector exports as the two vectors. */
  fill_image(image_a, sizeof image_a);
  (void)memcpy(image_a, vector_raw, 8u);
  import_all(machine, T64_SAVE_FORMAT_RAW, image_a);
  export_all(machine, T64_SAVE_FORMAT_SWAP16, image_b);
  T64_CHECK(memcmp(image_b, vector_swap16, 8u) == 0);
  T64_CHECK(memcmp(image_b, vector_wordrev32, 8u) != 0);
  export_all(machine, T64_SAVE_FORMAT_WORDREV32, image_c);
  T64_CHECK(memcmp(image_c, vector_wordrev32, 8u) == 0);
  T64_CHECK(memcmp(image_c, vector_swap16, 8u) != 0);
  t64_machine_destroy(machine);

  /* The three kinds at full size against the independent offset mapping. */
  check_full_size_transforms();
}

/* ------------------------------------------------------------------------ */

/* ------------------------------------------------------------------------ */
/* transform-vectors, full size                                              */
/* ------------------------------------------------------------------------ */

static uint8_t export_buffer[T64_SAVE_SRAM_BYTES + 8u];
static uint8_t expected_buffer[T64_SAVE_SRAM_BYTES];

static t64_status do_import(t64_machine *machine, uint32_t kind,
                            uint32_t format, const uint8_t *bytes,
                            size_t length) {
  return t64_machine_save_import(machine, kind, format, bytes, length);
}

static t64_status do_export(const t64_machine *machine, uint32_t kind,
                            uint32_t format, uint8_t *bytes, size_t capacity,
                            size_t *length_out) {
  return t64_machine_save_export(machine, kind, format, bytes, capacity,
                                 length_out);
}

/* For each of the three kinds the full-size image imported in each format and
 * exported RAW equals the independent offset mapping; exporting in each format
 * returns the inverse; the SWAP16 and WORDREV32 outputs differ; a same-format
 * round trip returns the input. */
static void check_full_size_transforms(void) {
  size_t row;

  for (row = 1u; row < PROFILE_ROWS; row++) {
    const profile_row *profile = &profile_rows[row];
    t64_machine *machine = create_admitted(profile->id);
    uint32_t format;
    size_t length = 0u;

    if (machine == NULL) continue;
    fill_image(image_a, profile->size);
    for (format = 1u; format <= 3u; format++) {
      T64_CHECK_INT(do_import(machine, profile->kind, format, image_a,
                              profile->size),
                    T64_STATUS_OK);
      T64_CHECK_INT(do_export(machine, profile->kind, T64_SAVE_FORMAT_RAW,
                              image_b, profile->size, &length),
                    T64_STATUS_OK);
      T64_CHECK_INT(length, profile->size);
      map_image(image_a, profile->size, format, expected_buffer);
      T64_CHECK(memcmp(image_b, expected_buffer, profile->size) == 0);
      T64_CHECK_INT(do_export(machine, profile->kind, format, image_c,
                              profile->size, &length),
                    T64_STATUS_OK);
      T64_CHECK(memcmp(image_c, image_a, profile->size) == 0);
    }
    T64_CHECK_INT(do_import(machine, profile->kind, T64_SAVE_FORMAT_RAW,
                            image_a, profile->size),
                  T64_STATUS_OK);
    for (format = 1u; format <= 3u; format++) {
      T64_CHECK_INT(do_export(machine, profile->kind, format, image_b,
                              profile->size, &length),
                    T64_STATUS_OK);
      map_image(image_a, profile->size, format, expected_buffer);
      T64_CHECK(memcmp(image_b, expected_buffer, profile->size) == 0);
    }
    T64_CHECK_INT(do_export(machine, profile->kind, T64_SAVE_FORMAT_SWAP16,
                            image_b, profile->size, &length),
                  T64_STATUS_OK);
    T64_CHECK_INT(do_export(machine, profile->kind, T64_SAVE_FORMAT_WORDREV32,
                            image_c, profile->size, &length),
                  T64_STATUS_OK);
    T64_CHECK(memcmp(image_b, image_c, profile->size) != 0);
    t64_machine_destroy(machine);
  }
}

/* ------------------------------------------------------------------------ */
/* import-export-roundtrip                                                   */
/* ------------------------------------------------------------------------ */

static void check_success_postconditions(const profile_row *profile) {
  t64_machine *machine = make_idle_machine(profile->id);
  t64_machine *fresh = make_idle_machine(profile->id);
  t64_machine *other = make_idle_machine(profile->id);
  static snapshot before;
  static snapshot other_before;
  uint8_t digest_machine[T64_DIGEST_SIZE];
  uint8_t digest_fresh[T64_DIGEST_SIZE];
  uint8_t digest_repeat[T64_DIGEST_SIZE];
  t64_save_info info = T64_SAVE_INFO_INIT;
  size_t length = 0u;
  uint32_t format;

  if (machine == NULL || fresh == NULL || other == NULL) {
    if (machine != NULL) t64_machine_destroy(machine);
    if (fresh != NULL) t64_machine_destroy(fresh);
    if (other != NULL) t64_machine_destroy(other);
    return;
  }
  T64_CHECK_INT(t64_machine_trace_set_enabled(machine, true), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_trace_set_enabled(fresh, true), T64_STATUS_OK);
  T64_CHECK(advance_guest_to(machine, 1000u));
  T64_CHECK(advance_guest_to(fresh, 1000u));
  T64_CHECK(advance_guest_to(other, 1000u));
  take_snapshot(other, true, &other_before);

  fill_image(image_a, profile->size);
  /* A stale non-zero deadline at or below the tick is cleared by an import. */
  machine->save.busy_until = 500u;
  take_snapshot(machine, false, &before);

  T64_CHECK_INT(do_import(machine, profile->kind, T64_SAVE_FORMAT_RAW,
                          image_a, profile->size),
                T64_STATUS_OK);
  T64_CHECK(memcmp(machine->save.bytes, image_a, profile->size) == 0);
  T64_CHECK_INT(machine->save.busy_until, 0u);
  T64_CHECK_INT(machine->queue_head, before.queue_head);
  T64_CHECK_INT(machine->queue_count, before.queue_count);
  T64_CHECK_INT(machine->next_sequence, before.next_sequence);
  T64_CHECK_INT(machine->tick, before.tick);
  T64_CHECK_INT(machine->trace_count, before.trace_count);
  T64_CHECK_INT(allocator_allocations, before.allocations);
  T64_CHECK_INT(allocator_releases, before.releases);
  T64_CHECK_INT(machine->save.write_generation, before.generation + 1u);

  /* The digest equals a fresh machine given the same bytes; the generation is
     outside the digest. */
  T64_CHECK_INT(do_import(fresh, profile->kind, T64_SAVE_FORMAT_RAW, image_a,
                          profile->size),
                T64_STATUS_OK);
  digest_of(machine, digest_machine);
  digest_of(fresh, digest_fresh);
  T64_CHECK(memcmp(digest_machine, digest_fresh, T64_DIGEST_SIZE) == 0);

  /* Importing identical bytes again leaves the digest equal while the
     generation advances. */
  T64_CHECK_INT(do_import(machine, profile->kind, T64_SAVE_FORMAT_RAW,
                          image_a, profile->size),
                T64_STATUS_OK);
  digest_of(machine, digest_repeat);
  T64_CHECK(memcmp(digest_machine, digest_repeat, T64_DIGEST_SIZE) == 0);
  T64_CHECK_INT(machine->save.write_generation, 2u);

  /* Each other format stores the mapped bytes and exports the input back; the
     generation advances by exactly one per import. */
  for (format = 2u; format <= 3u; format++) {
    T64_CHECK_INT(do_import(machine, profile->kind, format, image_a,
                            profile->size),
                  T64_STATUS_OK);
    map_image(image_a, profile->size, format, expected_buffer);
    T64_CHECK(memcmp(machine->save.bytes, expected_buffer, profile->size) == 0);
    T64_CHECK_INT(do_export(machine, profile->kind, format, export_buffer,
                            profile->size, &length),
                  T64_STATUS_OK);
    T64_CHECK(memcmp(export_buffer, image_a, profile->size) == 0);
    T64_CHECK_INT(machine->save.write_generation, format + 1u);
  }

  /* Info reports kind, size, busy flag and generation. */
  poison_save_info(&info);
  T64_CHECK_INT(t64_machine_save_info(machine, profile->kind, &info),
                T64_STATUS_OK);
  T64_CHECK_INT(info.kind, profile->kind);
  T64_CHECK_INT(info.size_bytes, profile->size);
  T64_CHECK_INT(info.flags & T64_SAVE_INFO_BUSY, 0u);
  T64_CHECK_INT(info.write_generation, 4u);

  /* Export is always allowed and returns the committed bytes, also while the
     deadline is in the future; the import is refused then. */
  machine->save.busy_until = machine->tick + 500u;
  poison_save_info(&info);
  T64_CHECK_INT(t64_machine_save_info(machine, profile->kind, &info),
                T64_STATUS_OK);
  T64_CHECK_INT(info.flags & T64_SAVE_INFO_BUSY, T64_SAVE_INFO_BUSY);
  T64_CHECK_INT(do_export(machine, profile->kind, T64_SAVE_FORMAT_RAW,
                          export_buffer, profile->size, &length),
                T64_STATUS_OK);
  T64_CHECK(memcmp(export_buffer, machine->save.bytes, profile->size) == 0);
  T64_CHECK_INT(do_import(machine, profile->kind, T64_SAVE_FORMAT_RAW,
                          image_a, profile->size),
                T64_STATUS_SAVE_BUSY);
  T64_CHECK_INT(machine->save.write_generation, 4u);

  /* Two machines of the same profile stay isolated. */
  T64_CHECK(snapshot_unchanged("isolated machine", other, true, &other_before));
  t64_machine_destroy(machine);
  t64_machine_destroy(fresh);
  t64_machine_destroy(other);
}

static void case_import_export_roundtrip(void) {
  size_t row;

  for (row = 1u; row < PROFILE_ROWS; row++) {
    check_success_postconditions(&profile_rows[row]);
  }
}

/* ------------------------------------------------------------------------ */
/* refusal-matrix                                                            */
/* ------------------------------------------------------------------------ */

#define LENGTH_SENTINEL ((size_t)0x5EEDC0DEu)

static size_t matrix_calls;

/* One call of the matrix, checked against the rule function and the
 * postconditions. Returns the violated row (-1 for none). */
static int matrix_call(t64_machine *machine, const profile_row *profile,
                       bool admitted, bool busy, bool is_export,
                       pointer_class pointer, uint32_t kind, uint32_t format,
                       size_t length, const snapshot *before, bool full,
                       const char *label) {
  request r;
  int row;
  t64_status want;
  t64_status got;
  size_t length_out = LENGTH_SENTINEL;
  uint8_t *buffer = NULL;

  (void)memset(&r, 0, sizeof r);
  r.is_export = is_export;
  r.admitted = admitted;
  r.pointer = pointer;
  r.length = length;
  r.profile_kind = profile->kind;
  r.size = profile->size;
  r.kind = kind;
  r.format = format;
  r.busy = busy;
  if (pointer == POINTER_VALID) {
    buffer = is_export ? export_buffer : image_a;
  } else if (pointer == POINTER_OVERLAP) {
    buffer = machine->ram + 0x2000u;
  }
  r.address = (uintptr_t)buffer;
  row = first_violated_row(&r);
  if (row < 0) return -1;
  want = expected_status(&r, row);
  if (is_export) {
    if (pointer == POINTER_VALID) (void)memset(export_buffer, 0xA5, 64u);
    got = do_export(machine, kind, format, buffer, length, &length_out);
  } else {
    got = do_import(machine, kind, format, buffer, length);
  }
  if (got != want) {
    fprintf(stderr,
            "%s: %s id=%s admitted=%d busy=%d pointer=%d kind=%u format=%u "
            "length=%zu: got %u want %u (row R%d)\n",
            label, is_export ? "export" : "import", profile->id, admitted,
            busy, (int)pointer, (unsigned)kind, (unsigned)format, length,
            (unsigned)got, (unsigned)want, row);
  }
  T64_CHECK_INT(got, want);
  if (is_export) {
    const bool too_small = row == 8;
    T64_CHECK(length_out == (too_small ? profile->size : LENGTH_SENTINEL));
    if (pointer == POINTER_VALID) {
      size_t at;
      bool untouched = true;
      for (at = 0u; at < 64u; at++) untouched = untouched && export_buffer[at] == 0xA5u;
      T64_CHECK(untouched);
    }
  }
  T64_CHECK(snapshot_unchanged(label, machine, full, before));
  return row;
}

static void run_matrix_group(const profile_row *profile, bool admitted,
                             bool busy) {
  static snapshot before;
  t64_machine *machine = admitted ? make_idle_machine(profile->id)
                                  : create_unadmitted(profile->id);
  const size_t ok_length = profile->size != 0u ? profile->size : 512u;
  const uint32_t ok_kind = profile->kind != 0u ? profile->kind : 3u;
  const uint32_t mismatch_kind = profile->kind == 1u ? 2u : 1u;
  const uint32_t kinds[4] = {ok_kind, 99u, 4u, mismatch_kind};
  const uint32_t formats[2] = {T64_SAVE_FORMAT_RAW, 99u};
  bool row_seen[10] = {false};
  size_t row_counts[10] = {0u};
  const char *label = "matrix";
  size_t pointer;
  size_t kind_index;
  size_t format_index;
  size_t length_index;
  size_t export_index;

  if (machine == NULL) return;
  fill_image(image_a, T64_SAVE_SRAM_BYTES);
  if (admitted && profile->kind != 0u) {
    /* The accepted baseline of the same setup: it succeeds before any
       refusal, in both directions. */
    size_t length = 0u;

    T64_CHECK_INT(do_import(machine, profile->kind, T64_SAVE_FORMAT_RAW,
                            image_a, profile->size),
                  T64_STATUS_OK);
    T64_CHECK_INT(do_export(machine, profile->kind, T64_SAVE_FORMAT_RAW,
                            export_buffer, profile->size, &length),
                  T64_STATUS_OK);
    T64_CHECK(memcmp(export_buffer, image_a, profile->size) == 0);
  }
  if (busy) machine->save.busy_until = machine->tick + 1000u;
  /* An unadmitted machine has no digest (NOT_READY): its comparison state is
     the structure checksum and the explicit items. */
  take_snapshot(machine, admitted, &before);

  for (pointer = 0u; pointer < 3u; pointer++) {
    for (kind_index = 0u; kind_index < 4u; kind_index++) {
      for (format_index = 0u; format_index < 2u; format_index++) {
        for (length_index = 0u; length_index < 2u; length_index++) {
          for (export_index = 0u; export_index < 2u; export_index++) {
            const bool is_export = export_index == 1u;
            const size_t length =
                length_index == 0u ? ok_length
                                   : (is_export ? ok_length - 1u
                                                : ok_length + 1u);
            int row = matrix_call(machine, profile, admitted, busy, is_export,
                                  (pointer_class)pointer, kinds[kind_index],
                                  formats[format_index], length, &before,
                                  false, label);
            if (row >= 0) {
              row_counts[row]++;
              matrix_calls++;
            }
            if (row >= 0 && !row_seen[row]) {
              /* The full digest is checked on the first call of each row. */
              row_seen[row] = true;
              T64_CHECK(snapshot_unchanged(label, machine, admitted,
                                           &before));
            }
          }
        }
      }
    }
  }
  T64_CHECK(snapshot_unchanged(label, machine, admitted, &before));
  /* Every intended row is reached: a green run alone does not show that the
     cases ran. An unadmitted machine reaches R1 only; the no-device id R2 and
     R3; a save-capable id R2 and R4 to R8 (R3 needs no device), and R9 only
     while the window is open. */
  {
    size_t row;

    for (row = 1u; row < 10u; row++) {
      bool reached;

      if (!admitted) {
        reached = row == 1u;
      } else if (profile->kind == 0u) {
        reached = row == 2u || row == 3u;
      } else {
        reached = row >= 2u && row != 3u && (row < 9u || busy);
      }
      if (reached != (row_counts[row] != 0u)) {
        fprintf(stderr,
                "matrix %s admitted=%d busy=%d: row R%zu reached %zu times\n",
                profile->id, admitted, busy, row, row_counts[row]);
      }
      T64_CHECK(reached == (row_counts[row] != 0u));
    }
  }
  t64_machine_destroy(machine);
}

static void case_refusal_matrix(void) {
  size_t row;

  for (row = 0u; row < PROFILE_ROWS; row++) {
    run_matrix_group(&profile_rows[row], true, false);
    run_matrix_group(&profile_rows[row], true, true);
    run_matrix_group(&profile_rows[row], false, false);
    run_matrix_group(&profile_rows[row], false, true);
  }
  /* Row R0: a NULL machine. */
  T64_CHECK_INT(t64_machine_save_import(NULL, 3u, 1u, image_a, 32768u),
                D24_ORDER[0].status);
  T64_CHECK_INT(t64_machine_save_export(NULL, 3u, 1u, export_buffer, 32768u,
                                        &(size_t){0u}),
                D24_ORDER[0].status);
  printf("refusal-matrix: refusal requests checked=%zu\n", matrix_calls);
}

/* ------------------------------------------------------------------------ */
/* no-caller-read                                                            */
/* ------------------------------------------------------------------------ */

typedef struct {
  const char *label;
  const char *id;
  bool is_export;
  uint32_t kind;
  uint32_t format;
  size_t length;
  bool busy;
  t64_status want;
} guard_row;

#define GUARD_MAPPING_BYTES 40960u

/* Runs in a forked child: an accepted baseline with a readable span, then the
 * refusal row against a PROT_NONE span. The child has no signal handler, so a
 * read or write of the guard span ends it with SIGSEGV or SIGBUS. */
static int guard_child(const guard_row *row) {
  uint8_t *guard;
  t64_machine *machine;
  size_t length_out = 0u;
  t64_status status;

  guard = (uint8_t *)mmap(NULL, GUARD_MAPPING_BYTES, PROT_NONE,
                          MAP_PRIVATE | MAP_ANON, -1, 0);
  if (guard == (uint8_t *)MAP_FAILED) return 3;
  machine = create_admitted(row->id);
  if (machine == NULL) return 4;
  if (strcmp(row->id, STEM_ID) != 0) {
    fill_image(image_a, T64_SAVE_SRAM_BYTES);
    if (do_import(machine, 3u, T64_SAVE_FORMAT_RAW, image_a,
                  T64_SAVE_SRAM_BYTES) != T64_STATUS_OK ||
        do_export(machine, 3u, T64_SAVE_FORMAT_RAW, export_buffer,
                  T64_SAVE_SRAM_BYTES, &length_out) != T64_STATUS_OK) {
      return 5;
    }
  }
  if (row->busy) machine->save.busy_until = machine->tick + 1000u;
  status = row->is_export
               ? do_export(machine, row->kind, row->format, guard,
                           row->length, &length_out)
               : do_import(machine, row->kind, row->format, guard,
                           row->length);
  return status == row->want ? 0 : 6;
}

static void case_no_caller_read(void) {
  static const guard_row rows[] = {
      {"import R3", STEM_ID, false, 3u, 1u, 32768u, false,
       T64_STATUS_SAVE_DEVICE_MISMATCH},
      {"import R4", S32K_ID, false, 99u, 1u, 32768u, false,
       T64_STATUS_SAVE_UNKNOWN_DEVICE},
      {"import R5", S32K_ID, false, 4u, 1u, 32768u, false,
       T64_STATUS_SAVE_UNSUPPORTED_DEVICE},
      {"import R6", S32K_ID, false, 1u, 1u, 32768u, false,
       T64_STATUS_SAVE_DEVICE_MISMATCH},
      {"import R7", S32K_ID, false, 3u, 99u, 32768u, false,
       T64_STATUS_SAVE_BAD_FORMAT},
      {"import R8 long", S32K_ID, false, 3u, 1u, 32769u, false,
       T64_STATUS_SAVE_SIZE_MISMATCH},
      {"import R8 short", S32K_ID, false, 3u, 1u, 32767u, false,
       T64_STATUS_SAVE_SIZE_MISMATCH},
      {"import R9", S32K_ID, false, 3u, 1u, 32768u, true,
       T64_STATUS_SAVE_BUSY},
      {"export R3", STEM_ID, true, 3u, 1u, 32768u, false,
       T64_STATUS_SAVE_DEVICE_MISMATCH},
      {"export R4", S32K_ID, true, 99u, 1u, 32768u, false,
       T64_STATUS_SAVE_UNKNOWN_DEVICE},
      {"export R5", S32K_ID, true, 4u, 1u, 32768u, false,
       T64_STATUS_SAVE_UNSUPPORTED_DEVICE},
      {"export R6", S32K_ID, true, 1u, 1u, 32768u, false,
       T64_STATUS_SAVE_DEVICE_MISMATCH},
      {"export R7", S32K_ID, true, 3u, 99u, 32768u, false,
       T64_STATUS_SAVE_BAD_FORMAT},
      {"export R8", S32K_ID, true, 3u, 1u, 32767u, false,
       T64_STATUS_BUFFER_TOO_SMALL},
  };
  size_t index;

  for (index = 0u; index < sizeof rows / sizeof rows[0]; index++) {
    pid_t pid = fork();
    int wait_status = 0;

    T64_CHECK(pid >= 0);
    if (pid < 0) continue;
    if (pid == 0) {
      _exit(guard_child(&rows[index]));
    }
    while (waitpid(pid, &wait_status, 0) < 0) {
    }
    if (WIFSIGNALED(wait_status)) {
      fprintf(stderr, "no-caller-read '%s': child ended by signal %d\n",
              rows[index].label, WTERMSIG(wait_status));
    } else if (!WIFEXITED(wait_status) || WEXITSTATUS(wait_status) != 0) {
      fprintf(stderr, "no-caller-read '%s': child exit status %d\n",
              rows[index].label,
              WIFEXITED(wait_status) ? WEXITSTATUS(wait_status) : -1);
    }
    T64_CHECK(WIFEXITED(wait_status) && WEXITSTATUS(wait_status) == 0);
  }
}

/* ------------------------------------------------------------------------ */
/* export-refusals                                                           */
/* ------------------------------------------------------------------------ */

static void check_export_untouched(const char *label, t64_machine *machine,
                                   const snapshot *before, t64_status want,
                                   t64_status got, size_t length_out,
                                   size_t length_want, const uint8_t *buffer,
                                   size_t sentinel_bytes) {
  size_t at;
  bool untouched = true;

  if (got != want) {
    fprintf(stderr, "export refusal '%s': got %u want %u\n", label,
            (unsigned)got, (unsigned)want);
  }
  T64_CHECK_INT(got, want);
  T64_CHECK(length_out == length_want);
  if (buffer != NULL) {
    for (at = 0u; at < sentinel_bytes; at++) {
      untouched = untouched && buffer[at] == 0xA5u;
    }
    T64_CHECK(untouched);
  }
  T64_CHECK(snapshot_unchanged(label, machine, false, before));
}

static void case_export_refusals(void) {
  static snapshot before;
  t64_machine *machine = make_idle_machine(S32K_ID);
  t64_machine *unadmitted = create_unadmitted(S32K_ID);
  t64_machine *stem = make_idle_machine(STEM_ID);
  size_t length_out;
  size_t row;
  t64_status status;

  if (machine == NULL || unadmitted == NULL || stem == NULL) {
    if (machine != NULL) t64_machine_destroy(machine);
    if (unadmitted != NULL) t64_machine_destroy(unadmitted);
    if (stem != NULL) t64_machine_destroy(stem);
    return;
  }
  fill_image(image_a, T64_SAVE_SRAM_BYTES);
  /* The accepted baseline of this setup, then one refusal per row. */
  T64_CHECK_INT(do_import(machine, 3u, T64_SAVE_FORMAT_RAW, image_a,
                          T64_SAVE_SRAM_BYTES),
                T64_STATUS_OK);
  length_out = LENGTH_SENTINEL;
  (void)memset(export_buffer, 0xA5, sizeof export_buffer);
  T64_CHECK_INT(do_export(machine, 3u, T64_SAVE_FORMAT_RAW, export_buffer,
                          T64_SAVE_SRAM_BYTES, &length_out),
                T64_STATUS_OK);
  T64_CHECK_INT(length_out, T64_SAVE_SRAM_BYTES);
  T64_CHECK(memcmp(export_buffer, image_a, T64_SAVE_SRAM_BYTES) == 0);
  take_snapshot(machine, false, &before);

#define EXPORT_ROW(label, call, want, length_want)                           \
  do {                                                                       \
    length_out = LENGTH_SENTINEL;                                            \
    (void)memset(export_buffer, 0xA5, sizeof export_buffer);                 \
    status = (call);                                                         \
    check_export_untouched(label, machine, &before, (want), status,          \
                           length_out, (length_want), export_buffer, 64u);   \
  } while (0)

  /* R0, R1 */
  length_out = LENGTH_SENTINEL;
  status = do_export(NULL, 3u, 1u, export_buffer, 32768u, &length_out);
  T64_CHECK_INT(status, D24_ORDER[0].status);
  T64_CHECK(length_out == LENGTH_SENTINEL);
  length_out = LENGTH_SENTINEL;
  status = do_export(unadmitted, 3u, 1u, export_buffer, 32768u, &length_out);
  T64_CHECK_INT(status, D24_ORDER[1].status);
  T64_CHECK(length_out == LENGTH_SENTINEL);
  /* R2 */
  length_out = LENGTH_SENTINEL;
  status = do_export(machine, 3u, 1u, export_buffer, 32768u, NULL);
  T64_CHECK_INT(status, D24_ORDER[2].status);
  EXPORT_ROW("R2 NULL buffer with capacity",
             do_export(machine, 3u, 1u, NULL, 1u, &length_out),
             D24_ORDER[2].status, LENGTH_SENTINEL);
  EXPORT_ROW("R2 buffer in machine storage",
             do_export(machine, 3u, 1u, machine->ram + 0x2000u, 32768u,
                       &length_out),
             D24_ORDER[2].status, LENGTH_SENTINEL);
  EXPORT_ROW("R2 length_out in machine storage",
             do_export(machine, 3u, 1u, export_buffer, 32768u,
                       (size_t *)(void *)(machine->ram + 0x2000u)),
             D24_ORDER[2].status, LENGTH_SENTINEL);
  /* A buffer that contains length_out overlaps it. */
  EXPORT_ROW("R2 buffer overlapping length_out",
             do_export(machine, 3u, 1u, export_buffer, 32768u,
                       (size_t *)(void *)(export_buffer + 4096u)),
             D24_ORDER[2].status, LENGTH_SENTINEL);
  EXPORT_ROW("R2 span arithmetic overflow",
             do_export(machine, 3u, 1u, export_buffer, SIZE_MAX, &length_out),
             D24_ORDER[2].status, LENGTH_SENTINEL);
  /* R3 to R7 */
  length_out = LENGTH_SENTINEL;
  (void)memset(export_buffer, 0xA5, sizeof export_buffer);
  status = do_export(stem, 3u, 1u, export_buffer, 32768u, &length_out);
  T64_CHECK_INT(status, D24_ORDER[3].status);
  T64_CHECK(length_out == LENGTH_SENTINEL);
  EXPORT_ROW("R4 unknown kind",
             do_export(machine, 99u, 1u, export_buffer, 32768u, &length_out),
             D24_ORDER[4].status, LENGTH_SENTINEL);
  EXPORT_ROW("R4 kind zero",
             do_export(machine, 0u, 1u, export_buffer, 32768u, &length_out),
             D24_ORDER[4].status, LENGTH_SENTINEL);
  EXPORT_ROW("R5 unsupported kind",
             do_export(machine, 4u, 1u, export_buffer, 32768u, &length_out),
             D24_ORDER[5].status, LENGTH_SENTINEL);
  EXPORT_ROW("R6 mismatched kind",
             do_export(machine, 1u, 1u, export_buffer, 32768u, &length_out),
             D24_ORDER[6].status, LENGTH_SENTINEL);
  EXPORT_ROW("R7 bad format",
             do_export(machine, 3u, 99u, export_buffer, 32768u, &length_out),
             D24_ORDER[7].status, LENGTH_SENTINEL);
  EXPORT_ROW("R7 format zero",
             do_export(machine, 3u, 0u, export_buffer, 32768u, &length_out),
             D24_ORDER[7].status, LENGTH_SENTINEL);
  /* R8: too small, for each capacity, reports the required length. */
  for (row = 0u; row < 3u; row++) {
    const size_t capacity = row == 0u ? 0u : (row == 1u ? 1u : 32767u);

    EXPORT_ROW("R8 capacity below the size",
               do_export(machine, 3u, 1u, export_buffer, capacity,
                         &length_out),
               T64_STATUS_BUFFER_TOO_SMALL, T64_SAVE_SRAM_BYTES);
  }
  EXPORT_ROW("R8 NULL buffer queries the length",
             do_export(machine, 3u, 1u, NULL, 0u, &length_out),
             T64_STATUS_BUFFER_TOO_SMALL, T64_SAVE_SRAM_BYTES);
  /* A zero capacity constructs no span, wherever the pointer points. */
  EXPORT_ROW("R8 empty span inside machine storage",
             do_export(machine, 3u, 1u, machine->ram + 0x2000u, 0u,
                       &length_out),
             T64_STATUS_BUFFER_TOO_SMALL, T64_SAVE_SRAM_BYTES);
#undef EXPORT_ROW

  /* The required length is the kind's own size on every kind. */
  for (row = 1u; row < PROFILE_ROWS; row++) {
    t64_machine *sized = make_idle_machine(profile_rows[row].id);

    if (sized == NULL) continue;
    length_out = LENGTH_SENTINEL;
    T64_CHECK_INT(do_export(sized, profile_rows[row].kind, 1u, export_buffer,
                            profile_rows[row].size - 1u, &length_out),
                  T64_STATUS_BUFFER_TOO_SMALL);
    T64_CHECK(length_out == profile_rows[row].size);
    length_out = LENGTH_SENTINEL;
    T64_CHECK_INT(do_export(sized, profile_rows[row].kind, 1u, export_buffer,
                            profile_rows[row].size, &length_out),
                  T64_STATUS_OK);
    T64_CHECK(length_out == profile_rows[row].size);
    t64_machine_destroy(sized);
  }
  t64_machine_destroy(machine);
  t64_machine_destroy(unadmitted);
  t64_machine_destroy(stem);
}

/* ------------------------------------------------------------------------ */
/* in-flight-real                                                            */
/* ------------------------------------------------------------------------ */

#define SI_DRAM_ADDR UINT32_C(0x04800000)
#define SI_RD64B UINT32_C(0x04800004)
#define SI_WR64B UINT32_C(0x04800010)
#define SI_STATUS UINT32_C(0x04800018)
#define PIF_ADDRESS UINT32_C(0x1FC007C0)
#define SEND_DRAM UINT32_C(0x3800)
#define RECEIVE_DRAM UINT32_C(0x3900)
#define PI_DRAM UINT32_C(0x04600000)
#define PI_CART UINT32_C(0x04600004)
#define PI_RD_LEN UINT32_C(0x04600008)
#define PI_STATUS UINT32_C(0x04600010)
#define DMA_DRAM UINT32_C(0x3800)
#define DMA_CART UINT32_C(0x08000100)

/* Literal policy figures (named policy, written out here): the handshake total
 * of a write frame on channel 4 is 2000 * 10 + 188 + 438 + 2000 * 1 + 250 =
 * 22,876, the read tail 4,096, the WR64B completion 2,500 and the busy window
 * 375,000. */
#define H_WRITE UINT64_C(22876)
#define READ_TAIL UINT64_C(4096)
#define WRITE_DMA_TICKS UINT64_C(2500)
#define BUSY_WINDOW UINT64_C(375000)

static void build_write_frame(uint8_t frame[64], const uint8_t tx[10]) {
  (void)memset(frame, 0, 64u);
  frame[4] = 10u;
  frame[5] = 1u;
  (void)memcpy(frame + 6, tx, 10u);
  frame[16] = 0xFFu;
  frame[17] = 0xFEu;
  frame[63] = 0x01u;
}

static t64_status bus_w32(t64_machine *machine, uint32_t address,
                          uint32_t value) {
  return t64_machine_bus_write(machine, address, 4u, value);
}

static uint32_t bus_value(t64_machine *machine, uint32_t address) {
  uint32_t value = UINT32_MAX;

  T64_CHECK_INT(t64_machine_bus_read(machine, address, 4u, &value),
                T64_STATUS_OK);
  return value;
}

/* A complete accepted WR64B of the frame at the current tick (move into the
 * PIF RAM, parse, settle the completion, acknowledge), then an RD64B started
 * at `read_start` that settles at the machine's completion tick. */
static void run_write_transaction(t64_machine *machine, const uint8_t frame[64],
                                  uint64_t read_start) {
  const uint64_t start = machine->tick;

  (void)memcpy(machine->ram + SEND_DRAM, frame, 64u);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_DRAM_ADDR, 4u, SEND_DRAM),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_WR64B, 4u, PIF_ADDRESS),
                T64_STATUS_OK);
  T64_CHECK(advance_guest_to(machine, start + WRITE_DMA_TICKS));
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_STATUS, 4u, 0u),
                T64_STATUS_OK);
  T64_CHECK(advance_guest_to(machine, read_start));
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_DRAM_ADDR, 4u, RECEIVE_DRAM),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_RD64B, 4u, PIF_ADDRESS),
                T64_STATUS_OK);
  T64_CHECK_INT(machine->si.execute_tick, read_start + H_WRITE);
  T64_CHECK(advance_guest_to(machine, machine->si.complete_tick));
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_STATUS, 4u, 0u),
                T64_STATUS_OK);
}

static void check_eeprom_in_flight(void) {
  static const uint8_t write_tx[10] = {0x05, 0x01, 0x11, 0x12, 0x13,
                                       0x14, 0x15, 0x16, 0x17, 0x18};
  static snapshot before;
  t64_machine *machine = make_idle_machine(E4K_ID);
  const uint64_t deadline = UINT64_C(10000) + H_WRITE + BUSY_WINDOW;
  uint8_t frame[64];
  t64_save_info info = T64_SAVE_INFO_INIT;
  size_t length = 0u;
  uint64_t generation;

  if (machine == NULL) return;
  T64_CHECK(advance_guest_to(machine, 3000u));
  build_write_frame(frame, write_tx);
  run_write_transaction(machine, frame, 10000u);
  /* Reached: the write was accepted through the real SI path and the window is
     open (deadline 407,876), the tick is far below it. */
  T64_CHECK_INT(machine->save.busy_until, deadline);
  T64_CHECK_INT(deadline, 407876u);
  T64_CHECK(machine->tick < deadline);
  fill_image(image_a, 512u);
  generation = machine->save.write_generation;

  T64_CHECK(advance_guest_to(machine, deadline - 1u));
  T64_CHECK(machine->tick < machine->save.busy_until);
  take_snapshot(machine, true, &before);
  T64_CHECK_INT(do_import(machine, 1u, T64_SAVE_FORMAT_RAW, image_a, 512u),
                T64_STATUS_SAVE_BUSY);
  T64_CHECK(snapshot_unchanged("eeprom import at deadline minus one", machine,
                               true, &before));
  poison_save_info(&info);
  T64_CHECK_INT(t64_machine_save_info(machine, 1u, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.flags & T64_SAVE_INFO_BUSY, T64_SAVE_INFO_BUSY);
  /* Export is allowed and returns the committed bytes: the block written
     through the SI is at offset 8. */
  T64_CHECK_INT(do_export(machine, 1u, T64_SAVE_FORMAT_RAW, export_buffer,
                          512u, &length),
                T64_STATUS_OK);
  T64_CHECK(memcmp(export_buffer + 8, write_tx + 2, 8u) == 0);

  T64_CHECK(advance_guest_to(machine, deadline));
  T64_CHECK_INT(machine->save.busy_until, machine->tick);
  T64_CHECK_INT(do_import(machine, 1u, T64_SAVE_FORMAT_RAW, image_a, 512u),
                T64_STATUS_OK);
  T64_CHECK_INT(machine->save.busy_until, 0u);
  T64_CHECK_INT(machine->save.write_generation, generation + 1u);
  T64_CHECK(memcmp(machine->save.bytes, image_a, 512u) == 0);
  t64_machine_destroy(machine);
}

static void check_sram_in_flight(void) {
  static snapshot before;
  t64_machine *machine = make_idle_machine(S32K_ID);
  size_t length = 0u;
  size_t at;
  bool blank = true;

  if (machine == NULL) return;
  fill_image(image_a, T64_SAVE_SRAM_BYTES);
  T64_CHECK(advance_guest_to(machine, 1000u));
  (void)memset(machine->ram + DMA_DRAM, 0x77, 16u);
  T64_CHECK_INT(bus_w32(machine, PI_DRAM, DMA_DRAM), T64_STATUS_OK);
  T64_CHECK_INT(bus_w32(machine, PI_CART, DMA_CART), T64_STATUS_OK);
  T64_CHECK_INT(bus_w32(machine, PI_RD_LEN, 15u), T64_STATUS_OK);
  /* Reached: a DMA to the SRAM window is active through the real PI path. */
  T64_CHECK(machine->pi.active_valid);
  T64_CHECK_INT(machine->pi.active_save_direction, 2u);
  T64_CHECK_INT(bus_value(machine, PI_STATUS), 1u);

  T64_CHECK(advance_guest_to(machine, 1000u + 8u));
  T64_CHECK_INT(bus_value(machine, PI_STATUS), 1u);
  take_snapshot(machine, true, &before);
  T64_CHECK_INT(do_import(machine, 3u, T64_SAVE_FORMAT_RAW, image_a,
                          T64_SAVE_SRAM_BYTES),
                T64_STATUS_SAVE_BUSY);
  T64_CHECK(snapshot_unchanged("sram import during a DMA", machine, true,
                               &before));
  /* Export returns the pre-DMA bytes: the effect is atomic at completion. */
  T64_CHECK_INT(do_export(machine, 3u, T64_SAVE_FORMAT_RAW, export_buffer,
                          T64_SAVE_SRAM_BYTES, &length),
                T64_STATUS_OK);
  for (at = 0u; at < 16u; at++) {
    blank = blank && export_buffer[0x100u + at] == 0u;
  }
  T64_CHECK(blank);

  T64_CHECK(advance_guest_to(machine, 1000u + 9u));
  T64_CHECK_INT(bus_value(machine, PI_STATUS), 0u);
  T64_CHECK(!machine->pi.active_valid);
  T64_CHECK_INT(do_import(machine, 3u, T64_SAVE_FORMAT_RAW, image_a,
                          T64_SAVE_SRAM_BYTES),
                T64_STATUS_OK);
  T64_CHECK(memcmp(machine->save.bytes, image_a, T64_SAVE_SRAM_BYTES) == 0);
  t64_machine_destroy(machine);
}

static void case_in_flight_real(void) {
  check_eeprom_in_flight();
  check_sram_in_flight();
}

/* ------------------------------------------------------------------------ */
/* replacement                                                               */
/* ------------------------------------------------------------------------ */

static void check_replacement(const profile_row *profile) {
  static snapshot refused_before;
  t64_machine *machine = create_admitted(profile->id);
  t64_machine *fresh = create_admitted(profile->id);
  uint8_t asset[IPL3_ASSET_BYTES];
  uint8_t digest_machine[T64_DIGEST_SIZE];
  uint8_t digest_fresh[T64_DIGEST_SIZE];
  size_t at;

  if (machine == NULL || fresh == NULL) {
    if (machine != NULL) t64_machine_destroy(machine);
    if (fresh != NULL) t64_machine_destroy(fresh);
    return;
  }
  T64_CHECK(read_openipl3_asset(asset));
  fill_image(image_a, profile->size);
  T64_CHECK_INT(do_import(machine, profile->kind, T64_SAVE_FORMAT_RAW,
                          image_a, profile->size),
                T64_STATUS_OK);
  /* A bus write dirties the save too: a CPU word store in the SRAM window; on
     the EEPROM ids the open window is a deadline set directly. */
  if (profile->kind == T64_SAVE_SRAM_32K) {
    T64_CHECK_INT(bus_w32(machine, UINT32_C(0x08000104), UINT32_C(0xCAFEBABE)),
                  T64_STATUS_OK);
  } else {
    machine->save.busy_until = machine->tick + BUSY_WINDOW;
  }
  T64_CHECK_INT(machine->save.write_generation, 1u);
  /* The diagnostic RSP observation of the run before: a replacement is a fresh
     machine for observers as well (plan 09-10 review L1 IN-05), set directly
     because the family has no RSP activity of its own here. */
  machine->rsp_observed_pc = UINT32_C(0x123);
  machine->rsp_observed_opcode = UINT32_C(0x4567);
  machine->rsp_observed_second_pc = UINT32_C(0x89a);
  machine->rsp_observed_second_opcode = UINT32_C(0xbcde);
  machine->rsp_observed_tick = UINT64_C(77);
  machine->rsp_observed_ready_tick = UINT64_C(78);
  machine->rsp_observed_dma_cursor = UINT64_C(79);
  machine->rsp_observed_stall_reason = 3u;
  machine->rsp_observed_refusal_reason = 2u;
  machine->rsp_observed_dma_action = 1u;
  machine->rsp_observed_outcome = 2u;
  machine->rsp_observation_valid = true;

  /* A refused admission (one asset byte altered) leaves bytes, deadline,
     generation and the digest unchanged. */
  take_snapshot(machine, true, &refused_before);
  asset[0x800] = (uint8_t)(asset[0x800] ^ 0x01u);
  T64_CHECK_INT(t64_machine_admit_rom(machine, asset, sizeof asset),
                T64_STATUS_INVALID_ROM);
  T64_CHECK(snapshot_unchanged("refused admission", machine, true,
                               &refused_before));
  asset[0x800] = (uint8_t)(asset[0x800] ^ 0x01u);

  /* A successful replacement restores blank content, clears the deadline (also
     while the window is open) and the generation, and the digest equals a
     freshly created and admitted machine. */
  T64_CHECK_INT(t64_machine_admit_rom(machine, asset, sizeof asset),
                T64_STATUS_OK);
  T64_CHECK_INT(machine->save.busy_until, 0u);
  T64_CHECK_INT(machine->save.write_generation, 0u);
  T64_CHECK(!machine->rsp_observation_valid);
  T64_CHECK(machine->rsp_observed_pc == 0u && machine->rsp_observed_opcode == 0u &&
            machine->rsp_observed_second_pc == 0u &&
            machine->rsp_observed_second_opcode == 0u &&
            machine->rsp_observed_tick == 0u &&
            machine->rsp_observed_ready_tick == 0u &&
            machine->rsp_observed_dma_cursor == 0u &&
            machine->rsp_observed_stall_reason == 0u &&
            machine->rsp_observed_refusal_reason == 0u &&
            machine->rsp_observed_dma_action == 0u &&
            machine->rsp_observed_outcome == 0u);
  for (at = 0u; at < profile->size; at++) {
    if (machine->save.bytes[at] != profile->blank) {
      T64_CHECK_INT(machine->save.bytes[at], profile->blank);
      break;
    }
  }
  digest_of(machine, digest_machine);
  digest_of(fresh, digest_fresh);
  T64_CHECK(memcmp(digest_machine, digest_fresh, T64_DIGEST_SIZE) == 0);
  t64_machine_destroy(machine);
  t64_machine_destroy(fresh);
}

/* The generation saturates at the 64-bit maximum on both writers: a host import
 * and a state import (the shared helper of plan 09-10, review L5 IN-05). The
 * state is set directly because 2^64 imports are unreachable. */
static void check_generation_saturation(const profile_row *profile) {
  t64_machine *machine = create_admitted(profile->id);

  if (machine == NULL) return;
  fill_image(image_a, profile->size);
  machine->save.write_generation = UINT64_MAX - 1u;
  T64_CHECK_INT(do_import(machine, profile->kind, T64_SAVE_FORMAT_RAW, image_a,
                          profile->size),
                T64_STATUS_OK);
  T64_CHECK_INT(machine->save.write_generation, UINT64_MAX);
  T64_CHECK_INT(do_import(machine, profile->kind, T64_SAVE_FORMAT_RAW, image_a,
                          profile->size),
                T64_STATUS_OK);
  T64_CHECK_INT(machine->save.write_generation, UINT64_MAX);
  t64_save_generation_advance(machine);
  T64_CHECK_INT(machine->save.write_generation, UINT64_MAX);
  t64_machine_destroy(machine);
}

static void case_replacement(void) {
  size_t row;

  for (row = 1u; row < PROFILE_ROWS; row++) {
    check_replacement(&profile_rows[row]);
    check_generation_saturation(&profile_rows[row]);
  }
}



/* ------------------------------------------------------------------------ */
/* mutation-bounded                                                          */
/* ------------------------------------------------------------------------ */

/* The campaign size follows the build: 2,000 requests in the gate preset and
 * 20,000 under AddressSanitizer. */
#if defined(__SANITIZE_ADDRESS__)
#define CAMPAIGN_SANITIZED 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define CAMPAIGN_SANITIZED 1
#endif
#endif
#ifdef CAMPAIGN_SANITIZED
#define CAMPAIGN_REQUESTS 20000u
#else
#define CAMPAIGN_REQUESTS 2000u
#endif
#define CAMPAIGN_SEED UINT32_C(0x5a7e3d09)

typedef enum {
  STATE_IDLE = 0,
  STATE_STALE = 1,
  STATE_FUTURE = 2,
  STATE_DMA = 3
} state_class;

typedef struct {
  size_t profile;
  bool admitted;
  bool is_export;
  pointer_class pointer;
  uint32_t kind;
  uint32_t format;
  size_t length;
  state_class state;
} params;

typedef struct {
  t64_machine *admitted[PROFILE_ROWS];
  t64_machine *unadmitted[PROFILE_ROWS];
  uint8_t asset[IPL3_ASSET_BYTES];
  size_t rows[10];       /* violated-row counts; index 0 counts successes */
  size_t requests;
  size_t resets;
} pool;

/* An admitted machine with its CPU event cancelled and the tick at 1,000 (a
 * stale deadline of 500 is then at or below the tick). */
static bool reidle(t64_machine *machine) {
  uint64_t cancelled_tick = UINT64_MAX;

  if (t64_schedule_cancel(machine, T64_DEVICE_CPU, T64_CPU_EVENT_STEP, 0u,
                          &cancelled_tick) != T64_STATUS_OK) {
    return false;
  }
  return advance_guest_to(machine, 1000u);
}

static bool pool_init(pool *p) {
  size_t index;

  (void)memset(p, 0, sizeof *p);
  if (!read_openipl3_asset(p->asset)) return false;
  for (index = 0u; index < PROFILE_ROWS; index++) {
    p->admitted[index] = make_idle_machine(profile_rows[index].id);
    p->unadmitted[index] = create_unadmitted(profile_rows[index].id);
    if (p->admitted[index] == NULL || p->unadmitted[index] == NULL ||
        !advance_guest_to(p->admitted[index], 1000u)) {
      return false;
    }
  }
  fill_image(image_a, T64_SAVE_SRAM_BYTES);
  return true;
}

static void pool_destroy(pool *p) {
  size_t index;

  for (index = 0u; index < PROFILE_ROWS; index++) {
    if (p->admitted[index] != NULL) t64_machine_destroy(p->admitted[index]);
    if (p->unadmitted[index] != NULL) t64_machine_destroy(p->unadmitted[index]);
  }
}

/* One request against the rule function. Every refusal leaves the compared
 * state bit-identical; every success is verified against the independent
 * mapping and followed by a replacement-based reset to a known state. */
static void execute_request(pool *p, const params *q) {
  static snapshot before;
  const profile_row *profile = &profile_rows[q->profile];
  t64_machine *machine =
      q->admitted ? p->admitted[q->profile] : p->unadmitted[q->profile];
  uint8_t *buffer = NULL;
  size_t length_out = LENGTH_SENTINEL;
  const bool sram = profile->kind == T64_SAVE_SRAM_32K;
  bool busy = false;
  request r;
  t64_status got;
  t64_status want;
  int row;

  /* The state class, edited directly (the N6 pattern). */
  machine->save.busy_until = 0u;
  machine->pi.active_valid = false;
  machine->pi.active_save_direction = T64_PI_DIRECTION_ROM_TO_RDRAM;
  if (q->admitted && q->state == STATE_STALE) {
    machine->save.busy_until = 500u;
  } else if (q->state == STATE_FUTURE) {
    machine->save.busy_until = machine->tick + 1000u;
    busy = true;
  } else if (q->state == STATE_DMA && sram) {
    machine->pi.active_valid = true;
    machine->pi.active_save_direction = T64_PI_DIRECTION_RDRAM_TO_SRAM;
    busy = true;
  }
  if (q->pointer == POINTER_VALID) {
    buffer = q->is_export ? export_buffer : image_a;
  } else if (q->pointer == POINTER_OVERLAP) {
    buffer = machine->ram + 0x2000u;
  }
  (void)memset(&r, 0, sizeof r);
  r.is_export = q->is_export;
  r.admitted = q->admitted;
  r.pointer = q->pointer;
  r.address = (uintptr_t)buffer;
  r.length = q->length;
  r.profile_kind = profile->kind;
  r.size = profile->size;
  r.kind = q->kind;
  r.format = q->format;
  r.busy = busy;
  row = first_violated_row(&r);
  want = expected_status(&r, row);
  take_snapshot(machine, false, &before);
  if (q->is_export) {
    if (q->pointer == POINTER_VALID) (void)memset(export_buffer, 0xA5, 64u);
    got = do_export(machine, q->kind, q->format, buffer, q->length,
                    &length_out);
  } else {
    got = do_import(machine, q->kind, q->format, buffer, q->length);
  }
  p->requests++;
  p->rows[row < 0 ? 0 : (size_t)row]++;
  if (got != want) {
    fprintf(stderr,
            "mutation-bounded: %s id=%s admitted=%d pointer=%d kind=%u "
            "format=%u length=%zu state=%d: got %u want %u (row R%d)\n",
            q->is_export ? "export" : "import", profile->id, q->admitted,
            (int)q->pointer, (unsigned)q->kind, (unsigned)q->format,
            q->length, (int)q->state, (unsigned)got, (unsigned)want, row);
  }
  T64_CHECK_INT(got, want);
  if (row >= 0) {
    if (q->is_export) {
      T64_CHECK(length_out == (row == 8 ? profile->size : LENGTH_SENTINEL));
    }
    T64_CHECK(snapshot_unchanged("campaign refusal", machine, false, &before));
    return;
  }
  /* A success: verify against the independent mapping, then reset. */
  if (q->is_export) {
    T64_CHECK(length_out == profile->size);
    map_image(machine->save.bytes, profile->size, q->format, expected_buffer);
    T64_CHECK(memcmp(export_buffer, expected_buffer, profile->size) == 0);
    T64_CHECK(snapshot_unchanged("campaign export", machine, false, &before));
  } else {
    map_image(image_a, profile->size, q->format, expected_buffer);
    T64_CHECK(memcmp(machine->save.bytes, expected_buffer, profile->size) == 0);
    T64_CHECK_INT(machine->save.busy_until, 0u);
    T64_CHECK_INT(machine->save.write_generation, before.generation + 1u);
    T64_CHECK_INT(machine->tick, before.tick);
    T64_CHECK_INT(machine->queue_count, before.queue_count);
    T64_CHECK_INT(machine->next_sequence, before.next_sequence);
    T64_CHECK_INT(machine->trace_count, before.trace_count);
  }
  /* Replacement-based reset: the same asset restores a blank, idle machine. */
  T64_CHECK_INT(t64_machine_admit_rom(machine, p->asset, IPL3_ASSET_BYTES),
                T64_STATUS_OK);
  T64_CHECK(reidle(machine));
  p->resets++;
}

static uint32_t campaign_state = CAMPAIGN_SEED;

static uint32_t next_random(void) {
  uint32_t x = campaign_state;

  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  campaign_state = x;
  return x;
}

static void case_mutation_bounded(void) {
  static pool machines;
  static const uint32_t kind_values[] = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u,
                                         99u, UINT32_C(0x7FFFFFFF),
                                         UINT32_C(0xFFFFFFFF)};
  static const uint32_t format_values[] = {0u, 1u, 2u, 3u, 4u, 99u,
                                           UINT32_C(0xFFFFFFFF)};
  size_t sweep_requests = 0u;
  size_t profile_index;
  size_t index;
  size_t row;

  if (!pool_init(&machines)) {
    T64_CHECK(false);
    pool_destroy(&machines);
    return;
  }
  /* Single-dimension sweeps from the accepted baseline of each id. */
  for (profile_index = 0u; profile_index < PROFILE_ROWS; profile_index++) {
    const profile_row *profile = &profile_rows[profile_index];
    const size_t ok_length = profile->size != 0u ? profile->size : 512u;
    const uint32_t ok_kind = profile->kind != 0u ? profile->kind : 3u;
    const size_t lengths[] = {0u, 1u, ok_length - 1u, ok_length,
                              ok_length + 1u, 2u * ok_length, SIZE_MAX};
    size_t direction;

    for (direction = 0u; direction < 2u; direction++) {
      params base;
      size_t admitted_index;

      (void)memset(&base, 0, sizeof base);
      base.profile = profile_index;
      base.admitted = true;
      base.is_export = direction == 1u;
      base.pointer = POINTER_VALID;
      base.kind = ok_kind;
      base.format = T64_SAVE_FORMAT_RAW;
      base.length = ok_length;
      base.state = STATE_IDLE;
      execute_request(&machines, &base);
      sweep_requests++;
      for (admitted_index = 0u; admitted_index < 2u; admitted_index++) {
        params q = base;
        size_t value;

        q.admitted = admitted_index == 0u;
        execute_request(&machines, &q);
        sweep_requests++;
        for (value = 0u; value < sizeof kind_values / sizeof kind_values[0];
             value++) {
          q = base;
          q.admitted = admitted_index == 0u;
          q.kind = kind_values[value];
          execute_request(&machines, &q);
          sweep_requests++;
        }
        for (value = 0u; value < sizeof format_values / sizeof format_values[0];
             value++) {
          q = base;
          q.admitted = admitted_index == 0u;
          q.format = format_values[value];
          execute_request(&machines, &q);
          sweep_requests++;
        }
        for (value = 0u; value < sizeof lengths / sizeof lengths[0]; value++) {
          q = base;
          q.admitted = admitted_index == 0u;
          q.length = lengths[value];
          execute_request(&machines, &q);
          sweep_requests++;
        }
        for (value = 0u; value < 4u; value++) {
          q = base;
          q.admitted = admitted_index == 0u;
          q.state = (state_class)value;
          execute_request(&machines, &q);
          sweep_requests++;
        }
        for (value = 0u; value < 3u; value++) {
          q = base;
          q.admitted = admitted_index == 0u;
          q.pointer = (pointer_class)value;
          execute_request(&machines, &q);
          sweep_requests++;
        }
      }
    }
  }
  /* The seeded campaign: combined requests drawn from the generator only; the
     expectations come from the rule function. */
  campaign_state = CAMPAIGN_SEED;
  for (index = 0u; index < CAMPAIGN_REQUESTS; index++) {
    const profile_row *profile;
    params q;
    uint32_t draw;
    size_t ok_length;

    (void)memset(&q, 0, sizeof q);
    q.profile = next_random() % PROFILE_ROWS;
    profile = &profile_rows[q.profile];
    ok_length = profile->size != 0u ? profile->size : 512u;
    q.admitted = next_random() % 16u != 0u;
    q.is_export = (next_random() & 1u) != 0u;
    draw = next_random() % 10u;
    q.pointer = draw < 8u ? POINTER_VALID
                          : (draw == 8u ? POINTER_NULL : POINTER_OVERLAP);
    draw = next_random() % 100u;
    q.kind = draw < 55u ? (profile->kind != 0u ? profile->kind : 3u)
                        : kind_values[next_random() %
                                      (sizeof kind_values /
                                       sizeof kind_values[0])];
    draw = next_random() % 100u;
    q.format = draw < 70u ? 1u + next_random() % 3u
                          : format_values[next_random() %
                                          (sizeof format_values /
                                           sizeof format_values[0])];
    draw = next_random() % 100u;
    if (draw < 65u) {
      q.length = ok_length;
    } else {
      const size_t lengths[] = {0u, 1u, ok_length - 1u, ok_length + 1u,
                                2u * ok_length, SIZE_MAX};

      q.length = lengths[next_random() % (sizeof lengths / sizeof lengths[0])];
    }
    draw = next_random() % 10u;
    q.state = draw < 6u ? STATE_IDLE
                        : (draw == 6u ? STATE_STALE
                                      : (draw < 9u ? STATE_FUTURE : STATE_DMA));
    execute_request(&machines, &q);
  }
  /* Every row was reached, and some requests succeeded. */
  for (row = 0u; row < 10u; row++) {
    if (machines.rows[row] == 0u) {
      fprintf(stderr, "mutation-bounded: row %zu never reached\n", row);
    }
    T64_CHECK(machines.rows[row] != 0u);
  }
  printf("mutation-bounded: sweeps=%zu campaign=%u total=%zu resets=%zu "
         "successes=%zu refusals by row:",
         sweep_requests, (unsigned)CAMPAIGN_REQUESTS, machines.requests,
         machines.resets, machines.rows[0]);
  for (row = 1u; row < 10u; row++) printf(" R%zu=%zu", row, machines.rows[row]);
  printf("\n");
#ifdef CAMPAIGN_SANITIZED
  printf("mutation-bounded: sanitized build, 20000-request campaign ran\n");
#endif
  pool_destroy(&machines);
}

typedef struct {
  const char *name;
  void (*run)(void);
} test_case;

static const test_case cases[] = {
    {"tracer-sram-raw", case_tracer_sram_raw},
    {"transform-vectors", case_transform_vectors},
    {"import-export-roundtrip", case_import_export_roundtrip},
    {"refusal-matrix", case_refusal_matrix},
    {"no-caller-read", case_no_caller_read},
    {"export-refusals", case_export_refusals},
    {"in-flight-real", case_in_flight_real},
    {"replacement", case_replacement},
    {"mutation-bounded", case_mutation_bounded},
};

int main(int argc, char **argv) {
  size_t index;
  const size_t count = sizeof cases / sizeof cases[0];

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (index = 0u; index < count; index++) puts(cases[index].name);
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    for (index = 0u; index < count; index++) {
      if (strcmp(cases[index].name, argv[2]) == 0) {
        cases[index].run();
        return T64_TEST_FINISH("unit.core.save.import-export");
      }
    }
    fprintf(stderr, "unknown save transfer case: %s\n", argv[2]);
    return 2;
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--case NAME]\n", argv[0]);
    return 2;
  }
  for (index = 0u; index < count; index++) {
    printf("case %s\n", cases[index].name);
    cases[index].run();
  }
  return T64_TEST_FINISH("unit.core.save.import-export");
}
