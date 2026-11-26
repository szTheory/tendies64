/*
 * Phase 10 depth tests on the descriptor 13 composite family (plan 10-10).
 *
 * The four registered ids ntsc-cart-4m-openipl3-rdram-b-t1-si-v2 (+ -e4k,
 * -e16k, -s32k) are the only composite ids with RDP depth active. Commands
 * go through the DP_START / DP_END registers exactly as in
 * core/tests/test_rdp_depth.c (that file is oracle-bound and is never
 * edited); the command words and the pixel literals here are copied from its
 * derivation records or derived by hand from docs/spec/rdp.md V7 and the
 * z_format[8] table (Nintendo 64 Programming Manual NU6-06-0030-001G,
 * printed p. 324), never computed by a production helper (z_compress,
 * z_decompress and the rest of core/t64_rdp_depth.c are not called).
 *
 * depth-composite-accepted: SetZImage, SetPrimDepth and a primitive-Z
 *     triangle write exact color, depth and hidden-sidecar bytes on each of
 *     the four ids; a farther second primitive is Z-tested away and a nearer
 *     third replaces the pixel.
 * depth-composite-wrong-depth: the same setup compared with expected windows
 *     in which one literal byte is changed must diverge at exactly that byte
 *     (the idiom of core/tests/test_vi.c case_vi_filter_wrong_expected), and
 *     a setup that differs by one input must diverge at the byte that input
 *     controls.
 * depth-composite-sidecar: all four Z triangle opcodes (0x09, 0x0b, 0x0d,
 *     0x0f), the opaque, transparent, interpenetrating and decal modes, the
 *     compare and update combinations and the far word 0xfffc; the aliased
 *     16-bit and 32-bit color and Z images; then an accepted CPU store, a PI
 *     DMA and an SP DMA over the depth image each clear exactly the
 *     overlapped sidecar granules, and a reset clears the whole sidecar.
 *
 * Evidence tier: unit-tested, authored scope. The hidden sidecar is a logical
 * model and the Z mode precedence is software policy (docs/spec/rdp.md V7),
 * not hardware truth.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_internal.h"
#include "t64_test.h"

#define IPL3_ASSET_BYTES 4096u
#define V2_STEM "ntsc-cart-4m-openipl3-rdram-b-t1-si-v2"
#define FAMILY_COUNT 4u
#define COMMAND_ADDRESS UINT32_C(0x100)
#define FRAMEBUFFER_ADDRESS UINT32_C(0x5000)
#define DEPTH_IMAGE_ADDRESS UINT32_C(0x6000)
#define DP_START UINT32_C(0x04100000)
#define DP_END (DP_START + UINT32_C(4))
/* The covered pixel (x=1, y=2) of the 8 pixel wide images. */
#define PIXEL_COLOR_ADDRESS (FRAMEBUFFER_ADDRESS + (2u * 8u + 1u) * 2u)
#define PIXEL_DEPTH_ADDRESS (DEPTH_IMAGE_ADDRESS + (2u * 8u + 1u) * 2u)

static const char *const family_ids[FAMILY_COUNT] = {
    V2_STEM, V2_STEM "-e4k", V2_STEM "-e16k", V2_STEM "-s32k"};

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

static uint8_t openipl3_asset[IPL3_ASSET_BYTES];

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
  t64_status status;

  T64_CHECK_INT(t64_profile_lookup(profile_id, &profile), T64_STATUS_OK);
  if (profile == NULL) return NULL;
  T64_CHECK_INT(t64_machine_create(
                    &(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(
                        &allocator, profile),
                    &machine),
                T64_STATUS_OK);
  if (machine == NULL) return NULL;
  if (!read_openipl3_asset(openipl3_asset)) {
    T64_CHECK(false);
    t64_machine_destroy(machine);
    return NULL;
  }
  status = t64_machine_admit_rom(machine, openipl3_asset,
                                 sizeof openipl3_asset);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return NULL;
  }
  return machine;
}

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

