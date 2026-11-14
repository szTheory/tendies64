#include "t64_cpu_diag_protocol.h"
#include "t64_diag_common.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t inventory_json[] =
    "{"
    "\"declared_counts\":{\"registrations\":\"1\",\"required_values\":\"1\",\"values\":\"1\"},"
    "\"identity\":{"
    "\"base_rom_sha256\":\"0000000000000000000000000000000000000000000000000000000000000000\","
    "\"core_build_sha256\":\"1111111111111111111111111111111111111111111111111111111111111111\","
    "\"overlay_sha256\":\"2222222222222222222222222222222222222222222222222222222222222222\","
    "\"profile_sha256\":\"3333333333333333333333333333333333333333333333333333333333333333\","
    "\"source_revision\":\"5555555555555555555555555555555555555555\","
    "\"tool_receipt_sha256\":\"6666666666666666666666666666666666666666666666666666666666666666\"},"
    "\"registrations\":[{\"execution\":\"values\",\"group\":\"arithmetic\",\"index\":0,\"lane\":\"required-base\",\"level\":\"Weird\",\"name\":\"authored.case\",\"reason\":\"required\",\"values\":[{\"id\":\"case.one\",\"index\":0,\"selected_rom_sha256\":\"4444444444444444444444444444444444444444444444444444444444444444\",\"selector\":\"authored-selector\"}]}],"
    "\"schema\":\"t64.cpu-inventory\",\"schema_version\":3}\n";

static const char expected_pass_batch[] =
    "{\"identity\":{\"base_rom_sha256\":\"0000000000000000000000000000000000000000000000000000000000000000\",\"core_build_sha256\":\"1111111111111111111111111111111111111111111111111111111111111111\",\"inventory_sha256\":\"0b5b78464f4cb5743af96e6a04fce411be4abe689c3ef3b88e74d575b15ce82b\",\"overlay_sha256\":\"2222222222222222222222222222222222222222222222222222222222222222\",\"profile_sha256\":\"3333333333333333333333333333333333333333333333333333333333333333\",\"selected_rom_sha256\":\"4444444444444444444444444444444444444444444444444444444444444444\",\"selector\":\"authored-selector\",\"source_revision\":\"5555555555555555555555555555555555555555\",\"tool_receipt_sha256\":\"6666666666666666666666666666666666666666666666666666666666666666\"},\"records\":[{\"event\":\"start\",\"registration_index\":0,\"value_index\":0},{\"event\":\"outcome\",\"status\":\"pass\"},{\"complete\":true,\"event\":\"done\",\"record_count\":\"3\"}],\"schema\":\"t64.cpu-batch\",\"schema_version\":2}\n";

static const char *const expected_groups[] = {
    "address_error_exception", "arithmetic", "cache", "cop0", "cop1",
    "cop_unusable", "endian_re", "exception_instructions", "jumps",
    "overflow_exception", "privilege", "tlb", "tlb64", "traps"};

#if defined(__clang__) || defined(__GNUC__)
__attribute__((format(printf, 4, 5)))
#endif
static bool appendf(char *output, size_t capacity, size_t *length,
                    const char *format, ...) {
  va_list args;
  int written;
  if (*length >= capacity) return false;
  va_start(args, format);
  written = vsnprintf(output + *length, capacity - *length, format, args);
  va_end(args);
  if (written < 0 || (size_t)written >= capacity - *length) return false;
  *length += (size_t)written;
  return true;
}

static bool append_registration(char *output, size_t capacity, size_t *length,
                                uint32_t registration_index, const char *name,
                                const char *group, const char *level,
                                const char *lane, const char *reason,
                                const char *execution,
                                const char *selector,
                                const char *const *value_ids,
                                size_t value_count) {
  size_t i;
  if (!appendf(output, capacity, length,
               "{\"execution\":\"%s\",\"group\":\"%s\",\"index\":%u,\"lane\":\"%s\",\"level\":\"%s\",\"name\":\"%s\",\"reason\":\"%s\",\"values\":[",
               execution, group, registration_index, lane, level, name,
               reason)) {
    return false;
  }
  for (i = 0u; i < value_count; i++) {
    if (i != 0u && !appendf(output, capacity, length, ",")) return false;
    if (value_count == 2u && strcmp(group, "arithmetic") == 0 && i == 1u) {
      if (!appendf(output, capacity, length,
                   "{\"id\":\"%s\",\"index\":%zu,"
                   "\"selected_rom_sha256\":\"7777777777777777777777777777777777777777777777777777777777777777\","
                   "\"selector\":\"second-selector\"}", value_ids[i], i)) {
        return false;
      }
    } else if (!appendf(
                   output, capacity, length,
                   "{\"id\":\"%s\",\"index\":%zu,"
                   "\"selected_rom_sha256\":\"4444444444444444444444444444444444444444444444444444444444444444\","
                   "\"selector\":\"%s\"}", value_ids[i], i, selector)) {
      return false;
    }
  }
  return appendf(output, capacity, length, "]}");
}

