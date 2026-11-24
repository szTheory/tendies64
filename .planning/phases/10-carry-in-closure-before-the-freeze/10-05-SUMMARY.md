---
phase: 10-carry-in-closure-before-the-freeze
plan: 05
subsystem: ci
tags: [review, scratch-clone, wave-0, push, hosted-ci, ci-review-log, gate-growth-ledger, gate-01, gate-02]

requires:
  - phase: 10-carry-in-closure-before-the-freeze
    provides: plans 10-01 to 10-04 (preflight, K1 and digest sweep, per-row oracle pins, COST and the 15-minute gate-macos limit) as the Wave 0 diff under review
  - phase: 09-controller-save-media-and-replay
    provides: the F-111 and F-114 before timings and the F-119 noise band that the hosted after-timings are read against
provides:
  - 10-REVIEW.md Wave 0 section (scratch gate, checks (a) to (i), findings table with no blocking finding)
  - the first Phase 10 push (origin/main 612a3ad to 33f6e9c, 48 commits) after a passing 67 s preflight, and the first green hosted pair (run 45311817943)
  - LOG.md Runs row and F-120, the GATE-01 warm after-measurement (gate-macos 346 s, ctest 184 s, unit.core.state.import starts 2nd of 227 on both OSes)
  - PIN-IMPACT.md Gate-growth ledger Wave 0 row and the Wave 1 base f6ab8c26d008b8301b729b5aae2715e1b1079318
affects: [10-06, 10-08, 10-13, 10-18, 10-19, phase-11]

actuals:
  tokens: 5019
  tasks: 3
  commits: 4

tech-stack:
  added: []
  patterns:
    - "a reviewer's scratch tree is a local clone detached at the reviewed head (own repository, full history), not a bare git archive extraction, because the gate reads git's tracked-file list and the fixture script reads manifest source revisions from history"
    - "a pinned-SHA push (refspec in braces under zsh) followed by selecting the ci.yml run by head SHA and a bounded gh run watch"
    - "hosted after-timings are read against the five-sample F-119 band, and a single sample is recorded as a measurement, not as a saving"

key-files:
  created:
    - .planning/phases/10-carry-in-closure-before-the-freeze/10-REVIEW.md
  modified:
    - .planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md
    - docs/ci-reviews/LOG.md

key-decisions:
  - "The Wave 0 head passes the full gate in an isolated scratch clone (227 of 227, 119 s) and nine mechanical checks find no moved oracle-bound file, a still-blocked local_decision, one bound-CI commit (4ca1054), 0 sweep differences over 1599 lines and reproducible Phase 9 claim-binding constants; no blocking finding"
  - "The scratch method of the plan (bare git archive tree) cannot pass the gate; the review plans 10-08, 10-13 and 10-18 should use a local clone detached at the head plus a copy of build/tools/install"
  - "First hosted pair of Phase 10 is green on both jobs with a Binutils cache hit and no cache deleted: gate-macos run 346 s, ctest 184 s (251 s in F-111), gate-linux 229 s; the four COST tests start 2nd to 5th on the fresh runners"
  - "The macOS saving is not claimed: 346 s is 3 s below the lowest of five earlier samples (349 to 487 s), so one sample cannot be told apart from runner spread; the cold run stays plan 10-19"
  - "Wave 1 base f6ab8c26d008b8301b729b5aae2715e1b1079318 is recorded in a commit that changes only PIN-IMPACT.md (b4b447b)"

patterns-established:
  - "Review plans record the reviewed head, run each cited command again and write findings as id, severity, file:line, finding, disposition rows"
  - "A recording commit (a base line) is separate from the evidence commit that precedes it, so the base is the head of the last non-recording commit"

requirements-completed: [GATE-01, GATE-02]

