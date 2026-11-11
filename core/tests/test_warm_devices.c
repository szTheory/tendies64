#include "t64_test.h"

#include "t64_internal.h"
#include "tendies64/t64_machine.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_IPL3_BYTES 4096u
#define TEST_HELLO_BYTES 4280u
#define TEST_RDRAM_BYTES UINT32_C(0x00400000)

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  void *memory;

  (void)context;
  if (storage_out == NULL || size == 0u || alignment == 0u) {
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
  return true;
}

static void test_release(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static bool read_exact(const char *environment_name, uint8_t *bytes,
                       size_t length) {
  const char *path = getenv(environment_name);
  FILE *file;
  size_t actual;
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
  actual = fread(bytes, 1u, length, file);
  extra = fgetc(file);
  read_error = ferror(file);
  close_error = fclose(file);
  return actual == length && extra == EOF && read_error == 0 &&
         close_error == 0;
}

static void write_be32(uint8_t *bytes, uint32_t value) {
  bytes[0] = (uint8_t)(value >> 24);
  bytes[1] = (uint8_t)(value >> 16);
  bytes[2] = (uint8_t)(value >> 8);
  bytes[3] = (uint8_t)value;
}

typedef struct {
  uint64_t token;
  uint64_t tick;
  uint32_t dram_address;
  uint32_t cart_address;
  uint32_t length;
  bool descriptor_observed;
  bool completion_observed;
  bool copied_bytes_match;
} warm_pi_issue;

typedef struct {
  uint64_t token;
  uint64_t tick;
  uint32_t raw_mem_address;
  uint32_t raw_dram_address;
  uint32_t raw_length;
  uint64_t start_tick;
  uint64_t end_tick;
  bool imem;
  bool descriptor_observed;
} warm_sp_issue;

typedef struct {
  size_t sp_issue_count;
  size_t sp_complete_count;
  size_t pi_issue_count;
  size_t pi_complete_count;
  size_t pi_copy_check_count;
  bool pi_copy_mismatch;
  bool authored_app_copy_verified;
  warm_sp_issue sp_issues[8];
  uint64_t sp_complete_tokens[8];
  uint64_t sp_complete_ticks[8];
  warm_pi_issue pi_issues[8];
  uint64_t pi_complete_tokens[8];
  uint64_t pi_complete_ticks[8];
  uint64_t sp_issue_tick;
  uint64_t sp_complete_tick;
  uint64_t pi_issue_tick;
  uint64_t pi_complete_tick;
} warm_dma_trace;

static bool drain_warm_dma_trace(t64_machine *machine,
                                warm_dma_trace *observed) {
  t64_trace_record records[T64_TRACE_RECORD_CAPACITY];
  size_t count = 0u;
  uint64_t dropped = 0u;
  bool saturated = false;
  size_t i;

  if (machine == NULL || observed == NULL ||
      t64_machine_trace_drain(machine, records,
                              sizeof records / sizeof records[0], &count,
                              &dropped, &saturated) != T64_STATUS_OK ||
      dropped != 0u || saturated) {
    return false;
  }
  for (i = 0u; i < count; i++) {
    if (records[i].device == T64_TRACE_DEVICE_SP &&
        records[i].kind == T64_TRACE_KIND_SP_ISSUE) {
      const t64_sp_dma_descriptor *descriptor = NULL;

      if (machine->sp.active.valid &&
          machine->sp.active.token == records[i].value) {
        descriptor = &machine->sp.active;
      } else if (machine->sp.pending.valid &&
                 machine->sp.pending.token == records[i].value) {
        descriptor = &machine->sp.pending;
      }
      if (descriptor != NULL &&
          observed->sp_issue_count <
              sizeof observed->sp_issues / sizeof observed->sp_issues[0]) {
        warm_sp_issue *issue =
            &observed->sp_issues[observed->sp_issue_count];

        if (observed->sp_issue_count == 0u) {
          observed->sp_issue_tick = records[i].tick;
        }
        issue->token = records[i].value;
        issue->tick = records[i].tick;
        issue->raw_mem_address = descriptor->raw_mem_address;
        issue->raw_dram_address = descriptor->raw_dram_address;
        issue->raw_length = descriptor->raw_length;
        issue->start_tick = descriptor->start_tick;
        issue->end_tick = descriptor->end_tick;
        issue->imem = descriptor->imem;
        issue->descriptor_observed = true;
        observed->sp_issue_count++;
      }
    } else if (records[i].device == T64_TRACE_DEVICE_SP &&
               records[i].kind == T64_TRACE_KIND_SP_COMPLETE) {
      if (observed->sp_complete_count == 0u) {
        observed->sp_complete_tick = records[i].tick;
      }
      if (observed->sp_complete_count <
          sizeof observed->sp_complete_tokens /
              sizeof observed->sp_complete_tokens[0]) {
        observed->sp_complete_tokens[observed->sp_complete_count] =
            records[i].value;
        observed->sp_complete_ticks[observed->sp_complete_count] =
            records[i].tick;
      }
      observed->sp_complete_count++;
    } else if (records[i].device == T64_TRACE_DEVICE_PI &&
               records[i].kind == T64_TRACE_KIND_PI_ISSUE) {
      if (machine->pi.active_valid &&
          machine->pi.active_token == records[i].value &&
          observed->pi_issue_count <
              sizeof observed->pi_issues / sizeof observed->pi_issues[0]) {
        warm_pi_issue *issue =
            &observed->pi_issues[observed->pi_issue_count];

        if (observed->pi_issue_count == 0u) {
          observed->pi_issue_tick = records[i].tick;
        }
        issue->token = records[i].value;
        issue->tick = records[i].tick;
        issue->dram_address = machine->pi.active_dram_address;
        issue->cart_address = machine->pi.active_cart_address;
        issue->length = machine->pi.active_length;
        issue->descriptor_observed = true;
        observed->pi_issue_count++;
      }
    } else if (records[i].device == T64_TRACE_DEVICE_PI &&
               records[i].kind == T64_TRACE_KIND_PI_COMPLETE) {
      if (observed->pi_complete_count == 0u) {
        observed->pi_complete_tick = records[i].tick;
      }
      if (observed->pi_complete_count <
          sizeof observed->pi_complete_tokens /
              sizeof observed->pi_complete_tokens[0]) {
        observed->pi_complete_tokens[observed->pi_complete_count] =
            records[i].value;
        observed->pi_complete_ticks[observed->pi_complete_count] =
            records[i].tick;
      }
      for (size_t issue_index = 0u;
           issue_index < observed->pi_issue_count &&
           issue_index < sizeof observed->pi_issues /
                             sizeof observed->pi_issues[0];
           issue_index++) {
        warm_pi_issue *issue = &observed->pi_issues[issue_index];

        if (issue->descriptor_observed &&
            issue->token == records[i].value) {
          uint64_t cart_offset =
              (uint64_t)issue->cart_address - UINT64_C(0x10000000);
          const uint8_t *image =
              (const uint8_t *)machine->image_storage.data;
          bool ranges_valid =
              image != NULL && issue->cart_address >= UINT32_C(0x10000000) &&
              cart_offset <= machine->image_length &&
              (uint64_t)issue->length <=
                  (uint64_t)machine->image_length - cart_offset &&
              (uint64_t)issue->dram_address + issue->length <=
                  machine->profile.rdram_bytes;

          issue->completion_observed = true;
          issue->copied_bytes_match =
              ranges_valid &&
              memcmp(machine->ram + issue->dram_address,
                     image + (size_t)cart_offset, issue->length) == 0;
          observed->pi_copy_check_count++;
          if (!issue->copied_bytes_match) {
            observed->pi_copy_mismatch = true;
          }
          if (issue->token == UINT64_C(2) &&
              issue->dram_address == UINT32_C(0x00000400) &&
              issue->cart_address == UINT32_C(0x10001000) &&
              issue->length == UINT32_C(184) &&
              issue->copied_bytes_match) {
            observed->authored_app_copy_verified = true;
          }
        }
      }
      observed->pi_complete_count++;
    }
  }
  return true;
}

static bool advance_in_bounded_chunks(t64_machine *machine,
                                      uint64_t target_tick,
                                      uint64_t maximum_chunk_ticks,
                                      warm_dma_trace *observed,
                                      bool collect_trace) {
  t64_stop_request request = {T64_STOP_GUEST_TICK, 0u};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};

  if (machine == NULL || maximum_chunk_ticks == 0u || target_tick < machine->tick) {
    return false;
  }
  while (machine->tick < target_tick) {
    uint64_t next_tick = target_tick - machine->tick > maximum_chunk_ticks
                             ? machine->tick + maximum_chunk_ticks
                             : target_tick;

    request.value = next_tick;
    if (t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
        result.reason != T64_STOP_REASON_BOUND || result.tick != next_tick) {
      return false;
    }
    if (collect_trace && !drain_warm_dma_trace(machine, observed)) {
      return false;
    }
  }
  return machine->tick == target_tick;
}

