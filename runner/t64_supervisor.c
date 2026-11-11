/*
 * t64-supervisor: runs synthetic v1 or descriptor-backed real v2 under a
 * wall-clock backstop. Real reports are independently checked against raw
 * capture and process status before an atomic result is written.
 *
 * See docs/spec/result-protocol.md for the CLI and the result contract.
 */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
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
#include "t64_input.h"
#include "t64_json.h"
#include "t64_jsonw.h"
#include "t64_num.h"
#include "t64_real_result.h"
#include "t64_real_fixture.h"
#include "t64_sha256.h"
#include "t64_verified_input.h"
#include "tendies64/t64_facade.h"
#include "tendies64/t64_machine.h"

#ifndef T64_BUILD_ID
#define T64_BUILD_ID "0.0.0+synthetic"
#endif

#define FIXTURE_MAX_BYTES (64ull << 20)
#define LOG_LIMIT_MAX (64ull << 20)
#define REPORT_MAX_BYTES 512u
#define FSIZE_SLACK (64ull << 10)
#define REAL_INPUT_SIZE 54u
#define REAL_INPUT_MAX_BYTES T64_VERIFIED_SCHEDULE_MAX_BYTES
#define REAL_GUEST_TICKS UINT64_C(530500)
#define REAL_PROFILE_ID "ntsc-cart-4m-openipl3-warm-v1"
#define REAL_INPUT_SHA256 \
  "6d5593bc2d95465b44ec7ce7785e5ff7d7d53605de9b039a021bc697d226186d"
#define REAL_PROFILE_SHA256 \
  "d8c12cb2a00410a2153ca62da318166f3ca029dea8e3d6b9c841262cb8203c71"
#define REAL_ASSET_SHA256 \
  "f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068"
