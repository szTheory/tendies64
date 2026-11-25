# Phase 11 pin-impact record

Planning record of Phase 11 (Installed Consumer and Freeze Candidate), opened by plan
11-01 (phase-wave W0, no production edit). Each later plan appends its own rows; the
sections below are written in the order the plans fill them. Evidence tier: unit-tested,
authored scope; a software-interface claim only.

## Wave bases

Format and rule (standing rule "Base records" of the Phase 11 plans): a base is the
40-hex output of `git rev-parse HEAD`, written here as exactly one line per label,
`- <label>: <40 lowercase hex>`, in a commit that changes only this file. The Wave 0
record below is the one exception: it is committed with Task 1 of plan 11-01, whose
commit has this base as its parent. Later labels ("Install base" by plan 11-15,
"Freeze base" by plan 11-21, "Qualification base" by plan 11-25) are appended by their
plans. No check uses the moving reference HEAD as a base.

- Wave 0 base: f9124ae9fce134f31b645c6f2911a6f27fa41b9a

## Bound-file inventory

Written by plan 11-01 Task 2 before any production edit. Question: does the planned shape
(a leading caller-filled `struct_size` on the in/out structs, fixed-width struct members,
the create-info, the `record_size` parameter of the trace drain and the public init macros)
need anything in the nine API-using oracle-bound test files other than API call-site
updates? The owner approval of 2026-11-25 ("Approve call-site-only rebind") allows call-site
updates only. Categories: (a) a public API call expression; (b) the initializer or
declaration of an object passed to or received from one public call (init macro, `struct_size`
set); (c) anything else.

Method (scratch only; the shared tree's sources, the checker and `docs/evidence` are
untouched, which Task 2 verifies with `git diff --quiet`):

1. `git clone --no-hardlinks` into `build/11-01-probe/src`, detached at `01e389e` (the
   Task 1 commit; its tree equals the Wave 0 base in every source file).
2. The scratch public header is `core/include/tendies64/t64_machine.h` rewritten to the
   planned shape: leading `uint32_t struct_size` on `t64_host_storage`, `t64_host_allocator`,
   `t64_profile_descriptor`, `t64_stop_request`, `t64_stop_result`, `t64_cpu_stop_detail`,
   `t64_output_drain_result`, `t64_video_packet_info`, `t64_state_import_report` and
   `t64_save_info` (`t64_audio_packet_info` already has it); every `size_t` struct member
   `uint64_t` and every `bool` struct member `uint32_t` (function parameters and the
   allocator callback typedef unchanged); `t64_machine_create(const t64_machine_create_info *,
   t64_machine **)`; `t64_machine_trace_drain` with `uint32_t record_size` after
   `records_out`; the five new functions, the new statuses 34 to 38, `extern "C"`, `T64_API`,
   the ABI constants and the init macros. Member names and relative order are unchanged.
   The scratch `core/t64_internal.h` keeps the retained VI and AI packet slots byte for byte
   as internal types `t64_vi_packet_slot` (488 bytes) and `t64_ai_packet_slot` (280 bytes).
3. Each unit is compiled unchanged with the gate flags of `ninja -C build/gate -t commands`
   (`-O3 -DNDEBUG -std=c17 -Wall -Wextra -Wpedantic -Wshadow -Wconversion
   -Wstrict-prototypes -Wmissing-prototypes -Wundef -Wvla -Wcast-qual -Wformat=2
   -Wwrite-strings -Werror`, Apple clang, arm64, `-c`, `-ferror-limit=0`, checkout path
   mapped to the scratch path): the nine files, the second unit of `test_rdp.c` with
   `-DT64_TEST_DP_TRANSPORT=1` (CTest executable `test_dp_transport`) and
   `runner/tests/test_rdp_video.c`. Diagnostics are the lines containing `error:` or
   `warning:`.
4. The minimal rule-tagged edits (scratch only) then make every unit compile with zero
   diagnostics, and a manual pass edits the sites that compile clean but would be refused at
   run time with `struct_size` 0 (compile-silent sites). Every `git diff -U0` hunk of the nine
   files is classified by its added text (a hunk may hold more than one edit, so edits and
   hunks are both counted).

