/*
 * t64-result-check: the consumer of t64.result v1/v2 records and t64.suite
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
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "t64_classify.h"
#include "t64_json.h"
#include "t64_real_fixture.h"
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

static const char *const real_result_keys[] = {
    "build_id",       "counts",       "detail",       "evidence_kind",
    "evidence_tier",  "fixture_id",   "fixture_sha256", "guest_stop",
    "guest_tick",     "input_digest", "instruction_count",
    "ipl3_payload_sha256", "ipl3_sha256", "log_sha256", "milestones",
    "outcome",        "profile_digest", "profile_id", "schema",
    "schema_version", "source_revision", "state_digest"};

static const char *const count_keys[] = {
    "fail_reported", "log_bytes",     "log_bytes_total",
    "log_lines",     "pass_reported", "unhandled_exception_lines"};

static const char *const real_count_keys[] = {"log_bytes", "log_bytes_total"};
static const char *const guest_stop_keys[] = {"bound_tick", "kind"};
static const char *const milestone_keys[] = {"guest_tick", "kind", "pc"};

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

static const char *const real_stop_names[] = {
    "authored-completion", "guest-bound", "host-error", "not-started",
    "output-overflow", "refused", "report-error", "unsupported-execution"};
static const char *const real_outcome_names[] = {
    "boot-fail", "guest-fail", "harness-error", "missing-fixture", "pass",
    "timeout"};
static const char *const real_detail_names[] = {
    "capture-invalid", "empty-output", "exec-failed", "fixture-absent",
    "guest-bound", "host-error", "no-result", "output-overflow",
    "positive-match", "refusal", "report-error", "report-invalid",
    "runner-exit-nonzero", "runner-signalled", "supervisor-signalled",
    "unsupported-execution", "usage", "wall-backstop", "wrong-transcript"};
static const char *const milestone_names[] = {
    "ipl3-entry", "loader-dma", "hello-entry", "hello-completion"};

static const char expected_profile_id[] = "ntsc-cart-4m-openipl3-warm-v1";
static const char expected_profile_digest[] =
    "d8c12cb2a00410a2153ca62da318166f3ca029dea8e3d6b9c841262cb8203c71";
static const char expected_ipl3_sha256[] =
    "f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068";
static const char expected_ipl3_payload_sha256[] =
    "f716a90b1d36ec4253d096790791256d3543455e54473a755ff8c3d1199b15d5";
static const char expected_fixture_id[] = "hello-openipl3";
static const char expected_fixture_sha256[] =
    "fa276cc175a4e764b6fd5413193c44b51f38fd8135e33e400032827e02f27c7c";
static const char expected_log_sha256[] =
    "86594505bea53c9843f9eaf07e4cf0121cbe0055670324674fd8385c88ffd18e";
static const char expected_empty_sha256[] =
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
static const uint64_t expected_log_bytes = 30u;
static const uint64_t max_real_log_bytes = 4096u;

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

static int write_all(int fd, const uint8_t *p, size_t n) {
  while (n > 0) {
    ssize_t r = write(fd, p, n);
    if (r < 0 && errno == EINTR) {
      continue;
    }
    if (r <= 0) {
      return -1;
    }
    p += r;
    n -= (size_t)r;
  }
  return 0;
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

static int is_lower_hex(const t64_json_node *n, size_t expected_len) {
  size_t i;
  if (n == NULL || n->type != T64_JSON_STRING || n->len != expected_len) {
    return 0;
  }
  for (i = 0; i < expected_len; i++) {
    char c = n->str[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
      return 0;
    }
  }
  return 1;
}

static int is_build_id(const t64_json_node *n) {
  static const char prefix[] = "t64-build-v1-";
  return n != NULL && n->type == T64_JSON_STRING &&
         n->len == sizeof prefix - 1u + 64u &&
         memcmp(n->str, prefix, sizeof prefix - 1u) == 0 &&
         is_lower_hex(&(t64_json_node){.type = T64_JSON_STRING,
                                      .str = n->str + sizeof prefix - 1u,
                                      .len = 64u},
                      64u);
}

static int is_pc(const t64_json_node *n) { return is_lower_hex(n, 16u); }

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

static int schema_and_version_exact(const t64_json_node *root,
                                    const char *schema, int64_t version,
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
  if (v == NULL || v->type != T64_JSON_INT || v->num != version) {
    set_diag(d, "R-VERSION", "schema_version is not the integer %lld",
             (long long)version);
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

/* SHA-256 of the empty string: the log_digest of an empty capture. */
static const char empty_digest[] =
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

