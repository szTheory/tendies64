#ifndef T64_TEST_TREE_H
#define T64_TEST_TREE_H

/* Per-process evidence scope for the runner tests (plan 09-10, FX-17, L3 IN-05).
 *
 * The runner tests keep their run directories under an evidence directory that
 * CTest shares between concurrent tests (or a fresh mkdtemp directory). Without
 * a scope every run leaves authored replays, 100,000-byte probe files, retained
 * stages and results behind. t64_test_scope_open() creates one private
 * subdirectory of the evidence directory for this process, points the test at
 * it and removes it (and a mkdtemp parent it was told it created) at normal
 * process exit; other processes' scopes are never touched, and a forked child
 * that exits does not remove the parent's scope. Set T64_KEEP_TEST_EVIDENCE to
 * any value to keep the tree for debugging.
 *
 * A test whose evidence is read after it exits (the hosted collector reads the
 * retained run directories of runner/tests/test_pad_poll.c by the path its
 * marker line names) calls t64_test_scope_keep() once it has retained a run:
 * that process then leaves its whole scope in place, which is the retention the
 * collector interface of plan 09-03.1 contracts. Plan 09-10 Task 6 found that
 * the cleanup without this call removed those directories and made the hosted
 * "Emit authored Phase 9 evidence" step refuse on both jobs (run 44776046324).
 *
 * Header-only, static inline: it holds no test logic. */

#include <dirent.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static char t64_scope_directory[PATH_MAX];
static char t64_scope_created_parent[PATH_MAX];
static pid_t t64_scope_owner;
static bool t64_scope_keep;

/* Removes a directory tree without following symlinks. */
static inline void t64_test_remove_tree(const char *path) {
  DIR *dir = opendir(path);
  struct dirent *entry;
  if (dir != NULL) {
    while ((entry = readdir(dir)) != NULL) {
      char child[PATH_MAX];
      struct stat info;
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 ||
          snprintf(child, sizeof child, "%s/%s", path, entry->d_name) >=
              (int)sizeof child) {
        continue;
      }
      if (lstat(child, &info) == 0 && S_ISDIR(info.st_mode)) {
        t64_test_remove_tree(child);
      } else {
        (void)unlink(child);
      }
    }
    (void)closedir(dir);
  }
  (void)rmdir(path);
}

static inline void t64_test_scope_cleanup(void) {
  if (t64_scope_owner != getpid() || t64_scope_directory[0] == '\0' ||
      t64_scope_keep || getenv("T64_KEEP_TEST_EVIDENCE") != NULL) {
    return;
  }
  t64_test_remove_tree(t64_scope_directory);
  t64_scope_directory[0] = '\0';
  if (t64_scope_created_parent[0] != '\0') {
    (void)rmdir(t64_scope_created_parent);
  }
}

/* Leaves this process's scope in place at exit: the evidence of a retained run
 * is read by another process after this one has ended. */
static inline void t64_test_scope_keep(void) { t64_scope_keep = true; }

/* `evidence` holds the evidence directory on entry and this process's scope on
 * return. `created_parent` is true when the directory itself was made by
 * mkdtemp for this process (and so is removed with the scope). */
static inline bool t64_test_scope_open(char *evidence, size_t capacity,
                                       bool created_parent) {
  char scope[PATH_MAX];
  if (snprintf(scope, sizeof scope, "%s/run.XXXXXX", evidence) >=
          (int)sizeof scope ||
      mkdtemp(scope) == NULL || strlen(scope) >= capacity) {
    return false;
  }
  if (created_parent) {
    (void)snprintf(t64_scope_created_parent, sizeof t64_scope_created_parent,
                   "%s", evidence);
  }
  (void)snprintf(t64_scope_directory, sizeof t64_scope_directory, "%s", scope);
  (void)snprintf(evidence, capacity, "%s", scope);
  t64_scope_owner = getpid();
  return atexit(t64_test_scope_cleanup) == 0;
}

#endif /* T64_TEST_TREE_H */
