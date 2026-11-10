/* Six-outcome precedence classifier and report parser. See t64_classify.h. */
#include "t64_classify.h"

#include <string.h>

#include "t64_num.h"
#include "t64_utf8.h"

static const struct {
  const char *name;
  t64_outcome outcome;
} detail_table[T64_D_COUNT_] = {
    {"usage", T64_OUT_HARNESS_ERROR},
    {"exec-failed", T64_OUT_HARNESS_ERROR},
    {"supervisor-signalled", T64_OUT_HARNESS_ERROR},
    {"wall-backstop", T64_OUT_HARNESS_ERROR},
    {"runner-signalled", T64_OUT_HARNESS_ERROR},
    {"runner-exit-nonzero", T64_OUT_HARNESS_ERROR},
    {"no-result", T64_OUT_HARNESS_ERROR},
    {"report-invalid", T64_OUT_HARNESS_ERROR},
    {"fixture-digest-mismatch", T64_OUT_HARNESS_ERROR},
    {"inventory-mismatch", T64_OUT_HARNESS_ERROR},
    {"suite-truncated", T64_OUT_HARNESS_ERROR},
    {"unclassified", T64_OUT_HARNESS_ERROR},
    {"fixture-absent", T64_OUT_MISSING_FIXTURE},
    {"log-over-limit", T64_OUT_GUEST_FAIL},
    {"log-invalid-utf8", T64_OUT_GUEST_FAIL},
    {"fail-reported", T64_OUT_GUEST_FAIL},
    {"output-after-done", T64_OUT_GUEST_FAIL},
    {"count-mismatch", T64_OUT_GUEST_FAIL},
    {"verdict-incomplete", T64_OUT_GUEST_FAIL},
    {"empty-output", T64_OUT_BOOT_FAIL},
    {"guest-bound", T64_OUT_TIMEOUT},
    {"positive-match", T64_OUT_PASS},
};

static const char *const outcome_names[] = {
    "pass", "harness-error", "missing-fixture", "guest-fail", "boot-fail",
    "timeout"};
static const int outcome_exits[] = {0, 1, 13, 10, 11, 12};

const char *t64_outcome_name(t64_outcome o) {
  if ((int)o < 0 || (int)o > (int)T64_OUT_TIMEOUT) {
    return NULL;
  }
  return outcome_names[o];
}

int t64_outcome_exit(t64_outcome o) {
  if ((int)o < 0 || (int)o > (int)T64_OUT_TIMEOUT) {
    return 1;
  }
  return outcome_exits[o];
}

const char *t64_detail_name(t64_detail d) {
  if ((int)d < 0 || (int)d >= (int)T64_D_COUNT_) {
    return NULL;
  }
  return detail_table[d].name;
}

t64_outcome t64_detail_outcome(t64_detail d) {
  if ((int)d < 0 || (int)d >= (int)T64_D_COUNT_) {
    return T64_OUT_HARNESS_ERROR;
  }
  return detail_table[d].outcome;
}

/* ---- report parser ------------------------------------------------------- */

static int report_line(const uint8_t *b, size_t len, size_t *pos,
                       const char **s, size_t *n) {
  size_t i = *pos;
  while (i < len && b[i] != '\n') {
    if (b[i] == '\r') {
      return 0;
    }
    i++;
  }
  if (i >= len) {
    return 0; /* every line must be LF-terminated */
  }
  *s = (const char *)(b + *pos);
  *n = i - *pos;
  *pos = i + 1;
  return 1;
}

static int has_prefix(const char *s, size_t n, const char *prefix) {
  size_t pl = strlen(prefix);
  return n >= pl && memcmp(s, prefix, pl) == 0;
}

int t64_parse_report(const uint8_t *buf, size_t len, t64_report *out) {
  return t64_parse_report_max(buf, len, T64_REPORT_MAX_BYTES, out);
}