#define REAL_PAYLOAD_SHA256 \
  "f716a90b1d36ec4253d096790791256d3543455e54473a755ff8c3d1199b15d5"
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
  if (s == NULL || strlen(s) != 64) {
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

static int read_real_capture(const char *path, uint8_t **out, size_t *out_len,
                             uint64_t *total_out) {
  struct stat st;
  size_t want;
  size_t got = 0u;
  uint8_t *buf;
  int fd = open_runner_file(path, &st);
  if (out == NULL || out_len == NULL || total_out == NULL) {
    if (fd >= 0) close(fd);
    return -1;
  }
  *out = NULL;
  *out_len = 0u;
  *total_out = 0u;
  if (fd < 0 || st.st_size < 0) {
    if (fd >= 0) close(fd);
    return -1;
  }
  want = st.st_size > 4096 ? 4096u : (size_t)st.st_size;
  buf = (uint8_t *)malloc(want + 1u);
  if (buf == NULL) {
    close(fd);
    return -1;
  }
  while (got < want) {
    ssize_t count = read(fd, buf + got, want - got);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) break;
    got += (size_t)count;
  }
  close(fd);
  if (got != want) {
    free(buf);
    return -1;
  }
  *out = buf;
  *out_len = want;
  *total_out = (uint64_t)st.st_size;
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
  const char *input;
  const char *input_sha256;
  const char *capture_out;
  uint64_t input_size;
  uint64_t wall_ms;
  uint64_t bound;
  uint64_t log_limit;
  uint64_t expect_pass;
  int real_mode;
  int input_options_seen;
#ifdef T64_SUPERVISOR_TESTING
  uint64_t test_ready_fd;
  uint64_t test_gate_fd;
  const char *test_fault;
#endif
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
      {"--input", &a->input, NULL, 0, 0},
      {"--input-size", NULL, &a->input_size, 0, 0},
      {"--input-sha256", &a->input_sha256, NULL, 0, 0},
#ifdef T64_SUPERVISOR_TESTING
      {"--test-ready-fd", NULL, &a->test_ready_fd, 0, 0},
      {"--test-gate-fd", NULL, &a->test_gate_fd, 0, 0},
      {"--test-fault", &a->test_fault, NULL, 0, 0},
#endif
      {"--expect-pass", NULL, &a->expect_pass, 1, 0},
      {"--capture-out", &a->capture_out, NULL, 0, 0},
  };
  size_t nopt = sizeof opts / sizeof opts[0];
  int i = 1;
  size_t k;
  int bad = 0;
  while (i < argc && strcmp(argv[i], "--") != 0) {
    if (strcmp(argv[i], "--real") == 0) {
      if (a->real_mode) {
        bad = 1;
        break;
      }
      a->real_mode = 1;
      i++;
      continue;
    }
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
    if (strcmp(argv[i], "--input-size") == 0 ||
        strcmp(argv[i], "--input-sha256") == 0) {
      a->input_options_seen = 1;
    }
    opts[k].seen = 1;
    if (opts[k].sp != NULL) {
      *opts[k].sp = argv[i + 1];
    } else if (!t64_parse_u64(argv[i + 1], strlen(argv[i + 1]), opts[k].np)) {
      bad = 1;
    }
    i += 2;
  }
  if (!bad && i < argc && strcmp(argv[i], "--") == 0) {
    a->runner_argv = &argv[i + 1];
    a->runner_argc = argc - i - 1;
    if (a->runner_argc == 0) {
      bad = 1;
    }
  } else if (!bad && !a->real_mode) {
    bad = 1;
  } else if (!bad && i < argc) {
    bad = 1;
  }
  if (!bad && a->real_mode) {
    if (a->result == NULL || a->meta == NULL || a->work_dir == NULL ||
        a->wall_ms == 0u || a->fixture == NULL || a->input == NULL ||
        a->runner_argc != 0) {
      bad = 1;
    }
    if (!bad && a->input_options_seen) {
      if (a->input_size == 0u || a->input_size > REAL_INPUT_MAX_BYTES ||
          !is_hex64(a->input_sha256)) {
        bad = 1;
      }
    } else if (!bad) {
      a->input_size = REAL_INPUT_SIZE;
      a->input_sha256 = REAL_INPUT_SHA256;
    }
    if (!bad && a->bound == 0u) {
      a->bound = REAL_GUEST_TICKS;
    } else if (!bad && a->bound > UINT64_C(600000)) {
      bad = 1;
    }
#ifdef T64_SUPERVISOR_TESTING
    if ((a->test_ready_fd == 0u) != (a->test_gate_fd == 0u) ||
        a->test_ready_fd > (uint64_t)INT_MAX ||
        a->test_gate_fd > (uint64_t)INT_MAX ||
        (a->test_ready_fd != 0u && a->test_ready_fd <= STDERR_FILENO) ||
        (a->test_gate_fd != 0u && a->test_gate_fd <= STDERR_FILENO)) {
      bad = 1;
    }
    if (a->test_fault != NULL &&
        strcmp(a->test_fault, "exit-nonzero") != 0 &&
        strcmp(a->test_fault, "capture-closed") != 0 &&
        strcmp(a->test_fault, "crash-after-output") != 0 &&
        strcmp(a->test_fault, "delay-after-output") != 0 &&
        strcmp(a->test_fault, "report-missing") != 0 &&
        strcmp(a->test_fault, "report-truncated") != 0 &&
        strcmp(a->test_fault, "report-oversize") != 0 &&
        strcmp(a->test_fault, "report-malformed") != 0 &&
        strcmp(a->test_fault, "report-unknown") != 0) {
      bad = 1;
    }
#endif
  } else if (!bad) {
    for (k = 0; k < nopt && !bad; k++) {
      if (opts[k].required && !opts[k].seen) bad = 1;
    }
    if (!bad &&
        (a->identity == NULL || strcmp(a->identity, "synthetic") != 0 ||
         a->wall_ms == 0u || a->log_limit > LOG_LIMIT_MAX ||
         a->fixture_id == NULL || !valid_fixture_id(a->fixture_id) ||
         a->fixture_sha == NULL || !is_hex64(a->fixture_sha) ||
         a->input != NULL || a->input_options_seen)) {
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
  fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
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

static int real_resolve_sibling(const char *self_path, const char *name,
                                char *output, size_t capacity) {
  char resolved[4096];
  char *slash;
  int length;
  if (self_path == NULL || name == NULL || output == NULL || capacity == 0u ||
      realpath(self_path, resolved) == NULL) {
    return -1;
  }
  slash = strrchr(resolved, '/');
  if (slash == NULL) return -1;
  slash[1] = '\0';
  length = snprintf(output, capacity, "%s%s", resolved, name);
  return length > 0 && (size_t)length < capacity ? 0 : -1;
}

static int real_json_string_equals(const t64_json_node *container,
                                   const char *key, const char *expected) {
  const t64_json_node *value = t64_json_get(container, key);
  size_t length = strlen(expected);
  return value != NULL && value->type == T64_JSON_STRING &&
         value->len == length && memcmp(value->str, expected, length) == 0;
}

static int real_json_u64(const t64_json_node *container, const char *key,
                         uint64_t *value_out) {
  const t64_json_node *value = t64_json_get(container, key);
  return value != NULL && t64_json_u64_string(value, value_out);
}

static int real_json_copy_string(const t64_json_node *node, char *output,
                                 size_t capacity) {
  if (node == NULL || node->type != T64_JSON_STRING || output == NULL ||
      capacity == 0u || node->len >= capacity ||
      memchr(node->str, '\0', node->len) != NULL) {
    return 0;
  }
  memcpy(output, node->str, node->len);
  output[node->len] = '\0';
  return 1;
}

static int real_json_copy_identity(const t64_json_node *root, const char *key,
                                   char *output, size_t capacity,
                                   bool *present) {
  const t64_json_node *node = t64_json_get(root, key);
  if (node == NULL || output == NULL || present == NULL) return 0;
  if (node->type == T64_JSON_NULL) {
    output[0] = '\0';
    *present = false;
    return 1;
  }
  if (!real_json_copy_string(node, output, capacity)) return 0;
  *present = true;
  return 1;
}

static int real_checker_accepts(char *checker, char *report) {
  pid_t pid = fork();
  int status = 0;
  if (pid == 0) {
    char result_mode[] = "result";
    char *const args[] = {checker, result_mode, report, NULL};
    int null_fd = open("/dev/null", O_WRONLY);
    if (null_fd >= 0) {
      (void)dup2(null_fd, STDOUT_FILENO);
      (void)dup2(null_fd, STDERR_FILENO);
    }
    execv(checker, args);
    _exit(127);
  }
  if (pid < 0) return 0;
  while (waitpid(pid, &status, 0) < 0) {
    if (errno == EINTR) continue;
    return 0;
  }
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static int real_report_matches_capture(const uint8_t *bytes, size_t length,
                                       const uint8_t *capture,
                                       size_t capture_length,
                                       uint64_t capture_total,
                                       const t64_real_fixture *fixture,
                                       const char *input_digest) {
  t64_json_doc document = {0};
  const t64_json_node *root;
  const t64_json_node *counts;
  const t64_json_node *stop;
  const char *build_id = t64_build_identity();
  const char *source_revision = t64_source_revision();
  uint64_t log_bytes = 0u;
  uint64_t log_total = 0u;
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  char digest_hex[T64_SHA256_HEX_SIZE];
  int valid = 0;

  if (bytes == NULL || length == 0u || build_id == NULL ||
      source_revision == NULL || fixture == NULL || input_digest == NULL ||
      t64_json_parse(bytes, length, NULL, &document, NULL) != 0) {
    return 0;
  }
  root = document.root;
  counts = t64_json_get(root, "counts");
  stop = t64_json_get(root, "guest_stop");
  if (counts == NULL || counts->type != T64_JSON_OBJECT ||
      !real_json_string_equals(root, "build_id", build_id) ||
      !real_json_string_equals(root, "source_revision", source_revision) ||
      !real_json_string_equals(root, "profile_id", REAL_PROFILE_ID) ||
      !real_json_string_equals(root, "profile_digest", REAL_PROFILE_SHA256) ||
      !real_json_string_equals(root, "fixture_id", fixture->id) ||
      !real_json_string_equals(root, "fixture_sha256", fixture->sha256) ||
      !real_json_string_equals(root, "ipl3_sha256", REAL_ASSET_SHA256) ||
      !real_json_string_equals(root, "ipl3_payload_sha256",
                               REAL_PAYLOAD_SHA256) ||
      !real_json_string_equals(root, "input_digest", input_digest) ||
      !real_json_u64(counts, "log_bytes", &log_bytes) ||
      !real_json_u64(counts, "log_bytes_total", &log_total) ||
      log_bytes != (uint64_t)capture_length ||
      capture_total != (uint64_t)capture_length ||
      (log_total != capture_total &&
       !(log_total > capture_total && log_bytes == UINT64_C(4096) &&
         capture_length == 4096u &&
         real_json_string_equals(root, "outcome", "harness-error") &&
         real_json_string_equals(root, "detail", "output-overflow") &&
         real_json_string_equals(stop, "kind", "output-overflow"))) ||
      log_total < log_bytes ||
      capture_length > T64_REAL_RESULT_MAX_BYTES) {
    goto done;
  }
  if (t64_json_get(root, "log_sha256")->type == T64_JSON_NULL &&
      log_bytes == 0u && log_total == 0u && capture_length == 0u &&
      capture_total == 0u &&
      real_json_string_equals(root, "outcome", "harness-error") &&
      real_json_string_equals(root, "detail", "host-error") &&
      real_json_string_equals(stop, "kind", "host-error")) {
    /* The producer may know the capture is untrustworthy while its attempted
     * total counter is saturated. Preserve that as an unknown log, not a
     * fabricated exact count or an empty-log digest. */
    valid = 1;
    goto done;
  }
  {
    t64_sha256_ctx hash;
    t64_sha256_init(&hash);
    t64_sha256_update(&hash, capture, capture_length);
    t64_sha256_final(&hash, digest);
  }
  t64_sha256_hex(digest, digest_hex);
  if (real_json_string_equals(root, "log_sha256", digest_hex)) {
    valid = 1;
  }
done:
  t64_json_free(&document);
  return valid;
}

static int real_result_write_atomic(const char *path,
                                    const t64_real_result *result);

static int real_report_decode(const uint8_t *bytes, size_t length,
                              t64_real_result *result) {
  t64_json_doc document = {0};
  const t64_json_node *root;
  const t64_json_node *counts;
  const t64_json_node *stop;
  const t64_json_node *milestones;
  const t64_json_node *item;
  size_t index = 0u;
  int valid = 0;

  if (bytes == NULL || length == 0u || result == NULL ||
      t64_json_parse(bytes, length, NULL, &document, NULL) != 0) {
    return 0;
  }
  root = document.root;
  counts = t64_json_get(root, "counts");
  stop = t64_json_get(root, "guest_stop");
  milestones = t64_json_get(root, "milestones");
  memset(result, 0, sizeof *result);
  result->prelaunch = false;
  if (root == NULL || root->type != T64_JSON_OBJECT || counts == NULL ||
      counts->type != T64_JSON_OBJECT || stop == NULL ||
      stop->type != T64_JSON_OBJECT || milestones == NULL ||
      milestones->type != T64_JSON_ARRAY ||
      milestones->count > T64_REAL_RESULT_MAX_MILESTONES ||
      !real_json_copy_identity(root, "build_id", result->build_id,
                               sizeof result->build_id, &(bool){false}) ||
      !real_json_copy_identity(root, "source_revision",
                               result->source_revision,
                               sizeof result->source_revision,
                               &(bool){false}) ||
      !real_json_copy_identity(root, "profile_id", result->profile_id,
                               sizeof result->profile_id, &(bool){false}) ||
      !real_json_copy_identity(root, "profile_digest",
                               result->profile_digest,
                               sizeof result->profile_digest, &(bool){false}) ||
      !real_json_copy_identity(root, "fixture_id", result->fixture_id,
                               sizeof result->fixture_id, &(bool){false}) ||
      !real_json_copy_identity(root, "fixture_sha256",
                               result->fixture_sha256,
                               sizeof result->fixture_sha256, &(bool){false}) ||
      !real_json_copy_identity(root, "ipl3_sha256", result->ipl3_sha256,
                               sizeof result->ipl3_sha256, &(bool){false}) ||
      !real_json_copy_identity(root, "ipl3_payload_sha256",
                               result->ipl3_payload_sha256,
                               sizeof result->ipl3_payload_sha256,
                               &(bool){false}) ||
      !real_json_copy_identity(root, "input_digest", result->input_digest,
                               sizeof result->input_digest,
                               &result->has_input_digest) ||
      !real_json_copy_identity(root, "log_sha256", result->log_sha256,
                               sizeof result->log_sha256,
                               &result->has_log_sha256) ||
      !real_json_copy_identity(root, "state_digest", result->state_digest,
                               sizeof result->state_digest,
                               &result->has_state_digest) ||
      !real_json_copy_string(t64_json_get(stop, "kind"),
                             result->guest_stop_kind,
                             sizeof result->guest_stop_kind) ||
      !real_json_copy_string(t64_json_get(root, "outcome"), result->outcome,
                             sizeof result->outcome) ||
      !real_json_copy_string(t64_json_get(root, "detail"), result->detail,
                             sizeof result->detail) ||
      !real_json_u64(stop, "bound_tick", &result->bound_tick) ||
      !real_json_u64(root, "guest_tick", &result->guest_tick) ||
      !real_json_u64(root, "instruction_count", &result->instruction_count) ||
      !real_json_u64(counts, "log_bytes", &result->log_bytes) ||
      !real_json_u64(counts, "log_bytes_total", &result->log_bytes_total)) {
    goto done;
  }
  for (item = milestones->first; item != NULL; item = item->next) {
    t64_real_milestone *milestone = &result->milestones[index++];
    if (item->type != T64_JSON_OBJECT ||
        !real_json_copy_string(t64_json_get(item, "kind"), milestone->kind,
                               sizeof milestone->kind) ||
        !real_json_copy_string(t64_json_get(item, "pc"), milestone->pc,
                               sizeof milestone->pc) ||
        !real_json_u64(item, "guest_tick", &milestone->guest_tick)) {
      goto done;
    }
  }
  result->milestone_count = index;
  valid = true;
done:
  t64_json_free(&document);
  return valid;
}

static int real_write_report_override(const char *path, const uint8_t *bytes,
                                      size_t length, const char *detail) {
  t64_real_result result;
  if (!real_report_decode(bytes, length, &result)) return -1;
  (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                 "host-error");
  (void)snprintf(result.outcome, sizeof result.outcome, "harness-error");
  (void)snprintf(result.detail, sizeof result.detail, "%s", detail);
  return real_result_write_atomic(path, &result);
}

static int real_result_write_atomic(const char *path,
                                   const t64_real_result *result) {
  char bytes[T64_REAL_RESULT_MAX_BYTES];
  size_t length = 0u;
  if (path == NULL ||
      !t64_real_result_encode(result, bytes, sizeof bytes, &length)) {
    return -1;
  }
  return atomic_write(path, bytes, length);
}

static void real_set_prelaunch(t64_real_result *result, const char *outcome,
                               const char *detail, const char *stop_kind,
                               uint64_t bound_tick) {
  memset(result, 0, sizeof *result);
  result->prelaunch = true;
  (void)snprintf(result->guest_stop_kind, sizeof result->guest_stop_kind,
                 "%s", stop_kind);
  (void)snprintf(result->outcome, sizeof result->outcome, "%s", outcome);
  (void)snprintf(result->detail, sizeof result->detail, "%s", detail);
  result->bound_tick = bound_tick;
}

static int real_schedule_digest(int fd, uint64_t expected_size,
                                char output[T64_REAL_RESULT_HEX_SIZE]) {
  uint8_t raw[REAL_INPUT_MAX_BYTES];
  t64_input_schedule *schedule = NULL;
  t64_input_error input_error;
  size_t total = 0u;
  size_t canonical_length = 0u;
  const uint8_t *canonical;
  t64_sha256_ctx hash;
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  uint8_t extra;
  if (expected_size == 0u || expected_size > sizeof raw ||
      lseek(fd, 0, SEEK_SET) != 0) {
    return -1;
  }
  while (total < (size_t)expected_size) {
    ssize_t count = read(fd, raw + total, (size_t)expected_size - total);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) return -1;
    total += (size_t)count;
  }
  for (;;) {
    ssize_t count = read(fd, &extra, sizeof extra);
    if (count < 0 && errno == EINTR) continue;
    if (count != 0) return -1;
    break;
  }
  if (t64_input_parse(raw, total, &schedule, &input_error) !=
      T64_INPUT_OK) {
    return -1;
  }
  canonical = t64_input_schedule_canonical_json(schedule, &canonical_length);
  if (canonical == NULL || canonical_length == 0u) {
    t64_input_schedule_destroy(schedule);
    return -1;
  }
  t64_sha256_init(&hash);
  t64_sha256_update(&hash, canonical, canonical_length);
  t64_sha256_final(&hash, digest);
  t64_sha256_hex(digest, output);
  t64_input_schedule_destroy(schedule);
  if (lseek(fd, 0, SEEK_SET) != 0) return -1;
  return 0;
}

static int real_fixture_identify_path(const char *path,
                                      const t64_real_fixture **fixture_out,
                                      int *missing_out) {
  uint8_t buffer[16384];
  t64_sha256_ctx hash;
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  char digest_hex[T64_SHA256_HEX_SIZE];
  struct stat st;
  uint64_t total = 0u;
  int fd;

  if (path == NULL || fixture_out == NULL || missing_out == NULL) return -1;
  *fixture_out = NULL;
  *missing_out = 0;
  fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0) {
    if (errno == ENOENT || errno == ENOTDIR) *missing_out = 1;
    return -1;
  }
  if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0 ||
      (uint64_t)st.st_size > T64_VERIFIED_FIXTURE_MAX_BYTES) {
    close(fd);
    return -1;
  }
  t64_sha256_init(&hash);
  for (;;) {
    ssize_t count = read(fd, buffer, sizeof buffer);
    if (count < 0 && errno == EINTR) continue;
    if (count < 0 || (uint64_t)count > T64_VERIFIED_FIXTURE_MAX_BYTES - total) {
      close(fd);
      return -1;
    }
    if (count == 0) break;
    t64_sha256_update(&hash, buffer, (size_t)count);
    total += (uint64_t)count;
  }
  if (close(fd) != 0 || total != (uint64_t)st.st_size) return -1;
  t64_sha256_final(&hash, digest);
  t64_sha256_hex(digest, digest_hex);
  *fixture_out = t64_real_fixture_find_bytes((size_t)total, digest_hex);
  return *fixture_out == NULL ? 0 : 1;
}

