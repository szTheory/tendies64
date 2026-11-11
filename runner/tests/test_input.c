/* TAP controls for t64.input v1 and actual warm-entry input scheduling. */
#include "t64_input.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_sha256.h"
#include "tendies64/t64_machine.h"

#define HELLO_BOUND UINT64_C(530437)
#define IPL3_BYTES 4096u
#define MAX_ROM_BYTES 1048576u

static const uint8_t expected_output[] = "Tendies64 hello via open IPL3\n";
static unsigned tap_count;
static unsigned tap_failures;

static bool check(bool passed, const char *name) {
  tap_count++;
  if (passed) {
    (void)printf("ok %u - %s\n", tap_count, name);
  } else {
    tap_failures++;
    (void)printf("not ok %u - %s\n", tap_count, name);
  }
  return passed;
}

static bool check_status(t64_input_status actual, t64_input_status expected,
                         const char *name) {
  tap_count++;
  if (actual == expected) {
    (void)printf("ok %u - %s\n", tap_count, name);
    return true;
  }
  tap_failures++;
  (void)printf("not ok %u - %s\n", tap_count, name);
  (void)printf("# expected status %u, observed %u\n", (unsigned)expected,
               (unsigned)actual);
  return false;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 4, 5)))
#endif
static bool append_text(char *buffer, size_t capacity, size_t *used,
                        const char *format, ...) {
  va_list arguments;
  int written;

  if (buffer == NULL || used == NULL || *used >= capacity) return false;
  va_start(arguments, format);
  written = vsnprintf(buffer + *used, capacity - *used, format, arguments);
  va_end(arguments);
  if (written < 0 || (size_t)written >= capacity - *used) return false;
  *used += (size_t)written;
  return true;
}

static uint64_t schedule_tick(size_t index, bool real_boot_schedule) {
  if (!real_boot_schedule) return (uint64_t)(index / 2u);
  if (index < 2u) return 0u;
  if (index == 2u) return 1u;
  if (index < 126u) return (uint64_t)(index - 1u);
  return index == 126u ? HELLO_BOUND : HELLO_BOUND + UINT64_C(1);
}

static bool build_schedule_json(size_t event_count, bool real_boot_schedule,
                                char **bytes_out, size_t *length_out) {
  size_t capacity;
  size_t used = 0u;
  size_t i;
  char *buffer;

  if (bytes_out == NULL || length_out == NULL || event_count > 129u) {
    return false;
  }
  capacity = event_count * 112u + 128u;
  buffer = (char *)malloc(capacity);
  if (buffer == NULL) return false;
  if (!append_text(buffer, capacity, &used,
                   "{\"schema\":\"t64.input\",\"schema_version\":1,\"events\":[")) {
    free(buffer);
    return false;
  }
  for (i = 0u; i < event_count; i++) {
    unsigned buttons = (unsigned)(i % 65536u);
    int32_t axis_x = (int32_t)(i % 256u) - 128;
    int32_t axis_y = 127 - (int32_t)(i % 256u);
    if (!append_text(buffer, capacity, &used,
                     "%s{\"buttons\":%u,\"port\":0,\"tick\":\"%llu\",\"x\":%d,\"y\":%d}",
                     i == 0u ? "" : ",", buttons,
                     (unsigned long long)schedule_tick(i, real_boot_schedule),
                     (int)axis_x, (int)axis_y)) {
      free(buffer);
      return false;
    }
  }
  if (!append_text(buffer, capacity, &used, "]}\n")) {
    free(buffer);
    return false;
  }
  *bytes_out = buffer;
  *length_out = used;
  return true;
}

static t64_input_schedule *parse_schedule(const uint8_t *bytes, size_t length,
                                          t64_input_status *status_out) {
  t64_input_schedule *schedule = NULL;
  t64_input_error error = {T64_INPUT_INVALID_ARGUMENT, 0u};
  t64_input_status status =
      t64_input_parse(bytes, length, &schedule, &error);
  if (status_out != NULL) *status_out = status;
  return schedule;
}

static bool bytes_equal(const uint8_t *a, size_t a_length,
                        const uint8_t *b, size_t b_length) {
  return a != NULL && b != NULL && a_length == b_length &&
         memcmp(a, b, a_length) == 0;
}

