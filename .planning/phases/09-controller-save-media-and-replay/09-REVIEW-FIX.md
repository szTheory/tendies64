---
phase: 09
plan: 09-10
task: 5
fixed_at: 2026-11-20
review_path: .planning/phases/09-controller-save-media-and-replay/09-REVIEW.md
security_path: .planning/phases/09-controller-save-media-and-replay/09-SECURITY.md
verification_path: .planning/phases/09-controller-save-media-and-replay/09-VERIFICATION.md
iteration: 1
cycle_groups: [G1, G2, G3]
status: g1_g2_complete_g3_open
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

Commit 0903cf1. **RED** (`build/gate/09-10-fx-04-red.log`): a `swr` at byte 3 of RD64B with room for two events retired as the fourth instruction, queued both SI events (queue 16 against 14), changed the SI state and the digest, and the sequence analogue failed alike; the `sw` control refused atomically. **Fix:** the budget translates a partial store's byte as the store path does and keys on the aligned word (the reverse-endian lane exchange for a full word). **Green:** refuse-then-accept pairs for `swr` and `swl` at both registers and the sequence analogue; the accept halves assert OK and four retired instructions. Limit: the reverse-endian branch mirrors the store path by reading; it is not reachable from the privileged unmapped window in user mode, so it has no control (recorded).

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

Commits edf2651 and 0a13f91 (a follow-up that lowered the matrix bound to 20,000 ticks for the sanitizer cost). Added inside existing cases (no new name): `argv-bound` of `runner.supervisor-argv-bound` runs each of the nine families alone as the accepted baseline (not a usage refusal), all 36 pairs (refused as usage at guest tick 0 with the artifact and raw directories empty, or accepted), the four producers without the pair, and the supervisor in synthetic mode with each of the 19 Phase 9 flags plus RAW and INPUT after a baseline that ends in the stand-in child's own `no-result` record; `import-refused` of `runner.save-roundtrip` runs the direct runner with `--save-import-fd` (accepted descriptor with the pinned transcript digest, then the linked, writable, offset, short and pipe descriptors as prelaunch refusals at tick 0 and a missing kind as a usage refusal); `import-option-refusals` of `runner.state-export` runs the state import with the artifact pair alone as an accepted run with an empty artifact directory. Every family carries a nonexistent path, so a supervisor that prepared a snapshot before the pair rule would report `refusal` instead of `usage`. **Before-state evidence** (these are tests of behavior the code already had, so RED is the survival of mutants before the rows existed; `build/gate/09-10-fx-13-red.log`, drivers `build/gate/09-10-regress/mutants_fx12.py` and `mutants_fx12b.py`): ten supervisor mutants and five runner mutants, each built under the gate flags, run and restored (re-run after the follow-up, all kills unchanged): replay accepting the save flags (4 failing checks), replay without the pair (2), state export without the pair (1), synthetic mode accepting `--replay-report` (1) and `--state-import-sha256` (1), raw capture with the artifact pair (1), three accepted pairs refused (SAVE or REPLAY with INPUT: 2 and 2; STATE-IN with the pair: 1 in the matrix and 1 in the state case), state import with the schedule (1 and 2), the runner accepting a linked descriptor (4), a writable descriptor (4) and never reading the save descriptor (113): all killed. Two descriptor checks of `read_snapshot` (the offset and the regular-file test) are redundant behind the read-to-length loop and their mutants are equivalent; one replay-validator check (raw capture) is redundant behind the pair rule and was not mutated.

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

