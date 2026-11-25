/*
 * Provisional deterministic machine seam. The public surface is not a frozen
 * ABI. Host callbacks own allocation and release; core execution never calls
 * back into the host.
 */
#ifndef TENDIES64_T64_MACHINE_H
#define TENDIES64_T64_MACHINE_H

/* C++ has bool; C99 needs the header (only function parameters use bool). */
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <stddef.h>
#include <stdint.h>

/*
 * Every public function is marked T64_API: default symbol visibility under
 * GCC and Clang, nothing elsewhere. A host that links the installed archive
 * sees exactly the marked functions.
 */
#if defined(__GNUC__) || defined(__clang__)
#define T64_API __attribute__((visibility("default")))
#else
#define T64_API
#endif

/*
 * ABI version of this header: the major in the upper 16 bits and the minor in
 * the lower 16. T64_ABI_VERSION is the value t64_abi_version() returns for the
 * library built from the same sources, so a host can assert that the header
 * it compiled against and the library it linked agree.
 */
#define T64_ABI_VERSION_MAJOR UINT32_C(0)
#define T64_ABI_VERSION_MINOR UINT32_C(1)
#define T64_ABI_VERSION                                                        \
  ((T64_ABI_VERSION_MAJOR << 16) | T64_ABI_VERSION_MINOR)

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
#define T64_TRACE_DEVICE_RDP UINT32_C(7)
#define T64_TRACE_DEVICE_VI UINT32_C(8)
/* Provisional until Phase 10: serial interface execute and complete events. */
#define T64_TRACE_DEVICE_SI UINT32_C(10)
#define T64_TRACE_KIND_ROM_ADMITTED UINT32_C(1)
#define T64_TRACE_KIND_CPU_ENTRY UINT32_C(2)
#define T64_TRACE_KIND_CPU_RETIREMENT UINT32_C(3)
#define T64_TRACE_KIND_CPU_UNSUPPORTED UINT32_C(4)
#define T64_TRACE_KIND_CPU_COMPLETION UINT32_C(5)
#define T64_TRACE_KIND_CPU_EXCEPTION UINT32_C(6)
#define T64_TRACE_KIND_CPU_EXCEPTION_STATE UINT32_C(7)
#define T64_TRACE_KIND_CPU_EXCEPTION_MI UINT32_C(8)
/* Exception value is EPC; STATE packs post-entry status/cause; MI packs
 * pending/mask. The three records share the exception tick. */
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
#define T64_TRACE_KIND_RDP_COMMAND UINT32_C(1)
#define T64_TRACE_KIND_RDP_COMPLETE UINT32_C(2)
#define T64_TRACE_KIND_VI_FRAME UINT32_C(1)
#define T64_TRACE_KIND_SI_EXECUTE UINT32_C(1)
#define T64_TRACE_KIND_SI_COMPLETE UINT32_C(2)
#define T64_VIDEO_SOURCE_RGBA5551 UINT32_C(2)
#define T64_VIDEO_SOURCE_RGBA8888 UINT32_C(3)
#define T64_VIDEO_ENCODING_RGBA8 UINT32_C(1)
#define T64_VIDEO_FILTER_AA UINT32_C(0x01)
#define T64_VIDEO_FILTER_DITHER_SMOOTH UINT32_C(0x02)
#define T64_VIDEO_FILTER_DIVOT_MEDIAN UINT32_C(0x04)
#define T64_VIDEO_FILTER_RESAMPLE_Q20 UINT32_C(0x08)
#define T64_VIDEO_FILTER_GAMMA_DITHER UINT32_C(0x10)
#define T64_VIDEO_FILTER_GAMMA_SQRT255 UINT32_C(0x20)

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
/* Save device request refusals (plan 09-04); provisional until Phase 10. A
 * profile with no device answers any requested kind with DEVICE_MISMATCH. */
