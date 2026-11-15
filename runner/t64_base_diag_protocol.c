/* Source-anchored whole-BASE diagnostic protocol implementation. */
#include "t64_base_diag_protocol.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "t64_diag_common.h"
#include "t64_cpu_diag_protocol.h"
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
  char (*record_sha256)[T64_SHA256_HEX_SIZE];
  char (*selected_rom_sha256)[T64_SHA256_HEX_SIZE];
  char (*expected_rom_sha256)[T64_SHA256_HEX_SIZE];
  const char **reasons;
  bool bound;
  char context_sha256[T64_SHA256_HEX_SIZE];
  char profile_id[65];
  char profile_sha256[T64_SHA256_HEX_SIZE];
  char core_build_id[129];
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
static const char *const bound_record_keys[] = {
    "capture_exact", "capture_lost", "capture_total", "context_sha256",
    "core_build_id", "entry_reached", "inventory_sha256", "original_sha256",
    "outcome", "park_sequence", "profile_id", "profile_sha256", "reason",
    "registration_index", "schema", "schema_version", "selected_rom_sha256",
    "terminal_delay_opcode", "terminal_opcode", "terminal_pc", "tick", "value_index"};

static bool safe_text(const char *text, size_t maximum, bool empty) {
  size_t index;
  size_t length;
  if (text == NULL) return false;
  length = strlen(text);
  if (length > maximum || (!empty && length == 0u)) return false;
  for (index = 0u; index < length; index++) {
    unsigned char byte = (unsigned char)text[index];
    if (byte < 0x20u || byte > 0x7eu) return false;
  }
  return true;
}

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

static bool valid_target(const t64_base_diag_target *target) {
  static const char prefix[] = "t64-build-v1-";
  return target != NULL && lower_hex_digest(target->context_sha256) &&
         lower_hex_digest(target->profile_sha256) &&
         safe_text(target->profile_id, 64u, false) &&
         safe_text(target->core_build_id, 128u, false) &&
         strlen(target->core_build_id) == sizeof prefix - 1u + 64u &&
         memcmp(target->core_build_id, prefix, sizeof prefix - 1u) == 0 &&
         lower_hex_digest(target->core_build_id + sizeof prefix - 1u);
}

static bool decimal_u64(const t64_json_node *root, const char *key,
                         uint64_t *out) {
  const t64_json_node *node = t64_json_get(root, key);
  uint64_t value = 0u;
  size_t index;
  if (node == NULL || node->type != T64_JSON_STRING || node->len == 0u ||
      node->len > 20u || (node->len > 1u && node->str[0] == '0')) return false;
  for (index = 0u; index < node->len; index++) {
    uint32_t digit;
    if (node->str[index] < '0' || node->str[index] > '9') return false;
    digit = (uint32_t)(node->str[index] - '0');
    if (value > (UINT64_MAX - digit) / UINT64_C(10)) return false;
    value = value * UINT64_C(10) + digit;
  }
  *out = value;
  return true;
}

