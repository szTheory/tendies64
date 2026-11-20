/* Single-file private no-clobber publisher.
 *
 * Provenance: a copy, with a one-file delta, of the descriptor-walk, private
 * stage, no-replace probe and identity-checked cleanup in
 * runner/t64_raw_capture.c (which is not modified). Maintained with the
 * runner; see runner/t64_artifact_publisher.h for the delta. */
#define _POSIX_C_SOURCE 200809L
#if defined(__linux__)
#define _GNU_SOURCE
#endif

#include "t64_artifact_publisher.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <sys/stdio.h>
#elif defined(__linux__)
#include <linux/fs.h>
#include <sys/syscall.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define ART_STAGE_CAPACITY 96u
#define ART_LEAF_CAPACITY 40u /* 32 stem + '.' + 3 extension + NUL, rounded */
#define ART_WRITE_CHUNK (1u << 20)

struct t64_artifact_publisher {
  int parent_fd;
  int stage_fd;
  int file_fd;
  char leaf[ART_LEAF_CAPACITY];
  char stage_name[ART_STAGE_CAPACITY];
  dev_t stage_device;
  ino_t stage_inode;
  dev_t file_device;
  ino_t file_inode;
  uint64_t byte_cap;
  uint64_t written;
  const char *error;
  bool committed;
#ifdef T64_REAL_RUNNER_TESTING
  t64_artifact_precommit_hook hook;
  void *hook_context;
  t64_artifact_precommit_hook postcommit_hook;
  void *postcommit_context;
  unsigned sync_faults;
  bool preserve_stage;
#endif
};

#ifdef T64_REAL_RUNNER_TESTING
/* Test twins only: a forced failure of the directory sync of the preflight. */
static bool g_preflight_sync_fault;
#endif

/* ---- leaf grammar ------------------------------------------------------- */

static bool leaf_is_valid(const char *leaf) {
  size_t length;
  size_t dot = 0u;
  size_t dots = 0u;
  size_t i;
  if (leaf == NULL) return false;
  length = strnlen(leaf, 64u);
  if (length == 0u || length >= 64u) return false;
  for (i = 0u; i < length; i++) {
    if (leaf[i] == '.') {
      dots++;
      dot = i;
    }
  }
  if (dots != 1u || dot < 1u || dot > 32u || length - dot - 1u != 3u) {
    return false;
  }
  for (i = 0u; i < dot; i++) {
    const char c = leaf[i];
    const bool alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    if (!(alnum || (i != 0u && c == '-'))) return false;
  }
  for (i = dot + 1u; i < length; i++) {
    if (leaf[i] < 'a' || leaf[i] > 'z') return false;
  }
  return true;
}

/* ---- descriptors, identity ---------------------------------------------- */

