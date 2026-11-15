/* Closed, source-anchored whole-BASE diagnostic inventory and aggregation. */
#ifndef T64_BASE_DIAG_PROTOCOL_H
#define T64_BASE_DIAG_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define T64_BASE_DIAG_INVENTORY_VERSION 1u
#define T64_BASE_DIAG_RECORD_VERSION 1u
#define T64_BASE_DIAG_MAX_INVENTORY_BYTES (4u * 1024u * 1024u)
#define T64_BASE_DIAG_MAX_RECORD_BYTES 8192u
#define T64_BASE_DIAG_MAX_ORIGINAL_BYTES 4096u
#define T64_BASE_DIAG_MAX_MEMBERS 131072u
#define T64_BASE_DIAG_MAX_TEXT_BYTES 512u
#define T64_BASE_DIAG_HASH_HEX_BYTES 64u
#define T64_BASE_DIAG_BOUND_RECORD_VERSION 2u
#define T64_BASE_DIAG_MAX_REPORT_BYTES (64u * 1024u * 1024u)

typedef enum {
  T64_BASE_DIAG_OK = 0,
  T64_BASE_DIAG_INVALID_ARGUMENT,
  T64_BASE_DIAG_SIZE_LIMIT,
  T64_BASE_DIAG_INVALID_JSON,
  T64_BASE_DIAG_INVALID_SCHEMA,
  T64_BASE_DIAG_INVALID_INVENTORY,
  T64_BASE_DIAG_INVALID_RECORD,
  T64_BASE_DIAG_INCOMPLETE,
  T64_BASE_DIAG_DUPLICATE_MEMBER,
  T64_BASE_DIAG_IDENTITY_MISMATCH,
  T64_BASE_DIAG_ALLOCATION_FAILED,
  T64_BASE_DIAG_CANONICALIZATION_FAILED,
  T64_BASE_DIAG_OUTPUT_OVERFLOW
} t64_base_diag_status;

typedef enum {
  T64_BASE_DIAG_OUTCOME_PASS = 0,
  T64_BASE_DIAG_OUTCOME_FAIL,
  T64_BASE_DIAG_OUTCOME_INCOMPLETE,
  T64_BASE_DIAG_OUTCOME_NOT_RUN,
  T64_BASE_DIAG_OUTCOME_GUEST_STOP,
  T64_BASE_DIAG_OUTCOME_HARNESS_ERROR,
  T64_BASE_DIAG_OUTCOME_TIMEOUT,
  T64_BASE_DIAG_OUTCOME_BLOCKED,
  T64_BASE_DIAG_OUTCOME_UNEXECUTED,
  T64_BASE_DIAG_OUTCOME_REFUSED
} t64_base_diag_outcome;

typedef struct t64_base_diag_inventory t64_base_diag_inventory;
typedef struct t64_base_diag_aggregate t64_base_diag_aggregate;

/* Expected identity comes from the independently verified immutable context.
 * Strings are NUL terminated; constructors copy them into aggregate storage. */
typedef struct {
  const char *context_sha256;
  const char *profile_id;
  const char *profile_sha256;
  const char *core_build_id;
} t64_base_diag_target;

typedef struct {
  const char *selected_rom_sha256;
  const char *reason;
  uint64_t tick;
  uint64_t capture_total;
  uint32_t park_sequence;
  uint32_t terminal_pc;
  uint32_t terminal_opcode;
  uint32_t terminal_delay_opcode;
  bool entry_reached;
  bool capture_exact;
  bool capture_lost;
} t64_base_diag_execution;

typedef struct {
  uint32_t registration_index;
  uint32_t value_index;
  const char *group;
  const char *level;
  const char *lane;
  const char *name;
  const char *execution;
  const char *reason;
  const char *value_id;
  bool required;
} t64_base_diag_member;

typedef struct {
  uint32_t registration_index;
  uint32_t value_index;
  t64_base_diag_outcome outcome;
  const char *original_sha256;
} t64_base_diag_member_result;

/* The expected authority digest comes from the independently checked context. */
t64_base_diag_status t64_base_diag_inventory_parse(
    const uint8_t *source_authority, size_t source_authority_length,
    const char *expected_source_authority_sha256, const uint8_t *inventory,
    size_t inventory_length, t64_base_diag_inventory **inventory_out);
void t64_base_diag_inventory_destroy(t64_base_diag_inventory *inventory);
size_t t64_base_diag_inventory_member_count(
    const t64_base_diag_inventory *inventory);
const char *t64_base_diag_inventory_sha256(
    const t64_base_diag_inventory *inventory);
bool t64_base_diag_inventory_member_at(
    const t64_base_diag_inventory *inventory, size_t ordinal,
    t64_base_diag_member *member_out);

/* Produce the canonical record consumed by the BASE aggregate. */
t64_base_diag_status t64_base_diag_encode_member_record(
    const t64_base_diag_inventory *inventory, uint32_t registration_index,
    uint32_t value_index, t64_base_diag_outcome outcome,
    const uint8_t *original_bytes, size_t original_length, uint8_t *record,
    size_t record_capacity, size_t *record_length);

t64_base_diag_status t64_base_diag_aggregate_create(
    const t64_base_diag_inventory *inventory,
    t64_base_diag_aggregate **aggregate_out);
t64_base_diag_status t64_base_diag_aggregate_create_bound(
    const t64_base_diag_inventory *inventory,
    const t64_base_diag_target *expected_target,
    t64_base_diag_aggregate **aggregate_out);
/* Expected selected image digest is derived from the qualified image/selector
 * closure, not from the member record being checked. */
t64_base_diag_status t64_base_diag_aggregate_bind_image(
    t64_base_diag_aggregate *aggregate, uint32_t registration_index,
    uint32_t value_index, const char *expected_rom_sha256);
t64_base_diag_status t64_base_diag_encode_bound_member_record(
    const t64_base_diag_inventory *inventory, uint32_t registration_index,
    uint32_t value_index, t64_base_diag_outcome outcome,
    const t64_base_diag_target *target,
    const t64_base_diag_execution *execution,
    const uint8_t *original_bytes, size_t original_length,
    uint8_t *record, size_t record_capacity, size_t *record_length);
void t64_base_diag_aggregate_destroy(t64_base_diag_aggregate *aggregate);
/* Authenticates the canonical member record and its exact original bytes. */
t64_base_diag_status t64_base_diag_aggregate_add_original(
    t64_base_diag_aggregate *aggregate, const uint8_t *record,
    size_t record_length, const uint8_t *original_bytes,
    size_t original_length);
bool t64_base_diag_aggregate_is_complete(
    const t64_base_diag_aggregate *aggregate);
bool t64_base_diag_aggregate_is_all_pass(
    const t64_base_diag_aggregate *aggregate);
bool t64_base_diag_aggregate_execution_complete(
    const t64_base_diag_aggregate *aggregate);
t64_base_diag_status t64_base_diag_aggregate_report(
    const t64_base_diag_aggregate *aggregate, uint8_t *output,
    size_t capacity, size_t *length_out);
size_t t64_base_diag_aggregate_member_count(
    const t64_base_diag_aggregate *aggregate);
bool t64_base_diag_aggregate_member_at(
    const t64_base_diag_aggregate *aggregate, size_t ordinal,
    t64_base_diag_member_result *result_out);

const char *t64_base_diag_status_name(t64_base_diag_status status);
const char *t64_base_diag_outcome_name(t64_base_diag_outcome outcome);

#endif /* T64_BASE_DIAG_PROTOCOL_H */
