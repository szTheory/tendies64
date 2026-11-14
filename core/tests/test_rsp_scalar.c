#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_IMEM_BASE UINT32_C(0x04001000)
#define TEST_DMEM_BASE UINT32_C(0x04000000)

typedef struct {
  uint32_t outcome;
  uint32_t refusal_reason;
  uint32_t pc;
  uint64_t retired;
  uint32_t gpr[32];
} rsp_scalar_observation;

typedef struct {
  t64_host_allocator allocator;
  t64_machine *machine;
} rsp_scalar_fixture;

static bool fixture_allocate(void *context, size_t size, size_t alignment,
                             t64_host_storage *storage_out) {
  void *data;

  (void)context;
  if (storage_out == NULL || size == 0u || alignment == 0u) {
    return false;
  }
  data = malloc(size);
  if (data == NULL || (uintptr_t)data % alignment != 0u) {
    free(data);
    return false;
  }
  storage_out->data = data;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  return true;
}

static void fixture_release(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static bool fixture_create(rsp_scalar_fixture *fixture) {
  const t64_profile_descriptor *profile = NULL;
  t64_host_storage image = {NULL, 0u, 0u};
  t64_status status;

  if (fixture == NULL) {
    return false;
  }
  (void)memset(fixture, 0, sizeof *fixture);
  fixture->allocator.allocate = fixture_allocate;
  fixture->allocator.release = fixture_release;
  status = t64_profile_lookup(
      "ntsc-cart-4m-openipl3-rsp-functional-v1", &profile);
  if (status != T64_STATUS_OK || profile == NULL ||
      t64_machine_create(&fixture->allocator, profile, &fixture->machine) !=
          T64_STATUS_OK ||
      fixture->machine == NULL ||
      !fixture_allocate(fixture, 2u, 1u, &image)) {
    return false;
  }
  ((uint8_t *)image.data)[0] = UINT8_C(0x5a);
  ((uint8_t *)image.data)[1] = UINT8_C(0xa5);
  fixture->machine->image_storage = image;
  fixture->machine->image_length = 2u;
  fixture->machine->has_image = true;
  return true;
}

static void fixture_destroy(rsp_scalar_fixture *fixture) {
  if (fixture != NULL && fixture->machine != NULL) {
    t64_machine_destroy(fixture->machine);
    fixture->machine = NULL;
  }
}

static uint32_t encode_i(uint32_t operation, uint32_t rs, uint32_t rt,
                         uint16_t immediate) {
  return (operation << 26) | (rs << 21) | (rt << 16) | immediate;
}

static uint32_t encode_r(uint32_t rs, uint32_t rt, uint32_t rd,
                         uint32_t shift, uint32_t function) {
  return (rs << 21) | (rt << 16) | (rd << 11) | (shift << 6) | function;
}

static uint32_t encode_cop0(bool to_cp0, uint32_t rt, uint32_t rd) {
  return (UINT32_C(0x10) << 26) | ((to_cp0 ? UINT32_C(4) : UINT32_C(0)) << 21) |
         (rt << 16) | (rd << 11);
}

static bool prepare_program(rsp_scalar_fixture *fixture,
                            const uint32_t *program, size_t instruction_count,
                            const uint32_t registers[32]) {
  t64_machine *machine;
  size_t index;

  if (fixture == NULL || fixture->machine == NULL || program == NULL ||
      instruction_count == 0u || instruction_count > 1024u) {
    return false;
  }
  machine = fixture->machine;
  t64_machine_reset_execution_state(machine);
  (void)memset(&machine->rsp, 0, sizeof machine->rsp);
  (void)memset(&machine->cpu, 0, sizeof machine->cpu);
  (void)memset(machine->sp_dmem, 0, sizeof machine->sp_dmem);
  (void)memset(machine->sp_imem, 0, sizeof machine->sp_imem);
  machine->rsp.sp_status = T64_RSP_STATUS_HALT;
  if (registers != NULL) {
    (void)memcpy(machine->rsp.gpr, registers, sizeof machine->rsp.gpr);
  }
  machine->rsp.gpr[0] = 0u;
  for (index = 0u; index < instruction_count; index++) {
    if (t64_machine_bus_write(
            machine, TEST_IMEM_BASE + (uint32_t)(index * 4u), 4u,
            program[index]) != T64_STATUS_OK) {
      return false;
    }
  }
  return true;
}

static bool run_program(rsp_scalar_fixture *fixture, const uint32_t *program,
                        size_t instruction_count,
                        const uint32_t registers[32], uint64_t ticks,
                        rsp_scalar_observation *observation) {
  t64_machine *machine;
  t64_stop_request request = {T64_STOP_GUEST_TICK, 0u};
  t64_stop_result result = {0u, 0u};

  if (observation == NULL || !prepare_program(fixture, program,
                                               instruction_count, registers)) {
    return false;
  }
  machine = fixture->machine;
  request.value = ticks;
  if (t64_rsp_start(machine) != T64_STATUS_OK ||
      t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
      result.tick != ticks) {
    return false;
  }
  observation->outcome = machine->rsp_observed_outcome;
  observation->refusal_reason = machine->rsp_observed_refusal_reason;
  observation->pc = machine->rsp.pc;
  observation->retired = machine->rsp.retired_instructions;
  (void)memcpy(observation->gpr, machine->rsp.gpr,
               sizeof observation->gpr);
  return true;
}

static bool expect_gpr(rsp_scalar_fixture *fixture, const char *name,
                       uint32_t instruction, uint32_t left, uint32_t right,
                       uint32_t destination, uint32_t expected) {
  uint32_t program[1] = {instruction};
  uint32_t registers[32] = {0u};
  rsp_scalar_observation observed;

  registers[1] = left;
  registers[2] = right;
  if (!run_program(fixture, program, 1u, registers, UINT64_C(1), &observed)) {
    fprintf(stderr, "%s setup failure\n", name);
    return false;
  }
  if (observed.outcome != T64_RSP_ATTEMPT_RETIRED ||
      observed.pc != UINT32_C(4) || observed.gpr[destination] != expected ||
      observed.gpr[0] != 0u) {
    fprintf(stderr,
            "%s fetched semantic assertion failed at PC=0x000: "
            "opcode=0x%08x expected=(retired,pc=0x004,r%u=0x%08x) "
            "actual=(%u,pc=0x%03x,r%u=0x%08x,reason=%u)\n",
            name, instruction, destination, expected,
            (unsigned)observed.outcome, (unsigned)observed.pc, destination,
            observed.gpr[destination], (unsigned)observed.refusal_reason);
    return false;
  }
  return true;
}

static bool scalar_alu_matrix_test(rsp_scalar_fixture *fixture) {
  const struct {
    const char *name;
    uint32_t instruction;
    uint32_t left;
    uint32_t right;
    uint32_t destination;
    uint32_t expected;
  } cases[] = {
      {"ADD wrap", encode_r(1u, 2u, 3u, 0u, 0x20u), 0x7fffffffu, 1u, 3u,
       0x80000000u},
      {"ADDU wrap", encode_r(1u, 2u, 3u, 0u, 0x21u), 0xffffffffu, 1u, 3u,
       0u},
      {"SUB wrap", encode_r(1u, 2u, 3u, 0u, 0x22u), 0x80000000u, 1u, 3u,
       0x7fffffffu},
      {"SUBU wrap", encode_r(1u, 2u, 3u, 0u, 0x23u), 0u, 1u, 3u,
       0xffffffffu},
      {"AND", encode_r(1u, 2u, 3u, 0u, 0x24u), 0xf0f0a55au, 0x0ff00f0fu,
       3u, 0x00f0050au},
      {"OR", encode_r(1u, 2u, 3u, 0u, 0x25u), 0xf0000001u, 0x00ff0002u,
       3u, 0xf0ff0003u},
      {"XOR", encode_r(1u, 2u, 3u, 0u, 0x26u), 0xf0000001u, 0x00ff0002u,
       3u, 0xf0ff0003u},
      {"NOR", encode_r(1u, 2u, 3u, 0u, 0x27u), 0xf0000001u, 0x00ff0002u,
       3u, 0x0f00fffcu},
      {"SLT signed", encode_r(1u, 2u, 3u, 0u, 0x2au), 0x80000000u, 1u,
       3u, 1u},
      {"SLTU unsigned", encode_r(1u, 2u, 3u, 0u, 0x2bu), 0x80000000u,
       1u, 3u, 0u},
      {"SLL", encode_r(0u, 2u, 3u, 31u, 0x00u), 0u, 3u, 3u, 0x80000000u},
      {"SRL", encode_r(0u, 2u, 3u, 31u, 0x02u), 0u, 0x80000000u, 3u,
       1u},
      {"SRA", encode_r(0u, 2u, 3u, 31u, 0x03u), 0u, 0x80000000u, 3u,
       0xffffffffu},
      {"SLLV", encode_r(1u, 2u, 3u, 0u, 0x04u), 33u, 3u, 3u, 6u},
      {"SRLV", encode_r(1u, 2u, 3u, 0u, 0x06u), 33u, 0x80000000u, 3u,
       0x40000000u},
      {"SRAV", encode_r(1u, 2u, 3u, 0u, 0x07u), 31u, 0x80000000u, 3u,
       0xffffffffu},
      {"ADDI", encode_i(0x08u, 1u, 3u, 0x7fffu), 0x7ffffff0u, 0u, 3u,
       0x80007fefu},
      {"ADDIU", encode_i(0x09u, 1u, 3u, 0xffffu), 0u, 0u, 3u,
       0xffffffffu},
      {"ANDI", encode_i(0x0cu, 1u, 3u, 0xf00fu), 0xf0f0a55au, 0u, 3u,
       0x0000a00au},
      {"ORI", encode_i(0x0du, 1u, 3u, 0xf00fu), 0x00000a50u, 0u, 3u,
       0x0000fa5fu},
      {"XORI", encode_i(0x0eu, 1u, 3u, 0xf00fu), 0x0000a55au, 0u, 3u,
       0x00005555u},
      {"LUI", encode_i(0x0fu, 0u, 3u, 0x8123u), 0u, 0u, 3u,
       0x81230000u},
      {"SLTI signed", encode_i(0x0au, 1u, 3u, 0xffffu), 0xffffffffu, 0u,
       3u, 0u},
      {"SLTIU signed immediate", encode_i(0x0bu, 1u, 3u, 0xffffu),
       0x7fffffffu, 0u, 3u, 1u}};
  size_t index;
  bool passed = true;

  for (index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
    if (!expect_gpr(fixture, cases[index].name, cases[index].instruction,
                    cases[index].left, cases[index].right,
                    cases[index].destination, cases[index].expected)) {
      passed = false;
    }
  }
  if (!expect_gpr(fixture, "zero register", encode_i(0x09u, 1u, 0u, 9u),
                  0x12345678u, 0u, 0u, 0u)) {
    passed = false;
  }
  return passed;
}

static bool expect_load(rsp_scalar_fixture *fixture, const char *name,
                        uint32_t operation, uint32_t base, uint16_t offset,
                        const uint8_t bytes[4096], uint32_t expected) {
  uint32_t program[1] = {encode_i(operation, 1u, 3u, offset)};
  uint32_t registers[32] = {0u};
  rsp_scalar_observation observed;

  registers[1] = base;
  if (!prepare_program(fixture, program, 1u, registers)) {
    fprintf(stderr, "%s setup failure while preparing fetched load\n", name);
    return false;
  }
  (void)memcpy(fixture->machine->sp_dmem, bytes,
               sizeof fixture->machine->sp_dmem);
  if (t64_rsp_start(fixture->machine) != T64_STATUS_OK) {
    return false;
  }
  {
    t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(1)};
    t64_stop_result result = {0u, 0u};

    if (t64_machine_advance(fixture->machine, &request, &result) !=
            T64_STATUS_OK ||
        result.tick != UINT64_C(1)) {
      fprintf(stderr, "%s setup failure while advancing fetched load\n", name);
      return false;
    }
  }
  observed.outcome = fixture->machine->rsp_observed_outcome;
  observed.refusal_reason = fixture->machine->rsp_observed_refusal_reason;
  observed.pc = fixture->machine->rsp.pc;
  observed.retired = fixture->machine->rsp.retired_instructions;
  (void)memcpy(observed.gpr, fixture->machine->rsp.gpr,
               sizeof observed.gpr);
  if (observed.outcome != T64_RSP_ATTEMPT_RETIRED ||
      observed.gpr[3] != expected || observed.pc != UINT32_C(4)) {
    fprintf(stderr,
            "%s fetched memory assertion failed at PC=0x000 opcode=0x%08x "
            "expected r3=0x%08x actual outcome=%u r3=0x%08x pc=0x%03x "
            "reason=%u\n",
            name, program[0], expected, (unsigned)observed.outcome,
            observed.gpr[3], (unsigned)observed.pc,
            (unsigned)observed.refusal_reason);
    return false;
  }
  return true;
}

