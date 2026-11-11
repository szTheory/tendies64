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
  (void)t64_machine_cpu_stop_detail(machine, &detail);
  if (status != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_CPU_UNSUPPORTED ||
      result.tick != UINT64_C(81) ||
      detail.retired_instructions != UINT64_C(80) ||
      detail.pc != UINT32_C(0xa40002b0) ||
      detail.opcode != UINT32_C(0xac430008)) {
    (void)printf("# actual stop status=%u reason=%u tick=%llu retired=%llu "
                 "pc=0x%08x opcode=0x%08x\n",
                 (unsigned)status, result.reason,
                 (unsigned long long)result.tick,
                 (unsigned long long)detail.retired_instructions, detail.pc,
                 detail.opcode);
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
  } cache_cases[] = {
      {UINT32_C(0xbd080000), UINT32_C(0x08)}, /* index store tag I */
      {UINT32_C(0xbd090000), UINT32_C(0x09)}, /* index store tag D */
      {UINT32_C(0xbd150000), UINT32_C(0x15)}, /* hit WB/invalidate D */
  };
  size_t i;

  for (i = 0u; i < sizeof cache_cases / sizeof cache_cases[0]; i++) {
    uint32_t program[] = {UINT32_C(0xad090000), cache_cases[i].opcode,
                          UINT32_C(0x7000003f)};
    t64_stop_result result = {UINT32_MAX, UINT64_MAX};
    t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};

    if (!install_program(machine, asset, program,
                         sizeof program / sizeof program[0])) {
      return false;
    }
    machine->cpu.gpr[8] = UINT64_C(0xffffffff80000100);
    machine->cpu.gpr[9] = UINT64_C(0x12345678);
    if (!stop_at_unsupported(machine, &result, &detail) ||
        detail.retired_instructions != UINT64_C(2) ||
        detail.pc != IPL3_ENTRY_PC + UINT32_C(8) ||
        machine->cache.policy_version != UINT32_C(1) ||
        machine->cache.last_operation != cache_cases[i].operation ||
        machine->cache.last_pc != IPL3_ENTRY_PC + UINT32_C(4) ||
        machine->cache.last_address != UINT32_C(0x100) ||
        !machine->cache.operation_seen || machine->ram[0x100u] != 0x12u ||
        machine->ram[0x101u] != 0x34u || machine->ram[0x102u] != 0x56u ||
        machine->ram[0x103u] != 0x78u) {
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
  static const uint32_t unsupported_cache[] = {
      UINT32_C(0xbd100000), /* unqualified CACHE operation */
      UINT32_C(0xbd150000), /* valid op with unsupported address mode */
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
  for (i = 0u; i < sizeof unsupported_cache / sizeof unsupported_cache[0];
       i++) {
    t64_stop_result result = {UINT32_MAX, UINT64_MAX};
    t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};

    if (!install_program(machine, asset, &unsupported_cache[i], 1u)) {
      return false;
    }
    machine->cpu.gpr[8] = i == 0u ? UINT64_C(0xffffffff80000100)
                                  : UINT64_C(0x100);
    if (!stop_at_unsupported(machine, &result, &detail) ||
        detail.retired_instructions != UINT64_C(0) ||
        detail.pc != IPL3_ENTRY_PC ||
        detail.opcode != unsupported_cache[i] ||
        machine->cache.operation_seen) {
      return false;
    }
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
      {"unknown CP0 selects and CACHE operations stop without retirement",
       test_cp0_cache_unsupported_encodings},
      {"CP0 and cache state participate in the v6 digest",
       test_cp0_cache_state_is_in_digest},
  };
  const cpu_test_case *cases;
  size_t case_count;
  const t64_profile_descriptor *profile = NULL;
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
  } else {
    cases = cpu_path_cases;
    case_count = sizeof cpu_path_cases / sizeof cpu_path_cases[0];
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
  for (i = 0u; i < case_count; i++) {
    bool okay = cases[i].run(machine, asset);

    (void)printf("%s %zu - %s\n", okay ? "ok" : "not ok", i + 1u,
                 cases[i].name);
    if (!okay) {
      failure_count++;
    }
  }
  t64_machine_destroy(machine);
  if (allocator_state_value.outstanding != 0u) {
    (void)printf("# allocator leak: %zu allocations remain\n",
                 allocator_state_value.outstanding);
    failure_count++;
  }
  return failure_count == 0u ? 0 : 1;
}
