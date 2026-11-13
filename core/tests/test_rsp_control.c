#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_SP_BASE UINT32_C(0x04040000)
#define TEST_DMEM_BASE UINT32_C(0x04000000)
#define TEST_IMEM_BASE UINT32_C(0x04001000)

typedef struct {
  t64_host_allocator allocator;
  t64_machine *machine;
  size_t allocations;
  size_t releases;
} rsp_control_fixture;

typedef struct {
  bool setup_ok;
  bool unchanged;
  uint32_t pc;
  uint32_t word_before;
  uint32_t word_after;
  uint32_t refusal_reason;
  t64_status write_status;
} active_pc_write_observation;

static bool fixture_allocate(void *context, size_t size, size_t alignment,
                             t64_host_storage *storage_out) {
  rsp_control_fixture *fixture = (rsp_control_fixture *)context;
  void *data;

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
  if (fixture != NULL) {
    fixture->allocations++;
  }
  return true;
}

static void fixture_release(void *context, t64_host_storage storage) {
  rsp_control_fixture *fixture = (rsp_control_fixture *)context;

  if (fixture != NULL) {
    fixture->releases++;
  }
  free(storage.data);
}

static bool fixture_create(rsp_control_fixture *fixture) {
  const t64_profile_descriptor *profile = NULL;
  t64_host_storage image = {NULL, 0u, 0u};
  t64_status status;

  if (fixture == NULL) {
    return false;
  }
  (void)memset(fixture, 0, sizeof *fixture);
  fixture->allocator.allocate = fixture_allocate;
  fixture->allocator.release = fixture_release;
  fixture->allocator.context = fixture;
  status = t64_profile_lookup(
      "ntsc-cart-4m-openipl3-rsp-functional-v1", &profile);
  if (status != T64_STATUS_OK || profile == NULL) {
    fprintf(stderr, "fixture profile lookup failed: status=%u\n",
            (unsigned)status);
    return false;
  }
  status = t64_machine_create(&fixture->allocator, profile,
                              &fixture->machine);
  if (status != T64_STATUS_OK || fixture->machine == NULL) {
    fprintf(stderr, "fixture machine create failed: status=%u\n",
            (unsigned)status);
    return false;
  }
  if (!fixture_allocate(fixture, 2u, 1u, &image)) {
    fprintf(stderr, "fixture synthetic image allocation failed\n");
    t64_machine_destroy(fixture->machine);
    fixture->machine = NULL;
    return false;
  }
  ((uint8_t *)image.data)[0] = UINT8_C(0x5a);
  ((uint8_t *)image.data)[1] = UINT8_C(0xa5);
  fixture->machine->image_storage = image;
  fixture->machine->image_length = 2u;
  fixture->machine->has_image = true;
  return true;
}

static bool advance_to(t64_machine *machine, uint64_t tick) {
  t64_stop_request request = {T64_STOP_GUEST_TICK, tick};
  t64_stop_result result = {0u, 0u};

  return t64_machine_advance(machine, &request, &result) == T64_STATUS_OK &&
         result.tick == tick;
}

static void reset_rsp_fixture(rsp_control_fixture *fixture, bool halted) {
  t64_machine_reset_execution_state(fixture->machine);
  (void)memset(&fixture->machine->rsp, 0, sizeof fixture->machine->rsp);
  (void)memset(&fixture->machine->cpu, 0, sizeof fixture->machine->cpu);
  fixture->machine->rsp.sp_status =
      halted ? T64_RSP_STATUS_HALT : UINT32_C(0);
  fixture->machine->mi_pending = 0u;
  fixture->machine->mi_mask = 0u;
  fixture->machine->mi_irq_line = false;
}

static bool machine_digest_equal(t64_machine *machine,
                                 const uint8_t expected[T64_DIGEST_SIZE]) {
  uint8_t actual[T64_DIGEST_SIZE];

  return t64_machine_digest(machine, actual) == T64_STATUS_OK &&
         memcmp(actual, expected, sizeof actual) == 0;
}

static bool read_sp_register(t64_machine *machine, uint32_t offset,
                             uint32_t *value_out) {
  return t64_machine_bus_read(machine, TEST_SP_BASE + offset, 4u,
                              value_out) == T64_STATUS_OK;
}

static bool write_imem_word(t64_machine *machine, uint32_t pc,
                            uint32_t instruction) {
  return t64_machine_bus_write(machine, TEST_IMEM_BASE + pc, 4u,
                               instruction) == T64_STATUS_OK;
}

static bool active_pc_imem_write_refusal_test(
    active_pc_write_observation *observation) {
  rsp_control_fixture fixture;
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  uint32_t imem_before = 0u;
  uint32_t imem_after = 0u;
  uint32_t refusal_reason = T64_RSP_REFUSAL_NONE;
  t64_status setup_status = T64_STATUS_INVALID_ARGUMENT;
  t64_status write_status = T64_STATUS_INVALID_ARGUMENT;
  t64_status digest_before_status = T64_STATUS_INVALID_ARGUMENT;
  t64_status digest_after_status = T64_STATUS_INVALID_ARGUMENT;
  bool unchanged = false;
  bool setup_ok;

  if (observation == NULL) {
    return false;
  }
  (void)memset(observation, 0, sizeof *observation);
  observation->write_status = T64_STATUS_INVALID_ARGUMENT;
  setup_ok = fixture_create(&fixture);
  if (setup_ok) {
    t64_machine_reset_execution_state(fixture.machine);
    fixture.machine->rsp.sp_status = T64_RSP_STATUS_HALT;
    setup_status =
        t64_machine_bus_write(fixture.machine, TEST_IMEM_BASE, 4u, 0u);
    setup_ok = setup_status == T64_STATUS_OK &&
               t64_machine_bus_write(fixture.machine, TEST_IMEM_BASE + 4u,
                                     4u, UINT32_C(0x24020009)) ==
                   T64_STATUS_OK &&
               t64_machine_bus_read(fixture.machine, TEST_IMEM_BASE + 4u,
                                    4u, &imem_before) == T64_STATUS_OK &&
               t64_rsp_start(fixture.machine) == T64_STATUS_OK &&
               advance_to(fixture.machine, UINT64_C(1)) &&
               fixture.machine->rsp.pc == UINT32_C(4) &&
               (fixture.machine->rsp.sp_status & T64_RSP_STATUS_HALT) == 0u;
  }
  if (setup_ok) {
    observation->pc = fixture.machine->rsp.pc;
    digest_before_status =
        t64_machine_digest(fixture.machine, digest_before);
    write_status = t64_machine_bus_write(
        fixture.machine, TEST_IMEM_BASE + UINT32_C(4), 4u,
        UINT32_C(0x0000000d));
    digest_after_status =
        t64_machine_digest(fixture.machine, digest_after);
    refusal_reason = fixture.machine->rsp_observed_refusal_reason;
    if (t64_machine_bus_read(fixture.machine, TEST_IMEM_BASE + 4u, 4u,
                             &imem_after) == T64_STATUS_OK &&
        digest_before_status == T64_STATUS_OK &&
        digest_after_status == T64_STATUS_OK) {
      unchanged = imem_before == imem_after &&
                  memcmp(digest_before, digest_after, sizeof digest_before) ==
                      0;
    }
  }
  observation->setup_ok = setup_ok;
  observation->word_before = imem_before;
  observation->word_after = imem_after;
  observation->write_status = write_status;
  observation->refusal_reason = refusal_reason;
  observation->unchanged = unchanged;
  if (fixture.machine != NULL) {
    t64_machine_destroy(fixture.machine);
  }
  return setup_ok && write_status == T64_STATUS_BUS_DEVICE_REFUSED &&
         refusal_reason == T64_RSP_REFUSAL_ACTIVE_IMEM_WRITE && unchanged;
}

typedef struct {
  uint32_t status_bit;
  uint32_t clear_command;
  uint32_t set_command;
  bool mi_interrupt;
} rsp_status_command_case;

