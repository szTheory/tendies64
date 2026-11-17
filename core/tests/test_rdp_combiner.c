#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_internal.h"
#include "tendies64/t64_machine.h"

static unsigned test_checks;
static unsigned test_failures;

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

#define COMBINER_PROFILE_ID \
  "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-combiner-v5"
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
  T64_CHECK_INT(t64_profile_lookup(COMBINER_PROFILE_ID, &profile),
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
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_START, 4u, COMMAND_ADDRESS),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_END, 4u, end),
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
  t64_video_packet_info info = {0};
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

static bool run_one_cycle_raw_dpc_vi(uint16_t *rdram_pixel_out,
                                    uint8_t vi_pixel_out[4]) {
  static const uint64_t commands[] = {
      UINT64_C(0x3f10000700005000), /* RGBA16 color image, width 8 */
      UINT64_C(0x2d00000000020020), /* scissor [0,8) x [0,8) */
      UINT64_C(0x2f00000000000000), /* explicitly select one-cycle mode */
      UINT64_C(0x3a000000c8c8c8ff), /* primitive RGBA=(200,200,200,255) */
      UINT64_C(0x3b000000505050ff), /* environment RGBA=(80,80,80,255) */
      UINT64_C(0xfc55566c8877ffff), /* independent SetCombine literal */
      UINT64_C(0x08000014000c0004), /* Edge: (1,1), (4,3), (1,5) */
      UINT64_C(0x00010000fffe8000),
      UINT64_C(0x0001000000000000),
      UINT64_C(0x0004000000018000),
  };
  uint8_t raw_vi[8u * 8u * 4u];
  size_t raw_vi_length = 0u;
  const uint64_t combine = UINT64_C(0xfc55566c8877ffff);
  t64_machine *machine = create_authored_machine();
  const size_t pixel_address = FRAMEBUFFER_ADDRESS + (2u * 8u + 1u) * 2u;
  const size_t vi_pixel_offset = (2u * 8u + 1u) * 4u;

  if (machine == NULL) return false;
  memset(machine->ram + FRAMEBUFFER_ADDRESS, 0xa5, 8u * 8u * 2u);
  T64_CHECK_INT((combine >> 56) & UINT64_C(0x3f), UINT64_C(0x3c));
  T64_CHECK(combine == commands[5]);
  T64_CHECK(run_commands(machine, commands,
                         sizeof commands / sizeof commands[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT(machine->rdp.last_opcode_field, UINT32_C(0x08));
  T64_CHECK(machine->rdp.last_original_word == commands[6]);
  T64_CHECK_INT(machine->rdp.triangle_count, 1u);
  if (rdram_pixel_out != NULL)
    *rdram_pixel_out = (uint16_t)(((uint32_t)machine->ram[pixel_address] << 8) |
                                  machine->ram[pixel_address + 1u]);
  if (!capture_raw_vi(machine, raw_vi, sizeof raw_vi, &raw_vi_length)) {
    t64_machine_destroy(machine);
    return false;
  }
  T64_CHECK_INT(raw_vi_length, sizeof raw_vi);
  if (vi_pixel_out != NULL)
    memcpy(vi_pixel_out, raw_vi + vi_pixel_offset, 4u);
  t64_machine_destroy(machine);
  return true;
}

static void case_one_cycle_raw_dpc_vi(void) {
  uint16_t rdram_pixel = 0u;
  uint8_t vi_pixel[4] = {0u, 0u, 0u, 0u};
  T64_CHECK(run_one_cycle_raw_dpc_vi(&rdram_pixel, vi_pixel));
  /* RGBA5551 stores RGB5=10; VI expands that field to 82 in RGBA8. */
  T64_CHECK_INT(rdram_pixel, UINT16_C(0x5295));
  T64_CHECK_INT(vi_pixel[0], 82u);
  T64_CHECK_INT(vi_pixel[1], 82u);
  T64_CHECK_INT(vi_pixel[2], 82u);
  T64_CHECK_INT(vi_pixel[3], 255u);
}

static uint64_t cycle0_mode(unsigned rgb_a, unsigned rgb_b, unsigned rgb_c,
                            unsigned rgb_d, unsigned alpha_a,
                            unsigned alpha_b, unsigned alpha_c,
                            unsigned alpha_d);
static void initialize_pipeline_inputs(t64_machine *machine,
                                       t64_rdp_combiner_inputs *inputs);

static void case_literal_wrong_output_control(void) {
  uint16_t rdram_pixel = 0u;
  uint8_t vi_pixel[4] = {0u, 0u, 0u, 0u};
  /* The wrong cycle-1/old primitive path is independently 0xCE73 / RGB 206. */
  T64_CHECK(run_one_cycle_raw_dpc_vi(&rdram_pixel, vi_pixel));
  T64_CHECK_INT(rdram_pixel, UINT16_C(0x5295));
  T64_CHECK(rdram_pixel != UINT16_C(0xce73));
  T64_CHECK_INT(vi_pixel[0], 82u);
  T64_CHECK(vi_pixel[0] != 206u);
}

static void case_refusal_preserves_pixels_and_noise(void) {
  static const uint64_t commands[] = {
      UINT64_C(0x3f10000700005000), /* RGBA16 color image, width 8 */
      UINT64_C(0x2d00000000020020), /* scissor [0,8) x [0,8) */
      UINT64_C(0x2f20000000000000), /* copy cycle is refused for triangles */
      UINT64_C(0x3a000000c8c8c8ff),
      UINT64_C(0x3b000000505050ff),
      UINT64_C(0xfc55566c8877ffff),
      UINT64_C(0x08000014000c0004),
      UINT64_C(0x00010000fffe8000),
      UINT64_C(0x0001000000000000),
      UINT64_C(0x0004000000018000),
  };
  uint8_t before[8u * 8u * 2u];
  t64_machine *machine = create_authored_machine();
  if (machine == NULL) return;
  memset(machine->ram + FRAMEBUFFER_ADDRESS, 0xa5, sizeof before);
  memcpy(before, machine->ram + FRAMEBUFFER_ADDRESS, sizeof before);
  T64_CHECK(run_commands(machine, commands,
                        sizeof commands / sizeof commands[0]));
  T64_CHECK_INT(machine->rdp.last_opcode_field, UINT32_C(0x08));
  T64_CHECK(machine->rdp.last_original_word == commands[6]);
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_RENDER_RANGE);
  T64_CHECK_INT(machine->rdp.random_state, 1u);
  T64_CHECK(memcmp(before, machine->ram + FRAMEBUFFER_ADDRESS,
                   sizeof before) == 0);
  t64_machine_destroy(machine);
}

static void case_interleaved_instance_noise(void) {
  t64_rdp_combiner_inputs inputs = {{0u}, {0u}, {0u}, 0u};
  static const uint8_t expected[] = {33u, 1u, 197u, 79u};
  t64_machine *a = create_authored_machine();
  t64_machine *b = create_authored_machine();
  uint32_t rgba_a = 0u, rgba_b = 0u;
  unsigned i;
  if (a == NULL || b == NULL) {
    if (a != NULL) t64_machine_destroy(a);
    if (b != NULL) t64_machine_destroy(b);
    return;
  }
  initialize_pipeline_inputs(a, &inputs);
  initialize_pipeline_inputs(b, &inputs);
  inputs.texel0[3] = UINT8_MAX;
  a->rdp.combine_mode =
      cycle0_mode(7u, 0u, 8u, 0u, 7u, 7u, 7u, 7u);
  b->rdp.combine_mode = a->rdp.combine_mode;
  a->rdp.other_modes = UINT64_C(0xef00000000000000);
  b->rdp.other_modes = a->rdp.other_modes;
  a->rdp.random_state = 1u;
  b->rdp.random_state = 1u;
  for (i = 0u; i < sizeof expected; i++) {
    T64_CHECK(t64_rdp_combine_pixel(a, &inputs, &rgba_a));
    T64_CHECK_INT((rgba_a >> 24) & UINT32_C(0xff), expected[i]);
    T64_CHECK(t64_rdp_combine_pixel(b, &inputs, &rgba_b));
    T64_CHECK_INT(rgba_b, rgba_a);
    T64_CHECK_INT(b->rdp.random_state, a->rdp.random_state);
  }
  {
    uint8_t digest_a[T64_DIGEST_SIZE], digest_b[T64_DIGEST_SIZE];
    T64_CHECK_INT(t64_machine_digest(a, digest_a), T64_STATUS_OK);
    T64_CHECK_INT(t64_machine_digest(b, digest_b), T64_STATUS_OK);
    T64_CHECK(memcmp(digest_a, digest_b, sizeof digest_a) == 0);
  }
  t64_machine_destroy(a);
  t64_machine_destroy(b);
}

static void case_policy_identity_and_digest(void) {
  const t64_profile_descriptor *profile = NULL;
  t64_profile_descriptor altered;
  uint8_t base_hash[T64_DIGEST_SIZE], altered_hash[T64_DIGEST_SIZE];
  uint8_t digest_before[T64_DIGEST_SIZE], digest_after[T64_DIGEST_SIZE];
  uint32_t original_seed;
  t64_machine *machine = create_authored_machine();
  if (machine == NULL) return;
  T64_CHECK_INT(t64_profile_lookup(COMBINER_PROFILE_ID, &profile),
                T64_STATUS_OK);
  T64_CHECK(profile != NULL);
  if (profile != NULL) {
    altered = *profile;
    T64_CHECK_INT(t64_profile_hash(profile, base_hash), T64_STATUS_OK);
    (void)snprintf(altered.rdp_combiner_policy,
                   sizeof altered.rdp_combiner_policy,
                   "%s", "rgba8-floor256-clamp-feedback-noise1-v1");
    T64_CHECK_INT(t64_profile_hash(&altered, altered_hash), T64_STATUS_OK);
    T64_CHECK(memcmp(base_hash, altered_hash, sizeof base_hash) != 0);
    altered = *profile;
    (void)snprintf(altered.rdp_undefined_color_policy,
                   sizeof altered.rdp_undefined_color_policy,
                   "%s", "one-color-v1");
    T64_CHECK_INT(t64_profile_hash(&altered, altered_hash), T64_STATUS_OK);
    T64_CHECK(memcmp(base_hash, altered_hash, sizeof base_hash) != 0);
  }
  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);
  original_seed = machine->rdp.random_state;
  machine->rdp.random_state ^= UINT32_C(1);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) != 0);
  machine->rdp.random_state = original_seed;
  machine->rdp.combine_mode = UINT64_C(0xfc55566c8877ffff);
  machine->rdp.combine_mode_valid = true;
  machine->rdp.environment_color = UINT32_C(0x12345678);
  machine->rdp.environment_color_valid = true;
  machine->rdp.random_state = UINT32_C(0x00042021);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) != 0);
  t64_machine_destroy(machine);
}

