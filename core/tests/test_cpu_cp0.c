#include "t64_internal.h"
#include "t64_test.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define CP0_INDEX UINT32_C(0)
#define CP0_RANDOM UINT32_C(1)
#define CP0_ENTRYLO0 UINT32_C(2)
#define CP0_ENTRYLO1 UINT32_C(3)
#define CP0_CONTEXT UINT32_C(4)
#define CP0_PAGEMASK UINT32_C(5)
#define CP0_WIRED UINT32_C(6)
#define CP0_BADVADDR UINT32_C(8)
#define CP0_COUNT UINT32_C(9)
#define CP0_ENTRYHI UINT32_C(10)
#define CP0_COMPARE UINT32_C(11)
#define CP0_STATUS UINT32_C(12)
#define CP0_CAUSE UINT32_C(13)
#define CP0_EPC UINT32_C(14)
#define CP0_PRID UINT32_C(15)
#define CP0_CONFIG UINT32_C(16)
#define CP0_LLADDR UINT32_C(17)
#define CP0_WATCHLO UINT32_C(18)
#define CP0_WATCHHI UINT32_C(19)
#define CP0_XCONTEXT UINT32_C(20)
#define CP0_PERR UINT32_C(26)
#define CP0_CACHEERR UINT32_C(27)
#define CP0_TAGLO UINT32_C(28)
#define CP0_TAGHI UINT32_C(29)
#define CP0_ERROREPC UINT32_C(30)

#define CP0_MFC0 UINT32_C(0)
#define CP0_DMFC0 UINT32_C(1)
#define CP0_MTC0 UINT32_C(4)
#define CP0_DMTC0 UINT32_C(5)
#define STATUS_TS UINT32_C(0x00200000)
#define STATUS_CU0 UINT32_C(0x10000000)
#define STATUS_KSU_USER UINT32_C(0x00000010)
#define STATUS_KSU_SUPERVISOR UINT32_C(0x00000008)
#define STATUS_UX UINT32_C(0x00000020)
#define STATUS_SX UINT32_C(0x00000040)
#define CAUSE_SOFTWARE_IP UINT32_C(0x00000300)
#define CAUSE_TIMER_IP UINT32_C(0x00008000)
#define CONFIG_SEED UINT32_C(0x7006e463)

static void setup_vr4300_machine(t64_machine *machine) {
  *machine = (t64_machine){0};
  machine->profile.machine_model_version = UINT32_C(3);
  machine->profile.state_serialization_version = UINT32_C(9);
}

static void setup_vr4300_compat_machine(t64_machine *machine) {
  setup_vr4300_machine(machine);
  machine->profile.machine_model_version = UINT32_C(4);
  machine->profile.state_serialization_version = UINT32_C(10);
}

static void setup_vr4300_compat_v2_machine(t64_machine *machine) {
  setup_vr4300_machine(machine);
  machine->profile.machine_model_version = UINT32_C(5);
  machine->profile.state_serialization_version = UINT32_C(11);
}

static uint32_t encode_cp0(uint32_t operation, uint32_t rt, uint32_t rd) {
  return (UINT32_C(0x10) << 26) | (operation << 21) | (rt << 16) |
         (rd << 11);
}

static t64_cp0_transfer_result apply_cp0(t64_machine *machine,
                                         uint32_t operation, uint32_t rt,
                                         uint32_t rd) {
  t64_cp0_effect effect = {0};
  t64_cp0_transfer_result result = t64_cp0_prepare_transfer(
      machine, encode_cp0(operation, rt, rd), &effect);

  if (result == T64_CP0_TRANSFER_OK) {
    t64_cp0_commit_transfer(machine, &effect);
    if (effect.write_gpr && effect.gpr_index != 0u) {
      machine->cpu.gpr[effect.gpr_index] = effect.gpr_value;
    }
  }
  return result;
}

static t64_cp0_transfer_result apply_tlb_instruction(t64_machine *machine,
                                                     uint32_t function) {
  t64_cp0_effect effect = {0};
  uint32_t opcode = (UINT32_C(0x10) << 26) | (UINT32_C(0x10) << 21) |
                    function;
  t64_cp0_transfer_result result =
      t64_cp0_prepare_tlb_instruction(machine, opcode, &effect);

  if (result == T64_CP0_TRANSFER_OK) {
    t64_cp0_commit_transfer(machine, &effect);
  }
  return result;
}

static bool status_bit_is_software_writable(unsigned bit) {
  return bit <= 17u || bit == 18u || bit == 20u || bit == 22u ||
         (bit >= 24u && bit <= 26u) || bit >= 28u;
}

