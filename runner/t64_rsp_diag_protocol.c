/* Closed, bounded RSP diagnostic inventory and authenticated batch protocol. */
#include "t64_rsp_diag_protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_diag_common.h"
#include "t64_sha256.h"

#define RSP_DIAG_MAX_JSON_NODES 262144u

typedef struct {
  uint32_t index;
  const char *id;
  const char *selector;
  const char *selected_rom_sha256;
} t64_rsp_diag_value;

typedef struct {
  uint32_t index;
  const char *group;
  const char *name;
  const char *source_name;
  size_t value_offset;
  size_t value_count;
} t64_rsp_diag_registration;

struct t64_rsp_diag_inventory {
  t64_json_doc document;
  t64_rsp_diag_identity identity;
  t64_rsp_diag_registration *registrations;
  t64_rsp_diag_value *values;
  size_t registration_count;
  size_t value_count;
  char sha256[T64_SHA256_HEX_SIZE];
};

struct t64_rsp_diag_aggregate {
  const t64_rsp_diag_inventory *inventory;
  t64_diag_partition *partition;
  struct t64_rsp_diag_owned_result *results;
  bool invalid;
};

typedef struct t64_rsp_diag_owned_result {
  t64_rsp_diag_member_result result;
  char operation[T64_RSP_DIAG_MAX_TEXT_BYTES + 1u];
  char reason[T64_RSP_DIAG_MAX_TEXT_BYTES + 1u];
  char result_sha256[T64_SHA256_HEX_SIZE];
  char capture_sha256[T64_SHA256_HEX_SIZE];
} t64_rsp_diag_owned_result;

static bool rsp_diag_identity_parse(const t64_json_node *node,
                                    t64_rsp_diag_identity *identity);

static const char *const rsp_diag_inventory_keys[] = {
    "component", "declared_counts", "identity", "registrations", "schema",
    "schema_version"};
static const char *const rsp_diag_count_keys[] = {
    "registrations", "required_values", "values"};
static const char *const rsp_diag_identity_keys[] = {
    "boot_sha256", "closure_sha256", "core_build_sha256",
    "core_source_revision", "overlay_sha256", "profile_descriptor_sha256", "profile_id",
    "qualification_profile_sha256", "qualification_sha256", "rom_sha256",
    "source_projection_sha256", "source_revision"};
static const char *const rsp_diag_registration_keys[] = {
    "constructor", "execution", "group", "index", "lane", "level",
    "name", "source_file", "source_file_sha256", "source_name",
    "source_value_count", "values", "values_method_sha256"};
static const char *const rsp_diag_value_keys[] = {
    "id", "index", "selected_rom_sha256", "selector"};
static const char *const rsp_diag_outcomes[] = {
    "pass", "guest-fail", "refused", "incomplete", "timeout",
    "harness-error"};
static const char *const rsp_diag_batch_keys[] = {
    "component", "identity", "member", "result", "schema",
    "schema_version"};
static const char *const rsp_diag_batch_identity_keys[] = {
    "boot_sha256", "closure_sha256", "core_build_sha256",
    "core_source_revision", "inventory_sha256", "overlay_sha256", "profile_descriptor_sha256",
    "profile_id", "qualification_profile_sha256", "qualification_sha256",
    "rom_sha256", "source_projection_sha256", "source_revision"};
static const char *const rsp_diag_batch_member_keys[] = {
    "group", "registration_index", "registration_name", "selected_rom_sha256",
    "selector", "source_name", "value_id", "value_index"};
static const char *const rsp_diag_batch_result_keys[] = {
    "capture_sha256", "operation", "outcome", "reason", "result_sha256",
    "retired_instructions", "terminal_delay_slot_retired", "terminal_opcode",
    "terminal_pc"};
static const char *const rsp_diag_batch_terminal_result_keys[] = {
    "capture_sha256", "operation", "outcome", "reason", "result_sha256",
    "retired_instructions", "terminal_delay_slot_opcode",
    "terminal_delay_slot_pc", "terminal_delay_slot_retired",
    "terminal_opcode", "terminal_pc"};

t64_rsp_diag_status t64_rsp_diag_guest_transcript_parse(
    const uint8_t *bytes, size_t length, uint32_t expected_registration_index,
    uint32_t expected_value_index, t64_rsp_diag_guest_result *result_out) {
  static const char *const root_keys[] = {"records", "schema", "schema_version"};
  static const char *const start_keys[] = {
      "event", "registration_index", "value_index"};
  static const char *const outcome_keys[] = {"event", "status"};
  static const char *const done_keys[] = {"complete", "event", "record_count"};
  t64_json_limits limits = {T64_RSP_DIAG_MAX_BATCH_BYTES, 8u, 64u, 32u};
  t64_json_error json_error = {T64_JSON_OK, 0u};
  t64_json_doc document = {0};
  const t64_json_node *records;
  const t64_json_node *record;
  const t64_json_node *field;
  uint32_t registration_index;
  uint32_t value_index;
  t64_rsp_diag_outcome outcome;
  t64_rsp_diag_status status = T64_RSP_DIAG_INVALID_BATCH;

  if (bytes == NULL || result_out == NULL)
    return T64_RSP_DIAG_INVALID_ARGUMENT;
  if (length == 0u || length > T64_RSP_DIAG_MAX_BATCH_BYTES)
    return T64_RSP_DIAG_SIZE_LIMIT;
  if (t64_json_parse(bytes, length, &limits, &document, &json_error) != 0)
    return T64_RSP_DIAG_INVALID_JSON;
  if (!t64_diag_json_object_has_exact_keys(
          document.root, root_keys, sizeof root_keys / sizeof root_keys[0]) ||
      !t64_diag_json_string_equals(t64_json_get(document.root, "schema"),
                                   "t64.rsp-run")) {
    status = T64_RSP_DIAG_INVALID_SCHEMA;
    goto cleanup;
  }
  {
    uint32_t version;
    if (!t64_diag_json_index16(t64_json_get(document.root, "schema_version"),
                               &version) ||
        version != T64_RSP_DIAG_GUEST_RUN_VERSION) {
      status = T64_RSP_DIAG_INVALID_SCHEMA;
      goto cleanup;
    }
  }
  if (!t64_diag_json_canonical_document(&document, bytes, length)) {
    status = T64_RSP_DIAG_INVALID_SCHEMA;
    goto cleanup;
  }
  records = t64_json_get(document.root, "records");
  if (records == NULL || records->type != T64_JSON_ARRAY ||
      records->count < 2u || records->count > 3u) {
    status = records != NULL && records->type == T64_JSON_ARRAY &&
                     records->count < 3u
                 ? T64_RSP_DIAG_INCOMPLETE
                 : T64_RSP_DIAG_INVALID_BATCH;
    goto cleanup;
  }
  record = records->first;
  if (record == NULL || !t64_diag_json_object_has_exact_keys(
                            record, start_keys,
                            sizeof start_keys / sizeof start_keys[0]) ||
      !t64_diag_json_string_equals(t64_json_get(record, "event"), "start") ||
      !t64_diag_json_index16(t64_json_get(record, "registration_index"),
                             &registration_index) ||
      !t64_diag_json_index16(t64_json_get(record, "value_index"),
                             &value_index)) {
    status = T64_RSP_DIAG_INVALID_BATCH;
    goto cleanup;
  }
  if (registration_index != expected_registration_index ||
      value_index != expected_value_index) {
    status = T64_RSP_DIAG_IDENTITY_MISMATCH;
    goto cleanup;
  }
  record = record->next;
  if (record == NULL || !t64_diag_json_object_has_exact_keys(
                            record, outcome_keys,
                            sizeof outcome_keys / sizeof outcome_keys[0]) ||
      !t64_diag_json_string_equals(t64_json_get(record, "event"), "outcome")) {
    status = T64_RSP_DIAG_INVALID_BATCH;
    goto cleanup;
  }
  field = t64_diag_json_string_member(record, "status");
  if (t64_diag_json_string_equals(field, "pass")) {
    outcome = T64_RSP_DIAG_OUTCOME_PASS;
  } else if (t64_diag_json_string_equals(field, "fail")) {
    outcome = T64_RSP_DIAG_OUTCOME_GUEST_FAIL;
  } else {
    status = T64_RSP_DIAG_INVALID_BATCH;
    goto cleanup;
  }
  record = record->next;
  if (record == NULL) {
    status = T64_RSP_DIAG_INCOMPLETE;
    goto cleanup;
  }
  if (!t64_diag_json_object_has_exact_keys(
          record, done_keys, sizeof done_keys / sizeof done_keys[0]) ||
      !t64_diag_json_string_equals(t64_json_get(record, "event"), "done")) {
    status = T64_RSP_DIAG_INVALID_BATCH;
    goto cleanup;
  }
  field = t64_json_get(record, "complete");
  if (field == NULL || field->type != T64_JSON_BOOL || field->num != 1 ||
      !t64_diag_json_string_equals(t64_json_get(record, "record_count"), "3")) {
    status = T64_RSP_DIAG_INCOMPLETE;
    goto cleanup;
  }
  if (record->next != NULL) {
    status = T64_RSP_DIAG_INVALID_BATCH;
    goto cleanup;
  }
  result_out->registration_index = registration_index;
  result_out->value_index = value_index;
  result_out->outcome = outcome;
  status = T64_RSP_DIAG_OK;
cleanup:
  t64_json_free(&document);
  return status;
}

