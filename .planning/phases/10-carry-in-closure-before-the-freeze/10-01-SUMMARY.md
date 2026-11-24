---
phase: 10-carry-in-closure-before-the-freeze
plan: 01
subsystem: infra
tags: [preflight, act, ctest, mutants, pin-impact, posix-sh, gate]

requires:
  - phase: 09-controller-save-media-and-replay
    provides: the Phase 9 lessons (F-102 act compile, F-107 post-Gate replay, F-108 TIMEOUT audit, F-113 retained-original rule), scripts/k5-mutants.py, the si-v1 family and the CI review log
provides:
  - scripts/preflight-phase10.sh, the scripted local preflight (8 steps, 23 s without a C change, 57 to 70 s with one) that every later Phase 10 plan cites
  - scripts/preflight-phase10-regexes.txt, 14 expected ctest -N -R count rows
  - PIN-IMPACT.md with the pin-impact map, the one-way registered identity record (4 ids, tuple 13/21/26, t64.state.si.26, container v2), the Wave 0 decisions, the Wave 0 base and the Gate-growth ledger baseline
  - scripts/p10-mutants.py (anchors, throwaway worktree, KILLED/SURVIVED/INVALID) with both controls proven, CTest repo.p10-mutant-anchors and the PHASE10_TESTS inventory block
affects: [10-02, 10-03, 10-04, 10-05, 10-06, 10-07, 10-08, 10-09, phase-10 later plans, phase-11]

actuals:
  tokens: 14600
  tasks: 4
  commits: 4

tech-stack:
  added: []
  patterns:
    - "step frame with chained whole-second timestamps so the total equals the step sum"
    - "regex count rows (count TAB regex) checked with ctest -N -R before any verify command is trusted"
    - "post-Gate replay read from ci.yml itself, fresh run id, snapshot-diff cleanup of only this run's retained outputs"
    - "mutant driver table with an equivalent control (SURVIVED) and a non-compiling control (INVALID)"

key-files:
  created:
    - scripts/preflight-phase10.sh
    - scripts/preflight-phase10-regexes.txt
    - scripts/p10-mutants.py
    - .planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md
  modified:
    - runner/CMakeLists.txt
    - scripts/check-test-inventory.sh

key-decisions:
  - "Registered ids ntsc-cart-4m-openipl3-rdram-b-t1-si-v2 with -e4k, -e16k and -s32k (38, 42, 43, 43 characters), tuple descriptor 13, model 21, state 26, digest domain t64.state.si.26, container version word 2 and format string t64.state-container.v2 are recorded in PIN-IMPACT.md before any digest pin (one-way once plan 10-09 pins a digest)"
  - "Wave 0 decisions: no new hosted CI step, no Phase 10 JUnit root in gate.sh, GATE-02 keeps whole-file hashes of the other 13 bound files with new tests in new files, the rsp-no-hle guard removal is measured last in 10-04 and declined if it needs a qualify-audio.py edit, the make -j2 change is declined"
  - "The preflight reads the post-Gate run lines from ci.yml (anchored on step names) instead of copying them, so the replay cannot drift from the workflow"
  - "p10-mutants.py counts only tracked changes outside .planning/ as a dirty tree, because planning records cannot affect a mutant built from HEAD"

patterns-established:
  - "Every Phase 10 plan cites sh scripts/preflight-phase10.sh; mid-task runs use --no-replay, pre-push runs use a clean committed tree"
  - "Each new Phase 10 CTest name is added to PHASE10_TESTS, carries the label phase10 and gets a regex count row"

requirements-completed: [GATE-01, VID-07, VER-06]

