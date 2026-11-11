#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "t64_sha256.h"
#include "t64_verified_input.h"
#include "t64_json.h"
#include "tendies64/t64_machine.h"

#define FIXTURE_SIZE 4280u
#define INPUT_SIZE 54u
#define FIXTURE_SHA256 \
  "fa276cc175a4e764b6fd5413193c44b51f38fd8135e33e400032827e02f27c7c"
#define INPUT_SHA256 \
  "6d5593bc2d95465b44ec7ce7785e5ff7d7d53605de9b039a021bc697d226186d"
#define INPUT_CANONICAL_SHA256 INPUT_SHA256
#define LOG_SHA256 \
  "86594505bea53c9843f9eaf07e4cf0121cbe0055670324674fd8385c88ffd18e"
#define PROFILE_SHA256 \
  "d8c12cb2a00410a2153ca62da318166f3ca029dea8e3d6b9c841262cb8203c71"

static unsigned checks;
static unsigned failures;

static void check(bool pass, const char *description) {
  checks++;
  printf("%s %u - %s\n", pass ? "ok" : "not ok", checks, description);
  if (!pass) failures++;
}

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  void *data = NULL;
  size_t host_alignment = alignment < sizeof(void *) ? sizeof(void *)
                                                     : alignment;
  (void)context;
  if (storage_out == NULL || size == 0u || alignment == 0u ||
      posix_memalign(&data, host_alignment, size) != 0) {
    return false;
  }
  storage_out->data = data;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  return true;
}

static void test_release(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static bool core_content_digest_matches(const uint8_t *rom, size_t length,
                                        const char *expected_sha256) {
  static const t64_host_allocator allocator = {
      test_allocate, test_release, NULL};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t digest[T64_DIGEST_SIZE];
  char digest_hex[T64_SHA256_HEX_SIZE];
  bool matches = false;
  if (rom == NULL || expected_sha256 == NULL ||
      t64_profile_lookup("ntsc-cart-4m-openipl3-warm-v1", &profile) !=
          T64_STATUS_OK ||
      profile == NULL ||
      t64_machine_create(&allocator, profile, &machine) != T64_STATUS_OK ||
      machine == NULL ||
      t64_machine_admit_rom(machine, rom, length) != T64_STATUS_OK ||
      t64_machine_content_digest(machine, digest) != T64_STATUS_OK) {
    goto done;
  }
  t64_sha256_hex(digest, digest_hex);
  matches = strcmp(digest_hex, expected_sha256) == 0;
done:
  if (machine != NULL) t64_machine_destroy(machine);
  return matches;
}

static void hash_file(const char *path, uint64_t size,
                      char output[T64_VERIFIED_INPUT_SHA256_HEX_SIZE]) {
  uint8_t buffer[65536];
  t64_sha256_ctx hash;
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  int fd = open(path, O_RDONLY);
  size_t total = 0u;
  if (fd < 0 || size > sizeof buffer) {
    output[0] = '\0';
    if (fd >= 0) close(fd);
    return;
  }
  while (total < (size_t)size) {
    ssize_t count = read(fd, buffer + total, (size_t)size - total);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) {
      output[0] = '\0';
      close(fd);
      return;
    }
    total += (size_t)count;
  }
  close(fd);
  t64_sha256_init(&hash);
  t64_sha256_update(&hash, buffer, total);
  t64_sha256_final(&hash, digest);
  t64_sha256_hex(digest, output);
}

static bool clear_cloexec(int fd) {
  int flags = fcntl(fd, F_GETFD);
  return flags >= 0 && fcntl(fd, F_SETFD, flags & ~FD_CLOEXEC) == 0;
}

