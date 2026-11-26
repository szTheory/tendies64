---
phase: 11-installed-consumer-and-freeze-candidate
plan: 28
subsystem: testing
tags: [defect-fix, negative-controls, cmake-3.31, cmake-4, readiness-clock, hosted-ci, harness-flake]
status: complete

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: the 11-21 review findings I-11 (blocking, F-130), I-01 and I-07 (F-129), the negative-control driver and table of 11-19, the red hosted run 45766004142
provides:
  - installed.controls.version-mismatch-minor and -patch expect the refusal headline both CMake majors print (compatible with requested version, requested version quoted), proven present in the hosted cmake 3.31.6 text and the local cmake 4.4.3 text and absent from the missing-package log
  - doctored-header-abi raises the ABI minor by exactly one (UINT32_C(1) to UINT32_C(2)), with a driver assertion before the build; the 11-19 record is corrected in PIN-IMPACT
  - test_closed_pipes_do_not_end_the_outer_deadline measures its 0.1 s outer deadline from guest readiness (a readiness clock), reproduced red under a 0.3 s interpreter start-up delay and green 20 of 20
  - origin/main 0e6b96d pushed (fast-forward from 463bd09) with a green hosted pair on both jobs; F-130 and F-129 resolved in the CI log (F-131, F-132)
affects: [11-21, 11-25]

actuals:
  tokens: 7053
  tasks: 3
  commits: 3

plan_head_before: 5cce7cf8ace4aa5cd6b88d66aaa2f8cd8b15b684
plan_head_after: c69b6c57cb95cdb8c407a9771ac105d47210be62

tech-stack:
  added: []
  patterns:
    - "a control that matches CMake output expects the headline that every supported CMake major prints on one line, tied to its own argument, and is checked against the hosted log of the runner that failed"
    - "a supervisor race test measures its deadline from a readiness signal by patching the supervisor's clock, instead of sleeping longer"

key-files:
  created: []
  modified:
    - scripts/check-install.sh
    - tests/installed-consumer/controls.tsv
    - scripts/tests/test_rsp_run.py
    - docs/ci-reviews/LOG.md
    - .planning/phases/11-installed-consumer-and-freeze-candidate/PIN-IMPACT.md
    - .planning/phases/11-installed-consumer-and-freeze-candidate/deferred-items.md

key-decisions:
  - "The version rows expect compatible with requested version plus the quoted requested version (0.2 and 0.1.5), not the CMake-4-only reason line and not a bare phrase: it is on one line in both majors' output, differs per control, and the missing-package refusal does not contain it (T-11-68)"
  - "The doctored-header assertion lives in the driver (original minor plus one, checked before the build) and not only in the awk, so a later change of the header line cannot silently doctor a different number"
  - "The F-129 fix patches the supervisor's time module inside the one test (monotonic stands at zero until the guest publishes its batch file, 30 real seconds of grace) and reorders the guest to close its pipes before it publishes readiness; no sleep was lengthened and the supervisor script is untouched"
  - "The preflight head-ci step fails by design on a red origin/main; the script was not edited and the remaining steps ran from a scratch copy without that one line, recorded as a deviation; the real script passed head-ci after the push"

patterns-established:
  - "a hosted failure whose text depends on the runner's tool version is reproduced from the job log with the driver's own judge before any change, and the new expectation is checked against both outputs"

requirements-completed: []

coverage:
  - id: D1
    description: "installed.controls.version-mismatch-minor and -patch hold on cmake 3.31.6 and 4 and still fail for the version refusal, not a missing package"
    requirement: VER-05
    verification:
      - kind: ctest
        ref: "installed.controls.version-mismatch-minor, installed.controls.version-mismatch-patch (local cmake 4.4.3, hosted cmake 3.31.6 and 4.5.0)"
        status: pass
    human_judgment: false
  - id: D2
    description: "doctored-header-abi edits only the minor number, to exactly the original plus one, and still ends in status 34"
    requirement: VER-05
    verification:
      - kind: ctest
        ref: "installed.controls.doctored-header-abi (log line: doctored ABI minor: 1 -> 2)"
        status: pass
    human_judgment: false
  - id: D3
    description: "the closed-pipes deadline test no longer races Python guest start-up (F-129)"
    requirement: VER-05
    verification:
      - kind: ctest
        ref: "harness.rsp-run; 20 of 20 consecutive runs at natural load and 20 of 20 under a 0.3 s start-up delay"
        status: pass
    human_judgment: false
  - id: D4
    description: "origin/main is green: the hosted pair of the pushed head passes on gate-linux and gate-macos"
    requirement: VER-05
    verification:
      - kind: command
        ref: "gh run view 45770631197 (conclusion success, 324 of 324 names on both jobs)"
        status: pass
    human_judgment: false

