---
phase: 11-installed-consumer-and-freeze-candidate
plan: 21
subsystem: testing
tags: [review, install-wave, hosted-ci, gate-growth, cold-measurement, freeze-base, linux-undefined-set]
status: complete

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: the INSTALL wave (11-16 to 11-20), the 11-27 advance span checks, the 11-28 repair of the red hosted pair (I-11, I-01, I-07) and the 11-29 header and spec text fixes (I-02, I-03, I-04)
provides:
  - 11-REVIEW.md "INSTALL": scratch-clone gate of the reviewed head, checklist (a) to (l), decision table, findings I-01 to I-11 and their final dispositions (resolved by 11-28 and 11-29, or accepted), the second push and the hosted result of check (l)
  - origin/main 4b13a90 pushed (fast-forward from 0e6b96d, no tag) with a green hosted pair on both operating systems (run 45775107983, warm, 324 of 324 names)
  - the Linux rows of docs/abi/installed-undefined.txt confirmed by a hosted measurement (api04.members on gate-linux, gcc 13.3.0)
  - docs/ci-reviews/LOG.md Runs rows for 45772586364 and 45775107983, finding F-133 (the measured cold gate-macos run, 731 s, margin 169 s) and F-134 (the INSTALL hosted pair)
  - PIN-IMPACT.md INSTALL row of the Gate-growth ledger (measured cold margin replaces the projections) and the Freeze base line
affects: [11-22, 11-23, 11-24, 11-25]

actuals:
  tokens: 14840
  tasks: 3
  commits: 17

plan_head_before: 862920da140e4a9a3936162abae7e660e0b99205
plan_head_after: 0d17d6f2c52c8d998dfbd951314f2023fb8018cf

tech-stack:
  added: []
  patterns:
    - "a review plan interrupted by a red hosted pair resumes after the numbered repair plans: the review record is appended with a resolutions table, never edited, and the second push gets its own LOG rows"
    - "a cold hosted measurement is recorded as a measurement that supersedes the earlier projections (append-only rows are not edited; the new finding names the superseded ones)"

key-files:
  created:
    - .planning/phases/11-installed-consumer-and-freeze-candidate/11-21-SUMMARY.md
  modified:
    - .planning/phases/11-installed-consumer-and-freeze-candidate/11-REVIEW.md
    - .planning/phases/11-installed-consumer-and-freeze-candidate/PIN-IMPACT.md
    - docs/ci-reviews/LOG.md

key-decisions:
  - "The Freeze base is the head of the last non-recording commit of this plan (189afa8, the commit that carries the LOG, review and ledger records), recorded by a commit that changes only PIN-IMPACT.md, as the plan's verify defines; it is a docs-only descendant of the pushed green head 4b13a90 (the three files differ), so the bound tree equals a tree with a green hosted pair"
  - "The measured cold gate-macos figure (731 s, margin 169 s, run 45772586364) is the figure of record in the INSTALL ledger row and replaces every projection; the slow-runner sensitivity (slowest warm sample 592 s plus the 193 s cold cost = 785 s, margin 115 s) is recorded as a warning for 11-25, not as a trigger of the reversal rule"
  - "No requirement is marked complete: the plan's own criteria deliver the review and the hosted proof, and 11-25 (requirements VER-05, API-01 to API-04) is the closing plan"

patterns-established:
  - "INSTALL ledger row form: names by plan, COST sum, hosted seconds per plan on both operating systems, the measured cold margin, then a labelled sensitivity note"

requirements-completed: []

