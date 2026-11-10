/*
 * unit.classify: table-driven test of the six-outcome precedence function and
 * the strict report parser. Every detail value reachable from a single run is
 * exercised with its exact outcome and exit code (inventory-mismatch and
 * suite-truncated belong to suite consumers and are only name-checked).
 * Expected values are written from the precedence table in
 * docs/spec/result-protocol.md, not derived from the code under test.
 */
#include <stdio.h>
#include <string.h>

#include "t64_classify.h"
#include "t64_test.h"

typedef struct {
  const char *name;
  int usage;
  t64_fixture_state fix;
  int launched;
  int exec_failed;
  int sup_sig;
  int wall;
  t64_wait_state wait;
  int code;
  size_t rep_len;
  int malformed;
  t64_report_stop stop;
  uint64_t tick;
  uint64_t total;
  const char *cap;
  size_t cap_len;
  uint64_t expect;
  uint64_t bound;
  uint64_t limit;
  t64_outcome out;
  t64_detail det;
  int exit_code;
} row;

#define PASS_OUT T64_OUT_PASS
#define HERR T64_OUT_HARNESS_ERROR
#define MISS T64_OUT_MISSING_FIXTURE
#define GFAIL T64_OUT_GUEST_FAIL
#define BFAIL T64_OUT_BOOT_FAIL
#define TMOUT T64_OUT_TIMEOUT
#define SB T64_REPORT_STOP_BOUND
#define SH T64_REPORT_STOP_HALT
#define SN T64_REPORT_STOP_NONE

/* A launched, cleanly exited runner with a consistent report (bound 1000,
 * log limit 4096). */
#define G(name, stop, tick, total, cap, expect, out, det, exit_code)           \
  {name, 0, T64_FIX_OK, 1, 0, 0, 0, T64_WAIT_EXITED, 0, 50, 0, stop, tick,     \
   total, cap, sizeof(cap) - 1u, expect, 1000, 4096, out, det, exit_code}

/* Fully specified row. */
#define X(name, usage, fix, launched, execf, sigs, wall, wait, code, replen,   \
          malf, stop, tick, total, cap, expect, limit, out, det, exit_code)    \
  {name, usage, fix, launched, execf, sigs, wall, wait, code, replen, malf,    \
   stop, tick, total, cap, sizeof(cap) - 1u, expect, 1000, limit, out, det,    \
   exit_code}

#define OKCAP "PASS a\nPASS b\nDone!\n"

