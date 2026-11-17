#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_test.h"
#include "../t64_internal.h"
#include "tendies64/t64_machine.h"

#define TEST_ROM_CAPACITY 65536u
#define TEST_SOURCE_BYTES 24u
#define TEST_PROFILE_ID "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v2"
#define TEST_COMPLETION_TICK UINT64_C(600000)

static bool advance_guest_to_tick(t64_machine *machine, uint64_t tick,
                                  t64_stop_result *stop_out);
static bool advance_to_next_vi_field(t64_machine *machine);
static bool advance_until_video_full(t64_machine *machine, uint64_t tick,
                                     t64_stop_result *stop_out);

static const uint8_t expected_packet[] = {
    255, 0, 0, 255, 0, 255, 0, 255, 255, 0, 0, 255, 0, 255, 0, 255,
    255, 0, 0, 255, 0, 255, 0, 255, 255, 0, 0, 255, 0, 255, 0, 255,
    255, 0, 0, 255, 0, 255, 0, 255, 255, 0, 0, 255, 0, 255, 0, 255,
};

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

static const t64_host_allocator test_allocator = {test_allocate, test_release,
                                                   NULL};

static bool load_authored_rom(uint8_t *bytes, size_t capacity,
                              size_t *length_out) {
  const char *path = getenv("T64_TEST_RDP_FILL_PATH");
  FILE *file;
  long length;
  if (path == NULL || path[0] == '\0' || bytes == NULL || length_out == NULL)
    return false;
  file = fopen(path, "rb");
  if (file == NULL) return false;
  if (fseek(file, 0L, SEEK_END) != 0 || (length = ftell(file)) <= 0 ||
      (unsigned long)length > capacity || fseek(file, 0L, SEEK_SET) != 0) {
    (void)fclose(file);
    return false;
  }
  if (fread(bytes, 1u, (size_t)length, file) != (size_t)length ||
      fclose(file) != 0) {
    return false;
  }
  *length_out = (size_t)length;
  return true;
}

static t64_machine *create_authored_machine_for_profile(
    const char *profile_id) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t rom[TEST_ROM_CAPACITY];
  size_t rom_length = 0u;
  t64_status status;

  T64_CHECK(load_authored_rom(rom, sizeof rom, &rom_length));
  if (rom_length == 0u) return NULL;
  T64_CHECK_INT(t64_profile_lookup(profile_id, &profile), T64_STATUS_OK);
  if (profile == NULL) return NULL;
  T64_CHECK_INT(t64_machine_create(&test_allocator, profile, &machine),
                T64_STATUS_OK);
  if (machine == NULL) return NULL;
  status = t64_machine_admit_rom(machine, rom, rom_length);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return NULL;
  }
  return machine;
}

static t64_machine *create_authored_machine(void) {
  return create_authored_machine_for_profile(TEST_PROFILE_ID);
}

#define VI_BASE UINT32_C(0x04400000)
#define MI_INTR UINT32_C(0x04300008)
#define MI_INTR_MASK UINT32_C(0x0430000c)
#define VI_V8_PROFILE_ID \
  "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v8"
#define VI_FILTER_PROFILE_ID \
  "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9"

static t64_machine *create_vi_v8_machine(void) {
  return create_authored_machine_for_profile(VI_V8_PROFILE_ID);
}

static t64_machine *create_vi_filter_machine(void) {
  return create_authored_machine_for_profile(VI_FILTER_PROFILE_ID);
}

static bool vi_write(t64_machine *machine, uint32_t offset, uint32_t value) {
  t64_status status = t64_machine_bus_write(machine, VI_BASE + offset, 4u,
                                            value);
  T64_CHECK_INT(status, T64_STATUS_OK);
  return status == T64_STATUS_OK;
}

static bool seed_vi_source_at(t64_machine *machine, uint32_t origin) {
  static const uint16_t pixels[12] = {
      UINT16_C(0xf801), UINT16_C(0x07c1), UINT16_C(0x003f), UINT16_C(0xffff),
      UINT16_C(0x07c1), UINT16_C(0xf801), UINT16_C(0xffff), UINT16_C(0x003f),
      UINT16_C(0x003f), UINT16_C(0xffff), UINT16_C(0xf801), UINT16_C(0x07c1),
  };
  size_t i;

  if (machine == NULL) return false;
  for (i = 0u; i < sizeof pixels / sizeof pixels[0]; i++) {
    t64_status status = t64_machine_bus_write(
        machine, origin + (uint32_t)(i * 2u), 2u, pixels[i]);
    T64_CHECK_INT(status, T64_STATUS_OK);
    if (status != T64_STATUS_OK) return false;
  }
  return true;
}

static bool seed_vi_source(t64_machine *machine) {
  return seed_vi_source_at(machine, UINT32_C(0x3000));
}

static bool seed_full_vi_source(t64_machine *machine, uint32_t origin) {
  size_t available;
  uint32_t row_count;
  uint32_t row;

  if (machine == NULL || (uint64_t)origin >= machine->profile.rdram_bytes)
    return false;
  available = (size_t)(machine->profile.rdram_bytes - origin);
  if (available > (size_t)307200u) available = (size_t)307200u;
  (void)memset(machine->ram + origin, 0, available);
  row_count = (uint32_t)(available / (size_t)640u);
  for (row = 0u; row < row_count; row++) {
    const uint32_t address = origin + row * UINT32_C(640);
    const uint16_t pixel = row == 478u ? UINT16_C(0x07c1)
                           : row == 479u ? UINT16_C(0xffff)
                           : (row & 1u) == 0u ? UINT16_C(0xf801)
                                              : UINT16_C(0x003f);
    if ((uint64_t)address + UINT64_C(2) > machine->profile.rdram_bytes)
      return false;
    if (t64_machine_bus_write(machine, address, 2u, pixel) != T64_STATUS_OK)
      return false;
  }
  return true;
}

static bool stage_full_vi_geometry(t64_machine *machine, uint32_t origin,
                                   uint32_t control, uint32_t y_scale) {
  return vi_write(machine, UINT32_C(0x04), origin) &&
         vi_write(machine, UINT32_C(0x08), UINT32_C(320)) &&
         vi_write(machine, UINT32_C(0x24), UINT32_C(320)) &&
         vi_write(machine, UINT32_C(0x28), UINT32_C(0x000001e0)) &&
         vi_write(machine, UINT32_C(0x30), UINT32_C(0x00000400)) &&
         vi_write(machine, UINT32_C(0x34), y_scale) &&
         vi_write(machine, UINT32_C(0x00), control);
}

static bool configure_vi_witness(t64_machine *machine) {
  return seed_vi_source(machine) &&
         vi_write(machine, UINT32_C(0x04), UINT32_C(0x3000)) &&
         vi_write(machine, UINT32_C(0x08), UINT32_C(4)) &&
         vi_write(machine, UINT32_C(0x0c), UINT32_C(2)) &&
         vi_write(machine, UINT32_C(0x14), UINT32_C(0)) &&
         vi_write(machine, UINT32_C(0x18), UINT32_C(8)) &&
         vi_write(machine, UINT32_C(0x1c), UINT32_C(15)) &&
         vi_write(machine, UINT32_C(0x24), UINT32_C(4)) &&
         vi_write(machine, UINT32_C(0x28), UINT32_C(0x00020008)) &&
         vi_write(machine, UINT32_C(0x30), UINT32_C(0x00000400)) &&
         vi_write(machine, UINT32_C(0x34), UINT32_C(0x00000400)) &&
         t64_machine_bus_write(machine, MI_INTR_MASK, 4u,
                               UINT32_C(0x80)) == T64_STATUS_OK &&
         vi_write(machine, UINT32_C(0x00), UINT32_C(2));
}

static bool configure_vi(t64_machine *machine, uint32_t control,
                         uint32_t origin, uint32_t width, uint32_t intr,
                         uint32_t v_sync, uint32_t h_sync, uint32_t h_start,
                         uint32_t v_start, uint32_t x_scale,
                         uint32_t y_scale) {
  return vi_write(machine, UINT32_C(0x04), origin) &&
         vi_write(machine, UINT32_C(0x08), width) &&
         vi_write(machine, UINT32_C(0x0c), intr) &&
         vi_write(machine, UINT32_C(0x14), UINT32_C(0)) &&
         vi_write(machine, UINT32_C(0x18), v_sync) &&
         vi_write(machine, UINT32_C(0x1c), h_sync) &&
         vi_write(machine, UINT32_C(0x20), UINT32_C(0)) &&
         vi_write(machine, UINT32_C(0x24), h_start) &&
         vi_write(machine, UINT32_C(0x28), v_start) &&
         vi_write(machine, UINT32_C(0x2c), UINT32_C(0)) &&
         vi_write(machine, UINT32_C(0x30), x_scale) &&
         vi_write(machine, UINT32_C(0x34), y_scale) &&
         vi_write(machine, UINT32_C(0x00), control);
}

static uint8_t *render_filter_fixture(
    const char *profile_id, uint32_t status, uint32_t source_width,
    uint32_t source_height,
    uint32_t output_width, uint32_t output_height, uint32_t source_format,
    uint32_t x_offset, uint32_t y_offset, const uint32_t *rgba8888,
    const uint16_t *rgba5551, const uint8_t *coverage,
    t64_video_packet_info *info_out, size_t *length_out) {
  const uint32_t origin = UINT32_C(0x3000);
  const size_t source_pixels = (size_t)source_width * source_height;
  const size_t pixel_bytes = source_format == T64_VIDEO_SOURCE_RGBA8888 ? 4u : 2u;
  const size_t packet_bytes =
      (size_t)output_width * output_height * (size_t)4u;
  t64_machine *machine =
      strcmp(profile_id, VI_FILTER_PROFILE_ID) == 0
          ? create_vi_filter_machine()
          : create_authored_machine_for_profile(profile_id);
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  uint8_t *packet = NULL;
  size_t i;
  t64_status status_code;

  if (machine == NULL || info_out == NULL || length_out == NULL ||
      source_width == 0u || source_height == 0u || output_width == 0u ||
      output_height == 0u ||
      (source_format == T64_VIDEO_SOURCE_RGBA8888 && rgba8888 == NULL) ||
      (source_format == T64_VIDEO_SOURCE_RGBA5551 && rgba5551 == NULL)) {
    if (machine != NULL) t64_machine_destroy(machine);
    return NULL;
  }
  packet = (uint8_t *)malloc(packet_bytes);
  if (packet == NULL) {
    t64_machine_destroy(machine);
    return NULL;
  }
  for (i = 0u; i < source_pixels; i++) {
    const uint32_t address = origin + (uint32_t)(i * pixel_bytes);
    if (pixel_bytes == 4u) {
      status_code = t64_machine_bus_write(machine, address, 4u, rgba8888[i]);
    } else {
      status_code = t64_machine_bus_write(machine, address, 2u, rgba5551[i]);
    }
    if (status_code != T64_STATUS_OK) {
      T64_CHECK_INT(status_code, T64_STATUS_OK);
      free(packet);
      t64_machine_destroy(machine);
      return NULL;
    }
    t64_rdp_memory_coverage_write(machine, address, (uint32_t)pixel_bytes,
                                  coverage == NULL ? 8u : coverage[i]);
  }
  if (!configure_vi(machine, status, origin, source_width, 0u, 8u, 15u,
                    output_width, output_height * 2u,
                    (x_offset << 16) | UINT32_C(0x400),
                    (y_offset << 16) | UINT32_C(0x400))) {
    free(packet);
    t64_machine_destroy(machine);
    return NULL;
  }
  if (!advance_guest_to_tick(machine, UINT64_C(83), &stop)) {
    free(packet);
    t64_machine_destroy(machine);
    return NULL;
  }
  status_code = t64_video_info(machine, info_out);
  if (status_code != T64_STATUS_OK) {
    T64_CHECK_INT(status_code, T64_STATUS_OK);
    free(packet);
    t64_machine_destroy(machine);
    return NULL;
  }
  if (info_out->packet_bytes != packet_bytes) {
    T64_CHECK_INT(info_out->packet_bytes, packet_bytes);
    free(packet);
    t64_machine_destroy(machine);
    return NULL;
  }
  status_code = t64_video_copy(machine, packet, packet_bytes, length_out);
  if (status_code != T64_STATUS_OK || *length_out != packet_bytes) {
    T64_CHECK_INT(status_code, T64_STATUS_OK);
    T64_CHECK_INT(*length_out, packet_bytes);
    free(packet);
    t64_machine_destroy(machine);
    return NULL;
  }
  t64_machine_destroy(machine);
  return packet;
}

static void case_vi_aa_modes(void) {
  static const uint32_t rgba[15] = {
      UINT32_C(0x2878c81f), UINT32_C(0x2878c81f), UINT32_C(0x2878c81f),
      UINT32_C(0x2878c81f), UINT32_C(0x2878c81f), UINT32_C(0x2878c81f),
      UINT32_C(0x2878c81f), UINT32_C(0xc8502807), UINT32_C(0x2878c81f),
      UINT32_C(0x2878c81f), UINT32_C(0x2878c81f), UINT32_C(0x2878c81f),
      UINT32_C(0x2878c81f), UINT32_C(0x2878c81f), UINT32_C(0x2878c81f),
  };
  static const uint8_t coverage[15] = {
      8u, 8u, 8u, 8u, 8u, 8u, 8u, 2u, 8u, 8u, 8u, 8u, 8u, 8u, 8u,
  };
  static const uint8_t expected_aa[4] = {80u, 110u, 160u, 57u};
  static const uint8_t expected_unfiltered[4] = {200u, 80u, 40u, 57u};
  static const uint32_t modes[4] = {
      UINT32_C(0x00000003), UINT32_C(0x00000103),
      UINT32_C(0x00000203), UINT32_C(0x00000303),
  };
  size_t mode;
  for (mode = 0u; mode < sizeof modes / sizeof modes[0]; mode++) {
    t64_video_packet_info info = {0};
    size_t length = 0u;
    uint8_t *packet = render_filter_fixture(
        VI_FILTER_PROFILE_ID, modes[mode], 5u, 3u, 5u, 3u,
        T64_VIDEO_SOURCE_RGBA8888, 0u, 0u,
        rgba, NULL, coverage, &info, &length);
    const uint8_t *expected = mode < 2u ? expected_aa : expected_unfiltered;
    const uint32_t expected_filter =
        mode < 2u ? (T64_VIDEO_FILTER_AA | T64_VIDEO_FILTER_RESAMPLE_Q20)
                  : mode == 2u ? T64_VIDEO_FILTER_RESAMPLE_Q20 : 0u;
    if (packet == NULL) return;
    T64_CHECK_INT(info.source_format, T64_VIDEO_SOURCE_RGBA8888);
    T64_CHECK_INT(info.width, 5u);
    T64_CHECK_INT(info.height, 3u);
    T64_CHECK_INT(info.source_bytes, 60u);
    T64_CHECK_INT(length, 60u);
    T64_CHECK_INT(info.field, 0u);
    T64_CHECK_INT(info.parity, 0u);
    T64_CHECK_INT(info.end_tick, 83u);
    T64_CHECK_INT(info.filter, expected_filter);
    T64_CHECK_STR(info.profile_id, VI_FILTER_PROFILE_ID);
    T64_CHECK_STR(info.vi_output_policy,
                  "5551-8888-aa53mdtl-q8f-df3-dvmed-bq20f-gd16r8-sqrt255-pcspan-v3");
    T64_CHECK_STR(info.build_id, t64_build_identity());
    T64_CHECK_INT(packet[28u], expected[0]);
    T64_CHECK_INT(packet[29u], expected[1]);
    T64_CHECK_INT(packet[30u], expected[2]);
    T64_CHECK_INT(packet[31u], expected[3]);
    free(packet);
  }
}

