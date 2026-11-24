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

typedef struct {
  const char *name;
  void (*run)(void);
} test_case;

static const test_case cases[] = {
    {"decal-equality", case_decal_equality},
    {"primitive-z-saturation", case_primitive_z_saturation},
    {"delta-ceiling", case_delta_ceiling},
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
