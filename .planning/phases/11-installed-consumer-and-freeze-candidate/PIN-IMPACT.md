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
| core/tests/test_rdp.c | 8 (13 with `T64_TEST_DP_TRANSPORT=1`) | 3 | 5 | 1 | 3 / 4 / 1 | (c) at line 486, approved by owner answer A of 2026-11-25 (see "Owner question"); applied by plan 11-14 in its isolated commit |
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

Result: one category (c) hunk exists in a bound file, so the plan recorded the owner
question below and stopped before Task 3 (T-11-03). The owner answered A on 2026-11-25
(section "Owner question", Resolution): the hunk is an approved hunk, stays listed in this
inventory as the single (c) entry, and is applied by plan 11-14 in its isolated commit. The
approval is not widened: every other edit of every bound file stays category (a) or (b).

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
| 3 | API call-site edits of the nine API-using bound files | the same hashes; measured edit set 17 (a), 120 (b), 1 (c) (section "Bound-file inventory"); the one (c) hunk (`test_rdp.c` line 486) is approved by owner answer A of 2026-11-25 and applied in the same isolated commit | `grep -c` per file; the scratch gate before each code commit in the DEV-11-01 window | 11-05 to 11-10 convert every other file; the bound files change only at the 11-12 flip (scratch clone) and the 11-14 isolated commit |
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

Resolution (2026-11-25): RESOLVED, answer A. The owner extended the call-site-only rebind
approval of 2026-11-25 to the one value-neutral initializer update `core/tests/test_rdp.c`
line 486 (`t64_host_storage image = {NULL, 0u, 0u};` becomes `{0}`). The extension is recorded
in STATE.md and CONTEXT.md (commit 238583e). Terms: the same identity-only, orchestrator-only,
one-isolated-commit placement as the approval it extends (plan 11-14 Task 2), `local_decision`
stays blocked, and `t64_host_storage` KEEPS its leading `struct_size` (the core fills it before
each allocate call; DR-06). The approval covers this one hunk only: no expected value, check,
case name, `sizeof` or offset pin, control-flow or format-string edit in any bound file is
covered, and any further category (c) hunk found later is a new owner question. The (c) hunk
stays listed in the "Bound-file inventory" as an approved hunk for the isolated commit of plan
11-14; alternatives B and C below are not taken. The text below is the question as raised.

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

Resume point (as raised): after the answer, Task 3 (Decision record, carry-in triage,
LibFuturo paper read, G4 position) and Task 4 (digest-sweep printer shim) were still to run;
Tasks 1 and 2 were committed. The DR-06 text and the Task 3 shape design follow the answer (A
keeps the probe shape, B would have removed `struct_size` from `t64_host_storage`); answer A
was given, so the probe shape stands and Tasks 3 and 4 run as the plan prescribes.

## Decision record

Written by plan 11-01 Task 3 before any production edit, after the owner answer A of
2026-11-25 (section "Owner question"). Every row binds each later Phase 11 plan: a later plan
that needs a different name, value or shape records the change in this table first. Columns:
the row, the decision, its source (CTX locked, CTX autonomous or discretion, with the CONTEXT
decision it implements) and a reversibility rating. Reversibility is costly for DR-01, DR-02,
DR-05 to DR-09 and DR-16 (they shape the bound record); they are already settled by CONTEXT or
by Claude's discretion, so no checkpoint is raised (REVERSIBILITY_GATES false). Evidence tier:
a software-interface claim only.