static uint64_t cycle0_mode(unsigned rgb_a, unsigned rgb_b, unsigned rgb_c,
                            unsigned rgb_d, unsigned alpha_a,
                            unsigned alpha_b, unsigned alpha_c,
                            unsigned alpha_d) {
  return UINT64_C(0xfc00000000000000) |
         ((uint64_t)(rgb_a & 15u) << 52) |
         ((uint64_t)(rgb_c & 31u) << 47) |
         ((uint64_t)(alpha_a & 7u) << 44) |
         ((uint64_t)(alpha_c & 7u) << 41) |
         ((uint64_t)(rgb_b & 15u) << 28) |
         ((uint64_t)(rgb_d & 7u) << 15) |
         ((uint64_t)(alpha_b & 7u) << 12) |
         ((uint64_t)(alpha_d & 7u) << 9);
}

static void initialize_pipeline_inputs(t64_machine *machine,
                                       t64_rdp_combiner_inputs *inputs) {
  const uint8_t texel0[4] = {11u, 22u, 33u, 44u};
  const uint8_t texel1[4] = {55u, 66u, 77u, 88u};
  const uint8_t shade[4] = {143u, 154u, 165u, 176u};
  memcpy(inputs->texel0, texel0, sizeof texel0);
  memcpy(inputs->texel1, texel1, sizeof texel1);
  memcpy(inputs->shade, shade, sizeof shade);
  inputs->lod_fraction = 145u;
  machine->rdp.primitive_color = UINT32_C(0x636e7984);
  machine->rdp.primitive_lod_fraction = 167u;
  machine->rdp.environment_color = UINT32_C(0xbbc6d1dc);
  machine->rdp.key_center[0] = 12u;
  machine->rdp.key_center[1] = 23u;
  machine->rdp.key_center[2] = 34u;
  machine->rdp.key_scale[0] = 45u;
  machine->rdp.key_scale[1] = 56u;
  machine->rdp.key_scale[2] = 67u;
  machine->rdp.convert_coefficients[4] = UINT32_C(0x55);
  machine->rdp.convert_coefficients[5] = UINT32_C(0x7b);
  machine->rdp.other_modes = UINT64_C(0xef00000000000000);
  machine->rdp.random_state = 1u;
  machine->rdp.combine_mode_valid = true;
}

