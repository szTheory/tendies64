#include "t64_internal.h"
#include "t64_test.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CP0_RANDOM UINT32_C(1)
#define CP0_ENTRYLO0 UINT32_C(2)
#define CP0_ENTRYLO1 UINT32_C(3)
#define CP0_WIRED UINT32_C(6)
#define CP0_COUNT UINT32_C(9)
#define CP0_ENTRYHI UINT32_C(10)
#define CP0_COMPARE UINT32_C(11)
#define CP0_STATUS UINT32_C(12)
#define CP0_CAUSE UINT32_C(13)
#define CP0_EPC UINT32_C(14)
#define STATUS_IE UINT32_C(0x00000001)
#define STATUS_EXL UINT32_C(0x00000002)
#define STATUS_ERL UINT32_C(0x00000004)
#define STATUS_IM0 UINT32_C(0x00000100)
#define STATUS_IM2 UINT32_C(0x00000400)
#define STATUS_IM7 UINT32_C(0x00008000)
#define CAUSE_IP0 UINT32_C(0x00000100)
#define CAUSE_IP2 UINT32_C(0x00000400)
#define CAUSE_IP7 UINT32_C(0x00008000)
#define T1_TEST_PI_BASE UINT32_C(0x04600000)
#define T1_TEST_CART_BASE UINT32_C(0x10000000)
#define T1_TEST_SP_BASE UINT32_C(0x04040000)
#define T1_TEST_SP_DMA_COMPLETE UINT32_C(5)
#define T1_TEST_RSP_STATUS_SINGLE_STEP UINT32_C(0x00000020)

static bool read_t1_ipl3_asset(uint8_t image[4096]) {
  const char *path = getenv("T64_TEST_IPL3_PATH");
  FILE *file;
  size_t count;
  int extra;
  int read_error;
  int close_error;

  if (path == NULL || path[0] == '\0') {
    return false;
  }
  file = fopen(path, "rb");
  if (file == NULL) {
    return false;
  }
  count = fread(image, 1u, 4096u, file);
  extra = fgetc(file);
  read_error = ferror(file);
  close_error = fclose(file);
  return count == 4096u && extra == EOF && read_error == 0 &&
         close_error == 0;
}

static void write_be32(uint8_t *bytes, uint32_t value);

static bool setup_machine(t64_machine *machine, bool with_ram) {
  const t64_profile_descriptor *profile = NULL;

  (void)memset(machine, 0, sizeof *machine);
  if (t64_profile_lookup("ntsc-cart-4m-openipl3-vr4300-v1", &profile) !=
          T64_STATUS_OK ||
      profile == NULL) {
    return false;
  }
  machine->profile = *profile;
  machine->has_image = true;
  machine->cpu.cp0[CP0_RANDOM] = UINT32_C(17);
  machine->cpu.count_compare_deadline = UINT64_MAX;
  if (with_ram) {
    machine->ram = (uint8_t *)calloc(machine->profile.rdram_bytes, 1u);
    if (machine->ram == NULL) {
      return false;
    }
  }
  return true;
}

static bool setup_t1_machine(t64_machine *machine, const char *profile_id) {
  const t64_profile_descriptor *profile = NULL;

  (void)memset(machine, 0, sizeof *machine);
  if (profile_id == NULL ||
      t64_profile_lookup(profile_id, &profile) != T64_STATUS_OK ||
      profile == NULL) {
    return false;
  }
  machine->profile = *profile;
  machine->has_image = true;
  machine->cpu.cp0[CP0_RANDOM] = UINT32_C(17);
  machine->cpu.count_compare_deadline = UINT64_MAX;
  machine->ram = (uint8_t *)calloc(machine->profile.rdram_bytes, 1u);
  return machine->ram != NULL;
}

static t64_status synchronize_to_tick(t64_machine *machine, uint64_t tick) {
  machine->tick = tick;
  return t64_cp0_synchronize_guest_time(machine, tick);
}

static void write_cp0(t64_machine *machine, uint32_t index, uint32_t value) {
  t64_cp0_effect effect = {0};
  uint32_t opcode = (UINT32_C(0x10) << 26) | (UINT32_C(4) << 21) |
                    (UINT32_C(8) << 16) | (index << 11);
  t64_cp0_transfer_result result;

  machine->cpu.gpr[8] = value;
  result = t64_cp0_prepare_transfer(machine, opcode, &effect);
  T64_CHECK_INT(result, T64_CP0_TRANSFER_OK);
  if (result == T64_CP0_TRANSFER_OK) {
    t64_cp0_commit_transfer(machine, &effect);
  }
}

static void destroy_machine(t64_machine *machine) {
  free(machine->ram);
  machine->ram = NULL;
}

static void reset_t1_cpu(t64_machine *machine, uint32_t opcode) {
  machine->tick = 0u;
  machine->queue_head = 0u;
  machine->queue_count = 0u;
  machine->next_sequence = 0u;
  machine->sequence_exhausted = false;
  machine->instruction_count = 0u;
  (void)memset(&machine->cpu, 0, sizeof machine->cpu);
  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = t64_cpu_canonical_address(machine->cpu.pc);
  machine->cpu.cp0[CP0_RANDOM] = UINT32_C(17);
  machine->cpu.count_compare_deadline = UINT64_MAX;
  write_be32(machine->ram + UINT32_C(0x1000), opcode);
}

static bool timer_state_equal(const t64_machine *left,
                              const t64_machine *right) {
  return left->tick == right->tick &&
         left->cpu.cp0[CP0_COUNT] == right->cpu.cp0[CP0_COUNT] &&
         left->cpu.cp0[CP0_COMPARE] == right->cpu.cp0[CP0_COMPARE] &&
         left->cpu.cp0[CP0_CAUSE] == right->cpu.cp0[CP0_CAUSE] &&
         left->cpu.count_sync_tick == right->cpu.count_sync_tick &&
         left->cpu.count_half_clock_phase ==
             right->cpu.count_half_clock_phase &&
         left->cpu.count_compare_deadline ==
             right->cpu.count_compare_deadline &&
         left->cpu.count_equality_latch_generation ==
             right->cpu.count_equality_latch_generation &&
         left->cpu.count_equality_latched ==
             right->cpu.count_equality_latched;
}

static void test_two_tick_boundaries_and_compare_clear(void) {
  t64_machine machine;

  T64_CHECK(setup_machine(&machine, false));
  if (machine.profile.machine_model_version != 3u) {
    return;
  }
  machine.cpu.cp0[CP0_COMPARE] = UINT32_C(2);

  /* The manual's half-PClock Count fact is cited in docs/spec/cpu-cp0.md;
     these exact endpoints use integer two-tick model-policy arithmetic. */
  T64_CHECK_INT(synchronize_to_tick(&machine, UINT64_C(1)),
                T64_STATUS_OK);
  T64_CHECK_INT(machine.cpu.cp0[CP0_COUNT], 0u);
  T64_CHECK_INT(machine.cpu.count_half_clock_phase, 1u);
  T64_CHECK_INT(machine.cpu.count_compare_deadline, 4u);
  T64_CHECK_INT(synchronize_to_tick(&machine, UINT64_C(2)),
                T64_STATUS_OK);
  T64_CHECK_INT(machine.cpu.cp0[CP0_COUNT], 1u);
  T64_CHECK_INT(machine.cpu.count_half_clock_phase, 0u);
  T64_CHECK(((uint32_t)machine.cpu.cp0[CP0_CAUSE] & CAUSE_IP7) == 0u);
  T64_CHECK_INT(synchronize_to_tick(&machine, UINT64_C(3)),
                T64_STATUS_OK);
  T64_CHECK_INT(machine.cpu.cp0[CP0_COUNT], 1u);
  T64_CHECK_INT(machine.cpu.count_half_clock_phase, 1u);
  T64_CHECK_INT(synchronize_to_tick(&machine, UINT64_C(4)),
                T64_STATUS_OK);
  T64_CHECK_INT(machine.cpu.cp0[CP0_COUNT], 2u);
  T64_CHECK_INT(machine.cpu.count_half_clock_phase, 0u);
  T64_CHECK(((uint32_t)machine.cpu.cp0[CP0_CAUSE] & CAUSE_IP7) != 0u);
  T64_CHECK_INT(machine.cpu.count_equality_latch_generation, 1u);
  T64_CHECK(machine.cpu.count_equality_latched);

  write_cp0(&machine, CP0_COMPARE, UINT32_C(2));
  T64_CHECK(((uint32_t)machine.cpu.cp0[CP0_CAUSE] & CAUSE_IP7) == 0u);
  T64_CHECK(machine.cpu.count_equality_latched);
  T64_CHECK_INT(synchronize_to_tick(&machine, UINT64_C(4)),
                T64_STATUS_OK);
  T64_CHECK(((uint32_t)machine.cpu.cp0[CP0_CAUSE] & CAUSE_IP7) == 0u);
  T64_CHECK_INT(synchronize_to_tick(&machine, UINT64_C(5)),
                T64_STATUS_OK);
  T64_CHECK_INT(machine.cpu.cp0[CP0_COUNT], 2u);
  T64_CHECK(((uint32_t)machine.cpu.cp0[CP0_CAUSE] & CAUSE_IP7) == 0u);
  T64_CHECK_INT(synchronize_to_tick(&machine, UINT64_C(6)),
                T64_STATUS_OK);
  T64_CHECK_INT(machine.cpu.cp0[CP0_COUNT], 3u);
  T64_CHECK(!machine.cpu.count_equality_latched);
  T64_CHECK(((uint32_t)machine.cpu.cp0[CP0_CAUSE] & CAUSE_IP7) == 0u);
  destroy_machine(&machine);
}

