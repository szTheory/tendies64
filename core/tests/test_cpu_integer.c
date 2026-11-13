#include "t64_internal.h"
#include "tendies64/t64_machine.h"
#include "t64_test.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IPL3_ASSET_BYTES 4096u
#define TEST_PC UINT64_C(0xffffffff80001000)
#define TEST_PHYSICAL_PC UINT32_C(0x00001000)
#define TEST_CAUSE UINT32_C(13)
#define TEST_STATUS_EXL UINT32_C(0x00000002)
#define TEST_STATUS_CU2 UINT64_C(0x40000000)
#define TEST_STATUS_CU3 UINT64_C(0x80000000)

#define ENCODE_R(rs, rt, rd, sa, fn)                                           \
  ((((uint32_t)(rs) & UINT32_C(0x1f)) << 21) |                                \
   (((uint32_t)(rt) & UINT32_C(0x1f)) << 16) |                                \
   (((uint32_t)(rd) & UINT32_C(0x1f)) << 11) |                                \
   (((uint32_t)(sa) & UINT32_C(0x1f)) << 6) |                                 \
   ((uint32_t)(fn) & UINT32_C(0x3f)))

#define ENCODE_I(op, rs, rt, imm)                                              \
  ((((uint32_t)(op) & UINT32_C(0x3f)) << 26) |                               \
   (((uint32_t)(rs) & UINT32_C(0x1f)) << 21) |                                \
   (((uint32_t)(rt) & UINT32_C(0x1f)) << 16) |                                \
   ((uint32_t)(uint16_t)(imm)))

#define ENCODE_J(op, target)                                                   \
  ((((uint32_t)(op) & UINT32_C(0x3f)) << 26) |                               \
   ((uint32_t)(target) & UINT32_C(0x03ffffff)))

#define ENCODE_COP_TRANSFER(op, format, rt, rd)                               \
  ((((uint32_t)(op) & UINT32_C(0x3f)) << 26) |                               \
   (((uint32_t)(format) & UINT32_C(0x1f)) << 21) |                           \
   (((uint32_t)(rt) & UINT32_C(0x1f)) << 16) |                              \
   (((uint32_t)(rd) & UINT32_C(0x1f)) << 11))

typedef struct {
  size_t outstanding;
} allocator_state;

typedef struct {
  const t64_host_allocator *allocator;
  const t64_profile_descriptor *profile;
  const uint8_t *asset;
} test_environment;

typedef void (*cpu_case_fn)(const test_environment *environment);

typedef struct {
  const char *name;
  cpu_case_fn run;
} cpu_case;

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

static void check_bool(const char *label, bool passed) {
  t64_test_checks++;
  if (!passed) {
    t64_test_failures++;
    (void)fprintf(stderr, "# failed: %s\n", label);
  }
}

static void check_u64(const char *label, uint64_t actual, uint64_t expected) {
  t64_test_checks++;
  if (actual != expected) {
    t64_test_failures++;
    (void)fprintf(stderr,
                  "# failed: %s expected=0x%016llx observed=0x%016llx\n",
                  label, (unsigned long long)expected,
                  (unsigned long long)actual);
  }
}

static void write_rdram_instruction(t64_machine *machine,
                                    uint32_t physical_address,
                                    uint32_t opcode) {
  machine->ram[physical_address] = (uint8_t)(opcode >> 24);
  machine->ram[physical_address + 1u] = (uint8_t)(opcode >> 16);
  machine->ram[physical_address + 2u] = (uint8_t)(opcode >> 8);
  machine->ram[physical_address + 3u] = (uint8_t)opcode;
}

static t64_machine *prepare_program(const test_environment *environment,
                                    uint64_t pc, const uint32_t *program,
                                    size_t instruction_count) {
  t64_machine *machine = NULL;
  uint32_t physical_pc = (uint32_t)pc & UINT32_C(0x1fffffff);
  size_t index;

  if (environment == NULL || environment->allocator == NULL ||
      environment->profile == NULL || environment->asset == NULL ||
      program == NULL || instruction_count == 0u ||
      t64_machine_create(environment->allocator, environment->profile,
                         &machine) != T64_STATUS_OK ||
      machine == NULL) {
    return NULL;
  }
  if (t64_machine_admit_rom(machine, environment->asset, IPL3_ASSET_BYTES) !=
      T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return NULL;
  }
  (void)memset(machine->cpu.gpr, 0, sizeof machine->cpu.gpr);
  machine->cpu.hi = UINT64_C(0);
  machine->cpu.lo = UINT64_C(0);
  machine->cpu.pc = (uint32_t)pc;
  machine->cpu.pc_full = pc;
  for (index = 0u; index < instruction_count; index++) {
    write_rdram_instruction(machine,
                            physical_pc + (uint32_t)(index * 4u),
                            program[index]);
  }
  return machine;
}

static t64_machine *prepare_profile_program(
    const test_environment *environment, const char *profile_id,
    uint64_t pc, const uint32_t *program, size_t instruction_count) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint32_t physical_pc = (uint32_t)pc & UINT32_C(0x1fffffff);
  size_t index;

  if (environment == NULL || environment->allocator == NULL ||
      environment->asset == NULL || program == NULL ||
      profile_id == NULL || instruction_count == 0u ||
      t64_profile_lookup(profile_id, &profile) != T64_STATUS_OK ||
      profile == NULL ||
      t64_machine_create(environment->allocator, profile, &machine) !=
          T64_STATUS_OK ||
      machine == NULL) {
    return NULL;
  }
  if (t64_machine_admit_rom(machine, environment->asset, IPL3_ASSET_BYTES) !=
      T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return NULL;
  }
  (void)memset(machine->cpu.gpr, 0, sizeof machine->cpu.gpr);
  machine->cpu.hi = UINT64_C(0);
  machine->cpu.lo = UINT64_C(0);
  machine->cpu.pc = (uint32_t)pc;
  machine->cpu.pc_full = pc;
  for (index = 0u; index < instruction_count; index++) {
    write_rdram_instruction(machine,
                            physical_pc + (uint32_t)(index * 4u),
                            program[index]);
  }
  return machine;
}

static t64_machine *prepare_compat_program(
    const test_environment *environment, uint64_t pc,
    const uint32_t *program, size_t instruction_count) {
  return prepare_profile_program(
      environment, "ntsc-cart-4m-openipl3-vr4300-compat-v1", pc, program,
      instruction_count);
}

static t64_machine *prepare_compat_v2_program(
    const test_environment *environment, uint64_t pc,
    const uint32_t *program, size_t instruction_count) {
  return prepare_profile_program(
      environment, "ntsc-cart-4m-openipl3-vr4300-compat-v2", pc, program,
      instruction_count);
}

static void destroy_machine(t64_machine *machine) {
  if (machine != NULL) {
    t64_machine_destroy(machine);
  }
}

static t64_status advance_to_tick(t64_machine *machine, uint64_t tick,
                                  t64_stop_result *result_out) {
  t64_stop_request request = {T64_STOP_GUEST_TICK, tick};

  return t64_machine_advance(machine, &request, result_out);
}

static void check_bounded_tick(t64_machine *machine, uint64_t tick,
                               const char *label) {
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_status status = advance_to_tick(machine, tick, &result);

  check_bool(label, status == T64_STATUS_OK &&
                        result.reason == T64_STOP_REASON_BOUND &&
                        result.tick == tick);
}

static void check_exception(t64_machine *machine, uint64_t code,
                            uint64_t pc, uint64_t retired,
                            const char *label) {
  uint32_t cause = (uint32_t)machine->cpu.cp0[13];

  check_bool(label,
             machine->cpu.attempt_outcome == T64_CPU_ATTEMPT_GUEST_EXCEPTION &&
                 (cause & UINT32_C(0x8000007c)) == (uint32_t)(code << 2) &&
                 machine->cpu.cp0[14] == pc &&
                 (machine->cpu.cp0[12] & TEST_STATUS_EXL) != 0u &&
                 machine->cpu.retired_instructions == retired);
}

static void check_unsupported(t64_machine *machine, uint64_t tick,
                              const char *label) {
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_status status = advance_to_tick(machine, tick, &result);

  check_bool(label, status == T64_STATUS_OK &&
                        result.reason == T64_STOP_REASON_CPU_UNSUPPORTED &&
                        result.tick == tick && machine->cpu.stopped &&
                        machine->cpu.attempt_outcome ==
                            T64_CPU_ATTEMPT_MODEL_UNSUPPORTED);
}

static bool check_fetched_instruction(t64_machine *machine,
                                      uint64_t expected_pc,
                                      uint32_t expected_opcode,
                                      const char *label) {
  bool matched;

  if (machine == NULL) return false;
  matched = machine->cpu.last_pc_full == expected_pc &&
            machine->cpu.last_opcode == expected_opcode;
  check_u64(label, machine->cpu.last_pc_full, expected_pc);
  check_u64("attempt fetched authored opcode", machine->cpu.last_opcode,
            expected_opcode);
  (void)fprintf(stderr,
                "# %s attempt pc=0x%016llx opcode=0x%08x outcome=%u "
                "cause=0x%016llx status=0x%016llx\n",
                label, (unsigned long long)machine->cpu.last_pc_full,
                (unsigned)machine->cpu.last_opcode,
                (unsigned)machine->cpu.attempt_outcome,
                (unsigned long long)machine->cpu.cp0[13],
                (unsigned long long)machine->cpu.cp0[12]);
  return matched;
}

