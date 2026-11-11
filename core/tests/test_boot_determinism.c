#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../t64_internal.h"
#include "../../tools/common/t64_sha256.h"
#include "t64_test.h"

#define IPL3_BYTES 4096u
#define HELLO_MAX_BYTES (1u << 20)
#define INPUT_MAX_BYTES 4096u
#define PROFILE_ID "ntsc-cart-4m-openipl3-warm-v1"
#define IPL3_SHA256 \
  "f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068"
#define IPL3_PAYLOAD_SHA256 \
  "f716a90b1d36ec4253d096790791256d3543455e54473a755ff8c3d1199b15d5"
#define HELLO_SHA256 \
  "fa276cc175a4e764b6fd5413193c44b51f38fd8135e33e400032827e02f27c7c"
#define INPUT_SHA256 \
  "6d5593bc2d95465b44ec7ce7785e5ff7d7d53605de9b039a021bc697d226186d"
#define PROFILE_SHA256 \
  "d8c12cb2a00410a2153ca62da318166f3ca029dea8e3d6b9c841262cb8203c71"
#define HELLO_BOUND UINT64_C(530500)
#define HELLO_COMPLETION_TICK UINT64_C(530438)
#define HELLO_INSTRUCTION_COUNT UINT64_C(530437)
#define INPUT_FIRST_VALUE UINT64_C(0x09f91357)
#define INPUT_SECOND_VALUE UINT64_C(0xfc032468)

static const uint8_t expected_log[] = "Tendies64 hello via open IPL3\n";

typedef struct {
  size_t outstanding;
} allocator_state;

typedef struct {
  uint8_t asset[IPL3_BYTES];
  uint8_t *hello;
  size_t hello_length;
  uint8_t input[INPUT_MAX_BYTES];
  size_t input_length;
} fixture_bundle;

typedef struct {
  t64_machine *machine;
  uint64_t request_bound;
  uint64_t partition;
  uint64_t logical_target;
  uint64_t current_tick;
  uint64_t drain_interval;
  size_t drain_capacity;
  size_t output_length;
  uint8_t output[T64_ISVIEWER_CAPTURE_BYTES];
  uint8_t captured_sha256[T64_DIGEST_SIZE];
  uint64_t total_attempted_bytes;
  uint32_t input_trace_index;
  bool trace_enabled;
  bool total_attempted_bytes_exact;
  bool done;
  bool saw_bad_input_trace_order;
  uint64_t resource_yields;
  t64_stop_result stop;
} machine_run;

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  allocator_state *state = (allocator_state *)context;
  void *memory = NULL;
  size_t host_alignment = alignment;

  if (state == NULL || storage_out == NULL || size == 0u || alignment == 0u) {
    return false;
  }
  if (host_alignment < sizeof(void *)) host_alignment = sizeof(void *);
  if (posix_memalign(&memory, host_alignment, size) != 0) return false;
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = host_alignment;
  state->outstanding++;
  return true;
}

static void test_release(void *context, t64_host_storage storage) {
  allocator_state *state = (allocator_state *)context;
  if (state != NULL && storage.data != NULL) {
    free(storage.data);
    state->outstanding--;
  }
}

static bool read_file(const char *path, uint8_t *bytes, size_t capacity,
                      size_t *length_out) {
  FILE *file;
  size_t length;
  int extra;
  int read_error;
  int close_error;

  if (path == NULL || path[0] == '\0' || bytes == NULL || length_out == NULL) {
    return false;
  }
  file = fopen(path, "rb");
  if (file == NULL) return false;
  length = fread(bytes, 1u, capacity, file);
  extra = fgetc(file);
  read_error = ferror(file);
  close_error = fclose(file);
  if (read_error != 0 || close_error != 0 || extra != EOF) return false;
  *length_out = length;
  return true;
}

static bool file_digest_matches(const uint8_t *bytes, size_t length,
                                const char *expected_hex) {
  t64_sha256_ctx context;
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  char actual[T64_SHA256_HEX_SIZE];

  t64_sha256_init(&context);
  t64_sha256_update(&context, bytes, length);
  t64_sha256_final(&context, digest);
  t64_sha256_hex(digest, actual);
  return strcmp(actual, expected_hex) == 0;
}

