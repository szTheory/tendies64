/*
 * Phase 10 depth tests on the descriptor 13 composite family (plan 10-10).
 *
 * The four registered ids ntsc-cart-4m-openipl3-rdram-b-t1-si-v2 (+ -e4k,
 * -e16k, -s32k) are the only composite ids with RDP depth active. Commands
 * go through the DP_START / DP_END registers exactly as in
 * core/tests/test_rdp_depth.c (that file is oracle-bound and is never
 * edited); the command words and the pixel literals here are copied from its
 * derivation records, never computed by a production helper.
 *
 * depth-composite-accepted: SetZImage, SetPrimDepth and a primitive-Z
 *     triangle write exact color, depth and hidden-sidecar bytes; a farther
 *     second primitive is Z-tested away and a nearer third replaces the
 *     pixel.
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

static const t64_host_allocator allocator = {test_allocate, test_release,
                                             NULL};

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
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
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

static void expect_pixel(const t64_machine *machine, uint16_t color,
                         uint16_t depth, uint8_t hidden) {
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT(read_be16(machine->ram + PIXEL_COLOR_ADDRESS), color);
  T64_CHECK_INT(read_be16(machine->ram + PIXEL_DEPTH_ADDRESS), depth);
  T64_CHECK_INT(machine->rdp_depth_hidden[PIXEL_DEPTH_ADDRESS / 2u], hidden);
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
  expect_pixel(machine, 0x07c1u, 0x09a8u, 0x83u);

  /* A nearer third primitive: raw 0x0800 gives linear 0x4000, which is
   * below the 0x9a80 + 9 threshold, so it passes and replaces the pixel:
   * mantissa 0x4000 >> 6 = 0x100, visible 0x100 << 2 = 0x0400, delta code 3
   * again (dZ 9), blue 0x003f (B=255>>3=31 << 1, alpha bit 1). */
  second_primitive(UINT64_C(0x2e00000008000009),
                   UINT64_C(0x3a0000000000ffff), more);
  run_commands(machine, more, 9u);
  expect_pixel(machine, 0x003fu, 0x0400u, 0x83u);
  t64_machine_destroy(machine);
}

static void case_depth_composite_accepted(void) {
  /* Tracer: one id. */
  check_accepted_effect(family_ids[0]);
}

int main(int argc, char **argv) {
  static const char *const cases[] = {"depth-composite-accepted"};

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
    fprintf(stderr, "unknown case: %s\n", argv[2]);
    return 2;
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--case NAME]\n", argv[0]);
    return 2;
  }
  case_depth_composite_accepted();
  return T64_TEST_FINISH("unit.core.rdp.depth-composite");
}
