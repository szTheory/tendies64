---
phase: 09-controller-save-media-and-replay
plan: "02"
subsystem: harness-mechanism
tags: [fixture-table, per-row-cap, transcript-pin, supervisor-argv, artifact-publisher, fsize-rule, admit-roms, rights-scanner, gate-arm, qualifier-stub]

requires:
  - phase: 09-controller-save-media-and-replay
    provides: "09-01 composite family ids (F0), ISViewer and AI gate under all four ids, PHASE09_TESTS, membership generator and list"
provides:
  - per-row guest tick cap (default 600,000, long 4,000,000, ceiling checked at parse in both binaries) and per-row transcript pin shared by the runner and the result checker
  - bounded supervisor child argv (64 slots, checked push, compile-time slot-sum assertion)
  - six authored admit ROMs from one source (four ids, wrong transcript, long spin) with rows, manifest rows and a profile hash print helper
  - single-file private no-clobber artifact publisher and the --artifact-dir/--artifact-max-bytes pair with the exact child write limit proven at its boundary byte in both configurations
  - isolated gate.sh arm for a Phase 9 private JUnit directory; isolated rights scanner rows for .sta, .rpl and T64STATE
  - Phase 9 case inventory checker, inventory data (identity, mechanism), claim skeleton (example fences only)
  - fail-closed qualifier stub with an exact upload allowlist and a bound source tuple
affects: [09-03, 09-04, 09-05, 09-06, 09-08, 09-09, 09-11]

actuals:
  tokens: 53000
  tasks: 7
  commits: 12
plan_head_before: b71b06a255f21f34256a46159ca6cdd90d01c277
plan_head_after: bf75429300bfba00fb2713114504a3404f0f00ff

tech-stack:
  added: []
  patterns:
    - "copy-with-provenance for guard-adjacent scripts (checker, qualifier, builder, publisher) instead of editing bound Phase 8 files"
    - "every refusal control runs its accepted baseline in the same setup and asserts the specific error name"
    - "out-of-tree mutant copies that must compile under -Werror and fail only their named case"

key-files:
  created:
    - runner/t64_artifact_publisher.h
    - runner/t64_artifact_publisher.c
    - runner/tests/test_artifact_publisher.c
    - runner/tests/test_phase09_runner.c
    - fixtures/phase09/admit.S
    - fixtures/phase09/admit.ld
    - scripts/build-phase09-admit-fixture.sh
    - scripts/qualify-phase09.py
    - scripts/tests/test_phase09_qualification.py
    - scripts/check-phase09-cases.py
    - docs/spec/phase09-cases.json
    - docs/claims/phase09.md
  modified:
    - runner/t64_real_fixture.h
    - runner/t64_real_fixture.c
    - runner/t64_real.c
    - runner/t64_supervisor.c
    - runner/t64_result_check.c
    - runner/CMakeLists.txt
    - core/CMakeLists.txt
    - fixtures/manifest.json
    - scripts/gate.sh
    - tools/rights/t64_blobscan.c
    - scripts/rights-scan.sh
    - scripts/check-test-inventory.sh
    - docs/spec/raw-capture.md

key-decisions:
  - "The supervised result of a runner prelaunch refusal (artifact preflight) is the supervisor's post-launch classification report-error/capture-invalid with guest tick 0; the runner binary itself reports detail refusal"
  - "The qualifier stub binds the membership generator and its list file; later plans refresh the list in the commit that adds a site"
  - "Replay artifacts are canonical JSON text with no fixed binary magic, so the rpl extension is the replay control and no replay magic row is added"
  - "requirements SAVE-01..04, CORE-04 and CORE-05 are NOT marked complete: this plan is mechanism only and makes no behavior claim"

requirements-completed: []