coverage:
  - id: D1
    description: "Scripted local preflight (oracle authority, ctest counts, host load, head CI, TIMEOUT audit, PII, act-image Linux compile, post-Gate replay) with per-step and total seconds, re-runnable, total at most 90 s"
    requirement: "GATE-01"
    verification:
      - kind: integration
        ref: "sh scripts/preflight-phase10.sh (PASS, 8 step ok lines, total 57 s with a C change, 23 s without; two consecutive runs pass)"
        status: pass
    human_judgment: false
  - id: D2
    description: "Regex count rows for every Phase 10 verify regex, failing on a seeded mismatch, plus the phase10 label check against the PHASE10_TESTS block"
    requirement: "GATE-01"
    verification:
      - kind: integration
        ref: "T64_PREFLIGHT_REGEXES=<scratch copy with one count edited> sh scripts/preflight-phase10.sh (exit 1, count mismatch line)"
        status: pass
    human_judgment: false
  - id: D3
    description: "PIN-IMPACT.md: pin-impact map, one-way identity record, Wave 0 decisions, Wave 0 base and Gate-growth ledger baseline recorded before any digest pin"
    requirement: "VID-07"
    verification:
      - kind: other
        ref: "grep -c ntsc-cart-4m-openipl3-rdram-b-t1-si-v2 PIN-IMPACT.md (4); base extraction idiom (one 40-hex ancestor commit); sh scripts/pii-scan.sh --tree (0 findings)"
        status: pass
    human_judgment: false
  - id: D4
    description: "Phase 10 mutant driver whose equivalent control is classified SURVIVED and whose non-compiling control is classified INVALID, with an anchor check and a dirty-tree refusal"
    requirement: "VER-06"
    verification:
      - kind: integration
        ref: "python3 -B scripts/p10-mutants.py --check-anchors (0); python3 -B scripts/p10-mutants.py --run (control-equivalent-depth-cap SURVIVED, control-no-compile INVALID)"
        status: pass
    human_judgment: false
  - id: D5
    description: "CTest repo.p10-mutant-anchors with label phase10 and the PHASE10_TESTS inventory block"
    requirement: "VER-06"
    verification:
      - kind: unit
        ref: "ctest --test-dir build/gate -N -R '^repo\\.p10-mutant-anchors$' (1); sh scripts/check-test-inventory.sh --preset gate and --selftest (PASS, 225 names); ctest --preset gate (225 of 225 passed)"
        status: pass
    human_judgment: false

duration: 23min
completed: 2026-11-23
status: complete
commits: 4
plan_head_before: 4ac633e880e0c26005cb03092e8d8485e721034c
plan_head_after: 3d504803b066f8a4ce2db64353f1d4295cdd36f4
---

# Phase 10 Plan 01: Scripted preflight, pin-impact map and mutant driver Summary

**One 8-step POSIX-sh preflight (act-image Linux compile, ci.yml post-Gate replay, ctest -N count rows, TIMEOUT audit, oracle check, PII scan, head CI, host load) at 23 s without a C change, with the one-way registered si-v2 identity record committed before any digest pin and a k5-style mutant driver whose two controls classify correctly.**

## Performance

- **Duration:** 23 min
- **Started:** 2026-11-24T00:30:34Z
- **Completed:** 2026-11-24T00:54:05Z
- **Tasks:** 4
- **Files modified:** 6 (4 created, 2 modified)

## Accomplishments

- `scripts/preflight-phase10.sh` runs oracle-authority, ctest-counts (14 rows plus the phase10 label check), host-load, head-ci, timeout-audit, pii, linux-compile and post-gate-replay and prints one `[preflight]` line per step, a summary and `[preflight] total Ns`. Measured: 23 s, 23 s and 24 s with no C change; 57 s with a CMakeLists change since the merge base; 70 s with a forced base that includes `core/t64_digest.c` (219 units compiled with the act image cc 13.3.0 in 35 to 47 s); 2 s with `--no-replay`. Host load was above the script's WARN threshold of 8 in every run, so these are upper bounds. Each step starts where the previous one ended, so the total equals the step sum from one `date +%s` source.
- A seeded count mismatch (one edited count in a scratch regex file via `T64_PREFLIGHT_REGEXES`) makes the preflight exit 1 with `count mismatch: ... expected 7 got 6`; a malformed row and a seeded TIMEOUT below 3.5 times the recorded seconds each fail their step.
- `PIN-IMPACT.md` records the pin-impact map, the four new registered ids with their lengths, the tuple 13/21/26, the digest domain `t64.state.si.26`, container version word 2 and format string `t64.state-container.v2`, the Wave 0 decisions with one reason each, the preflight measurements, the Wave 0 base (`4ac633e880e0c26005cb03092e8d8485e721034c`, parent of the first Phase 10 commit) and the Gate-growth ledger baseline (224 names, 99.7 s native at host load 13 to 16, hosted macOS ctest 251 s and 215 s, cold projection 673 s against 900 s).
- `scripts/p10-mutants.py` has `--list`, `--check-anchors` and `--run`; its equivalent control (primitive Z cap written `>= 0x8000`) is SURVIVED and its `#error` control is INVALID; it refuses a dirty tree with exit 2. CTest `repo.p10-mutant-anchors` (labels `runner;phase10`, TIMEOUT 60) and the `PHASE10_TESTS` block bring the gate to 225 names, all passing (76.5 s).

