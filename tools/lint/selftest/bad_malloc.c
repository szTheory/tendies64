/* EXPECT: L1-INCLUDE-BANNED L2-UNDEF-SYMBOL */
#include <stdlib.h>

void *t64_seed_alloc(unsigned long n) { return malloc(n); }
