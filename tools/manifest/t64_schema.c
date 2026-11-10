/* Schema-subset validator; see t64_schema.h for the contract (authored here). */
#include "t64_schema.h"

#include <limits.h>
#include <regex.h>
#include <stdio.h>
#include <string.h>

#define SCHEMA_MAX_DEPTH 24u

/* ------------------------------------------------------------ pointers */

void t64_ptr_init(t64_ptr *p) {
  p->s[0] = '\0';
  p->len = 0;
}

static void ptr_put(t64_ptr *p, char c) {
  if (p->len + 1u < sizeof p->s) {
    p->s[p->len++] = c;
    p->s[p->len] = '\0';
  }
}

size_t t64_ptr_push_key(t64_ptr *p, const char *key, size_t key_len) {
  size_t old = p->len;
  size_t i;

  ptr_put(p, '/');
  for (i = 0; i < key_len; i++) {
    unsigned char c = (unsigned char)key[i];
    if (c == '~') {
      ptr_put(p, '~');
      ptr_put(p, '0');
    } else if (c == '/') {
      ptr_put(p, '~');
      ptr_put(p, '1');
    } else if (c < 0x20u || c == 0x7fu) {
      ptr_put(p, '?');
    } else {
      ptr_put(p, (char)c);
    }
  }
  return old;
}

size_t t64_ptr_push_index(t64_ptr *p, size_t index) {
  size_t old = p->len;
  char digits[32];
  size_t n = 0;
  size_t i;

  ptr_put(p, '/');
  do {
    digits[n++] = (char)('0' + (int)(index % 10u));
    index /= 10u;
  } while (index != 0u && n < sizeof digits);
  for (i = n; i > 0; i--) ptr_put(p, digits[i - 1u]);
  return old;
}

void t64_ptr_pop(t64_ptr *p, size_t old_len) {
  if (old_len <= p->len) {
    p->len = old_len;
    p->s[old_len] = '\0';
  }
}

void t64_sanitize(const char *s, size_t len, char *out, size_t cap) {
  size_t i;
  size_t n = 0;
  size_t keep = len;
  int cut = 0;

  if (cap < 4u) {
    if (cap > 0u) out[0] = '\0';
    return;
  }
  /* Fits when len bytes plus the NUL fit in cap. Otherwise keep cap-4 bytes,
   * so the bytes, "..." and the NUL end exactly at out[cap-1] (CR-01: the
   * old guard let the NUL land on out[cap]). */
  if (len > cap - 1u) {
    keep = cap - 4u;
    cut = 1;
  }
  for (i = 0; i < keep; i++) {
    unsigned char c = (unsigned char)s[i];
    out[n++] = (c < 0x20u || c == 0x7fu) ? '?' : (char)c;
  }
  if (cut) {
    out[n++] = '.';
    out[n++] = '.';
    out[n++] = '.';
  }
  out[n] = '\0';
}

/* ------------------------------------------------------------ helpers */

static int key_is(const t64_json_node *n, const char *lit) {
  size_t l = strlen(lit);
  return n->key != NULL && n->key_len == l && memcmp(n->key, lit, l) == 0;
}

static int is_scalar(const t64_json_node *n) {
  return n->type == T64_JSON_NULL || n->type == T64_JSON_BOOL ||
         n->type == T64_JSON_INT || n->type == T64_JSON_STRING;
}

static int scalar_equal(const t64_json_node *a, const t64_json_node *b) {
  if (a->type != b->type) return 0;
  switch (a->type) {
    case T64_JSON_NULL:
      return 1;
    case T64_JSON_BOOL:
    case T64_JSON_INT:
      return a->num == b->num;
    case T64_JSON_STRING:
      return a->len == b->len && memcmp(a->str, b->str, a->len) == 0;
    default:
      return 0;
  }
}

