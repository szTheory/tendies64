/*
 * t64-runner-synthetic: a scripted synthetic guest behind the facade, plus the
 * drive loop and the report writer (authored here).
 *
 * Usage (the supervisor appends everything after --slice):
 *   t64-runner-synthetic --slice TICKS --fixture PATH --bound TICKS
 *                        --log-limit BYTES --report PATH --capture PATH
 *                        [--ignore-log-limit]
 *
 * --ignore-log-limit (test-only) writes the capture past the log limit, so a
 * flood control can prove the supervisor's RLIMIT_FSIZE backstop.
 *
 * Exit codes: 0 report written, 2 usage or script grammar violation (no
 * report), 3 I/O failure. See docs/spec/result-protocol.md for the grammar.
 */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "t64_num.h"
#include "tendies64/t64_facade.h"

#define SCRIPT_MAX_BYTES (1u << 20)
#define SCRIPT_MAX_LINE 4096u
#define REPORT_MAX_BYTES 512u
#define DRAIN_CHUNK 4096u
#define LOGREP_MAX_BYTES (64ull << 20)

/* One log record: len bytes at off, emitted rep times back to back (rep is 1
 * except for logrep). */
typedef struct {
  uint64_t tick;
  size_t off;
  size_t len;
  uint64_t rep;
} syn_event;

/* The last four host-misbehavior kinds act inside advance at the end tick;
 * EXIT7 behaves like quiesce and then exits 7 after the report is written. */
typedef enum {
  SYN_END_QUIESCE = 1,
  SYN_END_SPIN = 2,
  SYN_END_HANG = 3,
  SYN_END_ABORT = 4,
  SYN_END_SEGV = 5,
  SYN_END_EXIT0 = 6,
  SYN_END_EXIT7 = 7
} syn_end_kind;

typedef struct {
  uint8_t *data;
  size_t data_len;
  syn_event *ev;
  size_t nev;
  uint64_t end_tick;
  syn_end_kind end;
  /* run state, all instance-local */
  uint64_t tick;
  int halted;
  uint64_t halt_tick;
  size_t avail_ev;
  size_t drain_ev;
  uint64_t drain_off;
} syn_guest;