static int rsp_diag_compare_selector_pointer(const void *left,
                                             const void *right) {
  const char *const *left_text = (const char *const *)left;
  const char *const *right_text = (const char *const *)right;
  return strcmp(*left_text, *right_text);
}

static bool rsp_diag_digest_text(const char *text) {
  size_t i;
  if (text == NULL || strlen(text) != 64u) return false;
  for (i = 0u; i < 64u; i++) {
    if (!((text[i] >= '0' && text[i] <= '9') ||
          (text[i] >= 'a' && text[i] <= 'f'))) return false;
  }
  return true;
}

static bool rsp_diag_text_value(const char *text, size_t maximum) {
  size_t i;
  size_t length;
  if (text == NULL) return false;
  length = strlen(text);
  if (length > maximum) return false;
  for (i = 0u; i < length; i++) {
    if ((unsigned char)text[i] < 0x20u || (unsigned char)text[i] > 0x7eu)
      return false;
  }
  return true;
}

static bool rsp_diag_identity_equal(const t64_rsp_diag_identity *left,
                                    const t64_rsp_diag_identity *right) {
  return left != NULL && right != NULL &&
      strcmp(left->source_revision, right->source_revision) == 0 &&
      strcmp(left->source_projection_sha256,
             right->source_projection_sha256) == 0 &&
      strcmp(left->qualification_sha256, right->qualification_sha256) == 0 &&
      strcmp(left->closure_sha256, right->closure_sha256) == 0 &&
      strcmp(left->overlay_sha256, right->overlay_sha256) == 0 &&
      strcmp(left->core_build_sha256, right->core_build_sha256) == 0 &&
      strcmp(left->core_source_revision, right->core_source_revision) == 0 &&
      strcmp(left->profile_descriptor_sha256,
             right->profile_descriptor_sha256) == 0 &&
      strcmp(left->qualification_profile_sha256,
             right->qualification_profile_sha256) == 0 &&
      strcmp(left->profile_id, right->profile_id) == 0 &&
      strcmp(left->rom_sha256, right->rom_sha256) == 0 &&
      strcmp(left->boot_sha256, right->boot_sha256) == 0;
}

static bool rsp_diag_valid_member(const t64_rsp_diag_inventory *inventory,
                                  uint32_t registration_index,
                                  uint32_t value_index, size_t *flat_out,
                                  t64_rsp_diag_member *member_out) {
  size_t i;
  if (inventory == NULL) return false;
  for (i = 0u; i < inventory->registration_count; i++) {
    const t64_rsp_diag_registration *registration =
        &inventory->registrations[i];
    if (registration->index == registration_index &&
        value_index < registration->value_count) {
      size_t flat = registration->value_offset + value_index;
      const t64_rsp_diag_value *value = &inventory->values[flat];
      if (flat_out != NULL) *flat_out = flat;
      if (member_out != NULL) {
        member_out->registration_index = registration_index;
        member_out->value_index = value_index;
        member_out->group = registration->group;
        member_out->registration_name = registration->name;
        member_out->source_name = registration->source_name;
        member_out->value_id = value->id;
        member_out->selector = value->selector;
        member_out->selected_rom_sha256 = value->selected_rom_sha256;
      }
      return true;
    }
  }
  return false;
}

