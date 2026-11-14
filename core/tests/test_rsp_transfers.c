#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_IMEM_BASE UINT32_C(0x04001000)

typedef struct {
  t64_host_allocator allocator;
  t64_machine *machine;
} rsp_transfer_fixture;

typedef struct {
  uint32_t outcome;
  uint32_t refusal_reason;
  uint32_t pc;
  uint64_t retired;
  uint32_t gpr[32];
} rsp_transfer_observation;

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

static bool fixture_create(rsp_transfer_fixture *fixture) {
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

static void fixture_destroy(rsp_transfer_fixture *fixture) {
  if (fixture != NULL && fixture->machine != NULL) {
    t64_machine_destroy(fixture->machine);
    fixture->machine = NULL;
  }
}

static uint32_t encode_i(uint32_t operation, uint32_t base, uint32_t vt,
                         uint32_t subopcode, uint32_t element,
                         int32_t offset) {
  return (operation << 26) | (base << 21) | (vt << 16) |
         (subopcode << 11) | ((element & UINT32_C(0x0f)) << 7) |
         ((uint32_t)offset & UINT32_C(0x7f));
}

static uint32_t encode_cop2(uint32_t rs, uint32_t rt, uint32_t rd,
                            uint32_t low) {
  return (UINT32_C(0x12) << 26) | (rs << 21) | (rt << 16) | (rd << 11) |
         (low & UINT32_C(0x7ff));
}

static bool prepare_instruction(rsp_transfer_fixture *fixture,
                                uint32_t instruction, uint32_t base) {
  t64_machine *machine;
  uint32_t reg;
  uint32_t byte;

  if (fixture == NULL || fixture->machine == NULL) {
    return false;
  }
  machine = fixture->machine;
  t64_machine_reset_execution_state(machine);
  (void)memset(&machine->rsp, 0, sizeof machine->rsp);
  (void)memset(&machine->cpu, 0, sizeof machine->cpu);
  (void)memset(machine->sp_dmem, 0, sizeof machine->sp_dmem);
  (void)memset(machine->sp_imem, 0, sizeof machine->sp_imem);
  for (byte = 0u; byte < T64_SP_BANK_BYTES; byte++) {
    machine->sp_dmem[byte] = (uint8_t)(byte * 29u + 7u);
  }
  for (reg = 0u; reg < 32u; reg++) {
    for (byte = 0u; byte < 16u; byte++) {
      machine->rsp.vector[reg][byte] =
          (uint8_t)(reg * 43u + byte * 17u + 3u);
    }
  }
  for (reg = 0u; reg < 8u; reg++) {
    machine->rsp.accumulator[reg] =
        UINT64_C(0x1234567800000000) | ((uint64_t)reg << 16) | reg;
  }
  machine->rsp.vco = UINT16_C(0x935a);
  machine->rsp.vcc = UINT16_C(0x6ac3);
  machine->rsp.vce = UINT8_C(0xd7);
  machine->rsp.sp_status = T64_RSP_STATUS_HALT;
  machine->rsp.gpr[1] = base;
  machine->rsp.gpr[2] = UINT32_C(0x89abcdef);
  return t64_machine_bus_write(machine, TEST_IMEM_BASE, 4u, instruction) ==
         T64_STATUS_OK;
}

static bool execute_prepared(rsp_transfer_fixture *fixture,
                             rsp_transfer_observation *observed) {
  t64_machine *machine;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(1)};
  t64_stop_result result = {0u, 0u};

  if (fixture == NULL || fixture->machine == NULL || observed == NULL) {
    return false;
  }
  machine = fixture->machine;
  if (t64_rsp_start(machine) != T64_STATUS_OK ||
      t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
      result.tick != UINT64_C(1)) {
    return false;
  }
  observed->outcome = machine->rsp_observed_outcome;
  observed->refusal_reason = machine->rsp_observed_refusal_reason;
  observed->pc = machine->rsp.pc;
  observed->retired = machine->rsp.retired_instructions;
  (void)memcpy(observed->gpr, machine->rsp.gpr, sizeof observed->gpr);
  return true;
}