static int hexval(int c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

/* ---- the guest, behind the facade ------------------------------------- */

static void syn_publish(syn_guest *g) {
  while (g->avail_ev < g->nev && g->ev[g->avail_ev].tick <= g->tick) {
    g->avail_ev++;
  }
}

/* Host misbehavior, deliberately hostile (negative controls). Never returns. */
static void syn_misbehave(syn_end_kind k) {
  if (k == SYN_END_HANG) {
    struct timespec nap = {0, 50 * 1000 * 1000};
    for (;;) {
      nanosleep(&nap, NULL);
    }
  }
  if (k == SYN_END_ABORT) {
    abort();
  }
  if (k == SYN_END_SEGV) {
    raise(SIGSEGV);
  }
  _exit(k == SYN_END_EXIT0 ? 0 : 3);
}

static t64_advance_result syn_advance(void *inst, uint64_t bound_tick) {
  syn_guest *g = (syn_guest *)inst;
  t64_advance_result r;
  if (g->halted) {
    r.reason = T64_STOP_HALT;
    r.tick = g->halt_tick;
    return r;
  }
  if (bound_tick < g->tick) {
    bound_tick = g->tick;
  }
  if (g->end_tick <= bound_tick && g->end >= SYN_END_HANG &&
      g->end <= SYN_END_EXIT0) {
    syn_misbehave(g->end);
  }
  if ((g->end == SYN_END_QUIESCE || g->end == SYN_END_EXIT7) &&
      g->end_tick <= bound_tick) {
    g->tick = g->end_tick;
    g->halted = 1;
    g->halt_tick = g->end_tick;
    syn_publish(g);
    r.reason = T64_STOP_HALT;
    r.tick = g->halt_tick;
    return r;
  }
  g->tick = bound_tick;
  syn_publish(g);
  r.reason = T64_STOP_BOUND;
  r.tick = bound_tick;
  return r;
}

static size_t syn_drain(void *inst, uint64_t *tick_out, uint8_t *buf,
                        size_t cap) {
  syn_guest *g = (syn_guest *)inst;
  const syn_event *e;
  size_t n;
  if (cap == 0 || g->drain_ev >= g->avail_ev) {
    return 0;
  }
  e = &g->ev[g->drain_ev];
  {
    uint64_t total = (uint64_t)e->len * e->rep;
    uint64_t left = total - g->drain_off;
    size_t i;
    n = (left > (uint64_t)cap) ? cap : (size_t)left;
    for (i = 0; i < n; i++) {
      buf[i] = g->data[e->off + (size_t)((g->drain_off + i) % e->len)];
    }
    *tick_out = e->tick;
    g->drain_off += n;
    if (g->drain_off == total) {
      g->drain_ev++;
      g->drain_off = 0;
    }
  }
  return n;
}

static const t64_facade_ops syn_ops = {syn_advance, syn_drain};

/* ---- script parsing ---------------------------------------------------- */

static int syn_fail(const char *why, size_t lineno) {
  fprintf(stderr, "t64-runner-synthetic: script error line %lu: %s\n",
          (unsigned long)lineno, why);
  return -1;
}

/* Decode "log" text escapes into dst; returns decoded length or -1. */
static long syn_decode_text(const char *s, size_t n, uint8_t *dst) {
  size_t i = 0;
  size_t o = 0;
  while (i < n) {
    if (s[i] != '\\') {
      dst[o++] = (uint8_t)s[i++];
      continue;
    }
    if (i + 1 >= n) {
      return -1;
    }
    if (s[i + 1] == 'n') {
      dst[o++] = '\n';
      i += 2;
    } else if (s[i + 1] == '\\') {
      dst[o++] = '\\';
      i += 2;
    } else if (s[i + 1] == 'x') {
      int hi;
      int lo;
      if (i + 3 >= n) {
        return -1;
      }
      hi = hexval((unsigned char)s[i + 2]);
      lo = hexval((unsigned char)s[i + 3]);
      if (hi < 0 || lo < 0) {
        return -1;
      }
      dst[o++] = (uint8_t)(hi * 16 + lo);
      i += 4;
    } else {
      return -1;
    }
  }
  return (long)o;
}

/* Splits "<tick> rest" at the first space. Returns the rest offset or 0. */
static size_t syn_take_tick(const char *s, size_t n, uint64_t *tick) {
  size_t i = 0;
  while (i < n && s[i] != ' ') {
    i++;
  }
  if (i == n || !t64_parse_u64(s, i, tick)) {
    return 0;
  }
  return i + 1;
}

static int syn_parse(const uint8_t *buf, size_t len, syn_guest *g) {
  size_t pos = 0;
  size_t lineno = 0;
  size_t max_ev = 0;
  uint64_t last_tick = 0;
  int have_end = 0;
  size_t i;
  /* upper bound for the event table: one event per LF-terminated line */
  for (i = 0; i < len; i++) {
    if (buf[i] == '\n') {
      max_ev++;
    }
  }
  max_ev += 1;
  g->ev = (syn_event *)calloc(max_ev, sizeof(syn_event));
  g->data = (uint8_t *)malloc(len + 1u);
  if (g->ev == NULL || g->data == NULL) {
    return syn_fail("out of memory", 0);
  }
  while (pos < len) {
    size_t ls = pos;
    size_t le = pos;
    const char *line;
    size_t n;
    while (le < len && buf[le] != '\n') {
      le++;
    }
    pos = (le < len) ? le + 1 : le;
    lineno++;
    n = le - ls;
    line = (const char *)(buf + ls);
    if (n > SCRIPT_MAX_LINE) {
      return syn_fail("line longer than 4096 bytes", lineno);
    }
    if (lineno == 1) {
      if (n != 15 || memcmp(line, "t64-synthetic 1", 15) != 0) {
        return syn_fail("line 1 must be exactly 't64-synthetic 1'", lineno);
      }
      continue;
    }
    if (n == 0 || line[0] == '#') {
      continue;
    }
    if (have_end) {
      return syn_fail("content after the end line", lineno);
    }
    if (n >= 4 && memcmp(line, "log ", 4) == 0) {
      uint64_t tick;
      size_t rest = syn_take_tick(line + 4, n - 4, &tick);
      long dl;
      if (rest == 0 || tick < last_tick) {
        return syn_fail("bad or decreasing tick", lineno);
      }
      dl = syn_decode_text(line + 4 + rest, n - 4 - rest,
                           g->data + g->data_len);
      if (dl <= 0) {
        return syn_fail("bad or empty log text", lineno);
      }
      g->ev[g->nev].tick = tick;
      g->ev[g->nev].off = g->data_len;
      g->ev[g->nev].len = (size_t)dl;
      g->ev[g->nev].rep = 1;
      g->nev++;
      g->data_len += (size_t)dl;
      last_tick = tick;
    } else if (n >= 7 && memcmp(line, "logrep ", 7) == 0) {
      uint64_t tick;
      uint64_t count;
      size_t rest = syn_take_tick(line + 7, n - 7, &tick);
      size_t rest2;
      long dl;
      if (rest == 0 || tick < last_tick) {
        return syn_fail("bad or decreasing tick", lineno);
      }
      rest2 = syn_take_tick(line + 7 + rest, n - 7 - rest, &count);
      if (rest2 == 0 || count == 0) {
        return syn_fail("bad logrep count", lineno);
      }
      dl = syn_decode_text(line + 7 + rest + rest2, n - 7 - rest - rest2,
                           g->data + g->data_len);
      if (dl <= 0) {
        return syn_fail("bad or empty log text", lineno);
      }
      if (count > LOGREP_MAX_BYTES / (uint64_t)dl) {
        return syn_fail("logrep total above 64 MiB", lineno);
      }
      g->ev[g->nev].tick = tick;
      g->ev[g->nev].off = g->data_len;
      g->ev[g->nev].len = (size_t)dl;
      g->ev[g->nev].rep = count;
      g->nev++;
      g->data_len += (size_t)dl;
      last_tick = tick;
    } else if (n >= 5 && memcmp(line, "logx ", 5) == 0) {
      uint64_t tick;
      size_t rest = syn_take_tick(line + 5, n - 5, &tick);
      size_t p;
      size_t cnt = 0;
      const char *t;
      size_t tn;
      if (rest == 0 || tick < last_tick) {
        return syn_fail("bad or decreasing tick", lineno);
      }
      t = line + 5 + rest;
      tn = n - 5 - rest;
      p = 0;
      while (p < tn) {
        int hi;
        int lo;
        if (p + 2 > tn) {
          return syn_fail("bad hex byte", lineno);
        }
        hi = hexval((unsigned char)t[p]);
        lo = hexval((unsigned char)t[p + 1]);
        if (hi < 0 || lo < 0) {
          return syn_fail("bad hex byte", lineno);
        }
        g->data[g->data_len + cnt++] = (uint8_t)(hi * 16 + lo);
        p += 2;
        if (p < tn) {
          if (t[p] != ' ') {
            return syn_fail("hex bytes must be separated by a space", lineno);
          }
          p++;
          if (p == tn) {
            return syn_fail("trailing space", lineno);
          }
        }
      }
      if (cnt == 0) {
        return syn_fail("empty logx", lineno);
      }
      g->ev[g->nev].tick = tick;
      g->ev[g->nev].off = g->data_len;
      g->ev[g->nev].len = cnt;
      g->ev[g->nev].rep = 1;
      g->nev++;
      g->data_len += cnt;
      last_tick = tick;
    } else if (n >= 4 && memcmp(line, "end ", 4) == 0) {
      uint64_t tick;
      size_t rest = syn_take_tick(line + 4, n - 4, &tick);
      const char *d;
      size_t dn;
      if (rest == 0 || tick < last_tick) {
        return syn_fail("bad or decreasing tick", lineno);
      }
      d = line + 4 + rest;
      dn = n - 4 - rest;
      if (dn == 7 && memcmp(d, "quiesce", 7) == 0) {
        g->end = SYN_END_QUIESCE;
      } else if (dn == 4 && memcmp(d, "spin", 4) == 0) {
        g->end = SYN_END_SPIN;
      } else if (dn == 4 && memcmp(d, "hang", 4) == 0) {
        g->end = SYN_END_HANG;
      } else if (dn == 5 && memcmp(d, "abort", 5) == 0) {
        g->end = SYN_END_ABORT;
      } else if (dn == 4 && memcmp(d, "segv", 4) == 0) {
        g->end = SYN_END_SEGV;
      } else if (dn == 15 && memcmp(d, "exit0-no-result", 15) == 0) {
        g->end = SYN_END_EXIT0;
      } else if (dn == 5 && memcmp(d, "exit7", 5) == 0) {
        g->end = SYN_END_EXIT7;
      } else {
        return syn_fail("unknown end directive", lineno);
      }
      g->end_tick = tick;
      have_end = 1;
    } else {
      return syn_fail("unknown line", lineno);
    }
  }
  if (lineno == 0) {
    return syn_fail("empty script", 0);
  }
  if (!have_end) {
    return syn_fail("missing end line", lineno);
  }
  return 0;
}

static int syn_load(const char *path, syn_guest *g) {
  struct stat st;
  uint8_t *buf;
  size_t got = 0;
  int fd = open(path, O_RDONLY | O_NOFOLLOW);
  int rc;
  if (fd < 0) {
    return syn_fail("cannot open the fixture", 0);
  }
  if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0 ||
      (unsigned long long)st.st_size > SCRIPT_MAX_BYTES) {
    close(fd);
    return syn_fail("fixture is not a regular file of at most 1 MiB", 0);
  }
  buf = (uint8_t *)malloc((size_t)st.st_size + 1u);
  if (buf == NULL) {
    close(fd);
    return syn_fail("out of memory", 0);
  }
  while (got < (size_t)st.st_size) {
    ssize_t r = read(fd, buf + got, (size_t)st.st_size - got);
    if (r < 0 && errno == EINTR) {
      continue;
    }
    if (r <= 0) {
      break;
    }
    got += (size_t)r;
  }
  close(fd);
  if (got != (size_t)st.st_size) {
    free(buf);
    return syn_fail("short read of the fixture", 0);
  }
  rc = syn_parse(buf, got, g);
  free(buf);
  return rc;
}