coverage:
  - id: D1
    description: "the INSTALL head passes the full gate in an isolated scratch clone and the checklist (a) to (l) is recorded control by control"
    requirement: VER-05
    verification:
      - kind: command
        ref: "11-REVIEW.md INSTALL Task 1 and Task 2 (first session, head 862920d: [gate] PASS 324 of 324, checks (a) to (l))"
        status: pass
    human_judgment: false
  - id: D2
    description: "the pushed head 4b13a90 has a green hosted pair on gate-linux and gate-macos, and the Linux rows of docs/abi/installed-undefined.txt are confirmed by api04.members on gate-linux"
    requirement: API-04
    verification:
      - kind: command
        ref: "gh run view 45775107983 (conclusion success, 324 of 324 on both jobs); api04.members Passed 7.84 s on gate-linux"
        status: pass
    human_judgment: false
  - id: D3
    description: "the Gate-growth ledger has the INSTALL row with the measured cold figure and its margin, and the Freeze base is a single 40-hex line recorded by a commit that changes only PIN-IMPACT.md"
    requirement: VER-05
    verification:
      - kind: command
        ref: "the plan's two verify commands (origin/main run completed success; Freeze base form and diff) both exit 0"
        status: pass
    human_judgment: false

duration: 50min
completed: 2026-11-26
---

# Phase 11 Plan 21: INSTALL wave review, second push, hosted pair, ledger row and Freeze base Summary

**The INSTALL wave is reviewed in a scratch clone, its three warnings and its blocking finding are resolved by plans 11-28 and 11-29, the repaired head is on origin/main with a green hosted pair (gate-linux 240 s, gate-macos 592 s, 324 of 324), the Linux undefined rows are confirmed on the hosted runner, the measured cold gate-macos figure (731 s, margin 169 s) is in the Gate-growth ledger, and the Freeze base is recorded.**

## Performance

- **Duration:** about 50 min over two sessions (about 30 min for Tasks 1 and 2 and the first push, about 20 min for the resumed Task 3; the hosted run itself took 10 min of that)
- **Tasks:** 3 (Task 1 `tracer`, Tasks 2 and 3 `auto`)
- **Files modified:** 3 records (`11-REVIEW.md`, `PIN-IMPACT.md`, `docs/ci-reviews/LOG.md`) plus this summary; no source file

## Accomplishments

- **Task 1.** The reviewed INSTALL head `862920d` passed `sh scripts/gate.sh` in a detached scratch clone (324 of 324, 160 s). Recorded in `11-REVIEW.md` "INSTALL".
- **Task 2.** Checks (a) to (l), the decision table and findings I-01 to I-10 (first session). Every negative control was read at log level and fails for its own stated reason.
- **Task 3, first attempt (first session).** The first push (`463bd09`) gave run 45766004142, red on gate-linux (I-11: the two version-mismatch rows expected a reason line only CMake 4 prints); the plan stopped before the ledger row and the Freeze base (commit `4d6f05a`, F-129 and F-130).
- **Repairs between the two sessions (other plans).** 11-28 fixed I-11, I-01 (`9e5acee`) and I-07 (`0e6b96d`) and pushed `0e6b96d` with a green pair (run 45770631197, F-131, F-132); 11-29 fixed the header and spec texts I-02, I-03, I-04 (`21ca1d9`); the orchestrator took the cold gate-macos run 45772586364 on `0e6b96d`.
- **Task 3, resumed.** Recorded the cold run (Runs row and F-133: job 731 s against 900 s, margin 169 s, Gate 669 s with fixture-qualification 204 s; it supersedes every earlier cold projection); marked the findings resolved or accepted in `11-REVIEW.md` (appended table, the review tables are not edited); ran the full preflight (PASS, 88 s) and the full gate (PASS, 324 of 324, 145 s) on the clean local head; PII scans and the rights scan clean; pushed `4b13a9032dabfe1e00a8b86808cb7501504b100d` (fast-forward from `0e6b96d`, nine commits, no tag, no force); watched run 45775107983 to green on both jobs with no rerun; recorded check (l) hosted, the Runs row and F-134, the INSTALL ledger row and the Freeze base.

## Task Commits

