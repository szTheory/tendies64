---
phase: 09-controller-save-media-and-replay
plan: "10"
subsystem: verification
tags: [independent-review, security-audit, goal-backward-verification, fix-cycle, freeze-record, native-rehearsal, hosted-rehearsal, ci-timing]

requires:
  - phase: 09-controller-save-media-and-replay
    provides: plans 09-01 to 09-09.1 (SI and controller, save devices, import and export, scheduler, replay, state container and importer) and the FH tag phase09-fh
provides:
  - tracer --run-mutant (observer-range) through the real runner with an independent re-signer
  - drift reconciliation and the option-pair, status, flag and row verdicts of Task 2
  - 09-REVIEW.md (52 findings, five lanes), 09-SECURITY.md (SECURED, 86 of 86), 09-VERIFICATION.md (passed 13 of 13 after one gaps-only re-verification)
  - 09-REVIEW-FIX.md (triage, FX-01 to FX-39, DEV-01 to DEV-17, FD-01 to FD-06, golden record, FR freeze record, hosted rehearsal result)
  - docs/evidence/phase09-success-criteria.md (draft map, completed by 09-11)
  - the local tag phase09-fr on 8dd6af1cda44fe021fcc4b42c4103ffd5148c986 (not pushed)
  - a native rehearsal record and a hosted rehearsal pair, both verified, neither qualification evidence
affects: [09-11, phase-10]

actuals:
  tokens: 227900   # chars/4 over git diff d4ccad5..b57ca55 (911,571 characters, 61 files, 9,046 insertions, 536 deletions)
  tasks: 6
  commits: 67      # MEASURED: git rev-list --count d4ccad5facc2c2059e2d6488aeaaa80cd42557f1..HEAD at write time (docs commits of the closeout follow)

plan_head_before: d4ccad5facc2c2059e2d6488aeaaa80cd42557f1
plan_head_after: b57ca55803c1df342f637535a8dc1734e5c04565

tech-stack:
  added: []
  patterns:
    - "independent reviewer, auditor and verifier agents; the executor adds only the scope manifest, lane table and criteria map"
    - "RED first per fix with an accepted baseline of the same setup, isolated pin refreshes"
    - "cheap-first checks, then one native rehearsal, then one hosted rehearsal, then the FR record and local tag"

key-files:
  created:
    - .planning/phases/09-controller-save-media-and-replay/09-REVIEW.md
    - .planning/phases/09-controller-save-media-and-replay/09-REVIEW-FIX.md
    - .planning/phases/09-controller-save-media-and-replay/09-SECURITY.md
    - .planning/phases/09-controller-save-media-and-replay/09-VERIFICATION.md
    - docs/evidence/phase09-success-criteria.md
    - runner/tests/t64_test_tree.h
  modified:
    - runner/tests/test_state_runner.c
    - runner/tests/test_pad_poll.c
    - runner/CMakeLists.txt
    - scripts/tests/test_phase09_qualification.py
    - docs/ci-reviews/LOG.md

key-decisions:
  - "The RDP combiner block and memory coverage table are carried in the composite family state (STATE-CONTENT-F1, DEV-02); the 17 pre-Phase-9 ids did not move"
  - "Six frozen-file logic changes are declared FD-01 to FD-06; the hosted rehearsal of Task 6 is their hosted evidence"
  - "The hosted macOS cold projection of 619 s against the 600 s gate is accepted as a recorded deviation (DEV-17), no third candidate cycle and no frozen ci.yml edit, with a Binutils cache freshness obligation for 09-11"
  - "No requirement is marked complete here; the claims belong to 09-11"

patterns-established:
  - "Replay every hosted step after Gate against the local build before pushing a change to a retained-evidence producer (F-107)"
  - "Audit every registered TIMEOUT against the largest native, sanitizer and hosted seconds before the first push (F-108)"

requirements-completed: []   # SAVE-01..04, CORE-02, CORE-04, CORE-05 are claimed by 09-11, not here