static void test_parser_contract(void) {
  static const uint8_t empty_document[] =
      "{\"events\":[],\"schema\":\"t64.input\",\"schema_version\":1}\n";
  static const char formatted_a[] =
      "{\"schema_version\":1, \"schema\":\"t64.input\", \"events\":["
      "{\"x\":-128,\"tick\":\"0\",\"buttons\":65535,\"port\":0,\"y\":127}]}\n";
  static const char formatted_b[] =
      "{\"events\":[{\"buttons\":65535,\"port\":0,\"tick\":\"0\","
      "\"x\":-128,\"y\":127}],\"schema\":\"t64.input\","
      "\"schema_version\":1}";
  static const char ordered_document[] =
      "{\"schema\":\"t64.input\",\"schema_version\":1,\"events\":["
      "{\"buttons\":7,\"port\":0,\"tick\":\"5\",\"x\":0,\"y\":0},"
      "{\"buttons\":9,\"port\":0,\"tick\":\"5\",\"x\":1,\"y\":-1}]}";
  static const char max_tick_document[] =
      "{\"schema\":\"t64.input\",\"schema_version\":1,\"events\":["
      "{\"buttons\":0,\"port\":0,\"tick\":\"18446744073709551615\","
      "\"x\":0,\"y\":0}]}";
  static const struct {
    const char *document;
    t64_input_status expected;
    const char *name;
  } invalid_cases[] = {
      {"{\"schema\":\"t64.input\",\"schema_version\":1,\"events\":[],\"extra\":0}",
       T64_INPUT_INVALID_SCHEMA, "rejects an unknown root key"},
      {"{\"schema\\u0000suffix\":\"t64.input\",\"schema_version\":1,\"events\":[]}",
       T64_INPUT_INVALID_SCHEMA, "rejects a hidden NUL suffix in a root key"},
      {"{\"schema\":\"t64.input\\u0000suffix\",\"schema_version\":1,\"events\":[]}",
       T64_INPUT_INVALID_SCHEMA, "rejects a hidden NUL suffix in the schema"},
      {"{\"schema\":\"t64.input\",\"schema\":\"t64.input\",\"schema_version\":1,\"events\":[]}",
       T64_INPUT_INVALID_JSON, "rejects duplicate decoded root keys"},
      {"{\"schema\":\"t64.input\",\"schema_version\":1,\"events\":[{\"buttons\":0,\"port\":0,\"tick\":\"0\",\"x\":0,\"y\":0,\"extra\":0}]}",
       T64_INPUT_INVALID_SCHEMA, "rejects an unknown event key"},
      {"{\"schema\":\"t64.input\",\"schema_version\":1,\"events\":[{\"buttons\":0,\"port\":0,\"tick\":\"0\",\"tick\":\"0\",\"x\":0,\"y\":0}]}",
       T64_INPUT_INVALID_JSON, "rejects duplicate decoded event keys"},
      {"{\"schema\":\"t64.input\",\"schema_version\":1,\"events\":[{\"buttons\":0,\"port\":0,\"tick\\u0000suffix\":\"0\",\"x\":0,\"y\":0}]}",
       T64_INPUT_INVALID_SCHEMA, "rejects a hidden NUL suffix in an event key"},
      {"{\"schema\":\"t64.input\",\"schema_version\":1,\"events\":[{\"buttons\":0,\"port\":0,\"tick\":\"0\\u0000suffix\",\"x\":0,\"y\":0}]}",
       T64_INPUT_INVALID_EVENT, "rejects a hidden NUL suffix in an event tick"},
      {"{\"schema\":\"t64.input\",\"schema_version\":1.0,\"events\":[]}",
       T64_INPUT_INVALID_JSON, "rejects floating point fields"},
      {"{\"schema\":\"t64.input\",\"schema_version\":1e0,\"events\":[]}",
       T64_INPUT_INVALID_JSON, "rejects exponent fields"},
      {"{\"schema\":\"t64.input\",\"schema_version\":1,\"events\":[{\"buttons\":0,\"port\":0,\"tick\":0,\"x\":0,\"y\":0}]}",
       T64_INPUT_INVALID_EVENT, "requires ticks to be decimal strings"},
      {"{\"schema\":\"t64.input\",\"schema_version\":1,\"events\":[{\"buttons\":0,\"port\":0,\"tick\":\"00\",\"x\":0,\"y\":0}]}",
       T64_INPUT_INVALID_EVENT, "rejects leading-zero ticks"},
      {"{\"schema\":\"t64.input\",\"schema_version\":1,\"events\":[{\"buttons\":0,\"port\":0,\"tick\":\"18446744073709551616\",\"x\":0,\"y\":0}]}",
       T64_INPUT_INVALID_EVENT, "rejects uint64 tick overflow"},
      {"{\"schema\":\"t64.input\",\"schema_version\":1,\"events\":[{\"buttons\":0,\"port\":0,\"tick\":\"2\",\"x\":0,\"y\":0},{\"buttons\":0,\"port\":0,\"tick\":\"1\",\"x\":0,\"y\":0}]}",
       T64_INPUT_TIME_REVERSED, "rejects reversed event ticks"},
      {"{\"schema\":\"t64.input\",\"schema_version\":1,\"events\":[{\"buttons\":65536,\"port\":0,\"tick\":\"0\",\"x\":0,\"y\":0}]}",
       T64_INPUT_INVALID_EVENT, "rejects buttons above uint16"},
      {"{\"schema\":\"t64.input\",\"schema_version\":1,\"events\":[{\"buttons\":-1,\"port\":0,\"tick\":\"0\",\"x\":0,\"y\":0}]}",
       T64_INPUT_INVALID_EVENT, "rejects negative buttons"},
      {"{\"schema\":\"t64.input\",\"schema_version\":1,\"events\":[{\"buttons\":0,\"port\":1,\"tick\":\"0\",\"x\":0,\"y\":0}]}",
       T64_INPUT_INVALID_EVENT, "rejects nonzero ports"},
      {"{\"schema\":\"t64.input\",\"schema_version\":1,\"events\":[{\"buttons\":0,\"port\":0,\"tick\":\"0\",\"x\":-129,\"y\":0}]}",
       T64_INPUT_INVALID_EVENT, "rejects X below signed 8-bit range"},
      {"{\"schema\":\"t64.input\",\"schema_version\":1,\"events\":[{\"buttons\":0,\"port\":0,\"tick\":\"0\",\"x\":0,\"y\":128}]}",
       T64_INPUT_INVALID_EVENT, "rejects Y above signed 8-bit range"},
      {"{\"schema\":\"t64.input\",\"schema_version\":1,\"events\":[{\"buttons\":0,\"port\":0,\"tick\":\"0\",\"x\":0.5,\"y\":0}]}",
       T64_INPUT_INVALID_JSON, "rejects noninteger axis values"},
      {"{\"schema\":\"t64.input\",\"schema_version\":1,\"events\":[]} trailing",
       T64_INPUT_INVALID_JSON, "rejects malformed trailing input"}};
  t64_input_schedule *empty = NULL;
  t64_input_schedule *format_a = NULL;
  t64_input_schedule *format_b = NULL;
  t64_input_schedule *ordered = NULL;
  t64_input_schedule *max_tick = NULL;
  t64_input_status status = T64_INPUT_INVALID_ARGUMENT;
  size_t canonical_length = 0u;
  const uint8_t *canonical;
  size_t i;

  empty = parse_schedule(empty_document, sizeof empty_document - 1u, &status);
  (void)check_status(status, T64_INPUT_OK,
                     "accepts an empty canonical t64.input v1 schedule");
  (void)check(empty != NULL &&
                  t64_input_schedule_event_count(empty) == 0u,
              "empty schedule is owned and contains zero events");
  canonical = t64_input_schedule_canonical_json(empty, &canonical_length);
  (void)check(bytes_equal(canonical, canonical_length, empty_document,
                          sizeof empty_document - 1u),
              "empty canonical bytes include exactly the final LF");
  (void)check(t64_input_schedule_sha256(empty) != NULL,
              "empty schedule has an owned canonical digest");

  format_a = parse_schedule((const uint8_t *)formatted_a,
                            sizeof formatted_a - 1u, &status);
  (void)check_status(status, T64_INPUT_OK,
                     "accepts reordered keys and JSON whitespace");
  format_b = parse_schedule((const uint8_t *)formatted_b,
                            sizeof formatted_b - 1u, &status);
  (void)check_status(status, T64_INPUT_OK,
                     "accepts the same schedule in compact formatting");
  if (format_a != NULL && format_b != NULL) {
    size_t a_length = 0u;
    size_t b_length = 0u;
    const uint8_t *a_json =
        t64_input_schedule_canonical_json(format_a, &a_length);
    const uint8_t *b_json =
        t64_input_schedule_canonical_json(format_b, &b_length);
    (void)check(bytes_equal(a_json, a_length, b_json, b_length),
                "allowed JSON formatting produces identical canonical bytes");
    (void)check(memcmp(t64_input_schedule_sha256(format_a),
                       t64_input_schedule_sha256(format_b),
                       T64_INPUT_SHA256_BYTES) == 0,
                "allowed JSON formatting produces stable schedule identity");
  }

  ordered = parse_schedule((const uint8_t *)ordered_document,
                           sizeof ordered_document - 1u, &status);
  (void)check_status(status, T64_INPUT_OK,
                     "accepts nondecreasing equal-tick events");
  if (ordered != NULL) {
    t64_input_event first;
    t64_input_event second;
    bool source_order = t64_input_schedule_event(ordered, 0u, &first) &&
                        t64_input_schedule_event(ordered, 1u, &second) &&
                        first.tick == 5u && second.tick == 5u &&
                        first.buttons == 7u && second.buttons == 9u;
    (void)check(source_order,
                "equal-tick events retain their original array order");
  }
  max_tick = parse_schedule((const uint8_t *)max_tick_document,
                            sizeof max_tick_document - 1u, &status);
  (void)check_status(status, T64_INPUT_OK,
                     "accepts the maximum canonical uint64 tick string");

  for (i = 0u; i < sizeof invalid_cases / sizeof invalid_cases[0]; i++) {
    t64_input_schedule *invalid = parse_schedule(
        (const uint8_t *)invalid_cases[i].document,
        strlen(invalid_cases[i].document), &status);
    (void)check_status(status, invalid_cases[i].expected,
                       invalid_cases[i].name);
    t64_input_schedule_destroy(invalid);
  }

  {
    char *document = NULL;
    size_t length = 0u;
    t64_input_schedule *schedule = NULL;
    if (build_schedule_json(T64_INPUT_MAX_EVENTS, false, &document, &length)) {
      schedule = parse_schedule((const uint8_t *)document, length, &status);
      (void)check_status(status, T64_INPUT_OK,
                         "accepts exactly 128 events");
      (void)check(schedule != NULL &&
                      t64_input_schedule_event_count(schedule) == 128u,
                  "all 128 admitted events remain available in source order");
      t64_input_schedule_destroy(schedule);
      free(document);
    } else {
      (void)check(false, "builds the 128-event parser boundary input");
    }
    document = NULL;
    length = 0u;
    if (build_schedule_json(T64_INPUT_MAX_EVENTS + 1u, false, &document,
                            &length)) {
      schedule = parse_schedule((const uint8_t *)document, length, &status);
      (void)check_status(status, T64_INPUT_EVENT_LIMIT,
                         "refuses 129 events");
      (void)check(schedule == NULL,
                  "129-event refusal returns no partial schedule");
      t64_input_schedule_destroy(schedule);
      free(document);
    } else {
      (void)check(false, "builds the 129-event parser boundary input");
    }
  }

  {
    uint8_t *padded = (uint8_t *)malloc(T64_INPUT_MAX_BYTES + 1u);
    if (padded != NULL) {
      t64_input_schedule *schedule;
      t64_input_schedule *too_large_schedule;
      (void)memset(padded, ' ', T64_INPUT_MAX_BYTES + 1u);
      (void)memcpy(padded, empty_document, sizeof empty_document - 1u);
      schedule = parse_schedule(padded, T64_INPUT_MAX_BYTES, &status);
      (void)check_status(status, T64_INPUT_OK,
                         "accepts exactly 65536 raw input bytes");
      (void)check(schedule != NULL &&
                      t64_input_schedule_event_count(schedule) == 0u,
                  "65536-byte schedule is fully parsed before success");
      t64_input_schedule_destroy(schedule);
      too_large_schedule = parse_schedule(padded, T64_INPUT_MAX_BYTES + 1u,
                                          &status);
      (void)check_status(status, T64_INPUT_SIZE_LIMIT,
                         "refuses 65537 raw input bytes");
      (void)check(too_large_schedule == NULL,
                  "oversize refusal returns no partial schedule");
      t64_input_schedule_destroy(too_large_schedule);
      free(padded);
    } else {
      (void)check(false, "allocates bounded raw-byte boundary input");
    }
  }

  {
    t64_input_schedule *held = empty;
    t64_input_schedule *before = held;
    static const char invalid[] =
        "{\"schema\":\"t64.input\",\"schema_version\":1,\"events\":["
        "{\"buttons\":0,\"port\":0,\"tick\":\"2\",\"x\":0,\"y\":0},"
        "{\"buttons\":0,\"port\":0,\"tick\":\"1\",\"x\":0,\"y\":0}]}";
    status = t64_input_parse((const uint8_t *)invalid, sizeof invalid - 1u,
                             &held, NULL);
    (void)check(status == T64_INPUT_TIME_REVERSED && held == before &&
                    t64_input_schedule_event_count(held) == 0u,
                "refusal preserves the caller's previously owned schedule");
  }

  t64_input_schedule_destroy(max_tick);
  t64_input_schedule_destroy(ordered);
  t64_input_schedule_destroy(format_b);
  t64_input_schedule_destroy(format_a);
  t64_input_schedule_destroy(empty);
}

