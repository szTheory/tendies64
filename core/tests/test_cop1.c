#include "t64_internal.h"
#include "tendies64/t64_machine.h"
#include "t64_test.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define IPL3_ASSET_BYTES 4096u
#define TEST_PC UINT64_C(0xffffffff80001000)
#define STATUS_CU1 UINT64_C(0x20000000)
#define FCR31_CONDITION UINT32_C(0x00800000)

typedef struct { size_t outstanding; } allocator_state;

static bool allocate(void *context, size_t size, size_t alignment,
                     t64_host_storage *storage_out) {
  allocator_state *state = (allocator_state *)context;
  void *data = malloc(size);
  if (state == NULL || storage_out == NULL || data == NULL || alignment == 0u ||
      ((uintptr_t)data % alignment) != 0u) {
    free(data);
    return false;
  }
  storage_out->data = data;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  state->outstanding++;
  return true;
}

static void release(void *context, t64_host_storage storage) {
  allocator_state *state = (allocator_state *)context;
  if (storage.data != NULL) {
    free(storage.data);
    if (state != NULL) state->outstanding--;
  }
}

static bool read_asset(uint8_t bytes[IPL3_ASSET_BYTES]) {
  const char *path = getenv("T64_TEST_IPL3_PATH");
  FILE *file;
  size_t count;
  int extra;
  if (path == NULL || path[0] == '\0') return false;
  file = fopen(path, "rb");
  if (file == NULL) return false;
  count = fread(bytes, 1u, IPL3_ASSET_BYTES, file);
  extra = fgetc(file);
  return fclose(file) == 0 && count == IPL3_ASSET_BYTES && extra == EOF;
}

static void write_instruction(t64_machine *machine, uint32_t address,
                              uint32_t opcode) {
  machine->ram[address] = (uint8_t)(opcode >> 24);
  machine->ram[address + 1u] = (uint8_t)(opcode >> 16);
  machine->ram[address + 2u] = (uint8_t)(opcode >> 8);
  machine->ram[address + 3u] = (uint8_t)opcode;
}

static void replay_fpu_instruction(t64_machine *machine, uint32_t opcode) {
  write_instruction(machine, UINT32_C(0x1000), opcode);
  t64_cpu_icache_hit_invalidate(machine, TEST_PC, UINT32_C(0x1000));
  machine->cpu.pc = (uint32_t)TEST_PC;
  machine->cpu.pc_full = TEST_PC;
  machine->cpu.delay_slot_pending = false;
  machine->cpu.delay_slot_pc = 0u;
  machine->cpu.delay_target = 0u;
  machine->cpu.delay_slot_pc_full = 0u;
  machine->cpu.delay_target_full = 0u;
  machine->cpu.delay_origin_pc_full = 0u;
  machine->cpu.pending_exception_valid = false;
  machine->cpu.stopped = false;
  machine->cpu.stop_reason = 0u;
  machine->cpu.stop_pc = 0u;
  machine->cpu.stop_pc_full = 0u;
  machine->cpu.stop_opcode = 0u;
  machine->cpu.attempt_outcome = T64_CPU_ATTEMPT_NONE;
}

static uint32_t encode_cop1_transfer(uint32_t format, uint32_t rt,
                                    uint32_t fs) {
  return (UINT32_C(0x11) << 26) | ((format & UINT32_C(0x1f)) << 21) |
         ((rt & UINT32_C(0x1f)) << 16) | ((fs & UINT32_C(0x1f)) << 11);
}

static uint32_t encode_cvt_d_w(uint32_t fs, uint32_t fd) {
  return (UINT32_C(0x11) << 26) | (UINT32_C(0x14) << 21) |
         ((fs & UINT32_C(0x1f)) << 11) | ((fd & UINT32_C(0x1f)) << 6) |
         UINT32_C(0x21);
}

static uint32_t encode_cop1_operation(uint32_t format, uint32_t ft,
                                      uint32_t fs, uint32_t fd,
                                      uint32_t function) {
  return (UINT32_C(0x11) << 26) | ((format & UINT32_C(0x1f)) << 21) |
         ((ft & UINT32_C(0x1f)) << 16) | ((fs & UINT32_C(0x1f)) << 11) |
         ((fd & UINT32_C(0x1f)) << 6) | (function & UINT32_C(0x3f));
}

static bool advance_tick(t64_machine *machine, uint64_t tick);

static uint64_t read_fpr_bits(const t64_machine *machine, uint32_t reg,
                              uint32_t width) {
  if (machine == NULL || reg >= 32u) return 0u;
  if (width == 32u) return (uint64_t)(uint32_t)machine->cpu.fpr[reg];
  if (width != 64u) return 0u;
  if ((machine->cpu.cp0[12] & UINT64_C(0x04000000)) != 0u) {
    return machine->cpu.fpr[reg];
  }
  if ((reg & 1u) != 0u || reg >= 31u) return 0u;
  return ((uint64_t)(uint32_t)machine->cpu.fpr[reg + 1u] << 32) |
         (uint64_t)(uint32_t)machine->cpu.fpr[reg];
}

static bool execute_fpu_instruction(t64_machine *machine, uint32_t format,
                                     uint32_t ft, uint32_t fs, uint32_t fd,
                                     uint32_t function) {
  if (machine == NULL) return false;
  write_instruction(machine, UINT32_C(0x1000),
                    encode_cop1_operation(format, ft, fs, fd, function));
  return advance_tick(machine, UINT64_C(1));
}

static t64_machine *make_machine(const t64_host_allocator *allocator,
                                 const t64_profile_descriptor *profile,
                                 const uint8_t *asset) {
  static const uint32_t program[] = {
      UINT32_C(0x45010002), /* BC1T +2 */
      UINT32_C(0x24080001), /* delay: addiu t0,zero,1 */
      UINT32_C(0x240a0003), /* fall-through: addiu t2,zero,3 */
      UINT32_C(0x24090002), /* target: addiu t1,zero,2 */
  };
  t64_machine *machine = NULL;
  size_t index;
  if (t64_machine_create(allocator, profile, &machine) != T64_STATUS_OK ||
      machine == NULL ||
      t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) != T64_STATUS_OK) {
    if (machine != NULL) t64_machine_destroy(machine);
    return NULL;
  }
  machine->cpu.pc = (uint32_t)TEST_PC;
  machine->cpu.pc_full = TEST_PC;
  machine->cpu.cp0[12] |= STATUS_CU1;
  machine->cpu.fcr31 = FCR31_CONDITION;
  for (index = 0u; index < sizeof program / sizeof program[0]; index++) {
    write_instruction(machine, UINT32_C(0x1000) + (uint32_t)(index * 4u),
                      program[index]);
  }
  return machine;
}

static bool advance_tick(t64_machine *machine, uint64_t tick) {
  t64_stop_request request = {T64_STOP_GUEST_TICK, tick};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  return t64_machine_advance(machine, &request, &result) == T64_STATUS_OK &&
         result.reason == T64_STOP_REASON_BOUND && result.tick == tick;
}

static void test_branch_forms(const t64_host_allocator *allocator,
                              const t64_profile_descriptor *profile,
                              const uint8_t *asset) {
  uint32_t suboperation;
  uint32_t condition_value;

  for (suboperation = 0u; suboperation < 4u; suboperation++) {
    for (condition_value = 0u; condition_value < 2u; condition_value++) {
      t64_machine *machine = make_machine(allocator, profile, asset);
      bool likely = (suboperation & UINT32_C(2)) != 0u;
      bool taken = ((suboperation & UINT32_C(1)) != 0u) ==
                   (condition_value != 0u);
      uint32_t branch = UINT32_C(0x45000002) | (suboperation << 16);

      T64_CHECK(machine != NULL);
      if (machine == NULL) continue;
      write_instruction(machine, UINT32_C(0x1000), branch);
      machine->cpu.fcr31 = condition_value != 0u ? FCR31_CONDITION : 0u;
      if (likely && !taken) {
        write_instruction(machine, UINT32_C(0x1004), UINT32_C(0x0000000c));
      }
      T64_CHECK(advance_tick(machine, UINT64_C(1)));
      T64_CHECK(machine->cpu.attempt_outcome == T64_CPU_ATTEMPT_RETIRED);
      T64_CHECK(machine->cpu.cp0[13] == UINT64_C(0));
      T64_CHECK((machine->cpu.fcr31 & FCR31_CONDITION) ==
                (condition_value != 0u ? FCR31_CONDITION : 0u));
      if (likely && !taken) {
        T64_CHECK(machine->cpu.pc_full == TEST_PC + UINT64_C(8));
        T64_CHECK(!machine->cpu.delay_slot_pending);
        T64_CHECK(advance_tick(machine, UINT64_C(2)));
        T64_CHECK(machine->cpu.cp0[13] == UINT64_C(0));
        T64_CHECK(machine->cpu.pc_full == TEST_PC + UINT64_C(12));
        T64_CHECK(machine->cpu.retired_instructions == UINT64_C(2));
      } else {
        uint64_t expected_target = TEST_PC +
            (taken ? UINT64_C(12) : UINT64_C(8));
        T64_CHECK(machine->cpu.pc_full == TEST_PC + UINT64_C(4));
        T64_CHECK(machine->cpu.delay_slot_pending);
        T64_CHECK(machine->cpu.delay_target_full == expected_target);
        T64_CHECK(advance_tick(machine, UINT64_C(2)));
        T64_CHECK(machine->cpu.gpr[8] == UINT64_C(1));
        T64_CHECK(machine->cpu.pc_full == expected_target);
        T64_CHECK(advance_tick(machine, UINT64_C(3)));
        if (taken) {
          T64_CHECK(machine->cpu.gpr[9] == UINT64_C(2));
          T64_CHECK(machine->cpu.pc_full == expected_target + UINT64_C(4));
        } else {
          T64_CHECK(machine->cpu.gpr[10] == UINT64_C(3));
          T64_CHECK(machine->cpu.pc_full == expected_target + UINT64_C(4));
        }
      }
      t64_machine_destroy(machine);
    }
  }
}

static void test_branch_exceptions(const t64_host_allocator *allocator,
                                   const t64_profile_descriptor *profile,
                                   const uint8_t *asset) {
  t64_machine *machine = make_machine(allocator, profile, asset);

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  write_instruction(machine, UINT32_C(0x1004), UINT32_C(0x0000000c));
  T64_CHECK(advance_tick(machine, UINT64_C(1)));
  T64_CHECK(advance_tick(machine, UINT64_C(2)));
  T64_CHECK(((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x8000007c)) ==
            UINT32_C(0x80000020)); /* syscall with BD */
  T64_CHECK(machine->cpu.cp0[14] == TEST_PC);
  T64_CHECK((machine->cpu.fcr31 & FCR31_CONDITION) != 0u);
  t64_machine_destroy(machine);

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  write_instruction(machine, UINT32_C(0x1000), UINT32_C(0x45040002));
  T64_CHECK(advance_tick(machine, UINT64_C(1)));
  T64_CHECK(((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x8000007c)) ==
            UINT32_C(0x00000028)); /* reserved instruction */
  T64_CHECK(machine->cpu.cp0[14] == TEST_PC);
  t64_machine_destroy(machine);

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] &= ~STATUS_CU1;
  T64_CHECK(advance_tick(machine, UINT64_C(1)));
  T64_CHECK(((uint32_t)machine->cpu.cp0[13] & UINT32_C(0xf000007c)) ==
            UINT32_C(0x1000002c)); /* CE=1, coprocessor unusable */
  T64_CHECK(machine->cpu.cp0[14] == TEST_PC);
  t64_machine_destroy(machine);

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] &= ~STATUS_CU1;
  machine->cpu.gpr[8] = TEST_PC + UINT64_C(1);
  write_instruction(machine, UINT32_C(0x1000),
                    (UINT32_C(0x35) << 26) | (UINT32_C(8) << 21) |
                        (UINT32_C(2) << 16)); /* misaligned LDC1 */
  T64_CHECK(advance_tick(machine, UINT64_C(1)));
  T64_CHECK(((uint32_t)machine->cpu.cp0[13] & UINT32_C(0xf000007c)) ==
            UINT32_C(0x1000002c)); /* CU1 takes priority over AdEL */
  T64_CHECK(machine->cpu.cp0[8] == UINT64_C(0));
  t64_machine_destroy(machine);

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  write_instruction(machine, UINT32_C(0x1000), UINT32_C(0x10000002));
  write_instruction(machine, UINT32_C(0x1004), UINT32_C(0x45010002));
  machine->cpu.cp0[12] &= ~STATUS_CU1;
  T64_CHECK(advance_tick(machine, UINT64_C(1)));
  T64_CHECK(advance_tick(machine, UINT64_C(2)));
  T64_CHECK(((uint32_t)machine->cpu.cp0[13] & UINT32_C(0xf000007c)) ==
            UINT32_C(0x9000002c)); /* CU1 unusable in a delay slot */
  T64_CHECK(machine->cpu.cp0[14] == TEST_PC);
  t64_machine_destroy(machine);
}