static bool build_authored_inventory(
    char *output, size_t capacity, size_t *length_out,
    const char *const *groups, size_t group_count, bool include_other_lanes,
    bool two_arithmetic_values, size_t selector_length, size_t name_length,
    size_t value_id_length) {
  static const char *const other_lanes[][3] = {
      {"timing", "Timing", "not-required-timing"},
      {"cycle", "Cycle", "not-required-cycle"},
      {"cop0-hazard", "COP0Hazard", "not-required-cop0-hazard"},
      {"quirk", "PoorlyUnderstoodQuirk", "not-required-quirk"},
      {"stress", "StressTest", "not-required-stress"}};
  char selector[T64_CPU_DIAG_MAX_SELECTOR_BYTES + 1u];
  char name[T64_CPU_DIAG_MAX_NAME_BYTES + 1u];
  char value_id[T64_CPU_DIAG_MAX_VALUE_ID_BYTES + 1u];
  char second_value_id[T64_CPU_DIAG_MAX_VALUE_ID_BYTES + 1u];
  const char *values[2];
  size_t registration_count = group_count + (include_other_lanes ? 6u : 0u);
  size_t required_value_count = group_count;
  size_t value_count;
  size_t length = 0u;
  size_t i;
  uint32_t registration_index = 0u;
  if (groups == NULL || group_count == 0u || output == NULL || length_out == NULL ||
      selector_length == 0u || selector_length > T64_CPU_DIAG_MAX_SELECTOR_BYTES ||
      name_length > T64_CPU_DIAG_MAX_NAME_BYTES ||
      value_id_length > T64_CPU_DIAG_MAX_VALUE_ID_BYTES) {
    return false;
  }
  if (two_arithmetic_values) {
    for (i = 0u; i < group_count; i++) {
      if (strcmp(groups[i], "arithmetic") == 0) required_value_count++;
    }
  }
  value_count = required_value_count + (include_other_lanes ? 5u : 0u);
  (void)memset(selector, 's', selector_length);
  selector[selector_length] = '\0';
  if (!appendf(output, capacity, &length,
               "{\"declared_counts\":{\"registrations\":\"%zu\",\"required_values\":\"%zu\",\"values\":\"%zu\"},"
               "\"identity\":{\"base_rom_sha256\":\"0000000000000000000000000000000000000000000000000000000000000000\","
               "\"core_build_sha256\":\"1111111111111111111111111111111111111111111111111111111111111111\","
               "\"overlay_sha256\":\"2222222222222222222222222222222222222222222222222222222222222222\","
               "\"profile_sha256\":\"3333333333333333333333333333333333333333333333333333333333333333\","
               "\"source_revision\":\"5555555555555555555555555555555555555555\","
               "\"tool_receipt_sha256\":\"6666666666666666666666666666666666666666666666666666666666666666\"},"
               "\"registrations\":[",
               registration_count, required_value_count, value_count)) {
    return false;
  }
  for (i = 0u; i < group_count; i++) {
    size_t value_count_for_registration =
        two_arithmetic_values && strcmp(groups[i], "arithmetic") == 0 ? 2u : 1u;
    static const char *const required_levels[] = {
        "BasicFunctionality", "RarelyUsed", "Weird"};
    const char *level = required_levels[i % 3u];
    (void)snprintf(name, sizeof name, "authored.required.%02zu", i);
    if (name_length != 0u) {
      (void)memset(name, 'n', name_length);
      name[name_length] = '\0';
    }
    (void)snprintf(value_id, sizeof value_id, "authored.value.%02zu.0", i);
    (void)snprintf(second_value_id, sizeof second_value_id,
                   "authored.value.%02zu.1", i);
    if (value_id_length != 0u) {
      (void)memset(value_id, 'v', value_id_length);
      value_id[value_id_length] = '\0';
      (void)memset(second_value_id, 'w', value_id_length);
      second_value_id[value_id_length] = '\0';
    }
    values[0] = value_id;
    values[1] = second_value_id;
    if (i != 0u && !appendf(output, capacity, &length, ",")) return false;
    if (!append_registration(output, capacity, &length, registration_index,
                             name, groups[i], level, "required-base", "required",
                             "values", selector, values,
                             value_count_for_registration)) {
      return false;
    }
    registration_index++;
  }
  if (include_other_lanes) {
    for (i = 0u; i < sizeof other_lanes / sizeof other_lanes[0]; i++) {
      (void)snprintf(name, sizeof name, "authored.nonrequired.%02zu", i);
      (void)snprintf(value_id, sizeof value_id, "authored.nonrequired.value.%02zu", i);
      values[0] = value_id;
      if (!appendf(output, capacity, &length, ",") ||
          !append_registration(output, capacity, &length, registration_index,
                               name, "arithmetic", other_lanes[i][1],
                               other_lanes[i][0], other_lanes[i][2], "once",
                               selector, (const char *const[]){"once"}, 1u)) {
        return false;
      }
      registration_index++;
    }
    if (!appendf(output, capacity, &length, ",") ||
        !append_registration(output, capacity, &length, registration_index,
                             "authored.non-cpu", "non-cpu", "RDPBasic",
                             "skipped-non-cpu", "outside-cpu-scope", "omitted",
                             selector, NULL, 0u)) {
      return false;
    }
  }
  if (!appendf(output, capacity, &length,
               "],\"schema\":\"t64.cpu-inventory\",\"schema_version\":3}\n")) {
    return false;
  }
  *length_out = length;
  return true;
}

static unsigned failures;

static bool replace_text(char *bytes, size_t *length, size_t capacity,
                         const char *from, const char *to);

static void expect_status(const char *case_name, t64_cpu_diag_status actual,
                          t64_cpu_diag_status expected) {
  if (actual != expected) {
    (void)fprintf(stderr, "%s expected %s, observed %s\n", case_name,
                  t64_cpu_diag_status_name(expected),
                  t64_cpu_diag_status_name(actual));
    failures++;
  }
}

static void test_shared_partition_primitive(void) {
  t64_diag_partition *partition = NULL;
  uint8_t outcome = UINT8_MAX;
  if (t64_diag_partition_create(3u, &partition) != T64_DIAG_PARTITION_OK ||
      partition == NULL) {
    (void)fprintf(stderr, "shared partition allocation failed\n");
    failures++;
    return;
  }
  if (t64_diag_partition_member_count(partition) != 0u ||
      t64_diag_partition_seen(partition, 1u)) {
    (void)fprintf(stderr, "shared partition started with a reached member\n");
    failures++;
  }
  if (t64_diag_partition_add(partition, 1u, 4u) != T64_DIAG_PARTITION_OK ||
      !t64_diag_partition_seen(partition, 1u) ||
      !t64_diag_partition_outcome(partition, 1u, &outcome) || outcome != 4u ||
      t64_diag_partition_member_count(partition) != 1u) {
    (void)fprintf(stderr, "shared partition lost an accepted member result\n");
    failures++;
  }
  if (t64_diag_partition_add(partition, 1u, 4u) !=
      T64_DIAG_PARTITION_DUPLICATE) {
    (void)fprintf(stderr, "shared partition accepted a duplicate member\n");
    failures++;
  }
  if (t64_diag_partition_add(partition, 3u, 4u) !=
      T64_DIAG_PARTITION_OUT_OF_RANGE) {
    (void)fprintf(stderr, "shared partition accepted an out-of-range member\n");
    failures++;
  }
  if (t64_diag_partition_outcome(partition, 2u, &outcome)) {
    (void)fprintf(stderr, "shared partition reported a missing result\n");
    failures++;
  }
  t64_diag_partition_destroy(partition);
}

static t64_cpu_diag_inventory *parse_authored_inventory(void) {
  t64_cpu_diag_inventory *inventory = NULL;
  t64_cpu_diag_status status = t64_cpu_diag_inventory_parse(
      inventory_json, sizeof inventory_json - 1u, &inventory);
  expect_status("authored inventory parse", status, T64_CPU_DIAG_OK);
  return inventory;
}

static void test_inventory_v3_base_levels_and_stale_v2(void) {
  char bytes[sizeof inventory_json + 64u];
  size_t length = sizeof inventory_json - 1u;
  t64_cpu_diag_inventory *inventory = NULL;
  memcpy(bytes, inventory_json, length + 1u);
  expect_status("v3 Weird base member accepted",
                t64_cpu_diag_inventory_parse((const uint8_t *)bytes, length,
                                             &inventory),
                T64_CPU_DIAG_OK);
  t64_cpu_diag_inventory_destroy(inventory);
  inventory = NULL;

  memcpy(bytes, inventory_json, length + 1u);
  if (!replace_text(bytes, &length, sizeof bytes,
                    "\"schema_version\":3", "\"schema_version\":2")) {
    (void)fprintf(stderr, "could not construct stale v2 inventory control\n");
    failures++;
  } else {
    expect_status("stale inventory v2 rejected",
                  t64_cpu_diag_inventory_parse((const uint8_t *)bytes, length,
                                               &inventory),
                  T64_CPU_DIAG_INVALID_SCHEMA);
  }
  t64_cpu_diag_inventory_destroy(inventory);
  inventory = NULL;

  memcpy(bytes, inventory_json, sizeof inventory_json);
  length = sizeof inventory_json - 1u;
  if (!replace_text(bytes, &length, sizeof bytes,
                    "\"lane\":\"required-base\"",
                    "\"lane\":\"weird\"")) {
    (void)fprintf(stderr, "could not construct v3 excluded Weird-lane control\n");
    failures++;
  } else {
    expect_status("v3 Weird cannot be reclassified outside base",
                  t64_cpu_diag_inventory_parse((const uint8_t *)bytes, length,
                                               &inventory),
                  T64_CPU_DIAG_INVALID_INVENTORY);
  }
  t64_cpu_diag_inventory_destroy(inventory);
}

