#ifndef TENDIES64_T64_INTERNAL_H
#define TENDIES64_T64_INTERNAL_H

/* Internal symbols are renamed to t64i_<name> (DR-17); every core unit,
 * white-box test and the determinism lint reach the generated rename header
 * through this include or core/softfloat/platform.h. */
#include "t64_prefix.h"
#include "tendies64/t64_machine.h"
#include "softfloat/softfloat_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * v1 sizes of the public structs (plan 11-12, DR-06): a caller struct_size, or
 * the trace record_size, below these is refused with
 * T64_STATUS_STRUCT_TOO_SMALL. They are the sizes of the 0.1 candidate layouts
 * and stay fixed when a struct grows later; the asserts pin them on LP64.
 */
#define T64_HOST_STORAGE_V1_SIZE ((uint32_t)sizeof(t64_host_storage))
#define T64_HOST_ALLOCATOR_V1_SIZE ((uint32_t)sizeof(t64_host_allocator))
#define T64_PROFILE_DESCRIPTOR_V1_SIZE                                        \
  ((uint32_t)sizeof(t64_profile_descriptor))
#define T64_MACHINE_CREATE_INFO_V1_SIZE                                       \
  ((uint32_t)sizeof(t64_machine_create_info))
#define T64_STOP_REQUEST_V1_SIZE ((uint32_t)sizeof(t64_stop_request))
#define T64_STOP_RESULT_V1_SIZE ((uint32_t)sizeof(t64_stop_result))
#define T64_CPU_STOP_DETAIL_V1_SIZE ((uint32_t)sizeof(t64_cpu_stop_detail))
#define T64_TRACE_RECORD_V1_SIZE ((uint32_t)sizeof(t64_trace_record))
#define T64_OUTPUT_DRAIN_RESULT_V1_SIZE                                       \
  ((uint32_t)sizeof(t64_output_drain_result))
#define T64_VIDEO_PACKET_INFO_V1_SIZE ((uint32_t)sizeof(t64_video_packet_info))
#define T64_AUDIO_PACKET_INFO_V1_SIZE ((uint32_t)sizeof(t64_audio_packet_info))
#define T64_STATE_IMPORT_REPORT_V1_SIZE                                       \
  ((uint32_t)sizeof(t64_state_import_report))
#define T64_SAVE_INFO_V1_SIZE ((uint32_t)sizeof(t64_save_info))
#if UINTPTR_MAX == UINT64_MAX
_Static_assert(sizeof(t64_host_storage) == 32u, "t64_host_storage v1");
_Static_assert(sizeof(t64_host_allocator) == 32u, "t64_host_allocator v1");
_Static_assert(sizeof(t64_machine_create_info) == 24u,
               "t64_machine_create_info v1");
#endif
_Static_assert(sizeof(t64_profile_descriptor) == 2304u,
               "t64_profile_descriptor v1");
_Static_assert(sizeof(t64_stop_request) == 16u, "t64_stop_request v1");
_Static_assert(sizeof(t64_stop_result) == 16u, "t64_stop_result v1");
_Static_assert(sizeof(t64_cpu_stop_detail) == 24u, "t64_cpu_stop_detail v1");
_Static_assert(sizeof(t64_trace_record) == 24u, "t64_trace_record v1");
_Static_assert(sizeof(t64_output_drain_result) == 72u,
               "t64_output_drain_result v1");
_Static_assert(sizeof(t64_video_packet_info) == 488u,
               "t64_video_packet_info v1");
_Static_assert(sizeof(t64_audio_packet_info) == 280u,
               "t64_audio_packet_info v1");
_Static_assert(sizeof(t64_state_import_report) == 32u,
               "t64_state_import_report v1");
_Static_assert(sizeof(t64_save_info) == 24u, "t64_save_info v1");
_Static_assert(offsetof(t64_host_storage, struct_size) == 0u &&
                   offsetof(t64_host_allocator, struct_size) == 0u &&
                   offsetof(t64_profile_descriptor, struct_size) == 0u &&
                   offsetof(t64_machine_create_info, struct_size) == 0u,
               "leading struct_size");
/*
 * Copy-out of a record into a caller struct of the caller's own size (plan
 * 11-13, DR-06). The callee computes the record in a local of the known layout,
 * then copies the bytes after the leading struct_size member, at most
 * min(caller_size, known_size) bytes of the record, so the caller's struct_size
 * and every byte past the known size stay exactly as the caller left them. The
 * entry has already refused a caller_size below the v1 size, which is the known
 * size, so the copy always covers the whole record after struct_size.
 */
static inline void t64_internal_copy_out(void *caller_struct,
                                         uint32_t caller_size,
                                         const void *record,
                                         size_t known_size) {
  const size_t copy_size =
      (size_t)caller_size < known_size ? (size_t)caller_size : known_size;

  if (copy_size > sizeof(uint32_t)) {
    (void)memcpy((uint8_t *)caller_struct + sizeof(uint32_t),
                 (const uint8_t *)record + sizeof(uint32_t),
                 copy_size - sizeof(uint32_t));
  }
}

/* An empty host storage record: the core is the caller of t64_host_storage
 * and fills struct_size before every allocate callback (DR-06, INFO-03). */
#define T64_HOST_STORAGE_EMPTY {T64_HOST_STORAGE_V1_SIZE, NULL, 0u, 0u}

#define T64_ROM_HEADER_BYTES T64_ROM_HEADER_SIZE
#define T64_ROM_TITLE_BYTES 20u
#define T64_ROM_GAME_CODE_BYTES 4u
#define T64_EVENT_QUEUE_CAPACITY 256u
#define T64_CPU_BASE_ISSUE_INTERVAL_TICKS UINT64_C(1)
#define T64_SP_BANK_BYTES 4096u
#define T64_MI_INTERRUPT_BITS UINT32_C(0x3f)
#define T64_DEVICE_MI_TEST UINT32_C(0xffff0002)
#define T64_DEVICE_CPU UINT32_C(0x00000005)
#define T64_DEVICE_PI UINT32_C(0x00000003)
#define T64_DEVICE_SP UINT32_C(0x00000004)
#define T64_PI_EVENT_COMPLETE UINT32_C(1)
#define T64_SP_EVENT_COMPLETE UINT32_C(1)
#define T64_SP_EVENT_RSP_ISSUE UINT32_C(2)
#define T64_RSP_STATUS_HALT UINT32_C(0x00000001)
#define T64_RSP_STATUS_BROKE UINT32_C(0x00000002)
#define T64_RSP_STATUS_DMA_BUSY UINT32_C(0x00000004)
#define T64_RSP_STATUS_DMA_FULL UINT32_C(0x00000008)
#define T64_RSP_STATUS_SINGLE_STEP UINT32_C(0x00000020)
#define T64_RSP_STATUS_INTR_BREAK UINT32_C(0x00000040)
#define T64_RSP_STATUS_SIGNAL_MASK UINT32_C(0x00007f80)
#define T64_RSP_STATUS_READ_MASK UINT32_C(0x00007fe3)
#define T64_SP_REGISTER_BASE UINT32_C(0x04040000)
#define T64_SP_REGISTER_END UINT32_C(0x04040020)
#define T64_SP_PC_ADDRESS UINT32_C(0x04080000)
#define T64_RSP_REFUSAL_NONE UINT32_C(0)
#define T64_RSP_REFUSAL_RUNNING_PC_WRITE UINT32_C(1)
#define T64_RSP_REFUSAL_ACTIVE_IMEM_WRITE UINT32_C(2)
#define T64_RSP_REFUSAL_ASYNC_HALT UINT32_C(3)
#define T64_RSP_REFUSAL_SINGLE_STEP_IN_FLIGHT UINT32_C(4)
#define T64_RSP_REFUSAL_BANK_CONFLICT UINT32_C(5)
#define T64_RSP_REFUSAL_UNSUPPORTED_OPCODE UINT32_C(6)
#define T64_RSP_REFUSAL_DMA_QUEUE_FULL UINT32_C(7)
#define T64_RSP_REFUSAL_DMA_SPAN UINT32_C(8)
#define T64_RSP_REFUSAL_DP_UNAVAILABLE UINT32_C(9)
#define T64_RSP_REFUSAL_CP0_READ_ONLY UINT32_C(10)
#define T64_RSP_REFUSAL_CP0_INVALID_VALUE UINT32_C(11)
#define T64_RSP_REFUSAL_DMA_TOKEN_EXHAUSTED UINT32_C(12)
#define T64_RSP_DMA_ACTION_SETUP UINT32_C(1)
#define T64_RSP_DMA_ACTION_BEAT UINT32_C(2)
#define T64_RSP_DMA_ACTION_COMPLETE UINT32_C(4)
#define T64_RSP_DMA_ACTION_PROMOTE UINT32_C(8)
#define T64_RSP_STALL_SCALAR_READY UINT32_C(1)
#define T64_RSP_STALL_VECTOR_READY UINT32_C(2)
#define T64_RSP_STALL_LOAD_STORE UINT32_C(3)
#define T64_RSP_STALL_BRANCH_BUBBLE UINT32_C(4)
#define T64_RSP_STALL_BANK_CONFLICT UINT32_C(5)
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
#define T64_VIDEO_QUEUE_CAPACITY 2u
#define T64_AUDIO_QUEUE_CAPACITY 2u
#define T64_AUDIO_MAX_PACKET_BYTES ((size_t)0x3fff8u)
#define T64_AUDIO_QUEUE_STORAGE_BYTES \
  ((size_t)T64_AUDIO_QUEUE_CAPACITY * T64_AUDIO_MAX_PACKET_BYTES)
