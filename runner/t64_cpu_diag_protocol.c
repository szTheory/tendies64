/* Closed, bounded local CPU diagnostic schemas and batch aggregation. */
#include "t64_cpu_diag_protocol.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_json.h"
#include "t64_jsonw.h"
#include "t64_sha256.h"

#define T64_CPU_DIAG_MAX_JSON_NODES 262144u
#define T64_CPU_DIAG_MAX_IDENTITY_TEXT 128u

typedef struct {
  const char *source_revision;
  const char *overlay_sha256;
  const char *tool_receipt_sha256;
  const char *core_build_sha256;
  const char *profile_sha256;
  const char *base_rom_sha256;
  const char *selector;
  const char *selected_rom_sha256;
} t64_cpu_diag_identity;

typedef struct {
  uint32_t index;
  const char *id;
  size_t id_length;
} t64_cpu_diag_value;

typedef struct {
  uint32_t registration_index;
  uint32_t value_index;
  size_t flat_index;
} t64_cpu_diag_required_member;

typedef struct {
  uint32_t index;
  const char *name;
  size_t name_length;
  const char *group;
  const char *level;
  const char *lane;
  const char *reason;
  const char *execution;
  size_t value_offset;
  size_t value_count;
  bool required;
} t64_cpu_diag_registration;

struct t64_cpu_diag_inventory {
  t64_json_doc document;
  t64_cpu_diag_identity identity;
  t64_cpu_diag_registration *registrations;
  t64_cpu_diag_value *values;
  t64_cpu_diag_required_member *required_members;
  size_t registration_count;
  size_t value_count;
  size_t required_value_count;
  uint64_t declared_registration_count;
  uint64_t declared_value_count;
  uint64_t declared_required_value_count;
  char sha256[T64_SHA256_HEX_SIZE];
};

struct t64_cpu_diag_aggregate {
  const t64_cpu_diag_inventory *inventory;
  uint8_t *seen;
  uint8_t *outcomes;
  bool invalid;
};

static const char *const t64_cpu_diag_groups[] = {
    "address_error_exception", "arithmetic", "cache", "cop0", "cop1",
    "cop_unusable", "endian_re", "exception_instructions", "jumps",
    "overflow_exception", "privilege", "tlb", "tlb64", "traps"};

static const char *const t64_cpu_diag_levels[] = {
    "BasicFunctionality", "RarelyUsed", "Weird", "RDPBasic", "RDPPrecise",
    "Timing", "COP0Hazard", "Cycle", "PoorlyUnderstoodQuirk", "StressTest"};

static const char *const t64_cpu_diag_outcomes[] = {
    "pass", "fail", "incomplete", "not-run", "guest-stop", "harness-error"};

static const char *const t64_cpu_diag_inventory_keys[] = {
    "declared_counts", "identity", "registrations", "schema",
    "schema_version"};
static const char *const t64_cpu_diag_declared_count_keys[] = {
    "registrations", "required_values", "values"};
static const char *const t64_cpu_diag_identity_keys[] = {
    "base_rom_sha256", "core_build_sha256", "overlay_sha256",
    "profile_sha256", "selected_rom_sha256", "selector",
    "source_revision", "tool_receipt_sha256"};
static const char *const t64_cpu_diag_batch_identity_keys[] = {
    "base_rom_sha256", "core_build_sha256", "inventory_sha256",
    "overlay_sha256", "profile_sha256", "selected_rom_sha256", "selector",
    "source_revision", "tool_receipt_sha256"};
static const char *const t64_cpu_diag_registration_keys[] = {
    "execution", "group", "index", "lane", "level", "name", "reason",
    "values"};
static const char *const t64_cpu_diag_value_keys[] = {"id", "index"};
static const char *const t64_cpu_diag_batch_keys[] = {
    "identity", "records", "schema", "schema_version"};
static const char *const t64_cpu_diag_start_keys[] = {
    "event", "registration_index", "value_index"};
static const char *const t64_cpu_diag_outcome_keys[] = {"event", "status"};
static const char *const t64_cpu_diag_done_keys[] = {
    "complete", "event", "record_count"};

static bool string_equals(const t64_json_node *node, const char *value) {
  size_t length = strlen(value);
  return node != NULL && node->type == T64_JSON_STRING &&
         node->len == length && memcmp(node->str, value, length) == 0;
}

static bool object_has_exact_keys(const t64_json_node *object,
                                  const char *const *keys, size_t count) {
  return t64_json_check_keys(object, keys, count, keys, count, NULL) ==
         T64_JSON_KEYS_OK;
}

static const t64_json_node *string_member(const t64_json_node *object,
                                         const char *key) {
  const t64_json_node *node = t64_json_get(object, key);
  return node != NULL && node->type == T64_JSON_STRING ? node : NULL;
}

static bool printable_text(const t64_json_node *node, size_t maximum,
                           bool allow_empty) {
  size_t i;
  if (node == NULL || node->type != T64_JSON_STRING ||
      node->len > maximum || (!allow_empty && node->len == 0u) ||
      memchr(node->str, '\0', node->len) != NULL) {
    return false;
  }
  for (i = 0u; i < node->len; i++) {
    unsigned char ch = (unsigned char)node->str[i];
    if (ch < 0x20u || ch == 0x7fu) return false;
  }
  return true;
}

static bool lowercase_hex(const t64_json_node *node, size_t digits) {
  size_t i;
  if (node == NULL || node->type != T64_JSON_STRING || node->len != digits) {
    return false;
  }
  for (i = 0u; i < digits; i++) {
    char ch = node->str[i];
    if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) return false;
  }
  return true;
}

static bool selector_text(const t64_json_node *node) {
  size_t i;
  if (!printable_text(node, T64_CPU_DIAG_MAX_SELECTOR_BYTES, false)) return false;
  for (i = 0u; i < node->len; i++) {
    char ch = node->str[i];
    if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
          (ch >= '0' && ch <= '9') || ch == '.' || ch == '_' || ch == '+' ||
          ch == '-')) {
      return false;
    }
  }
  return true;
}

