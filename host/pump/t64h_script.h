/*
 * The scripted input source of the host pump (Phase 12, plan 12-08, API-05, decision
 * DR-13): a strict reader of the t64.input v1 subset and a source that queues the
 * events of a script into a machine lazily, before the advance that contains them.
 *
 * Pure C99 on the installed package: this file includes only <tendies64/t64_machine.h>
 * and C99 headers. The reader never opens a file: the caller (the test binary or the
 * shell) loads the bytes with a bounded read and passes them in. The runner's own
 * parser sits outside the include guard of host/, so this is a small strict copy of its
 * rules, not a shared module.
 *
 * Grammar (one JSON document, whitespace between tokens only, no escape sequences):
 *   {"events":[{"buttons":N,"port":0,"tick":"D","x":N,"y":N},...],
 *    "schema":"t64.input","schema_version":1}
 * The key order is free; the key sets are closed and every key is required once.
 * tick is a canonical decimal string (no sign, no leading zero, at most 2^64 - 1),
 * port is the integer 0, buttons is 0..65535, x and y are -128..127, ticks never
 * decrease along the array. The document is at most 65,536 bytes with at most 4,096
 * events. The first violation ends the read and names its rule.
 */
#ifndef T64H_SCRIPT_H
#define T64H_SCRIPT_H

#include <stddef.h>
#include <stdint.h>

#include <tendies64/t64_machine.h>

#define T64H_SCRIPT_MAX_BYTES 65536u
#define T64H_SCRIPT_MAX_EVENTS 4096u

typedef struct {
  uint64_t tick;
  uint16_t buttons;
  int32_t x;
  int32_t y;
} t64h_script_event;

/* A parsed script: the events in file order (port 0 only, ticks never decreasing). */
typedef struct {
  size_t count;
  t64h_script_event events[T64H_SCRIPT_MAX_EVENTS];
} t64h_script;

/* Each refusal has one stable rule name (t64h_script_status_name). */
typedef enum {
  T64H_SCRIPT_OK = 0,
  T64H_SCRIPT_BAD_ARGUMENT,   /* "bad-argument": a NULL bytes or output pointer */
  T64H_SCRIPT_SIZE,           /* "size": more bytes than the limit */
  T64H_SCRIPT_SYNTAX,         /* "syntax": not the JSON subset above */
  T64H_SCRIPT_UNKNOWN_KEY,    /* "unknown-key": a key outside the closed set */
  T64H_SCRIPT_MISSING_KEY,    /* "missing-key": a required key is absent */
  T64H_SCRIPT_DUPLICATE_KEY,  /* "duplicate-key": a key twice in one object */
  T64H_SCRIPT_SCHEMA,         /* "schema": schema is not the string t64.input */
  T64H_SCRIPT_VERSION,        /* "schema-version": schema_version is not the integer 1 */
  T64H_SCRIPT_EVENT_LIMIT,    /* "event-limit": more events than the limit */
  T64H_SCRIPT_TICK_TYPE,      /* "tick-type": tick is not a string */
  T64H_SCRIPT_TICK_FORMAT,    /* "tick-format": not a canonical decimal u64 */
  T64H_SCRIPT_PORT,           /* "port": not the integer 0 */
  T64H_SCRIPT_BUTTONS,        /* "buttons": not an integer in 0..65535 */
  T64H_SCRIPT_AXIS_X,         /* "axis-x": not an integer in -128..127 */
  T64H_SCRIPT_AXIS_Y,         /* "axis-y": not an integer in -128..127 */
  T64H_SCRIPT_TIME_REVERSED,  /* "time-reversed": a tick below the one before it */
  T64H_SCRIPT_TRAILING        /* "trailing-data": bytes after the document */
} t64h_script_status;

typedef struct {
  t64h_script_status status;
  size_t offset;  /* the byte offset of the violation (the length for the size rule) */
} t64h_script_error;

/* The limits of one read. The defaults are the two macros above; the cap refusals are
 * reachable through lowered or raised limits on the same code path (a minimal event is
 * 46 bytes, so 4,097 events never fit the default byte limit). */
typedef struct {
  size_t max_bytes;
  size_t max_events;
} t64h_script_limits;

#define T64H_SCRIPT_LIMITS_DEFAULT {T64H_SCRIPT_MAX_BYTES, T64H_SCRIPT_MAX_EVENTS}

/* The stable rule name of a status ("ok" for T64H_SCRIPT_OK). */
const char *t64h_script_status_name(t64h_script_status status);

/* Read one document. limits may be NULL (the defaults); max_events above
 * T64H_SCRIPT_MAX_EVENTS is capped to it (the script holds no more). On a refusal the
 * script is left empty (count 0) and error, when not NULL, names the rule and the
 * offset; error is also filled on success (OK, offset 0). */
t64h_script_status t64h_script_parse(const uint8_t *bytes, size_t length,
                                     const t64h_script_limits *limits, t64h_script *script,
                                     t64h_script_error *error);

/* How a source hands one event to the machine (port 0). The default is
 * t64_machine_queue_input; a test substitutes a model of a small queue. */
typedef t64_status (*t64h_queue_fn)(void *context, t64_machine *machine, uint64_t tick,
                                    uint16_t buttons, int32_t x, int32_t y);

typedef struct {
  const t64h_script *script;
  size_t next;               /* the first event not queued yet */
  t64h_queue_fn queue;       /* NULL: t64_machine_queue_input */
  void *queue_context;
  uint64_t calls;            /* calls of t64h_script_feed */
  uint64_t queued;           /* events the machine accepted */
  uint64_t deferred;         /* QUEUE_FULL answers: each one defers the event to a later call */
  t64_status refusal;        /* the status behind a failed feed, else T64_STATUS_OK */
} t64h_script_source;

/* Bind a source to a script that outlives it. queue may be NULL. */
void t64h_script_source_init(t64h_script_source *source, const t64h_script *script,
                             t64h_queue_fn queue, void *queue_context);

/* The t64h_input_fn of the pump (context is the t64h_script_source): queue, in file
 * order, every event whose tick is at most *bound and that the machine has not got yet.
 * A QUEUE_FULL answer defers that event and every later one: *bound is lowered to the
 * tick of the deferred event, so the guest stops there instead of passing it, and the
 * event is tried again by the next call. A deferred event whose tick is already at or
 * below the guest tick can never be queued in time (the machine would answer
 * TIME_REVERSED) and any other refusal ends the feed: both return 1 with the status in
 * source->refusal. Returns 0 otherwise. */
int t64h_script_feed(void *context, t64_machine *machine, uint64_t guest_tick,
                     uint64_t *bound);

#endif /* T64H_SCRIPT_H */