static bool status_command_matrix_test(void) {
  static const rsp_status_command_case cases[] = {
      {T64_RSP_STATUS_HALT, UINT32_C(0x1), UINT32_C(0x2), false},
      {T64_RSP_STATUS_BROKE, UINT32_C(0x4), UINT32_C(0), false},
      {UINT32_C(0), UINT32_C(0x8), UINT32_C(0x10), true},
      {T64_RSP_STATUS_SINGLE_STEP, UINT32_C(0x20), UINT32_C(0x40), false},
      {T64_RSP_STATUS_INTR_BREAK, UINT32_C(0x80), UINT32_C(0x100), false},
      {UINT32_C(1) << 7, UINT32_C(1) << 9, UINT32_C(1) << 10, false},
      {UINT32_C(1) << 8, UINT32_C(1) << 11, UINT32_C(1) << 12, false},
      {UINT32_C(1) << 9, UINT32_C(1) << 13, UINT32_C(1) << 14, false},
      {UINT32_C(1) << 10, UINT32_C(1) << 15, UINT32_C(1) << 16, false},
      {UINT32_C(1) << 11, UINT32_C(1) << 17, UINT32_C(1) << 18, false},
      {UINT32_C(1) << 12, UINT32_C(1) << 19, UINT32_C(1) << 20, false},
      {UINT32_C(1) << 13, UINT32_C(1) << 21, UINT32_C(1) << 22, false},
      {UINT32_C(1) << 14, UINT32_C(1) << 23, UINT32_C(1) << 24, false}};
  const uint32_t all_status_bits = T64_RSP_STATUS_HALT |
                                   T64_RSP_STATUS_BROKE |
                                   T64_RSP_STATUS_SINGLE_STEP |
                                   T64_RSP_STATUS_INTR_BREAK |
                                   T64_RSP_STATUS_SIGNAL_MASK;
  rsp_control_fixture fixture;
  uint32_t readback = 0u;
  size_t index;
  bool passed = fixture_create(&fixture);

  if (!passed) {
    return false;
  }
  for (index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
    uint32_t initial_status = all_status_bits;
    uint32_t expected_status;

    reset_rsp_fixture(&fixture, true);
    fixture.machine->rsp.sp_status = initial_status;
    if (cases[index].mi_interrupt) {
      t64_mi_set_pending(fixture.machine, UINT32_C(1));
    }
    if (!read_sp_register(fixture.machine, UINT32_C(0x10), &readback) ||
        readback != initial_status ||
        (cases[index].mi_interrupt && fixture.machine->mi_pending != 1u)) {
      passed = false;
      break;
    }
    if (t64_machine_bus_write(fixture.machine, TEST_SP_BASE + 0x10u, 4u,
                              cases[index].clear_command) != T64_STATUS_OK) {
      passed = false;
      break;
    }
    expected_status = initial_status & ~cases[index].status_bit;
    if (cases[index].mi_interrupt) {
      expected_status = initial_status;
      if (fixture.machine->mi_pending != 0u) {
        passed = false;
        break;
      }
    }
    if (!read_sp_register(fixture.machine, UINT32_C(0x10), &readback) ||
        readback != expected_status) {
      passed = false;
      break;
    }
    if (cases[index].set_command != 0u) {
      reset_rsp_fixture(&fixture, true);
      fixture.machine->rsp.sp_status = T64_RSP_STATUS_HALT;
      if (cases[index].mi_interrupt) {
        fixture.machine->mi_pending = 0u;
      }
      if (t64_machine_bus_write(fixture.machine, TEST_SP_BASE + 0x10u, 4u,
                                cases[index].set_command) != T64_STATUS_OK) {
        passed = false;
        break;
      }
      expected_status = T64_RSP_STATUS_HALT | cases[index].status_bit;
      if (cases[index].mi_interrupt) {
        expected_status = T64_RSP_STATUS_HALT;
        if (fixture.machine->mi_pending != 1u) {
          passed = false;
          break;
        }
      }
      if (!read_sp_register(fixture.machine, UINT32_C(0x10), &readback) ||
          readback != expected_status) {
        passed = false;
        break;
      }
      reset_rsp_fixture(&fixture, true);
      fixture.machine->rsp.sp_status =
          T64_RSP_STATUS_HALT | cases[index].status_bit;
      if (cases[index].mi_interrupt) {
        t64_mi_set_pending(fixture.machine, UINT32_C(1));
      }
      if (t64_machine_bus_write(
              fixture.machine, TEST_SP_BASE + 0x10u, 4u,
              cases[index].clear_command | cases[index].set_command) !=
          T64_STATUS_OK) {
        passed = false;
        break;
      }
      if (cases[index].mi_interrupt) {
        if (fixture.machine->mi_pending != 1u) {
          passed = false;
          break;
        }
      } else if (!read_sp_register(fixture.machine, UINT32_C(0x10),
                                   &readback) ||
                 readback != (T64_RSP_STATUS_HALT | cases[index].status_bit)) {
        passed = false;
        break;
      }
    }
  }
  if (passed) {
    reset_rsp_fixture(&fixture, true);
    fixture.machine->rsp.sp_status |= T64_RSP_STATUS_BROKE;
    fixture.machine->mi_pending = UINT32_C(1);
    {
      uint8_t before[T64_DIGEST_SIZE];

      passed = t64_machine_digest(fixture.machine, before) == T64_STATUS_OK &&
               t64_machine_bus_write(fixture.machine, TEST_SP_BASE + 0x10u,
                                     4u, UINT32_C(1) << 25) ==
                   T64_STATUS_BUS_UNSUPPORTED &&
               machine_digest_equal(fixture.machine, before);
    }
  }
  t64_machine_destroy(fixture.machine);
  return passed;
}

static bool semaphore_and_pc_wrap_test(void) {
  rsp_control_fixture fixture;
  uint32_t value = UINT32_MAX;
  bool passed = fixture_create(&fixture);

  if (!passed) {
    return false;
  }
  reset_rsp_fixture(&fixture, true);
  passed =
      t64_machine_bus_read(fixture.machine, TEST_SP_BASE + 0x1cu, 1u,
                           &value) == T64_STATUS_BUS_UNSUPPORTED &&
      !fixture.machine->rsp.sp_semaphore &&
      t64_sp_peek_register(fixture.machine, TEST_SP_BASE + 0x1cu, &value) ==
          T64_STATUS_OK &&
      value == 0u && !fixture.machine->rsp.sp_semaphore &&
      read_sp_register(fixture.machine, UINT32_C(0x1c), &value) &&
           value == 0u && fixture.machine->rsp.sp_semaphore;
  passed = passed &&
           read_sp_register(fixture.machine, UINT32_C(0x1c), &value) &&
           value == 1u;
  passed = passed &&
           t64_sp_peek_register(fixture.machine, TEST_SP_BASE + 0x1cu,
                                &value) == T64_STATUS_OK &&
           value == 1u && fixture.machine->rsp.sp_semaphore;
  passed = passed &&
           t64_machine_bus_write(fixture.machine, TEST_SP_BASE + 0x1cu, 4u,
                                 0u) == T64_STATUS_OK &&
           !fixture.machine->rsp.sp_semaphore &&
           read_sp_register(fixture.machine, UINT32_C(0x1c), &value) &&
           value == 0u;
  passed = passed &&
           t64_machine_bus_write(fixture.machine, T64_SP_PC_ADDRESS, 4u,
                                 UINT32_C(0x1008)) == T64_STATUS_OK &&
           fixture.machine->rsp.pc == UINT32_C(8) &&
           t64_machine_bus_read(fixture.machine, T64_SP_PC_ADDRESS, 4u,
                                &value) == T64_STATUS_OK &&
           value == UINT32_C(8);
  passed = passed &&
           t64_machine_bus_write(fixture.machine, T64_SP_PC_ADDRESS, 4u,
                                 UINT32_C(0x1fff)) == T64_STATUS_OK &&
           fixture.machine->rsp.pc == UINT32_C(0xffc);
  passed = passed && write_imem_word(fixture.machine, UINT32_C(0xffc), 0u) &&
           write_imem_word(fixture.machine, 0u, UINT32_C(0x24020009)) &&
           t64_rsp_start(fixture.machine) == T64_STATUS_OK &&
           advance_to(fixture.machine, UINT64_C(1)) &&
           fixture.machine->rsp.pc == 0u &&
           advance_to(fixture.machine, UINT64_C(2)) &&
           fixture.machine->rsp.gpr[2] == UINT32_C(9) &&
           fixture.machine->rsp.pc == UINT32_C(4);
  if (passed) {
    uint8_t before[T64_DIGEST_SIZE];
    t64_status status;

    passed = t64_machine_digest(fixture.machine, before) == T64_STATUS_OK;
    status = t64_machine_bus_write(fixture.machine, T64_SP_PC_ADDRESS, 4u,
                                   UINT32_C(0x20));
    passed = passed && status == T64_STATUS_BUS_DEVICE_REFUSED &&
             fixture.machine->rsp_observed_refusal_reason ==
                 T64_RSP_REFUSAL_RUNNING_PC_WRITE &&
             machine_digest_equal(fixture.machine, before);
  }
  t64_machine_destroy(fixture.machine);
  return passed;
}