static bool make_batch(const t64_cpu_diag_inventory *inventory,
                       t64_cpu_diag_outcome outcome, char *bytes,
                       size_t capacity, size_t *length) {
  t64_cpu_diag_status status = t64_cpu_diag_batch_encode(
      inventory, 0u, 0u, outcome, bytes, capacity, length);
  expect_status("authored batch encode", status, T64_CPU_DIAG_OK);
  return status == T64_CPU_DIAG_OK;
}

static bool remove_done_record(const char *bytes, size_t length, char *output,
                               size_t *output_length) {
  static const char done_record[] =
      ",{\"complete\":true,\"event\":\"done\",\"record_count\":\"3\"}";
  const char *match = strstr(bytes, done_record);
  size_t prefix_length;
  size_t suffix_offset;
  if (match == NULL) return false;
  prefix_length = (size_t)(match - bytes);
  suffix_offset = prefix_length + sizeof done_record - 1u;
  if (suffix_offset > length) return false;
  memcpy(output, bytes, prefix_length);
  memcpy(output + prefix_length, bytes + suffix_offset, length - suffix_offset);
  *output_length = length - (sizeof done_record - 1u);
  return true;
}

static bool empty_inventory_bytes(uint8_t *output, size_t capacity,
                                  size_t *length_out) {
  static const char registrations[] = "\"registrations\":[";
  const char *begin = strstr((const char *)inventory_json, registrations);
  const char *rows_end;
  size_t prefix_length;
  size_t suffix_length;
  size_t total;
  if (begin == NULL) return false;
  begin += sizeof registrations - 1u;
  rows_end = strstr(begin, "],\"schema\"");
  if (rows_end == NULL) return false;
  prefix_length = (size_t)(begin - (const char *)inventory_json);
  suffix_length = sizeof inventory_json - 1u - (size_t)(rows_end -
                                                        (const char *)inventory_json);
  total = prefix_length + suffix_length;
  if (total > capacity) return false;
  memcpy(output, inventory_json, prefix_length);
  memcpy(output + prefix_length, rows_end, suffix_length);
  *length_out = total;
  return true;
}

static bool make_inventory(char *bytes, size_t capacity, size_t *length_out,
                           size_t group_count, bool include_other_lanes,
                           bool two_arithmetic_values) {
  return build_authored_inventory(bytes, capacity, length_out, expected_groups,
                                  group_count, include_other_lanes,
                                  two_arithmetic_values, 17u, 0u, 0u);
}

static bool add_required_batch(t64_cpu_diag_inventory *inventory,
                               t64_cpu_diag_aggregate *aggregate,
                               uint32_t registration_index,
                               uint32_t value_index,
                               t64_cpu_diag_outcome outcome) {
  char bytes[T64_CPU_DIAG_MAX_BATCH_BYTES + 1u];
  size_t length = 0u;
  t64_cpu_diag_status status = t64_cpu_diag_batch_encode(
      inventory, registration_index, value_index, outcome, bytes, sizeof bytes,
      &length);
  if (status != T64_CPU_DIAG_OK) {
    (void)fprintf(stderr, "batch encode expected ok, observed %s\n",
                  t64_cpu_diag_status_name(status));
    failures++;
    return false;
  }
  status = t64_cpu_diag_aggregate_add_batch(
      aggregate, (const uint8_t *)bytes, length);
  if (status != T64_CPU_DIAG_OK) {
    (void)fprintf(stderr, "batch add expected ok, observed %s\n",
                  t64_cpu_diag_status_name(status));
    failures++;
    return false;
  }
  return true;
}

static bool add_all_required_in_reverse(t64_cpu_diag_inventory *inventory,
                                        t64_cpu_diag_aggregate *aggregate,
                                        size_t group_count,
                                        size_t skip_registration,
                                        size_t skip_value) {
  size_t registration_index = group_count;
  while (registration_index > 0u) {
    size_t current = --registration_index;
    size_t value_count = current == 1u ? 2u : 1u;
    size_t value_index = value_count;
    while (value_index > 0u) {
      size_t current_value = --value_index;
      if (current == skip_registration && current_value == skip_value) continue;
      if (!add_required_batch(inventory, aggregate, (uint32_t)current,
                              (uint32_t)current_value,
                              T64_CPU_DIAG_OUTCOME_PASS)) {
        return false;
      }
    }
  }
  return true;
}

static bool replace_text(char *bytes, size_t *length, size_t capacity,
                         const char *from, const char *to) {
  char *match = strstr(bytes, from);
  size_t from_length = strlen(from);
  size_t to_length = strlen(to);
  size_t tail_length;
  if (match == NULL || *length + to_length < from_length ||
      *length + to_length - from_length + 1u > capacity) {
    return false;
  }
  tail_length = *length - (size_t)(match - bytes) - from_length;
  memmove(match + to_length, match + from_length, tail_length + 1u);
  memcpy(match, to, to_length);
  *length = *length + to_length - from_length;
  return true;
}

static bool mutate_first_character_after(char *bytes, const char *prefix,
                                         char replacement) {
  char *match = strstr(bytes, prefix);
  char *value;
  if (match == NULL) return false;
  value = match + strlen(prefix);
  if (*value == '\0' || *value == '"') return false;
  *value = replacement;
  return true;
}

static void test_complete_batch_round_trip(void) {
  t64_cpu_diag_inventory *inventory = parse_authored_inventory();
  t64_cpu_diag_aggregate *aggregate = NULL;
  char bytes[T64_CPU_DIAG_MAX_BATCH_BYTES + 1u];
  size_t length = 0u;
  t64_cpu_diag_group_status group_status = T64_CPU_DIAG_GROUP_INCOMPLETE;
  size_t expected = 0u;
  size_t reached = 0u;
  size_t passed = 0u;
  size_t failed = 0u;
  if (inventory == NULL) return;
  if (t64_cpu_diag_inventory_registration_count(inventory) != 1u ||
      t64_cpu_diag_inventory_required_value_count(inventory) != 1u ||
      strcmp(t64_cpu_diag_inventory_sha256(inventory),
             "0b5b78464f4cb5743af96e6a04fce411be4abe689c3ef3b88e74d575b15ce82b") !=
          0) {
    (void)fprintf(stderr, "authored inventory returned unexpected count or digest\n");
    failures++;
  }
  expect_status("aggregate create",
                t64_cpu_diag_aggregate_create(inventory, &aggregate),
                T64_CPU_DIAG_OK);
  if (aggregate == NULL || !make_batch(inventory, T64_CPU_DIAG_OUTCOME_PASS,
                                       bytes, sizeof bytes, &length)) {
    goto cleanup;
  }
  if (length != sizeof expected_pass_batch - 1u ||
      memcmp(bytes, expected_pass_batch, length) != 0 ||
      length > T64_CPU_DIAG_MAX_BATCH_BYTES) {
    (void)fprintf(stderr,
                  "canonical batch differed: expected %zu bytes, observed %zu\n",
                  sizeof expected_pass_batch - 1u, length);
    failures++;
  }
  if (t64_cpu_diag_aggregate_is_complete(aggregate)) {
    (void)fprintf(stderr, "empty aggregate unexpectedly complete\n");
    failures++;
  }
  expect_status("add complete batch",
                t64_cpu_diag_aggregate_add_batch(
                    aggregate, (const uint8_t *)bytes, length),
                T64_CPU_DIAG_OK);
  expect_status("arithmetic group status",
                t64_cpu_diag_aggregate_group(aggregate, "arithmetic",
                                             &group_status, &expected,
                                             &reached, &passed, &failed),
                T64_CPU_DIAG_OK);
  if (group_status != T64_CPU_DIAG_GROUP_PASS || expected != 1u ||
      reached != 1u || passed != 1u || failed != 0u ||
      t64_cpu_diag_aggregate_is_complete(aggregate)) {
    (void)fprintf(stderr,
                  "single arithmetic group expected pass(1/1) and whole-inventory incomplete, observed state=%u expected=%zu reached=%zu pass=%zu fail=%zu whole_complete=%u\n",
                  (unsigned)group_status, expected, reached, passed, failed,
                  t64_cpu_diag_aggregate_is_complete(aggregate) ? 1u : 0u);
    failures++;
  }
  expect_status("duplicate complete batch",
                t64_cpu_diag_aggregate_add_batch(
                    aggregate, (const uint8_t *)bytes, length),
                T64_CPU_DIAG_DUPLICATE_MEMBER);

cleanup:
  t64_cpu_diag_aggregate_destroy(aggregate);
  t64_cpu_diag_inventory_destroy(inventory);
}

