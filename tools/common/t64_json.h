/*
 * Strict bounded JSON parser (authored here; RFC 8259 grammar, narrowed).
 *
 * The accepted language is deliberately smaller than RFC 8259 so every
 * consumer sees one unambiguous reading of an untrusted file:
 *   - numbers are integers only, within +-(2^53-1); no fraction, no exponent,
 *     no leading zero, no "-0"
 *   - strings are valid UTF-8 (RFC 3629) with the standard escapes; a lone
 *     surrogate escape is an error; raw control characters are errors
 *   - duplicate object keys (compared after unescaping) are errors
 *   - no comments, no trailing commas, no BOM, nothing after the root value
 *   - hard limits on input size, nesting depth, string length and node count
 *
 * u64 values must travel as decimal strings; see t64_json_u64_string.
 *
 * Memory: one node array and one string pool per document, sized from the
 * input and the limits, allocated once. All bounds are checked; recursion is
 * bounded by the depth limit (clamped to T64_JSON_HARD_MAX_DEPTH).
 *
 * Object keys longer than T64_JSON_MAX_KEY (127) bytes are rejected as
 * string-length, because the canonical writer cannot emit them. The canonical
 * writer also stops at 16 nested containers, which is the default depth limit.
 */
#ifndef T64_JSON_H
#define T64_JSON_H

#include <stddef.h>
#include <stdint.h>

#define T64_JSON_DEFAULT_MAX_INPUT (1024u * 1024u)
#define T64_JSON_DEFAULT_MAX_DEPTH 16u
#define T64_JSON_DEFAULT_MAX_STRING 4096u
#define T64_JSON_DEFAULT_MAX_NODES 4096u
#define T64_JSON_HARD_MAX_DEPTH 64u
#define T64_JSON_MAX_KEY 127u
#define T64_JSON_MAX_INT 9007199254740991LL /* 2^53 - 1 */

typedef struct {
  size_t max_input;  /* bytes of input */
  size_t max_depth;  /* nested containers; the root container is depth 1 */
  size_t max_string; /* decoded bytes of one string value */
  size_t max_nodes;  /* values in the document, containers included */
} t64_json_limits;

/* Fills *lim with the default limits (1 MiB, 16, 4096, 4096). */
void t64_json_limits_default(t64_json_limits *lim);

/* Closed set of failure categories. T64_JSON_OK is 0. */
typedef enum {
  T64_JSON_OK = 0,
  T64_JSON_E_SYNTAX,
  T64_JSON_E_TRAILING_COMMA,
  T64_JSON_E_COMMENT,
  T64_JSON_E_BOM,
  T64_JSON_E_CONTROL_CHAR,
  T64_JSON_E_INVALID_UTF8,
  T64_JSON_E_BAD_ESCAPE,
  T64_JSON_E_LONE_SURROGATE,
  T64_JSON_E_FLOAT,
  T64_JSON_E_EXPONENT,
  T64_JSON_E_LEADING_ZERO,
  T64_JSON_E_NEGATIVE_ZERO,
  T64_JSON_E_INT_RANGE,
  T64_JSON_E_DUPLICATE_KEY,
  T64_JSON_E_DEPTH,
  T64_JSON_E_SIZE,
  T64_JSON_E_STRING_LENGTH,
  T64_JSON_E_NODES,
  T64_JSON_E_TRAILING_DATA
} t64_json_category;

typedef struct {
  t64_json_category category;
  size_t offset; /* byte offset of the offending input byte */
} t64_json_error;

/* "syntax", "trailing-comma", ..., "trailing-data"; "ok" for T64_JSON_OK. */
const char *t64_json_category_name(t64_json_category c);

typedef enum {
  T64_JSON_NULL = 0,
  T64_JSON_BOOL,
  T64_JSON_INT,
  T64_JSON_STRING,
  T64_JSON_ARRAY,
  T64_JSON_OBJECT
} t64_json_type;

/*
 * One value. A member of an object carries its key (key != NULL). Children of
 * a container are a singly linked list: first, then next. Strings are
 * unescaped, NUL-terminated for convenience and may still contain NUL bytes
 * (use the length fields).
 */
typedef struct t64_json_node {
  t64_json_type type;
  int64_t num;      /* T64_JSON_INT value; T64_JSON_BOOL 0 or 1 */
  const char *str;  /* T64_JSON_STRING value */
  size_t len;       /* length of str */
  const char *key;  /* object member key, else NULL */
  size_t key_len;
  size_t count;     /* number of children of a container */
  struct t64_json_node *first;
  struct t64_json_node *next;
} t64_json_node;

typedef struct {
  const t64_json_node *root;
  size_t n_nodes;
  /* private */
  t64_json_node *nodes_;
  char *pool_;
} t64_json_doc;

/*
 * Parses buf[0..len) (not required to be NUL-terminated). lim may be NULL for
 * the defaults. Returns 0 and fills *out on success; otherwise returns 1,
 * fills *err (when not NULL) and leaves *out empty. out must be released with
 * t64_json_free on success.
 */
int t64_json_parse(const uint8_t *buf, size_t len, const t64_json_limits *lim,
                   t64_json_doc *out, t64_json_error *err);

void t64_json_free(t64_json_doc *doc);

/* Value of the member named key (NUL-terminated), or NULL. obj must be an object. */
const t64_json_node *t64_json_get(const t64_json_node *obj, const char *key);

typedef enum {
  T64_JSON_KEYS_OK = 0,
  T64_JSON_KEYS_NOT_OBJECT,
  T64_JSON_KEYS_UNKNOWN, /* *bad_key is the first member not in allowed */
  T64_JSON_KEYS_MISSING  /* *bad_key is the first required key that is absent */
} t64_json_keys_status;

/*
 * Checks that every member of obj is one of allowed[0..n_allowed) and that
 * every key of required[0..n_required) is present. bad_key may be NULL; the
 * string it receives is owned by the document or by the caller's table.
 */
t64_json_keys_status t64_json_check_keys(const t64_json_node *obj,
                                         const char *const *allowed,
                                         size_t n_allowed,
                                         const char *const *required,
                                         size_t n_required,
                                         const char **bad_key);

/*
 * Accepts a string node that matches 0|[1-9][0-9]* and is <= 2^64-1, and
 * stores its value in *out. Returns 1 on success, else 0.
 */
int t64_json_u64_string(const t64_json_node *node, uint64_t *out);

#endif /* T64_JSON_H */