coverage:
  - id: D1
    description: "Real hash-valid hostile state blob refused by the real runner with the inventory's own reason after the unmutated baseline passes (tracer)"
    verification:
      - kind: integration
        ref: "runner/tests/test_state_runner.c --run-mutant none and observer-range; build/gate/09-10-tracer.txt"
        status: pass
    human_judgment: false
  - id: D2
    description: "Independent deep review, security audit and goal-backward verification on file, one fix cycle closing every blocking item"
    verification:
      - kind: other
        ref: "09-REVIEW.md, 09-SECURITY.md (SECURED), 09-VERIFICATION.md (passed 13/13), 09-REVIEW-FIX.md FX-01 to FX-39"
        status: pass
    human_judgment: false
  - id: D3
    description: "Full native rehearsal (normal gate, full ASan and UBSan, gcc:13 closure, full continuation matrix step) on the candidate"
    verification:
      - kind: other
        ref: "python3 -B scripts/qualify-phase09.py --native --rehearsal --sha 8dd6af1... exit 0, 1,486 s; standalone record verification"
        status: pass
    human_judgment: false
  - id: D4
    description: "Hosted rehearsal of the same commit on both jobs, collected and verified against the native record"
    verification:
      - kind: other
        ref: "run 44790405712 attempt 1; --collect-hosted --rehearsal and --verify-evidence --rehearsal exit 0"
        status: pass
    human_judgment: false
  - id: D5
    description: "FR freeze record and local tag phase09-fr; nothing under the frozen trees differs between the tag and HEAD"
    verification:
      - kind: other
        ref: "git diff --quiet phase09-fr HEAD -- core runner tools scripts .github fixtures"
        status: pass
    human_judgment: false

duration: "Task 6 about 1 h 45 min of executor time over three candidates; the plan ran from 2026-11-20T00:02-05:00 (plan base d4ccad5) to the closeout the same day"
completed: 2026-11-20
status: complete
---

# Phase 09 Plan 10: Independent review, audit, verification, one fix cycle, the freeze record and the rehearsals

**An independent deep review (52 findings), a security audit (SECURED, 86 of 86 threats) and a goal-backward verification (passed 13 of 13 after the single gaps-only re-verification) landed before the freeze; one 39-entry fix cycle closed every blocking item; the fixed candidate `8dd6af1` passed the full native rehearsal (1,486 s) and a hosted rehearsal (both jobs green, collector and verifier pass) with one accepted hosted time deviation, and is marked by the local tag `phase09-fr`.**

## Performance

- **Plan span:** 2026-11-20 from the plan base `d4ccad5` (00:02 local) to this closeout, with the review, audit and verifier agents and four fix groups run by the orchestrator between executor sessions.
- **Task 6 (this executor):** about 1 h 45 min over three candidate commits; native rehearsals 1,652 s and 1,486 s (plus one stopped start of about 6 min), three hosted runs.
- **Tasks:** 6 (task 1 tracer, task 2 reconciliation, task 3 review and audit, task 4 criteria map and verification, task 5 the fix cycle and re-checks, task 6 the rehearsals and the freeze record).
- **Commits:** 67 measured from the ledger base (see frontmatter); the closeout docs commits follow.

## Accomplishments