coverage:
  - id: D1
    description: "The Wave 0 head passes the full gate (sh scripts/gate.sh, default environment) in a scratch clone outside the shared build tree"
    requirement: "GATE-02"
    verification:
      - kind: integration
        ref: "sh scripts/gate.sh in build/10-review/w0/src (clone detached at 87e7628): [gate] PASS, total 119s, ctest 227 of 227 passed"
        status: pass
    human_judgment: false
  - id: D2
    description: "Independent mechanical review of the Wave 0 diff: checks (a) to (i) re-run and recorded, no blocking finding, both GATE-02 bindings re-derived (39 row hashes, Phase 9 claim-binding digests, closed records and F2_PINS unchanged)"
    requirement: "GATE-02"
    verification:
      - kind: other
        ref: "10-REVIEW.md Wave 0 'Task 2' table (a) to (i); python3 -B scripts/digest-sweep.py --base WAVE0_BASE --head HEAD (0 differences, exit 0); --expect-blocked exit 0; ctest -R '^harness\\.phase09-qualification$' 1 of 1; sh scripts/check-ci-yml.sh --selftest 138 cases 0 failures"
        status: pass
    human_judgment: true
    rationale: "The reading of each Wave 0 diff against its plan is judgment; the mechanical checks are reproducible, the findings (one warning, four info) are the reviewer's classification"
  - id: D3
    description: "The Wave 0 head is pushed after a passing preflight and the hosted pair of that head concluded success on both jobs, with the warm after-timings recorded against F-111 and the F-119 band"
    requirement: "GATE-01"
    verification:
      - kind: integration
        ref: "sh scripts/preflight-phase10.sh PASS 67 s; run 45311817943 (ci.yml, push, head 33f6e9c) completed success on gate-linux and gate-macos; Task 3 verify 1 (gh run list for origin/main) passes; grep -c '^| F-120 ' docs/ci-reviews/LOG.md is 1"
        status: pass
    human_judgment: true
    rationale: "The hosted timings are measured, but one sample read against a five-sample noise band cannot establish the effect of COST; plan 10-19 owns the cold measurement and GATE-01 stays open until then"
  - id: D4
    description: "PIN-IMPACT.md carries the Wave 0 Gate-growth ledger row and the Wave 1 base as one 40-hex line in a commit that changes only that file"
    requirement: "GATE-01"
    verification:
      - kind: other
        ref: "Task 3 verify 3: base extraction idiom yields one commit, git diff --name-only BASE HEAD equals PIN-IMPACT.md; ledger row has 0.36 native seconds, the hosted ctest delta and the G4-exception reason"
        status: pass
    human_judgment: false

duration: 21min
completed: 2026-11-24
status: complete
commits: 4
plan_head_before: 87e76283968be0cdc0df4c37d025d792071e14a7
plan_head_after: b4b447bbfda607a353795e0635ad2f9ee3dcda1a
---

# Phase 10 Plan 05: Wave 0 review, first push and warm after-timings Summary

**Wave 0 (plans 10-01 to 10-04) was reviewed independently in a scratch clone (full gate 227 of 227 in 119 s, nine mechanical checks, no blocking finding); the head was pushed after a 67 s preflight and the first Phase 10 hosted pair (run 45311817943) is green with gate-macos at 346 s, ctest at 184 s and `unit.core.state.import` starting 2nd of 227; F-120, the Wave 0 ledger row and the Wave 1 base are recorded.**

## Performance

- **Duration:** 21 min (2026-11-24T02:09:44Z to about 02:31Z, of which the hosted run was 6 min)
- **Tasks:** 3 (Task 1 tracer, Task 2 auto, Task 3 auto)
- **Files:** 1 created (`10-REVIEW.md`), 2 modified (`PIN-IMPACT.md`, `docs/ci-reviews/LOG.md`); no source file touched
- **Host:** 1-minute load 6.5 to 8.4 during the runs, so local seconds are upper bounds

## Accomplishments

