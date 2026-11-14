#include "t64_internal.h"
#include "t64_rsp_divider.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  t64_host_allocator allocator;
  t64_machine *machine;
} rsp_divider_fixture;

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

static bool fixture_create(rsp_divider_fixture *fixture) {
  const t64_profile_descriptor *profile = NULL;
  t64_host_storage image = {NULL, 0u, 0u};
  t64_status status;

  (void)memset(fixture, 0, sizeof *fixture);
  fixture->allocator.allocate = fixture_allocate;
  fixture->allocator.release = fixture_release;
  status = t64_profile_lookup(
      "ntsc-cart-4m-openipl3-rsp-functional-v1", &profile);
  if (status != T64_STATUS_OK || profile == NULL ||
      t64_machine_create(&fixture->allocator, profile, &fixture->machine) !=
          T64_STATUS_OK ||
      fixture->machine == NULL || !fixture_allocate(fixture, 2u, 1u, &image)) {
    return false;
  }
  ((uint8_t *)image.data)[0] = UINT8_C(0x12);
  ((uint8_t *)image.data)[1] = UINT8_C(0x34);
  fixture->machine->image_storage = image;
  fixture->machine->image_length = 2u;
  fixture->machine->has_image = true;
  return true;
}

static void fixture_destroy(rsp_divider_fixture *fixture) {
  if (fixture != NULL && fixture->machine != NULL) {
    t64_machine_destroy(fixture->machine);
    fixture->machine = NULL;
  }
}

static uint32_t encode_vector(uint32_t element, uint32_t vt, uint32_t de,
                              uint32_t vd, uint32_t function) {
  return (UINT32_C(0x12) << 26) | (UINT32_C(0x10) << 21) |
         ((element & UINT32_C(0x0f)) << 21) | (vt << 16) | (de << 11) |
         (vd << 6) | function;
}

static void write_lane(uint8_t vector[16], uint32_t lane, uint16_t value) {
  vector[lane * 2u] = (uint8_t)(value >> 8);
  vector[lane * 2u + 1u] = (uint8_t)value;
}

static uint16_t read_lane(const uint8_t vector[16], uint32_t lane) {
  return (uint16_t)(((uint16_t)vector[lane * 2u] << 8) |
                    vector[lane * 2u + 1u]);
}

static void write_instruction(t64_machine *machine, uint32_t instruction) {
  uint32_t pc = machine->rsp.pc & UINT32_C(0x0fff);
  machine->sp_imem[pc] = (uint8_t)(instruction >> 24);
  machine->sp_imem[pc + 1u] = (uint8_t)(instruction >> 16);
  machine->sp_imem[pc + 2u] = (uint8_t)(instruction >> 8);
  machine->sp_imem[pc + 3u] = (uint8_t)instruction;
}

static bool execute_fetched(t64_machine *machine, uint32_t instruction) {
  uint32_t attempt;

  write_instruction(machine, instruction);
  if (machine->tick == UINT64_C(0) &&
      machine->rsp.issue_sequence == UINT64_C(0) &&
      t64_rsp_start(machine) != T64_STATUS_OK) {
    return false;
  }
  for (attempt = 0u; attempt < 8u; attempt++) {
    t64_stop_request request = {T64_STOP_GUEST_TICK,
                                machine->tick + UINT64_C(1)};
    t64_stop_result result = {0u, 0u};

    if (t64_machine_advance(machine, &request, &result) != T64_STATUS_OK) {
      return false;
    }
    if (machine->rsp_observed_outcome == T64_RSP_ATTEMPT_RETIRED &&
        machine->rsp_observed_opcode == instruction) {
      return true;
    }
    if (machine->rsp_observed_outcome == T64_RSP_ATTEMPT_REFUSED) {
      (void)fprintf(stderr,
                    "FAIL: fetched word=%08x outcome=%u refusal=%u pc=%03x "
                    "tick=%llu\n",
                    instruction, (unsigned)machine->rsp_observed_outcome,
                    (unsigned)machine->rsp_observed_refusal_reason,
                    machine->rsp.pc,
                    (unsigned long long)machine->tick);
      return false;
    }
  }
  (void)fprintf(stderr,
                "FAIL: fetched word=%08x did not retire within 8 ticks; "
                "outcome=%u pc=%03x observed=%08x stall=%u ready=%llu "
                "tick=%llu\n",
                instruction, (unsigned)machine->rsp_observed_outcome,
                machine->rsp.pc, machine->rsp_observed_opcode,
                machine->rsp_observed_stall_reason,
                (unsigned long long)machine->rsp_observed_ready_tick,
                (unsigned long long)machine->tick);
  return false;
}

