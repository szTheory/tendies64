---
phase: 11-installed-consumer-and-freeze-candidate
plan: 10
subsystem: api
tags: [init-macros, codemod, runner-tests, init-form-guard, shape-wave]

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: plans 11-05 to 11-09 (the DR-09 init macros, scripts/codemod-phase11.py and its conversions of runner and core/tests); 11-02 (tools/abi/CMakeLists.txt, PHASE11_TESTS and the regex table the new names extend)
provides:
  - the 7 runner/tests files with sites on the init macros (28 tool conversions, 2 hand removals); three listed files have no initializer site
  - scripts/check-api-init.py, the init-form guard (four rules, 17 seeded selftest cases) and CTests repo.api-init-guard (with --exclude-oracle-bound) and repo.api-init-guard-selftest
  - every non-bound consumer in core/tests, runner, runner/tests, tests and tools on the init form, proven by the guard (172 files, 0 findings) and by 120 findings in exactly the nine bound files
  - the consolidated per-file codemod table of 11-05 to 11-10 in PIN-IMPACT, reconciled with the 11-01 bound-file probe
affects: [11-12, 11-13, 11-14, 11-15, 11-16]

actuals:
  tokens: 16500
  tasks: 2
  commits: 3
plan_head_before: 0643ac7d7f867b0559d180e9c0252813500ce723
plan_head_after: 8dfaed573b1a21eea1ca4c3f56be66aa32f230a9

tech-stack:
  added: []
  patterns:
    - "the guard reads the public struct types from the header, derives each type's macro as T64_<NAME>_INIT (and _INIT_WITH), accepts an array only when every entry is that macro and the entry count covers the declared size, reports one finding per declaration, and exempts t64_host_storage (the core fills it) and t64_trace_record (array element)"
    - "a lone zero brace list of t64_profile_descriptor is the deliberate empty-descriptor fixture and passes; designated .member = value lists pass; any positional member is a finding"
    - "a memset or bzero is a finding when its target resolves (scoped locals, parameters and file-scope names, or the last member name of a member path) to a type that has an init macro, or when its size argument names one; pointer form included"

key-files:
  created:
    - scripts/check-api-init.py
  modified:
    - runner/tests/test_cpu_diagnostic.c
    - runner/tests/test_fixture_descriptor.c
    - runner/tests/test_input.c
    - runner/tests/test_pad_poll.c
    - runner/tests/test_real_runner.c
    - runner/tests/test_replay.c
    - runner/tests/test_state_runner.c
    - core/tests/test_state_import.c
    - tools/abi/CMakeLists.txt
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase11-regexes.txt
    - .planning/phases/11-installed-consumer-and-freeze-candidate/PIN-IMPACT.md

key-decisions:
  - "The guard has a fourth rule, API-INIT-MEMSET, beyond the three the plan names (uninitialized, positional or zero, compound): the running notes of 11-05, 11-07 and 11-08 require it because a zeroing memset after the flip would clear struct_size, and it found two real sites (see Deviations)"
  - "The default scan roots are core/tests, runner, tests and tools, not only the three the plan lists: the fixed exclusion of the digest-sweep printer is only meaningful if tools is scanned, and tools holds no other site"
  - "A lone zero brace list of t64_profile_descriptor passes the guard: it is the empty-descriptor fixture of core/tests/test_profile.c line 340, not a forgotten initializer; its expected status changes at the flip and the test fails loudly there, so the guard need not catch it (open item for 11-12/11-13)"
  - "scripts/codemod-phase11.py is not edited: its --check still lists the 7 INIT-initialized arrays as hand sites by design (recorded since 11-07), so its exit status stays 1; the guard accepts such arrays and is the arbiter"

patterns-established:
  - "An init-form guard finding in a non-bound file is fixed from the INIT form in a separate preparatory commit, so the guard commit is the one that turns the check on"

requirements-completed: []

