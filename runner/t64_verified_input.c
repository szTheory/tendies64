/* Immutable, unlinked host-side snapshots for untrusted runner inputs. */
#include "t64_verified_input.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "t64_sha256.h"

#define T64_VERIFIED_INPUT_OWNERSHIP_TAG UINT32_C(0x74363469)
#define T64_VERIFIED_INPUT_CHUNK_BYTES 65536u
#define T64_VERIFIED_INPUT_PATH_CAPACITY 4096u

static int valid_hex_sha256(const char *hex) {
  size_t i;
  if (hex == NULL || strlen(hex) != T64_SHA256_HEX_SIZE - 1u) return 0;
  for (i = 0u; i < T64_SHA256_HEX_SIZE - 1u; i++) {
    if (!((hex[i] >= '0' && hex[i] <= '9') ||
          (hex[i] >= 'a' && hex[i] <= 'f'))) {
      return 0;
    }
  }
  return 1;
}

static int set_close_on_exec(int fd) {
  int flags = fcntl(fd, F_GETFD);
  return flags < 0 ? -1 : fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

static int open_source(const char *path, struct stat *st,
                       t64_verified_input_status *status) {
  int fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0) {
    *status = errno == ENOENT || errno == ENOTDIR
                  ? T64_VERIFIED_INPUT_MISSING
                  : T64_VERIFIED_INPUT_ERROR;
    return -1;
  }
  if (set_close_on_exec(fd) != 0 || fstat(fd, st) != 0 ||
      !S_ISREG(st->st_mode) || st->st_size < 0) {
    close(fd);
    *status = T64_VERIFIED_INPUT_ERROR;
    return -1;
  }
  return fd;
}

static int make_temp_path(char path[T64_VERIFIED_INPUT_PATH_CAPACITY]) {
  const char *directory = getenv("TMPDIR");
  int n;
  if (directory == NULL || directory[0] == '\0') directory = "/tmp";
  n = snprintf(path, T64_VERIFIED_INPUT_PATH_CAPACITY, "%s%st64-input.XXXXXX",
               directory,
               directory[strlen(directory) - 1u] == '/' ? "" : "/");
  return n > 0 && (size_t)n < T64_VERIFIED_INPUT_PATH_CAPACITY ? 0 : -1;
}

static int copy_and_hash(int source_fd, int writer_fd, uint64_t expected_size,
                         uint64_t max_bytes, uint8_t digest_out[32]) {
  uint8_t buffer[T64_VERIFIED_INPUT_CHUNK_BYTES];
  t64_sha256_ctx hash;
  uint64_t total = 0u;
  t64_sha256_init(&hash);
  for (;;) {
    ssize_t count = read(source_fd, buffer, sizeof buffer);
    size_t offset = 0u;
    if (count < 0 && errno == EINTR) continue;
    if (count < 0) return -1;
    if (count == 0) break;
    if ((uint64_t)count > max_bytes - total ||
        (uint64_t)count > expected_size - total) {
      return -1;
    }
    while (offset < (size_t)count) {
      ssize_t written = write(writer_fd, buffer + offset,
                              (size_t)count - offset);
      if (written < 0 && errno == EINTR) continue;
      if (written <= 0) return -1;
      offset += (size_t)written;
    }
    t64_sha256_update(&hash, buffer, (size_t)count);
    total += (uint64_t)count;
  }
  if (total != expected_size || total > max_bytes) return -1;
  t64_sha256_final(&hash, digest_out);
  return 0;
}

static int verify_readonly_snapshot(int fd, const struct stat *writer_stat,
                                    uint64_t expected_size,
                                    const uint8_t expected_digest[32]) {
  struct stat st;
  uint8_t buffer[T64_VERIFIED_INPUT_CHUNK_BYTES];
  uint8_t digest[32];
  t64_sha256_ctx hash;
  uint64_t total = 0u;
  int file_flags;
  int fd_flags;
  if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0 ||
      (uint64_t)st.st_size != expected_size || st.st_dev != writer_stat->st_dev ||
      st.st_ino != writer_stat->st_ino || st.st_nlink != 1 ||
      (st.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH)) != 0) {
    return -1;
  }
  file_flags = fcntl(fd, F_GETFL);
  fd_flags = fcntl(fd, F_GETFD);
  if (file_flags < 0 || (file_flags & O_ACCMODE) != O_RDONLY ||
      fd_flags < 0 || (fd_flags & FD_CLOEXEC) == 0) {
    return -1;
  }
  t64_sha256_init(&hash);
  for (;;) {
    ssize_t count = read(fd, buffer, sizeof buffer);
    if (count < 0 && errno == EINTR) continue;
    if (count < 0) return -1;
    if (count == 0) break;
    if ((uint64_t)count > expected_size - total) return -1;
    t64_sha256_update(&hash, buffer, (size_t)count);
    total += (uint64_t)count;
  }
  if (total != expected_size) return -1;
  t64_sha256_final(&hash, digest);
  if (memcmp(digest, expected_digest, sizeof digest) != 0) return -1;
  return lseek(fd, 0, SEEK_SET) == 0 ? 0 : -1;
}

