---
phase: 12-minimal-host-playable-homebrew-and-final-bind
plan: 12
subsystem: review
tags: [review, scratch-clone, host, pump, mutants, sdl-lane, push, hosted-ci, gate-growth, bind-base]

requires:
  - phase: 12-06
    provides: the Host base 0836e40, the review-clone procedure (T-05) and the TITLE ledger row
  - phase: 12-07
    provides: the pure-C pump, check-host.sh, the host include and isolation guards, DR-35
  - phase: 12-08
    provides: the mapping table, the strict scripted source, the frame helpers, host.pump.title-two-speeds and scripts/p12-mutants.py (DR-36)
  - phase: 12-09
    provides: the SDL3 audit, the optional shell, the local sdl lane and repo.host-policy (DR-37)
  - phase: 12-10
    provides: the API-07 evaluation, repo.api07-evaluation, the host-found defect triage and the line 12-11 trigger: no
  - phase: 12-11
    provides: the untriggered amendment slot (one-line SUMMARY)
provides:
  - 12-REVIEW.md section "HOST" (Task 1 clone gate and full mutant run, Task 2 checks (a) to (l), decision read and findings HR-01 to HR-06, Task 3 push and hosted pair)
  - PIN-IMPACT "Host steps" mutant table (12 KILLED, both controls as expected) and the filled HOST row of the Gate-growth ledger
  - origin/main de21fcf7d5c30d8de59bde3d49143ab5dbb75faf (fast-forward from 884153a, no tag), hosted run 45904452436 green on both jobs
  - docs/ci-reviews/LOG.md Runs row 45904452436 and finding F-138
  - PIN-IMPACT "Wave bases" line "- Bind base: 49d41ac9e1966bd9cdb9e953b61d0355a1178554"
affects: [12-13, 12-14, 12-15, 12-16, 12-17, 12-18]

actuals:
  tokens: 15700
  tasks: 3
  commits: 4
plan_head_before: 35a2736f1d82b4ca746eb947408cded3722385f9
plan_head_after: 0d0d4035c91a69cc9bb7c454a49bed95b7b73900

tech-stack:
  added: []
  patterns:
    - "the full mutant driver runs in the review clone, whose own repository holds the throwaway worktree, so the shared repository is never touched"
    - "an independent spot check of a mutant kill: a git archive copy with the edit applied by sed, built and run through check-host.sh, reading the case's own failed assertion"
    - "guard selftests run in the act Linux image already on disk, with the clone mounted read-only and no network, before the first hosted Linux run"

key-files:
  created:
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-12-SUMMARY.md
  modified:
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-REVIEW.md
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/PIN-IMPACT.md
    - docs/ci-reviews/LOG.md

key-decisions:
  - "Review result: zero blocking findings, two warnings (HR-04 gate growth under the 120 s warning level, HR-06 the H-03 sentence missing from the 12-15 plan text) and four info rows (HR-01, HR-02, HR-03, HR-05)"
  - "The full p12-mutants.py --run executed in the review clone (no classifier denial): 12 of 12 mutants KILLED by their named tests, P12-C-EQUIV SURVIVED, P12-C-INVALID INVALID, 37 s"
  - "Pushed de21fcf7d5c30d8de59bde3d49143ab5dbb75faf to main as a fast-forward from 884153a (29 commits), no tag; hosted run 45904452436 green: gate-macos 572 s (ctest 312 s), gate-linux 306 s (ctest 175 s), 351 of 351 on both"
  - "Cold gate-macos projections (not measurements): 762 s (warm plus the F-136 cold cost), 816 s and 826 s (additive); none above 900 s, so the plan's G4 trigger is not reached and no reversal was applied before or after the push; the margins of 84 s and 74 s are under the 120 s warning level (HR-04)"
  - "Bind base 49d41ac9e1966bd9cdb9e953b61d0355a1178554, recorded by a commit that changes only PIN-IMPACT.md"

patterns-established:
  - "A review reads the hosted growth of a wave from the per-name seconds of the job log when the warm job delta is inside the runner spread"

