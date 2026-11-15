#include "t64_base_diag_protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_sha256.h"
#include "t64_diag_common.h"

static const char source_authority_sha256[] =
    "794ee1efc617ec02bde6949bd855b1d830f46f0d0e59bc116c46baf01a184a02";

static const uint8_t source_authority[] =
    "{\"members\":["
    "{\"execution\":\"once\",\"group\":\"arithmetic\",\"index\":0,\"lane\":\"base-required\",\"level\":\"BasicFunctionality\",\"name\":\"empty selector\",\"reason\":\"\",\"required\":true,\"value_id\":\"\",\"value_index\":0},"
    "{\"execution\":\"blocked\",\"group\":\"rdp\",\"index\":1,\"lane\":\"base-required\",\"level\":\"RDPBasic\",\"name\":\"blocked asset\",\"reason\":\"asset not cleared\",\"required\":true,\"value_id\":\"blocked.asset\",\"value_index\":0},"
    "{\"execution\":\"values\",\"group\":\"arithmetic\",\"index\":2,\"lane\":\"base-required\",\"level\":\"Weird\",\"name\":\"same display\",\"reason\":\"\",\"required\":true,\"value_id\":\"first\",\"value_index\":0},"
    "{\"execution\":\"values\",\"group\":\"arithmetic\",\"index\":3,\"lane\":\"base-required\",\"level\":\"Weird\",\"name\":\"same display\",\"reason\":\"\",\"required\":true,\"value_id\":\"second\",\"value_index\":0}],"
    "\"schema\":\"t64.base-source-authority\",\"schema_version\":1,"
    "\"source_revision\":\"0000000000000000000000000000000000000000\"}\n";

static const uint8_t inventory[] =
    "{\"members\":["
    "{\"execution\":\"once\",\"group\":\"arithmetic\",\"index\":0,\"lane\":\"base-required\",\"level\":\"BasicFunctionality\",\"name\":\"empty selector\",\"reason\":\"\",\"required\":true,\"value_id\":\"\",\"value_index\":0},"
    "{\"execution\":\"blocked\",\"group\":\"rdp\",\"index\":1,\"lane\":\"base-required\",\"level\":\"RDPBasic\",\"name\":\"blocked asset\",\"reason\":\"asset not cleared\",\"required\":true,\"value_id\":\"blocked.asset\",\"value_index\":0},"
    "{\"execution\":\"values\",\"group\":\"arithmetic\",\"index\":2,\"lane\":\"base-required\",\"level\":\"Weird\",\"name\":\"same display\",\"reason\":\"\",\"required\":true,\"value_id\":\"first\",\"value_index\":0},"
    "{\"execution\":\"values\",\"group\":\"arithmetic\",\"index\":3,\"lane\":\"base-required\",\"level\":\"Weird\",\"name\":\"same display\",\"reason\":\"\",\"required\":true,\"value_id\":\"second\",\"value_index\":0}],"
    "\"schema\":\"t64.base-inventory\",\"schema_version\":1,"
    "\"source_authority_sha256\":\"794ee1efc617ec02bde6949bd855b1d830f46f0d0e59bc116c46baf01a184a02\","
    "\"source_revision\":\"0000000000000000000000000000000000000000\"}\n";

static const uint8_t reduced_source_authority[] =
    "{\"members\":["
    "{\"execution\":\"once\",\"group\":\"arithmetic\",\"index\":0,\"lane\":\"base-required\",\"level\":\"BasicFunctionality\",\"name\":\"empty selector\",\"reason\":\"\",\"required\":true,\"value_id\":\"\",\"value_index\":0},"
    "{\"execution\":\"values\",\"group\":\"arithmetic\",\"index\":2,\"lane\":\"base-required\",\"level\":\"Weird\",\"name\":\"same display\",\"reason\":\"\",\"required\":true,\"value_id\":\"first\",\"value_index\":0},"
    "{\"execution\":\"values\",\"group\":\"arithmetic\",\"index\":3,\"lane\":\"base-required\",\"level\":\"Weird\",\"name\":\"same display\",\"reason\":\"\",\"required\":true,\"value_id\":\"second\",\"value_index\":0}],"
    "\"schema\":\"t64.base-source-authority\",\"schema_version\":1,"
    "\"source_revision\":\"0000000000000000000000000000000000000000\"}\n";