static void test_incomplete_and_wrong_identity_batches(void) {
  t64_cpu_diag_inventory *inventory = parse_authored_inventory();
  t64_cpu_diag_aggregate *aggregate = NULL;
  char bytes[T64_CPU_DIAG_MAX_BATCH_BYTES + 1u];
  char mutated[T64_CPU_DIAG_MAX_BATCH_BYTES + 1u];
  size_t length = 0u;
  size_t mutated_length = 0u;
  char *hash;
  if (inventory == NULL) return;
  expect_status("aggregate create for incomplete controls",
                t64_cpu_diag_aggregate_create(inventory, &aggregate),
                T64_CPU_DIAG_OK);
  if (aggregate == NULL ||
      !make_batch(inventory, T64_CPU_DIAG_OUTCOME_PASS, bytes, sizeof bytes,
                  &length)) {
    goto cleanup;
  }
  if (!remove_done_record(bytes, length, mutated, &mutated_length)) {
    (void)fprintf(stderr, "could not construct authored missing-done control\n");
    failures++;
  } else {
    expect_status("missing done terminator",
                  t64_cpu_diag_aggregate_add_batch(
                      aggregate, (const uint8_t *)mutated, mutated_length),
                  T64_CPU_DIAG_INCOMPLETE);
    if (t64_cpu_diag_aggregate_is_complete(aggregate)) {
      (void)fprintf(stderr, "missing done control completed inventory\n");
      failures++;
    }
  }
  (void)memcpy(mutated, bytes, length);
  mutated[length] = '\0';
  hash = strstr(mutated, "inventory_sha256\":\"");
  if (hash == NULL || hash[sizeof "inventory_sha256\":\"" - 1u] == '\0') {
    (void)fprintf(stderr, "could not construct authored identity-mismatch control\n");
    failures++;
  } else {
    char *digest = hash + sizeof "inventory_sha256\":\"" - 1u;
    digest[0] = digest[0] == '0' ? '1' : '0';
    expect_status("inventory identity mismatch",
                  t64_cpu_diag_aggregate_add_batch(
                      aggregate, (const uint8_t *)mutated, length),
                  T64_CPU_DIAG_IDENTITY_MISMATCH);
  }

cleanup:
  t64_cpu_diag_aggregate_destroy(aggregate);
  t64_cpu_diag_inventory_destroy(inventory);
}

static void test_failure_and_incomplete_outcomes(void) {
  const t64_cpu_diag_outcome outcomes[] = {
      T64_CPU_DIAG_OUTCOME_FAIL, T64_CPU_DIAG_OUTCOME_INCOMPLETE,
      T64_CPU_DIAG_OUTCOME_NOT_RUN, T64_CPU_DIAG_OUTCOME_GUEST_STOP,
      T64_CPU_DIAG_OUTCOME_HARNESS_ERROR};
  size_t i;
  for (i = 0u; i < sizeof outcomes / sizeof outcomes[0]; i++) {
    t64_cpu_diag_inventory *inventory = parse_authored_inventory();
    t64_cpu_diag_aggregate *aggregate = NULL;
    char bytes[T64_CPU_DIAG_MAX_BATCH_BYTES + 1u];
    size_t length = 0u;
    t64_cpu_diag_group_status group_status = T64_CPU_DIAG_GROUP_INCOMPLETE;
    size_t expected = 0u;
    size_t reached = 0u;
    size_t failed = 0u;
    if (inventory == NULL) continue;
    expect_status("aggregate create for outcome control",
                  t64_cpu_diag_aggregate_create(inventory, &aggregate),
                  T64_CPU_DIAG_OK);
    if (aggregate != NULL &&
        make_batch(inventory, outcomes[i], bytes, sizeof bytes, &length)) {
      expect_status("add outcome control",
                    t64_cpu_diag_aggregate_add_batch(
                        aggregate, (const uint8_t *)bytes, length),
                    T64_CPU_DIAG_OK);
      expect_status("outcome group status",
                    t64_cpu_diag_aggregate_group(aggregate, "arithmetic",
                                                 &group_status, &expected,
                                                 &reached, NULL, &failed),
                    T64_CPU_DIAG_OK);
      if (outcomes[i] == T64_CPU_DIAG_OUTCOME_FAIL) {
        if (group_status != T64_CPU_DIAG_GROUP_FAIL || expected != 1u ||
            reached != 1u || failed != 1u ||
            t64_cpu_diag_aggregate_is_complete(aggregate)) {
          (void)fprintf(stderr,
                        "fail outcome was not a failed group with whole inventory incomplete\n");
          failures++;
        }
      } else if (group_status != T64_CPU_DIAG_GROUP_INCOMPLETE ||
                 reached != 0u || t64_cpu_diag_aggregate_is_complete(aggregate)) {
        (void)fprintf(stderr, "%s outcome was not incomplete\n",
                      t64_cpu_diag_outcome_name(outcomes[i]));
        failures++;
      }
    }
    t64_cpu_diag_aggregate_destroy(aggregate);
    t64_cpu_diag_inventory_destroy(inventory);
  }
}

static void test_empty_inventory_refused(void) {
  uint8_t bytes[T64_CPU_DIAG_MAX_INVENTORY_BYTES];
  size_t length = 0u;
  t64_cpu_diag_inventory *inventory = NULL;
  if (!empty_inventory_bytes(bytes, sizeof bytes, &length)) {
    (void)fprintf(stderr, "could not construct authored empty-inventory control\n");
    failures++;
    return;
  }
  expect_status("empty required inventory",
                t64_cpu_diag_inventory_parse(bytes, length, &inventory),
                T64_CPU_DIAG_INVALID_INVENTORY);
  if (inventory != NULL) {
    (void)fprintf(stderr, "empty inventory unexpectedly returned a descriptor\n");
    failures++;
  }
  t64_cpu_diag_inventory_destroy(inventory);
}

