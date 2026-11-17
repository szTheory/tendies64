#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_internal.h"
#include "t64_test.h"
#include "tendies64/t64_machine.h"

#define TRIANGLE_PROFILE_ID \
  "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-triangle-v3"
#define TEST_ROM_CAPACITY 65536u
#define COMMAND_ADDRESS UINT32_C(0x100)
#define FRAMEBUFFER_ADDRESS UINT32_C(0x5000)
#define DP_START UINT32_C(0x04100000)
#define DP_END (DP_START + UINT32_C(4))

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

static t64_machine *create_authored_machine_for(const char *profile_id) {
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
  T64_CHECK_INT(t64_profile_lookup(profile_id, &profile),
                T64_STATUS_OK);
  T64_CHECK(profile != NULL);
  if (profile == NULL) return NULL;
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  if (machine == NULL) return NULL;
  T64_CHECK_INT(t64_machine_admit_rom(machine, rom, (size_t)length),
                T64_STATUS_OK);
  return machine;
}

static t64_machine *create_authored_machine(void) {
  return create_authored_machine_for(TRIANGLE_PROFILE_ID);
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
  T64_CHECK(machine->queue_count == 0u);
  return machine->queue_count == 0u;
}

static bool submit_commands(t64_machine *machine, const uint64_t *words,
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

static bool run_commands(t64_machine *machine, const uint64_t *words,
                         size_t count) {
  if (machine == NULL) return false;
  memset(machine->ram + FRAMEBUFFER_ADDRESS, 0xa5, 8u * 8u * 2u);
  return submit_commands(machine, words, count);
}

typedef struct {
  uint8_t framebuffer[8u * 8u * 2u];
  uint16_t *coverage;
} raster_snapshot;

static bool snapshot_raster(const t64_machine *machine,
                            raster_snapshot *snapshot) {
  snapshot->coverage = malloc(T64_RDP_TRIANGLE_COVERAGE_BYTES);
  T64_CHECK(snapshot->coverage != NULL);
  if (snapshot->coverage == NULL) return false;
  memcpy(snapshot->framebuffer, machine->ram + FRAMEBUFFER_ADDRESS,
         sizeof snapshot->framebuffer);
  memcpy(snapshot->coverage, machine->rdp_triangle_coverage,
         T64_RDP_TRIANGLE_COVERAGE_BYTES);
  return true;
}

static void check_raster_unchanged(const t64_machine *machine,
                                  raster_snapshot *snapshot) {
  T64_CHECK(memcmp(snapshot->framebuffer,
                   machine->ram + FRAMEBUFFER_ADDRESS,
                   sizeof snapshot->framebuffer) == 0);
  T64_CHECK(memcmp(snapshot->coverage, machine->rdp_triangle_coverage,
                   T64_RDP_TRIANGLE_COVERAGE_BYTES) == 0);
  free(snapshot->coverage);
}

static uint32_t framebuffer_pixel(uint32_t x, uint32_t y) {
  return FRAMEBUFFER_ADDRESS + (y * 8u + x) * 2u;
}

static void case_triangle_edge_interior_outside(void) {
  /* The setup modes are literal raw RDP words. The first four triangle words
   * independently encode (1,1), (4,3), (1,5) in the raw Edge layout. */
  static const uint64_t commands[] = {
      UINT64_C(0x3f10000700005000), /* RGBA16 color image, width 8 */
      UINT64_C(0x2d00000000020020), /* scissor [0,8) x [0,8) */
      UINT64_C(0x2f00000000000000), /* explicitly select 1-cycle mode */
      UINT64_C(0x3700000007c107c1), /* nonzero green fill register control */
      UINT64_C(0x3a00000000ff00ff), /* primitive green, alpha one */
      UINT64_C(0x08000014000c0004), /* Edge W0: YL=5, YM=3, YH=1 */
      UINT64_C(0x00010000fffe8000), /* W1: XL=1, DxLDy=-1.5 */
      UINT64_C(0x0001000000000000), /* W2: XH=1, DxHDy=0 */
      UINT64_C(0x0004000000018000), /* W3: XM=4, DxMDy=+1.5 */
  };
  static const uint8_t primitive_pixel[] = {0x07u, 0xc1u};
  static const uint8_t untouched_pixel[] = {0xa5u, 0xa5u};
  const uint64_t raw_w0 = UINT64_C(0x08000014000c0004);
  const uint64_t raw_w1 = UINT64_C(0x00010000fffe8000);
  t64_machine *machine = create_authored_machine();
  uint32_t inside;
  uint32_t outside;

  if (machine == NULL) return;
  T64_CHECK_INT((raw_w0 >> 56) & UINT64_C(0x3f), UINT64_C(0x08));
  T64_CHECK_INT((raw_w0 >> 55) & UINT64_C(1), 0u);
  T64_CHECK_INT((raw_w0 >> 32) & UINT64_C(0x3fff), 20u);
  T64_CHECK_INT((raw_w0 >> 16) & UINT64_C(0x3fff), 12u);
  T64_CHECK_INT(raw_w0 & UINT64_C(0x3fff), 4u);
  T64_CHECK_INT((raw_w1 >> 32) & UINT64_C(0xffffffff),
                UINT64_C(0x00010000));
  T64_CHECK_INT(raw_w1 & UINT64_C(0xffffffff), UINT64_C(0xfffe8000));

  T64_CHECK(run_commands(machine, commands,
                         sizeof commands / sizeof commands[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT(machine->rdp.last_opcode_field, UINT32_C(0x08));
  T64_CHECK(machine->rdp.last_original_word == raw_w0);
  inside = framebuffer_pixel(1u, 2u);
  outside = framebuffer_pixel(5u, 2u);
  T64_CHECK(memcmp(machine->ram + inside, primitive_pixel,
                   sizeof primitive_pixel) == 0);
  T64_CHECK(memcmp(machine->ram + outside, untouched_pixel,
                   sizeof untouched_pixel) == 0);
  t64_machine_destroy(machine);
}

static void case_triangle_shade_signed_dx(void) {
  static const uint64_t commands[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
      UINT64_C(0x2f00000000000000), UINT64_C(0x0c000014000c0004),
      UINT64_C(0x00010000fffe8000), UINT64_C(0x0001000000000000),
      UINT64_C(0x0004000000018000), UINT64_C(0x00ff0000000000ff),
      UINT64_C(0xffe0000000000000), UINT64_C(0), UINT64_C(0),
      UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0),
  };
  static const uint8_t expected[] = {0xe8u, 0x01u};
  t64_machine *machine = create_authored_machine();
  uint32_t inside = framebuffer_pixel(1u, 2u);
  if (machine == NULL) return;
  T64_CHECK_INT((commands[3] >> 56) & UINT64_C(0x3f), UINT64_C(0x0c));
  T64_CHECK_INT((commands[7] >> 48) & UINT64_C(0xffff), UINT64_C(0x00ff));
  T64_CHECK_INT((commands[8] >> 48) & UINT64_C(0xffff), UINT64_C(0xffe0));
  T64_CHECK(run_commands(machine, commands,
                         sizeof commands / sizeof commands[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK(memcmp(machine->ram + inside, expected, sizeof expected) == 0);
  t64_machine_destroy(machine);
}

static void case_triangle_shade_all_fields(void) {
  /* Literal W4-W11: RGBA bases (32.25,64.5,96.75,143.25),
   * Dx (8.5,-4.25,12.75,-8.5), De (16.25,8.75,-4.5,2.25),
   * Dy (4.75,12.25,8.5,-6.75). The major slope is 1/2.
   * At centers (2.5,2.5), (2.5,3.5), (1.5,2.5), the independently
   * floored RGBA results are (57,86,115,128), (66,105,115,127),
   * (49,90,102,136); the first two discriminate the alpha threshold. */
  static const uint64_t commands[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
      UINT64_C(0x2f00000000000000), UINT64_C(0x0c000014000c0004),
      UINT64_C(0x00010000fffe8000), UINT64_C(0x0001000000008000),
      UINT64_C(0x0004000000018000), UINT64_C(0x002000400060008f),
      UINT64_C(0x0008fffb000cfff7), UINT64_C(0x40008000c0004000),
      UINT64_C(0x8000c000c0008000), UINT64_C(0x00100008fffb0002),
      UINT64_C(0x0004000c0008fff9), UINT64_C(0x4000c00080004000),
      UINT64_C(0xc000400080004000),
  };
  static const uint8_t expected[][2] = {{0x3au, 0x9du}, {0x43u, 0x5cu},
                                       {0x32u, 0xd9u}};
  static const uint32_t x[] = {2u, 2u, 1u};
  static const uint32_t y[] = {2u, 3u, 2u};
  t64_machine *machine = create_authored_machine();
  size_t i;
  if (machine == NULL) return;
  T64_CHECK(run_commands(machine, commands,
                         sizeof commands / sizeof commands[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  for (i = 0u; i < sizeof x / sizeof x[0]; i++) {
    T64_CHECK(memcmp(machine->ram + framebuffer_pixel(x[i], y[i]),
                     expected[i], sizeof expected[i]) == 0);
    T64_CHECK(machine->rdp_triangle_coverage[
                  y[i] * T64_RDP_TRIANGLE_COVERAGE_WIDTH + x[i]] != 0u);
  }
  t64_machine_destroy(machine);
}

static void case_triangle_custom_profile_alignment(void) {
  const t64_host_allocator allocator = {test_allocate, test_release, NULL};
  const t64_profile_descriptor *base = NULL;
  t64_profile_descriptor profile;
  t64_machine *machine = NULL;
  T64_CHECK_INT(t64_profile_lookup(TRIANGLE_PROFILE_ID, &base), T64_STATUS_OK);
  if (base == NULL) return;
  profile = *base;
  (void)strcpy(profile.profile_id, "authored-odd-rdram");
  profile.rdram_bytes--;
  T64_CHECK_INT(t64_machine_create(&allocator, &profile, &machine),
                T64_STATUS_OK);
  if (machine == NULL) return;
  T64_CHECK(machine->rdp_triangle_coverage != NULL);
  T64_CHECK((uintptr_t)machine->rdp_triangle_coverage % _Alignof(uint16_t) == 0u);
  if (machine->rdp_triangle_coverage != NULL &&
      (uintptr_t)machine->rdp_triangle_coverage % _Alignof(uint16_t) == 0u) {
    size_t last = T64_RDP_TRIANGLE_COVERAGE_BYTES / sizeof(uint16_t) - 1u;
    machine->rdp_triangle_coverage[0] = UINT16_C(0x1234);
    machine->rdp_triangle_coverage[last] = UINT16_C(0xabcd);
    T64_CHECK_INT(machine->rdp_triangle_coverage[0], UINT16_C(0x1234));
    T64_CHECK_INT(machine->rdp_triangle_coverage[last], UINT16_C(0xabcd));
    T64_CHECK_INT(machine->ram[profile.rdram_bytes - 1u], 0u);
  }
  t64_machine_destroy(machine);
}

static void case_triangle_right_major_and_negative_x(void) {
  static const uint64_t right_major[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
      UINT64_C(0x2f00000000000000), UINT64_C(0x3a00000000ff00ff),
      UINT64_C(0x08800014000c0004), UINT64_C(0x0004000000018000),
      UINT64_C(0x0004000000000000), UINT64_C(0x00010000fffe8000),
  };
  static const uint64_t negative_x[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
      UINT64_C(0x2f00000000000000), UINT64_C(0x3a00000000ff00ff),
      UINT64_C(0x08000014000c0004), UINT64_C(0xffff000000018000),
      UINT64_C(0xffff000000000000), UINT64_C(0x0002000000018000),
  };
  static const uint8_t green[] = {0x07u, 0xc1u};
  t64_machine *machine = create_authored_machine();
  if (machine == NULL) return;
  T64_CHECK(run_commands(machine, right_major,
                         sizeof right_major / sizeof right_major[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK(memcmp(machine->ram + framebuffer_pixel(2u, 2u), green,
                   sizeof green) == 0);
  T64_CHECK(run_commands(machine, negative_x,
                         sizeof negative_x / sizeof negative_x[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK(memcmp(machine->ram + framebuffer_pixel(0u, 2u), green,
                   sizeof green) == 0);
  t64_machine_destroy(machine);
}

static void case_triangle_degenerate_and_old_profile_refusal(void) {
  static const uint64_t degenerate[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
      UINT64_C(0x2f00000000000000), UINT64_C(0x3a00000000ff00ff),
      UINT64_C(0x0800000400040004), UINT64_C(0x0001000000000000),
      UINT64_C(0x0001000000000000), UINT64_C(0x0001000000000000),
  };
  static const uint64_t old_profile[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
      UINT64_C(0x2f00000000000000), UINT64_C(0x3700000007c107c1),
      UINT64_C(0x08000014000c0004), UINT64_C(0x00010000fffe8000),
      UINT64_C(0x0001000000000000), UINT64_C(0x0004000000018000),
  };
  static const uint8_t untouched[] = {0xa5u, 0xa5u};
  t64_machine *machine = create_authored_machine();
  if (machine == NULL) return;
  T64_CHECK(run_commands(machine, degenerate,
                         sizeof degenerate / sizeof degenerate[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK(memcmp(machine->ram + framebuffer_pixel(1u, 2u), untouched,
                   sizeof untouched) == 0);
  t64_machine_destroy(machine);

  machine = create_authored_machine_for(
      "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v2");
  if (machine == NULL) return;
  T64_CHECK(run_commands(machine, old_profile,
                         sizeof old_profile / sizeof old_profile[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason,
                T64_RDP_REFUSAL_UNSUPPORTED_OPCODE);
  T64_CHECK_INT(machine->rdp.last_opcode_field, UINT32_C(0x08));
  T64_CHECK(machine->rdp.last_original_word == old_profile[4]);
  T64_CHECK_INT(machine->rdp.dpc_current, COMMAND_ADDRESS + UINT32_C(32));
  T64_CHECK(memcmp(machine->ram + framebuffer_pixel(1u, 2u), untouched,
                   sizeof untouched) == 0);
  t64_machine_destroy(machine);
}

static void case_triangle_shared_edge_complementary_samples(void) {
  static const uint64_t lower_right[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d0000000000c00c),
      UINT64_C(0x2f00000000000000), UINT64_C(0x3a00000000ff00ff),
      UINT64_C(0x0800001400040004), UINT64_C(0x0005000000000000),
      UINT64_C(0x0001000000010000), UINT64_C(0x0005000000000000),
  };
  static const uint64_t upper_left[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d0000000000c00c),
      UINT64_C(0x2f00000000000000), UINT64_C(0x3a00000000ff00ff),
      UINT64_C(0x0880001400040004), UINT64_C(0x0001000000000000),
      UINT64_C(0x0001000000010000), UINT64_C(0x0001000000000000),
  };
  t64_machine *machine = create_authored_machine();
  uint16_t right_mask;
  uint16_t left_mask;
  if (machine == NULL) return;
  T64_CHECK(run_commands(machine, lower_right,
                         sizeof lower_right / sizeof lower_right[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT(machine->rdp.triangle_last_coverage_x, 2u);
  T64_CHECK_INT(machine->rdp.triangle_last_coverage_y, 2u);
  right_mask = machine->rdp.triangle_last_coverage_mask;
  T64_CHECK(run_commands(machine, upper_left,
                         sizeof upper_left / sizeof upper_left[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  left_mask = machine->rdp.triangle_last_coverage_mask;
  T64_CHECK((right_mask & left_mask) == 0u);
  T64_CHECK((uint16_t)(right_mask | left_mask) == UINT16_MAX);
  T64_CHECK_INT(machine->rdp_triangle_coverage[
                    2u * T64_RDP_TRIANGLE_COVERAGE_WIDTH + 2u],
                UINT16_MAX);
  t64_machine_destroy(machine);
}

static void case_triangle_single_sample_and_max_extent(void) {
  static const uint64_t single_sample[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000004004),
      UINT64_C(0x2f00000000000000), UINT64_C(0x3a00000000ff00ff),
      UINT64_C(0x0800000200020001), UINT64_C(0x0000800000000000),
      UINT64_C(0x0000800000000000), UINT64_C(0x0000e00000018000),
  };
  static const uint8_t green[] = {0x07u, 0xc1u};
  static const uint64_t max_extent[] = {
      UINT64_C(0x3f1003ff00005000), UINT64_C(0x2d00000000ffc004),
      UINT64_C(0x2f00000000000000), UINT64_C(0x3a00000000ff00ff),
      UINT64_C(0x0800000400000000), UINT64_C(0x0400000000000000),
      UINT64_C(0x0000000000000000), UINT64_C(0x0400000000000000),
  };
  t64_machine *machine = create_authored_machine();
  if (machine == NULL) return;
  T64_CHECK(run_commands(machine, single_sample,
                         sizeof single_sample / sizeof single_sample[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT(machine->rdp.triangle_last_coverage_mask, UINT16_C(0x0040));
  T64_CHECK_INT(machine->rdp_triangle_coverage[0], UINT16_C(0x0040));
  T64_CHECK(memcmp(machine->ram + framebuffer_pixel(0u, 0u), green,
                   sizeof green) == 0);
  t64_machine_destroy(machine);

  /* The widest legal color image is clipped by the largest representable
   * scissor endpoint; the final visible pixel must stay inside RDRAM. */
  machine = create_authored_machine();
  if (machine == NULL) return;
  T64_CHECK(run_commands(machine, max_extent,
                         sizeof max_extent / sizeof max_extent[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK(memcmp(machine->ram + UINT32_C(0x5000) + 1022u * 2u, green,
                   sizeof green) == 0);
  t64_machine_destroy(machine);
}

static void case_triangle_coverage_state_digest_reset_isolation(void) {
  static const uint64_t single_sample[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000004004),
      UINT64_C(0x2f00000000000000), UINT64_C(0x3a00000000ff00ff),
      UINT64_C(0x0800000200020001), UINT64_C(0x0000800000000000),
      UINT64_C(0x0000800000000000), UINT64_C(0x0000e00000018000),
  };
  t64_machine *machine_a = create_authored_machine();
  t64_machine *machine_b = create_authored_machine();
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after_render[T64_DIGEST_SIZE];
  uint8_t digest_after_plane_edit[T64_DIGEST_SIZE];
  uint8_t digest_b[T64_DIGEST_SIZE];
  uint8_t *rom_copy = NULL;
  size_t image_length;

  if (machine_a == NULL || machine_b == NULL) {
    t64_machine_destroy(machine_a);
    t64_machine_destroy(machine_b);
    return;
  }
  T64_CHECK(machine_a->rdp_triangle_coverage != NULL);
  T64_CHECK(machine_b->rdp_triangle_coverage != NULL);
  T64_CHECK(machine_a->rdp_triangle_coverage !=
            machine_b->rdp_triangle_coverage);
  T64_CHECK_INT(machine_a->rdp_triangle_coverage[0], 0u);
  T64_CHECK_INT(machine_b->rdp_triangle_coverage[0], 0u);
  T64_CHECK_INT(t64_machine_digest(machine_a, digest_before), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(machine_b, digest_b), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_b, sizeof digest_before) == 0);

  T64_CHECK(run_commands(machine_a, single_sample,
                         sizeof single_sample / sizeof single_sample[0]));
  T64_CHECK_INT(machine_a->rdp_triangle_coverage[0], UINT16_C(0x0040));
  T64_CHECK_INT(machine_b->rdp_triangle_coverage[0], 0u);
  T64_CHECK_INT(t64_machine_digest(machine_a, digest_after_render),
                T64_STATUS_OK);
  machine_a->rdp_triangle_coverage[1234u] = UINT16_C(0x8001);
  T64_CHECK_INT(t64_machine_digest(machine_a, digest_after_plane_edit),
                T64_STATUS_OK);
  T64_CHECK(memcmp(digest_after_render, digest_after_plane_edit,
                   sizeof digest_after_render) != 0);
  T64_CHECK_INT(t64_machine_digest(machine_b, digest_b), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_b, sizeof digest_before) == 0);

  image_length = machine_a->image_length;
  rom_copy = (uint8_t *)malloc(image_length);
  T64_CHECK(rom_copy != NULL);
  if (rom_copy != NULL) {
    (void)memcpy(rom_copy, machine_a->image_storage.data, image_length);
    T64_CHECK_INT(t64_machine_admit_rom(machine_a, rom_copy, image_length),
                  T64_STATUS_OK);
    T64_CHECK_INT(machine_a->rdp_triangle_coverage[0], 0u);
    T64_CHECK_INT(machine_a->rdp_triangle_coverage[1234u], 0u);
    T64_CHECK_INT(t64_machine_digest(machine_a, digest_after_plane_edit),
                  T64_STATUS_OK);
    T64_CHECK(memcmp(digest_before, digest_after_plane_edit,
                     sizeof digest_before) == 0);
    free(rom_copy);
  }
  t64_machine_destroy(machine_a);
  t64_machine_destroy(machine_b);
}

static void case_triangle_extreme_coefficients_and_order(void) {
  static const uint64_t extreme[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
      UINT64_C(0x2f00000000000000), UINT64_C(0x3a00000000ff00ff),
      UINT64_C(0x0800000400003ffc), UINT64_C(0x7fffffff7fffffff),
      UINT64_C(0x800000007fffffff), UINT64_C(0x7fffffff7fffffff),
  };
  static const uint64_t overlap[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
      UINT64_C(0x2f00000000000000), UINT64_C(0x3a00000000ff00ff),
      UINT64_C(0x08000014000c0004), UINT64_C(0x00010000fffe8000),
      UINT64_C(0x0001000000000000), UINT64_C(0x0004000000018000),
      UINT64_C(0x3a000000ff0000ff),
      UINT64_C(0x08000014000c0004), UINT64_C(0x00010000fffe8000),
      UINT64_C(0x0001000000000000), UINT64_C(0x0004000000018000),
  };
  static const uint8_t red[] = {0xf8u, 0x01u};
  static const uint8_t untouched[] = {0xa5u, 0xa5u};
  t64_machine *machine = create_authored_machine();
  if (machine == NULL) return;
  T64_CHECK(run_commands(machine, extreme,
                         sizeof extreme / sizeof extreme[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK(memcmp(machine->ram + framebuffer_pixel(1u, 2u), untouched,
                   sizeof untouched) == 0);
  T64_CHECK(run_commands(machine, overlap,
                         sizeof overlap / sizeof overlap[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK(memcmp(machine->ram + framebuffer_pixel(1u, 2u), red,
                   sizeof red) == 0);
  t64_machine_destroy(machine);
}

static void case_triangle_shade_overflow_preflight(void) {
  static const uint64_t commands[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
      UINT64_C(0x2f00000000000000), UINT64_C(0x0c000014000c0004),
      UINT64_C(0x00010000fffe8000), UINT64_C(0x0001000000000000),
      UINT64_C(0x0004000000018000), UINT64_C(0),
      UINT64_C(0x8000000000000000), UINT64_C(0), UINT64_C(0),
      UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0),
  };
  static const uint8_t untouched[] = {0xa5u, 0xa5u};
  t64_machine *machine = create_authored_machine();
  if (machine == NULL) return;
  T64_CHECK(run_commands(machine, commands,
                         sizeof commands / sizeof commands[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_RENDER_RANGE);
  T64_CHECK_INT(machine->rdp.dpc_current, COMMAND_ADDRESS + UINT32_C(24));
  T64_CHECK(memcmp(machine->ram + framebuffer_pixel(1u, 2u), untouched,
                   sizeof untouched) == 0);
  t64_machine_destroy(machine);
}

static void case_triangle_composite_lengths_and_truncations(void) {
  static const uint32_t opcodes[] = {0x08u, 0x09u, 0x0au, 0x0bu,
                                     0x0cu, 0x0du, 0x0eu, 0x0fu};
  static const size_t lengths[] = {4u, 6u, 12u, 14u, 12u, 14u, 20u, 22u};
  static const size_t group_prefixes[8][4] = {
      {3u, 0u, 0u, 0u}, {3u, 5u, 0u, 0u},
      {3u, 11u, 0u, 0u}, {3u, 11u, 13u, 0u},
      {3u, 11u, 0u, 0u}, {3u, 11u, 13u, 0u},
      {3u, 11u, 19u, 0u}, {3u, 11u, 19u, 21u},
  };
  static const size_t group_counts[] = {1u, 2u, 2u, 3u, 2u, 3u, 3u, 4u};
  size_t i;

  for (i = 0u; i < sizeof opcodes / sizeof opcodes[0]; i++) {
    size_t boundary;
    T64_CHECK(lengths[i] == t64_rdp_triangle_word_count(opcodes[i]));
    for (boundary = 0u; boundary < group_counts[i]; boundary++) {
      uint64_t commands[3u + T64_RDP_TRIANGLE_MAX_WORDS] = {
          UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
          UINT64_C(0x2f00000000000000),
      };
      size_t j;
      size_t packet_words = group_prefixes[i][boundary];
      size_t count = 3u + packet_words;
      t64_machine *machine;
      raster_snapshot snapshot;
      commands[3] = ((uint64_t)opcodes[i] << 56) |
                    UINT64_C(0x000014000c0004);
      for (j = 4u; j < count; j++)
        commands[j] = UINT64_C(0x1111111111111111) * (uint64_t)(j - 3u);
      machine = create_authored_machine();
      if (machine == NULL) return;
      T64_CHECK(run_commands(machine, commands, 3u));
      T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
      machine->rdp_triangle_coverage[1234u] = UINT16_C(0x8001);
      if (!snapshot_raster(machine, &snapshot)) {
        t64_machine_destroy(machine);
        return;
      }
      T64_CHECK(run_commands(machine, commands, count));
      T64_CHECK_INT(machine->rdp.refusal_reason,
                    T64_RDP_REFUSAL_TRUNCATED_WORD);
      T64_CHECK_INT(machine->rdp.dpc_current,
                    COMMAND_ADDRESS + UINT32_C(24));
      T64_CHECK(memcmp(machine->ram + framebuffer_pixel(1u, 2u),
                       (const uint8_t[]){0xa5u, 0xa5u}, 2u) == 0);
      check_raster_unchanged(machine, &snapshot);
      t64_machine_destroy(machine);
    }
  }

  {
    uint64_t commands[3u + T64_RDP_TRIANGLE_MAX_WORDS] = {
        UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
        UINT64_C(0x2f00000000000000),
    };
    t64_machine *machine;
    raster_snapshot snapshot;
    commands[3] = UINT64_C(0x0e000014000c0004);
    for (i = 4u; i < 3u + lengths[6]; i++)
      commands[i] = UINT64_C(0x2222222222222222) * (uint64_t)(i - 3u);
    machine = create_authored_machine();
    if (machine == NULL) return;
    T64_CHECK(run_commands(machine, commands, 3u));
    T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
    if (!snapshot_raster(machine, &snapshot)) {
      t64_machine_destroy(machine);
      return;
    }
    T64_CHECK(run_commands(machine, commands, 3u + lengths[6]));
    T64_CHECK_INT(machine->rdp.refusal_reason,
                  T64_RDP_REFUSAL_UNSUPPORTED_OPCODE);
    T64_CHECK_INT(machine->rdp.dpc_current,
                  COMMAND_ADDRESS + UINT32_C(24));
    T64_CHECK(memcmp(machine->ram + framebuffer_pixel(1u, 2u),
                     (const uint8_t[]){0xa5u, 0xa5u}, 2u) == 0);
    check_raster_unchanged(machine, &snapshot);
    t64_machine_destroy(machine);
  }

  {
    uint64_t commands[] = {
        UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
        UINT64_C(0x2f00000000000000), UINT64_C(0x3a00000000ff00ff),
        UINT64_C(0x08000014000c0004), UINT64_C(0x00010000fffe8000),
        UINT64_C(0x0001000000000000), UINT64_C(0x0004000000018000),
        UINT64_C(0x0c000014000c0004), UINT64_C(0x00010000fffe8000),
        UINT64_C(0x0001000000000000), UINT64_C(0x0004000000018000),
        UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0),
        UINT64_C(0), UINT64_C(0), UINT64_C(0),
  };
    t64_machine *machine = create_authored_machine();
    raster_snapshot snapshot;
    if (machine == NULL) return;
    T64_CHECK(run_commands(machine, commands, 8u));
    T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
    if (!snapshot_raster(machine, &snapshot)) {
      t64_machine_destroy(machine);
      return;
    }
    T64_CHECK(submit_commands(machine, commands,
                              sizeof commands / sizeof commands[0]));
    T64_CHECK_INT(machine->rdp.refusal_reason,
                  T64_RDP_REFUSAL_TRUNCATED_WORD);
    T64_CHECK_INT(machine->rdp.dpc_current, COMMAND_ADDRESS + UINT32_C(64));
    T64_CHECK(memcmp(machine->ram + framebuffer_pixel(1u, 2u),
                     (const uint8_t[]){0x07u, 0xc1u}, 2u) == 0);
    check_raster_unchanged(machine, &snapshot);
    t64_machine_destroy(machine);
  }
}

typedef struct {
  const char *name;
  void (*run)(void);
} test_case;

static const test_case cases[] = {
    {"triangle-edge-interior-outside", case_triangle_edge_interior_outside},
    {"triangle-shade-signed-dx", case_triangle_shade_signed_dx},
    {"triangle-shade-all-fields", case_triangle_shade_all_fields},
    {"triangle-custom-profile-alignment", case_triangle_custom_profile_alignment},
    {"triangle-right-major-negative-x",
     case_triangle_right_major_and_negative_x},
    {"triangle-degenerate-old-profile-refusal",
     case_triangle_degenerate_and_old_profile_refusal},
    {"triangle-composite-length-truncation",
     case_triangle_composite_lengths_and_truncations},
    {"triangle-shared-edge-complementary-samples",
     case_triangle_shared_edge_complementary_samples},
    {"triangle-single-sample-max-extent",
     case_triangle_single_sample_and_max_extent},
    {"triangle-extreme-coefficients-overlap-order",
     case_triangle_extreme_coefficients_and_order},
    {"triangle-shade-overflow-preflight",
     case_triangle_shade_overflow_preflight},
    {"triangle-coverage-state-digest-reset-isolation",
     case_triangle_coverage_state_digest_reset_isolation},
};

int main(int argc, char **argv) {
  size_t i;

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) puts(cases[i].name);
    return 0;
  }
  if (argc != 1) return 2;
  for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) cases[i].run();
  return T64_TEST_FINISH("unit.core.rdp.triangles");
}