- **Tracer (Task 1).** `runner/test_state_runner --run-mutant ROW` exports a state from the long admit fixture through the real supervised runner, applies the `observer-range` row of `docs/spec/state-mutants.txt` with an independent re-signer, imports it through the real runner and prints one line. `none` expects the resumed baseline of the same setup (outcome `resumed`, guest tick 838,791, blob 8,448,755 bytes); `observer-range` ends refused at guest tick 0 with reason `OBSERVER_RANGE` (rule 16, `LEN_LE`), blob SHA-256 `7b337637...5f84c`. RED stub `0b84bed`, GREEN `cc0096a`. Freeze baseline against `phase09-fh` before any fix: 17 files, 13 identical, 3 DATA, 1 LOGIC (the file recorded as FD-01). Gate baseline 224 names, Phase 9 names 45, no ASan-only name.
- **Reconciliation and drift (Task 2, `935a483`).** No tracked file names the nonexistent `t64_machine_replace` (real entry `t64_machine_admit_rom`); the status table is contiguous 0 to 33 and equals every documented table; `docs/spec`, `09-VALIDATION.md` and the child-only option names reconciled. Drift sweep findings R-01 to R-06 (synthetic-mode refusal of the 19 flags, nine untested pairs, six undefined INPUT combinations, `--save-import-fd` direct test, a duplicated saturating increment, the slot-sum wording) all went to the fix cycle and closed (FX-13, FX-14, FX-09). Verdicts: the production-event mid-tick export and import row **PRESENT-GREEN** (`mid-tick-resume`, 1,875 checks); the `write_generation` row **PRESENT-GREEN** (`save-media-roundtrip`), its two increment sites unified by FX-09 and the saturation control added by FX-26. Option-pair matrix: nine families, 36 pairs, plan statement, supervisor behavior and test agree per pair after FX-13 and FX-14 (the unit that asserts the rows against the spec table is FX-33).
- **Review, audit, verification (Tasks 3 and 4).** `09-REVIEW.md`: 105 files, five lanes, 52 findings, 5 critical, 15 warning, 32 info. By lane (critical, warning, info): L1 core device models 1/2/5, L2 state container and importer 2/4/6, L3 runner 0/3/6, L4 harness, guards and CI 1/2/6, L5 tests, fixtures and specs 1/4/9. First audit `OPEN_THREATS` with four open high threats (T-09-67, T-09-73, T-09-82, T-09-87). First verifier report `gaps_found`, 6 of 13, gaps VG-01 to VG-11, pre-fix covered digest `v4:sha256:7b7b070b...`. Draft criteria map `docs/evidence/phase09-success-criteria.md` (`6963bf0`).
- **One fix cycle (Task 5).** Blocking set triaged before any fix (`7672aad`), bound raised to four executor groups (DEV-01), FX-01 to FX-37 across G1 to G4, one commit per finding, RED first with an accepted baseline of the same setup, pin refreshes isolated (`45b61d2`), no new CTest name. Targeted re-check: 22 findings (0 critical, 5 warning, 17 info), 14 fixed, 8 recorded with an owner; security re-audit **SECURED**, 86 of 86 threats closed, `threats_open` 0, T-09-88 to T-09-90 found and closed; gaps-only re-verification **passed 13 of 13**, no gap remaining, final covered digest `v4:sha256:955f5003487a7aa0743cc08beef7cbe2a0f58ecde5b31950c9a5f8634db6de43` (137 files, HEAD `944443f`).
- **Task 6 rehearsals.** Two defects were found and repaired on the way (FX-38, FX-39, below). The candidate `8dd6af1` passed the cheap-first checks, the full native rehearsal and a hosted rehearsal whose pair the collector and the rehearsal verifier accept. The FR record is written and the tag is on `8dd6af1`.

## Task Commits

1. **Task 1** tracer: `0b84bed` (RED stub), `cc0096a` (GREEN).
2. **Task 2** reconciliation: `935a483`.
3. **Task 3** review and audit: `2bac6f6`; criteria map draft `6963bf0` (Task 4 step 1).
4. **Task 4** verification draft (gaps_found, 6 of 13): `7d8f817`.
5. **Task 5** fix cycle: triage `7672aad`; G1 `a7e125f` FX-01, `45b61d2` (N1 pin refresh), `82450b0` FX-02, `23e3495` FX-03 (+ `20d2bb4`), `0903cf1` FX-04, `916055f` FX-05, `96409d2` FX-06, `6e7cee7` FX-07, `0ec7019` FX-08, `2fdd2dd` FX-09, record `3fb9f04`; G2 `07e87ba` FX-10, `98d1b57` FX-11, `e68db4c` FX-12, `edf2651` FX-13 (+ `0a13f91`), `b4a8c44` FX-14, `226c9cd` FX-15, `362cdb2` FX-16, `0cfa3b7` FX-17, `66f098f` FX-18, records `f364cb2`, `d593dd0`; G3 `9a35431` FX-19, `54b08a7` FX-20 (+ `becb1cd`), `a209c37` FX-21 (+ `c29e63b`), `a3ceecb` FX-22, `846093b` FX-23, record `7de85af`; G4 `25fc5bd` FX-24, `ed42269` FX-25, `37ec1b1` FX-26, `d0ac32e` FX-27, `17a99de` FX-28, `7d0ca21` FX-29 (+ `fa43a09`), `1fe9191` FX-30, `3c5035e` FX-31, `ef6a48c` FX-32, `284012d` FX-33, `0c55ca9` FX-34, `59620c8` FX-35, `4937d7f` FX-36, `9271189` FX-37, record `944443f`; re-verification `12c8937`; CONTEXT amendment K20 `49069a7` (orchestrator).
6. **Task 6** rehearsals and freeze record: `0d31e0e` (FR record), `c3bdff5` FX-38, `16849e7` (record), `ba477e2` FX-39, `8dd6af1` (record, the rehearsed candidate), `f87c84d` (CI review log), `2338bd9` (hosted result), `b57ca55` (DEV-17 and handoff).