static bool index_node(const t64_json_node *node, uint32_t *value_out) {
  if (node == NULL || node->type != T64_JSON_INT || node->num < 0 ||
      node->num > UINT16_MAX) {
    return false;
  }
  *value_out = (uint32_t)node->num;
  return true;
}

static bool declared_count(const t64_json_node *object, const char *key,
                           uint64_t *value_out) {
  const t64_json_node *node = t64_json_get(object, key);
  return t64_json_u64_string(node, value_out) != 0 &&
         *value_out <= T64_CPU_DIAG_MAX_ROWS;
}

static bool canonical_document(const t64_json_doc *document,
                               const uint8_t *bytes, size_t length) {
  char *canonical;
  size_t canonical_length = 0u;
  t64_jsonw writer;
  bool matches = false;
  if (document == NULL || document->root == NULL || bytes == NULL ||
      length == 0u || length >= T64_CPU_DIAG_MAX_INVENTORY_BYTES + 1u) {
    return false;
  }
  canonical = (char *)malloc(length + 1u);
  if (canonical == NULL) return false;
  t64_jsonw_init(&writer, canonical, length + 1u);
  if (t64_jsonw_node(&writer, document->root) == T64_JSONW_OK &&
      t64_jsonw_finish(&writer, &canonical_length) == T64_JSONW_OK &&
      canonical_length == length && memcmp(canonical, bytes, length) == 0) {
    matches = true;
  }
  free(canonical);
  return matches;
}

static bool parse_schema_version(const t64_json_node *root,
                                 const char *schema) {
  const t64_json_node *version = t64_json_get(root, "schema_version");
  return string_equals(t64_json_get(root, "schema"), schema) &&
         version != NULL && version->type == T64_JSON_INT &&
         version->num == (int64_t)T64_CPU_DIAG_SCHEMA_VERSION;
}

static bool parse_identity(const t64_json_node *object, bool include_inventory,
                           t64_cpu_diag_identity *identity,
                           const char **inventory_sha256_out) {
  static const char inventory_hash_key[] = "inventory_sha256";
  const t64_json_node *node;
  const char *const *keys = include_inventory
                                ? t64_cpu_diag_batch_identity_keys
                                : t64_cpu_diag_identity_keys;
  size_t key_count = include_inventory
                         ? sizeof t64_cpu_diag_batch_identity_keys /
                               sizeof t64_cpu_diag_batch_identity_keys[0]
                         : sizeof t64_cpu_diag_identity_keys /
                               sizeof t64_cpu_diag_identity_keys[0];
  if (identity == NULL || !object_has_exact_keys(object, keys, key_count)) {
    return false;
  }
  node = string_member(object, "base_rom_sha256");
  if (!lowercase_hex(node, 64u)) return false;
  identity->base_rom_sha256 = node->str;
  node = string_member(object, "core_build_sha256");
  if (!lowercase_hex(node, 64u)) return false;
  identity->core_build_sha256 = node->str;
  node = string_member(object, "overlay_sha256");
  if (!lowercase_hex(node, 64u)) return false;
  identity->overlay_sha256 = node->str;
  node = string_member(object, "profile_sha256");
  if (!lowercase_hex(node, 64u)) return false;
  identity->profile_sha256 = node->str;
  node = string_member(object, "selected_rom_sha256");
  if (!lowercase_hex(node, 64u)) return false;
  identity->selected_rom_sha256 = node->str;
  node = string_member(object, "selector");
  if (!selector_text(node)) return false;
  identity->selector = node->str;
  node = string_member(object, "source_revision");
  if (!lowercase_hex(node, 40u)) return false;
  identity->source_revision = node->str;
  node = string_member(object, "tool_receipt_sha256");
  if (!lowercase_hex(node, 64u)) return false;
  identity->tool_receipt_sha256 = node->str;
  if (include_inventory) {
    node = string_member(object, inventory_hash_key);
    if (!lowercase_hex(node, 64u) || inventory_sha256_out == NULL) return false;
    *inventory_sha256_out = node->str;
  } else if (inventory_sha256_out != NULL) {
    *inventory_sha256_out = NULL;
  }
  return true;
}

static bool identity_matches(const t64_cpu_diag_identity *left,
                             const t64_cpu_diag_identity *right) {
  return strcmp(left->source_revision, right->source_revision) == 0 &&
         strcmp(left->overlay_sha256, right->overlay_sha256) == 0 &&
         strcmp(left->tool_receipt_sha256, right->tool_receipt_sha256) == 0 &&
         strcmp(left->core_build_sha256, right->core_build_sha256) == 0 &&
         strcmp(left->profile_sha256, right->profile_sha256) == 0 &&
         strcmp(left->base_rom_sha256, right->base_rom_sha256) == 0 &&
         strcmp(left->selector, right->selector) == 0 &&
         strcmp(left->selected_rom_sha256, right->selected_rom_sha256) == 0;
}

static bool one_of(const char *value, const char *const *values, size_t count) {
  size_t i;
  for (i = 0u; i < count; i++) {
    if (strcmp(value, values[i]) == 0) return true;
  }
  return false;
}

static bool valid_group(const char *group) {
  return one_of(group, t64_cpu_diag_groups,
                sizeof t64_cpu_diag_groups / sizeof t64_cpu_diag_groups[0]);
}

static bool valid_level(const char *level) {
  return one_of(level, t64_cpu_diag_levels,
                sizeof t64_cpu_diag_levels / sizeof t64_cpu_diag_levels[0]);
}

