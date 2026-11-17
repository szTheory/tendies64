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

static bool capture_raw_vi(t64_machine *machine, uint8_t *bytes,
                           size_t capacity, size_t *length_out,
                           uint32_t control) {
  const struct {
    uint32_t offset;
    uint32_t value;
  } writes[] = {
      {UINT32_C(0x04), FRAMEBUFFER_ADDRESS}, {UINT32_C(0x08), 8u},
      {UINT32_C(0x1c), 3u}, {UINT32_C(0x18), 0u},
      {UINT32_C(0x24), 8u}, {UINT32_C(0x28), 8u},
      {UINT32_C(0x30), 1024u}, {UINT32_C(0x34), 1024u},
      {UINT32_C(0x00), control},
  };
  t64_video_packet_info info = {0};
  size_t i, step;
  for (i = 0u; i < sizeof writes / sizeof writes[0]; i++)
    T64_CHECK_INT(t64_machine_bus_write(machine, VI_BASE + writes[i].offset,
                                        4u, writes[i].value), T64_STATUS_OK);
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
  T64_CHECK_INT(info.source_format,
                control == 3u ? T64_VIDEO_SOURCE_RGBA8888
                              : T64_VIDEO_SOURCE_RGBA5551);
  T64_CHECK(info.packet_bytes <= capacity);
  if (info.packet_bytes > capacity) return false;
  T64_CHECK_INT(t64_video_copy(machine, bytes, capacity, length_out),
                T64_STATUS_OK);
  return true;
}

