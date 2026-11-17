#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_internal.h"
#include "tendies64/t64_machine.h"

static unsigned test_checks;
static unsigned test_failures;

typedef struct {
  size_t allocations;
  size_t releases;
} test_allocator_stats;

static unsigned population16(uint16_t value) {
  unsigned count = 0u;
  while (value != 0u) {
    count += value & 1u;
    value >>= 1;
  }
  return count;
}

#define T64_CHECK(expression)                                                  \
  do {                                                                         \
    bool t64_ok_ = (expression);                                               \
    test_checks++;                                                             \
    if (!t64_ok_) {                                                            \
      test_failures++;                                                         \
      printf("# assertion failed: %s\n", #expression);                        \
    }                                                                          \
  } while (0)

#define T64_CHECK_INT(got, want)                                               \
  do {                                                                         \
    long long t64_got_ = (long long)(got);                                     \
    long long t64_want_ = (long long)(want);                                   \
    test_checks++;                                                             \
    if (t64_got_ != t64_want_) {                                               \
      test_failures++;                                                         \
      printf("# assertion failed: %s got %lld want %lld\n", #got,             \
             t64_got_, t64_want_);                                             \
    }                                                                          \
  } while (0)

#define BLENDER_PROFILE_ID \
  "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-blender-v6"
#define TEST_ROM_CAPACITY 65536u
#define COMMAND_ADDRESS UINT32_C(0x100)
#define FRAMEBUFFER_ADDRESS UINT32_C(0x5000)
#define VI_BASE UINT32_C(0x04400000)

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  void *memory;
  if (storage_out == NULL || alignment == 0u) return false;
  memory = malloc(size);
  if (memory == NULL || (uintptr_t)memory % alignment != 0u) {
    free(memory);
    return false;
  }
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  if (context != NULL) ((test_allocator_stats *)context)->allocations++;
  return true;
}

static void test_release(void *context, t64_host_storage storage) {
  if (context != NULL) ((test_allocator_stats *)context)->releases++;
  free(storage.data);
}

static t64_machine *create_authored_machine(void) {
  const t64_host_allocator allocator = {test_allocate, test_release, NULL};
  const char *path = getenv("T64_TEST_RDP_FILL_PATH");
  const t64_profile_descriptor *profile = NULL;
  uint8_t rom[TEST_ROM_CAPACITY];
  t64_machine *machine = NULL;
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
  T64_CHECK_INT(t64_profile_lookup(BLENDER_PROFILE_ID, &profile),
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

static bool read_test_rom(uint8_t *rom, size_t capacity, size_t *length_out) {
  const char *path = getenv("T64_TEST_RDP_FILL_PATH");
  FILE *file;
  long length;
  if (path == NULL || rom == NULL || length_out == NULL) return false;
  file = fopen(path, "rb");
  if (file == NULL) return false;
  if (fseek(file, 0L, SEEK_END) != 0 || (length = ftell(file)) <= 0 ||
      (unsigned long)length > capacity || fseek(file, 0L, SEEK_SET) != 0) {
    (void)fclose(file);
    return false;
  }
  if (fread(rom, 1u, (size_t)length, file) != (size_t)length) {
    (void)fclose(file);
    return false;
  }
  if (fclose(file) != 0) return false;
  *length_out = (size_t)length;
  return true;
}

static void write_be64(uint8_t *bytes, uint64_t value) {
  uint32_t i;
  for (i = 0u; i < 8u; i++)
    bytes[i] = (uint8_t)(value >> (56u - i * 8u));
}

static bool run_commands(t64_machine *machine, const uint64_t *words,
                         size_t count) {
  size_t step;
  uint32_t end;
  if (machine == NULL || words == NULL || count == 0u ||
      count > (UINT32_MAX - COMMAND_ADDRESS) / 8u) return false;
  machine->queue_head = 0u;
  machine->queue_count = 0u;
  for (step = 0u; step < count; step++)
    write_be64(machine->ram + COMMAND_ADDRESS + step * 8u, words[step]);
  end = COMMAND_ADDRESS + (uint32_t)(count * 8u);
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x04100000), 4u,
                                      COMMAND_ADDRESS), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x04100004), 4u, end),
                T64_STATUS_OK);
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

static bool drain_events(t64_machine *machine) {
  size_t step;
  for (step = 0u; step < 4096u && machine->queue_count != 0u; step++) {
    t64_event event = machine->events[machine->queue_head];
    machine->queue_head = (machine->queue_head + 1u) %
                          T64_EVENT_QUEUE_CAPACITY;
    machine->queue_count--;
    machine->tick = event.tick;
    if (event.handler == NULL || event.handler(machine, &event) != T64_STATUS_OK)
      return false;
  }
  return machine->queue_count == 0u;
}

static size_t make_literal_commands(uint64_t *commands, bool rgba32) {
  static const uint64_t base[] = {
      UINT64_C(0x3f10000700005000),
      UINT64_C(0x2d00000000020020),
      UINT64_C(0xef1000ff0a7041c8),
      UINT64_C(0xfa000000c86428ff),
      UINT64_C(0xf8000000143c64ff),
      UINT64_C(0xf9000000000000ff),
      UINT64_C(0xfc00000000018600),
      UINT64_C(0x0c000014000c0004),
      UINT64_C(0x00010000fffe8000),
      UINT64_C(0x0001000000000000),
      UINT64_C(0x0004000000018000),
      UINT64_C(0x0000000000000080),
      UINT64_C(0x0000000000000000),
      UINT64_C(0x0000000000000000),
      UINT64_C(0x0000000000000000),
      UINT64_C(0x0000000000000000),
      UINT64_C(0x0000000000000000),
      UINT64_C(0x0000000000000000),
      UINT64_C(0x0000000000000000),
  };
  (void)memcpy(commands, base, sizeof base);
  if (rgba32) commands[0] = UINT64_C(0x3f18000700005000);
  return sizeof base / sizeof base[0];
}

