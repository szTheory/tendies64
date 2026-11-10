/*
 * t64-supervisor: runs one runner under a wall-clock backstop, never trusts
 * the runner's exit code, and always writes a canonical t64.result v1 plus a
 * non-canonical run-meta file, both atomically (authored here).
 *
 * See docs/spec/result-protocol.md for the CLI and the result contract.
 */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "t64_classify.h"
#include "t64_jsonw.h"
#include "t64_num.h"
#include "t64_sha256.h"
#include "tendies64/t64_facade.h"

#ifndef T64_BUILD_ID
#define T64_BUILD_ID "0.0.0+synthetic"
#endif

#define FIXTURE_MAX_BYTES (64ull << 20)
#define LOG_LIMIT_MAX (64ull << 20)
#define REPORT_MAX_BYTES 512u
#define FSIZE_SLACK (64ull << 10)
#define PROFILE_DESCRIPTOR                                                     \
  "{\"profile_id\":\"synthetic-stub\",\"schema\":\"t64.profile\","            \
  "\"schema_version\":1}\n"

/* ---- utilities --------------------------------------------------------- */

static volatile sig_atomic_t g_term = 0;

static void on_term(int sig) {
  (void)sig;
  g_term = 1;
}

static uint64_t now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

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

/*
 * Writes a temporary file next to PATH, then renames it over PATH. Returns 0
 * on success. The temporary name comes from mkstemp (PATH.tmp.XXXXXX,
 * created with O_EXCL), so two supervisors writing the same PATH never share
 * a temporary file, and nothing planted at a predictable name (a symlink, a
 * FIFO that would block open) is ever opened (IN-01).
 */
static int atomic_write(const char *path, const void *data, size_t len) {
  char tmp[4096];
  int fd;
  int n = snprintf(tmp, sizeof tmp, "%s.tmp.XXXXXX", path);
  if (n <= 0 || (size_t)n >= sizeof tmp) {
    return -1;
  }
  fd = mkstemp(tmp);
  if (fd < 0) {
    return -1;
  }
  if (fchmod(fd, 0644) != 0 || write_all(fd, data, len) != 0 || fsync(fd) != 0) {
    close(fd);
    unlink(tmp);
    return -1;
  }
  if (close(fd) != 0 || rename(tmp, path) != 0) {
    unlink(tmp);
    return -1;
  }
  return 0;
}

static int is_hex64(const char *s) {
  size_t i;
  if (strlen(s) != 64) {
    return 0;
  }
  for (i = 0; i < 64; i++) {
    if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) {
      return 0;
    }
  }
  return 1;
}

static int valid_fixture_id(const char *s) {
  size_t i;
  size_t n = strlen(s);
  if (n == 0 || n > 64) {
    return 0;
  }
  for (i = 0; i < n; i++) {
    char c = s[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) {
      return 0;
    }
  }
  return 1;
}

/* ---- result writers ----------------------------------------------------- */

#define J(x)                                                                   \
  do {                                                                         \
    if ((x) != T64_JSONW_OK) {                                                 \
      return -1;                                                               \
    }                                                                          \
  } while (0)

static int kv_str(t64_jsonw *w, const char *k, const char *v) {
  J(t64_jsonw_key(w, k, strlen(k)));
  J(t64_jsonw_string(w, v, strlen(v)));
  return 0;
}

static int kv_strn(t64_jsonw *w, const char *k, const char *v) {
  J(t64_jsonw_key(w, k, strlen(k)));
  if (v == NULL) {
    J(t64_jsonw_null(w));
  } else {
    J(t64_jsonw_string(w, v, strlen(v)));
  }
  return 0;
}

static int kv_int(t64_jsonw *w, const char *k, uint64_t v) {
  if (v > T64_MAX_SAFE_INT) {
    return -1;
  }
  J(t64_jsonw_key(w, k, strlen(k)));
  J(t64_jsonw_int(w, (int64_t)v));
  return 0;
}

