/* EXPECT: L1-INCLUDE-BANNED L1-NAME-ENV L2-UNDEF-SYMBOL */
#include <stdlib.h>

char *t64_seed_lookup(void) { return getenv("T64_SEED"); }
