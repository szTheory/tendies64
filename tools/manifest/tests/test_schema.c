/*
 * unit.schema-sanitize: t64_sanitize never writes at or past out[cap]
 * (review finding CR-01). Each call gets a buffer with guard bytes after
 * the declared capacity; a write at out[cap] changes a guard byte, so this
 * test fails on the old off-by-one even without a sanitizer. Expected strings
 * are written from the header contract ("..." tail when the input does not
 * fit, control bytes as '?'), not derived from the code under test.
 */
#include <string.h>

#include "t64_schema.h"
#include "t64_test.h"

#define GUARD 0x5Au

static void fill(char *s, size_t n, char c) {
  size_t i;
  for (i = 0; i < n; i++) s[i] = c;
}

/* Runs t64_sanitize(in, len, buf, cap) and checks the guard after cap. */
static void run(const char *in, size_t len, size_t cap, const char *want) {
  char buf[512];
  size_t i;
  int guard_ok = 1;

  memset(buf, (int)GUARD, sizeof buf);
  t64_sanitize(in, len, buf, cap);
  for (i = cap; i < sizeof buf; i++) {
    if ((unsigned char)buf[i] != GUARD) guard_ok = 0;
  }
  T64_CHECK(guard_ok);
  if (want != NULL) {
    T64_CHECK_STR(buf, want);
    T64_CHECK(strlen(buf) < cap);
  }
}

int main(void) {
  char in[300];
  char want[300];
  size_t cap;
  size_t len;

  /* Fits: copied verbatim, control bytes become '?'. */
  run("abc", 3u, 64u, "abc");
  run("a\tb\x7f", 4u, 64u, "a?b?");
  run("", 0u, 4u, "");

  /* Every length around every small capacity: never a byte at out[cap]. */
  for (cap = 4u; cap <= 100u; cap++) {
    for (len = 0u; len <= cap + 8u && len < sizeof in; len++) {
      fill(in, len, 'x');
      if (len + 1u <= cap) {
        fill(want, len, 'x');
        want[len] = '\0';
      } else {
        fill(want, cap - 4u, 'x');
        memcpy(want + cap - 4u, "...", 4u);
      }
      run(in, len, cap, want);
    }
  }

  /* The two review reproductions: 120 bytes into 64, 100 bytes into 64. */
  fill(in, 120u, 't');
  fill(want, 60u, 't');
  memcpy(want + 60u, "...", 4u);
  run(in, 120u, 64u, want);
  fill(in, 100u, 'r');
  fill(want, 60u, 'r');
  memcpy(want + 60u, "...", 4u);
  run(in, 100u, 64u, want);

  /* Below the minimum capacity only an empty string is written. */
  run("abcdef", 6u, 3u, "");
  run("abcdef", 6u, 1u, "");

  return T64_TEST_FINISH("unit.schema-sanitize");
}
