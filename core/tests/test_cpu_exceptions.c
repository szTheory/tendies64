#include "t64_internal.h"
#include "tendies64/t64_machine.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define IPL3_ASSET_BYTES 4096u
#define TEST_PC UINT64_C(0xffffffff80001000)
#define GENERAL_VECTOR UINT64_C(0xffffffff80000180)
#define T64_CP0_BAD_VADDR UINT32_C(8)
#define T64_CP0_STATUS UINT32_C(12)
#define T64_CP0_CAUSE UINT32_C(13)
#define T64_CP0_EPC UINT32_C(14)
#define T64_CP0_ERROR_EPC UINT32_C(30)
#define T64_STATUS_ERL UINT32_C(0x00000004)
#define T64_STATUS_EXL UINT32_C(0x00000002)
#define T64_STATUS_CU1 UINT32_C(0x20000000)
#define T64_CAUSE_BD UINT32_C(0x80000000)
#define T64_CAUSE_CE UINT32_C(0x30000000)

typedef struct {
  size_t outstanding;
} allocator_state;

typedef bool (*exception_test_fn)(t64_machine *machine,
                                  const uint8_t asset[IPL3_ASSET_BYTES]);

typedef struct {
  const char *name;
  exception_test_fn run;
} exception_test_case;

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  allocator_state *state = (allocator_state *)context;
  void *memory;

  if (state == NULL || storage_out == NULL || alignment == 0u) {
    return false;
  }
  memory = malloc(size);
  if (memory == NULL || ((uintptr_t)memory % alignment) != 0u) {
    free(memory);
    return false;
  }
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  state->outstanding++;
  return true;
}

static void test_release(void *context, t64_host_storage storage) {
  allocator_state *state = (allocator_state *)context;

  if (state != NULL && storage.data != NULL) {
    free(storage.data);
    state->outstanding--;
  }
}

static bool read_qualified_asset(uint8_t bytes[IPL3_ASSET_BYTES]) {
  const char *path = getenv("T64_TEST_IPL3_PATH");
  FILE *file;
  size_t length;
  int extra;
  int read_error;
  int close_error;

  if (path == NULL || path[0] == '\0' || bytes == NULL) {
    return false;
  }
  file = fopen(path, "rb");
  if (file == NULL) {
    return false;
  }
  length = fread(bytes, 1u, IPL3_ASSET_BYTES, file);
  extra = fgetc(file);
  read_error = ferror(file);
  close_error = fclose(file);
  return read_error == 0 && close_error == 0 &&
         length == IPL3_ASSET_BYTES && extra == EOF;
}

static uint32_t physical_address(uint64_t virtual_address) {
  return (uint32_t)virtual_address & UINT32_C(0x1fffffff);
}

static void write_instruction(t64_machine *machine, uint64_t address,
                              uint32_t opcode) {
  uint32_t physical = physical_address(address);

  machine->ram[physical] = (uint8_t)(opcode >> 24);
  machine->ram[physical + 1u] = (uint8_t)(opcode >> 16);
  machine->ram[physical + 2u] = (uint8_t)(opcode >> 8);
  machine->ram[physical + 3u] = (uint8_t)opcode;
}

static bool prepare_program(t64_machine *machine,
                            const uint8_t asset[IPL3_ASSET_BYTES],
                            uint64_t start, const uint32_t *program,
                            size_t count) {
  size_t i;

  if (machine == NULL || asset == NULL || program == NULL ||
      t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK) {
    return false;
  }
  for (i = 0u; i < count; i++) {
    write_instruction(machine, start + (uint64_t)(i * 4u), program[i]);
  }
  machine->cpu.pc_full = start;
  machine->cpu.pc = (uint32_t)start;
  return true;
}

static t64_status advance_ticks(t64_machine *machine, uint64_t ticks,
                                t64_stop_result *result_out) {
  t64_stop_request request = {T64_STOP_GUEST_TICK, ticks};

  return t64_machine_advance(machine, &request, result_out);
}