static bool scalar_load_matrix_test(rsp_scalar_fixture *fixture) {
  uint8_t bytes[4096] = {0u};
  bool passed = true;

  bytes[4095] = UINT8_C(0x80);
  if (!expect_load(fixture, "LB wrap", 0x20u, 0xfffu, 0u, bytes,
                   UINT32_C(0xffffff80))) {
    passed = false;
  }
  if (!expect_load(fixture, "LBU wrap", 0x24u, 0xfffu, 0u, bytes,
                   UINT32_C(0x80))) {
    passed = false;
  }
  bytes[0] = UINT8_C(0x01);
  if (!expect_load(fixture, "LH wrap", 0x21u, 0xfffu, 0u, bytes,
                   UINT32_C(0xffff8001))) {
    passed = false;
  }
  if (!expect_load(fixture, "LHU wrap", 0x25u, 0xfffu, 0u, bytes,
                   UINT32_C(0x8001))) {
    passed = false;
  }
  bytes[4094] = UINT8_C(0x89);
  bytes[4095] = UINT8_C(0xab);
  bytes[0] = UINT8_C(0xcd);
  bytes[1] = UINT8_C(0xef);
  if (!expect_load(fixture, "LW wrap", 0x23u, 0xffeu, 0u, bytes,
                   UINT32_C(0x89abcdef))) {
    passed = false;
  }
  return passed;
}

