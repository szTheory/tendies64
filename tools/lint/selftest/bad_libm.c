/* EXPECT: L1-INCLUDE-BANNED L1-TOKEN-FLOAT L2-UNDEF-SYMBOL */
#include <math.h>

double t64_seed_sine(double x) { return sin(x); }