static bool integer_table_bounds(void) {
  const uint64_t reciprocal_numerator = UINT64_C(1) << 34;
  const uint64_t square_limit = UINT64_C(1) << 44;
  uint32_t index;

  for (index = 0u; index < UINT32_C(512); index++) {
    uint16_t reciprocal = t64_rsp_divider_reciprocal_word(index);
    uint16_t rsqrt = t64_rsp_divider_rsqrt_word(index);
    uint32_t reciprocal_matches = 0u;
    uint32_t rsqrt_matches = 0u;
    uint32_t wrap;

    if (index == 0u) {
      if (reciprocal != UINT16_MAX) {
        (void)fprintf(stderr, "FAIL: reciprocal index 0 special word\n");
        return false;
      }
    } else {
      uint64_t denominator = (uint64_t)index + UINT64_C(512);
      for (wrap = 0u; wrap < 3u; wrap++) {
        uint64_t candidate = (uint64_t)reciprocal +
                             ((uint64_t)wrap << 16);
        /* Cross multiplication checks the rounded floor interval without
         * repeating production's division or shift expression. */
        bool lower = candidate == 0u ||
            (UINT64_C(256) * candidate - UINT64_C(1)) * denominator <=
                reciprocal_numerator;
        uint64_t upper =
            (UINT64_C(256) * (candidate + UINT64_C(1)) - UINT64_C(1)) *
            denominator;
        if (lower && reciprocal_numerator < upper) {
          reciprocal_matches++;
        }
      }
      if (reciprocal_matches != 1u) {
        (void)fprintf(stderr,
                      "FAIL: reciprocal index %u rational floor bound\n",
                      index);
        return false;
      }
    }

    {
      uint64_t a = ((uint64_t)index + UINT64_C(512)) >> (index & 1u);
      for (wrap = 0u; wrap < 3u; wrap++) {
        uint64_t half = (uint64_t)rsqrt + ((uint64_t)wrap << 16);
        uint32_t parity;
        for (parity = 0u; parity < 2u; parity++) {
          uint64_t b = (half << 1) + (uint64_t)parity;
          if (a * b * b < square_limit &&
              a * (b + UINT64_C(1)) * (b + UINT64_C(1)) >= square_limit) {
            rsqrt_matches++;
          }
        }
      }
    }
    if (rsqrt_matches != 1u) {
      (void)fprintf(stderr,
                    "FAIL: rsqrt index %u strict square bracket\n", index);
      return false;
    }
  }
  return true;
}

static bool divider_result_anchor(bool inverse_square_root, uint32_t input,
                                  uint32_t expected) {
  uint32_t actual = t64_rsp_divider_result(inverse_square_root, input);
  if (actual != expected) {
    (void)fprintf(stderr,
                  "FAIL: divider anchor rsqrt=%u input=%08x expected=%08x "
                  "actual=%08x\n",
                  inverse_square_root ? 1u : 0u, input, expected, actual);
    return false;
  }
  return true;
}

