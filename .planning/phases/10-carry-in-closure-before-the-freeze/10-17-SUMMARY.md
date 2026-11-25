---
phase: 10-carry-in-closure-before-the-freeze
plan: 17
subsystem: testing
tags: [qualifier, native-qualification, rehearsal, gate-growth-ledger, hermetic-tests, thin-delta]

requires:
  - phase: 10-carry-in-closure-before-the-freeze
    provides: the wave bases and the Gate-growth ledger of PIN-IMPACT.md (plans 10-01, 10-05, 10-08, 10-13), the digest sweep (10-02), the mutant driver (10-01 to 10-12), the PHASE10_TESTS inventory block and the preflight (10-01), the composite guest and format tests (10-14, 10-15), the freeze-ready spec docs (10-16)
provides:
  - scripts/qualify-phase10.py, a stdlib-only thin-delta native qualifier with --native, --rehearsal and --verify, eight stages with limits derived from measured seconds, and one private record under build/gate/10-qualification-private
  - scripts/tests/test_phase10_qualification.py and CTest harness.phase10-qualification (22 hermetic cases, about 5 s natively, no build and no nested CTest)
  - the first rehearsal of the qualifier on the pre-audit head (PIN-IMPACT.md section 14) and the measured Phase 10 gate-growth total with its cold-run projection (section 6 total row and 6.1)
  - deferred-items.md D-10-17-01 (the 99-attempt cap of the Phase 6 cold-boot tests)
affects: [10-18, 10-19, phase-11]

plan_head_before: 034881f042217a9350a65f06a495af9b18901e00
plan_head_after: 5142246743613d96620e4cd94be5a29c8892d410

actuals:
  tokens: 28377
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns:
    - "a qualifier binds only the delta of its phase: the files changed since a literal 40-hex base plus its own tools, with planning and CI-ledger metadata excluded by rule from both the bound inputs and the dirty check"
    - "every stage limit is a data row carrying its measured seconds, the record repeats both, and a hermetic test fails any limit under twice its measurement"
    - "qualifier tests run in scratch git repositories with stub stage tables, plus one control that starts the real read-only oracle and scanner commands, so a stage that cannot start is not hidden by stubs"
    - "a seeded-defect run over a scratch copy of the qualifier (25 one-line edits) proves each refusal test can fail"

key-files:
  created:
    - scripts/qualify-phase10.py
    - scripts/tests/test_phase10_qualification.py
    - .planning/phases/10-carry-in-closure-before-the-freeze/deferred-items.md
  modified:
    - runner/CMakeLists.txt
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase10-regexes.txt
    - .planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md

key-decisions:
  - "The bound inputs are the files changed since the Wave 0 base (40-hex literal read from the '- Wave 0 base:' line, never HEAD) plus 14 named tools; .planning/ and docs/ci-reviews/LOG.md are excluded from the bound inputs and from the clean-tree check because they cannot change a build"
  - "A final record requires the Qualification base line and refuses any non-planning path changed between it and HEAD; a rehearsal record needs only the Wave 0 base and is refused as final by --verify"
  - "The tested artifact identity is the build id embedded in the artifacts of the stages that built them (core, runner, runner test twin from the gate stage, ASan core from the asan stage); no stage means no identity entry"
  - "The digest sweep stage pins the 1599 shared lines of the 21 shipped ids, so a silently smaller sweep refuses; the scanner selftest must run at least the 138 cases of the final CI scanner"
  - "The qualifier writes its own JUnit under the private root; scripts/gate.sh, ci.yml and check-ci-yml.sh stay untouched (final for Phase 10) and no hosted step is added"
  - "The exhausted cold-boot attempt directories were archived by a move, not deleted, after the permission system refused the bulk prune of closed-phase run directories; the broader prune was not performed"

requirements-completed: [GATE-01, GATE-02, VER-06, VID-07, CORE-06, RUN-07]

