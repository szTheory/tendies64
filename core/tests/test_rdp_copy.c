#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_test.h"
#include "t64_internal.h"
#include "tendies64/t64_machine.h"

#define TEST_PROFILE_ID "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v2"
#define TEST_ROM_CAPACITY 65536u
#define COMMAND_ADDRESS UINT32_C(0x100)
#define TEXTURE_ADDRESS UINT32_C(0x4000)
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

static t64_machine *create_authored_machine(void) {
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
  T64_CHECK_INT(t64_profile_lookup(TEST_PROFILE_ID, &profile), T64_STATUS_OK);
  T64_CHECK(profile != NULL);
  if (profile == NULL) return NULL;
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  if (machine == NULL) return NULL;
  T64_CHECK_INT(t64_machine_admit_rom(machine, rom, (size_t)length),
                T64_STATUS_OK);
  return machine;
}

static void write_be64(uint8_t *bytes, uint64_t value) {
  size_t i;
  for (i = 0u; i < 8u; i++)
    bytes[i] = (uint8_t)(value >> (56u - (uint32_t)i * 8u));
}

static uint64_t set_image(uint32_t opcode, uint32_t format, uint32_t size,
                          uint32_t width, uint32_t address) {
  return ((uint64_t)opcode << 56) | ((uint64_t)format << 53) |
         ((uint64_t)size << 51) | ((uint64_t)(width - 1u) << 32) | address;
}

static uint64_t set_tile(uint32_t format, uint32_t size, uint32_t line_words,
                         uint32_t tmem_word, uint32_t tile,
                         uint32_t palette, uint32_t mode_t, uint32_t mask_t,
                         uint32_t shift_t, uint32_t mode_s, uint32_t mask_s,
                         uint32_t shift_s) {
  return (UINT64_C(0x35) << 56) | ((uint64_t)format << 53) |
         ((uint64_t)size << 51) | ((uint64_t)line_words << 41) |
         ((uint64_t)tmem_word << 32) | ((uint64_t)tile << 24) |
         ((uint64_t)palette << 20) | ((uint64_t)mode_t << 18) |
         ((uint64_t)mask_t << 14) | ((uint64_t)shift_t << 10) |
         ((uint64_t)mode_s << 8) | ((uint64_t)mask_s << 4) | shift_s;
}

static uint64_t tile_bounds(uint32_t opcode, uint32_t tile, uint32_t width,
                            uint32_t height) {
  return ((uint64_t)opcode << 56) | ((uint64_t)tile << 24) |
         ((uint64_t)(width - 1u) * 4u << 12) |
         ((uint64_t)(height - 1u) * 4u);
}

static uint64_t tile_rect(uint32_t opcode, uint32_t tile, uint32_t uls,
                          uint32_t ult, uint32_t lrs, uint32_t lrt) {
  return ((uint64_t)opcode << 56) | ((uint64_t)tile << 24) |
         ((uint64_t)(uls * 4u) << 44) |
         ((uint64_t)(ult * 4u) << 32) |
         ((uint64_t)(lrs * 4u) << 12) | (uint64_t)(lrt * 4u);
}

static bool dispatch_all(t64_machine *machine) {
  size_t step;

  for (step = 0u; step < 2048u && machine->queue_count != 0u; step++) {
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
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_START, 4u, COMMAND_ADDRESS),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_END, 4u, end),
                T64_STATUS_OK);
  return dispatch_all(machine);
}

static bool run_texture_load(t64_machine *machine, uint32_t format,
                             uint32_t size, uint32_t width, uint32_t height,
                             uint32_t source, uint32_t tmem_word,
                             uint32_t tile_index) {
  uint32_t line_words = (width * (UINT32_C(4) << size) + 63u) / 64u;
  uint64_t commands[4];

  if (line_words == 0u) line_words = 1u;
  commands[0] = set_image(UINT32_C(0x3d), format, size, width, source);
  commands[1] = set_tile(format, size, line_words, tmem_word, tile_index,
                         0u, 0u, 0u, 0u, 0u, 0u, 0u);
  commands[2] = tile_bounds(UINT32_C(0x34), tile_index, width, height);
  commands[3] = tile_bounds(UINT32_C(0x32), tile_index, width, height);
  return run_commands(machine, commands,
                      sizeof commands / sizeof commands[0]);
}