typedef struct {
  size_t outstanding;
} allocator_state;

static bool host_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  allocator_state *state = (allocator_state *)context;
  void *memory;

  if (state == NULL || storage_out == NULL || alignment == 0u) return false;
  memory = malloc(size);
  if (memory == NULL || (uintptr_t)memory % alignment != 0u) {
    free(memory);
    return false;
  }
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  state->outstanding++;
  return true;
}

static void host_release(void *context, t64_host_storage storage) {
  allocator_state *state = (allocator_state *)context;
  if (state != NULL && storage.data != NULL) {
    free(storage.data);
    state->outstanding--;
  }
}

static bool read_file(const char *path, size_t maximum, uint8_t **bytes_out,
                      size_t *length_out) {
  FILE *file;
  long file_length;
  uint8_t *bytes;
  size_t length;

  if (path == NULL || path[0] == '\0' || bytes_out == NULL ||
      length_out == NULL) {
    return false;
  }
  file = fopen(path, "rb");
  if (file == NULL) return false;
  if (fseek(file, 0L, SEEK_END) != 0 || (file_length = ftell(file)) < 0L ||
      (unsigned long)file_length > (unsigned long)maximum ||
      fseek(file, 0L, SEEK_SET) != 0) {
    (void)fclose(file);
    return false;
  }
  length = (size_t)file_length;
  bytes = (uint8_t *)malloc(length == 0u ? 1u : length);
  if (bytes == NULL) {
    (void)fclose(file);
    return false;
  }
  if (length > 0u && fread(bytes, 1u, length, file) != length) {
    free(bytes);
    (void)fclose(file);
    return false;
  }
  if (ferror(file) != 0 || fclose(file) != 0) {
    free(bytes);
    return false;
  }
  *bytes_out = bytes;
  *length_out = length;
  return true;
}

