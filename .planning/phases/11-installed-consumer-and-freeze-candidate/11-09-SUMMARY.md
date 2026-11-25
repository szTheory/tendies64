---
phase: 11-installed-consumer-and-freeze-candidate
plan: 09
subsystem: api
tags: [init-macros, codemod, core-tests, group-d, shape-wave]

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: plan 11-05 (the ten public init macros of DR-09 expanding to today's layouts, scripts/codemod-phase11.py with its bound-path refusal); plans 11-06 to 11-08 (groups A to C and the poison-helper pattern); plans 11-02 to 11-04 (the three Phase 11 API tests written with today's forms)
provides:
  - the 14 files of core/tests group D (scheduler, profile, lifecycle, composite and the Phase 11 API tests) on the init macros, with unchanged results (13 files written, test_api_surface.c has 0 sites)
  - file-local sentinel helpers in the two public-surface API tests (poison_* for eight record types plus scratch_poison and observations_poison) that keep a sentinel's detection power and a valid leading struct_size after the flip
  - core/tests has no convertible site left (7 remaining, all initialized arrays the tool lists by design) and per-file converted counts for the PIN-IMPACT consolidation of 11-10
affects: [11-10, 11-12, 11-13, 11-14, 11-15]

actuals:
  tokens: 16900
  tasks: 2
  commits: 1
plan_head_before: 359f687ca6a28a6303d6ce0dcae362ae04d290e2
plan_head_after: fbeb9ab585608d88da741ff7946d387cf7262d2e

tech-stack:
  added: []
  patterns:
    - "a byte-wise 0xA5 fill of a public in/out record becomes a file-local poison helper that assigns the INIT form and then sets every other member to the 0xa5.. value by name; the values equal the old byte fill, so every sentinel check keeps its meaning and only the leading struct_size of the flip survives"
    - "a struct that holds public records as members (the NOT_READY scratch record, the observations record) is filled by a helper that byte-fills it and then re-poisons each member from its INIT form; the reference side of a byte-for-byte compare is a memcpy of the poisoned side (or a one-time prototype copied by memcpy), never a second poisoning, so padding inside the members cannot differ"
    - "a zeroing memset of a macro-type object becomes an assignment from the INIT form (compound literal of the macro); an allocator held as a struct member is filled by one T64_HOST_ALLOCATOR_INIT assignment"

key-files:
  created: []
  modified:
    - core/tests/test_si_profile.c
    - core/tests/test_sched_ledger.c
    - core/tests/test_sched_matrix.c
    - core/tests/test_scheduler.c
    - core/tests/test_profile.c
    - core/tests/test_profile_v26.c
    - core/tests/test_lifecycle.c
    - core/tests/test_k1_refusal.c
    - core/tests/test_rdp_depth_composite.c
    - core/tests/test_vg_gaps.c
    - core/tests/test_warm_devices.c
    - core/tests/test_api_poison.c
    - core/tests/test_api_lifecycle.c

key-decisions:
  - "The two API tests that call only the public surface keep their own copies of the sentinel helpers (test_api_lifecycle.c includes just the public header and t64_test.h; test_api_poison.c is white-box); the plan forbids writing a shared header, so the duplication is recorded for the 11-15 SHAPE review instead of being factored out here"
  - "Sentinel fills of public records are poisoned, not reduced to a plain INIT assignment: the not-ready, poisoned and NULL-machine tables prove that a refused call leaves every output byte untouched, and an INIT-zero object would let a callee that writes zeros pass; the post-flip struct_size stays valid so the refusal is reached through the state checks the tables intend"
  - "In the NOT_READY and poison tables the expected side is a memcpy of the poisoned actual side; the observations record of test_api_poison.c is copied from one prototype built once, so the before/after compare of a refused write cannot see padding differences"
  - "The three t64_video_packet_info arrays of test_vg_gaps.c are initialized with one T64_VIDEO_PACKET_INFO_INIT element per entry and their redundant zeroing memsets are removed; the tool still lists them as hand sites (see Deviations)"

patterns-established:
  - "A struct member of a public in/out type inside a fixture (instance allocator, ai_run info, machine_fixture allocator) is filled from the INIT form right after the zeroing memset of the fixture; nothing writes the member by name afterwards"