static bool execute_copy_scene_with_shift(
    t64_machine *machine, const uint8_t *source, uint32_t format,
    uint32_t size, uint32_t source_width, uint32_t source_height,
    uint32_t output_width, uint32_t output_height, uint32_t start_s,
    uint32_t start_t, uint32_t mode_s, uint32_t mask_s, uint32_t shift_s,
    uint32_t shift_t, bool flip) {
  uint32_t line_words = (source_width * (UINT32_C(4) << size) + 63u) / 64u;
  uint64_t texrect_first;
  uint64_t texrect_second;
  uint64_t commands[10];
  size_t count = 0u;
  size_t source_bytes = ((size_t)source_width * source_height *
                         (UINT32_C(4) << size) + 7u) /
                        8u;

  if (machine == NULL || source == NULL || source_bytes > 4096u) return false;
  if (line_words == 0u) line_words = 1u;
  memcpy(machine->ram + TEXTURE_ADDRESS, source, source_bytes);
  (void)memset(machine->ram + FRAMEBUFFER_ADDRESS, 0xa5,
               (size_t)output_width * output_height * 2u);
  commands[count++] = set_image(UINT32_C(0x3f), 0u, 2u, output_width,
                                FRAMEBUFFER_ADDRESS);
  commands[count++] = (UINT64_C(0x2d) << 56) |
                      ((uint64_t)(output_width * 4u) << 12) |
                      ((uint64_t)(output_height * 4u));
  commands[count++] = (UINT64_C(0x2f) << 56) |
                      (UINT64_C(3) << 52) |
                      ((uint64_t)machine->rdp.tlut_format << 14);
  commands[count++] = set_image(UINT32_C(0x3d), format, size, source_width,
                                TEXTURE_ADDRESS);
  commands[count++] = set_tile(format, size, line_words, 0u, 0u, 0u,
                               0u, 0u, shift_t, mode_s, mask_s, shift_s);
  commands[count++] = tile_bounds(UINT32_C(0x34), 0u, source_width,
                                  source_height);
  commands[count++] = tile_bounds(UINT32_C(0x32), 0u, source_width,
                                  source_height);
  texrect_first = ((uint64_t)(flip ? 0x25u : 0x24u) << 56) |
                  ((uint64_t)(output_width * 4u) << 12) |
                  ((uint64_t)(output_height * 4u));
  texrect_second = ((uint64_t)start_s << 44) | ((uint64_t)start_t << 32) |
                   (UINT64_C(4096) << 16) | UINT64_C(1024);
  commands[count++] = texrect_first;
  commands[count++] = texrect_second;
  commands[count++] = UINT64_C(0x2900000000000000);
  if (!run_commands(machine, commands, count) ||
      machine->rdp.refusal_reason != T64_RDP_REFUSAL_NONE) return false;
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT(machine->rdp.dpc_current,
                COMMAND_ADDRESS + (uint32_t)(count * 8u));
  T64_CHECK_INT(machine->rdp.copy_count, 1u);
  return machine->rdp.refusal_reason == T64_RDP_REFUSAL_NONE;
}

static bool execute_copy_scene(t64_machine *machine, const uint8_t *source,
                               uint32_t format, uint32_t size,
                               uint32_t source_width, uint32_t source_height,
                               uint32_t output_width, uint32_t output_height,
                               uint32_t start_s, uint32_t start_t,
                               uint32_t mode_s, uint32_t mask_s, bool flip) {
  return execute_copy_scene_with_shift(
      machine, source, format, size, source_width, source_height,
      output_width, output_height, start_s, start_t, mode_s, mask_s, 0u, 0u,
      flip);
}

static t64_machine *run_raw_rect(const uint8_t *source, uint32_t format,
                                 uint32_t size, uint32_t source_width,
                                 uint32_t source_height,
                                 uint32_t output_width,
                                 uint32_t output_height, uint32_t xh,
                                 uint32_t yh, uint32_t xl, uint32_t yl,
                                 uint32_t scissor_xh, uint32_t scissor_yh,
                                 uint32_t scissor_xl, uint32_t scissor_yl,
                                 uint32_t color_address, bool include_second) {
  t64_machine *machine = create_authored_machine();
  uint32_t line_words = (source_width * (UINT32_C(4) << size) + 63u) / 64u;
  uint64_t commands[10];
  size_t count = 0u;
  size_t source_bytes = ((size_t)source_width * source_height *
                         (UINT32_C(4) << size) + 7u) /
                        8u;
  size_t output_bytes = (size_t)output_width * output_height * 2u;

  if (machine == NULL || source == NULL || source_bytes > 4096u) return machine;
  if (line_words == 0u) line_words = 1u;
  memcpy(machine->ram + TEXTURE_ADDRESS, source, source_bytes);
  if ((uint64_t)color_address < machine->profile.rdram_bytes) {
    size_t available = (size_t)(machine->profile.rdram_bytes - color_address);
    if (output_bytes > available) output_bytes = available;
    (void)memset(machine->ram + color_address, 0xa5, output_bytes);
  }
  commands[count++] = set_image(UINT32_C(0x3f), 0u, 2u, output_width,
                                color_address);
  commands[count++] = (UINT64_C(0x2d) << 56) |
                      ((uint64_t)scissor_xh << 44) |
                      ((uint64_t)scissor_yh << 32) |
                      ((uint64_t)scissor_xl << 12) | scissor_yl;
  commands[count++] = (UINT64_C(0x2f) << 56) | (UINT64_C(3) << 52);
  commands[count++] = set_image(UINT32_C(0x3d), format, size, source_width,
                                TEXTURE_ADDRESS);
  commands[count++] = set_tile(format, size, line_words, 0u, 0u, 0u,
                               0u, 0u, 0u, 0u, 0u, 0u);
  commands[count++] = tile_bounds(UINT32_C(0x34), 0u, source_width,
                                  source_height);
  commands[count++] = tile_bounds(UINT32_C(0x32), 0u, source_width,
                                  source_height);
  commands[count++] = (UINT64_C(0x24) << 56) |
                      ((uint64_t)xh << 44) | ((uint64_t)yh << 32) |
                      ((uint64_t)xl << 12) | yl;
  if (include_second)
    commands[count++] = (UINT64_C(4096) << 16) | UINT64_C(1024);
  if (include_second)
    commands[count++] = UINT64_C(0x2900000000000000);
  T64_CHECK(run_commands(machine, commands, count));
  return machine;
}

