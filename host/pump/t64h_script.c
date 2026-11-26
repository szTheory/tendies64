/*
 * The scripted input source of the host pump (Phase 12, plan 12-08, API-05, DR-13).
 * See t64h_script.h for the grammar and the contract. The reader is shape-driven (the
 * document has one fixed shape, so there is no general tree): it walks the bytes once,
 * names the first violation and never allocates.
 */
#include "t64h_script.h"

#include <string.h>

const char *t64h_script_status_name(t64h_script_status status) {
  switch (status) {
    case T64H_SCRIPT_OK: return "ok";
    case T64H_SCRIPT_BAD_ARGUMENT: return "bad-argument";
    case T64H_SCRIPT_SIZE: return "size";
    case T64H_SCRIPT_SYNTAX: return "syntax";
    case T64H_SCRIPT_UNKNOWN_KEY: return "unknown-key";
    case T64H_SCRIPT_MISSING_KEY: return "missing-key";
    case T64H_SCRIPT_DUPLICATE_KEY: return "duplicate-key";
    case T64H_SCRIPT_SCHEMA: return "schema";
    case T64H_SCRIPT_VERSION: return "schema-version";
    case T64H_SCRIPT_EVENT_LIMIT: return "event-limit";
    case T64H_SCRIPT_TICK_TYPE: return "tick-type";
    case T64H_SCRIPT_TICK_FORMAT: return "tick-format";
    case T64H_SCRIPT_PORT: return "port";
    case T64H_SCRIPT_BUTTONS: return "buttons";
    case T64H_SCRIPT_AXIS_X: return "axis-x";
    case T64H_SCRIPT_AXIS_Y: return "axis-y";
    case T64H_SCRIPT_TIME_REVERSED: return "time-reversed";
    case T64H_SCRIPT_TRAILING: return "trailing-data";
    default: return "unknown";
  }
}

/* ---- the reader -------------------------------------------------------------- */

typedef struct {
  const uint8_t *bytes;
  size_t length;
  size_t pos;
  t64h_script_status status;  /* the first violation, OK while none */
  size_t offset;
} reader;

/* Record the first violation and answer 0, so a caller can `return fail(...)`. */
static int fail(reader *r, t64h_script_status status, size_t offset) {
  if (r->status == T64H_SCRIPT_OK) {
    r->status = status;
    r->offset = offset;
  }
  return 0;
}

static void skip_space(reader *r) {
  while (r->pos < r->length &&
         (r->bytes[r->pos] == 0x20u || r->bytes[r->pos] == 0x09u ||
          r->bytes[r->pos] == 0x0au || r->bytes[r->pos] == 0x0du)) {
    r->pos++;
  }
}

static int peek_is(const reader *r, uint8_t byte) {
  return r->pos < r->length && r->bytes[r->pos] == byte;
}

static int expect(reader *r, uint8_t byte) {
  skip_space(r);
  if (!peek_is(r, byte)) {
    return fail(r, T64H_SCRIPT_SYNTAX, r->pos);
  }
  r->pos++;
  return 1;
}

/* A string without escapes: printable ASCII only, no backslash. The span excludes the
 * quotes; the reader stands after the closing quote. */
static int read_string(reader *r, size_t *start_out, size_t *length_out) {
  size_t start;

  if (!peek_is(r, (uint8_t)'"')) {
    return fail(r, T64H_SCRIPT_SYNTAX, r->pos);
  }
  r->pos++;
  start = r->pos;
  while (r->pos < r->length && r->bytes[r->pos] != (uint8_t)'"') {
    const uint8_t byte = r->bytes[r->pos];

    if (byte < 0x20u || byte > 0x7eu || byte == (uint8_t)'\\') {
      return fail(r, T64H_SCRIPT_SYNTAX, r->pos);
    }
    r->pos++;
  }
  if (!peek_is(r, (uint8_t)'"')) {
    return fail(r, T64H_SCRIPT_SYNTAX, r->pos);
  }
  *start_out = start;
  *length_out = r->pos - start;
  r->pos++;
  return 1;
}

typedef enum { VALUE_STRING, VALUE_INT, VALUE_OTHER } value_kind;

typedef struct {
  value_kind kind;
  size_t start;      /* the string content */
  size_t length;
  int64_t number;    /* VALUE_INT: saturated at +-10^18, so a range rule still refuses it */
} value;

static int is_digit(uint8_t byte) {
  return byte >= (uint8_t)'0' && byte <= (uint8_t)'9';
}

static int match_literal(reader *r, const char *word) {
  const size_t n = strlen(word);

  if (r->length - r->pos < n || memcmp(r->bytes + r->pos, word, n) != 0) {
    return fail(r, T64H_SCRIPT_SYNTAX, r->pos);
  }
  r->pos += n;
  return 1;
}

