/* EXPECT: L1-INCLUDE-BANNED L2-UNDEF-SYMBOL */
#include <stdio.h>

int t64_seed_print(int v) { return printf("%d\n", v); }