static bool rsp_diag_parse_batch(
    const t64_rsp_diag_inventory *inventory, const uint8_t *bytes,
    size_t length, size_t *flat_out, t64_rsp_diag_owned_result *result_out) {
  t64_json_limits limits = {T64_RSP_DIAG_MAX_BATCH_BYTES, 6u, 64u, 256u};
  t64_json_doc document = {0};
  t64_json_error error = {T64_JSON_OK, 0u};
  t64_rsp_diag_identity identity = {0};
  t64_rsp_diag_member member = {0};
  t64_rsp_diag_member_result result = {0};
  const t64_json_node *node;
  const t64_json_node *field;
  const char *outcome_text;
  size_t i;
  uint32_t version;
  bool valid = false;
  if (inventory == NULL || bytes == NULL || flat_out == NULL || result_out == NULL ||
      length == 0u || length > T64_RSP_DIAG_MAX_BATCH_BYTES ||
      t64_json_parse(bytes, length, &limits, &document, &error) != 0) {
    return false;
  }
  node = document.root;
  if (!t64_diag_json_object_has_exact_keys(
          node, rsp_diag_batch_keys,
          sizeof rsp_diag_batch_keys / sizeof rsp_diag_batch_keys[0]) ||
      !t64_diag_json_string_equals(t64_json_get(node, "schema"),
                                   "t64.rsp.batch") ||
      !t64_diag_json_index16(t64_json_get(node, "schema_version"), &version) ||
      version != T64_RSP_DIAG_BATCH_VERSION ||
      !t64_diag_json_string_equals(t64_json_get(node, "component"), "rsp") ||
      !t64_diag_json_canonical_document(&document, bytes, length)) goto done;
  {
    const t64_json_node *identity_node = t64_json_get(node, "identity");
    const t64_json_node *inventory_hash =
        t64_json_get(identity_node, "inventory_sha256");
    if (!t64_diag_json_object_has_exact_keys(
            identity_node, rsp_diag_batch_identity_keys,
            sizeof rsp_diag_batch_identity_keys /
                sizeof rsp_diag_batch_identity_keys[0]) ||
        !t64_diag_json_lowercase_hex(inventory_hash, 64u) ||
        strcmp(inventory_hash->str, inventory->sha256) != 0) goto done;
    /* Parse the closed inventory identity after excluding its batch-only key. */
    {
      /* Batch identity adds a closed inventory hash to the inventory identity. */
      const t64_json_node *f;
#define BATCH_DIGEST(member, key) do { f = t64_json_get(identity_node, key); \
  if (!t64_diag_json_lowercase_hex(f, 64u)) goto done; identity.member = f->str; \
} while (0)
      BATCH_DIGEST(boot_sha256, "boot_sha256");
      BATCH_DIGEST(closure_sha256, "closure_sha256");
      BATCH_DIGEST(core_build_sha256, "core_build_sha256");
      f = t64_json_get(identity_node, "core_source_revision");
      if (!t64_diag_json_lowercase_hex(f, 40u)) goto done;
      identity.core_source_revision = f->str;
      BATCH_DIGEST(overlay_sha256, "overlay_sha256");
      BATCH_DIGEST(profile_descriptor_sha256, "profile_descriptor_sha256");
      BATCH_DIGEST(qualification_profile_sha256, "qualification_profile_sha256");
      BATCH_DIGEST(qualification_sha256, "qualification_sha256");
      BATCH_DIGEST(rom_sha256, "rom_sha256");
      BATCH_DIGEST(source_projection_sha256, "source_projection_sha256");
#undef BATCH_DIGEST
      f = t64_json_get(identity_node, "source_revision");
      if (!t64_diag_json_lowercase_hex(f, 40u)) goto done;
      identity.source_revision = f->str;
      f = t64_json_get(identity_node, "profile_id");
      if (!t64_diag_json_printable_text(f, 128u, false)) goto done;
      identity.profile_id = f->str;
    }
    if (!rsp_diag_identity_equal(&identity, &inventory->identity)) goto done;
  }
  node = t64_json_get(node, "member");
  if (!t64_diag_json_object_has_exact_keys(
          node, rsp_diag_batch_member_keys,
          sizeof rsp_diag_batch_member_keys / sizeof rsp_diag_batch_member_keys[0]) ||
      !t64_diag_json_index16(t64_json_get(node, "registration_index"),
                             &result.registration_index) ||
      !t64_diag_json_index16(t64_json_get(node, "value_index"),
                             &result.value_index) ||
      !rsp_diag_valid_member(inventory, result.registration_index,
                             result.value_index, flat_out, &member)) goto done;
  if (!t64_diag_json_string_equals(t64_json_get(node, "group"), member.group) ||
      !t64_diag_json_string_equals(t64_json_get(node, "registration_name"),
                                   member.registration_name) ||
      !t64_diag_json_string_equals(t64_json_get(node, "source_name"),
                                   member.source_name) ||
      !t64_diag_json_string_equals(t64_json_get(node, "value_id"),
                                   member.value_id)) goto done;
  node = t64_json_get(document.root, "member");
  if (!t64_diag_json_string_equals(t64_json_get(node, "selector"),
                                   member.selector) ||
      !t64_diag_json_string_equals(t64_json_get(node, "selected_rom_sha256"),
                                   member.selected_rom_sha256)) goto done;
  node = t64_json_get(document.root, "result");
  field = t64_json_get(node, "outcome");
  outcome_text = field != NULL && field->type == T64_JSON_STRING ? field->str : NULL;
  for (i = 0u; i < sizeof rsp_diag_outcomes / sizeof rsp_diag_outcomes[0]; i++) {
    if (outcome_text != NULL && strcmp(outcome_text, rsp_diag_outcomes[i]) == 0) {
      result.outcome = (t64_rsp_diag_outcome)i;
      break;
    }
  }
  if (i == sizeof rsp_diag_outcomes / sizeof rsp_diag_outcomes[0] ||
      (result.outcome == T64_RSP_DIAG_OUTCOME_PASS ||
       result.outcome == T64_RSP_DIAG_OUTCOME_GUEST_FAIL
           ? !t64_diag_json_object_has_exact_keys(
                 node, rsp_diag_batch_terminal_result_keys,
                 sizeof rsp_diag_batch_terminal_result_keys /
                     sizeof rsp_diag_batch_terminal_result_keys[0])
           : (!t64_diag_json_object_has_exact_keys(
                  node, rsp_diag_batch_result_keys,
                  sizeof rsp_diag_batch_result_keys /
                      sizeof rsp_diag_batch_result_keys[0]) &&
              !t64_diag_json_object_has_exact_keys(
                  node, rsp_diag_batch_terminal_result_keys,
                  sizeof rsp_diag_batch_terminal_result_keys /
                      sizeof rsp_diag_batch_terminal_result_keys[0]))) ||
      !t64_diag_json_lowercase_hex(t64_json_get(node, "result_sha256"), 64u) ||
      !t64_diag_json_lowercase_hex(t64_json_get(node, "capture_sha256"), 64u)) goto done;
  result.result_sha256 = t64_json_get(node, "result_sha256")->str;
  result.capture_sha256 = t64_json_get(node, "capture_sha256")->str;
  field = t64_json_get(node, "operation");
  if (!t64_diag_json_printable_text(field, T64_RSP_DIAG_MAX_TEXT_BYTES, true)) goto done;
  result.operation = field->str;
  field = t64_json_get(node, "reason");
  if (!t64_diag_json_printable_text(field, T64_RSP_DIAG_MAX_TEXT_BYTES, true)) goto done;
  result.reason = field->str;
  field = t64_json_get(node, "retired_instructions");
  if (field == NULL || field->type != T64_JSON_INT || field->num < 0) goto done;
  result.retired_instructions = (uint64_t)field->num;
  field = t64_json_get(node, "terminal_pc");
  if (field == NULL || field->type != T64_JSON_INT || field->num < 0 ||
      (uint64_t)field->num > UINT32_MAX) goto done;
  result.terminal_pc = (uint32_t)field->num;
  field = t64_json_get(node, "terminal_delay_slot_pc");
  if (field == NULL) {
    if (result.outcome == T64_RSP_DIAG_OUTCOME_PASS ||
        result.outcome == T64_RSP_DIAG_OUTCOME_GUEST_FAIL) goto done;
  } else {
    if (field->type != T64_JSON_INT || field->num < 0 ||
        (uint64_t)field->num > UINT32_MAX) goto done;
    result.terminal_delay_slot_pc = (uint32_t)field->num;
  }
  field = t64_json_get(node, "terminal_delay_slot_opcode");
  if (field == NULL) {
    if (result.outcome == T64_RSP_DIAG_OUTCOME_PASS ||
        result.outcome == T64_RSP_DIAG_OUTCOME_GUEST_FAIL) goto done;
  } else {
    if (field->type != T64_JSON_INT || field->num < 0 ||
        (uint64_t)field->num > UINT32_MAX) goto done;
    result.terminal_delay_slot_opcode = (uint32_t)field->num;
  }
  field = t64_json_get(node, "terminal_opcode");
  if (field == NULL || field->type != T64_JSON_INT || field->num < 0 ||
      (uint64_t)field->num > UINT32_MAX) goto done;
  result.terminal_opcode = (uint32_t)field->num;
  field = t64_json_get(node, "terminal_delay_slot_retired");
  if (field == NULL || field->type != T64_JSON_BOOL) goto done;
  result.terminal_delay_slot_retired = field->num != 0;
  if ((result.outcome == T64_RSP_DIAG_OUTCOME_PASS &&
       (result.retired_instructions == 0u ||
        !result.terminal_delay_slot_retired || result.operation[0] != '\0' ||
        result.reason[0] != '\0')) ||
      (result.outcome == T64_RSP_DIAG_OUTCOME_GUEST_FAIL &&
       (result.retired_instructions == 0u ||
        !result.terminal_delay_slot_retired ||
        strcmp(result.operation, "guest-assertion") != 0 ||
        result.reason[0] == '\0')) ||
      (result.terminal_delay_slot_retired &&
       (result.terminal_pc != T64_RSP_DIAG_TERMINAL_PC ||
        result.terminal_opcode != T64_RSP_DIAG_TERMINAL_OPCODE ||
        result.terminal_delay_slot_pc != T64_RSP_DIAG_TERMINAL_PC + 4u ||
        result.terminal_delay_slot_opcode !=
            T64_RSP_DIAG_TERMINAL_DELAY_SLOT_OPCODE)) ||
      (result.outcome == T64_RSP_DIAG_OUTCOME_REFUSED &&
       (result.operation[0] == '\0' || result.reason[0] == '\0')) ||
      ((result.outcome == T64_RSP_DIAG_OUTCOME_INCOMPLETE ||
        result.outcome == T64_RSP_DIAG_OUTCOME_TIMEOUT ||
        result.outcome == T64_RSP_DIAG_OUTCOME_HARNESS_ERROR) &&
       result.reason[0] == '\0')) goto done;
  (void)memcpy(result_out->operation, result.operation,
               strlen(result.operation) + 1u);
  (void)memcpy(result_out->reason, result.reason, strlen(result.reason) + 1u);
  (void)memcpy(result_out->result_sha256, result.result_sha256,
               strlen(result.result_sha256) + 1u);
  (void)memcpy(result_out->capture_sha256, result.capture_sha256,
               strlen(result.capture_sha256) + 1u);
  result_out->result = result;
  result_out->result.operation = result_out->operation;
  result_out->result.reason = result_out->reason;
  result_out->result.result_sha256 = result_out->result_sha256;
  result_out->result.capture_sha256 = result_out->capture_sha256;
  valid = true;
