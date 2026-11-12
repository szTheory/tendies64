#include "t64_internal.h"
#include "tendies64/t64_machine.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IPL3_ASSET_BYTES 4096u
#define IPL3_ENTRY_PC UINT32_C(0xa4000040)
#define T64_CP0_CAUSE_SOFTWARE_MASK UINT32_C(0x00000300)
#define T64_CP0_CAUSE_TIMER_MASK UINT32_C(0x00008000)

typedef struct {
  size_t outstanding;
} allocator_state;

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

static void write_instruction(t64_machine *machine, size_t index,
                              uint32_t opcode) {
  size_t offset = (size_t)T64_SP_BANK_BYTES - IPL3_ASSET_BYTES +
                  (size_t)0x40u + index * 4u;

  machine->sp_dmem[offset] = (uint8_t)(opcode >> 24);
  machine->sp_dmem[offset + 1u] = (uint8_t)(opcode >> 16);
  machine->sp_dmem[offset + 2u] = (uint8_t)(opcode >> 8);
  machine->sp_dmem[offset + 3u] = (uint8_t)opcode;
}

static void write_rdram_instruction(t64_machine *machine,
                                    uint32_t physical_address,
                                    uint32_t opcode) {
  machine->ram[physical_address] = (uint8_t)(opcode >> 24);
  machine->ram[physical_address + 1u] = (uint8_t)(opcode >> 16);
  machine->ram[physical_address + 2u] = (uint8_t)(opcode >> 8);
  machine->ram[physical_address + 3u] = (uint8_t)opcode;
}

static bool install_program(t64_machine *machine,
                            const uint8_t asset[IPL3_ASSET_BYTES],
                            const uint32_t *program, size_t instruction_count) {
  size_t i;

  if (machine == NULL || asset == NULL || program == NULL ||
      instruction_count > (T64_SP_BANK_BYTES - 0x40u) / 4u ||
      t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK) {
    return false;
  }
  (void)memset(machine->cpu.gpr, 0, sizeof machine->cpu.gpr);
  machine->cpu.pc = IPL3_ENTRY_PC;
  for (i = 0u; i < instruction_count; i++) {
    write_instruction(machine, i, program[i]);
  }
  return true;
}

static bool test_vr4300_break_enters_rdram_handler(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  const uint32_t break_pc = UINT32_C(0x80001000);
  const uint32_t handler_pc = UINT32_C(0x80000180);
  const uint32_t store_value = UINT32_C(0xdeadbeef);
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(2)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_status status;
  uint32_t cause;
  uint32_t status_register;
  uint32_t stored_value;

  if (machine == NULL || asset == NULL ||
      t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK) {
    (void)puts("# new-profile exception fixture setup failed");
    return false;
  }
  write_rdram_instruction(machine, UINT32_C(0x1000), UINT32_C(0x0000000d));
  write_rdram_instruction(machine, UINT32_C(0x0180), UINT32_C(0xad280000));
  machine->cpu.pc = break_pc;
  machine->cpu.pc_full = t64_cpu_canonical_address(break_pc);
  machine->cpu.gpr[8] = store_value;
  machine->cpu.gpr[9] = UINT64_C(0xffffffff80002000);

  status = t64_machine_advance(machine, &request, &result);
  cause = (uint32_t)machine->cpu.cp0[13];
  status_register = (uint32_t)machine->cpu.cp0[12];
  stored_value = ((uint32_t)machine->ram[0x2000u] << 24) |
                 ((uint32_t)machine->ram[0x2001u] << 16) |
                 ((uint32_t)machine->ram[0x2002u] << 8) |
                 (uint32_t)machine->ram[0x2003u];
  if (status != T64_STATUS_OK || result.reason != T64_STOP_REASON_BOUND ||
      result.tick != UINT64_C(2) ||
      machine->cpu.retired_instructions != UINT64_C(1) ||
      machine->cpu.pc != handler_pc + UINT32_C(4) ||
      machine->cpu.cp0[14] != t64_cpu_canonical_address(break_pc) ||
      (cause & UINT32_C(0x8000007c)) != (UINT32_C(9) << 2) ||
      (status_register & UINT32_C(2)) == 0u ||
      stored_value != store_value) {
    (void)printf("# BREAK handler status=%u reason=%u tick=%llu retired=%llu "
                 "pc=%016llx cause=%08x epc=%016llx status=%08x "
                 "store=%08x\n",
                 (unsigned)status, result.reason,
                 (unsigned long long)result.tick,
                 (unsigned long long)machine->cpu.retired_instructions,
                 (unsigned long long)machine->cpu.pc_full, cause,
                 (unsigned long long)machine->cpu.cp0[14], status_register,
                 stored_value);
    return false;
  }
  return true;
}

static bool test_vr4300_tlb_refill_and_retry(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  const uint32_t load_pc = UINT32_C(0x80001000);
  const uint32_t refill_pc = UINT32_C(0x80000000);
  const uint32_t virtual_data = UINT32_C(0x00004000);
  const uint32_t physical_data = UINT32_C(0x00003000);
  const uint64_t expected_value = UINT64_C(0x12345678);
  const uint32_t entry_lo = UINT32_C(0x000000df);
  static const uint32_t refill_handler[] = {
      UINT32_C(0x341a00df), /* ori k0,zero,EntryLo */
      UINT32_C(0x409a1000), /* mtc0 k0,EntryLo0 */
      UINT32_C(0x409a1800), /* mtc0 k0,EntryLo1 */
      UINT32_C(0x42000002), /* tlbwi */
      UINT32_C(0x42000018), /* eret */
  };
  t64_vr4300_dcache_line data_line_before;
  uint8_t bytes_before[4];
  uint32_t linked_address_before;
  uint32_t linked_width_before;
  bool linked_valid_before;
  bool refill_state_ok;
  bool failed_access_state_ok;
  t64_status status;
  size_t index;

  if (machine == NULL || asset == NULL ||
      t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK) {
    (void)puts("# TLB refill fixture setup failed");
    return false;
  }
  for (index = 0u; index < sizeof refill_handler / sizeof refill_handler[0];
       index++) {
    write_rdram_instruction(machine, (uint32_t)(index * 4u),
                            refill_handler[index]);
  }
  write_rdram_instruction(machine, UINT32_C(0x1000), UINT32_C(0x8c880000));
  write_rdram_instruction(machine, UINT32_C(0x1004), UINT32_C(0x24090007));
  machine->ram[physical_data] = UINT8_C(0x12);
  machine->ram[physical_data + 1u] = UINT8_C(0x34);
  machine->ram[physical_data + 2u] = UINT8_C(0x56);
  machine->ram[physical_data + 3u] = UINT8_C(0x78);
  machine->cpu.pc = load_pc;
  machine->cpu.pc_full = t64_cpu_canonical_address(load_pc);
  machine->cpu.cp0[0] = UINT64_C(0);  /* Index selects entry zero. */
  machine->cpu.cp0[12] = UINT64_C(0); /* Kernel, BEV=0, 32-bit mode. */
  machine->cpu.gpr[4] = (uint64_t)virtual_data;
  machine->cpu.gpr[8] = UINT64_C(0xfeedface);
  machine->cpu.linked_valid = true;
  machine->cpu.linked_width = UINT32_C(4);
  machine->cpu.linked_physical_address = UINT32_C(0x80);
  data_line_before = machine->cpu.dcache[0];
  (void)memcpy(bytes_before, machine->ram + physical_data,
               sizeof bytes_before);
  linked_address_before = machine->cpu.linked_physical_address;
  linked_width_before = machine->cpu.linked_width;
  linked_valid_before = machine->cpu.linked_valid;

  status = t64_cpu_step(machine, NULL);
  refill_state_ok =
      status == T64_STATUS_OK && machine->cpu.pending_exception_valid &&
      machine->cpu.pc_full == t64_cpu_canonical_address(refill_pc) &&
      (uint32_t)machine->cpu.cp0[13] == (UINT32_C(2) << 2) &&
      machine->cpu.cp0[8] == (uint64_t)virtual_data &&
      machine->cpu.cp0[10] == (uint64_t)virtual_data;
  failed_access_state_ok =
      machine->cpu.gpr[8] == UINT64_C(0xfeedface) &&
      memcmp(bytes_before, machine->ram + physical_data,
             sizeof bytes_before) == 0 &&
      memcmp(&data_line_before, &machine->cpu.dcache[0],
             sizeof data_line_before) == 0 &&
      machine->cpu.linked_valid == linked_valid_before &&
      machine->cpu.linked_width == linked_width_before &&
      machine->cpu.linked_physical_address == linked_address_before;
  if (!refill_state_ok || !failed_access_state_ok) {
    (void)printf("# TLB miss observed status=%u pc=%016llx code=%u "
                 "badvaddr=%016llx entryhi=%016llx gpr8=%016llx "
                 "retired=%llu refill_ok=%u no_partial_effects=%u\n",
                 (unsigned)status,
                 (unsigned long long)machine->cpu.pc_full,
                 ((uint32_t)machine->cpu.cp0[13] >> 2) & UINT32_C(31),
                 (unsigned long long)machine->cpu.cp0[8],
                 (unsigned long long)machine->cpu.cp0[10],
                 (unsigned long long)machine->cpu.gpr[8],
                 (unsigned long long)machine->cpu.retired_instructions,
                 refill_state_ok ? 1u : 0u,
                 failed_access_state_ok ? 1u : 0u);
    return false;
  }

  for (index = 0u; index < 8u && machine->cpu.gpr[8] != expected_value;
       index++) {
    status = t64_cpu_step(machine, NULL);
    if (status != T64_STATUS_OK) {
      break;
    }
  }
  if (status == T64_STATUS_OK && machine->cpu.gpr[8] == expected_value) {
    status = t64_cpu_step(machine, NULL);
  }
  if (status != T64_STATUS_OK || machine->cpu.gpr[8] != expected_value ||
      machine->cpu.gpr[9] != UINT64_C(7) ||
      machine->cpu.tlb[0].entry_lo0 != (uint64_t)entry_lo ||
      machine->cpu.tlb[0].entry_lo1 != (uint64_t)entry_lo ||
      machine->cpu.pending_exception_valid) {
    (void)printf("# TLB retry observed status=%u pc=%016llx gpr8=%016llx "
                 "gpr9=%016llx tlblo0=%016llx tlblo1=%016llx "
                 "pending=%u\n",
                 (unsigned)status,
                 (unsigned long long)machine->cpu.pc_full,
                 (unsigned long long)machine->cpu.gpr[8],
                 (unsigned long long)machine->cpu.gpr[9],
                 (unsigned long long)machine->cpu.tlb[0].entry_lo0,
                 (unsigned long long)machine->cpu.tlb[0].entry_lo1,
                 machine->cpu.pending_exception_valid ? 1u : 0u);
    return false;
  }
  return true;
}