static bool scalar_store_matrix_test(rsp_scalar_fixture *fixture) {
  const struct {
    const char *name;
    uint32_t operation;
    uint32_t width;
    uint32_t base;
    uint32_t value;
  } cases[] = {{"SB wrap", 0x28u, 1u, 0xfffu, 0x89abcdefu},
               {"SH wrap", 0x29u, 2u, 0xfffu, 0x89abcdefu},
               {"SW wrap", 0x2bu, 4u, 0xffeu, 0x89abcdefu}};
  uint8_t expected[4096];
  size_t index;
  bool passed = true;

  for (index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
    uint32_t program[1] = {encode_i(cases[index].operation, 1u, 2u, 0u)};
    uint32_t registers[32] = {0u};
    rsp_scalar_observation observed;
    uint32_t byte;

    registers[1] = cases[index].base;
    registers[2] = cases[index].value;
    if (!prepare_program(fixture, program, 1u, registers)) {
      fprintf(stderr, "%s setup failure while preparing fetched store\n",
              cases[index].name);
      passed = false;
      continue;
    }
    (void)memset(fixture->machine->sp_dmem, 0xa5,
                 sizeof fixture->machine->sp_dmem);
    (void)memcpy(expected, fixture->machine->sp_dmem, sizeof expected);
    for (byte = 0u; byte < cases[index].width; byte++) {
      uint32_t address = (cases[index].base + byte) & UINT32_C(0x0fff);
      uint32_t shift = (cases[index].width - byte - 1u) * 8u;

      expected[address] = (uint8_t)(cases[index].value >> shift);
    }
    if (t64_rsp_start(fixture->machine) != T64_STATUS_OK) {
      fprintf(stderr, "%s setup failure starting RSP\n", cases[index].name);
      passed = false;
      continue;
    }
    {
      t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(1)};
      t64_stop_result result = {0u, 0u};

      if (t64_machine_advance(fixture->machine, &request, &result) !=
              T64_STATUS_OK ||
          result.tick != UINT64_C(1)) {
        fprintf(stderr, "%s setup failure while advancing fetched store\n",
                cases[index].name);
        passed = false;
        continue;
      }
    }
    observed.outcome = fixture->machine->rsp_observed_outcome;
    observed.refusal_reason = fixture->machine->rsp_observed_refusal_reason;
    observed.pc = fixture->machine->rsp.pc;
    observed.retired = fixture->machine->rsp.retired_instructions;
    (void)memcpy(observed.gpr, fixture->machine->rsp.gpr,
                 sizeof observed.gpr);
    if (observed.outcome != T64_RSP_ATTEMPT_RETIRED ||
        observed.pc != UINT32_C(4) ||
        memcmp(expected, fixture->machine->sp_dmem, sizeof expected) != 0) {
      fprintf(stderr,
              "%s fetched memory assertion failed at PC=0x000 "
              "opcode=0x%08x expected exact DMEM image; actual outcome=%u "
              "pc=0x%03x reason=%u\n",
              cases[index].name, program[0], (unsigned)observed.outcome,
              (unsigned)observed.pc, (unsigned)observed.refusal_reason);
      passed = false;
    }
  }
  return passed;
}