/* ---- drive loop and report -------------------------------------------- */

static int write_all(int fd, const void *p, size_t n) {
  const uint8_t *b = (const uint8_t *)p;
  while (n > 0) {
    ssize_t r = write(fd, b, n);
    if (r < 0 && errno == EINTR) {
      continue;
    }
    if (r <= 0) {
      return -1;
    }
    b += r;
    n -= (size_t)r;
  }
  return 0;
}

typedef struct {
  const char *fixture;
  const char *report;
  const char *capture;
  uint64_t slice;
  uint64_t bound;
  uint64_t log_limit;
  int ignore_log_limit; /* test-only: write past the log limit */
} syn_args;

static int parse_args(int argc, char **argv, syn_args *a) {
  int seen = 0;
  int i;
  memset(a, 0, sizeof *a);
  for (i = 1; i < argc; i += 2) {
    const char *k = argv[i];
    const char *v;
    uint64_t *np = NULL;
    const char **sp = NULL;
    int bit;
    if (strcmp(k, "--ignore-log-limit") == 0) {
      if (a->ignore_log_limit) {
        return -1;
      }
      a->ignore_log_limit = 1;
      i--; /* a flag takes no value: the loop's i += 2 nets one step */
      continue;
    }
    if (i + 1 >= argc) {
      return -1;
    }
    v = argv[i + 1];
    if (strcmp(k, "--slice") == 0) {
      np = &a->slice;
      bit = 1;
    } else if (strcmp(k, "--bound") == 0) {
      np = &a->bound;
      bit = 2;
    } else if (strcmp(k, "--log-limit") == 0) {
      np = &a->log_limit;
      bit = 4;
    } else if (strcmp(k, "--fixture") == 0) {
      sp = &a->fixture;
      bit = 8;
    } else if (strcmp(k, "--report") == 0) {
      sp = &a->report;
      bit = 16;
    } else if (strcmp(k, "--capture") == 0) {
      sp = &a->capture;
      bit = 32;
    } else {
      return -1;
    }
    if (seen & bit) {
      return -1;
    }
    seen |= bit;
    if (np != NULL && !t64_parse_u64(v, strlen(v), np)) {
      return -1;
    }
    if (sp != NULL) {
      *sp = v;
    }
  }
  if (i != argc || seen != 63 || a->slice == 0) {
    return -1;
  }
  return 0;
}

