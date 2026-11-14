#include "t64_rsp_diag_protocol.h"

#include <stdio.h>
#include <string.h>

static const char inventory_component_rsp[] =
    "{\"component\":\"rsp\",\"declared_counts\":{\"registrations\":1,\"required_values\":1,\"values\":1},"
    "\"identity\":{\"boot_sha256\":\"0000000000000000000000000000000000000000000000000000000000000000\","
    "\"closure_sha256\":\"1111111111111111111111111111111111111111111111111111111111111111\","
    "\"core_build_sha256\":\"2222222222222222222222222222222222222222222222222222222222222222\","
    "\"core_source_revision\":\"1212121212121212121212121212121212121212\","
    "\"overlay_sha256\":\"3333333333333333333333333333333333333333333333333333333333333333\","
    "\"profile_descriptor_sha256\":\"4444444444444444444444444444444444444444444444444444444444444444\","
    "\"profile_id\":\"ntsc-cart-4m-openipl3-rsp-functional-v1\","
    "\"qualification_profile_sha256\":\"5555555555555555555555555555555555555555555555555555555555555555\","
    "\"qualification_sha256\":\"6666666666666666666666666666666666666666666666666666666666666666\","
    "\"rom_sha256\":\"7777777777777777777777777777777777777777777777777777777777777777\","
    "\"source_projection_sha256\":\"8888888888888888888888888888888888888888888888888888888888888888\","
    "\"source_revision\":\"9999999999999999999999999999999999999999\"},"
    "\"registrations\":[{\"constructor\":\"rsp::authored\",\"execution\":\"once\","
    "\"group\":\"rsp\",\"index\":0,\"lane\":\"required-base\","
    "\"level\":\"BasicFunctionality\",\"name\":\"authored.rsp\","
    "\"source_file\":\"src/tests/rsp/authored.rs\","
    "\"source_file_sha256\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\","
    "\"source_name\":\"authored.rsp\",\"source_value_count\":0,"
    "\"values\":[{\"id\":\"once\",\"index\":0,"
    "\"selected_rom_sha256\":\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\","
    "\"selector\":\"r0000v00000\"}],"
    "\"values_method_sha256\":\"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc\"}],"
    "\"schema\":\"t64.rsp.inventory\",\"schema_version\":1}\n";

static const char registration_rsp_second[] =
    "{\"constructor\":\"rsp::authored.second\",\"execution\":\"once\","
    "\"group\":\"rsp\",\"index\":1,\"lane\":\"required-base\","
    "\"level\":\"BasicFunctionality\",\"name\":\"authored.rsp.member-with-a-deliberately-long-synthetic-label-xxxxxxxxxxxxxxxxxxxx\","
    "\"source_file\":\"src/tests/rsp/authored.rs\","
    "\"source_file_sha256\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\","
    "\"source_name\":\"authored.rsp.source-with-a-deliberately-long-synthetic-label-xxxxxxxxxxxxxxxxxxxx\",\"source_value_count\":0,"
    "\"values\":[{\"id\":\"once\",\"index\":0,"
    "\"selected_rom_sha256\":\"dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd\","
    "\"selector\":\"r0001v00000\"}],"
    "\"values_method_sha256\":\"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc\"}";

static const char guest_run_pass[] =
    "{\"records\":[{\"event\":\"start\",\"registration_index\":0,"
    "\"value_index\":0},{\"event\":\"outcome\",\"status\":\"pass\"},"
    "{\"complete\":true,\"event\":\"done\",\"record_count\":\"3\"}],"
    "\"schema\":\"t64.rsp-run\",\"schema_version\":1}\n";
static const char guest_run_incomplete[] =
    "{\"records\":[{\"event\":\"start\",\"registration_index\":0,"
    "\"value_index\":0},{\"event\":\"outcome\",\"status\":\"pass\"}],"
    "\"schema\":\"t64.rsp-run\",\"schema_version\":1}\n";

