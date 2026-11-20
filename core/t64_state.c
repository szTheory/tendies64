#include "t64_internal.h"

#include <stddef.h>
#include <stdint.h>

/* RED scaffold (plan 09-08 Task 1 step 2): the export is not implemented. */
t64_status t64_machine_state_export(const t64_machine *machine,
                                    uint8_t *bytes_out, size_t capacity,
                                    size_t *length_out) {
  (void)machine;
  (void)bytes_out;
  (void)capacity;
  (void)length_out;
  return T64_STATUS_INVALID_ARGUMENT;
}