int main(int argc, char **argv) {
  syn_args a;
  syn_guest g;
  uint8_t chunk[DRAIN_CHUNK];
  uint64_t tick = 0;
  uint64_t total = 0;
  uint64_t written = 0;
  const char *stop = "guest-bound";
  char rep[REPORT_MAX_BYTES];
  int rn;
  int cfd;
  int rfd;

  if (parse_args(argc, argv, &a) != 0) {
    fprintf(stderr, "t64-runner-synthetic: usage error\n");
    return 2;
  }
  memset(&g, 0, sizeof g);
  if (syn_load(a.fixture, &g) != 0) {
    return 2;
  }
  cfd = open(a.capture, O_WRONLY | O_TRUNC | O_NOFOLLOW);
  if (cfd < 0) {
    fprintf(stderr, "t64-runner-synthetic: cannot open capture\n");
    return 3;
  }
  for (;;) {
    uint64_t target = (a.slice > a.bound - tick) ? a.bound : tick + a.slice;
    t64_advance_result r = syn_ops.advance(&g, target);
    size_t n;
    uint64_t rec_tick;
    tick = r.tick;
    while ((n = syn_ops.drain_guest_log(&g, &rec_tick, chunk, sizeof chunk)) >
           0) {
      uint64_t room = (written < a.log_limit) ? a.log_limit - written : 0;
      if (a.ignore_log_limit) {
        room = n;
      }
      size_t w = (n > room) ? (size_t)room : n;
      total += n;
      if (w > 0) {
        if (write_all(cfd, chunk, w) != 0) {
          close(cfd);
          return 3;
        }
        written += w;
      }
    }
    if (r.reason == T64_STOP_HALT) {
      stop = "guest-halt";
      break;
    }
    if (tick >= a.bound) {
      break;
    }
  }
  if (close(cfd) != 0) {
    return 3;
  }
  rn = snprintf(rep, sizeof rep,
                "t64-report 1\nstop_reason %s\nguest_tick %llu\n"
                "log_bytes_total %llu\nend\n",
                stop, (unsigned long long)tick, (unsigned long long)total);
  if (rn <= 0 || (size_t)rn >= sizeof rep) {
    return 3;
  }
  rfd = open(a.report, O_WRONLY | O_TRUNC | O_NOFOLLOW);
  if (rfd < 0 || write_all(rfd, rep, (size_t)rn) != 0) {
    if (rfd >= 0) {
      close(rfd);
    }
    return 3;
  }
  if (close(rfd) != 0) {
    return 3;
  }
  free(g.ev);
  free(g.data);
  return g.end == SYN_END_EXIT7 ? 7 : 0;
}
