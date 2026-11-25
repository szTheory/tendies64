---
phase: 11-installed-consumer-and-freeze-candidate
plan: 06
subsystem: api
tags: [init-macros, codemod, core-tests, group-a, shape-wave]

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: plan 11-05 (the ten public init macros of DR-09 expanding to today's layouts, scripts/codemod-phase11.py with its bound-path refusal)
provides:
  - the 14 files of core/tests group A (CPU, boot and ROM admission) on the init macros, with unchanged results
  - per-file converted counts for the PIN-IMPACT consolidation of 11-10
affects: [11-07, 11-08, 11-09, 11-10, 11-12, 11-13, 11-14, 11-15]

actuals:
  tokens: 13900
  tasks: 2
  commits: 1
plan_head_before: 7ed520ffebf87f3ff31fa1660ce8e17441dac3f7
plan_head_after: 82012624551ebd0b1907fdec6320afb307ae3bd5

tech-stack:
  added: []
  patterns:
    - "a struct member of a macro type that the callee fills is set from the INIT form right after its owner is created (whole-struct assignment of a compound literal), so the flip's leading struct_size is present"
    - "a host allocator held as a struct member is filled by one T64_HOST_ALLOCATOR_INIT assignment instead of three member assignments"

key-files:
  created: []
  modified:
    - core/tests/fuzz_rom_admission.c
    - core/tests/test_rom_admission.c
    - core/tests/test_boot_determinism.c
    - core/tests/test_boot_entry.c
    - core/tests/test_bus.c
    - core/tests/test_cop1.c
    - core/tests/test_cpu_cache.c
    - core/tests/test_cpu_exceptions.c
    - core/tests/test_cpu_integer.c
    - core/tests/test_cpu_link.c
    - core/tests/test_cpu_memory.c
    - core/tests/test_cpu_mmu.c
    - core/tests/test_cpu_path.c
    - core/tests/test_cpu_time.c

key-decisions:
  - "The two struct members the codemod reported (test_boot_determinism.c machine_run.stop, test_cpu_link.c fixture.allocator) are filled from the INIT form by hand (three edits, the commit message counts them as three members), because both members are passed to the core (an out parameter and an allocator) and a zeroed leading struct_size would refuse after the flip"
  - "fuzz_rom_admission.c is built only by the asan preset (T64_HAS_ADDRESS_AND_UNDEFINED_SANITIZERS), so its conversion is verified by building and running fuzz.rom-admission there in addition to the gate preset"

patterns-established:
  - "Group conversions run --check first (RED), --dir (GREEN), then --check again; 0 hand sites reported means no memset of a macro-type object exists in the group"

requirements-completed: []

coverage:
  - id: D1
    description: "The 14 files of core/tests group A use the DR-09 init macros: positional brace initializers, compound literals and uninitialized declarations are converted, two struct members of macro type are filled from the INIT form, and no oracle-bound file, expected value, check, case name or control flow changed"
    requirement: "API-01"
    verification:
      - kind: unit
        ref: "python3 -B scripts/codemod-phase11.py --check over the 14 files: RED exit 1 (147 sites, 0 hand) at plan_head_before, GREEN exit 0 (0 sites) after conversion; python3 -B scripts/codemod-phase11.py --dir core/tests/test_rdp.c exits 2 (refused: oracle-bound)"
        status: pass
      - kind: integration
        ref: "cmake --build --preset gate; ctest -R group regex (20 of 20); ctest -L unit (128 of 128, 312 s under unrelated host load); sampling set (5 of 5); ctest --preset gate 268 of 268 in 118 s; fuzz.rom-admission under the asan preset (passed, 40.6 s); sh scripts/pii-scan.sh --tree findings=0"
        status: pass
    human_judgment: false

duration: 39min
completed: 2026-11-25
status: complete
---

# Phase 11 Plan 06: Core Tests Codemod, Group A (CPU, Boot and ROM Admission) Summary

**147 initializer sites in the 14 core/tests group A files moved to the DR-09 init macros by `scripts/codemod-phase11.py` in one compiling commit, plus 3 struct-member fills done by hand; the full gate (268 tests) passes unchanged.**

## Performance

- **Duration:** 39 min (about 25 min of it waiting for the shared host load to fall; see Issues Encountered)
- **Started:** 2026-11-25T21:26Z
- **Completed:** 2026-11-25T22:05Z (this summary and the state update follow)
- **Tasks:** 2 of 2 (Task 1 is the tracer and did not commit by plan; Task 2 committed once)
- **Files modified:** 14 (0 created)

## Accomplishments

- **Tracer (Task 1).** `--check` over exactly the 14 files reported 147 remaining sites (0 hand, 2 struct-member notes). `--dir` converted all 147 (brace 138, uninitialized 3, compound literal 6; per-file counts below), `--check` then exited 0 with 0 sites. The two member notes were handled by hand. The gate build is warning-free and the 20 CTests built from these files pass; `fuzz_rom_admission.c` is outside the gate preset and was built and run under the asan preset.
- **Commit (Task 2).** One commit, `8201262`, lists exactly the 14 files; no bound file, no deletion. The full `ctest --preset gate` (268 tests), the sampling set (5) and the PII scan pass before it.
- **Sentinel forms.** Seventeen `t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX}` sites (1 in `test_boot_entry.c`, 16 in `test_cpu_path.c`) became `T64_CPU_STOP_DETAIL_INIT` followed by the three named member assignments, in the member order of the type (`retired_instructions`, `pc`, `opcode`), so a leading `struct_size` cannot shift the values.

## Task Commits

1. **Task 1: Tracer, codemod over group A, hand conversions, build and run** - no commit (plan: Task 1 does not commit)
2. **Task 2: Full gate, sampling-set guards and the group A commit** - `8201262` (refactor)

**Plan metadata:** the summary and state commits that follow this file. The measured `commits:` count of 1 is `git rev-list --count 7ed520f..8201262` at write time.

## Per-file converted counts (consolidated into PIN-IMPACT by plan 11-10)

| File | Converted by tool | Brace | Uninit | Compound | Hand (member fills) | memset hand |
|------|-------------------|-------|--------|----------|---------------------|-------------|
| core/tests/fuzz_rom_admission.c | 1 | 0 | 0 | 1 | 0 | 0 |
| core/tests/test_rom_admission.c | 4 | 4 | 0 | 0 | 0 | 0 |
| core/tests/test_boot_determinism.c | 15 | 7 | 3 | 5 | 2 (`machine_run.stop` after the memset in `initialize_run`, and for `observation`) | 0 |
| core/tests/test_boot_entry.c | 14 | 14 | 0 | 0 | 0 | 0 |
| core/tests/test_bus.c | 9 | 9 | 0 | 0 | 0 | 0 |
| core/tests/test_cop1.c | 5 | 5 | 0 | 0 | 0 | 0 |
| core/tests/test_cpu_cache.c | 3 | 3 | 0 | 0 | 0 | 0 |
| core/tests/test_cpu_exceptions.c | 7 | 7 | 0 | 0 | 0 | 0 |
| core/tests/test_cpu_integer.c | 4 | 4 | 0 | 0 | 0 | 0 |
| core/tests/test_cpu_link.c | 4 | 4 | 0 | 0 | 1 (`fixture.allocator`) | 0 |
| core/tests/test_cpu_memory.c | 1 | 1 | 0 | 0 | 0 | 0 |
| core/tests/test_cpu_mmu.c | 1 | 1 | 0 | 0 | 0 | 0 |
| core/tests/test_cpu_path.c | 54 | 54 | 0 | 0 | 0 | 0 |
| core/tests/test_cpu_time.c | 25 | 25 | 0 | 0 | 0 | 0 |
| **Total** | **147** | 138 | 3 | 6 | 3 | 0 |

Brace plus uninitialized plus compound equals 147 (138 + 3 + 6). The tool's own per-file lines are the source: `test_boot_determinism.c` reports brace 7, uninit 3, compound 5 and `fuzz_rom_admission.c` compound 1. No `memset` of a macro-type object exists in this group (the tool reported 0 hand sites), so none was removed.

State at the end of this plan for the later plans (`--check`): core/tests 473 sites (55 hand: 48 memset of a macro-type object and 7 arrays; 9 bound files excluded; 620 before this plan, 620 - 147 = 473), runner/tests 30 sites (2 hand, both memset, 1 bound file excluded). Both numbers are unchanged apart from the 147 converted here.

## New non-bound callers (grep first)

The 14 listed files were checked against the codemod's own report: every file that uses a macro type in group A is in the plan's list; no additional group A file was found. The three members of the 11-04 running note (`test_api_surface.c`, `test_api_poison.c`, `test_api_lifecycle.c`) belong to 11-09's list and were not touched.

## Files Created/Modified

The 14 files above, initializer forms only. Per file diff (insertions): `test_cpu_path.c` 121, `test_cpu_time.c` 39, `test_boot_determinism.c` 32, `test_boot_entry.c` 30, `test_bus.c` 16, the other nine 2 to 9 each; 288 insertions and 170 deletions in all.

## Decisions Made

Binding decision is DR-09. The choices inside the plan's discretion are in the frontmatter (struct members filled from the INIT form; the asan-only fuzz binary).

## Deviations from Plan

None - plan executed exactly as written.

The plan action says to hand-convert "any array or struct-member site the tool reported (struct_size set in a loop or initializer; no other change)". The tool reported two struct members (not arrays), and three assignments were added or replaced for them:

- `core/tests/test_boot_determinism.c`: `machine_run` holds a `t64_stop_result stop` that `advance_one_segment` passes to `t64_machine_advance` as the out parameter. `initialize_run` zeroes the whole run with `memset`, and `observation` is declared with `{0}`; both now assign `stop = (t64_stop_result)T64_STOP_RESULT_INIT;` right after (two added lines). The `output` object of `cold_check_final` never passes its `stop` to the core and is untouched.
- `core/tests/test_cpu_link.c`: `fixture` holds a `t64_host_allocator allocator` filled by three member assignments and passed to `t64_machine_create`; replaced by one `value->allocator = (t64_host_allocator)T64_HOST_ALLOCATOR_INIT(test_allocate, test_release, value);`. The values are identical.

These are the plan's stated hand conversions, so they are recorded here for transparency rather than as deviations. The flip (11-12) changes only the macro bodies for them.

**Total deviations:** 0 (3 hand member fills as planned). **Impact:** none on scope.

## Authentication Gates

None.

## Issues Encountered

- **Unrelated host load.** Another process group on the shared host (a history audit of other repositories, not part of this project) drove the load average from about 10 to 130 during the `ctest -L unit` and sampling-set runs. Both passed regardless (unit label 128 of 128 in 312 s, sampling set 5 of 5 in 282 s) because their limits were not widened and their results are pass or fail; the runs took far longer than the 123 s the plan quotes for a full ctest. Before the full gate I waited for the load to fall to 13.9 (the level earlier plans passed at) and ran `ctest --preset gate` once: 268 of 268 passed in 118 s. No limit was widened and no test was re-run.
- `fuzz_rom_admission.c` is built only when `T64_HAS_ADDRESS_AND_UNDEFINED_SANITIZERS` holds, which is the asan preset, so `cmake --build --preset gate` does not compile it. It was compiled and run through `cmake --build --preset asan --target test_fuzz_rom_admission` and `ctest --test-dir build/asan -R '^fuzz\.rom-admission$'` (passed, 40.6 s).

## Open items for later plans (not acted on here)

- Remaining core/tests groups: 473 convertible sites and 55 hand sites (48 memset, 7 arrays in `test_ai.c` and `test_vg_gaps.c`) at the head of this plan; 11-07 to 11-10 apply the same tool and hand-convert each memset.
- Struct members of macro type that other groups fill (anything the tool prints as "is a struct member") need the same treatment as here when the callee reads their `struct_size`; this group's two are the model.

## Verification Results

| Check | Result |
|-------|--------|
| `python3 -B scripts/codemod-phase11.py --check` over the 14 files | RED: exit 1, 147 sites, 0 hand; GREEN: exit 0, 0 sites (the two member notes remain by design) |
| `python3 -B scripts/codemod-phase11.py --dir core/tests/test_rdp.c` | exit 2, "refused core/tests/test_rdp.c: oracle-bound" |
| `cmake --build --preset gate` | clean |
| group CTest regex `^(unit\.core\.(boot-entry\|cpu-path\|cpu-link\|cpu-icache\|cpu-cache-ops\|cpu-dcache\|cp0-cache\|cpu-time\|cpu-exceptions\|cpu-mmu\|cpu-memory\|cpu-integer\|cpu-control\|cop1-state\|cop1-arithmetic\|cop1-exceptions\|rom-admission\|bus)\|integration\.(open-ipl3-determinism\|cold-determinism))$` | 20 of 20 passed (10 s) |
| `ctest --test-dir build/gate -L unit` | 128 of 128 passed, 312 s (host load 70 to 130, unrelated) |
| asan preset `fuzz.rom-admission` | passed, 40.6 s |
| sampling set (`harness.rdp-oracle-authority`, `repo.si-membership-sites`, `harness.state-field-use-audit`, `repo.p10-mutant-anchors`, `repo.rsp-no-hle`) | 5 of 5 passed (282 s under the same load) |
| `ctest --preset gate` | 268 of 268 passed, 118 s (load 13.9 at start) |
| `sh scripts/pii-scan.sh --tree` | findings=0 |
| `git show --name-only --format= HEAD` | exactly the 14 files; no bound file, no deletion |

## Known Stubs

None.

## Threat Flags

None. T-11-59 (test semantics of group A) is mitigated by one compiling commit, the group's binaries, the unit label, the full gate and the sampling set before the commit, initializer forms only, and the diff left for the 11-15 review.

## Next Phase Readiness

Plan 11-07 can apply the same tool to the next core/tests group. API-01 is not complete: the remaining codemod groups, the flip, the `struct_size` prologues and the installed consumer are ahead. Nothing in this plan is orchestrator-only and no owner decision is pending.

## Self-Check: PASSED

- FOUND: all 14 modified files, `core/tests/test_bus.c` containing `_INIT`
- FOUND commit: 8201262 (ancestor of HEAD)
