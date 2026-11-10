/*
 * unit.json: independent negative corpus, positive and canonical round-trip
 * cases, helper tests and limit tests for the strict JSON parser.
 *
 * Every expected category and every canonical string below is hand-derived
 * from RFC 8259 and the rules in t64_json.h, not produced by the parser.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_json.h"
#include "t64_jsonw.h"
#include "t64_test.h"

static int negatives;

/* Parse from an exact-size heap copy so ASan sees any out-of-bounds read. */
static int parse_copy(const void *in, size_t len, const t64_json_limits *lim,
                      t64_json_doc *doc, t64_json_error *err) {
  uint8_t *copy = (uint8_t *)malloc(len > 0 ? len : 1);
  int rc;
  if (copy == NULL) {
    fprintf(stderr, "out of memory\n");
    exit(2);
  }
  if (len > 0) memcpy(copy, in, len);
  rc = t64_json_parse(copy, len, lim, doc, err);
  free(copy);
  return rc;
}

/* Expect a rejection with category cat; off < 0 skips the offset check. */
static void neg_lim(const void *in, size_t len, t64_json_category cat, long off,
                    const t64_json_limits *lim, const char *label) {
  t64_json_doc doc;
  t64_json_error err;
  int rc = parse_copy(in, len, lim, &doc, &err);
  negatives++;
  if (rc == 0) {
    fprintf(stderr, "negative accepted: %s\n", label);
    t64_json_free(&doc);
  }
  T64_CHECK(rc != 0);
  if (rc != 0 && err.category != cat) {
    fprintf(stderr, "negative %s: got %s, want %s\n", label,
            t64_json_category_name(err.category), t64_json_category_name(cat));
  }
  T64_CHECK_INT(err.category, cat);
  if (off >= 0) {
    if ((long)err.offset != off) {
      fprintf(stderr, "negative %s: offset %lu, want %ld\n", label,
              (unsigned long)err.offset, off);
    }
    T64_CHECK_INT(err.offset, off);
  }
}

/* sizeof(literal) - 1 keeps embedded NULs and high bytes intact. */
#define NEG(lit, cat, off) neg_lim((lit), sizeof(lit) - 1, (cat), (off), NULL, (lit))

static void ok_lim(const void *in, size_t len, const t64_json_limits *lim,
                   const char *label) {
  t64_json_doc doc;
  t64_json_error err;
  int rc = parse_copy(in, len, lim, &doc, &err);
  if (rc != 0) {
    fprintf(stderr, "positive rejected: %s (%s at %lu)\n", label,
            t64_json_category_name(err.category), (unsigned long)err.offset);
  } else {
    t64_json_free(&doc);
  }
  T64_CHECK(rc == 0);
}

#define OK(lit) ok_lim((lit), sizeof(lit) - 1, NULL, (lit))

/* Parse then emit; returns 1 and fills out/outlen when both succeed. */
static int canon(const void *in, size_t len, char *out, size_t cap,
                 size_t *outlen) {
  t64_json_doc doc;
  t64_json_error err;
  t64_jsonw w;
  int good;

  if (parse_copy(in, len, NULL, &doc, &err) != 0) return 0;
  t64_jsonw_init(&w, out, cap);
  good = t64_jsonw_node(&w, doc.root) == T64_JSONW_OK &&
         t64_jsonw_finish(&w, outlen) == T64_JSONW_OK;
  t64_json_free(&doc);
  return good;
}

/* Structural equality that ignores member order in objects. */
static int tree_eq(const t64_json_node *a, const t64_json_node *b) {
  const t64_json_node *m;
  const t64_json_node *x;
  const t64_json_node *y;

  if (a->type != b->type) return 0;
  switch (a->type) {
    case T64_JSON_NULL: return 1;
    case T64_JSON_BOOL:
    case T64_JSON_INT: return a->num == b->num;
    case T64_JSON_STRING:
      return a->len == b->len && memcmp(a->str, b->str, a->len) == 0;
    case T64_JSON_ARRAY:
      if (a->count != b->count) return 0;
      for (x = a->first, y = b->first; x != NULL && y != NULL;
           x = x->next, y = y->next) {
        if (!tree_eq(x, y)) return 0;
      }
      return 1;
    case T64_JSON_OBJECT:
      if (a->count != b->count) return 0;
      for (m = a->first; m != NULL; m = m->next) {
        const t64_json_node *o = NULL;
        for (y = b->first; y != NULL; y = y->next) {
          if (y->key_len == m->key_len && memcmp(y->key, m->key, m->key_len) == 0) {
            o = y;
            break;
          }
        }
        if (o == NULL || !tree_eq(m, o)) return 0;
      }
      return 1;
  }
  return 0;
}