static bool integer_vectors(void) {
  static const struct {
    uint32_t index;
    uint16_t reciprocal;
    uint16_t rsqrt;
  } table_anchors[] = {
      {0u, 0xffffu, 0x6a09u}, {1u, 0xff00u, 0xffffu},
      {2u, 0xfe01u, 0x6955u}, {255u, 0x55c7u, 0xa297u},
      {256u, 0x5555u, 0x279au}, {510u, 0x0080u, 0x0040u},
      {511u, 0x0040u, 0x6a64u}};
  uint32_t index;

  if (!integer_table_bounds()) {
    return false;
  }
  for (index = 0u;
       index < (uint32_t)(sizeof table_anchors / sizeof table_anchors[0]);
       index++) {
    const uint32_t table_index = table_anchors[index].index;
    if (t64_rsp_divider_reciprocal_word(table_index) !=
            table_anchors[index].reciprocal ||
        t64_rsp_divider_rsqrt_word(table_index) !=
            table_anchors[index].rsqrt) {
      (void)fprintf(stderr, "FAIL: divider table anchor index %u\n",
                    table_index);
      return false;
    }
  }
  return divider_result_anchor(false, UINT32_C(0), UINT32_C(0x7fffffff)) &&
         divider_result_anchor(true, UINT32_C(0), UINT32_C(0x7fffffff)) &&
         divider_result_anchor(false, UINT32_C(0x00004000),
                               UINT32_C(0x0001ffff)) &&
         divider_result_anchor(true, UINT32_C(0x00004000),
                               UINT32_C(0x00ffff80)) &&
         divider_result_anchor(false, UINT32_C(0x00008000),
                               UINT32_C(0x0000ffff)) &&
         divider_result_anchor(true, UINT32_C(0x00008000),
                               UINT32_C(0x00b50480)) &&
         divider_result_anchor(false, UINT32_C(0xffff8000),
                               UINT32_C(0xffff0000)) &&
         divider_result_anchor(true, UINT32_C(0xffff8000),
                               UINT32_C(0xffff0000)) &&
         divider_result_anchor(false, UINT32_C(0x80000000),
                               UINT32_C(0xfffffffe)) &&
         divider_result_anchor(true, UINT32_C(0x80000000),
                               UINT32_C(0xffff4acd)) &&
         divider_result_anchor(false, UINT32_MAX, UINT32_C(0x80003fff)) &&
         divider_result_anchor(true, UINT32_MAX, UINT32_C(0x80003fff));
}

static void prepare_machine(t64_machine *machine) {
  uint32_t reg;
  t64_machine_reset_execution_state(machine);
  (void)memset(&machine->rsp, 0, sizeof machine->rsp);
  (void)memset(&machine->cpu, 0, sizeof machine->cpu);
  (void)memset(machine->sp_dmem, 0, sizeof machine->sp_dmem);
  (void)memset(machine->sp_imem, 0, sizeof machine->sp_imem);
  for (reg = 0u; reg < 32u; reg++) {
    uint32_t byte;
    for (byte = 0u; byte < 16u; byte++) {
      machine->rsp.vector[reg][byte] =
          (uint8_t)(reg * 13u + byte * 7u + 3u);
    }
  }
  for (reg = 0u; reg < 8u; reg++) {
    machine->rsp.accumulator[reg] = UINT64_C(0x123456789abc) + reg;
  }
  machine->rsp.sp_status = UINT32_C(1);
}