static int open_directory_nofollow(int parent_fd, const char *name) {
  int fd = openat(parent_fd, name,
                  O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  struct stat status;
  if (fd < 0) return -1;
  if (fstat(fd, &status) != 0 || !S_ISDIR(status.st_mode)) {
    int saved_errno = errno == 0 ? ENOTDIR : errno;
    (void)close(fd);
    errno = saved_errno;
    return -1;
  }
  return fd;
}

/* Walks the path one component at a time without following symlinks and
 * refuses "." and ".." components. */
static int open_directory_path(const char *path) {
  char copy[PATH_MAX];
  char *cursor;
  int current_fd;
  size_t length;
  bool absolute;
  if (path == NULL || path[0] == '\0') {
    errno = EINVAL;
    return -1;
  }
  length = strnlen(path, sizeof copy);
  if (length == 0u || length >= sizeof copy) {
    errno = ENAMETOOLONG;
    return -1;
  }
  memcpy(copy, path, length + 1u);
  absolute = copy[0] == '/';
  current_fd = open(absolute ? "/" : ".",
                    O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  if (current_fd < 0) return -1;
  cursor = copy + (absolute ? 1 : 0);
  while (*cursor != '\0') {
    char *component = cursor;
    char *slash = strchr(cursor, '/');
    size_t component_length;
    int next_fd;
    if (slash != NULL) {
      *slash = '\0';
      cursor = slash + 1;
    } else {
      cursor += strlen(cursor);
    }
    component_length = strlen(component);
    if (component_length == 0u) continue;
    if ((component_length == 1u && component[0] == '.') ||
        (component_length == 2u && component[0] == '.' &&
         component[1] == '.')) {
      (void)close(current_fd);
      errno = EINVAL;
      return -1;
    }
    next_fd = open_directory_nofollow(current_fd, component);
    if (next_fd < 0) {
      int saved_errno = errno;
      (void)close(current_fd);
      errno = saved_errno;
      return -1;
    }
    (void)close(current_fd);
    current_fd = next_fd;
  }
  return current_fd;
}

/* Opens the artifact directory and requires an owner-only directory. */
static int open_artifact_directory(const char *directory) {
  struct stat status;
  int fd = open_directory_path(directory);
  if (fd < 0) return -1;
  if (fstat(fd, &status) != 0 || !S_ISDIR(status.st_mode) ||
      status.st_uid != geteuid() || (status.st_mode & 0022) != 0) {
    (void)close(fd);
    errno = EACCES;
    return -1;
  }
  return fd;
}

static int rename_noreplace(int old_directory_fd, const char *old_name,
                            int new_directory_fd, const char *new_name) {
#if defined(__APPLE__)
  return renameatx_np(old_directory_fd, old_name, new_directory_fd, new_name,
                      RENAME_EXCL);
#elif defined(__linux__) && defined(SYS_renameat2) && defined(RENAME_NOREPLACE)
  return (int)syscall(SYS_renameat2, old_directory_fd, old_name,
                      new_directory_fd, new_name, RENAME_NOREPLACE);
#else
  (void)old_directory_fd;
  (void)old_name;
  (void)new_directory_fd;
  (void)new_name;
  errno = ENOTSUP;
  return -1;
#endif
}

static bool make_stage_name(const char *kind, unsigned attempt,
                            char name_out[ART_STAGE_CAPACITY]) {
  struct timespec now;
  int length;
  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return false;
  length = snprintf(name_out, ART_STAGE_CAPACITY, ".t64-art-%s-%ld-%llx-%u",
                    kind, (long)getpid(), (unsigned long long)now.tv_nsec,
                    attempt);
  return length > 0 && (size_t)length < ART_STAGE_CAPACITY;
}

/* A directory entry that is still the directory created by this process. */
static bool owned_directory_entry(int directory_fd, const char *name,
                                  dev_t device, ino_t inode) {
  struct stat status;
  return fstatat(directory_fd, name, &status, AT_SYMLINK_NOFOLLOW) == 0 &&
         S_ISDIR(status.st_mode) && status.st_dev == device &&
         status.st_ino == inode && status.st_uid == geteuid();
}

static bool private_directory_entry(int directory_fd, const char *name,
                                    dev_t device, ino_t inode) {
  struct stat status;
  return owned_directory_entry(directory_fd, name, device, inode) &&
         fstatat(directory_fd, name, &status, AT_SYMLINK_NOFOLLOW) == 0 &&
         (status.st_mode & 0777) == 0700;
}

/* Creates a private 0700 stage directory inside parent_fd and opens it. The
 * identity is recorded before any later check so cleanup can still remove an
 * empty container this attempt created. */
static bool create_stage_directory(int parent_fd,
                                   char name_out[ART_STAGE_CAPACITY],
                                   int *fd_out, dev_t *device_out,
                                   ino_t *inode_out) {
  unsigned attempt;
  for (attempt = 0u; attempt < 32u; attempt++) {
    struct stat created;
    struct stat opened;
    struct stat parent;
    int fd;
    if (!make_stage_name("stage", attempt, name_out)) return false;
    if (mkdirat(parent_fd, name_out, 0700) != 0) {
      if (errno == EEXIST) continue;
      return false;
    }
    if (fstatat(parent_fd, name_out, &created, AT_SYMLINK_NOFOLLOW) != 0) {
      return false;
    }
    *device_out = created.st_dev;
    *inode_out = created.st_ino;
    if (fstat(parent_fd, &parent) != 0 || !S_ISDIR(created.st_mode) ||
        created.st_uid != geteuid() || created.st_dev != parent.st_dev) {
      return false;
    }
    fd = open_directory_nofollow(parent_fd, name_out);
    if (fd < 0) return false;
    if (fstat(fd, &opened) != 0 || !S_ISDIR(opened.st_mode) ||
        opened.st_uid != geteuid() || opened.st_dev != created.st_dev ||
        opened.st_ino != created.st_ino || fchmod(fd, 0700) != 0 ||
        !private_directory_entry(parent_fd, name_out, created.st_dev,
                                 created.st_ino)) {
      (void)close(fd);
      return false;
    }
    *fd_out = fd;
    return true;
  }
  errno = EEXIST;
  return false;
}

/* Creates the private staged file (0600, one link, owned by the caller). */
static int create_staged_file(int stage_fd, const char *name,
                              dev_t *device_out, ino_t *inode_out) {
  struct stat status;
  int fd = openat(stage_fd, name,
                  O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
  if (fd < 0) return -1;
  if (fchmod(fd, 0600) != 0 || fstat(fd, &status) != 0 ||
      !S_ISREG(status.st_mode) || status.st_nlink != 1 ||
      status.st_uid != geteuid() || (status.st_mode & 0777) != 0600) {
    (void)close(fd);
    (void)unlinkat(stage_fd, name, 0);
    return -1;
  }
  *device_out = status.st_dev;
  *inode_out = status.st_ino;
  return fd;
}

static bool write_all_fd(int fd, const uint8_t *bytes, size_t length) {
  size_t offset = 0u;
  while (offset < length) {
    size_t chunk = length - offset;
    ssize_t count;
    if (chunk > ART_WRITE_CHUNK) chunk = ART_WRITE_CHUNK;
    count = write(fd, bytes + offset, chunk);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) return false;
    offset += (size_t)count;
  }
  return true;
}

/* ---- preflight ---------------------------------------------------------- */

bool t64_artifact_preflight(const char *directory, const char **error_out) {
  static const char probe_leaf[] = "probe-a.tmp";
  static const char second_leaf[] = "probe-b.tmp";
  char stage_name[ART_STAGE_CAPACITY];
  char target_name[ART_STAGE_CAPACITY];
  dev_t stage_device = 0;
  ino_t stage_inode = 0;
  dev_t first_device = 0;
  ino_t first_inode = 0;
  dev_t second_device = 0;
  ino_t second_inode = 0;
  const char *error = NULL;
  bool target_created = false;
  bool stage_created = false;
  int parent_fd;
  int stage_fd = -1;
  int file_fd = -1;
  if (error_out != NULL) *error_out = NULL;
  parent_fd = open_artifact_directory(directory);
  if (parent_fd < 0) {
    if (error_out != NULL) *error_out = "artifact-dir-unsafe";
    return false;
  }
  if (!create_stage_directory(parent_fd, stage_name, &stage_fd, &stage_device,
                              &stage_inode)) {
    /* The directory object may exist even when a later check refused it. */
    if (stage_device != 0 || stage_inode != 0) {
      (void)unlinkat(parent_fd, stage_name, AT_REMOVEDIR);
    }
    (void)close(parent_fd);
    if (error_out != NULL) *error_out = "artifact-stage";
    return false;
  }
  stage_created = true;
  if (!make_stage_name("probe", 0u, target_name)) {
    error = "artifact-stage";
    goto cleanup;
  }
  file_fd = create_staged_file(stage_fd, probe_leaf, &first_device,
                               &first_inode);
  if (file_fd < 0) {
    error = "artifact-stage";
    goto cleanup;
  }
  (void)close(file_fd);
  file_fd = -1;
  if (rename_noreplace(stage_fd, probe_leaf, parent_fd, target_name) != 0) {
    error = "artifact-no-replace-unsupported";
    goto cleanup;
  }
  target_created = true;
  /* A real no-replace primitive must refuse an existing target, not silently
   * replace it. */
  file_fd = create_staged_file(stage_fd, second_leaf, &second_device,
                               &second_inode);
  if (file_fd < 0) {
    error = "artifact-stage";
    goto cleanup;
  }
  (void)close(file_fd);
  file_fd = -1;
  if (rename_noreplace(stage_fd, second_leaf, parent_fd, target_name) == 0 ||
      errno != EEXIST) {
    error = "artifact-no-replace-unsupported";
  }
  /* The directory sync that makes a rename durable is part of the publish
   * transaction; a filesystem that cannot do it is refused here, before the
   * guest runs, so a publish never has to fail on it after the commit. */
  if (error == NULL) {
#ifdef T64_REAL_RUNNER_TESTING
    if (g_preflight_sync_fault) {
      error = "artifact-write";
    } else
#endif
    if (fsync(parent_fd) != 0) {
      error = "artifact-write";
    }
  }
cleanup:
  if (file_fd >= 0) (void)close(file_fd);
  if (target_created) {
    struct stat status;
    if (fstatat(parent_fd, target_name, &status, AT_SYMLINK_NOFOLLOW) == 0 &&
        S_ISREG(status.st_mode) && status.st_uid == geteuid()) {
      (void)unlinkat(parent_fd, target_name, 0);
    }
  }
  if (stage_fd >= 0) {
    struct stat status;
    if (fstatat(stage_fd, second_leaf, &status, AT_SYMLINK_NOFOLLOW) == 0 &&
        S_ISREG(status.st_mode)) {
      (void)unlinkat(stage_fd, second_leaf, 0);
    }
    if (fstatat(stage_fd, probe_leaf, &status, AT_SYMLINK_NOFOLLOW) == 0 &&
        S_ISREG(status.st_mode)) {
      (void)unlinkat(stage_fd, probe_leaf, 0);
    }
    (void)close(stage_fd);
  }
  if (stage_created &&
      owned_directory_entry(parent_fd, stage_name, stage_device,
                            stage_inode)) {
    (void)unlinkat(parent_fd, stage_name, AT_REMOVEDIR);
  }
  (void)close(parent_fd);
  if (error != NULL) {
    if (error_out != NULL) *error_out = error;
    return false;
  }
  return true;
}

/* ---- publisher ---------------------------------------------------------- */

/* The directory sync of the parent; the only fallible step after the rename
 * apart from the identity check. A test twin can force either position to
 * fail. */
static bool publisher_sync_parent(const t64_artifact_publisher *publisher,
                                  unsigned position) {
#ifdef T64_REAL_RUNNER_TESTING
  if ((publisher->sync_faults & position) != 0u) {
    errno = EIO;
    return false;
  }
#else
  (void)position;
#endif
  return fsync(publisher->parent_fd) == 0;
}

static void publisher_close_fds(t64_artifact_publisher *publisher) {
  if (publisher->file_fd >= 0) (void)close(publisher->file_fd);
  if (publisher->stage_fd >= 0) (void)close(publisher->stage_fd);
  if (publisher->parent_fd >= 0) (void)close(publisher->parent_fd);
  publisher->file_fd = -1;
  publisher->stage_fd = -1;
  publisher->parent_fd = -1;
}

t64_artifact_publisher *t64_artifact_publisher_open(const char *directory,
                                                    const char *leaf,
                                                    uint64_t byte_cap,
                                                    const char **error_out) {
  t64_artifact_publisher *publisher;
  struct stat existing;
  size_t leaf_length;
  if (error_out != NULL) *error_out = NULL;
  if (!leaf_is_valid(leaf)) {
    if (error_out != NULL) *error_out = "artifact-leaf-name";
    return NULL;
  }
  if (byte_cap == 0u || byte_cap > T64_ARTIFACT_MAX_BYTES) {
    if (error_out != NULL) *error_out = "artifact-cap";
    return NULL;
  }
  publisher = (t64_artifact_publisher *)calloc(1u, sizeof *publisher);
  if (publisher == NULL) {
    if (error_out != NULL) *error_out = "artifact-stage";
    return NULL;
  }
  publisher->parent_fd = -1;
  publisher->stage_fd = -1;
  publisher->file_fd = -1;
  publisher->byte_cap = byte_cap;
  leaf_length = strlen(leaf);
  memcpy(publisher->leaf, leaf, leaf_length + 1u);
  publisher->parent_fd = open_artifact_directory(directory);
  if (publisher->parent_fd < 0) {
    if (error_out != NULL) *error_out = "artifact-dir-unsafe";
    free(publisher);
    return NULL;
  }
  if (fstatat(publisher->parent_fd, publisher->leaf, &existing,
              AT_SYMLINK_NOFOLLOW) == 0) {
    if (error_out != NULL) *error_out = "artifact-exists";
    publisher_close_fds(publisher);
    free(publisher);
    return NULL;
  }
  if (errno != ENOENT) {
    if (error_out != NULL) *error_out = "artifact-stage";
    publisher_close_fds(publisher);
    free(publisher);
    return NULL;
  }
  if (!create_stage_directory(publisher->parent_fd, publisher->stage_name,
                              &publisher->stage_fd, &publisher->stage_device,
                              &publisher->stage_inode)) {
    if (publisher->stage_device != 0 || publisher->stage_inode != 0) {
      (void)unlinkat(publisher->parent_fd, publisher->stage_name,
                     AT_REMOVEDIR);
    }
    if (error_out != NULL) *error_out = "artifact-stage";
    publisher_close_fds(publisher);
    free(publisher);
    return NULL;
  }
  publisher->file_fd = create_staged_file(publisher->stage_fd, publisher->leaf,
                                          &publisher->file_device,
                                          &publisher->file_inode);
  if (publisher->file_fd < 0) {
    if (owned_directory_entry(publisher->parent_fd, publisher->stage_name,
                              publisher->stage_device,
                              publisher->stage_inode)) {
      (void)unlinkat(publisher->parent_fd, publisher->stage_name,
                     AT_REMOVEDIR);
    }
    if (error_out != NULL) *error_out = "artifact-stage";
    publisher_close_fds(publisher);
    free(publisher);
    return NULL;
  }
  return publisher;
}

bool t64_artifact_publisher_write(t64_artifact_publisher *publisher,
                                  const uint8_t *bytes, size_t length) {
  if (publisher == NULL || publisher->error != NULL ||
      (bytes == NULL && length != 0u)) {
    return false;
  }
  /* written never exceeds byte_cap, so the subtraction cannot wrap. */
  if ((uint64_t)length > publisher->byte_cap - publisher->written) {
    publisher->error = "artifact-cap";
    return false;
  }
  if (!write_all_fd(publisher->file_fd, bytes, length)) {
    publisher->error = "artifact-write";
    return false;
  }
  publisher->written += (uint64_t)length;
  return true;
}

/* The held descriptor, the staged entry and the stage entry in the parent must
 * still be the objects this process created. */
static bool staged_identity_holds(const t64_artifact_publisher *publisher) {
  struct stat held;
  struct stat entry;
  if (fstat(publisher->file_fd, &held) != 0 || !S_ISREG(held.st_mode) ||
      held.st_dev != publisher->file_device ||
      held.st_ino != publisher->file_inode || held.st_nlink != 1 ||
      (uint64_t)held.st_size != publisher->written) {
    return false;
  }
  if (fstatat(publisher->stage_fd, publisher->leaf, &entry,
              AT_SYMLINK_NOFOLLOW) != 0 ||
      !S_ISREG(entry.st_mode) || entry.st_dev != publisher->file_device ||
      entry.st_ino != publisher->file_inode || entry.st_nlink != 1 ||
      entry.st_uid != geteuid() || (entry.st_mode & 0777) != 0600) {
    return false;
  }
  return private_directory_entry(publisher->parent_fd, publisher->stage_name,
                                 publisher->stage_device,
                                 publisher->stage_inode);
}

/* Withdraws a publication whose commit has already happened. The leaf is
 * removed only while it is still the held file (an entry that is not the held
 * file belongs to somebody else and is never touched), and the stage is removed
 * after its own identity check, so a failed finalize publishes nothing and
 * strands nothing. Idempotent: abort may run it again. */
static void withdraw_publication(t64_artifact_publisher *publisher) {
  struct stat entry;
  if (publisher->parent_fd >= 0 &&
      fstatat(publisher->parent_fd, publisher->leaf, &entry,
              AT_SYMLINK_NOFOLLOW) == 0 &&
      S_ISREG(entry.st_mode) && entry.st_dev == publisher->file_device &&
      entry.st_ino == publisher->file_inode) {
    (void)unlinkat(publisher->parent_fd, publisher->leaf, 0);
  }
  if (publisher->parent_fd >= 0 &&
      owned_directory_entry(publisher->parent_fd, publisher->stage_name,
                            publisher->stage_device,
                            publisher->stage_inode)) {
    (void)unlinkat(publisher->parent_fd, publisher->stage_name, AT_REMOVEDIR);
  }
}

bool t64_artifact_publisher_finalize(t64_artifact_publisher *publisher) {
  struct stat published;
  if (publisher == NULL || publisher->error != NULL || publisher->committed) {
    return false;
  }
  if (fsync(publisher->file_fd) != 0) {
    publisher->error = "artifact-write";
    return false;
  }
#ifdef T64_REAL_RUNNER_TESTING
  if (publisher->hook != NULL) {
    publisher->hook(publisher->hook_context, publisher->parent_fd,
                    publisher->stage_name, publisher->leaf);
  }
#endif
  if (!staged_identity_holds(publisher)) {
    publisher->error = "artifact-substitution";
    return false;
  }
  /* Every check that does not depend on the rename runs before it, so that the
   * failures left after the commit are only the ones the rename itself can
   * cause. The directory sync is proven here as well as in the preflight. */
  if (!publisher_sync_parent(publisher, T64_ARTIFACT_FAULT_SYNC_BEFORE_RENAME)) {
    publisher->error = "artifact-write";
    return false;
  }
  if (rename_noreplace(publisher->stage_fd, publisher->leaf,
                       publisher->parent_fd, publisher->leaf) != 0) {
    if (errno == EEXIST) {
      publisher->error = "artifact-exists";
    } else if (errno == ENOTSUP || errno == ENOSYS || errno == EINVAL) {
      publisher->error = "artifact-no-replace-unsupported";
    } else {
      publisher->error = "artifact-rename";
    }
    return false;
  }
  /* The file is now visible at its public name. Until the two checks below
   * pass, `committed` stays false and any failure withdraws the publication
   * (docs/spec/save-media.md: every outcome but success publishes nothing). */
#ifdef T64_REAL_RUNNER_TESTING
  if (publisher->postcommit_hook != NULL) {
    publisher->postcommit_hook(publisher->postcommit_context,
                               publisher->parent_fd, publisher->stage_name,
                               publisher->leaf);
  }
#endif
  if (fstatat(publisher->parent_fd, publisher->leaf, &published,
              AT_SYMLINK_NOFOLLOW) != 0 ||
      !S_ISREG(published.st_mode) || published.st_dev != publisher->file_device ||
      published.st_ino != publisher->file_inode) {
    publisher->error = "artifact-substitution";
    withdraw_publication(publisher);
    return false;
  }
  if (!publisher_sync_parent(publisher, T64_ARTIFACT_FAULT_SYNC_AFTER_RENAME)) {
    publisher->error = "artifact-write";
    withdraw_publication(publisher);
    return false;
  }
  publisher->committed = true;
  if (owned_directory_entry(publisher->parent_fd, publisher->stage_name,
                            publisher->stage_device,
                            publisher->stage_inode)) {
    (void)unlinkat(publisher->parent_fd, publisher->stage_name,
                   AT_REMOVEDIR);
  }
  return true;
}

void t64_artifact_publisher_abort(t64_artifact_publisher *publisher) {
  struct stat entry;
  if (publisher == NULL || publisher->committed) return;
#ifdef T64_REAL_RUNNER_TESTING
  if (publisher->preserve_stage) return;
#endif
  if (publisher->stage_fd >= 0 && publisher->file_fd >= 0 &&
      fstatat(publisher->stage_fd, publisher->leaf, &entry,
              AT_SYMLINK_NOFOLLOW) == 0 &&
      S_ISREG(entry.st_mode) && entry.st_dev == publisher->file_device &&
      entry.st_ino == publisher->file_inode) {
    (void)unlinkat(publisher->stage_fd, publisher->leaf, 0);
  }
  if (publisher->parent_fd >= 0 &&
      owned_directory_entry(publisher->parent_fd, publisher->stage_name,
                            publisher->stage_device,
                            publisher->stage_inode)) {
    (void)unlinkat(publisher->parent_fd, publisher->stage_name,
                   AT_REMOVEDIR);
  }
}

void t64_artifact_publisher_destroy(t64_artifact_publisher *publisher) {
  if (publisher == NULL) return;
  publisher_close_fds(publisher);
  free(publisher);
}

const char *t64_artifact_publisher_error(
    const t64_artifact_publisher *publisher) {
  return publisher == NULL || publisher->error == NULL ? "artifact-stage"
                                                       : publisher->error;
}

uint64_t t64_artifact_publisher_written(
    const t64_artifact_publisher *publisher) {
  return publisher == NULL ? 0u : publisher->written;
}

#ifdef T64_REAL_RUNNER_TESTING
int t64_artifact_publisher_stage_fd_for_test(
    const t64_artifact_publisher *publisher) {
  return publisher == NULL ? -1 : publisher->file_fd;
}

void t64_artifact_publisher_set_precommit_hook_for_test(
    t64_artifact_publisher *publisher, t64_artifact_precommit_hook hook,
    void *context) {
  if (publisher == NULL) return;
  publisher->hook = hook;
  publisher->hook_context = context;
}

void t64_artifact_publisher_preserve_stage_for_test(
    t64_artifact_publisher *publisher) {
  if (publisher != NULL) publisher->preserve_stage = true;
}

void t64_artifact_publisher_set_postcommit_hook_for_test(
    t64_artifact_publisher *publisher, t64_artifact_precommit_hook hook,
    void *context) {
  if (publisher == NULL) return;
  publisher->postcommit_hook = hook;
  publisher->postcommit_context = context;
}

void t64_artifact_publisher_set_sync_faults_for_test(
    t64_artifact_publisher *publisher, unsigned faults) {
  if (publisher != NULL) publisher->sync_faults = faults;
}

void t64_artifact_preflight_set_sync_fault_for_test(bool fail) {
  g_preflight_sync_fault = fail;
}
#endif
