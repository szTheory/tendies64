/*
 * unit.common: vector tests for the shared library (SHA-256, UTF-8, canonical
 * JSON writer).
 *
 * SHA-256 vectors are the published FIPS 180-4 / NIST CAVP examples (empty
 * string, "abc", the 448-bit and 896-bit messages, one million 'a'). The
 * block-boundary digests (55..65 bytes of 'a') come from an independent
 * implementation (the host's shasum), never from this one. UTF-8 cases are the
 * RFC 3629 section 4 negative set.
 */
#include <stdint.h>
#include <string.h>

#include "t64_jsonw.h"
#include "t64_sha256.h"
#include "t64_test.h"
#include "t64_utf8.h"

/* ---- SHA-256 ---------------------------------------------------------- */

static void sha_check(const char *expect_hex, const void *msg, size_t len,
                      size_t chunk) {
  t64_sha256_ctx ctx;
  uint8_t d[T64_SHA256_DIGEST_SIZE];
  char hex[T64_SHA256_HEX_SIZE];
  const uint8_t *p = (const uint8_t *)msg;
  size_t off = 0;

  t64_sha256_init(&ctx);
  while (off < len) {
    size_t n = len - off < chunk ? len - off : chunk;
    t64_sha256_update(&ctx, p + off, n);
    off += n;
  }
  t64_sha256_final(&ctx, d);
  t64_sha256_hex(d, hex);
  T64_CHECK_STR(hex, expect_hex);
}

static void test_sha256(void) {
  static char million[1000000];
  static char buf65[65];
  static const struct {
    size_t len;
    const char *hex;
  } boundary[] = {
      {55, "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318"},
      {56, "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a"},
      {63, "7d3e74a05d7db15bce4ad9ec0658ea98e3f06eeecf16b4c6fff2da457ddc2f34"},
      {64, "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb"},
      {65, "635361c48bb9eab14198e76ea8ab7f1a41685d6ad62aa9146d301d4f17eb0ae0"},
  };
  const char *m448 = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  const char *m896 =
      "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmn"
      "opjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";
  size_t i;

  sha_check("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
            "", 0, 1);
  sha_check("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
            "abc", 3, 3);
  sha_check("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
            m448, strlen(m448), strlen(m448));
  sha_check("cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1",
            m896, strlen(m896), strlen(m896));
  memset(million, 'a', sizeof million);
  sha_check("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
            million, sizeof million, sizeof million);

  /* Chunking must not change the digest. */
  sha_check("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
            "abc", 3, 1);
  sha_check("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
            million, sizeof million, 997);

  memset(buf65, 'a', sizeof buf65);
  for (i = 0; i < sizeof boundary / sizeof boundary[0]; i++) {
    sha_check(boundary[i].hex, buf65, boundary[i].len, boundary[i].len);
    sha_check(boundary[i].hex, buf65, boundary[i].len, 7);
  }
}

/* ---- UTF-8 ------------------------------------------------------------ */

static int utf8_ok(const uint8_t *b, size_t n) {
  return t64_utf8_valid(b, n, NULL);
}