static bool expect_branch(rsp_scalar_fixture *fixture, const char *name,
                          uint32_t instruction, uint32_t left, uint32_t right,
                          bool taken, bool links) {
  uint32_t program[5] = {instruction,
                         encode_i(0x09u, 0u, 3u, 5u), 0u, 0u, 0u};
  uint32_t registers[32] = {0u};
  rsp_scalar_observation observed;
  uint32_t expected_pc = taken ? UINT32_C(12) : UINT32_C(8);

  registers[1] = left;
  registers[2] = right;
  if (!run_program(fixture, program, 5u, registers, UINT64_C(2), &observed)) {
    fprintf(stderr, "%s branch setup failure\n", name);
    return false;
  }
  if (observed.outcome != T64_RSP_ATTEMPT_RETIRED ||
      observed.pc != expected_pc || observed.gpr[3] != 5u ||
      (links && observed.gpr[31] != 8u)) {
    fprintf(stderr,
            "%s fetched branch assertion failed at PC=0x000 "
            "opcode=0x%08x expected pc=0x%03x delay-r3=5%s actual "
            "outcome=%u pc=0x%03x r3=0x%08x r31=0x%08x reason=%u\n",
            name, instruction, expected_pc, links ? " link-r31=8" : "",
            (unsigned)observed.outcome, (unsigned)observed.pc,
            observed.gpr[3], observed.gpr[31],
            (unsigned)observed.refusal_reason);
    return false;
  }
  return true;
}

