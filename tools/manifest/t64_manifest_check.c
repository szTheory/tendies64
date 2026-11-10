/*
 * t64-manifest-check: validates fixtures/manifest.json (GOV-03), authored here.
 *
 *   t64-manifest-check [--schema PATH] [--require-fixtures]
 *                      [--emit-allowlist FILE] [--root DIR] MANIFEST
 *
 * Options come before the file. The schema (default
 * ROOT/fixtures/manifest.schema.json) is applied with the schema-subset
 * validator in t64_schema.c; then, when the manifest is structurally valid,
 * the cross-field rights rules run. ROOT defaults to the current directory;
 * fixture paths in the manifest are relative to it.
 *
 * Output: "RULE <id> <entry-id or pointer>: message" per finding, then
 *   manifest: entries=N permitted=P present=Q skipped=S errors=E
 * and "ok <file>" when clean. Exit 0 clean, 1 findings, 2 harness error (a
 * schema keyword outside the declared subset, an unreadable file, bad usage).
 *
 * Cross-field rules: M-ZERO-ENTRIES, M-DUP-ID, M-FORBIDDEN-DIGEST,
 * M-FORBIDDEN-REASON, M-NULL-DIGEST, M-BYTES-NOT-PERMITTED, M-PATH,
 * M-DIGEST-MISMATCH, M-FIXTURE-ABSENT (only with --require-fixtures),
 * M-HW-PROVENANCE, M-TOOLCHAIN-PIN, M-ID-FORMAT, M-DIGEST-FORMAT.
 *
 * The cross-field rules re-check in code every shape they rely on (string
 * types, the id and sha256 formats), because --schema PATH accepts any
 * subset-valid schema, including one that constrains nothing (WR-07). In
 * particular no value reaches an allowlist row unless it is free of TAB, LF
 * and every other control byte.
 *
 * --emit-allowlist FILE writes "path<TAB>sha256<TAB>manifest:<id>" rows for
 * entries whose binary_rights is authored-here or cleared, that name a path
 * and a sha256 and whose distribution includes commit. The file is written
 * only when the manifest is clean (otherwise it is left empty), so a manifest
 * with any finding can never widen the rights scan.
 */
#include <errno.h>
#include <fcntl.h>
#include <regex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "t64_json.h"
#include "t64_schema.h"
#include "t64_sha256.h"

enum { EXIT_CLEAN = 0, EXIT_FINDINGS = 1, EXIT_HARNESS = 2 };

#define MAX_FILE_BYTES T64_JSON_DEFAULT_MAX_INPUT
#define MAX_NODES 65536u
#define FULL_PATH_MAX 4096u

static long g_errors;
static int g_require_fixtures;
static const char *g_root = ".";
static size_t g_entries, g_permitted, g_present, g_skipped;

static void die(const char *msg) {
  (void)fprintf(stderr, "manifest-check: error: %s\n", msg);
  exit(EXIT_HARNESS);
}

static void usage(void) {
  (void)fprintf(stderr,
                "usage: t64-manifest-check [--schema PATH] [--require-fixtures] "
                "[--emit-allowlist FILE] [--root DIR] MANIFEST\n");
  exit(EXIT_HARNESS);
}

static void finding(const char *rule, const char *where, const char *msg) {
  char w[T64_PTR_MAX + 8];
  t64_sanitize(where, strlen(where), w, sizeof w);
  (void)printf("RULE %s %s: %s\n", rule, w, msg);
  g_errors++;
}

static void emit_schema_finding(void *ctx, const char *rule, const char *pointer,
                                const char *message) {
  (void)ctx;
  finding(rule, pointer[0] == '\0' ? "/" : pointer, message);
}

/* Reads a whole file of at most MAX_FILE_BYTES. Returns 0 or 1 (too large or
 * unreadable, with why filled). The buffer has one spare byte. */