static int type_name_known(const t64_json_node *n) {
  static const char *const names[] = {"object", "array",   "string",
                                      "integer", "boolean", "null"};
  size_t i;
  if (n->type != T64_JSON_STRING) return 0;
  for (i = 0; i < sizeof names / sizeof names[0]; i++) {
    if (n->len == strlen(names[i]) && memcmp(n->str, names[i], n->len) == 0)
      return 1;
  }
  return 0;
}

static const char *inst_type_name(const t64_json_node *n) {
  switch (n->type) {
    case T64_JSON_NULL: return "null";
    case T64_JSON_BOOL: return "boolean";
    case T64_JSON_INT: return "integer";
    case T64_JSON_STRING: return "string";
    case T64_JSON_ARRAY: return "array";
    case T64_JSON_OBJECT: return "object";
  }
  return "unknown";
}

static int name_matches(const t64_json_node *name, const t64_json_node *inst) {
  const char *t = inst_type_name(inst);
  return name->len == strlen(t) && memcmp(name->str, t, name->len) == 0;
}

static size_t code_points(const t64_json_node *s) {
  size_t i;
  size_t n = 0;
  for (i = 0; i < s->len; i++) {
    if (((unsigned char)s->str[i] & 0xC0u) != 0x80u) n++;
  }
  return n;
}

/* ------------------------------------------------------------ schema check */

typedef struct {
  t64_ptr ptr;
  char why[200];
} chk_ctx;

static int chk_fail(chk_ctx *c, const char *msg) {
  (void)snprintf(c->why, sizeof c->why, "%s", msg);
  return 1;
}

static int chk_pattern(chk_ctx *c, const t64_json_node *n) {
  regex_t re;
  int rc;
  char err[96];

  if (n->type != T64_JSON_STRING) return chk_fail(c, "pattern must be a string");
  if (memchr(n->str, '\0', n->len) != NULL)
    return chk_fail(c, "pattern contains a NUL byte");
  if (n->len < 2u || n->str[0] != '^' || n->str[n->len - 1u] != '$')
    return chk_fail(c, "pattern must be anchored as ^...$");
  rc = regcomp(&re, n->str, REG_EXTENDED | REG_NOSUB);
  if (rc != 0) {
    char msg[160];
    (void)regerror(rc, &re, err, sizeof err);
    (void)snprintf(msg, sizeof msg, "pattern does not compile: %s", err);
    return chk_fail(c, msg);
  }
  regfree(&re);
  return 0;
}

static int chk_schema(chk_ctx *c, const t64_json_node *s, size_t depth);