/* in must canonicalize to want (a NUL-terminated string, LF included). */
static void canon_is(const void *in, size_t len, const char *want) {
  char out[8192];
  char out2[8192];
  size_t n = 0;
  size_t n2 = 0;
  t64_json_doc d1;
  t64_json_doc d2;
  t64_json_error err;

  T64_CHECK(canon(in, len, out, sizeof out, &n));
  if (strcmp(out, want) != 0) {
    fprintf(stderr, "canon mismatch\n  got:  %s\n  want: %s\n", out, want);
  }
  T64_CHECK_STR(out, want);
  T64_CHECK_INT(n, strlen(want));
  /* canon(canon(x)) == canon(x) */
  T64_CHECK(canon(out, n, out2, sizeof out2, &n2));
  T64_CHECK(n2 == n && memcmp(out, out2, n) == 0);
  /* canon output parses back to an equal tree */
  T64_CHECK(parse_copy(in, len, NULL, &d1, &err) == 0);
  T64_CHECK(parse_copy(out, n, NULL, &d2, &err) == 0);
  if (d1.root != NULL && d2.root != NULL) T64_CHECK(tree_eq(d1.root, d2.root));
  t64_json_free(&d1);
  t64_json_free(&d2);
}

#define CANON(in, want) canon_is((in), sizeof(in) - 1, (want))