done:
  t64_json_free(&document);
  return valid;
}

static bool rsp_diag_write_identity(t64_jsonw *writer,
                                    const t64_rsp_diag_inventory *inventory) {
  const t64_rsp_diag_identity *identity = &inventory->identity;
  return t64_jsonw_begin_object(writer) == T64_JSONW_OK &&
      t64_diag_json_write_key(writer, "boot_sha256") &&
      t64_diag_json_write_string(writer, identity->boot_sha256) &&
      t64_diag_json_write_key(writer, "closure_sha256") &&
      t64_diag_json_write_string(writer, identity->closure_sha256) &&
      t64_diag_json_write_key(writer, "core_build_sha256") &&
      t64_diag_json_write_string(writer, identity->core_build_sha256) &&
      t64_diag_json_write_key(writer, "core_source_revision") &&
      t64_diag_json_write_string(writer, identity->core_source_revision) &&
      t64_diag_json_write_key(writer, "inventory_sha256") &&
      t64_diag_json_write_string(writer, inventory->sha256) &&
      t64_diag_json_write_key(writer, "overlay_sha256") &&
      t64_diag_json_write_string(writer, identity->overlay_sha256) &&
      t64_diag_json_write_key(writer, "profile_descriptor_sha256") &&
      t64_diag_json_write_string(writer, identity->profile_descriptor_sha256) &&
      t64_diag_json_write_key(writer, "profile_id") &&
      t64_diag_json_write_string(writer, identity->profile_id) &&
      t64_diag_json_write_key(writer, "qualification_profile_sha256") &&
      t64_diag_json_write_string(writer, identity->qualification_profile_sha256) &&
      t64_diag_json_write_key(writer, "qualification_sha256") &&
      t64_diag_json_write_string(writer, identity->qualification_sha256) &&
      t64_diag_json_write_key(writer, "rom_sha256") &&
      t64_diag_json_write_string(writer, identity->rom_sha256) &&
      t64_diag_json_write_key(writer, "source_projection_sha256") &&
      t64_diag_json_write_string(writer, identity->source_projection_sha256) &&
      t64_diag_json_write_key(writer, "source_revision") &&
      t64_diag_json_write_string(writer, identity->source_revision) &&
      t64_jsonw_end_object(writer) == T64_JSONW_OK;
}