| ID | Decision | Source | Reversibility |
|----|----------|--------|---------------|
| DR-01 | Statuses appended after 33 without renumbering any shipped value: 34 `T64_STATUS_ABI_MISMATCH` (refused, returned by create only), 35 `T64_STATUS_STRUCT_TOO_SMALL` (refused, a `struct_size` below the v1 minimum), 36 `T64_STATUS_REENTRANT` (refused, a call while a call on the same instance is in progress), 37 `T64_STATUS_POISONED` (defect, the answer of every call on a poisoned instance except destroy and the poison query), 38 `T64_STATUS_INTERNAL_DEFECT` (defect, returned by the call that detects an internal invariant violation, which poisons the instance). The header then defines 39 status constants (0 to 38); stop reasons 1 to 8 are unchanged. | CTX autonomous (ABI status names; class table AMBIG-B); exact names are discretion | costly (numeric values join the bound record in 11-23; additions stay cheap until then) |
| DR-02 | ABI constants: `T64_ABI_VERSION_MAJOR` UINT32_C(0), `T64_ABI_VERSION_MINOR` UINT32_C(1), `T64_ABI_VERSION` a `uint32_t` constant equal to the major shifted left 16 or the minor (0x00000001 for 0.1; the header spells it as a constant expression of the two UINT32_C values), and `uint32_t t64_abi_version(void)` returning it. `t64_machine_create` accepts only an equal major and, while the major is 0, an equal minor (after a major bump: caller minor at most library minor), the mupen64plus upper-16-bit rule; any other value answers `T64_STATUS_ABI_MISMATCH`. The CMake package version is 0.1.0 with `SameMinorVersion` (the ABI candidate number, not a release version; `project(VERSION)` stays 0.0.0, no tag). The minor increments on any golden change of the freeze record before the final bind (Phase 12). | CTX locked (create-info handshake, `t64_abi_version()` query); numbers are discretion | costly |
| DR-03 | `T64_API` marks every public function declaration: the default visibility attribute under GCC and Clang, empty otherwise. The public header carries `extern "C"` guards under `__cplusplus` and includes `stdbool.h` only when not compiled as C++ (the function parameters that keep `bool` need it; no struct member does, DR-07). | CTX locked (`extern "C"` guards; one `T64_API` macro); spelling is discretion | costly (a symbol-visibility and header-text contract) |
| DR-04 | Five new public functions, 35 in total (30 today): `uint32_t t64_abi_version(void)`, `t64_status t64_machine_reset(t64_machine *machine)`, `t64_status t64_machine_unload(t64_machine *machine)`, `t64_status t64_machine_is_poisoned(const t64_machine *machine, uint32_t *poisoned_out)` and `t64_status t64_machine_output_status(const t64_machine *machine, t64_output_status *status_out)`. The poison query and destroy are the only entries that work on a poisoned instance. | CTX locked (public reset and unload; poison query) and CTX autonomous (output status call); names are discretion | costly (each is a bound symbol) |
| DR-05 | `t64_machine_create_info` is `{uint32_t struct_size, uint32_t abi_version, const t64_host_allocator *allocator, const t64_profile_descriptor *profile}`; `t64_machine_create(const t64_machine_create_info *info, t64_machine **machine_out)`. Check order: NULL `machine_out` (`T64_STATUS_INVALID_ARGUMENT`), clear `*machine_out`, NULL `info` (`INVALID_ARGUMENT`), `struct_size` below the v1 minimum (`STRUCT_TOO_SMALL`), `abi_version` (`ABI_MISMATCH`), then today's checks in today's order. | CTX locked (create-info with `struct_size` and `abi_version`, distinct mismatch status) | costly (one-way: the first-call handshake is what makes later growth safe) |
| DR-06 | The `struct_size` set and its semantics. A leading `uint32_t struct_size` goes on `t64_host_storage`, `t64_host_allocator`, `t64_profile_descriptor`, `t64_stop_request`, `t64_stop_result`, `t64_cpu_stop_detail`, `t64_output_drain_result`, `t64_video_packet_info`, `t64_state_import_report` and `t64_save_info`; `t64_audio_packet_info` already has one; `t64_machine_create_info` and `t64_output_status` are born with it. The callee never writes the `struct_size` member, reads or writes at most `min(caller, known)` bytes, and refuses a `struct_size` below the v1 minimum with `STRUCT_TOO_SMALL`, leaving state and outputs unchanged. INFO-03, an explicit and recorded exception to the CONTEXT wording "leading caller-filled `uint32_t struct_size`" (kept by owner answer A of 2026-11-25): `t64_host_storage.struct_size` is filled by the core before each allocate call, because `t64_host_storage` is an out struct the core hands to the host allocator callback, so the core is its caller; the callback writes only the data, capacity and alignment members. `t64_profile_descriptor.struct_size` is excluded from the `t64_profile_hash` and `t64_profile_serialize` byte streams, so every shipped profile hash stays. `t64_trace_record` is the one array-element exemption: it carries no `struct_size`, and `t64_machine_trace_drain` gains a `uint32_t record_size` parameter after `records_out` (refused below the v1 record size, used as the record stride otherwise). No `sType` or `pNext` chain exists. | CTX locked (leading `struct_size`; no `sType`/`pNext`); the exemption and INFO-03 are discretion | costly (one-way: struct layouts cannot grow additively after a bind) |
| DR-07 | Struct members are fixed width: every `size_t` struct member becomes `uint64_t` and every `bool` struct member becomes `uint32_t` (0 or 1). Function parameters and the allocator callback typedef keep `size_t` and `bool`: C99 `_Bool`, C++ `bool` and `size_t` are fixed by each platform ABI, the locked decision names public structs, and libretro's `retro_serialize(void *, size_t)` is the precedent. The probe shape of the bound files compiles unchanged for every such parameter and callback (section "Bound-file inventory"). | CTX locked (`bool` and `size_t` leave public structs) with the parameter reading as discretion | costly (one-way) |
| DR-08 | Retained slots: the VI and AI packet slots held in `t64_vi_state` and the AI state become internal types `t64_vi_packet_slot` and `t64_ai_packet_slot`, byte-identical to today's `t64_video_packet_info` (488 bytes) and `t64_audio_packet_info` (280 bytes), pinned by `_Static_assert` on those two sizes in `core/t64_internal.h`; the audio slot's `struct_size` value is unchanged (hashed at `core/t64_digest.c` line 1000 and restored at `core/t64_state.c` line 2092, Hazard H1). The public `t64_video_packet_info` and `t64_audio_packet_info` are copy-out structs only; the planned public layouts are 488 and 280 bytes on the probe target (Hazard H2). `struct_size` goes on the public copy-out and copy-in structs only, so every bound-file edit stays a call-site edit. | discretion (PATTERNS H1 and H2); implements CTX locked (digest and expected bytes identical before and after) | costly |
| DR-09 | Public init macros in `t64_machine.h`, brace initializers valid in C99 and C++17 (no designated initializers): `T64_HOST_ALLOCATOR_INIT(allocate, release, context)`, `T64_STOP_REQUEST_INIT(kind, value)`, `T64_STOP_RESULT_INIT`, `T64_STOP_RESULT_INIT_WITH(reason, tick)`, `T64_CPU_STOP_DETAIL_INIT`, `T64_OUTPUT_DRAIN_RESULT_INIT`, `T64_VIDEO_PACKET_INFO_INIT`, `T64_AUDIO_PACKET_INFO_INIT`, `T64_STATE_IMPORT_REPORT_INIT`, `T64_SAVE_INFO_INIT`, `T64_OUTPUT_STATUS_INIT`, `T64_MACHINE_CREATE_INFO_INIT(allocator, profile)`. They land in 11-05 expanding to today's layouts (call sites converted by directory group in 11-05 to 11-10) and change only their bodies at the flip (11-12, which fills `struct_size` with `sizeof` of the type); `T64_MACHINE_CREATE_INFO_INIT` is born at the flip and `T64_OUTPUT_STATUS_INIT` is final since 11-04. | CTX autonomous (public initializer macros land first, then a mechanical codemod, compiling commits only) | costly (public names; bodies change once, at the flip) |
| DR-10 | Poison scope: two members, `poisoned` and `in_call`, inside `struct t64_machine`, outside the digest and the state container (both emit named members, never struct bytes) and set only by one internal function at INTERNAL_DEFECT sites. Entry order for every machine-taking entry except destroy and the poison query (the 23 existing ones plus reset, unload and output status): NULL machine (`INVALID_ARGUMENT`), poisoned (`POISONED`), in_call (`REENTRANT`), then today's checks; `t64_machine_destroy` and `t64_machine_is_poisoned` have no poison guard, and destroy always releases the host allocator storage. A caller fault never poisons. The in-tree trigger is `t64_test_force_defect`, beside the four `t64_test` hooks (renamed `t64i_test_*` by DR-17). | CTX autonomous (poison scope) and CTX locked (defect class: only destroy and a poison query succeed) | costly (internal data shape, not serialized; the entry order is observable) |
| DR-11 | Defect sites (AMBIG-A and AMBIG-B) as RESEARCH lists them: `core/t64_cp0.c:581` (corrupt `count_half_clock_phase`), `core/t64_machine.c:2943` (stored profile digest no longer matches the profile bytes in `admit_rom`) and the six internal-inconsistency sites of BAD_STORAGE (`core/t64_digest.c:345,350`, `core/t64_state.c:169,214,221,335`) return `T64_STATUS_INTERNAL_DEFECT` and poison; the allocator-contract sites of BAD_STORAGE (`core/t64_machine.c:2766,2804,2836`, `core/t64_rom.c:184`) stay refused. Also a defect: a failing `reset_guest_state` after the image swap in `admit_rom` (it leaves a replaced image with partial state). `t64_machine_admit_rom(NULL, ...)` answers `INVALID_ARGUMENT` (it answers `INVALID_ROM` today; moves no digest). State export no longer clears the output buffer on a refused status (header text lines 405 to 409). | CTX autonomous (AMBIG-A, AMBIG-B, NULL machine) | costly (class-table rows are re-bindable until the final bind; the status values are not) |
| DR-12 | Handler-error masking (AMBIG-E): the poison hook in the scheduler fires only on handler statuses outside a whitelist of statuses that the existing dispatch paths return by design and mask as `RESOURCE_LIMIT` (the `RESOURCE_EXHAUSTED` backpressure signal inside `dispatch_front` is the known member); the whitelist is measured in 11-03 as a static table (status, returning function, file:line, class) and proven by zero new poisons across the 1,599-line digest sweep. | CTX autonomous (AMBIG-C/E) | reversible (an internal hook; table wording re-bindable) |
| DR-13 | Bus carve-out (AMBIG-D): `t64_machine_bus_write` is carved out of the refusal-is-a-no-op domain by a written, tested statement of exactly what a refusing device can latch (PI length and sticky error, SI sticky DMA error and a trace record, header text lines 655 to 665); address, width, alignment and range refusals change nothing; `BUS_DEVICE_REFUSED` stays in the refused class. | CTX autonomous (AMBIG-D) | reversible (a documented carve-out is re-bindable) |
| DR-14 | `T64_STOP_REASON_RESOURCE_LIMIT` wording: a resumable guest-stop, resumable after drain or yield; a stall with no progress after a full drain is permanent (three causes: the per-advance event budget, an event that cannot dispatch for VI or AI output backpressure or counter overflow, and a masked non-OK handler status). `QUIESCENT` is a guest-stop resumable by queued input only. | CTX autonomous (AMBIG-C/E; stop reasons) | reversible (table wording, re-bindable until the final bind) |
| DR-15 | Dead values stay reserved, with no renumbering: `T64_STATUS_UNSUPPORTED_ROM_ORDER` (6, never returned, the only occurrence is the `#define`) and `T64_STOP_REASON_HALT` (4, set only by the synthetic halt event scheduled through the in-tree `t64_test` hooks, so unreachable through the installed API); both are classified in the table with a reserved note (HALT: reserved, terminal until reset). | CTX autonomous (class table dead values) | reversible (nothing to migrate) |
| DR-16 | The status-class table is `docs/abi/status-classes.txt`, one row per constant, format `NAME :: VALUE :: CLASS :: RESUME :: note`; classes `ok`, `refused`, `guest-stop`, `defect`, `reserved`; RESUME `resumable`, `terminal` or `n/a`. Stop classes: `RESOURCE_LIMIT` resumable guest-stop, `QUIESCENT` guest-stop, `CPU_UNSUPPORTED` terminal guest-stop that neither poisons nor claims the guest chose to stop, documented as an emulator limitation, `GUEST_COMPLETION` terminal guest-stop. A check fails when a status or stop reason is unclassified, duplicated or renumbered. | CTX locked (three error classes, one table) | costly (the table is one of the four bound goldens) |
| DR-17 | Internal prefix `t64i_`: every cross-TU internal `t64_X` becomes `t64i_X` and every other global (the unprefixed Berkeley SoftFloat names) becomes `t64i_X`, through a generated `core/t64_prefix.h` (`scripts/gen-prefix-header.py`, `#define name t64i_name`, regenerated and diffed by a gate test), passed with `-include` to `t64_core` and included by `core/t64_internal.h` and by `t64_facade.h` so white-box tests and probes follow (`t64_core_identity` is renamed with it; the facade is not installed); the four sha256 names folded into `t64_core` are renamed to `t64i_sha256_*` for core's private copy by compile definitions (DR-20), so host tools keep `t64_sha256_*`. The nine vendored SoftFloat sources lose their three self-define guard lines each (27 lines), recorded as modifications in `provenance/copies.json`. The prefix header is generated only after the last production symbol (DR-04) is added. | CTX autonomous (symbol rename mechanism); prefix spelling is discretion | reversible (build-time rename, reversible by deleting the header) |
| DR-18 | Installed headers: only `tendies64/t64_machine.h` (plus any new public header a later plan adds). `t64_facade.h` is the synthetic-guest runner header: it stays in-tree, is relabeled (no "PROVISIONAL until Phase 10" text) and is neither installed nor part of the freeze record. | CTX autonomous (installed headers) | reversible (the installed set is re-bindable until the Phase 12 final bind) |
| DR-19 | Stack protector (CONTEXT "Stack protector", revised 2026-11-25 after the plan check): the installed-archive allowlist holds exactly the stack-protector pair `__stack_chk_fail` and `__stack_chk_guard` on every platform (compiler runtime, no host service, abort only), plus on ELF and Linux only the position-independent-code artifact `_GLOBAL_OFFSET_TABLE_` (a linker-defined address symbol, not a host service); each row carries its platform and its reason; the amendment is recorded in ADR-0006; the shipped build stays hardened and the lint's own compile stays `-fno-stack-protector`. | CTX autonomous (stack protector, revised) | reversible (one allowlist entry plus an ADR note; build with `-fno-stack-protector` to reverse) |
| DR-20 | Package recipe of RESEARCH "CMake install recipe": the sha256 sources folded into `t64_core` (no private link, so `install(EXPORT)` works) with the four sha256 names renamed privately, `tendies64Config.cmake` and `tendies64ConfigVersion.cmake` (DR-02), the in-tree alias `tendies64::core`, export name `core`, `tendies64_BUILD_ID` and `tendies64_BUILD_TYPE` recorded in the config for the consumer to compare with `t64_build_identity()`, install component `Development` (the archive, the one public header and the CMake files; 6 files), relocatable through `_IMPORT_PREFIX`. | CTX locked (installability) | reversible (CMake recipe) |
| DR-21 | Phase 11 CTest names by plan, as the plans name them on 2026-11-25 (the `PHASE11_TESTS` block of `scripts/check-test-inventory.sh` is the exact list and the registering plan is authoritative for spelling): 11-02 `repo.abi-header`, `repo.abi-status-classes`, `repo.abi-selftest`, `unit.core.api.abi-version`; 11-03 `unit.core.api.poison`, `.reentrant`, `.refusals-never-poison`, `.defect-sites`, `.null-machine`, `.bus-carve-out`; 11-04 `unit.core.api.reset`, `.unload`, `.not-ready-table`, `.output-status`; 11-10 `repo.api-init-guard`, `repo.api-init-guard-selftest`; 11-11 `repo.abi-prefix`, `repo.stale-freeze-text`, `repo.stale-freeze-text-selftest`; 11-12 `unit.core.api.handshake`; 11-13 `unit.core.api.struct-size`; 11-16 fixture `install.setup`, `installed.consumer.build`, `installed.consumer.smoke`, `repo.consumer-include-guard`, `repo.consumer-include-guard-selftest`; 11-17 `install.abi-header`, `install.abi-layout`, `install.abi-symbols`, `install.abs-paths`, `repo.abi-layout`, `api04.members`, `api04.headers`, `api04.disasm`, `api04.seeded`; 11-18 `installed.consumer.composite`, `.literals`, `.three-way`; 11-19 the host-loop cases and one `installed.controls.<id>` per row of `tests/installed-consumer/controls.tsv`; 11-20 `installed.consumer.sequence`; 11-22 `repo.installed-case-map` and the W2-01 boundary case; 11-23 `repo.abi-freeze-record`, `repo.abi-freeze-selftest`; 11-24 `harness.phase11-qualification`. Every new name carries LABELS `<kind>;phase11`, a COST equal to its measured native seconds rounded up (checker W-11) and a TIMEOUT of at least 3.5 times its native seconds and at least 2 times its COST (F-108). | discretion (names); CTX autonomous (gate budget) | reversible |
| DR-22 | Deviation DEV-11-01 (corrected after the plan check, W-4): from the flip commit of 11-12 to the bound commit of 11-14 the nine bound test executables do not build in the shared tree, because the owner approval requires the bound edits as one isolated commit placed last in the shape wave and no header can accept both the old positional forms and the new layouts. The commits in that window are not consecutive: the 11-12 flip and its isolated no-HLE refresh, PIN-IMPACT planning commits, the 11-13 prologue commit and its refresh, and the 11-14 re-proof record precede the bound commit, and the 11-14 checkpoint pauses the window until the orchestrator commits. At every commit in the window every other target builds and passes, and before every code commit in it the full gate including the nine bound tests passes in a marked scratch clone with the proven bound patch applied (W-3). Nothing is pushed or gated in the window: the first push is 11-15, after the bound commit, so every pushed head compiles. `git bisect` across the window is unsupported for the nine bound tests (a bisect step inside it must apply `build/11-12-bound/bound-callsites.patch` or be skipped); the CONTEXT rule "compiling commits only" holds for every commit outside the window and for every target other than the nine executables inside it. | discretion (deviation from CTX "compiling commits only", recorded) | costly (it bounds the bisect-safe history) |
| DR-23 | Hosted footprint: the cheap-first checks and the install-from-build-tree CTests run in the existing gate on hosted macOS and Linux; the fuller fresh-archive install (`scripts/check-install.sh --fresh-archive`, run by the qualifier) is a local lane; no new workflow file and no timeout change. The act image has no cmake, so it covers only the compile, `nm` and lint-objects probes; the Linux install path is unverified until the first hosted run that carries the install names (the push of 11-21). | CTX autonomous (hosted footprint) | reversible |
| DR-24 | G4 position: new checks are CTest names with a COST inside the existing gate, no `ci.yml`, `check-ci-yml.sh` or `gate.sh` edit; sanitizer, TSan and fresh-archive runs stay local; a Phase 11 G4 exception for owner review is recorded in the "Gate-growth ledger" below with its reversal rule. | CTX autonomous (gate budget) | reversible |
| DR-25 | Carry-in triage: every Phase 10 carry-in and Phase 9 item is triaged as done, deferred or dropped, with one disposition, a reason and an owning plan, in section "Carry-in triage" below (13 rows: W2-01, W2-02, DEF-W1-01, W1-03, D-10-17-01, T1 to T8). | CTX locked (carry-ins triaged, none blocks install work) | reversible |
| DR-26 | The paper LibFuturo contract read of this phase is recorded in section "API-07 input: LibFuturo paper read" below: a local, unpublished commit read from committed objects only; no locked decision needs amending; any adapter is optional and does not shape the core API; the written evaluation is Phase 12. | CTX locked (paper read before the candidate; API-07 input) | reversible |
| DR-27 | Call-site rule: a bound-file edit is allowed only as category (a), a public API call expression, or category (b), the initializer or declaration of an object passed to or received from one public call (init macro or `struct_size` set). Reading of the approval text (checker I-1): it says "the nine bound test files" and lists ten paths; the nine API-using files (`core/tests/test_rdp{,_copy,_triangles,_texture,_combiner,_blender,_depth}.c`, `test_vi.c`, `test_video_output.c`) are edited and `runner/tests/test_rdp_video.c`, listed but needing no edit, keeps its hash. Extended by owner answer A of 2026-11-25 to the single value-neutral category (c) hunk `core/tests/test_rdp.c` line 486 (`t64_host_storage image = {NULL, 0u, 0u};` becomes `{0}`); the approval is not widened beyond it. | CTX locked (owner approval 2026-11-25, extended by answer A) | costly (changes the authority checker's pins) |
| DR-28 | Threading statement: distinct instances are independent; one instance needs external serialization (the in-call flag turns a reentrant call from an allocator callback into a refusal, DR-10, but concurrent calls on one instance remain undefined); checked by a two-thread, one-instance-each TSan consumer run in a local lane. | CTX autonomous (thread statement) | reversible (documentation plus an optional test) |
| DR-29 | Package smoke: the plain `cc` line is done (11-19); the `.pc` file is dropped (pkg-config is not verified on both runners and a second relocatable metadata file would join the freeze review). | CTX autonomous (package smoke: optional, only if it costs under one task) | reversible |
| DR-30 | `t64_profile_descriptor` takes a leading `struct_size`, not getters: 303 lines in 79 files copy a looked-up descriptor and edit its fields, so getters would rewrite each of them (the descriptor type is mentioned on 392 lines in 82 files by the "Public-struct call-site counts" measure), and the descriptor's `struct_size` stays out of the profile byte streams (DR-06). | CTX autonomous (descriptor struct: struct_size or getters, the planner picks the cheaper) | costly (one-way once bound) |
| DR-31 | CONTEXT amendment record: the "Stack protector" decision was revised by the orchestrator on 2026-11-25 after the plan check to add the ELF-only `_GLOBAL_OFFSET_TABLE_` row (research measured the Linux need: archive-level undefined set `_GLOBAL_OFFSET_TABLE_`, `__stack_chk_fail`, `memcmp`, `memcpy`, `memset` on x86_64 gcc 13.3); DR-19 implements the revised text and plans 11-11 and 11-17 cite it. | orchestrator amendment 2026-11-25 (CONTEXT revised) | reversible |
| DR-32 | Composite three-way scope (checker B-1): `composite.expected.json` is the independent source of the video packet bytes, the audio packets, the save, the transcript and the poll state; the completion tick, the stop reason, the final state digest and the video packet count have no independent source and are committed as `fixtures/composite/composite.runner-pins.json`, labeled "runner-recorded pin, not independent" with the citation Phase 10 PIN-IMPACT.md section 11.3 (plan 10-14 supervised runs). The installed consumer agrees with the in-tree runner on all values and with the independent literals on the values that have an independent source; no claim calls the runner-recorded pins independent. | CTX locked (three-way agreement) refined by the checker finding B-1 | reversible |

## Carry-in triage

Written by plan 11-01 Task 3 (DR-25). Every Phase 10 carry-in (Phase 10 PIN-IMPACT.md sections
13 and 13.1 and `deferred-items.md`) and every Phase 9 item tagged for this phase ends in done,
deferred or dropped with one disposition per row, a reason and an owning plan, so the freeze
candidate inherits nothing silently. The carry-ins are judged by the freeze-candidate review;
none blocks the install work. Plan 11-22 updates each row with its final commit and states the
final G3 count; the dispositions below are the plan-time decisions. Evidence tier: code
observations at the Task 1 commit and the Phase 10 records; nothing here is a compatibility or
hardware claim.

| Item | Origin | Disposition and owning plan | Reason |
|------|--------|-----------------------------|--------|
| W2-01 | Phase 10 Wave 2 review (`10-REVIEW.md`, `10-13-SUMMARY.md`) | done in 11-22 (boundary test plus two mutant rows) | The Z product bound `state_z_products_bounded` (`core/t64_state.c:1490`) and its parity case (`core/tests/test_state_import_v26.c:1295`) are unpinned: two perturbed bounds (limit `INT64_MAX / 2`, divisor `y_bound + 1`) survive every committed test. The fix is a committed bisection boundary case plus two rows of `scripts/p10-mutants.py`, with no core edit, and the freeze-candidate review judges pin strength of a state path the candidate exposes. |
| W2-02 | Phase 10 Wave 2 review | done in 11-11 | `t64_has_si_v25_profile` and `t64_has_si_v26_profile` become tokens of `scripts/list-membership-sites.py` and the regenerated `docs/spec/profile-membership-sites.txt` gains its six missing site rows and three details, in the shape wave because the list moves with edited bodies anyway; the script is a Phase 9 bound path, so `harness.phase09-qualification` runs after the commit. |
| DEF-W1-01 | Phase 10 Wave 1 review (W1-01), `10-08-SUMMARY.md` | deferred to v1.3 (no owning plan in Phase 11) | Three sibling copies of the perspective and major-edge floor (`core/t64_rdp_tmem.c:1218-1221`, `core/t64_rdp_raster.c:495` and `:556-557`) are unpinned behaviorally and sit in sealed RDP bodies; the fix is tests and mutants only. It has no API effect and VER-06 does not require it (VG-01 to VG-05 and the NTSC vector are closed), so it counts against G3 and waits. |
| W1-03 | Phase 10 Wave 1 review | done in 11-22 | One sentence stating the `ceil` rule in the v8 clock section of `docs/spec/vi.md` (the NTSC comment cites a section that does not state the rounding); docs only. |
| D-10-17-01 | Phase 10 `deferred-items.md` | done in 11-22 (per-process suffix) | `make_evidence_dir` in `runner/tests/test_cold_boot.c` (lines 124 to 139) caps attempt directories at 99 per build tree and stops `integration.cold-hello` and `integration.cold-startup` after 99 gate runs; the cap is replaced by a directory name carrying the process id and a counter, which moves the runner test identity only. |
| T1 | Phase 9 triage (Phase 10 section 13) | deferred beyond v1.2 (no owning plan) | Splitting `unit.core.state.import` changes the case list of a binary that `docs/spec/phase09-cases.json` inventories, and that file's digest is a closed F2 pin of `harness.phase09-qualification`. The hosted cost fits the limit (it starts 2nd by `COST`; the measured cold gate-macos run is 731 s of 900 s), so the Gate-growth ledger reversal rule, not a split, is the trigger. |
| T2 | Phase 9 triage | deferred to Phase 12 | The replay-driver duplication in `runner/t64_replay.c` has no state or digest effect, and the host pump of Phase 12 reshapes the host loop, so unifying it before the pump exists would be redone. |
| T3 | Phase 9 triage | done in Phase 10 (commit `3deee7d`, the dead `t64_artifact_publisher.c` link removed from the `test_replay` target; the case inventory did not change) | Closed in Phase 10; recorded here so every Phase 9 item has one disposition. |
| T4 | Phase 9 triage | done in 11-11 (the 20 stale "until Phase 10" production comments, 8 files, relabeled) | Editing a `core/` comment moves `T64_BUILD_ID`, so the relabel rides the shape wave; the check is `git grep -n -i -E 'until (the )?Phase 10' -- core runner tools` printing nothing. |
| T5 | Phase 9 triage | deferred to Phase 12 | Closed detail names for the recorder failures need a result-protocol vocabulary change and a checker change in the runner, which is not installed and not in the freeze record; no API effect. |
| T6 | Phase 9 triage | deferred to Phase 12 | `T64_REAL_KEEP_WORKSET` (`runner/t64_supervisor.c:1140`) is a runner-internal production supervisor name (gating it broke two Phase 8 tests, FX-18); the runner is not installed nor in the freeze record, so the API review does not own it. One disposition, as the plan check required (I-7). |
| T7 | Phase 9 triage | done in 11-11 | The third SI trace kind (`T64_TRACE_DEVICE_SI`, `T64_TRACE_KIND_SI_*`, `core/t64_si.c:15-19`) gets its public name, or a recorded reason it is not public, in the same wave as the relabel, because a core name edit moves the build id and the names are part of the provisional set until the candidate. |
| T8 | Phase 9 triage | done in 11-22 (for the Phase 11 case lists) | `repo.installed-case-map` proves the installed consumer's `--list-cases` set, `tests/installed-consumer/cases.txt` and the `installed.consumer.*` CTest names agree; the Phase 9 case inventory is not touched (a checker change with no state effect). |

G3 (at most 5 carried warning-level gaps): the count carried out of Phase 10 is five (W2-01,
W2-02, DEF-W1-01, W1-03, D-10-17-01). At plan time Phase 11 closes four of them (W2-01, W2-02,
W1-03, D-10-17-01) and carries one (DEF-W1-01, to v1.3); T1, T2, T5 and T6 are Phase 9 info
items deferred with reasons and were never counted in the Phase 10 G3 figure. Plan 11-22
states the final count after the closures are committed.

## API-07 input: LibFuturo paper read

Written by plan 11-01 Task 3 (DR-26). This is the paper read of the LibFuturo contract that
CONTEXT places before the freeze candidate; the written API-07 evaluation is Phase 12.

Source and pin. The sibling LibFuturo repository was read at commit
`4177a43e67312e2db88ab21cba13aa7350063105` (2026-11-23, "docs: finalize browser-host v2.4
state"). The pin is a local SHA and is not published: the checkout has no configured remote and
research found no public page for it, so the evaluation pin is "not published" and unverified
as published. The checkout is located through the `LIBFUTURO_DIR` environment variable and its
path is deliberately not recorded here. Re-verified 2026-11-25 with `git cat-file -e` on the
commit (present, type commit) and by reading `include/libfuturo/libfuturo.h` (347 lines) and
`docs/contract.md` (430 lines) with `git show` at that commit: committed objects only, never
the working tree (research saw 12 uncommitted lines in the header there, which are not read).
Nothing was written to that checkout. The earlier planning reference `a28cccd` is 550 commits
older, so Phase 12 takes the pin again.

What the contract says, against our decisions (in our words):

| Aspect | LibFuturo at `4177a43` | Relation to our decisions |
|--------|------------------------|---------------------------|
| Instance model | `lf_create(profile_id, options, &out)` and `lf_destroy(&owned)`, which consumes and clears the handle; the host owns and serializes the handle (LC-LIFE-01); allocator callbacks are `void *allocate(ctx, bytes)` and `deallocate` | Compatible. Our create-info adds alignment and a storage descriptor; no conflict. |
| Admission | `lf_load(content, configuration_id)` borrows the span; a rejected load keeps the previous session and an unloaded instance stays unloaded (LC-LOAD-01) | Matches our transactional admit. It has no unload or reset call; we add both. |
| Errors | `enum lf_status` with six values (`LF_OK` to `LF_RESOURCE_EXHAUSTED`) and no classes; LC-ERROR-01 tells callers to use the operation's documented postcondition and not to infer rollback from a status name | Our three-class table is stricter. An adapter maps refused to the nearest value; LibFuturo has no defect or poison shape, so an adapter must report poison as a distinct failure. |
| ABI hygiene | `size_t struct_size` first in each struct, enums for statuses and stop reasons, `extern "C"` present; the contract calls the API experimental and not a stable binary ABI ("rebuild against the matching header") | Our locked `uint32_t struct_size` and `uint32_t` status constants differ from `size_t` and enums; an adapter converts and our decisions do not change. |
| Timing and input | Work is counted in per-profile units (frame, synthetic step, source tick); input is sampled only at the start of an advance (LC-INPUT-01, LC-TIME-01); stop reasons are budget reached, rejected and core halted; cadence 0/0 is allowed | Our input stamped to guest ticks cannot be expressed; an adapter is lossy by design, and CONTEXT forbids it from shaping the core API. |
| Output | Caller-provided destination spans inside the run request; video formats GRAY8, RGB24 and INDEX8 only, one audio rate; the library must not retain output pointers (LC-OWN-01) | Our copy-out rings convert at the adapter; N64 RGBA8 is not representable. The no-retained-pointer rule matches our copy-out ring semantics. |
| Media and snapshot | `lf_copy_media` and `lf_import_media` with a `guest_generation` token, and snapshot size, capture and restore bound to an exact build identity (LC-HOST-01, LC-SNAPSHOT-01 to 03) | Ours: save export and import with `write_generation`, and state export and import bound to the build identity. Comparable, not shared. |
| Threading | The host serializes all calls on an instance; no concurrency promise | Same as DR-28. |

Conclusion for API-07: nothing in the LibFuturo contract requires amending a locked decision,
so no locked decision needs amending; any adapter is optional and does not shape the core API;
the expected no-adapter-now conclusion holds. Two facts carry into the Phase 12 text: LibFuturo
promises no stable ABI itself, and it has no defect class, so an adapter has no failure shape
for poison. The written evaluation and the retaken pin are Phase 12 (API-07).

## Gate-growth ledger

Written by plan 11-01 Task 3 (DR-24). ROADMAP guardrail G4 says the required gate may not grow
without removing equal time, and the Phase 10 G4 exception "covers Phase 10 only", so Phase 11
either offsets equal time or records its own exception for owner review. This ledger starts
from the measured Phase 10 cold run and measures growth instead of projecting it: one row per
wave is appended by the review plans (11-15 for the SHAPE wave, 11-21 for the INSTALL wave,
11-25 for the final audit and qualification). Row columns are those of Phase 10 section 6: the
wave, the new CTest names, the measured added native seconds (from
`build/gate/Testing/Temporary/CTestCostData.txt`), the hosted macOS ctest delta against the
baseline, the offset or the G4-exception reason, and the remaining margin to the 900 s
gate-macos limit.

| Wave | New CTest names | Measured added native seconds | Hosted macOS ctest delta | Offset or G4-exception reason | Margin to 900 s |
|------|-----------------|-------------------------------|--------------------------|-------------------------------|-----------------|
| Baseline (Phase 10 close; cold run 45550277510 at head `f081d39`, plan 10-19; later warm samples run 45650890027 at head `85ba477`) | none: 254 CTest names (30 `PHASE10_TESTS`) | not applicable (native `ctest --preset gate` 76.18 s wall at the last Phase 10 measurement; the whole local gate stage took 143 to 156 s) | ctest 265 s cold and 194 s warm on 254 names; the run-to-run spread on identical bound source is up to 52 s (F-119) | not applicable: the Phase 10 exception covers Phase 10 only | measured cold gate-macos 731 s of the 900 s limit, margin 169 s (18.8 percent); warm 368 s, margin 532 s; gate-linux 254 s (F-124, F-126) |
| Projection of the Phase 11 names (RESEARCH "CI and time budget"; not a measurement) | the cheap-first set and the install and consumer names of DR-21 (the plans name 44 explicitly; the 24 installed-consumer cases and the control rows of 11-19 are counted by their data files, so an estimate of 60 to 80 names by the end of the phase), first 11-02 | about 10 to 15 s native summed for the cheap-first set (header compile 0.5 s, layout asserts 0.3 s, `nm` golden 0.1 s, lint-objects 3.1 s, objdump 0.9 s, install under 1 s, consumer configure, build and run about 3 s each), plus about 0.2 s of `check-test-inventory.sh` selftest cost per name | hosted multiplier 2.8 to 4.3 (Phase 10 section 6.1) gives 30 to 65 s of summed test time, about 8 to 16 s of 4-job wall | see the exception below | projected cold margin 169 s minus about 8 to 16 s, above the 120 s warning level carried from plan 10-17; the measured numbers replace this row at 11-15 and 11-21 |

Phase 11 G4 position (DR-24) and exception for owner review. No offset is claimed. The new
CTest names are the required evidence of Phase 11 success criteria SC1 to SC5 (the installed
package, the independent consumer and its negative controls, the refusal-is-a-no-op and
call-sequence checks, the API-04 object checks and the bound freeze candidate), so their cost
is required Phase 11 gate evidence and not removable without removing the evidence. Everything
heavy stays outside the hosted gate: sanitizer runs, TSan, the fresh-archive install, the
three-way re-run with the long cap and the qualifier's wall are local or qualifier stages. No
`ci.yml`, `check-ci-yml.sh` or `gate.sh` edit is planned, so no second round of bound-source
edits arises, and every new name carries a `COST` and a `TIMEOUT` by the standing rule. This is
a recorded exception for owner review, not an owner approval, and it grants no oracle, rights
or publication authority.

Reversal rule of the Phase 11 G4 exception: if a review-plan hosted measurement (11-15, 11-21
or 11-25) projects a cold gate-macos run above 900 s, or a hosted run is cancelled at the
limit, a new numbered plan moves non-essential Phase 11 names (sampled sanitizer-style
repeats and duplicate-path controls) to a non-gating local lane and the measurement is
repeated. A review plan that projects less than 120 s of margin reports it as a warning in its
ledger row. The first measurement is the SHAPE-wave review (11-15), whose push is the first
hosted run of this phase.