Planned public struct sizes on the probe target (LP64, arm64) against today:
`t64_host_storage` 24 to 32, `t64_host_allocator` 24 to 32, `t64_profile_descriptor` 2,300 to
2,304, `t64_stop_request` 16 to 16, `t64_stop_result` 16 to 16, `t64_cpu_stop_detail` 16 to 24,
`t64_trace_record` 24 to 24 (no `struct_size`), `t64_output_drain_result` 64 to 72,
`t64_video_packet_info` 488 to 488 (the leading member fills the padding hole before the first
64-bit member), `t64_audio_packet_info` 280 to 280, `t64_state_import_report` 24 to 32,
`t64_save_info` 24 to 24; new `t64_machine_create_info` 24 and `t64_output_status` 40. The
retained slots stay at 488 and 280 by construction (Hazards H1 and H2 of the pattern map).

Compile result (unchanged files against the planned shape) and the edit set:

| File | Diagnostics unchanged | Edits (a) | Edits (b) | Edits (c) | Hunks a / b / c | Note |
|------|-----------------------|-----------|-----------|-----------|-----------------|------|
| core/tests/test_rdp.c | 8 (13 with `T64_TEST_DP_TRANSPORT=1`) | 3 | 5 | 1 | 3 / 4 / 1 | (c) at line 486, see "Owner question" |
| core/tests/test_rdp_copy.c | 4 | 1 | 2 | 0 | 1 / 2 / 0 | |
| core/tests/test_rdp_triangles.c | 8 | 2 | 2 | 0 | 2 / 2 / 0 | |
| core/tests/test_rdp_texture.c | 4 | 1 | 2 | 0 | 1 / 2 / 0 | |
| core/tests/test_rdp_combiner.c | 4 | 1 | 2 | 0 | 1 / 2 / 0 | |
| core/tests/test_rdp_blender.c | 9 | 2 | 3 | 0 | 2 / 3 / 0 | |
| core/tests/test_rdp_depth.c | 4 | 1 | 2 | 0 | 1 / 2 / 0 | |
| core/tests/test_vi.c | 97 | 5 | 90 | 0 | 5 / 64 / 0 | |
| core/tests/test_video_output.c | 16 | 1 | 12 | 0 | 1 / 11 / 0 | |
| runner/tests/test_rdp_video.c | 0 | 0 | 0 | 0 | 0 / 0 / 0 | compiled unchanged against the scratch tree; includes no core header (`t64_video_observation.h`, `t64_observation_pipe.h` and `t64_json.h` include none); needs no edit |
| Total (nine files) | 154 | 17 | 120 | 1 | 17 / 92 / 1 | 110 hunks |

The two compile errors that remain after every rule-tagged edit are the (c) site of
`test_rdp.c` (2 diagnostics, in the `T64_TEST_DP_TRANSPORT=1` unit only) and a `t64_stop_result`
compound literal of `test_vi.c` (2 diagnostics, a category (b) site the rule missed and a
manual edit then converted); with both edited, all eleven units compile with 0 diagnostics.

Edit kinds:

| Kind | Cat | Edits | Where |
|------|-----|-------|-------|
| `t64_machine_create(a, p, &m)` becomes `t64_machine_create(&(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(a, p), &m)` | (a) | 16 | test_rdp 3, copy 1, triangles 2, texture 1, combiner 1, blender 2, depth 1, vi 4, video_output 1 |
| `t64_machine_trace_drain` gains `(uint32_t)sizeof(t64_trace_record)` after `records_out` | (a) | 1 | vi |
| allocator initializer `{allocate, release, context}` becomes `T64_HOST_ALLOCATOR_INIT(...)` | (b) | 16 | one per create call above |
| `t64_stop_request` declaration initializer or compound literal becomes `T64_STOP_REQUEST_INIT(kind, value)` | (b) | 16 | rdp 1, vi 12, video_output 3 |
| `t64_stop_result` initializer or compound literal becomes `T64_STOP_RESULT_INIT` or `T64_STOP_RESULT_INIT_WITH(reason, tick)` | (b) | 35 | rdp 1, vi 31, video_output 3 |
| `t64_cpu_stop_detail` initializer becomes `T64_CPU_STOP_DETAIL_INIT` | (b) | 1 | vi |
| `t64_video_packet_info` zero initializer (48), uninitialized declaration (2: `test_vi.c` lines 714 and 1891), array of two (2: lines 2073 and 2074) becomes `T64_VIDEO_PACKET_INFO_INIT` | (b) | 52 | copy 1, texture 1, combiner 1, blender 1, depth 1, vi 42, video_output 5 |
| `t64_host_storage image = {NULL, 0u, 0u}` becomes `{0}` | (c) | 1 | rdp line 486 |

