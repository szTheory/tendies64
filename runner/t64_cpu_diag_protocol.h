/* Closed local CPU diagnostic protocol. */
#ifndef T64_CPU_DIAG_PROTOCOL_H
#define T64_CPU_DIAG_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
  T64_CPU_DIAG_OK = 0,
  T64_CPU_DIAG_NOT_IMPLEMENTED
} t64_cpu_diag_status;

typedef struct t64_cpu_diag_inventory t64_cpu_diag_inventory;

t64_cpu_diag_status t64_cpu_diag_inventory_parse(
    const uint8_t *bytes, size_t length,
    t64_cpu_diag_inventory **inventory_out);

#endif /* T64_CPU_DIAG_PROTOCOL_H */
