/*
 * Spike-only ROM-admission size and header check (docs/spikes/rust-vs-c).
 * Not part of the shipped tree, never built by CMake, never under core/.
 *
 * Contract (frozen in PREREG.md):
 *   t64s_admit(buf, len, limit, out) inspects only the length and, once the
 *   length is acceptable, the first 4 bytes of buf. buf may be NULL when
 *   len is 0. The checks run in this precedence order:
 *     1. len < 4096            -> T64S_ADMIT_TOO_SMALL
 *     2. len > limit           -> T64S_ADMIT_TOO_LARGE
 *     3. len % 4 != 0          -> T64S_ADMIT_MISALIGNED
 *     4. first 4 bytes are one of the three byte-order magics
 *          z64 80 37 12 40 -> T64S_ADMIT_OK_Z64
 *          v64 37 80 40 12 -> T64S_ADMIT_OK_V64
 *          n64 40 12 37 80 -> T64S_ADMIT_OK_N64
 *        otherwise          -> T64S_ADMIT_UNKNOWN_ORDER
 *   The magic values are format facts, not protected content.
 *   out (when not NULL) is set to {order -1, len 0} first, and to
 *   {order 0..2, len} on an ok result.
 */
#ifndef T64S_ADMISSION_H
#define T64S_ADMISSION_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
  T64S_ADMIT_OK_Z64 = 0,
  T64S_ADMIT_OK_V64 = 1,
  T64S_ADMIT_OK_N64 = 2,
  T64S_ADMIT_TOO_SMALL = 3,
  T64S_ADMIT_TOO_LARGE = 4,
  T64S_ADMIT_UNKNOWN_ORDER = 5,
  T64S_ADMIT_MISALIGNED = 6
} t64s_admit_result;

typedef struct {
  int order; /* 0 z64, 1 v64, 2 n64, -1 not admitted */
  size_t len;
} t64s_admit_info;

#define T64S_ADMIT_MIN_LEN ((size_t)4096)

int t64s_admit(const uint8_t *buf, size_t len, size_t limit, t64s_admit_info *out);

#endif