static bool load_fixtures(fixture_bundle *bundle) {
  const char *fixture_dir = getenv("T64_BOOT_FIXTURES_DIR");
  const char *input_path = getenv("T64_TEST_INPUT_PATH");
  const char *ipl3_path = getenv("T64_TEST_IPL3_PATH");
  char path[1024];
  int n;

  if (bundle == NULL || fixture_dir == NULL || input_path == NULL ||
      ipl3_path == NULL ||
      !read_file(ipl3_path, bundle->asset, sizeof bundle->asset,
                 &bundle->input_length) ||
      bundle->input_length != sizeof bundle->asset ||
      !read_file(input_path, bundle->input, sizeof bundle->input,
                 &bundle->input_length) ||
      !file_digest_matches(bundle->input, bundle->input_length, INPUT_SHA256) ||
      !file_digest_matches(bundle->asset, sizeof bundle->asset, IPL3_SHA256) ||
      !file_digest_matches(bundle->asset + 0x40u, 4032u,
                           IPL3_PAYLOAD_SHA256)) {
    return false;
  }
  bundle->hello = (uint8_t *)malloc(HELLO_MAX_BYTES);
  if (bundle->hello == NULL) return false;
  n = snprintf(path, sizeof path, "%s/hello.z64", fixture_dir);
  if (n <= 0 || (size_t)n >= sizeof path ||
      !read_file(path, bundle->hello, HELLO_MAX_BYTES, &bundle->hello_length) ||
      !file_digest_matches(bundle->hello, bundle->hello_length, HELLO_SHA256)) {
    free(bundle->hello);
    bundle->hello = NULL;
    return false;
  }
  return true;
}

static bool make_machine(const t64_profile_descriptor *profile,
                         const t64_host_allocator *allocator,
                         const uint8_t *rom, size_t rom_length,
                         bool trace_enabled, bool queue_inputs,
                         t64_machine **machine_out) {
  t64_machine *machine = NULL;
  if (t64_machine_create(allocator, profile, &machine) != T64_STATUS_OK ||
      machine == NULL ||
      t64_machine_trace_set_enabled(machine, trace_enabled) != T64_STATUS_OK ||
      t64_machine_admit_rom(machine, rom, rom_length) != T64_STATUS_OK ||
      (queue_inputs &&
       (t64_machine_queue_input(machine, UINT64_C(100), 0u,
                                UINT16_C(0x1357), -7, 9) != T64_STATUS_OK ||
        t64_machine_queue_input(machine, UINT64_C(100), 0u,
                                UINT16_C(0x2468), 3, -4) != T64_STATUS_OK))) {
    t64_machine_destroy(machine);
    return false;
  }
  *machine_out = machine;
  return true;
}

static bool drain_trace(machine_run *run) {
  t64_trace_record records[T64_TRACE_RECORD_CAPACITY];
  size_t count = 0u;
  uint64_t dropped = 0u;
  bool saturated = false;
  size_t i;

  if (t64_machine_trace_drain(run->machine, records,
                              T64_TRACE_RECORD_CAPACITY, &count, &dropped,
                              &saturated) != T64_STATUS_OK ||
      count > T64_TRACE_RECORD_CAPACITY || dropped != 0u || saturated) {
    return false;
  }
  for (i = 0u; i < count; i++) {
    if (records[i].device == T64_TRACE_DEVICE_INPUT &&
        records[i].kind == T64_TRACE_KIND_INPUT_SUBMITTED) {
      uint64_t expected = run->input_trace_index == 0u
                              ? INPUT_FIRST_VALUE
                              : INPUT_SECOND_VALUE;
      if (run->input_trace_index >= 2u || records[i].tick != 100u ||
          records[i].value != expected) {
        run->saw_bad_input_trace_order = true;
        return false;
      }
      run->input_trace_index++;
    }
  }
  return true;
}

static bool drain_output(machine_run *run, size_t capacity) {
  uint8_t copied[T64_ISVIEWER_CAPTURE_BYTES];
  t64_output_drain_result result;
  t64_status status;

  if (capacity > sizeof copied || run->output_length > sizeof run->output) {
    return false;
  }
  status = t64_machine_output_drain(run->machine, copied, capacity, &result);
  if (status == T64_STATUS_BUFFER_TOO_SMALL) {
    return result.copied_bytes == 0u &&
           result.required_bytes > capacity &&
           result.total_attempted_bytes_exact;
  }
  if (status != T64_STATUS_OK || !result.total_attempted_bytes_exact ||
      result.lost_output || result.copied_bytes > sizeof run->output -
                                                    run->output_length) {
    return false;
  }
  memcpy(run->captured_sha256, result.captured_sha256,
         sizeof run->captured_sha256);
  run->total_attempted_bytes = result.total_attempted_bytes;
  run->total_attempted_bytes_exact = result.total_attempted_bytes_exact;
  if (result.copied_bytes > 0u) {
    memcpy(run->output + run->output_length, copied, result.copied_bytes);
    run->output_length += result.copied_bytes;
  }
  return true;
}