static void case_vi_resample_fraction(void) {
  static const uint32_t rgba[4] = {
      UINT32_C(0x0000001f), UINT32_C(0x01010a1f),
      UINT32_C(0x01010a1f), UINT32_C(0x0202141f),
  };
  t64_video_packet_info info = {0};
  size_t length = 0u;
  uint8_t *packet = render_filter_fixture(
      VI_FILTER_PROFILE_ID, UINT32_C(0x00000203), 2u, 2u, 1u, 1u,
      T64_VIDEO_SOURCE_RGBA8888,
      512u, 512u, rgba, NULL, NULL, &info, &length);
  if (packet == NULL) return;
  T64_CHECK_INT(packet[0], 1u);
  T64_CHECK_INT(packet[1], 1u);
  T64_CHECK_INT(packet[2], 10u);
  T64_CHECK_INT(packet[3], 255u);
  T64_CHECK_INT(info.source_bytes, 16u);
  T64_CHECK_INT(info.filter, T64_VIDEO_FILTER_RESAMPLE_Q20);
  T64_CHECK_INT(length, 4u);
  free(packet);
}

static void case_vi_divot(void) {
  static const uint32_t rgba[15] = {
      UINT32_C(0x0000001f), UINT32_C(0x0000001f), UINT32_C(0x0000001f),
      UINT32_C(0x0000001f), UINT32_C(0x0000001f), UINT32_C(0x0000001f),
      UINT32_C(0x50140a1f), UINT32_C(0xc850281f), UINT32_C(0x5a140a1f),
      UINT32_C(0x0000001f), UINT32_C(0x0000001f), UINT32_C(0x0000001f),
      UINT32_C(0x0000001f), UINT32_C(0x0000001f), UINT32_C(0x0000001f),
  };
  static const uint8_t coverage[15] = {
      8u, 8u, 8u, 8u, 8u, 8u, 8u, 2u, 8u, 8u, 8u, 8u, 8u, 8u, 8u,
  };
  t64_video_packet_info info = {0};
  size_t length = 0u;
  uint8_t *packet = render_filter_fixture(
      VI_FILTER_PROFILE_ID, UINT32_C(0x00000013), 5u, 3u, 5u, 3u,
      T64_VIDEO_SOURCE_RGBA8888,
      0u, 0u, rgba, NULL, coverage, &info, &length);
  if (packet == NULL) return;
  T64_CHECK_INT(packet[28u], 80u);
  T64_CHECK_INT(packet[29u], 20u);
  T64_CHECK_INT(packet[30u], 10u);
  T64_CHECK_INT(packet[31u], 255u);
  T64_CHECK_INT(info.filter,
                T64_VIDEO_FILTER_AA | T64_VIDEO_FILTER_DIVOT_MEDIAN |
                    T64_VIDEO_FILTER_RESAMPLE_Q20);
  free(packet);
}

static void case_vi_gamma(void) {
  static const uint32_t rgba[1] = {UINT32_C(0x0464fa1f)};
  t64_video_packet_info info = {0};
  size_t length = 0u;
  uint8_t *packet = render_filter_fixture(
      VI_FILTER_PROFILE_ID, UINT32_C(0x0000000b), 1u, 1u, 1u, 1u,
      T64_VIDEO_SOURCE_RGBA8888,
      0u, 0u, rgba, NULL, NULL, &info, &length);
  if (packet == NULL) return;
  T64_CHECK_INT(packet[0], 31u);
  T64_CHECK_INT(packet[1], 159u);
  T64_CHECK_INT(packet[2], 252u);
  T64_CHECK_INT(packet[3], 255u);
  T64_CHECK_INT(info.filter,
                T64_VIDEO_FILTER_RESAMPLE_Q20 |
                    T64_VIDEO_FILTER_GAMMA_SQRT255);
  T64_CHECK_INT(info.source_bytes, 4u);
  free(packet);
}

static void case_vi_gamma_dither(void) {
  static const uint32_t rgba[4] = {
      UINT32_C(0x6464641f), UINT32_C(0x6464641f),
      UINT32_C(0x6464641f), UINT32_C(0x6464641f),
  };
  t64_video_packet_info info = {0};
  size_t length = 0u;
  uint8_t *packet = render_filter_fixture(
      VI_FILTER_PROFILE_ID, UINT32_C(0x0000000f), 4u, 1u, 2u, 1u,
      T64_VIDEO_SOURCE_RGBA8888,
      0u, 0u, rgba, NULL, NULL, &info, &length);
  if (packet == NULL) return;
  T64_CHECK_INT(packet[0], 158u);
  T64_CHECK_INT(packet[4], 160u);
  T64_CHECK_INT(packet[3], 255u);
  T64_CHECK_INT(info.filter,
                T64_VIDEO_FILTER_RESAMPLE_Q20 |
                    T64_VIDEO_FILTER_GAMMA_DITHER |
                    T64_VIDEO_FILTER_GAMMA_SQRT255);
  free(packet);
}

static void case_vi_dither_filter(void) {
  static const uint16_t rgba[9] = {
      0u, 0u, 0u, 0u, UINT16_C(0xffff), 0u, 0u, 0u, 0u,
  };
  t64_video_packet_info info = {0};
  size_t length = 0u;
  uint8_t *packet = render_filter_fixture(
      VI_FILTER_PROFILE_ID, UINT32_C(0x00010002), 3u, 3u, 3u, 3u,
      T64_VIDEO_SOURCE_RGBA5551,
      0u, 0u, NULL, rgba, NULL, &info, &length);
  if (packet == NULL) return;
  T64_CHECK_INT(packet[16u], 28u);
  T64_CHECK_INT(packet[17u], 28u);
  T64_CHECK_INT(packet[18u], 28u);
  T64_CHECK_INT(packet[19u], 255u);
  T64_CHECK_INT(info.filter,
                T64_VIDEO_FILTER_RESAMPLE_Q20 |
                    T64_VIDEO_FILTER_DITHER_SMOOTH);
  T64_CHECK_INT(info.source_bytes, 18u);
  free(packet);
}

static void case_vi_aa_dither_precedence(void) {
  static const uint16_t rgba[9] = {
      0u, 0u, 0u, 0u, UINT16_C(0xffff), 0u, 0u, 0u, 0u,
  };
  static const uint8_t coverage[9] = {8u, 8u, 8u, 8u, 2u, 8u, 8u, 8u, 8u};
  t64_video_packet_info info = {0};
  size_t length = 0u;
  uint8_t *packet = render_filter_fixture(
      VI_FILTER_PROFILE_ID, UINT32_C(0x00010102), 3u, 3u, 3u, 3u,
      T64_VIDEO_SOURCE_RGBA5551, 0u, 0u, NULL, rgba, coverage, &info,
      &length);
  if (packet == NULL) return;
  T64_CHECK_INT(packet[16u], 63u);
  T64_CHECK_INT(packet[17u], 63u);
  T64_CHECK_INT(packet[18u], 63u);
  T64_CHECK_INT(packet[19u], 255u);
  T64_CHECK_INT(info.filter,
                T64_VIDEO_FILTER_AA | T64_VIDEO_FILTER_DITHER_SMOOTH |
                    T64_VIDEO_FILTER_RESAMPLE_Q20);
  T64_CHECK_INT(length, 36u);
  free(packet);
}

static void case_vi_gamma_dither_requires_gamma(void) {
  static const uint32_t rgba[1] = {UINT32_C(0x2850781f)};
  t64_video_packet_info info = {0};
  size_t length = 0u;
  uint8_t *packet = render_filter_fixture(
      VI_FILTER_PROFILE_ID, UINT32_C(0x00000007), 1u, 1u, 1u, 1u,
      T64_VIDEO_SOURCE_RGBA8888, 0u, 0u, rgba, NULL, NULL, &info, &length);
  if (packet == NULL) return;
  T64_CHECK_INT(packet[0], 40u);
  T64_CHECK_INT(packet[1], 80u);
  T64_CHECK_INT(packet[2], 120u);
  T64_CHECK_INT(packet[3], 255u);
  T64_CHECK_INT(info.filter, T64_VIDEO_FILTER_RESAMPLE_Q20);
  free(packet);
}

static void case_vi_filter_stage_order(void) {
  static const uint32_t rgba[4] = {
      UINT32_C(0x0000001f), UINT32_C(0x01010a1f),
      UINT32_C(0x01010a1f), UINT32_C(0x0202141f),
  };
  t64_video_packet_info info = {0};
  size_t length = 0u;
  uint8_t *packet = render_filter_fixture(
      VI_FILTER_PROFILE_ID, UINT32_C(0x0000020b), 2u, 2u, 1u, 1u,
      T64_VIDEO_SOURCE_RGBA8888,
      512u, 512u, rgba, NULL, NULL, &info, &length);
  if (packet == NULL) return;
  T64_CHECK_INT(packet[0], 15u);
  T64_CHECK_INT(packet[1], 15u);
  T64_CHECK_INT(packet[2], 50u);
  T64_CHECK_INT(info.filter,
                T64_VIDEO_FILTER_RESAMPLE_Q20 |
                    T64_VIDEO_FILTER_GAMMA_SQRT255);
  free(packet);
}

static size_t first_difference(const uint8_t *left, const uint8_t *right,
                               size_t length) {
  size_t i;
  for (i = 0u; i < length; i++)
    if (left[i] != right[i]) return i;
  return SIZE_MAX;
}

static void case_vi_filter_wrong_expected(void) {
  static const uint8_t expected[60] = {
      40u, 120u, 200u, 255u, 40u, 120u, 200u, 255u,
      40u, 120u, 200u, 255u, 40u, 120u, 200u, 255u,
      40u, 120u, 200u, 255u, 40u, 120u, 200u, 255u,
      40u, 120u, 200u, 255u, 80u, 110u, 160u, 255u,
      40u, 120u, 200u, 255u, 40u, 120u, 200u, 255u,
      40u, 120u, 200u, 255u, 40u, 120u, 200u, 255u,
      40u, 120u, 200u, 255u, 40u, 120u, 200u, 255u,
      40u, 120u, 200u, 255u,
  };
  static const uint32_t rgba[15] = {
      UINT32_C(0x2878c81f), UINT32_C(0x2878c81f), UINT32_C(0x2878c81f),
      UINT32_C(0x2878c81f), UINT32_C(0x2878c81f), UINT32_C(0x2878c81f),
      UINT32_C(0x2878c81f), UINT32_C(0xc850281f), UINT32_C(0x2878c81f),
      UINT32_C(0x2878c81f), UINT32_C(0x2878c81f), UINT32_C(0x2878c81f),
      UINT32_C(0x2878c81f), UINT32_C(0x2878c81f), UINT32_C(0x2878c81f),
  };
  static const uint8_t coverage[15] = {
      8u, 8u, 8u, 8u, 8u, 8u, 8u, 2u, 8u, 8u, 8u, 8u, 8u, 8u, 8u,
  };
  t64_video_packet_info info = {0};
  size_t length = 0u;
  uint8_t *packet = render_filter_fixture(
      VI_FILTER_PROFILE_ID, UINT32_C(0x00000003), 5u, 3u, 5u, 3u,
      T64_VIDEO_SOURCE_RGBA8888,
      0u, 0u, rgba, NULL, coverage, &info, &length);
  uint8_t *wrong;
  if (packet == NULL) return;
  wrong = (uint8_t *)malloc(length);
  if (wrong == NULL) {
    free(packet);
    T64_CHECK(false);
    return;
  }
  T64_CHECK_INT(first_difference(packet, expected, sizeof expected),
                SIZE_MAX);
  (void)memcpy(wrong, expected, sizeof expected);
  wrong[28u] = (uint8_t)(wrong[28u] - 1u);
  T64_CHECK_INT(first_difference(packet, wrong, length), 28u);
  free(wrong);
  free(packet);
}

