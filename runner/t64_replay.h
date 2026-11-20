/* Phase 9 input replay (plan 09-07, D-34 and D-35): the closed bounded
 * t64.replay v1 operation log, the recorder that wraps the real accepted input
 * calls, identity binding, the stepwise player and in-process driver, and the
 * closed t64.replay-report v1 record. Runner side only: the core knows nothing
 * of replays. No API or ABI freeze is implied; names, keys and caps are
 * provisional until Phase 10.
 *
 * Boundaries owned here. Input: the replay bytes are untrusted and parsed with
 * explicit caps before any allocation. Time: ops carry guest ticks (the tick at
 * which the host submits the call, `at`, and the tick the event is stamped
 * with, `event_tick`); the module never reads a host clock. Output: canonical
 * bytes and a report; publication goes through the private single-file
 * publisher and never writes a host path, time or user name. Errors: every
 * refusal is a named status; the module never aborts. */
#ifndef T64_REPLAY_H
#define T64_REPLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tendies64/t64_machine.h"

#define T64_REPLAY_SCHEMA_VERSION 1u
#define T64_REPLAY_REPORT_SCHEMA_VERSION 1u
#define T64_REPLAY_FORMAT "t64.replay/1"
#define T64_REPLAY_REPORT_FORMAT "t64.replay-report/1"
#define T64_REPLAY_MAX_BYTES 4194304u
#define T64_REPLAY_MAX_OPS 65536u
#define T64_REPLAY_MAX_CHECKPOINTS 4096u
#define T64_REPLAY_REPORT_MAX_BYTES 4096u
#define T64_REPLAY_PORT 0u
/* The smallest checkpoint interval and the granularity of the runner slices. */
#define T64_REPLAY_MIN_INTERVAL 16u
#define T64_REPLAY_HEX_SIZE 65u
#define T64_REPLAY_TEXT_CAPACITY 96u

typedef enum {
  T64_REPLAY_OK = 0,
  T64_REPLAY_INVALID_ARGUMENT,
  T64_REPLAY_SIZE_LIMIT,
  T64_REPLAY_INVALID_JSON,
  T64_REPLAY_INVALID_SCHEMA,
  T64_REPLAY_INVALID_HEADER,
  T64_REPLAY_BOOT_UNSUPPORTED,
  T64_REPLAY_OP_LIMIT,
  T64_REPLAY_CHECKPOINT_LIMIT,
  T64_REPLAY_INVALID_OP,
  T64_REPLAY_AT_REVERSED,
  T64_REPLAY_EVENT_BEFORE_AT,
  T64_REPLAY_END_MISSING,
  T64_REPLAY_END_NOT_LAST,
  T64_REPLAY_ALLOCATION_FAILED,
  T64_REPLAY_CANONICALIZATION_FAILED
} t64_replay_status;

/* offset is a byte offset for T64_REPLAY_INVALID_JSON, an op index for the op
 * statuses and SIZE_MAX otherwise. */
typedef struct {
  t64_replay_status status;
  size_t offset;
} t64_replay_error;

typedef struct {
  size_t max_bytes;
  size_t max_ops;         /* including the end op */
  size_t max_checkpoints;
} t64_replay_limits;

typedef enum {
  T64_REPLAY_OP_INPUT = 1,
  T64_REPLAY_OP_CHECKPOINT = 2,
  T64_REPLAY_OP_END = 3
} t64_replay_op_kind;

typedef enum {
  T64_REPLAY_STOP_NONE = 0,
  T64_REPLAY_STOP_GUEST_BOUND = 1,
  T64_REPLAY_STOP_GUEST_COMPLETION = 2
} t64_replay_stop;

typedef struct {
  t64_replay_op_kind kind;
  uint64_t at;
  uint64_t event_tick;          /* input */
  uint32_t port;                /* input, always T64_REPLAY_PORT */
  uint16_t buttons;             /* input */
  int32_t axis_x;               /* input */
  int32_t axis_y;               /* input */
  uint8_t digest[T64_DIGEST_SIZE]; /* checkpoint and end */
  t64_replay_stop stop;         /* end */
} t64_replay_op;