static uint16_t read_be16(const uint8_t *bytes) {
  return (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}

/* The 8x8 images: RGBA16 color filled with the untouched pattern 0xa5a5 and
 * the Z image filled with the far word 0xfffc, the saturated far value of the
 * V7 table (exponent 7, mantissa 0x7ff, decoded 0x3ffff). */
static void seed_images(t64_machine *machine) {
  uint32_t i;

  (void)memset(machine->ram + FRAMEBUFFER_ADDRESS, 0xa5, 8u * 8u * 2u);
  for (i = 0u; i < 8u * 8u; i++) {
    write_be16(machine->ram + DEPTH_IMAGE_ADDRESS + i * 2u, UINT16_C(0xfffc));
  }
}

/* Command words of core/tests/test_rdp_depth.c
 * case_primitive_depth_raw_packet_accepted_effect (copied, that file is never
 * edited): RGBA16 color image at 0x5000 width 8, scissor [0,8) x [0,8),
 * primitive Z with compare and update, SetZImage at 0x6000, SetPrimDepth
 * Z=0x1357 dZ=9, green primitive color, the literal combiner that selects it,
 * and the Z triangle opcode 0x09 with a zero Z plane. */
static const uint64_t first_pass[] = {
    UINT64_C(0x3f10000700005000), /* RGBA16 color image, width 8 */
    UINT64_C(0x2d00000000020020), /* scissor [0,8) x [0,8) */
    UINT64_C(0x2f00000000000034), /* primitive Z, compare, update */
    UINT64_C(0x3e00000000006000), /* aligned byte-addressed Z image */
    UINT64_C(0x2e00000013570009), /* primary field literal: Z=0x1357,dZ=9 */
    UINT64_C(0x3a00000000ff00ff), /* green primitive color, opaque alpha */
    UINT64_C(0xfc00000000018600), /* literal combiner selects primitive */
    UINT64_C(0x09000014000c0004), /* Edge W0: YL=5, YM=3, YH=1 */
    UINT64_C(0x00010000fffe8000), /* Edge W1: XL=1, DxLDy=-1.5 */
    UINT64_C(0x0001000000000000), /* Edge W2: XH=1, DxHDy=0 */
    UINT64_C(0x0004000000018000), /* Edge W3: XM=4, DxMDy=+1.5 */
    UINT64_C(0x0000000000000000), /* Z W4 and W5: literal zero plane */
    UINT64_C(0x0000000000000000),
};

/* One more primitive over the same triangle: SetPrimDepth, SetPrimColor, the
 * unchanged opcode 0x09 packet and a FullSync. The first two words differ per
 * pass. */
static void second_primitive(uint64_t prim_depth, uint64_t prim_color,
                             uint64_t out[9]) {
  out[0] = prim_depth;
  out[1] = prim_color;
  out[2] = UINT64_C(0x09000014000c0004);
  out[3] = UINT64_C(0x00010000fffe8000);
  out[4] = UINT64_C(0x0001000000000000);
  out[5] = UINT64_C(0x0004000000018000);
  out[6] = UINT64_C(0x0000000000000000);
  out[7] = UINT64_C(0x0000000000000000);
  out[8] = UINT64_C(0x2900000000000000);
}

/* Compare one integer and name the id and the item on a mismatch (the
 * T64_CHECK_INT line of this helper is the failed check). */
static void expect_eq(const char *id, const char *what, long long got,
                      long long want) {
  if (got != want) {
    fprintf(stderr, "%s: %s got 0x%llx want 0x%llx\n", id, what,
            (unsigned long long)got, (unsigned long long)want);
  }
  T64_CHECK_INT(got, want);
}

static void expect_pixel(const char *id, const t64_machine *machine,
                         uint16_t color, uint16_t depth, uint8_t hidden) {
  expect_eq(id, "refusal reason", machine->rdp.refusal_reason,
            T64_RDP_REFUSAL_NONE);
  expect_eq(id, "color word", read_be16(machine->ram + PIXEL_COLOR_ADDRESS),
            color);
  expect_eq(id, "depth word", read_be16(machine->ram + PIXEL_DEPTH_ADDRESS),
            depth);
  expect_eq(id, "sidecar entry",
            machine->rdp_depth_hidden[PIXEL_DEPTH_ADDRESS / 2u], hidden);
}

/* ---- depth-composite-accepted ------------------------------------------- */

static void check_accepted_effect(const char *profile_id) {
  static const uint8_t expected_color[] = {0x07u, 0xc1u};
  static const uint8_t expected_depth[] = {0x09u, 0xa8u};
  t64_machine *machine = create_admitted(profile_id);
  uint64_t more[9];

  if (machine == NULL) return;
  seed_images(machine);
  T64_CHECK_INT((first_pass[2] & UINT64_C(0x3f)), UINT64_C(0x34));
  T64_CHECK_INT((first_pass[4] >> 16) & UINT64_C(0xffff), UINT64_C(0x1357));
  T64_CHECK_INT(first_pass[4] & UINT64_C(0xffff), UINT64_C(9));
  T64_CHECK_INT(machine->rdp_depth_hidden[PIXEL_DEPTH_ADDRESS / 2u], 0u);
  run_commands(machine, first_pass, sizeof first_pass / sizeof first_pass[0]);
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT(machine->rdp.last_opcode_field, UINT32_C(0x09));
  T64_CHECK(memcmp(machine->ram + PIXEL_COLOR_ADDRESS, expected_color,
                   sizeof expected_color) == 0);
  /* Independent software oracle: 0x1357*8 = 0x9ab8; exponent 0 (range
   * 0x00000 to 0x1ffff, shift 6, add 0); mantissa floor(0x9ab8 / 64) =
   * 0x26a; visible = 0x26a << 2 = 0x09a8; dZ 9 has its highest set bit at
   * index 3, so the delta code is 3: high two bits 0 join the visible word
   * and the low two bits 3 go to the sidecar, whose entry is 0x80 | 3. */
  T64_CHECK(memcmp(machine->ram + PIXEL_DEPTH_ADDRESS, expected_depth,
                   sizeof expected_depth) == 0);
  T64_CHECK_INT(machine->rdp_depth_hidden[PIXEL_DEPTH_ADDRESS / 2u] & 3u, 3u);
  T64_CHECK_INT(machine->rdp_depth_hidden[PIXEL_DEPTH_ADDRESS / 2u], 0x83u);

  /* A farther second primitive: raw 0x2000 gives linear 0x10000. The stored
   * depth decodes to 0x26a << 6 = 0x9a80 and its stored delta code 3 decodes
   * to 8; the incoming dZ 9 is larger, so the tolerance is 9 and the opaque
   * test 0x10000 <= 0x9a80 + 9 fails: color, depth word and sidecar stay.
   * Red would be 0xf801 (R=255>>3=31 << 11, alpha bit 1) if it had passed. */
  second_primitive(UINT64_C(0x2e00000020000009),
                   UINT64_C(0x3a000000ff0000ff), more);
  run_commands(machine, more, 9u);
  expect_pixel(profile_id, machine, 0x07c1u, 0x09a8u, 0x83u);

  /* A nearer third primitive: raw 0x0800 gives linear 0x4000, which is
   * below the 0x9a80 + 9 threshold, so it passes and replaces the pixel:
   * mantissa 0x4000 >> 6 = 0x100, visible 0x100 << 2 = 0x0400, delta code 3
   * again (dZ 9), blue 0x003f (B=255>>3=31 << 1, alpha bit 1). */
  second_primitive(UINT64_C(0x2e00000008000009),
                   UINT64_C(0x3a0000000000ffff), more);
  run_commands(machine, more, 9u);
  expect_pixel(profile_id, machine, 0x003fu, 0x0400u, 0x83u);
  t64_machine_destroy(machine);
}

static void case_depth_composite_accepted(void) {
  size_t i;

  for (i = 0u; i < FAMILY_COUNT; i++) check_accepted_effect(family_ids[i]);
}

/* ---- depth-composite-wrong-depth ---------------------------------------- */

#define WINDOW_BYTES 5u

/* The observed window of the covered pixel: color word, depth word, sidecar
 * entry. */
static void capture_window(const t64_machine *machine,
                           uint8_t window[WINDOW_BYTES]) {
  window[0] = machine->ram[PIXEL_COLOR_ADDRESS];
  window[1] = machine->ram[PIXEL_COLOR_ADDRESS + 1u];
  window[2] = machine->ram[PIXEL_DEPTH_ADDRESS];
  window[3] = machine->ram[PIXEL_DEPTH_ADDRESS + 1u];
  window[4] = machine->rdp_depth_hidden[PIXEL_DEPTH_ADDRESS / 2u];
}

static size_t first_difference(const uint8_t *left, const uint8_t *right,
                               size_t length) {
  size_t i;

  for (i = 0u; i < length; i++) {
    if (left[i] != right[i]) return i;
  }
  return SIZE_MAX;
}

/* The first pass with another SetPrimDepth word, on a fresh machine. */
static void capture_first_pass(const char *profile_id, uint64_t prim_depth,
                               uint8_t window[WINDOW_BYTES]) {
  t64_machine *machine = create_admitted(profile_id);
  uint64_t commands[sizeof first_pass / sizeof first_pass[0]];

  (void)memset(window, 0, WINDOW_BYTES);
  if (machine == NULL) return;
  (void)memcpy(commands, first_pass, sizeof commands);
  commands[4] = prim_depth;
  seed_images(machine);
  run_commands(machine, commands, sizeof commands / sizeof commands[0]);
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  capture_window(machine, window);
  t64_machine_destroy(machine);
}

static void check_wrong_depth(const char *profile_id) {
  /* color 0x07c1, depth 0x09a8, sidecar 0x83: the literals of the accepted
   * case (PrimDepth Z 0x1357, dZ 9). */
  static const uint8_t expected[WINDOW_BYTES] = {0x07u, 0xc1u, 0x09u, 0xa8u,
                                                 0x83u};
  /* Setups that differ in one input, each with the byte it must change:
   *   Z 0x1358: 0x1358*8 = 0x9ac0, mantissa 0x9ac0 >> 6 = 0x26b, visible
   *     0x26b << 2 = 0x09ac: the depth low byte (index 3).
   *   Z 0x2000: 0x2000*8 = 0x10000, mantissa 0x400, visible 0x1000: the depth
   *     high byte (index 2) and the low byte, whose first difference is
   *     index 2.
   *   dZ 0x10: highest set bit index 4, delta code 4: high two bits 1 join
   *     the visible word (0x09a9) and the low two bits 0 give the entry 0x80:
   *     first difference at the depth low byte (index 3).
   *   dZ 2: delta code 1: visible unchanged, entry 0x81: the sidecar byte
   *     (index 4). */
  static const struct {
    uint64_t prim_depth;
    uint8_t window[WINDOW_BYTES];
    size_t first_difference;
  } setups[] = {
      {UINT64_C(0x2e00000013570009), {0x07u, 0xc1u, 0x09u, 0xa8u, 0x83u},
       SIZE_MAX},
      {UINT64_C(0x2e00000013580009), {0x07u, 0xc1u, 0x09u, 0xacu, 0x83u}, 3u},
      {UINT64_C(0x2e00000020000009), {0x07u, 0xc1u, 0x10u, 0x00u, 0x83u}, 2u},
      {UINT64_C(0x2e00000013570010), {0x07u, 0xc1u, 0x09u, 0xa9u, 0x80u}, 3u},
      {UINT64_C(0x2e00000013570002), {0x07u, 0xc1u, 0x09u, 0xa8u, 0x81u}, 4u},
  };
  uint8_t actual[WINDOW_BYTES];
  uint8_t wrong[WINDOW_BYTES];
  size_t i;

  /* The accepted setup equals the literals. */
  capture_first_pass(profile_id, first_pass[4], actual);
  expect_eq(profile_id, "accepted window difference",
            (long long)first_difference(actual, expected, WINDOW_BYTES),
            (long long)SIZE_MAX);
  /* Control: one literal byte changed at a time; the first difference must be
   * exactly that byte. */
  for (i = 0u; i < WINDOW_BYTES; i++) {
    (void)memcpy(wrong, expected, sizeof wrong);
    wrong[i] = (uint8_t)(wrong[i] ^ 0x01u);
    expect_eq(profile_id, "wrong-literal first difference",
              (long long)first_difference(actual, wrong, WINDOW_BYTES),
              (long long)i);
  }
  /* Control: a depth word one raw unit apart (0x09a8 against 0x09a9) diverges
   * at the low byte only. */
  (void)memcpy(wrong, expected, sizeof wrong);
  wrong[3] = 0xa9u;
  expect_eq(profile_id, "depth low byte control",
            (long long)first_difference(actual, wrong, WINDOW_BYTES), 3);
  /* Setups that differ by one input. */
  for (i = 0u; i < sizeof setups / sizeof setups[0]; i++) {
    capture_first_pass(profile_id, setups[i].prim_depth, actual);
    expect_eq(profile_id, "setup window difference from the literals",
              (long long)first_difference(actual, expected, WINDOW_BYTES),
              (long long)setups[i].first_difference);
    expect_eq(profile_id, "setup window against its own literals",
              (long long)first_difference(actual, setups[i].window,
                                          WINDOW_BYTES),
              (long long)SIZE_MAX);
  }
}

static void case_depth_composite_wrong_depth(void) {
  size_t i;

  for (i = 0u; i < FAMILY_COUNT; i++) check_wrong_depth(family_ids[i]);
}

/* ---- depth-composite-sidecar -------------------------------------------- */

#define TEXTURE_ADDRESS UINT32_C(0x4000)
#define PI_BASE UINT32_C(0x04600000)
#define SP_BASE UINT32_C(0x04040000)
#define DMEM_BASE UINT32_C(0x04000000)
#define CART_BASE UINT32_C(0x10000000)
#define WINDOW_GRANULES 64u
#define WINDOW_FIRST (DEPTH_IMAGE_ADDRESS / 2u)

/* Z triangle opcodes: command words of core/tests/test_rdp_depth.c
 * case_all_z_triangle_opcodes_reach_pixel (the same packet layouts), with its
 * expected pixel literals. Opcode 0x0f is run with the combiner that selects
 * shade times texel and with the triangle Z group W20 = 8.0 (Q16.16): linear
 * (8 << 16) / 8192 = 0x40, exponent 0, mantissa 1, visible 0x0004, delta 0,
 * entry 0x80; the color is (128, 64, 192) from the shade base: 128 >> 3 = 16,
 * 64 >> 3 = 8, 192 >> 3 = 24, so 0x8000 | 0x0200 | 0x0030 | 1 = 0x8231. */
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

static const struct {
  uint32_t opcode;
  uint16_t color;
  uint16_t depth;
  uint8_t hidden;
} opcode_rows[] = {
    {0x09u, 0x07c1u, 0x09a8u, 0x83u},
    {0x0bu, 0x07c1u, 0x09a8u, 0x83u},
    {0x0du, 0x07c1u, 0x09a8u, 0x83u},
    {0x0fu, 0x8231u, 0x0004u, 0x80u},
};

static void check_z_triangle(const char *profile_id, uint32_t opcode,
                             uint16_t color, uint16_t depth,
                             uint8_t hidden) {
  const bool textured = opcode == 0x0bu || opcode == 0x0fu;
  const bool shaded = opcode == 0x0du || opcode == 0x0fu;
  uint64_t commands[40];
  size_t count = 0u;
  size_t triangle_at;
  uint32_t i;
  t64_machine *machine = create_admitted(profile_id);

  if (machine == NULL) return;
  (void)memcpy(commands + count, head_words, sizeof head_words);
  count += sizeof head_words / sizeof head_words[0];
  if (textured) {
    (void)memcpy(commands + count, texture_setup, sizeof texture_setup);
    count += sizeof texture_setup / sizeof texture_setup[0];
    /* Opcode 0x0f: only texel (0,0) is white, so a wrong W4 coordinate
     * clamps to a black texel and the color proves the offset. */
    for (i = 0u; i < 8u * 8u; i++) {
      write_be16(machine->ram + TEXTURE_ADDRESS + i * 2u,
                 opcode == 0x0fu && i != 0u ? 0u : UINT16_C(0xffff));
    }
  }
  triangle_at = count;
  commands[count++] = UINT64_C(0x00000014000c0004) | ((uint64_t)opcode << 56);
  (void)memcpy(commands + count, edge_words, sizeof edge_words);
  count += sizeof edge_words / sizeof edge_words[0];
  if (shaded && textured) {
    /* Shade base RGBA = (128, 64, 192, 255), zero gradients. */
    commands[count++] = UINT64_C(0x0080004000c000ff);
    for (i = 0u; i < 7u; i++) commands[count++] = 0u;
  }
  if (textured) {
    /* S and T are 0 and W is 1.0 in the third 16-bit lane of W12. */
    commands[count++] = UINT64_C(0x0000000000010000);
    for (i = 0u; i < 7u; i++) commands[count++] = 0u;
  }
  if (shaded && !textured) {
    for (i = 0u; i < 8u; i++) commands[count++] = 0u;
  }
  commands[count++] = UINT64_C(0x0000000000000000);
  commands[count++] = UINT64_C(0x0000000000000000);
  if (opcode == 0x0fu) {
    /* FC121800 selects (texel0 - 0) * shade / 255 for RGB and alpha; the Z
     * mode takes the triangle plane (no primitive Z); W20 is 8.0. */
    commands[6] = UINT64_C(0xfc12180000000000);
    commands[2] = UINT64_C(0x2f00000000000030);
    commands[count - 2u] = UINT64_C(0x0008000000000000);
  }
  commands[count++] = UINT64_C(0x2900000000000000);
  expect_eq(profile_id, "opcode of the built packet",
            (long long)((commands[triangle_at] >> 56) & UINT64_C(0x3f)),
            (long long)opcode);
  seed_images(machine);
  run_commands(machine, commands, count);
  expect_eq(profile_id, "last opcode", machine->rdp.last_opcode_field,
            UINT32_C(0x29));
  T64_CHECK(machine->rdp.fullsync_count > 0u);
  T64_CHECK((machine->rdp.dpc_status & T64_RDP_STATUS_ERROR) == 0u);
  expect_pixel(profile_id, machine, color, depth, hidden);
  t64_machine_destroy(machine);
}

/* Primitive-Z vectors of core/tests/test_rdp_depth.c
 * case_depth_compare_update_and_coverage_modes, with the old depth and the
 * old color coverage seeded, and one literal result each. Rules (rdp.md V7):
 * opaque passes when newZ <= oldZ + max(dZ new, dZ old); decal when
 * |newZ - oldZ| <= that tolerance; interpenetrating also passes when the
 * incoming coverage exceeds the stored coverage; transparent and decal never
 * update depth; compare off passes; update off never writes. The new linear Z
 * is raw * 8: 0x1350 gives 0x9a80, 0x1351 gives 0x9a88, 0x1352 gives 0x9a90,
 * 0x1357 gives 0x9ab8. The stored word 0x09a8 decodes to 0x26a << 6 = 0x9a80
 * (stored delta code 0 here: no sidecar entry, visible low bits 0, decoded
 * 1) and the incoming dZ is 9, so the tolerance is 9: 0x9a88 is within it,
 * 0x9a90 and 0x9ab8 are not. */
typedef struct {
  const char *name;
  uint64_t modes;
  uint64_t prim_depth;
  uint16_t old_depth;
  uint8_t old_coverage;
  uint16_t color;
  uint16_t depth;
  uint8_t hidden;
} mode_vector;

#define MODE_OPAQUE UINT64_C(0x2f00000000000034)
#define MODE_DECAL UINT64_C(0x2f00000000000c34)
#define MODE_INTERPENETRATING UINT64_C(0x2f00000000000434)
#define MODE_TRANSPARENT UINT64_C(0x2f00000000000834)
#define PRIM_1350 UINT64_C(0x2e00000013500009)
#define PRIM_1351 UINT64_C(0x2e00000013510009)
#define PRIM_1352 UINT64_C(0x2e00000013520009)
#define PRIM_1357 UINT64_C(0x2e00000013570009)

static const mode_vector mode_vectors[] = {
    /* name, modes, primitive depth, old depth, old coverage, then the
     * expected color, depth and sidecar entry */
    {"opaque over the far word", MODE_OPAQUE, PRIM_1357, 0xfffcu, 0u,
     0x07c1u, 0x09a8u, 0x83u},
    {"opaque nearer", MODE_OPAQUE, PRIM_1350, 0x09acu, 0u, 0x07c1u, 0x09a8u,
     0x83u},
    {"opaque equal", MODE_OPAQUE, PRIM_1350, 0x09a8u, 0u, 0x07c1u, 0x09a8u,
     0x83u},
    {"opaque farther", MODE_OPAQUE, PRIM_1350, 0x09a4u, 0u, 0xa5a5u, 0x09a4u,
     0u},
    {"opaque farther by the word", MODE_OPAQUE, PRIM_1357, 0x09a8u, 0u,
     0xa5a5u, 0x09a8u, 0u},
    {"decal within tolerance", MODE_DECAL, PRIM_1351, 0x09a8u, 0u, 0x07c1u,
     0x09a8u, 0u},
    {"decal beyond tolerance", MODE_DECAL, PRIM_1352, 0x09a8u, 0u, 0xa5a5u,
     0x09a8u, 0u},
    {"decal exact", MODE_DECAL, PRIM_1350, 0x09a8u, 0u, 0x07c1u, 0x09a8u, 0u},
    {"interpenetrating, stored coverage 1", MODE_INTERPENETRATING, PRIM_1357,
     0x09a8u, 1u, 0x07c1u, 0x09a8u, 0x83u},
    {"interpenetrating, stored coverage 8", MODE_INTERPENETRATING, PRIM_1357,
     0x09a8u, 8u, 0xa5a5u, 0x09a8u, 0u},
    {"transparent, farther", MODE_TRANSPARENT, PRIM_1357, 0x09a8u, 0u, 0xa5a5u,
     0x09a8u, 0u},
    {"transparent over the far word", MODE_TRANSPARENT, PRIM_1357, 0xfffcu,
     0u, 0x07c1u, 0xfffcu, 0u},
    {"update without compare", UINT64_C(0x2f00000000000024), PRIM_1357,
     0x09a8u, 0u, 0x07c1u, 0x09a8u, 0x83u},
    {"compare without update", UINT64_C(0x2f00000000000014), PRIM_1357,
     0xfffcu, 0u, 0x07c1u, 0xfffcu, 0u},
    {"neither compare nor update", UINT64_C(0x2f00000000000004), PRIM_1357,
     0xfffcu, 0u, 0x07c1u, 0xfffcu, 0u},
};

static void check_mode_vector(const char *profile_id,
                              const mode_vector *vector) {
  const uint64_t commands[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
      vector->modes,                UINT64_C(0x3e00000000006000),
      vector->prim_depth,           UINT64_C(0x3a00000000ff00ff),
      UINT64_C(0xfc00000000018600), UINT64_C(0x09000014000c0004),
      UINT64_C(0x00010000fffe8000), UINT64_C(0x0001000000000000),
      UINT64_C(0x0004000000018000), UINT64_C(0x0000000000000000),
      UINT64_C(0x0000000000000000), UINT64_C(0x2900000000000000),
  };
  t64_machine *machine = create_admitted(profile_id);
  uint32_t i;

  if (machine == NULL) return;
  (void)memset(machine->ram + FRAMEBUFFER_ADDRESS, 0xa5, 8u * 8u * 2u);
  for (i = 0u; i < 8u * 8u; i++) {
    write_be16(machine->ram + DEPTH_IMAGE_ADDRESS + i * 2u,
               vector->old_depth);
  }
  if (vector->old_coverage != 0u) {
    machine->rdp_memory_coverage[PIXEL_COLOR_ADDRESS / 2u] =
        (uint8_t)(UINT8_C(0x80) | vector->old_coverage);
  }
  run_commands(machine, commands, sizeof commands / sizeof commands[0]);
  if (machine->rdp.refusal_reason != T64_RDP_REFUSAL_NONE ||
      read_be16(machine->ram + PIXEL_COLOR_ADDRESS) != vector->color ||
      read_be16(machine->ram + PIXEL_DEPTH_ADDRESS) != vector->depth ||
      machine->rdp_depth_hidden[PIXEL_DEPTH_ADDRESS / 2u] != vector->hidden) {
    fprintf(stderr, "mode vector failed: %s\n", vector->name);
  }
  expect_pixel(profile_id, machine, vector->color, vector->depth,
               vector->hidden);
  T64_CHECK_INT(machine->rdp.fullsync_count, 1u);
  t64_machine_destroy(machine);
}

/* A machine that has drawn the accepted triangle, with sentinel sidecar
 * entries in rows that the triangle does not reach (it covers rows 1 to 4 of
 * the 8 by 8 image: YH = 1, YL = 5): granule 0 is 0x81, granule 43 (row 5,
 * x 3) is 0x82, granule 63 (row 7, x 7) is 0x83. The window of the 64
 * sidecar entries of the depth image is captured after the draw. */
static t64_machine *drawn_machine(const char *profile_id,
                                  uint8_t before[WINDOW_GRANULES]) {
  t64_machine *machine = create_admitted(profile_id);

  if (machine == NULL) return NULL;
  seed_images(machine);
  machine->rdp_depth_hidden[WINDOW_FIRST + 0u] = UINT8_C(0x81);
  machine->rdp_depth_hidden[WINDOW_FIRST + 43u] = UINT8_C(0x82);
  machine->rdp_depth_hidden[WINDOW_FIRST + 63u] = UINT8_C(0x83);
  run_commands(machine, first_pass, sizeof first_pass / sizeof first_pass[0]);
  expect_pixel(profile_id, machine, 0x07c1u, 0x09a8u, 0x83u);
  (void)memcpy(before, machine->rdp_depth_hidden + WINDOW_FIRST,
               WINDOW_GRANULES);
  expect_eq(profile_id, "sentinel 0", before[0], 0x81);
  expect_eq(profile_id, "sentinel 43", before[43], 0x82);
  expect_eq(profile_id, "sentinel 63", before[63], 0x83);
  return machine;
}

/* Bits first .. first + count - 1 of a 64 bit granule mask. */
static uint64_t granule_mask(uint32_t first, uint32_t count) {
  uint64_t mask = 0u;
  uint32_t i;

  for (i = 0u; i < count; i++) mask |= UINT64_C(1) << (first + i);
  return mask;
}

/* Every sidecar entry of the window equals its entry before the write except
 * the granules of the mask, which are zero. */
static void expect_window(const char *profile_id, const char *what,
                          const t64_machine *machine,
                          const uint8_t before[WINDOW_GRANULES],
                          uint64_t cleared) {
  uint32_t i;

  for (i = 0u; i < WINDOW_GRANULES; i++) {
    const uint8_t want = (cleared >> i) & 1u ? 0u : before[i];
    const uint8_t got = machine->rdp_depth_hidden[WINDOW_FIRST + i];

    if (got != want) {
      fprintf(stderr, "%s: %s: granule %u got 0x%02x want 0x%02x\n",
              profile_id, what, (unsigned)i, (unsigned)got, (unsigned)want);
    }
    T64_CHECK_INT(got, want);
  }
}

static void check_cpu_store(const char *profile_id) {
  uint8_t before[WINDOW_GRANULES];
  t64_machine *machine = drawn_machine(profile_id, before);
  uint64_t cleared;

  if (machine == NULL) return;
  /* The covered pixel (1,2) holds the accepted entry 0x83 at granule 17. */
  expect_eq(profile_id, "granule 17 before", before[17], 0x83);
  /* A one byte store into the second byte of its halfword (granule 17). */
  T64_CHECK_INT(t64_machine_bus_write(machine, PIXEL_DEPTH_ADDRESS + 1u, 1u,
                                      UINT32_C(0x12)), T64_STATUS_OK);
  expect_eq(profile_id, "stored byte", machine->ram[PIXEL_DEPTH_ADDRESS + 1u],
            0x12);
  expect_eq(profile_id, "neighbor byte", machine->ram[PIXEL_DEPTH_ADDRESS],
            0x09);
  cleared = granule_mask(17u, 1u);
  expect_window(profile_id, "byte store", machine, before, cleared);
  expect_eq(profile_id, "granule 17 after",
            machine->rdp_depth_hidden[WINDOW_FIRST + 17u], 0);
  /* A word store at the start of row 1 overlaps granules 8 and 9. */
  T64_CHECK_INT(t64_machine_bus_write(machine, DEPTH_IMAGE_ADDRESS + 16u, 4u,
                                      UINT32_C(0x12345678)), T64_STATUS_OK);
  expect_eq(profile_id, "word store first byte",
            machine->ram[DEPTH_IMAGE_ADDRESS + 16u], 0x12);
  expect_eq(profile_id, "word store last byte",
            machine->ram[DEPTH_IMAGE_ADDRESS + 19u], 0x78);
  cleared |= granule_mask(8u, 2u);
  expect_window(profile_id, "word store", machine, before, cleared);
  /* A store just past the 128 byte image does not reach the window. */
  T64_CHECK_INT(t64_machine_bus_write(machine, DEPTH_IMAGE_ADDRESS + 128u, 1u,
                                      UINT32_C(0x34)), T64_STATUS_OK);
  expect_window(profile_id, "store past the image", machine, before, cleared);
  t64_machine_destroy(machine);
}

static void check_pi_dma(const char *profile_id) {
  uint8_t before[WINDOW_GRANULES];
  t64_machine *machine = drawn_machine(profile_id, before);
  const uint32_t dram = DEPTH_IMAGE_ADDRESS + 32u; /* row 2, granules 16-19 */
  uint32_t i;

  if (machine == NULL) return;
  expect_eq(profile_id, "granule 17 before", before[17], 0x83);
  machine->queue_head = 0u;
  machine->queue_count = 0u;
  T64_CHECK_INT(t64_machine_bus_write(machine, PI_BASE, 4u, dram),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, PI_BASE + 4u, 4u,
                                      CART_BASE + 0x100u), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, PI_BASE + 12u, 4u, 7u),
                T64_STATUS_OK);
  dispatch_all(machine);
  /* The eight bytes of the cleared asset at cart offset 0x100 landed. */
  for (i = 0u; i < 8u; i++) {
    expect_eq(profile_id, "pi byte", machine->ram[dram + i],
              openipl3_asset[0x100u + i]);
  }
  expect_window(profile_id, "pi dma", machine, before, granule_mask(16u, 4u));
  expect_eq(profile_id, "granule 17 after",
            machine->rdp_depth_hidden[WINDOW_FIRST + 17u], 0);
  t64_machine_destroy(machine);
}