static void case_vi_rdp_fill_coverage_to_filtered_packet(void) {
  const t64_host_allocator allocator = {test_allocate, test_release, NULL};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t rom[TEST_ROM_CAPACITY];
  uint8_t packet[sizeof expected_packet];
  size_t rom_length = 0u;
  size_t packet_length = 0u;
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  t64_video_packet_info info = {0};
  size_t attempt;
  bool captured = false;

  T64_CHECK(load_authored_rom(rom, sizeof rom, &rom_length));
  if (rom_length == 0u) return;
  T64_CHECK_INT(t64_profile_lookup(VI_FILTER_PROFILE_ID, &profile),
                T64_STATUS_OK);
  if (profile == NULL) return;
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  if (machine == NULL) return;
  T64_CHECK_INT(t64_machine_admit_rom(machine, rom, rom_length),
                T64_STATUS_OK);
  for (attempt = 0u; attempt < 512u && !captured; attempt++) {
    t64_status status = t64_machine_advance(
        machine,
        &(t64_stop_request){T64_STOP_GUEST_TICK, TEST_COMPLETION_TICK},
        &stop);
    t64_video_packet_info queued;
    T64_CHECK_INT(status, T64_STATUS_OK);
    if (status != T64_STATUS_OK) break;
    while (t64_video_info(machine, &queued) == T64_STATUS_OK) {
      uint8_t candidate[sizeof expected_packet];
      size_t candidate_length = 0u;
      t64_status copy_status = t64_video_copy(
          machine, candidate, sizeof candidate, &candidate_length);
      T64_CHECK_INT(copy_status, T64_STATUS_OK);
      if (copy_status != T64_STATUS_OK) break;
      if (queued.commands_completed == 6u &&
          queued.fullsync_completed == 1u) {
        info = queued;
        packet_length = candidate_length;
        (void)memcpy(packet, candidate, candidate_length);
        captured = true;
        break;
      }
    }
    if (captured) break;
    if (stop.reason != T64_STOP_REASON_RESOURCE_LIMIT) break;
  }
  T64_CHECK(captured);
  if (!captured) {
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK_INT(info.field, 0u);
  T64_CHECK_INT(info.source_format, T64_VIDEO_SOURCE_RGBA5551);
  T64_CHECK_INT(info.width, 4u);
  T64_CHECK_INT(info.height, 1u);
  T64_CHECK_INT(info.source_bytes, (size_t)8u);
  T64_CHECK_INT(info.packet_bytes, (size_t)16u);
  T64_CHECK_INT(info.commands_completed, 6u);
  T64_CHECK_INT(info.fullsync_completed, 1u);
  T64_CHECK_INT(info.filter, T64_VIDEO_FILTER_RESAMPLE_Q20);
  T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, UINT32_C(0x3000), 2u),
                8u);
  T64_CHECK_INT(packet_length, (size_t)16u);
  T64_CHECK(memcmp(packet, expected_packet, (size_t)16u) == 0);
  t64_machine_destroy(machine);
}

static void case_vi_v8_filter_ignore(void) {
  static const uint32_t rgba[15] = {
      UINT32_C(0x2878c81f), UINT32_C(0x2878c81f), UINT32_C(0x2878c81f),
      UINT32_C(0x2878c81f), UINT32_C(0x2878c81f), UINT32_C(0x2878c81f),
      UINT32_C(0x2878c81f), UINT32_C(0xc850281f), UINT32_C(0x2878c81f),
      UINT32_C(0x2878c81f), UINT32_C(0x2878c81f), UINT32_C(0x2878c81f),
      UINT32_C(0x2878c81f), UINT32_C(0x2878c81f), UINT32_C(0x2878c81f),
  };
  static const uint8_t coverage[15] = {
      8u, 8u, 8u, 8u, 8u, 8u, 8u, 2u, 8u, 8u, 8u, 8u, 8u, 8u, 8u,
  };
  const uint32_t status_values[2] = {
      UINT32_C(0x00000003), UINT32_C(0x0000031f),
  };
  size_t i;
  for (i = 0u; i < sizeof status_values / sizeof status_values[0]; i++) {
    t64_video_packet_info info = {0};
    size_t length = 0u;
    uint8_t *packet = render_filter_fixture(
        VI_V8_PROFILE_ID, status_values[i], 5u, 3u, 5u, 3u,
        T64_VIDEO_SOURCE_RGBA8888, 0u, 0u, rgba, NULL, coverage, &info,
        &length);
    if (packet == NULL) return;
    T64_CHECK_INT(packet[28u], 200u);
    T64_CHECK_INT(packet[29u], 80u);
    T64_CHECK_INT(packet[30u], 40u);
    T64_CHECK_INT(packet[31u], 255u);
    T64_CHECK_INT(info.filter, 0u);
    free(packet);
  }
}

static void case_vi_ntsc_rational_field_timing(void) {
  t64_machine *machine = create_vi_v8_machine();
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  t64_video_packet_info packet_info = {0};
  uint32_t current = 0u;
  uint32_t interrupt = 0u;
  bool configured;
  uint8_t packet[48];
  size_t packet_length = 0u;
  static const uint8_t expected[48] = {
      255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255,
      0, 255, 0, 255, 255, 0, 0, 255, 255, 255, 255, 255, 0, 0, 255, 255,
      0, 0, 255, 255, 255, 255, 255, 255, 255, 0, 0, 255, 0, 255, 0, 255,
  };

  if (machine == NULL) return;
  configured = configure_vi_witness(machine);
  T64_CHECK(configured);
  if (!configured) {
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(21), &stop));
  T64_CHECK_INT(t64_machine_bus_read(machine, VI_BASE + UINT32_C(0x10), 4u,
                                     &current), T64_STATUS_OK);
  T64_CHECK_INT(current, UINT32_C(2));
  T64_CHECK_INT(t64_machine_bus_read(machine, MI_INTR, 4u, &interrupt),
                T64_STATUS_OK);
  T64_CHECK_INT(interrupt & UINT32_C(0x08), UINT32_C(0x08));
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(83), &stop));
  T64_CHECK_INT(stop.tick, UINT64_C(83));
  T64_CHECK_INT(t64_video_info(machine, &packet_info), T64_STATUS_OK);
  T64_CHECK_INT(packet_info.end_tick, UINT64_C(83));
  T64_CHECK_INT(packet_info.width, UINT32_C(4));
  T64_CHECK_INT(packet_info.height, UINT32_C(3));
  T64_CHECK_INT(packet_info.source_bytes, (size_t)24u);
  T64_CHECK_INT(t64_video_copy(machine, packet, sizeof packet, &packet_length),
                T64_STATUS_OK);
  T64_CHECK_INT(packet_length, sizeof expected);
  T64_CHECK(memcmp(packet, expected, sizeof expected) == 0);
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(166), &stop));
  T64_CHECK_INT(t64_video_info(machine, &packet_info), T64_STATUS_OK);
  T64_CHECK_INT(packet_info.field, UINT32_C(1));
  T64_CHECK_INT(packet_info.parity, UINT32_C(0));
  T64_CHECK_INT(t64_machine_bus_read(machine, VI_BASE + UINT32_C(0x10), 4u,
                                     &current), T64_STATUS_OK);
  T64_CHECK_INT(current, UINT32_C(0));
  T64_CHECK_INT(t64_video_copy(machine, packet, sizeof packet, &packet_length),
                T64_STATUS_OK);
  t64_machine_destroy(machine);
}

static void case_vi_register_inventory_readback_reserved_policy(void) {
  static const uint32_t offsets[14] = {
      0x00u, 0x04u, 0x08u, 0x0cu, 0x10u, 0x14u, 0x18u,
      0x1cu, 0x20u, 0x24u, 0x28u, 0x2cu, 0x30u, 0x34u,
  };
  static const uint32_t values[14] = {
      UINT32_C(0x00010040), UINT32_C(0xff123456), UINT32_C(0xabcd0004),
      UINT32_C(0xfeed0002), UINT32_C(0xa5a5a5a5), UINT32_C(0xdeadbeef),
      UINT32_C(0xcafe0008), UINT32_C(0x801f000f), UINT32_C(0x0fff0fff),
      UINT32_C(0xfc000004), UINT32_C(0xfc020008), UINT32_C(0xfc000000),
      UINT32_C(0xf0000400), UINT32_C(0xf0000400),
  };
  t64_machine *machine = create_vi_v8_machine();
  size_t i;

  if (machine == NULL) return;
  for (i = 0u; i < 14u; i++)
    T64_CHECK_INT(t64_machine_bus_write(machine, VI_BASE + offsets[i], 4u,
                                        values[i]), T64_STATUS_OK);
  for (i = 0u; i < 14u; i++) {
    uint32_t value = UINT32_MAX;
    T64_CHECK_INT(t64_machine_bus_read(machine, VI_BASE + offsets[i], 4u,
                                       &value), T64_STATUS_OK);
    T64_CHECK_INT(value, i == 4u ? 0u : values[i]);
  }
  T64_CHECK_INT(machine->vi.registers[4], UINT32_C(0xa5a5a5a5));
  T64_CHECK_INT(machine->vi.registers[0] & UINT32_C(0x00010040),
                UINT32_C(0x00010040));
  t64_machine_destroy(machine);
}

static void case_vi_diagnostic_vs_standard_timing(void) {
  t64_machine *machine = create_vi_v8_machine();
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  t64_video_packet_info info = {0};

  if (machine == NULL) return;
  T64_CHECK(seed_vi_source(machine));
  T64_CHECK(configure_vi(machine, 2u, UINT32_C(0x3000), 4u, 0u, 2u, 4u,
                         4u, UINT32_C(0x00020008), UINT32_C(0x400),
                         UINT32_C(0x400)));
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(7), &stop));
  T64_CHECK_INT(t64_video_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.end_tick, UINT64_C(7));
  T64_CHECK_INT(info.width, 4u);
  T64_CHECK_INT(info.height, 3u);
  T64_CHECK_STR(machine->profile.vi_clock_policy,
                "rsp62500000-ntsc48681812-halfline-rational-v2");
  t64_machine_destroy(machine);
}

static void case_vi_geometry_rgba5551_rgba8888(void) {
  static const uint32_t pixels[12] = {
      UINT32_C(0x112233ff), UINT32_C(0x4455661f), UINT32_C(0x77889900),
      UINT32_C(0xaabbcc10), UINT32_C(0x203040ff), UINT32_C(0x5060701f),
      UINT32_C(0x8090a000), UINT32_C(0xb0c0d010), UINT32_C(0x010203ff),
      UINT32_C(0x0405061f), UINT32_C(0x07080900), UINT32_C(0x0a0b0c10),
  };
  t64_machine *machine = create_vi_v8_machine();
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  t64_video_packet_info info = {0};
  uint8_t packet[48];
  size_t length = 0u;
  size_t i;

  if (machine == NULL) return;
  for (i = 0u; i < sizeof pixels / sizeof pixels[0]; i++)
    T64_CHECK_INT(t64_machine_bus_write(
                      machine, UINT32_C(0x3000) + (uint32_t)(i * 4u), 4u,
                      pixels[i]), T64_STATUS_OK);
  T64_CHECK(configure_vi(machine, 3u, UINT32_C(0x3000), 4u, 0u, 8u, 15u,
                         4u, UINT32_C(0x00020008), UINT32_C(0x400),
                         UINT32_C(0x400)));
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(83), &stop));
  T64_CHECK_INT(t64_video_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.source_format, T64_VIDEO_SOURCE_RGBA8888);
  T64_CHECK_INT(info.width, 4u);
  T64_CHECK_INT(info.height, 3u);
  T64_CHECK_INT(info.source_width, 4u);
  T64_CHECK_INT(info.source_pitch_bytes, 16u);
  T64_CHECK_INT(info.source_bytes, (size_t)48u);
  T64_CHECK_STR(info.vi_output_policy,
                "rgba5551-rgba8888-rawrw-q10step-serrate2-nextfield-nf-v2");
  T64_CHECK_INT(t64_video_copy(machine, packet, sizeof packet, &length),
                T64_STATUS_OK);
  T64_CHECK_INT(length, sizeof packet);
  T64_CHECK_INT(packet[0], 0x11u);
  T64_CHECK_INT(packet[1], 0x22u);
  T64_CHECK_INT(packet[2], 0x33u);
  T64_CHECK_INT(packet[3], 0xffu);
  t64_machine_destroy(machine);
}

static void case_vi_geometry_minimum_window(void) {
  t64_machine *machine = create_vi_v8_machine();
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  t64_video_packet_info info = {0};
  uint8_t packet[4] = {0u, 0u, 0u, 0u};
  size_t length = 0u;

  if (machine == NULL) return;
  T64_CHECK(seed_vi_source(machine));
  T64_CHECK(configure_vi(machine, 2u, UINT32_C(0x3000), 4u, 0u, 8u, 15u,
                         1u, UINT32_C(0x00000002), UINT32_C(0x400),
                         UINT32_C(0x400)));
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(83), &stop));
  T64_CHECK_INT(t64_video_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.width, UINT32_C(1));
  T64_CHECK_INT(info.height, UINT32_C(1));
  T64_CHECK_INT(info.source_width, UINT32_C(4));
  T64_CHECK_INT(info.source_pitch_bytes, UINT32_C(8));
  T64_CHECK_INT(info.source_bytes, (size_t)2u);
  T64_CHECK_INT(t64_video_copy(machine, packet, sizeof packet, &length),
                T64_STATUS_OK);
  T64_CHECK_INT(length, sizeof packet);
  T64_CHECK_INT(packet[0], UINT8_C(255));
  T64_CHECK_INT(packet[1], UINT8_C(0));
  T64_CHECK_INT(packet[2], UINT8_C(0));
  T64_CHECK_INT(packet[3], UINT8_C(255));
  t64_machine_destroy(machine);
}

static void case_vi_scaled_offset_window_source_pitch(void) {
  static const uint16_t palette[4] = {
      UINT16_C(0xf801), UINT16_C(0x07c1), UINT16_C(0x003f), UINT16_C(0xffff),
  };
  t64_machine *machine = create_vi_v8_machine();
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  t64_video_packet_info info = {0};
  uint8_t packet[8];
  size_t length = 0u;
  uint32_t y;
  uint32_t x;

  if (machine == NULL) return;
  for (y = 0u; y < 4u; y++) {
    for (x = 0u; x < 8u; x++) {
      uint32_t index = y * 8u + x;
      T64_CHECK_INT(t64_machine_bus_write(
                        machine, UINT32_C(0x3000) + index * 2u, 2u,
                        palette[x & 3u]), T64_STATUS_OK);
    }
  }
  T64_CHECK(configure_vi(machine, 2u, UINT32_C(0x3000), 8u, 0u, 8u, 15u,
                         4u, UINT32_C(0x00020008), UINT32_C(0x04000800),
                         UINT32_C(0x04000800)));
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(83), &stop));
  T64_CHECK_INT(t64_video_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.width, 2u);
  T64_CHECK_INT(info.height, 1u);
  T64_CHECK_INT(info.source_width, 8u);
  T64_CHECK_INT(info.source_pitch_bytes, 16u);
  T64_CHECK_INT(info.source_x_offset, UINT32_C(0x400));
  T64_CHECK_INT(info.source_y_offset, UINT32_C(0x400));
  T64_CHECK_INT(info.source_bytes, (size_t)24u);
  T64_CHECK_INT(t64_video_copy(machine, packet, sizeof packet, &length),
                T64_STATUS_OK);
  T64_CHECK_INT(length, sizeof packet);
  T64_CHECK_INT(packet[0], 0u);
  T64_CHECK_INT(packet[1], 255u);
  T64_CHECK_INT(packet[2], 0u);
  T64_CHECK_INT(packet[3], 255u);
  T64_CHECK_INT(packet[4], 255u);
  T64_CHECK_INT(packet[5], 255u);
  T64_CHECK_INT(packet[6], 255u);
  T64_CHECK_INT(packet[7], 255u);
  t64_machine_destroy(machine);
}