static bool expect_exception(t64_machine *machine, uint64_t ticks,
                             uint32_t code, uint64_t epc, bool branch_delay,
                             bool expect_bad_vaddr,
                             uint64_t bad_vaddr) {
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_status status = advance_ticks(machine, ticks, &result);
  uint32_t cause = (uint32_t)machine->cpu.cp0[T64_CP0_CAUSE];
  uint32_t expected_cause = code << 2;
  uint64_t observed_epc = machine->cpu.cp0[T64_CP0_EPC];
  uint32_t status_register = (uint32_t)machine->cpu.cp0[T64_CP0_STATUS];
  bool okay;

  if (branch_delay) {
    expected_cause |= T64_CAUSE_BD;
  }
  okay = status == T64_STATUS_OK && result.reason == T64_STOP_REASON_BOUND &&
         result.tick == ticks &&
         (cause & (UINT32_C(0x8000007c))) == expected_cause &&
         observed_epc == epc && (status_register & T64_STATUS_EXL) != 0u &&
         machine->cpu.attempt_outcome == T64_CPU_ATTEMPT_GUEST_EXCEPTION &&
         !machine->cpu.delay_slot_pending;
  if (expect_bad_vaddr) {
    okay = okay && machine->cpu.cp0[T64_CP0_BAD_VADDR] == bad_vaddr;
  }
  if (!okay) {
    (void)printf("# exception expected code=%u epc=%016llx bd=%u "
                 "actual status=%u reason=%u tick=%llu cause=%08x "
                 "epc=%016llx badvaddr=%016llx statusreg=%08x "
                 "outcome=%u retired=%llu pc=%016llx\n",
                 code, (unsigned long long)epc, branch_delay ? 1u : 0u,
                 (unsigned)status, result.reason,
                 (unsigned long long)result.tick, cause,
                 (unsigned long long)observed_epc,
                 (unsigned long long)machine->cpu.cp0[T64_CP0_BAD_VADDR],
                 status_register, machine->cpu.attempt_outcome,
                 (unsigned long long)machine->cpu.retired_instructions,
                 (unsigned long long)machine->cpu.pc_full);
  }
  return okay;
}

/* NEC U10504EJ7V0UMJ1, pp.171-174, 180-182, gives Cause/EPC/BD and vector
 * semantics; the expected ExcCode values below use Table 6-2 directly. */
static bool test_syscall_exception(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {UINT32_C(0x0000000c)};

  return prepare_program(machine, asset, TEST_PC, program, 1u) &&
         expect_exception(machine, UINT64_C(1), UINT32_C(8), TEST_PC, false,
                          false, 0u) &&
         machine->cpu.retired_instructions == 0u &&
         machine->cpu.pc_full == GENERAL_VECTOR;
}

static bool test_delay_slot_syscall(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {UINT32_C(0x10000002),
                                     UINT32_C(0x0000000c)};

  return prepare_program(machine, asset, TEST_PC, program, 2u) &&
         expect_exception(machine, UINT64_C(2), UINT32_C(8), TEST_PC, true,
                          false, 0u) &&
         machine->cpu.retired_instructions == UINT64_C(1);
}

static bool test_nested_exception_preserves_bd_one(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {UINT32_C(0x10000002),
                                     UINT32_C(0x0000000d)};

  if (!prepare_program(machine, asset, TEST_PC, program, 2u)) {
    return false;
  }
  write_instruction(machine, GENERAL_VECTOR, UINT32_C(0x0000000c));
  return expect_exception(machine, UINT64_C(3), UINT32_C(8), TEST_PC, true,
                          false, 0u) &&
         machine->cpu.retired_instructions == UINT64_C(1);
}

static bool test_old_exl_preserves_bd_zero(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {UINT32_C(0x10000002),
                                     UINT32_C(0x0000000d)};
  const uint64_t original_epc = UINT64_C(0xffffffff80002000);

  if (!prepare_program(machine, asset, TEST_PC, program, 2u)) {
    return false;
  }
  machine->cpu.cp0[T64_CP0_STATUS] |= T64_STATUS_EXL;
  machine->cpu.cp0[T64_CP0_EPC] = original_epc;
  machine->cpu.cp0[T64_CP0_CAUSE] &= ~T64_CAUSE_BD;
  return expect_exception(machine, UINT64_C(2), UINT32_C(9), original_epc,
                          false, false, 0u) &&
         machine->cpu.retired_instructions == UINT64_C(1);
}