static bool run_instruction(rsp_transfer_fixture *fixture,
                            uint32_t instruction, uint32_t base,
                            rsp_transfer_observation *observed) {
  return prepare_instruction(fixture, instruction, base) &&
         execute_prepared(fixture, observed);
}

static bool expect_retired(rsp_transfer_fixture *fixture, const char *name,
                           uint32_t instruction) {
  rsp_transfer_observation observed;

  if (!run_instruction(fixture, instruction, UINT32_C(0x140), &observed)) {
    fprintf(stderr, "%s fetched setup failure\n", name);
    return false;
  }
  if (observed.outcome != T64_RSP_ATTEMPT_RETIRED || observed.pc != 4u ||
      observed.retired != UINT64_C(1)) {
    fprintf(stderr,
            "%s fetched semantic assertion failed at PC=0x000: "
            "opcode=0x%08x expected retired/pc=0x004; actual "
            "outcome=%u reason=%u pc=0x%03x retired=%llu\n",
            name, instruction, (unsigned)observed.outcome,
            (unsigned)observed.refusal_reason, (unsigned)observed.pc,
            (unsigned long long)observed.retired);
    return false;
  }
  return true;
}

static uint16_t read_vector_half(const uint8_t vector[16], uint32_t lane) {
  uint32_t byte = lane * 2u;
  return (uint16_t)(((uint16_t)vector[byte] << 8) | vector[byte + 1u]);
}

static void write_expected_half(uint8_t vector[16], uint32_t lane,
                                uint16_t value) {
  uint32_t byte = lane * 2u;
  vector[byte] = (uint8_t)(value >> 8);
  vector[byte + 1u] = (uint8_t)value;
}

static uint32_t scale_for_subopcode(uint32_t subopcode) {
  if (subopcode == 1u) {
    return 1u;
  }
  if (subopcode == 2u) {
    return 2u;
  }
  if (subopcode == 3u || subopcode == 6u || subopcode == 7u) {
    return 3u;
  }
  if (subopcode >= 4u && subopcode <= 11u) {
    return 4u;
  }
  return 0u;
}

static bool legal_element(bool load, uint32_t subopcode,
                          uint32_t element) {
  switch (subopcode) {
  case 0u:
    return true;
  case 1u:
    return element <= 14u && (element & 1u) == 0u;
  case 2u:
    return element <= 12u && (element & 3u) == 0u;
  case 3u:
    return element == 0u || element == 8u;
  case 4u:
  case 5u:
  case 6u:
  case 7u:
  case 8u:
    return element == 0u;
  case 9u:
    return element == 0u || element == 8u;
  case 10u:
    return !load && element <= 14u && (element & 1u) == 0u;
  case 11u:
    return element <= 14u && (element & 1u) == 0u;
  default:
    return false;
  }
}