static void case_vi_serrate_interlace_parity_origin(void) {
  static const uint16_t row_colors[6] = {
      UINT16_C(0xf801), UINT16_C(0x07c1), UINT16_C(0x003f),
      UINT16_C(0xffff), UINT16_C(0xf801), UINT16_C(0x07c1),
  };
  t64_machine *machine = create_vi_v8_machine();
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  t64_video_packet_info info = {0};
  uint8_t packet[48];
  size_t length = 0u;
  uint32_t current = 0u;
  uint32_t y;
  uint32_t x;

  if (machine == NULL) return;
  for (y = 0u; y < 6u; y++) {
    for (x = 0u; x < 4u; x++)
      T64_CHECK_INT(t64_machine_bus_write(
                        machine, UINT32_C(0x3000) + (y * 4u + x) * 2u,
                        2u, row_colors[y]), T64_STATUS_OK);
  }
  T64_CHECK(configure_vi(machine, UINT32_C(0x42), UINT32_C(0x3000), 4u,
                         0u, 8u, 15u, 4u, UINT32_C(0x00020008),
                         UINT32_C(0x400), UINT32_C(0x400)));
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(83), &stop));
  T64_CHECK_INT(t64_video_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.field, 0u);
  T64_CHECK_INT(info.parity, 0u);
  T64_CHECK_INT(t64_video_copy(machine, packet, sizeof packet, &length),
                T64_STATUS_OK);
  T64_CHECK_INT(packet[0], 255u);
  T64_CHECK_INT(packet[1], 0u);
  T64_CHECK_INT(t64_machine_advance(
                    machine,
                    &(t64_stop_request){T64_STOP_GUEST_TICK, UINT64_C(166)},
                    &stop), T64_STATUS_OK);
  T64_CHECK_INT(t64_video_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.field, 1u);
  T64_CHECK_INT(info.parity, 1u);
  T64_CHECK_INT(t64_machine_bus_read(machine, VI_BASE + UINT32_C(0x10), 4u,
                                     &current), T64_STATUS_OK);
  T64_CHECK_INT(current, UINT32_C(0));
  T64_CHECK_INT(t64_video_copy(machine, packet, sizeof packet, &length),
                T64_STATUS_OK);
  T64_CHECK_INT(packet[0], 0u);
  T64_CHECK_INT(packet[1], 255u);
  T64_CHECK_INT(info.source_pitch_bytes, 8u);
  t64_machine_destroy(machine);
}

static void case_vi_intr_current_write_clear_mi(void) {
  t64_machine *machine = create_vi_v8_machine();
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  uint32_t current = 0u;
  uint32_t pending = 0u;

  if (machine == NULL) return;
  T64_CHECK(configure_vi_witness(machine));
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(21), &stop));
  T64_CHECK_INT(t64_machine_bus_read(machine, VI_BASE + UINT32_C(0x10), 4u,
                                     &current), T64_STATUS_OK);
  T64_CHECK_INT(current, UINT32_C(2));
  T64_CHECK_INT(t64_machine_bus_read(machine, MI_INTR, 4u, &pending),
                T64_STATUS_OK);
  T64_CHECK_INT(pending & UINT32_C(8), UINT32_C(8));
  T64_CHECK_INT(t64_machine_bus_write(machine, VI_BASE + UINT32_C(0x10), 4u,
                                      UINT32_C(0x155)), T64_STATUS_OK);
  current = 0u;
  pending = UINT32_MAX;
  T64_CHECK_INT(t64_machine_bus_read(machine, VI_BASE + UINT32_C(0x10), 4u,
                                     &current), T64_STATUS_OK);
  T64_CHECK_INT(current, UINT32_C(2));
  T64_CHECK_INT(t64_machine_bus_read(machine, MI_INTR, 4u, &pending),
                T64_STATUS_OK);
  T64_CHECK_INT(pending & UINT32_C(8), 0u);
  T64_CHECK(!machine->mi_irq_line);
  t64_machine_destroy(machine);
}

static void case_vi_active_intr_immediate_no_retroactive(void) {
  t64_machine *machine = create_vi_v8_machine();
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  uint32_t current = 0u;
  uint32_t pending = 0u;

  if (machine == NULL) return;
  T64_CHECK(seed_vi_source(machine));
  T64_CHECK_INT(t64_machine_bus_write(machine, MI_INTR_MASK, 4u,
                                      UINT32_C(0x80)), T64_STATUS_OK);
  T64_CHECK(configure_vi(machine, 2u, UINT32_C(0x3000), 4u, 4u, 8u, 15u,
                         4u, UINT32_C(0x00020008), UINT32_C(0x400),
                         UINT32_C(0x400)));
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(20), &stop));
  T64_CHECK(vi_write(machine, UINT32_C(0x0c), UINT32_C(2)));
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(21), &stop));
  T64_CHECK_INT(t64_machine_bus_read(machine, VI_BASE + UINT32_C(0x10), 4u,
                                     &current), T64_STATUS_OK);
  T64_CHECK_INT(current, UINT32_C(2));
  T64_CHECK_INT(t64_machine_bus_read(machine, MI_INTR, 4u, &pending),
                T64_STATUS_OK);
  T64_CHECK_INT(pending & UINT32_C(8), UINT32_C(8));

  T64_CHECK_INT(t64_machine_bus_write(machine, VI_BASE + UINT32_C(0x10), 4u,
                                      UINT32_C(0x155)), T64_STATUS_OK);
  T64_CHECK(vi_write(machine, UINT32_C(0x0c), UINT32_C(2)));
  pending = UINT32_MAX;
  T64_CHECK_INT(t64_machine_bus_read(machine, MI_INTR, 4u, &pending),
                T64_STATUS_OK);
  T64_CHECK_INT(pending & UINT32_C(8), 0u);
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(42), &stop));
  T64_CHECK_INT(t64_machine_bus_read(machine, VI_BASE + UINT32_C(0x10), 4u,
                                     &current), T64_STATUS_OK);
  T64_CHECK_INT(current, UINT32_C(4));
  T64_CHECK_INT(t64_machine_bus_read(machine, MI_INTR, 4u, &pending),
                T64_STATUS_OK);
  T64_CHECK_INT(pending & UINT32_C(8), 0u);
  t64_machine_destroy(machine);
}

static void check_latch_write_tick(uint64_t write_tick,
                                   uint32_t expected_second_origin) {
  t64_machine *machine = create_vi_v8_machine();
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  t64_video_packet_info info = {0};
  uint8_t packet[48];
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  size_t length = 0u;

  if (machine == NULL) return;
  T64_CHECK(seed_vi_source(machine));
  T64_CHECK(seed_vi_source(machine));
  T64_CHECK(configure_vi_witness(machine));
  T64_CHECK(advance_guest_to_tick(machine, write_tick, &stop));
  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);
  T64_CHECK(vi_write(machine, UINT32_C(0x04), UINT32_C(0x4000)));
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) != 0);
  if (machine->tick < UINT64_C(83))
    T64_CHECK(advance_guest_to_tick(machine, UINT64_C(83), &stop));
  T64_CHECK_INT(t64_video_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.source_address, UINT32_C(0x3000));
  T64_CHECK_INT(t64_video_copy(machine, packet, sizeof packet, &length),
                T64_STATUS_OK);
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(166), &stop));
  T64_CHECK_INT(t64_video_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.source_address, expected_second_origin);
  T64_CHECK_INT(t64_video_copy(machine, packet, sizeof packet, &length),
                T64_STATUS_OK);
  if (expected_second_origin == UINT32_C(0x3000)) {
    T64_CHECK(advance_guest_to_tick(machine, UINT64_C(249), &stop));
    T64_CHECK_INT(t64_video_info(machine, &info), T64_STATUS_OK);
    T64_CHECK_INT(info.source_address, UINT32_C(0x4000));
  }
  t64_machine_destroy(machine);
}

static void check_serrate_latch_current(uint64_t write_tick,
                                        uint32_t initial_control,
                                        uint32_t next_control,
                                        uint32_t expected_current) {
  t64_machine *machine = create_vi_v8_machine();
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  t64_video_packet_info info = {0};
  uint8_t packet[48];
  size_t length = 0u;
  uint32_t current = UINT32_MAX;
  const uint64_t first_boundary = UINT64_C(83);
  const uint64_t latch_boundary = write_tick < first_boundary
                                      ? first_boundary
                                      : UINT64_C(166);

  if (machine == NULL) return;
  T64_CHECK(seed_vi_source(machine));
  T64_CHECK(configure_vi(machine, initial_control, UINT32_C(0x3000), 4u, 0u,
                         8u, 15u, 4u, UINT32_C(0x00020008),
                         UINT32_C(0x400), UINT32_C(0x400)));
  if (write_tick >= first_boundary) {
    T64_CHECK(advance_guest_to_tick(machine, write_tick, &stop));
    T64_CHECK_INT(t64_video_info(machine, &info), T64_STATUS_OK);
    T64_CHECK_INT(info.control, initial_control);
    T64_CHECK_INT(t64_video_copy(machine, packet, sizeof packet, &length),
                  T64_STATUS_OK);
    T64_CHECK_INT(length, sizeof packet);
  } else {
    T64_CHECK(advance_guest_to_tick(machine, write_tick, &stop));
  }
  T64_CHECK(vi_write(machine, UINT32_C(0x00), next_control));
  T64_CHECK(advance_guest_to_tick(machine, latch_boundary, &stop));
  T64_CHECK_INT(t64_video_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.control, initial_control);
  T64_CHECK_INT(t64_video_copy(machine, packet, sizeof packet, &length),
                T64_STATUS_OK);
  T64_CHECK_INT(length, sizeof packet);
  T64_CHECK_INT(machine->vi.active_registers[0], next_control);
  T64_CHECK_INT(t64_machine_bus_read(machine, VI_BASE + UINT32_C(0x10), 4u,
                                     &current), T64_STATUS_OK);
  T64_CHECK_INT(current, expected_current);
  t64_machine_destroy(machine);
}

static void case_vi_serrate_latch_current_boundaries(void) {
  check_serrate_latch_current(UINT64_C(82), 2u, UINT32_C(0x42), 1u);
  check_serrate_latch_current(UINT64_C(82), UINT32_C(0x42), 2u, 0u);
  check_serrate_latch_current(UINT64_C(83), 2u, UINT32_C(0x42), 0u);
  check_serrate_latch_current(UINT64_C(83), UINT32_C(0x42), 2u, 0u);
  check_serrate_latch_current(UINT64_C(84), 2u, UINT32_C(0x42), 0u);
  check_serrate_latch_current(UINT64_C(84), UINT32_C(0x42), 2u, 0u);
}

static void case_vi_next_field_latch_before_on_after(void) {
  check_latch_write_tick(UINT64_C(82), UINT32_C(0x4000));
  check_latch_write_tick(UINT64_C(83), UINT32_C(0x3000));
  check_latch_write_tick(UINT64_C(84), UINT32_C(0x3000));
}

static void case_vi_disabled_blank_empty_windows(void) {
  t64_machine *machine = create_vi_v8_machine();
  t64_machine *zero_timer = create_vi_v8_machine();
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  t64_video_packet_info info = {0};
  uint8_t packet[48];
  size_t length = 0u;

  if (machine == NULL || zero_timer == NULL) {
    if (machine != NULL) t64_machine_destroy(machine);
    if (zero_timer != NULL) t64_machine_destroy(zero_timer);
    return;
  }
  T64_CHECK(seed_vi_source(machine));
  T64_CHECK(configure_vi_witness(machine));
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(21), &stop));
  T64_CHECK(vi_write(machine, UINT32_C(0x00), 0u));
  T64_CHECK(machine->vi.enabled);
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(83), &stop));
  T64_CHECK(!machine->vi.enabled);
  T64_CHECK_INT(t64_video_copy(machine, packet, sizeof packet, &length),
                T64_STATUS_OK);
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(166), &stop));
  T64_CHECK_INT(t64_video_info(machine, &info), T64_STATUS_NOT_READY);

  T64_CHECK(seed_vi_source(zero_timer));
  T64_CHECK(configure_vi(zero_timer, 2u, UINT32_C(0x3000), 4u, 0u, 0u,
                         15u, 4u, UINT32_C(0x00020008), UINT32_C(0x400),
                         UINT32_C(0x400)));
  T64_CHECK(!zero_timer->vi.enabled);
  T64_CHECK(advance_guest_to_tick(zero_timer, UINT64_C(100), &stop));
  T64_CHECK_INT(t64_video_info(zero_timer, &info), T64_STATUS_NOT_READY);
  t64_machine_destroy(machine);
  t64_machine_destroy(zero_timer);
}

static void case_vi_geometry_maximum_span(void) {
  t64_machine *machine = create_vi_v8_machine();
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  t64_video_packet_info info = {0};

  if (machine == NULL) return;
  T64_CHECK(configure_vi(machine, 2u, UINT32_C(0x3000), 320u, 0u, 8u, 15u,
                         320u, UINT32_C(0x000001e0), UINT32_C(0x400),
                         UINT32_C(0x400)));
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(83), &stop));
  T64_CHECK_INT(t64_video_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.width, 320u);
  T64_CHECK_INT(info.height, 240u);
  T64_CHECK_INT(info.stride_bytes, 1280u);
  T64_CHECK_INT(info.source_bytes, (size_t)153600u);
  T64_CHECK_INT(info.packet_bytes, (size_t)307200u);
  t64_machine_destroy(machine);
}