static int build_result(char *buf, size_t cap, size_t *len, const t64_verdict *v,
                        const char *fixture_id, const char *fixture_digest,
                        const char *log_digest, const char *stop_reason,
                        uint64_t guest_tick, const char *profile_hash) {
  t64_jsonw w;
  char tick[24];
  snprintf(tick, sizeof tick, "%llu", (unsigned long long)guest_tick);
  t64_jsonw_init(&w, buf, cap);
  J(t64_jsonw_begin_object(&w));
  if (kv_str(&w, "build_id", T64_BUILD_ID) != 0) {
    return -1;
  }
  J(t64_jsonw_key(&w, "counts", 6));
  J(t64_jsonw_begin_object(&w));
  if (kv_int(&w, "fail_reported", v->counts.fail_reported) != 0 ||
      kv_int(&w, "log_bytes", v->counts.log_bytes) != 0 ||
      kv_int(&w, "log_bytes_total", v->counts.log_bytes_total) != 0 ||
      kv_int(&w, "log_lines", v->counts.log_lines) != 0 ||
      kv_int(&w, "pass_reported", v->counts.pass_reported) != 0 ||
      kv_int(&w, "unhandled_exception_lines",
             v->counts.unhandled_exception_lines) != 0) {
    return -1;
  }
  J(t64_jsonw_end_object(&w));
  if (kv_str(&w, "detail", t64_detail_name(v->detail)) != 0 ||
      kv_str(&w, "evidence_tier", "synthetic") != 0 ||
      kv_strn(&w, "fixture_digest", fixture_digest) != 0 ||
      kv_str(&w, "fixture_id", fixture_id) != 0 ||
      kv_str(&w, "guest_tick", tick) != 0 ||
      kv_strn(&w, "log_digest", log_digest) != 0 ||
      kv_str(&w, "outcome", t64_outcome_name(v->outcome)) != 0 ||
      kv_str(&w, "profile_hash", profile_hash) != 0 ||
      kv_str(&w, "profile_id", "synthetic-stub") != 0 ||
      kv_str(&w, "schema", "t64.result") != 0 ||
      kv_int(&w, "schema_version", 1) != 0) {
    return -1;
  }
  J(t64_jsonw_key(&w, "state_digest", 12));
  J(t64_jsonw_null(&w));
  if (kv_str(&w, "stop_reason", stop_reason) != 0) {
    return -1;
  }
  J(t64_jsonw_end_object(&w));
  J(t64_jsonw_finish(&w, len));
  return 0;
}

static int build_meta(char *buf, size_t cap, size_t *len, int wait_status,
                      uint64_t wall_ms) {
  t64_jsonw w;
  struct utsname un;
  const char *os = "unknown";
  if (uname(&un) == 0) {
    os = un.sysname;
  }
  t64_jsonw_init(&w, buf, cap);
  J(t64_jsonw_begin_object(&w));
  if (kv_str(&w, "core_identity", t64_core_identity()) != 0 ||
      kv_str(&w, "host_os", os) != 0) {
    return -1;
  }
  J(t64_jsonw_key(&w, "wait_status", 11));
  J(t64_jsonw_int(&w, (int64_t)wait_status));
  if (kv_int(&w, "wall_ms", wall_ms) != 0) {
    return -1;
  }
  J(t64_jsonw_end_object(&w));
  J(t64_jsonw_finish(&w, len));
  return 0;
}

/* ---- file helpers for the private work dir ----------------------------- */

static int create_private(const char *path) {
  int fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
  return fd;
}

/*
 * Opens a file the runner may have replaced (WR-01). O_NONBLOCK keeps a
 * planted FIFO from blocking open() after the wall-clock backstop no longer
 * applies, O_NOFOLLOW refuses a symlink, and fstat must then show a regular
 * file. Returns the descriptor, -2 when the path does not exist, or -1 when
 * it exists but is unusable.
 */