- **Task 1 (tracer, `cb65dae`).** The committed head `87e7628` was built and tested outside the shared tree: `sh scripts/gate.sh` in `build/10-review/w0/src` prints `[gate] PASS`, `total 119s` (ctest 227 of 227 in 69 s; the three new names are `repo.p10-mutant-anchors` and the two `unit.core.k1.*` names). The plan's literal method needed a correction (deviation 1): two earlier attempts failed for environmental reasons that the review record documents.
- **Task 2 (`33f6e9c`).** Checks (a) to (i) re-run and recorded. No file of the 13 `EXPECTED_CORPUS_PATHS` moved; `--expect-blocked` exits 0 and `--require-approved` exits 2; `ci.yml`, `check-ci-yml.sh` and `gate.sh` changed in exactly one commit (`4ca1054`, `gate.sh` not at all); the CI scanner selftest has 138 cases and 0 failures; the 17 CTest count rows match; the 39 manifest row hashes recomputed independently equal `bound_rows` (same ids, same order); the manifest digest `b75864fd...`, the view digest `beb2b1c8...`, the closed Phase 9 records and the `F2_PINS` literal reproduce against the base; the digest sweep reports 1599 shared lines, 0 differences. In the scratch clone the mutant driver classifies its controls correctly and each K1 case alone kills the widened depth predicate. Findings: no blocking one; W0-01 (warning, scratch method), W0-02 to W0-05 (info).
- **Task 3 (`f6ab8c2`, then `b4b447b`).** The preflight passed on the clean committed tree (67 s, 220 units compiled under the act image cc, 11 post-Gate steps replayed, PII clean on the tree and the 48-commit range). `origin/main` moved from `612a3ad` to `33f6e9c` by a pinned-SHA fast-forward push. Run `45311817943` completed with success on both jobs and a Binutils cache hit on both. F-120 and the Runs row are in `LOG.md`; the PIN-IMPACT.md ledger has its Wave 0 row; the Wave 1 base is recorded.
- **Hosted after-timings (against F-111 and the F-119 band).** gate-macos run 346 s (Gate 298 s, ctest 184 s, audio records 28 s) against 487 s (410, 251, 42) and F-114 435 s; gate-linux run 229 s (Gate 181 s, ctest 114 s, audio records 26 s) against 203 s and 305 s. `unit.core.state.import` starts 2nd of 227 on both OSes (66th without COST), 162.57 s on macOS against 210.19 s; the other three COST tests start 3rd to 5th. The macOS result is at the bottom edge of the earlier band (349 to 487 s), so it is a measurement, not a proven saving.

## Task Commits

1. **Task 1: scratch gate of the Wave 0 head** - `cb65dae` (docs)
2. **Task 2: review checks (a) to (i) and findings** - `33f6e9c` (docs); this is the head that was pushed and run
3. **Task 3: hosted pair, F-120, ledger row** - `f6ab8c2` (docs)
4. **Task 3: Wave 1 base record (only PIN-IMPACT.md)** - `b4b447b` (docs)

**Plan metadata:** this SUMMARY and the STATE and ROADMAP updates follow in docs(10-05) commits. The pushed head `33f6e9c` differs from the gated tree `87e7628` only by `10-REVIEW.md`.

## Files Created/Modified

- `.planning/phases/10-carry-in-closure-before-the-freeze/10-REVIEW.md` - new; the Wave 0 section (scratch gate, checks, findings, preflight, push, hosted pair)
- `.planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md` - the Wave 0 ledger row and the `- Wave 1 base:` line
- `docs/ci-reviews/LOG.md` - one Runs row and F-120

## Decisions Made

See key-decisions. The ones later plans need: the Wave 1 base literal; the scratch-clone method for the review plans 10-08, 10-13 and 10-18; and that the hosted macOS saving is not claimed from one sample.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] The scratch tree of the plan cannot run the gate**
- **Found during:** Task 1
- **Issue:** the plan extracts the head with `git archive` into `build/10-review/w0/src`. That tree has no repository of its own and sits inside the ignored `build/` of the main repository, so tests that read git's tracked-file list fail (attempt 1: 221 of 227, six failures: `gov.claims.repo`, `gov.provenance.repo`, `repo.sched-k5-anchors`, `repo.p10-mutant-anchors`, `runner.cpu-diagnostic` and four sub-tests of `harness.phase09-qualification`). A one-commit `git init` (attempt 2) fails at fixture-qualification, which needs the manifest's source revisions from history.
- **Fix:** attempt 3 uses `git clone --no-checkout --no-hardlinks` of the main repository into the scratch directory, detached at the reviewed head, plus a copy of `build/tools/install`. `git archive` of the head from the main repository and from the clone have the same SHA-256, so the tree is exactly the archive.
- **Files modified:** none (scratch only, under the ignored `build/10-review/`)
- **Verification:** `[gate] PASS`, 227 of 227
- **Commit:** `cb65dae` (record)