duration: 45min
completed: 2026-11-26
---

# Phase 11 Plan 28: Version controls on both CMake majors, doctored ABI minor, rsp-run readiness clock Summary

**The two version-mismatch controls now expect the refusal headline that CMake 3.31 and 4 both print, the ABI doctoring raises the minor by exactly one, the F-129 flake is removed with a readiness clock, and the fix is on origin/main with a green hosted pair (gate-linux 362 s, gate-macos 538 s).**

## Performance

- **Duration:** about 45 min wall, of which about 15 min waited for unrelated host load (a VM and an Erlang workload held the 1-minute load average above 9) before the gate could start
- **Tasks:** 3 (all `type="auto"`, two with `tdd="true"`)
- **Files modified:** 6 (3 code/test, 3 records)

## Accomplishments

- **I-11 (blocking, F-130).** The hosted Linux runner has cmake 3.31.6, which prints the refusal headline but not the CMake-4 reason line the rows expected. The failing text was reproduced from the failed-job log with the driver's own judge (RED: the old text absent from the hosted output of both controls). The new expected column is `compatible with requested version "0.2"` and `"0.1.5"`: present in the hosted 3.31.6 text and the local 4.4.3 log of each control, absent from the `missing-package` log (T-11-68), with `considered but not accepted ... version: 0.1.0` in each version log (found and refused). Only the expected column of the two rows changed (plus a three-line comment in the file header).
- **I-01.** The awk strips `UINT32_C(` and `)` before the arithmetic (it kept the `32` and wrote 322), and `ctl_doctored` asserts original plus one before building; the log reads `doctored ABI minor: 1 -> 2 (exactly the original plus one)` and the control still observes status 34. The assertion applied to the old result (322) fails. The 11-19 record is corrected in PIN-IMPACT "11-28".
- **I-07 (F-129).** `test_closed_pipes_do_not_end_the_outer_deadline` now closes the guest's pipes, then publishes the batch file (its readiness), and patches the supervisor's `time` with a `ReadinessClock` whose `monotonic()` stays at zero until that file exists, so the 0.1 s deadline is measured from readiness. Reproduced without load by a scratch `sitecustomize` delaying every interpreter start by 0.3 s: old test 5 of 5 red at `assertIsNotNone(rejected_batch_path)` (the hosted failure); new test 20 of 20 green with the delay and 20 of 20 at natural load; a mutant supervisor that ends supervision when the pipes close still fails the new test.
- **Push and hosted pair.** Pinned SHA `0e6b96dd8c24734ce433b6739ea817a5190e2997`, fast-forward from `463bd09`, four commits, no tag, no force. Run 45770631197: success, 324 of 324 names on both jobs, no rerun.

## Task Commits

1. **Task 1: version controls on cmake 3.31 and 4, doctored-header edit exact** - `9e5acee` (fix)
2. **Task 2: F-129 readiness clock in test_rsp_run.py** - `0e6b96d` (fix)
3. **Task 3: gate, push, hosted pair, records** - the push (no commit of its own) and `c69b6c5` (docs: LOG.md rows Runs, F-131, F-132; PIN-IMPACT 11-28 section)

## Hosted measurement (run 45770631197, head `0e6b96d`, warm: Binutils cache hit on both jobs)

| Job | cmake | queue s | job s | Gate step s | ctest s | slowest named step |
|-----|-------|---------|-------|-------------|---------|--------------------|
| gate-linux | 3.31.6 | 2 | 362 | 300 | 208 | Gate 300 s |
| gate-macos | 4.5.0 | 3 | 538 | 470 | 282 | Gate 470 s |

Wall 542 s, critical path 541 s. The names this plan touched: `installed.controls.version-mismatch-minor` and `-patch` 0.22 s and 0.22 s on Linux, 0.89 s and 1.00 s on macOS; `doctored-header-abi` 0.40 s and 1.44 s; `harness.rsp-run` 0.89 s and 1.14 s.

## Files Created/Modified

- `tests/installed-consumer/controls.tsv` - expected column of the two version rows; header comment on why
- `scripts/check-install.sh` - doctoring awk, `ctl_abi_minor`, minor-plus-one assertion
- `scripts/tests/test_rsp_run.py` - `import time`, `ReadinessClock`, the closed-pipes test only
- `docs/ci-reviews/LOG.md` - Runs row for 45770631197, findings F-131 (resolves F-130) and F-132 (resolves F-129); append-only, no existing row edited
- `PIN-IMPACT.md` - section "11-28" with the corrected 11-19 record, RED and GREEN evidence, preflight note, hosted seconds
- `deferred-items.md` - the preflight head-ci observation (below)

## Decisions Made

See `key-decisions` in the frontmatter. In short: a per-argument headline both CMake majors print; the minor-plus-one assertion in the driver; a clock patch (not a longer sleep) for F-129; no edit of the preflight script.