## Task Commits

Each task was committed atomically:

1. **Task 1: Preflight tracer** - `c8c820e` (feat); its parent is the recorded Wave 0 base
2. **Task 2: Full Phase 9 lesson list** - `0f3bde8` (feat)
3. **Task 3: Pin-impact map, one-way ids, Wave 0 decisions** - `5a336f6` (docs)
4. **Task 4: Mutant driver, anchor CTest, inventory block** - `3d50480` (feat)

**Plan metadata:** this SUMMARY and the STATE, ROADMAP and REQUIREMENTS updates follow in docs(10-01) commits.

## Files Created/Modified

- `scripts/preflight-phase10.sh` - the preflight (POSIX sh, no new dependency; flags `--no-replay` and `--base REF`)
- `scripts/preflight-phase10-regexes.txt` - expected `ctest -N -R` counts, one `count<TAB>regex` row per verify regex
- `.planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md` - impact map, identity record, decisions, measurements, wave bases, ledger
- `scripts/p10-mutants.py` - Phase 10 mutant driver (controls only; later plans append rows)
- `runner/CMakeLists.txt` - registers `repo.p10-mutant-anchors`
- `scripts/check-test-inventory.sh` - `PHASE10_TESTS` block and join entry

## Decisions Made

- Recorded the one-way identity set before any pin (see key-decisions); no checkpoint, it was settled by CONTEXT "Identity naming".
- Took the post-Gate replay lines from `ci.yml` (job gate-macos, steps "Run authored audio qualification records" to "PII scan authored Phase 9 evidence", 11 lines) rather than copying them; the step fails with an explicit message when an anchor name disappears.
- The driver's controls use the bound `core/tests/test_rdp_depth.c` read-only (compiled in the throwaway worktree, never edited) and verify the reused `build/fixtures/rdp-fill.z64` against its `fixtures/manifest.json` row.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Mutant driver dirty-tree check ignores `.planning/`**
- **Found during:** Task 4 (the first `--run`)
- **Issue:** the copied k5 check refuses any modified tracked file, but the orchestrator's uncommitted `.planning/STATE.md` and `.planning/config.json` (and the executor's own STATE.md update before its metadata commit in sequential mode) would make every `--run` exit 2, although planning records cannot affect a mutant built from HEAD.
- **Fix:** `git status --porcelain --untracked-files=no -- . ':(exclude).planning'`. A modified tracked file outside `.planning/` still gives exit 2 (tested with a temporary `.gitignore` edit, restored with `git checkout -- .gitignore`).
- **Files modified:** `scripts/p10-mutants.py`
- **Verification:** `--run` passes with planning-only modifications; exit 2 with a source modification
- **Committed in:** `3d50480`

**2. [Rule 2 - Missing critical] Replay cleanup covers the retained records the replay actually creates**
- **Found during:** Task 2 (replay measured with a timestamp marker)
- **Issue:** the plan names the `build/evidence/combined/gate-macos` tree and the artifact-prefix run directories, but the 11 steps also create `build/evidence/gate-macos.json`, one private record each under `build/gate/08-05-audio-private` and `build/gate/09-11-phase09-private`, and a `build/gate/07-qualification/hosted-gate-macos-*` directory. Leaving them would grow retained directories and, for a repeated run id, trip the F-113 retained-original refusal.
- **Fix:** a listing is taken of the three retained roots before the replay; afterwards only entries that are new and name the replay job are removed, together with the evidence files and the prefix-named files. Entries created by CTest tests during the replay (for example `09-03-evidence/run.*`) are not touched.
- **Files modified:** `scripts/preflight-phase10.sh`
- **Verification:** two consecutive runs leave all three roots byte-identical to their before listings (`diff` empty) and `build/evidence` absent
- **Committed in:** `0f3bde8`