static bool scalar_branch_matrix_test(rsp_scalar_fixture *fixture) {
  bool passed = true;

  if (!expect_branch(fixture, "BEQ", encode_i(0x04u, 1u, 2u, 2u), 4u,
                     4u, true, false) ||
      !expect_branch(fixture, "BNE", encode_i(0x05u, 1u, 2u, 2u), 4u,
                     5u, true, false) ||
      !expect_branch(fixture, "BLEZ", encode_i(0x06u, 1u, 0u, 2u), 0u,
                     0u, true, false) ||
      !expect_branch(fixture, "BGTZ", encode_i(0x07u, 1u, 0u, 2u), 1u,
                     0u, true, false) ||
      !expect_branch(fixture, "BLTZ", encode_i(0x01u, 1u, 0u, 2u),
                     UINT32_MAX, 0u, true, false) ||
      !expect_branch(fixture, "BGEZ", encode_i(0x01u, 1u, 1u, 2u), 0u,
                     0u, true, false) ||
      !expect_branch(fixture, "BLTZAL", encode_i(0x01u, 1u, 16u, 2u),
                     UINT32_MAX, 0u, true, true) ||
      !expect_branch(fixture, "BGEZAL", encode_i(0x01u, 1u, 17u, 2u),
                     0u, 0u, true, true) ||
      !expect_branch(fixture, "BNE untaken", encode_i(0x05u, 1u, 2u, 2u),
                     4u, 4u, false, false) ||
      !expect_branch(fixture, "BLTZAL untaken still links",
                     encode_i(0x01u, 1u, 16u, 2u), 0u, 0u, false, true)) {
    passed = false;
  }
  return passed;
}

static bool branch_in_delay_slot_refusal_test(rsp_scalar_fixture *fixture) {
  uint32_t program[2] = {encode_i(0x04u, 1u, 2u, 2u),
                         encode_i(0x01u, 1u, 17u, 3u)};
  uint32_t registers[32] = {0u};
  rsp_scalar_observation observed;

  registers[1] = 0u;
  registers[2] = 0u;
  registers[31] = UINT32_C(0x13579bdf);
  if (!run_program(fixture, program, 2u, registers, UINT64_C(2), &observed)) {
    fprintf(stderr, "branch-in-delay setup failure\n");
    return false;
  }
  if (observed.outcome != T64_RSP_ATTEMPT_REFUSED || observed.pc != 4u ||
      observed.gpr[31] != UINT32_C(0x13579bdf) ||
      !fixture->machine->rsp.delay_slot_pending ||
      fixture->machine->rsp.delay_target != UINT32_C(12) ||
      observed.retired != UINT64_C(1)) {
    fprintf(stderr,
            "branch-in-delay refusal assertion failed: expected no second "
            "retirement/link/target change; actual outcome=%u pc=0x%03x "
            "r31=0x%08x target=0x%03x retired=%llu\n",
            (unsigned)observed.outcome, (unsigned)observed.pc,
            observed.gpr[31], fixture->machine->rsp.delay_target,
            (unsigned long long)observed.retired);
    return false;
  }
  return true;
}

static bool scalar_jump_matrix_test(rsp_scalar_fixture *fixture) {
  const struct {
    const char *name;
    uint32_t instruction;
    uint32_t target;
    bool links;
    uint32_t link_register;
  } cases[] = {
      {"J", (UINT32_C(2) << 26) | UINT32_C(0x0a), 40u, false, 0u},
      {"JAL", (UINT32_C(3) << 26) | UINT32_C(0x0a), 40u, true, 31u},
      {"JR", encode_r(1u, 0u, 0u, 0u, 0x08u), 64u, false, 0u},
      {"JALR", encode_r(1u, 0u, 3u, 0u, 0x09u), 64u, true, 3u}};
  size_t index;
  bool passed = true;

  for (index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
    uint32_t program[2] = {cases[index].instruction,
                           encode_i(0x09u, 0u, 2u, 7u)};
    uint32_t registers[32] = {0u};
    rsp_scalar_observation observed;

    registers[1] = cases[index].target;
    if (!run_program(fixture, program, 2u, registers, UINT64_C(2),
                     &observed)) {
      fprintf(stderr, "%s setup failure\n", cases[index].name);
      passed = false;
      continue;
    }
    if (observed.outcome != T64_RSP_ATTEMPT_RETIRED ||
        observed.pc != cases[index].target || observed.gpr[2] != 7u ||
        (cases[index].links &&
         observed.gpr[cases[index].link_register] != 8u)) {
      fprintf(stderr,
              "%s fetched jump assertion failed at PC=0x000: "
              "expected target=0x%03x after delay and link=%s; actual "
              "outcome=%u pc=0x%03x r2=%u reason=%u\n",
              cases[index].name, cases[index].target,
              cases[index].link_register == 31u ? "r31=8" : "rd=8",
              (unsigned)observed.outcome, (unsigned)observed.pc,
              observed.gpr[2], (unsigned)observed.refusal_reason);
      passed = false;
    }
  }
  return passed;
}