static bool run_pipeline(t64_machine *machine,
                         t64_rdp_combiner_inputs *inputs, uint64_t mode,
                         uint32_t cycle_type, uint32_t *rgba_out) {
  machine->rdp.combine_mode = mode;
  machine->rdp.other_modes = UINT64_C(0xef00000000000000) |
                             ((uint64_t)(cycle_type & 3u) << 52);
  machine->rdp.random_state = 1u;
  return t64_rdp_combine_pixel(machine, inputs, rgba_out);
}

static void case_two_cycle_feedback_literal(void) {
  t64_rdp_combiner_inputs inputs = {{0u}, {0u}, {0u}, 64u};
  t64_machine *machine = create_authored_machine();
  uint32_t rgba = 0u;
  if (machine == NULL) return;
  inputs.texel0[0] = 220u;
  inputs.texel0[1] = 100u;
  inputs.texel0[2] = 40u;
  inputs.texel0[3] = 200u;
  inputs.texel1[0] = 10u;
  inputs.texel1[1] = 200u;
  inputs.texel1[2] = 30u;
  inputs.texel1[3] = 128u;
  inputs.shade[0] = 128u;
  inputs.shade[1] = 64u;
  inputs.shade[2] = 192u;
  inputs.shade[3] = 100u;
  machine->rdp.primitive_color = UINT32_C(0x141e0a10);
  machine->rdp.primitive_lod_fraction = 64u;
  machine->rdp.environment_color = UINT32_C(0x0a324628);
  machine->rdp.combine_mode = UINT64_C(0xfc12140d321ad6cc);
  machine->rdp.combine_mode_valid = true;
  machine->rdp.other_modes = UINT64_C(0xef10000000000000);
  machine->rdp.random_state = 1u;
  T64_CHECK_INT((machine->rdp.combine_mode >> 56) & UINT64_C(0x3f),
                UINT64_C(0x3c));
  T64_CHECK(t64_rdp_combine_pixel(machine, &inputs, &rgba));
  T64_CHECK_INT(rgba, UINT32_C(0x2d001949));
  T64_CHECK_INT(machine->rdp.random_state, UINT32_C(0x00042021));
  t64_machine_destroy(machine);
}