static void test_single_group_does_not_complete_required_set(void) {
  t64_cpu_diag_inventory *inventory = parse_authored_inventory();
  t64_cpu_diag_aggregate *aggregate = NULL;
  char bytes[T64_CPU_DIAG_MAX_BATCH_BYTES + 1u];
  size_t length = 0u;
  t64_cpu_diag_group_status group_status = T64_CPU_DIAG_GROUP_INCOMPLETE;
  size_t expected = 0u;
  size_t reached = 0u;
  if (inventory == NULL) return;
  expect_status("single-group aggregate create",
                t64_cpu_diag_aggregate_create(inventory, &aggregate),
                T64_CPU_DIAG_OK);
  if (aggregate == NULL ||
      !make_batch(inventory, T64_CPU_DIAG_OUTCOME_PASS, bytes, sizeof bytes,
                  &length)) {
    goto cleanup;
  }
  expect_status("single exact group batch",
                t64_cpu_diag_aggregate_add_batch(
                    aggregate, (const uint8_t *)bytes, length),
                T64_CPU_DIAG_OK);
  expect_status("single exact group status",
                t64_cpu_diag_aggregate_group(aggregate, "arithmetic",
                                             &group_status, &expected, &reached,
                                             NULL, NULL),
                T64_CPU_DIAG_OK);
  if (group_status != T64_CPU_DIAG_GROUP_PASS || expected != 1u ||
      reached != 1u) {
    (void)fprintf(stderr,
                  "single exact group expected pass(1/1), observed state=%u expected=%zu reached=%zu\n",
                  (unsigned)group_status, expected, reached);
    failures++;
  }
  if (t64_cpu_diag_aggregate_is_complete(aggregate)) {
    (void)fprintf(stderr,
                  "whole inventory expected incomplete with 13 required groups absent, observed complete=1\n");
    failures++;
  }

cleanup:
  t64_cpu_diag_aggregate_destroy(aggregate);
  t64_cpu_diag_inventory_destroy(inventory);
}

static void test_full_set_order_and_duplicate_seam(void) {
  char *bytes = (char *)malloc(T64_CPU_DIAG_MAX_INVENTORY_BYTES);
  size_t length = 0u;
  size_t i;
  char duplicate_batch[T64_CPU_DIAG_MAX_BATCH_BYTES + 1u];
  size_t duplicate_length = 0u;
  t64_cpu_diag_inventory *inventory = NULL;
  t64_cpu_diag_aggregate *aggregate = NULL;
  t64_cpu_diag_member_binding first_binding;
  t64_cpu_diag_member_binding second_binding;
  if (bytes == NULL) {
    (void)fprintf(stderr, "authored full inventory allocation failed\n");
    failures++;
    return;
  }
  if (!make_inventory(bytes, T64_CPU_DIAG_MAX_INVENTORY_BYTES, &length,
                      sizeof expected_groups / sizeof expected_groups[0], true,
                      true)) {
    (void)fprintf(stderr, "could not construct authored full inventory\n");
    failures++;
    goto cleanup;
  }
  expect_status("full authored inventory parse",
                t64_cpu_diag_inventory_parse((const uint8_t *)bytes, length,
                                             &inventory),
                T64_CPU_DIAG_OK);
  if (inventory == NULL) goto cleanup;
  if (t64_cpu_diag_required_group_count() !=
      sizeof expected_groups / sizeof expected_groups[0]) {
    (void)fprintf(stderr, "required group map size was not exactly fourteen\n");
    failures++;
  }
  for (i = 0u; i < sizeof expected_groups / sizeof expected_groups[0]; i++) {
    const char *actual = t64_cpu_diag_required_group_name(i);
    if (actual == NULL || strcmp(actual, expected_groups[i]) != 0) {
      (void)fprintf(stderr, "required group order mismatch at index %zu\n", i);
      failures++;
    }
  }
  if (t64_cpu_diag_inventory_registration_count(inventory) != 20u ||
      t64_cpu_diag_inventory_required_value_count(inventory) != 15u) {
    (void)fprintf(stderr,
                  "full authored counts expected registrations=20 required-values=15, observed registrations=%zu required-values=%zu\n",
                  t64_cpu_diag_inventory_registration_count(inventory),
                  t64_cpu_diag_inventory_required_value_count(inventory));
    failures++;
  }
  if (!t64_cpu_diag_inventory_member_binding(inventory, 1u, 0u,
                                             &first_binding) ||
      !t64_cpu_diag_inventory_member_binding(inventory, 1u, 1u,
                                             &second_binding) ||
      strcmp(first_binding.selector, "sssssssssssssssss") != 0 ||
      strcmp(first_binding.selected_rom_sha256,
             "4444444444444444444444444444444444444444444444444444444444444444") !=
          0 ||
      strcmp(second_binding.selector, "second-selector") != 0 ||
      strcmp(second_binding.selected_rom_sha256,
             "7777777777777777777777777777777777777777777777777777777777777777") !=
          0) {
    (void)fprintf(stderr,
                  "distinct per-value selector/selected-ROM bindings were not retained\n");
    failures++;
  }
  expect_status("full aggregate create",
                t64_cpu_diag_aggregate_create(inventory, &aggregate),
                T64_CPU_DIAG_OK);
  if (aggregate == NULL ||
      !add_all_required_in_reverse(
          inventory, aggregate,
          sizeof expected_groups / sizeof expected_groups[0], SIZE_MAX,
          SIZE_MAX)) {
    goto cleanup;
  }
  if (!t64_cpu_diag_aggregate_is_complete(aggregate) ||
      t64_cpu_diag_aggregate_member_count(aggregate) != 15u) {
    (void)fprintf(stderr,
                  "full fourteen-group set expected complete with fifteen values\n");
    failures++;
  }
  for (i = 0u; i < 15u; i++) {
    t64_cpu_diag_member_result member;
    size_t expected_registration = i == 0u ? 0u : (i <= 2u ? 1u : i - 1u);
    size_t expected_value = i == 2u ? 1u : 0u;
    char expected_name[64];
    char expected_value_id[64];
    (void)snprintf(expected_name, sizeof expected_name,
                   "authored.required.%02zu", expected_registration);
    (void)snprintf(expected_value_id, sizeof expected_value_id,
                   "authored.value.%02zu.%zu", expected_registration,
                   expected_value);
    if (!t64_cpu_diag_aggregate_member_at(aggregate, i, &member) ||
        member.registration_index != expected_registration ||
        member.value_index != expected_value || !member.has_outcome ||
        member.outcome != T64_CPU_DIAG_OUTCOME_PASS ||
        strcmp(member.group, expected_groups[expected_registration]) != 0 ||
        strcmp(member.registration_name, expected_name) != 0 ||
        strcmp(member.value_id, expected_value_id) != 0) {
      (void)fprintf(stderr,
                    "aggregate member order expected registration=%zu value=%zu, observed index=%zu\n",
                    expected_registration, expected_value, i);
      failures++;
    }
  }
  for (i = 0u; i < sizeof expected_groups / sizeof expected_groups[0]; i++) {
    t64_cpu_diag_group_status group_status = T64_CPU_DIAG_GROUP_INCOMPLETE;
    size_t expected = 0u;
    size_t reached = 0u;
    size_t passed = 0u;
    size_t failed = 0u;
    size_t expected_count = i == 1u ? 2u : 1u;
    expect_status("complete group lookup",
                  t64_cpu_diag_aggregate_group(aggregate, expected_groups[i],
                                               &group_status, &expected,
                                               &reached, &passed, &failed),
                  T64_CPU_DIAG_OK);
    if (group_status != T64_CPU_DIAG_GROUP_PASS ||
        expected != expected_count || reached != expected_count ||
        passed != expected_count || failed != 0u) {
      (void)fprintf(stderr, "complete group status mismatch for %s\n",
                    expected_groups[i]);
      failures++;
    }
  }
  expect_status("duplicate seam batch encode",
                t64_cpu_diag_batch_encode(inventory, 0u, 0u,
                                         T64_CPU_DIAG_OUTCOME_PASS,
                                         duplicate_batch,
                                         sizeof duplicate_batch,
                                         &duplicate_length),
                T64_CPU_DIAG_OK);
  expect_status("duplicate first/last batch seam",
                t64_cpu_diag_aggregate_add_batch(
                    aggregate, (const uint8_t *)duplicate_batch,
                    duplicate_length),
                T64_CPU_DIAG_DUPLICATE_MEMBER);
  if (t64_cpu_diag_aggregate_is_complete(aggregate)) {
    (void)fprintf(stderr,
                  "duplicate batch seam did not invalidate complete inventory\n");
    failures++;
  }

cleanup:
  t64_cpu_diag_aggregate_destroy(aggregate);
  t64_cpu_diag_inventory_destroy(inventory);
  free(bytes);
}