static void test_control_registers(const t64_host_allocator *allocator,
                                   const t64_profile_descriptor *profile,
                                   const uint8_t *asset) {
  const uint32_t writable_fields = UINT32_C(0x0180007f);
  t64_machine *machine = make_machine(allocator, profile, asset);
  uint32_t pair_index;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  write_instruction(machine, UINT32_C(0x1000),
                    encode_cop1_transfer(6u, 8u, 31u)); /* CTC1 t0,FCR31 */
  write_instruction(machine, UINT32_C(0x1004),
                    encode_cop1_transfer(2u, 9u, 31u)); /* CFC1 t1,FCR31 */
  write_instruction(machine, UINT32_C(0x1008),
                    encode_cop1_transfer(2u, 10u, 0u)); /* CFC1 t2,FCR0 */
  write_instruction(machine, UINT32_C(0x100c),
                    encode_cop1_transfer(6u, 11u, 0u)); /* CTC1 t3,FCR0 */
  write_instruction(machine, UINT32_C(0x1010),
                    encode_cop1_transfer(2u, 12u, 0u)); /* CFC1 t4,FCR0 */
  machine->cpu.gpr[8] = writable_fields;
  machine->cpu.gpr[11] = UINT64_C(0xffffffff);
  T64_CHECK(advance_tick(machine, UINT64_C(1)));
  T64_CHECK(machine->cpu.fcr31 == writable_fields);
  T64_CHECK(advance_tick(machine, UINT64_C(2)));
  T64_CHECK(machine->cpu.gpr[9] == (uint64_t)writable_fields);
  T64_CHECK(advance_tick(machine, UINT64_C(3)));
  T64_CHECK(machine->cpu.gpr[10] == UINT64_C(0x00000b00));
  T64_CHECK(advance_tick(machine, UINT64_C(4)));
  T64_CHECK(machine->cpu.fcr0 == UINT32_C(0x00000b00));
  T64_CHECK(advance_tick(machine, UINT64_C(5)));
  T64_CHECK(machine->cpu.gpr[12] == UINT64_C(0x00000b00));
  T64_CHECK(machine->cpu.fcr31 == writable_fields);
  t64_machine_destroy(machine);

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  write_instruction(machine, UINT32_C(0x1000),
                    encode_cop1_transfer(6u, 8u, 31u));
  machine->cpu.gpr[8] = UINT64_C(0xffffffff); /* all defined fields set */
  T64_CHECK(advance_tick(machine, UINT64_C(1)));
  T64_CHECK(((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x8000007c)) ==
            UINT32_C(0x0000003c)); /* floating-point exception */
  T64_CHECK(machine->cpu.cp0[14] == TEST_PC);
  T64_CHECK(machine->cpu.fcr31 == UINT32_C(0x0183ffff));
  t64_machine_destroy(machine);

  for (pair_index = 0u; pair_index < 5u; pair_index++) {
    uint32_t cause = UINT32_C(1) << (12u + pair_index);
    uint32_t enable = UINT32_C(1) << (7u + pair_index);
    uint32_t value = cause | enable;

    machine = make_machine(allocator, profile, asset);
    T64_CHECK(machine != NULL);
    if (machine == NULL) continue;
    write_instruction(machine, UINT32_C(0x1000),
                      encode_cop1_transfer(6u, 8u, 31u));
    machine->cpu.gpr[8] = value;
    T64_CHECK(advance_tick(machine, UINT64_C(1)));
    T64_CHECK(((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x8000007c)) ==
              UINT32_C(0x0000003c));
    T64_CHECK(machine->cpu.cp0[14] == TEST_PC);
    T64_CHECK(machine->cpu.fcr31 == value);
    t64_machine_destroy(machine);
  }
}

static void test_control_register_walking_bits(
    const t64_host_allocator *allocator,
    const t64_profile_descriptor *profile, const uint8_t *asset) {
  const uint32_t allowed = UINT32_C(0x0183ffff);
  uint32_t bit;

  for (bit = 0u; bit < 32u; bit++) {
    t64_machine *machine = make_machine(allocator, profile, asset);
    uint32_t write_value = UINT32_C(1) << bit;
    uint32_t expected = write_value & allowed;

    T64_CHECK(machine != NULL);
    if (machine == NULL) continue;
    write_instruction(machine, UINT32_C(0x1000),
                      encode_cop1_transfer(6u, 8u, 31u)); /* CTC1 */
    write_instruction(machine, UINT32_C(0x1004),
                      encode_cop1_transfer(2u, 9u, 31u)); /* CFC1 */
    machine->cpu.gpr[8] = (uint64_t)write_value;
    if (bit == 17u) {
      write_instruction(machine, UINT32_C(0x0180),
                        encode_cop1_transfer(2u, 9u, 31u)); /* FPE handler */
    }
    T64_CHECK(advance_tick(machine, UINT64_C(1)));
    T64_CHECK(machine->cpu.fcr31 == expected);
    if (bit == 17u) {
      T64_CHECK(((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x7c)) ==
                UINT32_C(0x3c)); /* E always traps */
      T64_CHECK(machine->cpu.cp0[14] == TEST_PC);
    } else {
      T64_CHECK(machine->cpu.cp0[13] == UINT64_C(0));
    }
    T64_CHECK(advance_tick(machine, UINT64_C(2)));
    T64_CHECK(machine->cpu.gpr[9] == (uint64_t)expected);
    t64_machine_destroy(machine);
  }
}

static void test_register_lanes(const t64_host_allocator *allocator,
                                const t64_profile_descriptor *profile,
                                const uint8_t *asset) {
  t64_machine *machine = make_machine(allocator, profile, asset);
  uint32_t reg;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] |= UINT64_C(0x04000000); /* FR=1 independent lanes */
  for (reg = 0u; reg < 32u; reg++) {
    machine->cpu.fpr[reg] = UINT64_C(0xa5a5a5a500000000);
  }
  for (reg = 0u; reg < 32u; reg++) {
    uint32_t word = UINT32_C(0x60000000) + reg;
    t64_cop1_commit_fpr(machine, reg, 32u, word);
    T64_CHECK(machine->cpu.fpr[reg] ==
              (UINT64_C(0xa5a5a5a500000000) | (uint64_t)word));
  }
  for (reg = 0u; reg < 32u; reg++) {
    uint64_t bits = UINT64_C(0x1122334400000000) | (uint64_t)reg;
    t64_cop1_commit_fpr(machine, reg, 64u, bits);
    T64_CHECK(machine->cpu.fpr[reg] == bits);
  }

  machine->cpu.cp0[12] &= ~UINT64_C(0x04000000); /* FR=0 paired words */
  for (reg = 0u; reg < 32u; reg++) {
    machine->cpu.fpr[reg] = UINT64_C(0xa5a5a5a500000000);
  }
  for (reg = 0u; reg < 32u; reg++) {
    uint32_t word = UINT32_C(0x70000000) + reg;
    t64_cop1_commit_fpr(machine, reg, 32u, word);
    T64_CHECK(machine->cpu.fpr[reg] ==
              (UINT64_C(0xa5a5a5a500000000) | (uint64_t)word));
  }
  for (reg = 0u; reg < 32u; reg += 2u) {
    uint64_t bits = UINT64_C(0x5566778800000000) | (uint64_t)reg;
    uint64_t low_expected = UINT64_C(0xa5a5a5a500000000) |
                            (uint64_t)(uint32_t)bits;
    uint64_t high_expected = UINT64_C(0xa5a5a5a500000000) |
                             (uint64_t)(uint32_t)(bits >> 32);
    t64_cop1_commit_fpr(machine, reg, 64u, bits);
    T64_CHECK(machine->cpu.fpr[reg] == low_expected);
    T64_CHECK(machine->cpu.fpr[reg + 1u] == high_expected);
  }
  t64_machine_destroy(machine);

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] &= ~UINT64_C(0x04000000);
  machine->cpu.gpr[3] = UINT64_C(0xdeadbeef12345678);
  machine->cpu.fpr[1] = UINT64_C(0x1122334455667788);
  write_instruction(machine, UINT32_C(0x1000),
                    encode_cop1_transfer(1u, 3u, 1u)); /* odd DMFC1 in FR=0 */
  T64_CHECK(advance_tick(machine, UINT64_C(1)));
  T64_CHECK(((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x8000007c)) ==
            UINT32_C(0x0000003c));
  T64_CHECK(machine->cpu.gpr[3] == UINT64_C(0xdeadbeef12345678));
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) ==
            UINT32_C(0x00020000));
  t64_machine_destroy(machine);
}

static void test_integer_fpr_transfers(const t64_host_allocator *allocator,
                                      const t64_profile_descriptor *profile,
                                      const uint8_t *asset) {
  t64_machine *machine = make_machine(allocator, profile, asset);

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] &= ~UINT64_C(0x04000000); /* FR=0 paired view */
  machine->cpu.fpr[5] = UINT64_C(0xdeadbeef80000001);
  machine->cpu.fpr[2] = UINT64_C(0x1111111100000000) | UINT64_C(0x12345678);
  machine->cpu.fpr[3] = UINT64_C(0x2222222280000000);
  machine->cpu.fpr[4] = UINT64_C(0x3333333300000000);
  machine->cpu.gpr[15] = UINT64_C(0xaabbccddeeff0011);
  write_instruction(machine, UINT32_C(0x1000),
                    encode_cop1_transfer(0u, 13u, 5u)); /* MFC1 r13,f5 */
  write_instruction(machine, UINT32_C(0x1004),
                    encode_cop1_transfer(1u, 14u, 2u)); /* DMFC1 r14,f2 */
  write_instruction(machine, UINT32_C(0x1008),
                    encode_cop1_transfer(5u, 15u, 4u)); /* DMTC1 r15,f4 */
  write_instruction(machine, UINT32_C(0x100c),
                    encode_cop1_transfer(2u, 16u, 1u)); /* invalid CFC1 select */
  T64_CHECK(advance_tick(machine, UINT64_C(1)));
  T64_CHECK(machine->cpu.gpr[13] == UINT64_C(0xffffffff80000001));
  T64_CHECK(advance_tick(machine, UINT64_C(2)));
  T64_CHECK(machine->cpu.gpr[14] == UINT64_C(0x8000000012345678));
  T64_CHECK(advance_tick(machine, UINT64_C(3)));
  T64_CHECK((uint32_t)machine->cpu.fpr[4] == UINT32_C(0xeeff0011));
  T64_CHECK((uint32_t)machine->cpu.fpr[5] == UINT32_C(0xaabbccdd));
  T64_CHECK(advance_tick(machine, UINT64_C(4)));
  T64_CHECK(((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x8000007c)) ==
            UINT32_C(0x00000028)); /* invalid CFC1 select */
  T64_CHECK(machine->cpu.gpr[16] == UINT64_C(0));
  t64_machine_destroy(machine);
}

static void test_decoded_all_register_transfers(
    const t64_host_allocator *allocator,
    const t64_profile_descriptor *profile, const uint8_t *asset) {
  uint32_t mode;

  for (mode = 0u; mode < 2u; mode++) {
    t64_machine *machine = make_machine(allocator, profile, asset);
    uint64_t tick = 0u;
    uint32_t reg;

    T64_CHECK(machine != NULL);
    if (machine == NULL) continue;
    if (mode != 0u) {
      machine->cpu.cp0[12] |= UINT64_C(0x04000000);
    } else {
      machine->cpu.cp0[12] &= ~UINT64_C(0x04000000);
    }
    for (reg = 0u; reg < 32u; reg++) {
      machine->cpu.fpr[reg] = UINT64_C(0xa5a5a5a500000000);
      write_instruction(machine,
                        UINT32_C(0x1000) + reg * UINT32_C(8),
                        encode_cop1_transfer(4u, 8u, reg)); /* MTC1 */
      write_instruction(machine,
                        UINT32_C(0x1004) + reg * UINT32_C(8),
                        encode_cop1_transfer(0u, 9u, reg)); /* MFC1 */
    }
    for (reg = 0u; reg < 32u; reg++) {
      uint32_t word = UINT32_C(0x80000000) | reg;
      uint64_t expected = UINT64_C(0xffffffff00000000) | (uint64_t)word;

      machine->cpu.gpr[8] = (uint64_t)word;
      tick++;
      T64_CHECK(advance_tick(machine, tick));
      T64_CHECK(machine->cpu.fpr[reg] ==
                (UINT64_C(0xa5a5a5a500000000) | (uint64_t)word));
      tick++;
      T64_CHECK(advance_tick(machine, tick));
      T64_CHECK(machine->cpu.gpr[9] == expected);
    }
    t64_machine_destroy(machine);
  }
}

static void test_memory_alignment_faults(const t64_host_allocator *allocator,
                                         const t64_profile_descriptor *profile,
                                         const uint8_t *asset) {
  t64_machine *machine = make_machine(allocator, profile, asset);
  uint64_t address = TEST_PC + UINT64_C(1);

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.gpr[8] = address;
  machine->cpu.fpr[2] = UINT64_C(0x1122334455667788);
  write_instruction(machine, UINT32_C(0x1000),
                    (UINT32_C(0x35) << 26) | (UINT32_C(8) << 21) |
                        (UINT32_C(2) << 16)); /* misaligned LDC1 */
  T64_CHECK(advance_tick(machine, UINT64_C(1)));
  T64_CHECK(((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x7c)) ==
            UINT32_C(0x10)); /* AdEL */
  T64_CHECK(machine->cpu.cp0[8] == address);
  T64_CHECK(machine->cpu.fpr[2] == UINT64_C(0x1122334455667788));
  t64_machine_destroy(machine);

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.gpr[8] = address;
  machine->cpu.fpr[2] = UINT64_C(0x1122334455667788);
  write_instruction(machine, UINT32_C(0x1000),
                    (UINT32_C(0x3d) << 26) | (UINT32_C(8) << 21) |
                        (UINT32_C(2) << 16)); /* misaligned SDC1 */
  T64_CHECK(advance_tick(machine, UINT64_C(1)));
  T64_CHECK(((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x7c)) ==
            UINT32_C(0x14)); /* AdES */
  T64_CHECK(machine->cpu.cp0[8] == address);
  T64_CHECK(machine->cpu.fpr[2] == UINT64_C(0x1122334455667788));
  t64_machine_destroy(machine);
}

