/*
 * Check macros of the installed consumer (Phase 11, plan 11-16). The consumer
 * is an independent project: it includes no header of the repository tests,
 * so it carries its own counters. A case prints "name: checks=N failures=M"
 * and a case that ran no check fails.
 */
#ifndef T64C_CHECK_H
#define T64C_CHECK_H

#include <stdio.h>

typedef struct {
  unsigned long checks;
  unsigned long failures;
} t64c_tally;

#define T64C_CHECK(tally, condition)                                           \
  do {                                                                         \
    (tally)->checks++;                                                         \
    if (!(condition)) {                                                        \
      (tally)->failures++;                                                     \
      (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,            \
                    #condition);                                               \
    }                                                                          \
  } while (0)

#define T64C_CHECK_U32(tally, actual, expected)                                \
  do {                                                                         \
    const unsigned long t64c_actual = (unsigned long)(actual);                 \
    const unsigned long t64c_expected = (unsigned long)(expected);             \
    (tally)->checks++;                                                         \
    if (t64c_actual != t64c_expected) {                                        \
      (tally)->failures++;                                                     \
      (void)fprintf(stderr, "FAIL %s:%d: %s is %lu, expected %lu\n", __FILE__, \
                    __LINE__, #actual, t64c_actual, t64c_expected);            \
    }                                                                          \
  } while (0)

/* Print the case line; the exit status of the case: 0 only with checks and no
 * failure. */
static inline int t64c_report(const char *name, const t64c_tally *tally) {
  (void)printf("%s: checks=%lu failures=%lu\n", name, tally->checks,
               tally->failures);
  return (tally->checks == 0ul || tally->failures != 0ul) ? 1 : 0;
}

#endif /* T64C_CHECK_H */
