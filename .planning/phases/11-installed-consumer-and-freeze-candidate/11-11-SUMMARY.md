---
phase: 11-installed-consumer-and-freeze-candidate
plan: 11
subsystem: api
tags: [symbol-prefix, softfloat, provenance, stale-freeze-text, adr-amendment, carry-ins, shape-wave]

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: plans 11-02 to 11-04 (the last new public symbols, so the rename header was generated after them), 11-10 (tools/abi/CMakeLists.txt, PHASE11_TESTS and the regex table the new names extend)
provides:
  - core/t64_prefix.h (generated, 220 `#define name t64i_name` lines) and scripts/gen-prefix-header.py (--write and --check); the archive defines exactly the 35 public functions plus t64i_ names
  - the nine vendored SoftFloat primitives without their self-defining guard (20 deleted lines), recorded in provenance/copies.json
  - CTest repo.abi-prefix, repo.stale-freeze-text and repo.stale-freeze-text-selftest
  - the relabeled production text (20 lines in 8 files, the facade banner and line 6, the t64_machine.h banner, ADR-0002 lines 55 and 91), the ADR-0002 and ADR-0006 amendments
  - carry-ins W2-02 (two membership tokens, six site rows) and T7 (public name T64_TRACE_KIND_SI_REFUSED)
affects: [11-12, 11-13, 11-14, 11-15, 11-16, 11-17, 11-19]

actuals:
  tokens: 25700
  tasks: 4
  commits: 7

plan_head_before: c385a116609b52b2d98efd29d5a330e480d66e8b
plan_head_after: b37bebdeaf9f64a816bf46ea627a8c1d8576dcbf

tech-stack:
  added: []
  patterns:
    - "one #include of the generated core/t64_prefix.h from every core header a translation unit can reach first (t64_internal.h, softfloat/platform.h, t64_rsp_divider.h, t64_facade.h) instead of a -include flag, so the lint compile, white-box tests and probes see the same rename without a new compile option; the core directory is on the include path of t64_core, the lint default scan and the three runner targets that include the facade"
    - "the generator keeps a header entry only while the archive defines its t64i_ target and renames every other non-public defined global; --check fails on an unprefixed global, a stale entry, a missing public function or any drift of regeneration"
    - "checkers that read object symbols resolve a literal canonical name through the #define renames of core/t64_prefix.h (read, never restated)"

key-files:
  created:
    - core/t64_prefix.h
    - scripts/gen-prefix-header.py
    - scripts/check-stale-freeze-text.sh
  modified:
    - core/softfloat/s_approxRecip32_1.c
    - core/softfloat/s_approxRecipSqrt32_1.c
    - core/softfloat/s_countLeadingZeros32.c
    - core/softfloat/s_countLeadingZeros64.c
    - core/softfloat/s_mul64To128.c
    - core/softfloat/s_shiftRightJam32.c
    - core/softfloat/s_shiftRightJam64.c
    - core/softfloat/s_shiftRightJam64Extra.c
    - core/softfloat/s_shortShiftRightJam64.c
    - provenance/copies.json
    - core/softfloat/platform.h
    - core/t64_internal.h
    - core/t64_rsp_divider.h
    - core/include/tendies64/t64_facade.h
    - core/include/tendies64/t64_machine.h
    - core/t64_si.c
    - core/t64_machine.c
    - core/t64_state.c
    - core/CMakeLists.txt
    - runner/CMakeLists.txt
    - tools/lint/core-lint.sh
    - tools/abi/CMakeLists.txt
    - scripts/check-rsp-no-hle.py
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase11-regexes.txt
    - scripts/list-membership-sites.py
    - docs/spec/profile-membership-sites.txt
    - docs/adr/0002-language-build-and-abi.md
    - docs/adr/0006-determinism-lint-mechanism.md
    - runner/t64_artifact_publisher.h
    - runner/t64_replay.h
    - runner/t64_save_flags.h
    - runner/tests/test_state_runner.c
    - .planning/phases/11-installed-consumer-and-freeze-candidate/PIN-IMPACT.md

key-decisions:
  - "Prefix mechanism: one #include of core/t64_prefix.h from t64_internal.h, softfloat/platform.h, t64_rsp_divider.h and t64_facade.h (not -include); the core directory joins the t64_core include path privately and the lint default scan gains -I core"
  - "The runner headers (artifact publisher, replay, save flags) are relabeled as outside the ABI and the freeze record, not as provisional until the candidate: the runner is not installed, so the Phase 11 candidate does not bind its names"
  - "T7: the third SI trace kind is a refused SI register write that tracing records and the public drain returns, so it is public as T64_TRACE_KIND_SI_REFUSED (value 3, alias asserted in t64_si.c)"

