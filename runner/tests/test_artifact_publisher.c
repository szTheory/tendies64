/*
 * Controls for the single-file private no-clobber publisher. Every refusal
 * runs its accepted baseline in the same setup first and asserts the specific
 * error name, never just failure. Temporary directories live under
 * T64_PHASE09_EVIDENCE_DIR (default: a mkdtemp directory in TMPDIR).
 *
 * usage: test_artifact_publisher CASE
 *        test_artifact_publisher --list-cases
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "t64_artifact_publisher.h"

static char g_evidence[PATH_MAX];
static int g_failures;

static void check_impl(bool ok, const char *expr, int line) {
  if (!ok) {
    g_failures++;
    (void)fprintf(stderr, "FAIL line %d: %s\n", line, expr);
  }
}
#define CHECK(cond) check_impl((cond), #cond, __LINE__)

static bool join_path(char *out, size_t capacity, const char *a,
                      const char *b) {
  int length = snprintf(out, capacity, "%s/%s", a, b);
  return length > 0 && (size_t)length < capacity;
}

static bool prepare_evidence(void) {
  const char *configured = getenv("T64_PHASE09_EVIDENCE_DIR");
  const char *temp = getenv("TMPDIR");
  struct stat info;
  if (configured != NULL && configured[0] != '\0') {
    if (snprintf(g_evidence, sizeof g_evidence, "%s", configured) >=
        (int)sizeof g_evidence) {
      return false;
    }
    if (mkdir(g_evidence, 0700) != 0 && errno != EEXIST) return false;
    return stat(g_evidence, &info) == 0 && S_ISDIR(info.st_mode);
  }
  if (temp == NULL || temp[0] == '\0') temp = "/tmp";
  if (snprintf(g_evidence, sizeof g_evidence, "%s/t64-artifact.XXXXXX",
               temp) >= (int)sizeof g_evidence) {
    return false;
  }
  return mkdtemp(g_evidence) != NULL;
}

/* A fresh private (0700) directory for one scenario. */
static bool fresh_dir(char out[PATH_MAX]) {
  if (snprintf(out, PATH_MAX, "%s/pub.XXXXXX", g_evidence) >= PATH_MAX) {
    return false;
  }
  return mkdtemp(out) != NULL && chmod(out, 0700) == 0;
}

static size_t list_directory(const char *path, char first[256]) {
  DIR *dir = opendir(path);
  struct dirent *entry;
  size_t count = 0u;
  first[0] = '\0';
  if (dir == NULL) return (size_t)-1;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    if (count == 0u) (void)snprintf(first, 256u, "%s", entry->d_name);
    count++;
  }
  (void)closedir(dir);
  return count;
}

static void fill_pattern(uint8_t *bytes, size_t length, uint64_t offset) {
  size_t i;
  for (i = 0u; i < length; i++) {
    bytes[i] = (uint8_t)(((offset + (uint64_t)i) * 7u + 3u) % 256u);
  }
}

static bool read_back(const char *path, uint8_t *bytes, size_t capacity,
                      size_t *length_out) {
  int fd = open(path, O_RDONLY | O_NOFOLLOW);
  size_t total = 0u;
  if (fd < 0) return false;
  while (total < capacity) {
    ssize_t count = read(fd, bytes + total, capacity - total);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) break;
    total += (size_t)count;
  }
  (void)close(fd);
  *length_out = total;
  return true;
}

/* Publishes `size` pattern bytes as `leaf`; true when finalize succeeds. */
static bool publish_pattern(const char *dir, const char *leaf, size_t size) {
  const char *error = NULL;
  uint8_t bytes[2048];
  t64_artifact_publisher *p;
  bool ok;
  if (size > sizeof bytes) return false;
  p = t64_artifact_publisher_open(dir, leaf, (uint64_t)size, &error);
  if (p == NULL) return false;
  fill_pattern(bytes, size, 0u);
  ok = t64_artifact_publisher_write(p, bytes, size) &&
       t64_artifact_publisher_finalize(p);
  if (!ok) t64_artifact_publisher_abort(p);
  t64_artifact_publisher_destroy(p);
  return ok;
}

/* The published leaf holds exactly the pattern, mode 0600, one link. */
static bool leaf_is_published(const char *dir, const char *leaf, size_t size) {
  char path[PATH_MAX];
  uint8_t bytes[2048];
  uint8_t expected[2048];
  size_t length = 0u;
  struct stat info;
  if (!join_path(path, sizeof path, dir, leaf) ||
      lstat(path, &info) != 0 || !S_ISREG(info.st_mode) ||
      (info.st_mode & 0777) != 0600 || info.st_nlink != 1 ||
      (size_t)info.st_size != size || size > sizeof bytes) {
    return false;
  }
  fill_pattern(expected, size, 0u);
  return read_back(path, bytes, sizeof bytes, &length) && length == size &&
         memcmp(bytes, expected, size) == 0;
}