static bool warm_sp_descriptor_refused(t64_machine *machine,
                                       uint32_t mem_address,
                                       uint32_t dram_address,
                                       uint32_t raw_length) {
  size_t queue_before;
  uint64_t token_before;
  uint32_t sp_base = UINT32_C(0x04040000);
  t64_status status;

  if (machine == NULL || machine->sp.active.valid ||
      machine->sp.pending.valid) {
    return false;
  }
  queue_before = machine->queue_count;
  token_before = machine->sp.next_token;
  if (t64_machine_bus_write(machine, sp_base, 4u, mem_address) !=
          T64_STATUS_OK ||
      t64_machine_bus_write(machine, sp_base + 4u, 4u, dram_address) !=
          T64_STATUS_OK) {
    return false;
  }
  status = t64_machine_bus_write(machine, sp_base + 8u, 4u, raw_length);
  return status == T64_STATUS_BUS_DEVICE_REFUSED &&
         !machine->sp.active.valid && !machine->sp.pending.valid &&
         machine->queue_count == queue_before &&
         machine->sp.next_token == token_before;
}

static bool issue_exact_warm_sp(t64_machine *machine) {
  uint32_t sp_base = UINT32_C(0x04040000);

  return machine != NULL &&
         t64_machine_bus_write(machine, sp_base, 4u, UINT32_C(0x1000)) ==
             T64_STATUS_OK &&
         t64_machine_bus_write(machine, sp_base + 4u, 4u,
                              UINT32_C(0x00802000)) == T64_STATUS_OK &&
         t64_machine_bus_write(machine, sp_base + 8u, 4u,
                              UINT32_C(0x0fff)) == T64_STATUS_OK;
}

static bool issue_exact_warm_sp_dmem(t64_machine *machine) {
  uint32_t sp_base = UINT32_C(0x04040000);

  return machine != NULL &&
         t64_machine_bus_write(machine, sp_base, 4u, UINT32_C(0x0000)) ==
             T64_STATUS_OK &&
         t64_machine_bus_write(machine, sp_base + 4u, 4u,
                              UINT32_C(0x00802000)) == T64_STATUS_OK &&
         t64_machine_bus_write(machine, sp_base + 8u, 4u,
                              UINT32_C(0x0fff)) == T64_STATUS_OK;
}

static bool warm_sp_dmem_zero_fill_checks(
    const t64_host_allocator *allocator,
    const t64_profile_descriptor *profile,
    const uint8_t authored_rom[TEST_HELLO_BYTES]) {
  t64_machine *machine = NULL;
  t64_stop_request request = {T64_STOP_GUEST_TICK, 0u};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  uint64_t completion_tick;
  bool passed = true;

  if (t64_machine_create(allocator, profile, &machine) != T64_STATUS_OK ||
      machine == NULL ||
      t64_machine_admit_rom(machine, authored_rom, TEST_HELLO_BYTES) !=
          T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return false;
  }
  t64_machine_reset_execution_state(machine);
  (void)memset(machine->sp_dmem, UINT8_C(0x5a), sizeof machine->sp_dmem);
  (void)memset(machine->sp_imem, UINT8_C(0xa5), sizeof machine->sp_imem);
  if (!issue_exact_warm_sp_dmem(machine) || !machine->sp.active.valid ||
      machine->sp.pending.valid || machine->sp.active.imem ||
      !machine->sp.active.dram_to_sp ||
      machine->sp.active.raw_mem_address != 0u ||
      machine->sp.active.raw_dram_address != UINT32_C(0x00802000) ||
      machine->sp.active.raw_length != UINT32_C(0x0fff) ||
      machine->sp.active.payload_bytes != T64_SP_BANK_BYTES ||
      machine->sp.active.internal_start != 0u) {
    t64_machine_destroy(machine);
    return false;
  }
  completion_tick = machine->sp.active.end_tick;
  if (completion_tick == 0u) {
    t64_machine_destroy(machine);
    return false;
  }
  request.value = completion_tick - UINT64_C(1);
  if (t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_BOUND ||
      result.tick != request.value || machine->sp_dmem[0] != UINT8_C(0x5a) ||
      machine->sp_dmem[T64_SP_BANK_BYTES - 1u] != UINT8_C(0x5a) ||
      machine->sp_imem[0] != UINT8_C(0xa5) ||
      machine->sp_imem[T64_SP_BANK_BYTES - 1u] != UINT8_C(0xa5)) {
    t64_machine_destroy(machine);
    return false;
  }
  request.value = completion_tick;
  if (t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_BOUND ||
      result.tick != request.value || machine->sp.active.valid ||
      machine->queue_count != 0u) {
    t64_machine_destroy(machine);
    return false;
  }
  for (size_t i = 0u; i < T64_SP_BANK_BYTES; i++) {
    passed = passed && machine->sp_dmem[i] == 0u &&
             machine->sp_imem[i] == UINT8_C(0xa5);
  }
  if (!passed) {
    t64_machine_destroy(machine);
    return false;
  }

  t64_machine_reset_execution_state(machine);
  (void)memset(machine->sp_dmem, UINT8_C(0x5a), sizeof machine->sp_dmem);
  if (!issue_exact_warm_sp_dmem(machine) || !machine->sp.active.valid) {
    t64_machine_destroy(machine);
    return false;
  }
  completion_tick = machine->sp.active.end_tick;
  t64_machine_reset_execution_state(machine);
  if (machine->sp.active.valid || machine->sp.pending.valid ||
      machine->queue_count != 0u || machine->sp.next_token != 1u) {
    t64_machine_destroy(machine);
    return false;
  }
  request.value = completion_tick + UINT64_C(1);
  if (t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_BOUND ||
      result.tick != request.value) {
    t64_machine_destroy(machine);
    return false;
  }
  for (size_t i = 0u; i < T64_SP_BANK_BYTES; i++) {
    passed = passed && machine->sp_dmem[i] == UINT8_C(0x5a) &&
             machine->sp_imem[i] == UINT8_C(0xa5);
  }
  t64_machine_destroy(machine);
  return passed;
}

