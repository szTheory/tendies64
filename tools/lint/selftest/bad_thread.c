/* EXPECT: L1-INCLUDE-BANNED L1-NAME-THREAD L2-UNDEF-SYMBOL */
#include <pthread.h>

unsigned long t64_seed_self(void) { return (unsigned long)pthread_self(); }