requirements-completed: []

coverage:
  - id: D1
    description: "The 14 files of core/tests group D use the DR-09 init macros: 123 positional brace initializers, compound literals and uninitialized declarations are converted by the tool; 35 hand sites (8 zeroing memsets and 3 arrays of test_vg_gaps.c and the zeroing ones of the API tests done from the INIT form, 7 sentinel fills of the lifecycle test and 3 of the poison test poisoned, 11 sites the tool does not report: struct-member fills and a pointer-form fill) are done from the INIT form; test_api_surface.c has 0 sites; no oracle-bound file, expected value, check, case name or control flow changed"
    requirement: "API-01"
    verification:
      - kind: unit
        ref: "python3 -B scripts/codemod-phase11.py --check over the 14 files: RED exit 1 (147 remaining: 123 convertible, 24 hand) at plan_head_before; after conversion exit 1 only by tool design (remaining 3, convertible 0, the three initialized test_vg_gaps.c arrays); python3 -B scripts/codemod-phase11.py --dir core/tests/test_rdp.c exits 2 (refused: oracle-bound)"
        status: pass
      - kind: integration
        ref: "cmake --build --preset gate (no compiler warning); group CTest regex 41 of 41 in 15.3 s; ctest -L unit 128 of 128 in 135.8 s; sampling set 5 of 5 in 40.1 s; ctest --preset gate 268 of 268 in 110.2 s"
        status: pass
    human_judgment: false

duration: 19min
completed: 2026-11-25
status: complete
---

# Phase 11 Plan 09: Core Tests Codemod, Group D (Scheduler, Profile, Lifecycle, Composite and the Phase 11 API Tests) Summary

**123 initializer sites in the core/tests group D files moved to the DR-09 init macros by `scripts/codemod-phase11.py` in one compiling commit, plus 35 hand sites from the INIT form (zeroing memsets, poisoned sentinel fills, three arrays, struct-member fills); core/tests has no convertible site left and the full gate (268 tests) passes unchanged.**

## Performance

- **Duration:** 19 min, of which about 7 min were spent waiting before the full gate for unrelated host load (a virtual machine and Docker, 11 to 13 on the 1-minute average) to fall below 9; the gate then started at 8.76 and nothing was widened
- **Started:** 2026-11-25T22:31Z
- **Completed:** 2026-11-25T22:52Z (the summary and the state update follow the commit)
- **Tasks:** 2 of 2 (Task 1 is the tracer and did not commit by plan; Task 2 committed once)
- **Files modified:** 13 of the 14 listed (0 created; `test_api_surface.c` has no site and was not written); 506 insertions and 192 deletions

## Accomplishments

- **Tracer (Task 1).** `--check` over exactly the 14 files reported 147 remaining sites (123 convertible, 24 hand: 21 `memset` of a macro-type object and 3 arrays) and 25 struct-member notes. `--dir` converted all 123 (brace 84, uninitialized 36, compound literal 3; per-file counts below). The 24 reported hand sites and 11 more the tool does not report were then done from the INIT form (next section). The gate build is warning-free (only the pre-existing linker duplicate-library notice) and the 41 CTests built from these files pass; the unit label (128) passes too, so the tracer gate released the expansion task.
- **Commit (Task 2).** One commit, `fbeb9ab`, lists exactly 13 of the 14 files; no bound file, no deletion. The full `ctest --preset gate` (268 tests) and the sampling set (5) pass before it.

## Task Commits

1. **Task 1: Tracer, codemod over group D, hand conversions, build and run** - no commit (plan: Task 1 does not commit)
2. **Task 2: Full gate, sampling-set guards and the group D commit** - `fbeb9ab` (refactor)

**Plan metadata:** the summary and state commits that follow this file. The measured `commits:` count of 1 is `git rev-list --count 359f687..fbeb9ab` at write time.

## Per-file converted counts (consolidated into PIN-IMPACT by plan 11-10)