coverage:
  - id: D1
    description: "The 7 runner/tests files with sites use the DR-09 init macros: 28 brace, uninitialized and compound sites converted by the tool (brace 22, uninit 5, compound 1), two zeroing memsets of an INIT-form drain result removed by hand; test_audio_capture.c, test_phase09_runner.c and test_real_controls.c have no initializer site; runner/tests/test_rdp_video.c (oracle-bound) not touched; no expected value, check, case name or control flow changed"
    requirement: "API-01"
    verification:
      - kind: unit
        ref: "python3 -B scripts/codemod-phase11.py --check over the 10 listed files: RED exit 1 (30 remaining: 28 convertible, 2 hand) at plan_head_before; after conversion runner tests tools report 'remaining 0, hand 0'"
        status: pass
      - kind: integration
        ref: "cmake --build --preset gate (no compiler warning); runner and integration regex 34 of 34 in 62.9 s; ctest --preset gate 268 of 268 in 102.6 s; scripts/preflight-phase11.sh linux-compile compiles all 231 units with gcc 13.3.0"
        status: pass
    human_judgment: false
  - id: D2
    description: "scripts/check-api-init.py fails on an uninitialized t64_stop_request (field-wise fill), a positional or zero brace initializer, a compound literal and a memset (value and pointer form) of a public in/out struct, accepts arrays of the macro, designated descriptor lists and the exempt types, and each rule is proven by a seeded selftest case; CTests repo.api-init-guard (--exclude-oracle-bound) and repo.api-init-guard-selftest are registered with LABELS repo;phase11, COST 1 and TIMEOUT 60"
    requirement: "API-01"
    verification:
      - kind: unit
        ref: "ctest -R '^repo\\.api-init-guard(-selftest)?$' (2 of 2, 0.73 to 0.83 s and 0.08 to 0.10 s); python3 -B scripts/check-api-init.py --selftest 17 of 17; RED report build/gate/11-10-task2/red.txt (10 of 17 cases failing before the rules existed, including the four mandatory seeds)"
        status: pass
      - kind: integration
        ref: "ctest --preset gate 270 of 270 in 116.3 s (inventory 270, 16 phase11 names); sh scripts/preflight-phase11.sh PASS in 101 s; python3 -B scripts/check-api-init.py without the flag reports 120 findings, all in the nine bound files, equal per file and per type to the 11-01 probe"
        status: pass
    human_judgment: false
  - id: D3
    description: "PIN-IMPACT holds the consolidated per-file codemod table of 11-05 to 11-10 (633 tool conversions, 87 hand-converted residuals by the SUMMARY records) reconciled with the 11-01 bound-file inventory"
    requirement: "API-01"
    verification:
      - kind: unit
        ref: "grep of PIN-IMPACT section '11-05 to 11-10 codemod' (79 rows); sh scripts/pii-scan.sh --tree findings=0"
        status: pass
    human_judgment: false

duration: 22min
completed: 2026-11-25
status: complete
---

# Phase 11 Plan 10: Runner/Tests Codemod and the Init-Form Guard Summary

**28 initializer sites in 7 runner/tests files moved to the DR-09 init macros by `scripts/codemod-phase11.py`, and the new `scripts/check-api-init.py` proves that every non-bound consumer is on the init form (172 files, 0 findings) while reporting exactly the 120 category (b) sites of the 11-01 probe in the nine bound files; the full gate (270 tests) and the preflight pass.**

## Performance

- **Duration:** 22 min (22:52Z to 23:14Z); the host load stayed at 9 to 18 from unrelated processes, so the full gate waited for a dip below 9 in a bounded loop (started at 9.6 and 8.9) and no limit was widened
- **Tasks:** 2 of 2 (Task 1 tracer, Task 2 guard), plus one preparatory commit for two sites the guard found
- **Files modified:** 13 (1 created: the guard); 1,026 insertions and 36 deletions

## Accomplishments