## Files Created/Modified

- Reviews and records under `.planning/phases/09-controller-save-media-and-replay/`: `09-REVIEW.md`, `09-REVIEW-FIX.md` (triage, FX-01 to FX-39, DEV-01 to DEV-17, golden record, FR freeze record, hosted rehearsal result), `09-SECURITY.md`, `09-VERIFICATION.md`.
- `docs/evidence/phase09-success-criteria.md` (draft map, completed by 09-11); `docs/ci-reviews/LOG.md` (runs and F-107 to F-110).
- Task 6 code edits: `runner/tests/t64_test_tree.h`, `runner/tests/test_pad_poll.c`, `scripts/tests/test_phase09_qualification.py` (FX-38), `runner/CMakeLists.txt` (FX-39, four TIMEOUT numbers).
- Untracked build-gate working files: `09-10-freeze-check.py`, `09-10-freeze-baseline.txt`, `09-10-freeze-final.txt`, `09-10-timing.txt`, the drift, tracer, scope and recheck files, and the private rehearsal records under `build/gate/09-11-phase09-private/`.

## Decisions Made

DEV-01 to DEV-17 are recorded in `09-REVIEW-FIX.md`; the ones that bind later work: STATE-CONTENT-F1 (DEV-02), the six FD entries, the TIMEOUT control reading the CMake text (DEV-09), the supervisor contract after commit (DEV-12), the fail-closed digest walker (DEV-13), and the accepted hosted time deviation (DEV-17).

## Deviations from Plan

**DEV-01 to DEV-17** (full text in `09-REVIEW-FIX.md`):

| Id | Class | Summary |
| --- | --- | --- |
| DEV-01 | bound raise | blocking set above 10 commits, cycle run as groups G1 to G4 |
| DEV-02 | STATE-CONTENT-F1 | family state carries the RDP combiner block and coverage table (container +2,097,244 bytes), 17 old ids unchanged |
| DEV-03 | FD-02 | raster function inventory literal of the no-HLE guard (DATA under the corrected table) |
| DEV-04 | FD-03 | publisher transaction, `runner/t64_artifact_publisher.c` and `.h` |
| DEV-05 | FD-04 | `_check_source_route` of the guard pins the SRAM DMA apply and the SI RD64B completion |
| DEV-06 | FD-01 extended | `scripts/tests/test_rsp_no_hle.py`, 73 seeds |
| DEV-07 | FD-05 | `scripts/qualify-phase09.py` limits and the full-matrix native step |
| DEV-08 | FD-06 | `scripts/qualify-rdp-vi.py` five stage limits as constants |
| DEV-09 | control mechanism | TIMEOUT control reads the CMake text, not `ctest --show-only=json-v1` |
| DEV-10 | data edit | sanitizer TIMEOUT of `unit.core.state.import` 1800 to 2400 |
| DEV-11 | outside repository | `build/gate/09-10-freeze-check.py` classification table (anchored again in Task 6, R3 RC-07) |
| DEV-12 | design choice | supervisor states and pins the contract after the commit |
| DEV-13 | design choice | digest walker fails closed |
| DEV-14 | case renamed | `doc-count-drift`, no CTest name changed |
| DEV-15 | Task 6 defect | FX-38: FX-12's cleanup scope removed the retained evidence the hosted emit reads |
| DEV-16 | Task 6 hosted failure | FX-39: four registered TIMEOUTs below twice the measured seconds, one fired on hosted macOS |
| DEV-17 | accepted time deviation | hosted macOS cold projection 619 s against 600 s |

**Task 6 specific, beyond the DEV list:**