static bool bool_member(const t64_json_node *root, const char *key, bool *out) {
  const t64_json_node *node = t64_json_get(root, key);
  if (node == NULL || node->type != T64_JSON_BOOL) return false;
  *out = node->num != 0;
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
  static const uint8_t empty_bytes = 0u;
  const char *inventory_sha256;
  const char *outcome_name;
  char original_sha256[T64_SHA256_HEX_SIZE];
  uint8_t original_digest[T64_SHA256_DIGEST_SIZE];
  t64_sha256_ctx hash;
  t64_jsonw writer;
  t64_jsonw_status writer_status;
  if (record_length != NULL) *record_length = 0u;
  if (inventory == NULL || record == NULL || record_length == NULL ||
      (original_bytes == NULL && original_length != 0u)) {
    return T64_BASE_DIAG_INVALID_ARGUMENT;
  }
  if (original_length > T64_BASE_DIAG_MAX_ORIGINAL_BYTES)
    return T64_BASE_DIAG_SIZE_LIMIT;
  if (outcome > T64_BASE_DIAG_OUTCOME_HARNESS_ERROR)
    return T64_BASE_DIAG_INVALID_ARGUMENT;
  if (record_capacity == 0u) return T64_BASE_DIAG_OUTPUT_OVERFLOW;
  inventory_sha256 = t64_base_diag_inventory_sha256(inventory);
  outcome_name = t64_base_diag_outcome_name(outcome);
  if (inventory_sha256 == NULL || outcome_name == NULL) {
    return T64_BASE_DIAG_INVALID_ARGUMENT;
  }
  if (original_bytes == NULL) original_bytes = &empty_bytes;
  t64_sha256_init(&hash);
  t64_sha256_update(&hash, original_bytes, original_length);
  t64_sha256_final(&hash, original_digest);
  t64_sha256_hex(original_digest, original_sha256);

  t64_jsonw_init(&writer, (char *)record, record_capacity);
  writer_status = t64_jsonw_begin_object(&writer);
#define BASE_RECORD_KEY(key) \
  t64_jsonw_key(&writer, key, sizeof(key) - 1u)
  if (writer_status == T64_JSONW_OK)
    writer_status = BASE_RECORD_KEY("inventory_sha256");
  if (writer_status == T64_JSONW_OK)
    writer_status = t64_jsonw_string(&writer, inventory_sha256,
                                     strlen(inventory_sha256));
  if (writer_status == T64_JSONW_OK)
    writer_status = BASE_RECORD_KEY("original_sha256");
  if (writer_status == T64_JSONW_OK)
    writer_status = t64_jsonw_string(&writer, original_sha256,
                                     sizeof original_sha256 - 1u);
  if (writer_status == T64_JSONW_OK)
    writer_status = BASE_RECORD_KEY("outcome");
  if (writer_status == T64_JSONW_OK)
    writer_status = t64_jsonw_string(&writer, outcome_name,
                                     strlen(outcome_name));
  if (writer_status == T64_JSONW_OK)
    writer_status = BASE_RECORD_KEY("registration_index");
  if (writer_status == T64_JSONW_OK)
    writer_status = t64_jsonw_int(&writer, (int64_t)registration_index);
  if (writer_status == T64_JSONW_OK)
    writer_status = BASE_RECORD_KEY("schema");
  if (writer_status == T64_JSONW_OK)
    writer_status = t64_jsonw_string(&writer, "t64.base-member-record",
                                     sizeof "t64.base-member-record" - 1u);
  if (writer_status == T64_JSONW_OK)
    writer_status = BASE_RECORD_KEY("schema_version");
  if (writer_status == T64_JSONW_OK)
    writer_status = t64_jsonw_int(&writer, T64_BASE_DIAG_RECORD_VERSION);
  if (writer_status == T64_JSONW_OK)
    writer_status = BASE_RECORD_KEY("value_index");
  if (writer_status == T64_JSONW_OK)
    writer_status = t64_jsonw_int(&writer, (int64_t)value_index);
  if (writer_status == T64_JSONW_OK)
    writer_status = t64_jsonw_end_object(&writer);
  if (writer_status == T64_JSONW_OK)
    writer_status = t64_jsonw_finish(&writer, record_length);
#undef BASE_RECORD_KEY
  if (writer_status == T64_JSONW_E_OVERFLOW)
    return T64_BASE_DIAG_OUTPUT_OVERFLOW;
  if (writer_status != T64_JSONW_OK)
    return T64_BASE_DIAG_CANONICALIZATION_FAILED;
  return T64_BASE_DIAG_OK;
}