static int chk_member(chk_ctx *c, const t64_json_node *m, size_t depth) {
  const t64_json_node *e;
  size_t old;
  size_t i;

  if (key_is(m, "type")) {
    if (m->type == T64_JSON_STRING) {
      if (!type_name_known(m)) return chk_fail(c, "unsupported type name");
    } else if (m->type == T64_JSON_ARRAY && m->count > 0u) {
      for (e = m->first; e != NULL; e = e->next) {
        if (!type_name_known(e)) return chk_fail(c, "unsupported type name");
      }
    } else {
      return chk_fail(c, "type must be a name or a non-empty array of names");
    }
  } else if (key_is(m, "required")) {
    if (m->type != T64_JSON_ARRAY) return chk_fail(c, "required must be an array");
    for (e = m->first; e != NULL; e = e->next) {
      if (e->type != T64_JSON_STRING)
        return chk_fail(c, "required must list strings");
    }
  } else if (key_is(m, "properties")) {
    if (m->type != T64_JSON_OBJECT)
      return chk_fail(c, "properties must be an object");
    for (e = m->first; e != NULL; e = e->next) {
      old = t64_ptr_push_key(&c->ptr, e->key, e->key_len);
      if (chk_schema(c, e, depth + 1u)) return 1;
      t64_ptr_pop(&c->ptr, old);
    }
  } else if (key_is(m, "additionalProperties")) {
    if (m->type != T64_JSON_BOOL || m->num != 0)
      return chk_fail(c, "additionalProperties is supported only as false");
  } else if (key_is(m, "enum")) {
    if (m->type != T64_JSON_ARRAY || m->count == 0u)
      return chk_fail(c, "enum must be a non-empty array");
    for (e = m->first; e != NULL; e = e->next) {
      if (!is_scalar(e)) return chk_fail(c, "enum members must be scalars");
    }
  } else if (key_is(m, "const")) {
    if (!is_scalar(m)) return chk_fail(c, "const must be a scalar");
  } else if (key_is(m, "items")) {
    if (m->type != T64_JSON_OBJECT)
      return chk_fail(c, "items must be one schema object");
    if (chk_schema(c, m, depth + 1u)) return 1;
  } else if (key_is(m, "oneOf")) {
    if (m->type != T64_JSON_ARRAY || m->count == 0u)
      return chk_fail(c, "oneOf must be a non-empty array");
    i = 0;
    for (e = m->first; e != NULL; e = e->next) {
      old = t64_ptr_push_index(&c->ptr, i++);
      if (chk_schema(c, e, depth + 1u)) return 1;
      t64_ptr_pop(&c->ptr, old);
    }
  } else if (key_is(m, "minItems") || key_is(m, "maxItems") ||
             key_is(m, "minLength") || key_is(m, "maxLength")) {
    if (m->type != T64_JSON_INT || m->num < 0)
      return chk_fail(c, "value must be a non-negative integer");
  } else if (key_is(m, "minimum") || key_is(m, "maximum")) {
    if (m->type != T64_JSON_INT) return chk_fail(c, "value must be an integer");
  } else if (key_is(m, "uniqueItems")) {
    if (m->type != T64_JSON_BOOL) return chk_fail(c, "uniqueItems must be a boolean");
  } else if (key_is(m, "pattern")) {
    return chk_pattern(c, m);
  } else if (key_is(m, "$schema") || key_is(m, "$id") || key_is(m, "title") ||
             key_is(m, "description")) {
    if (m->type != T64_JSON_STRING) return chk_fail(c, "annotation must be a string");
  } else {
    return chk_fail(c, "keyword is outside the supported subset");
  }
  return 0;
}

static int chk_schema(chk_ctx *c, const t64_json_node *s, size_t depth) {
  const t64_json_node *m;
  size_t old;

  if (depth > SCHEMA_MAX_DEPTH) return chk_fail(c, "schema nesting is too deep");
  if (s->type != T64_JSON_OBJECT) return chk_fail(c, "a schema must be an object");
  for (m = s->first; m != NULL; m = m->next) {
    old = t64_ptr_push_key(&c->ptr, m->key, m->key_len);
    if (chk_member(c, m, depth)) return 1;
    t64_ptr_pop(&c->ptr, old);
  }
  return 0;
}

int t64_schema_check(const t64_json_node *schema, char *where, size_t where_cap,
                     char *why, size_t why_cap) {
  chk_ctx c;

  t64_ptr_init(&c.ptr);
  c.why[0] = '\0';
  if (chk_schema(&c, schema, 0u) == 0) return 0;
  (void)snprintf(where, where_cap, "%s", c.ptr.s);
  (void)snprintf(why, why_cap, "%s", c.why);
  return 1;
}

/* ------------------------------------------------------------ validation */

typedef struct {
  t64_schema_emit_fn emit;
  void *ctx;
  long count;
  int fatal;
  char why[160];
  t64_ptr ptr;
} vctx;

static void report(vctx *vc, const char *rule, const char *msg) {
  vc->count++;
  if (vc->emit != NULL) vc->emit(vc->ctx, rule, vc->ptr.s, msg);
}

static void fatal(vctx *vc, const char *msg) {
  if (!vc->fatal) {
    vc->fatal = 1;
    (void)snprintf(vc->why, sizeof vc->why, "%s", msg);
  }
}