/* ---- publish-and-refusals ----------------------------------------------- */

static int case_publish_and_refusals(void) {
  char dir[PATH_MAX];
  char first[256];
  const char *error = NULL;
  t64_artifact_publisher *p;
  size_t i;

  /* Accepted publish: exactly the requested bytes, no stage left. */
  CHECK(fresh_dir(dir));
  CHECK(t64_artifact_preflight(dir, &error));
  CHECK(list_directory(dir, first) == 0u);
  CHECK(publish_pattern(dir, "probe.sta", 1000u));
  CHECK(list_directory(dir, first) == 1u);
  CHECK(strcmp(first, "probe.sta") == 0);
  CHECK(leaf_is_published(dir, "probe.sta", 1000u));

  /* An existing target is refused and its bytes are unchanged. */
  error = NULL;
  p = t64_artifact_publisher_open(dir, "probe.sta", 1000u, &error);
  CHECK(p == NULL);
  CHECK(error != NULL && strcmp(error, "artifact-exists") == 0);
  CHECK(leaf_is_published(dir, "probe.sta", 1000u));
  CHECK(list_directory(dir, first) == 1u);

  /* Leaf-name refusals, each in a fresh directory after its own baseline. */
  {
    char long_stem[64];
    const char *bad[11];
    size_t count = 0u;
    (void)memset(long_stem, 'a', 37u);
    (void)snprintf(long_stem + 37, sizeof long_stem - 37u, ".sta");
    bad[count++] = "";
    bad[count++] = "../x.sta";
    bad[count++] = ".hidden.sta";
    bad[count++] = "a/b.sta";
    bad[count++] = "A.sta";
    bad[count++] = "x.STA";
    bad[count++] = "x.sta.";
    bad[count++] = "x.st";
    bad[count++] = "x.stat";
    bad[count++] = long_stem;
    bad[count++] = "-x.sta";
    for (i = 0u; i < count; i++) {
      CHECK(fresh_dir(dir));
      CHECK(publish_pattern(dir, "ok.sta", 64u));
      error = NULL;
      p = t64_artifact_publisher_open(dir, bad[i], 64u, &error);
      CHECK(p == NULL);
      CHECK(error != NULL && strcmp(error, "artifact-leaf-name") == 0);
      /* Nothing was created beside the accepted baseline. */
      CHECK(list_directory(dir, first) == 1u);
      CHECK(strcmp(first, "ok.sta") == 0);
    }
    /* Boundary: a 32-character stem is accepted, a 33-character one is not. */
    CHECK(fresh_dir(dir));
    {
      char edge[64];
      (void)memset(edge, 'b', 32u);
      (void)snprintf(edge + 32, sizeof edge - 32u, ".rpl");
      CHECK(publish_pattern(dir, edge, 16u));
      (void)memset(edge, 'b', 33u);
      (void)snprintf(edge + 33, sizeof edge - 33u, ".rpl");
      error = NULL;
      p = t64_artifact_publisher_open(dir, edge, 16u, &error);
      CHECK(p == NULL);
      CHECK(error != NULL && strcmp(error, "artifact-leaf-name") == 0);
    }
  }

  /* Parent refusals: the accepted directory publishes, the unsafe ones do not. */
  {
    char real_dir[PATH_MAX];
    char link_dir[PATH_MAX];
    char dotted[PATH_MAX];
    char root[PATH_MAX];
    CHECK(fresh_dir(root));
    CHECK(join_path(real_dir, sizeof real_dir, root, "real"));
    CHECK(mkdir(real_dir, 0700) == 0);
    CHECK(publish_pattern(real_dir, "ok.sta", 32u));
    /* A symlink path component and a symlink as the directory itself. */
    CHECK(join_path(link_dir, sizeof link_dir, root, "link"));
    CHECK(symlink("real", link_dir) == 0);
    error = NULL;
    CHECK(!t64_artifact_preflight(link_dir, &error));
    CHECK(error != NULL && strcmp(error, "artifact-dir-unsafe") == 0);
    error = NULL;
    p = t64_artifact_publisher_open(link_dir, "x.sta", 8u, &error);
    CHECK(p == NULL);
    CHECK(error != NULL && strcmp(error, "artifact-dir-unsafe") == 0);
    {
      char through[PATH_MAX];
      CHECK(join_path(through, sizeof through, link_dir, "."));
      CHECK(snprintf(dotted, sizeof dotted, "%s/real/../real", root) > 0);
      error = NULL;
      p = t64_artifact_publisher_open(dotted, "x.sta", 8u, &error);
      CHECK(p == NULL);
      CHECK(error != NULL && strcmp(error, "artifact-dir-unsafe") == 0);
    }
    /* A missing directory. */
    {
      char missing[PATH_MAX];
      CHECK(join_path(missing, sizeof missing, root, "absent"));
      error = NULL;
      p = t64_artifact_publisher_open(missing, "x.sta", 8u, &error);
      CHECK(p == NULL);
      CHECK(error != NULL && strcmp(error, "artifact-dir-unsafe") == 0);
    }
    /* Group- and world-writable directories, after the same directory
     * accepted a publish while owner-only. */
    CHECK(fresh_dir(dir));
    CHECK(publish_pattern(dir, "ok.sta", 32u));
    CHECK(chmod(dir, 0770) == 0);
    error = NULL;
    p = t64_artifact_publisher_open(dir, "x.sta", 8u, &error);
    CHECK(p == NULL);
    CHECK(error != NULL && strcmp(error, "artifact-dir-unsafe") == 0);
    error = NULL;
    CHECK(!t64_artifact_preflight(dir, &error));
    CHECK(error != NULL && strcmp(error, "artifact-dir-unsafe") == 0);
    CHECK(chmod(dir, 0707) == 0);
    error = NULL;
    p = t64_artifact_publisher_open(dir, "x.sta", 8u, &error);
    CHECK(p == NULL);
    CHECK(error != NULL && strcmp(error, "artifact-dir-unsafe") == 0);
    CHECK(chmod(dir, 0700) == 0);
    CHECK(list_directory(dir, first) == 1u);
    CHECK(publish_pattern(dir, "next.sta", 8u));
  }

  /* Cap boundary: exactly the cap succeeds, one more byte is refused and
   * nothing is published. */
  {
    uint8_t bytes[16];
    fill_pattern(bytes, sizeof bytes, 0u);
    CHECK(fresh_dir(dir));
    error = NULL;
    p = t64_artifact_publisher_open(dir, "cap.sta", 10u, &error);
    CHECK(p != NULL);
    if (p != NULL) {
      CHECK(t64_artifact_publisher_write(p, bytes, 10u));
      CHECK(t64_artifact_publisher_written(p) == 10u);
      CHECK(t64_artifact_publisher_finalize(p));
      t64_artifact_publisher_destroy(p);
    }
    CHECK(list_directory(dir, first) == 1u);
    CHECK(leaf_is_published(dir, "cap.sta", 10u));

    CHECK(fresh_dir(dir));
    p = t64_artifact_publisher_open(dir, "cap.sta", 10u, &error);
    CHECK(p != NULL);
    if (p != NULL) {
      CHECK(t64_artifact_publisher_write(p, bytes, 10u));
      CHECK(!t64_artifact_publisher_write(p, bytes, 1u));
      CHECK(strcmp(t64_artifact_publisher_error(p), "artifact-cap") == 0);
      CHECK(t64_artifact_publisher_written(p) == 10u);
      CHECK(!t64_artifact_publisher_finalize(p));
      t64_artifact_publisher_abort(p);
      t64_artifact_publisher_destroy(p);
    }
    CHECK(list_directory(dir, first) == 0u);

    /* The cap arithmetic does not wrap on a huge length. */
    CHECK(fresh_dir(dir));
    p = t64_artifact_publisher_open(dir, "cap.sta", 10u, &error);
    CHECK(p != NULL);
    if (p != NULL) {
      CHECK(t64_artifact_publisher_write(p, bytes, 4u));
      CHECK(!t64_artifact_publisher_write(p, bytes, SIZE_MAX));
      CHECK(strcmp(t64_artifact_publisher_error(p), "artifact-cap") == 0);
      CHECK(t64_artifact_publisher_written(p) == 4u);
      t64_artifact_publisher_abort(p);
      t64_artifact_publisher_destroy(p);
    }
    CHECK(list_directory(dir, first) == 0u);

    /* A zero cap and a cap above the maximum are refused. */
    CHECK(fresh_dir(dir));
    CHECK(publish_pattern(dir, "ok.sta", 8u));
    error = NULL;
    p = t64_artifact_publisher_open(dir, "z.sta", 0u, &error);
    CHECK(p == NULL);
    CHECK(error != NULL && strcmp(error, "artifact-cap") == 0);
    error = NULL;
    p = t64_artifact_publisher_open(dir, "z.sta", T64_ARTIFACT_MAX_BYTES + 1u,
                                    &error);
    CHECK(p == NULL);
    CHECK(error != NULL && strcmp(error, "artifact-cap") == 0);
    CHECK(list_directory(dir, first) == 1u);
    /* The maximum itself is accepted. */
    p = t64_artifact_publisher_open(dir, "max.sta", T64_ARTIFACT_MAX_BYTES,
                                    &error);
    CHECK(p != NULL);
    if (p != NULL) {
      t64_artifact_publisher_abort(p);
      t64_artifact_publisher_destroy(p);
    }
    CHECK(list_directory(dir, first) == 1u);
  }
  return g_failures == 0 ? 0 : 1;
}