The 52 `t64_video_packet_info` sites compile clean under the planned header and would be
refused at run time (`struct_size` 0 is below the v1 minimum); they are the sites that
only a scan, not the compiler, finds. The other 68 category (b) edits and the 17 category
(a) edits are compile-detected. Allocator callbacks that write `storage_out->data`,
`->capacity` and `->alignment` by name, all `size_t` function parameters, all `bool`
function parameters and every `T64_CHECK_INT` of a packet info member compile
unchanged, so no field-type adaptation, expected value, `sizeof` or offset pin, check,
case name, control-flow or format-string edit is needed in any bound file. No bound file reads a
retained slot array (`packet_info` and `packet_lengths` are never accessed); `machine->vi.packet_count`
is a scalar that the shape does not touch. The probe proves
compilation only; the behavior of the converted files is re-proved by plans 11-12 and
11-14, and the Linux gcc compile of the converted tree by the preflight step linux-compile.

Result: one category (c) hunk exists in a bound file, so the plan records the owner
question below and stops before Task 3 (T-11-03).

## Public-struct call-site counts

Measured at the Task 1 commit with `git grep -n -I -E` over `core`, `runner`, `tools` and
`tests` (lines / files). `T x;` counts declarations without an initializer (pointer
declarations excluded); these are the sites that compile after the change and fail at run time
with `struct_size` 0 (Hazard H3), so the Phase 11 guard `scripts/check-api-init.py` (plan
11-10) must reach every type below that callers fill.

| Type | mentions | `T x = {` | `(T){` | `T x = <expr>` | `T x;` |
|------|----------|-----------|--------|----------------|--------|
| t64_host_storage | 196 / 77 | 14 / 12 | 0 / 0 | 3 / 2 | 24 / 3 |
| t64_host_allocator | 218 / 77 | 130 / 60 | 1 / 1 | 0 / 0 | 14 / 14 |
| t64_profile_descriptor | 392 / 82 | 6 / 3 | 0 / 0 | 14 / 3 | 22 / 13 |
| t64_stop_request | 203 / 65 | 145 / 46 | 35 / 15 | 0 / 0 | 16 / 9 |
| t64_stop_result | 249 / 65 | 215 / 61 | 1 / 1 | 0 / 0 | 11 / 3 |
| t64_cpu_stop_detail | 32 / 12 | 27 / 10 | 0 / 0 | 0 / 0 | 1 / 1 |
| t64_output_drain_result | 30 / 16 | 9 / 5 | 1 / 1 | 0 / 0 | 15 / 11 |
| t64_video_packet_info | 95 / 20 | 51 / 8 | 0 / 0 | 0 / 0 | 17 / 10 |
| t64_audio_packet_info | 37 / 14 | 0 / 0 | 0 / 0 | 0 / 0 | 23 / 10 |
| t64_state_import_report | 62 / 8 | 0 / 0 | 0 / 0 | 0 / 0 | 38 / 7 |
| t64_save_info | 17 / 8 | 0 / 0 | 0 / 0 | 0 / 0 | 13 / 6 |
| t64_trace_record | 61 / 31 | 2 / 1 | 0 / 0 | 0 / 0 | 38 / 27 |

Call lines: `t64_machine_create(` 157 lines in 76 files (2 are the declaration and the definition,
so 155 call sites in 74 files); `t64_machine_trace_drain(` 52 lines in 28 files;
`t64_machine_advance(` 275 lines in 63 files. The pattern-map figures (145 stop-request
initializers, 35 compound literals, 16 uninitialized declarations in 9 files, 130 allocator
initializers, 215 stop-result initializers) reproduce exactly. The 14 positional
`t64_host_storage` initializers sit in 12 files; one of them is the (c) hunk of
`test_rdp.c` and the other 11 files are not bound.

## Class-table inventory

Inventory of every `T64_STATUS_*` (0 to 33) and `T64_STOP_REASON_*` (1 to 8) with its sites
in the production sources and the proposed class from RESEARCH "Status and stop-reason
inventory" and the AMBIG decisions of CONTEXT. The new statuses appended after 33 and the
final wording are fixed by the Decision record, not here.

Sites are lines of `core/t64_*.c`, `core/t64_internal.h` and `core/t64_rsp_divider.h` at
the Task 1 commit that contain the name outside comments (`git grep -n`); `ret` counts the
lines that return, assign or select it, `other` the remaining lines (comparisons and, for stop
reasons, the argument of `set_stop_result`). Sites are listed as `file:line`, core-relative,
first six lines per file and first six files; larger sets show counts per file.

