/*
 * The frame and audio helpers of the minimal host (Phase 12, plan 12-08, API-05, decision
 * DR-14): a bounds-checked copy of one RGBA8 video packet into a pixel buffer, and the
 * byte swap from the S16BE audio packets of the core to native samples.
 *
 * Pure C99 on the installed header; no allocation, no clock, no SDL type. A shell calls
 * t64h_frame_copy with the packet info and bytes the pump delivered and the pixel buffer
 * of its window; every size comes from the packet info (width, height, stride_bytes) and
 * every check runs before the first byte is written, so a refused copy leaves the
 * destination untouched.
 */
#ifndef T64H_FRAMES_H
#define T64H_FRAMES_H

#include <stddef.h>
#include <stdint.h>

#include <tendies64/t64_machine.h>

/* The largest video packet the host supports, RGBA8: 320 x 240 (307,200 bytes). */
#define T64H_FRAME_MAX_WIDTH 320u
#define T64H_FRAME_MAX_HEIGHT 240u
#define T64H_FRAME_BYTES_PER_PIXEL 4u

typedef enum {
  T64H_FRAME_OK = 0,
  T64H_FRAME_BAD_ARGUMENT,       /* a NULL pointer */
  T64H_FRAME_BAD_ENCODING,       /* the packet is not RGBA8 */
  T64H_FRAME_BAD_GEOMETRY,       /* zero size, a source stride or destination pitch under one row */
  T64H_FRAME_TOO_LARGE,          /* wider than 320 or taller than 240 */
  T64H_FRAME_SHORT_SOURCE,       /* the packet bytes do not hold the rows the info names */
  T64H_FRAME_SHORT_DESTINATION   /* the destination does not hold the rows at its pitch */
} t64h_frame_status;

/* The stable name of a status, for logs. */
const char *t64h_frame_status_name(t64h_frame_status status);

/* Copy the rows of an RGBA8 packet to dest: row y of the packet starts at
 * y * info->stride_bytes and only its width * 4 pixel bytes are copied (the bytes a
 * stride adds beyond the row are not), to y * dest_pitch of dest. dest_pitch is at least
 * width * 4 (a tight buffer has exactly that). Every check precedes the first write. */
t64h_frame_status t64h_frame_copy(const t64_video_packet_info *info, const uint8_t *packet,
                                  size_t packet_length, uint8_t *dest, size_t dest_capacity,
                                  size_t dest_pitch);

/* Convert sample_count big-endian signed 16-bit samples (the S16BE audio packets) to
 * native samples. bytes holds 2 * sample_count bytes. */
void t64h_swap_s16be(const uint8_t *bytes, size_t sample_count, int16_t *samples);

#endif /* T64H_FRAMES_H */