static void test_count_write_preserves_phase_and_checks_equality(void) {
  t64_machine machine;

  T64_CHECK(setup_machine(&machine, false));
  if (machine.profile.machine_model_version != 3u) {
    return;
  }
  machine.cpu.cp0[CP0_COMPARE] = UINT32_C(6);
  T64_CHECK_INT(synchronize_to_tick(&machine, UINT64_C(1)),
                T64_STATUS_OK);
  T64_CHECK_INT(machine.cpu.count_half_clock_phase, 1u);
  write_cp0(&machine, CP0_COUNT, UINT32_C(6));
  T64_CHECK_INT(machine.cpu.cp0[CP0_COUNT], 6u);
  T64_CHECK_INT(machine.cpu.count_half_clock_phase, 1u);
  T64_CHECK_INT(machine.cpu.count_sync_tick, 1u);
  T64_CHECK_INT(machine.cpu.count_equality_latch_generation, 1u);
  T64_CHECK(machine.cpu.count_equality_latched);
  T64_CHECK(((uint32_t)machine.cpu.cp0[CP0_CAUSE] & CAUSE_IP7) != 0u);

  write_cp0(&machine, CP0_COMPARE, UINT32_C(6));
  T64_CHECK(((uint32_t)machine.cpu.cp0[CP0_CAUSE] & CAUSE_IP7) == 0u);
  T64_CHECK(machine.cpu.count_equality_latched);
  T64_CHECK_INT(t64_cp0_synchronize_guest_time(&machine, UINT64_C(2)),
                T64_STATUS_OK);
  T64_CHECK_INT(machine.cpu.cp0[CP0_COUNT], 7u);
  T64_CHECK_INT(machine.cpu.count_half_clock_phase, 0u);
  T64_CHECK(((uint32_t)machine.cpu.cp0[CP0_CAUSE] & CAUSE_IP7) == 0u);
  destroy_machine(&machine);
}

static void test_interrupt_owners_and_masks(void) {
  t64_machine machine;
  uint32_t cause;

  T64_CHECK(setup_machine(&machine, false));
  if (machine.profile.machine_model_version != 3u) {
    return;
  }
  machine.cpu.cp0[CP0_CAUSE] = CAUSE_IP0 | CAUSE_IP7;
  machine.cpu.cp0[CP0_STATUS] = STATUS_IM0 | STATUS_IE;
  T64_CHECK(t64_cp0_interrupt_pending(&machine));
  machine.cpu.cp0[CP0_STATUS] |= STATUS_EXL;
  T64_CHECK(!t64_cp0_interrupt_pending(&machine));
  machine.cpu.cp0[CP0_STATUS] = STATUS_IM7 | STATUS_IE | STATUS_ERL;
  T64_CHECK(!t64_cp0_interrupt_pending(&machine));
  machine.cpu.cp0[CP0_STATUS] = STATUS_IM7;
  T64_CHECK(!t64_cp0_interrupt_pending(&machine));
  machine.cpu.cp0[CP0_STATUS] = STATUS_IM7 | STATUS_IE;
  T64_CHECK(t64_cp0_interrupt_pending(&machine));

  machine.cpu.cp0[CP0_STATUS] = STATUS_IM2 | STATUS_IE;
  machine.mi_irq_line = true;
  T64_CHECK(t64_cp0_interrupt_pending(&machine));
  cause = (uint32_t)machine.cpu.cp0[CP0_CAUSE];
  T64_CHECK((cause & (CAUSE_IP0 | CAUSE_IP2 | CAUSE_IP7)) ==
            (CAUSE_IP0 | CAUSE_IP2 | CAUSE_IP7));
  machine.mi_irq_line = false;
  T64_CHECK(!t64_cp0_interrupt_pending(&machine));
  cause = (uint32_t)machine.cpu.cp0[CP0_CAUSE];
  T64_CHECK((cause & (CAUSE_IP0 | CAUSE_IP2 | CAUSE_IP7)) ==
            (CAUSE_IP0 | CAUSE_IP7));
  destroy_machine(&machine);
}

static void test_scheduler_idle_partition_and_max_tick(void) {
  t64_machine one_shot;
  t64_machine partitioned;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_MAX};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  uint64_t expected_equality_count;

  T64_CHECK(setup_machine(&one_shot, false));
  T64_CHECK(setup_machine(&partitioned, false));
  if (one_shot.profile.machine_model_version != 3u ||
      partitioned.profile.machine_model_version != 3u) {
    return;
  }
  one_shot.cpu.cp0[CP0_COUNT] = UINT32_C(0xfffffffe);
  one_shot.cpu.cp0[CP0_COMPARE] = UINT32_C(0xffffffff);
  one_shot.cpu.count_half_clock_phase = 1u;
  partitioned.cpu.cp0[CP0_COUNT] = UINT32_C(0xfffffffe);
  partitioned.cpu.cp0[CP0_COMPARE] = UINT32_C(0xffffffff);
  partitioned.cpu.count_half_clock_phase = 1u;

  T64_CHECK_INT(t64_machine_advance(&one_shot, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.tick, UINT64_MAX);
  request.value = UINT64_MAX - UINT64_C(4);
  T64_CHECK_INT(t64_machine_advance(&partitioned, &request, &result),
                T64_STATUS_OK);
  request.value = UINT64_MAX;
  T64_CHECK_INT(t64_machine_advance(&partitioned, &request, &result),
                T64_STATUS_OK);
  T64_CHECK(timer_state_equal(&one_shot, &partitioned));
  T64_CHECK_INT(one_shot.cpu.cp0[CP0_COUNT], UINT32_C(0xfffffffe));
  T64_CHECK_INT(one_shot.cpu.count_half_clock_phase, 0u);
  T64_CHECK_INT(one_shot.cpu.count_sync_tick, UINT64_MAX);
  T64_CHECK_INT(one_shot.cpu.count_compare_deadline, UINT64_MAX);
  T64_CHECK(!one_shot.cpu.count_equality_latched);
  T64_CHECK(((uint32_t)one_shot.cpu.cp0[CP0_CAUSE] & CAUSE_IP7) != 0u);

  /* 2^63 model increments cross Compare once per 2^32 Count values. */
  expected_equality_count = UINT64_C(1) +
      ((UINT64_C(1) << 63) - UINT64_C(1)) / (UINT64_C(1) << 32);
  T64_CHECK_INT(one_shot.cpu.count_equality_latch_generation,
                expected_equality_count);
  destroy_machine(&one_shot);
  destroy_machine(&partitioned);
}

static void test_count_wrap_partition_and_initial_equal(void) {
  t64_machine one_shot;
  t64_machine partitioned;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(4)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};

  T64_CHECK(setup_machine(&one_shot, false));
  T64_CHECK(setup_machine(&partitioned, false));
  if (one_shot.profile.machine_model_version != 3u ||
      partitioned.profile.machine_model_version != 3u) {
    return;
  }
  one_shot.cpu.cp0[CP0_COUNT] = UINT32_C(0xfffffffe);
  one_shot.cpu.cp0[CP0_COMPARE] = 0u;
  partitioned.cpu.cp0[CP0_COUNT] = UINT32_C(0xfffffffe);
  partitioned.cpu.cp0[CP0_COMPARE] = 0u;
  T64_CHECK_INT(t64_machine_advance(&one_shot, &request, &result),
                T64_STATUS_OK);
  request.value = UINT64_C(2);
  T64_CHECK_INT(t64_machine_advance(&partitioned, &request, &result),
                T64_STATUS_OK);
  request.value = UINT64_C(4);
  T64_CHECK_INT(t64_machine_advance(&partitioned, &request, &result),
                T64_STATUS_OK);
  T64_CHECK(timer_state_equal(&one_shot, &partitioned));
  T64_CHECK_INT(one_shot.cpu.cp0[CP0_COUNT], 0u);
  T64_CHECK(((uint32_t)one_shot.cpu.cp0[CP0_CAUSE] & CAUSE_IP7) != 0u);
  T64_CHECK_INT(one_shot.cpu.count_equality_latch_generation, 1u);

  one_shot.cpu.cp0[CP0_COUNT] = UINT32_C(7);
  one_shot.cpu.cp0[CP0_COMPARE] = UINT32_C(7);
  one_shot.cpu.cp0[CP0_CAUSE] = 0u;
  one_shot.cpu.count_sync_tick = one_shot.tick;
  one_shot.cpu.count_half_clock_phase = 0u;
  one_shot.cpu.count_equality_latched = false;
  T64_CHECK_INT(synchronize_to_tick(&one_shot, one_shot.tick),
                T64_STATUS_OK);
  T64_CHECK(((uint32_t)one_shot.cpu.cp0[CP0_CAUSE] & CAUSE_IP7) == 0u);
  T64_CHECK(!one_shot.cpu.count_equality_latched);
  T64_CHECK_INT(synchronize_to_tick(&one_shot,
                                    one_shot.tick + UINT64_C(1)),
                T64_STATUS_OK);
  T64_CHECK(((uint32_t)one_shot.cpu.cp0[CP0_CAUSE] & CAUSE_IP7) == 0u);
  T64_CHECK(!one_shot.cpu.count_equality_latched);
  destroy_machine(&one_shot);
  destroy_machine(&partitioned);
}