static void test_negatives(void) {
  /* structure */
  NEG("", T64_JSON_E_SYNTAX, 0);
  NEG("  \n\t ", T64_JSON_E_SYNTAX, 5);
  NEG("[1,]", T64_JSON_E_TRAILING_COMMA, 2);
  NEG("{\"a\":1,}", T64_JSON_E_TRAILING_COMMA, 6);
  NEG("[1, ]", T64_JSON_E_TRAILING_COMMA, 2);
  NEG("[,1]", T64_JSON_E_SYNTAX, 1);
  NEG("[1,,2]", T64_JSON_E_SYNTAX, 3);
  NEG("[1 2]", T64_JSON_E_SYNTAX, 3);
  NEG("{\"a\" 1}", T64_JSON_E_SYNTAX, 5);
  NEG("{a:1}", T64_JSON_E_SYNTAX, 1);
  NEG("{'a':1}", T64_JSON_E_SYNTAX, 1);
  NEG("{\"a\":}", T64_JSON_E_SYNTAX, 5);
  NEG("{\"a\":1 \"b\":2}", T64_JSON_E_SYNTAX, 7);
  NEG("[1", T64_JSON_E_SYNTAX, 2);
  NEG("{\"a\":1", T64_JSON_E_SYNTAX, 6);
  NEG("\"abc", T64_JSON_E_SYNTAX, 4);
  NEG("tru", T64_JSON_E_SYNTAX, 0);
  NEG("True", T64_JSON_E_SYNTAX, 0);
  NEG("[nul]", T64_JSON_E_SYNTAX, 1);
  NEG("[}", T64_JSON_E_SYNTAX, 1);
  NEG("{]", T64_JSON_E_SYNTAX, 1);
  NEG("\x01", T64_JSON_E_SYNTAX, 0);
  NEG("\0", T64_JSON_E_SYNTAX, 0);
  NEG("[1]]", T64_JSON_E_TRAILING_DATA, 3);
  NEG("1 2", T64_JSON_E_TRAILING_DATA, 2);
  NEG("{} x", T64_JSON_E_TRAILING_DATA, 3);
  NEG("nulll", T64_JSON_E_TRAILING_DATA, 4);
  NEG("{}{}", T64_JSON_E_TRAILING_DATA, 2);
  /* comments and BOM */
  NEG("// c\n1", T64_JSON_E_COMMENT, 0);
  NEG("/* c */1", T64_JSON_E_COMMENT, 0);
  NEG("[1 /* c */]", T64_JSON_E_COMMENT, 3);
  NEG("{\"a\":1} // c", T64_JSON_E_COMMENT, 8);
  NEG("{\"a\"/**/:1}", T64_JSON_E_COMMENT, 4);
  NEG("\xEF\xBB\xBF{}", T64_JSON_E_BOM, 0);
  /* numbers */
  NEG("01", T64_JSON_E_LEADING_ZERO, 1);
  NEG("-01", T64_JSON_E_LEADING_ZERO, 2);
  NEG("[00]", T64_JSON_E_LEADING_ZERO, 2);
  NEG("1.0", T64_JSON_E_FLOAT, 1);
  NEG("1.", T64_JSON_E_FLOAT, 1);
  NEG("-1.5", T64_JSON_E_FLOAT, 2);
  NEG("0.5", T64_JSON_E_FLOAT, 1);
  NEG("1e3", T64_JSON_E_EXPONENT, 1);
  NEG("1E3", T64_JSON_E_EXPONENT, 1);
  NEG("[2e+3]", T64_JSON_E_EXPONENT, 2);
  NEG("-0", T64_JSON_E_NEGATIVE_ZERO, 0);
  NEG("[-0]", T64_JSON_E_NEGATIVE_ZERO, 1);
  NEG("9007199254740992", T64_JSON_E_INT_RANGE, 0); /* 2^53 */
  NEG("-9007199254740992", T64_JSON_E_INT_RANGE, 0);
  NEG("99999999999999999999999", T64_JSON_E_INT_RANGE, 0);
  NEG("-", T64_JSON_E_SYNTAX, 1);
  NEG("+1", T64_JSON_E_SYNTAX, 0);
  NEG(".5", T64_JSON_E_SYNTAX, 0);
  /* escapes */
  NEG("\"\\q\"", T64_JSON_E_BAD_ESCAPE, 1);
  NEG("\"\\x41\"", T64_JSON_E_BAD_ESCAPE, 1);
  NEG("\"\\u12\"", T64_JSON_E_BAD_ESCAPE, 1);
  NEG("\"\\u12G4\"", T64_JSON_E_BAD_ESCAPE, 1);
  NEG("\"\\", T64_JSON_E_BAD_ESCAPE, 1);
  NEG("\"\\ud800\"", T64_JSON_E_LONE_SURROGATE, 1);
  NEG("\"\\udc00\"", T64_JSON_E_LONE_SURROGATE, 1);
  NEG("\"\\ud800\\u0041\"", T64_JSON_E_LONE_SURROGATE, 1);
  NEG("\"\\ud800x\"", T64_JSON_E_LONE_SURROGATE, 1);
  NEG("\"\\ud800\\udc0\"", T64_JSON_E_BAD_ESCAPE, 7);
  NEG("\"\\ude00\\ud83d\"", T64_JSON_E_LONE_SURROGATE, 1);
  /* raw control characters */
  NEG("\"a\tb\"", T64_JSON_E_CONTROL_CHAR, 2);
  NEG("\"a\nb\"", T64_JSON_E_CONTROL_CHAR, 2);
  NEG("\"\x01\"", T64_JSON_E_CONTROL_CHAR, 1);
  NEG("{\"k\tx\":1}", T64_JSON_E_CONTROL_CHAR, 3);
  /* UTF-8 */
  NEG("\"\xff\"", T64_JSON_E_INVALID_UTF8, 1);
  NEG("\"\xc0\x80\"", T64_JSON_E_INVALID_UTF8, 1);   /* overlong NUL */
  NEG("\"\xed\xa0\x80\"", T64_JSON_E_INVALID_UTF8, 1); /* encoded surrogate */
  NEG("\"\xc3\"", T64_JSON_E_INVALID_UTF8, 1);        /* truncated */
  NEG("\"\xf4\x90\x80\x80\"", T64_JSON_E_INVALID_UTF8, 1); /* above U+10FFFF */
  NEG("\"\x80\"", T64_JSON_E_INVALID_UTF8, 1);        /* stray continuation */
  NEG("{\"\xff\":1}", T64_JSON_E_INVALID_UTF8, 2);
  NEG("{\"k\":\"\xe2\x82\"}", T64_JSON_E_INVALID_UTF8, 6);
  /* duplicates */
  NEG("{\"a\":1,\"a\":2}", T64_JSON_E_DUPLICATE_KEY, 7);
  NEG("{\"x\":{\"a\":1,\"b\":2,\"a\":3}}", T64_JSON_E_DUPLICATE_KEY, 18);
  NEG("{\"a\":1,\"\\u0061\":2}", T64_JSON_E_DUPLICATE_KEY, 7);
  NEG("[{\"k\":1,\"k\":1}]", T64_JSON_E_DUPLICATE_KEY, 8);
}