static uint64_t blender_modes(unsigned cycles, unsigned p0, unsigned a0,
                              unsigned m0, unsigned b0, unsigned p1,
                              unsigned a1, unsigned m1, unsigned b1,
                              bool force, bool antialias, bool image_read,
                              bool alpha_compare, bool alpha_dither,
                              bool alpha_cvg_select, bool cvg_times_alpha,
                              unsigned coverage_destination,
                              bool color_on_coverage) {
  uint64_t word = ((uint64_t)cycles & 3u) << 52;
  word |= UINT64_C(3) << 38; /* RGB dither disabled */
  word |= UINT64_C(3) << 36; /* alpha dither disabled */
  word |= UINT64_C(0xf) << 32;
  word |= ((uint64_t)p0 & 3u) << 30;
  word |= ((uint64_t)a0 & 3u) << 26;
  word |= ((uint64_t)m0 & 3u) << 22;
  word |= ((uint64_t)b0 & 3u) << 18;
  word |= ((uint64_t)p1 & 3u) << 28;
  word |= ((uint64_t)a1 & 3u) << 24;
  word |= ((uint64_t)m1 & 3u) << 20;
  word |= ((uint64_t)b1 & 3u) << 16;
  word |= (uint64_t)force << 14;
  word |= (uint64_t)alpha_cvg_select << 13;
  word |= (uint64_t)cvg_times_alpha << 12;
  word |= ((uint64_t)coverage_destination & 3u) << 8;
  word |= (uint64_t)color_on_coverage << 7;
  word |= (uint64_t)image_read << 6;
  word |= (uint64_t)antialias << 3;
  word |= (uint64_t)alpha_dither << 1;
  word |= (uint64_t)alpha_compare;
  return word;
}

static void seed_blender_inputs(t64_machine *machine, uint32_t address,
                                uint8_t old_coverage) {
  machine->rdp.other_modes_valid = true;
  machine->rdp.blend_color = UINT32_C(0x14283cff);
  machine->rdp.fog_color = UINT32_C(0xdcb48c80);
  machine->rdp.random_state = UINT32_C(0x12345678);
  machine->ram[address] = 40u;
  machine->ram[address + 1u] = 80u;
  machine->ram[address + 2u] = 120u;
  machine->ram[address + 3u] = UINT8_C(0xe8); /* alpha5=8 */
  t64_rdp_memory_coverage_write(machine, address, 4u, old_coverage);
}

static bool blend_direct(t64_machine *machine, uint64_t modes,
                         uint32_t pixel_rgba, uint32_t address,
                         uint32_t size_bytes, uint8_t new_coverage,
                         uint32_t *rgba_out, uint8_t *coverage_out,
                         bool *write_color_out) {
  t64_rdp_combiner_inputs inputs = {{0u}, {0u}, {0u}, 0u};
  machine->rdp.other_modes = modes;
  inputs.shade[3] = 128u;
  return t64_rdp_blend_pixel(machine, &inputs, pixel_rgba, address,
                             size_bytes, new_coverage, rgba_out,
                             coverage_out, write_color_out);
}

static bool capture_raw_vi(t64_machine *machine, uint8_t *bytes,
                           size_t capacity, size_t *length_out,
                           uint32_t control);

static uint64_t image_command(uint32_t opcode, uint32_t format,
                              uint32_t size, uint32_t width,
                              uint32_t address) {
  return ((uint64_t)opcode << 56) | ((uint64_t)format << 53) |
         ((uint64_t)size << 51) | ((uint64_t)(width - 1u) << 32) | address;
}

static uint64_t tile_command(uint32_t size, uint32_t line_words,
                             uint32_t tmem_word) {
  return (UINT64_C(0x35) << 56) | ((uint64_t)size << 51) |
         ((uint64_t)line_words << 41) | ((uint64_t)tmem_word << 32);
}

static uint64_t tile_bounds_command(uint32_t opcode, uint32_t width,
                                    uint32_t height) {
  return ((uint64_t)opcode << 56) |
         ((uint64_t)((width - 1u) * 4u) << 12) |
         (uint64_t)((height - 1u) * 4u);
}