| Value | Name | all / ret / other | Sites | Proposed class | Resume | Note |
|-------|------|-------------------|-------|----------------|--------|------|
| 0 | T64_STATUS_OK | 539 / 534 / 5 | 534 sites in 27 files (t64_bus.c 53, t64_cache.c 52, t64_machine.c 50, t64_rsp.c 42, t64_rdp.c 36) | ok | - | success row; every call that completes. |
| 1 | T64_STATUS_INVALID_ARGUMENT | 143 / 143 / 0 | 143 sites in 27 files (t64_machine.c 28, t64_scheduler.c 11, t64_cache.c 10, t64_save.c 10, t64_bus.c 9) | refused | n/a | NULL, overlap and range checks (a NULL machine answers INVALID_ARGUMENT, outputs untouched, documented per function). AMBIG-A: `t64_cp0.c:581` (corrupt `count_half_clock_phase`) and `t64_machine.c:2943` (stored profile digest no longer matches the profile bytes in `admit_rom`) are internal invariant violations, reclassified as defect (poison) by CONTEXT. |
| 2 | T64_STATUS_UNKNOWN_PROFILE | 1 / 1 / 0 | t64_machine.c:2566 | refused | n/a | `t64_profile_lookup`; no instance exists. |
| 3 | T64_STATUS_ALLOCATION_FAILED | 4 / 4 / 0 | t64_machine.c:2758,2791,2818; t64_rom.c:172 | refused | n/a | host allocator refusal at create and admit. |
| 4 | T64_STATUS_BAD_STORAGE | 10 / 10 / 0 | t64_digest.c:345,350; t64_machine.c:2766,2804,2836; t64_rom.c:184; t64_state.c:169,214,221,335 | refused and defect (AMBIG-B) | n/a | one value, two meanings. Allocator-contract sites (`t64_machine.c:2766,2804,2836`, `t64_rom.c:184`) stay refused. Internal-inconsistency sites (`t64_digest.c:345,350`, `t64_state.c:169,214,221,335`) become a new appended defect-class status that poisons; state export no longer clears the buffer on a refused status (header text lines 405 to 409). |
| 5 | T64_STATUS_INVALID_ROM | 6 / 6 / 0 | t64_machine.c:2935,2953; t64_rom.c:133,138,165,168 | refused | n/a | `t64_rom.c`; `t64_machine.c:2935` answers it for a NULL machine, which should be INVALID_ARGUMENT (candidate fix, moves no digest). |
| 6 | T64_STATUS_UNSUPPORTED_ROM_ORDER | 0 / 0 / 0 | none | reserved | n/a | never returned (0 sites; the only occurrence is the `#define`); value kept, classified with a note, no renumbering. |
| 7 | T64_STATUS_NOT_READY | 31 / 31 / 0 | t64_ai.c:260,304,633,635,661,663; t64_digest.c:336; t64_machine.c:2995,3010,3027,3053,3088; t64_observe.c:46,117; t64_rsp.c:45,2456; t64_save.c:308,379,436; +5 files | refused | n/a | no admitted image or terminal CPU stop. |
| 8 | T64_STATUS_TIME_REVERSED | 4 / 4 / 0 | t64_cp0.c:577; t64_scheduler.c:104,246,420 | refused | n/a | `t64_cp0.c:577` can follow earlier state changes inside a dispatch (AMBIG-C context: surfaces as RESOURCE_LIMIT). |
| 9 | T64_STATUS_BUFFER_TOO_SMALL | 7 / 7 / 0 | t64_ai.c:668; t64_machine.c:2612,3057; t64_observe.c:69; t64_save.c:408; t64_state.c:376; t64_vi.c:1313 | refused | n/a | size-then-copy-out queries; carries the required length. |
| 10 | T64_STATUS_LIMIT_REVERSED | 1 / 1 / 0 | t64_scheduler.c:474 | refused | n/a | `advance_counter_bound`. |
| 11 | T64_STATUS_QUEUE_FULL | 6 / 6 / 0 | t64_ai.c:206,488; t64_scheduler.c:107,163; t64_vi.c:990,1186 | refused | n/a | host advances, then retries. |
| 12 | T64_STATUS_SEQUENCE_EXHAUSTED | 10 / 10 / 0 | t64_ai.c:134,209,225,493,502; t64_rsp.c:22; t64_scheduler.c:110,113,173; t64_sp.c:484 | refused | terminal until reset | permanent until `t64_machine_reset_execution_state` clears the flag. |
| 13 | T64_STATUS_TICK_OVERFLOW | 5 / 4 / 1 | t64_ai.c:100; t64_pi.c:109; t64_scheduler.c:142,176 | refused | n/a | ai, pi, scheduler. |
| 14 | T64_STATUS_RESOURCE_EXHAUSTED | 55 / 54 / 1 | 54 sites in 10 files (t64_cpu.c 18, t64_vi.c 16, t64_sp.c 5, t64_pi.c 3, t64_state.c 3) | refused at the API boundary | n/a | container size ceiling; inside `dispatch_front` the same value is the backpressure signal and is never returned by `advance` (AMBIG-E). |
| 15 | T64_STATUS_BUS_UNMAPPED | 13 / 13 / 0 | t64_bus.c:352,379,415,569,617,809+1; t64_cpu.c:1794,2241,2411,2454,2468; t64_memory.c:162 | refused | n/a | debug bus wrappers over `t64_bus_read/write`. |
| 16 | T64_STATUS_BUS_UNSUPPORTED | 96 / 96 / 0 | 96 sites in 15 files (t64_bus.c 26, t64_rdram.c 14, t64_cache.c 10, t64_warm_devices.c 7, t64_vi.c 6) | refused | n/a | debug bus wrappers over `t64_bus_read/write`. |
| 17 | T64_STATUS_BUS_ALIGNMENT | 13 / 13 / 0 | t64_bus.c:211,214,562,798,895; t64_cpu.c:2246,2404; t64_isviewer.c:178; t64_rdp.c:703; t64_save.c:179,202; t64_vi.c:714,742 | refused | n/a | debug bus wrappers over `t64_bus_read/write`. |
| 18 | T64_STATUS_BUS_RANGE | 25 / 25 / 0 | t64_bus.c:208,363,541,565,587,591+8; t64_cache.c:86,92,108,133,267; t64_cpu.c:1794,2242,2412,2455,2469; t64_isviewer.c:186 | refused | n/a | debug bus wrappers over `t64_bus_read/write`. |
| 19 | T64_STATUS_BUS_DEVICE_REFUSED | 124 / 119 / 5 | 119 sites in 12 files (t64_rdram.c 27, t64_sp.c 21, t64_rsp.c 15, t64_vi.c 13, t64_bus.c 11) | refused with a written carve-out (AMBIG-D) | n/a | a refusing device can latch its own record (PI length and sticky error, SI sticky DMA error and a trace record, header text lines 655 to 665), so `t64_machine_bus_write` leaves the refusal-is-a-no-op domain; the status stays refused. |
| 20 | T64_STATUS_SAVE_UNKNOWN_DEVICE | 1 / 1 / 0 | t64_save.c:273 | refused | n/a | `t64_save.c`; exact check order R0 to R9 in the header text. |
| 21 | T64_STATUS_SAVE_UNSUPPORTED_DEVICE | 1 / 1 / 0 | t64_save.c:276 | refused | n/a | `t64_save.c`; exact check order R0 to R9 in the header text. |
| 22 | T64_STATUS_SAVE_DEVICE_MISMATCH | 2 / 2 / 0 | t64_save.c:269,279 | refused | n/a | `t64_save.c`; exact check order R0 to R9 in the header text. |
| 23 | T64_STATUS_SAVE_BAD_FORMAT | 2 / 2 / 0 | t64_save.c:403,449 | refused | n/a | `t64_save.c`; exact check order R0 to R9 in the header text. |
| 24 | T64_STATUS_SAVE_SIZE_MISMATCH | 1 / 1 / 0 | t64_save.c:453 | refused | n/a | `t64_save.c`; exact check order R0 to R9 in the header text. |
| 25 | T64_STATUS_SAVE_BUSY | 1 / 1 / 0 | t64_save.c:456 | refused | n/a | `t64_save.c`; clears after the busy window. |
| 26 | T64_STATUS_STATE_FORMAT | 1 / 1 / 0 | t64_state.c:521 | refused | n/a | `t64_state.c`; the machine stays bit-identical. |
| 27 | T64_STATUS_STATE_INTEGRITY | 1 / 1 / 0 | t64_state.c:492 | refused | n/a | `t64_state.c`; the machine stays bit-identical. |
| 28 | T64_STATUS_STATE_VERSION | 1 / 1 / 0 | t64_state.c:494 | refused | n/a | `t64_state.c`; the machine stays bit-identical. |
| 29 | T64_STATUS_STATE_PROFILE | 1 / 1 / 0 | t64_state.c:496 | refused | n/a | `t64_state.c`; the machine stays bit-identical. |
| 30 | T64_STATUS_STATE_CONTENT | 1 / 1 / 0 | t64_state.c:499 | refused | n/a | `t64_state.c`; the machine stays bit-identical. |
| 31 | T64_STATUS_STATE_BUILD | 1 / 1 / 0 | t64_state.c:501 | refused | n/a | `t64_state.c`; the machine stays bit-identical. |
| 32 | T64_STATUS_STATE_RANGE | 1 / 1 / 0 | t64_state.c:507 | refused | n/a | `t64_state.c`; the machine stays bit-identical. |
| 33 | T64_STATUS_STATE_INVARIANT | 1 / 1 / 0 | t64_state.c:517 | refused | n/a | `t64_state.c`; a blob fault, not an instance defect. |

