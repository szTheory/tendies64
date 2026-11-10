/*
 * t64-provenance-check: per-copy provenance records (GOV-02), authored here.
 *
 *   t64-provenance-check --root DIR --copies FILE --files-from LIST
 *
 * LIST holds NUL-separated repo-relative paths (scripts/check-provenance.sh
 * builds it from git). FILE is provenance/copies.json. Output is one line per
 * finding, "RULE <id> <path or entry>: message", then
 *   provenance: files=N copied_looking=C entries=E errors=X
 * and "ok <FILE>" when clean. Exit 0 clean, 1 findings, 2 harness error (bad
 * usage, an unreadable list).
 *
 * A listed file is copied-looking when its path is under third_party/, or
 * when, within its first 20 lines, a line carries a license-identifier tag at
 * line start (after optional whitespace and one comment leader from the set
 * "<!--", "/ *", "//", "--", "*", "#", ";") whose expression is not
 * Apache-2.0, or a copyright-text tag naming a holder other than
 * "Tendies64 contributors". A tag in the middle of a line, or after line 20,
 * never counts. Only the first 16 KiB of a file is read. The tag text is
 * assembled from fragments below so this source never carries a header-form
 * tag itself.
 *
 * Rules: P-JSON, P-FIELD, P-SHA, P-OWNER-MISSING, P-OWNER-EMAIL,
 * P-SPDX-NOT-ALLOWED, P-UNRECORDED-COPY, P-PATH-MISSING, P-PATH, P-DUP-PATH,
 * P-LICENSE-TEXT, P-LICENSES-EXTRA, P-ARCHIVED-OWNER, P-HEADER,
 * P-MODS-MISSING, P-ZERO-SCANNED.
 *
 * Every path, from the list or from a record, is checked lexically and then
 * with lstat on each component before any open; a symlink anywhere is
 * refused and files are opened with O_NOFOLLOW.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "t64_json.h"

enum { EXIT_CLEAN = 0, EXIT_FINDINGS = 1, EXIT_HARNESS = 2 };

#define MAX_LIST_BYTES (32u * 1024u * 1024u)
#define HEAD_BYTES 16384u
#define HEAD_LINES 20u
#define FULL_MAX 8192u
#define EXPR_MAX 256u

#define SPDX_WORD "SPDX"
static const char TAG_LICENSE[] = SPDX_WORD "-License-Identifier:";
static const char TAG_COPYRIGHT[] = SPDX_WORD "-FileCopyrightText:";

static const char *const ALLOWED_SPDX[] = {"ISC", "MIT", "BSD-3-Clause", "Unlicense"};
#define OWN_LICENSE "Apache-2.0"
#define OWN_HOLDER "Tendies64 contributors"
#define OWN_OWNER "Tendies64"

static const char *g_root = ".";
static long g_errors;
static size_t g_files, g_looking, g_entries;

#if defined(__GNUC__) || defined(__clang__)
#define T64_PRINTF_LIKE(a, b) __attribute__((format(printf, a, b)))
#else
#define T64_PRINTF_LIKE(a, b)
#endif

static void die(const char *msg) {
  (void)fprintf(stderr, "provenance-check: error: %s\n", msg);
  exit(EXIT_HARNESS);
}

static void usage(void) {
  (void)fprintf(stderr,
                "usage: t64-provenance-check --root DIR --copies FILE --files-from LIST\n");
  exit(EXIT_HARNESS);
}

/* Replaces every byte outside printable ASCII with '?'. keep_space keeps ' '. */
static void sanitize(char *s, int keep_space) {
  for (; *s != '\0'; s++) {
    unsigned char c = (unsigned char)*s;
    if (c < 0x20u || c >= 0x7fu || (c == ' ' && !keep_space)) *s = '?';
  }
}

T64_PRINTF_LIKE(3, 4)
static void finding(const char *rule, const char *where, const char *fmt, ...) {
  char w[200];
  char m[480];
  va_list ap;

  (void)snprintf(w, sizeof w, "%s", where);
  sanitize(w, 0);
  va_start(ap, fmt);
  (void)vsnprintf(m, sizeof m, fmt, ap);
  va_end(ap);
  sanitize(m, 1);
  (void)printf("RULE %s %s: %s\n", rule, w, m);
  g_errors++;
}

