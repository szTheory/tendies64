#include "t64_base_diag_protocol.h"

#include <stdio.h>
#include <string.h>

#include "t64_sha256.h"

static const char source_authority_sha256[] =
    "1ddce1bc2cc87c6da3418b438201604bdb0d6223218e572f314cf47d5322cfb0";

static const uint8_t source_authority[] =
    "{\"members\":["
    "{\"execution\":\"once\",\"group\":\"arithmetic\",\"index\":0,\"lane\":\"base-required\",\"level\":\"BasicFunctionality\",\"name\":\"empty selector\",\"reason\":\"\",\"required\":true,\"value_id\":\"\",\"value_index\":0},"
    "{\"execution\":\"blocked\",\"group\":\"rdp\",\"index\":1,\"lane\":\"base-required\",\"level\":\"RDPBasic\",\"name\":\"blocked asset\",\"reason\":\"asset not cleared\",\"required\":true,\"value_id\":\"blocked.asset\",\"value_index\":0},"
    "{\"execution\":\"values\",\"group\":\"arithmetic\",\"index\":2,\"lane\":\"base-required\",\"level\":\"Weird\",\"name\":\"same display\",\"reason\":\"\",\"required\":true,\"value_id\":\"first\",\"value_index\":0},"
    "{\"execution\":\"values\",\"group\":\"arithmetic\",\"index\":3,\"lane\":\"base-required\",\"level\":\"Weird\",\"name\":\"same display\",\"reason\":\"\",\"required\":true,\"value_id\":\"second\",\"value_index\":0}],"
    "\"schema\":\"t64.base-source-authority\",\"schema_version\":1,"
    "\"source_revision\":\"0000000000000000000000000000000000000000\"}";

static const uint8_t inventory[] =
    "{\"members\":["
    "{\"execution\":\"once\",\"group\":\"arithmetic\",\"index\":0,\"lane\":\"base-required\",\"level\":\"BasicFunctionality\",\"name\":\"empty selector\",\"reason\":\"\",\"required\":true,\"value_id\":\"\",\"value_index\":0},"
    "{\"execution\":\"blocked\",\"group\":\"rdp\",\"index\":1,\"lane\":\"base-required\",\"level\":\"RDPBasic\",\"name\":\"blocked asset\",\"reason\":\"asset not cleared\",\"required\":true,\"value_id\":\"blocked.asset\",\"value_index\":0},"
    "{\"execution\":\"values\",\"group\":\"arithmetic\",\"index\":2,\"lane\":\"base-required\",\"level\":\"Weird\",\"name\":\"same display\",\"reason\":\"\",\"required\":true,\"value_id\":\"first\",\"value_index\":0},"
    "{\"execution\":\"values\",\"group\":\"arithmetic\",\"index\":3,\"lane\":\"base-required\",\"level\":\"Weird\",\"name\":\"same display\",\"reason\":\"\",\"required\":true,\"value_id\":\"second\",\"value_index\":0}],"
    "\"schema\":\"t64.base-inventory\",\"schema_version\":1,"
    "\"source_authority_sha256\":\"1ddce1bc2cc87c6da3418b438201604bdb0d6223218e572f314cf47d5322cfb0\","
    "\"source_revision\":\"0000000000000000000000000000000000000000\"}";

static const uint8_t reduced_source_authority[] =
    "{\"members\":["
    "{\"execution\":\"once\",\"group\":\"arithmetic\",\"index\":0,\"lane\":\"base-required\",\"level\":\"BasicFunctionality\",\"name\":\"empty selector\",\"reason\":\"\",\"required\":true,\"value_id\":\"\",\"value_index\":0},"
    "{\"execution\":\"values\",\"group\":\"arithmetic\",\"index\":2,\"lane\":\"base-required\",\"level\":\"Weird\",\"name\":\"same display\",\"reason\":\"\",\"required\":true,\"value_id\":\"first\",\"value_index\":0},"
    "{\"execution\":\"values\",\"group\":\"arithmetic\",\"index\":3,\"lane\":\"base-required\",\"level\":\"Weird\",\"name\":\"same display\",\"reason\":\"\",\"required\":true,\"value_id\":\"second\",\"value_index\":0}],"
    "\"schema\":\"t64.base-source-authority\",\"schema_version\":1,"
    "\"source_revision\":\"0000000000000000000000000000000000000000\"}";

static const uint8_t reduced_inventory[] =
    "{\"members\":["
    "{\"execution\":\"once\",\"group\":\"arithmetic\",\"index\":0,\"lane\":\"base-required\",\"level\":\"BasicFunctionality\",\"name\":\"empty selector\",\"reason\":\"\",\"required\":true,\"value_id\":\"\",\"value_index\":0},"
    "{\"execution\":\"values\",\"group\":\"arithmetic\",\"index\":2,\"lane\":\"base-required\",\"level\":\"Weird\",\"name\":\"same display\",\"reason\":\"\",\"required\":true,\"value_id\":\"first\",\"value_index\":0},"
    "{\"execution\":\"values\",\"group\":\"arithmetic\",\"index\":3,\"lane\":\"base-required\",\"level\":\"Weird\",\"name\":\"same display\",\"reason\":\"\",\"required\":true,\"value_id\":\"second\",\"value_index\":0}],"
    "\"schema\":\"t64.base-inventory\",\"schema_version\":1,"
    "\"source_authority_sha256\":\"cc16d1b780ffe003e4d62babcb5521c4ab81f5bd2782933c47519c3e993f4d6c\","
    "\"source_revision\":\"0000000000000000000000000000000000000000\"}";

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
  int record_length;
  int failures = 0;
  t64_base_diag_inventory *parsed = NULL;
  t64_base_diag_aggregate *aggregate = NULL;
  t64_base_diag_status status;

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
    record_length = snprintf(record, sizeof record,
        "{\"inventory_sha256\":\"%s\",\"original_sha256\":\"%s\","
        "\"outcome\":\"pass\",\"registration_index\":0,"
        "\"schema\":\"t64.base-member-record\",\"schema_version\":1,"
        "\"value_index\":0}",
        t64_base_diag_inventory_sha256(parsed), original_sha256);
    failures += expect_true("original record fits bounded buffer",
                            record_length > 0 &&
                            (size_t)record_length < sizeof record);
    if (record_length > 0 && (size_t)record_length < sizeof record) {
      failures += expect_status("original member is authenticated and added",
          t64_base_diag_aggregate_add_original(
              aggregate, (const uint8_t *)record, (size_t)record_length,
              original, sizeof original - 1u), T64_BASE_DIAG_OK);
      failures += expect_true("one observed member does not complete BASE",
          t64_base_diag_aggregate_member_count(aggregate) == 1u &&
          !t64_base_diag_aggregate_is_complete(aggregate) &&
          !t64_base_diag_aggregate_is_all_pass(aggregate));
    }
  }

  t64_base_diag_aggregate_destroy(aggregate);
  t64_base_diag_inventory_destroy(parsed);
  if (failures != 0) return failures;
  (void)printf("BASE source authority and original-record controls PASS\n");
  return 0;
}