coverage:
  - id: D1
    description: "scripts/qualify-phase10.py is a thin-delta qualifier with --native, --rehearsal and --verify, a stage table of eight stages with measured limits, and one private record"
    requirement: "GATE-01"
    verification:
      - kind: unit
        ref: "ctest -R '^harness\\.phase10-qualification$' (22 hermetic cases pass, 5.2 s native); python3 -B scripts/qualify-phase10.py --help lists --native, --rehearsal and --verify"
        status: pass
    human_judgment: false
  - id: D2
    description: "--verify refuses a changed source input, a missing stage, a failed or over-limit stage, a rehearsal record presented as final, a record outside the private root, a changed tier or claim, and a malformed or non-40-hex base"
    requirement: "GATE-02"
    verification:
      - kind: unit
        ref: "scripts/tests/test_phase10_qualification.py cases verify-refuses-* and bases-refuse-head-short-duplicate-missing; 25 seeded qualifier defects each fail at least one case"
        status: pass
    human_judgment: false
  - id: D3
    description: "The new CTest name carries the label phase10 and has its PHASE10_TESTS entry and preflight count row"
    requirement: "GATE-01"
    verification:
      - kind: integration
        ref: "ctest -N -R '^harness\\.phase10-qualification$' prints 1 test; sh scripts/preflight-phase10.sh: 46 regex rows 0 mismatches, 30 tests carry the label phase10 matching the PHASE10_TESTS block"
        status: pass
    human_judgment: false
  - id: D4
    description: "A rehearsal on the pre-audit head passes all eight stages inside their limits and its record verifies"
    requirement: "VER-06"
    verification:
      - kind: integration
        ref: "python3 -B scripts/qualify-phase10.py --native --rehearsal (exit 0, 506 s) and --verify RECORD --rehearsal (exit 0); the same record without --rehearsal exits 1"
        status: pass
    human_judgment: false
  - id: D5
    description: "The measured Phase 10 gate-growth total, the cold-run projection and the warning against the 120 s margin are recorded before the cold run"
    requirement: "VID-07"
    verification:
      - kind: other
        ref: "PIN-IMPACT.md section 6 total row and 6.1, section 14"
        status: pass
    human_judgment: true
    rationale: "The projected hosted margin (165 to 204 s primary, 94 s serialized ceiling) is an estimate from recorded hosted ratios; whether the 94 s ceiling warrants action before the cold run of plan 10-19 is an orchestrator judgment, and the cold run itself is the only measurement"

duration: 44min
completed: 2026-11-25
status: complete
---

# Phase 10 Plan 17: Phase 10 native qualifier, hermetic tests and rehearsal Summary

**A stdlib-only Phase 10 qualifier that binds the Wave 0 delta, the PHASE10_TESTS inventory and the tested build ids, runs eight stages with limits derived from measured seconds (gate, JUnit, ASan selection, 25 mutants, digest sweep), refuses a rehearsal as final, and passed a verified 506 s rehearsal on the pre-audit head.**

## Performance

- **Duration:** 44 min (2026-11-25T03:41:37Z to about 04:26Z), of which about 24 min were stage measurements and the two rehearsal attempts
- **Tasks:** 3
- **Commits:** 3 task commits
- **Files modified:** 7 (3 created, 4 modified)

## Accomplishments