static bool cp0_matrix_test(rsp_scalar_fixture *fixture) {
  static const uint32_t expected_reads[8] = {
      UINT32_C(0x0abc), UINT32_C(0x123456), UINT32_C(0x000007),
      UINT32_C(0x000009), T64_RSP_STATUS_INTR_BREAK, 0u, 0u, 0u};
  bool passed = true;
  uint32_t reg;

  for (reg = 0u; reg < 8u; reg++) {
    uint32_t program[1] = {encode_cop0(false, 3u, reg)};
    uint32_t registers[32] = {0u};
    rsp_scalar_observation observed;

    if (!prepare_program(fixture, program, 1u, registers)) {
      fprintf(stderr, "MFC0 c%u setup failure\n", reg);
      passed = false;
      continue;
    }
    fixture->machine->sp.mem_address = UINT32_C(0x0abc);
    fixture->machine->sp.dram_address = UINT32_C(0x123456);
    fixture->machine->sp.read_length = UINT32_C(7);
    fixture->machine->sp.write_length = UINT32_C(9);
    fixture->machine->rsp.sp_status = T64_RSP_STATUS_INTR_BREAK;
    if (t64_rsp_start(fixture->machine) != T64_STATUS_OK) {
      passed = false;
      continue;
    }
    {
      t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(1)};
      t64_stop_result result = {0u, 0u};

      if (t64_machine_advance(fixture->machine, &request, &result) !=
              T64_STATUS_OK ||
          result.tick != UINT64_C(1)) {
        fprintf(stderr, "MFC0 c%u setup failure while advancing\n", reg);
        passed = false;
        continue;
      }
    }
    observed.outcome = fixture->machine->rsp_observed_outcome;
    observed.refusal_reason = fixture->machine->rsp_observed_refusal_reason;
    observed.pc = fixture->machine->rsp.pc;
    observed.retired = fixture->machine->rsp.retired_instructions;
    (void)memcpy(observed.gpr, fixture->machine->rsp.gpr,
                 sizeof observed.gpr);
    if (observed.outcome != T64_RSP_ATTEMPT_RETIRED ||
        observed.gpr[3] != expected_reads[reg] || observed.pc != 4u ||
        (reg == 7u && !fixture->machine->rsp.sp_semaphore)) {
      fprintf(stderr,
              "MFC0 c%u fetched assertion failed: expected r3=0x%08x, "
              "got outcome=%u r3=0x%08x pc=0x%03x\n",
              reg, expected_reads[reg], (unsigned)observed.outcome,
              observed.gpr[3], (unsigned)observed.pc);
      passed = false;
    }
  }

  for (reg = 0u; reg < 8u; reg++) {
    uint32_t program[1] = {encode_cop0(true, 2u, reg)};
    uint32_t registers[32] = {0u};
    rsp_scalar_observation observed;
    uint32_t value = reg == 4u ? UINT32_C(0x400) :
                     reg == 7u ? UINT32_C(0) :
                     reg == 2u || reg == 3u ? UINT32_C(7) :
                     reg == 0u ? UINT32_C(0x0200) :
                     reg == 1u ? UINT32_C(0x0300) : UINT32_C(0);
    t64_rsp_state rsp_before;
    t64_sp_state sp_before;
    uint8_t dmem_before[4096];

    registers[2] = value;
    if (!prepare_program(fixture, program, 1u, registers)) {
      fprintf(stderr, "MTC0 c%u setup failure\n", reg);
      passed = false;
      continue;
    }
    fixture->machine->sp.mem_address = 0u;
    fixture->machine->sp.dram_address = 0u;
    fixture->machine->rsp.sp_semaphore = reg == 7u;
    if (t64_rsp_start(fixture->machine) != T64_STATUS_OK) {
      passed = false;
      continue;
    }
    rsp_before = fixture->machine->rsp;
    sp_before = fixture->machine->sp;
    (void)memcpy(dmem_before, fixture->machine->sp_dmem,
                 sizeof dmem_before);
    {
      t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(1)};
      t64_stop_result result = {0u, 0u};

      if (t64_machine_advance(fixture->machine, &request, &result) !=
              T64_STATUS_OK ||
          result.tick != UINT64_C(1)) {
        fprintf(stderr, "MTC0 c%u setup failure while advancing\n", reg);
        passed = false;
        continue;
      }
    }
    observed.outcome = fixture->machine->rsp_observed_outcome;
    observed.refusal_reason = fixture->machine->rsp_observed_refusal_reason;
    observed.pc = fixture->machine->rsp.pc;
    observed.retired = fixture->machine->rsp.retired_instructions;
    (void)memcpy(observed.gpr, fixture->machine->rsp.gpr,
                 sizeof observed.gpr);
    if (reg == 5u || reg == 6u) {
      if (observed.outcome != T64_RSP_ATTEMPT_REFUSED ||
          observed.refusal_reason != T64_RSP_REFUSAL_CP0_READ_ONLY ||
          observed.pc != 0u ||
          memcmp(&rsp_before, &fixture->machine->rsp, sizeof rsp_before) !=
              0 ||
          memcmp(&sp_before, &fixture->machine->sp, sizeof sp_before) != 0 ||
          memcmp(dmem_before, fixture->machine->sp_dmem,
                 sizeof dmem_before) != 0) {
        fprintf(stderr,
                "MTC0 c%u read-only register must refuse before effects: "
                "outcome=%u reason=%u pc=0x%03x\n",
                reg, (unsigned)observed.outcome,
                (unsigned)observed.refusal_reason, (unsigned)observed.pc);
        passed = false;
      }
      continue;
    }
    if (observed.outcome != T64_RSP_ATTEMPT_RETIRED || observed.pc != 4u) {
      fprintf(stderr,
              "MTC0 c%u fetched assertion failed: expected retirement, "
              "actual outcome=%u reason=%u pc=0x%03x\n",
              reg, (unsigned)observed.outcome,
              (unsigned)observed.refusal_reason, (unsigned)observed.pc);
      passed = false;
      continue;
    }
    if ((reg == 0u && fixture->machine->sp.mem_address != value) ||
        (reg == 1u && fixture->machine->sp.dram_address != value) ||
        (reg == 2u &&
         (fixture->machine->sp.read_length != value ||
          !fixture->machine->sp.active.valid)) ||
        (reg == 3u &&
         (fixture->machine->sp.write_length != value ||
          !fixture->machine->sp.active.valid)) ||
        (reg == 4u &&
         (fixture->machine->rsp.sp_status & (UINT32_C(1) << 7)) == 0u) ||
        (reg == 7u && fixture->machine->rsp.sp_semaphore)) {
      fprintf(stderr,
              "MTC0 c%u SP mapping assertion failed after retirement\n",
              reg);
      passed = false;
    }
  }

  {
    uint32_t invalid_program[1] = {encode_cop0(true, 2u, 7u)};
    uint32_t invalid_registers[32] = {0u};
    rsp_scalar_observation invalid_observed;
    t64_rsp_state rsp_before;
    t64_sp_state sp_before;
    uint8_t dmem_before[4096];

    invalid_registers[2] = UINT32_C(1);
    if (!prepare_program(fixture, invalid_program, 1u, invalid_registers) ||
        t64_rsp_start(fixture->machine) != T64_STATUS_OK) {
      passed = false;
    } else {
      rsp_before = fixture->machine->rsp;
      sp_before = fixture->machine->sp;
      (void)memcpy(dmem_before, fixture->machine->sp_dmem,
                   sizeof dmem_before);
      {
        t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(1)};
        t64_stop_result result = {0u, 0u};

        if (t64_machine_advance(fixture->machine, &request, &result) !=
                T64_STATUS_OK ||
            result.tick != UINT64_C(1)) {
          passed = false;
        } else {
          invalid_observed.outcome =
              fixture->machine->rsp_observed_outcome;
          invalid_observed.refusal_reason =
              fixture->machine->rsp_observed_refusal_reason;
          invalid_observed.pc = fixture->machine->rsp.pc;
          invalid_observed.retired =
              fixture->machine->rsp.retired_instructions;
          (void)memcpy(invalid_observed.gpr, fixture->machine->rsp.gpr,
                       sizeof invalid_observed.gpr);
          if (invalid_observed.outcome != T64_RSP_ATTEMPT_REFUSED ||
              invalid_observed.refusal_reason !=
                  T64_RSP_REFUSAL_CP0_INVALID_VALUE ||
              invalid_observed.pc != 0u ||
              invalid_observed.gpr[2] != UINT32_C(1) ||
              memcmp(&rsp_before, &fixture->machine->rsp,
                     sizeof rsp_before) != 0 ||
              memcmp(&sp_before, &fixture->machine->sp,
                     sizeof sp_before) != 0 ||
              memcmp(dmem_before, fixture->machine->sp_dmem,
                     sizeof dmem_before) != 0) {
            fprintf(stderr,
                    "MTC0 c7 nonzero value must refuse before effects\n");
            passed = false;
          }
        }
      }
    }
  }

  for (reg = 8u; reg < 16u; reg++) {
    const bool directions[] = {false, true};
    size_t direction;

    for (direction = 0u; direction < 2u; direction++) {
      uint32_t dp_program[1] = {
          encode_cop0(directions[direction], directions[direction] ? 2u : 3u,
                      reg)};
      uint32_t dp_registers[32] = {0u};
      rsp_scalar_observation dp_observed;
      t64_rsp_state rsp_before;
      t64_sp_state sp_before;
      uint8_t dmem_before[4096];

      dp_registers[2] = UINT32_C(0x87654321);
      if (!prepare_program(fixture, dp_program, 1u, dp_registers)) {
        fprintf(stderr, "CP0 DP%u refusal setup failure\n", reg);
        passed = false;
        continue;
      }
      if (t64_rsp_start(fixture->machine) != T64_STATUS_OK) {
        passed = false;
        continue;
      }
      rsp_before = fixture->machine->rsp;
      sp_before = fixture->machine->sp;
      (void)memcpy(dmem_before, fixture->machine->sp_dmem,
                   sizeof dmem_before);
      {
        t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(1)};
        t64_stop_result result = {0u, 0u};

        if (t64_machine_advance(fixture->machine, &request, &result) !=
                T64_STATUS_OK ||
            result.tick != UINT64_C(1)) {
          fprintf(stderr, "CP0 DP%u setup failure advancing\n", reg);
          passed = false;
          continue;
        }
      }
      dp_observed.outcome = fixture->machine->rsp_observed_outcome;
      dp_observed.refusal_reason =
          fixture->machine->rsp_observed_refusal_reason;
      dp_observed.pc = fixture->machine->rsp.pc;
      dp_observed.retired = fixture->machine->rsp.retired_instructions;
      (void)memcpy(dp_observed.gpr, fixture->machine->rsp.gpr,
                   sizeof dp_observed.gpr);
      if (dp_observed.outcome != T64_RSP_ATTEMPT_REFUSED ||
          dp_observed.refusal_reason != T64_RSP_REFUSAL_DP_UNAVAILABLE ||
          dp_observed.pc != 0u ||
          dp_observed.gpr[directions[direction] ? 2u : 3u] !=
              (directions[direction] ? UINT32_C(0x87654321) : 0u) ||
          memcmp(&rsp_before, &fixture->machine->rsp, sizeof rsp_before) != 0 ||
          memcmp(&sp_before, &fixture->machine->sp, sizeof sp_before) != 0 ||
          memcmp(dmem_before, fixture->machine->sp_dmem,
                 sizeof dmem_before) != 0) {
        fprintf(stderr,
                "CP0 DP%u %s must refuse before effects: opcode=0x%08x "
                "actual outcome=%u reason=%u pc=0x%03x\n",
                reg, directions[direction] ? "write" : "read",
                dp_program[0], (unsigned)dp_observed.outcome,
                (unsigned)dp_observed.refusal_reason,
                (unsigned)dp_observed.pc);
        passed = false;
      }
    }
  }
  return passed;
}