static char *component_mutation(void) {
  static char mutated[sizeof inventory_component_rsp];
  const char *needle = "\"component\":\"rsp\"";
  const char *found = strstr(inventory_component_rsp, needle);
  size_t prefix;
  if (found == NULL) return NULL;
  prefix = (size_t)(found - inventory_component_rsp);
  memcpy(mutated, inventory_component_rsp, prefix);
  memcpy(mutated + prefix, "\"component\":\"cpu\"",
         strlen("\"component\":\"cpu\""));
  memcpy(mutated + prefix + strlen("\"component\":\"cpu\""),
         found + strlen(needle),
         sizeof inventory_component_rsp - prefix - strlen(needle));
  return mutated;
}

static unsigned failures;

static void expect_status(const char *name, const uint8_t *bytes, size_t length,
                          t64_rsp_diag_status expected) {
  t64_rsp_diag_inventory *inventory = NULL;
  t64_rsp_diag_status actual =
      t64_rsp_diag_inventory_parse(bytes, length, &inventory);
  if (actual != expected) {
    (void)fprintf(stderr, "FAIL: %s expected %s observed %s\n", name,
                  t64_rsp_diag_status_name(expected),
                  t64_rsp_diag_status_name(actual));
    failures++;
  }
  t64_rsp_diag_inventory_destroy(inventory);
}

static void test_inventory_component_boundary(void) {
  char *mutated = component_mutation();
  expect_status("rsp_inventory_accepts_authored_baseline",
                (const uint8_t *)inventory_component_rsp,
                sizeof inventory_component_rsp - 1u, T64_RSP_DIAG_OK);
  if (mutated == NULL) {
    (void)fprintf(stderr, "FAIL: component mutation could not be authored\n");
    failures++;
    return;
  }
  expect_status("rsp_inventory_rejects_cross_component",
                (const uint8_t *)mutated, sizeof inventory_component_rsp - 1u,
                T64_RSP_DIAG_INVALID_COMPONENT);
}

static bool parse_authored_inventory(t64_rsp_diag_inventory **inventory) {
  return t64_rsp_diag_inventory_parse(
             (const uint8_t *)inventory_component_rsp,
             sizeof inventory_component_rsp - 1u, inventory) ==
         T64_RSP_DIAG_OK;
}

static bool replace_once(char *destination, size_t capacity,
                         const char *source, const char *needle,
                         const char *replacement) {
  const char *found = strstr(source, needle);
  size_t prefix, total;
  if (found == NULL) return false;
  prefix = (size_t)(found - source);
  total = prefix + strlen(replacement) + strlen(found + strlen(needle));
  if (total >= capacity) return false;
  memcpy(destination, source, prefix);
  memcpy(destination + prefix, replacement, strlen(replacement));
  memcpy(destination + prefix + strlen(replacement), found + strlen(needle),
         strlen(found + strlen(needle)) + 1u);
  return true;
}

static bool parse_two_member_inventory(t64_rsp_diag_inventory **inventory) {
  static const char *const counts_needle =
      "\"declared_counts\":{\"registrations\":1,\"required_values\":1,\"values\":1}";
  static const char *const counts_replacement =
      "\"declared_counts\":{\"registrations\":2,\"required_values\":2,\"values\":2}";
  static const char *const row_needle =
      "\"values_method_sha256\":\"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc\"}],"
      "\"schema\":\"t64.rsp.inventory\"";
  char with_two_counts[sizeof inventory_component_rsp + 64u];
  char with_two_rows[sizeof inventory_component_rsp + sizeof registration_rsp_second + 64u];
  char row_replacement[sizeof registration_rsp_second + 128u];
  int written;
  if (!replace_once(with_two_counts, sizeof with_two_counts,
                    inventory_component_rsp, counts_needle,
                    counts_replacement)) {
    return false;
  }
  written = snprintf(row_replacement, sizeof row_replacement,
      "\"values_method_sha256\":\"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc\"},%s],"
      "\"schema\":\"t64.rsp.inventory\"", registration_rsp_second);
  if (written < 0 || (size_t)written >= sizeof row_replacement ||
      !replace_once(with_two_rows, sizeof with_two_rows, with_two_counts,
                    row_needle, row_replacement)) {
    return false;
  }
  return t64_rsp_diag_inventory_parse((const uint8_t *)with_two_rows,
      strlen(with_two_rows), inventory) == T64_RSP_DIAG_OK;
}