/* ---- substitution ------------------------------------------------------- */

typedef enum {
  SUBSTITUTE_STAGED_FILE,
  SUBSTITUTE_PARENT,
  SUBSTITUTE_TARGET,
  SUBSTITUTE_STAGE_DIR
} substitution;

typedef struct {
  substitution kind;
  char victim[PATH_MAX];  /* a file the attacker wants overwritten */
  char directory[PATH_MAX];
  char moved[PATH_MAX];
  char decoy[PATH_MAX];
} hook_context;

/* The substitution is the control's attack. If it cannot be mounted the
 * publisher is not attacked and the control's own comparison fails, so a
 * failure here is only reported. (glibc marks symlink and symlinkat
 * warn_unused_result, which a void cast does not silence.) */
static void attack_symlinkat(const char *target, int parent_fd, const char *name) {
  if (symlinkat(target, parent_fd, name) != 0) {
    (void)fprintf(stderr, "substitution symlink could not be created\n");
  }
}

static void attack_symlink(const char *target, const char *path) {
  if (symlink(target, path) != 0) {
    (void)fprintf(stderr, "substitution symlink could not be created\n");
  }
}

static void precommit_hook(void *context, int parent_fd,
                           const char *stage_name, const char *leaf) {
  hook_context *c = (hook_context *)context;
  char staged[PATH_MAX + 128];
  (void)snprintf(staged, sizeof staged, "%s/%s", stage_name, leaf);
  switch (c->kind) {
    case SUBSTITUTE_STAGED_FILE:
      (void)unlinkat(parent_fd, staged, 0);
      attack_symlinkat(c->victim, parent_fd, staged);
      break;
    case SUBSTITUTE_PARENT:
      (void)rename(c->directory, c->moved);
      attack_symlink(c->decoy, c->directory);
      break;
    case SUBSTITUTE_TARGET:
      attack_symlinkat(c->victim, parent_fd, leaf);
      break;
    case SUBSTITUTE_STAGE_DIR: {
      char away[PATH_MAX];
      int fd;
      (void)snprintf(away, sizeof away, "%s.away", stage_name);
      (void)renameat(parent_fd, stage_name, parent_fd, away);
      (void)mkdirat(parent_fd, stage_name, 0700);
      fd = openat(parent_fd, staged, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW,
                  0600);
      if (fd >= 0) (void)close(fd);
      break;
    }
  }
}