static bool in_flight_halt_and_single_step_refusal_test(void) {
  rsp_control_fixture fixture;
  uint8_t before[T64_DIGEST_SIZE];
  t64_status status;
  bool passed = fixture_create(&fixture);

  if (!passed) {
    return false;
  }
  reset_rsp_fixture(&fixture, true);
  passed = write_imem_word(fixture.machine, 0u, 0u) &&
           t64_machine_bus_write(fixture.machine, TEST_SP_BASE + 0x10u, 4u,
                                 UINT32_C(1)) == T64_STATUS_OK &&
           (fixture.machine->rsp.sp_status & T64_RSP_STATUS_HALT) == 0u &&
           fixture.machine->rsp.next_issue_tick == UINT64_C(1) &&
           t64_machine_digest(fixture.machine, before) == T64_STATUS_OK;
  if (passed) {
    status = t64_machine_bus_write(fixture.machine, TEST_SP_BASE + 0x10u,
                                   4u, UINT32_C(2));
    passed = status == T64_STATUS_BUS_DEVICE_REFUSED &&
             fixture.machine->rsp_observed_refusal_reason ==
                 T64_RSP_REFUSAL_ASYNC_HALT &&
             machine_digest_equal(fixture.machine, before);
  }
  if (passed) {
    status = t64_machine_bus_write(fixture.machine, TEST_SP_BASE + 0x10u,
                                   4u, UINT32_C(0x40));
    passed = status == T64_STATUS_BUS_DEVICE_REFUSED &&
             fixture.machine->rsp_observed_refusal_reason ==
                 T64_RSP_REFUSAL_SINGLE_STEP_IN_FLIGHT &&
             machine_digest_equal(fixture.machine, before);
  }
  t64_machine_destroy(fixture.machine);
  return passed;
}

static bool load_store_bubble_test(void) {
  static const uint32_t program[] = {
      UINT32_C(0x24010000), /* ADDIU r1,r0,0 */
      UINT32_C(0x8c220000), /* LW r2,0(r1) */
      UINT32_C(0x00000000), /* one intervening issue slot */
      UINT32_C(0xac230004), /* SW r3,4(r1), independent of loaded r2 */
      UINT32_C(0x0000000d)  /* BREAK */
  };
  rsp_control_fixture fixture;
  uint32_t byte;
  size_t index;
  bool passed = fixture_create(&fixture);

  if (!passed) {
    return false;
  }
  reset_rsp_fixture(&fixture, true);
  fixture.machine->rsp.gpr[3] = UINT32_C(0x89abcdef);
  fixture.machine->sp_dmem[0] = UINT8_C(0x12);
  fixture.machine->sp_dmem[1] = UINT8_C(0x34);
  fixture.machine->sp_dmem[2] = UINT8_C(0x56);
  fixture.machine->sp_dmem[3] = UINT8_C(0x78);
  for (index = 0u; index < sizeof program / sizeof program[0]; index++) {
    passed = passed &&
             write_imem_word(fixture.machine, (uint32_t)(index * 4u),
                             program[index]);
  }
  passed = passed && t64_rsp_start(fixture.machine) == T64_STATUS_OK &&
           advance_to(fixture.machine, UINT64_C(4)) &&
           fixture.machine->rsp.pc == UINT32_C(12) &&
           fixture.machine->rsp.retired_instructions == UINT64_C(3) &&
           fixture.machine->rsp.stall_slots == UINT64_C(1) &&
           fixture.machine->rsp_observed_stall_reason ==
               T64_RSP_STALL_LOAD_STORE &&
           fixture.machine->rsp_observed_ready_tick == UINT64_C(5) &&
           fixture.machine->sp_dmem[4] == 0u &&
           fixture.machine->sp_dmem[5] == 0u &&
           fixture.machine->sp_dmem[6] == 0u &&
           fixture.machine->sp_dmem[7] == 0u;
  passed = passed && advance_to(fixture.machine, UINT64_C(5)) &&
           fixture.machine->rsp.pc == UINT32_C(16) &&
           fixture.machine->rsp.retired_instructions == UINT64_C(4);
  byte = UINT32_C(0x89abcdef);
  passed = passed && fixture.machine->sp_dmem[4] == (uint8_t)(byte >> 24) &&
           fixture.machine->sp_dmem[5] == (uint8_t)(byte >> 16) &&
           fixture.machine->sp_dmem[6] == (uint8_t)(byte >> 8) &&
           fixture.machine->sp_dmem[7] == (uint8_t)byte;
  t64_machine_destroy(fixture.machine);
  return passed;
}

static bool active_pc_write_widths_test(void) {
  rsp_control_fixture fixture;
  uint8_t before[T64_DIGEST_SIZE];
  t64_status status;
  bool passed = fixture_create(&fixture);

  if (!passed) {
    return false;
  }
  reset_rsp_fixture(&fixture, true);
  passed = write_imem_word(fixture.machine, 0u, 0u) &&
           write_imem_word(fixture.machine, 4u, UINT32_C(0x24020009)) &&
           t64_rsp_start(fixture.machine) == T64_STATUS_OK &&
           advance_to(fixture.machine, UINT64_C(1)) &&
           fixture.machine->rsp.pc == UINT32_C(4) &&
           t64_machine_digest(fixture.machine, before) == T64_STATUS_OK;
  if (passed) {
    status = t64_machine_bus_write(fixture.machine, TEST_IMEM_BASE + 7u, 1u,
                                   UINT32_C(0xaa));
    passed = status == T64_STATUS_BUS_DEVICE_REFUSED &&
             fixture.machine->rsp_observed_refusal_reason ==
                 T64_RSP_REFUSAL_ACTIVE_IMEM_WRITE &&
             machine_digest_equal(fixture.machine, before);
  }
  if (passed) {
    status = t64_bus_write_masked(fixture.machine, TEST_IMEM_BASE + 4u, 4u,
                                  UINT8_C(0x8), UINT32_C(0x000000aa));
    passed = status == T64_STATUS_BUS_DEVICE_REFUSED &&
             fixture.machine->rsp_observed_refusal_reason ==
                 T64_RSP_REFUSAL_ACTIVE_IMEM_WRITE &&
             machine_digest_equal(fixture.machine, before);
  }
  if (passed) {
    status = t64_bus_write_masked(fixture.machine, TEST_IMEM_BASE, 8u,
                                  UINT8_C(0x10), UINT64_C(0xaa00000000));
    passed = status == T64_STATUS_BUS_DEVICE_REFUSED &&
             fixture.machine->rsp_observed_refusal_reason ==
                 T64_RSP_REFUSAL_ACTIVE_IMEM_WRITE &&
             machine_digest_equal(fixture.machine, before);
  }
  if (passed) {
    status = t64_bus_write_masked(fixture.machine, TEST_IMEM_BASE, 4u,
                                  UINT8_C(0x01), UINT32_C(0x000000aa));
    passed = status == T64_STATUS_OK;
  }
  t64_machine_destroy(fixture.machine);
  return passed;
}

typedef struct {
  uint32_t length_field;
  uint32_t count_field;
  uint32_t skip;
  uint32_t internal_address;
  uint32_t dram_address;
  uint32_t tag;
  bool dram_to_sp;
  bool imem;
} rsp_dma_descriptor_case;

