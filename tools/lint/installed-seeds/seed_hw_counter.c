/*
 * Seed for scripts/check-api04.sh (seeded subcommand), plan 11-17: an object
 * that reads a hardware counter. The seeded run appends it to the extracted
 * members of the installed archive and expects the disassembly scan to fail
 * naming the instruction (mrs of cntvct_el0 on aarch64, rdtsc on x86_64). It
 * imports no symbol, so the undefined-set lint passes it: only the
 * disassembly scan sees it. Compiled by the script, never by CMake, never
 * linked into the core.
 */
#if defined(__aarch64__)
unsigned long long t64_seed_hw_counter(void) {
  unsigned long long value;
  __asm__ volatile("mrs %0, cntvct_el0" : "=r"(value));
  return value;
}
#elif defined(__x86_64__)
unsigned long long t64_seed_hw_counter(void) {
  unsigned int low;
  unsigned int high;
  __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
  return ((unsigned long long)high << 32) | low;
}
#else
#error "seed_hw_counter.c seeds a hardware counter read for aarch64 and x86_64 only; add the instruction of this architecture to the seed and to scripts/check-api04.sh"
#endif
