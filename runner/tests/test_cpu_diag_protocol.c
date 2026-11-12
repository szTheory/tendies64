#include "t64_cpu_diag_protocol.h"

#include <stdio.h>
#include <string.h>

static const uint8_t inventory_json[] =
    "{\"identity\":{\"base_rom_sha256\":\"0000000000000000000000000000000000000000000000000000000000000000\",\"core_build_sha256\":\"1111111111111111111111111111111111111111111111111111111111111111\",\"overlay_sha256\":\"2222222222222222222222222222222222222222222222222222222222222222\",\"profile_sha256\":\"3333333333333333333333333333333333333333333333333333333333333333\",\"selected_rom_sha256\":\"4444444444444444444444444444444444444444444444444444444444444444\",\"selector\":\"authored-selector\",\"source_revision\":\"5555555555555555555555555555555555555555\",\"tool_receipt_sha256\":\"6666666666666666666666666666666666666666666666666666666666666666\"},\"registrations\":[{\"execution\":\"values\",\"group\":\"arithmetic\",\"index\":0,\"lane\":\"required-base\",\"level\":\"BasicFunctionality\",\"name\":\"authored.case\",\"reason\":\"required\",\"values\":[{\"id\":\"case.one\",\"index\":0}]}],\"schema\":\"t64.cpu-inventory\",\"schema_version\":1}\n";

static const char expected_pass_batch[] =
    "{\"identity\":{\"base_rom_sha256\":\"0000000000000000000000000000000000000000000000000000000000000000\",\"core_build_sha256\":\"1111111111111111111111111111111111111111111111111111111111111111\",\"inventory_sha256\":\"54e3c1520e2e534644cae7f96002099d03cf53dd6419ec5a4838e9e50bf286a5\",\"overlay_sha256\":\"2222222222222222222222222222222222222222222222222222222222222222\",\"profile_sha256\":\"3333333333333333333333333333333333333333333333333333333333333333\",\"selected_rom_sha256\":\"4444444444444444444444444444444444444444444444444444444444444444\",\"selector\":\"authored-selector\",\"source_revision\":\"5555555555555555555555555555555555555555\",\"tool_receipt_sha256\":\"6666666666666666666666666666666666666666666666666666666666666666\"},\"records\":[{\"event\":\"start\",\"registration_index\":0,\"value_index\":0},{\"event\":\"outcome\",\"status\":\"pass\"},{\"complete\":true,\"event\":\"done\",\"record_count\":\"3\"}],\"schema\":\"t64.cpu-batch\",\"schema_version\":1}\n";

static unsigned failures;

static void expect_status(const char *case_name, t64_cpu_diag_status actual,
                          t64_cpu_diag_status expected) {
  if (actual != expected) {
    (void)fprintf(stderr, "%s expected %s, observed %s\n", case_name,
                  t64_cpu_diag_status_name(expected),
                  t64_cpu_diag_status_name(actual));
    failures++;
  }
}

static t64_cpu_diag_inventory *parse_authored_inventory(void) {
  t64_cpu_diag_inventory *inventory = NULL;
  t64_cpu_diag_status status = t64_cpu_diag_inventory_parse(
      inventory_json, sizeof inventory_json - 1u, &inventory);
  expect_status("authored inventory parse", status, T64_CPU_DIAG_OK);
  return inventory;
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
             "54e3c1520e2e534644cae7f96002099d03cf53dd6419ec5a4838e9e50bf286a5") !=
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
      !t64_cpu_diag_aggregate_is_complete(aggregate)) {
    (void)fprintf(stderr,
                  "complete arithmetic batch expected pass(1/1), observed state=%u expected=%zu reached=%zu pass=%zu fail=%zu complete=%u\n",
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
            !t64_cpu_diag_aggregate_is_complete(aggregate)) {
          (void)fprintf(stderr, "fail outcome was not a complete failed group\n");
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

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "--inventory") == 0) {
    test_single_group_does_not_complete_required_set();
    if (failures != 0u) return 1;
    (void)printf("CPU diagnostic inventory controls PASS\n");
    return 0;
  }
  if (argc != 1) return 2;
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
