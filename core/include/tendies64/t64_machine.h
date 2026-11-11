/*
 * Provisional deterministic machine seam. The public surface is not a frozen
 * ABI. Host callbacks own allocation and release; core execution never calls
 * back into the host.
 */
#ifndef TENDIES64_T64_MACHINE_H
#define TENDIES64_T64_MACHINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define T64_DIGEST_SIZE 32u
#define T64_ROM_HEADER_SIZE 64u
#define T64_PROFILE_TEXT_CAPACITY 64u
#define T64_PROFILE_SERIALIZED_MAX_BYTES 2048u
#define T64_PROFILE_SERIALIZATION_VERSION UINT32_C(1)
#define T64_TRACE_RECORD_CAPACITY 256u
#define T64_INPUT_AXIS_MIN INT32_C(-128)
#define T64_INPUT_AXIS_MAX INT32_C(127)

#define T64_TRACE_DEVICE_MACHINE UINT32_C(0)
#define T64_TRACE_DEVICE_BUS UINT32_C(1)
#define T64_TRACE_DEVICE_MI UINT32_C(2)
#define T64_TRACE_DEVICE_PI UINT32_C(3)
#define T64_TRACE_DEVICE_SP UINT32_C(4)
#define T64_TRACE_DEVICE_INPUT UINT32_C(6)
#define T64_TRACE_KIND_ROM_ADMITTED UINT32_C(1)
#define T64_TRACE_KIND_BUS_READ8 UINT32_C(1)
#define T64_TRACE_KIND_BUS_WRITE8 UINT32_C(2)
#define T64_TRACE_KIND_BUS_READ32 UINT32_C(3)
#define T64_TRACE_KIND_BUS_WRITE32 UINT32_C(4)
#define T64_TRACE_KIND_MI_PENDING UINT32_C(1)
#define T64_TRACE_KIND_MI_MASK UINT32_C(2)
#define T64_TRACE_KIND_PI_ISSUE UINT32_C(1)
#define T64_TRACE_KIND_PI_REFUSED UINT32_C(2)
#define T64_TRACE_KIND_PI_CANCEL UINT32_C(3)
#define T64_TRACE_KIND_PI_COMPLETE UINT32_C(4)
#define T64_TRACE_KIND_SP_ISSUE UINT32_C(1)
#define T64_TRACE_KIND_SP_REFUSED UINT32_C(2)
#define T64_TRACE_KIND_SP_PROMOTE UINT32_C(3)
#define T64_TRACE_KIND_SP_DROP UINT32_C(4)
#define T64_TRACE_KIND_SP_COMPLETE UINT32_C(5)
#define T64_TRACE_KIND_INPUT_SUBMITTED UINT32_C(1)

#define T64_STATUS_OK UINT32_C(0)
#define T64_STATUS_INVALID_ARGUMENT UINT32_C(1)
#define T64_STATUS_UNKNOWN_PROFILE UINT32_C(2)
#define T64_STATUS_ALLOCATION_FAILED UINT32_C(3)
#define T64_STATUS_BAD_STORAGE UINT32_C(4)
#define T64_STATUS_INVALID_ROM UINT32_C(5)
#define T64_STATUS_UNSUPPORTED_ROM_ORDER UINT32_C(6)
#define T64_STATUS_NOT_READY UINT32_C(7)
#define T64_STATUS_TIME_REVERSED UINT32_C(8)
#define T64_STATUS_BUFFER_TOO_SMALL UINT32_C(9)
#define T64_STATUS_LIMIT_REVERSED UINT32_C(10)
#define T64_STATUS_QUEUE_FULL UINT32_C(11)
#define T64_STATUS_SEQUENCE_EXHAUSTED UINT32_C(12)
#define T64_STATUS_TICK_OVERFLOW UINT32_C(13)
#define T64_STATUS_RESOURCE_EXHAUSTED UINT32_C(14)
#define T64_STATUS_BUS_UNMAPPED UINT32_C(15)
#define T64_STATUS_BUS_UNSUPPORTED UINT32_C(16)
#define T64_STATUS_BUS_ALIGNMENT UINT32_C(17)
#define T64_STATUS_BUS_RANGE UINT32_C(18)
#define T64_STATUS_BUS_DEVICE_REFUSED UINT32_C(19)

#define T64_TV_NTSC UINT32_C(1)
#define T64_TV_PAL UINT32_C(2)
#define T64_REGION_US UINT32_C(1)
#define T64_REGION_EUROPE UINT32_C(2)
#define T64_MEDIA_CARTRIDGE UINT32_C(1)
#define T64_SAVE_NONE UINT32_C(0)