static bool test_vr4300_protected_cp0_guest_handler(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0x40886000), /* mtc0 t0,c0_status */
      UINT32_C(0x400a6000), /* mfc0 t2,c0_status */
      UINT32_C(0x402b7000), /* dmfc0 t3,c0_epc */
      UINT32_C(0x40896000), /* mtc0 t1,c0_status: enter user mode */
      UINT32_C(0x400d6000), /* mfc0 t5,c0_status: CU0 fault */
  };
  const uint32_t handler_pc = UINT32_C(0x80000180);
  const uint32_t expected_status = UINT32_C(0x90200001);
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(100)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};
  t64_status status;
  uint32_t observed_status;
  uint32_t cause;
  size_t index;

  if (machine == NULL || asset == NULL ||
      t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK) {
    (void)puts("# protected CP0 fixture setup failed");
    return false;
  }
  (void)memcpy(machine->cpu.gpr, (uint64_t[32]){0}, sizeof machine->cpu.gpr);
  machine->cpu.cp0[12] = UINT32_C(0x34200000); /* Keep hardware TS set. */
  machine->cpu.cp0[14] = UINT64_C(0x0123456789abcdef);
  machine->cpu.gpr[8] = UINT32_C(0x98000001);
  machine->cpu.gpr[9] = UINT32_C(0x00000010);
  machine->cpu.gpr[13] = UINT64_C(0x1122334455667788);
  for (index = 1u; index < T64_VR4300_TLB_ENTRY_COUNT; index++) {
    machine->cpu.tlb[index].asid = UINT32_C(1);
  }
  machine->cpu.tlb[0].vpn2 = UINT64_C(0x1000);
  machine->cpu.tlb[0].asid = UINT32_C(0);
  machine->cpu.tlb[0].entry_lo0 = UINT64_C(0x5e);
  machine->cpu.tlb[0].entry_lo1 = UINT64_C(0x5e);
  write_rdram_instruction(machine, UINT32_C(0x0180),
                          UINT32_C(0x240e005a)); /* addiu t6,zero,0x5a */
  write_rdram_instruction(machine, UINT32_C(0x0184),
                          UINT32_C(0xbc000000)); /* unsupported CACHE stop */
  for (index = 0u; index < sizeof program / sizeof program[0]; index++) {
    write_rdram_instruction(machine, UINT32_C(0x1000) +
                                        (uint32_t)(index * 4u),
                            program[index]);
  }
  machine->cpu.pc = UINT32_C(0x1000);
  machine->cpu.pc_full = UINT64_C(0x1000);

  status = t64_machine_advance(machine, &request, &result);
  (void)t64_machine_cpu_stop_detail(machine, &detail);
  observed_status = (uint32_t)machine->cpu.cp0[12];
  cause = (uint32_t)machine->cpu.cp0[13];
  if (status != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_CPU_UNSUPPORTED ||
      detail.pc != handler_pc + UINT32_C(4) ||
      machine->cpu.cp0[14] != UINT64_C(0x1010) ||
      ((cause >> 2) & UINT32_C(0x1f)) != UINT32_C(11) ||
      machine->cpu.gpr[10] != UINT64_C(0xffffffff90200001) ||
      machine->cpu.gpr[11] != UINT64_C(0x0123456789abcdef) ||
      machine->cpu.gpr[13] != UINT64_C(0x1122334455667788) ||
      machine->cpu.gpr[14] != UINT64_C(0x5a) ||
      observed_status != UINT32_C(0x00200012) ||
      detail.retired_instructions != UINT64_C(5)) {
    (void)printf("# expected status=0x%08x mfc0=0xffffffff%08x "
                 "dmfc0_epc=0x0123456789abcdef "
                 "unchanged_t5=0x1122334455667788 "
                 "actual status=%u reason=%u stop_pc=0x%08x opcode=0x%08x "
                 "cp0_status=0x%08x cause=0x%08x epc=0x%016llx "
                 "mfc0_t2=0x%016llx dmfc0_t3=0x%016llx t5=0x%016llx "
                 "t6=0x%016llx retired=%llu\n",
                 expected_status, expected_status,
                 (unsigned)status, result.reason, detail.pc, detail.opcode,
                 observed_status, cause,
                 (unsigned long long)machine->cpu.cp0[14],
                 (unsigned long long)machine->cpu.gpr[10],
                 (unsigned long long)machine->cpu.gpr[11],
                 (unsigned long long)machine->cpu.gpr[13],
                 (unsigned long long)machine->cpu.gpr[14],
                 (unsigned long long)detail.retired_instructions);
    return false;
  }
  return true;
}

