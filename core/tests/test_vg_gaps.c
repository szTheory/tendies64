/*
 * Phase 10 (VER-06) test-strength gap vectors, in a NEW binary so the
 * oracle-bound core/tests/test_rdp_depth.c and core/tests/test_vi.c stay
 * untouched. The helpers are copied from those files, never included.
 *
 * Every expected value below is a hex literal derived by hand from the
 * primary documents: docs/spec/rdp.md "V7 depth policy" and the eight-row
 * exponent table printed in the archived 07-08-DEPTH-FIELDS.md (Nintendo 64
 * Programming Manual NU6-06-0030-001G, printed p. 324), docs/spec/vi.md
 * "Clock and boundary policy". No expected value is taken from a production
 * helper or from any emulator output, and no production compression or
 * decompression routine is called here. The policies the vectors pin (the
 * primitive Z cap, signed floor division, the ceiling delta, the decal
 * tolerance) are software policy documented in rdp.md, not confirmed silicon
 * behavior. Each case ends with a same-setup wrong-value control: one
 * expected literal is changed and the first difference must be reported at
 * exactly that byte.
 *
 * The tests cover behavior that is already implemented, so they start green;
 * the failing direction is the named mutant of scripts/p10-mutants.py.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_internal.h"
#include "t64_test.h"
#include "tendies64/t64_machine.h"

#define DEPTH_PROFILE_ID \
  "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-depth-v7"
#define TEST_ROM_CAPACITY 65536u
#define COMMAND_ADDRESS UINT32_C(0x100)
#define FRAMEBUFFER_ADDRESS UINT32_C(0x5000)
#define DEPTH_IMAGE_ADDRESS UINT32_C(0x6000)
#define DP_START UINT32_C(0x04100000)
#define DP_END (DP_START + UINT32_C(4))

/* The pixel every vector observes: column 1, row 2 of the 8-pixel-wide
 * images, so a halfword index of 2 * 8 + 1 = 17 (byte offset 34). */
#define OBSERVED_COLOR (FRAMEBUFFER_ADDRESS + (2u * 8u + 1u) * 2u)
#define OBSERVED_DEPTH (DEPTH_IMAGE_ADDRESS + (2u * 8u + 1u) * 2u)

/* Color seen when the pixel was not drawn (the 0xa5 fill of the image) and
 * when it was drawn with the literal green primitive color. */
#define COLOR_UNTOUCHED UINT16_C(0xa5a5)
#define COLOR_DRAWN UINT16_C(0x07c1)

static uint8_t admitted_test_rom[TEST_ROM_CAPACITY];
static size_t admitted_test_rom_length;

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  void *memory;

  (void)context;
  if (storage_out == NULL || alignment == 0u) return false;
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

static t64_machine *create_machine_for_profile(const char *profile_id) {
  const t64_host_allocator allocator = {test_allocate, test_release, NULL};
  const char *path = getenv("T64_TEST_RDP_FILL_PATH");
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t rom[TEST_ROM_CAPACITY];
  FILE *file;
  long length;

  T64_CHECK(path != NULL && path[0] != '\0');
  if (path == NULL || path[0] == '\0') return NULL;
  file = fopen(path, "rb");
  T64_CHECK(file != NULL);
  if (file == NULL) return NULL;
  if (fseek(file, 0L, SEEK_END) != 0 || (length = ftell(file)) <= 0 ||
      (unsigned long)length > sizeof rom || fseek(file, 0L, SEEK_SET) != 0) {
    (void)fclose(file);
    T64_CHECK(false);
    return NULL;
  }
  T64_CHECK(fread(rom, 1u, (size_t)length, file) == (size_t)length);
  T64_CHECK(fclose(file) == 0);
  memcpy(admitted_test_rom, rom, (size_t)length);
  admitted_test_rom_length = (size_t)length;
  T64_CHECK_INT(t64_profile_lookup(profile_id, &profile), T64_STATUS_OK);
  T64_CHECK(profile != NULL);
  if (profile == NULL) return NULL;
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  if (machine == NULL) return NULL;
  T64_CHECK_INT(t64_machine_admit_rom(machine, rom, (size_t)length),
                T64_STATUS_OK);
  return machine;
}

static t64_machine *create_depth_machine(void) {
  return create_machine_for_profile(DEPTH_PROFILE_ID);
}

static void write_be16(uint8_t *bytes, uint16_t value) {
  bytes[0] = (uint8_t)(value >> 8);
  bytes[1] = (uint8_t)value;
}

static void write_be64(uint8_t *bytes, uint64_t value) {
  uint32_t i;
  for (i = 0u; i < 8u; i++)
    bytes[i] = (uint8_t)(value >> (56u - i * 8u));
}

static bool dispatch_all(t64_machine *machine) {
  size_t step;

  for (step = 0u; step < 4096u && machine->queue_count != 0u; step++) {
    t64_event event = machine->events[machine->queue_head];
    machine->queue_head = (machine->queue_head + 1u) %
                          T64_EVENT_QUEUE_CAPACITY;
    machine->queue_count--;
    machine->tick = event.tick;
    T64_CHECK(event.handler != NULL);
    if (event.handler != NULL)
      T64_CHECK_INT(event.handler(machine, &event), T64_STATUS_OK);
  }
  T64_CHECK_INT(machine->queue_count, 0u);
  return machine->queue_count == 0u;
}

