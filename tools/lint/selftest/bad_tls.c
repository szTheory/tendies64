/* EXPECT: L1-TLS L2-W-SECTION */
static __thread int t64_seed_tls;

int t64_seed_tls_bump(void) { return ++t64_seed_tls; }
