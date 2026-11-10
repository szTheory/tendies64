/* EXPECT: L1-ATTR-CTOR L2-CTOR */
/* The constructor must have an observable effect, or -O2 deletes it. */
extern void t64_seed_hook(void);

__attribute__((constructor)) void t64_seed_ctor(void) { t64_seed_hook(); }
