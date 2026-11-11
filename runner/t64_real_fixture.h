/* Exact, rights-cleared real guest fixture identities admitted by smoke mode. */
#ifndef T64_REAL_FIXTURE_H
#define T64_REAL_FIXTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define T64_REAL_FIXTURE_SHA256_SIZE 65u

typedef struct {
  const char *id;
  const char *sha256;
  size_t size;
  uint32_t completion_pc;
  bool pass_eligible;
} t64_real_fixture;

/* Unknown ids or byte identities are always refused. */
const t64_real_fixture *t64_real_fixture_find_bytes(
    size_t size, const char *sha256);
const t64_real_fixture *t64_real_fixture_find_identity(
    const char *id, const char *sha256);

#endif /* T64_REAL_FIXTURE_H */