- `scripts/qualify-phase10.py` (966 lines, stdlib only) copies the Refusal class, the snapshot, the private-root handling and the one-writer record of `qualify-phase09.py` and the bounded process-group runner of `qualify-rdp-vi.py`; nothing of the closed Phase 7, 8 or 9 qualifiers is edited or imported. It binds 60 inputs (changed since the Wave 0 base plus 14 tools), the four wave bases as 40-hex literals, the 30 `PHASE10_TESTS` names against `ctest -N -L phase10`, and the embedded build ids of the artifacts the stages built. Tier recorded: "unit-tested, authored scope", with hardware accuracy and oracle comparison both false.
- Stage table, cheap first, each command with a limit of at least twice its measured seconds (the measurements sit in the record and a hermetic test enforces the factor): ci-scanner (scanner and its 138-case selftest), inventory, oracle (`--expect-blocked` exit 0, `--require-approved` exit 2), gate (`scripts/gate.sh`), phase10-junit (30 testcases equal the block), asan (configure, build, `ctest --preset asan -L phase10`, 30 testcases equal the block), mutants (rows equal the driver's `--list`: 25 KILLED, 2 controls as expected) and sweep (1599 shared lines, 0 differences, 0 head-only lines against the Wave 0 literal).
- `harness.phase10-qualification` (`LABELS "harness;qualification;phase10"`, `TIMEOUT 120`) runs 22 hermetic cases in scratch git repositories with stub stage tables: base refusals (HEAD, short hash, ref name, upper case, 41 hex, unknown commit, non-ancestor, duplicate, missing), dirty tree, snapshot content, accepted native and verify, changed input, changed input after the qualification base, rehearsal as final, outside the private root, failed, over-limit and missing stage, changed bindings, private-root safety, a deadline control with a writing descendant, tier and claims, stage table shape, synthetic JUnit count rule, mutant, gate, sweep, scanner and inventory output rules, the real oracle and scanner commands starting, the CLI, and an undefined-name check. 25 seeded one-line defects of the qualifier each fail at least one case. Native time 5.2 s (6.4 s at host load 13); the case inventory, label count (30) and regex row are in place.
- Rehearsal on `da6af0d` (host load 12.0 to 13.6): all eight stages ok, 506.35 s: ci-scanner 1.98, inventory 0.62, oracle 0.14, gate 155.88 (`ctest --preset gate` wall 103.34 s, 254 of 254), phase10-junit 9.90, asan 175.12 (selection 163.52 s wall), mutants 110.72, sweep 51.54. `--verify RECORD --rehearsal` exits 0; the same record without `--rehearsal` exits 1.
- Gate-growth total recorded before the cold run (PIN-IMPACT.md section 6 total row and 6.1): the 30 names sum 30.61 s native (18.83 s mean of the cost history), the gate ctest wall rose 3.6 s over the Wave 0 baseline (103.34 against 99.7 s at comparable load), the projected cold gate-macos is 696 to 735 s (margin 204 to 165 s) and a serialized-ceiling cross-check of 806 s (margin 94 s) is recorded as a warning for plans 10-18 and 10-19. No offset is claimed; each wave row keeps its G4-exception reason and the reversal rule is restated.

## Task Commits

1. **Task 1: qualifier tracer** - `48c928e` (feat). Snapshot, oracle stage, record and verify, 17 hermetic cases, CTest, inventory and regex row. The tracer verify passed twice and a real `--native --rehearsal` smoke run with the oracle stage verified, so expansion continued.
2. **Task 2: remaining stages with measured limits** - `da6af0d` (feat). Stage table, output rules, 22 cases, limits from a one-run measurement of every stage command on `48c928e`.
3. **Task 3: rehearsal and ledger** - `5142246` (docs). PIN-IMPACT.md sections 6, 6.1 and 14 and `deferred-items.md`.

**Plan metadata:** the SUMMARY commit and the STATE, ROADMAP and REQUIREMENTS progress commit follow this file.

## Files Created/Modified

- `scripts/qualify-phase10.py` - the qualifier (`--native`, `--rehearsal`, `--verify`).
- `scripts/tests/test_phase10_qualification.py` - 22 hermetic cases.
- `runner/CMakeLists.txt` - registers `harness.phase10-qualification`.
- `scripts/check-test-inventory.sh` - the 30th `PHASE10_TESTS` name.
- `scripts/preflight-phase10-regexes.txt` - the count row for the new name.
- `.planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md` - the ledger total row, section 6.1 and section 14.
- `.planning/phases/10-carry-in-closure-before-the-freeze/deferred-items.md` - D-10-17-01.

## Decisions Made

See key-decisions. For plans 10-18 and 10-19: 10-19 runs `python3 -B scripts/qualify-phase10.py --native` and then `--verify` on the newest `record.json`; a final run requires the `- Qualification base:` line that 10-18 records and refuses any non-planning path changed since it, so the audit must record the base after its last source edit.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] The first rehearsal gate failed on an exhausted Phase 6 attempt counter, and the prune was only done non-destructively**
- **Found during:** Task 3 (rehearsal, first attempt)
- **Issue:** `integration.cold-hello` printed `not ok cold startup evidence directory prepared`: `make_evidence_dir` in `runner/tests/test_cold_boot.c` creates `build/gate/06-05-cold-hello-attempt-NN` for NN 01 to 99 and all 99 existed (the stage-measurement gate run took the 99th); `06-05-cold-startup-attempt-*` stood at 94. The plan's step "prune closed-phase run directories under build/gate" was exactly what the gate needed. The first prune command (the children of `08-04-evidence` and `09-05-evidence`, about 5,400 entries) was refused by the permission system and was not retried in any other form.
- **Fix:** moved, not deleted, the 193 attempt directories (about 1.5 MB) of the two cold tests into `build/gate/archived-closed-phase-runs/`, reran `integration.cold-hello` and `integration.cold-startup` (pass), then reran the rehearsal. No test source was edited (a closed Phase 6 test); the cause, the workaround and a suggested owner are in `deferred-items.md` D-10-17-01.
- **Files modified:** none tracked (build tree only)
- **Verification:** the second rehearsal passed all stages; the first attempt left the unreferenced record directory `20261125T040923048566Z-da6af0d6` (no `record.json`)
- **Committed in:** `5142246` (the deferred item and section 14 record it)