static void test_singleword_memory_operations(
    const t64_host_allocator *allocator,
    const t64_profile_descriptor *profile, const uint8_t *asset) {
  t64_machine *machine = make_machine(allocator, profile, asset);
  uint32_t line_index = (UINT32_C(0x2000) >> 4) &
                        (T64_VR4300_DCACHE_LINE_COUNT - UINT32_C(1));
  t64_vr4300_dcache_line *line;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.gpr[8] = UINT64_C(0xffffffff80002000);
  machine->cpu.fpr[2] = UINT64_C(0xaabbccdd00000000);
  machine->ram[0x2000] = UINT8_C(0x80);
  machine->ram[0x2001] = UINT8_C(0x12);
  machine->ram[0x2002] = UINT8_C(0x34);
  machine->ram[0x2003] = UINT8_C(0x56);
  machine->ram[0x2004] = UINT8_C(0xde);
  machine->ram[0x2005] = UINT8_C(0xad);
  machine->ram[0x2006] = UINT8_C(0xbe);
  machine->ram[0x2007] = UINT8_C(0xef);
  write_instruction(machine, UINT32_C(0x1000),
                    (UINT32_C(0x31) << 26) | (UINT32_C(8) << 21) |
                        (UINT32_C(2) << 16)); /* LWC1 f2,0(t0) */
  write_instruction(machine, UINT32_C(0x1004),
                    (UINT32_C(0x39) << 26) | (UINT32_C(8) << 21) |
                        (UINT32_C(2) << 16) | UINT32_C(4)); /* SWC1 f2,4(t0) */
  T64_CHECK(advance_tick(machine, UINT64_C(1)));
  T64_CHECK(machine->cpu.fpr[2] == UINT64_C(0xaabbccdd80123456));
  T64_CHECK(advance_tick(machine, UINT64_C(2)));
  line = &machine->cpu.dcache[line_index];
  T64_CHECK(line->valid && line->dirty);
  T64_CHECK(line->data[4] == UINT8_C(0x80));
  T64_CHECK(line->data[5] == UINT8_C(0x12));
  T64_CHECK(line->data[6] == UINT8_C(0x34));
  T64_CHECK(line->data[7] == UINT8_C(0x56));
  T64_CHECK(machine->ram[0x2004] == UINT8_C(0xde));
  T64_CHECK(machine->ram[0x2007] == UINT8_C(0xef));
  t64_machine_destroy(machine);
}

static void clear_tlb(t64_machine *machine) {
  uint32_t index;
  for (index = 0u; index < T64_VR4300_TLB_ENTRY_COUNT; index++) {
    machine->cpu.tlb[index] = (t64_vr4300_tlb_pair){0};
  }
}

static uint32_t make_entry_lo(uint64_t physical_base, bool dirty) {
  return (uint32_t)(((physical_base >> 12) << 6) & UINT64_C(0x03ffffc0)) |
         UINT32_C(0x1a) | (dirty ? UINT32_C(0x04) : UINT32_C(0));
}

static void map_user_page(t64_machine *machine, uint64_t virtual_address,
                          uint64_t physical_base, bool dirty) {
  t64_vr4300_tlb_pair *entry = &machine->cpu.tlb[0];
  entry->vpn2 = virtual_address & UINT64_C(0x000000ffffffe000);
  entry->region = (virtual_address >> 62) & UINT64_C(3);
  entry->asid = 0u;
  entry->entry_lo0 = make_entry_lo(physical_base, dirty);
  entry->entry_lo1 = make_entry_lo(physical_base + UINT64_C(0x1000), dirty);
}

static void test_tlb_permission_and_cache_failures(
    const t64_host_allocator *allocator,
    const t64_profile_descriptor *profile, const uint8_t *asset) {
  const uint64_t virtual_address = UINT64_C(0x00400000);
  t64_machine *machine = make_machine(allocator, profile, asset);

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  clear_tlb(machine);
  machine->cpu.cp0[12] = STATUS_CU1; /* kernel, CU1 on; kuseg via TLB */
  machine->cpu.cp0[10] = 0u;
  machine->cpu.gpr[8] = virtual_address;
  machine->cpu.fpr[2] = UINT64_C(0x1122334455667788);
  write_instruction(machine, UINT32_C(0x1000),
                    (UINT32_C(0x35) << 26) | (UINT32_C(8) << 21) |
                        (UINT32_C(2) << 16)); /* LDC1, unmapped page */
  T64_CHECK(advance_tick(machine, UINT64_C(1)));
  T64_CHECK(((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x7c)) ==
            UINT32_C(0x08)); /* TLBL refill */
  T64_CHECK(machine->cpu.cp0[8] == virtual_address);
  T64_CHECK(machine->cpu.fpr[2] == UINT64_C(0x1122334455667788));
  t64_machine_destroy(machine);

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  clear_tlb(machine);
  machine->cpu.cp0[12] = STATUS_CU1;
  machine->cpu.cp0[10] = 0u;
  machine->cpu.gpr[8] = virtual_address;
  machine->cpu.fpr[2] = UINT64_C(0x1122334455667788);
  map_user_page(machine, virtual_address, UINT64_C(0x2000), false);
  write_instruction(machine, UINT32_C(0x1000),
                    (UINT32_C(0x3d) << 26) | (UINT32_C(8) << 21) |
                        (UINT32_C(2) << 16)); /* SDC1, clean mapping */
  T64_CHECK(advance_tick(machine, UINT64_C(1)));
  T64_CHECK(((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x7c)) ==
            UINT32_C(0x04)); /* TLB Modified */
  T64_CHECK(machine->cpu.cp0[8] == virtual_address);
  T64_CHECK(machine->ram[0x2000] == UINT8_C(0));
  t64_machine_destroy(machine);

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  clear_tlb(machine);
  machine->cpu.cp0[12] = STATUS_CU1;
  machine->cpu.cp0[10] = 0u;
  machine->cpu.gpr[8] = virtual_address;
  machine->cpu.fpr[2] = UINT64_C(0x1122334455667788);
  map_user_page(machine, virtual_address, UINT64_C(0x20000000), false);
  write_instruction(machine, UINT32_C(0x1000),
                    (UINT32_C(0x35) << 26) | (UINT32_C(8) << 21) |
                        (UINT32_C(2) << 16)); /* LDC1, out-of-RDRAM PA */
  {
    t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(1)};
    t64_stop_result result = {UINT32_MAX, UINT64_MAX};
    uint32_t line_index =
        (UINT32_C(0x20000000) >> 4) &
        (T64_VR4300_DCACHE_LINE_COUNT - UINT32_C(1));

    T64_CHECK(t64_machine_advance(machine, &request, &result) ==
              T64_STATUS_OK);
    T64_CHECK(result.reason == T64_STOP_REASON_CPU_UNSUPPORTED);
    T64_CHECK(machine->cpu.attempt_outcome ==
              T64_CPU_ATTEMPT_MODEL_UNSUPPORTED);
    T64_CHECK(!machine->cpu.dcache[line_index].valid);
  }
  T64_CHECK(machine->cpu.fpr[2] == UINT64_C(0x1122334455667788));
  t64_machine_destroy(machine);
}

static void test_cause_and_sticky_flag_ownership(
    const t64_host_allocator *allocator,
    const t64_profile_descriptor *profile, const uint8_t *asset) {
  t64_machine *machine = make_machine(allocator, profile, asset);

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] &= ~UINT64_C(0x04000000); /* FR=0 pair */
  machine->cpu.fpr[0] = UINT64_C(0x80000000);
  machine->cpu.fpr[2] = UINT64_C(0x1122334400000001);
  machine->cpu.fpr[3] = UINT64_C(0x5566778800000002);
  machine->cpu.fcr31 = UINT32_C(0x00010027); /* V cause, O/I flags, RM=3 */
  write_instruction(machine, UINT32_C(0x1000), encode_cvt_d_w(0u, 2u));
  T64_CHECK(advance_tick(machine, UINT64_C(1)));
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) == UINT32_C(0));
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0000007f)) == UINT32_C(0x27));
  T64_CHECK((uint32_t)machine->cpu.fpr[2] == UINT32_C(0x00000000));
  T64_CHECK((uint32_t)machine->cpu.fpr[3] == UINT32_C(0xc1e00000));
  t64_machine_destroy(machine);
}

static void test_compat_v2_fr0_odd_register_aliases(
    const t64_host_allocator *allocator,
    const uint8_t *asset) {
  static const char *baseline_profile_ids[] = {
      "ntsc-cart-4m-openipl3-vr4300-v1",
      "ntsc-cart-4m-openipl3-vr4300-compat-v1",
  };
  static const uint32_t baseline_odd_opcodes[] = {
      (UINT32_C(0x11) << 26) | (UINT32_C(1) << 21) |
          (UINT32_C(3) << 16) | (UINT32_C(1) << 11), /* odd DMFC1 */
      (UINT32_C(0x11) << 26) | (UINT32_C(17) << 21) |
          (UINT32_C(30) << 16) | (UINT32_C(29) << 11) |
          (UINT32_C(1) << 6), /* ADD.D with odd indices */
      (UINT32_C(0x11) << 26) | (UINT32_C(21) << 21) |
          (UINT32_C(25) << 11) | (UINT32_C(3) << 6) | UINT32_C(0x21),
  };
  const t64_profile_descriptor *v2_profile = NULL;
  t64_machine *machine;
  size_t index;
  uint32_t odd_add_opcode =
      encode_cop1_operation(17u, 30u, 29u, 1u, 0u);
  uint32_t odd_cvt_d_l_opcode =
      encode_cop1_operation(21u, 0u, 25u, 3u, 0x21u);
  uint32_t odd_cvt_d_s_opcode =
      encode_cop1_operation(16u, 0u, 27u, 5u, 0x21u);
  uint32_t odd_cvt_s_w_opcode =
      encode_cop1_operation(20u, 0u, 9u, 5u, 0x20u);

  T64_CHECK(t64_profile_lookup("ntsc-cart-4m-openipl3-vr4300-compat-v2",
                               &v2_profile) == T64_STATUS_OK);
  T64_CHECK(v2_profile != NULL);
  if (v2_profile == NULL) return;

  machine = make_machine(allocator, v2_profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] &= ~UINT64_C(0x04000000); /* FR=0 paired words */
  machine->cpu.fpr[28] = UINT32_C(0x00000000);
  machine->cpu.fpr[29] = UINT32_C(0xc0240000); /* high word of -10.0 */
  machine->cpu.fpr[30] = UINT32_C(0x00000000);
  machine->cpu.fpr[31] = UINT32_C(0x40300000); /* high word of 16.0 */
  replay_fpu_instruction(machine, odd_add_opcode);
  T64_CHECK(advance_tick(machine, UINT64_C(1)));
  T64_CHECK(machine->cpu.last_pc_full == TEST_PC);
  T64_CHECK(machine->cpu.last_opcode == odd_add_opcode);
  (void)fprintf(stderr,
                "# compat-v2 first odd ADD.D attempt pc=0x%016llx "
                "opcode=0x%08x outcome=%u cause=0x%016llx "
                "status=0x%016llx\n",
                (unsigned long long)machine->cpu.last_pc_full,
                (unsigned)machine->cpu.last_opcode,
                (unsigned)machine->cpu.attempt_outcome,
                (unsigned long long)machine->cpu.cp0[13],
                (unsigned long long)machine->cpu.cp0[12]);
  T64_CHECK(machine->cpu.attempt_outcome == T64_CPU_ATTEMPT_RETIRED);
  if (machine->cpu.last_pc_full != TEST_PC ||
      machine->cpu.last_opcode != odd_add_opcode ||
      machine->cpu.attempt_outcome != T64_CPU_ATTEMPT_RETIRED) {
    t64_machine_destroy(machine);
  } else {
    T64_CHECK((machine->cpu.cp0[13] & UINT64_C(0x8000007c)) == UINT64_C(0));
    T64_CHECK((uint32_t)machine->cpu.fpr[0] == UINT32_C(0x00000000));
    T64_CHECK((uint32_t)machine->cpu.fpr[1] == UINT32_C(0x40180000));

    machine->cpu.fpr[24] = UINT32_C(42);
    machine->cpu.fpr[25] = UINT32_C(0);
    replay_fpu_instruction(machine, odd_cvt_d_l_opcode);
    T64_CHECK(advance_tick(machine, UINT64_C(2)));
    (void)fprintf(stderr,
                  "# compat-v2 odd CVT.D.L pc=0x%016llx opcode=0x%08x "
                  "outcome=%u cause=0x%016llx status=0x%016llx "
                  "source=(0x%016llx,0x%016llx) "
                  "destination=(0x%016llx,0x%016llx)\n",
                  (unsigned long long)machine->cpu.last_pc_full,
                  (unsigned)machine->cpu.last_opcode,
                  (unsigned)machine->cpu.attempt_outcome,
                  (unsigned long long)machine->cpu.cp0[13],
                  (unsigned long long)machine->cpu.cp0[12],
                  (unsigned long long)machine->cpu.fpr[24],
                  (unsigned long long)machine->cpu.fpr[25],
                  (unsigned long long)machine->cpu.fpr[2],
                  (unsigned long long)machine->cpu.fpr[3]);
    T64_CHECK(machine->cpu.last_pc_full == TEST_PC);
    T64_CHECK(machine->cpu.last_opcode == odd_cvt_d_l_opcode);
    T64_CHECK(machine->cpu.attempt_outcome == T64_CPU_ATTEMPT_RETIRED);
    T64_CHECK((machine->cpu.cp0[13] & UINT64_C(0x8000007c)) == UINT64_C(0));
    T64_CHECK((uint32_t)machine->cpu.fpr[2] == UINT32_C(0x00000000));
    T64_CHECK((uint32_t)machine->cpu.fpr[3] == UINT32_C(0x40450000));

    machine->cpu.fpr[26] = UINT32_C(0x40000000); /* +2.0f */
    machine->cpu.fpr[27] = UINT32_C(0xc0000000); /* -2.0f */
    replay_fpu_instruction(machine, odd_cvt_d_s_opcode);
    T64_CHECK(advance_tick(machine, UINT64_C(3)));
    (void)fprintf(stderr,
                  "# compat-v2 odd CVT.D.S pc=0x%016llx opcode=0x%08x "
                  "outcome=%u cause=0x%016llx status=0x%016llx "
                  "source=(0x%016llx,0x%016llx) "
                  "destination=(0x%016llx,0x%016llx)\n",
                  (unsigned long long)machine->cpu.last_pc_full,
                  (unsigned)machine->cpu.last_opcode,
                  (unsigned)machine->cpu.attempt_outcome,
                  (unsigned long long)machine->cpu.cp0[13],
                  (unsigned long long)machine->cpu.cp0[12],
                  (unsigned long long)machine->cpu.fpr[26],
                  (unsigned long long)machine->cpu.fpr[27],
                  (unsigned long long)machine->cpu.fpr[4],
                  (unsigned long long)machine->cpu.fpr[5]);
    T64_CHECK(machine->cpu.last_pc_full == TEST_PC);
    T64_CHECK(machine->cpu.last_opcode == odd_cvt_d_s_opcode);
    T64_CHECK(machine->cpu.attempt_outcome == T64_CPU_ATTEMPT_RETIRED);
    T64_CHECK((machine->cpu.cp0[13] & UINT64_C(0x8000007c)) == UINT64_C(0));
    T64_CHECK((uint32_t)machine->cpu.fpr[4] == UINT32_C(0x00000000));
    T64_CHECK((uint32_t)machine->cpu.fpr[5] == UINT32_C(0x40000000));

    machine->cpu.fpr[8] = UINT64_C(0x8899aabb00000007);
    machine->cpu.fpr[9] = UINT64_C(0xccddeeff00000063);
    machine->cpu.fpr[4] = UINT64_C(0x11223344a5a5a5a5);
    machine->cpu.fpr[5] = UINT64_C(0x55667788deadbeef);
    replay_fpu_instruction(machine, odd_cvt_s_w_opcode);
    T64_CHECK(advance_tick(machine, UINT64_C(4)));
    (void)fprintf(stderr,
                  "# compat-v2 odd-source CVT.S.W pc=0x%016llx "
                  "opcode=0x%08x outcome=%u cause=0x%016llx "
                  "status=0x%016llx source=(0x%016llx,0x%016llx) "
                  "destination=(0x%016llx,0x%016llx)\n",
                  (unsigned long long)machine->cpu.last_pc_full,
                  (unsigned)machine->cpu.last_opcode,
                  (unsigned)machine->cpu.attempt_outcome,
                  (unsigned long long)machine->cpu.cp0[13],
                  (unsigned long long)machine->cpu.cp0[12],
                  (unsigned long long)machine->cpu.fpr[8],
                  (unsigned long long)machine->cpu.fpr[9],
                  (unsigned long long)machine->cpu.fpr[4],
                  (unsigned long long)machine->cpu.fpr[5]);
    T64_CHECK(machine->cpu.last_pc_full == TEST_PC);
    T64_CHECK(machine->cpu.last_opcode == odd_cvt_s_w_opcode);
    T64_CHECK(machine->cpu.attempt_outcome == T64_CPU_ATTEMPT_RETIRED);
    T64_CHECK((machine->cpu.cp0[13] & UINT64_C(0x8000007c)) == UINT64_C(0));
    T64_CHECK(machine->cpu.fpr[4] == UINT64_C(0x11223344a5a5a5a5));
    T64_CHECK(machine->cpu.fpr[5] == UINT64_C(0x5566778840e00000));
    t64_machine_destroy(machine);
  }

  for (index = 0u;
       index < sizeof baseline_profile_ids / sizeof baseline_profile_ids[0];
       index++) {
    const t64_profile_descriptor *baseline_profile = NULL;
    size_t opcode_index;

    T64_CHECK(t64_profile_lookup(baseline_profile_ids[index],
                                 &baseline_profile) == T64_STATUS_OK);
    T64_CHECK(baseline_profile != NULL);
    if (baseline_profile == NULL) continue;
    for (opcode_index = 0u;
         opcode_index < sizeof baseline_odd_opcodes /
                            sizeof baseline_odd_opcodes[0];
         opcode_index++) {
      machine = make_machine(allocator, baseline_profile, asset);
      T64_CHECK(machine != NULL);
      if (machine == NULL) continue;
      machine->cpu.cp0[12] &= ~UINT64_C(0x04000000); /* FR=0 */
      machine->cpu.gpr[3] = UINT64_C(0xdeadbeef12345678);
      machine->cpu.fpr[0] = UINT32_C(0x00000000);
      machine->cpu.fpr[1] = UINT32_C(0x3ff00000);
      machine->cpu.fpr[2] = UINT32_C(0x11223344);
      machine->cpu.fpr[3] = UINT32_C(0x55667788);
      write_instruction(machine, UINT32_C(0x1000),
                        baseline_odd_opcodes[opcode_index]);
      T64_CHECK(advance_tick(machine, UINT64_C(1)));
      T64_CHECK(machine->cpu.attempt_outcome ==
                T64_CPU_ATTEMPT_GUEST_EXCEPTION);
      T64_CHECK((machine->cpu.cp0[13] & UINT64_C(0x8000007c)) ==
                UINT64_C(0x0000003c)); /* preserve unimplemented FPE */
      T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x00020000)) != 0u);
      if (opcode_index == 0u) {
        T64_CHECK(machine->cpu.gpr[3] == UINT64_C(0xdeadbeef12345678));
      } else {
        T64_CHECK((uint32_t)machine->cpu.fpr[2] == UINT32_C(0x11223344));
        T64_CHECK((uint32_t)machine->cpu.fpr[3] == UINT32_C(0x55667788));
      }
      t64_machine_destroy(machine);
    }
  }
}