#define T64_STATUS_SAVE_UNKNOWN_DEVICE UINT32_C(20)
#define T64_STATUS_SAVE_UNSUPPORTED_DEVICE UINT32_C(21)
#define T64_STATUS_SAVE_DEVICE_MISMATCH UINT32_C(22)
/* Save import and export refusals (plan 09-05); provisional until Phase 10. */
#define T64_STATUS_SAVE_BAD_FORMAT UINT32_C(23)
#define T64_STATUS_SAVE_SIZE_MISMATCH UINT32_C(24)
#define T64_STATUS_SAVE_BUSY UINT32_C(25)
/* State import refusals (plan 09-09); provisional until Phase 10. Each status
 * names one family of refusal and carries a closed reason in the import
 * report (T64_STATE_REASON_*). */
#define T64_STATUS_STATE_FORMAT UINT32_C(26)
#define T64_STATUS_STATE_INTEGRITY UINT32_C(27)
#define T64_STATUS_STATE_VERSION UINT32_C(28)
#define T64_STATUS_STATE_PROFILE UINT32_C(29)
#define T64_STATUS_STATE_CONTENT UINT32_C(30)
#define T64_STATUS_STATE_BUILD UINT32_C(31)
#define T64_STATUS_STATE_RANGE UINT32_C(32)
#define T64_STATUS_STATE_INVARIANT UINT32_C(33)
/* Appended after 33; no value 0 to 33 moves. The class of each status is in
 * docs/abi/status-classes.txt. */
/* refused: the create-info abi_version does not match this header's ABI. */
#define T64_STATUS_ABI_MISMATCH UINT32_C(34)
/* refused: a caller-filled struct_size is below the v1 minimum. */
#define T64_STATUS_STRUCT_TOO_SMALL UINT32_C(35)
/* refused: a call arrived while a call on the same instance is in progress. */
#define T64_STATUS_REENTRANT UINT32_C(36)
/* defect: the instance is poisoned; only destroy and the poison query work. */
#define T64_STATUS_POISONED UINT32_C(37)
/* defect: an internal invariant violation was found and the instance is
 * now poisoned. */
#define T64_STATUS_INTERNAL_DEFECT UINT32_C(38)

#define T64_TV_NTSC UINT32_C(1)
#define T64_TV_PAL UINT32_C(2)
#define T64_REGION_US UINT32_C(1)
#define T64_REGION_EUROPE UINT32_C(2)
#define T64_MEDIA_CARTRIDGE UINT32_C(1)
#define T64_SAVE_NONE UINT32_C(0)
/* Provisional until the Phase 10 interface review; no API/ABI freeze. */
#define T64_SAVE_EEPROM_4K UINT32_C(1)
#define T64_SAVE_EEPROM_16K UINT32_C(2)
#define T64_SAVE_SRAM_32K UINT32_C(3)
/* Known and unsupported (provisional): requestable through
 * t64_machine_save_info, refused with T64_STATUS_SAVE_UNSUPPORTED_DEVICE, and
 * still refused as a profile save_kind at create. */
#define T64_SAVE_FLASHRAM UINT32_C(4)
#define T64_SAVE_CONTROLLER_PAK UINT32_C(5)
#define T64_SAVE_RUMBLE_PAK UINT32_C(6)
#define T64_SAVE_TRANSFER_PAK UINT32_C(7)
/* Flag of t64_save_info: an EEPROM deadline is in the future at the current
 * guest tick, or an SRAM DMA is in flight. */
#define T64_SAVE_INFO_BUSY UINT32_C(1)
/* Byte-order selections of save import and export (plan 09-05). Authored
 * policy, not a hardware fact; provisional until Phase 10. RAW is the identity;
 * SWAP16 swaps the two bytes inside every 16-bit unit; WORDREV32 reverses the
 * four bytes inside every 32-bit unit. */
