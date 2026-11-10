/*
 * unit.json-fuzz: deterministic mutation run for the strict JSON parser.
 *
 * 200000 mutants are derived from a fixed positive corpus with a fixed-seed
 * xorshift64* generator (no host entropy, no clock), so a failure reproduces
 * exactly. Mutation operators: byte flip, byte insert, byte delete, truncate,
 * span duplicate. Each mutant is parsed from an exact-size heap copy (so
 * AddressSanitizer sees any over-read). A rejected mutant must carry a valid
 * category and an in-range offset. An accepted mutant must round-trip:
 * canon(m) parses, canon(canon(m)) == canon(m), and the trees are equal.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_json.h"
#include "t64_jsonw.h"
#include "t64_test.h"

#define MUTANTS 200000u
#define MAX_LEN 2048u

static const char *const corpus[] = {
    "{\"a\":1,\"b\":[true,false,null],\"c\":{\"d\":\"x\",\"e\":-42}}",
    "[0,-1,9007199254740991,\"\\u00e9\\ud83d\\ude00\\n\\t\\\"\\\\\\/\"]",
    ("{\"schema\":\"t64.result\",\"schema_version\":1,\"outcome\":\"pass\","
     "\"detail\":\"positive-match\",\"counts\":{\"pass\":2,\"fail\":0}}"),
    " { \"z\" : [ ] , \"y\" : { } , \"x\" : \"\\u0000\\u001f\" }\n",
    "[[[[[[[[[[[[[[[1]]]]]]]]]]]]]]]",
    "{\"k\":\"\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80\",\"n\":\"18446744073709551615\"}",
    "[\"a\",\"b\",\"c\",{\"q\":[1,2,3,4,5,6,7,8,9,10],\"p\":null},false]",
    "{\"a\":{\"b\":{\"c\":{\"d\":{\"e\":{\"f\":[{\"g\":true}]}}}}}}"};

static uint64_t rng_state = 0x9E3779B97F4A7C15ull;

static uint64_t rng(void) {
  uint64_t x = rng_state;
  x ^= x >> 12;
  x ^= x << 25;
  x ^= x >> 27;
  rng_state = x;
  return x * 0x2545F4914F6CDD1Dull;
}

static size_t rnd(size_t n) { return (size_t)(rng() % (uint64_t)n); }

/* Interesting bytes bias flips toward structure, numbers and escapes. */
static uint8_t pick_byte(void) {
  static const uint8_t special[] = {'{', '}', '[', ']', ',', ':', '"', '\\', '-',
                                    '.', 'e', '0', '1', '9', 'u', '/', '*', ' ',
                                    '\t', '\n', 0x00, 0x01, 0x1f, 0x7f, 0x80,
                                    0xbf, 0xc0, 0xc3, 0xe2, 0xed, 0xf0, 0xf4,
                                    0xff, 't', 'f', 'n', 'd', '8'};
  if (rnd(3) == 0) return (uint8_t)rng();
  return special[rnd(sizeof special)];
}

static size_t mutate(uint8_t *buf, size_t len) {
  size_t ops = 1 + rnd(3);
  size_t k;

  for (k = 0; k < ops; k++) {
    switch (rnd(5)) {
      case 0: /* flip */
        if (len > 0) buf[rnd(len)] = pick_byte();
        break;
      case 1: /* insert */
        if (len + 1 <= MAX_LEN) {
          size_t at = rnd(len + 1);
          memmove(buf + at + 1, buf + at, len - at);
          buf[at] = pick_byte();
          len++;
        }
        break;
      case 2: /* delete */
        if (len > 0) {
          size_t at = rnd(len);
          memmove(buf + at, buf + at + 1, len - at - 1);
          len--;
        }
        break;
      case 3: /* truncate */
        if (len > 0) len = rnd(len + 1);
        break;
      default: /* span duplicate */
        if (len > 0) {
          size_t at = rnd(len);
          size_t n = 1 + rnd(len - at < 16 ? len - at : 16);
          if (len + n <= MAX_LEN) {
            /* the original span stays in place, so this duplicates it */
            memmove(buf + at + n, buf + at, len - at);
            len += n;
          }
        }
        break;
    }
  }
  return len;
}

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