static bool warm_sp_reset_register_checks(t64_machine *machine,
                                          uint64_t active_end,
                                          uint64_t pending_end,
                                          size_t queue_count,
                                          uint64_t next_token) {
  uint32_t value = UINT32_C(0xdecafbad);

  if (machine == NULL ||
      t64_machine_bus_read(machine, UINT32_C(0x04040010), 4u, &value) !=
          T64_STATUS_OK ||
      value != UINT32_C(0x0d)) {
    return false;
  }
  t64_mi_set_pending(machine, UINT32_C(0x01));
  value = 0u;
  if (t64_machine_bus_read(machine, UINT32_C(0x04300008), 4u, &value) !=
          T64_STATUS_OK ||
      value != UINT32_C(0x01) ||
      t64_machine_bus_write(machine, UINT32_C(0x04040010), 4u,
                            UINT32_C(0x00aaaa0e)) != T64_STATUS_OK ||
      machine->mi_pending != 0u || machine->mi_irq_line ||
      !machine->sp.active.valid || !machine->sp.pending.valid ||
      machine->sp.active.end_tick != active_end ||
      machine->sp.pending.end_tick != pending_end ||
      machine->queue_count != queue_count ||
      machine->sp.next_token != next_token) {
    return false;
  }
  value = 0u;
  if (t64_machine_bus_read(machine, UINT32_C(0x04040010), 4u, &value) !=
          T64_STATUS_OK ||
      value != UINT32_C(0x0d) ||
      t64_machine_bus_write(machine, UINT32_C(0x04040010), 4u,
                            UINT32_C(0x00000001)) !=
          T64_STATUS_BUS_DEVICE_REFUSED ||
      t64_machine_bus_write(machine, UINT32_C(0x04040010), 1u,
                            UINT32_C(0x00)) != T64_STATUS_BUS_UNSUPPORTED ||
      t64_machine_bus_write(machine, UINT32_C(0x04080000), 4u,
                            UINT32_C(0x00000000)) != T64_STATUS_OK ||
      t64_machine_bus_write(machine, UINT32_C(0x04080000), 4u,
                            UINT32_C(0x00000004)) !=
          T64_STATUS_BUS_DEVICE_REFUSED ||
      t64_machine_bus_read(machine, UINT32_C(0x04080000), 4u, &value) !=
          T64_STATUS_OK ||
      value != UINT32_C(0x00000000) ||
      t64_machine_bus_write(machine, UINT32_C(0x0404001c), 4u,
                            UINT32_C(0x00000000)) != T64_STATUS_OK ||
      t64_machine_bus_read(machine, UINT32_C(0x0404001c), 4u, &value) !=
          T64_STATUS_OK ||
      value != UINT32_C(0x00000000) ||
      t64_machine_bus_write(machine, UINT32_C(0x0404001c), 4u,
                            UINT32_C(0x00000001)) !=
          T64_STATUS_BUS_DEVICE_REFUSED ||
      !machine->sp.active.valid || !machine->sp.pending.valid ||
      machine->sp.active.end_tick != active_end ||
      machine->sp.pending.end_tick != pending_end ||
      machine->queue_count != queue_count ||
      machine->sp.next_token != next_token) {
    return false;
  }
  return true;
}

static bool warm_mi_readonly_write_checks(t64_machine *machine,
                                          uint64_t active_end,
                                          uint64_t pending_end,
                                          size_t queue_count,
                                          uint64_t next_token) {
  uint32_t value = 0u;
  uint32_t mi_pending_before;
  uint32_t mi_mask_before;
  bool mi_irq_before;

  if (machine == NULL ||
      t64_machine_bus_write(machine, UINT32_C(0x0430000c), 4u,
                            UINT32_C(0x0a)) != T64_STATUS_OK) {
    return false;
  }
  t64_mi_set_pending(machine, UINT32_C(0x03));
  mi_pending_before = machine->mi_pending;
  mi_mask_before = machine->mi_mask;
  mi_irq_before = machine->mi_irq_line;
  if (mi_pending_before != UINT32_C(0x03) || mi_mask_before != UINT32_C(0x03) ||
      !mi_irq_before ||
      t64_machine_bus_write(machine, UINT32_C(0x04300008), 4u,
                            UINT32_C(0x555)) != T64_STATUS_OK ||
      machine->mi_pending != mi_pending_before ||
      machine->mi_mask != mi_mask_before ||
      machine->mi_irq_line != mi_irq_before ||
      t64_machine_bus_write(machine, UINT32_C(0x04300008), 4u,
                            UINT32_C(0x554)) !=
          T64_STATUS_BUS_DEVICE_REFUSED ||
      t64_machine_bus_write(machine, UINT32_C(0x04300008), 1u,
                            UINT32_C(0x55)) != T64_STATUS_BUS_UNSUPPORTED ||
      t64_machine_bus_write(machine, UINT32_C(0x04300004), 4u,
                            UINT32_C(0x555)) !=
          T64_STATUS_BUS_DEVICE_REFUSED ||
      machine->mi_pending != mi_pending_before ||
      machine->mi_mask != mi_mask_before ||
      machine->mi_irq_line != mi_irq_before ||
      !machine->sp.active.valid || !machine->sp.pending.valid ||
      machine->sp.active.end_tick != active_end ||
      machine->sp.pending.end_tick != pending_end ||
      machine->queue_count != queue_count ||
      machine->sp.next_token != next_token) {
    return false;
  }
  value = UINT32_C(0xdecafbad);
  if (t64_machine_bus_read(machine, UINT32_C(0x04300008), 4u, &value) !=
          T64_STATUS_OK ||
      value != UINT32_C(0x03)) {
    return false;
  }
  t64_mi_clear_pending(machine, UINT32_C(0x03));
  t64_mi_set_pending(machine, UINT32_C(0x06));
  value = UINT32_C(0xdecafbad);
  if (t64_machine_bus_read(machine, UINT32_C(0x0450000c), 4u, &value) !=
          T64_STATUS_OK ||
      value != 0u ||
      t64_machine_bus_write(machine, UINT32_C(0x0450000c), 4u, 0u) !=
          T64_STATUS_OK ||
      machine->mi_pending != UINT32_C(0x02) ||
      machine->mi_mask != mi_mask_before ||
      t64_machine_bus_write(machine, UINT32_C(0x0450000c), 4u,
                            UINT32_C(1)) !=
          T64_STATUS_BUS_DEVICE_REFUSED ||
      t64_machine_bus_write(machine, UINT32_C(0x0450000c), 1u,
                            UINT32_C(0)) != T64_STATUS_BUS_UNSUPPORTED ||
      machine->mi_pending != UINT32_C(0x02) ||
      t64_machine_bus_write(machine, UINT32_C(0x04800018), 4u, 0u) !=
          T64_STATUS_OK ||
      machine->mi_pending != 0u ||
      t64_machine_bus_write(machine, UINT32_C(0x04800018), 4u,
                            UINT32_C(1)) !=
          T64_STATUS_BUS_DEVICE_REFUSED ||
      machine->mi_mask != mi_mask_before ||
      !machine->sp.active.valid || !machine->sp.pending.valid ||
      machine->sp.active.end_tick != active_end ||
      machine->sp.pending.end_tick != pending_end ||
      machine->queue_count != queue_count ||
      machine->sp.next_token != next_token) {
    return false;
  }
  t64_mi_set_pending(machine, UINT32_C(0x20));
  if (t64_machine_bus_write(machine, UINT32_C(0x04300000), 4u,
                            UINT32_C(0x800)) != T64_STATUS_OK ||
      machine->mi_pending != 0u || machine->mi_mask != mi_mask_before ||
      t64_machine_bus_write(machine, UINT32_C(0x04300000), 4u,
                            UINT32_C(0x801)) !=
          T64_STATUS_BUS_DEVICE_REFUSED ||
      machine->mi_pending != 0u || machine->mi_mask != mi_mask_before ||
      !machine->sp.active.valid || !machine->sp.pending.valid ||
      machine->sp.active.end_tick != active_end ||
      machine->sp.pending.end_tick != pending_end ||
      machine->queue_count != queue_count ||
      machine->sp.next_token != next_token) {
    return false;
  }
  return true;
}