coverage:
  - id: M1
    description: "Per-row tick cap (K3) and transcript pin (K4): legacy rows unchanged, long row accepts up to 4,000,000, a bound above the cap is refused after identification, above the ceiling at parse in both binaries"
    requirement: "CORE-04"
    verification:
      - kind: integration
        ref: "runner.guest-tick-cap, runner.fixture-transcript-pin, the 25-name legacy runner group"
        status: pass
    human_judgment: false
  - id: M2
    description: "Bounded child argv (K6) and the exact write limit (K9): maximum option set under ASan, capacity-exhausted fault with no guest run, boundary byte in both configurations"
    requirement: "SAVE-03"
    verification:
      - kind: integration
        ref: "runner.supervisor-argv-bound, harness.fsize-artifact-real-fault, harness.fsize-report-capture-still-bounded"
        status: pass
    human_judgment: false
  - id: M3
    description: "Admit ROMs on all four ids with per-row pins, wrong-transcript control, N5 table checks"
    requirement: "SAVE-01"
    verification:
      - kind: integration
        ref: "runner.phase09-tracer, runner.phase09-admit, controls.real.phase09-admit-wrong-transcript, runner.profile-table-id-length"
        status: pass
    human_judgment: false
  - id: M4
    description: "Publisher refusals and three substitutions after accepted baselines, with passing mutants refused"
    requirement: "SAVE-03"
    verification:
      - kind: unit
        ref: "runner.artifact-publisher, controls.artifact-substitution"
        status: pass
    human_judgment: false
  - id: M5
    description: "Isolated guard rows with controls: gate.sh arm, rights scanner state and replay rows, fail-closed qualifier stub, exact upload allowlist, case checker, claim skeleton"
    requirement: "CORE-05"
    verification:
      - kind: unit
        ref: "harness.phase09-qualification, harness.phase09-stub-fails-closed, harness.upload-allowlist-artifacts, gov.rights.selftest (95 cases)"
        status: pass
    human_judgment: false
  - id: M6
    description: "Spec wording for the exact write limit and the evidence limit; claim skeleton wording"
    requirement: "SAVE-03"
    verification: []
    human_judgment: true
    rationale: "Wording review against the claim boundary (no hardware or timing claim, no API/ABI freeze) is a judgment; sentence checks and the claims, PII and rights scans are automated"

duration: 32min
completed: 2026-11-19
status: complete
---

# Phase 9 Plan 02: Harness mechanism for admit, publish, cap and qualify Summary

**Per-row tick cap and transcript pin, a bounded supervisor argv, six authored admit ROMs through the real supervised runner on all four new ids, a private no-clobber artifact publisher with the exact RLIMIT_FSIZE rule proven at its boundary byte, isolated gate and rights-scanner rows, a fail-closed qualifier stub with an exact upload allowlist, the case inventory checker and a claim skeleton. No behavior claim is made.**

## OPEN ISSUE FOR THE ORCHESTRATOR (read first)

The full CTest gate preset ends 202 of 203: `harness.rdp-oracle-authority` fails with `oracle-authority: fixture manifest SHA-256 changed`. The cause is this plan's manifest rows (Tasks 1 and 2, as the plan requires): `docs/evidence/phase07-oracle-authority.json` pins the SHA-256 and git blob id of `fixtures/manifest.json`, and `scripts/check-rdp-oracle-authority.py` pins the derived request hash. The plan did not list either file, and the brief for this plan says not to touch Phase 7 VID-04/Plan 13, so nothing was changed there. The same situation arose after Phase 8 and was reconciled by commit `15fd8d1` ("fix(07): rebind the prepared oracle request ..."; owner approved that reconciliation on 2026-11-19). Computed (not applied) values for the same kind of rebind at `bf75429`:

| Field | Old | New |
| --- | --- | --- |
| manifest `sha256` (two places in the request) | `f7c1953d651c77b5121ce8e454f6c0953ed3495a911bcbb883dfd8ff09917931` | `8a5a4819b1e263caf57f6b92abaf841bd045549002f9959a0a765d97dc03334d` |
| manifest `git_blob_id_sha1` (two places) | `63097d1657d90315a635aa83a8d7cc7f30ffe12d` | `101f8bbfc36a3ac0aec2c64879ba0d71c2ee3b9f` |
| `request_sha256` and `PINNED_REQUEST_SHA256` | `64b6f2608e820742d15df6e8baf40feecf2513a65eea1fef927c45ce653a9008` | `47906938f4f7404bba0520e7ba02d59e4ab2da21aa129c59830b5f57b0b4ca9f` |

The new request hash is `_canonical_sha256` of the request with the two manifest identities replaced (the old request reproduced its own pinned hash first, `True`). Every later Phase 9 plan that adds a manifest row will move the manifest hash again, so the decision (rebind per plan, or once at the end of Phase 9) is the orchestrator's or owner's. Until then the hosted gate and `scripts/gate.sh` would fail on this one test.