- **[Rule 1 - Bug] FX-38.** The first hosted run of the fix-cycle head (`49069a7`, run 44776046324, pushed by the orchestrator) failed both jobs at "Emit authored Phase 9 evidence". Cause: FX-12 (`e68db4c`) removed the retained run directories of `runner/tests/test_pad_poll.c` at exit. Fix `c3bdff5`: `t64_test_scope_keep()` called after a retained run, plus a static control inside `harness.phase09-qualification` (no new name). The case and emit lines replay locally in about 2 s and the whole post-Gate tail in about 20 s; the replay was green after the fix and Linux hosted proved it on a real runner. The native start on the earlier candidate (`0d31e0e`) was stopped after about 6 minutes at the sanitizer stage, because the same code was known to fail hosted.
- **[Rule 1 - Bug] FX-39.** Hosted rehearsal run 44784312630 (`16849e7`): gate-linux green; gate-macos failed at Gate when `harness.state-field-use-audit` hit its registered 60 s TIMEOUT (60.06 s, 48.7 s on the same runner earlier, 17.7 to 21.3 s natively). A margin audit of every registered TIMEOUT found four below 2x; raised in `runner/CMakeLists.txt` (data only; read back on both presets): `harness.state-field-use-audit` 60 to 240, `repo.rsp-no-hle` 60 to 240 (47.9 s hosted Linux), `runner.state-export` 240 to 480 (182 s sanitizer), `runner.cpu-diagnostic` 120 to 240 (62.8 s sanitizer). This was the one repeat of steps 2 to 4. The FX-38 episode is not counted as a repeat: no native or hosted rehearsal of this plan had completed when it was found.
- **[Plan wording] `--verify-record`.** The plan asks to run the record verifier "with `--rehearsal`" on the native record. `--verify-record` verifies a retained supervised run (it needs `--build`, `--last-test`, `--junit`), not the native record. The native record is verified by `--collect-hosted` and `--verify-evidence`; I also verified it standalone with the module function (rehearsal label accepted, final-mode refusal shown).
- **[Plan wording] Public output path.** `--out build/gate/09-11-phase09-private/public-rehearsal-10-X` is refused ("public phase09 output must be a fresh directory"); `build/gate/09-10-public-rehearsal-8dd6af1c` was used (the same correction as 09-03.1 deviation 7).
- **[Plan wording] Verify block 1.** `test -d "$NATIVE"` is a file test (the record is a file); run as `test -f`. Block 2 (the timing awk) exits 1 on the macOS line by design under DEV-17.
- **[Plan wording] Ledger and log.** The LOG commit adds Runs rows for the three earlier unlogged completed runs (44653319864, 44678456339, 44713831889) as well, and F-107 to F-110 (the plan names one finding with the figures; the hosted-only regression, the TIMEOUT audit lesson and the native cost are separate findings). One row text was corrected in the unpushed commit before the push.

**Total deviations:** 17 recorded plus the four wording items above; no guard weakened; no frozen file edited outside FD-01 to FD-06; no CTest name added.

## Frozen files and the FD set

`python3 -B build/gate/09-10-freeze-check.py` against `phase09-fh` after Task 6 (`build/gate/09-10-freeze-final.txt`): 17 files, **9 identical, 2 DATA, 6 LOGIC**, no new FD entry. `scripts/build-boot-fixtures.sh`, `.github/workflows/ci.yml`, `scripts/check-ci-yml.sh`, `scripts/gate.sh`, `tools/rights/t64_blobscan.c`, `scripts/check-phase09-cases.py`, `scripts/qualify-audio.py`, `scripts/check-audio-cases.py` and `scripts/collect-boot-evidence.sh` are identical to the tag.

| Id | File | Verdict | Reason |
| --- | --- | --- | --- |
| FD-01 | `scripts/tests/test_rsp_no_hle.py` | LOGIC | 73 mutation seeds, RDRAM-writer helper, two seed repairs |
| FD-02 | `scripts/check-rsp-no-hle.py` (inventory literal) | DATA | two reviewed names and four digests |
| FD-03 | `runner/t64_artifact_publisher.c`, `.h` | LOGIC | post-commit withdrawal and directory-sync proof |
| FD-04 | `scripts/check-rsp-no-hle.py` (`_check_source_route`) | LOGIC | two more bodies pinned, two invalidation tokens |
| FD-05 | `scripts/qualify-phase09.py` | LOGIC | limits as constants, `normal-state-import-full-matrix` |
| FD-06 | `scripts/qualify-rdp-vi.py` | LOGIC | five shared stage limits named |
| (DATA) | `scripts/rights-scan.sh`, `scripts/check-test-inventory.sh` | DATA | selftest cases, 15 test-name rows |