static uint8_t expected_rgb_a(unsigned code, unsigned channel) {
  static const uint8_t sources[8][3] = {
      {0u, 0u, 0u}, {11u, 22u, 33u}, {55u, 66u, 77u},
      {99u, 110u, 121u}, {143u, 154u, 165u}, {187u, 198u, 209u},
      {255u, 255u, 255u}, {33u, 33u, 33u}};
  return sources[code][channel];
}

static uint8_t expected_rgb_b(unsigned code, unsigned channel) {
  static const uint8_t sources[16][3] = {
      {0u, 0u, 0u}, {11u, 22u, 33u}, {55u, 66u, 77u},
      {99u, 110u, 121u}, {143u, 154u, 165u}, {187u, 198u, 209u},
      {12u, 23u, 34u}, {85u, 85u, 85u}, {0u, 0u, 0u},
      {0u, 0u, 0u}, {0u, 0u, 0u}, {0u, 0u, 0u},
      {0u, 0u, 0u}, {0u, 0u, 0u}, {0u, 0u, 0u},
      {0u, 0u, 0u}};
  return sources[code][channel];
}

static uint8_t expected_rgb_c(unsigned code, unsigned channel) {
  static const uint8_t sources[32][3] = {
      {0u, 0u, 0u}, {11u, 22u, 33u}, {55u, 66u, 77u},
      {99u, 110u, 121u}, {143u, 154u, 165u}, {187u, 198u, 209u},
      {45u, 56u, 67u}, {0u, 0u, 0u}, {44u, 44u, 44u},
      {88u, 88u, 88u}, {132u, 132u, 132u}, {176u, 176u, 176u},
      {220u, 220u, 220u}, {145u, 145u, 145u}, {167u, 167u, 167u},
      {123u, 123u, 123u}, {0u, 0u, 0u}, {0u, 0u, 0u},
      {0u, 0u, 0u}, {0u, 0u, 0u}, {0u, 0u, 0u}, {0u, 0u, 0u},
      {0u, 0u, 0u}, {0u, 0u, 0u}, {0u, 0u, 0u}, {0u, 0u, 0u},
      {0u, 0u, 0u}, {0u, 0u, 0u}, {0u, 0u, 0u}, {0u, 0u, 0u},
      {0u, 0u, 0u}, {0u, 0u, 0u}};
  return sources[code][channel];
}