static bool advance_one_segment(machine_run *run) {
  uint64_t target;
  bool segment_complete;

  if (run->done) return true;
  if (run->current_tick >= run->request_bound) return false;
  if (run->logical_target <= run->current_tick) {
    run->logical_target = run->request_bound - run->current_tick <
                                  run->partition
                              ? run->request_bound
                              : run->current_tick + run->partition;
  }
  target = run->logical_target;
  if (run->trace_enabled && target - run->current_tick > UINT64_C(16)) {
    target = run->current_tick + UINT64_C(16);
  }

  for (;;) {
    uint64_t before_tick = run->current_tick;
    uint64_t before_retired = run->machine->cpu.retired_instructions;
    uint64_t before_sequence = run->machine->next_sequence;
    size_t before_queue = run->machine->queue_count;
    t64_status status;

    status = t64_machine_advance(
        run->machine, &(t64_stop_request){T64_STOP_GUEST_TICK, target},
        &run->stop);
    if (status != T64_STATUS_OK || run->stop.tick < run->current_tick ||
        run->stop.tick > target) {
      return false;
    }
    run->current_tick = run->stop.tick;
    if (run->trace_enabled && !drain_trace(run)) return false;
    if (run->stop.reason == T64_STOP_REASON_RESOURCE_LIMIT) {
      if (++run->resource_yields > UINT64_C(512)) return false;
      if (run->current_tick == before_tick) {
        if (run->machine->cpu.retired_instructions == before_retired &&
            run->machine->next_sequence == before_sequence &&
            run->machine->queue_count == before_queue) {
          return false;
        }
      }
      continue;
    }
    if (run->stop.reason == T64_STOP_REASON_GUEST_COMPLETION) {
      run->done = true;
    } else if (run->stop.reason != T64_STOP_REASON_BOUND ||
               run->current_tick != target) {
      return false;
    }
    segment_complete = run->done || run->current_tick == run->logical_target;
    if (segment_complete &&
        (run->done || (run->drain_interval != 0u &&
                       run->logical_target % run->drain_interval == 0u)) &&
        !drain_output(run, run->drain_capacity)) {
      return false;
    }
    return true;
  }
}

static bool initialize_run(machine_run *run,
                           const t64_profile_descriptor *profile,
                           const fixture_bundle *bundle, uint64_t partition,
                           bool trace_enabled, uint64_t drain_interval,
                           size_t drain_capacity,
                           allocator_state *allocator_state_value) {
  const t64_host_allocator allocator = {test_allocate, test_release,
                                        allocator_state_value};
  (void)memset(run, 0, sizeof *run);
  run->request_bound = HELLO_BOUND;
  run->partition = partition;
  run->drain_interval = drain_interval;
  run->drain_capacity = drain_capacity;
  run->trace_enabled = trace_enabled;
  return make_machine(profile, &allocator, bundle->hello,
                      bundle->hello_length, trace_enabled, true,
                      &run->machine);
}

static bool drive_until_done(machine_run *run) {
  while (!run->done) {
    if (!advance_one_segment(run)) return false;
  }
  if (run->trace_enabled && run->input_trace_index != 2u) return false;
  return true;
}