| Value | Name | all / ret / other | Sites | Proposed class | Resume | Note |
|-------|------|-------------------|-------|----------------|--------|------|
| 1 | T64_STOP_REASON_BOUND | 1 / 0 / 1 | t64_scheduler.c:463 | guest-stop | resumable | end of `advance_time_bound`. |
| 2 | T64_STOP_REASON_INSTRUCTION_COUNT | 1 / 1 / 0 | t64_scheduler.c:409 | guest-stop | resumable | `reached_reason`. |
| 3 | T64_STOP_REASON_FRAME_EVENT_COUNT | 1 / 1 / 0 | t64_scheduler.c:410 | guest-stop | resumable | `reached_reason`. |
| 4 | T64_STOP_REASON_HALT | 4 / 0 / 4 | t64_scheduler.c:428,453,482,519 | guest-stop, reserved | terminal until reset | `machine->halted` is set only by the synthetic halt event scheduled through the `t64_test_*` hooks; unreachable through the installed public API (exercised in-tree); the four sites report it when `machine->halted`. |
| 5 | T64_STOP_REASON_QUIESCENT | 1 / 0 / 1 | t64_scheduler.c:494 | guest-stop | resumable by queued input only | counter bound with an empty event queue. |
| 6 | T64_STOP_REASON_RESOURCE_LIMIT | 6 / 2 / 4 | t64_scheduler.c:433,438,448,499,504,514 | guest-stop | resumable after drain or yield (AMBIG-C/E) | three causes: the per-advance event budget, a handler that cannot dispatch for VI or AI output backpressure or counter overflow, and any non-OK handler status (`t64_scheduler.c:444-451,510-517`); a stall with no progress after a full drain is permanent. Handler-error masking is whitelisted so the poison hook fires only on non-whitelisted statuses. |
| 7 | T64_STOP_REASON_CPU_UNSUPPORTED | 2 / 2 / 0 | t64_cpu.c:1333; t64_scheduler.c:397 | guest-stop | terminal | neither poisons nor claims the guest chose to stop; an emulator limitation. |
| 8 | T64_STOP_REASON_GUEST_COMPLETION | 3 / 3 / 0 | t64_cpu.c:1348; t64_scheduler.c:395,396 | guest-stop | terminal | software debug-stop convention, not a guest pass or CPU exception. |

