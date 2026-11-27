---
phase: 12-minimal-host-playable-homebrew-and-final-bind
plan: 17
subsystem: qualification
tags: [qualifier, hermetic-test, rehearsal, sdl-lane, bound-record, hosted-pair, gate-growth]

requires:
  - phase: 12-16
    provides: the bound tree at ABI 1.0 with its post-bind proofs (353 names), the BIND ledger stub, the act-linux SKIP record
  - phase: 12-14
    provides: the bound record (ABI 1.0, package 1.0.0, bound_minor 0) and the four post-bind goldens the qualifier binds
provides:
  - scripts/qualify-phase12.py, a thin delta of the unmodified Phase 11 qualifier that binds the post-bind tree
  - harness.phase12-qualification (28 hermetic cases, COST 16, TIMEOUT 150) in the gate, the PHASE12_TESTS block and the preflight row
  - a native rehearsal record (20261127T020322125432Z-9f4e39a3, not final evidence) with the SDL lane status pass and sc2_virtual_joystick closed
  - the bind plus qualifier head 9f4e39a pushed as a fast-forward (no tag) with a green hosted pair, run 45932117914
  - PIN-IMPACT "Qualification steps", the BIND hosted ledger row and CI review rows (the Runs row and finding F-140)
affects: [12-18]

actuals:
  tokens: 52510
  tasks: 3
  commits: 5
plan_head_before: 68d964f3b89a151b99cff29fe2d4e1d7ab615a0d
plan_head_after: c06fc001efd058dbb091a3fd57832f07b719af1d

tech-stack:
  added: []
  patterns:
    - "a qualifier whose label selection is read with -FA . (no fixture closure) and whose JUnit expectation adds the fixture closure as a constant, so a new fixture setup fails closed until the constant is updated"
    - "a three-valued tool verdict (pass, skipped, failed) recorded as two distinct statuses plus a derived clause field, with a hermetic case per value and a tamper case on the retained log"

key-files:
  created:
    - scripts/qualify-phase12.py
    - scripts/tests/test_phase12_qualification.py
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-17-SUMMARY.md
  modified:
    - runner/CMakeLists.txt
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase12-regexes.txt
    - docs/ci-reviews/LOG.md
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/PIN-IMPACT.md

key-decisions:
  - "The label selection of the inventory is read with `ctest -N -L phase12 -FA .` so it equals the 25-name PHASE12_TESTS block exactly as the preflight reads it; the real JUnit run adds the two fixture setups (install.setup, installed.consumer.build), which are the constant FIXTURE_CLOSURE and nothing else is accepted"
  - "Claims are compared by value and type per key against the table (final_abi is the one true claim, meaning only the ABI 1.0 binding); the Phase 11 'every claim is false' rule could not hold the new table"
  - "The closed Phase 11 evidence documents stay bound inputs: EXCLUDED_FILES is LOG.md plus the three Phase 12 documents only (the plan's must-have truth and 12-18's 'closed records stay as history')"
  - "harness.phase12-qualification COST 16 (15.3 to 15.6 s through CTest at host load 6 to 8); hosted macOS 32.29 s, Linux 6.24 s; no limit widened"

patterns-established:
  - "The stage limit of each qualifier stage is at least 2 times its measured seconds with the measurement in the table; the rehearsal ran every stage once with no retry"

requirements-completed: []