static bool valid_lane_level(const char *lane, const char *level) {
  if (strcmp(lane, "required-base") == 0) {
    return strcmp(level, "BasicFunctionality") == 0 ||
           strcmp(level, "RarelyUsed") == 0;
  }
  if (strcmp(lane, "weird") == 0) return strcmp(level, "Weird") == 0;
  if (strcmp(lane, "timing") == 0) return strcmp(level, "Timing") == 0;
  if (strcmp(lane, "cycle") == 0) return strcmp(level, "Cycle") == 0;
  if (strcmp(lane, "cop0-hazard") == 0) return strcmp(level, "COP0Hazard") == 0;
  if (strcmp(lane, "quirk") == 0) {
    return strcmp(level, "PoorlyUnderstoodQuirk") == 0;
  }
  if (strcmp(lane, "stress") == 0) return strcmp(level, "StressTest") == 0;
  return false;
}

static bool registration_fields(const t64_json_node *node,
                                t64_cpu_diag_registration *registration,
                                const t64_json_node **values_out,
                                uint32_t expected_index) {
  const t64_json_node *field;
  uint32_t index;
  if (!object_has_exact_keys(
          node, t64_cpu_diag_registration_keys,
          sizeof t64_cpu_diag_registration_keys /
              sizeof t64_cpu_diag_registration_keys[0])) {
    return false;
  }
  field = t64_json_get(node, "index");
  if (!index_node(field, &index) || index != expected_index) return false;
  registration->index = index;
  field = string_member(node, "name");
  if (!printable_text(field, T64_CPU_DIAG_MAX_NAME_BYTES, false)) return false;
  registration->name = field->str;
  registration->name_length = field->len;
  field = string_member(node, "group");
  if (!printable_text(field, 32u, false)) return false;
  registration->group = field->str;
  field = string_member(node, "level");
  if (!printable_text(field, 32u, false) || !valid_level(field->str)) return false;
  registration->level = field->str;
  field = string_member(node, "lane");
  if (!printable_text(field, 32u, false)) return false;
  registration->lane = field->str;
  field = string_member(node, "reason");
  if (!printable_text(field, T64_CPU_DIAG_MAX_NAME_BYTES, false)) return false;
  registration->reason = field->str;
  field = string_member(node, "execution");
  if (!printable_text(field, 16u, false)) return false;
  registration->execution = field->str;
  field = t64_json_get(node, "values");
  if (field == NULL || field->type != T64_JSON_ARRAY) return false;
  *values_out = field;

  registration->required = strcmp(registration->lane, "required-base") == 0;
  if (registration->required) {
    if (!valid_group(registration->group) ||
        !valid_lane_level(registration->lane, registration->level) ||
        strcmp(registration->reason, "required") != 0 ||
        (strcmp(registration->execution, "values") != 0 &&
         strcmp(registration->execution, "once") != 0) ||
        field->count == 0u) {
      return false;
    }
  } else if (strcmp(registration->lane, "skipped-non-cpu") == 0) {
    if (strcmp(registration->group, "non-cpu") != 0 ||
        strcmp(registration->execution, "omitted") != 0 ||
        field->count != 0u) {
      return false;
    }
  } else if (!valid_group(registration->group) ||
             !valid_lane_level(registration->lane, registration->level) ||
             strcmp(registration->reason, "required") == 0 ||
             (strcmp(registration->execution, "values") != 0 &&
              strcmp(registration->execution, "once") != 0) ||
             field->count == 0u) {
    return false;
  }
  if (strcmp(registration->execution, "once") == 0 &&
      (field->count != 1u ||
       !string_equals(t64_json_get(field->first, "id"), "once"))) {
    return false;
  }
  if (strcmp(registration->execution, "omitted") == 0 && field->count != 0u) {
    return false;
  }
  return true;
}

static bool value_fields(const t64_json_node *node, uint32_t expected_index,
                         t64_cpu_diag_value *value) {
  const t64_json_node *field;
  uint32_t index;
  if (!object_has_exact_keys(node, t64_cpu_diag_value_keys,
                             sizeof t64_cpu_diag_value_keys /
                                 sizeof t64_cpu_diag_value_keys[0])) {
    return false;
  }
  field = t64_json_get(node, "index");
  if (!index_node(field, &index) || index != expected_index) return false;
  field = string_member(node, "id");
  if (!printable_text(field, T64_CPU_DIAG_MAX_VALUE_ID_BYTES, false)) return false;
  value->index = index;
  value->id = field->str;
  value->id_length = field->len;
  return true;
}

static int compare_names(const void *left, const void *right) {
  const char *const *left_name = (const char *const *)left;
  const char *const *right_name = (const char *const *)right;
  return strcmp(*left_name, *right_name);
}