static t64_rsp_diag_member_result sample_result(t64_rsp_diag_outcome outcome) {
  t64_rsp_diag_member_result result = {0};
  result.outcome = outcome;
  result.operation = outcome == T64_RSP_DIAG_OUTCOME_REFUSED
                         ? "rsp-opcode" : "guest-run";
  result.reason = outcome == T64_RSP_DIAG_OUTCOME_PASS ? "" :
                  outcome == T64_RSP_DIAG_OUTCOME_GUEST_FAIL
                      ? "assertion-failed" :
                  outcome == T64_RSP_DIAG_OUTCOME_REFUSED
                      ? "unsupported-opcode" :
                  outcome == T64_RSP_DIAG_OUTCOME_INCOMPLETE
                      ? "tick-bound" :
                  outcome == T64_RSP_DIAG_OUTCOME_TIMEOUT
                      ? "child-deadline" : "adapter-failed";
  result.retired_instructions = outcome == T64_RSP_DIAG_OUTCOME_PASS ? 2u : 1u;
  result.terminal_pc = T64_RSP_DIAG_TERMINAL_PC;
  result.terminal_opcode = T64_RSP_DIAG_TERMINAL_OPCODE;
  result.terminal_delay_slot_pc = T64_RSP_DIAG_TERMINAL_PC + 4u;
  result.terminal_delay_slot_opcode =
      T64_RSP_DIAG_TERMINAL_DELAY_SLOT_OPCODE;
  result.terminal_delay_slot_retired =
      outcome == T64_RSP_DIAG_OUTCOME_PASS ||
      outcome == T64_RSP_DIAG_OUTCOME_GUEST_FAIL;
  result.result_sha256 =
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  result.capture_sha256 =
      "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
  if (outcome == T64_RSP_DIAG_OUTCOME_PASS) {
    result.operation = "";
  } else if (outcome == T64_RSP_DIAG_OUTCOME_GUEST_FAIL) {
    result.operation = "guest-assertion";
  }
  return result;
}