static void case_primitive_depth_raw_packet_accepted_effect(void) {
  static const uint64_t commands[] = {
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
  static const uint8_t expected_color[] = {0x07u, 0xc1u};
  static const uint8_t expected_depth[] = {0x09u, 0xa8u};
  static const uint8_t expected_vi[] = {0u, 255u, 0u, 255u};
  t64_machine *machine = create_depth_machine();
  uint32_t pixel_address = FRAMEBUFFER_ADDRESS + (2u * 8u + 1u) * 2u;
  uint32_t depth_address = DEPTH_IMAGE_ADDRESS + (2u * 8u + 1u) * 2u;
  uint8_t raw_vi[8u * 8u * 4u];
  size_t raw_vi_length = 0u;

  if (machine == NULL) return;
  T64_CHECK_INT((commands[2] & UINT64_C(0x3f)), UINT64_C(0x34));
  T64_CHECK_INT((commands[4] >> 16) & UINT64_C(0xffff), UINT64_C(0x1357));
  T64_CHECK_INT(commands[4] & UINT64_C(0xffff), UINT64_C(9));
  for (uint32_t i = 0u; i < 8u * 8u; i++)
    write_be16(machine->ram + DEPTH_IMAGE_ADDRESS + i * 2u,
               UINT16_C(0xfffc));
  memset(machine->ram + FRAMEBUFFER_ADDRESS, 0xa5, 8u * 8u * 2u);
  T64_CHECK(run_commands(machine, commands,
                         sizeof commands / sizeof commands[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT(machine->rdp.last_opcode_field, UINT32_C(0x09));
  T64_CHECK(memcmp(machine->ram + pixel_address, expected_color,
                   sizeof expected_color) == 0);
  /* Independent software oracle: 0x1357*8=0x9ab8, exponent=0,
   * floor(0x9ab8/64)=0x26a, visible=0x09a8, delta code=3. */
  T64_CHECK(memcmp(machine->ram + depth_address, expected_depth,
                   sizeof expected_depth) == 0);
  T64_CHECK_INT(machine->rdp_depth_hidden[depth_address / 2u], UINT8_C(0x83));
  T64_CHECK(capture_raw_vi(machine, raw_vi, sizeof raw_vi, &raw_vi_length,
                           2u));
  T64_CHECK_INT(raw_vi_length, sizeof raw_vi);
  T64_CHECK(memcmp(raw_vi + (2u * 8u + 1u) * 4u, expected_vi,
                   sizeof expected_vi) == 0);
  t64_machine_destroy(machine);
}

static void case_rgba8888_depth_and_vi_output(void) {
  static const uint64_t commands[] = {
      UINT64_C(0x3f18000700005000), UINT64_C(0x2d00000000020020),
      UINT64_C(0x2f00000000000034), UINT64_C(0x3e00000000006000),
      UINT64_C(0x2e00000013570009), UINT64_C(0x3a00000000ff00ff),
      UINT64_C(0xfc00000000018600), UINT64_C(0x09000014000c0004),
      UINT64_C(0x00010000fffe8000), UINT64_C(0x0001000000000000),
      UINT64_C(0x0004000000018000), UINT64_C(0x0000000000000000),
      UINT64_C(0x0000000000000000), UINT64_C(0x2900000000000000),
  };
  static const uint8_t expected_pixel[] = {0u, 255u, 0u, 255u};
  static const uint8_t expected_depth[] = {0x09u, 0xa8u};
  t64_machine *machine = create_depth_machine();
  uint8_t raw_vi[8u * 8u * 4u];
  size_t raw_vi_length = 0u;
  uint32_t i;
  const uint32_t color = FRAMEBUFFER_ADDRESS + (2u * 8u + 1u) * 4u;
  const uint32_t depth = DEPTH_IMAGE_ADDRESS + (2u * 8u + 1u) * 2u;
  if (machine == NULL) return;
  memset(machine->ram + FRAMEBUFFER_ADDRESS, 0xa5, 8u * 8u * 4u);
  for (i = 0u; i < 8u * 8u; i++)
    write_be16(machine->ram + DEPTH_IMAGE_ADDRESS + i * 2u, 0xfffcu);
  T64_CHECK(run_commands(machine, commands,
                         sizeof commands / sizeof commands[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK(memcmp(machine->ram + color, expected_pixel, 3u) == 0);
  T64_CHECK_INT(machine->ram[color + 3u] & UINT8_C(0x1f), UINT8_C(0x1f));
  T64_CHECK(memcmp(machine->ram + depth, expected_depth,
                   sizeof expected_depth) == 0);
  T64_CHECK(capture_raw_vi(machine, raw_vi, sizeof raw_vi, &raw_vi_length,
                           3u));
  T64_CHECK_INT(raw_vi_length, sizeof raw_vi);
  T64_CHECK(memcmp(raw_vi + (2u * 8u + 1u) * 4u, expected_pixel,
                   sizeof expected_pixel) == 0);
  t64_machine_destroy(machine);
}

typedef struct {
  uint16_t color;
  uint16_t depth;
  uint8_t hidden;
  uint8_t coverage;
  uint32_t refusal;
  uint64_t fullsync_count;
} primitive_result;

static bool run_primitive_vector(uint64_t modes, uint64_t primitive_depth,
                                 uint16_t old_depth,
                                 uint8_t old_coverage,
                                 primitive_result *result) {
  uint64_t commands[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020), modes,
      UINT64_C(0x3e00000000006000), primitive_depth,
      UINT64_C(0x3a00000000ff00ff), UINT64_C(0xfc00000000018600),
      UINT64_C(0x09000014000c0004), UINT64_C(0x00010000fffe8000),
      UINT64_C(0x0001000000000000), UINT64_C(0x0004000000018000),
      UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000),
      UINT64_C(0x2900000000000000),
  };
  const uint32_t color_address = FRAMEBUFFER_ADDRESS + (2u * 8u + 1u) * 2u;
  const uint32_t depth_address = DEPTH_IMAGE_ADDRESS + (2u * 8u + 1u) * 2u;
  t64_machine *machine = create_depth_machine();
  uint32_t i;

  if (machine == NULL || result == NULL) return false;
  memset(machine->ram + FRAMEBUFFER_ADDRESS, 0xa5, 8u * 8u * 2u);
  for (i = 0u; i < 8u * 8u; i++)
    write_be16(machine->ram + DEPTH_IMAGE_ADDRESS + i * 2u, old_depth);
  if (old_coverage != 0u)
    machine->rdp_memory_coverage[color_address / 2u] =
        (uint8_t)(UINT8_C(0x80) | old_coverage);
  T64_CHECK(run_commands(machine, commands,
                         sizeof commands / sizeof commands[0]));
  result->color = (uint16_t)(((uint16_t)machine->ram[color_address] << 8) |
                             machine->ram[color_address + 1u]);
  result->depth = (uint16_t)(((uint16_t)machine->ram[depth_address] << 8) |
                             machine->ram[depth_address + 1u]);
  result->hidden = machine->rdp_depth_hidden[depth_address / 2u];
  result->coverage = machine->rdp_memory_coverage[color_address / 2u];
  result->refusal = machine->rdp.refusal_reason;
  result->fullsync_count = machine->rdp.fullsync_count;
  t64_machine_destroy(machine);
  return true;
}

static void case_all_z_triangle_opcodes_reach_pixel(void) {
  static const uint64_t head[] = {
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
  static const uint64_t payload_zeros[16] = {0u};
  static const uint64_t triangle_heads[] = {
      UINT64_C(0x08000014000c0004), UINT64_C(0x09000014000c0004),
      UINT64_C(0x0a000014000c0004), UINT64_C(0x0b000014000c0004),
      UINT64_C(0x0c000014000c0004), UINT64_C(0x0d000014000c0004),
      UINT64_C(0x0e000014000c0004), UINT64_C(0x0f000014000c0004),
  };
  static const uint8_t opcodes[] = {
      0x08u, 0x09u, 0x0au, 0x0bu, 0x0cu, 0x0du, 0x0eu, 0x0fu,
  };
  static const uint16_t expected_color = UINT16_C(0x07c1);
  static const uint16_t expected_depth = UINT16_C(0x09a8);
  size_t index;

  for (index = 0u; index < sizeof opcodes / sizeof opcodes[0]; index++) {
    uint64_t commands[40];
    size_t count = 0u;
    size_t triangle_at;
    bool textured = opcodes[index] == 0x0au || opcodes[index] == 0x0bu ||
                    opcodes[index] == 0x0eu || opcodes[index] == 0x0fu;
    bool shaded = opcodes[index] == 0x0cu || opcodes[index] == 0x0du ||
                  opcodes[index] == 0x0eu || opcodes[index] == 0x0fu;
    bool depth_enabled = opcodes[index] == 0x09u || opcodes[index] == 0x0bu ||
                         opcodes[index] == 0x0du || opcodes[index] == 0x0fu;
    uint32_t i;
    t64_machine *machine = create_depth_machine();
    uint32_t color_address = FRAMEBUFFER_ADDRESS + (2u * 8u + 1u) * 2u;
    uint32_t depth_address = DEPTH_IMAGE_ADDRESS + (2u * 8u + 1u) * 2u;
    if (machine == NULL) return;
    memcpy(commands + count, head, sizeof head);
    count += sizeof head / sizeof head[0];
    if (textured) {
      memcpy(commands + count, texture_setup, sizeof texture_setup);
      count += sizeof texture_setup / sizeof texture_setup[0];
      memset(machine->ram + UINT32_C(0x4000), 0, 8u * 8u * 2u);
    }
    triangle_at = count;
    commands[count++] = triangle_heads[index];
    memcpy(commands + count, edge_words, sizeof edge_words);
    count += sizeof edge_words / sizeof edge_words[0];
    if (opcodes[index] == 0x0eu && shaded) {
      memcpy(commands + count, payload_zeros, 8u * sizeof(uint64_t));
      count += 8u;
    }
    if (textured) {
      commands[count++] = UINT64_C(0x0000000000010000);
      memcpy(commands + count, payload_zeros, 7u * sizeof(uint64_t));
      count += 7u;
    }
    if (shaded && opcodes[index] != 0x0eu) {
      memcpy(commands + count, payload_zeros, 8u * sizeof(uint64_t));
      count += 8u;
    }
    if (depth_enabled) {
      commands[count++] = UINT64_C(0x0000000000000000);
      commands[count++] = UINT64_C(0x0000000000000000);
    }
    if (opcodes[index] == 0x0fu) {
      /* Literal W20 carries Z base=8.0 Q16.16; W21 is zero dZ. */
      commands[2] = UINT64_C(0x2f00000000000030);
      commands[count - 2u] = UINT64_C(0x0008000000000000);
      T64_CHECK_INT(commands[count - 2u], UINT64_C(0x0008000000000000));
      T64_CHECK_INT(commands[count - 1u], UINT64_C(0x0000000000000000));
    }
    commands[count++] = UINT64_C(0x2900000000000000);
    T64_CHECK_INT((commands[triangle_at] >> 56) & UINT64_C(0x3f),
                  opcodes[index]);
    T64_CHECK_INT(count - triangle_at - 1u,
                  t64_rdp_triangle_word_count(opcodes[index]));
    memset(machine->ram + FRAMEBUFFER_ADDRESS, 0xa5, 8u * 8u * 2u);
    for (i = 0u; i < 8u * 8u; i++)
      write_be16(machine->ram + DEPTH_IMAGE_ADDRESS + i * 2u,
                 UINT16_C(0xfffc));
    T64_CHECK(run_commands(machine, commands, count));
    T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
    T64_CHECK_INT(machine->rdp.last_opcode_field, UINT32_C(0x29));
    T64_CHECK(machine->rdp.fullsync_count > 0u);
    T64_CHECK_INT((machine->ram[color_address] << 8) |
                      machine->ram[color_address + 1u],
                  expected_color);
    T64_CHECK_INT((machine->ram[depth_address] << 8) |
                      machine->ram[depth_address + 1u],
                  !depth_enabled
                      ? UINT16_C(0xfffc)
                      : (opcodes[index] == 0x0fu ? 0x0004u : expected_depth));
    T64_CHECK_INT(machine->rdp_depth_hidden[depth_address / 2u],
                  !depth_enabled
                      ? 0u
                      : (opcodes[index] == 0x0fu ? 0x80u : 0x83u));
    t64_machine_destroy(machine);
  }
}

static void case_split_interleaved_depth_outputs_equal(void) {
  static const uint64_t commands[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
      UINT64_C(0x2f00000000000034), UINT64_C(0x3e00000000006000),
      UINT64_C(0x2e00000013570009), UINT64_C(0x3a00000000ff00ff),
      UINT64_C(0xfc00000000018600), UINT64_C(0x09000014000c0004),
      UINT64_C(0x00010000fffe8000), UINT64_C(0x0001000000000000),
      UINT64_C(0x0004000000018000), UINT64_C(0x0000000000000000),
      UINT64_C(0x0000000000000000), UINT64_C(0x2900000000000000),
  };
  t64_machine *whole = create_depth_machine();
  t64_machine *split = create_depth_machine();
  t64_machine *interleaved_a = create_depth_machine();
  t64_machine *interleaved_b = create_depth_machine();
  t64_machine *machines[] = {whole, split, interleaved_a, interleaved_b};
  uint8_t raw_vi[4][8u * 8u * 4u];
  size_t raw_vi_length[4] = {0u, 0u, 0u, 0u};
  uint32_t i;
  size_t m;

  for (m = 0u; m < sizeof machines / sizeof machines[0]; m++) {
    if (machines[m] == NULL) goto cleanup;
    memset(machines[m]->ram + FRAMEBUFFER_ADDRESS, 0xa5, 128u);
    for (i = 0u; i < 64u; i++)
      write_be16(machines[m]->ram + DEPTH_IMAGE_ADDRESS + i * 2u,
                 UINT16_C(0xfffc));
  }
  T64_CHECK(run_commands(whole, commands,
                         sizeof commands / sizeof commands[0]));
  T64_CHECK(run_commands(split, commands, 7u));
  T64_CHECK(run_commands(split, commands + 7u,
                         sizeof commands / sizeof commands[0] - 7u));
  T64_CHECK(run_commands(interleaved_a, commands, 7u));
  T64_CHECK(run_commands(interleaved_b, commands, 7u));
  T64_CHECK(run_commands(interleaved_a, commands + 7u,
                         sizeof commands / sizeof commands[0] - 7u));
  T64_CHECK(run_commands(interleaved_b, commands + 7u,
                         sizeof commands / sizeof commands[0] - 7u));

  for (m = 0u; m < sizeof machines / sizeof machines[0]; m++) {
    T64_CHECK_INT(machines[m]->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
    T64_CHECK_INT(machines[m]->rdp.fullsync_count, 1u);
    T64_CHECK_INT(machines[m]->ram[FRAMEBUFFER_ADDRESS + 34u], 0x07u);
    T64_CHECK_INT(machines[m]->ram[FRAMEBUFFER_ADDRESS + 35u], 0xc1u);
    T64_CHECK_INT(machines[m]->ram[DEPTH_IMAGE_ADDRESS + 34u], 0x09u);
    T64_CHECK_INT(machines[m]->ram[DEPTH_IMAGE_ADDRESS + 35u], 0xa8u);
    T64_CHECK_INT(machines[m]->rdp_depth_hidden[
                      (DEPTH_IMAGE_ADDRESS + 34u) / 2u],
                  0x83u);
    T64_CHECK(capture_raw_vi(machines[m], raw_vi[m], sizeof raw_vi[m],
                             &raw_vi_length[m], 2u));
    T64_CHECK_INT(raw_vi_length[m], sizeof raw_vi[m]);
  }
  for (m = 1u; m < sizeof machines / sizeof machines[0]; m++) {
    T64_CHECK(memcmp(whole->ram + FRAMEBUFFER_ADDRESS,
                     machines[m]->ram + FRAMEBUFFER_ADDRESS, 128u) == 0);
    T64_CHECK(memcmp(whole->ram + DEPTH_IMAGE_ADDRESS,
                     machines[m]->ram + DEPTH_IMAGE_ADDRESS, 128u) == 0);
    T64_CHECK(memcmp(whole->rdp_depth_hidden + DEPTH_IMAGE_ADDRESS / 2u,
                     machines[m]->rdp_depth_hidden +
                         DEPTH_IMAGE_ADDRESS / 2u,
                     64u) == 0);
    T64_CHECK(memcmp(whole->rdp_memory_coverage + FRAMEBUFFER_ADDRESS / 2u,
                     machines[m]->rdp_memory_coverage +
                         FRAMEBUFFER_ADDRESS / 2u,
                     64u) == 0);
    T64_CHECK(memcmp(raw_vi[0], raw_vi[m], sizeof raw_vi[0]) == 0);
  }

cleanup:
  for (m = 0u; m < sizeof machines / sizeof machines[0]; m++)
    if (machines[m] != NULL) t64_machine_destroy(machines[m]);
}

static void case_q16_signed_fractional_depth_lanes(void) {
  static const uint64_t commands[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
      UINT64_C(0x2f00000000000030), UINT64_C(0x3e00000000006000),
      UINT64_C(0x3a00000000ff00ff), UINT64_C(0xfc00000000018600),
      UINT64_C(0x09000014000c0004), UINT64_C(0x00010000fffe8000),
      UINT64_C(0x0001000000008000), UINT64_C(0x0004000000018000),
      UINT64_C(0x0008000000008000), UINT64_C(0xffffc00000008000),
      UINT64_C(0x2900000000000000),
  };
  const uint32_t pixel = FRAMEBUFFER_ADDRESS + (2u * 8u + 1u) * 2u;
  const uint32_t depth = DEPTH_IMAGE_ADDRESS + (2u * 8u + 1u) * 2u;
  t64_machine *machine = create_depth_machine();
  uint32_t i;
  if (machine == NULL) return;
  memset(machine->ram + FRAMEBUFFER_ADDRESS, 0xa5, 8u * 8u * 2u);
  for (i = 0u; i < 8u * 8u; i++)
    write_be16(machine->ram + DEPTH_IMAGE_ADDRESS + i * 2u, 0xfffcu);
  T64_CHECK(run_commands(machine, commands,
                         sizeof commands / sizeof commands[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT((machine->ram[pixel] << 8) | machine->ram[pixel + 1u],
                UINT16_C(0x07c1));
  /* Q16.16: 8 + .5*(-.25) - .25*(.75) + .5*(1.5) = 8.4375;
   * floor(8.4375*8)=67, compressed mantissa 1. dZ=ceil((.5+.5)*8)=8. */
  T64_CHECK_INT((machine->ram[depth] << 8) | machine->ram[depth + 1u], 0x0004u);
  T64_CHECK_INT(machine->rdp_depth_hidden[depth / 2u], 0x83u);
  t64_machine_destroy(machine);
}

static void case_primitive_delta_priority_boundaries(void) {
  static const uint64_t modes = UINT64_C(0x2f00000000000034);
  static const uint64_t primitive_words[] = {
      UINT64_C(0x2e00000013570000), UINT64_C(0x2e00000013570001),
      UINT64_C(0x2e00000013570002), UINT64_C(0x2e00000013570003),
      UINT64_C(0x2e00000013570004), UINT64_C(0x2e00000013570007),
      UINT64_C(0x2e00000013570008), UINT64_C(0x2e00000013570009),
      UINT64_C(0x2e0000001357000f), UINT64_C(0x2e00000013570010),
      UINT64_C(0x2e0000001357ffff),
  };
  static const uint16_t visible[] = {
      0x09a8u, 0x09a8u, 0x09a8u, 0x09a8u, 0x09a8u, 0x09a8u,
      0x09a8u, 0x09a8u, 0x09a8u, 0x09a9u, 0x09abu,
  };
  static const uint8_t hidden[] = {
      0x80u, 0x80u, 0x81u, 0x81u, 0x82u, 0x82u,
      0x83u, 0x83u, 0x83u, 0x80u, 0x83u,
  };
  size_t i;
  for (i = 0u; i < sizeof primitive_words / sizeof primitive_words[0]; i++) {
    primitive_result result;
    T64_CHECK(run_primitive_vector(modes, primitive_words[i], 0xfffcu, 0u,
                                   &result));
    T64_CHECK_INT(result.refusal, T64_RDP_REFUSAL_NONE);
    T64_CHECK_INT(result.color, UINT16_C(0x07c1));
    T64_CHECK_INT(result.depth, visible[i]);
    T64_CHECK_INT(result.hidden, hidden[i]);
    T64_CHECK_INT(result.fullsync_count > 0u, 1u);
  }
}

static void case_primitive_compression_exponent_transitions(void) {
  static const uint64_t modes = UINT64_C(0x2f00000000000034);
  static const uint64_t primitive_words[] = {
      UINT64_C(0x2e00000000000009), UINT64_C(0x2e0000003fff0009),
      UINT64_C(0x2e00000040000009), UINT64_C(0x2e0000005fff0009),
      UINT64_C(0x2e00000060000009), UINT64_C(0x2e0000006fff0009),
      UINT64_C(0x2e00000070000009), UINT64_C(0x2e00000077ff0009),
      UINT64_C(0x2e00000078000009), UINT64_C(0x2e0000007bff0009),
      UINT64_C(0x2e0000007c000009), UINT64_C(0x2e0000007dff0009),
      UINT64_C(0x2e0000007e000009), UINT64_C(0x2e0000007eff0009),
      UINT64_C(0x2e0000007f000009), UINT64_C(0x2e0000007fff0009),
  };
  static const uint16_t visible[] = {
      0x0000u, 0x1ffcu, 0x2000u, 0x3ffcu,
      0x4000u, 0x5ffcu, 0x6000u, 0x7ffcu,
      0x8000u, 0x9ff8u, 0xa000u, 0xbff0u,
      0xc000u, 0xdfe0u, 0xe000u, 0xffe0u,
  };
  size_t i;
  for (i = 0u; i < sizeof primitive_words / sizeof primitive_words[0]; i++) {
    primitive_result result;
    T64_CHECK(run_primitive_vector(modes, primitive_words[i], 0xfffcu, 0u,
                                   &result));
    T64_CHECK_INT(result.refusal, T64_RDP_REFUSAL_NONE);
    T64_CHECK_INT(result.depth, visible[i]);
    T64_CHECK_INT(result.hidden, 0x83u);
  }
}

static void case_depth_compare_update_and_coverage_modes(void) {
  primitive_result result;
  /* Strictly less, equal, and greater comparisons use distinct primary
   * compressed words whose low delta bits are all zero. */
  T64_CHECK(run_primitive_vector(UINT64_C(0x2f00000000000034),
                                 UINT64_C(0x2e00000013500009), 0x09acu,
                                 0u, &result));
  T64_CHECK_INT(result.color, 0x07c1u);
  T64_CHECK_INT(result.depth, 0x09a8u);
  T64_CHECK_INT(result.hidden, 0x83u);

  T64_CHECK(run_primitive_vector(UINT64_C(0x2f00000000000034),
                                 UINT64_C(0x2e00000013500009), 0x09a8u,
                                 0u, &result));
  T64_CHECK_INT(result.color, 0x07c1u);
  T64_CHECK_INT(result.depth, 0x09a8u);
  T64_CHECK_INT(result.hidden, 0x83u);

  T64_CHECK(run_primitive_vector(UINT64_C(0x2f00000000000034),
                                 UINT64_C(0x2e00000013500009), 0x09a4u,
                                 0u, &result));
  T64_CHECK_INT(result.color, 0xa5a5u);
  T64_CHECK_INT(result.depth, 0x09a4u);
  T64_CHECK_INT(result.hidden, 0u);

  T64_CHECK(run_primitive_vector(UINT64_C(0x2f00000000000034),
                                 UINT64_C(0x2e00000013570009), 0x09a8u,
                                 0u, &result));
  T64_CHECK_INT(result.color, 0xa5a5u);
  T64_CHECK_INT(result.depth, 0x09a8u);
  T64_CHECK_INT(result.hidden, 0u);

  /* Decal equality tolerance is the larger decoded old/new delta: 1 raw Z
   * unit is eight linear units and passes; two raw units exceed it. */
  T64_CHECK(run_primitive_vector(UINT64_C(0x2f00000000000c34),
                                 UINT64_C(0x2e00000013510009), 0x09a8u,
                                 0u, &result));
  T64_CHECK_INT(result.color, 0x07c1u);
  T64_CHECK_INT(result.depth, 0x09a8u);
  T64_CHECK_INT(result.hidden, 0u);
  T64_CHECK(run_primitive_vector(UINT64_C(0x2f00000000000c34),
                                 UINT64_C(0x2e00000013520009), 0x09a8u,
                                 0u, &result));
  T64_CHECK_INT(result.color, 0xa5a5u);
  T64_CHECK_INT(result.depth, 0x09a8u);
  T64_CHECK_INT(result.hidden, 0u);

  T64_CHECK(run_primitive_vector(UINT64_C(0x2f00000000000434),
                                 UINT64_C(0x2e00000013570009), 0x09a8u,
                                 1u, &result));
  T64_CHECK_INT(result.color, 0x07c1u);
  T64_CHECK_INT(result.depth, 0x09a8u);
  T64_CHECK_INT(result.hidden, 0x83u);

  T64_CHECK(run_primitive_vector(UINT64_C(0x2f00000000000434),
                                 UINT64_C(0x2e00000013570009), 0x09a8u,
                                 8u, &result));
  T64_CHECK_INT(result.color, 0xa5a5u);
  T64_CHECK_INT(result.hidden, 0u);

  T64_CHECK(run_primitive_vector(UINT64_C(0x2f00000000000834),
                                 UINT64_C(0x2e00000013570009), 0x09a8u,
                                 0u, &result));
  T64_CHECK_INT(result.color, 0xa5a5u);
  T64_CHECK_INT(result.hidden, 0u);
  T64_CHECK(run_primitive_vector(UINT64_C(0x2f00000000000834),
                                 UINT64_C(0x2e00000013570009), 0xfffcu,
                                 0u, &result));
  T64_CHECK_INT(result.color, 0x07c1u);
  T64_CHECK_INT(result.depth, 0xfffcu);
  T64_CHECK_INT(result.hidden, 0u);

  T64_CHECK(run_primitive_vector(UINT64_C(0x2f00000000000c34),
                                 UINT64_C(0x2e00000013500009), 0x09a8u,
                                 0u, &result));
  T64_CHECK_INT(result.color, 0x07c1u);
  T64_CHECK_INT(result.depth, 0x09a8u);
  T64_CHECK_INT(result.hidden, 0u);

  T64_CHECK(run_primitive_vector(UINT64_C(0x2f00000000000024),
                                 UINT64_C(0x2e00000013570009), 0x09a8u,
                                 0u, &result));
  T64_CHECK_INT(result.depth, 0x09a8u);
  T64_CHECK_INT(result.hidden, 0x83u);
  T64_CHECK(run_primitive_vector(UINT64_C(0x2f00000000000014),
                                 UINT64_C(0x2e00000013570009), 0xfffcu,
                                 0u, &result));
  T64_CHECK_INT(result.color, 0x07c1u);
  T64_CHECK_INT(result.depth, 0xfffcu);
  T64_CHECK_INT(result.hidden, 0u);
}

static void case_alpha_reject_preserves_depth_and_color(void) {
  uint64_t commands[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
      UINT64_C(0x2f00000000000035), UINT64_C(0x3e00000000006000),
      UINT64_C(0x2e00000013570009), UINT64_C(0x3a00000000ff0000),
      UINT64_C(0xf9000000000000ff), UINT64_C(0xfc00000000018600),
      UINT64_C(0x09000014000c0004), UINT64_C(0x00010000fffe8000),
      UINT64_C(0x0001000000000000), UINT64_C(0x0004000000018000),
      UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000),
      UINT64_C(0x2900000000000000),
  };
  const uint32_t color = FRAMEBUFFER_ADDRESS + 34u;
  const uint32_t depth = DEPTH_IMAGE_ADDRESS + 34u;
  t64_machine *machine = create_depth_machine();
  uint32_t i;
  if (machine == NULL) return;
  memset(machine->ram + FRAMEBUFFER_ADDRESS, 0xa5, 128u);
  for (i = 0u; i < 64u; i++)
    write_be16(machine->ram + DEPTH_IMAGE_ADDRESS + i * 2u, 0xfffcu);
  T64_CHECK(run_commands(machine, commands,
                         sizeof commands / sizeof commands[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT((machine->ram[color] << 8) | machine->ram[color + 1u],
                0xa5a5u);
  T64_CHECK_INT((machine->ram[depth] << 8) | machine->ram[depth + 1u],
                0xfffcu);
  T64_CHECK_INT(machine->rdp_depth_hidden[depth / 2u], 0u);
  T64_CHECK((machine->rdp_memory_coverage[color / 2u] & 0x80u) != 0u);
  t64_machine_destroy(machine);
}

static void case_refusal_truncation_and_recovery(void) {
  static const uint64_t accepted_setup[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
      UINT64_C(0x2f00000000000034), UINT64_C(0x3e00000000006000),
      UINT64_C(0x2e00000013570009), UINT64_C(0x3a00000000ff00ff),
      UINT64_C(0xfc00000000018600),
  };
  static const uint64_t z_triangle[] = {
      UINT64_C(0x09000014000c0004), UINT64_C(0x00010000fffe8000),
      UINT64_C(0x0001000000000000), UINT64_C(0x0004000000018000),
      UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000),
  };
  static const uint64_t baseline[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
      UINT64_C(0x2f00000000000034), UINT64_C(0x3e00000000006000),
      UINT64_C(0x2e00000013570009), UINT64_C(0x3a00000000ff00ff),
      UINT64_C(0xfc00000000018600), UINT64_C(0x09000014000c0004),
      UINT64_C(0x00010000fffe8000), UINT64_C(0x0001000000000000),
      UINT64_C(0x0004000000018000), UINT64_C(0x0000000000000000),
      UINT64_C(0x0000000000000000), UINT64_C(0x2900000000000000),
  };
  static const uint64_t truncated[] = {
      UINT64_C(0x09000014000c0004), UINT64_C(0x00010000fffe8000),
  };
  static const uint64_t unaligned[] = {UINT64_C(0x3e00000000006004)};
  static const uint64_t old_profile[] = {UINT64_C(0x3e00000000006000)};
  static const uint64_t out_of_bounds[] = {
      UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
      UINT64_C(0x2f00000000000034), UINT64_C(0x3e000000003ffff8),
      UINT64_C(0x2e00000013570009), UINT64_C(0x3a00000000ff00ff),
      UINT64_C(0xfc00000000018600), UINT64_C(0x09000014000c0004),
      UINT64_C(0x00010000fffe8000), UINT64_C(0x0001000000000000),
      UINT64_C(0x0004000000018000), UINT64_C(0x0000000000000000),
      UINT64_C(0x0000000000000000),
  };
  const uint32_t color = FRAMEBUFFER_ADDRESS + 34u;
  const uint32_t depth = DEPTH_IMAGE_ADDRESS + 34u;
  uint8_t previous_color[2];
  uint8_t previous_depth[2];
  t64_machine *machine = create_depth_machine();
  uint32_t i;
  size_t prefix;
  if (machine == NULL) return;
  memset(machine->ram + FRAMEBUFFER_ADDRESS, 0xa5, 128u);
  for (i = 0u; i < 64u; i++)
    write_be16(machine->ram + DEPTH_IMAGE_ADDRESS + i * 2u, 0xfffcu);
  T64_CHECK(run_commands(machine, baseline,
                         sizeof baseline / sizeof baseline[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  memcpy(previous_color, machine->ram + color, sizeof previous_color);
  memcpy(previous_depth, machine->ram + depth, sizeof previous_depth);
  T64_CHECK(run_commands(machine, truncated,
                         sizeof truncated / sizeof truncated[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_TRUNCATED_WORD);
  T64_CHECK(memcmp(machine->ram + color, previous_color,
                   sizeof previous_color) == 0);
  T64_CHECK(memcmp(machine->ram + depth, previous_depth,
                   sizeof previous_depth) == 0);
  T64_CHECK_INT(machine->rdp_depth_hidden[depth / 2u], 0x83u);
  t64_machine_destroy(machine);

  /* Each incomplete prefix of the complete geometry + Z/DZ packet must be
   * rejected before the edge, visible-depth, or hidden-depth state changes. */
  for (prefix = 0u; prefix < 5u; prefix++) {
    uint8_t before_color[2] = {0xa5u, 0xa5u};
    uint8_t before_depth[2] = {0x09u, 0xa8u};
    uint8_t before_hidden = UINT8_C(0x5a);
    uint8_t before_coverage = UINT8_C(0x31);
    machine = create_depth_machine();
    if (machine == NULL) return;
    T64_CHECK(run_commands(machine, accepted_setup,
                           sizeof accepted_setup / sizeof accepted_setup[0]));
    memset(machine->ram + FRAMEBUFFER_ADDRESS, 0xa5, 128u);
    write_be16(machine->ram + depth, UINT16_C(0x09a8));
    machine->rdp_depth_hidden[depth / 2u] = before_hidden;
    machine->rdp_memory_coverage[color / 2u] = before_coverage;
    T64_CHECK(run_commands(machine, z_triangle, prefix + 1u));
    T64_CHECK_INT(machine->rdp.refusal_reason,
                  T64_RDP_REFUSAL_TRUNCATED_WORD);
    T64_CHECK(memcmp(machine->ram + color, before_color,
                     sizeof before_color) == 0);
    T64_CHECK(memcmp(machine->ram + depth, before_depth,
                     sizeof before_depth) == 0);
    T64_CHECK_INT(machine->rdp_depth_hidden[depth / 2u], before_hidden);
    T64_CHECK_INT(machine->rdp_memory_coverage[color / 2u], before_coverage);
    T64_CHECK_INT(machine->rdp.fullsync_count, 0u);
    t64_machine_destroy(machine);
  }

  machine = create_depth_machine();
  if (machine == NULL) return;
  T64_CHECK(run_commands(machine, unaligned,
                         sizeof unaligned / sizeof unaligned[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_INVALID_IMAGE);
  T64_CHECK(!machine->rdp.depth_image_valid);
  t64_machine_destroy(machine);

  machine = create_depth_machine();
  if (machine == NULL) return;
  memset(machine->ram + FRAMEBUFFER_ADDRESS, 0xa5, 128u);
  T64_CHECK(run_commands(machine, out_of_bounds,
                         sizeof out_of_bounds / sizeof out_of_bounds[0]));
  T64_CHECK_INT(machine->rdp.last_opcode_field, UINT32_C(0x09));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_RENDER_RANGE);
  T64_CHECK_INT(machine->ram[FRAMEBUFFER_ADDRESS + 34u], 0xa5u);
  T64_CHECK_INT(machine->ram[FRAMEBUFFER_ADDRESS + 35u], 0xa5u);
  T64_CHECK_INT(machine->rdp_depth_hidden[depth / 2u], 0u);
  t64_machine_destroy(machine);

  machine = create_machine_for_profile(
      "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-blender-v6");
  if (machine == NULL) return;
  T64_CHECK(run_commands(machine, old_profile,
                         sizeof old_profile / sizeof old_profile[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason,
                T64_RDP_REFUSAL_UNSUPPORTED_OPCODE);
  T64_CHECK(!machine->rdp.depth_image_valid);
  T64_CHECK_INT(machine->rdp_depth_hidden, NULL);
  t64_machine_destroy(machine);
}

static void case_external_write_invalidation_and_reset(void) {
  primitive_result result;
  t64_machine *machine;
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  T64_CHECK(run_primitive_vector(UINT64_C(0x2f00000000000034),
                                 UINT64_C(0x2e00000013570009), 0xfffcu,
                                 0u, &result));
  T64_CHECK_INT(result.hidden, 0x83u);

  machine = create_depth_machine();
  if (machine == NULL) return;
  machine->rdp_depth_hidden[DEPTH_IMAGE_ADDRESS / 2u] = UINT8_C(0x83);
  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);
  machine->rdp_depth_hidden[DEPTH_IMAGE_ADDRESS / 2u] = UINT8_C(0x82);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) != 0);
  machine->rdp_depth_hidden[DEPTH_IMAGE_ADDRESS / 2u] = UINT8_C(0x83);

  machine->rdp.depth_image_address += UINT32_C(8);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) != 0);
  machine->rdp.depth_image_address -= UINT32_C(8);
  machine->rdp.depth_image_valid = true;
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) != 0);
  machine->rdp.depth_image_valid = false;
  machine->rdp.primitive_z = UINT16_C(1);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) != 0);
  machine->rdp.primitive_z = 0u;
  machine->rdp.primitive_dz = UINT16_C(1);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) != 0);
  machine->rdp.primitive_dz = 0u;
  machine->rdp.primitive_depth_valid = true;
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) != 0);
  machine->rdp.primitive_depth_valid = false;

  T64_CHECK_INT(t64_machine_bus_write(machine, DEPTH_IMAGE_ADDRESS + 1u, 1u,
                                      UINT8_C(0x12)), T64_STATUS_OK);
  T64_CHECK_INT(machine->rdp_depth_hidden[DEPTH_IMAGE_ADDRESS / 2u], 0u);
  machine->rdp_depth_hidden[0] = UINT8_C(0x81);
  T64_CHECK_INT(t64_machine_bus_write(machine, 0u, 1u, UINT8_C(0x34)),
                T64_STATUS_OK);
  T64_CHECK_INT(machine->rdp_depth_hidden[0], 0u);
  {
    uint32_t last_byte = machine->profile.rdram_bytes - 1u;
    uint32_t last_halfword = last_byte / 2u;
    machine->rdp_depth_hidden[last_halfword] = UINT8_C(0x82);
    T64_CHECK_INT(t64_machine_bus_write(machine, last_byte, 1u,
                                        UINT8_C(0x56)), T64_STATUS_OK);
    T64_CHECK_INT(machine->rdp_depth_hidden[last_halfword], 0u);
  }
  machine->rdp_depth_hidden[DEPTH_IMAGE_ADDRESS / 2u] = UINT8_C(0x83);
  t64_rdp_reset_machine_state(machine);
  T64_CHECK_INT(t64_machine_admit_rom(machine, admitted_test_rom,
                                      admitted_test_rom_length),
                T64_STATUS_OK);
  T64_CHECK_INT(machine->rdp_depth_hidden[DEPTH_IMAGE_ADDRESS / 2u], 0u);
  t64_machine_destroy(machine);
}

typedef struct {
  const char *name;
  void (*run)(void);
} test_case;

static const test_case cases[] = {
    {"z-depth-primitive-accepted-raw-vi",
     case_primitive_depth_raw_packet_accepted_effect},
    {"z-depth-rgba8888-vi", case_rgba8888_depth_and_vi_output},
    {"z-depth-all-packet-opcodes", case_all_z_triangle_opcodes_reach_pixel},
    {"z-depth-split-interleaved-equality",
     case_split_interleaved_depth_outputs_equal},
    {"z-depth-signed-fractional-q16",
     case_q16_signed_fractional_depth_lanes},
    {"z-depth-delta-priority-boundaries",
     case_primitive_delta_priority_boundaries},
    {"z-depth-compression-exponent-transitions",
     case_primitive_compression_exponent_transitions},
    {"z-depth-compare-update-coverage-modes",
     case_depth_compare_update_and_coverage_modes},
    {"z-depth-alpha-reject-output-precedence",
     case_alpha_reject_preserves_depth_and_color},
    {"z-depth-refusal-truncation-recovery",
     case_refusal_truncation_and_recovery},
    {"z-depth-sidecar-invalidation-reset-digest",
     case_external_write_invalidation_and_reset},
};

int main(int argc, char **argv) {
  size_t i;

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) puts(cases[i].name);
    return 0;
  }
  if (argc != 1) return 2;
  for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) cases[i].run();
  return T64_TEST_FINISH("unit.core.rdp.depth");
}