static void test_status_each_bit_set_and_clear(void) {
  t64_machine machine;
  setup_vr4300_machine(&machine);

  for (unsigned bit = 0u; bit < 32u; bit++) {
    uint32_t value = UINT32_C(1) << bit;
    bool writable = status_bit_is_software_writable(bit);
    uint32_t expected_set = STATUS_TS | (writable ? value : UINT32_C(0));

    machine.cpu.cp0[CP0_STATUS] = STATUS_TS;
    machine.cpu.gpr[8] = value;
    T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_STATUS) ==
              T64_CP0_TRANSFER_OK);
    T64_CHECK(machine.cpu.cp0[CP0_STATUS] == expected_set);

    machine.cpu.cp0[CP0_STATUS] = expected_set;
    machine.cpu.gpr[8] = UINT64_C(0);
    T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_STATUS) ==
              T64_CP0_TRANSFER_OK);
    T64_CHECK(machine.cpu.cp0[CP0_STATUS] == STATUS_TS);
  }

  machine.cpu.cp0[CP0_STATUS] = UINT32_C(0x90200001);
  T64_CHECK(apply_cp0(&machine, CP0_MFC0, 10u, CP0_STATUS) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.gpr[10] == UINT64_C(0xffffffff90200001));
}

static void test_index_random_and_wired_fields(void) {
  t64_machine machine;
  setup_vr4300_machine(&machine);
  machine.cpu.cp0[CP0_INDEX] = UINT32_C(0x80000000);
  machine.cpu.gpr[8] = UINT64_C(0xffffffff);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_INDEX) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_INDEX] == UINT64_C(0x8000003f));
  machine.cpu.gpr[8] = UINT64_C(0);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_INDEX) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_INDEX] == UINT64_C(0x80000000));
  T64_CHECK(apply_cp0(&machine, CP0_MFC0, 9u, CP0_INDEX) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.gpr[9] == UINT64_C(0xffffffff80000000));

  machine.cpu.cp0[CP0_RANDOM] = UINT32_C(31);
  machine.cpu.gpr[8] = UINT64_C(0xffffffff);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_RANDOM) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_RANDOM] == UINT64_C(0x3f));
  T64_CHECK(apply_cp0(&machine, CP0_MFC0, 9u, CP0_RANDOM) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.gpr[9] == UINT64_C(0x3f));
  machine.cpu.gpr[8] = UINT64_C(0);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_RANDOM) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_RANDOM] == UINT64_C(31));

  machine.cpu.cp0[CP0_WIRED] = UINT64_C(0);
  machine.cpu.gpr[8] = UINT64_C(0x23);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_WIRED) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_WIRED] == UINT64_C(0x23));
  T64_CHECK(machine.cpu.cp0[CP0_RANDOM] == UINT64_C(31));
  T64_CHECK(apply_cp0(&machine, CP0_MFC0, 9u, CP0_WIRED) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.gpr[9] == UINT64_C(0x23));
  machine.cpu.gpr[8] = UINT64_C(0);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_WIRED) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_WIRED] == UINT64_C(0));
  T64_CHECK(machine.cpu.cp0[CP0_RANDOM] == UINT64_C(31));

  machine.cpu.cp0[CP0_RANDOM] = UINT64_C(0x2b);
  T64_CHECK_INT(t64_cp0_random_tlb_index(&machine), 11);
  t64_cp0_instruction_retired(&machine);
  T64_CHECK(machine.cpu.cp0[CP0_RANDOM] == UINT64_C(0x2a));
  machine.cpu.cp0[CP0_WIRED] = UINT64_C(3);
  machine.cpu.cp0[CP0_RANDOM] = UINT64_C(4);
  t64_cp0_instruction_retired(&machine);
  T64_CHECK(machine.cpu.cp0[CP0_RANDOM] == UINT64_C(3));
  t64_cp0_instruction_retired(&machine);
  T64_CHECK(machine.cpu.cp0[CP0_RANDOM] == UINT64_C(31));
  machine.cpu.cp0[CP0_RANDOM] = UINT64_C(0x3f);
  machine.cpu.cp0[CP0_WIRED] = UINT64_C(31);
  t64_cp0_instruction_retired(&machine);
  T64_CHECK(machine.cpu.cp0[CP0_RANDOM] == UINT64_C(0x3f));
}

