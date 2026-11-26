/*
 * Two threads, one instance each (Phase 11, plan 11-20, API-02, T-11-44). The
 * installed package says that distinct instances are independent and that one
 * instance needs external serialization. This case supports the first half:
 * two threads each create their own instance with their own allocator ledger
 * and run the host loop of the hostloop case to completion at the same time
 * (with the stalled host, so each meets a backpressure stop and resumes); both
 * results must equal the independent literals and the runner-recorded pins,
 * and each other.
 *
 * Built only with -DT64C_THREADS=ON (the consumer links pthreads, which is host
 * code: the library imports none). It is a local lane, never part of the gate:
 * scripts/check-install.sh tsan builds the library and this consumer with
 * -fsanitize=thread and runs the case, and a ThreadSanitizer report is a
 * finding. The TSan run needs the case to be seen to fail: -DT64C_THREADS_SEED_RACE=1
 * makes both threads advance the one same instance without serialization, which
 * the one-instance rule forbids, and check-install.sh tsan --seed-race expects
 * ThreadSanitizer to report it.
 *
 *   t64c --case threads --rom PATH
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef T64C_THREADS

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include <tendies64/t64_machine.h>

#include "t64c_cases.h"
#include "t64c_check.h"
#include "t64c_composite.h"
#include "t64c_sha256.h"

typedef struct {
  const char *rom_path;
  pthread_mutex_t *gate;
  pthread_cond_t *gate_open;
  unsigned *arrived;
  t64c_hostloop_result result;
  bool ran;
#ifdef T64C_THREADS_SEED_RACE
  t64c_fixture *shared;
#endif
} threads_job;

/* Both threads wait until both have arrived, then start together. */
static void threads_rendezvous(threads_job *job) {
  (void)pthread_mutex_lock(job->gate);
  (*job->arrived)++;
  if (*job->arrived >= 2u) {
    (void)pthread_cond_broadcast(job->gate_open);
  }
  while (*job->arrived < 2u) {
    (void)pthread_cond_wait(job->gate_open, job->gate);
  }
  (void)pthread_mutex_unlock(job->gate);
}

#ifdef T64C_THREADS_SEED_RACE
/* The seeded race: two threads advance the one same instance with nothing
 * serializing them. */
static void *threads_main(void *argument) {
  threads_job *job = (threads_job *)argument;
  uint64_t bound = 4096u;
  unsigned i;

  threads_rendezvous(job);
  for (i = 0u; i < 200u; i++) {
    t64_stop_result stop = T64_STOP_RESULT_INIT;

    (void)t64_machine_advance(
        job->shared->machine,
        &(t64_stop_request)T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, bound), &stop);
    bound += 4096u;
  }
  job->ran = true;
  return NULL;
}
#else
static void *threads_main(void *argument) {
  threads_job *job = (threads_job *)argument;

  threads_rendezvous(job);
  (void)t64c_hostloop_once(job->rom_path, &job->result);
  job->ran = true;
  return NULL;
}
#endif

#ifndef T64C_THREADS_SEED_RACE
static void threads_print(const char *label, const t64c_hostloop_result *result) {
  char hex[T64C_SHA256_HEX_SIZE];

  t64c_sha256_hex(result->digest, hex);
  (void)printf("threads: %s: completed=%d tick=%llu backpressure-stops=%u literals=%s pins=%s "
               "ledger=%s digest=%s\n",
               label, result->completed ? 1 : 0, (unsigned long long)result->tick,
               result->backpressure_stops, result->literals_equal ? "equal" : "DIFFERENT",
               result->pins_equal ? "equal" : "DIFFERENT",
               result->balanced ? "balanced" : "LEAKED", hex);
}
#endif

int t64c_case_threads(const t64c_args *args) {
  t64c_tally tally = {0ul, 0ul};
  pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t gate_open = PTHREAD_COND_INITIALIZER;
  unsigned arrived = 0u;
  threads_job jobs[2];
  pthread_t threads[2];
  bool started[2] = {false, false};
  unsigned i;

  (void)memset(jobs, 0, sizeof jobs);
  for (i = 0u; i < 2u; i++) {
    jobs[i].rom_path = args->rom_path;
    jobs[i].gate = &gate;
    jobs[i].gate_open = &gate_open;
    jobs[i].arrived = &arrived;
  }
#ifdef T64C_THREADS_SEED_RACE
  jobs[0].shared = t64c_fixture_new(args->rom_path);
  if (jobs[0].shared == NULL || t64c_fixture_admit(jobs[0].shared) != T64_STATUS_OK) {
    t64c_fixture_free(jobs[0].shared);
    return 2;
  }
  jobs[1].shared = jobs[0].shared;
#endif
  for (i = 0u; i < 2u; i++) {
    started[i] = pthread_create(&threads[i], NULL, threads_main, &jobs[i]) == 0;
    T64C_CHECK(&tally, started[i]);
  }
  for (i = 0u; i < 2u; i++) {
    if (started[i]) {
      T64C_CHECK(&tally, pthread_join(threads[i], NULL) == 0);
    }
  }
#ifdef T64C_THREADS_SEED_RACE
  (void)printf("threads: seeded race: two threads advanced one instance without serialization\n");
  T64C_CHECK(&tally, jobs[0].ran && jobs[1].ran);
  t64c_fixture_free(jobs[0].shared);
#else
  for (i = 0u; i < 2u; i++) {
    char label[16];

    (void)snprintf(label, sizeof label, "thread %u", i);
    threads_print(label, &jobs[i].result);
    T64C_CHECK(&tally, jobs[i].ran);
    T64C_CHECK(&tally, jobs[i].result.completed);
    T64C_CHECK(&tally, jobs[i].result.literals_equal);
    T64C_CHECK(&tally, jobs[i].result.pins_equal);
    T64C_CHECK(&tally, jobs[i].result.balanced);
    T64C_CHECK(&tally, jobs[i].result.backpressure_stops >= 1u);
  }
  T64C_CHECK(&tally, memcmp(jobs[0].result.digest, jobs[1].result.digest,
                            sizeof jobs[0].result.digest) == 0);
  T64C_CHECK(&tally, jobs[0].result.tick == jobs[1].result.tick);
#endif
  return t64c_report("threads", &tally);
}

#else /* T64C_THREADS */

/* The case is not part of this build: the file is an empty translation unit. */
typedef int t64c_threads_not_built;

#endif /* T64C_THREADS */