static void case_blender_selector_muxes_both_cycles(void) {
  static const uint8_t p0_rgb[4][3] = {
      {120u, 90u, 80u}, {40u, 80u, 120u}, {30u, 60u, 90u},
      {130u, 130u, 130u}};
  static const uint8_t a0_rgb[4][3] = {
      {195u, 99u, 42u}, {120u, 90u, 80u}, {120u, 90u, 80u},
      {40u, 80u, 120u}};
  static const uint8_t m0_rgb[4][3] = {
      {200u, 100u, 40u}, {120u, 90u, 80u}, {110u, 70u, 50u},
      {210u, 140u, 90u}};
  static const uint8_t b0_rgb[4][3] = {
      {120u, 90u, 80u}, {146u, 93u, 66u}, {93u, 86u, 93u},
      {200u, 100u, 40u}};
  static const uint8_t p1_rgb[4][3] = {
      {200u, 100u, 40u}, {40u, 80u, 120u}, {20u, 40u, 60u},
      {220u, 180u, 140u}};
  static const uint8_t a1_rgb[4][3] = {
      {200u, 102u, 43u}, {210u, 140u, 90u}, {210u, 140u, 90u},
      {220u, 180u, 140u}};
  static const uint8_t m1_rgb[4][3] = {
      {200u, 100u, 40u}, {120u, 90u, 80u}, {110u, 70u, 50u},
      {210u, 140u, 90u}};
  static const uint8_t b1_rgb[4][3] = {
      {210u, 140u, 90u}, {206u, 126u, 73u}, {213u, 153u, 106u},
      {200u, 100u, 40u}};
  t64_machine *machine = create_authored_machine();
  uint32_t rgba = 0u;
  uint8_t coverage = 0u;
  bool write_color = false;
  unsigned selector, channel;
  const uint32_t address = UINT32_C(0x6000);

  if (machine == NULL) return;
  seed_blender_inputs(machine, address, 0u);
  for (selector = 0u; selector < 4u; selector++) {
    uint64_t modes = blender_modes(0u, selector, 2u, 1u, 0u, 0u, 2u,
                                  1u, 0u, true, false, true, false, false,
                                  false, false, 0u, false);
    T64_CHECK(blend_direct(machine, modes, UINT32_C(0xc86428ff), address,
                           4u, 8u, &rgba, &coverage, &write_color));
    for (channel = 0u; channel < 3u; channel++)
      T64_CHECK_INT((rgba >> (24u - channel * 8u)) & 255u,
                    p0_rgb[selector][channel]);
  }
  for (selector = 0u; selector < 4u; selector++) {
    uint64_t modes = blender_modes(0u, 0u, selector, 1u, 0u, 0u, 2u,
                                  1u, 0u, true, false, true, false, false,
                                  false, false, 0u, false);
    T64_CHECK(blend_direct(machine, modes, UINT32_C(0xc86428ff), address,
                           4u, 8u, &rgba, &coverage, &write_color));
    for (channel = 0u; channel < 3u; channel++)
      T64_CHECK_INT((rgba >> (24u - channel * 8u)) & 255u,
                    a0_rgb[selector][channel]);
  }
  for (selector = 0u; selector < 4u; selector++) {
    uint64_t modes = blender_modes(0u, 0u, 2u, selector, 0u, 0u, 2u,
                                  1u, 0u, true, false, true, false, false,
                                  false, false, 0u, false);
    T64_CHECK(blend_direct(machine, modes, UINT32_C(0xc86428ff), address,
                           4u, 8u, &rgba, &coverage, &write_color));
    for (channel = 0u; channel < 3u; channel++)
      T64_CHECK_INT((rgba >> (24u - channel * 8u)) & 255u,
                    m0_rgb[selector][channel]);
  }
  for (selector = 0u; selector < 4u; selector++) {
    uint64_t modes = blender_modes(0u, 0u, 2u, 1u, selector, 0u, 2u,
                                  1u, 0u, true, false, true, false, false,
                                  false, false, 0u, false);
    T64_CHECK(blend_direct(machine, modes, UINT32_C(0xc86428ff), address,
                           4u, 8u, &rgba, &coverage, &write_color));
    for (channel = 0u; channel < 3u; channel++)
      T64_CHECK_INT((rgba >> (24u - channel * 8u)) & 255u,
                    b0_rgb[selector][channel]);
  }
  for (selector = 0u; selector < 4u; selector++) {
    uint64_t modes = blender_modes(1u, 0u, 0u, 1u, 3u, selector, 2u,
                                  3u, 3u, true, false, true, false, false,
                                  false, false, 0u, false);
    T64_CHECK(blend_direct(machine, modes, UINT32_C(0xc86428ff), address,
                           4u, 8u, &rgba, &coverage, &write_color));
    for (channel = 0u; channel < 3u; channel++)
      T64_CHECK_INT((rgba >> (24u - channel * 8u)) & 255u,
                    p1_rgb[selector][channel]);
  }
  for (selector = 0u; selector < 4u; selector++) {
    uint64_t modes = blender_modes(1u, 0u, 0u, 1u, 3u, 0u, selector,
                                  3u, 0u, true, false, true, false, false,
                                  false, false, 0u, false);
    T64_CHECK(blend_direct(machine, modes, UINT32_C(0xc86428ff), address,
                           4u, 8u, &rgba, &coverage, &write_color));
    for (channel = 0u; channel < 3u; channel++)
      T64_CHECK_INT((rgba >> (24u - channel * 8u)) & 255u,
                    a1_rgb[selector][channel]);
  }
  for (selector = 0u; selector < 4u; selector++) {
    uint64_t modes = blender_modes(1u, 0u, 0u, 1u, 3u, 0u, 2u,
                                  selector, 0u, true, false, true, false,
                                  false, false, false, 0u, false);
    T64_CHECK(blend_direct(machine, modes, UINT32_C(0xc86428ff), address,
                           4u, 8u, &rgba, &coverage, &write_color));
    for (channel = 0u; channel < 3u; channel++)
      T64_CHECK_INT((rgba >> (24u - channel * 8u)) & 255u,
                    m1_rgb[selector][channel]);
  }
  for (selector = 0u; selector < 4u; selector++) {
    uint64_t modes = blender_modes(1u, 0u, 0u, 1u, 3u, 0u, 2u,
                                  3u, selector, true, false, true, false,
                                  false, false, false, 0u, false);
    T64_CHECK(blend_direct(machine, modes, UINT32_C(0xc86428ff), address,
                           4u, 8u, &rgba, &coverage, &write_color));
    for (channel = 0u; channel < 3u; channel++)
      T64_CHECK_INT((rgba >> (24u - channel * 8u)) & 255u,
                    b1_rgb[selector][channel]);
  }
  T64_CHECK_INT(coverage, 8u);
  T64_CHECK(write_color);
  t64_machine_destroy(machine);
}