static void test_alu_word_doubleword_and_immediate(
    const test_environment *environment) {
  static const uint32_t program[] = {
      ENCODE_R(8u, 14u, 15u, 0u, 0x20u), /* add */
      ENCODE_R(8u, 14u, 16u, 0u, 0x21u), /* addu */
      ENCODE_R(8u, 14u, 17u, 0u, 0x22u), /* sub */
      ENCODE_R(8u, 14u, 18u, 0u, 0x23u), /* subu */
      ENCODE_R(10u, 14u, 19u, 0u, 0x2cu), /* dadd */
      ENCODE_R(10u, 14u, 20u, 0u, 0x2du), /* daddu */
      ENCODE_R(10u, 14u, 21u, 0u, 0x2eu), /* dsub */
      ENCODE_R(10u, 14u, 22u, 0u, 0x2fu), /* dsubu */
      ENCODE_R(11u, 10u, 23u, 0u, 0x24u), /* and */
      ENCODE_R(11u, 10u, 24u, 0u, 0x25u), /* or */
      ENCODE_R(11u, 10u, 25u, 0u, 0x26u), /* xor */
      ENCODE_R(11u, 10u, 26u, 0u, 0x27u), /* nor */
      ENCODE_R(12u, 10u, 27u, 0u, 0x2au), /* slt */
      ENCODE_R(12u, 10u, 28u, 0u, 0x2bu), /* sltu */
      ENCODE_I(0x08u, 8u, 29u, -1),       /* addi */
      ENCODE_I(0x09u, 8u, 30u, -1),       /* addiu */
      ENCODE_I(0x0au, 12u, 31u, -1),      /* slti */
      ENCODE_I(0x0bu, 10u, 7u, -1),        /* sltiu */
      ENCODE_I(0x0cu, 11u, 6u, 0x00ff),    /* andi */
      ENCODE_I(0x0du, 10u, 5u, 0x00ff),    /* ori */
      ENCODE_I(0x0eu, 10u, 4u, 0xffff),    /* xori */
      ENCODE_I(0x0fu, 0u, 3u, 0x8000),     /* lui */
      ENCODE_I(0x18u, 10u, 2u, -1),        /* daddi */
      ENCODE_I(0x19u, 10u, 1u, -1),        /* daddiu */
      ENCODE_R(8u, 14u, 0u, 0u, 0x21u),    /* zero-register destination */
  };
  const size_t count = sizeof program / sizeof program[0];
  t64_machine *machine = prepare_program(environment, TEST_PC, program, count);

  check_bool("ALU program fixture admitted", machine != NULL);
  if (machine == NULL) {
    return;
  }
  machine->cpu.gpr[8] = UINT64_C(0xdeadbeef7ffffffe);
  machine->cpu.gpr[9] = UINT64_C(1);
  machine->cpu.gpr[10] = UINT64_C(0x00000001fffffffe);
  machine->cpu.gpr[11] = UINT64_MAX;
  machine->cpu.gpr[12] = UINT64_C(0x8000000000000000);
  machine->cpu.gpr[14] = UINT64_C(1);
  check_bounded_tick(machine, count, "ALU matrix retires every instruction");
  check_u64("ADD word sign extension", machine->cpu.gpr[15],
            UINT64_C(0x7fffffff));
  check_u64("ADDU word sign extension", machine->cpu.gpr[16],
            UINT64_C(0x7fffffff));
  check_u64("SUB word result", machine->cpu.gpr[17], UINT64_C(0x7ffffffd));
  check_u64("SUBU word result", machine->cpu.gpr[18], UINT64_C(0x7ffffffd));
  check_u64("DADD doubleword result", machine->cpu.gpr[19],
            UINT64_C(0x00000001ffffffff));
  check_u64("DADDU doubleword result", machine->cpu.gpr[20],
            UINT64_C(0x00000001ffffffff));
  check_u64("DSUB doubleword result", machine->cpu.gpr[21],
            UINT64_C(0x00000001fffffffd));
  check_u64("DSUBU doubleword result", machine->cpu.gpr[22],
            UINT64_C(0x00000001fffffffd));
  check_u64("AND doubleword", machine->cpu.gpr[23],
            UINT64_C(0x00000001fffffffe));
  check_u64("OR doubleword", machine->cpu.gpr[24], UINT64_MAX);
  check_u64("XOR doubleword", machine->cpu.gpr[25],
            UINT64_C(0xfffffffe00000001));
  check_u64("NOR doubleword", machine->cpu.gpr[26], UINT64_C(0));
  check_u64("SLT signed extrema", machine->cpu.gpr[27], UINT64_C(1));
  check_u64("SLTU unsigned extrema", machine->cpu.gpr[28], UINT64_C(0));
  check_u64("ADDI sign-extended immediate", machine->cpu.gpr[29],
            UINT64_C(0x7ffffffd));
  check_u64("ADDIU sign-extended immediate", machine->cpu.gpr[30],
            UINT64_C(0x7ffffffd));
  check_u64("SLTI signed compare", machine->cpu.gpr[31], UINT64_C(1));
  check_u64("SLTIU unsigned sign-extended immediate", machine->cpu.gpr[7],
            UINT64_C(1));
  check_u64("ANDI zero-extended immediate", machine->cpu.gpr[6],
            UINT64_C(0x00ff));
  check_u64("ORI zero-extended immediate", machine->cpu.gpr[5],
            UINT64_C(0x00000001ffffffff));
  check_u64("XORI zero-extended immediate", machine->cpu.gpr[4],
            UINT64_C(0x00000001ffff0001));
  check_u64("LUI sign-extends its word result", machine->cpu.gpr[3],
            UINT64_C(0xffffffff80000000));
  check_u64("DADDI doubleword immediate", machine->cpu.gpr[2],
            UINT64_C(0x00000001fffffffd));
  check_u64("DADDIU doubleword immediate", machine->cpu.gpr[1],
            UINT64_C(0x00000001fffffffd));
  check_u64("GPR zero remains zero", machine->cpu.gpr[0], UINT64_C(0));
  destroy_machine(machine);
}

static void test_shift_matrix(const test_environment *environment) {
  static const uint32_t program[] = {
      ENCODE_R(0u, 8u, 10u, 31u, 0x00u), /* sll */
      ENCODE_R(0u, 8u, 11u, 31u, 0x02u), /* srl */
      ENCODE_R(0u, 28u, 12u, 31u, 0x03u), /* sra */
      ENCODE_R(9u, 8u, 13u, 0u, 0x04u), /* sllv */
      ENCODE_R(9u, 8u, 14u, 0u, 0x06u), /* srlv */
      ENCODE_R(9u, 28u, 15u, 0u, 0x07u), /* srav */
      ENCODE_R(25u, 26u, 16u, 0u, 0x14u), /* dsllv */
      ENCODE_R(25u, 27u, 17u, 0u, 0x16u), /* dsrlv */
      ENCODE_R(25u, 27u, 18u, 0u, 0x17u), /* dsrav */
      ENCODE_R(0u, 26u, 19u, 31u, 0x38u), /* dsll */
      ENCODE_R(0u, 27u, 20u, 31u, 0x3au), /* dsrl */
      ENCODE_R(0u, 27u, 21u, 31u, 0x3bu), /* dsra */
      ENCODE_R(0u, 26u, 22u, 31u, 0x3cu), /* dsll32 */
      ENCODE_R(0u, 27u, 23u, 31u, 0x3eu), /* dsrl32 */
      ENCODE_R(0u, 27u, 24u, 31u, 0x3fu), /* dsra32 */
      ENCODE_R(0u, 8u, 29u, 0u, 0x00u), /* sll by zero */
      ENCODE_R(0u, 26u, 30u, 0u, 0x38u), /* dsll by zero */
      ENCODE_R(0u, 26u, 31u, 0u, 0x3cu), /* dsll32 by 32 */
      ENCODE_R(0u, 27u, 1u, 0u, 0x16u), /* dsrlv by zero */
      ENCODE_R(0u, 27u, 2u, 0u, 0x3eu), /* dsrl32 by 32 */
      ENCODE_R(0u, 27u, 3u, 0u, 0x3fu), /* dsra32 by 32 */
      ENCODE_R(0u, 28u, 4u, 0u, 0x03u), /* sra by zero */
  };
  const size_t count = sizeof program / sizeof program[0];
  t64_machine *machine = prepare_program(environment, TEST_PC, program, count);

  check_bool("shift program fixture admitted", machine != NULL);
  if (machine == NULL) {
    return;
  }
  machine->cpu.gpr[8] = UINT64_C(0x80000001);
  machine->cpu.gpr[9] = UINT64_C(31);
  machine->cpu.gpr[26] = UINT64_C(1);
  machine->cpu.gpr[11] = UINT64_C(0x8000000000000001);
  machine->cpu.gpr[27] = UINT64_C(0x8000000000000001);
  machine->cpu.gpr[28] = UINT64_C(0x80000001);
  machine->cpu.gpr[25] = UINT64_C(63);
  check_bounded_tick(machine, count, "shift matrix retires every instruction");
  check_u64("SLL by 31", machine->cpu.gpr[10],
            UINT64_C(0xffffffff80000000));
  check_u64("SRL by 31", machine->cpu.gpr[11], UINT64_C(1));
  check_u64("SRA by 31", machine->cpu.gpr[12], UINT64_MAX);
  check_u64("SLLV masks amount to five bits", machine->cpu.gpr[13],
            UINT64_C(0xffffffff80000000));
  check_u64("SRLV variable by 31", machine->cpu.gpr[14], UINT64_C(1));
  check_u64("SRAV variable sign-fills", machine->cpu.gpr[15], UINT64_MAX);
  check_u64("DSLLV by 63", machine->cpu.gpr[16],
            UINT64_C(0x8000000000000000));
  check_u64("DSRLV by 63", machine->cpu.gpr[17], UINT64_C(1));
  check_u64("DSRAV by 63", machine->cpu.gpr[18], UINT64_MAX);
  check_u64("DSLL by 31", machine->cpu.gpr[19], UINT64_C(0x80000000));
  check_u64("DSRL by 31", machine->cpu.gpr[20],
            UINT64_C(0x0000000100000000));
  check_u64("DSRA by 31", machine->cpu.gpr[21],
            UINT64_C(0xffffffff00000000));
  check_u64("DSLL32 by 63", machine->cpu.gpr[22],
            UINT64_C(0x8000000000000000));
  check_u64("DSRL32 by 63", machine->cpu.gpr[23], UINT64_C(1));
  check_u64("DSRA32 by 63", machine->cpu.gpr[24], UINT64_MAX);
  check_u64("SLL by zero preserves low word and sign-extends",
            machine->cpu.gpr[29], UINT64_C(0xffffffff80000001));
  check_u64("DSLL by zero preserves doubleword", machine->cpu.gpr[30],
            UINT64_C(1));
  check_u64("DSLL32 covers shift 32", machine->cpu.gpr[31],
            UINT64_C(0x0000000100000000));
  check_u64("DSRLV by zero preserves doubleword", machine->cpu.gpr[1],
            UINT64_C(0x8000000000000001));
  check_u64("DSRL32 covers shift 32", machine->cpu.gpr[2],
            UINT64_C(0x0000000080000000));
  check_u64("DSRA32 covers shift 32", machine->cpu.gpr[3],
            UINT64_C(0xffffffff80000000));
  check_u64("SRA by zero sign-extends word", machine->cpu.gpr[4],
            UINT64_C(0xffffffff80000001));
  destroy_machine(machine);
}