coverage:
  - id: Q1
    description: "The Phase 12 qualifier binds the post-bind tree: the four golden hashes of the bound record at ABI 1.0, package 1.0.0, bound_minor 0, the 25-name PHASE12_TESTS inventory, the Wave 0, Host and Bind bases and one build id shared by the core, the installed core, the installed consumer and the host tests"
    requirement: "API-06"
    verification:
      - kind: unit
        ref: "harness.phase12-qualification 28 of 28 cases in the gate (hosted macOS 32.29 s, Linux 6.24 s, passed); the closed Phase 11 qualifier and its test unchanged (git diff empty)"
        status: pass
      - kind: integration
        ref: "python3 -B scripts/qualify-phase12.py --native --rehearsal (exit 0, 15 stages, record 20261127T020322125432Z-9f4e39a3) then --verify RECORD --rehearsal (exit 0, also after the documentation commits)"
        status: pass
    human_judgment: false
  - id: Q2
    description: "The SDL lane stage records pass or skipped as two distinct statuses; a skip never fails the record and never reads as a pass, and the sc2_virtual_joystick field is closed only by a pass"
    requirement: "API-05"
    verification:
      - kind: unit
        ref: "hermetic cases sdl-lane-status, gate-sweep-lane-output-rules (SDL output rules), stage-table-shape: pass, skipped, a failing smoke, neither or both verdict lines, a FAIL line, a relabelled record and a rewritten log"
        status: pass
      - kind: integration
        ref: "rehearsal sdl-lane stage: [host-sdl] PASS, SDL 3.4.10, mapping rows 30/30, 8 frames; record sdl_lane_status pass, sc2_virtual_joystick closed"
        status: pass
    human_judgment: false
  - id: Q3
    description: "A candidate block is refused as not the bound record, and an edit of docs/evidence/phase12-host-manual.md after the record leaves --verify valid while any other bound input refuses"
    requirement: "API-06"
    verification:
      - kind: unit
        ref: "hermetic cases bound-record-binding and host-manual-edit-after-record, evidence-docs-unbound"
        status: pass
    human_judgment: false
  - id: Q4
    description: "The bound tree is rehearsed natively and green on both hosted operating systems before the final qualification; no tag"
    requirement: "API-07"
    verification:
      - kind: integration
        ref: "ci.yml run 45932117914 at the pushed SHA 9f4e39a3caa6d1cd1f3ff2399b3fc3d89251904a: gate-macos 527 s, gate-linux 277 s, 354 of 354 names on both, conclusion success; git tag --points-at the SHA prints nothing"
        status: pass
    human_judgment: false

duration: about 92 min wall (about 39 min of it the host sleeping during a sanitizer lane; the qualifier clock does not count sleep)
completed: 2026-11-27
status: complete
---

# Phase 12 Plan 17: QUALIFY Summary

**The Phase 12 qualifier (`scripts/qualify-phase12.py`, a thin delta of the unmodified Phase 11 one) binds the post-bind tree at ABI 1.0 and passes its 28 hermetic cases in the gate; a native rehearsal of all 15 stages passed once each (record `20261127T020322125432Z-9f4e39a3`, labelled "this result is a rehearsal and not final evidence", SDL lane `pass`, `sc2_virtual_joystick` `closed`), and the rehearsed head `9f4e39a` was pushed as a fast-forward with no tag and a green hosted pair (gate-macos 527 s, gate-linux 277 s, 354 of 354 names, run 45932117914).**

## Performance

- **Duration:** about 92 min wall; about 39 min of it the host slept inside the instrumented sanitizer lane (the rehearsal's wall clock was 1 h 0 min 33 s, the record's monotonic total 1301 s), plus about 10 min waiting for the hosted pair
- **Tasks:** 3 of 3 (Task 1 is the tracer: its verify passed and expansion went ahead)
- **Files:** 5 outside `.planning` (two new), PIN-IMPACT.md and this summary

## Accomplishments

