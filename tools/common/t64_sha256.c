/* SHA-256 per FIPS 180-4 (identity digest only; see t64_sha256.h). */
#include "t64_sha256.h"

#include <string.h>

static const uint32_t k256[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu,
    0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u,
    0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u,
    0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u,
    0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u,
    0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u, 0x1e376c08u,
    0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu,
    0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

static uint32_t rotr32(uint32_t x, unsigned n) {
  return (x >> n) | (x << (32u - n));
}

static void compress(uint32_t h[8], const uint8_t blk[64]) {
  uint32_t w[64];
  uint32_t a, b, c, d, e, f, g, hh;
  size_t i;

  for (i = 0; i < 16; i++) {
    w[i] = ((uint32_t)blk[i * 4] << 24) | ((uint32_t)blk[i * 4 + 1] << 16) |
           ((uint32_t)blk[i * 4 + 2] << 8) | (uint32_t)blk[i * 4 + 3];
  }
  for (i = 16; i < 64; i++) {
    uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
    uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }

  a = h[0]; b = h[1]; c = h[2]; d = h[3];
  e = h[4]; f = h[5]; g = h[6]; hh = h[7];

  for (i = 0; i < 64; i++) {
    uint32_t big_s1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
    uint32_t ch = (e & f) ^ (~e & g);
    uint32_t t1 = hh + big_s1 + ch + k256[i] + w[i];
    uint32_t big_s0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
    uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    uint32_t t2 = big_s0 + maj;
    hh = g; g = f; f = e; e = d + t1;
    d = c; c = b; b = a; a = t1 + t2;
  }

  h[0] += a; h[1] += b; h[2] += c; h[3] += d;
  h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

void t64_sha256_init(t64_sha256_ctx *ctx) {
  static const uint32_t iv[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u,
                                 0xa54ff53au, 0x510e527fu, 0x9b05688cu,
                                 0x1f83d9abu, 0x5be0cd19u};
  memcpy(ctx->h, iv, sizeof iv);
  ctx->block_len = 0;
  ctx->total_len = 0;
}

void t64_sha256_update(t64_sha256_ctx *ctx, const void *data, size_t len) {
  const uint8_t *p = (const uint8_t *)data;

  ctx->total_len += (uint64_t)len;
  if (ctx->block_len > 0) {
    size_t take = 64 - ctx->block_len;
    if (take > len) take = len;
    memcpy(ctx->block + ctx->block_len, p, take);
    ctx->block_len += take;
    p += take;
    len -= take;
    if (ctx->block_len < 64) return;
    compress(ctx->h, ctx->block);
    ctx->block_len = 0;
  }
  while (len >= 64) {
    compress(ctx->h, p);
    p += 64;
    len -= 64;
  }
  if (len > 0) {
    memcpy(ctx->block, p, len);
    ctx->block_len = len;
  }
}

void t64_sha256_final(t64_sha256_ctx *ctx, uint8_t out[T64_SHA256_DIGEST_SIZE]) {
  uint64_t bits = ctx->total_len * 8u;
  size_t i;

  ctx->block[ctx->block_len++] = 0x80u;
  if (ctx->block_len > 56) {
    memset(ctx->block + ctx->block_len, 0, 64 - ctx->block_len);
    compress(ctx->h, ctx->block);
    ctx->block_len = 0;
  }
  memset(ctx->block + ctx->block_len, 0, 56 - ctx->block_len);
  /* Explicit big-endian length: no dependence on host byte order. */
  for (i = 0; i < 8; i++) {
    ctx->block[56 + i] = (uint8_t)(bits >> (56u - 8u * (unsigned)i));
  }
  compress(ctx->h, ctx->block);

  for (i = 0; i < 8; i++) {
    out[i * 4] = (uint8_t)(ctx->h[i] >> 24);
    out[i * 4 + 1] = (uint8_t)(ctx->h[i] >> 16);
    out[i * 4 + 2] = (uint8_t)(ctx->h[i] >> 8);
    out[i * 4 + 3] = (uint8_t)ctx->h[i];
  }
  memset(ctx, 0, sizeof *ctx);
}

void t64_sha256_hex(const uint8_t digest[T64_SHA256_DIGEST_SIZE],
                    char out[T64_SHA256_HEX_SIZE]) {
  static const char hexd[] = "0123456789abcdef";
  size_t i;
  for (i = 0; i < T64_SHA256_DIGEST_SIZE; i++) {
    out[i * 2] = hexd[digest[i] >> 4];
    out[i * 2 + 1] = hexd[digest[i] & 0x0fu];
  }
  out[64] = '\0';
}