static void test_guest_run_transcript(void) {
  static const char guest_run_fail[] =
      "{\"records\":[{\"event\":\"start\",\"registration_index\":0,"
      "\"value_index\":0},{\"event\":\"outcome\",\"status\":\"fail\"},"
      "{\"complete\":true,\"event\":\"done\",\"record_count\":\"3\"}],"
      "\"schema\":\"t64.rsp-run\",\"schema_version\":1}\n";
  static char wrong_selector[sizeof guest_run_pass];
  static char bad_done[sizeof guest_run_pass + 1u];
  t64_rsp_diag_guest_result result = {0u, 0u, T64_RSP_DIAG_OUTCOME_INCOMPLETE};
  char *selector;
  char *done;
  memcpy(wrong_selector, guest_run_pass, sizeof guest_run_pass);
  selector = strstr(wrong_selector, "\"value_index\":0");
  if (selector == NULL) {
    (void)fprintf(stderr, "FAIL: guest transcript selector control missing\n");
    failures++;
  } else {
    memcpy(selector + strlen("\"value_index\":"), "1", 1u);
  }
  memcpy(bad_done, guest_run_pass, sizeof guest_run_pass);
  done = strstr(bad_done, "\"complete\":true");
  if (done == NULL) {
    (void)fprintf(stderr, "FAIL: guest transcript done control missing\n");
    failures++;
  } else {
    size_t suffix_offset = strlen("\"complete\":true");
    size_t replacement_offset = strlen("\"complete\":false");
    memmove(done + replacement_offset, done + suffix_offset,
            strlen(done + suffix_offset) + 1u);
    memcpy(done + strlen("\"complete\":"), "false", 5u);
  }
  if (t64_rsp_diag_guest_transcript_parse(
          (const uint8_t *)guest_run_pass, sizeof guest_run_pass - 1u,
          0u, 0u, &result) != T64_RSP_DIAG_OK ||
      result.outcome != T64_RSP_DIAG_OUTCOME_PASS) {
    (void)fprintf(stderr, "FAIL: canonical selector-bound pass transcript refused\n");
    failures++;
  }
  if (t64_rsp_diag_guest_transcript_parse(
          (const uint8_t *)guest_run_fail, sizeof guest_run_fail - 1u,
          0u, 0u, &result) != T64_RSP_DIAG_OK ||
      result.outcome != T64_RSP_DIAG_OUTCOME_GUEST_FAIL) {
    (void)fprintf(stderr, "FAIL: canonical selector-bound guest-fail transcript refused\n");
    failures++;
  }
  if (t64_rsp_diag_guest_transcript_parse(
          (const uint8_t *)guest_run_incomplete,
          sizeof guest_run_incomplete - 1u, 0u, 0u, &result) !=
      T64_RSP_DIAG_INCOMPLETE) {
    (void)fprintf(stderr, "FAIL: missing guest done record accepted\n");
    failures++;
  }
  if (selector != NULL && t64_rsp_diag_guest_transcript_parse(
          (const uint8_t *)wrong_selector, sizeof wrong_selector - 1u,
          0u, 0u, &result) != T64_RSP_DIAG_IDENTITY_MISMATCH) {
    (void)fprintf(stderr, "FAIL: wrong guest selector accepted\n");
    failures++;
  }
  if (done != NULL && t64_rsp_diag_guest_transcript_parse(
          (const uint8_t *)bad_done, strlen(bad_done), 0u, 0u, &result) !=
      T64_RSP_DIAG_INCOMPLETE) {
    (void)fprintf(stderr, "FAIL: incomplete guest terminal record accepted\n");
    failures++;
  }
}

static bool encode_sample(t64_rsp_diag_inventory *inventory,
                          t64_rsp_diag_member_result *result,
                          char *batch, size_t capacity, size_t *length) {
  t64_rsp_diag_status status = t64_rsp_diag_batch_encode(
      inventory, 0u, 0u, result, batch, capacity, length);
  return status == T64_RSP_DIAG_OK;
}

static void test_batch_outcomes_and_partition(void) {
  static const char *const expected_names[] = {
      "pass", "guest-fail", "refused", "incomplete", "timeout",
      "harness-error"};
  t64_rsp_diag_inventory *inventory = NULL;
  size_t outcome_index;
  if (!parse_authored_inventory(&inventory)) {
    (void)fprintf(stderr, "FAIL: batch test inventory did not parse\n");
    failures++;
    return;
  }
  for (outcome_index = 0u;
       outcome_index < sizeof expected_names / sizeof expected_names[0];
       outcome_index++) {
    t64_rsp_diag_outcome outcome = (t64_rsp_diag_outcome)outcome_index;
    t64_rsp_diag_member_result result = sample_result(outcome);
    t64_rsp_diag_aggregate *aggregate = NULL;
    char batch[T64_RSP_DIAG_MAX_BATCH_BYTES];
    size_t length = 0u;
    t64_rsp_diag_status status;
    if (strcmp(t64_rsp_diag_outcome_name(outcome),
               expected_names[outcome_index]) != 0 ||
        !encode_sample(inventory, &result, batch, sizeof batch, &length) ||
        t64_rsp_diag_aggregate_create(inventory, &aggregate) !=
            T64_RSP_DIAG_OK) {
      (void)fprintf(stderr, "FAIL: outcome encoding %s\n",
                    expected_names[outcome_index]);
      failures++;
      t64_rsp_diag_aggregate_destroy(aggregate);
      continue;
    }
    status = t64_rsp_diag_aggregate_add_batch(
        aggregate, (const uint8_t *)batch, length);
    if (status != T64_RSP_DIAG_OK ||
        t64_rsp_diag_aggregate_member_count(aggregate) != 1u ||
        t64_rsp_diag_aggregate_is_complete(aggregate) !=
            (outcome == T64_RSP_DIAG_OUTCOME_PASS)) {
      (void)fprintf(stderr, "FAIL: outcome authentication/partition %s status=%s\n",
                    expected_names[outcome_index],
                    t64_rsp_diag_status_name(status));
      failures++;
    }
    if (outcome == T64_RSP_DIAG_OUTCOME_PASS &&
        t64_rsp_diag_aggregate_add_batch(
            aggregate, (const uint8_t *)batch, length) !=
            T64_RSP_DIAG_DUPLICATE_MEMBER) {
      (void)fprintf(stderr, "FAIL: duplicate RSP member was accepted\n");
      failures++;
    }
    t64_rsp_diag_aggregate_destroy(aggregate);
  }
  t64_rsp_diag_inventory_destroy(inventory);
}

