#include "t64_internal.h"
#include "t64_test.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_existing_conversion_and_missing_binary32_add(void) {
  t64_softfloat_context context = {0u, T64_SOFTFLOAT_TININESS_AFTER_ROUNDING,
                                   0u};
  t64_softfloat_result conversion = {UINT64_C(0x1122334455667788), UINT8_C(0xff)};
  t64_softfloat_result sum = {UINT64_C(0x1122334455667788), UINT8_C(0xff)};
  const uint64_t expected_conversion = UINT64_C(0xc1e0000000000000);
  const uint32_t expected_sum = UINT32_C(0x40400000); /* 1.5 * 2^1 */
  const uint32_t a = UINT32_C(0x3f800000); /* 1 = 1 * 2^0 */
  const uint32_t b = UINT32_C(0x40000000); /* 2 = 1 * 2^1 */
  t64_status conversion_status = t64_softfloat_execute(
      T64_SOFTFLOAT_OP_I32_TO_F64, &context, UINT32_C(0x80000000), 0u,
      &conversion);
  t64_status add_status = t64_softfloat_execute(
      T64_SOFTFLOAT_OP_F32_ADD, &context, a, b, &sum);

  (void)printf("# existing i32->f64 expected=%016llx observed=%016llx status=%u\n",
               (unsigned long long)expected_conversion,
               (unsigned long long)conversion.bits, conversion_status);
  T64_CHECK_INT(conversion_status, T64_STATUS_OK);
  T64_CHECK_INT(conversion.bits, expected_conversion);
  T64_CHECK_INT(conversion.exception_flags, 0u);
  (void)printf("# missing f32 add expected=%08x observed=%08x status=%u flags=%02x\n",
               expected_sum, (uint32_t)sum.bits, add_status,
               (unsigned int)sum.exception_flags);
  T64_CHECK_INT(add_status, T64_STATUS_OK);
  T64_CHECK_INT(sum.bits, expected_sum);
  T64_CHECK_INT(sum.exception_flags, 0u);
}

int main(int argc, char **argv) {
  if (argc != 2 || strcmp(argv[1], "--softfloat32") != 0) {
    (void)fprintf(stderr, "usage: test_softfloat --softfloat32\n");
    return 2;
  }
  test_existing_conversion_and_missing_binary32_add();
  return T64_TEST_FINISH("unit.core.softfloat32");
}
