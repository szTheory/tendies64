/*
 * Six-outcome precedence classifier (RUN-02) and the strict runner-report
 * parser (authored here). Pure: no I/O, no globals. The supervisor builds a
 * t64_observation from what it saw and calls t64_classify; the table-driven
 * test (runner/tests/test_classify.c) exercises every closed detail value.
 *
 * Normative contract: docs/spec/result-protocol.md.
 */
#ifndef T64_CLASSIFY_H
#define T64_CLASSIFY_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
  T64_OUT_PASS = 0,
  T64_OUT_HARNESS_ERROR,
  T64_OUT_MISSING_FIXTURE,
  T64_OUT_GUEST_FAIL,
  T64_OUT_BOOT_FAIL,
  T64_OUT_TIMEOUT
} t64_outcome;

/* The closed detail enum. Order is not significant; names are. */
typedef enum {
  /* harness-error */
  T64_D_USAGE = 0,
  T64_D_EXEC_FAILED,
  T64_D_SUPERVISOR_SIGNALLED,
  T64_D_WALL_BACKSTOP,
  T64_D_RUNNER_SIGNALLED,
  T64_D_RUNNER_EXIT_NONZERO,
  T64_D_NO_RESULT,
  T64_D_REPORT_INVALID,
  T64_D_FIXTURE_DIGEST_MISMATCH,
  T64_D_INVENTORY_MISMATCH, /* produced only by suite consumers */
  T64_D_SUITE_TRUNCATED,    /* produced only by suite consumers */
  T64_D_UNCLASSIFIED,       /* defensive default */
  /* missing-fixture */
  T64_D_FIXTURE_ABSENT,
  /* guest-fail */
  T64_D_LOG_OVER_LIMIT,
  T64_D_LOG_INVALID_UTF8,
  T64_D_FAIL_REPORTED,
  T64_D_OUTPUT_AFTER_DONE,
  T64_D_COUNT_MISMATCH,
  T64_D_VERDICT_INCOMPLETE,
  /* boot-fail */
  T64_D_EMPTY_OUTPUT,
  /* timeout */
  T64_D_GUEST_BOUND,
  /* pass */
  T64_D_POSITIVE_MATCH,
  T64_D_COUNT_ /* number of details; not a value */
} t64_detail;

typedef enum {
  T64_FIX_OK = 0,
  T64_FIX_ABSENT,
  T64_FIX_DIGEST_MISMATCH,
  T64_FIX_BAD /* present but not a regular file, too large or unreadable */
} t64_fixture_state;

typedef enum {
  T64_WAIT_NONE = 0, /* never reaped */
  T64_WAIT_EXITED,
  T64_WAIT_SIGNALLED
} t64_wait_state;

typedef enum {
  T64_REPORT_STOP_NONE = 0,
  T64_REPORT_STOP_BOUND,
  T64_REPORT_STOP_HALT
} t64_report_stop;

typedef struct {
  t64_report_stop stop;
  uint64_t guest_tick;
  uint64_t log_bytes_total;
} t64_report;

typedef struct {
  int usage_error;
  t64_fixture_state fixture;
  int launched;
  int exec_failed;
  int supervisor_signalled;
  int wall_backstop;
  t64_wait_state wait_state;
  int exit_code;
  size_t report_len; /* 0 means no report file content */
  int report_malformed;
  t64_report_stop stop;
  uint64_t guest_tick;
  uint64_t log_bytes_total;
  const uint8_t *capture;
  size_t capture_len;
  uint64_t expect_pass;
  uint64_t bound;
  uint64_t log_limit;
} t64_observation;

typedef struct {
  uint64_t fail_reported;
  uint64_t log_bytes;
  uint64_t log_bytes_total;
  uint64_t log_lines;
  uint64_t pass_reported;
  uint64_t unhandled_exception_lines;
} t64_counts;

typedef struct {
  t64_outcome outcome;
  t64_detail detail;
  int exit_code;
  int guest_data; /* 1: stop_reason, guest_tick, counts and log digest apply */
  t64_counts counts;
} t64_verdict;

void t64_classify(const t64_observation *obs, t64_verdict *out);

/* Names as written into t64.result. NULL for out-of-range values. */
const char *t64_outcome_name(t64_outcome o);
const char *t64_detail_name(t64_detail d);
/* The outcome a detail belongs to, and the exit code of an outcome. */
t64_outcome t64_detail_outcome(t64_detail d);
int t64_outcome_exit(t64_outcome o);

/*
 * Strict parse of the five-line runner report. Returns 1 and fills *out, or
 * 0 when the bytes deviate from the grammar in any way (including more than
 * 512 bytes, CR bytes, leading zeros, overflow or log_bytes_total above 2^53-1).
 */
int t64_parse_report(const uint8_t *buf, size_t len, t64_report *out);

#endif /* T64_CLASSIFY_H */