static int read_file(const char *path, uint8_t **out, size_t *out_len, char *why,
                     size_t why_cap) {
  FILE *f = fopen(path, "rb");
  uint8_t *buf;
  size_t n;

  if (f == NULL) {
    (void)snprintf(why, why_cap, "cannot open: %s", strerror(errno));
    return 1;
  }
  buf = (uint8_t *)malloc((size_t)MAX_FILE_BYTES + 2u);
  if (buf == NULL) {
    (void)fclose(f);
    die("out of memory");
  }
  n = fread(buf, 1u, (size_t)MAX_FILE_BYTES + 1u, f);
  if (ferror(f)) {
    (void)snprintf(why, why_cap, "read error");
    (void)fclose(f);
    free(buf);
    return 1;
  }
  (void)fclose(f);
  *out = buf;
  *out_len = n;
  return 0;
}

static int str_is(const t64_json_node *n, const char *lit) {
  size_t l = strlen(lit);
  return n != NULL && n->type == T64_JSON_STRING && n->len == l &&
         memcmp(n->str, lit, l) == 0;
}

static int dist_has(const t64_json_node *dist, const char *channel) {
  const t64_json_node *e;
  if (dist == NULL || dist->type != T64_JSON_ARRAY) return 0;
  for (e = dist->first; e != NULL; e = e->next) {
    if (str_is(e, channel)) return 1;
  }
  return 0;
}

static int blank(const t64_json_node *n) {
  size_t i;
  if (n == NULL || n->type != T64_JSON_STRING) return 1;
  for (i = 0; i < n->len; i++) {
    unsigned char c = (unsigned char)n->str[i];
    if (c != ' ' && c != '\t' && c != '\n' && c != '\r') return 0;
  }
  return 1;
}

/* Lexical rule for a fixture path, applied before any filesystem access. */
static int path_lexical(const t64_json_node *p, const char **why) {
  size_t i;
  size_t seg_start = 0;

  if (p->len == 0u) {
    *why = "path is empty";
    return 1;
  }
  if (strlen(p->str) != p->len) {
    *why = "path contains a NUL byte";
    return 1;
  }
  if (p->str[0] == '/') {
    *why = "path is absolute";
    return 1;
  }
  for (i = 0; i < p->len; i++) {
    unsigned char c = (unsigned char)p->str[i];
    if (c <= 0x20u || c >= 0x7fu || c == '\\') {
      *why = "path has a space, control, non-ASCII or backslash byte";
      return 1;
    }
  }
  for (i = 0; i <= p->len; i++) {
    if (i == p->len || p->str[i] == '/') {
      size_t sl = i - seg_start;
      if (sl == 0u) {
        *why = "path has an empty segment";
        return 1;
      }
      if (sl == 1u && p->str[seg_start] == '.') {
        *why = "path has a '.' segment";
        return 1;
      }
      if (sl == 2u && p->str[seg_start] == '.' && p->str[seg_start + 1u] == '.') {
        *why = "path has a '..' segment";
        return 1;
      }
      seg_start = i + 1u;
    }
  }
  if (p->len <= 9u || memcmp(p->str, "fixtures/", 9u) != 0) {
    *why = "path is not under fixtures/";
    return 1;
  }
  return 0;
}

typedef enum { FS_ABSENT, FS_PRESENT, FS_BAD } fs_state;

/* lstat every component below the root so no link or special file is
 * followed; the last component must be a regular file. */