static const row rows[] = {
    /* 6. pass */
    G("pass-halt", SH, 40, 20, OKCAP, 2, PASS_OUT, T64_D_POSITIVE_MATCH, 0),
    G("pass-bound-after-done", SB, 1000, 20, OKCAP, 2, PASS_OUT,
      T64_D_POSITIVE_MATCH, 0),
    G("pass-halt-at-bound", SH, 1000, 20, OKCAP, 2, PASS_OUT,
      T64_D_POSITIVE_MATCH, 0),
    G("pass-unterminated-final-line", SH, 40, 19, "PASS a\nPASS b\nDone!", 2,
      PASS_OUT, T64_D_POSITIVE_MATCH, 0),
    G("pass-expect-zero", SH, 40, 6, "Done!\n", 0, PASS_OUT,
      T64_D_POSITIVE_MATCH, 0),
    G("pass-unhandled-exception-after-done", SH, 40, 52,
      OKCAP "Unhandled Exception at 80000180\n", 2, PASS_OUT,
      T64_D_POSITIVE_MATCH, 0),
    G("pass-unhandled-exception-before-done", SH, 40, 40,
      "PASS a\nPASS b\nunhandled EXCEPTION\nDone!\n", 2, PASS_OUT,
      T64_D_POSITIVE_MATCH, 0),
    /* 5. timeout */
    G("timeout-bound-no-done", SB, 1000, 14, "PASS a\nPASS b\n", 2, TMOUT,
      T64_D_GUEST_BOUND, 12),
    /* 4. boot-fail */
    G("boot-empty-halt", SH, 5, 0, "", 0, BFAIL, T64_D_EMPTY_OUTPUT, 11),
    G("boot-empty-bound-not-timeout", SB, 1000, 0, "", 0, BFAIL,
      T64_D_EMPTY_OUTPUT, 11),
    /* 3. guest-fail, one row per detail and the precedence edges */
    X("gfail-over-limit", 0, T64_FIX_OK, 1, 0, 0, 0, T64_WAIT_EXITED, 0, 50, 0,
      SB, 1000, 20, "PASS a\nP", 2, 8, GFAIL, T64_D_LOG_OVER_LIMIT, 10),
    X("gfail-over-limit-beats-fail-line", 0, T64_FIX_OK, 1, 0, 0, 0,
      T64_WAIT_EXITED, 0, 50, 0, SB, 1000, 20, "FAIL a\nP", 2, 8, GFAIL,
      T64_D_LOG_OVER_LIMIT, 10),
    G("gfail-invalid-utf8", SH, 40, 22, "PASS a\n\xff\nPASS b\nDone!\n", 2,
      GFAIL, T64_D_LOG_INVALID_UTF8, 10),
    G("gfail-invalid-utf8-with-bound-reached", SB, 1000, 3, "\xc0\xaf\n", 2,
      GFAIL, T64_D_LOG_INVALID_UTF8, 10),
    G("gfail-invalid-utf8-beats-fail-line", SH, 40, 9, "FAIL \xed\xa0\x80\n",
      2, GFAIL, T64_D_LOG_INVALID_UTF8, 10),
    G("gfail-fail-reported", SH, 40, 20, "PASS a\nFAIL b\nDone!\n", 2, GFAIL,
      T64_D_FAIL_REPORTED, 10),
    G("gfail-fail-after-done-is-fail-reported", SH, 40, 27, OKCAP "FAIL c\n",
      2, GFAIL, T64_D_FAIL_REPORTED, 10),
    G("gfail-fail-beats-count-mismatch", SB, 1000, 7, "FAIL a\n", 2, GFAIL,
      T64_D_FAIL_REPORTED, 10),
    G("gfail-bare-fail-word-is-not-a-fail-line", SH, 40, 5, "FAIL\n", 2, GFAIL,
      T64_D_VERDICT_INCOMPLETE, 10),
    G("gfail-output-after-done", SH, 40, 26, OKCAP "extra\n", 2, GFAIL,
      T64_D_OUTPUT_AFTER_DONE, 10),
    G("gfail-second-done", SH, 40, 26, OKCAP "Done!\n", 2, GFAIL,
      T64_D_OUTPUT_AFTER_DONE, 10),
    G("gfail-blank-line-after-done", SH, 40, 21, OKCAP "\n", 2, GFAIL,
      T64_D_OUTPUT_AFTER_DONE, 10),
    G("gfail-output-after-done-beats-count", SH, 40, 19,
      "PASS a\nDone!\nextra\n", 2, GFAIL, T64_D_OUTPUT_AFTER_DONE, 10),
    G("gfail-count-too-few", SH, 40, 13, "PASS a\nDone!\n", 2, GFAIL,
      T64_D_COUNT_MISMATCH, 10),
    G("gfail-count-too-many", SH, 40, 27, "PASS a\nPASS b\nPASS c\nDone!\n", 2,
      GFAIL, T64_D_COUNT_MISMATCH, 10),
    G("gfail-verdict-incomplete", SH, 40, 14, "PASS a\nPASS b\n", 2, GFAIL,
      T64_D_VERDICT_INCOMPLETE, 10),
    G("gfail-done-with-trailing-space-is-not-done", SH, 40, 7, "Done! \n", 2,
      GFAIL, T64_D_VERDICT_INCOMPLETE, 10),
    /* 1. harness-error: report-invalid sub-conditions */
    X("report-malformed", 0, T64_FIX_OK, 1, 0, 0, 0, T64_WAIT_EXITED, 0, 50, 1,
      SN, 0, 0, "", 0, 4096, HERR, T64_D_REPORT_INVALID, 1),
    X("report-stop-none", 0, T64_FIX_OK, 1, 0, 0, 0, T64_WAIT_EXITED, 0, 50, 0,
      SN, 40, 20, OKCAP, 2, 4096, HERR, T64_D_REPORT_INVALID, 1),
    G("report-tick-above-bound", SH, 1001, 20, OKCAP, 2, HERR,
      T64_D_REPORT_INVALID, 1),
    G("report-bound-with-tick-below-bound", SB, 999, 20, OKCAP, 2, HERR,
      T64_D_REPORT_INVALID, 1),
    G("report-bound-with-tick-above-bound", SB, 1001, 20, OKCAP, 2, HERR,
      T64_D_REPORT_INVALID, 1),
    X("report-capture-above-limit", 0, T64_FIX_OK, 1, 0, 0, 0,
      T64_WAIT_EXITED, 0, 50, 0, SH, 40, 7, "PASS a\n", 2, 4, HERR,
      T64_D_REPORT_INVALID, 1),
    G("report-total-below-capture", SH, 40, 3, OKCAP, 2, HERR,
      T64_D_REPORT_INVALID, 1),
    /* WR-02: the capture must be exactly min(log_bytes_total, log_limit)
     * bytes; a short capture would hide a dropped FAIL line. */
    G("report-capture-shorter-than-total", SH, 40, 500, "Done!\n", 0, HERR,
      T64_D_REPORT_INVALID, 1),
    G("report-capture-one-byte-short", SH, 40, 21, OKCAP, 2, HERR,
      T64_D_REPORT_INVALID, 1),
    X("report-capture-short-of-limit", 0, T64_FIX_OK, 1, 0, 0, 0,
      T64_WAIT_EXITED, 0, 50, 0, SB, 1000, 100, "PASS a\n", 2, 64, HERR,
      T64_D_REPORT_INVALID, 1),
    X("report-capture-equal-to-limit-is-consistent", 0, T64_FIX_OK, 1, 0, 0, 0,
      T64_WAIT_EXITED, 0, 50, 0, SB, 1000, 100, "PASS a\n", 2, 7, GFAIL,
      T64_D_LOG_OVER_LIMIT, 10),
    /* 1. harness-error: process paths, never trusting the runner */
    X("process-usage", 1, T64_FIX_OK, 0, 0, 0, 0, T64_WAIT_NONE, 0, 0, 0, SN,
      0, 0, "", 0, 4096, HERR, T64_D_USAGE, 1),
    X("process-bad-fixture-is-usage", 0, T64_FIX_BAD, 0, 0, 0, 0,
      T64_WAIT_NONE, 0, 0, 0, SN, 0, 0, "", 0, 4096, HERR, T64_D_USAGE, 1),
    X("process-exec-failed", 0, T64_FIX_OK, 1, 1, 0, 0, T64_WAIT_EXITED, 127, 0,
      0, SN, 0, 0, "", 0, 4096, HERR, T64_D_EXEC_FAILED, 1),
    X("process-supervisor-signalled", 0, T64_FIX_OK, 1, 0, 1, 0,
      T64_WAIT_SIGNALLED, 0, 0, 0, SN, 0, 0, "", 0, 4096, HERR,
      T64_D_SUPERVISOR_SIGNALLED, 1),
    X("process-wall-backstop-with-valid-pass-report", 0, T64_FIX_OK, 1, 0, 0, 1,
      T64_WAIT_SIGNALLED, 0, 50, 0, SH, 40, 20, OKCAP, 2, 4096, HERR,
      T64_D_WALL_BACKSTOP, 1),
    X("process-runner-signalled-with-valid-pass-report", 0, T64_FIX_OK, 1, 0, 0,
      0, T64_WAIT_SIGNALLED, 0, 50, 0, SH, 40, 20, OKCAP, 2, 4096, HERR,
      T64_D_RUNNER_SIGNALLED, 1),
    X("process-runner-exit-nonzero-with-valid-pass-report", 0, T64_FIX_OK, 1, 0,
      0, 0, T64_WAIT_EXITED, 7, 50, 0, SH, 40, 20, OKCAP, 2, 4096, HERR,
      T64_D_RUNNER_EXIT_NONZERO, 1),
    X("process-exit-134-is-harness-error", 0, T64_FIX_OK, 1, 0, 0, 0,
      T64_WAIT_EXITED, 134, 50, 0, SH, 40, 20, OKCAP, 2, 4096, HERR,
      T64_D_RUNNER_EXIT_NONZERO, 1),
    X("process-exit0-without-report", 0, T64_FIX_OK, 1, 0, 0, 0,
      T64_WAIT_EXITED, 0, 0, 0, SN, 0, 0, "", 0, 4096, HERR, T64_D_NO_RESULT,
      1),
    /* 1/2. fixture states, resolved before launch */
    X("fixture-digest-mismatch", 0, T64_FIX_DIGEST_MISMATCH, 0, 0, 0, 0,
      T64_WAIT_NONE, 0, 0, 0, SN, 0, 0, "", 0, 4096, HERR,
      T64_D_FIXTURE_DIGEST_MISMATCH, 1),
    X("fixture-absent", 0, T64_FIX_ABSENT, 0, 0, 0, 0, T64_WAIT_NONE, 0, 0, 0,
      SN, 0, 0, "", 0, 4096, MISS, T64_D_FIXTURE_ABSENT, 13),
    /* precedence between groups */
    X("precedence-usage-beats-exec-failed", 1, T64_FIX_OK, 1, 1, 0, 0,
      T64_WAIT_EXITED, 127, 0, 0, SN, 0, 0, "", 0, 4096, HERR, T64_D_USAGE, 1),
    X("precedence-exec-failed-beats-signalled", 0, T64_FIX_OK, 1, 1, 1, 1,
      T64_WAIT_EXITED, 127, 0, 0, SN, 0, 0, "", 0, 4096, HERR,
      T64_D_EXEC_FAILED, 1),
    X("precedence-supervisor-signalled-beats-wall", 0, T64_FIX_OK, 1, 0, 1, 1,
      T64_WAIT_SIGNALLED, 0, 0, 0, SN, 0, 0, "", 0, 4096, HERR,
      T64_D_SUPERVISOR_SIGNALLED, 1),
    X("precedence-harness-error-beats-missing-fixture", 0, T64_FIX_ABSENT, 1, 0,
      0, 1, T64_WAIT_SIGNALLED, 0, 0, 0, SN, 0, 0, "", 0, 4096, HERR,
      T64_D_WALL_BACKSTOP, 1),
    X("precedence-report-invalid-beats-guest-fail", 0, T64_FIX_OK, 1, 0, 0, 0,
      T64_WAIT_EXITED, 0, 50, 0, SH, 2000, 27, "FAIL a\n", 2, 4096, HERR,
      T64_D_REPORT_INVALID, 1),
    /* defensive default */
    X("unclassified-launched-but-never-reaped", 0, T64_FIX_OK, 1, 0, 0, 0,
      T64_WAIT_NONE, 0, 50, 0, SH, 40, 20, OKCAP, 2, 4096, HERR,
      T64_D_UNCLASSIFIED, 1),
    X("unclassified-nothing-observed", 0, T64_FIX_OK, 0, 0, 0, 0,
      T64_WAIT_NONE, 0, 0, 0, SN, 0, 0, "", 0, 4096, HERR, T64_D_UNCLASSIFIED,
      1),
    X("unclassified-garbage-fixture-state", 0, (t64_fixture_state)99, 0, 0, 0,
      0, T64_WAIT_NONE, 0, 0, 0, SN, 0, 0, "", 0, 4096, HERR,
      T64_D_UNCLASSIFIED, 1),
};

