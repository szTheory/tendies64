/* Source-anchored whole-BASE diagnostic protocol implementation. */
#include "t64_base_diag_protocol.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "t64_diag_common.h"
#include "t64_sha256.h"

#define BASE_DIAG_MAX_JSON_NODES 262144u

typedef struct {
  t64_base_diag_member public;
} base_member;

struct t64_base_diag_inventory {
  t64_json_doc document;
  base_member *members;
  size_t member_count;
  char sha256[T64_SHA256_HEX_SIZE];
};

struct t64_base_diag_aggregate {
  const t64_base_diag_inventory *inventory;
  t64_diag_partition *partition;
  char (*original_sha256)[T64_SHA256_HEX_SIZE];
  bool invalid;
};

static const char *const authority_keys[] = {
    "members", "schema", "schema_version", "source_revision"};
static const char *const inventory_keys[] = {
    "members", "schema", "schema_version", "source_authority_sha256",
    "source_revision"};
static const char *const member_keys[] = {
    "execution", "group", "index", "lane", "level", "name", "reason",
    "required", "value_id", "value_index"};
static const char *const record_keys[] = {
    "inventory_sha256", "original_sha256", "outcome", "registration_index",
    "schema", "schema_version", "value_index"};

static bool exact_keys(const t64_json_node *object, const char *const *keys,
                       size_t count) {
  return t64_diag_json_object_has_exact_keys(object, keys, count);
}

static bool schema_version(const t64_json_node *root, const char *schema) {
  const t64_json_node *version = t64_json_get(root, "schema_version");
  return t64_diag_json_string_equals(t64_json_get(root, "schema"), schema) &&
         version != NULL && version->type == T64_JSON_INT &&
         version->num == 1;
}

static bool u32_member(const t64_json_node *object, const char *key,
                       uint32_t *out) {
  const t64_json_node *node = t64_json_get(object, key);
  if (node == NULL || node->type != T64_JSON_INT || node->num < 0 ||
      (uint64_t)node->num > UINT32_MAX) {
    return false;
  }
  *out = (uint32_t)node->num;
  return true;
}

static bool text_member(const t64_json_node *object, const char *key,
                        size_t maximum, bool allow_empty,
                        const char **out) {
  const t64_json_node *node = t64_json_get(object, key);
  if (!t64_diag_json_printable_text(node, maximum, allow_empty)) return false;
  *out = node->str;
  return true;
}

static bool parse_member(const t64_json_node *node,
                         t64_base_diag_member *member) {
  const t64_json_node *required;
  if (!exact_keys(node, member_keys,
                  sizeof member_keys / sizeof member_keys[0]) ||
      !u32_member(node, "index", &member->registration_index) ||
      !u32_member(node, "value_index", &member->value_index) ||
      !text_member(node, "execution", 16u, false, &member->execution) ||
      !text_member(node, "group", T64_BASE_DIAG_MAX_TEXT_BYTES, false,
                   &member->group) ||
      !text_member(node, "lane", T64_BASE_DIAG_MAX_TEXT_BYTES, false,
                   &member->lane) ||
      !text_member(node, "level", T64_BASE_DIAG_MAX_TEXT_BYTES, false,
                   &member->level) ||
      !text_member(node, "name", T64_BASE_DIAG_MAX_TEXT_BYTES, false,
                   &member->name) ||
      !text_member(node, "reason", T64_BASE_DIAG_MAX_TEXT_BYTES, true,
                   &member->reason) ||
      !text_member(node, "value_id", T64_BASE_DIAG_MAX_TEXT_BYTES, true,
                   &member->value_id)) {
    return false;
  }
  required = t64_json_get(node, "required");
  if (required == NULL || required->type != T64_JSON_BOOL) return false;
  member->required = required->num != 0;
  if (strcmp(member->execution, "values") != 0 &&
      strcmp(member->execution, "once") != 0 &&
      strcmp(member->execution, "blocked") != 0) {
    return false;
  }
  if (strcmp(member->execution, "blocked") == 0 && member->reason[0] == '\0')
    return false;
  if (strcmp(member->execution, "once") == 0 &&
      (member->value_index != 0u || member->value_id[0] != '\0')) {
    return false;
  }
  return true;
}

