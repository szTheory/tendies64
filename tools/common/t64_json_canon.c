/*
 * t64-json-canon: canonical round-trip oracle.
 *
 *   t64-json-canon FILE          prints canon(FILE) on stdout
 *   t64-json-canon --check FILE  exit 0 and "ok" when FILE is canonical;
 *                                exit 1 and "NOT-CANONICAL FILE" when it parses
 *                                but differs from canon(FILE); exit 2 and
 *                                "JSON-<category> offset=N" when it does not parse
 *
 * Exit 3 is a usage or I/O problem. In plain mode the exit-2 diagnostic goes to
 * stderr so stdout only ever carries canonical bytes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_json.h"
#include "t64_jsonw.h"

enum { EXIT_OK = 0, EXIT_NONCANON = 1, EXIT_PARSE = 2, EXIT_USAGE = 3 };

/* Reads at most max+1 bytes so an oversized file reports as size, not memory. */
static int read_file(const char *path, size_t max, uint8_t **out, size_t *outlen) {
  FILE *f = fopen(path, "rb");
  uint8_t *buf;
  size_t cap = max + 1;
  size_t n = 0;

  if (f == NULL) return 0;
  buf = (uint8_t *)malloc(cap);
  if (buf == NULL) {
    (void)fclose(f);
    return 0;
  }
  while (n < cap) {
    size_t got = fread(buf + n, 1, cap - n, f);
    if (got == 0) break;
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

int main(int argc, char **argv) {
  int check = 0;
  const char *path;
  t64_json_limits lim;
  t64_json_doc doc;
  t64_json_error err;
  uint8_t *in = NULL;
  size_t inlen = 0;
  char *out;
  size_t outcap;
  size_t outlen = 0;
  t64_jsonw w;
  int rc;

  if (argc == 3 && strcmp(argv[1], "--check") == 0) {
    check = 1;
    path = argv[2];
  } else if (argc == 2 && argv[1][0] != '-') {
    path = argv[1];
  } else {
    fprintf(stderr, "usage: t64-json-canon [--check] FILE\n");
    return EXIT_USAGE;
  }

  t64_json_limits_default(&lim);
  if (!read_file(path, lim.max_input, &in, &inlen)) {
    fprintf(stderr, "ERROR cannot read %s\n", path);
    return EXIT_USAGE;
  }
  if (t64_json_parse(in, inlen, &lim, &doc, &err) != 0) {
    FILE *dst = check ? stdout : stderr;
    fprintf(dst, "JSON-%s offset=%lu\n", t64_json_category_name(err.category),
            (unsigned long)err.offset);
    free(in);
    return EXIT_PARSE;
  }
  /* Canonical output is never longer than the input plus the final LF. */
  outcap = inlen + 16;
  out = (char *)malloc(outcap);
  if (out == NULL) {
    fprintf(stderr, "ERROR out of memory\n");
    t64_json_free(&doc);
    free(in);
    return EXIT_USAGE;
  }
  t64_jsonw_init(&w, out, outcap);
  if (t64_jsonw_node(&w, doc.root) != T64_JSONW_OK ||
      t64_jsonw_finish(&w, &outlen) != T64_JSONW_OK) {
    fprintf(stderr, "ERROR canonical writer failed (status %d)\n", (int)w.err);
    rc = EXIT_USAGE;
  } else if (check) {
    if (outlen == inlen && memcmp(out, in, inlen) == 0) {
      printf("ok\n");
      rc = EXIT_OK;
    } else {
      printf("NOT-CANONICAL %s\n", path);
      rc = EXIT_NONCANON;
    }
  } else {
    rc = fwrite(out, 1, outlen, stdout) == outlen ? EXIT_OK : EXIT_USAGE;
  }
  free(out);
  t64_json_free(&doc);
  free(in);
  return rc;
}