static bool test_vr4300_timer_interrupt_handler(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t main_program[] = {
      UINT32_C(0x40885800), /* mtc0 t0,c0_compare */
      UINT32_C(0x240a0066), /* addiu t2,zero,0x66 */
      UINT32_C(0x240b0077), /* addiu t3,zero,0x77 */
      UINT32_C(0x240d0055), /* interrupted then resumed */
  };
  static const uint32_t handler_program[] = {
      UINT32_C(0x40895800), /* mtc0 t1,c0_compare: clear IP7 */
      UINT32_C(0x240c004a), /* addiu t4,zero,0x4a */
      UINT32_C(0x42000018), /* eret */
  };
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(8)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_status status;
  uint32_t cause;
  uint32_t status_register;
  size_t i;

  if (machine == NULL || asset == NULL ||
      t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK) {
    (void)puts("# timer interrupt fixture setup failed");
    return false;
  }
  for (i = 0u; i < sizeof main_program / sizeof main_program[0]; i++) {
    write_rdram_instruction(machine, UINT32_C(0x1000) + (uint32_t)(i * 4u),
                            main_program[i]);
  }
  for (i = 0u; i < sizeof handler_program / sizeof handler_program[0]; i++) {
    write_rdram_instruction(machine, UINT32_C(0x0180) + (uint32_t)(i * 4u),
                            handler_program[i]);
  }
  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = t64_cpu_canonical_address(machine->cpu.pc);
  machine->cpu.gpr[8] = UINT64_C(2);
  machine->cpu.gpr[9] = UINT64_C(100);
  machine->cpu.cp0[12] = UINT32_C(0x00008001); /* IE and timer IM7. */

  status = t64_machine_advance(machine, &request, &result);
  cause = (uint32_t)machine->cpu.cp0[13];
  status_register = (uint32_t)machine->cpu.cp0[12];
  if (status != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_BOUND ||
      result.tick != UINT64_C(8) || machine->cpu.gpr[10] != UINT64_C(0x66) ||
      machine->cpu.gpr[11] != UINT64_C(0x77) ||
      machine->cpu.gpr[12] != UINT64_C(0x4a) ||
      machine->cpu.gpr[13] != UINT64_C(0x55) ||
      machine->cpu.cp0[14] != t64_cpu_canonical_address(
                                  UINT32_C(0x8000100c)) ||
      ((cause >> 2) & UINT32_C(0x1f)) != 0u ||
      (cause & T64_CP0_CAUSE_TIMER_MASK) != 0u ||
      (status_register & UINT32_C(2)) != 0u ||
      (uint32_t)machine->cpu.cp0[9] != UINT32_C(4) ||
      (uint32_t)machine->cpu.cp0[11] != UINT32_C(100)) {
    (void)printf("# timer handler status=%u reason=%u tick=%llu "
                 "retired=%llu pc=%016llx cause=%08x epc=%016llx "
                 "status=%08x count=%08x compare=%08x t2=%016llx "
                 "t3=%016llx t4=%016llx t5=%016llx\n",
                 (unsigned)status, result.reason,
                 (unsigned long long)result.tick,
                 (unsigned long long)machine->cpu.retired_instructions,
                 (unsigned long long)machine->cpu.pc_full, cause,
                 (unsigned long long)machine->cpu.cp0[14], status_register,
                 (uint32_t)machine->cpu.cp0[9],
                 (uint32_t)machine->cpu.cp0[11],
                 (unsigned long long)machine->cpu.gpr[10],
                 (unsigned long long)machine->cpu.gpr[11],
                 (unsigned long long)machine->cpu.gpr[12],
                 (unsigned long long)machine->cpu.gpr[13]);
    return false;
  }
  return true;
}

static bool test_vr4300_integer_delay_fault_and_full_width_jalr(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0x012a402d), /* daddu t0,t1,t2 -> 0x00000001fffffffe */
      UINT32_C(0x010c5821), /* addu t3,t0,t4 -> sign-extended 0xffffffff */
      UINT32_C(0x10000002), /* beq zero,zero,target */
      UINT32_C(0x01ae7820), /* delay slot: add t7,t5,t6 overflows */
      UINT32_C(0x00000000), /* skipped */
      UINT32_C(0x7000003f), /* branch target */
  };
  const uint64_t branch_pc = UINT64_C(0xffffffff80001008);
  const uint64_t target = UINT64_C(0x00000001fffffffe);
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(6)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_status status;
  uint32_t cause;
  uint32_t stored;
  size_t i;

  if (machine == NULL || asset == NULL ||
      t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK) {
    (void)puts("# integer-delay fixture setup failed");
    return false;
  }
  for (i = 0u; i < sizeof program / sizeof program[0]; i++) {
    write_rdram_instruction(machine, UINT32_C(0x1000) + (uint32_t)(i * 4u),
                            program[i]);
  }
  write_rdram_instruction(machine, UINT32_C(0x0180), UINT32_C(0xae0b0000));
  write_rdram_instruction(machine, UINT32_C(0x0184), UINT32_C(0x01004009));
  write_rdram_instruction(machine, UINT32_C(0x0188), UINT32_C(0x00000000));
  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = UINT64_C(0xffffffff80001000);
  machine->cpu.gpr[9] = UINT64_C(0x00000001fffffff0);
  machine->cpu.gpr[10] = UINT64_C(0x000000000000000e);
  machine->cpu.gpr[12] = UINT64_C(1);
  machine->cpu.gpr[13] = UINT64_C(0x7fffffff);
  machine->cpu.gpr[14] = UINT64_C(1);
  machine->cpu.gpr[16] = UINT64_C(0xffffffff80002000);

  status = t64_machine_advance(machine, &request, &result);
  cause = (uint32_t)machine->cpu.cp0[13];
  stored = ((uint32_t)machine->ram[0x2000u] << 24) |
           ((uint32_t)machine->ram[0x2001u] << 16) |
           ((uint32_t)machine->ram[0x2002u] << 8) |
           (uint32_t)machine->ram[0x2003u];
  if (status != T64_STATUS_OK || result.reason != T64_STOP_REASON_BOUND ||
      result.tick != UINT64_C(6) ||
      machine->cpu.retired_instructions != UINT64_C(5) ||
      machine->cpu.gpr[8] != UINT64_C(0xffffffff8000018c) ||
      machine->cpu.gpr[11] != UINT64_C(0xffffffffffffffff) ||
      machine->cpu.cp0[14] != branch_pc ||
      (cause & UINT32_C(0x8000007c)) != UINT32_C(0x80000030) ||
      stored != UINT32_MAX || machine->cpu.pc_full !=
                                  UINT64_C(0xffffffff80000188)) {
    (void)printf("# integer-delay status=%u reason=%u tick=%llu retired=%llu "
                 "t0=%016llx t3=%016llx pc=%016llx cause=%08x "
                 "epc=%016llx store=%08x\n",
                 (unsigned)status, result.reason,
                 (unsigned long long)result.tick,
                 (unsigned long long)machine->cpu.retired_instructions,
                 (unsigned long long)machine->cpu.gpr[8],
                 (unsigned long long)machine->cpu.gpr[11],
                 (unsigned long long)machine->cpu.pc_full, cause,
                 (unsigned long long)machine->cpu.cp0[14], stored);
    return false;
  }
  if (machine->cpu.delay_target_full != target) {
    (void)printf("# jalr source==destination target expected=%016llx "
                 "observed=%016llx link=%016llx slot=%016llx\n",
                 (unsigned long long)target,
                 (unsigned long long)machine->cpu.delay_target_full,
                 (unsigned long long)machine->cpu.gpr[8],
                 (unsigned long long)machine->cpu.pc_full);
    return false;
  }
  return true;
}

