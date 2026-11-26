/*
 * The logical input mapping of the minimal host (Phase 12, plan 12-08, API-05, decision
 * DR-12): one pure table from a raw pad (what a shell reads from a device) to the logical
 * pad the core takes (a 16-bit button mask and two signed 8-bit axes, port 0 only).
 *
 * No SDL type and no SDL header: the shell fills a t64h_raw_pad from its device and calls
 * t64h_map_pad; the table and the arithmetic are pinned by the host.pump.input-map case
 * without a device. Pure C99, no state, no clock, no allocation.
 *
 * The mapping:
 *   axes      the int16 stick value is divided by 256 (truncating toward zero) and clamped
 *             to -128..127; an absolute value below 8 (a raw magnitude below 2048) is the
 *             dead zone and maps to 0
 *   vertical  host down is positive and controller up is positive, so the host axis is
 *             inverted before the division by complement (-1 - value, not negation: the
 *             negation of -32768 does not fit and +32767, full host down, must reach -128,
 *             full controller down); the dead zone of that axis is therefore the raw range
 *             -2048..2046, one unit off the horizontal one
 *   buttons   A is bit 15, B 14, Start 12, D-pad up 11, down 10, left 9, right 8; the left
 *             trigger sets Z (bit 13) from half travel (16384 of 32767) on
 *   others    every other control (X, Y, back, guide, the stick buttons, the shoulders, the
 *             right stick and the right trigger) is unmapped and sets nothing
 *   absent    a pad that is not present (never plugged, or removed) maps to neutral input;
 *             hotplug is unsupported and keyboard input is off
 */
#ifndef T64H_INPUT_H
#define T64H_INPUT_H

#include <stdint.h>

/* The control bits of a raw pad: names of this host, filled by the shell. */
#define T64H_RAW_A UINT32_C(0x00000001)
#define T64H_RAW_B UINT32_C(0x00000002)
#define T64H_RAW_X UINT32_C(0x00000004)
#define T64H_RAW_Y UINT32_C(0x00000008)
#define T64H_RAW_START UINT32_C(0x00000010)
#define T64H_RAW_BACK UINT32_C(0x00000020)
#define T64H_RAW_GUIDE UINT32_C(0x00000040)
#define T64H_RAW_LEFT_STICK UINT32_C(0x00000080)
#define T64H_RAW_RIGHT_STICK UINT32_C(0x00000100)
#define T64H_RAW_LEFT_SHOULDER UINT32_C(0x00000200)
#define T64H_RAW_RIGHT_SHOULDER UINT32_C(0x00000400)
#define T64H_RAW_DPAD_UP UINT32_C(0x00000800)
#define T64H_RAW_DPAD_DOWN UINT32_C(0x00001000)
#define T64H_RAW_DPAD_LEFT UINT32_C(0x00002000)
#define T64H_RAW_DPAD_RIGHT UINT32_C(0x00004000)

/* The logical button bits of the standard controller mask the core takes. */
#define T64H_PAD_A UINT16_C(0x8000)
#define T64H_PAD_B UINT16_C(0x4000)
#define T64H_PAD_Z UINT16_C(0x2000)
#define T64H_PAD_START UINT16_C(0x1000)
#define T64H_PAD_UP UINT16_C(0x0800)
#define T64H_PAD_DOWN UINT16_C(0x0400)
#define T64H_PAD_LEFT UINT16_C(0x0200)
#define T64H_PAD_RIGHT UINT16_C(0x0100)

#define T64H_AXIS_DIVISOR 256
#define T64H_AXIS_DEADZONE 8
#define T64H_TRIGGER_HALF 16384

typedef struct {
  int present;            /* 0: no pad; every other field is then ignored */
  uint32_t buttons;       /* T64H_RAW_* bits */
  int16_t left_x;         /* host stick: right positive */
  int16_t left_y;         /* host stick: down positive */
  int16_t right_x;        /* unmapped */
  int16_t right_y;        /* unmapped */
  int16_t left_trigger;   /* 0..32767; Z from T64H_TRIGGER_HALF on */
  int16_t right_trigger;  /* unmapped */
} t64h_raw_pad;

typedef struct {
  uint16_t buttons;
  int8_t x;
  int8_t y;
} t64h_pad;

/* One axis: scale, clamp and dead zone; vertical (nonzero) inverts it first. */
int8_t t64h_map_axis(int16_t value, int vertical);

/* The whole pad. A NULL or absent raw pad is neutral (buttons 0, x 0, y 0). */
t64h_pad t64h_map_pad(const t64h_raw_pad *raw);

#endif /* T64H_INPUT_H */
