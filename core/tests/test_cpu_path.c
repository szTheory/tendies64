#include "t64_internal.h"
#include "tendies64/t64_machine.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IPL3_ASSET_BYTES 4096u
#define IPL3_ENTRY_PC UINT32_C(0xa4000040)

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

  if (t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK ||
      t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_BOUND || result.tick != UINT64_C(2) ||
      machine->cpu.pc != IPL3_ENTRY_PC + 8u ||
      machine->cpu.retired_instructions != UINT64_C(2) ||
      machine->instruction_count != UINT64_C(2) ||
      machine->cpu.gpr[29] != UINT64_C(0xffffffffa4000000)) {
    return false;
  }
  request.value = UINT64_C(10);
  return t64_machine_advance(machine, &request, &result) == T64_STATUS_OK &&
         result.reason == T64_STOP_REASON_CPU_UNSUPPORTED &&
         result.tick == UINT64_C(10) &&
         t64_machine_cpu_stop_detail(machine, &detail) == T64_STATUS_OK &&
         detail.retired_instructions == UINT64_C(9) &&
         detail.pc == UINT32_C(0xa4000064) &&
         detail.opcode == UINT32_C(0x40044800);
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

typedef bool (*cpu_test_fn)(t64_machine *machine,
                            const uint8_t asset[IPL3_ASSET_BYTES]);

typedef struct {
  const char *name;
  cpu_test_fn run;
} cpu_test_case;

int main(void) {
  allocator_state allocator_state_value = {0u};
  const t64_host_allocator allocator = {
      test_allocate, test_release, &allocator_state_value};
  static const cpu_test_case cases[] = {
      {"qualified IPL3 integer path reaches the CP0 boundary",
       test_real_entry_and_next_boundary},
      {"MIPS III sign extension, shifts and BAL delay slot",
       test_integer_sign_extension_and_bal_delay},
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
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t asset[IPL3_ASSET_BYTES];
  size_t i;
  size_t failure_count = 0u;

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
  (void)printf("1..%zu\n", sizeof cases / sizeof cases[0]);
  for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
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