typedef enum {
  T64_REPLAY_SAVE_BLANK = 1,
  T64_REPLAY_SAVE_IMAGE = 2
} t64_replay_save_kind;

/* What a replay binds to and what the live machine reports: profile id and
 * digest, the three versions, build id, content SHA-256 and size, and the save
 * content at the start of the run. */
typedef struct {
  char profile_id[T64_REPLAY_TEXT_CAPACITY];
  char profile_digest[T64_REPLAY_HEX_SIZE];
  uint32_t descriptor_version;
  uint32_t machine_model_version;
  uint32_t state_serialization_version;
  char build_id[T64_REPLAY_TEXT_CAPACITY];
  char content_sha256[T64_REPLAY_HEX_SIZE];
  uint64_t content_size;
  t64_replay_save_kind save_kind;
  uint64_t save_length;                /* image only */
  char save_sha256[T64_REPLAY_HEX_SIZE]; /* image only */
} t64_replay_live_identity;

typedef struct {
  t64_replay_live_identity identity;
  char source_revision[41]; /* informational, not bound */
} t64_replay_header;

typedef struct t64_replay t64_replay;

const char *t64_replay_status_name(t64_replay_status status);

/* Parses one complete t64.replay v1 document. On success *replay_out is a new
 * owned replay; on refusal it is untouched. The default limits are
 * {T64_REPLAY_MAX_BYTES, T64_REPLAY_MAX_OPS, T64_REPLAY_MAX_CHECKPOINTS}. */
t64_replay_status t64_replay_parse(const uint8_t *bytes, size_t length,
                                   t64_replay **replay_out,
                                   t64_replay_error *error_out);
t64_replay_status t64_replay_parse_limits(const uint8_t *bytes, size_t length,
                                          const t64_replay_limits *limits,
                                          t64_replay **replay_out,
                                          t64_replay_error *error_out);
void t64_replay_destroy(t64_replay *replay);
const t64_replay_header *t64_replay_header_get(const t64_replay *replay);
size_t t64_replay_op_count(const t64_replay *replay);
bool t64_replay_op_get(const t64_replay *replay, size_t index,
                       t64_replay_op *op_out);
/* Borrowed until destruction. The identity is the SHA-256 of the canonical
 * bytes. */
const uint8_t *t64_replay_canonical(const t64_replay *replay,
                                    size_t *length_out);
const uint8_t *t64_replay_sha256(const t64_replay *replay);

/* Raw canonical writer: sorted keys, decimal-string u64, lowercase hex. It
 * checks that every field is representable but not that the op list is
 * well-formed (tests use it to build refused documents). *bytes_out is
 * allocated with malloc. */
t64_replay_status t64_replay_encode(const t64_replay_header *header,
                                    const t64_replay_op *ops, size_t op_count,
                                    uint8_t **bytes_out, size_t *length_out);

/* ---- recorder ------------------------------------------------------------- */

typedef struct t64_replay_recorder t64_replay_recorder;

t64_replay_recorder *t64_replay_recorder_create(
    const t64_replay_header *header, const t64_replay_limits *limits);
void t64_replay_recorder_destroy(t64_replay_recorder *recorder);
/* Calls the real t64_machine_queue_input and records the op only when it
 * returns T64_STATUS_OK. An append that would pass a cap is refused before the
 * machine call (T64_STATUS_RESOURCE_EXHAUSTED) and makes the recorder fail. */
t64_status t64_replay_recorder_queue_input(t64_replay_recorder *recorder,
                                           t64_machine *machine, uint64_t at,
                                           uint64_t event_tick, uint32_t port,
                                           uint16_t buttons, int32_t axis_x,
                                           int32_t axis_y);
t64_replay_status t64_replay_recorder_checkpoint(
    t64_replay_recorder *recorder, const t64_machine *machine, uint64_t at);
t64_replay_status t64_replay_recorder_end(t64_replay_recorder *recorder,
                                          const t64_machine *machine,
                                          uint64_t at, t64_replay_stop stop);