static bool spawn_runner(char *runner, char *report_path, char *capture_path,
                         t64_verified_input *fixture,
                         t64_verified_input *input, uint64_t guest_ticks,
                         int *status_out) {
  char fixture_option[] = "--fixture-fd";
  char input_option[] = "--input-fd";
  char profile_option[] = "--profile";
  char profile_id[] = "ntsc-cart-4m-openipl3-warm-v1";
  char ticks_option[] = "--guest-ticks";
  char ticks[24];
  char fixture_fd[24];
  char input_fd[24];
  pid_t pid;
  int report_fd;
  int capture_fd;
  char *const args[] = {runner,
                        fixture_option,
                        fixture_fd,
                        input_option,
                        input_fd,
                        profile_option,
                        profile_id,
                        ticks_option,
                        ticks,
                        NULL};

  (void)snprintf(fixture_fd, sizeof fixture_fd, "%d", fixture->fd);
  (void)snprintf(input_fd, sizeof input_fd, "%d", input->fd);
  (void)snprintf(ticks, sizeof ticks, "%llu",
                 (unsigned long long)guest_ticks);
  if (lseek(fixture->fd, 0, SEEK_SET) != 0 ||
      lseek(input->fd, 0, SEEK_SET) != 0) {
    return false;
  }
  report_fd = open(report_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  capture_fd = open(capture_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (report_fd < 0 || capture_fd < 0) {
    if (report_fd >= 0) close(report_fd);
    if (capture_fd >= 0) close(capture_fd);
    return false;
  }
  pid = fork();
  if (pid == 0) {
    if (!clear_cloexec(fixture->fd) || !clear_cloexec(input->fd) ||
        dup2(report_fd, STDOUT_FILENO) < 0 ||
        dup2(capture_fd, STDERR_FILENO) < 0) {
      _exit(126);
    }
    close(report_fd);
    close(capture_fd);
    execv(runner, args);
    _exit(127);
  }
  close(report_fd);
  close(capture_fd);
  if (pid < 0) return false;
  while (waitpid(pid, status_out, 0) < 0) {
    if (errno == EINTR) continue;
    return false;
  }
  return true;
}

static bool spawn_supervisor_real(char *supervisor, char *result_path,
                                  char *meta_path, char *work_dir,
                                  char *capture_path, char *fixture_path,
                                  char *input_path, char *wall_limit,
                                  char *test_fault,
                                  int *status_out) {
  char real_option[] = "--real";
  char result_option[] = "--result";
  char meta_option[] = "--meta";
  char work_option[] = "--work-dir";
  char wall_option[] = "--wall-ms";
  char default_wall_ms[] = "30000";
  char *wall_ms = wall_limit != NULL ? wall_limit : default_wall_ms;
  char fixture_option[] = "--fixture";
  char input_option[] = "--input";
  char capture_option[] = "--capture-out";
  char test_fault_option[] = "--test-fault";
  char *const args[] = {supervisor, real_option, result_option, result_path,
                        meta_option, meta_path, work_option, work_dir,
                        wall_option, wall_ms, fixture_option, fixture_path,
                        input_option, input_path, capture_option, capture_path,
                        NULL};
  char *const fault_args[] = {supervisor, real_option, result_option,
                              result_path, meta_option, meta_path,
                              work_option, work_dir, wall_option, wall_ms,
                              fixture_option, fixture_path, input_option,
                              input_path, capture_option, capture_path,
                              test_fault_option, (char *)test_fault, NULL};
  (void)unlink(result_path);
  (void)unlink(meta_path);
  (void)unlink(capture_path);
  pid_t pid = fork();
  if (pid == 0) {
    execv(supervisor, test_fault != NULL ? fault_args : args);
    _exit(127);
  }
  if (pid < 0) return false;
  while (waitpid(pid, status_out, 0) < 0) {
    if (errno == EINTR) continue;
    return false;
  }
  return true;
}

static bool read_file(const char *path, uint8_t *bytes, size_t capacity,
                      size_t *length_out) {
  int fd = open(path, O_RDONLY);
  size_t total = 0u;
  if (fd < 0) return false;
  while (total < capacity) {
    ssize_t count = read(fd, bytes + total, capacity - total);
    if (count < 0 && errno == EINTR) continue;
    if (count < 0) {
      close(fd);
      return false;
    }
    if (count == 0) break;
    total += (size_t)count;
  }
  if (total == capacity) {
    uint8_t extra;
    ssize_t count;
    do {
      count = read(fd, &extra, 1u);
    } while (count < 0 && errno == EINTR);
    if (count != 0) {
      close(fd);
      return false;
    }
  }
  close(fd);
  *length_out = total;
  return true;
}

static bool checker_accepts(char *checker, char *report_path) {
  pid_t pid = fork();
  int status;
  if (pid == 0) {
    char mode[] = "result";
    char *const args[] = {checker, mode, report_path, NULL};
    int null_fd = open("/dev/null", O_WRONLY);
    if (null_fd >= 0) {
      (void)dup2(null_fd, STDOUT_FILENO);
      (void)dup2(null_fd, STDERR_FILENO);
    }
    execv(checker, args);
    _exit(127);
  }
  if (pid < 0) return false;
  while (waitpid(pid, &status, 0) < 0) {
    if (errno == EINTR) continue;
    return false;
  }
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool contains(const char *text, const char *needle) {
  return strstr(text, needle) != NULL;
}

static bool report_string_equals(const uint8_t *bytes, size_t length,
                                 const char *parent_key, const char *key,
                                 const char *expected) {
  t64_json_doc document = {0};
  const t64_json_node *container;
  const t64_json_node *value;
  size_t expected_length = strlen(expected);
  bool equal;
  if (t64_json_parse(bytes, length, NULL, &document, NULL) != 0) {
    return false;
  }
  container = parent_key == NULL ? document.root
                                 : t64_json_get(document.root, parent_key);
  value = container == NULL ? NULL : t64_json_get(container, key);
  equal = value != NULL && value->type == T64_JSON_STRING &&
          value->len == expected_length &&
          memcmp(value->str, expected, expected_length) == 0;
  t64_json_free(&document);
  return equal;
}

static bool report_field_is_null(const uint8_t *bytes, size_t length,
                                 const char *key) {
  t64_json_doc document = {0};
  const t64_json_node *value;
  bool is_null;
  if (t64_json_parse(bytes, length, NULL, &document, NULL) != 0) {
    return false;
  }
  value = t64_json_get(document.root, key);
  is_null = value != NULL && value->type == T64_JSON_NULL;
  t64_json_free(&document);
  return is_null;
}

static bool supervisor_workset_removed(const char *work_dir) {
  DIR *directory = opendir(work_dir);
  struct dirent *entry;
  bool removed = directory != NULL;

  if (directory == NULL) return false;
  while ((entry = readdir(directory)) != NULL) {
    if (strncmp(entry->d_name, "t64real.", sizeof "t64real." - 1u) == 0) {
      removed = false;
      break;
    }
  }
  if (closedir(directory) != 0) return false;
  return removed;
}

static bool supervisor_error_case(char *supervisor, char *checker,
                                  char *fixture_path, char *input_path,
                                  char *result_path, char *meta_path,
                                  char *work_dir, char *capture_path,
                                  const char *fault, const char *wall_limit,
                                  const char *detail, const char *stop_kind,
                                  bool trusted_progress,
                                  size_t expected_capture_length) {
  int status = 0;
  uint8_t report[4097] = {0};
  uint8_t capture[128];
  size_t report_length = 0u;
  size_t capture_length = 0u;
  char fault_arg[40];
  char wall_arg[24];
  char *fault_ptr = NULL;
  char *wall_ptr = NULL;
  bool started;
  bool files_read;
  if (fault != NULL) {
    size_t length = strlen(fault);
    if (length >= sizeof fault_arg) return false;
    memcpy(fault_arg, fault, length + 1u);
    fault_ptr = fault_arg;
  }
  if (wall_limit != NULL) {
    size_t length = strlen(wall_limit);
    if (length >= sizeof wall_arg) return false;
    memcpy(wall_arg, wall_limit, length + 1u);
    wall_ptr = wall_arg;
  }
  started = spawn_supervisor_real(
      supervisor, result_path, meta_path, work_dir, capture_path, fixture_path,
      input_path, wall_ptr, fault_ptr, &status);
  files_read = started &&
      read_file(result_path, report, sizeof report - 1u, &report_length) &&
      read_file(capture_path, capture, sizeof capture, &capture_length) &&
      supervisor_workset_removed(work_dir);
  if (!files_read) return false;
  report[report_length] = '\0';
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 1 ||
      !checker_accepts(checker, result_path) ||
      !report_string_equals(report, report_length, NULL, "outcome",
                            "harness-error") ||
      !report_string_equals(report, report_length, NULL, "detail", detail) ||
      !report_string_equals(report, report_length, "guest_stop", "kind",
                            stop_kind) ||
      report_field_is_null(report, report_length, "build_id") ||
      report_field_is_null(report, report_length, "input_digest") ||
      report_field_is_null(report, report_length, "log_sha256") ||
      capture_length != expected_capture_length) {
    return false;
  }
  if (expected_capture_length == sizeof "Tendies64 hello via open IPL3\n" - 1u &&
      memcmp(capture, "Tendies64 hello via open IPL3\n",
             expected_capture_length) != 0) {
    return false;
  }
  if (trusted_progress) {
    return !report_field_is_null(report, report_length, "state_digest") &&
           report_string_equals(report, report_length, NULL, "guest_tick",
                                "530438") &&
           report_string_equals(report, report_length, NULL,
                                "instruction_count", "530437") &&
           contains((const char *)report,
                    "\"kind\":\"hello-completion\"");
  }
  return report_field_is_null(report, report_length, "state_digest") &&
         report_string_equals(report, report_length, NULL, "guest_tick", "0") &&
         report_string_equals(report, report_length, NULL,
                              "instruction_count", "0") &&
         !contains((const char *)report, "hello-completion");
}

static bool write_bytes(const char *path, const uint8_t *bytes, size_t length) {
  int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
  size_t offset = 0u;
  if (fd < 0) return false;
  while (offset < length) {
    ssize_t count = write(fd, bytes + offset, length - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) {
      close(fd);
      unlink(path);
      return false;
    }
    offset += (size_t)count;
  }
  {
    int sync_status = fsync(fd);
    int close_status = close(fd);
    if (sync_status != 0 || close_status != 0) {
      unlink(path);
      return false;
    }
  }
  return true;
}

static bool atomic_replace_bytes(const char *path, const uint8_t *bytes,
                                 size_t length) {
  char replacement_path[512];
  int written = snprintf(replacement_path, sizeof replacement_path,
                         "%s.replacement", path);
  if (written <= 0 || (size_t)written >= sizeof replacement_path ||
      !write_bytes(replacement_path, bytes, length)) {
    return false;
  }
  if (rename(replacement_path, path) != 0) {
    unlink(replacement_path);
    return false;
  }
  return true;
}

static bool spawn_supervisor_barrier(char *supervisor, char *result_path,
                                     char *meta_path, char *work_dir,
                                     char *capture_path, char *fixture_path,
                                     char *input_path, pid_t *pid_out,
                                     int *ready_read_out,
                                     int *gate_write_out) {
  int ready_pipe[2] = {-1, -1};
  int gate_pipe[2] = {-1, -1};
  char ready_fd[24];
  char gate_fd[24];
  char real_option[] = "--real";
  char result_option[] = "--result";
  char meta_option[] = "--meta";
  char work_option[] = "--work-dir";
  char wall_option[] = "--wall-ms";
  char wall_ms[] = "30000";
  char fixture_option[] = "--fixture";
  char input_option[] = "--input";
  char capture_option[] = "--capture-out";
  char ready_option[] = "--test-ready-fd";
  char gate_option[] = "--test-gate-fd";
  char *const args[] = {
      supervisor, real_option, result_option, result_path, meta_option,
      meta_path, work_option, work_dir, wall_option, wall_ms,
      fixture_option, fixture_path, input_option, input_path,
      capture_option, capture_path, ready_option, ready_fd,
      gate_option, gate_fd, NULL};
  pid_t pid;

  if (pipe(ready_pipe) != 0 || pipe(gate_pipe) != 0) goto fail;
  (void)snprintf(ready_fd, sizeof ready_fd, "%d", ready_pipe[1]);
  (void)snprintf(gate_fd, sizeof gate_fd, "%d", gate_pipe[0]);
  pid = fork();
  if (pid == 0) {
    close(ready_pipe[0]);
    close(gate_pipe[1]);
    if (!clear_cloexec(ready_pipe[1]) || !clear_cloexec(gate_pipe[0])) {
      _exit(126);
    }
    execv(supervisor, args);
    _exit(127);
  }
  if (pid < 0) goto fail;
  close(ready_pipe[1]);
  close(gate_pipe[0]);
  *pid_out = pid;
  *ready_read_out = ready_pipe[0];
  *gate_write_out = gate_pipe[1];
  return true;

fail:
  if (ready_pipe[0] >= 0) close(ready_pipe[0]);
  if (ready_pipe[1] >= 0) close(ready_pipe[1]);
  if (gate_pipe[0] >= 0) close(gate_pipe[0]);
  if (gate_pipe[1] >= 0) close(gate_pipe[1]);
  return false;
}

static int run_source_replacement(char *checker, char *supervisor) {
  char *fixture_source = getenv("T64_TEST_HELLO_PATH");
  char *input_source = getenv("T64_TEST_INPUT_PATH");
  char directory[] = "/tmp/t64-real-replacement.XXXXXX";
  char fixture_path[256] = {0};
  char input_path[256] = {0};
  char report_path[256] = {0};
  char meta_path[256] = {0};
  char capture_path[256] = {0};
  char source_fixture_hash[T64_VERIFIED_INPUT_SHA256_HEX_SIZE];
  char source_input_hash[T64_VERIFIED_INPUT_SHA256_HEX_SIZE];
  char replacement_fixture_hash[T64_VERIFIED_INPUT_SHA256_HEX_SIZE];
  char replacement_input_hash[T64_VERIFIED_INPUT_SHA256_HEX_SIZE];
  uint8_t original_fixture[FIXTURE_SIZE];
  uint8_t changed_fixture[FIXTURE_SIZE];
  uint8_t original_input[INPUT_SIZE];
  uint8_t changed_input[INPUT_SIZE];
  uint8_t ready_token = 0u;
  uint8_t release_token = 1u;
  uint8_t report[4097] = {0};
  uint8_t capture[128];
  static const uint8_t expected_log[] = "Tendies64 hello via open IPL3\n";
  size_t fixture_length = 0u;
  size_t input_length = 0u;
  size_t report_length = 0u;
  size_t capture_length = 0u;
  int ready_read = -1;
  int gate_write = -1;
  int child_status = 0;
  pid_t child = -1;
  bool setup_ok = fixture_source != NULL && input_source != NULL &&
                  mkdtemp(directory) != NULL;
  bool ready = false;
  bool replaced = false;
  bool release_ok = true;
  bool child_done = false;

  (void)printf("TAP version 13\n");
  if (setup_ok) {
    (void)snprintf(fixture_path, sizeof fixture_path, "%s/hello.z64", directory);
    (void)snprintf(input_path, sizeof input_path, "%s/hello.input.json", directory);
    (void)snprintf(report_path, sizeof report_path, "%s/result.json", directory);
    (void)snprintf(meta_path, sizeof meta_path, "%s/run.meta", directory);
    (void)snprintf(capture_path, sizeof capture_path, "%s/guest.log", directory);
    setup_ok = read_file(fixture_source, original_fixture,
                         sizeof original_fixture, &fixture_length) &&
               read_file(input_source, original_input, sizeof original_input,
                         &input_length) &&
               fixture_length == FIXTURE_SIZE && input_length == INPUT_SIZE &&
               write_bytes(fixture_path, original_fixture, fixture_length) &&
               write_bytes(input_path, original_input, input_length);
  }
  if (setup_ok) {
    hash_file(fixture_path, fixture_length, source_fixture_hash);
    hash_file(input_path, input_length, source_input_hash);
    memcpy(changed_fixture, original_fixture, sizeof changed_fixture);
    memcpy(changed_input, original_input, sizeof changed_input);
    changed_fixture[sizeof changed_fixture / 2u] ^= 1u;
    {
      char *version = strstr((char *)changed_input, "\"schema_version\":1");
      if (version == NULL) setup_ok = false;
      else version[strlen("\"schema_version\":") ] = '2';
    }
    if (setup_ok) {
      t64_sha256_ctx hash;
      uint8_t digest[T64_SHA256_DIGEST_SIZE];
      t64_sha256_init(&hash);
      t64_sha256_update(&hash, changed_fixture, sizeof changed_fixture);
      t64_sha256_final(&hash, digest);
      t64_sha256_hex(digest, replacement_fixture_hash);
      t64_sha256_init(&hash);
      t64_sha256_update(&hash, changed_input, sizeof changed_input);
      t64_sha256_final(&hash, digest);
      t64_sha256_hex(digest, replacement_input_hash);
      setup_ok = strcmp(source_fixture_hash, FIXTURE_SHA256) == 0 &&
                 strcmp(source_input_hash, INPUT_SHA256) == 0 &&
                 core_content_digest_matches(original_fixture,
                                             sizeof original_fixture,
                                             source_fixture_hash) &&
                 strcmp(replacement_fixture_hash, source_fixture_hash) != 0 &&
                 strcmp(replacement_input_hash, source_input_hash) != 0;
    }
  }
  check(setup_ok,
        "core-normalized content digest equals the independently hashed pinned ROM before replacement");
  if (setup_ok) {
    setup_ok = spawn_supervisor_barrier(
        supervisor, report_path, meta_path, directory, capture_path,
        fixture_path, input_path, &child, &ready_read, &gate_write);
  }
  if (setup_ok) {
    struct pollfd wait_ready = {ready_read, POLLIN, 0};
    int poll_status = poll(&wait_ready, 1u, 30000);
    ssize_t count = poll_status > 0 ? read(ready_read, &ready_token, 1u) : -1;
    ready = count == 1 && ready_token == 1u;
  }
  check(ready,
        "supervisor barrier confirms both verified descriptors exist before admission");
  if (ready) {
    replaced = atomic_replace_bytes(fixture_path, changed_fixture,
                                    sizeof changed_fixture) &&
               atomic_replace_bytes(input_path, changed_input,
                                    sizeof changed_input);
    if (replaced) {
      char observed_fixture[T64_VERIFIED_INPUT_SHA256_HEX_SIZE];
      char observed_input[T64_VERIFIED_INPUT_SHA256_HEX_SIZE];
      hash_file(fixture_path, sizeof changed_fixture, observed_fixture);
      hash_file(input_path, sizeof changed_input, observed_input);
      replaced = strcmp(observed_fixture, replacement_fixture_hash) == 0 &&
                 strcmp(observed_input, replacement_input_hash) == 0;
    }
  }
  if (gate_write >= 0) {
    release_ok = write(gate_write, &release_token, 1u) == 1;
    if (close(gate_write) != 0) release_ok = false;
    gate_write = -1;
  }
  check(replaced && release_ok,
        "replaces both same-size source paths and releases the snapshot barrier");
  if (ready_read >= 0) {
    close(ready_read);
    ready_read = -1;
  }
  if (child > 0) {
    while (waitpid(child, &child_status, 0) < 0 && errno == EINTR) {
    }
    child_done = true;
    child = -1;
  }
  check(child_done && WIFEXITED(child_status) &&
            WEXITSTATUS(child_status) == 0,
        "supervisor executes and admits its original verified snapshots");
  {
    bool report_read = setup_ok &&
        read_file(report_path, report, sizeof report - 1u, &report_length);
    bool capture_read = setup_ok &&
        read_file(capture_path, capture, sizeof capture, &capture_length);
    if (report_read) report[report_length] = '\0';
    check(report_read && checker_accepts(checker, report_path) &&
              report_string_equals(report, report_length, NULL, "outcome",
                                   "pass") &&
              capture_read && capture_length == sizeof expected_log - 1u &&
              memcmp(capture, expected_log, sizeof expected_log - 1u) == 0,
          "actual core admission still produces the checked exact original guest transcript");
    check(report_read &&
              contains((const char *)report,
                       "\"fixture_sha256\":\"" FIXTURE_SHA256 "\"") &&
              contains((const char *)report,
                       "\"input_digest\":\"" INPUT_CANONICAL_SHA256 "\"") &&
              contains((const char *)report,
                       "\"kind\":\"hello-completion\""),
          "final v2 identities and completion milestone bind the original descriptor bytes");
  }
  if (ready_read >= 0) close(ready_read);
  if (gate_write >= 0) close(gate_write);
  if (child > 0) {
    (void)kill(child, SIGKILL);
    while (waitpid(child, &child_status, 0) < 0 && errno == EINTR) {
    }
  }
  (void)unlink(fixture_path);
  (void)unlink(input_path);
  (void)unlink(report_path);
  (void)unlink(meta_path);
  (void)unlink(capture_path);
  (void)rmdir(directory);
  printf("1..%u\n", checks);
  return failures == 0u ? 0 : 1;
}

int main(int argc, char **argv) {
  static const uint8_t expected_log[] = "Tendies64 hello via open IPL3\n";
  char *fixture_path = getenv("T64_TEST_HELLO_PATH");
  char *input_path = getenv("T64_TEST_INPUT_PATH");
  char fixture_hash[T64_VERIFIED_INPUT_SHA256_HEX_SIZE];
  char input_hash[T64_VERIFIED_INPUT_SHA256_HEX_SIZE];
  char directory[] = "/tmp/t64-real-runner.XXXXXX";
  char report_path[256];
  char capture_path[256];
  char bounded_report_path[256];
  char bounded_capture_path[256];
  char invalid_input_path[256];
  char invalid_report_path[256];
  char invalid_capture_path[256];
  char supervisor_report_path[256];
  char supervisor_meta_path[256];
  char supervisor_capture_path[256];
  char fault_report_path[256];
  char fault_meta_path[256];
  char fault_capture_path[256];
  char report[4097] = {0};
  uint8_t capture[128];
  char invalid_schedule[] =
      "{\"events\":[],\"schema\":\"t64.input\",\"schema_version\":2}";
  size_t report_length = 0u;
  size_t capture_length = 0u;
  t64_verified_input fixture = {-1, 0u, {0u}, {0}, 0u};
  t64_verified_input input = {-1, 0u, {0u}, {0}, 0u};
  t64_verified_input invalid_input = {-1, 0u, {0u}, {0}, 0u};
  char invalid_input_hash[T64_VERIFIED_INPUT_SHA256_HEX_SIZE];
  int runner_status = 0;
  bool setup_ok = argc == 5 && fixture_path != NULL && input_path != NULL;
  bool spawned = false;

  if (argc == 4 && strcmp(argv[1], "--source-replacement") == 0) {
    return run_source_replacement(argv[2], argv[3]);
  }
  (void)printf("TAP version 13\n");
  if (setup_ok) {
    hash_file(fixture_path, FIXTURE_SIZE, fixture_hash);
    hash_file(input_path, INPUT_SIZE, input_hash);
    setup_ok = strcmp(fixture_hash, FIXTURE_SHA256) == 0 &&
               strcmp(input_hash, INPUT_SHA256) == 0 &&
               mkdtemp(directory) != NULL;
  }
  check(setup_ok, "uses the pinned generated fixture and canonical schedule");
  if (setup_ok) {
    (void)snprintf(report_path, sizeof report_path, "%s/result.json", directory);
    (void)snprintf(capture_path, sizeof capture_path, "%s/guest.log", directory);
    (void)snprintf(bounded_report_path, sizeof bounded_report_path,
                   "%s/bounded.json", directory);
    (void)snprintf(bounded_capture_path, sizeof bounded_capture_path,
                   "%s/bounded.log", directory);
    (void)snprintf(invalid_input_path, sizeof invalid_input_path,
                   "%s/invalid.input.json", directory);
    (void)snprintf(invalid_report_path, sizeof invalid_report_path,
                   "%s/invalid.json", directory);
    (void)snprintf(invalid_capture_path, sizeof invalid_capture_path,
                   "%s/invalid.log", directory);
    (void)snprintf(supervisor_report_path, sizeof supervisor_report_path,
                   "%s/supervisor.json", directory);
    (void)snprintf(supervisor_meta_path, sizeof supervisor_meta_path,
                   "%s/supervisor.meta", directory);
    (void)snprintf(supervisor_capture_path, sizeof supervisor_capture_path,
                   "%s/supervisor.log", directory);
    (void)snprintf(fault_report_path, sizeof fault_report_path,
                   "%s/supervisor-fault.json", directory);
    (void)snprintf(fault_meta_path, sizeof fault_meta_path,
                   "%s/supervisor-fault.meta", directory);
    (void)snprintf(fault_capture_path, sizeof fault_capture_path,
                   "%s/supervisor-fault.log", directory);
    setup_ok = t64_verified_input_prepare(
                   fixture_path, FIXTURE_SIZE, FIXTURE_SHA256,
                   T64_VERIFIED_FIXTURE_MAX_BYTES, &fixture) ==
                   T64_VERIFIED_INPUT_OK &&
               t64_verified_input_prepare(
                   input_path, INPUT_SIZE, INPUT_SHA256,
                   T64_VERIFIED_SCHEDULE_MAX_BYTES, &input) ==
                   T64_VERIFIED_INPUT_OK;
  }
  check(setup_ok, "prepares verified unlinked read-only runner descriptors");
  if (setup_ok) {
    spawned = spawn_runner(argv[1], report_path, capture_path, &fixture, &input,
                           UINT64_C(530500), &runner_status);
  }
  check(spawned && WIFEXITED(runner_status) && WEXITSTATUS(runner_status) == 0,
        "actual producer exits successfully after guest completion");
  if (spawned) {
    (void)read_file(capture_path, capture, sizeof capture, &capture_length);
    (void)read_file(report_path, (uint8_t *)report, sizeof report - 1u,
                    &report_length);
    report[report_length] = '\0';
  }
  check(capture_length == sizeof expected_log - 1u &&
            memcmp(capture, expected_log, sizeof expected_log - 1u) == 0,
        "captures the exact authored guest transcript");
  check(spawned && checker_accepts(argv[2], report_path),
        "strict real-result checker accepts the producer report");
  check(spawned && report_string_equals((const uint8_t *)report,
                                        report_length, NULL, "outcome", "pass"),
        "parsed real-result outcome is pass");
  check(contains(report, "\"fixture_sha256\":\"" FIXTURE_SHA256 "\"") &&
            contains(report, "\"input_digest\":\"" INPUT_CANONICAL_SHA256 "\"") &&
            contains(report, "\"log_sha256\":\"" LOG_SHA256 "\"") &&
            contains(report, "\"profile_digest\":\"" PROFILE_SHA256 "\"") &&
            contains(report, "\"guest_tick\":\"530438\"") &&
            contains(report, "\"instruction_count\":\"530437\"") &&
            contains(report, "\"kind\":\"hello-completion\"") &&
            contains(report, "\"kind\":\"loader-dma\"") &&
            contains(report, "\"kind\":\"ipl3-entry\"") &&
            contains(report, "\"kind\":\"hello-entry\""),
        "report binds descriptor identities, real milestones and stop progress");

  {
    bool bounded_started = spawn_runner(
        argv[1], bounded_report_path, bounded_capture_path, &fixture, &input,
        UINT64_C(100), &runner_status);
    uint8_t bounded_report[4097] = {0};
    uint8_t bounded_capture[128];
    size_t bounded_report_length = 0u;
    size_t bounded_capture_length = 0u;
    bool bounded_files_read = bounded_started &&
        read_file(bounded_report_path, bounded_report,
                  sizeof bounded_report - 1u, &bounded_report_length) &&
        read_file(bounded_capture_path, bounded_capture,
                  sizeof bounded_capture, &bounded_capture_length);
    check(bounded_started && WIFEXITED(runner_status) &&
              WEXITSTATUS(runner_status) == 0,
          "bounded producer exits without claiming guest completion");
    check(bounded_files_read && checker_accepts(argv[2], bounded_report_path) &&
              report_string_equals(bounded_report, bounded_report_length,
                                   NULL, "outcome", "boot-fail") &&
              report_string_equals(bounded_report, bounded_report_length,
                                   NULL, "detail", "empty-output") &&
              report_string_equals(bounded_report, bounded_report_length,
                                   "guest_stop", "kind", "guest-bound") &&
              report_string_equals(bounded_report, bounded_report_length,
                                   NULL, "guest_tick", "100") &&
              bounded_capture_length == 0u,
          "bounded execution validates as timeout with no guest output or pass");
  }

  if (setup_ok) {
    int invalid_fd = open(invalid_input_path,
                          O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (invalid_fd >= 0) {
      size_t length = sizeof invalid_schedule - 1u;
      size_t offset = 0u;
      while (offset < length) {
        ssize_t count = write(invalid_fd, invalid_schedule + offset,
                              length - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        offset += (size_t)count;
      }
      if (close(invalid_fd) != 0 || offset != length) setup_ok = false;
    } else {
      setup_ok = false;
    }
  }
  if (setup_ok) {
    hash_file(invalid_input_path, sizeof invalid_schedule - 1u,
              invalid_input_hash);
    setup_ok = invalid_input_hash[0] != '\0' &&
        t64_verified_input_prepare(
            invalid_input_path, sizeof invalid_schedule - 1u,
            invalid_input_hash, T64_VERIFIED_SCHEDULE_MAX_BYTES,
            &invalid_input) == T64_VERIFIED_INPUT_OK;
  }
  check(setup_ok, "prepares a separately hashed malformed input descriptor");
  if (setup_ok) {
    bool invalid_started = spawn_runner(
        argv[1], invalid_report_path, invalid_capture_path, &fixture,
        &invalid_input, UINT64_C(530500), &runner_status);
    uint8_t invalid_report[4097] = {0};
    uint8_t invalid_capture[128];
    size_t invalid_report_length = 0u;
    size_t invalid_capture_length = 0u;
    bool invalid_files_read = invalid_started &&
        read_file(invalid_report_path, invalid_report,
                  sizeof invalid_report - 1u, &invalid_report_length) &&
        read_file(invalid_capture_path, invalid_capture,
                  sizeof invalid_capture, &invalid_capture_length);
    check(invalid_files_read && WIFEXITED(runner_status) &&
              WEXITSTATUS(runner_status) == 0 &&
              checker_accepts(argv[2], invalid_report_path) &&
              report_string_equals(invalid_report, invalid_report_length,
                                   NULL, "outcome", "harness-error") &&
              report_string_equals(invalid_report, invalid_report_length,
                                   "guest_stop", "kind", "refused") &&
              report_field_is_null(invalid_report, invalid_report_length,
                                   "input_digest") &&
              invalid_capture_length == 0u,
          "malformed canonical schedule refuses before launch and cannot pass");
  }

  {
    int supervisor_status = 0;
    bool supervisor_started = setup_ok && spawn_supervisor_real(
        argv[3], supervisor_report_path, supervisor_meta_path, directory,
        supervisor_capture_path, fixture_path, input_path, NULL, NULL,
        &supervisor_status);
    uint8_t supervisor_report[4097] = {0};
    uint8_t supervisor_capture[128];
    size_t supervisor_report_length = 0u;
    size_t supervisor_capture_length = 0u;
    bool supervisor_files_read = supervisor_started &&
        read_file(supervisor_report_path, supervisor_report,
                  sizeof supervisor_report - 1u, &supervisor_report_length) &&
        read_file(supervisor_capture_path, supervisor_capture,
                  sizeof supervisor_capture, &supervisor_capture_length);
    check(supervisor_files_read && WIFEXITED(supervisor_status) &&
              WEXITSTATUS(supervisor_status) == 0 &&
              checker_accepts(argv[2], supervisor_report_path) &&
              report_string_equals(supervisor_report,
                                   supervisor_report_length, NULL, "outcome",
                                   "pass") &&
              supervisor_capture_length == sizeof expected_log - 1u &&
              memcmp(supervisor_capture, expected_log,
                     sizeof expected_log - 1u) == 0,
          "real supervisor emits a checked v2 pass and exact captured guest bytes");
  }

  {
    static const struct {
      const char *fault;
      const char *wall_limit;
      const char *detail;
      const char *stop_kind;
      bool trusted_progress;
      size_t capture_length;
      const char *description;
    } cases[] = {
        {"exit-nonzero", NULL, "runner-exit-nonzero", "host-error", true,
         sizeof expected_log - 1u,
         "nonzero child exit overrides guest pass and retains checked progress"},
        {"report-missing", NULL, "no-result", "report-error", false,
         sizeof expected_log - 1u,
         "missing child report after real output becomes report-error"},
        {"report-truncated", NULL, "report-invalid", "report-error", false,
         sizeof expected_log - 1u,
         "truncated child report after real output cannot become a pass"},
        {"report-oversize", NULL, "report-invalid", "report-error", false,
         sizeof expected_log - 1u,
         "oversize child report after real output cannot become a pass"},
        {"report-malformed", NULL, "report-invalid", "report-error", false,
         sizeof expected_log - 1u,
         "malformed child report after real output cannot become a pass"},
        {"report-unknown", NULL, "report-invalid", "report-error", false,
         sizeof expected_log - 1u,
         "unknown child report field after real output cannot become a pass"},
        {"report-missing-log-sha256", NULL, "report-invalid", "report-error",
         false, sizeof expected_log - 1u,
         "missing log hash in an otherwise valid child report becomes report-invalid and removes its workset"},
        {"capture-closed", NULL, "capture-invalid", "report-error", false,
         0u, "closed capture descriptor after guest report cannot become a pass"},
        {"crash-after-output", NULL, "runner-signalled", "host-error", false,
         sizeof expected_log - 1u,
         "runner crash after correct guest text takes harness-error precedence"},
        {"delay-after-output", "2500", "wall-backstop", "host-error", false,
         sizeof expected_log - 1u,
         "wall kill after correct guest text takes harness-error precedence"},
    };
    size_t i;
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
      bool passed = setup_ok && supervisor_error_case(
          argv[4], argv[2], fixture_path, input_path, fault_report_path,
          fault_meta_path, directory, fault_capture_path, cases[i].fault,
          cases[i].wall_limit, cases[i].detail, cases[i].stop_kind,
          cases[i].trusted_progress, cases[i].capture_length);
      if (!passed) {
        uint8_t diagnostic_report[4097] = {0};
        uint8_t diagnostic_capture[128];
        size_t diagnostic_report_length = 0u;
        size_t diagnostic_capture_length = 0u;
        bool have_report = read_file(fault_report_path, diagnostic_report,
                                     sizeof diagnostic_report - 1u,
                                     &diagnostic_report_length);
        bool have_capture = read_file(fault_capture_path, diagnostic_capture,
                                      sizeof diagnostic_capture,
                                      &diagnostic_capture_length);
        fprintf(stderr,
                "supervisor fault diagnostic: fault=%s expected_detail=%s "
                "expected_stop=%s report_bytes=%zu capture_bytes=%s%zu "
                "report=%.*s\n",
                cases[i].fault, cases[i].detail, cases[i].stop_kind,
                have_report ? diagnostic_report_length : 0u,
                have_capture ? "" : "unreadable/too-large:",
                have_capture ? diagnostic_capture_length : 0u,
                have_report ? (int)diagnostic_report_length : 0,
                have_report ? (const char *)diagnostic_report : "");
      }
      check(passed, cases[i].description);
    }
  }

  {
    char missing_input_path[256] = {0};
    int missing_status = 0;
    uint8_t missing_report[4097] = {0};
    size_t missing_report_length = 0u;
    int written = snprintf(missing_input_path, sizeof missing_input_path,
                           "%s/absent.input.json", directory);
    if (written > 0 && (size_t)written < sizeof missing_input_path)
      (void)unlink(missing_input_path);
    bool missing_started = setup_ok && written > 0 &&
        (size_t)written < sizeof missing_input_path &&
        spawn_supervisor_real(argv[4], fault_report_path, fault_meta_path,
                              directory, fault_capture_path, fixture_path,
                              missing_input_path, NULL, NULL, &missing_status);
    bool missing_read = missing_started &&
        read_file(fault_report_path, missing_report,
                  sizeof missing_report - 1u, &missing_report_length);
    check(missing_read && WIFEXITED(missing_status) &&
              WEXITSTATUS(missing_status) == 1 &&
              checker_accepts(argv[2], fault_report_path) &&
              report_string_equals(missing_report, missing_report_length,
                                   NULL, "detail", "refusal") &&
              report_string_equals(missing_report, missing_report_length,
                                   "guest_stop", "kind", "refused") &&
              report_field_is_null(missing_report, missing_report_length,
                                   "build_id") &&
              report_field_is_null(missing_report, missing_report_length,
                                   "input_digest") &&
              access(fault_capture_path, F_OK) != 0 &&
              access(fault_meta_path, F_OK) != 0,
          "missing logical input refuses before child launch with null prelaunch identities");
  }

  t64_verified_input_release(&fixture);
  t64_verified_input_release(&input);
  t64_verified_input_release(&invalid_input);
  if (setup_ok && failures == 0u) {
    (void)unlink(report_path);
    (void)unlink(capture_path);
    (void)unlink(bounded_report_path);
    (void)unlink(bounded_capture_path);
    (void)unlink(invalid_input_path);
    (void)unlink(invalid_report_path);
    (void)unlink(invalid_capture_path);
    (void)unlink(supervisor_report_path);
    (void)unlink(supervisor_meta_path);
    (void)unlink(supervisor_capture_path);
    (void)unlink(fault_report_path);
    (void)unlink(fault_meta_path);
    (void)unlink(fault_capture_path);
    (void)rmdir(directory);
  } else if (setup_ok) {
    fprintf(stderr, "local failure artifacts: %s\n", directory);
  }
  printf("1..%u\n", checks);
  return failures == 0u ? 0 : 1;
}