t64_rsp_diag_status t64_rsp_diag_batch_encode(
    const t64_rsp_diag_inventory *inventory, uint32_t registration_index,
    uint32_t value_index, const t64_rsp_diag_member_result *result,
    char *bytes_out, size_t capacity, size_t *length_out) {
  t64_rsp_diag_member member;
  t64_jsonw writer;
  int64_t retired;
  if (inventory == NULL || result == NULL || bytes_out == NULL ||
      length_out == NULL || result->outcome > T64_RSP_DIAG_OUTCOME_HARNESS_ERROR ||
      !rsp_diag_valid_member(inventory, registration_index, value_index,
                             NULL, &member) ||
      !rsp_diag_digest_text(result->result_sha256) ||
      !rsp_diag_digest_text(result->capture_sha256) ||
      result->retired_instructions > (uint64_t)INT64_MAX ||
      result->operation == NULL ||
      result->reason == NULL ||
      (result->outcome == T64_RSP_DIAG_OUTCOME_PASS &&
       (result->retired_instructions == 0u ||
        !result->terminal_delay_slot_retired || result->operation[0] != '\0' ||
        result->reason[0] != '\0')) ||
      (result->outcome == T64_RSP_DIAG_OUTCOME_GUEST_FAIL &&
       (result->retired_instructions == 0u ||
        !result->terminal_delay_slot_retired ||
        strcmp(result->operation, "guest-assertion") != 0 ||
        result->reason[0] == '\0')) ||
      (result->terminal_delay_slot_retired &&
       (result->terminal_pc != T64_RSP_DIAG_TERMINAL_PC ||
        result->terminal_opcode != T64_RSP_DIAG_TERMINAL_OPCODE ||
        result->terminal_delay_slot_pc != T64_RSP_DIAG_TERMINAL_PC + 4u ||
        result->terminal_delay_slot_opcode !=
            T64_RSP_DIAG_TERMINAL_DELAY_SLOT_OPCODE)) ||
      (result->outcome == T64_RSP_DIAG_OUTCOME_REFUSED &&
       (result->operation[0] == '\0' || result->reason[0] == '\0')) ||
      ((result->outcome == T64_RSP_DIAG_OUTCOME_INCOMPLETE ||
        result->outcome == T64_RSP_DIAG_OUTCOME_TIMEOUT ||
        result->outcome == T64_RSP_DIAG_OUTCOME_HARNESS_ERROR) &&
       result->reason[0] == '\0')) {
    return T64_RSP_DIAG_INVALID_ARGUMENT;
  }
  if (!rsp_diag_text_value(result->operation, T64_RSP_DIAG_MAX_TEXT_BYTES) ||
      !rsp_diag_text_value(result->reason, T64_RSP_DIAG_MAX_TEXT_BYTES)) {
    return T64_RSP_DIAG_INVALID_ARGUMENT;
  }
  retired = (int64_t)result->retired_instructions;
  t64_jsonw_init(&writer, bytes_out, capacity);
#define WKEY(key) t64_diag_json_write_key(&writer, key)
#define WSTR(value) t64_diag_json_write_string(&writer, value)
#define WINT(value) (t64_jsonw_int(&writer, (int64_t)(value)) == T64_JSONW_OK)
  if (t64_jsonw_begin_object(&writer) != T64_JSONW_OK ||
      !WKEY("component") || !WSTR("rsp") || !WKEY("identity") ||
      !rsp_diag_write_identity(&writer, inventory) || !WKEY("member") ||
      t64_jsonw_begin_object(&writer) != T64_JSONW_OK ||
      !WKEY("group") || !WSTR(member.group) || !WKEY("registration_index") ||
      !WINT(registration_index) || !WKEY("registration_name") ||
      !WSTR(member.registration_name) || !WKEY("selected_rom_sha256") ||
      !WSTR(member.selected_rom_sha256) || !WKEY("selector") ||
      !WSTR(member.selector) || !WKEY("source_name") ||
      !WSTR(member.source_name) || !WKEY("value_id") ||
      !WSTR(member.value_id) || !WKEY("value_index") || !WINT(value_index) ||
      t64_jsonw_end_object(&writer) != T64_JSONW_OK || !WKEY("result") ||
      t64_jsonw_begin_object(&writer) != T64_JSONW_OK ||
      !WKEY("capture_sha256") || !WSTR(result->capture_sha256) ||
      !WKEY("operation") || !WSTR(result->operation) || !WKEY("outcome") ||
      !WSTR(t64_rsp_diag_outcome_name(result->outcome)) || !WKEY("reason") ||
      !WSTR(result->reason) || !WKEY("result_sha256") ||
      !WSTR(result->result_sha256) || !WKEY("retired_instructions") ||
      !WINT(retired) || !WKEY("terminal_delay_slot_opcode") ||
      !WINT(result->terminal_delay_slot_opcode) ||
      !WKEY("terminal_delay_slot_pc") ||
      !WINT(result->terminal_delay_slot_pc) ||
      !WKEY("terminal_delay_slot_retired") ||
      t64_jsonw_bool(&writer, result->terminal_delay_slot_retired ? 1 : 0) !=
          T64_JSONW_OK || !WKEY("terminal_opcode") ||
      !WINT(result->terminal_opcode) || !WKEY("terminal_pc") ||
      !WINT(result->terminal_pc) ||
      t64_jsonw_end_object(&writer) != T64_JSONW_OK || !WKEY("schema") ||
      !WSTR("t64.rsp.batch") || !WKEY("schema_version") ||
      !WINT(T64_RSP_DIAG_BATCH_VERSION) ||
      t64_jsonw_end_object(&writer) != T64_JSONW_OK ||
      t64_jsonw_finish(&writer, length_out) != T64_JSONW_OK) {
#undef WKEY
#undef WSTR
#undef WINT
    return writer.err == T64_JSONW_E_OVERFLOW ? T64_RSP_DIAG_OUTPUT_OVERFLOW
                                             : T64_RSP_DIAG_CANONICALIZATION_FAILED;
  }
#undef WKEY
#undef WSTR
#undef WINT
  return T64_RSP_DIAG_OK;
}

t64_rsp_diag_status t64_rsp_diag_aggregate_create(
    const t64_rsp_diag_inventory *inventory,
    t64_rsp_diag_aggregate **aggregate_out) {
  t64_rsp_diag_aggregate *aggregate;
  if (inventory == NULL || aggregate_out == NULL || inventory->value_count == 0u)
    return T64_RSP_DIAG_INVALID_ARGUMENT;
  aggregate = (t64_rsp_diag_aggregate *)calloc(1u, sizeof *aggregate);
  if (aggregate == NULL) return T64_RSP_DIAG_ALLOCATION_FAILED;
  if (t64_diag_partition_create(inventory->value_count, &aggregate->partition) !=
      T64_DIAG_PARTITION_OK) {
    free(aggregate);
    return T64_RSP_DIAG_ALLOCATION_FAILED;
  }
  aggregate->results = (t64_rsp_diag_owned_result *)calloc(
      inventory->value_count, sizeof *aggregate->results);
  if (aggregate->results == NULL) {
    t64_diag_partition_destroy(aggregate->partition);
    free(aggregate);
    return T64_RSP_DIAG_ALLOCATION_FAILED;
  }
  aggregate->inventory = inventory;
  *aggregate_out = aggregate;
  return T64_RSP_DIAG_OK;
}

void t64_rsp_diag_aggregate_destroy(t64_rsp_diag_aggregate *aggregate) {
  if (aggregate == NULL) return;
  t64_diag_partition_destroy(aggregate->partition);
  free(aggregate->results);
  free(aggregate);
}