static void test_omitted_value_and_missing_group(void) {
  char *bytes = (char *)malloc(T64_CPU_DIAG_MAX_INVENTORY_BYTES);
  size_t length = 0u;
  t64_cpu_diag_inventory *inventory = NULL;
  t64_cpu_diag_aggregate *aggregate = NULL;
  t64_cpu_diag_group_status group_status = T64_CPU_DIAG_GROUP_PASS;
  size_t expected = 0u;
  size_t reached = 0u;
  if (bytes == NULL) {
    (void)fprintf(stderr, "authored reduced inventory allocation failed\n");
    failures++;
    return;
  }
  if (!make_inventory(bytes, T64_CPU_DIAG_MAX_INVENTORY_BYTES, &length, 14u,
                      false, true)) {
    failures++;
    goto cleanup;
  }
  expect_status("full inventory for omitted value",
                t64_cpu_diag_inventory_parse((const uint8_t *)bytes, length,
                                             &inventory),
                T64_CPU_DIAG_OK);
  if (inventory == NULL) goto cleanup;
  expect_status("omitted aggregate create",
                t64_cpu_diag_aggregate_create(inventory, &aggregate),
                T64_CPU_DIAG_OK);
  if (aggregate == NULL ||
      !add_all_required_in_reverse(inventory, aggregate, 14u, 1u, 1u)) {
    goto cleanup;
  }
  expect_status("omitted arithmetic group lookup",
                t64_cpu_diag_aggregate_group(aggregate, "arithmetic",
                                             &group_status, &expected, &reached,
                                             NULL, NULL),
                T64_CPU_DIAG_OK);
  if (group_status != T64_CPU_DIAG_GROUP_INCOMPLETE || expected != 2u ||
      reached != 1u || t64_cpu_diag_aggregate_is_complete(aggregate)) {
    (void)fprintf(stderr,
                  "omitted value expected arithmetic incomplete(1/2), observed state=%u expected=%zu reached=%zu whole_complete=%u\n",
                  (unsigned)group_status, expected, reached,
                  t64_cpu_diag_aggregate_is_complete(aggregate) ? 1u : 0u);
    failures++;
  }
  if (!add_required_batch(inventory, aggregate, 1u, 1u,
                          T64_CPU_DIAG_OUTCOME_PASS) ||
      !t64_cpu_diag_aggregate_is_complete(aggregate)) {
    (void)fprintf(stderr, "complete membership remained incomplete after exact missing value arrived\n");
    failures++;
  }

cleanup:
  t64_cpu_diag_aggregate_destroy(aggregate);
  t64_cpu_diag_inventory_destroy(inventory);
  free(bytes);

  bytes = (char *)malloc(T64_CPU_DIAG_MAX_INVENTORY_BYTES);
  length = 0u;
  inventory = NULL;
  aggregate = NULL;
  if (bytes == NULL ||
      !make_inventory(bytes, T64_CPU_DIAG_MAX_INVENTORY_BYTES, &length, 13u,
                      true, true)) {
    (void)fprintf(stderr, "could not construct reduced required-group inventory\n");
    failures++;
    free(bytes);
    return;
  }
  expect_status("reduced thirteen-group inventory parse",
                t64_cpu_diag_inventory_parse((const uint8_t *)bytes, length,
                                             &inventory),
                T64_CPU_DIAG_OK);
  if (inventory != NULL) {
    expect_status("reduced aggregate create",
                  t64_cpu_diag_aggregate_create(inventory, &aggregate),
                  T64_CPU_DIAG_OK);
    if (aggregate != NULL) {
      (void)add_all_required_in_reverse(inventory, aggregate, 13u, SIZE_MAX,
                                        SIZE_MAX);
      group_status = T64_CPU_DIAG_GROUP_PASS;
      expected = 99u;
      reached = 99u;
      expect_status("missing traps group lookup",
                    t64_cpu_diag_aggregate_group(aggregate, "traps",
                                                 &group_status, &expected,
                                                 &reached, NULL, NULL),
                    T64_CPU_DIAG_OK);
      if (group_status != T64_CPU_DIAG_GROUP_INCOMPLETE || expected != 0u ||
          reached != 0u || t64_cpu_diag_aggregate_is_complete(aggregate)) {
        (void)fprintf(stderr,
                      "reduced inventory expected missing traps and incomplete whole set\n");
        failures++;
      }
    }
  }
  t64_cpu_diag_aggregate_destroy(aggregate);
  t64_cpu_diag_inventory_destroy(inventory);
  free(bytes);
}

static void test_inventory_count_name_and_order_controls(void) {
  char *bytes = (char *)malloc(T64_CPU_DIAG_MAX_INVENTORY_BYTES);
  size_t length = 0u;
  t64_cpu_diag_inventory *inventory = NULL;
  char *match;
  char *digits;
  if (bytes == NULL) {
    failures++;
    return;
  }
  if (!make_inventory(bytes, T64_CPU_DIAG_MAX_INVENTORY_BYTES, &length, 14u,
                      true, true)) {
    (void)fprintf(stderr, "could not construct inventory tampering controls\n");
    failures++;
    free(bytes);
    return;
  }
  if (!replace_text(bytes, &length, T64_CPU_DIAG_MAX_INVENTORY_BYTES,
                    "\"required_values\":\"15\"",
                    "\"required_values\":\"14\"")) {
    (void)fprintf(stderr, "could not construct declared-denominator mismatch\n");
    failures++;
  } else {
    expect_status("declared denominator mismatch",
                  t64_cpu_diag_inventory_parse((const uint8_t *)bytes, length,
                                               &inventory),
                  T64_CPU_DIAG_INVALID_INVENTORY);
  }
  if (inventory != NULL) {
    t64_cpu_diag_inventory_destroy(inventory);
    inventory = NULL;
  }
  if (!make_inventory(bytes, T64_CPU_DIAG_MAX_INVENTORY_BYTES, &length, 14u,
                      true, true)) {
    failures++;
    goto cleanup;
  }
  match = strstr(bytes, "\"name\":\"authored.required.01\"");
  if (match == NULL) {
    (void)fprintf(stderr, "could not construct duplicate-name inventory control\n");
    failures++;
  } else {
    digits = match + strlen("\"name\":\"authored.required.");
    digits[0] = '0';
    digits[1] = '0';
    expect_status("duplicate registration name",
                  t64_cpu_diag_inventory_parse((const uint8_t *)bytes, length,
                                               &inventory),
                  T64_CPU_DIAG_INVALID_INVENTORY);
  }
  if (inventory != NULL) {
    t64_cpu_diag_inventory_destroy(inventory);
    inventory = NULL;
  }
  if (!make_inventory(bytes, T64_CPU_DIAG_MAX_INVENTORY_BYTES, &length, 14u,
                      true, true)) {
    failures++;
    goto cleanup;
  }
  match = strstr(bytes, "\"index\":1,\"lane\":\"required-base\"");
  if (match == NULL) {
    (void)fprintf(stderr, "could not construct reordered registration control\n");
    failures++;
  } else {
    digits = match + strlen("\"index\":");
    *digits = '0';
    expect_status("reordered source registration index",
                  t64_cpu_diag_inventory_parse((const uint8_t *)bytes, length,
                                               &inventory),
                  T64_CPU_DIAG_INVALID_INVENTORY);
  }

cleanup:
  t64_cpu_diag_inventory_destroy(inventory);
  free(bytes);
}

