/* EXPECT: L1-INCLUDE-BANNED L1-NAME-ENTROPY L2-UNDEF-SYMBOL */
#include <stdlib.h>

int t64_seed_roll(void) { return rand(); }