static void expected_direct_transfer(bool load, uint32_t subopcode,
                                     uint32_t element, uint32_t address,
                                     uint8_t vectors[32][16],
                                     uint8_t dmem[4096]) {
  const uint32_t vector_register = 3u;
  uint32_t index;

  if (subopcode <= 3u) {
    uint32_t size = subopcode == 0u ? 1u :
                    subopcode == 1u ? 2u :
                    subopcode == 2u ? 4u : 8u;
    for (index = 0u; index < size; index++) {
      uint32_t byte = element + index;
      uint32_t memory_address = (address + index) & UINT32_C(0x0fff);
      if (load) {
        vectors[vector_register][byte] = dmem[memory_address];
      } else {
        dmem[memory_address] = vectors[vector_register][byte];
      }
    }
  } else if (subopcode == 4u || subopcode == 5u) {
    uint32_t count = subopcode == 4u
                         ? 16u - (address & 15u)
                         : address & 15u;
    uint32_t aligned = address & UINT32_C(0x0ff0);
    uint32_t vector_start = subopcode == 4u ? 0u : 16u - count;
    for (index = 0u; index < count; index++) {
      uint32_t byte = vector_start + index;
      uint32_t memory_address =
          (subopcode == 4u ? address + index : aligned + index) &
          UINT32_C(0x0fff);
      if (load) {
        vectors[vector_register][byte] = dmem[memory_address];
      } else {
        dmem[memory_address] = vectors[vector_register][byte];
      }
    }
  } else if (subopcode >= 6u && subopcode <= 9u) {
    uint32_t stride = subopcode == 8u ? 2u :
                      subopcode == 9u ? 4u : 1u;
    uint32_t count = subopcode == 9u ? 4u : 8u;
    uint32_t lane_start = subopcode == 9u ? element / 2u : 0u;
    for (index = 0u; index < count; index++) {
      uint32_t lane = lane_start + index;
      uint32_t memory_address =
          (address + index * stride) & UINT32_C(0x0fff);
      if (load) {
        uint8_t source = dmem[memory_address];
        uint16_t value = subopcode == 6u
                             ? (uint16_t)((uint16_t)source << 8)
                             : (uint16_t)(UINT16_C(0x4000) |
                                          ((uint16_t)source << 6));
        write_expected_half(vectors[vector_register], lane, value);
      } else {
        uint16_t value = read_vector_half(vectors[vector_register], lane);
        dmem[memory_address] = subopcode == 6u
                                   ? (uint8_t)(value >> 8)
                                   : (uint8_t)((value >> 7) & 0xffu);
      }
    }
  } else {
    uint32_t register_base = vector_register & UINT32_C(0x18);
    uint32_t rotation = element / 2u;
    uint32_t aligned = address & UINT32_C(0x0ff0);

    for (index = 0u; index < 8u; index++) {
      uint32_t vector = register_base | ((index + rotation) & 7u);
      uint32_t lane = subopcode == 10u
                          ? (index + rotation) & 7u
                          : index;
      uint32_t memory_address =
          (aligned + index * 2u) & UINT32_C(0x0fff);
      if (load) {
        uint16_t value =
            (uint16_t)(((uint16_t)dmem[memory_address] << 8) |
                       dmem[(memory_address + 1u) & UINT32_C(0x0fff)]);
        write_expected_half(vectors[vector], lane, value);
      } else {
        uint16_t value = read_vector_half(vectors[vector], lane);
        dmem[memory_address] = (uint8_t)(value >> 8);
        dmem[(memory_address + 1u) & UINT32_C(0x0fff)] = (uint8_t)value;
      }
    }
  }
}

