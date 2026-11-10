/*
 * SHA-256 (FIPS 180-4), authored for this project. Used only as an identity
 * digest (fixtures, logs, inventories), never as a key, MAC or signature.
 * The message length is encoded explicitly big-endian and no struct is ever
 * hashed, so digests are identical on every host.
 */
#ifndef T64_SHA256_H
#define T64_SHA256_H

#include <stddef.h>
#include <stdint.h>

#define T64_SHA256_DIGEST_SIZE 32u
#define T64_SHA256_HEX_SIZE 65u /* 64 hex characters plus NUL */

typedef struct {
  uint32_t h[8];
  uint8_t block[64];
  size_t block_len;
  uint64_t total_len; /* message length in bytes */
} t64_sha256_ctx;

void t64_sha256_init(t64_sha256_ctx *ctx);
void t64_sha256_update(t64_sha256_ctx *ctx, const void *data, size_t len);
void t64_sha256_final(t64_sha256_ctx *ctx, uint8_t out[T64_SHA256_DIGEST_SIZE]);
/* Lowercase hex of a digest, NUL-terminated. */
void t64_sha256_hex(const uint8_t digest[T64_SHA256_DIGEST_SIZE],
                    char out[T64_SHA256_HEX_SIZE]);

#endif /* T64_SHA256_H */