static uint32_t encode_rsp_dma_length(uint32_t length_field,
                                     uint32_t count_field, uint32_t skip) {
  return (length_field & UINT32_C(0x00000fff)) |
         ((count_field & UINT32_C(0x000000ff)) << 12) |
         ((skip & UINT32_C(0x00000fff)) << 20);
}

static uint8_t dma_test_byte(uint64_t position, uint32_t tag) {
  return (uint8_t)(UINT32_C(1) +
                   (uint32_t)((position * UINT64_C(37) +
                               (uint64_t)tag * UINT64_C(19)) %
                              UINT64_C(254)));
}

static bool run_dma_descriptor_case(
    const rsp_dma_descriptor_case *test_case) {
  uint8_t expected_payload[T64_SP_BANK_BYTES];
  uint8_t expected_bank[T64_SP_BANK_BYTES];
  rsp_control_fixture fixture;
  uint32_t line_bytes;
  uint32_t rows;
  uint64_t payload_bytes;
  uint64_t stride;
  uint64_t dram_span;
  uint32_t internal_start;
  uint8_t *bank;
  uint32_t row;
  uint32_t byte;
  uint64_t position = 0u;
  uint64_t end_tick;
  uint32_t status_value = 0u;
  bool passed;

  if (test_case == NULL) {
    return false;
  }
  line_bytes = ((test_case->length_field & UINT32_C(0x00000fff)) |
                UINT32_C(7)) +
               UINT32_C(1);
  rows = (test_case->count_field & UINT32_C(0x000000ff)) + UINT32_C(1);
  payload_bytes = (uint64_t)line_bytes * (uint64_t)rows;
  stride = (uint64_t)line_bytes + (uint64_t)test_case->skip;
  dram_span = stride * (uint64_t)rows;
  if (payload_bytes == 0u || payload_bytes > sizeof expected_payload ||
      test_case->dram_address > UINT32_MAX - dram_span ||
      (uint64_t)test_case->dram_address + dram_span > UINT64_C(0x00400000)) {
    return false;
  }
  passed = fixture_create(&fixture);
  if (!passed) {
    return false;
  }
  reset_rsp_fixture(&fixture, true);
  internal_start = test_case->internal_address & UINT32_C(0x00000ff8);
  bank = test_case->imem ? fixture.machine->sp_imem
                         : fixture.machine->sp_dmem;
  (void)memset(bank, UINT8_C(0x5a), T64_SP_BANK_BYTES);
  (void)memset(expected_bank, UINT8_C(0x5a), sizeof expected_bank);
  (void)memset(fixture.machine->ram + test_case->dram_address, UINT8_C(0xa5),
               (size_t)dram_span);
  for (row = 0u; row < rows; row++) {
    uint64_t dram_row = (uint64_t)test_case->dram_address +
                        (uint64_t)row * stride;

    for (byte = 0u; byte < line_bytes; byte++) {
      uint8_t value = dma_test_byte(position, test_case->tag);
      uint32_t bank_offset =
          (internal_start + (uint32_t)position) &
          (T64_SP_BANK_BYTES - UINT32_C(1));

      expected_payload[position] = value;
      if (test_case->dram_to_sp) {
        fixture.machine->ram[(size_t)dram_row + byte] = value;
        expected_bank[bank_offset] = value;
      } else {
        bank[bank_offset] = value;
      }
      position++;
    }
  }
  passed = t64_machine_bus_write(fixture.machine, TEST_SP_BASE, 4u,
                                 test_case->internal_address) ==
               T64_STATUS_OK &&
           t64_machine_bus_write(fixture.machine, TEST_SP_BASE + 4u, 4u,
                                 test_case->dram_address) == T64_STATUS_OK &&
           t64_machine_bus_write(
               fixture.machine,
               TEST_SP_BASE + (test_case->dram_to_sp ? 8u : 12u), 4u,
               encode_rsp_dma_length(test_case->length_field,
                                     test_case->count_field,
                                     test_case->skip)) == T64_STATUS_OK &&
           fixture.machine->sp.active.valid &&
           fixture.machine->sp.active.bytes_per_line == line_bytes &&
           fixture.machine->sp.active.line_count == rows &&
           fixture.machine->sp.active.skip == test_case->skip &&
           fixture.machine->sp.active.payload_bytes == payload_bytes;
  end_tick = UINT64_C(12) + payload_bytes / UINT64_C(8);
  passed = passed && advance_to(fixture.machine, end_tick) &&
           !fixture.machine->sp.active.valid &&
           !fixture.machine->sp.pending.valid &&
           fixture.machine->rsp_observed_dma_cursor == payload_bytes &&
           fixture.machine->rsp_observed_dma_action ==
               (T64_RSP_DMA_ACTION_BEAT | T64_RSP_DMA_ACTION_COMPLETE) &&
           read_sp_register(fixture.machine, UINT32_C(0x10), &status_value) &&
           (status_value & (T64_RSP_STATUS_DMA_BUSY |
                            T64_RSP_STATUS_DMA_FULL)) == 0u;
  if (passed && test_case->dram_to_sp) {
    passed = memcmp(bank, expected_bank, sizeof expected_bank) == 0;
  }
  if (passed && !test_case->dram_to_sp) {
    position = 0u;
    for (row = 0u; row < rows && passed; row++) {
      uint64_t dram_row = (uint64_t)test_case->dram_address +
                          (uint64_t)row * stride;

      for (byte = 0u; byte < line_bytes; byte++) {
        if (fixture.machine->ram[(size_t)dram_row + byte] !=
            expected_payload[position++]) {
          passed = false;
          break;
        }
      }
      for (byte = line_bytes; byte < line_bytes + test_case->skip && passed;
           byte++) {
        if (fixture.machine->ram[(size_t)dram_row + byte] != UINT8_C(0xa5)) {
          passed = false;
        }
      }
    }
  }
  t64_machine_destroy(fixture.machine);
  return passed;
}

static bool dma_descriptor_boundary_matrix_test(void) {
  static const rsp_dma_descriptor_case cases[] = {
      {0u, 0u, 0u, UINT32_C(0x00000fff), UINT32_C(0x00002000), 1u, true,
       false},
      {7u, 0u, 0u, UINT32_C(0x00001fff), UINT32_C(0x00012000), 2u, true,
       true},
      {8u, 0u, 0u, UINT32_C(0x00001ffb), UINT32_C(0x00022000), 3u, false,
       true},
      {UINT32_C(0x0fff), 0u, 0u, UINT32_C(0x00000fff), UINT32_C(0x00032000),
       4u, true, false},
      {0u, UINT32_C(0x00ff), UINT32_C(0x0fff), UINT32_C(0x00001fff),
       UINT32_C(0x00042000), 5u, false, true},
      {7u, 1u, UINT32_C(0x0fff), UINT32_C(0x00000fff), UINT32_C(0x00052000),
       6u, true, false}};
  size_t index;

  for (index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
    if (!run_dma_descriptor_case(&cases[index])) {
      return false;
    }
  }
  return true;
}

