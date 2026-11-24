---
phase: 10-carry-in-closure-before-the-freeze
plan: 04
subsystem: ci
tags: [gate-01, ctest-cost, ci-timeout, check-ci-yml, rsp-no-hle, ci-review-log, preflight]

requires:
  - phase: 10-carry-in-closure-before-the-freeze
    provides: plan 10-01 preflight and PIN-IMPACT.md, plan 10-03 per-row oracle pins (last plan before the bound-CI edit)
  - phase: 09-controller-save-media-and-replay
    provides: the F-111 and F-114 hosted measurements and the 673 s cold projection this plan answers
provides:
  - explicit CTest COST 210, 71, 48 and 35 on the four longest hosted tests, read back by ctest --show-only=json-v1 on the gate (227 names) and asan (229 names) presets
  - gate-macos timeout-minutes 15 (gate-linux stays 10) with a per-job CI-TIMEOUT scanner bound, five new selftest mutations (138 cases) and six scratch scanner mutants each killed by its case, in the single bound-CI commit 4ca1054
  - the measured disposition of the repeated rsp-no-hle work in the audio-records step (declined, with file and line evidence and the corrected premise)
  - LOG.md findings F-115 to F-119 (before timings, guard, F-106 outer limits, Binutils parallelism, the run-to-run noise band that answers F-114)
  - REQUIREMENTS.md GATE-01 one-line annotation citing F-116
  - the preflight timeout audit made correct for tests that carry an explicit COST
  - PIN-IMPACT.md section 10 (GATE-01 measurements 10.1 to 10.5)
affects: [10-05, 10-17, 10-18, 10-19, phase-11]

actuals:
  tokens: 5900
  tasks: 4
  commits: 6

tech-stack:
  added: []
  patterns:
    - "explicit COST in plain decimal hosted seconds beside the registration, outside the sanitizer conditional, orders parallel starts only"
    - "a job-scoped bound in the line-pattern scanner (job == gate-macos) with a mutation case per direction of each bound, plus a case that only the integer check can refuse (14.5)"
    - "prove every new selftest case can fail by running the selftest against single-line scanner mutants in scratch copies"

key-files:
  created: []
  modified:
    - core/CMakeLists.txt
    - runner/CMakeLists.txt
    - .github/workflows/ci.yml
    - scripts/check-ci-yml.sh
    - scripts/preflight-phase10.sh
    - docs/ci-reviews/LOG.md
    - .planning/REQUIREMENTS.md
    - .planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md

key-decisions:
  - "COST 210, 71, 48 and 35 are the hosted macOS seconds of the F-111 run (verified in its job log: 210.19, 70.62, 48.34 and 35.26 s); unit.core.state.import starts second behind the 0.01 s runner.fixture-descriptor prerequisite instead of 66th"
  - "gate-macos timeout-minutes 15, gate-linux 10; the scanner bound is per job so the two first-match awk secrets injections of the selftest keep binding to the gate-linux line; gate.sh unchanged"
  - "The rsp-no-hle guard removal is declined: the repeated cost is the CTest name repo.rsp-no-hle in the audio-records step (14.12 of 22 s locally, 41.06 of 42 s hosted macOS), not the direct check-rsp-no-hle.py run (0.19 s); removing the name edits docs/spec/audio-cases.json:182 and so EXPECTED_INVENTORY_SHA256 at scripts/qualify-audio.py:26, which F-106 keeps closed"
  - "The preflight timeout audit holds a test with an explicit COST to 2 times that COST (hosted seconds) instead of 3.5 times the cost-file value, because ctest weights COST into CTestCostData.txt"

patterns-established:
  - "CTest stores (runs * COST + this run) / (runs + 1) for a test with a COST property, so the cost file stops being a native-seconds record for those tests; read native seconds from a ctest log"
  - "A scanner test that only passes for the wrong reason against the old rule (every value above 10 is refused) is not RED evidence; the clean copy that the old rule refuses is"

requirements-completed: [GATE-01]