## Pin-impact map

Rows 1 to 4 and 7 to 11 restate RESEARCH "Pin-impact propagation" (not re-measured);
rows 5, 6, 12 and 13 are the hazards found by this plan's reads of the checkers and the
pattern map (read, not executed, unless stated).

| # | Edit | Pin or check it moves | Cheap check | Owning plan |
|---|------|-----------------------|-------------|-------------|
| 1 | Any production text edit (every core `.c` and `.h` is hashed) | `T64_BUILD_ID`, and through it the packet-bearing digests of pre-v25 ids (retained packet metadata keeps `profile_id` and `build_id` only through v24, `docs/spec/machine-contract.md` lines 957 to 962); no test pins a real build id literal | `python3 -B scripts/digest-sweep.py --base WAVE0_BASE --head WORKTREE` (forced build id, 1,599 shared lines) | every SHAPE plan; before and after proof in 11-12 and the review 11-15 |
| 2 | Edit of any of the 13 `EXPECTED_CORPUS_PATHS` files | whole-file SHA-256 and the derived `PINNED_REQUEST_SHA256` (`scripts/check-rdp-oracle-authority.py` line 23); orchestrator-only identity rebind | `ctest -R '^harness\.rdp-oracle-authority$'` (0.2 s) and `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked` | 11-14 Task 2 (orchestrator) |
| 3 | API call-site edits of the nine API-using bound files | the same hashes; measured edit set 17 (a), 120 (b), 1 (c) (section "Bound-file inventory") | `grep -c` per file; the scratch gate before each code commit in the DEV-11-01 window | 11-05 to 11-10 convert every other file; the bound files change only at the 11-12 flip (scratch clone) and the 11-14 isolated commit |
| 4 | Edit of a sealed body: the public observers `t64_video_info`, `t64_video_copy` (`core/t64_vi.c`), `t64_audio_info`, `t64_audio_copy` (`core/t64_ai.c`) and the callees `t64_bus_write`, `t64_bus_write_masked` of the public bus wrappers; 231 `REVIEWED_BODY_DIGESTS` keys (checked by parsing the table) | a digest mismatch in `scripts/check-rsp-no-hle.py`; an isolated refresh commit "isolated refresh of no-HLE pins for <names>" | `python3 -B scripts/check-rsp-no-hle.py --build build/gate` before and after | 11-13 (the `struct_size` prologues) and any plan that edits one of these bodies |
| 5 | Folding `tools/common/t64_sha256.c` into `t64_core` (research: the private link breaks `install(EXPORT)`) | `_check_build_route` (lines 1060 to 1170) compares the ninja object edges under `core/CMakeFiles/t64_core.dir/`, the archive link edge and the `ar -t` members with `CANONICAL_CORE_SOURCES` plus the SoftFloat sources: the new object edge (CMake places a source outside the target directory under `__/`, here `__/tools/common/t64_sha256.c.o`) and the new member `t64_sha256.c.o` are reported as "unreviewed core object edge(s)", "unreviewed object(s) linked" and "actual archive members differ from reviewed production object inventory"; the `add_library` body check (`_check_cmake_route`) only requires the two reviewed source variables and still passes | `python3 -B scripts/check-rsp-no-hle.py --build build/gate`; `scripts/tests/test_rsp_no_hle.py` holds 12 references to the archive and object paths and probably moves with it | 11-16 (inventory refresh in an isolated commit) |
| 6 | The `t64i_` prefix rename (generated `core/t64_prefix.h`) | `check_rsp_no_hle` looks up 12 symbols by literal name in the `nm -a` output of five objects and strips only leading underscores: `t64_rsp.c.o` (`t64_rsp_issue_event`, `t64_rsp_ensure_opportunity`, `t64_rsp_start`), `t64_rdp_raster.c.o` (`t64_rdp_triangle_bounds`, `t64_rdp_triangle_render_row`, `t64_rdp_triangle_word_count`), `t64_rdp_tmem.c.o` (`t64_rdp_tmem_triangle_pixel`, `t64_rdp_tmem_validate_triangle`), `t64_rdp_pipeline.c.o` (`t64_rdp_combine_pixel`) and `t64_rdp_depth.c.o` (`t64_rdp_depth_triangle_preflight`, `t64_rdp_depth_evaluate_pixel`, `t64_rdp_depth_commit_pixel`); `nm` of the gate objects shows 11 of them global (`T`, renamed) and `t64_rsp_issue_event` local (`t`, not in the generated set of defined globals), so 11 required symbols read "absent" after the rename | `python3 -B scripts/check-rsp-no-hle.py --build build/gate` after the rename | 11-11 (checker name map in an isolated refresh commit) |
| 7 | New `struct t64_machine` members (`poisoned`, `in_call`) | member audit rows in `docs/spec/machine-contract.md` line 957 (no test parses them); `state-field-rules.txt` covers only members used as index, length, shift or loop bound, so a plain flag needs no row | `ctest -R '^harness\.state-field-use-audit$'` | 11-03 (poison and in-call members) |
| 8 | Any function whose body mentions a profile version field or predicate | `docs/spec/profile-membership-sites.txt` (generated) | `ctest -R '^repo\.si-membership-sites$'` | the plan that edits such a body |
| 9 | Any new CTest name | exact lists in `scripts/check-test-inventory.sh` (`PHASE11_TESTS` block and the `ALL` concatenation), the preflight label count and regex rows, about 0.2 s selftest cost per name (baseline 254 names), and a `COST` and `TIMEOUT` by the standing rule | `sh scripts/check-test-inventory.sh --preset gate` | every plan that registers a name, first 11-02 |
| 10 | Edit of `.github/workflows/ci.yml`, `scripts/check-ci-yml.sh` or `scripts/gate.sh` | bound source and scanner rules `CI-RUN-LINE`, `CI-STRESS-WORKFLOW`, `CI-WORKFLOW-FILE` | `ctest -R '^repo\.ci-yml'` | none (no edit planned) |
| 11 | Phase-root literals (`PIN_IMPACT_SUBPATH`, `PF_ACT_IMAGE`, regex table) | copied into `qualify-phase11.py` and `preflight-phase11.sh` | `sh scripts/preflight-phase11.sh --no-replay` | 11-01 (preflight), 11-25 (qualifier) |
| 12 | Hazard H1: `t64_audio_packet_info.struct_size` is set at `core/t64_ai.c` line 536, hashed at `core/t64_digest.c` line 1000, restored at `core/t64_state.c` line 2092 and checked against `sizeof` by `runner/t64_raw_capture.c` line 829 and `runner/t64_real.c` line 742 | any change of the retained AI slot's `struct_size` value or size moves every AI-family digest and breaks state import; the planned public struct stays 280 bytes and the retained slot stays byte-identical | the digest sweep (row 1) and `ctest -R '^unit\.core\.digest'` | the plan that introduces the slot types, before the 11-12 flip |
| 13 | Hazard H2: size pins on public structs (`runner/t64_audio_observation.h` line 84 `sizeof(t64_audio_packet_info) == 280u`; `core/tests/test_digest_v26.c` lines 921 to 926 `sizeof(t64_video_packet_info) == 488u` and `sizeof(t64_vi_state) == 1208u`) | `t64_vi_state` embeds the retained VI slot; the planned public `t64_video_packet_info` is 488 bytes by the probe and the retained slot is the internal `t64_vi_packet_slot`, so no pin moves | the probe's `sizeof` table above; the 37-row member sweep of `test_digest_v26.c` | the plan that introduces the slot types, before the 11-12 flip |