The FR record (frozen tree identities, FD cost, data-only list, open findings with wording impact, name count) is in `09-REVIEW-FIX.md` section "FR freeze record". Tree identities at the tag: `core` 0b022763..., `runner` 0a82dabe..., `fixtures` ea48718a..., `tools` 0f5494fa..., `scripts` 3ccf2ef0..., `.github` 2e26a9d6..., `tools/rights` 1ac25b4f....

## Golden record

Every changed golden line is STATE-CONTENT-F1 (FX-02): family section 8 length 2,103,016 to 4,200,260 (and 2,103,112 to 4,200,356 for the two `all-*` rows), their SHA-256 values, `S8_IDLE_LENGTH`, `STEM_IDLE_DIGEST`, the replay-mix checkpoint and canonical pins re-captured at advance steps 4096 and 7 (equal), the size bounds (lower 6,303,756 to 8,400,908) and the documented sizes (+2,097,244). Unchanged: the idle digests of the 17 pre-Phase-9 ids (792 regression lines identical after FX-02 and again after FX-24), every profile hash, transcript pin and manifest row, so the oracle authority files were never touched. The detailed table is in `09-REVIEW-FIX.md`.

## Native rehearsal on the candidate (exit 0, 1,486 s)

`python3 -B scripts/qualify-phase09.py --native --rehearsal --sha 8dd6af1cda44fe021fcc4b42c4103ffd5148c986`; record `build/gate/09-11-phase09-private/native-rehearsal-10-8dd6af1.../native-record.json`, described everywhere as a rehearsal and not qualification evidence. No tracked file changed during the run. First end-to-end run of the new full-matrix step and the raised limits: no refusal.

| Step | Seconds | Limit |
| --- | --- | --- |
| shared native stage | 1,421.6 | 4,800 |
| normal gate | 136.1 | 600 |
| ASan configure | 0.2 | 120 |
| ASan build | 8.0 | 600 |
| ASan and UBSan full CTest (226 of 226) | 1,184.8 | 3,000 |
| gcc:13 closure (64 units) | 52.5 | 300 |
| `normal-state-import-full-matrix` | 53.0 | 300 |
| 26 inventory checks and 2 source-body checks | 0.2 to 0.5 each | 720, 120 |

Ten slowest Phase 9 names, normal gate JUnit (the scaling base for the hosted figures): `unit.core.state.import` 48.4 s, `harness.state-field-use-audit` 18.6, `runner.state-export` 7.6, `unit.core.sched.matrix` 6.9, `harness.phase09-qualification` 6.8, `unit.replay.suite` 4.5, `runner.save-roundtrip` 4.5, `runner.replay-mix` 4.4, `unit.core.state.export` 4.3, `unit.core.save.device` 3.2. Under the sanitizer `unit.core.state.import` takes 1,149.9 s, 97 percent of the 1,184.8 s wall. The earlier native rehearsal on `16849e7` measured 1,652 s with the same limits.

## Hosted rehearsal (non-qualifying; the pushed commits were `16849e7` and `8dd6af1` only)

Run 44790405712, attempt 1, source `8dd6af1c` (public run `https://github.com/szTheory/tendies64/actions/runs/44790405712`), both jobs success, Binutils cache hit on both. `--collect-hosted --rehearsal` (10 s) and `--verify-evidence --rehearsal` (5 s) pass and print the rehearsal statement. Artifact ids 14657170346 (Linux) and 14657440522 (macOS); public manifest SHA-256 `baabf803...f78c` and `33094c7e...ae97`; the upload stayed inside the frozen contract (one public manifest per job).

| | gate-linux | gate-macos |
| --- | --- | --- |
| queue / run (warm) | 2 s / 242 s | 2 s / 433 s |
| Gate step | 194 s (ctest 107) | 361 s (ctest 218) |
| audio qualification records | 33 s | 37 s |
| Phase 9 case run, emit, PII scan | 2, 1, 0 s | 6, 0, 1 s |
| upload | 1 s | 2 s |
| increment over F-100 (129 s, 275 s) | +113 s | +158 s |
| cold projection (warm + 84 s or 186 s, projected, not spent) | 326 s | **619 s** |
| gate (warm at most 540 s, cold at most 600 s) | pass | warm pass, **cold 19 s over: accepted under DEV-17** |

