/* EXPECT: PASS */
#include <stdint.h>
#include <string.h>

typedef struct {
  uint32_t value;
  uint8_t bytes[8];
} t64_seed_state;

static const uint32_t t64_seed_table[4] = {1u, 2u, 3u, 4u};

uint32_t t64_seed_good(t64_seed_state *st, const uint8_t *src) {
  memcpy(st->bytes, src, sizeof st->bytes);
  st->value = t64_seed_table[st->bytes[0] & 3u];
  return st->value;
}