## Owner question

Raised by plan 11-01 Task 2 under the standing rule of the plan (a required edit in a
bound file that is not an API call-site update becomes an owner question and the plan stops
before Task 3 instead of designing around it) and threat T-11-03.

Hunk: `core/tests/test_rdp.c` line 486, inside `create_rsp_only_test_machine` (compiled
into the CTest executable `test_dp_transport`, the `-DT64_TEST_DP_TRANSPORT=1` unit of the
bound file only):

    t64_host_storage image = {NULL, 0u, 0u};     becomes     t64_host_storage image = {0};

Why it falls outside the category list: the object is not passed to or received from any
public call. The test calls its own allocator callback (`test_allocate(NULL, 2u, 1u, &image)`)
and stores the result in an internal member (`machine->image_storage = image`) of a machine
that it assembles by hand for an RSP-only check. It is an initializer of an object of a
public struct type, forced by the new leading `struct_size`: the positional form
`{data, capacity, alignment}` no longer compiles (2 diagnostics under `-Wextra -Werror`: an int-conversion of the pointer and a missing `alignment` initializer).
The edit changes no expected value, check, case name, control flow or format string, and
the `{0}` form leaves `struct_size` 0 because nothing reads it on this path.

Approval text it falls outside: "API call-site updates only" (STATE.md, owner approval
2026-11-25 "Approve call-site-only rebind"; plan 11-01 DR-27 reads "call site" as categories
(a) and (b)).