static void case_vi_image_span_overflow_refusal(void) {
  t64_machine *machine = create_vi_v8_machine();
  t64_machine *last_valid = create_vi_v8_machine();
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  t64_status status;

  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  t64_video_packet_info info = {0};
  uint8_t packet[48];
  size_t length = 0u;

  if (machine == NULL || last_valid == NULL) {
    if (machine != NULL) t64_machine_destroy(machine);
    if (last_valid != NULL) t64_machine_destroy(last_valid);
    return;
  }
  T64_CHECK(seed_vi_source_at(last_valid, UINT32_C(0x003fffe8)));
  T64_CHECK(configure_vi(last_valid, 2u, UINT32_C(0x003fffe8), 4u, 0u, 8u,
                         15u, 4u, UINT32_C(0x00020008), UINT32_C(0x400),
                         UINT32_C(0x400)));
  T64_CHECK(advance_guest_to_tick(last_valid, UINT64_C(83), &stop));
  T64_CHECK_INT(t64_video_info(last_valid, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.source_address, UINT32_C(0x003fffe8));
  T64_CHECK_INT(info.source_bytes, (size_t)24u);
  T64_CHECK_INT(t64_video_copy(last_valid, packet, sizeof packet, &length),
                T64_STATUS_OK);
  T64_CHECK_INT(length, sizeof packet);

  stop = (t64_stop_result){UINT32_MAX, UINT64_MAX};
  T64_CHECK(seed_vi_source(machine));
  T64_CHECK(configure_vi_witness(machine));
  T64_CHECK(vi_write(machine, UINT32_C(0x00), 0u));
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(83), &stop));
  T64_CHECK(!machine->vi.enabled);
  T64_CHECK_INT(t64_video_copy(machine, packet, sizeof packet, &length),
                T64_STATUS_OK);
  T64_CHECK(vi_write(machine, UINT32_C(0x04), UINT32_C(0x003ffff0)));
  T64_CHECK_INT(t64_machine_digest(machine, before), T64_STATUS_OK);
  status = t64_machine_bus_write(machine, VI_BASE, 4u, UINT32_C(2));
  T64_CHECK_INT(status, T64_STATUS_BUS_DEVICE_REFUSED);
  T64_CHECK_INT(t64_machine_digest(machine, after), T64_STATUS_OK);
  T64_CHECK(memcmp(before, after, sizeof before) == 0);
  T64_CHECK(!machine->vi.enabled);
  t64_machine_destroy(machine);
  t64_machine_destroy(last_valid);
}

static void case_vi_rational_whole_split_phase(void) {
  t64_machine *whole = create_vi_v8_machine();
  t64_machine *split = create_vi_v8_machine();
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  uint8_t whole_digest[T64_DIGEST_SIZE];
  uint8_t split_digest[T64_DIGEST_SIZE];
  uint8_t whole_packet[48];
  uint8_t split_packet[48];
  t64_video_packet_info whole_info = {0};
  t64_video_packet_info split_info = {0};
  size_t whole_length = 0u;
  size_t split_length = 0u;

  if (whole == NULL || split == NULL) {
    if (whole != NULL) t64_machine_destroy(whole);
    if (split != NULL) t64_machine_destroy(split);
    return;
  }
  T64_CHECK(seed_vi_source(whole));
  T64_CHECK(seed_vi_source(split));
  T64_CHECK(configure_vi_witness(whole));
  T64_CHECK(configure_vi_witness(split));
  T64_CHECK(advance_guest_to_tick(whole, UINT64_C(83), &stop));
  T64_CHECK(advance_guest_to_tick(split, UINT64_C(21), &stop));
  T64_CHECK(advance_guest_to_tick(split, UINT64_C(42), &stop));
  T64_CHECK(advance_guest_to_tick(split, UINT64_C(63), &stop));
  T64_CHECK(advance_guest_to_tick(split, UINT64_C(83), &stop));
  T64_CHECK_INT(t64_machine_digest(whole, whole_digest), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(split, split_digest), T64_STATUS_OK);
  T64_CHECK(memcmp(whole_digest, split_digest, sizeof whole_digest) == 0);
  T64_CHECK_INT(t64_video_info(whole, &whole_info), T64_STATUS_OK);
  T64_CHECK_INT(t64_video_info(split, &split_info), T64_STATUS_OK);
  T64_CHECK_INT(whole_info.end_tick, split_info.end_tick);
  T64_CHECK_INT(t64_video_copy(whole, whole_packet, sizeof whole_packet,
                               &whole_length), T64_STATUS_OK);
  T64_CHECK_INT(t64_video_copy(split, split_packet, sizeof split_packet,
                               &split_length), T64_STATUS_OK);
  T64_CHECK_INT(whole_length, split_length);
  T64_CHECK(memcmp(whole_packet, split_packet, whole_length) == 0);
  t64_machine_destroy(whole);
  t64_machine_destroy(split);
}

static void case_vi_same_tick_dp_vi_mi_cpu_sp_order(void) {
  t64_machine *machine = create_vi_v8_machine();
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  t64_video_packet_info info = {0};
  t64_trace_record records[T64_TRACE_RECORD_CAPACITY];
  size_t count = 0u;
  size_t vi_frame = SIZE_MAX;
  size_t dp_command = SIZE_MAX;
  size_t pi_complete = SIZE_MAX;
  size_t sp_complete = SIZE_MAX;
  size_t mi_pi_pending = SIZE_MAX;
  size_t mi_vi_pending = SIZE_MAX;
  size_t cpu_retirement = SIZE_MAX;
  uint64_t dropped = UINT64_MAX;
  bool saturated = true;
  uint32_t pending = 0u;
  size_t i;

  if (machine == NULL) return;
  T64_CHECK(seed_vi_source(machine));
  T64_CHECK(configure_vi(machine, 2u, UINT32_C(0x3000), 4u, 20u, 20u, 0u,
                         4u, UINT32_C(0x00000014), UINT32_C(0x400),
                         UINT32_C(0x400)));
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x04040000), 4u,
                                      UINT32_C(0)), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x04040004), 4u,
                                      UINT32_C(0x3000)), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x04040008), 4u,
                                      UINT32_C(0)), T64_STATUS_OK);
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(8), &stop));
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x04600000), 4u,
                                      UINT32_C(0x2000)), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x04600004), 4u,
                                      UINT32_C(0x10000000)), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x0460000c), 4u,
                                      UINT32_C(7)), T64_STATUS_OK);
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(11), &stop));
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x100), 4u,
                                      UINT32_C(0x37000000)), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x104), 4u,
                                      UINT32_C(0xf80107c1)), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x04100000), 4u,
                                      UINT32_C(0x100)), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x04100004), 4u,
                                      UINT32_C(0x108)), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_trace_set_enabled(machine, true), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_advance(
                    machine,
                    &(t64_stop_request){T64_STOP_GUEST_TICK, UINT64_C(13)},
                    &stop), T64_STATUS_OK);
  T64_CHECK_INT(stop.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(stop.tick, UINT64_C(13));
  T64_CHECK_INT(t64_video_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.end_tick, UINT64_C(13));
  T64_CHECK_INT(info.commands_completed, UINT64_C(1));
  T64_CHECK_INT(info.sequence, UINT64_C(0));
  T64_CHECK_INT(machine->rdp.command_count, UINT64_C(1));
  for (i = 0u; i < 8u; i++) {
    T64_CHECK_INT(machine->sp_dmem[i], machine->ram[UINT32_C(0x3000) + i]);
    T64_CHECK_INT(machine->ram[UINT32_C(0x2000) + i],
                  ((const uint8_t *)machine->image_storage.data)[i]);
  }
  T64_CHECK_INT(t64_machine_bus_read(machine, MI_INTR, 4u, &pending),
                T64_STATUS_OK);
  T64_CHECK_INT(pending & UINT32_C(0x18), UINT32_C(0x18));
  T64_CHECK_INT(t64_machine_trace_drain(
                    machine, records, T64_TRACE_RECORD_CAPACITY, &count,
                    &dropped, &saturated), T64_STATUS_OK);
  T64_CHECK_INT(dropped, UINT64_C(0));
  T64_CHECK(!saturated);
  for (i = 0u; i < count; i++) {
    const t64_trace_record *record = &records[i];
    if (record->tick != UINT64_C(13)) continue;
    if (record->device == T64_TRACE_DEVICE_VI &&
        record->kind == T64_TRACE_KIND_VI_FRAME)
      vi_frame = i;
    if (record->device == T64_TRACE_DEVICE_RDP &&
        record->kind == T64_TRACE_KIND_RDP_COMMAND)
      dp_command = i;
    if (record->device == T64_TRACE_DEVICE_PI &&
        record->kind == T64_TRACE_KIND_PI_COMPLETE)
      pi_complete = i;
    if (record->device == T64_TRACE_DEVICE_SP &&
        record->kind == T64_TRACE_KIND_SP_COMPLETE)
      sp_complete = i;
    if (record->device == T64_TRACE_DEVICE_MI &&
        record->kind == T64_TRACE_KIND_MI_PENDING &&
        (record->value & UINT64_C(0xff)) == UINT64_C(0x10))
      mi_pi_pending = i;
    if (record->device == T64_TRACE_DEVICE_MI &&
        record->kind == T64_TRACE_KIND_MI_PENDING &&
        (record->value & UINT64_C(0xff)) == UINT64_C(0x18))
      mi_vi_pending = i;
    if (record->device == T64_TRACE_DEVICE_MACHINE &&
        record->kind == T64_TRACE_KIND_CPU_RETIREMENT)
      cpu_retirement = i;
  }
  T64_CHECK(sp_complete != SIZE_MAX);
  T64_CHECK(pi_complete != SIZE_MAX);
  T64_CHECK(mi_pi_pending != SIZE_MAX);
  T64_CHECK(vi_frame != SIZE_MAX);
  T64_CHECK(mi_vi_pending != SIZE_MAX);
  T64_CHECK(dp_command != SIZE_MAX);
  T64_CHECK(cpu_retirement != SIZE_MAX);
  if (sp_complete != SIZE_MAX && pi_complete != SIZE_MAX &&
      cpu_retirement != SIZE_MAX &&
      mi_pi_pending != SIZE_MAX && vi_frame != SIZE_MAX &&
      mi_vi_pending != SIZE_MAX && dp_command != SIZE_MAX) {
    T64_CHECK(mi_pi_pending < pi_complete);
    T64_CHECK(pi_complete < cpu_retirement);
    T64_CHECK(cpu_retirement < sp_complete);
    T64_CHECK(sp_complete < dp_command);
    T64_CHECK(dp_command < mi_vi_pending);
    T64_CHECK(mi_vi_pending < vi_frame);
  }
  t64_machine_destroy(machine);
}

static void case_vi_interleaved_machine_phase(void) {
  t64_machine *left = create_vi_v8_machine();
  t64_machine *right = create_vi_v8_machine();
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  uint8_t left_digest[T64_DIGEST_SIZE];
  uint8_t right_digest[T64_DIGEST_SIZE];

  if (left == NULL || right == NULL) {
    if (left != NULL) t64_machine_destroy(left);
    if (right != NULL) t64_machine_destroy(right);
    return;
  }
  T64_CHECK(seed_vi_source(left));
  T64_CHECK(seed_vi_source(right));
  T64_CHECK(configure_vi_witness(left));
  T64_CHECK(configure_vi_witness(right));
  T64_CHECK(advance_guest_to_tick(left, UINT64_C(21), &stop));
  T64_CHECK(advance_guest_to_tick(right, UINT64_C(83), &stop));
  T64_CHECK(advance_guest_to_tick(left, UINT64_C(83), &stop));
  T64_CHECK_INT(t64_machine_digest(left, left_digest), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(right, right_digest), T64_STATUS_OK);
  T64_CHECK(memcmp(left_digest, right_digest, sizeof left_digest) == 0);
  t64_machine_destroy(left);
  t64_machine_destroy(right);
}

static const t64_event *find_pending_vi_event(const t64_machine *machine);

static void case_vi_short_period_backpressure_resume(void) {
  t64_machine *machine = create_vi_v8_machine();
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  t64_video_packet_info info = {0};
  uint8_t packet[32];
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  size_t length = 0u;
  const t64_event *pending_event;
  t64_status status;

  if (machine == NULL) return;
  T64_CHECK(seed_vi_source(machine));
  T64_CHECK(configure_vi(machine, 2u, UINT32_C(0x3000), 4u, 0u, 1u, 0u,
                         4u, UINT32_C(0x00000004), UINT32_C(0x400),
                         UINT32_C(0x400)));
  status = t64_machine_advance(
      machine, &(t64_stop_request){T64_STOP_GUEST_TICK, UINT64_C(2)}, &stop);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(stop.reason, T64_STOP_REASON_RESOURCE_LIMIT);
  T64_CHECK_INT(stop.tick, UINT64_C(2));
  T64_CHECK_INT(machine->vi.packet_count, T64_VIDEO_QUEUE_CAPACITY);
  pending_event = find_pending_vi_event(machine);
  T64_CHECK(pending_event != NULL);
  if (pending_event != NULL) T64_CHECK_INT(pending_event->tick, UINT64_C(2));
  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);
  T64_CHECK_INT(t64_video_copy(machine, packet, sizeof packet, &length),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(2), &stop));
  T64_CHECK_INT(stop.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(machine->tick, UINT64_C(2));
  T64_CHECK_INT(machine->vi.packet_count, T64_VIDEO_QUEUE_CAPACITY);
  T64_CHECK_INT(machine->vi.field, UINT32_C(3));
  T64_CHECK_INT(t64_video_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.sequence, UINT64_C(1));
  T64_CHECK_INT(info.end_tick, UINT64_C(2));
  T64_CHECK_INT(t64_video_copy(machine, packet, sizeof packet, &length),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_video_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.sequence, UINT64_C(2));
  T64_CHECK_INT(info.end_tick, UINT64_C(2));
  t64_machine_destroy(machine);
}

static void case_vi_old_profiles_unchanged(void) {
  const t64_profile_descriptor *v2 = NULL;
  const t64_profile_descriptor *v7 = NULL;
  const t64_profile_descriptor *v8 = NULL;
  T64_CHECK_INT(t64_profile_lookup(TEST_PROFILE_ID, &v2), T64_STATUS_OK);
  T64_CHECK_INT(t64_profile_lookup(
                    "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-depth-v7", &v7),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_profile_lookup(VI_V8_PROFILE_ID, &v8), T64_STATUS_OK);
  T64_CHECK(v2 != NULL && v7 != NULL && v8 != NULL);
  if (v2 != NULL) {
    T64_CHECK_INT(v2->descriptor_version, 6u);
    T64_CHECK_INT(v2->machine_model_version, 11u);
    T64_CHECK_INT(v2->state_serialization_version, 16u);
    T64_CHECK_STR(v2->vi_clock_policy,
                  "rsp62500000-ntsc48681812-rational-v1");
    T64_CHECK_STR(v2->vi_output_policy,
                  "rgba5551-nearest-no-filter-rgba8-two-packet-v1");
  }
  if (v7 != NULL) {
    T64_CHECK_INT(v7->descriptor_version, 10u);
    T64_CHECK_INT(v7->machine_model_version, 16u);
    T64_CHECK_INT(v7->state_serialization_version, 21u);
    T64_CHECK_STR(v7->vi_clock_policy,
                  "rsp62500000-ntsc48681812-rational-v1");
    T64_CHECK_STR(v7->vi_output_policy,
                  "rgba5551-rgba8888-nearest-no-filter-rgba8-two-packet-v1");
  }
  if (v8 != NULL) {
    T64_CHECK_INT(v8->descriptor_version, 10u);
    T64_CHECK_INT(v8->machine_model_version, 17u);
    T64_CHECK_INT(v8->state_serialization_version, 22u);
    T64_CHECK_STR(v8->vi_clock_policy,
                  "rsp62500000-ntsc48681812-halfline-rational-v2");
    T64_CHECK_STR(v8->vi_output_policy,
                  "rgba5551-rgba8888-rawrw-q10step-serrate2-nextfield-nf-v2");
  }
}