static bool unsupported_scalar_refusal_test(rsp_scalar_fixture *fixture) {
  const uint32_t unsupported[] = {
      encode_r(1u, 2u, 3u, 0u, 0x18u), /* MULT */
      encode_i(0x11u, 0u, 2u, 0u),     /* COP1 */
      encode_i(0x14u, 1u, 2u, 1u),    /* BEQL is absent on RSP */
      encode_r(1u, 2u, 3u, 1u, 0x20u) /* ADD reserved shift bits */};
  uint32_t registers[32] = {0u};
  size_t index;
  bool passed = true;

  for (index = 0u; index < sizeof unsupported / sizeof unsupported[0];
       index++) {
    uint32_t program[1] = {unsupported[index]};
    rsp_scalar_observation observed;

    registers[1] = UINT32_C(0x12345678);
    registers[2] = UINT32_C(0x87654321);
    if (!run_program(fixture, program, 1u, registers, UINT64_C(1),
                     &observed)) {
      fprintf(stderr, "unsupported scalar setup failure\n");
      passed = false;
      continue;
    }
    if (observed.outcome != T64_RSP_ATTEMPT_REFUSED ||
        observed.refusal_reason != T64_RSP_REFUSAL_UNSUPPORTED_OPCODE ||
        observed.pc != 0u || observed.retired != 0u ||
        observed.gpr[2] != UINT32_C(0x87654321)) {
      fprintf(stderr,
              "unsupported scalar transaction assertion failed: "
              "opcode=0x%08x outcome=%u reason=%u pc=0x%03x retired=%llu\n",
              unsupported[index], (unsigned)observed.outcome,
              (unsigned)observed.refusal_reason, (unsigned)observed.pc,
              (unsigned long long)observed.retired);
      passed = false;
    }
  }
  return passed;
}

