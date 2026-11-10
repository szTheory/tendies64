/*
 * t64-blobscan: byte classifier for the GOV-07 rights scan (authored here).
 *
 *   t64-blobscan --git-batch NAMES [--allow TSV]...   (stdin: git cat-file --batch)
 *   t64-blobscan --files [--allow TSV]...             (stdin: NUL-separated paths)
 *
 * NAMES holds "<oid> <path>" lines (git rev-list --objects output, extended with
 * every other name a blob had in history). Every name of a shared blob is
 * reported. Content is streamed through a constant-size buffer and is never
 * printed: a finding is "RULE <id> <path> [oid]", an advisory is
 * "ADVISORY <id> <path>", and the last line is
 *   blobscan: scanned=N findings=M advisories=K
 * Exit 0 clean, 1 findings, 2 harness error (malformed stream, unreadable
 * input, or nothing scanned).
 *
 * ROM magic at offset 0, in any byte order, is a rule regardless of extension:
 * 80371240 (z64), 37804012 (v64) and 40123780 (n64). These are format facts,
 * not protected content, so they may appear here.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "t64_sha256.h"

#define BUFSZ 65536u
#define HEAD_KEEP 512u
#define MAX_PATH_BYTES (1u << 20)
#define MAX_TABLE_BYTES (256u * 1024u * 1024u)

enum { EXIT_CLEAN = 0, EXIT_FINDINGS = 1, EXIT_HARNESS = 2 };

/* Content classes (bit flags). */
#define C_ROM 1u
#define CONTENT_MASK (C_ROM)

typedef struct {
  char *path;
  char sha[T64_SHA256_HEX_SIZE];
} allow_row;

typedef struct {
  const char *oid;
  const char *path;
} name_row;

static allow_row *g_allow;
static size_t g_allow_n, g_allow_cap;
static name_row *g_names;
static size_t g_names_n;

static uint64_t g_scanned, g_findings, g_advisories;

static void die(const char *msg) {
  (void)fprintf(stderr, "blobscan: error: %s\n", msg);
  exit(EXIT_HARNESS);
}

/* Prints a path with control bytes replaced, so logs cannot carry escapes. */
static void print_path(const char *p) {
  for (; *p != '\0'; p++) {
    unsigned char c = (unsigned char)*p;
    (void)putchar((c < 0x20u || c == 0x7fu) ? '?' : (int)c);
  }
}

static void emit(const char *kind, const char *id, const char *path,
                 const char *oid) {
  (void)printf("%s %s ", kind, id);
  print_path(path);
  if (oid != NULL) (void)printf(" %s", oid);
  (void)putchar('\n');
  if (kind[0] == 'R') {
    g_findings++;
  } else {
    g_advisories++;
  }
}