patterns-established:
  - "A checker follow-up that only repairs the first isolated refresh is a second checker-only commit, never an amend"

requirements-completed: []

coverage:
  - id: D1
    description: "Internal symbols are prefixed: nm of the core archive lists 35 public names and 220 t64i_ names (255 defined globals before the rename); the nine SoftFloat guards are removed with 20 deleted lines and no added line; SoftFloat suites keep checks=10466 and 10478 with failures=0"
    requirement: "API-01"
    verification:
      - kind: unit
        ref: "python3 -B scripts/gen-prefix-header.py --check build/gate/core/libt64_core.a (ok, 35 public functions, 220 renames); ctest unit.core.softfloat32/softfloat64/softfloat-isolation/cop1-state/lifecycle 5 of 5; sh tools/lint/core-lint.sh (99 files, 0 violations) and --selftest 27 of 27; sh scripts/check-provenance.sh errors=0"
        status: pass
      - kind: integration
        ref: "ctest --preset gate 273 of 273 (126 s) at the last code commit; four generator controls (unprefixed global, stale entry, order drift, idempotent write) as seeded"
        status: pass
    human_judgment: false
  - id: D2
    description: "repo.abi-prefix registered (LABELS repo;phase11, COST 1, TIMEOUT 30); scripts/check-rsp-no-hle.py resolves its canonical symbols through the prefix map in checker-only commits; the p10 mutant drivers build and classify against the renamed core"
    requirement: "API-01"
    verification:
      - kind: integration
        ref: "ctest repo.abi-prefix, repo.rsp-no-hle, harness.rdp-oracle-authority, repo.si-membership-sites, harness.state-field-use-audit, repo.p10-mutant-anchors pass; p10-mutants.py --run: control-equivalent-depth-cap SURVIVED, control-no-compile INVALID, eight mutants (one per target source) KILLED"
        status: pass
    human_judgment: false
  - id: D3
    description: "No production comment or ADR line claims a Phase 10 freeze: git grep for 'until (the )?Phase 10' over core, runner and tools prints nothing (20 lines before), repo.stale-freeze-text and its selftest pass; ADR-0002 and ADR-0006 carry the 2026-11-25 amendments; closed Phase 9 records untouched"
    requirement: "API-01"
    verification:
      - kind: unit
        ref: "sh scripts/check-stale-freeze-text.sh (files=295 findings=0) and --selftest (10 cases, 0 failures; RED: stubbed rules failed 7 of 10, the unrelabeled tree reported 23 findings); sh scripts/check-docs.sh ok and --selftest 23 cases 0 failures; git diff --quiet f9124ae9fce1 -- docs/claims docs/evidence fixtures/state/decode_container.py"
        status: pass
    human_judgment: false
  - id: D4
    description: "Carry-ins W2-02 and T7 are done and the digest sweep against the Wave 0 base reports 0 differences"
    requirement: "API-01"
    verification:
      - kind: integration
        ref: "python3 -B scripts/digest-sweep.py --base f9124ae9fce1 --head e9e0cbe: sweep: 1599 shared lines, 0 differences, 0 head-only lines; repo.si-membership-sites and harness.phase09-qualification pass; PIN-IMPACT rows W2-02, T4 and T7 read done (11-11)"
        status: pass
    human_judgment: false

duration: 32 min
completed: 2026-11-25
status: complete
---

# Phase 11 Plan 11: Internal Symbol Prefix, Stale Freeze Text and Shape-Wave Carry-Ins Summary

**The core archive now defines exactly the 35 public functions plus 220 `t64i_` names (a generated `core/t64_prefix.h`, nine SoftFloat guards removed, `repo.abi-prefix` as the gate), the 20 stale "until Phase 10" production lines and their banners are relabeled with a deny-list guard, ADR-0002 and ADR-0006 carry the 2026-11-25 amendments, and carry-ins W2-02 and T7 are done; the sweep reports 0 differences and the full gate passes 273 of 273.**

## Performance

- **Duration:** 32 min (start 23:15Z); the host load was 9 to 24 from unrelated processes, so the two gate runs and the preflight waited in a bounded loop for a dip below 9 and no limit was widened
- **Tasks:** 4 of 4 (Task 1 tracer, Task 2 checker map and CTest, Task 3 relabel and guard, Task 4 carry-ins and record); the plan stayed inside the 45 minute bound, so no split was needed
- **Files modified:** 36 outside `.planning` (3 created) plus PIN-IMPACT; 893 insertions and 88 deletions