static const uint8_t reduced_inventory[] =
    "{\"members\":["
    "{\"execution\":\"once\",\"group\":\"arithmetic\",\"index\":0,\"lane\":\"base-required\",\"level\":\"BasicFunctionality\",\"name\":\"empty selector\",\"reason\":\"\",\"required\":true,\"value_id\":\"\",\"value_index\":0},"
    "{\"execution\":\"values\",\"group\":\"arithmetic\",\"index\":2,\"lane\":\"base-required\",\"level\":\"Weird\",\"name\":\"same display\",\"reason\":\"\",\"required\":true,\"value_id\":\"first\",\"value_index\":0},"
    "{\"execution\":\"values\",\"group\":\"arithmetic\",\"index\":3,\"lane\":\"base-required\",\"level\":\"Weird\",\"name\":\"same display\",\"reason\":\"\",\"required\":true,\"value_id\":\"second\",\"value_index\":0}],"
    "\"schema\":\"t64.base-inventory\",\"schema_version\":1,"
    "\"source_authority_sha256\":\"ea6829e21abfb31976bd999da2f3e6af79afbbbf5d8b5aba2bccfd909d3028c8\","
    "\"source_revision\":\"0000000000000000000000000000000000000000\"}\n";

static void print_hash(const uint8_t *bytes, size_t length, char output[65]) {
  static const char digits[] = "0123456789abcdef";
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  size_t i;
  t64_sha256_ctx context;
  t64_sha256_init(&context);
  t64_sha256_update(&context, bytes, length);
  t64_sha256_final(&context, digest);
  for (i = 0u; i < sizeof digest; i++) {
    output[i * 2u] = digits[digest[i] >> 4];
    output[i * 2u + 1u] = digits[digest[i] & 0x0fu];
  }
  output[64] = '\0';
}

static int expect_status(const char *label, t64_base_diag_status actual,
                         t64_base_diag_status expected) {
  if (actual == expected) return 0;
  (void)fprintf(stderr, "FAIL: %s: expected %s, got %s\n", label,
                t64_base_diag_status_name(expected),
                t64_base_diag_status_name(actual));
  return 1;
}

static int expect_true(const char *label, bool actual) {
  if (actual) return 0;
  (void)fprintf(stderr, "FAIL: %s\n", label);
  return 1;
}