static bool finish_output(machine_run *run, bool test_short_refusal) {
  t64_output_drain_result observation;
  uint8_t copied[T64_ISVIEWER_CAPTURE_BYTES];
  t64_status status;

  if (test_short_refusal && run->output_length == 0u) {
    uint8_t before[T64_DIGEST_SIZE];
    uint8_t after[T64_DIGEST_SIZE];
    T64_CHECK_INT(t64_machine_digest(run->machine, before), T64_STATUS_OK);
    status = t64_machine_output_drain(run->machine, NULL, 0u, &observation);
    T64_CHECK_INT(status, T64_STATUS_BUFFER_TOO_SMALL);
    T64_CHECK_INT(observation.required_bytes, sizeof expected_log - 1u);
    T64_CHECK_INT(observation.copied_bytes, 0u);
    T64_CHECK_INT(t64_machine_digest(run->machine, after), T64_STATUS_OK);
    T64_CHECK(memcmp(before, after, sizeof before) == 0);
  }
  if (!drain_output(run, sizeof copied)) return false;
  if (run->machine->output.captured_bytes != sizeof expected_log - 1u ||
      run->machine->output.total_attempted_bytes != sizeof expected_log - 1u ||
      run->machine->output.total_attempted_bytes_saturated ||
      run->machine->output.lost_output ||
      run->output_length != sizeof expected_log - 1u ||
      memcmp(run->output, expected_log, sizeof expected_log - 1u) != 0) {
    return false;
  }
  if (!run->total_attempted_bytes_exact ||
      run->total_attempted_bytes != sizeof expected_log - 1u) {
    return false;
  }
  {
    uint8_t expected_digest[T64_DIGEST_SIZE];
    t64_sha256_ctx context;
    t64_sha256_init(&context);
    t64_sha256_update(&context, expected_log, sizeof expected_log - 1u);
    t64_sha256_final(&context, expected_digest);
    if (memcmp(run->captured_sha256, expected_digest,
               sizeof expected_digest) != 0) {
      return false;
    }
  }
  return true;
}

static bool run_matches(const machine_run *expected,
                        const machine_run *actual) {
  uint8_t expected_state[T64_DIGEST_SIZE];
  uint8_t actual_state[T64_DIGEST_SIZE];
  uint8_t expected_content[T64_DIGEST_SIZE];
  uint8_t actual_content[T64_DIGEST_SIZE];
  uint8_t expected_profile[T64_DIGEST_SIZE];
  uint8_t actual_profile[T64_DIGEST_SIZE];
  const t64_profile_descriptor *expected_descriptor = NULL;
  const t64_profile_descriptor *actual_descriptor = NULL;

  return expected->done && actual->done &&
         expected->request_bound == actual->request_bound &&
         expected->stop.reason == actual->stop.reason &&
         expected->stop.tick == actual->stop.tick &&
         expected->machine->tick == actual->machine->tick &&
         expected->machine->instruction_count ==
             actual->machine->instruction_count &&
         t64_machine_digest(expected->machine, expected_state) == T64_STATUS_OK &&
         t64_machine_digest(actual->machine, actual_state) == T64_STATUS_OK &&
         memcmp(expected_state, actual_state, sizeof expected_state) == 0 &&
         t64_machine_content_digest(expected->machine, expected_content) ==
             T64_STATUS_OK &&
         t64_machine_content_digest(actual->machine, actual_content) ==
             T64_STATUS_OK &&
         memcmp(expected_content, actual_content, sizeof expected_content) == 0 &&
         t64_machine_profile(expected->machine, &expected_descriptor) ==
             T64_STATUS_OK &&
         t64_machine_profile(actual->machine, &actual_descriptor) ==
             T64_STATUS_OK &&
         t64_profile_hash(expected_descriptor, expected_profile) ==
             T64_STATUS_OK &&
         t64_profile_hash(actual_descriptor, actual_profile) == T64_STATUS_OK &&
         memcmp(expected_profile, actual_profile, sizeof expected_profile) == 0 &&
         expected->output_length == actual->output_length &&
         memcmp(expected->output, actual->output,
                expected->output_length) == 0 &&
         expected->total_attempted_bytes_exact &&
         actual->total_attempted_bytes_exact &&
         expected->total_attempted_bytes == actual->total_attempted_bytes &&
         memcmp(expected->captured_sha256, actual->captured_sha256,
                sizeof expected->captured_sha256) == 0;
}

static bool run_to_completion(machine_run *run) {
  if (!drive_until_done(run)) return false;
  if (run->trace_enabled && run->input_trace_index != 2u) return false;
  return finish_output(run, false);
}

