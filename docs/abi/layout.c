/*
 * GENERATED FILE: do not edit. Written by
 *   python3 -B scripts/check-abi.py layout --write docs/abi/layout.c
 * from the public header tendies64/t64_machine.h.
 *
 * Freeze-candidate record of the public layout (Phase 11, plan 11-17, API-03):
 * sizeof of every public struct, offsetof of every member, the value of every
 * integer T64_* constant and the size of every string constant, as compile-time
 * assertions. The file is compiled with -fsyntax-only against the source
 * include directory (CTest repo.abi-layout) and against the installed include
 * directory (CTest install.abi-layout) as C99, C11 and C++17, and regenerated
 * and diffed on every run, so a changed, added or removed member or constant
 * fails. It is not part of any build target. Re-bind with the command above in
 * a commit that also records the amendment; the pins hold for targets with
 * 64-bit pointers (macOS arm64, Linux x86_64), other data models compile it
 * without checking anything.
 */
#include <stddef.h>
#include <stdint.h>

#include <tendies64/t64_machine.h>

#define T64_LAYOUT_JOIN2(a, b) a##b
#define T64_LAYOUT_JOIN(a, b) T64_LAYOUT_JOIN2(a, b)
#if defined(__cplusplus)
#define T64_LAYOUT_PIN(condition) static_assert(condition, #condition)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define T64_LAYOUT_PIN(condition) _Static_assert(condition, #condition)
#else
/* C99 has no static assertion: a negative array size is the portable form. */
#define T64_LAYOUT_PIN(condition) \
  typedef char T64_LAYOUT_JOIN(t64_layout_pin_, __LINE__)[(condition) ? 1 : -1]
#endif

#if UINTPTR_MAX == 0xffffffffffffffffu

/* t64_host_storage */
T64_LAYOUT_PIN(sizeof(t64_host_storage) == 32u);
T64_LAYOUT_PIN(offsetof(t64_host_storage, struct_size) == 0u);
T64_LAYOUT_PIN(offsetof(t64_host_storage, data) == 8u);
T64_LAYOUT_PIN(offsetof(t64_host_storage, capacity) == 16u);
T64_LAYOUT_PIN(offsetof(t64_host_storage, alignment) == 24u);

/* t64_host_allocator */
T64_LAYOUT_PIN(sizeof(t64_host_allocator) == 32u);
T64_LAYOUT_PIN(offsetof(t64_host_allocator, struct_size) == 0u);
T64_LAYOUT_PIN(offsetof(t64_host_allocator, allocate) == 8u);
T64_LAYOUT_PIN(offsetof(t64_host_allocator, release) == 16u);
T64_LAYOUT_PIN(offsetof(t64_host_allocator, context) == 24u);

/* t64_profile_descriptor */
T64_LAYOUT_PIN(sizeof(t64_profile_descriptor) == 2304u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, struct_size) == 0u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, profile_id) == 4u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, console_target) == 68u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, console_qualification) == 132u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, tv_standard) == 196u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, region) == 200u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, media_kind) == 204u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, rdram_bytes) == 208u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, expansion_bytes) == 212u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, peripheral_flags) == 216u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, save_kind) == 220u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, boot_route) == 224u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, boot_status) == 288u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, guest_tick_unit) == 352u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, cpu_rsp_ratio_numerator) == 416u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, cpu_rsp_ratio_denominator) == 420u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, synchronization_quantum) == 424u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, timing_tier) == 428u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, scheduler_version) == 492u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, pi_model_version) == 556u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, sp_model_version) == 620u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, rom_min_bytes) == 684u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, rom_max_bytes) == 688u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, event_queue_capacity) == 692u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, sequence_width_bits) == 696u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, advance_event_budget) == 700u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, pi_duration_base_ticks) == 704u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, pi_duration_bytes_divisor) == 708u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, sp_duration_base_rsp_cycles) == 712u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, sp_duration_bytes_divisor) == 716u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, rom_order_policy) == 720u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, event_order_policy) == 784u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, queue_exhaustion_policy) == 848u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, sequence_exhaustion_policy) == 912u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, advance_budget_policy) == 976u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, guest_time_bound_policy) == 1040u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, halt_precedence_policy) == 1104u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, pi_register_policy) == 1168u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, sp_register_policy) == 1232u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, device_error_policy) == 1296u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, dma_cancel_policy) == 1360u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, dma_visibility_policy) == 1424u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, rdp_work_policy) == 1488u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, vi_clock_policy) == 1552u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, rdp_noise_policy) == 1616u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, rdp_undefined_color_policy) == 1680u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, vi_output_policy) == 1744u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, rdp_triangle_policy) == 1808u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, rdp_combiner_policy) == 1872u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, rdp_depth_policy) == 1936u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, ai_policy) == 2000u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, si_policy) == 2064u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, save_policy) == 2128u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, descriptor_version) == 2192u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, machine_model_version) == 2196u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, state_serialization_version) == 2200u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, asset_sha256) == 2204u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, payload_sha256) == 2236u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, boot_policy_sha256) == 2268u);
T64_LAYOUT_PIN(offsetof(t64_profile_descriptor, hle_seed_reset_version) == 2300u);

