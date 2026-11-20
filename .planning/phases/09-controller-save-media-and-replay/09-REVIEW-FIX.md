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
status: in_progress
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
| L2 IN-01 | INFO | DOC | INFO recorded; mechanism fixed by G2 (L3 WR-02) | A runner-published state re-imported by the runner; the claim text is 09-11. |
| L2 IN-02 | INFO | LOGIC-core | INFO recorded | Dead scratch facts and an audit-only public rule constant; no behavior effect; owner Phase 10 cleanup. |
| L2 IN-03 | INFO | DOC | FIX G1 | Two header sentences (export BAD_STORAGE clear, import blob must not change during the call). |
| L2 IN-04 | INFO | LOGIC-core | INFO recorded | Retained packet metadata is memory-safe; the runner validates it; host contract of Phase 10. |
| L2 IN-05 | INFO | TEST | INFO recorded; time limit is G3 (VG-03) | Splitting the heavy cases into their own CTest names is not possible (all 45 K13 names are used, no ASan-only name); the aggregate keeps TIMEOUT 1800. |
| L2 IN-06 | INFO | DOC, LOGIC-core | NOT-A-DEFECT (f), FIX G1 (g, shared helper with L1 IN-03), NOT-A-DEFECT (B05) | (f) is L1 IN-05; (g) see L1 IN-03; the B05 row text already says "(direct test-only edit)" in its Reached column. |

### Lane L3 (runner)

| Id | Sev | Class | Disposition | Rationale |
| --- | --- | --- | --- | --- |
| L3 WR-01 | WARNING | LOGIC-runner, FROZEN-FILE | FIX G2 | Publisher failure after the rename (frozen file `runner/t64_artifact_publisher.c`: G2 records the FD entry). |
| L3 WR-02 | WARNING | LOGIC-runner | FIX G2 | Resumed run of a runner-published state: closed unknown-log form, no pass-rule weakening. |
| L3 WR-03 | WARNING | DOC | FIX G2 | `result-protocol.md` pass rule text. |
| L3 IN-01 | INFO | LOGIC-runner | FIX G2 | Replay interval versus the 4,096 checkpoint cap. |
| L3 IN-02 | INFO | LOGIC-runner | FIX G2 | Dead `t64_replay_publish`. |
| L3 IN-03 | INFO | TEST | FIX G1 | Tracer prints an expected status it never observes: fixed with L2 WR-02. |
| L3 IN-04 | INFO | DOC | FIX G1 | `runner/t64_real.c` 1115-1118 comment and `state-container.md` 552-556 sentence. |
| L3 IN-05 | INFO | TEST | FIX G2 | Five runner tests never remove run directories. |
| L3 IN-06 | INFO | LOGIC-runner | G2: `T64_REAL_KEEP_WORKSET` gated if trivial, else INFO owner Phase 10; failed `--meta` write in real mode INFO owner Phase 10 | Pre-Phase-9 carries; neither leaks blob bytes today. |

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
| T-09-82 | high, OPEN | TEST, DOC | FIX G2 (R-01 to R-04, R-06, nine pairs, six INPUT pairs defined) + G1 (resume-equal property rests on RDP-STATE) | The mid-tick row and the write_generation case exist and pass; the property they establish now includes the RDP members. |
| T-09-87 | high, OPEN | STATE-CONTENT-F1 | FIX G1 (RDP-STATE) | See above. |
| T-09-65 | low, accepted | DESIGN | INFO recorded | Orchestrator: add to the accepted-risk log. |
| Non-blocking F-1 to F-11 | info, low | various | INFO recorded; F-1 is fixed with L2 WR-03; F-2, F-3, F-5, F-6 are Phase 10 hardening items except F-6, which G2 fixes with L3 WR-01 | No threat counts toward `threats_open`. |

### Verifier gaps of `09-VERIFICATION.md`

| Id | Class | Disposition | Owner and rationale |
| --- | --- | --- | --- |
| VG-01 | blocking | FIX G1 | RDP-STATE. |
| VG-02 | blocking | FIX G1 | IMPORT-BOUNDS. |
| VG-03 | blocking | FIX G3 | Native stage wrapper. |
| VG-04 | non-blocking | FIX G1 | D-44 budget (L1 WR-01). |
| VG-05 | non-blocking | FIX G1 (controls) + G3 (guard pins) | L5 WR-01 to WR-03, L1 WR-02. |
| VG-06 | non-blocking | FIX G1 | EEPROM read with rx above 8, SRAM DMA ending exactly at and two bytes past the window end, each with a mutant. |
| VG-07 | non-blocking | FIX G2 | Nine pair tests, synthetic-mode refusal, `--save-import-fd`, spec sentences for the six INPUT combinations. |
| VG-08 | non-blocking | FIX G3 | Full matrix as a native step or "sampled" wording bound. |
| VG-09 | non-blocking | FIX G1 | Campaign, tracer tuple, field-use scan, report span (L2 WR-01 to WR-04). |
| VG-10 | non-blocking | FIX G2 | Publisher failure contract and resumed-run record. |
| VG-11 | non-blocking | FIX G1 (`si-pif-controller.md`, `machine-contract.md`) + G2 (`result-protocol.md`) + orchestrator (09-VALIDATION status at phase completion) | Spec and record drift. |

### Drift sweep records R-01 to R-06 (Task 2)

| Id | Disposition | Owner and rationale |
| --- | --- | --- |
| R-01 | FIX G2 | Test for synthetic-mode refusal of the 19 Phase 9 flags. |
| R-02 | FIX G2 | Tests for the nine defined pairs that have none. |
| R-03 | FIX G2 | Define the six INPUT combinations (accepted by design) in the spec. |
| R-04 | FIX G2 | Direct runner test of `--save-import-fd`. |
| R-05 | FIX G1 if trivial | Shared saturating increment helper (with L1 IN-03). |
| R-06 | FIX G2 | The 45 of 64 slot sum bound and the replay-row wording of the spec matrix. |

### Tracer and confirmation items

The tracer reaches the inventory reason but not the inventory tuple (L2 WR-02, L3 IN-03): FIX G1. The `busy_until` bound (M5) is confirmed present and tested: NOT-A-DEFECT. The replay `save_init` limit (M6) is a stated limit; the unpinned refusal behind it is VG-07 (G2).

## Fixed issues (FX entries)

None yet. G1 appends one entry per fix, each with the commit, the RED observation, the fix and the green result.

## Golden record

No golden changed yet. G1 lists every changed golden line here with its finding id (STATE-CONTENT-F1).

## Verification notes

Not run yet.