static bool same_member(const t64_base_diag_member *left,
                        const t64_base_diag_member *right) {
  return left->registration_index == right->registration_index &&
         left->value_index == right->value_index &&
         left->required == right->required &&
         strcmp(left->execution, right->execution) == 0 &&
         strcmp(left->group, right->group) == 0 &&
         strcmp(left->lane, right->lane) == 0 &&
         strcmp(left->level, right->level) == 0 &&
         strcmp(left->name, right->name) == 0 &&
         strcmp(left->reason, right->reason) == 0 &&
         strcmp(left->value_id, right->value_id) == 0;
}

static bool lower_hex_digest(const char *text) {
  size_t i;
  if (text == NULL || strlen(text) != T64_BASE_DIAG_HASH_HEX_BYTES) return false;
  for (i = 0u; i < T64_BASE_DIAG_HASH_HEX_BYTES; i++) {
    if (!((text[i] >= '0' && text[i] <= '9') ||
          (text[i] >= 'a' && text[i] <= 'f'))) {
      return false;
    }
  }
  return true;
}

static void sha256_hex(const uint8_t *bytes, size_t length,
                       char output[T64_SHA256_HEX_SIZE]) {
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  t64_sha256_ctx context;
  t64_sha256_init(&context);
  t64_sha256_update(&context, bytes, length);
  t64_sha256_final(&context, digest);
  t64_sha256_hex(digest, output);
}

static bool json_document(const uint8_t *bytes, size_t length,
                          size_t maximum, size_t max_nodes,
                          t64_json_doc *document,
                          t64_base_diag_status *status) {
  t64_json_limits limits = {maximum, 8u, T64_BASE_DIAG_MAX_TEXT_BYTES,
                            max_nodes};
  t64_json_error error = {T64_JSON_OK, 0u};
  if (t64_json_parse(bytes, length, &limits, document, &error) != 0) {
    *status = T64_BASE_DIAG_INVALID_JSON;
    return false;
  }
  if (!t64_diag_json_canonical_document(document, bytes, length)) {
    *status = T64_BASE_DIAG_INVALID_SCHEMA;
    return false;
  }
  return true;
}

static bool row_order_valid(const base_member *members, size_t count) {
  size_t i;
  for (i = 1u; i < count; i++) {
    const t64_base_diag_member *previous = &members[i - 1u].public;
    const t64_base_diag_member *current = &members[i].public;
    if (current->registration_index < previous->registration_index) return false;
    if (current->registration_index == previous->registration_index) {
      if (current->value_index != previous->value_index + 1u ||
          strcmp(previous->execution, "once") == 0 ||
          strcmp(current->execution, previous->execution) != 0 ||
          (strcmp(current->execution, "values") != 0 &&
           strcmp(current->execution, "blocked") != 0) ||
          strcmp(previous->group, current->group) != 0 ||
          strcmp(previous->lane, current->lane) != 0 ||
          strcmp(previous->level, current->level) != 0 ||
          strcmp(previous->name, current->name) != 0 ||
          strcmp(previous->reason, current->reason) != 0 ||
          previous->required != current->required) {
        return false;
      }
    } else if (strcmp(current->execution, "values") == 0 &&
               current->value_index != 0u) {
      return false;
    }
  }
  if (count != 0u && strcmp(members[0].public.execution, "values") == 0 &&
      members[0].public.value_index != 0u) {
    return false;
  }
  return true;
}