static uint8_t expected_rgb_d(unsigned code, unsigned channel) {
  static const uint8_t sources[8][3] = {
      {0u, 0u, 0u}, {11u, 22u, 33u}, {55u, 66u, 77u},
      {99u, 110u, 121u}, {143u, 154u, 165u}, {187u, 198u, 209u},
      {255u, 255u, 255u}, {0u, 0u, 0u}};
  return sources[code][channel];
}

static uint8_t expected_alpha(unsigned code) {
  static const uint8_t sources[8] = {0u, 44u, 88u, 132u,
                                     176u, 220u, 255u, 0u};
  return sources[code];
}

static uint8_t expected_alpha_c(unsigned code) {
  static const uint8_t sources[8] = {145u, 44u, 88u, 132u,
                                     176u, 220u, 167u, 0u};
  return sources[code];
}

static void case_rgb_selector_denominator(void) {
  t64_rdp_combiner_inputs inputs = {{0u}, {0u}, {0u}, 0u};
  t64_machine *machine = create_authored_machine();
  uint32_t rgba;
  unsigned code, channel;
  if (machine == NULL) return;
  initialize_pipeline_inputs(machine, &inputs);
  for (code = 0u; code < 8u; code++) {
    T64_CHECK(run_pipeline(machine, &inputs,
                           cycle0_mode(code, 0u, 8u, 0u, 7u, 7u, 7u, 7u),
                           0u, &rgba));
    for (channel = 0u; channel < 3u; channel++) {
      uint8_t expected = (uint8_t)(((uint32_t)expected_rgb_a(code, channel) *
                                    UINT32_C(44)) / UINT32_C(255));
      T64_CHECK_INT((rgba >> (24u - channel * 8u)) & UINT32_C(0xff),
                    expected);
    }
  }
  for (code = 8u; code < 16u; code++) {
    T64_CHECK(run_pipeline(machine, &inputs,
                           cycle0_mode(code, 0u, 8u, 0u, 7u, 7u, 7u, 7u),
                           0u, &rgba));
    for (channel = 0u; channel < 3u; channel++)
      T64_CHECK_INT((rgba >> (24u - channel * 8u)) & UINT32_C(0xff), 0u);
  }
  for (code = 0u; code < 16u; code++) {
    T64_CHECK(run_pipeline(machine, &inputs,
                           cycle0_mode(6u, code, 8u, 0u, 7u, 7u, 7u, 7u),
                           0u, &rgba));
    for (channel = 0u; channel < 3u; channel++) {
      uint8_t source = expected_rgb_b(code, channel);
      uint8_t expected = (uint8_t)(((UINT32_C(255) - source) * 44u) / 255u);
      T64_CHECK_INT((rgba >> (24u - channel * 8u)) & UINT32_C(0xff),
                    expected);
    }
  }
  for (code = 0u; code < 32u; code++) {
    T64_CHECK(run_pipeline(machine, &inputs,
                           cycle0_mode(6u, 0u, code, 0u, 7u, 7u, 7u, 7u),
                           0u, &rgba));
    for (channel = 0u; channel < 3u; channel++)
      T64_CHECK_INT((rgba >> (24u - channel * 8u)) & UINT32_C(0xff),
                    expected_rgb_c(code, channel));
  }
  for (code = 0u; code < 8u; code++) {
    T64_CHECK(run_pipeline(machine, &inputs,
                           cycle0_mode(0u, 0u, 8u, code, 7u, 7u, 7u, 7u),
                           0u, &rgba));
    for (channel = 0u; channel < 3u; channel++)
      T64_CHECK_INT((rgba >> (24u - channel * 8u)) & UINT32_C(0xff),
                    expected_rgb_d(code, channel));
  }
  t64_machine_destroy(machine);
}