static void describe_value(const t64_json_node *n, char *out, size_t cap) {
  char tmp[64];
  if (n->type == T64_JSON_STRING) {
    t64_sanitize(n->str, n->len, tmp, sizeof tmp);
    (void)snprintf(out, cap, "\"%s\"", tmp);
  } else if (n->type == T64_JSON_INT) {
    (void)snprintf(out, cap, "%lld", (long long)n->num);
  } else {
    (void)snprintf(out, cap, "a %s", inst_type_name(n));
  }
}

static void validate(vctx *vc, const t64_json_node *sch, const t64_json_node *inst,
                     size_t depth);

static void check_type(vctx *vc, const t64_json_node *t, const t64_json_node *inst,
                       int *ok) {
  const t64_json_node *e;
  char msg[160];

  *ok = 0;
  if (t->type == T64_JSON_STRING) {
    *ok = name_matches(t, inst);
  } else {
    for (e = t->first; e != NULL; e = e->next) {
      if (name_matches(e, inst)) *ok = 1;
    }
  }
  if (!*ok) {
    char want[64];
    if (t->type == T64_JSON_STRING) {
      t64_sanitize(t->str, t->len, want, sizeof want);
    } else {
      size_t n = 0;
      want[0] = '\0';
      for (e = t->first; e != NULL; e = e->next) {
        char nm[24];
        t64_sanitize(e->str, e->len, nm, sizeof nm);
        n += (size_t)snprintf(want + n, sizeof want - n, "%s%s", n ? "|" : "", nm);
        if (n >= sizeof want) {
          n = sizeof want - 1u;
          break;
        }
      }
    }
    (void)snprintf(msg, sizeof msg, "expected %s, got %s", want,
                   inst_type_name(inst));
    report(vc, "M-SCHEMA-TYPE", msg);
  }
}

static void check_string(vctx *vc, const t64_json_node *sch,
                         const t64_json_node *inst) {
  const t64_json_node *k;
  char msg[200];
  size_t cp = code_points(inst);

  k = t64_json_get(sch, "minLength");
  if (k != NULL && (long long)cp < (long long)k->num) {
    (void)snprintf(msg, sizeof msg, "length %zu is below minLength %lld", cp,
                   (long long)k->num);
    report(vc, "M-SCHEMA-LENGTH", msg);
  }
  k = t64_json_get(sch, "maxLength");
  if (k != NULL && (long long)cp > (long long)k->num) {
    (void)snprintf(msg, sizeof msg, "length %zu is above maxLength %lld", cp,
                   (long long)k->num);
    report(vc, "M-SCHEMA-LENGTH", msg);
  }
  k = t64_json_get(sch, "pattern");
  if (k != NULL) {
    char pat[96];
    regex_t re;
    int rc;

    t64_sanitize(k->str, k->len, pat, sizeof pat);
    if (memchr(inst->str, '\0', inst->len) != NULL) {
      (void)snprintf(msg, sizeof msg, "value contains a NUL byte, pattern %s", pat);
      report(vc, "M-SCHEMA-PATTERN", msg);
      return;
    }
    rc = regcomp(&re, k->str, REG_EXTENDED | REG_NOSUB);
    if (rc != 0) {
      fatal(vc, "pattern failed to compile at validation time");
      return;
    }
    rc = regexec(&re, inst->str, 0, NULL, 0);
    regfree(&re);
    if (rc != 0) {
      (void)snprintf(msg, sizeof msg, "value does not match pattern %s", pat);
      report(vc, "M-SCHEMA-PATTERN", msg);
    }
  }
}