## Performance

- **Duration:** 32 min (2026-11-19T20:26:29Z to 20:58:16Z, before this closeout)
- **Tasks:** 7 of 7 (Task 1 is the tracer; Task 1 steps 2 to 6 are separate commits)
- **Commits:** 12 production and test commits measured from the ledger base (`b71b06a`)
- **Files:** 12 created, 13 modified (25 changed, 4962 insertions, 62 deletions)

## Accomplishments

- Per-row cap (`T64_REAL_GUEST_TICKS_DEFAULT_CAP` 600,000, long cap and ceiling 4,000,000): the ceiling is checked at parse in the supervisor and the runner, the row cap after fixture identification; only `phase09-admit-long-si-v1` carries the long cap. Per-row transcript pin read by the runner pass rule and by the result checker through one accessor; the legacy 30-byte pin is the default.
- Supervisor child argv: 64 slots, one checked push helper (refuses when the push plus the terminating NULL would not fit, child `_exit(126)` before exec), per-option slot sum with a `_Static_assert`, supervisor-only fault `argv-capacity-exhausted`.
- Six admit ROMs from `fixtures/phase09/admit.S` (digit, wrong, spin defsyms), built by `scripts/build-phase09-admit-fixture.sh` (copy of the audio builder with the same pins), target `t64-phase09-admit-fixture`, rows and manifest rows, `--print-profile-hashes` (lengths 38, 42, 43, 43).
- `runner/t64_artifact_publisher.{h,c}` (copy with provenance of the raw-capture transaction, single file, leaf grammar, hard cap, test hooks), the artifact pair in both binaries, preflight before guest start, child limit exactly maximum plus 65,536 only with the pair, test faults `artifact-publish-probe`, `artifact-write-limit-exact/-over`, `capture-write-limit-exact/-over`.
- Isolated guard commits: `scripts/gate.sh` (one arm, numstat 2/1) and `tools/rights/t64_blobscan.c` (two extension strings and one magic row, 3/1) each with controls.
- `scripts/check-phase09-cases.py`, `docs/spec/phase09-cases.json` (identity and mechanism suites), `docs/claims/phase09.md` (six example fences), `scripts/qualify-phase09.py` (every evidence mode refuses; exact allowlist; bound source tuple).

## Task Commits

1. **Task 1 (tracer):** `fba0755` K3 cap, `5712ddd` K4 pin, `45eee61` K6 argv, `c1a0232` admit sources, build script, target and hash helper, `4ebb917` stem row, manifest row and tracer case (manifest `source.revision` is `c1a0232b95f31c8679ec3ccd5178764eb7b0d640`), `166ad73` publisher, artifact pair and the exact limit
2. **Task 2:** `d2b6428` five rows, manifest rows, cases and the table accessor
3. **Task 3:** `d9c405d` publisher and boundary controls, spec paragraph
4. **Task 4:** `1c5e778` gate.sh arm (isolated)
5. **Task 5:** `a8dfc36` rights scanner rows (isolated)
6. **Task 6:** `aa2fe78` checker, inventory, claim skeleton
7. **Task 7:** `bf75429` qualifier stub, allowlist, source tuple

## Tracer

Task 1 proved one id end to end before expansion: the stem admit ROM on the stem id is admitted, ticked and passed through the real supervised runner under a 20-byte per-row pin, and the test twin published one 0600 `probe.sta` under the exact limit. The full Task 1 verify block (29 of 29 names, guard, inventory, PII and rights scans, region gates) and its ASan block were re-run on the committed content: `Tracer verified end-to-end - expanding`.

## RED records

**Task 1 step 2 (K3), observed before the supervisor and runner edits.** Bound 600,001 on `hello-openipl3` through the supervisor: `"detail":"usage","outcome":"harness-error","guest_stop":{"bound_tick":"530500","kind":"not-started"},"guest_tick":"0"` where `refusal`/`refused` was required; the runner binary behaved the same (usage). Runner record for bound 4,000,001: `"detail":"usage","guest_stop":{"bound_tick":"4000001","kind":"not-started"}` (that one is the required behavior). Failing assertions: `detail == "refusal"` and `stop_kind == "refused"`, twice.

**Step 3 (K4): started green by design.** No pinned row existed until step 5; the characterization was the unchanged 25-name legacy group plus `runner.guest-tick-cap` (26 of 26) and the new `runner.fixture-transcript-pin`. The pinned RED is step 5.