static void case_alpha_selector_denominator(void) {
  t64_rdp_combiner_inputs inputs = {{0u}, {0u}, {0u}, 145u};
  t64_machine *machine = create_authored_machine();
  uint32_t rgba;
  unsigned code;
  if (machine == NULL) return;
  initialize_pipeline_inputs(machine, &inputs);
  for (code = 0u; code < 8u; code++) {
    T64_CHECK(run_pipeline(machine, &inputs,
                           cycle0_mode(0u, 0u, 8u, 0u, code, 7u, 1u, 7u),
                           0u, &rgba));
    T64_CHECK_INT(rgba & UINT32_C(0xff),
                  ((uint32_t)expected_alpha(code) * 44u) / 255u);
    T64_CHECK(run_pipeline(machine, &inputs,
                           cycle0_mode(0u, 0u, 8u, 0u, 6u, code, 1u, 7u),
                           0u, &rgba));
    T64_CHECK_INT(rgba & UINT32_C(0xff),
                  ((UINT32_C(255) - expected_alpha(code)) * 44u) / 255u);
    T64_CHECK(run_pipeline(machine, &inputs,
                           cycle0_mode(0u, 0u, 8u, 0u, 6u, 7u, code, 7u),
                           0u, &rgba));
    T64_CHECK_INT(rgba & UINT32_C(0xff), expected_alpha_c(code));
    T64_CHECK(run_pipeline(machine, &inputs,
                           cycle0_mode(0u, 0u, 8u, 0u, 7u, 7u, 1u, code),
                           0u, &rgba));
    T64_CHECK_INT(rgba & UINT32_C(0xff), expected_alpha(code));
  }
  t64_machine_destroy(machine);
}