/* One scalar value of a known key: a string, an integer, or something else. A nested
 * object or array is VALUE_OTHER and is not entered (the caller refuses it at once, so
 * the reader never walks into it). */
static int read_value(reader *r, value *out) {
  skip_space(r);
  if (r->pos >= r->length) {
    return fail(r, T64H_SCRIPT_SYNTAX, r->pos);
  }
  out->kind = VALUE_OTHER;
  out->start = 0u;
  out->length = 0u;
  out->number = 0;
  if (r->bytes[r->pos] == (uint8_t)'"') {
    out->kind = VALUE_STRING;
    return read_string(r, &out->start, &out->length);
  }
  if (r->bytes[r->pos] == (uint8_t)'-' || is_digit(r->bytes[r->pos])) {
    int negative = 0;
    int64_t magnitude = 0;

    if (r->bytes[r->pos] == (uint8_t)'-') {
      negative = 1;
      r->pos++;
    }
    if (r->pos >= r->length || !is_digit(r->bytes[r->pos])) {
      return fail(r, T64H_SCRIPT_SYNTAX, r->pos);
    }
    if (r->bytes[r->pos] == (uint8_t)'0' && r->pos + 1u < r->length &&
        is_digit(r->bytes[r->pos + 1u])) {
      return fail(r, T64H_SCRIPT_SYNTAX, r->pos);
    }
    while (r->pos < r->length && is_digit(r->bytes[r->pos])) {
      if (magnitude < INT64_C(1000000000000000000)) {
        magnitude = magnitude * 10 + (int64_t)(r->bytes[r->pos] - (uint8_t)'0');
      }
      r->pos++;
    }
    if (r->pos < r->length && (r->bytes[r->pos] == (uint8_t)'.' || r->bytes[r->pos] == (uint8_t)'e' ||
                               r->bytes[r->pos] == (uint8_t)'E')) {
      /* A fraction or an exponent: a number, but not an integer. */
      while (r->pos < r->length &&
             (is_digit(r->bytes[r->pos]) || r->bytes[r->pos] == (uint8_t)'.' ||
              r->bytes[r->pos] == (uint8_t)'e' || r->bytes[r->pos] == (uint8_t)'E' ||
              r->bytes[r->pos] == (uint8_t)'+' || r->bytes[r->pos] == (uint8_t)'-')) {
        r->pos++;
      }
      return 1;
    }
    out->kind = VALUE_INT;
    out->number = negative ? -magnitude : magnitude;
    return 1;
  }
  if (r->bytes[r->pos] == (uint8_t)'t') {
    return match_literal(r, "true");
  }
  if (r->bytes[r->pos] == (uint8_t)'f') {
    return match_literal(r, "false");
  }
  if (r->bytes[r->pos] == (uint8_t)'n') {
    return match_literal(r, "null");
  }
  if (r->bytes[r->pos] == (uint8_t)'{' || r->bytes[r->pos] == (uint8_t)'[') {
    return 1;
  }
  return fail(r, T64H_SCRIPT_SYNTAX, r->pos);
}

/* A canonical decimal u64: 1 to 20 digits, no sign, no leading zero, no overflow. */
static int canonical_u64(const uint8_t *text, size_t length, uint64_t *out) {
  uint64_t v = 0u;
  size_t i;

  if (length == 0u || length > 20u || (text[0] == (uint8_t)'0' && length > 1u)) {
    return 0;
  }
  for (i = 0u; i < length; i++) {
    uint64_t digit;

    if (!is_digit(text[i])) {
      return 0;
    }
    digit = (uint64_t)(text[i] - (uint8_t)'0');
    if (v > (UINT64_MAX - digit) / 10u) {
      return 0;
    }
    v = v * 10u + digit;
  }
  *out = v;
  return 1;
}

static int key_is(const reader *r, size_t start, size_t length, const char *name) {
  return strlen(name) == length && memcmp(r->bytes + start, name, length) == 0;
}

enum { EV_TICK = 0, EV_PORT, EV_BUTTONS, EV_X, EV_Y, EV_KEYS };

static int int_in(const value *v, int64_t low, int64_t high) {
  return v->kind == VALUE_INT && v->number >= low && v->number <= high;
}

/* One event object. previous is the tick of the event before it (have_previous 0 for the
 * first). */