static void test_entrylo_context_pagemask(void) {
  static const uint32_t legal_page_masks[] = {
      UINT32_C(0x00000000), UINT32_C(0x00006000), UINT32_C(0x0001e000),
      UINT32_C(0x0007e000), UINT32_C(0x001fe000), UINT32_C(0x007fe000),
      UINT32_C(0x01ffe000)};
  static const uint32_t illegal_page_masks[] = {
      UINT32_C(0x00002000), UINT32_C(0x0000e000), UINT32_C(0x0003e000)};
  t64_machine machine;
  setup_vr4300_machine(&machine);

  for (uint32_t rd = CP0_ENTRYLO0; rd <= CP0_ENTRYLO1; rd++) {
    machine.cpu.gpr[8] = UINT64_C(0xffffffff);
    T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, rd) ==
              T64_CP0_TRANSFER_OK);
    T64_CHECK(machine.cpu.cp0[rd] == UINT64_C(0x03ffffff));
    T64_CHECK(apply_cp0(&machine, CP0_MFC0, 9u, rd) ==
              T64_CP0_TRANSFER_OK);
    T64_CHECK(machine.cpu.gpr[9] == UINT64_C(0x03ffffff));
    machine.cpu.gpr[8] = UINT64_C(0);
    T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, rd) ==
              T64_CP0_TRANSFER_OK);
    T64_CHECK(machine.cpu.cp0[rd] == UINT64_C(0));
  }

  machine.cpu.cp0[CP0_CONTEXT] = UINT64_C(0xabcdef0000012340);
  machine.cpu.gpr[8] = UINT64_C(0xffffffff);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_CONTEXT) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_CONTEXT] ==
            UINT64_C(0xabcdef00ff812340));
  T64_CHECK(apply_cp0(&machine, CP0_MFC0, 9u, CP0_CONTEXT) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.gpr[9] == UINT64_C(0xffffffff80012340));
  machine.cpu.gpr[8] = UINT64_MAX;
  T64_CHECK(apply_cp0(&machine, CP0_DMTC0, 8u, CP0_CONTEXT) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_CONTEXT] ==
            UINT64_C(0xffffffff80012340));
  machine.cpu.gpr[8] = UINT64_C(0);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_CONTEXT) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_CONTEXT] ==
            UINT64_C(0xffffffff00012340));

  for (size_t i = 0u;
       i < sizeof legal_page_masks / sizeof legal_page_masks[0]; i++) {
    machine.cpu.gpr[8] = legal_page_masks[i];
    T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_PAGEMASK) ==
              T64_CP0_TRANSFER_OK);
    T64_CHECK(machine.cpu.cp0[CP0_PAGEMASK] == legal_page_masks[i]);
    T64_CHECK(apply_cp0(&machine, CP0_MFC0, 9u, CP0_PAGEMASK) ==
              T64_CP0_TRANSFER_OK);
    T64_CHECK(machine.cpu.gpr[9] == legal_page_masks[i]);
  }
  for (size_t i = 0u;
       i < sizeof illegal_page_masks / sizeof illegal_page_masks[0]; i++) {
    uint64_t before = machine.cpu.cp0[CP0_PAGEMASK];
    machine.cpu.gpr[8] = illegal_page_masks[i];
    T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_PAGEMASK) ==
              T64_CP0_TRANSFER_UNSUPPORTED);
    T64_CHECK(machine.cpu.cp0[CP0_PAGEMASK] == before);
  }
}