static void test_wrong_batch_identity_unknown_member_and_aggregate_only(void) {
  static const char aggregate_only[] =
      "{\"groups\":[{\"group\":\"arithmetic\",\"status\":\"pass\"}],\"schema\":\"t64.cpu-groups\",\"schema_version\":1}\n";
  char *inventory_bytes = (char *)malloc(T64_CPU_DIAG_MAX_INVENTORY_BYTES);
  char batch[T64_CPU_DIAG_MAX_BATCH_BYTES + 1u];
  char original_second_hash[] =
      "\"selected_rom_sha256\":\"7777777777777777777777777777777777777777777777777777777777777777\"";
  char swapped_second_hash[] =
      "\"selected_rom_sha256\":\"4444444444444444444444444444444444444444444444444444444444444444\"";
  char unregistered_hash[] =
      "\"selected_rom_sha256\":\"8888888888888888888888888888888888888888888888888888888888888888\"";
  size_t inventory_length = 0u;
  size_t batch_length = 0u;
  t64_cpu_diag_inventory *inventory = NULL;
  t64_cpu_diag_aggregate *aggregate = NULL;
  if (inventory_bytes == NULL) {
    failures++;
    return;
  }
  if (!make_inventory(inventory_bytes, T64_CPU_DIAG_MAX_INVENTORY_BYTES,
                      &inventory_length, 14u, true, true)) {
    failures++;
    free(inventory_bytes);
    return;
  }
  expect_status("identity-control inventory parse",
                t64_cpu_diag_inventory_parse((const uint8_t *)inventory_bytes,
                                             inventory_length, &inventory),
                T64_CPU_DIAG_OK);
  if (inventory == NULL) {
    free(inventory_bytes);
    return;
  }

  expect_status("registered alternate member batch encode",
                t64_cpu_diag_batch_encode(inventory, 1u, 1u,
                                          T64_CPU_DIAG_OUTCOME_PASS, batch,
                                          sizeof batch, &batch_length),
                T64_CPU_DIAG_OK);
  expect_status("swapped binding aggregate create",
                t64_cpu_diag_aggregate_create(inventory, &aggregate),
                T64_CPU_DIAG_OK);
  if (aggregate != NULL) {
    if (!replace_text(batch, &batch_length, sizeof batch,
                      original_second_hash, swapped_second_hash) ||
        !replace_text(batch, &batch_length, sizeof batch,
                      "\"selector\":\"second-selector\"",
                      "\"selector\":\"sssssssssssssssss\"")) {
      (void)fprintf(stderr, "could not construct swapped member binding control\n");
      failures++;
    } else {
      expect_status("swapped registered member binding",
                    t64_cpu_diag_aggregate_add_batch(
                        aggregate, (const uint8_t *)batch, batch_length),
                    T64_CPU_DIAG_IDENTITY_MISMATCH);
    }
  }
  t64_cpu_diag_aggregate_destroy(aggregate);
  aggregate = NULL;

  expect_status("unregistered binding batch encode",
                t64_cpu_diag_batch_encode(inventory, 1u, 1u,
                                          T64_CPU_DIAG_OUTCOME_PASS, batch,
                                          sizeof batch, &batch_length),
                T64_CPU_DIAG_OK);
  expect_status("unregistered binding aggregate create",
                t64_cpu_diag_aggregate_create(inventory, &aggregate),
                T64_CPU_DIAG_OK);
  if (aggregate != NULL) {
    if (!replace_text(batch, &batch_length, sizeof batch,
                      original_second_hash, unregistered_hash) ||
        !replace_text(batch, &batch_length, sizeof batch,
                      "\"selector\":\"second-selector\"",
                      "\"selector\":\"unregistered-selector\"")) {
      (void)fprintf(stderr, "could not construct unregistered binding control\n");
      failures++;
    } else {
      expect_status("unregistered member binding",
                    t64_cpu_diag_aggregate_add_batch(
                        aggregate, (const uint8_t *)batch, batch_length),
                    T64_CPU_DIAG_IDENTITY_MISMATCH);
    }
  }
  t64_cpu_diag_aggregate_destroy(aggregate);
  aggregate = NULL;

  expect_status("ROM identity aggregate create",
                t64_cpu_diag_aggregate_create(inventory, &aggregate),
                T64_CPU_DIAG_OK);
  if (aggregate != NULL &&
      make_batch(inventory, T64_CPU_DIAG_OUTCOME_PASS, batch, sizeof batch,
                 &batch_length)) {
    if (!mutate_first_character_after(batch, "\"base_rom_sha256\":\"", '1')) {
      (void)fprintf(stderr, "could not construct ROM-identity mismatch control\n");
      failures++;
    } else {
      expect_status("batch base ROM mismatch",
                    t64_cpu_diag_aggregate_add_batch(
                        aggregate, (const uint8_t *)batch, batch_length),
                    T64_CPU_DIAG_IDENTITY_MISMATCH);
      if (t64_cpu_diag_aggregate_is_complete(aggregate)) {
        (void)fprintf(stderr, "ROM-identity mismatch completed inventory\n");
        failures++;
      }
    }
  }
  t64_cpu_diag_aggregate_destroy(aggregate);
  aggregate = NULL;

  expect_status("selector identity aggregate create",
                t64_cpu_diag_aggregate_create(inventory, &aggregate),
                T64_CPU_DIAG_OK);
  if (aggregate != NULL &&
      make_batch(inventory, T64_CPU_DIAG_OUTCOME_PASS, batch, sizeof batch,
                 &batch_length)) {
    if (!mutate_first_character_after(batch, "\"selector\":\"", 'x')) {
      (void)fprintf(stderr, "could not construct selector mismatch control\n");
      failures++;
    } else {
      expect_status("batch selector mismatch",
                    t64_cpu_diag_aggregate_add_batch(
                        aggregate, (const uint8_t *)batch, batch_length),
                    T64_CPU_DIAG_IDENTITY_MISMATCH);
    }
  }
  t64_cpu_diag_aggregate_destroy(aggregate);
  aggregate = NULL;

  expect_status("unknown-member aggregate create",
                t64_cpu_diag_aggregate_create(inventory, &aggregate),
                T64_CPU_DIAG_OK);
  if (aggregate != NULL) {
    expect_status("unknown-member batch encode",
                  t64_cpu_diag_batch_encode(inventory, 13u, 0u,
                                            T64_CPU_DIAG_OUTCOME_PASS, batch,
                                            sizeof batch, &batch_length),
                  T64_CPU_DIAG_OK);
    if (!replace_text(batch, &batch_length, sizeof batch,
                      "\"registration_index\":13",
                      "\"registration_index\":65535")) {
      (void)fprintf(stderr, "could not construct unknown-member batch control\n");
      failures++;
    } else {
      expect_status("unknown member index",
                    t64_cpu_diag_aggregate_add_batch(
                        aggregate, (const uint8_t *)batch, batch_length),
                    T64_CPU_DIAG_INVALID_BATCH);
    }
  }
  t64_cpu_diag_aggregate_destroy(aggregate);
  aggregate = NULL;

  expect_status("truncated aggregate create",
                t64_cpu_diag_aggregate_create(inventory, &aggregate),
                T64_CPU_DIAG_OK);
  if (aggregate != NULL &&
      make_batch(inventory, T64_CPU_DIAG_OUTCOME_PASS, batch, sizeof batch,
                 &batch_length)) {
    expect_status("truncated batch transcript",
                  t64_cpu_diag_aggregate_add_batch(
                      aggregate, (const uint8_t *)batch, batch_length - 1u),
                  T64_CPU_DIAG_INVALID_SCHEMA);
  }
  t64_cpu_diag_aggregate_destroy(aggregate);
  aggregate = NULL;

  expect_status("aggregate-only summary create",
                t64_cpu_diag_aggregate_create(inventory, &aggregate),
                T64_CPU_DIAG_OK);
  if (aggregate != NULL) {
    expect_status("aggregate-only summary rejected",
                  t64_cpu_diag_aggregate_add_batch(
                      aggregate, (const uint8_t *)aggregate_only,
                      sizeof aggregate_only - 1u),
                  T64_CPU_DIAG_INVALID_SCHEMA);
    if (t64_cpu_diag_aggregate_is_complete(aggregate)) {
      (void)fprintf(stderr, "aggregate-only summary completed the suite\n");
      failures++;
    }
  }

  t64_cpu_diag_aggregate_destroy(aggregate);
  t64_cpu_diag_inventory_destroy(inventory);
  free(inventory_bytes);
}

