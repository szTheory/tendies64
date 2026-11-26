/*
 * A host that already owns two common symbol names (Phase 11, plan 11-19, VER-05,
 * T-11-38). This file defines f32_add (the name of the core's SoftFloat 32-bit add)
 * and t64_sha256_init (the name of the SHA-256 initializer of tools/common) with
 * plausible signatures, deliberately wrong results and a call counter. It includes
 * neither header: a host would not.
 *
 * It is linked only into the executable t64c_collision (CMake option
 * T64C_COLLISION=ON). With the internal names of the archive renamed to t64i_* the
 * link succeeds, the core never calls these two functions, the composite case gives
 * the same results and the same digest as the executable without them, and both
 * counters stay at 0. Without the rename the linker would silently resolve the
 * core's own calls to these definitions or stop on a duplicate symbol.
 */
#include <stdint.h>
#include <stdio.h>

#include "t64c_cases.h"

/* The shape of the SoftFloat 32-bit value (a struct holding one uint32_t) and of
 * the context pointers; own types, because the consumer includes no core header. */
typedef struct {
  uint32_t v;
} t64c_collide_f32;

static unsigned long t64c_f32_add_calls;
static unsigned long t64c_sha256_init_calls;

t64c_collide_f32 f32_add(void *context, t64c_collide_f32 a, t64c_collide_f32 b);
void t64_sha256_init(void *context);

/* Deliberately wrong: the core would get 0xdeadbeef for every addition. */
t64c_collide_f32 f32_add(void *context, t64c_collide_f32 a, t64c_collide_f32 b) {
  t64c_collide_f32 wrong;

  (void)context;
  (void)a;
  (void)b;
  t64c_f32_add_calls++;
  wrong.v = UINT32_C(0xdeadbeef);
  return wrong;
}

/* Deliberately wrong: it leaves the context untouched, so a hash that used it would
 * start from whatever the caller's memory held. */
void t64_sha256_init(void *context) {
  (void)context;
  t64c_sha256_init_calls++;
}

int t64c_collision_report(int status) {
  (void)printf("collision: the consumer's own f32_add and t64_sha256_init are linked into this executable\n");
  (void)printf("f32_add calls: %lu\n", t64c_f32_add_calls);
  (void)printf("t64_sha256_init calls: %lu\n", t64c_sha256_init_calls);
  if (t64c_f32_add_calls != 0ul || t64c_sha256_init_calls != 0ul) {
    (void)fprintf(stderr, "t64c: the core called the host's own symbol\n");
    return 1;
  }
  return status;
}
