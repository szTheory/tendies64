#include "t64_test.h"

#include "t64_internal.h"
#include "tendies64/t64_machine.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define IPL3_ASSET_BYTES 4096u
#define TEST_RI_BASE UINT32_C(0x04700000)
#define TEST_RI_CONFIG (TEST_RI_BASE + UINT32_C(4))
#define TEST_RI_CURRENT_LOAD (TEST_RI_BASE + UINT32_C(8))

typedef struct {
  t64_guest_device_state guest_devices;
  uint32_t mi_pending;
  uint32_t mi_mask;
  bool mi_irq_line;
} rdram_latch_snapshot;

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  void *memory;

  (void)context;
  if (storage_out == NULL || size == 0u || alignment == 0u ||
      alignment > _Alignof(max_align_t)) {
    return false;
  }
  memory = malloc(size);
  if (memory == NULL || ((uintptr_t)memory % alignment) != 0u) {
    free(memory);
    return false;
  }
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = _Alignof(max_align_t);
  return true;
}

static void test_release(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static void write_instruction(t64_machine *machine, uint32_t offset,
                              uint32_t instruction) {
  uint8_t *bytes = machine->sp_dmem + offset;

  bytes[0] = (uint8_t)(instruction >> 24);
  bytes[1] = (uint8_t)(instruction >> 16);
  bytes[2] = (uint8_t)(instruction >> 8);
  bytes[3] = (uint8_t)instruction;
}

static bool read_ipl3_asset(uint8_t bytes[IPL3_ASSET_BYTES]) {
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
  count = fread(bytes, 1u, IPL3_ASSET_BYTES, file);
  extra = fgetc(file);
  read_error = ferror(file);
  close_error = fclose(file);
  return count == IPL3_ASSET_BYTES && extra == EOF && read_error == 0 &&
         close_error == 0;
}

static bool create_machine(const char *profile_id,
                           t64_machine **machine_out) {
  const t64_host_allocator allocator = {test_allocate, test_release, NULL};
  const t64_profile_descriptor *profile = NULL;
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  t64_status status;

  if (profile_id == NULL || machine_out == NULL ||
      !read_ipl3_asset(ipl3_asset) ||
      t64_profile_lookup(profile_id, &profile) != T64_STATUS_OK ||
      profile == NULL ||
      t64_machine_create(&allocator, profile, machine_out) != T64_STATUS_OK ||
      *machine_out == NULL) {
    return false;
  }
  status = t64_machine_admit_rom(*machine_out, ipl3_asset,
                                 sizeof ipl3_asset);
  if (status != T64_STATUS_OK) {
    t64_machine_destroy(*machine_out);
    *machine_out = NULL;
    return false;
  }
  return true;
}

static rdram_latch_snapshot capture_latches(const t64_machine *machine) {
  rdram_latch_snapshot snapshot = {0};

  if (machine != NULL) {
    snapshot.guest_devices = machine->guest_devices;
    snapshot.mi_pending = machine->mi_pending;
    snapshot.mi_mask = machine->mi_mask;
    snapshot.mi_irq_line = machine->mi_irq_line;
  }
  return snapshot;
}

static void check_latches_unchanged(const t64_machine *machine,
                                   const rdram_latch_snapshot *before) {
  T64_CHECK(before != NULL);
  if (machine == NULL || before == NULL) {
    return;
  }
  T64_CHECK_INT(machine->guest_devices.policy_version,
                before->guest_devices.policy_version);
  T64_CHECK_INT(machine->guest_devices.mi_version,
                before->guest_devices.mi_version);
  T64_CHECK_INT(machine->guest_devices.ri_select,
                before->guest_devices.ri_select);
  T64_CHECK_INT(machine->guest_devices.mi_version_known,
                before->guest_devices.mi_version_known);
  T64_CHECK_INT(machine->guest_devices.ri_select_known,
                before->guest_devices.ri_select_known);
  T64_CHECK_INT(machine->guest_devices.rdram_protocol.ri_config,
                before->guest_devices.rdram_protocol.ri_config);
  T64_CHECK_INT(machine->guest_devices.rdram_protocol.ri_config_known,
                before->guest_devices.rdram_protocol.ri_config_known);
  T64_CHECK_INT(machine->guest_devices.rdram_protocol.ri_current_load,
                before->guest_devices.rdram_protocol.ri_current_load);
  T64_CHECK_INT(machine->guest_devices.rdram_protocol.ri_current_load_known,
                before->guest_devices.rdram_protocol.ri_current_load_known);
  T64_CHECK_INT(machine->guest_devices.rdram_protocol.ri_config_tick,
                before->guest_devices.rdram_protocol.ri_config_tick);
  T64_CHECK_INT(
      machine->guest_devices.rdram_protocol.ri_current_load_deadline_tick,
      before->guest_devices.rdram_protocol.ri_current_load_deadline_tick);
  T64_CHECK_INT(machine->mi_pending, before->mi_pending);
  T64_CHECK_INT(machine->mi_mask, before->mi_mask);
  T64_CHECK_INT(machine->mi_irq_line, before->mi_irq_line);
}

static void check_register_trace(t64_machine *machine) {
  static t64_trace_record records[T64_TRACE_RECORD_CAPACITY];
  size_t record_count = 0u;
  uint64_t dropped = 0u;
  bool drop_count_saturated = false;
  size_t index;
  uint32_t selector_reads = 0u;
  uint32_t config_writes = 0u;
  uint32_t config_reads = 0u;

  T64_CHECK_INT(t64_machine_trace_drain(machine, records,
                                        T64_TRACE_RECORD_CAPACITY,
                                        &record_count, &dropped,
                                        &drop_count_saturated),
                T64_STATUS_OK);
  T64_CHECK_INT(dropped, 0u);
  T64_CHECK(!drop_count_saturated);
  for (index = 0u; index < record_count; index++) {
    const t64_trace_record *record = &records[index];

    if (record->device != T64_TRACE_DEVICE_BUS) {
      continue;
    }
    if (record->kind == T64_TRACE_KIND_BUS_READ32 &&
        record->value == ((uint64_t)UINT32_C(0x0470000c) << 32)) {
      selector_reads++;
    }
    if (record->kind == T64_TRACE_KIND_BUS_WRITE32 &&
        record->value == (((uint64_t)TEST_RI_CONFIG << 32) | UINT64_C(0x40))) {
      config_writes++;
    }
    if (record->kind == T64_TRACE_KIND_BUS_READ32 &&
        record->value == (((uint64_t)TEST_RI_CONFIG << 32) | UINT64_C(0x40))) {
      config_reads++;
    }
  }
  T64_CHECK_INT(selector_reads, 1u);
  T64_CHECK_INT(config_writes, 1u);
  T64_CHECK_INT(config_reads, 1u);
}

static void test_new_command_is_refused_by_legacy_profile(void) {
  t64_machine *machine = NULL;
  rdram_latch_snapshot before;

  T64_CHECK(create_machine("ntsc-cart-4m-openipl3-vr4300-v1", &machine));
  if (machine == NULL) {
    return;
  }
  before = capture_latches(machine);
  T64_CHECK_INT(t64_bus_write(machine, TEST_RI_CONFIG, 4u, UINT32_C(0x40)),
                T64_STATUS_BUS_UNSUPPORTED);
  check_latches_unchanged(machine, &before);
  t64_machine_destroy(machine);
}

static void test_cold_command_requires_zero_selection(void) {
  t64_machine *machine = NULL;
  rdram_latch_snapshot before;

  T64_CHECK(create_machine("ntsc-cart-4m-openipl3-rdram-b-t1-v1", &machine));
  if (machine == NULL) {
    return;
  }
  /* Native fixture injection seeds an out-of-order state, not a reset fact. */
  machine->guest_devices.ri_select = 1u;
  before = capture_latches(machine);
  T64_CHECK_INT(t64_bus_write(machine, TEST_RI_CONFIG, 4u, UINT32_C(0x40)),
                T64_STATUS_BUS_DEVICE_REFUSED);
  check_latches_unchanged(machine, &before);
  t64_machine_destroy(machine);
}

static void test_prepared_profile_does_not_admit_cold_registers(void) {
  t64_machine *machine = NULL;
  rdram_latch_snapshot before;

  T64_CHECK(create_machine("ntsc-cart-4m-openipl3-rdram-a-t1-v1", &machine));
  if (machine == NULL) {
    return;
  }
  T64_CHECK_INT(machine->guest_devices.ri_select, 5u);
  before = capture_latches(machine);
  T64_CHECK_INT(t64_bus_write(machine, TEST_RI_CONFIG, 4u, UINT32_C(0x40)),
                T64_STATUS_BUS_UNSUPPORTED);
  check_latches_unchanged(machine, &before);
  t64_machine_destroy(machine);
}

static void test_current_load_after_source_wait(void) {
  const uint32_t start_pc = UINT32_C(0xa4000100);
  const uint32_t start_offset = UINT32_C(0x100);
  const uint32_t delay_instructions = UINT32_C(0x100);
  t64_machine *machine = NULL;
  uint64_t retired_before;
  uint32_t index;

  T64_CHECK(create_machine("ntsc-cart-4m-openipl3-rdram-b-t1-v1", &machine));
  if (machine == NULL) {
    return;
  }
  T64_CHECK_INT(t64_bus_write(machine, TEST_RI_CONFIG, 4u, UINT32_C(0x40)),
                T64_STATUS_OK);
  retired_before = machine->cpu.retired_instructions;
  for (index = 0u; index < delay_instructions; index++) {
    write_instruction(machine, start_offset + index * UINT32_C(4), 0u);
  }
  machine->cpu.pc = start_pc;
  machine->cpu.pc_full = UINT64_C(0xffffffffa4000100);

  /* The pinned source calls wait(0x100). The named v1 software model maps
     that interval to 256 guest ticks; it is not a hardware timing claim. */
  for (index = 0u; index < delay_instructions; index++) {
    const uint64_t target_tick = machine->tick + UINT64_C(1);
    const t64_stop_request request = {T64_STOP_GUEST_TICK, target_tick};
    t64_stop_result result = {UINT32_MAX, UINT64_MAX};

    T64_CHECK_INT(t64_machine_advance(machine, &request, &result),
                  T64_STATUS_OK);
    T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
    T64_CHECK_INT(result.tick, target_tick);
    T64_CHECK_INT(machine->cpu.last_pc_full,
                  UINT64_C(0xffffffffa4000100) +
                      (uint64_t)index * UINT64_C(4));
    T64_CHECK_INT(machine->cpu.attempt_outcome, T64_CPU_ATTEMPT_RETIRED);
    T64_CHECK_INT(machine->cpu.retired_instructions,
                  retired_before + (uint64_t)index + UINT64_C(1));
  }

  /* Source algorithm's next register operation applies the completed load. */
  T64_CHECK_INT(t64_bus_write(machine, TEST_RI_CURRENT_LOAD, 4u, 0u),
                T64_STATUS_OK);
  t64_machine_destroy(machine);
}

static void advance_to_guest_tick(t64_machine *machine, uint64_t target_tick) {
  const t64_stop_request request = {T64_STOP_GUEST_TICK, target_tick};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};

  T64_CHECK_INT(t64_machine_advance(machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, target_tick);
}

static void prepare_variable_cost_sequence(t64_machine *machine,
                                          uint32_t instruction_count) {
  uint32_t index;

  machine->queue_head = 0u;
  machine->queue_count = 0u;
  machine->next_sequence = 0u;
  machine->sequence_exhausted = false;
  machine->cpu.pc = UINT32_C(0xa4000100);
  machine->cpu.pc_full = UINT64_C(0xffffffffa4000100);
  machine->cpu.gpr[1] = UINT64_C(21);
  machine->cpu.gpr[2] = UINT64_C(3);
  for (index = 0u; index < instruction_count; index++) {
    write_instruction(machine, UINT32_C(0x100) + index * UINT32_C(4),
                      index == 0u ? UINT32_C(0x0022001a) : 0u);
  }
  T64_CHECK_INT(t64_cpu_schedule(machine), T64_STATUS_OK);
}

static void test_current_load_uses_guest_ticks_and_is_partition_invariant(
    void) {
  t64_machine *idle = NULL;
  t64_machine *one_shot = NULL;
  t64_machine *partitioned = NULL;
  uint64_t deadline;
  uint64_t retired_before;
  rdram_latch_snapshot before;

  T64_CHECK(create_machine("ntsc-cart-4m-openipl3-rdram-b-t1-v1", &idle));
  T64_CHECK(create_machine("ntsc-cart-4m-openipl3-rdram-b-t1-v1", &one_shot));
  T64_CHECK(create_machine("ntsc-cart-4m-openipl3-rdram-b-t1-v1",
                           &partitioned));
  if (idle == NULL || one_shot == NULL || partitioned == NULL) {
    t64_machine_destroy(idle);
    t64_machine_destroy(one_shot);
    t64_machine_destroy(partitioned);
    return;
  }

  T64_CHECK_INT(t64_bus_write(idle, TEST_RI_CONFIG, 4u, UINT32_C(0x40)),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_bus_write(one_shot, TEST_RI_CONFIG, 4u, UINT32_C(0x40)),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_bus_write(partitioned, TEST_RI_CONFIG, 4u,
                              UINT32_C(0x40)),
                T64_STATUS_OK);
  deadline = idle->guest_devices.rdram_protocol.ri_current_load_deadline_tick;
  T64_CHECK_INT(deadline, idle->tick + UINT64_C(0x100));

  /* Empty queues let guest time advance while the CPU is idle. The rejected
     just-before-deadline write must preserve every captured device latch. */
  idle->queue_head = 0u;
  idle->queue_count = 0u;
  advance_to_guest_tick(idle, deadline - UINT64_C(1));
  before = capture_latches(idle);
  T64_CHECK_INT(t64_bus_write(idle, TEST_RI_CURRENT_LOAD, 4u, 0u),
                T64_STATUS_BUS_DEVICE_REFUSED);
  check_latches_unchanged(idle, &before);
  advance_to_guest_tick(idle, deadline);
  T64_CHECK_INT(t64_bus_write(idle, TEST_RI_CURRENT_LOAD, 4u, 0u),
                T64_STATUS_OK);
  T64_CHECK_INT(idle->cpu.retired_instructions, 0u);

  /* A variable-cost authored DIV delays retirement while guest ticks pass.
     One-shot and partitioned advances must reach the same deadline state. */
  retired_before = one_shot->cpu.retired_instructions;
  prepare_variable_cost_sequence(one_shot, 256u);
  prepare_variable_cost_sequence(partitioned, 256u);
  advance_to_guest_tick(one_shot, deadline);
  advance_to_guest_tick(partitioned, deadline - UINT64_C(73));
  advance_to_guest_tick(partitioned, deadline);
  T64_CHECK(one_shot->cpu.retired_instructions - retired_before < 256u);
  T64_CHECK_INT(one_shot->tick, partitioned->tick);
  T64_CHECK_INT(one_shot->cpu.retired_instructions,
                partitioned->cpu.retired_instructions);
  T64_CHECK_INT(one_shot->cpu.pc_full, partitioned->cpu.pc_full);
  T64_CHECK_INT(one_shot->cpu.last_pc_full, partitioned->cpu.last_pc_full);
  T64_CHECK_INT(one_shot->cpu.last_opcode, partitioned->cpu.last_opcode);
  T64_CHECK_INT(one_shot->cpu.hi, partitioned->cpu.hi);
  T64_CHECK_INT(one_shot->cpu.lo, partitioned->cpu.lo);
  T64_CHECK_INT(one_shot->queue_count, partitioned->queue_count);
  T64_CHECK_INT(t64_bus_write(one_shot, TEST_RI_CURRENT_LOAD, 4u, 0u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_bus_write(partitioned, TEST_RI_CURRENT_LOAD, 4u, 0u),
                T64_STATUS_OK);

  t64_machine_destroy(idle);
  t64_machine_destroy(one_shot);
  t64_machine_destroy(partitioned);
}

static void test_first_cold_register_instruction(void) {
  static const uint32_t instructions[] = {
      UINT32_C(0x8c42000c), /* lw v0,12(v0), the retained RI_SELECT word. */
      UINT32_C(0x3c08a470), /* lui t0,0xa470 */
      UINT32_C(0x34090040), /* ori t1,zero,0x0040 */
      UINT32_C(0xad090004), /* sw t1,4(t0): source-derived RI_CONFIG write. */
      UINT32_C(0x8d0b0004)  /* lw t3,4(t0): versioned model readback. */
  };
  const uint32_t start_pc = UINT32_C(0xa4000080);
  const uint32_t start_offset = UINT32_C(0x80);
  t64_machine *machine = NULL;
  rdram_latch_snapshot before;
  uint32_t value32 = UINT32_C(0xfeedface);
  uint64_t value64 = UINT64_C(0x1122334455667788);
  size_t index;

  T64_CHECK(create_machine("ntsc-cart-4m-openipl3-rdram-b-t1-v1", &machine));
  if (machine == NULL) {
    return;
  }

  /* The admitted Tier-B value is a profile seed, not a reset measurement. */
  T64_CHECK_INT(machine->guest_devices.ri_select, 0u);
  T64_CHECK(machine->guest_devices.ri_select_known);
  before = capture_latches(machine);
  T64_CHECK_INT(t64_bus_read(machine, TEST_RI_CONFIG, 4u, &value32),
                T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(value32, UINT32_C(0xfeedface));
  check_latches_unchanged(machine, &before);
  T64_CHECK_INT(t64_machine_trace_set_enabled(machine, true), T64_STATUS_OK);
  for (index = 0u; index < sizeof instructions / sizeof instructions[0];
       index++) {
    write_instruction(machine,
                      start_offset + (uint32_t)(index * sizeof(uint32_t)),
                      instructions[index]);
  }
  machine->cpu.pc = start_pc;
  machine->cpu.pc_full = UINT64_C(0xffffffffa4000080);
  machine->cpu.gpr[0] = 0u;
  machine->cpu.gpr[2] = UINT64_C(0xffffffffa4700000);
  machine->cpu.gpr[8] = 0u;
  machine->cpu.gpr[9] = 0u;

  /* The observed selector read still executes in this independent sequence. */
  T64_CHECK_INT(t64_cpu_step(machine, NULL), T64_STATUS_OK);
  T64_CHECK_INT(machine->cpu.last_pc_full,
                UINT64_C(0xffffffffa4000080));
  T64_CHECK_INT(machine->cpu.last_opcode, UINT32_C(0x8c42000c));
  T64_CHECK_INT(machine->cpu.attempt_outcome, T64_CPU_ATTEMPT_RETIRED);
  T64_CHECK_INT(machine->cpu.gpr[2], 0u);
  T64_CHECK_INT(machine->cpu.retired_instructions, 1u);

  T64_CHECK_INT(t64_cpu_step(machine, NULL), T64_STATUS_OK);
  T64_CHECK_INT(t64_cpu_step(machine, NULL), T64_STATUS_OK);
  T64_CHECK_INT(machine->cpu.gpr[8], UINT64_C(0xffffffffa4700000));
  T64_CHECK_INT(machine->cpu.gpr[9], UINT64_C(0x40));

  T64_CHECK_INT(t64_cpu_step(machine, NULL), T64_STATUS_OK);
  T64_CHECK_INT(machine->cpu.last_pc_full,
                UINT64_C(0xffffffffa400008c));
  T64_CHECK_INT(machine->cpu.last_opcode, UINT32_C(0xad090004));
  T64_CHECK_INT(machine->cpu.attempt_outcome, T64_CPU_ATTEMPT_RETIRED);
  T64_CHECK_INT(machine->cpu.retired_instructions, 4u);
  T64_CHECK_INT(machine->guest_devices.rdram_protocol.ri_config, 0x40u);
  T64_CHECK(machine->guest_devices.rdram_protocol.ri_config_known);

  T64_CHECK_INT(t64_cpu_step(machine, NULL), T64_STATUS_OK);
  T64_CHECK_INT(machine->cpu.last_pc_full,
                UINT64_C(0xffffffffa4000090));
  T64_CHECK_INT(machine->cpu.last_opcode, UINT32_C(0x8d0b0004));
  T64_CHECK_INT(machine->cpu.gpr[11], UINT64_C(0x40));
  T64_CHECK_INT(machine->cpu.attempt_outcome, T64_CPU_ATTEMPT_RETIRED);
  T64_CHECK_INT(machine->cpu.retired_instructions, 5u);
  check_register_trace(machine);

  before = capture_latches(machine);
  T64_CHECK_INT(t64_bus_write(machine, TEST_RI_CONFIG, 2u, UINT32_C(0x40)),
                T64_STATUS_BUS_UNSUPPORTED);
  check_latches_unchanged(machine, &before);

  T64_CHECK_INT(t64_bus_write(machine, TEST_RI_CONFIG, 4u, UINT32_C(0x41)),
                T64_STATUS_BUS_DEVICE_REFUSED);
  check_latches_unchanged(machine, &before);

  T64_CHECK_INT(t64_bus_read_wide(machine, TEST_RI_BASE, &value64),
                T64_STATUS_BUS_DEVICE_REFUSED);
  T64_CHECK_INT(value64, UINT64_C(0x1122334455667788));
  check_latches_unchanged(machine, &before);

  t64_machine_destroy(machine);
}

int main(void) {
  test_new_command_is_refused_by_legacy_profile();
  test_cold_command_requires_zero_selection();
  test_prepared_profile_does_not_admit_cold_registers();
  test_current_load_after_source_wait();
  test_current_load_uses_guest_ticks_and_is_partition_invariant();
  test_first_cold_register_instruction();
  return T64_TEST_FINISH("unit.core.rdram-protocol");
}
