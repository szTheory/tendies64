/* EXPECT: L1-NAME-TIME */
/*
 * A host cycle-counter intrinsic compiles to an instruction, not a call, so
 * Layer 2 sees no symbol; Layer 1 must name it (WR-16). Compilers without
 * this builtin reject the file, which Layer 2 reports as well.
 */
#include <stdint.h>

uint64_t t64_seed_cycles(void) {
  return (uint64_t)__builtin_readcyclecounter();
}