static bool test_vr4300_not_taken_likely_annuls_faulting_slot(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0x50080002), /* beql zero,t0,target (not taken) */
      UINT32_C(0x8d4b0000), /* annulled lw t3,0(t2) from unmapped KUSEG */
      UINT32_C(0x240c0007), /* addiu t4,zero,7 */
      UINT32_C(0x7000003f), /* explicit unsupported stop */
  };
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(2)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  size_t i;

  if (machine == NULL || asset == NULL ||
      t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK) {
    (void)puts("# branch-likely fixture setup failed");
    return false;
  }
  for (i = 0u; i < sizeof program / sizeof program[0]; i++) {
    write_rdram_instruction(machine, UINT32_C(0x3000) + (uint32_t)(i * 4u),
                            program[i]);
  }
  machine->cpu.pc = UINT32_C(0x80003000);
  machine->cpu.pc_full = UINT64_C(0xffffffff80003000);
  machine->cpu.gpr[8] = UINT64_C(1);
  machine->cpu.gpr[10] = UINT64_C(0x100);
  machine->cpu.gpr[11] = UINT64_C(0x1234);

  if (t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_BOUND || result.tick != UINT64_C(2) ||
      machine->cpu.retired_instructions != UINT64_C(2) ||
      machine->cpu.gpr[11] != UINT64_C(0x1234) ||
      machine->cpu.gpr[12] != UINT64_C(7) ||
      machine->cpu.pc_full != UINT64_C(0xffffffff8000300c) ||
      machine->cpu.attempt_outcome != T64_CPU_ATTEMPT_RETIRED ||
      ((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x8000007c)) != 0u) {
    (void)printf("# likely-annul reason=%u tick=%llu retired=%llu "
                 "pc=%016llx t3=%016llx t4=%016llx cause=%08x outcome=%u\n",
                 result.reason, (unsigned long long)result.tick,
                 (unsigned long long)machine->cpu.retired_instructions,
                 (unsigned long long)machine->cpu.pc_full,
                 (unsigned long long)machine->cpu.gpr[11],
                 (unsigned long long)machine->cpu.gpr[12],
                 (uint32_t)machine->cpu.cp0[13], machine->cpu.attempt_outcome);
    return false;
  }
  return true;
}

static bool stop_at_unsupported(t64_machine *machine,
                                t64_stop_result *result_out,
                                t64_cpu_stop_detail *detail_out) {
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(100)};

  return t64_machine_advance(machine, &request, result_out) ==
             T64_STATUS_OK &&
         result_out->reason == T64_STOP_REASON_CPU_UNSUPPORTED &&
         t64_machine_cpu_stop_detail(machine, detail_out) == T64_STATUS_OK &&
         result_out->tick == detail_out->retired_instructions + UINT64_C(1);
}

static bool test_real_entry_and_next_boundary(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(2)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};
  t64_status status;

  if (t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK ||
      t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_BOUND || result.tick != UINT64_C(2) ||
      machine->cpu.pc != IPL3_ENTRY_PC + 8u ||
      machine->cpu.retired_instructions != UINT64_C(2) ||
      machine->instruction_count != UINT64_C(2) ||
      machine->cpu.gpr[29] != UINT64_C(0xffffffffa4000000)) {
    (void)printf("# initial entry state pc=0x%08x retired=%llu gpr29=0x%016llx\n",
                 machine->cpu.pc,
                 (unsigned long long)machine->cpu.retired_instructions,
                 (unsigned long long)machine->cpu.gpr[29]);
    return false;
  }
  request.value = UINT64_C(100);
  status = t64_machine_advance(machine, &request, &result);
  if (status != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_BOUND ||
      result.tick != UINT64_C(100) ||
      machine->cpu.retired_instructions != UINT64_C(100) ||
      machine->cpu.pc != UINT32_C(0xa4000354) ||
      machine->cpu.last_pc != UINT32_C(0xa4000350) ||
      machine->cpu.last_opcode != UINT32_C(0x240af000) ||
      t64_machine_cpu_stop_detail(machine, &detail) != T64_STATUS_NOT_READY) {
    (void)printf("# actual boundary status=%u reason=%u tick=%llu retired=%llu "
                 "pc=0x%08x last_pc=0x%08x last_opcode=0x%08x\n",
                 (unsigned)status, result.reason,
                 (unsigned long long)result.tick,
                 (unsigned long long)machine->cpu.retired_instructions,
                 machine->cpu.pc, machine->cpu.last_pc,
                 machine->cpu.last_opcode);
    return false;
  }
  return true;
}

static bool test_integer_sign_extension_and_bal_delay(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0x3c088000), /* lui t0,0x8000 */
      UINT32_C(0x2508ffff), /* addiu t0,t0,-1 */
      UINT32_C(0x01004821), /* addu t1,t0,zero */
      UINT32_C(0x00095040), /* sll t2,t1,1 */
      UINT32_C(0x000a5843), /* sra t3,t2,1 */
      UINT32_C(0x00086078), /* dsll t4,t0,1 */
      UINT32_C(0x000c687b), /* dsra t5,t4,1 */
      UINT32_C(0x04110002), /* bal target */
      UINT32_C(0x240e1234), /* delay slot */
      UINT32_C(0x240e0001), /* skipped by the taken BAL */
      UINT32_C(0x7000003f), /* explicit unsupported stop */
  };
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};

  if (!install_program(machine, asset, program,
                       sizeof program / sizeof program[0])) {
    return false;
  }
  return stop_at_unsupported(machine, &result, &detail) &&
         detail.retired_instructions == UINT64_C(9) &&
         detail.pc == IPL3_ENTRY_PC + UINT32_C(40) &&
         detail.opcode == UINT32_C(0x7000003f) &&
         machine->cpu.gpr[8] == UINT64_C(0x000000007fffffff) &&
         machine->cpu.gpr[9] == UINT64_C(0x000000007fffffff) &&
         machine->cpu.gpr[10] == UINT64_C(0xfffffffffffffffe) &&
         machine->cpu.gpr[11] == UINT64_C(0xffffffffffffffff) &&
         machine->cpu.gpr[12] == UINT64_C(0x00000000fffffffe) &&
         machine->cpu.gpr[13] == UINT64_C(0x000000007fffffff) &&
         machine->cpu.gpr[14] == UINT64_C(0x1234) &&
         machine->cpu.gpr[31] == UINT64_C(0xffffffffa4000064);
}

static bool test_signed_multiply_and_hilo(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0x01090018), /* mult t0,t1 */
      UINT32_C(0x00005012), /* mflo t2 */
      UINT32_C(0x00005810), /* mfhi t3 */
      UINT32_C(0x7000003f), /* explicit unsupported stop */
  };
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};

  if (!install_program(machine, asset, program,
                       sizeof program / sizeof program[0])) {
    return false;
  }
  machine->cpu.gpr[8] = UINT64_C(0xffffffff80000002);
  machine->cpu.gpr[9] = UINT64_C(3);
  if (!stop_at_unsupported(machine, &result, &detail)) {
    return false;
  }
  if (detail.retired_instructions != UINT64_C(3) ||
      detail.pc != IPL3_ENTRY_PC + UINT32_C(12) ||
      machine->cpu.hi != UINT64_C(0xfffffffffffffffe) ||
      machine->cpu.lo != UINT64_C(0xffffffff80000006) ||
      machine->cpu.gpr[10] != machine->cpu.lo ||
      machine->cpu.gpr[11] != machine->cpu.hi) {
    (void)printf("# actual retired=%llu pc=0x%08x opcode=0x%08x "
                 "hi=0x%016llx lo=0x%016llx\n",
                 (unsigned long long)detail.retired_instructions,
                 detail.pc, detail.opcode,
                 (unsigned long long)machine->cpu.hi,
                 (unsigned long long)machine->cpu.lo);
    return false;
  }
  return true;
}