static void test_rsp_functional_profile_cop1_transfer(
    const t64_host_allocator *allocator, const uint8_t *asset) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine;
  uint32_t opcode = encode_cop1_transfer(6u, 2u, 31u); /* CTC1 v0,FCR31 */

  T64_CHECK(t64_profile_lookup("ntsc-cart-4m-openipl3-rsp-functional-v1",
                               &profile) == T64_STATUS_OK);
  T64_CHECK(profile != NULL);
  if (profile == NULL) return;
  T64_CHECK(profile->machine_model_version == 6u);
  T64_CHECK(profile->state_serialization_version == 12u);
  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;

  machine->cpu.gpr[2] = FCR31_CONDITION;
  replay_fpu_instruction(machine, opcode);
  T64_CHECK(advance_tick(machine, UINT64_C(1)));
  T64_CHECK(machine->cpu.last_pc_full == TEST_PC);
  T64_CHECK(machine->cpu.last_opcode == opcode);
  T64_CHECK(machine->cpu.attempt_outcome == T64_CPU_ATTEMPT_RETIRED);
  T64_CHECK(machine->cpu.fcr31 == FCR31_CONDITION);
  T64_CHECK((machine->cpu.cp0[13] & UINT64_C(0x8000007c)) == 0u);
  t64_machine_destroy(machine);
}

static void test_arithmetic_tie_rounding(
    const t64_host_allocator *allocator,
    const t64_profile_descriptor *profile, const uint8_t *asset) {
  static const uint64_t expected_f32[4] = {
      UINT64_C(0x3f800000), UINT64_C(0x3f800000),
      UINT64_C(0x3f800001), UINT64_C(0x3f800000)};
  static const uint64_t expected_f64[4] = {
      UINT64_C(0x3ff0000000000000), UINT64_C(0x3ff0000000000000),
      UINT64_C(0x3ff0000000000001), UINT64_C(0x3ff0000000000000)};
  uint32_t format_index;
  uint32_t rounding_mode;

  for (format_index = 0u; format_index < 2u; format_index++) {
    uint32_t format = format_index == 0u ? UINT32_C(16) : UINT32_C(17);
    uint32_t width = format_index == 0u ? 32u : 64u;
    uint64_t one = format_index == 0u ? UINT64_C(0x3f800000)
                                     : UINT64_C(0x3ff0000000000000);
    uint64_t half_ulp = format_index == 0u ? UINT64_C(0x33800000)
                                           : UINT64_C(0x3ca0000000000000);
    const uint64_t *expected = format_index == 0u ? expected_f32 : expected_f64;

    for (rounding_mode = 0u; rounding_mode < 4u; rounding_mode++) {
      t64_machine *machine = make_machine(allocator, profile, asset);

      T64_CHECK(machine != NULL);
      if (machine == NULL) continue;
      machine->cpu.cp0[12] |= UINT64_C(0x04000000); /* independent FR=1 lanes */
      machine->cpu.fcr31 = rounding_mode;
      t64_cop1_commit_fpr(machine, 1u, width, one);
      t64_cop1_commit_fpr(machine, 2u, width, half_ulp);
      t64_cop1_commit_fpr(machine, 3u, width, UINT64_C(0x1122334455667788));
      write_instruction(machine, UINT32_C(0x1000),
                        encode_cop1_operation(format, 2u, 1u, 3u, 0u));
      T64_CHECK(advance_tick(machine, UINT64_C(1)));
      T64_CHECK(machine->cpu.attempt_outcome == T64_CPU_ATTEMPT_RETIRED);
      if (machine->cpu.fpr[3] != expected[rounding_mode]) {
        fprintf(stderr,
                "tie mismatch fmt=%u rm=%u opcode=%08x got=%016llx want=%016llx fcr31=%08x\n",
                format, rounding_mode,
                encode_cop1_operation(format, 2u, 1u, 3u, 0u),
                (unsigned long long)machine->cpu.fpr[3],
                (unsigned long long)expected[rounding_mode],
                machine->cpu.fcr31);
      }
      T64_CHECK(machine->cpu.fpr[3] == expected[rounding_mode]);
      T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f07f)) ==
                (UINT32_C(0x00001004) | rounding_mode));
      t64_machine_destroy(machine);
    }
  }
}

static void test_exact_arithmetic_operations(
    const t64_host_allocator *allocator,
    const t64_profile_descriptor *profile, const uint8_t *asset) {
  static const struct {
    uint32_t function;
    uint64_t left;
    uint64_t right;
    uint64_t expected;
  } vectors[2][8] = {
      {{0u, UINT64_C(0x3fc00000), UINT64_C(0x40100000),
        UINT64_C(0x40700000)},
       {1u, UINT64_C(0x40700000), UINT64_C(0x40100000),
        UINT64_C(0x3fc00000)},
       {2u, UINT64_C(0x3fc00000), UINT64_C(0x40000000),
        UINT64_C(0x40400000)},
       {3u, UINT64_C(0x40400000), UINT64_C(0x40000000),
        UINT64_C(0x3fc00000)},
       {4u, UINT64_C(0x40800000), 0u, UINT64_C(0x40000000)},
       {5u, UINT64_C(0xbfc00000), 0u, UINT64_C(0x3fc00000)},
       {6u, UINT64_C(0x80000000), 0u, UINT64_C(0x80000000)},
       {7u, UINT64_C(0x3fc00000), 0u, UINT64_C(0xbfc00000)}},
      {{0u, UINT64_C(0x3ff8000000000000), UINT64_C(0x4002000000000000),
        UINT64_C(0x400e000000000000)},
       {1u, UINT64_C(0x400e000000000000), UINT64_C(0x4002000000000000),
        UINT64_C(0x3ff8000000000000)},
       {2u, UINT64_C(0x3ff8000000000000), UINT64_C(0x4000000000000000),
        UINT64_C(0x4008000000000000)},
       {3u, UINT64_C(0x4008000000000000), UINT64_C(0x4000000000000000),
        UINT64_C(0x3ff8000000000000)},
       {4u, UINT64_C(0x4010000000000000), 0u,
        UINT64_C(0x4000000000000000)},
       {5u, UINT64_C(0xbff8000000000000), 0u,
        UINT64_C(0x3ff8000000000000)},
       {6u, UINT64_C(0x8000000000000000), 0u,
        UINT64_C(0x8000000000000000)},
       {7u, UINT64_C(0x3ff8000000000000), 0u,
        UINT64_C(0xbff8000000000000)}}};
  uint32_t format_index;
  uint32_t vector_index;
  uint32_t rounding_mode;

  for (format_index = 0u; format_index < 2u; format_index++) {
    uint32_t format = format_index == 0u ? UINT32_C(16) : UINT32_C(17);
    uint32_t width = format_index == 0u ? 32u : 64u;

    for (vector_index = 0u; vector_index < 8u; vector_index++) {
      const uint32_t function = vectors[format_index][vector_index].function;
      uint32_t ft = function <= UINT32_C(3) ? 2u : 0u;
      bool move = function == UINT32_C(6);

      for (rounding_mode = 0u; rounding_mode < 4u; rounding_mode++) {
        t64_machine *machine = make_machine(allocator, profile, asset);

        T64_CHECK(machine != NULL);
        if (machine == NULL) continue;
        machine->cpu.cp0[12] |= UINT64_C(0x04000000);
        machine->cpu.fcr31 = UINT32_C(0x0001007c) | rounding_mode;
        t64_cop1_commit_fpr(machine, 1u, width,
                            vectors[format_index][vector_index].left);
        t64_cop1_commit_fpr(machine, 2u, width,
                            vectors[format_index][vector_index].right);
        t64_cop1_commit_fpr(machine, 3u, width,
                            UINT64_C(0x1122334455667788));
        T64_CHECK(execute_fpu_instruction(machine, format, ft, 1u, 3u,
                                          function));
        T64_CHECK(machine->cpu.attempt_outcome == T64_CPU_ATTEMPT_RETIRED);
        T64_CHECK(read_fpr_bits(machine, 3u, width) ==
                  vectors[format_index][vector_index].expected);
        T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0000007c)) ==
                  UINT32_C(0x0000007c));
        T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) ==
                  (move ? UINT32_C(0x00010000) : 0u));
        T64_CHECK((machine->cpu.fcr31 & UINT32_C(3)) == rounding_mode);
        t64_machine_destroy(machine);
      }
    }
  }
}