#define T64_VIDEO_MAX_WIDTH 320u
#define T64_VIDEO_MAX_HEIGHT 240u
#define T64_VIDEO_MAX_PACKET_BYTES \
  ((size_t)T64_VIDEO_MAX_WIDTH * (size_t)T64_VIDEO_MAX_HEIGHT * 4u)
#define T64_VIDEO_QUEUE_STORAGE_BYTES \
  ((size_t)T64_VIDEO_QUEUE_CAPACITY * T64_VIDEO_MAX_PACKET_BYTES)
#define T64_RDP_DEVICE UINT32_C(7)
#define T64_VI_DEVICE UINT32_C(8)
#define T64_AI_DEVICE UINT32_C(9)
#define T64_SI_DEVICE UINT32_C(10)
#define T64_SI_EVENT_EXECUTE UINT32_C(1)
#define T64_SI_EVENT_COMPLETE UINT32_C(2)
#define T64_MI_INTERRUPT_AI UINT32_C(0x04)
#define T64_AI_EVENT_SAMPLE UINT32_C(1)
#define T64_AI_REGISTER_BASE UINT32_C(0x04500000)
#define T64_AI_REGISTER_END UINT32_C(0x04500018)
#define T64_RDP_EVENT_WORK UINT32_C(1)
#define T64_VI_EVENT_FIELD UINT32_C(1)
#define T64_RDP_STATUS_BUSY UINT32_C(0x00000001)
#define T64_RDP_STATUS_ERROR UINT32_C(0x00000002)
#define T64_DPC_STATUS_XBUS UINT32_C(0x00000001)
#define T64_DPC_STATUS_FREEZE UINT32_C(0x00000002)
#define T64_DPC_STATUS_FLUSH UINT32_C(0x00000004)
#define T64_DPC_STATUS_START_GCLK UINT32_C(0x00000008)
#define T64_DPC_STATUS_TMEM_BUSY UINT32_C(0x00000010)
#define T64_DPC_STATUS_PIPE_BUSY UINT32_C(0x00000020)
#define T64_DPC_STATUS_CMD_BUSY UINT32_C(0x00000040)
#define T64_DPC_STATUS_CBUF_READY UINT32_C(0x00000080)
#define T64_DPC_STATUS_DMA_BUSY UINT32_C(0x00000100)
#define T64_DPC_STATUS_END_VALID UINT32_C(0x00000200)
#define T64_DPC_STATUS_START_VALID UINT32_C(0x00000400)
#define T64_RDP_REFUSAL_NONE UINT32_C(0)
#define T64_RDP_REFUSAL_SPAN UINT32_C(1)
#define T64_RDP_REFUSAL_TRUNCATED_WORD UINT32_C(2)
#define T64_RDP_REFUSAL_UNSUPPORTED_OPCODE UINT32_C(3)
#define T64_RDP_REFUSAL_INVALID_IMAGE UINT32_C(4)
#define T64_RDP_REFUSAL_RENDER_RANGE UINT32_C(5)
#define T64_RDP_REFUSAL_INVALID_TEXTURE UINT32_C(6)
#define T64_RDP_REFUSAL_TEXTURE_RANGE UINT32_C(7)
#define T64_RDP_TMEM_BYTES 4096u
#define T64_RDP_TRIANGLE_MAX_WORDS 22u
#define T64_RDP_TRIANGLE_COVERAGE_WIDTH 1024u
#define T64_RDP_TRIANGLE_COVERAGE_HEIGHT 1024u
#define T64_RDP_TRIANGLE_COVERAGE_PIXELS \
  (T64_RDP_TRIANGLE_COVERAGE_WIDTH * T64_RDP_TRIANGLE_COVERAGE_HEIGHT)
#define T64_RDP_TRIANGLE_COVERAGE_BYTES \
  (T64_RDP_TRIANGLE_COVERAGE_PIXELS * sizeof(uint16_t))
#define T64_RDP_MEMORY_COVERAGE_GRANULE_BYTES 2u
#define T64_RDP_MEMORY_COVERAGE_BYTES(rdram_bytes) \
  (((size_t)(rdram_bytes) + T64_RDP_MEMORY_COVERAGE_GRANULE_BYTES - 1u) / \
   T64_RDP_MEMORY_COVERAGE_GRANULE_BYTES)
#define T64_RDP_DEPTH_HIDDEN_BYTES(rdram_bytes) \
  T64_RDP_MEMORY_COVERAGE_BYTES(rdram_bytes)
#define T64_RDP_TILE_COUNT 8u
#define T64_VI_REFUSAL_NONE UINT32_C(0)
#define T64_VI_REFUSAL_FORMAT UINT32_C(1)
#define T64_VI_REFUSAL_GEOMETRY UINT32_C(2)
#define T64_VI_REFUSAL_SOURCE_RANGE UINT32_C(3)
#define T64_VI_REFUSAL_PHASE_OVERFLOW UINT32_C(4)
#define T64_VI_REFUSAL_SEQUENCE_EXHAUSTED UINT32_C(5)

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
  uint32_t format;
  uint32_t size;
  uint32_t line_words;
  uint32_t tmem_word;
  uint32_t palette;
  uint32_t mask_s;
  uint32_t mask_t;
  uint32_t shift_s;
  uint32_t shift_t;
  uint32_t uls;
  uint32_t ult;
  uint32_t lrs;
  uint32_t lrt;
  bool clamp_s;
  bool mirror_s;
  bool clamp_t;
  bool mirror_t;
  bool valid;
  bool size_valid;
} t64_rdp_tile;

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
  /* T64_PI_DIRECTION_* of the active descriptor; 0 for the ROM path. */
  uint32_t active_save_direction;
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
  T64_RSP_DMA_STEP_NONE = 0,
  T64_RSP_DMA_STEP_SETUP = 1,
  T64_RSP_DMA_STEP_BEAT = 2
} t64_rsp_dma_step_kind;

typedef struct {
  t64_rsp_dma_step_kind kind;
  uint64_t token;
  uint64_t expected_cursor;
  uint64_t next_event_sequence;
  uint32_t internal_offset;
  uint32_t dram_offset;
  bool dram_to_sp;
  bool imem;
} t64_rsp_dma_step;

typedef struct {
  t64_sp_dma_descriptor descriptor;
  uint64_t token;
  uint32_t raw_length;
  uint32_t offset;
  t64_status status;
  uint32_t refusal_reason;
  bool dram_to_sp;
  bool pending_slot;
  bool valid;
} t64_rsp_dma_admission;

typedef enum {
  T64_RSP_STATUS_ORIGIN_HOST = 0,
  T64_RSP_STATUS_ORIGIN_GUEST = 1
} t64_rsp_status_origin;

/* Every field here can affect future behavior and belongs to state v12. */
typedef struct {
  uint32_t gpr[32];
  uint8_t vector[32][16];
  uint64_t accumulator[8];
  uint16_t vco;
  uint16_t vcc;
  uint8_t vce;
  uint32_t pc;
  uint32_t delay_target;
  bool delay_slot_pending;
  bool branch_bubble_pending;
  uint32_t divider_input;
  uint32_t divider_result;
  bool divider_high_valid;
  uint64_t scalar_ready_tick[32];
  uint64_t vector_ready_tick[32];
  uint64_t accumulator_ready_tick[8];
  uint64_t memory_access_ready_tick;
  uint64_t next_issue_tick;
  uint64_t issue_sequence;
  uint64_t retired_instructions;
  uint64_t stall_slots;
  uint32_t sp_status;
  bool sp_semaphore;
  uint64_t dma_beat_cursor;
  uint64_t dma_setup_remaining;
  uint64_t active_dma_token;
  uint64_t pending_dma_token;
  uint64_t dma_event_sequence;
} t64_rsp_state;

typedef enum {
  T64_RSP_ATTEMPT_NONE = 0,
  T64_RSP_ATTEMPT_RETIRED = 1,
  T64_RSP_ATTEMPT_REFUSED = 2
} t64_rsp_attempt_outcome;

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
  bool clear_fcr31_causes;
  uint32_t fcr31_causes;
  uint32_t fcr31_flags;
  bool write_fcr31_condition;
  bool fcr31_condition;
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
  uint64_t delay_origin_pc_full;
  uint64_t stop_pc_full;
  uint32_t attempt_outcome;
  uint32_t semantic_policy_version;
  uint64_t cp2_transfer_latch;
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

#define T64_RDRAM_PROTOCOL_SLOT_COUNT 2u
#define T64_RDRAM_PROTOCOL_REGISTER_COUNT 10u
#define T64_RDRAM_PROTOCOL_SLOT_BYTES UINT32_C(0x00200000)