1. **Task 1: clone gate of the INSTALL head** - `93806be` (docs)
2. **Task 2: INSTALL checklist, control-by-control log review, findings** - `463bd09` (docs)
3. **Task 3, first attempt: red hosted pair recorded, stop before the Freeze base** - `4d6f05a` (docs)
4. **Task 3, resumed: cold run F-133** - `584656f` (docs)
5. **Task 3, resumed: findings resolved or accepted in 11-REVIEW.md (the pushed head)** - `4b13a90` (docs)
6. **Task 3, resumed: green pair, check (l) hosted, F-134, INSTALL ledger row** - `189afa8` (docs)
7. **Task 3, resumed: Freeze base** - `0d17d6f` (docs; PIN-IMPACT.md only)

`commits: 17` in the frontmatter is `git rev-list --count 862920d..0d17d6f` (the measured instrument): it includes ten commits of the repair plans that ran inside this plan's span (`5cce7cf`, five of 11-28, four of 11-29). This plan's own commits are the seven above.

## Hosted measurement (run 45775107983, head `4b13a90`, warm: Binutils cache hit on both jobs)

| Job | cmake | queue s | job s | Gate step s | ctest s | slowest named step |
|-----|-------|---------|-------|-------------|---------|--------------------|
| gate-linux | 3.31.6 | 2 | 240 | 197 | 128 | Gate 197 s |
| gate-macos | 4.5.0 | 3 | 592 | 525 | 317 | Gate 525 s |

Wall 596 s, critical path 595 s, 324 of 324 names on both jobs, no rerun.