#define T64_SAVE_FORMAT_RAW UINT32_C(1)
#define T64_SAVE_FORMAT_SWAP16 UINT32_C(2)
#define T64_SAVE_FORMAT_WORDREV32 UINT32_C(3)
#define T64_PERIPHERAL_STANDARD_CONTROLLER_PORT0 UINT32_C(1)

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
/* Software debug-stop convention; it is not a guest pass or CPU exception. */
#define T64_STOP_REASON_GUEST_COMPLETION UINT32_C(8)

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t t64_status;

typedef struct t64_machine t64_machine;

/* Build identity is separate from machine/profile/state identity. */
T64_API const char *t64_build_identity(void);
T64_API const char *t64_source_revision(void);
/* The ABI version of the library as a uint32_t (T64_ABI_VERSION). */
T64_API uint32_t t64_abi_version(void);

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
  /* Descriptor v5 fields. Legacy descriptor hashes ignore these fields. */
  char rdp_work_policy[T64_PROFILE_TEXT_CAPACITY];
  char vi_clock_policy[T64_PROFILE_TEXT_CAPACITY];
  char rdp_noise_policy[T64_PROFILE_TEXT_CAPACITY];
  char rdp_undefined_color_policy[T64_PROFILE_TEXT_CAPACITY];
  char vi_output_policy[T64_PROFILE_TEXT_CAPACITY];
  /* Descriptor v7 field: raw triangle numeric/raster policy. */
  char rdp_triangle_policy[T64_PROFILE_TEXT_CAPACITY];
  /* Descriptor v8 field: color combiner arithmetic/source policy. */
  char rdp_combiner_policy[T64_PROFILE_TEXT_CAPACITY];
  /* Descriptor v10 field: depth mapping/comparison/update policy. */
  char rdp_depth_policy[T64_PROFILE_TEXT_CAPACITY];
  /* Descriptor v11 field: deterministic AI DMA/sample policy. */
  char ai_policy[T64_PROFILE_TEXT_CAPACITY];
  /* Descriptor v12 fields: SI/joybus wire policy and explicit save-media
   * policy. Older descriptor hashes ignore them. Provisional until Phase 10. */
  char si_policy[T64_PROFILE_TEXT_CAPACITY];
  char save_policy[T64_PROFILE_TEXT_CAPACITY];
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
  uint64_t total_attempted_bytes;
  uint8_t captured_sha256[T64_DIGEST_SIZE];
  bool lost_output;
  bool total_attempted_bytes_exact;
} t64_output_drain_result;

/* One queued raw VI packet. Strings and numeric policy fields are part of the
 * packet observation, not a claim about measured hardware behavior. */
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
} t64_video_packet_info;

/* One retained raw stereo PCM packet produced by AI DMA. */
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
} t64_audio_packet_info;

#define T64_AUDIO_ENCODING_S16BE UINT32_C(1)

/* The returned descriptor is immutable and remains valid for program life. */
T64_API t64_status t64_profile_lookup(const char *profile_id,
                                      const t64_profile_descriptor **profile_out);
T64_API t64_status t64_profile_hash(const t64_profile_descriptor *profile,
                                    uint8_t digest_out[T64_DIGEST_SIZE]);
/*
 * Output spans must not overlap each other or the source profile. On
 * BUFFER_TOO_SMALL, length_out receives the required size; bytes stay intact.
 */
T64_API t64_status t64_profile_serialize(const t64_profile_descriptor *profile,
                                         uint8_t *bytes_out, size_t capacity,
                                         size_t *length_out);

/* Create/destroy and admission are the only operations that call the host. */
T64_API t64_status t64_machine_create(const t64_host_allocator *allocator,
                                      const t64_profile_descriptor *profile,
                                      t64_machine **machine_out);