- **Tracer (Task 1, `ad1dd6c`).** `--check` over the 10 listed files reported 30 remaining sites (28 convertible, 2 hand memsets). `--dir` converted 28 (brace 22, uninitialized 5, compound literal 1) in 7 files; the 2 hand memsets (a zeroing `memset(&drain, ...)` after an INIT-form declaration in `test_pad_poll.c` and `test_replay.c`) were removed. The gate build is warning-free, the 34 runner, integration and phase 9 harness tests pass (62.9 s) and `ctest --preset gate` passes 268 of 268 (102.6 s), which released the expansion task. Tracer verified end to end, then expanded.
- **Two sites the codemod does not see (`e22030d`).** The new guard's memset rule found two pointer-form fills of an import report in non-bound files outside the plan's list: the 0xA5 canary fill of the aliasing report span in `core/tests/test_state_import.c` (now `poison_import_report`, the shared helper of 11-08) and the zeroing `memset(report_out, ...)` of `inprocess_import` in `runner/tests/test_state_runner.c` (now an assignment from `T64_STATE_IMPORT_REPORT_INIT`). Built and run: `unit.core.state.import`, the two v26 import tests, `runner.state-export` and `runner.state-format-v26` pass.
- **Init-form guard (Task 2, `8dfaed5`).** `scripts/check-api-init.py` (stdlib) reports `RULE path:line: message`, exit 0, 1, 2. Rules `API-INIT-UNINIT`, `API-INIT-POSITIONAL`, `API-INIT-COMPOUND` and `API-INIT-MEMSET` (see key-decisions and patterns). It reads the types from the public header, `EXPECTED_CORPUS_PATHS` with `ast`, has the fixed exclusion for `tools/digest-sweep/t64_digest_sweep.c` with its reason, `--list-cases`, `--case NAME` and `--selftest`; seeds run in a temporary tree under `build/`. RED first: the framework with the rules stubbed failed 10 of 17 cases (`build/gate/11-10-task2/red.txt`), including the four mandatory seeds; the rules then pass 17 of 17 (`green.txt`).
- **CTest names.** `repo.api-init-guard` (the registered command carries `--exclude-oracle-bound`, which the preflight guards step reads back) and `repo.api-init-guard-selftest`, LABELS `repo;phase11`, `COST 1` (measured 0.73 to 0.83 s and 0.08 to 0.10 s, rounded up) and `TIMEOUT 60`; `PHASE11_TESTS` and the regex row `^repo\.api-init-guard` (2) updated in the same commit; inventory 270 and 16 `phase11` names.
- **Consolidated table.** PIN-IMPACT section "11-05 to 11-10 codemod" lists one row per file (79 rows) with the converted count and the hand-converted residuals from the 11-05 to 11-09 SUMMARY files and this plan, the totals per plan, and the reconciliation with the 11-01 probe.

## Task Commits

1. **Task 1: Tracer, codemod commit for runner/tests** - `ad1dd6c` (refactor)
2. **Preparatory (Rule 3): two pointer-form import-report fills found by the guard** - `e22030d` (refactor)
3. **Task 2: Init-form guard, selftest, CTest names, consolidated table** - `8dfaed5` (feat)

**Plan metadata:** the summary and state commits that follow this file. `commits: 3` is `git rev-list --count 0643ac7..8dfaed5` at write time. The Task 1 commit message states the tool breakdown as "brace 21 ... plus 1 more brace counted"; the correct per-file lines give brace 22, uninitialized 5, compound literal 1 (28), as in this file and PIN-IMPACT. The commit is not amended (history is not rewritten).

## Per-file converted counts (Task 1)

| File | Converted | Brace | Uninit | Compound | Hand |
|------|-----------|-------|--------|----------|------|
| runner/tests/test_audio_capture.c | 0 | 0 | 0 | 0 | 0 (a const pointer only) |
| runner/tests/test_cpu_diagnostic.c | 7 | 6 | 0 | 1 | 0 |
| runner/tests/test_fixture_descriptor.c | 1 | 1 | 0 | 0 | 0 |
| runner/tests/test_input.c | 4 | 3 | 1 | 0 | 0 |
| runner/tests/test_pad_poll.c | 4 | 3 | 1 | 0 | 1 (zeroing memset of `drain` removed) |
| runner/tests/test_phase09_runner.c | 0 | 0 | 0 | 0 | 0 (two descriptor lookups) |
| runner/tests/test_real_controls.c | 0 | 0 | 0 | 0 | 0 (see below) |
| runner/tests/test_real_runner.c | 1 | 1 | 0 | 0 | 0 |
| runner/tests/test_replay.c | 6 | 5 | 1 | 0 | 1 (zeroing memset of `drain` removed) |
| runner/tests/test_state_runner.c | 5 | 3 | 2 | 0 | 0 (+1 guard-found, `e22030d`) |
| **Total** | **28** | 22 | 5 | 1 | 2 (+2 guard-found over two files) |

`test_real_controls.c` (checker item I-3, profile-descriptor consumer) copies a looked-up descriptor into a local, edits one member and re-hashes it: it is already the whole-struct assignment `changed = *profile` and stays as it is; after the flip the copy carries the registry's `struct_size`, so the re-hash input stays valid. `test_phase09_runner.c` only looks descriptors up. Three listed files therefore have no initializer site and are not in the commit (7 of 10 files, the same reading as 11-09's 13 of 14); `runner/tests/test_rdp_video.c` is not in it either. Pointer-form memsets in `runner`, `tools` and `tests` of an object of a macro type: none beyond the `report_out` site (the guard reports none after the fix).

## Guard behavior in numbers