/* ---- JSON helpers ------------------------------------------------------ */

static int is_str(const t64_json_node *n) {
  return n != NULL && n->type == T64_JSON_STRING && strlen(n->str) == n->len;
}

static int is_blank(const t64_json_node *n) {
  size_t i;
  if (!is_str(n)) return 1;
  for (i = 0; i < n->len; i++) {
    unsigned char c = (unsigned char)n->str[i];
    if (c != ' ' && c != '\t' && c != '\n' && c != '\r') return 0;
  }
  return 1;
}

static int str_eq(const t64_json_node *n, const char *lit) {
  return is_str(n) && strcmp(n->str, lit) == 0;
}

static int key_in(const char *key, const char *const *set, size_t n) {
  size_t i;
  for (i = 0; i < n; i++) {
    if (strcmp(key, set[i]) == 0) return 1;
  }
  return 0;
}

/* YYYY-MM-DD with month 01-12 and day 01-31. */
static int is_date(const t64_json_node *n) {
  const char *s;
  size_t i;
  int mo, da;
  if (!is_str(n) || n->len != 10u) return 0;
  s = n->str;
  for (i = 0; i < 10u; i++) {
    if (i == 4u || i == 7u) {
      if (s[i] != '-') return 0;
    } else if (s[i] < '0' || s[i] > '9') {
      return 0;
    }
  }
  mo = (s[5] - '0') * 10 + (s[6] - '0');
  da = (s[8] - '0') * 10 + (s[9] - '0');
  return mo >= 1 && mo <= 12 && da >= 1 && da <= 31;
}

static int is_hex40(const t64_json_node *n) {
  size_t i;
  if (!is_str(n) || n->len != 40u) return 0;
  for (i = 0; i < 40u; i++) {
    char c = n->str[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return 0;
  }
  return 1;
}

/* Characters allowed in an SPDX id used to build LICENSES/<id>.txt. */
static int spdx_safe(const char *s) {
  size_t i;
  if (s[0] == '\0') return 0;
  for (i = 0; s[i] != '\0'; i++) {
    char c = s[i];
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          c == '.' || c == '+' || c == '-'))
      return 0;
  }
  return 1;
}

static int owner_shape_ok(const char *s) {
  size_t i;
  char c = s[0];
  if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
    return 0;
  for (i = 1; s[i] != '\0'; i++) {
    c = s[i];
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          c == ' ' || c == '.' || c == '_' || c == '-'))
      return 0;
  }
  return 1;
}

/* Reports every member of obj that is not in allowed, as P-FIELD. */
static void check_unknown_keys(const t64_json_node *obj, const char *const *allowed,
                               size_t n_allowed, const char *where) {
  const t64_json_node *m;
  for (m = obj->first; m != NULL; m = m->next) {
    if (m->key == NULL || !key_in(m->key, allowed, n_allowed))
      finding("P-FIELD", where, "unknown key \"%s\"", m->key != NULL ? m->key : "?");
  }
}

static int require_keys(const t64_json_node *obj, const char *const *req, size_t n,
                        const char *where) {
  size_t i;
  int ok = 1;
  for (i = 0; i < n; i++) {
    if (t64_json_get(obj, req[i]) == NULL) {
      finding("P-FIELD", where, "missing required key \"%s\"", req[i]);
      ok = 0;
    }
  }
  return ok;
}

/* ---- paths ------------------------------------------------------------- */

/* Lexical rule for a repo-relative path, applied before any filesystem access.
 * Returns NULL when acceptable, else the reason. */
static const char *path_lexical(const char *p, size_t len) {
  size_t i;
  size_t seg = 0;
  if (len == 0u) return "path is empty";
  if (strlen(p) != len) return "path contains a NUL byte";
  if (p[0] == '/') return "path is absolute";
  if (len + 1u >= 4000u) return "path is too long";
  for (i = 0; i < len; i++) {
    unsigned char c = (unsigned char)p[i];
    if (c < 0x20u || c == 0x7fu) return "path contains a control byte";
    if (c == '\\') return "path contains a backslash";
  }
  for (i = 0; i <= len; i++) {
    if (i == len || p[i] == '/') {
      size_t sl = i - seg;
      if (sl == 0u) return "path has an empty segment";
      if (sl == 1u && p[seg] == '.') return "path has a '.' segment";
      if (sl == 2u && p[seg] == '.' && p[seg + 1u] == '.') return "path has a '..' segment";
      seg = i + 1u;
    }
  }
  return NULL;
}

