/* EXPECT: L1-TOKEN-ATOMIC L2-W-SECTION */
static _Atomic int t64_seed_counter;

int t64_seed_inc(void) { return ++t64_seed_counter; }
