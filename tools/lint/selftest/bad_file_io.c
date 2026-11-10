/* EXPECT: L1-INCLUDE-BANNED L1-NAME-FILE L2-UNDEF-SYMBOL */
#include <stdio.h>

void *t64_seed_open(const char *path) { return fopen(path, "rb"); }