static int open_runner_file(const char *path, struct stat *st) {
  int fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
  if (fd < 0) {
    return errno == ENOENT ? -2 : -1;
  }
  if (fstat(fd, st) != 0 || !S_ISREG(st->st_mode)) {
    close(fd);
    return -1;
  }
  return fd;
}

/* Reads up to cap bytes of the runner's report. Returns the bytes read, 0
 * when the file is missing or empty, or -1 when the path is not a regular
 * file or a read failed. */
static long read_report(const char *path, uint8_t *buf, size_t cap) {
  struct stat st;
  size_t got = 0;
  int fd = open_runner_file(path, &st);
  if (fd == -2) {
    return 0;
  }
  if (fd < 0) {
    return -1;
  }
  while (got < cap) {
    ssize_t r = read(fd, buf + got, cap - got);
    if (r < 0 && errno == EINTR) {
      continue;
    }
    if (r < 0) {
      close(fd);
      return -1;
    }
    if (r == 0) {
      break;
    }
    got += (size_t)r;
  }
  close(fd);
  return (long)got;
}

/*
 * Reads the capture: the whole file, or log_limit+1 bytes when it is longer
 * (so an over-long capture is still seen as one). Returns 0 and fills *out
 * and *out_len (*out may be NULL for an empty file), or -1 when the capture
 * is missing, not a regular file, could not be allocated or was not read in
 * full (WR-02: never an empty or short capture that could pass).
 */
static int read_capture(const char *path, uint64_t log_limit, uint8_t **out,
                        size_t *out_len) {
  struct stat st;
  size_t want;
  size_t got = 0;
  uint8_t *buf;
  int fd = open_runner_file(path, &st);
  *out = NULL;
  *out_len = 0;
  if (fd < 0) {
    return -1;
  }
  if (st.st_size < 0) {
    close(fd);
    return -1;
  }
  want = (uint64_t)st.st_size > log_limit ? (size_t)log_limit + 1u
                                          : (size_t)st.st_size;
  buf = (uint8_t *)malloc(want + 1u);
  if (buf == NULL) {
    close(fd);
    return -1;
  }
  while (got < want) {
    ssize_t r = read(fd, buf + got, want - got);
    if (r < 0 && errno == EINTR) {
      continue;
    }
    if (r <= 0) {
      break;
    }
    got += (size_t)r;
  }
  close(fd);
  if (got != want) {
    free(buf);
    return -1;
  }
  *out = buf;
  *out_len = got;
  return 0;
}

/* ---- arguments ----------------------------------------------------------- */

typedef struct {
  const char *identity;
  const char *result;
  const char *meta;
  const char *work_dir;
  const char *fixture_id;
  const char *fixture;
  const char *fixture_sha;
  const char *capture_out;
  uint64_t wall_ms;
  uint64_t bound;
  uint64_t log_limit;
  uint64_t expect_pass;
  char **runner_argv;
  int runner_argc;
} sup_args;

typedef struct {
  const char *name;
  const char **sp;
  uint64_t *np;
  int required;
  int seen;
} sup_opt;