static void test_comparison_predicate_bits(
    const t64_host_allocator *allocator,
    const t64_profile_descriptor *profile, const uint8_t *asset) {
  uint32_t format_index;
  uint32_t function;

  for (format_index = 0u; format_index < 2u; format_index++) {
    uint32_t format = format_index == 0u ? UINT32_C(16) : UINT32_C(17);
    uint32_t width = format_index == 0u ? 32u : 64u;
    uint64_t one = format_index == 0u ? UINT64_C(0x3f800000)
                                     : UINT64_C(0x3ff0000000000000);
    uint64_t two = format_index == 0u ? UINT64_C(0x40000000)
                                     : UINT64_C(0x4000000000000000);
    uint64_t quiet_nan = format_index == 0u ? UINT64_C(0x7f800001)
                                           : UINT64_C(0x7ff0000000000001);

    for (function = UINT32_C(0x30); function <= UINT32_C(0x3f); function++) {
      t64_machine *machine = make_machine(allocator, profile, asset);
      bool expected_less = (function & UINT32_C(4)) != 0u;

      T64_CHECK(machine != NULL);
      if (machine == NULL) continue;
      machine->cpu.cp0[12] |= UINT64_C(0x04000000);
      machine->cpu.fcr31 = FCR31_CONDITION | UINT32_C(0x0000007c);
      t64_cop1_commit_fpr(machine, 1u, width, one);
      t64_cop1_commit_fpr(machine, 2u, width, two);
      T64_CHECK(execute_fpu_instruction(machine, format, 2u, 1u, 0u,
                                        function));
      T64_CHECK(machine->cpu.attempt_outcome == T64_CPU_ATTEMPT_RETIRED);
      T64_CHECK(((machine->cpu.fcr31 & FCR31_CONDITION) != 0u) ==
                expected_less);
      T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) == 0u);
      T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0000007c)) ==
                UINT32_C(0x0000007c));
      t64_machine_destroy(machine);

      machine = make_machine(allocator, profile, asset);
      T64_CHECK(machine != NULL);
      if (machine == NULL) continue;
      machine->cpu.cp0[12] |= UINT64_C(0x04000000);
      machine->cpu.fcr31 = FCR31_CONDITION | UINT32_C(0x0000007c);
      t64_cop1_commit_fpr(machine, 1u, width, quiet_nan);
      t64_cop1_commit_fpr(machine, 2u, width, one);
      T64_CHECK(execute_fpu_instruction(machine, format, 2u, 1u, 0u,
                                        function));
      T64_CHECK(machine->cpu.attempt_outcome == T64_CPU_ATTEMPT_RETIRED);
      T64_CHECK(((machine->cpu.fcr31 & FCR31_CONDITION) != 0u) ==
                ((function & UINT32_C(1)) != 0u));
      T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) ==
                ((function & UINT32_C(8)) != 0u
                     ? UINT32_C(0x00010000)
                     : 0u));
      T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0000007c)) ==
                (((function & UINT32_C(8)) != 0u
                      ? UINT32_C(0x00000040)
                      : 0u) |
                 UINT32_C(0x0000007c)));
      t64_machine_destroy(machine);
    }
  }
}

static void test_format_and_integer_conversions(
    const t64_host_allocator *allocator,
    const t64_profile_descriptor *profile, const uint8_t *asset) {
  static const struct {
    uint32_t source_format;
    uint32_t function;
    uint32_t source_width;
    uint32_t destination_width;
    uint64_t source;
    uint64_t expected;
  } vectors[] = {
      {17u, 0x20u, 64u, 32u, UINT64_C(0x400c000000000000),
       UINT64_C(0x40600000)}, /* CVT.S.D 3.5 */
      {16u, 0x21u, 32u, 64u, UINT64_C(0x40600000),
       UINT64_C(0x400c000000000000)}, /* CVT.D.S 3.5 */
      {20u, 0x20u, 32u, 32u, UINT64_C(0xfffffff3),
       UINT64_C(0xc1500000)}, /* CVT.S.W -13 */
      {20u, 0x21u, 32u, 64u, UINT64_C(0xfffffff3),
       UINT64_C(0xc02a000000000000)}, /* CVT.D.W -13 */
      {21u, 0x21u, 64u, 64u, UINT64_C(0x0000000100000001),
       UINT64_C(0x41f0000000100000)}}; /* CVT.D.L 2^32+1 */
  static const int64_t expected_positive[4] = {2, 2, 3, 2};
  static const int64_t expected_negative[4] = {-2, -2, -2, -3};
  size_t vector_index;
  uint32_t source_format_index;
  uint32_t rounding_mode;
  uint32_t function;

  for (vector_index = 0u;
       vector_index < sizeof vectors / sizeof vectors[0]; vector_index++) {
    for (rounding_mode = 0u; rounding_mode < 4u; rounding_mode++) {
      t64_machine *machine = make_machine(allocator, profile, asset);

      T64_CHECK(machine != NULL);
      if (machine == NULL) continue;
      machine->cpu.cp0[12] |= UINT64_C(0x04000000);
      machine->cpu.fcr31 = rounding_mode;
      t64_cop1_commit_fpr(machine, 1u, vectors[vector_index].source_width,
                          vectors[vector_index].source);
      t64_cop1_commit_fpr(machine, 3u,
                          vectors[vector_index].destination_width,
                          UINT64_C(0x1122334455667788));
      T64_CHECK(execute_fpu_instruction(
          machine, vectors[vector_index].source_format, 0u, 1u, 3u,
          vectors[vector_index].function));
      T64_CHECK(machine->cpu.attempt_outcome == T64_CPU_ATTEMPT_RETIRED);
      T64_CHECK(read_fpr_bits(machine, 3u,
                              vectors[vector_index].destination_width) ==
                vectors[vector_index].expected);
      t64_machine_destroy(machine);
    }
  }

  {
    static const uint32_t positive_tie[4] = {
        UINT32_C(0x3f800000), UINT32_C(0x3f800000),
        UINT32_C(0x3f800001), UINT32_C(0x3f800000)};
    static const uint32_t negative_tie[4] = {
        UINT32_C(0xbf800000), UINT32_C(0xbf800000),
        UINT32_C(0xbf800000), UINT32_C(0xbf800001)};
    static const uint32_t positive_long[4] = {
        UINT32_C(0x4f800000), UINT32_C(0x4f800000),
        UINT32_C(0x4f800001), UINT32_C(0x4f800000)};
    static const uint32_t negative_long[4] = {
        UINT32_C(0xcf800000), UINT32_C(0xcf800000),
        UINT32_C(0xcf800000), UINT32_C(0xcf800001)};

    for (rounding_mode = 0u; rounding_mode < 4u; rounding_mode++) {
      t64_machine *machine = make_machine(allocator, profile, asset);

      T64_CHECK(machine != NULL);
      if (machine == NULL) continue;
      machine->cpu.cp0[12] |= UINT64_C(0x04000000);
      machine->cpu.fcr31 = rounding_mode;
      t64_cop1_commit_fpr(machine, 1u, 64u,
                          UINT64_C(0x3ff0000010000000));
      T64_CHECK(execute_fpu_instruction(machine, 17u, 0u, 1u, 3u, 0x20u));
      T64_CHECK(read_fpr_bits(machine, 3u, 32u) == positive_tie[rounding_mode]);
      T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f07f)) ==
                (UINT32_C(0x00001004) | rounding_mode));
      t64_machine_destroy(machine);

      machine = make_machine(allocator, profile, asset);
      T64_CHECK(machine != NULL);
      if (machine == NULL) continue;
      machine->cpu.cp0[12] |= UINT64_C(0x04000000);
      machine->cpu.fcr31 = rounding_mode;
      t64_cop1_commit_fpr(machine, 1u, 64u,
                          UINT64_C(0xbff0000010000000));
      T64_CHECK(execute_fpu_instruction(machine, 17u, 0u, 1u, 3u, 0x20u));
      T64_CHECK(read_fpr_bits(machine, 3u, 32u) == negative_tie[rounding_mode]);
      T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f07f)) ==
                (UINT32_C(0x00001004) | rounding_mode));
      t64_machine_destroy(machine);

      machine = make_machine(allocator, profile, asset);
      T64_CHECK(machine != NULL);
      if (machine == NULL) continue;
      machine->cpu.cp0[12] |= UINT64_C(0x04000000);
      machine->cpu.fcr31 = rounding_mode;
      t64_cop1_commit_fpr(machine, 1u, 64u, UINT64_C(0x0000000100000001));
      T64_CHECK(execute_fpu_instruction(machine, 21u, 0u, 1u, 3u, 0x20u));
      T64_CHECK(read_fpr_bits(machine, 3u, 32u) == positive_long[rounding_mode]);
      T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f07f)) ==
                (UINT32_C(0x00001004) | rounding_mode));
      t64_machine_destroy(machine);

      machine = make_machine(allocator, profile, asset);
      T64_CHECK(machine != NULL);
      if (machine == NULL) continue;
      machine->cpu.cp0[12] |= UINT64_C(0x04000000);
      machine->cpu.fcr31 = rounding_mode;
      t64_cop1_commit_fpr(machine, 1u, 64u,
                          UINT64_C(0xfffffffeffffffff)); /* -(2^32+1) */
      T64_CHECK(execute_fpu_instruction(machine, 21u, 0u, 1u, 3u, 0x20u));
      T64_CHECK(read_fpr_bits(machine, 3u, 32u) == negative_long[rounding_mode]);
      T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f07f)) ==
                (UINT32_C(0x00001004) | rounding_mode));
      t64_machine_destroy(machine);
    }
  }

  for (source_format_index = 0u; source_format_index < 2u;
       source_format_index++) {
    uint32_t format = source_format_index == 0u ? UINT32_C(16)
                                                : UINT32_C(17);
    uint32_t source_width = source_format_index == 0u ? 32u : 64u;
    uint64_t positive_half = source_format_index == 0u
                                 ? UINT64_C(0x40200000)
                                 : UINT64_C(0x4004000000000000);
    uint64_t negative_half = source_format_index == 0u
                                 ? UINT64_C(0xc0200000)
                                 : UINT64_C(0xc004000000000000);

    for (rounding_mode = 0u; rounding_mode < 4u; rounding_mode++) {
      for (function = UINT32_C(0x24); function <= UINT32_C(0x25); function++) {
        uint32_t result_width = function == UINT32_C(0x24) ? 32u : 64u;
        int64_t expected = expected_positive[rounding_mode];
        t64_machine *machine = make_machine(allocator, profile, asset);

        T64_CHECK(machine != NULL);
        if (machine == NULL) continue;
        machine->cpu.cp0[12] |= UINT64_C(0x04000000);
        machine->cpu.fcr31 = rounding_mode;
        t64_cop1_commit_fpr(machine, 1u, source_width, positive_half);
        T64_CHECK(execute_fpu_instruction(machine, format, 0u, 1u, 3u,
                                          function));
        T64_CHECK(read_fpr_bits(machine, 3u, result_width) ==
                  (result_width == 32u ? (uint32_t)expected
                                       : (uint64_t)expected));
        T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f07f)) ==
                  (UINT32_C(0x00001004) | rounding_mode));
        t64_machine_destroy(machine);

        expected = expected_negative[rounding_mode];
        machine = make_machine(allocator, profile, asset);
        T64_CHECK(machine != NULL);
        if (machine == NULL) continue;
        machine->cpu.cp0[12] |= UINT64_C(0x04000000);
        machine->cpu.fcr31 = rounding_mode;
        t64_cop1_commit_fpr(machine, 1u, source_width, negative_half);
        T64_CHECK(execute_fpu_instruction(machine, format, 0u, 1u, 3u,
                                          function));
        T64_CHECK(read_fpr_bits(machine, 3u, result_width) ==
                  (result_width == 32u ? (uint32_t)expected
                                       : (uint64_t)expected));
        T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f07f)) ==
                  (UINT32_C(0x00001004) | rounding_mode));
        t64_machine_destroy(machine);
      }
    }

    for (function = UINT32_C(0x08); function <= UINT32_C(0x0f); function++) {
      uint32_t result_width = function < UINT32_C(0x0c) ? 64u : 32u;
      uint32_t explicit_mode = function & UINT32_C(3);
      int64_t expected = expected_positive[explicit_mode];
      t64_machine *machine;

      if (explicit_mode == 0u || explicit_mode == 1u) expected = 2;
      if (explicit_mode == 2u) expected = 3;
      if (explicit_mode == 3u) expected = 2;
      machine = make_machine(allocator, profile, asset);
      T64_CHECK(machine != NULL);
      if (machine == NULL) continue;
      machine->cpu.cp0[12] |= UINT64_C(0x04000000);
      machine->cpu.fcr31 = UINT32_C(3); /* explicit operation ignores RM */
      t64_cop1_commit_fpr(machine, 1u, source_width, positive_half);
      T64_CHECK(execute_fpu_instruction(machine, format, 0u, 1u, 3u,
                                        function));
      T64_CHECK(read_fpr_bits(machine, 3u, result_width) ==
                (result_width == 32u ? (uint32_t)expected
                                     : (uint64_t)expected));
      T64_CHECK((machine->cpu.fcr31 & UINT32_C(3)) == UINT32_C(3));
      t64_machine_destroy(machine);

      expected = explicit_mode == 3u ? -3 : -2;
      machine = make_machine(allocator, profile, asset);
      T64_CHECK(machine != NULL);
      if (machine == NULL) continue;
      machine->cpu.cp0[12] |= UINT64_C(0x04000000);
      machine->cpu.fcr31 = UINT32_C(0);
      t64_cop1_commit_fpr(machine, 1u, source_width, negative_half);
      T64_CHECK(execute_fpu_instruction(machine, format, 0u, 1u, 3u,
                                        function));
      T64_CHECK(read_fpr_bits(machine, 3u, result_width) ==
                (result_width == 32u ? (uint32_t)expected
                                     : (uint64_t)expected));
      T64_CHECK((machine->cpu.fcr31 & UINT32_C(3)) == UINT32_C(0));
      t64_machine_destroy(machine);
    }
  }
}

