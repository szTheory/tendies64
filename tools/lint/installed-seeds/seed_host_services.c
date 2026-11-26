/*
 * Seed for scripts/check-api04.sh (seeded subcommand), plan 11-17: an object
 * that imports four host services (a clock, a file open, an entropy source and
 * a thread). The seeded run appends it to the extracted members of the
 * installed archive and expects exactly four L2-UNDEF-SYMBOL lines, one per
 * name. It is compiled by the script, never by CMake, never linked into the
 * core and kept outside tools/lint/selftest so the per-file lint selftest does
 * not run it. No system header is included: the prototypes are written out so
 * the object imports the plain names on every platform.
 */
extern int clock_gettime(int clock_id, void *timespec_out);
extern void *fopen(const char *path, const char *mode);
extern int getentropy(void *buffer, unsigned long length);
extern int pthread_create(void *thread, const void *attributes,
                          void *(*start)(void *), void *argument);

static void *seed_start(void *argument) {
  return argument;
}

int t64_seed_host_services(void) {
  char buffer[8];
  char timespec_out[16];
  void *thread[2];
  int result = clock_gettime(0, timespec_out);
  result += fopen("seed", "r") != 0;
  result += getentropy(buffer, sizeof buffer);
  result += pthread_create(thread, 0, seed_start, buffer);
  return result;
}
