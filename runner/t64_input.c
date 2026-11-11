/* Closed, bounded parser and canonical identity for t64.input v1. */
#include "t64_input.h"

#include <stdlib.h>
#include <string.h>

#include "t64_json.h"
#include "t64_jsonw.h"
#include "t64_sha256.h"

struct t64_input_schedule {
  size_t event_count;
  t64_input_event events[T64_INPUT_MAX_EVENTS];
  uint8_t *canonical_json;
  size_t canonical_length;
  uint8_t sha256[T64_INPUT_SHA256_BYTES];
};

static void set_error(t64_input_error *error_out, t64_input_status status,
                      size_t input_offset) {
  if (error_out != NULL) {
    error_out->status = status;
    error_out->input_offset = input_offset;
  }
}

static bool key_is(const t64_json_node *node, const char *key, size_t length) {
  return node->key != NULL && node->key_len == length &&
         memcmp(node->key, key, length) == 0;
}

static const t64_json_node *member(const t64_json_node *object,
                                   const char *key, size_t length) {
  const t64_json_node *node;

  if (object == NULL || object->type != T64_JSON_OBJECT) return NULL;
  for (node = object->first; node != NULL; node = node->next) {
    if (key_is(node, key, length)) return node;
  }
  return NULL;
}

static bool closed_keys(const t64_json_node *object,
                        const char *const *keys, size_t key_count) {
  const t64_json_node *node;
  bool seen[5] = {false, false, false, false, false};
  size_t found = 0u;

  if (object == NULL || object->type != T64_JSON_OBJECT || key_count > 5u) {
    return false;
  }
  for (node = object->first; node != NULL; node = node->next) {
    size_t i;
    bool matched = false;
    for (i = 0u; i < key_count; i++) {
      size_t length = strlen(keys[i]);
      if (key_is(node, keys[i], length)) {
        if (seen[i]) return false;
        seen[i] = true;
        found++;
        matched = true;
        break;
      }
    }
    if (!matched) return false;
  }
  return found == key_count;
}

static bool node_int(const t64_json_node *node, int64_t minimum,
                     int64_t maximum, int64_t *value_out) {
  if (node == NULL || node->type != T64_JSON_INT ||
      node->num < minimum || node->num > maximum) {
    return false;
  }
  *value_out = node->num;
  return true;
}

static bool parse_event(const t64_json_node *node, t64_input_event *event,
                        uint64_t previous_tick, bool have_previous,
                        t64_input_status *status_out) {
  static const char *const keys[] = {"tick", "port", "buttons", "x", "y"};
  const t64_json_node *tick_node;
  const t64_json_node *port_node;
  const t64_json_node *buttons_node;
  const t64_json_node *x_node;
  const t64_json_node *y_node;
  int64_t value;
  uint64_t tick;

  if (!closed_keys(node, keys, sizeof keys / sizeof keys[0])) {
    *status_out = T64_INPUT_INVALID_SCHEMA;
    return false;
  }
  tick_node = member(node, "tick", 4u);
  port_node = member(node, "port", 4u);
  buttons_node = member(node, "buttons", 7u);
  x_node = member(node, "x", 1u);
  y_node = member(node, "y", 1u);
  if (!t64_json_u64_string(tick_node, &tick) ||
      !node_int(port_node, 0, 0, &value)) {
    *status_out = T64_INPUT_INVALID_EVENT;
    return false;
  }
  event->tick = tick;
  event->port = 0u;
  if (!node_int(buttons_node, 0, UINT16_MAX, &value)) {
    *status_out = T64_INPUT_INVALID_EVENT;
    return false;
  }
  event->buttons = (uint16_t)value;
  if (!node_int(x_node, -128, 127, &value)) {
    *status_out = T64_INPUT_INVALID_EVENT;
    return false;
  }
  event->axis_x = (int32_t)value;
  if (!node_int(y_node, -128, 127, &value)) {
    *status_out = T64_INPUT_INVALID_EVENT;
    return false;
  }
  event->axis_y = (int32_t)value;
  if (have_previous && tick < previous_tick) {
    *status_out = T64_INPUT_TIME_REVERSED;
    return false;
  }
  *status_out = T64_INPUT_OK;
  return true;
}

