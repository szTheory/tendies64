/*
 * t64-result-check: the consumer of t64.result v1 records and t64.suite
 * files (RUN-02, RUN-04). Normative contract: docs/spec/result-protocol.md.
 *
 *   t64-result-check result FILE
 *   t64-result-check suite --inventory INV FILE
 *   t64-result-check seal --inventory INV --out FILE RESULT...
 *   t64-result-check identity FILE
 *
 * Exit 0 ok, 1 invalid or harness-error, 2 usage or unreadable input. Options
 * always precede the file argument. Diagnostics are "RULE-ID path[:line]:
 * message" on stderr; a suite failure of kind S-TRUNCATED or S-INVENTORY also
 * prints "outcome=harness-error detail=suite-truncated|inventory-mismatch".
 *
 * Every record is parsed by the strict parser (t64_json_parse), checked
 * against exact key sets and types, and finally compared byte for byte with
 * its own canonical re-emission, so a record that validates is the canonical
 * record and nothing else.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_classify.h"
#include "t64_json.h"
#include "t64_jsonw.h"
#include "t64_sha256.h"

#define RECORD_MAX_BYTES 65536u              /* one result record or file */
#define SUITE_MAX_BYTES (16u * 1024u * 1024u) /* one suite file */
#define INVENTORY_MAX_BYTES (1024u * 1024u)
#define INVENTORY_MAX_LINES 65536u
#define ID_CAP 65u   /* fixture_id: 1..64 bytes plus NUL */
#define NAME_CAP 129u /* profile_id, build_id: 1..128 bytes plus NUL */
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

enum { EXIT_OK = 0, EXIT_INVALID = 1, EXIT_USAGE = 2 };

static const char *const result_keys[] = {
    "build_id",       "counts",       "detail",
    "evidence_tier",  "fixture_digest", "fixture_id",
    "guest_tick",     "log_digest",   "outcome",
    "profile_hash",   "profile_id",   "schema",
    "schema_version", "state_digest", "stop_reason"};

static const char *const count_keys[] = {
    "fail_reported", "log_bytes",     "log_bytes_total",
    "log_lines",     "pass_reported", "unhandled_exception_lines"};

static const char *const suite_keys[] = {"case_count", "inventory_sha256",
                                         "schema", "schema_version"};

static const char *const tier_names[] = {"smoke",
                                         "regression-pinned",
                                         "unit-tested",
                                         "self-checking-fixture",
                                         "emulator-differential",
                                         "hardware-derived",
                                         "synthetic"};

static const char *const stop_names[] = {"guest-bound", "guest-halt", "none"};

/* ---- diagnostics --------------------------------------------------------- */

typedef struct {
  const char *rule;
  char msg[240];
  size_t line;  /* 1-based, 0 when the failure is not about one line */
  int detail;   /* a t64_detail to echo as harness-error, or -1 */
} diag;

#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 3, 4)))
#endif
static void set_diag(diag *d, const char *rule, const char *fmt, ...) {
  va_list ap;
  d->rule = rule;
  d->detail = -1;
  d->line = 0;
  va_start(ap, fmt);
  (void)vsnprintf(d->msg, sizeof d->msg, fmt, ap);
  va_end(ap);
}

/* Copies s for display: printable ASCII only, at most cap-1 bytes. */
static void show(const char *s, size_t len, char *out, size_t cap) {
  size_t i;
  size_t n = 0;
  for (i = 0; i < len && n + 1 < cap; i++) {
    unsigned char c = (unsigned char)s[i];
    out[n++] = (c >= 0x20 && c < 0x7f) ? (char)c : '?';
  }
  out[n] = '\0';
}

static void print_fail(const char *path, const diag *d) {
  if (d->line > 0) {
    fprintf(stderr, "%s %s:%lu: %s\n", d->rule, path, (unsigned long)d->line,
            d->msg);
  } else {
    fprintf(stderr, "%s %s: %s\n", d->rule, path, d->msg);
  }
  if (d->detail >= 0) {
    fprintf(stderr, "outcome=%s detail=%s\n",
            t64_outcome_name(T64_OUT_HARNESS_ERROR),
            t64_detail_name((t64_detail)d->detail));
  }
}

/* ---- file input ---------------------------------------------------------- */

/* Reads at most max+1 bytes so an oversized file reports as size, not memory. */
static int read_file(const char *path, size_t max, uint8_t **out,
                     size_t *outlen) {
  FILE *f = fopen(path, "rb");
  uint8_t *buf;
  size_t cap = max + 1;
  size_t n = 0;

  if (f == NULL) {
    return 0;
  }
  buf = (uint8_t *)malloc(cap);
  if (buf == NULL) {
    (void)fclose(f);
    return 0;
  }
  while (n < cap) {
    size_t got = fread(buf + n, 1, cap - n, f);
    if (got == 0) {
      break;
    }
    n += got;
  }
  if (ferror(f)) {
    free(buf);
    (void)fclose(f);
    return 0;
  }
  (void)fclose(f);
  *out = buf;
  *outlen = n;
  return 1;
}