static bool file_sha256_matches(const uint8_t *bytes, size_t length,
                                const char *expected_hex) {
  t64_sha256_ctx context;
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  char hex[T64_SHA256_HEX_SIZE];
  t64_sha256_init(&context);
  t64_sha256_update(&context, bytes, length);
  t64_sha256_final(&context, digest);
  t64_sha256_hex(digest, hex);
  return strcmp(hex, expected_hex) == 0;
}

typedef struct {
  uint64_t tick;
  uint64_t value;
} input_trace;

typedef enum {
  PARTITION_TRACE_16 = 0,
  PARTITION_TRACE_PATTERN,
  PARTITION_UNTRACED_4096
} partition_kind;

typedef struct {
  bool completed;
  bool output_matches;
  bool trace_matches;
  size_t trace_count;
  uint8_t digest[T64_DIGEST_SIZE];
} run_result;

static t64_machine *prepare_machine(const t64_host_allocator *allocator,
                                    const uint8_t *hello,
                                    size_t hello_length,
                                    const t64_input_schedule *schedule,
                                    bool trace_enabled) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  size_t i;

  if (t64_profile_lookup("ntsc-cart-4m-openipl3-warm-v1", &profile) !=
          T64_STATUS_OK ||
      profile == NULL ||
      t64_machine_create(allocator, profile, &machine) != T64_STATUS_OK ||
      machine == NULL) {
    if (machine != NULL) t64_machine_destroy(machine);
    return NULL;
  }
  if (t64_machine_admit_rom(machine, hello, hello_length) != T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return NULL;
  }
  for (i = 0u; i < t64_input_schedule_event_count(schedule); i++) {
    t64_input_event event;
    if (!t64_input_schedule_event(schedule, i, &event) ||
        t64_machine_queue_input(machine, event.tick, event.port,
                                event.buttons, event.axis_x,
                                event.axis_y) != T64_STATUS_OK) {
      /* Preparation fails closed: the fresh machine is disposed before run. */
      t64_machine_destroy(machine);
      return NULL;
    }
  }
  if (t64_machine_trace_set_enabled(machine, trace_enabled) != T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return NULL;
  }
  return machine;
}

