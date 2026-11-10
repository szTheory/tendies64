/*
 * Spike-only fuzz and differential driver for admission.c.
 *
 * Registered run (no arguments): a boundary sweep of 10 sizes x 4 headers,
 * then 2,000,000 deterministic mutations from a fixed xorshift64 seed.
 * Inputs exist only in memory, each in an exact-size heap block so that a
 * sanitizer sees any out-of-bounds read.
 *
 * The generator, the schedule and the output format are defined identically
 * in the Rust test `differential` in admission.rs. Printed lines:
 *   outcomes: c0 c1 c2 c3 c4 c5 c6     per-outcome counts
 *   fnv1a64: <16 hex>                  FNV-1a 64 over the one-byte outcome codes
 *
 * Options (harness controls only; the registered run passes none):
 *   --mutations N   run N mutations instead of 2,000,000
 *   --table         run the fixed contract table and exit
 */
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "admission.h"

#if defined(__SANITIZE_ADDRESS__)
#define T64S_SANITIZED 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define T64S_SANITIZED 1
#endif
#endif

#ifdef T64S_SANITIZED
extern void __sanitizer_set_death_callback(void (*cb)(void));
#endif

#define SEED UINT64_C(0x2545F4914F6CDD1D)
#define LIMIT ((size_t)67108864)
#define DEFAULT_MUTATIONS UINT64_C(2000000)
#define FNV_OFFSET UINT64_C(0xcbf29ce484222325)
#define FNV_PRIME UINT64_C(0x100000001b3)

static const uint8_t MAGIC[4][4] = {
    {0x80, 0x37, 0x12, 0x40}, /* z64 */
    {0x37, 0x80, 0x40, 0x12}, /* v64 */
    {0x40, 0x12, 0x37, 0x80}, /* n64 */
    {0x00, 0x00, 0x00, 0x00}  /* none */
};

static uint64_t rng_state = SEED;
static uint64_t fnv_state = FNV_OFFSET;
static uint64_t counts[7];
static uint64_t inputs_run;
static uint64_t info_bad;

/* The input being checked, for the death callback. */
static const uint8_t *cur_buf;
static size_t cur_len;
static uint64_t cur_index;
static const char *cur_phase = "none";

static uint64_t rnext(void) {
  uint64_t x = rng_state;
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;
  rng_state = x;
  return x;
}

static uint64_t fnv_bytes(const uint8_t *p, size_t n) {
  uint64_t h = FNV_OFFSET;
  size_t i;
  for (i = 0; i < n; i++) {
    h ^= p[i];
    h *= FNV_PRIME;
  }
  return h;
}

#ifdef T64S_SANITIZED
static void on_death(void) {
  uint64_t h = (cur_buf != NULL) ? fnv_bytes(cur_buf, cur_len) : fnv_bytes((const uint8_t *)"", 0);
  fprintf(stderr, "FAIL: input phase=%s index=%llu len=%llu fnv1a64=%016llx\n", cur_phase,
          (unsigned long long)cur_index, (unsigned long long)cur_len, (unsigned long long)h);
}
#endif

static uint8_t *alloc_exact(size_t len) {
  uint8_t *b = NULL;
  if (len != 0) {
    b = (uint8_t *)calloc(len, 1);
    if (b == NULL) {
      fprintf(stderr, "harness: allocation of %llu bytes failed\n", (unsigned long long)len);
      exit(3);
    }
  }
  return b;
}

/* Header magic, then 32 random bytes at offsets 4..35, rest zero. */
static uint8_t *make_input(size_t len, unsigned m) {
  uint8_t *b = alloc_exact(len);
  size_t off;
  size_t k;
  if (len >= 4) memcpy(b, MAGIC[m], 4);
  for (off = 4; off < len && off < 36; off += 8) {
    uint64_t v = rnext();
    for (k = 0; k < 8 && off + k < len; k++) b[off + k] = (uint8_t)(v >> (8 * k));
  }
  return b;
}

static int run_one(const uint8_t *buf, size_t len, size_t limit) {
  t64s_admit_info info;
  int rc;
  cur_buf = buf;
  cur_len = len;
  info.order = -2;
  info.len = 0;
  rc = t64s_admit(buf, len, limit, &info);
  if (rc < 0 || rc > 6) {
    fprintf(stderr, "harness: outcome code %d out of range\n", rc);
    exit(4);
  }
  if (rc <= 2) {
    if (info.order != rc || info.len != len) info_bad++;
  } else if (info.order != -1 || info.len != 0) {
    info_bad++;
  }
  counts[rc]++;
  fnv_state ^= (uint8_t)rc;
  fnv_state *= FNV_PRIME;
  inputs_run++;
  return rc;
}

