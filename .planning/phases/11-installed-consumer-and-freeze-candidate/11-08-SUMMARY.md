---
phase: 11-installed-consumer-and-freeze-candidate
plan: 08
subsystem: api
tags: [init-macros, codemod, core-tests, group-c, shape-wave]

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: plan 11-05 (the ten public init macros of DR-09 expanding to today's layouts, scripts/codemod-phase11.py with its bound-path refusal); plans 11-06 and 11-07 (groups A and B and the poison-helper pattern)
provides:
  - the 14 files of core/tests group C (state, save, digest and trace) on the init macros, with unchanged results
  - two reusable poison helpers (import report, save info) that keep a sentinel's detection power and a valid leading struct_size after the flip
  - per-file converted counts for the PIN-IMPACT consolidation of 11-10
affects: [11-09, 11-10, 11-12, 11-13, 11-14, 11-15]

actuals:
  tokens: 9500
  tasks: 2
  commits: 1
plan_head_before: db30f35b0c0d65f76807027f2e03e48ad8a40e07
plan_head_after: 2ff693119b7d84c82767cb23796882f014c6df99

tech-stack:
  added: []
  patterns:
    - "a byte-wise 0xA5 fill of an import report or a save info becomes a poison helper that assigns the INIT form and then sets every other member to 0xa5.. by name; the values equal the old byte fill, so every sentinel check keeps its meaning and only the leading struct_size of the flip survives"
    - "a zeroing memset of a macro-type object (report, drain result) becomes an assignment from the INIT form (compound literal of the macro)"
    - "a host allocator held as a struct member is filled by one T64_HOST_ALLOCATOR_INIT assignment (same pattern as 11-06 and 11-07)"

key-files:
  created: []
  modified:
    - core/tests/fuzz_state_import.c
    - core/tests/state_import_helpers.h
    - core/tests/test_state_export.c
    - core/tests/test_state_export_v26.c
    - core/tests/test_state_import.c
    - core/tests/test_state_import_v26.c
    - core/tests/test_save.c
    - core/tests/test_save_transfer.c
    - core/tests/test_digest_trace.c
    - core/tests/test_digest_v25.c
    - core/tests/test_digest_v26.c
    - core/tests/test_determinism.c
    - core/tests/test_tracer.c
    - core/tests/test_si.c

key-decisions:
  - "poison_import_report and import_report_is_poisoned live in the shared core/tests/state_import_helpers.h (static inline, as every function of that header), because four of the 14 files include it; poison_save_info is a file-local static in test_save.c and test_save_transfer.c, which share no header"
  - "The fifteen 0xA5 fills (5 import reports, 10 save infos) are poisoned, not reduced to a plain INIT assignment: a plain assignment would let a callee that writes zeros or leaves a member alone pass the checks of the fields it must overwrite, and would turn the unchanged-on-refusal memcmp comparisons of test_save.c into comparisons of two zero objects"
  - "The eight zeroing memsets (6 import reports in test_state_export_v26.c, 2 drain results in test_state_export.c) become INIT assignments: their original value was zero, so the assignment is the same bytes"

patterns-established:
  - "A test that reads an output of a call that leaves it untouched (here a NULL machine at the entry guard) passed before only because the uninitialized object held stack residue; the deterministic INIT form exposes it, so the row now poisons the object first and checks that it is untouched"

requirements-completed: []

coverage:
  - id: D1
    description: "The 14 files of core/tests group C use the DR-09 init macros: positional brace initializers and uninitialized declarations are converted, 8 zeroing memsets, 15 sentinel fills (5 import reports, 10 save infos), 1 pointer-form sentinel fill and 1 allocator member are done from the INIT form, and no oracle-bound file, expected value (one corrected row, see Deviations), case name or control flow changed"
    requirement: "API-01"
    verification:
      - kind: unit
        ref: "python3 -B scripts/codemod-phase11.py --check over the 14 files: RED exit 1 (169 remaining: 147 convertible, 22 hand) at plan_head_before; after conversion exit 0, remaining 0 (only the informational member note of test_determinism.c, now filled from the INIT form); python3 -B scripts/codemod-phase11.py --dir core/tests/test_rdp.c exits 2 (refused: oracle-bound)"
        status: pass
      - kind: integration
        ref: "cmake --build --preset gate (no compiler warning); group CTest regex 20 of 20 (after the corrected row); ctest -L unit 128 of 128 in 123.2 s; sampling set 5 of 5 in 39.4 s; ctest --preset gate 268 of 268 in 110.4 s"
        status: pass
    human_judgment: false

duration: 12min
completed: 2026-11-25
status: complete
---

# Phase 11 Plan 08: Core Tests Codemod, Group C (State, Save, Digest and Trace) Summary

**147 initializer sites in the 14 core/tests group C files moved to the DR-09 init macros by `scripts/codemod-phase11.py` in one compiling commit, plus 24 hand conversions from the INIT form (8 zeroing memsets, 15 poisoned sentinel fills, 1 allocator member) and one corrected test row; the full gate (268 tests) passes.**

## Performance

- **Duration:** 12 min (host load fell from 13.7 to 8 during the run, unrelated to this project; nothing waited, no limit changed)
- **Started:** 2026-11-25T22:16Z
- **Completed:** 2026-11-25T22:28Z (the summary and the state update follow the commit)
- **Tasks:** 2 of 2 (Task 1 is the tracer and did not commit by plan; Task 2 committed once)
- **Files modified:** 14 (0 created); 275 insertions and 198 deletions

## Accomplishments

- **Tracer (Task 1).** `--check` over exactly the 14 files reported 169 remaining sites (147 convertible, 22 hand, all `memset`) and one member note. `--dir` converted all 147 (brace 90, uninitialized 57, compound literal 0; per-file counts below). The 22 hand sites, one memset through a pointer the tool does not see and the allocator member were then done from the INIT form (next section). The gate build is warning-free (the linker's duplicate-library notice is pre-existing and unrelated) and the 20 CTests built from these files pass.
- **Commit (Task 2).** One commit, `2ff6931`, lists exactly the 14 files; no bound file, no deletion. The full `ctest --preset gate` (268 tests) and the sampling set (5) pass before it.

## Task Commits

1. **Task 1: Tracer, codemod over group C, hand conversions, build and run** - no commit (plan: Task 1 does not commit)
2. **Task 2: Full gate, sampling-set guards and the group C commit** - `2ff6931` (refactor)

**Plan metadata:** the summary and state commits that follow this file. The measured `commits:` count of 1 is `git rev-list --count db30f35..2ff6931` at write time.

## Per-file converted counts (consolidated into PIN-IMPACT by plan 11-10)

| File | Converted by tool | Brace | Uninit | Compound | Hand from the INIT form | Kind of hand site |
|------|-------------------|-------|--------|----------|-------------------------|-------------------|
| core/tests/fuzz_state_import.c | 3 | 1 | 2 | 0 | 1 | 0xA5 fill of a report (poisoned) |
| core/tests/state_import_helpers.h | 4 | 3 | 1 | 0 | 0 | adds `poison_import_report`, `import_report_is_poisoned` |
| core/tests/test_state_export.c | 6 | 4 | 2 | 0 | 2 | zeroing memset of a drain result |
| core/tests/test_state_export_v26.c | 1 | 0 | 1 | 0 | 6 | zeroing memset of a report |
| core/tests/test_state_import.c | 43 | 13 | 30 | 0 | 2 (+1 corrected row) | 0xA5 fill of a report (poisoned) |
| core/tests/test_state_import_v26.c | 12 | 0 | 12 | 0 | 2 | 1 reported 0xA5 fill; 1 pointer-form fill of `*report_out` the tool did not report |
| core/tests/test_save.c | 8 | 3 | 5 | 0 | 5 | 0xA5 fill of a save info (poisoned) |
| core/tests/test_save_transfer.c | 6 | 3 | 3 | 0 | 5 | 0xA5 fill of a save info (poisoned) |
| core/tests/test_digest_trace.c | 27 | 27 | 0 | 0 | 0 | |
| core/tests/test_digest_v25.c | 5 | 5 | 0 | 0 | 0 | |
| core/tests/test_digest_v26.c | 3 | 3 | 0 | 0 | 0 | |
| core/tests/test_determinism.c | 7 | 6 | 1 | 0 | 1 | allocator member of the fixture |
| core/tests/test_tracer.c | 19 | 19 | 0 | 0 | 0 | |
| core/tests/test_si.c | 3 | 3 | 0 | 0 | 0 | |
| **Total** | **147** | 90 | 57 | 0 | 24 (8 zeroing, 15 poisoned fills, 1 allocator) | |

Brace plus uninitialized plus compound equals 147 (90 + 57 + 0). The tool's own per-file lines are the source. Of the 22 hand sites the tool reported (all `memset`), 14 were 0xA5 sentinel fills and 8 were zeroing calls; the fifteenth poisoned fill is the pointer-form `memset(report_out, 0xA5, ...)` in `import_fresh` of `test_state_import_v26.c`, which the tool does not detect. `union {` grep over the 14 files: none.

State at the end of this plan for the later plans (`--check`, measured): core/tests remaining 151, hand 28 (4 initialized `test_ai.c` arrays that the tool still lists, 3 arrays and 3 memsets in `test_vg_gaps.c`, 15 memsets in `test_api_lifecycle.c`, 3 in `test_api_poison.c`), 9 bound files excluded; runner/tests remaining 30, hand 2, 1 bound file excluded. The 151 are the 320 of the end of 11-07 less the 147 converted here and the 22 memset sites replaced by hand.

## Hand conversions in detail

- **Import report fills (5 poisoned, 6 zeroing).** `fuzz_state_import.c:271`, `test_state_import.c` (two, lines 141 and 1200) and `test_state_import_v26.c` (the row runner and `import_fresh`) filled a `t64_state_import_report` with 0xA5 bytes so a refusal or acceptance that leaves a member alone shows. All five call `poison_import_report` in `state_import_helpers.h`: the INIT form first, then `status`, `reason`, `section_id` and `rule` set to `0xa5a5a5a5` and `position` to `0xa5a5a5a5a5a5a5a5`. Those are exactly the values the old byte fill produced, so each following check is unchanged. The six zeroing memsets of `test_state_export_v26.c` (lines 520, 545, 562, 582, 598, 608) are zero in the original and become `report = (t64_state_import_report)T64_STATE_IMPORT_REPORT_INIT;`.
- **Save info fills (10 poisoned).** Five in `test_save.c` (the kinds loop, the per-row loop, the refusal helper with a `sentinel = info` copy, the accepted success and the NULL machine row with a `sentinel = info` copy) and five in `test_save_transfer.c` (the first info query, the post-import generation row, two busy-flag rows and the EEPROM deadline row). Each calls the file-local `poison_save_info` (INIT form, then `kind`, `size_bytes` and `flags` to `0xa5a5a5a5`, `write_generation` to `0xa5a5a5a5a5a5a5a5`). The `memcmp(&info, &sentinel, ...)` rows still compare a non-zero object, so a refusal that writes zeros is still caught.
- **Drain result zeroing (2).** `test_state_export.c:1190` and `:1507` (an observer drain that advances the cursor before an export) become `drain = (t64_output_drain_result)T64_OUTPUT_DRAIN_RESULT_INIT;`.
- **Allocator member (1).** `fixture_init` of `test_determinism.c` set `allocate`, `release` and `context` member by member after `memset(value, 0, ...)` of the fixture; it is now one `T64_HOST_ALLOCATOR_INIT(test_allocate, test_release, &value->host)` assignment (same values, the flip changes only the macro body). `test_digest_trace.c` and `test_boot_determinism.c` assign `machine->allocator.context`, which is a member of the internal `t64_machine`, not of a public in/out struct; they stay.
- **Non-zero struct values the tool handled.** For `t64_cpu_stop_detail detail = {99, 99, 99}` in `test_tracer.c` the tool wrote the INIT form and three member assignments (the pattern of the zero-only macro), keeping the values.

## New non-bound callers (grep first)

The 14 listed files were checked against the codemod's own report over all of core/tests: every group C file that uses a macro type is in the plan's list; no additional group C file was found. The files with remaining sites after this plan (`test_ai.c`, `test_api_lifecycle.c`, `test_api_poison.c`, `test_k1_refusal.c`, `test_lifecycle.c`, `test_profile.c`, `test_profile_v26.c`, `test_rdp_depth_composite.c`, `test_sched_ledger.c`, `test_sched_matrix.c`, `test_scheduler.c`, `test_si_profile.c`, `test_vg_gaps.c`, `test_warm_devices.c`) are outside this plan and belong to 11-09's list.

## Decisions Made

Binding decision is DR-09. The choices inside the plan's discretion are in the frontmatter (shared and file-local poison helpers, poison over plain assignment for the 0xA5 fills, INIT assignment for the zeroing fills).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] A NULL machine row read a report no call had written**
- **Found during:** Task 1 (first group run, `unit.core.state.import`: `test_state_import.c:1293: CHECK_INT failed: report.reason got 0 want 1`)
- **Issue:** `case_container_refusals` called `t64_machine_state_import(NULL, ...)` and then checked `report.reason == T64_STATE_REASON_ARG_NULL`. The NULL machine answers INVALID_ARGUMENT at the entry guard (since 11-03) and, by `docs/abi/status-classes.txt` ("a NULL machine answers it and every output stays untouched"), never writes the report. The `report` declaration was uninitialized, so the old check read an indeterminate value and passed on stack residue; the deterministic INIT form made it read 0 and fail. This is a latent defect of the test, not a production change: the production behavior is the documented contract.
- **Fix:** the row now calls `poison_import_report(&report)` before the call, still expects INVALID_ARGUMENT, and checks `import_report_is_poisoned(&report)` (the output stays untouched) in place of the reason check. The other five argument rows of the same function are unchanged and write their reasons. This is the only changed check of the plan; the plan's "no expected value changes" intends mechanical initializer forms, and leaving the declaration uninitialized would have kept a read of indeterminate memory that the flip and the 11-10 init-form guard both forbid.
- **Files modified:** core/tests/test_state_import.c, core/tests/state_import_helpers.h (the `import_report_is_poisoned` helper)
- **Verification:** `unit.core.state.import` 1 of 1 (47.7 s) after the fix, then the unit label and the full gate pass
- **Commit:** 2ff6931

**Total deviations:** 1 auto-fixed (Rule 1). **Impact:** one row of one test in the group; for the 11-15 SHAPE review, the row now documents the NULL-machine contract instead of reading stack residue. No production file was touched.

## Authentication Gates

None.

## Issues Encountered

- **Unrelated host load.** The host load average fell from 13.7 to 8 while the plan ran (other projects' processes); no run was repeated for load and no limit was widened. `unit.core.state.import` took 47.7 s alone, the unit label 123.2 s and the full gate 110.4 s.
- **zsh array expansion and globs.** The tool was driven with a real array, and `--include=*.c` in a `grep -r` needed quotes; neither affected the tree.

## Open items for later plans (not acted on here)

- The 11-10 init-form guard must treat the four `test_ai.c` arrays and the `test_vg_gaps.c` arrays as fine when every entry uses the INIT form (the tool lists every array declaration of a macro type as hand), as noted by 11-07.
- The pointer-form fill `memset(report_out, ...)` is invisible to the codemod's memset detection (it sees `&x` and plain identifiers of declared objects). 11-09 and 11-10 should grep `memset(` with a pointer to a macro type (`*out`, `->result`, parameters) in `test_api_lifecycle.c`, `test_api_poison.c` and the runner/tools files, and the 11-10 guard should flag it too.
- Candidate for the 11-15 SHAPE review: other tests may read a field of an uninitialized report after a call that leaves it untouched. The full gate shows none besides the row fixed here, but the check is by run, not by static analysis.

## Verification Results

| Check | Result |
|-------|--------|
| `python3 -B scripts/codemod-phase11.py --check` over the 14 files | RED: exit 1, 169 remaining (147 convertible, 22 hand); after conversion: exit 0, remaining 0 (one informational member note) |
| `python3 -B scripts/codemod-phase11.py --dir core/tests/test_rdp.c` | exit 2, "refused core/tests/test_rdp.c: oracle-bound" |
| `cmake --build --preset gate` | clean (only the pre-existing linker duplicate-library notice) |
| group CTest regex `^(unit\.core\.(tracer\|digest\.(sink-legacy-identity\|v25-reference\|v25-strip-differential\|v26-completeness\|v26-strip-differential)\|state\.(export\|export-v26\|export-v26-bounds\|import\|import-v26-roundtrip\|import-v26-hostile)\|si\.(atomic-queue-full\|device)\|cpu\.si-store-budget\|input\.latch-partition\|save\.(device\|import-export)\|digest-trace\|determinism))$` (20 tests; in the shell the dots and dashes are escaped) | first run 19 of 20 (the NULL machine row, see Deviations); after the fix 20 of 20 (`unit.core.state.import` 47.7 s alone) |
| `ctest --test-dir build/gate -L unit` | 128 of 128 passed, 123.2 s (load about 8) |
| sampling set (`harness.rdp-oracle-authority`, `repo.si-membership-sites`, `harness.state-field-use-audit`, `repo.p10-mutant-anchors`, `repo.rsp-no-hle`) | 5 of 5 passed, 39.4 s |
| `ctest --preset gate` | 268 of 268 passed, 110.4 s (load 10 at start) |
| `git show --name-only --format= HEAD` | exactly the 14 files; no bound file, no deletion |

## Known Stubs

None.

## Threat Flags

None. T-11-61 (test semantics of group C) is mitigated by one compiling commit, the group's binaries, the unit label, the full gate and the sampling set before the commit, initializer forms only (one corrected row, recorded above), and the diff left for the 11-15 review.

## Next Phase Readiness

Plan 11-09 can apply the same tool to the remaining core/tests files (the 151 sites above, including `test_api_surface.c`, `test_api_poison.c` and `test_api_lifecycle.c`) with the poison, pointer-form and array notes. API-01 is not complete: the remaining codemod groups, the flip, the `struct_size` prologues and the installed consumer are ahead. Nothing in this plan is orchestrator-only and no owner decision is pending.

## Self-Check: PASSED

- FOUND: all 14 modified files, each containing `_INIT`
- FOUND commit: 2ff6931 (ancestor of HEAD)