static int real_result_initialize(t64_real_result *result,
                                  const t64_real_fixture *fixture,
                                  const char *input_digest) {
  const t64_profile_descriptor *profile = NULL;
  const char *build_id = t64_build_identity();
  const char *source_revision = t64_source_revision();
  uint8_t profile_digest[T64_DIGEST_SIZE];
  char profile_hex[T64_SHA256_HEX_SIZE];
  char asset_hex[T64_SHA256_HEX_SIZE];
  char payload_hex[T64_SHA256_HEX_SIZE];
  if (result == NULL || fixture == NULL || build_id == NULL ||
      source_revision == NULL ||
      input_digest == NULL ||
      strlen(build_id) >= sizeof result->build_id ||
      strlen(source_revision) >= sizeof result->source_revision ||
      t64_profile_lookup(REAL_PROFILE_ID, &profile) != T64_STATUS_OK ||
      profile == NULL || t64_profile_hash(profile, profile_digest) !=
                             T64_STATUS_OK) {
    return -1;
  }
  t64_sha256_hex(profile_digest, profile_hex);
  t64_sha256_hex(profile->asset_sha256, asset_hex);
  t64_sha256_hex(profile->payload_sha256, payload_hex);
  if (strcmp(profile_hex, REAL_PROFILE_SHA256) != 0 ||
      strcmp(asset_hex, REAL_ASSET_SHA256) != 0 ||
      strcmp(payload_hex, REAL_PAYLOAD_SHA256) != 0) {
    return -1;
  }
  memset(result, 0, sizeof *result);
  (void)snprintf(result->build_id, sizeof result->build_id, "%s", build_id);
  (void)snprintf(result->source_revision, sizeof result->source_revision,
                 "%s", source_revision);
  (void)snprintf(result->profile_id, sizeof result->profile_id, "%s",
                 REAL_PROFILE_ID);
  (void)snprintf(result->profile_digest, sizeof result->profile_digest, "%s",
                 profile_hex);
  (void)snprintf(result->fixture_id, sizeof result->fixture_id, "%s",
                 fixture->id);
  (void)snprintf(result->fixture_sha256, sizeof result->fixture_sha256, "%s",
                 fixture->sha256);
  (void)snprintf(result->ipl3_sha256, sizeof result->ipl3_sha256, "%s",
                 asset_hex);
  (void)snprintf(result->ipl3_payload_sha256,
                 sizeof result->ipl3_payload_sha256, "%s", payload_hex);
  (void)snprintf(result->input_digest, sizeof result->input_digest, "%s",
                 input_digest);
  result->has_input_digest = true;
  result->bound_tick = REAL_GUEST_TICKS;
  (void)snprintf(result->guest_stop_kind, sizeof result->guest_stop_kind,
                 "host-error");
  (void)snprintf(result->outcome, sizeof result->outcome, "harness-error");
  (void)snprintf(result->detail, sizeof result->detail, "host-error");
  return 0;
}