/* ---- small validators ---------------------------------------------------- */

static int str_is(const t64_json_node *n, const char *lit) {
  size_t l = strlen(lit);
  return n != NULL && n->type == T64_JSON_STRING && n->len == l &&
         memcmp(n->str, lit, l) == 0;
}

static int is_hex64(const t64_json_node *n) {
  size_t i;
  if (n == NULL || n->type != T64_JSON_STRING || n->len != 64) {
    return 0;
  }
  for (i = 0; i < 64; i++) {
    char c = n->str[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
      return 0;
    }
  }
  return 1;
}

/* [A-Za-z0-9._-] (plus '+' when allow_plus), 1..max bytes. */
static int name_ok(const t64_json_node *n, size_t max, int allow_plus) {
  size_t i;
  if (n == NULL || n->type != T64_JSON_STRING || n->len == 0 || n->len > max) {
    return 0;
  }
  for (i = 0; i < n->len; i++) {
    char c = n->str[i];
    int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
             (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-' ||
             (allow_plus && c == '+');
    if (!ok) {
      return 0;
    }
  }
  return 1;
}

static int in_list(const t64_json_node *n, const char *const *list,
                   size_t count) {
  size_t i;
  for (i = 0; i < count; i++) {
    if (str_is(n, list[i])) {
      return 1;
    }
  }
  return 0;
}

/* Exact key set (every key required, none unknown). 1 when it matches. */
static int keys_ok(const t64_json_node *obj, const char *const *keys, size_t n,
                   const char *what, diag *d) {
  const char *bad = NULL;
  char shown[64];

  switch (t64_json_check_keys(obj, keys, n, keys, n, &bad)) {
    case T64_JSON_KEYS_OK:
      return 1;
    case T64_JSON_KEYS_NOT_OBJECT:
      set_diag(d, "R-TYPE", "%s is not an object", what);
      return 0;
    case T64_JSON_KEYS_UNKNOWN:
      show(bad != NULL ? bad : "", bad != NULL ? strlen(bad) : 0, shown,
           sizeof shown);
      set_diag(d, "R-UNKNOWN-FIELD", "unknown field in %s: %s", what, shown);
      return 0;
    case T64_JSON_KEYS_MISSING:
      show(bad != NULL ? bad : "", bad != NULL ? strlen(bad) : 0, shown,
           sizeof shown);
      set_diag(d, "R-MISSING-FIELD", "missing field in %s: %s", what, shown);
      return 0;
  }
  set_diag(d, "R-TYPE", "%s: key check failed", what);
  return 0;
}

static int schema_and_version(const t64_json_node *root, const char *schema,
                              diag *d) {
  const t64_json_node *s;
  const t64_json_node *v;

  if (root->type != T64_JSON_OBJECT) {
    set_diag(d, "R-TYPE", "record is not an object");
    return 0;
  }
  s = t64_json_get(root, "schema");
  if (s != NULL && !str_is(s, schema)) {
    set_diag(d, "R-SCHEMA", "schema is not %s", schema);
    return 0;
  }
  v = t64_json_get(root, "schema_version");
  if (v != NULL && !(v->type == T64_JSON_INT && v->num == 1)) {
    set_diag(d, "R-VERSION", "schema_version is not the integer 1");
    return 0;
  }
  return 1;
}

/* ---- result record ------------------------------------------------------- */

typedef struct {
  char fixture_id[ID_CAP];
  char profile_id[NAME_CAP];
  char build_id[NAME_CAP];
  char tier[32];
  char outcome[32];
  size_t line;
} rec;

typedef struct {
  uint64_t case_count;
  char hash[65];
} suite_rec;

static void copy_str(char *dst, size_t cap, const t64_json_node *n) {
  size_t l = n->len < cap - 1 ? n->len : cap - 1;
  memcpy(dst, n->str, l);
  dst[l] = '\0';
}

static int lookup_detail(const t64_json_node *n, t64_detail *out) {
  int i;
  for (i = 0; i < (int)T64_D_COUNT_; i++) {
    if (str_is(n, t64_detail_name((t64_detail)i))) {
      *out = (t64_detail)i;
      return 1;
    }
  }
  return 0;
}

static int lookup_outcome(const t64_json_node *n, t64_outcome *out) {
  int i;
  for (i = 0; i <= (int)T64_OUT_TIMEOUT; i++) {
    if (str_is(n, t64_outcome_name((t64_outcome)i))) {
      *out = (t64_outcome)i;
      return 1;
    }
  }
  return 0;
}

static int validate_result(const t64_json_node *root, rec *r, diag *d) {
  const t64_json_node *n_counts;
  const t64_json_node *n;
  uint64_t counts[ARRAY_LEN(count_keys)];
  uint64_t tick;
  t64_detail detail = T64_D_USAGE;
  t64_outcome outcome = T64_OUT_PASS;
  int guest_outcome;
  int stamp_tier;
  int stamp_profile;
  int stamp_build;
  size_t i;

  if (!schema_and_version(root, "t64.result", d)) {
    return 0;
  }
  if (!keys_ok(root, result_keys, ARRAY_LEN(result_keys), "record", d)) {
    return 0;
  }

  /* R-TYPE: shape of every field. */
  if (!name_ok(t64_json_get(root, "fixture_id"), 64, 0)) {
    set_diag(d, "R-TYPE", "fixture_id must match [A-Za-z0-9._-]{1,64}");
    return 0;
  }
  if (!name_ok(t64_json_get(root, "profile_id"), 128, 1)) {
    set_diag(d, "R-TYPE", "profile_id must match [A-Za-z0-9._+-]{1,128}");
    return 0;
  }
  if (!name_ok(t64_json_get(root, "build_id"), 128, 1)) {
    set_diag(d, "R-TYPE", "build_id must match [A-Za-z0-9._+-]{1,128}");
    return 0;
  }
  if (!is_hex64(t64_json_get(root, "profile_hash"))) {
    set_diag(d, "R-TYPE", "profile_hash must be 64 lowercase hex characters");
    return 0;
  }
  n = t64_json_get(root, "fixture_digest");
  if (n->type != T64_JSON_NULL && !is_hex64(n)) {
    set_diag(d, "R-TYPE", "fixture_digest must be null or 64 lowercase hex");
    return 0;
  }
  n = t64_json_get(root, "log_digest");
  if (n->type != T64_JSON_NULL && !is_hex64(n)) {
    set_diag(d, "R-TYPE", "log_digest must be null or 64 lowercase hex");
    return 0;
  }
  if (!t64_json_u64_string(t64_json_get(root, "guest_tick"), &tick)) {
    set_diag(d, "R-TYPE", "guest_tick must be a decimal u64 string");
    return 0;
  }
  n_counts = t64_json_get(root, "counts");
  if (n_counts->type != T64_JSON_OBJECT) {
    set_diag(d, "R-TYPE", "counts is not an object");
    return 0;
  }
  if (!keys_ok(n_counts, count_keys, ARRAY_LEN(count_keys), "counts", d)) {
    return 0;
  }
  for (i = 0; i < ARRAY_LEN(count_keys); i++) {
    n = t64_json_get(n_counts, count_keys[i]);
    if (n->type != T64_JSON_INT || n->num < 0) {
      set_diag(d, "R-TYPE", "counts.%s must be a non-negative integer",
               count_keys[i]);
      return 0;
    }
    counts[i] = (uint64_t)n->num;
  }
  {
    static const char *const strings[] = {"detail", "evidence_tier", "outcome",
                                          "stop_reason"};
    for (i = 0; i < ARRAY_LEN(strings); i++) {
      if (t64_json_get(root, strings[i])->type != T64_JSON_STRING) {
        set_diag(d, "R-TYPE", "%s must be a string", strings[i]);
        return 0;
      }
    }
  }

  /* R-ENUM: closed vocabularies. */
  if (!lookup_outcome(t64_json_get(root, "outcome"), &outcome)) {
    set_diag(d, "R-ENUM", "outcome is not one of the six outcomes");
    return 0;
  }
  if (!lookup_detail(t64_json_get(root, "detail"), &detail)) {
    set_diag(d, "R-ENUM", "detail is not in the closed detail enum");
    return 0;
  }
  if (!in_list(t64_json_get(root, "evidence_tier"), tier_names,
               ARRAY_LEN(tier_names))) {
    set_diag(d, "R-ENUM", "evidence_tier is not in the tier enum");
    return 0;
  }
  if (!in_list(t64_json_get(root, "stop_reason"), stop_names,
               ARRAY_LEN(stop_names))) {
    set_diag(d, "R-ENUM", "stop_reason is not guest-bound, guest-halt or none");
    return 0;
  }

  /* R-DETAIL-OUTCOME */
  if (t64_detail_outcome(detail) != outcome) {
    set_diag(d, "R-DETAIL-OUTCOME", "detail %s does not belong to outcome %s",
             t64_detail_name(detail), t64_outcome_name(outcome));
    return 0;
  }

  /* R-SYNTHETIC-STAMP: all three markers present or all absent. */
  stamp_tier = str_is(t64_json_get(root, "evidence_tier"), "synthetic");
  stamp_profile = str_is(t64_json_get(root, "profile_id"), "synthetic-stub");
  n = t64_json_get(root, "build_id");
  stamp_build = n->len >= 10 && memcmp(n->str + n->len - 10, "+synthetic", 10) == 0;
  if (stamp_tier != stamp_profile || stamp_profile != stamp_build) {
    set_diag(d, "R-SYNTHETIC-STAMP",
             "evidence_tier synthetic, profile_id synthetic-stub and a "
             "+synthetic build_id must be all present or all absent "
             "(tier=%d profile=%d build=%d)",
             stamp_tier, stamp_profile, stamp_build);
    return 0;
  }

  /* R-NULL-RULES */
  if (t64_json_get(root, "state_digest")->type != T64_JSON_NULL) {
    set_diag(d, "R-NULL-RULES", "state_digest must be null in phase 1");
    return 0;
  }
  guest_outcome = outcome != T64_OUT_HARNESS_ERROR &&
                  outcome != T64_OUT_MISSING_FIXTURE;
  if (!guest_outcome) {
    int any_count = 0;
    for (i = 0; i < ARRAY_LEN(counts); i++) {
      any_count |= counts[i] != 0;
    }
    if (t64_json_get(root, "log_digest")->type != T64_JSON_NULL) {
      set_diag(d, "R-NULL-RULES", "%s requires log_digest null",
               t64_outcome_name(outcome));
      return 0;
    }
    if (any_count) {
      set_diag(d, "R-NULL-RULES", "%s requires every count 0",
               t64_outcome_name(outcome));
      return 0;
    }
    if (!str_is(t64_json_get(root, "stop_reason"), "none") || tick != 0) {
      set_diag(d, "R-NULL-RULES",
               "%s requires stop_reason none and guest_tick \"0\"",
               t64_outcome_name(outcome));
      return 0;
    }
    if (outcome == T64_OUT_MISSING_FIXTURE &&
        t64_json_get(root, "fixture_digest")->type != T64_JSON_NULL) {
      set_diag(d, "R-NULL-RULES", "missing-fixture requires fixture_digest null");
      return 0;
    }
  } else {
    /* counts order: fail_reported, log_bytes, log_bytes_total, ... */
    if (t64_json_get(root, "log_digest")->type == T64_JSON_NULL ||
        t64_json_get(root, "fixture_digest")->type == T64_JSON_NULL) {
      set_diag(d, "R-NULL-RULES",
               "%s requires fixture_digest and log_digest to be present",
               t64_outcome_name(outcome));
      return 0;
    }
    if (str_is(t64_json_get(root, "stop_reason"), "none")) {
      set_diag(d, "R-NULL-RULES", "%s requires stop_reason guest-bound or guest-halt",
               t64_outcome_name(outcome));
      return 0;
    }
    if (counts[1] > counts[2]) {
      set_diag(d, "R-NULL-RULES", "counts.log_bytes exceeds counts.log_bytes_total");
      return 0;
    }
  }

  copy_str(r->fixture_id, sizeof r->fixture_id, t64_json_get(root, "fixture_id"));
  copy_str(r->profile_id, sizeof r->profile_id, t64_json_get(root, "profile_id"));
  copy_str(r->build_id, sizeof r->build_id, t64_json_get(root, "build_id"));
  copy_str(r->tier, sizeof r->tier, t64_json_get(root, "evidence_tier"));
  copy_str(r->outcome, sizeof r->outcome, t64_json_get(root, "outcome"));
  return 1;
}

static int validate_suite_record(const t64_json_node *root, suite_rec *s,
                                 diag *d) {
  const t64_json_node *n;

  if (!schema_and_version(root, "t64.suite", d)) {
    return 0;
  }
  if (!keys_ok(root, suite_keys, ARRAY_LEN(suite_keys), "suite record", d)) {
    return 0;
  }
  n = t64_json_get(root, "case_count");
  if (n->type != T64_JSON_INT || n->num < 0) {
    set_diag(d, "R-TYPE", "case_count must be a non-negative integer");
    return 0;
  }
  s->case_count = (uint64_t)n->num;
  n = t64_json_get(root, "inventory_sha256");
  if (!is_hex64(n)) {
    set_diag(d, "R-TYPE", "inventory_sha256 must be 64 lowercase hex characters");
    return 0;
  }
  memcpy(s->hash, n->str, 64);
  s->hash[64] = '\0';
  return 1;
}

/* ---- one line (or one file) ---------------------------------------------- */

enum { KIND_NONE = 0, KIND_RESULT = 1, KIND_SUITE = 2 };

/*
 * Parses and validates one record, `line` including its LF when it has one.
 * Returns KIND_RESULT or KIND_SUITE, or KIND_NONE with *d filled. A suite
 * record is refused (R-SCHEMA) unless allow_suite.
 */
static int check_line(const uint8_t *line, size_t len, int allow_suite,
                      rec *r, suite_rec *s, diag *d) {
  t64_json_limits lim;
  t64_json_doc doc;
  t64_json_error err;
  t64_jsonw w;
  char *canon;
  size_t canon_len = 0;
  int kind;
  int bad;

  t64_json_limits_default(&lim);
  lim.max_input = RECORD_MAX_BYTES;
  if (t64_json_parse(line, len, &lim, &doc, &err) != 0) {
    set_diag(d, "R-JSON", "JSON-%s offset=%lu",
             t64_json_category_name(err.category), (unsigned long)err.offset);
    return KIND_NONE;
  }
  if (doc.root->type == T64_JSON_OBJECT &&
      str_is(t64_json_get(doc.root, "schema"), "t64.suite")) {
    if (!allow_suite) {
      set_diag(d, "R-SCHEMA", "a t64.suite record is not a t64.result");
      t64_json_free(&doc);
      return KIND_NONE;
    }
    kind = validate_suite_record(doc.root, s, d) ? KIND_SUITE : KIND_NONE;
  } else {
    kind = validate_result(doc.root, r, d) ? KIND_RESULT : KIND_NONE;
  }
  if (kind == KIND_NONE) {
    t64_json_free(&doc);
    return KIND_NONE;
  }

  /* R-NOT-CANONICAL: the bytes must be exactly the canonical emission. */
  canon = (char *)malloc(len + 16);
  if (canon == NULL) {
    set_diag(d, "R-NOT-CANONICAL", "out of memory while canonicalizing");
    t64_json_free(&doc);
    return KIND_NONE;
  }
  t64_jsonw_init(&w, canon, len + 16);
  bad = t64_jsonw_node(&w, doc.root) != T64_JSONW_OK ||
        t64_jsonw_finish(&w, &canon_len) != T64_JSONW_OK ||
        canon_len != len || memcmp(canon, line, len) != 0;
  free(canon);
  t64_json_free(&doc);
  if (bad) {
    set_diag(d, "R-NOT-CANONICAL",
             "bytes differ from the canonical form (or the final LF is missing)");
    return KIND_NONE;
  }
  return kind;
}

/* ---- inventory ----------------------------------------------------------- */

typedef struct {
  char (*ids)[ID_CAP];
  size_t count;
  char hash[65];
} inventory;

static void inventory_free(inventory *inv) {
  free(inv->ids);
  inv->ids = NULL;
  inv->count = 0;
}

/* Loads INV: sorted (unsigned byte order), strictly ascending fixture_ids, LF-terminated. */
static int inventory_load(const char *path, inventory *inv) {
  uint8_t *buf = NULL;
  size_t len = 0;
  size_t pos = 0;
  size_t cap = 0;
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  t64_sha256_ctx ctx;

  inv->ids = NULL;
  inv->count = 0;
  if (!read_file(path, INVENTORY_MAX_BYTES, &buf, &len)) {
    fprintf(stderr, "ERROR cannot read inventory %s\n", path);
    return 0;
  }
  if (len > INVENTORY_MAX_BYTES || len == 0 || buf[len - 1] != '\n') {
    fprintf(stderr, "ERROR inventory %s must be non-empty, LF-terminated and at most 1 MiB\n", path);
    free(buf);
    return 0;
  }
  while (pos < len) {
    size_t e = pos;
    size_t k;
    while (buf[e] != '\n') {
      e++;
    }
    if (e == pos || e - pos > ID_CAP - 1 || inv->count >= INVENTORY_MAX_LINES) {
      fprintf(stderr, "ERROR inventory %s line %lu is empty, too long or too many lines\n",
              path, (unsigned long)(inv->count + 1));
      free(buf);
      inventory_free(inv);
      return 0;
    }
    for (k = pos; k < e; k++) {
      unsigned char c = buf[k];
      int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
               (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
      if (!ok) {
        fprintf(stderr, "ERROR inventory %s line %lu has a character outside [A-Za-z0-9._-]\n",
                path, (unsigned long)(inv->count + 1));
        free(buf);
        inventory_free(inv);
        return 0;
      }
    }
    if (inv->count == cap) {
      size_t ncap = cap == 0 ? 64 : cap * 2;
      void *p = realloc(inv->ids, ncap * sizeof inv->ids[0]);
      if (p == NULL) {
        fprintf(stderr, "ERROR out of memory\n");
        free(buf);
        inventory_free(inv);
        return 0;
      }
      inv->ids = (char(*)[ID_CAP])p;
      cap = ncap;
    }
    memcpy(inv->ids[inv->count], buf + pos, e - pos);
    inv->ids[inv->count][e - pos] = '\0';
    if (inv->count > 0 && strcmp(inv->ids[inv->count - 1], inv->ids[inv->count]) >= 0) {
      fprintf(stderr, "ERROR inventory %s line %lu is not strictly ascending (LC_ALL=C order, no duplicates)\n",
              path, (unsigned long)(inv->count + 1));
      free(buf);
      inventory_free(inv);
      return 0;
    }
    inv->count++;
    pos = e + 1;
  }
  t64_sha256_init(&ctx);
  t64_sha256_update(&ctx, buf, len);
  t64_sha256_final(&ctx, digest);
  t64_sha256_hex(digest, inv->hash);
  free(buf);
  return 1;
}

/* ---- suite and identity scan --------------------------------------------- */

typedef struct {
  rec *v;
  size_t n;
  size_t cap;
} recvec;

static int recvec_push(recvec *rv, const rec *r) {
  if (rv->n == rv->cap) {
    size_t ncap = rv->cap == 0 ? 64 : rv->cap * 2;
    void *p = realloc(rv->v, ncap * sizeof rv->v[0]);
    if (p == NULL) {
      return 0;
    }
    rv->v = (rec *)p;
    rv->cap = ncap;
  }
  rv->v[rv->n++] = *r;
  return 1;
}

static int cmp_rec_ptr(const void *a, const void *b) {
  const rec *x = *(const rec *const *)a;
  const rec *y = *(const rec *const *)b;
  int c = strcmp(x->fixture_id, y->fixture_id);
  if (c != 0) {
    return c;
  }
  return x->line < y->line ? -1 : (x->line > y->line ? 1 : 0);
}

/*
 * Scans a JSON Lines buffer. With inv != NULL it is the strict suite check
 * (suite record last, inventory comparison). With inv == NULL (identity) the
 * suite record is optional but, when present, must be last and well formed.
 * Result records are appended to *out. Returns 1 when valid; otherwise fills
 * *d and returns 0.
 */
static int scan(const uint8_t *buf, size_t len, const inventory *inv,
                recvec *out, diag *d) {
  size_t pos = 0;
  size_t line_no = 0;
  int have_suite = 0;
  suite_rec suite;
  size_t i;
  rec **idx;

  memset(&suite, 0, sizeof suite);
  if (inv != NULL && len == 0) {
    set_diag(d, "S-TRUNCATED", "empty file: no suite record");
    d->detail = (int)T64_D_SUITE_TRUNCATED;
    return 0;
  }
  while (pos < len) {
    const uint8_t *nl = (const uint8_t *)memchr(buf + pos, '\n', len - pos);
    size_t end = nl != NULL ? (size_t)(nl - buf) + 1 : len;
    int is_last = end == len;
    rec r;
    int kind;

    line_no++;
    if (inv != NULL && nl == NULL) {
      set_diag(d, "S-TRUNCATED", "final line is not LF-terminated");
      d->line = line_no;
      d->detail = (int)T64_D_SUITE_TRUNCATED;
      return 0;
    }
    memset(&r, 0, sizeof r);
    kind = check_line(buf + pos, end - pos, 1, &r, &suite, d);
    if (kind == KIND_NONE) {
      d->line = line_no;
      return 0;
    }
    if (kind == KIND_SUITE) {
      if (!is_last) {
        set_diag(d, "S-ORDER", "the suite record must be the last line");
        d->line = line_no;
        return 0;
      }
      have_suite = 1;
    } else {
      if (is_last && inv != NULL) {
        set_diag(d, "S-TRUNCATED", "last line is a result: the suite record is missing");
        d->line = line_no;
        d->detail = (int)T64_D_SUITE_TRUNCATED;
        return 0;
      }
      r.line = line_no;
      if (!recvec_push(out, &r)) {
        set_diag(d, "R-JSON", "out of memory");
        return 0;
      }
    }
    pos = end;
  }
  if (out->n == 0) {
    set_diag(d, "S-EMPTY", "the file holds no result records (zero results is never a pass)");
    return 0;
  }
  if (inv == NULL) {
    return 1;
  }
  (void)have_suite; /* inv != NULL implies the last line was a suite record */

  /* S-DUPLICATE, then S-ORDER, over fixture_id. */
  idx = (rec **)malloc(out->n * sizeof idx[0]);
  if (idx == NULL) {
    set_diag(d, "S-INVENTORY", "out of memory");
    return 0;
  }
  for (i = 0; i < out->n; i++) {
    idx[i] = &out->v[i];
  }
  qsort(idx, out->n, sizeof idx[0], cmp_rec_ptr);
  for (i = 1; i < out->n; i++) {
    if (strcmp(idx[i - 1]->fixture_id, idx[i]->fixture_id) == 0) {
      set_diag(d, "S-DUPLICATE", "fixture_id %s appears on lines %lu and %lu",
               idx[i]->fixture_id, (unsigned long)idx[i - 1]->line,
               (unsigned long)idx[i]->line);
      free(idx);
      return 0;
    }
  }
  for (i = 1; i < out->n; i++) {
    if (strcmp(out->v[i - 1].fixture_id, out->v[i].fixture_id) > 0) {
      set_diag(d, "S-ORDER", "fixture_id %s on line %lu is out of ascending order",
               out->v[i].fixture_id, (unsigned long)out->v[i].line);
      free(idx);
      return 0;
    }
  }

  /* S-INVENTORY: count, hash, then the id set. */
  if (suite.case_count != (uint64_t)out->n) {
    set_diag(d, "S-INVENTORY", "suite record case_count %lu but the file holds %lu results",
             (unsigned long)suite.case_count, (unsigned long)out->n);
    d->detail = (int)T64_D_INVENTORY_MISMATCH;
    free(idx);
    return 0;
  }
  if (strcmp(suite.hash, inv->hash) != 0) {
    set_diag(d, "S-INVENTORY", "inventory_sha256 does not match the pinned INVENTORY");
    d->detail = (int)T64_D_INVENTORY_MISMATCH;
    free(idx);
    return 0;
  }
  if ((size_t)out->n != inv->count) {
    set_diag(d, "S-INVENTORY", "%lu results but the pinned inventory has %lu cases",
             (unsigned long)out->n, (unsigned long)inv->count);
    d->detail = (int)T64_D_INVENTORY_MISMATCH;
    free(idx);
    return 0;
  }
  for (i = 0; i < out->n; i++) {
    if (strcmp(idx[i]->fixture_id, inv->ids[i]) != 0) {
      set_diag(d, "S-INVENTORY", "case set differs from the inventory at position %lu (suite %s, inventory %s)",
               (unsigned long)(i + 1), idx[i]->fixture_id, inv->ids[i]);
      d->detail = (int)T64_D_INVENTORY_MISMATCH;
      free(idx);
      return 0;
    }
  }
  free(idx);
  return 1;
}

/* ---- subcommands --------------------------------------------------------- */

static int load_input(const char *path, size_t max, uint8_t **buf, size_t *len) {
  if (!read_file(path, max, buf, len)) {
    fprintf(stderr, "ERROR cannot read %s\n", path);
    return 0;
  }
  return 1;
}

static int cmd_result(const char *path) {
  uint8_t *buf;
  size_t len;
  rec r;
  diag d;
  int kind;

  if (!load_input(path, RECORD_MAX_BYTES, &buf, &len)) {
    return EXIT_USAGE;
  }
  memset(&r, 0, sizeof r);
  kind = check_line(buf, len, 0, &r, NULL, &d);
  free(buf);
  if (kind == KIND_NONE) {
    print_fail(path, &d);
    return EXIT_INVALID;
  }
  printf("ok %s\n", path);
  return EXIT_OK;
}

static int cmd_suite(const char *inv_path, const char *path) {
  inventory inv;
  uint8_t *buf;
  size_t len;
  recvec rv = {NULL, 0, 0};
  diag d;
  int ok;

  if (!inventory_load(inv_path, &inv)) {
    return EXIT_USAGE;
  }
  if (!load_input(path, SUITE_MAX_BYTES, &buf, &len)) {
    inventory_free(&inv);
    return EXIT_USAGE;
  }
  if (len > SUITE_MAX_BYTES) {
    set_diag(&d, "R-JSON", "JSON-size: the file exceeds 16 MiB");
    print_fail(path, &d);
    free(buf);
    inventory_free(&inv);
    return EXIT_INVALID;
  }
  ok = scan(buf, len, &inv, &rv, &d);
  free(buf);
  free(rv.v);
  inventory_free(&inv);
  if (!ok) {
    print_fail(path, &d);
    return EXIT_INVALID;
  }
  printf("ok %s\n", path);
  return EXIT_OK;
}

static int cmd_identity(const char *path) {
  uint8_t *buf;
  size_t len;
  recvec rv = {NULL, 0, 0};
  diag d;
  size_t i;
  int ok;

  if (!load_input(path, SUITE_MAX_BYTES, &buf, &len)) {
    return EXIT_USAGE;
  }
  if (len > SUITE_MAX_BYTES) {
    set_diag(&d, "R-JSON", "JSON-size: the file exceeds 16 MiB");
    print_fail(path, &d);
    free(buf);
    return EXIT_INVALID;
  }
  ok = scan(buf, len, NULL, &rv, &d);
  free(buf);
  if (!ok) {
    print_fail(path, &d);
    free(rv.v);
    return EXIT_INVALID;
  }
  /* Nothing is printed unless every record validated. */
  for (i = 0; i < rv.n; i++) {
    printf("%s\t%s\t%s\t%s\t%s\n", rv.v[i].fixture_id, rv.v[i].profile_id,
           rv.v[i].build_id, rv.v[i].tier, rv.v[i].outcome);
  }
  free(rv.v);
  return EXIT_OK;
}

typedef struct {
  rec r;
  uint8_t *bytes;
  size_t len;
} sealed;

static int cmp_sealed(const void *a, const void *b) {
  const sealed *x = (const sealed *)a;
  const sealed *y = (const sealed *)b;
  return strcmp(x->r.fixture_id, y->r.fixture_id);
}

static int cmd_seal(const char *inv_path, const char *out_path, int n_in,
                    char **in_paths) {
  inventory inv;
  sealed *items = NULL;
  size_t n_items = 0;
  size_t total = 0;
  uint8_t *outbuf = NULL;
  size_t outlen = 0;
  size_t i;
  int rc = EXIT_INVALID;
  recvec rv = {NULL, 0, 0};
  diag d;
  char tmp_path[4096];
  char suite_line[256];
  t64_jsonw w;
  size_t suite_len = 0;
  FILE *f;

  if (!inventory_load(inv_path, &inv)) {
    return EXIT_USAGE;
  }
  if (n_in > 0) {
    items = (sealed *)calloc((size_t)n_in, sizeof items[0]);
    if (items == NULL) {
      fprintf(stderr, "ERROR out of memory\n");
      inventory_free(&inv);
      return EXIT_USAGE;
    }
  }
  for (i = 0; i < (size_t)n_in; i++) {
    uint8_t *b = NULL;
    size_t l = 0;
    if (!load_input(in_paths[i], RECORD_MAX_BYTES, &b, &l)) {
      rc = EXIT_USAGE;
      goto done;
    }
    if (check_line(b, l, 0, &items[n_items].r, NULL, &d) == KIND_NONE) {
      print_fail(in_paths[i], &d);
      free(b);
      goto done;
    }
    items[n_items].bytes = b;
    items[n_items].len = l;
    n_items++;
    total += l;
  }
  if (n_items > 1) {
    qsort(items, n_items, sizeof items[0], cmp_sealed);
  }

  /* The suite record, written by the canonical writer. */
  t64_jsonw_init(&w, suite_line, sizeof suite_line);
  (void)t64_jsonw_begin_object(&w);
  (void)t64_jsonw_key(&w, "case_count", 10);
  (void)t64_jsonw_int(&w, (int64_t)n_items);
  (void)t64_jsonw_key(&w, "inventory_sha256", 16);
  (void)t64_jsonw_string(&w, inv.hash, 64);
  (void)t64_jsonw_key(&w, "schema", 6);
  (void)t64_jsonw_string(&w, "t64.suite", 9);
  (void)t64_jsonw_key(&w, "schema_version", 14);
  (void)t64_jsonw_int(&w, 1);
  (void)t64_jsonw_end_object(&w);
  if (t64_jsonw_finish(&w, &suite_len) != T64_JSONW_OK) {
    fprintf(stderr, "ERROR cannot build the suite record\n");
    rc = EXIT_USAGE;
    goto done;
  }

  outbuf = (uint8_t *)malloc(total + suite_len + 1);
  if (outbuf == NULL) {
    fprintf(stderr, "ERROR out of memory\n");
    rc = EXIT_USAGE;
    goto done;
  }
  for (i = 0; i < n_items; i++) {
    memcpy(outbuf + outlen, items[i].bytes, items[i].len);
    outlen += items[i].len;
  }
  memcpy(outbuf + outlen, suite_line, suite_len);
  outlen += suite_len;

  /* The output must pass the suite check before anything is written. */
  if (!scan(outbuf, outlen, &inv, &rv, &d)) {
    print_fail(out_path, &d);
    goto done;
  }
  if (snprintf(tmp_path, sizeof tmp_path, "%s.tmp", out_path) >= (int)sizeof tmp_path) {
    fprintf(stderr, "ERROR output path is too long\n");
    rc = EXIT_USAGE;
    goto done;
  }
  f = fopen(tmp_path, "wb");
  if (f == NULL || fwrite(outbuf, 1, outlen, f) != outlen || fclose(f) != 0 ||
      rename(tmp_path, out_path) != 0) {
    fprintf(stderr, "ERROR cannot write %s\n", out_path);
    (void)remove(tmp_path);
    rc = EXIT_USAGE;
    goto done;
  }
  printf("ok %s\n", out_path);
  rc = EXIT_OK;

done:
  for (i = 0; i < n_items; i++) {
    free(items[i].bytes);
  }
  free(items);
  free(outbuf);
  free(rv.v);
  inventory_free(&inv);
  return rc;
}

static int usage(void) {
  fprintf(stderr,
          "usage: t64-result-check result FILE\n"
          "       t64-result-check suite --inventory INV FILE\n"
          "       t64-result-check seal --inventory INV --out FILE RESULT...\n"
          "       t64-result-check identity FILE\n");
  return EXIT_USAGE;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    return usage();
  }
  if (strcmp(argv[1], "result") == 0 && argc == 3 && argv[2][0] != '-') {
    return cmd_result(argv[2]);
  }
  if (strcmp(argv[1], "identity") == 0 && argc == 3 && argv[2][0] != '-') {
    return cmd_identity(argv[2]);
  }
  if (strcmp(argv[1], "suite") == 0 && argc == 5 &&
      strcmp(argv[2], "--inventory") == 0 && argv[4][0] != '-') {
    return cmd_suite(argv[3], argv[4]);
  }
  if (strcmp(argv[1], "seal") == 0) {
    const char *inv = NULL;
    const char *out = NULL;
    int i = 2;
    while (i < argc && argv[i][0] == '-') {
      if (strcmp(argv[i], "--inventory") == 0 && i + 1 < argc && inv == NULL) {
        inv = argv[i + 1];
      } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc && out == NULL) {
        out = argv[i + 1];
      } else {
        return usage();
      }
      i += 2;
    }
    if (inv == NULL || out == NULL) {
      return usage();
    }
    return cmd_seal(inv, out, argc - i, argv + i);
  }
  return usage();
}