T64_API void t64_machine_destroy(t64_machine *machine);
/*
 * Defect class and poison query (plan 11-03). A call that finds an internal
 * invariant of the core broken returns T64_STATUS_INTERNAL_DEFECT and poisons
 * the instance; a caller fault never poisons. On a poisoned instance every
 * entry that takes a machine except t64_machine_destroy and this query returns
 * T64_STATUS_POISONED and leaves every output buffer and out-struct exactly as
 * the caller left it, and t64_machine_destroy still releases every host
 * allocation. A NULL machine is T64_STATUS_INVALID_ARGUMENT with every output
 * untouched on each entry that takes a machine, and t64_machine_destroy(NULL)
 * does nothing. A call made on an instance from inside one of its own host
 * allocator callbacks (admission and destruction call the host) is refused with
 * T64_STATUS_REENTRANT before it reads or writes anything. One instance still
 * needs external serialization; distinct instances are independent.
 *
 * This query reports 1 in *poisoned_out for a poisoned instance and 0
 * otherwise. It is the one entry other than destroy with no poison or
 * reentrancy guard; a NULL machine, a NULL poisoned_out or an output span that
 * overlaps the instance is T64_STATUS_INVALID_ARGUMENT with *poisoned_out
 * untouched.
 */
T64_API t64_status t64_machine_is_poisoned(const t64_machine *machine,
                                           uint32_t *poisoned_out);
T64_API t64_status t64_machine_admit_rom(t64_machine *machine, const uint8_t *bytes,
                                         size_t length);
T64_API t64_status t64_machine_advance(t64_machine *machine,
                                       const t64_stop_request *request,
                                       t64_stop_result *result_out);
T64_API t64_status t64_machine_content_digest(
    const t64_machine *machine, uint8_t digest_out[T64_DIGEST_SIZE]);
/* Read-only admission observations; outputs are unchanged on NOT_READY/refusal. */
T64_API t64_status t64_machine_rom_size(const t64_machine *machine,
                                        size_t *size_out);
/*
 * Copies the fixed normalized header on success. A NULL/zero-capacity query
 * returns BUFFER_TOO_SMALL and the required 64-byte length. On any
 * BUFFER_TOO_SMALL result the output bytes remain unchanged.
 */
T64_API t64_status t64_machine_rom_header(const t64_machine *machine,
                                          uint8_t *header_out, size_t capacity,
                                          size_t *length_out);
/* The returned descriptor is borrowed and valid until machine destruction. */
T64_API t64_status t64_machine_profile(
    const t64_machine *machine, const t64_profile_descriptor **profile_out);
T64_API t64_status t64_machine_digest(const t64_machine *machine,
                                      uint8_t digest_out[T64_DIGEST_SIZE]);

/*
 * State export (provisional until Phase 10: the name, the container layout
 * and the format string are not frozen and no API or ABI freeze is implied).
 * The container is the versioned binary blob described in
 * docs/spec/state-container.md; it is an integrity-checked copy of the
 * canonical machine state plus the host observer cursors, not an
 * authenticated or portable artifact. Export is defined for the composite
 * family: state serialization version 25 (container version 1) and, since plan
 * 10-11, version 26 (container version 2, the si-v2 profiles).
 *
 * Size-then-copy-out: a NULL buffer or a capacity below the required length
 * returns BUFFER_TOO_SMALL, sets *length_out to the required length and
 * writes no byte of bytes_out. An exact or larger buffer receives the
 * container, *length_out is its length and no byte past it changes. The
 * machine is never modified (const). Argument spans must be valid and must
 * not overlap each other or any machine-owned storage (INVALID_ARGUMENT); no
 * admitted image is NOT_READY; a profile outside the composite family is
 * INVALID_ARGUMENT; a container beyond the size ceiling is RESOURCE_EXHAUSTED.
 * Every refusal leaves *length_out and the machine intact and leaves the caller
 * bytes intact except one case: an internal inconsistency found while writing
 * (BAD_STORAGE, unreachable for a const machine) clears the first total bytes of
 * the buffer, so a defect never publishes a partial container.
 */