static void test_paired_registers_and_compare_branch(
    const t64_host_allocator *allocator,
    const t64_profile_descriptor *profile, const uint8_t *asset) {
  t64_machine *machine = make_machine(allocator, profile, asset);

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] &= ~UINT64_C(0x04000000); /* FR=0 paired double lanes */
  t64_cop1_commit_fpr(machine, 2u, 64u, UINT64_C(0x3ff8000000000000));
  t64_cop1_commit_fpr(machine, 4u, 64u, UINT64_C(0x4002000000000000));
  T64_CHECK(execute_fpu_instruction(machine, 17u, 4u, 2u, 6u, 0u));
  T64_CHECK(read_fpr_bits(machine, 6u, 64u) ==
            UINT64_C(0x400e000000000000));
  T64_CHECK((uint32_t)machine->cpu.fpr[6] == UINT32_C(0x00000000));
  T64_CHECK((uint32_t)machine->cpu.fpr[7] == UINT32_C(0x400e0000));
  t64_machine_destroy(machine);

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] |= UINT64_C(0x04000000);
  machine->cpu.fcr31 = 0u;
  t64_cop1_commit_fpr(machine, 1u, 32u, UINT32_C(0x3f800000));
  t64_cop1_commit_fpr(machine, 2u, 32u, UINT32_C(0x40000000));
  write_instruction(machine, UINT32_C(0x1000),
                    encode_cop1_operation(16u, 2u, 1u, 0u, 0x3cu));
  write_instruction(machine, UINT32_C(0x1004), UINT32_C(0x45010001));
  write_instruction(machine, UINT32_C(0x1008), UINT32_C(0x24080001));
  write_instruction(machine, UINT32_C(0x100c), UINT32_C(0x24090002));
  T64_CHECK(advance_tick(machine, UINT64_C(1)));
  T64_CHECK((machine->cpu.fcr31 & FCR31_CONDITION) != 0u);
  T64_CHECK(advance_tick(machine, UINT64_C(2)));
  T64_CHECK(machine->cpu.delay_slot_pending);
  T64_CHECK(machine->cpu.delay_target_full == TEST_PC + UINT64_C(12));
  T64_CHECK(advance_tick(machine, UINT64_C(3)));
  T64_CHECK(machine->cpu.pc_full == TEST_PC + UINT64_C(12));
  T64_CHECK(advance_tick(machine, UINT64_C(4)));
  T64_CHECK(machine->cpu.gpr[9] == UINT64_C(2));
  T64_CHECK(machine->cpu.gpr[8] == UINT64_C(1));
  t64_machine_destroy(machine);
}

static void test_interleaved_machine_rounding(
    const t64_host_allocator *allocator,
    const t64_profile_descriptor *profile, const uint8_t *asset) {
  t64_machine *round_up = make_machine(allocator, profile, asset);
  t64_machine *round_down;

  T64_CHECK(round_up != NULL);
  if (round_up == NULL) return;
  round_down = make_machine(allocator, profile, asset);
  T64_CHECK(round_down != NULL);
  if (round_down == NULL) {
    t64_machine_destroy(round_up);
    return;
  }
  round_up->cpu.cp0[12] |= UINT64_C(0x04000000);
  round_down->cpu.cp0[12] |= UINT64_C(0x04000000);
  round_up->cpu.fcr31 = UINT32_C(2);
  round_down->cpu.fcr31 = UINT32_C(3);
  t64_cop1_commit_fpr(round_up, 1u, 32u, UINT32_C(0x3f800000));
  t64_cop1_commit_fpr(round_up, 2u, 32u, UINT32_C(0x33800000));
  t64_cop1_commit_fpr(round_down, 1u, 32u, UINT32_C(0x3f800000));
  t64_cop1_commit_fpr(round_down, 2u, 32u, UINT32_C(0x33800000));
  T64_CHECK(execute_fpu_instruction(round_up, 16u, 2u, 1u, 3u, 0u));
  T64_CHECK(execute_fpu_instruction(round_down, 16u, 2u, 1u, 3u, 0u));
  T64_CHECK(read_fpr_bits(round_up, 3u, 32u) == UINT32_C(0x3f800001));
  T64_CHECK(read_fpr_bits(round_down, 3u, 32u) == UINT32_C(0x3f800000));
  T64_CHECK((round_up->cpu.fcr31 & UINT32_C(0x0003f07f)) ==
            UINT32_C(0x00001006));
  T64_CHECK((round_down->cpu.fcr31 & UINT32_C(0x0003f07f)) ==
            UINT32_C(0x00001007));
  t64_machine_destroy(round_down);
  t64_machine_destroy(round_up);
}

static void test_invalid_integer_source_format(
    const t64_host_allocator *allocator,
    const t64_profile_descriptor *profile, const uint8_t *asset) {
  t64_machine *machine = make_machine(allocator, profile, asset);
  const uint32_t opcode = encode_cop1_operation(20u, 0u, 1u, 3u, 0x24u);
  const uint64_t destination = UINT64_C(0x1122334455667788);

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] |= UINT64_C(0x04000000); /* independent FR=1 lanes */
  machine->cpu.fcr31 = UINT32_C(0x0001007c); /* prior V Cause and sticky flags */
  t64_cop1_commit_fpr(machine, 1u, 32u, UINT32_C(0x3f800000));
  t64_cop1_commit_fpr(machine, 3u, 64u, destination);
  write_instruction(machine, UINT32_C(0x1000), opcode);
  T64_CHECK(advance_tick(machine, UINT64_C(1)));
  if (((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x7c)) != UINT32_C(0x3c) ||
      machine->cpu.cp0[14] != TEST_PC ||
      (machine->cpu.fcr31 & UINT32_C(0x0003f000)) != UINT32_C(0x00020000) ||
      (machine->cpu.fcr31 & UINT32_C(0x0000007c)) != UINT32_C(0x0000007c) ||
      machine->cpu.fpr[3] != destination) {
    fprintf(stderr,
            "invalid-format expected opcode=%08x pc=%016llx FPE/E, "
            "FCR31 cause=00020000 flags=7c dest=%016llx; observed "
            "opcode=%08x pc=%016llx cp0-cause=%08x epc=%016llx "
            "fcr31=%08x dest=%016llx outcome=%u\n",
            opcode, (unsigned long long)TEST_PC,
            (unsigned long long)destination,
            machine->cpu.last_opcode,
            (unsigned long long)machine->cpu.last_pc_full,
            (uint32_t)machine->cpu.cp0[13],
            (unsigned long long)machine->cpu.cp0[14], machine->cpu.fcr31,
            (unsigned long long)machine->cpu.fpr[3],
            (unsigned)machine->cpu.attempt_outcome);
  }
  T64_CHECK(((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x7c)) ==
            UINT32_C(0x3c)); /* FPE, not a retired conversion */
  T64_CHECK(machine->cpu.cp0[14] == TEST_PC);
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) ==
            UINT32_C(0x00020000)); /* E replaces only the last Cause */
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0000007c)) ==
            UINT32_C(0x0000007c)); /* sticky flags survive */
  T64_CHECK(machine->cpu.fpr[3] == destination);
  T64_CHECK(machine->cpu.attempt_outcome == T64_CPU_ATTEMPT_GUEST_EXCEPTION);
  t64_machine_destroy(machine);
}

static void test_ieee_and_fs_exception_matrix(
    const t64_host_allocator *allocator,
    const t64_profile_descriptor *profile, const uint8_t *asset) {
  const uint64_t destination = UINT64_C(0x1122334455667788);
  t64_machine *machine;

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] |= UINT64_C(0x04000000);
  t64_cop1_commit_fpr(machine, 1u, 32u, UINT32_C(0x3f800000));
  t64_cop1_commit_fpr(machine, 2u, 32u, UINT32_C(0x00000000));
  T64_CHECK(execute_fpu_instruction(machine, 16u, 2u, 1u, 3u, 3u));
  T64_CHECK(machine->cpu.attempt_outcome == T64_CPU_ATTEMPT_RETIRED);
  T64_CHECK(read_fpr_bits(machine, 3u, 32u) == UINT32_C(0x7f800000));
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) ==
            UINT32_C(0x00008000)); /* Z Cause */
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0000007c)) ==
            UINT32_C(0x00000020)); /* masked Z sets its sticky Flag */
  T64_CHECK(machine->cpu.cp0[13] == UINT64_C(0));
  t64_machine_destroy(machine);

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] |= UINT64_C(0x04000000);
  machine->cpu.fcr31 = UINT32_C(0x00000400); /* enable Z */
  t64_cop1_commit_fpr(machine, 1u, 32u, UINT32_C(0x3f800000));
  t64_cop1_commit_fpr(machine, 2u, 32u, UINT32_C(0x00000000));
  t64_cop1_commit_fpr(machine, 3u, 64u, destination);
  T64_CHECK(execute_fpu_instruction(machine, 16u, 2u, 1u, 3u, 3u));
  T64_CHECK(((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x7c)) ==
            UINT32_C(0x3c)); /* FPE due to enabled Z */
  T64_CHECK(machine->cpu.cp0[14] == TEST_PC);
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) ==
            UINT32_C(0x00008000));
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0000007c)) == 0u);
  T64_CHECK(machine->cpu.fpr[3] == destination);
  t64_machine_destroy(machine);

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] |= UINT64_C(0x04000000);
  t64_cop1_commit_fpr(machine, 1u, 32u, UINT32_C(0x00000000));
  t64_cop1_commit_fpr(machine, 2u, 32u, UINT32_C(0x00000000));
  T64_CHECK(execute_fpu_instruction(machine, 16u, 2u, 1u, 3u, 3u));
  T64_CHECK(read_fpr_bits(machine, 3u, 32u) == UINT32_C(0x7f800001));
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) ==
            UINT32_C(0x00010000)); /* V Cause for 0/0 */
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0000007c)) ==
            UINT32_C(0x00000040)); /* masked V Flag */
  t64_machine_destroy(machine);

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] |= UINT64_C(0x04000000);
  machine->cpu.fcr31 = UINT32_C(0x01000000); /* FS affects result only */
  t64_cop1_commit_fpr(machine, 1u, 32u, UINT32_C(0x00000001));
  t64_cop1_commit_fpr(machine, 2u, 32u, UINT32_C(0x3f800000));
  t64_cop1_commit_fpr(machine, 3u, 64u, destination);
  T64_CHECK(execute_fpu_instruction(machine, 16u, 2u, 1u, 3u, 0u));
  T64_CHECK(((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x7c)) ==
            UINT32_C(0x3c)); /* input denormals still raise E */
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) ==
            UINT32_C(0x00020000));
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0000007c)) == 0u);
  T64_CHECK(machine->cpu.fpr[3] == destination);
  t64_machine_destroy(machine);

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] |= UINT64_C(0x04000000);
  machine->cpu.fcr31 = UINT32_C(0x00000000); /* FS clear */
  t64_cop1_commit_fpr(machine, 1u, 32u, UINT32_C(0x00800000));
  t64_cop1_commit_fpr(machine, 2u, 32u, UINT32_C(0x3f000000));
  t64_cop1_commit_fpr(machine, 3u, 64u, destination);
  T64_CHECK(execute_fpu_instruction(machine, 16u, 2u, 1u, 3u, 2u));
  T64_CHECK(((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x7c)) ==
            UINT32_C(0x3c)); /* result denormal raises E with FS clear */
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) ==
            UINT32_C(0x00020000));
  T64_CHECK(machine->cpu.fpr[3] == destination);
  t64_machine_destroy(machine);

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] |= UINT64_C(0x04000000);
  machine->cpu.fcr31 = UINT32_C(0x01000000); /* FS set, RN */
  t64_cop1_commit_fpr(machine, 1u, 32u, UINT32_C(0x00800000));
  t64_cop1_commit_fpr(machine, 2u, 32u, UINT32_C(0x3f000000));
  T64_CHECK(execute_fpu_instruction(machine, 16u, 2u, 1u, 3u, 2u));
  T64_CHECK(read_fpr_bits(machine, 3u, 32u) == UINT32_C(0x00000000));
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) ==
            UINT32_C(0x00003000)); /* U and I Cause */
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0000007c)) ==
            UINT32_C(0x0000000c)); /* U and I Flag */
  t64_machine_destroy(machine);

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] |= UINT64_C(0x04000000);
  machine->cpu.fcr31 = UINT32_C(0x01000002); /* FS set, RP */
  t64_cop1_commit_fpr(machine, 1u, 32u, UINT32_C(0x00800000));
  t64_cop1_commit_fpr(machine, 2u, 32u, UINT32_C(0x3f000000));
  T64_CHECK(execute_fpu_instruction(machine, 16u, 2u, 1u, 3u, 2u));
  T64_CHECK(read_fpr_bits(machine, 3u, 32u) == UINT32_C(0x00800000));
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f07f)) ==
            UINT32_C(0x0000300e));
  t64_machine_destroy(machine);

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] |= UINT64_C(0x04000000);
  machine->cpu.fcr31 = UINT32_C(0x01000100); /* FS, enable U */
  t64_cop1_commit_fpr(machine, 1u, 32u, UINT32_C(0x00800000));
  t64_cop1_commit_fpr(machine, 2u, 32u, UINT32_C(0x3f000000));
  t64_cop1_commit_fpr(machine, 3u, 64u, destination);
  T64_CHECK(execute_fpu_instruction(machine, 16u, 2u, 1u, 3u, 2u));
  T64_CHECK(((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x7c)) ==
            UINT32_C(0x3c));
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) ==
            UINT32_C(0x00020000)); /* E-only cause when U is enabled */
  T64_CHECK(machine->cpu.fpr[3] == destination);
  t64_machine_destroy(machine);

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] |= UINT64_C(0x04000000);
  machine->cpu.fcr31 = 0u;
  t64_cop1_commit_fpr(machine, 1u, 32u, UINT32_C(0x7f7fffff));
  t64_cop1_commit_fpr(machine, 2u, 32u, UINT32_C(0x40000000));
  T64_CHECK(execute_fpu_instruction(machine, 16u, 2u, 1u, 3u, 2u));
  T64_CHECK(read_fpr_bits(machine, 3u, 32u) == UINT32_C(0x7f800000));
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) ==
            UINT32_C(0x00005000)); /* O and I Cause */
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0000007c)) ==
            UINT32_C(0x00000014)); /* masked O and I Flags */
  t64_machine_destroy(machine);

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] |= UINT64_C(0x04000000);
  machine->cpu.fcr31 = UINT32_C(0x0000007c); /* sticky flags survive E */
  t64_cop1_commit_fpr(machine, 1u, 32u, UINT32_C(0x4f000000)); /* 2^31 */
  t64_cop1_commit_fpr(machine, 3u, 32u, UINT32_C(0x55667788));
  T64_CHECK(execute_fpu_instruction(machine, 16u, 0u, 1u, 3u, 0x24u));
  T64_CHECK(((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x7c)) ==
            UINT32_C(0x3c)); /* FP-to-W overflow is E-only */
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) ==
            UINT32_C(0x00020000));
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0000007c)) ==
            UINT32_C(0x0000007c));
  T64_CHECK(read_fpr_bits(machine, 3u, 32u) == UINT32_C(0x55667788));
  t64_machine_destroy(machine);
}

