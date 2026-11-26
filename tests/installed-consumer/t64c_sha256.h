/*
 * SHA-256 of the installed consumer (Phase 11, plan 11-18). Written fresh from
 * FIPS 180-4 for the consumer alone: it is never the hash of the core, of
 * tools/common or of the library's captured_sha256, so a digest the consumer
 * compares against a literal is computed by code that shares nothing with the
 * producer. t64c_sha256_selfcheck() runs the published vectors.
 */
#ifndef T64C_SHA256_H
#define T64C_SHA256_H

#include <stddef.h>
#include <stdint.h>

#define T64C_SHA256_SIZE 32u
#define T64C_SHA256_HEX_SIZE 65u

/* One-shot digest of length bytes. */
void t64c_sha256(const uint8_t *bytes, size_t length,
                 uint8_t digest[T64C_SHA256_SIZE]);

/* Lower-case hexadecimal of a digest, NUL-terminated. */
void t64c_sha256_hex(const uint8_t digest[T64C_SHA256_SIZE],
                     char hex[T64C_SHA256_HEX_SIZE]);

/* Convenience: the hex of the SHA-256 of the bytes. */
void t64c_sha256_hex_of(const uint8_t *bytes, size_t length,
                        char hex[T64C_SHA256_HEX_SIZE]);

/* The empty-string, "abc" and the 448-bit two-block published vectors: the
 * number of vectors that failed (0 is a pass). */
unsigned t64c_sha256_selfcheck(void);

#endif /* T64C_SHA256_H */
