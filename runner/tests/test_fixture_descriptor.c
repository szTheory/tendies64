#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "t64_sha256.h"
#include "t64_verified_input.h"

#define FIXTURE_SIZE 4280u
#define FIXTURE_SHA256 \
  "fa276cc175a4e764b6fd5413193c44b51f38fd8135e33e400032827e02f27c7c"
#define TEST_COUNT 11u

static unsigned test_number;
static unsigned failures;

static void check(int condition, const char *name) {
  test_number++;
  if (condition) {
    printf("ok %u - %s\n", test_number, name);
  } else {
    printf("not ok %u - %s\n", test_number, name);
    failures++;
  }
}

static int count_open_fds(void) {
  int fd;
  int count = 0;
  for (fd = 0; fd < 1024; fd++) {
    if (fcntl(fd, F_GETFD) >= 0) count++;
  }
  return count;
}

static int count_entries(const char *directory) {
  DIR *dir = opendir(directory);
  struct dirent *entry;
  int count = 0;
  if (dir == NULL) return -1;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
      count++;
  }
  closedir(dir);
  return count;
}

static int copy_fixture(char path[4096], char directory[4096]) {
  const char *source = getenv("T64_TEST_HELLO_PATH");
  char template_path[] = "/tmp/t64-descriptor-test.XXXXXX";
  struct stat st;
  int in = -1;
  int out = -1;
  if (source == NULL || mkdtemp(template_path) == NULL) return -1;
  (void)snprintf(directory, 4096u, "%s", template_path);
  if (snprintf(path, 4096u, "%s/fixture.bin", directory) >= 4096) return -1;
  in = open(source, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
  if (in < 0 || fstat(in, &st) != 0 || !S_ISREG(st.st_mode) ||
      st.st_size != FIXTURE_SIZE) goto fail;
  out = open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
  if (out < 0) goto fail;
  for (;;) {
    uint8_t buffer[4096];
    ssize_t count = read(in, buffer, sizeof buffer);
    if (count < 0 && errno == EINTR) continue;
    if (count < 0) goto fail;
    if (count == 0) break;
    {
      size_t offset = 0u;
      while (offset < (size_t)count) {
        ssize_t written = write(out, buffer + offset, (size_t)count - offset);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) goto fail;
        offset += (size_t)written;
      }
    }
  }
  if (close(in) != 0) return -1;
  if (close(out) != 0) return -1;
  return 0;
fail:
  if (in >= 0) close(in);
  if (out >= 0) close(out);
  unlink(path);
  rmdir(directory);
  return -1;
}

static void remove_source_dir(const char *source_path, const char *directory) {
  if (source_path[0] != '\0') unlink(source_path);
  if (directory[0] != '\0') rmdir(directory);
}

