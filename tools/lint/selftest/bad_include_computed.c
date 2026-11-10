/* EXPECT: L1-INCLUDE-UNLISTED */
/* A computed include names its header through a macro, which the header
 * allowlist cannot check (WR-16). */
#define T64_SEED_HDR <stdint.h>
#include T64_SEED_HDR

uint32_t t64_seed_computed(void) { return 1u; }
