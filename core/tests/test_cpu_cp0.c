#include "t64_internal.h"
#include "t64_test.h"

#include <stdint.h>
#include <stdio.h>

#define CP0_INDEX UINT32_C(0)
#define CP0_KERNEL_MTC0 UINT32_C(4)

static uint32_t encode_cp0(uint32_t operation, uint32_t rt, uint32_t rd) {
  return (UINT32_C(0x10) << 26) | (operation << 21) | (rt << 16) |
         (rd << 11);
}

static void test_index_write_preserves_probe_and_accepts_low_six_bits(void) {
  t64_machine machine = {0};
  t64_cp0_effect effect = {0};
  t64_cp0_transfer_result result;

  machine.profile.machine_model_version = UINT32_C(3);
  machine.profile.state_serialization_version = UINT32_C(9);
  machine.cpu.cp0[CP0_INDEX] = UINT32_C(0x80000000);
  machine.cpu.gpr[8] = UINT64_C(0xffffffff);

  result = t64_cp0_prepare_transfer(
      &machine, encode_cp0(CP0_KERNEL_MTC0, UINT32_C(8), CP0_INDEX), &effect);
  if (result == T64_CP0_TRANSFER_OK) {
    t64_cp0_commit_transfer(&machine, &effect);
  }
  (void)printf("# Index MTC0 expected=0x8000003f result=%u actual=0x%016llx\n",
               (unsigned)result,
               (unsigned long long)machine.cpu.cp0[CP0_INDEX]);
  T64_CHECK(result == T64_CP0_TRANSFER_OK);
  T64_CHECK_INT(machine.cpu.cp0[CP0_INDEX], UINT32_C(0x8000003f));
}

int main(void) {
  test_index_write_preserves_probe_and_accepts_low_six_bits();
  return T64_TEST_FINISH("unit.core.cpu-cp0");
}