static bool fetched_vrcph_returns_previous_high_and_latches_input(
    rsp_divider_fixture *fixture) {
  const uint32_t instruction =
      encode_vector(0u, 2u, 1u, 3u, UINT32_C(0x32));
  t64_machine *machine = fixture->machine;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(1)};
  t64_stop_result result = {0u, 0u};
  uint64_t accumulator_before[8];
  uint32_t lane;

  prepare_machine(machine);
  write_lane(machine->rsp.vector[2], 0u, UINT16_C(0x1234));
  write_lane(machine->rsp.vector[3], 1u, UINT16_C(0xfeed));
  machine->rsp.divider_input = UINT32_C(0x5678);
  machine->rsp.divider_result = UINT32_C(0x89abcdef);
  machine->rsp.divider_high_valid = false;
  (void)memcpy(accumulator_before, machine->rsp.accumulator,
               sizeof accumulator_before);
  if (instruction != UINT32_C(0x4a0208f2)) {
    (void)fprintf(stderr, "SETUP: VRCPH raw-word anchor changed\n");
    return false;
  }
  machine->sp_imem[0] = (uint8_t)(instruction >> 24);
  machine->sp_imem[1] = (uint8_t)(instruction >> 16);
  machine->sp_imem[2] = (uint8_t)(instruction >> 8);
  machine->sp_imem[3] = (uint8_t)instruction;
  if (t64_rsp_start(machine) != T64_STATUS_OK ||
      t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
      result.tick != UINT64_C(1)) {
    (void)fprintf(stderr, "SETUP: VRCPH fetched execution did not advance\n");
    return false;
  }
  if (machine->rsp_observed_outcome != T64_RSP_ATTEMPT_RETIRED ||
      machine->rsp_observed_opcode != instruction ||
      machine->rsp.pc != UINT32_C(4) ||
      machine->rsp.retired_instructions != UINT64_C(1)) {
    (void)fprintf(
        stderr,
        "FAIL: fetched VRCPH word=%08x pc=%03x expected=retired/pc004 "
        "actual=outcome%u/refusal%u/pc%03x retired=%llu\n",
        instruction, machine->rsp_observed_pc,
        (unsigned)machine->rsp_observed_outcome,
        (unsigned)machine->rsp_observed_refusal_reason,
        (unsigned)machine->rsp.pc,
        (unsigned long long)machine->rsp.retired_instructions);
    return false;
  }
  if (read_lane(machine->rsp.vector[3], 1u) != UINT16_C(0x89ab) ||
      machine->rsp.divider_input != UINT32_C(0x1234) ||
      machine->rsp.divider_result != UINT32_C(0x89abcdef) ||
      !machine->rsp.divider_high_valid) {
    (void)fprintf(stderr,
                  "FAIL: VRCPH destination or divider latch mismatch\n");
    return false;
  }
  for (lane = 0u; lane < 8u; lane++) {
    if ((machine->rsp.accumulator[lane] & UINT64_C(0xffff)) !=
            UINT64_C(0x1234) ||
        (machine->rsp.accumulator[lane] & ~UINT64_C(0xffff)) !=
            (accumulator_before[lane] & ~UINT64_C(0xffff))) {
      (void)fprintf(stderr, "FAIL: VRCPH ACC low slice lane %u mismatch\n",
                    lane);
      return false;
    }
  }
  return true;
}

static bool accumulator_lows_equal(const t64_machine *machine,
                                   uint16_t expected) {
  uint32_t lane;
  for (lane = 0u; lane < 8u; lane++) {
    if ((uint16_t)machine->rsp.accumulator[lane] != expected) {
      (void)fprintf(stderr,
                    "FAIL: ACC low lane %u expected=%04x actual=%04llx\n",
                    lane, expected,
                    (unsigned long long)(machine->rsp.accumulator[lane] &
                                         UINT64_C(0xffff)));
      return false;
    }
  }
  return true;
}

