#ifndef TENDIES64_T64_INTERNAL_H
#define TENDIES64_T64_INTERNAL_H

#include "tendies64/t64_machine.h"
#include "softfloat/softfloat_types.h"

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
#define T64_DEVICE_CPU UINT32_C(0x00000005)
#define T64_DEVICE_PI UINT32_C(0x00000003)
#define T64_DEVICE_SP UINT32_C(0x00000004)
#define T64_PI_EVENT_COMPLETE UINT32_C(1)
#define T64_SP_EVENT_COMPLETE UINT32_C(1)
#define T64_WARM_DEVICE_POLICY_VERSION UINT32_C(3)
#define T64_WARM_ABSENT_RDRAM_PROBE_ADDRESS UINT32_C(0x00400000)
#define T64_WARM_ABSENT_RDRAM_PROBE_BYTES UINT32_C(4)
#define T64_ISVIEWER_BASE UINT32_C(0x13ff0000)
#define T64_ISVIEWER_MAGIC_OFFSET UINT32_C(0x0000)
#define T64_ISVIEWER_COUNT_OFFSET UINT32_C(0x0014)
#define T64_ISVIEWER_BUFFER_OFFSET UINT32_C(0x0020)
#define T64_ISVIEWER_BUFFER_BYTES 512u
#define T64_ISVIEWER_CAPTURE_BYTES 4096u
#define T64_ISVIEWER_DEVICE_BYTES UINT32_C(0x0220)
#define T64_ISVIEWER_POLICY_VERSION UINT32_C(3)
#define T64_VR4300_TLB_ENTRY_COUNT 32u
#define T64_VR4300_ICACHE_LINE_COUNT 512u
#define T64_VR4300_DCACHE_LINE_COUNT 512u
#define T64_VR4300_ICACHE_LINE_BYTES 32u
#define T64_VR4300_DCACHE_LINE_BYTES 16u

#define T64_DEVICE_SYNTHETIC_TEST UINT32_C(0xffff0001)
#define T64_TEST_EVENT_NOOP UINT32_C(1)
#define T64_TEST_EVENT_INSTRUCTION UINT32_C(2)
#define T64_TEST_EVENT_FRAME UINT32_C(3)
#define T64_TEST_EVENT_HALT UINT32_C(4)
#define T64_TEST_EVENT_REPEAT_INSTRUCTION UINT32_C(5)

#define T64_CPU_EVENT_STEP UINT32_C(1)
#define T64_CPU_SOFTWARE_DEBUG_STOP_BREAK UINT32_C(0x0000000d)
#define T64_TRACE_KIND_BUS_READ16 UINT32_C(5)
#define T64_TRACE_KIND_BUS_WRITE16 UINT32_C(6)

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

typedef enum {
  T64_CPU_ATTEMPT_NONE = 0,
  T64_CPU_ATTEMPT_RETIRED = 1,
  T64_CPU_ATTEMPT_GUEST_EXCEPTION = 2,
  T64_CPU_ATTEMPT_MODEL_UNSUPPORTED = 3,
  T64_CPU_ATTEMPT_SOFTWARE_STOP = 4,
  T64_CPU_ATTEMPT_HOST_ERROR = 5
} t64_cpu_attempt_outcome;

typedef struct {
  uint64_t page_mask;
  uint64_t vpn2;
  uint64_t region;
  uint32_t asid;
  uint64_t entry_lo0;
  uint64_t entry_lo1;
} t64_vr4300_tlb_pair;

typedef struct {
  uint32_t physical_tag;
  bool valid;
  uint8_t data[T64_VR4300_ICACHE_LINE_BYTES];
} t64_vr4300_icache_line;

typedef struct {
  uint32_t physical_tag;
  bool valid;
  bool dirty;
  uint8_t data[T64_VR4300_DCACHE_LINE_BYTES];
} t64_vr4300_dcache_line;

/* Raw decode input and narrow, side-effect-free contracts for later CPU slices. */
typedef struct {
  uint64_t pc;
  uint32_t instruction_word;
  uint32_t opcode;
  uint32_t rs;
  uint32_t rt;
  uint32_t rd;
  uint32_t shift;
  uint32_t immediate;
  bool in_delay_slot;
} t64_cpu_raw_context;

