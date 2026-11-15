#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "t64_internal.h"
#include "t64_sha256.h"
#include "tendies64/t64_machine.h"

#define COLD_HELLO_BYTES 4280u
#define COLD_ROM_LIMIT (1u << 20)
#define COLD_TICK_LIMIT UINT64_C(600000)
#define COLD_TRACE_STEP UINT64_C(16)
#define COLD_CAPTURE_CAPACITY 4096u
#define COLD_RECENT_RETIREMENTS 4u
#define COLD_RI_BASE UINT32_C(0x04700000)
#define COLD_RI_LIMIT UINT32_C(0x04800000)
#define COLD_MI_MODE UINT32_C(0x04300000)
#define COLD_RDRAM_DEVICE_BASE UINT32_C(0x03f00000)
#define COLD_RDRAM_BROADCAST_BASE UINT32_C(0x03f80000)
#define COLD_RDRAM_END UINT32_C(0x04000000)
#define COLD_RDRAM_DEVICE_STRIDE UINT32_C(0x400)
#define COLD_ABSENT_RDRAM_WORD UINT32_C(0x00400000)
#define COLD_PROFILE_A "ntsc-cart-4m-openipl3-rdram-a-t1-v1"
#define COLD_PROFILE_B "ntsc-cart-4m-openipl3-rdram-b-t1-v1"
#define COLD_COMPLETION_PC UINT32_C(0x80000478)
#define COLD_COMPLETION_OPCODE UINT32_C(0x0000000d)
#define COLD_LAST_RETIRED_PC UINT32_C(0x80000474)
#define COLD_LAST_RETIRED_OPCODE UINT32_C(0xa50e0016)
#define COLD_HELLO_SHA256 \
  "fa276cc175a4e764b6fd5413193c44b51f38fd8135e33e400032827e02f27c7c"
#define COLD_IPL3_PAYLOAD_SHA256 \
  "f716a90b1d36ec4253d096790791256d3543455e54473a755ff8c3d1199b15d5"

static const uint8_t COLD_EXPECTED_HELLO[] =
    "Tendies64 hello via open IPL3\n";

typedef struct {
  uint8_t bytes[COLD_HELLO_BYTES];
  size_t length;
  char sha256[T64_SHA256_HEX_SIZE];
} cold_rom;

typedef struct {
  bool found;
  bool pending;
  bool write;
  uint32_t address;
  uint32_t width;
  uint32_t value;
  uint32_t pc;
  uint32_t opcode;
  uint64_t tick;
} cold_operation;

typedef struct {
  uint8_t bytes[COLD_CAPTURE_CAPACITY];
  size_t length;
  uint64_t attempted;
  bool exact;
  bool lost;
} cold_output;

typedef struct {
  cold_operation pending;
  uint64_t operation_order;
  uint64_t ri_config_order;
  uint64_t ri_current_load_order;
  uint64_t ri_select_order;
  uint64_t ri_reset_order;
  uint64_t ri_standard_order;
  uint64_t repeat_order;
  uint64_t broadcast_order;
  uint64_t device_id_order;
  uint64_t calibration_order;
  uint64_t capacity_order;
  uint64_t upper_mode_set_order;
  uint64_t upper_mode_clear_order;
  uint64_t ri_config_tick;
  uint64_t ri_reset_tick;
  uint64_t ri_standard_tick;
  uint64_t calibrated_devices;
  bool completion_seen;
  bool unsupported_seen;
  uint32_t unsupported_pc;
  uint32_t unsupported_opcode;
  uint32_t recent_retirement_pc[COLD_RECENT_RETIREMENTS];
  uint32_t recent_retirement_opcode[COLD_RECENT_RETIREMENTS];
  size_t recent_retirement_count;
  size_t recent_retirement_next;
  bool have_previous_retirement;
  uint64_t previous_retirement_pc;
  uint32_t previous_retirement_opcode;
  bool exception_vector_predecessor_seen;
  uint64_t exception_vector_predecessor_pc;
  uint32_t exception_vector_predecessor_opcode;
  bool header_entry_retired;
  uint64_t header_entry_tick;
  uint32_t header_entry_opcode;
  bool boot_info_write_pending;
  bool boot_info_write_seen;
  uint32_t boot_info_write_address;
  uint32_t boot_info_write_value;
  uint32_t boot_info_write_width;
  uint64_t boot_info_write_tick;
  uint64_t boot_info_write_pc;
  uint32_t boot_info_write_opcode;
  uint32_t terminal_pc;
  uint32_t terminal_opcode;
  uint32_t last_retired_pc;
  uint32_t last_retired_opcode;
} cold_run_observation;

typedef struct {
  cold_output output;
  uint8_t profile_digest[T64_DIGEST_SIZE];
  cold_run_observation observation;
  uint64_t stop_tick;
  uint64_t retired_instructions;
  uint32_t stop_reason;
  uint32_t terminal_pc;
  uint32_t terminal_opcode;
  bool terminal_in_delay_slot;
  bool protocol_state_valid;
  bool cpu_stopped;
  uint32_t cpu_current_pc;
  uint32_t cpu_last_pc;
  uint32_t cpu_last_opcode;
  uint32_t cpu_stop_pc;
  uint32_t cpu_stop_opcode;
  uint32_t cpu_attempt_outcome;
  bool pending_exception_valid;
  uint32_t pending_exception_code;
  uint64_t pending_exception_address;
  uint64_t pending_exception_epc;
  bool pending_exception_bd;
  uint32_t cp0_cause;
  uint32_t cp0_status;
  bool epc_instruction_available;
  uint32_t epc_instruction;
  uint32_t sp_status;
  bool protocol_upper_mode;
  bool has_store_attempt;
  uint32_t attempted_virtual_address;
  uint32_t attempted_physical_address;
  uint32_t attempted_store_value;
  uint32_t pi_dram_address;
  uint32_t pi_cart_address;
  uint32_t pi_write_length;
  uint32_t pi_active_length;
  size_t image_length;
  uint64_t cpu_s0;
} cold_profile_result;

static bool allocate_machine(void *context, size_t size, size_t alignment,
                             t64_host_storage *storage_out) {
  void *data = NULL;
  size_t actual_alignment = alignment < sizeof(void *) ? sizeof(void *) : alignment;
  (void)context;
  if (storage_out == NULL || posix_memalign(&data, actual_alignment, size) != 0) {
    return false;
  }
  storage_out->data = data;
  storage_out->capacity = size;
  storage_out->alignment = actual_alignment;
  return true;
}