t64_base_diag_status t64_base_diag_inventory_parse(
    const uint8_t *source_authority, size_t source_authority_length,
    const char *expected_source_authority_sha256, const uint8_t *inventory,
    size_t inventory_length, t64_base_diag_inventory **inventory_out) {
  t64_base_diag_status status = T64_BASE_DIAG_INVALID_INVENTORY;
  t64_json_doc authority_doc = {0};
  t64_json_doc inventory_doc = {0};
  t64_base_diag_inventory *parsed = NULL;
  const t64_json_node *authority_members;
  const t64_json_node *inventory_members;
  const t64_json_node *authority_row;
  const t64_json_node *inventory_row;
  const t64_json_node *revision;
  const t64_json_node *inventory_revision;
  const t64_json_node *authority_digest;
  char actual_authority_sha256[T64_SHA256_HEX_SIZE];
  size_t i;

  if (inventory_out == NULL) return T64_BASE_DIAG_INVALID_ARGUMENT;
  *inventory_out = NULL;
  if (source_authority == NULL || inventory == NULL ||
      expected_source_authority_sha256 == NULL) {
    return T64_BASE_DIAG_INVALID_ARGUMENT;
  }
  if (source_authority_length == 0u || inventory_length == 0u ||
      source_authority_length > T64_BASE_DIAG_MAX_INVENTORY_BYTES ||
      inventory_length > T64_BASE_DIAG_MAX_INVENTORY_BYTES) {
    return T64_BASE_DIAG_SIZE_LIMIT;
  }
  if (!lower_hex_digest(expected_source_authority_sha256))
    return T64_BASE_DIAG_INVALID_ARGUMENT;
  sha256_hex(source_authority, source_authority_length, actual_authority_sha256);
  if (strcmp(actual_authority_sha256, expected_source_authority_sha256) != 0)
    return T64_BASE_DIAG_IDENTITY_MISMATCH;
  if (!json_document(source_authority, source_authority_length,
                     T64_BASE_DIAG_MAX_INVENTORY_BYTES,
                     BASE_DIAG_MAX_JSON_NODES, &authority_doc, &status) ||
      !json_document(inventory, inventory_length,
                     T64_BASE_DIAG_MAX_INVENTORY_BYTES,
                     BASE_DIAG_MAX_JSON_NODES, &inventory_doc, &status)) {
    goto cleanup;
  }
  if (!exact_keys(authority_doc.root, authority_keys,
                  sizeof authority_keys / sizeof authority_keys[0]) ||
      !schema_version(authority_doc.root, "t64.base-source-authority") ||
      !exact_keys(inventory_doc.root, inventory_keys,
                  sizeof inventory_keys / sizeof inventory_keys[0]) ||
      !schema_version(inventory_doc.root, "t64.base-inventory")) {
    status = T64_BASE_DIAG_INVALID_SCHEMA;
    goto cleanup;
  }
  authority_members = t64_json_get(authority_doc.root, "members");
  inventory_members = t64_json_get(inventory_doc.root, "members");
  revision = t64_json_get(authority_doc.root, "source_revision");
  inventory_revision = t64_json_get(inventory_doc.root, "source_revision");
  authority_digest = t64_json_get(inventory_doc.root,
                                  "source_authority_sha256");
  if (authority_members == NULL || authority_members->type != T64_JSON_ARRAY ||
      authority_members->count == 0u ||
      authority_members->count > T64_BASE_DIAG_MAX_MEMBERS ||
      inventory_members == NULL || inventory_members->type != T64_JSON_ARRAY ||
      inventory_members->count != authority_members->count ||
      !t64_diag_json_lowercase_hex(revision, 40u) ||
      inventory_revision == NULL || inventory_revision->type != T64_JSON_STRING ||
      revision->len != inventory_revision->len ||
      memcmp(revision->str, inventory_revision->str, revision->len) != 0 ||
      !t64_diag_json_lowercase_hex(authority_digest, 64u) ||
      strcmp(authority_digest->str, actual_authority_sha256) != 0) {
    status = T64_BASE_DIAG_IDENTITY_MISMATCH;
    goto cleanup;
  }
  parsed = (t64_base_diag_inventory *)calloc(1u, sizeof *parsed);
  if (parsed == NULL) {
    status = T64_BASE_DIAG_ALLOCATION_FAILED;
    goto cleanup;
  }
  parsed->members = (base_member *)calloc(authority_members->count,
                                           sizeof *parsed->members);
  if (parsed->members == NULL) {
    status = T64_BASE_DIAG_ALLOCATION_FAILED;
    goto cleanup;
  }
  authority_row = authority_members->first;
  inventory_row = inventory_members->first;
  for (i = 0u; i < authority_members->count; i++) {
    t64_base_diag_member authoritative;
    t64_base_diag_member materialized;
    if (authority_row == NULL || inventory_row == NULL ||
        !parse_member(authority_row, &authoritative) ||
        !parse_member(inventory_row, &materialized)) {
      status = T64_BASE_DIAG_INVALID_INVENTORY;
      goto cleanup;
    }
    if (!same_member(&authoritative, &materialized)) {
      status = T64_BASE_DIAG_IDENTITY_MISMATCH;
      goto cleanup;
    }
    parsed->members[i].public = materialized;
    authority_row = authority_row->next;
    inventory_row = inventory_row->next;
  }
  if (authority_row != NULL || inventory_row != NULL ||
      !row_order_valid(parsed->members, authority_members->count)) {
    status = T64_BASE_DIAG_INVALID_INVENTORY;
    goto cleanup;
  }
  parsed->member_count = authority_members->count;
  sha256_hex(inventory, inventory_length, parsed->sha256);
  parsed->document = inventory_doc;
  (void)memset(&inventory_doc, 0, sizeof inventory_doc);
  *inventory_out = parsed;
  parsed = NULL;
  status = T64_BASE_DIAG_OK;

cleanup:
  if (parsed != NULL) {
    free(parsed->members);
    free(parsed);
  }
  t64_json_free(&authority_doc);
  t64_json_free(&inventory_doc);
  return status;
}