static int read_event(reader *r, t64h_script_event *event, int have_previous,
                      uint64_t previous) {
  static const char *const names[EV_KEYS] = {"tick", "port", "buttons", "x", "y"};
  int seen[EV_KEYS] = {0, 0, 0, 0, 0};
  size_t found = 0u;
  size_t i;

  if (!expect(r, (uint8_t)'{')) {
    return 0;
  }
  skip_space(r);
  if (peek_is(r, (uint8_t)'}')) {
    return fail(r, T64H_SCRIPT_MISSING_KEY, r->pos);
  }
  for (;;) {
    size_t key_start = 0u;
    size_t key_length = 0u;
    size_t key_offset;
    size_t key = EV_KEYS;
    value v;

    skip_space(r);
    key_offset = r->pos;
    if (!read_string(r, &key_start, &key_length)) {
      return 0;
    }
    for (i = 0u; i < (size_t)EV_KEYS; i++) {
      if (key_is(r, key_start, key_length, names[i])) {
        key = i;
      }
    }
    if (key == (size_t)EV_KEYS) {
      return fail(r, T64H_SCRIPT_UNKNOWN_KEY, key_offset);
    }
    if (seen[key]) {
      return fail(r, T64H_SCRIPT_DUPLICATE_KEY, key_offset);
    }
    seen[key] = 1;
    found++;
    if (!expect(r, (uint8_t)':') || !read_value(r, &v)) {
      return 0;
    }
    switch (key) {
      case EV_TICK:
        if (v.kind != VALUE_STRING) {
          return fail(r, T64H_SCRIPT_TICK_TYPE, r->pos);
        }
        if (!canonical_u64(r->bytes + v.start, v.length, &event->tick)) {
          return fail(r, T64H_SCRIPT_TICK_FORMAT, v.start);
        }
        break;
      case EV_PORT:
        if (!int_in(&v, 0, 0)) {
          return fail(r, T64H_SCRIPT_PORT, r->pos);
        }
        break;
      case EV_BUTTONS:
        if (!int_in(&v, 0, 65535)) {
          return fail(r, T64H_SCRIPT_BUTTONS, r->pos);
        }
        event->buttons = (uint16_t)v.number;
        break;
      case EV_X:
        if (!int_in(&v, -128, 127)) {
          return fail(r, T64H_SCRIPT_AXIS_X, r->pos);
        }
        event->x = (int32_t)v.number;
        break;
      default:
        if (!int_in(&v, -128, 127)) {
          return fail(r, T64H_SCRIPT_AXIS_Y, r->pos);
        }
        event->y = (int32_t)v.number;
        break;
    }
    skip_space(r);
    if (peek_is(r, (uint8_t)',')) {
      r->pos++;
      continue;
    }
    if (peek_is(r, (uint8_t)'}')) {
      r->pos++;
      break;
    }
    return fail(r, T64H_SCRIPT_SYNTAX, r->pos);
  }
  if (found != (size_t)EV_KEYS) {
    return fail(r, T64H_SCRIPT_MISSING_KEY, r->pos);
  }
  if (have_previous && event->tick < previous) {
    return fail(r, T64H_SCRIPT_TIME_REVERSED, r->pos);
  }
  return 1;
}

static int read_events(reader *r, t64h_script *script, size_t max_events) {
  if (!expect(r, (uint8_t)'[')) {
    return 0;
  }
  skip_space(r);
  if (peek_is(r, (uint8_t)']')) {
    r->pos++;
    return 1;
  }
  for (;;) {
    t64h_script_event event = {0u, 0u, 0, 0};

    skip_space(r);
    if (script->count >= max_events) {
      return fail(r, T64H_SCRIPT_EVENT_LIMIT, r->pos);
    }
    if (!read_event(r, &event, script->count > 0u,
                    script->count > 0u ? script->events[script->count - 1u].tick : 0u)) {
      return 0;
    }
    script->events[script->count++] = event;
    skip_space(r);
    if (peek_is(r, (uint8_t)',')) {
      r->pos++;
      continue;
    }
    if (peek_is(r, (uint8_t)']')) {
      r->pos++;
      return 1;
    }
    return fail(r, T64H_SCRIPT_SYNTAX, r->pos);
  }
}

enum { ROOT_SCHEMA = 0, ROOT_VERSION, ROOT_EVENTS, ROOT_KEYS };