coverage:
  - id: D1
    description: "COST 210, 71, 48 and 35 are registered on unit.core.state.import, harness.state-field-use-audit, repo.rsp-no-hle and harness.phase09-qualification and visible in ctest --show-only=json-v1 on both presets with no name or inventory change"
    requirement: "GATE-01"
    verification:
      - kind: other
        ref: "Task 1 verify (cmake --preset gate and the json-v1 read-back prints COST ok; cmake --preset asan and grep -c '\"COST\"' prints 4); sh scripts/check-test-inventory.sh --preset gate (227) and --preset asan (229) pass"
        status: pass
    human_judgment: false
  - id: D2
    description: "COST makes a fresh runner start the longest test first and shortens the hosted ctest wall"
    requirement: "GATE-01"
    verification:
      - kind: other
        ref: "build/gate/10-04-task1 fresh-run logs: start positions 2nd to 5th against 66th, 125th, 126th and 174th; local ctest wall 68.9 s and 72.9 s with COST against 70.1 s without (inside the noise)"
        status: pass
    human_judgment: true
    rationale: "The start order is measured locally; the hosted wall effect (up to about 40 s estimated in F-111) is not measured by this plan and is read against the F-114 noise band by plan 10-05 (warm) and plan 10-19 (cold)"
  - id: D3
    description: "gate-macos has timeout-minutes 15, gate-linux keeps 10, and the scanner enforces the bound per job (accepts 15 and 10, refuses 16, 11, 0 and non-integers) with rule comment and selftest changed in the same commit"
    requirement: "GATE-01"
    verification:
      - kind: unit
        ref: "sh scripts/check-ci-yml.sh and --selftest (138 cases, 0 failures); ctest --test-dir build/gate -R '^repo\\.ci-yml(-selftest)?$' (2 of 2); six scratch scanner mutants each fail their intended case; actionlint exit 0; git show --stat 4ca1054 lists exactly ci.yml and check-ci-yml.sh; git diff --quiet scripts/gate.sh"
        status: pass
    human_judgment: false
  - id: D4
    description: "The redundant rsp-no-hle work is measured and its removal declined with a recorded reason before ci.yml was edited, without touching the closed Phase 8 qualifier, inventory or checker"
    requirement: "GATE-01"
    verification:
      - kind: other
        ref: "grep -c repo.rsp-no-hle docs/spec/audio-cases.json is 1; git diff --quiet over scripts/qualify-audio.py, docs/spec/audio-cases.json, scripts/check-audio-cases.py, docs/evidence/phase08-audio.md exits 0; PIN-IMPACT.md section 10.2"
        status: pass
    human_judgment: false
  - id: D5
    description: "LOG.md has F-115 to F-119 (append-only, five rows, each taken or declined with a reason, F-119 naming both F-114 run ids) and REQUIREMENTS.md carries the one-line GATE-01 annotation"
    requirement: "GATE-01"
    verification:
      - kind: other
        ref: "grep -c '^| F-11[5-9] ' docs/ci-reviews/LOG.md is 5; git diff of LOG.md shows 5 added and 0 removed lines; REQUIREMENTS.md 1 added and 0 removed; sh scripts/pii-scan.sh --tree findings=0; sh scripts/preflight-phase10.sh PASS (63 s with the post-gate replay on the clean committed tree)"
        status: pass
    human_judgment: false
  - id: D6
    description: "The hosted macOS gate has a measured cold-cache run inside its 900 s limit"
    requirement: "GATE-01"
    verification:
      - kind: other
        ref: "not delivered by this plan: plan 10-19 deletes the macOS Binutils cache entry, dispatches ci.yml on the final tree and reads gh run view"
        status: pending
    human_judgment: true
    rationale: "GATE-01 is open until plan 10-19: the requirement text makes a measured cold-cache hosted run, not a projection, the success criterion; this plan is the local half"

duration: 19min
completed: 2026-11-24
status: complete
commits: 6
plan_head_before: 5c6ebb6907615a838ac4f1a2dbafa6d723bee5c6
plan_head_after: e5674f12868eca7e3df684eb9b4ec0d7565d90ca
---

# Phase 10 Plan 04: GATE-01 local half Summary

**The longest hosted tests carry explicit CTest COST (210/71/48/35), gate-macos has a 15-minute job limit enforced per job by the CI-TIMEOUT scanner with five new mutation cases (the only bound-CI change of Phase 10, commit 4ca1054), the duplicated rsp-no-hle work is measured and its removal declined with file-and-line evidence, and F-115 to F-119 record the before timings and the run-to-run noise band; the measured cold run stays plan 10-19.**

## Performance

- **Duration:** 19 min
- **Started:** 2026-11-24T01:47:35Z
- **Completed:** 2026-11-24T02:07Z
- **Tasks:** 4 (Task 1 tracer, Task 2 auto, Task 3 auto tdd, Task 4 auto)
- **Files modified:** 8 (0 created, 8 modified); the plan listed 7, the eighth is `scripts/preflight-phase10.sh` (deviation 1)
- **Host:** 1-minute load 5.8 to 9.6 throughout, so every second below is an upper bound for a quiet host

## Accomplishments

