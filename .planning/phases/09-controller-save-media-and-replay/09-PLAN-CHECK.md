# Phase 9 plan check (iteration 1)

Checker: gsd-plan-checker (light tier) on all thirteen plans, plus the deterministic probes
(138 `<automated>` commands: 0 blocker, 0 warning for path resolution and failing direction),
plus an independent cross-plan consistency review (`09-PLAN-CROSSCHECK.md`). This file records
the plan checker's result and the orchestrator's disposition. Nothing here is an owner approval.

## Result: ISSUES FOUND (3 blockers, 5 warnings, 5 info)

Verified clean by the checker: all seven requirement IDs covered; every D-01..D-46 and K1..K16
cited by a plan; roadmap success criteria 1-4 map to named tests; 72 tasks each carry an
`<automated>` command with `<fails_when>`; dependency graph acyclic with no forward references;
no ROM, save, PIF or state blob committed or fetched; no workstation paths; guard pin refreshes
are isolated tasks with controls; hosted timing keeps the 540 s warm and 600 s cold targets;
VID-04 and oracle stop untouched.

| ID | Severity | Finding | Disposition |
|----|----------|---------|-------------|
| B1 | blocker | 09-09.1 declared wave 9 with depends_on 09-09 (also wave 9) | Accept: declared waves become the strictly sequential computed waves 1..13 (done by the orchestrator after the revision edits) |
| B2 | blocker | Gate names sum to 45 and 09-03 also registers ASan-only `fuzz.joybus`, so 46 new inventory names against the K13 budget of 45 | Accept: fold the joybus fuzz into an existing name as a case with env-raised iteration counts under ASan (the pattern 09-09 already uses for state import); no new name |
| B3 | blocker | 09-RESEARCH.md Open Questions not marked resolved | Accept: marked RESOLVED with pointers (orchestrator) |
| W1 | warning | Every plan exceeds the generic 100,000-token estimate (150k-300k), several exceed 15 files | Accept-modified: recorded scope decision below; plans were already split twice where authors judged one executor insufficient (03.1, 09.1) and CONTEXT leaves task count to discretion; the Phase 8 precedent ran five plans at the same density |
| W2 | warning | FH moved from end of 09-03 to end of 09-03.1 and the full-gate schedule differs from locked D-06/D-38/D-40 text | Accept: CONTEXT amendment and VALIDATION schedule updated (orchestrator) |
| W3 | warning | Mid-tick state import row (D-36) carried by neither 09-09 nor 09-09.1; `write_generation` on state import decided only in the review plan | Accept: mid-tick export/import cases move into the 09-09.1 boundary matrix; write_generation is decided in 09-09 with a test; 09-10 carries findings only (D-46) |
| W4 | warning | `t64_machine_replace` named in 09-03 and 09-04 does not exist (real name `t64_machine_admit_rom`) | Accept: corrected at source in 09-03, 09-04 and 09-09 |
| W5 | warning | Some verify blocks chain full builds and sanitizer runs, durations unmeasured | Accept-modified: each SUMMARY records measured seconds per verify block; build-heavy checks stay out of focused per-task commands where practical |
| I1 | info | Prohibitions all `unresolved/judgment` | Accept: attach existing scanner commands where a stop is checkable |
| I2 | info | `git ls-files | xargs` in 09-11 passes vacuously on an empty list | Accept: add a count assertion |
| I3 | info | K5 variant (si-completion-from-event-budget) not recorded as a K-amendment | Accept: CONTEXT amendment K17 |
| I4 | info | D-39 says three assembler sources; plans use six (one per family) | Accept: CONTEXT amendment K18 |
| I5 | info | 09-11 edits the bound test file | Accept: listed in the 09-11 SUMMARY |

## Cross-plan review (09-PLAN-CROSSCHECK.md) dispositions

B1 (09-09 Task 4 regex names a nonexistent CTest) fixed in 09-09. H1 replacement name: fixed with
W4. H2 qualifier source tuple omits `scripts/k5-mutants.py`, `docs/spec/state-field-rules.txt`,
`docs/spec/state-mutants.txt` (and `scripts/build-boot-fixtures.sh`): fixed in 09-03.1 and the
files' creating plans. H3 hosted time measured only once: a non-qualifying hosted rehearsal after
the 09-10 fix cycle is added so overruns are repaired before the freeze. H4: same as W3. H5 option
pairs: later flag families refuse combination with earlier families (one artifact producer per
run), verified by the 09-10 drift sweep. Medium and low items are dispositioned in the revision
edits; the goal-backward verifier lane moves inside 09-10 so findings land in the one budgeted fix
cycle before F2 (confirmatory only after 09-11).