static void check_array(vctx *vc, const t64_json_node *sch,
                        const t64_json_node *inst, size_t depth) {
  const t64_json_node *k;
  const t64_json_node *a;
  const t64_json_node *b;
  char msg[160];
  size_t i;

  k = t64_json_get(sch, "minItems");
  if (k != NULL && (long long)inst->count < (long long)k->num) {
    (void)snprintf(msg, sizeof msg, "%zu items, minItems is %lld", inst->count,
                   (long long)k->num);
    report(vc, "M-SCHEMA-ITEMS", msg);
  }
  k = t64_json_get(sch, "maxItems");
  if (k != NULL && (long long)inst->count > (long long)k->num) {
    (void)snprintf(msg, sizeof msg, "%zu items, maxItems is %lld", inst->count,
                   (long long)k->num);
    report(vc, "M-SCHEMA-ITEMS", msg);
  }
  k = t64_json_get(sch, "uniqueItems");
  if (k != NULL && k->num != 0) {
    int dup = 0;
    for (a = inst->first; a != NULL && !dup; a = a->next) {
      if (!is_scalar(a)) {
        fatal(vc, "uniqueItems over non-scalar items is not supported");
        return;
      }
      for (b = a->next; b != NULL; b = b->next) {
        if (is_scalar(b) && scalar_equal(a, b)) {
          dup = 1;
          break;
        }
      }
    }
    if (dup) report(vc, "M-SCHEMA-ITEMS", "items are not unique");
  }
  k = t64_json_get(sch, "items");
  if (k != NULL) {
    i = 0;
    for (a = inst->first; a != NULL && !vc->fatal; a = a->next) {
      size_t old = t64_ptr_push_index(&vc->ptr, i++);
      validate(vc, k, a, depth + 1u);
      t64_ptr_pop(&vc->ptr, old);
    }
  }
}

static void check_object(vctx *vc, const t64_json_node *sch,
                         const t64_json_node *inst, size_t depth) {
  const t64_json_node *k;
  const t64_json_node *props = t64_json_get(sch, "properties");
  const t64_json_node *e;
  const t64_json_node *m;

  k = t64_json_get(sch, "required");
  if (k != NULL) {
    for (e = k->first; e != NULL; e = e->next) {
      if (t64_json_get(inst, e->str) == NULL || strlen(e->str) != e->len) {
        size_t old = t64_ptr_push_key(&vc->ptr, e->str, e->len);
        report(vc, "M-SCHEMA-REQUIRED", "missing required key");
        t64_ptr_pop(&vc->ptr, old);
      }
    }
  }
  if (props != NULL) {
    for (e = props->first; e != NULL && !vc->fatal; e = e->next) {
      const t64_json_node *child;
      size_t old;
      /* Look the member up by its exact key bytes, not through a C string. */
      for (child = inst->first; child != NULL; child = child->next) {
        if (child->key_len == e->key_len &&
            memcmp(child->key, e->key, e->key_len) == 0)
          break;
      }
      if (child == NULL) continue;
      old = t64_ptr_push_key(&vc->ptr, child->key, child->key_len);
      validate(vc, e, child, depth + 1u);
      t64_ptr_pop(&vc->ptr, old);
    }
  }
  k = t64_json_get(sch, "additionalProperties");
  if (k != NULL && k->num == 0) {
    for (m = inst->first; m != NULL; m = m->next) {
      int known = 0;
      if (props != NULL) {
        for (e = props->first; e != NULL; e = e->next) {
          if (e->key_len == m->key_len && memcmp(e->key, m->key, m->key_len) == 0) {
            known = 1;
            break;
          }
        }
      }
      if (!known) {
        size_t old = t64_ptr_push_key(&vc->ptr, m->key, m->key_len);
        report(vc, "M-SCHEMA-ADDITIONAL", "key is not allowed here");
        t64_ptr_pop(&vc->ptr, old);
      }
    }
  }
}