static void case_vi_hsync_latch_field_continuity(void) {
  t64_machine *machine = create_vi_v8_machine();
  t64_video_packet_info info = {0};
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  uint8_t packet[48];
  size_t length = 0u;
  t64_status status;
  bool configured;

  if (machine == NULL) return;
  configured = seed_vi_source(machine) &&
               configure_vi(machine, 2u, UINT32_C(0x3000), 4u, 0u, 8u,
                            4u, 4u, UINT32_C(0x00020008),
                            UINT32_C(0x400), UINT32_C(0x400));
  T64_CHECK(configured);
  if (!configured) {
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK(vi_write(machine, UINT32_C(0x1c), UINT32_C(5)));
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(26), &stop));
  status = t64_video_info(machine, &info);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK_INT(info.sequence, UINT64_C(0));
  T64_CHECK_INT(info.end_tick, UINT64_C(26));
  T64_CHECK_INT(info.width, UINT32_C(4));
  T64_CHECK_INT(info.height, UINT32_C(3));
  T64_CHECK_INT(t64_video_copy(machine, packet, sizeof packet, &length),
                T64_STATUS_OK);
  T64_CHECK_INT(length, sizeof packet);

  /* H_SYNC=5 is latched at the accepted first field boundary. */
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(57), &stop));
  status = t64_video_info(machine, &info);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK_INT(info.sequence, UINT64_C(1));
  T64_CHECK_INT(info.end_tick, UINT64_C(57));
  T64_CHECK_INT(info.h_sync, UINT32_C(5));
  T64_CHECK_INT(t64_video_copy(machine, packet, sizeof packet, &length),
                T64_STATUS_OK);
  T64_CHECK_INT(length, sizeof packet);
  T64_CHECK(machine->vi.enabled);
  t64_machine_destroy(machine);
}

static void case_vi_vsync_latch_field_continuity(void) {
  t64_machine *machine = create_vi_v8_machine();
  t64_video_packet_info info = {0};
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  uint8_t packet[48];
  size_t length = 0u;
  bool configured;

  if (machine == NULL) return;
  configured = seed_vi_source(machine) &&
               configure_vi(machine, 2u, UINT32_C(0x3000), 4u, 0u, 8u,
                            4u, 4u, UINT32_C(0x00020008),
                            UINT32_C(0x400), UINT32_C(0x400));
  T64_CHECK(configured);
  if (!configured) {
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK(vi_write(machine, UINT32_C(0x18), UINT32_C(10)));
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(26), &stop));
  T64_CHECK_INT(t64_video_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.sequence, UINT64_C(0));
  T64_CHECK_INT(info.end_tick, UINT64_C(26));
  T64_CHECK_INT(info.v_sync, UINT32_C(8));
  T64_CHECK_INT(t64_video_copy(machine, packet, sizeof packet, &length),
                T64_STATUS_OK);
  T64_CHECK_INT(length, sizeof packet);
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(58), &stop));
  T64_CHECK_INT(t64_video_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.sequence, UINT64_C(1));
  T64_CHECK_INT(info.end_tick, UINT64_C(58));
  T64_CHECK_INT(info.v_sync, UINT32_C(10));
  T64_CHECK_INT(t64_video_copy(machine, packet, sizeof packet, &length),
                T64_STATUS_OK);
  T64_CHECK_INT(length, sizeof packet);
  T64_CHECK(machine->vi.enabled);
  t64_machine_destroy(machine);
}

static void case_vi_decoded_geometry_endpoints(void) {
  t64_machine *machine = create_vi_v8_machine();
  t64_video_packet_info info = {0};
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};

  if (machine == NULL) return;
  T64_CHECK(seed_vi_source(machine));
  T64_CHECK(vi_write(machine, UINT32_C(0x04), UINT32_C(0x3000)));
  T64_CHECK(vi_write(machine, UINT32_C(0x08), UINT32_C(4)));
  T64_CHECK(vi_write(machine, UINT32_C(0x18), UINT32_C(8)));
  T64_CHECK(vi_write(machine, UINT32_C(0x1c), UINT32_C(15)));
  T64_CHECK(vi_write(machine, UINT32_C(0x24), UINT32_C(0x00020006)));
  T64_CHECK(vi_write(machine, UINT32_C(0x28), UINT32_C(0x0004000a)));
  T64_CHECK(vi_write(machine, UINT32_C(0x30), UINT32_C(0x00000400)));
  T64_CHECK(vi_write(machine, UINT32_C(0x34), UINT32_C(0x00000400)));
  T64_CHECK(vi_write(machine, UINT32_C(0x00), UINT32_C(2)));
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(83), &stop));
  T64_CHECK_INT(t64_video_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.width, UINT32_C(4));
  T64_CHECK_INT(info.height, UINT32_C(3));
  T64_CHECK_INT(info.h_start, UINT32_C(2));
  T64_CHECK_INT(info.h_end, UINT32_C(6));
  T64_CHECK_INT(info.v_start, UINT32_C(4));
  T64_CHECK_INT(info.v_end, UINT32_C(10));
  t64_machine_destroy(machine);
}

static const t64_event *find_pending_vi_event(const t64_machine *machine) {
  size_t offset;

  for (offset = 0u; offset < machine->queue_count; offset++) {
    size_t slot = (machine->queue_head + offset) % T64_EVENT_QUEUE_CAPACITY;
    const t64_event *event = &machine->events[slot];

    if (event->device == T64_VI_DEVICE &&
        event->kind == T64_VI_EVENT_FIELD &&
        event->token == machine->vi.event_generation) {
      return event;
    }
  }
  return NULL;
}

static bool advance_guest_to_tick(t64_machine *machine, uint64_t tick,
                                  t64_stop_result *stop_out) {
  size_t attempt;

  for (attempt = 0u; attempt < 512u; attempt++) {
    t64_status status = t64_machine_advance(
        machine, &(t64_stop_request){T64_STOP_GUEST_TICK, tick}, stop_out);

    T64_CHECK_INT(status, T64_STATUS_OK);
    if (status != T64_STATUS_OK) return false;
    if (stop_out->reason != T64_STOP_REASON_RESOURCE_LIMIT) {
      return stop_out->reason == T64_STOP_REASON_BOUND;
    }
    if (machine->tick == tick && machine->vi.packet_count >=
                                     T64_VIDEO_QUEUE_CAPACITY) {
      return false;
    }
  }
  T64_CHECK(false);
  return false;
}

static bool advance_until_video_full(t64_machine *machine, uint64_t tick,
                                     t64_stop_result *stop_out) {
  size_t attempt;

  for (attempt = 0u; attempt < 512u; attempt++) {
    t64_status status = t64_machine_advance(
        machine, &(t64_stop_request){T64_STOP_GUEST_TICK, tick}, stop_out);

    T64_CHECK_INT(status, T64_STATUS_OK);
    if (status != T64_STATUS_OK) return false;
    if (machine->vi.packet_count >= T64_VIDEO_QUEUE_CAPACITY) {
      return stop_out->reason == T64_STOP_REASON_RESOURCE_LIMIT;
    }
    if (stop_out->reason != T64_STOP_REASON_RESOURCE_LIMIT) return false;
  }
  T64_CHECK(false);
  return false;
}

static bool advance_to_next_vi_field(t64_machine *machine) {
  const t64_event *event = find_pending_vi_event(machine);
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  uint64_t tick;
  uint32_t field;
  size_t attempt;

  if (event == NULL) return false;
  tick = event->tick;
  field = machine->vi.field;
  for (attempt = 0u; attempt < 512u; attempt++) {
    t64_status status = t64_machine_advance(
        machine, &(t64_stop_request){T64_STOP_GUEST_TICK, tick}, &stop);

    T64_CHECK_INT(status, T64_STATUS_OK);
    if (status != T64_STATUS_OK) return false;
    if (machine->vi.field != field) break;
    if (stop.reason != T64_STOP_REASON_RESOURCE_LIMIT) return false;
  }
  if (attempt == 512u) T64_CHECK(false);
  T64_CHECK_INT(machine->vi.field, field + 1u);
  return machine->vi.field == field + 1u;
}

static bool copy_packet(t64_machine *machine, t64_video_packet_info *info_out,
                        uint8_t *bytes_out, size_t capacity,
                        size_t *length_out) {
  t64_status status;

  status = t64_video_info(machine, info_out);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) return false;
  status = t64_video_copy(machine, bytes_out, capacity, length_out);
  T64_CHECK_INT(status, T64_STATUS_OK);
  return status == T64_STATUS_OK;
}

static void check_video_copy_refusals(t64_machine *machine,
                                      const t64_video_packet_info *expected) {
  union {
    size_t length;
    uint8_t bytes[sizeof expected_packet];
  } overlapping;
  uint8_t destination[sizeof expected_packet];
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  t64_video_packet_info after = {0};
  size_t length;
  size_t i;

  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);

  length = (size_t)0x1234u;
  T64_CHECK_INT(t64_video_copy(machine, NULL, 1u, &length),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK_INT(length, (size_t)0x1234u);

  (void)memset(destination, 0xa5, sizeof destination);
  length = (size_t)0x5678u;
  T64_CHECK_INT(t64_video_copy(machine, destination,
                               sizeof destination - 1u, &length),
                T64_STATUS_BUFFER_TOO_SMALL);
  T64_CHECK_INT(length, sizeof expected_packet);
  for (i = 0u; i < sizeof destination; i++)
    T64_CHECK_INT(destination[i], UINT8_C(0xa5));

  (void)memset(overlapping.bytes, 0x5a, sizeof overlapping.bytes);
  T64_CHECK_INT(t64_video_copy(machine, overlapping.bytes,
                               sizeof overlapping.bytes,
                               &overlapping.length),
                T64_STATUS_INVALID_ARGUMENT);
  for (i = 0u; i < sizeof overlapping.bytes; i++)
    T64_CHECK_INT(overlapping.bytes[i], UINT8_C(0x5a));

  T64_CHECK_INT(t64_video_info(machine, &after), T64_STATUS_OK);
  T64_CHECK_INT(after.sequence, expected->sequence);
  T64_CHECK_INT(after.packet_bytes, expected->packet_bytes);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);
}

static void case_cpu_authored_fill_packet_and_dp_lifecycle(void) {
  const t64_host_allocator allocator = {test_allocate, test_release, NULL};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t rom[TEST_ROM_CAPACITY];
  uint8_t packet[sizeof expected_packet];
  uint8_t discarded[64];
  size_t rom_length = 0u;
  size_t packet_length = 0u;
  t64_status status;
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {0u, 0u, 0u};
  t64_video_packet_info first_info = {0};
  uint32_t word = 0u;
  size_t i;
  bool packet_captured = false;

  T64_CHECK(load_authored_rom(rom, sizeof rom, &rom_length));
  if (rom_length == 0u) return;
  T64_CHECK_INT(t64_profile_lookup(TEST_PROFILE_ID, &profile), T64_STATUS_OK);
  if (profile == NULL) return;
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  if (machine == NULL) return;
  T64_CHECK_INT(t64_machine_admit_rom(machine, rom, rom_length),
                T64_STATUS_OK);

  status = t64_machine_advance(
      machine, &(t64_stop_request){T64_STOP_GUEST_TICK, TEST_COMPLETION_TICK},
      &stop);
  for (i = 0u; status == T64_STATUS_OK &&
               stop.reason == T64_STOP_REASON_RESOURCE_LIMIT && i < 512u;
       i++) {
    t64_video_packet_info queued;
    while (t64_video_info(machine, &queued) == T64_STATUS_OK) {
      size_t required = 0u;
      status = t64_video_copy(machine, NULL, 0u, &required);
      if (status != T64_STATUS_BUFFER_TOO_SMALL ||
          required > sizeof discarded) {
        status = T64_STATUS_RESOURCE_EXHAUSTED;
        break;
      }
      if (!packet_captured) check_video_copy_refusals(machine, &queued);
      status = t64_video_copy(machine, discarded, sizeof discarded,
                              &required);
      if (status != T64_STATUS_OK) break;
      if (!packet_captured) {
        first_info = queued;
        packet_length = required;
        (void)memcpy(packet, discarded, required);
        packet_captured = true;
      }
    }
    if (status != T64_STATUS_OK) break;
    status = t64_machine_advance(
        machine,
        &(t64_stop_request){T64_STOP_GUEST_TICK, TEST_COMPLETION_TICK},
        &stop);
  }
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(stop.reason, T64_STOP_REASON_GUEST_COMPLETION);
  if (status == T64_STATUS_OK &&
      stop.reason == T64_STOP_REASON_GUEST_COMPLETION) {
    T64_CHECK_INT(t64_machine_cpu_stop_detail(machine, &detail),
                  T64_STATUS_OK);
    T64_CHECK_INT(detail.pc, UINT32_C(0x80000558));
    T64_CHECK_INT(detail.opcode, UINT32_C(0x0000000d));
  }

  if (!packet_captured) {
    size_t required = 0u;
    T64_CHECK_INT(t64_video_info(machine, &first_info), T64_STATUS_OK);
    T64_CHECK_INT(t64_video_copy(machine, NULL, 0u, &required),
                  T64_STATUS_BUFFER_TOO_SMALL);
    T64_CHECK_INT(required, sizeof expected_packet);
    check_video_copy_refusals(machine, &first_info);
    T64_CHECK_INT(t64_video_copy(machine, packet, sizeof packet,
                                 &packet_length), T64_STATUS_OK);
    packet_captured = packet_length == sizeof packet;
  }
  T64_CHECK(packet_captured);
  T64_CHECK_INT(first_info.width, 4u);
  T64_CHECK_INT(first_info.height, 3u);
  T64_CHECK_INT(first_info.stride_bytes, 16u);
  T64_CHECK_INT(first_info.source_address, UINT32_C(0x3000));
  T64_CHECK_INT(first_info.source_format, T64_VIDEO_SOURCE_RGBA5551);
  T64_CHECK_INT(first_info.output_encoding, T64_VIDEO_ENCODING_RGBA8);
  T64_CHECK_INT(first_info.source_bytes, TEST_SOURCE_BYTES);
  T64_CHECK_INT(first_info.packet_bytes, sizeof expected_packet);
  T64_CHECK_INT(first_info.commands_completed, 6u);
  T64_CHECK_INT(first_info.fullsync_completed, 1u);
  T64_CHECK_INT(first_info.last_command_word,
                UINT64_C(0x2900000000000000));
  T64_CHECK_INT(first_info.start_tick, UINT64_C(538862));
  T64_CHECK_INT(first_info.end_tick, UINT64_C(538882));
  T64_CHECK_INT(first_info.end_tick - first_info.start_tick, 20u);
  T64_CHECK_INT(packet_length, sizeof expected_packet);
  T64_CHECK(memcmp(packet, expected_packet, sizeof packet) == 0);

  for (i = 0u; i < TEST_SOURCE_BYTES; i += 4u) {
    uint32_t actual = 0u;
    T64_CHECK_INT(t64_machine_bus_read(machine,
                                       UINT32_C(0x3000) + (uint32_t)i, 4u,
                                       &actual),
                  T64_STATUS_OK);
    T64_CHECK_INT(actual, UINT32_C(0xf80107c1));
  }
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04100008), 4u,
                                     &word),
                T64_STATUS_OK);
  T64_CHECK_INT(word, UINT32_C(0x3130));
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04100004), 4u,
                                     &word),
                T64_STATUS_OK);
  T64_CHECK_INT(word, UINT32_C(0x3130));
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04300008), 4u,
                                     &word),
                T64_STATUS_OK);
  T64_CHECK_INT(word, 0u);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x0430000c), 4u,
                                     &word),
                T64_STATUS_OK);
  T64_CHECK_INT(word, UINT32_C(0x20));
  t64_machine_destroy(machine);
}

