#ifndef TENDIES64_T64_INTERNAL_H
#define TENDIES64_T64_INTERNAL_H

#include "tendies64/t64_machine.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define T64_ROM_HEADER_BYTES T64_ROM_HEADER_SIZE
#define T64_ROM_TITLE_BYTES 20u
#define T64_ROM_GAME_CODE_BYTES 4u
#define T64_EVENT_QUEUE_CAPACITY 256u
#define T64_SP_BANK_BYTES 4096u
#define T64_MI_INTERRUPT_BITS UINT32_C(0x3f)
#define T64_DEVICE_MI_TEST UINT32_C(0xffff0002)
#define T64_DEVICE_PI UINT32_C(0x00000003)
#define T64_DEVICE_SP UINT32_C(0x00000004)
#define T64_PI_EVENT_COMPLETE UINT32_C(1)
#define T64_SP_EVENT_COMPLETE UINT32_C(1)

#define T64_DEVICE_SYNTHETIC_TEST UINT32_C(0xffff0001)
#define T64_TEST_EVENT_NOOP UINT32_C(1)
#define T64_TEST_EVENT_INSTRUCTION UINT32_C(2)
#define T64_TEST_EVENT_FRAME UINT32_C(3)
#define T64_TEST_EVENT_HALT UINT32_C(4)
#define T64_TEST_EVENT_REPEAT_INSTRUCTION UINT32_C(5)

typedef struct {
  uintptr_t begin;
  uintptr_t end;
} t64_address_range;

typedef struct {
  t64_host_storage storage;
  size_t length;
  uint8_t header[T64_ROM_HEADER_BYTES];
  uint8_t title[T64_ROM_TITLE_BYTES];
  uint8_t game_code[T64_ROM_GAME_CODE_BYTES];
  uint8_t version;
  uint8_t digest[T64_DIGEST_SIZE];
  bool owns_storage;
} t64_rom_candidate;

typedef struct {
  uint32_t dram_address;
  uint32_t cart_address;
  uint32_t read_length;
  uint32_t write_length;
  uint32_t domain1_latency;
  uint32_t domain1_pulse_width;
  uint32_t domain1_page_size;
  uint32_t domain1_release_duration;
  uint32_t domain2_latency;
  uint32_t domain2_pulse_width;
  uint32_t domain2_page_size;
  uint32_t domain2_release_duration;
  bool error;
  bool active_valid;
  bool active_canceled;
  uint32_t active_dram_address;
  uint32_t active_cart_address;
  uint32_t active_length;
  uint64_t active_token;
  uint64_t next_token;
  bool token_exhausted;
} t64_pi_state;

typedef struct {
  bool valid;
  bool dram_to_sp;
  bool imem;
  uint32_t raw_mem_address;
  uint32_t raw_dram_address;
  uint32_t raw_length;
  uint32_t aligned_dram_start;
  uint32_t internal_start;
  uint32_t bytes_per_line;
  uint32_t line_count;
  uint32_t skip;
  uint32_t payload_bytes;
  uint64_t start_tick;
  uint64_t end_tick;
  uint64_t token;
} t64_sp_dma_descriptor;

typedef struct {
  uint32_t mem_address;
  uint32_t dram_address;
  uint32_t read_length;
  uint32_t write_length;
  t64_sp_dma_descriptor active;
  t64_sp_dma_descriptor pending;
  uint64_t next_token;
  bool token_exhausted;
} t64_sp_state;

typedef struct t64_event t64_event;
typedef t64_status (*t64_event_handler_fn)(t64_machine *machine,
                                           const t64_event *event);

struct t64_event {
  uint64_t tick;
  uint64_t sequence;
  uint32_t device;
  uint32_t kind;
  uint64_t value;
  uint64_t token;
  t64_event_handler_fn handler;
};

typedef struct {
  t64_machine *machine;
  t64_event event;
  size_t expected_queue_count;
  uint64_t expected_tick;
  bool valid;
} t64_event_reservation;

struct t64_machine {
  t64_host_allocator allocator;
  t64_host_storage machine_storage;
  t64_host_storage image_storage;
  t64_profile_descriptor profile;
  uint8_t profile_digest[T64_DIGEST_SIZE];
  uint8_t content_digest[T64_DIGEST_SIZE];
  uint8_t *ram;
  uint8_t sp_dmem[T64_SP_BANK_BYTES];
  uint8_t sp_imem[T64_SP_BANK_BYTES];
  uint32_t mi_pending;
  uint32_t mi_mask;
  bool mi_irq_line;
  t64_pi_state pi;
  t64_sp_state sp;
  size_t image_length;
  uint64_t tick;
  uint64_t instruction_count;
  uint64_t frame_event_count;
  uint32_t sticky_error_status;
  uint8_t rom_header[T64_ROM_HEADER_BYTES];
  uint8_t rom_title[T64_ROM_TITLE_BYTES];
  uint8_t rom_game_code[T64_ROM_GAME_CODE_BYTES];
  uint8_t rom_version;
  bool has_image;
  bool halted;
  t64_event events[T64_EVENT_QUEUE_CAPACITY];
  size_t queue_head;
  size_t queue_count;
  uint64_t next_sequence;
  bool sequence_exhausted;
  t64_trace_record trace_records[T64_TRACE_RECORD_CAPACITY];
  size_t trace_head;
  size_t trace_count;
  uint64_t trace_drop_count;
  bool trace_drop_count_saturated;
  bool trace_enabled;
};

