#include "t64_internal.h"
#include "t64_test.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CP0_RANDOM UINT32_C(1)
#define CP0_COUNT UINT32_C(9)
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

int main(void) {
  test_two_tick_boundaries_and_compare_clear();
  test_count_write_preserves_phase_and_checks_equality();
  test_interrupt_owners_and_masks();
  test_scheduler_idle_partition_and_max_tick();
  test_count_wrap_partition_and_initial_equal();
  test_interrupt_preempts_delay_slot_cp0_write();
  test_equal_tick_fifo_samples_new_irq_at_next_boundary();
  test_random_advances_only_on_retirement();
  return T64_TEST_FINISH("unit.core.cpu-time");
}