static void test_single_member_at_nonzero_inventory_ordinal(void) {
  t64_rsp_diag_inventory *inventory = NULL;
  t64_rsp_diag_aggregate *aggregate = NULL;
  t64_rsp_diag_member long_member = {0};
  t64_rsp_diag_member_result expected = sample_result(T64_RSP_DIAG_OUTCOME_GUEST_FAIL);
  t64_rsp_diag_member_result observed = {0};
  char batch[T64_RSP_DIAG_MAX_BATCH_BYTES];
  size_t length = 0u;
  if (!parse_two_member_inventory(&inventory) ||
      !t64_rsp_diag_inventory_member_at(inventory, 1u, &long_member) ||
      strlen(long_member.registration_name) <= 64u ||
      strlen(long_member.source_name) <= 64u ||
      t64_rsp_diag_batch_encode(inventory, 1u, 0u, &expected, batch,
                                sizeof batch, &length) != T64_RSP_DIAG_OK ||
      t64_rsp_diag_aggregate_create(inventory, &aggregate) != T64_RSP_DIAG_OK ||
      t64_rsp_diag_aggregate_add_batch(aggregate, (const uint8_t *)batch,
                                      length) != T64_RSP_DIAG_OK) {
    (void)fprintf(stderr, "FAIL: nonzero authored member setup failed\n");
    failures++;
    goto cleanup;
  }
  if (t64_rsp_diag_aggregate_member_count(aggregate) != 1u ||
      t64_rsp_diag_aggregate_member_at(aggregate, 0u, &observed) ||
      !t64_rsp_diag_aggregate_member_at(aggregate, 1u, &observed) ||
      !t64_rsp_diag_aggregate_single_member(aggregate, &observed) ||
      observed.registration_index != 1u || observed.value_index != 0u ||
      observed.outcome != T64_RSP_DIAG_OUTCOME_GUEST_FAIL) {
    (void)fprintf(stderr, "FAIL: single-member lookup lost a nonzero inventory ordinal\n");
    failures++;
  }
cleanup:
  t64_rsp_diag_aggregate_destroy(aggregate);
  t64_rsp_diag_inventory_destroy(inventory);
}

static void test_registration_name_bound_plus_one_is_rejected(void) {
  t64_rsp_diag_inventory *inventory = NULL;
  char oversized_name[T64_RSP_DIAG_MAX_NAME_BYTES + 2u];
  char replacement[sizeof oversized_name + 16u];
  char mutated[sizeof inventory_component_rsp + sizeof oversized_name + 32u];
  int written;
  t64_rsp_diag_status status;
  (void)memset(oversized_name, 'x', sizeof oversized_name - 1u);
  oversized_name[sizeof oversized_name - 1u] = '\0';
  written = snprintf(replacement, sizeof replacement, "\"name\":\"%s\"",
                     oversized_name);
  if (written < 0 || (size_t)written >= sizeof replacement ||
      !replace_once(mutated, sizeof mutated, inventory_component_rsp,
                    "\"name\":\"authored.rsp\"", replacement)) {
    (void)fprintf(stderr, "FAIL: oversized authored label setup failed\n");
    failures++;
    return;
  }
  status = t64_rsp_diag_inventory_parse((const uint8_t *)mutated,
      strlen(mutated), &inventory);
  if (status != T64_RSP_DIAG_INVALID_INVENTORY || inventory != NULL) {
    (void)fprintf(stderr, "FAIL: inventory accepted a label above its field bound\n");
    failures++;
  }
  t64_rsp_diag_inventory_destroy(inventory);
}