## Deviations from Plan

### Auto-fixed Issues

None.

### Process deviations

**1. [Rule 3 - Blocking] Preflight `head-ci` fails on the red head this plan repairs**
- **Found during:** Task 3 (before the gate)
- **Issue:** `sh scripts/preflight-phase11.sh` ends in `head-ci FAILED` (origin/main `463bd09` has a completed failure) and exits before `timeout-audit`, `pii`, `linux-compile` and the replay run. The step guards against long native work on a red head; here the red head is the work.
- **Fix:** the script was not edited. The real script was run once (oracle-authority, ctest-counts, abi-cheap, guards pass, head-ci fails as designed, output kept in the ignored build tree). The later steps ran from a scratch copy without the single `step head-ci` line (ROOT fixed; ignored build tree): `--no-replay` PASS 7 s, then with replay on the clean committed tree PASS 36 s (timeout-audit 177 TIMEOUTs, 0 too low; pii; linux-compile skipped, no C change; 11 ci.yml steps replayed). After the push the real script passes `head-ci` on the green origin/main (`sh scripts/preflight-phase11.sh --no-replay`: PASS).
- **Files modified:** none committed
- **Commit:** none

**2. Host load wait.** The full gate was held until the 1-minute load average was below 9 on two samples 10 s apart (it was 9 to 21 for about 15 minutes from an unrelated VM and Erlang workload); the gate then passed (324 of 324, 161 s) while the load rose to about 15. No limit was widened.

**Total deviations:** 1 recorded (Rule 3, process) plus the load wait. **Impact:** none on the delivered evidence; the preflight coverage was complete and the unedited script passes on the green head.

## Authentication Gates

None.

## Issues Encountered

- `sed` on macOS treats `\t` inside a bracket expression as a literal backslash and `t`, so a first attempt to doctor a TSV row for a log-keeping run did not match; the rows were rewritten with `awk -F` and the original restored byte for byte (checked with `cmp`). No committed file was affected.

## Verification

| Check | Result |
|-------|--------|
| `cmake --build --preset gate && ctest --preset gate -R '^installed\.controls\.'` | 11 of 11 pass (10 controls plus `install.setup`) |
| `python3 -B scripts/tests/test_rsp_run.py -v` and `ctest -R '^harness\.rsp-run$'` | 12 tests OK; 1 of 1 |
| Task 2 loop | 20 of 20 at natural load; 20 of 20 under the 0.3 s start-up delay; old test 5 of 5 red under the delay |
| `sh scripts/gate.sh` at `0e6b96d` | PASS, 324 of 324, ctest 94 s, total 161 s |
| `sh scripts/pii-scan.sh --tree` and `--range origin/main..HEAD` | clean (1423 files; 4 commits) before the push; tree clean again before every later commit |
| Hosted run 45770631197 | success on gate-linux and gate-macos, 324 of 324 each |
| `gh run list --branch main --limit 1` conclusion | success |
| `sh scripts/preflight-phase11.sh --no-replay` after the push | PASS (head-ci ok on `0e6b96d`) |

## Known Stubs

None.

## Threat Flags

None. The version rows are matched against refusal text only; T-11-68 is mitigated by the absence check against the missing-package log recorded in PIN-IMPACT.

## Notes for the orchestrator

- **Gate growth (for the cold measurement at 11-25).** No name was added, yet the warm gate-macos job is 538 s against 487 s in F-130 (ctest 282 s against 291 s, test-inventory-selftest 69 s against 59 s). The F-130 projection that adds the F-123 to F-124 cold delta (346 s) to the warm job now reads 884 s (margin 16 s); the other three projections do not use the warm sample. Recorded in F-131 and PIN-IMPACT; no limit widened.
- **Order.** This plan ran before the rest of 11-21's Task 3. STATE keeps 11-21 (the INSTALL ledger row, the Freeze base SHA and the 11-21 SUMMARY) as the next plan; 11-29 (I-02, I-03, I-04 text fixes) is next after that per the review order. Local unpushed docs commits: `c69b6c5` and this plan's closeout commits (they go out at the next push a plan names).
- **Preflight head-ci.** A future plan that repairs a red head will meet the same by-design failure; see `deferred-items.md`.

## Self-Check: PASSED

- `scripts/check-install.sh`, `tests/installed-consumer/controls.tsv`, `scripts/tests/test_rsp_run.py`, `docs/ci-reviews/LOG.md`, PIN-IMPACT.md and deferred-items.md exist and are committed
- commits `9e5acee`, `0e6b96d`, `c69b6c5` are ancestors of HEAD; `0e6b96d` equals origin/main
- the hosted conclusion of run 45770631197 is success on both jobs