typedef enum { FS_ABSENT, FS_PRESENT, FS_BAD } fs_state;

/* lstat every component below the root so no link or special file is
 * followed; the last component must be a regular file. */
static fs_state fs_check(const char *rel, char *full, size_t full_cap, const char **why) {
  size_t rl = strlen(g_root);
  size_t pl = strlen(rel);
  size_t i;
  struct stat st;

  if (rl + 1u + pl + 1u > full_cap) {
    *why = "path is too long";
    return FS_BAD;
  }
  memcpy(full, g_root, rl);
  full[rl] = '/';
  for (i = 0; i <= pl; i++) {
    if (i == pl || rel[i] == '/') {
      memcpy(full + rl + 1u, rel, i);
      full[rl + 1u + i] = '\0';
      if (lstat(full, &st) != 0) {
        if (errno == ENOENT) return FS_ABSENT;
        if (errno == ENOTDIR) {
          *why = "a parent of the path is not a directory";
          return FS_BAD;
        }
        *why = "cannot stat the path";
        return FS_BAD;
      }
      if (S_ISLNK(st.st_mode)) {
        *why = "path goes through a symlink";
        return FS_BAD;
      }
      if (i == pl) {
        if (!S_ISREG(st.st_mode)) {
          *why = "path is not a regular file";
          return FS_BAD;
        }
      } else if (!S_ISDIR(st.st_mode)) {
        *why = "a parent of the path is not a directory";
        return FS_BAD;
      }
    }
  }
  return FS_PRESENT;
}

/* ---- header detection -------------------------------------------------- */

typedef struct {
  int other_license; /* a license tag with an expression other than Apache-2.0 */
  int foreign_holder; /* a copyright tag naming another holder */
  int want_match;     /* a license tag equal to the wanted id */
} hdr_t;

static int starts_with(const char *s, size_t len, const char *lit) {
  size_t l = strlen(lit);
  return len >= l && memcmp(s, lit, l) == 0;
}

static int is_ws(char c) { return c == ' ' || c == '\t' || c == '\r'; }

/* Copies the value part of a tag line (after the tag) into out: leading
 * whitespace dropped, a trailing comment closer and whitespace trimmed. */
static void tag_value(const char *s, size_t len, char *out, size_t cap) {
  size_t b = 0;
  size_t e = len;
  size_t n;
  while (b < e && is_ws(s[b])) b++;
  while (e > b && is_ws(s[e - 1u])) e--;
  if (e - b >= 3u && memcmp(s + e - 3u, "-->", 3u) == 0) e -= 3u;
  else if (e - b >= 2u && memcmp(s + e - 2u, "*/", 2u) == 0) e -= 2u;
  while (e > b && is_ws(s[e - 1u])) e--;
  n = e - b;
  if (n >= cap) n = cap - 1u;
  memcpy(out, s + b, n);
  out[n] = '\0';
}

/* Holder part of a copyright value: drops a leading "Copyright", "(c)" or a
 * copyright sign, then year ranges, commas and spaces. */
static const char *holder_of(const char *v) {
  const char *p = v;
  for (;;) {
    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, "Copyright", 9u) == 0 || strncmp(p, "copyright", 9u) == 0) {
      p += 9;
    } else if (strncmp(p, "(c)", 3u) == 0 || strncmp(p, "(C)", 3u) == 0) {
      p += 3;
    } else if ((unsigned char)p[0] == 0xC2u && (unsigned char)p[1] == 0xA9u) {
      p += 2;
    } else if ((*p >= '0' && *p <= '9') || *p == '-' || *p == ',') {
      p++;
    } else {
      break;
    }
  }
  return p;
}