Alternatives (the plan does not choose; the question is for the owner through the
orchestrator):

- A. Read the approval as covering initializer updates of public-struct objects that the
  added member forces (this one value-neutral hunk). The plan continues at Task 3 unchanged
  and plan 11-14 applies the hunk in its isolated commit. Recommended: the edit is
  mechanical and value-neutral, and it keeps the CONTEXT decision that the core fills
  `t64_host_storage.struct_size` before each allocate call, which lets a later core add storage
  fields without breaking an older host allocator.
- B. Change the shape instead: `t64_host_storage` keeps its three members and gets no
  `struct_size` (an out struct that the host callback fills and the core hands back by value
  to release; the original RESEARCH suggestion of an exemption with a layout assert). The
  hunk disappears and the bound-file edit set is (a) 17 and (b) 120 only. Cost: an explicit
  exception to the CONTEXT wording "every in/out public struct carries a leading
  `struct_size`", and a later growth of the storage struct would need a new callback type.
  Decision row DR-06 would then say so.
- C. Widen nothing and leave `test_rdp.c` line 486 as it is: the `test_dp_transport` unit then
  no longer compiles after the flip, which breaks the gate; listed only for completeness.

Resume point: after the answer, Task 3 (Decision record, carry-in triage, LibFuturo paper
read, G4 position) and Task 4 (digest-sweep printer shim) are still to run; Tasks 1 and 2 are
committed. The DR-06 text and the Task 3 shape design must follow the answer (A keeps the
probe shape, B removes `struct_size` from `t64_host_storage`).