static bool run_commands(t64_machine *machine, const uint64_t *words,
                         size_t count) {
  size_t i;
  uint32_t end;

  if (machine == NULL || words == NULL || count == 0u ||
      count > (UINT32_MAX - COMMAND_ADDRESS) / 8u)
    return false;
  machine->queue_head = 0u;
  machine->queue_count = 0u;
  for (i = 0u; i < count; i++)
    write_be64(machine->ram + COMMAND_ADDRESS + i * 8u, words[i]);
  end = COMMAND_ADDRESS + (uint32_t)(count * 8u);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_START, 4u,
                                      COMMAND_ADDRESS), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_END, 4u, end),
                T64_STATUS_OK);
  return dispatch_all(machine);
}

/* Index of the first differing byte, or SIZE_MAX when the buffers are equal
 * (the idiom of test_vi.c). */
static size_t first_difference(const uint8_t *left, const uint8_t *right,
                               size_t length) {
  size_t i;
  for (i = 0u; i < length; i++)
    if (left[i] != right[i]) return i;
  return SIZE_MAX;
}

typedef struct {
  uint16_t color;
  uint16_t depth;
  uint8_t hidden;
  uint32_t refusal;
} pixel_result;

/* Seed the 8 x 8 images (color 0xa5 fill, one depth word and one hidden byte
 * for every pixel), run the command words, and read the observed pixel. */
static bool run_seeded_stream(const uint64_t *commands, size_t count,
                              uint16_t old_depth, uint8_t old_hidden,
                              pixel_result *result) {
  t64_machine *machine = create_depth_machine();
  uint32_t i;

  if (machine == NULL || result == NULL) return false;
  memset(machine->ram + FRAMEBUFFER_ADDRESS, 0xa5, 8u * 8u * 2u);
  for (i = 0u; i < 8u * 8u; i++) {
    write_be16(machine->ram + DEPTH_IMAGE_ADDRESS + i * 2u, old_depth);
    machine->rdp_depth_hidden[(DEPTH_IMAGE_ADDRESS + i * 2u) / 2u] =
        old_hidden;
  }
  if (!run_commands(machine, commands, count)) {
    t64_machine_destroy(machine);
    return false;
  }
  result->color =
      (uint16_t)(((uint16_t)machine->ram[OBSERVED_COLOR] << 8) |
                 machine->ram[OBSERVED_COLOR + 1u]);
  result->depth =
      (uint16_t)(((uint16_t)machine->ram[OBSERVED_DEPTH] << 8) |
                 machine->ram[OBSERVED_DEPTH + 1u]);
  result->hidden = machine->rdp_depth_hidden[OBSERVED_DEPTH / 2u];
  result->refusal = machine->rdp.refusal_reason;
  t64_machine_destroy(machine);
  return true;
}

/* One accepted fill triangle with primitive Z: color image RGBA16 width 8 at
 * 0x5000, scissor [0,8) x [0,8), Z image at 0x6000, green primitive color,
 * combiner selecting the primitive color, edge words of the Phase 7 depth
 * matrix (Edge W0: YL=5 YM=3 YH=1; the pixel at column 1, row 2 is fully
 * covered), no Z coefficients (the Z words are zero and unused because the
 * primitive-Z mode bit is set), FullSync. `modes` is the SetOtherModes word
 * and `primitive_depth` the SetPrimDepth word (Z in bits 31..16, dZ in bits
 * 15..0). */
static bool run_primitive(uint64_t modes, uint64_t primitive_depth,
                          uint16_t old_depth, uint8_t old_hidden,
                          pixel_result *result) {
  const uint64_t commands[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020), modes,
      UINT64_C(0x3e00000000006000), primitive_depth,
      UINT64_C(0x3a00000000ff00ff), UINT64_C(0xfc00000000018600),
      UINT64_C(0x09000014000c0004), UINT64_C(0x00010000fffe8000),
      UINT64_C(0x0001000000000000), UINT64_C(0x0004000000018000),
      UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000),
      UINT64_C(0x2900000000000000),
  };
  return run_seeded_stream(commands, sizeof commands / sizeof commands[0],
                           old_depth, old_hidden, result);
}

/* SetPrimDepth word for a raw 16-bit Z and a 16-bit dZ. */
static uint64_t primitive_depth_word(uint32_t raw_z, uint32_t dz) {
  return UINT64_C(0x2e00000000000000) | ((uint64_t)(raw_z & 0xffffu) << 16) |
         (uint64_t)(dz & 0xffffu);
}

static void observe_bytes(const pixel_result *result, uint8_t *bytes) {
  bytes[0] = (uint8_t)(result->color >> 8);
  bytes[1] = (uint8_t)result->color;
  bytes[2] = (uint8_t)(result->depth >> 8);
  bytes[3] = (uint8_t)result->depth;
  bytes[4] = result->hidden;
}