static bool fetched_family_and_latch_sequences(
    rsp_divider_fixture *fixture) {
  t64_machine *machine = fixture->machine;
  uint32_t high;
  uint32_t low;

  prepare_machine(machine);
  machine->rsp.divider_input = UINT32_C(0x5555);
  machine->rsp.divider_result = UINT32_C(0x89abcdef);
  write_lane(machine->rsp.vector[2], 0u, UINT16_C(0x1111));
  write_lane(machine->rsp.vector[4], 0u, UINT16_C(0x2222));
  high = encode_vector(0u, 2u, 1u, 3u, UINT32_C(0x32));
  low = encode_vector(8u, 4u, 2u, 3u, UINT32_C(0x36));
  if (!execute_fetched(machine, high)) {
    (void)fprintf(stderr, "FAIL: first high instruction did not retire\n");
    return false;
  }
  if (machine->rsp.divider_input != UINT32_C(0x1111) ||
      !machine->rsp.divider_high_valid ||
      read_lane(machine->rsp.vector[3], 1u) != UINT16_C(0x89ab) ||
      !accumulator_lows_equal(machine, UINT16_C(0x1111))) {
    (void)fprintf(stderr, "FAIL: first high instruction state\n");
    return false;
  }
  if (!execute_fetched(machine, low)) {
    (void)fprintf(stderr, "FAIL: second high instruction did not retire\n");
    return false;
  }
  if (machine->rsp.divider_input != UINT32_C(0x2222) ||
      !machine->rsp.divider_high_valid ||
      machine->rsp.divider_result != UINT32_C(0x89abcdef) ||
      read_lane(machine->rsp.vector[3], 2u) != UINT16_C(0x89ab) ||
      !accumulator_lows_equal(machine, UINT16_C(0x2222))) {
    (void)fprintf(stderr, "FAIL: second high instruction state\n");
    return false;
  }

  prepare_machine(machine);
  machine->rsp.divider_input = UINT32_C(0x5555);
  machine->rsp.divider_result = UINT32_C(0xdeadbeef);
  machine->rsp.divider_high_valid = false;
  write_lane(machine->rsp.vector[2], 3u, UINT16_C(0x4000));
  low = encode_vector(3u, 2u, 2u, 3u, UINT32_C(0x31));
  if (!execute_fetched(machine, low) ||
      machine->rsp.divider_input != UINT32_C(0x5555) ||
      machine->rsp.divider_result != UINT32_C(0x0001ffff) ||
      machine->rsp.divider_high_valid ||
      read_lane(machine->rsp.vector[3], 2u) != UINT16_C(0xffff) ||
      !accumulator_lows_equal(machine, UINT16_C(0x4000))) {
    (void)fprintf(stderr, "FAIL: low reciprocal without a pending high half\n");
    return false;
  }

  prepare_machine(machine);
  machine->rsp.divider_result = UINT32_C(0x89abcdef);
  write_lane(machine->rsp.vector[2], 0u, UINT16_C(0x1234));
  write_lane(machine->rsp.vector[2], 1u, UINT16_C(0x5678));
  high = encode_vector(0u, 2u, 1u, 3u, UINT32_C(0x32));
  low = encode_vector(1u, 2u, 2u, 3u, UINT32_C(0x35));
  if (!execute_fetched(machine, high) ||
      !execute_fetched(machine, low) ||
      machine->rsp.divider_result != UINT32_C(0x0001e039) ||
      machine->rsp.divider_input != UINT32_C(0x1234) ||
      machine->rsp.divider_high_valid ||
      read_lane(machine->rsp.vector[3], 2u) != UINT16_C(0xe039) ||
      !accumulator_lows_equal(machine, UINT16_C(0x5678))) {
    (void)fprintf(stderr, "FAIL: reciprocal-high to rsqrt-low mixed sequence\n");
    return false;
  }

  prepare_machine(machine);
  write_lane(machine->rsp.vector[2], 3u, UINT16_C(0x4000));
  if (!execute_fetched(machine,
                       encode_vector(3u, 2u, 1u, 3u, UINT32_C(0x30))) ||
      machine->rsp.divider_result != UINT32_C(0x0001ffff) ||
      read_lane(machine->rsp.vector[3], 1u) != UINT16_C(0xffff) ||
      machine->rsp.divider_high_valid) {
    (void)fprintf(stderr, "FAIL: single reciprocal fetched result\n");
    return false;
  }

  prepare_machine(machine);
  write_lane(machine->rsp.vector[2], 3u, UINT16_C(0x4000));
  if (!execute_fetched(machine,
                       encode_vector(3u, 2u, 1u, 3u, UINT32_C(0x34))) ||
      machine->rsp.divider_result != UINT32_C(0x00ffff80) ||
      read_lane(machine->rsp.vector[3], 1u) != UINT16_C(0xff80) ||
      machine->rsp.divider_high_valid) {
    (void)fprintf(stderr, "FAIL: single rsqrt fetched result\n");
    return false;
  }
  return true;
}