#define T64_RDRAM_CALIBRATION_NONE UINT8_C(0)
#define T64_RDRAM_CALIBRATION_MANUAL_COMMAND UINT8_C(1)
#define T64_RDRAM_CALIBRATION_AUTO_COMMAND UINT8_C(2)

typedef struct {
  uint16_t device_id;
  uint8_t calibration_state;
  bool present;
  bool device_id_known;
  uint16_t registers_known;
  uint32_t register_values[T64_RDRAM_PROTOCOL_REGISTER_COUNT];
} t64_rdram_device_state;

typedef struct {
  uint32_t ri_config;
  bool ri_config_known;
  uint32_t ri_current_load;
  bool ri_current_load_known;
  uint64_t ri_config_tick;
  uint64_t ri_current_load_deadline_tick;
  uint32_t ri_mode;
  bool ri_mode_known;
  uint64_t ri_mode_tick;
  uint64_t ri_mode_deadline_tick;
  uint32_t ri_refresh;
  bool ri_refresh_known;
  bool mi_repeat_enabled;
  bool mi_repeat_known;
  uint32_t mi_repeat_length;
  bool mi_upper_mode;
  uint16_t broadcast_registers_known;
  uint32_t broadcast_register_values[T64_RDRAM_PROTOCOL_REGISTER_COUNT];
  uint8_t logical_device_count;
  bool capacity_scan_complete;
  uint32_t logical_capacity_bytes;
  t64_rdram_device_state devices[T64_RDRAM_PROTOCOL_SLOT_COUNT];
} t64_rdram_protocol_state;