static void test_maximum_legal_shape_and_size_limits(void) {
  char *inventory_bytes = (char *)malloc(T64_CPU_DIAG_MAX_INVENTORY_BYTES + 1u);
  uint8_t oversized_batch[T64_CPU_DIAG_MAX_BATCH_BYTES + 1u];
  size_t inventory_length = 0u;
  size_t batch_length = 0u;
  const char *const one_group[] = {"arithmetic"};
  t64_cpu_diag_inventory *inventory = NULL;
  t64_cpu_diag_inventory *over_inventory = NULL;
  t64_cpu_diag_aggregate *aggregate = NULL;
  t64_cpu_diag_group_status group_status = T64_CPU_DIAG_GROUP_INCOMPLETE;
  size_t expected = 0u;
  size_t reached = 0u;
  char batch[T64_CPU_DIAG_MAX_BATCH_BYTES + 1u];
  if (inventory_bytes == NULL) {
    failures++;
    return;
  }
  if (!build_authored_inventory(inventory_bytes,
                                T64_CPU_DIAG_MAX_INVENTORY_BYTES,
                                &inventory_length, one_group, 1u, false, false,
                                T64_CPU_DIAG_MAX_SELECTOR_BYTES,
                                T64_CPU_DIAG_MAX_NAME_BYTES,
                                T64_CPU_DIAG_MAX_VALUE_ID_BYTES)) {
    (void)fprintf(stderr, "could not construct maximum legal inventory fields\n");
    failures++;
    goto cleanup;
  }
  expect_status("maximum legal field inventory parse",
                t64_cpu_diag_inventory_parse((const uint8_t *)inventory_bytes,
                                             inventory_length, &inventory),
                T64_CPU_DIAG_OK);
  if (inventory == NULL) goto cleanup;
  expect_status("maximum shape aggregate create",
                t64_cpu_diag_aggregate_create(inventory, &aggregate),
                T64_CPU_DIAG_OK);
  if (aggregate != NULL) {
    expect_status("maximum shape batch encode",
                  t64_cpu_diag_batch_encode(inventory, 0u, 0u,
                                            T64_CPU_DIAG_OUTCOME_PASS, batch,
                                            sizeof batch, &batch_length),
                  T64_CPU_DIAG_OK);
    if (batch_length > T64_CPU_DIAG_MAX_BATCH_BYTES) {
      (void)fprintf(stderr, "maximum legal batch exceeded the 4096-byte capture\n");
      failures++;
    }
    expect_status("maximum shape batch add",
                  t64_cpu_diag_aggregate_add_batch(
                      aggregate, (const uint8_t *)batch, batch_length),
                  T64_CPU_DIAG_OK);
    expect_status("maximum shape group status",
                  t64_cpu_diag_aggregate_group(aggregate, "arithmetic",
                                               &group_status, &expected,
                                               &reached, NULL, NULL),
                  T64_CPU_DIAG_OK);
    if (group_status != T64_CPU_DIAG_GROUP_PASS || expected != 1u ||
        reached != 1u) {
      (void)fprintf(stderr, "maximum legal batch did not preserve its member\n");
      failures++;
    }
  }
  t64_cpu_diag_aggregate_destroy(aggregate);
  aggregate = NULL;

  (void)memset(oversized_batch, ' ', sizeof oversized_batch);
  expect_status("oversized aggregate create",
                t64_cpu_diag_aggregate_create(inventory, &aggregate),
                T64_CPU_DIAG_OK);
  if (aggregate != NULL) {
    expect_status("batch one byte over capture bound",
                  t64_cpu_diag_aggregate_add_batch(
                      aggregate, oversized_batch, sizeof oversized_batch),
                  T64_CPU_DIAG_SIZE_LIMIT);
    if (t64_cpu_diag_aggregate_is_complete(aggregate)) {
      (void)fprintf(stderr, "oversized transcript completed inventory\n");
      failures++;
    }
  }
  t64_cpu_diag_aggregate_destroy(aggregate);
  aggregate = NULL;

  if (inventory_length < T64_CPU_DIAG_MAX_INVENTORY_BYTES + 1u) {
    (void)memset(inventory_bytes + inventory_length, ' ',
                 T64_CPU_DIAG_MAX_INVENTORY_BYTES + 1u - inventory_length);
    expect_status("inventory one byte above metadata limit",
                  t64_cpu_diag_inventory_parse(
                      (const uint8_t *)inventory_bytes,
                      T64_CPU_DIAG_MAX_INVENTORY_BYTES + 1u,
                      &over_inventory),
                  T64_CPU_DIAG_SIZE_LIMIT);
  }

cleanup:
  t64_cpu_diag_aggregate_destroy(aggregate);
  t64_cpu_diag_inventory_destroy(inventory);
  t64_cpu_diag_inventory_destroy(over_inventory);
  free(inventory_bytes);
}

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "--inventory") == 0) {
    test_shared_partition_primitive();
    test_inventory_v3_base_levels_and_stale_v2();
    test_single_group_does_not_complete_required_set();
    test_full_set_order_and_duplicate_seam();
    test_omitted_value_and_missing_group();
    test_inventory_count_name_and_order_controls();
    test_wrong_batch_identity_unknown_member_and_aggregate_only();
    test_maximum_legal_shape_and_size_limits();
    if (failures != 0u) return 1;
    (void)printf("CPU diagnostic inventory controls PASS\n");
    return 0;
  }
  if (argc != 1) return 2;
  test_shared_partition_primitive();
  test_inventory_v3_base_levels_and_stale_v2();
  test_complete_batch_round_trip();
  test_incomplete_and_wrong_identity_batches();
  test_failure_and_incomplete_outcomes();
  test_empty_inventory_refused();
  if (failures != 0u) {
    (void)fprintf(stderr, "CPU diagnostic protocol controls failed=%u\n", failures);
    return 1;
  }
  (void)printf("CPU diagnostic protocol controls PASS\n");
  return 0;
}