Earlier runs of this task: run 44776046324 (`49069a7`) failed both jobs at the Phase 9 emit (FX-38); run 44784312630 (`16849e7`) gate-linux green (Gate 147 s, run 189 s) and gate-macos failed at Gate on the 60 s TIMEOUT (FX-39; Gate 379 s). macOS spread on near identical code: 349 s at the plan base `d4ccad5`, 420 s at `7d8f817`, 433 s here (Gate 304 to 379 s). macOS ctest at the rehearsal: 224 names, summed 810 s over four jobs, Phase 9 names 449 s (55 percent), `unit.core.state.import` 190 s alone. The plan's timing awk exits 1 on the macOS line by design (DEV-17). Findings F-107 to F-110 and the Runs rows are in `docs/ci-reviews/LOG.md` (a `[skip ci]` commit that touches only that file).

## Lesson: a hosted-only regression of a fix (FX-12)

FX-12 removed litter from five runner tests, one of which retains evidence for the hosted collector. No gate test reads a retained directory after its process exits, so the local gate, every focused batch, the targeted re-check, the security re-audit and the re-verification could not see it; only the first hosted run of the fix-cycle head did. The cheap check that finds it is a 20 s local replay of the CI steps after Gate (case run, emits, scans) against the built tree. F-107 and F-108 record both lessons (replay the hosted tail before a push; audit registered TIMEOUTs against hosted seconds, which run 2 to 22 times the native figure).

## Verify blocks over 30 seconds (measured)

- Full normal gate: 140 s (on the FX-38 commit; 136.1 s inside the native stage).
- Focused CTest batch over the changed areas (130 names): 74 s.
- Linux compile pre-flight in the act image (219 units, platform not forced): 42 s and 50 s.
- Pinned gcc:13 closure over the 27 units this plan changed: 30.6 s.
- Native rehearsals: 1,652 s and 1,486 s; hosted jobs 242 s and 433 s.

## Verification state at closeout

- `09-VERIFICATION.md`: status passed, 13 of 13, digest `955f5003...` verified at `944443f`. **The digest is stale for the covered files edited since** (the K20 amendment of `09-CONTEXT.md`, the Task 6 edits of `09-REVIEW-FIX.md`, FX-38 in `runner/tests/t64_test_tree.h`, `runner/tests/test_pad_poll.c` and `scripts/tests/test_phase09_qualification.py`, FX-39 in `runner/CMakeLists.txt`) and was not recomputed here because the verifier agent authors it. 09-11 performs the confirmatory re-verification after F2.
- `09-SECURITY.md`: SECURED at ASVS level 1, `threats_open` 0; the 09-11 register T-09-83 to T-09-86 is pending and not counted.
- `09-VALIDATION.md` stays draft with pending rows until the final verification.
- Plan-level exit checks: `git diff --quiet phase09-fr HEAD -- core runner tools scripts .github fixtures` exits 0; the files that follow the tag are `docs/ci-reviews/LOG.md` and files under `.planning/` only; `git ls-remote --tags origin phase09-fr` prints nothing; no `__pycache__` directory remains; the PII scan, rights scan, claims check (skeleton, zero real blocks), test inventory (224 gate, 226 ASan), the no-HLE guard and `git ls-files` over the save, state and replay extensions all pass or print nothing.

## Handoff for 09-11