static bool test_beq_delay_slot(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0x10000002), /* beq zero,zero,target */
      UINT32_C(0x24080007), /* delay slot executes */
      UINT32_C(0x24090001), /* skipped by the taken branch */
      UINT32_C(0x7000003f), /* target */
  };
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};

  if (!install_program(machine, asset, program,
                       sizeof program / sizeof program[0])) {
    return false;
  }
  return stop_at_unsupported(machine, &result, &detail) &&
         detail.retired_instructions == UINT64_C(2) &&
         detail.pc == IPL3_ENTRY_PC + UINT32_C(12) &&
         machine->cpu.gpr[8] == UINT64_C(7) &&
         machine->cpu.gpr[9] == UINT64_C(0);
}

static bool test_indirect_jump_and_link(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t jr_program[] = {
      UINT32_C(0x01000008), /* jr t0 */
      UINT32_C(0x240b0007), /* delay slot */
      UINT32_C(0x240c0001), /* skipped */
      UINT32_C(0x7000003f), /* indirect target */
  };
  static const uint32_t jalr_program[] = {
      UINT32_C(0x01004809), /* jalr t1,t0 */
      UINT32_C(0x240b0007), /* delay slot */
      UINT32_C(0x240c0001), /* skipped */
      UINT32_C(0x7000003f), /* indirect target */
  };
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};

  if (!install_program(machine, asset, jr_program,
                       sizeof jr_program / sizeof jr_program[0])) {
    return false;
  }
  machine->cpu.gpr[8] = UINT64_C(0xffffffffa400004c);
  if (!stop_at_unsupported(machine, &result, &detail) ||
      detail.retired_instructions != UINT64_C(2) ||
      detail.pc != IPL3_ENTRY_PC + UINT32_C(12) ||
      machine->cpu.gpr[11] != UINT64_C(7) ||
      machine->cpu.gpr[12] != UINT64_C(0)) {
    return false;
  }

  if (!install_program(machine, asset, jalr_program,
                       sizeof jalr_program / sizeof jalr_program[0])) {
    return false;
  }
  machine->cpu.gpr[8] = UINT64_C(0xffffffffa400004c);
  return stop_at_unsupported(machine, &result, &detail) &&
         detail.retired_instructions == UINT64_C(2) &&
         detail.pc == IPL3_ENTRY_PC + UINT32_C(12) &&
         machine->cpu.gpr[9] == UINT64_C(0xffffffffa4000048) &&
         machine->cpu.gpr[11] == UINT64_C(7) &&
         machine->cpu.gpr[12] == UINT64_C(0);
}

static bool test_kseg_endian_and_partial_memory(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0x3c048000), /* lui a0,0x8000 */
      UINT32_C(0x34840100), /* ori a0,a0,0x100 */
      UINT32_C(0x8c8f0000), /* lw t7,0(a0) */
      UINT32_C(0xac8f0004), /* sw t7,4(a0) */
      UINT32_C(0x3c05a000), /* lui a1,0xa000 */
      UINT32_C(0x34a50100), /* ori a1,a1,0x100 */
      UINT32_C(0x8cb80000), /* lw t8,0(a1), KSEG1 alias */
      UINT32_C(0x50180002), /* beql zero,t8,target (not taken) */
      UINT32_C(0x24190001), /* annulled delay slot */
      UINT32_C(0x24190002), /* executes after not-taken likely branch */
      UINT32_C(0x3c0faabb), /* lui t7,0xaabb */
      UINT32_C(0x35efccdd), /* ori t7,t7,0xccdd */
      UINT32_C(0xa88f0001), /* swl t7,1(a0) */
      UINT32_C(0xb88f0005), /* swr t7,5(a0) */
      UINT32_C(0xb08d0009), /* sdl t5,9(a0) */
      UINT32_C(0xb48d0011), /* sdr t5,17(a0) */
      UINT32_C(0x88880001), /* lwl t0,1(a0) */
      UINT32_C(0x98890002), /* lwr t1,2(a0) */
      UINT32_C(0x7000003f), /* explicit unsupported stop */
  };
  uint32_t kseg0_physical = UINT32_MAX;
  uint32_t kseg1_physical = UINT32_MAX;
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};
  size_t i;

  if (!install_program(machine, asset, program,
                       sizeof program / sizeof program[0])) {
    return false;
  }
  for (i = 0u; i < 24u; i++) {
    machine->ram[0x100u + i] = (uint8_t)i;
  }
  machine->ram[0x100u] = UINT8_C(0x11);
  machine->ram[0x101u] = UINT8_C(0x22);
  machine->ram[0x102u] = UINT8_C(0x33);
  machine->ram[0x103u] = UINT8_C(0x44);
  machine->cpu.gpr[13] = UINT64_C(0x0123456789abcdef);

  if (!t64_cpu_translate(UINT64_C(0xffffffff80000100), &kseg0_physical) ||
      !t64_cpu_translate(UINT64_C(0xffffffffa0000100), &kseg1_physical) ||
      kseg0_physical != UINT32_C(0x100) ||
      kseg1_physical != kseg0_physical ||
      !stop_at_unsupported(machine, &result, &detail)) {
    return false;
  }
  return detail.retired_instructions == UINT64_C(17) &&
         detail.pc == IPL3_ENTRY_PC + UINT32_C(72) &&
         detail.opcode == UINT32_C(0x7000003f) &&
         machine->cpu.gpr[15] == UINT64_C(0xffffffffaabbccdd) &&
         machine->cpu.gpr[24] == UINT64_C(0x11223344) &&
         machine->cpu.gpr[25] == UINT64_C(2) &&
         machine->cpu.gpr[8] == UINT64_C(0xffffffffaabbcc00) &&
         machine->cpu.gpr[9] == UINT64_C(0x00bbcc44) &&
         machine->ram[0x100u] == UINT8_C(0xaa) &&
         machine->ram[0x101u] == UINT8_C(0xbb) &&
         machine->ram[0x102u] == UINT8_C(0xcc) &&
         machine->ram[0x103u] == UINT8_C(0x44) &&
         machine->ram[0x104u] == UINT8_C(0x11) &&
         machine->ram[0x105u] == UINT8_C(0x22) &&
         machine->ram[0x106u] == UINT8_C(0xcc) &&
         machine->ram[0x107u] == UINT8_C(0xdd) &&
         machine->ram[0x108u] == UINT8_C(0x01) &&
         machine->ram[0x109u] == UINT8_C(0x23) &&
         machine->ram[0x10au] == UINT8_C(0x45) &&
         machine->ram[0x10bu] == UINT8_C(0x67) &&
         machine->ram[0x10cu] == UINT8_C(0x89) &&
         machine->ram[0x10du] == UINT8_C(0xab) &&
         machine->ram[0x10eu] == UINT8_C(0xcd) &&
         machine->ram[0x10fu] == UINT8_C(0x0f) &&
         machine->ram[0x116u] == UINT8_C(0xcd) &&
         machine->ram[0x117u] == UINT8_C(0xef);
}

static bool test_unsupported_alignment_and_address_modes(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t misaligned_program[] = {
      UINT32_C(0x8c880001), UINT32_C(0x7000003f)};
  static const uint32_t unsupported_address_program[] = {
      UINT32_C(0x8c880000), UINT32_C(0x7000003f)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};
  uint32_t rejected_output = UINT32_C(0xdeadbeef);

  if (!t64_cpu_translate(UINT64_C(0xffffffff80000100), &rejected_output) ||
      t64_cpu_translate(UINT64_C(0x0000000080000100), &rejected_output) ||
      rejected_output != UINT32_C(0x100)) {
    return false;
  }
  if (!install_program(machine, asset, misaligned_program,
                       sizeof misaligned_program /
                           sizeof misaligned_program[0])) {
    return false;
  }
  machine->cpu.gpr[4] = UINT64_C(0xffffffff80000100);
  if (!stop_at_unsupported(machine, &result, &detail) ||
      detail.retired_instructions != UINT64_C(0) ||
      detail.pc != IPL3_ENTRY_PC ||
      detail.opcode != UINT32_C(0x8c880001)) {
    return false;
  }
  if (!install_program(machine, asset, unsupported_address_program,
                       sizeof unsupported_address_program /
                           sizeof unsupported_address_program[0])) {
    return false;
  }
  machine->cpu.gpr[4] = UINT64_C(0x100);
  return stop_at_unsupported(machine, &result, &detail) &&
         detail.retired_instructions == UINT64_C(0) &&
         detail.pc == IPL3_ENTRY_PC &&
         detail.opcode == UINT32_C(0x8c880000);
}