static void test_readonly_and_word_registers(void) {
  t64_machine machine;
  setup_vr4300_machine(&machine);

  machine.cpu.cp0[CP0_BADVADDR] = UINT64_C(0x0123456789abcdef);
  machine.cpu.gpr[8] = UINT64_MAX;
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_BADVADDR) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_BADVADDR] == UINT64_C(0x0123456789abcdef));
  T64_CHECK(apply_cp0(&machine, CP0_MFC0, 9u, CP0_BADVADDR) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.gpr[9] == UINT64_C(0xffffffff89abcdef));
  T64_CHECK(apply_cp0(&machine, CP0_DMFC0, 10u, CP0_BADVADDR) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.gpr[10] == UINT64_C(0x0123456789abcdef));
  T64_CHECK(apply_cp0(&machine, CP0_DMTC0, 8u, CP0_BADVADDR) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_BADVADDR] ==
            UINT64_C(0x0123456789abcdef));

  machine.cpu.cp0[CP0_COUNT] = UINT64_C(0x12345678);
  machine.cpu.gpr[8] = UINT64_C(0xdeadbeef87654321);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_COUNT) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_COUNT] == UINT64_C(0x87654321));
  T64_CHECK(apply_cp0(&machine, CP0_MFC0, 9u, CP0_COUNT) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.gpr[9] == UINT64_C(0xffffffff87654321));
  T64_CHECK(apply_cp0(&machine, CP0_DMFC0, 9u, CP0_COUNT) ==
            T64_CP0_TRANSFER_UNSUPPORTED);
  machine.cpu.gpr[8] = UINT64_C(0);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_COUNT) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_COUNT] == UINT64_C(0));

  machine.cpu.cp0[CP0_PRID] = UINT64_C(0xffffffff);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_PRID) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_PRID] == UINT64_C(0xffffffff));
  T64_CHECK(apply_cp0(&machine, CP0_MFC0, 9u, CP0_PRID) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.gpr[9] == UINT64_C(0x0000000000000b00));
  T64_CHECK(apply_cp0(&machine, CP0_DMFC0, 9u, CP0_PRID) ==
            T64_CP0_TRANSFER_UNSUPPORTED);

  machine.cpu.cp0[CP0_LLADDR] = UINT64_C(0);
  machine.cpu.linked_valid = true;
  machine.cpu.linked_physical_address = UINT32_C(0x12345670);
  machine.cpu.gpr[8] = UINT64_MAX;
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_LLADDR) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_LLADDR] == UINT64_C(0x0fffffff));
  T64_CHECK(apply_cp0(&machine, CP0_MFC0, 9u, CP0_LLADDR) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.gpr[9] == UINT64_C(0x0fffffff));
  T64_CHECK(machine.cpu.linked_valid);
  T64_CHECK(machine.cpu.linked_physical_address == UINT32_C(0x12345670));
  machine.cpu.gpr[8] = UINT64_C(0);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_LLADDR) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_LLADDR] == UINT64_C(0));

  machine.cpu.gpr[8] = UINT64_MAX;
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_WATCHLO) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_WATCHLO] == UINT64_C(0xfffffffb));
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_WATCHHI) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_WATCHHI] == UINT64_C(0x0f));
  T64_CHECK(apply_cp0(&machine, CP0_MFC0, 9u, CP0_WATCHHI) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.gpr[9] == UINT64_C(0x0f));
  T64_CHECK(apply_cp0(&machine, CP0_MFC0, 9u, CP0_WATCHLO) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.gpr[9] == UINT64_C(0xfffffffffffffffb));
  machine.cpu.gpr[8] = UINT64_C(0);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_WATCHLO) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_WATCHLO] == UINT64_C(0));
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_WATCHHI) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_WATCHHI] == UINT64_C(0));
  machine.cpu.gpr[8] = UINT64_MAX;
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_PERR) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_PERR] == UINT64_C(0xff));
  T64_CHECK(apply_cp0(&machine, CP0_MFC0, 9u, CP0_PERR) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.gpr[9] == UINT64_C(0xff));
  machine.cpu.gpr[8] = UINT64_C(0);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_PERR) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_PERR] == UINT64_C(0));
  machine.cpu.gpr[8] = UINT64_MAX;
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_TAGLO) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_TAGLO] == UINT64_C(0x0fffffc0));
  T64_CHECK(apply_cp0(&machine, CP0_MFC0, 9u, CP0_TAGLO) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.gpr[9] == UINT64_C(0x0fffffc0));
  machine.cpu.gpr[8] = UINT64_C(0);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_TAGLO) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_TAGLO] == UINT64_C(0));
  machine.cpu.gpr[8] = UINT64_MAX;
  machine.cpu.cp0[CP0_TAGHI] = UINT64_C(0xffffffff);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_TAGHI) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_TAGHI] == UINT64_C(0));
  T64_CHECK(apply_cp0(&machine, CP0_MFC0, 9u, CP0_TAGHI) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.gpr[9] == UINT64_C(0));
  machine.cpu.cp0[CP0_CACHEERR] = UINT64_C(0xffffffff);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_CACHEERR) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_CACHEERR] == UINT64_C(0xffffffff));
  T64_CHECK(apply_cp0(&machine, CP0_MFC0, 9u, CP0_CACHEERR) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.gpr[9] == UINT64_C(0));
}