static void scan_line(const char *s, size_t len, const char *want, hdr_t *h) {
  static const char *const leaders[] = {"<!--", "/*", "//", "--", "*", "#", ";"};
  size_t i = 0;
  size_t k;
  char expr[EXPR_MAX];

  while (i < len && (s[i] == ' ' || s[i] == '\t')) i++;
  for (k = 0; k < sizeof leaders / sizeof leaders[0]; k++) {
    if (starts_with(s + i, len - i, leaders[k])) {
      i += strlen(leaders[k]);
      break;
    }
  }
  while (i < len && (s[i] == ' ' || s[i] == '\t')) i++;

  if (starts_with(s + i, len - i, TAG_LICENSE)) {
    size_t t = strlen(TAG_LICENSE);
    tag_value(s + i + t, len - i - t, expr, sizeof expr);
    if (strcmp(expr, OWN_LICENSE) != 0) h->other_license = 1;
    if (want != NULL && strcmp(expr, want) == 0) h->want_match = 1;
  } else if (starts_with(s + i, len - i, TAG_COPYRIGHT)) {
    size_t t = strlen(TAG_COPYRIGHT);
    tag_value(s + i + t, len - i - t, expr, sizeof expr);
    if (strcmp(holder_of(expr), OWN_HOLDER) != 0) h->foreign_holder = 1;
  }
}

/* Looks at the first HEAD_LINES lines of buf[0..n). */
static void scan_header(const char *buf, size_t n, const char *want, hdr_t *h) {
  size_t pos = 0;
  size_t line;
  h->other_license = 0;
  h->foreign_holder = 0;
  h->want_match = 0;
  for (line = 0; line < HEAD_LINES && pos < n; line++) {
    size_t end = pos;
    while (end < n && buf[end] != '\n') end++;
    scan_line(buf + pos, end - pos, want, h);
    pos = end + 1u;
  }
}

/* Reads up to HEAD_BYTES of a regular file without following a symlink.
 * Returns 0 on success. */
static int read_head(const char *full, char *buf, size_t *n) {
  int fd = open(full, O_RDONLY | O_NOFOLLOW);
  struct stat st;
  size_t got = 0;

  if (fd < 0) return 1;
  if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
    (void)close(fd);
    return 1;
  }
  while (got < HEAD_BYTES) {
    ssize_t r = read(fd, buf + got, HEAD_BYTES - got);
    if (r < 0) {
      if (errno == EINTR) continue;
      (void)close(fd);
      return 1;
    }
    if (r == 0) break;
    got += (size_t)r;
  }
  (void)close(fd);
  *n = got;
  return 0;
}

/* ---- records ----------------------------------------------------------- */

typedef struct {
  const char *path;
  size_t entry;
} rec_t;

static rec_t *g_rec;
static size_t g_rec_n, g_rec_cap;
static const char **g_lic; /* license_text paths named by entries */
static size_t g_lic_n, g_lic_cap;
static const char **g_ids;
static size_t g_ids_n, g_ids_cap;

static void *grow(void *p, size_t *cap, size_t elem) {
  size_t nc = *cap == 0u ? 16u : *cap * 2u;
  void *np = realloc(p, nc * elem);
  if (np == NULL) die("out of memory");
  *cap = nc;
  return np;
}

static size_t rec_find(const char *path) {
  size_t i;
  for (i = 0; i < g_rec_n; i++) {
    if (strcmp(g_rec[i].path, path) == 0) return i;
  }
  return (size_t)-1;
}

static void rec_add(const char *path, size_t entry) {
  if (g_rec_n == g_rec_cap) g_rec = (rec_t *)grow(g_rec, &g_rec_cap, sizeof *g_rec);
  g_rec[g_rec_n].path = path;
  g_rec[g_rec_n].entry = entry;
  g_rec_n++;
}

static int lic_has(const char *path) {
  size_t i;
  for (i = 0; i < g_lic_n; i++) {
    if (strcmp(g_lic[i], path) == 0) return 1;
  }
  return 0;
}

static void lic_add(const char *path) {
  if (g_lic_n == g_lic_cap) g_lic = (const char **)grow((void *)g_lic, &g_lic_cap, sizeof *g_lic);
  g_lic[g_lic_n++] = path;
}