static bool test_overflow_and_unsupported_opcode(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t addi_program[] = {
      UINT32_C(0x21280001), UINT32_C(0x7000003f)};
  static const uint32_t daddi_program[] = {
      UINT32_C(0x616a0001), UINT32_C(0x7000003f)};
  static const uint32_t unsupported_program[] = {UINT32_C(0x7000003f)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};

  if (!install_program(machine, asset, addi_program,
                       sizeof addi_program / sizeof addi_program[0])) {
    return false;
  }
  machine->cpu.gpr[8] = UINT64_C(0x55);
  machine->cpu.gpr[9] = UINT64_C(0x7fffffff);
  if (!stop_at_unsupported(machine, &result, &detail) ||
      detail.retired_instructions != UINT64_C(0) ||
      detail.pc != IPL3_ENTRY_PC ||
      detail.opcode != UINT32_C(0x21280001) ||
      machine->cpu.gpr[8] != UINT64_C(0x55)) {
    return false;
  }
  if (!install_program(machine, asset, daddi_program,
                       sizeof daddi_program / sizeof daddi_program[0])) {
    return false;
  }
  machine->cpu.gpr[10] = UINT64_C(0x66);
  machine->cpu.gpr[11] = UINT64_C(0x7fffffffffffffff);
  if (!stop_at_unsupported(machine, &result, &detail) ||
      detail.retired_instructions != UINT64_C(0) ||
      detail.pc != IPL3_ENTRY_PC ||
      detail.opcode != UINT32_C(0x616a0001) ||
      machine->cpu.gpr[10] != UINT64_C(0x66)) {
    return false;
  }
  if (!install_program(machine, asset, unsupported_program,
                       sizeof unsupported_program /
                           sizeof unsupported_program[0])) {
    return false;
  }
  return stop_at_unsupported(machine, &result, &detail) &&
         detail.retired_instructions == UINT64_C(0) &&
         detail.pc == IPL3_ENTRY_PC &&
         detail.opcode == UINT32_C(0x7000003f);
}

static bool test_cp0_count_wrap_and_compare(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0x400a4800), /* mfc0 t2,c0_count */
      UINT32_C(0x00000000), /* nop */
      UINT32_C(0x00000000), /* nop */
      UINT32_C(0x7000003f), /* explicit unsupported stop */
  };
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};

  if (!install_program(machine, asset, program,
                       sizeof program / sizeof program[0])) {
    return false;
  }
  machine->cpu.cp0[9] = UINT64_C(0xfffffffe);
  machine->cpu.cp0[11] = UINT64_C(0);
  if (!stop_at_unsupported(machine, &result, &detail)) {
    return false;
  }
  if (detail.retired_instructions != UINT64_C(3) ||
      detail.pc != IPL3_ENTRY_PC + UINT32_C(12) ||
      machine->cpu.gpr[10] != UINT64_C(0xfffffffffffffffe) ||
      machine->cpu.cp0[9] != UINT64_C(1) ||
      (machine->cpu.cp0[13] & T64_CP0_CAUSE_TIMER_MASK) == 0u) {
    (void)printf("# actual retired=%llu pc=0x%08x opcode=0x%08x "
                 "mfc0_t2=0x%016llx count=0x%08llx cause=0x%08llx\n",
                 (unsigned long long)detail.retired_instructions,
                 detail.pc, detail.opcode,
                 (unsigned long long)machine->cpu.gpr[10],
                 (unsigned long long)machine->cpu.cp0[9],
                 (unsigned long long)machine->cpu.cp0[13]);
    return false;
  }
  return true;
}

static bool test_cp0_register_masks(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0x40886800), /* mtc0 t0,c0_cause */
      UINT32_C(0x40899000), /* mtc0 t1,c0_watchlo */
      UINT32_C(0x408ae000), /* mtc0 t2,c0_taglo */
      UINT32_C(0x408be800), /* mtc0 t3,c0_taghi */
      UINT32_C(0x7000003f), /* explicit unsupported stop */
  };
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};

  if (!install_program(machine, asset, program,
                       sizeof program / sizeof program[0])) {
    return false;
  }
  machine->cpu.gpr[8] = UINT64_C(0xffffffff);
  machine->cpu.gpr[9] = UINT64_C(0xffffffff);
  machine->cpu.gpr[10] = UINT64_C(0xffffffff);
  machine->cpu.gpr[11] = UINT64_C(0xffffffff);
  machine->cpu.cp0[13] = T64_CP0_CAUSE_TIMER_MASK;
  return stop_at_unsupported(machine, &result, &detail) &&
         detail.retired_instructions == UINT64_C(4) &&
         machine->cpu.cp0[13] ==
             (T64_CP0_CAUSE_TIMER_MASK | T64_CP0_CAUSE_SOFTWARE_MASK) &&
         machine->cpu.cp0[18] == UINT64_C(0xfffffffb) &&
         machine->cpu.cp0[28] == UINT64_C(0x0fffffc0) &&
         machine->cpu.cp0[29] == UINT64_C(0);
}

static bool test_cp0_compare_write_clears_timer(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0x40885800), /* mtc0 t0,c0_compare */
      UINT32_C(0x7000003f), /* explicit unsupported stop */
  };
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};

  if (!install_program(machine, asset, program,
                       sizeof program / sizeof program[0])) {
    return false;
  }
  machine->cpu.gpr[8] = UINT64_C(7);
  machine->cpu.cp0[13] =
      T64_CP0_CAUSE_TIMER_MASK | T64_CP0_CAUSE_SOFTWARE_MASK;
  return stop_at_unsupported(machine, &result, &detail) &&
         detail.retired_instructions == UINT64_C(1) &&
         machine->cpu.cp0[11] == UINT64_C(7) &&
         machine->cpu.cp0[13] == T64_CP0_CAUSE_SOFTWARE_MASK;
}