t64_base_diag_status t64_base_diag_encode_bound_member_record(
    const t64_base_diag_inventory *inventory, uint32_t registration_index,
    uint32_t value_index, t64_base_diag_outcome outcome,
    const t64_base_diag_target *target,
    const t64_base_diag_execution *execution,
    const uint8_t *original_bytes, size_t original_length,
    uint8_t *record, size_t record_capacity, size_t *record_length) {
  static const uint8_t empty = 0u;
  char original_sha256[T64_SHA256_HEX_SIZE];
  t64_jsonw writer;
  const char *name = t64_base_diag_outcome_name(outcome);
  bool ok;
  if (record_length != NULL) *record_length = 0u;
  if (inventory == NULL || !valid_target(target) || execution == NULL ||
      record == NULL || record_length == NULL || name == NULL ||
      !safe_text(execution->reason, T64_BASE_DIAG_MAX_TEXT_BYTES, true) ||
      execution->selected_rom_sha256 == NULL ||
      (execution->selected_rom_sha256[0] != '\0' &&
       !lower_hex_digest(execution->selected_rom_sha256)) ||
      (original_bytes == NULL && original_length != 0u))
    return T64_BASE_DIAG_INVALID_ARGUMENT;
  if (original_length > T64_BASE_DIAG_MAX_ORIGINAL_BYTES)
    return T64_BASE_DIAG_SIZE_LIMIT;
  sha256_hex(original_bytes == NULL ? &empty : original_bytes,
               original_length, original_sha256);
  t64_jsonw_init(&writer, (char *)record, record_capacity);
  ok = t64_jsonw_begin_object(&writer) == T64_JSONW_OK;
#define BKEY(key) (ok = ok && t64_diag_json_write_key(&writer, key))
#define BSTR(key, value) do { BKEY(key); ok = ok && t64_diag_json_write_string(&writer, value); } while (0)
#define BINT(key, value) do { BKEY(key); ok = ok && t64_jsonw_int(&writer, (int64_t)(value)) == T64_JSONW_OK; } while (0)
#define BBOOL(key, value) do { BKEY(key); ok = ok && t64_jsonw_bool(&writer, value) == T64_JSONW_OK; } while (0)
#define BU64(key, value) do { BKEY(key); ok = ok && t64_diag_json_write_u64_string(&writer, value); } while (0)
  BBOOL("capture_exact", execution->capture_exact);
  BBOOL("capture_lost", execution->capture_lost);
  BU64("capture_total", execution->capture_total);
  BSTR("context_sha256", target->context_sha256);
  BSTR("core_build_id", target->core_build_id);
  BBOOL("entry_reached", execution->entry_reached);
  BSTR("inventory_sha256", inventory->sha256);
  BSTR("original_sha256", original_sha256);
  BSTR("outcome", name);
  BINT("park_sequence", execution->park_sequence);
  BSTR("profile_id", target->profile_id);
  BSTR("profile_sha256", target->profile_sha256);
  BSTR("reason", execution->reason);
  BINT("registration_index", registration_index);
  BSTR("schema", "t64.base-member-record");
  BINT("schema_version", T64_BASE_DIAG_BOUND_RECORD_VERSION);
  BSTR("selected_rom_sha256", execution->selected_rom_sha256);
  BINT("terminal_delay_opcode", execution->terminal_delay_opcode);
  BINT("terminal_opcode", execution->terminal_opcode);
  BINT("terminal_pc", execution->terminal_pc);
  BU64("tick", execution->tick);
  BINT("value_index", value_index);
  ok = ok && t64_jsonw_end_object(&writer) == T64_JSONW_OK &&
       t64_jsonw_finish(&writer, record_length) == T64_JSONW_OK;
#undef BKEY
#undef BSTR
#undef BINT
#undef BBOOL
#undef BU64
  if (!ok) return writer.err == T64_JSONW_E_OVERFLOW
                      ? T64_BASE_DIAG_OUTPUT_OVERFLOW
                      : T64_BASE_DIAG_CANONICALIZATION_FAILED;
  return T64_BASE_DIAG_OK;
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
  aggregate->record_sha256 = (char (*)[T64_SHA256_HEX_SIZE])calloc(
      inventory->member_count, sizeof *aggregate->record_sha256);
  aggregate->selected_rom_sha256 = (char (*)[T64_SHA256_HEX_SIZE])calloc(
      inventory->member_count, sizeof *aggregate->selected_rom_sha256);
  aggregate->expected_rom_sha256 = (char (*)[T64_SHA256_HEX_SIZE])calloc(
      inventory->member_count, sizeof *aggregate->expected_rom_sha256);
  aggregate->reasons = (const char **)calloc(inventory->member_count,
                                             sizeof *aggregate->reasons);
  if (aggregate->original_sha256 == NULL || aggregate->record_sha256 == NULL ||
      aggregate->selected_rom_sha256 == NULL ||
      aggregate->expected_rom_sha256 == NULL || aggregate->reasons == NULL) {
    t64_base_diag_aggregate_destroy(aggregate);
    return T64_BASE_DIAG_ALLOCATION_FAILED;
  }
  aggregate->inventory = inventory;
  *aggregate_out = aggregate;
  return T64_BASE_DIAG_OK;
}