static int real_result_set_capture(t64_real_result *result,
                                   const uint8_t *capture,
                                   size_t capture_length,
                                   uint64_t capture_total,
                                   bool capture_valid) {
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  char digest_hex[T64_SHA256_HEX_SIZE];
  if (result == NULL) return -1;
  result->has_log_sha256 = false;
  result->log_bytes = 0u;
  result->log_bytes_total = 0u;
  if (!capture_valid) return 0;
  if (capture_length > 4096u || capture_total < capture_length ||
      (capture_length != 0u && capture == NULL)) {
    return -1;
  }
  t64_sha256_ctx hash;
  t64_sha256_init(&hash);
  t64_sha256_update(&hash, capture, capture_length);
  t64_sha256_final(&hash, digest);
  t64_sha256_hex(digest, digest_hex);
  (void)snprintf(result->log_sha256, sizeof result->log_sha256, "%s",
                 digest_hex);
  result->has_log_sha256 = true;
  result->log_bytes = (uint64_t)capture_length;
  result->log_bytes_total = capture_total;
  return 0;
}

static int real_write_prelaunch(const char *path, const char *outcome,
                                const char *detail, const char *stop_kind,
                                uint64_t bound_tick) {
  t64_real_result result;
  real_set_prelaunch(&result, outcome, detail, stop_kind, bound_tick);
  return real_result_write_atomic(path, &result);
}

