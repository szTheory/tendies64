/* Helper header for bad_include_quoted.c. It lives outside core/ on purpose. */
#ifndef T64_SEED_WRAP_H
#define T64_SEED_WRAP_H
#include <time.h>
static inline unsigned long long t64_seed_wrap_now(void) {
  return (unsigned long long)time(0);
}
#endif