static uint64_t expected_event_value(const t64_input_event *event) {
  uint64_t buttons = (uint64_t)event->buttons;
  uint64_t x = (uint64_t)(uint8_t)event->axis_x;
  uint64_t y = (uint64_t)(uint8_t)event->axis_y;
  return buttons | (x << 16) | (y << 24);
}

static bool collect_trace(t64_machine *machine, input_trace *inputs,
                          size_t input_capacity, size_t *input_count,
                          uint64_t *cumulative_drops, bool *saturated) {
  t64_trace_record records[T64_TRACE_RECORD_CAPACITY];
  size_t count = 0u;
  uint64_t dropped = 0u;
  bool drop_saturated = false;
  size_t i;

  if (t64_machine_trace_drain(machine, records, T64_TRACE_RECORD_CAPACITY,
                              &count, &dropped, &drop_saturated) !=
      T64_STATUS_OK) {
    return false;
  }
  if (dropped > *cumulative_drops) *cumulative_drops = dropped;
  if (drop_saturated) *saturated = true;
  if (count == T64_TRACE_RECORD_CAPACITY && dropped > 0u) return false;
  for (i = 0u; i < count; i++) {
    if (records[i].device == T64_TRACE_DEVICE_INPUT &&
        records[i].kind == T64_TRACE_KIND_INPUT_SUBMITTED) {
      if (*input_count >= input_capacity) return false;
      inputs[*input_count].tick = records[i].tick;
      inputs[*input_count].value = records[i].value;
      (*input_count)++;
    }
  }
  return true;
}