static size_t u64_decimal(uint64_t value, char output[20]) {
  char reverse[20];
  size_t count = 0u;
  size_t i;

  do {
    reverse[count++] = (char)('0' + (int)(value % UINT64_C(10)));
    value /= UINT64_C(10);
  } while (value != 0u);
  for (i = 0u; i < count; i++) output[i] = reverse[count - i - 1u];
  return count;
}

static bool write_canonical(t64_input_schedule *schedule, size_t input_length) {
  t64_jsonw writer;
  size_t capacity = input_length + 2u;
  size_t output_length = 0u;
  size_t i;

  schedule->canonical_json = (uint8_t *)malloc(capacity);
  if (schedule->canonical_json == NULL) return false;
  t64_jsonw_init(&writer, (char *)schedule->canonical_json, capacity);
  if (t64_jsonw_begin_object(&writer) != T64_JSONW_OK ||
      t64_jsonw_key(&writer, "events", 6u) != T64_JSONW_OK ||
      t64_jsonw_begin_array(&writer) != T64_JSONW_OK) {
    return false;
  }
  for (i = 0u; i < schedule->event_count; i++) {
    const t64_input_event *event = &schedule->events[i];
    char tick_text[20];
    size_t tick_length = u64_decimal(event->tick, tick_text);
    if (t64_jsonw_begin_object(&writer) != T64_JSONW_OK ||
        t64_jsonw_key(&writer, "buttons", 7u) != T64_JSONW_OK ||
        t64_jsonw_int(&writer, (int64_t)event->buttons) != T64_JSONW_OK ||
        t64_jsonw_key(&writer, "port", 4u) != T64_JSONW_OK ||
        t64_jsonw_int(&writer, (int64_t)event->port) != T64_JSONW_OK ||
        t64_jsonw_key(&writer, "tick", 4u) != T64_JSONW_OK ||
        t64_jsonw_string(&writer, tick_text, tick_length) != T64_JSONW_OK ||
        t64_jsonw_key(&writer, "x", 1u) != T64_JSONW_OK ||
        t64_jsonw_int(&writer, (int64_t)event->axis_x) != T64_JSONW_OK ||
        t64_jsonw_key(&writer, "y", 1u) != T64_JSONW_OK ||
        t64_jsonw_int(&writer, (int64_t)event->axis_y) != T64_JSONW_OK ||
        t64_jsonw_end_object(&writer) != T64_JSONW_OK) {
      return false;
    }
  }
  if (t64_jsonw_end_array(&writer) != T64_JSONW_OK ||
      t64_jsonw_key(&writer, "schema", 6u) != T64_JSONW_OK ||
      t64_jsonw_string(&writer, "t64.input", 9u) != T64_JSONW_OK ||
      t64_jsonw_key(&writer, "schema_version", 14u) != T64_JSONW_OK ||
      t64_jsonw_int(&writer, T64_INPUT_SCHEMA_VERSION) != T64_JSONW_OK ||
      t64_jsonw_end_object(&writer) != T64_JSONW_OK ||
      t64_jsonw_finish(&writer, &output_length) != T64_JSONW_OK) {
    return false;
  }
  schedule->canonical_length = output_length;
  {
    t64_sha256_ctx context;
    t64_sha256_init(&context);
    t64_sha256_update(&context, schedule->canonical_json, output_length);
    t64_sha256_final(&context, schedule->sha256);
  }
  return true;
}