requirements-completed: []

coverage:
  - id: D1
    description: "The HOST head passes the full gate in an isolated clone and the full local mutant run kills every pump and title mutant with both controls as expected"
    requirement: "API-05"
    verification:
      - kind: integration
        ref: "build/12-review/host/src: sh scripts/gate.sh PASS 351/351, total 193 s; python3 -B scripts/p12-mutants.py --run exit 0, 12 KILLED, EQUIV SURVIVED, INVALID INVALID"
        status: pass
    human_judgment: false
  - id: D2
    description: "Checks (a) to (l): installed API only, clock rule, two-speed equality, DR-12 table, SDL out of the gate, policy and audit order and an SDL lane PASS, API-07 pin, 12-11 outcome, digest sweep, authority blocked, PII range, COST"
    requirement: "API-06"
    verification:
      - kind: integration
        ref: "ctest -R '^(host\\.pump\\.|repo\\.host-|repo\\.api07-evaluation$|repo\\.p12-mutant-anchors$)' 16 of 16; own reader of the pump lines runs=4 checks=77 failures=0; digest sweep 1599 shared lines 0 differences; check-host.sh sdl [host-sdl] PASS"
        status: pass
    human_judgment: false
  - id: D3
    description: "Reviewed head pushed with no tag, hosted pair green, HOST ledger row with the hosted delta and the margin to 900 s, Bind base recorded"
    requirement: "API-07"
    verification:
      - kind: integration
        ref: "gh run 45904452436 completed success for origin/main de21fcf, no tag (Task 3 verify 1); Bind base verify (one 40-hex line, recording commit changes only PIN-IMPACT.md) exit 0"
        status: pass
    human_judgment: false

duration: 30min
completed: 2026-11-26
status: complete
---

# Phase 12 Plan 12: HOST Review, Full Mutant Run, Push and Bind Base Summary

**Independent scratch-clone review of the HOST wave 12-07 to 12-11 (gate 351/351, the first full p12 mutant run with 12 of 12 killed and both controls as expected, checks (a) to (l) with an own reader of the two-speed pump lines, 0 digest differences, an SDL lane PASS) with no blocking finding, then a routine fast-forward push of de21fcf whose hosted pair is green (gate-macos 572 s, gate-linux 306 s, first hosted run of the 15 HOST names), the HOST ledger row at projected cold margins of 74 to 138 s (none above 900 s; warning HR-04) and the Bind base 49d41ac.**

## Performance

- **Duration:** about 30 min
- **Started:** 2026-11-26T22:07:01Z
- **Completed:** 2026-11-26T22:37:00Z
- **Tasks:** 3 of 3
- **Files:** 1 created (this record), 3 modified (records only: no production, test, fixture, CMake or script file)

## Accomplishments