static bool run_to_bound(t64_machine *machine, partition_kind partition,
                         input_trace *inputs, size_t input_capacity,
                         size_t *input_count) {
  static const uint64_t pattern[] = {1u, 7u, 16u, 3u, 15u, 4u};
  uint64_t current_tick = 0u;
  uint64_t cumulative_drops = 0u;
  bool saturated = false;
  size_t partition_index = 0u;

  while (current_tick < HELLO_BOUND) {
    uint64_t maximum_chunk;
    uint64_t next_tick;
    t64_stop_request request;
    t64_stop_result result = {UINT32_MAX, UINT64_MAX};
    t64_status status;

    if (partition == PARTITION_TRACE_16) {
      maximum_chunk = 16u;
    } else if (partition == PARTITION_TRACE_PATTERN) {
      maximum_chunk = pattern[partition_index %
                              (sizeof pattern / sizeof pattern[0])];
      partition_index++;
    } else {
      maximum_chunk = 4096u;
    }
    next_tick = HELLO_BOUND - current_tick > maximum_chunk
                    ? current_tick + maximum_chunk
                    : HELLO_BOUND;
    request.kind = T64_STOP_GUEST_TICK;
    request.value = next_tick;
    status = t64_machine_advance(machine, &request, &result);
    if (status != T64_STATUS_OK || result.tick <= current_tick ||
        result.tick > next_tick) {
      return false;
    }
    if (result.reason == T64_STOP_REASON_BOUND) {
      if (result.tick != next_tick) return false;
    } else if (result.reason == T64_STOP_REASON_RESOURCE_LIMIT) {
      /* Continue only a work-budget yield that made monotonic guest progress. */
      if (result.tick >= next_tick) return false;
    } else {
      /* Preserve terminal/error outcomes such as unsupported CPU execution. */
      return false;
    }
    current_tick = result.tick;
    if (partition != PARTITION_UNTRACED_4096 &&
        !collect_trace(machine, inputs, input_capacity, input_count,
                       &cumulative_drops, &saturated)) {
      return false;
    }
    if (partition != PARTITION_UNTRACED_4096 &&
        (cumulative_drops != 0u || saturated)) {
      return false;
    }
  }
  return current_tick == HELLO_BOUND;
}