#define NROWS (sizeof rows / sizeof rows[0])

static void run_rows(void) {
  int seen[T64_D_COUNT_];
  size_t i;
  int d;
  memset(seen, 0, sizeof seen);
  for (i = 0; i < NROWS; i++) {
    const row *r = &rows[i];
    t64_observation o;
    t64_verdict v;
    int bad;
    memset(&o, 0, sizeof o);
    o.usage_error = r->usage;
    o.fixture = r->fix;
    o.launched = r->launched;
    o.exec_failed = r->exec_failed;
    o.supervisor_signalled = r->sup_sig;
    o.wall_backstop = r->wall;
    o.wait_state = r->wait;
    o.exit_code = r->code;
    o.report_len = r->rep_len;
    o.report_malformed = r->malformed;
    o.stop = r->stop;
    o.guest_tick = r->tick;
    o.log_bytes_total = r->total;
    o.capture = (const uint8_t *)r->cap;
    o.capture_len = r->cap_len;
    o.expect_pass = r->expect;
    o.bound = r->bound;
    o.log_limit = r->limit;
    t64_classify(&o, &v);
    bad = v.outcome != r->out || v.detail != r->det ||
          v.exit_code != r->exit_code;
    if (bad) {
      fprintf(stderr, "row %s: got %s/%s exit %d, want %s/%s exit %d\n",
              r->name, t64_outcome_name(v.outcome), t64_detail_name(v.detail),
              v.exit_code, t64_outcome_name(r->out), t64_detail_name(r->det),
              r->exit_code);
    }
    T64_CHECK(!bad);
    if ((int)r->det >= 0 && (int)r->det < (int)T64_D_COUNT_) {
      seen[r->det] = 1;
    }
  }
  /* Every detail reachable from a single run must have at least one row. */
  for (d = 0; d < (int)T64_D_COUNT_; d++) {
    if (d == (int)T64_D_INVENTORY_MISMATCH || d == (int)T64_D_SUITE_TRUNCATED) {
      continue;
    }
    if (!seen[d]) {
      fprintf(stderr, "no row covers detail %s\n", t64_detail_name((t64_detail)d));
    }
    T64_CHECK(seen[d]);
  }
  printf("classify rows: %lu\n", (unsigned long)NROWS);
}

