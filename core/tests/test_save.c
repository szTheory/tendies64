/*
 * Phase 9 save media tests (plan 09-04): EEPROM on joybus channel 4, the SRAM
 * window and its PI DMA, selection by profile only, refusal statuses and the
 * save-media digest section.
 *
 * Every figure below is a named software-profile policy value written out as
 * a literal in this file; nothing is taken from a production helper, and no
 * hardware accuracy or hardware timing is claimed. All contents are authored
 * byte patterns; no ROM, save or firmware image is used (the only external
 * input is the already cleared openIPL3 asset named by T64_TEST_IPL3_PATH).
 *
 * usage: test_save [--list-cases|--suite device|--case NAME]
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

/* ---- digest stream capture ----------------------------------------------- */

#define SPAN_CAPACITY 4096u

typedef struct {
  uint8_t *bytes;
  size_t length;
  t64_digest_span spans[SPAN_CAPACITY];
  size_t span_count;
} stream;

static void free_stream(stream *captured) {
  free(captured->bytes);
  captured->bytes = NULL;
}

static bool capture_stream(const t64_machine *machine, stream *out) {
  t64_digest_sink sink;
  size_t length;

  (void)memset(out, 0, sizeof *out);
  (void)memset(&sink, 0, sizeof sink);
  T64_CHECK_INT(t64_digest_stream(machine, &sink), T64_STATUS_OK);
  length = sink.offset;
  out->bytes = (uint8_t *)malloc(length > 0u ? length : 1u);
  if (out->bytes == NULL) {
    T64_CHECK(false);
    return false;
  }
  (void)memset(&sink, 0, sizeof sink);
  sink.buffer = out->bytes;
  sink.capacity = length;
  sink.spans = out->spans;
  sink.span_capacity = SPAN_CAPACITY;
  T64_CHECK_INT(t64_digest_stream(machine, &sink), T64_STATUS_OK);
  T64_CHECK(!sink.overflow);
  T64_CHECK_INT(sink.offset, length);
  out->length = length;
  out->span_count = sink.span_count;
  return !sink.overflow && sink.offset == length;
}