t64_base_diag_status t64_base_diag_aggregate_create_bound(
    const t64_base_diag_inventory *inventory,
    const t64_base_diag_target *expected_target,
    t64_base_diag_aggregate **aggregate_out) {
  t64_base_diag_status status;
  t64_base_diag_aggregate *aggregate;
  if (aggregate_out != NULL) *aggregate_out = NULL;
  if (!valid_target(expected_target)) return T64_BASE_DIAG_INVALID_ARGUMENT;
  status = t64_base_diag_aggregate_create(inventory, aggregate_out);
  if (status != T64_BASE_DIAG_OK) return status;
  aggregate = *aggregate_out;
  aggregate->bound = true;
  memcpy(aggregate->context_sha256, expected_target->context_sha256, 65u);
  memcpy(aggregate->profile_sha256, expected_target->profile_sha256, 65u);
  memcpy(aggregate->profile_id, expected_target->profile_id,
           strlen(expected_target->profile_id) + 1u);
  memcpy(aggregate->core_build_id, expected_target->core_build_id,
           strlen(expected_target->core_build_id) + 1u);
  return T64_BASE_DIAG_OK;
}

void t64_base_diag_aggregate_destroy(t64_base_diag_aggregate *aggregate) {
  if (aggregate == NULL) return;
  t64_diag_partition_destroy(aggregate->partition);
  free(aggregate->original_sha256);
  free(aggregate->record_sha256);
  free(aggregate->selected_rom_sha256);
  free(aggregate->expected_rom_sha256);
  free(aggregate->reasons);
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

t64_base_diag_status t64_base_diag_aggregate_bind_image(
    t64_base_diag_aggregate *aggregate, uint32_t registration_index,
    uint32_t value_index, const char *expected_rom_sha256) {
  size_t ordinal;
  if (aggregate == NULL) return T64_BASE_DIAG_INVALID_ARGUMENT;
  if (!aggregate->bound || aggregate->invalid ||
      !lower_hex_digest(expected_rom_sha256)) {
    aggregate->invalid = true;
    return T64_BASE_DIAG_INVALID_ARGUMENT;
  }
  if (!find_member(aggregate->inventory, registration_index, value_index,
                    &ordinal) ||
      strcmp(aggregate->inventory->members[ordinal].public.execution,
               "blocked") == 0) {
    aggregate->invalid = true;
    return T64_BASE_DIAG_IDENTITY_MISMATCH;
  }
  if (aggregate->expected_rom_sha256[ordinal][0] != '\0') {
    aggregate->invalid = true;
    return T64_BASE_DIAG_DUPLICATE_MEMBER;
  }
  memcpy(aggregate->expected_rom_sha256[ordinal], expected_rom_sha256,
           T64_SHA256_HEX_SIZE);
  return T64_BASE_DIAG_OK;
}

static bool bound_execution_valid(
    const t64_base_diag_aggregate *aggregate, const t64_json_node *root,
    const t64_base_diag_member *member, t64_base_diag_outcome outcome,
    size_t original_length, const char *expected_rom_sha256,
    t64_base_diag_execution *execution,
    const char **owned_reason) {
  static const char *const reasons[] = {
      "not-selected", "interrupted", "window-exhausted", "host-timeout",
      "guest-tick-bound", "incomplete-transcript", "cpu-unsupported",
      "unsupported-device", "adapter-harness-error", "cleanup-unverified",
      "output-overflow", "input-refused"};
  const char *reason;
  const char *rom;
  size_t index;
  if (!t64_diag_json_string_equals(t64_json_get(root, "context_sha256"),
                                    aggregate->context_sha256) ||
      !t64_diag_json_string_equals(t64_json_get(root, "profile_id"),
                                    aggregate->profile_id) ||
      !t64_diag_json_string_equals(t64_json_get(root, "profile_sha256"),
                                    aggregate->profile_sha256) ||
      !t64_diag_json_string_equals(t64_json_get(root, "core_build_id"),
                                    aggregate->core_build_id) ||
      !text_member(root, "selected_rom_sha256", 64u, true, &rom) ||
      !text_member(root, "reason", T64_BASE_DIAG_MAX_TEXT_BYTES, true, &reason) ||
      !decimal_u64(root, "tick", &execution->tick) ||
      execution->tick > UINT64_C(10000000) ||
      !decimal_u64(root, "capture_total", &execution->capture_total) ||
      !bool_member(root, "capture_exact", &execution->capture_exact) ||
      !bool_member(root, "capture_lost", &execution->capture_lost) ||
      !bool_member(root, "entry_reached", &execution->entry_reached) ||
      !u32_member(root, "park_sequence", &execution->park_sequence) ||
      execution->park_sequence > 6u ||
      !u32_member(root, "terminal_pc", &execution->terminal_pc) ||
      !u32_member(root, "terminal_opcode", &execution->terminal_opcode) ||
      !u32_member(root, "terminal_delay_opcode", &execution->terminal_delay_opcode))
    return false;
  execution->selected_rom_sha256 = rom;
  execution->reason = reason;
  if (outcome == T64_BASE_DIAG_OUTCOME_BLOCKED ||
      outcome == T64_BASE_DIAG_OUTCOME_UNEXECUTED) {
    if (rom[0] != '\0' || execution->tick != 0u || original_length != 0u ||
        execution->capture_total != 0u || !execution->capture_exact ||
        execution->capture_lost || execution->entry_reached ||
        execution->park_sequence != 0u || execution->terminal_pc != 0u ||
        execution->terminal_opcode != 0u || execution->terminal_delay_opcode != 0u)
      return false;
    if (outcome == T64_BASE_DIAG_OUTCOME_BLOCKED) {
      if (strcmp(member->execution, "blocked") != 0 ||
          strcmp(reason, member->reason) != 0) return false;
      *owned_reason = member->reason;
      return true;
    }
    if (strcmp(member->execution, "blocked") == 0) return false;
    for (index = 0u; index < 3u; index++) {
      if (strcmp(reason, reasons[index]) == 0) {
        *owned_reason = reasons[index];
        return true;
      }
    }
    return false;
  }
  if (outcome == T64_BASE_DIAG_OUTCOME_NOT_RUN ||
      strcmp(member->execution, "blocked") == 0 || !lower_hex_digest(rom) ||
      strcmp(rom, expected_rom_sha256) != 0)
    return false;
  if (outcome == T64_BASE_DIAG_OUTCOME_PASS ||
      outcome == T64_BASE_DIAG_OUTCOME_FAIL) {
    if (!execution->capture_exact || execution->capture_lost ||
        execution->capture_total != (uint64_t)original_length ||
        !execution->entry_reached || execution->tick == 0u ||
        execution->park_sequence < 4u ||
        execution->terminal_pc != UINT32_C(0x80000420) ||
        execution->terminal_opcode != UINT32_C(0x08000108) ||
        execution->terminal_delay_opcode != 0u || reason[0] != '\0') return false;
    *owned_reason = "";
    return true;
  }
  for (index = 3u; index < sizeof reasons / sizeof reasons[0]; index++) {
    if (strcmp(reason, reasons[index]) == 0) {
      bool allowed =
          (outcome == T64_BASE_DIAG_OUTCOME_TIMEOUT && index == 3u) ||
          (outcome == T64_BASE_DIAG_OUTCOME_INCOMPLETE &&
           (index == 4u || index == 5u)) ||
          ((outcome == T64_BASE_DIAG_OUTCOME_GUEST_STOP ||
            outcome == T64_BASE_DIAG_OUTCOME_REFUSED) &&
           (index == 4u || index == 6u || index == 7u)) ||
          (outcome == T64_BASE_DIAG_OUTCOME_HARNESS_ERROR && index >= 8u);
      if (!allowed) return false;
      *owned_reason = reasons[index];
      return true;
    }
  }
  return false;
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
  const char *owned_reason = "";
  t64_base_diag_execution execution = {0};
  static const char *const outcome_names[] = {
      "pass", "fail", "incomplete", "not-run", "guest-stop",
      "harness-error", "timeout", "blocked", "unexecuted", "refused"};
  size_t i;
  if (aggregate == NULL || record == NULL || original_bytes == NULL) {
    return T64_BASE_DIAG_INVALID_ARGUMENT;
  }
  if (record_length == 0u || record_length > T64_BASE_DIAG_MAX_RECORD_BYTES) {
    aggregate->invalid = true;
    return T64_BASE_DIAG_SIZE_LIMIT;
  }
  if (original_length > T64_BASE_DIAG_MAX_ORIGINAL_BYTES) {
    aggregate->invalid = true;
    return T64_BASE_DIAG_SIZE_LIMIT;
  }
  if (!json_document(record, record_length, T64_BASE_DIAG_MAX_RECORD_BYTES,
                     64u, &document, &status)) {
    aggregate->invalid = true;
    goto cleanup;
  }
  if (!(aggregate->bound
            ? (exact_keys(document.root, bound_record_keys,
                            sizeof bound_record_keys / sizeof bound_record_keys[0]) &&
               t64_diag_json_string_equals(t64_json_get(document.root, "schema"),
                                           "t64.base-member-record") &&
               t64_json_get(document.root, "schema_version") != NULL &&
               t64_json_get(document.root, "schema_version")->type == T64_JSON_INT &&
               t64_json_get(document.root, "schema_version")->num == 2)
            : (exact_keys(document.root, record_keys,
                            sizeof record_keys / sizeof record_keys[0]) &&
               schema_version(document.root, "t64.base-member-record"))) ||
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
  for (i = 0u; i < (aggregate->bound ? 10u : 6u); i++) {
    if (outcome_name != NULL && strcmp(outcome_name, outcome_names[i]) == 0)
      break;
  }
  if (i == (aggregate->bound ? 10u : 6u)) {
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
  if (aggregate->bound &&
      !bound_execution_valid(aggregate, document.root,
          &aggregate->inventory->members[ordinal].public, outcome,
          original_length, aggregate->expected_rom_sha256[ordinal],
          &execution, &owned_reason)) {
    aggregate->invalid = true;
    status = T64_BASE_DIAG_IDENTITY_MISMATCH;
    goto cleanup;
  }
  if (outcome == T64_BASE_DIAG_OUTCOME_PASS ||
      outcome == T64_BASE_DIAG_OUTCOME_FAIL) {
    t64_cpu_diag_guest_result guest;
    if (t64_cpu_diag_guest_transcript_parse(original_bytes, original_length,
            registration_index, value_index, &guest) != T64_CPU_DIAG_OK ||
        (outcome == T64_BASE_DIAG_OUTCOME_PASS &&
         guest.outcome != T64_CPU_DIAG_OUTCOME_PASS) ||
        (outcome == T64_BASE_DIAG_OUTCOME_FAIL &&
         guest.outcome != T64_CPU_DIAG_OUTCOME_FAIL)) {
      aggregate->invalid = true;
      status = T64_BASE_DIAG_INVALID_RECORD;
      goto cleanup;
    }
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
  sha256_hex(record, record_length, aggregate->record_sha256[ordinal]);
  aggregate->reasons[ordinal] = owned_reason;
  if (aggregate->bound && execution.selected_rom_sha256[0] != '\0') {
    memcpy(aggregate->selected_rom_sha256[ordinal],
             execution.selected_rom_sha256, T64_SHA256_HEX_SIZE);
  }
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
    if ((aggregate->bound || member->required) &&
        !t64_diag_partition_seen(aggregate->partition, i))
      return false;
  }
  return true;
}

bool t64_base_diag_aggregate_is_all_pass(
    const t64_base_diag_aggregate *aggregate) {
  size_t i;
  if (!t64_base_diag_aggregate_is_complete(aggregate)) return false;
  if (!t64_base_diag_aggregate_execution_complete(aggregate))
    return false;
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

bool t64_base_diag_aggregate_execution_complete(
    const t64_base_diag_aggregate *aggregate) {
  size_t index;
  size_t required = 0u;
  if (aggregate == NULL || !aggregate->bound ||
      !t64_base_diag_aggregate_is_complete(aggregate)) return false;
  for (index = 0u; index < aggregate->inventory->member_count; index++) {
    uint8_t outcome;
    if (!aggregate->inventory->members[index].public.required) continue;
    required++;
    if (!t64_diag_partition_outcome(aggregate->partition, index, &outcome) ||
        (outcome != T64_BASE_DIAG_OUTCOME_PASS &&
         outcome != T64_BASE_DIAG_OUTCOME_FAIL)) return false;
  }
  return required != 0u;
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

typedef struct {
  size_t expected;
  size_t classified;
  size_t passed;
  size_t failed;
  size_t incomplete;
  size_t refused;
  size_t harness_error;
  size_t timeout;
  size_t blocked;
  size_t unexecuted;
} base_stats;

typedef struct {
  const char *group;
  const char *lane;
  size_t ordinal;
} base_group_ref;

static int compare_group_refs(const void *left, const void *right) {
  const base_group_ref *a = (const base_group_ref *)left;
  const base_group_ref *b = (const base_group_ref *)right;
  int comparison = strcmp(a->group, b->group);
  if (comparison == 0) comparison = strcmp(a->lane, b->lane);
  if (comparison != 0) return comparison;
  return (a->ordinal > b->ordinal) - (a->ordinal < b->ordinal);
}

static bool accumulate_stats(base_stats *stats,
                              const t64_base_diag_aggregate *aggregate,
                              size_t ordinal) {
  uint8_t outcome;
  stats->expected++;
  if (!t64_diag_partition_outcome(aggregate->partition, ordinal, &outcome))
    return true;
  stats->classified++;
  switch ((t64_base_diag_outcome)outcome) {
  case T64_BASE_DIAG_OUTCOME_PASS: stats->passed++; break;
  case T64_BASE_DIAG_OUTCOME_FAIL: stats->failed++; break;
  case T64_BASE_DIAG_OUTCOME_INCOMPLETE: stats->incomplete++; break;
  case T64_BASE_DIAG_OUTCOME_GUEST_STOP:
  case T64_BASE_DIAG_OUTCOME_REFUSED: stats->refused++; break;
  case T64_BASE_DIAG_OUTCOME_HARNESS_ERROR: stats->harness_error++; break;
  case T64_BASE_DIAG_OUTCOME_TIMEOUT: stats->timeout++; break;
  case T64_BASE_DIAG_OUTCOME_BLOCKED: stats->blocked++; break;
  case T64_BASE_DIAG_OUTCOME_NOT_RUN:
  case T64_BASE_DIAG_OUTCOME_UNEXECUTED: stats->unexecuted++; break;
  default: return false;
  }
  return true;
}

static bool write_stats(t64_jsonw *writer, const base_stats *stats) {
  bool ok = t64_jsonw_begin_object(writer) == T64_JSONW_OK;
#define STAT(key, value) do { ok = ok && t64_diag_json_write_key(writer, key) && t64_jsonw_int(writer, (int64_t)(value)) == T64_JSONW_OK; } while (0)
  STAT("blocked", stats->blocked);
  STAT("classified", stats->classified);
  STAT("expected", stats->expected);
  STAT("failed", stats->failed);
  STAT("harness_error", stats->harness_error);
  STAT("incomplete", stats->incomplete);
  STAT("missing", stats->expected - stats->classified);
  STAT("passed", stats->passed);
  STAT("reached", stats->classified - stats->blocked - stats->unexecuted);
  STAT("refused", stats->refused);
  STAT("timeout", stats->timeout);
  STAT("unexecuted", stats->unexecuted);
#undef STAT
  return ok && t64_jsonw_end_object(writer) == T64_JSONW_OK;
}

t64_base_diag_status t64_base_diag_aggregate_report(
    const t64_base_diag_aggregate *aggregate, uint8_t *output,
    size_t capacity, size_t *length_out) {
  t64_jsonw writer;
  base_group_ref *groups;
  base_stats totals = {0};
  size_t count;
  size_t index;
  bool ok;
  if (length_out != NULL) *length_out = 0u;
  if (aggregate == NULL || !aggregate->bound || output == NULL ||
      length_out == NULL) return T64_BASE_DIAG_INVALID_ARGUMENT;
  if (aggregate->invalid) return T64_BASE_DIAG_INVALID_RECORD;
  count = aggregate->inventory->member_count;
  groups = (base_group_ref *)calloc(count, sizeof *groups);
  if (groups == NULL) return T64_BASE_DIAG_ALLOCATION_FAILED;
  for (index = 0u; index < count; index++) {
    groups[index].group = aggregate->inventory->members[index].public.group;
    groups[index].lane = aggregate->inventory->members[index].public.lane;
    groups[index].ordinal = index;
    if (!accumulate_stats(&totals, aggregate, index)) {
      free(groups);
      return T64_BASE_DIAG_INVALID_RECORD;
    }
  }
  qsort(groups, count, sizeof *groups, compare_group_refs);
  if (capacity > T64_BASE_DIAG_MAX_REPORT_BYTES)
    capacity = T64_BASE_DIAG_MAX_REPORT_BYTES;
  t64_jsonw_init(&writer, (char *)output, capacity);
  ok = t64_jsonw_begin_object(&writer) == T64_JSONW_OK;
#define RKEY(key) (ok = ok && t64_diag_json_write_key(&writer, key))
#define RSTR(key, value) do { RKEY(key); ok = ok && t64_diag_json_write_string(&writer, value); } while (0)
#define RINT(key, value) do { RKEY(key); ok = ok && t64_jsonw_int(&writer, (int64_t)(value)) == T64_JSONW_OK; } while (0)
#define RBOOL(key, value) do { RKEY(key); ok = ok && t64_jsonw_bool(&writer, value) == T64_JSONW_OK; } while (0)
  RBOOL("all_pass", t64_base_diag_aggregate_is_all_pass(aggregate));
  RBOOL("authenticated_execution_complete",
         t64_base_diag_aggregate_execution_complete(aggregate));
  RSTR("context_sha256", aggregate->context_sha256);
  RKEY("counts"); ok = ok && write_stats(&writer, &totals);
  RKEY("groups"); ok = ok && t64_jsonw_begin_array(&writer) == T64_JSONW_OK;
  index = 0u;
  while (index < count && ok) {
    size_t first = index;
    base_stats stats = {0};
    do {
      if (!accumulate_stats(&stats, aggregate, groups[index].ordinal)) ok = false;
      index++;
    } while (index < count && strcmp(groups[first].group, groups[index].group) == 0 &&
             strcmp(groups[first].lane, groups[index].lane) == 0);
    ok = ok && t64_jsonw_begin_object(&writer) == T64_JSONW_OK;
    RBOOL("all_pass", stats.expected != 0u && stats.passed == stats.expected);
    RBOOL("authenticated_execution_complete", stats.expected != 0u &&
          stats.passed + stats.failed == stats.expected);
    RKEY("counts"); ok = ok && write_stats(&writer, &stats);
    RSTR("group", groups[first].group);
    RSTR("lane", groups[first].lane);
    RBOOL("source_complete", stats.classified == stats.expected);
    ok = ok && t64_jsonw_end_object(&writer) == T64_JSONW_OK;
  }
  ok = ok && t64_jsonw_end_array(&writer) == T64_JSONW_OK;
  RSTR("inventory_sha256", aggregate->inventory->sha256);
  RKEY("members"); ok = ok && t64_jsonw_begin_array(&writer) == T64_JSONW_OK;
  for (index = 0u; index < count && ok; index++) {
    const t64_base_diag_member *member = &aggregate->inventory->members[index].public;
    uint8_t outcome = 0u;
    bool seen = t64_diag_partition_outcome(aggregate->partition, index, &outcome);
    ok = ok && t64_jsonw_begin_object(&writer) == T64_JSONW_OK;
    RSTR("execution", member->execution);
    RSTR("expected_rom_sha256", aggregate->expected_rom_sha256[index]);
    RSTR("group", member->group);
    RSTR("lane", member->lane);
    RSTR("name", member->name);
    RSTR("original_sha256", seen ? aggregate->original_sha256[index] : "");
    RSTR("outcome", seen ? t64_base_diag_outcome_name((t64_base_diag_outcome)outcome) : "missing");
    RSTR("reason", seen ? aggregate->reasons[index] : "");
    RSTR("record_sha256", seen ? aggregate->record_sha256[index] : "");
    RINT("registration_index", member->registration_index);
    RBOOL("required", member->required);
    RSTR("selected_rom_sha256", seen ? aggregate->selected_rom_sha256[index] : "");
    RSTR("value_id", member->value_id);
    RINT("value_index", member->value_index);
    ok = ok && t64_jsonw_end_object(&writer) == T64_JSONW_OK;
  }
  ok = ok && t64_jsonw_end_array(&writer) == T64_JSONW_OK;
  RSTR("schema", "t64.base-classification");
  RINT("schema_version", 1u);
  RBOOL("source_complete", t64_base_diag_aggregate_is_complete(aggregate));
  RKEY("target"); ok = ok && t64_jsonw_begin_object(&writer) == T64_JSONW_OK;
  RSTR("core_build_id", aggregate->core_build_id);
  RSTR("profile_id", aggregate->profile_id);
  RSTR("profile_sha256", aggregate->profile_sha256);
  ok = ok && t64_jsonw_end_object(&writer) == T64_JSONW_OK &&
       t64_jsonw_end_object(&writer) == T64_JSONW_OK &&
       t64_jsonw_finish(&writer, length_out) == T64_JSONW_OK;
#undef RKEY
#undef RSTR
#undef RINT
#undef RBOOL
  free(groups);
  if (!ok) return writer.err == T64_JSONW_E_OVERFLOW
                      ? T64_BASE_DIAG_OUTPUT_OVERFLOW
                      : T64_BASE_DIAG_CANONICALIZATION_FAILED;
  return T64_BASE_DIAG_OK;
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
      "harness-error", "timeout", "blocked", "unexecuted", "refused"};
  if ((size_t)outcome >= sizeof names / sizeof names[0]) return NULL;
  return names[(size_t)outcome];
}