static int id_seen(const char *id) {
  size_t i;
  for (i = 0; i < g_ids_n; i++) {
    if (strcmp(g_ids[i], id) == 0) return 1;
  }
  return 0;
}

static void id_add(const char *id) {
  if (g_ids_n == g_ids_cap) g_ids = (const char **)grow((void *)g_ids, &g_ids_cap, sizeof *g_ids);
  g_ids[g_ids_n++] = id;
}

static void check_exposure_item(const t64_json_node *it, const char *where) {
  static const char *const keys[] = {"date", "who", "source", "license", "action"};
  size_t i;
  if (it->type != T64_JSON_OBJECT) {
    finding("P-FIELD", where, "an exposure item must be an object");
    return;
  }
  check_unknown_keys(it, keys, 5u, where);
  if (!require_keys(it, keys, 5u, where)) return;
  for (i = 0; i < 5u; i++) {
    const t64_json_node *v = t64_json_get(it, keys[i]);
    if (is_blank(v)) finding("P-FIELD", where, "exposure key \"%s\" must be a non-blank string", keys[i]);
  }
  if (!is_date(t64_json_get(it, "date")))
    finding("P-FIELD", where, "exposure date must be YYYY-MM-DD");
}

static void check_exposure_list(const t64_json_node *arr, const char *where) {
  const t64_json_node *it;
  if (arr->type != T64_JSON_ARRAY) {
    finding("P-FIELD", where, "exposure must be an array");
    return;
  }
  for (it = arr->first; it != NULL; it = it->next) check_exposure_item(it, where);
}

/* Checks the license text file named by an entry: exactly LICENSES/<spdx>.txt,
 * present, regular, not a symlink, not empty. */
static void check_license_text(const t64_json_node *lt, const t64_json_node *spdx,
                               const char *label) {
  char want[128];
  char full[FULL_MAX];
  const char *why = "";
  struct stat st;
  fs_state s;

  if (!is_str(lt)) {
    finding("P-FIELD", label, "license_text must be a string");
    return;
  }
  lic_add(lt->str);
  if (!is_str(spdx) || !spdx_safe(spdx->str) || strlen(spdx->str) > 100u) {
    /* The id is already reported; there is no safe path to build from it. */
    return;
  }
  (void)snprintf(want, sizeof want, "LICENSES/%s.txt", spdx->str);
  if (strcmp(lt->str, want) != 0) {
    finding("P-LICENSE-TEXT", label, "license_text must be exactly %s", want);
    return;
  }
  s = fs_check(want, full, sizeof full, &why);
  if (s == FS_ABSENT) {
    finding("P-LICENSE-TEXT", label, "%s does not exist", want);
  } else if (s == FS_BAD) {
    finding("P-LICENSE-TEXT", label, "%s: %s", want, why);
  } else if (stat(full, &st) != 0 || st.st_size <= 0) {
    finding("P-LICENSE-TEXT", label, "%s is empty", want);
  }
}

static void check_paths(const t64_json_node *e, size_t idx, const char *label,
                        const t64_json_node *spdx) {
  const t64_json_node *p = t64_json_get(e, "paths");
  const t64_json_node *it;
  char full[FULL_MAX];
  static char head[HEAD_BYTES];

  if (p == NULL) return;
  if (p->type != T64_JSON_ARRAY || p->count == 0u) {
    finding("P-FIELD", label, "paths must be an array with at least one path");
    return;
  }
  for (it = p->first; it != NULL; it = it->next) {
    const char *why = "";
    const char *lex;
    size_t prior;
    fs_state s;

    if (!is_str(it)) {
      finding("P-FIELD", label, "every item of paths must be a string");
      continue;
    }
    lex = path_lexical(it->str, it->len);
    if (lex != NULL) {
      finding("P-PATH", label, "%s: %s", it->str, lex);
      continue;
    }
    prior = rec_find(it->str);
    if (prior != (size_t)-1) {
      finding("P-DUP-PATH", it->str, "recorded by entry %zu and entry %zu", g_rec[prior].entry, idx);
      continue;
    }
    rec_add(it->str, idx);
    s = fs_check(it->str, full, sizeof full, &why);
    if (s == FS_ABSENT) {
      finding("P-PATH-MISSING", label, "%s does not exist", it->str);
    } else if (s == FS_BAD) {
      finding("P-PATH", label, "%s: %s", it->str, why);
    } else if (is_str(spdx)) {
      hdr_t h;
      size_t n = 0;
      if (read_head(full, head, &n) != 0) {
        finding("P-HEADER", it->str, "cannot read the file header");
      } else {
        scan_header(head, n, spdx->str, &h);
        if (!h.want_match)
          finding("P-HEADER", it->str,
                  "the first %u lines carry no license tag equal to the record's id \"%s\"",
                  HEAD_LINES, spdx->str);
      }
    }
  }
}