static void test_limit_negatives(void) {
  char buf[16384];
  t64_json_limits lim;
  size_t i;
  size_t n;

  /* depth 17 */
  memset(buf, '[', 17);
  memset(buf + 17, ']', 17);
  neg_lim(buf, 34, T64_JSON_E_DEPTH, 16, NULL, "depth 17 arrays");
  n = 0;
  for (i = 0; i < 17; i++) {
    memcpy(buf + n, "{\"a\":", 5);
    n += 5;
  }
  buf[n++] = '1';
  memset(buf + n, '}', 17);
  neg_lim(buf, n + 17, T64_JSON_E_DEPTH, 80, NULL, "depth 17 objects");

  /* a 4097-byte string value, and a 4097-byte key budget case via the key cap */
  buf[0] = '"';
  memset(buf + 1, 'a', 4097);
  buf[4098] = '"';
  neg_lim(buf, 4099, T64_JSON_E_STRING_LENGTH, 0, NULL, "string 4097 bytes");
  /* escapes count as decoded bytes: 4097 x \n is 8194 raw bytes, 4097 decoded */
  buf[0] = '"';
  for (i = 0; i < 4097; i++) memcpy(buf + 1 + 2 * i, "\\n", 2);
  buf[1 + 2 * 4097] = '"';
  neg_lim(buf, 2 + 2 * 4097, T64_JSON_E_STRING_LENGTH, 0, NULL, "escaped string 4097");
  /* a key of 128 bytes exceeds the writer's 127-byte key cap */
  n = 0;
  memcpy(buf, "{\"", 2);
  n = 2;
  memset(buf + n, 'k', 128);
  n += 128;
  memcpy(buf + n, "\":1}", 4);
  n += 4;
  neg_lim(buf, n, T64_JSON_E_STRING_LENGTH, 1, NULL, "key 128 bytes");

  /* 4097 nodes: the array plus 4096 integers */
  n = 0;
  buf[n++] = '[';
  for (i = 0; i < 4096; i++) {
    buf[n++] = '1';
    buf[n++] = ',';
  }
  buf[n - 1] = ']';
  neg_lim(buf, n, T64_JSON_E_NODES, -1, NULL, "4097 nodes");

  /* input one byte over 1 MiB */
  {
    size_t big = T64_JSON_DEFAULT_MAX_INPUT + 1;
    char *p = (char *)malloc(big);
    T64_CHECK(p != NULL);
    if (p != NULL) {
      memset(p, ' ', big);
      p[0] = '1';
      neg_lim(p, big, T64_JSON_E_SIZE, -1, NULL, "input 1 MiB + 1");
      ok_lim(p, big - 1, NULL, "input exactly 1 MiB");
      free(p);
    }
  }

  /* raised and lowered limits are honoured */
  t64_json_limits_default(&lim);
  lim.max_depth = 20;
  ok_lim("[[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]]", 34, &lim, "depth 17 with raised limit");
  t64_json_limits_default(&lim);
  lim.max_string = 3;
  neg_lim("\"abcd\"", 6, T64_JSON_E_STRING_LENGTH, 0, &lim, "lowered string limit");
  t64_json_limits_default(&lim);
  lim.max_nodes = 3;
  neg_lim("[1,2,3]", 7, T64_JSON_E_NODES, -1, &lim, "lowered node limit");
  t64_json_limits_default(&lim);
  lim.max_input = 4;
  neg_lim("[1,2,3]", 7, T64_JSON_E_SIZE, -1, &lim, "lowered input limit");
  t64_json_limits_default(&lim);
  lim.max_depth = 1000; /* clamped to the hard maximum, still bounded */
  memset(buf, '[', 100);
  memset(buf + 100, ']', 100);
  neg_lim(buf, 200, T64_JSON_E_DEPTH, T64_JSON_HARD_MAX_DEPTH, &lim, "hard depth clamp");
}