static int test_bound_records(const t64_base_diag_inventory *parsed,
                               const uint8_t *original, size_t original_length) {
  static const char hash0[] =
      "0000000000000000000000000000000000000000000000000000000000000000";
  static const char hash1[] =
      "1111111111111111111111111111111111111111111111111111111111111111";
  static const char build[] =
      "t64-build-v1-2222222222222222222222222222222222222222222222222222222222222222";
  static const uint8_t empty[] = "";
  t64_base_diag_target target = {
      hash0, "ntsc-cart-4m-openipl3-rdram-b-t1-v1", hash1, build};
  t64_base_diag_execution proof = {
      .selected_rom_sha256 = hash1, .reason = "", .tick = 42u,
      .capture_total = original_length, .park_sequence = 4u,
      .terminal_pc = 0x80000420u, .terminal_opcode = 0x08000108u,
      .terminal_delay_opcode = 0u, .entry_reached = true,
      .capture_exact = true, .capture_lost = false};
  uint8_t (*records)[T64_BASE_DIAG_MAX_RECORD_BYTES] =
      calloc(4u, sizeof *records);
  size_t lengths[4] = {0u, 0u, 0u, 0u};
  uint8_t *reports[2] = {malloc(65536u), malloc(65536u)};
  size_t report_lengths[2] = {0u, 0u};
  int failures = 0;
  size_t index;
  unsigned variant;
  if (records == NULL || reports[0] == NULL || reports[1] == NULL) {
    failures += expect_true("bound fixture allocations succeed", false);
    goto cleanup;
  }
  failures += expect_status("bound producer encodes original verdict",
      t64_base_diag_encode_bound_member_record(parsed, 0u, 0u,
          T64_BASE_DIAG_OUTCOME_PASS, &target, &proof, original, original_length,
          records[0], sizeof records[0], &lengths[0]), T64_BASE_DIAG_OK);
  for (index = 1u; index < 4u; index++) {
    t64_base_diag_execution disposition = {
        .selected_rom_sha256 = "", .reason = index == 1u
            ? "asset not cleared" : "not-selected", .capture_exact = true};
    failures += expect_status("bound producer encodes source disposition",
        t64_base_diag_encode_bound_member_record(parsed, (uint32_t)index, 0u,
            index == 1u ? T64_BASE_DIAG_OUTCOME_BLOCKED
                        : T64_BASE_DIAG_OUTCOME_UNEXECUTED,
            &target, &disposition, empty, 0u, records[index],
            sizeof records[index], &lengths[index]), T64_BASE_DIAG_OK);
  }
  for (variant = 0u; variant < 2u; variant++) {
    t64_base_diag_aggregate *aggregate = NULL;
    failures += expect_status("bound aggregate creates for checked target",
        t64_base_diag_aggregate_create_bound(parsed, &target, &aggregate),
        T64_BASE_DIAG_OK);
    if (aggregate == NULL) continue;
    failures += expect_status("selected image binds independently of result",
        t64_base_diag_aggregate_bind_image(aggregate, 0u, 0u, hash1),
        T64_BASE_DIAG_OK);
    for (index = 0u; index < 4u; index++) {
      size_t ordinal = variant == 0u ? index : 3u - index;
      failures += expect_status("every result or disposition authenticates",
          t64_base_diag_aggregate_add_original(aggregate, records[ordinal],
              lengths[ordinal], ordinal == 0u ? original : empty,
              ordinal == 0u ? original_length : 0u), T64_BASE_DIAG_OK);
    }
    failures += expect_true("complete classification is not complete execution",
        t64_base_diag_aggregate_is_complete(aggregate) &&
        !t64_base_diag_aggregate_execution_complete(aggregate) &&
        !t64_base_diag_aggregate_is_all_pass(aggregate));
    failures += expect_status("C owns full group and member report",
        t64_base_diag_aggregate_report(aggregate, reports[variant], 65536u,
                                       &report_lengths[variant]),
        T64_BASE_DIAG_OK);
    failures += expect_status("bounded report refuses undersized storage",
        t64_base_diag_aggregate_report(aggregate, records[0], 8u,
                                       &lengths[0]), T64_BASE_DIAG_OUTPUT_OVERFLOW);
    /* Restore the original record after the deliberate small-buffer mutation. */
    failures += expect_status("original encoder restores independent fixture",
        t64_base_diag_encode_bound_member_record(parsed, 0u, 0u,
            T64_BASE_DIAG_OUTCOME_PASS, &target, &proof, original, original_length,
            records[0], sizeof records[0], &lengths[0]), T64_BASE_DIAG_OK);
    t64_base_diag_aggregate_destroy(aggregate);
  }
  failures += expect_true("arrival permutations preserve exact C classification",
      report_lengths[0] != 0u && report_lengths[0] == report_lengths[1] &&
      memcmp(reports[0], reports[1], report_lengths[0]) == 0);
  {
    t64_json_doc report = {0};
    t64_json_error error;
    const t64_json_limits limits = {65536u, 16u, 2048u, 512u};
    int result = t64_json_parse(reports[0], report_lengths[0], &limits,
                                &report, &error);
    failures += expect_true("full C report is canonical JSON",
        result == 0 && t64_diag_json_canonical_document(&report,
            reports[0], report_lengths[0]));
    if (result == 0) {
      const t64_json_node *counts = t64_json_get(report.root, "counts");
      const t64_json_node *members = t64_json_get(report.root, "members");
      const t64_json_node *source_complete = t64_json_get(report.root, "source_complete");
      const t64_json_node *execution_complete = t64_json_get(report.root, "authenticated_execution_complete");
      const t64_json_node *all_pass = t64_json_get(report.root, "all_pass");
      failures += expect_true("C distinguishes all classification flags",
          source_complete != NULL && source_complete->type == T64_JSON_BOOL &&
          source_complete->num == 1 && execution_complete != NULL &&
          execution_complete->num == 0 && all_pass != NULL && all_pass->num == 0);
      failures += expect_true("C report keeps blocked and unexecuted members counted",
          counts != NULL && t64_json_get(counts, "expected") != NULL &&
          t64_json_get(counts, "expected")->num == 4 &&
          t64_json_get(counts, "classified")->num == 4 &&
          t64_json_get(counts, "blocked")->num == 1 &&
          t64_json_get(counts, "unexecuted")->num == 2 &&
          members != NULL && members->type == T64_JSON_ARRAY && members->count == 4u);
    }
    t64_json_free(&report);
  }
  for (variant = 0u; variant < 14u; variant++) {
    t64_base_diag_aggregate *aggregate = NULL;
    t64_base_diag_target changed_target = target;
    t64_base_diag_execution changed_proof = proof;
    size_t length = 0u;
    if (variant == 0u) changed_target.context_sha256 = hash1;
    if (variant == 1u) changed_target.profile_id = "historical-warm";
    if (variant == 2u) changed_target.profile_sha256 = hash0;
    if (variant == 3u) changed_target.core_build_id =
        "t64-build-v1-0000000000000000000000000000000000000000000000000000000000000000";
    if (variant == 4u) changed_proof.terminal_pc += 4u;
    if (variant == 5u) changed_proof.terminal_opcode ^= 1u;
    if (variant == 6u) changed_proof.terminal_delay_opcode = 1u;
    if (variant == 7u) changed_proof.park_sequence = 3u;
    if (variant == 8u) changed_proof.entry_reached = false;
    if (variant == 9u) changed_proof.capture_exact = false;
    if (variant == 10u) changed_proof.capture_lost = true;
    if (variant == 11u) changed_proof.capture_total++;
    if (variant == 12u) changed_proof.selected_rom_sha256 = hash0;
    if (variant == 13u) changed_proof.tick = UINT64_C(10000001);
    failures += expect_status("mutation uses production bound encoder",
        t64_base_diag_encode_bound_member_record(parsed, 0u, 0u,
            T64_BASE_DIAG_OUTCOME_PASS, &changed_target, &changed_proof,
            original, original_length, records[0], sizeof records[0], &length),
        T64_BASE_DIAG_OK);
    failures += expect_status("fresh aggregate for each intended mutation",
        t64_base_diag_aggregate_create_bound(parsed, &target, &aggregate),
        T64_BASE_DIAG_OK);
    if (aggregate != NULL) {
      failures += expect_status("independent image identity survives mutation",
          t64_base_diag_aggregate_bind_image(aggregate, 0u, 0u, hash1),
          T64_BASE_DIAG_OK);
      failures += expect_status("foreign target or forged terminal is refused",
          t64_base_diag_aggregate_add_original(aggregate, records[0], length,
              original, original_length), T64_BASE_DIAG_IDENTITY_MISMATCH);
      failures += expect_true("failed authentication poisons completion",
          !t64_base_diag_aggregate_is_complete(aggregate) &&
          !t64_base_diag_aggregate_is_all_pass(aggregate));
    }
    t64_base_diag_aggregate_destroy(aggregate);
  }
  {
    static const char member[] =
        "{\"execution\":\"once\",\"group\":\"authored\",\"index\":0,\"lane\":\"required-base\",\"level\":\"BasicFunctionality\",\"name\":\"one member\",\"reason\":\"\",\"required\":true,\"value_id\":\"\",\"value_index\":0}";
    static const t64_base_diag_outcome outcomes[] = {
        T64_BASE_DIAG_OUTCOME_PASS, T64_BASE_DIAG_OUTCOME_FAIL,
        T64_BASE_DIAG_OUTCOME_INCOMPLETE, T64_BASE_DIAG_OUTCOME_GUEST_STOP,
        T64_BASE_DIAG_OUTCOME_HARNESS_ERROR, T64_BASE_DIAG_OUTCOME_TIMEOUT,
        T64_BASE_DIAG_OUTCOME_UNEXECUTED, T64_BASE_DIAG_OUTCOME_REFUSED};
    static const char *const reasons[] = {"", "", "guest-tick-bound",
        "cpu-unsupported", "adapter-harness-error", "host-timeout",
        "not-selected", "unsupported-device"};
    char authority[1024];
    char one_inventory[1200];
    char authority_hash[65];
    char *fail_original = malloc(original_length + 1u);
    t64_base_diag_inventory *one = NULL;
    int size = snprintf(authority, sizeof authority,
        "{\"members\":[%s],\"schema\":\"t64.base-source-authority\",\"schema_version\":1,\"source_revision\":\"%s\"}\n",
        member, "0000000000000000000000000000000000000000");
    if (fail_original == NULL || size <= 0 || (size_t)size >= sizeof authority) {
      failures += expect_true("one-member positive control setup succeeds", false);
      free(fail_original);
      goto cleanup;
    }
    memcpy(fail_original, original, original_length);
    fail_original[original_length] = '\0';
    {
      char *status = strstr(fail_original, "\"status\":\"pass\"");
      if (status == NULL) {
        failures += expect_true("independent fail transcript starts from canonical fixture", false);
        free(fail_original);
        goto cleanup;
      }
      memcpy(status + sizeof "\"status\":\"" - 1u, "fail", 4u);
    }
    print_hash((const uint8_t *)authority, (size_t)size, authority_hash);
    size = snprintf(one_inventory, sizeof one_inventory,
        "{\"members\":[%s],\"schema\":\"t64.base-inventory\",\"schema_version\":1,\"source_authority_sha256\":\"%s\",\"source_revision\":\"%s\"}\n",
        member, authority_hash, "0000000000000000000000000000000000000000");
    if (size <= 0 || (size_t)size >= sizeof one_inventory) {
      failures += expect_true("one-member inventory fits its bound", false);
      free(fail_original);
      goto cleanup;
    }
    failures += expect_status("separate authored universe has an exact source anchor",
        t64_base_diag_inventory_parse((const uint8_t *)authority, strlen(authority),
            authority_hash, (const uint8_t *)one_inventory, (size_t)size, &one),
        T64_BASE_DIAG_OK);
    if (one != NULL) for (index = 0u; index < sizeof outcomes / sizeof outcomes[0]; index++) {
      t64_base_diag_aggregate *aggregate = NULL;
      t64_base_diag_execution state = proof;
      const uint8_t *bytes = index == 0u ? original : index == 1u
                                ? (const uint8_t *)fail_original : empty;
      size_t length = index <= 1u ? original_length : 0u;
      size_t record_length = 0u;
      state.reason = reasons[index];
      if (index > 1u) {
        memset(&state, 0, sizeof state);
        state.selected_rom_sha256 = index == 6u ? "" : hash1;
        state.reason = reasons[index];
        state.capture_exact = true;
      }
      failures += expect_status("all outcome classes use canonical production encoding",
          t64_base_diag_encode_bound_member_record(one, 0u, 0u,
              outcomes[index], &target, &state, bytes, length, records[0],
              sizeof records[0], &record_length), T64_BASE_DIAG_OK);
      failures += expect_status("fresh source-bound aggregate for each class",
          t64_base_diag_aggregate_create_bound(one, &target, &aggregate),
          T64_BASE_DIAG_OK);
      if (aggregate != NULL) {
        failures += expect_status("positive image binding is checked before add",
            t64_base_diag_aggregate_bind_image(aggregate, 0u, 0u, hash1),
            T64_BASE_DIAG_OK);
        failures += expect_status("actual verdict or disposition class authenticates",
            t64_base_diag_aggregate_add_original(aggregate, records[0],
                record_length, bytes, length), T64_BASE_DIAG_OK);
        failures += expect_true("source completeness is independent of outcome",
            t64_base_diag_aggregate_is_complete(aggregate));
        failures += expect_true("genuine fail completes execution but never passes suite",
            t64_base_diag_aggregate_execution_complete(aggregate) == (index <= 1u) &&
            t64_base_diag_aggregate_is_all_pass(aggregate) == (index == 0u));
        failures += expect_status("each outcome class has a C classification",
            t64_base_diag_aggregate_report(aggregate, reports[0], 65536u,
                                           &report_lengths[0]), T64_BASE_DIAG_OK);
      }
      t64_base_diag_aggregate_destroy(aggregate);
    }
    t64_base_diag_inventory_destroy(one);
    free(fail_original);
  }
cleanup:
  free(records);
  free(reports[0]);
  free(reports[1]);
  return failures;
}