static void check_upstream(const t64_json_node *up, const char *label) {
  static const char *const keys[] = {"repo", "path", "revision"};
  if (up == NULL) return;
  if (up->type != T64_JSON_OBJECT) {
    finding("P-FIELD", label, "upstream must be an object");
    return;
  }
  check_unknown_keys(up, keys, 3u, label);
  if (is_blank(t64_json_get(up, "repo")))
    finding("P-FIELD", label, "upstream.repo must be a non-blank string");
  if (is_blank(t64_json_get(up, "path")))
    finding("P-FIELD", label, "upstream.path must be a non-blank string");
  if (!is_hex40(t64_json_get(up, "revision")))
    finding("P-SHA", label, "upstream.revision must be a 40-character lowercase hex commit id");
}

static void check_entry(const t64_json_node *e, size_t idx) {
  static const char *const allowed[] = {"id",     "paths",           "upstream",
                                        "retrieved", "spdx",         "license_text",
                                        "notice", "mode",            "modifications",
                                        "verification_test", "maintenance_owner",
                                        "upstream_status", "exposure"};
  static const char *const required[] = {"id",     "paths",  "upstream",         "retrieved",
                                         "spdx",   "license_text", "notice",     "mode",
                                         "verification_test", "upstream_status", "exposure"};
  static const char *const statuses[] = {"active", "archived", "unknown"};
  const t64_json_node *id_n = t64_json_get(e, "id");
  const t64_json_node *spdx = t64_json_get(e, "spdx");
  const t64_json_node *mode = t64_json_get(e, "mode");
  const t64_json_node *mods = t64_json_get(e, "modifications");
  const t64_json_node *owner = t64_json_get(e, "maintenance_owner");
  const t64_json_node *status = t64_json_get(e, "upstream_status");
  const t64_json_node *n;
  char label[96];

  if (is_str(id_n) && id_n->len > 0u && id_n->len < 64u)
    (void)snprintf(label, sizeof label, "%s", id_n->str);
  else
    (void)snprintf(label, sizeof label, "copies[%zu]", idx);

  check_unknown_keys(e, allowed, sizeof allowed / sizeof allowed[0], label);
  (void)require_keys(e, required, sizeof required / sizeof required[0], label);

  if (id_n != NULL) {
    if (!is_str(id_n) || id_n->len == 0u || id_n->len >= 64u) {
      finding("P-FIELD", label, "id must be a string of 1 to 63 characters");
    } else if (id_seen(id_n->str)) {
      finding("P-FIELD", label, "duplicate id");
    } else {
      id_add(id_n->str);
    }
  }

  check_upstream(t64_json_get(e, "upstream"), label);

  n = t64_json_get(e, "retrieved");
  if (n != NULL && !is_date(n)) finding("P-FIELD", label, "retrieved must be YYYY-MM-DD");

  if (spdx != NULL) {
    if (!is_str(spdx)) {
      finding("P-FIELD", label, "spdx must be a string");
    } else if (!key_in(spdx->str, ALLOWED_SPDX, sizeof ALLOWED_SPDX / sizeof ALLOWED_SPDX[0])) {
      finding("P-SPDX-NOT-ALLOWED", label,
              "license \"%s\" is not on the allowlist (ISC, MIT, BSD-3-Clause, Unlicense)",
              spdx->str);
    }
  }

  n = t64_json_get(e, "license_text");
  if (n != NULL) check_license_text(n, spdx, label);

  n = t64_json_get(e, "notice");
  if (n != NULL && is_blank(n)) finding("P-FIELD", label, "notice must be the verbatim upstream notice");

  if (mode != NULL && !str_eq(mode, "verbatim") && !str_eq(mode, "adapted"))
    finding("P-FIELD", label, "mode must be \"verbatim\" or \"adapted\"");
  if (mods != NULL && !is_str(mods)) finding("P-FIELD", label, "modifications must be a string");
  if (str_eq(mode, "adapted")) {
    if (is_blank(mods)) finding("P-MODS-MISSING", label, "an adapted copy must say what was modified");
  } else if (mods == NULL && mode != NULL) {
    finding("P-FIELD", label, "missing required key \"modifications\"");
  }

  n = t64_json_get(e, "verification_test");
  if (n != NULL && is_blank(n)) finding("P-FIELD", label, "verification_test must be a non-blank string");

  if (owner == NULL || is_blank(owner)) {
    finding("P-OWNER-MISSING", label, "maintenance_owner is missing or blank");
  } else if (strchr(owner->str, '@') != NULL) {
    finding("P-OWNER-EMAIL", label, "maintenance_owner must be a GitHub login or a role, never an email");
  } else if (!owner_shape_ok(owner->str)) {
    finding("P-FIELD", label, "maintenance_owner must be a login or a role in letters, digits, space, '.', '_' and '-'");
  }

  if (status != NULL) {
    if (!is_str(status) ||
        !key_in(status->str, statuses, sizeof statuses / sizeof statuses[0])) {
      finding("P-FIELD", label, "upstream_status must be active, archived or unknown");
    } else if (strcmp(status->str, "archived") == 0 && is_str(owner) &&
               strcmp(owner->str, OWN_OWNER) != 0) {
      finding("P-ARCHIVED-OWNER", label,
              "the upstream is archived, so maintenance_owner must be %s", OWN_OWNER);
    }
  }

  n = t64_json_get(e, "exposure");
  if (n != NULL) check_exposure_list(n, label);

  check_paths(e, idx, label, spdx);
}