int t64_parse_report_max(const uint8_t *buf, size_t len, size_t max_len,
                         t64_report *out) {
  size_t pos = 0;
  const char *s;
  size_t n;
  t64_report r;
  if (buf == NULL || len == 0 || len > max_len) {
    return 0;
  }
  if (!report_line(buf, len, &pos, &s, &n) || n != 12 ||
      memcmp(s, "t64-report 1", 12) != 0) {
    return 0;
  }
  if (!report_line(buf, len, &pos, &s, &n) || !has_prefix(s, n, "stop_reason ")) {
    return 0;
  }
  if (n - 12 == 11 && memcmp(s + 12, "guest-bound", 11) == 0) {
    r.stop = T64_REPORT_STOP_BOUND;
  } else if (n - 12 == 10 && memcmp(s + 12, "guest-halt", 10) == 0) {
    r.stop = T64_REPORT_STOP_HALT;
  } else {
    return 0;
  }
  if (!report_line(buf, len, &pos, &s, &n) || !has_prefix(s, n, "guest_tick ") ||
      !t64_parse_u64(s + 11, n - 11, &r.guest_tick)) {
    return 0;
  }
  if (!report_line(buf, len, &pos, &s, &n) ||
      !has_prefix(s, n, "log_bytes_total ") ||
      !t64_parse_u64(s + 16, n - 16, &r.log_bytes_total) ||
      r.log_bytes_total > T64_MAX_SAFE_INT) {
    return 0;
  }
  if (!report_line(buf, len, &pos, &s, &n) || n != 3 ||
      memcmp(s, "end", 3) != 0 || pos != len) {
    return 0;
  }
  *out = r;
  return 1;
}

/* ---- line analysis --------------------------------------------------------- */

static int line_has_unhandled(const uint8_t *p, size_t n) {
  static const char pat[] = "unhandled exception";
  size_t plen = sizeof pat - 1u;
  size_t i;
  size_t j;
  if (n < plen) {
    return 0;
  }
  for (i = 0; i + plen <= n; i++) {
    for (j = 0; j < plen; j++) {
      int c = p[i + j];
      if (c >= 'A' && c <= 'Z') {
        c += 'a' - 'A';
      }
      if (c != pat[j]) {
        break;
      }
    }
    if (j == plen) {
      return 1;
    }
  }
  return 0;
}

typedef struct {
  int done_seen;
  int output_after_done;
} line_facts;

/* Lines split on LF; a final unterminated line counts as a line. */
static void analyze(const t64_observation *o, t64_counts *c, line_facts *f) {
  size_t pos = 0;
  memset(c, 0, sizeof *c);
  memset(f, 0, sizeof *f);
  c->log_bytes = o->capture_len;
  c->log_bytes_total = o->log_bytes_total;
  while (pos < o->capture_len) {
    const uint8_t *p = o->capture + pos;
    size_t n = 0;
    int unhandled;
    while (pos + n < o->capture_len && p[n] != '\n') {
      n++;
    }
    pos += n + ((pos + n < o->capture_len) ? 1u : 0u);
    c->log_lines++;
    unhandled = line_has_unhandled(p, n);
    if (n >= 5 && memcmp(p, "PASS ", 5) == 0) {
      c->pass_reported++;
    }
    if (n >= 5 && memcmp(p, "FAIL ", 5) == 0) {
      c->fail_reported++;
    }
    if (unhandled) {
      c->unhandled_exception_lines++;
    }
    if (f->done_seen && !unhandled) {
      f->output_after_done = 1;
    }
    if (!f->done_seen && n == 5 && memcmp(p, "Done!", 5) == 0) {
      f->done_seen = 1;
    }
  }
}

static void set(t64_verdict *v, t64_detail d) {
  v->detail = d;
  v->outcome = t64_detail_outcome(d);
  v->exit_code = t64_outcome_exit(v->outcome);
}

/* For an honest runner the capture holds exactly the first
 * min(log_bytes_total, log_limit) bytes the guest emitted (WR-02). A shorter
 * capture means bytes were lost (a read error, a runner that did not flush),
 * and a verdict computed on it could miss a dropped FAIL line. */