- **Task 1 (tracer).** The Phase 11 qualifier and its hermetic test were copied with the Phase 12 constants: private root `build/gate/12-qualification-private`, the Phase 12 PIN-IMPACT path, schema `t64.phase12-qualification`, bases Wave 0 / Host / Bind / Qualification, the three Phase 12 evidence documents as the only exclusions besides `.planning/` and `LOG.md`, the Phase 12 tool list (title builder and oracle, the host check scripts, `check-host-title.py`, `check-api07-evaluation.sh`, `p12-mutants.py`, the Phase 12 preflight and the qualifier with its test) and the claims table (`hardware_accuracy`, `oracle_comparison`, `release`, `real_time`, `libfuturo_adapter` false; `final_abi` true only as the ABI 1.0 binding). `harness.phase12-qualification` is registered next to the Phase 11 name, listed in `PHASE12_TESTS` and its preflight row is raised to 1. Its verify passed (both qualification names, inventory 354).
- **Task 2.** The stage table is rewritten for Phase 12: `phase12-suite` (the label suite with JUnit, 27 testcases), `host` (the host build and the nine `host.pump` names, naming the host test binary), `sdl-lane`, the inherited lanes with their measured limits, and `mutants` last. The SDL lane verdict is parsed into two distinct statuses, `pass` (the `[host-sdl] PASS` line with the lane's own smoke lines) and `skipped` (exactly the recorded skip line); neither, both, a FAIL line or a missing smoke line is refused; the record carries `sdl_lane_status` and `sc2_virtual_joystick` (`closed` only for a pass, `not closed (skipped)` otherwise), both re-derived from the retained log at verify and printed by `--native` and `--verify`. `freeze_binding` requires ABI 1.0, package 1.0.0, `bound_minor` 0 and `candidate_amendments` equal to the dated amendment-log rows (all made before the bind); a candidate block is refused as "not the bound record". The host test binary joins the artifact list and the one-build-id rule. Three hermetic cases were added (`bound-record-binding`, `sdl-lane-status`, `host-manual-edit-after-record`) plus the SDL and mutant output rules and the fixture-closure rule (28 cases).
- **Task 3.** At the clean head the full rehearsal ran (15 stages, one attempt each, none retried), `--verify RECORD --rehearsal` passed, the preflight passed every step except `linux-compile` (Docker unresponsive, below), the range was scanned (PII range, tree and rights 0 findings) and the pinned SHA was pushed as a fast-forward from `de21fcf` without a tag. Hosted run 45932117914 was green on both jobs. The Runs row, finding F-140, the PIN-IMPACT "Qualification steps" and the BIND hosted ledger row record the numbers.

## Task Commits

1. **Task 1 (tracer): qualifier frame, hermetic test, registration** - `830860661ff1c4c576b1767d4cf1ab1ac5195e7a` (feat)
2. **Task 2: stage table, bound-record binding, host build id, hermetic cases** - `b282294629118ce9d1eac0701e032a9b46dedd59` (feat)
3. **Task 3: measured COST of the qualification test (pushed head)** - `9f4e39a3caa6d1cd1f3ff2399b3fc3d89251904a` (chore)
4. **Task 3: CI review log rows (run 45932117914, F-140)** - `336b73d1325782beefefbeae607c42057598cf77` (docs, `[skip ci]`)
5. **Task 3: PIN-IMPACT Qualification steps and the BIND hosted row** - `c06fc001efd058dbb091a3fd57832f07b719af1d` (docs, `[skip ci]`)

Plan metadata: the SUMMARY, STATE and ROADMAP commit follows this file.

## Rehearsal and hosted pair

| Item | Value |
|------|-------|
| Rehearsal record id | `20261127T020322125432Z-9f4e39a3` (`rehearsal: true`, commit `9f4e39a`, private, never committed) |
| Stages (seconds of limit, one attempt each) | ci-scanner 3.09/90, inventory 0.94/60, oracle 0.23/60, abi-cheap 12.90/720, phase12-suite 22.12/180, host 6.78/180, sdl-lane 2.19/180, gate 295.29/600, sanitize-shipped 116.27/300, sanitize-instrumented 696.03/1200, tsan 12.72/120, fresh-archive 8.01/120, sweep 63.33/300, freeze 0.24/60, mutants 60.19/240 |
| sdl-lane status | `pass` (SDL 3.4.10, dummy drivers, mapping rows 30/30, 8 frames); `sc2_virtual_joystick` `closed` |
| Pushed SHA | `9f4e39a3caa6d1cd1f3ff2399b3fc3d89251904a` (fast-forward from `de21fcf7d5c30d8de59bde3d49143ab5dbb75faf`, no tag) |
| Hosted run | 45932117914, success, no rerun, Binutils cache hit (warm sample) |
| gate-macos | job 527 s, ctest 303.02 s, 354 names (HOST pair F-138: 572 s, 312 s, 351 names) |
| gate-linux | job 277 s, ctest 156.87 s, 354 names (HOST pair: 306 s, 175 s) |
| Phase 12 names | 25 carry the label `phase12` (equal to `PHASE12_TESTS`); 3 new since the HOST pair |
| New names, hosted macOS then Linux | `harness.phase12-qualification` 32.29 and 6.24 s, `unit.core.api.abi-major-rule` 0.01 and 0.00 s, `installed.controls.version-mismatch-major` 1.00 and 0.15 s |
| Cold projection against 900 s | P = 717 s (warm 527 s plus the F-136 cold cost of 190 s; margin 183 s); 580 s from the F-139 cold measurement of the HOST head plus the three names (margin 320 s); reversal rule not triggered |

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] The label selection includes the fixture closure**
- **Found during:** Task 1 (a probe of `ctest -N -L phase12`)
- **Issue:** `ctest -N -L phase12` lists 26 names (the 24 then-registered names plus `install.setup` and `installed.consumer.build`, which carry the label `phase11` and are added by CTest's fixture rule), so the copied inventory check (`names == label selection`) would refuse every real run; the same two names appear in the JUnit of a real run.
- **Fix:** `label_names()` reads `-L phase12 -FA .` (the form the preflight uses); `FIXTURE_CLOSURE = (install.setup, installed.consumer.build)` is added to the JUnit expectation and nothing else is accepted (hermetic cases for a missing and an unexpected name).
- **Files modified:** `scripts/qualify-phase12.py`, `scripts/tests/test_phase12_qualification.py`
- **Commit:** 830860661ff1, b282294629

**2. [Rule 2 - Missing critical functionality] The delta is larger than "constants, stage table, artifact list and binding functions"**
- **Found during:** Tasks 1 and 2
- **Issue:** the plan's acceptance wording says the two qualifiers differ only in the docstring, constants, stage table, artifact list and binding functions. Three further changes were necessary for the plan's own requirements: (a) the Phase 11 claims rule ("every value is false") cannot hold a table whose `final_abi` is true, so the compare is per key by value and type; (b) the SDL lane verdict, the mutant table and the fixture closure need their own detail checks (`_sdl_details`, `_mutants_details`, `sdl_summary`, the `junit` expectation) and the record needs the two new fields; (c) `--native` and `--verify` print the SDL status and the sc2 field so the two values are distinct in the output. All of it is copied code plus additions, and every helper (bounded runner, snapshot, bases, private root) is unmodified.
- **Diff stat against the Phase 11 files** (`git diff --no-index --stat`): the qualifier 324 insertions and 113 deletions (1402 lines), the hermetic test 426 insertions and 95 deletions (1535 lines).
- **Commit:** 830860661ff1, b282294629

**3. [Plan wording] EXCLUDED_FILES**
- The action says "EXCLUDED_PREFIXES and EXCLUDED_FILES as Phase 11 plus those"; the must-have truth lists exactly `.planning/`, `LOG.md` and the three Phase 12 documents, and plan 12-18 says the closed Phase 11 records stay as history. The two Phase 11 evidence documents are therefore NOT excluded (a hermetic case proves an edit of either refuses a final record).

### Environmental findings (not plan defects)

- **The preflight `linux-compile` step failed**: `docker is unavailable and a C change exists`. The Docker engine did not answer in plan 12-16 and was not started or restarted here; the preflight was run with a stand-in `docker` that exits 1 at once (the script's own `docker info` hangs). Every other preflight step is ok (ctest-counts 23 rows 0 mismatches; 25 names carry `phase12`; abi-cheap 16; host-cheap 7; the four guards; head-ci success; timeout-audit 207 TIMEOUTs 0 too low; pii). The first Linux compile and run of the 12-16 and 12-17 changes is the hosted gate-linux job, which passed (354 of 354).
- **The host slept for about 39 minutes** during `sanitize-instrumented`. The qualifier's clock is monotonic and does not count sleep, so no stage limit was hit; the stage seconds are awake seconds. The gate (295 s against 148 s in plan 12-16), the shipped lane (116 s against 54 to 59 s in Phase 11) and the instrumented lane (696 s against 288 to 299 s in Phase 11) ran at host load 8 to 19 from unrelated processes: the final run of 12-18 should start at a quiet host (the limits 600, 300 and 1200 s are 2.0, 2.6 and 1.7 times these figures).
- **A broken-windows ledger entry was tried and removed.** `gsd_run windows append` created a new `.planning/WINDOWS.md` (none existed); the unrun Linux compile is environmental, already recorded here, in PIN-IMPACT and resolved by the hosted run, so the new untracked file was deleted rather than leaving an open entry that would block a later ship step.

### Other notes

- `harness.phase12-qualification` costs 15.3 to 15.6 s native (28 cases), 32.29 s on hosted macOS (about 2.1 times) and 6.24 s on Linux: the largest hosted addition since F-138 (the Phase 11 analogue costs 25.13 and 4.56 s in the same run). The hermetic cases are dominated by scratch git repositories and `verify` calls; trimming them would remove coverage inherited from the Phase 11 set, so the figure is recorded in the ledger instead (cold projection 717 s, margin 183 s).
- Commits are on `main`; only `9f4e39a` and its ancestors were pushed (the three docs commits after it are local), no tag was made, `project(VERSION)` is untouched, no bound file, golden, header or version number was edited, and the closed Phase 11 qualifier and its test are unchanged.
- Every file touched is in the plan's frontmatter list (the qualifier and its test, `runner/CMakeLists.txt`, `scripts/check-test-inventory.sh`, `scripts/preflight-phase12-regexes.txt`, PIN-IMPACT.md and `docs/ci-reviews/LOG.md`); nothing else was edited.

## Authentication Gates

None.

## Issues Encountered

- Host load was 6 to 27 from unrelated processes (Docker Desktop, another project's builds, `fseventsd`); the rehearsal waited for the 1-minute load to fall under 9 before it started and no limit was widened.
- A first run of the new SDL hermetic case used an SDL stub that expected the failing exit code; fixed in the test (the stub expects exit 0 and the real exit code fails the stage).

## Verification Results

| Check | Result |
|-------|--------|
| Task 1 verify (`cmake --build --preset gate`, `ctest -R '^harness\.phase1[12]-qualification$'`, inventory) | 2 of 2 passed; `test inventory PASS preset=gate count=354` |
| Closed tools unchanged (`git diff --quiet HEAD -- scripts/qualify-phase11.py scripts/tests/test_phase11_qualification.py`) | exit 0 |
| Task 2 verify (`ctest -R '^harness\.phase12-qualification$'`, `test_phase12_qualification.py --selftest`) | passed; 28 tests OK |
| Task 2 acceptance (`grep -c -E 'sdl-lane\|p12-mutants\|bound_minor\|phase12-host-manual\|sc2_virtual_joystick'`) | 31 (at least 5) |
| `python3 -B scripts/qualify-phase12.py --native --rehearsal` | exit 0, 15 stages ok, record `20261127T020322125432Z-9f4e39a3` |
| `--verify RECORD --rehearsal` (at the rehearsed head and again after the documentation commits) | exit 0; prints the rehearsal statement and `sdl-lane status: pass; sc2_virtual_joystick: closed` |
| Preflight `--no-replay` (Docker stand-in) | all steps ok except `linux-compile` (environmental, see above) |
| `pii-scan.sh --range origin/main..HEAD`, `--tree`, `rights-scan.sh` before the push and `--tree` after the documentation edits | 0 findings each |
| Documentation readers after the edits (22 names: `installed.consumer.literals`, `repo.api07-evaluation`, `repo.installed-case-map`, `repo.stale-freeze*`, `repo.abi-freeze*`, `repo.host*`, `repo.docs*`, `gov.pii*`, `gov.rights*`, `repo.ci*`) | 22 of 22 |
| Task 3 verify 1 (pushed SHA has a completed successful ci.yml run, no tag) | printed `VERIFY1 ok`; run 45932117914 |
| Task 3 verify 2 (`grep -c rehearsal PIN-IMPACT.md`) | 8 (at least 1) |
| Post-commit deletion check | no file deleted by any task commit |

## Handoff (12-18 and the orchestrator)

- **Nothing is waiting on the orchestrator for this plan.** The pushed head is `9f4e39a3caa6d1cd1f3ff2399b3fc3d89251904a` (origin/main) with a green hosted pair (run 45932117914). Three documentation commits (`336b73d`, `c06fc00` and the SUMMARY/STATE/ROADMAP commit) are local and unpushed; plan 12-18 pushes the final head.
- **12-18** runs the final qualification: append the "Qualification base" line to PIN-IMPACT first, then `python3 -B scripts/qualify-phase12.py --native` (no `--rehearsal`) at a quiet host; the three Phase 12 evidence documents are the only new files it may add (they are excluded from the bound inputs). Any other change after the base makes `--verify` refuse. The rehearsal record is not final evidence and `--verify` refuses it as final.
- **Host load matters for the final run:** at load 8 to 19 the gate took 295 s of its 600 s limit, the shipped lane 116 s of 300 and the instrumented lane 696 s of 1200 (Phase 11 measured 59 s and 299 s at lower load). A load above about 9 risks a limit failure; never widen a limit.
- **The sdl-lane stage depends on the local SDL3 install** (3.4.10 here). A host without SDL3 records `skipped` and `not closed (skipped)`; the record still passes, and 12-18's evidence text must carry the state exactly as recorded.
- **Docker** is still unresponsive; the preflight `linux-compile` step fails for that reason until it answers. The hosted gate-linux job is the Linux compile evidence for this head.
- **G4:** cold projection 717 s (margin 183 s) after this push; the final cold measurement after the 12-18 push is orchestrator-only and replaces every projection. `harness.phase12-qualification` (hosted macOS 32.29 s) is the largest recent addition and a candidate of the reversal rule's selftest family if the cold run ever approaches 900 s.

## Known Stubs

None.

## Threat Flags

None. T-12-46 holds: the private root is mode 0700 under `build/gate` and only sanitized summaries are committed (PIN-IMPACT carries no log and no local path). T-12-47 holds: the rehearsal is labelled and `--verify` refuses it as final; pass and skipped are distinct values with hermetic cases. T-12-48 holds: bound inputs are hashed and only planning, `LOG.md` and the three Phase 12 documents are excluded (hermetic cases for each).

## Self-Check: PASSED

The created files (`scripts/qualify-phase12.py`, `scripts/tests/test_phase12_qualification.py`) exist; commits `830860661ff1`, `b282294629`, `9f4e39a3caa6`, `336b73d13257` and `c06fc001efd0` are ancestors of HEAD (`git merge-base --is-ancestor`), and `9f4e39a` is `origin/main`.