## Accomplishments

- **Tracer (Task 1, `df6962c`).** The nine primitives lost their `#ifndef`, trailing `#endif` and (two files) self `#define`: 20 deleted lines, none added, nine `copies.json` modifications strings extended, `sh scripts/check-provenance.sh` errors=0. `scripts/gen-prefix-header.py --write` generated 220 renames from the built archive (255 defined globals, 35 public); one `#include "t64_prefix.h"` in `core/t64_internal.h`, `core/softfloat/platform.h`, `core/t64_rsp_divider.h` and `core/include/tendies64/t64_facade.h` makes the t64_core build, every white-box test, the lint compile and the three runner facade consumers see it. nm of the rebuilt archive: 35 names without the prefix and 220 with it, all `T`. Verify run end to end (tracer gate, interactive mode with `end-of-phase` human verification and an automated-only verify: re-run, passed, no checkpoint): `--check` ok, the five unit suites pass (checks 10466, 10478, 84, 829, 317, failures 0), lint 99 files 0 violations and selftest 27 of 27. Tracer verified end to end, then expanded.
- **Checker map (Task 2, `270dc51`, `0b3d911`, `979a747`).** `scripts/check-rsp-no-hle.py` resolves the five groups of literal canonical symbols through the `#define` renames of `core/t64_prefix.h`; no `REVIEWED_BODY_DIGESTS` value moved and no other script reads object symbols (the grep found only `lint-objects.sh`, which compares defined against undefined names within one compile). `repo.abi-prefix` is registered (`python3 -B scripts/gen-prefix-header.py --check ${CMAKE_BINARY_DIR}/core/libt64_core.a`, LABELS `repo;phase11`, COST 1, TIMEOUT 30), `PHASE11_TESTS` lists it and `^repo\.abi` is 4. `p10-mutants.py --run`: both controls as before (SURVIVED, INVALID) and eight mutants, one per target source, KILLED (ids in PIN-IMPACT).
- **Relabel, ADRs and guard (Task 3, `3f9a8bd`).** All 20 "until Phase 10" production lines (8 files), `t64_facade.h` line 6 and its banner, the `t64_machine.h` banner and ADR-0002 lines 55 and 91 are relabeled to "the Phase 11 freeze candidate (final ABI bind at the end of Phase 12)" or, for the three runner headers and one test comment, to a statement that the runner is outside the ABI and the freeze record. ADR-0002 gains the dated 2026-11-25 amendment (the language-reversal window closes at the Phase 12 final bind) and ADR-0006 the allowlist amendment with the three DR-19 rows and the DR-31 revision note. `scripts/check-stale-freeze-text.sh` (POSIX sh, `LC_ALL=C`, `grep -rn -I -i -E`, no git) has a seeded selftest of 10 cases; RED was the stubbed rules failing 7 of 10 cases and the real tree reporting 23 findings, GREEN is 0 findings and 10 of 10 cases.
- **Carry-ins and record (Task 4, `e9e0cbe`, `b37bebd`).** W2-02: the two tokens regenerate the list with the six named site rows and three changed detail columns. T7: `T64_TRACE_KIND_SI_REFUSED` (value 3) with a `_Static_assert` alias in `core/t64_si.c`; the value and every digest are unchanged (sweep 0 differences). PIN-IMPACT holds the 11-11 shape-step row, the plan evidence and the three carry-in rows as `done (11-11)` with evidence.

## Task Commits

1. **Task 1: tracer, SoftFloat guards and the generated prefix header** - `df6962c` (feat)
2. **Task 2: isolated no-HLE lookup refresh** - `270dc51` (chore), checker-only follow-up `0b3d911` (chore), `repo.abi-prefix` registration `979a747` (feat)
3. **Task 3: relabel, ADR amendments, stale-text guard** - `3f9a8bd` (feat)
4. **Task 4: carry-ins W2-02 and T7** - `e9e0cbe` (feat); PIN-IMPACT record `b37bebd` (docs)

**Plan metadata:** the summary and state commits that follow this file. `commits: 7` is `git rev-list --count c385a11..<head at write time>`.

## Decisions Made