#define T64_STATE_CONTAINER_FORMAT "t64.state-container.v1"
T64_API t64_status t64_machine_state_export(const t64_machine *machine,
                                            uint8_t *bytes_out, size_t capacity,
                                            size_t *length_out);
/*
 * The public name of container version 2 (plan 10-15, CORE-06), beside
 * T64_STATE_CONTAINER_FORMAT above: a state serialization version 26 machine
 * (the si-v2 profiles) writes container version word 2. This is a state-format
 * constant, outside the ABI promise: a state blob is accepted only by the build
 * identity and profile that wrote it (docs/spec/state-container.md, "Stability"),
 * and the string is not public API work (the API is Phase 11). The runner pairs
 * the string with the profile at prelaunch: the v1 string with state 25 and the
 * v2 string with state 26; every other pair is refused at guest tick 0.
 */
#define T64_STATE_CONTAINER_FORMAT_V2 "t64.state-container.v2"

/*
 * State import (plan 09-09; provisional until Phase 10, no API or ABI freeze).
 * Import accepts a container produced by the same build for the same version,
 * profile and content. The contract is memory safety plus liveness: a blob
 * that passes validation cannot cause an out-of-range access, an unbounded
 * loop or undefined arithmetic, and a state that is valid but semantically
 * unreachable is accepted and then runs deterministically. The hashes inside
 * the container are integrity checks, not authentication, and no statement is
 * made that an imported state is reachable.
 *
 * Order: arguments and disjointness from live storage; header; whole-blob
 * SHA-256 trailer; identity binding (a distinct status per mismatch); section
 * table; per-field range rules; structural and per-device invariants; the
 * canonical digest recomputed from the blob bytes against the declared digest;
 * then one infallible commit. A refusal leaves the machine bit-identical, makes
 * no host call and releases nothing. The report is a caller record that never
 * holds blob bytes; it may be NULL.
 *
 * Observers: by default the video and audio ring occupancy and the output
 * drain cursor of the blob are restored. T64_STATE_IMPORT_DROP_OBSERVERS sets
 * both ring occupancies to empty and the drain cursor to the retained capture
 * length, so no pre-import byte is re-delivered; the canonical digest is the
 * same either way.
 */
#define T64_STATE_IMPORT_DROP_OBSERVERS UINT32_C(0x1)