static bool fetched_vmoves_and_independent_instances(
    rsp_divider_fixture *fixture) {
  rsp_divider_fixture peer;
  t64_machine *machine = fixture->machine;
  uint32_t vmov;
  uint32_t high;
  bool peer_created = fixture_create(&peer);

  if (!peer_created) {
    fixture_destroy(&peer);
    (void)fprintf(stderr, "SETUP: unable to create independent RSP fixture\n");
    return false;
  }

  prepare_machine(machine);
  write_lane(machine->rsp.vector[2], 0u, UINT16_C(0x1111));
  write_lane(machine->rsp.vector[2], 4u, UINT16_C(0x2222));
  vmov = encode_vector(8u, 2u, 4u, 2u, UINT32_C(0x33));
  high = encode_vector(4u, 2u, 1u, 3u, UINT32_C(0x32));
  if (!execute_fetched(machine, vmov) ||
      read_lane(machine->rsp.vector[2], 4u) != UINT16_C(0x1111) ||
      !execute_fetched(machine, high) ||
      machine->rsp.divider_input != UINT32_C(0x1111) ||
      !machine->rsp.divider_high_valid ||
      !accumulator_lows_equal(machine, UINT16_C(0x1111))) {
    fixture_destroy(&peer);
    (void)fprintf(stderr, "FAIL: same-register VMOV/divider interaction\n");
    return false;
  }

  prepare_machine(machine);
  prepare_machine(peer.machine);
  write_lane(machine->rsp.vector[2], 0u, UINT16_C(0x1234));
  write_lane(peer.machine->rsp.vector[2], 0u, UINT16_C(0x5678));
  high = encode_vector(0u, 2u, 1u, 3u, UINT32_C(0x32));
  if (!execute_fetched(machine, high) ||
      !execute_fetched(peer.machine, high) ||
      machine->rsp.divider_input != UINT32_C(0x1234) ||
      peer.machine->rsp.divider_input != UINT32_C(0x5678) ||
      !machine->rsp.divider_high_valid ||
      !peer.machine->rsp.divider_high_valid) {
    fixture_destroy(&peer);
    (void)fprintf(stderr, "FAIL: divider latch state leaked across machines\n");
    return false;
  }
  fixture_destroy(&peer);
  return true;
}

int main(int argc, char **argv) {
  rsp_divider_fixture fixture;
  bool passed;

  if (argc == 2 && strcmp(argv[1], "--integer-vectors") == 0) {
    if (!integer_vectors()) {
      return 1;
    }
    (void)puts("PASS: divider integer vectors and independent table bounds");
    return 0;
  }
  if (argc != 1) {
    (void)fprintf(stderr, "usage: test_rsp_divider [--integer-vectors]\n");
    return 2;
  }

  if (!fixture_create(&fixture)) {
    (void)fprintf(stderr, "SETUP: unable to create RSP divider fixture\n");
    fixture_destroy(&fixture);
    return 2;
  }
  passed = integer_vectors() &&
           fetched_vrcph_returns_previous_high_and_latches_input(&fixture) &&
           fetched_family_and_latch_sequences(&fixture) &&
           fetched_vmoves_and_independent_instances(&fixture);
  fixture_destroy(&fixture);
  if (!passed) {
    return 1;
  }
  (void)puts("PASS: fetched VRCPH returns previous high result and latches input");
  return 0;
}