static bool execute_case(const uint8_t *ipl3, size_t ipl3_length,
                         const uint8_t *hello, size_t hello_length,
                         const t64_input_schedule *schedule,
                         partition_kind partition, run_result *out) {
  allocator_state state = {0u};
  t64_host_allocator allocator = {host_allocate, host_release, &state};
  input_trace observed[T64_INPUT_MAX_EVENTS];
  size_t observed_count = 0u;
  bool trace_enabled = partition != PARTITION_UNTRACED_4096;
  t64_machine *machine;
  t64_output_drain_result drain = {0};
  uint8_t output[sizeof expected_output - 1u];
  bool did_run;

  if (ipl3 == NULL || ipl3_length != IPL3_BYTES || hello == NULL ||
      hello_length == 0u || schedule == NULL || out == NULL) {
    return false;
  }
  (void)memset(out, 0, sizeof *out);
  machine = prepare_machine(&allocator, hello, hello_length, schedule,
                            trace_enabled);
  if (machine == NULL) return false;
  /* Every parsed event has been queued on this fresh instance before advance. */
  did_run = run_to_bound(machine, partition, observed,
                         T64_INPUT_MAX_EVENTS, &observed_count);
  if (did_run) {
    t64_status drain_status = t64_machine_output_drain(
        machine, output, sizeof output, &drain);
    out->output_matches =
        drain_status == T64_STATUS_OK && drain.copied_bytes == sizeof output &&
        drain.total_attempted_bytes == sizeof output && !drain.lost_output &&
        drain.total_attempted_bytes_exact &&
        memcmp(output, expected_output, sizeof output) == 0;
    out->completed = t64_machine_digest(machine, out->digest) ==
                     T64_STATUS_OK;
  }
  out->trace_count = observed_count;
  if (trace_enabled) {
    size_t i;
    size_t expected_count = t64_input_schedule_event_count(schedule);
    if (expected_count > 0u) expected_count--;
    out->trace_matches = did_run && observed_count == expected_count;
    for (i = 0u; out->trace_matches && i < expected_count; i++) {
      t64_input_event event;
      out->trace_matches =
          t64_input_schedule_event(schedule, i, &event) &&
          observed[i].tick == event.tick &&
          observed[i].value == expected_event_value(&event);
    }
  }
  t64_machine_destroy(machine);
  return did_run && state.outstanding == 0u;
}

