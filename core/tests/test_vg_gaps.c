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
#define VI_BASE UINT32_C(0x04400000)
#define MI_INTR UINT32_C(0x04300008)
#define MI_INTR_MASK UINT32_C(0x0430000c)
#define VI_V8_PROFILE_ID "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v8"
#define VI_V9_PROFILE_ID "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9"
#define VI_SOURCE_ORIGIN UINT32_C(0x3000)
#define VI_PACKET_BYTES 48u

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

/* The same accepted triangle with an interpolated Z plane (SetOtherModes has
 * the primitive-Z bit clear). The two Z coefficient words are
 * {base, dZ/dx} and {dZ/de, dZ/dy}, every field a two's-complement Q16.16
 * value; dZ/de is zero here because the major edge is vertical (DxHDy 0),
 * so the observed pixel (column 1, row 2) sees
 *   x offset = (1 + 1/2) - 1 = 1/2   (pixel center minus the major edge x),
 *   y offset = (2 + 1/2) - 1 = 3/2   (pixel center minus YH),
 * i.e. 0x8000 and 0x18000 in Q16. */
static bool run_plane(uint64_t modes, uint32_t z_base, uint32_t dzdx,
                      uint32_t dzdy, uint16_t old_depth, uint8_t old_hidden,
                      pixel_result *result) {
  const uint64_t commands[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020), modes,
      UINT64_C(0x3e00000000006000), UINT64_C(0x3a00000000ff00ff),
      UINT64_C(0xfc00000000018600), UINT64_C(0x09000014000c0004),
      UINT64_C(0x00010000fffe8000), UINT64_C(0x0001000000000000),
      UINT64_C(0x0004000000018000), ((uint64_t)z_base << 32) | dzdx,
      (uint64_t)dzdy, UINT64_C(0x2900000000000000),
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

/*
 * VG-01 (b): primitive Z saturation.
 *
 * Independent derivation (rdp.md V7 "Primitive Z is min(raw16, 0x7fff) << 3"):
 *   raw 0x7ffe -> 0x7ffe << 3 = 0x3fff0
 *   raw 0x7fff -> 0x7fff << 3 = 0x3fff8
 *   raw 0x8000 -> capped to 0x7fff -> 0x3fff8
 *   raw 0xffff -> capped to 0x7fff -> 0x3fff8
 * All four lie in exponent 7 (0x3f800 .. 0x3ffff, shift 0). The mantissa is
 * linear - 0x3f800: 0x7f0, 0x7f8, 0x7f8, 0x7f8. With dZ 0 the priority code
 * is 0, so the visible word is 0xe000 | (mantissa << 2) and the hidden byte
 * is 0x80:
 *   0xe000 | 0x1fc0 = 0xffc0, 0xe000 | 0x1fe0 = 0xffe0 (three times).
 * The cap therefore separates raw 0x7ffe from every value at or above 0x7fff,
 * and a cap one lower (0x7ffe) turns the second to fourth words into 0xffc0.
 */
static void case_primitive_z_saturation(void) {
  static const uint64_t opaque_update = UINT64_C(0x2f00000000000034);
  static const struct {
    uint32_t raw_z;
    uint16_t expected_depth;
  } vectors[] = {
      {0x7ffeu, 0xffc0u},
      {0x7fffu, 0xffe0u},
      {0x8000u, 0xffe0u},
      {0xffffu, 0xffe0u},
  };
  static const uint8_t expected_cap[2] = {0xffu, 0xe0u};
  pixel_result capped = {0};
  uint8_t observed[5];
  uint8_t wrong[2];
  size_t i;

  for (i = 0u; i < sizeof vectors / sizeof vectors[0]; i++) {
    pixel_result result = {0};
    T64_CHECK(run_primitive(opaque_update,
                            primitive_depth_word(vectors[i].raw_z, 0u),
                            0xfffcu, 0u, &result));
    T64_CHECK_INT(result.refusal, T64_RDP_REFUSAL_NONE);
    T64_CHECK_INT(result.color, COLOR_DRAWN);
    T64_CHECK_INT(result.depth, vectors[i].expected_depth);
    T64_CHECK_INT(result.hidden, 0x80u);
  }

  /* Wrong-value control on the first capped vector (raw 0x7fff): the
   * accepted word equals the literal and a copy with the low byte lowered to
   * the uncapped value 0xc0 differs at byte 1. */
  T64_CHECK(run_primitive(opaque_update, primitive_depth_word(0x7fffu, 0u),
                          0xfffcu, 0u, &capped));
  observe_bytes(&capped, observed);
  T64_CHECK_INT(first_difference(observed + 2, expected_cap,
                                 sizeof expected_cap),
                SIZE_MAX);
  memcpy(wrong, expected_cap, sizeof wrong);
  wrong[1] = 0xc0u;
  T64_CHECK_INT(first_difference(observed + 2, wrong, sizeof wrong), 1u);
}

/*
 * VG-01 (c): per-pixel delta ceiling.
 *
 * Independent derivation (rdp.md V7: "Per-pixel delta is
 * ceil((abs(dx)+abs(dy))/8192), clamped to 16 bits"; priority code
 * floor(log2 delta), at most 15). The plane base is 0x00080000 (8.0 in Q16.16,
 * 524288), so with zero slopes floor(524288 / 8192) = 64 = 0x40 and the
 * visible word is 0x0004 (exponent 0, mantissa 0x40 >> 6 = 1). At the
 * observed pixel the plane evaluates to
 *   q16 = 524288 + floor(dzdx * 0x8000 / 65536) + floor(dzdy * 0x18000 / 65536)
 * and linear Z = floor(q16 / 8192). Visible high bits are code >> 2 (0 below
 * code 4) and the hidden byte is 0x80 | (code & 3).
 *
 *   dzdx 8193      x 4096          q16 528384  linear 0x40  sum 8193 delta 2 code 1
 *   dzdx 8192      x 4096          q16 528384  linear 0x40  sum 8192 delta 1 code 0
 *   dzdx 8191      x 4095          q16 528383  linear 0x40  sum 8191 delta 1 code 0
 *   0, 0                           q16 524288  linear 0x40  sum 0    delta 0 code 0
 *   dzdx -4097, dzdy 4096   x -2049 y 6144  q16 528383  linear 0x40  sum 8193 delta 2 code 1
 *   dzdx 4096, dzdy -4097   x 2048 y -6146  q16 520190  linear 0x3f  sum 8193 delta 2 code 1
 *     (linear 0x3f is exponent 0, mantissa 0, visible 0x0000: compression is lossy)
 *   dzdx 24577     x 12288         q16 536576  linear 0x41  sum 24577 delta 4 code 2
 *   dzdx 24576     x 12288         q16 536576  linear 0x41  sum 24576 delta 3 code 1
 *   dzdx = dzdy = 0x7fffffff  q16 4295491581  linear clamp 0x3ffff
 *     (exponent 7, mantissa 0x7ff, 0xfffc); sum 4294967294, delta ceil
 *     524288 clamped to 65535, code 15, visible high bits 3, so 0xffff, and
 *     hidden 0x80 | 3 = 0x83.
 *
 * Under a floor instead of a ceiling the first, fifth, sixth and seventh
 * rows lose their code (8193 -> delta 1, 24577 -> delta 3).
 */
static void case_delta_ceiling(void) {
  static const uint64_t opaque_update = UINT64_C(0x2f00000000000030);
  static const uint32_t plane_base = UINT32_C(0x00080000);
  static const struct {
    uint32_t dzdx;
    uint32_t dzdy;
    uint16_t expected_depth;
    uint8_t expected_hidden;
  } vectors[] = {
      {UINT32_C(0x00002001), UINT32_C(0x00000000), 0x0004u, 0x81u},
      {UINT32_C(0x00002000), UINT32_C(0x00000000), 0x0004u, 0x80u},
      {UINT32_C(0x00001fff), UINT32_C(0x00000000), 0x0004u, 0x80u},
      {UINT32_C(0x00000000), UINT32_C(0x00000000), 0x0004u, 0x80u},
      {UINT32_C(0xffffefff), UINT32_C(0x00001000), 0x0004u, 0x81u},
      {UINT32_C(0x00001000), UINT32_C(0xffffefff), 0x0000u, 0x81u},
      {UINT32_C(0x00006001), UINT32_C(0x00000000), 0x0004u, 0x82u},
      {UINT32_C(0x00006000), UINT32_C(0x00000000), 0x0004u, 0x81u},
      {UINT32_C(0x7fffffff), UINT32_C(0x7fffffff), 0xffffu, 0x83u},
  };
  static const uint8_t expected_ceiling[3] = {0x00u, 0x04u, 0x81u};
  pixel_result ceiling = {0};
  uint8_t observed[5];
  uint8_t wrong[3];
  size_t i;

  for (i = 0u; i < sizeof vectors / sizeof vectors[0]; i++) {
    pixel_result result = {0};
    T64_CHECK(run_plane(opaque_update, plane_base, vectors[i].dzdx,
                        vectors[i].dzdy, 0xfffcu, 0u, &result));
    T64_CHECK_INT(result.refusal, T64_RDP_REFUSAL_NONE);
    T64_CHECK_INT(result.color, COLOR_DRAWN);
    T64_CHECK_INT(result.depth, vectors[i].expected_depth);
    T64_CHECK_INT(result.hidden, vectors[i].expected_hidden);
  }

  /* Wrong-value control on the 8193 row: the hidden byte of a floor result
   * would be 0x80, so a copy carrying it differs at byte 2. */
  T64_CHECK(run_plane(opaque_update, plane_base, UINT32_C(0x00002001),
                      UINT32_C(0), 0xfffcu, 0u, &ceiling));
  observe_bytes(&ceiling, observed);
  T64_CHECK_INT(first_difference(observed + 2, expected_ceiling,
                                 sizeof expected_ceiling),
                SIZE_MAX);
  memcpy(wrong, expected_ceiling, sizeof wrong);
  wrong[2] = 0x80u;
  T64_CHECK_INT(first_difference(observed + 2, wrong, sizeof wrong), 2u);
}

/*
 * Z exponent table boundaries (VER-06 numeric edges).
 *
 * The eight-row table printed in 07-08-DEPTH-FIELDS.md (manual p. 324) is
 *   exponent 0 shift 6 add 0x00000   exponent 4 shift 2 add 0x3c000
 *   exponent 1 shift 5 add 0x20000   exponent 5 shift 1 add 0x3e000
 *   exponent 2 shift 4 add 0x30000   exponent 6 shift 0 add 0x3f000
 *   exponent 3 shift 3 add 0x38000   exponent 7 shift 0 add 0x3f800
 * and each row spans 2048 << shift linear values, so the rows tile 0 .. 0x3ffff
 * exactly. A linear Z picks the last row whose add does not exceed it,
 * mantissa = floor((Z - add) / 2^shift) clamped to 0x7ff, and the visible word
 * is exponent << 13 | mantissa << 2 (delta code 0, hidden 0x80). The value read
 * back is (mantissa << shift) + add. Compression is lossy: the values below
 * are the first and last of each exponent range.
 *
 *   linear   exp mantissa  visible  decoded
 *   0x3f      0   0x000    0x0000   0x00000   (6 low bits dropped)
 *   0x40      0   0x001    0x0004   0x00040
 *   0x1ffff   0   0x7ff    0x1ffc   0x1ffc0
 *   0x20000   1   0x000    0x2000   0x20000
 *   0x2ffff   1   0x7ff    0x3ffc   0x2ffe0
 *   0x30000   2   0x000    0x4000   0x30000
 *   0x37fff   2   0x7ff    0x5ffc   0x37ff0
 *   0x38000   3   0x000    0x6000   0x38000
 *   0x3bfff   3   0x7ff    0x7ffc   0x3bff8
 *   0x3c000   4   0x000    0x8000   0x3c000
 *   0x3dfff   4   0x7ff    0x9ffc   0x3dffc
 *   0x3e000   5   0x000    0xa000   0x3e000
 *   0x3efff   5   0x7ff    0xbffc   0x3effe
 *   0x3f000   6   0x000    0xc000   0x3f000
 *   0x3f7ff   6   0x7ff    0xdffc   0x3f7ff
 *   0x3f800   7   0x000    0xe000   0x3f800
 *   0x3ffff   7   0x7ff    0xfffc   0x3ffff   (the far word)
 *
 * Every value is produced exactly through the command path: a plane Z with
 * base = linear * 8192 (Q16.16) and zero slopes has floor(base / 8192) =
 * linear at every pixel, and the eight multiples of 8 (0x40, 0x20000,
 * 0x30000, 0x38000, 0x3c000, 0x3e000, 0x3f000, 0x3f800) are also produced
 * through primitive Z with raw = linear >> 3. No value needed replacing by a
 * neighbor.
 *
 * Decode proof: with the visible word as the old depth (no hidden byte, so
 * the stored delta decodes to 1) and a decal plane Z of delta 0, the
 * tolerance is max(0, 1) = 1 and a new linear Z passes exactly when
 * |newZ - decoded| <= 1. The probes decoded - 2 .. decoded + 2 therefore
 * pass at -1, 0 and +1 and fail at -2 and +2 (where the value is inside
 * 0 .. 0x3ffff), which fixes the decoded value without calling a decode
 * routine. Decal never updates, so the stored word must not change.
 */
static void case_z_exponent_boundaries(void) {
  static const uint64_t opaque_plane = UINT64_C(0x2f00000000000030);
  static const uint64_t opaque_primitive = UINT64_C(0x2f00000000000034);
  static const uint64_t decal_plane = UINT64_C(0x2f00000000000c30);
  static const uint32_t linear[17] = {
      0x0003fu, 0x00040u, 0x1ffffu, 0x20000u, 0x2ffffu, 0x30000u,
      0x37fffu, 0x38000u, 0x3bfffu, 0x3c000u, 0x3dfffu, 0x3e000u,
      0x3efffu, 0x3f000u, 0x3f7ffu, 0x3f800u, 0x3ffffu,
  };
  static const uint16_t visible[17] = {
      0x0000u, 0x0004u, 0x1ffcu, 0x2000u, 0x3ffcu, 0x4000u,
      0x5ffcu, 0x6000u, 0x7ffcu, 0x8000u, 0x9ffcu, 0xa000u,
      0xbffcu, 0xc000u, 0xdffcu, 0xe000u, 0xfffcu,
  };
  static const uint32_t decoded[17] = {
      0x00000u, 0x00040u, 0x1ffc0u, 0x20000u, 0x2ffe0u, 0x30000u,
      0x37ff0u, 0x38000u, 0x3bff8u, 0x3c000u, 0x3dffcu, 0x3e000u,
      0x3effeu, 0x3f000u, 0x3f7ffu, 0x3f800u, 0x3ffffu,
  };
  static const uint8_t expected_row_2[2] = {0x1fu, 0xfcu};
  unsigned visible_literals = 0u;
  unsigned decoded_literals = 0u;
  unsigned decode_probes = 0u;
  unsigned primitive_rows = 0u;
  pixel_result control = {0};
  pixel_result far_word = {0};
  uint8_t observed[5];
  uint8_t wrong[2];
  size_t i;
  int offset;

  for (i = 0u; i < sizeof linear / sizeof linear[0]; i++) {
    pixel_result plane = {0};

    T64_CHECK(run_plane(opaque_plane, linear[i] << 13, 0u, 0u, 0xfffcu, 0u,
                        &plane));
    T64_CHECK_INT(plane.refusal, T64_RDP_REFUSAL_NONE);
    T64_CHECK_INT(plane.color, COLOR_DRAWN);
    T64_CHECK_INT(plane.depth, visible[i]);
    T64_CHECK_INT(plane.hidden, 0x80u);
    visible_literals++;

    if (linear[i] % 8u == 0u) {
      pixel_result primitive = {0};
      T64_CHECK(run_primitive(opaque_primitive,
                              primitive_depth_word(linear[i] >> 3, 0u),
                              0xfffcu, 0u, &primitive));
      T64_CHECK_INT(primitive.refusal, T64_RDP_REFUSAL_NONE);
      T64_CHECK_INT(primitive.color, COLOR_DRAWN);
      T64_CHECK_INT(primitive.depth, visible[i]);
      T64_CHECK_INT(primitive.hidden, 0x80u);
      primitive_rows++;
    }

    for (offset = -2; offset <= 2; offset++) {
      const int64_t probe = (int64_t)decoded[i] + offset;
      const bool passes = offset >= -1 && offset <= 1;
      pixel_result decal = {0};

      if (probe < 0 || probe > INT64_C(0x3ffff)) continue;
      T64_CHECK(run_plane(decal_plane, (uint32_t)probe << 13, 0u, 0u,
                          visible[i], 0u, &decal));
      T64_CHECK_INT(decal.refusal, T64_RDP_REFUSAL_NONE);
      T64_CHECK_INT(decal.color, passes ? COLOR_DRAWN : COLOR_UNTOUCHED);
      T64_CHECK_INT(decal.depth, visible[i]);
      T64_CHECK_INT(decal.hidden, 0u);
      decode_probes++;
    }
    decoded_literals++;
  }

  /* The word that machines and tests seed as uninitialized depth, 0xfffc,
   * decodes to the saturated far value 0x3ffff: the last row above, and an
   * explicit probe that two linear steps nearer fails the decal comparison
   * while the far value itself passes. */
  T64_CHECK_INT(visible[16], 0xfffcu);
  T64_CHECK_INT(decoded[16], 0x3ffffu);
  T64_CHECK(run_plane(decal_plane, UINT32_C(0x3ffff) << 13, 0u, 0u, 0xfffcu,
                      0u, &far_word));
  T64_CHECK_INT(far_word.color, COLOR_DRAWN);
  T64_CHECK(run_plane(decal_plane, UINT32_C(0x3fffd) << 13, 0u, 0u, 0xfffcu,
                      0u, &far_word));
  T64_CHECK_INT(far_word.color, COLOR_UNTOUCHED);

  /* Wrong-value control on the 0x1ffff row: the accepted word equals the
   * literal 0x1ffc and a copy whose high byte names the next exponent
   * (0x20) differs at byte 0. */
  T64_CHECK(run_plane(opaque_plane, UINT32_C(0x1ffff) << 13, 0u, 0u, 0xfffcu,
                      0u, &control));
  observe_bytes(&control, observed);
  T64_CHECK_INT(first_difference(observed + 2, expected_row_2,
                                 sizeof expected_row_2),
                SIZE_MAX);
  memcpy(wrong, expected_row_2, sizeof wrong);
  wrong[0] = 0x20u;
  T64_CHECK_INT(first_difference(observed + 2, wrong, sizeof wrong), 0u);

  T64_CHECK_INT(visible_literals, 17u);
  T64_CHECK_INT(primitive_rows, 8u);
  printf("z-exponent-boundaries: %u visible-word literals, %u decoded-value "
         "literals, %u decode probes, %u primitive-Z rows, far word 0xfffc "
         "decodes to 0x3ffff\n",
         visible_literals, decoded_literals, decode_probes, primitive_rows);
}

static bool vi_write(t64_machine *machine, uint32_t offset, uint32_t value) {
  t64_status status = t64_machine_bus_write(machine, VI_BASE + offset, 4u,
                                            value);
  T64_CHECK_INT(status, T64_STATUS_OK);
  return status == T64_STATUS_OK;
}

/* Twelve RGBA5551 source pixels at 0x3000 (a 4 x 3 window), the same literal
 * words as the VI witness of test_vi.c. */
static bool seed_vi_source(t64_machine *machine) {
  static const uint16_t pixels[12] = {
      UINT16_C(0xf801), UINT16_C(0x07c1), UINT16_C(0x003f), UINT16_C(0xffff),
      UINT16_C(0x07c1), UINT16_C(0xf801), UINT16_C(0xffff), UINT16_C(0x003f),
      UINT16_C(0x003f), UINT16_C(0xffff), UINT16_C(0xf801), UINT16_C(0x07c1),
  };
  size_t i;

  if (machine == NULL) return false;
  for (i = 0u; i < sizeof pixels / sizeof pixels[0]; i++) {
    t64_status status = t64_machine_bus_write(
        machine, UINT32_C(0x3000) + (uint32_t)(i * 2u), 2u, pixels[i]);
    T64_CHECK_INT(status, T64_STATUS_OK);
    if (status != T64_STATUS_OK) return false;
  }
  return true;
}

/* Program the 4 x 3 window with the standard NTSC sync registers
 * (H_SYNC low 12 bits 3093, V_SYNC low 10 bits 525, VI interrupt line 2),
 * unmask the VI interrupt and enable 16-bit output. The enabling write is the
 * last one, so the field starts at the machine tick of the call. */
static bool configure_standard_ntsc(t64_machine *machine) {
  return seed_vi_source(machine) &&
         vi_write(machine, UINT32_C(0x04), UINT32_C(0x3000)) &&
         vi_write(machine, UINT32_C(0x08), UINT32_C(4)) &&
         vi_write(machine, UINT32_C(0x0c), UINT32_C(2)) &&
         vi_write(machine, UINT32_C(0x14), UINT32_C(0)) &&
         vi_write(machine, UINT32_C(0x18), UINT32_C(525)) &&
         vi_write(machine, UINT32_C(0x1c), UINT32_C(3093)) &&
         vi_write(machine, UINT32_C(0x24), UINT32_C(4)) &&
         vi_write(machine, UINT32_C(0x28), UINT32_C(0x00020008)) &&
         vi_write(machine, UINT32_C(0x30), UINT32_C(0x00000400)) &&
         vi_write(machine, UINT32_C(0x34), UINT32_C(0x00000400)) &&
         t64_machine_bus_write(machine, MI_INTR_MASK, 4u,
                               UINT32_C(0x80)) == T64_STATUS_OK &&
         vi_write(machine, UINT32_C(0x00), UINT32_C(2));
}

static bool advance_guest_to_tick(t64_machine *machine, uint64_t tick,
                                  t64_stop_result *stop_out) {
  size_t attempt;

  for (attempt = 0u; attempt < 512u; attempt++) {
    t64_status status = t64_machine_advance(
        machine, &(t64_stop_request){T64_STOP_GUEST_TICK, tick}, stop_out);

    T64_CHECK_INT(status, T64_STATUS_OK);
    if (status != T64_STATUS_OK) return false;
    if (stop_out->reason != T64_STOP_REASON_RESOURCE_LIMIT)
      return stop_out->reason == T64_STOP_REASON_BOUND &&
             machine->tick == tick;
  }
  T64_CHECK(false);
  return false;
}

static uint32_t vi_register(t64_machine *machine, uint32_t offset) {
  uint32_t value = UINT32_MAX;
  T64_CHECK_INT(t64_machine_bus_read(machine, VI_BASE + offset, 4u, &value),
                T64_STATUS_OK);
  return value;
}

static uint32_t mi_interrupt(t64_machine *machine) {
  uint32_t value = UINT32_MAX;
  T64_CHECK_INT(t64_machine_bus_read(machine, MI_INTR, 4u, &value),
                T64_STATUS_OK);
  return value;
}

/*
 * Standard NTSC VI clock vector (the gap that 07-VERIFICATION.md Observation 3
 * records: the witness fields of 64 source cycles cannot see the source clock).
 *
 * Independent derivation (vi.md "Clock and boundary policy": one horizontal
 * period is (H_SYNC.low12 + 1) source cycles, V_SYNC.low10 counts half-lines
 * in a field, guest ticks run at 62,500,000 per second, the source clock is
 * 48,681,812 Hz, a boundary is the first whole tick at or after the exact
 * phase):
 *   field = (3093 + 1) * 525 / 2 = 812,175 source cycles
 *   ticks = 812175 * 62,500,000 / 48,681,812 = 1,042,708.4657..., so the
 *           first field boundary is the 1,042,709th tick after the enabling
 *           write and the tick before it (1,042,708) has none.
 *   line  = VI_INTR 2 is sampled after two half-lines = 3094 source cycles
 *           = 3094 * 62,500,000 / 48,681,812 = 3972.2227... ticks, so the
 *           interrupt and VI_CURRENT = 2 appear at tick 3973 and not at 3972.
 * (Both quotients were computed with exact rational arithmetic.) A source
 * clock of 48,600,000 would give 1,044,464 and 3979 instead.
 */
static void case_ntsc_clock(void) {
  static const uint64_t first_field_ticks = UINT64_C(1042709);
  static const uint64_t first_line_ticks = UINT64_C(3973);
  t64_machine *machine = create_machine_for_profile(VI_V8_PROFILE_ID);
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  t64_video_packet_info info = {0};
  uint64_t t0;
  uint8_t observed_ticks_bytes[8];
  uint8_t expected_ticks_bytes[8];
  uint8_t wrong[8];

  if (machine == NULL) return;
  t0 = machine->tick;
  T64_CHECK(configure_standard_ntsc(machine));
  T64_CHECK(machine->vi.enabled);
  T64_CHECK_INT(machine->vi.field_start_tick, t0);

  /* One tick before the first line sample: nothing is pending. */
  T64_CHECK(advance_guest_to_tick(machine, t0 + first_line_ticks - 1u, &stop));
  T64_CHECK_INT(mi_interrupt(machine) & UINT32_C(0x08), 0u);
  T64_CHECK_INT(vi_register(machine, UINT32_C(0x10)), 0u);
  T64_CHECK_INT(machine->vi.packet_count, 0u);
  T64_CHECK(advance_guest_to_tick(machine, t0 + first_line_ticks, &stop));
  T64_CHECK_INT(mi_interrupt(machine) & UINT32_C(0x08), UINT32_C(0x08));
  T64_CHECK_INT(vi_register(machine, UINT32_C(0x10)), 2u);

  /* One tick before the first field boundary: no packet. */
  T64_CHECK(advance_guest_to_tick(machine, t0 + first_field_ticks - 1u,
                                  &stop));
  T64_CHECK_INT(machine->vi.packet_count, 0u);
  T64_CHECK_INT(t64_video_info(machine, &info), T64_STATUS_NOT_READY);

  /* The boundary tick itself: exactly one packet, ending at that tick. */
  T64_CHECK(advance_guest_to_tick(machine, t0 + first_field_ticks, &stop));
  T64_CHECK_INT(machine->vi.packet_count, 1u);
  T64_CHECK_INT(t64_video_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.start_tick, t0);
  T64_CHECK_INT(info.end_tick, t0 + first_field_ticks);
  T64_CHECK_INT(info.field, 0u);
  T64_CHECK_INT(info.h_sync, 3093u);
  T64_CHECK_INT(info.v_sync, 525u);

  /* Same-setup wrong-value control: the observed duration, as eight
   * big-endian bytes, equals the literal 1,042,709 = 0x000fe915 and a copy
   * holding the floor neighbor 1,042,708 = 0x000fe914 differs at the low
   * byte (index 7). */
  write_be64(observed_ticks_bytes, info.end_tick - info.start_tick);
  write_be64(expected_ticks_bytes, UINT64_C(0x00000000000fe915));
  T64_CHECK_INT(first_difference(observed_ticks_bytes, expected_ticks_bytes,
                                 sizeof expected_ticks_bytes),
                SIZE_MAX);
  memcpy(wrong, expected_ticks_bytes, sizeof wrong);
  wrong[7] = (uint8_t)(wrong[7] - 1u);
  T64_CHECK_INT(first_difference(observed_ticks_bytes, wrong, sizeof wrong),
                7u);
  t64_machine_destroy(machine);
}

/* Program the short-field window of the Phase 7 VI tests: H_SYNC 15 and
 * V_SYNC 8 give a field of (15 + 1) * 8 / 2 = 64 source cycles, so the first
 * field boundary is the 83rd tick and the second the 166th. `output_width`
 * is the H_START end (start 0), `v_start` the whole V_START word. The guest
 * of the fill ROM programs the VI itself only after the first few hundred
 * ticks (see case_ntsc_clock), so these two boundaries are the vector's own. */
static bool configure_short_vi(t64_machine *machine, uint32_t status,
                               uint32_t source_width, uint32_t output_width,
                               uint32_t v_start, uint32_t y_scale) {
  return vi_write(machine, UINT32_C(0x04), VI_SOURCE_ORIGIN) &&
         vi_write(machine, UINT32_C(0x08), source_width) &&
         vi_write(machine, UINT32_C(0x0c), UINT32_C(0)) &&
         vi_write(machine, UINT32_C(0x14), UINT32_C(0)) &&
         vi_write(machine, UINT32_C(0x18), UINT32_C(8)) &&
         vi_write(machine, UINT32_C(0x1c), UINT32_C(15)) &&
         vi_write(machine, UINT32_C(0x20), UINT32_C(0)) &&
         vi_write(machine, UINT32_C(0x24), output_width) &&
         vi_write(machine, UINT32_C(0x28), v_start) &&
         vi_write(machine, UINT32_C(0x2c), UINT32_C(0)) &&
         vi_write(machine, UINT32_C(0x30), UINT32_C(0x00000400)) &&
         vi_write(machine, UINT32_C(0x34), y_scale) &&
         vi_write(machine, UINT32_C(0x00), status);
}

/* Seed `source_width * source_rows` big-endian RGBA5551 words at 0x3000, scan
 * out `fields` consecutive fields (1 or 2) of a 4 x 3 window and copy each
 * 48-byte packet. When `status_out` is not NULL the STATUS register is read
 * back after the programming. A field is drained before the next boundary so
 * that the second packet is the one of field 1. */
static bool scan_out_5551(const char *profile_id, uint32_t status,
                          uint32_t source_width, uint32_t source_rows,
                          const uint16_t *pixels, uint32_t v_start,
                          uint32_t y_scale, unsigned fields,
                          uint8_t packets[2][VI_PACKET_BYTES],
                          t64_video_packet_info infos[2],
                          uint32_t *status_out) {
  static const uint64_t boundary_ticks[2] = {UINT64_C(83), UINT64_C(166)};
  t64_machine *machine = create_machine_for_profile(profile_id);
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  size_t length = 0u;
  unsigned field;
  uint32_t i;
  bool ok = true;

  if (machine == NULL || fields == 0u || fields > 2u) return false;
  for (i = 0u; i < source_width * source_rows; i++) {
    t64_status written = t64_machine_bus_write(
        machine, VI_SOURCE_ORIGIN + i * 2u, 2u, pixels[i]);
    T64_CHECK_INT(written, T64_STATUS_OK);
    if (written != T64_STATUS_OK) ok = false;
  }
  ok = ok && configure_short_vi(machine, status, source_width, 4u, v_start,
                                y_scale);
  if (ok && status_out != NULL) {
    uint32_t value = UINT32_MAX;
    T64_CHECK_INT(t64_machine_bus_read(machine, VI_BASE, 4u, &value),
                  T64_STATUS_OK);
    *status_out = value;
  }
  for (field = 0u; ok && field < fields; field++) {
    ok = advance_guest_to_tick(machine, boundary_ticks[field], &stop);
    T64_CHECK(ok);
    if (!ok) break;
    T64_CHECK_INT(t64_video_info(machine, &infos[field]), T64_STATUS_OK);
    T64_CHECK_INT(infos[field].packet_bytes, VI_PACKET_BYTES);
    T64_CHECK_INT(infos[field].field, field);
    T64_CHECK_INT(t64_video_copy(machine, packets[field], VI_PACKET_BYTES,
                                 &length), T64_STATUS_OK);
    T64_CHECK_INT(length, VI_PACKET_BYTES);
  }
  t64_machine_destroy(machine);
  return ok;
}

/*
 * VG-03: an RGBA5551 source pixel with bit 0 clear scans out with alpha 0.
 *
 * Independent derivation (vi.md "v8 geometry policy": STATUS mode 2 reads
 * big-endian RGBA5551, each RGB5 channel expands as (value << 3) | (value >> 2)
 * and alpha bit 0 maps to 0/255): the channel value 31 expands to
 * (31 << 3) | (31 >> 2) = 248 | 7 = 255 and 0 stays 0. The 4 x 3 source
 *   0xf800 0x07c1 0x003f 0xffff
 *   0x07c1 0xf801 0xffff 0x003f
 *   0x003f 0xffff 0xf801 0x07c1
 * is the Phase 7 witness with one change: pixel (0,0) is 0xf800 (red 31,
 * green 0, blue 0, alpha bit CLEAR) instead of 0xf801. Reading the words by
 * hand: 0xf800 = 11111 00000 00000 0 -> 255,0,0, alpha 0; 0x07c1 = 00000
 * 11111 00000 1 -> 0,255,0 alpha 255; 0x003f = 00000 00000 11111 1 -> 0,0,255
 * alpha 255; 0xf801 -> 255,0,0 alpha 255; 0xffff -> 255,255,255,255. The 48
 * output bytes follow. The same 48-byte literal is asserted through the v8
 * path (render_packet) and through the v9 path (vi_read_source_pixel via
 * render_filter_packet, STATUS 0x302: mode 2, anti-alias mode 3 = nearest
 * sample, no filter stage), so a constant 255 alpha in either function turns
 * byte 3 into 255.
 */
static void case_rgba5551_alpha_clear(void) {
  static const uint16_t pixels[12] = {
      UINT16_C(0xf800), UINT16_C(0x07c1), UINT16_C(0x003f), UINT16_C(0xffff),
      UINT16_C(0x07c1), UINT16_C(0xf801), UINT16_C(0xffff), UINT16_C(0x003f),
      UINT16_C(0x003f), UINT16_C(0xffff), UINT16_C(0xf801), UINT16_C(0x07c1),
  };
  static const uint8_t expected[VI_PACKET_BYTES] = {
      255, 0,   0,   0,   0,   255, 0,   255, 0,   0,   255, 255,
      255, 255, 255, 255, 0,   255, 0,   255, 255, 0,   0,   255,
      255, 255, 255, 255, 0,   0,   255, 255, 0,   0,   255, 255,
      255, 255, 255, 255, 255, 0,   0,   255, 0,   255, 0,   255,
  };
  uint8_t v8_packets[2][VI_PACKET_BYTES];
  uint8_t v9_packets[2][VI_PACKET_BYTES];
  uint8_t wrong[VI_PACKET_BYTES];
  t64_video_packet_info v8_infos[2];
  t64_video_packet_info v9_infos[2];

  memset(v8_packets, 0, sizeof v8_packets);
  memset(v9_packets, 0, sizeof v9_packets);
  memset(v8_infos, 0, sizeof v8_infos);
  memset(v9_infos, 0, sizeof v9_infos);

  T64_CHECK(scan_out_5551(VI_V8_PROFILE_ID, UINT32_C(0x02), 4u, 3u, pixels,
                          UINT32_C(0x00020008), UINT32_C(0x400), 1u,
                          v8_packets, v8_infos, NULL));
  T64_CHECK_INT(v8_infos[0].source_format, T64_VIDEO_SOURCE_RGBA5551);
  T64_CHECK_INT(v8_infos[0].filter, 0u);
  T64_CHECK_INT(first_difference(v8_packets[0], expected, sizeof expected),
                SIZE_MAX);
  T64_CHECK_INT(v8_packets[0][3], 0u);
  T64_CHECK_INT(v8_packets[0][7], 255u);

  T64_CHECK(scan_out_5551(VI_V9_PROFILE_ID, UINT32_C(0x302), 4u, 3u, pixels,
                          UINT32_C(0x00020008), UINT32_C(0x400), 1u,
                          v9_packets, v9_infos, NULL));
  T64_CHECK_INT(v9_infos[0].source_format, T64_VIDEO_SOURCE_RGBA5551);
  T64_CHECK_INT(v9_infos[0].filter, 0u);
  T64_CHECK_INT(first_difference(v9_packets[0], expected, sizeof expected),
                SIZE_MAX);
  T64_CHECK_INT(v9_packets[0][3], 0u);
  T64_CHECK_INT(v9_packets[0][7], 255u);

  /* Same-setup wrong-value control: the literal with alpha forced to 255 at
   * pixel (0,0) is what a constant-255 alpha would produce, and the first
   * difference from the observed packet is byte 3 on both paths. */
  memcpy(wrong, expected, sizeof wrong);
  wrong[3] = 255u;
  T64_CHECK_INT(first_difference(v8_packets[0], wrong, sizeof wrong), 3u);
  T64_CHECK_INT(first_difference(v9_packets[0], wrong, sizeof wrong), 3u);
}

static void fill_packet(uint8_t packet[VI_PACKET_BYTES],
                        const uint8_t pixel[4]) {
  size_t i;
  for (i = 0u; i < VI_PACKET_BYTES; i++) packet[i] = pixel[i % 4u];
}

/*
 * VG-02: v9 filtered output under serrate uses the selected parity row and the
 * same-parity next row.
 *
 * Independent derivation (vi.md "Composite v9 filtered profile": "Serrate uses
 * the selected parity and same-parity next row"; v8 policy: serrate maps output
 * row n to source row 2 * n + field parity). With STATUS 0x242 (mode 2, serrate
 * bit 6, anti-alias mode 2, no smoothing, no divot, no gamma) and field 1, the
 * two source rows blended for output row n are 2n + 1 and 2n + 3.
 *
 * The 4 x 8 source is built so that the CORRECT pair is uniform: every odd row
 * holds X = 0xc27d, every even row holds Y = 0x1f18, in every column.
 *   X = 11000 01001 11110 1 -> R5 24, G5 9, B5 30, alpha 1
 *     -> expand (v << 3) | (v >> 2): 24 -> 192|6 = 198, 9 -> 72|2 = 74,
 *        30 -> 240|7 = 247, alpha 255
 *   Y = 00011 11100 01100 0 -> R5 3, G5 28, B5 12, alpha 0
 *     -> 3 -> 24|0 = 24, 28 -> 224|7 = 231, 12 -> 96|3 = 99, alpha 0
 * X and Y differ in every channel and in alpha. Y_SCALE is 0x02000400 (step
 * 1.0, source offset 0.5), so every output row has the vertical fraction 512 of
 * 1024 and the horizontal fraction is 0.
 *
 *   field 1, correct rows 2n+1 and 2n+3: both X, so the filtered value is
 *     (X * w + X * (2^20 - w)) >> 20 = X for every weight w: the literal does
 *     not depend on the filter weights. Output pixel 198,74,247,255.
 *   field 1, parity ignored (rows 2n and 2n+2): both Y, so 24,231,99 with
 *     alpha 0 (alpha comes from the first row): differs from X at byte 0.
 *   field 1, next row instead of the same-parity next row (rows 2n+1 and
 *     2n+2, X above Y): weights 2^19 each, so floor((198 + 24) / 2) = 111,
 *     floor((74 + 231) / 2) = 152, floor((247 + 99) / 2) = 173 and alpha 255
 *     from row 2n+1: 111,152,173,255: differs from X at byte 0.
 *   field 0, correct rows 2n and 2n+2: both Y, so 24,231,99,0 (an independent
 *     literal that the parity term must select; it also differs from field 1).
 *
 * source_bytes (vi.md: the bounding span from ORIGIN through the highest valid
 * source-color tap actually read): the highest row read is 2 * 2 + 1 + 2 = 7
 * in field 1 and 2 * 2 + 2 = 6 in field 0, in column 3 of a 4-pixel row:
 * (7 * 4 + 3 + 1) * 2 = 64 and (6 * 4 + 3 + 1) * 2 = 56 bytes.
 */
static void case_serrate_filter_parity(void) {
  static const uint16_t row_x = UINT16_C(0xc27d);
  static const uint16_t row_y = UINT16_C(0x1f18);
  static const uint8_t pixel_x[4] = {198u, 74u, 247u, 255u};
  static const uint8_t pixel_y[4] = {24u, 231u, 99u, 0u};
  static const uint8_t pixel_blend[4] = {111u, 152u, 173u, 255u};
  uint16_t source[4u * 8u];
  uint8_t packets[2][VI_PACKET_BYTES];
  t64_video_packet_info infos[2];
  uint8_t expected_field1[VI_PACKET_BYTES];
  uint8_t expected_field0[VI_PACKET_BYTES];
  uint8_t wrong[VI_PACKET_BYTES];
  uint32_t status_readback = 0u;
  uint32_t row;
  uint32_t column;

  for (row = 0u; row < 8u; row++)
    for (column = 0u; column < 4u; column++)
      source[row * 4u + column] = (row & 1u) != 0u ? row_x : row_y;
  fill_packet(expected_field1, pixel_x);
  fill_packet(expected_field0, pixel_y);
  memset(packets, 0, sizeof packets);
  memset(infos, 0, sizeof infos);

  T64_CHECK(scan_out_5551(VI_V9_PROFILE_ID, UINT32_C(0x242), 4u, 8u, source,
                          UINT32_C(0x00020008), UINT32_C(0x02000400), 2u,
                          packets, infos, &status_readback));
  /* STATUS bit 6 (serrate) reads back as written. */
  T64_CHECK_INT(status_readback & UINT32_C(0x40), UINT32_C(0x40));
  T64_CHECK_INT(status_readback, UINT32_C(0x242));
  T64_CHECK_INT(infos[0].field, 0u);
  T64_CHECK_INT(infos[1].field, 1u);
  T64_CHECK_INT(infos[1].parity, 1u);
  T64_CHECK_INT(infos[1].control & UINT32_C(0x40), UINT32_C(0x40));
  T64_CHECK_INT(infos[1].filter, T64_VIDEO_FILTER_RESAMPLE_Q20);

  T64_CHECK_INT(first_difference(packets[1], expected_field1,
                                 sizeof expected_field1), SIZE_MAX);
  T64_CHECK_INT(first_difference(packets[0], expected_field0,
                                 sizeof expected_field0), SIZE_MAX);
  T64_CHECK_INT(infos[1].source_bytes, 64u);
  T64_CHECK_INT(infos[0].source_bytes, 56u);

  /* Same-setup wrong-value controls: the three candidate pairings give three
   * different literals, each rejected at byte 0 by the observed field 1
   * packet, and one changed byte of the right literal is reported there. */
  fill_packet(wrong, pixel_y);
  T64_CHECK_INT(first_difference(packets[1], wrong, sizeof wrong), 0u);
  fill_packet(wrong, pixel_blend);
  T64_CHECK_INT(first_difference(packets[1], wrong, sizeof wrong), 0u);
  memcpy(wrong, expected_field1, sizeof wrong);
  wrong[21] = (uint8_t)(wrong[21] + 1u);
  T64_CHECK_INT(first_difference(packets[1], wrong, sizeof wrong), 21u);
}

typedef struct {
  const char *name;
  void (*run)(void);
} test_case;

static const test_case cases[] = {
    {"decal-equality", case_decal_equality},
    {"primitive-z-saturation", case_primitive_z_saturation},
    {"delta-ceiling", case_delta_ceiling},
    {"z-exponent-boundaries", case_z_exponent_boundaries},
    {"ntsc-clock", case_ntsc_clock},
    {"rgba5551-alpha-clear", case_rgba5551_alpha_clear},
    {"serrate-filter-parity", case_serrate_filter_parity},
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
