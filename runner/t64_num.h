/*
 * Strict decimal parsing shared by the runner and the supervisor (authored
 * here). Accepts one or more ASCII digits, no sign, no whitespace, no leading
 * zeros except the single digit "0", and rejects anything above UINT64_MAX.
 */
#ifndef T64_NUM_H
#define T64_NUM_H

#include <stddef.h>
#include <stdint.h>

#define T64_MAX_SAFE_INT 9007199254740991ull /* 2^53 - 1 */

static inline int t64_parse_u64(const char *s, size_t len, uint64_t *out) {
  uint64_t v = 0;
  size_t i;
  if (len == 0 || len > 20) {
    return 0;
  }
  if (s[0] == '0' && len > 1) {
    return 0;
  }
  for (i = 0; i < len; i++) {
    unsigned d;
    if (s[i] < '0' || s[i] > '9') {
      return 0;
    }
    d = (unsigned)(s[i] - '0');
    if (v > (UINT64_MAX - d) / 10u) {
      return 0;
    }
    v = v * 10u + d;
  }
  *out = v;
  return 1;
}

#endif /* T64_NUM_H */