- **Task 1 (tracer, `594b402`).** `COST` set beside each registration, outside the sanitizer conditional. Read back on the gate preset (227 names) and the asan preset (229 names, four `COST` entries); `check-test-inventory.sh` passes on both. Fresh-runner simulation with the cost file moved aside: with COST the four tests start 2nd to 5th (a 0.01 s `DEPENDS` prerequisite, `runner.fixture-descriptor`, starts first with or without COST); a scratch revert of the COST lines starts them 66th, 125th, 126th and 174th. Local ctest wall 68.9 s and 72.9 s with COST, 70.1 s without: no local saving beyond noise, as expected for a cheap prefix and a 48 to 51 s longest test. The hosted effect is not measured here.
- **Task 2 (`9ca78c3`).** Measured the audio-records step before editing `ci.yml`. The plan's premise was that the guard is 25 to 32 s of the 42 s hosted step. The hosted job log of run 44795894664 shows the repeated cost is the CTest name `repo.rsp-no-hle` (41.06 s of the 42 s step; 48.34 s in the Gate), locally 14.12 s of a 22 s step; the direct `check-rsp-no-hle.py` run is 0.19 s and its command and `PASS` stdout are required by the hosted emit step. Removing the CTest name edits `docs/spec/audio-cases.json:182`, hence `EXPECTED_INVENTORY_SHA256` (`scripts/qualify-audio.py:26`, used at six more lines, and by the closed Phase 8 test), which F-106 keeps untouched. Declined; the 15-minute limit already fits the 673 s cold projection with 227 s margin.
- **Task 3 (`4ca1054`, one commit, `ci.yml` and `check-ci-yml.sh` only).** `gate-macos` 10 to 15, `gate-linux` kept at 10. CI-TIMEOUT is now 1 to 15 for `gate-macos` and 1 to 10 otherwise, rule comment, message and selftest in the same commit. RED against the unchanged rule and a scratch `ci.yml` with gate-macos at 15: only `clean` and `dir-clean` fail (the new mutation cases pass for the wrong reason because the old rule refuses every value above 10, so they are not counted as RED). GREEN: 138 cases, 0 failures; `ctest -R '^repo\.ci-yml(-selftest)?$'` 2 of 2; `actionlint` exit 0; `act -l` lists both jobs. Six single-line scanner mutants (global 10, global 15, macOS 16, no lower bound, decimals accepted, the wide bound on the wrong job) are each killed by the intended case.
- **Task 4 (`af286b3`).** `ci-review.sh` appended F-115 to F-119 (run column 44795894664), REQUIREMENTS.md got one added line, and PIN-IMPACT.md sections 10.3 and 10.4 record the evidence. The full preflight (with the post-Gate replay) found the audit problem of deviation 1, fixed in `df5442f`, then passed.
- **Close-out records (`e5674f1`).** PIN-IMPACT.md 10.5 records how COST weights the CTest cost history and the audit change.

## Task Commits

1. **Task 1: COST properties and fresh-runner start order** - `594b402` (perf)
2. **Task 2: measure the repeated rsp-no-hle work, decline removal** - `9ca78c3` (docs)
3. **Task 3: gate-macos 15, per-job CI-TIMEOUT bound, five mutations (single bound-CI commit)** - `4ca1054` (ci)
4. **Task 4: F-115 to F-119, REQUIREMENTS annotation, evidence sections** - `af286b3` (docs)
5. **Deviation 1: timeout audit holds COST tests to 2 times COST** - `df5442f` (fix)
6. **COST and cost-history record** - `e5674f1` (docs)

**Plan metadata:** the SUMMARY commit and the STATE, ROADMAP and REQUIREMENTS updates follow in docs(10-04) commits.

## Files Created/Modified

