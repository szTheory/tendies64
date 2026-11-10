/* EXPECT: L1-INCLUDE-BANNED L1-NAME-ALLOC L2-UNDEF-SYMBOL */
#include <stdlib.h>

void *t64_seed_alloc(unsigned long n) { return malloc(n); }
