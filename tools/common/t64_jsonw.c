/* Bounded canonical JSON writer; see t64_jsonw.h for the format contract. */
#include "t64_jsonw.h"

#include <string.h>

#include "t64_json.h"
#include "t64_utf8.h"

#define T64_JSONW_MAX_INT 9007199254740991LL /* 2^53 - 1 */

static t64_jsonw_status fail(t64_jsonw *w, t64_jsonw_status s) {
  if (w->err == T64_JSONW_OK) w->err = s;
  return w->err;
}

/* Append one byte, always leaving room for the terminating NUL. */
static int put(t64_jsonw *w, char c) {
  if (w->cap == 0 || w->len + 1 >= w->cap) {
    (void)fail(w, T64_JSONW_E_OVERFLOW);
    return 0;
  }
  w->buf[w->len++] = c;
  w->buf[w->len] = '\0';
  return 1;
}

static int put_str(t64_jsonw *w, const char *s) {
  for (; *s != '\0'; s++) {
    if (!put(w, *s)) return 0;
  }
  return 1;
}

void t64_jsonw_init(t64_jsonw *w, char *buf, size_t cap) {
  memset(w, 0, sizeof *w);
  w->buf = buf;
  w->cap = cap;
  if (cap > 0) buf[0] = '\0';
}

/* Common prelude for every value: separators and state checks. */
static int begin_value(t64_jsonw *w) {
  int top = w->depth - 1;

  if (w->err != T64_JSONW_OK) return 0;
  if (w->depth == 0) {
    if (w->root_done) {
      (void)fail(w, T64_JSONW_E_STATE);
      return 0;
    }
    return 1;
  }
  if (w->is_object[top]) {
    if (!w->have_key[top]) {
      (void)fail(w, T64_JSONW_E_STATE);
      return 0;
    }
    w->have_key[top] = 0; /* comma was emitted with the key */
    return 1;
  }
  if (w->count[top] > 0 && !put(w, ',')) return 0;
  w->count[top]++;
  return 1;
}

static void end_value(t64_jsonw *w) {
  if (w->depth == 0) w->root_done = 1;
}

static int write_string(t64_jsonw *w, const char *s, size_t len) {
  static const char hexd[] = "0123456789abcdef";
  size_t i;

  if (!t64_utf8_valid((const uint8_t *)s, len, NULL)) {
    (void)fail(w, T64_JSONW_E_UTF8);
    return 0;
  }
  if (!put(w, '"')) return 0;
  for (i = 0; i < len; i++) {
    unsigned char c = (unsigned char)s[i];
    int ok;
    switch (c) {
      case '"': ok = put_str(w, "\\\""); break;
      case '\\': ok = put_str(w, "\\\\"); break;
      case '\b': ok = put_str(w, "\\b"); break;
      case '\t': ok = put_str(w, "\\t"); break;
      case '\n': ok = put_str(w, "\\n"); break;
      case '\f': ok = put_str(w, "\\f"); break;
      case '\r': ok = put_str(w, "\\r"); break;
      default:
        if (c < 0x20u) {
          ok = put_str(w, "\\u00") && put(w, hexd[c >> 4]) &&
               put(w, hexd[c & 0x0fu]);
        } else {
          ok = put(w, (char)c);
        }
        break;
    }
    if (!ok) return 0;
  }
  return put(w, '"');
}

static t64_jsonw_status begin_container(t64_jsonw *w, int is_object) {
  if (!begin_value(w)) return w->err;
  if (w->depth >= T64_JSONW_MAX_DEPTH) return fail(w, T64_JSONW_E_DEPTH);
  if (!put(w, is_object ? '{' : '[')) return w->err;
  w->is_object[w->depth] = is_object;
  w->have_key[w->depth] = 0;
  w->count[w->depth] = 0;
  w->prev_key_len[w->depth] = 0;
  w->depth++;
  return T64_JSONW_OK;
}

static t64_jsonw_status end_container(t64_jsonw *w, int is_object) {
  int top = w->depth - 1;

  if (w->err != T64_JSONW_OK) return w->err;
  if (w->depth == 0 || w->is_object[top] != is_object ||
      (is_object && w->have_key[top])) {
    return fail(w, T64_JSONW_E_STATE);
  }
  if (!put(w, is_object ? '}' : ']')) return w->err;
  w->depth--;
  end_value(w);
  return T64_JSONW_OK;
}

t64_jsonw_status t64_jsonw_begin_object(t64_jsonw *w) {
  return begin_container(w, 1);
}
t64_jsonw_status t64_jsonw_end_object(t64_jsonw *w) {
  return end_container(w, 1);
}
t64_jsonw_status t64_jsonw_begin_array(t64_jsonw *w) {
  return begin_container(w, 0);
}
t64_jsonw_status t64_jsonw_end_array(t64_jsonw *w) {
  return end_container(w, 0);
}

t64_jsonw_status t64_jsonw_key(t64_jsonw *w, const char *key, size_t len) {
  int top = w->depth - 1;

  if (w->err != T64_JSONW_OK) return w->err;
  if (w->depth == 0 || !w->is_object[top] || w->have_key[top]) {
    return fail(w, T64_JSONW_E_STATE);
  }
  if (len > T64_JSONW_MAX_KEY) return fail(w, T64_JSONW_E_KEY);
  if (w->count[top] > 0) {
    size_t plen = w->prev_key_len[top];
    size_t n = plen < len ? plen : len;
    int c = memcmp(w->prev_key[top], key, n);
    /* strictly ascending in unsigned byte order; a proper prefix sorts first */
    if (c > 0 || (c == 0 && plen >= len)) return fail(w, T64_JSONW_E_ORDER);
  }
  if (w->count[top] > 0 && !put(w, ',')) return w->err;
  if (!write_string(w, key, len)) return w->err;
  if (!put(w, ':')) return w->err;
  memcpy(w->prev_key[top], key, len);
  w->prev_key_len[top] = len;
  w->count[top]++;
  w->have_key[top] = 1;
  return T64_JSONW_OK;
}