static void names_and_exits(void) {
  /* Written from the contract: closed enum names, owning outcome, exit code. */
  static const struct {
    t64_detail d;
    const char *name;
    t64_outcome out;
    int exit_code;
  } want[] = {
      {T64_D_USAGE, "usage", HERR, 1},
      {T64_D_EXEC_FAILED, "exec-failed", HERR, 1},
      {T64_D_SUPERVISOR_SIGNALLED, "supervisor-signalled", HERR, 1},
      {T64_D_WALL_BACKSTOP, "wall-backstop", HERR, 1},
      {T64_D_RUNNER_SIGNALLED, "runner-signalled", HERR, 1},
      {T64_D_RUNNER_EXIT_NONZERO, "runner-exit-nonzero", HERR, 1},
      {T64_D_NO_RESULT, "no-result", HERR, 1},
      {T64_D_REPORT_INVALID, "report-invalid", HERR, 1},
      {T64_D_FIXTURE_DIGEST_MISMATCH, "fixture-digest-mismatch", HERR, 1},
      {T64_D_INVENTORY_MISMATCH, "inventory-mismatch", HERR, 1},
      {T64_D_SUITE_TRUNCATED, "suite-truncated", HERR, 1},
      {T64_D_UNCLASSIFIED, "unclassified", HERR, 1},
      {T64_D_FIXTURE_ABSENT, "fixture-absent", MISS, 13},
      {T64_D_LOG_OVER_LIMIT, "log-over-limit", GFAIL, 10},
      {T64_D_LOG_INVALID_UTF8, "log-invalid-utf8", GFAIL, 10},
      {T64_D_FAIL_REPORTED, "fail-reported", GFAIL, 10},
      {T64_D_OUTPUT_AFTER_DONE, "output-after-done", GFAIL, 10},
      {T64_D_COUNT_MISMATCH, "count-mismatch", GFAIL, 10},
      {T64_D_VERDICT_INCOMPLETE, "verdict-incomplete", GFAIL, 10},
      {T64_D_EMPTY_OUTPUT, "empty-output", BFAIL, 11},
      {T64_D_GUEST_BOUND, "guest-bound", TMOUT, 12},
      {T64_D_POSITIVE_MATCH, "positive-match", PASS_OUT, 0},
  };
  size_t i;
  T64_CHECK_INT(sizeof want / sizeof want[0], T64_D_COUNT_);
  for (i = 0; i < sizeof want / sizeof want[0]; i++) {
    T64_CHECK_STR(t64_detail_name(want[i].d), want[i].name);
    T64_CHECK_INT(t64_detail_outcome(want[i].d), want[i].out);
    T64_CHECK_INT(t64_outcome_exit(want[i].out), want[i].exit_code);
  }
  T64_CHECK_STR(t64_outcome_name(PASS_OUT), "pass");
  T64_CHECK_STR(t64_outcome_name(HERR), "harness-error");
  T64_CHECK_STR(t64_outcome_name(MISS), "missing-fixture");
  T64_CHECK_STR(t64_outcome_name(GFAIL), "guest-fail");
  T64_CHECK_STR(t64_outcome_name(BFAIL), "boot-fail");
  T64_CHECK_STR(t64_outcome_name(TMOUT), "timeout");
  T64_CHECK(t64_detail_name((t64_detail)-1) == NULL);
  T64_CHECK(t64_detail_name(T64_D_COUNT_) == NULL);
  T64_CHECK(t64_outcome_name((t64_outcome)6) == NULL);
}