static void test_hilo_multiply_and_move_matrix(
    const test_environment *environment) {
  static const uint32_t program[] = {
      ENCODE_R(8u, 0u, 0u, 0u, 0x11u), /* mthi */
      ENCODE_R(9u, 0u, 0u, 0u, 0x13u), /* mtlo */
      ENCODE_R(0u, 0u, 10u, 0u, 0x10u), /* mfhi */
      ENCODE_R(0u, 0u, 11u, 0u, 0x12u), /* mflo */
      ENCODE_R(8u, 9u, 0u, 0u, 0x18u), /* mult */
      ENCODE_R(0u, 0u, 12u, 0u, 0x12u), /* mflo */
      ENCODE_R(0u, 0u, 13u, 0u, 0x10u), /* mfhi */
      ENCODE_R(14u, 15u, 0u, 0u, 0x19u), /* multu */
      ENCODE_R(0u, 0u, 16u, 0u, 0x12u), /* mflo */
      ENCODE_R(0u, 0u, 17u, 0u, 0x10u), /* mfhi */
      ENCODE_R(18u, 19u, 0u, 0u, 0x1cu), /* dmult */
      ENCODE_R(0u, 0u, 20u, 0u, 0x12u), /* mflo */
      ENCODE_R(0u, 0u, 21u, 0u, 0x10u), /* mfhi */
      ENCODE_R(22u, 23u, 0u, 0u, 0x1du), /* dmultu */
      ENCODE_R(0u, 0u, 24u, 0u, 0x12u), /* mflo */
      ENCODE_R(0u, 0u, 25u, 0u, 0x10u), /* mfhi */
      ENCODE_R(0u, 0u, 0u, 0u, 0x0fu), /* sync */
      ENCODE_R(26u, 27u, 0u, 0u, 0x1du), /* dmultu full cross carries */
      ENCODE_R(0u, 0u, 28u, 0u, 0x12u), /* mflo */
      ENCODE_R(0u, 0u, 29u, 0u, 0x10u), /* mfhi */
  };
  const size_t count = sizeof program / sizeof program[0];
  t64_machine *machine = prepare_program(environment, TEST_PC, program, count);

  check_bool("HI/LO program fixture admitted", machine != NULL);
  if (machine == NULL) {
    return;
  }
  machine->cpu.gpr[8] = UINT64_C(0xffffffff80000002);
  machine->cpu.gpr[9] = UINT64_C(3);
  machine->cpu.gpr[14] = UINT64_MAX;
  machine->cpu.gpr[15] = UINT64_C(2);
  machine->cpu.gpr[18] = UINT64_C(0xfffffffffffffffe);
  machine->cpu.gpr[19] = UINT64_C(3);
  machine->cpu.gpr[22] = UINT64_C(0x0000000100000000);
  machine->cpu.gpr[23] = UINT64_C(0x0000000200000003);
  machine->cpu.gpr[26] = UINT64_MAX;
  machine->cpu.gpr[27] = UINT64_MAX;
  check_bounded_tick(machine, count, "HI/LO matrix retires including SYNC");
  check_u64("MTHI then MFHI", machine->cpu.gpr[10],
            UINT64_C(0xffffffff80000002));
  check_u64("MTLO then MFLO", machine->cpu.gpr[11], UINT64_C(3));
  check_u64("MULT signed LO", machine->cpu.gpr[12],
            UINT64_C(0xffffffff80000006));
  check_u64("MULT signed HI", machine->cpu.gpr[13], UINT64_C(0xfffffffffffffffe));
  check_u64("MULTU unsigned LO", machine->cpu.gpr[16], UINT64_C(0xfffffffffffffffe));
  check_u64("MULTU unsigned HI", machine->cpu.gpr[17], UINT64_C(1));
  check_u64("DMULT signed LO", machine->cpu.gpr[20], UINT64_C(0xfffffffffffffffa));
  check_u64("DMULT signed HI", machine->cpu.gpr[21], UINT64_MAX);
  check_u64("DMULTU limb product LO", machine->cpu.gpr[24],
            UINT64_C(0x0000000300000000));
  check_u64("DMULTU limb product HI", machine->cpu.gpr[25], UINT64_C(2));
  check_u64("DMULTU all-one limb product LO", machine->cpu.gpr[28],
            UINT64_C(1));
  check_u64("DMULTU all-one limb product HI", machine->cpu.gpr[29],
            UINT64_C(0xfffffffffffffffe));
  check_bool("SYNC preserves machine state and retires",
             machine->cpu.retired_instructions == count &&
                 machine->cpu.pc_full == TEST_PC + (count * 4u));
  destroy_machine(machine);
}

static void test_hilo_division_matrix(const test_environment *environment) {
  static const uint32_t program[] = {
      ENCODE_R(8u, 9u, 0u, 0u, 0x1au), /* div */
      ENCODE_R(0u, 0u, 10u, 0u, 0x12u), /* mflo */
      ENCODE_R(0u, 0u, 11u, 0u, 0x10u), /* mfhi */
      ENCODE_R(12u, 13u, 0u, 0u, 0x1bu), /* divu */
      ENCODE_R(0u, 0u, 14u, 0u, 0x12u), /* mflo */
      ENCODE_R(0u, 0u, 15u, 0u, 0x10u), /* mfhi */
      ENCODE_R(16u, 17u, 0u, 0u, 0x1eu), /* ddiv */
      ENCODE_R(0u, 0u, 18u, 0u, 0x12u), /* mflo */
      ENCODE_R(0u, 0u, 19u, 0u, 0x10u), /* mfhi */
      ENCODE_R(20u, 21u, 0u, 0u, 0x1fu), /* ddivu */
      ENCODE_R(0u, 0u, 22u, 0u, 0x12u), /* mflo */
      ENCODE_R(0u, 0u, 23u, 0u, 0x10u), /* mfhi */
  };
  const size_t count = sizeof program / sizeof program[0];
  t64_machine *machine = prepare_program(environment, TEST_PC, program, count);

  check_bool("division program fixture admitted", machine != NULL);
  if (machine == NULL) {
    return;
  }
  machine->cpu.gpr[8] = UINT64_C(0xffffffffffffff9c); /* -100 */
  machine->cpu.gpr[9] = UINT64_C(7);
  machine->cpu.gpr[12] = UINT64_MAX;
  machine->cpu.gpr[13] = UINT64_C(3);
  machine->cpu.gpr[16] = UINT64_C(0xffffffffffffff9c); /* -100 */
  machine->cpu.gpr[17] = UINT64_C(7);
  machine->cpu.gpr[20] = UINT64_MAX;
  machine->cpu.gpr[21] = UINT64_C(3);
  check_bounded_tick(machine, count, "division matrix retires all four widths");
  check_u64("DIV quotient", machine->cpu.gpr[10], UINT64_C(0xfffffffffffffff2));
  check_u64("DIV remainder", machine->cpu.gpr[11], UINT64_C(0xfffffffffffffffe));
  check_u64("DIVU quotient", machine->cpu.gpr[14], UINT64_C(0x55555555));
  check_u64("DIVU remainder", machine->cpu.gpr[15], UINT64_C(0));
  check_u64("DDIV quotient", machine->cpu.gpr[18], UINT64_C(0xfffffffffffffff2));
  check_u64("DDIV remainder", machine->cpu.gpr[19], UINT64_C(0xfffffffffffffffe));
  check_u64("DDIVU quotient", machine->cpu.gpr[22],
            UINT64_C(0x5555555555555555));
  check_u64("DDIVU remainder", machine->cpu.gpr[23], UINT64_C(0));
  destroy_machine(machine);
}

static void test_compat_division_undefined_cases(
    const test_environment *environment) {
  static const uint32_t program[] = {
      ENCODE_R(8u, 9u, 0u, 0u, 0x1bu), /* divu */
      ENCODE_R(0u, 0u, 10u, 0u, 0x12u), /* mflo */
      ENCODE_R(0u, 0u, 11u, 0u, 0x10u), /* mfhi */
      ENCODE_R(12u, 13u, 0u, 0u, 0x1au), /* div */
      ENCODE_R(0u, 0u, 14u, 0u, 0x12u), /* mflo */
      ENCODE_R(0u, 0u, 15u, 0u, 0x10u), /* mfhi */
      ENCODE_R(16u, 17u, 0u, 0u, 0x1fu), /* ddivu */
      ENCODE_R(0u, 0u, 18u, 0u, 0x12u), /* mflo */
      ENCODE_R(0u, 0u, 19u, 0u, 0x10u), /* mfhi */
      ENCODE_R(20u, 21u, 0u, 0u, 0x1eu), /* ddiv */
      ENCODE_R(0u, 0u, 22u, 0u, 0x12u), /* mflo */
      ENCODE_R(0u, 0u, 23u, 0u, 0x10u), /* mfhi */
      ENCODE_R(24u, 25u, 0u, 0u, 0x1eu), /* ddiv signed overflow */
      ENCODE_R(0u, 0u, 26u, 0u, 0x12u), /* mflo */
      ENCODE_R(0u, 0u, 27u, 0u, 0x10u), /* mfhi */
  };
  const size_t count = sizeof program / sizeof program[0];
  t64_machine *machine = prepare_compat_program(environment, TEST_PC, program,
                                               count);

  check_bool("compat division fixture admitted", machine != NULL);
  if (machine == NULL) {
    return;
  }
  machine->cpu.gpr[8] = UINT64_C(0x00000000fedcba98);
  machine->cpu.gpr[9] = 0u;
  machine->cpu.gpr[12] = UINT64_C(0xffffffff80000000);
  machine->cpu.gpr[13] = UINT64_MAX;
  machine->cpu.gpr[16] = UINT64_C(0x1122334455667788);
  machine->cpu.gpr[17] = 0u;
  machine->cpu.gpr[20] = UINT64_C(0xfffffffffffffff9);
  machine->cpu.gpr[21] = 0u;
  machine->cpu.gpr[24] = UINT64_C(0x8000000000000000);
  machine->cpu.gpr[25] = UINT64_MAX;
  check_bounded_tick(machine, count,
                     "compat word divide zero and signed overflow retire");
  check_u64("compat DIVU zero uses all-one quotient",
            machine->cpu.gpr[10], UINT64_MAX);
  check_u64("compat DIVU zero preserves dividend as remainder",
            machine->cpu.gpr[11], UINT64_C(0xfffffffffedcba98));
  check_u64("compat DIV overflow wraps quotient to signed minimum",
            machine->cpu.gpr[14], UINT64_C(0xffffffff80000000));
  check_u64("compat DIV overflow remainder is zero", machine->cpu.gpr[15],
            UINT64_C(0));
  check_u64("compat DDIVU zero uses all-one quotient",
            machine->cpu.gpr[18], UINT64_MAX);
  check_u64("compat DDIVU zero preserves dividend as remainder",
            machine->cpu.gpr[19], UINT64_C(0x1122334455667788));
  check_u64("compat negative DDIV zero uses positive-one quotient",
            machine->cpu.gpr[22], UINT64_C(1));
  check_u64("compat negative DDIV zero preserves dividend remainder",
            machine->cpu.gpr[23], UINT64_C(0xfffffffffffffff9));
  check_u64("compat DDIV overflow wraps quotient to signed minimum",
            machine->cpu.gpr[26], UINT64_C(0x8000000000000000));
  check_u64("compat DDIV overflow remainder is zero", machine->cpu.gpr[27],
            UINT64_C(0));
  destroy_machine(machine);
}