void t64_base_diag_inventory_destroy(t64_base_diag_inventory *inventory) {
  if (inventory == NULL) return;
  free(inventory->members);
  t64_json_free(&inventory->document);
  free(inventory);
}

size_t t64_base_diag_inventory_member_count(
    const t64_base_diag_inventory *inventory) {
  return inventory == NULL ? 0u : inventory->member_count;
}

const char *t64_base_diag_inventory_sha256(
    const t64_base_diag_inventory *inventory) {
  return inventory == NULL ? NULL : inventory->sha256;
}

bool t64_base_diag_inventory_member_at(
    const t64_base_diag_inventory *inventory, size_t ordinal,
    t64_base_diag_member *member_out) {
  if (inventory == NULL || member_out == NULL ||
      ordinal >= inventory->member_count) {
    return false;
  }
  *member_out = inventory->members[ordinal].public;
  return true;
}

t64_base_diag_status t64_base_diag_encode_member_record(
    const t64_base_diag_inventory *inventory, uint32_t registration_index,
    uint32_t value_index, t64_base_diag_outcome outcome,
    const uint8_t *original_bytes, size_t original_length, uint8_t *record,
    size_t record_capacity, size_t *record_length) {
  (void)inventory;
  (void)registration_index;
  (void)value_index;
  (void)outcome;
  (void)original_bytes;
  (void)original_length;
  (void)record;
  (void)record_capacity;
  if (record_length != NULL) *record_length = 0u;
  return T64_BASE_DIAG_OUTPUT_OVERFLOW;
}

t64_base_diag_status t64_base_diag_aggregate_create(
    const t64_base_diag_inventory *inventory,
    t64_base_diag_aggregate **aggregate_out) {
  t64_base_diag_aggregate *aggregate;
  if (aggregate_out == NULL || inventory == NULL) {
    return T64_BASE_DIAG_INVALID_ARGUMENT;
  }
  *aggregate_out = NULL;
  aggregate = (t64_base_diag_aggregate *)calloc(1u, sizeof *aggregate);
  if (aggregate == NULL) return T64_BASE_DIAG_ALLOCATION_FAILED;
  if (t64_diag_partition_create(inventory->member_count,
                                &aggregate->partition) !=
      T64_DIAG_PARTITION_OK) {
    free(aggregate);
    return T64_BASE_DIAG_ALLOCATION_FAILED;
  }
  aggregate->original_sha256 = (char (*)[T64_SHA256_HEX_SIZE])calloc(
      inventory->member_count, sizeof *aggregate->original_sha256);
  if (aggregate->original_sha256 == NULL) {
    t64_diag_partition_destroy(aggregate->partition);
    free(aggregate);
    return T64_BASE_DIAG_ALLOCATION_FAILED;
  }
  aggregate->inventory = inventory;
  *aggregate_out = aggregate;
  return T64_BASE_DIAG_OK;
}