static int parse_args(int argc, char **argv, sup_args *a) {
  sup_opt opts[] = {
      {"--identity", &a->identity, NULL, 1, 0},
      {"--result", &a->result, NULL, 1, 0},
      {"--meta", &a->meta, NULL, 1, 0},
      {"--work-dir", &a->work_dir, NULL, 1, 0},
      {"--wall-ms", NULL, &a->wall_ms, 1, 0},
      {"--bound", NULL, &a->bound, 1, 0},
      {"--log-limit", NULL, &a->log_limit, 1, 0},
      {"--fixture-id", &a->fixture_id, NULL, 1, 0},
      {"--fixture", &a->fixture, NULL, 1, 0},
      {"--fixture-sha256", &a->fixture_sha, NULL, 1, 0},
      {"--expect-pass", NULL, &a->expect_pass, 1, 0},
      {"--capture-out", &a->capture_out, NULL, 0, 0},
  };
  size_t nopt = sizeof opts / sizeof opts[0];
  int i = 1;
  size_t k;
  int bad = 0;
  while (i < argc && strcmp(argv[i], "--") != 0) {
    int found = 0;
    for (k = 0; k < nopt; k++) {
      if (strcmp(argv[i], opts[k].name) == 0) {
        found = 1;
        break;
      }
    }
    if (!found || i + 1 >= argc || opts[k].seen) {
      bad = 1;
      break;
    }
    opts[k].seen = 1;
    if (opts[k].sp != NULL) {
      *opts[k].sp = argv[i + 1];
    } else if (!t64_parse_u64(argv[i + 1], strlen(argv[i + 1]), opts[k].np)) {
      bad = 1;
    }
    i += 2;
  }
  if (!bad && (i >= argc || strcmp(argv[i], "--") != 0 || i + 1 >= argc)) {
    bad = 1;
  }
  if (!bad) {
    a->runner_argv = &argv[i + 1];
    a->runner_argc = argc - i - 1;
  }
  for (k = 0; k < nopt && !bad; k++) {
    if (opts[k].required && !opts[k].seen) {
      bad = 1;
    }
  }
  if (!bad) {
    if (strcmp(a->identity, "synthetic") != 0 || a->wall_ms == 0 ||
        a->log_limit > LOG_LIMIT_MAX || !valid_fixture_id(a->fixture_id) ||
        !is_hex64(a->fixture_sha)) {
      bad = 1;
    }
  }
  return bad ? -1 : 0;
}

/* --result is needed to report anything, so find it even when parsing failed. */
static const char *find_opt_value(int argc, char **argv, const char *name) {
  int i;
  for (i = 1; i + 1 < argc; i++) {
    if (strcmp(argv[i], "--") == 0) {
      break;
    }
    if (strcmp(argv[i], name) == 0) {
      return argv[i + 1];
    }
  }
  return NULL;
}

/* ---- fixture ------------------------------------------------------------- */

static t64_fixture_state check_fixture(const char *path, const char *want_hex,
                               char digest_hex[T64_SHA256_HEX_SIZE],
                               int *have_digest) {
  struct stat st;
  t64_sha256_ctx ctx;
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  uint8_t chunk[65536];
  uint64_t total = 0;
  int fd;
  *have_digest = 0;
  if (lstat(path, &st) != 0) {
    return (errno == ENOENT || errno == ENOTDIR) ? T64_FIX_ABSENT : T64_FIX_BAD;
  }
  if (!S_ISREG(st.st_mode) || st.st_size < 0 ||
      (unsigned long long)st.st_size > FIXTURE_MAX_BYTES) {
    return T64_FIX_BAD;
  }
  fd = open(path, O_RDONLY | O_NOFOLLOW);
  if (fd < 0) {
    return T64_FIX_BAD;
  }
  if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
    close(fd);
    return T64_FIX_BAD;
  }
  t64_sha256_init(&ctx);
  for (;;) {
    ssize_t r = read(fd, chunk, sizeof chunk);
    if (r < 0 && errno == EINTR) {
      continue;
    }
    if (r < 0) {
      close(fd);
      return T64_FIX_BAD;
    }
    if (r == 0) {
      break;
    }
    total += (uint64_t)r;
    if (total > FIXTURE_MAX_BYTES) {
      close(fd);
      return T64_FIX_BAD;
    }
    t64_sha256_update(&ctx, chunk, (size_t)r);
  }
  close(fd);
  t64_sha256_final(&ctx, digest);
  t64_sha256_hex(digest, digest_hex);
  *have_digest = 1;
  return strcmp(digest_hex, want_hex) == 0 ? T64_FIX_OK : T64_FIX_DIGEST_MISMATCH;
}