typedef struct {
  const char *name;
  uint32_t opcode;
  uint64_t true_left;
  uint64_t true_right;
  uint64_t false_left;
  uint64_t false_right;
} trap_vector;

static void run_trap_vector(const test_environment *environment,
                            const trap_vector *vector, bool trap_expected,
                            uint64_t left, uint64_t right,
                            const char *case_suffix) {
  const uint32_t program[] = {vector->opcode};
  t64_machine *machine = prepare_program(environment, TEST_PC, program, 1u);
  char label[128];

  if (machine == NULL) {
    check_bool("trap fixture admitted", false);
    return;
  }
  machine->cpu.gpr[8] = left;
  machine->cpu.gpr[9] = right;
  (void)snprintf(label, sizeof label, "%s %s observes expected trap result",
                 vector->name, case_suffix);
  check_bounded_tick(machine, 1u, label);
  if (trap_expected) {
    (void)snprintf(label, sizeof label, "%s %s enters Trap exception",
                   vector->name, case_suffix);
    check_exception(machine, UINT64_C(13), TEST_PC, UINT64_C(0), label);
  } else {
    (void)snprintf(label, sizeof label, "%s %s retires without Trap",
                   vector->name, case_suffix);
    check_u64(label, machine->cpu.retired_instructions, UINT64_C(1));
  }
  destroy_machine(machine);
}

static void test_trap_register_and_immediate_matrix(
    const test_environment *environment) {
  static const trap_vector vectors[] = {
      {"TGE", ENCODE_R(8u, 9u, 0u, 0u, 0x30u), 7u, 4u,
       UINT64_C(0xffffffffffffffff), 0u},
      {"TGEU", ENCODE_R(8u, 9u, 0u, 0u, 0x31u), 7u, 4u, 0u, 1u},
      {"TLT", ENCODE_R(8u, 9u, 0u, 0u, 0x32u),
       UINT64_C(0xffffffffffffffff), 0u, 1u, 0u},
      {"TLTU", ENCODE_R(8u, 9u, 0u, 0u, 0x33u), 0u, 1u, 2u, 1u},
      {"TEQ", ENCODE_R(8u, 9u, 0u, 0u, 0x34u), 5u, 5u, 5u, 4u},
      {"TNE", ENCODE_R(8u, 9u, 0u, 0u, 0x36u), 5u, 4u, 5u, 5u},
      {"TGEI", ENCODE_I(0x01u, 8u, 8u, 4), 7u, 0u, 3u, 0u},
      {"TGEIU", ENCODE_I(0x01u, 8u, 9u, -1), UINT64_MAX, 0u, 0u, 0u},
      {"TLTI", ENCODE_I(0x01u, 8u, 10u, 4), 3u, 0u, 7u, 0u},
      {"TLTIU", ENCODE_I(0x01u, 8u, 11u, 4), 3u, 0u, 7u, 0u},
      {"TEQI", ENCODE_I(0x01u, 8u, 12u, 4), 4u, 0u, 3u, 0u},
      {"TNEI", ENCODE_I(0x01u, 8u, 14u, 4), 3u, 0u, 4u, 0u},
  };
  size_t index;

  for (index = 0u; index < sizeof vectors / sizeof vectors[0]; index++) {
    run_trap_vector(environment, &vectors[index], true,
                    vectors[index].true_left, vectors[index].true_right,
                    "true control");
    run_trap_vector(environment, &vectors[index], false,
                    vectors[index].false_left, vectors[index].false_right,
                    "false control");
  }
}

static void test_divide_by_zero_and_signed_overflow_policy(
    const test_environment *environment) {
  static const uint32_t positive_zero[] = {ENCODE_R(8u, 9u, 0u, 0u, 0x1au)};
  static const uint32_t negative_zero[] = {ENCODE_R(8u, 9u, 0u, 0u, 0x1au)};
  static const uint32_t unsigned_zero[] = {ENCODE_R(8u, 9u, 0u, 0u, 0x1bu)};
  static const uint32_t double_zero[] = {ENCODE_R(8u, 9u, 0u, 0u, 0x1eu)};
  static const uint32_t double_unsigned_zero[] = {
      ENCODE_R(8u, 9u, 0u, 0u, 0x1fu)};
  static const uint32_t word_overflow[] = {ENCODE_R(8u, 9u, 0u, 0u, 0x1au)};
  static const uint32_t double_overflow[] = {
      ENCODE_R(8u, 9u, 0u, 0u, 0x1eu)};
  t64_machine *machine;

  machine = prepare_program(environment, TEST_PC, positive_zero, 1u);
  check_bool("positive DIV zero fixture admitted", machine != NULL);
  if (machine != NULL) {
    machine->cpu.gpr[8] = 5u;
    machine->cpu.gpr[9] = 0u;
    check_bounded_tick(machine, 1u, "DIV zero uses the VR4300 word result");
    check_u64("DIV zero positive quotient", machine->cpu.lo,
              UINT64_C(0x7fffffff));
    check_u64("DIV zero positive remainder", machine->cpu.hi, UINT64_C(5));
    destroy_machine(machine);
  }

  machine = prepare_program(environment, TEST_PC, negative_zero, 1u);
  check_bool("negative DIV zero fixture admitted", machine != NULL);
  if (machine != NULL) {
    machine->cpu.gpr[8] = UINT64_C(0xfffffffffffffffb);
    machine->cpu.gpr[9] = 0u;
    check_bounded_tick(machine, 1u, "negative DIV zero uses manual result");
    check_u64("DIV zero negative quotient", machine->cpu.lo,
              UINT64_C(0xffffffff80000001));
    check_u64("DIV zero negative remainder", machine->cpu.hi,
              UINT64_C(0xfffffffffffffffb));
    destroy_machine(machine);
  }

  machine = prepare_program(environment, TEST_PC, unsigned_zero, 1u);
  check_bool("DIVU zero fixture admitted", machine != NULL);
  if (machine != NULL) {
    machine->cpu.gpr[8] = UINT64_C(0xffffffff);
    machine->cpu.gpr[9] = 0u;
    machine->cpu.hi = UINT64_C(0x1122334455667788);
    machine->cpu.lo = UINT64_C(0x8877665544332211);
    check_unsupported(machine, 1u,
                      "DIVU zero is refused without a manual result");
    check_u64("DIVU zero preserves HI", machine->cpu.hi,
              UINT64_C(0x1122334455667788));
    check_u64("DIVU zero preserves LO", machine->cpu.lo,
              UINT64_C(0x8877665544332211));
    destroy_machine(machine);
  }

  machine = prepare_program(environment, TEST_PC, double_zero, 1u);
  check_bool("DDIV zero fixture admitted", machine != NULL);
  if (machine != NULL) {
    machine->cpu.gpr[8] = UINT64_C(9);
    machine->cpu.gpr[9] = UINT64_C(0);
    machine->cpu.hi = UINT64_C(0x1122334455667788);
    machine->cpu.lo = UINT64_C(0x8877665544332211);
    check_unsupported(machine, 1u, "DDIV zero is undefined and refused");
    check_u64("DDIV zero preserves HI", machine->cpu.hi,
              UINT64_C(0x1122334455667788));
    check_u64("DDIV zero preserves LO", machine->cpu.lo,
              UINT64_C(0x8877665544332211));
    check_u64("DDIV zero does not retire", machine->cpu.retired_instructions,
              UINT64_C(0));
    destroy_machine(machine);
  }

  machine = prepare_program(environment, TEST_PC, double_unsigned_zero, 1u);
  check_bool("DDIVU zero fixture admitted", machine != NULL);
  if (machine != NULL) {
    machine->cpu.gpr[8] = UINT64_MAX;
    machine->cpu.gpr[9] = UINT64_C(0);
    machine->cpu.hi = UINT64_C(0x1122334455667788);
    machine->cpu.lo = UINT64_C(0x8877665544332211);
    check_unsupported(machine, 1u, "DDIVU zero is undefined and refused");
    check_u64("DDIVU zero preserves HI", machine->cpu.hi,
              UINT64_C(0x1122334455667788));
    check_u64("DDIVU zero preserves LO", machine->cpu.lo,
              UINT64_C(0x8877665544332211));
    destroy_machine(machine);
  }

  machine = prepare_program(environment, TEST_PC, word_overflow, 1u);
  check_bool("word divide overflow fixture admitted", machine != NULL);
  if (machine != NULL) {
    machine->cpu.gpr[8] = UINT64_C(0xffffffff80000000);
    machine->cpu.gpr[9] = UINT64_MAX;
    machine->cpu.hi = UINT64_C(0x1122334455667788);
    machine->cpu.lo = UINT64_C(0x8877665544332211);
    check_unsupported(machine, 1u,
                     "DIV signed quotient overflow is refused before mutation");
    check_u64("DIV overflow preserves HI", machine->cpu.hi,
              UINT64_C(0x1122334455667788));
    check_u64("DIV overflow preserves LO", machine->cpu.lo,
              UINT64_C(0x8877665544332211));
    destroy_machine(machine);
  }

  machine = prepare_program(environment, TEST_PC, double_overflow, 1u);
  check_bool("double divide overflow fixture admitted", machine != NULL);
  if (machine != NULL) {
    machine->cpu.gpr[8] = UINT64_C(0x8000000000000000);
    machine->cpu.gpr[9] = UINT64_MAX;
    machine->cpu.hi = UINT64_C(0x1122334455667788);
    machine->cpu.lo = UINT64_C(0x8877665544332211);
    check_unsupported(machine, 1u,
                     "DDIV signed quotient overflow is refused before mutation");
    check_u64("DDIV overflow preserves HI", machine->cpu.hi,
              UINT64_C(0x1122334455667788));
    check_u64("DDIV overflow preserves LO", machine->cpu.lo,
              UINT64_C(0x8877665544332211));
    destroy_machine(machine);
  }
}