/*
 * R-CONSISTENCY (WR-03): relations between outcome, detail, stop_reason, the
 * counts and log_digest that the classifier makes deterministic for a guest
 * outcome (docs/spec/result-protocol.md, precedence section; ADR-0005). The
 * counts are derived from the capture, the capture holds exactly
 * min(log_bytes_total, log limit) bytes, and the guest-fail details are tried
 * in a fixed order before boot-fail, timeout and pass. A record that breaks
 * one of these was not produced by the classifier. counts order:
 * fail_reported, log_bytes, log_bytes_total, log_lines, pass_reported,
 * unhandled_exception_lines.
 */
static int check_consistency(const t64_json_node *root, const uint64_t *c,
                             t64_detail detail, diag *d) {
  const uint64_t fail = c[0];
  const uint64_t bytes = c[1];
  const uint64_t total = c[2];
  const uint64_t lines = c[3];
  const uint64_t pass = c[4];
  const uint64_t unhandled = c[5];
  const t64_json_node *ld = t64_json_get(root, "log_digest");
  const t64_json_node *stop = t64_json_get(root, "stop_reason");
  const char *name = t64_detail_name(detail);
  int is_empty_digest = ld->len == sizeof empty_digest - 1u &&
                        memcmp(ld->str, empty_digest, ld->len) == 0;

  /* Line counts: every counted line is one line, and every line has a byte
   * (an empty line is its LF; an unterminated last line is non-empty). */
  if (pass + fail > lines) {
    set_diag(d, "R-CONSISTENCY",
             "pass_reported + fail_reported exceeds log_lines");
    return 0;
  }
  if (unhandled > lines) {
    set_diag(d, "R-CONSISTENCY", "unhandled_exception_lines exceeds log_lines");
    return 0;
  }
  if (lines > bytes) {
    set_diag(d, "R-CONSISTENCY", "log_lines exceeds log_bytes");
    return 0;
  }
  if (bytes > 0 && lines == 0) {
    set_diag(d, "R-CONSISTENCY", "a non-empty capture has at least one line");
    return 0;
  }
  /* The digest of an empty capture is the digest of the empty string. */
  if ((bytes == 0) != is_empty_digest) {
    set_diag(d, "R-CONSISTENCY",
             "log_digest is the empty-string digest exactly when log_bytes is 0");
    return 0;
  }
  /* The capture is cut short of log_bytes_total only by the log limit, and
   * then log-over-limit is the first guest detail that applies. */
  if (detail == T64_D_LOG_OVER_LIMIT && bytes >= total) {
    set_diag(d, "R-CONSISTENCY",
             "log-over-limit requires log_bytes below log_bytes_total");
    return 0;
  }
  if (detail != T64_D_LOG_OVER_LIMIT && bytes != total) {
    set_diag(d, "R-CONSISTENCY",
             "%s requires log_bytes equal to log_bytes_total", name);
    return 0;
  }
  /* A FAIL line makes fail-reported win over every later guest detail. */
  if (fail > 0 && detail != T64_D_LOG_OVER_LIMIT &&
      detail != T64_D_LOG_INVALID_UTF8 && detail != T64_D_FAIL_REPORTED) {
    set_diag(d, "R-CONSISTENCY", "%s requires fail_reported 0", name);
    return 0;
  }
  switch (detail) {
    case T64_D_FAIL_REPORTED:
      if (fail == 0) {
        set_diag(d, "R-CONSISTENCY", "fail-reported requires fail_reported > 0");
        return 0;
      }
      break;
    case T64_D_OUTPUT_AFTER_DONE:
      if (lines < 2) {
        set_diag(d, "R-CONSISTENCY",
                 "output-after-done requires at least 2 log_lines");
        return 0;
      }
      break;
    case T64_D_VERDICT_INCOMPLETE:
      if (!str_is(stop, "guest-halt")) {
        set_diag(d, "R-CONSISTENCY",
                 "verdict-incomplete requires stop_reason guest-halt");
        return 0;
      }
      break;
    case T64_D_EMPTY_OUTPUT:
      if (bytes != 0) {
        set_diag(d, "R-CONSISTENCY", "boot-fail requires log_bytes 0");
        return 0;
      }
      break;
    case T64_D_GUEST_BOUND:
      if (!str_is(stop, "guest-bound")) {
        set_diag(d, "R-CONSISTENCY", "timeout requires stop_reason guest-bound");
        return 0;
      }
      break;
    default:
      break;
  }
  /* An empty capture is boot-fail unless an earlier detail applied first;
   * only log-over-limit (a zero log limit) can precede it with no bytes. */
  if (bytes == 0 && detail != T64_D_EMPTY_OUTPUT &&
      detail != T64_D_LOG_OVER_LIMIT) {
    set_diag(d, "R-CONSISTENCY", "%s requires log_bytes > 0", name);
    return 0;
  }
  return 1;
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
    /* These details happen only after the fixture was read and verified (a
     * digest mismatch, or anything after the runner was launched). */
    if (t64_json_get(root, "fixture_digest")->type == T64_JSON_NULL &&
        (detail == T64_D_FIXTURE_DIGEST_MISMATCH || detail == T64_D_EXEC_FAILED ||
         detail == T64_D_SUPERVISOR_SIGNALLED || detail == T64_D_WALL_BACKSTOP ||
         detail == T64_D_RUNNER_SIGNALLED || detail == T64_D_RUNNER_EXIT_NONZERO ||
         detail == T64_D_NO_RESULT || detail == T64_D_REPORT_INVALID)) {
      set_diag(d, "R-NULL-RULES", "%s requires fixture_digest to be present",
               t64_detail_name(detail));
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
    if (!check_consistency(root, counts, detail, d)) {
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

static int copy_result_identity(rec *r, const t64_json_node *root, diag *d) {
  const t64_json_node *fixture = t64_json_get(root, "fixture_id");
  const t64_json_node *profile = t64_json_get(root, "profile_id");
  const t64_json_node *build = t64_json_get(root, "build_id");
  const t64_json_node *tier = t64_json_get(root, "evidence_tier");
  const t64_json_node *outcome = t64_json_get(root, "outcome");

  if (fixture->type == T64_JSON_STRING) {
    copy_str(r->fixture_id, sizeof r->fixture_id, fixture);
  } else {
    r->fixture_id[0] = '\0';
  }
  if (profile->type == T64_JSON_STRING) {
    copy_str(r->profile_id, sizeof r->profile_id, profile);
  } else {
    r->profile_id[0] = '\0';
  }
  if (build->type == T64_JSON_STRING) {
    copy_str(r->build_id, sizeof r->build_id, build);
  } else {
    r->build_id[0] = '\0';
  }
  if (tier->type == T64_JSON_STRING) {
    copy_str(r->tier, sizeof r->tier, tier);
  } else {
    r->tier[0] = '\0';
  }
  if (outcome->type == T64_JSON_STRING) {
    copy_str(r->outcome, sizeof r->outcome, outcome);
  } else {
    set_diag(d, "R-TYPE", "outcome must be a string");
    return 0;
  }
  return 1;
}

static int real_detail_outcome_ok(const t64_json_node *detail,
                                 const t64_json_node *outcome) {
  if (str_is(detail, "fixture-absent")) {
    return str_is(outcome, "missing-fixture");
  }
  if (str_is(detail, "wrong-transcript")) {
    return str_is(outcome, "guest-fail");
  }
  if (str_is(detail, "empty-output") ||
      str_is(detail, "unsupported-execution")) {
    return str_is(outcome, "boot-fail");
  }
  if (str_is(detail, "guest-bound")) {
    return str_is(outcome, "timeout");
  }
  if (str_is(detail, "positive-match")) {
    return str_is(outcome, "pass");
  }
  return str_is(outcome, "harness-error");
}

static int real_milestone_index(const t64_json_node *kind) {
  size_t i;
  for (i = 0; i < ARRAY_LEN(milestone_names); i++) {
    if (str_is(kind, milestone_names[i])) {
      return (int)i;
    }
  }
  return -1;
}

typedef struct {
  size_t count;
  int index[4];
  uint64_t tick[4];
} real_milestone_summary;

static int validate_real_milestones(const t64_json_node *array,
                                    uint64_t guest_tick,
                                    real_milestone_summary *summary,
                                    diag *d) {
  const t64_json_node *item;
  int previous_index = -1;
  uint64_t previous_tick = 0u;
  size_t i = 0u;

  if (array->type != T64_JSON_ARRAY || array->count > 4u) {
    set_diag(d, "R-TYPE", "milestones must be an array of at most four records");
    return 0;
  }
  summary->count = array->count;
  for (item = array->first; item != NULL; item = item->next) {
    const t64_json_node *kind;
    const t64_json_node *tick_node;
    const t64_json_node *pc;
    uint64_t tick;
    int index;

    if (i >= ARRAY_LEN(summary->index) ||
        !keys_ok(item, milestone_keys, ARRAY_LEN(milestone_keys),
                 "milestone", d)) {
      return 0;
    }
    kind = t64_json_get(item, "kind");
    tick_node = t64_json_get(item, "guest_tick");
    pc = t64_json_get(item, "pc");
    index = real_milestone_index(kind);
    if (index < 0) {
      set_diag(d, "R-ENUM", "milestone kind is not in the closed milestone set");
      return 0;
    }
    if (!t64_json_u64_string(tick_node, &tick) || !is_pc(pc)) {
      set_diag(d, "R-TYPE",
               "milestone guest_tick must be a decimal u64 and pc must be 16 lowercase hex");
      return 0;
    }
    if (index <= previous_index || tick < previous_tick || tick > guest_tick) {
      set_diag(d, "R-CONSISTENCY",
               "milestones must be unique, ordered and no later than guest_tick");
      return 0;
    }
    if (index == 0 &&
        !str_is(pc, "ffffffffa4000040")) {
      set_diag(d, "R-IDENTITY", "ipl3-entry pc does not match the pinned IPL3 entry");
      return 0;
    }
    if (index == 2 && !str_is(pc, "ffffffff80000400")) {
      set_diag(d, "R-IDENTITY", "hello-entry pc does not match the authored fixture entry");
      return 0;
    }
    summary->index[i] = index;
    summary->tick[i] = tick;
    previous_index = index;
    previous_tick = tick;
    i++;
  }
  return 1;
}

static int validate_real_result(const t64_json_node *root, rec *r, diag *d) {
  static const char *const identity_fields[] = {
      "build_id", "source_revision", "profile_id", "profile_digest",
      "fixture_id", "fixture_sha256", "ipl3_sha256",
      "ipl3_payload_sha256", "input_digest", "log_sha256", "state_digest"};
  const t64_json_node *counts_node;
  const t64_json_node *stop_node;
  const t64_json_node *milestones_node;
  const t64_json_node *n;
  const t64_json_node *detail;
  const t64_json_node *outcome;
  const t64_json_node *stop_kind;
  uint64_t guest_tick;
  uint64_t instruction_count;
  uint64_t bound_tick;
  uint64_t log_bytes;
  uint64_t log_bytes_total;
  real_milestone_summary milestones;
  size_t null_identities = 0u;
  size_t i;
  int prelaunch;
  int supervisor_identity_missing = 0;
  int input_digest_missing = 0;
  int log_digest_missing = 0;
  int state_digest_missing = 0;
  int postlaunch_untrusted = 0;

  if (!schema_and_version_exact(root, "t64.result", 2, d)) {
    return 0;
  }
  if (!keys_ok(root, real_result_keys, ARRAY_LEN(real_result_keys), "record", d)) {
    return 0;
  }

  n = t64_json_get(root, "evidence_kind");
  if (!str_is(n, "guest-execution") ||
      !str_is(t64_json_get(root, "evidence_tier"), "smoke")) {
    set_diag(d, "R-ENUM", "v2 requires evidence_kind guest-execution and evidence_tier smoke");
    return 0;
  }
  if (!name_ok(t64_json_get(root, "fixture_id"), 64u, 0)) {
    const t64_json_node *fixture_id = t64_json_get(root, "fixture_id");
    if (fixture_id->type != T64_JSON_NULL) {
      set_diag(d, "R-TYPE", "fixture_id must match [A-Za-z0-9._-]{1,64} or be null prelaunch");
      return 0;
    }
  }

  for (i = 0; i < ARRAY_LEN(identity_fields); i++) {
    n = t64_json_get(root, identity_fields[i]);
    if (n->type == T64_JSON_NULL) {
      null_identities++;
      if (i < 8u) supervisor_identity_missing = 1;
      else if (i == 8u) input_digest_missing = 1;
      else if (i == 9u) log_digest_missing = 1;
      else state_digest_missing = 1;
    } else if (n->type != T64_JSON_STRING) {
      set_diag(d, "R-TYPE", "%s must be a string or null prelaunch", identity_fields[i]);
      return 0;
    }
  }
  prelaunch = null_identities == ARRAY_LEN(identity_fields);
  postlaunch_untrusted = null_identities != 0u && !prelaunch;
  if (postlaunch_untrusted && supervisor_identity_missing) {
    set_diag(d, "R-NULL-RULES",
             "postlaunch records must retain supervisor-known build, profile, fixture and IPL3 identities");
    return 0;
  }

  if (!prelaunch) {
    const t64_json_node *fixture_id = t64_json_get(root, "fixture_id");
    const t64_json_node *fixture_sha = t64_json_get(root, "fixture_sha256");
    const t64_json_node *source_revision = t64_json_get(root, "source_revision");

    if (!is_build_id(t64_json_get(root, "build_id")) ||
        !is_lower_hex(source_revision, 40u) ||
        !name_ok(t64_json_get(root, "profile_id"), 128u, 1) ||
        !is_hex64(t64_json_get(root, "profile_digest")) ||
        !is_hex64(fixture_sha) ||
        !is_hex64(t64_json_get(root, "ipl3_sha256")) ||
        !is_hex64(t64_json_get(root, "ipl3_payload_sha256")) ||
        (!input_digest_missing &&
         !is_hex64(t64_json_get(root, "input_digest"))) ||
        (!log_digest_missing &&
         !is_hex64(t64_json_get(root, "log_sha256"))) ||
        (!state_digest_missing &&
         !is_hex64(t64_json_get(root, "state_digest")))) {
      set_diag(d, "R-TYPE", "real identities must use their closed name and SHA-256 forms");
      return 0;
    }
    if (t64_real_fixture_find_identity(fixture_id->str,
                                       fixture_sha->str) == NULL) {
      set_diag(d, "R-IDENTITY",
               "fixture id and SHA-256 pair is not in the real smoke allowlist");
      return 0;
    }
    if (!str_is(t64_json_get(root, "profile_id"), expected_profile_id) ||
        !str_is(t64_json_get(root, "profile_digest"), expected_profile_digest) ||
        !str_is(t64_json_get(root, "ipl3_sha256"), expected_ipl3_sha256) ||
        !str_is(t64_json_get(root, "ipl3_payload_sha256"),
                expected_ipl3_payload_sha256)) {
      set_diag(d, "R-IDENTITY", "profile or pinned IPL3 identity does not match this v2 contract");
      return 0;
    }
  }

  n = t64_json_get(root, "guest_tick");
  if (!t64_json_u64_string(n, &guest_tick) ||
      !t64_json_u64_string(t64_json_get(root, "instruction_count"),
                           &instruction_count)) {
    set_diag(d, "R-TYPE", "guest_tick and instruction_count must be decimal u64 strings");
    return 0;
  }

  stop_node = t64_json_get(root, "guest_stop");
  if (!keys_ok(stop_node, guest_stop_keys, ARRAY_LEN(guest_stop_keys),
               "guest_stop", d)) {
    return 0;
  }
  stop_kind = t64_json_get(stop_node, "kind");
  if (!in_list(stop_kind, real_stop_names, ARRAY_LEN(real_stop_names))) {
    set_diag(d, "R-ENUM", "guest_stop.kind is outside the closed v2 stop set");
    return 0;
  }
  if (!t64_json_u64_string(t64_json_get(stop_node, "bound_tick"), &bound_tick) ||
      guest_tick > bound_tick || instruction_count > guest_tick) {
    set_diag(d, "R-CONSISTENCY",
             "guest_tick must not exceed bound_tick or precede instruction_count");
    return 0;
  }

  counts_node = t64_json_get(root, "counts");
  if (!keys_ok(counts_node, real_count_keys, ARRAY_LEN(real_count_keys),
               "counts", d)) {
    return 0;
  }
  if (!t64_json_u64_string(t64_json_get(counts_node, "log_bytes"), &log_bytes) ||
      !t64_json_u64_string(t64_json_get(counts_node, "log_bytes_total"),
                           &log_bytes_total) ||
      log_bytes > max_real_log_bytes || log_bytes > log_bytes_total) {
    set_diag(d, "R-TYPE", "counts must hold bounded log_bytes and decimal u64 log_bytes_total");
    return 0;
  }

  detail = t64_json_get(root, "detail");
  outcome = t64_json_get(root, "outcome");
  if (detail->type != T64_JSON_STRING || outcome->type != T64_JSON_STRING) {
    set_diag(d, "R-TYPE", "detail and outcome must be strings");
    return 0;
  }
  if (!in_list(outcome, real_outcome_names, ARRAY_LEN(real_outcome_names)) ||
      !in_list(detail, real_detail_names, ARRAY_LEN(real_detail_names))) {
    set_diag(d, "R-ENUM", "outcome or detail is outside the closed v2 enum");
    return 0;
  }
  if (!real_detail_outcome_ok(detail, outcome)) {
    set_diag(d, "R-DETAIL-OUTCOME", "detail does not belong to outcome");
    return 0;
  }

  milestones_node = t64_json_get(root, "milestones");
  if (!validate_real_milestones(milestones_node, guest_tick, &milestones, d)) {
    return 0;
  }

  if (prelaunch) {
    int fixture_absent = str_is(detail, "fixture-absent") &&
                         str_is(outcome, "missing-fixture") &&
                         str_is(stop_kind, "not-started");
    int refused = str_is(detail, "refusal") &&
                  str_is(outcome, "harness-error") &&
                  str_is(stop_kind, "refused");
    int rejected_before_launch =
        str_is(outcome, "harness-error") &&
        (str_is(detail, "usage") || str_is(detail, "exec-failed")) &&
        str_is(stop_kind, "not-started");

    if ((!fixture_absent && !refused && !rejected_before_launch) ||
        guest_tick != 0u || instruction_count != 0u || log_bytes != 0u ||
        log_bytes_total != 0u || milestones.count != 0u) {
      set_diag(d, "R-NULL-RULES",
               "null identities are limited to zero-progress prelaunch errors or missing-fixture");
      return 0;
    }
    if (!copy_result_identity(r, root, d)) {
      return 0;
    }
    return 1;
  }

  if (postlaunch_untrusted) {
    if (!str_is(outcome, "harness-error") ||
        !(str_is(stop_kind, "host-error") ||
          str_is(stop_kind, "report-error")) ||
        ((input_digest_missing || state_digest_missing) &&
         (!state_digest_missing || guest_tick != 0u ||
          instruction_count != 0u || milestones.count != 0u)) ||
        (log_digest_missing &&
         (log_bytes != 0u || log_bytes_total != 0u))) {
      set_diag(d, "R-NULL-RULES",
               "postlaunch null input/state/log digests are limited to untrusted harness errors with zero verified state progress and zero counts for unknown logs");
      return 0;
    }
  }

  if (str_is(stop_kind, "not-started") || str_is(stop_kind, "refused")) {
    set_diag(d, "R-NULL-RULES", "not-started and refused stops require null prelaunch identities");
    return 0;
  }
  if (log_digest_missing) {
    if (!postlaunch_untrusted || log_bytes != 0u || log_bytes_total != 0u) {
      set_diag(d, "R-NULL-RULES",
               "null log digest requires an untrusted postlaunch harness error and zero trusted counts");
      return 0;
    }
  } else if (log_bytes == 0u) {
    if (!str_is(t64_json_get(root, "log_sha256"), expected_empty_sha256)) {
      set_diag(d, "R-CONSISTENCY", "empty output requires the SHA-256 of empty bytes");
      return 0;
    }
  } else if (str_is(t64_json_get(root, "log_sha256"), expected_empty_sha256)) {
    set_diag(d, "R-CONSISTENCY", "non-empty output cannot use the empty-string digest");
    return 0;
  }

  if (str_is(stop_kind, "guest-bound")) {
    if (guest_tick != bound_tick ||
        (milestones.count > 0u && milestones.index[milestones.count - 1u] == 3)) {
      set_diag(d, "R-CONSISTENCY",
               "guest-bound must reach bound_tick without a completion milestone");
      return 0;
    }
    if (log_bytes == 0u) {
      if (!str_is(detail, "empty-output") || !str_is(outcome, "boot-fail")) {
        set_diag(d, "R-CONSISTENCY", "empty output at the guest bound is boot-fail");
        return 0;
      }
    } else if (!str_is(detail, "guest-bound") || !str_is(outcome, "timeout")) {
      set_diag(d, "R-CONSISTENCY", "non-empty incomplete output at the guest bound is timeout");
      return 0;
    }
  } else if (str_is(stop_kind, "authored-completion")) {
    if (milestones.count != 4u || milestones.index[0] != 0 ||
        milestones.index[1] != 1 || milestones.index[2] != 2 ||
        milestones.index[3] != 3) {
      set_diag(d, "R-CONSISTENCY",
               "authored completion requires ordered IPL3, loader-DMA, hello-entry and completion milestones");
      return 0;
    }
    if (log_bytes == 0u) {
      if (!str_is(detail, "empty-output") || !str_is(outcome, "boot-fail")) {
        set_diag(d, "R-CONSISTENCY", "empty output at authored completion is boot-fail");
        return 0;
      }
    } else if (str_is(t64_json_get(root, "log_sha256"), expected_log_sha256) &&
               log_bytes == expected_log_bytes &&
               log_bytes_total == expected_log_bytes) {
      if (instruction_count == 0u ||
          !str_is(t64_json_get(root, "fixture_id"), expected_fixture_id) ||
          !str_is(t64_json_get(root, "fixture_sha256"), expected_fixture_sha256) ||
          !str_is(detail, "positive-match") || !str_is(outcome, "pass") ||
          !str_is(t64_json_get(milestones_node->first->next->next->next, "pc"),
                  "ffffffff80000478")) {
        set_diag(d, "R-CONSISTENCY",
                 "pass requires the exact fixture, transcript, nonzero retirement and completion PC");
        return 0;
      }
    } else if (!str_is(detail, "wrong-transcript") ||
               !str_is(outcome, "guest-fail")) {
      set_diag(d, "R-CONSISTENCY", "wrong completed transcript is guest-fail");
      return 0;
    }
  } else if (str_is(stop_kind, "unsupported-execution")) {
    if (!str_is(detail, "unsupported-execution") ||
        !str_is(outcome, "boot-fail") ||
        (milestones.count > 0u && milestones.index[milestones.count - 1u] == 3)) {
      set_diag(d, "R-CONSISTENCY",
               "unsupported execution before completion is boot-fail with no completion milestone");
      return 0;
    }
  } else if (str_is(stop_kind, "output-overflow")) {
    if (!str_is(detail, "output-overflow") ||
        !str_is(outcome, "harness-error") ||
        log_bytes != max_real_log_bytes || log_bytes_total <= log_bytes) {
      set_diag(d, "R-CONSISTENCY",
               "output overflow is harness-error with a full retained prefix and larger total");
      return 0;
    }
  } else if (str_is(stop_kind, "host-error")) {
    if (!str_is(outcome, "harness-error") ||
        !(str_is(detail, "host-error") || str_is(detail, "wall-backstop") ||
          str_is(detail, "supervisor-signalled") ||
          str_is(detail, "runner-signalled") ||
          str_is(detail, "runner-exit-nonzero"))) {
      set_diag(d, "R-DETAIL-OUTCOME", "host-error stop requires a host harness-error detail");
      return 0;
    }
  } else if (str_is(stop_kind, "report-error")) {
    if (!str_is(outcome, "harness-error") ||
        !(str_is(detail, "report-error") || str_is(detail, "no-result") ||
          str_is(detail, "report-invalid") ||
          str_is(detail, "capture-invalid"))) {
      set_diag(d, "R-DETAIL-OUTCOME", "report-error stop requires a report harness-error detail");
      return 0;
    }
  }

  if (str_is(detail, "output-overflow") && !str_is(stop_kind, "output-overflow")) {
    set_diag(d, "R-DETAIL-OUTCOME", "output-overflow detail requires output-overflow stop");
    return 0;
  }
  if (log_bytes != log_bytes_total &&
      !str_is(stop_kind, "output-overflow") &&
      !str_is(detail, "capture-invalid")) {
    set_diag(d, "R-CONSISTENCY", "complete captures require log_bytes equal log_bytes_total");
    return 0;
  }

  return copy_result_identity(r, root, d);
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

static int is_real_v2_candidate(const t64_json_node *root) {
  const t64_json_node *schema;
  const t64_json_node *version;
  int has_v2_fields;
  int synthetic_v1_stamp;

  if (root->type != T64_JSON_OBJECT) {
    return 0;
  }
  schema = t64_json_get(root, "schema");
  if (!str_is(schema, "t64.result")) {
    return 0;
  }
  version = t64_json_get(root, "schema_version");
  has_v2_fields = t64_json_get(root, "evidence_kind") != NULL ||
                  t64_json_get(root, "guest_stop") != NULL ||
                  t64_json_get(root, "instruction_count") != NULL ||
                  t64_json_get(root, "milestones") != NULL;
  if (has_v2_fields) {
    return 1;
  }
  synthetic_v1_stamp =
      str_is(t64_json_get(root, "evidence_tier"), "synthetic") &&
      str_is(t64_json_get(root, "profile_id"), "synthetic-stub") &&
      str_is(t64_json_get(root, "build_id"), "0.0.0+synthetic") &&
      t64_json_get(root, "fixture_digest") != NULL &&
      t64_json_get(root, "log_digest") != NULL &&
      t64_json_get(root, "stop_reason") != NULL;
  /* The locked synthetic v1 stamp is the one legacy-shaped exception needed
   * for v1's version-before-keys diagnostic. All other version-2 objects,
   * including malformed v2 objects missing marker fields, enter v2 validation. */
  if (synthetic_v1_stamp) {
    return 0;
  }
  return version != NULL && version->type == T64_JSON_INT && version->num == 2;
}

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
    if (is_real_v2_candidate(doc.root)) {
      kind = validate_real_result(doc.root, r, d) ? KIND_RESULT : KIND_NONE;
    } else {
      kind = validate_result(doc.root, r, d) ? KIND_RESULT : KIND_NONE;
    }
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
  int fd;

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
  /* IN-01: a fresh mkstemp name (O_EXCL) next to FILE, so nothing planted at
   * a predictable FILE.tmp (a symlink to a victim) is followed, and the
   * descriptor is closed on every path. */
  if (snprintf(tmp_path, sizeof tmp_path, "%s.tmp.XXXXXX", out_path) >=
      (int)sizeof tmp_path) {
    fprintf(stderr, "ERROR output path is too long\n");
    rc = EXIT_USAGE;
    goto done;
  }
  fd = mkstemp(tmp_path);
  if (fd < 0) {
    fprintf(stderr, "ERROR cannot write %s\n", out_path);
    rc = EXIT_USAGE;
    goto done;
  }
  {
    int bad_write = fchmod(fd, 0644) != 0 || write_all(fd, outbuf, outlen) != 0;
    int bad_close = close(fd) != 0;
    if (bad_write || bad_close || rename(tmp_path, out_path) != 0) {
      fprintf(stderr, "ERROR cannot write %s\n", out_path);
      (void)unlink(tmp_path);
      rc = EXIT_USAGE;
      goto done;
    }
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