static t64_cpu_diag_status canonical_inventory_parse(
    const uint8_t *bytes, size_t length, t64_cpu_diag_inventory **inventory_out) {
  t64_json_limits limits = {T64_CPU_DIAG_MAX_INVENTORY_BYTES, 8u, 256u,
                            T64_CPU_DIAG_MAX_JSON_NODES};
  t64_json_error json_error = {T64_JSON_OK, 0u};
  t64_cpu_diag_inventory *inventory = NULL;
  t64_json_doc document = {0};
  const t64_json_node *registrations_node;
  const t64_json_node *declared_counts_node;
  const t64_json_node *registration_node;
  const t64_json_node *values_node;
  const t64_json_node *value_node;
  const char *inventory_hash = NULL;
  const char **names = NULL;
  size_t registration_count;
  size_t registration_index = 0u;
  size_t value_count = 0u;
  size_t required_value_count = 0u;
  size_t total_rows = 0u;
  uint64_t declared_registration_count = 0u;
  uint64_t declared_value_count = 0u;
  uint64_t declared_required_value_count = 0u;
  size_t required_member_index = 0u;
  size_t i;
  t64_cpu_diag_status status = T64_CPU_DIAG_INVALID_INVENTORY;
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  t64_sha256_ctx sha;

  if (bytes == NULL || inventory_out == NULL) {
    return T64_CPU_DIAG_INVALID_ARGUMENT;
  }
  if (length == 0u || length > T64_CPU_DIAG_MAX_INVENTORY_BYTES) {
    return T64_CPU_DIAG_SIZE_LIMIT;
  }
  if (t64_json_parse(bytes, length, &limits, &document, &json_error) != 0) {
    return T64_CPU_DIAG_INVALID_JSON;
  }
  if (!object_has_exact_keys(
          document.root, t64_cpu_diag_inventory_keys,
          sizeof t64_cpu_diag_inventory_keys /
              sizeof t64_cpu_diag_inventory_keys[0]) ||
      !parse_schema_version(document.root, "t64.cpu-inventory")) {
    status = T64_CPU_DIAG_INVALID_SCHEMA;
    goto cleanup;
  }
  if (!canonical_document(&document, bytes, length)) {
    status = T64_CPU_DIAG_INVALID_SCHEMA;
    goto cleanup;
  }
  if (!parse_identity(t64_json_get(document.root, "identity"), false,
                      &(t64_cpu_diag_identity){0}, &inventory_hash)) {
    status = T64_CPU_DIAG_INVALID_INVENTORY;
    goto cleanup;
  }
  registrations_node = t64_json_get(document.root, "registrations");
  declared_counts_node = t64_json_get(document.root, "declared_counts");
  if (registrations_node == NULL || registrations_node->type != T64_JSON_ARRAY ||
      registrations_node->count == 0u ||
      registrations_node->count > T64_CPU_DIAG_MAX_ROWS ||
      !object_has_exact_keys(
          declared_counts_node, t64_cpu_diag_declared_count_keys,
          sizeof t64_cpu_diag_declared_count_keys /
              sizeof t64_cpu_diag_declared_count_keys[0]) ||
      !declared_count(declared_counts_node, "registrations",
                      &declared_registration_count) ||
      !declared_count(declared_counts_node, "required_values",
                      &declared_required_value_count) ||
      !declared_count(declared_counts_node, "values", &declared_value_count) ||
      declared_registration_count != registrations_node->count) {
    status = T64_CPU_DIAG_INVALID_INVENTORY;
    goto cleanup;
  }
  registration_count = registrations_node->count;
  inventory = (t64_cpu_diag_inventory *)calloc(1u, sizeof *inventory);
  if (inventory == NULL) {
    status = T64_CPU_DIAG_ALLOCATION_FAILED;
    goto cleanup;
  }
  inventory->registrations = (t64_cpu_diag_registration *)calloc(
      registration_count, sizeof *inventory->registrations);
  names = (const char **)calloc(registration_count, sizeof *names);
  if (inventory->registrations == NULL || names == NULL) {
    status = T64_CPU_DIAG_ALLOCATION_FAILED;
    goto cleanup;
  }
  registration_node = registrations_node->first;
  for (registration_index = 0u; registration_index < registration_count;
       registration_index++) {
    t64_cpu_diag_registration *registration =
        &inventory->registrations[registration_index];
    if (registration_node == NULL ||
        !registration_fields(registration_node, registration, &values_node,
                             (uint32_t)registration_index)) {
      status = T64_CPU_DIAG_INVALID_INVENTORY;
      goto cleanup;
    }
    if (values_node->count > T64_CPU_DIAG_MAX_ROWS - 1u ||
        total_rows > T64_CPU_DIAG_MAX_ROWS - 1u - values_node->count) {
      status = T64_CPU_DIAG_SIZE_LIMIT;
      goto cleanup;
    }
    total_rows += 1u + values_node->count;
    if (total_rows > T64_CPU_DIAG_MAX_ROWS ||
        value_count > T64_CPU_DIAG_MAX_ROWS - values_node->count) {
      status = T64_CPU_DIAG_SIZE_LIMIT;
      goto cleanup;
    }
    value_count += values_node->count;
    if (registration->required) required_value_count += values_node->count;
    names[registration_index] = registration->name;
    registration->value_offset = value_count - values_node->count;
    registration->value_count = values_node->count;
    value_node = values_node->first;
    for (i = 0u; i < values_node->count; i++) {
      t64_cpu_diag_value parsed;
      if (value_node == NULL ||
          !value_fields(value_node, (uint32_t)i, &parsed)) {
        status = T64_CPU_DIAG_INVALID_INVENTORY;
        goto cleanup;
      }
      value_node = value_node->next;
    }
    registration_node = registration_node->next;
  }
  if (registration_node != NULL || required_value_count == 0u) {
    status = T64_CPU_DIAG_INVALID_INVENTORY;
    goto cleanup;
  }
  if (declared_value_count != value_count ||
      declared_required_value_count != required_value_count) {
    status = T64_CPU_DIAG_INVALID_INVENTORY;
    goto cleanup;
  }
  qsort(names, registration_count, sizeof *names, compare_names);
  for (i = 1u; i < registration_count; i++) {
    if (strcmp(names[i - 1u], names[i]) == 0) {
      status = T64_CPU_DIAG_INVALID_INVENTORY;
      goto cleanup;
    }
  }
  inventory->values = (t64_cpu_diag_value *)calloc(value_count,
                                                    sizeof *inventory->values);
  if (value_count != 0u && inventory->values == NULL) {
    status = T64_CPU_DIAG_ALLOCATION_FAILED;
    goto cleanup;
  }
  registration_node = registrations_node->first;
  for (registration_index = 0u; registration_index < registration_count;
       registration_index++) {
    t64_cpu_diag_registration *registration =
        &inventory->registrations[registration_index];
    values_node = t64_json_get(registration_node, "values");
    value_node = values_node->first;
    for (i = 0u; i < values_node->count; i++) {
      if (value_node == NULL ||
          !value_fields(value_node, (uint32_t)i,
                        &inventory->values[registration->value_offset + i])) {
        status = T64_CPU_DIAG_INVALID_INVENTORY;
        goto cleanup;
      }
      value_node = value_node->next;
    }
    registration_node = registration_node->next;
  }
  for (registration_index = 0u; registration_index < registration_count;
       registration_index++) {
    const t64_cpu_diag_registration *registration =
        &inventory->registrations[registration_index];
    const char **value_names;
    size_t j;
    if (registration->value_count < 2u) continue;
    value_names = (const char **)calloc(registration->value_count,
                                        sizeof *value_names);
    if (value_names == NULL) {
      status = T64_CPU_DIAG_ALLOCATION_FAILED;
      goto cleanup;
    }
    for (j = 0u; j < registration->value_count; j++) {
      value_names[j] = inventory->values[registration->value_offset + j].id;
    }
    qsort(value_names, registration->value_count, sizeof *value_names,
          compare_names);
    for (j = 1u; j < registration->value_count; j++) {
      if (strcmp(value_names[j - 1u], value_names[j]) == 0) {
        free(value_names);
        status = T64_CPU_DIAG_INVALID_INVENTORY;
        goto cleanup;
      }
    }
    free(value_names);
  }

  inventory->required_members = (t64_cpu_diag_required_member *)calloc(
      required_value_count, sizeof *inventory->required_members);
  if (inventory->required_members == NULL) {
    status = T64_CPU_DIAG_ALLOCATION_FAILED;
    goto cleanup;
  }
  for (registration_index = 0u; registration_index < registration_count;
       registration_index++) {
    const t64_cpu_diag_registration *registration =
        &inventory->registrations[registration_index];
    size_t j;
    if (!registration->required) continue;
    for (j = 0u; j < registration->value_count; j++) {
      t64_cpu_diag_required_member *member =
          &inventory->required_members[required_member_index++];
      member->registration_index = (uint32_t)registration_index;
      member->value_index = (uint32_t)j;
      member->flat_index = registration->value_offset + j;
    }
  }
  if (required_member_index != required_value_count) {
    status = T64_CPU_DIAG_INVALID_INVENTORY;
    goto cleanup;
  }

  if (!parse_identity(t64_json_get(document.root, "identity"), false,
                      &inventory->identity, &inventory_hash)) {
    status = T64_CPU_DIAG_INVALID_INVENTORY;
    goto cleanup;
  }
  inventory->registration_count = registration_count;
  inventory->value_count = value_count;
  inventory->required_value_count = required_value_count;
  inventory->declared_registration_count = declared_registration_count;
  inventory->declared_value_count = declared_value_count;
  inventory->declared_required_value_count = declared_required_value_count;
  t64_sha256_init(&sha);
  t64_sha256_update(&sha, bytes, length);
  t64_sha256_final(&sha, digest);
  t64_sha256_hex(digest, inventory->sha256);
  inventory->document = document;
  memset(&document, 0, sizeof document);
  *inventory_out = inventory;
  inventory = NULL;
  status = T64_CPU_DIAG_OK;

cleanup:
  free(names);
  if (inventory != NULL) {
    free(inventory->registrations);
    free(inventory->values);
    free(inventory->required_members);
    free(inventory);
  }
  t64_json_free(&document);
  return status;
}