| File | Converted by tool | Brace | Uninit | Compound | Hand reported by tool | Hand found by grep (not reported) |
|------|-------------------|-------|--------|----------|-----------------------|-----------------------------------|
| core/tests/test_si_profile.c | 13 | 11 | 1 | 1 | 0 | 1 (`ai_run.info` member) |
| core/tests/test_sched_ledger.c | 10 | 8 | 1 | 1 | 0 | 0 |
| core/tests/test_sched_matrix.c | 12 | 9 | 3 | 0 | 0 | 0 |
| core/tests/test_scheduler.c | 18 | 18 | 0 | 0 | 0 | 1 (`machine_fixture.allocator` member) |
| core/tests/test_profile.c | 2 | 2 | 0 | 0 | 0 | 0 |
| core/tests/test_profile_v26.c | 7 | 7 | 0 | 0 | 0 | 0 |
| core/tests/test_lifecycle.c | 13 | 13 | 0 | 0 | 0 | 0 |
| core/tests/test_k1_refusal.c | 1 | 1 | 0 | 0 | 0 | 0 |
| core/tests/test_rdp_depth_composite.c | 1 | 1 | 0 | 0 | 0 | 0 |
| core/tests/test_vg_gaps.c | 5 | 4 | 0 | 1 | 6 (3 arrays, 3 memsets) | 0 |
| core/tests/test_warm_devices.c | 10 | 10 | 0 | 0 | 0 | 0 |
| core/tests/test_api_surface.c | 0 | 0 | 0 | 0 | 0 | 0 |
| core/tests/test_api_poison.c | 13 | 0 | 13 | 0 | 3 (sentinel fills of `t64_stop_result`) | 5 (3 scratch fill sites, 1 pointer-form fill of `observations`, 1 `instance.allocator` member) |
| core/tests/test_api_lifecycle.c | 18 | 0 | 18 | 0 | 15 (8 zeroing, 7 sentinel) | 4 (1 scratch fill site, 1 `fresh_scratch`, 1 `larger` fill, 1 `instance.allocator` member) |
| **Total** | **123** | 84 | 36 | 3 | **24** | **11** |

Brace plus uninitialized plus compound equals 123 (84 + 36 + 3). The tool's own per-file lines are the source (`test_api_surface.c` prints "converted 0"). 35 hand sites in all (24 reported, 11 found by grep). `union {` grep over the 14 files: `test_lifecycle.c` (1) and `test_profile.c` (3) hold only `t64_profile_descriptor`, `size_t` and byte arrays, never a macro-type member; no change was needed. `test_ai.c` is not in this plan's list and was not written; its four audio-info arrays already carry the INIT form (plan 11-07) and are among the 7 sites the tool still lists.

State at the end of this plan for the later plans (`--check`, measured): core/tests remaining 7, hand 7, all initialized arrays (4 in `test_ai.c`, 3 in `test_vg_gaps.c`), 9 bound files excluded, nothing convertible; runner/tests remaining 30, hand 2 (the directory-wide check over `runner tools tests` reports the same 30 and 2, with 2 bound files excluded; `tools` and `tests` have none). The 7 are the 151 of the end of 11-08 less the 123 converted here and the 21 memset sites replaced by hand; the 3 initialized `test_vg_gaps.c` arrays join the 4 `test_ai.c` arrays as listings.

## Hand conversions in detail