static bool test_cache_operations_are_coherent(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const struct {
    uint32_t opcode;
    uint32_t operation;
    uint64_t virtual_base;
    uint32_t physical_address;
  } cache_cases[] = {
      {UINT32_C(0xbd080000), UINT32_C(0x08),
       UINT64_C(0xffffffff80000100), UINT32_C(0x100)}, /* index store tag I */
      {UINT32_C(0xbd090000), UINT32_C(0x09),
       UINT64_C(0xffffffff80000100), UINT32_C(0x100)}, /* index store tag D */
      {UINT32_C(0xbd110000), UINT32_C(0x11),
       UINT64_C(0xffffffff807f8000), UINT32_C(0x007f8000)}, /* hit invalidate D */
      {UINT32_C(0xbd150000), UINT32_C(0x15),
       UINT64_C(0xffffffff80000100), UINT32_C(0x100)}, /* hit WB/invalidate D */
  };
  size_t i;

  for (i = 0u; i < sizeof cache_cases / sizeof cache_cases[0]; i++) {
    uint32_t program[] = {UINT32_C(0xad490000), cache_cases[i].opcode,
                          UINT32_C(0x7000003f)};
    t64_stop_result result = {UINT32_MAX, UINT64_MAX};
    t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};

    if (!install_program(machine, asset, program,
                         sizeof program / sizeof program[0])) {
      return false;
    }
    machine->cpu.gpr[8] = cache_cases[i].virtual_base;
    machine->cpu.gpr[9] = UINT64_C(0x12345678);
    machine->cpu.gpr[10] = UINT64_C(0xffffffff80000100);
    if (!stop_at_unsupported(machine, &result, &detail) ||
        detail.retired_instructions != UINT64_C(2) ||
        detail.pc != IPL3_ENTRY_PC + UINT32_C(8) ||
        machine->cache.policy_version != UINT32_C(1) ||
        machine->cache.last_operation != cache_cases[i].operation ||
        machine->cache.last_pc != IPL3_ENTRY_PC + UINT32_C(4) ||
        machine->cache.last_address != cache_cases[i].physical_address ||
        !machine->cache.operation_seen || machine->ram[0x100u] != 0x12u ||
        machine->ram[0x101u] != 0x34u || machine->ram[0x102u] != 0x56u ||
        machine->ram[0x103u] != 0x78u) {
      (void)printf("# cache case=%zu stop=%llu pc=0x%08x opcode=0x%08x "
                   "op=0x%x cache_pc=0x%08x cache_addr=0x%08x "
                   "seen=%d ram=%02x%02x%02x%02x\n",
                   i, (unsigned long long)detail.retired_instructions,
                   detail.pc, detail.opcode, machine->cache.last_operation,
                   machine->cache.last_pc, machine->cache.last_address,
                   machine->cache.operation_seen ? 1 : 0,
                   machine->ram[0x100u], machine->ram[0x101u],
                   machine->ram[0x102u], machine->ram[0x103u]);
      (void)printf("# gpr8=0x%016llx gpr9=0x%016llx gpr10=0x%016llx "
                   "rdram=%llu tick=%llu bus_read=%d\n",
                   (unsigned long long)machine->cpu.gpr[8],
                   (unsigned long long)machine->cpu.gpr[9],
                   (unsigned long long)machine->cpu.gpr[10],
                   (unsigned long long)machine->profile.rdram_bytes,
                   (unsigned long long)machine->tick,
                   (int)t64_bus_read(machine, UINT32_C(0x100), 4u,
                                     &(uint32_t){0}));
      return false;
    }
  }
  return true;
}

static bool test_cp0_cache_unsupported_encodings(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t unsupported_cp0[] = {
      UINT32_C(0x40004801), /* mfc0 t0,count,sel=1 */
      UINT32_C(0x40880000), /* mtc0 t0,c0_index */
  };
  static const uint32_t cache_cases[] = {
      UINT32_C(0xbd100000), /* undefined/unqualified CACHE operation */
      UINT32_C(0xbd150000), /* Hit_Writeback_Invalidate_D */
  };
  size_t i;

  for (i = 0u; i < sizeof unsupported_cp0 / sizeof unsupported_cp0[0];
       i++) {
    t64_stop_result result = {UINT32_MAX, UINT64_MAX};
    t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};

    if (!install_program(machine, asset, &unsupported_cp0[i], 1u) ||
        !stop_at_unsupported(machine, &result, &detail) ||
        detail.retired_instructions != UINT64_C(0) ||
        detail.pc != IPL3_ENTRY_PC ||
        detail.opcode != unsupported_cp0[i]) {
      return false;
    }
  }
  for (i = 0u; i < sizeof cache_cases / sizeof cache_cases[0]; i++) {
    t64_stop_result result = {UINT32_MAX, UINT64_MAX};
    t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};

    if (!install_program(machine, asset, &cache_cases[i], 1u)) {
      return false;
    }
    machine->cpu.gpr[8] = i == 0u ? UINT64_C(0xffffffff80000100)
                                  : UINT64_C(0x100);
    if (i == 0u) {
      if (!stop_at_unsupported(machine, &result, &detail) ||
          detail.retired_instructions != UINT64_C(0) ||
          detail.pc != IPL3_ENTRY_PC || detail.opcode != cache_cases[i] ||
          machine->cache.operation_seen) {
        return false;
      }
    } else {
      /* The manual specifies virtual-to-physical translation for CACHE
       * operands and allows TLB miss/invalid exceptions. Use ASID 1 so this
       * KUSEG address does not collide with the all-zero invalid seed tags. */
      t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(1)};
      machine->cpu.cp0[10] = UINT64_C(1);
      if (t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
          result.reason != T64_STOP_REASON_BOUND ||
          machine->cpu.pc_full != t64_cpu_canonical_address(
                                      UINT32_C(0x80000000)) ||
          (uint32_t)machine->cpu.cp0[13] >> 2 != UINT32_C(2) ||
          machine->cpu.cp0[8] != UINT64_C(0x100) ||
          machine->cpu.cp0[10] != UINT64_C(0x101) ||
          machine->cpu.retired_instructions != UINT64_C(0) ||
          machine->cache.operation_seen) {
        return false;
      }
    }
  }
  return true;
}

static bool test_software_debug_stop_break(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t completion_program[] = {
      T64_CPU_SOFTWARE_DEBUG_STOP_BREAK,
  };
  static const uint32_t unrecognized_break_program[] = {
      UINT32_C(0x0000004d), /* BREAK with a nonzero code field */
  };
  static const uint32_t delay_slot_program[] = {
      UINT32_C(0x10000001), /* beq zero,zero,+1 */
      T64_CPU_SOFTWARE_DEBUG_STOP_BREAK,
  };
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(100)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};
  t64_trace_record records[16];
  size_t count = 0u;
  uint64_t dropped = UINT64_MAX;
  bool saturated = true;
  bool saw_completion = false;
  bool retired_break = false;
  size_t i;

  if (!install_program(machine, asset, completion_program,
                       sizeof completion_program / sizeof completion_program[0]) ||
      t64_machine_trace_set_enabled(machine, true) != T64_STATUS_OK ||
      t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_GUEST_COMPLETION ||
      result.tick != UINT64_C(1) || machine->cpu.retired_instructions != 0u ||
      machine->cpu.pc != IPL3_ENTRY_PC ||
      t64_machine_cpu_stop_detail(machine, &detail) != T64_STATUS_OK ||
      detail.retired_instructions != 0u || detail.pc != IPL3_ENTRY_PC ||
      detail.opcode != T64_CPU_SOFTWARE_DEBUG_STOP_BREAK ||
      t64_machine_trace_drain(machine, records,
                              sizeof records / sizeof records[0], &count,
                              &dropped, &saturated) != T64_STATUS_OK ||
      dropped != 0u || saturated) {
    return false;
  }
  for (i = 0u; i < count; i++) {
    if (records[i].device == T64_TRACE_DEVICE_MACHINE &&
        records[i].kind == T64_TRACE_KIND_CPU_COMPLETION &&
        records[i].value ==
            (((uint64_t)IPL3_ENTRY_PC << 32) |
             T64_CPU_SOFTWARE_DEBUG_STOP_BREAK)) {
      saw_completion = true;
    }
    if (records[i].device == T64_TRACE_DEVICE_MACHINE &&
        records[i].kind == T64_TRACE_KIND_CPU_RETIREMENT &&
        records[i].value ==
            (((uint64_t)IPL3_ENTRY_PC << 32) |
             T64_CPU_SOFTWARE_DEBUG_STOP_BREAK)) {
      retired_break = true;
    }
  }
  if (!saw_completion || retired_break ||
      !install_program(machine, asset, unrecognized_break_program,
                       sizeof unrecognized_break_program /
                           sizeof unrecognized_break_program[0]) ||
      t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_CPU_UNSUPPORTED ||
      result.tick != UINT64_C(1) ||
      t64_machine_cpu_stop_detail(machine, &detail) != T64_STATUS_OK ||
      detail.retired_instructions != 0u || detail.pc != IPL3_ENTRY_PC ||
      detail.opcode != unrecognized_break_program[0] ||
      !install_program(machine, asset, delay_slot_program,
                       sizeof delay_slot_program / sizeof delay_slot_program[0]) ||
      t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_CPU_UNSUPPORTED ||
      result.tick != UINT64_C(2) ||
      t64_machine_cpu_stop_detail(machine, &detail) != T64_STATUS_OK ||
      detail.retired_instructions != UINT64_C(1) ||
      detail.pc != IPL3_ENTRY_PC + UINT32_C(4) ||
      detail.opcode != T64_CPU_SOFTWARE_DEBUG_STOP_BREAK) {
    return false;
  }
  return true;
}