**3. [Rule 2 - Missing critical] Failure hint when the replay meets a dirty bound source**
- **Found during:** Task 4 (a preflight run with uncommitted `runner/CMakeLists.txt` and inventory edits)
- **Issue:** the emit step `qualify-audio.py --emit-hosted` refuses with "hosted source inputs differ from the selected clean commit" when a bound source is modified, which the original failure text mislabeled as a hosted-only defect.
- **Fix:** the failure prints the number of modified tracked files outside `.planning/` and tells the user to commit them or use `--no-replay` mid-task; the script header states that the replay runs on a clean tree.
- **Files modified:** `scripts/preflight-phase10.sh`
- **Verification:** the same run on the committed tree passes (replay 20 s)
- **Committed in:** `3d50480`

---

**Total deviations:** 3 auto-fixed (1 Rule 3 blocking, 2 Rule 2 missing critical)
**Impact on plan:** all three are inside the files the plan names; none widens scope or touches an oracle-bound file.

## Issues Encountered

- Host load stayed between 8.9 and 16 during the whole plan (other agent sessions on the same machine), so every timing is an upper bound for a quiet host; the preflight prints a WARN line in that case.
- `origin/main` (`612a3ad`) has no hosted `ci.yml` run (planning commits were `[skip ci]`); the newest hosted run on main is `928c9d8`, completed, success. `head-ci` reports both facts and does not fail.
- Observation for plan 10-06 (not a defect): in a scratch run, a mutant lowering the primitive Z cap from `0x7fff` to `0x7ffe` was KILLED by the existing `test_rdp_depth` binary, so that VG-01 mutant is not a survivor of the existing test; the new test's own named check must be the kill that is recorded for it.

## Authentication Gates

None.

## User Setup Required

None - no external service configuration required.

## Known Stubs

None.

## Threat Flags

None. T-10-01 is mitigated (the pii step, removal of this run's replay outputs only, PII scan before each commit; 0 findings); T-10-02 is mitigated (edits only in `build/10-mutants/wt`, dirty-tree refusal tested, controls classified correctly); the act image container runs with `--pull never`, `--network none` and a read-only mount.

## Next Phase Readiness

- Ready for plan 10-02 (digest sweep tool and K1 characterization). Cite `sh scripts/preflight-phase10.sh` (use `--no-replay` mid-task and a clean committed tree before a push); each new Phase 10 CTest name goes into `PHASE10_TESTS`, carries the label `phase10` and gets a count row in `scripts/preflight-phase10-regexes.txt`.
- The base for later waves: `PIN-IMPACT.md` "Wave bases" holds the Wave 0 record only; 10-05, 10-08, 10-13 and 10-18 append theirs.
- No blockers. The tracked `.planning/STATE.md` and `.planning/config.json` modifications that were present at start belong to the orchestrator; only STATE.md is committed by this plan's metadata commit.

---
*Phase: 10-carry-in-closure-before-the-freeze*
*Completed: 2026-11-23*

## Self-Check: PASSED

All created and modified files exist, the four task commits (`c8c820e`, `0f3bde8`, `5a336f6`, `3d50480`) are ancestors of HEAD, the evaluation scope lists them, and the plan-level checks passed on the committed tree: `sh scripts/preflight-phase10.sh` PASS (57 s), `python3 -B scripts/p10-mutants.py --check-anchors` 0, `--run` classified SURVIVED and INVALID, `sh scripts/check-test-inventory.sh --preset gate` and `--selftest` PASS, `ctest --preset gate` 225 of 225 passed, `sh scripts/pii-scan.sh --tree` 0 findings.
