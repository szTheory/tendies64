/*
 * Tiny shared C unit-test header for Tendies64 (authored here).
 *
 * Usage: include it, call T64_CHECK / T64_CHECK_STR / T64_CHECK_INT in main(),
 * then `return T64_TEST_FINISH("unit.name");`. A failed check prints
 * file:line and the expression and is counted; it does not abort, so one run
 * reports every failure. The summary line is
 *   <name>: checks=N failures=M
 * and the exit status is nonzero when M > 0, or when N == 0 (a test that
 * checked nothing is not a pass).
 */
#ifndef T64_TEST_H
#define T64_TEST_H

#include <stdio.h>
#include <string.h>

static int t64_test_checks;
static int t64_test_failures;

#define T64_CHECK(expr)                                                        \
  do {                                                                         \
    t64_test_checks++;                                                         \
    if (!(expr)) {                                                             \
      t64_test_failures++;                                                     \
      fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #expr); \
    }                                                                          \
  } while (0)

/* Compare two NUL-terminated strings; print both on mismatch. */
#define T64_CHECK_STR(got, want)                                               \
  do {                                                                         \
    const char *t64_got_ = (got);                                              \
    const char *t64_want_ = (want);                                            \
    t64_test_checks++;                                                         \
    if (strcmp(t64_got_, t64_want_) != 0) {                                    \
      t64_test_failures++;                                                     \
      fprintf(stderr, "%s:%d: CHECK_STR failed: %s\n  got:  %s\n  want: %s\n", \
              __FILE__, __LINE__, #got, t64_got_, t64_want_);                  \
    }                                                                          \
  } while (0)

/* Compare two integers (as long long). */
#define T64_CHECK_INT(got, want)                                               \
  do {                                                                         \
    long long t64_got_i_ = (long long)(got);                                   \
    long long t64_want_i_ = (long long)(want);                                 \
    t64_test_checks++;                                                         \
    if (t64_got_i_ != t64_want_i_) {                                           \
      t64_test_failures++;                                                     \
      fprintf(stderr, "%s:%d: CHECK_INT failed: %s got %lld want %lld\n",      \
              __FILE__, __LINE__, #got, t64_got_i_, t64_want_i_);              \
    }                                                                          \
  } while (0)

/* Print the summary line and evaluate to the process exit status. */
#define T64_TEST_FINISH(name)                                                  \
  (printf("%s: checks=%d failures=%d\n", (name), t64_test_checks,              \
          t64_test_failures),                                                  \
   (t64_test_failures == 0 && t64_test_checks > 0) ? 0 : 1)

#endif /* T64_TEST_H */
