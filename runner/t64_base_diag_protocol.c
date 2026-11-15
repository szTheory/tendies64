/* Source-anchored whole-BASE diagnostic protocol implementation. */
#include "t64_base_diag_protocol.h"

const char *t64_base_diag_status_name(t64_base_diag_status status) {
  static const char *const names[] = {
      "ok", "invalid-argument", "size-limit", "invalid-json",
      "invalid-schema", "invalid-inventory", "invalid-record", "incomplete",
      "duplicate-member", "identity-mismatch", "allocation-failed",
      "canonicalization-failed", "output-overflow"};
  if ((size_t)status >= sizeof names / sizeof names[0]) return "unknown";
  return names[(size_t)status];
}

const char *t64_base_diag_outcome_name(t64_base_diag_outcome outcome) {
  static const char *const names[] = {
      "pass", "fail", "incomplete", "not-run", "guest-stop",
      "harness-error"};
  if ((size_t)outcome >= sizeof names / sizeof names[0]) return NULL;
  return names[(size_t)outcome];
}

t64_base_diag_status t64_base_diag_inventory_parse(
    const uint8_t *source_authority, size_t source_authority_length,
    const char *expected_source_authority_sha256, const uint8_t *inventory,
    size_t inventory_length, t64_base_diag_inventory **inventory_out) {
  (void)source_authority;
  (void)source_authority_length;
  (void)expected_source_authority_sha256;
  (void)inventory;
  (void)inventory_length;
  if (inventory_out != NULL) *inventory_out = NULL;
  return T64_BASE_DIAG_INVALID_INVENTORY;
}

void t64_base_diag_inventory_destroy(t64_base_diag_inventory *inventory) {
  (void)inventory;
}

size_t t64_base_diag_inventory_member_count(
    const t64_base_diag_inventory *inventory) {
  (void)inventory;
  return 0u;
}

const char *t64_base_diag_inventory_sha256(
    const t64_base_diag_inventory *inventory) {
  (void)inventory;
  return NULL;
}

bool t64_base_diag_inventory_member_at(
    const t64_base_diag_inventory *inventory, size_t ordinal,
    t64_base_diag_member *member_out) {
  (void)inventory;
  (void)ordinal;
  (void)member_out;
  return false;
}

t64_base_diag_status t64_base_diag_aggregate_create(
    const t64_base_diag_inventory *inventory,
    t64_base_diag_aggregate **aggregate_out) {
  (void)inventory;
  if (aggregate_out != NULL) *aggregate_out = NULL;
  return T64_BASE_DIAG_INVALID_ARGUMENT;
}

void t64_base_diag_aggregate_destroy(t64_base_diag_aggregate *aggregate) {
  (void)aggregate;
}

t64_base_diag_status t64_base_diag_aggregate_add_original(
    t64_base_diag_aggregate *aggregate, const uint8_t *record,
    size_t record_length, const uint8_t *original_bytes,
    size_t original_length) {
  (void)aggregate;
  (void)record;
  (void)record_length;
  (void)original_bytes;
  (void)original_length;
  return T64_BASE_DIAG_INVALID_RECORD;
}

bool t64_base_diag_aggregate_is_complete(
    const t64_base_diag_aggregate *aggregate) {
  (void)aggregate;
  return false;
}

bool t64_base_diag_aggregate_is_all_pass(
    const t64_base_diag_aggregate *aggregate) {
  (void)aggregate;
  return false;
}

size_t t64_base_diag_aggregate_member_count(
    const t64_base_diag_aggregate *aggregate) {
  (void)aggregate;
  return 0u;
}

bool t64_base_diag_aggregate_member_at(
    const t64_base_diag_aggregate *aggregate, size_t ordinal,
    t64_base_diag_member_result *result_out) {
  (void)aggregate;
  (void)ordinal;
  (void)result_out;
  return false;
}
