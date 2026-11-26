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
- Install base: 06e31ecab5b84c08996aba79cb3ab6e29bb021cc

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
| DR-12 | Handler-error masking (AMBIG-E): the poison hook in the scheduler fires only on handler statuses outside a whitelist of statuses that the existing dispatch paths return by design and mask as `RESOURCE_LIMIT` (the `RESOURCE_EXHAUSTED` backpressure signal inside `dispatch_front` is the known member); the whitelist is measured in 11-03 as a static table (status, returning function, file:line, class) and proven by zero new poisons across the 1,599-line digest sweep (recorded in section "DR-12 record" below). | CTX autonomous (AMBIG-C/E) | reversible (an internal hook; table wording re-bindable) |
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

### DR-12 record: handler-error whitelist, defect sites and measured set (plan 11-03)

Written by plan 11-03 Task 3 before the scheduler hook was edited (the static derivation and
the measurement both precede the code change). Evidence tier: unit-tested, authored scope; a
software-interface claim only.

**Handlers.** `dispatch_front` (`core/t64_scheduler.c`) returns the status of
`t64_cp0_synchronize_guest_time` (`core/t64_cp0.c:558`) or of `event.handler`. The closed handler
table of `t64_state_handler_lookup` (`core/t64_state.c:815`) names the ten production handlers:
`t64_cpu_step` (`core/t64_cpu.c:2106`), `t64_pi_complete` (`core/t64_pi.c:286`), `t64_sp_complete`
(`core/t64_sp.c:921`), `t64_rsp_issue_event` (`core/t64_rsp.c:1341`, reached through
`t64_rsp_issue_event_handler`), `t64_rdp_event` (`core/t64_rdp.c:776`), `t64_vi_event`
(`core/t64_vi.c:1164`), `t64_ai_event` (`core/t64_ai.c:457`), `t64_input_event_handler`
(`core/t64_observe.c:87`), `t64_si_execute_event` (`core/t64_si.c:266`) and
`t64_si_complete_event` (`core/t64_si.c:291`). The five synthetic test handlers
(`core/t64_scheduler.c:28-60`) return OK. The set of statuses each can return was derived by a
throwaway scratch script (not committed) that walks every function body reachable from the handler
(comments and strings stripped) and lists each `T64_STATUS_*` token with its function and line; the
full per-site list is retained locally in `build/gate/11-03-task3/whitelist-sites.txt` and the
per-status summary below carries every non-bus site and a per-file count for the bus statuses.
Over-approximation: a callee that converts a status into a guest-visible effect without returning
it is still counted, so the whitelist can only be wider than the truth, never narrower.

**Static whitelist.** Statuses found in the closure of the eleven functions (12 values; the other
27 values of 0 to 38 never occur in it):

| Status | Class | Returning function and file:line (sites in files) |
|--------|-------|---------------------------------------------------|
| RESOURCE_EXHAUSTED | whitelisted: backpressure, CPU stop signal, DMA range outcome | 50 sites in 8 files: `t64_cpu_step` returns it at `t64_cpu.c:2157-2575` when it stops the CPU or cannot continue (18 sites), `t64_pi_complete:309,321`, `t64_sp_complete:940,962`, `t64_vi_event:1190,1201,1208`, `t64_ai_event:484`, `t64_si_complete_event:307`, `start_dma` of pi (`t64_pi.c:101,102`), sp (`t64_sp.c:256`) and si (`t64_si.c:163`), `vi_v8_event` (`t64_vi.c:958-1143`), `t64_save_sram_dma_apply:226`, `t64_schedule_cancel:239` |
| QUEUE_FULL | whitelisted: event queue exhaustion | 6 sites: `t64_schedule_reserve:107`, `t64_schedule_check_capacity:163` (`t64_scheduler.c`), `t64_vi_event:1186`, `vi_v8_event:990`, `t64_ai_event:488`, `submit_dma:206` |
| SEQUENCE_EXHAUSTED | whitelisted: sequence counter exhaustion (terminal until reset) | 10 sites: `t64_schedule_reserve:110,113`, `t64_schedule_check_capacity:173`, `t64_ai_event:493,502`, `activate_queued_descriptor:134`, `submit_dma:209,225`, `t64_rsp_schedule_next:22`, `t64_sp_rsp_dma_preflight:484` |
| TICK_OVERFLOW | whitelisted: tick counter overflow | 6 sites: `t64_schedule_reserve_after:142`, `t64_schedule_check_capacity:176`, `reserve_sample_event:100` (ai), `start_dma:108,109` (pi), `decode_descriptor:148` (sp) |
| TIME_REVERSED | whitelisted: event or synchronization time before the machine time (can follow earlier state changes inside a dispatch) | 3 sites: `t64_cp0_synchronize_guest_time:577`, `t64_schedule_reserve:104`, `t64_schedule_cancel:246` |
| NOT_READY | whitelisted: absent or not ready device (AI or SI register access on a profile without the device, no image, an unfound event to cancel, an unsupported softfloat operation) | 8 sites: `t64_ai_read_register:260`, `t64_ai_write_register:304`, `t64_si_read_register:104`, `t64_si_write_register:245`, `t64_rsp_ensure_opportunity:45`, `t64_schedule_reserve:101`, `t64_schedule_cancel:245`, `t64_softfloat_execute:146` |
| BUS_UNMAPPED, BUS_UNSUPPORTED, BUS_ALIGNMENT, BUS_RANGE, BUS_DEVICE_REFUSED | whitelisted: guest bus access and device refusal outcomes | 13, 97, 13, 25 and 126 sites: `t64_bus.c` 7/26/5/14/11, `t64_rdram.c` 0/14/0/0/27, `t64_sp.c` 0/5/0/0/26, `t64_rsp.c` 0/0/0/0/14, `t64_vi.c` 0/6/2/0/13 and the cache, cpu, memory, isviewer, mi, pi, rdp, save, si, ai and warm-device files |
| INVALID_ARGUMENT | internal invariant: poisons | 78 sites in 22 files, all argument preconditions that no guest input reaches: NULL `machine`, `event` or output pointers (`t64_pi_complete:293`, `t64_sp_complete:932`, `t64_rdp_event:789`, `t64_vi_event:1178`, `t64_ai_event:473`, `t64_input_event_handler:91`, `t64_si_execute_event:272`, `t64_si_complete_event:297`, `t64_rsp_issue_event:1423`, `t64_cpu_step:2154` and the bus, cache, memory, device-register and scheduler entry checks), widths and masks the CPU derives from decoded instructions (`validate_access` `t64_bus.c:205`, `t64_bus_write_masked:790,795`, `t64_cpu_dcache_write:380,385`), register addresses already decoded into a device window by the bus (`t64_pi_read_register:164`, `t64_sp_write_register:858,874`, `t64_mi_register_write:124`), unreachable switch defaults (`t64_cpu_cache_operation:634`) and the `T64_CPU_ATTEMPT_HOST_ERROR` returns of `t64_cpu.c` (`1390,1697,1709,1740,1750,2208`) |
| INTERNAL_DEFECT | internal invariant: poisons | returned only by `t64_internal_defect`; poisons at the call site and again in the scheduler hook |
| every other value (BAD_STORAGE, STATE_*, SAVE_*, ABI_MISMATCH, STRUCT_TOO_SMALL, REENTRANT, POISONED, ...) | internal invariant: poisons | no handler or callee returns them; a future return would be an invariant violation |

The hook (`handler_status_is_whitelisted` and `handler_status_is_defect`, `core/t64_scheduler.c`)
checks the status in both masking blocks of `advance_time_bound` and `advance_counter_bound`: a
status of the whitelist keeps today's `RESOURCE_LIMIT` stop (or the CPU stop reason when the CPU
stopped); `INTERNAL_DEFECT`, or any non-OK status outside the whitelist with the CPU not stopped,
calls `t64_internal_defect` and `advance` returns `INTERNAL_DEFECT` with `result_out` untouched.

**Measured set.** Command: a throwaway edit (not committed, reverted by `git apply -R`) of the two
masking blocks that appended `<block> <status> <cpu stopped>` to the file named by an environment
variable, run over (1) `ctest --preset gate` at commit `7592675` (262 of 262 tests passed) and (2)
`python3 -B scripts/digest-sweep.py --base <Wave 0 base> --head WORKTREE` (1,599 shared lines, 0
differences) with the same variable set (the head printer is the instrumented tree):

| Run | Status (value) | Count | CPU stopped | Whitelisted |
|-----|----------------|-------|-------------|-------------|
| ctest gate | RESOURCE_EXHAUSTED (14) | 262 + 4 | 262 yes, 4 no | yes |
| ctest gate | SEQUENCE_EXHAUSTED (12) | 6 | no | yes |
| ctest gate | QUEUE_FULL (11) | 2 | no | yes |
| digest sweep | RESOURCE_EXHAUSTED (14) | 97 | 97 yes | yes |

Measured set {11, 12, 14} is a subset of the static whitelist; no INVALID_ARGUMENT, no non-whitelisted
status and no status 37 or 38 reached either masking block. Counts per status: 274 lines over the
gate run, 97 over the sweep.

**Zero new poisons, proven after the hook.** `python3 -B scripts/digest-sweep.py --base <Wave 0
base> --head WORKTREE` after the hook reports `0 differences` and the head output holds no
status 37 or 38 (the line is recorded in "Shape steps"); the full `ctest --preset gate` passes 263 of
263. `unit.core.api.defect-sites` drives one real handler per whitelisted value that a public
sequence can reach, through `dispatch_front`, and asserts today's stop reason and a clear poison
query: `cpu-stop` (the boot asset runs to `CPU_UNSUPPORTED`, 400 advances), `sequence-exhausted`
(narrow sequence width set white-box, the CPU step handler cannot reschedule), `queue-full`
(queue capacity set white-box to the count of far-future synthetic events), `tick-overflow`
(machine tick set to `UINT64_MAX - 1`), `time-reversed` (CP0 synchronization tick ahead of the
event) and `pi-refusal` (authored PI register stores; the device refuses a source beyond the cart
window, a bus outcome that never poisons, and the machine then advances). Not driven by an
authored program, and why: NOT_READY and the BUS_* values reach `dispatch_front` only through
guest CPU or RSP stores that this test layer has no program loader for, and the PI and SP
completion ranges cannot be made invalid after a valid start without editing the image
mid-flight; they stay covered by the static whitelist alone, which is the conservative side.

**Defect sites (DR-11) and their triggers.**

| Site | Now returns | Trigger in `unit.core.api.defect-sites` |
|------|-------------|-----------------------------------------|
| `core/t64_cp0.c:584` (count half-clock phase above 1) | INTERNAL_DEFECT, poisons | `defect-cp0-phase`: phase set to 7, then `advance` |
| `core/t64_machine.c:2987` (stored profile digest or pinned-id re-check in `admit_rom`) | INTERNAL_DEFECT, poisons, before any allocation | `defect-profile-digest`: one byte of the stored digest flipped (admitted and not admitted) |
| `core/t64_machine.c:3040` (failing `reset_guest_state` after the image swap) | INTERNAL_DEFECT, poisons, after every host allocation is released | no trigger: `reset_guest_state` can fail only through `t64_cpu_schedule` on a queue that was just emptied; covered by `t64_test_force_defect` and by the entry-guard cases |
| `core/t64_digest.c:345` (coverage table missing, v25 framing) | INTERNAL_DEFECT, poisons | `defect-sites`: `rdp_memory_coverage` set to NULL, through `t64_machine_digest` and through `t64_machine_state_export` (buffer untouched) |
| `core/t64_digest.c:350` (hidden depth table missing, state 26) | INTERNAL_DEFECT, poisons | same case on the si-v2 profile with `rdp_depth_hidden` NULL |
| `core/t64_state.c:171` (`plan_layout`, markers out of order) | INTERNAL_DEFECT, poisons, before any write | no trigger: the walker would have to record markers out of order; force hook only |
| `core/t64_state.c:224,231` (`emit_canonical`, stream differs from the plan) | INTERNAL_DEFECT, poisons | no trigger: the walker would have to stream differently on consecutive passes over a const machine; force hook only |
| `core/t64_state.c:389,399` (`layout_is_writable`, container writers) | INTERNAL_DEFECT, poisons | no trigger: geometry derived from the same layout; force hook only |

The allocator-contract sites of BAD_STORAGE (`core/t64_machine.c` create and admit, `core/t64_rom.c`)
are unchanged and stay refused.

**State export order (outputs untouched).** The checks that need no write now run before the first
byte of the caller buffer changes: `plan_layout` (no write), then a verify pass of the canonical
region (`emit_canonical` with a NULL buffer: hash only, markers and length against the plan), then
`layout_is_writable` (the geometry every writer relies on, with three `_Static_assert`s that the
fixed container sizes hold the stored fields), and only then the header and table are zeroed and the
write pass copies the region. The three post-write checks that remain (`emit_canonical` in the write
pass and the four writers) can fire only if the const machine streams differently on two
consecutive passes; they poison, return INTERNAL_DEFECT and clear the first `total` bytes so a
defect never publishes a partial container. No refused status reaches the write path, so state
export no longer clears the buffer on a refusal (`export-refusal-untouched`: BUFFER_TOO_SMALL,
NOT_READY and an unsupported profile leave a 4,096-byte sentinel intact). This keeps the locked
rule for every defect that can be found without writing; the residual post-write branch is the
recorded exception and has no input that triggers it. Cost: one more pass over the canonical region
on export (plan, verify, write instead of plan, write), measured as not observable in the export
tests.

**Known limit, for the review plan 11-15.** A caller-supplied descriptor that `t64_machine_create`
accepts but that cannot schedule its first CPU event (for example `event_queue_capacity` 0) makes the
admission fail in `reset_guest_state` after the image swap. That outcome used to be a refusal
status with a half-reset instance; it is now INTERNAL_DEFECT with a poisoned instance, which is the
honest class for a state that cannot be rolled back, but the cause is a caller fault that create
does not refuse. Refusing such descriptors at create is a one-line follow-up of the next plan
that touches `profile_fields_are_valid` (not done here: it changes the set of accepted
descriptors).

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
| W2-02 | Phase 10 Wave 2 review | done (11-11): commit `e9e0cbe`; the regenerated list gains the six missing site rows (`t64_has_si_profile` in `core/t64_internal.h` and the five importer sites `state_commit`, `state_domain_for`, `state_parse_table`, `state_read_rdp` and `state_read_rdp_carried` in `core/t64_state.c`) and three changed detail columns (`t64_digest_stream`, `t64_has_rdp_depth_profile`, `state_version_registered`); `repo.si-membership-sites` and `harness.phase09-qualification` pass | `t64_has_si_v25_profile` and `t64_has_si_v26_profile` become tokens of `scripts/list-membership-sites.py` and the regenerated `docs/spec/profile-membership-sites.txt` gains its six missing site rows and three details, in the shape wave because the list moves with edited bodies anyway; the script is a Phase 9 bound path, so `harness.phase09-qualification` runs after the commit. |
| DEF-W1-01 | Phase 10 Wave 1 review (W1-01), `10-08-SUMMARY.md` | deferred to v1.3 (no owning plan in Phase 11) | Three sibling copies of the perspective and major-edge floor (`core/t64_rdp_tmem.c:1218-1221`, `core/t64_rdp_raster.c:495` and `:556-557`) are unpinned behaviorally and sit in sealed RDP bodies; the fix is tests and mutants only. It has no API effect and VER-06 does not require it (VG-01 to VG-05 and the NTSC vector are closed), so it counts against G3 and waits. |
| W1-03 | Phase 10 Wave 1 review | done in 11-22 | One sentence stating the `ceil` rule in the v8 clock section of `docs/spec/vi.md` (the NTSC comment cites a section that does not state the rounding); docs only. |
| D-10-17-01 | Phase 10 `deferred-items.md` | done in 11-22 (per-process suffix) | `make_evidence_dir` in `runner/tests/test_cold_boot.c` (lines 124 to 139) caps attempt directories at 99 per build tree and stops `integration.cold-hello` and `integration.cold-startup` after 99 gate runs; the cap is replaced by a directory name carrying the process id and a counter, which moves the runner test identity only. |
| T1 | Phase 9 triage (Phase 10 section 13) | deferred beyond v1.2 (no owning plan) | Splitting `unit.core.state.import` changes the case list of a binary that `docs/spec/phase09-cases.json` inventories, and that file's digest is a closed F2 pin of `harness.phase09-qualification`. The hosted cost fits the limit (it starts 2nd by `COST`; the measured cold gate-macos run is 731 s of 900 s), so the Gate-growth ledger reversal rule, not a split, is the trigger. |
| T2 | Phase 9 triage | deferred to Phase 12 | The replay-driver duplication in `runner/t64_replay.c` has no state or digest effect, and the host pump of Phase 12 reshapes the host loop, so unifying it before the pump exists would be redone. |
| T3 | Phase 9 triage | done in Phase 10 (commit `3deee7d`, the dead `t64_artifact_publisher.c` link removed from the `test_replay` target; the case inventory did not change) | Closed in Phase 10; recorded here so every Phase 9 item has one disposition. |
| T4 | Phase 9 triage | done (11-11): commit `3f9a8bd`; the relabel count went from 20 to 0 ("until Phase 10" production lines, 8 files; `git grep -n -i -E 'until (the )?Phase 10' -- core runner tools` now prints nothing) and `repo.stale-freeze-text` (the broader deny list, 23 findings at the base of this plan across `core`, `runner` and `docs/adr/0002-language-build-and-abi.md`, 0 after) keeps it so | Editing a `core/` comment moves `T64_BUILD_ID`, so the relabel rides the shape wave; the check is `git grep -n -i -E 'until (the )?Phase 10' -- core runner tools` printing nothing. |
| T5 | Phase 9 triage | deferred to Phase 12 | Closed detail names for the recorder failures need a result-protocol vocabulary change and a checker change in the runner, which is not installed and not in the freeze record; no API effect. |
| T6 | Phase 9 triage | deferred to Phase 12 | `T64_REAL_KEEP_WORKSET` (`runner/t64_supervisor.c:1140`) is a runner-internal production supervisor name (gating it broke two Phase 8 tests, FX-18); the runner is not installed nor in the freeze record, so the API review does not own it. One disposition, as the plan check required (I-7). |
| T7 | Phase 9 triage | done (11-11): commit `e9e0cbe`; the third SI trace kind is `T64_SI_TRACE_KIND_REFUSED` (value 3, a refused SI register write, value word = status in the upper 32 bits and register offset in the lower 32); `t64_trace_emit` records it whenever tracing is enabled and `t64_machine_trace_drain` returns it, so it can appear in a public trace record and gets the public name `T64_TRACE_KIND_SI_REFUSED` with a `_Static_assert` alias in `core/t64_si.c` (value unchanged; sweep 0 differences) | The third SI trace kind (`T64_TRACE_DEVICE_SI`, `T64_TRACE_KIND_SI_*`, `core/t64_si.c:15-19`) gets its public name, or a recorded reason it is not public, in the same wave as the relabel, because a core name edit moves the build id and the names are part of the provisional set until the candidate. |
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
| SHAPE (11-02 to 11-14; plan 11-15, hosted run 45736981784 at head `43b31e2`, warm: Binutils cache hit on both jobs; F-127) | 21 names, 275 in total (`unit.core.api.abi-version`, `.poison`, `.refusals-never-poison`, `.reentrant`, `.null-machine`, `.defect-sites`, `.bus-carve-out`, `.reset`, `.unload`, `.not-ready-table`, `.output-status`, `.handshake`, `.struct-size`, `repo.abi-header`, `repo.abi-status-classes`, `repo.abi-selftest`, `repo.api-init-guard`, `repo.api-init-guard-selftest`, `repo.abi-prefix`, `repo.stale-freeze-text`, `repo.stale-freeze-text-selftest`) | 8.23 s summed from `build/gate/Testing/Temporary/CTestCostData.txt` of the 11-15 scratch clone (fresh tree, one run per name, so the file holds measured seconds; the ctest `Passed` lines sum to 8.22 s; load 10 to 12); largest `unit.core.api.struct-size` 2.54 s, `.bus-carve-out` 1.18 s, `.output-status` 1.12 s | ctest 219 s on 275 names against F-126 194 s on 254 names: +25 s (gate-linux ctest 132 s, +4 s); the 21 names sum 19.62 s on macOS and 14.77 s on Linux; gate-macos job 445 s against F-126 368 s (+77 s, of which Gate +52 s and audio records +12 s; steps no SHAPE edit touches also slowed, so part is runner speed within the F-119 spread of 52 s) | no offset; the Phase 11 G4 exception below (the 21 names are the required evidence of the shape decisions of API-01 and API-02: ABI header, class table, poison, handshake, struct-size, prefix and init guards) | warm 455 s. No cold run was made (the cache entry was not deleted; a cold run is orchestrator-only). Cold projections: F-126 projection 733 s plus attributable growth (19.62 s plus about 4 s of inventory selftest) about 757 s, margin 143 s; plus the whole warm delta of 77 s 810 s, margin 90 s; this warm run plus the F-123 to F-124 cold delta of 346 s 791 s, margin 109 s. WARNING: two of three projections are under the 120 s warning margin; none is above 900 s, so the reversal rule is not triggered. A measured cold run should replace the projections before the INSTALL wave adds its names (11-21 row) |
| INSTALL tracer (plan 11-16; hosted run 45742744889 at head `e5a2e3c`, warm: Binutils cache hit on both jobs; F-128) | 5 names, 280 in total (`install.setup`, `installed.consumer.build`, `installed.consumer.smoke`, `repo.consumer-include-guard`, `repo.consumer-include-guard-selftest`) | 1.58 s summed from the registered COST values (0.05, 0.43, 0.02, 0.07, 1.01), measured natively at 0.05 to 0.08, 0.43 to 0.53, 0.02 to 0.03, 0.06 to 0.07 and 0.61 to 1.01 s (load 10 to 20) | ctest 257 s on 280 names against 219 s on 275 names (F-127): +38 s; the five names sum 4.81 s on macOS (0.46, 1.74, 0.06, 0.13, 2.42) and 0.76 s on Linux; gate-linux ctest 154 s against 132 s (+22 s); gate-macos job 496 s against 445 s (+51 s; Gate step 436 s against 367 s, +69 s): about 5 s is attributable to the five names, the rest is runner speed (spread up to 52 s on macOS, F-119) | no offset; the Phase 11 G4 exception below (the install path, the consumer independence guard and the init guard over every consumer are required evidence of VER-05 and SC1; nothing heavy was added to the hosted gate, no ci.yml edit) | warm 496 s, margin 404 s. No cold run. Cold projections: F-126 projection 733 s plus the attributable growth of SHAPE and INSTALL (about 24 s and 6 s) 763 s, margin 137 s; plus the whole warm delta since F-126 (128 s) 861 s, margin 39 s; this warm run plus the F-123 to F-124 cold delta of 346 s 842 s, margin 58 s. WARNING: two of three projections are under the 120 s warning margin (39 s and 58 s); none is above 900 s, so the reversal rule is not triggered, but the warm samples are climbing (368, 445, 496 s on near-identical bound source plus the new names). A measured cold run (orchestrator-only) should replace the projections before the 11-21 row |

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