t64_status t64_schedule_reserve(t64_machine *machine, uint64_t tick,
                                uint32_t device, uint32_t kind,
                                uint64_t value, uint64_t token,
                                t64_event_handler_fn handler,
                                t64_event_reservation *reservation_out);
t64_status t64_schedule_reserve_after(t64_machine *machine, uint64_t delay,
                                      uint32_t device, uint32_t kind,
                                      uint64_t value, uint64_t token,
                                      t64_event_handler_fn handler,
                                      t64_event_reservation *reservation_out);
t64_status t64_schedule_commit(t64_machine *machine,
                               t64_event_reservation *reservation);
t64_status t64_scheduler_advance(t64_machine *machine,
                                 const t64_stop_request *request,
                                 t64_stop_result *result_out);
t64_status t64_test_schedule_synthetic(t64_machine *machine, uint64_t tick,
                                       uint32_t kind, uint64_t value);
t64_status t64_test_schedule_synthetic_after(t64_machine *machine,
                                             uint64_t delay, uint32_t kind,
                                             uint64_t value);
t64_status t64_test_reserve_synthetic(t64_machine *machine, uint64_t tick,
                                      uint32_t kind, uint64_t value,
                                      t64_event_reservation *reservation_out);
bool t64_test_event_handler_matches(const t64_machine *machine,
                                    size_t logical_offset);
void t64_machine_reset_execution_state(t64_machine *machine);
void t64_trace_reset(t64_machine *machine);
void t64_trace_emit(t64_machine *machine, uint64_t tick, uint32_t device,
                    uint32_t kind, uint64_t value);
t64_status t64_bus_read(t64_machine *machine, uint32_t address,
                        uint32_t width, uint32_t *value_out);
t64_status t64_bus_write(t64_machine *machine, uint32_t address,
                         uint32_t width, uint32_t value);
void t64_mi_set_pending(t64_machine *machine, uint32_t bits);
void t64_mi_clear_pending(t64_machine *machine, uint32_t bits);
void t64_mi_write_mask(t64_machine *machine, uint32_t command);
bool t64_mi_register_read(const t64_machine *machine, uint32_t address,
                          uint32_t *value_out);
t64_status t64_mi_register_write(t64_machine *machine, uint32_t address,
                                uint32_t value);
t64_status t64_pi_read_register(const t64_machine *machine, uint32_t address,
                                uint32_t *value_out);
t64_status t64_pi_write_register(t64_machine *machine, uint32_t address,
                                 uint32_t value);
t64_status t64_pi_complete(t64_machine *machine, const t64_event *event);
void t64_pi_reset_machine_state(t64_machine *machine);
t64_status t64_sp_read_register(const t64_machine *machine, uint32_t address,
                                uint32_t *value_out);
t64_status t64_sp_write_register(t64_machine *machine, uint32_t address,
                                 uint32_t value);
t64_status t64_sp_complete(t64_machine *machine, const t64_event *event);
void t64_sp_reset_machine_state(t64_machine *machine);

static inline bool t64_internal_make_range(const void *pointer, size_t length,
                                           t64_address_range *range_out) {
  uintptr_t begin;

  if (pointer == NULL || range_out == NULL ||
      (uintmax_t)length > (uintmax_t)UINTPTR_MAX -
                              (uintmax_t)(uintptr_t)pointer) {
    return false;
  }
  begin = (uintptr_t)pointer;
  range_out->begin = begin;
  range_out->end = begin + (uintptr_t)length;
  return true;
}

static inline bool t64_internal_ranges_overlap(t64_address_range left,
                                               t64_address_range right) {
  return left.begin < right.end && right.begin < left.end;
}

static inline bool t64_internal_storage_range(t64_host_storage storage,
                                              size_t required,
                                              size_t alignment,
                                              t64_address_range *range_out) {
  return storage.data != NULL && storage.capacity >= required &&
         alignment > 0u && storage.alignment >= alignment &&
         ((uintptr_t)storage.data % alignment) == 0u &&
         t64_internal_make_range(storage.data, storage.capacity, range_out);
}

t64_status t64_rom_candidate_prepare(
    const t64_host_allocator *allocator,
    const t64_profile_descriptor *profile, const uint8_t *bytes,
    size_t length, t64_host_storage machine_storage,
    t64_host_storage current_image_storage, bool has_current_image,
    t64_rom_candidate *candidate_out);
void t64_rom_candidate_discard(const t64_host_allocator *allocator,
                               t64_rom_candidate *candidate);

#endif /* TENDIES64_T64_INTERNAL_H */