static void test_arithmetic_overflow_exceptions(
    const test_environment *environment) {
  static const struct {
    const char *name;
    uint32_t opcode;
    uint64_t left;
    uint64_t right;
  } vectors[] = {
      {"ADD", ENCODE_R(8u, 9u, 10u, 0u, 0x20u),
       UINT64_C(0x7fffffff), 1u},
      {"SUB", ENCODE_R(8u, 9u, 10u, 0u, 0x22u),
       UINT64_C(0x80000000), 1u},
      {"ADDI", ENCODE_I(0x08u, 8u, 10u, 1),
       UINT64_C(0x7fffffff), 0u},
      {"DADD", ENCODE_R(8u, 9u, 10u, 0u, 0x2cu),
       UINT64_C(0x7fffffffffffffff), 1u},
      {"DSUB", ENCODE_R(8u, 9u, 10u, 0u, 0x2eu),
       UINT64_C(0x8000000000000000), 1u},
      {"DADDI", ENCODE_I(0x18u, 8u, 10u, 1),
       UINT64_C(0x7fffffffffffffff), 0u},
  };
  size_t index;

  for (index = 0u; index < sizeof vectors / sizeof vectors[0]; index++) {
    uint32_t program[] = {vectors[index].opcode};
    t64_machine *machine =
        prepare_program(environment, TEST_PC, program, 1u);
    uint64_t destination_before = UINT64_C(0x123456789abcdef0);

    check_bool("overflow fixture admitted", machine != NULL);
    if (machine == NULL) {
      continue;
    }
    machine->cpu.gpr[8] = vectors[index].left;
    machine->cpu.gpr[9] = vectors[index].right;
    machine->cpu.gpr[10] = destination_before;
    check_bounded_tick(machine, 1u, vectors[index].name);
    check_exception(machine, UINT64_C(12), TEST_PC, UINT64_C(0),
                    vectors[index].name);
    check_u64("overflow preserves destination", machine->cpu.gpr[10],
              destination_before);
    destroy_machine(machine);
  }
}

typedef enum {
  BRANCH_EQUAL,
  BRANCH_NOT_EQUAL,
  BRANCH_LE_ZERO,
  BRANCH_GT_ZERO,
  BRANCH_LT_ZERO,
  BRANCH_GE_ZERO
} branch_condition_kind;

typedef struct {
  const char *name;
  uint32_t opcode;
  branch_condition_kind condition;
  bool likely;
  bool link;
} branch_vector;

static void set_branch_operands(t64_machine *machine,
                                branch_condition_kind condition,
                                bool taken) {
  machine->cpu.gpr[8] = UINT64_C(0);
  machine->cpu.gpr[9] = UINT64_C(0);
  switch (condition) {
  case BRANCH_EQUAL:
    machine->cpu.gpr[8] = UINT64_C(7);
    machine->cpu.gpr[9] = taken ? UINT64_C(7) : UINT64_C(8);
    break;
  case BRANCH_NOT_EQUAL:
    machine->cpu.gpr[8] = UINT64_C(7);
    machine->cpu.gpr[9] = taken ? UINT64_C(8) : UINT64_C(7);
    break;
  case BRANCH_LE_ZERO:
    machine->cpu.gpr[8] = taken ? UINT64_C(0) : UINT64_C(1);
    break;
  case BRANCH_GT_ZERO:
    machine->cpu.gpr[8] = taken ? UINT64_C(1) : UINT64_C(0);
    break;
  case BRANCH_LT_ZERO:
    machine->cpu.gpr[8] = taken ? UINT64_MAX : UINT64_C(0);
    break;
  case BRANCH_GE_ZERO:
    machine->cpu.gpr[8] = taken ? UINT64_C(0) : UINT64_MAX;
    break;
  }
}

static void run_branch_vector(const test_environment *environment,
                              const branch_vector *vector, bool taken) {
  uint32_t program[] = {
      vector->opcode,
      ENCODE_I(0x09u, 0u, 11u, 0x11), /* delay slot */
      ENCODE_I(0x09u, 0u, 12u, 0x22), /* fallthrough */
      ENCODE_I(0x09u, 0u, 13u, 0x33), /* taken target */
      UINT32_C(0x7000003f),
  };
  t64_machine *machine = prepare_program(environment, TEST_PC, program,
                                          sizeof program / sizeof program[0]);
  uint64_t elapsed;
  uint64_t expected_link;
  char label[128];

  if (machine == NULL) {
    check_bool("branch fixture admitted", false);
    return;
  }
  set_branch_operands(machine, vector->condition, taken);
  machine->cpu.gpr[31] = UINT64_C(0xfeedfacecafebeef);
  (void)snprintf(label, sizeof label, "%s %s branch attempt", vector->name,
                 taken ? "taken" : "not-taken");
  check_bounded_tick(machine, 1u, label);
  expected_link = vector->link ? TEST_PC + UINT64_C(8)
                               : UINT64_C(0xfeedfacecafebeef);
  check_u64("conditional link written only when condition is true",
            machine->cpu.gpr[31], expected_link);
  check_u64("branch delay-state follows likely outcome",
            machine->cpu.delay_slot_pending ? UINT64_C(1) : UINT64_C(0),
            (!vector->likely || taken) ? UINT64_C(1) : UINT64_C(0));
  elapsed = taken || !vector->likely ? UINT64_C(3) : UINT64_C(2);
  (void)snprintf(label, sizeof label, "%s %s reaches the expected next PC",
                 vector->name, taken ? "taken" : "not-taken");
  check_bounded_tick(machine, elapsed, label);
  check_u64("branch slot runs unless annulled", machine->cpu.gpr[11],
            (taken || !vector->likely) ? UINT64_C(0x11) : UINT64_C(0));
  check_u64("not-taken branch reaches fallthrough", machine->cpu.gpr[12],
            taken ? UINT64_C(0) : UINT64_C(0x22));
  check_u64("taken branch reaches target after slot", machine->cpu.gpr[13],
            taken ? UINT64_C(0x33) : UINT64_C(0));
  destroy_machine(machine);
}

static void test_conditional_branch_outcomes(const test_environment *environment) {
  static const branch_vector vectors[] = {
      {"BEQ", ENCODE_I(0x04u, 8u, 9u, 2), BRANCH_EQUAL, false, false},
      {"BNE", ENCODE_I(0x05u, 8u, 9u, 2), BRANCH_NOT_EQUAL, false, false},
      {"BLEZ", ENCODE_I(0x06u, 8u, 0u, 2), BRANCH_LE_ZERO, false, false},
      {"BGTZ", ENCODE_I(0x07u, 8u, 0u, 2), BRANCH_GT_ZERO, false, false},
      {"BEQL", ENCODE_I(0x14u, 8u, 9u, 2), BRANCH_EQUAL, true, false},
      {"BNEL", ENCODE_I(0x15u, 8u, 9u, 2), BRANCH_NOT_EQUAL, true, false},
      {"BLEZL", ENCODE_I(0x16u, 8u, 0u, 2), BRANCH_LE_ZERO, true, false},
      {"BGTZL", ENCODE_I(0x17u, 8u, 0u, 2), BRANCH_GT_ZERO, true, false},
      {"BLTZ", ENCODE_I(0x01u, 8u, 0u, 2), BRANCH_LT_ZERO, false, false},
      {"BGEZ", ENCODE_I(0x01u, 8u, 1u, 2), BRANCH_GE_ZERO, false, false},
      {"BLTZL", ENCODE_I(0x01u, 8u, 2u, 2), BRANCH_LT_ZERO, true, false},
      {"BGEZL", ENCODE_I(0x01u, 8u, 3u, 2), BRANCH_GE_ZERO, true, false},
      {"BLTZAL", ENCODE_I(0x01u, 8u, 16u, 2), BRANCH_LT_ZERO, false, true},
      {"BGEZAL", ENCODE_I(0x01u, 8u, 17u, 2), BRANCH_GE_ZERO, false, true},
      {"BLTZALL", ENCODE_I(0x01u, 8u, 18u, 2), BRANCH_LT_ZERO, true, true},
      {"BGEZALL", ENCODE_I(0x01u, 8u, 19u, 2), BRANCH_GE_ZERO, true, true},
  };
  size_t index;

  for (index = 0u; index < sizeof vectors / sizeof vectors[0]; index++) {
    run_branch_vector(environment, &vectors[index], true);
    run_branch_vector(environment, &vectors[index], false);
  }
}

