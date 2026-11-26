/*
 * SHA-256 of the installed consumer, written from FIPS 180-4 (Phase 11, plan
 * 11-18). See t64c_sha256.h.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "t64c_sha256.h"

static const uint32_t t64c_k[64] = {
    UINT32_C(0x428a2f98), UINT32_C(0x71374491), UINT32_C(0xb5c0fbcf),
    UINT32_C(0xe9b5dba5), UINT32_C(0x3956c25b), UINT32_C(0x59f111f1),
    UINT32_C(0x923f82a4), UINT32_C(0xab1c5ed5), UINT32_C(0xd807aa98),
    UINT32_C(0x12835b01), UINT32_C(0x243185be), UINT32_C(0x550c7dc3),
    UINT32_C(0x72be5d74), UINT32_C(0x80deb1fe), UINT32_C(0x9bdc06a7),
    UINT32_C(0xc19bf174), UINT32_C(0xe49b69c1), UINT32_C(0xefbe4786),
    UINT32_C(0x0fc19dc6), UINT32_C(0x240ca1cc), UINT32_C(0x2de92c6f),
    UINT32_C(0x4a7484aa), UINT32_C(0x5cb0a9dc), UINT32_C(0x76f988da),
    UINT32_C(0x983e5152), UINT32_C(0xa831c66d), UINT32_C(0xb00327c8),
    UINT32_C(0xbf597fc7), UINT32_C(0xc6e00bf3), UINT32_C(0xd5a79147),
    UINT32_C(0x06ca6351), UINT32_C(0x14292967), UINT32_C(0x27b70a85),
    UINT32_C(0x2e1b2138), UINT32_C(0x4d2c6dfc), UINT32_C(0x53380d13),
    UINT32_C(0x650a7354), UINT32_C(0x766a0abb), UINT32_C(0x81c2c92e),
    UINT32_C(0x92722c85), UINT32_C(0xa2bfe8a1), UINT32_C(0xa81a664b),
    UINT32_C(0xc24b8b70), UINT32_C(0xc76c51a3), UINT32_C(0xd192e819),
    UINT32_C(0xd6990624), UINT32_C(0xf40e3585), UINT32_C(0x106aa070),
    UINT32_C(0x19a4c116), UINT32_C(0x1e376c08), UINT32_C(0x2748774c),
    UINT32_C(0x34b0bcb5), UINT32_C(0x391c0cb3), UINT32_C(0x4ed8aa4a),
    UINT32_C(0x5b9cca4f), UINT32_C(0x682e6ff3), UINT32_C(0x748f82ee),
    UINT32_C(0x78a5636f), UINT32_C(0x84c87814), UINT32_C(0x8cc70208),
    UINT32_C(0x90befffa), UINT32_C(0xa4506ceb), UINT32_C(0xbef9a3f7),
    UINT32_C(0xc67178f2)};

static uint32_t t64c_rotr(uint32_t value, unsigned count) {
  return (value >> count) | (value << (32u - count));
}

static void t64c_compress(uint32_t state[8], const uint8_t block[64]) {
  uint32_t w[64];
  uint32_t a, b, c, d, e, f, g, h;
  unsigned i;

  for (i = 0u; i < 16u; i++) {
    w[i] = ((uint32_t)block[4u * i] << 24) |
           ((uint32_t)block[4u * i + 1u] << 16) |
           ((uint32_t)block[4u * i + 2u] << 8) | (uint32_t)block[4u * i + 3u];
  }
  for (i = 16u; i < 64u; i++) {
    const uint32_t s0 = t64c_rotr(w[i - 15u], 7u) ^ t64c_rotr(w[i - 15u], 18u) ^
                        (w[i - 15u] >> 3);
    const uint32_t s1 = t64c_rotr(w[i - 2u], 17u) ^ t64c_rotr(w[i - 2u], 19u) ^
                        (w[i - 2u] >> 10);
    w[i] = w[i - 16u] + s0 + w[i - 7u] + s1;
  }
  a = state[0];
  b = state[1];
  c = state[2];
  d = state[3];
  e = state[4];
  f = state[5];
  g = state[6];
  h = state[7];
  for (i = 0u; i < 64u; i++) {
    const uint32_t big1 =
        t64c_rotr(e, 6u) ^ t64c_rotr(e, 11u) ^ t64c_rotr(e, 25u);
    const uint32_t choose = (e & f) ^ (~e & g);
    const uint32_t t1 = h + big1 + choose + t64c_k[i] + w[i];
    const uint32_t big0 =
        t64c_rotr(a, 2u) ^ t64c_rotr(a, 13u) ^ t64c_rotr(a, 22u);
    const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
    const uint32_t t2 = big0 + majority;
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  state[5] += f;
  state[6] += g;
  state[7] += h;
}

void t64c_sha256(const uint8_t *bytes, size_t length,
                 uint8_t digest[T64C_SHA256_SIZE]) {
  uint32_t state[8] = {UINT32_C(0x6a09e667), UINT32_C(0xbb67ae85),
                       UINT32_C(0x3c6ef372), UINT32_C(0xa54ff53a),
                       UINT32_C(0x510e527f), UINT32_C(0x9b05688c),
                       UINT32_C(0x1f83d9ab), UINT32_C(0x5be0cd19)};
  uint8_t tail[128];
  size_t offset = 0u;
  size_t remaining;
  size_t tail_length;
  uint64_t bit_length;
  unsigned i;

  while (length - offset >= 64u) {
    t64c_compress(state, bytes + offset);
    offset += 64u;
  }
  remaining = length - offset;
  memset(tail, 0, sizeof tail);
  if (remaining > 0u) {
    memcpy(tail, bytes + offset, remaining);
  }
  tail[remaining] = 0x80u;
  tail_length = remaining + 1u + 8u <= 64u ? 64u : 128u;
  bit_length = (uint64_t)length * 8u;
  for (i = 0u; i < 8u; i++) {
    tail[tail_length - 1u - i] = (uint8_t)(bit_length >> (8u * i));
  }
  t64c_compress(state, tail);
  if (tail_length == 128u) {
    t64c_compress(state, tail + 64u);
  }
  for (i = 0u; i < 8u; i++) {
    digest[4u * i] = (uint8_t)(state[i] >> 24);
    digest[4u * i + 1u] = (uint8_t)(state[i] >> 16);
    digest[4u * i + 2u] = (uint8_t)(state[i] >> 8);
    digest[4u * i + 3u] = (uint8_t)state[i];
  }
}

void t64c_sha256_hex(const uint8_t digest[T64C_SHA256_SIZE],
                     char hex[T64C_SHA256_HEX_SIZE]) {
  static const char digits[] = "0123456789abcdef";
  unsigned i;

  for (i = 0u; i < T64C_SHA256_SIZE; i++) {
    hex[2u * i] = digits[digest[i] >> 4];
    hex[2u * i + 1u] = digits[digest[i] & 0x0fu];
  }
  hex[2u * T64C_SHA256_SIZE] = '\0';
}

void t64c_sha256_hex_of(const uint8_t *bytes, size_t length,
                        char hex[T64C_SHA256_HEX_SIZE]) {
  uint8_t digest[T64C_SHA256_SIZE];

  t64c_sha256(bytes, length, digest);
  t64c_sha256_hex(digest, hex);
}

unsigned t64c_sha256_selfcheck(void) {
  static const char two_block[] =
      "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  char hex[T64C_SHA256_HEX_SIZE];
  unsigned failed = 0u;

  t64c_sha256_hex_of((const uint8_t *)"", 0u, hex);
  if (strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") != 0) {
    failed++;
  }
  t64c_sha256_hex_of((const uint8_t *)"abc", 3u, hex);
  if (strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") != 0) {
    failed++;
  }
  t64c_sha256_hex_of((const uint8_t *)two_block, sizeof two_block - 1u, hex);
  if (strcmp(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1") != 0) {
    failed++;
  }
  return failed;
}
