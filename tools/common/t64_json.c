/* Strict bounded JSON parser; see t64_json.h for the accepted language. */
#include "t64_json.h"

#include <stdlib.h>
#include <string.h>

#include "t64_utf8.h"

typedef struct {
  const uint8_t *buf;
  size_t len;
  size_t pos;
  size_t max_depth;
  size_t max_string;
  size_t max_nodes;
  t64_json_node *nodes;
  size_t nnodes;
  size_t cap; /* node array capacity: never more nodes than input bytes */
  char *pool;
  size_t poolpos;
  size_t poolcap;
  t64_json_category cat;
  size_t off;
} parser;

static void fail(parser *p, t64_json_category c, size_t off) {
  if (p->cat == T64_JSON_OK) {
    p->cat = c;
    p->off = off;
  }
}

/* Failure at the current position: a comment opener or generic syntax. */
static void unexpected(parser *p) {
  size_t o = p->pos;
  if (o + 1 < p->len && p->buf[o] == '/' &&
      (p->buf[o + 1] == '/' || p->buf[o + 1] == '*')) {
    fail(p, T64_JSON_E_COMMENT, o);
  } else {
    fail(p, T64_JSON_E_SYNTAX, o);
  }
}

static void skip_ws(parser *p) {
  while (p->pos < p->len) {
    uint8_t c = p->buf[p->pos];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      p->pos++;
    } else {
      break;
    }
  }
}

static int is_digit(uint8_t c) { return c >= '0' && c <= '9'; }

static t64_json_node *new_node(parser *p, size_t off, t64_json_type type) {
  t64_json_node *n;
  if (p->nnodes >= p->max_nodes || p->nnodes >= p->cap) {
    fail(p, T64_JSON_E_NODES, off);
    return NULL;
  }
  n = &p->nodes[p->nnodes++];
  memset(n, 0, sizeof *n);
  n->type = type;
  return n;
}