static void write_be32(uint8_t *bytes, uint32_t value) {
  bytes[0] = (uint8_t)(value >> 24);
  bytes[1] = (uint8_t)(value >> 16);
  bytes[2] = (uint8_t)(value >> 8);
  bytes[3] = (uint8_t)value;
}

static void test_interrupt_preempts_delay_slot_cp0_write(void) {
  t64_machine machine;
  const uint64_t branch_pc = t64_cpu_canonical_address(UINT32_C(0x80001000));
  const uint64_t slot_pc = branch_pc + UINT64_C(4);
  const uint32_t mtc0_t0_count = (UINT32_C(0x10) << 26) |
                                 (UINT32_C(4) << 21) |
                                 (UINT32_C(8) << 16) |
                                 (CP0_COUNT << 11);

  T64_CHECK(setup_machine(&machine, true));
  if (machine.ram == NULL) {
    return;
  }
  machine.tick = UINT64_C(2);
  machine.cpu.count_sync_tick = 0u;
  machine.cpu.cp0[CP0_COUNT] = 0u;
  machine.cpu.cp0[CP0_COMPARE] = 1u;
  machine.cpu.cp0[CP0_STATUS] = STATUS_IE | STATUS_IM7;
  machine.cpu.gpr[8] = UINT64_C(9);
  machine.cpu.pc_full = slot_pc;
  machine.cpu.pc = (uint32_t)slot_pc;
  machine.cpu.delay_slot_pending = true;
  machine.cpu.delay_slot_pc_full = slot_pc;
  machine.cpu.delay_target_full = t64_cpu_canonical_address(
      UINT32_C(0x80002000));
  write_be32(machine.ram + UINT32_C(0x1004), mtc0_t0_count);

  T64_CHECK_INT(t64_cpu_step(&machine, NULL), T64_STATUS_OK);
  T64_CHECK_INT(machine.cpu.cp0[CP0_COUNT], 1u);
  T64_CHECK_INT(machine.cpu.cp0[CP0_COMPARE], 1u);
  T64_CHECK_INT(machine.cpu.cp0[CP0_EPC], branch_pc);
  T64_CHECK(((uint32_t)machine.cpu.cp0[CP0_CAUSE] &
             (UINT32_C(0x8000007c))) == UINT32_C(0x80000000));
  T64_CHECK(((uint32_t)machine.cpu.cp0[CP0_STATUS] & STATUS_EXL) != 0u);
  T64_CHECK_INT(machine.cpu.gpr[8], 9u);
  T64_CHECK_INT(machine.cpu.retired_instructions, 0u);
  T64_CHECK_INT(machine.cpu.pending_exception_code, 0u);
  T64_CHECK(machine.cpu.pending_exception_bd);
  destroy_machine(&machine);
}

static t64_status set_mi_irq_event(t64_machine *machine,
                                   const t64_event *event) {
  machine->mi_irq_line = event->value != 0u;
  return T64_STATUS_OK;
}