static bool test_reserved_instruction_exception(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {UINT32_C(0x7000003f)};

  return prepare_program(machine, asset, TEST_PC, program, 1u) &&
         expect_exception(machine, UINT64_C(1), UINT32_C(10), TEST_PC, false,
                          false, 0u) &&
         machine->cpu.retired_instructions == 0u;
}

static bool test_trap_exception(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {UINT32_C(0x012a0034)};

  if (!prepare_program(machine, asset, TEST_PC, program, 1u)) {
    return false;
  }
  machine->cpu.gpr[9] = UINT64_C(0x12345678);
  machine->cpu.gpr[10] = UINT64_C(0x12345678);
  return expect_exception(machine, UINT64_C(1), UINT32_C(13), TEST_PC, false,
                          false, 0u) &&
         machine->cpu.retired_instructions == 0u;
}

static bool test_integer_overflow_exception(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {UINT32_C(0x012a4020)};

  if (!prepare_program(machine, asset, TEST_PC, program, 1u)) {
    return false;
  }
  machine->cpu.gpr[9] = UINT64_C(0x7fffffff);
  machine->cpu.gpr[10] = UINT64_C(1);
  machine->cpu.gpr[8] = UINT64_C(0x1234);
  return expect_exception(machine, UINT64_C(1), UINT32_C(12), TEST_PC, false,
                          false, 0u) &&
         machine->cpu.gpr[8] == UINT64_C(0x1234) &&
         machine->cpu.retired_instructions == 0u;
}

static bool test_coprocessor_unusable_ce(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {UINT32_C(0x46000000)};

  if (!prepare_program(machine, asset, TEST_PC, program, 1u)) {
    return false;
  }
  machine->cpu.cp0[T64_CP0_STATUS] &= ~T64_STATUS_CU1;
  return expect_exception(machine, UINT64_C(1), UINT32_C(11), TEST_PC, false,
                          false, 0u) &&
         (((uint32_t)machine->cpu.cp0[T64_CP0_CAUSE] & T64_CAUSE_CE) >> 28) ==
             UINT32_C(1);
}

static bool test_watch_exception(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {UINT32_C(0x8d280000)};

  if (!prepare_program(machine, asset, TEST_PC, program, 1u)) {
    return false;
  }
  machine->cpu.gpr[9] = UINT64_C(0xffffffff80002000);
  machine->ram[0x2000u] = UINT8_C(0x12);
  machine->cpu.cp0[18] = UINT32_C(0x00002001);
  return expect_exception(machine, UINT64_C(1), UINT32_C(23), TEST_PC, false,
                          false, 0u) &&
         machine->cpu.retired_instructions == 0u;
}

static bool test_eret_epc_and_resume(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t handler[] = {UINT32_C(0x42000018)};
  static const uint32_t resume[] = {UINT32_C(0x24081234)};
  const uint64_t resume_pc = UINT64_C(0xffffffff80002000);
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};

  if (!prepare_program(machine, asset, GENERAL_VECTOR, handler, 1u)) {
    return false;
  }
  write_instruction(machine, resume_pc, resume[0]);
  machine->cpu.cp0[T64_CP0_STATUS] |= T64_STATUS_EXL;
  machine->cpu.cp0[T64_CP0_EPC] = resume_pc;
  machine->cpu.linked_valid = true;
  machine->cpu.linked_width = UINT32_C(4);
  machine->cpu.linked_physical_address = UINT32_C(0x2000);
  if (advance_ticks(machine, UINT64_C(2), &result) != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_BOUND || result.tick != UINT64_C(2) ||
      machine->cpu.pc_full != resume_pc + UINT64_C(4) ||
      machine->cpu.gpr[8] != UINT64_C(0x1234) ||
      machine->cpu.retired_instructions != UINT64_C(2) ||
      (machine->cpu.cp0[T64_CP0_STATUS] & T64_STATUS_EXL) != 0u ||
      machine->cpu.linked_valid) {
    (void)printf("# ERET EPC resume tick=%llu pc=%016llx gpr8=%016llx "
                 "retired=%llu status=%08x linked=%u\n",
                 (unsigned long long)result.tick,
                 (unsigned long long)machine->cpu.pc_full,
                 (unsigned long long)machine->cpu.gpr[8],
                 (unsigned long long)machine->cpu.retired_instructions,
                 (uint32_t)machine->cpu.cp0[T64_CP0_STATUS],
                 machine->cpu.linked_valid ? 1u : 0u);
    return false;
  }
  return true;
}