static void release_machine(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static void digest_hex(const void *bytes, size_t length,
                       char output[T64_SHA256_HEX_SIZE]) {
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  t64_sha256_ctx context;
  t64_sha256_init(&context);
  t64_sha256_update(&context, bytes, length);
  t64_sha256_final(&context, digest);
  t64_sha256_hex(digest, output);
}

static const char *read_cleared_hello(const char *path, cold_rom *rom) {
  struct stat metadata;
  size_t offset = 0u;
  int fd;
  if (path == NULL || rom == NULL) return "fixture-argument";
  fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
  if (fd < 0) return "fixture-open";
  if (fstat(fd, &metadata) != 0 || !S_ISREG(metadata.st_mode) ||
      metadata.st_size != (off_t)COLD_HELLO_BYTES ||
      (uint64_t)metadata.st_size > COLD_ROM_LIMIT) {
    (void)close(fd);
    return "fixture-metadata";
  }
  while (offset < sizeof rom->bytes) {
    ssize_t count = read(fd, rom->bytes + offset, sizeof rom->bytes - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) {
      (void)close(fd);
      return "fixture-read";
    }
    offset += (size_t)count;
  }
  if (close(fd) != 0) return "fixture-close";
  rom->length = offset;
  digest_hex(rom->bytes, rom->length, rom->sha256);
  if (strcmp(rom->sha256, COLD_HELLO_SHA256) != 0) {
    return "fixture-identity";
  }
  {
    char payload_digest[T64_SHA256_HEX_SIZE];
    digest_hex(rom->bytes + 0x40u, 4032u, payload_digest);
    if (strcmp(payload_digest, COLD_IPL3_PAYLOAD_SHA256) != 0) {
      return "fixture-ipl3-identity";
    }
  }
  return NULL;
}

static bool known_profile(const char *profile_id) {
  return profile_id != NULL &&
         (strcmp(profile_id, COLD_PROFILE_A) == 0 ||
          strcmp(profile_id, COLD_PROFILE_B) == 0);
}

static bool is_target_operation(uint32_t address) {
  return (address >= COLD_RI_BASE && address < COLD_RI_LIMIT) ||
         address == COLD_ABSENT_RDRAM_WORD;
}

static bool collect_trace(t64_machine *machine, cold_operation *operation,
                          uint64_t *dropped_total) {
  t64_trace_record records[T64_TRACE_RECORD_CAPACITY];
  size_t count = 0u;
  uint64_t dropped = 0u;
  bool saturated = false;
  size_t index;
  if (t64_machine_trace_drain(machine, records,
                              T64_TRACE_RECORD_CAPACITY, &count, &dropped,
                              &saturated) != T64_STATUS_OK || saturated ||
      dropped != 0u || count > T64_TRACE_RECORD_CAPACITY) {
    return false;
  }
  if (dropped > *dropped_total) *dropped_total = dropped;
  for (index = 0u; index < count; index++) {
    const t64_trace_record *record = &records[index];
    if (record->device == T64_TRACE_DEVICE_BUS &&
        (record->kind == T64_TRACE_KIND_BUS_READ8 ||
         record->kind == T64_TRACE_KIND_BUS_WRITE8 ||
         record->kind == T64_TRACE_KIND_BUS_READ16 ||
         record->kind == T64_TRACE_KIND_BUS_WRITE16 ||
         record->kind == T64_TRACE_KIND_BUS_READ32 ||
         record->kind == T64_TRACE_KIND_BUS_WRITE32)) {
      uint32_t address = (uint32_t)(record->value >> 32);
      if (!operation->found && is_target_operation(address)) {
        operation->pending = true;
        operation->address = address;
        operation->value = (uint32_t)record->value;
        operation->width = (record->kind == T64_TRACE_KIND_BUS_READ8 ||
                            record->kind == T64_TRACE_KIND_BUS_WRITE8)
                               ? 1u
                               : (record->kind == T64_TRACE_KIND_BUS_READ16 ||
                                  record->kind == T64_TRACE_KIND_BUS_WRITE16)
                                     ? 2u : 4u;
        operation->write = record->kind == T64_TRACE_KIND_BUS_WRITE8 ||
                           record->kind == T64_TRACE_KIND_BUS_WRITE16 ||
                           record->kind == T64_TRACE_KIND_BUS_WRITE32;
        operation->tick = record->tick;
      }
    } else if (record->device == T64_TRACE_DEVICE_MACHINE &&
               record->kind == T64_TRACE_KIND_CPU_RETIREMENT &&
               operation->pending && !operation->found) {
      operation->pc = (uint32_t)(record->value >> 32);
      operation->opcode = (uint32_t)record->value;
      operation->found = true;
      operation->pending = false;
    }
  }
  return true;
}

static bool is_bus_trace_kind(uint32_t kind) {
  return kind == T64_TRACE_KIND_BUS_READ8 ||
         kind == T64_TRACE_KIND_BUS_WRITE8 ||
         kind == T64_TRACE_KIND_BUS_READ16 ||
         kind == T64_TRACE_KIND_BUS_WRITE16 ||
         kind == T64_TRACE_KIND_BUS_READ32 ||
         kind == T64_TRACE_KIND_BUS_WRITE32;
}

/* CPU-retirement trace records carry the low 32-bit PC. Normalize that
   segment form to the canonical sign-extended VR4300 address before comparing
   it with the profile's pinned virtual addresses. */
static uint64_t canonical_trace_pc(uint32_t pc) {
  return (pc & UINT32_C(0x80000000)) != 0u
             ? UINT64_C(0xffffffff00000000) | (uint64_t)pc
             : (uint64_t)pc;
}

static bool is_protocol_address(uint32_t address) {
  return (address >= COLD_RI_BASE && address < COLD_RI_LIMIT) ||
         address == COLD_MI_MODE ||
         (address >= COLD_RDRAM_DEVICE_BASE && address < COLD_RDRAM_END);
}

static void observe_protocol_operation(cold_run_observation *observation,
                                       const cold_operation *operation) {
  uint64_t order;

  if (observation == NULL || operation == NULL) return;
  order = ++observation->operation_order;
  if (operation->address == UINT32_C(0x04700004) && operation->write &&
      operation->width == 4u && operation->value == UINT32_C(0x40) &&
      observation->ri_config_order == 0u) {
    observation->ri_config_order = order;
    observation->ri_config_tick = operation->tick;
  } else if (operation->address == UINT32_C(0x04700008) &&
             operation->write && operation->width == 4u &&
             operation->value == 0u &&
             observation->ri_config_order != 0u &&
             observation->ri_current_load_order == 0u &&
             operation->tick >= observation->ri_config_tick &&
             operation->tick - observation->ri_config_tick >=
                 COLD_TRACE_STEP * UINT64_C(16)) {
    observation->ri_current_load_order = order;
  } else if (operation->address == UINT32_C(0x0470000c) &&
             operation->write && operation->width == 4u &&
             operation->value == UINT32_C(0x14) &&
             observation->ri_current_load_order != 0u &&
             observation->ri_select_order == 0u) {
    observation->ri_select_order = order;
  } else if (operation->address == UINT32_C(0x04700000) &&
             operation->write && operation->width == 4u &&
             operation->value == 0u && observation->ri_select_order != 0u &&
             observation->ri_reset_order == 0u) {
    observation->ri_reset_order = order;
    observation->ri_reset_tick = operation->tick;
  } else if (operation->address == UINT32_C(0x04700000) &&
             operation->write && operation->width == 4u &&
             operation->value == UINT32_C(0x0e) &&
             observation->ri_reset_order != 0u &&
             observation->ri_standard_order == 0u &&
             operation->tick >= observation->ri_reset_tick &&
             operation->tick - observation->ri_reset_tick >=
                 COLD_TRACE_STEP * UINT64_C(16)) {
    observation->ri_standard_order = order;
    observation->ri_standard_tick = operation->tick;
  } else if (operation->address == COLD_MI_MODE && operation->write &&
             operation->width == 4u &&
             operation->value == UINT32_C(0x2000) &&
             observation->broadcast_order != 0u &&
             observation->upper_mode_set_order == 0u) {
    observation->upper_mode_set_order = order;
  } else if (operation->address == COLD_MI_MODE && operation->write &&
             operation->width == 4u &&
             operation->value == UINT32_C(0x1000) &&
             observation->upper_mode_set_order != 0u &&
             observation->upper_mode_clear_order == 0u) {
    observation->upper_mode_clear_order = order;
  } else if (operation->address == COLD_MI_MODE && operation->write &&
             operation->width == 4u &&
             operation->value == UINT32_C(0x10f) &&
             observation->ri_standard_order != 0u &&
             observation->repeat_order == 0u &&
             operation->tick >= observation->ri_standard_tick &&
             operation->tick - observation->ri_standard_tick >=
                 COLD_TRACE_STEP * UINT64_C(16)) {
    observation->repeat_order = order;
  } else if (operation->address >= COLD_RDRAM_BROADCAST_BASE &&
             operation->address < COLD_RDRAM_END && operation->write &&
             observation->repeat_order != 0u &&
             observation->broadcast_order == 0u) {
    observation->broadcast_order = order;
  } else if (operation->address >= COLD_RDRAM_DEVICE_BASE &&
             operation->address < COLD_RDRAM_BROADCAST_BASE &&
             operation->write && observation->broadcast_order != 0u) {
    uint32_t offset = operation->address - COLD_RDRAM_DEVICE_BASE;
    uint32_t device_id = offset / COLD_RDRAM_DEVICE_STRIDE;
    uint32_t register_index =
        (offset % COLD_RDRAM_DEVICE_STRIDE) / UINT32_C(4);

    if (register_index == 1u && observation->device_id_order == 0u) {
      observation->device_id_order = order;
    } else if (register_index == 3u && device_id == 0u &&
               (operation->value & UINT32_C(0x80000000)) != 0u &&
               observation->device_id_order != 0u) {
      observation->calibrated_devices |= UINT64_C(1);
      if (observation->calibration_order == 0u) {
        observation->calibration_order = order;
      }
    } else if (register_index == 3u && device_id == 2u &&
               (operation->value & UINT32_C(0x80000000)) != 0u &&
               observation->device_id_order != 0u) {
      observation->calibrated_devices |= UINT64_C(2);
      if (observation->calibration_order == 0u) {
        observation->calibration_order = order;
      }
    }
  } else if (operation->address >= COLD_RDRAM_DEVICE_BASE &&
             operation->address < COLD_RDRAM_BROADCAST_BASE &&
             !operation->write && observation->broadcast_order != 0u) {
    uint32_t offset = operation->address - COLD_RDRAM_DEVICE_BASE;
    uint32_t device_id = offset / COLD_RDRAM_DEVICE_STRIDE;
    uint32_t register_index =
        (offset % COLD_RDRAM_DEVICE_STRIDE) / UINT32_C(4);

    if (register_index == 3u && device_id == 4u && operation->value == 0u &&
        observation->calibrated_devices == UINT64_C(3) &&
        observation->capacity_order == 0u) {
      observation->capacity_order = order;
    }
  }
}

static bool collect_complete_trace(t64_machine *machine,
                                   cold_run_observation *observation,
                                   uint64_t *dropped_total) {
  t64_trace_record records[T64_TRACE_RECORD_CAPACITY];
  size_t count = 0u;
  uint64_t dropped = 0u;
  bool saturated = false;
  size_t index;

  if (machine == NULL || observation == NULL || dropped_total == NULL ||
      t64_machine_trace_drain(machine, records,
                              T64_TRACE_RECORD_CAPACITY, &count, &dropped,
                              &saturated) != T64_STATUS_OK || saturated ||
      dropped != 0u || count > T64_TRACE_RECORD_CAPACITY) {
    return false;
  }
  if (dropped > *dropped_total) *dropped_total = dropped;
  for (index = 0u; index < count; index++) {
    const t64_trace_record *record = &records[index];

    if (record->device == T64_TRACE_DEVICE_BUS &&
        is_bus_trace_kind(record->kind)) {
      uint32_t address = (uint32_t)(record->value >> 32);
      uint32_t width =
          (record->kind == T64_TRACE_KIND_BUS_READ8 ||
           record->kind == T64_TRACE_KIND_BUS_WRITE8)
              ? 1u
              : (record->kind == T64_TRACE_KIND_BUS_READ16 ||
                 record->kind == T64_TRACE_KIND_BUS_WRITE16)
                    ? 2u
                    : 4u;
      bool write = record->kind == T64_TRACE_KIND_BUS_WRITE8 ||
                   record->kind == T64_TRACE_KIND_BUS_WRITE16 ||
                   record->kind == T64_TRACE_KIND_BUS_WRITE32;

      if (write && !observation->boot_info_write_seen &&
          !observation->boot_info_write_pending &&
          address <= UINT32_C(0x00000300) &&
          (uint64_t)address + width > UINT64_C(0x00000300)) {
        observation->boot_info_write_pending = true;
        observation->boot_info_write_address = address;
        observation->boot_info_write_value = (uint32_t)record->value;
        observation->boot_info_write_width = width;
        observation->boot_info_write_tick = record->tick;
      }

      if (!is_protocol_address(address)) continue;
      if (observation->pending.pending) return false;
      observation->pending.pending = true;
      observation->pending.address = address;
      observation->pending.value = (uint32_t)record->value;
      observation->pending.tick = record->tick;
      observation->pending.width =
          (record->kind == T64_TRACE_KIND_BUS_READ8 ||
           record->kind == T64_TRACE_KIND_BUS_WRITE8)
              ? 1u
              : (record->kind == T64_TRACE_KIND_BUS_READ16 ||
                 record->kind == T64_TRACE_KIND_BUS_WRITE16)
                    ? 2u
                    : 4u;
      observation->pending.write =
          record->kind == T64_TRACE_KIND_BUS_WRITE8 ||
          record->kind == T64_TRACE_KIND_BUS_WRITE16 ||
          record->kind == T64_TRACE_KIND_BUS_WRITE32;
    } else if (record->device == T64_TRACE_DEVICE_MACHINE &&
               record->kind == T64_TRACE_KIND_CPU_RETIREMENT) {
      observation->last_retired_pc = (uint32_t)(record->value >> 32);
      observation->last_retired_opcode = (uint32_t)record->value;
      {
        uint64_t retired_pc_full =
            canonical_trace_pc(observation->last_retired_pc);

        if (observation->boot_info_write_pending) {
          observation->boot_info_write_seen = true;
          observation->boot_info_write_pc = retired_pc_full;
          observation->boot_info_write_opcode =
              observation->last_retired_opcode;
          observation->boot_info_write_pending = false;
        }
        if (retired_pc_full == UINT64_C(0xffffffff80000400) &&
            !observation->header_entry_retired) {
          observation->header_entry_retired = true;
          observation->header_entry_tick = record->tick;
          observation->header_entry_opcode =
              observation->last_retired_opcode;
        }
        if (retired_pc_full == UINT64_C(0xffffffff80000180) &&
            !observation->exception_vector_predecessor_seen &&
            observation->have_previous_retirement) {
          observation->exception_vector_predecessor_seen = true;
          observation->exception_vector_predecessor_pc =
              observation->previous_retirement_pc;
          observation->exception_vector_predecessor_opcode =
              observation->previous_retirement_opcode;
        }
      }
      observation->recent_retirement_pc[
          observation->recent_retirement_next] =
          observation->last_retired_pc;
      observation->recent_retirement_opcode[
          observation->recent_retirement_next] =
          observation->last_retired_opcode;
      observation->recent_retirement_next =
          (observation->recent_retirement_next + 1u) %
          COLD_RECENT_RETIREMENTS;
      if (observation->recent_retirement_count <
          COLD_RECENT_RETIREMENTS) {
        observation->recent_retirement_count++;
      }
      if (observation->pending.pending) {
        observation->pending.pc = observation->last_retired_pc;
        observation->pending.opcode = observation->last_retired_opcode;
        observation->pending.found = true;
        observe_protocol_operation(observation, &observation->pending);
        observation->pending.pending = false;
      }
      observation->previous_retirement_pc =
          canonical_trace_pc(observation->last_retired_pc);
      observation->previous_retirement_opcode =
          observation->last_retired_opcode;
      observation->have_previous_retirement = true;
    } else if (record->device == T64_TRACE_DEVICE_MACHINE &&
               record->kind == T64_TRACE_KIND_CPU_COMPLETION) {
      if (observation->completion_seen || observation->pending.pending) {
        return false;
      }
      observation->completion_seen = true;
      observation->terminal_pc = (uint32_t)(record->value >> 32);
      observation->terminal_opcode = (uint32_t)record->value;
    } else if (record->device == T64_TRACE_DEVICE_MACHINE &&
               record->kind == T64_TRACE_KIND_CPU_UNSUPPORTED) {
      if (observation->unsupported_seen) return false;
      observation->unsupported_seen = true;
      observation->unsupported_pc = (uint32_t)(record->value >> 32);
      observation->unsupported_opcode = (uint32_t)record->value;
    }
  }
  return true;
}

static bool protocol_trace_complete(const cold_run_observation *observation) {
  return observation != NULL && observation->ri_config_order != 0u &&
         observation->ri_current_load_order > observation->ri_config_order &&
         observation->ri_select_order > observation->ri_current_load_order &&
         observation->ri_reset_order > observation->ri_select_order &&
         observation->ri_standard_order > observation->ri_reset_order &&
         observation->repeat_order > observation->ri_standard_order &&
         observation->broadcast_order > observation->repeat_order &&
         observation->upper_mode_set_order > observation->broadcast_order &&
         observation->upper_mode_clear_order >
             observation->upper_mode_set_order &&
         observation->device_id_order > observation->broadcast_order &&
         observation->calibration_order > observation->device_id_order &&
         observation->calibrated_devices == UINT64_C(3) &&
         observation->capacity_order > observation->calibration_order;
}

static bool protocol_state_complete(const t64_machine *machine) {
  const t64_rdram_protocol_state *protocol;
  size_t index;
  const uint16_t required_broadcast =
      (uint16_t)((UINT16_C(1) << 1) | (UINT16_C(1) << 2) |
                 (UINT16_C(1) << 3) | (UINT16_C(1) << 5));

  if (machine == NULL) return false;
  protocol = &machine->guest_devices.rdram_protocol;
  if (!protocol->ri_current_load_known || !protocol->mi_repeat_enabled ||
      protocol->mi_repeat_length != UINT32_C(16) ||
      (protocol->broadcast_registers_known & required_broadcast) !=
          required_broadcast ||
      protocol->logical_device_count != T64_RDRAM_PROTOCOL_SLOT_COUNT ||
      !protocol->capacity_scan_complete ||
      protocol->mi_upper_mode ||
      protocol->logical_capacity_bytes != machine->profile.rdram_bytes) {
    return false;
  }
  for (index = 0u; index < T64_RDRAM_PROTOCOL_SLOT_COUNT; index++) {
    const t64_rdram_device_state *device = &protocol->devices[index];

    if (!device->present || !device->device_id_known ||
        device->device_id != (uint16_t)(index * 2u) ||
        device->calibration_state != T64_RDRAM_CALIBRATION_AUTO_COMMAND) {
      return false;
    }
  }
  return true;
}

static bool is_store_attempt(uint32_t opcode) {
  switch (opcode >> 26) {
  case 0x28u: case 0x29u: case 0x2au: case 0x2bu:
  case 0x2cu: case 0x2du: case 0x2eu: case 0x39u:
  case 0x3du: case 0x3fu:
    return true;
  default:
    return false;
  }
}

static const char *bus_owner_name(uint32_t physical_address) {
  if (physical_address == COLD_MI_MODE) return "mi-mode";
  if (physical_address >= COLD_RDRAM_DEVICE_BASE &&
      physical_address < COLD_RDRAM_END) return "rdram-protocol";
  if (physical_address >= COLD_RI_BASE &&
      physical_address < COLD_RI_LIMIT) return "ri-registers";
  if (physical_address < UINT32_C(0x00400000)) return "installed-rdram";
  return "unclassified";
}

static bool read_ram_word_at_virtual_address(const t64_machine *machine,
                                             uint64_t virtual_address,
                                             uint32_t *word_out) {
  uint32_t address = (uint32_t)virtual_address;
  uint32_t physical_address = address;
  const uint8_t *bytes;

  if (machine == NULL || machine->ram == NULL || word_out == NULL) {
    return false;
  }
  if ((address & UINT32_C(0xe0000000)) == UINT32_C(0x80000000) ||
      (address & UINT32_C(0xe0000000)) == UINT32_C(0xa0000000)) {
    physical_address = address & UINT32_C(0x1fffffff);
  }
  if (machine->profile.rdram_bytes < sizeof(uint32_t) ||
      physical_address > machine->profile.rdram_bytes - sizeof(uint32_t)) {
    return false;
  }
  bytes = machine->ram + physical_address;
  *word_out = ((uint32_t)bytes[0] << 24) |
              ((uint32_t)bytes[1] << 16) |
              ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
  return true;
}

static void capture_profile_cpu_state(const t64_machine *machine,
                                      cold_profile_result *result) {
  uint32_t opcode;

  result->cpu_stopped = machine->cpu.stopped;
  result->pi_dram_address = machine->pi.dram_address;
  result->pi_cart_address = machine->pi.cart_address;
  result->pi_write_length = machine->pi.write_length;
  result->pi_active_length = machine->pi.active_length;
  result->image_length = machine->image_length;
  result->cpu_s0 = machine->cpu.gpr[16];
  result->cpu_current_pc = machine->cpu.pc;
  result->cpu_last_pc = machine->cpu.last_pc;
  result->cpu_last_opcode = machine->cpu.last_opcode;
  result->cpu_stop_pc = machine->cpu.stopped ? machine->cpu.stop_pc : 0u;
  result->cpu_stop_opcode = machine->cpu.stopped
                                ? machine->cpu.stop_opcode
                                : 0u;
  result->cpu_attempt_outcome = machine->cpu.attempt_outcome;
  result->pending_exception_valid = machine->cpu.pending_exception_valid;
  result->pending_exception_code = machine->cpu.pending_exception_code;
  result->pending_exception_address =
      machine->cpu.pending_exception_address;
  result->pending_exception_epc = machine->cpu.pending_exception_epc;
  result->pending_exception_bd = machine->cpu.pending_exception_bd;
  result->cp0_cause = (uint32_t)machine->cpu.cp0[13];
  result->cp0_status = (uint32_t)machine->cpu.cp0[12];
  result->epc_instruction_available =
      result->pending_exception_valid &&
      read_ram_word_at_virtual_address(machine,
                                       result->pending_exception_epc,
                                       &result->epc_instruction);
  result->sp_status = machine->rsp.sp_status;
  result->protocol_upper_mode =
      machine->guest_devices.rdram_protocol.mi_upper_mode;

  opcode = result->cpu_stop_opcode;
  if (!result->cpu_stopped || !is_store_attempt(opcode)) return;
  {
    uint32_t base_register = (opcode >> 21) & UINT32_C(0x1f);
    uint32_t source_register = (opcode >> 16) & UINT32_C(0x1f);
    int32_t offset = (int16_t)(uint16_t)opcode;
    uint32_t virtual_address = (uint32_t)(
        machine->cpu.gpr[base_register] + (uint64_t)(int64_t)offset);
    uint32_t physical_address = virtual_address;

    if ((virtual_address & UINT32_C(0xe0000000)) ==
            UINT32_C(0x80000000) ||
        (virtual_address & UINT32_C(0xe0000000)) ==
            UINT32_C(0xa0000000)) {
      physical_address = virtual_address & UINT32_C(0x1fffffff);
    }
    result->has_store_attempt = true;
    result->attempted_virtual_address = virtual_address;
    result->attempted_physical_address = physical_address;
    result->attempted_store_value =
        (uint32_t)machine->cpu.gpr[source_register];
  }
}

static bool bytes_to_hex(const uint8_t *bytes, size_t length, char *output,
                         size_t capacity) {
  static const char digits[] = "0123456789abcdef";
  size_t index;

  if (bytes == NULL || output == NULL || length > (capacity - 1u) / 2u) {
    return false;
  }
  for (index = 0u; index < length; index++) {
    output[index * 2u] = digits[bytes[index] >> 4];
    output[index * 2u + 1u] = digits[bytes[index] & UINT8_C(0x0f)];
  }
  output[length * 2u] = '\0';
  return true;
}

/* The complete-CPU profiles take an architectural BREAK exception. Qualify
 * the authored boundary in the runner without changing guest CPU semantics. */
static bool authored_break_reached(const cold_profile_result *result) {
  return result->pending_exception_valid &&
         result->pending_exception_code == 9u &&
         result->pending_exception_epc ==
             canonical_trace_pc(COLD_COMPLETION_PC) &&
         !result->pending_exception_bd &&
         result->cpu_attempt_outcome == T64_CPU_ATTEMPT_GUEST_EXCEPTION &&
         result->cpu_last_pc == COLD_COMPLETION_PC &&
         result->cpu_last_opcode == COLD_COMPLETION_OPCODE &&
         result->epc_instruction_available &&
         result->epc_instruction == COLD_COMPLETION_OPCODE &&
         result->observation.last_retired_pc == COLD_LAST_RETIRED_PC &&
         result->observation.last_retired_opcode == COLD_LAST_RETIRED_OPCODE;
}

static bool literal_output_valid(const cold_output *output) {
  const size_t length = sizeof COLD_EXPECTED_HELLO - 1u;
  return output->exact && !output->lost && output->length == length &&
         output->attempted == (uint64_t)length &&
         memcmp(output->bytes, COLD_EXPECTED_HELLO, length) == 0;
}

static bool output_controls(void) {
  cold_output baseline = {0};
  cold_output changed;
  unsigned variant;
  baseline.length = sizeof COLD_EXPECTED_HELLO - 1u;
  baseline.attempted = (uint64_t)baseline.length;
  baseline.exact = true;
  memcpy(baseline.bytes, COLD_EXPECTED_HELLO, baseline.length);
  if (!literal_output_valid(&baseline)) return false;
  for (variant = 0u; variant < 5u; variant++) {
    changed = baseline;
    switch (variant) {
    case 0u: changed.length--; break;
    case 1u: changed.exact = false; break;
    case 2u: changed.lost = true; break;
    case 3u: changed.attempted++; break;
    default: changed.bytes[0] ^= UINT8_C(1); break;
    }
    if (literal_output_valid(&changed)) return false;
  }
  return true;
}

static int terminal_controls(void) {
  cold_profile_result baseline = {0};
  cold_profile_result changed;
  if (!output_controls()) return 1;
  baseline.pending_exception_valid = true;
  baseline.pending_exception_code = 9u;
  baseline.pending_exception_epc = canonical_trace_pc(COLD_COMPLETION_PC);
  baseline.cpu_attempt_outcome = T64_CPU_ATTEMPT_GUEST_EXCEPTION;
  baseline.cpu_last_pc = COLD_COMPLETION_PC;
  baseline.cpu_last_opcode = COLD_COMPLETION_OPCODE;
  baseline.epc_instruction_available = true;
  baseline.epc_instruction = COLD_COMPLETION_OPCODE;
  baseline.observation.last_retired_pc = COLD_LAST_RETIRED_PC;
  baseline.observation.last_retired_opcode = COLD_LAST_RETIRED_OPCODE;
  if (!authored_break_reached(&baseline)) return 1;
#define REFUSE_TERMINAL_CHANGE(change) \
  do { changed = baseline; change; \
       if (authored_break_reached(&changed)) return 1; } while (0)
  REFUSE_TERMINAL_CHANGE(changed.pending_exception_valid = false);
  REFUSE_TERMINAL_CHANGE(changed.pending_exception_code = 10u);
  REFUSE_TERMINAL_CHANGE(changed.pending_exception_epc += 4u);
  REFUSE_TERMINAL_CHANGE(changed.pending_exception_bd = true);
  REFUSE_TERMINAL_CHANGE(changed.cpu_attempt_outcome = T64_CPU_ATTEMPT_RETIRED);
  REFUSE_TERMINAL_CHANGE(changed.cpu_last_pc += 4u);
  REFUSE_TERMINAL_CHANGE(changed.cpu_last_opcode ^= 64u);
  REFUSE_TERMINAL_CHANGE(changed.epc_instruction_available = false);
  REFUSE_TERMINAL_CHANGE(changed.epc_instruction ^= 64u);
  REFUSE_TERMINAL_CHANGE(changed.observation.last_retired_pc -= 4u);
  REFUSE_TERMINAL_CHANGE(changed.observation.last_retired_opcode ^= 1u);
#undef REFUSE_TERMINAL_CHANGE
  {
    /* Independent authored wire operations: address, value, tick, write.
     * Omitted/altered source families must not qualify a transcript. */
    static const uint32_t steps[][4] = {
        {0x04700004u, 0x40u, 0u, 1u},
        {0x04700008u, 0u, 256u, 1u},
        {0x0470000cu, 0x14u, 257u, 1u},
        {0x04700000u, 0u, 258u, 1u},
        {0x04700000u, 0x0eu, 514u, 1u},
        {0x04300000u, 0x10fu, 770u, 1u},
        {0x03f80008u, 0xefbeaddeu, 771u, 1u},
        {0x04300000u, 0x2000u, 772u, 1u},
        {0x04300000u, 0x1000u, 773u, 1u},
        {0x03f7fc04u, 0u, 774u, 1u},
        {0x03f0000cu, 0xc6000000u, 775u, 1u},
        {0x03f7fc04u, 0x08000000u, 776u, 1u},
        {0x03f0080cu, 0xc6000000u, 777u, 1u},
        {0x03f0100cu, 0x46000000u, 778u, 1u},
        {0x03f0100cu, 0u, 779u, 0u},
    };
    unsigned variant;
    for (variant = 0u; variant < 9u; variant++) {
      cold_run_observation observation = {0};
      size_t index;
      for (index = 0u; index < sizeof steps / sizeof steps[0]; index++) {
        cold_operation operation = {0};
        if ((variant == 1u && (index == 10u || index == 12u)) ||
            (variant == 3u && index == 14u) ||
            (variant == 4u && index == 1u) ||
            (variant == 5u && index == 8u) ||
            (variant == 6u && index == 6u)) continue;
        operation.address = steps[index][0];
        operation.value = steps[index][1];
        operation.tick = steps[index][2];
        operation.write = steps[index][3] != 0u;
        operation.width = 4u;
        if (variant == 2u && (index == 10u || index == 12u)) {
          operation.value = 0x46000000u;
        }
        if (variant == 7u && index == 1u) operation.tick = 255u;
        if (variant == 8u && index == 14u) operation.value = 1u;
        observe_protocol_operation(&observation, &operation);
      }
      if (protocol_trace_complete(&observation) != (variant == 0u)) return 1;
    }
  }
  (void)puts("classification=terminal-controls-pass");
  return 0;
}

static bool drain_output(t64_machine *machine, cold_output *output) {
  t64_output_drain_result result = {0};
  size_t capacity = sizeof output->bytes - output->length;
  if (t64_machine_output_drain(machine, output->bytes + output->length,
                               capacity, &result) != T64_STATUS_OK ||
      result.copied_bytes > capacity) {
    return false;
  }
  output->length += result.copied_bytes;
  output->attempted = result.total_attempted_bytes;
  output->exact = result.total_attempted_bytes_exact;
  output->lost = result.lost_output;
  return output->exact && !output->lost;
}

static const char *stop_name(uint32_t reason) {
  switch (reason) {
  case T64_STOP_REASON_BOUND: return "guest-tick-bound";
  case T64_STOP_REASON_CPU_UNSUPPORTED: return "cpu-unsupported";
  case T64_STOP_REASON_GUEST_COMPLETION: return "software-stop";
  case T64_STOP_REASON_HALT: return "halt";
  case T64_STOP_REASON_QUIESCENT: return "quiescent";
  case T64_STOP_REASON_RESOURCE_LIMIT: return "resource-limit";
  default: return "other";
  }
}

static const char *run_complete_profile(const cold_rom *rom,
                                       const char *profile_id,
                                       cold_profile_result *result) {
  static const t64_host_allocator allocator = {
      allocate_machine, release_machine, NULL};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  t64_stop_result stop = {T64_STOP_REASON_BOUND, 0u};
  uint64_t current_tick = 0u;
  uint64_t dropped_total = 0u;
  uint8_t content_digest[T64_DIGEST_SIZE];
  char content_hex[T64_SHA256_HEX_SIZE];
  const char *error = NULL;

  if (rom == NULL || result == NULL) return "compare-arguments";
  memset(result, 0, sizeof *result);
  result->output.exact = true;
  if (!known_profile(profile_id) ||
      t64_profile_lookup(profile_id, &profile) != T64_STATUS_OK ||
      profile == NULL || strcmp(profile->profile_id, profile_id) != 0 ||
      profile->rdram_bytes != UINT32_C(0x00400000) ||
      profile->expansion_bytes != 0u) {
    return "profile-identity";
  }
  if (t64_profile_hash(profile, result->profile_digest) != T64_STATUS_OK) {
    return "profile-digest";
  }
  if (t64_machine_create(&allocator, profile, &machine) != T64_STATUS_OK ||
      machine == NULL) {
    error = "machine-create";
    goto cleanup;
  }
  if (t64_machine_trace_set_enabled(machine, true) != T64_STATUS_OK ||
      t64_machine_admit_rom(machine, rom->bytes, rom->length) != T64_STATUS_OK ||
      machine->cpu.pc != UINT32_C(0xa4000040) ||
      t64_machine_content_digest(machine, content_digest) != T64_STATUS_OK) {
    error = "boot-entry";
    goto cleanup;
  }
  t64_sha256_hex(content_digest, content_hex);
  if (strcmp(content_hex, rom->sha256) != 0) {
    error = "admitted-content-identity";
    goto cleanup;
  }
  if (!collect_complete_trace(machine, &result->observation, &dropped_total) ||
      !drain_output(machine, &result->output)) {
    error = "initial-observation";
    goto cleanup;
  }
  while (current_tick < COLD_TICK_LIMIT &&
         stop.reason == T64_STOP_REASON_BOUND) {
    uint64_t remaining = COLD_TICK_LIMIT - current_tick;
    uint64_t quantum = result->observation.header_entry_retired
                           ? UINT64_C(1) : COLD_TRACE_STEP;
    uint64_t target = current_tick +
        (remaining < quantum ? remaining : quantum);
    t64_status status = t64_machine_advance(
        machine, &(t64_stop_request){T64_STOP_GUEST_TICK, target}, &stop);
    current_tick = stop.tick;
    if (status != T64_STATUS_OK) {
      if (machine->cpu.stopped) {
        stop.reason = machine->cpu.stop_reason;
      } else {
        error = "advance-status";
        goto cleanup;
      }
    }
    if (machine->cpu.stopped) {
      stop.reason = machine->cpu.stop_reason;
      current_tick = machine->tick;
    }
    if (!collect_complete_trace(machine, &result->observation,
                                &dropped_total) ||
        !drain_output(machine, &result->output)) {
      error = "observation-overflow";
      goto cleanup;
    }
    if (result->observation.header_entry_retired) {
      capture_profile_cpu_state(machine, result);
      if (authored_break_reached(result)) {
        result->observation.completion_seen = true;
        result->observation.terminal_pc = COLD_COMPLETION_PC;
        result->observation.terminal_opcode = COLD_COMPLETION_OPCODE;
        break;
      }
    }
    if (stop.reason != T64_STOP_REASON_BOUND &&
        stop.reason != T64_STOP_REASON_CPU_UNSUPPORTED &&
        stop.reason != T64_STOP_REASON_GUEST_COMPLETION &&
        stop.reason != T64_STOP_REASON_HALT &&
        stop.reason != T64_STOP_REASON_QUIESCENT &&
        stop.reason != T64_STOP_REASON_RESOURCE_LIMIT) {
      error = "unexpected-stop";
      goto cleanup;
    }
  }
  result->stop_tick = current_tick;
  result->retired_instructions = machine->cpu.retired_instructions;
  result->stop_reason = stop.reason;
  result->terminal_pc = result->observation.terminal_pc;
  result->terminal_opcode = result->observation.terminal_opcode;
  result->terminal_in_delay_slot = machine->cpu.delay_slot_pending;
  result->protocol_state_valid = protocol_state_complete(machine);
  capture_profile_cpu_state(machine, result);
  if (result->observation.pending.pending) error = "unpaired-protocol-operation";

cleanup:
  (void)dropped_total;
  if (machine != NULL) t64_machine_destroy(machine);
  return error;
}

static void print_profile_result(const char *profile_id,
                                 const cold_profile_result *result) {
  char output_hex[COLD_CAPTURE_CAPACITY * 2u + 1u];
  char profile_hex[T64_SHA256_HEX_SIZE];
  t64_sha256_hex(result->profile_digest, profile_hex);
  (void)printf("profile-identity profile=%s sha256=%s\n", profile_id, profile_hex);
  bool hex_ok = bytes_to_hex(result->output.bytes, result->output.length,
                             output_hex, sizeof output_hex);
  (void)printf("profile=%s stop=%s stop_tick=%llu retired=%llu terminal=0x%08x/0x%08x completion-seen=%u last-retired=0x%08x/0x%08x terminal-in-delay-slot=%u protocol-trace-complete=%u protocol-state-complete=%u output-bytes=%zu output-exact=%u output-lost=%u hello-hex=%s\n",
               profile_id, result->observation.completion_seen
                               ? "qualified-terminal" : stop_name(result->stop_reason),
               (unsigned long long)result->stop_tick,
               (unsigned long long)result->retired_instructions,
               result->terminal_pc, result->terminal_opcode,
               result->observation.completion_seen ? 1u : 0u,
               result->observation.last_retired_pc,
               result->observation.last_retired_opcode,
               result->terminal_in_delay_slot ? 1u : 0u,
               protocol_trace_complete(&result->observation) ? 1u : 0u,
               result->protocol_state_valid ? 1u : 0u,
               result->output.length, result->output.exact ? 1u : 0u,
               result->output.lost ? 1u : 0u, hex_ok ? output_hex : "invalid");
  (void)printf("cpu-state profile=%s stopped=%u current-pc=0x%08x last-pc=0x%08x/0x%08x attempt-outcome=%u stop-attempt=",
               profile_id, result->cpu_stopped ? 1u : 0u,
               result->cpu_current_pc, result->cpu_last_pc,
               result->cpu_last_opcode, result->cpu_attempt_outcome);
  if (result->cpu_stopped) {
    (void)printf("0x%08x/0x%08x", result->cpu_stop_pc,
                 result->cpu_stop_opcode);
  } else {
    (void)fputs("none", stdout);
  }
  if (result->observation.unsupported_seen) {
    (void)printf(" unsupported-trace=0x%08x/0x%08x",
                 result->observation.unsupported_pc,
                 result->observation.unsupported_opcode);
  } else {
    (void)fputs(" unsupported-trace=none", stdout);
  }
  if (result->has_store_attempt) {
    (void)printf(" store-attempt=va:0x%08x,pa:0x%08x,value:0x%08x,owner:%s",
                 result->attempted_virtual_address,
                 result->attempted_physical_address,
                 result->attempted_store_value,
                 bus_owner_name(result->attempted_physical_address));
  } else {
    (void)fputs(" store-attempt=none", stdout);
  }
  (void)printf(" exception=%u/%u/badva:0x%llx/epc:0x%llx/bd:%u cp0-cause:0x%08x cp0-status:0x%08x epc-word=",
               result->pending_exception_valid ? 1u : 0u,
               result->pending_exception_code,
               (unsigned long long)result->pending_exception_address,
               (unsigned long long)result->pending_exception_epc,
               result->pending_exception_bd ? 1u : 0u,
               result->cp0_cause, result->cp0_status);
  if (result->epc_instruction_available) {
    (void)printf("0x%08x", result->epc_instruction);
  } else {
    (void)fputs("unavailable", stdout);
  }
  (void)printf(" sp-status=0x%08x mi-upper-mode=%u\n", result->sp_status,
               result->protocol_upper_mode ? 1u : 0u);
  (void)printf("pi-state profile=%s dram=0x%08x cart=0x%08x write-length=0x%08x active-length=%u image-bytes=%zu s0=0x%016llx\n",
               profile_id, result->pi_dram_address, result->pi_cart_address,
               result->pi_write_length, result->pi_active_length,
               result->image_length, (unsigned long long)result->cpu_s0);
  (void)puts("terminal-policy=authenticated-architectural-break-v1");
  (void)printf("boot-observation profile=%s header-entry-retired=%u tick=%llu opcode=0x%08x exception-vector-predecessor=",
               profile_id,
               result->observation.header_entry_retired ? 1u : 0u,
               (unsigned long long)result->observation.header_entry_tick,
               result->observation.header_entry_opcode);
  if (result->observation.exception_vector_predecessor_seen) {
    (void)printf("0x%016llx/0x%08x",
                 (unsigned long long)
                     result->observation.exception_vector_predecessor_pc,
                 result->observation.exception_vector_predecessor_opcode);
  } else {
    (void)fputs("none", stdout);
  }
  (void)fputs(" boot-info-write=", stdout);
  if (result->observation.boot_info_write_seen) {
    (void)printf("address:0x%08x,width:%u,value:0x%08x,tick:%llu,pc:0x%016llx,opcode:0x%08x",
                 result->observation.boot_info_write_address,
                 result->observation.boot_info_write_width,
                 result->observation.boot_info_write_value,
                 (unsigned long long)result->observation.boot_info_write_tick,
                 (unsigned long long)
                     result->observation.boot_info_write_pc,
                 result->observation.boot_info_write_opcode);
  } else {
    (void)fputs("none", stdout);
  }
  (void)putchar('\n');
  (void)printf("retirement-tail profile=%s count=%zu",
               profile_id, result->observation.recent_retirement_count);
  {
    size_t index;
    size_t start = (result->observation.recent_retirement_next +
                    COLD_RECENT_RETIREMENTS -
                    result->observation.recent_retirement_count) %
                   COLD_RECENT_RETIREMENTS;
    for (index = 0u;
         index < result->observation.recent_retirement_count; index++) {
      size_t slot = (start + index) % COLD_RECENT_RETIREMENTS;
      (void)printf(" %08x/%08x",
                   result->observation.recent_retirement_pc[slot],
                   result->observation.recent_retirement_opcode[slot]);
    }
  }
  (void)putchar('\n');
}

static int run_compare(const char *profile_a, const char *profile_b,
                       const char *fixture_path) {
  cold_rom rom = {{0}, 0u, {0}};
  cold_profile_result result_a;
  cold_profile_result result_b;
  const char *error;

  if (profile_a == NULL || profile_b == NULL || fixture_path == NULL ||
      strcmp(profile_a, COLD_PROFILE_A) != 0 ||
      strcmp(profile_b, COLD_PROFILE_B) != 0) {
    (void)puts("classification=refused reason=profile-identity");
    return 2;
  }
  error = read_cleared_hello(fixture_path, &rom);
  if (error != NULL) {
    (void)printf("classification=refused reason=%s\n", error);
    return 2;
  }
  (void)printf("comparison-identity fixture-a=hello-openipl3-rdram-a-t1 fixture-b=hello-openipl3-rdram-b-t1 rom-sha256=%s ipl3-payload-sha256=%s build-id=%s source-revision=%s compiler-policy=bound-by-build-id-v1\n",
               rom.sha256, COLD_IPL3_PAYLOAD_SHA256, t64_build_identity(),
               t64_source_revision());
  error = run_complete_profile(&rom, profile_a, &result_a);
  if (error != NULL) {
    (void)printf("classification=compare-incomplete profile=%s reason=%s\n",
                 profile_a, error);
    return 1;
  }
  print_profile_result(profile_a, &result_a);
  error = run_complete_profile(&rom, profile_b, &result_b);
  if (error != NULL) {
    (void)printf("classification=compare-incomplete profile=%s reason=%s\n",
                 profile_b, error);
    return 1;
  }
  print_profile_result(profile_b, &result_b);
  if (!result_a.observation.completion_seen ||
      !result_b.observation.completion_seen ||
      result_a.stop_reason != T64_STOP_REASON_BOUND ||
      result_b.stop_reason != T64_STOP_REASON_BOUND ||
      result_a.terminal_pc != COLD_COMPLETION_PC ||
      result_b.terminal_pc != COLD_COMPLETION_PC ||
      result_a.terminal_opcode != COLD_COMPLETION_OPCODE ||
      result_b.terminal_opcode != COLD_COMPLETION_OPCODE ||
      result_a.observation.last_retired_pc != COLD_LAST_RETIRED_PC ||
      result_b.observation.last_retired_pc != COLD_LAST_RETIRED_PC ||
      result_a.observation.last_retired_opcode != COLD_LAST_RETIRED_OPCODE ||
      result_b.observation.last_retired_opcode != COLD_LAST_RETIRED_OPCODE ||
      result_a.terminal_in_delay_slot || result_b.terminal_in_delay_slot ||
      !protocol_trace_complete(&result_b.observation) ||
      !result_b.protocol_state_valid) {
    (void)puts("classification=compare-incomplete reason=terminal-or-protocol");
    return 1;
  }
  if (!literal_output_valid(&result_a.output) ||
      !literal_output_valid(&result_b.output) ||
      result_a.output.length != result_b.output.length ||
      memcmp(result_a.output.bytes, result_b.output.bytes,
             result_a.output.length) != 0) {
    (void)puts("classification=compare-incomplete reason=literal-output");
    return 1;
  }
  (void)puts("classification=compare-pass comparison=equal terminal=0x80000478/0x0000000d last-retired=0x80000474/0xa50e0016 terminal-in-delay-slot=0 hello-hex=54656e6469657336342068656c6c6f20766961206f70656e2049504c330a");
  return 0;
}

static int run_probe(const char *profile_id, const char *fixture_path) {
  static const t64_host_allocator allocator = {
      allocate_machine, release_machine, NULL};
  cold_rom rom = {{0}, 0u, {0}};
  cold_operation operation = {0};
  cold_output output = {{0}, 0u, 0u, true, false};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  t64_stop_result stop = {T64_STOP_REASON_BOUND, 0u};
  uint64_t current_tick = 0u;
  uint64_t dropped_total = 0u;
  uint8_t content_digest[T64_DIGEST_SIZE];
  char content_hex[T64_SHA256_HEX_SIZE];
  uint8_t output_hash[T64_SHA256_DIGEST_SIZE];
  char output_hex[T64_SHA256_HEX_SIZE];
  t64_sha256_ctx output_context;
  const char *error;
  int result = 2;

  error = read_cleared_hello(fixture_path, &rom);
  if (error != NULL) {
    (void)printf("classification=refused reason=%s\n", error);
    return 2;
  }
  if (!known_profile(profile_id) ||
      t64_profile_lookup(profile_id, &profile) != T64_STATUS_OK ||
      profile == NULL || strcmp(profile->profile_id, profile_id) != 0 ||
      profile->rdram_bytes != UINT32_C(0x00400000) ||
      profile->expansion_bytes != 0u) {
    (void)puts("classification=refused reason=profile-identity");
    return 2;
  }
  if (t64_machine_create(&allocator, profile, &machine) != T64_STATUS_OK ||
      machine == NULL) {
    (void)puts("classification=refused reason=machine-create");
    goto cleanup;
  }
  if (t64_machine_trace_set_enabled(machine, true) != T64_STATUS_OK ||
      t64_machine_admit_rom(machine, rom.bytes, rom.length) != T64_STATUS_OK ||
      machine->cpu.pc != UINT32_C(0xa4000040) ||
      t64_machine_content_digest(machine, content_digest) != T64_STATUS_OK) {
    (void)puts("classification=refused reason=boot-entry");
    goto cleanup;
  }
  t64_sha256_hex(content_digest, content_hex);
  if (strcmp(content_hex, rom.sha256) != 0) {
    (void)puts("classification=harness-error reason=admitted-content-identity");
    goto cleanup;
  }
  if (!collect_trace(machine, &operation, &dropped_total) ||
      !drain_output(machine, &output)) {
    (void)puts("classification=harness-error reason=initial-observation");
    goto cleanup;
  }
  while (current_tick < COLD_TICK_LIMIT &&
         stop.reason == T64_STOP_REASON_BOUND) {
    uint64_t remaining = COLD_TICK_LIMIT - current_tick;
    uint64_t target = current_tick +
        (remaining < COLD_TRACE_STEP ? remaining : COLD_TRACE_STEP);
    t64_status status = t64_machine_advance(
        machine, &(t64_stop_request){T64_STOP_GUEST_TICK, target}, &stop);
    current_tick = stop.tick;
    if (status != T64_STATUS_OK) {
      if (machine->cpu.stopped &&
          machine->cpu.stop_reason == T64_STOP_REASON_CPU_UNSUPPORTED) {
        stop.reason = T64_STOP_REASON_CPU_UNSUPPORTED;
      } else {
        (void)printf("classification=harness-error reason=advance-status status=%u tick=%llu\n",
                     (unsigned int)status,
                     (unsigned long long)current_tick);
        goto cleanup;
      }
    }
    if (machine->cpu.stopped) {
      stop.reason = machine->cpu.stop_reason;
      current_tick = machine->tick;
    }
    if (!collect_trace(machine, &operation, &dropped_total) ||
        !drain_output(machine, &output)) {
      (void)puts("classification=harness-error reason=observation-overflow");
      goto cleanup;
    }
    if (stop.reason != T64_STOP_REASON_BOUND &&
        stop.reason != T64_STOP_REASON_CPU_UNSUPPORTED &&
        stop.reason != T64_STOP_REASON_GUEST_COMPLETION &&
        stop.reason != T64_STOP_REASON_HALT &&
        stop.reason != T64_STOP_REASON_QUIESCENT &&
        stop.reason != T64_STOP_REASON_RESOURCE_LIMIT) {
      (void)printf("classification=harness-error reason=unexpected-stop stop=%u\n",
                   stop.reason);
      goto cleanup;
    }
  }
  t64_sha256_init(&output_context);
  t64_sha256_update(&output_context, output.bytes, output.length);
  t64_sha256_final(&output_context, output_hash);
  t64_sha256_hex(output_hash, output_hex);
  if (!operation.found) {
    (void)printf("classification=incomplete reason=missing-ri-rdram-operation profile=%s stop=%s tick=%llu output_bytes=%zu output_sha256=%s\n",
                 profile_id, stop_name(stop.reason),
                 (unsigned long long)current_tick, output.length, output_hex);
    result = 1;
    goto cleanup;
  }
  (void)printf("classification=observed profile=%s operation=%s address=0x%08x width=%u value=0x%08x pc=0x%08x opcode=0x%08x tick=%llu stop=%s stop_tick=%llu output_bytes=%zu output_sha256=%s dropped=%llu\n",
               profile_id, operation.write ? "write" : "read",
               operation.address, operation.width, operation.value,
               operation.pc, operation.opcode,
               (unsigned long long)operation.tick, stop_name(stop.reason),
               (unsigned long long)current_tick, output.length, output_hex,
               (unsigned long long)dropped_total);
  result = 0;

cleanup:
  if (machine != NULL) t64_machine_destroy(machine);
  return result;
}

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "--terminal-controls") == 0) {
    return terminal_controls();
  }
  if (argc >= 2 && strcmp(argv[1], "--compare") == 0) {
    if (argc != 8 || strcmp(argv[2], "--profile-a") != 0 ||
        strcmp(argv[4], "--profile-b") != 0 ||
        strcmp(argv[6], "--fixture") != 0) {
      (void)puts("classification=refused reason=usage");
      return 2;
    }
    return run_compare(argv[3], argv[5], argv[7]);
  }
  if (argc != 6 || strcmp(argv[1], "--probe") != 0 ||
      strcmp(argv[2], "--profile") != 0 ||
      strcmp(argv[4], "--fixture") != 0) {
    (void)puts("classification=refused reason=usage");
    return 2;
  }
  return run_probe(argv[3], argv[5]);
}