/* Emit canonically into a fresh exact-fit-ish buffer; 1 on success. */
static int emit(const t64_json_node *root, size_t in_len, char *out, size_t cap,
                size_t *outlen) {
  t64_jsonw w;
  (void)in_len;
  t64_jsonw_init(&w, out, cap);
  return t64_jsonw_node(&w, root) == T64_JSONW_OK &&
         t64_jsonw_finish(&w, outlen) == T64_JSONW_OK;
}

int main(void) {
  static uint8_t work[MAX_LEN + 64];
  static char canon1[MAX_LEN + 64];
  static char canon2[MAX_LEN + 64];
  unsigned long accepted = 0;
  unsigned long rejected = 0;
  unsigned long cats[32];
  unsigned i;

  memset(cats, 0, sizeof cats);
  /* the unmutated corpus must itself parse and round-trip */
  for (i = 0; i < sizeof corpus / sizeof corpus[0]; i++) {
    t64_json_doc d;
    t64_json_error e;
    T64_CHECK(t64_json_parse((const uint8_t *)corpus[i], strlen(corpus[i]), NULL, &d, &e) == 0);
    t64_json_free(&d);
  }

  for (i = 0; i < MUTANTS; i++) {
    const char *base = corpus[rnd(sizeof corpus / sizeof corpus[0])];
    size_t len = strlen(base);
    uint8_t *copy;
    t64_json_doc d;
    t64_json_error e;
    int rc;

    memcpy(work, base, len);
    len = mutate(work, len);
    copy = (uint8_t *)malloc(len > 0 ? len : 1);
    if (copy == NULL) return 2;
    if (len > 0) memcpy(copy, work, len);
    rc = t64_json_parse(copy, len, NULL, &d, &e);
    free(copy);

    if (rc != 0) {
      rejected++;
      T64_CHECK(e.category != T64_JSON_OK && e.category <= T64_JSON_E_TRAILING_DATA);
      T64_CHECK(e.offset <= len || e.category == T64_JSON_E_SIZE);
      if ((unsigned)e.category < 32u) cats[e.category]++;
      continue;
    }
    accepted++;
    {
      size_t n1 = 0;
      size_t n2 = 0;
      t64_json_doc d2;
      t64_json_error e2;
      int good = emit(d.root, len, canon1, sizeof canon1, &n1);
      T64_CHECK(good);
      if (good) {
        int rc2 = t64_json_parse((const uint8_t *)canon1, n1, NULL, &d2, &e2);
        T64_CHECK(rc2 == 0);
        if (rc2 == 0) {
          int good2 = emit(d2.root, n1, canon2, sizeof canon2, &n2);
          T64_CHECK(good2);
          T64_CHECK(good2 && n1 == n2 && memcmp(canon1, canon2, n1) == 0);
          T64_CHECK(tree_eq(d.root, d2.root));
          T64_CHECK(n1 <= len + 1); /* canonical form never grows past input + LF */
          t64_json_free(&d2);
        }
      }
      t64_json_free(&d);
    }
  }

  printf("json fuzz: mutants=%u accepted=%lu rejected=%lu\n", MUTANTS, accepted,
         rejected);
  {
    unsigned c;
    unsigned seen = 0;
    for (c = 1; c <= (unsigned)T64_JSON_E_TRAILING_DATA; c++) {
      if (cats[c] > 0) seen++;
    }
    printf("json fuzz: categories seen=%u of %u\n", seen,
           (unsigned)T64_JSON_E_TRAILING_DATA);
    /* a run that only ever accepts or only ever rejects exercises one side */
    T64_CHECK(accepted > 1000);
    T64_CHECK(rejected > 1000);
    T64_CHECK(seen >= 12);
  }
  return T64_TEST_FINISH("unit.json-fuzz");
}