static bool run_same_tick_irq_case(bool irq_first, uint64_t expected_retired,
                                   uint64_t expected_epc) {
  t64_machine machine;
  t64_event_reservation reservation;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(2)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_status status;

  if (!setup_machine(&machine, true) || machine.ram == NULL) {
    T64_CHECK(false);
    destroy_machine(&machine);
    return false;
  }
  machine.cpu.pc = UINT32_C(0x80001000);
  machine.cpu.pc_full = t64_cpu_canonical_address(machine.cpu.pc);
  machine.cpu.cp0[CP0_STATUS] = STATUS_IE | STATUS_IM2;
  write_be32(machine.ram + UINT32_C(0x1000), 0u); /* nop */
  write_be32(machine.ram + UINT32_C(0x1004), 0u); /* nop */
  if (irq_first) {
    request.value = UINT64_C(1);
  }

  if (irq_first) {
    status = t64_schedule_reserve(&machine, UINT64_C(1),
                                  T64_DEVICE_MI_TEST, UINT32_C(1),
                                  UINT64_C(1), UINT64_C(0),
                                  set_mi_irq_event, &reservation);
    T64_CHECK_INT(status, T64_STATUS_OK);
    if (status == T64_STATUS_OK) {
      T64_CHECK_INT(t64_schedule_commit(&machine, &reservation),
                    T64_STATUS_OK);
    }
    T64_CHECK_INT(t64_cpu_schedule(&machine), T64_STATUS_OK);
  } else {
    T64_CHECK_INT(t64_cpu_schedule(&machine), T64_STATUS_OK);
    status = t64_schedule_reserve(&machine, UINT64_C(1),
                                  T64_DEVICE_MI_TEST, UINT32_C(1),
                                  UINT64_C(1), UINT64_C(0),
                                  set_mi_irq_event, &reservation);
    T64_CHECK_INT(status, T64_STATUS_OK);
    if (status == T64_STATUS_OK) {
      T64_CHECK_INT(t64_schedule_commit(&machine, &reservation),
                    T64_STATUS_OK);
    }
  }
  T64_CHECK_INT(t64_machine_advance(&machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.tick, irq_first ? UINT64_C(1) : UINT64_C(2));
  T64_CHECK_INT(machine.cpu.retired_instructions, expected_retired);
  T64_CHECK_INT(machine.cpu.cp0[CP0_EPC], expected_epc);
  T64_CHECK(((uint32_t)machine.cpu.cp0[CP0_CAUSE] & CAUSE_IP2) != 0u);
  T64_CHECK(((uint32_t)machine.cpu.cp0[CP0_STATUS] & STATUS_EXL) != 0u);
  destroy_machine(&machine);
  return true;
}

static void test_equal_tick_fifo_samples_new_irq_at_next_boundary(void) {
  const uint64_t start_pc =
      t64_cpu_canonical_address(UINT32_C(0x80001000));
  const uint64_t next_pc = start_pc + UINT64_C(4);

  /* Device-first FIFO is visible to the CPU on that boundary. CPU-first FIFO
     completes its current instruction, then samples the new line next tick. */
  (void)run_same_tick_irq_case(true, UINT64_C(0), start_pc);
  (void)run_same_tick_irq_case(false, UINT64_C(1), next_pc);
}

static void test_random_advances_only_on_retirement(void) {
  t64_machine machine;

  T64_CHECK(setup_machine(&machine, false));
  if (machine.profile.machine_model_version != 3u) {
    return;
  }
  T64_CHECK_INT(synchronize_to_tick(&machine, UINT64_C(100)),
                T64_STATUS_OK);
  T64_CHECK_INT(machine.cpu.cp0[CP0_RANDOM], 17u);
  T64_CHECK_INT(machine.cpu.retired_instructions, 0u);
  t64_cp0_instruction_retired(&machine);
  T64_CHECK_INT(machine.cpu.cp0[CP0_RANDOM], 16u);
  T64_CHECK_INT(machine.cpu.retired_instructions, 0u);
  destroy_machine(&machine);
}

static void test_cold_t1_cost_table_for_profile(const char *profile_id) {
  typedef struct {
    uint32_t opcode;
    uint64_t interval;
    uint64_t left;
    uint64_t right;
  } expected_cost;
  static const expected_cost costs[] = {
      {UINT32_C(0x24030001), UINT64_C(1), 0u, 0u},
      {UINT32_C(0x00220018), UINT64_C(6), 6u, 7u},
      {UINT32_C(0x00220019), UINT64_C(6), 6u, 7u},
      {UINT32_C(0x0022001a), UINT64_C(38), 21u, 3u},
      {UINT32_C(0x0022001b), UINT64_C(38), 21u, 3u},
      {UINT32_C(0x0022001c), UINT64_C(9), 6u, 7u},
      {UINT32_C(0x0022001d), UINT64_C(9), 6u, 7u},
      {UINT32_C(0x0022001e), UINT64_C(70), 21u, 3u},
      {UINT32_C(0x0022001f), UINT64_C(70), 21u, 3u},
  };
  t64_machine machine;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(0)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  size_t index;

  T64_CHECK(setup_t1_machine(&machine, profile_id));
  if (machine.ram == NULL) {
    return;
  }
  for (index = 0u; index < sizeof costs / sizeof costs[0]; index++) {
    reset_t1_cpu(&machine, costs[index].opcode);
    machine.cpu.gpr[1] = costs[index].left;
    machine.cpu.gpr[2] = costs[index].right;
    T64_CHECK_INT(t64_cpu_schedule(&machine), T64_STATUS_OK);
    request.value = machine.tick + costs[index].interval;
    T64_CHECK_INT(t64_machine_advance(&machine, &request, &result),
                  T64_STATUS_OK);
    T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
    T64_CHECK_INT(result.tick, request.value);
    T64_CHECK_INT(machine.cpu.retired_instructions, 1u);
    T64_CHECK_INT(machine.instruction_count, 1u);
    T64_CHECK_INT(machine.cpu.last_pc_full,
                  UINT64_C(0xffffffff80001000));
    T64_CHECK_INT(machine.cpu.last_opcode, costs[index].opcode);
    T64_CHECK_INT(machine.cpu.cp0[CP0_RANDOM], 16u);
    T64_CHECK_INT(machine.queue_count, 1u);
    if (machine.queue_count == 1u) {
      /* NEC VR43XX Table 3-12 gives 5/37/8/69 stall cycles for the
         MDU pairs. T1 adds the issue interval; funct values 18-1f in
         these literal SPECIAL words are anchored in the same manual. */
      T64_CHECK_INT(machine.events[machine.queue_head].tick,
                    costs[index].interval + UINT64_C(1));
    }
    if (index == 1u) {
      T64_CHECK_INT(machine.cpu.hi, 0u);
      T64_CHECK_INT(machine.cpu.lo, 42u);
    } else if (index == 0u) {
      T64_CHECK_INT(machine.cpu.gpr[3], 1u);
    }
  }
  destroy_machine(&machine);
}

static void test_cold_t1_mult_interval(void) {
  test_cold_t1_cost_table_for_profile(
      "ntsc-cart-4m-openipl3-rdram-a-t1-v1");
  test_cold_t1_cost_table_for_profile(
      "ntsc-cart-4m-openipl3-rdram-b-t1-v1");
}

static void test_cold_t1_delay_slot_interval_for_profile(
    const char *profile_id) {
  t64_machine machine;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(2)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};

  T64_CHECK(setup_t1_machine(&machine, profile_id));
  if (machine.ram == NULL) {
    return;
  }
  reset_t1_cpu(&machine, UINT32_C(0x10000001)); /* beq $zero,$zero,+1 */
  write_be32(machine.ram + UINT32_C(0x1004), UINT32_C(0x0022001a));
  machine.cpu.gpr[1] = UINT64_C(21);
  machine.cpu.gpr[2] = UINT64_C(3);
  T64_CHECK_INT(t64_cpu_schedule(&machine), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_advance(&machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, 2u);
  T64_CHECK_INT(machine.cpu.retired_instructions, 2u);
  T64_CHECK_INT(machine.cpu.cp0[CP0_RANDOM], 15u);
  T64_CHECK_INT(machine.cpu.cp0[CP0_COUNT], 1u);
  T64_CHECK_INT(machine.cpu.count_half_clock_phase, 0u);
  T64_CHECK_INT(machine.cpu.last_pc_full,
                UINT64_C(0xffffffff80001004));
  T64_CHECK_INT(machine.cpu.last_opcode, UINT32_C(0x0022001a));
  T64_CHECK_INT(machine.cpu.lo, 7u);
  T64_CHECK_INT(machine.queue_count, 1u);
  if (machine.queue_count == 1u) {
    /* Branch and its slot each issue once; DIV adds 37 policy stalls. */
    T64_CHECK_INT(machine.events[machine.queue_head].tick, 40u);
  }
  destroy_machine(&machine);
}

static void test_cold_t1_delay_slot_interval(void) {
  test_cold_t1_delay_slot_interval_for_profile(
      "ntsc-cart-4m-openipl3-rdram-a-t1-v1");
  test_cold_t1_delay_slot_interval_for_profile(
      "ntsc-cart-4m-openipl3-rdram-b-t1-v1");
}

static void test_cold_t1_count_random_follow_elapsed_ticks_for_profile(
    const char *profile_id) {
  t64_machine machine;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(2)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};

  T64_CHECK(setup_t1_machine(&machine, profile_id));
  if (machine.ram == NULL) {
    return;
  }

  reset_t1_cpu(&machine, 0u);
  write_be32(machine.ram + UINT32_C(0x1004), 0u);
  machine.cpu.cp0[CP0_COMPARE] = UINT32_C(1);
  T64_CHECK_INT(t64_cpu_schedule(&machine), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_advance(&machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.tick, 2u);
  T64_CHECK_INT(machine.cpu.retired_instructions, 2u);
  T64_CHECK_INT(machine.cpu.cp0[CP0_COUNT], 1u);
  T64_CHECK_INT(machine.cpu.count_half_clock_phase, 0u);
  T64_CHECK(((uint32_t)machine.cpu.cp0[CP0_CAUSE] & CAUSE_IP7) != 0u);
  T64_CHECK_INT(machine.cpu.cp0[CP0_RANDOM], 15u);
  T64_CHECK_INT(machine.events[machine.queue_head].tick, 3u);

  reset_t1_cpu(&machine, UINT32_C(0x00220018));
  machine.cpu.gpr[1] = UINT64_C(6);
  machine.cpu.gpr[2] = UINT64_C(7);
  machine.cpu.cp0[CP0_COMPARE] = UINT32_C(3);
  T64_CHECK_INT(t64_cpu_schedule(&machine), T64_STATUS_OK);
  request.value = UINT64_C(6);
  T64_CHECK_INT(t64_machine_advance(&machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.tick, 6u);
  T64_CHECK_INT(machine.cpu.retired_instructions, 1u);
  T64_CHECK_INT(machine.cpu.cp0[CP0_COUNT], 3u);
  T64_CHECK_INT(machine.cpu.count_half_clock_phase, 0u);
  T64_CHECK(((uint32_t)machine.cpu.cp0[CP0_CAUSE] & CAUSE_IP7) != 0u);
  T64_CHECK_INT(machine.cpu.cp0[CP0_RANDOM], 16u);
  T64_CHECK_INT(machine.events[machine.queue_head].tick, 7u);
  destroy_machine(&machine);
}

static void test_cold_t1_count_random_follow_elapsed_ticks(void) {
  test_cold_t1_count_random_follow_elapsed_ticks_for_profile(
      "ntsc-cart-4m-openipl3-rdram-a-t1-v1");
  test_cold_t1_count_random_follow_elapsed_ticks_for_profile(
      "ntsc-cart-4m-openipl3-rdram-b-t1-v1");
}

static void test_cold_t1_fetched_compare_clear_for_profile(
    const char *profile_id) {
  const uint32_t mtc0_compare = UINT32_C(0x40885800);
  t64_machine machine;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(1)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};

  T64_CHECK(setup_t1_machine(&machine, profile_id));
  if (machine.ram == NULL) {
    return;
  }
  reset_t1_cpu(&machine, 0u);
  write_be32(machine.ram + UINT32_C(0x1004), mtc0_compare);
  machine.cpu.cp0[CP0_COMPARE] = UINT32_C(1);
  machine.cpu.gpr[8] = UINT64_C(1);
  T64_CHECK_INT(t64_cpu_schedule(&machine), T64_STATUS_OK);

  T64_CHECK_INT(t64_machine_advance(&machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, UINT64_C(1));
  T64_CHECK_INT(machine.cpu.last_pc_full,
                UINT64_C(0xffffffff80001000));
  T64_CHECK_INT(machine.cpu.last_opcode, 0u);
  T64_CHECK_INT(machine.cpu.retired_instructions, 1u);
  T64_CHECK_INT(machine.cpu.cp0[CP0_COUNT], 0u);
  T64_CHECK_INT(machine.cpu.count_half_clock_phase, 1u);
  T64_CHECK(((uint32_t)machine.cpu.cp0[CP0_CAUSE] & CAUSE_IP7) == 0u);

  request.value = machine.tick + UINT64_C(1);
  T64_CHECK_INT(t64_machine_advance(&machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, UINT64_C(2));
  T64_CHECK_INT(machine.cpu.last_pc_full,
                UINT64_C(0xffffffff80001004));
  T64_CHECK_INT(machine.cpu.last_opcode, mtc0_compare);
  T64_CHECK_INT(machine.cpu.retired_instructions, 2u);
  T64_CHECK_INT(machine.cpu.cp0[CP0_COUNT], 1u);
  T64_CHECK_INT(machine.cpu.cp0[CP0_COMPARE], 1u);
  T64_CHECK_INT(machine.cpu.count_half_clock_phase, 0u);
  T64_CHECK_INT(machine.cpu.count_equality_latch_generation, 1u);
  T64_CHECK(machine.cpu.count_equality_latched);
  T64_CHECK(((uint32_t)machine.cpu.cp0[CP0_CAUSE] & CAUSE_IP7) == 0u);
  T64_CHECK_INT(machine.queue_count, 1u);
  if (machine.queue_count == 1u) {
    T64_CHECK_INT(machine.events[machine.queue_head].tick, 3u);
  }
  destroy_machine(&machine);
}

static void test_cold_t1_fetched_compare_clear(void) {
  test_cold_t1_fetched_compare_clear_for_profile(
      "ntsc-cart-4m-openipl3-rdram-a-t1-v1");
  test_cold_t1_fetched_compare_clear_for_profile(
      "ntsc-cart-4m-openipl3-rdram-b-t1-v1");
}

static void test_cold_t1_fetched_wired_random_tlbwr_sequence_for_profile(
    const char *profile_id) {
  const uint32_t mtc0_wired = UINT32_C(0x40883000);
  const uint32_t mfc0_random = UINT32_C(0x40090800);
  const uint32_t tlbwr = UINT32_C(0x42000006);
  t64_machine machine;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(1)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};

  T64_CHECK(setup_t1_machine(&machine, profile_id));
  if (machine.ram == NULL) {
    return;
  }
  reset_t1_cpu(&machine, mtc0_wired);
  write_be32(machine.ram + UINT32_C(0x1004), mfc0_random);
  write_be32(machine.ram + UINT32_C(0x1008), tlbwr);
  machine.cpu.gpr[8] = UINT64_C(2);
  machine.cpu.cp0[CP0_ENTRYHI] = UINT64_C(0x000000000000a009);
  machine.cpu.cp0[CP0_ENTRYLO0] = UINT64_C(0x0000000000012345);
  machine.cpu.cp0[CP0_ENTRYLO1] = UINT64_C(0x0000000000056789);
  T64_CHECK_INT(t64_cpu_schedule(&machine), T64_STATUS_OK);

  T64_CHECK_INT(t64_machine_advance(&machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, UINT64_C(1));
  T64_CHECK_INT(machine.cpu.retired_instructions, 1u);
  T64_CHECK_INT(machine.cpu.last_pc_full,
                UINT64_C(0xffffffff80001000));
  T64_CHECK_INT(machine.cpu.last_opcode, mtc0_wired);
  T64_CHECK_INT(machine.cpu.cp0[CP0_WIRED], 2u);
  T64_CHECK_INT(machine.cpu.cp0[CP0_RANDOM], 30u);
  T64_CHECK_INT(machine.queue_count, 1u);
  if (machine.queue_count == 1u) {
    T64_CHECK_INT(machine.events[machine.queue_head].tick, 2u);
  }

  request.value = machine.tick + UINT64_C(1);
  T64_CHECK_INT(t64_machine_advance(&machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, UINT64_C(2));
  T64_CHECK_INT(machine.cpu.retired_instructions, 2u);
  T64_CHECK_INT(machine.cpu.last_pc_full,
                UINT64_C(0xffffffff80001004));
  T64_CHECK_INT(machine.cpu.last_opcode, mfc0_random);
  T64_CHECK_INT(machine.cpu.gpr[9], 30u);
  T64_CHECK_INT(machine.cpu.cp0[CP0_RANDOM], 29u);
  T64_CHECK_INT(machine.queue_count, 1u);
  if (machine.queue_count == 1u) {
    T64_CHECK_INT(machine.events[machine.queue_head].tick, 3u);
  }

  request.value = machine.tick + UINT64_C(1);
  T64_CHECK_INT(t64_machine_advance(&machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, UINT64_C(3));
  T64_CHECK_INT(machine.cpu.retired_instructions, 3u);
  T64_CHECK_INT(machine.cpu.last_pc_full,
                UINT64_C(0xffffffff80001008));
  T64_CHECK_INT(machine.cpu.last_opcode, tlbwr);
  T64_CHECK_INT(machine.cpu.cp0[CP0_RANDOM], 28u);
  T64_CHECK_INT(machine.cpu.tlb[29].vpn2, UINT64_C(0xa000));
  T64_CHECK_INT(machine.cpu.tlb[29].asid, 9u);
  T64_CHECK_INT(machine.cpu.tlb[29].entry_lo0, UINT64_C(0x12345));
  T64_CHECK_INT(machine.cpu.tlb[29].entry_lo1, UINT64_C(0x56789));
  T64_CHECK_INT(machine.cpu.tlb[28].vpn2, 0u);
  T64_CHECK_INT(machine.cpu.tlb[28].entry_lo0, 0u);
  T64_CHECK_INT(machine.queue_count, 1u);
  if (machine.queue_count == 1u) {
    T64_CHECK_INT(machine.events[machine.queue_head].tick, 4u);
  }
  destroy_machine(&machine);
}

static void test_cold_t1_fetched_wired_random_tlbwr_sequence(void) {
  test_cold_t1_fetched_wired_random_tlbwr_sequence_for_profile(
      "ntsc-cart-4m-openipl3-rdram-a-t1-v1");
  test_cold_t1_fetched_wired_random_tlbwr_sequence_for_profile(
      "ntsc-cart-4m-openipl3-rdram-b-t1-v1");
}

static void test_cold_t1_high_wired_modulo_for_profile(
    const char *profile_id, uint64_t expected_random) {
  const uint32_t mtc0_wired = UINT32_C(0x40883000);
  const uint32_t tlbwr = UINT32_C(0x42000006);
  t64_machine machine;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(32)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  size_t index;

  T64_CHECK(setup_t1_machine(&machine, profile_id));
  if (machine.ram == NULL) {
    return;
  }
  reset_t1_cpu(&machine, mtc0_wired);
  machine.cpu.gpr[8] = UINT64_C(32);
  machine.cpu.cp0[CP0_ENTRYHI] = UINT64_C(0x000000000000a009);
  machine.cpu.cp0[CP0_ENTRYLO0] = UINT64_C(0x0000000000012345);
  machine.cpu.cp0[CP0_ENTRYLO1] = UINT64_C(0x0000000000056789);
  for (index = 1u; index <= 31u; index++) {
    write_be32(machine.ram + UINT32_C(0x1000) + (uint32_t)(index * 4u),
               UINT32_C(0));
  }
  write_be32(machine.ram + UINT32_C(0x1080), tlbwr);
  T64_CHECK_INT(t64_cpu_schedule(&machine), T64_STATUS_OK);
  for (index = 0u; index < 32u; index++) {
    request.value = machine.tick + UINT64_C(1);
    T64_CHECK_INT(t64_machine_advance(&machine, &request, &result),
                  T64_STATUS_OK);
    T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
    T64_CHECK_INT(result.tick, request.value);
    T64_CHECK_INT(machine.cpu.retired_instructions, index + 1u);
    T64_CHECK_INT(machine.cpu.last_pc_full,
                  UINT64_C(0xffffffff80001000) + (uint64_t)index * 4u);
    T64_CHECK_INT(machine.cpu.last_opcode,
                  index == 0u ? mtc0_wired : UINT32_C(0));
  }
  T64_CHECK_INT(machine.tick, UINT64_C(32));
  T64_CHECK_INT(machine.cpu.cp0[CP0_WIRED], 32u);
  T64_CHECK_INT(machine.cpu.cp0[CP0_RANDOM], expected_random);
  T64_CHECK_INT(machine.queue_count, 1u);
  if (machine.queue_count == 1u) {
    T64_CHECK_INT(machine.events[machine.queue_head].tick, 33u);
  }

  request.value = machine.tick + UINT64_C(1);
  T64_CHECK_INT(t64_machine_advance(&machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, UINT64_C(33));
  T64_CHECK_INT(machine.cpu.retired_instructions, 33u);
  T64_CHECK_INT(machine.cpu.last_pc_full,
                UINT64_C(0xffffffff80001080));
  T64_CHECK_INT(machine.cpu.last_opcode, tlbwr);
  T64_CHECK_INT(machine.cpu.cp0[CP0_RANDOM], UINT64_C(62));
  T64_CHECK_INT(machine.cpu.tlb[31].vpn2, UINT64_C(0xa000));
  T64_CHECK_INT(machine.cpu.tlb[31].asid, 9u);
  T64_CHECK_INT(machine.cpu.tlb[31].entry_lo0, UINT64_C(0x12345));
  T64_CHECK_INT(machine.cpu.tlb[30].vpn2, 0u);
  T64_CHECK_INT(machine.queue_count, 1u);
  if (machine.queue_count == 1u) {
    T64_CHECK_INT(machine.events[machine.queue_head].tick, 34u);
  }
  destroy_machine(&machine);
}

static void test_cold_t1_high_wired_policy(void) {
  test_cold_t1_high_wired_modulo_for_profile(
      "ntsc-cart-4m-openipl3-rdram-a-t1-v1", UINT64_C(63));
  test_cold_t1_high_wired_modulo_for_profile(
      "ntsc-cart-4m-openipl3-rdram-b-t1-v1", UINT64_C(63));
}

static void check_t1_exception_boundary(t64_machine *machine) {
  T64_CHECK_INT(machine->queue_count, 1u);
  if (machine->queue_count == 1u) {
    T64_CHECK_INT(machine->events[machine->queue_head].tick, 2u);
  }
  T64_CHECK_INT(machine->cpu.retired_instructions, 0u);
  T64_CHECK_INT(machine->cpu.cp0[CP0_RANDOM], 17u);
}

static void test_cold_t1_exception_and_interrupt_boundaries_for_profile(
    const char *profile_id) {
  t64_machine machine;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(1)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};

  T64_CHECK(setup_t1_machine(&machine, profile_id));
  if (machine.ram == NULL) {
    return;
  }
  reset_t1_cpu(&machine, 0u);
  machine.cpu.pc = UINT32_C(0x80001002);
  machine.cpu.pc_full = t64_cpu_canonical_address(machine.cpu.pc);
  T64_CHECK_INT(t64_cpu_schedule(&machine), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_advance(&machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, 1u);
  check_t1_exception_boundary(&machine);
  T64_CHECK_INT(machine.cpu.last_opcode, 0u);
  T64_CHECK(((uint32_t)machine.cpu.cp0[CP0_STATUS] & STATUS_EXL) != 0u);

  reset_t1_cpu(&machine, UINT32_C(0x7000003f));
  T64_CHECK_INT(t64_cpu_schedule(&machine), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_advance(&machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, 1u);
  check_t1_exception_boundary(&machine);
  T64_CHECK_INT(machine.cpu.last_pc_full,
                UINT64_C(0xffffffff80001000));
  T64_CHECK_INT(machine.cpu.last_opcode, UINT32_C(0x7000003f));
  T64_CHECK_INT((uint32_t)machine.cpu.cp0[CP0_CAUSE] & UINT32_C(0x7c),
                UINT32_C(0x28));
  T64_CHECK(((uint32_t)machine.cpu.cp0[CP0_STATUS] & STATUS_EXL) != 0u);

  reset_t1_cpu(&machine, 0u);
  machine.cpu.cp0[CP0_STATUS] = STATUS_IE | STATUS_IM7;
  machine.cpu.cp0[CP0_CAUSE] = CAUSE_IP7;
  T64_CHECK_INT(t64_cpu_schedule(&machine), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_advance(&machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, 1u);
  check_t1_exception_boundary(&machine);
  T64_CHECK_INT(machine.cpu.last_opcode, 0u);
  T64_CHECK(((uint32_t)machine.cpu.cp0[CP0_STATUS] & STATUS_EXL) != 0u);
  destroy_machine(&machine);
}

static void test_cold_t1_exception_and_interrupt_boundaries(void) {
  test_cold_t1_exception_and_interrupt_boundaries_for_profile(
      "ntsc-cart-4m-openipl3-rdram-a-t1-v1");
  test_cold_t1_exception_and_interrupt_boundaries_for_profile(
      "ntsc-cart-4m-openipl3-rdram-b-t1-v1");
}

static void test_cold_t1_idle_count_compare_for_profile(const char *profile_id) {
  t64_machine machine;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(12)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};

  T64_CHECK(setup_t1_machine(&machine, profile_id));
  if (machine.ram == NULL) {
    return;
  }
  reset_t1_cpu(&machine, 0u);
  machine.cpu.cp0[CP0_COMPARE] = UINT32_C(6);
  T64_CHECK_INT(t64_test_schedule_synthetic(&machine, UINT64_C(8),
                                            T64_TEST_EVENT_NOOP, 1u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_advance(&machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, UINT64_C(12));
  T64_CHECK_INT(machine.tick, UINT64_C(12));
  T64_CHECK_INT(machine.cpu.cp0[CP0_COUNT], UINT32_C(6));
  T64_CHECK_INT(machine.cpu.count_half_clock_phase, 0u);
  T64_CHECK(machine.cpu.count_equality_latched);
  T64_CHECK_INT(machine.cpu.count_equality_latch_generation, 1u);
  T64_CHECK(((uint32_t)machine.cpu.cp0[CP0_CAUSE] & CAUSE_IP7) != 0u);
  T64_CHECK_INT(machine.instruction_count, 0u);
  T64_CHECK_INT(machine.cpu.retired_instructions, 0u);
  T64_CHECK_INT(machine.queue_count, 0u);
  destroy_machine(&machine);
}

static void test_cold_t1_idle_count_compare(void) {
  test_cold_t1_idle_count_compare_for_profile(
      "ntsc-cart-4m-openipl3-rdram-a-t1-v1");
  test_cold_t1_idle_count_compare_for_profile(
      "ntsc-cart-4m-openipl3-rdram-b-t1-v1");
}

static void fill_t1_cpu_queue(t64_machine *machine, size_t count) {
  machine->queue_head = 0u;
  machine->queue_count = count;
}

static void test_cold_t1_preflight_before_mutation(void) {
  t64_machine machine;
  const char *profile_id = "ntsc-cart-4m-openipl3-rdram-a-t1-v1";
  const uint32_t pi_store = UINT32_C(0xac22000c);
  const uint32_t mtc0_count = UINT32_C(0x40884800);
  const uint32_t fetched_line_index = UINT32_C(0x80);
  t64_vr4300_icache_line fetched_line_before;
  t64_status status;

  T64_CHECK(setup_t1_machine(&machine, profile_id));
  if (machine.ram == NULL) {
    return;
  }

  reset_t1_cpu(&machine, UINT32_C(0x00220018));
  machine.tick = UINT64_MAX - UINT64_C(5);
  machine.cpu.count_sync_tick = machine.tick;
  machine.cpu.hi = UINT64_C(0x1111);
  machine.cpu.lo = UINT64_C(0x2222);
  (void)memcpy(&fetched_line_before,
               &machine.cpu.icache[fetched_line_index],
               sizeof fetched_line_before);
  status = t64_cpu_step(&machine, NULL);
  T64_CHECK_INT(status, T64_STATUS_TICK_OVERFLOW);
  T64_CHECK_INT(machine.cpu.last_pc_full,
                UINT64_C(0xffffffff80001000));
  T64_CHECK_INT(machine.cpu.last_opcode, UINT32_C(0x00220018));
  T64_CHECK_INT(machine.cpu.hi, UINT64_C(0x1111));
  T64_CHECK_INT(machine.cpu.lo, UINT64_C(0x2222));
  T64_CHECK_INT(machine.cpu.retired_instructions, 0u);
  T64_CHECK_INT(machine.instruction_count, 0u);
  T64_CHECK_INT(machine.queue_count, 0u);
  T64_CHECK(machine.cpu.icache[fetched_line_index].valid ==
            fetched_line_before.valid);
  T64_CHECK_INT(machine.cpu.icache[fetched_line_index].physical_tag,
                fetched_line_before.physical_tag);
  T64_CHECK(memcmp(machine.cpu.icache[fetched_line_index].data,
                   fetched_line_before.data,
                   sizeof fetched_line_before.data) == 0);

  reset_t1_cpu(&machine, pi_store);
  machine.cpu.gpr[1] = t64_cpu_canonical_address(UINT32_C(0xa4600000));
  machine.cpu.gpr[2] = UINT64_C(0x100);
  fill_t1_cpu_queue(&machine,
                    (size_t)machine.profile.event_queue_capacity - 1u);
  (void)memcpy(&fetched_line_before,
               &machine.cpu.icache[fetched_line_index],
               sizeof fetched_line_before);
  status = t64_cpu_step(&machine, NULL);
  T64_CHECK_INT(status, T64_STATUS_QUEUE_FULL);
  T64_CHECK_INT(machine.cpu.last_pc_full,
                UINT64_C(0xffffffff80001000));
  T64_CHECK_INT(machine.cpu.last_opcode, pi_store);
  T64_CHECK_INT(machine.pi.dram_address, 0u);
  T64_CHECK_INT(machine.pi.cart_address, 0u);
  T64_CHECK_INT(machine.cpu.retired_instructions, 0u);
  T64_CHECK_INT(machine.queue_count,
                (size_t)machine.profile.event_queue_capacity - 1u);
  T64_CHECK(machine.cpu.icache[fetched_line_index].valid ==
            fetched_line_before.valid);
  T64_CHECK_INT(machine.cpu.icache[fetched_line_index].physical_tag,
                fetched_line_before.physical_tag);
  T64_CHECK(memcmp(machine.cpu.icache[fetched_line_index].data,
                   fetched_line_before.data,
                   sizeof fetched_line_before.data) == 0);

  reset_t1_cpu(&machine, pi_store);
  machine.cpu.gpr[1] = t64_cpu_canonical_address(UINT32_C(0xa4600000));
  machine.cpu.gpr[2] = UINT64_C(0x100);
  machine.next_sequence = UINT64_MAX;
  (void)memcpy(&fetched_line_before,
               &machine.cpu.icache[fetched_line_index],
               sizeof fetched_line_before);
  status = t64_cpu_step(&machine, NULL);
  T64_CHECK_INT(status, T64_STATUS_SEQUENCE_EXHAUSTED);
  T64_CHECK_INT(machine.cpu.last_pc_full,
                UINT64_C(0xffffffff80001000));
  T64_CHECK_INT(machine.cpu.last_opcode, pi_store);
  T64_CHECK_INT(machine.pi.dram_address, 0u);
  T64_CHECK_INT(machine.pi.cart_address, 0u);
  T64_CHECK_INT(machine.cpu.retired_instructions, 0u);
  T64_CHECK(machine.cpu.icache[fetched_line_index].valid ==
            fetched_line_before.valid);
  T64_CHECK_INT(machine.cpu.icache[fetched_line_index].physical_tag,
                fetched_line_before.physical_tag);
  T64_CHECK(memcmp(machine.cpu.icache[fetched_line_index].data,
                   fetched_line_before.data,
                   sizeof fetched_line_before.data) == 0);

  reset_t1_cpu(&machine, mtc0_count);
  machine.cpu.gpr[8] = UINT64_C(123);
  fill_t1_cpu_queue(&machine,
                    (size_t)machine.profile.event_queue_capacity);
  status = t64_cpu_step(&machine, NULL);
  T64_CHECK_INT(status, T64_STATUS_QUEUE_FULL);
  T64_CHECK_INT(machine.cpu.cp0[CP0_COUNT], 0u);
  T64_CHECK_INT(machine.cpu.gpr[8], 123u);
  T64_CHECK_INT(machine.cpu.retired_instructions, 0u);

  reset_t1_cpu(&machine, 0u);
  machine.cpu.pc = UINT32_C(0x80001002);
  machine.cpu.pc_full = t64_cpu_canonical_address(machine.cpu.pc);
  fill_t1_cpu_queue(&machine,
                    (size_t)machine.profile.event_queue_capacity);
  status = t64_cpu_step(&machine, NULL);
  T64_CHECK_INT(status, T64_STATUS_QUEUE_FULL);
  T64_CHECK_INT(machine.cpu.cp0[CP0_STATUS], 0u);
  T64_CHECK_INT(machine.cpu.cp0[CP0_EPC], 0u);
  T64_CHECK(!machine.cpu.pending_exception_valid);
  T64_CHECK_INT(machine.cpu.retired_instructions, 0u);
  destroy_machine(&machine);
}

static bool t1_dma_machine_allocate(void *context, size_t size,
                                   size_t alignment,
                                   t64_host_storage *storage_out) {
  void *memory;

  (void)context;
  if (storage_out == NULL || size == 0u || alignment == 0u) {
    return false;
  }
  memory = malloc(size);
  if (memory == NULL || (uintptr_t)memory % alignment != 0u) {
    free(memory);
    return false;
  }
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  return true;
}

static void t1_dma_machine_release(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static bool setup_t1_dma_machine(t64_machine **machine_out,
                                const char *profile_id, uint8_t image[4096],
                                uint8_t dram_seed) {
  t64_host_allocator allocator = {t1_dma_machine_allocate,
                                  t1_dma_machine_release, NULL};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine;
  size_t index;

  if (machine_out == NULL || profile_id == NULL || image == NULL) {
    return false;
  }
  *machine_out = NULL;
  if (!read_t1_ipl3_asset(image) ||
      t64_profile_lookup(profile_id, &profile) != T64_STATUS_OK ||
      profile == NULL ||
      t64_machine_create(&allocator, profile, machine_out) != T64_STATUS_OK ||
      *machine_out == NULL) {
    return false;
  }
  machine = *machine_out;
  if (t64_machine_admit_rom(machine, image, 4096u) != T64_STATUS_OK) {
    t64_machine_destroy(machine);
    *machine_out = NULL;
    return false;
  }
  reset_t1_cpu(machine, UINT32_C(0x0022001e)); /* DDIV $zero,$1,$2 */
  machine->cpu.gpr[1] = UINT64_C(21);
  machine->cpu.gpr[2] = UINT64_C(3);
  write_be32(machine->sp_imem, UINT32_C(0x24010007)); /* ADDIU r1,r0,7 */
  machine->rsp.sp_status = T1_TEST_RSP_STATUS_SINGLE_STEP;
  machine->pi.next_token = UINT64_C(1);
  machine->sp.next_token = UINT64_C(1);
  machine->cpu.cp0[CP0_COMPARE] = UINT32_C(4);
  machine->cpu.cp0[CP0_STATUS] = STATUS_IE | STATUS_IM7;
  for (index = 0u; index < 8u; index++) {
    machine->ram[UINT32_C(0x200) + index] =
        (uint8_t)(dram_seed + (uint8_t)index);
  }
  if (t64_machine_trace_set_enabled(machine, true) != T64_STATUS_OK ||
      t64_machine_bus_write(machine, T1_TEST_PI_BASE, 4u,
                            UINT32_C(0x100)) != T64_STATUS_OK ||
      t64_machine_bus_write(machine, T1_TEST_PI_BASE + UINT32_C(4), 4u,
                            T1_TEST_CART_BASE + UINT32_C(0x100)) !=
          T64_STATUS_OK ||
      t64_machine_bus_write(machine, T1_TEST_PI_BASE + UINT32_C(12), 4u,
                            UINT32_C(13)) != T64_STATUS_OK ||
      t64_machine_bus_write(machine, T1_TEST_SP_BASE, 4u, 0u) !=
          T64_STATUS_OK ||
      t64_machine_bus_write(machine, T1_TEST_SP_BASE + UINT32_C(4), 4u,
                            UINT32_C(0x200)) != T64_STATUS_OK ||
      t64_machine_bus_write(machine, T1_TEST_SP_BASE + UINT32_C(8), 4u,
                            UINT32_C(7)) != T64_STATUS_OK ||
      t64_cpu_schedule(machine) != T64_STATUS_OK) {
    return false;
  }
  return true;
}

static void check_t1_dma_gap_state(const t64_machine *machine,
                                  const uint8_t image[4096],
                                  uint8_t dram_seed) {
  size_t index;
  bool cpu_successor_found = false;

  T64_CHECK_INT(machine->tick, UINT64_C(14));
  T64_CHECK_INT(machine->cpu.retired_instructions, 1u);
  T64_CHECK_INT(machine->instruction_count, 1u);
  T64_CHECK_INT(machine->cpu.last_pc_full,
                UINT64_C(0xffffffff80001000));
  T64_CHECK_INT(machine->cpu.last_opcode, UINT32_C(0x0022001e));
  T64_CHECK_INT(machine->cpu.hi, 0u);
  T64_CHECK_INT(machine->cpu.lo, 7u);
  T64_CHECK_INT(machine->cpu.cp0[CP0_RANDOM], 16u);
  T64_CHECK_INT(machine->rsp.pc, UINT32_C(4));
  T64_CHECK_INT(machine->rsp.gpr[1], UINT32_C(7));
  T64_CHECK_INT(machine->rsp.retired_instructions, UINT64_C(1));
  T64_CHECK((machine->rsp.sp_status & UINT32_C(1)) != 0u);
  T64_CHECK_INT(machine->cpu.cp0[CP0_COUNT], 7u);
  T64_CHECK_INT(machine->cpu.count_half_clock_phase, 0u);
  T64_CHECK_INT(machine->cpu.count_equality_latch_generation, 1u);
  T64_CHECK(!machine->cpu.count_equality_latched);
  T64_CHECK(((uint32_t)machine->cpu.cp0[CP0_CAUSE] & CAUSE_IP7) != 0u);
  T64_CHECK(!machine->pi.active_valid);
  T64_CHECK((machine->mi_pending & UINT32_C(0x10)) != 0u);
  T64_CHECK(!machine->sp.active.valid);
  T64_CHECK(!machine->sp.pending.valid);
  for (index = 0u; index < 14u; index++) {
    T64_CHECK_INT(machine->ram[UINT32_C(0x100) + index],
                  image[UINT32_C(0x100) + index]);
  }
  for (index = 0u; index < 8u; index++) {
    T64_CHECK_INT(machine->sp_dmem[index],
                  (uint8_t)(dram_seed + (uint8_t)index));
  }
  for (index = 0u; index < machine->queue_count; index++) {
    const size_t event_index =
        (machine->queue_head + index) % T64_EVENT_QUEUE_CAPACITY;
    const t64_event *event = &machine->events[event_index];

    if (event->device == T64_DEVICE_CPU &&
        event->kind == T64_CPU_EVENT_STEP) {
      cpu_successor_found = true;
      T64_CHECK_INT(event->tick, UINT64_C(71));
    }
  }
  T64_CHECK(cpu_successor_found);
}

static void test_cold_t1_mdu_gap_dispatches_dma_and_compare(void) {
  t64_machine *one_shot_a = NULL;
  t64_machine *partitioned_a = NULL;
  t64_machine *isolated_b = NULL;
  t64_machine *interleaved_b = NULL;
  uint8_t images[4][4096];
  uint8_t digest_one_shot_a[T64_DIGEST_SIZE];
  uint8_t digest_partitioned_a[T64_DIGEST_SIZE];
  uint8_t digest_isolated_b[T64_DIGEST_SIZE];
  uint8_t digest_interleaved_b[T64_DIGEST_SIZE];
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(14)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  size_t record_count;
  size_t pi_at_eight = SIZE_MAX;
  size_t rsp_at_eight = SIZE_MAX;
  size_t sp_complete_at_thirteen = SIZE_MAX;
  size_t index;

  T64_CHECK(setup_t1_dma_machine(
      &one_shot_a, "ntsc-cart-4m-openipl3-rdram-a-t1-v1", images[0],
      UINT8_C(0x80)));
  T64_CHECK(setup_t1_dma_machine(
      &partitioned_a, "ntsc-cart-4m-openipl3-rdram-a-t1-v1", images[1],
      UINT8_C(0x80)));
  T64_CHECK(setup_t1_dma_machine(
      &isolated_b, "ntsc-cart-4m-openipl3-rdram-b-t1-v1", images[2],
      UINT8_C(0x90)));
  T64_CHECK(setup_t1_dma_machine(
      &interleaved_b, "ntsc-cart-4m-openipl3-rdram-b-t1-v1", images[3],
      UINT8_C(0x90)));
  if (one_shot_a == NULL || partitioned_a == NULL || isolated_b == NULL ||
      interleaved_b == NULL || one_shot_a->ram == NULL ||
      partitioned_a->ram == NULL ||
      isolated_b->ram == NULL || interleaved_b->ram == NULL) {
    t64_machine_destroy(one_shot_a);
    t64_machine_destroy(partitioned_a);
    t64_machine_destroy(isolated_b);
    t64_machine_destroy(interleaved_b);
    return;
  }

  T64_CHECK_INT(t64_machine_advance(one_shot_a, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, UINT64_C(14));
  check_t1_dma_gap_state(one_shot_a, images[0], UINT8_C(0x80));
  /* The single-step ADDIU halts RSP after tick 1. It reserves opportunities
     through the SP DMA at tick 13 (12 + 8 / 8); completion leaves no tick-15
     issue event. */
  T64_CHECK_INT(one_shot_a->rsp.issue_sequence, UINT64_C(14));
  T64_CHECK_INT(one_shot_a->rsp.next_issue_tick, UINT64_C(14));
  T64_CHECK_INT(one_shot_a->rsp.dma_setup_remaining, 0u);
  T64_CHECK_INT(one_shot_a->rsp.dma_beat_cursor, 0u);
  T64_CHECK_INT(one_shot_a->rsp.active_dma_token, 0u);
  record_count = one_shot_a->trace_count;
  for (index = 0u; index < record_count; index++) {
    const size_t trace_index =
        (one_shot_a->trace_head + index) % T64_TRACE_RECORD_CAPACITY;
    const t64_trace_record *record =
        &one_shot_a->trace_records[trace_index];

    if (record->tick == UINT64_C(8) &&
        record->device == T64_DEVICE_PI &&
        record->kind == T64_TRACE_KIND_PI_COMPLETE) {
      pi_at_eight = index;
    }
    if (record->tick == UINT64_C(8) &&
        record->device == T64_DEVICE_SP &&
        record->kind == T64_SP_EVENT_RSP_ISSUE) {
      rsp_at_eight = index;
    }
    if (record->tick == UINT64_C(13) &&
        record->device == T64_DEVICE_SP &&
        record->kind == T1_TEST_SP_DMA_COMPLETE) {
      sp_complete_at_thirteen = index;
    }
  }
  T64_CHECK(pi_at_eight != SIZE_MAX);
  T64_CHECK(rsp_at_eight != SIZE_MAX);
  T64_CHECK(pi_at_eight < rsp_at_eight);
  T64_CHECK(sp_complete_at_thirteen != SIZE_MAX);

  request.value = UINT64_C(1);
  T64_CHECK_INT(t64_machine_advance(partitioned_a, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, UINT64_C(1));
  T64_CHECK_INT(partitioned_a->cpu.last_pc_full,
                UINT64_C(0xffffffff80001000));
  T64_CHECK_INT(partitioned_a->cpu.last_opcode, UINT32_C(0x0022001e));
  T64_CHECK_INT(partitioned_a->cpu.retired_instructions, UINT64_C(1));
  T64_CHECK_INT(partitioned_a->rsp_observed_tick, UINT64_C(1));
  T64_CHECK(partitioned_a->rsp_observation_valid);
  T64_CHECK_INT(partitioned_a->rsp_observed_pc, 0u);
  T64_CHECK_INT(partitioned_a->rsp_observed_opcode, UINT32_C(0x24010007));
  T64_CHECK_INT(partitioned_a->rsp_observed_outcome,
                T64_RSP_ATTEMPT_RETIRED);
  T64_CHECK_INT(partitioned_a->rsp.retired_instructions, UINT64_C(1));
  T64_CHECK_INT(partitioned_a->rsp.gpr[1], UINT32_C(7));
  request.value = UINT64_C(4);
  T64_CHECK_INT(t64_machine_advance(partitioned_a, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, UINT64_C(4));
  request.value = UINT64_C(8);
  T64_CHECK_INT(t64_machine_advance(partitioned_a, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, UINT64_C(8));
  T64_CHECK_INT(partitioned_a->cpu.cp0[CP0_COUNT], UINT32_C(4));
  T64_CHECK_INT(partitioned_a->cpu.count_half_clock_phase, 0u);
  T64_CHECK(partitioned_a->cpu.count_equality_latched);
  T64_CHECK_INT(partitioned_a->cpu.count_equality_latch_generation, 1u);
  T64_CHECK(((uint32_t)partitioned_a->cpu.cp0[CP0_CAUSE] & CAUSE_IP7) != 0u);
  T64_CHECK_INT(partitioned_a->pi.active_valid, false);
  T64_CHECK_INT(partitioned_a->rsp.dma_beat_cursor, 0u);
  request.value = UINT64_C(14);
  T64_CHECK_INT(t64_machine_advance(partitioned_a, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, UINT64_C(14));
  request.value = UINT64_C(14);
  T64_CHECK_INT(t64_machine_advance(isolated_b, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, UINT64_C(14));
  request.value = UINT64_C(1);
  T64_CHECK_INT(t64_machine_advance(interleaved_b, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, UINT64_C(1));
  T64_CHECK_INT(interleaved_b->rsp_observed_tick, UINT64_C(1));
  T64_CHECK(interleaved_b->rsp_observation_valid);
  T64_CHECK_INT(interleaved_b->rsp_observed_pc, 0u);
  T64_CHECK_INT(interleaved_b->rsp_observed_opcode, UINT32_C(0x24010007));
  T64_CHECK_INT(interleaved_b->rsp_observed_outcome,
                T64_RSP_ATTEMPT_RETIRED);
  T64_CHECK_INT(interleaved_b->rsp.retired_instructions, UINT64_C(1));
  T64_CHECK_INT(interleaved_b->rsp.gpr[1], UINT32_C(7));
  request.value = UINT64_C(5);
  T64_CHECK_INT(t64_machine_advance(interleaved_b, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, UINT64_C(5));
  request.value = UINT64_C(9);
  T64_CHECK_INT(t64_machine_advance(interleaved_b, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, UINT64_C(9));
  request.value = UINT64_C(14);
  T64_CHECK_INT(t64_machine_advance(interleaved_b, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, UINT64_C(14));
  check_t1_dma_gap_state(partitioned_a, images[1], UINT8_C(0x80));
  check_t1_dma_gap_state(isolated_b, images[2], UINT8_C(0x90));
  check_t1_dma_gap_state(interleaved_b, images[3], UINT8_C(0x90));
  T64_CHECK_INT(t64_machine_digest(one_shot_a, digest_one_shot_a),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(partitioned_a, digest_partitioned_a),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(isolated_b, digest_isolated_b),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(interleaved_b, digest_interleaved_b),
                T64_STATUS_OK);
  T64_CHECK(memcmp(digest_one_shot_a, digest_partitioned_a,
                   T64_DIGEST_SIZE) == 0);
  T64_CHECK(memcmp(digest_isolated_b, digest_interleaved_b,
                   T64_DIGEST_SIZE) == 0);

  request.value = one_shot_a->tick + UINT64_C(57);
  T64_CHECK_INT(t64_machine_advance(one_shot_a, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, UINT64_C(71));
  T64_CHECK_INT(one_shot_a->cpu.retired_instructions, 1u);
  T64_CHECK_INT(one_shot_a->cpu.attempt_outcome,
                T64_CPU_ATTEMPT_GUEST_EXCEPTION);
  T64_CHECK_INT(one_shot_a->cpu.last_pc_full,
                UINT64_C(0xffffffff80001004));
  T64_CHECK_INT(one_shot_a->cpu.last_opcode, 0u);
  T64_CHECK_INT(one_shot_a->cpu.cp0[CP0_EPC],
                UINT64_C(0xffffffff80001004));
  T64_CHECK(((uint32_t)one_shot_a->cpu.cp0[CP0_CAUSE] &
             UINT32_C(0x7c)) == 0u);
  T64_CHECK(((uint32_t)one_shot_a->cpu.cp0[CP0_STATUS] & STATUS_EXL) != 0u);
  T64_CHECK_INT(one_shot_a->cpu.cp0[CP0_RANDOM], 16u);
  /* RSP single-step is halted and both DMAs are complete; the sole queued
     event is the next CPU exception boundary after this one. */
  T64_CHECK_INT(one_shot_a->queue_count, 1u);
  if (one_shot_a->queue_count == 1u) {
    const t64_event *next = &one_shot_a->events[one_shot_a->queue_head];

    T64_CHECK_INT(next->device, T64_DEVICE_CPU);
    T64_CHECK_INT(next->kind, T64_CPU_EVENT_STEP);
    T64_CHECK_INT(next->tick, UINT64_C(72));
  }

  t64_machine_destroy(one_shot_a);
  t64_machine_destroy(partitioned_a);
  t64_machine_destroy(isolated_b);
  t64_machine_destroy(interleaved_b);
}

int main(void) {
  test_two_tick_boundaries_and_compare_clear();
  test_count_write_preserves_phase_and_checks_equality();
  test_interrupt_owners_and_masks();
  test_scheduler_idle_partition_and_max_tick();
  test_count_wrap_partition_and_initial_equal();
  test_interrupt_preempts_delay_slot_cp0_write();
  test_equal_tick_fifo_samples_new_irq_at_next_boundary();
  test_random_advances_only_on_retirement();
  test_cold_t1_mult_interval();
  test_cold_t1_delay_slot_interval();
  test_cold_t1_count_random_follow_elapsed_ticks();
  test_cold_t1_fetched_compare_clear();
  test_cold_t1_fetched_wired_random_tlbwr_sequence();
  test_cold_t1_high_wired_policy();
  test_cold_t1_exception_and_interrupt_boundaries();
  test_cold_t1_idle_count_compare();
  test_cold_t1_preflight_before_mutation();
  test_cold_t1_mdu_gap_dispatches_dma_and_compare();
  return T64_TEST_FINISH("unit.core.cpu-time");
}