t64_rsp_diag_status t64_rsp_diag_aggregate_add_batch(
    t64_rsp_diag_aggregate *aggregate, const uint8_t *bytes, size_t length) {
  size_t flat;
  t64_rsp_diag_owned_result result = {0};
  if (aggregate == NULL) return T64_RSP_DIAG_INVALID_ARGUMENT;
  if (!rsp_diag_parse_batch(aggregate->inventory, bytes, length, &flat, &result)) {
    aggregate->invalid = true;
    return T64_RSP_DIAG_INVALID_BATCH;
  }
  if (t64_diag_partition_add(aggregate->partition, flat,
                             (uint8_t)result.result.outcome) ==
      T64_DIAG_PARTITION_DUPLICATE) {
    aggregate->invalid = true;
    return T64_RSP_DIAG_DUPLICATE_MEMBER;
  }
  aggregate->results[flat] = result;
  aggregate->results[flat].result.operation = aggregate->results[flat].operation;
  aggregate->results[flat].result.reason = aggregate->results[flat].reason;
  aggregate->results[flat].result.result_sha256 =
      aggregate->results[flat].result_sha256;
  aggregate->results[flat].result.capture_sha256 =
      aggregate->results[flat].capture_sha256;
  return T64_RSP_DIAG_OK;
}

bool t64_rsp_diag_aggregate_is_complete(
    const t64_rsp_diag_aggregate *aggregate) {
  size_t i;
  if (aggregate == NULL || aggregate->inventory == NULL || aggregate->invalid)
    return false;
  for (i = 0u; i < aggregate->inventory->value_count; i++) {
    uint8_t outcome;
    if (!t64_diag_partition_outcome(aggregate->partition, i, &outcome) ||
        outcome != T64_RSP_DIAG_OUTCOME_PASS ||
        !aggregate->results[i].result.terminal_delay_slot_retired ||
        aggregate->results[i].result.retired_instructions == 0u) return false;
  }
  return true;
}

size_t t64_rsp_diag_aggregate_member_count(
    const t64_rsp_diag_aggregate *aggregate) {
  size_t i;
  size_t count = 0u;
  if (aggregate == NULL || aggregate->inventory == NULL) return 0u;
  for (i = 0u; i < aggregate->inventory->value_count; i++)
    if (t64_diag_partition_seen(aggregate->partition, i)) count++;
  return count;
}

bool t64_rsp_diag_aggregate_member_at(
    const t64_rsp_diag_aggregate *aggregate, size_t required_ordinal,
    t64_rsp_diag_member_result *result_out) {
  if (aggregate == NULL || result_out == NULL || aggregate->inventory == NULL ||
      required_ordinal >= aggregate->inventory->value_count ||
      !t64_diag_partition_seen(aggregate->partition, required_ordinal))
    return false;
  *result_out = aggregate->results[required_ordinal].result;
  return true;
}

static bool rsp_diag_small_count(const t64_json_node *node, size_t *value_out) {
  if (node == NULL || node->type != T64_JSON_INT || node->num < 0 ||
      (uint64_t)node->num > (uint64_t)T64_RSP_DIAG_MAX_ROWS ||
      value_out == NULL) {
    return false;
  }
  *value_out = (size_t)node->num;
  return true;
}

static bool rsp_diag_identity_parse(const t64_json_node *node,
                                    t64_rsp_diag_identity *identity) {
  const t64_json_node *field;
  if (identity == NULL ||
      !t64_diag_json_object_has_exact_keys(
          node, rsp_diag_identity_keys,
          sizeof rsp_diag_identity_keys / sizeof rsp_diag_identity_keys[0])) {
    return false;
  }
#define RSP_DIGEST_FIELD(member, name)                                          \
  do {                                                                          \
    field = t64_json_get(node, name);                                           \
    if (!t64_diag_json_lowercase_hex(field, 64u)) return false;                 \
    identity->member = field->str;                                              \
  } while (0)
  RSP_DIGEST_FIELD(boot_sha256, "boot_sha256");
  RSP_DIGEST_FIELD(closure_sha256, "closure_sha256");
  RSP_DIGEST_FIELD(core_build_sha256, "core_build_sha256");
  field = t64_json_get(node, "core_source_revision");
  if (!t64_diag_json_lowercase_hex(field, 40u)) return false;
  identity->core_source_revision = field->str;
  RSP_DIGEST_FIELD(overlay_sha256, "overlay_sha256");
  RSP_DIGEST_FIELD(profile_descriptor_sha256, "profile_descriptor_sha256");
  RSP_DIGEST_FIELD(qualification_profile_sha256,
                   "qualification_profile_sha256");
  RSP_DIGEST_FIELD(qualification_sha256, "qualification_sha256");
  RSP_DIGEST_FIELD(rom_sha256, "rom_sha256");
  RSP_DIGEST_FIELD(source_projection_sha256, "source_projection_sha256");
#undef RSP_DIGEST_FIELD
  field = t64_json_get(node, "source_revision");
  if (!t64_diag_json_lowercase_hex(field, 40u)) return false;
  identity->source_revision = field->str;
  field = t64_diag_json_string_member(node, "profile_id");
  if (!t64_diag_json_printable_text(field, 128u, false)) return false;
  identity->profile_id = field->str;
  return true;
}