static int move_above_stdio(int *fd_inout) {
  int moved;
  if (*fd_inout > STDERR_FILENO) return 0;
  moved = fcntl(*fd_inout, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
  if (moved < 0) return -1;
  close(*fd_inout);
  *fd_inout = moved;
  return 0;
}

static void cleanup_prepare(int source_fd, int writer_fd, int snapshot_fd,
                            const char *temp_path) {
  if (source_fd >= 0) close(source_fd);
  if (writer_fd >= 0) close(writer_fd);
  if (snapshot_fd >= 0) close(snapshot_fd);
  if (temp_path != NULL && temp_path[0] != '\0') unlink(temp_path);
}

t64_verified_input_status t64_verified_input_prepare(
    const char *source_path, uint64_t expected_size,
    const char *expected_sha256_hex, uint64_t max_bytes,
    t64_verified_input *output) {
  char temp_path[T64_VERIFIED_INPUT_PATH_CAPACITY] = {0};
  struct stat source_stat;
  struct stat source_after;
  struct stat writer_stat;
  struct stat unlinked_stat;
  t64_verified_input prepared;
  t64_verified_input_status status = T64_VERIFIED_INPUT_ERROR;
  uint8_t copied_digest[T64_SHA256_DIGEST_SIZE];
  char copied_hex[T64_SHA256_HEX_SIZE];
  int source_fd = -1;
  int writer_fd = -1;
  int snapshot_fd = -1;

  if (source_path == NULL || output == NULL ||
      !valid_hex_sha256(expected_sha256_hex) || max_bytes == 0u ||
      max_bytes > T64_VERIFIED_HARNESS_MAX_BYTES || expected_size > max_bytes) {
    return T64_VERIFIED_INPUT_ERROR;
  }
  source_fd = open_source(source_path, &source_stat, &status);
  if (source_fd < 0) return status;
  if ((uint64_t)source_stat.st_size != expected_size ||
      (uint64_t)source_stat.st_size > max_bytes || make_temp_path(temp_path) != 0) {
    cleanup_prepare(source_fd, -1, -1, NULL);
    return T64_VERIFIED_INPUT_ERROR;
  }
  writer_fd = mkstemp(temp_path);
  if (writer_fd < 0 || set_close_on_exec(writer_fd) != 0 ||
      fstat(writer_fd, &writer_stat) != 0 || !S_ISREG(writer_stat.st_mode) ||
      copy_and_hash(source_fd, writer_fd, expected_size, max_bytes,
                    copied_digest) != 0) {
    cleanup_prepare(source_fd, writer_fd, -1, temp_path);
    return T64_VERIFIED_INPUT_ERROR;
  }
  t64_sha256_hex(copied_digest, copied_hex);
  if (strcmp(copied_hex, expected_sha256_hex) != 0 ||
      fstat(source_fd, &source_after) != 0 || !S_ISREG(source_after.st_mode) ||
      source_after.st_dev != source_stat.st_dev ||
      source_after.st_ino != source_stat.st_ino || source_after.st_size < 0 ||
      (uint64_t)source_after.st_size != expected_size || fsync(writer_fd) != 0 ||
      fchmod(writer_fd, 0400) != 0 || fsync(writer_fd) != 0 ||
      fstat(writer_fd, &writer_stat) != 0) {
    cleanup_prepare(source_fd, writer_fd, -1, temp_path);
    return T64_VERIFIED_INPUT_ERROR;
  }
  close(source_fd);
  source_fd = -1;
  {
    int closing = writer_fd;
    writer_fd = -1;
    if (close(closing) != 0) {
      cleanup_prepare(-1, -1, -1, temp_path);
      return T64_VERIFIED_INPUT_ERROR;
    }
  }
  snapshot_fd = open(temp_path,
                     O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
  if (snapshot_fd < 0 || set_close_on_exec(snapshot_fd) != 0 ||
      verify_readonly_snapshot(snapshot_fd, &writer_stat, expected_size,
                               copied_digest) != 0 || unlink(temp_path) != 0 ||
      fstat(snapshot_fd, &unlinked_stat) != 0 || unlinked_stat.st_nlink != 0 ||
      lseek(snapshot_fd, 0, SEEK_SET) != 0 || move_above_stdio(&snapshot_fd) != 0) {
    cleanup_prepare(-1, -1, snapshot_fd, temp_path);
    return T64_VERIFIED_INPUT_ERROR;
  }
  memset(&prepared, 0, sizeof prepared);
  prepared.fd = snapshot_fd;
  prepared.size = expected_size;
  memcpy(prepared.sha256, copied_digest, sizeof copied_digest);
  memcpy(prepared.sha256_hex, copied_hex, sizeof copied_hex);
  prepared.ownership_tag = T64_VERIFIED_INPUT_OWNERSHIP_TAG;
  *output = prepared;
  return T64_VERIFIED_INPUT_OK;
}

void t64_verified_input_release(t64_verified_input *input) {
  int fd;
  if (input == NULL ||
      input->ownership_tag != T64_VERIFIED_INPUT_OWNERSHIP_TAG || input->fd < 0) {
    return;
  }
  fd = input->fd;
  input->fd = -1;
  input->ownership_tag = 0u;
  input->size = 0u;
  memset(input->sha256, 0, sizeof input->sha256);
  memset(input->sha256_hex, 0, sizeof input->sha256_hex);
  close(fd);
}