static bool test_cp0_cache_state_is_in_digest(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  uint8_t baseline[T64_DIGEST_SIZE];
  uint8_t changed[T64_DIGEST_SIZE];
  uint64_t cp0_value;
  t64_cache_state cache_value;
  bool cp0_hashed;
  bool cache_hashed;

  if (t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK ||
      t64_machine_digest(machine, baseline) != T64_STATUS_OK) {
    return false;
  }
  cp0_value = machine->cpu.cp0[9];
  machine->cpu.cp0[9] = cp0_value ^ UINT64_C(1);
  cp0_hashed = t64_machine_digest(machine, changed) == T64_STATUS_OK &&
               memcmp(baseline, changed, sizeof baseline) != 0;
  machine->cpu.cp0[9] = cp0_value;
  cache_value = machine->cache;
  machine->cache.last_operation ^= UINT32_C(1);
  cache_hashed = t64_machine_digest(machine, changed) == T64_STATUS_OK &&
                 memcmp(baseline, changed, sizeof baseline) != 0;
  machine->cache = cache_value;
  return cp0_hashed && cache_hashed;
}

typedef bool (*cpu_test_fn)(t64_machine *machine,
                            const uint8_t asset[IPL3_ASSET_BYTES]);

typedef struct {
  const char *name;
  cpu_test_fn run;
} cpu_test_case;

int main(int argc, char **argv) {
  allocator_state allocator_state_value = {0u};
  const t64_host_allocator allocator = {
      test_allocate, test_release, &allocator_state_value};
  static const cpu_test_case cpu_path_cases[] = {
      {"qualified IPL3 integer path reaches the MI device boundary",
       test_real_entry_and_next_boundary},
      {"MIPS III sign extension, shifts and BAL delay slot",
       test_integer_sign_extension_and_bal_delay},
      {"signed 32-bit multiply writes sign-extended HI/LO words",
       test_signed_multiply_and_hilo},
      {"taken conditional branch retires its delay slot",
       test_beq_delay_slot},
      {"indirect jump and link preserve delay-slot behavior",
       test_indirect_jump_and_link},
      {"KSEG aliases, endian loads and partial load/store lanes",
       test_kseg_endian_and_partial_memory},
      {"alignment and unsupported address modes stop without retirement",
       test_unsupported_alignment_and_address_modes},
      {"guest overflow and unsupported opcodes stop without retirement",
       test_overflow_and_unsupported_opcode},
      {"software debug-stop BREAK is exact, nonretiring and excluded in delay slots",
       test_software_debug_stop_break},
  };
  static const cpu_test_case cp0_cache_cases[] = {
      {"CP0 Count reads before its retirement tick and compares after wrap",
       test_cp0_count_wrap_and_compare},
      {"CP0 Cause, WatchLo, and cache tags apply documented masks",
       test_cp0_register_masks},
      {"MTC0 Compare clears the latched timer interrupt",
       test_cp0_compare_write_clears_timer},
      {"CACHE index tags and D hit writeback preserve coherent memory",
       test_cache_operations_are_coherent},
      {"unknown CP0 selects stop and CACHE operands use TLB translation",
       test_cp0_cache_unsupported_encodings},
      {"CP0 and cache state participate in the v6 digest",
       test_cp0_cache_state_is_in_digest},
  };
  static const cpu_test_case vr4300_cases[] = {
      {"new-profile BREAK schedules a nonretiring RDRAM handler",
       test_vr4300_break_enters_rdram_handler},
      {"mapped load miss is repaired by a guest TLBWI handler and retried",
       test_vr4300_tlb_refill_and_retry},
      {"guest-time Count equality interrupts then resumes after Compare commit",
       test_vr4300_timer_interrupt_handler},
      {"protected CP0 transfers resume a guest handler without destination writes",
       test_vr4300_protected_cp0_guest_handler},
      {"DADDU/ADDU branch fault resumes handler and JALR keeps full target",
       test_vr4300_integer_delay_fault_and_full_width_jalr},
      {"not-taken branch-likely annuls a faulting delay slot",
       test_vr4300_not_taken_likely_annuls_faulting_slot},
  };
  const cpu_test_case *cases;
  size_t case_count;
  size_t existing_case_count;
  bool run_vr4300_exception;
  const t64_profile_descriptor *profile = NULL;
  const t64_profile_descriptor *vr4300_profile = NULL;
  t64_machine *machine = NULL;
  uint8_t asset[IPL3_ASSET_BYTES];
  size_t i;
  size_t failure_count = 0u;

  if (argc != 2 ||
      (strcmp(argv[1], "--cpu-path") != 0 &&
       strcmp(argv[1], "--cp0-cache") != 0)) {
    (void)puts("1..1");
    (void)puts("not ok 1 - select --cpu-path or --cp0-cache");
    return 2;
  }
  if (strcmp(argv[1], "--cp0-cache") == 0) {
    cases = cp0_cache_cases;
    case_count = sizeof cp0_cache_cases / sizeof cp0_cache_cases[0];
    run_vr4300_exception = false;
  } else {
    cases = cpu_path_cases;
    case_count = sizeof cpu_path_cases / sizeof cpu_path_cases[0];
    run_vr4300_exception = true;
  }
  existing_case_count = case_count;
  if (run_vr4300_exception) {
    case_count += sizeof vr4300_cases / sizeof vr4300_cases[0];
  }
  if (!read_qualified_asset(asset) ||
      t64_profile_lookup("ntsc-cart-4m-openipl3-warm-v1", &profile) !=
          T64_STATUS_OK ||
      profile == NULL ||
      t64_machine_create(&allocator, profile, &machine) != T64_STATUS_OK ||
      machine == NULL) {
    (void)puts("1..1");
    (void)puts("not ok 1 - qualified IPL3 fixture setup");
    if (machine != NULL) {
      t64_machine_destroy(machine);
    }
    return 1;
  }
  (void)printf("1..%zu\n", case_count);
  for (i = 0u; i < existing_case_count; i++) {
    bool okay = cases[i].run(machine, asset);

    (void)printf("%s %zu - %s\n", okay ? "ok" : "not ok", i + 1u,
                 cases[i].name);
    if (!okay) {
      failure_count++;
    }
  }
  t64_machine_destroy(machine);
  if (run_vr4300_exception) {
    for (i = 0u; i < sizeof vr4300_cases / sizeof vr4300_cases[0]; i++) {
      t64_machine *vr4300_machine = NULL;
      bool okay = t64_profile_lookup(
                      "ntsc-cart-4m-openipl3-vr4300-v1", &vr4300_profile) ==
                      T64_STATUS_OK &&
                  vr4300_profile != NULL &&
                  t64_machine_create(&allocator, vr4300_profile,
                                     &vr4300_machine) == T64_STATUS_OK &&
                  vr4300_machine != NULL;

      if (okay) {
        okay = vr4300_cases[i].run(vr4300_machine, asset);
      } else {
        (void)puts("# new-profile CPU machine setup failed");
      }
      (void)printf("%s %zu - %s\n", okay ? "ok" : "not ok",
                   existing_case_count + i + 1u, vr4300_cases[i].name);
      if (!okay) {
        failure_count++;
      }
      if (vr4300_machine != NULL) {
        t64_machine_destroy(vr4300_machine);
      }
    }
  }
  if (allocator_state_value.outstanding != 0u) {
    (void)printf("# allocator leak: %zu allocations remain\n",
                 allocator_state_value.outstanding);
    failure_count++;
  }
  return failure_count == 0u ? 0 : 1;
}