static t64_machine *run_copy_scene(const uint8_t *source, uint32_t format,
                                   uint32_t size, uint32_t source_width,
                                   uint32_t source_height,
                                   uint32_t output_width,
                                   uint32_t output_height, uint32_t start_s,
                                   uint32_t start_t, uint32_t mode_s,
                                   uint32_t mask_s, bool flip) {
  t64_machine *machine = create_authored_machine();

  if (machine == NULL) return NULL;
  if (!execute_copy_scene(machine, source, format, size, source_width,
                          source_height, output_width, output_height,
                          start_s, start_t, mode_s, mask_s, flip)) {
    return machine;
  }
  return machine;
}

static bool configure_vi_and_copy(t64_machine *machine, uint32_t width,
                                  uint32_t height, uint8_t *bytes,
                                  size_t capacity, size_t *length_out) {
  const struct {
    uint32_t offset;
    uint32_t value;
  } writes[] = {
      {UINT32_C(0x04), FRAMEBUFFER_ADDRESS},
      {UINT32_C(0x08), width},
      {UINT32_C(0x1c), 3u},
      {UINT32_C(0x18), 0u},
      {UINT32_C(0x24), width},
      {UINT32_C(0x28), height},
      {UINT32_C(0x30), UINT32_C(1024)},
      {UINT32_C(0x34), UINT32_C(1024)},
      {UINT32_C(0x00), UINT32_C(2)},
  };
  size_t i;
  size_t step;
  t64_video_packet_info info = {0};

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
  T64_CHECK_INT(info.width, width);
  T64_CHECK_INT(info.height, height);
  T64_CHECK(info.packet_bytes <= capacity);
  if (info.packet_bytes > capacity) return false;
  T64_CHECK_INT(t64_video_copy(machine, bytes, capacity, length_out),
                T64_STATUS_OK);
  return true;
}

static void case_load_tile_layout(void) {
  const uint8_t source[] = {
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
      0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
      0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28};
  const uint8_t expected_tmem[16] = {
      0x13, 0x14, 0x15, 0x16, 0, 0, 0, 0,
      0, 0, 0, 0, 0x23, 0x24, 0x25, 0x26};
  const uint64_t commands[] = {
      set_image(UINT32_C(0x3d), 0u, 2u, 4u, TEXTURE_ADDRESS),
      set_tile(0u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u),
      tile_rect(UINT32_C(0x34), 0u, 1u, 1u, 2u, 2u),
  };
  t64_machine *machine = create_authored_machine();

  if (machine == NULL) return;
  memcpy(machine->ram + TEXTURE_ADDRESS, source, sizeof source);
  T64_CHECK(run_commands(machine, commands,
                         sizeof commands / sizeof commands[0]));
  T64_CHECK(memcmp(machine->rdp.tmem, expected_tmem,
                   sizeof expected_tmem) == 0);
  T64_CHECK_INT(machine->rdp.tiles[0].uls, 4u);
  T64_CHECK_INT(machine->rdp.tiles[0].ult, 4u);
  T64_CHECK_INT(machine->rdp.tiles[0].lrs, 8u);
  T64_CHECK_INT(machine->rdp.tiles[0].lrt, 8u);
  t64_machine_destroy(machine);
}