static bool dma_final_row_span_test(void) {
  rsp_control_fixture fixture;
  uint32_t invalid_address;
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t *bank;
  uint32_t status_value = 0u;
  uint32_t byte;
  t64_status status;
  bool passed = fixture_create(&fixture);

  if (!passed) {
    return false;
  }
  reset_rsp_fixture(&fixture, true);
  invalid_address = fixture.machine->profile.rdram_bytes - UINT32_C(8);
  passed = t64_machine_bus_write(fixture.machine, TEST_SP_BASE, 4u, 0u) ==
               T64_STATUS_OK &&
           t64_machine_bus_write(fixture.machine, TEST_SP_BASE + 4u, 4u,
                                 UINT32_C(0x1000)) == T64_STATUS_OK &&
           t64_machine_bus_write(fixture.machine, TEST_SP_BASE + 8u, 4u,
                                 UINT32_C(7)) == T64_STATUS_OK &&
           fixture.machine->sp.active.valid;
  passed = passed &&
           t64_machine_bus_write(fixture.machine, TEST_SP_BASE + 4u, 4u,
                                 invalid_address) == T64_STATUS_OK &&
           t64_machine_bus_write(fixture.machine, TEST_SP_BASE, 4u,
                                 UINT32_C(0x100)) == T64_STATUS_OK &&
           t64_machine_bus_read(fixture.machine, TEST_SP_BASE + 0x10u, 4u,
                                &status_value) == T64_STATUS_OK &&
           (status_value & T64_RSP_STATUS_DMA_FULL) == 0u &&
           t64_machine_digest(fixture.machine, before) == T64_STATUS_OK;
  if (passed) {
    status = t64_machine_bus_write(fixture.machine, TEST_SP_BASE + 8u, 4u,
                                   UINT32_C(8));
    passed = status == T64_STATUS_BUS_DEVICE_REFUSED &&
             fixture.machine->rsp_observed_refusal_reason ==
                 T64_RSP_REFUSAL_DMA_SPAN &&
             machine_digest_equal(fixture.machine, before) &&
             fixture.machine->sp.active.valid &&
             !fixture.machine->sp.pending.valid;
  }
  t64_machine_destroy(fixture.machine);
  if (!passed) {
    return false;
  }

  passed = fixture_create(&fixture);
  if (!passed) {
    return false;
  }
  reset_rsp_fixture(&fixture, true);
  bank = fixture.machine->sp_dmem;
  for (byte = 0u; byte < 16u; byte++) {
    fixture.machine->ram[fixture.machine->profile.rdram_bytes - 16u + byte] =
        (uint8_t)(UINT32_C(0x80) + byte);
  }
  passed = t64_machine_bus_write(fixture.machine, TEST_SP_BASE, 4u, 0u) ==
               T64_STATUS_OK &&
           t64_machine_bus_write(
               fixture.machine, TEST_SP_BASE + 4u, 4u,
               fixture.machine->profile.rdram_bytes - UINT32_C(16)) ==
               T64_STATUS_OK &&
           t64_machine_bus_write(fixture.machine, TEST_SP_BASE + 8u, 4u,
                                 UINT32_C(8)) == T64_STATUS_OK &&
           advance_to(fixture.machine, UINT64_C(14));
  for (byte = 0u; byte < 16u && passed; byte++) {
    passed = bank[byte] == (uint8_t)(UINT32_C(0x80) + byte);
  }
  t64_machine_destroy(fixture.machine);
  return passed;
}

static bool dma_queue_full_promotion_test(void) {
  rsp_control_fixture fixture;
  uint8_t before[T64_DIGEST_SIZE];
  uint64_t active_token;
  uint64_t pending_token;
  uint64_t next_token;
  size_t queue_count;
  uint32_t status_value = 0u;
  uint32_t byte;
  t64_status status;
  bool passed = fixture_create(&fixture);

  if (!passed) {
    return false;
  }
  reset_rsp_fixture(&fixture, true);
  for (byte = 0u; byte < 24u; byte++) {
    fixture.machine->ram[UINT32_C(0x100) + byte] =
        (uint8_t)(UINT32_C(0x20) + byte);
  }
  passed = t64_machine_bus_write(fixture.machine, TEST_SP_BASE, 4u, 0u) ==
               T64_STATUS_OK &&
           t64_machine_bus_write(fixture.machine, TEST_SP_BASE + 4u, 4u,
                                 UINT32_C(0x100)) == T64_STATUS_OK &&
           t64_machine_bus_write(fixture.machine, TEST_SP_BASE + 8u, 4u,
                                 UINT32_C(7)) == T64_STATUS_OK &&
           fixture.machine->sp.active.valid;
  active_token = fixture.machine->rsp.active_dma_token;
  passed = passed &&
           t64_machine_bus_write(fixture.machine, TEST_SP_BASE, 4u,
                                 UINT32_C(0x1000)) == T64_STATUS_OK &&
           t64_machine_bus_write(fixture.machine, TEST_SP_BASE + 4u, 4u,
                                 UINT32_C(0x108)) == T64_STATUS_OK &&
           t64_machine_bus_write(fixture.machine, TEST_SP_BASE + 8u, 4u,
                                 UINT32_C(7)) == T64_STATUS_OK &&
           fixture.machine->sp.pending.valid;
  pending_token = fixture.machine->rsp.pending_dma_token;
  passed = passed && active_token != 0u && pending_token != 0u &&
           active_token != pending_token &&
           t64_machine_bus_write(fixture.machine, TEST_SP_BASE, 4u,
                                 UINT32_C(0x2000)) == T64_STATUS_OK &&
           t64_machine_bus_write(fixture.machine, TEST_SP_BASE + 4u, 4u,
                                 UINT32_C(0x110)) == T64_STATUS_OK &&
           t64_machine_bus_read(fixture.machine, TEST_SP_BASE + 0x10u, 4u,
                                &status_value) == T64_STATUS_OK &&
           (status_value & (T64_RSP_STATUS_DMA_BUSY |
                            T64_RSP_STATUS_DMA_FULL)) ==
               (T64_RSP_STATUS_DMA_BUSY | T64_RSP_STATUS_DMA_FULL) &&
           t64_machine_digest(fixture.machine, before) == T64_STATUS_OK;
  queue_count = fixture.machine->queue_count;
  next_token = fixture.machine->sp.next_token;
  if (passed) {
    status = t64_machine_bus_write(fixture.machine, TEST_SP_BASE + 8u, 4u,
                                   UINT32_C(15));
    passed = status == T64_STATUS_BUS_DEVICE_REFUSED &&
             fixture.machine->rsp_observed_refusal_reason ==
                 T64_RSP_REFUSAL_DMA_QUEUE_FULL &&
             machine_digest_equal(fixture.machine, before) &&
             fixture.machine->queue_count == queue_count &&
             fixture.machine->sp.next_token == next_token &&
             fixture.machine->rsp.active_dma_token == active_token &&
             fixture.machine->rsp.pending_dma_token == pending_token &&
             fixture.machine->sp.active.valid &&
             fixture.machine->sp.pending.valid &&
             fixture.machine->sp.read_length == UINT32_C(7);
  }
  passed = passed && advance_to(fixture.machine, UINT64_C(13)) &&
           fixture.machine->sp.active.valid &&
           fixture.machine->sp.active.token == pending_token &&
           !fixture.machine->sp.pending.valid &&
           fixture.machine->rsp.dma_setup_remaining == UINT64_C(12) &&
           fixture.machine->rsp_observed_dma_action ==
               (T64_RSP_DMA_ACTION_BEAT | T64_RSP_DMA_ACTION_COMPLETE |
                T64_RSP_DMA_ACTION_PROMOTE);
  for (byte = 0u; byte < 8u && passed; byte++) {
    passed = fixture.machine->sp_dmem[byte] ==
             (uint8_t)(UINT32_C(0x20) + byte);
  }
  passed = passed && advance_to(fixture.machine, UINT64_C(26)) &&
           !fixture.machine->sp.active.valid &&
           !fixture.machine->sp.pending.valid &&
           !fixture.machine->rsp.active_dma_token &&
           read_sp_register(fixture.machine, UINT32_C(0x10), &status_value) &&
           (status_value & (T64_RSP_STATUS_DMA_BUSY |
                            T64_RSP_STATUS_DMA_FULL)) == 0u;
  for (byte = 0u; byte < 8u && passed; byte++) {
    passed = fixture.machine->sp_imem[byte] ==
             (uint8_t)(UINT32_C(0x28) + byte);
  }
  t64_machine_destroy(fixture.machine);
  return passed;
}