int main(int argc, char **argv) {
  rsp_scalar_fixture fixture;
  bool passed = fixture_create(&fixture);

  if (!passed) {
    fprintf(stderr, "fixture setup failure\n");
    fixture_destroy(&fixture);
    return 1;
  }
  if (argc == 2 && strcmp(argv[1], "--addi") == 0) {
    passed = expect_gpr(
        &fixture, "ADDI sign-extension and wrap",
        encode_i(0x08u, 1u, 3u, UINT16_C(0x7fff)), UINT32_C(0x7ffffff0), 0u,
        3u, UINT32_C(0x80007fef));
    fixture_destroy(&fixture);
    if (passed) {
      puts("RSP fetched ADDI case PASS");
      return 0;
    }
    return 1;
  }
  passed = scalar_alu_matrix_test(&fixture) && passed;
  passed = scalar_load_matrix_test(&fixture) && passed;
  passed = scalar_store_matrix_test(&fixture) && passed;
  passed = scalar_branch_matrix_test(&fixture) && passed;
  passed = branch_in_delay_slot_refusal_test(&fixture) && passed;
  passed = scalar_jump_matrix_test(&fixture) && passed;
  passed = cp0_matrix_test(&fixture) && passed;
  passed = unsupported_scalar_refusal_test(&fixture) && passed;
  fixture_destroy(&fixture);
  if (!passed) {
    return 1;
  }
  puts("RSP fetched scalar, control, memory, and CP0 matrix PASS");
  return 0;
}