static void check_sp_dma(const char *profile_id) {
  uint8_t before[WINDOW_GRANULES];
  t64_machine *machine = drawn_machine(profile_id, before);
  const uint32_t dram = DEPTH_IMAGE_ADDRESS + 48u; /* row 3, granules 24-27 */
  uint32_t i;

  if (machine == NULL) return;
  expect_eq(profile_id, "granule 25 before", before[25], 0x83);
  machine->queue_head = 0u;
  machine->queue_count = 0u;
  for (i = 0u; i < 8u; i++) {
    T64_CHECK_INT(t64_machine_bus_write(machine, DMEM_BASE + i, 1u,
                                        UINT32_C(0xa0) + i), T64_STATUS_OK);
  }
  T64_CHECK_INT(t64_machine_bus_write(machine, SP_BASE, 4u, 0u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, SP_BASE + 4u, 4u, dram),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, SP_BASE + 12u, 4u, 7u),
                T64_STATUS_OK);
  dispatch_all(machine);
  for (i = 0u; i < 8u; i++) {
    expect_eq(profile_id, "sp byte", machine->ram[dram + i], 0xa0u + i);
  }
  expect_window(profile_id, "sp dma", machine, before, granule_mask(24u, 4u));
  expect_eq(profile_id, "granule 25 after",
            machine->rdp_depth_hidden[WINDOW_FIRST + 25u], 0);
  t64_machine_destroy(machine);
}