- `core/CMakeLists.txt` - `COST 210` on `unit.core.state.import`
- `runner/CMakeLists.txt` - `COST 71`, `48` and `35` on `harness.state-field-use-audit`, `repo.rsp-no-hle` and `harness.phase09-qualification`
- `.github/workflows/ci.yml` - `gate-macos` `timeout-minutes: 15` (one line)
- `scripts/check-ci-yml.sh` - job-scoped CI-TIMEOUT bound, rule text, `mut_timeout` helper and five mutation cases
- `scripts/preflight-phase10.sh` - timeout audit: 2 times COST for tests that carry COST (not in the plan's list)
- `docs/ci-reviews/LOG.md` - five appended findings
- `.planning/REQUIREMENTS.md` - one annotation line under GATE-01
- `.planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md` - section 10 (GATE-01 measurements)

## Decisions Made

See key-decisions. The decisions that matter to later plans: `ci.yml`, `check-ci-yml.sh` and `gate.sh` are now final for Phase 10; COST is in place for the hosted warm run of plan 10-05; the guard removal is closed as declined and can only come back at a Phase 8 requalification.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Preflight timeout audit read COST-blended values as native seconds**
- **Found during:** Task 4 (the full preflight on the clean committed tree)
- **Issue:** CTest keeps an explicit COST as the running mean of its cost history: the stored value is `(runs * COST + this run) / (runs + 1)` (checked on `harness.phase09-qualification`, 55 runs, COST 35, run 8.07 s stored 34.52; and on `repo.rsp-no-hle`, 112 runs, COST 48, run 14.1 s stored 47.70). `scripts/preflight-phase10.sh` reads that file as native seconds, so after the first local run the audit failed `harness.phase09-qualification` (TIMEOUT 120 s against 3.5 times 34.52 s), and would have failed `unit.core.state.import` (600 against 3.5 times about 207) and `harness.state-field-use-audit` (240 against 3.5 times about 70) after their next runs. Every later Phase 10 plan cites the preflight, so this blocked the phase.
- **Fix:** a test with an explicit COST is audited against 2 times its COST (hosted macOS seconds, the "below 2x" line of F-108); every other test keeps 3.5 times its recorded native seconds. Ratios against COST: 2.86, 3.38, 5.0 and 3.43, all above 2. Checked with crafted inputs (passing set, a COST ratio just below 2, a native ratio just below 3.5), then the real data.
- **Files modified:** `scripts/preflight-phase10.sh` (a plan 10-01 file, not in this plan's list)
- **Verification:** full `sh scripts/preflight-phase10.sh` PASS, 63 s including the replay of 11 ci.yml steps
- **Commit:** `df5442f`; record in PIN-IMPACT.md 10.5 (`e5674f1`)

**2. [Rule 2 - Missing critical] A fifth selftest mutation (14.5) beyond the plan's four**
- **Found during:** Task 3 (scanner mutant proofs)
- **Issue:** the plan's non-integer case (15.5) is also refused by the upper bound, so removing the integer check would not fail it.
- **Fix:** added `gate-macos timeout-minutes 14.5`; the "decimals accepted" scanner mutant is killed only by it.
- **Files modified:** `scripts/check-ci-yml.sh`
- **Commit:** `4ca1054`

**3. [Rule 1 - Premise correction] The guard share and the make -j2 premise**
- **Found during:** Tasks 2 and 4
- **Issue:** the plan estimated the guard at about 25 to 32 s of the 42 s step (the measured repeated cost is the CTest name, about 41 s hosted, while the direct guard is 0.19 s), and named an "optional make -j2" in `build-boot-fixtures.sh`, which already runs `make -j2 all-binutils all-gas all-ld` (line 259, since `e6f0655`).
- **Fix:** F-116 and F-118 record the measured premise; the decisions (both declined) are unchanged.
- **Files modified:** `docs/ci-reviews/LOG.md`, `PIN-IMPACT.md` (new rows only, no earlier row edited)
- **Commit:** `9ca78c3`, `af286b3`

---

**Total deviations:** 3 (1 Rule 3, 1 Rule 2, 1 Rule 1 premise correction)
**Impact on plan:** deviation 1 touched a file outside the plan's list but only the audit function and its header comment; nothing hosted, bound or oracle-related changed. Deviations 2 and 3 are inside the plan's files.

## Issues Encountered

- A scratch run needed a temporary revert of the COST lines (saved as a patch, reverted with `git checkout -- <the two files>`, re-applied and re-verified by the json-v1 read-back) to get the without-COST start order; the original cost file was copied aside and restored afterwards.
- The RED output of Task 3 contains the workstation temporary directory of the selftest; it stays under the ignored `build/gate/10-04-task3/` and is not committed.
- The hosted effect of COST and the cold-cache run are not claimable from this plan (see coverage D2 and D6).

## Authentication Gates

None. `gh` was already authenticated for the read-only job log and run reads.

## User Setup Required

None - no external service configuration required.

## Known Stubs

None.

## Threat Flags

None new. T-10-08 mitigated (only CI-TIMEOUT changed; 138 selftest cases pass, five new mutations, six scanner mutants each killed). T-10-09 mitigated by margin (900 s limit over the 673 s projection; the measured cold run is plan 10-19). T-10-10 mitigated (the Phase 8 qualifier, inventory, checker and record show no diff; guard removal declined).

## Next Phase Readiness

- Plan 10-05 reads the first Phase 10 hosted run (warm) against F-111 and the F-119 noise band: the four COST tests' start order, ctest wall and the audio-records step, and appends F-120 onward; `ci.yml`, `check-ci-yml.sh` and `gate.sh` must not change again.
- For the four COST tests read native seconds from a ctest run log, not from `CTestCostData.txt` (PIN-IMPACT.md 10.5); new Phase 10 names carry no COST, so the gate-growth ledger rule stays valid for them.
- Plan 10-19 owns the measured cold-cache run (delete the macOS Binutils cache entry, dispatch, read `gh run view`); GATE-01 stays unchecked until then.
- Orchestrator items: (1) the preflight audit change is in a plan 10-01 file; (2) `requirements.ready-ids` reports GATE-01 blocked by sibling plans, so no checkbox is flipped; (3) nothing was pushed.

---
*Phase: 10-carry-in-closure-before-the-freeze*
*Completed: 2026-11-24*

## Self-Check: PASSED