static void test_nan_exception_polarity_and_suppression(
    const t64_host_allocator *allocator,
    const t64_profile_descriptor *profile, const uint8_t *asset) {
  const uint64_t destination = UINT64_C(0x1122334455667788);
  t64_machine *machine = make_machine(allocator, profile, asset);

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] |= UINT64_C(0x04000000);
  machine->cpu.fcr31 = UINT32_C(0x0000007c);
  t64_cop1_commit_fpr(machine, 1u, 32u, UINT32_C(0x7f800001)); /* VR QNaN */
  t64_cop1_commit_fpr(machine, 2u, 32u, UINT32_C(0x3f800000));
  t64_cop1_commit_fpr(machine, 3u, 64u, destination);
  T64_CHECK(execute_fpu_instruction(machine, 16u, 2u, 1u, 3u, 0u));
  T64_CHECK(((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x7c)) ==
            UINT32_C(0x3c)); /* VR QNaN uses E, not SoftFloat invalid */
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) ==
            UINT32_C(0x00020000));
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0000007c)) ==
            UINT32_C(0x0000007c));
  T64_CHECK(machine->cpu.fpr[3] == destination);
  t64_machine_destroy(machine);

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] |= UINT64_C(0x04000000);
  machine->cpu.fcr31 = 0u;
  t64_cop1_commit_fpr(machine, 1u, 32u, UINT32_C(0x7fc00001)); /* VR SNaN */
  t64_cop1_commit_fpr(machine, 2u, 32u, UINT32_C(0x3f800000));
  T64_CHECK(execute_fpu_instruction(machine, 16u, 2u, 1u, 3u, 0u));
  T64_CHECK(read_fpr_bits(machine, 3u, 32u) == UINT32_C(0x7f800001));
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) ==
            UINT32_C(0x00010000)); /* VR SNaN produces V */
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0000007c)) ==
            UINT32_C(0x00000040));
  t64_machine_destroy(machine);

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] |= UINT64_C(0x04000000);
  machine->cpu.fcr31 = UINT32_C(0x00000800); /* enable V */
  t64_cop1_commit_fpr(machine, 1u, 32u, UINT32_C(0x7fc00001));
  t64_cop1_commit_fpr(machine, 2u, 32u, UINT32_C(0x3f800000));
  t64_cop1_commit_fpr(machine, 3u, 64u, destination);
  T64_CHECK(execute_fpu_instruction(machine, 16u, 2u, 1u, 3u, 0u));
  T64_CHECK(((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x7c)) ==
            UINT32_C(0x3c)); /* enabled V traps */
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) ==
            UINT32_C(0x00010000));
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0000007c)) == 0u);
  T64_CHECK(machine->cpu.fpr[3] == destination);
  t64_machine_destroy(machine);

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] |= UINT64_C(0x04000000);
  machine->cpu.fcr31 = UINT32_C(0x00000800) | FCR31_CONDITION;
  t64_cop1_commit_fpr(machine, 1u, 32u, UINT32_C(0x7fc00001));
  t64_cop1_commit_fpr(machine, 2u, 32u, UINT32_C(0x3f800000));
  T64_CHECK(execute_fpu_instruction(machine, 16u, 2u, 1u, 0u, 0x32u));
  T64_CHECK(((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x7c)) ==
            UINT32_C(0x3c)); /* signaling compare raises V */
  T64_CHECK((machine->cpu.fcr31 & FCR31_CONDITION) != 0u);
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0000007c)) == 0u);
  t64_machine_destroy(machine);
}

static void assert_conversion_e(const t64_host_allocator *allocator,
                               const t64_profile_descriptor *profile,
                               const uint8_t *asset, uint32_t format,
                               uint32_t source_width, uint64_t source,
                               uint32_t function) {
  const uint64_t destination = UINT64_C(0x1122334455667788);
  t64_machine *machine = make_machine(allocator, profile, asset);

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] |= UINT64_C(0x04000000);
  machine->cpu.fcr31 = UINT32_C(0x0000007c);
  t64_cop1_commit_fpr(machine, 1u, source_width, source);
  t64_cop1_commit_fpr(machine, 3u, 64u, destination);
  T64_CHECK(execute_fpu_instruction(machine, format, 0u, 1u, 3u, function));
  T64_CHECK(((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x7c)) ==
            UINT32_C(0x3c));
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) ==
            UINT32_C(0x00020000)); /* conversion exceptional input is E-only */
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0000007c)) ==
            UINT32_C(0x0000007c));
  T64_CHECK(machine->cpu.fpr[3] == destination);
  t64_machine_destroy(machine);
}

static void assert_conversion_result(const t64_host_allocator *allocator,
                                    const t64_profile_descriptor *profile,
                                    const uint8_t *asset, uint32_t format,
                                    uint32_t source_width, uint64_t source,
                                    uint32_t function,
                                    uint32_t result_width,
                                    uint64_t expected,
                                    uint32_t rounding_mode) {
  t64_machine *machine = make_machine(allocator, profile, asset);

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] |= UINT64_C(0x04000000);
  machine->cpu.fcr31 = rounding_mode;
  t64_cop1_commit_fpr(machine, 1u, source_width, source);
  T64_CHECK(execute_fpu_instruction(machine, format, 0u, 1u, 3u, function));
  T64_CHECK(machine->cpu.attempt_outcome == T64_CPU_ATTEMPT_RETIRED);
  if (read_fpr_bits(machine, 3u, result_width) != expected) {
    fprintf(stderr,
            "conversion mismatch fmt=%u width=%u source=%016llx funct=%02x rm=%u got=%016llx want=%016llx fcr31=%08x\n",
            format, source_width, (unsigned long long)source, function,
            rounding_mode,
            (unsigned long long)read_fpr_bits(machine, 3u, result_width),
            (unsigned long long)expected, machine->cpu.fcr31);
  }
  T64_CHECK(read_fpr_bits(machine, 3u, result_width) == expected);
  t64_machine_destroy(machine);
}

static void test_binary64_and_conversion_boundaries(
    const t64_host_allocator *allocator,
    const t64_profile_descriptor *profile, const uint8_t *asset) {
  const uint64_t destination = UINT64_C(0x1122334455667788);
  t64_machine *machine;

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] |= UINT64_C(0x04000000);
  t64_cop1_commit_fpr(machine, 1u, 64u, UINT64_C(0x3ff0000000000000));
  t64_cop1_commit_fpr(machine, 2u, 64u, UINT64_C(0x0000000000000000));
  T64_CHECK(execute_fpu_instruction(machine, 17u, 2u, 1u, 3u, 3u));
  T64_CHECK(read_fpr_bits(machine, 3u, 64u) ==
            UINT64_C(0x7ff0000000000000));
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) ==
            UINT32_C(0x00008000));
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0000007c)) ==
            UINT32_C(0x00000020));
  t64_machine_destroy(machine);

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] |= UINT64_C(0x04000000);
  t64_cop1_commit_fpr(machine, 1u, 64u, UINT64_C(0x7fefffffffffffff));
  t64_cop1_commit_fpr(machine, 2u, 64u, UINT64_C(0x4000000000000000));
  T64_CHECK(execute_fpu_instruction(machine, 17u, 2u, 1u, 3u, 2u));
  T64_CHECK(read_fpr_bits(machine, 3u, 64u) ==
            UINT64_C(0x7ff0000000000000));
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) ==
            UINT32_C(0x00005000));
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0000007c)) ==
            UINT32_C(0x00000014));
  t64_machine_destroy(machine);

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] |= UINT64_C(0x04000000);
  machine->cpu.fcr31 = UINT32_C(0x01000000); /* FS, RN */
  t64_cop1_commit_fpr(machine, 1u, 64u, UINT64_C(0x0010000000000000));
  t64_cop1_commit_fpr(machine, 2u, 64u, UINT64_C(0x3fe0000000000000));
  T64_CHECK(execute_fpu_instruction(machine, 17u, 2u, 1u, 3u, 2u));
  T64_CHECK(read_fpr_bits(machine, 3u, 64u) == UINT64_C(0));
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f07c)) ==
            UINT32_C(0x0000300c));
  t64_machine_destroy(machine);

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] |= UINT64_C(0x04000000);
  t64_cop1_commit_fpr(machine, 1u, 64u, UINT64_C(0x8000000000000000));
  t64_cop1_commit_fpr(machine, 2u, 64u, UINT64_C(0x0000000000000000));
  T64_CHECK(execute_fpu_instruction(machine, 17u, 2u, 1u, 0u, 0x32u));
  T64_CHECK((machine->cpu.fcr31 & FCR31_CONDITION) != 0u); /* -0 equals +0 */
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) == 0u);
  t64_machine_destroy(machine);

  assert_conversion_result(allocator, profile, asset, 16u, 32u,
                           UINT32_C(0xcf000000), 0x24u, 32u,
                           UINT32_C(0x80000000), 0u); /* exact W minimum */
  assert_conversion_result(allocator, profile, asset, 17u, 64u,
                           UINT64_C(0xc3e0000000000000), 0x25u, 64u,
                           UINT64_C(0x8000000000000000), 0u); /* exact L minimum */
  assert_conversion_result(allocator, profile, asset, 17u, 64u,
                           UINT64_C(0x43dfffffffffffff), 0x25u, 64u,
                           UINT64_C(0x7ffffffffffffc00), 0u); /* L max - 1023 */

  {
    static const uint64_t f32_w_max = UINT64_C(0x4effffff);
    static const uint64_t f64_w_max = UINT64_C(0x41dfffffffc00000);
    static const uint64_t f32_l_max = UINT64_C(0x5effffff);
    static const uint64_t expected_w_max_from_f32 = UINT64_C(0x7fffff80);
    static const uint64_t expected_w_max_from_f64 = UINT64_C(0x7fffffff);
    static const uint64_t expected_l_max_from_f32 =
        UINT64_C(0x7fffff8000000000);
    uint32_t rounding_mode;

    for (rounding_mode = 0u; rounding_mode < 4u; rounding_mode++) {
      assert_conversion_result(allocator, profile, asset, 16u, 32u,
                               f32_w_max, 0x24u, 32u,
                               expected_w_max_from_f32, rounding_mode);
      assert_conversion_result(allocator, profile, asset, 17u, 64u,
                               f64_w_max, 0x24u, 32u,
                               expected_w_max_from_f64, rounding_mode);
      assert_conversion_result(allocator, profile, asset, 16u, 32u,
                               f32_l_max, 0x25u, 64u,
                               expected_l_max_from_f32, rounding_mode);
    }
  }

  assert_conversion_e(allocator, profile, asset, 16u, 32u,
                      UINT32_C(0x4f000000), 0x24u); /* +2^31 to W */
  assert_conversion_e(allocator, profile, asset, 16u, 32u,
                      UINT32_C(0xcf000001), 0x24u); /* below W minimum */
  assert_conversion_e(allocator, profile, asset, 17u, 64u,
                      UINT64_C(0x43e0000000000000), 0x25u); /* +2^63 to L */
  assert_conversion_e(allocator, profile, asset, 16u, 32u,
                      UINT32_C(0x7f800000), 0x24u); /* infinity to W */
  assert_conversion_e(allocator, profile, asset, 17u, 64u,
                      UINT64_C(0x7ff0000000000000), 0x25u); /* infinity to L */
  assert_conversion_e(allocator, profile, asset, 16u, 32u,
                      UINT32_C(0x7f800001), 0x24u); /* QNaN to W */
  assert_conversion_e(allocator, profile, asset, 17u, 64u,
                      UINT64_C(0x7ff0000000000001), 0x25u); /* QNaN to L */
  assert_conversion_e(allocator, profile, asset, 16u, 32u,
                      UINT32_C(0x7fc00001), 0x24u); /* SNaN to W */
  assert_conversion_e(allocator, profile, asset, 17u, 64u,
                      UINT64_C(0x7ff8000000000001), 0x25u); /* SNaN to L */

  machine = make_machine(allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] |= UINT64_C(0x04000000);
  machine->cpu.fcr31 = UINT32_C(0x00000100); /* enable U only */
  t64_cop1_commit_fpr(machine, 1u, 32u, UINT32_C(0x00800000));
  t64_cop1_commit_fpr(machine, 2u, 32u, UINT32_C(0x3f000000));
  t64_cop1_commit_fpr(machine, 3u, 64u, destination);
  T64_CHECK(execute_fpu_instruction(machine, 16u, 2u, 1u, 3u, 2u));
  T64_CHECK(((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x7c)) ==
            UINT32_C(0x3c));
  T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) ==
            UINT32_C(0x00020000)); /* E if U or I is enabled */
  T64_CHECK(machine->cpu.fpr[3] == destination);
  t64_machine_destroy(machine);
}