static bool warm_sp_queue_reset_checks(
    const t64_host_allocator *allocator,
    const t64_profile_descriptor *profile,
    const uint8_t authored_rom[TEST_HELLO_BYTES]) {
  t64_machine *machine = NULL;
  t64_stop_request request = {T64_STOP_GUEST_TICK, 0u};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  uint64_t active_end;
  uint64_t pending_end;
  size_t queue_before_refusal;
  uint64_t token_before_refusal;
  uint32_t sp_base = UINT32_C(0x04040000);
  t64_status refusal_status;
  bool passed;

  if (t64_machine_create(allocator, profile, &machine) != T64_STATUS_OK ||
      machine == NULL ||
      t64_machine_admit_rom(machine, authored_rom, TEST_HELLO_BYTES) !=
          T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return false;
  }
  t64_machine_reset_execution_state(machine);
  (void)memset(machine->sp_imem, UINT8_C(0x5a), sizeof machine->sp_imem);
  if (!issue_exact_warm_sp(machine) || !machine->sp.active.valid ||
      machine->sp.pending.valid) {
    t64_machine_destroy(machine);
    return false;
  }
  active_end = machine->sp.active.end_tick;
  if (!issue_exact_warm_sp(machine) || !machine->sp.active.valid ||
      !machine->sp.pending.valid) {
    t64_machine_destroy(machine);
    return false;
  }
  pending_end = machine->sp.pending.end_tick;
  queue_before_refusal = machine->queue_count;
  token_before_refusal = machine->sp.next_token;
  if (active_end == 0u || pending_end <= active_end ||
      machine->sp.active.raw_mem_address != UINT32_C(0x1000) ||
      machine->sp.active.raw_dram_address != UINT32_C(0x00802000) ||
      machine->sp.active.raw_length != UINT32_C(0x0fff) ||
      machine->sp.pending.raw_mem_address != UINT32_C(0x1000) ||
      machine->sp.pending.raw_dram_address != UINT32_C(0x00802000) ||
      machine->sp.pending.raw_length != UINT32_C(0x0fff)) {
    t64_machine_destroy(machine);
    return false;
  }
  refusal_status = t64_machine_bus_write(machine, sp_base + 8u, 4u,
                                         UINT32_C(0x0fff));
  if (refusal_status != T64_STATUS_BUS_DEVICE_REFUSED ||
      machine->queue_count != queue_before_refusal ||
      machine->sp.next_token != token_before_refusal ||
      !machine->sp.active.valid || !machine->sp.pending.valid ||
      machine->sp.active.end_tick != active_end ||
      machine->sp.pending.end_tick != pending_end) {
    t64_machine_destroy(machine);
    return false;
  }
  if (!warm_sp_reset_register_checks(machine, active_end, pending_end,
                                     queue_before_refusal,
                                     token_before_refusal)) {
    t64_machine_destroy(machine);
    return false;
  }
  if (!warm_mi_readonly_write_checks(machine, active_end, pending_end,
                                     queue_before_refusal,
                                     token_before_refusal)) {
    t64_machine_destroy(machine);
    return false;
  }
  t64_machine_reset_execution_state(machine);
  if (machine->sp.active.valid || machine->sp.pending.valid ||
      machine->queue_count != 0u || machine->sp.next_token != 1u) {
    t64_machine_destroy(machine);
    return false;
  }
  request.value = pending_end + UINT64_C(1);
  if (t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_BOUND ||
      result.tick != request.value) {
    t64_machine_destroy(machine);
    return false;
  }
  passed = true;
  for (size_t i = 0u; i < sizeof machine->sp_imem; i++) {
    passed = passed && machine->sp_imem[i] == UINT8_C(0x5a);
  }
  t64_machine_destroy(machine);
  return passed;
}