typedef struct {
  uint64_t bits;
  uint8_t exception_flags;
} t64_softfloat_result;

typedef enum {
  T64_SOFTFLOAT_OP_I32_TO_F64 = 1,
  T64_SOFTFLOAT_OP_F32_ADD = 2,
  T64_SOFTFLOAT_OP_F32_SUB = 3,
  T64_SOFTFLOAT_OP_F32_MUL = 4,
  T64_SOFTFLOAT_OP_F32_DIV = 5,
  T64_SOFTFLOAT_OP_F32_SQRT = 6,
  T64_SOFTFLOAT_OP_F32_EQ = 7,
  T64_SOFTFLOAT_OP_F32_LE = 8,
  T64_SOFTFLOAT_OP_F32_LT = 9,
  T64_SOFTFLOAT_OP_F32_TO_F64 = 10,
  T64_SOFTFLOAT_OP_F32_TO_I32 = 11,
  T64_SOFTFLOAT_OP_F32_TO_I64 = 12,
  T64_SOFTFLOAT_OP_F64_ADD = 13,
  T64_SOFTFLOAT_OP_F64_SUB = 14,
  T64_SOFTFLOAT_OP_F64_MUL = 15,
  T64_SOFTFLOAT_OP_F64_DIV = 16,
  T64_SOFTFLOAT_OP_F64_SQRT = 17,
  T64_SOFTFLOAT_OP_F64_EQ = 18,
  T64_SOFTFLOAT_OP_F64_LE = 19,
  T64_SOFTFLOAT_OP_F64_LT = 20,
  T64_SOFTFLOAT_OP_F64_TO_F32 = 21,
  T64_SOFTFLOAT_OP_F64_TO_I32 = 22,
  T64_SOFTFLOAT_OP_F64_TO_I64 = 23,
  T64_SOFTFLOAT_OP_I32_TO_F32 = 24,
  T64_SOFTFLOAT_OP_I64_TO_F32 = 25,
  T64_SOFTFLOAT_OP_I64_TO_F64 = 26
} t64_softfloat_operation;

typedef struct {
  bool valid;
  uint32_t code;
  uint64_t bad_virtual_address;
  uint64_t epc;
  bool branch_delay;
  bool sets_coprocessor;
  uint32_t coprocessor;
  bool sets_tlb_context;
  bool tlb_refill;
  bool tlb_extended;
} t64_cpu_exception_result;

typedef enum {
  T64_CP0_TRANSFER_OK = 0,
  T64_CP0_TRANSFER_RESERVED = 1,
  T64_CP0_TRANSFER_UNSUPPORTED = 2
} t64_cp0_transfer_result;

typedef struct {
  bool write_gpr;
  uint32_t gpr_index;
  uint64_t gpr_value;
  bool write_cp0;
  uint32_t cp0_index;
  uint64_t cp0_value;
  bool clear_timer_pending;
  bool reset_random;
  uint32_t tlb_operation;
  uint32_t tlb_index;
  bool tlb_duplicate;
} t64_cp0_effect;

typedef enum {
  T64_CP0_TLB_NONE = 0,
  T64_CP0_TLB_PROBE = 1,
  T64_CP0_TLB_READ = 2,
  T64_CP0_TLB_WRITE_INDEXED = 3,
  T64_CP0_TLB_WRITE_RANDOM = 4
} t64_cp0_tlb_operation;

typedef enum {
  T64_MMU_ACCESS_FETCH = 0,
  T64_MMU_ACCESS_READ = 1,
  T64_MMU_ACCESS_WRITE = 2
} t64_mmu_access_kind;

typedef enum {
  T64_MMU_DOMAIN_BUS = 0,
  T64_MMU_DOMAIN_XKPHYS = 1
} t64_mmu_physical_domain;

typedef enum {
  T64_MMU_FAULT_NONE = 0,
  T64_MMU_FAULT_ADDRESS = 1,
  T64_MMU_FAULT_TLB_REFILL = 2,
  T64_MMU_FAULT_TLB_INVALID = 3,
  T64_MMU_FAULT_TLB_MODIFIED = 4,
  T64_MMU_FAULT_DUPLICATE_MATCH = 5
} t64_mmu_fault;

