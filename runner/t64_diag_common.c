/* Shared bounded helpers for closed local diagnostic protocols. */
#include "t64_diag_common.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct t64_diag_partition {
  size_t member_count;
  size_t reached_count;
  uint8_t *seen;
  uint8_t *outcomes;
};

bool t64_diag_json_string_equals(const t64_json_node *node,
                                 const char *value) {
  size_t length;
  if (node == NULL || node->type != T64_JSON_STRING || value == NULL) {
    return false;
  }
  length = strlen(value);
  return node->len == length && memcmp(node->str, value, length) == 0;
}

bool t64_diag_json_object_has_exact_keys(const t64_json_node *object,
                                         const char *const *keys,
                                         size_t count) {
  return t64_json_check_keys(object, keys, count, keys, count, NULL) ==
         T64_JSON_KEYS_OK;
}

const t64_json_node *t64_diag_json_string_member(
    const t64_json_node *object, const char *key) {
  const t64_json_node *node = t64_json_get(object, key);
  return node != NULL && node->type == T64_JSON_STRING ? node : NULL;
}

bool t64_diag_json_printable_text(const t64_json_node *node, size_t maximum,
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

bool t64_diag_json_lowercase_hex(const t64_json_node *node, size_t digits) {
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

bool t64_diag_json_selector_text(const t64_json_node *node, size_t maximum) {
  size_t i;
  if (!t64_diag_json_printable_text(node, maximum, false)) return false;
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

bool t64_diag_json_index16(const t64_json_node *node, uint32_t *value_out) {
  if (node == NULL || node->type != T64_JSON_INT || node->num < 0 ||
      node->num > UINT16_MAX || value_out == NULL) {
    return false;
  }
  *value_out = (uint32_t)node->num;
  return true;
}

bool t64_diag_json_canonical_document(const t64_json_doc *document,
                                      const uint8_t *bytes, size_t length) {
  char *canonical;
  size_t canonical_length = 0u;
  t64_jsonw writer;
  bool matches = false;
  if (document == NULL || document->root == NULL || bytes == NULL ||
      length == 0u) {
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

bool t64_diag_json_write_key(t64_jsonw *writer, const char *key) {
  return writer != NULL && key != NULL &&
         t64_jsonw_key(writer, key, strlen(key)) == T64_JSONW_OK;
}

bool t64_diag_json_write_string(t64_jsonw *writer, const char *value) {
  return writer != NULL && value != NULL &&
         t64_jsonw_string(writer, value, strlen(value)) == T64_JSONW_OK;
}

bool t64_diag_json_write_u64_string(t64_jsonw *writer, uint64_t value) {
  char text[20];
  int length;
  if (writer == NULL) return false;
  length = snprintf(text, sizeof text, "%" PRIu64, value);
  return length > 0 && (size_t)length < sizeof text &&
         t64_jsonw_string(writer, text, (size_t)length) == T64_JSONW_OK;
}

t64_diag_partition_status t64_diag_partition_create(
    size_t member_count, t64_diag_partition **partition_out) {
  t64_diag_partition *partition;
  if (partition_out == NULL) {
    return T64_DIAG_PARTITION_INVALID_ARGUMENT;
  }
  *partition_out = NULL;
  if (member_count == 0u) return T64_DIAG_PARTITION_INVALID_ARGUMENT;
  partition = (t64_diag_partition *)calloc(1u, sizeof *partition);
  if (partition == NULL) return T64_DIAG_PARTITION_ALLOCATION_FAILED;
  partition->seen = (uint8_t *)calloc(member_count, sizeof *partition->seen);
  partition->outcomes =
      (uint8_t *)calloc(member_count, sizeof *partition->outcomes);
  if (partition->seen == NULL || partition->outcomes == NULL) {
    free(partition->seen);
    free(partition->outcomes);
    free(partition);
    return T64_DIAG_PARTITION_ALLOCATION_FAILED;
  }
  partition->member_count = member_count;
  *partition_out = partition;
  return T64_DIAG_PARTITION_OK;
}

void t64_diag_partition_destroy(t64_diag_partition *partition) {
  if (partition == NULL) return;
  free(partition->seen);
  free(partition->outcomes);
  free(partition);
}

t64_diag_partition_status t64_diag_partition_add(
    t64_diag_partition *partition, size_t member_index, uint8_t outcome) {
  if (partition == NULL) return T64_DIAG_PARTITION_INVALID_ARGUMENT;
  if (member_index >= partition->member_count) {
    return T64_DIAG_PARTITION_OUT_OF_RANGE;
  }
  if (partition->seen[member_index] != 0u) {
    return T64_DIAG_PARTITION_DUPLICATE;
  }
  partition->seen[member_index] = 1u;
  partition->outcomes[member_index] = outcome;
  partition->reached_count++;
  return T64_DIAG_PARTITION_OK;
}

bool t64_diag_partition_seen(const t64_diag_partition *partition,
                             size_t member_index) {
  return partition != NULL && member_index < partition->member_count &&
         partition->seen[member_index] != 0u;
}

bool t64_diag_partition_outcome(const t64_diag_partition *partition,
                                size_t member_index, uint8_t *outcome_out) {
  if (partition == NULL || member_index >= partition->member_count ||
      !t64_diag_partition_seen(partition, member_index) ||
      outcome_out == NULL) {
    return false;
  }
  *outcome_out = partition->outcomes[member_index];
  return true;
}

size_t t64_diag_partition_member_count(const t64_diag_partition *partition) {
  return partition == NULL ? 0u : partition->reached_count;
}