static void case_copy_refusal_is_atomic(void) {
  const t64_host_allocator allocator = {test_allocate, test_release, NULL};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t rom[TEST_ROM_CAPACITY];
  size_t rom_length = 0u;
  t64_status status;
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  t64_video_packet_info queued = {0};
  size_t i;

  T64_CHECK(load_authored_rom(rom, sizeof rom, &rom_length));
  if (rom_length == 0u) return;
  T64_CHECK_INT(t64_profile_lookup(TEST_PROFILE_ID, &profile), T64_STATUS_OK);
  if (profile == NULL) return;
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  if (machine == NULL) return;
  T64_CHECK_INT(t64_machine_admit_rom(machine, rom, rom_length),
                T64_STATUS_OK);
  status = t64_machine_advance(
      machine, &(t64_stop_request){T64_STOP_GUEST_TICK, TEST_COMPLETION_TICK},
      &stop);
  for (i = 0u; status == T64_STATUS_OK && i < 512u &&
               t64_video_info(machine, &queued) == T64_STATUS_NOT_READY;
       i++) {
    status = t64_machine_advance(
        machine,
        &(t64_stop_request){T64_STOP_GUEST_TICK, TEST_COMPLETION_TICK},
        &stop);
  }
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK_INT(t64_video_info(machine, &queued), T64_STATUS_OK);
  if (t64_video_info(machine, &queued) != T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK_INT(queued.packet_bytes, sizeof expected_packet);
  check_video_copy_refusals(machine, &queued);
  t64_machine_destroy(machine);
}

static void case_v16_digest_covers_rdp_tmem_and_inherited_state(void) {
  t64_machine *machine = create_authored_machine();
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after_tmem[T64_DIGEST_SIZE];
  uint8_t after_tile[T64_DIGEST_SIZE];
  uint8_t after_vi[T64_DIGEST_SIZE];
  uint8_t after_warm[T64_DIGEST_SIZE];
  t64_status status;

  if (machine == NULL) return;
  T64_CHECK(t64_has_rdp_vi_profile(machine));
  T64_CHECK(t64_has_t1_policy(machine));
  T64_CHECK_INT(machine->profile.descriptor_version, 6u);
  T64_CHECK_INT(machine->profile.machine_model_version, 11u);
  T64_CHECK_INT(machine->profile.state_serialization_version, 16u);
  T64_CHECK_STR(machine->profile.rdp_work_policy,
                "dpc-fetch-effect-tmem-copy-row-event-1tick-v3");
  T64_CHECK_INT(t64_machine_digest(machine, before), T64_STATUS_OK);
  machine->rdp.tmem[0] = UINT8_C(0x5a);
  T64_CHECK_INT(t64_machine_digest(machine, after_tmem), T64_STATUS_OK);
  T64_CHECK(memcmp(before, after_tmem, sizeof before) != 0);
  machine->rdp.tmem[0] = 0u;
  machine->rdp.tiles[0].line_words++;
  T64_CHECK_INT(t64_machine_digest(machine, after_tile), T64_STATUS_OK);
  T64_CHECK(memcmp(before, after_tile, sizeof before) != 0);
  machine->rdp.tiles[0].line_words--;
  status = t64_machine_bus_write(machine, UINT32_C(0x04400004), 4u,
                                 UINT32_C(0x3000));
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(machine, after_vi), T64_STATUS_OK);
  T64_CHECK(memcmp(before, after_vi, sizeof before) != 0);

  machine->boot.seed_reset_version++;
  T64_CHECK_INT(t64_machine_digest(machine, after_warm), T64_STATUS_OK);
  T64_CHECK(memcmp(after_vi, after_warm, sizeof after_vi) != 0);
  t64_machine_destroy(machine);
}

static void case_drain_cadence_preserves_digest_and_packet_history(void) {
  t64_machine *early = create_authored_machine();
  t64_machine *delayed = create_authored_machine();
  t64_stop_result early_stop = {UINT32_MAX, UINT64_MAX};
  t64_stop_result delayed_stop = {UINT32_MAX, UINT64_MAX};
  t64_video_packet_info early_info[2] = {{0}};
  t64_video_packet_info delayed_info[2] = {{0}};
  uint8_t early_packets[2][sizeof expected_packet] = {{0}};
  uint8_t delayed_packets[2][sizeof expected_packet] = {{0}};
  uint8_t before_copy[T64_DIGEST_SIZE];
  uint8_t after_copy[T64_DIGEST_SIZE];
  uint8_t early_digest[T64_DIGEST_SIZE];
  uint8_t delayed_digest[T64_DIGEST_SIZE];
  size_t early_lengths[2] = {0u, 0u};
  size_t delayed_lengths[2] = {0u, 0u};
  size_t i;

  if (early == NULL || delayed == NULL) {
    t64_machine_destroy(early);
    t64_machine_destroy(delayed);
    return;
  }
  T64_CHECK(advance_guest_to_tick(early, UINT64_C(538881), &early_stop));
  T64_CHECK(
      advance_guest_to_tick(delayed, UINT64_C(538881), &delayed_stop));
  T64_CHECK(early->vi.enabled);
  T64_CHECK(delayed->vi.enabled);

  T64_CHECK(advance_to_next_vi_field(early));
  T64_CHECK_INT(t64_video_info(early, &early_info[0]), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(early, before_copy), T64_STATUS_OK);
  T64_CHECK_INT(t64_video_copy(early, early_packets[0],
                               sizeof early_packets[0], &early_lengths[0]),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(early, after_copy), T64_STATUS_OK);
  T64_CHECK(memcmp(before_copy, after_copy, sizeof before_copy) == 0);

  T64_CHECK(advance_to_next_vi_field(delayed));
  T64_CHECK(advance_to_next_vi_field(early));
  T64_CHECK(copy_packet(early, &early_info[1], early_packets[1],
                        sizeof early_packets[1], &early_lengths[1]));
  T64_CHECK(advance_to_next_vi_field(delayed));

  T64_CHECK_INT(early->tick, delayed->tick);
  T64_CHECK_INT(early->vi.field, delayed->vi.field);
  T64_CHECK_INT(early->vi.next_sequence, delayed->vi.next_sequence);
  T64_CHECK_INT(early->vi.packet_count, 0u);
  T64_CHECK_INT(delayed->vi.packet_count, 2u);
  T64_CHECK_INT(t64_machine_digest(early, early_digest), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(delayed, delayed_digest), T64_STATUS_OK);
  T64_CHECK(memcmp(early_digest, delayed_digest, sizeof early_digest) == 0);

  for (i = 0u; i < 2u; i++) {
    T64_CHECK(copy_packet(delayed, &delayed_info[i], delayed_packets[i],
                          sizeof delayed_packets[i], &delayed_lengths[i]));
    T64_CHECK_INT(delayed_info[i].sequence, early_info[i].sequence);
    T64_CHECK_INT(delayed_lengths[i], early_lengths[i]);
    T64_CHECK(memcmp(delayed_packets[i], early_packets[i],
                     early_lengths[i]) == 0);
  }
  T64_CHECK_INT(t64_machine_digest(delayed, after_copy), T64_STATUS_OK);
  T64_CHECK(memcmp(delayed_digest, after_copy, sizeof delayed_digest) == 0);
  t64_machine_destroy(early);
  t64_machine_destroy(delayed);
}

static void case_full_queue_copy_preserves_event_order(void) {
  t64_machine *machine = create_authored_machine();
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  const t64_event *pending_event;
  const t64_event *resumed_event;
  uint64_t pending_tick;
  uint64_t pending_sequence = UINT64_MAX;
  uint64_t machine_sequence_before_copy;
  uint64_t tick_before_copy;
  uint64_t field_before_resume;
  uint64_t frames_before_resume;
  size_t scheduler_count_before_copy;
  size_t length = 0u;
  uint8_t packet[sizeof expected_packet];
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  size_t vi_trace_index = SIZE_MAX;
  size_t synthetic_trace_index = SIZE_MAX;
  size_t i;
  t64_status status;

  if (machine == NULL) return;
  T64_CHECK(advance_until_video_full(machine, TEST_COMPLETION_TICK, &stop));
  status = T64_STATUS_OK;
  T64_CHECK_INT(stop.reason, T64_STOP_REASON_RESOURCE_LIMIT);
  T64_CHECK_INT(machine->vi.packet_count, T64_VIDEO_QUEUE_CAPACITY);

  pending_event = find_pending_vi_event(machine);
  T64_CHECK(pending_event != NULL);
  pending_tick = pending_event == NULL ? stop.tick : pending_event->tick;
  if (pending_event != NULL) pending_sequence = pending_event->sequence;
  T64_CHECK(machine->tick <= pending_tick);
  T64_CHECK_INT(t64_test_schedule_synthetic(
                    machine, pending_tick, T64_TEST_EVENT_FRAME, 1u),
                T64_STATUS_OK);
  machine_sequence_before_copy = machine->next_sequence;
  scheduler_count_before_copy = machine->queue_count;
  tick_before_copy = machine->tick;
  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);

  machine->trace_enabled = true;
  t64_trace_reset(machine);
  T64_CHECK_INT(t64_video_copy(machine, packet, sizeof packet, &length),
                T64_STATUS_OK);
  T64_CHECK_INT(length, sizeof expected_packet);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);
  T64_CHECK_INT(machine->next_sequence, machine_sequence_before_copy);
  T64_CHECK_INT(machine->queue_count, scheduler_count_before_copy);
  T64_CHECK_INT(machine->tick, tick_before_copy);

  resumed_event = find_pending_vi_event(machine);
  T64_CHECK(resumed_event != NULL);
  if (pending_sequence != UINT64_MAX && resumed_event != NULL) {
    T64_CHECK_INT(resumed_event->tick, pending_tick);
    T64_CHECK_INT(resumed_event->sequence, pending_sequence);
  }

  field_before_resume = machine->vi.field;
  frames_before_resume = machine->frame_event_count;
  status = t64_machine_advance(
      machine, &(t64_stop_request){T64_STOP_GUEST_TICK, pending_tick}, &stop);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(machine->vi.field, field_before_resume + 1u);
  T64_CHECK_INT(machine->frame_event_count, frames_before_resume + 1u);
  for (i = 0u; i < machine->trace_count; i++) {
    size_t slot = (machine->trace_head + i) % T64_TRACE_RECORD_CAPACITY;
    const t64_trace_record *record = &machine->trace_records[slot];

    if (record->tick != pending_tick) continue;
    if (record->device == T64_VI_DEVICE &&
        record->kind == T64_VI_EVENT_FIELD && record->value == 0u &&
        vi_trace_index == SIZE_MAX) {
      vi_trace_index = i;
    }
    if (record->device == T64_DEVICE_SYNTHETIC_TEST &&
        record->kind == T64_TEST_EVENT_FRAME &&
        synthetic_trace_index == SIZE_MAX) {
      synthetic_trace_index = i;
    }
  }
  T64_CHECK(vi_trace_index != SIZE_MAX);
  T64_CHECK(synthetic_trace_index != SIZE_MAX);
  T64_CHECK(vi_trace_index < synthetic_trace_index);
  t64_machine_destroy(machine);
}

