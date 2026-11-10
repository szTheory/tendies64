/* EXPECT: L1-ATTR-SECTION L2-W-SECTION */
/*
 * Mutable state in a section whose name looks read-only: on ELF the
 * section carries the write flag, on Mach-O the section is not on the
 * explicit read-only list (CR-13).
 */
#include <stdint.h>

#if defined(__APPLE__)
#define T64_RO_SEC "__TEXT,__t64"
#else
#define T64_RO_SEC ".rodata.t64"
#endif

__attribute__((section(T64_RO_SEC))) uint32_t t64_ro_state;

uint32_t t64_seed_ro_bump(void) { return ++t64_ro_state; }