static int read_root(reader *r, t64h_script *script, size_t max_events) {
  static const char *const names[ROOT_KEYS] = {"schema", "schema_version", "events"};
  int seen[ROOT_KEYS] = {0, 0, 0};
  size_t i;

  if (!expect(r, (uint8_t)'{')) {
    return 0;
  }
  skip_space(r);
  if (peek_is(r, (uint8_t)'}')) {
    return fail(r, T64H_SCRIPT_MISSING_KEY, r->pos);
  }
  for (;;) {
    size_t key_start = 0u;
    size_t key_length = 0u;
    size_t key_offset;
    size_t key = ROOT_KEYS;

    skip_space(r);
    key_offset = r->pos;
    if (!read_string(r, &key_start, &key_length)) {
      return 0;
    }
    for (i = 0u; i < (size_t)ROOT_KEYS; i++) {
      if (key_is(r, key_start, key_length, names[i])) {
        key = i;
      }
    }
    if (key == (size_t)ROOT_KEYS) {
      return fail(r, T64H_SCRIPT_UNKNOWN_KEY, key_offset);
    }
    if (seen[key]) {
      return fail(r, T64H_SCRIPT_DUPLICATE_KEY, key_offset);
    }
    seen[key] = 1;
    if (!expect(r, (uint8_t)':')) {
      return 0;
    }
    if (key == (size_t)ROOT_EVENTS) {
      if (!read_events(r, script, max_events)) {
        return 0;
      }
    } else {
      value v;

      if (!read_value(r, &v)) {
        return 0;
      }
      if (key == (size_t)ROOT_SCHEMA) {
        if (v.kind != VALUE_STRING || !key_is(r, v.start, v.length, "t64.input")) {
          return fail(r, T64H_SCRIPT_SCHEMA, r->pos);
        }
      } else if (!int_in(&v, 1, 1)) {
        return fail(r, T64H_SCRIPT_VERSION, r->pos);
      }
    }
    skip_space(r);
    if (peek_is(r, (uint8_t)',')) {
      r->pos++;
      continue;
    }
    if (peek_is(r, (uint8_t)'}')) {
      r->pos++;
      break;
    }
    return fail(r, T64H_SCRIPT_SYNTAX, r->pos);
  }
  for (i = 0u; i < (size_t)ROOT_KEYS; i++) {
    if (!seen[i]) {
      return fail(r, T64H_SCRIPT_MISSING_KEY, r->pos);
    }
  }
  skip_space(r);
  if (r->pos != r->length) {
    return fail(r, T64H_SCRIPT_TRAILING, r->pos);
  }
  return 1;
}

t64h_script_status t64h_script_parse(const uint8_t *bytes, size_t length,
                                     const t64h_script_limits *limits, t64h_script *script,
                                     t64h_script_error *error) {
  reader r;
  size_t max_bytes = T64H_SCRIPT_MAX_BYTES;
  size_t max_events = T64H_SCRIPT_MAX_EVENTS;

  if (error != NULL) {
    error->status = T64H_SCRIPT_OK;
    error->offset = 0u;
  }
  if (bytes == NULL || script == NULL) {
    if (error != NULL) {
      error->status = T64H_SCRIPT_BAD_ARGUMENT;
    }
    return T64H_SCRIPT_BAD_ARGUMENT;
  }
  script->count = 0u;
  if (limits != NULL) {
    max_bytes = limits->max_bytes;
    max_events = limits->max_events;
  }
  if (max_events > T64H_SCRIPT_MAX_EVENTS) {
    max_events = T64H_SCRIPT_MAX_EVENTS;
  }
  r.bytes = bytes;
  r.length = length;
  r.pos = 0u;
  r.status = T64H_SCRIPT_OK;
  r.offset = 0u;
  if (length > max_bytes) {
    (void)fail(&r, T64H_SCRIPT_SIZE, length);
  } else {
    (void)read_root(&r, script, max_events);
  }
  if (r.status != T64H_SCRIPT_OK) {
    script->count = 0u;
    if (error != NULL) {
      error->status = r.status;
      error->offset = r.offset;
    }
  }
  return r.status;
}

/* ---- the scripted source ------------------------------------------------------- */

static t64_status default_queue(void *context, t64_machine *machine, uint64_t tick,
                                uint16_t buttons, int32_t x, int32_t y) {
  (void)context;
  return t64_machine_queue_input(machine, tick, 0u, buttons, x, y);
}

void t64h_script_source_init(t64h_script_source *source, const t64h_script *script,
                             t64h_queue_fn queue, void *queue_context) {
  *source = (t64h_script_source){0};
  source->script = script;
  source->queue = queue != NULL ? queue : default_queue;
  source->queue_context = queue_context;
  source->refusal = T64_STATUS_OK;
}

int t64h_script_feed(void *context, t64_machine *machine, uint64_t guest_tick,
                     uint64_t *bound) {
  t64h_script_source *source = (t64h_script_source *)context;

  if (source == NULL || source->script == NULL || source->queue == NULL || bound == NULL) {
    return 1;
  }
  source->calls++;
  while (source->next < source->script->count) {
    const t64h_script_event *event = &source->script->events[source->next];
    t64_status status;

    if (event->tick > *bound) {
      break;
    }
    status = source->queue(source->queue_context, machine, event->tick, event->buttons,
                           event->x, event->y);
    if (status == T64_STATUS_OK) {
      source->next++;
      source->queued++;
      continue;
    }
    if (status == T64_STATUS_QUEUE_FULL) {
      source->deferred++;
      if (event->tick <= guest_tick) {
        source->refusal = status;
        return 1;
      }
      *bound = event->tick;
      return 0;
    }
    source->refusal = status;
    return 1;
  }
  return 0;
}