static void test_jump_targets_and_link_alias(const test_environment *environment) {
  static const uint32_t jump_program[] = {
      ENCODE_J(0x02u, UINT32_C(0x00000403)), /* j 0xffffffff8000100c */
      ENCODE_I(0x09u, 0u, 11u, 7),
      UINT32_C(0x7000003f),
      ENCODE_I(0x09u, 0u, 12u, 9),
  };
  static const uint32_t jr_program[] = {
      ENCODE_R(8u, 0u, 0u, 0u, 0x08u), /* jr t0 */
      ENCODE_I(0x09u, 0u, 11u, 7),
      UINT32_C(0x7000003f),
      ENCODE_I(0x09u, 0u, 12u, 9),
  };
  static const uint32_t jal_program[] = {
      ENCODE_J(0x03u, UINT32_C(0x00000403)), /* jal 0xffffffff8000100c */
      ENCODE_I(0x09u, 0u, 11u, 7),
      ENCODE_I(0x09u, 0u, 12u, 9),
      UINT32_C(0x7000003f),
  };
  static const uint32_t jalr_program[] = {
      ENCODE_R(8u, 0u, 8u, 0u, 0x09u), /* jalr t0,t0: source aliases link */
      ENCODE_I(0x09u, 0u, 11u, 7),
      UINT32_C(0x7000003f),
  };
  t64_machine *machine =
      prepare_program(environment, TEST_PC, jump_program,
                      sizeof jump_program / sizeof jump_program[0]);

  check_bool("J fixture admitted", machine != NULL);
  if (machine == NULL) {
    return;
  }
  machine->cpu.gpr[31] = UINT64_C(0xfeedfacecafebeef);
  check_bounded_tick(machine, 1u, "J target and delay slot stage");
  check_u64("J full-width target", machine->cpu.delay_target_full,
            UINT64_C(0xffffffff8000100c));
  check_bounded_tick(machine, 3u, "J executes slot then target");
  check_u64("J delay-slot side effect", machine->cpu.gpr[11], UINT64_C(7));
  check_u64("J target side effect", machine->cpu.gpr[12], UINT64_C(9));
  check_u64("J does not write the link register", machine->cpu.gpr[31],
            UINT64_C(0xfeedfacecafebeef));
  destroy_machine(machine);

  machine = prepare_program(environment, TEST_PC, jr_program,
                            sizeof jr_program / sizeof jr_program[0]);
  check_bool("JR fixture admitted", machine != NULL);
  if (machine == NULL) {
    return;
  }
  machine->cpu.gpr[8] = TEST_PC + UINT64_C(12);
  machine->cpu.gpr[31] = UINT64_C(0xfeedfacecafebeef);
  check_bounded_tick(machine, 1u, "JR captures its target");
  check_u64("JR full-width target", machine->cpu.delay_target_full,
            TEST_PC + UINT64_C(12));
  check_bounded_tick(machine, 3u, "JR executes slot then target");
  check_u64("JR delay-slot side effect", machine->cpu.gpr[11], UINT64_C(7));
  check_u64("JR target side effect", machine->cpu.gpr[12], UINT64_C(9));
  check_u64("JR does not write the link register", machine->cpu.gpr[31],
            UINT64_C(0xfeedfacecafebeef));
  destroy_machine(machine);

  machine =
      prepare_program(environment, TEST_PC, jal_program,
                      sizeof jal_program / sizeof jal_program[0]);

  check_bool("JAL fixture admitted", machine != NULL);
  if (machine == NULL) {
    return;
  }
  check_bounded_tick(machine, 1u, "JAL target and link commit");
  check_u64("JAL full-width target", machine->cpu.delay_target_full,
            UINT64_C(0xffffffff8000100c));
  check_u64("JAL full-width PC+8 link", machine->cpu.gpr[31],
            TEST_PC + UINT64_C(8));
  destroy_machine(machine);

  machine = prepare_program(environment, TEST_PC, jalr_program,
                           sizeof jalr_program / sizeof jalr_program[0]);
  check_bool("JALR fixture admitted", machine != NULL);
  if (machine == NULL) {
    return;
  }
  machine->cpu.gpr[8] = UINT64_C(0x0000000100002000);
  check_bounded_tick(machine, 1u, "JALR captures source before link write");
  check_u64("JALR preserves full-width source target",
            machine->cpu.delay_target_full, UINT64_C(0x0000000100002000));
  check_u64("JALR source/destination alias writes PC+8",
            machine->cpu.gpr[8], TEST_PC + UINT64_C(8));
  check_bounded_tick(machine, 2u, "JALR delay slot runs before full target");
  check_u64("JR target survives delay-slot retirement", machine->cpu.pc_full,
            UINT64_C(0x0000000100002000));
  check_u64("JALR delay-slot side effect", machine->cpu.gpr[11],
            UINT64_C(7));
  destroy_machine(machine);
}

static void test_negative_branch_target_and_branch_in_slot(
    const test_environment *environment) {
  static const uint32_t negative_program[] = {
      ENCODE_I(0x04u, 0u, 0u, -2),
      ENCODE_I(0x09u, 0u, 8u, 1),
  };
  static const uint32_t nested_branch_program[] = {
      ENCODE_I(0x04u, 0u, 0u, 1),
      ENCODE_R(8u, 0u, 9u, 0u, 0x09u), /* undefined JALR in delay slot */
  };
  t64_machine *machine =
      prepare_program(environment, TEST_PC, negative_program,
                      sizeof negative_program / sizeof negative_program[0]);

  check_bool("negative branch fixture admitted", machine != NULL);
  if (machine == NULL) {
    return;
  }
  check_bounded_tick(machine, 1u, "negative offset branch executes");
  check_u64("negative branch offset sign extends in 64-bit PC",
            machine->cpu.delay_target_full, TEST_PC - UINT64_C(4));
  destroy_machine(machine);

  machine = prepare_program(environment, TEST_PC, nested_branch_program,
                           sizeof nested_branch_program /
                               sizeof nested_branch_program[0]);
  check_bool("branch-in-slot fixture admitted", machine != NULL);
  if (machine == NULL) {
    return;
  }
  machine->cpu.gpr[8] = UINT64_C(0xffffffff80002000);
  machine->cpu.gpr[9] = UINT64_C(0xfeedfacecafebeef);
  check_bounded_tick(machine, 1u, "outer branch retires");
  check_unsupported(machine, 2u,
                    "branch in delay slot stops as model-unsupported");
  check_u64("undefined delay branch does not write link register",
            machine->cpu.gpr[9], UINT64_C(0xfeedfacecafebeef));
  check_u64("undefined delay branch does not retire",
            machine->cpu.retired_instructions, UINT64_C(1));
  destroy_machine(machine);
}

static void test_compat_branch_in_delay_slot_has_one_nested_slot(
    const test_environment *environment) {
  static const uint32_t program[] = {
      ENCODE_I(0x04u, 0u, 0u, 3),
      ENCODE_R(8u, 0u, 9u, 0u, 0x09u),
      ENCODE_I(0x09u, 0u, 10u, 0x9999),
      UINT32_C(0),
      ENCODE_I(0x09u, 0u, 10u, 0x1234),
  };
  static const uint32_t target_instruction = ENCODE_I(0x09u, 0u, 11u, 0x5678);
  const uint64_t target = UINT64_C(0xffffffff80002000);
  t64_machine *machine = prepare_compat_program(
      environment, TEST_PC, program, sizeof program / sizeof program[0]);
  t64_status status;

  check_bool("compat branch-in-delay fixture admitted", machine != NULL);
  if (machine == NULL) {
    return;
  }
  write_rdram_instruction(machine, UINT32_C(0x2000), target_instruction);
  machine->cpu.gpr[8] = target;

  status = t64_cpu_step(machine, NULL);
  check_bool("compat outer branch schedules its delay slot",
             status == T64_STATUS_OK && machine->cpu.delay_slot_pending &&
                 machine->cpu.pc_full == TEST_PC + UINT64_C(4));
  status = t64_cpu_step(machine, NULL);
  check_bool("compat branch in delay slot retires and retargets",
             status == T64_STATUS_OK &&
                 machine->cpu.attempt_outcome == T64_CPU_ATTEMPT_RETIRED &&
                 machine->cpu.delay_slot_pending &&
                 machine->cpu.pc_full == TEST_PC + UINT64_C(16) &&
                 machine->cpu.delay_target_full == target &&
                 machine->cpu.delay_origin_pc_full == TEST_PC + UINT64_C(4));
  check_u64("compat delayed JALR writes PC+8 link", machine->cpu.gpr[9],
            TEST_PC + UINT64_C(12));
  status = t64_cpu_step(machine, NULL);
  check_bool("outer target instruction is the inner branch delay slot",
             status == T64_STATUS_OK && !machine->cpu.delay_slot_pending &&
                 machine->cpu.pc_full == target &&
                 machine->cpu.gpr[10] == UINT64_C(0x1234));
  status = t64_cpu_step(machine, NULL);
  check_bool("compat inner branch target executes",
             status == T64_STATUS_OK &&
                 machine->cpu.gpr[11] == UINT64_C(0x5678));
  destroy_machine(machine);
}

