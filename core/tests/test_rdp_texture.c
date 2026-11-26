#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_internal.h"
#include "t64_test.h"
#include "tendies64/t64_machine.h"

#define TRIANGLE_PROFILE_ID \
  "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-texture-v4"
#define TEST_ROM_CAPACITY 65536u
#define COMMAND_ADDRESS UINT32_C(0x100)
#define FRAMEBUFFER_ADDRESS UINT32_C(0x5000)
#define DP_START UINT32_C(0x04100000)
#define DP_END (DP_START + UINT32_C(4))
#define VI_BASE UINT32_C(0x04400000)

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
  const t64_host_allocator allocator =
      T64_HOST_ALLOCATOR_INIT(test_allocate, test_release, NULL);
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
  T64_CHECK_INT(t64_machine_create(
                    &(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(
                        &allocator, profile),
                    &machine),
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

static size_t dispatch_some(t64_machine *machine, size_t maximum_steps) {
  size_t step;
  for (step = 0u; step < maximum_steps && machine->queue_count != 0u; step++) {
    t64_event event = machine->events[machine->queue_head];
    machine->queue_head = (machine->queue_head + 1u) %
                          T64_EVENT_QUEUE_CAPACITY;
    machine->queue_count--;
    machine->tick = event.tick;
    T64_CHECK(event.handler != NULL);
    if (event.handler != NULL)
      T64_CHECK_INT(event.handler(machine, &event), T64_STATUS_OK);
  }
  return step;
}

static bool queue_commands(t64_machine *machine, const uint64_t *words,
                           size_t count) {
  size_t i;
  uint32_t end;
  if (machine == NULL || words == NULL || count == 0u ||
      count > (UINT32_MAX - COMMAND_ADDRESS) / 8u) return false;
  machine->queue_head = 0u;
  machine->queue_count = 0u;
  for (i = 0u; i < count; i++)
    write_be64(machine->ram + COMMAND_ADDRESS + i * 8u, words[i]);
  end = COMMAND_ADDRESS + (uint32_t)(count * 8u);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_START, 4u,
                                      COMMAND_ADDRESS), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_END, 4u, end),
                T64_STATUS_OK);
  return true;
}

static bool run_commands(t64_machine *machine, const uint64_t *words,
                         size_t count) {
  return queue_commands(machine, words, count) && dispatch_all(machine);
}

static uint32_t framebuffer_pixel(uint32_t x, uint32_t y) {
  return FRAMEBUFFER_ADDRESS + (y * 8u + x) * 2u;
}

static const uint64_t perspective_texture_scene[] = {
    UINT64_C(0x3f10000700005000),
    UINT64_C(0x2d00000000020020),
    UINT64_C(0x2f080c0000000000),
    UINT64_C(0x3510040000080200),
    UINT64_C(0x320000000001c01c),
    UINT64_C(0x0a80002000100000),
    UINT64_C(0x0000000000000000),
    UINT64_C(0x0004000000000000),
    UINT64_C(0x00000000ffff0000),
    UINT64_C(0x0006fffe00020000),
    UINT64_C(0x0000000000000000),
    UINT64_C(0x0000000000000000),
    UINT64_C(0x8000400040000000),
    UINT64_C(0x0000000000000000),
    UINT64_C(0x0000000000000000),
    UINT64_C(0x0000000000000000),
    UINT64_C(0x0000000000000000),
};

static const uint64_t affine_filter_scene[] = {
    UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
    UINT64_C(0x2f000c0000000000), UINT64_C(0x3510040000080200),
    UINT64_C(0x320000000001c01c), UINT64_C(0x0a80002000100000),
    UINT64_C(0), UINT64_C(0x0004000000000000),
    UINT64_C(0x00000000ffff0000), UINT64_C(0x0005000000010000),
    UINT64_C(0), UINT64_C(0), UINT64_C(0x8000000000000000),
    UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0),
};

static void set_logical_texel16(t64_machine *machine, uint32_t x, uint32_t y,
                                uint16_t value) {
  size_t offset = (size_t)y * 16u + (size_t)x * 2u;
  if ((y & 1u) != 0u) offset ^= 4u;
  machine->rdp.tmem[offset] = (uint8_t)(value >> 8);
  machine->rdp.tmem[offset + 1u] = (uint8_t)value;
}

static void set_logical_texel16_at_word(t64_machine *machine,
                                        uint32_t tmem_word, uint32_t x,
                                        uint32_t y, uint16_t value) {
  size_t offset = (size_t)tmem_word * 8u + (size_t)y * 16u +
                  (size_t)x * 2u;
  if ((y & 1u) != 0u) offset ^= 4u;
  machine->rdp.tmem[offset] = (uint8_t)(value >> 8);
  machine->rdp.tmem[offset + 1u] = (uint8_t)value;
}

static void fill_logical_tile16(t64_machine *machine, uint16_t value) {
  uint32_t x, y;
  for (y = 0u; y < 8u; y++)
    for (x = 0u; x < 8u; x++) set_logical_texel16(machine, x, y, value);
}

static bool capture_raw_vi(t64_machine *machine, uint8_t *bytes,
                           size_t capacity, size_t *length_out) {
  static const struct {
    uint32_t offset;
    uint32_t value;
  } writes[] = {
      {UINT32_C(0x04), FRAMEBUFFER_ADDRESS},
      {UINT32_C(0x08), 8u},
      {UINT32_C(0x1c), 3u},
      {UINT32_C(0x18), 0u},
      {UINT32_C(0x24), 8u},
      {UINT32_C(0x28), 8u},
      {UINT32_C(0x30), UINT32_C(1024)},
      {UINT32_C(0x34), UINT32_C(1024)},
      {UINT32_C(0x00), UINT32_C(2)},
  };
  t64_video_packet_info info = T64_VIDEO_PACKET_INFO_INIT;
  size_t i, step;

  for (i = 0u; i < sizeof writes / sizeof writes[0]; i++)
    T64_CHECK_INT(t64_machine_bus_write(machine, VI_BASE + writes[i].offset,
                                        4u, writes[i].value),
                  T64_STATUS_OK);
  for (step = 0u; step < 32u && machine->vi.packet_count == 0u; step++) {
    t64_event event;
    if (machine->queue_count == 0u) break;
    event = machine->events[machine->queue_head];
    machine->queue_head = (machine->queue_head + 1u) %
                          T64_EVENT_QUEUE_CAPACITY;
    machine->queue_count--;
    machine->tick = event.tick;
    T64_CHECK(event.handler != NULL);
    if (event.handler != NULL)
      T64_CHECK_INT(event.handler(machine, &event), T64_STATUS_OK);
  }
  T64_CHECK(machine->vi.packet_count > 0u);
  if (machine->vi.packet_count == 0u) return false;
  T64_CHECK_INT(t64_video_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.width, 8u);
  T64_CHECK_INT(info.height, 8u);
  T64_CHECK(info.packet_bytes <= capacity);
  if (info.packet_bytes > capacity) return false;
  T64_CHECK_INT(t64_video_copy(machine, bytes, capacity, length_out),
                T64_STATUS_OK);
  return true;
}

