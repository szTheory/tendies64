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
    T64_CHECK(allocation.outstanding == 0u);
    return T64_TEST_FINISH("unit.core.cop1-arithmetic");
  }
  if (argc == 2 && strcmp(argv[1], "--exceptions") == 0) {
    test_invalid_integer_source_format(&allocator, profile, asset);
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