static bool rsp_diag_registration_parse(
    const t64_json_node *node, t64_rsp_diag_registration *registration,
    t64_rsp_diag_value *values, size_t values_capacity, size_t *value_count_out) {
  const t64_json_node *field;
  const t64_json_node *value_array;
  const t64_json_node *value_node;
  size_t source_value_count;
  size_t expected_value_count;
  size_t i;
  char expected_id[T64_RSP_DIAG_MAX_NAME_BYTES];
  char expected_selector[T64_RSP_DIAG_MAX_SELECTOR_BYTES];
  if (registration == NULL || value_count_out == NULL ||
      !t64_diag_json_object_has_exact_keys(
          node, rsp_diag_registration_keys,
          sizeof rsp_diag_registration_keys /
              sizeof rsp_diag_registration_keys[0])) {
    return false;
  }
  field = t64_json_get(node, "index");
  if (!t64_diag_json_index16(field, &registration->index)) {
    return false;
  }
  field = t64_json_get(node, "group");
  if (!t64_diag_json_string_equals(field, "rsp")) return false;
  registration->group = field->str;
  field = t64_json_get(node, "name");
  if (!t64_diag_json_printable_text(field, T64_RSP_DIAG_MAX_NAME_BYTES, false))
    return false;
  registration->name = field->str;
  field = t64_json_get(node, "source_name");
  if (!t64_diag_json_printable_text(field, T64_RSP_DIAG_MAX_NAME_BYTES, false))
    return false;
  registration->source_name = field->str;
  field = t64_json_get(node, "constructor");
  if (!t64_diag_json_printable_text(field, T64_RSP_DIAG_MAX_TEXT_BYTES, false))
    return false;
  field = t64_json_get(node, "source_file");
  if (!t64_diag_json_printable_text(field, T64_RSP_DIAG_MAX_TEXT_BYTES, false) ||
      strncmp(field->str, "src/tests/rsp/", 14u) != 0 ||
      strstr(field->str, "..") != NULL) {
    return false;
  }
  if (!t64_diag_json_lowercase_hex(t64_json_get(node, "source_file_sha256"),
                                   64u) ||
      !t64_diag_json_lowercase_hex(
          t64_json_get(node, "values_method_sha256"), 64u)) {
    return false;
  }
  field = t64_json_get(node, "level");
  if (field == NULL || field->type != T64_JSON_STRING) return false;
  if (!(t64_diag_json_string_equals(field, "BasicFunctionality") ||
        t64_diag_json_string_equals(field, "RarelyUsed") ||
        t64_diag_json_string_equals(field, "Weird") ||
        t64_diag_json_string_equals(field, "Timing") ||
        t64_diag_json_string_equals(field, "Cycle") ||
        t64_diag_json_string_equals(field, "COP0Hazard") ||
        t64_diag_json_string_equals(field, "PoorlyUnderstoodQuirk") ||
        t64_diag_json_string_equals(field, "StressTest") ||
        t64_diag_json_string_equals(field, "RDPBasic") ||
        t64_diag_json_string_equals(field, "RDPPrecise"))) {
    return false;
  }
  field = t64_json_get(node, "lane");
  if (field == NULL || field->type != T64_JSON_STRING ||
      !(t64_diag_json_string_equals(field, "required-base") ||
        t64_diag_json_string_equals(field, "timing") ||
        t64_diag_json_string_equals(field, "cycle") ||
        t64_diag_json_string_equals(field, "cop0-hazard") ||
        t64_diag_json_string_equals(field, "quirk") ||
        t64_diag_json_string_equals(field, "stress") ||
        t64_diag_json_string_equals(field, "informational"))) {
    return false;
  }
  if (!rsp_diag_small_count(t64_json_get(node, "source_value_count"),
                            &source_value_count)) {
    return false;
  }
  expected_value_count = source_value_count == 0u ? 1u : source_value_count;
  field = t64_json_get(node, "execution");
  if (!((source_value_count == 0u &&
         t64_diag_json_string_equals(field, "once")) ||
        (source_value_count != 0u &&
         t64_diag_json_string_equals(field, "values")))) {
    return false;
  }
  value_array = t64_json_get(node, "values");
  if (value_array == NULL || value_array->type != T64_JSON_ARRAY ||
      value_array->count != expected_value_count ||
      expected_value_count > values_capacity) {
    return false;
  }
  registration->value_offset = *value_count_out;
  registration->value_count = expected_value_count;
  value_node = value_array->first;
  for (i = 0u; i < expected_value_count; i++) {
    t64_rsp_diag_value *value = &values[*value_count_out + i];
    field = value_node == NULL ? NULL : t64_json_get(value_node, "index");
    if (!t64_diag_json_object_has_exact_keys(
            value_node, rsp_diag_value_keys,
            sizeof rsp_diag_value_keys / sizeof rsp_diag_value_keys[0]) ||
        !t64_diag_json_index16(field, &value->index) || value->index != i) {
      return false;
    }
    field = t64_json_get(value_node, "id");
    if (!t64_diag_json_printable_text(field, T64_RSP_DIAG_MAX_NAME_BYTES, false))
      return false;
    if (source_value_count == 0u) {
      (void)snprintf(expected_id, sizeof expected_id, "once");
    } else {
      (void)snprintf(expected_id, sizeof expected_id, "v%05u",
                     (unsigned)value->index);
    }
    if (strcmp(field->str, expected_id) != 0) return false;
    value->id = field->str;
    field = t64_json_get(value_node, "selector");
    if (!t64_diag_json_selector_text(field, T64_RSP_DIAG_MAX_SELECTOR_BYTES))
      return false;
    (void)snprintf(expected_selector, sizeof expected_selector,
                   "r%04uv%05u", (unsigned)registration->index,
                   (unsigned)value->index);
    if (strcmp(field->str, expected_selector) != 0) return false;
    value->selector = field->str;
    field = t64_json_get(value_node, "selected_rom_sha256");
    if (!t64_diag_json_lowercase_hex(field, 64u)) return false;
    value->selected_rom_sha256 = field->str;
    value_node = value_node->next;
  }
  *value_count_out += expected_value_count;
  return true;
}