static void case_texture_perspective_nonzero_signed_q16(void) {
  const uint64_t *commands = perspective_texture_scene;
  static const uint8_t red[] = {0xf8u, 0x01u};
  static const uint8_t untouched[] = {0xa5u, 0xa5u};
  uint8_t raw_vi[8u * 8u * 4u];
  size_t raw_vi_length = 0u;
  const uint64_t raw_w0 = UINT64_C(0x0a80002000100000);
  const uint64_t raw_w4 = UINT64_C(0x0006fffe00020000);
  const uint64_t raw_w7 = UINT64_C(0x8000400040000000);
  t64_machine *machine = create_authored_machine();
  uint32_t inside = framebuffer_pixel(1u, 4u);
  uint32_t outside = framebuffer_pixel(6u, 4u);

  if (machine == NULL) return;
  memset(machine->ram + FRAMEBUFFER_ADDRESS, 0xa5, 8u * 8u * 2u);
  fill_logical_tile16(machine, 0u);
  machine->rdp.tmem[6] = 0xf8u;  /* independent logical texel (3,0) */
  machine->rdp.tmem[7] = 0x01u;
  machine->rdp.tmem[8] = 0x07u;  /* independent logical texel (4,0) */
  machine->rdp.tmem[9] = 0xc1u;
  machine->rdp.tmem[122] = 0x00u; /* logical texel (3,7), odd bank */
  machine->rdp.tmem[123] = 0x3fu;
  machine->rdp.tmem[124] = 0xffu; /* logical texel (4,7), odd bank */
  machine->rdp.tmem[125] = 0xffu;

  T64_CHECK_INT((raw_w0 >> 56) & UINT64_C(0x3f), UINT64_C(0x0a));
  T64_CHECK_INT((raw_w0 >> 55) & UINT64_C(1), 1u);
  T64_CHECK_INT((raw_w0 >> 32) & UINT64_C(0x3fff), 32u);
  T64_CHECK_INT((raw_w0 >> 16) & UINT64_C(0x3fff), 16u);
  T64_CHECK_INT(raw_w0 & UINT64_C(0x3fff), 0u);
  T64_CHECK_INT((raw_w4 >> 32) & UINT64_C(0xffff), UINT64_C(0xfffe));
  T64_CHECK_INT((raw_w4 >> 16) & UINT64_C(0xffff), 2u);
  T64_CHECK_INT((raw_w7 >> 48) & UINT64_C(0xffff), UINT64_C(0x8000));
  T64_CHECK_INT((raw_w7 >> 32) & UINT64_C(0xffff), UINT64_C(0x4000));
  T64_CHECK_INT((raw_w7 >> 16) & UINT64_C(0xffff), UINT64_C(0x4000));

  T64_CHECK(run_commands(machine, commands,
                         sizeof perspective_texture_scene /
                             sizeof perspective_texture_scene[0]));
  T64_CHECK_INT(machine->rdp.last_opcode_field, UINT32_C(0x0a));
  T64_CHECK(machine->rdp.last_original_word == raw_w0);
  T64_CHECK_INT(machine->rdp.dpc_current,
                COMMAND_ADDRESS + (uint32_t)(sizeof perspective_texture_scene /
                                              sizeof perspective_texture_scene[0]) *
                                      UINT32_C(8));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK(memcmp(machine->ram + inside, red, sizeof red) == 0);
  T64_CHECK(memcmp(machine->ram + outside, untouched, sizeof untouched) == 0);
  T64_CHECK(capture_raw_vi(machine, raw_vi, sizeof raw_vi, &raw_vi_length));
  T64_CHECK_INT(raw_vi_length, sizeof raw_vi);
  T64_CHECK(raw_vi[132] == 255u && raw_vi[133] == 0u &&
            raw_vi[134] == 0u && raw_vi[135] == 255u);
  t64_machine_destroy(machine);
}