static void check_records(const t64_json_node *root) {
  static const char *const keys[] = {"schema", "schema_version", "copies", "exposure"};
  const t64_json_node *copies;
  const t64_json_node *n;
  size_t idx = 0;

  if (root->type != T64_JSON_OBJECT) {
    finding("P-FIELD", "/", "the document must be an object");
    return;
  }
  check_unknown_keys(root, keys, 4u, "/");
  (void)require_keys(root, keys, 4u, "/");
  if (t64_json_get(root, "schema") != NULL && !str_eq(t64_json_get(root, "schema"), "t64.copies"))
    finding("P-FIELD", "/", "schema must be \"t64.copies\"");
  n = t64_json_get(root, "schema_version");
  if (n != NULL && !(n->type == T64_JSON_INT && n->num == 1))
    finding("P-FIELD", "/", "schema_version must be 1");
  n = t64_json_get(root, "exposure");
  if (n != NULL) check_exposure_list(n, "exposure");

  copies = t64_json_get(root, "copies");
  if (copies == NULL) return;
  if (copies->type != T64_JSON_ARRAY) {
    finding("P-FIELD", "/", "copies must be an array");
    return;
  }
  for (n = copies->first; n != NULL; n = n->next, idx++) {
    g_entries++;
    if (n->type != T64_JSON_OBJECT) {
      char lab[48];
      (void)snprintf(lab, sizeof lab, "copies[%zu]", idx);
      finding("P-FIELD", lab, "an entry must be an object");
      continue;
    }
    check_entry(n, idx);
  }
}

/* ---- the scanned tree --------------------------------------------------- */

static char *read_all(const char *path, size_t cap, size_t *len) {
  FILE *f = fopen(path, "rb");
  char *buf;
  size_t n;
  if (f == NULL) return NULL;
  buf = (char *)malloc(cap + 2u);
  if (buf == NULL) die("out of memory");
  n = fread(buf, 1u, cap + 1u, f);
  if (ferror(f)) {
    (void)fclose(f);
    free(buf);
    return NULL;
  }
  (void)fclose(f);
  if (n > cap) {
    free(buf);
    (void)fprintf(stderr, "provenance-check: error: %s is larger than %zu bytes\n", path, cap);
    exit(EXIT_HARNESS);
  }
  buf[n] = '\0';
  *len = n;
  return buf;
}