## Shape steps

Appended by each SHAPE plan after its before/after proof (plan 11-02 starts the table).
Columns: the plan, the files it changed, the two-tree sweep result line against the Wave 0
base of "Wave bases" (never the moving reference HEAD), whether `T64_BUILD_ID` moved, the
sealed bodies (`REVIEWED_BODY_DIGESTS`) changed, and the CTest names the plan registered with
their native seconds. Native seconds are the `Passed` seconds that ctest printed on the local
host (load 8 to 15, a range over repeated runs), not the values of `CTestCostData.txt`: each new
name carries an explicit `COST` of 1 (DR-21, rounded up from the measured seconds), and ctest
then blends that property into the recorded cost, so the file no longer holds native seconds.

| Plan | Files changed | Sweep result | Build id moved | Sealed bodies changed | New CTest names (native seconds) |
|------|---------------|--------------|----------------|-----------------------|----------------------------------|
| 11-02 | `core/include/tendies64/t64_machine.h` (T64_API, `extern "C"`, ABI constants, `t64_abi_version`, statuses 34 to 38), `core/t64_machine.c` (`t64_abi_version` definition), `core/tests/test_api_surface.c` (new), `core/CMakeLists.txt`, `docs/abi/status-classes.txt` (new), `scripts/check-abi.py` (new), `tools/abi/CMakeLists.txt` (new), `scripts/check-test-inventory.sh` (`PHASE11_TESTS`), `scripts/preflight-phase11-regexes.txt` (rows `^repo\.abi` 3, `^unit\.core\.api\.` 1) | `sweep: 1599 shared lines, 0 differences, 0 head-only lines` (base `f9124ae9fce1`, head `a396fa904bf7`, the Task 2 commit) | yes, by design (a `core` text edit changes the build fingerprint; the sweep forces one build id, so it is not a difference) | none: `python3 -B scripts/check-rsp-no-hle.py --build build/gate` exits 0 with the checker unchanged, and `repo.rsp-no-hle` passes | 4: `unit.core.api.abi-version` 0.00 s, `repo.abi-header` 0.09 to 0.16 s, `repo.abi-status-classes` 0.03 to 0.08 s, `repo.abi-selftest` 0.45 to 0.94 s (about 0.6 to 1.2 s summed; all `COST 1`, `TIMEOUT` 30 or 60) |
| 11-03 | `core/t64_internal.h` (members `poisoned` and `in_call`, `T64_ENTRY_GUARD`, `t64_internal_defect`, `t64_test_force_defect`), `core/t64_machine.c` (poison functions, `t64_machine_is_poisoned`, in-call brackets in `admit_rom` and destroy, defect sites), the 23 entry guards in `core/t64_machine.c`, `t64_digest.c`, `t64_state.c` (export write order), `t64_observe.c`, `t64_save.c`, `t64_bus.c`, `t64_trace.c`, `t64_vi.c`, `t64_ai.c`, `core/t64_cp0.c`, `core/t64_scheduler.c` (whitelist and poison hook), `core/include/tendies64/t64_machine.h` (poison query, export and bus-write statements), `core/tests/test_api_poison.c` (new), `core/tests/fuzz_rom_admission.c`, `test_boot_entry.c`, `test_digest_v25.c`, `core/CMakeLists.txt`, `docs/abi/status-classes.txt` (notes), `docs/spec/machine-contract.md` (member rows, carve-out note), `scripts/check-rsp-no-hle.py` (four digests, isolated commit `7592675`), `scripts/check-test-inventory.sh`, `scripts/preflight-phase11-regexes.txt` (`^unit\.core\.api\.` 7) | `sweep: 1599 shared lines, 0 differences, 0 head-only lines` (base `f9124ae9fce1`, head `eb3148ee8c32`, the Task 4 code commit); statuses 37/38 in head sweep output: 0 (115 `stop` lines carry status 0, 20 `skip` lines carry status 5; the head output equals the base output line for line) | yes, by design (the sweep forces one build id, so it is not a difference) | four, in one isolated commit: `t64_video_info`, `t64_video_copy` (`core/t64_vi.c`), `t64_audio_info`, `t64_audio_copy` (`core/t64_ai.c`); the checker diff is 4 old and 4 new digest lines and nothing else; `python3 -B scripts/check-rsp-no-hle.py --build build/gate` exits 0 | 6: `unit.core.api.poison` 0.09 to 0.10 s, `unit.core.api.refusals-never-poison` 0.15 to 0.17 s, `unit.core.api.reentrant` 0.06 to 0.07 s, `unit.core.api.null-machine` 0.00 s, `unit.core.api.defect-sites` 0.10 to 0.12 s (all `COST 1`, `TIMEOUT` 60) and `unit.core.api.bus-carve-out` 1.6 to 2.3 s (`COST 3`, `TIMEOUT` 60); about 2.1 to 2.9 s summed |
| 11-04 | `core/include/tendies64/t64_machine.h` (`t64_machine_reset`, `t64_machine_unload`, `t64_machine_output_status`, `t64_output_status`, `T64_OUTPUT_STATUS_V1_SIZE`, `T64_OUTPUT_STATUS_INIT`), `core/t64_machine.c` (reset, unload, `clear_guest_state` split out of `reset_guest_state`), `core/t64_observe.c` (output status), `core/t64_internal.h` (`t64_vi_packet_slot`, `t64_ai_packet_slot` with size and member asserts; the two retained arrays use them), `core/t64_vi.c`, `core/t64_ai.c`, `core/t64_digest.c`, `core/t64_state.c` (slot types; copy-out member by member in `t64_video_info` and `t64_audio_info`), `core/tests/test_api_lifecycle.c` (new), `core/tests/test_api_poison.c` (26 entries), `core/tests/test_digest_v26.c` (slot type, same 1208/488/37 pins), `core/tests/test_state_import.c` (diagnostic dump type), `core/CMakeLists.txt`, `docs/spec/machine-contract.md`, `docs/spec/state-field-rules.txt` (13 slot member rows), `scripts/check-rsp-no-hle.py` (five digests, isolated commit `fbe5704`), `scripts/check-test-inventory.sh`, `scripts/preflight-phase11-regexes.txt` (`^unit\.core\.api\.` 11) | `sweep: 1599 shared lines, 0 differences, 0 head-only lines` (base `f9124ae9fce1`, head `fbe5704a16b1`, the isolated refresh commit) | yes, by design (the sweep forces one build id, so it is not a difference) | five, in one isolated commit: `t64_vi_event`, `vi_v8_event`, `t64_video_info` (`core/t64_vi.c`), `t64_ai_event`, `t64_audio_info` (`core/t64_ai.c`); the checker diff is 5 old and 5 new digest lines and nothing else; `python3 -B scripts/check-rsp-no-hle.py --build build/gate` exits 0 | 4: `unit.core.api.reset` 0.17 to 0.18 s, `unit.core.api.unload` 0.23 s, `unit.core.api.not-ready-table` 0.03 s, `unit.core.api.output-status` 0.86 to 0.87 s (all `COST 1`, `TIMEOUT` 60); about 1.3 s summed |
| 11-10 | `runner/tests/test_cpu_diagnostic.c`, `test_fixture_descriptor.c`, `test_input.c`, `test_pad_poll.c`, `test_real_runner.c`, `test_replay.c`, `test_state_runner.c` (init macros, commit `ad1dd6c`); `core/tests/test_state_import.c` and `runner/tests/test_state_runner.c` (two pointer-form import-report fills found by the guard, commit `e22030d`); `scripts/check-api-init.py` (new), `tools/abi/CMakeLists.txt`, `scripts/check-test-inventory.sh` (`PHASE11_TESTS`), `scripts/preflight-phase11-regexes.txt` (`^repo\.api-init-guard` 2) | not run: no production source changed (test sources, one script, CMake registrations and records only), so the two-tree sweep has nothing to compare | no (the build fingerprint hashes the production core sources only) | none | 2: `repo.api-init-guard` 0.73 to 0.83 s, `repo.api-init-guard-selftest` 0.08 to 0.10 s (both `COST 1`, `TIMEOUT` 60); about 0.9 s summed |
| 11-11 | `core/t64_prefix.h` (new, generated, 220 renames), `scripts/gen-prefix-header.py` (new), the nine SoftFloat primitives `core/softfloat/s_approxRecip32_1.c`, `s_approxRecipSqrt32_1.c`, `s_countLeadingZeros32.c`, `s_countLeadingZeros64.c`, `s_mul64To128.c`, `s_shiftRightJam32.c`, `s_shiftRightJam64.c`, `s_shiftRightJam64Extra.c`, `s_shortShiftRightJam64.c` (20 deleted lines, no added line) and their `provenance/copies.json` rows, the include of the prefix header in `core/t64_internal.h`, `core/softfloat/platform.h`, `core/t64_rsp_divider.h` and `core/include/tendies64/t64_facade.h`, `core/CMakeLists.txt` (core directory on the `t64_core` include path), `runner/CMakeLists.txt` (three facade consumers), `tools/lint/core-lint.sh` (`-I core`), `scripts/check-rsp-no-hle.py` (two isolated commits, lookups only), the relabel of 20 production lines in 8 files plus the banners of `t64_machine.h` and `t64_facade.h` and `docs/adr/0002` lines 55 and 91, the ADR-0002 and ADR-0006 amendments, `scripts/check-stale-freeze-text.sh` (new), `scripts/list-membership-sites.py` and `docs/spec/profile-membership-sites.txt` (W2-02), `core/include/tendies64/t64_machine.h` and `core/t64_si.c` (T7), `tools/abi/CMakeLists.txt`, `scripts/check-test-inventory.sh` (`PHASE11_TESTS`), `scripts/preflight-phase11-regexes.txt` (`^repo\.abi` 4, `^repo\.stale-freeze-text$` 1, `^repo\.stale-freeze-text-selftest$` 1) | `sweep: 1599 shared lines, 0 differences, 0 head-only lines` (base `f9124ae9fce1`, head `e9e0cbe14e21`, the last code commit) | yes, by design (the sweep forces one build id, so it is not a difference) | none: the rename is a preprocessor macro, so no sealed body text moved; the canonical-symbol lookups of `scripts/check-rsp-no-hle.py` follow the prefix map in commits `270dc51` and `0b3d911` (no `REVIEWED_BODY_DIGESTS` value changed) and `python3 -B scripts/check-rsp-no-hle.py --build build/gate` exits 0 | 3: `repo.abi-prefix` 0.06 to 0.11 s, `repo.stale-freeze-text` 0.5 to 0.6 s, `repo.stale-freeze-text-selftest` 0.5 s (all `COST 1`, `TIMEOUT` 30); about 1.2 s summed |
| 11-12 | the flip commit `0338380171fc198c175b78e70e499c1cee554fac` (79 files): `core/include/tendies64/t64_machine.h` (leading `struct_size` on the ten in/out structs, fixed-width members, `t64_machine_create_info`, `T64_MACHINE_CREATE_INFO_INIT`, the ten init-macro bodies, `t64_machine_create` and `t64_machine_trace_drain` signatures), `core/t64_internal.h` (v1 size constants), `core/t64_machine.c` (create handshake in the DR-05 order, allocator `struct_size`, descriptor tables, hash and serialize skip the member), `core/t64_rom.c` (the three allocator sites, W-1), `core/t64_trace.c` (`record_size`), `core/t64_observe.c` and `core/t64_state.c` (two lines each, not sealed: `t64_machine_cpu_stop_detail`, `t64_machine_output_drain` and `t64_machine_state_import` write the caller's `struct_size` back unchanged), `core/CMakeLists.txt`, `core/tests/test_api_struct_size.c` (new), `scripts/codemod-phase11.py` (`--create`, `--trace`, INIT-initialized arrays incl. 2D, selftest 19 of 19), `docs/abi/status-classes.txt`, `scripts/check-test-inventory.sh` (`PHASE11_TESTS`), `scripts/preflight-phase11-regexes.txt` (`^unit\.core\.api\.` to 12); 66 non-bound callers rewritten by one codemod invocation (197 conversions in 67 scanned files: 141 create, 53 trace, 3 descriptors; `core/tests/test_api_surface.c` needed none); the nine bound files are not in the commit (their patch is the scratch-proven `build/11-12-bound/bound-callsites.patch`, 97 hunks, section "Bound patch hunks") | `sweep: 1599 shared lines, 0 differences, 0 head-only lines` (base `f9124ae9fce1`, head `0338380171fc`, the flip commit, shared tree, 49 s; the same line in the scratch clone with the bound patch, 39 s) | yes, by design (a `core` text edit changes the build fingerprint; the sweep forces one build id, so it is not a difference) | none: `python3 -B scripts/check-rsp-no-hle.py --build build/gate` exits 0 at the flip commit (`REVIEWED_BODY_DIGESTS` unchanged, so the isolated refresh commit was skipped) | 1: `unit.core.api.handshake` 0.00 to 0.01 s (`COST 1`, `TIMEOUT` 30) |
| 11-13 | the prologue commit `24d45e8f23e9a6eb692fe2fba1b9b6a05978ed3f` (14 files): `core/t64_machine.c` (the advance request and result, `t64_machine_cpu_stop_detail`, `t64_profile_hash`, `t64_profile_serialize`), `core/t64_observe.c` (`t64_machine_output_drain`), `core/t64_state.c` (`t64_machine_state_import`), `core/t64_save.c` (`t64_machine_save_info`), `core/t64_vi.c` (`t64_video_info`), `core/t64_ai.c` (`t64_audio_info`), `core/t64_internal.h` (the static inline `t64_internal_copy_out` and `<string.h>`, no new external symbol), `core/include/tendies64/t64_machine.h` (a check-order comment only), `core/tests/test_api_struct_size.c` (the struct-size group, nine cases), `core/CMakeLists.txt`, `docs/abi/status-classes.txt` (every `STRUCT_TOO_SMALL` call site), `docs/spec/state-container.md` (the report size refusal), `scripts/check-test-inventory.sh` (`PHASE11_TESTS`), `scripts/preflight-phase11-regexes.txt` (`^unit\.core\.api\.` to 13); the isolated refresh commit `96d33230f0da` and the cost commit `205688d` follow it; no bound file, authority checker or authority record in any commit | `sweep: 1599 shared lines, 0 differences, 0 head-only lines` (base `f9124ae9fce1`, head the scratch clone worktree with the staged prologues, the bound patch and the digest refresh, 41 s) | yes, by design (a `core` text edit changes the build fingerprint; the sweep forces one build id, so it is not a difference) | two, in one isolated commit (`96d33230f0da`): `t64_video_info` (`core/t64_vi.c`) and `t64_audio_info` (`core/t64_ai.c`); the checker diff is 2 old and 2 new digest lines and nothing else; `python3 -B scripts/check-rsp-no-hle.py --build build/gate` exits 0 | 1: `unit.core.api.struct-size` 1.94 to 2.00 s (`COST 2`, `TIMEOUT` 60) |
| 11-14 | Task 1 (re-proof, commit `2abe207`) changed no source file: only this record. The orchestrator's Task 2 commit `e66625a6494863ba6921da575fdb86e379a1c76d` (the last commit of the shape wave; 12 files, 249 insertions, 152 deletions) changed exactly the nine bound test files (`core/tests/test_rdp.c`, `test_rdp_copy.c`, `test_rdp_triangles.c`, `test_rdp_texture.c`, `test_rdp_combiner.c`, `test_rdp_blender.c`, `test_rdp_depth.c`, `test_vi.c`, `test_video_output.c`) and the three authority files (`scripts/check-rdp-oracle-authority.py` pin line only, `docs/evidence/phase07-oracle-authority.json`, `.md`) and nothing else; `PINNED_REQUEST_SHA256` old `e131cbe7e255052e706668b6cb0d7192c4ac877b65bd7c3a33e65194962927ab`, new `75f2b651394825ade3a2661950b889522a59b03e2fbc21c65c70f271adda2779`; the DEV-11-01 window closed at that commit | closing: `sweep: 1599 shared lines, 0 differences, 0 head-only lines` (base `f9124ae9fce1`, head `e66625a64948`, shared tree, 40 s); re-proof: the same line (head the clone worktree at `7632f4417c77` with the bound patch, SHA-256 `beb2cafb0a8a66a66e0cdef84a9233cef11973cbd66ed1e84f2f704c50f262cb`, 55 s; clone gate 136 s, stopped at its ctest step with exactly the two identity-block names failed) | not by this step (test sources, one pin line and two records only; no production text edit) | none | none |
| 11-26 | defect-fix plan for the SHAPE findings, executed out of wave order right after 11-15: `core/t64_observe.c` (`t64_machine_output_drain` and `t64_machine_output_status` answer a drain cursor beyond the capture length, a capture length beyond the capture buffer and a ring head or count out of range through `t64_internal_defect`; the `ring_free` helper that floored a bad count is removed), `core/tests/test_api_poison.c` (white-box cases beside `unit.core.api.defect-sites`: 2 drain and 6 status breaks, each after a healthy control call), `docs/abi/status-classes.txt` (notes on the `RESOURCE_EXHAUSTED` and `INTERNAL_DEFECT` rows), `core/include/tendies64/t64_machine.h` (comment lines only), `docs/spec/state-container.md` (check 1 wording), the plan file (workstation paths of its `execution_context`). Commits `6e53ecb` (S-02), `259784a` (S-01, S-03), `8d7f9de` (init-guard fix of the new locals), `766d20a` (plan-file paths) | `sweep: 1599 shared lines, 0 differences, 0 head-only lines` (base `f9124ae9fce1`, head `8d7f9de42a39`, shared tree, 47 s; the same line at the Task 1 code state, head WORKTREE, 42 s) | yes, by design (a `core` text edit changes the build fingerprint; the sweep forces one build id, so it is not a difference) | none: `python3 -B scripts/check-rsp-no-hle.py --build build/gate` exits 0 with the checker unchanged (neither edited function is a sealed body), so no isolated refresh commit | none (the new cases join `unit.core.api.defect-sites`, which stays `COST 1`: 0.10 to 0.12 s in the 11-03 row, 0.07 to 0.09 s with the new cases) |

Plan 11-02 evidence (all at the Task 2 commit, tree clean): the inventory count is 258
(254 + 4), the label comparison reports 4 tests carrying `phase11` against a 4-name block,
`harness.rdp-oracle-authority`, `repo.si-membership-sites`, `harness.state-field-use-audit`,
`repo.p10-mutant-anchors` and `repo.rsp-no-hle` pass, and the full `sh scripts/preflight-phase11.sh`
(including the 229-unit `linux-compile` with the act image gcc 13.3.0, which covers the new
`extern "C"` block, and the post-gate replay) reports PASS in 77 s. The `linux-compile` step
compiles C units only, so the C++17 compile of the header is the local `repo.abi-header` only. No
status value 0 to 33 and no stop reason 1 to 8 changed (`git diff` shows no removed
`#define` line); the table holds 47 rows against 47 constants. Evidence tier: unit-tested,
authored scope; a software-interface claim only.

Plan 11-03 evidence: the inventory count is 264 (258 + 6) and the label comparison reports 10
tests carrying `phase11` against a 10-name block. The full `ctest --preset gate` passed 263 of 263
at the Task 3 commit (176 s under host load 40, no limit widened) and the focused checks
`harness.rdp-oracle-authority`, `repo.si-membership-sites`, `harness.state-field-use-audit`,
`repo.p10-mutant-anchors`, `repo.rsp-no-hle` and the three `repo.abi-*` names pass at the Task 4
commit. `repo.rsp-no-hle` was red between commits `82aa24f` and `7592675` by design (exactly the four
re-pinned bodies). Measured handler statuses, the static whitelist, the defect-site table and the
state-export order are in section "DR-12 record". Evidence tier: unit-tested, authored scope; a
software-interface claim only.

Plan 11-04 evidence: the inventory count is 268 (264 + 4) and the label comparison reports 14 tests
carrying `phase11` against a 14-name block. The full `ctest --preset gate` passed 268 of 268 at the
refresh commit (83 s at load 8), `unit.core.digest.v26-completeness` keeps its 791 member, 400
named-exclusion and 17 padding bytes of 1,208 (the case passes with the `VMT` and `VMTX` rows now
naming `t64_vi_packet_slot`), and the retained slots are asserted at 488 and 280 bytes with their first
and last members in `core/t64_internal.h`. `repo.rsp-no-hle` was red between `1aa965a` and `fbe5704` by
design (exactly the five re-pinned bodies). The slot types joined the member set of the state-field
audit, so 13 member names (`width`, `height`, `stride_bytes`, `source_width`, `source_x_offset`,
`source_y_offset`, `x_scale`, `y_scale`, `profile_id`, `build_id`, `vi_clock_policy`, `vi_output_policy`,
`struct_size`) got rule NONE rows with a comment, because the slot values are echoed metadata and the
lexical hits belong to same-named geometry members. Evidence tier: unit-tested, authored scope; a
software-interface claim only.

Plan 11-10 evidence: the inventory count is 270 (268 + 2) and the label comparison reports 16 tests
carrying `phase11` against a 16-name block. The full `ctest --preset gate` passed 268 of 268 at the
Task 1 commit (102.6 s, start load 8.9) and 270 of 270 before the Task 2 commit (116.3 s, start load
9.6); `harness.rdp-oracle-authority`, `repo.si-membership-sites`, `harness.state-field-use-audit`,
`repo.p10-mutant-anchors` and `repo.rsp-no-hle` are in that run and unchanged. The RED report
`build/gate/11-10-task2/red.txt` shows 10 of the 17 selftest cases failing before the rules existed
(the four mandatory seeds, the positional descriptor, the two memset forms, the two array forms and
the oracle-bound flag case) and the seven clean-case seeds passing; the selftest then passes 17 of 17.
The full `sh scripts/preflight-phase11.sh` at the Task 2 commit (clean tree) reports PASS in 101 s: the
ctest counts (`^repo\.api-init-guard` 2), the guards step (172 files, 0 findings, `--exclude-oracle-bound` as
registered), the 231-unit `linux-compile` with the act image gcc 13.3.0 (it covers the converted
`runner/tests` sources) and the post-gate replay; its host-load step warned at load 18 (unrelated host
processes), which never fails.
Evidence tier: unit-tested, authored scope; a software-interface claim only.

Plan 11-11 evidence: the inventory count is 273 (270 + 3) and the label comparison reports 19 tests carrying
`phase11` against a 19-name block. The archive `build/gate/core/libt64_core.a` held 255 defined globals before the
rename (35 public functions and 220 internal names, against the plan-time 248 estimate: the 11-02 to 11-04 additions
moved it) and holds 35 names without the `t64i_` prefix and 220 `t64i_` names after it, all of type `T`;
`python3 -B scripts/gen-prefix-header.py --check build/gate/core/libt64_core.a` prints `ok (35 public functions, 220 renames)`.
Four generator controls fail or pass as seeded: a host global added to a copy of the archive (`unprefixed`, exit 1), a stale
entry in a copy of the header (`stale`, exit 1), two entries swapped (`differs` with a unified diff, exit 1) and `--write`
over the committed header (byte-identical). The mechanism is one `#include "t64_prefix.h"` placed in `core/t64_internal.h`,
`core/softfloat/platform.h`, `core/t64_rsp_divider.h` (the divider unit includes no other core header) and
`core/include/tendies64/t64_facade.h`, with the core directory on the include path of `t64_core`, the lint compile
(`-I core` in the default scan of `tools/lint/core-lint.sh`) and the three runner targets that include the facade; the
`-include` option was not used, so the lint compile line needs no new option form. The nine SoftFloat files lost 20 lines
and added none; `sh scripts/check-provenance.sh` passes (`errors=0`); `unit.core.softfloat32` checks=10466,
`unit.core.softfloat64` checks=10478, `unit.core.softfloat-isolation` 84, `unit.core.cop1-state` 829 and
`unit.core.lifecycle` 317, all failures=0; `sh tools/lint/core-lint.sh` (99 files, 0 violations) and its `--selftest`
(27 of 27) pass. `repo.rsp-no-hle` was red from the Task 1 commit to the isolated refresh commit `270dc51` by design (11
canonical-symbol findings, all of the `absent` kind), and its control suite then failed once because its fixture copies a
fixed source set without the header (fixed in the checker-only follow-up `0b3d911`). The first full `ctest --preset gate`
at the registration state passed 270 of 271 (the one failure was that control suite, 104 s); the full gate at the last
code commit passed 273 of 273 (126 s, start load 8.4, no limit widened). `scripts/p10-mutants.py --run` over the renamed
core: control `control-equivalent-depth-cap` SURVIVED and control `control-no-compile` INVALID as before, and eight
mutants, one per target source, are KILLED (`k1-predicate-widened` in `core/t64_internal.h`, `vg01-decal-strict` in
`core/t64_rdp_depth.c`, `vi-clock-48600000` in `core/t64_vi.c`, `vg04-floor-off-s` in `core/t64_rdp_tmem.c`,
`vg05-shade-trunc` in `core/t64_rdp_raster.c`, `v26-sidecar-invalidation-removed` in `core/t64_rdp_pipeline.c`,
`v26-digest-omit-scalar` in `core/t64_digest.c`, `v26-export-version-word-1` in `core/t64_state.c`). The relabel guard's RED
milestone is `build/gate/11-11-task3/red-selftest.txt` (the stubbed rules fail 7 of 10 selftest cases) and
`red-real-tree.txt` (23 findings on the unrelabeled tree); after the relabel `repo.stale-freeze-text` and its selftest pass,
`git diff --quiet f9124ae9fce1 -- docs/claims docs/evidence fixtures/state/decode_container.py` exits 0 and the ADR-0006
amendment holds the three allowlist rows. `repo.si-membership-sites` and `harness.phase09-qualification` pass after the
W2-02 regeneration. The full `sh scripts/preflight-phase11.sh` at the PIN-IMPACT commit (clean tree) reports PASS in 80 s: the ctest counts
(20 rows, 0 mismatches; 19 tests carry `phase11`), the guards step (`check-api-init.py` and `check-stale-freeze-text.sh` passed), the 231-unit
`linux-compile` with the act image gcc 13.3.0 (it compiles the renamed core and the include of the prefix header on Linux; it does not
archive or link, so `repo.abi-prefix` on ELF is first exercised by the hosted run) and the post-gate replay.
Evidence tier: unit-tested, authored scope; a software-interface claim only.

Plan 11-12 evidence: the inventory count is 274 (273 + 1) and 20 tests carry `phase11`. The flip was proven before it was
committed (W-3): the full non-bound `ctest` in the shared tree passed at the flip commit (262 of 262 names outside the bound
regex, 278 s at load 8 to 12, no limit widened; the commit is byte-identical to the staged state that the clone was built from, and an
earlier pass during the task flagged two init-guard findings in the then-uncommitted `core/tests/test_api_struct_size.c` and a stray
`scripts/__pycache__` file (`repo.api-init-guard`, `gov.rights.repo`, `gov.pii.repo`); the findings were fixed and the file removed before the commit), and in the marked scratch clone
`build/11-12-bound/src` with the flip and the bound patch applied `cmake --preset gate` and `cmake --build --preset gate`
passed (8 s, every target including the nine bound test executables, `-Werror`) and `ctest --preset gate` ran 274 tests in 85 s
with 272 passing, among them all 12 bound CTest names, and 2 failing by design (the identity block below). The sweep reports
`sweep: 1599 shared lines, 0 differences, 0 head-only lines` in the clone (head the worktree with the bound patch) and in the
shared tree at the flip commit; the printer's head branch (`defined(T64_MACHINE_CREATE_INFO_INIT)`) therefore ran twice and
matched the base on every id. After the commit `python3 -B scripts/check-rsp-no-hle.py --build build/gate` exits 0 (no sealed
body re-pinned, no refresh commit), `python3 -B scripts/gen-prefix-header.py --check build/gate/core/libt64_core.a` prints
`ok (35 public functions, 220 renames)` (the flip adds no external symbol), and `unit.core.api.handshake`, `repo.abi-prefix` and
`repo.abi-status-classes` pass. `python3 -B scripts/tests/test_rsp_qualification.py` (30 tests) passes with the file unchanged:
its profile-policy seed uses designated initializers and the parser reads named fields only, so `struct_size` needs no seed row (I-3).
Four design points the plan did not spell out: (1) the create call sites became the compound-literal form
`&(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(allocator, profile)` rather than a named local `info` (the codemod emits one form for
statement and expression callers alike); (2) `t64_machine_cpu_stop_detail`, `t64_machine_output_drain` and
`t64_machine_state_import` write the caller's `struct_size` back unchanged into their local result, which touches `core/t64_observe.c`
and `core/t64_state.c` (neither sealed); (3) create refuses (`T64_STATUS_BAD_STORAGE`) host storage that overlaps the create-info
object; (4) nine non-bound `t64_host_storage` positional `{NULL, 0u, 0u}` initializers became `{0}`.
Evidence tier: unit-tested, authored scope; a software-interface claim only.

Plan 11-13 evidence: the inventory count is 275 (274 + 1) and 21 tests carry `phase11`. The prologues were proven before they were
committed (W-3): the full non-bound `ctest` in the shared tree passed 263 of 263 names outside the bound regex (275 s at load 7 to 8, no
limit widened; an earlier pass at the same code before two corrections flagged three failures that are recorded below), and in the
marked scratch clone `build/11-13-bound/src` (detached at `715c790dc4660922410d7274107daf87db42b6be`, the shared HEAD the prologue commit
has as its parent) with `build/11-13-bound/prologues.patch` (SHA-256 `17ee4d2ebb9ab3b667b1fc2dbe0a612289eae857b6d9cd5ff055456c4978356c`),
the unchanged `build/11-12-bound/bound-callsites.patch` (SHA-256 `beb2cafb0a8a66a66e0cdef84a9233cef11973cbd66ed1e84f2f704c50f262cb`, verified
against the Bound patch hunks record before it was applied) and the not-yet-committed no-HLE digest refresh (so `repo.rsp-no-hle` is not a
second identity-block name) applied, `cmake --preset gate`, `cmake --build --preset gate` and `ctest --preset gate -E
'^(harness\.rdp-oracle-authority|harness\.phase10-qualification)$'` passed 273 of 273 (the other 2 of 275 are the identity block of 11-12,
72 s at load 8; all 12 bound CTest names among the 273), `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked` exits 2 with
`authored corpus SHA-256 changed: core/tests/test_rdp.c` and the sweep reports `sweep: 1599 shared lines, 0 differences, 0 head-only lines`
(base `f9124ae9fce134f31b645c6f2911a6f27fa41b9a`, head the clone worktree, 41 s). After the commits `python3 -B scripts/check-rsp-no-hle.py
--build build/gate` exits 0 (`sources_scanned=85`), `python3 -B scripts/gen-prefix-header.py --check build/gate/core/libt64_core.a` prints
`ok (35 public functions, 220 renames)` (the plan adds no external symbol), and `unit.core.api.handshake`, `unit.core.api.struct-size`,
`repo.abi-prefix` and `repo.rsp-no-hle` pass.
Design points the plan did not spell out: (1) the struct span is judged before the struct is read (the existing `INVALID_ARGUMENT` order is
kept: `test_ai.c` passes a wrapping `info_out` and `test_isviewer.c` a result span inside RDRAM, and the first pass that read `struct_size`
first segfaulted the first and answered `STRUCT_TOO_SMALL` for the second), so the check order of an observer is NULL, span, `struct_size`,
admission and state; `t64_machine_advance` has no span check and orders NULL, `struct_size` of the request and of the result, then
`NOT_READY`; (2) copy-out entries build the record in a zeroed local and copy the bytes after `struct_size` with the static inline
`t64_internal_copy_out` (also for `t64_machine_cpu_stop_detail`, whose old local copy carried uninitialized padding); `t64_video_info` and
`t64_audio_info` used to clear the caller `struct_size` (video) or set it from the retained slot (audio) and now leave it alone;
(3) `t64_profile_hash` and `t64_profile_serialize` read named members only, so a larger caller descriptor needs no local copy of its 2,304
bytes: the floor check is the whole prologue; (4) `t64_machine_state_import` reads the report size only when the report span is usable
(not wrapping, not aliasing the machine), so an unusable span is refused exactly as before; a NULL report carries no size and is accepted;
(5) `t64_machine_output_status` (11-04) joined the same case matrix. Case names of `unit.core.api.struct-size`: `advance`, `cpu-stop-detail`,
`output-drain`, `state-import`, `video-info`, `audio-info`, `output-status`, `save-info`, `profile-descriptor` (each holds the floor and the
larger-caller case); the RED milestone is `build/gate/11-13-task1/red.txt` (the advance cases fail with no prologue) and
`build/gate/11-13-task2/red.txt` (104 failures over the seven entries that had no prologue yet; `output-status` already passed from 11-04).
Evidence tier: unit-tested, authored scope; a software-interface claim only.

Plan 11-14 evidence (Task 1, the re-proof of the bound patch at the 11-13 head; Task 2 is the orchestrator's): the patch
`build/11-12-bound/bound-callsites.patch` has SHA-256 `beb2cafb0a8a66a66e0cdef84a9233cef11973cbd66ed1e84f2f704c50f262cb`, equal to the
"Bound patch hunks" record and to the 11-13 record; it touches 9 files, 97 hunks, and not `runner/tests/test_rdp_video.c`. It was applied
unchanged in the marked scratch clone `build/11-14-bound/src` (marker `build/11-14-bound/.t64-bound-scratch`, `git clone --no-hardlinks`,
detached at `7632f4417c7722dc290c23b14cdad9c7b102be43`, the shared HEAD after plan 11-13, `build/tools/install` copied in): `git apply --check`
and `git apply` were clean, and `git diff` regenerated in the clone is byte-identical to the recorded patch (`cmp`). The shared tree's nine
bound files, the authority checker and the authority record stayed byte-identical (`git diff --quiet HEAD` on them).
- Clone gate: `sh scripts/gate.sh` stops at its ctest step (exit 1, `[gate] ctest FAILED`) with 273 of 275 passed and exactly two names failed,
  `harness.phase10-qualification` (its `test_real_stage_commands_start` sees exit 2 where the `--expect-blocked` command expects 0) and
  `harness.rdp-oracle-authority` (`authored corpus SHA-256 changed: core/tests/test_rdp.c`): the identity-block exception of the orchestrator
  decision of 2026-11-25, option A, which clears only at the Task 2 rebind. Stages before ctest passed (`sh-n` 1 s, `lint-selftest` 4 s,
  `fixture-qualification` 1 s, `configure` 0 s, `build` 0 s on the second run, `boot-evidence-selftest` 13 s, `test-inventory-selftest` 26 s,
  `test-inventory` 0 s); ctest ran 90.5 s; the whole gate took 136 s (a first run in the clean clone took 148 s with a 5 s build). Host load
  was 6.5 at the start and rose to 14 at the end from an unrelated project's CI on the same machine; no limit was widened and the load was waited
  out before the second run.
- Plan verify defect found (deviation, Rule 1, no widening): the first clone run (the plan's verify redirects `sh scripts/gate.sh > gate.out`
  inside the clone) failed a third name, `gov.pii.repo`, with two `P-PRIVATE-PATH` hits in `gate.out` itself (the cmake and ctest banner lines
  carry the clone path), because `scripts/pii-scan.sh` scans untracked, non-ignored files of the tree and the gate output file was one. The
  second run wrote the gate output outside the clone tree and `gov.pii.repo` passed. Any gate run that redirects its output must write it to an
  ignored or external location (a file under `build/` is ignored); the orchestrator's post-rebind `sh scripts/gate.sh` in the shared tree needs no
  redirect.
- `sh tools/lint/core-lint.sh` (the stage the ctest stop prevented) passes in the clone: `core-lint: files=99 violations=0` (13 s).
- `python3 -B scripts/digest-sweep.py --base f9124ae9fce134f31b645c6f2911a6f27fa41b9a --head WORKTREE` in the clone prints
  `sweep: 1599 shared lines, 0 differences, 0 head-only lines` (exit 0, 55 s).
- `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked` in the clone exits 2 with `oracle-authority: authored corpus SHA-256 changed:
  core/tests/test_rdp.c` (the checker stops at the first changed file). Comparing the 13 `EXPECTED_CORPUS_PATHS` byte for byte between the
  clone and the shared tree, exactly the nine API-using files differ; `docs/spec/rdp-vi-cases.json`, `fixtures/rdp/fill.S`,
  `scripts/check-rdp-vi-cases.py` and `runner/tests/test_rdp_video.c` are identical.
- Old-hash reproduction for the orchestrator (read-only; prints the recomputed canonical request hash and the recorded `request_sha256`, both must
  equal `e131cbe7e255052e706668b6cb0d7192c4ac877b65bd7c3a33e65194962927ab`, which is also the `PINNED_REQUEST_SHA256` line of the checker). The
  command is one line and applies the checker's own `_canonical_sha256` encoding to `request` (13 rows in
  `request.corpus.bound_authored_inputs`, of which the orchestrator rehashes the nine edited ones):

  ```sh
  git show HEAD:docs/evidence/phase07-oracle-authority.json | python3 -B -I -c 'import hashlib,json,sys; r=json.load(sys.stdin); print(hashlib.sha256(json.dumps(r["request"],sort_keys=True,separators=(",",":"),ensure_ascii=True,allow_nan=False).encode("ascii")).hexdigest(), r["request_sha256"])'
  ```

- I-1 reading of the approval text (owner approval 2026-11-25, "Approve call-site-only rebind"): the text says "the nine bound test files" and lists
  ten paths; the nine API-using files (`core/tests/test_rdp.c`, `test_rdp_copy.c`, `test_rdp_triangles.c`, `test_rdp_texture.c`,
  `test_rdp_combiner.c`, `test_rdp_blender.c`, `test_rdp_depth.c`, `test_vi.c`, `test_video_output.c`) are edited, and the tenth listed path,
  `runner/tests/test_rdp_video.c`, needs no edit (11-01 probe) and keeps its hash; the one category (c) hunk (`test_rdp.c` line 486) is the owner's
  answer A of 2026-11-25 (DR-27). The bound commit grants no oracle, rights or publication authority; `local_decision` stays blocked and both
  `TRUSTED_*_DECISION_SHA256` stay `None`.
- Closing (Task 2 done by the orchestrator under the owner approvals of 2026-11-25; this record written after the bound commit): the DEV-11-01
  window is closed at the bound commit `e66625a6494863ba6921da575fdb86e379a1c76d`, the last commit of the shape wave; the nine bound
  executables and `test_dp_transport` compile again in the shared tree and the full preset builds. The old pinned request hash
  `e131cbe7e255052e706668b6cb0d7192c4ac877b65bd7c3a33e65194962927ab` was reproduced from the committed record before the edit; the new
  `PINNED_REQUEST_SHA256` is `75f2b651394825ade3a2661950b889522a59b03e2fbc21c65c70f271adda2779`.
- Acceptance checks of the bound commit: `git show --name-only` lists exactly the nine bound test files and the three authority files (12 paths,
  no deletion); `git show e66625a -- scripts/check-rdp-oracle-authority.py` changes only the `PINNED_REQUEST_SHA256` line (one line removed, one
  added); the `TRUSTED_LOCAL_DECISION_SHA256` and `TRUSTED_PUBLICATION_DECISION_SHA256` lines are unchanged and still `None`;
  `runner/tests/test_rdp_video.c` is not in the commit.
- Authority checker after the rebind: `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked` exits 0 with `authority record valid;
  blocked: no oracle operation is authorized`; `--require-approved` exits exactly 2 (`oracle-authority: no pinned competent owner/legal decision;
  oracle operations remain blocked`). `local_decision` stays blocked and publication stays not authorized.
- Post-rebind gate in the shared tree (measured by the orchestrator, output kept at `build/11-14-final-gate.out`): `sh scripts/gate.sh` ends
  `[gate] PASS`, total 224 s (ctest 125 s with `100% tests passed out of 275`, core-lint 16 s, `test-inventory-selftest` 49 s,
  `boot-evidence-selftest` 20 s, `lint-selftest` 7 s, build 3 s); no name is excluded and `harness.rdp-oracle-authority` and
  `harness.phase10-qualification` pass again; no limit was widened (the closeout checks ran at load averages 15.5, 18.1 and 14.2).
  `python3 -B scripts/check-api-init.py` prints `check-api-init: files=183 findings=0 excluded=1` (exit 0): no init-guard finding in any file,
  including the nine bound files.
- Closing digest sweep: `python3 -B scripts/digest-sweep.py --base f9124ae9fce134f31b645c6f2911a6f27fa41b9a --head HEAD` prints
  `sweep: 1599 shared lines, 0 differences, 0 head-only lines` (exit 0, 40 s; step lines `build head-e66625a64948`, `sweep head-e66625a64948`):
  the shape wave moved no existing digest and no expected byte.
- Per-hunk token-preservation finding: the orchestrator's hunk-token comparison of the bound patch reported every numeric and identifier token of
  the bound hunks preserved. An independent re-check by this executor over `git show -U0 e66625a` for the nine files (110 zero-context hunks, token
  multiset of removed lines minus added lines per hunk) found that the only tokens present on removed lines and absent from the added lines of the
  same hunk are the literal zero initializers `0` and `0u` (and `NULL` in the category (c) hunk) that an `*_INIT` macro or a `{0}` now supplies:
  no non-zero numeric literal, no expected value, no case name and no non-API identifier is removed in any hunk. The one (c) hunk is
  `test_rdp.c` line 486 (`t64_host_storage image = {NULL, 0u, 0u};` to `{0}`), the owner's answer A of 2026-11-25 (DR-27): a value-neutral
  initializer (all-zero before and after).
Evidence tier: unit-tested, authored scope; a software-interface claim only. The DEV-11-01 window is closed at the bound commit `e66625a6494863ba6921da575fdb86e379a1c76d`;
the bound commit grants no oracle, rights or publication authority.

Plan 11-26 evidence (executed out of wave order, after 11-15 and before 11-16; nothing is pushed): the inventory count stays 275 and 21
tests carry `phase11` (no CTest name was added). Review findings and their closure:
- S-02 resolved (commit `6e53ecb`). The RED run is kept at `build/gate/11-26-task1/red.txt` (ignored): 36 failed checks, all in the new cases
  (`t64_machine_output_drain` answered 14 `RESOURCE_EXHAUSTED` where 38 `INTERNAL_DEFECT` was wanted; `t64_machine_output_status` answered 0 `OK`
  for each of the six breaks and left the instance unpoisoned), after a healthy control call that answered `OK` in each row, so the branch under
  test is the one reached. After the change `unit.core.api.defect-sites`, the other `unit.core.api.*` names and the four `repo.abi*` names pass
  (17 of 17); the poisoned drain and status calls leave the result, the data span and the status struct byte for byte as poisoned by the test, and
  the next call answers `POISONED`. The status text pending bytes are still 0 when the boot is not prepared or the capture policy is not the
  ISViewer policy (a legal state, not a defect). `python3 -B scripts/check-abi.py classes` reports `constants=47 rows=47 findings=0`; the
  `RESOURCE_EXHAUSTED` row now says the drain no longer answers it for a bad cursor or capture length, and the `INTERNAL_DEFECT` row lists the
  output observer inconsistencies as defect sites. `python3 -B scripts/gen-prefix-header.py --check build/gate/core/libt64_core.a` prints
  `ok (35 public functions, 220 renames)`.
- S-01 and S-03 resolved (commit `259784a`). Header text: `T64_API` marks the public functions and hides nothing, the archive's other globals
  carry the `t64i_` prefix and the gate checks the symbol list against the marked functions (no visibility attribute is promised; the optional
  `C_VISIBILITY_PRESET hidden` of the finding was not taken); `t64_machine_destroy` is unguarded by design and must not be called from inside one of
  the instance's own allocator callbacks; the descriptor of `t64_machine_profile` is the instance's v1-sized copy and a consumer built against a
  larger descriptor checks `struct_size` before any later member. The diff of the header has comment lines only (no declaration, macro or layout
  line), `python3 -B scripts/check-abi.py header --include core/include` prints `c99 ok`, `c++17 ok`, `findings=0` (the plan's verify command omits
  `--include core/include`, which the checker requires). `docs/spec/state-container.md` check 1 now says a NULL machine answers INVALID_ARGUMENT at the
  entry guard with the report untouched and only a NULL blob answers `ARG_NULL`. `sh scripts/check-docs.sh` and `sh scripts/check-stale-freeze-text.sh`
  pass.
- Gate (shared tree, committed tree at `8d7f9de`, output redirected under `build/`, host load 8.8 at the start after waiting it down from 17.6, 11.3 at
  the end, no limit widened): `sh scripts/gate.sh` stopped at its ctest step with `[gate] ctest FAILED` after 154 s (stages before ctest passed;
  ctest 97.10 s real, 274 of 275 passed). The one failed name is `gov.pii.repo`, and the cause is not this plan's change: its full-history scan reports two
  `P-PRIVATE-PATH` hits in commit `df72747` (the orchestrator's commit that added `11-26-PLAN.md`, whose `execution_context` named two absolute home
  paths). The tree scan is clean (`sh scripts/pii-scan.sh --tree`: `findings=0`; commit `766d20a` replaced the two plan-file lines), the history hit
  stays because the commit is immutable for this executor, and `pii-scan.sh` says a real hit is an owner stop (never rewrite history). So
  `[gate] PASS` is NOT recorded for this plan. Before any push the orchestrator must repair the unpublished range or obtain an owner waiver
  (`tools/rights/pii-history-waivers.tsv`); `preflight-phase11.sh` fails at its `pii` step (`--range origin/main..HEAD`) for the same reason. The
  stage that the ctest stop prevented, `sh tools/lint/core-lint.sh`, passes (`core-lint: files=99 violations=0`). The preflight steps the `pii` failure
  prevented were run with only the `pii` check stubbed out (`sh -c` of the script text with that one step replaced by `true`): `linux-compile` ok
  (63 s, act image gcc, includes the changed `core/t64_observe.c`), the other steps ok, `[preflight] PASS` in 66 s.
- Digest sweep at `8d7f9de` (full 40-hex base `f9124ae9fce134f31b645c6f2911a6f27fa41b9a`): `sweep: 1599 shared lines, 0 differences, 0 head-only
  lines`; no digest, state byte or existing test result moved.
- Carried review items (not acted on here): S-05 (implicit padding holes in six public structs) goes to 11-23, whose layout table pins per-target offsets
  and weighs explicit reserved members as a Phase 12 final-bind question; S-09 (duplicated test helpers: counting allocator, fixture reader, nine
  `poison_*` helpers) goes to 11-20 as one shared test header. The new cases reuse the existing `poison_drain_result` and `poison_output_status` of
  `core/tests/test_api_poison.c` and add no copy.
Evidence tier: unit-tested, authored scope; a software-interface claim only.

### DEV-11-01 window build scope (plan 11-12)

Written by plan 11-12 Task 1 and Task 2 for the DEV-11-01 window (DR-22): from the 11-12 flip commit
to the 11-14 bound commit the nine bound test executables (and `test_dp_transport`, the second unit
of `test_rdp.c`) do not compile in the shared tree, so every verify of 11-12 and 11-13 builds exactly the
target list below and runs every CTest except the bound regex below. The list is the CMake target set of
`cmake --build build/gate --target help` (phony targets without a directory part) minus `all`, `test`,
`edit_cache`, `rebuild_cache`, the `lib*.a` aliases of the library targets, `test_rdp`, `test_rdp_copy`,
`test_rdp_triangles`, `test_rdp_texture`, `test_rdp_combiner`, `test_rdp_blender`, `test_rdp_depth`,
`test_vi`, `test_video_output` and `test_dp_transport`, plus `test_api_struct_size` (Task 2);
122 targets. `runner/tests/test_rdp_video.c` (`test_rdp_video`) compiles unchanged and stays
on the list. The regex selects the 12 CTest names whose command is one of the excluded executables
(`ctest --show-only=json-v1`); no other CTest names an excluded executable in its command.

- Non-bound targets: t64-audio-fixture t64-base-diagnostic t64-base-diagnostic-controls t64-blobscan t64-boot-fixtures t64-cold-boot-probe t64-cpu-diagnostic t64-cpu-diagnostic-test-hooks t64-json-canon t64-manifest-check t64-model-result-check t64-phase09-admit-fixture t64-phase09-pad-fixture t64-phase09-replay-fixture t64-phase09-save-fixture t64-phase10-composite-fixture t64-provenance-check t64-rdp-fill-fixture t64-result-check t64-rsp-diagnostic t64-rsp-smoke t64-runner-real t64-runner-synthetic t64-runner-test t64-skeleton-probe t64-supervisor t64-supervisor-test t64_base_diag_protocol t64_classify t64_common t64_core t64_cpu_diag_protocol t64_diag_common t64_input t64_rsp_diag_protocol t64_sha256 t64_verified_input t64_verified_input_test test_ai test_api_lifecycle test_api_poison test_api_surface test_artifact_publisher test_audio_capture test_base_diag_protocol test_boot_determinism test_boot_entry test_bus test_classify test_cold_boot test_common test_composite_two_run test_cop1 test_cpu_cache test_cpu_cp0 test_cpu_diag_protocol test_cpu_diagnostic test_cpu_exceptions test_cpu_integer test_cpu_link test_cpu_memory test_cpu_mmu test_cpu_path test_cpu_time test_determinism test_digest_trace test_digest_v25 test_digest_v26 test_fixture_descriptor test_input test_isviewer test_joybus test_json test_json_fuzz test_k1_refusal test_lifecycle test_model_result test_pad_poll test_phase09_runner test_pi_dma test_profile test_profile_v26 test_rdp_depth_composite test_rdp_video test_rdram test_real_controls test_real_runner test_replay test_result_v2 test_rom_admission test_rsp_compare test_rsp_control test_rsp_diag_protocol test_rsp_divider test_rsp_fuzz test_rsp_multiply test_rsp_scalar test_rsp_transfers test_rsp_vector test_save test_save_fixtures test_save_roundtrip test_save_transfer test_sched_ledger test_sched_matrix test_scheduler test_schema test_si test_si_profile test_skeleton_probe test_softfloat test_sp_dma test_state_export test_state_export_v26 test_state_format_v26 test_state_import test_state_import_v26 test_state_runner test_tracer test_vg_gaps test_warm_devices test_api_struct_size
- Bound CTest regex: ^(unit\.core\.dp\.transport|unit\.core\.rdp\.(blender|combiner|copy|depth|fill|texture|triangles)|unit\.core\.vi\.(fill|filters|output|timing))$

### Bound patch hunks (plan 11-12)

Written by plan 11-12 Task 3 (W-3, I-2) in the marked scratch clone `build/11-12-bound/src` (detached at
`d1bc22c7121afdf568b3007f0e70ff50307ac0c4`, the shared HEAD the flip was staged on, with
`build/11-12-bound/flip.patch` applied). The nine API-using bound files were converted by
`scripts/codemod-phase11.py --scratch-root build/11-12-bound/src --create --trace --dir <nine>` (17 call
expressions: 16 `t64_machine_create`, 1 `t64_machine_trace_drain` in `test_vi.c`) and the plain `--dir` pass
on the same nine (118 initializers: `test_rdp.c` 5, `test_rdp_copy.c` 2, `test_rdp_triangles.c` 2,
`test_rdp_texture.c` 2, `test_rdp_combiner.c` 2, `test_rdp_blender.c` 3, `test_rdp_depth.c` 2, `test_vi.c` 88,
`test_video_output.c` 12), plus three hand edits inside the clone: the two `t64_video_packet_info` arrays of
`test_vi.c` (`early_info[2]`, `delayed_info[2]`, `{{0}}` to two `T64_VIDEO_PACKET_INFO_INIT` entries, category
(b)) and the owner-approved category (c) edit of `test_rdp.c` line 486 (`t64_host_storage image = {NULL, 0u, 0u};`
to `{0}`, owner answer A). Edit totals equal the 11-01 probe: 17 (a), 120 (b), 1 (c).
`runner/tests/test_rdp_video.c` is not in the patch.

- Bound patch: build/11-12-bound/bound-callsites.patch (not committed), SHA-256 beb2cafb0a8a66a66e0cdef84a9233cef11973cbd66ed1e84f2f704c50f262cb, 9 files, 97 hunks (`git diff` default context; the U0 probe of 11-01 counted 110)

Classification rule per hunk (by its added text): (a) a public API call expression (`t64_machine_create` in the
create-info form, the trace record size argument); (b) the initializer or declaration of an object passed to or
received from one public call, named by the call it belongs to (allocator of `t64_machine_create`, stop request and
result of `t64_machine_advance`, CPU stop detail of `t64_machine_cpu_stop_detail`, video packet info of
`t64_video_info`). A hunk that holds several edits lists each category. Result: 14 hunks (a), 80 hunks (b), 2 hunks
(a) + (b), 1 hunk (a) + (b) + (c) (the approved `test_rdp.c` line 486 edit shares a hunk with the allocator and
create edits of `case_zero_area_fill_is_noop`); no hunk holds any other edit.

| File | Hunk header | Category | Public call of the object or call expression |
|------|-------------|----------|-----------------------------------------------|
| `core/tests/test_rdp.c` | `@@ -65,7 +65,8 @@` | b | t64_host_allocator of t64_machine_create |
| `core/tests/test_rdp.c` | `@@ -75,7 +76,10 @@` | a | t64_machine_create (call) |
| `core/tests/test_rdp.c` | `@@ -117,7 +121,8 @@` | b | t64_host_allocator of t64_machine_create |
| `core/tests/test_rdp.c` | `@@ -125,7 +130,10 @@` | a | t64_machine_create (call) |
| `core/tests/test_rdp.c` | `@@ -481,15 +489,19 @@` | a + b + c (owner answer A, test_rdp.c line 486) | t64_machine_create (call); t64_host_allocator of t64_machine_create; t64_host_storage image (value-neutral {0}) |
| `core/tests/test_rdp.c` | `@@ -534,9 +546,9 @@` | b | t64_stop_request of t64_machine_advance; t64_stop_result of t64_machine_advance |
| `core/tests/test_rdp_blender.c` | `@@ -77,7 +77,8 @@` | b | t64_host_allocator of t64_machine_create |
| `core/tests/test_rdp_blender.c` | `@@ -101,7 +102,10 @@` | a | t64_machine_create (call) |
| `core/tests/test_rdp_blender.c` | `@@ -599,8 +603,8 @@` | b | t64_host_allocator of t64_machine_create |
| `core/tests/test_rdp_blender.c` | `@@ -679,7 +683,10 @@` | a | t64_machine_create (call) |
| `core/tests/test_rdp_blender.c` | `@@ -994,7 +1001,7 @@` | b | t64_video_packet_info of t64_video_info |
| `core/tests/test_rdp_combiner.c` | `@@ -63,7 +63,8 @@` | b | t64_host_allocator of t64_machine_create |
| `core/tests/test_rdp_combiner.c` | `@@ -88,7 +89,10 @@` | a | t64_machine_create (call) |
| `core/tests/test_rdp_combiner.c` | `@@ -147,7 +151,7 @@` | b | t64_video_packet_info of t64_video_info |
| `core/tests/test_rdp_copy.c` | `@@ -40,7 +40,8 @@` | b | t64_host_allocator of t64_machine_create |
| `core/tests/test_rdp_copy.c` | `@@ -64,7 +65,10 @@` | a | t64_machine_create (call) |
| `core/tests/test_rdp_copy.c` | `@@ -326,7 +330,7 @@` | b | t64_video_packet_info of t64_video_info |
| `core/tests/test_rdp_depth.c` | `@@ -44,7 +44,8 @@` | b | t64_host_allocator of t64_machine_create |
| `core/tests/test_rdp_depth.c` | `@@ -70,7 +71,10 @@` | a | t64_machine_create (call) |
| `core/tests/test_rdp_depth.c` | `@@ -143,7 +147,7 @@` | b | t64_video_packet_info of t64_video_info |
| `core/tests/test_rdp_texture.c` | `@@ -39,7 +39,8 @@` | b | t64_host_allocator of t64_machine_create |
| `core/tests/test_rdp_texture.c` | `@@ -64,7 +65,10 @@` | a | t64_machine_create (call) |
| `core/tests/test_rdp_texture.c` | `@@ -210,7 +214,7 @@` | b | t64_video_packet_info of t64_video_info |
| `core/tests/test_rdp_triangles.c` | `@@ -39,7 +39,8 @@` | b | t64_host_allocator of t64_machine_create |
| `core/tests/test_rdp_triangles.c` | `@@ -64,7 +65,10 @@` | a | t64_machine_create (call) |
| `core/tests/test_rdp_triangles.c` | `@@ -263,7 +267,8 @@` | b | t64_host_allocator of t64_machine_create |
| `core/tests/test_rdp_triangles.c` | `@@ -272,7 +277,10 @@` | a | t64_machine_create (call) |
| `core/tests/test_vi.c` | `@@ -45,8 +45,8 @@` | b | t64_host_allocator of t64_machine_create |
| `core/tests/test_vi.c` | `@@ -82,7 +82,10 @@` | a | t64_machine_create (call) |
| `core/tests/test_vi.c` | `@@ -232,7 +235,7 @@` | b | t64_stop_result of t64_machine_advance |
| `core/tests/test_vi.c` | `@@ -346,7 +349,7 @@` | b | t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -383,7 +386,7 @@` | b | t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -410,7 +413,7 @@` | b | t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -438,7 +441,7 @@` | b | t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -460,7 +463,7 @@` | b | t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -490,7 +493,7 @@` | b | t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -509,7 +512,7 @@` | b | t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -532,7 +535,7 @@` | b | t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -553,7 +556,7 @@` | b | t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -577,7 +580,7 @@` | b | t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -597,7 +600,7 @@` | b | t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -616,7 +619,7 @@` | b | t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -661,7 +664,7 @@` | b | t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -685,15 +688,16 @@` | b | t64_host_allocator of t64_machine_create; t64_stop_result of t64_machine_advance; t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -702,7 +706,10 @@` | a | t64_machine_create (call) |
| `core/tests/test_vi.c` | `@@ -710,9 +717,10 @@` | b | t64_stop_request of t64_machine_advance; t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -771,7 +779,7 @@` | b | t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -789,8 +797,8 @@` | b | t64_stop_result of t64_machine_advance; t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -872,8 +880,8 @@` | b | t64_stop_result of t64_machine_advance; t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -898,8 +906,8 @@` | b | t64_stop_result of t64_machine_advance; t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -934,8 +942,8 @@` | b | t64_stop_result of t64_machine_advance; t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -966,8 +974,8 @@` | b | t64_stop_result of t64_machine_advance; t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -1014,8 +1022,8 @@` | b | t64_stop_result of t64_machine_advance; t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -1042,7 +1050,8 @@` | b | t64_stop_request of t64_machine_advance |
| `core/tests/test_vi.c` | `@@ -1060,7 +1069,7 @@` | b | t64_stop_result of t64_machine_advance |
| `core/tests/test_vi.c` | `@@ -1089,7 +1098,7 @@` | b | t64_stop_result of t64_machine_advance |
| `core/tests/test_vi.c` | `@@ -1130,8 +1139,8 @@` | b | t64_stop_result of t64_machine_advance; t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -1170,8 +1179,8 @@` | b | t64_stop_result of t64_machine_advance; t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -1227,8 +1236,8 @@` | b | t64_stop_result of t64_machine_advance; t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -1262,8 +1271,8 @@` | b | t64_stop_result of t64_machine_advance; t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -1286,8 +1295,8 @@` | b | t64_stop_result of t64_machine_advance; t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -1308,7 +1317,7 @@` | b | t64_stop_result of t64_machine_advance |
| `core/tests/test_vi.c` | `@@ -1330,13 +1339,13 @@` | b | t64_stop_result of t64_machine_advance; t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -1372,8 +1381,8 @@` | b | t64_stop_result of t64_machine_advance; t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -1418,7 +1427,8 @@` | b | t64_stop_request of t64_machine_advance |
| `core/tests/test_vi.c` | `@@ -1436,7 +1446,8 @@` | a | t64_machine_trace_drain (call) |
| `core/tests/test_vi.c` | `@@ -1491,7 +1502,7 @@` | b | t64_stop_result of t64_machine_advance |
| `core/tests/test_vi.c` | `@@ -1518,8 +1529,8 @@` | b | t64_stop_result of t64_machine_advance; t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -1533,7 +1544,10 @@` | b | t64_stop_request of t64_machine_advance |
| `core/tests/test_vi.c` | `@@ -1603,8 +1617,8 @@` | b | t64_stop_result of t64_machine_advance; t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -1656,8 +1670,8 @@` | b | t64_stop_result of t64_machine_advance; t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -1695,8 +1709,8 @@` | b | t64_stop_result of t64_machine_advance; t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -1742,7 +1756,9 @@` | b | t64_stop_request of t64_machine_advance |
| `core/tests/test_vi.c` | `@@ -1764,7 +1780,9 @@` | b | t64_stop_request of t64_machine_advance |
| `core/tests/test_vi.c` | `@@ -1779,7 +1797,7 @@` | b | t64_stop_result of t64_machine_advance |
| `core/tests/test_vi.c` | `@@ -1789,7 +1807,9 @@` | b | t64_stop_request of t64_machine_advance |
| `core/tests/test_vi.c` | `@@ -1823,7 +1843,7 @@` | b | t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -1859,7 +1879,8 @@` | b | t64_host_allocator of t64_machine_create |
| `core/tests/test_vi.c` | `@@ -1868,9 +1889,9 @@` | b | t64_cpu_stop_detail of t64_machine_cpu_stop_detail; t64_stop_result of t64_machine_advance; t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -1879,19 +1900,24 @@` | a + b | t64_machine_create (call); t64_stop_request of t64_machine_advance; t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -1914,7 +1940,8 @@` | b | t64_stop_request of t64_machine_advance |
| `core/tests/test_vi.c` | `@@ -1985,34 +2012,41 @@` | a + b | t64_machine_create (call); t64_host_allocator of t64_machine_create; t64_stop_request of t64_machine_advance; t64_stop_result of t64_machine_advance; t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -2071,10 +2105,14 @@` | b | t64_stop_result of t64_machine_advance; t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -2136,7 +2174,7 @@` | b | t64_stop_result of t64_machine_advance |
| `core/tests/test_vi.c` | `@@ -2195,7 +2233,10 @@` | b | t64_stop_request of t64_machine_advance |
| `core/tests/test_vi.c` | `@@ -2223,8 +2264,8 @@` | b | t64_stop_result of t64_machine_advance; t64_video_packet_info of t64_video_info |
| `core/tests/test_vi.c` | `@@ -2310,8 +2351,8 @@` | b | t64_stop_result of t64_machine_advance; t64_video_packet_info of t64_video_info |
| `core/tests/test_video_output.c` | `@@ -75,7 +75,8 @@` | b | t64_host_allocator of t64_machine_create |
| `core/tests/test_video_output.c` | `@@ -87,7 +88,10 @@` | a | t64_machine_create (call) |
| `core/tests/test_video_output.c` | `@@ -133,11 +137,14 @@` | b | t64_stop_request of t64_machine_advance; t64_stop_result of t64_machine_advance |
| `core/tests/test_video_output.c` | `@@ -157,14 +164,16 @@` | b | t64_stop_request of t64_machine_advance; t64_stop_result of t64_machine_advance |
| `core/tests/test_video_output.c` | `@@ -199,7 +208,7 @@` | b | t64_stop_result of t64_machine_advance |
| `core/tests/test_video_output.c` | `@@ -207,7 +216,9 @@` | b | t64_stop_request of t64_machine_advance |
| `core/tests/test_video_output.c` | `@@ -263,7 +274,7 @@` | b | t64_video_packet_info of t64_video_info |
| `core/tests/test_video_output.c` | `@@ -304,7 +315,7 @@` | b | t64_video_packet_info of t64_video_info |
| `core/tests/test_video_output.c` | `@@ -402,7 +413,7 @@` | b | t64_video_packet_info of t64_video_info |
| `core/tests/test_video_output.c` | `@@ -482,8 +493,8 @@` | b | t64_video_packet_info of t64_video_info |

Scratch proof before the flip commit (W-3), in the clone with the flip and the bound patch, start load 14 to 17
(unrelated host load, no limit widened): `cmake --preset gate` and `cmake --build --preset gate` pass (8 s, every
target including the nine bound test executables and `test_dp_transport`, `-Werror`); `ctest --preset gate` runs 274
tests in 85 s: 272 pass, including all 12 bound CTest names, and 2 fail, `harness.rdp-oracle-authority` and
`harness.phase10-qualification` (its `test_real_stage_commands_start` runs `check-rdp-oracle-authority.py
--expect-blocked` and expects exit 0). Both are the oracle identity block that only the orchestrator's rebind of
plan 11-14 clears: `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked` in the clone exits 2 with
`oracle-authority: authored corpus SHA-256 changed: core/tests/test_rdp.c` (the checker stops at the first changed
file); of the 13 `EXPECTED_CORPUS_PATHS` exactly the nine bound files differ from the shared tree (the other four,
`docs/spec/rdp-vi-cases.json`, `fixtures/rdp/fill.S`, `runner/tests/test_rdp_video.c` and
`scripts/check-rdp-vi-cases.py`, are byte-identical); with only the nine files restored to the pinned bytes in the
clone (`git checkout --` of the nine paths, the rest of the flip kept) the checker prints `authority record valid;
blocked: no oracle operation is authorized` (exit 0) and both harness tests pass; the patch was then re-applied and
its diff is byte-identical to the recorded patch. The two-tree sweep in the clone (the printer's head branch, selected
by `defined(T64_MACHINE_CREATE_INFO_INIT)`, runs for the first time) reports
`sweep: 1599 shared lines, 0 differences, 0 head-only lines` (base `f9124ae9fce1`, head the clone worktree, 43 s).
Planning gap for the orchestrator: the W-3 verify of 11-12 Task 3 and the 11-14 Task 1 clone gate both require a
green full gate in a clone whose nine bound files differ from the pinned corpus, while the same plans expect the
checker to report exactly that SHA change; the two harness names cannot pass before the 11-14 rebind.

Resolution (orchestrator decision of 2026-11-25, option A; recorded by plan 11-12): the two failures are the expected identity
block, not a defect. The checker is correct to report "authored corpus SHA-256 changed" once the bound call sites change, and
the evidence (the nine files restored to the pinned bytes make both names pass; the 12 bound CTest names and the other 272 pass;
the sweep reports 0 differences) shows nothing else differs. They clear only at the orchestrator's rebind of plan 11-14. Until
then every scratch-clone gate that holds the bound patch (plan 11-13 Task 3, plan 11-14 Task 1) excludes exactly those two names
or requires them to fail with exactly that checker message; the plans were amended accordingly, and the post-rebind gate of 11-14
must be fully green with every name. The flip commit is `0338380171fc198c175b78e70e499c1cee554fac`; its parent is the head the clone was taken from
(`d1bc22c7121afdf568b3007f0e70ff50307ac0c4`).

### 11-05 to 11-10 codemod

Written by plan 11-10 Task 2 from the "Per-file converted counts" tables of the 11-05 to 11-09
SUMMARY files and the 11-10 Task 1 run (one row per file the plans listed, a count of 0 is a
recorded count). "Converted" is the tool's own per-file count of brace initializers,
uninitialized declarations and compound literals moved to the DR-09 init macros by
`scripts/codemod-phase11.py`; "Hand-converted residuals" are the sites the tool reports as hand
sites (memsets of a converted object, arrays, sentinel fills) or does not detect (struct member
fills, pointer-form memsets), each done from the INIT form. Plan 11-05 covered the `runner/`
sources, 11-06 to 11-09 the four `core/tests` groups, 11-10 the `runner/tests` group.

| Plan | File | Converted | Hand-converted residuals |
|------|------|-----------|--------------------------|
| 11-05 | `runner/t64_real.c` | 9 | 1 (memset removed by hand) |
| 11-05 | `runner/t64_base_diagnostic.c` | 4 | none |
| 11-05 | `runner/t64_cold_boot.c` | 7 | none |
| 11-05 | `runner/t64_cpu_diagnostic.c` | 4 | none |
| 11-05 | `runner/t64_replay.c` | 3 | none |
| 11-05 | `runner/t64_rsp_diagnostic.c` | 5 | none |
| 11-05 | `runner/t64_rsp_smoke.c` | 5 | none |
| 11-05 | `runner/t64_skeleton_probe.c` | 3 | none |
| 11-05 | `runner/t64_raw_capture.c` | 0 | none |
| 11-05 | `runner/t64_raw_capture.h` | 0 | none |
| 11-05 | `runner/t64_audio_observation.h` | 0 | none |
| 11-05 | `runner/t64_supervisor.c` | 0 | none |
| 11-06 | `core/tests/fuzz_rom_admission.c` | 1 | none |
| 11-06 | `core/tests/test_rom_admission.c` | 4 | none |
| 11-06 | `core/tests/test_boot_determinism.c` | 15 | 2 (`machine_run.stop` after the memset in `initialize_run`, and for `observation`) |
| 11-06 | `core/tests/test_boot_entry.c` | 14 | none |
| 11-06 | `core/tests/test_bus.c` | 9 | none |
| 11-06 | `core/tests/test_cop1.c` | 5 | none |
| 11-06 | `core/tests/test_cpu_cache.c` | 3 | none |
| 11-06 | `core/tests/test_cpu_exceptions.c` | 7 | none |
| 11-06 | `core/tests/test_cpu_integer.c` | 4 | none |
| 11-06 | `core/tests/test_cpu_link.c` | 4 | 1 (`fixture.allocator`) |
| 11-06 | `core/tests/test_cpu_memory.c` | 1 | none |
| 11-06 | `core/tests/test_cpu_mmu.c` | 1 | none |
| 11-06 | `core/tests/test_cpu_path.c` | 54 | none |
| 11-06 | `core/tests/test_cpu_time.c` | 25 | none |
| 11-07 | `core/tests/test_rsp_compare.c` | 10 | 1 (`fixture->allocator`) |
| 11-07 | `core/tests/test_rsp_control.c` | 8 | 1 (`fixture->allocator`, context = fixture) |
| 11-07 | `core/tests/test_rsp_divider.c` | 4 | 1 (`fixture->allocator`) |
| 11-07 | `core/tests/test_rsp_fuzz.c` | 6 | 1 (`fixture->allocator`) |
| 11-07 | `core/tests/test_rsp_multiply.c` | 8 | 1 (`fixture->allocator`) |
| 11-07 | `core/tests/test_rsp_scalar.c` | 16 | 1 (`fixture->allocator`) |
| 11-07 | `core/tests/test_rsp_transfers.c` | 4 | 1 (`fixture->allocator`) |
| 11-07 | `core/tests/test_rsp_vector.c` | 2 | 1 (`fixture->allocator`) |
| 11-07 | `core/tests/test_sp_dma.c` | 12 | 2 (`value->allocator`, two fixture init functions, context = value) |
| 11-07 | `core/tests/test_pi_dma.c` | 22 | none |
| 11-07 | `core/tests/test_rdram.c` | 5 | none |
| 11-07 | `core/tests/test_ai.c` | 30 | 4 (arrays `whole_info`, `split_info`, `interleaved_info`, `reference_info`) |
| 11-07 | `core/tests/test_isviewer.c` | 18 | 6 sentinel fills (1 in `test_public_output_and_input_seams`, 4 in `test_output_drain_refusals_and_isolation`, 1 on `overlap.result`); 5 (reported by the tool; the sixth, on the union, was not) |
| 11-07 | `core/tests/test_joybus.c` | 3 | none |
| 11-08 | `core/tests/fuzz_state_import.c` | 3 | 1 (0xA5 fill of a report (poisoned)) |
| 11-08 | `core/tests/state_import_helpers.h` | 4 | 0 (adds `poison_import_report`, `import_report_is_poisoned`) |
| 11-08 | `core/tests/test_state_export.c` | 6 | 2 (zeroing memset of a drain result) |
| 11-08 | `core/tests/test_state_export_v26.c` | 1 | 6 (zeroing memset of a report) |
| 11-08 | `core/tests/test_state_import.c` | 43 | 2 (+1 corrected row) (0xA5 fill of a report (poisoned)) |
| 11-08 | `core/tests/test_state_import_v26.c` | 12 | 2 (1 reported 0xA5 fill; 1 pointer-form fill of `*report_out` the tool did not report) |
| 11-08 | `core/tests/test_save.c` | 8 | 5 (0xA5 fill of a save info (poisoned)) |
| 11-08 | `core/tests/test_save_transfer.c` | 6 | 5 (0xA5 fill of a save info (poisoned)) |
| 11-08 | `core/tests/test_digest_trace.c` | 27 | none |
| 11-08 | `core/tests/test_digest_v25.c` | 5 | none |
| 11-08 | `core/tests/test_digest_v26.c` | 3 | none |
| 11-08 | `core/tests/test_determinism.c` | 7 | 1 (allocator member of the fixture) |
| 11-08 | `core/tests/test_tracer.c` | 19 | none |
| 11-08 | `core/tests/test_si.c` | 3 | none |
| 11-09 | `core/tests/test_si_profile.c` | 13 | 1 (`ai_run.info` member) |
| 11-09 | `core/tests/test_sched_ledger.c` | 10 | none |
| 11-09 | `core/tests/test_sched_matrix.c` | 12 | none |
| 11-09 | `core/tests/test_scheduler.c` | 18 | 1 (`machine_fixture.allocator` member) |
| 11-09 | `core/tests/test_profile.c` | 2 | none |
| 11-09 | `core/tests/test_profile_v26.c` | 7 | none |
| 11-09 | `core/tests/test_lifecycle.c` | 13 | none |
| 11-09 | `core/tests/test_k1_refusal.c` | 1 | none |
| 11-09 | `core/tests/test_rdp_depth_composite.c` | 1 | none |
| 11-09 | `core/tests/test_vg_gaps.c` | 5 | 6 (3 arrays, 3 memsets) |
| 11-09 | `core/tests/test_warm_devices.c` | 10 | none |
| 11-09 | `core/tests/test_api_surface.c` | 0 | none |
| 11-09 | `core/tests/test_api_poison.c` | 13 | 3 (sentinel fills of `t64_stop_result`); 5 (3 scratch fill sites, 1 pointer-form fill of `observations`, 1 `instance.allocator` member) |
| 11-09 | `core/tests/test_api_lifecycle.c` | 18 | 15 (8 zeroing, 7 sentinel); 4 (1 scratch fill site, 1 `fresh_scratch`, 1 `larger` fill, 1 `instance.allocator` member) |
| 11-10 | `runner/tests/test_audio_capture.c` | 0 | none (a `const t64_audio_packet_info *` only) |
| 11-10 | `runner/tests/test_cpu_diagnostic.c` | 7 | none |
| 11-10 | `runner/tests/test_fixture_descriptor.c` | 1 | none |
| 11-10 | `runner/tests/test_input.c` | 4 | none |
| 11-10 | `runner/tests/test_pad_poll.c` | 4 | 1 (zeroing memset of `drain` removed) |
| 11-10 | `runner/tests/test_phase09_runner.c` | 0 | none (two `t64_profile_descriptor` lookups; no initializer) |
| 11-10 | `runner/tests/test_real_controls.c` | 0 | none (the edited descriptor copy is a whole-struct assignment `changed = *profile`, kept: after the flip it carries the registry's `struct_size`) |
| 11-10 | `runner/tests/test_real_runner.c` | 1 | none |
| 11-10 | `runner/tests/test_replay.c` | 6 | 1 (zeroing memset of `drain` removed) |
| 11-10 | `runner/tests/test_state_runner.c` | 5 | 1 (zeroing memset of `*report_out` in `inprocess_import`, found by the guard, commit `e22030d`) |
| 11-10 | `core/tests/test_state_import.c` | 0 | 1 (0xA5 canary memset of the aliasing report span becomes `poison_import_report`, found by the guard, commit `e22030d`; the 43 conversions of 11-08 are above) |

Totals converted by the tool: 11-05 40, 11-06 147, 11-07 148, 11-08 147, 11-09 123, 11-10 28
(brace 22, uninitialized 5, compound literal 1); 633 in all. Hand-converted residuals as the
SUMMARY files record them: 11-05 1, 11-06 3, 11-07 20, 11-08 24, 11-09 35, 11-10 4 (two memsets
removed in Task 1, two pointer-form report fills found by the guard); 87 in all. The `runner`
directory beyond the 12 files of 11-05 and the `tests` and `tools` directories hold no site.

Reconciliation with "Bound-file inventory" (11-01). The nine API-using bound files are the only
files with an initializer form left; `runner/tests/test_rdp_video.c` has none. Run without the flag,
`python3 -B scripts/check-api-init.py` reports 120 findings in exactly those nine files, the 120
category (b) edits of the probe, per file: `test_rdp.c` 5, `test_rdp_copy.c` 2,
`test_rdp_triangles.c` 2, `test_rdp_texture.c` 2, `test_rdp_combiner.c` 2, `test_rdp_blender.c` 3,
`test_rdp_depth.c` 2, `test_vi.c` 90 (75 positional, 13 compound literal, 2 uninitialized),
`test_video_output.c` 12 (9 positional, 3 compound literal); by type `t64_host_allocator` 16,
`t64_stop_request` 16, `t64_stop_result` 35, `t64_cpu_stop_detail` 1, `t64_video_packet_info` 52
(the probe's 16, 16, 35, 1 and 52). The 17 category (a) edits are call expressions, which no
initializer rule sees, and the one category (c) hunk (`test_rdp.c` line 486, `t64_host_storage`) is
exempt by type (the core fills `t64_host_storage.struct_size`, DR-06), so neither is a finding.
With `--exclude-oracle-bound` the guard scans 172 files and reports 0.

State at the end of 11-10 (`python3 -B scripts/codemod-phase11.py --check core/tests runner tests`):
the nine bound files and `runner/tests/test_rdp_video.c` listed as excluded, `runner` and `tests`
with no site, and `core/tests` with 7 hand listings that are all arrays whose every entry already
uses the INIT form (4 in `test_ai.c`, 3 in `test_vg_gaps.c`): the tool lists every array of a macro
type as a hand site by design (recorded since 11-07), so its exit status stays 1; the guard accepts
such arrays and is the arbiter from here on. Open item for plans 11-12 and 11-13, not acted on
here: `core/tests/test_profile.c` line 340 (`t64_profile_descriptor empty = {0};` hashed for an
INVALID_ARGUMENT check) is the one non-bound zero descriptor; the guard accepts a lone zero for a
descriptor (the deliberate empty-descriptor fixture), but after the flip its leading `struct_size`
is 0, so the expected status of that row changes and the test needs the descriptor's `struct_size`
set by hand.

### 11-04 NOT_READY table: every machine-taking entry after `t64_machine_unload`

Written by plan 11-04 Task 2 from the code of each entry (grep of `has_image` over `core/*.c`)
and pinned by `unit.core.api.not-ready-table`, which runs every row on four profiles (the warm
skeleton-class profile, the composite family profile with the 4 KiB EEPROM, the VI profile and
the VI plus AI profile) against an unloaded instance and, for each row, against an instance that
never admitted an image (the same status both times, so unload leaves no image-derived state).
Rule: an entry that needs an image answers `NOT_READY` and leaves every output byte as the
caller left it; an entry documented as image-independent answers as it does on a new instance.
No entry needed a new `has_image` test; the three entries that do not test it are image
independent by their code (the debug bus reaches RDRAM and device registers, the trace calls
are observer controls over a ring that exists from creation, and `t64_machine_profile` reads the
kept descriptor).

| # | Entry | After unload | Outputs | Decision source |
|---|-------|--------------|---------|-----------------|
| 1 | `t64_machine_admit_rom` | OK (admits) | written | image-independent: an unloaded instance admits exactly as a new one; digest equals a fresh admission (`unit.core.api.unload`) |
| 2 | `t64_machine_advance` | NOT_READY | untouched | `core/t64_machine.c` has_image test |
| 3 | `t64_machine_content_digest` | NOT_READY | untouched | has_image test |
| 4 | `t64_machine_rom_size` | NOT_READY | untouched | has_image test |
| 5 | `t64_machine_rom_header` | NOT_READY | untouched | has_image test |
| 6 | `t64_machine_profile` | OK | written | image-independent: profile and allocator are kept |
| 7 | `t64_machine_digest` | NOT_READY | untouched | `core/t64_digest.c` walker refuses without an image |
| 8 | `t64_machine_state_export` | NOT_READY | untouched | `core/t64_state.c` has_image test, before any write |
| 9 | `t64_machine_state_import` | NOT_READY | the import report is filled (reason `NOT_ADMITTED`), no other byte | `core/t64_state.c`; the report is the caller record of the attempt |
| 10 | `t64_machine_cpu_stop_detail` | NOT_READY | untouched | has_image test |
| 11 | `t64_machine_output_drain` | NOT_READY | untouched | `core/t64_observe.c` has_image and boot-prepared test |
| 12 | `t64_video_info` | NOT_READY | untouched | `core/t64_vi.c` has_image test |
| 13 | `t64_video_copy` | NOT_READY | untouched | `core/t64_vi.c` has_image test |
| 14 | `t64_machine_save_info` | NOT_READY | untouched | `core/t64_save.c` has_image test (precedes the device checks) |
| 15 | `t64_machine_save_export` | NOT_READY | untouched | `core/t64_save.c` has_image test |
| 16 | `t64_machine_save_import` | NOT_READY | n/a (input only) | `core/t64_save.c` has_image test |
| 17 | `t64_audio_info` | NOT_READY | untouched | `core/t64_ai.c` has_image test |
| 18 | `t64_audio_copy` | NOT_READY | untouched | `core/t64_ai.c` has_image test |
| 19 | `t64_machine_queue_input` | NOT_READY | n/a | `core/t64_observe.c` has_image test |
| 20 | `t64_machine_bus_read` (RDRAM address) | OK | written | image-independent: RDRAM and device registers exist from creation |
| 21 | `t64_machine_bus_read` (cartridge window) | BUS_UNSUPPORTED | untouched | the window has no bytes without an image, and a new instance answers the same; an admitted instance answers OK |
| 22 | `t64_machine_bus_write` (RDRAM address) | OK | n/a | image-independent |
| 23 | `t64_machine_bus_write` (cartridge window) | BUS_DEVICE_REFUSED | n/a | the window is read-only with or without an image (the DR-13 carve-out names what a refusing device may latch) |
| 24 | `t64_machine_trace_set_enabled` | OK | n/a | observer control, image-independent |
| 25 | `t64_machine_trace_drain` | OK (zero records) | written | observer ring exists from creation; unload clears it |
| 26 | `t64_machine_reset` | NOT_READY | n/a | no image to reset |
| 27 | `t64_machine_unload` | NOT_READY | n/a | no image to release; counters unchanged |
| 28 | `t64_machine_is_poisoned` | OK (0) | written | query, no guard |
| 28a | `t64_machine_output_status` | OK, zero counts, full free capacity, no pending text | written (struct_size never) | image-independent by design (Task 3): a host paces from it before the first admission as well; `unit.core.api.output-status` checks the empty cases with and without an image |
| 29 | `t64_machine_destroy` | releases the remaining blocks (no image block) | n/a | `unit.core.api.unload` asserts allocations equal releases and zero outstanding after unload then destroy |

Row 28a was added by plan 11-04 Task 3 (the table test carries it as the `output_status` row). Row 21 is the one
row where an image-requiring read does not answer `NOT_READY`: it keeps today's bus answer so no
pre-admission observation changes.

## Install steps

Appended by each INSTALL plan (11-16 starts it). Evidence tier: unit-tested, authored scope; a
software-interface claim only. Native seconds are the `Passed` seconds that ctest printed on the
local host (load 10 to 20), a range over repeated runs; each new name carries an explicit `COST`
equal to its measured seconds (W-11), so `CTestCostData.txt` no longer holds native seconds for
these names.

### 11-16 INSTALL tracer (sha256 fold, package export, moved prefix, independent consumer)

| Item | Result |
|------|--------|
| Commits | `31df5de` (sha256 fold and package export), `c197e4c` (isolated no-HLE inventory refresh), `904b55c` (driver, consumer, install CTest names), `ea83392` (consumer guard, init guard over every consumer), `0213aba` (preflight row count), the preflight guard-default fix and this record |
| Rename choice for the folded sha256 TU | the four private compile definitions of RESEARCH (`t64_sha256_{init,update,final,hex}=t64i_sha256_*` on `t64_core` only); `scripts/gen-prefix-header.py` and `core/t64_prefix.h` are unchanged (generator `--check`: 35 public functions, 220 renames), so host tools keep linking `t64_sha256` with the unprefixed names. The archive now defines 259 globals on macOS: 35 public and 224 `t64i_` (220 renames plus the four sha256 names) |
| Link fallout | `test_tracer` and `test_isviewer` called `t64_sha256_*` through the old transitive private link of `t64_core`; both now list `t64_sha256` explicitly (the only two link failures of the full gate build) |
| Package | six files in the moved prefix (`lib/libt64_core.a`, `include/tendies64/t64_machine.h`, `lib/cmake/tendies64/tendies64{Config,ConfigVersion,Targets,Targets-release}.cmake`), asserted by `scripts/check-install.sh setup`; `tendies64ConfigVersion.cmake` is `VERSION 0.1.0` `SameMinorVersion`; `project(VERSION)` still `0.0.0` (1 line); the config sets `tendies64_BUILD_ID` and `tendies64_BUILD_TYPE` (`Release`); no file of the moved prefix or of the consumer tree mentions the original install path, and the moved prefix holds no `/Users/`, `/private/` or `/var/folders` string |
| Consumer | `tests/installed-consumer/` is a separate top-level project (never `add_subdirectory`'d, outside the `tools/*` glob), `find_package(tendies64 0.1 REQUIRED CONFIG)` from the moved prefix whose path contains spaces; the smoke case runs 13 checks, 0 failures: `t64_abi_version() == T64_ABI_VERSION`, `t64_build_identity()` equals `tendies64_BUILD_ID`, and a create and destroy round trip through a counting host allocator releases every allocation (outstanding 0, no bad release) |
| No-HLE inventory refresh | `scripts/check-rsp-no-hle.py` reported exactly three findings, all about the one object `core/CMakeFiles/t64_core.dir/__/tools/common/t64_sha256.c.o`, before the edit and none after; the edit adds that one expected object edge, archive member and on-disk object; no body digest, pin or seed changed; `repo.rsp-no-hle` passes (18 s) |
| Sweep driver | `python3 -B scripts/digest-sweep.py --selftest` passes on the edited CMake (the literal `set(T64_BUILD_ID ...)` line occurs once) |
| New CTest names (native seconds) | `install.setup` 0.05 to 0.08, `installed.consumer.build` 0.43 to 0.53, `installed.consumer.smoke` 0.02 to 0.03, `repo.consumer-include-guard` 0.06 to 0.07, `repo.consumer-include-guard-selftest` 0.61 to 1.01 (five names, 280 in total; `repo.api-init-guard` 0.98 s now also scans the nine bound files and the consumer) |
| Preflight | `sh scripts/preflight-phase11.sh` PASS (77 s at load 12 to 16): ctest-counts, 26 `phase11` tests equal to the 26-name block, guards, timeout-audit (133 TIMEOUTs, 0 too low), pii (tree and 18 unpublished commits), linux-compile (233 units under the act image gcc 13.3.0, 43 s), post-gate replay |

Linux pre-flight of the folded archive (Task 3). Image: `ghcr.io/catthehacker/ubuntu:act-24.04`
(amd64 under emulation, gcc 13.3.0, GNU binutils 2.42), already on disk, no new image. The 86
translation units of `core/libt64_core.a` (85 core and SoftFloat units plus `tools/common/t64_sha256.c`)
were compiled with the ninja compile commands of `build/gate` (checkout mapped to `/src`, macOS flags
removed) into objects and an `ar` archive: 86 objects, 259 defined globals of which 35 unprefixed
(the 35 public functions) and 224 `t64i_`; `gen-prefix-header.py --check` on that Linux archive
reports `ok (35 public functions, 220 renames)`, so the nm parsing of the generator and the prefix
hold on ELF as well. `cmake --install` is not run there (the act image has no cmake and no ninja):
the Linux install path stays hosted-only, first exercised by the push at the end of this plan.

Linux undefined set (act image, object compile), measured in the act image; confirmed or refuted by
the hosted gate-linux run of 11-21. It is `nm -P -u` over the 86 objects, names only, minus the names
`nm -P -g --defined-only` reports over the same objects, `LC_ALL=C sort -u` (5 names). Plan 11-17
writes the Linux rows of `docs/abi/installed-undefined.txt` from this table only.

| Linux undefined name | Origin |
|----------------------|--------|
| `_GLOBAL_OFFSET_TABLE_` | linker-defined symbol that position-independent objects reference (Ubuntu gcc default PIE code) |
| `__stack_chk_fail` | stack protector (Ubuntu default) |
| `memcmp` | libc |
| `memcpy` | libc |
| `memset` | libc |

For comparison, the macOS archive of the same tree has six undefined names (`__stack_chk_fail`,
`__stack_chk_guard`, `bzero`, `memcmp`, `memcpy`, `memset`); the macOS set is measured from the real
archive by plan 11-17.

Hosted evidence (Task 4). Pushed `e5a2e3c8ad11d40991c0c2048999d339261e3146` to `origin/main`
(fast-forward from `43b31e2`, no tag, 20 commits; `--tree` and `--range origin/main..HEAD` PII scans
clean, preflight PASS, full `sh scripts/gate.sh` PASS 280 of 280 in 176 s before the push). Hosted run
45742744889 (event push) succeeded on both jobs: gate-linux 279 s and gate-macos 496 s. The first
hosted Linux install path (RESEARCH A3, DR-23) worked as on macOS: `install.setup` 0.04 s, consumer
build 0.30 s, smoke 0.02 s on gate-linux (macOS 0.46 s, 1.74 s, 0.06 s); the six files installed under
`lib/` (GNUInstallDirs gives `lib` for a custom prefix), `repo.abi-prefix` passed on the ELF archive
and `repo.rsp-no-hle` passed with the folded member. The hosted run does not print the archive-level
undefined set; the Linux undefined set table above stays the act-image measurement until the hosted
run of 11-21 confirms or refutes it. Finding F-128 and the Runs row are in `docs/ci-reviews/LOG.md`.

### 11-17 Cheap-first ABI checks and API-04 on the shipped archive

Evidence tier unchanged: unit-tested, authored scope; a software-interface claim only. Native seconds
are the `Passed` seconds ctest printed at host load 10 to 17 (a range over repeated runs); every new
name carries an explicit `COST` equal to its measured seconds.

| Item | Result |
|------|--------|
| Layout pins (Task 1, `b4060bb`) | `docs/abi/layout.c` is generated by `check-abi.py layout --write` from a probe program that prints `sizeof` and `offsetof` of every public struct (14 structs, 165 members) and the value of every integer `T64_*` define plus the size of the two string defines (190 constants). One assertion per line through `T64_LAYOUT_PIN`: C++17 `static_assert`, C11 `_Static_assert`, and for C99 the negative-array typedef (C99 has no static assertion, and `_Static_assert` under `-std=c99 -pedantic -Werror` is a diagnosed extension). A 64-bit-pointer guard (`UINTPTR_MAX`) wraps the pins; other data models compile the file without checking anything. `layout --check` compiles the file against the include directory as C99, C11 and C++17 (`-pedantic -Wall -Wextra -Werror -fsyntax-only`) and then regenerates it from that directory and diffs, so an added, removed or changed member or constant fails even though the pins alone could not see an addition |
| Layout seeded failures | an offset edited in a temporary copy of `layout.c` fails all three compiles and the regeneration diff (`selftest layout-offset-edited`); a member added to `t64_save_info` in a temporary copy of the header fails the committed layout (`selftest layout-header-drift`); both are cases of the existing `repo.abi-selftest` (no new name) |
| New names, Task 1 (native seconds) | `repo.abi-layout` 0.19 to 0.47, `install.abi-header` 0.10 to 0.15, `install.abi-layout` 0.18 to 0.42 |
| nm golden (Task 2) | `docs/abi/public-symbols.txt` holds the 35 public functions (no banner: `wc -l` is 35), written from the moved installed archive by `check-abi.py symbols --write`; the archive defines 224 further globals that all start with `t64i_`; every defined global is an `nm` type `T` (no data symbol). `symbols --check` fails an unexpected unprefixed name, a missing public name, a non-text global, an unsorted or duplicated golden; the synthetic-`nm` seeds are `selftest symbols-controls` |
| Path scan (Task 2) | `check-abi.py abspaths` reads the bytes of every file of the moved prefix (the archive included, so no `strings(1)` dependency) and fails on the source root, the build root, `/Users/`, `/home/`, `/private/` or `/var/folders`; the two roots are named in findings by position, never by value; an install prefix without `lib/libt64_core.a` is a usage error (an empty scan proves nothing). Result on the real prefix: 6 files, 0 findings. Seeds: `selftest abspaths-controls` |
| New names, Task 2 (native seconds) | `install.abi-symbols` 0.08 to 0.11, `install.abs-paths` 0.07 |
| Preflight (Task 2) | full `sh scripts/preflight-phase11.sh --no-replay` PASS (67 s at load 15; linux-compile 62 s of it); the `abi-cheap` step ran 10 names (the five `repo.abi*` names, `install.setup` through the fixture closure, and the four `install.*` names above) in 1 s, within the 15 s target; `repo.abi-selftest` itself is 1.1 s with the four new seeded cases |
| Q6 spaced-prefix ar x (Task 3, RESEARCH Open Question 6, I-8) | works. `ar x` was run once from the archive path inside the moved prefix of the `t64_install` fixture (quoted, `moved prefix/lib/libt64_core.a`, a path that contains a space) into a space-free `mktemp -d` directory under `TMPDIR` and extracted 86 members: the `ar t` listing minus the BSD symbol table entry `__.SYMDEF SORTED` (a further file whose name contains a space, excluded by the `*.o` glob and by the member count). `lint-objects.sh --objects` over those members ran with the installed allowlist and reported nothing. No fallback copy was needed; `scripts/check-api04.sh` reports `route=in-place archive-path-has-space=yes` and keeps the copy-then-extract fallback for a platform where the in-place `ar x` fails. The check lives in 11-17 Task 3, not in 11-10, because the spaced, moved prefix exists only from 11-16 (INFO-02) |
| Installed allowlist (Task 3) | `tools/lint/allowlist-installed.txt` holds exactly three rows (`__stack_chk_fail` and `__stack_chk_guard` on every platform, `_GLOBAL_OFFSET_TABLE_` on Linux only), each with its reason and the ADR-0006 amendment of 2026-11-25; `lint-objects.sh --allow FILE` merges it for one run (OS column honored, default unchanged); `tools/lint/allowlist.txt` is byte-identical to the Wave 0 base and the lint's own compile flags are untouched |
| RED (Task 3) | with the installed allowlist holding no row, `check-api04.sh members` over the installed archive failed with 26 `L2-UNDEF-SYMBOL` lines, 13 members times the two stack-protector names `__stack_chk_fail` and `__stack_chk_guard` (saved to the ignored build tree, `11-17-task3/red.txt`); with the three rows it passes |
| Recorded undefined set (Task 3) | `docs/abi/installed-undefined.txt` Darwin rows measured from the real archive by `check-api04.sh members` (archive-level undefined minus defined over the 86 members): `__stack_chk_fail`, `__stack_chk_guard`, `bzero`, `memcmp`, `memcpy`, `memset`. The Linux rows (`_GLOBAL_OFFSET_TABLE_`, `__stack_chk_fail`, `memcmp`, `memcpy`, `memset`) are copied from the act-image table of 11-16 Task 3 only and are act-measured, not hosted-measured: the hosted gate-linux run of 11-21 confirms or refutes them, and a mismatch there is a finding for a new numbered plan. The control run without the installed allowlist names exactly `__stack_chk_fail` and `__stack_chk_guard` (26 lines) on macOS |
| New name, Task 3 (native seconds) | `api04.members` 6.0 to 7.8 at host load 15 to 43 (two lint runs side by side over 86 members; the lint dominates) |
| API-04 results (Tasks 3 and 4) | on the installed archive (86 members, 95,916 disassembled instruction lines on macOS arm64): `api04.members` clean with the installed allowlist and the archive-level undefined set equals the six Darwin rows; `api04.disasm` finds no `mrs` of CNTVCT, CNTPCT or RNDR and no `rdtsc`, `rdtscp`, `rdrand`, `rdseed` or `rdpmc` (the scan refuses an empty disassembly); `api04.headers` runs the Layer-1 scan over the one installed header (`files=1 violations=0`, the `<stdbool.h>`, `<stddef.h>` and `<stdint.h>` includes are on the L1-INCLUDE allowlist) |
| Seeded violations (Task 4) | `api04.seeded`: members plus `seed_host_services.c` fail with exactly four `L2-UNDEF-SYMBOL` lines naming `clock_gettime`, `fopen`, `getentropy` and `pthread_create`; members plus an object importing `memmove` (allowlisted by the source allowlist, so the lint passes it) fail the expected-set comparison with `unrecorded import: memmove`; members plus `seed_hw_counter.c` fail the disassembly scan naming `mrs x8, CNTVCT_EL0`; the installed include directory plus `seed_banned_include.h` fails with `L1-INCLUDE-BANNED`. The seeds are in `tools/lint/installed-seeds/` (no `tools/lint/selftest/bad_installed*` file exists, so `core-lint.sh --selftest` still runs its 27 seeds only) |
| RED (Task 4) | a copy of the script with the lint, the Layer-1 scanner and the disassembly patterns replaced by no-ops reports three of the four seeded runs as `NOT DETECTED` and exits 1 (the unrecorded-import control compares sets and is independent of the lint); the real script reports all four detected and exits 0 (saved to the ignored build tree, `11-17-task4/red.txt`) |
| New names, Task 4 (native seconds, `CTestCostData.txt` after three runs at host load 15 to 40) | `api04.disasm` 0.78, `api04.headers` 0.14, `api04.seeded` 6.58; with Task 3 and the earlier tasks: `repo.abi-layout` 0.36, `install.abi-header` 0.14, `install.abi-layout` 0.34, `install.abi-symbols` 0.19, `install.abs-paths` 0.14, `api04.members` 7.82; nine new names in the gate (289 names in total, was 280), about 16 s of test time at that load (about 8 s of it `api04.members` and `api04.seeded`, which share the lint over 86 members) |
| abi-cheap step with every new name | `sh scripts/preflight-phase11.sh --no-replay` PASS (94 s at load 20 to 33, linux-compile 81 s of it); the `abi-cheap` step ran 14 names (five `repo.abi*`, `install.setup` through the fixture, four `install.*`, four `api04.*`) in 8 s at that load, within the 15 s target |
| Linux pre-flight of the 11-17 checks (act image, not hosted) | image `ghcr.io/catthehacker/ubuntu:act-24.04` (gcc 13.3.0, GNU binutils 2.42, Python 3.12.3), already on disk, no new image. The 86 core objects were compiled with the ninja compile commands of `build/gate` (checkout mapped to `/src`, macOS flags removed) into an `ar` archive inside a path with a space, next to the header. Results: `install.abi-header` and `install.abi-layout` equivalents pass (C99, C11 and C++17, regenerated file identical, 14 structs, 165 members, 190 constants); the nm golden holds (35 public, 224 `t64i_`, 0 findings); the path scan has 0 findings; `check-api04.sh undefined` prints `_GLOBAL_OFFSET_TABLE_`, `__stack_chk_fail`, `memcmp`, `memcpy`, `memset`, equal to the recorded Linux rows; `members` passes with `ar x` in place from the spaced path (86 members, GNU `ar`, so Q6 holds on GNU as well) and the control without the installed allowlist names exactly `_GLOBAL_OFFSET_TABLE_` and `__stack_chk_fail` (33 lines); `disasm` passes (85,358 instruction lines); `headers` passes; `seeded` detects 4 of 4 (the counter seed names `rdtsc`). `cmake --install` is not run there (no cmake in the image), so the hosted run stays the first real Linux install; the recorded Linux rows are therefore now two act-image measurements (11-16 and this one) and still no hosted one |
| Full gate (11-17 closeout) | `sh scripts/gate.sh` PASS 289 of 289 names (was 280), ctest 96 s, total 156 s at host load 9 to 11 (the run waited for the load to fall below 10; no limit changed) |

### 11-18 Installed consumer functional suite (composite three-way, accepted and refused case per operation)

Evidence tier unchanged: unit-tested, authored scope; a software-interface claim only. Native seconds
are the `Passed` seconds ctest printed at host load 5 to 10 (a range over three runs); every new name
carries an explicit `COST` equal to its measured seconds, and `tools/abi/CMakeLists.txt` refuses to
configure a case of `tests/installed-consumer/cases.txt` that has no recorded cost.

| Item | Result |
|------|--------|
| Commits | `588da28` (Task 1: tracer, composite against independent literals), `d244b6e` (Task 2: three-way and nine operation cases), and the Task 3 commit that holds this record |
| B-1 runner-recorded pins | `fixtures/composite/composite.runner-pins.json` carries exactly the label `runner-recorded pin, not independent`, the source `Phase 10 PIN-IMPACT.md section 11.3, plan 10-14 supervised runs (raw-capture and save-out rows)` and four values: completion tick 599357, stop reason GUEST_COMPLETION, final state digest `97f27027...1267c`, video packet count 3. `scripts/check-consumer-literals.py` generates `tests/installed-consumer/t64c_composite.h` from it, `composite.expected.json` and `composite.input.json` (two labeled sections: INDEPENDENT and RUNNER-RECORDED PINS), `--check` diffs the committed header, refuses a pins file whose label is not exact and requires the cited Phase 10 record to contain `599,357` and the digest (DR-32); its seeded `--selftest` has 7 cases (clean control, flipped header byte, wrong label, tick not in the record, digest not in the record, wrong schema, header stale after a fixture edit) and is part of `installed.consumer.literals` |
| Three-way (G6) | `sh scripts/check-install.sh three-way` runs the in-tree runner as the two supervised runs of `runner.composite-two-run` (a raw-capture run and a save-out run, the runner is not re-pointed at the installed package) and the installed consumer's composite case stepping 16 ticks per advance like the runner; the consumer's exit status carries the independent-literal leg (12 literal comparisons). It prints one line per value with its source class: final state digest (runner raw run = runner save run = consumer), completion tick, stop reason and video packet count are labeled `runner-recorded pin, not independent`; the three video packets, two audio packets, the 32,768-byte save and the 188-byte transcript are compared byte for byte (`cmp`) and labeled as independent literals checked by the consumer. Result: all equal, digest `97f270274d456e883ceaa11f100f92c7fbf3150e89031fa51164e79991f1267c` on both sides, tick 599357 |
| Slice independence | the composite case reaches the same digest, tick, packets, transcript and save at 4096 ticks per advance (the case default), at 16 (the runner's step), at 256 and at 1024 (each checked once by hand with the full case, 25 checks, 0 failures); `ops-reset` also runs the guest twice on one instance and compares the two runs |
| Consumer independence of the checks | the hashes of the case are computed by `t64c_sha256.c` (FIPS 180-4, own code, self-checked against the empty, `abc` and 448-bit published vectors at start), never by the core's hash and never from `captured_sha256`; `content_digest` is compared with the consumer's own SHA-256 of the ROM file bytes |
| Seeded failures shown once | a copy of the consumer with one byte of the video literal changed (scratch build against the moved prefix) fails the composite case with `video packet ...: DIFFERENT` (`checks=25 failures=1`); a copy with `T64_STATUS_NOT_READY` classed as a guest-stop in the consumer's class table and one expected status changed fails `ops-unload` (6 failures) and `ops-input` (2 failures) |
| Case list | `tests/installed-consumer/cases.txt` lists exactly the 19 binary cases (smoke, composite and the 17 `ops-*` cases); `scripts/check-install.sh build` fails when `t64c --list-cases` and the file differ; the three script names (build, literals, three-way) are not binary cases. `installed.consumer.smoke` is registered once, by its explicit 11-16 registration, and the foreach skips that row |
| ctest counts | `ctest -N -R '^installed\.consumer\.'` is 5 after Task 1, 15 after Task 2 and 23 after Task 3: the plan's 4, 14 and 22 do not count `install.setup`, which CTest adds through the `t64_install` fixture of `installed.consumer.build`; `-N -R '^installed\.consumer\.smoke$'` is 3 (smoke, build, setup), not 1, for the same reason. The preflight row is 23 |
| Observed backpressure stop (RESEARCH ASSUMED item resolved) | an undrained run of the composite stalls with `RESOURCE_LIMIT` at guest tick 540952 (after 260 stops at the per-advance event budget of 4096 events, each of which moved the tick): the video ring holds two packets and has no free slot, the third frame cannot dispatch, and a further advance returns `RESOURCE_LIMIT` at the same tick with the machine digest unchanged. Draining the two packets lets the next advance progress to the guest's completion at 599357; the run delivers 3 video packets (pin) and 2 audio packets, every video packet equal to the 48-byte literal. At completion the audio ring is full (2 queued, 0 free), 188 transcript bytes are pending and the video ring is empty |
| Refusal classes asserted | every refusal asserts the exact status and its class `refused` in the consumer's own table `t64c_classes.h` (hand-written from `docs/abi/status-classes.txt`; plan 11-20 adds the `repo.abi-status-classes --consumer-table` comparison), and either an unchanged machine digest or, for a machine without an image (which has no digest: `t64_machine_digest` answers NOT_READY), that it still answers NOT_READY; refused outputs are compared with a sentinel byte copy |
| Operations covered | create (caller faults, the allocator refusing the k-th allocation for every k, REENTRANT from inside an allocator callback), the handshake (abi minor +1 and -1, major +1, create-info, allocator and profile struct_size one below v1, a larger create-info), struct_size one below v1 for advance request and result, cpu_stop_detail, output_drain result, output_status, video_info, audio_info, save_info, state_import report, profile_hash, profile_serialize, trace_drain record_size, and larger caller structs with 16 sentinel bytes behind them for advance result, output_status, save_info, output_drain result, state_import report, cpu_stop_detail, profile_hash; profile lookup, hash and serialize; admit (length, signature, NULL, allocator refusal, previous image kept, replacement); input (port, both axes, past tick, QUEUE_FULL at 255 queued, retry after an advance); advance (all three request kinds, TIME_REVERSED, LIMIT_REVERSED, unknown kind, NULL request, result and machine, the terminal GUEST_COMPLETION repeating at the same tick); reset; unload; video, audio and text rings; save info, import and export in the three byte orders with R0 to R9 (R9 SAVE_BUSY produced through a PI DMA into the SRAM window); state export and import (round trip to the same digest, a second instance, DROP_OBSERVERS, a NULL report, ten hostile blobs with the documented status and reason: flag bit, length 0, NULL blob, truncated, magic, container version, one flipped body byte, and profile digest, content digest and build identity with the trailer recomputed by the consumer's own SHA-256); queries; trace (observer-only digest, FIFO order, a record_size above v1, below v1); bus (reads of the cartridge window equal the file bytes, writes at three widths, every refusal status, the read-only cartridge window refusal that changes nothing, a rejected PI request that leaves the public observations alone and adds exactly one PI_REFUSED trace record); is_poisoned |
| New names (native seconds, three runs) | `installed.consumer.composite` 0.11 to 0.13, `.literals` 0.07, `.three-way` 0.48 to 0.57, `.ops-lifetime` 0.02 to 0.03, `.ops-handshake` 0.02, `.ops-struct-size` 0.32 to 0.33, `.ops-profile` 0.25 to 0.26, `.ops-admission` 0.36 to 0.37, `.ops-input` 0.30, `.ops-advance` 0.37, `.ops-reset` 0.27 to 0.28, `.ops-unload` 0.06 to 0.07, `.ops-rings` 0.44, `.ops-text` 0.23, `.ops-save` 0.48, `.ops-state` 1.08 to 1.10 (about ten 10.5-MB container validations; the heaviest name), `.ops-queries` 0.46, `.ops-trace` 0.20 to 0.22, `.ops-bus` 0.34, `.ops-poison-query` 0.11 to 0.12. Twenty new names, about 6.6 s of native test time in total; at the hosted multiplier of 4.3 about 28 s more on gate-macos, which the orchestrator's measured cold run must confirm against the 900 s limit before the 11-21 ledger row (`ops-state` is the first name to trim if the margin is short) |
| Observations kept out of the assertions (for the SHAPE-style reviews) | (a) the openipl3 authored-profile admission accepts only the big-endian dump order: the byte-swapped (`37 80 40 12`) and word-reversed (`40 12 37 80`) forms of the composite ROM, which `docs/spec/machine-contract.md` lists as accepted raw orders, answer INVALID_ROM on this profile, so no case asserts either order; (b) the frame-event stop kind counts synthetic frame events that only the in-tree scheduler tests schedule, so through the public API only the limit 0 is reachable (a limit of 1 runs to the guest's end); (c) `t64_machine_digest` answers NOT_READY on an instance with no image (before an admission and after an unload), so a refusal on such an instance cannot compare digests and asserts NOT_READY instead (admission after an unload still reaches the first-admission digest); (d) bus read and write and the trace enable and drain need no admitted image; (e) the header does not say what `t64_profile_lookup` leaves in `*profile_out` on a refusal, so the case asserts only that no descriptor is returned |

### 11-19 Host-shaped loop, negative controls, symbol collision and the plain cc line

Evidence tier unchanged: unit-tested, authored scope; a software-interface claim only. Native seconds are
the `Passed` seconds ctest printed at host load 10 to 20 (a range over three runs); every new name carries
an explicit `COST` equal to its measured seconds. The host reads no clock and passes the library nothing
but guest ticks; the injected delay is a sleep, never a value.

| Item | Result |
|------|--------|
| Commits | `caf2da0` (Task 1: host-shaped loop), `6b29352` (Task 2: negative-control matrix), `e117e55` (Task 3: symbol collision and plain cc), and the commit that holds this record |
| Host-shaped loop (`hostloop.c`) | frame budget of 8192 guest ticks (a host policy value: a real 60 Hz frame at the profile's 62,500,000 ticks per second is 1,041,666 ticks and would hold the whole composite run), 74 host frames; each input event is queued before the frame that holds its tick (not up front); after an advance the host drains video, then audio, then ISViewer text; a RESOURCE_LIMIT stop that moved the tick is the per-advance event budget (210 of them in a run, resumed without a drain) and one that moved nothing is backpressure (drain, resume, and the next advance must move or the loop fails with "stall without progress") |
| `hostloop` | a stalled host that drains nothing in frames 64 to 71 meets one backpressure stop at guest tick 540952 (the tick 11-18 recorded for the undrained run), drains and completes at tick 599357 with digest `97f270274d456e883ceaa11f100f92c7fbf3150e89031fa51164e79991f1267c`, the 3 video, 2 audio, transcript and save equal to the independent literals; the seeded host that never drains is stopped by the detector ("stall without progress" at tick 540952), so the detector can fail |
| `hostloop-cadence` | four runs, one digest and equal outputs: drain every slice (284 drains), every third slice (25), only on a stop (2, one backpressure stop), and not at all until the first backpressure stop and then every advance (21, one backpressure stop); every backpressure stop is at tick 540952 whatever the cadence |
| `hostloop-delay` | 284 host delays of 1 ms (one after every advance) give the same digest and the same outputs as the run without delay; this case is the longest of the four because of the sleeps (0.64 s), not the library |
| `two-instance` | two machines with disjoint counting allocators (no storage block held by both ledgers, zero outstanding after both destroys) advance alternately slice by slice: the first with 8192-tick frames and a drain every slice finishes at tick 599357; the second (4096-tick frames, drains only on a stop) is then at tick 303104, the first is destroyed and the second's digest and allocator ledger are unchanged; the second then meets its own backpressure stop at tick 540952 and completes at the same digest and literals |
| Negative controls (`controls.tsv`, `check-install.sh control ID`) | one CTest name per row, each in its own directory below the install-check scratch directory; every log states the expected and the observed text or create status; `missing-package` (empty `CMAKE_PREFIX_PATH`: `Could not find a package configuration file provided by "tendies64"`), `version-mismatch-minor` (request 0.2) and `version-mismatch-patch` (request 0.1.5) (`The version found is not compatible with the version requested`, observed with the CMake of the build tree, the package is 0.1.0 with SameMinorVersion), `visible-source-tree` (compile database and `build.ninja` of the consumer name nothing of the repository outside its own sources, build tree and the moved prefix) with `visible-source-tree-seed` (an extra `-I` of the source include directory is found by the same scan), `doctored-header-abi` (a copied prefix whose `T64_ABI_VERSION_MINOR` is one higher: create answers status 34 ABI_MISMATCH and machine NULL), `doctored-header-size` (a copied prefix whose create-info lacks its last member and its initializer: status 35 STRUCT_TOO_SMALL, machine NULL), `relocated-spaces` (the prefix copied and moved to a second path with spaces: the smoke case runs and neither the new prefix nor the new build tree names an earlier prefix) |
| RED (Task 2) | with `controls.tsv` and the CTest registration but no `control` subcommand, all eight rows failed with `check-install: unknown subcommand: control`, exit 2 (`install.setup` passed; saved in the ignored build tree as `11-19-task2/red.txt`); the first doctored-size build also failed loudly once (the whole consumer reads `info.profile`, so the control builds the new `t64c_smoke` target, the smoke case alone, against doctored and relocated prefixes) |
| Symbol collision | `collision.c` defines `f32_add` and `t64_sha256_init` with wrong results and call counters and is linked only into `t64c_collision` (`T64C_COLLISION=ON`); against the installed archive the link succeeds, the archive has `t64i_f32_add` and `t64i_sha256_init` and defines or references neither unprefixed name, and the composite case ends at `97f270274d456e883ceaa11f100f92c7fbf3150e89031fa51164e79991f1267c` with `f32_add calls: 0` and `t64_sha256_init calls: 0` (25 checks, 0 failures) |
| Unprefixed scratch run (one time, not a gate test) | a scratch clone of the head with `core/t64_prefix.h` reduced to its include guard and the four sha256 compile definitions removed (archive: `f32_add` and `t64_sha256_init` defined, 0 `t64i_` names) was installed into a scratch prefix and the same row run against it: it FAILED at link with `duplicate symbol '_t64_sha256_init'` (the archive member that defines the hash functions is pulled in and defines the name the host also defines). With only `f32_add` colliding the link succeeded silently and the composite FAILED by changed results: `the bound was reached before completion`, a different final digest, 16 failed checks and `f32_add calls: 5105` (the core does call `f32_add` during the composite run). Both failure modes are therefore shown; the scratch tree was removed (logs kept in the ignored build tree under `11-19-task3`) |
| Plain cc line | `cc -std=c99 -I <prefix>/include -DT64C_EXPECTED_BUILD_ID=<id> <six consumer sources> <prefix>/lib/libt64_core.a` (no CMake) compiles and links the consumer against the moved prefix and `--case smoke` runs (13 checks, 0 failures); the `.pc` file is dropped (DR-29). Compiling as C99 needed the consumer's counting allocator to align by arithmetic over `malloc` instead of `aligned_alloc` (a C11 function), the only change to shared consumer code |
| New names (native seconds) | `installed.consumer.hostloop` 0.19 to 0.22, `.hostloop-cadence` 0.42 to 0.50, `.hostloop-delay` 0.63 to 0.66, `.two-instance` 0.28 to 0.31; `installed.controls.missing-package` 0.41 to 0.44, `.version-mismatch-minor` 0.39 to 0.43, `.version-mismatch-patch` 0.41 to 0.49, `.visible-source-tree` 0.40 to 0.51, `.visible-source-tree-seed` 0.40 to 0.50, `.doctored-header-abi` 0.54 to 0.55, `.doctored-header-size` 0.60 to 0.92, `.relocated-spaces` 0.63 to 0.73, `.symbol-collision` 1.2 to 2.2, `.plain-cc` 0.26 to 0.54. Fourteen new names, about 7.5 s of native test time in total (about 32 s at the hosted multiplier 4.3 if run serially; the controls share the t64_install fixture and run in parallel). Half a second of each configure-only control is CMake's own compiler detection of the consumer project |
| ctest counts | `ctest -N -R '^installed\.consumer\.'` is 27 (4 host-loop names added to 23, `install.setup` included) and `^installed\.controls\.` is 9 after Task 2 and 11 after Task 3: the plan's 26, 8 and 10 do not count the `t64_install` fixture's `install.setup`; the preflight rows are 27 and 11 |

### 11-20 Call-sequence generator, sanitizer lanes and the two-thread TSan run

Evidence tier unchanged: unit-tested, authored scope; a software-interface claim only. Sanitizer and TSan runs
are local lanes: no sanitizer or TSan run is in the hosted gate or `ci.yml`.

**Task 1 (tracer): the refusal-is-a-no-op generator through the installed package (`sequence.c`).**

| Item | Result |
|------|--------|
| Assumption A5 (black-box defect trigger) | HELD. The consumer computes the profile digest with the public `t64_profile_hash`, scans the live allocations of its own counting allocator (the allocator now records each slot's size) and finds the 32 bytes exactly once (slot 0, the machine storage, offset 2400, of three create-time slots); flipping one bit and calling `t64_machine_admit_rom` answers `T64_STATUS_INTERNAL_DEFECT` (38), `t64_machine_is_poisoned` then reports 1 and every later call but destroy and the poison query answers `T64_STATUS_POISONED` (37). No test-only symbol and no core-internal header is used, so the `t64i_test_force_defect` fallback was not needed and no deviation is recorded for it |
| Grammar | a constant-seed 64-bit LCG owned by the consumer (seed `0x7e4d1e5c0ffee064`); 16 operations with 143 argument variants (admit, advance by tick, instruction count and frame-event count, queue_input, video and audio copy, output_drain, 24 observer variants, save export and import, state export and import, reset, unload, bus read and write, trace, the poison query, create with descriptor mutations (S-07) and a full drain); variants that may change the instance are kept one time in six so that machine digests stay rare. Two milestones (steps/4 and steps/4 + steps/2) replace the image, queue the two literal inputs and run the guest to completion with no drain until the output ring stalls it, drain, resume and compare the final digest with the runner-recorded pin; one deliberate defect at step steps - steps/8 |
| What is observed before every classified call | content digest, output status, the oldest video and audio packet info, save info, CPU stop detail, ROM size and header, the poison query (must be 0 before the defect step, so a caller fault that poisons is caught), and the allocator's live count and bad releases, each out-struct poisoned with a sentinel first so a refused or a copied-out record is compared whole; compared bit-for-bit after every call whose status the consumer's own table (`t64c_classes.h`) calls refused, and after every observation call. `t64_machine_digest` (the full-state hash) costs about 22 ms (4 MiB of RDRAM), so it is compared over a refusal burst: taken before the first refused call after a state change and again before the next call that may change state; a failure names the step and the calls of the burst. `--digest-stride N` verifies every Nth burst only (default 2 in the gate, 1 verifies all) |
| Guest-stop rules | every advance stop reason is a guest-stop row of the class table; guest time never decreases and BOUND is exactly at the bound; a larger tick bound makes progress, ends in a terminal stop, or stalls (RESOURCE_LIMIT that moved nothing) and then the output status must show a full ring; after a full drain the same advance must move or be terminal; a terminal stop stays the same stop at the same tick until a reset |
| Bus write carve-out | `t64_machine_bus_write` calls in the PI or SI register windows are not asserted to leave the machine digest alone when the device refuses (the write may move its own latch); every other observation stays bit-equal and every other refusal (unmapped, misaligned, width, the read-only cartridge window) is checked with the digest |
| Default run (400 steps, stride 2) | 1140 classified calls: 60 ok, 278 refused, 624 guest-stop results (606 RESOURCE_LIMIT, 8 BOUND, 4 INSTRUCTION_COUNT, 2 FRAME_EVENT_COUNT, 4 GUEST_COMPLETION), 1 defect, then 177 calls on the poisoned instance (146 POISONED); 27 refusal bursts, 13 machine digests verified (114 refusals were checked on the cheap snapshot only); two milestones both ended at tick 599357 with digest `97f270274d456e883ceaa11f100f92c7fbf3150e89031fa51164e79991f1267c` after one backpressure stall each (the milestone first stops a fresh machine at an instruction count, resumes it, and stops at a frame-event count, so every guest-stop reason a composite run can reach but QUIESCENT is seen); 5743 checks, 0 failures; the output is identical on repeated runs (with stride 1: 27 bursts verified, 5757 checks; no PI/SI device latch is drawn at 400 steps). Statuses seen: 0, 1, 3, 5, 7, 8, 9, 15, 17, 18, 20 to 24, 26, 27, 34, 35, 37, 38. The 20,000-step runs of Task 2 add QUEUE_FULL, BUS_DEVICE_REFUSED (22 PI/SI latches under the carve-out), TIME_REVERSED and QUIESCENT stops |
| The generator can fail (`--seed-fault N`, one time, stride 1) | fault 1 (a refused call that moved only the machine digest: a write into RDRAM) FAILED with `a refused call changed the machine digest: refusal burst from step 0, calls: ...`; fault 2 (a refused call that changed save info) FAILED with `a refused call changed the observation "save info"`; fault 3 (a caller-side poison before the defect step) FAILED with `the instance was poisoned before the deliberate defect step`; fault 0 passed. At the default stride 2 fault 1 was not seen because its burst was not one of the verified ones, which is what the stride means |
| `check-abi.py classes --consumer-table` | compares `t64c_classes.h` (hand-written, 47 rows) with `docs/abi/status-classes.txt` for name, class and resume (a missing, orphan, duplicate or changed row is a finding); registered on `repo.abi-status-classes`; six seeds in the new selftest case `consumer-table-controls` (`repo.abi-selftest` now has 15 cases) |
| Outside the plan's file list | `tests/installed-consumer/t64c_alloc.h` (slot size), `t64c_cases.h` (args and prototypes). The advance aliasing probe (S-06) is a finding, see the carried items of this plan |
| New name (native seconds) | `installed.consumer.sequence` 1.0 to 1.1 s at host load 6 to 8 (`COST 1.10`); `ctest -N -R '^installed\.consumer\.'` is 28 (27 plus sequence, `install.setup` and `installed.consumer.build` included; the plan's 27 is stale) |

**Finding F-20-A (S-06 aliasing probe, not fixed here): `t64_machine_advance` has no span check.** The header
(check order of every entry that takes a struct) says a struct span that overlaps the machine is
`T64_STATUS_INVALID_ARGUMENT` before the struct is read, and names `t64_machine_advance` among the entries that
check its request and its result that way. A throwaway probe (one instance, an out pointer placed 4096 bytes into
the machine's own storage, which the consumer owns through its allocator; not committed) found that every
out-pointer entry refuses it with `INVALID_ARGUMENT` (output status, video and audio info, save info, CPU stop
detail, machine and content digest, ROM size and header, video copy, save export, the import report, the drain
result and the poison query) except `t64_machine_advance`, which answers `OK` for a request or a result inside the
instance and writes the result into the instance (the probe's advance then stopped at a different reason and tick:
instance memory was changed by the call), and `t64_machine_bus_read`, whose `value_out` is also accepted (the header
makes no span promise for the bus calls). A caller that aliases its own result into the instance's allocation is a
caller fault, but the header promises a refusal and every other entry gives it. Disposition: a defect for a new
numbered plan (a span check for the advance request and result in `core/t64_machine.c`, a decision about the bus
read, one aliasing row each in the generator and `ops-advance`); it is not added to the gate generator here because it
would fail, and 11-20's files do not include `core/`.


**Task 2: ASan and UBSan against the shipped library and against an instrumented library (local lane, never in the gate).**

`sh scripts/check-install.sh sanitize shipped|instrumented [--steps N] [--digest-stride N]` builds the consumer with
`-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer` against a fresh install in a prefix
whose path has a space (below the ignored `build/gate/sanitize-lane/`, not under the install-check directory that the
CTest fixture clears) and runs every case of `cases.txt` (24: smoke, composite, 17 ops cases, the three host-loop
cases, two-instance and sequence) with the sequence at 20,000 steps. The lane proves which side is instrumented by the
undefined AddressSanitizer symbols of the archive and of the executable instead of trusting its flags, and fails on a
non-zero exit, a failing case, or any `ERROR: AddressSanitizer`, `runtime error:` or `ERROR: LeakSanitizer` line.

| Item | Result |
|------|--------|
| `shipped` | the Release archive of `build/gate` as shipped (build id `t64-build-v1-02b2ce1080322de84f2f6d1676432debc16dadf26ff9f1eef05fcbdd01cc7029`: 0 AddressSanitizer symbols in the archive, 34 in the consumer), an instrumented consumer; all 24 cases passed, no sanitizer report; the sequence at `--steps 20000 --digest-stride 1` ran 21,481 classified calls (2,469 ok, 14,480 refused, 1,904 guest-stop, 1 defect, then 2,627 on the poisoned instance), 860 refusal bursts, 867 machine digests verified, 22 PI/SI device latches seen, 99,281 checks, 0 failures; wall 54 s and 59 s in all over two runs (the sequence 45 s and 48 s) at host load 5 to 19 |
| `instrumented` | `cmake --preset asan` (Debug, ASan and UBSan, build id `t64-build-v1-2d35333166a66295a0150080f962ada519a6ae249a61554eec5bedee1412e090`: 733 AddressSanitizer symbols in the archive, 36 in the consumer), `cmake --build --preset asan --target t64_core`, the Development component installed, the same consumer; all 24 cases passed, no sanitizer report; the sequence at `--steps 20000 --digest-stride 8` ran the same 21,481 calls, 860 bursts of which 108 were verified against the machine digest (9,482 refusals were checked on the cheap snapshot only), 98,522 checks, 0 failures; wall 288 s and 299 s in all over two runs (the sequence 177 s and 180 s: a machine digest costs about 0.35 s with the instrumented Debug library, so stride 8 keeps the lane near five minutes; stride 1 would be about 15 times that) |
| Both modes | the milestones at steps 5,000 and 15,000 both ended at tick 599357 with digest `97f270274d456e883ceaa11f100f92c7fbf3150e89031fa51164e79991f1267c`; the defect at step 17,500 found the profile digest once (slot 0, offset 2400) and poisoned the instance; the poison sweep made 128 calls that all answered POISONED or the poison query with their outputs untouched; destroy left nothing outstanding; guest-stop reasons seen: BOUND 186, INSTRUCTION_COUNT 4, FRAME_EVENT_COUNT 2, QUIESCENT 6, RESOURCE_LIMIT 1,700, GUEST_COMPLETION 6; statuses seen: 0, 1, 3, 5, 7, 8, 9, 11, 15, 17 to 24, 26, 27, 34, 35, 37, 38 |
| First attempt of the instrumented lane | the same 20,000-step run passed (289 s) but its exit status was 2: the script had been edited by this plan while it ran, and the shell read the changed tail. It was run again unedited (`exit=0`, 288 s). Lesson: never edit `check-install.sh` while a lane runs |
| No sanitizer report | no AddressSanitizer or UBSan report in either mode, so no defect to record for a new numbered plan from this task |

**Task 3: two threads, one instance each, under ThreadSanitizer (local lane, never in the gate).**

`sh scripts/check-install.sh tsan` configures the whole tree into a scratch directory below the ignored
`build/gate/sanitize-lane/tsan` with the gate's cache values (Release, `T64_WERROR=ON`) plus `-fsanitize=thread`,
builds only `t64_core`, installs its Development component into a prefix with a space, builds the consumer with
`-DT64C_THREADS=ON` and `-fsanitize=thread`, and runs the `threads` case. `threads.c` is an empty translation unit
unless `T64C_THREADS` is on (the consumer is host code and may link pthreads; the library imports none), so `threads`
is not in `cases.txt`, no CTest name registers it and the hosted gate cannot run it; `check-install.sh build` against
a case list that names it fails (`the consumer case list is not closed`, shown once with a scratch edit of
`cases.txt`, restored). `ctest -N -R '^installed\.consumer\.threads'` reports 0 tests.

| Item | Result |
|------|--------|
| The case | two threads rendezvous (a mutex and a condition variable, no `pthread_barrier`, which macOS lacks) and each runs `t64c_hostloop_once`, the stalled-host loop of the `hostloop` case, on its own instance and its own allocator ledger (the function is silent and has no shared mutable state; the only globals of the consumer sources are the two call counters of `collision.c`, which `t64c_collision` alone links). Each run meets one backpressure stop, resumes, completes at tick 599357, equals the independent literals and the runner-recorded pins, and leaves its ledger balanced; the two digests are equal. 18 checks, 0 failures |
| TSan run | library and consumer both instrumented (ThreadSanitizer symbols: 564 in the archive, 21 in the consumer; build id `t64-build-v1-c26a7ea4448cda047355370b9af9874baed55506ef865efad1ea0a9e72a5cfbf`): no ThreadSanitizer report; both digests `97f270274d456e883ceaa11f100f92c7fbf3150e89031fa51164e79991f1267c`; library build 2 s, consumer build 1 s, run 2 s, 5.4 s in all. RESEARCH A9 (the lint forbids writable globals, so the result is expected clean) held |
| The lane can fail (`tsan --seed-race`) | `-DT64C_THREADS_SEED_RACE=ON` makes both threads advance the one same instance with nothing serializing them, the use the header forbids; ThreadSanitizer reported it (9 reports, a read and a write of the same 8 bytes in `t64i_scheduler_advance`, reached from `t64_machine_advance` on both threads) and the process then aborted (exit 134 from the corrupted instance); the lane passes only when a report appears |
| Statement supported | distinct instances are independent (no data race between two threads running two instances) and one instance needs external serialization (the seeded race). It is a local check of this build on this host: it is not a promise about other compilers or targets |

**Task 4 (review item S-09, ordered by the orchestrator, outside the plan's file list): one shared test header.**

`core/tests/api_test_support.h` (new, public header and system headers only) holds what `test_api_poison.c`,
`test_api_lifecycle.c` and `test_api_struct_size.c` each carried: the counting allocator (live blocks and sizes, an
optional hook that runs first in every callback), the fixture readers (`read_file`, `read_fixture` of
`T64_BOOT_FIXTURES_DIR`) and the eight poison helpers (one per in/out record) with their `POISON_*` constants.
Everything is `static inline`, so a test that uses some of it compiles clean.

| Item | Result |
|------|--------|
| Copies removed | the counting allocator and its release (three copies, two with live tracking and one counters-only), the fixture reader (two copies, plus the IPL3 asset reader of `test_api_poison.c`, now a three-line wrapper of `read_file` with an exact-length check) and the eight `poison_*` helpers (two copies); the reentrancy probe of `test_api_poison.c` is its own struct and rides on the allocator's hook (`counts.hook`, `counts.hook_context`), so its 25 probe accesses read `probe.` instead of `counts.` and nothing else of that test changed. 496 lines out, 48 in, the shared header is new |
| Same checks | `ctest -R '^unit\.core\.api\.'` (13 names) before and after: every name has the same `checks=` count (poison 113, refusals-never-poison 341, reentrant 39, null-machine 55, defect-sites 556, bus-carve-out 426, reset 384, unload 257, not-ready-table 2324, output-status 1390, handshake 379, struct-size 1591, abi-version 7) and 0 failures |
| Still separate | `poison_import_report` in `core/tests/state_import_helpers.h`, `poison_save_info` in the save tests and `poison_drain_result` in `core/tests/test_isviewer.c` stay as they are: those files are shared with oracle-bound or runner tests outside this fold; a later plan may point them at the shared header. The guard now has one place to extend when a public struct gains a member |
