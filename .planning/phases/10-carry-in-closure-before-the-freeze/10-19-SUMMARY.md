---
phase: 10-carry-in-closure-before-the-freeze
plan: 19
subsystem: qualification
tags: [final-qualification, tracer-gate, cold-cache-run, gate-01, halted, host-stall, supervisor-wall-backstop]

requires:
  - phase: 10-carry-in-closure-before-the-freeze
    provides: the audited head f081d39 with its green hosted pair, the Qualification base ad39b6be2f803af45af15d4ed511b72a89460339, the qualifier and its rehearsal (10-17), the Gate-growth ledger and the owner-review flags (10-18)
provides:
  - PIN-IMPACT.md section 15 "Qualification (plan 10-19)": the failed first native attempt with stage seconds, the three failed names, the diagnostics and the disposition
  - Task 2 re-verified by the executor against the orchestrator's cold run 45550277510 (both automated verify commands pass with the planned wording) and the cold run's step seconds, cache lines and cache state, kept below for the resuming executor
  - NOT provided: a qualification record, the LOG.md cold-run rows, docs/evidence/phase10-qualification.md and docs/claims/phase10.md (Tasks 1, 3 and 4 are open)
affects: [10-19-resume, phase-11]

plan_head_before: 1f862a0f7a255c5f0dd1dabb488acf44dc0a5259
plan_head_after: 4c27f4c98dae97e5e629dc26345687c635b1142a

actuals:
  tokens: 5000
  tasks: 1
  commits: 1

tech-stack:
  added: []
  patterns:
    - "a failed final-qualification attempt is recorded in full (stage seconds, failed names, observed against expected, diagnostics) in the commit that stops the plan, and a later passing record is stated beside it, never instead of it"
    - "copy build/gate/Testing/Temporary/LastTest.log before any further ctest run: it is overwritten, and the private step log keeps only the ctest summary"

key-files:
  created:
    - .planning/phases/10-carry-in-closure-before-the-freeze/10-19-SUMMARY.md
  modified:
    - .planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md

key-decisions:
  - "The plan halts at the tracer gate: the one Phase 10 qualification (python3 -B scripts/qualify-phase10.py --native) was refused at its gate stage, and the plan says a failed stage stops the plan. No source, test, script or pin was edited, the qualification was not repeated, and Tasks 3 and 4 were not started (never expand after a failed tracer)."
  - "The failure is recorded as host state, not as a Phase 10 defect, with the evidence and its limit: the same tree passed 254 of 254 at load 12 to 14 (10-17 rehearsal) and in a clone at load 6 to 10.7 (10-18), then failed three different supervised-run tests in each of two consecutive gate runs (ctest wall 389 s and 213 s against 103 s), every failure a supervisor wall backstop, and the three names of the first run pass alone. This is an inference from timing, not a proven cause."
  - "No retry by the executor: a repeat that passes must be recorded beside this failed attempt, so the choice between a repeat on a quiet host and a numbered fix plan is the orchestrator's."

requirements-completed: []

coverage:
  - id: D1
    description: "The orchestrator's cold-cache hosted gate-macos run 45550277510 on head f081d39 concluded success inside the 900 s limit with a cache miss on the macOS Binutils key, the Binutils build inside the Gate step and the cache saved afterwards"
    requirement: "GATE-01"
    verification:
      - kind: other
        ref: "gh run view 45550277510 --json conclusion,jobs (success); gate-macos steps Restore pinned Binutils=success and Gate=success; log lines 'Cache not found for input keys: binutils-macOS' and 'Cache saved with key: binutils-macOS' (one each); gate-macos 731 s"
        status: pass
    human_judgment: false
  - id: D2
    description: "The final native qualification passes on the audited head and --verify accepts its record"
    requirement: "GATE-01"
    verification:
      - kind: other
        ref: "python3 -B scripts/qualify-phase10.py --native (attempt 1, exit 1: stage gate command 0 exited 1, ctest 251 of 254)"
        status: fail
    human_judgment: true
    rationale: "No qualification record exists; whether the failure is host state or a code defect needs the orchestrator's decision between a repeat on a quiet host and a numbered fix plan"