typedef struct {
  uint64_t virtual_address;
  uint32_t width;
  t64_mmu_access_kind kind;
} t64_mmu_request;

typedef struct {
  t64_mmu_fault fault;
  uint64_t virtual_address;
  uint64_t physical_address;
  uint32_t matched_entry;
  uint32_t entry_lo;
  uint32_t width;
  uint8_t cache_algorithm;
  t64_mmu_physical_domain physical_domain;
  bool translated;
  bool cacheable;
  bool extended_address_mode;
  bool tlb_context_valid;
  bool tlb_refill_vector;
} t64_mmu_result;

typedef struct {
  bool supported;
  bool hit;
  bool dirty;
  uint32_t line_index;
  uint32_t physical_tag;
} t64_cpu_cache_result;

typedef struct {
  bool supported;
  bool trapped;
  bool write_gpr;
  uint32_t destination_gpr;
  uint64_t gpr_value;
  bool write_fpr;
  uint32_t destination_fpr;
  uint32_t fpr_width;
  uint64_t result_bits;
  uint32_t fcr31_flags;
} t64_cpu_cop1_result;

typedef struct {
  uint64_t gpr[32];
  uint64_t hi;
  uint64_t lo;
  uint64_t cp0[32];
  uint64_t retired_instructions;
  uint32_t pc;
  uint32_t last_pc;
  uint32_t last_opcode;
  uint32_t delay_slot_pc;
  uint32_t delay_target;
  bool delay_slot_pending;
  bool stopped;
  uint32_t stop_reason;
  uint32_t stop_pc;
  uint32_t stop_opcode;
  uint64_t pc_full;
  uint64_t last_pc_full;
  uint64_t delay_slot_pc_full;
  uint64_t delay_target_full;
  uint64_t stop_pc_full;
  uint32_t attempt_outcome;
  uint32_t semantic_policy_version;
  /* Prepared CPU entry begins after Config EP/BE initialization is closed. */
  bool cp0_config_init_window;
  bool tlb_shutdown;
  bool pending_exception_valid;
  uint32_t pending_exception_code;
  uint64_t pending_exception_address;
  uint64_t pending_exception_epc;
  bool pending_exception_bd;
  t64_vr4300_tlb_pair tlb[T64_VR4300_TLB_ENTRY_COUNT];
  uint64_t count_sync_tick;
  uint32_t count_half_clock_phase;
  uint64_t count_compare_deadline;
  uint64_t count_equality_latch_generation;
  bool count_equality_latched;
  uint32_t linked_physical_address;
  uint32_t linked_width;
  bool linked_valid;
  t64_vr4300_icache_line icache[T64_VR4300_ICACHE_LINE_COUNT];
  t64_vr4300_dcache_line dcache[T64_VR4300_DCACHE_LINE_COUNT];
  uint64_t fpr[32];
  uint32_t fcr0;
  uint32_t fcr31;
} t64_cpu_state;

static inline uint64_t t64_cpu_canonical_address(uint32_t address) {
  return (address & UINT32_C(0x80000000)) != 0u
             ? (UINT64_C(0xffffffff00000000) | (uint64_t)address)
             : (uint64_t)address;
}

typedef struct {
  uint8_t asset_sha256[T64_DIGEST_SIZE];
  uint8_t payload_sha256[T64_DIGEST_SIZE];
  uint8_t policy_sha256[T64_DIGEST_SIZE];
  uint32_t source_offset;
  uint32_t payload_length;
  uint32_t dmem_offset;
  uint32_t entry_pc;
  uint32_t seed_reset_version;
  uint32_t seed_s3_rom_type;
  uint32_t seed_s4_tv_type;
  uint32_t seed_s5_reset_type;
  uint32_t seed_s6_entropy;
  uint32_t seed_s7_boot_info_version;
  bool prepared;
} t64_boot_state;

typedef struct {
  uint32_t policy_version;
  uint32_t last_operation;
  uint32_t last_pc;
  uint32_t last_address;
  bool operation_seen;
} t64_cache_state;

