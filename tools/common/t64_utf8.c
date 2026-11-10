/* UTF-8 validation per RFC 3629 section 4 (Syntax of UTF-8 Byte Sequences). */
#include "t64_utf8.h"

static int is_cont(uint8_t b) { return b >= 0x80u && b <= 0xBFu; }

int t64_utf8_valid(const uint8_t *buf, size_t len, size_t *bad_offset) {
  size_t i = 0;

  while (i < len) {
    uint8_t b = buf[i];
    size_t need; /* continuation bytes after the lead byte */
    uint8_t lo = 0x80u; /* allowed range of the first continuation byte */
    uint8_t hi = 0xBFu;
    size_t k;

    if (b <= 0x7Fu) {
      i++;
      continue;
    }
    if (b >= 0xC2u && b <= 0xDFu) {
      need = 1;
    } else if (b >= 0xE0u && b <= 0xEFu) {
      need = 2;
      if (b == 0xE0u) lo = 0xA0u; /* no overlong 3-byte forms */
      if (b == 0xEDu) hi = 0x9Fu; /* no surrogates */
    } else if (b >= 0xF0u && b <= 0xF4u) {
      need = 3;
      if (b == 0xF0u) lo = 0x90u; /* no overlong 4-byte forms */
      if (b == 0xF4u) hi = 0x8Fu; /* nothing above U+10FFFF */
    } else {
      /* 80..BF stray continuation, C0, C1, F5..FF */
      goto bad;
    }
    if (len - i <= need) goto bad; /* truncated */
    if (buf[i + 1] < lo || buf[i + 1] > hi) goto bad;
    for (k = 2; k <= need; k++) {
      if (!is_cont(buf[i + k])) goto bad;
    }
    i += need + 1;
  }
  if (bad_offset != NULL) *bad_offset = len;
  return 1;

bad:
  if (bad_offset != NULL) *bad_offset = i;
  return 0;
}
