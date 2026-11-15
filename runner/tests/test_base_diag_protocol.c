#include "t64_base_diag_protocol.h"

#include <stdio.h>
#include <string.h>

#include "t64_sha256.h"

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

int main(void) {
  static const uint8_t original[] = "PASS authored member output\n";
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
  t64_base_diag_inventory_destroy(parsed);
  if (failures != 0) return failures;
  (void)printf("BASE source authority and original-record controls PASS\n");
  return 0;
}