static bool test_eret_error_epc_preserves_exl(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t handler[] = {UINT32_C(0x42000018)};
  static const uint32_t resume[] = {UINT32_C(0x24081234)};
  const uint64_t resume_pc = UINT64_C(0xffffffff80002000);
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};

  if (!prepare_program(machine, asset, GENERAL_VECTOR, handler, 1u)) {
    return false;
  }
  write_instruction(machine, resume_pc, resume[0]);
  machine->cpu.cp0[T64_CP0_STATUS] |= T64_STATUS_EXL | T64_STATUS_ERL;
  machine->cpu.cp0[T64_CP0_EPC] = UINT64_C(0xffffffff80003000);
  machine->cpu.cp0[T64_CP0_ERROR_EPC] = resume_pc;
  if (advance_ticks(machine, UINT64_C(2), &result) != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_BOUND || result.tick != UINT64_C(2) ||
      machine->cpu.pc_full != resume_pc + UINT64_C(4) ||
      machine->cpu.gpr[8] != UINT64_C(0x1234) ||
      (machine->cpu.cp0[T64_CP0_STATUS] & T64_STATUS_ERL) != 0u ||
      (machine->cpu.cp0[T64_CP0_STATUS] & T64_STATUS_EXL) == 0u) {
    return false;
  }
  return true;
}

static bool test_eret_invalid_alignment_raises_adel(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t handler[] = {UINT32_C(0x42000018)};
  const uint64_t invalid_pc = UINT64_C(0xffffffff80002002);

  if (!prepare_program(machine, asset, GENERAL_VECTOR, handler, 1u)) {
    return false;
  }
  machine->cpu.cp0[T64_CP0_STATUS] |= T64_STATUS_EXL;
  machine->cpu.cp0[T64_CP0_EPC] = invalid_pc;
  return expect_exception(machine, UINT64_C(2), UINT32_C(4), invalid_pc,
                          false, true, invalid_pc) &&
         machine->cpu.retired_instructions == UINT64_C(1);
}

static bool test_eret_full_width_maximum_address(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t handler[] = {UINT32_C(0x42000018)};
  const uint64_t maximum_aligned = UINT64_C(0xfffffffffffffffc);
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};

  if (!prepare_program(machine, asset, GENERAL_VECTOR, handler, 1u)) {
    return false;
  }
  machine->cpu.cp0[T64_CP0_STATUS] |= T64_STATUS_ERL;
  machine->cpu.cp0[T64_CP0_ERROR_EPC] = maximum_aligned;
  return advance_ticks(machine, UINT64_C(1), &result) == T64_STATUS_OK &&
         result.reason == T64_STOP_REASON_BOUND &&
         machine->cpu.pc_full == maximum_aligned &&
         machine->cpu.pc == UINT32_C(0xfffffffc) &&
         (machine->cpu.cp0[T64_CP0_STATUS] & T64_STATUS_ERL) == 0u &&
         machine->cpu.retired_instructions == UINT64_C(1);
}

static bool test_fetch_alignment_precedes_syscall_decode(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {UINT32_C(0x0000000c)};
  const uint64_t invalid_pc = TEST_PC + UINT64_C(2);

  if (!prepare_program(machine, asset, TEST_PC, program, 1u)) {
    return false;
  }
  machine->cpu.pc = (uint32_t)invalid_pc;
  machine->cpu.pc_full = invalid_pc;
  return expect_exception(machine, UINT64_C(1), UINT32_C(4), invalid_pc, false,
                          true, invalid_pc) &&
         machine->cpu.retired_instructions == 0u;
}