int main(void) {
  static const uint8_t original[] =
      "{\"records\":[{\"event\":\"start\",\"registration_index\":0,\"value_index\":0},"
      "{\"event\":\"outcome\",\"status\":\"pass\"},"
      "{\"complete\":true,\"event\":\"done\",\"record_count\":\"3\"}],"
      "\"schema\":\"t64.cpu-run\",\"schema_version\":1}\n";
  char original_sha256[65];
  char record[512];
  size_t record_length = 0u;
  int failures = 0;
  t64_base_diag_inventory *parsed = NULL;
  t64_base_diag_aggregate *aggregate = NULL;
  t64_base_diag_status status;
  record[0] = '\0';

  status = t64_base_diag_inventory_parse(
      source_authority, sizeof source_authority - 1u,
      source_authority_sha256, inventory, sizeof inventory - 1u, &parsed);
  failures += expect_status("valid source-derived inventory accepted", status,
                            T64_BASE_DIAG_OK);
  if (status != T64_BASE_DIAG_OK || parsed == NULL) {
    t64_base_diag_inventory_destroy(parsed);
    return failures == 0 ? 1 : failures;
  }

  failures += expect_true("all source-ordered BASE members remain present",
                          t64_base_diag_inventory_member_count(parsed) == 4u);
  {
    t64_base_diag_member member;
    failures += expect_true("empty values execute once at their source ordinal",
        t64_base_diag_inventory_member_at(parsed, 0u, &member) &&
        member.registration_index == 0u && member.value_index == 0u &&
        member.execution != NULL && strcmp(member.execution, "once") == 0 &&
        member.value_id != NULL && member.value_id[0] == '\0');
    failures += expect_true("blocked non-CPU member remains required",
        t64_base_diag_inventory_member_at(parsed, 1u, &member) &&
        member.registration_index == 1u && member.required &&
        member.group != NULL && strcmp(member.group, "rdp") == 0 &&
        member.execution != NULL && strcmp(member.execution, "blocked") == 0 &&
        member.reason != NULL && member.reason[0] != '\0');
    failures += expect_true("duplicate display names retain source ordinals",
        t64_base_diag_inventory_member_at(parsed, 2u, &member) &&
        member.registration_index == 2u && member.name != NULL &&
        strcmp(member.name, "same display") == 0 &&
        t64_base_diag_inventory_member_at(parsed, 3u, &member) &&
        member.registration_index == 3u && member.name != NULL &&
        strcmp(member.name, "same display") == 0);
  }

  {
    t64_base_diag_inventory *reduced = NULL;
    failures += expect_status("reduced and resealed source denied by full authority",
        t64_base_diag_inventory_parse(
            reduced_source_authority, sizeof reduced_source_authority - 1u,
            source_authority_sha256, reduced_inventory,
            sizeof reduced_inventory - 1u, &reduced),
        T64_BASE_DIAG_IDENTITY_MISMATCH);
    t64_base_diag_inventory_destroy(reduced);
  }

  failures += expect_status("aggregate creates from full inventory",
      t64_base_diag_aggregate_create(parsed, &aggregate), T64_BASE_DIAG_OK);
  if (aggregate != NULL) {
    print_hash(original, sizeof original - 1u, original_sha256);
    failures += expect_status("production record encoder emits a member",
        t64_base_diag_encode_member_record(
            parsed, 0u, 0u, T64_BASE_DIAG_OUTCOME_PASS, original,
            sizeof original - 1u, (uint8_t *)record, sizeof record,
            &record_length), T64_BASE_DIAG_OK);
    failures += expect_true("production record uses the exact inventory key",
        strstr(record, "\"inventory_sha256\":") != NULL &&
        strstr(record, "\"inventory_sha25\":") == NULL);
    if (record_length > 0u && record_length < sizeof record) {
      failures += expect_status("original member is authenticated and added",
          t64_base_diag_aggregate_add_original(
              aggregate, (const uint8_t *)record, record_length,
              original, sizeof original - 1u), T64_BASE_DIAG_OK);
      failures += expect_true("one observed member does not complete BASE",
          t64_base_diag_aggregate_member_count(aggregate) == 1u &&
          !t64_base_diag_aggregate_is_complete(aggregate) &&
          !t64_base_diag_aggregate_is_all_pass(aggregate));
      {
        t64_base_diag_member_result result;
        failures += expect_true("aggregate retains the exact original digest",
            t64_base_diag_aggregate_member_at(aggregate, 0u, &result) &&
            result.registration_index == 0u && result.value_index == 0u &&
            result.outcome == T64_BASE_DIAG_OUTCOME_PASS &&
            result.original_sha256 != NULL &&
            strcmp(result.original_sha256, original_sha256) == 0);
      }
      failures += expect_status("duplicate member poisons the aggregate",
          t64_base_diag_aggregate_add_original(
              aggregate, (const uint8_t *)record, record_length,
              original, sizeof original - 1u), T64_BASE_DIAG_DUPLICATE_MEMBER);
      failures += expect_true("poisoned duplicate cannot complete BASE",
          !t64_base_diag_aggregate_is_complete(aggregate) &&
          !t64_base_diag_aggregate_is_all_pass(aggregate));
    }
  }

  {
    static const uint8_t changed_original[] =
        "PASS authored member output changed\n";
    t64_base_diag_aggregate *tampered = NULL;
    if (t64_base_diag_aggregate_create(parsed, &tampered) !=
        T64_BASE_DIAG_OK) {
      failures += expect_true("aggregate available for original-byte control",
                              false);
    } else if (record[0] != '\0') {
      failures += expect_status("changed original bytes are rejected",
          t64_base_diag_aggregate_add_original(
              tampered, (const uint8_t *)record, record_length,
              changed_original, sizeof changed_original - 1u),
          T64_BASE_DIAG_IDENTITY_MISMATCH);
      failures += expect_true("tampered original cannot complete BASE",
                              !t64_base_diag_aggregate_is_complete(tampered));
    }
    t64_base_diag_aggregate_destroy(tampered);
  }

  t64_base_diag_aggregate_destroy(aggregate);
  {
    static const uint8_t forged_pass[] = "not an authenticated guest verdict\n";
    t64_base_diag_aggregate *forged = NULL;
    size_t forged_length = 0u;
    failures += expect_status("aggregate available for forged-pass control",
        t64_base_diag_aggregate_create(parsed, &forged), T64_BASE_DIAG_OK);
    failures += expect_status("forged-pass record has a matching original hash",
        t64_base_diag_encode_member_record(parsed, 0u, 0u,
            T64_BASE_DIAG_OUTCOME_PASS, forged_pass, sizeof forged_pass - 1u,
            (uint8_t *)record, sizeof record, &forged_length), T64_BASE_DIAG_OK);
    if (forged != NULL && forged_length != 0u) {
      failures += expect_status("matching bytes alone cannot authenticate a pass",
          t64_base_diag_aggregate_add_original(forged,
              (const uint8_t *)record, forged_length,
              forged_pass, sizeof forged_pass - 1u), T64_BASE_DIAG_INVALID_RECORD);
    }
    t64_base_diag_aggregate_destroy(forged);
  }
  failures += test_bound_records(parsed, original, sizeof original - 1u);
  t64_base_diag_inventory_destroy(parsed);
  if (failures != 0) return failures;
  (void)printf("BASE source authority and original-record controls PASS\n");
  return 0;
}
