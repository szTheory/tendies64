#include "t64_cpu_diag_protocol.h"

#include <stdio.h>

static const uint8_t inventory_json[] =
    "{\"identity\":{\"base_rom_sha256\":\"0000000000000000000000000000000000000000000000000000000000000000\",\"core_build_sha256\":\"1111111111111111111111111111111111111111111111111111111111111111\",\"overlay_sha256\":\"2222222222222222222222222222222222222222222222222222222222222222\",\"profile_sha256\":\"3333333333333333333333333333333333333333333333333333333333333333\",\"selected_rom_sha256\":\"4444444444444444444444444444444444444444444444444444444444444444\",\"selector\":\"authored-selector\",\"source_revision\":\"5555555555555555555555555555555555555555\",\"tool_receipt_sha256\":\"6666666666666666666666666666666666666666666666666666666666666666\"},\"registrations\":[{\"execution\":\"values\",\"group\":\"arithmetic\",\"index\":0,\"lane\":\"required-base\",\"level\":\"BasicFunctionality\",\"name\":\"authored.case\",\"reason\":\"required\",\"values\":[{\"id\":\"case.one\",\"index\":0}]}],\"schema\":\"t64.cpu-inventory\",\"schema_version\":1}\n";

int main(void) {
  t64_cpu_diag_inventory *inventory = NULL;
  t64_cpu_diag_status status = t64_cpu_diag_inventory_parse(
      inventory_json, sizeof inventory_json - 1u, &inventory);
  if (status != T64_CPU_DIAG_OK) {
    (void)fprintf(stderr,
                  "CPU diagnostic RED: inventory parse expected ok, observed not-implemented\n");
    return 1;
  }
  (void)fprintf(stderr, "unexpected implementation before RED assertion\n");
  return 2;
}