static void test_utf8(void) {
  static const uint8_t overlong2[] = {0xC0, 0x80};
  static const uint8_t overlong3[] = {0xE0, 0x80, 0x80};
  static const uint8_t overlong4[] = {0xF0, 0x80, 0x80, 0x80};
  static const uint8_t surrogate[] = {0xED, 0xA0, 0x80};
  static const uint8_t too_big[] = {0xF4, 0x90, 0x80, 0x80};
  static const uint8_t lone_cont[] = {0x80};
  static const uint8_t trunc2[] = {0xC3};
  static const uint8_t trunc3[] = {0xE2, 0x82};
  static const uint8_t trunc4[] = {0xF0, 0x9F, 0x98};
  static const uint8_t bad_cont[] = {0xE2, 0x28, 0xA1};
  static const uint8_t del[] = {0x7F};
  static const uint8_t max2[] = {0xDF, 0xBF};
  static const uint8_t bmp_max[] = {0xEF, 0xBF, 0xBF};
  static const uint8_t cp_max[] = {0xF4, 0x8F, 0xBF, 0xBF};
  static const uint8_t mixed[] = {'a', 0xC3, 0xA9, 'z', 0xF0, 0x9F, 0x98, 0x80};
  static const uint8_t mixed_bad[] = {'a', 'b', 0xFF, 'c'};
  uint8_t lead[1];
  size_t bad = 99;
  unsigned v;

  T64_CHECK(!utf8_ok(overlong2, sizeof overlong2));
  T64_CHECK(!utf8_ok(overlong3, sizeof overlong3));
  T64_CHECK(!utf8_ok(overlong4, sizeof overlong4));
  T64_CHECK(!utf8_ok(surrogate, sizeof surrogate));
  T64_CHECK(!utf8_ok(too_big, sizeof too_big));
  T64_CHECK(!utf8_ok(lone_cont, sizeof lone_cont));
  T64_CHECK(!utf8_ok(trunc2, sizeof trunc2));
  T64_CHECK(!utf8_ok(trunc3, sizeof trunc3));
  T64_CHECK(!utf8_ok(trunc4, sizeof trunc4));
  T64_CHECK(!utf8_ok(bad_cont, sizeof bad_cont));

  /* Every lead byte F5..FF is invalid on its own and with continuations. */
  for (v = 0xF5u; v <= 0xFFu; v++) {
    uint8_t seq[4];
    lead[0] = (uint8_t)v;
    T64_CHECK(!utf8_ok(lead, 1));
    seq[0] = (uint8_t)v;
    seq[1] = seq[2] = seq[3] = 0x80;
    T64_CHECK(!utf8_ok(seq, 4));
  }

  T64_CHECK(utf8_ok(NULL, 0));
  T64_CHECK(utf8_ok(del, sizeof del));
  T64_CHECK(utf8_ok(max2, sizeof max2));
  T64_CHECK(utf8_ok(bmp_max, sizeof bmp_max));
  T64_CHECK(utf8_ok(cp_max, sizeof cp_max));
  T64_CHECK(utf8_ok(mixed, sizeof mixed));

  /* bad_offset points at the start of the bad or truncated sequence. */
  T64_CHECK(!t64_utf8_valid(mixed_bad, sizeof mixed_bad, &bad));
  T64_CHECK_INT(bad, 2);
  T64_CHECK(!t64_utf8_valid(trunc4, sizeof trunc4, &bad));
  T64_CHECK_INT(bad, 0);
  T64_CHECK(t64_utf8_valid(mixed, sizeof mixed, &bad));
  T64_CHECK_INT(bad, sizeof mixed);
}

/* ---- Canonical JSON writer -------------------------------------------- */