static void test_batch_total_byte_limit_is_preserved(void) {
  t64_rsp_diag_inventory *inventory = NULL;
  t64_rsp_diag_aggregate *aggregate = NULL;
  t64_rsp_diag_member_result result = sample_result(T64_RSP_DIAG_OUTCOME_PASS);
  char batch[T64_RSP_DIAG_MAX_BATCH_BYTES];
  uint8_t oversized[T64_RSP_DIAG_MAX_BATCH_BYTES + 1u];
  size_t length = 0u;
  if (!parse_authored_inventory(&inventory) ||
      t64_rsp_diag_batch_encode(inventory, 0u, 0u, &result, batch,
                                sizeof batch, &length) != T64_RSP_DIAG_OK ||
      length >= sizeof oversized ||
      t64_rsp_diag_aggregate_create(inventory, &aggregate) != T64_RSP_DIAG_OK) {
    (void)fprintf(stderr, "FAIL: oversized batch setup failed\n");
    failures++;
    goto cleanup;
  }
  (void)memcpy(oversized, batch, length);
  (void)memset(oversized + length, ' ', sizeof oversized - length);
  if (t64_rsp_diag_aggregate_add_batch(aggregate, oversized,
          sizeof oversized) != T64_RSP_DIAG_INVALID_BATCH) {
    (void)fprintf(stderr, "FAIL: batch parser accepted input above its total byte cap\n");
    failures++;
  }
cleanup:
  t64_rsp_diag_aggregate_destroy(aggregate);
  t64_rsp_diag_inventory_destroy(inventory);
}

