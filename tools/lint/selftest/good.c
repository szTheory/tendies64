/* EXPECT: PASS */
/*
 * Allowed constructs. The words float, double, time, malloc, rand and
 * __TIME__ appear only in comments and strings, which the lint strips.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>

typedef struct {
  uint32_t value;
  uint32_t floating_point_mode;
  uint8_t bytes[8];
} t64_seed_state;

static const uint32_t t64_seed_table[4] = {1u, 2u, 3u, 4u};
static const char t64_seed_note[] = "float time malloc 0.5 __TIME__ 'x'";
static const char t64_seed_quote = '"';

static uint32_t t64_seed_mix(uint32_t a, uint32_t b) { return (a << 3) ^ b; }

uint32_t t64_seed_good(t64_seed_state *st, const uint8_t *src, size_t n) {
  memcpy(st->bytes, src, sizeof st->bytes);
  memset(st->bytes, 0, n & 7u);
  st->floating_point_mode = (uint32_t)t64_seed_note[0] + (uint32_t)t64_seed_quote;
  st->value = t64_seed_mix(t64_seed_table[st->bytes[0] & 3u], st->floating_point_mode);
  return st->value;
}