static bool transfer_case(rsp_transfer_fixture *fixture, bool load,
                          uint32_t subopcode, uint32_t element,
                          uint32_t base, int32_t encoded_offset) {
  t64_machine *machine;
  rsp_transfer_observation observed;
  uint8_t expected_vectors[32][16];
  uint8_t expected_dmem[4096];
  uint64_t expected_accumulator[8];
  uint16_t expected_vco;
  uint16_t expected_vcc;
  uint8_t expected_vce;
  uint32_t instruction;
  uint32_t scale = scale_for_subopcode(subopcode);
  uint32_t effective_address;
  bool valid = legal_element(load, subopcode, element);

  if (!valid && subopcode >= 12u) {
    valid = false;
  }
  instruction = encode_i(load ? UINT32_C(0x32) : UINT32_C(0x3a), 1u, 3u,
                         subopcode, element, encoded_offset);
  if (!prepare_instruction(fixture, instruction, base)) {
    fprintf(stderr, "transfer setup failed: op=%s sub=%u e=%u\n",
            load ? "load" : "store", subopcode, element);
    return false;
  }
  machine = fixture->machine;
  (void)memcpy(expected_vectors, machine->rsp.vector,
               sizeof expected_vectors);
  (void)memcpy(expected_dmem, machine->sp_dmem, sizeof expected_dmem);
  (void)memcpy(expected_accumulator, machine->rsp.accumulator,
               sizeof expected_accumulator);
  expected_vco = machine->rsp.vco;
  expected_vcc = machine->rsp.vcc;
  expected_vce = machine->rsp.vce;
  effective_address =
      (base + ((uint32_t)encoded_offset << scale)) & UINT32_C(0x0fff);
  if (valid) {
    expected_direct_transfer(load, subopcode, element, effective_address,
                             expected_vectors, expected_dmem);
  }
  if (!execute_prepared(fixture, &observed)) {
    fprintf(stderr, "transfer execute failed: op=%s sub=%u e=%u\n",
            load ? "load" : "store", subopcode, element);
    return false;
  }
  if (valid) {
    if (observed.outcome != T64_RSP_ATTEMPT_RETIRED || observed.pc != 4u ||
        observed.retired != UINT64_C(1) ||
        memcmp(expected_vectors, machine->rsp.vector,
               sizeof expected_vectors) != 0 ||
        memcmp(expected_dmem, machine->sp_dmem, sizeof expected_dmem) != 0 ||
        memcmp(expected_accumulator, machine->rsp.accumulator,
               sizeof expected_accumulator) != 0 ||
        machine->rsp.vco != expected_vco || machine->rsp.vcc != expected_vcc ||
        machine->rsp.vce != expected_vce) {
      fprintf(stderr,
              "transfer byte/lane assertion failed: op=%s sub=%u e=%u "
              "base=0x%03x offset=%d actual=(outcome=%u reason=%u pc=0x%03x)\n",
              load ? "load" : "store", subopcode, element, base,
              encoded_offset, (unsigned)observed.outcome,
              (unsigned)observed.refusal_reason, (unsigned)observed.pc);
      return false;
    }
  } else if (observed.outcome != T64_RSP_ATTEMPT_REFUSED ||
             observed.refusal_reason != T64_RSP_REFUSAL_UNSUPPORTED_OPCODE ||
             observed.pc != 0u || observed.retired != 0u ||
             memcmp(expected_vectors, machine->rsp.vector,
                    sizeof expected_vectors) != 0 ||
             memcmp(expected_dmem, machine->sp_dmem, sizeof expected_dmem) !=
                 0 ||
             memcmp(expected_accumulator, machine->rsp.accumulator,
                    sizeof expected_accumulator) != 0 ||
             machine->rsp.vco != expected_vco ||
             machine->rsp.vcc != expected_vcc ||
             machine->rsp.vce != expected_vce) {
    fprintf(stderr,
            "transfer refusal must be transactional: op=%s sub=%u e=%u "
            "actual=(outcome=%u reason=%u pc=0x%03x retired=%llu)\n",
            load ? "load" : "store", subopcode, element,
            (unsigned)observed.outcome, (unsigned)observed.refusal_reason,
            (unsigned)observed.pc, (unsigned long long)observed.retired);
    return false;
  }
  return true;
}

