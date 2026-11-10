/*
 * Dependency-free validator for a declared subset of JSON Schema (authored
 * here). It exists so that every constraint a schema file states is a
 * constraint that is enforced: a schema keyword outside the subset is refused
 * before any instance is looked at, so an unenforced constraint cannot hide.
 *
 * Keyword subset (anything else is M-SCHEMA-KEYWORD):
 *   type (a name or an array of names: object array string integer boolean
 *   null), required, properties, additionalProperties (the value false only),
 *   enum, const (scalar members only), items (one schema), minItems, maxItems,
 *   uniqueItems (arrays of scalars only), minLength, maxLength (counted in
 *   code points), pattern (POSIX ERE through regcomp with REG_EXTENDED and
 *   REG_NOSUB, written ^...$ so it is anchored), minimum, maximum, oneOf.
 *   Annotations that change nothing: $schema $id title description.
 *
 * Findings carry a rule id and a JSON pointer into the instance:
 *   M-SCHEMA-TYPE, M-SCHEMA-REQUIRED (the pointer names the missing key),
 *   M-SCHEMA-ENUM (enum and const), M-SCHEMA-PATTERN, M-SCHEMA-ADDITIONAL,
 *   M-SCHEMA-LENGTH, M-SCHEMA-ITEMS, M-SCHEMA-RANGE, M-SCHEMA-ONEOF.
 *
 * oneOf: every branch is tried silently. Exactly one clean branch passes.
 * When none is clean, the findings of the closest branch (fewest findings) are
 * reported, followed by one M-SCHEMA-ONEOF finding; when several are clean
 * only the M-SCHEMA-ONEOF finding is reported.
 */
#ifndef T64_SCHEMA_H
#define T64_SCHEMA_H

#include <stddef.h>

#include "t64_json.h"

#define T64_PTR_MAX 384u

/* A JSON pointer under construction. Keys are escaped (~0, ~1) and control
 * bytes become '?', so a pointer is always safe to print on one line. */
typedef struct {
  char s[T64_PTR_MAX];
  size_t len;
} t64_ptr;

void t64_ptr_init(t64_ptr *p);
/* Each push returns the previous length for t64_ptr_pop. */
size_t t64_ptr_push_key(t64_ptr *p, const char *key, size_t key_len);
size_t t64_ptr_push_index(t64_ptr *p, size_t index);
void t64_ptr_pop(t64_ptr *p, size_t old_len);

/* Copies s into out (capacity cap, at least 4) with control bytes replaced by
 * '?' and a "..." tail when it does not fit. */
void t64_sanitize(const char *s, size_t len, char *out, size_t cap);

typedef void (*t64_schema_emit_fn)(void *ctx, const char *rule,
                                   const char *pointer, const char *message);

/*
 * Walks the schema and returns 0 when every keyword is in the subset, every
 * keyword value has the right shape and every pattern compiles. Otherwise
 * returns 1, filling where (a pointer into the schema) and why.
 */
int t64_schema_check(const t64_json_node *schema, char *where, size_t where_cap,
                     char *why, size_t why_cap);

/*
 * Validates inst against a schema that passed t64_schema_check. Returns the
 * number of findings (0 means valid) and calls emit for each one (emit may be
 * NULL). Returns -1 on a harness error (filling why), for example a
 * uniqueItems constraint over non-scalar items.
 */
long t64_schema_validate(const t64_json_node *schema, const t64_json_node *inst,
                         t64_schema_emit_fn emit, void *ctx, char *why,
                         size_t why_cap);

#endif /* T64_SCHEMA_H */