- Task 1 (tracer): clone `build/12-review/host/src` detached at `35a2736` with a copied `build/tools/install`, the public fetch URL and a disabled push URL (T-05 applied from the start); git archive digests equal; `sh scripts/gate.sh` PASS 351/351 in 193 s (ctest 111 s) at load 6 to 16; 22 `phase12` names. `python3 -B scripts/p12-mutants.py --run` ran in the clone (the first execution of the driver's worktree, revert and RESULTS code): exit 0 in 37 s, 12 KILLED by their named tests, P12-C-EQUIV SURVIVED, P12-C-INVALID INVALID; worktree removed and the clone clean afterwards. An independent sed-applied copy of P12-P04 and P12-P07 failed at the cases' own assertions (`t64h_tests.c:780`, `:1215`). The tracer gate passed end to end before Task 2.
- Task 2: checks (a) installed API only (include guard 16 seeds plus 7 controls, isolation 14 seeds, no source-tree path in the host build), (b) clock rule (0 time, sleep, thread or file calls in `host/pump`; origin-relative ns/16 bound read once per step), (c) pump equality (comparator lines plus an own reader, 77 checks, both scripts at 3 and 240 Hz, the two queueing extremes), (d) the DR-12 table row by row, (e) no SDL name or CI, gate or root CMake change, (f) policy 30 seeds over 12 rules, the audit before `host/sdl`, the lane PASS on SDL 3.4.10, (g) API-07 pin and no adapter code, (h) 15 triage rows 13 accept / 2 document / 0 amend and 12-11 not triggered, (i) digest sweep 0 differences, (j) authority blocked, (k) PII range clean, (l) COST and TIMEOUT on all 22 names. Guard selftests also passed in the act Linux image. Decision table for the 13 HOST-wave CONTEXT decisions and findings HR-01 to HR-06.
- Task 3: shared-tree gate PASS (148 s), full preflight PASS (65 s), PII and rights scans clean; push `de21fcf` (fast-forward, no tag); hosted run 45904452436 green on both jobs; LOG Runs row and F-138; HOST ledger row; Bind base.

## Task Commits

1. **Task 1: scratch-clone gate and full mutant run** - `7985d49` (docs, `[skip ci]`)
2. **Task 2: checks (a) to (l), decision read and findings** - `de21fcf` (docs; the pushed head)
3. **Task 3: hosted pair, F-138, HOST ledger row, review Task 3** - `49d41ac` (docs, `[skip ci]`); **Bind base** - `0d0d403` (docs, PIN-IMPACT.md only, `[skip ci]`)

**Plan metadata:** the commit of this summary with STATE and ROADMAP follows.

`commits: 4` is `git rev-list --count 35a2736f1d82b4ca746eb947408cded3722385f9..0d0d4035c91a69cc9bb7c454a49bed95b7b73900`, read from the persisted ledger file of the plan.

## Findings (12-REVIEW.md "HOST")

| id | severity | finding (short) | disposition |
|----|----------|-----------------|-------------|
| HR-01 | info | the pump latches the reserved, unreachable `HALT` as terminal; the header comment and DR-35 name only two reasons | accepted; wording candidate for a later plan that edits `t64h_pump.h` |
| HR-02 | info | `check-host-title.py` does not require both scripts to be present | accepted; a set check is a candidate for a later comparator edit; 12-17 should read both report lines |
| HR-03 | info | a QUEUE_FULL deferral queues the event at the reached tick, after same-tick events already queued; model-queue only, never reached by the title | accepted; pump equality claim scoped to the title |
| HR-04 | warning | hosted HOST cost 45.60 s macOS; additive cold projections 816 s and 826 s, margins 84 s and 74 s (under 120 s, not above 900 s); about 842 s with the names still to come | not blocking; recommended new numbered plan 12-19 (appended after 12-18) to move `installed.consumer.title-persist` and the two host selftests to a local lane; its order is an orchestrator decision |
| HR-05 | info | first Linux build and run of `host/` and first hosted run of the host guards at this push | closed by the green hosted pair (no mismatch) |
| HR-06 | warning | the 12-15 plan text does not carry the H-03 sentence for `docs/spec/api.md` 8.1 | carried: the orchestrator adds it to 12-15 before 12-15 runs; 12-18 checks it |

## Deviations from Plan

### Auto-fixed Issues

None (the reviewer fixes nothing).

### Other deviations (recorded for transparency)

- **The full mutant run was not denied.** The orchestrator's fallback (an emulated copy-based run, or the orchestrator running the real driver) was not needed: `python3 -B scripts/p12-mutants.py --run` ran in the review clone, whose own repository received the throwaway worktree. No git operation of the driver touched the shared repository.
- **Reversal-rule wording.** The ledger's reversal rule names a margin under 120 s as a warning that leads to a new numbered plan, while this plan's text and must-have stop only for a projection above 900 s. No projection is above 900 s, so the plan continued to the Bind base; the warning branch is reported as HR-04 with a recommended plan and no plan file was written (an orchestrator decision).
- **HOST-wave name count.** The ledger HOST row listed 14 names; the hosted growth was measured for 15 (the 14 plus `repo.api07-evaluation` of 12-10, which is registered in `tools/hostcheck/CMakeLists.txt`); recorded in the row.
- **Local Linux check before the hosted run.** Beyond the plan, the four host guard selftests and two scans ran in the act image already on disk (AUTONOMY section 2, local first); nothing was installed or pulled.
- **Commit messages.** Tasks 1 and 3 and the Bind base carry `[skip ci]`; the Task 2 commit, the pushed head, does not.
- `requirements-completed` is empty on purpose: API-07 was completed by 12-10; API-05 and API-06 also need the owner checklist and the final audit (12-18).

## Authentication Gates

None.

## Issues Encountered

- Unrelated host load (a virtual machine at about 2.6 cores) took the 1-minute load to 16 during the scratch gate; the gate passed with no wall backstop and no limit widened.
- `gh run view --log` refuses while any job of a run is still running; the per-job logs were read through `gh api .../actions/jobs/ID/logs` with escape sequences removed.

## Verification Results

| Check | Result |
|-------|--------|
| clone `sh scripts/gate.sh` | PASS 351/351, total 193 s, ctest 111 s |
| clone `python3 -B scripts/p12-mutants.py --run` | exit 0, 37 s, 12 KILLED, EQUIV SURVIVED, INVALID INVALID |
| Task 2 verify 1 (clone) | ctest 16 of 16; host-includes, host-policy findings=0; api07-evaluation findings=0 |
| Task 2 verify 2 (base idiom, digest sweep) | `sweep: 1599 shared lines, 0 differences, 0 head-only lines`, 47 s |
| clone `sh scripts/check-host.sh sdl` | `[host-sdl] PASS` (SDL 3.4.10, dummy drivers) |
| shared-tree `sh scripts/gate.sh` at `de21fcf` | PASS 351/351, total 148 s, ctest 90 s |
| `sh scripts/preflight-phase12.sh` (full) | PASS, 65 s |
| `pii-scan --tree`, `--range origin/main..HEAD`; `rights-scan --tree --range origin/main..HEAD` | 0, 0 (29 commits), 0 findings |
| Task 3 verify 1 (hosted run of origin/main, no tag) | `completed success` for `de21fcf7d5c30d8de59bde3d49143ab5dbb75faf`, no tag |
| Task 3 verify 2 (Bind base) | one 40-hex line, existing commit, recording commit changes only PIN-IMPACT.md |
| hosted run 45904452436 | gate-macos 572 s (Gate 504 s, ctest 312 s), gate-linux 306 s (Gate 252 s, ctest 175 s), 351 of 351 on both; HOST names 45.60 s macOS, 23.91 s Linux |

## Known Stubs

None.

## Threat Flags

None. T-12-39 is mitigated by the full preflight pii step, `pii-scan --tree` and `--range` and the rights scan before the push (raw logs under `build/`); T-12-40 by the full mutant run in the clone (12 kills) and the independent spot check; T-12-41 by the cold projections against 900 s with the reversal rule (trigger not reached; warning HR-04).

## Next Phase Readiness

The BIND plans 12-13 to 12-16 build on the Bind base `49d41ac9e1966bd9cdb9e953b61d0355a1178554`; the 12-14 commit is the bind. Before 12-15 runs the orchestrator should add the H-03 sentence for `docs/spec/api.md` 8.1 to its plan text (HR-06), and decide whether the recommended gate-trim plan 12-19 (HR-04) runs before the bind or after the final cold measurement. origin/main is `de21fcf`; the records commits after it (`49d41ac`, `0d0d403` and the summary commit) go out with the next push a plan names. The final cold gate-macos measurement after the 12-18 push stays orchestrator-only.

## Self-Check: PASSED

- FOUND: 12-REVIEW.md "## HOST" (Tasks 1 to 3), PIN-IMPACT "- Bind base:" line, the mutant table in "Host steps" and the filled HOST ledger row, LOG.md rows 45904452436 and F-138
- FOUND commits: 7985d49, de21fcf, 49d41ac, 0d0d403 (all ancestors of HEAD; de21fcf equals origin/main)