duration: 21min
completed: 2026-11-25
status: halted
---

# Phase 10 Plan 19: Qualification, orchestrator cold-cache run, evidence and claims Summary

**HALTED at the Task 1 tracer gate: the one final native qualification was refused at its gate stage (ctest 251 of 254, three supervised-run tests ended at their wall backstops while the host ran 2 to 4 times slower than at the rehearsal), a diagnostic gate run on the unchanged tree failed three other supervised-run tests, so no record exists and the cold run, evidence and claims were not written; the orchestrator's cold run 45550277510 is verified (success, gate-macos 731 s of 900 s, cache miss and re-save) and its numbers are kept below.**

## Performance

- **Duration:** 21 min (2026-11-25T05:14:00Z to 05:35:13Z); most of it waiting on the attempt (about 7.5 min) and one diagnostic gate run
- **Tasks:** 1 of 4 attempted (Task 1 failed at its gate), Task 2 verified read-only, Tasks 3 and 4 not started
- **Files:** 2 touched (`PIN-IMPACT.md` +75 lines, this summary); no tracked source file changed
- **New CTest names:** none

## Accomplishments

- **Task 1 (tracer), FAILED.** `sh scripts/preflight-phase10.sh` passed in 41 s; the Qualification base `ad39b6be...` is one 40-hex literal and `git diff --name-only` base to head lists only `.planning/` paths (so every bound source input equals the base's); the tree was clean. `python3 -B scripts/qualify-phase10.py --native` at load 6.2 exited 1 after ci-scanner 1.79 s, inventory 0.58 s, oracle 0.14 s and a gate stage of 441 s that failed on `ctest --preset gate` (251 of 254; wall 389.31 s against 103.34 s in the rehearsal). The failed names are `runner.save-roundtrip`, `runner.state-export` and `controls.real.rdp-drain-cadence`, none a Phase 10 name. No `record.json` was written, so `--verify` could not run. Details and observed-against-expected: PIN-IMPACT.md section 15 (commit `4c27f4c`).
- **Diagnostics (not qualification records).** The three names alone, `-j1`, load 13.6: 3 of 3 pass (0.29, 6.43 and 46.46 s). One more `sh scripts/gate.sh` on the same tree at load 7.7 to 9.3: failed again with three other names (`runner.cpu-diagnostic` "status=child-or-wall-bound", `integration.open-ipl3-hello` case 22 "wall kill after correct guest text takes harness-error precedence", `controls.suite-dropped` "detail=wall-backstop"), ctest wall 213.23 s, test-inventory-selftest 80 s against 23 to 35 s. Every failure in both runs is a supervisor wall limit.
- **Task 2 (orchestrator-only), verified by the executor.** Both automated verify commands of Task 2 pass with the planned wording against `COLD_RUN_ID=45550277510`: conclusion `success`; `Restore pinned Binutils=success` and `Gate=success` on gate-macos; exactly one `Cache not found for input keys: binutils-macOS-ARM64-69d0bfd4...` line and one `Cache saved with key: binutils-macOS-ARM64-69d0bfd4...` line. Nothing was deleted or dispatched by the executor.

## Task Commits

1. **Task 1: final native qualification (failed attempt recorded)** - `4c27f4c` (docs, PIN-IMPACT.md only)

**Plan metadata:** this summary and the STATE session update follow in docs(10-19) commits (planning only). ROADMAP progress was not advanced: the plan is not complete.

## Cold run 45550277510 as read by the executor (for Task 3 when the plan resumes)

Read-only `gh run view` and `gh cache list`, taken at 2026-11-25 05:2x UTC.

| Item | Value |
|------|-------|
| run | workflow `ci`, event `workflow_dispatch`, head `f081d392763ad384c3d50ae1b24b709d5af3b7d4`, created 04:57:47Z, updated 05:10:04Z, conclusion success |
| gate-macos job | 04:57:52Z to 05:10:03Z = **731 s**, inside 900 s by 169 s (18.8 percent of the limit); the old 600 s limit would have been exceeded by 131 s |
| gate-macos steps (s) | Set up job 2, Checkout 6, Restore pinned Binutils 1 (miss), Gate 653, audio qualification records 40, Phase 9 qualification records 5, emits 1+0+0+1, rights scan 0, PII scans 3+4+0+0, upload 2, Post Restore pinned Binutils 3 (save), Complete job 4 |
| Gate step inside (`[gate]` lines) | sh-n 0, lint-selftest 12, **fixture-qualification 219 (the Binutils build)**, configure 4, build 31, boot-evidence-selftest 31, test-inventory-selftest 57, test-inventory 3, **ctest 265**, core-lint 28, total 653 |
| gate-linux job | 04:57:50Z to 05:02:00Z = 250 s; Restore pinned Binutils 3 s (hit), Gate 200 s, audio records 28 s |
| cache lines | `Cache not found for input keys: binutils-macOS-ARM64-69d0bfd4279dc48f436582c76ec0e08c98eef6e7e27d969d562c2db17485b384` (restore step) and `Cache saved with key: binutils-macOS-ARM64-69d0bfd4279dc48f436582c76ec0e08c98eef6e7e27d969d562c2db17485b384` (post step); the observed words equal the plan's, so no match was loosened |
| cache state after (`gh cache list`) | macOS entry recreated: id 11078086756, 5.03 MiB, created 05:09:56Z; Linux entry id 10796791825, 31.10 MiB, created 2026-11-19, last accessed 04:57:58Z, untouched. Assumption A5 (a timed-out job saves no cache) was not exercised because the job did not time out |
| acceptance check | `git diff --name-only <base> f081d39` lists only `.planning/` paths and `docs/ci-reviews/LOG.md` (the base is a descendant of the run head, the difference is the two recording commits) |

Reading against the earlier numbers (for the LOG.md finding): the measured cold 731 s is 160 s above the plan 10-18 projection of 571 s (warm 385 s plus 186 s), 58 s above the F-111 cold projection of 673 s, at the top of the 10-17 primary projection of 696 to 735 s and inside its cross-check of 709 to 806 s. Against the warm run 45548987145 (385 s) the cold run adds 346 s: fixture-qualification 219 s against about 1 to 2 s warm (+217 s, the Binutils build, close to the 179 to 186 s that F-096, F-111 and F-114 measured), ctest 265 s against 196 s (+69 s on the same 254 names, which is runner spread beyond F-119's 52 s and not cold-cache cost), and smaller rises in test-inventory-selftest, build, core-lint and the audio records. The G4 reversal rule (cold above 900 s or cancelled) is not triggered; the margin of 169 s is one sample.

## Files Created/Modified

- `.planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md` - section 15, the failed attempt and its diagnostics
- `.planning/phases/10-carry-in-closure-before-the-freeze/10-19-SUMMARY.md` - this halted summary

## Decisions Made

See `key-decisions` above. In short: halt at the tracer gate, record the failure as host state with its evidence limits, and leave the repeat-or-fix decision to the orchestrator.

## Deviations from Plan

**1. [Rule 1 surfaced - tracer gate] Task 1 failed; the plan halts**
- **Found during:** Task 1 (`python3 -B scripts/qualify-phase10.py --native`)
- **Issue:** stage `gate` exited 1: `ctest --preset gate` 251 of 254, three supervised-run tests ended at supervisor wall backstops under a slow host.
- **Fix:** none attempted (the dispatch and the plan: a failed stage stops the plan and is returned for the orchestrator's decision; do not patch around it). No file other than PIN-IMPACT.md and this summary was written.
- **Verification:** the three names pass alone; a second gate run failed three other supervised-run tests; both are recorded in PIN-IMPACT.md section 15.
- **Commit:** `4c27f4c`

**2. [Diagnostic, not in the plan] Two extra local runs after the failure**
- An isolated `ctest -R` of the three failed names and one `sh scripts/gate.sh`; neither is a qualification record, neither changed a tracked file, both consumed cold-boot attempt numbers (counters now 4 and 4 of 99) and the first overwrote the attempt's `LastTest.log` before the third failure's output was read (recorded as a lesson in section 15).

**Total deviations:** 2 (1 gate stop, 1 diagnostic addition). **Impact on plan:** Tasks 1, 3 and 4 remain open; no source or evidence moved.

## Issues Encountered

- The first qualification attempt and the diagnostic gate run both failed on supervisor wall limits while the host was slow (ctest wall 389 s and 213 s against 103 s; a bursty slowdown, other long-running processes holding about three cores and a Docker virtual machine holding about 10.8 GB; free memory 59 percent with 4.6 GB of swap in use when checked). The cause of the slowdown was not identified.
- `LastTest.log` of the failed attempt was lost to a later ctest run; the step log keeps only the ctest summary.

## Authentication Gates

None. `gh` was already authenticated; only read-only run and cache queries were made.

## User Setup Required

None - no external service configuration required.

## Known Stubs

None.

## Threat Flags

None new. T-10-46 (cold-cache timeout loop): the measured cold run finished inside 900 s and the macOS cache entry was recreated, so the loop did not occur. T-10-47 (disclosure): no evidence document was written; the PIN-IMPACT.md addition holds no workstation path and the tree PII scan reported 0 findings.

## Next Phase Readiness (resume instructions for the orchestrator)

Phase 10 is not ready: the final qualification is open. Decision needed: **repeat Task 1 on a quiet host, or insert a numbered fix plan.** The evidence favours a repeat (the source is unchanged from a tree that passed 254 of 254 twice, and the failures move between names with the host speed), but a repeat must be gated by a plain `sh scripts/gate.sh` passing first, and the failed attempt stays in the evidence.

1. **Before a repeat:** make the host quiet (the gate wall was 103 s at the rehearsal, 123 s in the 10-18 clone; 213 to 389 s now): quit memory-heavy applications (the Docker virtual machine held about 10.8 GB), check `memory_pressure` and `uptime`, then run `sh scripts/gate.sh` alone and require `[gate] PASS` before `python3 -B scripts/qualify-phase10.py --native`. Copy `build/gate/Testing/Temporary/LastTest.log` after any failure before running ctest again. Cold-boot counters stand at 4 and 4 of 99.
2. **If a quiet host still fails** the same supervised-run tests, that is a fix-plan matter: the failing assertions are Phase 8 and 9 closed tests (`runner/tests/test_save_roundtrip.c`, `test_state_runner.c`, `controls` suite) whose wall limits sit in bound or frozen source, so any edit moves pins; do not widen a limit ad hoc.
3. **Resume order after a passing record:** `--verify` the record; Task 2 is already satisfied (cold run 45550277510 on `f081d39`); Task 3 records the cold run from the table above (`sh scripts/ci-review.sh 45550277510 --append docs/ci-reviews/LOG.md --finding "F-124|..."`, next free id F-124; seconds 731 against 900 compared as integers; cache state; PIN-IMPACT.md "Cold run" and the Gate-growth ledger cold row); Task 4 writes `docs/evidence/phase10-qualification.md` and `docs/claims/phase10.md`.
4. **Task 4 notes already worked out.** Counting units (Phase 9 precedent: registered CTest names that passed in the retained gate JUnit, or one accepted authored ROM row): VID-07 `unit.core.rdp.depth-composite-accepted`, `-wrong-depth`, `-sidecar` on `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2`; CORE-06 `unit.core.state.import-v26-roundtrip`, `-hostile`, `unit.core.digest.v26-completeness`, `-strip-differential` on the same stem; RUN-07 the `phase10-composite-s32k` row of `runner.composite-two-run` (profile `...-si-v2-s32k`, to be confirmed in `fixtures/manifest.json`); VER-06 the nine `unit.core.vg.*` names, which run on six descriptor-10 ids (`rdp-depth-v7` for decal-equality, primitive-z-saturation, delta-ceiling and z-exponent-boundaries; `rdp-vi-v8` for ntsc-clock and the first half of rgba5551-alpha-clear; `rdp-vi-filter-v9` for serrate-filter-parity and the second half of rgba5551-alpha-clear; `rdp-texture-v4` for perspective-floor; `rdp-triangle-v3` and `rdp-blender-v6` for shade-floor-bayer), so bind each name to one id per block and list the other ids as exercised and not claimed, as the Phase 9 stem rule does. `manifest_digest` is the SHA-256 of the manifest at the qualified commit (the manifest moved once, in plan 10-14); the Phase 9 records stay untouched (`git diff --quiet -- docs/claims/phase09.md docs/evidence/phase09-qualification.md`). No tag is created (D6), so the evidence names the commit by its 40-hex id.
5. **Owner-review items the evidence document must carry in "Recorded exceptions for owner review"** (all already recorded elsewhere; none is an owner approval): T-10-49 (the Phase 9 claim-binding comparator in `scripts/tests/test_phase09_qualification.py` changed from the live whole-file manifest digest to the pinned Phase 9 manifest identity plus a per-row view; basis delegated autonomy plus ROADMAP SC5 and the 2026-11-23 per-row approval, not an owner approval of this comparator change; reversible; no Phase 9 claim content, tier or authority moved; PIN-IMPACT.md section 9); the G4 gate-growth exception with the measured numbers (cold 731 s, margin 169 s; ledger rows); the G5 plan-count exception (19 plans); the two accepted wave-3 deviations (one literal of `runner/tests/test_state_runner.c` v2 to v3; the weaker wrong-ROM control on the guest read-back record); the GATE-01 guard-removal decline (the `rsp-no-hle` guard stays: its removal needs `scripts/qualify-audio.py` and `docs/spec/audio-cases.json` edits frozen by F-106); the deferred carry-ins now at the G3 limit of five (W2-01, W2-02, DEF-W1-01, W1-03, D-10-17-01) plus the Phase 9 triage deferrals (T1, T2, T4 to T8); and that `PINNED_REQUEST_SHA256` was rebound once by the orchestrator (`a4bb7774...` to `e131cbe7...`) and has not moved since (re-read at this plan: `e131cbe7e255...`, `git diff` of the script from the Wave 3 base is empty). VID-04 stays deferred and unclaimed; D5, D6 and D7 stops intact.
6. **Unpushed:** local `main` is `origin/main` (`f081d39`) plus the 10-18 recording and summary commits and this plan's commits; nothing was pushed by this plan.
7. `requirements.ready-ids` keeps all six requirements unchecked while this plan is open; this summary declares none complete.

---
*Phase: 10-carry-in-closure-before-the-freeze*
*Halted: 2026-11-25*

## Self-Check: PASSED

`PIN-IMPACT.md` section 15 exists (`grep -c '^## 15. Qualification (plan 10-19)'` is 1); `4c27f4c` is an ancestor of HEAD; `git rev-list --count 1f862a0f7a255c5f0dd1dabb488acf44dc0a5259..4c27f4c98dae97e5e629dc26345687c635b1142a` is 1; the tree PII scan reported 0 findings; `git status --short` was empty before the attempt and after the diagnostics (no tracked file other than the two planning files changed); `PINNED_REQUEST_SHA256` is `e131cbe7...` and unchanged. The self-check covers what was done; the plan itself is not complete (Tasks 1, 3 and 4 open), which this summary states by `status: halted`.
