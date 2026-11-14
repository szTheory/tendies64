/* Shared bounded helpers for closed local diagnostic protocols. */
#ifndef T64_DIAG_COMMON_H
#define T64_DIAG_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "t64_json.h"
#include "t64_jsonw.h"

typedef struct t64_diag_partition t64_diag_partition;

typedef enum {
  T64_DIAG_PARTITION_OK = 0,
  T64_DIAG_PARTITION_INVALID_ARGUMENT,
  T64_DIAG_PARTITION_OUT_OF_RANGE,
  T64_DIAG_PARTITION_DUPLICATE,
  T64_DIAG_PARTITION_ALLOCATION_FAILED
} t64_diag_partition_status;

bool t64_diag_json_string_equals(const t64_json_node *node,
                                 const char *value);
bool t64_diag_json_object_has_exact_keys(const t64_json_node *object,
                                         const char *const *keys,
                                         size_t count);
const t64_json_node *t64_diag_json_string_member(
    const t64_json_node *object, const char *key);
bool t64_diag_json_printable_text(const t64_json_node *node, size_t maximum,
                                  bool allow_empty);
bool t64_diag_json_lowercase_hex(const t64_json_node *node, size_t digits);
bool t64_diag_json_selector_text(const t64_json_node *node, size_t maximum);
bool t64_diag_json_index16(const t64_json_node *node, uint32_t *value_out);
bool t64_diag_json_canonical_document(const t64_json_doc *document,
                                      const uint8_t *bytes, size_t length);

bool t64_diag_json_write_key(t64_jsonw *writer, const char *key);
bool t64_diag_json_write_string(t64_jsonw *writer, const char *value);
bool t64_diag_json_write_u64_string(t64_jsonw *writer, uint64_t value);

t64_diag_partition_status t64_diag_partition_create(
    size_t member_count, t64_diag_partition **partition_out);
void t64_diag_partition_destroy(t64_diag_partition *partition);
t64_diag_partition_status t64_diag_partition_add(
    t64_diag_partition *partition, size_t member_index, uint8_t outcome);
bool t64_diag_partition_seen(const t64_diag_partition *partition,
                             size_t member_index);
bool t64_diag_partition_outcome(const t64_diag_partition *partition,
                                size_t member_index, uint8_t *outcome_out);
size_t t64_diag_partition_member_count(const t64_diag_partition *partition);

#endif /* T64_DIAG_COMMON_H */