static void test_entryhi_cause_compare_and_return_pc(void) {
  static const uint32_t return_pc_registers[] = {CP0_EPC, CP0_ERROREPC};
  t64_machine machine;
  setup_vr4300_machine(&machine);

  machine.cpu.cp0[CP0_ENTRYHI] = UINT64_C(0xc00000ab00000000);
  machine.cpu.gpr[8] = UINT64_MAX;
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_ENTRYHI) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_ENTRYHI] ==
            UINT64_C(0xc00000abffffe0ff));
  T64_CHECK(apply_cp0(&machine, CP0_MFC0, 9u, CP0_ENTRYHI) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.gpr[9] == UINT64_C(0xffffffffffffe0ff));
  machine.cpu.gpr[8] = UINT64_C(0);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_ENTRYHI) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_ENTRYHI] ==
            UINT64_C(0xc00000ab00000000));
  machine.cpu.gpr[8] = UINT64_MAX;
  T64_CHECK(apply_cp0(&machine, CP0_DMTC0, 8u, CP0_ENTRYHI) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_ENTRYHI] ==
            UINT64_C(0xc00000ffffffe0ff));
  T64_CHECK(apply_cp0(&machine, CP0_DMFC0, 9u, CP0_ENTRYHI) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.gpr[9] == UINT64_C(0xc00000ffffffe0ff));

  machine.cpu.cp0[CP0_CAUSE] = UINT64_C(0xffffffff);
  machine.cpu.gpr[8] = UINT64_MAX;
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_CAUSE) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_CAUSE] == UINT64_C(0xb000ff7c));
  machine.cpu.gpr[8] = UINT64_C(0);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_CAUSE) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_CAUSE] == UINT64_C(0xb000fc7c));
  T64_CHECK(apply_cp0(&machine, CP0_MFC0, 9u, CP0_CAUSE) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.gpr[9] == UINT64_C(0xffffffffb000fc7c));

  machine.cpu.cp0[CP0_CAUSE] = UINT64_C(0xa5a58108);
  machine.cpu.cp0[CP0_COMPARE] = UINT64_C(0x12345678);
  machine.cpu.gpr[8] = UINT64_C(0xdeadbeef87654321);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_COMPARE) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_COMPARE] == UINT64_C(0x87654321));
  T64_CHECK(machine.cpu.cp0[CP0_CAUSE] == UINT64_C(0xa5a50108));
  T64_CHECK(apply_cp0(&machine, CP0_MFC0, 9u, CP0_COMPARE) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.gpr[9] == UINT64_C(0xffffffff87654321));
  machine.cpu.cp0[CP0_CAUSE] |= CAUSE_TIMER_IP;
  machine.cpu.gpr[8] = UINT64_C(0);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_COMPARE) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_COMPARE] == UINT64_C(0));
  T64_CHECK((machine.cpu.cp0[CP0_CAUSE] & CAUSE_TIMER_IP) == 0u);

  for (size_t i = 0u;
       i < sizeof return_pc_registers / sizeof return_pc_registers[0]; i++) {
    uint32_t rd = return_pc_registers[i];
    machine.cpu.cp0[rd] = UINT64_C(0x1122334455667788);
    machine.cpu.gpr[8] = UINT64_C(0xdeadbeef80000001);
    T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, rd) ==
              T64_CP0_TRANSFER_OK);
    T64_CHECK(machine.cpu.cp0[rd] == UINT64_C(0x1122334480000001));
    T64_CHECK(apply_cp0(&machine, CP0_MFC0, 9u, rd) ==
              T64_CP0_TRANSFER_OK);
    T64_CHECK(machine.cpu.gpr[9] == UINT64_C(0xffffffff80000001));
    machine.cpu.gpr[8] = UINT64_C(0);
    T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, rd) ==
              T64_CP0_TRANSFER_OK);
    T64_CHECK(machine.cpu.cp0[rd] == UINT64_C(0x1122334400000000));
    machine.cpu.gpr[8] = UINT64_C(0xfedcba9876543210);
    T64_CHECK(apply_cp0(&machine, CP0_DMTC0, 8u, rd) ==
              T64_CP0_TRANSFER_OK);
    T64_CHECK(machine.cpu.cp0[rd] == UINT64_C(0xfedcba9876543210));
    T64_CHECK(apply_cp0(&machine, CP0_DMFC0, 10u, rd) ==
              T64_CP0_TRANSFER_OK);
    T64_CHECK(machine.cpu.gpr[10] == UINT64_C(0xfedcba9876543210));
    machine.cpu.gpr[8] = UINT64_C(0);
    T64_CHECK(apply_cp0(&machine, CP0_DMTC0, 8u, rd) ==
              T64_CP0_TRANSFER_OK);
    T64_CHECK(machine.cpu.cp0[rd] == UINT64_C(0));
  }
}