**Step 4 (K6).** Against the HEAD supervisor the accepted baseline (advance step, video fd, audio fd) passed under the 24-slot array; the fault run was refused as an unknown test fault: `exit=2 outcome=harness-error detail=usage stop=not-started guest_tick=0` (setup-class). After the change: `exit=1 outcome=harness-error detail=runner-exit-nonzero stop=host-error guest_tick=0`. Discriminating control: an out-of-tree mutant with the capacity check removed compiled under `-Werror` and ran the fault to `exit=0 outcome=pass detail=positive-match stop=authored-completion guest_tick=551001`, failing four assertions.

**Step 5 (K12), stem ROM built, row absent:** `{"detail":"refusal","fixture_id":null,"guest_stop":{"bound_tick":"600000","kind":"refused"},"guest_tick":"0","outcome":"harness-error",...}`; GREEN with the row (`pass`, `positive-match`, 20 bytes, SHA-256 `5fc4d0d3...fbc9`, completion `ffffffff80000470`).

**Task 2 (rows absent).** The stem run in the same setup passed first. Then every other row run reported `"detail":"refusal","guest_stop":{"kind":"refused"},"guest_tick":"0","fixture_id":null` at bounds 600,000, 1,200,000 and 4,000,000; `admit-ids` failed on the e4k, e16k and s32k iterations (fixture id, profile id, 20 bytes, pin, milestones), `profile-table` failed `pinned >= 6` and the four family-id presence checks.

**Task 3.** The publisher controls started green: the module of Task 1 step 6 was written complete (identity re-check, cap check, hooks), so no original RED exists for it. Evidence is current, not history: out-of-tree mutants, each compiled under `-Werror` and failing only its named case: (a) staged-identity check removed fails `substitution` (the staged symlink was renamed into the leaf: four assertions), `publish-and-refusals` stayed green; (b) cap allowing one extra byte fails `publish-and-refusals` (five assertions), `substitution` stayed green. For the faults the RED was the unknown fault name (`detail=usage`, setup-class). Extra mutants of the supervisor (compiled under `-Werror`): artifact limit loosened by one byte fails `fsize-artifact` (the over write is no longer signalled), default limit loosened by one byte fails `fsize-report-capture`, and the pair ignored fails `fsize-artifact`.

**Task 4 (before the arm).** Exit codes of the real case block: `.../09-11-phase09-private/run/junit.xml` exit 2 (wanted 0); the other six values (Phase 8 path 0; `..` in the middle and at the end, outside, near-miss prefix, the bare directory: 2) already as wanted.

**Task 5 (before the scanner edit).** `selftest: cases=95 failures=6`: FAIL `ext-state-sta`, `ext-replay-rpl`, `ext-state-replay-trailing-mixed-case`, `magic-t64state`, `staging-state-ext`, `staging-t64state-magic`, each `exit 0, wanted 1`; the two near-miss controls print `ok` before and after (they must stay clean). After: 95 cases, 0 failures (baseline was 87, plus 8).

**Task 6.** `check-phase09-cases.py` absent: Python exit 2 (`can't open file`); claim file absent: `FileNotFoundError`. The mismatch refusal bites: an out-of-tree checker copy with the case-list equality removed fails the case (`0 != 1` on the first mismatch refusal).

**Task 7 (permissive skeleton qualifier).** `--native` exit 0 where 1 was required (empty stdout and stderr); `validate_case_inventory` returned 0 suites where 2; the allowlist scan returned an empty object (`KeyError: 'schema'`); `source-paths-exist-and-cover-guards` failed on an empty tuple. `no-undefined-global-names` started green (a static check, no failure to manufacture).

## Task 4 pin record (isolated commit `1c5e778`)

Pin: the `T64_GATE_JUNIT_OUT` case block in `scripts/gate.sh`. Old: one arm `"$ROOT"/build/gate/08-05-audio-private/*)` and the refusal text `... must be a private Phase08 evidence path`. New: one added arm `"$ROOT"/build/gate/09-11-phase09-private/*)` and the text `... a private Phase08 or Phase09 evidence path` (`git show --numstat`: `2 1`). Cause: a Phase 9 native record needs its own private directory. Control: `gate-junit-hook-private-phase09` runs the real extracted block with seven values (accepted 0, 0; refused 2 for `..` in the middle, `..` at the end, a foreign path, a near-miss prefix and the bare directory); `harness.audio-qualification` still passes unchanged.