static void test_compat_nested_likely_branch_taken_and_annulled(
    const test_environment *environment) {
  static const uint32_t taken_program[] = {
      ENCODE_I(0x04u, 0u, 0u, 3),
      ENCODE_I(0x14u, 8u, 9u, 3), /* beql t0,t1,inner-target */
      ENCODE_I(0x09u, 0u, 12u, 0x1111),
      ENCODE_I(0x09u, 0u, 13u, 0x2222),
      ENCODE_I(0x09u, 0u, 10u, 0x3333), /* outer target: inner delay slot */
      ENCODE_I(0x09u, 0u, 11u, 0x4444), /* inner target */
  };
  static const uint32_t not_taken_program[] = {
      ENCODE_I(0x04u, 0u, 0u, 3),
      ENCODE_I(0x14u, 8u, 9u, 3), /* beql t0,t1,inner-target */
      ENCODE_I(0x09u, 0u, 12u, 0x1111),
      ENCODE_I(0x09u, 0u, 13u, 0x2222), /* inner PC+8 */
      ENCODE_I(0x09u, 0u, 10u, 0x3333), /* displaced outer target */
      ENCODE_I(0x09u, 0u, 11u, 0x4444),
  };
  const uint64_t outer_target = TEST_PC + UINT64_C(16);
  const uint64_t inner_target = TEST_PC + UINT64_C(20);
  t64_machine *machine = prepare_compat_program(
      environment, TEST_PC, taken_program,
      sizeof taken_program / sizeof taken_program[0]);
  t64_status status;

  check_bool("compat nested taken-likely fixture admitted", machine != NULL);
  if (machine == NULL) {
    return;
  }
  machine->cpu.gpr[8] = UINT64_C(5);
  machine->cpu.gpr[9] = UINT64_C(5);
  status = t64_cpu_step(machine, NULL);
  check_bool("taken-likely outer branch schedules inner branch",
             status == T64_STATUS_OK && machine->cpu.delay_slot_pending);
  status = t64_cpu_step(machine, NULL);
  check_bool("taken likely branch uses outer target as its delay slot",
             status == T64_STATUS_OK && machine->cpu.delay_slot_pending &&
                 machine->cpu.pc_full == outer_target &&
                 machine->cpu.delay_target_full == inner_target &&
                 machine->cpu.delay_origin_pc_full == TEST_PC + UINT64_C(4));
  status = t64_cpu_step(machine, NULL);
  check_bool("taken likely branch retires the displaced slot then targets",
             status == T64_STATUS_OK && !machine->cpu.delay_slot_pending &&
                 machine->cpu.pc_full == inner_target &&
                 machine->cpu.gpr[10] == UINT64_C(0x3333));
  status = t64_cpu_step(machine, NULL);
  check_bool("taken likely branch reaches its inner target",
             status == T64_STATUS_OK &&
                 machine->cpu.gpr[11] == UINT64_C(0x4444));
  destroy_machine(machine);

  machine = prepare_compat_program(
      environment, TEST_PC, not_taken_program,
      sizeof not_taken_program / sizeof not_taken_program[0]);
  check_bool("compat nested not-taken-likely fixture admitted",
             machine != NULL);
  if (machine == NULL) {
    return;
  }
  machine->cpu.gpr[8] = UINT64_C(5);
  machine->cpu.gpr[9] = UINT64_C(6);
  status = t64_cpu_step(machine, NULL);
  check_bool("not-taken-likely outer branch schedules inner branch",
             status == T64_STATUS_OK && machine->cpu.delay_slot_pending);
  status = t64_cpu_step(machine, NULL);
  check_bool("not-taken likely branch annuls displaced slot",
             status == T64_STATUS_OK && !machine->cpu.delay_slot_pending &&
                 machine->cpu.pc_full == TEST_PC + UINT64_C(12) &&
                 machine->cpu.delay_target_full == 0u &&
                 machine->cpu.delay_origin_pc_full == 0u);
  check_u64("not-taken likely branch has no annulled sequential-slot effect",
            machine->cpu.gpr[12], UINT64_C(0));
  check_u64("not-taken likely branch retires only the two branches",
            machine->cpu.retired_instructions, UINT64_C(2));
  status = t64_cpu_step(machine, NULL);
  check_bool("not-taken likely branch resumes at inner PC+8",
             status == T64_STATUS_OK &&
                 machine->cpu.pc_full == outer_target &&
                 machine->cpu.gpr[13] == UINT64_C(0x2222) &&
                 machine->cpu.gpr[10] == 0u);
  destroy_machine(machine);
}

static void test_compat_displaced_slot_fault_uses_inner_branch_origin(
    const test_environment *environment) {
  static const uint32_t program[] = {
      ENCODE_I(0x04u, 0u, 0u, 3),
      ENCODE_R(8u, 0u, 9u, 0u, 0x09u),
      UINT32_C(0),
      UINT32_C(0),
      ENCODE_I(0x23u, 4u, 8u, 0), /* lw t0,0(a0): fault in displaced slot */
  };
  const uint64_t target = UINT64_C(0xffffffff80002000);
  const uint64_t misaligned = UINT64_C(0xffffffff80003001);
  t64_machine *machine = prepare_compat_program(
      environment, TEST_PC, program, sizeof program / sizeof program[0]);
  t64_status status;
  uint32_t cause;

  check_bool("compat displaced-slot fault fixture admitted", machine != NULL);
  if (machine == NULL) {
    return;
  }
  machine->cpu.gpr[8] = target;
  machine->cpu.gpr[4] = misaligned;
  status = t64_cpu_step(machine, NULL);
  check_bool("compat outer branch schedules its delay instruction",
             status == T64_STATUS_OK && machine->cpu.delay_slot_pending);
  status = t64_cpu_step(machine, NULL);
  check_bool("compat inner branch makes outer target its delay instruction",
             status == T64_STATUS_OK && machine->cpu.delay_slot_pending &&
                 machine->cpu.pc_full == TEST_PC + UINT64_C(16));
  status = t64_cpu_step(machine, NULL);
  cause = (uint32_t)machine->cpu.cp0[13];
  check_bool("displaced delay-slot fault enters guest exception",
             status == T64_STATUS_OK &&
                 machine->cpu.attempt_outcome ==
                     T64_CPU_ATTEMPT_GUEST_EXCEPTION);
  check_u64("displaced delay-slot fault EPC names inner branch",
            machine->cpu.cp0[14], TEST_PC + UINT64_C(4));
  check_u64("displaced delay-slot fault preserves BD and AdEL",
            cause & UINT32_C(0x8000007c), UINT32_C(0x80000010));
  check_u64("displaced delay-slot fault records bad virtual address",
            machine->cpu.cp0[8], misaligned);
  check_u64("exception entry clears compat branch origin",
            machine->cpu.delay_origin_pc_full, UINT64_C(0));
  destroy_machine(machine);
}

static void test_reserved_integer_encodings_raise_ri(
    const test_environment *environment) {
  static const struct {
    const char *name;
    uint32_t opcode;
  } vectors[] = {
      {"SLL with nonzero rs", ENCODE_R(1u, 2u, 3u, 1u, 0x00u)},
      {"JR with nonzero rd", ENCODE_R(8u, 0u, 9u, 0u, 0x08u)},
      {"MULT with nonzero rd", ENCODE_R(8u, 9u, 1u, 0u, 0x18u)},
      {"SYNC with nonzero stype", ENCODE_R(0u, 0u, 0u, 1u, 0x0fu)},
      {"LUI with nonzero rs", ENCODE_I(0x0fu, 1u, 8u, 0x1234)},
      {"BLEZ with nonzero rt", ENCODE_I(0x06u, 8u, 1u, 2)},
      {"REGIMM reserved suboperation", ENCODE_I(0x01u, 8u, 4u, 2)},
  };
  size_t index;

  for (index = 0u; index < sizeof vectors / sizeof vectors[0]; index++) {
    uint32_t program[] = {vectors[index].opcode};
    t64_machine *machine =
        prepare_program(environment, TEST_PC, program, 1u);
    char label[128];

    check_bool("reserved encoding fixture admitted", machine != NULL);
    if (machine == NULL) {
      continue;
    }
    machine->cpu.gpr[3] = UINT64_C(0x123456789abcdef0);
    machine->cpu.gpr[8] = UINT64_C(0xffffffff80002000);
    machine->cpu.hi = UINT64_C(0x1111222233334444);
    machine->cpu.lo = UINT64_C(0x5555666677778888);
    check_bounded_tick(machine, 1u, vectors[index].name);
    (void)snprintf(label, sizeof label, "%s raises Reserved Instruction",
                   vectors[index].name);
    check_exception(machine, UINT64_C(10), TEST_PC, UINT64_C(0), label);
    check_u64("reserved encoding preserves HI", machine->cpu.hi,
              UINT64_C(0x1111222233334444));
    check_u64("reserved encoding preserves LO", machine->cpu.lo,
              UINT64_C(0x5555666677778888));
    check_u64("reserved encoding preserves GPR destination",
              machine->cpu.gpr[3], UINT64_C(0x123456789abcdef0));
    destroy_machine(machine);
  }
}

static void check_compat_v2_exception(
    const test_environment *environment, uint32_t opcode, uint64_t status_bits,
    uint64_t exception_code, uint64_t expected_coprocessor_error,
    const char *label) {
  uint32_t program[] = {opcode};
  t64_machine *machine = prepare_compat_v2_program(
      environment, TEST_PC, program, sizeof program / sizeof program[0]);

  check_bool("compat-v2 exception fixture admitted", machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] &= ~(TEST_STATUS_CU2 | TEST_STATUS_CU3);
  machine->cpu.cp0[12] |= status_bits;
  machine->cpu.gpr[8] = UINT64_C(0x123456789abcdef0);
  machine->cpu.gpr[9] = UINT64_C(0x0badc0de76543210);
  machine->cpu.cp2_transfer_latch = UINT64_C(0xaabbccdd11223344);
  check_bounded_tick(machine, UINT64_C(1), label);
  check_exception(machine, exception_code, TEST_PC, UINT64_C(0), label);
  check_u64("compat-v2 exception CE", machine->cpu.cp0[13] &
                                            UINT64_C(0x30000000),
            expected_coprocessor_error);
  check_u64("compat-v2 exception preserves source/destination GPR",
            machine->cpu.gpr[9], UINT64_C(0x0badc0de76543210));
  check_u64("compat-v2 exception preserves transfer latch",
            machine->cpu.cp2_transfer_latch,
            UINT64_C(0xaabbccdd11223344));
  destroy_machine(machine);
}