static void test_full_partition_determinism(
    const fixture_bundle *bundle, const t64_profile_descriptor *profile) {
  allocator_state allocator_states[6] = {{0u}};
  machine_run runs[6];
  bool initialized[6] = {false, false, false, false, false, false};
  uint8_t baseline_state[T64_DIGEST_SIZE];
  char profile_hex[T64_SHA256_HEX_SIZE];
  t64_status status;
  size_t i;

  T64_CHECK_INT(t64_profile_hash(profile, baseline_state), T64_STATUS_OK);
  t64_sha256_hex(baseline_state, profile_hex);
  T64_CHECK_STR(profile_hex, PROFILE_SHA256);
  initialized[0] = initialize_run(&runs[0], profile, bundle, HELLO_BOUND,
                                  false, 0u, T64_ISVIEWER_CAPTURE_BYTES,
                                  &allocator_states[0]);
  initialized[1] = initialize_run(&runs[1], profile, bundle, 1u, false, 0u,
                                  T64_ISVIEWER_CAPTURE_BYTES,
                                  &allocator_states[1]);
  initialized[2] = initialize_run(&runs[2], profile, bundle, 7u, true, 7u,
                                  64u, &allocator_states[2]);
  initialized[3] = initialize_run(&runs[3], profile, bundle, 31u, true, 31u,
                                  64u, &allocator_states[3]);
  initialized[4] = initialize_run(&runs[4], profile, bundle, 7u, false, 7u,
                                  30u, &allocator_states[4]);
  initialized[5] = initialize_run(&runs[5], profile, bundle, 31u, true, 31u,
                                  64u, &allocator_states[5]);
  for (i = 0u; i < 6u; i++) T64_CHECK(initialized[i]);
  for (i = 0u; i < 4u; i++) {
    if (!initialized[i]) goto done;
    T64_CHECK(run_to_completion(&runs[i]));
  }
  if (!initialized[4] || !initialized[5]) goto done;

  /* Interleave the two independent instances one logical partition at a
   * time. Trace-on advancement still drains at most every 16 guest ticks. */
  while (!runs[4].done || !runs[5].done) {
    if (!runs[4].done) T64_CHECK(advance_one_segment(&runs[4]));
    if (!runs[5].done) T64_CHECK(advance_one_segment(&runs[5]));
    if (t64_test_failures != 0u) goto done;
  }
  T64_CHECK(runs[5].input_trace_index == 2u);
  T64_CHECK(finish_output(&runs[4], false));
  T64_CHECK(finish_output(&runs[5], false));

  for (i = 0u; i < 6u; i++) {
    T64_CHECK_INT(runs[i].stop.reason, T64_STOP_REASON_GUEST_COMPLETION);
    T64_CHECK_INT(runs[i].stop.tick, HELLO_COMPLETION_TICK);
    T64_CHECK_INT(runs[i].machine->instruction_count,
                  HELLO_INSTRUCTION_COUNT);
    T64_CHECK(run_matches(&runs[0], &runs[i]));
  }
  status = t64_machine_digest(runs[0].machine, baseline_state);
  T64_CHECK_INT(status, T64_STATUS_OK);

done:
  for (i = 0u; i < 6u; i++) {
    t64_machine_destroy(runs[i].machine);
    T64_CHECK_INT(allocator_states[i].outstanding, 0u);
  }
}