static bool run_dma_link_notification_case(bool dram_to_sp,
                                           uint32_t byte_index,
                                           bool overlap) {
  rsp_control_fixture fixture;
  uint32_t index;
  bool passed = fixture_create(&fixture);

  if (!passed) {
    return false;
  }
  reset_rsp_fixture(&fixture, true);
  if (dram_to_sp) {
    for (index = 0u; index < 8u; index++) {
      fixture.machine->ram[UINT32_C(0x100) + index] =
          (uint8_t)(UINT32_C(0x41) + index);
    }
  } else {
    for (index = 0u; index < 8u; index++) {
      fixture.machine->sp_dmem[UINT32_C(0x20) + index] =
          (uint8_t)(UINT32_C(0x51) + index);
      fixture.machine->ram[UINT32_C(0x100) + index] = UINT8_C(0xa5);
    }
  }
  fixture.machine->cpu.linked_valid = true;
  fixture.machine->cpu.linked_physical_address =
      dram_to_sp ? TEST_DMEM_BASE + UINT32_C(0x20) +
                       (overlap ? byte_index : UINT32_C(8))
                 : UINT32_C(0x100) +
                       (overlap ? byte_index : UINT32_C(8));
  fixture.machine->cpu.linked_width = 1u;
  passed = t64_machine_bus_write(fixture.machine, TEST_SP_BASE, 4u,
                                 UINT32_C(0x20)) == T64_STATUS_OK &&
           t64_machine_bus_write(fixture.machine, TEST_SP_BASE + 4u, 4u,
                                 UINT32_C(0x100)) == T64_STATUS_OK &&
           t64_machine_bus_write(
               fixture.machine,
               TEST_SP_BASE + (dram_to_sp ? UINT32_C(8) : UINT32_C(12)), 4u,
               UINT32_C(7)) == T64_STATUS_OK &&
           advance_to(fixture.machine, UINT64_C(13)) &&
           fixture.machine->cpu.linked_valid == !overlap;
  t64_machine_destroy(fixture.machine);
  return passed;
}

static bool dma_destination_link_notification_test(void) {
  uint32_t byte_index;

  for (byte_index = 0u; byte_index < UINT32_C(8); byte_index++) {
    if (!run_dma_link_notification_case(true, byte_index, true) ||
        !run_dma_link_notification_case(false, byte_index, true)) {
      return false;
    }
  }
  return run_dma_link_notification_case(true, 0u, false) &&
         run_dma_link_notification_case(false, 0u, false);
}

static bool run_same_bank_conflict_case(bool imem_dma) {
  const uint32_t program_pc = UINT32_C(0x100);
  const uint32_t dma_mem_address =
      imem_dma ? UINT32_C(0x1200) : UINT32_C(0x0200);
  const uint32_t first_instruction =
      imem_dma ? UINT32_C(0) : UINT32_C(0xac010000);
  rsp_control_fixture fixture;
  t64_rsp_dma_step step;
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t imem_before[T64_SP_BANK_BYTES];
  uint8_t dmem_before[T64_SP_BANK_BYTES];
  uint8_t vector_result_before[2];
  uint64_t dma_event_sequence;
  uint64_t issue_sequence;
  uint64_t next_issue_tick;
  uint64_t stall_slots;
  uint64_t scalar_ready_tick[32];
  uint64_t vector_ready_tick[32];
  uint64_t memory_access_ready_tick;
  size_t queue_count;
  uint32_t byte;
  t64_status preflight_status;
  bool passed = fixture_create(&fixture);

  if (!passed) {
    return false;
  }
  reset_rsp_fixture(&fixture, true);
  fixture.machine->rsp.pc = program_pc;
  fixture.machine->rsp.gpr[1] = UINT32_C(0x12345678);
  fixture.machine->rsp.vector[1][0] = 0u;
  fixture.machine->rsp.vector[1][1] = 1u;
  fixture.machine->rsp.vector[2][0] = 0u;
  fixture.machine->rsp.vector[2][1] = 2u;
  fixture.machine->rsp.vector[3][0] = 0u;
  fixture.machine->rsp.vector[3][1] = 9u;
  passed = write_imem_word(fixture.machine, program_pc, first_instruction) &&
           write_imem_word(fixture.machine, program_pc + 4u,
                           UINT32_C(0x4a0108d0));
  for (byte = 0u; byte < 8u && passed; byte++) {
    fixture.machine->ram[UINT32_C(0x100) + byte] =
        (uint8_t)(UINT32_C(0x90) + byte);
  }
  passed = passed &&
           t64_machine_bus_write(fixture.machine, T64_SP_PC_ADDRESS, 4u,
                                 program_pc) == T64_STATUS_OK &&
           t64_machine_bus_write(fixture.machine, TEST_SP_BASE, 4u,
                                 dma_mem_address) == T64_STATUS_OK &&
           t64_machine_bus_write(fixture.machine, TEST_SP_BASE + 4u, 4u,
                                 UINT32_C(0x100)) == T64_STATUS_OK &&
           t64_machine_bus_write(fixture.machine, TEST_SP_BASE + 8u, 4u,
                                 UINT32_C(7)) == T64_STATUS_OK &&
           fixture.machine->sp.active.valid &&
           advance_to(fixture.machine, UINT64_C(12)) &&
           fixture.machine->rsp.dma_setup_remaining == 0u &&
           t64_machine_bus_write(fixture.machine, TEST_SP_BASE + 0x10u, 4u,
                                 UINT32_C(1)) == T64_STATUS_OK &&
           (fixture.machine->rsp.sp_status & T64_RSP_STATUS_HALT) == 0u &&
           t64_machine_digest(fixture.machine, digest_before) ==
               T64_STATUS_OK;
  if (!passed) {
    t64_machine_destroy(fixture.machine);
    return false;
  }
  (void)memcpy(imem_before, fixture.machine->sp_imem, sizeof imem_before);
  (void)memcpy(dmem_before, fixture.machine->sp_dmem, sizeof dmem_before);
  (void)memcpy(scalar_ready_tick, fixture.machine->rsp.scalar_ready_tick,
               sizeof scalar_ready_tick);
  (void)memcpy(vector_ready_tick, fixture.machine->rsp.vector_ready_tick,
               sizeof vector_ready_tick);
  vector_result_before[0] = fixture.machine->rsp.vector[3][0];
  vector_result_before[1] = fixture.machine->rsp.vector[3][1];
  queue_count = fixture.machine->queue_count;
  issue_sequence = fixture.machine->rsp.issue_sequence;
  next_issue_tick = fixture.machine->rsp.next_issue_tick;
  dma_event_sequence = fixture.machine->rsp.dma_event_sequence;
  stall_slots = fixture.machine->rsp.stall_slots;
  memory_access_ready_tick = fixture.machine->rsp.memory_access_ready_tick;
  preflight_status = t64_sp_rsp_dma_preflight(
      fixture.machine, true, !imem_dma, &step);
  passed = preflight_status == T64_STATUS_BUS_DEVICE_REFUSED &&
           step.kind == T64_RSP_DMA_STEP_NONE &&
           fixture.machine->rsp_observed_refusal_reason ==
               T64_RSP_REFUSAL_BANK_CONFLICT &&
           machine_digest_equal(fixture.machine, digest_before) &&
           fixture.machine->queue_count == queue_count &&
           fixture.machine->rsp.issue_sequence == issue_sequence &&
           fixture.machine->rsp.next_issue_tick == next_issue_tick &&
           fixture.machine->rsp.dma_event_sequence == dma_event_sequence &&
           fixture.machine->rsp.stall_slots == stall_slots &&
           fixture.machine->rsp.memory_access_ready_tick ==
               memory_access_ready_tick &&
           memcmp(scalar_ready_tick,
                  fixture.machine->rsp.scalar_ready_tick,
                  sizeof scalar_ready_tick) == 0 &&
           memcmp(vector_ready_tick,
                  fixture.machine->rsp.vector_ready_tick,
                  sizeof vector_ready_tick) == 0 &&
           memcmp(imem_before, fixture.machine->sp_imem,
                  sizeof imem_before) == 0 &&
           memcmp(dmem_before, fixture.machine->sp_dmem,
                  sizeof dmem_before) == 0;
  passed = passed && advance_to(fixture.machine, UINT64_C(13)) &&
           fixture.machine->rsp_observed_outcome == T64_RSP_ATTEMPT_REFUSED &&
           fixture.machine->rsp_observed_refusal_reason ==
               T64_RSP_REFUSAL_BANK_CONFLICT &&
           fixture.machine->rsp_observed_second_pc == program_pc + 4u &&
           fixture.machine->rsp_observed_second_opcode ==
               UINT32_C(0x4a0108d0) &&
           fixture.machine->rsp.pc == program_pc &&
           fixture.machine->rsp.retired_instructions == 0u &&
           fixture.machine->rsp.stall_slots == stall_slots + UINT64_C(1) &&
           fixture.machine->rsp.dma_beat_cursor == 0u &&
           fixture.machine->rsp.dma_event_sequence == dma_event_sequence &&
           fixture.machine->rsp.memory_access_ready_tick ==
               memory_access_ready_tick &&
           memcmp(scalar_ready_tick,
                  fixture.machine->rsp.scalar_ready_tick,
                  sizeof scalar_ready_tick) == 0 &&
           memcmp(vector_ready_tick,
                  fixture.machine->rsp.vector_ready_tick,
                  sizeof vector_ready_tick) == 0 &&
           fixture.machine->rsp.vector[3][0] == vector_result_before[0] &&
           fixture.machine->rsp.vector[3][1] == vector_result_before[1] &&
           memcmp(imem_before, fixture.machine->sp_imem,
                  sizeof imem_before) == 0 &&
           memcmp(dmem_before, fixture.machine->sp_dmem,
                  sizeof dmem_before) == 0;
  t64_machine_destroy(fixture.machine);
  return passed;
}