static void test_context_config_and_xcontext_policy(void) {
  t64_machine machine;
  setup_vr4300_machine(&machine);

  machine.cpu.cp0[CP0_CONFIG] = CONFIG_SEED;
  machine.cpu.gpr[8] = UINT64_MAX;
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_CONFIG) ==
            T64_CP0_TRANSFER_UNSUPPORTED);
  T64_CHECK(machine.cpu.cp0[CP0_CONFIG] == CONFIG_SEED);
  machine.cpu.gpr[8] = CONFIG_SEED & ~UINT32_C(0x00008000);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_CONFIG) ==
            T64_CP0_TRANSFER_UNSUPPORTED);
  T64_CHECK(machine.cpu.cp0[CP0_CONFIG] == CONFIG_SEED);
  machine.cpu.gpr[8] = CONFIG_SEED ^ UINT32_C(0x0f);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_CONFIG) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_CONFIG] ==
            (uint64_t)(CONFIG_SEED ^ UINT32_C(0x0f)));
  T64_CHECK(apply_cp0(&machine, CP0_MFC0, 9u, CP0_CONFIG) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.gpr[9] ==
            (uint64_t)(CONFIG_SEED ^ UINT32_C(0x0f)));
  machine.cpu.gpr[8] = CONFIG_SEED;
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_CONFIG) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_CONFIG] == CONFIG_SEED);
  machine.cpu.cp0[CP0_CONFIG] = CONFIG_SEED;
  machine.cpu.cp0_config_init_window = true;
  machine.cpu.gpr[8] = (CONFIG_SEED & ~UINT32_C(0x0f000000)) |
                       UINT32_C(0x06000000);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_CONFIG) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_CONFIG] ==
            UINT64_C(0x7606e463));
  machine.cpu.gpr[8] = UINT64_C(0x76066463);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_CONFIG) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_CONFIG] == UINT64_C(0x76066463));
  machine.cpu.gpr[8] = (uint64_t)((uint32_t)machine.cpu.cp0[CP0_CONFIG] &
                                  ~UINT32_C(0x0f000000)) |
                       UINT32_C(0x05000000);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_CONFIG) ==
            T64_CP0_TRANSFER_UNSUPPORTED);
  T64_CHECK(machine.cpu.cp0[CP0_CONFIG] == UINT64_C(0x76066463));
  T64_CHECK(apply_cp0(&machine, CP0_DMTC0, 8u, CP0_CONFIG) ==
            T64_CP0_TRANSFER_UNSUPPORTED);

  machine.cpu.cp0[CP0_XCONTEXT] = UINT64_C(0x00000001abcd1230);
  machine.cpu.gpr[8] = UINT64_MAX;
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_XCONTEXT) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_XCONTEXT] ==
            UINT64_C(0x00000001abcd1230));
  T64_CHECK(apply_cp0(&machine, CP0_DMTC0, 8u, CP0_XCONTEXT) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_XCONTEXT] ==
            UINT64_C(0xffffffffabcd1230));
  T64_CHECK(apply_cp0(&machine, CP0_DMFC0, 9u, CP0_XCONTEXT) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.gpr[9] == UINT64_C(0xffffffffabcd1230));
  T64_CHECK(apply_cp0(&machine, CP0_MFC0, 9u, CP0_XCONTEXT) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.gpr[9] == UINT64_C(0xffffffffabcd1230));
  machine.cpu.gpr[8] = UINT64_C(0);
  T64_CHECK(apply_cp0(&machine, CP0_DMTC0, 8u, CP0_XCONTEXT) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_XCONTEXT] ==
            UINT64_C(0x00000001abcd1230));
}