Unchanged on purpose: the idle digests of the 17 pre-Phase-9 ids (literals of `unit.core.digest.sink-legacy-identity`), the profile hashes, every transcript pin and every identity of `fixtures/manifest.json` and `docs/evidence/phase09-first-hosted-pair.json`; no manifest row changed, so the oracle authority files were not touched.

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
- Full gate CTest on the final tree: 224 of 224 passed, 83.4 s real at `-j 4` (`build/gate/09-10-g2-full.log`, `.junit.xml`, `.LastTest.log`); the heaviest names `unit.core.state.import` 59.6 s, `harness.state-field-use-audit` 19.3 s, `runner.cpu-diagnostic` 18.4 s.
- Pinned gcc:13 closure over the 11 changed C units (`runner/t64_artifact_publisher.c`, `t64_real.c`, `t64_supervisor.c`, `t64_replay.c` and the seven changed test sources): clean, 11.7 s (`build/gate/09-10-g2-gcc13.log`).
- Linux compile pre-flight of `docs/ci-reviews/LOG.md` F-102 without forcing a platform: the 219 compile commands of the gate build (the macOS-only `-arch` and `_DARWIN_C_SOURCE` removed, outputs discarded) rerun with the Ubuntu 24.04 `cc` (13.3.0) of the act image: 219 units, no diagnostic, 34 s (`build/gate/09-10-g2-linux-preflight.log`).
- ASan and UBSan for the touched runner units (`cmake --build --preset asan` then `ctest --preset asan -j 4` over `runner.artifact-publisher`, `controls.artifact-substitution`, `runner.state-export`, `runner.save-roundtrip`, `runner.replay-mix`, `unit.replay.suite`, `runner.supervisor-argv-bound`, `runner.phase09-tracer`, `runner.phase09-admit`, `runner.pad-poll`, `runner.save-fixtures`, `harness.fsize-artifact-real-fault`, `harness.fsize-report-capture-still-bounded`): 13 of 13 passed, no sanitizer report, 164.1 s real (`build/asan/09-10-g2-asan-ctest.log`). Per name under load 11: `runner.state-export` 145.2 s, `runner.replay-mix` 91.4 s, `runner.save-roundtrip` 91.3 s, `unit.replay.suite` 86.7 s, `runner.supervisor-argv-bound` 45.0 s in this batch and 27.4 s alone after the follow-up that lowered the matrix bound from 200,000 to 20,000 ticks (native 0.9 s; the 36 pair rows, nine baselines and 21 synthetic rows are the new cost, mostly process start under ASan; registered `TIMEOUT` 120 s), `harness.fsize-report-capture-still-bounded` 20.2 s, `runner.save-fixtures` 19.6 s, `runner.phase09-admit` 18.7 s, `runner.phase09-tracer` 17.8 s, `harness.fsize-artifact-real-fault` 13.7 s, `runner.pad-poll` 11.0 s, the two publisher names 0.12 s each. The state-import ASan matrix was not run (group G3 owns its timing).
- Native seconds, quiet moments of the same host: `runner.supervisor-argv-bound` 1.1 s, the publisher names 0.02 s each, `runner.state-export` 5.8 to 8.1 s, `runner.save-roundtrip` 4.0 to 5.7 s, `runner.replay-mix` 4.0 to 5.6 s, the batch `^(runner|controls)\.` (81 names) 22 to 26 s at `-j 4`.
- Freeze check against `phase09-fh` (`build/gate/09-10-freeze-after-g2.txt`): 17 files, 11 identical, 2 DATA (`scripts/rights-scan.sh`, `scripts/check-test-inventory.sh`), 4 LOGIC: `scripts/tests/test_rsp_no_hle.py` (FD-01), `scripts/check-rsp-no-hle.py` (FD-02) and the two new verdicts of this group, `runner/t64_artifact_publisher.c` and `runner/t64_artifact_publisher.h` (FD-03, DEV-04). No other frozen file differs; no frozen file other than the publisher pair was edited by G2 (`scripts/qualify-*.py`, `scripts/tests/test_phase09_qualification.py`, `ci.yml`, `check-ci-yml.sh`, `gate.sh`, the rights scanner and `check-rsp-no-hle.py` are untouched).
- Guard pin refresh: none needed (the guard pins no `runner/` body); no `fixtures/manifest.json` row changed, so `harness.rdp-oracle-authority` is untouched.
- Mutants (working-tree, built under the gate flags, restored): FX-10 one (three removals), FX-13 twelve (plus two equivalent and one redundant check named in FX-13); drivers in `build/gate/09-10-regress/mutants_fx12.py` and `mutants_fx12b.py`.

### Open after G2

- G3: unchanged from the G1 list (L4 CR-01, WR-01, WR-02 pin, IN-01, IN-03, VG-03, VG-05 guard part, VG-08), with two more inputs: the ASan seconds above (`runner.supervisor-argv-bound` grew to 27 to 45 s under load against its registered `TIMEOUT` of 120 s; re-measure it if the qualifier wrapper sums runner names) and the new FD-03 (a frozen-file edit already recorded here).
- Orchestrator: `09-VALIDATION.md` frontmatter and status column (VG-11, still draft); the claim wording of 09-11 (the closed unknown-log record of a resumed runner-published state, FX-11; the stated replay checkpoint limit, FX-16; `T64_REAL_KEEP_WORKSET` as a Phase 8 carry, FX-18); the targeted re-check and the gaps-only re-verification now include the G2 files (runner publisher, supervisor, real runner, replay, the seven runner tests, `result-protocol.md`, `state-container.md`, `replay.md`, `save-media.md`).
- Phase 10: closed detail names for the recorder failures (L3 IN-01), the in-process replay driver duplicating the runner loop (L3 IN-02), the production `T64_REAL_KEEP_WORKSET` retention switch (L3 IN-06).