| Run | Result |
|-----|--------|
| `check-api-init.py --exclude-oracle-bound` (as registered) | 172 files scanned, 11 excluded (the 10 bound `.c` paths of `EXPECTED_CORPUS_PATHS` and the digest-sweep printer), findings 0, exit 0, about 0.8 s |
| `check-api-init.py` (no flag) | 182 files scanned, 1 excluded, 120 findings, all in the nine bound files, exit 1; per file: `test_rdp.c` 5, `test_rdp_copy.c` 2, `test_rdp_triangles.c` 2, `test_rdp_texture.c` 2, `test_rdp_combiner.c` 2, `test_rdp_blender.c` 3, `test_rdp_depth.c` 2, `test_vi.c` 90, `test_video_output.c` 12; `runner/tests/test_rdp_video.c` 0 |
| by type, no flag | `t64_host_allocator` 16, `t64_stop_request` 16, `t64_stop_result` 35, `t64_cpu_stop_detail` 1, `t64_video_packet_info` 52 = 120, the category (b) edit counts of the 11-01 probe, per file and per type |
| `--selftest` | 17 of 17 (seeds: uninitialized stop request, positional allocator, zero result, compound request, macro forms clean, designated descriptor clean, positional descriptor, empty descriptor fixture clean, memset value form, memset pointer form, arrays of the macro clean, array with a zero entry, array covering too few entries, exempt types clean, members clean, oracle-bound flag, digest-sweep exclusion) |

The 17 category (a) edits of the probe are call expressions and the one category (c) hunk is a `t64_host_storage` initializer (exempt by type, DR-06), so neither is an initializer-form finding. The array rule reports one finding per declaration: the first version reported the two `test_vi.c` arrays twice (an entry and a coverage finding), which made 122; fixed before the commit.

## Decisions Made

Binding decisions are DR-09 (the init macros) and DR-21 (names, COST, TIMEOUT). The choices inside the plan's discretion are in the frontmatter (the memset rule, the scan roots, the lone-zero descriptor, no codemod edit).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Two pointer-form import-report fills in non-bound files outside the plan's list**
- **Found during:** Task 2 (first run of the guard on the tree)
- **Issue:** `python3 -B scripts/check-api-init.py --exclude-oracle-bound` reported two `API-INIT-MEMSET` findings the codemod does not detect: `core/tests/test_state_import.c` (a 0xA5 canary memset of the report span aliased onto live RDRAM) and `runner/tests/test_state_runner.c` (zeroing `memset(report_out, ...)`). The plan's verify and the preflight guards step need the flagged run to exit 0.
- **Fix:** the canary became `poison_import_report(inside)` (the shared helper, so the untouched-bytes compare keeps a non-zero canary and a valid `struct_size` after the flip) and the zeroing became an assignment from `T64_STATE_IMPORT_REPORT_INIT`. A separate commit before the guard commit keeps each commit a compiling, passing state.
- **Files modified:** core/tests/test_state_import.c, runner/tests/test_state_runner.c
- **Verification:** `unit.core.state.import`, `unit.core.state.import-v26-roundtrip`, `unit.core.state.import-v26-hostile`, `runner.state-export`, `runner.state-format-v26` pass; the full gate passes
- **Commit:** `e22030d`

### Other differences from the plan text (not rule deviations)

- **The codemod check cannot report "only the nine bound files".** The acceptance text expects `python3 -B scripts/codemod-phase11.py --check core/tests runner tests` to list only excluded files. It still prints 7 hand listings in `core/tests` (4 arrays in `test_ai.c`, 3 in `test_vg_gaps.c`), all arrays whose every entry already is the INIT form, because the tool lists every array of a macro type as a hand site (recorded since 11-07); `runner` and `tests` print nothing and the exit status stays 1. The plan does not list `scripts/codemod-phase11.py` and the running notes forbid editing it in codemod plans, so it is unchanged; the guard accepts such arrays and is the arbiter from here on.
- **A fourth rule and one more scan root.** The memset rule and the `tools` root are additions, with the reasons in key-decisions.
- **`--selftest` is an extra mode.** The plan names `--list-cases` and `--case NAME`; the CTest runs `--selftest`, which runs every case, as `scripts/check-abi.py` does.
- **Task 1 commit holds 7 of the 10 listed files** (three have no site), as 11-09 held 13 of 14.

**Total deviations:** 1 auto-fixed (Rule 3). **Impact:** none on production code; the two test edits are initializer forms with unchanged expected values and control flow.

## Authentication Gates

None.

## Issues Encountered

- **Unrelated host load.** The 1-minute load average stayed between 9 and 18 from other processes. The full gate for Task 2 waited in a bounded loop until it dipped below 9 and started at 9.6 (116.3 s); the Task 1 gate started at 8.9 (102.6 s). The preflight ran at load 18 (its host-load step warns and never fails). No limit was widened and no run was repeated for load.
- **zsh and git grep.** `git grep -E` with `\b` alternations matched nothing on this host; the type scan was redone without word boundaries. No effect on the tree.