/* Reset (a fresh admission) clears the whole sidecar, not only the window:
 * every entry is made nonzero first. */
static void check_reset(const char *profile_id) {
  uint8_t before[WINDOW_GRANULES];
  t64_machine *machine = drawn_machine(profile_id, before);
  size_t bytes;
  size_t i;
  size_t nonzero = 0u;

  if (machine == NULL) return;
  bytes = T64_RDP_DEPTH_HIDDEN_BYTES(machine->profile.rdram_bytes);
  (void)memset(machine->rdp_depth_hidden, 0x83, bytes);
  T64_CHECK(machine->rdp.depth_image_valid);
  T64_CHECK_INT(t64_machine_admit_rom(machine, openipl3_asset,
                                      sizeof openipl3_asset), T64_STATUS_OK);
  for (i = 0u; i < bytes; i++) {
    if (machine->rdp_depth_hidden[i] != 0u) nonzero++;
  }
  expect_eq(profile_id, "nonzero sidecar entries after reset",
            (long long)nonzero, 0);
  T64_CHECK(!machine->rdp.depth_image_valid);
  expect_eq(profile_id, "depth word after reset",
            read_be16(machine->ram + PIXEL_DEPTH_ADDRESS), 0);
  t64_machine_destroy(machine);
}

/* Aliased color and Z images (command words and literals of
 * core/tests/test_rdp_depth.c
 * case_aliased_coverage_and_alpha_preserve_hidden_depth; V7: coverage-only
 * changes write no color bytes and preserve the aliased hidden depth, an
 * alpha-rejected write suppresses the color write and the depth update, and
 * an actual color write invalidates the hidden entry of every written
 * granule). */
