/* EXPECT: L1-ATTR-SECTION L1-GLOBAL-MUTABLE L2-W-SECTION */
/*
 * A section pragma moves later objects into a named section without any
 * attribute on the declaration (clang; other compilers ignore it and the
 * object stays in ordinary writable data). Both layers must fail it.
 */
#include <stdint.h>

#if defined(__clang__) && defined(__APPLE__)
#pragma clang section data = "__DATA,__pst"
#elif defined(__clang__)
#pragma clang section data = ".t64_pstate"
#endif

uint32_t t64_pragma_state = 3u;

uint32_t t64_seed_pragma_bump(void) { return ++t64_pragma_state; }