static void test_privilege_width_and_reserved_selects(void) {
  static const uint32_t word_registers[] = {
      CP0_INDEX,     CP0_RANDOM, CP0_ENTRYLO0, CP0_ENTRYLO1, CP0_PAGEMASK,
      CP0_WIRED,     CP0_COUNT,  CP0_COMPARE,  CP0_STATUS,   CP0_CAUSE,
      CP0_PRID,      CP0_CONFIG, CP0_LLADDR,    CP0_WATCHLO,  CP0_WATCHHI,
      CP0_PERR,      CP0_CACHEERR, CP0_TAGLO,   CP0_TAGHI};
  t64_machine machine;
  t64_cp0_effect effect = {0};
  setup_vr4300_machine(&machine);

  machine.cpu.cp0[CP0_STATUS] = STATUS_KSU_USER;
  T64_CHECK(t64_cp0_access_unusable(&machine));
  machine.cpu.cp0[CP0_STATUS] = STATUS_KSU_USER | STATUS_CU0;
  T64_CHECK(!t64_cp0_access_unusable(&machine));
  machine.cpu.cp0[CP0_STATUS] = STATUS_KSU_USER | STATUS_UX | STATUS_CU0;
  machine.cpu.cp0[CP0_CONTEXT] = UINT64_C(0x123456789abcdef0);
  T64_CHECK(apply_cp0(&machine, CP0_DMFC0, 9u, CP0_CONTEXT) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.gpr[9] == UINT64_C(0x12345678803cdef0));
  machine.cpu.cp0[CP0_STATUS] = STATUS_KSU_USER | STATUS_CU0;
  T64_CHECK(apply_cp0(&machine, CP0_DMFC0, 9u, CP0_CONTEXT) ==
            T64_CP0_TRANSFER_RESERVED);
  machine.cpu.cp0[CP0_STATUS] = STATUS_KSU_SUPERVISOR | STATUS_CU0;
  T64_CHECK(apply_cp0(&machine, CP0_DMTC0, 8u, CP0_EPC) ==
            T64_CP0_TRANSFER_RESERVED);
  machine.cpu.cp0[CP0_STATUS] = STATUS_KSU_SUPERVISOR | STATUS_SX |
                                STATUS_CU0;
  T64_CHECK(apply_cp0(&machine, CP0_DMTC0, 8u, CP0_EPC) ==
            T64_CP0_TRANSFER_OK);
  machine.cpu.cp0[CP0_STATUS] = UINT32_C(0);
  T64_CHECK(!t64_cp0_access_unusable(&machine));

  for (size_t i = 0u;
       i < sizeof word_registers / sizeof word_registers[0]; i++) {
    uint32_t rd = word_registers[i];
    uint64_t before = machine.cpu.cp0[rd];
    uint64_t before_gpr9 = machine.cpu.gpr[9];
    machine.cpu.gpr[8] = UINT64_MAX;
    T64_CHECK(apply_cp0(&machine, CP0_DMFC0, 9u, rd) ==
              T64_CP0_TRANSFER_UNSUPPORTED);
    T64_CHECK(machine.cpu.gpr[9] == before_gpr9);
    T64_CHECK(apply_cp0(&machine, CP0_DMTC0, 8u, rd) ==
              T64_CP0_TRANSFER_UNSUPPORTED);
    T64_CHECK(machine.cpu.cp0[rd] == before);
  }

  machine.cpu.cp0[UINT32_C(7)] = UINT64_C(0x1234);
  machine.cpu.gpr[8] = UINT64_MAX;
  T64_CHECK(t64_cp0_prepare_transfer(
                &machine, encode_cp0(CP0_MTC0, 8u, UINT32_C(7)), &effect) ==
            T64_CP0_TRANSFER_RESERVED);
  T64_CHECK(machine.cpu.cp0[UINT32_C(7)] == UINT64_C(0x1234));
  T64_CHECK(t64_cp0_prepare_transfer(
                &machine,
                encode_cp0(CP0_MFC0, 8u, CP0_COUNT) | UINT32_C(1),
                &effect) == T64_CP0_TRANSFER_RESERVED);
  T64_CHECK(t64_cp0_prepare_transfer(
                &machine, encode_cp0(UINT32_C(2), 8u, CP0_COUNT), &effect) ==
            T64_CP0_TRANSFER_RESERVED);
}

static void test_compat_doubleword_transfers_to_narrow_registers(void) {
  t64_machine machine;
  setup_vr4300_compat_machine(&machine);

  machine.cpu.cp0[CP0_COUNT] = UINT64_C(0xdeadbeef87654321);
  T64_CHECK(apply_cp0(&machine, CP0_DMFC0, 9u, CP0_COUNT) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.gpr[9] == UINT64_C(0x0000000087654321));

  machine.cpu.gpr[8] = UINT64_C(0xfeedbeef87654321);
  T64_CHECK(apply_cp0(&machine, CP0_DMTC0, 8u, CP0_COUNT) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_COUNT] == UINT64_C(0x87654321));

  machine.cpu.cp0[CP0_CAUSE] = CAUSE_SOFTWARE_IP | CAUSE_TIMER_IP;
  machine.cpu.gpr[8] = UINT64_C(0xfeedbeef12345678);
  T64_CHECK(apply_cp0(&machine, CP0_DMTC0, 8u, CP0_COMPARE) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_COMPARE] == UINT64_C(0x12345678));
  T64_CHECK(machine.cpu.cp0[CP0_CAUSE] == CAUSE_SOFTWARE_IP);

  machine.cpu.cp0[CP0_PAGEMASK] = UINT64_C(0x00006000);
  machine.cpu.cp0[CP0_RANDOM] = UINT64_C(0x0000002b);
  machine.cpu.gpr[8] = UINT64_C(0xffffffffffff2000);
  T64_CHECK(apply_cp0(&machine, CP0_DMTC0, 8u, CP0_PAGEMASK) ==
            T64_CP0_TRANSFER_UNSUPPORTED);
  T64_CHECK(machine.cpu.cp0[CP0_PAGEMASK] == UINT64_C(0x00006000));
  T64_CHECK(machine.cpu.cp0[CP0_RANDOM] == UINT64_C(0x0000002b));

  machine.cpu.gpr[8] = UINT64_C(0xffffffff01ffe000);
  T64_CHECK(apply_cp0(&machine, CP0_DMTC0, 8u, CP0_PAGEMASK) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_PAGEMASK] == UINT64_C(0x01ffe000));
}

