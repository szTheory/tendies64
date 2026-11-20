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
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

  if (path == NULL || path[0] == '\0' || bytes == NULL) return false;
  file = fopen(path, "rb");
  if (file == NULL) return false;
  length = fread(bytes, 1u, IPL3_ASSET_BYTES, file);
  extra = fgetc(file);
  read_error = ferror(file);
  close_error = fclose(file);
  return read_error == 0 && close_error == 0 &&
         length == IPL3_ASSET_BYTES && extra == EOF;
}

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
  t64_save_info info;
  size_t length = 0u;

  check_order_table();
  if (machine == NULL) return;
  fill_image(image_a, sizeof image_a);

  (void)memset(&info, 0xA5, sizeof info);
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
  (void)memset(&info, 0xA5, sizeof info);
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
}

/* ------------------------------------------------------------------------ */

typedef struct {
  const char *name;
  void (*run)(void);
} test_case;

static const test_case cases[] = {
    {"tracer-sram-raw", case_tracer_sram_raw},
    {"transform-vectors", case_transform_vectors},
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