static void case_alpha_threshold_and_blend_enable_modes(void) {
  t64_machine *machine = create_authored_machine();
  const uint32_t address = UINT32_C(0x6000);
  const uint32_t opaque = UINT32_C(0xc86428ff);
  uint64_t modes;
  uint64_t dither_modes;
  const uint32_t dither_pixel = UINT32_C(0x464b5a80);
  uint32_t rgba = 0u;
  uint8_t coverage = 0u;
  bool write_color = false;

  if (machine == NULL) return;
  seed_blender_inputs(machine, address, 6u);
  machine->rdp.blend_color = UINT32_C(0x00000064);
  modes = blender_modes(0u, 0u, 2u, 1u, 0u, 0u, 2u, 1u, 0u, false,
                        false, true, true, false, false, false, 0u, false);
  T64_CHECK(blend_direct(machine, modes, UINT32_C(0xc8642864), address, 4u,
                         3u, &rgba, &coverage, &write_color));
  T64_CHECK(write_color); /* alpha equals threshold */
  T64_CHECK_INT(coverage, 8u);
  T64_CHECK(blend_direct(machine, modes, UINT32_C(0xc8642863), address, 4u,
                         3u, &rgba, &coverage, &write_color));
  T64_CHECK(!write_color);
  T64_CHECK_INT(coverage, 8u); /* alpha rejection leaves coverage update */

  machine->rdp.random_state = UINT32_C(0x12340064);
  modes = blender_modes(0u, 0u, 2u, 1u, 0u, 0u, 2u, 1u, 0u, false,
                        false, true, true, true, false, false, 0u, false);
  T64_CHECK(blend_direct(machine, modes, UINT32_C(0xc8642864), address, 4u,
                         3u, &rgba, &coverage, &write_color));
  T64_CHECK(write_color);
  machine->rdp.random_state = UINT32_C(0x12340065);
  T64_CHECK(blend_direct(machine, modes, UINT32_C(0xc8642864), address, 4u,
                         3u, &rgba, &coverage, &write_color));
  T64_CHECK(!write_color);

  modes = blender_modes(0u, 0u, 2u, 1u, 0u, 0u, 2u, 1u, 0u, false,
                        true, true, false, false, false, false, 0u, false);
  T64_CHECK(blend_direct(machine, modes, opaque, address, 4u, 4u, &rgba,
                         &coverage, &write_color));
  T64_CHECK_INT(rgba >> 8, UINT32_C(0x785a50));
  T64_CHECK_INT(coverage, 8u);
  T64_CHECK(blend_direct(machine, modes, opaque, address, 4u, 8u, &rgba,
                         &coverage, &write_color));
  T64_CHECK_INT(rgba >> 8, UINT32_C(0xc86428)); /* full coverage bypass */
  modes = blender_modes(0u, 0u, 2u, 1u, 0u, 0u, 2u, 1u, 0u, true,
                        false, true, false, false, false, false, 0u, false);
  T64_CHECK(blend_direct(machine, modes, opaque, address, 4u, 8u, &rgba,
                         &coverage, &write_color));
  T64_CHECK_INT(rgba >> 8, UINT32_C(0x785a50)); /* force even at full */

  modes = blender_modes(0u, 0u, 2u, 1u, 0u, 0u, 2u, 1u, 0u, true,
                        false, false, false, false, false, false, 0u, false);
  T64_CHECK(blend_direct(machine, modes, opaque, address, 4u, 4u, &rgba,
                         &coverage, &write_color));
  T64_CHECK_INT(rgba >> 8, UINT32_C(0x643214)); /* image read is disabled */
  T64_CHECK_INT(coverage, 4u);

  dither_modes = blender_modes(0u, 0u, 2u, 1u, 0u, 0u, 2u, 1u, 0u,
                               true, false, true, false, false, false,
                               false, 0u, false);
  machine->rdp.random_state = UINT32_C(0x1234567f);
  dither_modes &= ~((UINT64_C(3) << 38) | (UINT64_C(3) << 36));
  machine->rdp.other_modes = dither_modes;
  T64_CHECK_INT(t64_rdp_quantize_rgba5551_v6(machine, dither_pixel, 3u, 2u),
                UINT16_C(0x4a98)); /* magic RGB + alpha pattern */
  T64_CHECK_INT(t64_rdp_quantize_alpha5_v6(machine, dither_pixel, 3u, 2u),
                16u);
  machine->rdp.other_modes = dither_modes | (UINT64_C(1) << 36);
  T64_CHECK_INT(t64_rdp_quantize_rgba5551_v6(machine, dither_pixel, 3u, 2u),
                UINT16_C(0x4a99)); /* inverse alpha pattern */
  T64_CHECK_INT(t64_rdp_quantize_alpha5_v6(machine, dither_pixel, 3u, 2u),
                15u);
  machine->rdp.other_modes =
      dither_modes | (UINT64_C(2) << 38) | (UINT64_C(2) << 36);
  T64_CHECK_INT(t64_rdp_quantize_rgba5551_v6(machine, dither_pixel, 3u, 2u),
                UINT16_C(0x4a98)); /* xorshift low-three-bit dither */
  T64_CHECK_INT(t64_rdp_quantize_alpha5_v6(machine, dither_pixel, 3u, 2u),
                16u);
  machine->rdp.other_modes = dither_modes | (UINT64_C(3) << 36);
  T64_CHECK_INT(t64_rdp_quantize_rgba5551_v6(machine, dither_pixel, 3u, 2u),
                UINT16_C(0x4a99)); /* alpha dither disabled */
  T64_CHECK_INT(t64_rdp_quantize_alpha5_v6(machine, dither_pixel, 3u, 2u),
                16u);
  machine->rdp.other_modes =
      (dither_modes & ~(UINT64_C(3) << 38)) | (UINT64_C(1) << 38) |
      (UINT64_C(3) << 36);
  T64_CHECK_INT(t64_rdp_quantize_rgba5551_v6(machine, dither_pixel, 3u, 2u),
                UINT16_C(0x4a57)); /* Bayer RGB selection */
  t64_machine_destroy(machine);
}

static void case_coverage_alpha_and_destination_modes(void) {
  t64_machine *machine = create_authored_machine();
  const uint32_t address = UINT32_C(0x6000);
  const uint32_t pixel = UINT32_C(0xc8642880);
  uint64_t modes;
  uint32_t rgba = 0u;
  uint8_t coverage = 0u;
  bool write_color = false;
  unsigned destination;

  if (machine == NULL) return;
  for (destination = 0u; destination < 4u; destination++) {
    seed_blender_inputs(machine, address, 6u);
    modes = blender_modes(0u, 0u, 2u, 1u, 0u, 0u, 2u, 1u, 0u, true,
                          false, true, false, false, false, false,
                          destination, false);
    T64_CHECK(blend_direct(machine, modes, pixel, address, 4u, 3u, &rgba,
                           &coverage, &write_color));
    T64_CHECK_INT(coverage, destination == 1u ? 1u :
                               destination == 3u ? 6u : 8u);
    T64_CHECK(write_color);
  }
  seed_blender_inputs(machine, address, 5u);
  modes = blender_modes(0u, 0u, 2u, 1u, 0u, 0u, 2u, 1u, 0u, true,
                        false, true, false, false, false, false, 1u, true);
  T64_CHECK(blend_direct(machine, modes, pixel, address, 4u, 3u, &rgba,
                         &coverage, &write_color));
  T64_CHECK_INT(coverage, 8u); /* exact unity is not overflow */
  T64_CHECK(!write_color);
  seed_blender_inputs(machine, address, 6u);
  T64_CHECK(blend_direct(machine, modes, pixel, address, 4u, 3u, &rgba,
                         &coverage, &write_color));
  T64_CHECK_INT(coverage, 1u); /* nine wraps to one and permits color */
  T64_CHECK(write_color);
  T64_CHECK(blend_direct(machine, modes, pixel, address, 4u, 0u, &rgba,
                         &coverage, &write_color));
  T64_CHECK_INT(coverage, 6u);
  T64_CHECK(!write_color);

  seed_blender_inputs(machine, address, 1u);
  modes = blender_modes(0u, 0u, 0u, 1u, 0u, 0u, 0u, 1u, 0u, true,
                        false, true, false, false, false, true, 0u, false);
  T64_CHECK(blend_direct(machine, modes, pixel, address, 4u, 3u, &rgba,
                         &coverage, &write_color));
  T64_CHECK_INT(coverage, 3u); /* 3 * 128 rounds to coverage 2 */
  T64_CHECK_INT(rgba >> 8, UINT32_C(0x785a50));
  modes = blender_modes(0u, 0u, 0u, 1u, 0u, 0u, 0u, 1u, 0u, true,
                        false, true, false, false, true, true, 0u, false);
  T64_CHECK(blend_direct(machine, modes, pixel, address, 4u, 3u, &rgba,
                         &coverage, &write_color));
  T64_CHECK_INT(coverage, 3u);
  T64_CHECK_INT(rgba >> 8, UINT32_C(0x505564));
  modes = blender_modes(0u, 0u, 0u, 1u, 0u, 0u, 0u, 1u, 0u, true,
                        false, true, false, false, true, false, 0u, false);
  T64_CHECK(blend_direct(machine, modes, pixel, address, 4u, 3u, &rgba,
                         &coverage, &write_color));
  T64_CHECK_INT(coverage, 4u);
  T64_CHECK_INT(rgba >> 8, UINT32_C(0x64575a));
  t64_machine_destroy(machine);
}