/*
 * VG-01 (a): decal comparison at equality.
 *
 * Independent derivation (rdp.md V7, 07-08 exponent table):
 *  - old stored pixel: primitive Z raw 0x2000 with dZ 9. Linear Z is
 *    min(raw, 0x7fff) << 3 = 0x10000, inside exponent 0 (0 .. 0x1ffff, shift
 *    6), mantissa 0x10000 >> 6 = 0x400, visible word 0x400 << 2 = 0x1000 (the
 *    high two bits of the delta code are 0). dZ 9 has priority code
 *    floor(log2 9) = 3, so the hidden byte is 0x80 | 3 = 0x83 and the stored
 *    delta decodes to 1 << 3 = 8. The stored depth decodes to
 *    (0x400 << 6) + 0 = 0x10000.
 *  - new decal pixel: primitive dZ 2, so the tolerance is
 *    max(new delta 2, stored delta 8) = 8 and the pixel passes exactly when
 *    |newZ - oldZ| <= 8:
 *      raw 0x2000 -> 0x10000, difference 0, passes;
 *      raw 0x2001 -> 0x10008, difference 8 (equality), passes;
 *      raw 0x2002 -> 0x10010, difference 16, fails;
 *      raw 0x1fff -> 0x0fff8, difference 8 (equality below), passes;
 *      raw 0x1ffe -> 0x0fff0, difference 16, fails.
 *  - decal never updates depth (rdp.md), so the stored word stays 0x1000 with
 *    hidden 0x83 whether or not the color is drawn.
 */
static void case_decal_equality(void) {
  static const uint64_t opaque_update = UINT64_C(0x2f00000000000034);
  static const uint64_t decal_update = UINT64_C(0x2f00000000000c34);
  static const struct {
    uint32_t raw_z;
    uint16_t expected_color;
  } vectors[] = {
      {0x2000u, COLOR_DRAWN},     {0x2001u, COLOR_DRAWN},
      {0x2002u, COLOR_UNTOUCHED}, {0x1fffu, COLOR_DRAWN},
      {0x1ffeu, COLOR_UNTOUCHED},
  };
  static const uint8_t expected_equality[5] = {0x07u, 0xc1u, 0x10u, 0x00u,
                                               0x83u};
  pixel_result old_pixel = {0};
  pixel_result equality = {0};
  uint8_t observed[5];
  uint8_t wrong[5];
  size_t i;

  /* The old pixel is drawn through the command path and must read back as
   * the derived stored word. */
  T64_CHECK_INT((primitive_depth_word(0x2000u, 9u) >> 16) & 0xffffu, 0x2000u);
  T64_CHECK_INT(primitive_depth_word(0x2000u, 9u) & 0xffffu, 9u);
  T64_CHECK(run_primitive(opaque_update, primitive_depth_word(0x2000u, 9u),
                          0xfffcu, 0u, &old_pixel));
  T64_CHECK_INT(old_pixel.refusal, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT(old_pixel.color, COLOR_DRAWN);
  T64_CHECK_INT(old_pixel.depth, 0x1000u);
  T64_CHECK_INT(old_pixel.hidden, 0x83u);

  for (i = 0u; i < sizeof vectors / sizeof vectors[0]; i++) {
    pixel_result result = {0};
    T64_CHECK(run_primitive(decal_update,
                            primitive_depth_word(vectors[i].raw_z, 2u),
                            old_pixel.depth, old_pixel.hidden, &result));
    T64_CHECK_INT(result.refusal, T64_RDP_REFUSAL_NONE);
    T64_CHECK_INT(result.color, vectors[i].expected_color);
    T64_CHECK_INT(result.depth, 0x1000u);
    T64_CHECK_INT(result.hidden, 0x83u);
  }

  /* Same-setup wrong-value control on the equality vector (raw 0x2001): the
   * accepted observation equals the literal, and changing the stored depth
   * low byte of a copy is reported at that byte (index 3). */
  T64_CHECK(run_primitive(decal_update, primitive_depth_word(0x2001u, 2u),
                          old_pixel.depth, old_pixel.hidden, &equality));
  observe_bytes(&equality, observed);
  T64_CHECK_INT(first_difference(observed, expected_equality,
                                 sizeof expected_equality),
                SIZE_MAX);
  memcpy(wrong, expected_equality, sizeof wrong);
  wrong[3] = (uint8_t)(wrong[3] + 1u);
  T64_CHECK_INT(first_difference(observed, wrong, sizeof wrong), 3u);
}

typedef struct {
  const char *name;
  void (*run)(void);
} test_case;

static const test_case cases[] = {
    {"decal-equality", case_decal_equality},
};

int main(int argc, char **argv) {
  size_t i;

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) puts(cases[i].name);
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
      if (strcmp(argv[2], cases[i].name) == 0) {
        cases[i].run();
        return T64_TEST_FINISH(cases[i].name);
      }
    }
    return 2;
  }
  return 2;
}