static int is_hex_lower(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

static int valid_oid(const char *s, size_t n) {
  size_t i;
  if (n != 40u && n != 64u) return 0;
  for (i = 0; i < n; i++) {
    if (!is_hex_lower(s[i])) return 0;
  }
  return 1;
}

/* Reads a whole file into a NUL-terminated heap buffer, bounded. */
static char *slurp(const char *path, size_t *outlen) {
  FILE *f = fopen(path, "rb");
  char *buf;
  size_t n = 0, cap = 4096;
  if (f == NULL) die("cannot open input file");
  buf = (char *)malloc(cap);
  if (buf == NULL) die("out of memory");
  for (;;) {
    size_t got;
    if (n + 1 >= cap) {
      char *nb;
      if (cap >= MAX_TABLE_BYTES) die("input file too large");
      cap *= 2;
      nb = (char *)realloc(buf, cap);
      if (nb == NULL) die("out of memory");
      buf = nb;
    }
    got = fread(buf + n, 1, cap - n - 1, f);
    if (got == 0) break;
    n += got;
  }
  if (ferror(f)) die("read error on input file");
  (void)fclose(f);
  buf[n] = '\0';
  *outlen = n;
  return buf;
}

static void load_allow(const char *path) {
  size_t len, pos = 0;
  unsigned long lineno = 0;
  char *buf = slurp(path, &len);
  while (pos < len) {
    char *line = buf + pos;
    char *nl = memchr(line, '\n', len - pos);
    char *c1, *c2;
    size_t ll;
    if (nl != NULL) {
      *nl = '\0';
      ll = (size_t)(nl - line);
      pos += ll + 1u;
    } else {
      ll = strlen(line);
      pos = len;
    }
    lineno++;
    if (ll == 0 || line[0] == '#') continue;
    c1 = strchr(line, '\t');
    c2 = (c1 != NULL) ? strchr(c1 + 1, '\t') : NULL;
    if (c1 == NULL || c2 == NULL || c1 == line || c2[1] == '\0') {
      (void)fprintf(stderr, "blobscan: error: malformed allowlist row %lu\n",
                    lineno);
      exit(EXIT_HARNESS);
    }
    *c1 = '\0';
    *c2 = '\0';
    if (strlen(c1 + 1) != 64u) {
      (void)fprintf(stderr, "blobscan: error: bad sha256 in allowlist row %lu\n",
                    lineno);
      exit(EXIT_HARNESS);
    }
    {
      size_t i;
      for (i = 0; i < 64u; i++) {
        if (!is_hex_lower(c1[1 + i])) {
          (void)fprintf(stderr,
                        "blobscan: error: bad sha256 in allowlist row %lu\n",
                        lineno);
          exit(EXIT_HARNESS);
        }
      }
    }
    if (g_allow_n == g_allow_cap) {
      size_t nc = g_allow_cap == 0 ? 16u : g_allow_cap * 2u;
      allow_row *na = (allow_row *)realloc(g_allow, nc * sizeof *na);
      if (na == NULL) die("out of memory");
      g_allow = na;
      g_allow_cap = nc;
    }
    g_allow[g_allow_n].path = strdup(line);
    if (g_allow[g_allow_n].path == NULL) die("out of memory");
    memcpy(g_allow[g_allow_n].sha, c1 + 1, 64u);
    g_allow[g_allow_n].sha[64] = '\0';
    g_allow_n++;
  }
  free(buf);
}

/* path == NULL matches on the digest alone (history blobs). */
static int allow_match(const char *path, const char *sha) {
  size_t i;
  for (i = 0; i < g_allow_n; i++) {
    if (strcmp(g_allow[i].sha, sha) != 0) continue;
    if (path == NULL || strcmp(g_allow[i].path, path) == 0) return 1;
  }
  return 0;
}

static int name_cmp(const void *a, const void *b) {
  const name_row *x = (const name_row *)a;
  const name_row *y = (const name_row *)b;
  int c = strcmp(x->oid, y->oid);
  if (c != 0) return c;
  return strcmp(x->path, y->path);
}

static void load_names(const char *path) {
  size_t len, pos = 0, cap = 0;
  char *buf = slurp(path, &len);
  if (memchr(buf, '\0', len) != NULL) die("NUL byte in names file");
  while (pos < len) {
    char *line = buf + pos;
    char *nl = memchr(line, '\n', len - pos);
    char *sp;
    size_t ll;
    if (nl != NULL) {
      *nl = '\0';
      ll = (size_t)(nl - line);
      pos += ll + 1u;
    } else {
      ll = strlen(line);
      pos = len;
    }
    if (ll == 0) continue;
    sp = memchr(line, ' ', ll);
    if (sp != NULL) *sp = '\0';
    if (!valid_oid(line, strlen(line))) die("malformed names line");
    if (g_names_n == cap) {
      size_t nc = cap == 0 ? 256u : cap * 2u;
      name_row *nn = (name_row *)realloc(g_names, nc * sizeof *nn);
      if (nn == NULL) die("out of memory");
      g_names = nn;
      cap = nc;
    }
    g_names[g_names_n].oid = line;
    g_names[g_names_n].path = (sp != NULL) ? sp + 1 : "";
    g_names_n++;
  }
  if (g_names_n > 1u) qsort(g_names, g_names_n, sizeof *g_names, name_cmp);
}

/* ------------------------------------------------------------------ scan */

typedef struct {
  t64_sha256_ctx sha;
  uint64_t size;
  unsigned char head[HEAD_KEEP];
  size_t headlen;
} scan_state;

static void scan_init(scan_state *s) {
  t64_sha256_init(&s->sha);
  s->size = 0;
  s->headlen = 0;
}

static void scan_feed(scan_state *s, const unsigned char *b, size_t n) {
  t64_sha256_update(&s->sha, b, n);
  if (s->headlen < HEAD_KEEP) {
    size_t take = HEAD_KEEP - s->headlen;
    if (take > n) take = n;
    memcpy(s->head + s->headlen, b, take);
    s->headlen += take;
  }
  s->size += (uint64_t)n;
}

static int rom_magic(const unsigned char *h, size_t n) {
  if (n < 4u) return 0;
  if (h[0] == 0x80 && h[1] == 0x37 && h[2] == 0x12 && h[3] == 0x40) return 1;
  if (h[0] == 0x37 && h[1] == 0x80 && h[2] == 0x40 && h[3] == 0x12) return 1;
  if (h[0] == 0x40 && h[1] == 0x12 && h[2] == 0x37 && h[3] == 0x80) return 1;
  return 0;
}

static unsigned scan_finish(scan_state *s, char hex[T64_SHA256_HEX_SIZE]) {
  uint8_t d[T64_SHA256_DIGEST_SIZE];
  unsigned f = 0;
  t64_sha256_final(&s->sha, d);
  t64_sha256_hex(d, hex);
  if (rom_magic(s->head, s->headlen)) f |= C_ROM;
  return f;
}

/* Emits the findings of one name. */
static void report_name(const char *path, const char *oid, unsigned flags) {
  if (flags & C_ROM) emit("RULE", "R-ROM-MAGIC", path, oid);
}

/* ------------------------------------------------------------- git batch */

static int read_line(FILE *in, char *line, size_t cap) {
  size_t n = 0;
  int c;
  while ((c = getc(in)) != EOF) {
    if (c == '\n') {
      line[n] = '\0';
      return 1;
    }
    if (n + 1 >= cap) die("batch header too long");
    line[n++] = (char)c;
  }
  if (n == 0) return 0;
  die("truncated batch header");
  return -1;
}

static int parse_size(const char *s, uint64_t *out) {
  uint64_t v = 0;
  size_t i, n = strlen(s);
  if (n == 0 || n > 18u) return 0;
  for (i = 0; i < n; i++) {
    if (s[i] < '0' || s[i] > '9') return 0;
    v = v * 10u + (uint64_t)(s[i] - '0');
  }
  *out = v;
  return 1;
}

static void run_git_batch(void) {
  static unsigned char buf[BUFSZ];
  char line[512];
  while (read_line(stdin, line, sizeof line) == 1) {
    char *sp1 = strchr(line, ' ');
    char *sp2;
    uint64_t size, left;
    scan_state st;
    char hex[T64_SHA256_HEX_SIZE];
    unsigned flags;
    size_t lo, hi, i;
    if (sp1 == NULL) die("malformed batch header");
    *sp1 = '\0';
    sp2 = strchr(sp1 + 1, ' ');
    if (sp2 == NULL) die("malformed batch header (missing object?)");
    *sp2 = '\0';
    if (!valid_oid(line, strlen(line))) die("malformed oid in batch header");
    if (strcmp(sp1 + 1, "blob") != 0) die("non-blob object in batch stream");
    if (!parse_size(sp2 + 1, &size)) die("malformed size in batch header");
    scan_init(&st);
    for (left = size; left > 0;) {
      size_t want = left > BUFSZ ? BUFSZ : (size_t)left;
      size_t got = fread(buf, 1, want, stdin);
      if (got == 0) die("short read in batch content");
      scan_feed(&st, buf, got);
      left -= (uint64_t)got;
    }
    if (getc(stdin) != '\n') die("missing batch record terminator");
    flags = scan_finish(&st, hex);
    g_scanned++;
    if (allow_match(NULL, hex)) flags &= ~(unsigned)CONTENT_MASK;
    /* binary search for the first name of this oid */
    lo = 0;
    hi = g_names_n;
    while (lo < hi) {
      size_t mid = lo + (hi - lo) / 2u;
      if (strcmp(g_names[mid].oid, line) < 0) {
        lo = mid + 1u;
      } else {
        hi = mid;
      }
    }
    if (lo >= g_names_n || strcmp(g_names[lo].oid, line) != 0) {
      report_name("(unnamed)", line, flags);
    } else {
      for (i = lo; i < g_names_n && strcmp(g_names[i].oid, line) == 0; i++) {
        report_name(g_names[i].path, line, flags);
      }
    }
  }
}

/* ----------------------------------------------------------------- files */

static void scan_path(const char *path) {
  static unsigned char buf[BUFSZ];
  struct stat sb;
  scan_state st;
  char hex[T64_SHA256_HEX_SIZE];
  unsigned flags;
  int fd;
  const char *shown = path;

  while (shown[0] == '.' && shown[1] == '/') shown += 2;
  if (lstat(path, &sb) != 0) {
    if (errno == ENOENT) {
      emit("ADVISORY", "A-MISSING", shown, NULL);
      return;
    }
    die("cannot stat input path");
  }
  if (!S_ISREG(sb.st_mode)) die("non-regular file (not supported yet)");
  fd = open(path, O_RDONLY | O_NOFOLLOW);
  if (fd < 0) die("cannot open input path");
  scan_init(&st);
  for (;;) {
    ssize_t got = read(fd, buf, sizeof buf);
    if (got < 0) {
      if (errno == EINTR) continue;
      die("read error on input path");
    }
    if (got == 0) break;
    scan_feed(&st, buf, (size_t)got);
  }
  (void)close(fd);
  flags = scan_finish(&st, hex);
  g_scanned++;
  if (allow_match(shown, hex)) flags &= ~(unsigned)CONTENT_MASK;
  report_name(shown, NULL, flags);
}

static void run_files(void) {
  char *buf = NULL;
  size_t n = 0, cap = 0;
  int c;
  for (;;) {
    c = getc(stdin);
    if (c == EOF || c == '\0') {
      if (n > 0) {
        buf[n] = '\0';
        scan_path(buf);
      }
      n = 0;
      if (c == EOF) break;
      continue;
    }
    if (n + 2u > cap) {
      size_t nc = cap == 0 ? 256u : cap * 2u;
      char *nb;
      if (nc > MAX_PATH_BYTES) die("path too long");
      nb = (char *)realloc(buf, nc);
      if (nb == NULL) die("out of memory");
      buf = nb;
      cap = nc;
    }
    buf[n++] = (char)c;
  }
  if (ferror(stdin)) die("read error on stdin");
  free(buf);
}

int main(int argc, char **argv) {
  const char *names = NULL;
  int files = 0, i;

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--git-batch") == 0 && i + 1 < argc) {
      names = argv[++i];
    } else if (strcmp(argv[i], "--files") == 0) {
      files = 1;
    } else if (strcmp(argv[i], "--allow") == 0 && i + 1 < argc) {
      load_allow(argv[++i]);
    } else {
      die("usage: t64-blobscan (--git-batch NAMES | --files) [--allow TSV]...");
    }
  }
  if ((names == NULL) == (files == 0)) {
    die("usage: t64-blobscan (--git-batch NAMES | --files) [--allow TSV]...");
  }
  if (names != NULL) {
    load_names(names);
    run_git_batch();
  } else {
    run_files();
  }
  if (g_scanned == 0) {
    (void)printf("RULE R-ZERO-SCANNED -\n");
    (void)printf("blobscan: scanned=0 findings=%llu advisories=%llu\n",
                 (unsigned long long)(g_findings + 1u),
                 (unsigned long long)g_advisories);
    return EXIT_HARNESS;
  }
  (void)printf("blobscan: scanned=%llu findings=%llu advisories=%llu\n",
               (unsigned long long)g_scanned, (unsigned long long)g_findings,
               (unsigned long long)g_advisories);
  return g_findings > 0 ? EXIT_FINDINGS : EXIT_CLEAN;
}