static void case_fetched_overlap_and_coverage_sidecar_edges(void) {
  uint64_t commands[19];
  const uint32_t target =
      FRAMEBUFFER_ADDRESS + (2u * 8u + 3u) * 2u;
  uint8_t raw_vi[8u * 8u * 4u];
  size_t raw_length = 0u;
  t64_machine *machine = create_authored_machine();
  uint32_t coverage_first = UINT32_C(0x6000);
  uint32_t coverage_last;
  uint8_t before[ T64_DIGEST_SIZE ];
  uint8_t after[ T64_DIGEST_SIZE ];
  uint8_t other_before[ T64_DIGEST_SIZE ];
  uint8_t other_after[ T64_DIGEST_SIZE ];
  uint8_t rom[TEST_ROM_CAPACITY];
  size_t rom_length = 0u;
  t64_machine *other = create_authored_machine();
  const t64_profile_descriptor *profile = NULL;
  t64_profile_descriptor odd_profile;
  test_allocator_stats allocator_stats = {0u, 0u};
  const t64_host_allocator allocator = {test_allocate, test_release,
                                         &allocator_stats};
  t64_machine *odd = NULL;
  uint32_t custom_granule;

  if (machine == NULL || other == NULL) {
    if (machine != NULL) t64_machine_destroy(machine);
    if (other != NULL) t64_machine_destroy(other);
    return;
  }
  T64_CHECK_INT(make_literal_commands(commands, false), 19u);
  machine->ram[target] = UINT8_C(0x2a);
  machine->ram[target + 1u] = UINT8_C(0x9f);
  t64_rdp_memory_coverage_write(machine, target, 2u, 6u);
  other->ram[target] = UINT8_C(0x2a);
  other->ram[target + 1u] = UINT8_C(0x9f);
  t64_rdp_memory_coverage_write(other, target, 2u, 6u);
  T64_CHECK(run_commands(machine, commands, 19u));
  T64_CHECK(run_commands(other, commands, 19u));
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, target, 2u), 1u);
  T64_CHECK_INT(t64_rdp_memory_coverage_read(other, target, 2u), 1u);
  T64_CHECK_INT(machine->ram[target], UINT8_C(0x42));
  T64_CHECK_INT(machine->ram[target + 1u], UINT8_C(0x57));
  T64_CHECK(run_commands(machine, commands, 19u));
  T64_CHECK(run_commands(other, commands, 19u));
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, target, 2u), 4u);
  T64_CHECK_INT(t64_rdp_memory_coverage_read(other, target, 2u), 4u);
  T64_CHECK_INT(machine->ram[target], UINT8_C(0x42));
  T64_CHECK_INT(machine->ram[target + 1u], UINT8_C(0x57));
  T64_CHECK_INT(machine->rdp.triangle_count, 2u);
  T64_CHECK_INT(other->rdp.triangle_count, 2u);
  T64_CHECK(capture_raw_vi(machine, raw_vi, sizeof raw_vi, &raw_length, 2u));
  T64_CHECK_INT(raw_vi[((2u * 8u + 3u) * 4u)], 66u);
  T64_CHECK(capture_raw_vi(other, raw_vi, sizeof raw_vi, &raw_length, 2u));
  T64_CHECK_INT(raw_vi[((2u * 8u + 3u) * 4u)], 66u);

  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, coverage_first, 2u), 0u);
  t64_rdp_memory_coverage_write(machine, coverage_first, 2u, 7u);
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, coverage_first, 4u), 0u);
  t64_rdp_memory_coverage_write(machine, coverage_first + 2u, 2u, 3u);
  t64_rdp_memory_coverage_write(other, coverage_first, 2u, 7u);
  t64_rdp_memory_coverage_write(other, coverage_first + 2u, 2u, 3u);
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, coverage_first, 4u), 3u);
  T64_CHECK_INT(t64_machine_digest(machine, before), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(other, other_before), T64_STATUS_OK);
  T64_CHECK(memcmp(before, other_before, sizeof before) == 0);
  t64_rdp_memory_coverage_invalidate(machine, coverage_first + 2u, 1u);
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, coverage_first, 2u), 7u);
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, coverage_first, 4u), 0u);
  T64_CHECK_INT(t64_machine_digest(machine, after), T64_STATUS_OK);
  T64_CHECK(memcmp(before, after, sizeof before) != 0);
  T64_CHECK_INT(t64_machine_digest(other, other_after), T64_STATUS_OK);
  T64_CHECK(memcmp(other_before, other_after, sizeof other_before) == 0);
  t64_rdp_memory_coverage_invalidate(machine, coverage_first + 3u, 1u);
  t64_rdp_memory_coverage_invalidate(other, coverage_first + 2u, 2u);
  t64_rdp_memory_coverage_invalidate(machine, coverage_first + 1u, 1u);
  t64_rdp_memory_coverage_invalidate(other, coverage_first, 2u);
  T64_CHECK_INT(t64_machine_digest(machine, after), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(other, other_after), T64_STATUS_OK);
  T64_CHECK(memcmp(after, other_after, sizeof after) == 0);
  t64_rdp_memory_coverage_invalidate(machine, coverage_first + 1u, 1u);
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, coverage_first, 2u), 0u);
  t64_rdp_memory_coverage_write(machine, 0u, 2u, 8u);
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, 0u, 2u), 8u);
  coverage_last = machine->profile.rdram_bytes - 2u;
  t64_rdp_memory_coverage_write(machine, coverage_last, 2u, 5u);
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, coverage_last, 2u), 5u);
  t64_rdp_memory_coverage_write(machine, coverage_last, 4u, 7u);
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, coverage_last, 2u), 5u);
  t64_rdp_memory_coverage_invalidate(machine, coverage_last + 1u, 1u);
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, coverage_last, 2u), 0u);

  T64_CHECK_INT(t64_profile_lookup(BLENDER_PROFILE_ID, &profile),
                T64_STATUS_OK);
  T64_CHECK(profile != NULL);
  if (profile != NULL) {
    odd_profile = *profile;
    (void)memset(odd_profile.profile_id, 0, sizeof odd_profile.profile_id);
    (void)memcpy(odd_profile.profile_id, "test-blender-odd-span", 22u);
    odd_profile.rdram_bytes = UINT32_C(4097);
    T64_CHECK_INT(t64_machine_create(&allocator, &odd_profile, &odd),
                  T64_STATUS_OK);
    T64_CHECK(odd != NULL);
    if (odd != NULL) {
      custom_granule = odd_profile.rdram_bytes / 2u;
      T64_CHECK_INT(T64_RDP_MEMORY_COVERAGE_BYTES(odd_profile.rdram_bytes),
                    2049u);
      T64_CHECK_INT(odd->rdp_memory_coverage[custom_granule], 0u);
      odd->rdp_memory_coverage[custom_granule] = UINT8_C(0x83);
      t64_rdp_memory_coverage_invalidate(odd, odd_profile.rdram_bytes - 1u,
                                         1u);
      T64_CHECK_INT(odd->rdp_memory_coverage[custom_granule], 0u);
      t64_machine_destroy(odd);
      T64_CHECK(allocator_stats.allocations > 0u);
      T64_CHECK_INT(allocator_stats.releases, allocator_stats.allocations);
    }
  }
  T64_CHECK(read_test_rom(rom, sizeof rom, &rom_length));
  if (rom_length != 0u) {
    t64_rdp_memory_coverage_write(machine, coverage_first, 2u, 4u);
    T64_CHECK_INT(t64_machine_admit_rom(machine, rom, rom_length),
                  T64_STATUS_OK);
    T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, coverage_first, 2u),
                  0u);
  }
  t64_machine_destroy(other);
  t64_machine_destroy(machine);
}