int main(void) {
  uint8_t source_asset[TEST_IPL3_BYTES];
  uint8_t authored_rom[TEST_HELLO_BYTES];
  uint8_t malformed_rom[TEST_HELLO_BYTES + 4u];
  uint8_t variant_rom[TEST_HELLO_BYTES];
  const t64_host_allocator allocator = {test_allocate, test_release, NULL};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  t64_machine *interleaved_machine = NULL;
  uint32_t value = UINT32_C(0xdecafbad);
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(100)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  bool image_ok;
  bool advanced_past_device;
  bool unsupported_stop_observed = false;
  bool warm_cache_hit_invalidate_seen = false;
  uint32_t warm_cache_hit_invalidate_physical = 0u;
  bool authored_dmem_zero_issue_observed = false;
  bool authored_dmem_zero_complete_observed = false;
  uint64_t authored_dmem_zero_token = 0u;
  uint64_t authored_dmem_zero_end_tick = 0u;
  bool authored_entry_observed = false;
  uint64_t authored_entry_tick = 0u;
  uint64_t authored_entry_retired = 0u;
  uint32_t authored_entry_pc = 0u;
  uint64_t loader_tick_limit = UINT64_C(700000);
  warm_dma_trace observed_dma = {0u};
  uint64_t next_tick;
  t64_cpu_stop_detail stop_detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};
  static const struct {
    uint32_t mem_address;
    uint32_t dram_address;
    uint32_t raw_length;
  } refused_warm_sp_cases[] = {
      {UINT32_C(0x1000), UINT32_C(0x00802004), UINT32_C(0x0fff)},
      {UINT32_C(0x1010), UINT32_C(0x00802000), UINT32_C(0x0fff)},
      {UINT32_C(0x0008), UINT32_C(0x00802000), UINT32_C(0x0fff)},
      {UINT32_C(0x1000), UINT32_C(0x00802000), UINT32_C(0x0ff7)},
      {UINT32_C(0x0000), UINT32_C(0x00802004), UINT32_C(0x0fff)},
      {UINT32_C(0x0010), UINT32_C(0x00802000), UINT32_C(0x0fff)},
      {UINT32_C(0x1008), UINT32_C(0x00802000), UINT32_C(0x0fff)},
      {UINT32_C(0x0000), UINT32_C(0x00802000), UINT32_C(0x0ff7)},
  };
  const char *fixtures_dir = getenv("T64_BOOT_FIXTURES_DIR");
  static const char *const authored_variants[] = {
      "hello.z64", "hello-empty-break.z64", "hello-wrong-text.z64",
      "hello-empty-loop.z64", "hello-incomplete-loop.z64",
      "hello-unsupported.z64", "hello-output-overflow.z64"};
  size_t variant_index;

  puts("TAP version 13");
  image_ok = read_exact("T64_TEST_IPL3_PATH", source_asset,
                        sizeof source_asset) &&
             read_exact("T64_TEST_HELLO_PATH", authored_rom,
                        sizeof authored_rom);
  T64_CHECK(image_ok);
  T64_CHECK_INT(t64_profile_lookup("ntsc-cart-4m-openipl3-warm-v1", &profile),
                T64_STATUS_OK);
  T64_CHECK(profile != NULL);
  if (profile == NULL || !image_ok) {
    puts("not ok 1 - qualified warm-device contract admits authored ROM and executes RI path");
    puts("1..1");
    return 1;
  }

  T64_CHECK_INT(profile->rom_max_bytes, UINT32_C(1048576));
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  T64_CHECK(machine != NULL);
  if (machine == NULL) {
    puts("not ok 1 - qualified warm-device contract admits authored ROM and executes RI path");
    puts("1..1");
    return 1;
  }

  memcpy(malformed_rom, authored_rom, sizeof authored_rom);
  malformed_rom[UINT32_C(0x40)] ^= UINT8_C(1);
  T64_CHECK_INT(t64_machine_admit_rom(machine, malformed_rom,
                                      sizeof authored_rom),
                T64_STATUS_INVALID_ROM);
  T64_CHECK(!machine->has_image);
  T64_CHECK_INT(machine->queue_count, 0u);

  memcpy(malformed_rom, authored_rom, sizeof authored_rom);
  write_be32(malformed_rom + UINT32_C(0x08), UINT32_C(0x807ffffc));
  T64_CHECK_INT(t64_machine_admit_rom(machine, malformed_rom,
                                      sizeof authored_rom),
                T64_STATUS_INVALID_ROM);
  T64_CHECK(!machine->has_image);
  T64_CHECK_INT(machine->queue_count, 0u);

  memcpy(malformed_rom, authored_rom, sizeof authored_rom);
  write_be32(malformed_rom + UINT32_C(0x10), UINT32_C(180));
  T64_CHECK_INT(t64_machine_admit_rom(machine, malformed_rom,
                                      sizeof authored_rom),
                T64_STATUS_INVALID_ROM);
  T64_CHECK(!machine->has_image);
  T64_CHECK_INT(machine->queue_count, 0u);

  memcpy(malformed_rom, authored_rom, sizeof authored_rom);
  memcpy(malformed_rom + sizeof authored_rom, "\0\0\0\0", 4u);
  T64_CHECK_INT(t64_machine_admit_rom(machine, malformed_rom,
                                      sizeof malformed_rom),
                T64_STATUS_INVALID_ROM);
  T64_CHECK(!machine->has_image);
  T64_CHECK_INT(machine->queue_count, 0u);

  memcpy(malformed_rom, authored_rom, sizeof authored_rom);
  malformed_rom[UINT32_C(0x1000)] ^= UINT8_C(1);
  T64_CHECK_INT(t64_machine_admit_rom(machine, malformed_rom,
                                      sizeof authored_rom),
                T64_STATUS_OK);
  T64_CHECK(machine->has_image);
  T64_CHECK_INT(t64_machine_admit_rom(machine, authored_rom,
                                      sizeof authored_rom),
                T64_STATUS_OK);
  if (!machine->has_image) {
    T64_CHECK_INT(t64_machine_admit_rom(machine, source_asset,
                                        sizeof source_asset),
                  T64_STATUS_OK);
  }
  T64_CHECK(machine->has_image);

  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04300004), 4u,
                                     &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, 0u);
  value = UINT32_C(0xdecafbad);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x0470000c), 4u,
                                     &value),
                T64_STATUS_OK);
  T64_CHECK(value != 0u);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x0470000d), 1u,
                                     &value),
                T64_STATUS_BUS_UNSUPPORTED);
  value = UINT32_C(0xdecafbad);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04700010), 4u,
                                     &value),
                T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(value, UINT32_C(0xdecafbad));
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x04300004), 4u, 1u),
                T64_STATUS_BUS_DEVICE_REFUSED);

  T64_CHECK_INT(t64_machine_bus_write(machine, TEST_RDRAM_BYTES - 1u, 1u,
                                      UINT32_C(0xa5)),
                T64_STATUS_OK);
  value = 0u;
  T64_CHECK_INT(t64_machine_bus_read(machine, TEST_RDRAM_BYTES - 1u, 1u,
                                     &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, UINT32_C(0xa5));
  value = UINT32_C(0xdecafbad);
  T64_CHECK_INT(t64_machine_bus_read(machine, TEST_RDRAM_BYTES, 4u, &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, 0u);
  T64_CHECK_INT(t64_machine_bus_write(machine, TEST_RDRAM_BYTES, 4u,
                                      UINT32_C(0xffffffff)),
                T64_STATUS_OK);
  value = UINT32_C(0xdecafbad);
  T64_CHECK_INT(t64_machine_bus_read(machine, TEST_RDRAM_BYTES + 4u, 4u,
                                     &value),
                T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(value, UINT32_C(0xdecafbad));

  for (variant_index = 0u;
       variant_index < sizeof refused_warm_sp_cases /
                           sizeof refused_warm_sp_cases[0];
       variant_index++) {
    T64_CHECK(warm_sp_descriptor_refused(
        machine, refused_warm_sp_cases[variant_index].mem_address,
        refused_warm_sp_cases[variant_index].dram_address,
        refused_warm_sp_cases[variant_index].raw_length));
  }
  T64_CHECK(warm_sp_queue_reset_checks(&allocator, profile, authored_rom));
  T64_CHECK(warm_sp_dmem_zero_fill_checks(&allocator, profile,
                                           authored_rom));

  T64_CHECK_INT(t64_machine_create(&allocator, profile, &interleaved_machine),
                T64_STATUS_OK);
  T64_CHECK(interleaved_machine != NULL);
  if (interleaved_machine != NULL) {
    T64_CHECK_INT(t64_machine_admit_rom(interleaved_machine, authored_rom,
                                        sizeof authored_rom),
                  T64_STATUS_OK);
    t64_machine_reset_execution_state(interleaved_machine);
    memset(interleaved_machine->sp_imem, UINT8_C(0x5a),
           sizeof interleaved_machine->sp_imem);
    request.value = UINT64_C(20);
    T64_CHECK_INT(t64_machine_advance(interleaved_machine, &request,
                                      &result),
                  T64_STATUS_OK);
    T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
    T64_CHECK_INT(result.tick, UINT64_C(20));
    T64_CHECK(issue_exact_warm_sp(interleaved_machine));
    T64_CHECK(interleaved_machine->sp.active.valid);
    if (interleaved_machine->sp.active.valid) {
      T64_CHECK_INT(interleaved_machine->sp.active.start_tick,
                    UINT64_C(20));
      T64_CHECK_INT(interleaved_machine->sp.active.end_tick,
                    UINT64_C(544));
    }
  }

  T64_CHECK_INT(t64_machine_trace_set_enabled(machine, true), T64_STATUS_OK);
  memset(machine->sp_imem, UINT8_C(0xa5), sizeof machine->sp_imem);
  machine->sp_dmem[0] = UINT8_C(0x5a);
  for (next_tick = UINT64_C(16); next_tick < UINT64_C(100);
       next_tick += UINT64_C(16)) {
    request.value = next_tick;
    T64_CHECK_INT(t64_machine_advance(machine, &request, &result),
                  T64_STATUS_OK);
    T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
    T64_CHECK_INT(result.tick, next_tick);
    T64_CHECK(drain_warm_dma_trace(machine, &observed_dma));
  }
  request.value = UINT64_C(100);
  T64_CHECK_INT(t64_machine_advance(machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, UINT64_C(100));
  T64_CHECK(machine->sp.active.valid);
  if (machine->sp.active.valid) {
    T64_CHECK_INT(machine->sp.active.raw_mem_address, UINT32_C(0x1000));
    T64_CHECK_INT(machine->sp.active.raw_dram_address, UINT32_C(0x00802000));
    T64_CHECK_INT(machine->sp.active.raw_length, UINT32_C(0x0fff));
    T64_CHECK_INT(machine->sp.active.start_tick, UINT64_C(81));
    T64_CHECK_INT(machine->sp.active.end_tick, UINT64_C(605));
  }
  T64_CHECK_INT(machine->sp_imem[0], UINT8_C(0xa5));
  advanced_past_device = result.reason == T64_STOP_REASON_BOUND &&
                         result.tick == UINT64_C(100) &&
                         machine->cpu.retired_instructions > UINT64_C(80);
  T64_CHECK(advanced_past_device);
  (void)printf("# warm-path boundary reason=%u tick=%llu retired=%llu pc=0x%08x last_pc=0x%08x last_opcode=0x%08x\n",
               result.reason, (unsigned long long)result.tick,
               (unsigned long long)machine->cpu.retired_instructions,
               machine->cpu.pc, machine->cpu.last_pc,
               machine->cpu.last_opcode);
  T64_CHECK(drain_warm_dma_trace(machine, &observed_dma));
  T64_CHECK(observed_dma.sp_issue_count >= 1u);
  T64_CHECK_INT(observed_dma.sp_issue_tick, UINT64_C(81));

  for (next_tick = UINT64_C(132); next_tick <= UINT64_C(604);
       next_tick += UINT64_C(32)) {
    request.value = next_tick;
    T64_CHECK_INT(t64_machine_advance(machine, &request, &result),
                  T64_STATUS_OK);
    T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
    T64_CHECK_INT(result.tick, next_tick);
    T64_CHECK(drain_warm_dma_trace(machine, &observed_dma));
  }
  T64_CHECK(machine->sp.active.valid);
  T64_CHECK_INT(machine->sp_imem[0], UINT8_C(0xa5));
  T64_CHECK_INT(machine->sp_dmem[0], UINT8_C(0x5a));
  if (interleaved_machine != NULL) {
    T64_CHECK(interleaved_machine->sp.active.valid);
    T64_CHECK_INT(interleaved_machine->sp_imem[0], UINT8_C(0x5a));
  }
  value = 0u;
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04001000), 4u,
                                     &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, UINT32_C(0xa5a5a5a5));

  request.value = UINT64_C(605);
  T64_CHECK_INT(t64_machine_advance(machine, &request, &result),
                T64_STATUS_OK);
  if (machine->queue_count > 0u) {
    const t64_event *front = &machine->events[machine->queue_head];
    (void)printf("# at-deadline result=%u tick=%llu active=%u pending=%u active-end=%llu active-token=%llu active-desc=(mem=0x%x dram=0x%x len=0x%x start=%llu bytes=%u) queue=%zu front=(%llu,%u,%u,%llu)\n",
                 result.reason, (unsigned long long)result.tick,
                 (unsigned)machine->sp.active.valid,
                 (unsigned)machine->sp.pending.valid,
                 (unsigned long long)machine->sp.active.end_tick,
                 (unsigned long long)machine->sp.active.token,
                 machine->sp.active.raw_mem_address,
                 machine->sp.active.raw_dram_address,
                 machine->sp.active.raw_length,
                 (unsigned long long)machine->sp.active.start_tick,
                 machine->sp.active.payload_bytes,
                 machine->queue_count,
                 (unsigned long long)front->tick, front->device,
                 front->kind, (unsigned long long)front->token);
  }
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, UINT64_C(605));
  T64_CHECK_INT(machine->sp_imem[0], 0u);
  T64_CHECK_INT(machine->sp_imem[T64_SP_BANK_BYTES - 1u], 0u);
  T64_CHECK_INT(machine->sp_dmem[0], UINT8_C(0x5a));
  if (interleaved_machine != NULL) {
    request.value = UINT64_C(543);
    T64_CHECK_INT(t64_machine_advance(interleaved_machine, &request,
                                      &result),
                  T64_STATUS_OK);
    T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
    T64_CHECK_INT(result.tick, UINT64_C(543));
    T64_CHECK_INT(interleaved_machine->sp_imem[0], UINT8_C(0x5a));
    request.value = UINT64_C(544);
    T64_CHECK_INT(t64_machine_advance(interleaved_machine, &request,
                                      &result),
                  T64_STATUS_OK);
    T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
    T64_CHECK_INT(result.tick, UINT64_C(544));
    T64_CHECK_INT(interleaved_machine->sp_imem[0], 0u);
  }
  T64_CHECK(drain_warm_dma_trace(machine, &observed_dma));
  T64_CHECK(observed_dma.sp_complete_count >= 1u);
  T64_CHECK_INT(observed_dma.sp_complete_tick, UINT64_C(605));
  value = UINT32_MAX;
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04001000), 4u,
                                     &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, 0u);
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x04001000), 4u,
                                      UINT32_C(0xdeadbeef)),
                T64_STATUS_OK);
  request.value = UINT64_C(606);
  T64_CHECK_INT(t64_machine_advance(machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, UINT64_C(606));
  value = 0u;
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04001000), 4u,
                                     &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, UINT32_C(0xdeadbeef));
  T64_CHECK(drain_warm_dma_trace(machine, &observed_dma));
  (void)printf("# after first SP completion tick=%llu retired=%llu pc=0x%08x last_pc=0x%08x last_opcode=0x%08x next-sp-token=%llu active=%u\n",
               (unsigned long long)machine->tick,
               (unsigned long long)machine->cpu.retired_instructions,
               machine->cpu.pc, machine->cpu.last_pc,
               machine->cpu.last_opcode,
               (unsigned long long)machine->sp.active.token,
               (unsigned)machine->sp.active.valid);

  T64_CHECK_INT(t64_machine_trace_set_enabled(machine, false), T64_STATUS_OK);
  while (machine->tick < loader_tick_limit && !machine->cpu.stopped) {
    if (machine->sp.active.valid &&
        machine->sp.active.token >= UINT64_C(5)) {
      uint64_t capture_start = machine->sp.active.end_tick > UINT64_C(64)
                                   ? machine->sp.active.end_tick - UINT64_C(64)
                                   : machine->tick;

      T64_CHECK(advance_in_bounded_chunks(machine, capture_start,
                                          UINT64_C(3000), &observed_dma,
                                          false));
      T64_CHECK_INT(t64_machine_trace_set_enabled(machine, true),
                    T64_STATUS_OK);
      while (machine->tick < loader_tick_limit && !machine->cpu.stopped) {
        uint64_t instruction_limit = machine->instruction_count + UINT64_C(1);
        t64_status advance_status;

        request.kind = T64_STOP_INSTRUCTION_COUNT;
        request.value = instruction_limit;
        advance_status = t64_machine_advance(machine, &request, &result);
        T64_CHECK_INT(advance_status, T64_STATUS_OK);
        if (advance_status != T64_STATUS_OK ||
            !drain_warm_dma_trace(machine, &observed_dma)) {
          break;
        }
        if (machine->cache.operation_seen &&
            machine->cache.last_operation == UINT32_C(0x11)) {
          warm_cache_hit_invalidate_seen = true;
          warm_cache_hit_invalidate_physical = machine->cache.last_address;
        }
        if (machine->cpu.last_pc == UINT32_C(0x80000400)) {
          authored_entry_observed = true;
          authored_entry_tick = machine->tick;
          authored_entry_retired = machine->cpu.retired_instructions;
          authored_entry_pc = machine->cpu.last_pc;
        }
        if (result.reason == T64_STOP_REASON_CPU_UNSUPPORTED) {
          unsupported_stop_observed = true;
          break;
        }
        if (result.reason != T64_STOP_REASON_INSTRUCTION_COUNT ||
            machine->instruction_count != instruction_limit) {
          T64_CHECK_INT(result.reason, T64_STOP_REASON_INSTRUCTION_COUNT);
          T64_CHECK_INT(machine->instruction_count, instruction_limit);
          break;
        }
      }
      break;
    }
    if (!advance_in_bounded_chunks(machine,
                                   machine->tick + UINT64_C(3000),
                                   UINT64_C(3000), &observed_dma, false)) {
      break;
    }
  }
  if (unsupported_stop_observed) {
    T64_CHECK_INT(t64_machine_cpu_stop_detail(machine, &stop_detail),
                  T64_STATUS_OK);
    if (stop_detail.pc == UINT32_C(0x803fffc4)) {
      uint32_t base = (uint32_t)machine->cpu.gpr[2];
      uint32_t target = base + (stop_detail.opcode & UINT32_C(0xffff));
      (void)printf("# MI_INTR candidate base=0x%08x virtual=0x%08x physical=0x%08x value=0x%08x\n",
                   base, target, target & UINT32_C(0x1fffffff),
                   (uint32_t)machine->cpu.gpr[5]);
    }
    if (stop_detail.pc == UINT32_C(0xb0000ae4)) {
      uint32_t base = (uint32_t)machine->cpu.gpr[2];
      uint32_t target = base + (stop_detail.opcode & UINT32_C(0xffff));
      (void)printf("# zero-source SP candidate base=0x%08x virtual=0x%08x physical=0x%08x value=0x%08x latches=(mem=0x%04x dram=0x%08x read_len=0x%08x active=%u token=%llu desc=(mem=0x%04x dram=0x%08x len=0x%08x imem=%u end=%llu) pending=%u queue=%zu next_token=%llu)\n",
                   base, target, target & UINT32_C(0x1fffffff),
                   (uint32_t)machine->cpu.gpr[3], machine->sp.mem_address,
                   machine->sp.dram_address, machine->sp.read_length,
                   (unsigned)machine->sp.active.valid,
                   (unsigned long long)machine->sp.active.token,
                   machine->sp.active.raw_mem_address,
                   machine->sp.active.raw_dram_address,
                   machine->sp.active.raw_length,
                   (unsigned)machine->sp.active.imem,
                   (unsigned long long)machine->sp.active.end_tick,
                   (unsigned)machine->sp.pending.valid, machine->queue_count,
                   (unsigned long long)machine->sp.next_token);
    }
    if (stop_detail.pc == UINT32_C(0x8000041c)) {
      (void)printf("# authored ISViewer store target=0x%016llx value=0x%016llx\n",
                   (unsigned long long)machine->cpu.gpr[8],
                   (unsigned long long)machine->cpu.gpr[10]);
    }
  }
  (void)printf("# authored entry executed pc=0x%08x tick=%llu retired=%llu\n",
               authored_entry_pc, (unsigned long long)authored_entry_tick,
               (unsigned long long)authored_entry_retired);
  (void)printf("# final loader tick=%llu retired=%llu pc=0x%08x last_pc=0x%08x last_opcode=0x%08x stopped=%u stop=(pc=0x%08x opcode=0x%08x) cache-last=(op=0x%x virtual=0x%08x physical=0x%08x) cache11=(seen=%u virtual=0x%08x physical=0x%08x) pi=(active=%u issue=%zu@%llu complete=%zu@%llu)\n",
               (unsigned long long)machine->tick,
               (unsigned long long)machine->cpu.retired_instructions,
               machine->cpu.pc, machine->cpu.last_pc,
               machine->cpu.last_opcode, (unsigned)machine->cpu.stopped,
               stop_detail.pc, stop_detail.opcode,
               machine->cache.last_operation,
               UINT32_C(0x80000000) | machine->cache.last_address,
               machine->cache.last_address,
               (unsigned)warm_cache_hit_invalidate_seen,
               UINT32_C(0x80000000) |
                   warm_cache_hit_invalidate_physical,
               warm_cache_hit_invalidate_physical,
               (unsigned)machine->pi.active_valid,
               observed_dma.pi_issue_count,
               (unsigned long long)observed_dma.pi_issue_tick,
               observed_dma.pi_complete_count,
               (unsigned long long)observed_dma.pi_complete_tick);
  for (size_t pi_index = 0u;
       pi_index < observed_dma.pi_issue_count &&
       pi_index < sizeof observed_dma.pi_issues / sizeof observed_dma.pi_issues[0];
       pi_index++) {
    const warm_pi_issue *issue = &observed_dma.pi_issues[pi_index];

    (void)printf("# PI issue token=%llu tick=%llu descriptor=%u dram=0x%08x cart=0x%08x length=%u\n",
                 (unsigned long long)issue->token,
                 (unsigned long long)issue->tick,
                 (unsigned)issue->descriptor_observed, issue->dram_address,
                 issue->cart_address, issue->length);
  }
  for (size_t sp_index = 0u;
       sp_index < observed_dma.sp_issue_count &&
       sp_index < sizeof observed_dma.sp_issues /
                      sizeof observed_dma.sp_issues[0];
       sp_index++) {
    const warm_sp_issue *issue = &observed_dma.sp_issues[sp_index];

    (void)printf("# SP issue token=%llu tick=%llu descriptor=%u mem=0x%04x dram=0x%08x length=0x%08x bank=%s start=%llu end=%llu\n",
                 (unsigned long long)issue->token,
                 (unsigned long long)issue->tick,
                 (unsigned)issue->descriptor_observed,
                 issue->raw_mem_address, issue->raw_dram_address,
                 issue->raw_length, issue->imem ? "IMEM" : "DMEM",
                 (unsigned long long)issue->start_tick,
                 (unsigned long long)issue->end_tick);
    if (issue->descriptor_observed && !issue->imem &&
        issue->raw_mem_address == 0u &&
        issue->raw_dram_address == UINT32_C(0x00802000) &&
        issue->raw_length == UINT32_C(0x0fff)) {
      authored_dmem_zero_issue_observed = true;
      authored_dmem_zero_token = issue->token;
      authored_dmem_zero_end_tick = issue->end_tick;
    }
  }
  for (size_t complete_index = 0u;
       complete_index < observed_dma.sp_complete_count &&
       complete_index < sizeof observed_dma.sp_complete_tokens /
                            sizeof observed_dma.sp_complete_tokens[0];
       complete_index++) {
    (void)printf("# SP complete token=%llu tick=%llu\n",
                 (unsigned long long)observed_dma.sp_complete_tokens[complete_index],
                 (unsigned long long)observed_dma.sp_complete_ticks[complete_index]);
    if (observed_dma.sp_complete_tokens[complete_index] ==
            authored_dmem_zero_token &&
        observed_dma.sp_complete_ticks[complete_index] ==
            authored_dmem_zero_end_tick) {
      authored_dmem_zero_complete_observed = true;
    }
  }
  for (size_t complete_index = 0u;
       complete_index < observed_dma.pi_complete_count &&
       complete_index < sizeof observed_dma.pi_complete_tokens /
                            sizeof observed_dma.pi_complete_tokens[0];
       complete_index++) {
    (void)printf("# PI complete token=%llu tick=%llu\n",
                 (unsigned long long)observed_dma.pi_complete_tokens[complete_index],
                 (unsigned long long)observed_dma.pi_complete_ticks[complete_index]);
  }
  T64_CHECK(machine->cache.operation_seen);
  T64_CHECK(warm_cache_hit_invalidate_seen);
  T64_CHECK_INT(warm_cache_hit_invalidate_physical,
                UINT32_C(0x007f8ff0));
  T64_CHECK(observed_dma.pi_issue_count > 0u);
  T64_CHECK(observed_dma.pi_complete_count > 0u);
  T64_CHECK(unsupported_stop_observed);
  T64_CHECK_INT(observed_dma.pi_copy_check_count, UINT64_C(2));
  T64_CHECK(!observed_dma.pi_copy_mismatch);
  T64_CHECK(observed_dma.authored_app_copy_verified);
  T64_CHECK(authored_dmem_zero_issue_observed);
  T64_CHECK(authored_dmem_zero_complete_observed);
  T64_CHECK_INT(authored_dmem_zero_token, UINT64_C(8));
  T64_CHECK(authored_entry_observed);
  T64_CHECK_INT(authored_entry_pc, UINT32_C(0x80000400));
  T64_CHECK_INT(stop_detail.pc, UINT32_C(0x8000041c));
  T64_CHECK_INT(stop_detail.opcode, UINT32_C(0xad0a0000));
  T64_CHECK_INT(machine->cpu.gpr[8], UINT64_C(0x0000000013ff0000));

  T64_CHECK(fixtures_dir != NULL && fixtures_dir[0] != '\0');
  for (variant_index = 0u;
       variant_index < sizeof authored_variants / sizeof authored_variants[0];
       variant_index++) {
    char path[512];
    FILE *file;
    size_t actual;
    int extra;
    int read_error;
    int close_error;

    if (fixtures_dir == NULL ||
        snprintf(path, sizeof path, "%s/%s", fixtures_dir,
                 authored_variants[variant_index]) >= (int)sizeof path) {
      T64_CHECK(false);
      continue;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
      T64_CHECK(false);
      continue;
    }
    actual = fread(variant_rom, 1u, sizeof variant_rom, file);
    extra = fgetc(file);
    read_error = ferror(file);
    close_error = fclose(file);
    T64_CHECK(actual > UINT32_C(0x1000) && extra == EOF &&
              read_error == 0 && close_error == 0);
    if (actual > UINT32_C(0x1000) && extra == EOF && read_error == 0 &&
        close_error == 0) {
      T64_CHECK_INT(t64_machine_admit_rom(machine, variant_rom, actual),
                    T64_STATUS_OK);
      T64_CHECK(machine->has_image);
    }
  }

  puts(t64_test_failures == 0
           ? "ok 1 - qualified warm-device contract admits authored ROM and executes RI path"
           : "not ok 1 - qualified warm-device contract admits authored ROM and executes RI path");
  puts("1..1");
  t64_machine_destroy(machine);
  t64_machine_destroy(interleaved_machine);
  return t64_test_failures == 0 ? 0 : 1;
}