## Task 5 pin record (isolated commit `a8dfc36`)

Pins: the denied-extension table and the container magic list of `tools/rights/t64_blobscan.c`. Old: `z64 n64 v64 sav eep sra fla mpk ndd d64`, no state magic. New: the same plus `sta` and `rpl`, and one prefix row `T64STATE` (8 bytes at offset 0); 3 insertions, 1 deletion, no logic change. Cause: state and replay containers are private (D-33). **Deviation from the outline wording:** no replay magic row, because replay artifacts are canonical JSON text with no fixed binary magic and a prefix match on quotable text would flag documentation; the `rpl` extension row is the replay control. Controls (inside `gov.rights.selftest`, 87 to 95 cases): `ext-state-sta`, `ext-replay-rpl`, `ext-state-replay-near-miss`, `ext-state-replay-trailing-mixed-case`, `magic-t64state`, `magic-t64state-near-miss`, `staging-state-ext`, `staging-t64state-magic`. **Mapping for 09-VALIDATION row 9-V25:** `harness.rights-scan-state-replay` is satisfied by these eight cases inside the existing `gov.rights.selftest` (no new CTest name, K13).

## Measurements

- Long-ROM completion tick (spin count 100,000, three instructions per iteration): **838,791** with bound 1,200,000 and with bound 4,000,000; stem completion tick 538,781. Tests assert a range (greater than 600,000, less than 1,500,000), never the exact tick.
- Inventory selftest: 20 s on this host (203 gate names, 205 asan; 0.1 s per name). **Cumulative Phase 9 CTest names: 24** (10 from 09-01 plus 14 here) of the K13 budget of about 45; gate inventory 203, asan 205.
- Verify blocks (seconds, warm host): Task 1 legacy baseline 3.4; Task 1 block 1 on committed content 8 (incremental build, 29 names); Task 1 ASan block 5 build plus 13 tests; Task 3 block 1 4 (14 names), ASan 20; Task 5 selftest 9, governance batch about 10; Task 6 block 6 (both suites); Task 7 block 1 2; sweep: scans under 1, core lint 9, governance batch 8, ASan selection 3 build plus 32 tests (11 names); full CTest gate preset 45 (203 names, one failure, see the open issue).

## Deviations from Plan

**1. [Plan defect, unresolved] Phase 7 oracle request pins the manifest.** See the open issue at the top. Not fixed here (Phase 7 evidence and its guard are outside this plan and the brief).

**2. [Rule 3 - Blocking] `cmake` omitted from `SOURCE_PATHS`.** The plan lists `cmake`; no such directory exists, and the plan's own control requires every entry to exist in git. Dropped.
- **Commit:** `bf75429`

**3. [Rule 1 - Bug] Evidence directory creation race.** `test_phase09_runner` created its evidence directory with stat-then-mkdir; concurrent CTest jobs under the ASan preset raced and two tests exited 2 in 0.07 s. A racing mkdir that finds the directory is now success; the ASan block was re-run from a deleted directory and passed.
- **Commit:** `166ad73`

**4. [Rule 3 - Blocking] Direct runner controls need unlinked snapshots.** The runner accepts only read-only descriptors on unlinked regular files; plain open files were refused for the wrong reason. The test builds an unlinked copy per descriptor and every direct-runner refusal runs an accepted baseline in the same setup.

**5. [Design note] Preflight refusal classification.** The runner reports the existing prelaunch refusal (detail `refusal`, kind `refused`, tick 0) for a missing or group-writable artifact directory; through the supervisor that record carries no identities, so the supervised result is `harness-error`, stop `report-error`, detail `capture-invalid`, guest tick 0, accepted by the checker. Both are asserted.

**6. [Scope] Requirements not marked complete.** The plan lists SAVE-01..03, CORE-04 and CORE-05 but states mechanism only and no behavior claim, so `requirements mark-complete` was not run; `requirements-completed` is empty.

**7. [Plan arithmetic] None found** in the verify blocks (Task 1 expected at least 29 names, 29 passed; Task 3 14; Task 7 asan 11). The Task 3 text "run before the hooks exist" could not be followed literally for the publisher (see Task 3 RED).