static void test_actual_boot_schedule(void) {
  static const char expected_asset_sha[] =
      "f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068";
  const char *asset_path = getenv("T64_TEST_IPL3_PATH");
  const char *hello_path = getenv("T64_TEST_HELLO_PATH");
  const char *fixtures_dir = getenv("T64_BOOT_FIXTURES_DIR");
  uint8_t *asset = NULL;
  size_t asset_length = 0u;
  uint8_t *hello = NULL;
  size_t hello_length = 0u;
  uint8_t *fixture_input = NULL;
  size_t fixture_input_length = 0u;
  char fixture_input_path[1024];
  t64_input_schedule *empty_schedule = NULL;
  t64_input_schedule *full_schedule = NULL;
  t64_input_schedule *prefix_schedule = NULL;
  t64_input_status status = T64_INPUT_INVALID_ARGUMENT;
  char *full_document = NULL;
  size_t full_length = 0u;
  char *prefix_document = NULL;
  size_t prefix_length = 0u;

  run_result trace_16 = {0};
  run_result trace_pattern = {0};
  run_result untraced = {0};
  run_result no_future = {0};
  bool files_ok;
  bool fixture_identity = false;
  bool schedules_ok = false;
  bool runs_ok = false;

  files_ok = read_file(asset_path, IPL3_BYTES, &asset, &asset_length) &&
             asset_length == IPL3_BYTES &&
             file_sha256_matches(asset, asset_length, expected_asset_sha) &&
             read_file(hello_path, MAX_ROM_BYTES, &hello, &hello_length) &&
             hello_length > IPL3_BYTES && fixtures_dir != NULL &&
             snprintf(fixture_input_path, sizeof fixture_input_path,
                      "%s/hello.input.json", fixtures_dir) > 0 &&
             (size_t)snprintf(fixture_input_path, sizeof fixture_input_path,
                              "%s/hello.input.json", fixtures_dir) <
                 sizeof fixture_input_path &&
             read_file(fixture_input_path, T64_INPUT_MAX_BYTES,
                       &fixture_input, &fixture_input_length);
  (void)check(files_ok,
              "loads the pinned IPL3, authored hello, and produced input fixture");
  if (!files_ok) goto cleanup;

  empty_schedule = parse_schedule(fixture_input, fixture_input_length, &status);
  if (empty_schedule != NULL) {
    size_t canonical_length = 0u;
    const uint8_t *canonical = t64_input_schedule_canonical_json(
        empty_schedule, &canonical_length);
    fixture_identity = t64_input_schedule_event_count(empty_schedule) == 0u &&
                       bytes_equal(canonical, canonical_length, fixture_input,
                                   fixture_input_length);
  }
  (void)check(status == T64_INPUT_OK && fixture_identity,
              "generated empty fixture equals the canonical t64.input bytes including LF");

  if (build_schedule_json(T64_INPUT_MAX_EVENTS, true, &full_document,
                          &full_length) &&
      build_schedule_json(T64_INPUT_MAX_EVENTS - 1u, true, &prefix_document,
                          &prefix_length)) {
    full_schedule = parse_schedule((const uint8_t *)full_document, full_length,
                                   &status);
    schedules_ok = status == T64_INPUT_OK &&
                   t64_input_schedule_event_count(full_schedule) ==
                       T64_INPUT_MAX_EVENTS;
    prefix_schedule = parse_schedule((const uint8_t *)prefix_document,
                                     prefix_length, &status);
    schedules_ok = schedules_ok && status == T64_INPUT_OK &&
                   t64_input_schedule_event_count(prefix_schedule) ==
                       T64_INPUT_MAX_EVENTS - 1u;
  }
  (void)check(schedules_ok,
              "parses the 128-event boot schedule and its 127-event pending-event control");
  if (!schedules_ok) goto cleanup;

  (void)printf("# actual boot bound=%llu; trace-enabled partitions <=16 ticks; beyond-bound input=%llu\n",
               (unsigned long long)HELLO_BOUND,
               (unsigned long long)(HELLO_BOUND + UINT64_C(1)));
  runs_ok = execute_case(asset, asset_length, hello, hello_length,
                         full_schedule, PARTITION_TRACE_16, &trace_16) &&
            execute_case(asset, asset_length, hello, hello_length,
                         full_schedule, PARTITION_TRACE_PATTERN,
                         &trace_pattern) &&
            execute_case(asset, asset_length, hello, hello_length,
                         full_schedule, PARTITION_UNTRACED_4096, &untraced) &&
            execute_case(asset, asset_length, hello, hello_length,
                         prefix_schedule, PARTITION_UNTRACED_4096,
                         &no_future);
  (void)check(runs_ok,
              "prequeues each complete schedule on a fresh instance and advances only to the fixed bound");
  if (!runs_ok) goto cleanup;

  (void)check(trace_16.completed && trace_pattern.completed &&
                  untraced.completed && no_future.completed,
              "all actual hello runs reach the requested guest-time bound");
  (void)check(trace_16.output_matches && trace_pattern.output_matches &&
                  untraced.output_matches && no_future.output_matches,
              "all bounded actual hello runs capture the independent 30-byte output");
  (void)check(trace_16.trace_matches && trace_pattern.trace_matches &&
                  trace_16.trace_count == T64_INPUT_MAX_EVENTS - 1u &&
                  trace_pattern.trace_count == T64_INPUT_MAX_EVENTS - 1u,
              "matching input source and kind yields ordered records through the bound with no drops");
  (void)check(memcmp(trace_16.digest, trace_pattern.digest,
                     T64_DIGEST_SIZE) == 0 &&
                  memcmp(trace_16.digest, untraced.digest,
                         T64_DIGEST_SIZE) == 0,
              "same actual boot schedule has identical final state across partitions and trace toggles");
  (void)check(memcmp(untraced.digest, no_future.digest,
                     T64_DIGEST_SIZE) != 0,
              "the input one tick beyond the bound remains pending in canonical guest state");
  (void)check(trace_16.trace_count == T64_INPUT_MAX_EVENTS - 1u,
              "the event exactly at the bound is consumed and the later event is not traced");

cleanup:
  free(prefix_document);
  free(full_document);
  t64_input_schedule_destroy(prefix_schedule);
  t64_input_schedule_destroy(full_schedule);
  t64_input_schedule_destroy(empty_schedule);
  free(fixture_input);
  free(hello);
  free(asset);
}

int main(void) {
  (void)puts("TAP version 13");
  test_parser_contract();
  test_actual_boot_schedule();
  (void)printf("1..%u\n", tap_count);
  return tap_count > 0u && tap_failures == 0u ? 0 : 1;
}