static void check_aliases(const char *profile_id) {
  static const uint64_t coverage_only[] = {
      UINT64_C(0x3f10000700006000), UINT64_C(0x2d00000000020020),
      UINT64_C(0x2f000000000000b4), UINT64_C(0x3e00000000006000),
      UINT64_C(0x2e00000008000000), UINT64_C(0x3a00000000ff00ff),
      UINT64_C(0xfc00000000018600), UINT64_C(0x09000014000c0004),
      UINT64_C(0x00010000fffe8000), UINT64_C(0x0001000000000000),
      UINT64_C(0x0004000000018000), 0u, 0u, UINT64_C(0x2900000000000000),
  };
  static const uint64_t follow_coverage[] = {
      UINT64_C(0x2f00000000000014), UINT64_C(0x2e00000008010000),
      UINT64_C(0x09000014000c0004), UINT64_C(0x00010000fffe8000),
      UINT64_C(0x0001000000000000), UINT64_C(0x0004000000018000),
      0u, 0u, UINT64_C(0x2900000000000000),
  };
  static const uint64_t alpha_rejected[] = {
      UINT64_C(0x3f18000700005fdc), UINT64_C(0x2d00000000020020),
      UINT64_C(0x2f00000000000035), UINT64_C(0x3e00000000006000),
      UINT64_C(0x2e00000008000000), UINT64_C(0x3a00000000ff0000),
      UINT64_C(0xf9000000000000ff), UINT64_C(0xfc00000000018600),
      UINT64_C(0x09000014000c0004), UINT64_C(0x00010000fffe8000),
      UINT64_C(0x0001000000000000), UINT64_C(0x0004000000018000),
      0u, 0u, UINT64_C(0x2900000000000000),
  };
  static const uint64_t follow_alpha[] = {
      UINT64_C(0x2f00000000000014), UINT64_C(0x2e00000008010000),
      UINT64_C(0x3a00000000ff00ff), UINT64_C(0x09000014000c0004),
      UINT64_C(0x00010000fffe8000), UINT64_C(0x0001000000000000),
      UINT64_C(0x0004000000018000), 0u, 0u, UINT64_C(0x2900000000000000),
  };
  /* The RGBA32 color image starts 36 bytes before the Z image, so its pixel
   * 18 (4 bytes) and the Z pixel 18 (2 bytes) begin at the same address. */
  const uint32_t coverage_pixel = DEPTH_IMAGE_ADDRESS + 34u;
  const uint32_t rgba32_color_base = DEPTH_IMAGE_ADDRESS - 36u;
  const uint32_t rgba32_pixel = rgba32_color_base + 18u * 4u;
  const uint32_t rgba32_depth = DEPTH_IMAGE_ADDRESS + 18u * 2u;
  uint8_t before_rgba32[4];
  t64_machine *machine = create_admitted(profile_id);
  uint32_t i;

  if (machine == NULL) return;
  /* 16-bit alias: the RGBA16 color image and the Z image are the same words.
   * Color-on-coverage records coverage 8 (entry 0x88) and leaves the bytes
   * and the hidden entry 0x83; a later real color write replaces the word
   * with 0x07c1 and clears the entry. */
  for (i = 0u; i < 8u * 8u; i++) {
    write_be16(machine->ram + DEPTH_IMAGE_ADDRESS + i * 2u, UINT16_C(0x0400));
    machine->rdp_depth_hidden[DEPTH_IMAGE_ADDRESS / 2u + i] = UINT8_C(0x83);
  }
  run_commands(machine, coverage_only,
               sizeof coverage_only / sizeof coverage_only[0]);
  expect_eq(profile_id, "alias16 refusal", machine->rdp.refusal_reason,
            T64_RDP_REFUSAL_NONE);
  expect_eq(profile_id, "alias16 word high", machine->ram[coverage_pixel],
            0x04);
  expect_eq(profile_id, "alias16 word low", machine->ram[coverage_pixel + 1u],
            0x00);
  expect_eq(profile_id, "alias16 hidden kept",
            machine->rdp_depth_hidden[coverage_pixel / 2u], 0x83);
  expect_eq(profile_id, "alias16 coverage entry",
            machine->rdp_memory_coverage[coverage_pixel / 2u], 0x88);
  run_commands(machine, follow_coverage,
               sizeof follow_coverage / sizeof follow_coverage[0]);
  expect_eq(profile_id, "alias16 follow refusal", machine->rdp.refusal_reason,
            T64_RDP_REFUSAL_NONE);
  expect_eq(profile_id, "alias16 color high", machine->ram[coverage_pixel],
            0x07);
  expect_eq(profile_id, "alias16 color low", machine->ram[coverage_pixel + 1u],
            0xc1);
  expect_eq(profile_id, "alias16 hidden cleared",
            machine->rdp_depth_hidden[coverage_pixel / 2u], 0);
  t64_machine_destroy(machine);

  /* 32-bit alias: an alpha-rejected write leaves the RGBA32 pixel bytes and
   * the hidden entry of the aliased Z word; the following opaque write stores
   * (0, 255, 0, 255) and clears both halfword entries. */
  machine = create_admitted(profile_id);
  if (machine == NULL) return;
  (void)memset(machine->ram + rgba32_color_base, 0xa5, 8u * 8u * 4u);
  for (i = 0u; i < 8u * 8u; i++) {
    write_be16(machine->ram + DEPTH_IMAGE_ADDRESS + i * 2u, UINT16_C(0x0400));
  }
  machine->rdp_depth_hidden[rgba32_depth / 2u] = UINT8_C(0x83);
  (void)memcpy(before_rgba32, machine->ram + rgba32_pixel,
               sizeof before_rgba32);
  expect_eq(profile_id, "alias32 addresses", rgba32_pixel, rgba32_depth);
  run_commands(machine, alpha_rejected,
               sizeof alpha_rejected / sizeof alpha_rejected[0]);
  expect_eq(profile_id, "alias32 refusal", machine->rdp.refusal_reason,
            T64_RDP_REFUSAL_NONE);
  T64_CHECK(memcmp(machine->ram + rgba32_pixel, before_rgba32,
                   sizeof before_rgba32) == 0);
  expect_eq(profile_id, "alias32 hidden kept",
            machine->rdp_depth_hidden[rgba32_depth / 2u], 0x83);
  run_commands(machine, follow_alpha,
               sizeof follow_alpha / sizeof follow_alpha[0]);
  expect_eq(profile_id, "alias32 follow refusal", machine->rdp.refusal_reason,
            T64_RDP_REFUSAL_NONE);
  expect_eq(profile_id, "alias32 red", machine->ram[rgba32_pixel], 0);
  expect_eq(profile_id, "alias32 green", machine->ram[rgba32_pixel + 1u], 255);
  expect_eq(profile_id, "alias32 blue", machine->ram[rgba32_pixel + 2u], 0);
  expect_eq(profile_id, "alias32 alpha", machine->ram[rgba32_pixel + 3u], 255);
  expect_eq(profile_id, "alias32 hidden cleared",
            machine->rdp_depth_hidden[rgba32_depth / 2u], 0);
  expect_eq(profile_id, "alias32 next hidden cleared",
            machine->rdp_depth_hidden[rgba32_depth / 2u + 1u], 0);
  t64_machine_destroy(machine);
}