static int real_write_postlaunch_error(const char *path,
                                       const t64_real_result *identities,
                                       const char *stop_kind,
                                       const char *detail,
                                       const uint8_t *capture,
                                       size_t capture_length,
                                       uint64_t capture_total,
                                       bool capture_valid) {
  t64_real_result result;
  if (identities == NULL) return -1;
  result = *identities;
  result.prelaunch = false;
  result.guest_tick = 0u;
  result.instruction_count = 0u;
  result.milestone_count = 0u;
  result.has_state_digest = false;
  if (real_result_set_capture(&result, capture, capture_length,
                              capture_total,
                              capture_valid) != 0) {
    return -1;
  }
  (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind, "%s",
                 stop_kind);
  (void)snprintf(result.outcome, sizeof result.outcome, "harness-error");
  (void)snprintf(result.detail, sizeof result.detail, "%s", detail);
  return real_result_write_atomic(path, &result);
}

static int real_clear_cloexec(int fd) {
  int flags = fcntl(fd, F_GETFD);
  return flags >= 0 && fcntl(fd, F_SETFD, flags & ~FD_CLOEXEC) == 0;
}

static int real_test_barrier(const sup_args *args) {
#ifdef T64_SUPERVISOR_TESTING
  if (args->test_ready_fd != 0u) {
    int ready_fd = (int)args->test_ready_fd;
    int gate_fd = (int)args->test_gate_fd;
    uint8_t token = 1u;
    ssize_t count;
    if (write_all(ready_fd, &token, 1u) != 0) {
      close(ready_fd);
      close(gate_fd);
      return -1;
    }
    do {
      count = read(gate_fd, &token, 1u);
    } while (count < 0 && errno == EINTR);
    close(ready_fd);
    close(gate_fd);
    return count == 1 && token == 1u ? 0 : -1;
  }
#else
  (void)args;
#endif
  return 0;
}