static void case_cpu_pi_and_sp_write_ownership(void) {
  static const uint32_t pi_base = UINT32_C(0x04600000);
  static const uint32_t sp_base = UINT32_C(0x04040000);
  static const uint32_t dmem_base = UINT32_C(0x04000000);
  static const uint32_t cart_base = UINT32_C(0x10000000);
  t64_machine *machine = create_authored_machine();
  uint8_t cache_line[T64_VR4300_DCACHE_LINE_BYTES];
  uint8_t rom[TEST_ROM_CAPACITY];
  size_t rom_length = 0u;
  uint32_t i;
  t64_rsp_dma_step step;
  t64_rsp_dma_step stale;
  t64_rsp_dma_admission admission;
  t64_rsp_dma_step no_step = {T64_RSP_DMA_STEP_NONE, 0u, 0u, 0u,
                              0u, 0u, false, false};
  uint32_t address = UINT32_C(0x6100);

  if (machine == NULL) return;
  t64_rdp_memory_coverage_write(machine, address, 2u, 7u);
  T64_CHECK_INT(t64_machine_bus_write(machine, address, 1u, UINT32_C(0xaa)),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, address, 2u), 0u);

  address = UINT32_C(0x6200);
  t64_rdp_memory_coverage_write(machine, address, 2u, 7u);
  t64_rdp_memory_coverage_write(machine, address + 2u, 2u, 6u);
  t64_rdp_memory_coverage_write(machine, address + 4u, 2u, 5u);
  t64_rdp_memory_coverage_write(machine, address + 6u, 2u, 4u);
  T64_CHECK_INT(t64_bus_write_masked(machine, address, 4u, UINT8_C(0x01),
                                     UINT64_C(0x11223344)), T64_STATUS_OK);
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, address, 2u), 0u);
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, address + 2u, 2u), 6u);
  T64_CHECK_INT(t64_bus_write_masked(machine, address + 4u, 4u,
                                     UINT8_C(0x04), UINT64_C(0x55667788)),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, address + 4u, 2u), 5u);
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, address + 6u, 2u), 0u);
  t64_rdp_memory_coverage_write(machine, address + 0x40u, 2u, 4u);
  T64_CHECK_INT(t64_bus_write_masked(machine, address + 0x40u, 4u, 0u, 0u),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, address + 0x40u, 2u), 4u);

  for (i = 0u; i < sizeof cache_line; i++) cache_line[i] = (uint8_t)i;
  for (i = 0u; i < T64_VR4300_DCACHE_LINE_BYTES; i += 2u)
    t64_rdp_memory_coverage_write(machine, UINT32_C(0x6300) + i, 2u, 5u);
  t64_rdp_memory_coverage_write(machine, UINT32_C(0x6310), 2u, 7u);
  T64_CHECK_INT(t64_bus_writeback_cache_line(
                    machine, UINT32_C(0x6300), cache_line,
                    T64_VR4300_DCACHE_LINE_BYTES), T64_STATUS_OK);
  for (i = 0u; i < T64_VR4300_DCACHE_LINE_BYTES; i += 2u)
    T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, UINT32_C(0x6300) + i,
                                               2u), 0u);
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, UINT32_C(0x6310), 2u),
                7u);

  machine->queue_head = 0u;
  machine->queue_count = 0u;
  for (i = 0u; i < 8u; i++)
    T64_CHECK_INT(t64_machine_bus_write(machine, dmem_base + i, 1u,
                                        UINT32_C(0xa0) + i), T64_STATUS_OK);
  t64_rdp_memory_coverage_write(machine, UINT32_C(0x6400), 4u, 7u);
  T64_CHECK_INT(t64_machine_bus_write(machine, sp_base, 4u, 0u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, sp_base + 4u, 4u,
                                      UINT32_C(0x6400)), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, sp_base + 12u, 4u, 7u),
                T64_STATUS_OK);
  T64_CHECK(drain_events(machine));
  T64_CHECK_INT(machine->ram[UINT32_C(0x6400)], UINT8_C(0xa0));
  T64_CHECK_INT(machine->ram[UINT32_C(0x6407)], UINT8_C(0xa7));
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, UINT32_C(0x6400), 4u),
                0u);

  t64_rdp_memory_coverage_write(machine, UINT32_C(0x6500), 2u, 5u);
  T64_CHECK_INT(t64_machine_bus_write(machine, pi_base, 4u,
                                      UINT32_C(0x6500)), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, pi_base + 4u, 4u,
                                      cart_base + 0x100u), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, pi_base + 12u, 4u, 1u),
                T64_STATUS_OK);
  T64_CHECK(drain_events(machine));
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, UINT32_C(0x6500), 2u),
                0u);

  /* The functional RSP path uses admitted descriptor, preflight and commit. */
  for (i = 0u; i < 8u; i++)
    T64_CHECK_INT(t64_machine_bus_write(machine, dmem_base + 0x20u + i, 1u,
                                        UINT32_C(0xb0) + i), T64_STATUS_OK);
  machine->sp.mem_address = UINT32_C(0x20);
  machine->sp.dram_address = UINT32_C(0x6600);
  T64_CHECK_INT(t64_sp_rsp_dma_admission_preflight(
                    machine, false, 7u, UINT32_C(0x0c), &no_step,
                    &admission), T64_STATUS_OK);
  t64_sp_rsp_dma_admission_commit(machine, &admission);
  T64_CHECK(machine->sp.active.valid);
  t64_rdp_memory_coverage_write(machine, UINT32_C(0x6600), 4u, 6u);
  while (machine->rsp.dma_setup_remaining != 0u) {
    T64_CHECK_INT(t64_sp_rsp_dma_preflight(machine, false, false, &step),
                  T64_STATUS_OK);
    T64_CHECK_INT(step.kind, T64_RSP_DMA_STEP_SETUP);
    t64_sp_rsp_dma_commit(machine, &step);
  }
  T64_CHECK_INT(t64_sp_rsp_dma_preflight(machine, false, true, &step),
                T64_STATUS_BUS_DEVICE_REFUSED);
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, UINT32_C(0x6600), 4u),
                6u);
  T64_CHECK_INT(t64_sp_rsp_dma_preflight(machine, false, false, &step),
                T64_STATUS_OK);
  T64_CHECK_INT(step.kind, T64_RSP_DMA_STEP_BEAT);
  stale = step;
  stale.token++;
  t64_sp_rsp_dma_commit(machine, &stale);
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, UINT32_C(0x6600), 4u),
                6u);
  t64_sp_rsp_dma_commit(machine, &step);
  T64_CHECK_INT(machine->ram[UINT32_C(0x6600)], UINT8_C(0xb0));
  T64_CHECK_INT(machine->ram[UINT32_C(0x6607)], UINT8_C(0xb7));
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, UINT32_C(0x6600), 4u),
                0u);

  T64_CHECK(read_test_rom(rom, sizeof rom, &rom_length));
  t64_machine_destroy(machine);
}