static bool input_events_queued(const t64_machine *machine, size_t expected) {
  size_t count = 0u;
  size_t i;
  for (i = 0u; i < machine->queue_count; i++) {
    size_t index = (machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY;
    if (machine->events[index].device == T64_TRACE_DEVICE_INPUT) count++;
  }
  return count == expected;
}

static bool advance_one_request(t64_machine *machine, uint64_t bound,
                                t64_stop_result *result_out) {
  uint64_t yields = 0u;
  for (;;) {
    uint64_t before_tick = machine->tick;
    uint64_t before_retired = machine->cpu.retired_instructions;
    uint64_t before_sequence = machine->next_sequence;
    size_t before_queue = machine->queue_count;
    t64_status status = t64_machine_advance(
        machine, &(t64_stop_request){T64_STOP_GUEST_TICK, bound}, result_out);
    if (status != T64_STATUS_OK || result_out->tick < before_tick ||
        result_out->tick > bound) {
      return false;
    }
    if (result_out->reason != T64_STOP_REASON_RESOURCE_LIMIT) return true;
    if (++yields > UINT64_C(512)) return false;
    if (result_out->tick == before_tick &&
        machine->cpu.retired_instructions == before_retired &&
        machine->next_sequence == before_sequence &&
        machine->queue_count == before_queue) {
      return false;
    }
  }
}

static void test_completion_deadline_precedence(
    const fixture_bundle *bundle, const t64_profile_descriptor *profile) {
  allocator_state state = {0u};
  const t64_host_allocator allocator = {test_allocate, test_release, &state};
  t64_machine *machine = NULL;
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];

  T64_CHECK(make_machine(profile, &allocator, bundle->hello,
                         bundle->hello_length, false, false, &machine));
  if (machine == NULL) return;
  T64_CHECK(advance_one_request(machine, HELLO_COMPLETION_TICK - 1u,
                                &result));
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, HELLO_COMPLETION_TICK - 1u);
  T64_CHECK_INT(machine->cpu.retired_instructions,
                HELLO_INSTRUCTION_COUNT);
  T64_CHECK(advance_one_request(machine, HELLO_COMPLETION_TICK, &result));
  T64_CHECK_INT(result.reason, T64_STOP_REASON_GUEST_COMPLETION);
  T64_CHECK_INT(result.tick, HELLO_COMPLETION_TICK);
  T64_CHECK_INT(machine->cpu.retired_instructions, HELLO_INSTRUCTION_COUNT);
  T64_CHECK_INT(t64_machine_digest(machine, before), T64_STATUS_OK);
  T64_CHECK(advance_one_request(machine, HELLO_COMPLETION_TICK + 1u, &result));
  T64_CHECK_INT(result.reason, T64_STOP_REASON_GUEST_COMPLETION);
  T64_CHECK_INT(result.tick, HELLO_COMPLETION_TICK);
  T64_CHECK_INT(machine->cpu.retired_instructions, HELLO_INSTRUCTION_COUNT);
  T64_CHECK_INT(t64_machine_digest(machine, after), T64_STATUS_OK);
  T64_CHECK(memcmp(before, after, sizeof before) == 0);
  t64_machine_destroy(machine);
  T64_CHECK_INT(state.outstanding, 0u);
}