#define T64_STOP_GUEST_TICK UINT32_C(1)
#define T64_STOP_INSTRUCTION_COUNT UINT32_C(2)
#define T64_STOP_FRAME_EVENT_COUNT UINT32_C(3)
#define T64_STOP_REASON_BOUND UINT32_C(1)
#define T64_STOP_REASON_INSTRUCTION_COUNT UINT32_C(2)
#define T64_STOP_REASON_FRAME_EVENT_COUNT UINT32_C(3)
#define T64_STOP_REASON_HALT UINT32_C(4)
#define T64_STOP_REASON_QUIESCENT UINT32_C(5)
#define T64_STOP_REASON_RESOURCE_LIMIT UINT32_C(6)
#define T64_STOP_REASON_CPU_UNSUPPORTED UINT32_C(7)
#define T64_STOP_REASON_GUEST_COMPLETION UINT32_C(8)

typedef uint32_t t64_status;

typedef struct t64_machine t64_machine;

/* Build identity is separate from machine/profile/state identity. */
const char *t64_build_identity(void);
const char *t64_source_revision(void);

typedef struct {
  void *data;
  size_t capacity;
  size_t alignment;
} t64_host_storage;

typedef bool (*t64_host_allocate_fn)(void *context, size_t size,
                                    size_t alignment,
                                    t64_host_storage *storage_out);
typedef void (*t64_host_release_fn)(void *context, t64_host_storage storage);

typedef struct {
  t64_host_allocate_fn allocate;
  t64_host_release_fn release;
  void *context;
} t64_host_allocator;

typedef struct {
  char profile_id[T64_PROFILE_TEXT_CAPACITY];
  char console_target[T64_PROFILE_TEXT_CAPACITY];
  char console_qualification[T64_PROFILE_TEXT_CAPACITY];
  uint32_t tv_standard;
  uint32_t region;
  uint32_t media_kind;
  uint32_t rdram_bytes;
  uint32_t expansion_bytes;
  uint32_t peripheral_flags;
  uint32_t save_kind;
  char boot_route[T64_PROFILE_TEXT_CAPACITY];
  char boot_status[T64_PROFILE_TEXT_CAPACITY];
  char guest_tick_unit[T64_PROFILE_TEXT_CAPACITY];
  uint32_t cpu_rsp_ratio_numerator;
  uint32_t cpu_rsp_ratio_denominator;
  uint32_t synchronization_quantum;
  char timing_tier[T64_PROFILE_TEXT_CAPACITY];
  char scheduler_version[T64_PROFILE_TEXT_CAPACITY];
  char pi_model_version[T64_PROFILE_TEXT_CAPACITY];
  char sp_model_version[T64_PROFILE_TEXT_CAPACITY];
  uint32_t rom_min_bytes;
  uint32_t rom_max_bytes;
  uint32_t event_queue_capacity;
  uint32_t sequence_width_bits;
  uint32_t advance_event_budget;
  uint32_t pi_duration_base_ticks;
  uint32_t pi_duration_bytes_divisor;
  uint32_t sp_duration_base_rsp_cycles;
  uint32_t sp_duration_bytes_divisor;
  char rom_order_policy[T64_PROFILE_TEXT_CAPACITY];
  char event_order_policy[T64_PROFILE_TEXT_CAPACITY];
  char queue_exhaustion_policy[T64_PROFILE_TEXT_CAPACITY];
  char sequence_exhaustion_policy[T64_PROFILE_TEXT_CAPACITY];
  char advance_budget_policy[T64_PROFILE_TEXT_CAPACITY];
  char guest_time_bound_policy[T64_PROFILE_TEXT_CAPACITY];
  char halt_precedence_policy[T64_PROFILE_TEXT_CAPACITY];
  char pi_register_policy[T64_PROFILE_TEXT_CAPACITY];
  char sp_register_policy[T64_PROFILE_TEXT_CAPACITY];
  char device_error_policy[T64_PROFILE_TEXT_CAPACITY];
  char dma_cancel_policy[T64_PROFILE_TEXT_CAPACITY];
  char dma_visibility_policy[T64_PROFILE_TEXT_CAPACITY];
  uint32_t descriptor_version;
  uint32_t machine_model_version;
  uint32_t state_serialization_version;
  /* Version-2 profile identity; legacy descriptor serialization ignores it. */
  uint8_t asset_sha256[T64_DIGEST_SIZE];
  uint8_t payload_sha256[T64_DIGEST_SIZE];
  uint8_t boot_policy_sha256[T64_DIGEST_SIZE];
  uint32_t hle_seed_reset_version;
} t64_profile_descriptor;

typedef struct {
  uint32_t kind;
  uint64_t value;
} t64_stop_request;

typedef struct {
  uint32_t reason;
  uint64_t tick;
} t64_stop_result;

typedef struct {
  uint64_t retired_instructions;
  uint32_t pc;
  uint32_t opcode;
} t64_cpu_stop_detail;

typedef struct {
  uint64_t tick;
  uint32_t device;
  uint32_t kind;
  uint64_t value;
} t64_trace_record;

/* Result of a host copy from the retained per-machine ISViewer prefix. */
typedef struct {
  size_t copied_bytes;
  size_t required_bytes;
  uint64_t total_produced_bytes;
  uint8_t captured_sha256[T64_DIGEST_SIZE];
  bool lost_output;
} t64_output_drain_result;

/* The returned descriptor is immutable and remains valid for program life. */
t64_status t64_profile_lookup(const char *profile_id,
                              const t64_profile_descriptor **profile_out);
