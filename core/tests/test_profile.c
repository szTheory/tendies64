#line 1 "core/tests/test_profile.c"
#include <stddef.h>
#include <stdint.h>

#include "t64_test.h"
#include "tendies64/t64_machine.h"

static void test_profile_serialization_is_available(void) {
  const t64_profile_descriptor *profile = NULL;
  uint8_t serialized[T64_PROFILE_SERIALIZED_MAX_BYTES];
  size_t serialized_length = 0u;
  t64_status status;

  T64_CHECK_INT(t64_profile_lookup("ntsc-cart-4m-skeleton", &profile),
                T64_STATUS_OK);
  T64_CHECK(profile != NULL);
  if (profile == NULL) {
    return;
  }
  status = t64_profile_serialize(profile, serialized, sizeof serialized,
                                 &serialized_length);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK(serialized_length > 0u);
}

int main(void) {
  test_profile_serialization_is_available();
  return T64_TEST_FINISH("unit.core.profile");
}