static void case_load_block_dxt(void) {
  const uint8_t source[] = {0xf8, 0x01, 0x07, 0xc1, 0x00, 0x3f, 0xff, 0xff,
                            0x12, 0x35, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
  const uint64_t commands[] = {
      set_image(UINT32_C(0x3d), 0u, 2u, 4u, TEXTURE_ADDRESS),
      set_tile(0u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u),
      (UINT64_C(0x33) << 56) | (UINT64_C(28) << 12) | UINT64_C(0x800),
  };
  const uint8_t expected[16] = {0xf8, 0x01, 0x07, 0xc1, 0x00, 0x3f, 0xff, 0xff,
                                0x89, 0xab, 0xcd, 0xef, 0x12, 0x35, 0x45, 0x67};
  const uint8_t expected_linear[16] = {
      0xf8, 0x01, 0x07, 0xc1, 0x00, 0x3f, 0xff, 0xff,
      0x12, 0x35, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
  const uint64_t linear_commands[] = {
      set_image(UINT32_C(0x3d), 0u, 2u, 8u, TEXTURE_ADDRESS),
      set_tile(0u, 2u, 2u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u),
      (UINT64_C(0x33) << 56) | (UINT64_C(28) << 12),
  };
  const uint64_t overflowing_commands[] = {
      set_image(UINT32_C(0x3d), 0u, 2u, 8u, TEXTURE_ADDRESS),
      set_tile(0u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u),
      (UINT64_C(0x33) << 56) | (UINT64_C(28) << 12),
  };
  t64_machine *machine = create_authored_machine();

  if (machine == NULL) return;
  memcpy(machine->ram + TEXTURE_ADDRESS, source, sizeof source);
  T64_CHECK(run_commands(machine, commands,
                         sizeof commands / sizeof commands[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK(memcmp(machine->rdp.tmem, expected, sizeof expected) == 0);
  T64_CHECK(run_commands(machine, linear_commands,
                         sizeof linear_commands / sizeof linear_commands[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK(memcmp(machine->rdp.tmem, expected_linear,
                   sizeof expected_linear) == 0);
  T64_CHECK(run_commands(machine, overflowing_commands,
                         sizeof overflowing_commands /
                             sizeof overflowing_commands[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason,
                T64_RDP_REFUSAL_TEXTURE_RANGE);
  T64_CHECK(memcmp(machine->rdp.tmem, expected_linear,
                   sizeof expected_linear) == 0);
  t64_machine_destroy(machine);
}

static void case_load_tlut_ci(void) {
  const uint8_t palette[] = {0xf8, 0x01, 0x07, 0xc1};
  const uint8_t indices[] = {0u, 1u};
  const uint8_t expected_pixels[] = {0xf8, 0x01, 0x07, 0xc1};
  const uint8_t expected_vi[] = {255, 0, 0, 255, 0, 255, 0, 255};
  const uint8_t ia_palette[] = {0x80, 0x40};
  const uint8_t ci_index[] = {0u};
  const uint8_t expected_ia_pixel[] = {0x84, 0x20};
  const uint64_t tlut_commands[] = {
      set_image(UINT32_C(0x3d), 0u, 2u, 2u, TEXTURE_ADDRESS),
      set_tile(0u, 2u, 1u, 256u, 7u, 0u, 0u, 0u, 0u, 0u, 0u, 0u),
      (UINT64_C(0x30) << 56) | (UINT64_C(7) << 24) | (UINT64_C(4) << 12),
      (UINT64_C(0x2f) << 56) | (UINT64_C(2) << 14),
  };
  t64_machine *machine;
  uint8_t video[8];
  size_t video_length = 0u;
  size_t i;

  machine = create_authored_machine();
  if (machine == NULL) return;
  memcpy(machine->ram + TEXTURE_ADDRESS, palette, sizeof palette);
  T64_CHECK(run_commands(machine, tlut_commands,
                         sizeof tlut_commands / sizeof tlut_commands[0]));
  for (i = 0u; i < 2u; i++)
    T64_CHECK(memcmp(machine->rdp.tmem + 2048u + i * 8u, palette + i * 2u,
                     2u) == 0);
  memcpy(machine->ram + TEXTURE_ADDRESS + 16u, indices, sizeof indices);
  T64_CHECK(execute_copy_scene(machine, indices, 2u, 1u, 2u, 1u,
                               2u, 1u, 0u, 0u, 0u, 0u, false));
  T64_CHECK(memcmp(machine->ram + FRAMEBUFFER_ADDRESS, expected_pixels,
                   sizeof expected_pixels) == 0);
  T64_CHECK(configure_vi_and_copy(machine, 2u, 1u, video, sizeof video,
                                  &video_length));
  T64_CHECK_INT(video_length, sizeof expected_vi);
  T64_CHECK(memcmp(video, expected_vi, sizeof expected_vi) == 0);

  t64_machine_destroy(machine);
  machine = create_authored_machine();
  if (machine == NULL) return;
  memcpy(machine->ram + TEXTURE_ADDRESS + 32u, ia_palette,
         sizeof ia_palette);
  {
    const uint64_t ia_tlut_commands[] = {
        set_image(UINT32_C(0x3d), 3u, 2u, 1u, TEXTURE_ADDRESS + 32u),
        set_tile(3u, 2u, 1u, 256u, 7u, 0u, 0u, 0u, 0u, 0u, 0u, 0u),
        (UINT64_C(0x30) << 56) | (UINT64_C(7) << 24),
        (UINT64_C(0x2f) << 56) | (UINT64_C(3) << 14),
    };
    T64_CHECK(run_commands(machine, ia_tlut_commands,
                           sizeof ia_tlut_commands /
                               sizeof ia_tlut_commands[0]));
  }
  T64_CHECK_INT(machine->rdp.tlut_format, 3u);
  T64_CHECK(execute_copy_scene(machine, ci_index, 2u, 0u, 1u, 1u,
                               1u, 1u, 0u, 0u, 0u, 0u, false));
  T64_CHECK(memcmp(machine->ram + FRAMEBUFFER_ADDRESS, expected_ia_pixel,
                   sizeof expected_ia_pixel) == 0);
  t64_machine_destroy(machine);
}

static void case_load_tlut_boundary(void) {
  t64_machine *machine = create_authored_machine();
  uint64_t full_table[] = {
      set_image(UINT32_C(0x3d), 0u, 2u, 256u, TEXTURE_ADDRESS),
      set_tile(0u, 2u, 64u, 256u, 7u, 0u, 0u, 0u, 0u, 0u, 0u, 0u),
      (UINT64_C(0x30) << 56) | (UINT64_C(7) << 24) |
          (UINT64_C(1020) << 12),
  };
  uint64_t overrun[] = {
      set_tile(0u, 2u, 1u, 511u, 7u, 0u, 0u, 0u, 0u, 0u, 0u, 0u),
      (UINT64_C(0x30) << 56) | (UINT64_C(7) << 24) | (UINT64_C(4) << 12),
  };
  uint8_t last_entry[8];
  uint32_t i;

  if (machine == NULL) return;
  for (i = 0u; i < 512u; i++)
    machine->ram[TEXTURE_ADDRESS + i] = (uint8_t)(i ^ UINT32_C(0x5a));
  T64_CHECK(run_commands(machine, full_table,
                         sizeof full_table / sizeof full_table[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT(machine->rdp.tmem[2048], machine->ram[TEXTURE_ADDRESS]);
  T64_CHECK_INT(machine->rdp.tmem[2049], machine->ram[TEXTURE_ADDRESS + 1u]);
  T64_CHECK_INT(machine->rdp.tmem[4094], machine->ram[TEXTURE_ADDRESS + 510u]);
  T64_CHECK_INT(machine->rdp.tmem[4095], machine->ram[TEXTURE_ADDRESS + 511u]);
  memcpy(last_entry, machine->rdp.tmem + 4088u, sizeof last_entry);
  T64_CHECK(run_commands(machine, overrun,
                         sizeof overrun / sizeof overrun[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason,
                T64_RDP_REFUSAL_TEXTURE_RANGE);
  T64_CHECK(memcmp(machine->rdp.tmem + 4088u, last_entry,
                   sizeof last_entry) == 0);
  t64_machine_destroy(machine);
}

static void case_rgba_16_32(void) {
  const uint8_t rgba16[] = {0xf8, 0x01, 0x07, 0xc1};
  const uint8_t rgba32[] = {0xff, 0x00, 0x00, 0xff,
                            0x00, 0xff, 0x00, 0xff};
  t64_machine *machine = create_authored_machine();

  if (machine == NULL) return;
  memcpy(machine->ram + TEXTURE_ADDRESS, rgba16, sizeof rgba16);
  T64_CHECK(run_texture_load(machine, 0u, 2u, 2u, 1u,
                            TEXTURE_ADDRESS, 0u, 0u));
  T64_CHECK(memcmp(machine->rdp.tmem, rgba16, sizeof rgba16) == 0);
  memcpy(machine->ram + TEXTURE_ADDRESS + 16u, rgba32, sizeof rgba32);
  T64_CHECK(run_texture_load(machine, 0u, 3u, 2u, 1u,
                            TEXTURE_ADDRESS + 16u, 0u, 0u));
  T64_CHECK(memcmp(machine->rdp.tmem, rgba32, sizeof rgba32) == 0);
  t64_machine_destroy(machine);
}

static void check_packed_format(uint32_t format, uint32_t size,
                                const uint8_t *source, size_t length) {
  t64_machine *machine = create_authored_machine();

  if (machine == NULL) return;
  memcpy(machine->ram + TEXTURE_ADDRESS, source, length);
  T64_CHECK(run_texture_load(machine, format, size, 2u, 1u,
                            TEXTURE_ADDRESS, 0u, 0u));
  T64_CHECK(memcmp(machine->rdp.tmem, source, length) == 0);
  t64_machine_destroy(machine);
}

static void case_ia_i_4_8_16(void) {
  const uint8_t ia4[] = {0x6d};
  const uint8_t ia8[] = {0x4f, 0x82};
  const uint8_t ia16[] = {0x40, 0x80, 0xff, 0x00};
  const uint8_t i4[] = {0x3c};
  const uint8_t i8[] = {0x55, 0xaa};

  check_packed_format(3u, 0u, ia4, sizeof ia4);
  check_packed_format(3u, 1u, ia8, sizeof ia8);
  check_packed_format(3u, 2u, ia16, sizeof ia16);
  check_packed_format(4u, 0u, i4, sizeof i4);
  check_packed_format(4u, 1u, i8, sizeof i8);
}

static void case_truncated_load(void) {
  const uint32_t source = UINT32_C(0x00400000) - 2u;
  const uint64_t commands[] = {
      set_image(UINT32_C(0x3d), 0u, 2u, 2u, source),
      set_tile(0u, 2u, 1u, 511u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u),
      tile_bounds(UINT32_C(0x34), 0u, 2u, 1u),
  };
  t64_machine *machine = create_authored_machine();
  uint8_t unchanged[8];

  if (machine == NULL) return;
  memset(machine->rdp.tmem + 4088u, 0x5a, sizeof unchanged);
  T64_CHECK(run_commands(machine, commands,
                         sizeof commands / sizeof commands[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason,
                T64_RDP_REFUSAL_TEXTURE_RANGE);
  memset(unchanged, 0x5a, sizeof unchanged);
  T64_CHECK(memcmp(machine->rdp.tmem + 4088u, unchanged,
                   sizeof unchanged) == 0);
  t64_machine_destroy(machine);
}

static void case_yuv_conversion(void) {
  const uint8_t yuv_pair[] = {100u, 148u, 110u, 168u};
  const uint64_t set_convert = (UINT64_C(0x2c) << 56) |
                               (UINT64_C(0x040) << 45);
  const uint64_t load_commands[] = {
      set_image(UINT32_C(0x3d), 1u, 2u, 2u, TEXTURE_ADDRESS),
      set_tile(1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u),
      tile_bounds(UINT32_C(0x34), 0u, 2u, 1u),
  };
  const uint64_t convert_commands[] = {set_convert};
  uint8_t rgb[3] = {0u, 0u, 0u};
  t64_machine *machine = create_authored_machine();

  if (machine == NULL) return;
  memcpy(machine->ram + TEXTURE_ADDRESS, yuv_pair, sizeof yuv_pair);
  T64_CHECK(run_commands(machine, load_commands,
                         sizeof load_commands / sizeof load_commands[0]));
  T64_CHECK(memcmp(machine->rdp.tmem, yuv_pair, sizeof yuv_pair) == 0);
  T64_CHECK(run_commands(machine, convert_commands, 1u));
  t64_rdp_tmem_convert_yuv(&machine->rdp, machine->rdp.tmem[0],
                           machine->rdp.tmem[1], machine->rdp.tmem[3], rgb);
  T64_CHECK_INT(rgb[0], 120u);
  T64_CHECK_INT(rgb[1], 100u);
  T64_CHECK_INT(rgb[2], 100u);
  t64_machine_destroy(machine);
}

static void case_copy_rect_flip(void) {
  const uint8_t source[] = {0xf8, 0x01, 0x07, 0xc1,
                            0x00, 0x3f, 0xff, 0xff};
  const uint8_t expected_raw[] = {0xf8, 0x01, 0x00, 0x3f,
                                  0x07, 0xc1, 0xff, 0xff};
  const uint8_t expected_video[] = {255, 0, 0, 255, 0, 0, 255, 255,
                                    0, 255, 0, 255, 255, 255, 255, 255};
  t64_machine *machine = run_copy_scene(source, 0u, 2u, 2u, 2u, 2u, 2u,
                                        0u, 0u, 0u, 0u, true);
  uint8_t video[sizeof expected_video];
  size_t video_length = 0u;

  if (machine == NULL) return;
  T64_CHECK(memcmp(machine->ram + FRAMEBUFFER_ADDRESS, expected_raw,
                   sizeof expected_raw) == 0);
  T64_CHECK(configure_vi_and_copy(machine, 2u, 2u, video, sizeof video,
                                  &video_length));
  T64_CHECK_INT(video_length, sizeof expected_video);
  T64_CHECK(memcmp(video, expected_video, sizeof expected_video) == 0);
  t64_machine_destroy(machine);
}

static void case_copy_tile_origin(void) {
  const uint8_t source[] = {0xf8, 0x01, 0x07, 0xc1,
                            0x00, 0x3f, 0xff, 0xff};
  const uint8_t expected[] = {0x07, 0xc1, 0x00, 0x3f};
  const uint64_t commands[] = {
      set_image(UINT32_C(0x3f), 0u, 2u, 2u, FRAMEBUFFER_ADDRESS),
      (UINT64_C(0x2d) << 56) | (UINT64_C(8) << 12) | UINT64_C(4),
      (UINT64_C(0x2f) << 56) | (UINT64_C(3) << 52),
      set_image(UINT32_C(0x3d), 0u, 2u, 4u, TEXTURE_ADDRESS),
      set_tile(0u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u),
      tile_rect(UINT32_C(0x34), 0u, 1u, 0u, 2u, 0u),
      tile_rect(UINT32_C(0x32), 0u, 1u, 0u, 2u, 0u),
      (UINT64_C(0x24) << 56) | (UINT64_C(8) << 12) | UINT64_C(4),
      (UINT64_C(32) << 44) | (UINT64_C(4096) << 16) | UINT64_C(1024),
      UINT64_C(0x2900000000000000),
  };
  t64_machine *machine = create_authored_machine();

  if (machine == NULL) return;
  memcpy(machine->ram + TEXTURE_ADDRESS, source, sizeof source);
  T64_CHECK(run_commands(machine, commands,
                         sizeof commands / sizeof commands[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT(machine->rdp.copy_count, 1u);
  T64_CHECK(memcmp(machine->ram + FRAMEBUFFER_ADDRESS, expected,
                   sizeof expected) == 0);
  t64_machine_destroy(machine);
}

static void case_tile_wrap_mirror_clamp(void) {
  const uint8_t source[] = {0xf8, 0x01, 0x07, 0xc1,
                            0x00, 0x3f, 0xff, 0xff};
  const uint8_t wrap_expected[] = {0xff, 0xff, 0xf8, 0x01,
                                   0x07, 0xc1, 0x00, 0x3f};
  const uint8_t mirror_expected[] = {0xff, 0xff, 0xff, 0xff,
                                     0x00, 0x3f, 0x07, 0xc1};
  const uint8_t clamp_expected[] = {0x00, 0x3f, 0xff, 0xff,
                                    0xff, 0xff, 0xff, 0xff};
  const uint8_t shift_expected[] = {0x07, 0xc1, 0x07, 0xc1};
  t64_machine *machine;

  machine = run_copy_scene(source, 0u, 2u, 4u, 1u, 4u, 1u,
                           3u * 32u, 0u, 0u, 2u, false);
  if (machine != NULL) {
    T64_CHECK(memcmp(machine->ram + FRAMEBUFFER_ADDRESS, wrap_expected,
                     sizeof wrap_expected) == 0);
    t64_machine_destroy(machine);
  }
  machine = run_copy_scene(source, 0u, 2u, 4u, 1u, 4u, 1u,
                           3u * 32u, 0u, 1u, 2u, false);
  if (machine != NULL) {
    T64_CHECK(memcmp(machine->ram + FRAMEBUFFER_ADDRESS, mirror_expected,
                     sizeof mirror_expected) == 0);
    t64_machine_destroy(machine);
  }
  machine = run_copy_scene(source, 0u, 2u, 4u, 1u, 4u, 1u,
                           2u * 32u, 0u, 2u, 0u, false);
  if (machine != NULL) {
    T64_CHECK(memcmp(machine->ram + FRAMEBUFFER_ADDRESS, clamp_expected,
                     sizeof clamp_expected) == 0);
    t64_machine_destroy(machine);
  }
  machine = create_authored_machine();
  if (machine != NULL) {
    T64_CHECK(execute_copy_scene_with_shift(
        machine, source, 0u, 2u, 4u, 1u, 2u, 1u, 2u * 32u, 0u,
        0u, 0u, 1u, 0u, false));
    T64_CHECK(memcmp(machine->ram + FRAMEBUFFER_ADDRESS, shift_expected,
                     sizeof shift_expected) == 0);
    t64_machine_destroy(machine);
  }
}

static void case_tmem_first_last(void) {
  const uint8_t source[] = {0x01, 0x23, 0x45, 0x67,
                            0x89, 0xab, 0xcd, 0xef};
  t64_machine *machine = create_authored_machine();

  if (machine == NULL) return;
  memcpy(machine->ram + TEXTURE_ADDRESS, source, sizeof source);
  T64_CHECK(run_texture_load(machine, 4u, 0u, 16u, 1u,
                            TEXTURE_ADDRESS, 0u, 0u));
  T64_CHECK(memcmp(machine->rdp.tmem, source, sizeof source) == 0);
  T64_CHECK(run_texture_load(machine, 4u, 0u, 16u, 1u,
                            TEXTURE_ADDRESS, 511u, 7u));
  T64_CHECK(memcmp(machine->rdp.tmem + 4088u, source, sizeof source) == 0);
  t64_machine_destroy(machine);
}

static void case_adjacent_tile_no_alias(void) {
  const uint8_t first[] = {0xf8, 0x01, 0x07, 0xc1,
                           0x00, 0x3f, 0xff, 0xff};
  const uint8_t second[] = {0x12, 0x34, 0x56, 0x78,
                            0x9a, 0xbc, 0xde, 0xf0};
  t64_machine *machine = create_authored_machine();

  if (machine == NULL) return;
  memcpy(machine->ram + TEXTURE_ADDRESS, first, sizeof first);
  T64_CHECK(run_texture_load(machine, 0u, 2u, 4u, 1u,
                            TEXTURE_ADDRESS, 0u, 0u));
  memcpy(machine->ram + TEXTURE_ADDRESS + 16u, second, sizeof second);
  T64_CHECK(run_texture_load(machine, 0u, 2u, 4u, 1u,
                            TEXTURE_ADDRESS + 16u, 1u, 1u));
  T64_CHECK(memcmp(machine->rdp.tmem, first, sizeof first) == 0);
  T64_CHECK(memcmp(machine->rdp.tmem + 8u, second, sizeof second) == 0);
  t64_machine_destroy(machine);
}

static void case_scissor_touch(void) {
  const uint8_t texture[] = {0xf8, 0x01};
  t64_machine *machine = run_raw_rect(
      texture, 0u, 2u, 1u, 1u, 2u, 1u, 0u, 0u, 4u, 4u,
      4u, 0u, 8u, 4u, FRAMEBUFFER_ADDRESS, true);
  const uint8_t untouched[] = {0xa5, 0xa5, 0xa5, 0xa5};

  if (machine == NULL) return;
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK(memcmp(machine->ram + FRAMEBUFFER_ADDRESS, untouched,
                   sizeof untouched) == 0);
  t64_machine_destroy(machine);
}

static void case_single_texel(void) {
  const uint8_t texture[] = {0x07, 0xc1};
  t64_machine *machine = run_copy_scene(texture, 0u, 2u, 1u, 1u,
                                        1u, 1u, 0u, 0u, 0u, 0u, false);

  if (machine == NULL) return;
  T64_CHECK(memcmp(machine->ram + FRAMEBUFFER_ADDRESS, texture,
                   sizeof texture) == 0);
  t64_machine_destroy(machine);
}

static void case_zero_area(void) {
  const uint8_t texture[] = {0xf8, 0x01};
  const uint8_t untouched[] = {0xa5, 0xa5};
  t64_machine *machine = run_raw_rect(
      texture, 0u, 2u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 0u,
      0u, 0u, 4u, 4u, FRAMEBUFFER_ADDRESS, true);

  if (machine == NULL) return;
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK(memcmp(machine->ram + FRAMEBUFFER_ADDRESS, untouched,
                   sizeof untouched) == 0);
  t64_machine_destroy(machine);
}

static void case_bad_format(void) {
  const uint8_t texture[] = {0xf8, 0x01, 0x07, 0xc1};
  const uint64_t bad_size[] = {
      set_image(UINT32_C(0x3d), 1u, 1u, 2u, TEXTURE_ADDRESS)};
  const uint64_t valid_load[] = {
      set_image(UINT32_C(0x3d), 0u, 2u, 2u, TEXTURE_ADDRESS),
      set_tile(0u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u),
      tile_bounds(UINT32_C(0x34), 0u, 2u, 1u),
      tile_bounds(UINT32_C(0x32), 0u, 2u, 1u),
  };
  const uint64_t inverted_size[] = {
      (UINT64_C(0x32) << 56) | (UINT64_C(4) << 44) |
      (UINT64_C(4) << 32)};
  t64_machine *machine = create_authored_machine();
  uint32_t old_uls, old_lrs, old_lrt;

  if (machine == NULL) return;
  memcpy(machine->ram + TEXTURE_ADDRESS, texture, sizeof texture);
  T64_CHECK(run_commands(machine, bad_size, 1u));
  T64_CHECK_INT(machine->rdp.refusal_reason,
                T64_RDP_REFUSAL_INVALID_TEXTURE);
  T64_CHECK(run_commands(machine, valid_load,
                         sizeof valid_load / sizeof valid_load[0]));
  old_uls = machine->rdp.tiles[0].uls;
  old_lrs = machine->rdp.tiles[0].lrs;
  old_lrt = machine->rdp.tiles[0].lrt;
  T64_CHECK(run_commands(machine, inverted_size, 1u));
  T64_CHECK_INT(machine->rdp.refusal_reason,
                T64_RDP_REFUSAL_INVALID_TEXTURE);
  T64_CHECK_INT(machine->rdp.tiles[0].uls, old_uls);
  T64_CHECK_INT(machine->rdp.tiles[0].lrs, old_lrs);
  T64_CHECK_INT(machine->rdp.tiles[0].lrt, old_lrt);
  t64_machine_destroy(machine);

  /* Copy mode explicitly refuses YUV texture tiles before writing pixels. */
  machine = run_copy_scene(texture, 1u, 2u, 2u, 1u, 2u, 1u,
                           0u, 0u, 0u, 0u, false);
  if (machine != NULL) {
    T64_CHECK_INT(machine->rdp.refusal_reason,
                  T64_RDP_REFUSAL_INVALID_TEXTURE);
    T64_CHECK_INT(machine->rdp.copy_count, 0u);
    T64_CHECK_INT(machine->ram[FRAMEBUFFER_ADDRESS], 0xa5u);
    t64_machine_destroy(machine);
  }

  /* RGBA32 loads are valid, but copy mode accepts only RGBA16. */
  machine = run_copy_scene(texture, 0u, 3u, 1u, 1u, 1u, 1u,
                           0u, 0u, 0u, 0u, false);
  if (machine != NULL) {
    T64_CHECK_INT(machine->rdp.refusal_reason,
                  T64_RDP_REFUSAL_INVALID_TEXTURE);
    T64_CHECK_INT(machine->rdp.copy_count, 0u);
    T64_CHECK_INT(machine->ram[FRAMEBUFFER_ADDRESS], 0xa5u);
    t64_machine_destroy(machine);
  }
}

static void case_truncated_copy(void) {
  const uint8_t texture[] = {0xf8, 0x01};
  const uint8_t untouched[] = {0xa5, 0xa5};
  t64_machine *machine = run_raw_rect(
      texture, 0u, 2u, 1u, 1u, 1u, 1u, 0u, 0u, 4u, 4u,
      0u, 0u, 4u, 4u, FRAMEBUFFER_ADDRESS, false);

  if (machine == NULL) return;
  T64_CHECK_INT(machine->rdp.refusal_reason,
                T64_RDP_REFUSAL_TRUNCATED_WORD);
  T64_CHECK(memcmp(machine->ram + FRAMEBUFFER_ADDRESS, untouched,
                   sizeof untouched) == 0);
  T64_CHECK_INT(machine->rdp.copy_count, 0u);
  t64_machine_destroy(machine);
}

static void case_overflow_rejected(void) {
  const uint8_t texture[] = {0xf8, 0x01, 0x07, 0xc1};
  uint32_t target = UINT32_C(0x00400000) - 2u;
  const uint8_t untouched[] = {0xa5, 0xa5};
  t64_machine *machine = run_raw_rect(
      texture, 0u, 2u, 2u, 1u, 2u, 1u, 0u, 0u, 8u, 4u,
      0u, 0u, 8u, 4u, target, true);

  if (machine == NULL) return;
  T64_CHECK_INT(machine->rdp.refusal_reason,
                T64_RDP_REFUSAL_RENDER_RANGE);
  T64_CHECK(memcmp(machine->ram + target, untouched, sizeof untouched) == 0);
  t64_machine_destroy(machine);
}

static size_t first_difference(const uint8_t *actual,
                               const uint8_t *expected, size_t length) {
  size_t i;
  for (i = 0u; i < length; i++)
    if (actual[i] != expected[i]) return i;
  return SIZE_MAX;
}

static void case_copy_first_difference(void) {
  const uint8_t texture[] = {0xf8, 0x01, 0x07, 0xc1};
  const uint8_t expected[] = {0xf8, 0x01, 0x07, 0xc1};
  uint8_t corrupted[sizeof expected];
  t64_machine *machine = run_copy_scene(texture, 0u, 2u, 2u, 1u,
                                        2u, 1u, 0u, 0u, 0u, 0u, false);

  if (machine == NULL) return;
  T64_CHECK_INT(first_difference(machine->ram + FRAMEBUFFER_ADDRESS,
                                 expected, sizeof expected), SIZE_MAX);
  memcpy(corrupted, machine->ram + FRAMEBUFFER_ADDRESS, sizeof corrupted);
  corrupted[0] ^= UINT8_C(1);
  T64_CHECK_INT(first_difference(corrupted, expected, sizeof corrupted), 0u);
  T64_CHECK_INT(corrupted[0], (uint8_t)(expected[0] ^ UINT8_C(1)));
  t64_machine_destroy(machine);
}

typedef struct {
  const char *name;
  void (*run)(void);
} test_case;

static const test_case cases[] = {
    {"load-tile-layout", case_load_tile_layout},
    {"load-block-dxt", case_load_block_dxt},
    {"load-tlut-ci", case_load_tlut_ci},
    {"load-tlut-boundary", case_load_tlut_boundary},
    {"rgba-16-32", case_rgba_16_32},
    {"ia-i-4-8-16", case_ia_i_4_8_16},
    {"truncated-load", case_truncated_load},
    {"yuv-conversion", case_yuv_conversion},
    {"copy-rect-flip", case_copy_rect_flip},
    {"copy-tile-origin", case_copy_tile_origin},
    {"tile-wrap-mirror-clamp", case_tile_wrap_mirror_clamp},
    {"tmem-first-last", case_tmem_first_last},
    {"adjacent-tile-no-alias", case_adjacent_tile_no_alias},
    {"scissor-touch", case_scissor_touch},
    {"single-texel", case_single_texel},
    {"zero-area", case_zero_area},
    {"bad-format", case_bad_format},
    {"truncated-copy", case_truncated_copy},
    {"overflow-rejected", case_overflow_rejected},
    {"copy-first-difference", case_copy_first_difference},
};

int main(int argc, char **argv) {
  size_t i;

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) puts(cases[i].name);
    return 0;
  }
  if (argc != 1) return 2;
  for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) cases[i].run();
  return T64_TEST_FINISH("unit.core.rdp.copy");
}