t64_input_status t64_input_parse(const uint8_t *bytes, size_t length,
                                 t64_input_schedule **schedule_out,
                                 t64_input_error *error_out) {
  static const char *const root_keys[] = {"schema", "schema_version", "events"};
  t64_json_limits limits = {T64_INPUT_MAX_BYTES, 3u, 64u, 1024u};
  t64_json_doc document = {0};
  t64_json_error json_error = {T64_JSON_OK, 0u};
  const t64_json_node *schema_node;
  const t64_json_node *version_node;
  const t64_json_node *events_node;
  t64_input_event parsed_events[T64_INPUT_MAX_EVENTS];
  t64_input_schedule *schedule;
  t64_input_status status = T64_INPUT_OK;
  uint64_t previous_tick = 0u;
  size_t i = 0u;

  if (schedule_out == NULL || bytes == NULL) {
    set_error(error_out, T64_INPUT_INVALID_ARGUMENT, SIZE_MAX);
    return T64_INPUT_INVALID_ARGUMENT;
  }
  if (length > T64_INPUT_MAX_BYTES) {
    set_error(error_out, T64_INPUT_SIZE_LIMIT, SIZE_MAX);
    return T64_INPUT_SIZE_LIMIT;
  }
  if (t64_json_parse(bytes, length, &limits, &document, &json_error) != 0) {
    set_error(error_out, T64_INPUT_INVALID_JSON, json_error.offset);
    return T64_INPUT_INVALID_JSON;
  }
  if (!closed_keys(document.root, root_keys,
                   sizeof root_keys / sizeof root_keys[0])) {
    status = T64_INPUT_INVALID_SCHEMA;
    goto cleanup;
  }
  schema_node = member(document.root, "schema", 6u);
  version_node = member(document.root, "schema_version", 14u);
  events_node = member(document.root, "events", 6u);
  if (schema_node == NULL || schema_node->type != T64_JSON_STRING ||
      schema_node->len != 9u ||
      memcmp(schema_node->str, "t64.input", 9u) != 0 ||
      version_node == NULL || version_node->type != T64_JSON_INT ||
      version_node->num != (int64_t)T64_INPUT_SCHEMA_VERSION ||
      events_node == NULL || events_node->type != T64_JSON_ARRAY) {
    status = T64_INPUT_INVALID_SCHEMA;
    goto cleanup;
  }
  if (events_node->count > T64_INPUT_MAX_EVENTS) {
    status = T64_INPUT_EVENT_LIMIT;
    goto cleanup;
  }
  {
    const t64_json_node *node;
    for (node = events_node->first; node != NULL; node = node->next) {
      if (i >= T64_INPUT_MAX_EVENTS ||
          !parse_event(node, &parsed_events[i], previous_tick, i > 0u,
                       &status)) {
        goto cleanup;
      }
      previous_tick = parsed_events[i].tick;
      i++;
    }
  }

  schedule = (t64_input_schedule *)calloc(1u, sizeof *schedule);
  if (schedule == NULL) {
    status = T64_INPUT_ALLOCATION_FAILED;
    goto cleanup;
  }
  schedule->event_count = i;
  if (i > 0u) {
    (void)memcpy(schedule->events, parsed_events, i * sizeof parsed_events[0]);
  }
  if (!write_canonical(schedule, length)) {
    t64_input_schedule_destroy(schedule);
    status = T64_INPUT_CANONICALIZATION_FAILED;
    goto cleanup;
  }
  *schedule_out = schedule;
  set_error(error_out, T64_INPUT_OK, SIZE_MAX);
  t64_json_free(&document);
  return T64_INPUT_OK;

cleanup:
  t64_json_free(&document);
  set_error(error_out, status, SIZE_MAX);
  return status;
}

void t64_input_schedule_destroy(t64_input_schedule *schedule) {
  if (schedule != NULL) {
    free(schedule->canonical_json);
    free(schedule);
  }
}

size_t t64_input_schedule_event_count(const t64_input_schedule *schedule) {
  return schedule == NULL ? 0u : schedule->event_count;
}

bool t64_input_schedule_event(const t64_input_schedule *schedule,
                              size_t index, t64_input_event *event_out) {
  if (schedule == NULL || event_out == NULL || index >= schedule->event_count) {
    return false;
  }
  *event_out = schedule->events[index];
  return true;
}

const uint8_t *t64_input_schedule_canonical_json(
    const t64_input_schedule *schedule, size_t *length_out) {
  if (schedule == NULL || length_out == NULL) return NULL;
  *length_out = schedule->canonical_length;
  return schedule->canonical_json;
}

const uint8_t *t64_input_schedule_sha256(
    const t64_input_schedule *schedule) {
  return schedule == NULL ? NULL : schedule->sha256;
}