static void test_compat_v2_wired_random_and_pagemask_policy(void) {
  static const struct {
    uint32_t source_mask;
    uint32_t stored_entry_mask;
    uint32_t tlbr_readback;
  } page_masks[] = {
      {UINT32_C(0x00002000), UINT32_C(0x00002000), UINT32_C(0x00000000)},
      {UINT32_C(0x00004000), UINT32_C(0x00004000), UINT32_C(0x00006000)},
      {UINT32_C(0x0000e000), UINT32_C(0x0000e000), UINT32_C(0x00006000)},
      {UINT32_C(0x00010000), UINT32_C(0x00010000), UINT32_C(0x00018000)},
      {UINT32_C(0x00006000), UINT32_C(0x00006000), UINT32_C(0x00006000)},
      {UINT32_C(0x01ffe000), UINT32_C(0x01ffe000), UINT32_C(0x01ffe000)},
  };
  t64_machine machine;
  setup_vr4300_compat_v2_machine(&machine);

  machine.cpu.gpr[8] = UINT64_C(0xffffffffffffffff);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_PAGEMASK) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_PAGEMASK] == UINT64_C(0x01ffe000));
  machine.cpu.gpr[8] = UINT64_C(0x000000000000e000);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_PAGEMASK) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_PAGEMASK] == UINT64_C(0x0000e000));

  machine.cpu.gpr[8] = UINT64_C(32);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_WIRED) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_WIRED] == UINT64_C(32));
  T64_CHECK(machine.cpu.cp0[CP0_RANDOM] == UINT64_C(31));
  {
    bool seen[64] = {false};
    for (unsigned step = 0u; step < 96u; step++) {
      uint32_t random = (uint32_t)machine.cpu.cp0[CP0_RANDOM] &
                        UINT32_C(0x3f);
      seen[random] = true;
      T64_CHECK(t64_cp0_random_tlb_index(&machine) <= UINT32_C(31));
      t64_cp0_instruction_retired(&machine);
    }
    for (size_t index = 0u; index < sizeof seen / sizeof seen[0]; index++) {
      T64_CHECK(seen[index]);
    }
  }

  for (size_t index = 0u;
       index < sizeof page_masks / sizeof page_masks[0]; index++) {
    machine.cpu.gpr[8] = page_masks[index].source_mask;
    T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_PAGEMASK) ==
              T64_CP0_TRANSFER_OK);
    T64_CHECK(machine.cpu.cp0[CP0_PAGEMASK] ==
              (uint64_t)(page_masks[index].source_mask &
                         UINT32_C(0x01ffe000)));
    machine.cpu.cp0[CP0_INDEX] = (uint64_t)index;
    T64_CHECK(apply_tlb_instruction(&machine, UINT32_C(0x02)) ==
              T64_CP0_TRANSFER_OK);
    T64_CHECK(machine.cpu.tlb[index].page_mask ==
              (uint64_t)page_masks[index].stored_entry_mask);

    machine.cpu.cp0[CP0_PAGEMASK] = UINT64_C(0);
    T64_CHECK(apply_tlb_instruction(&machine, UINT32_C(0x01)) ==
              T64_CP0_TRANSFER_OK);
    T64_CHECK(machine.cpu.cp0[CP0_PAGEMASK] ==
              (uint64_t)page_masks[index].tlbr_readback);
  }

  /* The v1 profile keeps its published five-bit Random cycle. */
  setup_vr4300_compat_machine(&machine);
  machine.cpu.gpr[8] = UINT64_C(32);
  T64_CHECK(apply_cp0(&machine, CP0_MTC0, 8u, CP0_WIRED) ==
            T64_CP0_TRANSFER_OK);
  T64_CHECK(machine.cpu.cp0[CP0_WIRED] == UINT64_C(32));
  {
    bool seen_upper_index = false;
    for (unsigned step = 0u; step < 96u; step++) {
      seen_upper_index |= machine.cpu.cp0[CP0_RANDOM] >= UINT64_C(32);
      T64_CHECK(t64_cp0_random_tlb_index(&machine) <= UINT32_C(31));
      t64_cp0_instruction_retired(&machine);
    }
    T64_CHECK(!seen_upper_index);
  }
}

int main(void) {
  test_status_each_bit_set_and_clear();
  test_index_random_and_wired_fields();
  test_entrylo_context_pagemask();
  test_readonly_and_word_registers();
  test_entryhi_cause_compare_and_return_pc();
  test_context_config_and_xcontext_policy();
  test_privilege_width_and_reserved_selects();
  test_compat_doubleword_transfers_to_narrow_registers();
  test_compat_v2_wired_random_and_pagemask_policy();
  return T64_TEST_FINISH("unit.core.cpu-cp0");
}