static void counts(void) {
  static const char cap[] =
      "PASS a\nPASS b\nDone!\nUnhandled Exception at 80000180\nUNHANDLED "
      "exception again";
  t64_observation o;
  t64_verdict v;
  memset(&o, 0, sizeof o);
  o.fixture = T64_FIX_OK;
  o.launched = 1;
  o.wait_state = T64_WAIT_EXITED;
  o.report_len = 50;
  o.stop = SH;
  o.guest_tick = 40;
  o.capture = (const uint8_t *)cap;
  o.capture_len = sizeof cap - 1u;
  o.log_bytes_total = o.capture_len;
  o.expect_pass = 2;
  o.bound = 1000;
  o.log_limit = 4096;
  t64_classify(&o, &v);
  T64_CHECK_INT(v.outcome, PASS_OUT);
  T64_CHECK_INT(v.guest_data, 1);
  T64_CHECK_INT(v.counts.log_lines, 5);
  T64_CHECK_INT(v.counts.pass_reported, 2);
  T64_CHECK_INT(v.counts.fail_reported, 0);
  T64_CHECK_INT(v.counts.unhandled_exception_lines, 2);
  T64_CHECK_INT(v.counts.log_bytes, sizeof cap - 1u);
  T64_CHECK_INT(v.counts.log_bytes_total, sizeof cap - 1u);
  /* Harness-error and missing-fixture carry no guest data. */
  o.wall_backstop = 1;
  t64_classify(&o, &v);
  T64_CHECK_INT(v.guest_data, 0);
  T64_CHECK_INT(v.counts.log_lines, 0);
}