t64_cpu_diag_status t64_cpu_diag_inventory_parse(
    const uint8_t *bytes, size_t length,
    t64_cpu_diag_inventory **inventory_out) {
  return canonical_inventory_parse(bytes, length, inventory_out);
}

void t64_cpu_diag_inventory_destroy(t64_cpu_diag_inventory *inventory) {
  if (inventory == NULL) return;
  free(inventory->registrations);
  free(inventory->values);
  free(inventory->required_members);
  t64_json_free(&inventory->document);
  free(inventory);
}

size_t t64_cpu_diag_inventory_registration_count(
    const t64_cpu_diag_inventory *inventory) {
  return inventory == NULL ? 0u : inventory->registration_count;
}

size_t t64_cpu_diag_inventory_required_value_count(
    const t64_cpu_diag_inventory *inventory) {
  return inventory == NULL ? 0u : inventory->required_value_count;
}

size_t t64_cpu_diag_required_group_count(void) {
  return sizeof t64_cpu_diag_groups / sizeof t64_cpu_diag_groups[0];
}

const char *t64_cpu_diag_required_group_name(size_t index) {
  if (index >= t64_cpu_diag_required_group_count()) return NULL;
  return t64_cpu_diag_groups[index];
}

const char *t64_cpu_diag_inventory_sha256(
    const t64_cpu_diag_inventory *inventory) {
  return inventory == NULL ? NULL : inventory->sha256;
}

const char *t64_cpu_diag_status_name(t64_cpu_diag_status status) {
  static const char *const names[] = {
      "ok",          "invalid-argument", "size-limit", "invalid-json",
      "invalid-schema", "invalid-inventory", "invalid-batch", "incomplete",
      "duplicate-member", "identity-mismatch", "allocation-failed",
      "canonicalization-failed", "output-overflow"};
  if ((size_t)status >= sizeof names / sizeof names[0]) return "unknown";
  return names[(size_t)status];
}

const char *t64_cpu_diag_outcome_name(t64_cpu_diag_outcome outcome) {
  if ((size_t)outcome >=
      sizeof t64_cpu_diag_outcomes / sizeof t64_cpu_diag_outcomes[0]) {
    return NULL;
  }
  return t64_cpu_diag_outcomes[(size_t)outcome];
}

static bool write_key(t64_jsonw *writer, const char *key) {
  return t64_jsonw_key(writer, key, strlen(key)) == T64_JSONW_OK;
}

static bool write_string(t64_jsonw *writer, const char *value) {
  return value != NULL &&
         t64_jsonw_string(writer, value, strlen(value)) == T64_JSONW_OK;
}

static bool write_u64_string(t64_jsonw *writer, uint64_t value) {
  char text[20];
  int length = snprintf(text, sizeof text, "%" PRIu64, value);
  return length > 0 && (size_t)length < sizeof text &&
         t64_jsonw_string(writer, text, (size_t)length) == T64_JSONW_OK;
}

