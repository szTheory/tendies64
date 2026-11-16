/* Bounded private pipe record for the authored RDP/VI tracer. */
#ifndef T64_VIDEO_OBSERVATION_H
#define T64_VIDEO_OBSERVATION_H

#include <stdint.h>

#define T64_VIDEO_OBSERVATION_MAGIC UINT32_C(0x54363456)
#define T64_VIDEO_OBSERVATION_VERSION UINT32_C(1)
#define T64_VIDEO_OBSERVATION_SOURCE_BYTES 24u
#define T64_VIDEO_OBSERVATION_PACKET_BYTES 48u
#define T64_VIDEO_OBSERVATION_FLAG_DP_MASK UINT32_C(0x01)
#define T64_VIDEO_OBSERVATION_FLAG_DP_PENDING UINT32_C(0x02)
#define T64_VIDEO_OBSERVATION_FLAG_DP_CLEARED UINT32_C(0x04)

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t width;
  uint32_t height;
  uint32_t stride_bytes;
  uint32_t source_address;
  uint32_t source_format;
  uint32_t output_encoding;
  uint32_t field;
  uint32_t parity;
  uint32_t control;
  uint32_t burst;
  uint32_t h_sync;
  uint32_t v_sync;
  uint32_t h_start;
  uint32_t v_start;
  uint32_t x_scale;
  uint32_t y_scale;
  uint32_t source_bytes;
  uint32_t packet_bytes;
  uint32_t semantic_flags;
  uint32_t mi_pending_before_clear;
  uint32_t mi_mask_before_clear;
  uint32_t mi_pending_after_clear;
  uint32_t dpc_current;
  uint32_t dpc_end;
  uint64_t start_tick;
  uint64_t end_tick;
  uint64_t sequence;
  uint64_t commands_completed;
  uint64_t fullsync_completed;
  uint64_t last_command_word;
  uint64_t fill_command_word;
  uint32_t last_command_byte;
  uint32_t last_opcode_field;
  uint32_t fill_command_byte;
  uint32_t fill_opcode_field;
  char profile_id[64];
  char build_id[96];
  char vi_clock_policy[64];
  uint8_t source[T64_VIDEO_OBSERVATION_SOURCE_BYTES];
  uint8_t packet[T64_VIDEO_OBSERVATION_PACKET_BYTES];
} t64_video_observation;

#endif
