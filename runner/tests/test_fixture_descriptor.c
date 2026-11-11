#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "t64_verified_input.h"

#define FIXTURE_SIZE 4280u
#define FIXTURE_SHA256 \
  "fa276cc175a4e764b6fd5413193c44b51f38fd8135e33e400032827e02f27c7c"

static int copy_fixture(char path[4096], char directory[4096]) {
  const char *source = getenv("T64_TEST_HELLO_PATH");
  char template_path[] = "/tmp/t64-descriptor-test.XXXXXX";
  struct stat st;
  int in = -1;
  int out = -1;
  ssize_t count;
  if (source == NULL || mkdtemp(template_path) == NULL) return -1;
  (void)snprintf(directory, 4096u, "%s", template_path);
  if (snprintf(path, 4096u, "%s/fixture.bin", directory) >= 4096) return -1;
  in = open(source, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
  if (in < 0 || fstat(in, &st) != 0 || !S_ISREG(st.st_mode) ||
      st.st_size != FIXTURE_SIZE) goto fail;
  out = open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
  if (out < 0) goto fail;
  for (;;) {
    char buffer[4096];
    count = read(in, buffer, sizeof buffer);
    if (count < 0 && errno == EINTR) continue;
    if (count < 0) goto fail;
    if (count == 0) break;
    {
      ssize_t offset = 0;
      while (offset < count) {
        ssize_t written = write(out, buffer + offset, (size_t)(count - offset));
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) goto fail;
        offset += written;
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

int main(void) {
  char source_path[4096] = {0};
  char directory[4096] = {0};
  t64_verified_input input = {-1, 0u, {0u}, {0}, 0u};
  t64_verified_input_status status;
  int copied = copy_fixture(source_path, directory);
  status = copied == 0
               ? t64_verified_input_prepare(
                     source_path, FIXTURE_SIZE, FIXTURE_SHA256,
                     T64_VERIFIED_FIXTURE_MAX_BYTES, &input)
               : T64_VERIFIED_INPUT_ERROR;
  printf("1..1\n");
  if (status == T64_VERIFIED_INPUT_OK && input.fd >= 0 &&
      input.size == FIXTURE_SIZE &&
      strcmp(input.sha256_hex, FIXTURE_SHA256) == 0) {
    puts("ok 1 - prepares a verified immutable descriptor");
    t64_verified_input_release(&input);
    unlink(source_path);
    rmdir(directory);
    return 0;
  }
  (void)status;
  puts("not ok 1 - prepares a verified immutable descriptor");
  if (copied == 0) {
    unlink(source_path);
    rmdir(directory);
  }
  return 1;
}
