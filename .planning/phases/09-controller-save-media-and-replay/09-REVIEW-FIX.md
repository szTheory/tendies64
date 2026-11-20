---
phase: 09
plan: 09-10
task: 5
fixed_at: 2026-11-20
review_path: .planning/phases/09-controller-save-media-and-replay/09-REVIEW.md
security_path: .planning/phases/09-controller-save-media-and-replay/09-SECURITY.md
verification_path: .planning/phases/09-controller-save-media-and-replay/09-VERIFICATION.md
iteration: 1
cycle_groups: [G1, G2, G3, G4, T6-FX38, T6-FX39]
status: g1_g2_g3_g4_complete_fx38_fx39_task6_fr_tagged_dev17
recheck_paths:
  - build/gate/09-10-recheck-R1.md
  - build/gate/09-10-recheck-R2.md
  - build/gate/09-10-recheck-R3.md
  - build/gate/09-10-security-recheck.md
---

# Phase 09: Code Review Fix Record (plan 09-10 Task 5, the single budgeted fix cycle)

**Source documents:** `09-REVIEW.md` (52 findings: 5 critical, 15 warning, 32 info, five lanes), `09-SECURITY.md` (verdict OPEN_THREATS: T-09-67, T-09-73, T-09-82 and the new T-09-87 open at high severity), `09-VERIFICATION.md` (gaps_found, 6 of 13, gaps VG-01 to VG-11), and the drift sweep records R-01 to R-06 of Task 2 (`build/gate/09-10-drift-report.txt`).

**Brief:** the orchestrator's fix brief (`build/gate/09-10-fix-brief.md`) fixes the dispositions below. This file is the record of the cycle: the triage table was committed before any fix; every fix appends an FX entry (RED observation, fix, green); the golden record and the deviations are kept current.

## Deviations of the cycle

### DEV-01 [bound raise]: the blocking set exceeds the plan's 10-commit bound, the cycle runs as three executor groups

Plan 09-10 Task 5 step 1 says: if the blocking set exceeds 10 fix commits, stop after the triage commit and return the stop to the orchestrator. The blocking set (below) needs far more than 10 commits: one state-content change with goldens (RDP-STATE), the importer bounds, nine further fixes in the core and in the tests of G1, the runner and publisher fixes of G2 (option pairs, publisher post-commit failure, resumed-run record), and the qualifier time limits, the matrix registration and the guard pin of G3. The orchestrator **raised the bound for this cycle** and split it into three sequential executor groups, because an executor cannot spawn further agents:

- **G1** (this executor): core state, digest and importer (RDP-STATE, IMPORT-BOUNDS), the SI store budget, the argument-refusal aliasing, the tracer tuple, the field-use scan, the campaign reach, tests that can fail for SI and save, and the cheap spec drift of the files G1 touches.
- **G2**: runner, publisher, spec text of the options (L3, VG-07, VG-10, R-NN).
- **G3**: qualifier and scripts and guard and CI time (L4, VG-03, VG-08, the guard pin of L4 WR-02), each frozen-file edit recorded as FD/FR.

The other stop rules of the plan stand: no silent change of a locked CONTEXT decision, no weakened guard, an FD entry for every frozen-file edit, goldens move only with an explained diff, pin refreshes are isolated commits. The one design-level choice of the cycle (the carry of the RDP members into the v25 state) is recorded as the F1 deviation STATE-CONTENT-F1 below and does not reverse a locked decision: K1 (depth inactive on descriptors 11 and 12) stays, and D-05 (strip-and-compare against v24) stays because the new members are streamed in the new-span class.

### DEV-02 [STATE-CONTENT-F1]: the composite family state carries the RDP combiner block and the memory coverage table

Class STATE-CONTENT-F1 (a recorded defect fix of the frozen state content, D-06, decided by the orchestrator in the fix brief). The v25 walker, the container and the importer carry the combiner block (88 bytes) and the memory coverage table (4 + 2,097,152 bytes) as new-class spans at the end of section 8 for the four family ids only. No locked decision changes: K1 (depth inactive on descriptors 11 and 12) stays and the depth members stay out, D-05 (strip-and-compare against v24) holds because the spans are new-class, and no state, digest or golden of the 17 pre-Phase-9 ids moved (510 digests of the 17 ids across three authored ROMs and ten checkpoints are byte identical between cfe4d34 and the fix tree with one build identity; the same comparison with a changed build identity differs in 30 of 792 lines, so it can fail). The importer rules for the new members are in `docs/spec/state-container.md` and the corrected text of `docs/spec/machine-contract.md` states which tuples hash which members. Cost: every family digest moved (golden record below); the container grew by 2,097,244 bytes (idle stem machine 8,448,795 bytes, inside the 11,534,336 ceiling and above the 8,400,908 lower bound); hashing and the sanitizer cost grew by about a third (measured below).

### DEV-03 [FD-02]: one data literal of a frozen file, `scripts/check-rsp-no-hle.py`

The raster function inventory `REVIEWED_RDP_RASTER_FUNCTIONS` gained `t64_rdp_triangle_extent` and `t64_rdp_triangle_opcode_accepted`, and four body digests changed or were added, in the isolated pin refresh commit 45b61d2. The freeze check classifies the literal `REVIEWED_RDP_RASTER_FUNCTIONS` as LOGIC because its data table lists only `CANONICAL_CORE_SOURCES` and `REVIEWED_BODY_DIGESTS`; no check was changed, weakened or removed (the diff is 9 insertions and 4 deletions of names and hex digests). Reason it cannot be made elsewhere: the guard requires the exact set of function names of `core/t64_rdp_raster.c`, and the importer must share the acceptance proof, which needs the new function in that file. Cost: the file is in the bound source set, so the source digest of the qualification changes (a covered edit before F2). `scripts/tests/test_rsp_no_hle.py` gained two data rows in the existing pin-deletion tuple `NEW_SAVE_BODY_PINS` (covered by FD-01, which already records the logic status of that file). The freeze check after the cycle therefore shows two LOGIC verdicts, FD-01 and FD-02, and no other frozen file differs from the baseline.

### DEV-04 [FD-03]: two frozen files, `runner/t64_artifact_publisher.c` and `runner/t64_artifact_publisher.h`

Class FD (a logic change of a frozen file of the Task 1 list), recorded by group G2 for L3 WR-01 (FX-10). Reason it cannot be made elsewhere: the defect is the publisher's own transaction (`committed` was set right after the no-replace rename and `abort` is a no-op once committed, so a failing identity check or directory sync left a published leaf and a stage); the callers (state export, save export, replay record and report) already call `abort` after a failed `finalize` and could not withdraw a publication from outside. The change moves the directory-sync capability proof into the preflight and again before the rename, sets `committed` only after both post-rename checks pass, withdraws the publication (leaf removed only while it is still the held file, stage always removed) on any failure after the rename, and adds test-twin-only hooks (a post-commit hook, forced directory-sync faults, a preflight sync fault); two control-name constants sit outside the testing block. Cost: both files are in the bound source set, so the qualification's source digest changes (a covered edit before F2), and logic changed after the only real hosted pair, so the hosted rehearsal of Task 6 is the hosted evidence (as for FD-01 and FD-02). The freeze check after G2 shows four LOGIC verdicts: `scripts/tests/test_rsp_no_hle.py` (FD-01), `scripts/check-rsp-no-hle.py` (FD-02) and the two publisher files (FD-03). The guard does not pin any runner body (`scripts/check-rsp-no-hle.py` has no `runner/` entry), so no pin refresh was needed.

### DEV-05 [FD-04]: `scripts/check-rsp-no-hle.py`, the function `_check_source_route` (group G3, FX-19)

Class FD (a logic change of a frozen file of the Task 1 list). The guard reads only the sources it names and routes only the bodies it lists in `_check_source_route`; pinning `t64_save_sram_dma_apply` and `t64_si_complete_event` needs the function to read `core/t64_save.c` and `core/t64_si.c`, route the two bodies through the digest table and check two invalidation tokens in each (the pattern of the existing depth-commit check). The digest rows are data (the table is a data name of the freeze check). Reason it cannot be made elsewhere: the route list is inside the function. No check was removed or weakened (30 added lines and one changed line in the commit; the file existence test names two more files). Cost: the file is in the bound source set, so the source digest of the qualification changes (a covered edit before F2), and logic changed after the only real hosted pair, so the hosted rehearsal of Task 6 is the hosted evidence (as for FD-01 to FD-03).

### DEV-06 [FD-01 extended]: `scripts/tests/test_rsp_no_hle.py` (group G3, FX-19)

Four seeds for the call deletions (73 seeds, `MUTATION_SEEDS` extended), two pin-deletion rows (data, `NEW_SAVE_BODY_PINS`), one helper (`_check_rdram_writer_invalidation`), and two repairs found by the new seeds (Rule 1, test defects since plan 07-01): the seeds "RDP fill production body mutation" and "DPC counter bus-window body mutation" now restore the file they mutate, and a clean check with its own control follows the last restoring seed. No existing check was removed or weakened; every existing seed still rejects its own mutation (the measure and the counts are in FX-19). Cost as DEV-05 (a covered edit before F2).

### DEV-07 [FD-05]: `scripts/qualify-phase09.py` (group G3, FX-20 to FX-22)

The outer limit and the record bound of the shared stage are module constants (`NATIVE_SHARED_STAGE_TIMEOUT_SECONDS` 4,800, `NATIVE_SHARED_RECORD_MAX_TIMEOUT_SECONDS` 3,000) and the native command plan gains its last item `normal-state-import-full-matrix` with its marker check (`_state_import_checks`, `_verify_full_matrix_step`, `_verify_native_step`, the creation branch and the verification loop of a native record). Reason it cannot be made elsewhere: the limits and the plan are inside `_native_command_plan` and `_verify_rdp_native_receipt`. No pass rule was loosened: the verification of every existing plan item is the same code (moved into `_verify_native_step`), the record bound was raised by the measured need only and the new step adds a requirement. Cost: a covered edit before F2, and the qualification record now includes one more original step (the matrix run, 74 s natively) in every native run.

### DEV-08 [FD-06]: `scripts/qualify-rdp-vi.py` (group G3, FX-20 and FX-22)

The five stage limits of `_run_native` are named module constants (`NATIVE_GATE_TIMEOUT_SECONDS` 600, `NATIVE_ASAN_CONFIGURE_TIMEOUT_SECONDS` 120, `NATIVE_ASAN_BUILD_TIMEOUT_SECONDS` 600, `NATIVE_ASAN_FULL_CTEST_TIMEOUT_SECONDS` 3,000, `NATIVE_GCC13_TIMEOUT_SECONDS` 300); the ASan and gcc13 values changed. The stage is shared with Phases 7 and 8; their tests and selftests pass (FX-20). Reason it cannot be made elsewhere: the limit is the argument of `_run_recorded` inside `_run_native`. Cost: a covered edit before F2; a new Phase 8 native qualification would need its own outer limit and record bound raised (FX-20).

### DEV-09 [control mechanism]: the TIMEOUT control reads the CMake text, not `ctest --show-only=json-v1` (VG-03 wording)

The verifier asked for a control that reads every registered TIMEOUT from `ctest --show-only=json-v1`. The control reads the `TIMEOUT` literals of every `CMakeLists.txt` instead, because the file's contract is "no builds and no nested CTest" and a discovery run rewrites `LastTest.log` of the build it runs in (the retention rule of `qualify-phase09.py`), which a gate test must not do to its own run; the sanitizer-only TIMEOUT lives in the sanitizer block of `core/CMakeLists.txt`, which the gate tree does not register but the text does. The parse is itself controlled: it fails if a mention of `TIMEOUT` outside a comment is not a literal number, it must find at least 40 values and the known maximum. The registered value was cross-checked once with `ctest --test-dir build/asan --show-only=json-v1` after FX-22 (`unit.core.state.import` TIMEOUT 2400).

### DEV-10 [data edit of a bound area, not in the brief]: `core/CMakeLists.txt`, TIMEOUT of `unit.core.state.import` under the sanitizer preset 1800 to 2400 (FX-22)

Reason: the measured 1,339.5 s left a 1.34 times margin to a deadline that fails the test by itself, whatever the wrapper allows. Cost: none in coverage (a deadline, not a count); the wrapper, the outer limit and the record bound moved with it (DEV-07, DEV-08). The iteration counts of the sanitizer block were evaluated and not lowered (FX-20b).

### DEV-11 [classification table outside the repository]: `build/gate/09-10-freeze-check.py`

Edited as the brief directed (the file is untracked and owned by the orchestrator): names matching `REVIEWED_*_FUNCTIONS` of `scripts/check-rsp-no-hle.py` are data (so the literal of DEV-03/FD-02 is DATA), the FD table names FD-01, FD-04, FD-05, FD-06 by the fingerprint of their changed names and the publisher diffs are hand-classified as FD-03 by diff hash. The final verdicts are in the verification notes of group G3.

### DEV-12 [design choice, FX-29]: the supervisor states and pins what a failure after the commit leaves, it does not withdraw a leaf it does not hold