static bool test_faulting_store_has_no_partial_write(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {UINT32_C(0xad280000)};
  const uint64_t address = UINT64_C(0xffffffff80002001);
  uint32_t physical = physical_address(address);

  if (!prepare_program(machine, asset, TEST_PC, program, 1u)) {
    return false;
  }
  machine->ram[physical - 1u] = UINT8_C(0x11);
  machine->ram[physical] = UINT8_C(0x22);
  machine->ram[physical + 1u] = UINT8_C(0x33);
  machine->ram[physical + 2u] = UINT8_C(0x44);
  machine->cpu.gpr[8] = UINT64_C(0xdeadbeef);
  machine->cpu.gpr[9] = address;
  return expect_exception(machine, UINT64_C(1), UINT32_C(5), TEST_PC, false,
                          true, address) &&
         machine->cpu.retired_instructions == 0u &&
         machine->ram[physical - 1u] == UINT8_C(0x11) &&
         machine->ram[physical] == UINT8_C(0x22) &&
         machine->ram[physical + 1u] == UINT8_C(0x33) &&
         machine->ram[physical + 2u] == UINT8_C(0x44);
}

static bool test_instruction_bus_error(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {UINT32_C(0x24080001)};
  const uint64_t unmapped_pc = UINT64_C(0xffffffffbfc00800);

  if (!prepare_program(machine, asset, TEST_PC, program, 1u)) {
    return false;
  }
  machine->cpu.pc = (uint32_t)unmapped_pc;
  machine->cpu.pc_full = unmapped_pc;
  return expect_exception(machine, UINT64_C(1), UINT32_C(6), unmapped_pc,
                          false, false, 0u) &&
         machine->cpu.retired_instructions == 0u;
}

static bool test_data_bus_error(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {UINT32_C(0x8d280000)};
  const uint64_t unmapped_address = UINT64_C(0xffffffffbfc00800);

  if (!prepare_program(machine, asset, TEST_PC, program, 1u)) {
    return false;
  }
  machine->cpu.gpr[9] = unmapped_address;
  return expect_exception(machine, UINT64_C(1), UINT32_C(7), TEST_PC, false,
                          false, 0u) &&
         machine->cpu.retired_instructions == 0u;
}

static bool test_instruction_tlb_miss(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {UINT32_C(0x24080001)};
  const uint64_t kuseg_pc = UINT64_C(0x00001000);

  return prepare_program(machine, asset, kuseg_pc, program, 1u) &&
         expect_exception(machine, UINT64_C(1), UINT32_C(2), kuseg_pc, false,
                          true, kuseg_pc) &&
         machine->cpu.retired_instructions == 0u;
}

static bool test_data_tlb_miss(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {UINT32_C(0x8d280000)};
  const uint64_t kuseg_address = UINT64_C(0x00001000);

  if (!prepare_program(machine, asset, TEST_PC, program, 1u)) {
    return false;
  }
  machine->cpu.gpr[9] = kuseg_address;
  return expect_exception(machine, UINT64_C(1), UINT32_C(2), TEST_PC, false,
                          true, kuseg_address) &&
         machine->cpu.retired_instructions == 0u;
}