## Open items for later plans (not acted on here)

- `core/tests/test_profile.c` line 340 (`t64_profile_descriptor empty = {0};`, hashed for an INVALID_ARGUMENT check) is the one non-bound zero descriptor; the guard accepts it, but after the flip its `struct_size` is 0 and the expected status of that row changes, so 11-12/11-13 must set the descriptor's `struct_size` there by hand.
- Struct members of the checked types (fixtures holding a `t64_host_allocator`, the capture structs of `runner/t64_raw_capture.c` and `runner/t64_audio_observation.h`) cannot carry an initializer and are outside the guard's reach; 11-06 to 11-09 converted the ones the codemod listed, and `runner` capture entries get their `struct_size` from the call path the flip prologues (11-13) define.
- Statically initialized descriptor tables written with designated initializers (`core/t64_machine.c`, `runner/t64_skeleton_probe.c` `expected_profile`, `core/tests/test_profile.c` `serialization_vector`) pass the guard now; the flip decides whether they gain `.struct_size`.
- The duplicated poison helpers of `test_api_poison.c` and `test_api_lifecycle.c` (11-09) remain a 11-15 review candidate; carried unchanged: the other 11-15 items of 11-03, 11-04 and 11-08.

## Verification Results

| Check | Result |
|-------|--------|
| `python3 -B scripts/codemod-phase11.py --check` over the 10 files | RED: exit 1, 30 remaining (28 convertible, 2 hand) at plan_head_before; after: `runner tests tools` remaining 0, hand 0 |
| `python3 -B scripts/codemod-phase11.py --check core/tests runner tests` | 10 files excluded (nine bound plus `test_rdp_video.c`); 7 INIT-initialized arrays still listed by tool design (see Deviations); nothing convertible |
| `cmake --build --preset gate` | clean (only the pre-existing linker duplicate-library notice) |
| runner and integration regex `^(runner\.\|integration\.\|harness\.phase09)` | 34 of 34, 62.9 s |
| `ctest --preset gate` at the Task 1 commit | 268 of 268, 102.6 s (start load 8.9) |
| import and runner state tests after `e22030d` | 7 of 7 (with the two new names), 72.9 s |
| `ctest --preset gate` before the Task 2 commit | 270 of 270, 116.3 s (start load 9.6); `harness.rdp-oracle-authority`, `repo.si-membership-sites`, `harness.state-field-use-audit`, `repo.p10-mutant-anchors`, `repo.rsp-no-hle` among them, unchanged |
| `python3 -B scripts/check-api-init.py --exclude-oracle-bound` | 172 files, 0 findings, exit 0 |
| `python3 -B scripts/check-api-init.py` (no flag) | 120 findings, all in the nine bound files, equal to the 11-01 probe |
| `ctest -R '^repo\.api-init-guard'` and `--show-only=json-v1` | 2 of 2; COST 1.0, TIMEOUT 60, LABELS phase11 and repo on both |
| `sh scripts/check-test-inventory.sh --preset gate` | PASS, count 270 |
| `sh scripts/preflight-phase11.sh` (clean tree) | PASS, 101 s (ctest counts, guards, timeout audit, pii, 231-unit linux-compile with gcc 13.3.0, post-gate replay) |
| `sh scripts/pii-scan.sh --tree` | findings=0 |
| `git show --name-only --format= ad1dd6c` | exactly 7 files of this task; no bound file, no `test_rdp_video.c`, no deletion |

## Known Stubs

None.

## Threat Flags

None. T-11-16 (a silent zero `struct_size` at the flip) is mitigated: `repo.api-init-guard` rejects the uninitialized, positional, zero, compound and memset forms with a seeded selftest per rule, and the codemod keeps refusing `runner/tests/test_rdp_video.c` and the other bound files.

## Next Phase Readiness

The SHAPE wave can continue with 11-11 (prefix rename and stale-freeze text) and then the flip (11-12): every non-bound consumer of a public in/out struct now uses the init macros, so the flip changes macro bodies only; the nine bound files still hold their 120 old-form sites, applied by 11-14 in the isolated commit. API-01 is not complete (the flip, the `struct_size` prologues and the installed consumer are ahead). Nothing in this plan is orchestrator-only, no owner decision is pending and nothing was pushed.

## Self-Check: PASSED

- FOUND: scripts/check-api-init.py; the 7 changed runner/tests files and core/tests/test_state_import.c
- FOUND commits (ancestors of HEAD): ad1dd6c, e22030d, 8dfaed5
