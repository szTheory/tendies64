/* EXPECT: L1-ASM */
int t64_seed_asm(int x) {
  __asm__ volatile("" : "+r"(x));
  return x;
}