static void test_jsonw(void) {
  char buf[256];
  char tiny[8];
  t64_jsonw w;
  size_t n = 0;
  static const char bad_utf8[] = {(char)0xC0, (char)0x80};
  static const char ctl[] = {'a', 0x1f, 'b'};

  /* Nested object with sorted keys: exact bytes including the trailing LF. */
  t64_jsonw_init(&w, buf, sizeof buf);
  T64_CHECK_INT(t64_jsonw_begin_object(&w), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_key(&w, "a", 1), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_int(&w, 1), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_key(&w, "b", 1), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_begin_object(&w), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_key(&w, "c", 1), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_begin_array(&w), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_int(&w, -2), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_null(&w), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_bool(&w, 1), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_bool(&w, 0), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_string(&w, "x", 1), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_end_array(&w), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_key(&w, "d", 1), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_string(&w, ctl, sizeof ctl), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_end_object(&w), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_key(&w, "z", 1), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_string(&w, "q\"\\\b\t\n\f\r\x7f", 9), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_end_object(&w), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_finish(&w, &n), T64_JSONW_OK);
  /* A control character is a lowercase \u001f; DEL (0x7f) stays raw. */
  T64_CHECK_STR(buf,
                "{\"a\":1,\"b\":{\"c\":[-2,null,true,false,\"x\"],"
                "\"d\":\"a\\u001fb\"},\"z\":\"q\\\"\\\\\\b\\t\\n\\f\\r\x7f\"}\n");
  T64_CHECK_INT(n, strlen(buf));
  T64_CHECK(buf[n - 1] == '\n');

  /* Non-ASCII is emitted raw (U+00E9 as C3 A9). */
  t64_jsonw_init(&w, buf, sizeof buf);
  T64_CHECK_INT(t64_jsonw_string(&w, "\xC3\xA9", 2), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_finish(&w, &n), T64_JSONW_OK);
  T64_CHECK_STR(buf, "\"\xC3\xA9\"\n");

  /* Out-of-order key is an error; so is a repeated key. */
  t64_jsonw_init(&w, buf, sizeof buf);
  T64_CHECK_INT(t64_jsonw_begin_object(&w), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_key(&w, "b", 1), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_int(&w, 1), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_key(&w, "a", 1), T64_JSONW_E_ORDER);
  T64_CHECK_INT(t64_jsonw_end_object(&w), T64_JSONW_E_ORDER); /* sticky */
  t64_jsonw_init(&w, buf, sizeof buf);
  T64_CHECK_INT(t64_jsonw_begin_object(&w), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_key(&w, "k", 1), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_int(&w, 1), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_key(&w, "k", 1), T64_JSONW_E_ORDER);
  /* A proper prefix sorts before its extension, not after. */
  t64_jsonw_init(&w, buf, sizeof buf);
  T64_CHECK_INT(t64_jsonw_begin_object(&w), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_key(&w, "ab", 2), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_int(&w, 1), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_key(&w, "a", 1), T64_JSONW_E_ORDER);

  /* Integer bounds: 2^53-1 is the last accepted value. */
  t64_jsonw_init(&w, buf, sizeof buf);
  T64_CHECK_INT(t64_jsonw_int(&w, 9007199254740991LL), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_finish(&w, &n), T64_JSONW_OK);
  T64_CHECK_STR(buf, "9007199254740991\n");
  t64_jsonw_init(&w, buf, sizeof buf);
  T64_CHECK_INT(t64_jsonw_int(&w, -9007199254740991LL), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_finish(&w, &n), T64_JSONW_OK);
  T64_CHECK_STR(buf, "-9007199254740991\n");
  t64_jsonw_init(&w, buf, sizeof buf);
  T64_CHECK_INT(t64_jsonw_int(&w, 9007199254740992LL), T64_JSONW_E_RANGE);
  t64_jsonw_init(&w, buf, sizeof buf);
  T64_CHECK_INT(t64_jsonw_int(&w, -9007199254740992LL), T64_JSONW_E_RANGE);
  t64_jsonw_init(&w, buf, sizeof buf);
  T64_CHECK_INT(t64_jsonw_int(&w, 0), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_finish(&w, &n), T64_JSONW_OK);
  T64_CHECK_STR(buf, "0\n");

  /* Invalid UTF-8 is refused, in values and in keys. */
  t64_jsonw_init(&w, buf, sizeof buf);
  T64_CHECK_INT(t64_jsonw_string(&w, bad_utf8, sizeof bad_utf8),
                T64_JSONW_E_UTF8);
  t64_jsonw_init(&w, buf, sizeof buf);
  T64_CHECK_INT(t64_jsonw_begin_object(&w), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_key(&w, bad_utf8, sizeof bad_utf8), T64_JSONW_E_UTF8);

  /* Structural misuse. */
  t64_jsonw_init(&w, buf, sizeof buf);
  T64_CHECK_INT(t64_jsonw_begin_object(&w), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_int(&w, 1), T64_JSONW_E_STATE); /* value, no key */
  t64_jsonw_init(&w, buf, sizeof buf);
  T64_CHECK_INT(t64_jsonw_begin_array(&w), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_finish(&w, &n), T64_JSONW_E_STATE); /* unclosed */
  t64_jsonw_init(&w, buf, sizeof buf);
  T64_CHECK_INT(t64_jsonw_null(&w), T64_JSONW_OK);
  T64_CHECK_INT(t64_jsonw_null(&w), T64_JSONW_E_STATE); /* second root */

  /* Bounded buffer: overflow is an error, never a write past the end. */
  t64_jsonw_init(&w, tiny, sizeof tiny);
  T64_CHECK_INT(t64_jsonw_string(&w, "abcdefghij", 10), T64_JSONW_E_OVERFLOW);
  T64_CHECK(strlen(tiny) < sizeof tiny);
}

int main(void) {
  test_sha256();
  test_utf8();
  test_jsonw();
  return T64_TEST_FINISH("unit.common");
}