static bool cop2_move_matrix_test(rsp_transfer_fixture *fixture) {
  uint32_t element;
  uint32_t ignored_element;
  bool passed = true;

  for (element = 0u; element <= 14u; element += 2u) {
    uint32_t instruction =
        encode_cop2(0u, 4u, 3u, element << 7);
    rsp_transfer_observation observed;
    uint16_t value;
    uint32_t expected;

    if (!run_instruction(fixture, instruction, UINT32_C(0x100), &observed)) {
      return false;
    }
    value = (uint16_t)(((uint16_t)(3u * 43u + element * 17u + 3u) << 8) |
                       (uint8_t)(3u * 43u + (element + 1u) * 17u + 3u));
    expected = (value & UINT16_C(0x8000)) != 0u
                   ? UINT32_C(0xffff0000) | value
                   : value;
    if (observed.outcome != T64_RSP_ATTEMPT_RETIRED || observed.pc != 4u ||
        observed.gpr[4] != expected) {
      fprintf(stderr, "MFC2 e=%u expected r4=0x%08x got 0x%08x\n", element,
              expected, observed.gpr[4]);
      passed = false;
    }
  }

  for (element = 0u; element <= 14u; element += 2u) {
    uint32_t instruction = encode_cop2(4u, 2u, 3u, element << 7);
    rsp_transfer_observation observed;
    uint8_t expected_vector[16];
    uint32_t byte;

    if (!prepare_instruction(fixture, instruction, UINT32_C(0x100))) {
      return false;
    }
    (void)memcpy(expected_vector, fixture->machine->rsp.vector[3],
                 sizeof expected_vector);
    fixture->machine->rsp.gpr[2] = UINT32_C(0x89abcdef);
    expected_vector[element] = UINT8_C(0xcd);
    expected_vector[element + 1u] = UINT8_C(0xef);
    if (!execute_prepared(fixture, &observed)) {
      return false;
    }
    if (observed.outcome != T64_RSP_ATTEMPT_RETIRED || observed.pc != 4u ||
        memcmp(expected_vector, fixture->machine->rsp.vector[3],
               sizeof expected_vector) != 0) {
      fprintf(stderr, "MTC2 e=%u did not preserve adjacent vector bytes\n",
              element);
      passed = false;
    }
    for (byte = 0u; byte < 16u; byte++) {
      if (byte != element && byte != element + 1u &&
          fixture->machine->rsp.vector[3][byte] != expected_vector[byte]) {
        passed = false;
      }
    }
  }

  for (element = 0u; element < 3u; element++) {
    for (ignored_element = 0u; ignored_element < 16u; ignored_element++) {
      uint32_t instruction =
          encode_cop2(2u, 4u, element, ignored_element << 7);
      rsp_transfer_observation observed;
      uint32_t expected = element == 0u ? UINT32_C(0x935a) :
                          element == 1u ? UINT32_C(0x6ac3) : UINT32_C(0xd7);

      if (!run_instruction(fixture, instruction, UINT32_C(0x100), &observed)) {
        return false;
      }
      if (observed.outcome != T64_RSP_ATTEMPT_RETIRED ||
          observed.gpr[4] != expected) {
        fprintf(stderr, "CFC2 rd=%u e=%u expected 0x%08x got 0x%08x\n",
                element, ignored_element, expected, observed.gpr[4]);
        passed = false;
      }
    }
  }
  for (element = 0u; element < 3u; element++) {
    for (ignored_element = 0u; ignored_element < 16u; ignored_element++) {
      uint32_t instruction =
          encode_cop2(6u, 2u, element, ignored_element << 7);
      rsp_transfer_observation observed;
      uint16_t expected_vco = element == 0u ? UINT16_C(0x96e1)
                                            : UINT16_C(0x935a);
      uint16_t expected_vcc = element == 1u ? UINT16_C(0x96e1)
                                            : UINT16_C(0x6ac3);
      uint8_t expected_vce = element == 2u ? UINT8_C(0xe1)
                                           : UINT8_C(0xd7);

      if (!prepare_instruction(fixture, instruction, UINT32_C(0x100))) {
        return false;
      }
      fixture->machine->rsp.gpr[2] = UINT32_C(0xa5c396e1);
      if (!execute_prepared(fixture, &observed) ||
          observed.outcome != T64_RSP_ATTEMPT_RETIRED) {
        fprintf(stderr, "CTC2 rd=%u e=%u failed to retire\n", element,
                ignored_element);
        passed = false;
      } else if (fixture->machine->rsp.vco != expected_vco ||
                 fixture->machine->rsp.vcc != expected_vcc ||
                 fixture->machine->rsp.vce != expected_vce) {
        fprintf(stderr, "CTC2 rd=%u e=%u flag mapping mismatch\n", element,
                ignored_element);
        passed = false;
      }
    }
  }
  return passed;
}