typedef struct {
  uint32_t policy_version;
  uint32_t mi_version;
  uint32_t ri_select;
  bool mi_version_known;
  bool ri_select_known;
} t64_guest_device_state;

typedef struct {
  uint32_t policy_version;
  bool video_output_ready;
  bool audio_output_ready;
  uint32_t magic_register;
  uint32_t byte_count;
  uint8_t buffer[T64_ISVIEWER_BUFFER_BYTES];
  uint32_t captured_bytes;
  uint8_t captured[T64_ISVIEWER_CAPTURE_BYTES];
  bool lost_output;
  uint64_t total_attempted_bytes;
  bool total_attempted_bytes_saturated;
} t64_output_state;

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
  t64_cpu_state cpu;
  t64_boot_state boot;
  t64_cache_state cache;
  t64_guest_device_state guest_devices;
  t64_output_state output;
  /* Host observer cursor; never contributes to guest identity. */
  size_t output_drain_cursor;
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
t64_status t64_input_event_handler(t64_machine *machine,
                                   const t64_event *event);
t64_status t64_scheduler_advance(t64_machine *machine,
                                 const t64_stop_request *request,
                                 t64_stop_result *result_out);
t64_status t64_cpu_schedule(t64_machine *machine);
t64_status t64_cpu_step(t64_machine *machine, const t64_event *event);
t64_status t64_cop1_execute(t64_machine *machine, uint32_t opcode,
                            bool in_delay_slot,
                            t64_cpu_cop1_result *result_out);
void t64_cop1_commit_fpr(t64_machine *machine, uint32_t reg,
                         uint32_t width, uint64_t bits);
t64_status t64_memory_execute_cop1_status(
    t64_machine *machine, uint32_t opcode, uint64_t store_value,
    bool *supported_out, uint64_t *value_out);
t64_status t64_softfloat_i32_to_f64(int32_t input,
                                    t64_softfloat_context *context,
                                    t64_softfloat_result *result_out);
t64_status t64_softfloat_execute(t64_softfloat_operation operation,
                                 t64_softfloat_context *context,
                                 uint64_t a_bits, uint64_t b_bits,
                                 t64_softfloat_result *result_out);
t64_status t64_cpu_exception_enter(
    t64_machine *machine, const t64_cpu_exception_result *exception);
t64_cp0_transfer_result t64_cp0_prepare_transfer(
    const t64_machine *machine, uint32_t opcode, t64_cp0_effect *effect_out);
t64_cp0_transfer_result t64_cp0_prepare_tlb_instruction(
    const t64_machine *machine, uint32_t opcode,
    t64_cp0_effect *effect_out);
void t64_cp0_commit_transfer(t64_machine *machine,
                             const t64_cp0_effect *effect);
t64_status t64_cp0_synchronize_guest_time(t64_machine *machine,
                                          uint64_t tick);
bool t64_cp0_interrupt_pending(t64_machine *machine);
bool t64_cp0_access_unusable(const t64_machine *machine);
uint32_t t64_cp0_random_tlb_index(const t64_machine *machine);
void t64_cp0_instruction_retired(t64_machine *machine);
bool t64_mmu_translate(const t64_machine *machine,
                       const t64_mmu_request *request,
                       t64_mmu_result *result_out);
bool t64_mmu_probe(const t64_machine *machine, uint64_t entry_hi,
                   uint32_t *entry_index_out, bool *duplicate_out);
bool t64_mmu_page_mask_is_defined(uint64_t page_mask);
bool t64_cpu_translate(uint64_t virtual_address,
                       uint32_t *physical_address_out);
t64_status t64_bus_probe_access(const t64_machine *machine,
                                uint32_t physical_address, uint32_t width);
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
t64_status t64_cpu_icache_fetch(t64_machine *machine,
                                uint64_t virtual_address,
                                uint32_t physical_address,
                                uint32_t *opcode_out);
void t64_cpu_icache_hit_invalidate(t64_machine *machine,
                                   uint64_t virtual_address,
                                   uint32_t physical_address);
t64_status t64_cpu_dcache_read(t64_machine *machine,
                               uint64_t virtual_address,
                               uint32_t physical_address, uint32_t width,
                               uint64_t *value_out);
