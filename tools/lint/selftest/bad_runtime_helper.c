/* EXPECT: L1-RUNTIME-HELPER */
unsigned long long t64_seed_pop(unsigned long long x) {
  return (unsigned long long)__builtin_popcountll(x);
}

unsigned long long t64_seed_wide(unsigned long long a, unsigned long long b) {
  unsigned __int128 p = (unsigned __int128)a * b;
  return (unsigned long long)(p >> 64);
}