static int real_run_mode(const sup_args *args, const char *self_path) {
  t64_verified_input fixture = {-1, 0u, {0u}, {0}, 0u};
  t64_verified_input input = {-1, 0u, {0u}, {0}, 0u};
  t64_verified_input_status fixture_status;
  t64_verified_input_status input_status;
  const t64_real_fixture *fixture_identity = NULL;
  t64_real_result identities;
  workset ws;
  char canonical_input[T64_REAL_RESULT_HEX_SIZE];
  char runner_path[4096];
  char checker_path[4096];
  char fixture_fd_text[24];
  char input_fd_text[24];
  char guest_ticks_text[24];
  char report_bytes[T64_REAL_RESULT_MAX_BYTES + 1u];
  uint8_t *capture = NULL;
  size_t capture_length = 0u;
  uint64_t capture_total = 0u;
  long report_length = 0;
  bool capture_valid = false;
  bool capture_export_failed = false;
  bool report_schema_valid = false;
  bool capture_matches_report = false;
  bool report_valid = false;
  bool process_ok = false;
  int exit_code = 1;
  int wait_status = -1;
  int exec_pipe[2] = {-1, -1};
  int report_fd = -1;
  int capture_fd = -1;
  pid_t pid = -1;
  uint64_t start_ms = now_ms();
  uint64_t deadline = 0u;
  bool wall_backstop = false;
  bool supervisor_signalled = false;
  int fixture_missing = 0;
  int fixture_known;
  int exec_failed = 0;
  int runner_status = 0;
  struct rlimit limit;
#ifdef T64_SUPERVISOR_TESTING
  const char *runner_name = "t64-runner-test";
#else
  const char *runner_name = "t64-runner-real";
#endif

  memset(&ws, 0, sizeof ws);
  fixture_known = real_fixture_identify_path(args->fixture,
                                             &fixture_identity,
                                             &fixture_missing);
  if (fixture_known != 1 || fixture_identity == NULL) {
    if (fixture_missing) {
      (void)real_write_prelaunch(args->result, "missing-fixture",
                                 "fixture-absent", "not-started",
                                 args->bound);
    } else {
      (void)real_write_prelaunch(args->result, "harness-error", "refusal",
                                 "refused", args->bound);
    }
    goto cleanup;
  }
  fixture_status = t64_verified_input_prepare(
      args->fixture, fixture_identity->size, fixture_identity->sha256,
      T64_VERIFIED_FIXTURE_MAX_BYTES, &fixture);
  if (fixture_status != T64_VERIFIED_INPUT_OK) {
    if (fixture_status == T64_VERIFIED_INPUT_MISSING) {
      (void)real_write_prelaunch(args->result, "missing-fixture",
                                 "fixture-absent", "not-started",
                                 args->bound);
    } else {
      (void)real_write_prelaunch(args->result, "harness-error", "refusal",
                                 "refused", args->bound);
    }
    goto cleanup;
  }
  input_status = t64_verified_input_prepare(
      args->input, args->input_size, args->input_sha256,
      T64_VERIFIED_SCHEDULE_MAX_BYTES, &input);
  if (input_status != T64_VERIFIED_INPUT_OK ||
      real_schedule_digest(input.fd, args->input_size, canonical_input) != 0 ||
      real_result_initialize(&identities, fixture_identity,
                             canonical_input) != 0) {
    (void)real_write_prelaunch(args->result, "harness-error", "refusal",
                               "refused", args->bound);
    goto cleanup;
  }
  if (real_test_barrier(args) != 0) {
    (void)real_write_prelaunch(args->result, "harness-error", "refusal",
                               "refused", args->bound);
    goto cleanup;
  }
  start_ms = now_ms();
  if (real_resolve_sibling(self_path, runner_name, runner_path,
                           sizeof runner_path) != 0 ||
      real_resolve_sibling(self_path, "t64-result-check", checker_path,
                           sizeof checker_path) != 0) {
    (void)real_write_prelaunch(args->result, "harness-error", "exec-failed",
                               "not-started", args->bound);
    goto cleanup;
  }
  {
    int length = snprintf(ws.dir, sizeof ws.dir, "%s/t64real.XXXXXX",
                          args->work_dir);
    if (length <= 0 || (size_t)length >= sizeof ws.dir ||
        mkdtemp(ws.dir) == NULL) {
      (void)real_write_prelaunch(args->result, "harness-error", "usage",
                                 "not-started", args->bound);
      goto cleanup;
    }
  }
  ws.made_dir = 1;
  (void)snprintf(ws.report, sizeof ws.report, "%s/report", ws.dir);
  (void)snprintf(ws.capture, sizeof ws.capture, "%s/capture", ws.dir);
  report_fd = create_private(ws.report);
  capture_fd = create_private(ws.capture);
  if (report_fd < 0 || capture_fd < 0) {
    (void)real_write_prelaunch(args->result, "harness-error", "usage",
                               "not-started", args->bound);
    goto cleanup;
  }
  close(report_fd);
  report_fd = -1;
  close(capture_fd);
  capture_fd = -1;
  if (real_resolve_sibling(self_path, runner_name, runner_path,
                           sizeof runner_path) != 0) {
    goto before_launch_error;
  }
  if (pipe(exec_pipe) != 0 ||
      fcntl(exec_pipe[0], F_SETFD, FD_CLOEXEC) != 0 ||
      fcntl(exec_pipe[1], F_SETFD, FD_CLOEXEC) != 0 ||
      fcntl(exec_pipe[0], F_SETFL, O_NONBLOCK) != 0) {
    goto before_launch_error;
  }
  (void)snprintf(fixture_fd_text, sizeof fixture_fd_text, "%d", fixture.fd);
  (void)snprintf(input_fd_text, sizeof input_fd_text, "%d", input.fd);
  (void)snprintf(guest_ticks_text, sizeof guest_ticks_text, "%llu",
                 (unsigned long long)args->bound);
  deadline = args->wall_ms > UINT64_MAX - start_ms
                 ? UINT64_MAX
                 : start_ms + args->wall_ms;
  pid = fork();
  if (pid < 0) goto before_launch_error;
  if (pid == 0) {
    int devnull;
    char option_fixture[] = "--fixture-fd";
    char option_input[] = "--input-fd";
    char option_profile[] = "--profile";
    char option_ticks[] = "--guest-ticks";
    char profile_id[] = REAL_PROFILE_ID;
    char *const child_args[] = {
        runner_path, option_fixture, fixture_fd_text, option_input,
        input_fd_text, option_profile, profile_id, option_ticks,
        guest_ticks_text, NULL};
    setpgid(0, 0);
    limit.rlim_cur = 0;
    limit.rlim_max = 0;
    (void)setrlimit(RLIMIT_CORE, &limit);
    limit.rlim_cur = (rlim_t)(T64_REAL_RESULT_MAX_BYTES + FSIZE_SLACK);
    limit.rlim_max = limit.rlim_cur;
    (void)setrlimit(RLIMIT_FSIZE, &limit);
    devnull = open("/dev/null", O_RDONLY);
    if (devnull >= 0) (void)dup2(devnull, STDIN_FILENO);
    report_fd = open(ws.report, O_WRONLY | O_TRUNC | O_NOFOLLOW);
    capture_fd = open(ws.capture, O_WRONLY | O_TRUNC | O_NOFOLLOW);
    if (report_fd < 0 || capture_fd < 0 ||
        !real_clear_cloexec(fixture.fd) || !real_clear_cloexec(input.fd) ||
        dup2(report_fd, STDOUT_FILENO) < 0 ||
        dup2(capture_fd, STDERR_FILENO) < 0) {
      _exit(126);
    }
#ifdef T64_SUPERVISOR_TESTING
    if (args->test_fault != NULL &&
        setenv("T64_REAL_RUNNER_TEST_FAULT", args->test_fault, 1) != 0) {
      _exit(126);
    }
#endif
    close(report_fd);
    close(capture_fd);
    close(exec_pipe[0]);
    signal(SIGPIPE, SIG_DFL);
    execv(runner_path, child_args);
    {
      int exec_error = errno;
      (void)!write(exec_pipe[1], &exec_error, sizeof exec_error);
    }
    _exit(127);
  }
  (void)setpgid(pid, pid);
  close(exec_pipe[1]);
  exec_pipe[1] = -1;
  identities.bound_tick = args->bound;
  for (;;) {
    siginfo_t info;
    memset(&info, 0, sizeof info);
    if (waitid(P_PID, (id_t)pid, &info,
               WEXITED | WNOHANG | WNOWAIT) == 0 && info.si_pid == pid) {
      break;
    }
    if (g_term) {
      supervisor_signalled = true;
      break;
    }
    if (now_ms() >= deadline) {
      wall_backstop = true;
      break;
    }
    {
      struct timespec nap = {0, 2 * 1000 * 1000};
      (void)nanosleep(&nap, NULL);
    }
  }
  (void)kill(-pid, SIGKILL);
  (void)kill(pid, SIGKILL);
  while (waitpid(pid, &wait_status, 0) < 0 && errno == EINTR) {
  }
  pid = -1;
  if (WIFEXITED(wait_status)) {
    runner_status = WEXITSTATUS(wait_status);
  }
  {
    int error_number = 0;
    if (read(exec_pipe[0], &error_number, sizeof error_number) ==
        (ssize_t)sizeof error_number) {
      exec_failed = 1;
    }
    close(exec_pipe[0]);
    exec_pipe[0] = -1;
  }
  report_length = read_report(ws.report, (uint8_t *)report_bytes,
                              sizeof report_bytes);
  capture_valid = read_real_capture(ws.capture, &capture, &capture_length,
                                    &capture_total) == 0;
  report_valid = false;
  if (report_length > 0 &&
      (size_t)report_length <= T64_REAL_RESULT_MAX_BYTES && capture_valid) {
    report_schema_valid =
        real_checker_accepts(checker_path, ws.report);
    capture_matches_report = real_report_matches_capture(
        (const uint8_t *)report_bytes, (size_t)report_length, capture,
        capture_length, capture_total, fixture_identity, canonical_input);
    report_valid = report_schema_valid && capture_matches_report;
  }
  process_ok = !wall_backstop && !supervisor_signalled && !exec_failed &&
               WIFEXITED(wait_status) && runner_status == 0;
  if (args->capture_out != NULL && capture_valid &&
      atomic_write(args->capture_out, capture, capture_length) != 0) {
    capture_export_failed = true;
  }
  if (process_ok && report_valid && capture_valid &&
      !capture_export_failed) {
    if (atomic_write(args->result, report_bytes, (size_t)report_length) == 0) {
      exit_code = 0;
    }
  } else if (!wall_backstop && !supervisor_signalled && !exec_failed &&
             WIFEXITED(wait_status) && runner_status != 0 && report_valid &&
             capture_valid &&
             real_write_report_override(
                 args->result, (const uint8_t *)report_bytes,
                 (size_t)report_length, "runner-exit-nonzero") == 0) {
    exit_code = 1;
  } else if (capture_export_failed && report_valid && capture_valid &&
             real_write_report_override(args->result,
                                        (const uint8_t *)report_bytes,
                                        (size_t)report_length,
                                        "host-error") == 0) {
    exit_code = 1;
  } else {
    const char *kind = "host-error";
    const char *detail = "host-error";
    if (capture_export_failed) detail = "host-error";
    else if (wall_backstop) detail = "wall-backstop";
    else if (supervisor_signalled) detail = "supervisor-signalled";
    else if (exec_failed) detail = "exec-failed";
    else if (WIFSIGNALED(wait_status)) detail = "runner-signalled";
    else if (WIFEXITED(wait_status) && runner_status != 0)
      detail = "runner-exit-nonzero";
    else if (report_length == 0) {
      kind = "report-error";
      detail = "no-result";
    } else if (report_length < 0 ||
               (size_t)report_length > T64_REAL_RESULT_MAX_BYTES) {
      kind = "report-error";
      detail = "report-invalid";
    } else if (!capture_valid ||
               (report_schema_valid && !capture_matches_report)) {
      kind = "report-error";
      detail = "capture-invalid";
    } else {
      kind = "report-error";
      detail = "report-invalid";
    }
    if (real_write_postlaunch_error(args->result, &identities, kind, detail,
                                    capture, capture_length,
                                    capture_total,
                                    capture_valid) == 0) {
      exit_code = 1;
    }
  }
  {
    char meta_buffer[1024];
    size_t meta_length = 0u;
    if (build_meta(meta_buffer, sizeof meta_buffer, &meta_length, wait_status,
                   now_ms() - start_ms) == 0) {
      (void)atomic_write(args->meta, meta_buffer, meta_length);
    }
  }
  goto cleanup;

before_launch_error:
  (void)real_write_prelaunch(args->result, "harness-error", "exec-failed",
                             "not-started", args->bound);
  goto cleanup;

cleanup:
  if (exec_pipe[0] >= 0) close(exec_pipe[0]);
  if (exec_pipe[1] >= 0) close(exec_pipe[1]);
  if (report_fd >= 0) close(report_fd);
  if (capture_fd >= 0) close(capture_fd);
  if (pid > 0) {
    (void)kill(-pid, SIGKILL);
    (void)kill(pid, SIGKILL);
    while (waitpid(pid, &wait_status, 0) < 0 && errno == EINTR) {
    }
  }
  workset_cleanup(&ws);
  free(capture);
  t64_verified_input_release(&fixture);
  t64_verified_input_release(&input);
  return exit_code;
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
  if (a.real_mode) {
    if (parse_rc != 0) {
      if (result_path != NULL) {
        (void)real_write_prelaunch(result_path, "harness-error", "usage",
                                   "not-started", REAL_GUEST_TICKS);
      }
      return 2;
    }
    return real_run_mode(&a, argv[0]);
  }
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