static void test_pass_requires_retired_delay_slot(void) {
  t64_rsp_diag_inventory *inventory = NULL;
  t64_rsp_diag_member_result result =
      sample_result(T64_RSP_DIAG_OUTCOME_PASS);
  char batch[T64_RSP_DIAG_MAX_BATCH_BYTES];
  char forged[T64_RSP_DIAG_MAX_BATCH_BYTES];
  size_t length = 0u;
  const char *terminal;
  const char *terminal_pc;
  size_t prefix;
  t64_rsp_diag_aggregate *aggregate = NULL;
  if (!parse_authored_inventory(&inventory) ||
      !encode_sample(inventory, &result, batch, sizeof batch, &length)) {
    (void)fprintf(stderr, "FAIL: authenticated completion baseline missing\n");
    failures++;
    t64_rsp_diag_inventory_destroy(inventory);
    return;
  }
  result.terminal_delay_slot_retired = false;
  if (t64_rsp_diag_batch_encode(inventory, 0u, 0u, &result, forged,
                                sizeof forged, &length) !=
      T64_RSP_DIAG_INVALID_ARGUMENT) {
    (void)fprintf(stderr, "FAIL: pass without delay-slot retirement encoded\n");
    failures++;
  }
  result.terminal_delay_slot_retired = true;
  result.terminal_delay_slot_pc++;
  if (t64_rsp_diag_batch_encode(inventory, 0u, 0u, &result, forged,
                                sizeof forged, &length) !=
      T64_RSP_DIAG_INVALID_ARGUMENT) {
    (void)fprintf(stderr, "FAIL: altered terminal delay-slot PC encoded\n");
    failures++;
  }
  result.terminal_delay_slot_pc--;
  result.terminal_opcode ^= 4u;
  if (t64_rsp_diag_batch_encode(inventory, 0u, 0u, &result, forged,
                                sizeof forged, &length) !=
      T64_RSP_DIAG_INVALID_ARGUMENT) {
    (void)fprintf(stderr, "FAIL: altered terminal opcode encoded\n");
    failures++;
  }
  result.terminal_opcode = T64_RSP_DIAG_TERMINAL_OPCODE;
  if (!encode_sample(inventory, &result, batch, sizeof batch, &length)) {
    failures++;
    t64_rsp_diag_inventory_destroy(inventory);
    return;
  }
  terminal_pc = strstr(batch, "\"terminal_pc\":2147484704");
  if (terminal_pc == NULL) {
    (void)fprintf(stderr, "FAIL: terminal PC control field missing\n");
    failures++;
  } else {
    size_t offset = (size_t)(terminal_pc - batch) +
                    strlen("\"terminal_pc\":214748470");
    memcpy(forged, batch, length);
    forged[offset] = '5';
    if (t64_rsp_diag_aggregate_create(inventory, &aggregate) !=
            T64_RSP_DIAG_OK ||
        t64_rsp_diag_aggregate_add_batch(
            aggregate, (const uint8_t *)forged, length) !=
            T64_RSP_DIAG_INVALID_BATCH ||
        t64_rsp_diag_aggregate_is_complete(aggregate)) {
      (void)fprintf(stderr, "FAIL: forged terminal PC accepted\n");
      failures++;
    }
    t64_rsp_diag_aggregate_destroy(aggregate);
    aggregate = NULL;
  }
  terminal = strstr(batch, "\"terminal_delay_slot_retired\":true");
  if (terminal == NULL) {
    (void)fprintf(stderr, "FAIL: terminal retirement field absent\n");
    failures++;
  } else {
    prefix = (size_t)(terminal - batch);
    memcpy(forged, batch, length);
    memcpy(forged + prefix, "\"terminal_delay_slot_retired\":false",
           strlen("\"terminal_delay_slot_retired\":false"));
    if (t64_rsp_diag_aggregate_create(inventory, &aggregate) !=
            T64_RSP_DIAG_OK ||
        t64_rsp_diag_aggregate_add_batch(
            aggregate, (const uint8_t *)forged, length) !=
            T64_RSP_DIAG_INVALID_BATCH ||
        t64_rsp_diag_aggregate_is_complete(aggregate)) {
      (void)fprintf(stderr, "FAIL: altered delay-slot terminal record accepted\n");
      failures++;
    }
  }
  t64_rsp_diag_aggregate_destroy(aggregate);
  t64_rsp_diag_inventory_destroy(inventory);
}

static void test_missing_terminal_record_is_rejected(void) {
  static const char terminal_field[] = ",\"terminal_delay_slot_retired\":true";
  t64_rsp_diag_inventory *inventory = NULL;
  t64_rsp_diag_member_result result = sample_result(T64_RSP_DIAG_OUTCOME_PASS);
  char batch[T64_RSP_DIAG_MAX_BATCH_BYTES];
  char forged[T64_RSP_DIAG_MAX_BATCH_BYTES];
  size_t length = 0u;
  size_t forged_length;
  const char *field;
  t64_rsp_diag_aggregate *aggregate = NULL;
  if (!parse_authored_inventory(&inventory) ||
      !encode_sample(inventory, &result, batch, sizeof batch, &length)) {
    (void)fprintf(stderr, "FAIL: terminal omission baseline missing\n");
    failures++;
    t64_rsp_diag_inventory_destroy(inventory);
    return;
  }
  field = strstr(batch, terminal_field);
  if (field == NULL) {
    (void)fprintf(stderr, "FAIL: terminal omission control field missing\n");
    failures++;
    t64_rsp_diag_inventory_destroy(inventory);
    return;
  }
  {
    size_t prefix = (size_t)(field - batch);
    size_t suffix = length - prefix - (sizeof terminal_field - 1u);
    memcpy(forged, batch, prefix);
    memcpy(forged + prefix, field + sizeof terminal_field - 1u, suffix);
    forged_length = prefix + suffix;
  }
  if (t64_rsp_diag_aggregate_create(inventory, &aggregate) !=
          T64_RSP_DIAG_OK ||
      t64_rsp_diag_aggregate_add_batch(
          aggregate, (const uint8_t *)forged, forged_length) !=
          T64_RSP_DIAG_INVALID_BATCH ||
      t64_rsp_diag_aggregate_is_complete(aggregate)) {
    (void)fprintf(stderr, "FAIL: batch without terminal retirement record accepted\n");
    failures++;
  }
  t64_rsp_diag_aggregate_destroy(aggregate);
  t64_rsp_diag_inventory_destroy(inventory);
}