static bool write_batch_identity(t64_jsonw *writer,
                                 const t64_cpu_diag_inventory *inventory) {
  const t64_cpu_diag_identity *identity = &inventory->identity;
  return t64_jsonw_begin_object(writer) == T64_JSONW_OK &&
         write_key(writer, "base_rom_sha256") &&
         write_string(writer, identity->base_rom_sha256) &&
         write_key(writer, "core_build_sha256") &&
         write_string(writer, identity->core_build_sha256) &&
         write_key(writer, "inventory_sha256") &&
         write_string(writer, inventory->sha256) &&
         write_key(writer, "overlay_sha256") &&
         write_string(writer, identity->overlay_sha256) &&
         write_key(writer, "profile_sha256") &&
         write_string(writer, identity->profile_sha256) &&
         write_key(writer, "selected_rom_sha256") &&
         write_string(writer, identity->selected_rom_sha256) &&
         write_key(writer, "selector") && write_string(writer, identity->selector) &&
         write_key(writer, "source_revision") &&
         write_string(writer, identity->source_revision) &&
         write_key(writer, "tool_receipt_sha256") &&
         write_string(writer, identity->tool_receipt_sha256) &&
         t64_jsonw_end_object(writer) == T64_JSONW_OK;
}

static bool valid_member(const t64_cpu_diag_inventory *inventory,
                         uint32_t registration_index, uint32_t value_index,
                         size_t *flat_index_out) {
  const t64_cpu_diag_registration *registration;
  if (inventory == NULL || registration_index >= inventory->registration_count) {
    return false;
  }
  registration = &inventory->registrations[registration_index];
  if (!registration->required || value_index >= registration->value_count) {
    return false;
  }
  if (flat_index_out != NULL) {
    *flat_index_out = registration->value_offset + (size_t)value_index;
  }
  return true;
}

t64_cpu_diag_status t64_cpu_diag_batch_encode(
    const t64_cpu_diag_inventory *inventory, uint32_t registration_index,
    uint32_t value_index, t64_cpu_diag_outcome outcome,
    char *bytes_out, size_t capacity, size_t *length_out) {
  t64_jsonw writer;
  size_t length = 0u;
  const char *outcome_text = t64_cpu_diag_outcome_name(outcome);
  if (inventory == NULL || bytes_out == NULL || length_out == NULL ||
      outcome_text == NULL || capacity == 0u) {
    return T64_CPU_DIAG_INVALID_ARGUMENT;
  }
  if (!valid_member(inventory, registration_index, value_index, NULL)) {
    return T64_CPU_DIAG_INVALID_INVENTORY;
  }
  t64_jsonw_init(&writer, bytes_out, capacity);
  if (t64_jsonw_begin_object(&writer) != T64_JSONW_OK ||
      !write_key(&writer, "identity") ||
      !write_batch_identity(&writer, inventory) ||
      !write_key(&writer, "records") ||
      t64_jsonw_begin_array(&writer) != T64_JSONW_OK ||
      t64_jsonw_begin_object(&writer) != T64_JSONW_OK ||
      !write_key(&writer, "event") || !write_string(&writer, "start") ||
      !write_key(&writer, "registration_index") ||
      t64_jsonw_int(&writer, (int64_t)registration_index) != T64_JSONW_OK ||
      !write_key(&writer, "value_index") ||
      t64_jsonw_int(&writer, (int64_t)value_index) != T64_JSONW_OK ||
      t64_jsonw_end_object(&writer) != T64_JSONW_OK ||
      t64_jsonw_begin_object(&writer) != T64_JSONW_OK ||
      !write_key(&writer, "event") || !write_string(&writer, "outcome") ||
      !write_key(&writer, "status") || !write_string(&writer, outcome_text) ||
      t64_jsonw_end_object(&writer) != T64_JSONW_OK ||
      t64_jsonw_begin_object(&writer) != T64_JSONW_OK ||
      !write_key(&writer, "complete") ||
      t64_jsonw_bool(&writer, 1) != T64_JSONW_OK ||
      !write_key(&writer, "event") || !write_string(&writer, "done") ||
      !write_key(&writer, "record_count") ||
      !write_u64_string(&writer, UINT64_C(3)) ||
      t64_jsonw_end_object(&writer) != T64_JSONW_OK ||
      t64_jsonw_end_array(&writer) != T64_JSONW_OK ||
      !write_key(&writer, "schema") || !write_string(&writer, "t64.cpu-batch") ||
      !write_key(&writer, "schema_version") ||
      t64_jsonw_int(&writer, T64_CPU_DIAG_SCHEMA_VERSION) != T64_JSONW_OK ||
      t64_jsonw_end_object(&writer) != T64_JSONW_OK ||
      t64_jsonw_finish(&writer, &length) != T64_JSONW_OK) {
    return writer.err == T64_JSONW_E_OVERFLOW ? T64_CPU_DIAG_OUTPUT_OVERFLOW
                                               : T64_CPU_DIAG_CANONICALIZATION_FAILED;
  }
  if (length > T64_CPU_DIAG_MAX_BATCH_BYTES) return T64_CPU_DIAG_SIZE_LIMIT;
  *length_out = length;
  return T64_CPU_DIAG_OK;
}

static bool parse_outcome(const t64_json_node *node,
                          t64_cpu_diag_outcome *outcome_out) {
  size_t i;
  if (node == NULL || node->type != T64_JSON_STRING) return false;
  for (i = 0u; i < sizeof t64_cpu_diag_outcomes / sizeof t64_cpu_diag_outcomes[0];
       i++) {
    if (string_equals(node, t64_cpu_diag_outcomes[i])) {
      *outcome_out = (t64_cpu_diag_outcome)i;
      return true;
    }
  }
  return false;
}