/* Sticky: the first failure of any recorder call. */
t64_replay_status t64_replay_recorder_error(
    const t64_replay_recorder *recorder);
/* Encodes the recorded ops and re-parses its own output as a self-check. */
t64_replay_status t64_replay_recorder_finish(t64_replay_recorder *recorder,
                                             t64_replay **replay_out,
                                             t64_replay_error *error_out);

/* ---- identity -------------------------------------------------------------- */

/* Reads the save content of the machine through the save export and classifies
 * it: blank (no device, or the bytes equal the blank pattern of the device) or
 * an image with its SHA-256 and length. False on an unexpected refusal. */
bool t64_replay_live_save_init(const t64_machine *machine,
                               t64_replay_live_identity *identity);
/* The full live identity of an admitted machine. */
bool t64_replay_live_identity_compose(const t64_machine *machine,
                                      t64_replay_live_identity *identity);

typedef enum {
  T64_REPLAY_BIND_OK = 0,
  T64_REPLAY_BIND_PROFILE,
  T64_REPLAY_BIND_BUILD,
  T64_REPLAY_BIND_CONTENT,
  T64_REPLAY_BIND_SAVE_INIT
} t64_replay_bind_status;

/* The closed mismatch field set in check order. */
#define T64_REPLAY_FIELD_NONE "none"
#define T64_REPLAY_FIELD_PROFILE_ID "profile-id"
#define T64_REPLAY_FIELD_PROFILE_DIGEST "profile-digest"
#define T64_REPLAY_FIELD_DESCRIPTOR_VERSION "descriptor-version"
#define T64_REPLAY_FIELD_MACHINE_MODEL_VERSION "machine-model-version"
#define T64_REPLAY_FIELD_STATE_SERIALIZATION_VERSION \
  "state-serialization-version"
#define T64_REPLAY_FIELD_BUILD_ID "build-id"
#define T64_REPLAY_FIELD_CONTENT_SHA256 "content-sha256"
#define T64_REPLAY_FIELD_CONTENT_SIZE "content-size"
#define T64_REPLAY_FIELD_SAVE_INIT "save-init"

/* Compares in the fixed order profile, build, content, save_init and returns
 * the first mismatch with its field name (the status names the family). */
t64_replay_bind_status t64_replay_bind(const t64_replay_header *recorded,
                                       const t64_replay_live_identity *live,
                                       const char **field_out);

/* ---- report ----------------------------------------------------------------- */

typedef enum {
  T64_REPLAY_RESULT_MATCH = 1,
  T64_REPLAY_RESULT_DIVERGED,
  T64_REPLAY_RESULT_IDENTITY_MISMATCH,
  T64_REPLAY_RESULT_REFUSED
} t64_replay_result;

typedef enum {
  T64_REPLAY_DETAIL_NONE = 0,
  T64_REPLAY_DETAIL_ARTIFACT_INVALID,
  T64_REPLAY_DETAIL_INPUT_REFUSED,
  T64_REPLAY_DETAIL_QUEUE_FULL,
  T64_REPLAY_DETAIL_ADVANCE_INVALID,
  T64_REPLAY_DETAIL_STOP_EARLY,
  T64_REPLAY_DETAIL_STOP_KIND,
  T64_REPLAY_DETAIL_DIGEST
} t64_replay_detail;

typedef struct {
  t64_replay_result result;
  t64_replay_detail detail;
  char field[32];
  bool has_actual_digest;
  bool has_expected_digest;
  uint8_t actual_digest[T64_DIGEST_SIZE];
  uint8_t expected_digest[T64_DIGEST_SIZE];
  bool has_at;
  uint64_t at;
  bool has_op_index;
  uint64_t op_index;
  uint64_t checkpoints_matched;
  uint64_t ops_applied;
  uint64_t live_tick;
  bool has_replay_sha256;
  uint8_t replay_sha256[T64_DIGEST_SIZE];
} t64_replay_report;