static fs_state fs_check(const char *rel, char *full, size_t full_cap,
                         const char **why) {
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

/* Streams the file through SHA-256. Returns 0 on success. */
static int hash_file(const char *full, char hex[T64_SHA256_HEX_SIZE],
                     uint64_t *size, const char **why) {
  static unsigned char buf[65536];
  t64_sha256_ctx ctx;
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  struct stat st;
  int fd = open(full, O_RDONLY | O_NOFOLLOW);
  uint64_t total = 0;

  if (fd < 0) {
    *why = "cannot open the file";
    return 1;
  }
  if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
    (void)close(fd);
    *why = "path is not a regular file";
    return 1;
  }
  t64_sha256_init(&ctx);
  for (;;) {
    ssize_t n = read(fd, buf, sizeof buf);
    if (n < 0) {
      if (errno == EINTR) continue;
      (void)close(fd);
      *why = "read error";
      return 1;
    }
    if (n == 0) break;
    t64_sha256_update(&ctx, buf, (size_t)n);
    total += (uint64_t)n;
  }
  (void)close(fd);
  t64_sha256_final(&ctx, digest);
  t64_sha256_hex(digest, hex);
  *size = total;
  return 0;
}

/* The id rule of the pinned schema: ^[a-z0-9][a-z0-9-]*$, at most 64 bytes. */
static int id_format_ok(const t64_json_node *id) {
  size_t i;
  if (id == NULL || id->type != T64_JSON_STRING || id->len == 0u || id->len > 64u)
    return 0;
  for (i = 0; i < id->len; i++) {
    unsigned char c = (unsigned char)id->str[i];
    int ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (i > 0u && c == '-');
    if (!ok) return 0;
  }
  return 1;
}