/* t64_stop_request */
T64_LAYOUT_PIN(sizeof(t64_stop_request) == 16u);
T64_LAYOUT_PIN(offsetof(t64_stop_request, struct_size) == 0u);
T64_LAYOUT_PIN(offsetof(t64_stop_request, kind) == 4u);
T64_LAYOUT_PIN(offsetof(t64_stop_request, value) == 8u);

/* t64_stop_result */
T64_LAYOUT_PIN(sizeof(t64_stop_result) == 16u);
T64_LAYOUT_PIN(offsetof(t64_stop_result, struct_size) == 0u);
T64_LAYOUT_PIN(offsetof(t64_stop_result, reason) == 4u);
T64_LAYOUT_PIN(offsetof(t64_stop_result, tick) == 8u);

/* t64_cpu_stop_detail */
T64_LAYOUT_PIN(sizeof(t64_cpu_stop_detail) == 24u);
T64_LAYOUT_PIN(offsetof(t64_cpu_stop_detail, struct_size) == 0u);
T64_LAYOUT_PIN(offsetof(t64_cpu_stop_detail, retired_instructions) == 8u);
T64_LAYOUT_PIN(offsetof(t64_cpu_stop_detail, pc) == 16u);
T64_LAYOUT_PIN(offsetof(t64_cpu_stop_detail, opcode) == 20u);

/* t64_trace_record */
T64_LAYOUT_PIN(sizeof(t64_trace_record) == 24u);
T64_LAYOUT_PIN(offsetof(t64_trace_record, tick) == 0u);
T64_LAYOUT_PIN(offsetof(t64_trace_record, device) == 8u);
T64_LAYOUT_PIN(offsetof(t64_trace_record, kind) == 12u);
T64_LAYOUT_PIN(offsetof(t64_trace_record, value) == 16u);

/* t64_output_drain_result */
T64_LAYOUT_PIN(sizeof(t64_output_drain_result) == 72u);
T64_LAYOUT_PIN(offsetof(t64_output_drain_result, struct_size) == 0u);
T64_LAYOUT_PIN(offsetof(t64_output_drain_result, copied_bytes) == 8u);
T64_LAYOUT_PIN(offsetof(t64_output_drain_result, required_bytes) == 16u);
T64_LAYOUT_PIN(offsetof(t64_output_drain_result, total_attempted_bytes) == 24u);
T64_LAYOUT_PIN(offsetof(t64_output_drain_result, captured_sha256) == 32u);
T64_LAYOUT_PIN(offsetof(t64_output_drain_result, lost_output) == 64u);
T64_LAYOUT_PIN(offsetof(t64_output_drain_result, total_attempted_bytes_exact) == 68u);

/* t64_output_status */
T64_LAYOUT_PIN(sizeof(t64_output_status) == 48u);
T64_LAYOUT_PIN(offsetof(t64_output_status, struct_size) == 0u);
T64_LAYOUT_PIN(offsetof(t64_output_status, video_queued) == 4u);
T64_LAYOUT_PIN(offsetof(t64_output_status, video_free) == 8u);
T64_LAYOUT_PIN(offsetof(t64_output_status, reserved0) == 12u);
T64_LAYOUT_PIN(offsetof(t64_output_status, video_next_bytes) == 16u);
T64_LAYOUT_PIN(offsetof(t64_output_status, audio_queued) == 24u);
T64_LAYOUT_PIN(offsetof(t64_output_status, audio_free) == 28u);
T64_LAYOUT_PIN(offsetof(t64_output_status, audio_next_bytes) == 32u);
T64_LAYOUT_PIN(offsetof(t64_output_status, text_pending_bytes) == 40u);