static int table_check(void) {
  struct {
    size_t len;
    int m;
    size_t limit;
    int expect;
  } t[] = {
      {4096, 0, LIMIT, 0}, {4096, 1, LIMIT, 1}, {4096, 2, LIMIT, 2}, {4096, 3, LIMIT, 5},
      {4095, 0, LIMIT, 3}, {4097, 0, LIMIT, 6}, {8192, 0, 4096, 4},   {0, 0, LIMIT, 3},
      {4100, 2, LIMIT, 2}, {LIMIT + 4, 0, LIMIT, 4},
  };
  size_t i;
  int bad = 0;
  for (i = 0; i < sizeof t / sizeof t[0]; i++) {
    uint8_t *b = alloc_exact(t[i].len);
    if (t[i].len >= 4) memcpy(b, MAGIC[t[i].m], 4);
    {
      int rc = t64s_admit(b, t[i].len, t[i].limit, NULL);
      if (rc != t[i].expect) {
        fprintf(stderr, "table case %zu: expected %d got %d\n", i, t[i].expect, rc);
        bad++;
      }
    }
    free(b);
  }
  printf("table: %s\n", bad == 0 ? "ok" : "FAILED");
  return bad == 0 ? 0 : 1;
}

int main(int argc, char **argv) {
  static const size_t sizes[10] = {0,    1,    3,           4,          4031,
                                   4032, 4033, LIMIT - 1u, LIMIT,      LIMIT + 1u};
  uint64_t mutations = DEFAULT_MUTATIONS;
  uint64_t i;
  size_t si;
  unsigned m;
  int a;

#ifdef T64S_SANITIZED
  __sanitizer_set_death_callback(on_death);
#endif
  for (a = 1; a < argc; a++) {
    if (strcmp(argv[a], "--table") == 0) return table_check();
    if (strcmp(argv[a], "--mutations") == 0 && a + 1 < argc) {
      mutations = (uint64_t)strtoull(argv[++a], NULL, 10);
    } else {
      fprintf(stderr, "usage: fuzz_admission [--mutations N | --table]\n");
      return 2;
    }
  }

  /* Boundary sweep: each size once per header variant, in memory only. */
  cur_phase = "boundary";
  for (si = 0; si < 10; si++) {
    uint8_t *b = alloc_exact(sizes[si]);
    int codes[4];
    cur_index = si;
    for (m = 0; m < 4; m++) {
      if (sizes[si] >= 4) memcpy(b, MAGIC[m], 4);
      codes[m] = run_one(b, sizes[si], LIMIT);
    }
    printf("boundary %llu: %d%d%d%d\n", (unsigned long long)sizes[si], codes[0], codes[1], codes[2],
           codes[3]);
    free(b);
  }

  cur_phase = "mutation";
  for (i = 0; i < mutations; i++) {
    uint64_t r = rnext();
    uint64_t v;
    size_t len;
    size_t limit = LIMIT;
    uint8_t *b;
    cur_index = i;
    switch (r & 3u) {
      case 0: { /* byte flips inside the header */
        unsigned nf;
        unsigned j;
        len = (size_t)(4096u + 4u * ((r >> 2) % 16u));
        m = (unsigned)((r >> 8) % 3u);
        b = make_input(len, m);
        nf = (unsigned)(1u + (r >> 16) % 4u);
        for (j = 0; j < nf; j++) {
          v = rnext();
          b[v % 8u] ^= (uint8_t)((v >> 8) | 1u);
        }
        break;
      }
      case 1: /* truncation around the minimum */
        len = (size_t)((r >> 2) % 4100u);
        m = (unsigned)((r >> 20) % 3u);
        b = make_input(len, m);
        break;
      case 2: /* extension, with a small random limit */
        len = (size_t)(4096u + (r >> 2) % 4200u);
        m = (unsigned)((r >> 20) % 3u);
        b = make_input(len, m);
        v = rnext();
        limit = (size_t)(4096u + v % 4200u);
        break;
      default: /* random length and header, including no magic */
        len = (size_t)((r >> 2) % 16384u);
        m = (unsigned)((r >> 20) % 4u);
        b = make_input(len, m);
        break;
    }
    cur_buf = b;
    cur_len = len;
    (void)run_one(b, len, limit);
    free(b);
  }

  printf("mutations: %llu\n", (unsigned long long)mutations);
  printf("inputs: %llu\n", (unsigned long long)inputs_run);
  printf("outcomes: %llu %llu %llu %llu %llu %llu %llu\n", (unsigned long long)counts[0],
         (unsigned long long)counts[1], (unsigned long long)counts[2], (unsigned long long)counts[3],
         (unsigned long long)counts[4], (unsigned long long)counts[5], (unsigned long long)counts[6]);
  printf("fnv1a64: %016llx\n", (unsigned long long)fnv_state);
  printf("info-consistency: %s\n", info_bad == 0 ? "ok" : "BAD");
  return info_bad == 0 ? 0 : 5;
}