static t64_cpu_diag_status parse_batch(
    const t64_cpu_diag_inventory *inventory, const uint8_t *bytes, size_t length,
    uint32_t *registration_index_out, uint32_t *value_index_out,
    t64_cpu_diag_outcome *outcome_out) {
  t64_json_limits limits = {T64_CPU_DIAG_MAX_BATCH_BYTES, 8u, 256u, 64u};
  t64_json_doc document = {0};
  t64_json_error json_error = {T64_JSON_OK, 0u};
  const t64_json_node *records;
  const t64_json_node *start;
  const t64_json_node *outcome_node;
  const t64_json_node *done;
  const t64_json_node *field;
  t64_cpu_diag_identity batch_identity = {0};
  const char *batch_inventory_sha = NULL;
  uint32_t registration_index;
  uint32_t value_index;
  uint64_t record_count;
  t64_cpu_diag_outcome outcome;
  t64_cpu_diag_status status = T64_CPU_DIAG_INVALID_BATCH;

  if (inventory == NULL || bytes == NULL || registration_index_out == NULL ||
      value_index_out == NULL || outcome_out == NULL) {
    return T64_CPU_DIAG_INVALID_ARGUMENT;
  }
  if (length == 0u || length > T64_CPU_DIAG_MAX_BATCH_BYTES) {
    return T64_CPU_DIAG_SIZE_LIMIT;
  }
  if (t64_json_parse(bytes, length, &limits, &document, &json_error) != 0) {
    return T64_CPU_DIAG_INVALID_JSON;
  }
  if (!object_has_exact_keys(document.root, t64_cpu_diag_batch_keys,
                             sizeof t64_cpu_diag_batch_keys /
                                 sizeof t64_cpu_diag_batch_keys[0]) ||
      !parse_schema_version(document.root, "t64.cpu-batch")) {
    status = T64_CPU_DIAG_INVALID_SCHEMA;
    goto cleanup;
  }
  if (!canonical_document(&document, bytes, length)) {
    status = T64_CPU_DIAG_INVALID_SCHEMA;
    goto cleanup;
  }
  if (!parse_identity(t64_json_get(document.root, "identity"), true,
                      &batch_identity, &batch_inventory_sha)) {
    status = T64_CPU_DIAG_INVALID_BATCH;
    goto cleanup;
  }
  if (!identity_matches(&inventory->identity, &batch_identity) ||
      strcmp(inventory->sha256, batch_inventory_sha) != 0) {
    status = T64_CPU_DIAG_IDENTITY_MISMATCH;
    goto cleanup;
  }
  records = t64_json_get(document.root, "records");
  if (records == NULL || records->type != T64_JSON_ARRAY) {
    status = T64_CPU_DIAG_INCOMPLETE;
    goto cleanup;
  }
  if (records->count < 3u) {
    status = T64_CPU_DIAG_INCOMPLETE;
    goto cleanup;
  }
  if (records->count != 3u) {
    status = T64_CPU_DIAG_INVALID_BATCH;
    goto cleanup;
  }
  start = records->first;
  outcome_node = start == NULL ? NULL : start->next;
  done = outcome_node == NULL ? NULL : outcome_node->next;
  if (!object_has_exact_keys(start, t64_cpu_diag_start_keys,
                             sizeof t64_cpu_diag_start_keys /
                                 sizeof t64_cpu_diag_start_keys[0]) ||
      !string_equals(t64_json_get(start, "event"), "start") ||
      !index_node(t64_json_get(start, "registration_index"),
                  &registration_index) ||
      !index_node(t64_json_get(start, "value_index"), &value_index)) {
    status = T64_CPU_DIAG_INVALID_BATCH;
    goto cleanup;
  }
  if (!valid_member(inventory, registration_index, value_index, NULL)) {
    status = T64_CPU_DIAG_INVALID_BATCH;
    goto cleanup;
  }
  if (!object_has_exact_keys(outcome_node, t64_cpu_diag_outcome_keys,
                             sizeof t64_cpu_diag_outcome_keys /
                                 sizeof t64_cpu_diag_outcome_keys[0]) ||
      !string_equals(t64_json_get(outcome_node, "event"), "outcome") ||
      !parse_outcome(t64_json_get(outcome_node, "status"), &outcome) ||
      !object_has_exact_keys(done, t64_cpu_diag_done_keys,
                             sizeof t64_cpu_diag_done_keys /
                                 sizeof t64_cpu_diag_done_keys[0]) ||
      !string_equals(t64_json_get(done, "event"), "done")) {
    status = T64_CPU_DIAG_INVALID_BATCH;
    goto cleanup;
  }
  field = t64_json_get(done, "complete");
  if (field == NULL || field->type != T64_JSON_BOOL || field->num != 1 ||
      !t64_json_u64_string(t64_json_get(done, "record_count"), &record_count) ||
      record_count != UINT64_C(3)) {
    status = T64_CPU_DIAG_INCOMPLETE;
    goto cleanup;
  }
  *registration_index_out = registration_index;
  *value_index_out = value_index;
  *outcome_out = outcome;
  status = T64_CPU_DIAG_OK;

cleanup:
  t64_json_free(&document);
  return status;
}

t64_cpu_diag_status t64_cpu_diag_aggregate_create(
    const t64_cpu_diag_inventory *inventory,
    t64_cpu_diag_aggregate **aggregate_out) {
  t64_cpu_diag_aggregate *aggregate;
  if (inventory == NULL || aggregate_out == NULL) {
    return T64_CPU_DIAG_INVALID_ARGUMENT;
  }
  if (inventory->required_value_count == 0u) {
    return T64_CPU_DIAG_INVALID_INVENTORY;
  }
  aggregate = (t64_cpu_diag_aggregate *)calloc(1u, sizeof *aggregate);
  if (aggregate == NULL) return T64_CPU_DIAG_ALLOCATION_FAILED;
  aggregate->seen = (uint8_t *)calloc(inventory->value_count, sizeof *aggregate->seen);
  aggregate->outcomes = (uint8_t *)calloc(inventory->value_count,
                                          sizeof *aggregate->outcomes);
  if (aggregate->seen == NULL || aggregate->outcomes == NULL) {
    free(aggregate->seen);
    free(aggregate->outcomes);
    free(aggregate);
    return T64_CPU_DIAG_ALLOCATION_FAILED;
  }
  aggregate->inventory = inventory;
  *aggregate_out = aggregate;
  return T64_CPU_DIAG_OK;
}

void t64_cpu_diag_aggregate_destroy(t64_cpu_diag_aggregate *aggregate) {
  if (aggregate == NULL) return;
  free(aggregate->seen);
  free(aggregate->outcomes);
  free(aggregate);
}

