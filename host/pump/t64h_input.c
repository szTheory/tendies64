/*
 * The logical input mapping of the minimal host (Phase 12, plan 12-08, DR-12). See
 * t64h_input.h for the table and the arithmetic; this file is the one table.
 */
#include "t64h_input.h"

#include <stddef.h>

typedef enum {
  SOURCE_BUTTON,        /* a bit of t64h_raw_pad.buttons */
  SOURCE_LEFT_TRIGGER   /* the left trigger at or past half travel */
} source_kind;

typedef struct {
  source_kind kind;
  uint32_t raw_bit;     /* SOURCE_BUTTON: the T64H_RAW_* bit */
  uint16_t logical;     /* the T64H_PAD_* bit it sets */
} mapping_row;

/* The whole button mapping. A control that has no row sets nothing. */
static const mapping_row mapping_table[] = {
    {SOURCE_BUTTON, T64H_RAW_A, T64H_PAD_A},
    {SOURCE_BUTTON, T64H_RAW_B, T64H_PAD_B},
    {SOURCE_LEFT_TRIGGER, 0u, T64H_PAD_Z},
    {SOURCE_BUTTON, T64H_RAW_START, T64H_PAD_START},
    {SOURCE_BUTTON, T64H_RAW_DPAD_UP, T64H_PAD_UP},
    {SOURCE_BUTTON, T64H_RAW_DPAD_DOWN, T64H_PAD_DOWN},
    {SOURCE_BUTTON, T64H_RAW_DPAD_LEFT, T64H_PAD_LEFT},
    {SOURCE_BUTTON, T64H_RAW_DPAD_RIGHT, T64H_PAD_RIGHT},
};

int8_t t64h_map_axis(int16_t value, int vertical) {
  int32_t v = value;
  int32_t scaled;

  if (vertical) {
    v = -1 - v;
  }
  scaled = v / T64H_AXIS_DIVISOR;
  if (scaled < -128) {
    scaled = -128;
  }
  if (scaled > 127) {
    scaled = 127;
  }
  if (scaled > -T64H_AXIS_DEADZONE && scaled < T64H_AXIS_DEADZONE) {
    return 0;
  }
  return (int8_t)scaled;
}

t64h_pad t64h_map_pad(const t64h_raw_pad *raw) {
  t64h_pad pad = {0u, 0, 0};
  size_t i;

  if (raw == NULL || !raw->present) {
    return pad;
  }
  for (i = 0u; i < sizeof mapping_table / sizeof mapping_table[0]; i++) {
    const mapping_row *row = &mapping_table[i];
    int active = 0;

    if (row->kind == SOURCE_BUTTON) {
      active = (raw->buttons & row->raw_bit) != 0u;
    } else {
      active = raw->left_trigger >= T64H_TRIGGER_HALF;
    }
    if (active) {
      pad.buttons = (uint16_t)(pad.buttons | row->logical);
    }
  }
  pad.x = t64h_map_axis(raw->left_x, 0);
  pad.y = t64h_map_axis(raw->left_y, 1);
  return pad;
}