Binding decisions are DR-17 (prefix mechanism and header), DR-18 (facade not installed, relabeled), DR-19 and DR-31 (installed-archive allowlist rows), DR-21 (names, COST, TIMEOUT), DR-25 (carry-in dispositions). The choices inside the plan's discretion are in the frontmatter.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Four files outside the plan's list needed the rename include or its include path**
- **Found during:** Task 1 (first full build after the include in `t64_internal.h` and `platform.h`)
- **Issue:** `core/t64_rsp_divider.c` includes no other core header, so its definitions stayed unprefixed while `t64_rsp.c` called `t64i_rsp_divider_step` (link failure in every test); the SoftFloat translation units, the lint compile and the three runner targets that include the facade could not resolve `t64_prefix.h` without the core directory on their include path
- **Fix:** `#include "t64_prefix.h"` in `core/t64_rsp_divider.h`; `-I core` in the default scan of `tools/lint/core-lint.sh` (the plan names "the matching flag in the lint's compile line"); the core directory as a private include of `t64_core` in `core/CMakeLists.txt` (listed) and of `t64-runner-synthetic`, `t64-supervisor` and `t64-supervisor-test` in `runner/CMakeLists.txt`; `core/softfloat/platform.h` carries the include for the SoftFloat units
- **Files modified:** core/t64_rsp_divider.h, core/softfloat/platform.h, tools/lint/core-lint.sh, runner/CMakeLists.txt
- **Verification:** the gate builds clean, lint 0 violations, `--check` ok, full gate 273 of 273
- **Commit:** `df6962c`

**2. [Rule 1 - Bug] The no-HLE control suite failed after the first isolated refresh**
- **Found during:** Task 2 (first full `ctest --preset gate`, 270 of 271)
- **Issue:** `scripts/tests/test_rsp_no_hle.py` copies a fixed source set (no `core/t64_prefix.h`) next to the real build objects, so `_prefix_renames(root)` found no header and the checker reported the symbols absent
- **Fix:** the map is read from the inspected tree and, when that tree has no header, from the checker's own repository (the one that compiled the objects); with no header at all every lookup fails closed. Recorded as a second checker-only commit because the first isolated commit is not amended (no history rewrite); both touch only `scripts/check-rsp-no-hle.py`
- **Files modified:** scripts/check-rsp-no-hle.py
- **Verification:** `repo.rsp-no-hle` passes (including its control suite) and `python3 -B scripts/check-rsp-no-hle.py --build build/gate` exits 0
- **Commit:** `0b3d911`

### Other differences from the plan text (not rule deviations)

- **Measured counts differ from the plan's estimates.** The plan states 248 unprefixed globals at HEAD; the archive held 255 (35 public, 220 internal) because plans 11-02 to 11-04 added symbols after the estimate. The acceptance `grep -c 't64_has_si_v26_profile' docs/spec/profile-membership-sites.txt` is stated as 0 at HEAD; it was 1 (the function column of the `t64_internal.h` row) and is 10 after the regeneration; the six new site rows and three changed detail columns, the substance of W2-02, are as the plan describes.
- **Runner headers say "outside the freeze record" instead of "provisional until the candidate".** The runner is not installed and the candidate does not bind its names; one phrase does not fit them.
- **The ADR-0006 amendment names `tools/lint/allowlist-installed.txt`, which plan 11-17 creates.** The amendment records the rule (DR-19, DR-31); the file lands with the installed-object check.
- **TDD evidence for Task 3 is the RED milestone, not a separate `test(...)` commit.** The task is `type="auto" tdd="true"` inside an `execute` plan; as in plan 11-10 the RED report (`build/gate/11-11-task3/red-selftest.txt`, `red-real-tree.txt`) is kept in the build tree and the guard, selftest and relabel are one commit.
- **Task 2 registers one CTest name (`repo.abi-prefix`) and no selftest for the generator.** The generator's failing controls (unprefixed global, stale entry, order drift, idempotent write) were run by hand and are recorded in PIN-IMPACT; a registered selftest would add a CTest name beyond the DR-21 list.

**Total deviations:** 2 auto-fixed (one Rule 3, one Rule 1). **Impact:** none on behavior; both are build-plumbing or checker-fixture repairs the plan's own "all consumers must see it" and "checkers follow the prefix" sentences require.

## Authentication Gates

None.

## Issues Encountered

- **Unrelated host load.** The load average stayed between 9 and 24 from other processes; the gate runs and the preflight started after a bounded wait for a dip below 9 (8.9 and 8.4); no limit was widened and no run was repeated for load. The mutant run (3 to 8 s per mutant) started without the wait.
- **zsh PIPESTATUS.** `${PIPESTATUS[0]}` is empty in this shell; exit codes were read with `${pipestatus[1]}` or directly.

## Open items for later plans (not acted on here)

