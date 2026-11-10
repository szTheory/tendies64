/* EXPECT: L1-INCLUDE-UNLISTED L2-UNDEF-SYMBOL */
/*
 * A quoted include of a header outside core/ hides its contents from
 * Layer 1 (here a static inline wrapper around the host clock), so it is
 * itself a violation (WR-16). Layer 2 still sees the clock symbol.
 */
#include "t64_seed_wrap.h"

unsigned long long t64_seed_now(void) { return t64_seed_wrap_now(); }