static void test_compat_v2_coprocessor_transfer_policy(
    const test_environment *environment) {
  static const uint32_t transfer_program[] = {
      ENCODE_COP_TRANSFER(0x12u, 4u, 8u, 5u),  /* MTC2 */
      ENCODE_COP_TRANSFER(0x12u, 0u, 9u, 6u),  /* MFC2 */
      ENCODE_COP_TRANSFER(0x12u, 1u, 10u, 31u), /* DMFC2 */
      ENCODE_COP_TRANSFER(0x12u, 5u, 11u, 5u), /* DMTC2 */
      ENCODE_COP_TRANSFER(0x12u, 0u, 12u, 30u), /* MFC2 */
      ENCODE_COP_TRANSFER(0x12u, 1u, 13u, 0u), /* DMFC2 */
      ENCODE_COP_TRANSFER(0x12u, 2u, 14u, 6u), /* CFC2 */
      ENCODE_COP_TRANSFER(0x12u, 6u, 15u, 6u), /* CTC2 */
      ENCODE_COP_TRANSFER(0x12u, 0u, 16u, 30u), /* MFC2 */
  };
  static const uint32_t reserved_cop2_formats[] = {3u, 7u};
  static const uint32_t reserved_primary_opcodes[] = {0x33u, 0x3bu};
  static const char *baseline_profiles[] = {
      "ntsc-cart-4m-openipl3-vr4300-v1",
      "ntsc-cart-4m-openipl3-vr4300-compat-v1",
  };
  t64_machine *machine = prepare_compat_v2_program(
      environment, TEST_PC, transfer_program,
      sizeof transfer_program / sizeof transfer_program[0]);
  size_t index;

  check_bool("compat-v2 transfer fixture admitted", machine != NULL);
  if (machine == NULL) return;
  machine->cpu.cp0[12] |= TEST_STATUS_CU2;
  machine->cpu.gpr[8] = UINT64_C(0x0123456789abcdef);
  machine->cpu.gpr[11] = UINT64_C(0xdeadbeef11223344);
  machine->cpu.gpr[14] = UINT64_C(0xaaaa5555aaaa5555);
  machine->cpu.gpr[15] = UINT64_C(0x5555aaaa5555aaaa);

  check_bounded_tick(machine, UINT64_C(1), "compat-v2 MTC2 retires");
  check_bool("compat-v2 first MTC2 retires",
             machine->cpu.attempt_outcome == T64_CPU_ATTEMPT_RETIRED);
  if (!check_fetched_instruction(machine, TEST_PC, transfer_program[0],
                                 "compat-v2 first MTC2") ||
      machine->cpu.attempt_outcome != T64_CPU_ATTEMPT_RETIRED) {
    destroy_machine(machine);
  } else {
    check_u64("compat-v2 MTC2 writes full transfer latch",
              machine->cpu.cp2_transfer_latch,
              UINT64_C(0x0123456789abcdef));
    check_bounded_tick(machine, UINT64_C(2), "compat-v2 MFC2 retires");
    check_u64("compat-v2 MFC2 sign-extends the low word",
              machine->cpu.gpr[9], UINT64_C(0xffffffff89abcdef));
    check_bounded_tick(machine, UINT64_C(3), "compat-v2 DMFC2 retires");
    check_u64("compat-v2 DMFC2 reads full latch through aliased rd",
              machine->cpu.gpr[10], UINT64_C(0x0123456789abcdef));
    check_bounded_tick(machine, UINT64_C(4), "compat-v2 DMTC2 retires");
    check_u64("compat-v2 DMTC2 writes full transfer latch",
              machine->cpu.cp2_transfer_latch,
              UINT64_C(0xdeadbeef11223344));
    check_bounded_tick(machine, UINT64_C(5), "compat-v2 MFC2 alias retires");
    check_u64("compat-v2 MFC2 ignores rd selector",
              machine->cpu.gpr[12], UINT64_C(0x0000000011223344));
    check_bounded_tick(machine, UINT64_C(6), "compat-v2 DMFC2 alias retires");
    check_u64("compat-v2 DMFC2 ignores rd selector",
              machine->cpu.gpr[13], UINT64_C(0xdeadbeef11223344));
    check_bounded_tick(machine, UINT64_C(7), "compat-v2 CFC2 retires");
    check_u64("compat-v2 CFC2 is inert", machine->cpu.gpr[14],
              UINT64_C(0xaaaa5555aaaa5555));
    check_bounded_tick(machine, UINT64_C(8), "compat-v2 CTC2 retires");
    check_u64("compat-v2 CTC2 leaves transfer latch unchanged",
              machine->cpu.cp2_transfer_latch,
              UINT64_C(0xdeadbeef11223344));
    check_bounded_tick(machine, UINT64_C(9), "compat-v2 final MFC2 retires");
    check_u64("compat-v2 latch survives inert controls",
              machine->cpu.gpr[16], UINT64_C(0x0000000011223344));
    destroy_machine(machine);
  }

  check_compat_v2_exception(
      environment, ENCODE_COP_TRANSFER(0x12u, 0u, 9u, 6u), 0u,
      UINT64_C(11), UINT64_C(0x20000000),
      "compat-v2 disabled COP2 transfer raises CopUnusable");
  for (index = 0u;
       index < sizeof reserved_cop2_formats / sizeof reserved_cop2_formats[0];
       index++) {
    uint32_t format = reserved_cop2_formats[index];

    check_compat_v2_exception(
        environment, ENCODE_COP_TRANSFER(0x12u, format, 9u, 6u),
        TEST_STATUS_CU2, UINT64_C(10), UINT64_C(0x20000000),
        "compat-v2 reserved COP2 form raises CE=2 RI");
    check_compat_v2_exception(
        environment, ENCODE_COP_TRANSFER(0x12u, format, 9u, 6u), 0u,
        UINT64_C(11), UINT64_C(0x20000000),
        "compat-v2 disabled reserved COP2 form raises CE=2 CopUnusable");
  }
  check_compat_v2_exception(
      environment, ENCODE_COP_TRANSFER(0x13u, 0u, 9u, 6u), 0u,
      UINT64_C(10), UINT64_C(0),
      "compat-v2 COP3 instruction raises RI with CU3 clear");
  check_compat_v2_exception(
      environment, ENCODE_COP_TRANSFER(0x13u, 0u, 9u, 6u), TEST_STATUS_CU3,
      UINT64_C(10), UINT64_C(0),
      "compat-v2 COP3 instruction raises RI with CU3 set");
  for (index = 0u;
       index < sizeof reserved_primary_opcodes /
                   sizeof reserved_primary_opcodes[0];
       index++) {
    uint32_t opcode = reserved_primary_opcodes[index] << 26;

    check_compat_v2_exception(
        environment, opcode, 0u, UINT64_C(10), UINT64_C(0),
        "compat-v2 reserved primary opcode raises RI");
  }

  for (index = 0u;
       index < sizeof baseline_profiles / sizeof baseline_profiles[0];
       index++) {
    static const uint32_t baseline_opcodes[] = {
        ENCODE_COP_TRANSFER(0x12u, 0u, 9u, 6u),
        ENCODE_COP_TRANSFER(0x13u, 0u, 9u, 6u),
        UINT32_C(0x33) << 26,
        UINT32_C(0x3b) << 26,
    };
    size_t opcode_index;

    for (opcode_index = 0u;
         opcode_index < sizeof baseline_opcodes / sizeof baseline_opcodes[0];
         opcode_index++) {
      uint32_t program[] = {baseline_opcodes[opcode_index]};
      t64_machine *baseline = prepare_profile_program(
          environment, baseline_profiles[index], TEST_PC, program, 1u);

      check_bool("strict/compat-v1 baseline fixture admitted",
                 baseline != NULL);
      if (baseline == NULL) continue;
      baseline->cpu.cp0[12] |= TEST_STATUS_CU2 | TEST_STATUS_CU3;
      check_unsupported(baseline, UINT64_C(1),
                        "strict/compat-v1 retains unsupported outcome");
      destroy_machine(baseline);
    }
  }
}

static const cpu_case integer_cases[] = {
    {"word/doubleword ALU, immediate, comparisons and zero register",
     test_alu_word_doubleword_and_immediate},
    {"fixed, variable, 32-offset and arithmetic shift matrix",
     test_shift_matrix},
    {"HI/LO moves, signed/unsigned products and SYNC",
     test_hilo_multiply_and_move_matrix},
    {"word/doubleword signed and unsigned division",
     test_hilo_division_matrix},
    {"compat word/doubleword divide undefined cases use fixed results",
     test_compat_division_undefined_cases},
    {"all register/immediate trap operations and false controls",
     test_trap_register_and_immediate_matrix},
    {"divide zero/overflow policy refuses undefined mutations",
     test_divide_by_zero_and_signed_overflow_policy},
    {"word/doubleword arithmetic overflow enters precise exception",
     test_arithmetic_overflow_exceptions},
    {"reserved integer encodings raise RI before side effects",
     test_reserved_integer_encodings_raise_ri},
    {"compat-v2 COP2 latch transfers, COP3 RI and reserved opcodes",
     test_compat_v2_coprocessor_transfer_policy},
};

static const cpu_case control_cases[] = {
    {"all normal/likely/REGIMM branches test both outcomes",
     test_conditional_branch_outcomes},
    {"JAL/JALR full-width targets and link source alias",
     test_jump_targets_and_link_alias},
    {"negative full-width branch offset and undefined branch-in-slot",
     test_negative_branch_target_and_branch_in_slot},
    {"compat branch-in-delay policy retargets through one nested slot",
     test_compat_branch_in_delay_slot_has_one_nested_slot},
    {"compat nested likely branches handle taken and annulled outcomes",
     test_compat_nested_likely_branch_taken_and_annulled},
    {"compat displaced-slot fault uses inner branch EPC and BD",
     test_compat_displaced_slot_fault_uses_inner_branch_origin},
    {"reserved control fields raise RI without partial mutation",
     test_reserved_integer_encodings_raise_ri},
};

int main(int argc, char **argv) {
  allocator_state allocator_state_value = {0u};
  const t64_host_allocator allocator = {
      test_allocate, test_release, &allocator_state_value};
  const t64_profile_descriptor *profile = NULL;
  test_environment environment = {&allocator, NULL, NULL};
  const cpu_case *cases;
  size_t case_count;
  uint8_t asset[IPL3_ASSET_BYTES];
  size_t index;
  bool integer_mode;

  if (argc != 2 ||
      (strcmp(argv[1], "--integer") != 0 &&
       strcmp(argv[1], "--control") != 0)) {
    (void)puts("1..1");
    (void)puts("not ok 1 - select --integer or --control");
    return 2;
  }
  integer_mode = strcmp(argv[1], "--integer") == 0;
  cases = integer_mode ? integer_cases : control_cases;
  case_count = integer_mode
                   ? sizeof integer_cases / sizeof integer_cases[0]
                   : sizeof control_cases / sizeof control_cases[0];
  if (!read_qualified_asset(asset) ||
      t64_profile_lookup("ntsc-cart-4m-openipl3-vr4300-v1", &profile) !=
          T64_STATUS_OK ||
      profile == NULL) {
    (void)puts("1..1");
    (void)puts("not ok 1 - qualified IPL3 and VR4300 profile setup");
    return 1;
  }
  environment.profile = profile;
  environment.asset = asset;
  (void)printf("1..%zu\n", case_count);
  for (index = 0u; index < case_count; index++) {
    int prior_failures = t64_test_failures;

    cases[index].run(&environment);
    (void)printf("%s %zu - %s\n",
                 prior_failures == t64_test_failures ? "ok" : "not ok",
                 index + 1u, cases[index].name);
  }
  if (allocator_state_value.outstanding != 0u) {
    check_u64("all CPU test machines are released",
              allocator_state_value.outstanding, UINT64_C(0));
  }
  return T64_TEST_FINISH(integer_mode ? "unit.core.cpu-integer"
                                     : "unit.core.cpu-control");
}