static void test_positives(void) {
  OK("0");
  OK("-1");
  OK("9007199254740991");
  OK("-9007199254740991");
  OK("true");
  OK("false");
  OK("null");
  OK("\"\"");
  OK("[]");
  OK("{}");
  OK(" \t\r\n[ 1 , 2 ]\n");
  OK("{\"a\":{\"a\":1},\"b\":{\"a\":2}}"); /* same key in different objects */
  OK("[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]");  /* depth 16 */
  OK("\"\\u0000\"");                       /* NUL via escape */

  {
    char buf[8192];
    size_t i;
    size_t n = 0;
    buf[n++] = '[';
    for (i = 0; i < 4095; i++) {
      buf[n++] = '1';
      buf[n++] = ',';
    }
    buf[n - 1] = ']';
    ok_lim(buf, n, NULL, "4096 nodes");
    buf[0] = '"';
    memset(buf + 1, 'a', 4096);
    buf[4097] = '"';
    ok_lim(buf, 4098, NULL, "string 4096 bytes");
    n = 0;
    memcpy(buf, "{\"", 2);
    n = 2;
    memset(buf + n, 'k', 127);
    n += 127;
    memcpy(buf + n, "\":1}", 4);
    n += 4;
    ok_lim(buf, n, NULL, "key 127 bytes");
  }
}

static void test_canon(void) {
  char want[64];
  char in[16];
  unsigned c;

  CANON("{\"b\":1,\"a\":[ true , false , null ]}", "{\"a\":[true,false,null],\"b\":1}\n");
  CANON(" [ ] ", "[]\n");
  CANON("{}", "{}\n");
  CANON("\r\n\t 7 \r\n", "7\n");
  CANON("\"\\u00e9A\\/\"", "\"\xc3\xa9" "A/\"\n");
  CANON("\"\\ud83d\\ude00\"", "\"\xf0\x9f\x98\x80\"\n");
  CANON("\"\\uD83D\\uDE00\"", "\"\xf0\x9f\x98\x80\"\n");
  CANON("\"\\u007f\"", "\"\x7f\"\n");
  CANON("\"\\u0000\\u001f\\b\\f\\n\\r\\t\\\"\\\\\"",
        "\"\\u0000\\u001f\\b\\f\\n\\r\\t\\\"\\\\\"\n");
  CANON("[0,-1,9007199254740991,-9007199254740991]",
        "[0,-1,9007199254740991,-9007199254740991]\n");
  CANON("{\"ab\":1,\"a\":2}", "{\"a\":2,\"ab\":1}\n"); /* a proper prefix sorts first */
  CANON("{\"z\":{\"y\":1,\"x\":2},\"a\":[{\"d\":1,\"c\":2}]}",
        "{\"a\":[{\"c\":2,\"d\":1}],\"z\":{\"x\":2,\"y\":1}}\n");
  CANON("{\"\\u0062\":1,\"a\":2}", "{\"a\":2,\"b\":1}\n");
  CANON("{\"\xc3\xa9\":1,\"z\":2}", "{\"z\":2,\"\xc3\xa9\":1}\n"); /* 0x7a < 0xc3 */
  CANON("{\"b\":1,\"a\":2,\"c\":3,\"A\":4}", "{\"A\":4,\"a\":2,\"b\":1,\"c\":3}\n");
  CANON("[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]", "[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]\n");

  /* every escape of a single control character canonicalizes as specified */
  for (c = 0; c < 0x20u; c++) {
    int n = snprintf(in, sizeof in, "\"\\u%04x\"", c);
    switch (c) {
      case 0x08: snprintf(want, sizeof want, "\"\\b\"\n"); break;
      case 0x09: snprintf(want, sizeof want, "\"\\t\"\n"); break;
      case 0x0a: snprintf(want, sizeof want, "\"\\n\"\n"); break;
      case 0x0c: snprintf(want, sizeof want, "\"\\f\"\n"); break;
      case 0x0d: snprintf(want, sizeof want, "\"\\r\"\n"); break;
      default: snprintf(want, sizeof want, "\"\\u%04x\"\n", c); break;
    }
    T64_CHECK(n > 0);
    canon_is(in, (size_t)n, want);
  }
}