static bool write_victim(const char *path) {
  static const char text[] = "victim bytes that must never change";
  int fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
  if (fd < 0) return false;
  if (write(fd, text, sizeof text - 1u) != (ssize_t)(sizeof text - 1u)) {
    (void)close(fd);
    return false;
  }
  return close(fd) == 0;
}

static bool victim_unchanged(const char *path) {
  static const char text[] = "victim bytes that must never change";
  uint8_t bytes[128];
  size_t length = 0u;
  return read_back(path, bytes, sizeof bytes, &length) &&
         length == sizeof text - 1u &&
         memcmp(bytes, text, sizeof text - 1u) == 0;
}

/* Runs one substitution scenario after its accepted baseline: the same hook
 * with SUBSTITUTE_NONE-like behavior is the plain publish_pattern above. */
static int run_substitution(substitution kind) {
  char root[PATH_MAX];
  char dir[PATH_MAX];
  char first[256];
  const char *error = NULL;
  hook_context context;
  t64_artifact_publisher *p;
  uint8_t bytes[64];
  bool finalized;
  memset(&context, 0, sizeof context);
  context.kind = kind;
  CHECK(fresh_dir(root));
  CHECK(join_path(dir, sizeof dir, root, "art"));
  CHECK(mkdir(dir, 0700) == 0);
  CHECK(join_path(context.victim, sizeof context.victim, root, "victim.dat"));
  CHECK(write_victim(context.victim));
  CHECK(snprintf(context.directory, sizeof context.directory, "%s", dir) > 0);
  CHECK(join_path(context.moved, sizeof context.moved, root, "moved"));
  CHECK(join_path(context.decoy, sizeof context.decoy, root, "decoy"));
  CHECK(mkdir(context.decoy, 0700) == 0);

  /* Accepted baseline in the same setup. */
  CHECK(publish_pattern(dir, "base.sta", 64u));
  CHECK(leaf_is_published(dir, "base.sta", 64u));

  fill_pattern(bytes, sizeof bytes, 0u);
  p = t64_artifact_publisher_open(dir, "probe.sta", 64u, &error);
  CHECK(p != NULL);
  if (p == NULL) return 1;
  CHECK(t64_artifact_publisher_write(p, bytes, sizeof bytes));
  t64_artifact_publisher_set_precommit_hook_for_test(p, precommit_hook,
                                                     &context);
  finalized = t64_artifact_publisher_finalize(p);
  switch (kind) {
    case SUBSTITUTE_STAGED_FILE:
      /* The staged file replaced by a symlink to the victim is refused at
       * finalize; the victim is untouched and nothing is published. */
      CHECK(!finalized);
      CHECK(strcmp(t64_artifact_publisher_error(p), "artifact-substitution") ==
            0);
      CHECK(victim_unchanged(context.victim));
      {
        char path[PATH_MAX];
        struct stat info;
        CHECK(join_path(path, sizeof path, dir, "probe.sta"));
        CHECK(lstat(path, &info) != 0);
      }
      break;
    case SUBSTITUTE_PARENT:
      /* The directory renamed away and a symlink planted at its path: the
       * publish goes into the original directory object and the symlink
       * target stays empty. */
      CHECK(finalized);
      CHECK(list_directory(context.decoy, first) == 0u);
      CHECK(leaf_is_published(context.moved, "probe.sta", 64u));
      CHECK(leaf_is_published(context.moved, "base.sta", 64u));
      CHECK(list_directory(context.moved, first) == 2u);
      break;
    case SUBSTITUTE_TARGET:
      /* A symlink created at the target leaf is never replaced through. */
      CHECK(!finalized);
      CHECK(strcmp(t64_artifact_publisher_error(p), "artifact-exists") == 0 ||
            strcmp(t64_artifact_publisher_error(p), "artifact-rename") == 0);
      CHECK(victim_unchanged(context.victim));
      break;
    case SUBSTITUTE_STAGE_DIR:
      /* The stage swapped for a look-alike is refused. */
      CHECK(!finalized);
      CHECK(strcmp(t64_artifact_publisher_error(p), "artifact-substitution") ==
            0);
      {
        char path[PATH_MAX];
        struct stat info;
        CHECK(join_path(path, sizeof path, dir, "probe.sta"));
        CHECK(lstat(path, &info) != 0);
      }
      break;
  }
  t64_artifact_publisher_abort(p);
  t64_artifact_publisher_destroy(p);
  return g_failures == 0 ? 0 : 1;
}