static bool same_bank_conflict_atomicity_test(void) {
  return run_same_bank_conflict_case(true) &&
         run_same_bank_conflict_case(false);
}

static bool active_pc_dma_destination_refusal_test(void) {
  const uint32_t program_pc = UINT32_C(0x100);
  rsp_control_fixture fixture;
  t64_rsp_dma_step step;
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t imem_before[T64_SP_BANK_BYTES];
  uint64_t dma_event_sequence;
  size_t queue_count;
  t64_status preflight_status;
  bool passed = fixture_create(&fixture);

  if (!passed) {
    return false;
  }
  reset_rsp_fixture(&fixture, true);
  passed = write_imem_word(fixture.machine, program_pc, 0u) &&
           write_imem_word(fixture.machine, program_pc + 4u,
                           UINT32_C(0x4a0108d0)) &&
           t64_machine_bus_write(fixture.machine, T64_SP_PC_ADDRESS, 4u,
                                 program_pc) == T64_STATUS_OK &&
           t64_machine_bus_write(fixture.machine, TEST_SP_BASE, 4u,
                                 UINT32_C(0x1100)) == T64_STATUS_OK &&
           t64_machine_bus_write(fixture.machine, TEST_SP_BASE + 4u, 4u,
                                 UINT32_C(0x100)) == T64_STATUS_OK &&
           t64_machine_bus_write(fixture.machine, TEST_SP_BASE + 8u, 4u,
                                 UINT32_C(7)) == T64_STATUS_OK &&
           advance_to(fixture.machine, UINT64_C(12)) &&
           fixture.machine->rsp.dma_setup_remaining == 0u &&
           t64_machine_bus_write(fixture.machine, TEST_SP_BASE + 0x10u, 4u,
                                 UINT32_C(1)) == T64_STATUS_OK &&
           t64_machine_digest(fixture.machine, digest_before) ==
               T64_STATUS_OK;
  if (!passed) {
    t64_machine_destroy(fixture.machine);
    return false;
  }
  (void)memcpy(imem_before, fixture.machine->sp_imem, sizeof imem_before);
  queue_count = fixture.machine->queue_count;
  dma_event_sequence = fixture.machine->rsp.dma_event_sequence;
  preflight_status = t64_sp_rsp_dma_preflight(
      fixture.machine, true, false, &step);
  passed = preflight_status == T64_STATUS_BUS_DEVICE_REFUSED &&
           step.kind == T64_RSP_DMA_STEP_NONE &&
           fixture.machine->rsp_observed_refusal_reason ==
               T64_RSP_REFUSAL_ACTIVE_IMEM_WRITE &&
           machine_digest_equal(fixture.machine, digest_before) &&
           fixture.machine->queue_count == queue_count &&
           fixture.machine->rsp.dma_event_sequence == dma_event_sequence &&
           memcmp(imem_before, fixture.machine->sp_imem,
                  sizeof imem_before) == 0;
  passed = passed && advance_to(fixture.machine, UINT64_C(13)) &&
           fixture.machine->rsp_observed_outcome == T64_RSP_ATTEMPT_REFUSED &&
           fixture.machine->rsp_observed_refusal_reason ==
               T64_RSP_REFUSAL_ACTIVE_IMEM_WRITE &&
           fixture.machine->rsp.pc == program_pc &&
           fixture.machine->rsp.retired_instructions == 0u &&
           fixture.machine->rsp.dma_beat_cursor == 0u &&
           fixture.machine->rsp.dma_event_sequence == dma_event_sequence &&
           memcmp(imem_before, fixture.machine->sp_imem,
                  sizeof imem_before) == 0;
  t64_machine_destroy(fixture.machine);
  return passed;
}

static bool unsupported_dp_cp0_refusal_test(void) {
  rsp_control_fixture fixture;
  t64_rsp_state rsp_before;
  t64_sp_state sp_before;
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(1)};
  t64_stop_result result = {0u, 0u};
  t64_status advance_status;
  bool passed = fixture_create(&fixture);

  if (!passed) {
    return false;
  }
  reset_rsp_fixture(&fixture, true);
  fixture.machine->rsp.gpr[1] = UINT32_C(0x00000003);
  passed = write_imem_word(fixture.machine, 0u,
                           UINT32_C(0x40818000)) && /* MTC0 r1,DPC_STATUS */
           t64_rsp_start(fixture.machine) == T64_STATUS_OK;
  rsp_before = fixture.machine->rsp;
  if (passed) {
    sp_before = fixture.machine->sp;
    advance_status = t64_machine_advance(fixture.machine, &request, &result);
    passed = advance_status == T64_STATUS_OK &&
             result.tick == UINT64_C(1) &&
             memcmp(&rsp_before, &fixture.machine->rsp, sizeof rsp_before) ==
                 0 &&
             memcmp(&sp_before, &fixture.machine->sp, sizeof sp_before) == 0 &&
             fixture.machine->rsp_observation_valid &&
             fixture.machine->rsp_observed_pc == 0u &&
             fixture.machine->rsp_observed_opcode == UINT32_C(0x40818000) &&
             fixture.machine->rsp_observed_outcome ==
                 T64_RSP_ATTEMPT_REFUSED &&
             fixture.machine->rsp_observed_refusal_reason ==
                 T64_RSP_REFUSAL_UNSUPPORTED_OPCODE &&
             fixture.machine->rsp.retired_instructions == 0u &&
             fixture.machine->queue_count == 0u &&
             t64_machine_digest(fixture.machine, digest_before) ==
                 T64_STATUS_OK;
  }
  fixture.machine->rsp_observed_pc = UINT32_C(0x1234);
  fixture.machine->rsp_observed_opcode = UINT32_MAX;
  fixture.machine->rsp_observed_outcome = T64_RSP_ATTEMPT_RETIRED;
  fixture.machine->rsp_observation_valid = false;
  passed = passed &&
           t64_machine_digest(fixture.machine, digest_after) == T64_STATUS_OK &&
           memcmp(digest_before, digest_after, sizeof digest_before) == 0;
  t64_machine_destroy(fixture.machine);
  return passed;
}