static bool expect_cop2_refused(rsp_transfer_fixture *fixture,
                                const char *name, uint32_t instruction) {
  t64_machine *machine;
  rsp_transfer_observation observed;
  uint8_t vectors[32][16];
  uint8_t dmem[4096];
  uint64_t accumulator[8];
  uint32_t gpr[32];
  uint16_t vco;
  uint16_t vcc;
  uint8_t vce;

  if (!prepare_instruction(fixture, instruction, UINT32_C(0x100))) {
    return false;
  }
  machine = fixture->machine;
  (void)memcpy(vectors, machine->rsp.vector, sizeof vectors);
  (void)memcpy(dmem, machine->sp_dmem, sizeof dmem);
  (void)memcpy(accumulator, machine->rsp.accumulator, sizeof accumulator);
  (void)memcpy(gpr, machine->rsp.gpr, sizeof gpr);
  vco = machine->rsp.vco;
  vcc = machine->rsp.vcc;
  vce = machine->rsp.vce;
  if (!execute_prepared(fixture, &observed)) {
    return false;
  }
  if (observed.outcome != T64_RSP_ATTEMPT_REFUSED ||
      observed.refusal_reason != T64_RSP_REFUSAL_UNSUPPORTED_OPCODE ||
      observed.pc != 0u || observed.retired != 0u ||
      memcmp(vectors, machine->rsp.vector, sizeof vectors) != 0 ||
      memcmp(dmem, machine->sp_dmem, sizeof dmem) != 0 ||
      memcmp(accumulator, machine->rsp.accumulator, sizeof accumulator) != 0 ||
      memcmp(gpr, machine->rsp.gpr, sizeof gpr) != 0 || machine->rsp.vco != vco ||
      machine->rsp.vcc != vcc || machine->rsp.vce != vce) {
    fprintf(stderr,
            "%s must refuse before effects: opcode=0x%08x outcome=%u "
            "reason=%u pc=0x%03x\n",
            name, instruction, (unsigned)observed.outcome,
            (unsigned)observed.refusal_reason, (unsigned)observed.pc);
    return false;
  }
  return true;
}

static bool cop2_reserved_refusal_test(rsp_transfer_fixture *fixture) {
  const struct {
    const char *name;
    uint32_t instruction;
  } cases[] = {
      {"MFC2 odd element", encode_cop2(0u, 4u, 3u, UINT32_C(1) << 7)},
      {"MTC2 odd element", encode_cop2(4u, 2u, 3u, UINT32_C(1) << 7)},
      {"MFC2 reserved low bit", encode_cop2(0u, 4u, 3u, 1u)},
      {"MTC2 reserved low bit", encode_cop2(4u, 2u, 3u, 1u)},
      {"CFC2 reserved register", encode_cop2(2u, 4u, 3u, 0u)},
      {"CTC2 reserved register", encode_cop2(6u, 2u, 3u, 0u)},
      {"CFC2 reserved low bit", encode_cop2(2u, 4u, 0u, 1u)},
      {"CTC2 reserved low bit", encode_cop2(6u, 2u, 0u, 1u)}
  };
  size_t index;

  for (index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
    if (!expect_cop2_refused(fixture, cases[index].name,
                             cases[index].instruction)) {
      return false;
    }
  }
  return true;
}