static int case_substitution(void) {
  (void)run_substitution(SUBSTITUTE_STAGED_FILE);
  (void)run_substitution(SUBSTITUTE_PARENT);
  (void)run_substitution(SUBSTITUTE_TARGET);
  (void)run_substitution(SUBSTITUTE_STAGE_DIR);
  return g_failures == 0 ? 0 : 1;
}

typedef struct {
  const char *name;
  int (*run)(void);
} case_entry;

static const case_entry cases[] = {
    {"publish-and-refusals", case_publish_and_refusals},
    {"substitution", case_substitution},
};

int main(int argc, char **argv) {
  size_t i;
  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
      (void)puts(cases[i].name);
    }
    return 0;
  }
  if (argc != 2) {
    (void)fprintf(stderr, "usage: %s CASE|--list-cases\n", argv[0]);
    return 2;
  }
  if (!prepare_evidence()) {
    (void)fprintf(stderr, "evidence directory unavailable\n");
    return 2;
  }
  for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
    if (strcmp(argv[1], cases[i].name) == 0) {
      int status = cases[i].run();
      (void)fprintf(stderr, "case %s: %s\n", cases[i].name,
                    status == 0 ? "ok" : "FAILED");
      return status;
    }
  }
  (void)fprintf(stderr, "unknown case: %s\n", argv[1]);
  return 2;
}