static t64_rsp_diag_status rsp_diag_inventory_parse(
    const uint8_t *bytes, size_t length,
    t64_rsp_diag_inventory **inventory_out) {
  t64_json_limits limits = {T64_RSP_DIAG_MAX_INVENTORY_BYTES, 8u, 512u,
                            RSP_DIAG_MAX_JSON_NODES};
  t64_json_doc document = {0};
  t64_json_error json_error = {T64_JSON_OK, 0u};
  t64_rsp_diag_inventory *inventory = NULL;
  const t64_json_node *registrations;
  const t64_json_node *registration_node;
  const t64_json_node *counts;
  size_t declared_registration_count;
  size_t declared_value_count;
  size_t declared_required_count;
  size_t registration_index;
  size_t value_count = 0u;
  const char **selectors = NULL;
  uint32_t last_registration_index = 0u;
  bool have_last_registration = false;
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  t64_sha256_ctx sha;
  t64_rsp_diag_status status = T64_RSP_DIAG_INVALID_INVENTORY;

  if (bytes == NULL || inventory_out == NULL) {
    return T64_RSP_DIAG_INVALID_ARGUMENT;
  }
  *inventory_out = NULL;
  if (length == 0u || length > T64_RSP_DIAG_MAX_INVENTORY_BYTES) {
    return T64_RSP_DIAG_SIZE_LIMIT;
  }
  if (t64_json_parse(bytes, length, &limits, &document, &json_error) != 0) {
    return T64_RSP_DIAG_INVALID_JSON;
  }
  if (!t64_diag_json_object_has_exact_keys(
          document.root, rsp_diag_inventory_keys,
          sizeof rsp_diag_inventory_keys / sizeof rsp_diag_inventory_keys[0]) ||
      !t64_diag_json_string_equals(t64_json_get(document.root, "schema"),
                                   "t64.rsp.inventory") ||
      t64_json_get(document.root, "schema_version") == NULL ||
      t64_json_get(document.root, "schema_version")->type != T64_JSON_INT ||
      t64_json_get(document.root, "schema_version")->num !=
          T64_RSP_DIAG_INVENTORY_VERSION) {
    status = T64_RSP_DIAG_INVALID_SCHEMA;
    goto cleanup;
  }
  if (!t64_diag_json_canonical_document(&document, bytes, length)) {
    status = T64_RSP_DIAG_INVALID_SCHEMA;
    goto cleanup;
  }
  if (!t64_diag_json_string_equals(t64_json_get(document.root, "component"),
                                  "rsp")) {
    status = T64_RSP_DIAG_INVALID_COMPONENT;
    goto cleanup;
  }
  if (
      !rsp_diag_identity_parse(t64_json_get(document.root, "identity"),
                               &(t64_rsp_diag_identity){0})) {
    status = T64_RSP_DIAG_INVALID_INVENTORY;
    goto cleanup;
  }
  counts = t64_json_get(document.root, "declared_counts");
  registrations = t64_json_get(document.root, "registrations");
  if (!t64_diag_json_object_has_exact_keys(
          counts, rsp_diag_count_keys,
          sizeof rsp_diag_count_keys / sizeof rsp_diag_count_keys[0]) ||
      registrations == NULL || registrations->type != T64_JSON_ARRAY ||
      registrations->count == 0u ||
      registrations->count > T64_RSP_DIAG_MAX_ROWS ||
      !rsp_diag_small_count(t64_json_get(counts, "registrations"),
                            &declared_registration_count) ||
      !rsp_diag_small_count(t64_json_get(counts, "values"),
                            &declared_value_count) ||
      !rsp_diag_small_count(t64_json_get(counts, "required_values"),
                            &declared_required_count) ||
      declared_registration_count != registrations->count ||
      declared_value_count == 0u || declared_value_count > T64_RSP_DIAG_MAX_ROWS ||
      declared_required_count != declared_value_count) {
    status = T64_RSP_DIAG_INVALID_INVENTORY;
    goto cleanup;
  }
  inventory = (t64_rsp_diag_inventory *)calloc(1u, sizeof *inventory);
  if (inventory == NULL) {
    status = T64_RSP_DIAG_ALLOCATION_FAILED;
    goto cleanup;
  }
  inventory->registrations = (t64_rsp_diag_registration *)calloc(
      declared_registration_count, sizeof *inventory->registrations);
  inventory->values =
      (t64_rsp_diag_value *)calloc(declared_value_count, sizeof *inventory->values);
  selectors = (const char **)calloc(declared_value_count, sizeof *selectors);
  if (inventory->registrations == NULL || inventory->values == NULL ||
      selectors == NULL) {
    status = T64_RSP_DIAG_ALLOCATION_FAILED;
    goto cleanup;
  }
  if (!rsp_diag_identity_parse(t64_json_get(document.root, "identity"),
                               &inventory->identity)) {
    status = T64_RSP_DIAG_INVALID_INVENTORY;
    goto cleanup;
  }
  registration_node = registrations->first;
  for (registration_index = 0u; registration_index < declared_registration_count;
       registration_index++) {
    if (registration_node == NULL ||
        !rsp_diag_registration_parse(
            registration_node, &inventory->registrations[registration_index],
            inventory->values,
            declared_value_count, &value_count)) {
      status = T64_RSP_DIAG_INVALID_INVENTORY;
      goto cleanup;
    }
    if (have_last_registration &&
        inventory->registrations[registration_index].index <=
            last_registration_index) {
      status = T64_RSP_DIAG_INVALID_INVENTORY;
      goto cleanup;
    }
    last_registration_index =
        inventory->registrations[registration_index].index;
    have_last_registration = true;
    registration_node = registration_node->next;
  }
  if (registration_node != NULL || value_count != declared_value_count) {
    status = T64_RSP_DIAG_INVALID_INVENTORY;
    goto cleanup;
  }
  for (registration_index = 0u; registration_index < value_count;
       registration_index++) {
    selectors[registration_index] = inventory->values[registration_index].selector;
  }
  qsort(selectors, value_count, sizeof *selectors,
        rsp_diag_compare_selector_pointer);
  for (registration_index = 1u; registration_index < value_count;
       registration_index++) {
    if (strcmp(selectors[registration_index - 1u],
               selectors[registration_index]) == 0) {
      status = T64_RSP_DIAG_INVALID_INVENTORY;
      goto cleanup;
    }
  }
  inventory->document = document;
  document = (t64_json_doc){0};
  inventory->registration_count = declared_registration_count;
  inventory->value_count = value_count;
  t64_sha256_init(&sha);
  t64_sha256_update(&sha, bytes, length);
  t64_sha256_final(&sha, digest);
  t64_sha256_hex(digest, inventory->sha256);
  free(selectors);
  *inventory_out = inventory;
  return T64_RSP_DIAG_OK;

cleanup:
  t64_json_free(&document);
  free(selectors);
  if (inventory != NULL) {
    free(inventory->registrations);
    free(inventory->values);
    free(inventory);
  }
  return status;
}

t64_rsp_diag_status t64_rsp_diag_inventory_parse(
    const uint8_t *bytes, size_t length,
    t64_rsp_diag_inventory **inventory_out) {
  return rsp_diag_inventory_parse(bytes, length, inventory_out);
}

void t64_rsp_diag_inventory_destroy(t64_rsp_diag_inventory *inventory) {
  if (inventory == NULL) return;
  t64_json_free(&inventory->document);
  free(inventory->registrations);
  free(inventory->values);
  free(inventory);
}

size_t t64_rsp_diag_inventory_registration_count(
    const t64_rsp_diag_inventory *inventory) {
  return inventory == NULL ? 0u : inventory->registration_count;
}

size_t t64_rsp_diag_inventory_required_value_count(
    const t64_rsp_diag_inventory *inventory) {
  return inventory == NULL ? 0u : inventory->value_count;
}

const char *t64_rsp_diag_inventory_sha256(
    const t64_rsp_diag_inventory *inventory) {
  return inventory == NULL ? NULL : inventory->sha256;
}

bool t64_rsp_diag_inventory_identity(const t64_rsp_diag_inventory *inventory,
                                     t64_rsp_diag_identity *identity_out) {
  if (inventory == NULL || identity_out == NULL) return false;
  *identity_out = inventory->identity;
  return true;
}

bool t64_rsp_diag_inventory_member_at(const t64_rsp_diag_inventory *inventory,
                                      size_t required_ordinal,
                                      t64_rsp_diag_member *member_out) {
  size_t i;
  if (inventory == NULL || member_out == NULL ||
      required_ordinal >= inventory->value_count) {
    return false;
  }
  for (i = 0u; i < inventory->registration_count; i++) {
    const t64_rsp_diag_registration *registration =
        &inventory->registrations[i];
    if (required_ordinal >= registration->value_offset &&
        required_ordinal - registration->value_offset <
            registration->value_count) {
      const t64_rsp_diag_value *value = &inventory->values[required_ordinal];
      member_out->registration_index = registration->index;
      member_out->value_index = value->index;
      member_out->group = registration->group;
      member_out->registration_name = registration->name;
      member_out->source_name = registration->source_name;
      member_out->value_id = value->id;
      member_out->selector = value->selector;
      member_out->selected_rom_sha256 = value->selected_rom_sha256;
      return true;
    }
  }
  return false;
}

/* Outcome serialization and aggregation are added with the run adapter. */
const char *t64_rsp_diag_status_name(t64_rsp_diag_status status) {
  static const char *const names[] = {
      "ok", "invalid-argument", "size-limit", "invalid-json",
      "invalid-schema", "invalid-component", "invalid-inventory",
      "invalid-batch", "incomplete", "duplicate-member",
      "identity-mismatch", "allocation-failed", "canonicalization-failed",
      "output-overflow"};
  return (size_t)status < sizeof names / sizeof names[0] ? names[status] : NULL;
}

const char *t64_rsp_diag_outcome_name(t64_rsp_diag_outcome outcome) {
  return (size_t)outcome < sizeof rsp_diag_outcomes / sizeof rsp_diag_outcomes[0]
             ? rsp_diag_outcomes[outcome]
             : NULL;
}
