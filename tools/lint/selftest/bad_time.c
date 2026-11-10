/* EXPECT: L1-INCLUDE-BANNED L1-NAME-TIME L2-UNDEF-SYMBOL */
#include <time.h>

long t64_seed_now(void) { return (long)time(0); }
