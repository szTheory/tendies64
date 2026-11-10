/*
 * Bounded canonical JSON writer (authored here).
 *
 * Output goes to a caller buffer and is always NUL-terminated. Canonical
 * form: no whitespace, object keys strictly ascending in unsigned byte order,
 * integers only (within +-(2^53-1); larger values must be written as decimal
 * strings by the caller), strings validated as UTF-8, and one trailing LF
 * added by t64_jsonw_finish.
 *
 * Escapes: only '"', '\\', \b \t \n \f \r, and the other controls below 0x20
 * as lowercase \u00xx. Everything else (including 0x7f and all non-ASCII) is
 * emitted raw.
 *
 * Errors are sticky: after the first failure every call returns that status
 * and nothing more is written.
 */
#ifndef T64_JSONW_H
#define T64_JSONW_H

#include <stddef.h>
#include <stdint.h>

#define T64_JSONW_MAX_DEPTH 16
#define T64_JSONW_MAX_KEY 127

typedef enum {
  T64_JSONW_OK = 0,
  T64_JSONW_E_OVERFLOW, /* caller buffer too small */
  T64_JSONW_E_ORDER,    /* key not strictly greater than the previous key */
  T64_JSONW_E_UTF8,     /* string or key is not valid UTF-8 */
  T64_JSONW_E_RANGE,    /* integer outside +-(2^53-1) */
  T64_JSONW_E_STATE,    /* call not valid here (e.g. value without a key) */
  T64_JSONW_E_DEPTH,    /* nesting deeper than T64_JSONW_MAX_DEPTH */
  T64_JSONW_E_KEY       /* key longer than T64_JSONW_MAX_KEY bytes */
} t64_jsonw_status;

typedef struct {
  char *buf;
  size_t cap;
  size_t len;
  t64_jsonw_status err;
  int depth;
  int root_done;
  int is_object[T64_JSONW_MAX_DEPTH];
  int have_key[T64_JSONW_MAX_DEPTH];
  size_t count[T64_JSONW_MAX_DEPTH];
  size_t prev_key_len[T64_JSONW_MAX_DEPTH];
  uint8_t prev_key[T64_JSONW_MAX_DEPTH][T64_JSONW_MAX_KEY];
} t64_jsonw;

void t64_jsonw_init(t64_jsonw *w, char *buf, size_t cap);

t64_jsonw_status t64_jsonw_begin_object(t64_jsonw *w);
t64_jsonw_status t64_jsonw_end_object(t64_jsonw *w);
t64_jsonw_status t64_jsonw_begin_array(t64_jsonw *w);
t64_jsonw_status t64_jsonw_end_array(t64_jsonw *w);
t64_jsonw_status t64_jsonw_key(t64_jsonw *w, const char *key, size_t len);
t64_jsonw_status t64_jsonw_string(t64_jsonw *w, const char *s, size_t len);
t64_jsonw_status t64_jsonw_int(t64_jsonw *w, int64_t v);
t64_jsonw_status t64_jsonw_null(t64_jsonw *w);
t64_jsonw_status t64_jsonw_bool(t64_jsonw *w, int v);

/*
 * Completes the document: requires exactly one finished root value, appends
 * the LF, and stores the total length (including the LF) in *out_len.
 */
t64_jsonw_status t64_jsonw_finish(t64_jsonw *w, size_t *out_len);

#endif /* T64_JSONW_H */
