/*
 * unit.common, step 1: SHA-256 vectors with plain checks. Task 2 moves this
 * file to tests/t64_test.h.
 *
 * Vectors are the published FIPS 180-4 / NIST CAVP examples (SHA-256 example
 * values: empty string, "abc", the 448-bit and 896-bit messages, and one
 * million repetitions of 'a'). They are not derived from this implementation.
 */
#include <stdio.h>
#include <string.h>

#include "t64_sha256.h"

static int checks;
static int failures;

static void check_digest(const char *what, const void *msg, size_t len,
                         const char *expect_hex) {
  t64_sha256_ctx ctx;
  uint8_t d[T64_SHA256_DIGEST_SIZE];
  char hex[T64_SHA256_HEX_SIZE];

  t64_sha256_init(&ctx);
  t64_sha256_update(&ctx, msg, len);
  t64_sha256_final(&ctx, d);
  t64_sha256_hex(d, hex);
  checks++;
  if (strcmp(hex, expect_hex) != 0) {
    failures++;
    fprintf(stderr, "FAIL sha256 %s: got %s want %s\n", what, hex, expect_hex);
  }
}

int main(void) {
  static char million[1000000];

  check_digest("empty", "", 0,
               "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  check_digest("abc", "abc", 3,
               "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  {
    const char *m448 = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    check_digest("448-bit", m448, strlen(m448),
                 "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  }
  {
    const char *m896 =
        "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmn"
        "opjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";
    check_digest("896-bit", m896, strlen(m896),
                 "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1");
  }
  memset(million, 'a', sizeof million);
  check_digest("million-a", million, sizeof million,
               "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");

  printf("unit.common: checks=%d failures=%d\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