static int exact_descriptor_bytes(t64_verified_input *input) {
  uint8_t bytes[FIXTURE_SIZE];
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  char hex[T64_SHA256_HEX_SIZE];
  ssize_t count = read(input->fd, bytes, sizeof bytes);
  if (count != (ssize_t)sizeof bytes) return 0;
  {
    t64_sha256_ctx hash;
    t64_sha256_init(&hash);
    t64_sha256_update(&hash, bytes, sizeof bytes);
    t64_sha256_final(&hash, digest);
  }
  t64_sha256_hex(digest, hex);
  return strcmp(hex, FIXTURE_SHA256) == 0 &&
         strcmp(hex, input->sha256_hex) == 0 &&
         lseek(input->fd, 0, SEEK_SET) == 0;
}

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "--replacement") == 0) {
    puts("1..1");
    puts("not ok 1 - preserves admitted descriptor bytes after source replacement");
    return 1;
  }
  char source_path[4096] = {0};
  char directory[4096] = {0};
  char bad_path[4096];
  char link_path[4096];
  char fifo_path[4096];
  char large_path[4096];
  t64_verified_input input = {-1, 0u, {0u}, {0}, 0u};
  t64_verified_input unchanged = {-1, 9u, {0u}, {0}, 0u};
  t64_verified_input_status status;
  int unrelated = -1;
  int before_fds;
  struct stat st;
  int copied = copy_fixture(source_path, directory);
  if (copied == 0) setenv("TMPDIR", directory, 1);
  printf("1..%u\n", TEST_COUNT);

  status = copied == 0
               ? t64_verified_input_prepare(
                     source_path, FIXTURE_SIZE, FIXTURE_SHA256,
                     T64_VERIFIED_FIXTURE_MAX_BYTES, &input)
               : T64_VERIFIED_INPUT_ERROR;
  check(status == T64_VERIFIED_INPUT_OK && input.fd >= 0,
        "prepares a verified immutable descriptor");
  check(status == T64_VERIFIED_INPUT_OK &&
            fstat(input.fd, &st) == 0 && S_ISREG(st.st_mode) &&
            (st.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH)) == 0 &&
            (fcntl(input.fd, F_GETFL) & O_ACCMODE) == O_RDONLY &&
            (fcntl(input.fd, F_GETFD) & FD_CLOEXEC) != 0,
        "descriptor is read-only, regular and close-on-exec");
  check(status == T64_VERIFIED_INPUT_OK && input.size == FIXTURE_SIZE &&
            input.sha256_hex[0] != '\0' && strcmp(input.sha256_hex, FIXTURE_SHA256) == 0 &&
            fstat(input.fd, &st) == 0 && st.st_nlink == 0 && st.st_size == FIXTURE_SIZE,
        "descriptor is unlinked and carries the exact raw identity");
  check(status == T64_VERIFIED_INPUT_OK && exact_descriptor_bytes(&input),
        "descriptor starts at byte zero and yields the pinned fixture bytes");
  check(status == T64_VERIFIED_INPUT_OK && count_entries(directory) == 1,
        "successful preparation leaves no named private snapshot");
  if (status == T64_VERIFIED_INPUT_OK) t64_verified_input_release(&input);
  errno = 0;
  check(fcntl(input.fd, F_GETFD) < 0 && errno == EBADF,
        "release closes the owned descriptor");

  before_fds = count_open_fds();
  (void)snprintf(bad_path, sizeof bad_path, "%s/missing", directory);
  status = t64_verified_input_prepare(bad_path, FIXTURE_SIZE, FIXTURE_SHA256,
                                     T64_VERIFIED_FIXTURE_MAX_BYTES, &unchanged);
  check(status == T64_VERIFIED_INPUT_MISSING && unchanged.fd == -1 &&
            unchanged.size == 9u && count_open_fds() == before_fds &&
            count_entries(directory) == 1,
        "missing source refuses without changing output or leaking resources");

  (void)snprintf(link_path, sizeof link_path, "%s/symlink", directory);
  (void)symlink(source_path, link_path);
  before_fds = count_open_fds();
  status = t64_verified_input_prepare(link_path, FIXTURE_SIZE, FIXTURE_SHA256,
                                     T64_VERIFIED_FIXTURE_MAX_BYTES, &unchanged);
  unlink(link_path);
  check(status == T64_VERIFIED_INPUT_ERROR && count_open_fds() == before_fds &&
            count_entries(directory) == 1,
        "symlink source refuses without following or leaking resources");

  (void)snprintf(fifo_path, sizeof fifo_path, "%s/fifo", directory);
  (void)mkfifo(fifo_path, 0600);
  before_fds = count_open_fds();
  status = t64_verified_input_prepare(fifo_path, FIXTURE_SIZE, FIXTURE_SHA256,
                                     T64_VERIFIED_FIXTURE_MAX_BYTES, &unchanged);
  unlink(fifo_path);
  check(status == T64_VERIFIED_INPUT_ERROR && count_open_fds() == before_fds &&
            count_entries(directory) == 1,
        "non-regular FIFO source refuses without blocking or leaking resources");

  (void)snprintf(large_path, sizeof large_path, "%s/oversize", directory);
  unrelated = open(large_path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
  if (unrelated >= 0) {
    (void)ftruncate(unrelated, T64_VERIFIED_FIXTURE_MAX_BYTES + 1u);
    close(unrelated);
  }
  before_fds = count_open_fds();
  status = t64_verified_input_prepare(large_path,
                                     T64_VERIFIED_FIXTURE_MAX_BYTES + 1u,
                                     FIXTURE_SHA256,
                                     T64_VERIFIED_FIXTURE_MAX_BYTES, &unchanged);
  unlink(large_path);
  check(status == T64_VERIFIED_INPUT_ERROR && count_open_fds() == before_fds &&
            count_entries(directory) == 1,
        "profile ceiling rejects oversize source before copying");

  unrelated = open("/dev/null", O_RDONLY);
  before_fds = count_open_fds();
  status = t64_verified_input_prepare(source_path, FIXTURE_SIZE,
      "0000000000000000000000000000000000000000000000000000000000000000",
      T64_VERIFIED_FIXTURE_MAX_BYTES, &unchanged);
  t64_verified_input_release(&unchanged);
  check(status == T64_VERIFIED_INPUT_ERROR && unrelated >= 0 &&
            fcntl(unrelated, F_GETFD) >= 0 && count_open_fds() == before_fds &&
            count_entries(directory) == 1,
        "digest mismatch and release refuse without closing caller descriptors");
  if (unrelated >= 0) close(unrelated);

  remove_source_dir(source_path, directory);
  return failures == 0u && test_number == TEST_COUNT ? 0 : 1;
}
