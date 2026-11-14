#ifndef T64_RSP_DIVIDER_H
#define T64_RSP_DIVIDER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint32_t input;
  uint32_t result;
  bool high_valid;
} t64_rsp_divider_state;

uint16_t t64_rsp_divider_reciprocal_word(uint32_t index);
uint16_t t64_rsp_divider_rsqrt_word(uint32_t index);
uint32_t t64_rsp_divider_result(bool inverse_square_root, uint32_t input);
bool t64_rsp_divider_step(uint32_t function, uint16_t scalar_input,
                          const t64_rsp_divider_state *previous,
                          t64_rsp_divider_state *next,
                          uint16_t *destination_value);

#endif