static void test_legacy_refusal_without_terminal_pair_is_accepted(void) {
  static const char terminal_pair_fields[] =
      ",\"terminal_delay_slot_opcode\":0,\"terminal_delay_slot_pc\":0";
  t64_rsp_diag_inventory *inventory = NULL;
  t64_rsp_diag_member_result result =
      sample_result(T64_RSP_DIAG_OUTCOME_REFUSED);
  char batch[T64_RSP_DIAG_MAX_BATCH_BYTES];
  char legacy[T64_RSP_DIAG_MAX_BATCH_BYTES];
  size_t length = 0u;
  size_t legacy_length;
  char *field;
  size_t prefix;
  t64_rsp_diag_aggregate *aggregate = NULL;
  result.terminal_pc = UINT32_C(0x1234);
  result.terminal_opcode = UINT32_C(0x5678);
  result.terminal_delay_slot_pc = 0u;
  result.terminal_delay_slot_opcode = 0u;
  if (!parse_authored_inventory(&inventory) ||
      !encode_sample(inventory, &result, batch, sizeof batch, &length)) {
    (void)fprintf(stderr, "FAIL: refused result baseline missing\n");
    failures++;
    t64_rsp_diag_inventory_destroy(inventory);
    return;
  }
  batch[length] = '\0';
  field = strstr(batch, terminal_pair_fields);
  if (field == NULL) {
    (void)fprintf(stderr, "FAIL: optional terminal pair control fields missing\n");
    failures++;
    t64_rsp_diag_inventory_destroy(inventory);
    return;
  }
  prefix = (size_t)(field - batch);
  legacy_length = length - (sizeof terminal_pair_fields - 1u);
  memcpy(legacy, batch, prefix);
  memcpy(legacy + prefix,
         field + sizeof terminal_pair_fields - 1u,
         length - prefix - (sizeof terminal_pair_fields - 1u));
  legacy[legacy_length] = '\0';
  if (t64_rsp_diag_aggregate_create(inventory, &aggregate) != T64_RSP_DIAG_OK ||
      t64_rsp_diag_aggregate_add_batch(
          aggregate, (const uint8_t *)legacy, legacy_length) !=
          T64_RSP_DIAG_OK ||
      t64_rsp_diag_aggregate_member_count(aggregate) != 1u ||
      t64_rsp_diag_aggregate_is_complete(aggregate)) {
    (void)fprintf(stderr,
                  "FAIL: legacy refusal without terminal proof was not retained\n");
    failures++;
  }
  t64_rsp_diag_aggregate_destroy(aggregate);
  t64_rsp_diag_inventory_destroy(inventory);
}

int main(int argc, char **argv) {
  if (argc != 2 || strcmp(argv[1], "--protocol") != 0) return 2;
  test_inventory_component_boundary();
  test_guest_run_transcript();
  test_batch_outcomes_and_partition();
  test_single_member_at_nonzero_inventory_ordinal();
  test_registration_name_bound_plus_one_is_rejected();
  test_batch_total_byte_limit_is_preserved();
  test_pass_requires_retired_delay_slot();
  test_missing_terminal_record_is_rejected();
  test_legacy_refusal_without_terminal_pair_is_accepted();
  if (failures != 0u) return 1;
  (void)puts("RSP diagnostic protocol controls PASS");
  return 0;
}