/* Closed reasons, in check order. */
#define T64_STATE_REASON_NONE UINT32_C(0)
#define T64_STATE_REASON_ARG_NULL UINT32_C(1)
#define T64_STATE_REASON_ARG_LENGTH UINT32_C(2)
#define T64_STATE_REASON_ARG_FLAGS UINT32_C(3)
#define T64_STATE_REASON_ARG_OVERLAP_LIVE UINT32_C(4)
#define T64_STATE_REASON_NOT_ADMITTED UINT32_C(5)
#define T64_STATE_REASON_MAGIC UINT32_C(6)
#define T64_STATE_REASON_CONTAINER_VERSION UINT32_C(7)
#define T64_STATE_REASON_TOTAL_LENGTH UINT32_C(8)
#define T64_STATE_REASON_SECTION_COUNT UINT32_C(9)
#define T64_STATE_REASON_HEADER_FLAGS UINT32_C(10)
#define T64_STATE_REASON_HEADER_RESERVED UINT32_C(11)
#define T64_STATE_REASON_TRAILER_SHA256 UINT32_C(12)
#define T64_STATE_REASON_STATE_VERSION UINT32_C(13)
#define T64_STATE_REASON_PROFILE_DIGEST UINT32_C(14)
#define T64_STATE_REASON_CONTENT_DIGEST UINT32_C(15)
#define T64_STATE_REASON_CONTENT_LENGTH UINT32_C(16)
#define T64_STATE_REASON_BUILD_ID UINT32_C(17)
#define T64_STATE_REASON_SECTION_ID_UNKNOWN UINT32_C(18)
#define T64_STATE_REASON_SECTION_ORDER UINT32_C(19)
#define T64_STATE_REASON_SECTION_FLAGS UINT32_C(20)
#define T64_STATE_REASON_SECTION_BOUNDS UINT32_C(21)
#define T64_STATE_REASON_SECTION_OVERLAP UINT32_C(22)
#define T64_STATE_REASON_SECTION_LENGTH_EXPECTED UINT32_C(23)
#define T64_STATE_REASON_SECTION_SHA256 UINT32_C(24)
#define T64_STATE_REASON_SECTION_MISSING UINT32_C(25)
#define T64_STATE_REASON_FIELD_RANGE UINT32_C(26)
#define T64_STATE_REASON_FIELD_PADDING UINT32_C(27)
#define T64_STATE_REASON_FIELD_RESERVED UINT32_C(28)
#define T64_STATE_REASON_FIELD_DERIVED UINT32_C(29)
#define T64_STATE_REASON_QUEUE_COUNT UINT32_C(30)
#define T64_STATE_REASON_QUEUE_ORDER UINT32_C(31)
#define T64_STATE_REASON_QUEUE_SEQUENCE UINT32_C(32)
#define T64_STATE_REASON_QUEUE_TICK UINT32_C(33)
#define T64_STATE_REASON_QUEUE_KIND_UNKNOWN UINT32_C(34)
#define T64_STATE_REASON_DEVICE_EVENT_MISSING UINT32_C(35)
#define T64_STATE_REASON_DEVICE_EVENT_EXTRA UINT32_C(36)
#define T64_STATE_REASON_DEVICE_TOKEN_MISMATCH UINT32_C(37)
#define T64_STATE_REASON_DEVICE_STATE_INCONSISTENT UINT32_C(38)
#define T64_STATE_REASON_DECLARED_DIGEST UINT32_C(39)
#define T64_STATE_REASON_OBSERVER_RANGE UINT32_C(40)

/* Closed rules, recorded for the field-level reasons. */
#define T64_STATE_RULE_NONE UINT32_C(0)
#define T64_STATE_RULE_COUNT_CAP UINT32_C(1)
#define T64_STATE_RULE_INDEX_LT UINT32_C(2)
#define T64_STATE_RULE_ENUM_CLOSED UINT32_C(3)
#define T64_STATE_RULE_BOOL_01 UINT32_C(4)
#define T64_STATE_RULE_ALIGN UINT32_C(5)
#define T64_STATE_RULE_ADDR_RANGE UINT32_C(6)
#define T64_STATE_RULE_TICK_GE UINT32_C(7)
#define T64_STATE_RULE_TICK_LE UINT32_C(8)
#define T64_STATE_RULE_SEQ_LT UINT32_C(9)
#define T64_STATE_RULE_SHIFT_LT UINT32_C(10)
#define T64_STATE_RULE_LEN_LE UINT32_C(11)
#define T64_STATE_RULE_PROFILE_DERIVED UINT32_C(12)
#define T64_STATE_RULE_DERIVED_EQ UINT32_C(13)
#define T64_STATE_RULE_PAD_ZERO UINT32_C(14)
#define T64_STATE_RULE_RESERVED_ZERO UINT32_C(15)
#define T64_STATE_RULE_MASKED_AT_USE UINT32_C(16)

/* Caller record of one import attempt: the status, the first failing reason,
 * the section id and rule where they apply and a position (a byte position in
 * the blob or a member ordinal). It never carries blob bytes. */
typedef struct {
  t64_status status;
  uint32_t reason;
  uint32_t section_id;
  uint32_t rule;
  uint64_t position;
} t64_state_import_report;

T64_API t64_status t64_machine_state_import(t64_machine *machine,
                                            const uint8_t *bytes, size_t length,
                                            uint32_t flags,
                                            t64_state_import_report *report_out);