1. **The tag.** `phase09-fr` is on `8dd6af1cda44fe021fcc4b42c4103ffd5148c986` and is local; 09-11 pushes it with its first push.
2. **Data-only edit list** (everything else is a declared qualification reset): the literal pin constants and `HOSTED_ROWS` data of `scripts/qualify-phase09.py`; the PHASE09 rows of `scripts/check-test-inventory.sh` (none may be added); `fixtures/manifest.json` pins (a moved row means the orchestrator rebinds the oracle request, run `ctest -R '^harness\.rdp-oracle-authority$'` at once); `docs/spec/phase09-cases.json`; `docs/claims/phase09.md`; `docs/evidence/phase09-*`; `docs/ci-reviews/LOG.md` through `scripts/ci-review.sh --append`; `.planning/`; plus the one sanctioned file `scripts/tests/test_phase09_qualification.py` (the F2 pin table and the claims-final case). A hosted overrun found in 09-11 cannot be repaired by a logic edit and is a declared deviation.
3. **Freeze procedure.** `git diff --quiet phase09-fr HEAD -- core runner tools scripts .github fixtures` apart from the data paths; `python3 -B build/gate/09-10-freeze-check.py` (change its `TAG` to `phase09-fr`) for the body comparison; compare the SHA-256 table of the FR record; `sh scripts/check-test-inventory.sh --preset gate` and `ctest --preset gate -N` must still show 45 Phase 9 names.
4. **No CTest name may be added** (45 of 45, gate 224, ASan 226, no ASan-only name added by Phase 9); a hermetic case pair inside an existing test file is the only test addition.
5. **Hosted time (DEV-17) and the cache obligation.** gate-macos warm 433 s, cold projection 619 s (19 s over 600 s); gate-linux 242 s and 326 s. Before 09-11's first push run `gh cache list --repo szTheory/tendies64` and confirm the Binutils entries for both operating systems exist, are fresh and carry the current key (it hashes the frozen `scripts/build-boot-fixtures.sh`, so the key is unchanged); treat a cold macOS run as a known risk of about 619 s against the 600 s job timeout; record 09-11's own measured seconds. Before pushing, replay the CI steps after Gate locally (about 20 s) and run the TIMEOUT audit.
6. **K20 claim-wording limits** (`09-CONTEXT.md`): RDP state carried for the four family ids and authored fixtures only; the continuation matrix has 17 rows B01 to B17, four full (B01, B07, B10, B14), thirteen sampled, the full set only in the native step `normal-state-import-full-matrix` whose step records are unsealed originals, and never "all 16 rows"; a resumed runner run never reports `pass` (claim the mechanism and the digest equality only); a failed publish publishes nothing, but supervisor failures after the runner published can leave the leaf beside a failure record and a caller accepts a leaf only with exit 0 and a `pass` record; the first hosted pair's state digests are valid for source `2db1be4d` only and the 09-11 pair supersedes them; raw check totals are not strength evidence; the qualifier limits are the shared ASan wrapper 3,000 s, the sanitizer TIMEOUT of `unit.core.state.import` 2,400 s, outer 4,800 s, record bound 3,000 s, gcc:13 300 s, full-matrix step 300 s; `scripts/qualify-audio.py` keeps its own 2,400 s limits (F-106); no state, container or digest version marker changed in this plan (provisional until Phase 10); a manifest of a rehearsal SHA is distinguished only by `workflow_sha`.
7. **Open non-blocking findings** with owner and wording impact are tabled in the FR record (R1 RC-06, R1 RC-07, R2 RC-04, RC-06, RC-08, R3 RC-02, RC-04, RC-05 remaining, L4 IN-04, IN-05, security F-1 to F-8): they bound the `not_claimed` lists of the 09-11 claim blocks.
8. **Owner metadata.** A native record binds `AGENTS.md`, `.planning/config.json` and `.planning/AUTONOMY.md`; do not edit them between the native run and its hosted verification.
9. **Margins to watch.** `unit.core.state.import` under the sanitizer 1,150 to 1,257 s of 2,400 s (1.9 times, a decision of FX-22 that is tied to frozen constants); `runner.state-export` under the sanitizer 160 to 182 s of 480 s; hosted macOS `unit.core.state.import` 190 s and `harness.state-field-use-audit` 65 s of 240 s.

## Known Stubs

None. No placeholder text or empty value flows to a consumer in the files this plan created or modified.

## Threat Flags

None new beyond the register already closed by the re-audit (T-09-88 to T-09-90); Task 6 added a test helper function and four CMake deadline numbers, with no new network endpoint, auth path, file access pattern or schema.

## Self-Check: PASSED

Checked after writing: the review, fix, security and verification records, the criteria map, `runner/tests/t64_test_tree.h` and this summary exist; the 18 task, fix and record commits named above are ancestors of HEAD; the tag `phase09-fr` resolves to `8dd6af1cda44fe021fcc4b42c4103ffd5148c986`; `ROADMAP.md` shows 12 of 13 plans for Phase 9 and `STATE.md` points at plan 13; no requirement was marked complete.