void t64_base_diag_aggregate_destroy(t64_base_diag_aggregate *aggregate) {
  if (aggregate == NULL) return;
  t64_diag_partition_destroy(aggregate->partition);
  free(aggregate->original_sha256);
  free(aggregate);
}

static bool find_member(const t64_base_diag_inventory *inventory,
                        uint32_t registration_index, uint32_t value_index,
                        size_t *ordinal_out) {
  size_t low = 0u;
  size_t high = inventory->member_count;
  while (low < high) {
    size_t middle = low + (high - low) / 2u;
    const t64_base_diag_member *member = &inventory->members[middle].public;
    if (member->registration_index < registration_index ||
        (member->registration_index == registration_index &&
         member->value_index < value_index)) {
      low = middle + 1u;
    } else {
      high = middle;
    }
  }
  if (low >= inventory->member_count ||
      inventory->members[low].public.registration_index != registration_index ||
      inventory->members[low].public.value_index != value_index) {
    return false;
  }
  *ordinal_out = low;
  return true;
}

t64_base_diag_status t64_base_diag_aggregate_add_original(
    t64_base_diag_aggregate *aggregate, const uint8_t *record,
    size_t record_length, const uint8_t *original_bytes,
    size_t original_length) {
  t64_json_doc document = {0};
  t64_base_diag_status status = T64_BASE_DIAG_INVALID_RECORD;
  const t64_json_node *node;
  uint32_t registration_index;
  uint32_t value_index;
  size_t ordinal;
  char original_sha256[T64_SHA256_HEX_SIZE];
  t64_base_diag_outcome outcome;
  const char *outcome_name;
  static const char *const outcome_names[] = {
      "pass", "fail", "incomplete", "not-run", "guest-stop",
      "harness-error"};
  size_t i;
  if (aggregate == NULL || record == NULL || original_bytes == NULL) {
    return T64_BASE_DIAG_INVALID_ARGUMENT;
  }
  if (record_length == 0u || record_length > T64_BASE_DIAG_MAX_RECORD_BYTES) {
    aggregate->invalid = true;
    return T64_BASE_DIAG_SIZE_LIMIT;
  }
  if (!json_document(record, record_length, T64_BASE_DIAG_MAX_RECORD_BYTES,
                     64u, &document, &status)) {
    aggregate->invalid = true;
    goto cleanup;
  }
  if (!exact_keys(document.root, record_keys,
                  sizeof record_keys / sizeof record_keys[0]) ||
      !schema_version(document.root, "t64.base-member-record") ||
      !u32_member(document.root, "registration_index", &registration_index) ||
      !u32_member(document.root, "value_index", &value_index)) {
    status = T64_BASE_DIAG_INVALID_RECORD;
    aggregate->invalid = true;
    goto cleanup;
  }
  node = t64_json_get(document.root, "inventory_sha256");
  if (!t64_diag_json_lowercase_hex(node, 64u) ||
      strcmp(node->str, aggregate->inventory->sha256) != 0) {
    status = T64_BASE_DIAG_IDENTITY_MISMATCH;
    aggregate->invalid = true;
    goto cleanup;
  }
  node = t64_json_get(document.root, "original_sha256");
  if (!t64_diag_json_lowercase_hex(node, 64u)) {
    status = T64_BASE_DIAG_INVALID_RECORD;
    aggregate->invalid = true;
    goto cleanup;
  }
  sha256_hex(original_bytes, original_length, original_sha256);
  if (strcmp(node->str, original_sha256) != 0) {
    status = T64_BASE_DIAG_IDENTITY_MISMATCH;
    aggregate->invalid = true;
    goto cleanup;
  }
  node = t64_json_get(document.root, "outcome");
  outcome_name = node != NULL && node->type == T64_JSON_STRING ? node->str : NULL;
  for (i = 0u; i < sizeof outcome_names / sizeof outcome_names[0]; i++) {
    if (outcome_name != NULL && strcmp(outcome_name, outcome_names[i]) == 0)
      break;
  }
  if (i == sizeof outcome_names / sizeof outcome_names[0]) {
    status = T64_BASE_DIAG_INVALID_RECORD;
    aggregate->invalid = true;
    goto cleanup;
  }
  outcome = (t64_base_diag_outcome)i;
  if (!find_member(aggregate->inventory, registration_index, value_index,
                   &ordinal)) {
    status = T64_BASE_DIAG_IDENTITY_MISMATCH;
    aggregate->invalid = true;
    goto cleanup;
  }
  {
    t64_diag_partition_status partition_status = t64_diag_partition_add(
        aggregate->partition, ordinal, (uint8_t)outcome);
    if (partition_status == T64_DIAG_PARTITION_DUPLICATE) {
      status = T64_BASE_DIAG_DUPLICATE_MEMBER;
      aggregate->invalid = true;
      goto cleanup;
    }
    if (partition_status != T64_DIAG_PARTITION_OK) {
      status = T64_BASE_DIAG_INVALID_RECORD;
      aggregate->invalid = true;
      goto cleanup;
    }
  }
  (void)memcpy(aggregate->original_sha256[ordinal], original_sha256,
               T64_SHA256_HEX_SIZE);
  status = T64_BASE_DIAG_OK;

cleanup:
  t64_json_free(&document);
  return status;
}

