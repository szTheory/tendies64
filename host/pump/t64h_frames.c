/*
 * The frame and audio helpers of the minimal host (Phase 12, plan 12-08, DR-14). See
 * t64h_frames.h for the contract.
 */
#include "t64h_frames.h"

#include <string.h>

const char *t64h_frame_status_name(t64h_frame_status status) {
  switch (status) {
    case T64H_FRAME_OK: return "ok";
    case T64H_FRAME_BAD_ARGUMENT: return "bad-argument";
    case T64H_FRAME_BAD_ENCODING: return "bad-encoding";
    case T64H_FRAME_BAD_GEOMETRY: return "bad-geometry";
    case T64H_FRAME_TOO_LARGE: return "too-large";
    case T64H_FRAME_SHORT_SOURCE: return "short-source";
    case T64H_FRAME_SHORT_DESTINATION: return "short-destination";
    default: return "unknown";
  }
}

t64h_frame_status t64h_frame_copy(const t64_video_packet_info *info, const uint8_t *packet,
                                  size_t packet_length, uint8_t *dest, size_t dest_capacity,
                                  size_t dest_pitch) {
  size_t width;
  size_t height;
  size_t stride;
  size_t row_bytes;
  size_t y;

  if (info == NULL || packet == NULL || dest == NULL) {
    return T64H_FRAME_BAD_ARGUMENT;
  }
  if (info->output_encoding != T64_VIDEO_ENCODING_RGBA8) {
    return T64H_FRAME_BAD_ENCODING;
  }
  if (info->width == 0u || info->height == 0u) {
    return T64H_FRAME_BAD_GEOMETRY;
  }
  if (info->width > T64H_FRAME_MAX_WIDTH || info->height > T64H_FRAME_MAX_HEIGHT) {
    return T64H_FRAME_TOO_LARGE;
  }
  width = (size_t)info->width;
  height = (size_t)info->height;
  stride = (size_t)info->stride_bytes;
  row_bytes = width * T64H_FRAME_BYTES_PER_PIXEL;
  if (stride < row_bytes || dest_pitch < row_bytes) {
    return T64H_FRAME_BAD_GEOMETRY;
  }
  /* The last row needs only its pixel bytes, not the stride padding after it. */
  if (packet_length < stride * (height - 1u) + row_bytes) {
    return T64H_FRAME_SHORT_SOURCE;
  }
  if (dest_capacity < dest_pitch * (height - 1u) + row_bytes) {
    return T64H_FRAME_SHORT_DESTINATION;
  }
  for (y = 0u; y < height; y++) {
    (void)memcpy(dest + y * dest_pitch, packet + y * stride, row_bytes);
  }
  return T64H_FRAME_OK;
}

void t64h_swap_s16be(const uint8_t *bytes, size_t sample_count, int16_t *samples) {
  size_t i;

  for (i = 0u; i < sample_count; i++) {
    const uint16_t unsigned_value = (uint16_t)(((uint16_t)bytes[2u * i] << 8) |
                                               (uint16_t)bytes[2u * i + 1u]);

    if (unsigned_value >= UINT16_C(0x8000)) {
      samples[i] = (int16_t)(-(int32_t)(UINT32_C(0x10000) - (uint32_t)unsigned_value));
    } else {
      samples[i] = (int16_t)unsigned_value;
    }
  }
}