static void case_fill_and_copy_author_full_coverage(void) {
  const uint64_t fill_commands[] = {
      UINT64_C(0x3f18000700005000),
      UINT64_C(0x2d00000000020020),
      UINT64_C(0xef3000ff00000000),
      UINT64_C(0xf700000012345678),
      UINT64_C(0x3600c00800000000),
  };
  const uint8_t source[] = {UINT8_C(0xf8), UINT8_C(0x01),
                             UINT8_C(0x07), UINT8_C(0xc1)};
  uint64_t copy_commands[11];
  t64_machine *machine = create_authored_machine();
  const uint32_t fill_address = FRAMEBUFFER_ADDRESS;
  const uint32_t copy_address = UINT32_C(0x7000);
  size_t count = 0u;

  if (machine == NULL) return;
  T64_CHECK(run_commands(machine, fill_commands,
                         sizeof fill_commands / sizeof fill_commands[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT(machine->rdp.fill_count, 1u);
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, fill_address, 4u), 8u);
  T64_CHECK_INT(machine->ram[fill_address], UINT8_C(0x12));
  T64_CHECK_INT(machine->ram[fill_address + 1u], UINT8_C(0x34));
  T64_CHECK_INT(machine->ram[fill_address + 2u], UINT8_C(0x56));
  T64_CHECK_INT(machine->ram[fill_address + 3u], UINT8_C(0xf8));

  (void)memcpy(machine->ram + UINT32_C(0x4000), source, sizeof source);
  copy_commands[count++] = image_command(0x3fu, 0u, 3u, 2u, copy_address);
  copy_commands[count++] = UINT64_C(0x2d00000000008004);
  copy_commands[count++] = UINT64_C(0xef2000ff00000000);
  copy_commands[count++] = image_command(0x3du, 0u, 2u, 2u,
                                         UINT32_C(0x4000));
  copy_commands[count++] = tile_command(2u, 1u, 0u);
  copy_commands[count++] = tile_bounds_command(0x34u, 2u, 1u);
  copy_commands[count++] = tile_bounds_command(0x32u, 2u, 1u);
  copy_commands[count++] = UINT64_C(0x2400000000004000);
  copy_commands[count++] = UINT64_C(0x0000000010000400);
  copy_commands[count++] = UINT64_C(0x2900000000000000);
  T64_CHECK_INT(count, sizeof copy_commands / sizeof copy_commands[0] - 1u);
  T64_CHECK(run_commands(machine, copy_commands, count));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT(machine->rdp.copy_count, 1u);
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, copy_address, 4u), 8u);
  T64_CHECK_INT(machine->ram[copy_address], UINT8_C(0xff));
  T64_CHECK_INT(machine->ram[copy_address + 1u], 0u);
  T64_CHECK_INT(machine->ram[copy_address + 2u], 0u);
  T64_CHECK_INT(machine->ram[copy_address + 3u], UINT8_C(0xff));
  t64_machine_destroy(machine);
}

