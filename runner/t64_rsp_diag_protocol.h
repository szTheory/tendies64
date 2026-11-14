/* Closed, component-bound RSP diagnostic inventory and result protocol. */
#ifndef T64_RSP_DIAG_PROTOCOL_H
#define T64_RSP_DIAG_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define T64_RSP_DIAG_INVENTORY_VERSION 1u
#define T64_RSP_DIAG_BATCH_VERSION 1u
#define T64_RSP_DIAG_GUEST_RUN_VERSION 1u
#define T64_RSP_DIAG_MAX_INVENTORY_BYTES (1024u * 1024u)
#define T64_RSP_DIAG_MAX_BATCH_BYTES 4096u
#define T64_RSP_DIAG_MAX_ROWS 65536u
#define T64_RSP_DIAG_MAX_NAME_BYTES 256u
#define T64_RSP_DIAG_MAX_TEXT_BYTES 512u
#define T64_RSP_DIAG_MAX_SELECTOR_BYTES 128u

typedef enum {
  T64_RSP_DIAG_OK = 0,
  T64_RSP_DIAG_INVALID_ARGUMENT,
  T64_RSP_DIAG_SIZE_LIMIT,
  T64_RSP_DIAG_INVALID_JSON,
  T64_RSP_DIAG_INVALID_SCHEMA,
  T64_RSP_DIAG_INVALID_COMPONENT,
  T64_RSP_DIAG_INVALID_INVENTORY,
  T64_RSP_DIAG_INVALID_BATCH,
  T64_RSP_DIAG_INCOMPLETE,
  T64_RSP_DIAG_DUPLICATE_MEMBER,
  T64_RSP_DIAG_IDENTITY_MISMATCH,
  T64_RSP_DIAG_ALLOCATION_FAILED,
  T64_RSP_DIAG_CANONICALIZATION_FAILED,
  T64_RSP_DIAG_OUTPUT_OVERFLOW
} t64_rsp_diag_status;

typedef enum {
  T64_RSP_DIAG_OUTCOME_PASS = 0,
  T64_RSP_DIAG_OUTCOME_GUEST_FAIL,
  T64_RSP_DIAG_OUTCOME_REFUSED,
  T64_RSP_DIAG_OUTCOME_INCOMPLETE,
  T64_RSP_DIAG_OUTCOME_TIMEOUT,
  T64_RSP_DIAG_OUTCOME_HARNESS_ERROR
} t64_rsp_diag_outcome;

typedef struct t64_rsp_diag_inventory t64_rsp_diag_inventory;
typedef struct t64_rsp_diag_aggregate t64_rsp_diag_aggregate;

typedef struct {
  const char *source_revision;
  const char *source_projection_sha256;
  const char *qualification_sha256;
  const char *closure_sha256;
  const char *overlay_sha256;
  const char *core_build_sha256;
  const char *profile_descriptor_sha256;
  const char *profile_sha256;
  const char *profile_id;
  const char *rom_sha256;
  const char *boot_sha256;
} t64_rsp_diag_identity;

typedef struct {
  uint32_t registration_index;
  uint32_t value_index;
  const char *group;
  const char *registration_name;
  const char *source_name;
  const char *value_id;
  const char *selector;
  const char *selected_rom_sha256;
} t64_rsp_diag_member;

typedef struct {
  uint32_t registration_index;
  uint32_t value_index;
  t64_rsp_diag_outcome outcome;
  const char *operation;
  const char *reason;
  uint64_t retired_instructions;
  uint32_t terminal_pc;
  uint32_t terminal_opcode;
  bool terminal_delay_slot_retired;
  const char *result_sha256;
  const char *capture_sha256;
} t64_rsp_diag_member_result;

t64_rsp_diag_status t64_rsp_diag_inventory_parse(
    const uint8_t *bytes, size_t length,
    t64_rsp_diag_inventory **inventory_out);
void t64_rsp_diag_inventory_destroy(t64_rsp_diag_inventory *inventory);
size_t t64_rsp_diag_inventory_registration_count(
    const t64_rsp_diag_inventory *inventory);
size_t t64_rsp_diag_inventory_required_value_count(
    const t64_rsp_diag_inventory *inventory);
const char *t64_rsp_diag_inventory_sha256(
    const t64_rsp_diag_inventory *inventory);
bool t64_rsp_diag_inventory_identity(const t64_rsp_diag_inventory *inventory,
                                     t64_rsp_diag_identity *identity_out);
bool t64_rsp_diag_inventory_member_at(const t64_rsp_diag_inventory *inventory,
                                      size_t required_ordinal,
                                      t64_rsp_diag_member *member_out);

t64_rsp_diag_status t64_rsp_diag_batch_encode(
    const t64_rsp_diag_inventory *inventory, uint32_t registration_index,
    uint32_t value_index, const t64_rsp_diag_member_result *result,
    char *bytes_out, size_t capacity, size_t *length_out);
t64_rsp_diag_status t64_rsp_diag_aggregate_create(
    const t64_rsp_diag_inventory *inventory,
    t64_rsp_diag_aggregate **aggregate_out);
void t64_rsp_diag_aggregate_destroy(t64_rsp_diag_aggregate *aggregate);
t64_rsp_diag_status t64_rsp_diag_aggregate_add_batch(
    t64_rsp_diag_aggregate *aggregate, const uint8_t *bytes, size_t length);
bool t64_rsp_diag_aggregate_is_complete(
    const t64_rsp_diag_aggregate *aggregate);
size_t t64_rsp_diag_aggregate_member_count(
    const t64_rsp_diag_aggregate *aggregate);
bool t64_rsp_diag_aggregate_member_at(
    const t64_rsp_diag_aggregate *aggregate, size_t required_ordinal,
    t64_rsp_diag_member_result *result_out);

const char *t64_rsp_diag_status_name(t64_rsp_diag_status status);
const char *t64_rsp_diag_outcome_name(t64_rsp_diag_outcome outcome);

#endif /* T64_RSP_DIAG_PROTOCOL_H */