/* WR-01, WR-02: a report or capture that is not a regular file, or a capture
 * that could not be read in full, is report-invalid; it never reaches the
 * guest verdict and is never mistaken for no-result. */
static void unreadable_files(void) {
  t64_observation o;
  t64_verdict v;
  memset(&o, 0, sizeof o);
  o.fixture = T64_FIX_OK;
  o.launched = 1;
  o.wait_state = T64_WAIT_EXITED;
  o.report_len = 50;
  o.stop = SH;
  o.guest_tick = 40;
  o.capture = (const uint8_t *)OKCAP;
  o.capture_len = sizeof OKCAP - 1u;
  o.log_bytes_total = o.capture_len;
  o.expect_pass = 2;
  o.bound = 1000;
  o.log_limit = 4096;
  t64_classify(&o, &v);
  T64_CHECK_INT(v.detail, T64_D_POSITIVE_MATCH); /* the baseline passes */
  o.capture_unreadable = 1;
  t64_classify(&o, &v);
  T64_CHECK_INT(v.detail, T64_D_REPORT_INVALID);
  T64_CHECK_INT(v.guest_data, 0);
  /* An unreadable capture with nothing read is still not boot-fail. */
  o.capture = NULL;
  o.capture_len = 0;
  o.log_bytes_total = 0;
  t64_classify(&o, &v);
  T64_CHECK_INT(v.detail, T64_D_REPORT_INVALID);
  /* A report path that is not a regular file is report-invalid, even though
   * nothing could be read from it (report_len 0 would be no-result). */
  memset(&o, 0, sizeof o);
  o.fixture = T64_FIX_OK;
  o.launched = 1;
  o.wait_state = T64_WAIT_EXITED;
  o.report_unreadable = 1;
  o.bound = 1000;
  o.log_limit = 4096;
  t64_classify(&o, &v);
  T64_CHECK_INT(v.detail, T64_D_REPORT_INVALID);
  T64_CHECK_INT(v.exit_code, 1);
  /* Process failures still win over the report. */
  o.exit_code = 3;
  t64_classify(&o, &v);
  T64_CHECK_INT(v.detail, T64_D_RUNNER_EXIT_NONZERO);
}