- **Zeroing memsets of a public record (8, all `test_api_lifecycle.c`).** Four `t64_output_drain_result` (lines 224, 235, 263 and 975 of the original), two `t64_video_packet_info` (`first_info`, `replay_info`, 352 and 373) and one each of `t64_video_packet_info video` and `t64_audio_packet_info audio` in `check_status_against_rings` (841, 842) become `x = (T)T64_..._INIT;`. The value is the same zero object, the leading struct_size of the flip is kept.
- **Sentinel fills of a public record (7 in `test_api_lifecycle.c`, 3 in `test_api_poison.c`).** `detail` (246), two drain results (249, 274), `untouched` video info (359, copied into `replay_info` and compared byte for byte after the NOT_READY call), `audio` (368), and the two `t64_output_status` fills of `status_refused` (1008, 1015, with `struct_size` assigned right after as before) call a file-local `poison_*` helper; the poison test's `defect_cp0_phase` (a `result` plus a byte-equal `expected`, now a poison and a memcpy) and `guest_driven_case` call `poison_stop_result`. Each helper assigns the INIT form and then sets every other member by name to exactly the value the byte fill produced (uint32 0xa5a5a5a5, uint64 and size_t 0xa5.. repeated, char and hash arrays by memset of the member, bools to true), so each following check keeps its meaning.
- **Records inside a scratch struct (the two API tests).** `scratch` (the NOT_READY table of `test_api_lifecycle.c`, the 26-entry table of `test_api_poison.c` at three sites: the reentrancy hook, the poisoned instance and the NULL machine) holds eight public records. `scratch_poison` byte-fills the whole record and then re-poisons each member from its INIT form; the `expected` copy is a `memcpy` of the poisoned `actual`, so the byte-for-byte "outputs untouched" compare cannot see padding differences. The fresh-instance scratch of `table_for_profile` and the `larger` caller struct of `status_refused` (known part poisoned, the 24-byte tail still 0xa5 and still checked) follow the same pattern. `observations` of `test_api_poison.c` (a pointer-form `memset(o, 0xa5, sizeof *o)` the tool does not see) is filled from a prototype built once and copied by `memcpy`, so the before and after observations of a refused bus write compare equal or not only by what the calls wrote.
- **Allocator and info members (3).** `instance.allocator` in `make_instance` of both API tests (3 member assignments each, now one `T64_HOST_ALLOCATOR_INIT` assignment), `machine_fixture.allocator` in `fixture_init_with_profile` of `test_scheduler.c` (same), and `ai_run.info` in `run_ai_dma` of `test_si_profile.c` (a `memset(run, 0, ...)` followed by one `T64_AUDIO_PACKET_INFO_INIT` assignment; `run_ai_dma` then fills it through `t64_audio_info` as before).
- **Arrays (3, `test_vg_gaps.c`).** `v8_infos[2]`, `v9_infos[2]` and `infos[2]` start from one `T64_VIDEO_PACKET_INFO_INIT` element per entry; the three zeroing memsets of those arrays are removed (the packet byte arrays keep theirs). The comparisons are by field, so nothing else changed.
- **Non-zero struct values the tool handled.** For `t64_stop_result stop = {UINT32_MAX, UINT64_MAX}` the tool wrote `T64_STOP_RESULT_INIT_WITH(UINT32_MAX, UINT64_MAX)`; for `t64_cpu_stop_detail stop_detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX}` in `test_warm_devices.c` it wrote the INIT form and three member assignments keeping the values (the pattern of 11-08).

## New non-bound callers (grep first)

The 14 listed files were checked against the codemod's own report over all of core/tests before and after: every file that still used a macro type at the start of this plan is in the plan's list, and the three Phase 11 API tests (`test_api_surface.c`, `test_api_poison.c`, `test_api_lifecycle.c`) are in it; no additional non-bound caller was found. `test_api_surface.c` has no initializer of a macro type (it exercises the ABI version query) and needed no change; the plan's list stands.

## Decisions Made

Binding decision is DR-09. The choices inside the plan's discretion are in the frontmatter (file-local sentinel helpers duplicated across the two API tests, poison over plain assignment, memcpy and prototype for the byte-equal references, one INIT element per array entry).

## Deviations from Plan

None - plan executed exactly as written.

Two points where the measured result differs from the plan's text, neither a rule deviation:

- **`--check` exits 1 after conversion, by tool design.** The plan's Task 1 verify runs `python3 -B scripts/codemod-phase11.py --check` over the 14 files and expects exit 0. The tool lists every array declaration of a macro type as a hand site whether or not it is initialized (recorded since 11-07), so the three converted `test_vg_gaps.c` arrays still print as `hand`: the post-conversion output is `remaining 3 (convertible 0, hand 3)`, exit 1. Nothing convertible remains, `grep -n T64_VIDEO_PACKET_INFO_INIT core/tests/test_vg_gaps.c` shows each of the three declarations carrying the INIT form, and the plan forbids writing outside the 14 files, so `scripts/codemod-phase11.py` was not changed.
- **13 of the 14 files are in the commit.** `test_api_surface.c` has 0 convertible sites ("A file with 0 convertible sites is recorded as 0"), so the acceptance criterion "lists only the 14 files of this plan" holds with a count of 13.

**Total deviations:** 0 auto-fixed. **Impact:** none on production code; the test diff is initializer forms, sentinel helpers and one prototype copy, with no changed expected value, case name or control flow.