- Plan 11-16 (install recipe): the core directory is a private include of `t64_core`; the install interface must expose only `core/include`. The four sha256 names keep their host names until DR-20 renames core's private copy.
- `repo.abi-prefix` and the generator's nm parsing are proven on macOS only; the first hosted Linux run (the 11-15 push) is the first ELF check, and a platform-conditional global would show there as a difference between the Darwin-generated header and the Linux archive.
- A new core translation unit that includes none of the four rename carriers would define unprefixed names; `repo.abi-prefix` fails it, and the fix is the include, not a header entry.

## Verification Results

| Check | Result |
|-------|--------|
| `cmake --build --preset gate` | clean (only the pre-existing linker duplicate-library notice) |
| `python3 -B scripts/gen-prefix-header.py --check build/gate/core/libt64_core.a` | ok, 35 public functions, 220 renames; four controls as seeded |
| `ctest -R '^unit\.core\.(softfloat32\|softfloat64\|softfloat-isolation\|cop1-state\|lifecycle)$'` | 5 of 5; checks 10466, 10478, 84, 829, 317; failures 0 |
| `sh tools/lint/core-lint.sh`, `--selftest`, `sh scripts/check-provenance.sh` | 99 files 0 violations, 27 of 27, errors=0 |
| `git diff --numstat` of the nine SoftFloat files | 0 added, 20 deleted (3, 3 and seven times 2) |
| `python3 -B scripts/check-rsp-no-hle.py --build build/gate` | exit 0 (red between `df6962c` and `270dc51` by design) |
| `ctest --preset gate` at the registration state | 270 of 271 in 104 s; the one failure was the control suite fixed by `0b3d911` |
| `ctest --preset gate` at the last code commit (`e9e0cbe`) | 273 of 273 in 126 s (start load 8.4) |
| `p10-mutants.py --run` | controls SURVIVED and INVALID as before; 8 mutants KILLED (ids in PIN-IMPACT) |
| `git grep -n -i -E 'until (the )?Phase 10' -- core runner tools` | prints nothing (20 lines before); `grep -c 'freeze is Phase 10' core/include/tendies64/t64_facade.h` 0 |
| `sh scripts/check-stale-freeze-text.sh` and `--selftest` | files=295 findings=0; 10 cases 0 failures |
| `sh scripts/check-docs.sh` and `--selftest` | ok; 23 cases 0 failures |
| `git diff --quiet f9124ae9fce1 -- docs/claims docs/evidence fixtures/state/decode_container.py` | exit 0 |
| `python3 -B scripts/digest-sweep.py --base f9124ae9fce1 --head e9e0cbe` | 1599 shared lines, 0 differences, 0 head-only lines |
| `repo.si-membership-sites`, `harness.phase09-qualification`, `repo.abi-status-classes` | pass |
| `sh scripts/check-test-inventory.sh --preset gate` | PASS, count 273 |
| `sh scripts/pii-scan.sh --tree` | findings=0 |
| `sh scripts/preflight-phase11.sh` (clean tree) | PASS in 80 s: ctest counts (20 rows, 0 mismatches; 19 tests carry `phase11`), guards (`check-api-init.py`, `check-stale-freeze-text.sh`), timeout audit (126 TIMEOUTs, 0 too low), pii, the 231-unit `linux-compile` with the act image gcc 13.3.0 and the post-gate replay (11 steps); its host-load step never fails |

## Known Stubs

None.

## Threat Flags

None. T-11-17 (symbol interposition) is mitigated at the archive: every non-public global is `t64i_`-prefixed and `repo.abi-prefix` fails any other; the results-asserting collision control is plan 11-19. T-11-18 is mitigated (each of the nine `copies.json` rows records the edit, the provenance check and the SoftFloat suites keep their counts). T-11-19 is mitigated (the two checker commits change lookups only, no digest).

## Next Phase Readiness

The SHAPE wave continues with 11-12 (the `struct_size` flip): the init macros, the prefix and the relabeled banners are in, and the build id moved once for this plan. No owner decision is pending, nothing is orchestrator-only in this plan and nothing was pushed. API-01 and API-04 are not complete (the flip, the handshake, the installed consumer and the installed-object lint are ahead), so no requirement is marked complete.

## Self-Check: PASSED

- FOUND: core/t64_prefix.h, scripts/gen-prefix-header.py, scripts/check-stale-freeze-text.sh
- FOUND commits (ancestors of HEAD): df6962c, 270dc51, 0b3d911, 979a747, 3f9a8bd, e9e0cbe, b37bebd

