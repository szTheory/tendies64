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
  t64_rsp_diag_member_result *results;
  bool invalid;
};

static const char *const rsp_diag_inventory_keys[] = {
    "component", "declared_counts", "identity", "registrations", "schema",
    "schema_version"};
static const char *const rsp_diag_count_keys[] = {
    "registrations", "required_values", "values"};
static const char *const rsp_diag_identity_keys[] = {
    "boot_sha256", "closure_sha256", "core_build_sha256",
    "overlay_sha256", "profile_descriptor_sha256", "profile_id",
    "profile_sha256", "qualification_sha256", "rom_sha256",
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
  RSP_DIGEST_FIELD(overlay_sha256, "overlay_sha256");
  RSP_DIGEST_FIELD(profile_descriptor_sha256, "profile_descriptor_sha256");
  RSP_DIGEST_FIELD(profile_sha256, "profile_sha256");
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
    value->id = field->str;
    field = t64_json_get(value_node, "selector");
    if (!t64_diag_json_selector_text(field, T64_RSP_DIAG_MAX_SELECTOR_BYTES))
      return false;
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
  /* Component is parsed as a closed field. Its exact value check is the RED. */
  if (t64_diag_json_string_member(document.root, "component") == NULL ||
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
  if (inventory->registrations == NULL || inventory->values == NULL) {
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
  inventory->document = document;
  document = (t64_json_doc){0};
  inventory->registration_count = declared_registration_count;
  inventory->value_count = value_count;
  t64_sha256_init(&sha);
  t64_sha256_update(&sha, bytes, length);
  t64_sha256_final(&sha, digest);
  t64_sha256_hex(digest, inventory->sha256);
  *inventory_out = inventory;
  return T64_RSP_DIAG_OK;

cleanup:
  t64_json_free(&document);
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