## Authentication Gates

None.

## Issues Encountered

- **Unrelated host load.** The load average sat at 10 to 13 from other processes (a virtual machine at 300 to 800 percent and Docker). The unit label ran at 9.4 (135.8 s) and the sampling set at 12.7 (40.1 s); the full gate waited about 7 min in a bounded loop until the 1-minute average fell below 9 and started at 8.76 (110.2 s). No limit was widened and no run was repeated for load.
- **zsh array expansion.** The 14 paths were first passed as one word to `grep`; re-run with a real array, no effect on the tree.

## Open items for later plans (not acted on here)

- 11-10's init-form guard must accept an array whose declaration initializes every entry from the INIT form (`test_ai.c`, `test_vg_gaps.c`), treat a `memset` of a macro-type object as a violation, and flag a pointer-form `memset(*out, ...)` of one; core/tests is clean of both after this plan (the grep found one pointer-form fill, `observe` in `test_api_poison.c`, now converted). The runner/tools pointer-form grep remains for 11-10.
- Candidate for the 11-15 SHAPE review: the duplicate sentinel helpers of `test_api_lifecycle.c` and `test_api_poison.c` (about 130 lines each) could move to one shared test header; the two files could not share one here because the plan writes only the 14 listed files and `test_api_lifecycle.c` must keep seeing only the public header.
- Carried from 11-03/11-08 and unchanged: the other items listed there for 11-15 (state-export defect branches, create-time refusal of a descriptor with a zero event queue capacity, white-box limiting state of the guest-driven poison case, an uninitialized-report read after an untouched call).

## Verification Results

| Check | Result |
|-------|--------|
| `python3 -B scripts/codemod-phase11.py --check` over the 14 files | RED: exit 1, 147 remaining (123 convertible, 24 hand) at the plan head; after conversion: exit 1 by tool design, remaining 3, convertible 0 (the three initialized `test_vg_gaps.c` arrays) |
| `python3 -B scripts/codemod-phase11.py --dir core/tests/test_rdp.c` | exit 2, "refused core/tests/test_rdp.c: oracle-bound" |
| `cmake --build --preset gate` | clean (only the pre-existing linker duplicate-library notice) |
| group CTest regex (41 tests: warm-device-policy, profile, ai.profile-pins and profile-mutations, profile.predicate-matrix, v12-identity, quantum-identity, family-devices, k1.dispatch-refusal and apply-refusal, api (13), profile.v26 (3), rdp.depth-composite (3), vg (9), sched.ledger, characterize-fifo and matrix, lifecycle, scheduler; written to the scratchpad, the dots escaped in the shell) | 41 of 41 passed, 15.3 s (`unit.core.sched.matrix` 10.3 s, 1926163 checks) |
| `ctest --test-dir build/gate -L unit` | 128 of 128 passed, 135.8 s (load about 9.4) |
| sampling set (`harness.rdp-oracle-authority`, `repo.si-membership-sites`, `harness.state-field-use-audit`, `repo.p10-mutant-anchors`, `repo.rsp-no-hle`) | 5 of 5 passed, 40.1 s |
| `ctest --preset gate` | 268 of 268 passed, 110.2 s (start load 8.76) |
| `sh scripts/pii-scan.sh --tree` | findings=0 |
| `git show --name-only --format= HEAD` | exactly 13 files of this plan; no bound file, no deletion |

## Known Stubs

None.

## Threat Flags

None. T-11-62 (test semantics of group D) is mitigated by one compiling commit, the group's binaries, the unit label, the full gate and the sampling set before the commit, initializer forms and sentinel helpers only, and the diff left for the 11-15 review.

## Next Phase Readiness

Plan 11-10 can convert the remaining runner/tests sites (30, hand 2), add the init-form guard (`scripts/check-api-init.py`) with the notes above, and consolidate the per-file counts of 11-05 to 11-09 into PIN-IMPACT. API-01 is not complete: the last codemod group, the flip, the `struct_size` prologues and the installed consumer are ahead. Nothing in this plan is orchestrator-only and no owner decision is pending.

## Self-Check: PASSED

- FOUND: all 13 modified files, each containing `_INIT` (checked by grep)
- FOUND commit: fbeb9ab (ancestor of HEAD)
