/* Closed local CPU diagnostic protocol and whole-inventory aggregator. */
#ifndef T64_CPU_DIAG_PROTOCOL_H
#define T64_CPU_DIAG_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define T64_CPU_DIAG_SCHEMA_VERSION 1u
#define T64_CPU_DIAG_MAX_INVENTORY_BYTES (1024u * 1024u)
#define T64_CPU_DIAG_MAX_BATCH_BYTES 4096u
#define T64_CPU_DIAG_MAX_ROWS 65536u
#define T64_CPU_DIAG_MAX_NAME_BYTES 256u
#define T64_CPU_DIAG_MAX_VALUE_ID_BYTES 128u
#define T64_CPU_DIAG_MAX_SELECTOR_BYTES 128u
#define T64_CPU_DIAG_HASH_HEX_BYTES 64u

typedef enum {
  T64_CPU_DIAG_OK = 0,
  T64_CPU_DIAG_INVALID_ARGUMENT,
  T64_CPU_DIAG_SIZE_LIMIT,
  T64_CPU_DIAG_INVALID_JSON,
  T64_CPU_DIAG_INVALID_SCHEMA,
  T64_CPU_DIAG_INVALID_INVENTORY,
  T64_CPU_DIAG_INVALID_BATCH,
  T64_CPU_DIAG_INCOMPLETE,
  T64_CPU_DIAG_DUPLICATE_MEMBER,
  T64_CPU_DIAG_IDENTITY_MISMATCH,
  T64_CPU_DIAG_ALLOCATION_FAILED,
  T64_CPU_DIAG_CANONICALIZATION_FAILED,
  T64_CPU_DIAG_OUTPUT_OVERFLOW
} t64_cpu_diag_status;

typedef enum {
  T64_CPU_DIAG_OUTCOME_PASS = 0,
  T64_CPU_DIAG_OUTCOME_FAIL,
  T64_CPU_DIAG_OUTCOME_INCOMPLETE,
  T64_CPU_DIAG_OUTCOME_NOT_RUN,
  T64_CPU_DIAG_OUTCOME_GUEST_STOP,
  T64_CPU_DIAG_OUTCOME_HARNESS_ERROR
} t64_cpu_diag_outcome;

typedef enum {
  T64_CPU_DIAG_GROUP_INCOMPLETE = 0,
  T64_CPU_DIAG_GROUP_PASS,
  T64_CPU_DIAG_GROUP_FAIL
} t64_cpu_diag_group_status;

typedef struct t64_cpu_diag_inventory t64_cpu_diag_inventory;
typedef struct t64_cpu_diag_aggregate t64_cpu_diag_aggregate;

typedef struct {
  uint32_t registration_index;
  uint32_t value_index;
  const char *group;
  const char *registration_name;
  const char *value_id;
  bool has_outcome;
  t64_cpu_diag_outcome outcome;
} t64_cpu_diag_member_result;

/* Parses one canonical, closed t64.cpu-inventory v1 document. */
t64_cpu_diag_status t64_cpu_diag_inventory_parse(
    const uint8_t *bytes, size_t length,
    t64_cpu_diag_inventory **inventory_out);
void t64_cpu_diag_inventory_destroy(t64_cpu_diag_inventory *inventory);
size_t t64_cpu_diag_inventory_registration_count(
    const t64_cpu_diag_inventory *inventory);
size_t t64_cpu_diag_inventory_required_value_count(
    const t64_cpu_diag_inventory *inventory);
size_t t64_cpu_diag_required_group_count(void);
/* Borrowed fixed map entry; NULL when index is out of range. */
const char *t64_cpu_diag_required_group_name(size_t index);
/* Borrowed lowercase digest of the canonical inventory bytes. */
const char *t64_cpu_diag_inventory_sha256(
    const t64_cpu_diag_inventory *inventory);

/* Encodes one fresh-run start/outcome/done transcript for a required member. */
t64_cpu_diag_status t64_cpu_diag_batch_encode(
    const t64_cpu_diag_inventory *inventory, uint32_t registration_index,
    uint32_t value_index, t64_cpu_diag_outcome outcome,
    char *bytes_out, size_t capacity, size_t *length_out);

/* Aggregates one and only one result for each required inventory member. */
t64_cpu_diag_status t64_cpu_diag_aggregate_create(
    const t64_cpu_diag_inventory *inventory,
    t64_cpu_diag_aggregate **aggregate_out);
void t64_cpu_diag_aggregate_destroy(t64_cpu_diag_aggregate *aggregate);
t64_cpu_diag_status t64_cpu_diag_aggregate_add_batch(
    t64_cpu_diag_aggregate *aggregate, const uint8_t *bytes, size_t length);
bool t64_cpu_diag_aggregate_is_complete(
    const t64_cpu_diag_aggregate *aggregate);
size_t t64_cpu_diag_aggregate_member_count(
    const t64_cpu_diag_aggregate *aggregate);
/* Iterates required members in pinned source-registration/value order. */
bool t64_cpu_diag_aggregate_member_at(
    const t64_cpu_diag_aggregate *aggregate, size_t required_ordinal,
    t64_cpu_diag_member_result *result_out);
t64_cpu_diag_status t64_cpu_diag_aggregate_group(
    const t64_cpu_diag_aggregate *aggregate, const char *group,
    t64_cpu_diag_group_status *status_out, size_t *expected_out,
    size_t *reached_out, size_t *passed_out, size_t *failed_out);

const char *t64_cpu_diag_status_name(t64_cpu_diag_status status);
const char *t64_cpu_diag_outcome_name(t64_cpu_diag_outcome outcome);

#endif /* T64_CPU_DIAG_PROTOCOL_H */