/* Value of hex digit c, or -1. */
static int hexval(uint8_t c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

/* Four hex digits at buf[i..i+4), or -1 when short or not hex. */
static long hex4(const parser *p, size_t i) {
  long v = 0;
  size_t k;
  if (i > p->len || p->len - i < 4) return -1;
  for (k = 0; k < 4; k++) {
    int d = hexval(p->buf[i + k]);
    if (d < 0) return -1;
    v = v * 16 + d;
  }
  return v;
}

static size_t utf8_encode(unsigned long cp, uint8_t out[4]) {
  if (cp < 0x80ul) {
    out[0] = (uint8_t)cp;
    return 1;
  }
  if (cp < 0x800ul) {
    out[0] = (uint8_t)(0xC0ul | (cp >> 6));
    out[1] = (uint8_t)(0x80ul | (cp & 0x3Ful));
    return 2;
  }
  if (cp < 0x10000ul) {
    out[0] = (uint8_t)(0xE0ul | (cp >> 12));
    out[1] = (uint8_t)(0x80ul | ((cp >> 6) & 0x3Ful));
    out[2] = (uint8_t)(0x80ul | (cp & 0x3Ful));
    return 3;
  }
  out[0] = (uint8_t)(0xF0ul | (cp >> 18));
  out[1] = (uint8_t)(0x80ul | ((cp >> 12) & 0x3Ful));
  out[2] = (uint8_t)(0x80ul | ((cp >> 6) & 0x3Ful));
  out[3] = (uint8_t)(0x80ul | (cp & 0x3Ful));
  return 4;
}

/*
 * Parses the string starting at the opening quote into the pool. maxlen bounds
 * the decoded length. Returns 1 on success and stores the pooled bytes.
 */
static int parse_string(parser *p, size_t maxlen, const char **out,
                        size_t *outlen) {
  size_t start = p->pos;
  size_t i = start + 1;
  size_t n = 0;
  char *dst = p->pool + p->poolpos;
  const uint8_t *b = p->buf;

  for (;;) {
    uint8_t c;
    uint8_t enc[4];
    size_t k = 0;

    if (i >= p->len) {
      fail(p, T64_JSON_E_SYNTAX, i); /* unterminated string */
      return 0;
    }
    c = b[i];
    if (c == '"') {
      i++;
      break;
    }
    if (c < 0x20u) {
      fail(p, T64_JSON_E_CONTROL_CHAR, i);
      return 0;
    }
    if (c == '\\') {
      uint8_t e;
      size_t adv;
      if (i + 1 >= p->len) {
        fail(p, T64_JSON_E_BAD_ESCAPE, i);
        return 0;
      }
      e = b[i + 1];
      adv = 2;
      switch (e) {
        case '"': enc[0] = '"'; k = 1; break;
        case '\\': enc[0] = '\\'; k = 1; break;
        case '/': enc[0] = '/'; k = 1; break;
        case 'b': enc[0] = '\b'; k = 1; break;
        case 'f': enc[0] = '\f'; k = 1; break;
        case 'n': enc[0] = '\n'; k = 1; break;
        case 'r': enc[0] = '\r'; k = 1; break;
        case 't': enc[0] = '\t'; k = 1; break;
        case 'u': {
          long hi = hex4(p, i + 2);
          unsigned long cp;
          if (hi < 0) {
            fail(p, T64_JSON_E_BAD_ESCAPE, i);
            return 0;
          }
          cp = (unsigned long)hi;
          adv = 6;
          if (cp >= 0xDC00ul && cp <= 0xDFFFul) {
            fail(p, T64_JSON_E_LONE_SURROGATE, i);
            return 0;
          }
          if (cp >= 0xD800ul && cp <= 0xDBFFul) {
            long lo;
            if (i + 7 < p->len && b[i + 6] == '\\' && b[i + 7] == 'u') {
              lo = hex4(p, i + 8);
              if (lo < 0) {
                fail(p, T64_JSON_E_BAD_ESCAPE, i + 6);
                return 0;
              }
              if (lo < 0xDC00l || lo > 0xDFFFl) {
                fail(p, T64_JSON_E_LONE_SURROGATE, i);
                return 0;
              }
              cp = 0x10000ul + ((cp - 0xD800ul) << 10) +
                   ((unsigned long)lo - 0xDC00ul);
              adv = 12;
            } else {
              fail(p, T64_JSON_E_LONE_SURROGATE, i);
              return 0;
            }
          }
          k = utf8_encode(cp, enc);
          break;
        }
        default:
          fail(p, T64_JSON_E_BAD_ESCAPE, i);
          return 0;
      }
      i += adv;
    } else if (c >= 0x80u) {
      size_t seq = c >= 0xF0u ? 4u : (c >= 0xE0u ? 3u : (c >= 0xC0u ? 2u : 1u));
      size_t bad = 0;
      if (seq > p->len - i) seq = p->len - i;
      if (!t64_utf8_valid(b + i, seq, &bad)) {
        fail(p, T64_JSON_E_INVALID_UTF8, i);
        return 0;
      }
      memcpy(enc, b + i, seq);
      k = seq;
      i += seq;
    } else {
      enc[0] = c;
      k = 1;
      i++;
    }

    if (k > maxlen || n > maxlen - k) {
      fail(p, T64_JSON_E_STRING_LENGTH, start);
      return 0;
    }
    if (p->poolpos + n + k >= p->poolcap) { /* defensive: leave room for NUL */
      fail(p, T64_JSON_E_SIZE, start);
      return 0;
    }
    memcpy(dst + n, enc, k);
    n += k;
  }
  dst[n] = '\0';
  p->poolpos += n + 1;
  p->pos = i;
  *out = dst;
  *outlen = n;
  return 1;
}

static t64_json_node *parse_number(parser *p) {
  const uint8_t *b = p->buf;
  size_t start = p->pos;
  size_t i = start;
  int neg = 0;
  int over = 0;
  uint64_t v = 0;
  t64_json_node *n;

  if (b[i] == '-') {
    neg = 1;
    i++;
  }
  if (i >= p->len || !is_digit(b[i])) {
    fail(p, T64_JSON_E_SYNTAX, i);
    return NULL;
  }
  if (b[i] == '0') {
    i++;
    if (i < p->len && is_digit(b[i])) {
      fail(p, T64_JSON_E_LEADING_ZERO, i);
      return NULL;
    }
  } else {
    while (i < p->len && is_digit(b[i])) {
      uint64_t d = (uint64_t)(b[i] - '0');
      if (!over) {
        if (v > ((uint64_t)T64_JSON_MAX_INT - d) / 10u) {
          over = 1;
        } else {
          v = v * 10u + d;
        }
      }
      i++;
    }
  }
  if (i < p->len && b[i] == '.') {
    fail(p, T64_JSON_E_FLOAT, i);
    return NULL;
  }
  if (i < p->len && (b[i] == 'e' || b[i] == 'E')) {
    fail(p, T64_JSON_E_EXPONENT, i);
    return NULL;
  }
  if (neg && v == 0 && !over) {
    fail(p, T64_JSON_E_NEGATIVE_ZERO, start);
    return NULL;
  }
  if (over) {
    fail(p, T64_JSON_E_INT_RANGE, start);
    return NULL;
  }
  n = new_node(p, start, T64_JSON_INT);
  if (n == NULL) return NULL;
  n->num = neg ? -(int64_t)v : (int64_t)v;
  p->pos = i;
  return n;
}

static t64_json_node *parse_value(parser *p, size_t depth);

static int match_literal(parser *p, const char *lit) {
  size_t n = strlen(lit);
  if (p->len - p->pos >= n && memcmp(p->buf + p->pos, lit, n) == 0) {
    p->pos += n;
    return 1;
  }
  return 0;
}

static t64_json_node *parse_array(parser *p, size_t depth) {
  size_t open = p->pos;
  t64_json_node *arr;
  t64_json_node *tail = NULL;

  if (depth + 1 > p->max_depth) {
    fail(p, T64_JSON_E_DEPTH, open);
    return NULL;
  }
  arr = new_node(p, open, T64_JSON_ARRAY);
  if (arr == NULL) return NULL;
  p->pos++;
  skip_ws(p);
  if (p->pos < p->len && p->buf[p->pos] == ']') {
    p->pos++;
    return arr;
  }
  for (;;) {
    t64_json_node *v = parse_value(p, depth + 1);
    if (v == NULL) return NULL;
    if (tail == NULL) {
      arr->first = v;
    } else {
      tail->next = v;
    }
    tail = v;
    arr->count++;
    skip_ws(p);
    if (p->pos >= p->len) {
      fail(p, T64_JSON_E_SYNTAX, p->pos);
      return NULL;
    }
    if (p->buf[p->pos] == ',') {
      size_t comma = p->pos;
      p->pos++;
      skip_ws(p);
      if (p->pos < p->len && p->buf[p->pos] == ']') {
        fail(p, T64_JSON_E_TRAILING_COMMA, comma);
        return NULL;
      }
      continue;
    }
    if (p->buf[p->pos] == ']') {
      p->pos++;
      return arr;
    }
    unexpected(p);
    return NULL;
  }
}

static t64_json_node *parse_object(parser *p, size_t depth) {
  size_t open = p->pos;
  size_t klimit = p->max_string < T64_JSON_MAX_KEY ? p->max_string
                                                    : (size_t)T64_JSON_MAX_KEY;
  t64_json_node *obj;
  t64_json_node *tail = NULL;

  if (depth + 1 > p->max_depth) {
    fail(p, T64_JSON_E_DEPTH, open);
    return NULL;
  }
  obj = new_node(p, open, T64_JSON_OBJECT);
  if (obj == NULL) return NULL;
  p->pos++;
  skip_ws(p);
  if (p->pos < p->len && p->buf[p->pos] == '}') {
    p->pos++;
    return obj;
  }
  for (;;) {
    size_t kstart;
    const char *key;
    size_t klen;
    t64_json_node *v;
    t64_json_node *m;

    skip_ws(p);
    if (p->pos >= p->len || p->buf[p->pos] != '"') {
      unexpected(p);
      return NULL;
    }
    kstart = p->pos;
    if (!parse_string(p, klimit, &key, &klen)) return NULL;
    skip_ws(p);
    if (p->pos >= p->len || p->buf[p->pos] != ':') {
      unexpected(p);
      return NULL;
    }
    p->pos++;
    for (m = obj->first; m != NULL; m = m->next) {
      if (m->key_len == klen && memcmp(m->key, key, klen) == 0) {
        fail(p, T64_JSON_E_DUPLICATE_KEY, kstart);
        return NULL;
      }
    }
    v = parse_value(p, depth + 1);
    if (v == NULL) return NULL;
    v->key = key;
    v->key_len = klen;
    if (tail == NULL) {
      obj->first = v;
    } else {
      tail->next = v;
    }
    tail = v;
    obj->count++;
    skip_ws(p);
    if (p->pos >= p->len) {
      fail(p, T64_JSON_E_SYNTAX, p->pos);
      return NULL;
    }
    if (p->buf[p->pos] == ',') {
      size_t comma = p->pos;
      p->pos++;
      skip_ws(p);
      if (p->pos < p->len && p->buf[p->pos] == '}') {
        fail(p, T64_JSON_E_TRAILING_COMMA, comma);
        return NULL;
      }
      continue;
    }
    if (p->buf[p->pos] == '}') {
      p->pos++;
      return obj;
    }
    unexpected(p);
    return NULL;
  }
}

static t64_json_node *parse_value(parser *p, size_t depth) {
  uint8_t c;
  t64_json_node *n;

  skip_ws(p);
  if (p->pos >= p->len) {
    fail(p, T64_JSON_E_SYNTAX, p->pos);
    return NULL;
  }
  c = p->buf[p->pos];
  switch (c) {
    case '{': return parse_object(p, depth);
    case '[': return parse_array(p, depth);
    case '"': {
      size_t start = p->pos;
      const char *s;
      size_t slen;
      n = new_node(p, start, T64_JSON_STRING);
      if (n == NULL) return NULL;
      if (!parse_string(p, p->max_string, &s, &slen)) return NULL;
      n->str = s;
      n->len = slen;
      return n;
    }
    case 't':
    case 'f':
    case 'n': {
      size_t start = p->pos;
      t64_json_type t = c == 'n' ? T64_JSON_NULL : T64_JSON_BOOL;
      int val = c == 't';
      const char *lit = c == 't' ? "true" : (c == 'f' ? "false" : "null");
      if (!match_literal(p, lit)) {
        fail(p, T64_JSON_E_SYNTAX, start);
        return NULL;
      }
      n = new_node(p, start, t);
      if (n == NULL) return NULL;
      n->num = val;
      return n;
    }
    default:
      if (c == '-' || is_digit(c)) return parse_number(p);
      unexpected(p);
      return NULL;
  }
}

void t64_json_limits_default(t64_json_limits *lim) {
  lim->max_input = T64_JSON_DEFAULT_MAX_INPUT;
  lim->max_depth = T64_JSON_DEFAULT_MAX_DEPTH;
  lim->max_string = T64_JSON_DEFAULT_MAX_STRING;
  lim->max_nodes = T64_JSON_DEFAULT_MAX_NODES;
}

const char *t64_json_category_name(t64_json_category c) {
  switch (c) {
    case T64_JSON_OK: return "ok";
    case T64_JSON_E_SYNTAX: return "syntax";
    case T64_JSON_E_TRAILING_COMMA: return "trailing-comma";
    case T64_JSON_E_COMMENT: return "comment";
    case T64_JSON_E_BOM: return "bom";
    case T64_JSON_E_CONTROL_CHAR: return "control-char";
    case T64_JSON_E_INVALID_UTF8: return "invalid-utf8";
    case T64_JSON_E_BAD_ESCAPE: return "bad-escape";
    case T64_JSON_E_LONE_SURROGATE: return "lone-surrogate";
    case T64_JSON_E_FLOAT: return "float";
    case T64_JSON_E_EXPONENT: return "exponent";
    case T64_JSON_E_LEADING_ZERO: return "leading-zero";
    case T64_JSON_E_NEGATIVE_ZERO: return "negative-zero";
    case T64_JSON_E_INT_RANGE: return "int-range";
    case T64_JSON_E_DUPLICATE_KEY: return "duplicate-key";
    case T64_JSON_E_DEPTH: return "depth";
    case T64_JSON_E_SIZE: return "size";
    case T64_JSON_E_STRING_LENGTH: return "string-length";
    case T64_JSON_E_NODES: return "nodes";
    case T64_JSON_E_TRAILING_DATA: return "trailing-data";
  }
  return "syntax";
}

int t64_json_parse(const uint8_t *buf, size_t len, const t64_json_limits *lim,
                   t64_json_doc *out, t64_json_error *err) {
  t64_json_limits L;
  parser p;
  t64_json_node *root;

  memset(out, 0, sizeof *out);
  if (err != NULL) {
    err->category = T64_JSON_OK;
    err->offset = 0;
  }
  if (lim != NULL) {
    L = *lim;
  } else {
    t64_json_limits_default(&L);
  }
  if (L.max_depth > T64_JSON_HARD_MAX_DEPTH) L.max_depth = T64_JSON_HARD_MAX_DEPTH;

  memset(&p, 0, sizeof p);
  p.buf = buf;
  p.len = len;
  p.max_depth = L.max_depth;
  p.max_string = L.max_string;
  p.max_nodes = L.max_nodes;

  if (len > L.max_input) {
    fail(&p, T64_JSON_E_SIZE, L.max_input);
  } else if (len >= 3 && buf[0] == 0xEF && buf[1] == 0xBB && buf[2] == 0xBF) {
    fail(&p, T64_JSON_E_BOM, 0);
  } else if (len == 0 || buf == NULL) {
    fail(&p, T64_JSON_E_SYNTAX, 0);
  }

  if (p.cat == T64_JSON_OK) {
    /* Every value needs at least one input byte, so len bounds the node count. */
    p.cap = len < p.max_nodes ? len : p.max_nodes;
    p.poolcap = len + 1;
    p.nodes = (t64_json_node *)malloc(p.cap * sizeof *p.nodes);
    p.pool = (char *)malloc(p.poolcap);
    if (p.nodes == NULL || p.pool == NULL) {
      fail(&p, T64_JSON_E_SIZE, 0);
    }
  }

  root = NULL;
  if (p.cat == T64_JSON_OK) {
    root = parse_value(&p, 0);
    if (root != NULL) {
      skip_ws(&p);
      if (p.pos < p.len) {
        if (p.pos + 1 < p.len && p.buf[p.pos] == '/' &&
            (p.buf[p.pos + 1] == '/' || p.buf[p.pos + 1] == '*')) {
          fail(&p, T64_JSON_E_COMMENT, p.pos);
        } else {
          fail(&p, T64_JSON_E_TRAILING_DATA, p.pos);
        }
      }
    }
  }

  if (p.cat != T64_JSON_OK || root == NULL) {
    if (err != NULL) {
      err->category = p.cat == T64_JSON_OK ? T64_JSON_E_SYNTAX : p.cat;
      err->offset = p.off;
    }
    free(p.nodes);
    free(p.pool);
    return 1;
  }
  out->root = root;
  out->n_nodes = p.nnodes;
  out->nodes_ = p.nodes;
  out->pool_ = p.pool;
  return 0;
}

void t64_json_free(t64_json_doc *doc) {
  free(doc->nodes_);
  free(doc->pool_);
  memset(doc, 0, sizeof *doc);
}

const t64_json_node *t64_json_get(const t64_json_node *obj, const char *key) {
  const t64_json_node *m;
  size_t klen;

  if (obj == NULL || obj->type != T64_JSON_OBJECT || key == NULL) return NULL;
  klen = strlen(key);
  for (m = obj->first; m != NULL; m = m->next) {
    if (m->key_len == klen && memcmp(m->key, key, klen) == 0) return m;
  }
  return NULL;
}

t64_json_keys_status t64_json_check_keys(const t64_json_node *obj,
                                         const char *const *allowed,
                                         size_t n_allowed,
                                         const char *const *required,
                                         size_t n_required,
                                         const char **bad_key) {
  const t64_json_node *m;
  size_t i;

  if (obj == NULL || obj->type != T64_JSON_OBJECT) return T64_JSON_KEYS_NOT_OBJECT;
  for (m = obj->first; m != NULL; m = m->next) {
    int found = 0;
    for (i = 0; i < n_allowed; i++) {
      size_t alen = strlen(allowed[i]);
      if (alen == m->key_len && memcmp(allowed[i], m->key, alen) == 0) {
        found = 1;
        break;
      }
    }
    if (!found) {
      if (bad_key != NULL) *bad_key = m->key;
      return T64_JSON_KEYS_UNKNOWN;
    }
  }
  for (i = 0; i < n_required; i++) {
    if (t64_json_get(obj, required[i]) == NULL) {
      if (bad_key != NULL) *bad_key = required[i];
      return T64_JSON_KEYS_MISSING;
    }
  }
  return T64_JSON_KEYS_OK;
}

int t64_json_u64_string(const t64_json_node *node, uint64_t *out) {
  uint64_t v = 0;
  size_t i;

  if (node == NULL || node->type != T64_JSON_STRING) return 0;
  if (node->len == 0 || node->len > 20) return 0;
  if (node->str[0] == '0' && node->len > 1) return 0;
  for (i = 0; i < node->len; i++) {
    uint64_t d;
    if (node->str[i] < '0' || node->str[i] > '9') return 0;
    d = (uint64_t)(node->str[i] - '0');
    if (v > (UINT64_MAX - d) / 10u) return 0;
    v = v * 10u + d;
  }
  *out = v;
  return 1;
}
