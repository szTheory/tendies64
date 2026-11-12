/* Temporary interface stub for the first executable RED. */
#include "t64_cpu_diag_protocol.h"

t64_cpu_diag_status t64_cpu_diag_inventory_parse(
    const uint8_t *bytes, size_t length,
    t64_cpu_diag_inventory **inventory_out) {
  (void)bytes;
  (void)length;
  (void)inventory_out;
  return T64_CPU_DIAG_NOT_IMPLEMENTED;
}