typedef struct {
  uint32_t policy_version;
  uint32_t mi_version;
  uint32_t ri_select;
  bool mi_version_known;
  bool ri_select_known;
  t64_rdram_protocol_state rdram_protocol;
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

typedef struct {
  uint32_t dpc_start;
  uint32_t dpc_end;
  uint32_t dpc_current;
  uint32_t dpc_status;
  uint32_t dpc_clock_counter;
  uint32_t dpc_buffer_busy_counter;
  uint32_t dpc_pipe_busy_counter;
  uint32_t dpc_tmem_counter;
  uint32_t pending_command_address;
  uint32_t pending_command_byte;
  uint32_t pending_opcode_field;
  uint64_t pending_command_word;
  uint64_t fence_generation;
  uint32_t color_image_address;
  uint32_t depth_image_address;
  uint32_t color_image_width;
  uint32_t color_image_format;
  uint32_t color_image_size;
  uint32_t scissor_xh;
  uint32_t scissor_yh;
  uint32_t scissor_xl;
  uint32_t scissor_yl;
  uint64_t other_modes;
  uint32_t fill_color;
  uint16_t primitive_z;
  uint16_t primitive_dz;
  uint32_t texture_image_address;
  uint32_t texture_image_width;
  uint32_t texture_image_format;
  uint32_t texture_image_size;
  uint32_t texture_image_valid;
  uint32_t tlut_format;
  uint32_t render_kind;
  uint32_t render_texture_tile;
  int32_t render_texture_s;
  int32_t render_texture_t;
  int32_t render_texture_dsdx;
  int32_t render_texture_dtdy;
  uint64_t copy_count;
  uint32_t convert_coefficients[6];
  uint64_t pending_texrect_word;
  uint64_t pending_triangle_words[T64_RDP_TRIANGLE_MAX_WORDS];
  uint32_t pending_triangle_word_count;
  uint32_t primitive_color;
  uint32_t primitive_lod_fraction;
  uint32_t environment_color;
  uint32_t fog_color;
  uint32_t blend_color;
  uint64_t combine_mode;
  uint32_t key_center[3];
  uint32_t key_scale[3];
  uint32_t key_width[3];
  uint32_t triangle_count;
  uint32_t triangle_last_coverage_x;
  uint32_t triangle_last_coverage_y;
  uint16_t triangle_last_coverage_mask;
  uint8_t tmem[T64_RDP_TMEM_BYTES];
  t64_rdp_tile tiles[T64_RDP_TILE_COUNT];
  uint32_t work_policy_version;
  uint32_t random_state;
  uint32_t refusal_reason;
  uint64_t command_count;
  uint64_t fill_count;
  uint64_t fullsync_count;
  uint64_t last_original_word;
  uint32_t last_command_byte;
  uint32_t last_opcode_field;
  uint32_t render_x_begin;
  uint32_t render_x_end;
  uint32_t render_y_begin;
  uint32_t render_y_end;
  uint32_t render_y;
  bool color_image_valid;
  bool depth_image_valid;
  bool primitive_depth_valid;
  bool scissor_valid;
  bool other_modes_valid;
  bool fill_color_valid;
  bool tlut_valid;
  bool render_active;
  bool fence_pending;
  bool busy;
  bool dpc_xbus;
  bool dpc_freeze;
  bool dpc_flush;
  bool dpc_start_valid;
  bool dpc_end_valid;
  bool command_pending;
  bool texrect_pending;
  bool render_texture_flip;
  bool primitive_color_valid;
  bool environment_color_valid;
  bool fog_color_valid;
  bool blend_color_valid;
  bool key_center_valid;
  bool key_scale_valid;
  bool key_width_valid;
  bool combine_mode_valid;
  bool render_triangle;
  bool shade_triangle;
} t64_rdp_state;

typedef struct {
  uint8_t texel0[4];
  uint8_t texel1[4];
  uint8_t shade[4];
  uint8_t lod_fraction;
} t64_rdp_combiner_inputs;

typedef struct {
  uint32_t address;
  uint16_t visible_word;
  uint8_t hidden_value;
  bool passed;
  bool update;
} t64_rdp_depth_pixel_result;

/*
 * Retained packet slots (plan 11-04, DR-08). The VI and AI queues keep their
 * packet metadata in these two internal types, member for member the layout of
 * today's public t64_video_packet_info and t64_audio_packet_info (488 and 280
 * bytes on LP64), so the public copy-out structs can grow a leading struct_size
 * at the flip without moving a retained byte, a digest member or a state
 * container byte. The audio slot keeps its own leading struct_size member: the
 * value (sizeof the slot) is stored at fill time, hashed by the digest and
 * restored by state import (PATTERNS H1).
 */
typedef struct {
  uint32_t width;
  uint32_t height;
  uint32_t stride_bytes;
  uint32_t source_address;
  uint32_t source_format;
  uint32_t output_encoding;
  uint32_t field;
  uint32_t parity;
  uint32_t control;
  uint32_t burst;
  uint32_t h_sync;
  uint32_t v_sync;
  uint32_t h_start;
  uint32_t h_end;
  uint32_t v_start;
  uint32_t v_end;
  uint32_t source_width;
  uint32_t source_pitch_bytes;
  uint32_t source_x_offset;
  uint32_t source_y_offset;
  uint32_t x_scale;
  uint32_t y_scale;
  uint32_t filter;
  uint64_t start_tick;
  uint64_t end_tick;
  uint64_t sequence;
  uint64_t commands_completed;
  uint64_t fullsync_completed;
  uint64_t last_command_word;
  uint32_t last_command_byte;
  uint32_t last_opcode_field;
  size_t source_bytes;
  size_t packet_bytes;
  char profile_id[T64_PROFILE_TEXT_CAPACITY];
  char build_id[128];
  char vi_clock_policy[T64_PROFILE_TEXT_CAPACITY];
  char vi_output_policy[T64_PROFILE_TEXT_CAPACITY];
} t64_vi_packet_slot;

typedef struct {
  uint32_t struct_size;
  uint64_t sequence;
  uint64_t first_sample_index;
  uint64_t first_tick;
  uint64_t last_tick;
  uint32_t frame_count;
  uint64_t rate_numerator;
  uint64_t rate_denominator;
  uint32_t channels;
  uint32_t bits_per_sample;
  uint32_t encoding;
  size_t byte_length;
  char profile_id[T64_PROFILE_TEXT_CAPACITY];
  char build_id[128];
} t64_ai_packet_slot;

#if UINTPTR_MAX == UINT64_MAX
_Static_assert(sizeof(t64_vi_packet_slot) == 488u,
               "retained VI slot layout is pinned by the digest and the state");
_Static_assert(offsetof(t64_vi_packet_slot, width) == 0u,
               "retained VI slot first member moved");
_Static_assert(offsetof(t64_vi_packet_slot, vi_output_policy) +
                       sizeof(((t64_vi_packet_slot *)0)->vi_output_policy) ==
                   488u,
               "retained VI slot last member moved");
_Static_assert(sizeof(t64_ai_packet_slot) == 280u,
               "retained AI slot layout is pinned by the digest and the state");
_Static_assert(offsetof(t64_ai_packet_slot, struct_size) == 0u,
               "retained AI slot first member moved");
_Static_assert(offsetof(t64_ai_packet_slot, build_id) +
                       sizeof(((t64_ai_packet_slot *)0)->build_id) ==
                   280u,
               "retained AI slot last member moved");
#endif

typedef struct {
  uint32_t registers[14];
  uint32_t active_registers[14];
  uint64_t phase_remainder;
  uint64_t phase_cycles;
  uint64_t phase_units;
  uint64_t next_boundary_units;
  uint64_t last_guest_tick;
  uint64_t field_start_tick;
  uint64_t next_sequence;
  uint64_t event_generation;
  uint32_t field;
  uint32_t current_readback;
  uint32_t refusal_reason;
  size_t packet_lengths[T64_VIDEO_QUEUE_CAPACITY];
  t64_vi_packet_slot packet_info[T64_VIDEO_QUEUE_CAPACITY];
  /* Host observer cursor and occupancy; neither is part of guest identity. */
  size_t packet_head;
  size_t packet_count;
  bool enabled;
  bool sequence_exhausted;
  bool latch_pending;
} t64_vi_state;

typedef struct {
  uint32_t address;
  uint32_t length;
  uint64_t sequence;
  uint32_t dacrate;
  uint32_t bitrate;
} t64_ai_dma_descriptor;

typedef struct {
  uint32_t staged_address;
  uint32_t active_byte_cursor;
  uint32_t control;
  uint32_t dacrate;
  uint32_t bitrate;
  uint32_t latched_dacrate;
  uint32_t latched_bitrate;
  uint64_t phase_remainder;
  uint64_t sample_index;
  uint64_t descriptor_sequence;
  uint64_t event_token;
  uint64_t event_generation;
  uint64_t packet_start_tick;
  uint64_t packet_first_sample;
  size_t packet_length;
  size_t packet_lengths[T64_AUDIO_QUEUE_CAPACITY];
  t64_ai_packet_slot packet_info[T64_AUDIO_QUEUE_CAPACITY];
  t64_ai_dma_descriptor descriptors[T64_AUDIO_QUEUE_CAPACITY];
  size_t descriptor_head;
  size_t descriptor_count;
  size_t output_reservation_slot;
  /* Host observer cursor and occupancy are excluded from guest identity. */
  size_t packet_head;
  size_t packet_count;
  bool active;
  bool paused;
  bool address_staged;
  bool packet_reserved;
  bool sequence_exhausted;
  uint64_t paused_remaining_ticks;
  uint64_t packet_history_count;
  uint8_t packet_history_sha256[T64_DIGEST_SIZE];
} t64_ai_state;

/*
 * Serial interface, PIF RAM and standard-controller latch (Phase 9, plan
 * 09-03). Every figure below is a named software-profile policy value of the
 * composite family; none is a hardware measurement.
 */
#define T64_SI_BASE UINT32_C(0x04800000)
#define T64_MI_INTERRUPT_SI UINT32_C(0x00000002)
#define T64_SI_REG_DRAM_ADDR UINT32_C(0x00)
#define T64_SI_REG_RD64B UINT32_C(0x04)
#define T64_SI_REG_WR64B UINT32_C(0x10)
#define T64_SI_REG_STATUS UINT32_C(0x18)
#define T64_SI_PIF_ADDRESS UINT32_C(0x1FC007C0)
#define T64_PIF_RAM_BYTES 64u
#define T64_SI_DRAM_ADDRESS_MASK UINT32_C(0x00FFFFFF)
#define T64_SI_STATUS_DMA_BUSY UINT32_C(0x00000001)
#define T64_SI_STATUS_DMA_ERROR UINT32_C(0x00000008)
#define T64_SI_STATUS_INTERRUPT UINT32_C(0x00001000)
#define T64_SI_WRITE_DMA_TICKS UINT64_C(2500)
#define T64_SI_READ_TAIL_TICKS UINT64_C(4096)
#define T64_JOYBUS_BYTE_TICKS UINT64_C(2000)
#define T64_JOYBUS_CONSOLE_STOP_TICKS UINT64_C(188)
#define T64_JOYBUS_TX_RX_GAP_TICKS UINT64_C(438)
#define T64_JOYBUS_DEVICE_STOP_TICKS UINT64_C(250)
#define T64_JOYBUS_TIMEOUT_TICKS UINT64_C(3907)
#define T64_JOYBUS_RESET_TICKS UINT64_C(62500)
#define T64_JOYBUS_CHANNELS 5u
#define T64_SI_PORTS 4u
#define T64_SI_DIRECTION_NONE UINT32_C(0)
#define T64_SI_DIRECTION_WR64B UINT32_C(1)
#define T64_SI_DIRECTION_RD64B UINT32_C(2)
/* Internal trace kind for a refused SI register write; not a scheduled
 * event kind, so it never appears in the event ledger. */
#define T64_SI_TRACE_KIND_REFUSED UINT32_C(3)

typedef enum {
  T64_JOYBUS_ENTRY_UNUSED = 0,
  T64_JOYBUS_ENTRY_TRANSACT = 1,
  T64_JOYBUS_ENTRY_RESET = 2
} t64_joybus_entry_kind;

typedef struct {
  uint32_t kind;
  uint32_t channel;
  uint32_t offset;
  uint32_t tx_count;
  uint32_t rx_count;
  uint32_t flags;
} t64_joybus_entry;

typedef struct {
  bool parsed;
  uint32_t entry_count;
  t64_joybus_entry entries[T64_JOYBUS_CHANNELS];
} t64_joybus_table;

/* The controller value the executor reports for channel 0; sampled from the
 * input latch at the execute tick by the SI layer. */
typedef struct {
  bool present;
  uint16_t buttons;
  uint8_t x;
  uint8_t y;
} t64_joybus_controller;

typedef struct {
  uint16_t current_buttons;
  uint8_t current_x;
  uint8_t current_y;
  uint16_t next_buttons;
  uint8_t next_x;
  uint8_t next_y;
  uint64_t next_visible_from;
  bool next_valid;
} t64_si_port_latch;

typedef struct {
  uint32_t dram_address;
  bool error;
  bool active_valid;
  uint32_t active_direction;
  uint32_t active_dram_address;
  uint64_t start_tick;
  uint64_t execute_tick;
  uint64_t complete_tick;
  uint64_t active_token;
  uint64_t next_token;
  bool token_exhausted;
  bool execute_pending;
  uint8_t pif_ram[T64_PIF_RAM_BYTES];
  t64_joybus_table table;
  t64_si_port_latch latch[T64_SI_PORTS];
} t64_si_state;

/*
 * Phase 9 save media (plan 09-04). Named software-profile policy values; no
 * hardware accuracy or hardware timing is claimed. The storage is a trailing
 * region of the single machine allocation; the kind comes from the profile
 * only (never from the image).
 */
#define T64_SAVE_EEPROM_4K_BYTES 512u
#define T64_SAVE_EEPROM_16K_BYTES 2048u
#define T64_SAVE_SRAM_BYTES 32768u
#define T64_SAVE_EEPROM_BLOCK_BYTES 8u
/* eeprom-busy-ceiling-6ms-v1: 375,000 ticks, a named policy built on an
 * up-to-6-ms ceiling; never an exact or measured value. */
#define T64_SAVE_EEPROM_BUSY_TICKS UINT64_C(375000)
#define T64_SRAM_BASE UINT32_C(0x08000000)
#define T64_PI_DIRECTION_ROM_TO_RDRAM 0u
#define T64_PI_DIRECTION_SRAM_TO_RDRAM 1u
#define T64_PI_DIRECTION_RDRAM_TO_SRAM 2u

typedef struct {
  /* The tick from which the EEPROM answers idle again; 0 until a write. */
  uint64_t busy_until;
  /* Successful host imports since creation or the last replacement (plan
   * 09-05). Observer only: outside the canonical digest and the walker;
   * guest writes do not increment it. */
  uint64_t write_generation;
  /* Storage bytes inside the trailing region; NULL when the profile has no
   * save device. */
  uint8_t *bytes;
} t64_save_state;

/* The one saturating increment of the import generation (plan 09-10, review
 * L1 IN-03 and R-05): a host save import and a state import into a profile with
 * a save device add one; it never wraps. */
void t64_save_generation_advance(t64_machine *machine);

typedef enum {
  T64_SAVE_CH4_ABSENT = 0,
  T64_SAVE_CH4_VALID = 1,
  T64_SAVE_CH4_TIMEOUT = 2
} t64_save_channel4_class;

size_t t64_rdp_triangle_word_count(uint32_t opcode);
/* The registers a triangle row reads, as plain values: the same view is built
 * from the live RDP state at command acceptance and from the blob values by
 * the state importer (09-10 FX-01). */
typedef struct {
  uint32_t scissor_xh;
  uint32_t scissor_yh;
  uint32_t scissor_xl;
  uint32_t scissor_yl;
  uint32_t image_address;
  uint32_t image_width;
  uint32_t image_size;
  uint64_t other_modes;
  uint32_t depth_address;
  bool depth_valid;
  bool primitive_depth_valid;
} t64_rdp_triangle_view;
bool t64_rdp_triangle_opcode_accepted(const t64_machine *machine,
                                      uint32_t opcode);
bool t64_rdp_triangle_extent(const t64_machine *machine, uint32_t opcode,
                             const uint64_t *words, size_t word_count,
                             const t64_rdp_triangle_view *view,
                             uint32_t *y_begin_out, uint32_t *y_end_out);
bool t64_rdp_triangle_bounds(const t64_machine *machine, uint32_t opcode,
                             const uint64_t *words, size_t word_count,
                             uint32_t *y_begin_out,
                             uint32_t *y_end_out);
void t64_rdp_triangle_render_row(t64_machine *machine);

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

/*
 * State import scratch (plan 09-09, K8). The validate pass of the importer
 * records the scalars the structural and per-device checks need, and a
 * per-(device, kind) summary of the scheduler queue, here instead of on a
 * stack frame (the core builds with -Wframe-larger-than=2048). The summary is
 * zero outside an import, never part of the digest, the export or a profile,
 * and is cleared on every exit path of an attempt.
 */
#define T64_STATE_PAIR_COUNT 10u
#define T64_STATE_SUMMARY_MAX_BYTES 8192u

typedef struct {
  uint32_t count;     /* queued events of this (device, kind) pair */
  uint32_t max_count; /* how many of them carry the largest token */
  uint64_t tick;      /* the first event seen: tick, token and value */
  uint64_t token;
  uint64_t value;
  uint64_t token_max; /* the largest token seen */
} t64_state_pair;

typedef struct {
  uint64_t tick;                 /* machine tick of the blob */
  uint64_t pi_active_token;
  uint64_t pi_active_length;
  uint64_t rsp_issue_sequence;
  uint64_t rsp_next_issue_tick;
  uint64_t vi_event_generation;
  uint64_t ai_event_token;
  uint64_t ai_paused_remaining_ticks;
  uint64_t si_start_tick;
  uint64_t si_execute_tick;
  uint64_t si_complete_tick;
  uint64_t si_active_token;
  uint32_t halted;
  uint32_t cpu_stopped;
  uint32_t cpu_attempt_outcome;
  uint32_t pi_active_valid;
  uint32_t rsp_running;
  uint32_t sp_dma_active;
  uint32_t rdp_idle;
  uint32_t rdp_in_flight;
  uint32_t rdp_held;
  uint32_t vi_enabled;
  uint32_t ai_active;
  uint32_t ai_paused;
  uint32_t si_active_valid;
  uint32_t si_direction;
  uint32_t si_execute_pending;
  uint32_t pi_save_direction;
  uint32_t ai_descriptor_count;
  uint32_t captured_bytes;
  uint32_t queue_count;
  uint32_t sequence_exhausted;
  uint32_t reserved;
  uint64_t next_sequence;
  uint64_t previous_tick;
  uint64_t previous_sequence;
} t64_state_device_facts;

typedef struct {
  t64_state_device_facts facts;
  t64_state_pair pairs[T64_STATE_PAIR_COUNT];
} t64_state_summary;

_Static_assert(sizeof(t64_state_summary) <= T64_STATE_SUMMARY_MAX_BYTES,
               "the import scratch must stay below its size limit");

struct t64_machine {
  t64_host_allocator allocator;
  t64_host_storage machine_storage;
  t64_host_storage image_storage;
  t64_profile_descriptor profile;
  uint8_t profile_digest[T64_DIGEST_SIZE];
  uint8_t content_digest[T64_DIGEST_SIZE];
  uint8_t *ram;
  uint16_t *rdp_triangle_coverage;
  uint8_t *rdp_memory_coverage;
  uint8_t *rdp_depth_hidden;
  uint8_t sp_dmem[T64_SP_BANK_BYTES];
  uint8_t sp_imem[T64_SP_BANK_BYTES];
  uint32_t mi_pending;
  uint32_t mi_mask;
  bool mi_irq_line;
  t64_pi_state pi;
  t64_sp_state sp;
  t64_rsp_state rsp;
  t64_cpu_state cpu;
  t64_boot_state boot;
  t64_cache_state cache;
  t64_guest_device_state guest_devices;
  t64_output_state output;
  t64_rdp_state rdp;
  t64_vi_state vi;
  t64_host_storage video_storage;
  uint8_t *video_packets;
  t64_ai_state ai;
  t64_si_state si;
  t64_save_state save;
  t64_host_storage audio_storage;
  uint8_t *audio_packets;
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
  /*
   * Defect and reentrancy flags (plan 11-03, DR-10). Both are outside the
   * digest, the state container and every walker (those emit named members,
   * never struct bytes), and neither is touched by replacement or import.
   * poisoned is set only by t64_internal_defect at an internal invariant
   * violation and is never cleared; in_call is non-zero while the core calls
   * the host allocator on behalf of this instance.
   */
  uint8_t poisoned;
  uint8_t in_call;
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
  /* Diagnostic observation only; excluded from canonical guest state. */
  uint32_t rsp_observed_pc;
  uint32_t rsp_observed_opcode;
  uint32_t rsp_observed_second_pc;
  uint32_t rsp_observed_second_opcode;
  uint64_t rsp_observed_tick;
  uint64_t rsp_observed_ready_tick;
  uint64_t rsp_observed_dma_cursor;
  uint32_t rsp_observed_stall_reason;
  uint32_t rsp_observed_refusal_reason;
  uint32_t rsp_observed_dma_action;
  uint32_t rsp_observed_outcome;
  bool rsp_observation_valid;
  /* Import scratch (plan 09-09): zero outside an import attempt; excluded from
   * the digest, the export and replacement. */
  t64_state_summary state_summary;
};

/*
 * Entry guard of every public entry that takes a machine except
 * t64_machine_destroy and t64_machine_is_poisoned (plan 11-03, DR-10). The
 * order is observable: a NULL machine is INVALID_ARGUMENT, a poisoned
 * instance is POISONED, a call made while the core is inside a host allocator
 * callback for the same instance is REENTRANT; only then do the entry's own
 * checks run. A guard refusal returns before the entry reads or writes
 * anything else, so every output stays byte for byte as the caller left it.
 */
static inline t64_status t64_internal_entry_guard(
    const t64_machine *machine) {
  if (machine == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (machine->poisoned != 0u) {
    return T64_STATUS_POISONED;
  }
  if (machine->in_call != 0u) {
    return T64_STATUS_REENTRANT;
  }
  return T64_STATUS_OK;
}

#define T64_ENTRY_GUARD(machine_)                                           \
  do {                                                                      \
    const t64_status t64_guard_status_ = t64_internal_entry_guard(machine_); \
    if (t64_guard_status_ != T64_STATUS_OK) {                               \
      return t64_guard_status_;                                             \
    }                                                                       \
  } while (0)

/*
 * Poisons the instance and returns T64_STATUS_INTERNAL_DEFECT. This is the
 * only function that sets the poisoned flag, and it is called only where the
 * core finds one of its own invariants broken, never for a caller fault. It
 * takes a const pointer because const entries (digest, state export) detect
 * defects; the definition documents the cast.
 */
t64_status t64_internal_defect(const t64_machine *machine);

static inline bool t64_has_ai_profile(const t64_machine *machine) {
  return machine != NULL && machine->profile.descriptor_version == 11u &&
         machine->profile.machine_model_version == 19u &&
         machine->profile.state_serialization_version == 24u;
}

/*
 * Phase 9 composite family: the Phase 8 machine plus the SI/PIF controller
 * and an explicit save device, all four registered ids sharing one tuple.
 * This exact tuple selects the state 25 digest domain, the state version
 * registry row and the exporter.
 */
static inline bool t64_has_si_v25_profile(const t64_machine *machine) {
  return machine != NULL && machine->profile.descriptor_version == 12u &&
         machine->profile.machine_model_version == 20u &&
         machine->profile.state_serialization_version == 25u;
}

/*
 * Phase 10 composite family (plan 10-09): the same devices under four new
 * registered ids with RDP depth active, one tuple. This exact tuple selects
 * the state 26 digest domain; the registry and the exporter refuse it until
 * their rows exist.
 */
static inline bool t64_has_si_v26_profile(const t64_machine *machine) {
  return machine != NULL && machine->profile.descriptor_version == 13u &&
         machine->profile.machine_model_version == 21u &&
         machine->profile.state_serialization_version == 26u;
}

/* SI device family: the exact 12/20/25 tuple or the exact 13/21/26 tuple. */
static inline bool t64_has_si_profile(const t64_machine *machine) {
  return t64_has_si_v25_profile(machine) || t64_has_si_v26_profile(machine);
}

static inline bool t64_has_vi_filter_v9_profile(
    const t64_machine *machine) {
  return (machine != NULL && machine->profile.descriptor_version == 10u &&
          machine->profile.machine_model_version == 18u &&
          machine->profile.state_serialization_version == 23u) ||
         t64_has_ai_profile(machine) || t64_has_si_profile(machine);
}

/* The audio interface exists on the Phase 8 profile and on the composite
 * family; the digest keeps the exact tuple predicates. */
static inline bool t64_has_ai_device(const t64_machine *machine) {
  return t64_has_ai_profile(machine) || t64_has_si_profile(machine);
}

/* Save devices exist on the composite family only, selected by the profile
 * save_kind and by nothing in the image. */
static inline bool t64_has_eeprom_save(const t64_machine *machine) {
  return t64_has_si_profile(machine) &&
         (machine->profile.save_kind == T64_SAVE_EEPROM_4K ||
          machine->profile.save_kind == T64_SAVE_EEPROM_16K);
}

static inline bool t64_has_sram_save(const t64_machine *machine) {
  return t64_has_si_profile(machine) &&
         machine->profile.save_kind == T64_SAVE_SRAM_32K;
}

static inline bool t64_has_rdp_vi_profile(const t64_machine *machine) {
  return machine != NULL &&
         ((machine->profile.machine_model_version == 11u &&
           machine->profile.state_serialization_version == 16u) ||
          (machine->profile.machine_model_version == 12u &&
           machine->profile.state_serialization_version == 17u) ||
          (machine->profile.machine_model_version == 13u &&
           machine->profile.state_serialization_version == 18u) ||
          (machine->profile.machine_model_version == 14u &&
           machine->profile.state_serialization_version == 19u) ||
          (machine->profile.machine_model_version == 15u &&
           machine->profile.state_serialization_version == 20u) ||
          (machine->profile.machine_model_version == 16u &&
           machine->profile.state_serialization_version == 21u) ||
          (machine->profile.machine_model_version == 17u &&
           machine->profile.state_serialization_version == 22u) ||
          t64_has_vi_filter_v9_profile(machine));
}

static inline bool t64_has_vi_scanout_v8_profile(
    const t64_machine *machine) {
  return machine != NULL && machine->profile.descriptor_version == 10u &&
         machine->profile.machine_model_version == 17u &&
         machine->profile.state_serialization_version == 22u;
}

static inline bool t64_has_vi_scanout_profile(const t64_machine *machine) {
  return t64_has_vi_scanout_v8_profile(machine) ||
         t64_has_vi_filter_v9_profile(machine);
}

static inline bool t64_has_rdp_triangle_profile(const t64_machine *machine) {
  return machine != NULL &&
         ((machine->profile.machine_model_version == 12u &&
           machine->profile.state_serialization_version == 17u) ||
          (machine->profile.machine_model_version == 13u &&
           machine->profile.state_serialization_version == 18u) ||
          (machine->profile.machine_model_version == 14u &&
           machine->profile.state_serialization_version == 19u) ||
          (machine->profile.machine_model_version == 15u &&
           machine->profile.state_serialization_version == 20u) ||
          (machine->profile.machine_model_version == 16u &&
           machine->profile.state_serialization_version == 21u) ||
          (machine->profile.machine_model_version == 17u &&
           machine->profile.state_serialization_version == 22u) ||
          t64_has_vi_filter_v9_profile(machine));
}

static inline bool t64_has_rdp_texture_profile(const t64_machine *machine) {
  return machine != NULL &&
         ((machine->profile.machine_model_version == 13u &&
           machine->profile.state_serialization_version == 18u) ||
          (machine->profile.machine_model_version == 14u &&
           machine->profile.state_serialization_version == 19u) ||
          (machine->profile.machine_model_version == 15u &&
           machine->profile.state_serialization_version == 20u) ||
          (machine->profile.machine_model_version == 16u &&
           machine->profile.state_serialization_version == 21u) ||
          (machine->profile.machine_model_version == 17u &&
           machine->profile.state_serialization_version == 22u) ||
          t64_has_vi_filter_v9_profile(machine));
}

static inline bool t64_has_rdp_combiner_profile(const t64_machine *machine) {
  return machine != NULL &&
         ((machine->profile.machine_model_version == 14u &&
           machine->profile.state_serialization_version == 19u) ||
          (machine->profile.machine_model_version == 15u &&
           machine->profile.state_serialization_version == 20u) ||
          (machine->profile.machine_model_version == 16u &&
           machine->profile.state_serialization_version == 21u) ||
          (machine->profile.machine_model_version == 17u &&
           machine->profile.state_serialization_version == 22u) ||
          t64_has_vi_filter_v9_profile(machine));
}

static inline bool t64_has_rdp_blender_profile(const t64_machine *machine) {
  return machine != NULL &&
         ((machine->profile.machine_model_version == 15u &&
           machine->profile.state_serialization_version == 20u) ||
          (machine->profile.machine_model_version == 16u &&
           machine->profile.state_serialization_version == 21u) ||
          (machine->profile.machine_model_version == 17u &&
           machine->profile.state_serialization_version == 22u) ||
          t64_has_vi_filter_v9_profile(machine));
}

/*
 * RDP depth is active on the descriptor 10 profiles of models 16 to 18 and on
 * the descriptor 13 family (plan 10-09). The v26 term sits outside the
 * descriptor 10 conjunction, so the descriptor 11 and 12 composite ids keep
 * their Z refusal (K1) although the family predicates below include them.
 */
static inline bool t64_has_rdp_depth_profile(const t64_machine *machine) {
  return (machine != NULL && machine->profile.descriptor_version == 10u &&
          ((machine->profile.machine_model_version == 16u &&
            machine->profile.state_serialization_version == 21u) ||
           (machine->profile.machine_model_version == 17u &&
            machine->profile.state_serialization_version == 22u) ||
           t64_has_vi_filter_v9_profile(machine))) ||
         t64_has_si_v26_profile(machine);
}

static inline bool t64_has_vr4300_cpu_state(const t64_machine *machine) {
  return machine != NULL &&
         ((machine->profile.machine_model_version == 3u &&
           machine->profile.state_serialization_version == 9u) ||
          (machine->profile.machine_model_version == 4u &&
           machine->profile.state_serialization_version == 10u) ||
          (machine->profile.machine_model_version == 5u &&
           machine->profile.state_serialization_version == 11u) ||
          (machine->profile.machine_model_version == 6u &&
           machine->profile.state_serialization_version == 12u) ||
          (machine->profile.machine_model_version == 7u &&
           machine->profile.state_serialization_version == 13u) ||
          (machine->profile.machine_model_version == 8u &&
           machine->profile.state_serialization_version == 13u) ||
          t64_has_rdp_vi_profile(machine));
}

static inline bool t64_has_t1_policy(const t64_machine *machine) {
  return machine != NULL &&
         ((machine->profile.machine_model_version == 7u &&
           machine->profile.state_serialization_version == 13u) ||
          (machine->profile.machine_model_version == 8u &&
           machine->profile.state_serialization_version == 13u) ||
          t64_has_rdp_vi_profile(machine));
}

static inline bool t64_has_rdram_protocol_policy(
    const t64_machine *machine) {
  return machine != NULL &&
         ((machine->profile.machine_model_version == 8u &&
           machine->profile.state_serialization_version == 13u) ||
          t64_has_rdp_vi_profile(machine));
}

static inline bool t64_has_cpu_compat_extension_policy(
    const t64_machine *machine) {
  return machine != NULL &&
         ((machine->profile.machine_model_version == 5u &&
           machine->profile.state_serialization_version == 11u) ||
          (machine->profile.machine_model_version == 6u &&
           machine->profile.state_serialization_version == 12u) ||
          (machine->profile.machine_model_version == 7u &&
           machine->profile.state_serialization_version == 13u) ||
          (machine->profile.machine_model_version == 8u &&
           machine->profile.state_serialization_version == 13u) ||
          t64_has_rdp_vi_profile(machine));
}

static inline bool t64_has_rsp_functional_profile(
    const t64_machine *machine) {
  return machine != NULL &&
         ((machine->profile.machine_model_version == 6u &&
           machine->profile.state_serialization_version == 12u) ||
          (machine->profile.machine_model_version == 7u &&
           machine->profile.state_serialization_version == 13u) ||
          (machine->profile.machine_model_version == 8u &&
           machine->profile.state_serialization_version == 13u) ||
          t64_has_rdp_vi_profile(machine));
}

static inline bool t64_has_cpu_compat_policy(const t64_machine *machine) {
  return machine != NULL &&
         ((machine->profile.machine_model_version == 4u &&
           machine->profile.state_serialization_version == 10u) ||
          t64_has_cpu_compat_extension_policy(machine));
}

static inline uint64_t
t64_cpu_delay_branch_epc(const t64_machine *machine) {
  if (machine == NULL) {
    return UINT64_C(0);
  }
  return t64_has_cpu_compat_policy(machine)
             ? machine->cpu.delay_origin_pc_full
             : machine->cpu.delay_slot_pc_full - UINT64_C(4);
}

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
/*
 * Pure capacity pre-check shared by every operation that needs several events
 * (CPU preflight, SI start): queue room, sequence headroom and tick headroom
 * for event_count events whose latest delay is max_delay_ticks. It reserves
 * nothing and changes nothing; the caller then reserves and commits the events
 * one after another, so a refusal never leaves a partial start (K16).
 */
t64_status t64_schedule_check_capacity(const t64_machine *machine,
                                       size_t event_count,
                                       uint64_t max_delay_ticks);
t64_status t64_schedule_commit(t64_machine *machine,
                               t64_event_reservation *reservation);
t64_status t64_schedule_cancel(t64_machine *machine, uint32_t device,
                               uint32_t kind, uint64_t token,
                               uint64_t *tick_out);
t64_status t64_input_event_handler(t64_machine *machine,
                                   const t64_event *event);
t64_status t64_scheduler_advance(t64_machine *machine,
                                 const t64_stop_request *request,
                                 t64_stop_result *result_out);
t64_status t64_cpu_schedule(t64_machine *machine);
t64_status t64_cpu_step(t64_machine *machine, const t64_event *event);
uint64_t t64_cpu_instruction_cost(const t64_machine *machine,
                                 uint32_t opcode);
t64_status t64_cop1_execute(t64_machine *machine, uint32_t opcode,
                            bool in_delay_slot,
                            t64_cpu_cop1_result *result_out);
bool t64_cop1_is_branch(uint32_t opcode);
bool t64_cop1_branch_encoding_valid(uint32_t opcode);
bool t64_cop1_branch_condition(const t64_cpu_state *cpu, uint32_t opcode,
                               bool *likely_out);
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
uint64_t t64_mmu_compat_normalize_page_mask(uint64_t page_mask);
bool t64_cpu_compat_zero_data_range(const t64_machine *machine,
                                   uint32_t address, size_t bytes);
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
/* In-tree trigger of the defect class: poisons the instance exactly as an
 * internal invariant violation does (plan 11-03, DR-10). */
void t64_test_force_defect(t64_machine *machine);
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
t64_status t64_rdram_read(const t64_machine *machine, uint32_t address,
                          uint32_t width, uint32_t *value_out);
t64_status t64_rdram_write(t64_machine *machine, uint32_t address,
                           uint32_t width, uint32_t value);
uint64_t t64_rdram_filter_read(const t64_machine *machine, uint32_t address,
                               uint32_t width, uint64_t value);
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
t64_status t64_rdp_read_register(const t64_machine *machine,
                                 uint32_t address, uint32_t *value_out);
t64_status t64_rdp_write_register(t64_machine *machine, uint32_t address,
                                  uint32_t value);
t64_status t64_rdp_event(t64_machine *machine, const t64_event *event);
bool t64_rdp_tmem_handles_opcode(uint32_t opcode);
bool t64_rdp_tmem_validate_command(const t64_machine *machine,
                                   uint32_t opcode, uint64_t word,
                                   uint32_t *reason_out);
void t64_rdp_tmem_apply_command(t64_machine *machine, uint32_t opcode,
                                uint64_t word);
bool t64_rdp_tmem_validate_copy(const t64_machine *machine, uint64_t first,
                                uint64_t second, uint32_t x0, uint32_t y0,
                                uint32_t x1, uint32_t y1,
                                uint32_t *reason_out);
bool t64_rdp_tmem_validate_triangle(const t64_machine *machine,
                                    uint32_t tile_index);
void t64_rdp_tmem_begin_copy(t64_machine *machine, uint64_t first,
                             uint64_t second, uint32_t x0, uint32_t y0,
                             uint32_t x1, uint32_t y1);
bool t64_rdp_tmem_copy_pixel(const t64_machine *machine, uint32_t x,
                             uint32_t y, uint16_t *pixel_out);
bool t64_rdp_tmem_triangle_pixel(const t64_machine *machine,
                                 uint32_t tile_index, int64_t s_q16,
                                 int64_t t_q16, int64_t w_q16,
                                 bool perspective, uint16_t *pixel_out);
bool t64_rdp_tmem_triangle_pixel_rgba8(const t64_machine *machine,
                                       uint32_t tile_index, int64_t s_q16,
                                       int64_t t_q16, int64_t w_q16,
                                       bool perspective,
                                       uint32_t *rgba_out);
bool t64_rdp_combine_pixel(t64_machine *machine,
                           const t64_rdp_combiner_inputs *inputs,
                           uint32_t *rgba_out);
uint8_t t64_rdp_memory_coverage_read(const t64_machine *machine,
                                    uint32_t address, uint32_t size_bytes);
void t64_rdp_memory_coverage_write(t64_machine *machine, uint32_t address,
                                   uint32_t size_bytes, uint8_t coverage);
void t64_rdp_memory_coverage_invalidate(t64_machine *machine,
                                        uint32_t address,
                                        uint32_t size_bytes);
bool t64_rdp_depth_triangle_preflight(const t64_machine *machine,
                                      uint32_t opcode, const uint64_t *words,
                                      size_t word_count, uint32_t x0,
                                      uint32_t x1, uint32_t y_begin,
                                      uint32_t y_end);
bool t64_rdp_depth_evaluate_pixel(
    const t64_machine *machine, uint32_t opcode, const uint64_t *words,
    int64_t xcenter, int64_t ycenter, int64_t xh16, int64_t yh16,
    int64_t major_center, uint32_t x, uint32_t y, uint32_t color_address,
    uint32_t color_size_bytes, uint8_t incoming_coverage, bool color_write,
    t64_rdp_depth_pixel_result *result_out);
void t64_rdp_depth_commit_pixel(t64_machine *machine,
                                const t64_rdp_depth_pixel_result *result);
bool t64_rdp_blend_pixel(t64_machine *machine,
                         const t64_rdp_combiner_inputs *inputs,
                         uint32_t pixel_rgba, uint32_t address,
                         uint32_t size_bytes, uint8_t new_coverage,
                         uint32_t *rgba_out, uint8_t *coverage_out,
                         bool *write_color_out);
uint16_t t64_rdp_quantize_rgba5551_v6(const t64_machine *machine,
                                      uint32_t color, uint32_t x, uint32_t y);
uint8_t t64_rdp_quantize_alpha5_v6(const t64_machine *machine,
                                   uint32_t color, uint32_t x, uint32_t y);
void t64_rdp_tmem_convert_yuv(const t64_rdp_state *rdp, uint8_t y,
                              uint8_t u, uint8_t v, uint8_t rgb_out[3]);
t64_status t64_vi_read_register(const t64_machine *machine,
                                uint32_t address, uint32_t *value_out);
t64_status t64_vi_write_register(t64_machine *machine, uint32_t address,
                                 uint32_t value);
t64_status t64_vi_event(t64_machine *machine, const t64_event *event);
void t64_rdp_reset_machine_state(t64_machine *machine);
void t64_vi_reset_machine_state(t64_machine *machine);
void t64_ai_reset_machine_state(t64_machine *machine);
t64_status t64_ai_read_register(const t64_machine *machine,
                               uint32_t address, uint32_t *value_out);
t64_status t64_ai_write_register(t64_machine *machine, uint32_t address,
                                 uint32_t value);
t64_status t64_ai_event(t64_machine *machine, const t64_event *event);
bool t64_ai_event_waits_for_output_capacity(const t64_machine *machine,
                                            const t64_event *event);
bool t64_ai_event_can_dispatch(const t64_machine *machine,
                               const t64_event *event);
t64_status t64_pi_read_register(const t64_machine *machine, uint32_t address,
                                uint32_t *value_out);
t64_status t64_pi_write_register(t64_machine *machine, uint32_t address,
                                 uint32_t value);
t64_status t64_pi_complete(t64_machine *machine, const t64_event *event);
void t64_pi_reset_machine_state(t64_machine *machine);
t64_status t64_si_read_register(const t64_machine *machine, uint32_t address,
                                uint32_t *value_out);
t64_status t64_si_write_register(t64_machine *machine, uint32_t address,
                                 uint32_t value);
t64_status t64_si_execute_event(t64_machine *machine, const t64_event *event);
t64_status t64_si_complete_event(t64_machine *machine,
                                 const t64_event *event);
void t64_si_reset_machine_state(t64_machine *machine);
/* Latch store: value packs buttons (bits 0 to 15), x (16 to 23) and y (24 to
 * 31); tick is the event tick of the input. Latch sample: promote at the
 * sample tick, then return the current value. */
void t64_si_input_latch_store(t64_machine *machine, uint32_t port,
                              uint64_t value, uint64_t tick);
void t64_si_latch_sample(t64_machine *machine, uint32_t port, uint64_t tick,
                         uint16_t *buttons_out, uint8_t *x_out,
                         uint8_t *y_out);
/* Pure joybus parser, tick planner and executor over a 64-byte PIF image. */
void t64_joybus_parse(const uint8_t pif_ram[T64_PIF_RAM_BYTES],
                      t64_joybus_table *table_out);
uint64_t t64_joybus_plan(const uint8_t pif_ram[T64_PIF_RAM_BYTES],
                         const t64_joybus_table *table, bool port0_present);
void t64_joybus_execute(uint8_t pif_ram[T64_PIF_RAM_BYTES],
                        const t64_joybus_table *table,
                        const t64_joybus_controller *port0);
/* Machine variants of the planner and executor (plan 09-04): the cartridge
 * channel (channel 4) is answered by the profile's save device at the execute
 * tick; the pure variants above keep answering absent there. */
uint64_t t64_joybus_plan_machine(const t64_machine *machine,
                                 const uint8_t pif_ram[T64_PIF_RAM_BYTES],
                                 const t64_joybus_table *table,
                                 bool port0_present);
void t64_joybus_execute_machine(t64_machine *machine, uint64_t tick,
                                uint8_t pif_ram[T64_PIF_RAM_BYTES],
                                const t64_joybus_table *table,
                                const t64_joybus_controller *port0);
/* Save media (core/t64_save.c). */
size_t t64_save_storage_bytes(uint32_t kind);
void t64_save_reset_machine_state(t64_machine *machine);
t64_save_channel4_class t64_save_channel4_classify(const t64_machine *machine,
                                                   uint8_t command,
                                                   uint32_t tx_count,
                                                   uint32_t rx_count);
uint8_t t64_save_channel4_execute(t64_machine *machine, uint64_t tick,
                                  const uint8_t *tx, uint32_t tx_count,
                                  uint8_t *rx, uint32_t rx_count);
/* SRAM: aligned word access inside the 32 KiB window (the bus has already
 * decoded the region) and the PI DMA effect at the completion event. */
t64_status t64_save_sram_read_word(const t64_machine *machine,
                                   uint32_t address, uint32_t *value_out);
t64_status t64_save_sram_write_word(t64_machine *machine, uint32_t address,
                                    uint32_t value);
/* True while an EEPROM deadline is in the future at the machine tick or an
 * SRAM DMA is active (the in-flight query behind T64_SAVE_INFO_BUSY). */
bool t64_save_in_flight(const t64_machine *machine);
t64_status t64_save_sram_dma_apply(t64_machine *machine, uint32_t direction,
                                   uint32_t dram_address,
                                   uint32_t sram_offset, uint32_t length);
t64_status t64_sp_read_register(t64_machine *machine, uint32_t address,
                                uint32_t *value_out);
t64_status t64_sp_peek_register(const t64_machine *machine,
                                uint32_t address, uint32_t *value_out);
t64_status t64_sp_write_register(t64_machine *machine, uint32_t address,
                                 uint32_t value);
t64_status t64_sp_rsp_guest_status_write(t64_machine *machine,
                                         uint32_t command);
t64_status t64_rsp_ensure_opportunity(t64_machine *machine);
t64_status t64_rsp_validate_status_command(const t64_machine *machine,
                                           uint32_t command,
                                           t64_rsp_status_origin origin);
t64_status t64_sp_rsp_dma_preflight(t64_machine *machine, bool rsp_fetches,
                                    bool rsp_touches_dmem,
                                    t64_rsp_dma_step *step_out);
void t64_sp_rsp_dma_commit(t64_machine *machine,
                           const t64_rsp_dma_step *step);
t64_status t64_sp_rsp_dma_admission_preflight(
    const t64_machine *machine, bool dram_to_sp, uint32_t raw_length,
    uint32_t offset, const t64_rsp_dma_step *step,
    t64_rsp_dma_admission *admission_out);
void t64_sp_rsp_dma_admission_commit(
    t64_machine *machine, const t64_rsp_dma_admission *admission);
void t64_sp_rsp_dma_admission_refuse(
    t64_machine *machine, const t64_rsp_dma_admission *admission);
bool t64_sp_rsp_dma_active(const t64_machine *machine);
t64_status t64_sp_complete(t64_machine *machine, const t64_event *event);
void t64_sp_reset_machine_state(t64_machine *machine);
t64_status t64_rsp_start(t64_machine *machine);

/* State import (core/t64_state.c). The closed handler table maps a queued
 * (device, kind) pair to its handler, or NULL for any pair outside the ten
 * production pairs of the event ledger (the synthetic test device included).
 * t64_rsp_issue_event_handler (core/t64_rsp.c) supplies the one handler that
 * is file-static. t64_state_summary_is_clear reports whether the scratch is
 * all zero. */
t64_event_handler_fn t64_state_handler_lookup(uint32_t device, uint32_t kind);
t64_event_handler_fn t64_rsp_issue_event_handler(void);
bool t64_state_summary_is_clear(const t64_machine *machine);

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
  if (machine->video_storage.data != NULL &&
      (!t64_internal_make_range(machine->video_storage.data,
                                machine->video_storage.capacity,
                                &owned_range) ||
       t64_internal_ranges_overlap(output_range, owned_range))) {
    return false;
  }
  if (machine->audio_storage.data != NULL &&
      (!t64_internal_make_range(machine->audio_storage.data,
                                machine->audio_storage.capacity,
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

/*
 * Canonical state stream sink (Phase 9). One walker produces the digest
 * preimage; a sink either hashes it, copies it out, or only counts it. Spans
 * record the non-body regions (identity, marker, new section) so that a
 * strip-and-compare against an older stream can tell them from body bytes.
 */
#define T64_DIGEST_CLASS_BODY UINT32_C(0)
#define T64_DIGEST_CLASS_IDENTITY UINT32_C(1)
#define T64_DIGEST_CLASS_MARKER UINT32_C(2)
#define T64_DIGEST_CLASS_NEW UINT32_C(3)

typedef struct {
  size_t offset;
  size_t length;
  uint32_t class;
} t64_digest_span;

/* Most section markers one stream can carry (the section recorder below). */
#define T64_DIGEST_MAX_SECTIONS 32u

typedef struct {
  uint8_t *buffer;         /* copy-out destination, or NULL */
  size_t capacity;         /* bytes available at buffer */
  size_t offset;           /* bytes streamed so far, even past capacity */
  void *hash;              /* t64_sha256_ctx updated as bytes stream, or NULL */
  t64_digest_span *spans;  /* non-body span records, or NULL */
  size_t span_count;
  size_t span_capacity;
  bool overflow;           /* a write passed capacity or the span array */
  /* Section recorder (plan 09-08): every v25 marker write appends its id and
   * the running offset of the marker (the body starts four bytes later). The
   * legacy streams carry no markers and never record. Callers zero-initialize
   * the sink, so a stream that does not need the recorder pays nothing. */
  uint32_t section_ids[T64_DIGEST_MAX_SECTIONS];
  size_t section_offsets[T64_DIGEST_MAX_SECTIONS];
  size_t section_count;
  bool section_overflow;   /* a 33rd marker: nothing further is recorded */
} t64_digest_sink;

/*
 * State section ids of the v25 stream. Each section is a 32-bit big-endian id
 * (marker class) followed by its body, in ascending id order. The v25 stream
 * is not comparable to the v24 stream; stripping markers and identity spans
 * from both makes the shared sections comparable.
 */
#define T64_STATE_SECTION_IDENTITY UINT32_C(1)
#define T64_STATE_SECTION_MACHINE_CORE UINT32_C(2)
#define T64_STATE_SECTION_RDRAM UINT32_C(3)
#define T64_STATE_SECTION_SP_MEMORY UINT32_C(4)
#define T64_STATE_SECTION_INTERRUPT_PI_SP UINT32_C(5)
#define T64_STATE_SECTION_SCHEDULER UINT32_C(6)
#define T64_STATE_SECTION_WARM_DEVICES UINT32_C(7)
#define T64_STATE_SECTION_RDP_VI UINT32_C(8)
#define T64_STATE_SECTION_AUDIO UINT32_C(9)
/* Emitted by the v25 walker (plan 09-03): SI, PIF RAM and the input latch,
 * every field in the new-span class. */
#define T64_STATE_SECTION_SI_PIF UINT32_C(10)
/* Emitted by the v25 walker for the three save ids (plan 09-04); the stem
 * stream stops at section 10. */
#define T64_STATE_SECTION_SAVE_MEDIA UINT32_C(11)
/* Observer continuity (plan 09-08): written by the state container export
 * after the canonical region and outside the digest; the walker never emits
 * it. */
#define T64_STATE_SECTION_OBSERVER_CONTINUITY UINT32_C(16)

/*
 * State container (plan 09-08, provisional until the Phase 11 freeze
 * candidate, final ABI bind at the end of Phase 12). All integers are
 * big-endian. Layout, offsets from the blob start: a 0x120-byte header, a
 * table of 56-byte entries (id, flags, body offset, body length, body
 * SHA-256), the canonical region (exactly the v25 digest preimage), the
 * 40-byte observer body and a 32-byte SHA-256 trailer over every byte before
 * it. docs/spec/state-container.md carries the literal offsets.
 */
#define T64_STATE_CONTAINER_MAGIC "T64STATE"
#define T64_STATE_CONTAINER_VERSION UINT32_C(1)
/* Container version word of a state 26 blob (plan 10-11). It is the one
 * version marker of the state 26 bump: a blob carries 1 with state 25 and 2
 * with state 26, never another pairing. Both are state-format constants of a
 * build-identity-bound artifact and sit outside the ABI promise. */
#define T64_STATE_CONTAINER_VERSION_V2 UINT32_C(2)
#define T64_STATE_HEADER_BYTES 0x120u
#define T64_STATE_ENTRY_BYTES 56u
#define T64_STATE_TRAILER_BYTES 32u
#define T64_STATE_OBSERVER_BYTES 40u
#define T64_STATE_MAX_SECTIONS 32u
#define T64_STATE_MAX_BYTES UINT64_C(67108864)
#define T64_STATE_BUILD_ID_BYTES 128u
#define T64_STATE_SECTION_FLAG_OUTSIDE_DIGEST UINT32_C(1)

/*
 * Total container length for a canonical region of canonical_length bytes
 * and section_count table entries (the observer entry included): the header,
 * the table, the canonical region, the observer body and the trailer, in
 * checked 64-bit arithmetic. Returns false, leaving *length_out unchanged,
 * for a count of zero or above T64_STATE_MAX_SECTIONS, any arithmetic wrap, or
 * a total above T64_STATE_MAX_BYTES.
 */
bool t64_state_container_length(uint64_t canonical_length,
                                uint32_t section_count,
                                uint64_t *length_out);

/*
 * Stream the canonical state of an admitted machine into the sink in the
 * order t64_machine_digest hashes it. NOT_READY before admission.
 */
t64_status t64_digest_stream(const t64_machine *machine,
                             t64_digest_sink *sink);

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