t64_status t64_cpu_dcache_write(t64_machine *machine,
                                uint64_t virtual_address,
                                uint32_t physical_address, uint32_t width,
                                uint8_t byte_mask, uint64_t value);
t64_status t64_cpu_dcache_hit_writeback(t64_machine *machine,
                                        uint64_t virtual_address,
                                        uint32_t physical_address);
t64_status t64_cpu_icache_fill(t64_machine *machine,
                               uint64_t virtual_address,
                               uint32_t physical_address);
t64_status t64_cpu_cache_operation(t64_machine *machine,
                                   uint32_t operation,
                                   uint64_t virtual_address,
                                   uint32_t physical_address);
t64_status t64_bus_validate_cache_writeback(t64_machine *machine,
                                            uint32_t physical_address,
                                            uint32_t line_bytes);
t64_status t64_bus_writeback_cache_line(
    t64_machine *machine, uint32_t physical_address, const uint8_t *data,
    uint32_t line_bytes);
t64_status t64_bus_validate_dcache_line_writeback(
    t64_machine *machine, uint32_t physical_address);
t64_status t64_bus_writeback_dcache_line(
    t64_machine *machine, uint32_t physical_address,
    const uint8_t data[T64_VR4300_DCACHE_LINE_BYTES]);
t64_status t64_bus_read_wide(t64_machine *machine, uint32_t address,
                             uint64_t *value_out);
t64_status t64_bus_write(t64_machine *machine, uint32_t address,
                         uint32_t width, uint32_t value);
t64_status t64_bus_write_masked(t64_machine *machine, uint32_t address,
                                 uint32_t width, uint8_t byte_mask,
                                 uint64_t value);
void t64_cpu_link_invalidate_range(t64_machine *machine,
                                   uint32_t physical_address,
                                   uint32_t width);
void t64_cpu_link_invalidate_dcache_line(t64_machine *machine,
                                         uint32_t physical_address);
bool t64_memory_execute(t64_machine *machine, uint32_t opcode,
                        bool *write_gpr_out, uint32_t *destination_out,
                        uint64_t *value_out);
t64_status t64_memory_execute_status(
    t64_machine *machine, uint32_t opcode, bool *supported_out,
    bool *write_gpr_out, uint32_t *destination_out, uint64_t *value_out);
t64_status t64_warm_device_read(const t64_machine *machine, uint32_t address,
                                uint32_t width, uint32_t *value_out);
t64_status t64_warm_device_write(t64_machine *machine, uint32_t address,
                                 uint32_t width, uint32_t value);
bool t64_isviewer_active(const t64_machine *machine);
t64_status t64_isviewer_read(const t64_machine *machine, uint32_t address,
                             uint32_t width, uint32_t *value_out);
t64_status t64_isviewer_write(t64_machine *machine, uint32_t address,
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

static inline bool t64_internal_observation_range_is_disjoint(
    const t64_machine *machine, t64_address_range output_range) {
  t64_address_range owned_range;

  if (!t64_internal_make_range(machine->machine_storage.data,
                               machine->machine_storage.capacity,
                               &owned_range) ||
      t64_internal_ranges_overlap(output_range, owned_range)) {
    return false;
  }
  if (machine->has_image &&
      (!t64_internal_make_range(machine->image_storage.data,
                                machine->image_storage.capacity,
                                &owned_range) ||
       t64_internal_ranges_overlap(output_range, owned_range))) {
    return false;
  }
  return true;
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

t64_status t64_rom_input_preflight(
    const t64_profile_descriptor *profile, const uint8_t *bytes,
    size_t length, t64_host_storage machine_storage,
    t64_host_storage current_image_storage, bool has_current_image,
    t64_address_range *input_range_out);
t64_status t64_rom_candidate_prepare(
    const t64_host_allocator *allocator,
    const t64_profile_descriptor *profile, const uint8_t *bytes,
    size_t length, t64_host_storage machine_storage,
    t64_host_storage current_image_storage, bool has_current_image,
    t64_rom_candidate *candidate_out);
void t64_rom_candidate_discard(const t64_host_allocator *allocator,
                               t64_rom_candidate *candidate);

#endif /* TENDIES64_T64_INTERNAL_H */