/* t64_video_packet_info */
T64_LAYOUT_PIN(sizeof(t64_video_packet_info) == 488u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, struct_size) == 0u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, width) == 4u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, height) == 8u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, stride_bytes) == 12u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, source_address) == 16u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, source_format) == 20u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, output_encoding) == 24u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, field) == 28u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, parity) == 32u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, control) == 36u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, burst) == 40u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, h_sync) == 44u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, v_sync) == 48u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, h_start) == 52u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, h_end) == 56u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, v_start) == 60u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, v_end) == 64u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, source_width) == 68u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, source_pitch_bytes) == 72u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, source_x_offset) == 76u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, source_y_offset) == 80u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, x_scale) == 84u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, y_scale) == 88u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, filter) == 92u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, start_tick) == 96u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, end_tick) == 104u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, sequence) == 112u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, commands_completed) == 120u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, fullsync_completed) == 128u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, last_command_word) == 136u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, last_command_byte) == 144u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, last_opcode_field) == 148u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, source_bytes) == 152u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, packet_bytes) == 160u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, profile_id) == 168u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, build_id) == 232u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, vi_clock_policy) == 360u);
T64_LAYOUT_PIN(offsetof(t64_video_packet_info, vi_output_policy) == 424u);

/* t64_audio_packet_info */
T64_LAYOUT_PIN(sizeof(t64_audio_packet_info) == 280u);
T64_LAYOUT_PIN(offsetof(t64_audio_packet_info, struct_size) == 0u);
T64_LAYOUT_PIN(offsetof(t64_audio_packet_info, sequence) == 8u);
T64_LAYOUT_PIN(offsetof(t64_audio_packet_info, first_sample_index) == 16u);
T64_LAYOUT_PIN(offsetof(t64_audio_packet_info, first_tick) == 24u);
T64_LAYOUT_PIN(offsetof(t64_audio_packet_info, last_tick) == 32u);
T64_LAYOUT_PIN(offsetof(t64_audio_packet_info, frame_count) == 40u);
T64_LAYOUT_PIN(offsetof(t64_audio_packet_info, rate_numerator) == 48u);
T64_LAYOUT_PIN(offsetof(t64_audio_packet_info, rate_denominator) == 56u);
T64_LAYOUT_PIN(offsetof(t64_audio_packet_info, channels) == 64u);
T64_LAYOUT_PIN(offsetof(t64_audio_packet_info, bits_per_sample) == 68u);
T64_LAYOUT_PIN(offsetof(t64_audio_packet_info, encoding) == 72u);
T64_LAYOUT_PIN(offsetof(t64_audio_packet_info, byte_length) == 80u);
T64_LAYOUT_PIN(offsetof(t64_audio_packet_info, profile_id) == 88u);
T64_LAYOUT_PIN(offsetof(t64_audio_packet_info, build_id) == 152u);

/* t64_machine_create_info */
T64_LAYOUT_PIN(sizeof(t64_machine_create_info) == 24u);
T64_LAYOUT_PIN(offsetof(t64_machine_create_info, struct_size) == 0u);
T64_LAYOUT_PIN(offsetof(t64_machine_create_info, abi_version) == 4u);
T64_LAYOUT_PIN(offsetof(t64_machine_create_info, allocator) == 8u);
T64_LAYOUT_PIN(offsetof(t64_machine_create_info, profile) == 16u);

/* t64_state_import_report */
T64_LAYOUT_PIN(sizeof(t64_state_import_report) == 32u);
T64_LAYOUT_PIN(offsetof(t64_state_import_report, struct_size) == 0u);
T64_LAYOUT_PIN(offsetof(t64_state_import_report, status) == 4u);
T64_LAYOUT_PIN(offsetof(t64_state_import_report, reason) == 8u);
T64_LAYOUT_PIN(offsetof(t64_state_import_report, section_id) == 12u);
T64_LAYOUT_PIN(offsetof(t64_state_import_report, rule) == 16u);
T64_LAYOUT_PIN(offsetof(t64_state_import_report, position) == 24u);

/* t64_save_info */
T64_LAYOUT_PIN(sizeof(t64_save_info) == 24u);
T64_LAYOUT_PIN(offsetof(t64_save_info, struct_size) == 0u);
T64_LAYOUT_PIN(offsetof(t64_save_info, kind) == 4u);
T64_LAYOUT_PIN(offsetof(t64_save_info, size_bytes) == 8u);
T64_LAYOUT_PIN(offsetof(t64_save_info, flags) == 12u);
T64_LAYOUT_PIN(offsetof(t64_save_info, write_generation) == 16u);