static void check_one_of(vctx *vc, const t64_json_node *branches,
                         const t64_json_node *inst, size_t depth) {
  const t64_json_node *b;
  const t64_json_node *best = NULL;
  long best_count = LONG_MAX;
  size_t matches = 0;
  char msg[160];

  for (b = branches->first; b != NULL; b = b->next) {
    vctx sub = *vc;
    sub.emit = NULL;
    sub.count = 0;
    validate(&sub, b, inst, depth + 1u);
    if (sub.fatal) {
      fatal(vc, sub.why);
      return;
    }
    if (sub.count == 0) {
      matches++;
    } else if (sub.count < best_count) {
      best_count = sub.count;
      best = b;
    }
  }
  if (matches == 1u) return;
  if (matches > 1u) {
    (void)snprintf(msg, sizeof msg, "matches %zu of %zu shapes; exactly one is required",
                   matches, branches->count);
    report(vc, "M-SCHEMA-ONEOF", msg);
    return;
  }
  if (best != NULL) validate(vc, best, inst, depth + 1u);
  (void)snprintf(msg, sizeof msg,
                 "matches none of %zu shapes; findings above are from the closest",
                 branches->count);
  report(vc, "M-SCHEMA-ONEOF", msg);
}

static void validate(vctx *vc, const t64_json_node *sch, const t64_json_node *inst,
                     size_t depth) {
  const t64_json_node *k;
  const t64_json_node *e;
  char msg[200];
  char val[80];
  int ok = 1;

  if (vc->fatal) return;
  if (depth > SCHEMA_MAX_DEPTH * 2u) {
    fatal(vc, "validation nesting is too deep");
    return;
  }
  k = t64_json_get(sch, "type");
  if (k != NULL) {
    check_type(vc, k, inst, &ok);
    if (!ok) return;
  }
  k = t64_json_get(sch, "enum");
  if (k != NULL) {
    int found = 0;
    for (e = k->first; e != NULL; e = e->next) {
      if (is_scalar(inst) && scalar_equal(inst, e)) found = 1;
    }
    if (!found) {
      describe_value(inst, val, sizeof val);
      (void)snprintf(msg, sizeof msg, "%s is not one of the allowed values", val);
      report(vc, "M-SCHEMA-ENUM", msg);
    }
  }
  k = t64_json_get(sch, "const");
  if (k != NULL && !(is_scalar(inst) && scalar_equal(inst, k))) {
    describe_value(inst, val, sizeof val);
    (void)snprintf(msg, sizeof msg, "%s is not the required constant", val);
    report(vc, "M-SCHEMA-ENUM", msg);
  }
  switch (inst->type) {
    case T64_JSON_STRING:
      check_string(vc, sch, inst);
      break;
    case T64_JSON_INT:
      k = t64_json_get(sch, "minimum");
      if (k != NULL && inst->num < k->num) {
        (void)snprintf(msg, sizeof msg, "%lld is below minimum %lld",
                       (long long)inst->num, (long long)k->num);
        report(vc, "M-SCHEMA-RANGE", msg);
      }
      k = t64_json_get(sch, "maximum");
      if (k != NULL && inst->num > k->num) {
        (void)snprintf(msg, sizeof msg, "%lld is above maximum %lld",
                       (long long)inst->num, (long long)k->num);
        report(vc, "M-SCHEMA-RANGE", msg);
      }
      break;
    case T64_JSON_ARRAY:
      check_array(vc, sch, inst, depth);
      break;
    case T64_JSON_OBJECT:
      check_object(vc, sch, inst, depth);
      break;
    default:
      break;
  }
  k = t64_json_get(sch, "oneOf");
  if (k != NULL && !vc->fatal) check_one_of(vc, k, inst, depth);
}

long t64_schema_validate(const t64_json_node *schema, const t64_json_node *inst,
                         t64_schema_emit_fn emit, void *ctx, char *why,
                         size_t why_cap) {
  vctx vc;

  vc.emit = emit;
  vc.ctx = ctx;
  vc.count = 0;
  vc.fatal = 0;
  vc.why[0] = '\0';
  t64_ptr_init(&vc.ptr);
  validate(&vc, schema, inst, 0u);
  if (vc.fatal) {
    (void)snprintf(why, why_cap, "%s", vc.why);
    return -1;
  }
  return vc.count;
}
