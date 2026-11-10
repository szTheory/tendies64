/* EXPECT: L1-GLOBAL-MUTABLE L2-W-SECTION */
int t64_seed_global = 7;

int t64_seed_read(void) { return t64_seed_global; }