static uint64_t expected_capture_len(const t64_observation *o) {
  return o->log_bytes_total < o->log_limit ? o->log_bytes_total : o->log_limit;
}

static int report_invalid(const t64_observation *o) {
  return o->report_malformed || o->capture_unreadable ||
         o->stop == T64_REPORT_STOP_NONE || o->guest_tick > o->bound ||
         (o->stop == T64_REPORT_STOP_BOUND && o->guest_tick != o->bound) ||
         o->capture_len > o->log_limit || o->log_bytes_total < o->capture_len ||
         (uint64_t)o->capture_len != expected_capture_len(o);
}

void t64_classify(const t64_observation *o, t64_verdict *v) {
  line_facts f;
  memset(v, 0, sizeof *v);

  /* 1. harness-error: process and protocol failures, in table order. */
  if (o->usage_error || o->fixture == T64_FIX_BAD) {
    set(v, T64_D_USAGE);
    return;
  }
  if (o->exec_failed) {
    set(v, T64_D_EXEC_FAILED);
    return;
  }
  if (o->supervisor_signalled) {
    set(v, T64_D_SUPERVISOR_SIGNALLED);
    return;
  }
  if (o->wall_backstop) {
    set(v, T64_D_WALL_BACKSTOP);
    return;
  }
  if (o->launched) {
    if (o->wait_state == T64_WAIT_SIGNALLED) {
      set(v, T64_D_RUNNER_SIGNALLED);
      return;
    }
    if (o->wait_state != T64_WAIT_EXITED) {
      set(v, T64_D_UNCLASSIFIED);
      return;
    }
    if (o->exit_code != 0) {
      set(v, T64_D_RUNNER_EXIT_NONZERO);
      return;
    }
    if (o->report_unreadable) {
      set(v, T64_D_REPORT_INVALID);
      return;
    }
    if (o->report_len == 0) {
      set(v, T64_D_NO_RESULT);
      return;
    }
    if (report_invalid(o)) {
      set(v, T64_D_REPORT_INVALID);
      return;
    }
  }
  if (o->fixture == T64_FIX_DIGEST_MISMATCH) {
    set(v, T64_D_FIXTURE_DIGEST_MISMATCH);
    return;
  }

  /* 2. missing-fixture. */
  if (o->fixture == T64_FIX_ABSENT) {
    set(v, T64_D_FIXTURE_ABSENT);
    return;
  }
  if (o->fixture != T64_FIX_OK || !o->launched) {
    set(v, T64_D_UNCLASSIFIED);
    return;
  }

  /* Guest outcomes: the run is complete and the report is consistent. */
  v->guest_data = 1;
  analyze(o, &v->counts, &f);

  /* 3. guest-fail, in order. */
  if (o->log_bytes_total > o->log_limit) {
    set(v, T64_D_LOG_OVER_LIMIT);
    return;
  }
  if (!t64_utf8_valid(o->capture != NULL ? o->capture : (const uint8_t *)"",
                      o->capture_len, NULL)) {
    set(v, T64_D_LOG_INVALID_UTF8);
    return;
  }
  if (v->counts.fail_reported > 0) {
    set(v, T64_D_FAIL_REPORTED);
    return;
  }
  if (f.output_after_done) {
    set(v, T64_D_OUTPUT_AFTER_DONE);
    return;
  }
  if (f.done_seen && v->counts.pass_reported != o->expect_pass) {
    set(v, T64_D_COUNT_MISMATCH);
    return;
  }
  if (o->stop == T64_REPORT_STOP_HALT && o->capture_len > 0 && !f.done_seen) {
    set(v, T64_D_VERDICT_INCOMPLETE);
    return;
  }

  /* 4. boot-fail. */
  if (o->capture_len == 0) {
    set(v, T64_D_EMPTY_OUTPUT);
    return;
  }

  /* 5. timeout: the guest-time bound was reached without Done!. */
  if (!f.done_seen) {
    set(v, T64_D_GUEST_BOUND);
    return;
  }

  /* 6. pass, only on a positive match. */
  set(v, T64_D_POSITIVE_MATCH);
}
