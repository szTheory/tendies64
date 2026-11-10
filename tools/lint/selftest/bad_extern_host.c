/* EXPECT: L2-UNDEF-SYMBOL */
extern int host_poll(void);

int t64_seed_poll(void) { return host_poll(); }