| Cold run 45772586364 (workflow_dispatch, head `0e6b96d`, macOS Binutils cache entry deleted first; the orchestrator's measurement) | s |
|------------------------------------------|---|
| gate-macos job | 731 (margin 169 s, 18.8 percent) |
| of which Gate step | 669 (fixture-qualification 204, ctest 284, test-inventory-selftest 75, boot-evidence-selftest 35, build 30, core-lint 24) |
| gate-linux job (cache hit) | 263 |

The Phase 10 cold run (F-124) was also 731 s (254 names, Gate 653 s): equal totals by coincidence with different step mixes (F-133).

**Gate-growth reading.** The 49 INSTALL-wave names sum 100.65 s on hosted macOS and 39.05 s on hosted Linux (F-134, per plan in the ledger row); registered COST sum 31.85 s. The measured cold margin (169 s) is above the 120 s warning level and the G4 reversal rule (a cold run above 900 s) is not triggered. Warning recorded for 11-25: the three warm gate-macos samples of the same 324 names are 487, 538 and 592 s, and the slowest plus the 193 s cold cost is 785 s (margin 115 s), a sensitivity and not a measurement.

## Decisions Made

See `key-decisions` in the frontmatter.

## Deviations from Plan

### Auto-fixed Issues

None (the reviewer fixes nothing; the findings went to numbered plans 11-28 and 11-29).

### Process deviations

**1. Task 3 interrupted and finished in a second session**
- **What:** the plan runs push, hosted pair, ledger row and Freeze base in one step; the first push was red (I-11), so Task 3 stopped (blocking finding rule), 11-28 and 11-29 ran, and Task 3 was finished here on a second push. Two pushes of the plan: `463bd09` (red) and `4b13a90` (green).
- **Files modified:** none beyond the three records of the plan.
- **Commit:** `4d6f05a` (stop record), `584656f` to `0d17d6f` (resumed).

**2. The cold figure is a measurement, not a projection**
- **What:** the plan's rule projects a cold figure from the warm hosted seconds plus the Binutils delta. The orchestrator measured the cold run (run 45772586364), so the ledger row carries the measurement; all earlier projections (673 to 884 s) are recorded as superseded in F-133 (append-only, none edited).

**3. Freeze base form**
- **What:** the resume instruction named the pushed green head as the Freeze base; the plan's verify requires a base whose diff to HEAD is exactly PIN-IMPACT.md, and the records (hosted seconds, ledger row) can only be written after the push. The base is therefore the head of the records commit `189afa8`, a docs-only descendant of the pushed green head `4b13a90` (its diff to `4b13a90` is `docs/ci-reviews/LOG.md`, `11-REVIEW.md`, `PIN-IMPACT.md`). Both verify commands of the plan pass.

**4. Checklist not re-run in a clone after the repairs**
- **What:** checks (a) to (l) were run in the clone at the reviewed head `862920d`. After it only the control driver and table (11-28), one test script (11-28) and comment and spec text (11-29) changed; the resumed session re-ran the full preflight (including the Linux compile and the `ci.yml` replay) and the full gate on the final head instead of repeating the clone checklist, and the 11-29 digest sweep reported 0 differences.

**Total deviations:** 4 recorded (all process). **Impact:** none on the delivered evidence.

## Authentication Gates

None.

## Issues Encountered

- Host load varied between 5 and 12 during the preflight and gate; both passed at the first attempt, no limit widened.
- The job logs of a run are only readable after the whole run completes (the gate-linux log was unavailable while gate-macos was still running); the check (l) result was read after completion.

## Verification

| Check | Result |
|-------|--------|
| `sh scripts/preflight-phase11.sh` (full, clean committed tree at `4b13a90`) | PASS, 88 s (head-ci ok on `0e6b96d`, linux-compile 233 units, post-gate-replay 11 steps) |
| `sh scripts/gate.sh` at `4b13a90` | PASS, 324 of 324, ctest 86 s, total 145 s |
| `sh scripts/pii-scan.sh --tree` / `--range origin/main..HEAD` | 0 findings (1425 files; 9 commits); clean again before each later docs commit |
| `sh scripts/rights-scan.sh --tree --range origin/main..HEAD` | `tree=1425 history=15 findings=0` |
| Hosted run 45775107983 | success on gate-linux and gate-macos, 324 of 324, no rerun |
| Plan verify 1 (`origin/main` run completed success) | `completed success` for `4b13a9032dabfe1e00a8b86808cb7501504b100d` |
| Plan verify 2 (Freeze base form, commit exists, diff is PIN-IMPACT.md only) | exit 0 (base `189afa8ff454415870b5a703e9f728e4ae3dc5ea`) |

## Known Stubs

None.

## Threat Flags

None. T-11-45 (information disclosure) was mitigated by the full preflight pii step and `pii-scan --range` before the push; T-11-46 by the log-level control review of Task 2; T-11-47 by the measured cold figure against 900 s (margin 169 s).

## Notes for the orchestrator

- **Nothing is unpushed except the docs commits made after the push.** origin/main is `4b13a90` (green). Local commits after it: `189afa8`, `0d17d6f` and this plan's closeout commits (docs only).
- **Freeze base** is `189afa8ff454415870b5a703e9f728e4ae3dc5ea` (see deviation 3). 11-22 to 11-25 read it from PIN-IMPACT "Wave bases".
- **Requirements.** None marked complete (VER-05, API-02, API-04 stay pending; 11-25 closes them).
- **Warning for 11-25.** Cold gate-macos measured 731 s (margin 169 s); the slowest warm sample plus the cold cost gives 785 s (margin 115 s). 11-25 repeats the cold run with the final test set; keep every new CTest name cheap and `COST` set.
- **Accepted info items** still open as notes: I-05 (bus-read `value_out` span), I-06 (digest sampling in the gate lane), I-08, I-09, I-10.

## Self-Check: PASSED

- `11-REVIEW.md`, `PIN-IMPACT.md` and `docs/ci-reviews/LOG.md` exist and are committed; this summary exists
- commits `93806be`, `463bd09`, `4d6f05a`, `584656f`, `4b13a90`, `189afa8`, `0d17d6f` are ancestors of HEAD; `4b13a90` equals origin/main
- the hosted conclusion of run 45775107983 is success on both jobs; the Freeze base line is present exactly once