static void case_signed_floor_clamp_and_saturation(void) {
  t64_rdp_combiner_inputs inputs = {{0u}, {0u}, {0u}, 0u};
  t64_machine *machine = create_authored_machine();
  uint32_t rgba;
  if (machine == NULL) return;
  initialize_pipeline_inputs(machine, &inputs);
  inputs.texel0[3] = 128u;
  inputs.texel0[0] = 1u;
  T64_CHECK(run_pipeline(machine, &inputs,
                         cycle0_mode(0u, 1u, 8u, 1u, 7u, 7u, 7u, 7u),
                         0u, &rgba));
  /* floor((-1*128)/255)+1 is zero; truncation toward zero would be one. */
  T64_CHECK_INT((rgba >> 24) & UINT32_C(0xff), 0u);
  T64_CHECK_INT(rgba & UINT32_C(0xff), 0u);
  inputs.texel0[3] = UINT8_MAX;
  T64_CHECK(run_pipeline(machine, &inputs,
                         cycle0_mode(6u, 0u, 8u, 6u, 6u, 7u, 1u, 6u),
                         0u, &rgba));
  T64_CHECK_INT((rgba >> 24) & UINT32_C(0xff), 255u);
  T64_CHECK_INT(rgba & UINT32_C(0xff), 255u);
  T64_CHECK(run_pipeline(machine, &inputs,
                         cycle0_mode(0u, 0u, 8u, 1u, 7u, 7u, 7u, 7u),
                         0u, &rgba));
  T64_CHECK_INT((rgba >> 24) & UINT32_C(0xff), 1u);
  T64_CHECK_INT(rgba & UINT32_C(0xff), 0u);
  t64_machine_destroy(machine);
}

static void case_key_convert_and_lod_sources(void) {
  static const uint64_t key_commands[] = {
      UINT64_C(0xfa00a700636e7984),
      UINT64_C(0xaa12345617382243),
      UINT64_C(0xab00000007890c2d),
  };
  t64_rdp_combiner_inputs inputs = {{0u}, {0u}, {0u}, 145u};
  t64_machine *machine = create_authored_machine();
  uint32_t rgba;
  if (machine == NULL) return;
  initialize_pipeline_inputs(machine, &inputs);
  T64_CHECK(run_commands(machine, key_commands,
                         sizeof key_commands / sizeof key_commands[0]));
  T64_CHECK_INT(machine->rdp.command_count, 3u);
  T64_CHECK_INT(machine->rdp.key_width[0], UINT32_C(0x789));
  T64_CHECK_INT(machine->rdp.key_width[1], UINT32_C(0x123));
  T64_CHECK_INT(machine->rdp.key_width[2], UINT32_C(0x456));
  T64_CHECK_INT(machine->rdp.key_center[0], 12u);
  T64_CHECK_INT(machine->rdp.key_center[1], 23u);
  T64_CHECK_INT(machine->rdp.key_center[2], 34u);
  T64_CHECK_INT(machine->rdp.key_scale[0], 45u);
  T64_CHECK_INT(machine->rdp.key_scale[1], 56u);
  T64_CHECK_INT(machine->rdp.key_scale[2], 67u);
  T64_CHECK_INT(machine->rdp.primitive_lod_fraction, 167u);
  T64_CHECK(run_pipeline(machine, &inputs,
                         cycle0_mode(6u, 6u, 13u, 0u, 6u, 7u, 6u, 0u),
                         0u, &rgba));
  T64_CHECK_INT((rgba >> 24) & UINT32_C(0xff), 138u);
  T64_CHECK_INT((rgba >> 16) & UINT32_C(0xff), 131u);
  T64_CHECK_INT((rgba >> 8) & UINT32_C(0xff), 125u);
  T64_CHECK_INT(rgba & UINT32_C(0xff), 167u);
  T64_CHECK(run_pipeline(machine, &inputs,
                         cycle0_mode(6u, 6u, 6u, 0u, 6u, 7u, 6u, 0u),
                         0u, &rgba));
  T64_CHECK_INT((rgba >> 24) & UINT32_C(0xff), 42u);
  T64_CHECK_INT((rgba >> 16) & UINT32_C(0xff), 50u);
  T64_CHECK_INT((rgba >> 8) & UINT32_C(0xff), 58u);
  T64_CHECK_INT(rgba & UINT32_C(0xff), 167u);
  T64_CHECK(run_pipeline(machine, &inputs,
                         cycle0_mode(6u, 6u, 14u, 0u, 6u, 7u, 6u, 0u),
                         0u, &rgba));
  T64_CHECK_INT((rgba >> 24) & UINT32_C(0xff), 159u);
  T64_CHECK_INT(rgba & UINT32_C(0xff), 167u);
  T64_CHECK(run_pipeline(machine, &inputs,
                         cycle0_mode(6u, 7u, 15u, 0u, 6u, 7u, 0u, 0u),
                         0u, &rgba));
  T64_CHECK_INT((rgba >> 24) & UINT32_C(0xff), 82u);
  machine->rdp.convert_coefficients[5] = UINT32_C(0xff);
  T64_CHECK(run_pipeline(machine, &inputs,
                         cycle0_mode(6u, 7u, 15u, 0u, 6u, 7u, 0u, 0u),
                         0u, &rgba));
  T64_CHECK_INT((rgba >> 24) & UINT32_C(0xff), 170u);
  t64_machine_destroy(machine);
}