static bool partitioned_advance_digest_test(void) {
  rsp_control_fixture whole;
  rsp_control_fixture partitioned;
  uint8_t whole_digest[T64_DIGEST_SIZE];
  uint8_t partitioned_digest[T64_DIGEST_SIZE];
  uint32_t index;
  bool passed = fixture_create(&whole);

  if (!passed) {
    return false;
  }
  passed = fixture_create(&partitioned);
  if (!passed) {
    t64_machine_destroy(whole.machine);
    return false;
  }
  reset_rsp_fixture(&whole, true);
  reset_rsp_fixture(&partitioned, true);
  for (index = 0u; index < UINT32_C(16); index += UINT32_C(4)) {
    passed = passed && write_imem_word(whole.machine, index, 0u) &&
             write_imem_word(partitioned.machine, index, 0u);
  }
  for (index = 0u; index < UINT32_C(16); index++) {
    uint8_t value = (uint8_t)(UINT32_C(0x60) + index);

    whole.machine->ram[UINT32_C(0x100) + index] = value;
    partitioned.machine->ram[UINT32_C(0x100) + index] = value;
  }
  passed = passed &&
           t64_machine_bus_write(whole.machine, TEST_SP_BASE, 4u,
                                 UINT32_C(0x300)) == T64_STATUS_OK &&
           t64_machine_bus_write(whole.machine, TEST_SP_BASE + 4u, 4u,
                                 UINT32_C(0x100)) == T64_STATUS_OK &&
           t64_machine_bus_write(whole.machine, TEST_SP_BASE + 8u, 4u,
                                 UINT32_C(8)) == T64_STATUS_OK &&
           t64_machine_bus_write(partitioned.machine, TEST_SP_BASE, 4u,
                                 UINT32_C(0x300)) == T64_STATUS_OK &&
           t64_machine_bus_write(partitioned.machine, TEST_SP_BASE + 4u, 4u,
                                 UINT32_C(0x100)) == T64_STATUS_OK &&
           t64_machine_bus_write(partitioned.machine, TEST_SP_BASE + 8u, 4u,
                                 UINT32_C(8)) == T64_STATUS_OK &&
           t64_machine_bus_write(whole.machine, TEST_SP_BASE + 0x10u, 4u,
                                 UINT32_C(1)) == T64_STATUS_OK &&
           t64_machine_bus_write(partitioned.machine,
                                 TEST_SP_BASE + 0x10u, 4u,
                                 UINT32_C(1)) == T64_STATUS_OK;
  passed = passed && advance_to(whole.machine, UINT64_C(20)) &&
           advance_to(partitioned.machine, UINT64_C(1)) &&
           advance_to(partitioned.machine, UINT64_C(4)) &&
           advance_to(partitioned.machine, UINT64_C(12)) &&
           advance_to(partitioned.machine, UINT64_C(13)) &&
           advance_to(partitioned.machine, UINT64_C(14)) &&
           advance_to(partitioned.machine, UINT64_C(20)) &&
           t64_machine_digest(whole.machine, whole_digest) == T64_STATUS_OK &&
           t64_machine_digest(partitioned.machine, partitioned_digest) ==
               T64_STATUS_OK &&
           memcmp(whole_digest, partitioned_digest,
                  sizeof whole_digest) == 0 &&
           memcmp(whole.machine->sp_dmem, partitioned.machine->sp_dmem,
                  sizeof whole.machine->sp_dmem) == 0 &&
           whole.machine->queue_count == partitioned.machine->queue_count &&
           whole.machine->rsp.retired_instructions ==
               partitioned.machine->rsp.retired_instructions &&
           whole.machine->rsp.pc == partitioned.machine->rsp.pc;
  t64_machine_destroy(whole.machine);
  t64_machine_destroy(partitioned.machine);
  return passed;
}

int main(int argc, char **argv) {
  active_pc_write_observation observation;
  bool status_passed;
  bool sem_pc_passed;
  bool in_flight_passed;
  bool active_pc_passed;
  bool active_pc_widths_passed;
  bool dma_boundaries_passed;
  bool dma_span_passed;
  bool dma_queue_passed;
  bool dma_links_passed;
  bool bank_conflict_passed;
  bool partitioned_passed;
  bool active_pc_dma_passed;
  bool unsupported_dp_passed;
  bool load_store_passed;
  bool passed;

  if (argc == 2 &&
      strcmp(argv[1], "--tdd-active-pc-imem-refusal") == 0) {
    passed = active_pc_imem_write_refusal_test(&observation);
    puts("TAP version 13");
    printf("%s 1 - CPU write to active RSP PC in IMEM refuses atomically\n",
           passed ? "ok" : "not ok");
    if (!passed && observation.setup_ok) {
      printf("# active PC=0x%03x write_status=%u refusal=%u unchanged=%u "
             "word_before=0x%08x word_after=0x%08x\n",
             observation.pc, (unsigned)observation.write_status,
             observation.refusal_reason, observation.unchanged ? 1u : 0u,
             observation.word_before, observation.word_after);
    } else if (!passed) {
      puts("# active-PC refusal fixture setup failed before the write assertion");
    }
    puts("1..1");
    return passed ? 0 : 1;
  }
  if (argc != 1) {
    fputs("usage: test_rsp_control [--tdd-active-pc-imem-refusal]\n",
          stderr);
    return 2;
  }
  status_passed = status_command_matrix_test();
  sem_pc_passed = semaphore_and_pc_wrap_test();
  in_flight_passed = in_flight_halt_and_single_step_refusal_test();
  active_pc_passed = active_pc_imem_write_refusal_test(&observation);
  active_pc_widths_passed = active_pc_write_widths_test();
  dma_boundaries_passed = dma_descriptor_boundary_matrix_test();
  dma_span_passed = dma_final_row_span_test();
  dma_queue_passed = dma_queue_full_promotion_test();
  dma_links_passed = dma_destination_link_notification_test();
  bank_conflict_passed = same_bank_conflict_atomicity_test();
  active_pc_dma_passed = active_pc_dma_destination_refusal_test();
  unsupported_dp_passed = unsupported_dp_cp0_refusal_test();
  load_store_passed = load_store_bubble_test();
  partitioned_passed = partitioned_advance_digest_test();
  passed = status_passed && sem_pc_passed && in_flight_passed &&
           active_pc_passed && active_pc_widths_passed &&
           dma_boundaries_passed && dma_span_passed && dma_queue_passed &&
           dma_links_passed && bank_conflict_passed &&
           active_pc_dma_passed && unsupported_dp_passed &&
           load_store_passed && partitioned_passed;
  puts("TAP version 13");
  printf("%s 1 - status command matrix and model clear/set precedence\n",
         status_passed ? "ok" : "not ok");
  printf("%s 2 - semaphore read-to-set and SP_PC masks/wrap\n",
         sem_pc_passed ? "ok" : "not ok");
  printf("%s 3 - asynchronous halt and in-flight single-step refuse\n",
         in_flight_passed ? "ok" : "not ok");
  printf("%s 4 - CPU write to active RSP PC in IMEM refuses atomically\n",
         active_pc_passed ? "ok" : "not ok");
  printf("%s 5 - active-PC refusal covers byte and masked bus writes\n",
         active_pc_widths_passed ? "ok" : "not ok");
  printf("%s 6 - DMA length/count/skip/bank/direction boundary matrix\n",
         dma_boundaries_passed ? "ok" : "not ok");
  printf("%s 7 - DMA validates final valid and first invalid DRAM rows\n",
         dma_span_passed ? "ok" : "not ok");
  printf("%s 8 - DMA queue-full refusal and pending promotion are atomic\n",
         dma_queue_passed ? "ok" : "not ok");
  printf("%s 9 - every committed DMA byte notifies LL observers\n",
         dma_links_passed ? "ok" : "not ok");
  printf("%s 10 - same-bank preflight preserves digest and paired effects\n",
         bank_conflict_passed ? "ok" : "not ok");
  printf("%s 11 - active-PC DMA destination refuses before a beat\n",
         active_pc_dma_passed ? "ok" : "not ok");
  printf("%s 12 - unsupported CP0 DPC access refuses without side effects\n",
         unsupported_dp_passed ? "ok" : "not ok");
  printf("%s 13 - load-to-store hazard emits one modeled bubble\n",
         load_store_passed ? "ok" : "not ok");
  printf("%s 14 - partitioned advancement preserves the full digest\n",
         partitioned_passed ? "ok" : "not ok");
  if (!active_pc_passed && observation.setup_ok) {
    printf("# active PC=0x%03x write_status=%u refusal=%u unchanged=%u "
           "word_before=0x%08x word_after=0x%08x\n",
           observation.pc, (unsigned)observation.write_status,
           observation.refusal_reason, observation.unchanged ? 1u : 0u,
           observation.word_before, observation.word_after);
  } else if (!active_pc_passed) {
    if (!observation.setup_ok) {
      puts("# active-PC refusal fixture setup failed before the write assertion");
    }
  }
  puts("1..14");
  return passed ? 0 : 1;
}