/* Returns NOT_READY until a terminal CPU stop; refusals leave output intact. */
T64_API t64_status t64_machine_cpu_stop_detail(
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
T64_API t64_status t64_machine_output_drain(t64_machine *machine, uint8_t *bytes_out,
                                           size_t capacity,
                                           t64_output_drain_result *result_out);

/*
 * Raw video is a bounded, lossless, two-packet queue on the RDP/VI profile.
 * info reports the oldest packet. copy with NULL/zero queries its size and
 * leaves the queue untouched; a successful full copy dequeues that packet.
 * Refused or undersized copies leave both output bytes and queue unchanged.
 */
T64_API t64_status t64_video_info(const t64_machine *machine,
                                  t64_video_packet_info *info_out);
T64_API t64_status t64_video_copy(t64_machine *machine, uint8_t *bytes_out,
                                  size_t capacity, size_t *length_out);

/*
 * Save device information (provisional until Phase 10; no API/ABI freeze).
 * kind is the profile's device kind, size_bytes its storage size and flags
 * T64_SAVE_INFO_BUSY while a write window or a DMA is in flight.
 * write_generation counts successful save imports and successful state imports
 * into a profile with a save device since creation or the last replacement
 * (observer only: it is outside the canonical digest and the state container,
 * guest writes do not increment it, it saturates at the 64-bit maximum and a
 * host that compares a remembered value must also track its admissions, because
 * replacement restarts it at 0).
 */
typedef struct {
  uint32_t kind;
  uint32_t size_bytes;
  uint32_t flags;
  uint64_t write_generation;
} t64_save_info;

/*
 * Reports the save device of an admitted machine when the requested kind is
 * the profile's kind. The check order is exact: argument pointers
 * (INVALID_ARGUMENT, info untouched), admitted (NOT_READY), the profile has a
 * device (a profile with none answers T64_STATUS_SAVE_DEVICE_MISMATCH for any
 * kind), the kind is known (0 and values above 7 answer
 * T64_STATUS_SAVE_UNKNOWN_DEVICE), the kind is supported (4 to 7 answer
 * T64_STATUS_SAVE_UNSUPPORTED_DEVICE) and equals the profile kind (otherwise
 * T64_STATUS_SAVE_DEVICE_MISMATCH). Every refusal leaves the machine and the
 * output unchanged. The device is selected by the profile only: no header,
 * title, game code, checksum or file name is an input.
 */
T64_API t64_status t64_machine_save_info(const t64_machine *machine,
                                         uint32_t requested_kind,
                                         t64_save_info *info_out);

/*
 * Host import and export of the save device bytes (plan 09-05; provisional
 * until Phase 10, no API/ABI freeze). Both move exactly the kind's size
 * (512, 2048 or 32,768 bytes, no padding, no container, no autodetect) in the
 * named byte order; the core never opens a file or loads or flushes a save on
 * its own. The check order is exact and every check precedes any read of
 * caller bytes and any mutation, so a refusal changes nothing:
 *   R0 machine NULL                          T64_STATUS_INVALID_ARGUMENT
 *   R1 no admitted image                     T64_STATUS_NOT_READY
 *   R2 pointer or span (import: bytes NULL, span overflow, span overlapping
 *      machine storage; export: length_out NULL, bytes_out NULL with a
 *      non-zero capacity, span overlapping machine storage or length_out)
 *                                             T64_STATUS_INVALID_ARGUMENT
 *   R3 the profile has no save device        T64_STATUS_SAVE_DEVICE_MISMATCH
 *   R4 kind not in 1 to 7                    T64_STATUS_SAVE_UNKNOWN_DEVICE
 *   R5 kind in 4 to 7                        T64_STATUS_SAVE_UNSUPPORTED_DEVICE
 *   R6 kind in 1 to 3 but not the profile's  T64_STATUS_SAVE_DEVICE_MISMATCH
 *   R7 format not in 1 to 3                  T64_STATUS_SAVE_BAD_FORMAT
 *   R8 import length not the kind size       T64_STATUS_SAVE_SIZE_MISMATCH
 *      export capacity below the kind size   T64_STATUS_BUFFER_TOO_SMALL with
 *      *length_out set to the size and the buffer untouched
 *   R9 import only: an EEPROM write window is open or an SRAM DMA is active
 *                                             T64_STATUS_SAVE_BUSY
 * Export has no R9 and always returns the committed bytes. A successful import
 * stores the normalized bytes, clears the EEPROM deadline, leaves the
 * scheduler queue and the guest tick unchanged and increments write_generation
 * (outside the digest). Replacement (t64_machine_admit_rom on an admitted
 * machine) restores blank save content, clears the deadline and resets the
 * generation.
 */
T64_API t64_status t64_machine_save_export(const t64_machine *machine,
                                           uint32_t device_kind, uint32_t format,
                                           uint8_t *bytes_out, size_t capacity,
                                           size_t *length_out);
T64_API t64_status t64_machine_save_import(t64_machine *machine, uint32_t device_kind,
                                           uint32_t format, const uint8_t *bytes,
                                           size_t length);

/* Audio follows the same bounded, lossless two-packet copy/dequeue contract. */
T64_API t64_status t64_audio_info(const t64_machine *machine,
                                  t64_audio_packet_info *info_out);
T64_API t64_status t64_audio_copy(t64_machine *machine, uint8_t *bytes_out,
                                  size_t capacity, size_t *length_out);

/*
 * Submit a deterministic port-0 input event. Buttons are bits 0..15; signed
 * axes are logical 8-bit values in [-128,127]. Events are scheduled and traced
 * in tick/sequence order. Legacy profiles do not consume them; the composite
 * family latches the value so a poll sampled at tick E sees the last input
 * stamped strictly before E (provisional until Phase 10). Calls must be
 * serialized with advance. Refusals do not change the queue or sequence.
 * Submission is accepted only for an admitted, nonterminal machine and a tick
 * at or after the current guest tick.
 */
T64_API t64_status t64_machine_queue_input(t64_machine *machine, uint64_t tick,
                                           uint32_t port, uint16_t buttons,
                                           int32_t axis_x, int32_t axis_y);

/*
 * Host debug access to physical byte addresses, called between advance calls.
 * Width must be 1, 2, or 4. Halfwords and words are big-endian and require
 * natural alignment; individual device registers can further restrict width.
 * A write that the bus rejects for its address, width or alignment has no
 * effect; a write that a device refuses can leave the device's own record of
 * the refusal (the PI latches its length and sets its sticky error on a busy
 * start, the SI sets its sticky DMA error and writes a refusal trace record),
 * and rejected reads leave value_out unchanged. These calls do not execute guest
 * code or translate virtual addresses.
 */
T64_API t64_status t64_machine_bus_read(t64_machine *machine, uint32_t address,
                                        uint32_t width, uint32_t *value_out);
T64_API t64_status t64_machine_bus_write(t64_machine *machine, uint32_t address,
                                         uint32_t width, uint32_t value);

/* Trace state is observer-only and never changes the canonical digest. */
T64_API t64_status t64_machine_trace_set_enabled(t64_machine *machine, bool enabled);
/*
 * Copies up to capacity whole records in FIFO order and reports the
 * cumulative saturated drop observation. A zero capacity permits NULL
 * records_out, copies nothing, and leaves queued records intact. All output
 * parameters are required; invalid outputs leave observer state unchanged.
 */
T64_API t64_status t64_machine_trace_drain(
    t64_machine *machine, t64_trace_record *records_out, size_t capacity,
    size_t *count_out, uint64_t *dropped_out,
    bool *drop_count_saturated_out);

#ifdef __cplusplus
}
#endif

#endif /* TENDIES64_T64_MACHINE_H */