static bool vmov_matrix_test(rsp_transfer_fixture *fixture) {
  uint32_t element;
  uint32_t destination_lane;
  bool passed = true;

  for (element = 0u; element < 16u; element++) {
    for (destination_lane = 0u; destination_lane < 8u;
         destination_lane++) {
      uint32_t instruction = encode_cop2(
          UINT32_C(0x10) | element, 3u, destination_lane,
          (UINT32_C(4) << 6) | UINT32_C(0x33));
      rsp_transfer_observation observed;
      uint8_t expected_vector[16];
      uint64_t expected_accumulator[8];
      uint32_t source_lane;
      uint32_t source_byte;
      uint16_t expected_value;
      bool valid = element != 1u;

      /* These fixed raw words pin the four-bit e and five-bit vs/de fields.
       * In particular, e=8 and e=15 are whole-vector broadcast selectors,
       * while vs/de independently selects the one destination lane. */
      if ((element == 8u && destination_lane == 2u &&
           instruction != UINT32_C(0x4b031133)) ||
          (element == 15u && destination_lane == 5u &&
           instruction != UINT32_C(0x4be32933))) {
        fprintf(stderr,
                "VMOV raw field encoding mismatch: e=%u de=%u word=0x%08x\n",
                element, destination_lane, instruction);
        return false;
      }

      if (!prepare_instruction(fixture, instruction, UINT32_C(0x100))) {
        return false;
      }
      (void)memcpy(expected_vector, fixture->machine->rsp.vector[4],
                   sizeof expected_vector);
      (void)memcpy(expected_accumulator, fixture->machine->rsp.accumulator,
                   sizeof expected_accumulator);
      if (element == 0u) {
        source_lane = destination_lane;
      } else if ((element & UINT32_C(0x0e)) == 2u) {
        source_lane = (element & UINT32_C(1)) +
                      (destination_lane & UINT32_C(6));
      } else if ((element & UINT32_C(0x0c)) == 4u) {
        source_lane = (element & UINT32_C(3)) +
                      (destination_lane & UINT32_C(4));
      } else {
        source_lane = element & UINT32_C(7);
      }
      if ((element == 8u && destination_lane == 2u && source_lane != 0u) ||
          (element == 15u && destination_lane == 5u && source_lane != 7u)) {
        fprintf(stderr,
                "VMOV whole-vector raw selector mismatch: e=%u de=%u lane=%u\n",
                element, destination_lane, source_lane);
        return false;
      }
      source_byte = source_lane * 2u;
      expected_value = (uint16_t)(
          ((uint16_t)fixture->machine->rsp.vector[3][source_byte] << 8) |
          fixture->machine->rsp.vector[3][source_byte + 1u]);
      if (valid) {
        expected_vector[destination_lane * 2u] =
            (uint8_t)(expected_value >> 8);
        expected_vector[destination_lane * 2u + 1u] =
            (uint8_t)expected_value;
        expected_accumulator[destination_lane] =
            (expected_accumulator[destination_lane] & ~UINT64_C(0xffff)) |
            expected_value;
      }
      if (!execute_prepared(fixture, &observed)) {
        return false;
      }
      if (valid) {
        if (observed.outcome != T64_RSP_ATTEMPT_RETIRED ||
            memcmp(expected_vector, fixture->machine->rsp.vector[4],
                   sizeof expected_vector) != 0 ||
            memcmp(expected_accumulator, fixture->machine->rsp.accumulator,
                   sizeof expected_accumulator) != 0) {
          fprintf(stderr, "VMOV e=%u de=%u lane/accumulator mismatch\n",
                  element, destination_lane);
          passed = false;
        }
      } else if (observed.outcome != T64_RSP_ATTEMPT_REFUSED ||
                 observed.refusal_reason !=
                     T64_RSP_REFUSAL_UNSUPPORTED_OPCODE ||
                 observed.pc != 0u || observed.retired != 0u ||
                 memcmp(expected_vector, fixture->machine->rsp.vector[4],
                        sizeof expected_vector) != 0 ||
                 memcmp(expected_accumulator,
                        fixture->machine->rsp.accumulator,
                        sizeof expected_accumulator) != 0) {
        fprintf(stderr,
                "VMOV invalid e=%u de=%u must refuse before effects\n",
                element, destination_lane);
        passed = false;
      }
    }
  }

  for (element = 8u; element < 32u; element++) {
    uint32_t instruction = encode_cop2(
        UINT32_C(0x10), 3u, element,
        (UINT32_C(4) << 6) | UINT32_C(0x33));
    if (!expect_cop2_refused(fixture, "VMOV reserved destination lane",
                             instruction)) {
      return false;
    }
  }

  /* The destination and source may alias; the source is snapshotted first. */
  {
    uint32_t instruction = encode_cop2(0x18u, 3u, 6u,
                                       (UINT32_C(3) << 6) | UINT32_C(0x33));
    rsp_transfer_observation observed;
    uint8_t expected[16];
    uint16_t source;
    if (!prepare_instruction(fixture, instruction, UINT32_C(0x100))) {
      return false;
    }
    (void)memcpy(expected, fixture->machine->rsp.vector[3], sizeof expected);
    source = read_vector_half(expected, 0u);
    write_expected_half(expected, 6u, source);
    if (!execute_prepared(fixture, &observed)) {
      fprintf(stderr, "VMOV self-alias execute failed\n");
      return false;
    }
    if (observed.outcome != T64_RSP_ATTEMPT_RETIRED ||
        memcmp(expected, fixture->machine->rsp.vector[3], sizeof expected) !=
            0) {
      fprintf(stderr,
              "VMOV self-alias failed: outcome=%u reason=%u pc=0x%03x "
              "expected-lane6=0x%04x actual-lane6=0x%04x\n",
              (unsigned)observed.outcome,
              (unsigned)observed.refusal_reason, (unsigned)observed.pc,
              read_vector_half(expected, 6u),
              read_vector_half(fixture->machine->rsp.vector[3], 6u));
      passed = false;
    }
  }
  return passed;
}