static void case_noise_seed_and_cadence(void) {
  t64_rdp_combiner_inputs inputs = {{0u}, {0u}, {0u}, 0u};
  t64_machine *machine = create_authored_machine();
  uint32_t rgba;
  if (machine == NULL) return;
  initialize_pipeline_inputs(machine, &inputs);
  inputs.texel0[3] = UINT8_MAX;
  T64_CHECK(run_pipeline(machine, &inputs,
                         cycle0_mode(7u, 0u, 8u, 0u, 7u, 7u, 7u, 7u),
                         0u, &rgba));
  T64_CHECK_INT((rgba >> 24) & UINT32_C(0xff), 33u);
  T64_CHECK_INT(machine->rdp.random_state, UINT32_C(0x00042021));
  machine->rdp.random_state = 1u;
  T64_CHECK(run_pipeline(machine, &inputs,
                         cycle0_mode(1u, 0u, 8u, 0u, 7u, 7u, 7u, 7u),
                         1u, &rgba));
  T64_CHECK_INT(machine->rdp.random_state, UINT32_C(0x00042021));
  t64_machine_destroy(machine);
}

typedef struct {
  const char *name;
  void (*run)(void);
} test_case;

static const test_case cases[] = {
    {"combiner-one-cycle-raw-dpc-vi", case_one_cycle_raw_dpc_vi},
    {"combiner-literal-wrong-control", case_literal_wrong_output_control},
    {"combiner-two-cycle-feedback-literal", case_two_cycle_feedback_literal},
    {"combiner-rgb-selector-denominator", case_rgb_selector_denominator},
    {"combiner-alpha-selector-denominator", case_alpha_selector_denominator},
    {"combiner-signed-floor-clamp-and-saturation",
     case_signed_floor_clamp_and_saturation},
    {"combiner-key-convert-lod-sources", case_key_convert_and_lod_sources},
    {"combiner-noise-seed-and-cadence", case_noise_seed_and_cadence},
    {"combiner-noise-interleaved-instances", case_interleaved_instance_noise},
    {"combiner-refusal-noise-unchanged",
     case_refusal_preserves_pixels_and_noise},
    {"combiner-undefined-color-policy-digest",
     case_policy_identity_and_digest},
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
