/* EXPECT: L1-ATTR-SECTION L2-W-SECTION */
/*
 * Mutable state placed in a custom-named section through a macro. Layer 1
 * reads the macro use as a function-like declarator, so only the attribute
 * word and the object check catch it (CR-13).
 */
#include <stdint.h>

#if defined(__APPLE__)
#define T64_SEC "__DATA,__st"
#else
#define T64_SEC ".t64_state"
#endif
#define T64_DECL(n) __attribute__((section(T64_SEC))) uint32_t n

T64_DECL(t64_hidden_state);

uint32_t t64_seed_bump(void) { return ++t64_hidden_state; }