static uint32_t be32(const uint8_t *bytes) {
  return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
         ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

/* The marker ids in stream order; returns how many markers were found. The
 * ids are copied into ids[] (at most capacity). */
static size_t list_markers(const stream *captured, uint32_t *ids,
                           size_t capacity) {
  size_t index;
  size_t found = 0u;

  for (index = 0u; index < captured->span_count; index++) {
    if (captured->spans[index].class != T64_DIGEST_CLASS_MARKER) continue;
    if (found < capacity) {
      ids[found] = be32(captured->bytes + captured->spans[index].offset);
    }
    found++;
  }
  return found;
}

/* The span index of marker `id`, or SIZE_MAX. */
static size_t marker_span(const stream *captured, uint32_t id) {
  size_t index;

  for (index = 0u; index < captured->span_count; index++) {
    if (captured->spans[index].class != T64_DIGEST_CLASS_MARKER) continue;
    if (be32(captured->bytes + captured->spans[index].offset) == id) {
      return index;
    }
  }
  return SIZE_MAX;
}

static void to_hex(const uint8_t digest[T64_DIGEST_SIZE],
                   char hex[2u * T64_DIGEST_SIZE + 1u]) {
  static const char digits[] = "0123456789abcdef";
  size_t i;

  for (i = 0u; i < T64_DIGEST_SIZE; i++) {
    hex[2u * i] = digits[digest[i] >> 4];
    hex[2u * i + 1u] = digits[digest[i] & 0x0fu];
  }
  hex[2u * T64_DIGEST_SIZE] = '\0';
}

static void digest_of(const t64_machine *machine,
                      uint8_t digest[T64_DIGEST_SIZE]) {
  T64_CHECK_INT(t64_machine_digest(machine, digest), T64_STATUS_OK);
}

/* ------------------------------------------------------------------------ */
/* storage-blank                                                             */
/* ------------------------------------------------------------------------ */

typedef struct {
  const char *id;
  uint32_t kind;
  size_t size;
  uint8_t fill;
} blank_row;

/* Independent literals: the kind numbering and the byte sizes are policy; the
 * blank contents are 0xFF for the EEPROM and 0x00 for the SRAM. */
static const blank_row blank_rows[] = {
    {STEM_ID, 0u, 0u, 0x00u},
    {E4K_ID, 1u, 512u, 0xFFu},
    {E16K_ID, 2u, 2048u, 0xFFu},
    {S32K_ID, 3u, 32768u, 0x00u},
};

/* The idle digest of the stem id (no save device) captured at the 09-03 tree
 * before this plan's first edit; the section 11 addition must not move it. */
#define STEM_IDLE_DIGEST \
  "5ed31137a518103e1086ec9d84350f44306f81fef57145ca4ca72a7f0815d9b0"

static void case_storage_blank(void) {
  size_t index;

  for (index = 0u; index < sizeof blank_rows / sizeof blank_rows[0];
       index++) {
    const blank_row *row = &blank_rows[index];
    t64_machine *machine = create_admitted(row->id);
    stream captured;
    uint32_t ids[16];
    size_t count;
    size_t at;
    size_t position;

    if (machine == NULL) continue;
    T64_CHECK_INT(machine->profile.save_kind, row->kind);
    T64_CHECK_INT(machine->save.busy_until, 0u);
    if (row->size == 0u) {
      T64_CHECK(machine->save.bytes == NULL);
    } else {
      T64_CHECK(machine->save.bytes != NULL);
      if (machine->save.bytes != NULL) {
        T64_CHECK_INT((uintptr_t)machine->save.bytes % 8u, 0u);
        for (position = 0u; position < row->size; position++) {
          if (machine->save.bytes[position] != row->fill) {
            T64_CHECK_INT(machine->save.bytes[position], row->fill);
            break;
          }
        }
      }
    }

    if (capture_stream(machine, &captured)) {
      count = list_markers(&captured, ids, 16u);
      /* Stem: markers 1 to 10 only; save ids: 1 to 11, ascending. */
      T64_CHECK_INT(count, row->kind == 0u ? 10u : 11u);
      for (at = 0u; at < count && at < 16u; at++) {
        T64_CHECK_INT(ids[at], at + 1u);
      }
      if (row->kind == 0u) {
        T64_CHECK(marker_span(&captured, 11u) == SIZE_MAX);
      } else {
        size_t span = marker_span(&captured, 11u);

        T64_CHECK(span != SIZE_MAX);
        if (span != SIZE_MAX) {
          /* Section 11 is the last section: the body is one new-span run of
             kind, size, deadline, direction and the storage bytes. */
          T64_CHECK(span + 1u < captured.span_count);
          if (span + 1u < captured.span_count) {
            const t64_digest_span *body = &captured.spans[span + 1u];

            T64_CHECK_INT(body->class, T64_DIGEST_CLASS_NEW);
            T64_CHECK_INT(body->length, 20u + row->size);
            T64_CHECK_INT(be32(captured.bytes + body->offset), row->kind);
            T64_CHECK_INT(be32(captured.bytes + body->offset + 4u),
                          row->size);
          }
        }
      }
      free_stream(&captured);
    }

    if (row->kind == 0u) {
      uint8_t digest[T64_DIGEST_SIZE];
      char hex[2u * T64_DIGEST_SIZE + 1u];

      digest_of(machine, digest);
      to_hex(digest, hex);
      if (strcmp(hex, STEM_IDLE_DIGEST) != 0) {
        fprintf(stderr, "stem idle digest: got %s\n", hex);
      }
      T64_CHECK_STR(hex, STEM_IDLE_DIGEST);
    } else if (machine->save.bytes != NULL) {
      uint8_t before[T64_DIGEST_SIZE];
      uint8_t after[T64_DIGEST_SIZE];

      /* A one-byte change of the storage alters the digest, at the first, a
         middle and the last byte. */
      const size_t spots[3] = {0u, row->size / 2u, row->size - 1u};
      size_t spot;

      digest_of(machine, before);
      for (spot = 0u; spot < 3u; spot++) {
        machine->save.bytes[spots[spot]] ^= UINT8_C(0x01);
        digest_of(machine, after);
        T64_CHECK(memcmp(before, after, T64_DIGEST_SIZE) != 0);
        machine->save.bytes[spots[spot]] ^= UINT8_C(0x01);
        digest_of(machine, after);
        T64_CHECK(memcmp(before, after, T64_DIGEST_SIZE) == 0);
      }
    }
    t64_machine_destroy(machine);
  }
}

/* ------------------------------------------------------------------------ */
/* Dispatch                                                                  */
/* ------------------------------------------------------------------------ */

typedef struct {
  const char *name;
  void (*run)(void);
} test_case;

static const test_case cases[] = {
    {"storage-blank", case_storage_blank},
};

int main(int argc, char **argv) {
  size_t index;
  const size_t count = sizeof cases / sizeof cases[0];

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (index = 0u; index < count; index++) puts(cases[index].name);
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--suite") == 0) {
    if (strcmp(argv[2], "device") != 0) {
      fprintf(stderr, "unknown save suite: %s\n", argv[2]);
      return 2;
    }
    for (index = 0u; index < count; index++) {
      printf("case %s\n", cases[index].name);
      cases[index].run();
    }
    return T64_TEST_FINISH("unit.core.save.device");
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    for (index = 0u; index < count; index++) {
      if (strcmp(cases[index].name, argv[2]) == 0) {
        cases[index].run();
        return T64_TEST_FINISH("unit.core.save.device");
      }
    }
    fprintf(stderr, "unknown save case: %s\n", argv[2]);
    return 2;
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--suite device|--case NAME]\n",
            argv[0]);
    return 2;
  }
  for (index = 0u; index < count; index++) {
    printf("case %s\n", cases[index].name);
    cases[index].run();
  }
  return T64_TEST_FINISH("unit.core.save.device");
}