typedef enum {
  T64_REPLAY_REPORT_OK = 0,
  T64_REPLAY_REPORT_INVALID_ARGUMENT,
  T64_REPLAY_REPORT_SIZE_LIMIT,
  T64_REPLAY_REPORT_INVALID_JSON,
  T64_REPLAY_REPORT_INVALID_SCHEMA,
  T64_REPLAY_REPORT_BAD_RESULT,
  T64_REPLAY_REPORT_BAD_DETAIL,
  T64_REPLAY_REPORT_BAD_FIELD,
  T64_REPLAY_REPORT_BAD_VALUE,
  T64_REPLAY_REPORT_MATCH_SHAPE,
  T64_REPLAY_REPORT_DIVERGED_SHAPE,
  T64_REPLAY_REPORT_IDENTITY_SHAPE,
  T64_REPLAY_REPORT_REFUSED_SHAPE,
  T64_REPLAY_REPORT_COUNT_ORDER
} t64_replay_report_status;

const char *t64_replay_report_status_name(t64_replay_report_status status);
const char *t64_replay_result_name(t64_replay_result result);
const char *t64_replay_detail_name(t64_replay_detail detail);

/* Validates the cross-field rules and writes the canonical record. */
t64_replay_report_status t64_replay_report_encode(
    const t64_replay_report *report, uint8_t *bytes_out, size_t capacity,
    size_t *length_out);
/* Parses and validates one report. report_out may be NULL. */
t64_replay_report_status t64_replay_report_check(const uint8_t *bytes,
                                                 size_t length,
                                                 t64_replay_report *report_out);

/* ---- player and driver ------------------------------------------------------- */

typedef struct t64_replay_player t64_replay_player;

typedef enum {
  T64_REPLAY_STEP_CONTINUE = 0,
  T64_REPLAY_STEP_MATCH,
  T64_REPLAY_STEP_FAILED
} t64_replay_step_result;

t64_replay_player *t64_replay_player_create(const t64_replay *replay);
void t64_replay_player_destroy(t64_replay_player *player);
/* The `at` of the next unapplied op (the end op included), or UINT64_MAX when
 * every op has been applied. */
uint64_t t64_replay_player_next_at(const t64_replay_player *player);
/* The `at` of the end op. */
uint64_t t64_replay_player_end_at(const t64_replay_player *player);
/* Applies, in array order, every op whose `at` equals the completed guest tick
 * `tick`: input ops go through the real t64_machine_queue_input, checkpoints
 * compare the machine digest and the end op compares the terminal stop kind
 * and the digest. `terminal` is T64_REPLAY_STOP_NONE while the run continues.
 * On T64_REPLAY_STEP_MATCH or T64_REPLAY_STEP_FAILED the report is filled. */
t64_replay_step_result t64_replay_player_step(t64_replay_player *player,
                                              t64_machine *machine,
                                              uint64_t tick,
                                              t64_replay_stop terminal,
                                              t64_replay_report *report);

typedef struct {
  uint64_t advance_step;   /* ticks per slice, at least 1 */
  uint64_t advance_calls;  /* out: calls to t64_machine_advance */
  /* Called after every advance call (a hook for interleaved machines). */
  void (*after_advance)(void *context, uint64_t tick);
  void *context;
} t64_replay_play_options;

/* Binds the replay to the live machine (refusals before any advance: the
 * report says identity-mismatch and advance_calls stays 0), then plays the ops
 * with inclusive tick-bound advances to each next op `at`, a resource-limit
 * yield continued under the progress rule of the runner (64 yields without
 * progress end in detail advance-invalid). */
t64_replay_step_result t64_replay_play(t64_machine *machine,
                                       const t64_replay *replay,
                                       t64_replay_play_options *options,
                                       t64_replay_report *report);

/* Publishes bytes as one private single file through the artifact publisher
 * (0600, no-clobber, never uploaded). *error_out is the publisher's stable
 * error name on failure. */
bool t64_replay_publish(const char *directory, const char *leaf,
                        uint64_t byte_cap, const uint8_t *bytes, size_t length,
                        const char **error_out);

#endif /* T64_REPLAY_H */