**2. [Rule 1 - Premise correction] Tasks 1 and 2 were committed before the push, so the pushed head is a docs commit**
- **Found during:** Task 3
- **Issue:** the plan's action lists the 10-REVIEW.md commit after the hosted run, which would have left Tasks 1 and 2 uncommitted during the push. The task commit protocol requires a commit per task.
- **Fix:** Tasks 1 and 2 were committed first; the pushed and hosted head `33f6e9c` therefore contains the review record of Tasks 1 and 2 and differs from the gated tree `87e7628` only by that file. The hosted run is on the exact tree that the scratch gate ran, plus one planning file.
- **Commit:** `cb65dae`, `33f6e9c`

**3. [Rule 1 - Wording] F-120 start positions**
- **Found during:** Task 3 (before the commit)
- **Issue:** the first draft of F-120 gave 176th for `harness.phase09-qualification` (its test number) where the measured start position of the local simulation is 174th.
- **Fix:** corrected in the working tree before F-120 was ever committed; the text now cites the plan 10-04 simulation (66th, 126th, 125th and 174th).
- **Commit:** `f6ab8c2`

---

**Total deviations:** 3 (1 Rule 3, 2 Rule 1)
**Impact on plan:** none on scope or evidence; deviation 1 changes how the later review plans should extract their scratch tree.

## Issues Encountered

- The first push command failed locally because zsh expanded `$SHA:refs/heads/main` as a modifier; git reported an unmatched source refspec and published nothing. The second attempt, with the refspec in braces, pushed the pinned SHA.
- The scratch gate and the sweep ran at a 1-minute load of 6.5 to 8.4 (other sessions on the same host), so their seconds are upper bounds.
- The `build/10-review/w0` clone (about 3 GB with its build) and `build/10-sweep` remain under the ignored `build/` for the orchestrator to prune.

## Authentication Gates

None. `gh` was authenticated for the push, the read-only run reads and the log download.

## User Setup Required

None - no external service configuration required.

## Known Stubs

None.

## Threat Flags

None new. T-10-11 mitigated (the preflight `pii` step passed on the tree and on the 48-commit range, the range held no binary, ROM-extension file, workstation path or e-mail address, and the remote is the public szTheory repository). T-10-12 mitigated (all builds in `build/10-review/w0`; the shared `build/gate` tree only ran `ctest -N` and two read-only ctest runs, no clean or fixture rebuild). No hosted cache was deleted (T-10 prohibition).

## Review findings (by severity)

| Severity | Count | Rows |
|----------|-------|------|
| blocking | 0 | none |
| warning | 1 | W0-01 scratch method (resolved here; later review plans should reword it) |
| info | 4 | W0-02 digest-sweep head-extra ids are not coverage-checked; W0-03 mutant driver reports the last failing kill check; W0-04 COST tests held to 2x COST in the timeout audit (thinnest 2.86); W0-05 recorded exception of the Phase 9 claim comparator (T-10-49) |

## Next Phase Readiness

- Wave 1 starts at the base `f6ab8c26d008b8301b729b5aae2715e1b1079318` (`PIN-IMPACT.md` section 5); plan 10-06 (VG-01 on descriptor 10) may begin. Every later Wave 0 check reads the Wave 0 base `4ac633e...`, never HEAD.
- Plan 10-08 and the later reviews should say "clone detached at the head" instead of a bare `git archive` tree (W0-01).
- Plans 10-09 to 10-13: assert that the four si-v2 ids appear as head-only digest lines in the sweep (W0-02).
- Plan 10-19 owns the measured cold run and must surface the Phase 9 comparator exception (W0-05) to the owner; GATE-01 stays open until then.
- Orchestrator items: (1) `origin/main` is now `33f6e9c`; the two later docs(10-05) commits (`f6ab8c2`, `b4b447b`), this SUMMARY and the STATE and ROADMAP updates are local and unpushed; (2) the scratch clones under `build/10-review/` may be pruned; (3) `requirements.ready-ids` decides which of GATE-01 and GATE-02 can be checked off.

---
*Phase: 10-carry-in-closure-before-the-freeze*
*Completed: 2026-11-24*

## Self-Check: PASSED

`10-REVIEW.md`, `PIN-IMPACT.md`, `docs/ci-reviews/LOG.md` and this file exist; the four task commits (`cb65dae`, `33f6e9c`, `f6ab8c2`, `b4b447b`) are ancestors of HEAD and `33f6e9c` is an ancestor of `origin/main`; the Wave 1 base line is one 40-hex literal; `grep -c "^| F-120 "` is 1; the three Task 3 verify commands passed.
