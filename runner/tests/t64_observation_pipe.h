/* Bounded concurrent drain for supervised fixed-size observation records. */
#ifndef T64_TEST_OBSERVATION_PIPE_H
#define T64_TEST_OBSERVATION_PIPE_H

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define T64_TEST_OBSERVATION_PIPE_MAX 2u
#define T64_TEST_OBSERVATION_PIPE_TIMEOUT_MS UINT64_C(35000)

typedef struct {
  int fd;
  uint8_t *bytes;
  size_t expected;
  size_t received;
  bool eof;
  bool overflow;
} t64_test_observation_pipe;

static bool t64_test_observation_elapsed_ms(const struct timespec *start,
                                            uint64_t *elapsed_ms) {
  struct timespec now;
  int64_t seconds;
  int64_t nanoseconds;
  if (start == NULL || elapsed_ms == NULL ||
      clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
    return false;
  }
  seconds = (int64_t)now.tv_sec - (int64_t)start->tv_sec;
  nanoseconds = (int64_t)now.tv_nsec - (int64_t)start->tv_nsec;
  if (nanoseconds < 0) {
    seconds--;
    nanoseconds += INT64_C(1000000000);
  }
  if (seconds < 0) return false;
  *elapsed_ms = (uint64_t)seconds * UINT64_C(1000) +
                (uint64_t)nanoseconds / UINT64_C(1000000);
  return true;
}

static bool t64_test_observation_pipes_wait(
    pid_t child, t64_test_observation_pipe *pipes, size_t pipe_count,
    int *status_out) {
  struct timespec start;
  bool child_done = false;
  bool valid = true;
  int child_status = 0;
  pid_t waited = -1;
  size_t i;

  if (child <= 0 || pipes == NULL || pipe_count == 0u ||
      pipe_count > T64_TEST_OBSERVATION_PIPE_MAX || status_out == NULL ||
      clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
    return false;
  }
  *status_out = 0;
  for (i = 0u; i < pipe_count; i++) {
    int flags;
    if (pipes[i].fd < 0 || pipes[i].bytes == NULL ||
        pipes[i].expected == 0u) {
      valid = false;
      continue;
    }
    flags = fcntl(pipes[i].fd, F_GETFL);
    if (flags < 0 || fcntl(pipes[i].fd, F_SETFL, flags | O_NONBLOCK) != 0) {
      valid = false;
    }
  }

  for (;;) {
    struct pollfd poll_fds[T64_TEST_OBSERVATION_PIPE_MAX];
    uint64_t elapsed_ms;
    int poll_result;
    bool all_eof = true;

    if (!child_done) {
      do {
        waited = waitpid(child, &child_status, WNOHANG);
      } while (waited < 0 && errno == EINTR);
      if (waited == child) {
        child_done = true;
        *status_out = child_status;
      } else if (waited < 0) {
        valid = false;
        child_done = true;
      }
    }
    for (i = 0u; i < pipe_count; i++) {
      poll_fds[i].fd = pipes[i].eof ? -1 : pipes[i].fd;
      poll_fds[i].events = POLLIN | POLLHUP | POLLERR;
      poll_fds[i].revents = 0;
      if (!pipes[i].eof) all_eof = false;
    }
    if (child_done && all_eof) break;

    if (!t64_test_observation_elapsed_ms(&start, &elapsed_ms) ||
        elapsed_ms >= T64_TEST_OBSERVATION_PIPE_TIMEOUT_MS) {
      valid = false;
      if (!child_done) {
        unsigned retry;
        (void)kill(child, SIGTERM);
        for (retry = 0u; retry < 10u; retry++) {
          do {
            waited = waitpid(child, &child_status, WNOHANG);
          } while (waited < 0 && errno == EINTR);
          if (waited == child || waited < 0) break;
          (void)poll(NULL, 0u, 100);
        }
        if (waited != child) {
          (void)kill(child, SIGKILL);
          do {
            waited = waitpid(child, &child_status, 0);
          } while (waited < 0 && errno == EINTR);
        }
        if (waited == child) *status_out = child_status;
      }
      break;
    }

    do {
      poll_result = poll(poll_fds, (nfds_t)pipe_count, 100);
    } while (poll_result < 0 && errno == EINTR);
    if (poll_result < 0) {
      valid = false;
      continue;
    }
    for (i = 0u; i < pipe_count; i++) {
      if (poll_fds[i].fd < 0 || poll_fds[i].revents == 0) continue;
      for (;;) {
        uint8_t discard[256];
        uint8_t *destination;
        size_t capacity;
        ssize_t count;
        if (pipes[i].received < pipes[i].expected) {
          destination = pipes[i].bytes + pipes[i].received;
          capacity = pipes[i].expected - pipes[i].received;
        } else {
          destination = discard;
          capacity = sizeof discard;
        }
        count = read(pipes[i].fd, destination, capacity);
        if (count < 0 && errno == EINTR) continue;
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        if (count < 0) {
          valid = false;
          pipes[i].eof = true;
          break;
        }
        if (count == 0) {
          pipes[i].eof = true;
          break;
        }
        if (pipes[i].received == pipes[i].expected) {
          pipes[i].overflow = true;
          valid = false;
        } else {
          pipes[i].received += (size_t)count;
        }
      }
    }
  }

  for (i = 0u; i < pipe_count; i++) {
    if (pipes[i].received != pipes[i].expected || !pipes[i].eof ||
        pipes[i].overflow) {
      valid = false;
    }
  }
  if (!child_done) {
    do {
      waited = waitpid(child, &child_status, 0);
    } while (waited < 0 && errno == EINTR);
    if (waited == child) {
      *status_out = child_status;
    } else {
      valid = false;
    }
  }
  return valid;
}

#endif
