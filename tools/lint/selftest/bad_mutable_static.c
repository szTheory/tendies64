/* EXPECT: L1-STATIC-MUTABLE L2-W-SECTION */
static int t64_seed_counter;

int t64_seed_bump(void) { return ++t64_seed_counter; }