static void case_depth_composite_sidecar(void) {
  size_t id;
  size_t i;

  for (id = 0u; id < FAMILY_COUNT; id++) {
    const char *profile_id = family_ids[id];

    for (i = 0u; i < sizeof opcode_rows / sizeof opcode_rows[0]; i++) {
      check_z_triangle(profile_id, opcode_rows[i].opcode, opcode_rows[i].color,
                       opcode_rows[i].depth, opcode_rows[i].hidden);
    }
    for (i = 0u; i < sizeof mode_vectors / sizeof mode_vectors[0]; i++) {
      check_mode_vector(profile_id, &mode_vectors[i]);
    }
    check_aliases(profile_id);
    check_cpu_store(profile_id);
    check_pi_dma(profile_id);
    check_sp_dma(profile_id);
    check_reset(profile_id);
  }
}

int main(int argc, char **argv) {
  static const char *const cases[] = {"depth-composite-accepted",
                                      "depth-composite-wrong-depth",
                                      "depth-composite-sidecar"};

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    size_t index;

    for (index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
      printf("%s\n", cases[index]);
    }
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    if (strcmp(argv[2], "depth-composite-accepted") == 0) {
      case_depth_composite_accepted();
      return T64_TEST_FINISH("unit.core.rdp.depth-composite-accepted");
    }
    if (strcmp(argv[2], "depth-composite-wrong-depth") == 0) {
      case_depth_composite_wrong_depth();
      return T64_TEST_FINISH("unit.core.rdp.depth-composite-wrong-depth");
    }
    if (strcmp(argv[2], "depth-composite-sidecar") == 0) {
      case_depth_composite_sidecar();
      return T64_TEST_FINISH("unit.core.rdp.depth-composite-sidecar");
    }
    fprintf(stderr, "unknown case: %s\n", argv[2]);
    return 2;
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--case NAME]\n", argv[0]);
    return 2;
  }
  case_depth_composite_accepted();
  case_depth_composite_wrong_depth();
  case_depth_composite_sidecar();
  return T64_TEST_FINISH("unit.core.rdp.depth-composite");
}