#define V1 "t64-report 1\n"
static void report_parser(void) {
  static const char *const good[] = {
      V1 "stop_reason guest-halt\nguest_tick 40\nlog_bytes_total 27\nend\n",
      V1 "stop_reason guest-bound\nguest_tick 0\nlog_bytes_total 0\nend\n",
      V1 "stop_reason guest-bound\nguest_tick 18446744073709551615\n"
         "log_bytes_total 9007199254740991\nend\n",
  };
  static const char *const bad[] = {
      "",
      V1 "stop_reason guest-halt\nguest_tick 40\nlog_bytes_total 27\nend",
      V1 "stop_reason guest-halt\nguest_tick 40\nlog_bytes_total 27\nend\nx\n",
      V1 "stop_reason guest-halt\nguest_tick 40\nlog_bytes_total 27\nend\n\n",
      "t64-report 2\nstop_reason guest-halt\nguest_tick 40\nlog_bytes_total "
      "27\nend\n",
      V1 "stop_reason guest-stop\nguest_tick 40\nlog_bytes_total 27\nend\n",
      V1 "stop_reason guest-halt \nguest_tick 40\nlog_bytes_total 27\nend\n",
      V1 "stop_reason guest-halt\nguest_tick 040\nlog_bytes_total 27\nend\n",
      V1 "stop_reason guest-halt\nguest_tick -1\nlog_bytes_total 27\nend\n",
      V1 "stop_reason guest-halt\nguest_tick \nlog_bytes_total 27\nend\n",
      V1 "stop_reason guest-halt\nguest_tick 18446744073709551616\n"
         "log_bytes_total 27\nend\n",
      V1 "stop_reason guest-halt\nguest_tick 40\n"
         "log_bytes_total 9007199254740992\nend\n",
      V1 "stop_reason guest-halt\r\nguest_tick 40\nlog_bytes_total 27\nend\n",
      V1 "guest_tick 40\nstop_reason guest-halt\nlog_bytes_total 27\nend\n",
      V1 "stop_reason guest-halt\nguest_tick 40\nend\n",
  };
  size_t i;
  t64_report r;
  char big[700];
  for (i = 0; i < sizeof good / sizeof good[0]; i++) {
    T64_CHECK(t64_parse_report((const uint8_t *)good[i], strlen(good[i]), &r) ==
              1);
  }
  for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
    int ok = t64_parse_report((const uint8_t *)bad[i], strlen(bad[i]), &r);
    if (ok) {
      fprintf(stderr, "bad report %lu accepted\n", (unsigned long)i);
    }
    T64_CHECK(ok == 0);
  }
  T64_CHECK(t64_parse_report((const uint8_t *)good[0], strlen(good[0]), &r) ==
            1);
  T64_CHECK_INT(r.stop, SH);
  T64_CHECK_INT(r.guest_tick, 40);
  T64_CHECK_INT(r.log_bytes_total, 27);
  /* More than 512 bytes is invalid even when every line is well formed. */
  memset(big, 'x', sizeof big);
  memcpy(big, V1, sizeof V1 - 1u);
  big[sizeof big - 1u] = '\n';
  T64_CHECK(t64_parse_report((const uint8_t *)big, sizeof big, &r) == 0);
  T64_CHECK(t64_parse_report(NULL, 0, &r) == 0);
}

int main(void) {
  run_rows();
  names_and_exits();
  counts();
  unreadable_files();
  report_parser();
  return T64_TEST_FINISH("unit.classify");
}