---

**Total deviations:** 1 auto-fixed (1 blocking). The broader prune of `08-04-evidence` (3,404 entries, 467 MB) and `09-05-evidence` (2,044 entries) was not performed.
**Impact on plan:** none on the deliverables. The local build tree still holds the two large evidence directories; they cost disk, not correctness.

### Known small inaccuracies (recorded, not fixed here)

- The `mutants` stage derivation string in `scripts/qualify-phase10.py` and the Task 2 commit message say "26 mutants and 2 controls"; the driver lists 25 mutants and 2 controls (27 rows). The check is exact (rows equal `--list`). The file is left as rehearsed so the rehearsal stays the run of the committed code; plan 10-18 can correct the sentence with its other audit edits.
- The ASan build command keeps `--parallel 1` (the Phase 7 and 9 shape); the rehearsal build took 11.39 s with a rebuilt revision annotation, so the choice costs nothing here.

## Issues Encountered

- The first rehearsal attempt failed as described above; the second passed. Host load stayed at 12 to 14 for the whole rehearsal, so every native second in section 14 is an upper bound.
- Hosted CI: origin/main `f57b019` is green (preflight head-ci); this plan's three commits and the earlier waves are unpushed (plan 10-18 pushes), so no hosted run of this head exists yet.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 10-18 can review the qualifier together with the Wave 3 tooling: its record, stage table and the 22 hermetic cases are in place. It should correct the one derivation sentence, record the Qualification base last, push, and compare its first warm gate-macos run with the 696 to 735 s projection and the 94 s warning of PIN-IMPACT.md section 6.1.
- Plan 10-19 runs `python3 -B scripts/qualify-phase10.py --native` once and `--verify` on its record. Before it, the cold-boot attempt counters need a prune or the archive move again if more than about 90 gate runs have happened in the same build tree (D-10-17-01).
- No blocker for the next plan.

## Threat Flags

None. The qualifier only reads the repository, writes under the git-ignored `build/gate/10-qualification-private` (mode 0700, files 0600, symlink and group-readable roots refused: T-10-41) and refuses a rehearsal record as final (hermetic case: T-10-42); it opens no network endpoint and adds no hosted step.

## Known Stubs

None.

---
*Phase: 10-carry-in-closure-before-the-freeze*
*Completed: 2026-11-25*

## Self-Check: PASSED