/* ---- main ---------------------------------------------------------------- */

typedef struct {
  char dir[4096];
  char report[4200];
  char capture[4200];
  char out[4200];
  char err[4200];
  int made_dir;
} workset;

static void workset_cleanup(workset *ws) {
  if (!ws->made_dir) {
    return;
  }
  unlink(ws->report);
  unlink(ws->capture);
  unlink(ws->out);
  unlink(ws->err);
  rmdir(ws->dir);
  ws->made_dir = 0;
}

int main(int argc, char **argv) {
  sup_args a;
  t64_observation o;
  t64_verdict v;
  workset ws;
  char fixture_digest[T64_SHA256_HEX_SIZE];
  char log_digest[T64_SHA256_HEX_SIZE];
  char profile_hash[T64_SHA256_HEX_SIZE];
  int have_fixture_digest = 0;
  int have_log_digest = 0;
  const char *result_path;
  const char *meta_path;
  uint8_t *capture = NULL;
  uint8_t repbuf[REPORT_MAX_BYTES + 1u];
  int wait_status = -1;
  uint64_t t_start = now_ms();
  char resbuf[2048];
  char metabuf[1024];
  size_t reslen = 0;
  size_t metalen = 0;
  const char *fixture_id_out = "none";
  const char *stop_reason = "none";
  uint64_t guest_tick = 0;
  t64_sha256_ctx sctx;
  uint8_t sdig[T64_SHA256_DIGEST_SIZE];
  struct sigaction sa;
  int parse_rc;
  pid_t pid = -1;

  memset(&a, 0, sizeof a);
  memset(&o, 0, sizeof o);
  memset(&ws, 0, sizeof ws);
  result_path = find_opt_value(argc, argv, "--result");
  meta_path = find_opt_value(argc, argv, "--meta");

  memset(&sa, 0, sizeof sa);
  sa.sa_handler = on_term; /* no SA_RESTART: waitpid polling still wakes */
  sigemptyset(&sa.sa_mask);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);
  signal(SIGPIPE, SIG_IGN);

  t64_sha256_init(&sctx);
  t64_sha256_update(&sctx, PROFILE_DESCRIPTOR, sizeof PROFILE_DESCRIPTOR - 1u);
  t64_sha256_final(&sctx, sdig);
  t64_sha256_hex(sdig, profile_hash);

  parse_rc = parse_args(argc, argv, &a);
  if (parse_rc != 0) {
    o.usage_error = 1;
  } else {
    fixture_id_out = a.fixture_id;
    o.bound = a.bound;
    o.log_limit = a.log_limit;
    o.expect_pass = a.expect_pass;
    o.fixture = check_fixture(a.fixture, a.fixture_sha, fixture_digest,
                              &have_fixture_digest);
  }

  if (!o.usage_error && o.fixture == T64_FIX_OK) {
    int ep[2];
    /* Every runner argument lives in this block's own storage, so building
     * the argv cannot fail or silently truncate it (IN-02). */
    char *child_argv[64];
    char opt_fixture[] = "--fixture";
    char opt_bound[] = "--bound";
    char opt_limit[] = "--log-limit";
    char opt_report[] = "--report";
    char opt_capture[] = "--capture";
    char fixture_s[4096];
    char bound_s[24];
    char limit_s[24];
    int nargv = 0;
    int i;
    int outfd;
    int errfd;
    int n;
    struct rlimit rl;
    uint64_t deadline;
    int status = 0;

    if (a.runner_argc > 50) {
      o.usage_error = 1;
      goto classified;
    }
    n = snprintf(ws.dir, sizeof ws.dir, "%s/t64sup.XXXXXX", a.work_dir);
    if (n <= 0 || (size_t)n >= sizeof ws.dir || mkdtemp(ws.dir) == NULL) {
      o.usage_error = 1;
      goto classified;
    }
    ws.made_dir = 1;
    snprintf(ws.report, sizeof ws.report, "%s/report", ws.dir);
    snprintf(ws.capture, sizeof ws.capture, "%s/capture", ws.dir);
    snprintf(ws.out, sizeof ws.out, "%s/stdout", ws.dir);
    snprintf(ws.err, sizeof ws.err, "%s/stderr", ws.dir);
    {
      int rfd = create_private(ws.report);
      int cfd = create_private(ws.capture);
      outfd = create_private(ws.out);
      errfd = create_private(ws.err);
      if (rfd < 0 || cfd < 0 || outfd < 0 || errfd < 0) {
        o.usage_error = 1;
        goto classified;
      }
      close(rfd);
      close(cfd);
    }
    n = snprintf(fixture_s, sizeof fixture_s, "%s", a.fixture);
    if (n <= 0 || (size_t)n >= sizeof fixture_s) {
      o.usage_error = 1;
      goto classified;
    }
    snprintf(bound_s, sizeof bound_s, "%llu", (unsigned long long)a.bound);
    snprintf(limit_s, sizeof limit_s, "%llu", (unsigned long long)a.log_limit);
    for (i = 0; i < a.runner_argc; i++) {
      child_argv[nargv++] = a.runner_argv[i];
    }
    child_argv[nargv++] = opt_fixture;
    child_argv[nargv++] = fixture_s;
    child_argv[nargv++] = opt_bound;
    child_argv[nargv++] = bound_s;
    child_argv[nargv++] = opt_limit;
    child_argv[nargv++] = limit_s;
    child_argv[nargv++] = opt_report;
    child_argv[nargv++] = ws.report;
    child_argv[nargv++] = opt_capture;
    child_argv[nargv++] = ws.capture;
    child_argv[nargv] = NULL;

    if (pipe(ep) != 0 || fcntl(ep[0], F_SETFD, FD_CLOEXEC) != 0 ||
        fcntl(ep[1], F_SETFD, FD_CLOEXEC) != 0 ||
        fcntl(ep[0], F_SETFL, O_NONBLOCK) != 0) {
      o.usage_error = 1;
      goto classified;
    }
    {
      /* WR-05: saturate, so a huge --wall-ms cannot wrap the deadline below
       * the current time and turn a valid run into wall-backstop. */
      uint64_t start = now_ms();
      deadline = a.wall_ms > UINT64_MAX - start ? UINT64_MAX : start + a.wall_ms;
    }
    pid = fork();
    if (pid < 0) {
      close(ep[0]);
      close(ep[1]);
      o.usage_error = 1;
      goto classified;
    }
    if (pid == 0) {
      int devnull;
      int e;
      setpgid(0, 0);
      rl.rlim_cur = 0;
      rl.rlim_max = 0;
      setrlimit(RLIMIT_CORE, &rl);
      rl.rlim_cur = (rlim_t)(a.log_limit + FSIZE_SLACK);
      rl.rlim_max = rl.rlim_cur;
      setrlimit(RLIMIT_FSIZE, &rl);
      devnull = open("/dev/null", O_RDONLY);
      if (devnull >= 0) {
        dup2(devnull, 0);
      }
      dup2(outfd, 1);
      dup2(errfd, 2);
      signal(SIGPIPE, SIG_DFL);
      execvp(child_argv[0], child_argv);
      e = errno;
      (void)!write(ep[1], &e, sizeof e);
      _exit(127);
    }
    setpgid(pid, pid); /* both sides, to avoid the race */
    close(ep[1]);
    close(outfd);
    close(errfd);
    o.launched = 1;
    for (;;) {
      siginfo_t si;
      memset(&si, 0, sizeof si);
      /* WNOWAIT leaves an exited runner as a zombie, so its pid, and with it
       * the process group id, cannot be reused before the group is killed. */
      if (waitid(P_PID, (id_t)pid, &si, WEXITED | WNOHANG | WNOWAIT) == 0 &&
          si.si_pid == pid) {
        break;
      }
      if (g_term) {
        o.supervisor_signalled = 1;
        break;
      }
      if (now_ms() >= deadline) {
        o.wall_backstop = 1;
        break;
      }
      {
        struct timespec nap = {0, 2 * 1000 * 1000};
        nanosleep(&nap, NULL);
      }
    }
    /* WR-04: kill the runner's whole process group on every path, including
     * a normal exit, before the report and capture are read, so no
     * descendant survives the run or writes to its files after they are
     * read. ESRCH (nothing left in the group) is expected and ignored. */
    (void)kill(-pid, SIGKILL);
    (void)kill(pid, SIGKILL);
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    wait_status = status;
    {
      int e = 0;
      if (read(ep[0], &e, sizeof e) == (ssize_t)sizeof e) {
        o.exec_failed = 1;
      }
    }
    close(ep[0]);
    if (WIFEXITED(status)) {
      o.wait_state = T64_WAIT_EXITED;
      o.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      o.wait_state = T64_WAIT_SIGNALLED;
    }

    /* Read the report and the capture only when they could matter. */
    {
      long rl2 = read_report(ws.report, repbuf, sizeof repbuf);
      t64_report rep;
      if (rl2 < 0) {
        o.report_unreadable = 1;
      }
      o.report_len = rl2 > 0 ? (size_t)rl2 : 0;
      if (o.report_len > 0) {
        if (t64_parse_report(repbuf, o.report_len, &rep)) {
          o.stop = rep.stop;
          o.guest_tick = rep.guest_tick;
          o.log_bytes_total = rep.log_bytes_total;
        } else {
          o.report_malformed = 1;
        }
      }
    }
    if (read_capture(ws.capture, a.log_limit, &capture, &o.capture_len) != 0) {
      o.capture_unreadable = 1;
    }
    o.capture = capture;
  }
