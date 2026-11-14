#ifndef T64_RSP_DIVIDER_H
#define T64_RSP_DIVIDER_H

#include <stdbool.h>
#include <stdint.h>

uint16_t t64_rsp_divider_reciprocal_word(uint32_t index);
uint16_t t64_rsp_divider_rsqrt_word(uint32_t index);
uint32_t t64_rsp_divider_result(bool inverse_square_root, uint32_t input);

#endif