static void scan_one(const char *rel) {
  char full[FULL_MAX];
  static char head[HEAD_BYTES];
  const char *why = "";
  const char *lex = path_lexical(rel, strlen(rel));
  fs_state s;
  int looking = 0;
  int third = 0;
  hdr_t h;
  size_t n = 0;

  if (lex != NULL) {
    finding("P-PATH", rel[0] != '\0' ? rel : "(empty)", "listed path: %s", lex);
    return;
  }
  third = starts_with(rel, strlen(rel), "third_party/");

  s = fs_check(rel, full, sizeof full, &why);
  if (s == FS_ABSENT) return; /* listed but deleted from the working tree */
  g_files++;

  if (starts_with(rel, strlen(rel), "LICENSES/") && !lic_has(rel))
    finding("P-LICENSES-EXTRA", rel,
            "LICENSES/ holds only the license texts that a record names");

  if (third) {
    looking = 1;
  } else if (s == FS_PRESENT) {
    if (read_head(full, head, &n) == 0) {
      scan_header(head, n, NULL, &h);
      looking = h.other_license || h.foreign_holder;
    }
  }
  if (!looking) return;
  g_looking++;
  if (rec_find(rel) == (size_t)-1)
    finding("P-UNRECORDED-COPY", rel,
            "copied-looking (%s) and not recorded in provenance/copies.json",
            third ? "under third_party/" : "header names another license or holder");
}

int main(int argc, char **argv) {
  const char *copies_path = NULL;
  const char *list_path = NULL;
  char *list;
  size_t list_len = 0;
  char *json;
  size_t json_len = 0;
  t64_json_doc doc;
  t64_json_error jerr;
  int i;
  size_t pos;
  int parsed = 0;

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--root") == 0 && i + 1 < argc) g_root = argv[++i];
    else if (strcmp(argv[i], "--copies") == 0 && i + 1 < argc) copies_path = argv[++i];
    else if (strcmp(argv[i], "--files-from") == 0 && i + 1 < argc) list_path = argv[++i];
    else usage();
  }
  if (copies_path == NULL || list_path == NULL) usage();

  list = read_all(list_path, MAX_LIST_BYTES, &list_len);
  if (list == NULL) die("cannot read the --files-from list");

  json = read_all(copies_path, T64_JSON_DEFAULT_MAX_INPUT, &json_len);
  if (json == NULL) {
    finding("P-JSON", copies_path, "cannot read the records file: %s", strerror(errno));
  } else if (t64_json_parse((const uint8_t *)json, json_len, NULL, &doc, &jerr) != 0) {
    finding("P-JSON", copies_path, "not valid strict JSON: %s at byte %zu",
            t64_json_category_name(jerr.category), jerr.offset);
  } else {
    parsed = 1;
    check_records(doc.root);
  }
  if (!parsed) {
    /* Records that do not parse cannot say which files are recorded; a tree
     * scan against them would only add noise. Report the counts and stop. */
    (void)printf("provenance: files=0 copied_looking=0 entries=0 errors=%ld\n", g_errors);
    return EXIT_FINDINGS;
  }

  for (pos = 0; pos < list_len;) {
    size_t end = pos;
    while (end < list_len && list[end] != '\0') end++;
    list[end] = '\0';
    if (end > pos || end < list_len) scan_one(list + pos);
    pos = end + 1u;
  }
  if (g_files == 0u)
    finding("P-ZERO-SCANNED", "files", "no files were scanned (a check over zero files proves nothing)");

  (void)printf("provenance: files=%zu copied_looking=%zu entries=%zu errors=%ld\n", g_files,
               g_looking, g_entries, g_errors);
  if (g_errors == 0) {
    (void)printf("ok %s\n", copies_path);
    return EXIT_CLEAN;
  }
  return EXIT_FINDINGS;
}