classified:
  t64_classify(&o, &v);
  if (v.guest_data) {
    t64_sha256_ctx lc;
    uint8_t ld[T64_SHA256_DIGEST_SIZE];
    t64_sha256_init(&lc);
    t64_sha256_update(&lc, o.capture != NULL ? o.capture : (const uint8_t *)"",
                      o.capture_len);
    t64_sha256_final(&lc, ld);
    t64_sha256_hex(ld, log_digest);
    have_log_digest = 1;
    stop_reason =
        o.stop == T64_REPORT_STOP_HALT ? "guest-halt" : "guest-bound";
    guest_tick = o.guest_tick;
  }

  if (a.capture_out != NULL && o.capture != NULL && !o.usage_error) {
    (void)atomic_write(a.capture_out, o.capture, o.capture_len);
  }
  if (result_path == NULL) {
    fprintf(stderr, "t64-supervisor: usage error (no --result)\n");
    workset_cleanup(&ws);
    free(capture);
    return 1;
  }
  if (build_result(resbuf, sizeof resbuf, &reslen, &v, fixture_id_out,
                   have_fixture_digest ? fixture_digest : NULL,
                   have_log_digest ? log_digest : NULL, stop_reason,
                   guest_tick, profile_hash) != 0 ||
      atomic_write(result_path, resbuf, reslen) != 0) {
    fprintf(stderr, "t64-supervisor: cannot write the result\n");
    workset_cleanup(&ws);
    free(capture);
    return 1;
  }
  if (meta_path != NULL &&
      (build_meta(metabuf, sizeof metabuf, &metalen, wait_status,
                  now_ms() - t_start) != 0 ||
       atomic_write(meta_path, metabuf, metalen) != 0)) {
    fprintf(stderr, "t64-supervisor: cannot write run-meta\n");
  }
  fprintf(stderr, "t64-supervisor: outcome=%s detail=%s\n",
          t64_outcome_name(v.outcome), t64_detail_name(v.detail));
  workset_cleanup(&ws);
  free(capture);
  return v.exit_code;
}