/* constants: values of the integer T64_* defines, size of the string ones */
T64_LAYOUT_PIN((long long)(T64_ABI_VERSION_MAJOR) == 0ll);
T64_LAYOUT_PIN((long long)(T64_ABI_VERSION_MINOR) == 1ll);
T64_LAYOUT_PIN((long long)(T64_ABI_VERSION) == 1ll);
T64_LAYOUT_PIN((long long)(T64_DIGEST_SIZE) == 32ll);
T64_LAYOUT_PIN((long long)(T64_ROM_HEADER_SIZE) == 64ll);
T64_LAYOUT_PIN((long long)(T64_PROFILE_TEXT_CAPACITY) == 64ll);
T64_LAYOUT_PIN((long long)(T64_PROFILE_SERIALIZED_MAX_BYTES) == 2048ll);
T64_LAYOUT_PIN((long long)(T64_PROFILE_SERIALIZATION_VERSION) == 1ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_RECORD_CAPACITY) == 256ll);
T64_LAYOUT_PIN((long long)(T64_INPUT_AXIS_MIN) == -128ll);
T64_LAYOUT_PIN((long long)(T64_INPUT_AXIS_MAX) == 127ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_DEVICE_MACHINE) == 0ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_DEVICE_BUS) == 1ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_DEVICE_MI) == 2ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_DEVICE_PI) == 3ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_DEVICE_SP) == 4ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_DEVICE_INPUT) == 6ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_DEVICE_RDP) == 7ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_DEVICE_VI) == 8ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_DEVICE_SI) == 10ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_ROM_ADMITTED) == 1ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_CPU_ENTRY) == 2ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_CPU_RETIREMENT) == 3ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_CPU_UNSUPPORTED) == 4ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_CPU_COMPLETION) == 5ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_CPU_EXCEPTION) == 6ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_CPU_EXCEPTION_STATE) == 7ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_CPU_EXCEPTION_MI) == 8ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_BUS_READ8) == 1ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_BUS_WRITE8) == 2ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_BUS_READ32) == 3ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_BUS_WRITE32) == 4ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_MI_PENDING) == 1ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_MI_MASK) == 2ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_PI_ISSUE) == 1ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_PI_REFUSED) == 2ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_PI_CANCEL) == 3ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_PI_COMPLETE) == 4ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_SP_ISSUE) == 1ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_SP_REFUSED) == 2ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_SP_PROMOTE) == 3ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_SP_DROP) == 4ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_SP_COMPLETE) == 5ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_INPUT_SUBMITTED) == 1ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_RDP_COMMAND) == 1ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_RDP_COMPLETE) == 2ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_VI_FRAME) == 1ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_SI_EXECUTE) == 1ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_SI_COMPLETE) == 2ll);
T64_LAYOUT_PIN((long long)(T64_TRACE_KIND_SI_REFUSED) == 3ll);
T64_LAYOUT_PIN((long long)(T64_VIDEO_SOURCE_RGBA5551) == 2ll);
T64_LAYOUT_PIN((long long)(T64_VIDEO_SOURCE_RGBA8888) == 3ll);
T64_LAYOUT_PIN((long long)(T64_VIDEO_ENCODING_RGBA8) == 1ll);
T64_LAYOUT_PIN((long long)(T64_VIDEO_FILTER_AA) == 1ll);
T64_LAYOUT_PIN((long long)(T64_VIDEO_FILTER_DITHER_SMOOTH) == 2ll);
T64_LAYOUT_PIN((long long)(T64_VIDEO_FILTER_DIVOT_MEDIAN) == 4ll);
T64_LAYOUT_PIN((long long)(T64_VIDEO_FILTER_RESAMPLE_Q20) == 8ll);
T64_LAYOUT_PIN((long long)(T64_VIDEO_FILTER_GAMMA_DITHER) == 16ll);
T64_LAYOUT_PIN((long long)(T64_VIDEO_FILTER_GAMMA_SQRT255) == 32ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_OK) == 0ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_INVALID_ARGUMENT) == 1ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_UNKNOWN_PROFILE) == 2ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_ALLOCATION_FAILED) == 3ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_BAD_STORAGE) == 4ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_INVALID_ROM) == 5ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_UNSUPPORTED_ROM_ORDER) == 6ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_NOT_READY) == 7ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_TIME_REVERSED) == 8ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_BUFFER_TOO_SMALL) == 9ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_LIMIT_REVERSED) == 10ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_QUEUE_FULL) == 11ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_SEQUENCE_EXHAUSTED) == 12ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_TICK_OVERFLOW) == 13ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_RESOURCE_EXHAUSTED) == 14ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_BUS_UNMAPPED) == 15ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_BUS_UNSUPPORTED) == 16ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_BUS_ALIGNMENT) == 17ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_BUS_RANGE) == 18ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_BUS_DEVICE_REFUSED) == 19ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_SAVE_UNKNOWN_DEVICE) == 20ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_SAVE_UNSUPPORTED_DEVICE) == 21ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_SAVE_DEVICE_MISMATCH) == 22ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_SAVE_BAD_FORMAT) == 23ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_SAVE_SIZE_MISMATCH) == 24ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_SAVE_BUSY) == 25ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_STATE_FORMAT) == 26ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_STATE_INTEGRITY) == 27ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_STATE_VERSION) == 28ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_STATE_PROFILE) == 29ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_STATE_CONTENT) == 30ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_STATE_BUILD) == 31ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_STATE_RANGE) == 32ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_STATE_INVARIANT) == 33ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_ABI_MISMATCH) == 34ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_STRUCT_TOO_SMALL) == 35ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_REENTRANT) == 36ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_POISONED) == 37ll);
T64_LAYOUT_PIN((long long)(T64_STATUS_INTERNAL_DEFECT) == 38ll);
T64_LAYOUT_PIN((long long)(T64_TV_NTSC) == 1ll);
T64_LAYOUT_PIN((long long)(T64_TV_PAL) == 2ll);
T64_LAYOUT_PIN((long long)(T64_REGION_US) == 1ll);
T64_LAYOUT_PIN((long long)(T64_REGION_EUROPE) == 2ll);
T64_LAYOUT_PIN((long long)(T64_MEDIA_CARTRIDGE) == 1ll);
T64_LAYOUT_PIN((long long)(T64_SAVE_NONE) == 0ll);
T64_LAYOUT_PIN((long long)(T64_SAVE_EEPROM_4K) == 1ll);
T64_LAYOUT_PIN((long long)(T64_SAVE_EEPROM_16K) == 2ll);
T64_LAYOUT_PIN((long long)(T64_SAVE_SRAM_32K) == 3ll);
T64_LAYOUT_PIN((long long)(T64_SAVE_FLASHRAM) == 4ll);
T64_LAYOUT_PIN((long long)(T64_SAVE_CONTROLLER_PAK) == 5ll);
T64_LAYOUT_PIN((long long)(T64_SAVE_RUMBLE_PAK) == 6ll);
T64_LAYOUT_PIN((long long)(T64_SAVE_TRANSFER_PAK) == 7ll);
T64_LAYOUT_PIN((long long)(T64_SAVE_INFO_BUSY) == 1ll);
T64_LAYOUT_PIN((long long)(T64_SAVE_FORMAT_RAW) == 1ll);
T64_LAYOUT_PIN((long long)(T64_SAVE_FORMAT_SWAP16) == 2ll);
T64_LAYOUT_PIN((long long)(T64_SAVE_FORMAT_WORDREV32) == 3ll);
T64_LAYOUT_PIN((long long)(T64_PERIPHERAL_STANDARD_CONTROLLER_PORT0) == 1ll);
T64_LAYOUT_PIN((long long)(T64_STOP_GUEST_TICK) == 1ll);
T64_LAYOUT_PIN((long long)(T64_STOP_INSTRUCTION_COUNT) == 2ll);
T64_LAYOUT_PIN((long long)(T64_STOP_FRAME_EVENT_COUNT) == 3ll);
T64_LAYOUT_PIN((long long)(T64_STOP_REASON_BOUND) == 1ll);
T64_LAYOUT_PIN((long long)(T64_STOP_REASON_INSTRUCTION_COUNT) == 2ll);
T64_LAYOUT_PIN((long long)(T64_STOP_REASON_FRAME_EVENT_COUNT) == 3ll);
T64_LAYOUT_PIN((long long)(T64_STOP_REASON_HALT) == 4ll);
T64_LAYOUT_PIN((long long)(T64_STOP_REASON_QUIESCENT) == 5ll);
T64_LAYOUT_PIN((long long)(T64_STOP_REASON_RESOURCE_LIMIT) == 6ll);
T64_LAYOUT_PIN((long long)(T64_STOP_REASON_CPU_UNSUPPORTED) == 7ll);
T64_LAYOUT_PIN((long long)(T64_STOP_REASON_GUEST_COMPLETION) == 8ll);
T64_LAYOUT_PIN((long long)(T64_OUTPUT_STATUS_V1_SIZE) == 48ll);
T64_LAYOUT_PIN((long long)(T64_AUDIO_ENCODING_S16BE) == 1ll);
T64_LAYOUT_PIN(sizeof(T64_STATE_CONTAINER_FORMAT) == 23u);
T64_LAYOUT_PIN(sizeof(T64_STATE_CONTAINER_FORMAT_V2) == 23u);
T64_LAYOUT_PIN((long long)(T64_STATE_IMPORT_DROP_OBSERVERS) == 1ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_NONE) == 0ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_ARG_NULL) == 1ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_ARG_LENGTH) == 2ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_ARG_FLAGS) == 3ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_ARG_OVERLAP_LIVE) == 4ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_NOT_ADMITTED) == 5ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_MAGIC) == 6ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_CONTAINER_VERSION) == 7ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_TOTAL_LENGTH) == 8ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_SECTION_COUNT) == 9ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_HEADER_FLAGS) == 10ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_HEADER_RESERVED) == 11ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_TRAILER_SHA256) == 12ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_STATE_VERSION) == 13ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_PROFILE_DIGEST) == 14ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_CONTENT_DIGEST) == 15ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_CONTENT_LENGTH) == 16ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_BUILD_ID) == 17ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_SECTION_ID_UNKNOWN) == 18ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_SECTION_ORDER) == 19ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_SECTION_FLAGS) == 20ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_SECTION_BOUNDS) == 21ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_SECTION_OVERLAP) == 22ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_SECTION_LENGTH_EXPECTED) == 23ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_SECTION_SHA256) == 24ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_SECTION_MISSING) == 25ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_FIELD_RANGE) == 26ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_FIELD_PADDING) == 27ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_FIELD_RESERVED) == 28ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_FIELD_DERIVED) == 29ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_QUEUE_COUNT) == 30ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_QUEUE_ORDER) == 31ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_QUEUE_SEQUENCE) == 32ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_QUEUE_TICK) == 33ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_QUEUE_KIND_UNKNOWN) == 34ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_DEVICE_EVENT_MISSING) == 35ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_DEVICE_EVENT_EXTRA) == 36ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_DEVICE_TOKEN_MISMATCH) == 37ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_DEVICE_STATE_INCONSISTENT) == 38ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_DECLARED_DIGEST) == 39ll);
T64_LAYOUT_PIN((long long)(T64_STATE_REASON_OBSERVER_RANGE) == 40ll);
T64_LAYOUT_PIN((long long)(T64_STATE_RULE_NONE) == 0ll);
T64_LAYOUT_PIN((long long)(T64_STATE_RULE_COUNT_CAP) == 1ll);
T64_LAYOUT_PIN((long long)(T64_STATE_RULE_INDEX_LT) == 2ll);
T64_LAYOUT_PIN((long long)(T64_STATE_RULE_ENUM_CLOSED) == 3ll);
T64_LAYOUT_PIN((long long)(T64_STATE_RULE_BOOL_01) == 4ll);
T64_LAYOUT_PIN((long long)(T64_STATE_RULE_ALIGN) == 5ll);
T64_LAYOUT_PIN((long long)(T64_STATE_RULE_ADDR_RANGE) == 6ll);
T64_LAYOUT_PIN((long long)(T64_STATE_RULE_TICK_GE) == 7ll);
T64_LAYOUT_PIN((long long)(T64_STATE_RULE_TICK_LE) == 8ll);
T64_LAYOUT_PIN((long long)(T64_STATE_RULE_SEQ_LT) == 9ll);
T64_LAYOUT_PIN((long long)(T64_STATE_RULE_SHIFT_LT) == 10ll);
T64_LAYOUT_PIN((long long)(T64_STATE_RULE_LEN_LE) == 11ll);
T64_LAYOUT_PIN((long long)(T64_STATE_RULE_PROFILE_DERIVED) == 12ll);
T64_LAYOUT_PIN((long long)(T64_STATE_RULE_DERIVED_EQ) == 13ll);
T64_LAYOUT_PIN((long long)(T64_STATE_RULE_PAD_ZERO) == 14ll);
T64_LAYOUT_PIN((long long)(T64_STATE_RULE_RESERVED_ZERO) == 15ll);
T64_LAYOUT_PIN((long long)(T64_STATE_RULE_MASKED_AT_USE) == 16ll);

#endif /* UINTPTR_MAX == 0xffffffffffffffffu */