bool t64_base_diag_aggregate_is_complete(
    const t64_base_diag_aggregate *aggregate) {
  size_t i;
  if (aggregate == NULL || aggregate->inventory == NULL || aggregate->invalid)
    return false;
  for (i = 0u; i < aggregate->inventory->member_count; i++) {
    const t64_base_diag_member *member =
        &aggregate->inventory->members[i].public;
    if (member->required && !t64_diag_partition_seen(aggregate->partition, i))
      return false;
  }
  return true;
}

bool t64_base_diag_aggregate_is_all_pass(
    const t64_base_diag_aggregate *aggregate) {
  size_t i;
  if (!t64_base_diag_aggregate_is_complete(aggregate)) return false;
  for (i = 0u; i < aggregate->inventory->member_count; i++) {
    const t64_base_diag_member *member =
        &aggregate->inventory->members[i].public;
    uint8_t outcome;
    if (member->required &&
        (!t64_diag_partition_outcome(aggregate->partition, i, &outcome) ||
         outcome != T64_BASE_DIAG_OUTCOME_PASS)) {
      return false;
    }
  }
  return true;
}

size_t t64_base_diag_aggregate_member_count(
    const t64_base_diag_aggregate *aggregate) {
  size_t i;
  size_t count = 0u;
  if (aggregate == NULL || aggregate->inventory == NULL) return 0u;
  for (i = 0u; i < aggregate->inventory->member_count; i++) {
    if (t64_diag_partition_seen(aggregate->partition, i)) count++;
  }
  return count;
}

bool t64_base_diag_aggregate_member_at(
    const t64_base_diag_aggregate *aggregate, size_t ordinal,
    t64_base_diag_member_result *result_out) {
  size_t i;
  size_t seen = 0u;
  if (aggregate == NULL || aggregate->inventory == NULL || result_out == NULL)
    return false;
  for (i = 0u; i < aggregate->inventory->member_count; i++) {
    uint8_t outcome;
    if (!t64_diag_partition_outcome(aggregate->partition, i, &outcome)) continue;
    if (seen++ != ordinal) continue;
    result_out->registration_index =
        aggregate->inventory->members[i].public.registration_index;
    result_out->value_index = aggregate->inventory->members[i].public.value_index;
    result_out->outcome = (t64_base_diag_outcome)outcome;
    result_out->original_sha256 = aggregate->original_sha256[i];
    return true;
  }
  return false;
}

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
