/*
 * UTF-8 validation per RFC 3629. Rejects overlong encodings, surrogates
 * (U+D800..U+DFFF), values above U+10FFFF, bytes C0, C1 and F5..FF, stray
 * continuation bytes and truncated sequences.
 */
#ifndef T64_UTF8_H
#define T64_UTF8_H

#include <stddef.h>
#include <stdint.h>

/*
 * Returns 1 when buf[0..len) is valid UTF-8, else 0. On failure, when
 * bad_offset is not NULL, it receives the offset of the first byte of the
 * first invalid or truncated sequence. On success *bad_offset is set to len.
 */
int t64_utf8_valid(const uint8_t *buf, size_t len, size_t *bad_offset);

#endif /* T64_UTF8_H */