t64_cpu_diag_status t64_cpu_diag_aggregate_add_batch(
    t64_cpu_diag_aggregate *aggregate, const uint8_t *bytes, size_t length) {
  uint32_t registration_index;
  uint32_t value_index;
  size_t flat_index;
  t64_cpu_diag_outcome outcome;
  t64_cpu_diag_status status;
  if (aggregate == NULL) return T64_CPU_DIAG_INVALID_ARGUMENT;
  status = parse_batch(aggregate->inventory, bytes, length, &registration_index,
                       &value_index, &outcome);
  if (status != T64_CPU_DIAG_OK) {
    aggregate->invalid = true;
    return status;
  }
  if (!valid_member(aggregate->inventory, registration_index, value_index,
                    &flat_index)) {
    aggregate->invalid = true;
    return T64_CPU_DIAG_INVALID_BATCH;
  }
  if (aggregate->seen[flat_index] != 0u) {
    aggregate->invalid = true;
    return T64_CPU_DIAG_DUPLICATE_MEMBER;
  }
  aggregate->seen[flat_index] = 1u;
  aggregate->outcomes[flat_index] = (uint8_t)outcome;
  return T64_CPU_DIAG_OK;
}

bool t64_cpu_diag_aggregate_is_complete(
    const t64_cpu_diag_aggregate *aggregate) {
  size_t i;
  if (aggregate == NULL || aggregate->inventory == NULL ||
      aggregate->inventory->required_value_count == 0u || aggregate->invalid) {
    return false;
  }
  for (i = 0u; i < t64_cpu_diag_required_group_count(); i++) {
    size_t registration_index;
    size_t expected = 0u;
    for (registration_index = 0u;
         registration_index < aggregate->inventory->registration_count;
         registration_index++) {
      const t64_cpu_diag_registration *registration =
          &aggregate->inventory->registrations[registration_index];
      if (registration->required &&
          strcmp(registration->group,
                 t64_cpu_diag_required_group_name(i)) == 0) {
        expected += registration->value_count;
      }
    }
    if (expected == 0u) return false;
  }
  for (i = 0u; i < aggregate->inventory->required_value_count; i++) {
    size_t flat_index = aggregate->inventory->required_members[i].flat_index;
    if (aggregate->seen[flat_index] == 0u ||
        aggregate->outcomes[flat_index] > T64_CPU_DIAG_OUTCOME_FAIL) {
      return false;
    }
  }
  return true;
}

size_t t64_cpu_diag_aggregate_member_count(
    const t64_cpu_diag_aggregate *aggregate) {
  return aggregate == NULL ? 0u : aggregate->inventory->required_value_count;
}

bool t64_cpu_diag_aggregate_member_at(
    const t64_cpu_diag_aggregate *aggregate, size_t required_ordinal,
    t64_cpu_diag_member_result *result_out) {
  const t64_cpu_diag_required_member *member;
  const t64_cpu_diag_registration *registration;
  const t64_cpu_diag_value *value;
  if (aggregate == NULL || result_out == NULL ||
      required_ordinal >= aggregate->inventory->required_value_count) {
    return false;
  }
  member = &aggregate->inventory->required_members[required_ordinal];
  registration =
      &aggregate->inventory->registrations[member->registration_index];
  value = &aggregate->inventory->values[member->flat_index];
  result_out->registration_index = member->registration_index;
  result_out->value_index = member->value_index;
  result_out->group = registration->group;
  result_out->registration_name = registration->name;
  result_out->value_id = value->id;
  result_out->has_outcome = aggregate->seen[member->flat_index] != 0u;
  result_out->outcome = result_out->has_outcome
                            ? (t64_cpu_diag_outcome)aggregate->outcomes[
                                  member->flat_index]
                            : T64_CPU_DIAG_OUTCOME_INCOMPLETE;
  return true;
}

t64_cpu_diag_status t64_cpu_diag_aggregate_group(
    const t64_cpu_diag_aggregate *aggregate, const char *group,
    t64_cpu_diag_group_status *status_out, size_t *expected_out,
    size_t *reached_out, size_t *passed_out, size_t *failed_out) {
  size_t expected = 0u;
  size_t reached = 0u;
  size_t passed = 0u;
  size_t failed = 0u;
  size_t i;
  bool complete;
  if (aggregate == NULL || group == NULL || status_out == NULL) {
    return T64_CPU_DIAG_INVALID_ARGUMENT;
  }
  if (!valid_group(group)) return T64_CPU_DIAG_INVALID_SCHEMA;
  complete = !aggregate->invalid;
  for (i = 0u; i < aggregate->inventory->registration_count; i++) {
    const t64_cpu_diag_registration *registration =
        &aggregate->inventory->registrations[i];
    size_t j;
    if (!registration->required || strcmp(registration->group, group) != 0) {
      continue;
    }
    expected += registration->value_count;
    for (j = 0u; j < registration->value_count; j++) {
      size_t flat_index = registration->value_offset + j;
      if (aggregate->seen[flat_index] == 0u) {
        complete = false;
      } else if (aggregate->outcomes[flat_index] == T64_CPU_DIAG_OUTCOME_PASS) {
        reached++;
        passed++;
      } else if (aggregate->outcomes[flat_index] == T64_CPU_DIAG_OUTCOME_FAIL) {
        reached++;
        failed++;
      } else {
        complete = false;
      }
    }
  }
  *status_out = (!complete || expected == 0u)
                    ? T64_CPU_DIAG_GROUP_INCOMPLETE
                    : (failed != 0u ? T64_CPU_DIAG_GROUP_FAIL
                                    : T64_CPU_DIAG_GROUP_PASS);
  if (expected_out != NULL) *expected_out = expected;
  if (reached_out != NULL) *reached_out = reached;
  if (passed_out != NULL) *passed_out = passed;
  if (failed_out != NULL) *failed_out = failed;
  return T64_CPU_DIAG_OK;
}
