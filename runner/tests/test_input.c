/* TAP checks for the host-side logical-input schedule parser. */
#include "t64_input.h"

#include <stdio.h>
#include <string.h>

static unsigned tap_count;
static unsigned tap_failures;

static void check_status(t64_input_status actual, t64_input_status expected,
                         const char *name) {
  tap_count++;
  if (actual == expected) {
    (void)printf("ok %u - %s\n", tap_count, name);
  } else {
    tap_failures++;
    (void)printf("not ok %u - %s\n", tap_count, name);
    (void)printf("# expected status %u, observed %u\n", (unsigned)expected,
                 (unsigned)actual);
  }
}

int main(void) {
  static const uint8_t empty_schedule[] =
      "{\"events\":[],\"schema\":\"t64.input\",\"schema_version\":1}\n";
  t64_input_schedule *schedule = NULL;
  t64_input_error error = {T64_INPUT_INVALID_ARGUMENT, 0u};
  t64_input_status status;

  (void)puts("TAP version 13");
  (void)memset(&error, 0, sizeof error);
  status = t64_input_parse(empty_schedule, sizeof empty_schedule - 1u,
                           &schedule, &error);
  check_status(status, T64_INPUT_OK,
               "accepts an empty canonical t64.input v1 schedule");
  t64_input_schedule_destroy(schedule);
  (void)printf("1..%u\n", tap_count);
  return tap_count > 0u && tap_failures == 0u ? 0 : 1;
}
