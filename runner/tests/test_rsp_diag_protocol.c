#include "t64_rsp_diag_protocol.h"

#include <stdio.h>
#include <string.h>

static const char inventory_component_rsp[] =
    "{\"component\":\"rsp\",\"declared_counts\":{\"registrations\":1,\"required_values\":1,\"values\":1},"
    "\"identity\":{\"boot_sha256\":\"0000000000000000000000000000000000000000000000000000000000000000\","
    "\"closure_sha256\":\"1111111111111111111111111111111111111111111111111111111111111111\","
    "\"core_build_sha256\":\"2222222222222222222222222222222222222222222222222222222222222222\","
    "\"overlay_sha256\":\"3333333333333333333333333333333333333333333333333333333333333333\","
    "\"profile_descriptor_sha256\":\"4444444444444444444444444444444444444444444444444444444444444444\","
    "\"profile_id\":\"ntsc-cart-4m-openipl3-rsp-functional-v1\","
    "\"profile_sha256\":\"5555555555555555555555555555555555555555555555555555555555555555\","
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

int main(int argc, char **argv) {
  if (argc != 2 || strcmp(argv[1], "--protocol") != 0) return 2;
  test_inventory_component_boundary();
  if (failures != 0u) return 1;
  (void)puts("RSP diagnostic protocol controls PASS");
  return 0;
}
