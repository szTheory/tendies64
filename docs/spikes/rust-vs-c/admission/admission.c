/* Spike-only admission parser. See admission.h for the frozen contract. */
#include "admission.h"

int t64s_admit(const uint8_t *buf, size_t len, size_t limit, t64s_admit_info *out) {
  uint8_t b0;
  uint8_t b1;
  uint8_t b2;
  uint8_t b3;
  int order;

  if (out != NULL) {
    out->order = -1;
    out->len = 0;
  }
  if (len < T64S_ADMIT_MIN_LEN) return T64S_ADMIT_TOO_SMALL;
  if (len > limit) return T64S_ADMIT_TOO_LARGE;
  if ((len % 4u) != 0u) return T64S_ADMIT_MISALIGNED;

#ifdef T64S_SEEDED_OVERREAD
  /* Harness negative control only: one byte past the end. The registered
   * build never defines this macro. */
  {
    volatile uint8_t sink = buf[len];
    (void)sink;
  }
#endif

  b0 = buf[0];
  b1 = buf[1];
  b2 = buf[2];
  b3 = buf[3];
  if (b0 == 0x80u && b1 == 0x37u && b2 == 0x12u && b3 == 0x40u) {
    order = 0;
  } else if (b0 == 0x37u && b1 == 0x80u && b2 == 0x40u && b3 == 0x12u) {
    order = 1;
  } else if (b0 == 0x40u && b1 == 0x12u && b2 == 0x37u && b3 == 0x80u) {
    order = 2;
  } else {
    return T64S_ADMIT_UNKNOWN_ORDER;
  }
  if (out != NULL) {
    out->order = order;
    out->len = len;
  }
  return order; /* 0, 1, 2 equal the three ok result codes */
}