static bool vector_memory_matrix_test(rsp_transfer_fixture *fixture) {
  uint32_t direction;
  uint32_t subopcode;
  uint32_t element;
  uint32_t alignment;
  bool passed = true;

  for (direction = 0u; direction < 2u; direction++) {
    bool load = direction == 0u;
    uint32_t maximum_subopcode = 11u;

    for (subopcode = 0u; subopcode <= maximum_subopcode; subopcode++) {
      for (element = 0u; element < 16u; element++) {
        for (alignment = 0u; alignment < 16u; alignment++) {
          if (!transfer_case(fixture, load, subopcode, element,
                             UINT32_C(0x0ff0) + alignment, 0)) {
            return false;
          }
        }
      }
    }
  }

  /* A negative signed seven-bit offset must wrap each byte through DMEM. */
  for (direction = 0u; direction < 2u; direction++) {
    bool load = direction == 0u;
    for (subopcode = 0u; subopcode < 12u; subopcode++) {
      for (element = 0u; element < 16u; element++) {
        if (!transfer_case(fixture, load, subopcode, element,
                           UINT32_C(4), -1)) {
          return false;
        }
      }
    }
  }
  for (direction = 0u; direction < 2u; direction++) {
    bool load = direction == 0u;
    for (subopcode = 12u; subopcode < 32u; subopcode++) {
      if (!transfer_case(fixture, load, subopcode, 0u,
                         UINT32_C(0x0ff0), 0)) {
        return false;
      }
    }
  }
  return passed;
}

static bool cop2_transfer_matrix_test(rsp_transfer_fixture *fixture) {
  bool passed = true;

  passed = expect_retired(fixture, "MFC2 element 0",
                          encode_cop2(0u, 2u, 3u, 0u)) &&
           passed;
  passed = expect_retired(fixture, "MTC2 element 0",
                          encode_cop2(4u, 2u, 3u, 0u)) &&
           passed;
  passed = expect_retired(fixture, "CFC2 VCO", encode_cop2(2u, 2u, 0u, 0u)) &&
           passed;
  passed = expect_retired(fixture, "CTC2 VCO", encode_cop2(6u, 2u, 0u, 0u)) &&
           passed;
  passed = expect_retired(fixture, "VMOV element 0",
                          encode_cop2(16u, 3u, 0u,
                                      (UINT32_C(4) << 6) | UINT32_C(0x33))) &&
           passed;
  passed = expect_retired(fixture, "LBV element 0",
                          encode_i(UINT32_C(0x32), 1u, 3u, 0u, 0u, 0)) &&
           passed;
  passed = expect_retired(fixture, "SBV element 0",
                          encode_i(UINT32_C(0x3a), 1u, 3u, 0u, 0u, 0)) &&
           passed;
  passed = cop2_move_matrix_test(fixture) && passed;
  passed = cop2_reserved_refusal_test(fixture) && passed;
  passed = vmov_matrix_test(fixture) && passed;
  passed = vector_memory_matrix_test(fixture) && passed;
  return passed;
}

int main(void) {
  rsp_transfer_fixture fixture;
  bool passed = fixture_create(&fixture);

  if (!passed) {
    fprintf(stderr, "fixture setup failure\n");
    fixture_destroy(&fixture);
    return 1;
  }
  passed = cop2_transfer_matrix_test(&fixture);
  fixture_destroy(&fixture);
  if (!passed) {
    return 1;
  }
  puts("RSP fetched COP2 transfer and vector memory matrix PASS");
  return 0;
}