t64_jsonw_status t64_jsonw_string(t64_jsonw *w, const char *s, size_t len) {
  if (!begin_value(w)) return w->err;
  if (!write_string(w, s, len)) return w->err;
  end_value(w);
  return T64_JSONW_OK;
}

t64_jsonw_status t64_jsonw_int(t64_jsonw *w, int64_t v) {
  char tmp[24];
  size_t n = 0;
  uint64_t mag;
  int neg = v < 0;

  if (!begin_value(w)) return w->err;
  if (v > T64_JSONW_MAX_INT || v < -T64_JSONW_MAX_INT) {
    return fail(w, T64_JSONW_E_RANGE);
  }
  mag = neg ? (uint64_t)(-v) : (uint64_t)v;
  do {
    tmp[n++] = (char)('0' + (int)(mag % 10u));
    mag /= 10u;
  } while (mag != 0);
  if (neg && !put(w, '-')) return w->err;
  while (n > 0) {
    if (!put(w, tmp[--n])) return w->err;
  }
  end_value(w);
  return T64_JSONW_OK;
}

t64_jsonw_status t64_jsonw_null(t64_jsonw *w) {
  if (!begin_value(w)) return w->err;
  if (!put_str(w, "null")) return w->err;
  end_value(w);
  return T64_JSONW_OK;
}

t64_jsonw_status t64_jsonw_bool(t64_jsonw *w, int v) {
  if (!begin_value(w)) return w->err;
  if (!put_str(w, v ? "true" : "false")) return w->err;
  end_value(w);
  return T64_JSONW_OK;
}

t64_jsonw_status t64_jsonw_finish(t64_jsonw *w, size_t *out_len) {
  if (w->err != T64_JSONW_OK) return w->err;
  if (w->depth != 0 || !w->root_done) return fail(w, T64_JSONW_E_STATE);
  if (!put(w, '\n')) return w->err;
  if (out_len != NULL) *out_len = w->len;
  return T64_JSONW_OK;
}

/* Unsigned-byte key order; a proper prefix sorts first (matches t64_jsonw_key). */
static int key_cmp(const t64_json_node *a, const t64_json_node *b) {
  size_t n = a->key_len < b->key_len ? a->key_len : b->key_len;
  int c = memcmp(a->key, b->key, n);
  if (c != 0) return c;
  return (a->key_len > b->key_len) - (a->key_len < b->key_len);
}

static t64_jsonw_status emit_members(t64_jsonw *w, const t64_json_node *obj) {
  const t64_json_node *m;
  const t64_json_node *prev = NULL;
  int sorted = 1;
  size_t i;

  for (m = obj->first; m != NULL && m->next != NULL; m = m->next) {
    if (key_cmp(m, m->next) >= 0) {
      sorted = 0;
      break;
    }
  }
  if (sorted) {
    for (m = obj->first; m != NULL; m = m->next) {
      if (t64_jsonw_key(w, m->key, m->key_len) != T64_JSONW_OK) return w->err;
      if (t64_jsonw_node(w, m) != T64_JSONW_OK) return w->err;
    }
    return w->err;
  }
  /* Unsorted tree: repeatedly take the smallest key above the previous one. */
  for (i = 0; i < obj->count; i++) {
    const t64_json_node *best = NULL;
    for (m = obj->first; m != NULL; m = m->next) {
      if (prev != NULL && key_cmp(m, prev) <= 0) continue;
      if (best == NULL || key_cmp(m, best) < 0) best = m;
    }
    if (best == NULL) return fail(w, T64_JSONW_E_ORDER); /* duplicate keys */
    if (t64_jsonw_key(w, best->key, best->key_len) != T64_JSONW_OK) return w->err;
    if (t64_jsonw_node(w, best) != T64_JSONW_OK) return w->err;
    prev = best;
  }
  return w->err;
}

t64_jsonw_status t64_jsonw_node(t64_jsonw *w, const t64_json_node *node) {
  const t64_json_node *c;

  if (w->err != T64_JSONW_OK) return w->err;
  if (node == NULL) return fail(w, T64_JSONW_E_STATE);
  switch (node->type) {
    case T64_JSON_NULL: return t64_jsonw_null(w);
    case T64_JSON_BOOL: return t64_jsonw_bool(w, node->num != 0);
    case T64_JSON_INT: return t64_jsonw_int(w, node->num);
    case T64_JSON_STRING: return t64_jsonw_string(w, node->str, node->len);
    case T64_JSON_ARRAY:
      if (t64_jsonw_begin_array(w) != T64_JSONW_OK) return w->err;
      for (c = node->first; c != NULL; c = c->next) {
        if (t64_jsonw_node(w, c) != T64_JSONW_OK) return w->err;
      }
      return t64_jsonw_end_array(w);
    case T64_JSON_OBJECT:
      if (t64_jsonw_begin_object(w) != T64_JSONW_OK) return w->err;
      if (emit_members(w, node) != T64_JSONW_OK) return w->err;
      return t64_jsonw_end_object(w);
  }
  return fail(w, T64_JSONW_E_STATE);
}