static bool capture_raw_vi(t64_machine *machine, uint8_t *bytes,
                           size_t capacity, size_t *length_out,
                           uint32_t control) {
  const struct {
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
  T64_CHECK(info.packet_bytes <= capacity);
  if (info.packet_bytes > capacity) return false;
  T64_CHECK_INT(t64_video_copy(machine, bytes, capacity, length_out),
                T64_STATUS_OK);
  return true;
}

static void case_fetched_literal_blender_to_raw_vi(void) {
  uint64_t commands[19];
  const size_t target = FRAMEBUFFER_ADDRESS + (2u * 8u + 3u) * 2u;
  const size_t vi_offset = (2u * 8u + 3u) * 4u;
  uint8_t raw_vi[8u * 8u * 4u];
  size_t raw_length = 0u;
  t64_machine *machine = create_authored_machine();

  if (machine == NULL) return;
  T64_CHECK_INT(make_literal_commands(commands, false), 19u);
  /* RGBA5551 approximates the literal memory RGB as (41,82,123); the literal
   * factors make the independently calculated final RGB remain (70,75,90). */
  machine->ram[target] = UINT8_C(0x2a);
  machine->ram[target + 1u] = UINT8_C(0x9f);
  t64_rdp_memory_coverage_write(machine, (uint32_t)target, 2u, 6u);
  T64_CHECK(run_commands(machine, commands, 19u));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT(machine->rdp.triangle_count, 1u);
  T64_CHECK_INT(machine->rdp.last_opcode_field, UINT32_C(0x0c));
  T64_CHECK(machine->rdp.last_original_word == commands[7]);
  T64_CHECK_INT(population16(machine->rdp_triangle_coverage[
                                 2u * T64_RDP_TRIANGLE_COVERAGE_WIDTH + 3u]),
                5u);
  T64_CHECK_INT((machine->rdp.other_modes >> 8) & UINT64_C(3), 1u);
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, (uint32_t)target, 2u),
                1u);
  T64_CHECK(capture_raw_vi(machine, raw_vi, sizeof raw_vi, &raw_length, 2u));
  T64_CHECK_INT(raw_length, sizeof raw_vi);
  T64_CHECK_INT(machine->ram[target], UINT8_C(0x42));
  T64_CHECK_INT(machine->ram[target + 1u], UINT8_C(0x57));
  T64_CHECK_INT(raw_vi[vi_offset], 66u);
  T64_CHECK_INT(raw_vi[vi_offset + 1u], 74u);
  T64_CHECK_INT(raw_vi[vi_offset + 2u], 90u);
  T64_CHECK_INT(raw_vi[vi_offset + 3u], 255u);
  t64_machine_destroy(machine);
}

static void case_rgba8888_alias_and_raw_vi(void) {
  uint64_t commands[19];
  const size_t target = FRAMEBUFFER_ADDRESS + (2u * 8u + 3u) * 4u;
  const size_t vi_offset = (2u * 8u + 3u) * 4u;
  uint8_t raw_vi[8u * 8u * 4u];
  size_t raw_length = 0u;
  t64_machine *machine = create_authored_machine();

  if (machine == NULL) return;
  T64_CHECK_INT(make_literal_commands(commands, true), 19u);
  machine->ram[target] = 40u;
  machine->ram[target + 1u] = 80u;
  machine->ram[target + 2u] = 120u;
  machine->ram[target + 3u] = UINT8_C(0xff);
  t64_rdp_memory_coverage_write(machine, (uint32_t)target, 4u, 6u);
  T64_CHECK(run_commands(machine, commands, 19u));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, (uint32_t)target, 4u),
                1u);
  T64_CHECK_INT(machine->ram[target], 70u);
  T64_CHECK_INT(machine->ram[target + 1u], 75u);
  T64_CHECK_INT(machine->ram[target + 2u], 90u);
  T64_CHECK_INT(machine->ram[target + 3u] >> 5, 0u);
  T64_CHECK(capture_raw_vi(machine, raw_vi, sizeof raw_vi, &raw_length, 3u));
  T64_CHECK_INT(raw_vi[vi_offset], 70u);
  T64_CHECK_INT(raw_vi[vi_offset + 1u], 75u);
  T64_CHECK_INT(raw_vi[vi_offset + 2u], 90u);
  T64_CHECK_INT(raw_vi[vi_offset + 3u], 255u);
  t64_machine_destroy(machine);
}

static void case_fetched_dither_quantization_to_raw_vi(void) {
  uint64_t commands[19];
  const size_t target = FRAMEBUFFER_ADDRESS + (2u * 8u + 3u) * 2u;
  const size_t vi_offset = (2u * 8u + 3u) * 4u;
  uint8_t raw_vi[8u * 8u * 4u];
  size_t raw_length = 0u;
  t64_machine *machine = create_authored_machine();

  if (machine == NULL) return;
  T64_CHECK_INT(make_literal_commands(commands, false), 19u);
  commands[2] &= ~((UINT64_C(3) << 38) | (UINT64_C(3) << 36));
  machine->ram[target] = UINT8_C(0x2a);
  machine->ram[target + 1u] = UINT8_C(0x9f);
  t64_rdp_memory_coverage_write(machine, (uint32_t)target, 2u, 6u);
  T64_CHECK(run_commands(machine, commands, 19u));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, (uint32_t)target, 2u),
                1u);
  T64_CHECK_INT(machine->ram[target], UINT8_C(0x4a));
  T64_CHECK_INT(machine->ram[target + 1u], UINT8_C(0x99));
  T64_CHECK(capture_raw_vi(machine, raw_vi, sizeof raw_vi, &raw_length, 2u));
  T64_CHECK_INT(raw_vi[vi_offset], 74u);
  T64_CHECK_INT(raw_vi[vi_offset + 1u], 82u);
  T64_CHECK_INT(raw_vi[vi_offset + 2u], 99u);
  T64_CHECK_INT(raw_vi[vi_offset + 3u], 255u);
  t64_machine_destroy(machine);
}

typedef struct {
  const char *name;
  void (*run)(void);
} test_case;

static const test_case cases[] = {
    {"blender-fetched-literal-packet-to-raw-vi",
     case_fetched_literal_blender_to_raw_vi},
    {"blender-rgba8888-alias-to-raw-vi",
     case_rgba8888_alias_and_raw_vi},
    {"blender-all-selectors-both-cycles",
     case_blender_selector_muxes_both_cycles},
    {"blender-alpha-threshold-and-blend-enable",
     case_alpha_threshold_and_blend_enable_modes},
    {"blender-fetched-dither-quantization-to-raw-vi",
     case_fetched_dither_quantization_to_raw_vi},
    {"blender-coverage-alpha-and-destinations",
     case_coverage_alpha_and_destination_modes},
    {"blender-fetched-overlap-and-sidecar-edges",
     case_fetched_overlap_and_coverage_sidecar_edges},
    {"blender-cpu-pi-sp-write-ownership",
     case_cpu_pi_and_sp_write_ownership},
    {"blender-fill-copy-full-coverage",
     case_fill_and_copy_author_full_coverage},
};

int main(int argc, char **argv) {
  size_t i;
  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) puts(cases[i].name);
    return 0;
  }
  if (argc != 1) return 2;
  printf("TAP version 13\n1..%lu\n", (unsigned long)(sizeof cases /
                                                      sizeof cases[0]));
  for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
    unsigned failures_before = test_failures;
    cases[i].run();
    printf("%s %lu - %s\n", test_failures == failures_before ? "ok" : "not ok",
           (unsigned long)(i + 1u), cases[i].name);
  }
  return test_failures == 0u && test_checks > 0u ? 0 : 1;
}