/* Exactly 64 lowercase hex characters (so no TAB, LF or NUL either). */
static int sha_format_ok(const t64_json_node *sha) {
  size_t i;
  if (sha == NULL || sha->type != T64_JSON_STRING || sha->len != 64u) return 0;
  for (i = 0; i < 64u; i++) {
    char c = sha->str[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return 0;
  }
  return 1;
}

static int toolchain_pinned(const t64_json_node *item, regex_t *re) {
  if (item->type != T64_JSON_STRING) return 0;
  if (strlen(item->str) != item->len) return 0;
  if (regexec(re, item->str, 0, NULL, 0) != 0) return 0;
  if (item->len >= 2u && item->str[item->len - 2u] == '.' &&
      (item->str[item->len - 1u] == 'x' || item->str[item->len - 1u] == 'X'))
    return 0;
  return 1;
}

typedef struct {
  const char *path;
  char sha[T64_SHA256_HEX_SIZE];
  char id[96];
} allow_row;

static allow_row *g_rows;
static size_t g_rows_n;

static void add_row(const t64_json_node *path, const t64_json_node *sha,
                    const char *id) {
  allow_row *nr = (allow_row *)realloc(g_rows, (g_rows_n + 1u) * sizeof *nr);
  if (nr == NULL) die("out of memory");
  g_rows = nr;
  g_rows[g_rows_n].path = path->str;
  memcpy(g_rows[g_rows_n].sha, sha->str, 64u);
  g_rows[g_rows_n].sha[64] = '\0';
  (void)snprintf(g_rows[g_rows_n].id, sizeof g_rows[g_rows_n].id, "%s", id);
  g_rows_n++;
}

static void check_entry(const t64_json_node *e, regex_t *pin_re) {
  const t64_json_node *id_n = t64_json_get(e, "id");
  const t64_json_node *sr = t64_json_get(e, "source_rights");
  const t64_json_node *br = t64_json_get(e, "binary_rights");
  const t64_json_node *dist = t64_json_get(e, "distribution");
  const t64_json_node *sha = t64_json_get(e, "sha256");
  const t64_json_node *size = t64_json_get(e, "size");
  const t64_json_node *path = t64_json_get(e, "path");
  char id[96];
  char msg[256];

  t64_sanitize(id_n->str, id_n->len, id, sizeof id);
  if (!id_format_ok(id_n)) {
    finding("M-ID-FORMAT", id,
            "id must match ^[a-z0-9][a-z0-9-]*$ and be at most 64 bytes");
  }

  if (str_is(sr, "forbidden") || str_is(br, "forbidden")) {
    if (blank(t64_json_get(e, "forbidden_reason"))) {
      finding("M-FORBIDDEN-REASON", id,
              "an entry with a forbidden right needs a non-blank forbidden_reason");
    }
  }

  if (str_is(br, "forbidden")) {
    static const char *const banned[] = {"sha256", "size",     "source",
                                         "build",  "expected", "path"};
    size_t i;
    for (i = 0; i < sizeof banned / sizeof banned[0]; i++) {
      if (t64_json_get(e, banned[i]) != NULL) {
        (void)snprintf(msg, sizeof msg,
                       "a forbidden entry must not carry %s (no digest or source "
                       "of protected material)",
                       banned[i]);
        finding("M-FORBIDDEN-DIGEST", id, msg);
      }
    }
    return;
  }

  {
    int pending = str_is(br, "pending");
    int permitted_rights = str_is(br, "authored-here") || str_is(br, "cleared");
    int has_commit = dist_has(dist, "commit");
    int has_artifact = dist_has(dist, "artifact");
    int sha_ok = sha != NULL && sha->type == T64_JSON_STRING;

    if (sha_ok && !sha_format_ok(sha)) {
      finding("M-DIGEST-FORMAT", id, "sha256 must be 64 lowercase hex characters");
      sha_ok = 0;
    }

    if (!pending && (!sha_ok || size == NULL || size->type != T64_JSON_INT)) {
      finding("M-NULL-DIGEST", id,
              "sha256 and size may be null only while binary_rights is pending");
    }
    if ((has_commit || has_artifact) && !permitted_rights) {
      finding("M-BYTES-NOT-PERMITTED", id,
              "commit or artifact distribution needs binary_rights authored-here "
              "or cleared");
    } else if (path != NULL && !(permitted_rights && has_commit)) {
      finding("M-BYTES-NOT-PERMITTED", id,
              "a committed path needs binary_rights authored-here or cleared and "
              "commit in distribution");
    }

    {
      const t64_json_node *exp = t64_json_get(e, "expected");
      if (str_is(t64_json_get(exp, "kind"), "hardware-derived") &&
          t64_json_get(e, "capture_provenance") == NULL) {
        finding("M-HW-PROVENANCE", id,
                "a hardware-derived expectation needs capture_provenance");
      }
    }

    {
      const t64_json_node *tc = t64_json_get(t64_json_get(e, "build"), "toolchain");
      const t64_json_node *it;
      size_t idx = 0;
      if (tc != NULL && tc->type != T64_JSON_ARRAY) {
        finding("M-TOOLCHAIN-PIN", id, "build.toolchain is not an array");
      } else if (tc != NULL) {
        for (it = tc->first; it != NULL; it = it->next, idx++) {
          if (it->type != T64_JSON_STRING) {
            (void)snprintf(msg, sizeof msg, "toolchain item %zu is not a string", idx);
            finding("M-TOOLCHAIN-PIN", id, msg);
          } else if (!toolchain_pinned(it, pin_re)) {
            char shown[64];
            t64_sanitize(it->str, it->len, shown, sizeof shown);
            (void)snprintf(msg, sizeof msg,
                           "toolchain item %zu \"%s\" is not pinned to an exact "
                           "version",
                           idx, shown);
            finding("M-TOOLCHAIN-PIN", id, msg);
          }
        }
      }
    }

    if (path != NULL) {
      const char *why = NULL;
      if (path->type != T64_JSON_STRING) {
        finding("M-PATH", id, "path is not a string");
        return;
      }
      if (path_lexical(path, &why)) {
        finding("M-PATH", id, why);
        return;
      }
    } else if (permitted_rights && has_commit) {
      finding("M-PATH", id, "a committed fixture needs a path");
      return;
    }

    if (permitted_rights && has_commit && path != NULL && sha_ok) {
      char full[FULL_PATH_MAX];
      const char *why = NULL;
      fs_state st;

      g_permitted++;
      if (permitted_rights) add_row(path, sha, id);
      st = fs_check(path->str, full, sizeof full, &why);
      if (st == FS_BAD) {
        finding("M-PATH", id, why);
      } else if (st == FS_ABSENT) {
        if (g_require_fixtures) {
          finding("M-FIXTURE-ABSENT", id,
                  "permitted fixture bytes are absent and --require-fixtures is set");
        } else {
          g_skipped++;
          (void)printf("skipped %s: bytes absent\n", id);
        }
      } else {
        char hex[T64_SHA256_HEX_SIZE];
        uint64_t fsize = 0;
        if (hash_file(full, hex, &fsize, &why)) {
          finding("M-PATH", id, why);
        } else {
          g_present++;
          if (strcmp(hex, sha->str) != 0) {
            finding("M-DIGEST-MISMATCH", id, "sha256 does not match the file");
          } else if (size != NULL && size->type == T64_JSON_INT &&
                     (uint64_t)size->num != fsize) {
            finding("M-DIGEST-MISMATCH", id, "size does not match the file");
          }
        }
      }
    }
  }
}

static void check_cross_field(const t64_json_node *root) {
  const t64_json_node *entries = t64_json_get(root, "entries");
  const t64_json_node *e;
  const t64_json_node *prev;
  regex_t pin_re;
  int rc;

  if (entries == NULL || entries->type != T64_JSON_ARRAY) return;
  g_entries = entries->count;
  if (entries->count == 0u) {
    finding("M-ZERO-ENTRIES", "/entries",
            "the manifest lists no entries (a manifest that checks nothing is not "
            "a pass)");
    return;
  }
  rc = regcomp(&pin_re,
               "^[A-Za-z0-9][A-Za-z0-9._+ -]*[ @=]([0-9][A-Za-z0-9._+-]*|nightly-"
               "[0-9]{4}-[0-9]{2}-[0-9]{2})$",
               REG_EXTENDED | REG_NOSUB);
  if (rc != 0) die("toolchain pin pattern does not compile");

  for (e = entries->first; e != NULL; e = e->next) {
    const t64_json_node *id = t64_json_get(e, "id");
    int dup = 0;
    if (e->type != T64_JSON_OBJECT || id == NULL || id->type != T64_JSON_STRING)
      continue;
    for (prev = entries->first; prev != e; prev = prev->next) {
      const t64_json_node *pid = t64_json_get(prev, "id");
      if (pid != NULL && pid->type == T64_JSON_STRING && pid->len == id->len &&
          memcmp(pid->str, id->str, id->len) == 0) {
        dup = 1;
        break;
      }
    }
    if (dup) {
      char sid[96];
      t64_sanitize(id->str, id->len, sid, sizeof sid);
      finding("M-DUP-ID", sid, "id is used by an earlier entry");
    }
    check_entry(e, &pin_re);
  }
  regfree(&pin_re);
}

static void write_allowlist(const char *path) {
  FILE *f = fopen(path, "w");
  size_t i;

  if (f == NULL) die("cannot write the allowlist file");
  if (g_errors == 0) {
    for (i = 0; i < g_rows_n; i++) {
      (void)fprintf(f, "%s\t%s\tmanifest:%s\n", g_rows[i].path, g_rows[i].sha,
                    g_rows[i].id);
    }
  }
  if (fclose(f) != 0) die("cannot write the allowlist file");
}

int main(int argc, char **argv) {
  const char *schema_path = NULL;
  const char *manifest_path = NULL;
  const char *allow_path = NULL;
  char default_schema[FULL_PATH_MAX];
  uint8_t *sbuf = NULL;
  uint8_t *mbuf = NULL;
  size_t slen = 0;
  size_t mlen = 0;
  char why[256];
  char where[T64_PTR_MAX];
  t64_json_limits lim;
  t64_json_doc sdoc;
  t64_json_doc mdoc;
  t64_json_error jerr;
  long found;
  int i;

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--schema") == 0 && i + 1 < argc) {
      schema_path = argv[++i];
    } else if (strcmp(argv[i], "--require-fixtures") == 0) {
      g_require_fixtures = 1;
    } else if (strcmp(argv[i], "--emit-allowlist") == 0 && i + 1 < argc) {
      allow_path = argv[++i];
    } else if (strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
      g_root = argv[++i];
    } else if (argv[i][0] == '-' && argv[i][1] != '\0') {
      usage();
    } else if (manifest_path == NULL) {
      manifest_path = argv[i];
    } else {
      usage();
    }
  }
  if (manifest_path == NULL) usage();
  if (g_root[0] == '\0') g_root = ".";
  if (schema_path == NULL) {
    if (strlen(g_root) + 32u > sizeof default_schema) die("root path is too long");
    (void)snprintf(default_schema, sizeof default_schema,
                   "%s/fixtures/manifest.schema.json", g_root);
    schema_path = default_schema;
  }

  t64_json_limits_default(&lim);
  lim.max_nodes = MAX_NODES;

  /* The schema first: a problem here is a harness error, never a finding. */
  if (read_file(schema_path, &sbuf, &slen, why, sizeof why)) {
    (void)fprintf(stderr, "manifest-check: error: schema: %s\n", why);
    return EXIT_HARNESS;
  }
  if (t64_json_parse(sbuf, slen, &lim, &sdoc, &jerr)) {
    (void)printf("RULE M-JSON schema: JSON-%s offset=%zu\n",
                 t64_json_category_name(jerr.category), jerr.offset);
    return EXIT_HARNESS;
  }
  if (t64_schema_check(sdoc.root, where, sizeof where, why, sizeof why)) {
    char w[T64_PTR_MAX + 8];
    t64_sanitize(where, strlen(where), w, sizeof w);
    (void)printf("RULE M-SCHEMA-KEYWORD schema#%s: %s\n", w, why);
    return EXIT_HARNESS;
  }

  if (read_file(manifest_path, &mbuf, &mlen, why, sizeof why)) {
    (void)fprintf(stderr, "manifest-check: error: manifest: %s\n", why);
    return EXIT_HARNESS;
  }
  if (t64_json_parse(mbuf, mlen, &lim, &mdoc, &jerr)) {
    char shown[160];
    char msg[96];
    t64_sanitize(manifest_path, strlen(manifest_path), shown, sizeof shown);
    (void)snprintf(msg, sizeof msg, "JSON-%s offset=%zu",
                   t64_json_category_name(jerr.category), jerr.offset);
    finding("M-JSON", shown, msg);
    (void)printf("manifest: entries=0 permitted=0 present=0 skipped=0 errors=%ld\n",
                 g_errors);
    if (allow_path != NULL) write_allowlist(allow_path);
    return EXIT_FINDINGS;
  }

  found = t64_schema_validate(sdoc.root, mdoc.root, emit_schema_finding, NULL, why,
                              sizeof why);
  if (found < 0) {
    (void)fprintf(stderr, "manifest-check: error: schema: %s\n", why);
    return EXIT_HARNESS;
  }
  if (found == 0) {
    check_cross_field(mdoc.root);
  } else {
    const t64_json_node *entries = t64_json_get(mdoc.root, "entries");
    if (entries != NULL && entries->type == T64_JSON_ARRAY) g_entries = entries->count;
  }

  (void)printf("manifest: entries=%zu permitted=%zu present=%zu skipped=%zu errors=%ld\n",
               g_entries, g_permitted, g_present, g_skipped, g_errors);
  if (allow_path != NULL) write_allowlist(allow_path);
  if (g_errors == 0) (void)printf("ok %s\n", manifest_path);

  t64_json_free(&mdoc);
  t64_json_free(&sdoc);
  free(sbuf);
  free(mbuf);
  free(g_rows);
  return g_errors == 0 ? EXIT_CLEAN : EXIT_FINDINGS;
}