static void case_vi_interlace_full_height_source_span(void) {
  t64_machine *machine = create_vi_v8_machine();
  t64_video_packet_info info = {0};
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  uint8_t *packet = (uint8_t *)malloc((size_t)307200u);
  size_t length = 0u;
  uint32_t origin = UINT32_C(0x3000);
  size_t last_row_pixel = (size_t)239u * (size_t)320u * 4u;
  t64_status status;

  if (machine == NULL) {
    free(packet);
    return;
  }
  T64_CHECK(packet != NULL);
  if (packet == NULL) {
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK(seed_full_vi_source(machine, origin));
  T64_CHECK(configure_vi(machine, 2u, origin, 320u, 0u, 8u, 15u, 4u,
                         UINT32_C(0x00020008), UINT32_C(0x400),
                         UINT32_C(0x400)));
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(83), &stop));
  T64_CHECK_INT(t64_video_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.sequence, UINT64_C(0));
  T64_CHECK_INT(t64_video_copy(machine, packet, (size_t)307200u, &length),
                T64_STATUS_OK);

  T64_CHECK(stage_full_vi_geometry(machine, origin, UINT32_C(0x42),
                                   UINT32_C(0x00000400)));
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(166), &stop));
  T64_CHECK_INT(t64_video_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.sequence, UINT64_C(1));
  T64_CHECK_INT(info.control, UINT32_C(2));
  T64_CHECK_INT(t64_video_copy(machine, packet, (size_t)307200u, &length),
                T64_STATUS_OK);

  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(249), &stop));
  status = t64_video_info(machine, &info);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) {
    free(packet);
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK_INT(info.width, UINT32_C(320));
  T64_CHECK_INT(info.height, UINT32_C(240));
  T64_CHECK_INT(info.field, UINT32_C(2));
  T64_CHECK_INT(info.parity, UINT32_C(0));
  T64_CHECK_INT(info.source_bytes, (size_t)306560u);
  T64_CHECK_INT(info.packet_bytes, (size_t)307200u);
  T64_CHECK_INT(t64_video_copy(machine, packet, (size_t)307200u, &length),
                T64_STATUS_OK);
  T64_CHECK_INT(length, (size_t)307200u);
  T64_CHECK_INT(packet[0], UINT8_C(255));
  T64_CHECK_INT(packet[1], UINT8_C(0));
  T64_CHECK_INT(packet[last_row_pixel], UINT8_C(0));
  T64_CHECK_INT(packet[last_row_pixel + 1u], UINT8_C(255));
  T64_CHECK_INT(packet[last_row_pixel + 2u], UINT8_C(0));

  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(332), &stop));
  status = t64_video_info(machine, &info);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) {
    free(packet);
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK_INT(info.width, UINT32_C(320));
  T64_CHECK_INT(info.height, UINT32_C(240));
  T64_CHECK_INT(info.field, UINT32_C(3));
  T64_CHECK_INT(info.parity, UINT32_C(1));
  T64_CHECK_INT(info.source_bytes, (size_t)307200u);
  T64_CHECK_INT(t64_video_copy(machine, packet, (size_t)307200u, &length),
                T64_STATUS_OK);
  T64_CHECK_INT(packet[0], UINT8_C(0));
  T64_CHECK_INT(packet[1], UINT8_C(0));
  T64_CHECK_INT(packet[2], UINT8_C(255));
  T64_CHECK_INT(packet[last_row_pixel], UINT8_C(255));
  T64_CHECK_INT(packet[last_row_pixel + 1u], UINT8_C(255));
  T64_CHECK_INT(packet[last_row_pixel + 2u], UINT8_C(255));
  free(packet);
  t64_machine_destroy(machine);
}

static void case_vi_progressive_high_offset_source_span(void) {
  t64_machine *machine = create_vi_v8_machine();
  t64_machine *beyond = create_vi_v8_machine();
  t64_video_packet_info info = {0};
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  uint8_t *packet = (uint8_t *)malloc((size_t)307200u);
  size_t length = 0u;
  uint32_t origin;
  uint32_t beyond_origin;
  const uint32_t y_offset_scale = UINT32_C(0x04000400);
  t64_status status;

  if (machine == NULL || beyond == NULL) {
    if (machine != NULL) t64_machine_destroy(machine);
    if (beyond != NULL) t64_machine_destroy(beyond);
    free(packet);
    return;
  }
  T64_CHECK(packet != NULL);
  if (packet == NULL) {
    t64_machine_destroy(machine);
    t64_machine_destroy(beyond);
    return;
  }
  origin = (uint32_t)(machine->profile.rdram_bytes - UINT64_C(154240));
  beyond_origin = origin + UINT32_C(8);
  T64_CHECK(seed_full_vi_source(machine, origin));
  T64_CHECK(configure_vi(machine, 2u, origin, 320u, 0u, 8u, 15u, 4u,
                         UINT32_C(0x00020008), UINT32_C(0x400),
                         UINT32_C(0x400)));
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(83), &stop));
  status = t64_video_info(machine, &info);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) {
    free(packet);
    t64_machine_destroy(machine);
    t64_machine_destroy(beyond);
    return;
  }
  T64_CHECK_INT(info.width, UINT32_C(4));
  T64_CHECK_INT(info.height, UINT32_C(3));
  T64_CHECK_INT(t64_video_copy(machine, packet, (size_t)307200u, &length),
                T64_STATUS_OK);
  T64_CHECK(stage_full_vi_geometry(machine, origin, 2u, y_offset_scale));
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(166), &stop));
  T64_CHECK_INT(t64_video_copy(machine, packet, (size_t)307200u, &length),
                T64_STATUS_OK);
  T64_CHECK(advance_guest_to_tick(machine, UINT64_C(249), &stop));
  status = t64_video_info(machine, &info);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) {
    free(packet);
    t64_machine_destroy(machine);
    t64_machine_destroy(beyond);
    return;
  }
  T64_CHECK_INT(info.width, UINT32_C(320));
  T64_CHECK_INT(info.height, UINT32_C(240));
  T64_CHECK_INT(info.source_y_offset, UINT32_C(1024));
  T64_CHECK_INT(info.source_bytes, (size_t)154240u);
  T64_CHECK_INT(origin + info.source_bytes,
                (uint32_t)machine->profile.rdram_bytes);
  T64_CHECK_INT(t64_video_copy(machine, packet, (size_t)307200u, &length),
                T64_STATUS_OK);
  T64_CHECK_INT(packet[0], UINT8_C(0));
  T64_CHECK_INT(packet[2], UINT8_C(255));
  T64_CHECK_INT(packet[(size_t)239u * (size_t)320u * 4u],
                UINT8_C(255));

  T64_CHECK(seed_full_vi_source(beyond, beyond_origin));
  T64_CHECK(configure_vi(beyond, 2u, beyond_origin, 320u, 0u, 8u, 15u, 4u,
                         UINT32_C(0x00020008), UINT32_C(0x400),
                         UINT32_C(0x400)));
  T64_CHECK(advance_guest_to_tick(beyond, UINT64_C(83), &stop));
  status = t64_video_info(beyond, &info);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) {
    free(packet);
    t64_machine_destroy(machine);
    t64_machine_destroy(beyond);
    return;
  }
  T64_CHECK_INT(info.width, UINT32_C(4));
  T64_CHECK_INT(info.height, UINT32_C(3));
  T64_CHECK_INT(t64_video_copy(beyond, packet, (size_t)307200u, &length),
                T64_STATUS_OK);
  T64_CHECK(stage_full_vi_geometry(beyond, beyond_origin, 2u,
                                   y_offset_scale));
  T64_CHECK(advance_guest_to_tick(beyond, UINT64_C(166), &stop));
  T64_CHECK_INT(t64_video_copy(beyond, packet, (size_t)307200u, &length),
                T64_STATUS_OK);
  T64_CHECK(advance_guest_to_tick(beyond, UINT64_C(249), &stop));
  T64_CHECK_INT(t64_video_info(beyond, &info), T64_STATUS_NOT_READY);
  T64_CHECK(!beyond->vi.enabled);
  T64_CHECK_INT(beyond->vi.refusal_reason, T64_VI_REFUSAL_GEOMETRY);
  free(packet);
  t64_machine_destroy(machine);
  t64_machine_destroy(beyond);
}

static void case_vi_register_inventory_readback_reserved_policy(void);
static void case_vi_diagnostic_vs_standard_timing(void);
static void case_vi_geometry_rgba5551_rgba8888(void);
static void case_vi_geometry_minimum_window(void);
static void case_vi_scaled_offset_window_source_pitch(void);
static void case_vi_serrate_interlace_parity_origin(void);
static void case_vi_serrate_latch_current_boundaries(void);
static void case_vi_intr_current_write_clear_mi(void);
static void case_vi_next_field_latch_before_on_after(void);
static void case_vi_disabled_blank_empty_windows(void);
static void case_vi_geometry_maximum_span(void);
static void case_vi_image_span_overflow_refusal(void);
static void case_vi_rational_whole_split_phase(void);
static void case_vi_same_tick_dp_vi_mi_cpu_sp_order(void);
static void case_vi_interleaved_machine_phase(void);
static void case_vi_short_period_backpressure_resume(void);
static void case_vi_old_profiles_unchanged(void);
static void case_vi_hsync_latch_field_continuity(void);
static void case_vi_vsync_latch_field_continuity(void);
static void case_vi_decoded_geometry_endpoints(void);
static void case_vi_interlace_full_height_source_span(void);
static void case_vi_progressive_high_offset_source_span(void);
static void case_vi_active_intr_immediate_no_retroactive(void);
static void case_vi_aa_modes(void);
static void case_vi_resample_fraction(void);
static void case_vi_divot(void);
static void case_vi_gamma(void);
static void case_vi_gamma_dither(void);
static void case_vi_dither_filter(void);
static void case_vi_aa_dither_precedence(void);
static void case_vi_gamma_dither_requires_gamma(void);
static void case_vi_filter_stage_order(void);
static void case_vi_filter_wrong_expected(void);
static void case_vi_rdp_fill_coverage_to_filtered_packet(void);
static void case_vi_v8_filter_ignore(void);

typedef struct {
  const char *name;
  void (*run)(void);
  const char *suite;
} test_case;

static const test_case cases[] = {
    {"cpu-authored-fill-packet-and-dp-lifecycle",
     case_cpu_authored_fill_packet_and_dp_lifecycle, "fill"},
    {"copy-refusal-is-atomic", case_copy_refusal_is_atomic, "fill"},
    {"v16-digest-covers-rdp-tmem-and-inherited-state",
     case_v16_digest_covers_rdp_tmem_and_inherited_state, "fill"},
    {"drain-cadence-preserves-digest-and-packet-history",
     case_drain_cadence_preserves_digest_and_packet_history, "fill"},
    {"full-queue-copy-preserves-event-order",
     case_full_queue_copy_preserves_event_order, "fill"},
    {"vi-register-inventory-readback-reserved-policy",
     case_vi_register_inventory_readback_reserved_policy, "vi-timing"},
    {"vi-ntsc-rational-field-timing", case_vi_ntsc_rational_field_timing,
     "vi-timing"},
    {"vi-diagnostic-vs-standard-timing",
     case_vi_diagnostic_vs_standard_timing, "vi-timing"},
    {"vi-geometry-rgba5551-rgba8888",
     case_vi_geometry_rgba5551_rgba8888, "vi-timing"},
    {"vi-geometry-minimum-window", case_vi_geometry_minimum_window,
     "vi-timing"},
    {"vi-scaled-offset-window-source-pitch",
     case_vi_scaled_offset_window_source_pitch, "vi-timing"},
    {"vi-serrate-interlace-parity-origin",
     case_vi_serrate_interlace_parity_origin, "vi-timing"},
    {"vi-serrate-latch-current-boundaries",
     case_vi_serrate_latch_current_boundaries, "vi-timing"},
    {"vi-intr-current-write-clear-mi", case_vi_intr_current_write_clear_mi,
     "vi-timing"},
    {"vi-active-intr-immediate-no-retroactive",
     case_vi_active_intr_immediate_no_retroactive, "vi-timing"},
    {"vi-next-field-latch-before-on-after",
     case_vi_next_field_latch_before_on_after, "vi-timing"},
    {"vi-disabled-blank-empty-windows",
     case_vi_disabled_blank_empty_windows, "vi-timing"},
    {"vi-geometry-maximum-span", case_vi_geometry_maximum_span, "vi-timing"},
    {"vi-image-span-overflow-refusal", case_vi_image_span_overflow_refusal,
     "vi-timing"},
    {"vi-rational-whole-split-phase", case_vi_rational_whole_split_phase,
     "vi-timing"},
    {"vi-same-tick-dp-vi-mi-cpu-sp-order",
     case_vi_same_tick_dp_vi_mi_cpu_sp_order, "vi-timing"},
    {"vi-interleaved-machine-phase", case_vi_interleaved_machine_phase,
     "vi-timing"},
    {"vi-short-period-backpressure-resume",
     case_vi_short_period_backpressure_resume, "vi-timing"},
    {"vi-old-profiles-unchanged", case_vi_old_profiles_unchanged,
     "vi-timing"},
    {"vi-hsync-latch-field-continuity",
     case_vi_hsync_latch_field_continuity, "vi-timing"},
    {"vi-vsync-latch-field-continuity",
     case_vi_vsync_latch_field_continuity, "vi-timing"},
    {"vi-decoded-geometry-endpoints", case_vi_decoded_geometry_endpoints,
     "vi-timing"},
    {"vi-interlace-full-height-source-span",
     case_vi_interlace_full_height_source_span, "vi-timing"},
    {"vi-progressive-high-offset-source-span",
     case_vi_progressive_high_offset_source_span, "vi-timing"},
    {"vi-aa-modes", case_vi_aa_modes, "vi-filters"},
    {"vi-resample-fraction", case_vi_resample_fraction, "vi-filters"},
    {"vi-divot", case_vi_divot, "vi-filters"},
    {"vi-gamma", case_vi_gamma, "vi-filters"},
    {"vi-gamma-dither", case_vi_gamma_dither, "vi-filters"},
    {"vi-dither-filter", case_vi_dither_filter, "vi-filters"},
    {"vi-aa-dither-precedence", case_vi_aa_dither_precedence,
     "vi-filters"},
    {"vi-gamma-dither-requires-gamma",
     case_vi_gamma_dither_requires_gamma, "vi-filters"},
    {"vi-filter-stage-order", case_vi_filter_stage_order, "vi-filters"},
    {"vi-filter-wrong-expected", case_vi_filter_wrong_expected,
     "vi-filters"},
    {"vi-rdp-fill-coverage-to-filtered-packet",
     case_vi_rdp_fill_coverage_to_filtered_packet, "vi-filters"},
    {"vi-v8-filter-ignore", case_vi_v8_filter_ignore, "vi-filters"},
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
        return T64_TEST_FINISH(cases[i].suite);
      }
    }
    return 2;
  }
  if (argc == 3 && strcmp(argv[1], "--suite") == 0) {
    size_t selected = 0u;
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
      if (strcmp(argv[2], cases[i].suite) == 0) {
        cases[i].run();
        selected++;
      }
    }
    if (selected == 0u) return 2;
    return T64_TEST_FINISH(argv[2]);
  }
  if (argc != 1) return 2;
  for (i = 0u; i < sizeof cases / sizeof cases[0]; i++)
    if (strcmp(cases[i].suite, "fill") == 0) cases[i].run();
  return T64_TEST_FINISH("fill");
}