t64_status t64_profile_hash(const t64_profile_descriptor *profile,
                            uint8_t digest_out[T64_DIGEST_SIZE]);
/*
 * Output spans must not overlap each other or the source profile. On
 * BUFFER_TOO_SMALL, length_out receives the required size; bytes stay intact.
 */
t64_status t64_profile_serialize(const t64_profile_descriptor *profile,
                                 uint8_t *bytes_out, size_t capacity,
                                 size_t *length_out);

/* Create/destroy and admission are the only operations that call the host. */
t64_status t64_machine_create(const t64_host_allocator *allocator,
                              const t64_profile_descriptor *profile,
                              t64_machine **machine_out);
void t64_machine_destroy(t64_machine *machine);
t64_status t64_machine_admit_rom(t64_machine *machine, const uint8_t *bytes,
                                 size_t length);
t64_status t64_machine_advance(t64_machine *machine,
                               const t64_stop_request *request,
                               t64_stop_result *result_out);
t64_status t64_machine_content_digest(
    const t64_machine *machine, uint8_t digest_out[T64_DIGEST_SIZE]);
/* Read-only admission observations; outputs are unchanged on NOT_READY/refusal. */
t64_status t64_machine_rom_size(const t64_machine *machine,
                                size_t *size_out);
/*
 * Copies the fixed normalized header on success. A NULL/zero-capacity query
 * returns BUFFER_TOO_SMALL and the required 64-byte length. On any
 * BUFFER_TOO_SMALL result the output bytes remain unchanged.
 */
t64_status t64_machine_rom_header(const t64_machine *machine,
                                  uint8_t *header_out, size_t capacity,
                                  size_t *length_out);
/* The returned descriptor is borrowed and valid until machine destruction. */
t64_status t64_machine_profile(
    const t64_machine *machine, const t64_profile_descriptor **profile_out);
t64_status t64_machine_digest(const t64_machine *machine,
                              uint8_t digest_out[T64_DIGEST_SIZE]);
/* Returns NOT_READY until a terminal CPU stop; refusals leave output intact. */
t64_status t64_machine_cpu_stop_detail(
    const t64_machine *machine, t64_cpu_stop_detail *detail_out);

/*
 * Copy newly observed bytes from the retained guest capture. The guest's
 * lifetime capture is never consumed or freed by this observer cursor. Calls
 * must be serialized with advance and other instance access. Result and data
 * spans must be valid, disjoint from each other, and disjoint from all
 * machine-owned storage, including the admitted image and RDRAM. On
 * BUFFER_TOO_SMALL, required_bytes and the capture observations are reported,
 * while copied_bytes is zero and no bytes or cursor are changed. Other
 * refusals leave both caller spans and the machine unchanged. An empty drain
 * succeeds with zero copied bytes. Available after ROM admission, including
 * after a terminal CPU stop; reset or successful image replacement clears
 * both capture and observer cursor.
 */
t64_status t64_machine_output_drain(t64_machine *machine, uint8_t *bytes_out,
                                   size_t capacity,
                                   t64_output_drain_result *result_out);

/*
 * Submit a deterministic port-0 observation event. Buttons are bits 0..15;
 * signed axes are logical 8-bit values in [-128,127]. Events are scheduled
 * and traced in tick/sequence order but are not consumed by SI/PIF or a
 * controller model. Calls must be serialized with advance. Refusals do not
 * change the queue or sequence. Submission is accepted only for an admitted,
 * nonterminal machine and a tick at or after the current guest tick.
 */
t64_status t64_machine_queue_input(t64_machine *machine, uint64_t tick,
                                   uint32_t port, uint16_t buttons,
                                   int32_t axis_x, int32_t axis_y);

/*
 * Host debug access to physical byte addresses, called between advance calls.
 * Width must be 1, 2, or 4. Halfwords and words are big-endian and require
 * natural alignment; individual device registers can further restrict width.
 * Rejected writes have no partial effects, and rejected reads leave value_out
 * unchanged. These calls do not execute guest code or translate virtual
 * addresses.
 */
t64_status t64_machine_bus_read(t64_machine *machine, uint32_t address,
                                uint32_t width, uint32_t *value_out);
t64_status t64_machine_bus_write(t64_machine *machine, uint32_t address,
                                 uint32_t width, uint32_t value);

/* Trace state is observer-only and never changes the canonical digest. */
t64_status t64_machine_trace_set_enabled(t64_machine *machine, bool enabled);
/*
 * Copies up to capacity whole records in FIFO order and reports the
 * cumulative saturated drop observation. A zero capacity permits NULL
 * records_out, copies nothing, and leaves queued records intact. All output
 * parameters are required; invalid outputs leave observer state unchanged.
 */
t64_status t64_machine_trace_drain(
    t64_machine *machine, t64_trace_record *records_out, size_t capacity,
    size_t *count_out, uint64_t *dropped_out,
    bool *drop_count_saturated_out);

#endif /* TENDIES64_T64_MACHINE_H */