Re-check R2 RC-01 (the supervisor can replace a runner success by a failure record after the runner already published, and the leaf stays) offered a stronger contract (the supervisor withdraws the leaf through the publisher's descriptor walk) and a stated one. No held-descriptor mechanism exists across the process boundary (the descriptor lives in the child), a withdrawal by name would be a new deletion path in the supervisor (the class the security re-audit just closed as T-09-89), and the brief said to prefer proving the known trigger before the guest runs. FX-29 therefore (1) proves the two supervisor outputs that follow a publication (`--result` and `--capture-out`) writable before the guest runs, (2) states the contract by outcome in `docs/spec/save-media.md` and (3) pins every reachable trigger next to the accepted baseline. Two behavior changes follow and are intended: an unusable capture path is now a usage refusal at guest tick 0 (it was a `host-error` record after the whole run, with the artifact published) and an unusable result path ends with exit status 1 before the guest runs (it ended the same way after the run). No pass rule moved. Not a frozen file (`runner/t64_supervisor.c` is outside the FD list).

### DEV-13 [design choice, FX-24]: the digest walker fails closed instead of losing the guard

R1 RC-01 allowed removing the guard or failing closed. Removing it leaves no control that can fail (the pointer is set at creation); failing closed with `BAD_STORAGE` has one (the member edited directly in a test, four ids, accepted baseline in the same setup). No digest, golden or state changed (792 regression lines identical).

### DEV-14 [registered case renamed, FX-34]: the selftest case of FX-25 is now `doc-count-drift`

FX-25 registered the case `mutant-count-drift` inside `harness.state-field-use-audit`; FX-34 gave the same case a second seed (the matrix size sentence) and renamed it `doc-count-drift` in the script, `docs/spec/phase09-cases.json` and the criteria draft. No CTest name was added or renamed (45 of 45).

### DEV-15 [Task 6 finding, FX-38]: the cleanup scope of FX-12 removed the evidence the hosted collector reads

Found after the one fix cycle ended, by the first hosted run of the fix-cycle head: the orchestrator's push of `49069a7` ran hosted run 44776046324 and both jobs failed at the step "Emit authored Phase 9 evidence" with `qualify-phase09: refused original evidence: retained run directory is missing or unsafe` (the Gate step and the audio steps passed, the steps after the emit never ran). Cause: FX-12 (`e68db4c`) gave every runner test a per-process scope removed at exit, including `runner/tests/test_pad_poll.c`, whose retained run directories (`result.json`, `command.args`, `input.json`, the altered fixture copy) are read by the hosted emit after the process has ended (plan 09-03.1 collector interface). A Rule 1 defect of the fix cycle itself, in a test helper (a test source, not a frozen file). The local gate, the focused CTest batches, the targeted re-check, the security re-audit and the gaps-only re-verification could not see it: no gate test reads a retained directory after its process exits, and the emit replay was not part of the cheap checks. Cost: one more commit (FX-38, `c3bdff5`), the native rehearsal that had started on the earlier candidate was stopped at its ASan stage and restarted on the new candidate (the record of a native run binds the exact source), and the covered files `runner/tests/t64_test_tree.h`, `runner/tests/test_pad_poll.c` and `scripts/tests/test_phase09_qualification.py` changed after the verifier's digest (the digest was already stale from the CONTEXT amendment K20 and this record, see the handoff in the SUMMARY). The repeat budget of Task 6 (one repeat of steps 2 to 4 for a native failure or a hosted overrun) is not spent: no native or hosted rehearsal of this plan had completed when the defect was found, and it was found by a read-only look at a hosted run that already existed.

### DEV-16 [Task 6 hosted rehearsal failure, FX-39]: four registered TIMEOUTs had less than twice the measured seconds, one fired on the hosted macOS runner

The first hosted rehearsal (run 44784312630, attempt 1, source `16849e7`) failed the job `gate-macos` at the Gate step: `harness.state-field-use-audit` ended `***Timeout 60.06 sec` against its registered 60 s (the same test took 48.7 s on that runner in run 44776046324 and 17.7 to 21.3 s natively; `gate-linux` succeeded, Gate 147 s, run 189 s, and so proved the FX-38 emit and the rest of the hosted tail on a real runner). Classified from the step log: not product code, not frozen logic and not an infrastructure fault; a registered deadline of 1.0 to 1.2 times the hosted figure, which a rerun would pass or fail on runner noise and which would then fail 09-11's qualifying runs, where logic cannot be edited. A rerun of the same SHA was therefore not used. The repair is data only (CMake registration TIMEOUTs, the lever plan 09-10 Task 6 step 5 names) and is the one repeat of steps 2 to 4: the native rehearsal on `16849e7` (exit 0, 1,652 s) and its record are superseded by a new native rehearsal and a new hosted rehearsal on the repaired candidate. See FX-39.

### DEV-17 [accepted hosted time deviation, orchestrator decision]: the macOS cold projection of the hosted rehearsal is 619 s against the 600 s gate

Plan 09-10 Task 6 step 4 gates the hosted rehearsal on warm at most 540 s and a cold projection (warm plus 186 s macOS or 84 s Linux) at most 600 s on both jobs. The rehearsal on `8dd6af1` (run 44790405712, attempt 1) measured gate-linux warm 242 s, cold projection 326 s, and gate-macos warm 433 s, cold projection 619 s, so the macOS cold projection is 19 s over. The orchestrator **accepted the measured rehearsal with this recorded deviation and ordered no third candidate cycle**. Reasoning: the warm figure 433 s is inside the 540 s warm limit; the cold figure is a projection (the warm time plus the 186 s Binutils source build, F-096 and F-100) that matters only on a Binutils cache miss and then only against the 10 minute job timeout; a cold run was never made or spent; the same macOS job measured 349 s at the plan base `d4ccad5`, 420 s at `7d8f817` and 433 s here on near identical code, so a single sample is as noisy as the margin; both jobs were green and the collector and the rehearsal verifier passed on the pair. The plan's second verify block (the timing awk) therefore exits 1 on the macOS line by design and that exit is this accepted deviation, not a defect; its output is in `build/gate/09-10-timing.txt`. No frozen `.github/workflows/ci.yml` edit is made. **Mitigation obligation for 09-11** (carried in the SUMMARY handoff): before 09-11's first push confirm with `gh cache list` that the Binutils cache entries for both operating systems exist and are fresh with the key unchanged (the key hashes `scripts/build-boot-fixtures.sh`, which is frozen), and treat a cold macOS run as a known risk (a cold macOS job would take about 619 s against the 600 s timeout and could be cancelled by it); 09-11 records its own measured seconds against the same figures.

### Blocking set

Blocking (every CR, every WR with a correctness, security or evidence effect, every open high threat, every VG about a success criterion, a locked decision or a validation row):

| Root | Findings | Group |
| --- | --- | --- |
| RDP-STATE | L1 CR-01, L2 CR-01, L5 CR-01, T-09-87, VG-01 | G1 |
| IMPORT-BOUNDS | L2 CR-02, T-09-67, T-09-73 (mutant and campaign part), VG-02 | G1 |
| native time limit | L4 CR-01, VG-03 | G3 |
| D-44 budget | L1 WR-01, VG-04 | G1 |
| arguments aliasing | L2 WR-01 (VG-09 part) | G1 |
| evidence tooling | L2 WR-02, L2 WR-03, L2 WR-04, VG-09, T-09-73 audit part, L3 IN-03 | G1 |
| tests that cannot fail | L5 WR-01 to WR-03, L1 WR-02 (tests), VG-05, VG-06 | G1 |
| guard pin | L4 WR-02 (pin part), L1 WR-02 (guard part) | G3 |
| full matrix | L4 WR-01, VG-08 | G3 |
| publisher and resume | L3 WR-01, L3 WR-02, VG-10 | G2 |
| options | R-01 to R-04, R-06, VG-07, T-09-82 | G2 |
| spec text | L3 WR-03 (G2), L5 WR-04, L1 IN-01, VG-11 (G1 and G2) | G1, G2 |

Size: 5 critical findings on 3 root causes, 15 warnings, 4 open threats, 11 verifier gaps. Stop condition: not met beyond the bound raise above (no locked decision changes; the frozen-file edits are listed per group).

## Triage table

Classes: LOGIC-core, LOGIC-runner, TEST, DOC, FROZEN-FILE, STATE-CONTENT-F1, DESIGN. Dispositions: FIX (group), INFO (recorded with owner and reason), NOT-A-DEFECT (evidence).

### Lane L1 (core device models and identity)

| Id | Sev | Class | Disposition | Rationale |
| --- | --- | --- | --- | --- |
| L1 CR-01 | BLOCKER | STATE-CONTENT-F1, LOGIC-core | FIX G1 (RDP-STATE) | Carry the RDP combiner block and the memory-coverage table in the canonical state and the digest of the composite family (descriptor 12), importer rules, commit stops zeroing them; digest-completeness flip test, same-triangle continuation control, authored coverage case; no change for the 17 pre-Phase-9 ids. |
| L1 WR-01 | WARNING | LOGIC-core | FIX G1 | D-44 budget keyed on the unadjusted effective address; budget from the address the store performs (partial stores align down, reverse endian). RED from the review. |
| L1 WR-02 | WARNING | TEST, FROZEN-FILE | FIX G1 (controls) + G3 (guard pin) | One behavior control per RDRAM writer (SI RD64B completion, SRAM to RDRAM DMA) for the CPU link and the coverage marks, with the out-of-tree mutant as RED; the pin of `t64_save_sram_dma_apply` and the SI completion bodies is the isolated G3 refresh. |
| L1 IN-01 | INFO | DOC | FIX G1 | `si-pif-controller.md` channel 4 text (shared with L5 WR-04). |
| L1 IN-02 | INFO | DOC | FIX G1 | Stale "reserved" comment on `T64_STATE_SECTION_SAVE_MEDIA`. |
| L1 IN-03 | INFO | LOGIC-core, DOC | FIX G1 if trivial (shared helper), else INFO | `write_generation` aliasing across replacement stated in `save-media.md`; one shared saturating increment helper (R-05, L5 IN-05, L2 IN-06 g). |
| L1 IN-04 | INFO | DOC | FIX G1 | `t64_machine_bus_write` comment: name the sticky-error and trace effects of refused device writes. The public name of the third SI trace kind is INFO (provisional API, no freeze; owner Phase 10). |
| L1 IN-05 | INFO | LOGIC-core | INFO recorded, fix if trivial (G1) | `rsp_observed_*` not cleared by replacement: no digest, observer or evidence path of the family depends on it; clear in `reset_guest_state` only if the guard-pinned bodies are untouched, else record in the replacement table (owner Phase 10). |

### Lane L2 (state container, importer, audit scripts)

| Id | Sev | Class | Disposition | Rationale |
| --- | --- | --- | --- | --- |
| L2 CR-01 | BLOCKER | STATE-CONTENT-F1, LOGIC-core | FIX G1 (RDP-STATE) | Same root as L1 CR-01; the reviewer's option 1 (extend the canonical state) is chosen by the orchestrator, so the digest also sees the members. |
| L2 CR-02 | BLOCKER | LOGIC-core | FIX G1 (IMPORT-BOUNDS) | Triangle render in progress accepted without the acceptance-time bounds (rows against the 1,024-row coverage array, packet words, shade and texture products); fixed together with RDP-STATE because carrying the combiner block makes the out-of-array write live. |
| L2 WR-01 | WARNING | LOGIC-core | FIX G1 | Argument refusals must not write the report into a span that aliases the machine; RED for NULL blob, zero length and bad flags. |
| L2 WR-02 | WARNING | TEST | FIX G1 | Tracer observer-range mutant compares the full tuple (status, reason, section, rule) and edits the field that raises `LEN_LE`; the runner prints the rule. |
| L2 WR-03 | WARNING | TEST | FIX G1 | Field-use scan lookbehind (91 members), newly unmapped names mapped with specific rules, selftest seeds a nested access. |
| L2 WR-04 | WARNING | TEST | FIX G1 | Campaign and sweep reach field-level readers (section-biased mutation, header total and trailer re-signed), reached-rule counters asserted, the tautology assertion replaced. |
| L2 IN-01 | INFO | DOC | INFO recorded; mechanism fixed by G2 (L3 WR-02, FX-11) | A runner-published state re-imported by the runner; the claim text is 09-11. |
| L2 IN-02 | INFO | LOGIC-core | INFO recorded | Dead scratch facts and an audit-only public rule constant; no behavior effect; owner Phase 10 cleanup. |
| L2 IN-03 | INFO | DOC | FIX G1 | Two header sentences (export BAD_STORAGE clear, import blob must not change during the call). |
| L2 IN-04 | INFO | LOGIC-core | INFO recorded | Retained packet metadata is memory-safe; the runner validates it; host contract of Phase 10. |
| L2 IN-05 | INFO | TEST | INFO recorded; time limit is G3 (VG-03) | Splitting the heavy cases into their own CTest names is not possible (all 45 K13 names are used, no ASan-only name); the aggregate keeps TIMEOUT 1800. |
| L2 IN-06 | INFO | DOC, LOGIC-core | NOT-A-DEFECT (f), FIX G1 (g, shared helper with L1 IN-03), NOT-A-DEFECT (B05) | (f) is L1 IN-05; (g) see L1 IN-03; the B05 row text already says "(direct test-only edit)" in its Reached column. |

### Lane L3 (runner)

| Id | Sev | Class | Disposition | Rationale |
| --- | --- | --- | --- | --- |
| L3 WR-01 | WARNING | LOGIC-runner, FROZEN-FILE | FIX G2, done (FX-10, FD-03) | Publisher failure after the rename (frozen file `runner/t64_artifact_publisher.c`: G2 records the FD entry). |
| L3 WR-02 | WARNING | LOGIC-runner | FIX G2, done (FX-11) | Resumed run of a runner-published state: closed unknown-log form, no pass-rule weakening. |
| L3 WR-03 | WARNING | DOC | FIX G2, done (FX-15) | `result-protocol.md` pass rule text, with a drift control against the fixture table. |
| L3 IN-01 | INFO | LOGIC-runner | STATED LIMIT, documented (FX-16, no code change) | Replay interval versus the 4,096 checkpoint cap: an early usage refusal needs the end tick of the guest, which option parse does not know; a refusal on the bound alone would refuse a run that completes (measured on the mix row, FX-16). Owner of a closed detail name for the recorder failures: Phase 10. |
| L3 IN-02 | INFO | LOGIC-runner | FIX G2, done (FX-17) | Dead `t64_replay_publish` removed; the in-process driver (`t64_replay_play`, `t64_replay_report_check`, `t64_replay_player_end_at`) stays because the unit cases use it (moving it into the test target is a Phase 10 refactor). |
| L3 IN-03 | INFO | TEST | FIX G1 (verified by G2: the tracer compares the full tuple in process, FX-05; the tracer lines were regenerated after FX-11) | Tracer prints an expected status it never observes: fixed with L2 WR-02. |
| L3 IN-04 | INFO | DOC | FIX G1 | `runner/t64_real.c` 1115-1118 comment and `state-container.md` 552-556 sentence. |
| L3 IN-05 | INFO | TEST | FIX G2, done (FX-12) | Five runner tests never remove run directories. |
| L3 IN-06 | INFO | LOGIC-runner | failed `--meta` write in real mode: FIX G2, done (FX-18); `T64_REAL_KEEP_WORKSET`: INFO owner Phase 10 (gating it broke two Phase 8 tests, reason in FX-18) | Pre-Phase-9 carries; neither leaks blob bytes today. |

### Lane L4 (harness, guards, CI, scanners)

| Id | Sev | Class | Disposition | Rationale |
| --- | --- | --- | --- | --- |
| L4 CR-01 | BLOCKER | FROZEN-FILE | FIX G3 | 600 s wrapper versus 631.8 s ASan test; wrapper, outer limit, verifier bound, pinned test, TIMEOUT-reading control; FD entries. |
| L4 WR-01 | WARNING | TEST, FROZEN-FILE | FIX G3 | Full continuation matrix as a recorded native step if it costs at most about 120 s, else "sampled" wording bound. |
| L4 WR-02 | WARNING | FROZEN-FILE | FIX G3 (isolated pin refresh) | After G1 stops editing the bodies; mutation control deletes the invalidation call. |
| L4 IN-01 | INFO | FROZEN-FILE | FIX G3 | gcc:13 120 s limit re-derived (closure 51 to 64 files); cold ASan build measured. |
| L4 IN-02 | INFO | TEST | INFO recorded | Case inventory proves the case list, not a CTest mapping; every listed case was traced as reached; owner Phase 10 checker follow-up. |
| L4 IN-03 | INFO | FROZEN-FILE | FIX G3 if trivial | Runner build-id input list omits headers. |
| L4 IN-04 | INFO | DESIGN | INFO recorded | No rehearsal marker in the closed public key set; binding is by SHA. |
| L4 IN-05 | INFO | TEST | INFO recorded; see DEV note for identity changes | The first hosted pair record is coupled to authored rows; an identity-affecting fix regenerates the first pair only if a row identity changes (G1 changes no profile hash or transcript hash; see the golden record). |
| L4 IN-06 | INFO | DESIGN | INFO recorded | Oracle request pin re-bound on every manifest edit: owner decision of the orchestrator (owner-approved identity-only rebind); G1 avoids manifest edits. |

### Lane L5 (tests, fixtures, oracles, specs, claims, evidence)

| Id | Sev | Class | Disposition | Rationale |
| --- | --- | --- | --- | --- |
| L5 CR-01 | BLOCKER | STATE-CONTENT-F1 | FIX G1 (RDP-STATE) | Same root; plus the digest-completeness test and the corrected `machine-contract.md` lines. |
| L5 WR-01 | WARNING | TEST | FIX G1 | SI active-descriptor immutability control (RD64B and WR64B), accepted baseline in the same setup. |
| L5 WR-02 | WARNING | TEST | FIX G1 | Non-0xFF fill (0xA5) in the timeout, absent and busy-read frames of the unit tests; mutants. The guest-visible fixtures (ROM frames, oracle rows) stay: re-pinning ROM rows edits the manifest and the oracle authority files (see INFO below). |
| L5 WR-03 | WARNING | TEST, DOC | FIX G1 | Spec text for the RD64B and SRAM DMA invalidation, one control each with a non-overlapping accepted baseline. |
| L5 WR-04 | WARNING | DOC | FIX G1 | `si-pif-controller.md` channel 4 and device map. |
| L5 IN-01 | INFO | TEST | FIX G1 where cheap | Accept halves of `budget_room_pair` and `budget_sequence_pair` assert OK and four retired instructions (rows touched by FX for D-44); helper-only controls labeled as comparator self-checks. |
| L5 IN-02 | INFO | DOC | FIX G1 | Stale header comment in `test_sched_ledger.c`. |
| L5 IN-03 | INFO | TEST | INFO recorded | `T64_CHECK(true)` padding of check totals; do not quote raw check counts as strength (09-11 claim wording). |
| L5 IN-04 | INFO | TEST | FIX G1 | Dead digest in `dma_refusal_rows`, two `\\n` diagnostics. |
| L5 IN-05 | INFO | TEST | FIX G1 with L1 IN-03 | One shared helper, one direct-state control at `UINT64_MAX`, one header sentence. |
| L5 IN-06 | INFO | DOC | INFO recorded | Mutant results of the 09-03 to 09-05 specs are historical records in the plan summaries; the mutants added by this cycle are named in the FX entries. |
| L5 IN-07 | INFO | DOC | FIX G1 | `machine-contract.md` input paragraph scoped to the profiles without the family tuple. |
| L5 IN-08 | INFO | TEST | INFO recorded | `fixtures/replay/mix.S` spin range assertion: a source edit changes the pinned manifest source hash and the oracle authority files; build-time hazard only for an override; owner Phase 10. |
| L5 IN-09 | INFO | DESIGN | INFO recorded | Phase 9 plans edit the Phase 7 authority record: orchestrator-owned, owner-approved identity-only rebinds. |

### Threats of `09-SECURITY.md`

| Id | Sev | Class | Disposition | Rationale |
| --- | --- | --- | --- | --- |
| T-09-67 | high, OPEN | LOGIC-core | FIX G1 (IMPORT-BOUNDS) | Row range, packet words and products bounded at import; mutant `render-triangle-row-beyond-coverage`. |
| T-09-73 | high, OPEN | TEST | FIX G1 | Mapping names the coverage-height bound, nested-access scan, mutant rows reach the rule, campaign base blob with a render in progress. |
| T-09-82 | high, OPEN | TEST, DOC | FIX G2, done (FX-13 tests: R-01, R-02, R-04, the nine pairs; FX-14 spec: R-03, R-06, six INPUT pairs defined) + G1 (resume-equal property rests on RDP-STATE) | The mid-tick row and the write_generation case exist and pass; the property they establish now includes the RDP members. |
| T-09-87 | high, OPEN | STATE-CONTENT-F1 | FIX G1 (RDP-STATE) | See above. |
| T-09-65 | low, accepted | DESIGN | INFO recorded | Orchestrator: add to the accepted-risk log. |
| Non-blocking F-1 to F-11 | info, low | various | INFO recorded; F-1 is fixed with L2 WR-03; F-2, F-3, F-5, F-6 are Phase 10 hardening items except F-6, which G2 fixed with L3 WR-01 (FX-10) | No threat counts toward `threats_open`. |

### Verifier gaps of `09-VERIFICATION.md`

| Id | Class | Disposition | Owner and rationale |
| --- | --- | --- | --- |
| VG-01 | blocking | FIX G1 | RDP-STATE. |
| VG-02 | blocking | FIX G1 | IMPORT-BOUNDS. |
| VG-03 | blocking | FIX G3 | Native stage wrapper. |
| VG-04 | non-blocking | FIX G1 | D-44 budget (L1 WR-01). |
| VG-05 | non-blocking | FIX G1 (controls) + G3 (guard pins) | L5 WR-01 to WR-03, L1 WR-02. |
| VG-06 | non-blocking | FIX G1 | EEPROM read with rx above 8, SRAM DMA ending exactly at and two bytes past the window end, each with a mutant. |
| VG-07 | non-blocking | FIX G2, done (FX-13, FX-14) | Nine pair tests (all 36 pairs), synthetic-mode refusal, `--save-import-fd`, spec sentences for the six INPUT combinations. |
| VG-08 | non-blocking | FIX G3 | Full matrix as a native step or "sampled" wording bound. |
| VG-09 | non-blocking | FIX G1 | Campaign, tracer tuple, field-use scan, report span (L2 WR-01 to WR-04). |
| VG-10 | non-blocking | FIX G2, done (FX-10, FX-11) | Publisher failure contract and resumed-run record. |
| VG-11 | non-blocking | FIX G1 (`si-pif-controller.md`, `machine-contract.md`) + G2 (`result-protocol.md`, done FX-15) + orchestrator (09-VALIDATION status at phase completion: still draft, G2 did not touch it because its rows resolve only against the final verification) | Spec and record drift. |

### Drift sweep records R-01 to R-06 (Task 2)

| Id | Disposition | Owner and rationale |
| --- | --- | --- |
| R-01 | FIX G2, done (FX-13) | Test for synthetic-mode refusal of the 19 Phase 9 flags. |
| R-02 | FIX G2, done (FX-13) | Tests for the nine defined pairs that have none (the table of all 36 pairs). |
| R-03 | FIX G2, done (FX-14) | Define the six INPUT combinations (accepted by design) in the spec. |
| R-04 | FIX G2, done (FX-13) | Direct runner test of `--save-import-fd`. |
| R-05 | FIX G1 if trivial | Shared saturating increment helper (with L1 IN-03). |
| R-06 | FIX G2, done (FX-14) | The 45 of 64 slot sum bound and the replay-row wording of the spec matrix. |

### Tracer and confirmation items

The tracer reaches the inventory reason but not the inventory tuple (L2 WR-02, L3 IN-03): FIX G1. The `busy_until` bound (M5) is confirmed present and tested: NOT-A-DEFECT. The replay `save_init` limit (M6) is a stated limit; the unpinned refusal behind it is VG-07 (G2).

## Fixed issues (FX entries)

Group G1 (this executor). One commit per fix, a RED observation before each fix (the logs are untracked under `build/gate`), the control run next to an accepted baseline of the same setup, and every control compiled under the gate flags with `-Werror`.

### FX-01 [LOGIC-core] triangle render in progress bounded at import: L2 CR-02, T-09-67, T-09-73 (mutant part), VG-02

Commits a7e125f (fix) and 45b61d2 (isolated pin refresh, N1). **RED** (`build/gate/09-10-fx-01-red.log`): eight new mutant rows `render-triangle-*` each returned status 0 (accepted) next to an accepted baseline, 40 failing checks. **Fix:** `t64_rdp_triangle_bounds` is split into `t64_rdp_triangle_extent` (geometry, RDRAM extent, shade and texture products from the packet words and a view of the registers) and the unchanged tile and depth preflight; the reader of section 8 builds the same view from the blob and checks, in order: rows below the 1,024-row coverage height (`INDEX_LT`), accepted opcode (`ENUM_CLOSED`), word count of the opcode, shade flag, the extent and products (`LEN_LE`), first and last row, `render_y` between them, and no triangle flag without a render in progress. **Green:** 0 failures. Pin refresh: derived `texture_products_are_bounded` by two substitutions; `t64_rdp_triangle_bounds` evidenced by a differential of the old and new function over 3.2 million random states on eight profiles (679,701 accepted, 128,425 accepted non-empty, zero differences); the guard rejects an unauthorized change of each new body (control run) and fails when either new pin row is deleted. The UBSan overflow and the out-of-array write of the review are reproduced by the accepted blobs of the RED rows; the sanitizer run of the cycle shows none after the fix.

### FX-02 [STATE-CONTENT-F1] RDP combiner block and memory coverage table carried: L1 CR-01, L2 CR-01, L5 CR-01, T-09-87, VG-01

Commit 82450b0. **RED** (`build/gate/09-10-fx-02-red.log`, `-b17.log`, pre-carry walker and reader, new tests): the completeness sweep reports `blend_color` and the coverage table outside the stream (41 failures) and a combine-mode flip leaves the digest unchanged; `rdp-state-carry` reproduces the review (source `triangle_count` 1 and refusal 0, peer 0 and refusal 5, framebuffers and digests differ; coverage 0 on the peer against 72, and against 12 on the authored `rdp-fill` fixture); row B17 differs at checkpoints +310, +10,000 and +30,000; the five `rdp-carried-*` mutants have no section tail to edit. **Fix:** walker, reader and rules as DEV-02. **Green:** all of the above plus the independent encoder of the tail (`v25-reference`), the strip differential with the new section-8 span, the F1 table, the boundary rows B06 and B07 rebuilt with their coverage marks, the replay pins. The completeness sweep flips the first byte of every member of `t64_rdp_state` (a size assertion forces new members into the table) on the stem id and a representative set on the three save ids; the named exclusions are the five depth members (K1), the three `triangle_last_coverage_*` observation members, the packet words at or after the count and padding. The importer's no-change-for-old-profiles scope limit holds (regression above).

### FX-03 [LOGIC-core] report span judged before any argument refusal: L2 WR-01, VG-09

Commit 23e3495 (and 20d2bb4, a rename of a local that shadowed the `row` typedef, found by the gcc:13 closure). **RED** (`build/gate/09-10-fx-03-red.log`): with the report span inside the machine allocation a NULL blob, a zero length, a length above the ceiling, a bad flag word and a blob in live storage each overwrote the aliased bytes (a canary per row, five failures). **Fix:** `state_check_arguments` decides the usability of the report span first. **Green:** each argument row runs with a disjoint span (the accepted arm that proves its reason) and with an aliasing span compared byte for byte.

### FX-04 [LOGIC-core] D-44 budget from the word the store writes: L1 WR-01, VG-04, L5 IN-01

Commit 0903cf1. **RED** (`build/gate/09-10-fx-04-red.log`): a `swr` at byte 3 of RD64B with room for two events retired as the fourth instruction, queued both SI events (queue 16 against 14), changed the SI state and the digest, and the sequence analogue failed alike; the `sw` control refused atomically. **Fix:** the budget translates a partial store's byte as the store path does and keys on the aligned word (the reverse-endian lane exchange for a full word). **Green:** refuse-then-accept pairs for `swr` and `swl` at both registers and the sequence analogue; the accept halves assert OK and four retired instructions. Limit: the reverse-endian branch mirrors the store path by reading; it is not reachable from the privileged unmapped window in user mode, so it has no control (recorded). **Correction (G4, FX-28):** the arm is reachable through a TLB-mapped SI page in user mode with Status.RE and Config.BE and has controls since FX-28 (R1 RC-05).

### FX-05 [TEST] tracer reaches and compares the full observer tuple: L2 WR-02, L3 IN-03, VG-09

Commit 916055f. **RED** (`build/gate/09-10-fx-05-red.log`): the tracer edited the video head and the importer raised `STATE_RANGE/OBSERVER_RANGE/16/INDEX_LT`, not the `LEN_LE` tuple of the row it is named after; a reason-only comparator passed. **Fix:** the tracer edits the drain cursor and imports the same blob in process, comparing status, reason, section, rule and position with the inventory row next to the accepted baseline; the observer reader checks each value right after its read, so the position names the member; the result line prints expected and observed tuples. **Green:** rows `none` and `observer-range` (`build/gate/09-10-tracer-none.txt`, `-mutant.txt`).

### FX-06 [TEST] nested accesses scanned, one row per bound: L2 WR-03, T-09-73 (audit part), VG-09

Commit 96409d2. **RED** (`build/gate/09-10-fx-06-red.log`): a seeded member reached as `machine->pi.seeded_nested_index` was not found by the scan of 09-09 (findings empty). **Fix:** the pattern accepts a dot after any expression; 20 newly visible names are mapped (members 72 to 92) with rules and sites; a member may have one row per distinct bound (`render_y` also `INDEX_LT`, the coverage-height bound); the selftest gains the nested seed (six cases). **Green:** `--check` findings 0, selftest six of six.

### FX-07 [TEST] campaign and sweep aimed at the readers: L2 WR-04, T-09-73, VG-09

Commit 6e7cee7. **RED** (`build/gate/09-10-fx-07-red.log`): with the reach assertion the campaign at the gate default failed (12 of 12 iterations at the container level; the old assertion was true by construction). **Fix:** windows over the rule-bearing fields, section-aware fallback, header total updated after truncation and extension, six draws in eight re-signed, a second run on a shaded triangle render in progress, the sweep sampling the same windows. **Green:** 12 iterations give 9 refusals (5 at reader level) and 3 accepted; 120 iterations 36 reader-level refusals and 35 accepted.

### FX-08 [TEST] tests that can fail: L5 WR-01 to WR-03, L1 WR-02 (controls), VG-05, VG-06

Commit 0ec7019. **RED** (`build/gate/09-10-fx-08-red.log`, driver `build/gate/09-10-regress/mutants_fx08.py`): eleven working-tree mutants, each built under the gate flags and restored, all killed: SI completion reading the live latch (2 failures), the receive area filled with 0xFF in the joybus timeout branch (8 and 26), in the save timeout class (26), in the busy read (1) and in the absent branch (17), the RD64B completion dropping both invalidations (33), only the coverage one (32) or only the link one (1), the SRAM to RDRAM apply dropping both (2), a read with rx 9 classified valid (8), a DMA two bytes past the window tolerated (2); before the new controls every one of these survived. **Fix:** tests only (immutability of the active descriptor for RD64B and WR64B, frames with a 0xA5 receive fill, invalidation arms with a non-overlapping accepted baseline, the window-end rows) and the spec sentences. The guard pin of the two RDRAM writers (L4 WR-02) stays with G3.

### FX-09 [LOGIC-core, DOC] replacement clears the RSP observation, one generation helper, drift: L1 IN-01 to IN-05, L2 IN-03, L3 IN-04, L5 WR-04, IN-02, IN-04, IN-05, IN-07, R-05, VG-11

Commit 2fdd2dd. **RED** (`build/gate/09-10-fx-09-red.log`): after a replacement the `rsp_observed_*` members and the valid flag survived (6 failures over the three save ids). **Fix:** `reset_guest_state` clears them; `t64_save_generation_advance` is the one saturating increment of both writers with a direct-state control at the maximum; spec and comment drift corrected (list in the commit). **Green:** full gate CTest 224 of 224 after this commit.

### Recorded, not fixed in G1 (INFO)

L1 IN-04 (the public name of the third SI trace kind: provisional API, Phase 10), L2 IN-01 (mechanism fixed by G2), L2 IN-02 and IN-04 (dead facts, packet metadata: Phase 10), L2 IN-05 (K13: no new CTest name), L5 IN-03, IN-06, IN-08, IN-09, L4 IN-06 (owner decisions), the guest-visible fixtures and their oracle rows keep their 0xFF fill (re-pinning ROM rows edits the manifest and the Phase 7 authority files), the label of helper-only controls as comparator self-checks (L5 IN-01, comment only, not done).

## Fixed issues, group G2 (FX-10 to FX-18)

Group G2 (runner, publisher, spec text of the options, tests of options). One commit per finding, RED first (the logs are untracked under `build/gate`), every control compiled under the gate flags with `-Werror` and run after an accepted baseline of the same setup. No new CTest name (45 of 45 used) and no case-list change: every new row runs inside an existing case, so `docs/spec/phase09-cases.json` is untouched. No manifest row changed, so the oracle authority files were not touched; no golden changed.

### FX-10 [LOGIC-runner, FD-03] a failed publication publishes nothing and strands no stage: L3 WR-01, VG-10, SECURITY F-6

Commit 07e87ba. **RED** (`build/gate/09-10-fx-10-red.log`: the publisher with the new test-twin plumbing and the old logic, the new controls: 16 failing checks; `build/gate/09-10-fx-10-red-mutant.log`: a working-tree mutant of the fixed module without the withdrawal and the two directory-sync checks, 21 failing checks over the publisher case and the supervised state export): a directory sync that fails after the commit left the leaf published and the stage in place and the next publish of the same leaf was refused; a replaced leaf left the stage; the preflight accepted a filesystem whose directory sync fails; through the supervised runner the state export with the post-commit fault left the leaf in the artifact directory and a run with the preflight fault was accepted and passed. **Fix:** as DEV-04. `docs/spec/save-media.md` states the contract. **Controls:** `controls.artifact-substitution` (case `substitution`): the accepted baseline (hooks installed, no fault) publishes the leaf and leaves no stage; a failure of the pre-rename sync (nothing published, the caller's abort removes the stage); a failure of the post-rename sync (leaf withdrawn, stage removed before any abort); the published leaf replaced by a symlink to a victim file and by another principal's regular file (reported as `artifact-substitution`, the entry and the victim untouched, the stage removed); the preflight with a forced sync fault (`artifact-write`, directory left as it was); and `runner.state-export` (case `export-fsize-at-state-size`): the supervised state export through the test twin with `artifact-sync-after-rename` ends in the closed `state-export-publish` detail with the accepted run's tick and an empty directory, with `artifact-sync-preflight` in the prelaunch refusal at guest tick 0 with an empty directory, next to the accepted export. Limit: an unlink that itself fails after the commit leaves the leaf; the record names the same failure (stated in `save-media.md`); a substitution by the runner's own effective user between the check and the unlink is outside the Phase 8 CR-01 threat class (another principal), as the review stated.

### FX-11 [LOGIC-runner] a resumed run of a runner-published state ends in a record the checker accepts: L3 WR-02, VG-10, L2 IN-01

Commit 98d1b57. **RED** (`build/gate/09-10-fx-11-red.log`, 12 failing checks over `import-accepted-continues` and the tracer baseline `none`): the child wrote `log_bytes` 0, `log_bytes_total` 20 and no digest, which the checker refuses (R-CONSISTENCY); the supervised record was the closed `report-error` / `report-invalid` one with guest tick 0 and no state digest. **Fix:** the finalize block of `runner/t64_real.c` takes the closed unknown-log branch of a saturated total (both counts zero, no digest, `harness-error` / `host-error`) also when the machine's attempted total disagrees with the bytes the run drained and the detail is not `output-overflow`; the overflow record keeps its exact counts and digest; no pass rule was changed and a resumed run still never reports pass. **Green:** the resumed run of the runner's own published state ends in the accepted record with the uninterrupted run's guest tick (838,791) and final state digest, the checker accepts both the child's record and the supervised record, and the uninterrupted run is the same-setup comparator (known log, digest present); the mid-run native state (`run C`) still ends in its complete-log record. `docs/spec/result-protocol.md` and `docs/spec/state-container.md` state the form. The tracer lines (`build/gate/09-10-tracer-none.txt`, `-mutant.txt`, `09-10-tracer.txt`) were regenerated: row `none` prints `outcome=resumed`, row `observer-range` is unchanged.

### FX-12 [TEST] five runner tests remove the run directories they create: L3 IN-05

Commit e68db4c. **RED** (`build/gate/09-10-fx-12-red.log`): one run of the five unfixed binaries left 51, 2, 3 and 61 entries in `09-02-evidence`, `09-03-evidence`, `09-04-evidence` and `09-07-evidence`. **Fix:** one header-only helper (`runner/tests/t64_test_tree.h`) gives each test process a private scope under the shared evidence directory and removes it at normal exit (other processes' scopes are never touched, a forked child cannot remove its parent's scope, `T64_KEEP_TEST_EVIDENCE` keeps the tree for debugging). **Green:** 0 entries after the same run (`build/gate/09-10-fx-12-green.log`).

### FX-13 [TEST] option-pair tests: VG-07, R-01, R-02, R-04, T-09-82

Commits edf2651 and 0a13f91 (a follow-up that lowered the matrix bound to 20,000 ticks for the sanitizer cost). Added inside existing cases (no new name): `argv-bound` of `runner.supervisor-argv-bound` runs each of the nine families alone as the accepted baseline (not a usage refusal), all 36 pairs (refused as usage at guest tick 0 with the artifact and raw directories empty, or accepted), the four producers without the pair, and the supervisor in synthetic mode with each of the 19 Phase 9 flags plus RAW and INPUT after a baseline that ends in the stand-in child's own `no-result` record; `import-refused` of `runner.save-roundtrip` runs the direct runner with `--save-import-fd` (accepted descriptor with the pinned transcript digest, then the linked, writable, offset, short and pipe descriptors as prelaunch refusals at tick 0 and a missing kind as a usage refusal); `import-option-refusals` of `runner.state-export` runs the state import with the artifact pair alone as an accepted run with an empty artifact directory. Every family carries a nonexistent path, so a supervisor that prepared a snapshot before the pair rule would report `refusal` instead of `usage`. **Correction (G4, FX-33, R2 RC-07):** only the three families that import a file (SAVE-IN, REPLAY-PLAY, STATE-IN) carry such a path, so the ordering is observable only for the refused pairs that hold one of them; the test comment now says so and the matrix test asserts its 36 rows, their uniqueness and equality with the spec table. **Before-state evidence** (these are tests of behavior the code already had, so RED is the survival of mutants before the rows existed; `build/gate/09-10-fx-13-red.log`, drivers `build/gate/09-10-regress/mutants_fx12.py` and `mutants_fx12b.py`): ten supervisor mutants and five runner mutants, each built under the gate flags, run and restored (re-run after the follow-up, all kills unchanged): replay accepting the save flags (4 failing checks), replay without the pair (2), state export without the pair (1), synthetic mode accepting `--replay-report` (1) and `--state-import-sha256` (1), raw capture with the artifact pair (1), three accepted pairs refused (SAVE or REPLAY with INPUT: 2 and 2; STATE-IN with the pair: 1 in the matrix and 1 in the state case), state import with the schedule (1 and 2), the runner accepting a linked descriptor (4), a writable descriptor (4) and never reading the save descriptor (113): all killed. Two descriptor checks of `read_snapshot` (the offset and the regular-file test) are redundant behind the read-to-length loop and their mutants are equivalent; one replay-validator check (raw capture) is redundant behind the pair rule and was not mutated.

### FX-14 [DOC] the option-pair matrix defines every pair: R-03, R-06, T-09-82

Commit b4a8c44. `docs/spec/state-container.md`: a 9 by 9 table of all 36 pairs (R refused, A accepted), the four-statement rule behind it, the six INPUT combinations accepted by design (SAVE-IN, SAVE-OUT, REPLAY-REC, REPLAY-PLAY and RAW with INPUT, and INPUT with PAIR), the pairless forms, the nineteen synthetic-mode flags, the compile-time slot sum (45 of 64, with the term-by-term sum) and what `argv-bound` (19 slots) and `argv-max-save` (27) prove, and the replay rows stated in full. The table is the one the case of FX-13 runs.

### FX-15 [DOC, TEST] the pass rule names the per-row pins: L3 WR-03, VG-11

Commit 226c9cd. **RED** (`build/gate/09-10-fx-15-red.log`): the drift control in `case_transcript_pin` derives from the fixture table that 15 rows carry their own pin with the distinct lengths 20, 68, 72, 101, 144, 191 and 192 bytes and found neither fragment in `docs/spec/result-protocol.md`. **Fix:** the pass-rule bullet now says a registered pass-eligible row passes under its own pinned length and SHA-256 or the legacy 30-byte pin, in the runner and the checker alike, and that a resumed run never reports pass; the detail list names the host details the checker accepts beyond the printed set. **Green:** `build/gate/09-10-fx-15-green.log`. The other VG-11 passages (`si-pif-controller.md`, `machine-contract.md`, the `state-container.md` descriptor sentence and the `t64_real.c` comment) were fixed by G1 (FX-09) and checked here; `09-VALIDATION.md` was not touched (see Open after G2).

### FX-16 [DOC] the replay checkpoint cap and the interval are a stated limit, no code change: L3 IN-01

Commit 362cdb2. A usage refusal of an interval that demands more than 4,096 checkpoints by the bound was implemented first, run against the real runner and **reverted**: the end tick of a guest is not known at option parse, and a rule on the bound alone refuses runs that complete. Measured on the authored mix row (`build/gate/09-10-fx-16-red.log` is the run of the two unrefused rows, 16 and 352, each of which used the whole 60 s wall budget of the test: 2 m 03 s; a separate supervised run with a 900 s wall): the mix row completes at guest tick 1,141,425, an interval of 352 takes 3,242 checkpoints (4,261 are demanded by the bound of 1,500,000), the run completed in 63 s natively and ended in `artifact-cap` at `--artifact-max-bytes` 65,536. `docs/spec/replay.md` states `floor(T / interval) <= 4096`, the artifact maximum as the first practical limit and the Phase 10 item (closed detail names for the recorder failures); the recorder side (cap, latched error, refused finish) is pinned by the existing cases of `unit.replay.suite`. A checkpoint is a full machine digest, so a boundary test that records 4,096 checkpoints is not affordable in CI (about 100 s natively); the rule's accepted side is therefore the documented limit, not a test.

### FX-17 [LOGIC-runner] the dead `t64_replay_publish` is removed from the production runner: L3 IN-02

Commit 0cfa3b7. The function had no caller anywhere in the repository; the object of the HEAD module defines the symbol and the linked production runner no longer does (`build/gate/09-10-fx-17-red.log`). The header comment now says the module does not publish. The in-process driver (`t64_replay_play`, `t64_replay_report_check`, `t64_replay_player_end_at`) stays: the unit cases use it, and moving it into the test target is a Phase 10 refactor (the real controls cover the runner's own loop). `unit.replay.suite` and `runner.replay-mix` pass.

### FX-18 [LOGIC-runner] a failed run-meta write fails a real-mode run closed: L3 IN-06

Commit 66f098f. **RED** (`build/gate/09-10-fx-18-red.log`): a real-mode run with `--meta` in a missing directory exited 0 with a valid pass record (the failure was ignored by `(void)atomic_write`). **Fix:** the supervisor prints the diagnostic synthetic mode already printed and exits 1; the result record is untouched; the accepted baseline is the same run with a writable meta path. `docs/spec/result-protocol.md` states the rule. **Not done, recorded for Phase 10:** `T64_REAL_KEEP_WORKSET` stays honored by the production supervisor. Gating it behind `T64_SUPERVISOR_TESTING` was implemented, the full runner batch then failed `runner.raw-capture` and `controls.real.raw-capture-io-errors` (the `actual-av-bundle` case drives the production supervisor with the switch to read the retained producer report, which is the production-path evidence of Phase 8), and the gate was reverted (`build/gate/09-10-fx-18-keep-workset-attempt.log`). A retention mechanism that is not in the production binary needs a Phase 8 test redesign, not a one-line gate; the retained set holds the closed import reason line and the guest transcript, never blob bytes, and only a caller that sets the variable gets it.

### Recorded, not fixed in G2 (INFO)

- L3 IN-01: closed result details for the recorder failures (checkpoint, op and byte caps ending in the generic `host-error`) need a result-protocol and checker change: Phase 10 (stated limit in `replay.md`, FX-16).
- L3 IN-02, second half: the in-process replay driver duplicates the runner's play loop with different terminal logic; the unit cases validate the driver, the real controls the runner loop; unifying them is a Phase 10 refactor.
- L3 IN-06: `T64_REAL_KEEP_WORKSET` in the production supervisor (FX-18).
- `09-VALIDATION.md` frontmatter and status column (VG-11): orchestrator, at phase completion.
- The result-protocol drift control of FX-15 covers the pass-rule sentence only; the closed `detail` vocabulary is still compared with the checker by the existing vocabulary case, not by a spec-versus-source check.

## Golden record

Every changed golden line is STATE-CONTENT-F1 (FX-02); no golden changed in FX-01, FX-03 to FX-09, and no golden changed in group G2 (FX-10 to FX-18: no state content, digest, profile, transcript pin or manifest row moved).

| Golden | Before | After |
| --- | --- | --- |
| F1 table, section 8 length, rows `idle-stem`, `idle-e4k`, `idle-e16k`, `idle-s32k` (`core/tests/test_sched_matrix.c`, `docs/spec/machine-contract.md`) | 2,103,016 | 4,200,260 |
| F1 table, section 8 SHA-256 of those four rows | 87adef4ee097...8065 | b6c352fc4a13...0e69 |
| F1 table, section 8 length, rows `all-stem`, `all-s32k` | 2,103,112 | 4,200,356 |
| F1 table, section 8 SHA-256 of those two rows | 68e538d595cc...a705c | 9f7eca0565f3...949f |
| the other 52 F1 rows (sections 2 to 7, 9, 10, 11) | unchanged | unchanged (`--print-sections` before and after: six rows differ, all section 8) |
| `S8_IDLE_LENGTH` (`core/tests/test_state_import.c`) | 2,103,016 | 4,200,260 |
| `STEM_IDLE_DIGEST` (`core/tests/test_save.c`) | 5ed31137...d9b0 | ec4be64f...a5d8 |
| replay-mix checkpoint, end and canonical identity pins (`runner/tests/replay_mix_golden.h`: 7 + 1 pre-queue, 5 + 1 just-in-time digests; canonical SHA-256 of both documents; sizes 2,013 and 1,822 unchanged) | see the header history | re-captured by `test_replay --print-goldens` at advance steps 4096 and 7 (equal); the canonical values re-derived by a Python procedure that first reproduced the old pins byte for byte (`build/gate/09-10-regress/replay_canon.py`) |
| `op-digest-uppercase` mutation row of `runner/tests/test_replay.c` (text keyed to the first digest) | `4a87` | `d6a7` |
| size bounds (`core/tests/test_state_export.c`, `runner/tests/test_state_runner.c`) | lower 6,303,756 | lower 8,400,908 (upper 11,534,336 unchanged) |
| documented sizes (`docs/spec/state-container.md`) | idle stem 6,351,551 and the rest | each plus 2,097,244 |

Unchanged on purpose: the idle digests of the 17 pre-Phase-9 ids (literals of `unit.core.digest.sink-legacy-identity`), the profile hashes, every transcript pin and every identity of `fixtures/manifest.json` and `docs/evidence/phase09-first-hosted-pair.json`; no manifest row changed, so the oracle authority files were not touched. **Correction (G4, FX-27, R1 RC-04):** the digest column of the hosted pair is the one exception to "unchanged": its four non-null `state_digest` values are digests of the digest function of source revision 2db1be4d, FX-02 changed every family digest, and a current build does not reproduce them (the neutral-script row gives a different value). The record itself is retained unedited; the values are valid for 2db1be4d only and are superseded by the 09-11 pair, which `docs/spec/si-pif-controller.md` now states. No test compares them with current output.

## Verification notes (group G1)

Run on the final G1 tree (HEAD after the commit that adds this record):

- Cheap checks: `sh tools/lint/core-lint.sh` violations 0; `sh scripts/pii-scan.sh --tree` and `sh scripts/rights-scan.sh --tree` findings 0; `sh scripts/check-claims.sh --files docs/claims/phase09.md` clean; `python3 -B scripts/check-state-field-use.py --check` findings 0 and `--selftest` six of six; the guard (`scripts/check-rsp-no-hle.py --build`) exit 0 on `build/gate` and `build/asan`, its self-test passes; `sh scripts/check-test-inventory.sh --preset gate` PASS (224) and `--preset asan` PASS (226); `python3 -I -B build/gate/09-10-criteria-check.py` map ok (rows 4, named CTest 23, claim pairs 10).
- Pinned gcc:13 closure over the 19 changed C units (core, core tests, runner, runner tests): clean, 22.5 s (`build/gate/09-10-g1-gcc13.log`). It found one real Linux-only `-Wshadow` error in the first version of the FX-03 control (a local named `row` shadowing the `row` typedef), fixed in 20d2bb4.
- Linux compile pre-flight of `docs/ci-reviews/LOG.md` F-102 without forcing a platform: the 219 C units of the gate build rerun with the Ubuntu 24.04 `cc` of the act image, exit 0, 126 s.
- Full gate CTest after FX-09: 224 of 224 passed (`build/gate/09-10-fx09-full.log`); the first full gate run after the cycle's first commits showed only `harness.state-field-use-audit` failing for the then-unmapped `rdp_memory_coverage` member, fixed before the next run. The fixture identity check passed in that first run (the fixtures regenerated by the review are byte identical to the manifest hashes).
- Native seconds, quiet host: `unit.core.state.import` 42.6 s, 44.8 s with the new campaign counts (gate default, 31,026 checks), 49 to 51 s inside a full parallel CTest run; `unit.core.sched.matrix` 6 s; `unit.core.state.export` 3.5 s; `unit.core.digest.v25-reference` 0.5 s with the completeness sweep.
- ASan and UBSan, selection `unit.core.state.import`, `unit.core.state.export`, `unit.core.save.*`, `unit.core.si.*`, `unit.core.digest.*`, `unit.core.cpu.si-store-budget`, `unit.core.joybus.parser`, `unit.core.sched.matrix`, `unit.replay.suite`, `runner.state-export`, `runner.replay-mix` with `T64_STATE_MATRIX_FULL=1`: 15 of 15 passed, no sanitizer report (`build/asan/09-10-g1-asan-ctest.log`). **`unit.core.state.import` took 1,257.7 s under ASan with the full matrix** on a host at load 10 to 13 (the registered `TIMEOUT` is 1800 s); the others took 114 s (`unit.core.sched.matrix`), 119 s (`runner.state-export`), 88 s and 95 s (replay), 69 s (`unit.core.state.export`), 59 s (`unit.core.save.device`). Without the full-matrix variable, the registered (qualified) command took 1,197.9 s on the same host, 100 percent passed and no sanitizer report; the campaign case alone (120 iterations plus 60 on the triangle base blob, about 1.6 s per iteration, almost all of it the declared-digest recompute and the 2,000-tick advance of an accepted state) takes 297 s of it, so lowering `T64_STATE_FUZZ_ITERATIONS` in the sanitizer block of `core/CMakeLists.txt` (a data value) is the cheap knob if the native wrapper limit has to shrink (`build/asan/09-10-g1-asan-import-default.log`); the review's measurement before the carry was 631.8 s, on a quieter host (this host ran at load 10 to 13, so both figures are upper-biased and G3 re-measures on a quiet host). The carry adds 2,097,152 bytes to every digest (about +33 percent hashing) and the campaign now accepts about a third of its mutants and advances them, which is the cost group G3 has to put into the native wrapper derivation (VG-03).
- Old-profile regression (the scope limit of RDP-STATE): the Phase 9 source base cfe4d34 and the final G1 tree built with one build identity string; the profile hash, the admission status, the advance result and the canonical digest of the 17 pre-Phase-9 ids for `hello.z64`, `rdp-fill.z64` and `audio-pcm.z64` at ten checkpoints (0, 5, 1,000, 20,000, 200,000, 400,000, 538,000, 539,500, 541,000, 600,000): 510 checkpoint digests, zero differences (`build/gate/09-10-regress/cfe4d34-17ids.txt` and `g1-final-17ids.txt`); the same comparison with a different build identity string differs in 30 of 792 lines (control).
- Freeze check against `phase09-fh` (`build/gate/09-10-freeze-after.txt`): 17 files, 13 identical, 2 DATA, 2 LOGIC (FD-01 and FD-02); the baseline was 13, 3, 1. The change of `scripts/check-rsp-no-hle.py` from DATA to LOGIC is the one literal of DEV-03; the freeze table (outside the repository, owned by the orchestrator) does not list `REVIEWED_RDP_RASTER_FUNCTIONS` as a data name.

### Open after G1

- G2: L3 WR-01 to WR-03, IN-01, IN-02, IN-05, IN-06, VG-07, VG-10, R-01 to R-04, R-06, T-09-82.
- G3: L4 CR-01, WR-01, WR-02 (guard pin of `t64_save_sram_dma_apply` and the SI completion bodies, after G1 stopped editing them: they are unchanged by G1 except `t64_machine_save_import`, which is not pinned), IN-01, IN-03, VG-03, VG-05 (guard part), VG-08. Inputs for G3: the ASan figures above, the native 44.8 s, the gcc:13 closure time (22.5 s for 19 units), and the new data tuple rows of `scripts/tests/test_rsp_no_hle.py`.
- Orchestrator: the targeted re-check, the gaps-only re-verification, `09-SECURITY.md` and `09-VERIFICATION.md` updates, `09-VALIDATION.md` status, the claim wording of 09-11 (the resumed-run limit, the sampled partitions, the closed RDP limit).
- No new CTest name was added (45 of 45 used); the one new case `rdp-state-carry` runs inside `unit.core.state.import` and the new selftest case inside `harness.state-field-use-audit`.

## Verification notes (group G2)

Run on the final G2 tree (HEAD after the commit that adds this record). Host load average 8 to 11 during every measurement, so all seconds are upper-biased.

- Cheap checks: `sh tools/lint/core-lint.sh` violations 0; `sh scripts/pii-scan.sh --tree` and `sh scripts/rights-scan.sh --tree` findings 0; `sh scripts/check-claims.sh --files docs/claims/phase09.md` clean; `sh scripts/check-docs.sh` ok; `python3 -B scripts/check-state-field-use.py --check` findings 0; the guard (`scripts/check-rsp-no-hle.py --build`) exit 0 on `build/gate` and `build/asan`, its self-test exit 0; `sh scripts/check-test-inventory.sh --preset gate` PASS (224) and `--preset asan` PASS (226); `python3 -I -B build/gate/09-10-criteria-check.py` map ok (rows 4, named CTest 23, claim pairs 10).
- Full gate CTest on the final tree (after the follow-up of FX-13): 224 of 224 passed, 69.1 s real at `-j 4` (`build/gate/09-10-g2-final.log`, `.junit.xml`, `.LastTest.log`); the run before the follow-up also passed 224 of 224 in 83.4 s (`09-10-g2-full.log`); the heaviest names `unit.core.state.import` 59.6 s, `harness.state-field-use-audit` 19.3 s, `runner.cpu-diagnostic` 18.4 s.
- Pinned gcc:13 closure over the 11 changed C units (`runner/t64_artifact_publisher.c`, `t64_real.c`, `t64_supervisor.c`, `t64_replay.c` and the seven changed test sources): clean, 11.7 s (`build/gate/09-10-g2-gcc13.log`).
- Linux compile pre-flight of `docs/ci-reviews/LOG.md` F-102 without forcing a platform: the 219 compile commands of the gate build (the macOS-only `-arch` and `_DARWIN_C_SOURCE` removed, outputs discarded) rerun with the Ubuntu 24.04 `cc` (13.3.0) of the act image: 219 units, no diagnostic, 34 s (`build/gate/09-10-g2-linux-preflight.log`).
- ASan and UBSan for the touched runner units (`cmake --build --preset asan` then `ctest --preset asan -j 4` over `runner.artifact-publisher`, `controls.artifact-substitution`, `runner.state-export`, `runner.save-roundtrip`, `runner.replay-mix`, `unit.replay.suite`, `runner.supervisor-argv-bound`, `runner.phase09-tracer`, `runner.phase09-admit`, `runner.pad-poll`, `runner.save-fixtures`, `harness.fsize-artifact-real-fault`, `harness.fsize-report-capture-still-bounded`): 13 of 13 passed, no sanitizer report, 164.1 s real (`build/asan/09-10-g2-asan-ctest.log`). Per name under load 11: `runner.state-export` 145.2 s, `runner.replay-mix` 91.4 s, `runner.save-roundtrip` 91.3 s, `unit.replay.suite` 86.7 s, `runner.supervisor-argv-bound` 45.0 s in this batch and 27.4 s alone after the follow-up that lowered the matrix bound from 200,000 to 20,000 ticks (native 0.9 s; the 36 pair rows, nine baselines and 21 synthetic rows are the new cost, mostly process start under ASan; registered `TIMEOUT` 120 s), `harness.fsize-report-capture-still-bounded` 20.2 s, `runner.save-fixtures` 19.6 s, `runner.phase09-admit` 18.7 s, `runner.phase09-tracer` 17.8 s, `harness.fsize-artifact-real-fault` 13.7 s, `runner.pad-poll` 11.0 s, the two publisher names 0.12 s each. The state-import ASan matrix was not run (group G3 owns its timing).
- Native seconds, quiet moments of the same host: `runner.supervisor-argv-bound` 1.1 s, the publisher names 0.02 s each, `runner.state-export` 5.8 to 8.1 s, `runner.save-roundtrip` 4.0 to 5.7 s, `runner.replay-mix` 4.0 to 5.6 s, the batch `^(runner|controls)\.` (81 names) 22 to 26 s at `-j 4`.
- Freeze check against `phase09-fh` (`build/gate/09-10-freeze-after-g2.txt`): 17 files, 11 identical, 2 DATA (`scripts/rights-scan.sh`, `scripts/check-test-inventory.sh`), 4 LOGIC: `scripts/tests/test_rsp_no_hle.py` (FD-01), `scripts/check-rsp-no-hle.py` (FD-02) and the two new verdicts of this group, `runner/t64_artifact_publisher.c` and `runner/t64_artifact_publisher.h` (FD-03, DEV-04). No other frozen file differs; no frozen file other than the publisher pair was edited by G2 (`scripts/qualify-*.py`, `scripts/tests/test_phase09_qualification.py`, `ci.yml`, `check-ci-yml.sh`, `gate.sh`, the rights scanner and `check-rsp-no-hle.py` are untouched).
- Guard pin refresh: none needed (the guard pins no `runner/` body); no `fixtures/manifest.json` row changed, so `harness.rdp-oracle-authority` is untouched.
- Mutants (working-tree, built under the gate flags, restored): FX-10 one (three removals), FX-13 thirteen killed (ten supervisor, three runner), two equivalent runner mutants and one redundant check named in FX-13; drivers in `build/gate/09-10-regress/mutants_fx12.py` and `mutants_fx12b.py`.

### Open after G2

- G3: unchanged from the G1 list (L4 CR-01, WR-01, WR-02 pin, IN-01, IN-03, VG-03, VG-05 guard part, VG-08), with two more inputs: the ASan seconds above (`runner.supervisor-argv-bound` grew to 27 to 45 s under load against its registered `TIMEOUT` of 120 s; re-measure it if the qualifier wrapper sums runner names) and the new FD-03 (a frozen-file edit already recorded here).
- Orchestrator: `09-VALIDATION.md` frontmatter and status column (VG-11, still draft); the claim wording of 09-11 (the closed unknown-log record of a resumed runner-published state, FX-11; the stated replay checkpoint limit, FX-16; `T64_REAL_KEEP_WORKSET` as a Phase 8 carry, FX-18); the targeted re-check and the gaps-only re-verification now include the G2 files (runner publisher, supervisor, real runner, replay, the seven runner tests, `result-protocol.md`, `state-container.md`, `replay.md`, `save-media.md`).
- Phase 10: closed detail names for the recorder failures (L3 IN-01), the in-process replay driver duplicating the runner loop (L3 IN-02), the production `T64_REAL_KEEP_WORKSET` retention switch (L3 IN-06).

## Fixed issues, group G3 (FX-19 onward)

Group G3 (qualifier, scripts, guard, CI time). One commit per finding id, RED first (logs untracked under `build/gate/09-10-g3`), every frozen-file edit an FD entry below. No new CTest name (45 of 45 used), no case-list change (`docs/spec/phase09-cases.json` untouched: every new control runs inside an existing case of `scripts/tests/test_phase09_qualification.py` or inside `repo.rsp-no-hle`), no manifest row changed (the oracle authority files were not touched), no golden changed.

### FX-19 [FROZEN-FILE, FD-01 extended, FD-04] the SRAM DMA apply and the SI RD64B completion are pinned in the no-HLE guard: L4 WR-02, L1 WR-02 (guard part), VG-05 (guard part)

Commit 9a35431 (isolated pin refresh, N1). **RED** (`build/gate/09-10-g3/fx-19-red2.log`): with the four new seeds in `scripts/tests/test_rsp_no_hle.py` (delete the coverage invalidation call and the LL-link invalidation call of `t64_save_sram_dma_apply` in `core/t64_save.c` and of `t64_si_complete_event` in `core/t64_si.c`) and the guard unchanged, the first seed ended in `mutation accepted: SRAM DMA coverage invalidation call removed`: the guard stayed PASS on a tree whose SRAM to RDRAM transfer no longer invalidated the coverage sidecar, as the review's scratch copy showed. **Fix:** `scripts/check-rsp-no-hle.py` reads `core/t64_save.c` and `core/t64_si.c`, routes the two bodies through the body-digest table (new rows, no old value: both bodies were unpinned; the last change of `t64_save_sram_dma_apply` is 5ad6aaa (09-04) and of `t64_si_complete_event` b322147 (09-03), neither was touched by G1 or G2) and adds a token check per body that names a missing coverage invalidation or a missing LL-link invalidation (the pattern of the depth-commit check), so a blind digest refresh cannot hide it. Digests (SHA-256 of the whitespace-stripped, comment- and literal-blanked body): `core/t64_save.c :: t64_save_sram_dma_apply` 1e59b39c...3618, `core/t64_si.c :: t64_si_complete_event` c54482c6...6d80. Coverage of the other RDRAM writers (every `machine->ram` write site of `core/` mapped to a pinned body): `t64_bus_write`, `t64_bus_write_masked`, `t64_bus_writeback_cache_line`, `t64_pi_complete`, the SP functions (all pinned by name), the RDP fill, raster and depth bodies were already pinned; the two above were the only unpinned writers; `core/t64_state.c` replaces RDRAM and the coverage table together (FX-02) and has no body pin by the decision of 09-08 (roster row only). **Controls** (`scripts/tests/test_rsp_no_hle.py`, seeds 70 to 73, 73 seeds in all): each of the four deletions fails the guard through the body digest and, with the digest refreshed to the mutated body, through the token check with its exact message; the two new pins join the pin-deletion rows (`NEW_SAVE_BODY_PINS`), deleting either pin fails the guard with its own message. **A second finding of the same run (Rule 1, test defect since plan 07-01):** the seeds "RDP fill production body mutation" and "DPC counter bus-window body mutation" never restored the file they mutated, so every later seed was also rejected for the leftover finding (`core/t64_rdp.c :: apply_pending_command` from the 55 seeds after the first, `core/t64_bus.c :: decode_region` after the second) and a guard regression in any of them would have been masked. Measured at HEAD before the fix: each of the 55 later seeds also had a finding of its own, so no past pass was false; both seeds restore their file now, and a clean check after the last restoring seed fails on a leftover mutation (its own control: a deliberately leaked mutation is detected). **Green:** `build/gate/09-10-g3/fx-19-green3.log` (`PASS: repo.rsp-no-hle scanned=85 mutation_seeds=73`), the guard on `build/gate` and `build/asan` exit 0.

### FX-20 [FROZEN-FILE, FD-05, FD-06] the native stage limits come from measurements and are pinned: L4 CR-01, L4 IN-01, VG-03

Commit 54b08a7. **RED:** the shared wrapper killed the full ASan CTest at 600 s; the measured wall of the qualifier's exact command (`ctest --preset asan --no-tests=error`, `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`, `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, jobs 4 from the preset) on the fix tree is **1,380.7 s** (226 of 226 passed, no sanitizer report; `build/gate/09-10-g3/asan-full-ctest.log`, `asan-full-junit.xml`, `asan-full.LastTest.log`). The pinned relation of the new check fails on the limits before the fix (`build/gate/09-10-g3/fx-20-red.log`: "asan full CTest limit 600.0 does not exceed the largest registered TIMEOUT 1800"). Nothing was run for 600 s to watch the kill: the measured wall is the evidence and the wrapper's kill is the documented, deterministic fail-closed path (`_run_bounded`).

**Measurements** (host: 18 cores; two other projects' fuzzers and one system daemon kept about three cores busy, load average 7.6 to 14 over the runs, so every figure is upper-biased and "quiet" means no other work of this plan; during the last third of the ASan run I ran two native `unit.core.state.import` runs, seven native campaign runs and the Python controls, each on one core):

| Quantity | Measured |
| --- | --- |
| full ASan CTest, wall (-j 4) | 1,380.7 s |
| `unit.core.state.import` under ASan | 1,339.5 s (97 percent of the wall: the other 225 tests sum to 2,259.3 s of process time, 565 s if packed on four jobs; the earlier figures 1,197.9 s (G1) and 1,263 s (verifier, load above 20) are in the same range) |
| `unit.core.state.import` under ASan, alone (`ctest --preset asan -R '^unit\.core\.state\.import$'` with the registered environment, nothing else of this plan running, load 8 to 9 from the other projects; run started after FX-23) | **1,049.5 s** (`build/gate/09-10-g3/asan-import-quiet.log`, `asan-import-quiet-junit.xml`); the range over four runs is 1,049 to 1,339 s, the spread being the host and the four-job contention of the full run |
| sum of all 226 ASan test times | 3,598.8 s |
| other heavy ASan names | `runner.state-export` 141.4, `unit.core.sched.matrix` 120.1, `unit.replay.suite` 105.1, `runner.replay-mix` 102.3, `runner.save-roundtrip` 94.9, `unit.core.state.export` 77.9, `runner.cpu-diagnostic` 68.4, `integration.open-ipl3-determinism` 67.4, `unit.core.save.device` 64.3, `fuzz.rom-admission` 62.4, `runner.supervisor-argv-bound` 55.4 (all far below their registered TIMEOUT) |
| cold serial ASan build (`cmake --preset asan` into a fresh directory, `--parallel 1`, 342 steps) | 34 s (configure under 1 s) |
| gcc13 closure of the qualifier (64 units, 91,020 lines, `--platform linux/amd64`, the exact `docker run` of `_gcc13_closure_command`), load about 12 | 90.6 s against the 120 s kill while the per-case ASan profile ran, and 80.5 s again at load 12 to 13 after it (`gcc13-closure-run-quiet.txt`; the Phase 9 rehearsal measured 47.7 s for 51 units) |
| `unit.core.state.import` natively, sampled and with `T64_STATE_MATRIX_FULL=1` | 62.5 s and 73.8 s (loaded; 44.8 s quiet in G1) |

**Limits and derivation** (every limit is a kill deadline of one recorded command; a limit below the real cost of a stage fails every native qualification closed, so the rule is: at least twice the measurement, and the ASan wrapper at least 600 s above the largest registered TIMEOUT). FX-20 set the first values; FX-22 (below) raised the one registered TIMEOUT that had only a 1.34 times margin and moved the wrapper, the outer limit and the record bound with it. **Final values after FX-22:**

| Limit | Before the cycle | Final | Derivation |
| --- | --- | --- | --- |
| ASan full CTest (`NATIVE_ASAN_FULL_CTEST_TIMEOUT_SECONDS`) | 600 | **3,000** (FX-20: 2,400) | 2.2 times the 1,380.7 s wall, and 600 s above the largest registered TIMEOUT (now 2,400 s: `unit.core.state.import` under the sanitizer preset), so a per-test TIMEOUT, not the wrapper, is the first deadline of a hung test |
| gcc13 closure (`NATIVE_GCC13_TIMEOUT_SECONDS`) | 120 | **300** | 3.3 times the loaded 90.6 s and 3.7 times the 80.5 s (the 120 s kill was 1.3 to 1.5 times a measurement; the review projected 72 to 104 s) |
| outer limit of the shared stage (`NATIVE_SHARED_STAGE_TIMEOUT_SECONDS` of `scripts/qualify-phase09.py`) | 2,400 | **4,800** (FX-20: 4,200) | the sum of the five stage limits 600 + 120 + 600 + 3,000 + 300 = 4,620 plus 120 s of preflight (selftest and source snapshots), rounded up to 4,800; the old limits summed to 2,040 against the old 2,400 |
| record bound of the receipt verifier (`NATIVE_SHARED_RECORD_MAX_TIMEOUT_SECONDS`) | 2,400 | **3,000** (FX-20: 2,400) | the largest single stage limit (the verifier compares each recorded limit with it, not the sum) |
| gate (`sh scripts/gate.sh`), ASan configure, ASan build | 600, 120, 600 | unchanged | gate 187 s in the full gate of this cycle at load 13 (111 s of it CTest; 109 s in the last rehearsal): 3.2 times; configure under 1 s: more than 100 times; cold build 34 s: 17 times |
| full continuation matrix step (FX-21) | none | 300 | 4 times the loaded native 73.8 s, 3 times the verifier's 101.8 s |

**Phase 7 and Phase 8:** the five stage limits are the only shared constants changed (`scripts/qualify-rdp-vi.py` named them instead of the literals); the gate, configure and build values did not move. `scripts/tests/test_rdp_vi_qualification.py`, `scripts/tests/test_audio_qualification.py`, `qualify-rdp-vi.py --selftest` and `--deadline-selftest` and `qualify-audio.py --selftest` pass (`build/gate/09-10-g3/`, logs named `selftest-*` and `test_*_qualification.log`). Consequence recorded, not edited: `scripts/qualify-audio.py` keeps its own outer limit (2,400) and record bound (2,400); its record bound (2,400) is now below the shared ASan limit (3,000) and its outer limit (2,400) is below the shared stage's new worst case, so a new Phase 8 native qualification would need the same two literals raised. Phase 8 and Phase 7 are closed (their native records are recorded and verify with the recorded 600 s) and are not re-run by this plan.

**Controls** (`scripts/tests/test_phase09_qualification.py`, inside `native-command-plan-and-private-root`; no new case): the five stage limits, the outer limit, the record bound and the matrix step limit are pinned literals; the relations are checked (outer at least the stage sum plus 120 s, record bound at least the largest stage, the ASan limit at least 600 s above every `TIMEOUT` registered in a `CMakeLists.txt` (follow-up becb1cd made the margin part of the relation), parsed as text outside comments, with a check that every mention is a literal number so none escapes as a variable and that the maximum found is the known one (1,800 at FX-20, 2,400 after FX-22), the parse is not run through `ctest --show-only=json-v1` because that rewrites `LastTest.log` and the file's contract is no nested CTest); the check is shown to fail on each pin raised or lowered by one, on the pre-fix limits, on a later TIMEOUT above the wrapper, on an outer limit equal to the stage sum and on a record bound below the largest stage, each after the accepted baseline (the real limits) passed. **Fuzz iterations (`T64_STATE_FUZZ_ITERATIONS`) were evaluated and not lowered:** see FX-20b below.

### FX-20b [evaluation, no edit] lowering the sanitizer campaign count loses reader coverage

The brief allowed lowering `T64_STATE_FUZZ_ITERATIONS` in the sanitizer block only if the reached-rule counters still show every field-level reader reached. The campaign is deterministic (seed 0x5EED0911 and 0x7121A9E), so the counters per count are reproducible natively (`build/gate/09-10-g3/native-fuzz-*.log`, gate preset, the registered environment). Reader-level reasons (26 and above) reached, campaign on the stem state and campaign on the triangle render state (iterations are N and N/2):

| N | refusals / accepted (stem) | reader-level reasons reached on the stem blob | reader-level reasons on the triangle blob |
| --- | --- | --- | --- |
| 120 (registered) | 85 / 35 | 26 (15), 27 (1), 28 (2), 29 (10), 37 (1), 39 (2), 40 (5) | 26 (6), 28 (1), 29 (1), 39 (2), 40 (2) |
| 80 | 54 / 26 | 26, 27, 28, 29, 39, 40 (37 lost) | 26, 28, 39, 40 (29 lost) |
| 60 | 44 / 16 | 26, 27, 28, 29, 39, 40 (37 lost) | 26, 28, 39, 40 (29 lost) |
| 40 | 29 / 11 | 26, 27, 28, 29 (39, 40, 37 lost) | 26, 40 (28, 39, 29 lost) |
| 12 (gate default) | 9 / 3 | 26, 27, 28, 29 | 40 only (26 lost) |

Lowering to 60 would save about half of the campaign's 297 s under the sanitizer (about 150 s of the 1,340 s) and would lose `DEVICE_TOKEN_MISMATCH` (37) on the stem blob and `FIELD_DERIVED` (29) on the triangle blob from the reached set, so the claim "every reader reached by the registered campaign" would shrink. The saving is 11 percent of the test and does not change a limit decision, so the registered counts (120, sweep 12 and 12 and 2 blobs) stay; the earlier decision of plan 09-09.1 (the counts are a recorded cost decision) is not reopened. The remaining per-case ASan seconds are in the profile below.

### FX-21 [FROZEN-FILE, FD-05] the full continuation matrix is a recorded native step: L4 WR-01, VG-08

Commit a209c37. **Decision rule applied:** `unit.core.state.import` natively takes 62.5 s sampled and **73.8 s with `T64_STATE_MATRIX_FULL=1`** (31,026 and 31,979 checks; `build/gate/09-10-g3/native-import-default.junit.xml` and `native-import-full.junit.xml`; the verifier of this phase measured 101.8 s on a loaded host, VG-08), inside the 120 s bound, so the step is **added** and the claim for criterion 3 may name the full set, not "sampled partitions". **RED** (`build/gate/09-10-g3/fx-21-red.log`): the controls written for the step fail against the qualifier without it (the plan labels differ, the step controls find no step). **Fix:** `normal-state-import-full-matrix` is the last item of `_native_command_plan` (a second CTest run in `build/gate` rewrites `LastTest.log`, which the inventory checks of the earlier steps compare byte for byte with the full gate's original, so it cannot run before them): `ctest --test-dir build/gate --no-tests=error -R '^unit\.core\.state\.import$' --output-junit <private>/normal-state-import-full-matrix-junit.xml` with `T64_STATE_MATRIX_FULL=1` in the recorded environment and a 300 s limit (4 times the loaded native measurement, 3 times the verifier's). Creation and verification of a native record run the same marker check (`_verify_native_step`): the original command, environment, limit and outcome (`_verify_process_record`), the CTest pass line, a passed JUnit case and **more checks than the sampled run of the full gate in the same record** (the variable's effect is shown by the check total, not only by the recorded environment). **Controls** (inside `native-record-rejects-failed-truncated-or-stale` and `native-command-plan-and-private-root`; no new case): synthetic originals for every plan item are accepted, then an omitted step, a record whose environment lacks the variable, a failed CTest, a missing JUnit, a sampled run presented as the full one, a JUnit with a failure, with a skip and with two cases are each refused after the accepted baseline. **Claim wording for 09-11:** the CORE-04 / criterion 3 claim may say the continuation matrix of D-32 ran in full (one-shot and 1, 7, 31 and 4,096 tick partitions, all 16 rows, the instruction-count mid-tick row) in the native qualification of the exact SHA; it must still say that the gate, hosted and sanitizer runs sample (the 7 tick partition on twelve rows) and that the full set is not run on the hosted platforms or under the sanitizer. **Correction (G4, FX-34, R3 RC-01):** the matrix has seventeen rows, B01 to B17 (B17 was added by this cycle), four of them full (B01, B07, B10, B14), so the wording is "all 17 rows" and "the 7 tick partition on thirteen of the seventeen rows"; better, name the rows (`B01` to `B17`) than a count. `docs/spec/state-container.md` says so and the audit script compares the quoted counts with the tables. `docs/spec/state-container.md` now says so (the earlier sentence "the native sanitizer run is the full set" was wrong: the sanitizer registration never set the variable). The `core/CMakeLists.txt` sanitizer comment names the recorded step. **Follow-up c29e63b (found by running the step on real originals):** the full gate with `T64_GATE_JUNIT_OUT` under the private root, then the plan's exact command through `_run_saved_stage` (64.3 s, exit 0, JUnit 31,979 checks against 31,026 in the gate's JUnit), then `_verify_native_step`: the first marker version refused the real step because ctest 4.4.3 prints `100% tests passed out of 1`, not the CTest 3 text; the marker accepts both with the count of one, the control's accepted baseline is the real stdout (`build/gate/09-10-g3/fx-21b-real-ctest-stdout.txt`), and the real originals verify (`fx-21-endtoend.log`), while presenting the sampled JUnit as the full run is refused. Limit: the step is not exercised inside the whole native flow here (that is the Task 6 rehearsal and the qualifying run of 09-11); the plan, the marker on real originals, the verification and the controls are.

### FX-22 [FROZEN-FILE, FD-05, FD-06, DEV-10] the largest registered TIMEOUT gets margin and the limits follow: L4 CR-01, VG-03

Commit a3ceecb. **RED** (`build/gate/09-10-g3/fx-22-red.log`): `unit.core.state.import` under the sanitizer measured 1,339.5 s in the full run (1,198 s in G1, 1,263 s in the verifier's loaded run, 1,049.5 s alone), against its TIMEOUT of 1800 s: a margin of 1.34 times on the slowest, so a slower final run would fail the test by its own deadline. The data edit (TIMEOUT 2400) made the pinned relation fail first (the wrapper 2,400 did not exceed the new TIMEOUT 2,400), then the pins moved with the limits (final values in the table of FX-20). The registered value was read back with `ctest --show-only=json-v1` on the reconfigured asan tree (2400). **Per-case ASan seconds** of the import test (28 cases, each in its own process, four jobs, the registered environment, `build/gate/09-10-g3/case-profile-asan.tsv`; the sum is 1,676 s against 1,339.5 s in one process because the shared target preparation is paid once there): `fuzz-campaign` 292.9, `boundary-continuation` 157.2, `mutant-refusals` 152.4, `refusal-untouched` 148.7, `mutation-sweep` 128.9, `boundary-perturbation` 101.7, `field-range-refusals` 85.2, `boundary-roundtrip` 83.9, `boundary-event-rules` 72.0, `invariant-refusals` 69.3, `continuation-partitions` 54.0, the other 17 cases 0.1 to 37.3 each. No case is coverage-neutral to cut by an environment value (the one count in the block is the campaign, FX-20b).

### FX-23 [FROZEN-FILE-adjacent, bound source] the runner build identity covers the headers and the source `t64-runner-real` includes: L4 IN-03

Commit 846093b. **RED** (`build/gate/09-10-g3/fx-23-red-green.log`): a scratch configure of the gate preset with the list of HEAD gives the same `t64-runner-v1-...` before and after a header-only edit of `runner/t64_verified_input.h`; with the new list it changes (both the production and the test identity). The static control on the HEAD list finds exactly `t64_audio_observation.h`, `t64_input.c`, `t64_input.h`, `t64_real_result.h`, `t64_verified_input.h`, `t64_video_observation.h`. **Fix:** the six names join `T64_RUNNER_BUILD_INPUTS` in `runner/CMakeLists.txt`; the control (inside `source-paths-exist-and-cover-guards`) computes the runner-local include closure of the `t64-runner-real` sources and fails on a missing file, and shows it by removing three listed names in turn. The identity string changes once; no pinned literal holds it (the first-pair record holds the build identity of its own hosted run).

### Recorded, not fixed in G3 (INFO)

- **L4 IN-04** (no rehearsal marker in the public manifest): unchanged. The closed public key set is frozen (the pair verifier and the closed-key test pin it); the rehearsal stamp lives in both private records and on stdout, binding is by SHA. Owner: 09-11 claim and handoff wording (say that a manifest of a rehearsal SHA is distinguished by `workflow_sha`), Phase 10 if the key set opens.
- **L4 IN-05** (the committed first-pair record is coupled to authored rows): no fix. G1, G2 and G3 changed no profile hash, fixture hash or transcript hash, so `harness.phase09-collect-real` still accepts the committed pair; the family digests moved (FX-02) but the pair record holds only the rows of its table, which the G1 record lists as unchanged. Owner: 09-11 (an identity-affecting change after this cycle regenerates the first pair with a hosted run, or the test compares only the fields it needs).
- **L4 IN-02** (the case inventory proves the case list, not a CTest mapping): unchanged, owner Phase 10 checker follow-up; the full matrix step (FX-21) is a CTest of an inventory name, not a new case.
- **L4 IN-06** (the oracle request pin is re-bound on every manifest edit): owner decision, orchestrator. No manifest row was edited by G3, the oracle files were not touched, and `harness.rdp-oracle-authority` is unchanged.

### Final FD set after group G3

`build/gate/09-10-freeze-after-g3.txt` (freeze check against `phase09-fh`, table updated per DEV-11): 17 files, 9 identical, 2 DATA (`scripts/rights-scan.sh`, `scripts/check-test-inventory.sh`), **6 LOGIC, every one recorded**: `scripts/tests/test_rsp_no_hle.py` (FD-01, DEV-06), `runner/t64_artifact_publisher.c` and `runner/t64_artifact_publisher.h` (FD-03, DEV-04), `scripts/check-rsp-no-hle.py` (FD-04, DEV-05), `scripts/qualify-phase09.py` (FD-05, DEV-07), `scripts/qualify-rdp-vi.py` (FD-06, DEV-08). FD-02 (DEV-03) is DATA under the corrected table: the literal `REVIEWED_RDP_RASTER_FUNCTIONS` is a reviewed-function inventory and the table now classes every `REVIEWED_*_FUNCTIONS` name of the guard as data (the guard's `_check_source_route` is the only changed function, FD-04). FD-03 is two files, so six LOGIC verdicts have five FD ids. No other frozen file differs from the tag (`.github/workflows/ci.yml`, `scripts/check-ci-yml.sh`, `scripts/gate.sh`, `tools/rights/t64_blobscan.c`, `scripts/check-phase09-cases.py`, `scripts/qualify-audio.py`, `scripts/check-audio-cases.py`, `scripts/collect-boot-evidence.sh`, `scripts/build-boot-fixtures.sh` are identical). Every LOGIC file is in the bound source set, so the source digest of the qualification changes (a covered edit before F2) and logic changed after the only real hosted pair: the hosted rehearsal of Task 6 is the hosted evidence for all of them.

## Verification notes (group G3)

Run on the final G3 tree (HEAD after the commit that adds this record). Host: 18 cores, load average 8 to 13 from other projects' work for every measurement (see FX-20 for what ran alongside), so all seconds are upper-biased.

- Cheap checks: `ast.parse` of the five changed Python files and the freeze check; `sh scripts/pii-scan.sh --tree` findings 0 (1,226 files); `sh scripts/rights-scan.sh --tree` findings 0; `sh scripts/check-claims.sh --files docs/claims/phase09.md` clean; `sh tools/lint/core-lint.sh` violations 0 (98 files); `sh scripts/check-docs.sh` ok; `python3 -I -B build/gate/09-10-criteria-check.py` map ok (rows 4, named CTest 23, claim pairs 10); `python3 -B scripts/check-state-field-use.py --check` is untouched by G3 (no core source changed); `python3 -B build/gate/09-10-freeze-check.py` as above.
- Full normal gate (`sh scripts/gate.sh`, with `T64_GATE_JUNIT_OUT` under the private root): PASS, **187 s** total (sh-n 0, lint-selftest 6, fixture-qualification 1, configure 0, build 4, boot-evidence-selftest 19, test-inventory-selftest 33, test-inventory 1, CTest 111, core-lint 12); CTest 224 of 224 passed (`build/gate/09-10-g3/gate-full.log`, `gate-full-junit.xml`). After the last commit (c29e63b) the names `harness.*`, `repo.*`, `unit.core.state.import` and `runner.phase09-*`: 32 of 32 passed in 53 s (`final-harness.log`, `.junit.xml`, `.LastTest.log`).
- Pinned gcc:13 closure of the qualifier (64 units, 91,020 lines): clean, 90.6 s and 80.5 s at load 11 to 13 (`gcc13-closure-run.txt`, `gcc13-closure-run-quiet.txt`); no C unit changed in G3, so the closure over "changed units" is the qualifier's own closure.
- Linux compile pre-flight (`docs/ci-reviews/LOG.md` F-102, platform not forced: the act image `ghcr.io/catthehacker/ubuntu:act-24.04`, Ubuntu 24.04 `cc` 13.3.0, an amd64 image run under emulation on this arm64 host; the 219 compile commands of the gate build from `ninja -t commands` with the macOS-only `-arch`, `-D_DARWIN_C_SOURCE` and the dependency options removed, outputs discarded, the repository mounted read-only at `/src`): 219 units, exit 0, no diagnostic, 134.8 s (G1 126 s, G2 34 s at a quieter moment) (`linux-preflight.log`; the driver is a scratch script, `linux_preflight.py` in the session scratchpad, not committed).
- ASan and UBSan: the full ASan CTest of the fix tree before FX-22 to FX-23 (the code those commits touch is outside the sanitizer's reach: limits, a CMake input list and Python): 226 of 226 passed, no sanitizer report, 1,380.7 s (`asan-full-ctest.log`, `asan-full-junit.xml`, `asan-full.LastTest.log`); `unit.core.state.import` alone 1,049.5 s (`asan-import-quiet.log`); `cmake --build --preset asan` after FX-23 clean (the runner identity string changed); the registered TIMEOUT 2400 read back with `ctest --show-only=json-v1`.
- Guards: `python3 -B scripts/check-rsp-no-hle.py --build build/gate` and `--build build/asan` PASS (85 sources); `repo.rsp-no-hle` passes with 73 mutation seeds. Inventories: `sh scripts/check-test-inventory.sh --preset gate` PASS (224) and `--preset asan` PASS (226); no CTest name was added.
- Qualifier controls: `python3 -B scripts/qualify-phase09.py --selftest` exit 0 (8.5 s, every case); `scripts/tests/test_audio_qualification.py`, `scripts/tests/test_rdp_vi_qualification.py`, `scripts/tests/test_rdp_oracle_authority.py` exit 0; `scripts/qualify-rdp-vi.py --selftest`, `--deadline-selftest` and `scripts/qualify-audio.py --selftest` exit 0 (`selftest-*.log`).
- Native seconds: `unit.core.state.import` 62.5 s sampled and 73.8 s with `T64_STATE_MATRIX_FULL=1` in two ctest runs (64.3 s for the plan's step on real originals), the check totals 31,026 and 31,979.
- Not run, by the brief: the qualifying rehearsal (`--native`, `--collect-hosted`), the hosted rehearsal, a push.

### Open after G3

- **Task 6 (orchestrator):** the native rehearsal now costs about 30 minutes or more (ASan stage 17 to 23 minutes, gate 2 to 3, gcc13 1.5, the matrix step 1.2, the checkers) against 616 s at the Phase 9 rehearsal, inside the new limits (outer 4,800 s); the new step `normal-state-import-full-matrix` and the new limits are exercised in the whole native flow for the first time there. The hosted rehearsal is required for all six LOGIC verdicts and the runner identity change (FX-23).
- **Claim wording for 09-11** (all three come from this group): the continuation matrix claim may name the full set only for the native qualification of the exact SHA (FX-21), and must keep "sampled" for the gate, hosted and sanitizer runs; the criterion 3 claim must not quote raw check totals as strength (L5 IN-03); a manifest of a rehearsal SHA is distinguished only by `workflow_sha` (L4 IN-04).
- **Phase 8 qualifier:** `scripts/qualify-audio.py` keeps its own outer limit (2,400 s) and record bound (2,400 s), both below the shared stage's new worst case; a new Phase 8 native qualification would need them raised (not planned).
- **Margin to watch:** `unit.core.state.import` under the sanitizer ranges 1,049 to 1,339 s against its TIMEOUT of 2,400 s (1.8 times the slowest); the full ASan CTest wall is dominated by that one test (97 percent), so a faster ASan run needs that test split, which the 45 of 45 name bound forbids (L2 IN-05, Phase 10).
- **Orchestrator items carried unchanged from G1 and G2:** the targeted re-check, the gaps-only re-verification, `09-SECURITY.md` and `09-VERIFICATION.md` updates (VG-03, VG-05 guard part, VG-08 close with FX-19 to FX-23), `09-VALIDATION.md` status, the Task 5 verify commands that were not part of any group's scope.

## Fixed issues, group G4 (FX-24 to FX-37): the findings of the targeted re-check

Group G4 closes the findings of the targeted re-check (section below) that carry a fix. One commit per finding, RED first (logs untracked under `build/gate/09-10-g4`, the control run next to an accepted baseline of the same setup and compiled under the gate flags with `-Werror`), no new CTest name (45 of 45; the inventories print 224 for the gate preset and 226 for the sanitizer preset, as before), no manifest row changed (the oracle authority files were not touched), no golden changed, no frozen file edited (the FD set after G3 stands, see the verification notes). The re-check ids are `RC-NN` within a lane (R1, R2, R3).

### FX-24 [LOGIC-core] the v25 digest walker fails closed when the coverage table is missing: R1 RC-01

Commit 25fc5bd. **RED** (`build/gate/09-10-g4/rc1-01-red.log`): the control in `unit.core.digest.v25-reference` clears `rdp_memory_coverage` of each of the four family ids after an accepted baseline (the unedited machine digests, and the restored one digests the same); the guarded walker returned OK, a shorter stream that no longer covers the member plan 09-10 added, for all four ids (4 failures, got 0 want 4). **Fix:** `t64_digest_stream` returns `BAD_STORAGE` before the stream for a v25 machine without the table and the inner `!= NULL` skip is gone (the v20 and v21 blocks hash the table unguarded). **Green:** `unit.core.digest.v25` 2,340 checks, 0 failures; the 17 pre-Phase-9 ids: 792 regression lines byte identical to the G1 final tree (`build/gate/09-10-regress/g4-rc1-01-17ids.txt`, same method as FX-02); the digest, state export, save, scheduler and SI names (11) pass. Recorded as DEV-13.

### FX-25 [TEST, DOC] the mutant counts of the state container spec equal the inventory: R1 RC-02

Commit ed42269. **RED** (`rc1-02-red-check.log`): `scripts/check-state-field-use.py --check` with the new drift check reported two findings on the stale text (fields: the spec says 26, the inventory has 31; total: 60 against 65); the new selftest case failed its own baseline. **Fix:** the spec says 31 field rows and 65 rows in all; the audit compares the quoted per-group and total counts with the rows of `docs/spec/state-mutants.txt`; the selftest case (renamed `doc-count-drift` by FX-34) removes one inventory row from a copy and requires the finding next to the accepted baseline; registered in `docs/spec/phase09-cases.json` and the criteria draft, inside `harness.state-field-use-audit`. **Green:** findings 0, selftest passes, `check-phase09-cases.py --suite state-audit --run` and the criteria map pass.

### FX-26 [TEST] a state import saturates the save generation, a control kills the plain increment: R1 RC-03

Commit 37ec1b1. **RED** (`rc1-03-red-survival.log`): a working-tree mutant of the fixed library (`state_commit` ending in the pre-cycle plain increment) passed `save-media-roundtrip` (607 checks, 0 failures) as the review said it passed every core suite. **Fix (test only):** after the normal increments (1, 2, 3, the accepted baseline of the setup) the peer generation is set to `UINT64_MAX - 1`, the exported state is imported twice and the generation must be the maximum both times, then the counter is set back for the refusal rows below. **Green:** 616 checks, 0 failures; the mutant now fails one check (the second import ends at 0, `rc1-03-red-mutant-killed.log`). A first version of the row left the maximum in place and broke the later rows that assert a refused import never changes the generation (5 failures), fixed at once.

### FX-27 [DOC, TEST] the recorded state digests of the hosted pair are valid for 2db1be4d only: R1 RC-04

Commit d0ac32e. The committed record is not edited. **RED** (`rc1-04-red.log`): the evidence text had no scope statement (count 0); no test compares the recorded digests with a current build (the pair tests compare the authored rows without `guest_tick` and `state_digest`). **Fix:** `docs/spec/si-pif-controller.md` (the "Hosted rehearsal" section) states that the digests belong to the digest function of 2db1be4d, that plan 09-10 widened the family stream, that they are superseded by the 09-11 pair and that no test compares them with current output; the existing pair case asserts the statement and that the compared rows carry neither field; the golden record above carries the correction. **Green:** `harness.phase09-collect-real` passes.

### FX-28 [TEST] the reverse-endian arm of the D-44 budget is pinned: R1 RC-05

Commit 17a99de. **RED:** the review's mutant (the reverse-endian branch of `cpu_successor_extra_event_budget` deleted) survived `test_si` (and the guard) unchanged. **Fix (test and spec text):** `cpu-store-budget` gains a user-mode machine with one TLB pair (even page: the SI registers, uncached; odd page: the program page) and refuse-then-accept rows: three without the reverse-endian mode (the accepted baseline of the mapping: RD64B, WR64B, DRAM_ADDR by their plain offsets) and two with Status.RE and Config.BE (byte 0 of the page reaches RD64B, two events; byte 4 reaches DRAM_ADDR, one event); WR64B shares the legacy budget of one event, so the lane exchange has no discriminating row in that direction (stated in the test and in `docs/spec/si-pif-controller.md`). **Green:** `unit.core.cpu.si-store-budget` 723 checks (756 with the first version that had the WR64B row, dropped as non-discriminating), `unit.core.si` 534,447 checks, 0 failures; the mutant fails six checks (an SI DMA started with a budget of one event: status 11 at room 2, retired 3 against 4, queue 16 against 14, SI state and digest changed; `rc1-05-mutant-killed-final.log`). The FX-04 limit is corrected above.

### FX-29 [LOGIC-runner, DOC, TEST] the supervisor proves its result and capture paths before the guest runs, the contract after the commit is stated and pinned: R2 RC-01

Commits 7d0ca21 and fa43a09 (a follow-up that moves the run-meta write failure out of the failure-record bullet of the contract: the record stays the runner's `pass`, exit status 1). **RED** (`rc2-01-red.log`, `runner.state-export` case `export-fsize-at-state-size`): with `--capture-out` inside a directory that does not exist the guest ran, the runner published the leaf and the record was `host-error` (not the usage record at tick 0; the artifact directory held one entry, 3 failing checks plus the listing); with `--result` inside one the guest ran and the leaf was published with no record. **Fix:** real mode probes both paths with the temporary name and the rename target of `atomic_write` (nothing is left behind) before the guest runs (a capture path: usage refusal at tick 0; a result path: exit status 1, no record can be written); `docs/spec/save-media.md` states the contract by outcome (what publishes nothing, which supervisor failures after the commit leave the complete private leaf next to a closed failure record) and the runner carries one comment. **Controls** (inside the existing case, next to the accepted baseline that is the passing run of the same spec): the two unusable paths end at tick 0 with an empty artifact directory; each reachable trigger of the test twin leaves a leaf of the exact state size (mode 0600, one link) and exit status 1 with the closed record: `exit-nonzero` (`host-error` / `runner-exit-nonzero`), `crash-after-output` (`runner-signalled`), `report-missing` (`report-error` / `no-result`), `report-malformed` (`report-error` / `report-invalid`); the first draft of the first row expected the wrong stop kind and was corrected from the observed record. `wall-backstop` and `supervisor-signalled` are timing outcomes without a control. **Green:** `runner.state-export` passes natively (6.3 s) and under the sanitizer (131.5 s against the registered TIMEOUT of 240 s), the runner, controls and harness batch (102 names) passes. Recorded as DEV-12.

### FX-30 [DOC, TEST] the closed detail vocabulary names every host detail the checker accepts: R2 RC-02

Commit 1fe9191. **RED** (`rc2-02-red.log`): the drift control derived from `runner/t64_result_check.c` finds seven names of the host-detail table that the spec never names: `resource-yield-limit`, `resource-yield-no-progress`, `stop-reason-unexpected` and the four observation details (the review named the first three; the observation names were equally absent). **Fix:** the sentence lists the eleven host details exactly with a gloss from the emitting code; `runner.fixture-transcript-pin` parses the host-detail, artifact publisher and state-export tables of the checker and requires every name in the spec in backticks (at least 40 names, so a parse that finds none cannot pass). The raw capture names (54) are specified with the raw capture and are not part of this control. **Green:** the case passes.

### FX-31 [TEST] the state runner and save round trip tests remove their run directories: R2 RC-03

Commit 3c5035e. **RED** (`rc2-03-red.log`): one clean green run of each into a fresh evidence directory left 86 and 54 entries. **Fix:** both open the per-process evidence scope of FX-12. **Green:** 0 and 0 (`rc2-03-green.log`).

### FX-32 [DOC] the child-only names sentence: R2 RC-05

Commit ef6a48c. `docs/spec/state-container.md`: the declared size and the SHA-256 are verified by the supervisor only; a directly started child checks the shape of the descriptor and, for a save image, the exact kind size (the `SAVE_FD_SHORT` row of `runner.save-roundtrip`).

### FX-33 [TEST] the option-pair matrix test asserts its rows against the spec: R2 RC-07

Commit 284012d. A `_Static_assert` on the row count (nine families, 36 pairs), a run-time check that each unordered pair appears once with the lower family first, and a parse of the 9 by 9 table of `state-container.md` compared with the rows in both triangles; the comment names the three import families. The control fails on a flipped cell of the spec table (`rc2-07-red-spec-mutant.log`) and on a dropped row (compile error, `rc2-07-red-dropped-row.log`); `runner.supervisor-argv-bound` passes.

### FX-34 [DOC, TEST] the sampling sentence of the boundary matrix: R3 RC-01

Commit 0c55ca9. **RED** (`rc3-01-red-check.log`): with the full rows named in the sentence and the counts left as written, the audit reported "the spec says twelve of sixteen sampled, the tables have 17 rows and 4 full". **Fix:** "thirteen of the seventeen rows, every partition size on the four full rows (B01, B07, B10 and B14)"; `check-state-field-use.py` compares the matrix size sentence and the sampling sentence with the rows of the spec table and the rows marked full in the test table; the selftest case is renamed `doc-count-drift` and seeds both (DEV-14); a stale comment of the boundary table is corrected. The claim wording correction for 09-11 is in FX-21 above. **Green:** findings 0, selftest 7 of 7.

### FX-35 [DOC] the completeness sweep wording: R3 RC-06

Commit 59620c8. `docs/spec/machine-contract.md`: every member on the stem id (a member with a named exclusion must leave the digest unchanged) and, on the three save ids, a representative set (index a multiple of 11 and no named exclusion).

### FX-36 [TEST] controls for the remaining conditions of the full matrix marker: R3 RC-03

Commit 4937d7f. **RED** (`rc3-03-mutants.log` is the same run against the new controls; before them the review showed all five survive): the five conditions without a control (private root, symlink, status run, the `failures=0` anchor, exactly one total) each removed or relaxed in `scripts/qualify-phase09.py`. **Fix (test only):** inside `native-record-rejects-failed-truncated-or-stale`, after the accepted baseline of the same setup, a case that did not run, a check total with failures, two totals, a valid original outside the private root and a symlink to a valid original inside it are each refused; the dangling assertion on the path helper is replaced. **Green:** `test_phase09_qualification.py --selftest` passes (29 tests); the five mutants are all killed and the qualifier is byte identical afterwards.

### FX-37 [TEST] the registered TIMEOUT control reads the tracked files: R3 RC-05 (first part)

Commit 9271189. **RED** (`rc3-05-red.log`): an untracked copy of the repository under the working directory holding one `CMakeLists.txt` with `TIMEOUT 9999` moved the maximum of the control to 9999 and failed `native-command-plan-and-private-root`. **Fix:** the file list is `git ls-files -- '*CMakeLists.txt'` (at least 8 files). **Green:** the same copy leaves the control green. The other limits of RC-05 (`*.cmake` files, a `#` inside a quoted string, upper relations) are recorded below.

### Recorded, not fixed in G4

- **R1 RC-06** (INFO): the v25 container and digest carry no marker that the state content changed in plan 09-10. Decision recorded earlier (F1 deviation DEV-02, provisional state version): the internal pre-release state and container versions are intentionally unchanged, an older blob is refused by the build identity or by the length rule, nothing imports one, and the digest and serialization versions are decided at the Phase 10 freeze. Owner Phase 10.
- **R1 RC-07** (INFO, necessity): the NULL test in `t64_save_generation_advance` follows the module convention (guard NULL at every entry); the helper is internal and its two callers have dereferenced the machine. Kept; owner Phase 10 cleanup.
- **R2 RC-04** (INFO): `test_replay` still links `t64_artifact_publisher.c` after FX-17 (a dead link input, no behavior). Not changed here to keep runner build edits out of this cycle; owner Phase 10 refactor with the in-process replay driver.
- **R2 RC-06** (INFO): the "never touched" and "publishes nothing" wording is stronger than the mechanism (a same-euid swap between the identity check and the unlink, the visibility window between the rename and the withdrawal, a kill in that window, and the placement of the pre-rename directory sync behind the identity check). FX-29 states the kill window for the two timing outcomes; the publisher header and spec text and the reordering would be a further edit of the frozen publisher pair (FD-03, a hosted rehearsal cost) for a same-euid-only effect. Owner Phase 10.
- **R2 RC-08** (INFO): a failing runner test erases its own evidence at exit (the scope removes the tree on every normal exit; `T64_KEEP_TEST_EVIDENCE` keeps it). CI does not upload these directories. Owner Phase 10 (a failure flag in the helper).
- **R3 RC-02** (WARNING, recorded as a known limitation, owner: the next Phase 8 requalification): `scripts/qualify-audio.py` keeps its own outer limit (2,400 s) and record bound (2,400 s) while the shared native stage now records a 3,000 s ASan limit (outer 4,800 s), so a new Phase 8 native qualification on this tree would be refused closed by its own verifier (a refusal, not a false pass). The Phase 8 qualifier and `docs/evidence/phase08-audio.md` are left untouched on purpose (a frozen file and closed evidence; the existing Phase 8 native record verifies with its recorded 600 s). When Phase 8 is requalified the two literals are raised to the shared values (4,800 and 3,000) with an FD entry, or the Phase 8 qualifier gets a pinned relation like the Phase 9 test (`audio outer >= shared stage sum + 120`, `record bound >= largest shared stage`). The note is also in `docs/ci-reviews/LOG.md` (F-106).
- **R3 RC-04** (INFO): the matrix JUnit of the new step is the only proof that the variable took effect and `native-record.json` does not hash it (the whole `steps/` tree has the same property: step records are self-consistent originals, not sealed). The claim for criterion 3 states that the step records are unsealed originals, or 09-11 records the hash. Owner 09-11 claim wording.
- **R3 RC-05** (INFO, remaining parts): `*.cmake` files are not read (the only one holds no registration), a `#` inside a quoted string would cut a line, and the two relations bound the limits from below only (upward drift is stopped by the literal pins). Optional hardening for the Task 6 rehearsal: a scratch-configure `ctest --show-only=json-v1` cross-check.
- **R3 RC-07** (INFO): the freeze check classes two timeout constants as data because `_SHA` matches `_SHARED` in its pin pattern (the file is LOGIC through FD-05 anyway). The script is untracked and owned by the orchestrator: anchor the pattern (`(^|_)SHA256$|_DIGEST$|_HASH$`) or list `NATIVE_SHARED_STAGE_TIMEOUT_SECONDS` and `NATIVE_SHARED_RECORD_MAX_TIMEOUT_SECONDS` in `NEVER_DATA`.
- The non-blocking findings F-1 to F-8 of the security re-audit are carried as the auditor recorded them (see the section below).

## Targeted re-check (plan 09-10 Task 5 step 3)

The deep review ran once more over the files the cycle changed and their direct callers, in three lanes, and `gsd-secure-phase` ran once more for the previously open threats. The reports were produced by the reviewers and are not edited here: `build/gate/09-10-recheck-R1.md` (core, state container, importer; tree 7de85af), `build/gate/09-10-recheck-R2.md` (runner, publisher, runner-facing specs) and `build/gate/09-10-recheck-R3.md` (qualifier, guard, pins, claims, records), and the security re-audit `build/gate/09-10-security-recheck.md`. The tracked `09-REVIEW.md` is the original review (unmodified), and `09-SECURITY.md` now carries the re-audit with the initial audit appended verbatim.

| Lane | Scope | Critical | Warning | Info | Total |
| --- | --- | --- | --- | --- | --- |
| R1 | core, state container, importer, direct callers | 0 | 1 | 6 | 7 |
| R2 | runner, publisher, supervisor, runner-facing specs | 0 | 2 | 6 | 8 |
| R3 | qualifier, guard, pins, success criteria, this record | 0 | 2 | 5 | 7 |
| Total | | 0 | 5 | 17 | 22 |

Security re-audit: verdict **SECURED** at ASVS 1 with `block_on` high; 86 of 86 threats closed, `threats_open` 0; the four previously open threats T-09-67, T-09-73, T-09-82 and T-09-87 are closed with code evidence and reproduced exploit scenarios; three new threats found in the pass, T-09-88 (the carried RDP section as hostile input), T-09-89 (deleting a leaf on the failure path) and T-09-90 (raised qualifier time limits hiding hangs), are closed; 8 non-blocking findings (F-1 to F-8). The pending register of plan 09-11 (T-09-83 to T-09-86) is not counted.

No new blocker (CR level) in any lane: no stop and no second cycle (the plan's rule). The verdicts of the lanes: the cycle did not fail; every claimed fix of FX-01 to FX-23 closes its finding when the original scenario is replayed.

| Id | Sev | Disposition | Commit and entry |
| --- | --- | --- | --- |
| R1 RC-01 | WARNING | FIXED | 25fc5bd, FX-24 |
| R1 RC-02 | INFO | FIXED | ed42269, FX-25 |
| R1 RC-03 | INFO | FIXED | 37ec1b1, FX-26 |
| R1 RC-04 | INFO | FIXED (text and control; the retained record is unedited) | d0ac32e, FX-27 |
| R1 RC-05 | INFO | FIXED (reachable, controls added) | 17a99de, FX-28 |
| R1 RC-06 | INFO | RECORDED, owner Phase 10 | recorded above |
| R1 RC-07 | INFO | RECORDED, convention kept | recorded above |
| R2 RC-01 | WARNING | FIXED (probes before the guest, contract stated, each reachable trigger pinned) | 7d0ca21 and fa43a09, FX-29, DEV-12 |
| R2 RC-02 | WARNING | FIXED (seven names, drift control) | 1fe9191, FX-30 |
| R2 RC-03 | INFO | FIXED | 3c5035e, FX-31 |
| R2 RC-04 | INFO | RECORDED, owner Phase 10 | recorded above |
| R2 RC-05 | INFO | FIXED | ef6a48c, FX-32 |
| R2 RC-06 | INFO | RECORDED (kill window stated by FX-29), owner Phase 10 | recorded above |
| R2 RC-07 | INFO | FIXED | 284012d, FX-33 |
| R2 RC-08 | INFO | RECORDED, owner Phase 10 | recorded above |
| R3 RC-01 | WARNING | FIXED (text, claim wording correction, doc control) | 0c55ca9, FX-34 |
| R3 RC-02 | WARNING | RECORDED known limitation, owner next Phase 8 requalification (this record, `docs/ci-reviews/LOG.md` F-106); `scripts/qualify-audio.py` and `docs/evidence/phase08-audio.md` untouched | recorded above |
| R3 RC-03 | INFO | FIXED | 4937d7f, FX-36 |
| R3 RC-04 | INFO | RECORDED, owner 09-11 claim wording | recorded above |
| R3 RC-05 | INFO | FIXED in part (`git ls-files`), rest RECORDED | 9271189, FX-37 |
| R3 RC-06 | INFO | FIXED | 59620c8, FX-35 |
| R3 RC-07 | INFO | RECORDED, orchestrator-owned script | recorded above |
| Security F-1 to F-8 | low, info | RECORDED as the auditor listed them (non-blocking, not counted in `threats_open`) | `09-SECURITY.md` |

Counts: 14 findings fixed (R1: 5, R2: 5, R3: 4, one of them in part), 8 recorded with an owner. The two WARNING items that carry a recorded limitation instead of a code change are R3 RC-02 (a Phase 8 qualifier limit with an owner and a note in the CI review log) and the stated-contract half of R2 RC-01 (the two timing outcomes have no control).

## Verification notes (group G4)

Run on the final G4 tree (HEAD after the commit that adds this record). Host: 18 cores, load average 10 to 11 from other projects' work for every measurement, so all seconds are upper-biased. Logs are untracked under `build/gate/09-10-g4` (the regression text under `build/gate/09-10-regress`).

- Cheap checks: `sh tools/lint/core-lint.sh` violations 0 (98 files, 10.7 s); `sh scripts/pii-scan.sh --tree` findings 0 (1,226 files) and `sh scripts/rights-scan.sh --tree` findings 0, rerun after the last docs edit; `sh scripts/check-claims.sh --files docs/claims/phase09.md` clean; `sh scripts/check-docs.sh` ok; `python3 -B scripts/check-state-field-use.py --check` findings 0 and `--selftest` seven of seven; `python3 -B scripts/qualify-phase09.py --selftest` exit 0 (6.7 s); `python3 -B scripts/tests/test_phase09_qualification.py --selftest` OK, 29 tests (6.2 s); `python3 -I -B build/gate/09-10-criteria-check.py` map ok (rows 4, named CTest 23, claim pairs 10, 15 binaries); `check-phase09-cases.py --suite state-audit --run` passes with the renamed case; `__pycache__` directories removed.
- Guards and inventories: `python3 -B scripts/check-rsp-no-hle.py --build build/gate` and `--build build/asan` exit 0 (85 sources); `sh scripts/check-test-inventory.sh --preset gate` PASS (224) and `--preset asan` PASS (226); no CTest name was added or renamed.
- Full normal gate (`sh scripts/gate.sh`): **PASS, 128 s** total (sh-n 0, lint-selftest 4, fixture-qualification 1, configure 0, build 0, boot-evidence-selftest 12, test-inventory-selftest 19, test-inventory 1, CTest 81, core-lint 10); CTest 224 of 224 passed (`gate-full.log`, `gate-full.LastTest.log`). Per name natively in that run: `unit.core.state.import` 46.6 s, `harness.state-field-use-audit` 17.7 s, `runner.state-export` 7.0 s, `unit.core.si.device` 1.8 s, `unit.core.cpu.si-store-budget` 1.7 s. The always-run set was part of this run (all of `unit.core.state.*`, `unit.core.save.*`, `unit.core.sched.*`, `unit.core.digest.*`, `unit.core.profile.*`, `runner.*`, `controls.*`, `harness.*`, `repo.*`); two earlier focused batches during the group passed as well: the digest, state export, save, scheduler and SI names (11 of 11, 6.0 s) and the runner, controls and harness names (102 of 102, 27.1 s).
- ASan and UBSan (`cmake --build --preset asan`, then `T64_STATE_MATRIX_FULL=1 ctest --preset asan -j 4` over `unit.core.digest.*`, `unit.core.state.export`, `unit.core.si.*`, `unit.core.cpu.si-store-budget`, `unit.core.save.*`, `runner.state-export`, `runner.save-roundtrip`, `runner.supervisor-argv-bound`, `runner.fixture-transcript-pin`): **13 of 13 passed, no sanitizer report, 153.6 s wall** (156 s with the incremental build; `asan-ctest.log`, `asan.LastTest.log`). Per name: `runner.state-export` 153.6 s (131.5 s when run alone: the new FX-29 rows add four supervised runs; registered TIMEOUT 240 s, so the margin is 1.6 times the loaded figure), `runner.save-roundtrip` 87.0, `unit.core.state.export` 67.0, `unit.core.save.device` 53.3, `runner.supervisor-argv-bound` 44.1, `unit.core.save.import-export` 40.1, `unit.core.cpu.si-store-budget` 33.7, `unit.core.si.device` 31.6, `unit.core.digest.v25-reference` 7.2, `runner.fixture-transcript-pin` 7.7. The state import ASan run was not repeated (measured 1,049 to 1,339 s in G3, by the brief); the import test ran natively inside the gate (46.6 s, 616 checks in `save-media-roundtrip`).
- Old-profile regression after FX-24: 792 lines (17 ids, three authored ROMs, ten checkpoints), byte identical to the G1 final tree, 9.5 s.
- Pinned gcc:13 closure over the 9 changed C units (`core/t64_digest.c`, `core/tests/test_digest_v25.c`, `test_si.c`, `test_state_import.c`, `runner/t64_real.c`, `t64_supervisor.c`, `tests/test_phase09_runner.c`, `test_save_roundtrip.c`, `test_state_runner.c`), the exact command of `_gcc13_closure_command` and the pinned image: clean, 12.8 s (`gcc13-closure.log`).
- Linux compile pre-flight (`docs/ci-reviews/LOG.md` F-102, platform not forced: the act image, Ubuntu 24.04 `cc` 13.3.0, an amd64 image run under emulation on this arm64 host; the 219 compile commands of the gate build from `ninja -t commands` with the macOS-only `-arch`, `-isysroot`, `-D_DARWIN_C_SOURCE` and dependency options removed, outputs discarded, four parallel jobs, the repository mounted read-only): 219 units, exit 0, no diagnostic, 44 s (`linux-preflight.log`; the driver is a scratch script outside the repository).
- Freeze check against `phase09-fh` (`freeze-after-g4.txt`): 17 files, 9 identical, 2 DATA, **6 LOGIC, no new FD entry**: the same set as after G3 (FD-01 `scripts/tests/test_rsp_no_hle.py`, FD-03 the two publisher files, FD-04 `scripts/check-rsp-no-hle.py`, FD-05 `scripts/qualify-phase09.py`, FD-06 `scripts/qualify-rdp-vi.py`); `git diff` of those six files and of `scripts/qualify-audio.py`, the workflow, `scripts/gate.sh`, `scripts/check-ci-yml.sh` and `fixtures/manifest.json` between the G3 head and now is empty. G4 edited `scripts/tests/test_phase09_qualification.py` (the test of the FD-05 file, not on the freeze table), `scripts/check-state-field-use.py` and the tracked sources and specs listed per FX entry. Every edited source above is in the bound source set, so the source digest of the qualification changes again (a covered edit before F2) and the hosted rehearsal of Task 6 stays the hosted evidence.
- Mutant evidence of the group (working-tree mutants of the fixed code or of the qualifier, each restored and byte compared): the plain generation increment (FX-26: survived before, fails one check), the deleted reverse-endian branch (FX-28: fails six checks), five conditions of the matrix marker (FX-36: five survived before, five killed), the dropped spec table row and flipped table cell (FX-33), the untracked copy of the repository (FX-37). No fixture, manifest row, golden or oracle file changed (`fixtures/manifest.json` is identical, so `harness.rdp-oracle-authority` is unaffected and passes in the gate).
- Not run, by the brief: the state import ASan run, the qualifying rehearsal (`--native`, `--collect-hosted`), the hosted rehearsal, a push, the gaps-only re-verification.

### Open after G4

- **Orchestrator:** the gaps-only re-verification (Task 5 step 3b) over the final tree and the recomputed `covered_digest` (the covered set now includes the G4 files; the tracked `09-SECURITY.md` was replaced by the re-audit with the initial audit appended, as the plan's step 3 asks); `09-VERIFICATION.md` status; `09-VALIDATION.md` was **not** changed (frontmatter `status: draft`, every row `pending`: its rows resolve only against the final re-verification and the sign-off needs the exact-SHA evidence, so VG-11 stays with the orchestrator, as recorded after G2); the freeze table of `build/gate/09-10-freeze-check.py` (R3 RC-07, anchor the `_SHA` pin pattern).
- **Task 6:** the native rehearsal and the hosted rehearsal are required for all six LOGIC verdicts, the runner identity change of FX-23 and the G4 edits of `core/t64_digest.c`, `runner/t64_supervisor.c` and `runner/t64_real.c`; the first native run with the new matrix step is still the Task 6 rehearsal. The hosted pair record of 2db1be4d stays retained unedited; its state digests are superseded by the 09-11 pair (FX-27).
- **Claim wording for 09-11** (added by G4): name the matrix rows (`B01` to `B17`) or say seventeen, four full, thirteen sampled (FX-34); say which supervisor failures leave a published leaf next to a failure record and that exit status 0 plus a `pass` record are what a caller accepts (FX-29); the recorded state digests of the hosted pair are valid for 2db1be4d only (FX-27); the step records of the native qualification are unsealed originals (R3 RC-04).
- **Margin to watch:** `runner.state-export` under the sanitizer measures 131.5 s alone and 153.6 s loaded against its TIMEOUT of 240 s.
- **Next Phase 8 requalification:** the `qualify-audio.py` limits (R3 RC-02, `docs/ci-reviews/LOG.md` F-106).
- **Phase 10:** R1 RC-06 (version markers of the state content), R1 RC-07, R2 RC-04, RC-06, RC-08, the remaining parts of R3 RC-05, and the items carried from G1 to G3.

## Task 6 repairs (FX-38, FX-39)

### FX-38 [TEST] a runner test that retains run evidence for the hosted collector keeps its scope past exit: regression of FX-12, DEV-15

Commit c3bdff5. **RED** (`build/gate/09-10-g5/rc6-01-red.log`; the public hosted log of run 44776046324 holds the same refusal on both jobs): replayed locally on the clean tree `0d31e0e` with the exact CI lines (`check-phase09-cases.py --suite hosted --run` exit 0, then `qualify-phase09.py --emit-hosted ...`): `refused original evidence: retained run directory is missing or unsafe`, exit 1, about 2 s. Note on the log: it is a transcription of the live terminal output of that replay, not a tool capture (the binary of that moment was replaced by the fix build before the log could be re-run, and the emit refuses a dirty tree); the case-step capture is retained as a scratch file. **Fix (test only):** `t64_test_scope_keep()` in `runner/tests/t64_test_tree.h` makes the scope cleanup return (the flag sits beside the existing `T64_KEEP_TEST_EVIDENCE` condition); `runner/tests/test_pad_poll.c` calls it after `retain_run_evidence` succeeds, before the marker line, so a pad-poll process that retained a run leaves its whole scope in place (the pre-FX-12 behavior for exactly the processes the collector reads; the other six tests still clean up). **Control** (inside `harness.phase09-qualification`, case `hosted-rows-pinned-independently`, no new name): `retained_scope_gaps` finds, after the accepted baseline of the real sources (exactly one retaining test source, `test_pad_poll.c`), the keep call missing between the retain call and the marker, the cleanup not honoring the flag and the helper missing, each removed in turn. This is a static control: the end-to-end proof is the replay of every hosted step after Gate. **Green:** the case step and the Phase 9 emit at `c3bdff5` exit 0, and the whole tail replayed in order with the CI lines (audio case run 17 s, Phase 9 case run 2 s, audio emit, Phase 9 emit, raw RDP VI emit, guest emit, staging rights scan 8 files 0 findings, four PII scans) all exit 0 after a full `sh scripts/gate.sh` (PASS, 140 s); `test_phase09_qualification.py --selftest` passes (29 cases); the pinned gcc:13 closure over the seven units that include the header is clean; the Linux compile pre-flight (219 units, act image, platform not forced) is clean, 50 s.

### FX-39 [DATA, Task 6 repair, repeat 1 of 1] four registered TIMEOUTs get the margin the measurements need: DEV-16

Commit ba477e2, `runner/CMakeLists.txt` only (four numbers). **Trigger and classification:** DEV-16. **Margin audit** (every registered TIMEOUT of both presets, read with `ctest --show-only=json-v1`, against the largest of: native gate JUnit and native sanitizer JUnit of the rehearsal on `16849e7`, hosted Linux and macOS seconds of runs 44776046324 and 44784312630; the rule is the one of FX-20 and FX-22, at least twice the measurement):

| Name | Registered | Largest measured | Ratio | Basis | Now |
| --- | --- | --- | --- | --- | --- |
| `harness.state-field-use-audit` | 60 | 60.1 (killed) | 1.0 | hosted macOS, run 44784312630 (48.7 s in run 44776046324, native 17.7 to 21.3 s) | 240 |
| `repo.rsp-no-hle` | 60 | 47.9 | 1.25 | hosted Linux, run 44776046324 (73 mutation seeds since FX-19) | 240 |
| `runner.state-export` | 240 | 182.4 | 1.32 | native sanitizer full run (153.6 s in G4, 131.5 s alone) | 480 |
| `runner.cpu-diagnostic` | 120 | 62.8 | 1.91 | native sanitizer full run | 240 |

Left unchanged: `unit.core.state.import` under the sanitizer (1,257 s of 2,400 s, 1.91; the margin FX-22 decided together with the wrapper, record bound and outer limit, which are frozen constants), `fuzz.rom-admission` 2.15, every other name 2.19 or more. **Fix:** the four numbers above; a deadline is not a count, case or guard, so no coverage changed and no pass rule moved; a hung test is now reported after 240 or 480 s instead of 60, 120 or 240 s, still inside the 10 minute job limit for the 240 s names (the largest, `runner.state-export`, is a sanitizer-scale test that takes 8 s natively). Read back with `ctest --test-dir build/gate --show-only=json-v1` and the same for `build/asan` after a reconfigure: 240, 240, 240 and 480 on both presets; inventories PASS (224 and 226); the qualifier's registered-TIMEOUT control (maximum 2,400, wrapper 600 s above it) still passes. Cost: the `runner` tree identity changes once (FR record refreshed); a covered file edit after the verifier's digest (see the SUMMARY handoff).

## FR freeze record

**Status: in force.** The local tag `phase09-fr` is on `8dd6af1cda44fe021fcc4b42c4103ffd5148c986` (not pushed; 09-11 pushes it with its first push) after both rehearsals, with the accepted hosted time deviation DEV-17 (macOS cold projection 619 s). Written by plan 09-10 Task 6 step 1, before the rehearsal, so the rehearsed commit contains it; the tree identities were refreshed after FX-38 (the `runner` and `scripts` trees changed by that fix) and after FX-39 (the `runner` tree again, `runner/CMakeLists.txt`); no other tree and no frozen file changed. The local tag `phase09-fr` is created on the rehearsed commit only after both rehearsals pass (step 6) and is not pushed by this plan; plan 09-11 pushes it with its first push. FR is the end of 09-10 (D-06): from the tagged commit on, all logic is frozen and 09-11 may edit data only.

### Frozen logic set

Git tree identities at the commit being recorded (`git rev-parse HEAD:PATH`; this docs commit touches only `.planning/`, so the identities are the same before and after it):

| Tree | Identity |
| --- | --- |
| `core` | `0b0227638576babc7645c4d653bd4c156b387c77` |
| `runner` | `0a82dabe0cf62e333fd319086b5bbd5066f68e04` |
| `fixtures` | `ea48718ab44dde5788863e3bc166d79125e8d2f2` |
| `tools` | `0f5494fa789999b4132ab8fe57c734937cc96155` |
| `tools/rights` | `1ac25b4f0027932f555fcf3dea4f23ac32e65e6f` |
| `scripts` | `3ccf2ef074f9a1989de515a82e9c32808ffced37` |
| `.github` | `2e26a9d64749aaeb5e989cec80348ac6bfffb4d9` |

The frozen file list of Task 1 (SHA-256 at the recorded commit; the verdict is against `phase09-fh`, `build/gate/09-10-freeze-check.py` with the anchored pin pattern of R3 RC-07, `build/gate/09-10-freeze-r3rc07.txt`):

| File | SHA-256 | Verdict against `phase09-fh` |
| --- | --- | --- |
| `scripts/qualify-phase09.py` | `1dc15977afcd03ee8cbcc9932749a21865bff723b9bd61bb668c365c92a2a9fb` | LOGIC, FD-05 |
| `.github/workflows/ci.yml` | `e226f9456847272d2070d86b41d2e2abee55995c32c220c9affbb7463e06eda0` | identical |
| `scripts/check-ci-yml.sh` | `d6f89479155df5699b7665403356404f13c8e7913302d928d4c5970fcb79d754` | identical |
| `scripts/gate.sh` | `d95ca44eca9757e09918b43ac9dd5c4f0c6e23b25a8a3785339cd9988038e242` | identical |
| `scripts/rights-scan.sh` | `f1aaf59c51a2fc959aaf6e0f0ce93e5a6ed78163bf72aac287a4bf97736fdb06` | DATA (selftest cases beside existing ones, 09-05) |
| `tools/rights/t64_blobscan.c` | `d2e506ea05130c32917c4143231b8d0d3a3b02f4d889f26c7bb0aac72bb133f1` | identical |
| `scripts/check-test-inventory.sh` | `253b855d62a0f134651821897165861b9a506b25bbe2f8204414d54ffe87b986` | DATA (15 test-name rows) |
| `scripts/check-rsp-no-hle.py` | `29079d8cbe46a4f09270aa1f69d5d9c771e5110518fbf083bf8b8afc89d9a18d` | LOGIC, FD-04 (data names: the digest table, the canonical source list and the raster function inventory, FD-02) |
| `scripts/tests/test_rsp_no_hle.py` | `4bfc6cb345493f4d5965c40e8532e12c2291e31c9d74b9719f49be2e8e7be177` | LOGIC, FD-01 |
| `scripts/check-phase09-cases.py` | `469ffe807205c52b2b1990f3762f8e3bea945bbbb3f96cd072355e8269305c92` | identical |
| `scripts/qualify-rdp-vi.py` | `164ba18975d5654e642913959ae4f16b5f65752d8c2b867b403d13b887d4cde9` | LOGIC, FD-06 |
| `scripts/qualify-audio.py` | `d6d391bc54c97b7dab3b1c4c996d7e3e51d4de46eb0321e7857a9ce77ec63b9d` | identical |
| `scripts/check-audio-cases.py` | `fd2b1745f13d27c50607b618b230f924e6ee69b09fd085b86a111c054f62e44f` | identical |
| `scripts/collect-boot-evidence.sh` | `8c41f7f9ca6d845d7a367c314a98a248ca57a74cad7d43d651dd39842b51c241` | identical |
| `runner/t64_artifact_publisher.c` | `2903dc1448f97f2e9a682c39ddaca97a9e378b1c31dbd5497f01e8c5e7acdcf5` | LOGIC, FD-03 |
| `runner/t64_artifact_publisher.h` | `1834611746bf5665a6ab146e529339040840f320bdf8d6f3a17ea4907f15c79f` | LOGIC, FD-03 |
| `scripts/build-boot-fixtures.sh` | `f2defb1eed35c3bc9aa72e87b64f97a23bab7973e5c949ce108b3c316d644d11` | identical |

17 files: 9 identical, 2 DATA, 6 LOGIC (five FD ids, FD-03 covers two files). The R3 RC-07 correction of the table changed one thing: `NATIVE_SHARED_STAGE_TIMEOUT_SECONDS` and `NATIVE_SHARED_RECORD_MAX_TIMEOUT_SECONDS` of `scripts/qualify-phase09.py` are now reported as LOGIC assignments inside FD-05 (the old substring pattern `_SHA` matched `_SHARED` and listed them as data); the file was LOGIC through FD-05 before, so no verdict moved. `scripts/build-boot-fixtures.sh` is identical, as required.

### Declared FH deviations (FD) and their cost

Each is a logic change to a file frozen at `phase09-fh`, made by this plan's fix cycle for a recorded finding (reason and no-weakening statement in the DEV entry named):

| Id | Files | Entry | Cost |
| --- | --- | --- | --- |
| FD-01 | `scripts/tests/test_rsp_no_hle.py` | DEV-06 (and the data rows of DEV-03) | 73 mutation seeds, one new helper, two test defects of plan 07-01 repaired; no check removed or weakened |
| FD-02 | `scripts/check-rsp-no-hle.py` (the raster function inventory literal only) | DEV-03 | DATA under the corrected table; two names and four digests |
| FD-03 | `runner/t64_artifact_publisher.c`, `runner/t64_artifact_publisher.h` | DEV-04 | the publisher transaction changed after the only real hosted pair |
| FD-04 | `scripts/check-rsp-no-hle.py` (`_check_source_route`) | DEV-05 | two more bodies pinned and two invalidation tokens checked |
| FD-05 | `scripts/qualify-phase09.py` | DEV-07 | native limits, the full-matrix step and its marker check; one more native original step (74 s natively) |
| FD-06 | `scripts/qualify-rdp-vi.py` | DEV-08 | the five shared stage limits named, the ASan and gcc13 values raised; Phase 8 `scripts/qualify-audio.py` keeps its own 2,400 s limits (F-106) |

Common cost: every file above is in the bound source set, so the source digest of the qualification differs from the one of the first hosted pair (`2db1be4`), and the changed logic has no hosted data from before this plan. The hosted rehearsal of Task 6 on the rehearsed commit (the commit the tag `phase09-fr` marks) is the hosted evidence for all of them; its result is in the section `Hosted rehearsal result`. No other file of the frozen list differs from the tag.

### Open non-blocking findings and the wording they impose on 09-11

| Finding | Owner | Wording impact for the claims and their `not_claimed` lists |
| --- | --- | --- |
| R1 RC-06 | Phase 10 | the state and digest versions are provisional and no version marker moved with the new state content; claim equality and refusal behavior on the exact SHA only, not a durable state format |
| R1 RC-07 | Phase 10 | none (a NULL-guard convention) |
| R2 RC-04 | Phase 10 | none (a dead link input of `test_replay`) |
| R2 RC-06 | Phase 10 | do not write "never touched" or "publishes nothing" without the stated limits: a same-uid swap and a kill between the rename and the withdrawal can leave a leaf (FX-29, security F-3) |
| R2 RC-08 | Phase 10 | none (a failing runner test erases its own evidence unless `T64_KEEP_TEST_EVIDENCE` is set; CI does not upload those directories) |
| R3 RC-02 | next Phase 8 requalification | a new Phase 8 native qualification on this tree would be refused closed by its own verifier (F-106); do not claim Phase 8 requalification on this tree |
| R3 RC-04 | 09-11 | the step records of the native qualification are unsealed originals; the matrix JUnit is the only proof that `T64_STATE_MATRIX_FULL=1` took effect |
| R3 RC-05 (remaining) | Phase 10 | none for claims (the TIMEOUT control reads tracked `CMakeLists.txt` files only) |
| R3 RC-07 | orchestrator script, closed by this task | none |
| L4 IN-04 | 09-11 | a manifest of a rehearsal SHA is distinguished from a qualifying one by `workflow_sha` only |
| L4 IN-05 | 09-11 | the first hosted pair's state digests are valid for source `2db1be4d` only (FX-27); the 09-11 pair supersedes them |
| Security F-1 | Phase 10 | the triangle campaign at gate defaults reaches reason 40 only; the field readers are reached by the registered sanitizer counts and the named mutants; do not claim reader reach from the gate run |
| Security F-2 | Phase 10 | `RLIMIT_FSIZE` and `RLIMIT_CORE` results are discarded in the child; the exact-backstop claim does not hold on a host whose inherited hard limit is lower |
| Security F-3, F-5 | Phase 10 | the publisher withdraw is check-then-unlink against a same-uid race; `T64_REAL_KEEP_WORKSET` is still honored and a verified-input copy sits in `TMPDIR` (mode 0600) until unlinked; no claim of protection against a same-uid actor |
| Security F-8 and the margin items | Phase 10 | native qualification costs about 30 minutes or more; `unit.core.state.import` under the sanitizer is 97 percent of the sanitizer wall and a split needs a name the budget forbids |

Claim-wording limits that come from the fix cycle itself, carried in `09-CONTEXT.md` K20: 17 matrix rows B01 to B17 (four full, thirteen sampled, the full set only in the native step `normal-state-import-full-matrix`), a resumed runner run never reports `pass`, the publication contract by outcome (a caller accepts a leaf only with exit status 0 and a `pass` record), the RDP state carried for the four family ids only, raw check totals are not strength evidence, and `09-VALIDATION.md` stays draft until the final verification.

### Final gate-name count

45 Phase 9 CTest names (the `PHASE09_TESTS` block of `scripts/check-test-inventory.sh`, 45 of the K13 budget of 45), no ASan-only name added (`ASAN_ONLY` holds the pre-Phase-9 `fuzz.rom-admission`), gate inventory 224 names and sanitizer inventory 226. Plan 09-11 may add none; a hermetic case pair inside an existing test file is the only test addition it may make.

### What 09-11 may change (data only) and what is a qualification reset

After the tag `phase09-fr` 09-11 may change only:

- the literal pin constants and the `HOSTED_ROWS` data of `scripts/qualify-phase09.py` (constants and data tuples, no function body);
- the PHASE09 rows of `scripts/check-test-inventory.sh` (a row may be removed with its budget record; none may be added);
- `fixtures/manifest.json` pins (and, if a row moves, the owner-gated oracle request is an orchestrator task: run `ctest -R '^harness\.rdp-oracle-authority$'` at once);
- `docs/spec/phase09-cases.json`;
- `docs/claims/phase09.md`;
- `docs/evidence/phase09-*`, including the completion of the success-criteria draft of Task 4 from the retained originals;
- `docs/ci-reviews/LOG.md`, only through `scripts/ci-review.sh --append`;
- `.planning/`;
- the one sanctioned exception `scripts/tests/test_phase09_qualification.py` (the F2 pin table and the claims-final case replacing the skeleton case), declared here so the freeze check does not file it as an FD.

Everything else is a declared qualification reset. A hosted overrun (warm above 540 s, or cold projection above 600 s) found in 09-11 cannot be repaired there by a logic edit; it is a declared deviation that forces a new candidate cycle, which is why the hosted rehearsal of Task 6 runs in this plan, before the freeze.

### Handoff obligations for 09-11 from Task 6

- **Hosted time (DEV-17).** gate-macos warm 433 s (inside 540 s), cold projection 619 s (19 s over 600 s, accepted); gate-linux warm 242 s, cold 326 s. Before 09-11's first push run `gh cache list --repo szTheory/tendies64` and confirm the Binutils entries for Linux and macOS exist, are recent and carry the current key (`binutils-<OS>-<ARCH>-<hash of scripts/build-boot-fixtures.sh>`, key unchanged because that script is frozen); a cold macOS run is a known risk of about 619 s against the 600 s job timeout. 09-11 adds only a hermetic test-file case pair and documents and records its own measured seconds against these figures; a hosted overrun found there is a declared deviation, not a logic repair.
- **Stale verifier digest.** The digest of `09-VERIFICATION.md` (`v4:sha256:955f5003...`, verified at `944443f`) predates the K20 amendment of `09-CONTEXT.md`, the Task 6 edits of this record, FX-38 (`runner/tests/t64_test_tree.h`, `runner/tests/test_pad_poll.c`, `scripts/tests/test_phase09_qualification.py`) and FX-39 (`runner/CMakeLists.txt`); 09-11 performs the confirmatory re-verification after F2. Nothing of this plan edits `09-VERIFICATION.md`.
- **Owner metadata.** A native record binds `AGENTS.md`, `.planning/config.json` and `.planning/AUTONOMY.md` (`OWNER_METADATA_PATHS`); do not edit them between the native qualification run and its hosted verification.
- **Margins to watch.** `unit.core.state.import` under the sanitizer 1,150 to 1,257 s of 2,400 s (1.9 times, the FX-22 decision; the wrapper, record bound and outer limit are frozen constants); `runner.state-export` under the sanitizer 160 to 182 s of 480 s; hosted macOS `unit.core.state.import` 190 s and `harness.state-field-use-audit` 65 s of 240 s.
- **Cheap checks to repeat before 09-11's first push** (F-107, F-108): replay the CI steps that follow Gate against the local build (about 20 s), and compare every registered TIMEOUT with the largest native, sanitizer and hosted seconds (at least twice).
- **Claim wording.** The K20 limits of `09-CONTEXT.md` and the open-findings table above stand; no CTest name may be added (45 of 45).

### Procedure 09-11 uses to verify the freeze

1. `git diff --quiet phase09-fr HEAD -- core runner tools scripts .github fixtures` must exit 0 except for the data paths above; list the rest with `git diff --stat phase09-fr HEAD -- core runner tools scripts .github fixtures`.
2. `python3 -B build/gate/09-10-freeze-check.py` (untracked helper; to compare against `phase09-fr` instead of `phase09-fh` change its `TAG`) and the body comparison it performs: parse `git show phase09-fr:PATH` and the working file, `ast.dump` every top-level function and class, and list the names that differ; allowed differences: none for logic. For `scripts/qualify-phase09.py` only module-level data may differ (`HOSTED_ROWS`, the literal pin constants; `SOURCE_PATHS` and `PENDING_SOURCE_PATHS` must not change); the five shared limits, `FULL_MATRIX_*` and `PHASE09_SOURCE_BASE` are never data. For `scripts/check-test-inventory.sh` every changed line must be a bare test-name row. For the shell, YAML and C files the diff must be empty.
3. Compare the SHA-256 table above with `shasum -a 256` of each file; a mismatch of a file that steps 1 and 2 passed is legitimate only for `scripts/check-test-inventory.sh` (data rows) and `scripts/qualify-phase09.py` (data tuples).
4. `sh scripts/check-test-inventory.sh --preset gate` and `ctest --preset gate -N` must print the same Phase 9 name count (45).

## Hosted rehearsal result

**Status: accepted with a recorded deviation (DEV-17).** The hosted rehearsal on `8dd6af1` met the warm limit on both jobs and the cold projection on gate-linux; the gate-macos cold projection is 619 s against 600 s. The one repeat of steps 2 to 4 (plan 09-10 Task 6 step 3) was spent (the FX-39 cycle); the orchestrator accepted the measured result instead of a third candidate cycle. The tag `phase09-fr` is on `8dd6af1` (local, not pushed) and the FR record above is in force. Both rehearsal records exist and verify; neither is qualification evidence.

### The rehearsed candidate

Source `8dd6af1cda44fe021fcc4b42c4103ffd5148c986` (prefix `8dd6af1c`), HEAD of `main` when pushed (commits after it: `[skip ci]` records only). It is the second candidate of this task: `0d31e0e` (the FR record; its native start was stopped after 6 minutes when a hosted failure of the same code was found, DEV-15), `16849e7` (FX-38 applied; native exit 0 in 1,652 s; hosted run 44784312630 failed, DEV-16) and `8dd6af1` (FX-39 applied). Repairs made: FX-38 (a runner test that retains run evidence keeps its scope past exit) and FX-39 (four registered TIMEOUTs raised, data only). No frozen file was edited by either: the freeze check against `phase09-fh` after both (`build/gate/09-10-freeze-final.txt`) shows the same table as after G4: 17 files, 9 identical, 2 DATA, 6 LOGIC (FD-01, FD-03 twice, FD-04, FD-05, FD-06), no new FD entry.

### Native rehearsal on 8dd6af1 (`qualify-phase09.py --native --rehearsal`, exit 0, 1,486 s)

Private record: `build/gate/09-11-phase09-private/native-rehearsal-10-8dd6af1cda44fe021fcc4b42c4103ffd5148c986/native-record.json` (untracked, bound by SHA-256 `60b1850b74153b5fb212be96dd55401929d1d60f12abcd7cc25e1bc41a4b78b3` into the hosted record). The record verifies standalone with the rehearsal label and is refused by a final-mode verification (the label differs). Printed: `qualify-phase09: native qualification passed` and `qualify-phase09: this result is a rehearsal and not final evidence`.

| Step | Seconds | Limit |
| --- | --- | --- |
| shared native stage | 1,421.6 | 4,800 |
| normal gate (`sh scripts/gate.sh`) | 136.1 | 600 |
| ASan configure | 0.2 | 120 |
| ASan build | 8.0 | 600 |
| ASan and UBSan full CTest (226 of 226) | 1,184.8 | 3,000 |
| gcc:13 closure (64 units) | 52.5 | 300 |
| `normal-state-import-full-matrix` (first run of the new step in the whole flow) | 53.0 | 300 |
| 26 inventory checks (13 suites, normal and sanitizer builds) and 2 source-body checks | 0.2 to 0.5 each | 720 and 120 |

The ten slowest Phase 9 names in the normal gate JUnit (seconds): `unit.core.state.import` 48.4, `harness.state-field-use-audit` 18.6, `runner.state-export` 7.6, `unit.core.sched.matrix` 6.9, `harness.phase09-qualification` 6.8, `unit.replay.suite` 4.5, `runner.save-roundtrip` 4.5, `runner.replay-mix` 4.4, `unit.core.state.export` 4.3, `unit.core.save.device` 3.2. Under the sanitizer: `unit.core.state.import` 1,149.9 (97 percent of the 1,184.8 s wall), `runner.state-export` 160.4, `unit.core.sched.matrix` 120.4, `runner.replay-mix` 100.7, `unit.replay.suite` 98.1, `runner.save-roundtrip` 92.8. The new limits and the matrix marker were exercised end to end with no refusal. Margins to watch (registered TIMEOUT against the sanitizer figure): `unit.core.state.import` 2,400 against 1,150 to 1,257 s (1.9 times, the FX-22 decision), `runner.state-export` 480 against 160 to 182 s after FX-39.

### Hosted rehearsal: run 44790405712, attempt 1 (public repository run `https://github.com/szTheory/tendies64/actions/runs/44790405712`)

Both jobs succeeded on exactly `8dd6af1c`; Binutils cache hit on both (restored from the key). Collector and rehearsal verifier passed on the pair (`--collect-hosted --rehearsal` 10 s, `--verify-evidence --rehearsal` 5 s): `qualify-phase09: complete native, ASan, GCC13, and both hosted platform records verified` and `qualify-phase09: this result is a rehearsal and not final evidence`. Hosted record `hosted-8dd6af1cda44-44790405712-1-...` under the private root; public output `build/gate/09-10-public-rehearsal-8dd6af1c` (outside the private store, the same correction as 09-03.1 deviation 7: the plan's path under the private root is refused with `public phase09 output must be a fresh directory`).

| | gate-linux | gate-macos |
| --- | --- | --- |
| artifact id | 14657170346 | 14657440522 |
| archive digest | `sha256:faae546de2b47becc721650d604ff5c0a82f8dccfaef232331dfe4e0d370650d` | `sha256:7ed8b5c27f7064907c1c1665419ed406843071756a8f927dfa93bb7bcdab13de` |
| public manifest SHA-256 | `baabf8038c027d623b97dba9fbb8c2e1e0e444b07a8cc9603dd8b630d326f78c` | `33094c7ec977f66477dae56229abb0700d5e0d08360237ab7923b11c9bd2ae97` |
| queue / run (warm) | 2 s / 242 s | 2 s / 433 s |
| Gate step | 194 s (ctest 107, test-inventory-selftest 24, build 19, core-lint 14, lint-selftest 13, boot-evidence-selftest 13) | 361 s (ctest 218, test-inventory-selftest 47, boot-evidence-selftest 31, build 25, core-lint 21, lint-selftest 11) |
| audio qualification records | 33 s | 37 s |
| Phase 9 case run, emit, PII scan | 2, 1, 0 s | 6, 0, 1 s |
| upload | 1 s | 2 s |
| increment over F-100 (129 s Linux, 275 s macOS) | +113 s | +158 s |
| cold projection (warm plus 84 s or 186 s) | **326 s** | **619 s** |
| gate (warm at most 540 s, cold at most 600 s) | pass | **FAIL: cold projection 619 s is 19 s over** (warm 433 s passes) |

The plan's second verify block exits 1 on the macOS line. The upload stayed inside the frozen contract (one public manifest per job, `phase09/gate-*/manifest.json`).

**What the number is made of.** macOS ctest: 224 names, summed test time 810 s on four jobs, wall 218 s, the Phase 9 names 449 s (55 percent); `unit.core.state.import` 190 s alone (native 48 s, about 3.9 times) and `harness.state-field-use-audit` 65 s (native 19 s). Run-to-run spread of the same macOS job is as large as the margin: 349 s at the plan base `d4ccad5` (run 44678456339, cold projection 535 s), 420 s at `7d8f817` (run 44713831889, before any fix, 606 s), 433 s here; the Gate step alone ranged 304 to 379 s across near identical code. The two earlier runs of this task, both partial: run 44776046324 (`49069a7`, failed at the emit, DEV-15): Linux 251 s and macOS 367 s up to the failure; run 44784312630 (`16849e7`, macOS failed at Gate, DEV-16): Linux 189 s complete, macOS Gate 379 s and 398 s run before the later steps (about 433 s complete, projected cold about 619 s). The completed-run ledger and findings F-107 to F-110 are in `docs/ci-reviews/LOG.md`.

### The stop and the choices it left (decided: choice 3 with DEV-17; choices 1 and 2 were not taken)

The measured macOS cold projection is at the margin and not a deterministic function of the code; a single sample passes or fails on runner noise. Choices, with the measured cost of each (all are covered edits before the freeze that need a new native and a new hosted rehearsal, a third candidate cycle):

1. A time repair of macOS by about 60 s of warm time (to about 375 s, cold about 560 s) from levers the plan names: the sampled state import matrix at the gate defaults (190 s of macOS CPU, the longest single test), the field-use audit selftest (65 s of macOS CPU, 15 s natively), and starting the longest test first through the CTest COST property (the wall of 218 s against 203 s of summed time over four jobs). None of these is evidence-neutral without a decision on sampling. The audio qualification records step (37 s macOS, 33 s Linux) re-runs the audio names the Gate already ran (F-097, F-105) but sits in the frozen CI contract (an FD entry for `.github/workflows/ci.yml` and `scripts/check-ci-yml.sh` and a hosted rehearsal).
2. A declared deviation that widens the cold margin (for example the 10 minute `timeout-minutes` of the macOS job, a frozen file edit with an FD entry) or accepts the measured figure with the warm figure inside 540 s, recorded as a known limit for 09-11, which can then only record its own measured seconds.
3. Accept the single sample as the rehearsal result and record that the cold projection is a projection, not a spent run (a cold run was never made).

**Decision (orchestrator):** choice 3, recorded as DEV-17 and in the FR handoff obligations: no third candidate cycle, no frozen `ci.yml` edit, the Binutils cache freshness is checked before 09-11's first push.

State of the tree: the tag `phase09-fr` is on `8dd6af1` and is not pushed (`git ls-remote --tags origin phase09-fr` prints nothing); nothing under `core`, `runner`, `tools`, `scripts`, `.github` or `fixtures` differs between the tag and HEAD; only `docs/ci-reviews/LOG.md` and files under `.planning/` follow the tag.