static void test_helpers(void) {
  t64_json_doc doc;
  t64_json_error err;
  const char doc_text[] =
      "{\"name\":\"x\",\"n\":3,\"big\":\"18446744073709551615\",\"o\":{}}";
  const char *allowed[] = {"name", "n", "big", "o", "extra"};
  const char *req_ok[] = {"name", "n"};
  const char *req_bad[] = {"name", "zzz"};
  const char *small_allowed[] = {"name", "n"};
  const char *bad = NULL;
  uint64_t v = 0;
  static const char *u64_bad[] = {"\"\"",
                                  "\"01\"",
                                  "\"-1\"",
                                  "\"1a\"",
                                  "\" 1\"",
                                  "\"18446744073709551616\"",
                                  "\"99999999999999999999\"",
                                  "\"184467440737095516150\"",
                                  "\"0x10\"",
                                  "5"};
  static const char *u64_good[] = {"\"0\"", "\"7\"", "\"9007199254740993\"",
                                   "\"18446744073709551615\""};
  static const uint64_t u64_val[] = {0u, 7u, 9007199254740993u,
                                     18446744073709551615u};
  size_t i;

  T64_CHECK(parse_copy(doc_text, sizeof doc_text - 1, NULL, &doc, &err) == 0);
  if (doc.root == NULL) return;
  T64_CHECK(t64_json_get(doc.root, "name") != NULL);
  T64_CHECK(t64_json_get(doc.root, "nope") == NULL);
  T64_CHECK(t64_json_get(doc.root, "n")->num == 3);
  T64_CHECK(t64_json_get(doc.root->first, "name") == NULL); /* not an object */
  T64_CHECK_INT(t64_json_check_keys(doc.root, allowed, 5, req_ok, 2, &bad),
                T64_JSON_KEYS_OK);
  T64_CHECK_INT(t64_json_check_keys(doc.root, allowed, 5, req_bad, 2, &bad),
                T64_JSON_KEYS_MISSING);
  T64_CHECK_STR(bad, "zzz");
  T64_CHECK_INT(t64_json_check_keys(doc.root, small_allowed, 2, req_ok, 2, &bad),
                T64_JSON_KEYS_UNKNOWN);
  T64_CHECK_STR(bad, "big");
  T64_CHECK_INT(t64_json_check_keys(doc.root->first, allowed, 5, req_ok, 2, &bad),
                T64_JSON_KEYS_NOT_OBJECT);
  T64_CHECK(t64_json_u64_string(t64_json_get(doc.root, "big"), &v) == 1);
  T64_CHECK(v == 18446744073709551615u);
  T64_CHECK(t64_json_u64_string(t64_json_get(doc.root, "n"), &v) == 0);
  t64_json_free(&doc);

  for (i = 0; i < sizeof u64_good / sizeof u64_good[0]; i++) {
    T64_CHECK(parse_copy(u64_good[i], strlen(u64_good[i]), NULL, &doc, &err) == 0);
    T64_CHECK(t64_json_u64_string(doc.root, &v) == 1);
    T64_CHECK(v == u64_val[i]);
    t64_json_free(&doc);
  }
  for (i = 0; i < sizeof u64_bad / sizeof u64_bad[0]; i++) {
    T64_CHECK(parse_copy(u64_bad[i], strlen(u64_bad[i]), NULL, &doc, &err) == 0);
    T64_CHECK(t64_json_u64_string(doc.root, &v) == 0);
    t64_json_free(&doc);
  }
  T64_CHECK(t64_json_u64_string(NULL, &v) == 0);
}

static void test_names(void) {
  /* the closed category set */
  static const char *const names[] = {
      "ok",           "syntax",         "trailing-comma", "comment",
      "bom",          "control-char",   "invalid-utf8",   "bad-escape",
      "lone-surrogate", "float",        "exponent",       "leading-zero",
      "negative-zero", "int-range",     "duplicate-key",  "depth",
      "size",         "string-length",  "nodes",          "trailing-data"};
  size_t i;
  for (i = 0; i < sizeof names / sizeof names[0]; i++) {
    T64_CHECK_STR(t64_json_category_name((t64_json_category)i), names[i]);
  }
}

int main(void) {
  test_negatives();
  test_limit_negatives();
  test_positives();
  test_canon();
  test_helpers();
  test_names();
  printf("json negatives: %d\n", negatives);
  T64_CHECK(negatives >= 40);
  return T64_TEST_FINISH("unit.json");
}