**Total deviations:** 7 (1 unresolved cross-phase pin, 2 blocking, 1 bug, 1 design note, 1 scope, 1 process note). **Impact:** no guard weakened; the only pins touched are the two in the isolated Task 4 and Task 5 commits.

## Authentication gates

None.

## Known Stubs

- `scripts/qualify-phase09.py` refuses every evidence mode on purpose (fail-closed); 09-03 provides the collector and verifier. Not a stub that hides a missing result: the refusal text states it.
- `docs/claims/phase09.md` holds only `t64-claim-example` fences with placeholder values; real blocks arrive in 09-11.

## Threat Flags

| Flag | File | Description |
| --- | --- | --- |
| threat_flag: private-stage-remnant | runner/t64_artifact_publisher.c | A child killed at the write limit leaves a private (0700) stage directory in the artifact directory and publishes nothing; documented in `docs/spec/raw-capture.md`, asserted by `fsize-artifact`. |

## Inputs for 09-03

- Finalize `SOURCE_PATHS` in `scripts/qualify-phase09.py` (and its control) with every file 09-04 to 09-09 will create (for example `scripts/check-state-field-use.py`) before the FH freeze; a later logic edit to a frozen file is an FH deviation.
- Extend `T64_SUP_CHILD_ARGS_SLOT_SUM` in `runner/t64_supervisor.c` in the commit that adds any child flag (current maximum 23 slots plus the NULL, capacity 64).
- The publisher API, the fixture row layout (`guest_tick_cap`, `expected_log_bytes`, `expected_log_sha256`), the option pair and the exact limit rule are frozen after this plan (D-06); a change is a recorded deviation.
- `qualify-phase09.py --selftest` runs `test_phase09_qualification.py --selftest`; no case in that file may call `qualify-phase09.py --selftest` (recursion).
- The oracle-request rebind decision above (the manifest hash moves with every added row).
- To re-pin an admit row: `test_phase09_runner --print-profile-hashes` and the build metadata in `build/fixtures/phase09-admit-*.json`; the spin count lives in `scripts/build-phase09-admit-fixture.sh` and changing it changes the source-content hash and every row's recipe text.

## CI/CD efficiency notes (AUTONOMY section 2)

- The Phase 7 oracle pin was found only by the full CTest run at the end. Seconds-long check that would have caught it right after the first manifest edit: `ctest --preset gate -R '^harness\.rdp-oracle-authority$'` (0.2 s); worth adding to every plan that edits `fixtures/manifest.json`.
- The evidence-directory race appeared only under ASan with four jobs; the same test passed on the gate preset because the directory already existed. A clean-directory run of each new test (delete its evidence directory first) is a seconds-long local check.
- A warm ASan configure and build of the whole tree took 3 to 5 s; the ASan test run (11 names) took 32 s, dominated by the guest runs (about 15 times slower than native), so run it once at the end of a task, not per step.
- The pipe-less direct runner control needs unlinked snapshots; the cheap way to prove a refusal control reaches its branch is the accepted baseline in the same setup, now present in every new refusal case.

## Self-Check: PASSED

- Created files exist and are tracked: the twelve listed under key-files.created (`git ls-files`).
- All twelve commit hashes above are ancestors of HEAD; `git log --oneline b71b06a..HEAD -- scripts/check-audio-cases.py scripts/qualify-audio.py docs/spec/audio-cases.json runner/t64_raw_capture.c scripts/build-boot-fixtures.sh` prints nothing; `git log --name-only --format= b71b06a..HEAD -- core/` lists only `core/CMakeLists.txt`.
- `scripts/gate.sh` changed only in `1c5e778` and `tools/rights/t64_blobscan.c` only in `a8dfc36`.
- Gate-preset checks on the final content: Task 1 verify block 29 of 29; Task 3 block 14 of 14; Task 7 blocks 4 of 4 (gate) and 11 of 11 (ASan, no sanitizer report); guard green on `build/gate` and `build/asan`; inventory passes for gate (203) and asan (205); PII scan, rights scan, claims check and core lint clean; no `__pycache__` remains. The full CTest preset is 202 of 203 with the single failure explained above.
- No ROM, save, PIF/IPL image, 64DD media, hardware measurement or oracle was used; the only external input is the already cleared openIPL3 asset embedded by the authored ROMs. No push was made.
