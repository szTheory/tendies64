/* Exact, rights-cleared real guest fixture identities admitted by smoke mode. */
#ifndef T64_REAL_FIXTURE_H
#define T64_REAL_FIXTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define T64_REAL_FIXTURE_SHA256_SIZE 65u

/* Guest tick bounds (K3). Every row without an explicit cap uses the default
 * cap; only a named long row may use the long cap; the ceiling is the largest
 * bound either binary accepts at option parse. */
#define T64_REAL_GUEST_TICKS_DEFAULT_CAP UINT64_C(600000)
#define T64_REAL_GUEST_TICKS_LONG_CAP UINT64_C(4000000)
#define T64_REAL_GUEST_TICKS_CEILING T64_REAL_GUEST_TICKS_LONG_CAP

typedef struct {
  const char *id;
  const char *sha256;
  size_t size;
  const char *profile_id;
  const char *profile_sha256;
  uint32_t completion_pc;
  bool pass_eligible;
  uint64_t guest_tick_cap; /* 0 means T64_REAL_GUEST_TICKS_DEFAULT_CAP */
} t64_real_fixture;

/* Unknown ids or byte identities are always refused. */
const t64_real_fixture *t64_real_fixture_find_bytes(
    size_t size, const char *sha256);
const t64_real_fixture *t64_real_fixture_find_identity(
    const char *id, const char *sha256);
bool t64_real_fixture_profile_allowed(const char *profile_id);

/* Largest guest tick bound the row accepts (default cap when the row has none
 * or when no row is given). */
uint64_t t64_real_fixture_tick_cap(const t64_real_fixture *fixture);

#endif /* T64_REAL_FIXTURE_H */