static void test_inclusive_deadline_and_stop_bounds(
    const fixture_bundle *bundle, const t64_profile_descriptor *profile) {
  allocator_state state = {0u};
  const t64_host_allocator allocator = {test_allocate, test_release, &state};
  t64_machine *machine = NULL;
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  machine_run observation = {0};
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  t64_event_reservation reservation;
  uint64_t before_next_sequence;
  size_t before_queue_count;

  T64_CHECK(make_machine(profile, &allocator, bundle->hello,
                         bundle->hello_length, true, true, &machine));
  if (machine == NULL) return;
  observation.machine = machine;
  observation.trace_enabled = true;
  observation.partition = UINT64_C(16);
  T64_CHECK_INT(t64_machine_advance(
                    machine, &(t64_stop_request){T64_STOP_GUEST_TICK, 0u},
                    &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, 0u);
  /* The CPU has not retired before guest tick one. */
  T64_CHECK_INT(machine->instruction_count, 0u);
  T64_CHECK_INT(machine->cpu.retired_instructions, 0u);
  observation.current_tick = result.tick;
  T64_CHECK(drain_trace(&observation));
  T64_CHECK_INT(t64_machine_advance(
                    machine, &(t64_stop_request){T64_STOP_GUEST_TICK, 1u},
                    &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, 1u);
  T64_CHECK_INT(machine->instruction_count, 1u);
  T64_CHECK_INT(machine->cpu.retired_instructions, 1u);
  observation.current_tick = result.tick;
  observation.logical_target = result.tick;
  T64_CHECK(drain_trace(&observation));
  observation.request_bound = UINT64_C(99);
  while (observation.current_tick < observation.request_bound) {
    bool step_ok = advance_one_segment(&observation);
    T64_CHECK(step_ok);
    if (!step_ok) break;
  }
  T64_CHECK_INT(observation.stop.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(observation.stop.tick, 99u);
  T64_CHECK_INT(observation.current_tick, 99u);
  T64_CHECK(input_events_queued(machine, 2u));
  observation.request_bound = UINT64_C(100);
  T64_CHECK(advance_one_segment(&observation));
  T64_CHECK_INT(observation.stop.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(observation.stop.tick, 100u);
  T64_CHECK(input_events_queued(machine, 0u));
  T64_CHECK_INT(observation.input_trace_index, 2u);
  T64_CHECK(!observation.saw_bad_input_trace_order);
  observation.request_bound = UINT64_C(101);
  T64_CHECK(advance_one_segment(&observation));
  T64_CHECK_INT(observation.stop.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(observation.stop.tick, 101u);

  T64_CHECK_INT(t64_machine_digest(machine, before), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_advance(
                    machine, &(t64_stop_request){T64_STOP_GUEST_TICK, 100u},
                    &result),
                T64_STATUS_TIME_REVERSED);
  T64_CHECK_INT(t64_machine_digest(machine, after), T64_STATUS_OK);
  T64_CHECK(memcmp(before, after, sizeof before) == 0);

  machine->tick = UINT64_MAX;
  before_next_sequence = machine->next_sequence;
  before_queue_count = machine->queue_count;
  T64_CHECK_INT(t64_machine_digest(machine, before), T64_STATUS_OK);
  T64_CHECK_INT(t64_schedule_reserve_after(
                    machine, 1u, T64_DEVICE_SYNTHETIC_TEST,
                    T64_TEST_EVENT_NOOP, 1u, 0u, NULL, &reservation),
                T64_STATUS_TICK_OVERFLOW);
  T64_CHECK_INT(machine->next_sequence, before_next_sequence);
  T64_CHECK_INT(machine->queue_count, before_queue_count);
  T64_CHECK_INT(t64_machine_digest(machine, after), T64_STATUS_OK);
  T64_CHECK(memcmp(before, after, sizeof before) == 0);
  t64_machine_destroy(machine);
  T64_CHECK_INT(state.outstanding, 0u);
}

static bool digest_changes_after_toggle(t64_machine *machine, void *field,
                                         size_t field_size) {
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  uint8_t saved[sizeof(uint64_t)];
  t64_status before_status;
  t64_status after_status;

  if (field == NULL || field_size == 0u || field_size > sizeof saved) {
    return false;
  }
  before_status = t64_machine_digest(machine, before);
  memcpy(saved, field, field_size);
  ((uint8_t *)field)[0] ^= UINT8_C(1);
  after_status = t64_machine_digest(machine, after);
  memcpy(field, saved, field_size);
  return before_status == T64_STATUS_OK && after_status == T64_STATUS_OK &&
         memcmp(before, after, sizeof before) != 0;
}

static void test_warm_future_field_audit(
    const fixture_bundle *bundle, const t64_profile_descriptor *profile) {
  allocator_state state = {0u};
  const t64_host_allocator allocator = {test_allocate, test_release, &state};
  t64_machine *machine = NULL;
  uint8_t digest[T64_DIGEST_SIZE];
  char digest_hex[T64_SHA256_HEX_SIZE];
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  allocator_state alternate_host = {0u};
  size_t i;

  T64_CHECK(make_machine(profile, &allocator, bundle->asset,
                         sizeof bundle->asset, false, false, &machine));
  if (machine == NULL) return;
  T64_CHECK_INT(t64_machine_digest(machine, digest), T64_STATUS_OK);
  t64_sha256_hex(digest, digest_hex);
  T64_CHECK_STR(digest_hex,
                "d2212ea39cadf8642cab2451fbcea484f5c6997d51619faef41e9ada24aa9724");

#define AUDIT(field) \
  T64_CHECK(digest_changes_after_toggle(machine, &(field), sizeof(field)))
  for (i = 0u; i < 32u; i++) {
    AUDIT(machine->cpu.gpr[i]);
    AUDIT(machine->cpu.cp0[i]);
  }
  AUDIT(machine->cpu.hi);
  AUDIT(machine->cpu.lo);
  AUDIT(machine->cpu.retired_instructions);
  AUDIT(machine->cpu.pc);
  AUDIT(machine->cpu.last_pc);
  AUDIT(machine->cpu.last_opcode);
  AUDIT(machine->cpu.delay_slot_pc);
  AUDIT(machine->cpu.delay_target);
  AUDIT(machine->cpu.delay_slot_pending);
  AUDIT(machine->cpu.stopped);
  AUDIT(machine->cpu.stop_reason);
  AUDIT(machine->cpu.stop_pc);
  AUDIT(machine->cpu.stop_opcode);
  AUDIT(machine->cache.policy_version);
  AUDIT(machine->cache.last_operation);
  AUDIT(machine->cache.last_pc);
  AUDIT(machine->cache.last_address);
  AUDIT(machine->cache.operation_seen);
  AUDIT(machine->guest_devices.policy_version);
  AUDIT(machine->guest_devices.mi_version);
  AUDIT(machine->guest_devices.ri_select);
  AUDIT(machine->guest_devices.mi_version_known);
  AUDIT(machine->guest_devices.ri_select_known);
  AUDIT(machine->boot.source_offset);
  AUDIT(machine->boot.payload_length);
  AUDIT(machine->boot.dmem_offset);
  AUDIT(machine->boot.entry_pc);
  AUDIT(machine->boot.seed_reset_version);
  AUDIT(machine->boot.seed_s3_rom_type);
  AUDIT(machine->boot.seed_s4_tv_type);
  AUDIT(machine->boot.seed_s5_reset_type);
  AUDIT(machine->boot.seed_s6_entropy);
  AUDIT(machine->boot.seed_s7_boot_info_version);
  AUDIT(machine->boot.prepared);
  AUDIT(machine->boot.asset_sha256[0]);
  AUDIT(machine->boot.asset_sha256[sizeof machine->boot.asset_sha256 - 1u]);
  AUDIT(machine->boot.payload_sha256[0]);
  AUDIT(machine->boot.payload_sha256[sizeof machine->boot.payload_sha256 - 1u]);
  AUDIT(machine->boot.policy_sha256[0]);
  AUDIT(machine->boot.policy_sha256[sizeof machine->boot.policy_sha256 - 1u]);
  AUDIT(machine->output.policy_version);
  AUDIT(machine->output.video_output_ready);
  AUDIT(machine->output.audio_output_ready);
  AUDIT(machine->output.magic_register);
  AUDIT(machine->output.byte_count);
  AUDIT(machine->output.buffer[0]);
  AUDIT(machine->output.buffer[sizeof machine->output.buffer - 1u]);
  AUDIT(machine->output.captured_bytes);
  AUDIT(machine->output.captured[0]);
  AUDIT(machine->output.captured[sizeof machine->output.captured - 1u]);
  AUDIT(machine->output.lost_output);
  AUDIT(machine->output.total_attempted_bytes);
  AUDIT(machine->output.total_attempted_bytes_saturated);
  T64_CHECK_INT(t64_machine_queue_input(machine, 500u, 0u, 7u, -1, 1),
                T64_STATUS_OK);
  AUDIT(machine->queue_count);
  AUDIT(machine->next_sequence);
  AUDIT(machine->sequence_exhausted);
  for (i = 0u; i < machine->queue_count; i++) {
    size_t index = (machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY;
    t64_event *event = &machine->events[index];
    if (event->device == T64_TRACE_DEVICE_INPUT) {
      AUDIT(event->tick);
      AUDIT(event->sequence);
      AUDIT(event->device);
      AUDIT(event->kind);
      AUDIT(event->value);
      AUDIT(event->token);
      break;
    }
  }
#undef AUDIT

  T64_CHECK_INT(t64_machine_digest(machine, before), T64_STATUS_OK);
  machine->allocator.context = &alternate_host;
  machine->output_drain_cursor = 1u;
  machine->trace_enabled = true;
  machine->trace_count = 1u;
  machine->trace_records[0].tick = UINT64_C(999);
  T64_CHECK_INT(t64_machine_digest(machine, after), T64_STATUS_OK);
  T64_CHECK(memcmp(before, after, sizeof before) == 0);
  machine->allocator.context = (void *)&state;
  machine->output_drain_cursor = 0u;
  machine->trace_enabled = false;
  machine->trace_count = 0u;
  t64_machine_destroy(machine);
  T64_CHECK_INT(state.outstanding, 0u);
}

int main(void) {
  fixture_bundle bundle = {0};
  const t64_profile_descriptor *profile = NULL;

  T64_CHECK(load_fixtures(&bundle));
  T64_CHECK_INT(t64_profile_lookup(PROFILE_ID, &profile), T64_STATUS_OK);
  T64_CHECK(profile != NULL);
  if (bundle.hello != NULL && profile != NULL) {
    test_full_partition_determinism(&bundle, profile);
    test_completion_deadline_precedence(&bundle, profile);
    test_inclusive_deadline_and_stop_bounds(&bundle, profile);
    test_warm_future_field_audit(&bundle, profile);
  }
  free(bundle.hello);
  puts("TAP version 13");
  (void)printf("%s 1 - actual open IPL3 guest state is deterministic across partitions and observers\n",
               t64_test_failures == 0 ? "ok" : "not ok");
  puts("1..1");
  return T64_TEST_FINISH("integration.open-ipl3-determinism");
}
