/*
 * SHA-256 of the host tests (Phase 12, plan 12-07), a renamed copy of the
 * installed consumer's t64c_sha256 (Phase 11, plan 11-18), written from FIPS
 * 180-4. It is never the hash of the core, of tools/common or of the library's
 * captured_sha256, so a digest the host tests compare against an expected value
 * is computed by code that shares nothing with the producer.
 * t64h_sha256_selfcheck() runs the published vectors.
 */
#ifndef T64H_SHA256_H
#define T64H_SHA256_H

#include <stddef.h>
#include <stdint.h>

#define T64H_SHA256_SIZE 32u
#define T64H_SHA256_HEX_SIZE 65u

/* One-shot digest of length bytes. */
void t64h_sha256(const uint8_t *bytes, size_t length,
                 uint8_t digest[T64H_SHA256_SIZE]);

/* Lower-case hexadecimal of a digest, NUL-terminated. */
void t64h_sha256_hex(const uint8_t digest[T64H_SHA256_SIZE],
                     char hex[T64H_SHA256_HEX_SIZE]);

/* Convenience: the hex of the SHA-256 of the bytes. */
void t64h_sha256_hex_of(const uint8_t *bytes, size_t length,
                        char hex[T64H_SHA256_HEX_SIZE]);

/* The empty-string, "abc" and the 448-bit two-block published vectors: the
 * number of vectors that failed (0 is a pass). */
unsigned t64h_sha256_selfcheck(void);

#endif /* T64H_SHA256_H */