static void test_fs_and_overflow_rounding_tables(
    const t64_host_allocator *allocator,
    const t64_profile_descriptor *profile, const uint8_t *asset) {
  uint32_t format_index;
  uint32_t sign;
  uint32_t rounding_mode;

  for (format_index = 0u; format_index < 2u; format_index++) {
    uint32_t format = format_index == 0u ? UINT32_C(16) : UINT32_C(17);
    uint32_t width = format_index == 0u ? 32u : 64u;
    uint64_t minimum_normal = format_index == 0u
                                  ? UINT64_C(0x00800000)
                                  : UINT64_C(0x0010000000000000);
    uint64_t half = format_index == 0u ? UINT64_C(0x3f000000)
                                       : UINT64_C(0x3fe0000000000000);
    uint64_t maximum_finite = format_index == 0u
                                  ? UINT64_C(0x7f7fffff)
                                  : UINT64_C(0x7fefffffffffffff);
    uint64_t two = format_index == 0u ? UINT64_C(0x40000000)
                                      : UINT64_C(0x4000000000000000);
    uint64_t positive_infinity = format_index == 0u
                                     ? UINT64_C(0x7f800000)
                                     : UINT64_C(0x7ff0000000000000);
    uint64_t negative_infinity = format_index == 0u
                                     ? UINT64_C(0xff800000)
                                     : UINT64_C(0xfff0000000000000);

    for (sign = 0u; sign < 2u; sign++) {
      for (rounding_mode = 0u; rounding_mode < 4u; rounding_mode++) {
        uint64_t source = minimum_normal |
                          (sign != 0u ? (UINT64_C(1) <<
                                         (format_index == 0u ? 31u : 63u))
                                     : 0u);
        bool outward = (sign == 0u && rounding_mode == 2u) ||
                       (sign != 0u && rounding_mode == 3u);
        uint64_t expected = source &
                            (format_index == 0u ? UINT64_C(0x80000000)
                                                : UINT64_C(0x8000000000000000));
        t64_machine *machine = make_machine(allocator, profile, asset);

        if (outward) expected |= minimum_normal;
        T64_CHECK(machine != NULL);
        if (machine == NULL) continue;
        machine->cpu.cp0[12] |= UINT64_C(0x04000000);
        machine->cpu.fcr31 = UINT32_C(0x01000000) | rounding_mode;
        t64_cop1_commit_fpr(machine, 1u, width, source);
        t64_cop1_commit_fpr(machine, 2u, width, half);
        T64_CHECK(execute_fpu_instruction(machine, format, 2u, 1u, 3u, 2u));
        T64_CHECK(read_fpr_bits(machine, 3u, width) == expected);
        T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f07f)) ==
                  (UINT32_C(0x0000300c) | rounding_mode));
        t64_machine_destroy(machine);
      }
    }

    for (sign = 0u; sign < 2u; sign++) {
      for (rounding_mode = 0u; rounding_mode < 4u; rounding_mode++) {
        bool to_infinity = rounding_mode == 0u ||
                           (sign == 0u && rounding_mode == 2u) ||
                           (sign != 0u && rounding_mode == 3u);
        uint64_t source = maximum_finite |
                          (sign != 0u ? (UINT64_C(1) <<
                                         (format_index == 0u ? 31u : 63u))
                                     : 0u);
        uint64_t expected = to_infinity
                                ? (sign != 0u ? negative_infinity
                                              : positive_infinity)
                                : source;
        t64_machine *machine = make_machine(allocator, profile, asset);

        T64_CHECK(machine != NULL);
        if (machine == NULL) continue;
        machine->cpu.cp0[12] |= UINT64_C(0x04000000);
        machine->cpu.fcr31 = rounding_mode;
        t64_cop1_commit_fpr(machine, 1u, width, source);
        t64_cop1_commit_fpr(machine, 2u, width, two);
        T64_CHECK(execute_fpu_instruction(machine, format, 2u, 1u, 3u, 2u));
        T64_CHECK(read_fpr_bits(machine, 3u, width) == expected);
        T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f07f)) ==
                  (UINT32_C(0x00005014) | rounding_mode));
        t64_machine_destroy(machine);
      }
    }
  }
}

static void test_nan_arithmetic_transfer_and_compare_exceptions(
    const t64_host_allocator *allocator,
    const t64_profile_descriptor *profile, const uint8_t *asset) {
  uint32_t format_index;

  for (format_index = 0u; format_index < 2u; format_index++) {
    uint32_t format = format_index == 0u ? UINT32_C(16) : UINT32_C(17);
    uint32_t width = format_index == 0u ? 32u : 64u;
    uint64_t one = format_index == 0u ? UINT64_C(0x3f800000)
                                     : UINT64_C(0x3ff0000000000000);
    uint64_t quiet_nan = format_index == 0u ? UINT64_C(0x7f800001)
                                            : UINT64_C(0x7ff0000000000001);
    uint64_t signaling_nan = format_index == 0u ? UINT64_C(0x7fc00001)
                                                : UINT64_C(0x7ff8000000000001);
    uint64_t subnormal = UINT64_C(1);
    uint64_t destination = UINT64_C(0x1122334455667788);
    t64_machine *machine;

    machine = make_machine(allocator, profile, asset);
    T64_CHECK(machine != NULL);
    if (machine == NULL) continue;
    machine->cpu.cp0[12] |= UINT64_C(0x04000000);
    t64_cop1_commit_fpr(machine, 1u, width, quiet_nan);
    t64_cop1_commit_fpr(machine, 2u, width, one);
    t64_cop1_commit_fpr(machine, 3u, 64u, destination);
    T64_CHECK(execute_fpu_instruction(machine, format, 0u, 1u, 3u, 0x05u));
    T64_CHECK(((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x7c)) ==
              UINT32_C(0x3c)); /* ABS is arithmetic for QNaN exception rules */
    T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) ==
              UINT32_C(0x00020000));
    T64_CHECK(machine->cpu.fpr[3] == destination);
    t64_machine_destroy(machine);

    machine = make_machine(allocator, profile, asset);
    T64_CHECK(machine != NULL);
    if (machine == NULL) continue;
    machine->cpu.cp0[12] |= UINT64_C(0x04000000);
    machine->cpu.fcr31 = UINT32_C(0x0001007c);
    t64_cop1_commit_fpr(machine, 1u, width, quiet_nan);
    t64_cop1_commit_fpr(machine, 3u, width, destination);
    T64_CHECK(execute_fpu_instruction(machine, format, 0u, 1u, 3u, 0x06u));
    T64_CHECK(read_fpr_bits(machine, 3u, width) == quiet_nan); /* MOV copies */
    T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) ==
              UINT32_C(0x00010000)); /* MOV leaves Cause untouched */
    T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0000007c)) ==
              UINT32_C(0x0000007c));
    T64_CHECK(machine->cpu.cp0[13] == UINT64_C(0));
    t64_machine_destroy(machine);

    machine = make_machine(allocator, profile, asset);
    T64_CHECK(machine != NULL);
    if (machine == NULL) continue;
    machine->cpu.cp0[12] |= UINT64_C(0x04000000);
    t64_cop1_commit_fpr(machine, 1u, width, signaling_nan);
    t64_cop1_commit_fpr(machine, 3u, width, destination);
    T64_CHECK(execute_fpu_instruction(machine, format, 0u, 1u, 3u, 0x07u));
    T64_CHECK(read_fpr_bits(machine, 3u, width) == quiet_nan);
    T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) ==
              UINT32_C(0x00010000)); /* NEG is arithmetic for SNaN */
    T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0000007c)) ==
              UINT32_C(0x00000040));
    t64_machine_destroy(machine);

    machine = make_machine(allocator, profile, asset);
    T64_CHECK(machine != NULL);
    if (machine == NULL) continue;
    machine->cpu.cp0[12] |= UINT64_C(0x04000000);
    t64_cop1_commit_fpr(machine, 1u, width, subnormal);
    t64_cop1_commit_fpr(machine, 2u, width, 0u);
    T64_CHECK(execute_fpu_instruction(machine, format, 2u, 1u, 0u, 0x34u));
    T64_CHECK((machine->cpu.fcr31 & FCR31_CONDITION) == 0u);
    T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) == 0u);
    t64_machine_destroy(machine);

    if (format == UINT32_C(17)) {
      machine = make_machine(allocator, profile, asset);
      T64_CHECK(machine != NULL);
      if (machine == NULL) continue;
      machine->cpu.cp0[12] &= ~UINT64_C(0x04000000); /* paired FR=0 */
      t64_cop1_commit_fpr(machine, 2u, width, one);
      t64_cop1_commit_fpr(machine, 4u, width,
                          UINT64_C(0x4000000000000000));
      t64_cop1_commit_fpr(machine, 6u, width, destination);
      T64_CHECK(execute_fpu_instruction(machine, format, 4u, 2u, 7u, 0x00u));
      T64_CHECK(((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x7c)) ==
                UINT32_C(0x3c)); /* odd D destination is unimplemented */
      T64_CHECK((machine->cpu.fcr31 & UINT32_C(0x0003f000)) ==
                UINT32_C(0x00020000));
      T64_CHECK((uint32_t)machine->cpu.fpr[6] ==
                (uint32_t)destination);
      T64_CHECK((uint32_t)machine->cpu.fpr[7] ==
                (uint32_t)(destination >> 32));
      t64_machine_destroy(machine);
    }
  }
}

int main(int argc, char **argv) {
  allocator_state allocation = {0u};
  const t64_host_allocator allocator = {allocate, release, &allocation};
  const t64_profile_descriptor *profile = NULL;
  uint8_t asset[IPL3_ASSET_BYTES];
  t64_machine *machine;

  if (!read_asset(asset) ||
      t64_profile_lookup("ntsc-cart-4m-openipl3-vr4300-v1", &profile) !=
          T64_STATUS_OK || profile == NULL) {
    puts("1..1\nnot ok 1 - qualified IPL3 and VR4300 profile setup");
    return 1;
  }
  if (argc == 2 && strcmp(argv[1], "--arithmetic") == 0) {
    test_arithmetic_tie_rounding(&allocator, profile, asset);
    test_exact_arithmetic_operations(&allocator, profile, asset);
    test_comparison_predicate_bits(&allocator, profile, asset);
    test_format_and_integer_conversions(&allocator, profile, asset);
    test_compat_v2_fr0_odd_register_aliases(&allocator, asset);
    test_paired_registers_and_compare_branch(&allocator, profile, asset);
    test_interleaved_machine_rounding(&allocator, profile, asset);
    T64_CHECK(allocation.outstanding == 0u);
    return T64_TEST_FINISH("unit.core.cop1-arithmetic");
  }
  if (argc == 2 && strcmp(argv[1], "--exceptions") == 0) {
    test_invalid_integer_source_format(&allocator, profile, asset);
    test_ieee_and_fs_exception_matrix(&allocator, profile, asset);
    test_nan_exception_polarity_and_suppression(&allocator, profile, asset);
    test_binary64_and_conversion_boundaries(&allocator, profile, asset);
    test_fs_and_overflow_rounding_tables(&allocator, profile, asset);
    test_nan_arithmetic_transfer_and_compare_exceptions(&allocator, profile,
                                                       asset);
    T64_CHECK(allocation.outstanding == 0u);
    return T64_TEST_FINISH("unit.core.cop1-exceptions");
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--arithmetic|--exceptions]\n", argv[0]);
    return 2;
  }
  machine = make_machine(&allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine != NULL) {
    T64_CHECK(advance_tick(machine, UINT64_C(1)));
    T64_CHECK(machine->cpu.attempt_outcome == T64_CPU_ATTEMPT_RETIRED);
    T64_CHECK(machine->cpu.pc_full == TEST_PC + UINT64_C(4));
    T64_CHECK(machine->cpu.delay_slot_pending);
    T64_CHECK(machine->cpu.delay_target_full == TEST_PC + UINT64_C(12));
    T64_CHECK(machine->cpu.cp0[13] == UINT64_C(0));
    t64_machine_destroy(machine);
  }
  test_branch_forms(&allocator, profile, asset);
  test_branch_exceptions(&allocator, profile, asset);
  test_rsp_functional_profile_cop1_transfer(&allocator, asset);
  test_control_registers(&allocator, profile, asset);
  test_control_register_walking_bits(&allocator, profile, asset);
  test_register_lanes(&allocator, profile, asset);
  test_integer_fpr_transfers(&allocator, profile, asset);
  test_decoded_all_register_transfers(&allocator, profile, asset);
  test_memory_alignment_faults(&allocator, profile, asset);
  test_singleword_memory_operations(&allocator, profile, asset);
  test_tlb_permission_and_cache_failures(&allocator, profile, asset);
  test_cause_and_sticky_flag_ownership(&allocator, profile, asset);
  T64_CHECK(allocation.outstanding == 0u);
  return T64_TEST_FINISH("unit.core.cop1-state");
}