static bool test_scheduler_exhaustion_is_host_error(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {UINT32_C(0x0000000d)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_status status;

  if (!prepare_program(machine, asset, TEST_PC, program, 1u)) {
    return false;
  }
  machine->cpu.cp0[T64_CP0_STATUS] |= T64_STATUS_EXL;
  machine->cpu.cp0[T64_CP0_EPC] = UINT64_C(0xffffffff80003000);
  machine->sequence_exhausted = true;
  status = advance_ticks(machine, UINT64_C(1), &result);
  return status == T64_STATUS_OK &&
         result.reason == T64_STOP_REASON_RESOURCE_LIMIT &&
         result.tick == UINT64_C(1) &&
         machine->cpu.attempt_outcome == T64_CPU_ATTEMPT_HOST_ERROR &&
         machine->cpu.cp0[T64_CP0_EPC] == UINT64_C(0xffffffff80003000) &&
         (uint32_t)machine->cpu.cp0[T64_CP0_CAUSE] == 0u &&
         ((uint32_t)machine->cpu.cp0[T64_CP0_STATUS] & T64_STATUS_EXL) != 0u &&
         machine->cpu.retired_instructions == 0u;
}

int main(void) {
  allocator_state allocator_state_value = {0u};
  const t64_host_allocator allocator = {
      test_allocate, test_release, &allocator_state_value};
  static const exception_test_case cases[] = {
      {"SYSCALL enters the common vector without retirement",
       test_syscall_exception},
      {"delay-slot SYSCALL captures branch EPC and BD",
       test_delay_slot_syscall},
      {"nested fault under old BD=1 preserves EPC and BD",
       test_nested_exception_preserves_bd_one},
      {"nested delay fault under old BD=0 preserves the original pair",
       test_old_exl_preserves_bd_zero},
      {"reserved encoding raises RI", test_reserved_instruction_exception},
      {"TEQ raises Tr when its operands are equal", test_trap_exception},
      {"ADD overflow raises Ov without a destination write",
       test_integer_overflow_exception},
      {"unusable COP1 records CE=1", test_coprocessor_unusable_ce},
      {"WatchLo read match raises Watch before a load commit",
       test_watch_exception},
      {"ERET uses EPC, clears EXL, and resumes full-width execution",
       test_eret_epc_and_resume},
      {"ERET uses ErrorEPC under ERL and preserves EXL",
       test_eret_error_epc_preserves_exl},
      {"unaligned ERET target raises AdEL before fetch",
       test_eret_invalid_alignment_raises_adel},
      {"ERET preserves the maximum aligned 64-bit return address",
       test_eret_full_width_maximum_address},
      {"fetch alignment fault precedes SYSCALL decode",
       test_fetch_alignment_precedes_syscall_decode},
      {"faulting unaligned store preserves all memory bytes",
       test_faulting_store_has_no_partial_write},
      {"unmapped instruction fetch raises IBE", test_instruction_bus_error},
      {"unmapped data load raises DBE", test_data_bus_error},
      {"unmapped instruction translation raises TLBL",
       test_instruction_tlb_miss},
      {"unmapped data translation raises TLBL", test_data_tlb_miss},
      {"scheduler exhaustion remains a host error with no guest commit",
       test_scheduler_exhaustion_is_host_error},
  };
  const t64_profile_descriptor *profile = NULL;
  uint8_t asset[IPL3_ASSET_BYTES];
  size_t failures = 0u;
  size_t i;

  if (!read_qualified_asset(asset) ||
      t64_profile_lookup("ntsc-cart-4m-openipl3-vr4300-v1", &profile) !=
          T64_STATUS_OK ||
      profile == NULL) {
    (void)puts("1..1");
    (void)puts("not ok 1 - qualified new-profile exception fixture setup");
    return 1;
  }
  (void)printf("1..%zu\n", sizeof cases / sizeof cases[0]);
  for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
    t64_machine *machine = NULL;
    bool okay = t64_machine_create(&allocator, profile, &machine) ==
                    T64_STATUS_OK &&
                machine != NULL;

    if (okay) {
      okay = cases[i].run(machine, asset);
    } else {
      (void)puts("# machine allocation failed");
    }
    (void)printf("%s %zu - %s\n", okay ? "ok" : "not ok", i + 1u,
                 cases[i].name);
    if (!okay) {
      failures++;
    }
    if (machine != NULL) {
      t64_machine_destroy(machine);
    }
  }
  if (allocator_state_value.outstanding != 0u) {
    (void)printf("# allocator leak: %zu allocations remain\n",
                 allocator_state_value.outstanding);
    failures++;
  }
  return failures == 0u ? 0 : 1;
}