static void case_texture_fetched_load_tile_to_vi(void) {
  static const uint64_t commands[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
      UINT64_C(0x2f080c0000000000), UINT64_C(0x3d10000700004000),
      UINT64_C(0x3510040000080200), UINT64_C(0x320000000001c01c),
      UINT64_C(0x340000000001c01c),
      UINT64_C(0x0a80002000100000), UINT64_C(0),
      UINT64_C(0x0004000000000000), UINT64_C(0x00000000ffff0000),
      UINT64_C(0x0006fffe00020000), UINT64_C(0), UINT64_C(0),
      UINT64_C(0x8000400040000000), UINT64_C(0), UINT64_C(0),
      UINT64_C(0), UINT64_C(0),
  };
  static const uint8_t red[] = {0xf8u, 0x01u};
  static const uint8_t untouched[] = {0xa5u, 0xa5u};
  static const uint8_t red_rgba[] = {255u, 0u, 0u, 255u};
  t64_machine *machine = create_authored_machine();
  uint8_t raw_vi[8u * 8u * 4u];
  size_t raw_vi_length = 0u;
  uint32_t x, y;

  if (machine == NULL) return;
  memset(machine->ram + FRAMEBUFFER_ADDRESS, 0xa5, 8u * 8u * 2u);
  memset(machine->ram + UINT32_C(0x4000), 0, 8u * 8u * 2u);
  for (y = 0u; y < 8u; y++) {
    for (x = 0u; x < 8u; x++) {
      size_t offset = UINT32_C(0x4000) + ((size_t)y * 8u + x) * 2u;
      uint16_t texel = (x == 3u && y == 0u) ? UINT16_C(0xf801)
                                            : UINT16_C(0x0000);
      machine->ram[offset] = (uint8_t)(texel >> 8);
      machine->ram[offset + 1u] = (uint8_t)texel;
    }
  }
  T64_CHECK_INT((commands[3] >> 56) & UINT64_C(0x3f), UINT64_C(0x3d));
  T64_CHECK_INT((commands[4] >> 56) & UINT64_C(0x3f), UINT64_C(0x35));
  T64_CHECK_INT((commands[5] >> 56) & UINT64_C(0x3f), UINT64_C(0x32));
  T64_CHECK_INT((commands[6] >> 56) & UINT64_C(0x3f), UINT64_C(0x34));
  T64_CHECK(run_commands(machine, commands,
                         sizeof commands / sizeof commands[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK(machine->rdp.tmem[6] == 0xf8u && machine->rdp.tmem[7] == 0x01u);
  T64_CHECK(memcmp(machine->ram + framebuffer_pixel(1u, 4u), red,
                   sizeof red) == 0);
  T64_CHECK(memcmp(machine->ram + framebuffer_pixel(6u, 4u), untouched,
                   sizeof untouched) == 0);
  T64_CHECK(capture_raw_vi(machine, raw_vi, sizeof raw_vi, &raw_vi_length));
  T64_CHECK_INT(raw_vi_length, sizeof raw_vi);
  T64_CHECK(memcmp(raw_vi + 132u, red_rgba, sizeof red_rgba) == 0);
  t64_machine_destroy(machine);
}

static void run_affine_filter_case(uint64_t raw_modes, uint16_t expected,
                                   uint32_t filter_type,
                                   uint32_t midpoint) {
  uint64_t commands[sizeof affine_filter_scene / sizeof affine_filter_scene[0]];
  static const uint16_t pattern[4] = {
      UINT16_C(0xf801), UINT16_C(0x07c1), UINT16_C(0x003f), UINT16_C(0xffff)};
  t64_machine *machine = create_authored_machine();
  if (machine == NULL) return;
  memcpy(commands, affine_filter_scene, sizeof commands);
  commands[2] = raw_modes;
  fill_logical_tile16(machine, 0u);
  set_logical_texel16(machine, 3u, 0u, pattern[0]);
  set_logical_texel16(machine, 4u, 0u, pattern[1]);
  set_logical_texel16(machine, 3u, 1u, pattern[2]);
  set_logical_texel16(machine, 4u, 1u, pattern[3]);
  T64_CHECK_INT((raw_modes >> 56) & UINT64_C(0x3f), UINT64_C(0x2f));
  T64_CHECK_INT((raw_modes >> 45) & UINT64_C(1), filter_type);
  T64_CHECK_INT((raw_modes >> 44) & UINT64_C(1), midpoint);
  T64_CHECK_INT((commands[9] >> 48) & UINT64_C(0xffff), 5u);
  T64_CHECK_INT((commands[12] >> 48) & UINT64_C(0xffff), UINT64_C(0x8000));
  T64_CHECK(run_commands(machine, commands,
                         sizeof commands / sizeof commands[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT((machine->ram[framebuffer_pixel(1u, 4u)] << 8) |
                    machine->ram[framebuffer_pixel(1u, 4u) + 1u],
                expected);
  t64_machine_destroy(machine);
}

static void case_texture_affine_point(void) {
  run_affine_filter_case(UINT64_C(0x2f000c0000000000), UINT16_C(0xf801),
                         0u, 0u);
}

static void case_texture_filter_box(void) {
  run_affine_filter_case(UINT64_C(0x2f003c0000000000), UINT16_C(0x7bdf),
                         1u, 1u);
}

static void case_texture_filter_threepoint(void) {
  run_affine_filter_case(UINT64_C(0x2f002c0000000000), UINT16_C(0x3dc1),
                         1u, 0u);
}

static void case_texture_shade_gradient(void) {
  static const uint64_t commands[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
      UINT64_C(0x2f000c0000000000), UINT64_C(0x3510040000080200),
      UINT64_C(0x320000000001c01c), UINT64_C(0x0e000014000c0004),
      UINT64_C(0x00010000fffe8000), UINT64_C(0x0001000000008000),
      UINT64_C(0x0004000000018000), UINT64_C(0x002000400060008f),
      UINT64_C(0x0008fffb000cfff7), UINT64_C(0x40008000c0004000),
      UINT64_C(0x8000c000c0008000), UINT64_C(0x00100008fffb0002),
      UINT64_C(0x0004000c0008fff9), UINT64_C(0x4000c00080004000),
      UINT64_C(0xc000400080004000), UINT64_C(0x0002000000010000),
      UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0),
      UINT64_C(0), UINT64_C(0), UINT64_C(0),
  };
  t64_machine *machine = create_authored_machine();
  if (machine == NULL) return;
  fill_logical_tile16(machine, UINT16_C(0xffff));
  T64_CHECK_INT((commands[5] >> 56) & UINT64_C(0x3f), UINT64_C(0x0e));
  T64_CHECK_INT((commands[9] >> 48) & UINT64_C(0xffff), UINT64_C(0x0020));
  T64_CHECK_INT((commands[17] >> 48) & UINT64_C(0xffff), 2u);
  T64_CHECK(run_commands(machine, commands,
                         sizeof commands / sizeof commands[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT((machine->ram[framebuffer_pixel(2u, 2u)] << 8) |
                    machine->ram[framebuffer_pixel(2u, 2u) + 1u],
                UINT16_C(0x329b));
  t64_machine_destroy(machine);
}

static void case_texture_lod_detail_sharpen(void) {
  static const uint64_t lod_scene[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
      UINT64_C(0x2f010c0000000000), UINT64_C(0x3510040000080200),
      UINT64_C(0x320000000001c01c), UINT64_C(0x3510041001080200),
      UINT64_C(0x320000000101c01c), UINT64_C(0x0a88002000100000),
      UINT64_C(0), UINT64_C(0x0004000000000000),
      UINT64_C(0x00000000ffff0000), UINT64_C(0x0002000000010000),
      UINT64_C(0), UINT64_C(0), UINT64_C(0xc000000000000000),
      UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0),
  };
  static const struct {
    uint64_t modes;
    uint16_t expected;
  } modes[] = {
      {UINT64_C(0x2f000c0000000000), UINT16_C(0x7bdf)},
      {UINT64_C(0x2f010c0000000000), UINT16_C(0x7bdf)},
      {UINT64_C(0x2f050c0000000000), UINT16_C(0xdef7)},
      {UINT64_C(0x2f030c0000000000), UINT16_C(0x5ad7)},
  };
  static const uint64_t lod_two_scene[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
      UINT64_C(0x2f010c0000000000), UINT64_C(0x3510040000080200),
      UINT64_C(0x320000000001c01c), UINT64_C(0x3510041001080200),
      UINT64_C(0x320000000101c01c), UINT64_C(0x0a88002000100000),
      UINT64_C(0), UINT64_C(0x0004000000000000),
      UINT64_C(0x00000000ffff0000), UINT64_C(0x0002000000010000),
      UINT64_C(0x0002000000000000), UINT64_C(0), UINT64_C(0),
      UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0),
  };
  size_t i;
  for (i = 0u; i < sizeof modes / sizeof modes[0]; i++) {
    uint64_t commands[sizeof lod_scene / sizeof lod_scene[0]];
    t64_machine *machine = create_authored_machine();
    uint32_t x, y;
    if (machine == NULL) continue;
    memcpy(commands, lod_scene, sizeof commands);
    commands[2] = modes[i].modes;
    for (y = 0u; y < 8u; y++) {
      for (x = 0u; x < 8u; x++) {
        set_logical_texel16_at_word(machine, 0u, x, y,
                                    UINT16_C(0x7bdf));
        set_logical_texel16_at_word(machine, 16u, x, y,
                                    UINT16_C(0xffff));
      }
    }
    T64_CHECK(run_commands(machine, commands,
                           sizeof commands / sizeof commands[0]));
    T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
    T64_CHECK_INT((machine->ram[framebuffer_pixel(1u, 4u)] << 8) |
                      machine->ram[framebuffer_pixel(1u, 4u) + 1u],
                  modes[i].expected);
    t64_machine_destroy(machine);
  }
  {
    t64_machine *machine = create_authored_machine();
    if (machine == NULL) return;
    for (uint32_t y = 0u; y < 8u; y++) {
      for (uint32_t x = 0u; x < 8u; x++) {
        set_logical_texel16_at_word(machine, 0u, x, y,
                                    UINT16_C(0x7bdf));
        set_logical_texel16_at_word(machine, 16u, x, y,
                                    UINT16_C(0xffff));
      }
    }
    T64_CHECK_INT((lod_two_scene[12] >> 48) & UINT64_C(0xffff), 2u);
    T64_CHECK(run_commands(machine, lod_two_scene,
                           sizeof lod_two_scene / sizeof lod_two_scene[0]));
    T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
    T64_CHECK_INT((machine->ram[framebuffer_pixel(1u, 4u)] << 8) |
                      machine->ram[framebuffer_pixel(1u, 4u) + 1u],
                  UINT16_C(0xffff));
    t64_machine_destroy(machine);
  }
}

static void case_texture_old_profile_refusal(void) {
  t64_machine *machine = create_authored_machine_for(
      "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-triangle-v3");
  static const uint8_t untouched[] = {0xa5u, 0xa5u};
  if (machine == NULL) return;
  memset(machine->ram + FRAMEBUFFER_ADDRESS, 0xa5, 8u * 8u * 2u);
  T64_CHECK(run_commands(machine, perspective_texture_scene,
                         sizeof perspective_texture_scene /
                             sizeof perspective_texture_scene[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason,
                T64_RDP_REFUSAL_UNSUPPORTED_OPCODE);
  T64_CHECK_INT(machine->rdp.last_opcode_field, UINT32_C(0x0a));
  T64_CHECK(machine->rdp.last_original_word ==
            perspective_texture_scene[5]);
  T64_CHECK_INT(machine->rdp.dpc_current,
                COMMAND_ADDRESS + UINT32_C(40));
  T64_CHECK(memcmp(machine->ram + framebuffer_pixel(1u, 4u), untouched,
                   sizeof untouched) == 0);
  t64_machine_destroy(machine);
}

static void case_texture_profile_v4_identity(void) {
  t64_machine *machine = create_authored_machine();
  if (machine == NULL) return;
  T64_CHECK_INT(machine->profile.descriptor_version, 7u);
  T64_CHECK_INT(machine->profile.machine_model_version, 13u);
  T64_CHECK_INT(machine->profile.state_serialization_version, 18u);
  T64_CHECK(strcmp(machine->profile.profile_id,
                   TRIANGLE_PROFILE_ID) == 0);
  T64_CHECK(strcmp(machine->profile.rdp_triangle_policy,
                   "triangle-twos-complement-q16-logical-coverage-texture-policy-v1") == 0);
  T64_CHECK(machine->rdp_triangle_coverage != NULL);
  T64_CHECK_INT(T64_RDP_TRIANGLE_COVERAGE_BYTES, 2097152u);
  t64_machine_destroy(machine);
}

static void case_texture_mask_mirror_boundary(void) {
  uint64_t commands[sizeof affine_filter_scene / sizeof affine_filter_scene[0]];
  t64_machine *machine = create_authored_machine();
  if (machine == NULL) return;
  memcpy(commands, affine_filter_scene, sizeof commands);
  commands[2] = UINT64_C(0x2f002c0000000000);
  commands[3] = UINT64_C(0x3510040000080130);
  commands[9] = UINT64_C(0x0009000000010000);
  fill_logical_tile16(machine, 0u);
  set_logical_texel16(machine, 7u, 0u, UINT16_C(0xf801));
  set_logical_texel16(machine, 0u, 0u, UINT16_C(0x003f));
  T64_CHECK_INT((commands[3] >> 8) & UINT64_C(3), 1u);
  T64_CHECK_INT((commands[3] >> 4) & UINT64_C(15), 3u);
  T64_CHECK_INT((commands[9] >> 48) & UINT64_C(0xffff), 9u);
  T64_CHECK(run_commands(machine, commands,
                         sizeof commands / sizeof commands[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT((machine->ram[framebuffer_pixel(1u, 4u)] << 8) |
                    machine->ram[framebuffer_pixel(1u, 4u) + 1u],
                UINT16_C(0xf801));
  t64_machine_destroy(machine);
}

static void case_texture_odd_tmem_row(void) {
  static const uint64_t load_scene[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
      UINT64_C(0x2f000c0000000000), UINT64_C(0x3d10000700004000),
      UINT64_C(0x3510040000080200), UINT64_C(0x320000000001c01c),
      UINT64_C(0x340000000001c01c),
      UINT64_C(0x0a80002000100000), UINT64_C(0),
      UINT64_C(0x0004000000000000), UINT64_C(0x00000000ffff0000),
      UINT64_C(0x0003000100010000), UINT64_C(0), UINT64_C(0),
      UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0),
      UINT64_C(0),
  };
  t64_machine *machine = create_authored_machine();
  if (machine == NULL) return;
  memset(machine->ram + UINT32_C(0x4000), 0, 8u * 8u * 2u);
  machine->ram[UINT32_C(0x4000) + 8u * 2u + 3u * 2u] = 0xf8u;
  machine->ram[UINT32_C(0x4000) + 8u * 2u + 3u * 2u + 1u] = 0x01u;
  T64_CHECK(run_commands(machine, load_scene,
                         sizeof load_scene / sizeof load_scene[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK(machine->rdp.tmem[18] == 0xf8u &&
            machine->rdp.tmem[19] == 0x01u);
  T64_CHECK_INT((machine->ram[framebuffer_pixel(1u, 4u)] << 8) |
                    machine->ram[framebuffer_pixel(1u, 4u) + 1u],
                UINT16_C(0xf801));
  t64_machine_destroy(machine);
}

static void case_texture_negative_gradient(void) {
  uint64_t commands[sizeof affine_filter_scene / sizeof affine_filter_scene[0]];
  t64_machine *machine = create_authored_machine();
  if (machine == NULL) return;
  memcpy(commands, affine_filter_scene, sizeof commands);
  commands[9] = UINT64_C(0x0002000000010000);
  commands[10] = UINT64_C(0xffff000000000000);
  commands[12] = UINT64_C(0x8000000000000000);
  fill_logical_tile16(machine, 0u);
  set_logical_texel16(machine, 3u, 0u, UINT16_C(0xf801));
  set_logical_texel16(machine, 4u, 0u, UINT16_C(0x07c1));
  T64_CHECK_INT((commands[10] >> 48) & UINT64_C(0xffff), UINT64_C(0xffff));
  T64_CHECK_INT((commands[12] >> 48) & UINT64_C(0xffff), UINT64_C(0x8000));
  T64_CHECK(run_commands(machine, commands,
                         sizeof commands / sizeof commands[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT((machine->ram[framebuffer_pixel(1u, 4u)] << 8) |
                    machine->ram[framebuffer_pixel(1u, 4u) + 1u],
                UINT16_C(0xf801));
  t64_machine_destroy(machine);
}

static void case_texture_w_zero_sign_policy(void) {
  uint64_t negative[sizeof perspective_texture_scene /
                    sizeof perspective_texture_scene[0]];
  uint64_t zero[sizeof perspective_texture_scene /
                sizeof perspective_texture_scene[0]];
  static const uint8_t red[] = {0xf8u, 0x01u};
  uint8_t prior_pixels[8u * 8u * 2u];
  uint16_t *prior_coverage;
  t64_machine *machine = create_authored_machine();
  if (machine == NULL) return;
  memcpy(negative, perspective_texture_scene, sizeof negative);
  negative[12] = 0u;
  negative[9] = UINT64_C(0x0006fffeffff0000);
  fill_logical_tile16(machine, 0u);
  set_logical_texel16(machine, 0u, 2u, UINT16_C(0xf801));
  T64_CHECK_INT((negative[9] >> 16) & UINT64_C(0xffff), UINT64_C(0xffff));
  T64_CHECK(run_commands(machine, negative,
                         sizeof negative / sizeof negative[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK(memcmp(machine->ram + framebuffer_pixel(1u, 4u), red,
                   sizeof red) == 0);

  memcpy(prior_pixels, machine->ram + FRAMEBUFFER_ADDRESS,
         sizeof prior_pixels);
  prior_coverage = malloc(T64_RDP_TRIANGLE_COVERAGE_BYTES);
  T64_CHECK(prior_coverage != NULL);
  if (prior_coverage == NULL) {
    t64_machine_destroy(machine);
    return;
  }
  memcpy(prior_coverage, machine->rdp_triangle_coverage,
         T64_RDP_TRIANGLE_COVERAGE_BYTES);
  memcpy(zero, perspective_texture_scene, sizeof zero);
  zero[9] = UINT64_C(0x0006fffe00000000);
  zero[12] = 0u;
  T64_CHECK(run_commands(machine, zero, sizeof zero / sizeof zero[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_RENDER_RANGE);
  T64_CHECK_INT(machine->rdp.dpc_current, COMMAND_ADDRESS + 40u);
  T64_CHECK(memcmp(machine->ram + FRAMEBUFFER_ADDRESS, prior_pixels,
                   sizeof prior_pixels) == 0);
  T64_CHECK(memcmp(machine->rdp_triangle_coverage, prior_coverage,
                   T64_RDP_TRIANGLE_COVERAGE_BYTES) == 0);
  /* Complete packet with W=2 and no W derivatives: the excessive signed
   * S derivative must reach arithmetic preflight, not a W-zero refusal. */
  memcpy(zero, perspective_texture_scene, sizeof zero);
  zero[10] = UINT64_C(0x8000000000000000);
  zero[12] = 0u;
  T64_CHECK_INT((zero[9] >> 16) & UINT64_C(0xffff), 2u);
  T64_CHECK(run_commands(machine, zero, sizeof zero / sizeof zero[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_RENDER_RANGE);
  T64_CHECK_INT(machine->rdp.dpc_current, COMMAND_ADDRESS + 40u);
  T64_CHECK(memcmp(machine->ram + FRAMEBUFFER_ADDRESS, prior_pixels,
                   sizeof prior_pixels) == 0);
  T64_CHECK(memcmp(machine->rdp_triangle_coverage, prior_coverage,
                   T64_RDP_TRIANGLE_COVERAGE_BYTES) == 0);
  free(prior_coverage);
  t64_machine_destroy(machine);
}

static void case_texture_fractional_tie(void) {
  uint64_t commands[sizeof affine_filter_scene / sizeof affine_filter_scene[0]];
  t64_machine *machine = create_authored_machine();
  if (machine == NULL) return;
  memcpy(commands, affine_filter_scene, sizeof commands);
  commands[2] = UINT64_C(0x2f002c0000000000);
  commands[9] = UINT64_C(0x0004000000010000);
  commands[11] = UINT64_C(0xc000800000000000);
  fill_logical_tile16(machine, 0u);
  set_logical_texel16(machine, 3u, 0u, UINT16_C(0xf801));
  set_logical_texel16(machine, 4u, 0u, UINT16_C(0x07c1));
  set_logical_texel16(machine, 3u, 1u, UINT16_C(0x07c1));
  set_logical_texel16(machine, 4u, 1u, UINT16_C(0x003f));
  T64_CHECK_INT((commands[11] >> 48) & UINT64_C(0xffff), UINT64_C(0xc000));
  T64_CHECK_INT((commands[11] >> 32) & UINT64_C(0xffff), UINT64_C(0x8000));
  T64_CHECK(run_commands(machine, commands,
                         sizeof commands / sizeof commands[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT((machine->ram[framebuffer_pixel(1u, 4u)] << 8) |
                    machine->ram[framebuffer_pixel(1u, 4u) + 1u],
                UINT16_C(0x781f));
  t64_machine_destroy(machine);
}

static void case_texture_format_matrix(void) {
  static const struct {
    uint32_t format;
    uint32_t size;
    uint16_t expected;
    uint8_t bytes[4];
    uint32_t byte_count;
  } formats[] = {
      {0u, 2u, UINT16_C(0xf801), {0xf8u, 0x01u, 0u, 0u}, 2u},
      {0u, 3u, UINT16_C(0xf801), {0xffu, 0u, 0u, 0xffu}, 4u},
      {1u, 2u, UINT16_C(0x8421), {0x80u, 0x80u, 0x80u, 0x80u}, 4u},
      {2u, 0u, UINT16_C(0xf801), {0u, 0u, 0u, 0u}, 1u},
      {2u, 1u, UINT16_C(0xf801), {0u, 0u, 0u, 0u}, 1u},
      {3u, 0u, UINT16_C(0xffff), {0xf0u, 0u, 0u, 0u}, 1u},
      {3u, 1u, UINT16_C(0xffff), {0xffu, 0u, 0u, 0u}, 1u},
      {3u, 2u, UINT16_C(0xffff), {0xffu, 0xffu, 0u, 0u}, 2u},
      {4u, 0u, UINT16_C(0xffff), {0xf0u, 0u, 0u, 0u}, 1u},
      {4u, 1u, UINT16_C(0xffff), {0xffu, 0u, 0u, 0u}, 1u},
  };
  t64_machine *machine = create_authored_machine();
  size_t i;
  if (machine == NULL) return;
  for (i = 0u; i < sizeof formats / sizeof formats[0]; i++) {
    t64_rdp_tile *tile = &machine->rdp.tiles[0];
    uint16_t actual = 0u;
    memset(machine->rdp.tmem, 0, sizeof machine->rdp.tmem);
    memset(tile, 0, sizeof *tile);
    tile->format = formats[i].format;
    tile->size = formats[i].size;
    tile->line_words = 1u;
    tile->tmem_word = 0u;
    tile->valid = true;
    tile->size_valid = true;
    tile->clamp_s = true;
    tile->clamp_t = true;
    if (formats[i].format == 2u) {
      tile->palette = 0u;
      machine->rdp.tlut_valid = true;
      machine->rdp.tlut_format = 2u;
      machine->rdp.tmem[2048] = 0xf8u;
      machine->rdp.tmem[2049] = 0x01u;
    }
    memcpy(machine->rdp.tmem, formats[i].bytes, formats[i].byte_count);
    if (formats[i].format == 2u) {
      machine->rdp.tmem[2048] = 0xf8u;
      machine->rdp.tmem[2049] = 0x01u;
    }
    T64_CHECK(t64_rdp_tmem_validate_triangle(machine, 0u));
    T64_CHECK(t64_rdp_tmem_triangle_pixel(machine, 0u, 0, 0,
                                          INT64_C(65536), false, &actual));
    T64_CHECK_INT(actual, formats[i].expected);
  }
  t64_machine_destroy(machine);
}

static void case_texture_truncated_group_preserves_pixels(void) {
  static const uint64_t truncated_scene[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
      UINT64_C(0x2f080c0000000000), UINT64_C(0x0a80002000100000),
      UINT64_C(0), UINT64_C(0x0004000000000000),
      UINT64_C(0x00000000ffff0000), UINT64_C(0x0006fffe00020000),
      UINT64_C(0), UINT64_C(0), UINT64_C(0x8000400040000000),
      UINT64_C(0), UINT64_C(0), UINT64_C(0),
  };
  uint8_t prior_pixels[8u * 8u * 2u];
  uint16_t *prior_coverage;
  t64_machine *machine = create_authored_machine();
  if (machine == NULL) return;
  fill_logical_tile16(machine, 0u);
  set_logical_texel16(machine, 3u, 0u, UINT16_C(0xf801));
  T64_CHECK(run_commands(machine, perspective_texture_scene,
                         sizeof perspective_texture_scene /
                             sizeof perspective_texture_scene[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  memcpy(prior_pixels, machine->ram + FRAMEBUFFER_ADDRESS,
         sizeof prior_pixels);
  prior_coverage = (uint16_t *)malloc(T64_RDP_TRIANGLE_COVERAGE_BYTES);
  T64_CHECK(prior_coverage != NULL);
  if (prior_coverage == NULL) {
    t64_machine_destroy(machine);
    return;
  }
  memcpy(prior_coverage, machine->rdp_triangle_coverage,
         T64_RDP_TRIANGLE_COVERAGE_BYTES);
  T64_CHECK_INT((truncated_scene[3] >> 56) & UINT64_C(0x3f),
                UINT64_C(0x0a));
  T64_CHECK_INT(sizeof truncated_scene / sizeof truncated_scene[0], 14u);
  T64_CHECK(run_commands(machine, truncated_scene,
                         sizeof truncated_scene / sizeof truncated_scene[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason,
                T64_RDP_REFUSAL_TRUNCATED_WORD);
  T64_CHECK_INT(machine->rdp.dpc_current,
                COMMAND_ADDRESS + UINT32_C(24));
  T64_CHECK(memcmp(machine->ram + FRAMEBUFFER_ADDRESS, prior_pixels,
                   sizeof prior_pixels) == 0);
  T64_CHECK(memcmp(machine->rdp_triangle_coverage, prior_coverage,
                   T64_RDP_TRIANGLE_COVERAGE_BYTES) == 0);
  free(prior_coverage);
  t64_machine_destroy(machine);
}

static void case_texture_split_digest_interleave(void) {
  t64_machine *whole = create_authored_machine();
  t64_machine *split = create_authored_machine();
  t64_machine *interleaved_a = create_authored_machine();
  t64_machine *interleaved_b = create_authored_machine();
  uint8_t whole_digest[T64_DIGEST_SIZE];
  uint8_t split_digest[T64_DIGEST_SIZE];
  uint8_t other_digest[T64_DIGEST_SIZE];
  uint8_t changed_digest[T64_DIGEST_SIZE];
  size_t steps = 0u;
  if (whole == NULL || split == NULL || interleaved_a == NULL ||
      interleaved_b == NULL) {
    t64_machine_destroy(whole);
    t64_machine_destroy(split);
    t64_machine_destroy(interleaved_a);
    t64_machine_destroy(interleaved_b);
    return;
  }
  fill_logical_tile16(whole, 0u);
  set_logical_texel16(whole, 3u, 0u, UINT16_C(0xf801));
  fill_logical_tile16(split, 0u);
  set_logical_texel16(split, 3u, 0u, UINT16_C(0xf801));
  fill_logical_tile16(interleaved_a, 0u);
  set_logical_texel16(interleaved_a, 3u, 0u, UINT16_C(0xf801));
  fill_logical_tile16(interleaved_b, 0u);
  set_logical_texel16(interleaved_b, 3u, 0u, UINT16_C(0xf801));

  T64_CHECK_INT(t64_machine_digest(whole, whole_digest), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(split, split_digest), T64_STATUS_OK);
  T64_CHECK(memcmp(whole_digest, split_digest, sizeof whole_digest) == 0);
  T64_CHECK(queue_commands(whole, perspective_texture_scene,
                           sizeof perspective_texture_scene /
                               sizeof perspective_texture_scene[0]));
  T64_CHECK(dispatch_all(whole));
  T64_CHECK(queue_commands(split, perspective_texture_scene,
                           sizeof perspective_texture_scene /
                               sizeof perspective_texture_scene[0]));
  T64_CHECK_INT(dispatch_some(split, 1u), 1u);
  T64_CHECK(dispatch_some(split, 3u) <= 3u);
  T64_CHECK(dispatch_all(split));

  T64_CHECK(queue_commands(interleaved_a, perspective_texture_scene,
                           sizeof perspective_texture_scene /
                               sizeof perspective_texture_scene[0]));
  T64_CHECK(queue_commands(interleaved_b, perspective_texture_scene,
                           sizeof perspective_texture_scene /
                               sizeof perspective_texture_scene[0]));
  while ((interleaved_a->queue_count != 0u ||
          interleaved_b->queue_count != 0u) && steps < 4096u) {
    if (interleaved_a->queue_count != 0u)
      (void)dispatch_some(interleaved_a, 1u);
    if (interleaved_b->queue_count != 0u)
      (void)dispatch_some(interleaved_b, 1u);
    steps++;
  }
  T64_CHECK(steps < 4096u);
  T64_CHECK_INT(interleaved_a->queue_count, 0u);
  T64_CHECK_INT(interleaved_b->queue_count, 0u);
  T64_CHECK_INT(t64_machine_digest(whole, whole_digest), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(split, split_digest), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(interleaved_a, other_digest),
                T64_STATUS_OK);
  T64_CHECK(memcmp(whole_digest, split_digest, sizeof whole_digest) == 0);
  T64_CHECK(memcmp(whole_digest, other_digest, sizeof whole_digest) == 0);
  T64_CHECK_INT(t64_machine_digest(interleaved_b, changed_digest),
                T64_STATUS_OK);
  T64_CHECK(memcmp(whole_digest, changed_digest, sizeof whole_digest) == 0);

  split->rdp.tmem[0] ^= UINT8_C(1);
  T64_CHECK_INT(t64_machine_digest(split, changed_digest), T64_STATUS_OK);
  T64_CHECK(memcmp(whole_digest, changed_digest, sizeof whole_digest) != 0);
  T64_CHECK_INT(t64_machine_digest(whole, other_digest), T64_STATUS_OK);
  T64_CHECK(memcmp(whole_digest, other_digest, sizeof whole_digest) == 0);
  split->rdp.tmem[0] ^= UINT8_C(1);
  T64_CHECK_INT(t64_machine_digest(split, split_digest), T64_STATUS_OK);
  T64_CHECK(memcmp(whole_digest, split_digest, sizeof whole_digest) == 0);

  t64_machine_destroy(whole);
  t64_machine_destroy(split);
  t64_machine_destroy(interleaved_a);
  t64_machine_destroy(interleaved_b);
}

static void case_texture_header_tile_and_level(void) {
  size_t variant;
  /* SGI W0 tile=bits50:48, maximum mip level=bits53:51. A named tile
   * must work even with no tile0, and tile0 must never be a fallback. */
  for (variant = 0u; variant < 3u; variant++) {
    uint64_t commands[sizeof perspective_texture_scene /
                      sizeof perspective_texture_scene[0]];
    t64_machine *machine = create_authored_machine();
    uint32_t tile = variant == 2u ? 7u : 1u;
    if (machine == NULL) return;
    if (variant == 1u) {
      T64_CHECK(run_commands(machine, perspective_texture_scene, 5u));
      set_logical_texel16(machine, 3u, 0u, UINT16_C(0xf801));
    }
    memcpy(commands, perspective_texture_scene, sizeof commands);
    commands[3] = UINT64_C(0x3510041000080200) | ((uint64_t)tile << 24);
    commands[4] = UINT64_C(0x320000000001c01c) | ((uint64_t)tile << 24);
    commands[5] = UINT64_C(0x0a80002000100000) | ((uint64_t)tile << 48);
    set_logical_texel16_at_word(machine, 16u, 3u, 0u, UINT16_C(0x07c1));
    T64_CHECK(run_commands(machine, commands,
                           sizeof commands / sizeof commands[0]));
    T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
    T64_CHECK_INT((machine->ram[framebuffer_pixel(1u, 4u)] << 8) |
                      machine->ram[framebuffer_pixel(1u, 4u) + 1u],
                  UINT16_C(0x07c1));
    if (variant == 2u) {
      uint8_t pixels[8u * 8u * 2u];
      uint16_t *coverage = malloc(T64_RDP_TRIANGLE_COVERAGE_BYTES);
      T64_CHECK(coverage != NULL);
      if (coverage != NULL) {
        memcpy(pixels, machine->ram + FRAMEBUFFER_ADDRESS, sizeof pixels);
        memcpy(coverage, machine->rdp_triangle_coverage,
               T64_RDP_TRIANGLE_COVERAGE_BYTES);
        commands[2] = UINT64_C(0x2f090c0000000000);
        commands[5] = UINT64_C(0x0a8f002000100000); /* tile7 + level1 =8 */
        T64_CHECK(run_commands(machine, commands,
                               sizeof commands / sizeof commands[0]));
        T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_RENDER_RANGE);
        T64_CHECK_INT(machine->rdp.dpc_current, COMMAND_ADDRESS + 40u);
        T64_CHECK(memcmp(pixels, machine->ram + FRAMEBUFFER_ADDRESS,
                         sizeof pixels) == 0);
        T64_CHECK(memcmp(coverage, machine->rdp_triangle_coverage,
                         T64_RDP_TRIANGLE_COVERAGE_BYTES) == 0);
        free(coverage);
      }
    }
    t64_machine_destroy(machine);
  }
  for (variant = 0u; variant < 2u; variant++) {
    /* rho=2: header level0 caps the sample to tile1; level1 permits tile2.
     * Tile0 and unrelated descriptors are deliberately unnecessary. */
    uint64_t commands[] = {
        UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
        UINT64_C(0x2f010c0000000000), UINT64_C(0x3510041001080200),
        UINT64_C(0x320000000101c01c), UINT64_C(0x3510042002080200),
        UINT64_C(0x320000000201c01c), UINT64_C(0x0a81002000100000),
        UINT64_C(0), UINT64_C(0x0004000000000000),
        UINT64_C(0x00000000ffff0000), UINT64_C(0x0006000000010000),
        UINT64_C(0x0002000000000000), UINT64_C(0), UINT64_C(0),
        UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0),
    };
    t64_machine *machine = create_authored_machine();
    uint32_t x, y;
    if (machine == NULL) return;
    if (variant != 0u) commands[7] = UINT64_C(0x0a89002000100000);
    for (y = 0u; y < 8u; y++) for (x = 0u; x < 8u; x++) {
      set_logical_texel16_at_word(machine, 16u, x, y, UINT16_C(0x07c1));
      set_logical_texel16_at_word(machine, 32u, x, y, UINT16_C(0x003f));
    }
    T64_CHECK(run_commands(machine, commands,
                           sizeof commands / sizeof commands[0]));
    T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
    T64_CHECK_INT((machine->ram[framebuffer_pixel(1u, 4u)] << 8) |
                      machine->ram[framebuffer_pixel(1u, 4u) + 1u],
                  variant == 0u ? UINT16_C(0x07c1) : UINT16_C(0x003f));
    t64_machine_destroy(machine);
  }
}

static void case_texture_filter_alpha_floor(void) {
  unsigned filter, mask;
  for (filter = 0u; filter < 4u; filter++) for (mask = 0u; mask < 16u; mask++) {
    uint64_t commands[sizeof affine_filter_scene / sizeof affine_filter_scene[0]];
    t64_machine *machine = create_authored_machine();
    uint16_t expected_alpha;
    if (machine == NULL) return;
    memcpy(commands, affine_filter_scene, sizeof commands);
    commands[2] = filter == 0u ? UINT64_C(0x2f003c0000000000)
                              : UINT64_C(0x2f002c0000000000);
    commands[9] = UINT64_C(0x0003000000010000);
    commands[11] = UINT64_C(0x8000800000000000);
    if (filter == 2u) commands[11] = UINT64_C(0x4000c00000000000);
    if (filter == 3u) commands[11] = UINT64_C(0xc000400000000000);
    commands[12] = 0u;
    set_logical_texel16(machine, 3u, 0u, (uint16_t)(0xfffeu | (mask & 1u)));
    set_logical_texel16(machine, 4u, 0u, (uint16_t)(0xfffeu | ((mask >> 1) & 1u)));
    set_logical_texel16(machine, 3u, 1u, (uint16_t)(0xfffeu | ((mask >> 2) & 1u)));
    set_logical_texel16(machine, 4u, 1u, (uint16_t)(0xfffeu | ((mask >> 3) & 1u)));
    /* At (.5,.5), box=floor(sum/4); diagonal filter=floor((TL+BR)/2).
     * Include every mixed-alpha pattern and the exact-half ties. */
    /* Unequal fractions cover both sides too: (.25,.75) uses TL/BL/BR,
     * (.75,.25) uses TL/TR/BR, with middle sample weight exactly one half. */
    expected_alpha = (uint16_t)(filter == 0u ? mask == 15u :
        filter == 1u ? (mask & 9u) == 9u :
        filter == 2u ? (mask & 13u) == 13u : (mask & 11u) == 11u);
    T64_CHECK(run_commands(machine, commands,
                           sizeof commands / sizeof commands[0]));
    T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
    T64_CHECK_INT((machine->ram[framebuffer_pixel(1u, 4u)] << 8) |
                      machine->ram[framebuffer_pixel(1u, 4u) + 1u],
                  UINT16_C(0xfffe) | expected_alpha);
    t64_machine_destroy(machine);
  }
}

static void case_texture_all_coefficient_rows(void) {
  /* Independent Q16 vectors: S/T/W bases=(5.25,6.5,2.25),
   * Dx=(1.5,-.75,.125), De=(-.5,1.25,-.0625), Dy=(.75,-.5,.0625).
   * Major slope=.5. At (2.5,2.5),(2.5,3.5),(1.5,2.5),(3.5,2.5),
   * perspective S/T are (456/153,392/153),(440/151,424/151),
   * (360/145,440/145),(552/161,344/161). Hence texels (2,2),(2,2),
   * (2,3),(3,2); dropping any derivative lane changes a selected texel. */
  static const uint64_t texture_group[] = {
      UINT64_C(0x0005000600020000), UINT64_C(0x0001ffff00000000),
      UINT64_C(0x4000800040000000), UINT64_C(0x8000400020000000),
      UINT64_C(0xffff0001ffff0000), UINT64_C(0x0000ffff00000000),
      UINT64_C(0x80004000f0000000), UINT64_C(0xc000800010000000),
  };
  static const uint32_t xs[] = {2u, 2u, 1u, 3u};
  static const uint32_t ys[] = {2u, 3u, 2u, 2u};
  static const uint16_t expected[] = {0x18cfu, 0x18cfu, 0x1913u, 0x20d1u};
  unsigned shaded;
  for (shaded = 0u; shaded < 2u; shaded++) {
    uint64_t commands[5u + 20u] = {
        UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
        UINT64_C(0x2f080c0000000000), UINT64_C(0x3510040000080200),
        UINT64_C(0x320000000001c01c), UINT64_C(0x0a000014000c0004),
        UINT64_C(0x00010000fffe8000), UINT64_C(0x0001000000008000),
        UINT64_C(0x0004000000018000),
    };
    t64_machine *machine = create_authored_machine();
    size_t offset = shaded != 0u ? 17u : 9u;
    uint32_t x, y;
    size_t i;
    if (machine == NULL) return;
    if (shaded != 0u) {
      commands[5] = UINT64_C(0x0e000014000c0004);
      commands[9] = UINT64_C(0x00ff00ff00ff00ff);
    }
    memcpy(commands + offset, texture_group, sizeof texture_group);
    for (y = 0u; y < 8u; y++) for (x = 0u; x < 8u; x++)
      set_logical_texel16(machine, x, y,
          (uint16_t)(((x + 1u) << 11) | ((y + 1u) << 6) |
                     ((x + 2u * y + 1u) << 1) | 1u));
    T64_CHECK(run_commands(machine, commands, offset + 8u));
    T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
    for (i = 0u; i < sizeof xs / sizeof xs[0]; i++) {
      uint32_t address = framebuffer_pixel(xs[i], ys[i]);
      T64_CHECK_INT((machine->ram[address] << 8) | machine->ram[address + 1u],
                    expected[i]);
    }
    t64_machine_destroy(machine);
  }
}

typedef struct {
  const char *name;
  void (*run)(void);
} test_case;

static const test_case cases[] = {
    {"texture-header-tile-level", case_texture_header_tile_and_level},
    {"texture-filter-alpha-floor", case_texture_filter_alpha_floor},
    {"texture-all-coefficient-rows", case_texture_all_coefficient_rows},
    {"texture-perspective-nonzero-signed-q16",
     case_texture_perspective_nonzero_signed_q16},
    {"texture-fetched-load-tile-to-vi",
     case_texture_fetched_load_tile_to_vi},
    {"texture-affine-point", case_texture_affine_point},
    {"texture-filter-box", case_texture_filter_box},
    {"texture-filter-threepoint", case_texture_filter_threepoint},
    {"texture-shade-gradient", case_texture_shade_gradient},
    {"texture-lod-detail-sharpen", case_texture_lod_detail_sharpen},
    {"texture-old-profile-refusal", case_texture_old_profile_refusal},
    {"texture-profile-v4-identity", case_texture_profile_v4_identity},
    {"texture-mask-mirror-boundary", case_texture_mask_mirror_boundary},
    {"texture-odd-tmem-row", case_texture_odd_tmem_row},
    {"texture-negative-gradient", case_texture_negative_gradient},
    {"texture-w-zero-sign-policy", case_texture_w_zero_sign_policy},
    {"texture-fractional-tie", case_texture_fractional_tie},
    {"texture-format-matrix", case_texture_format_matrix},
    {"texture-truncated-group-preserves-pixels",
     case_texture_truncated_group_preserves_pixels},
    {"texture-split-digest-interleave", case_texture_split_digest_interleave},
};

int main(int argc, char **argv) {
  size_t i;
  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) puts(cases[i].name);
    return 0;
  }
  if (argc != 1) return 2;
  for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) cases[i].run();
  return T64_TEST_FINISH("unit.core.rdp.texture");
}
