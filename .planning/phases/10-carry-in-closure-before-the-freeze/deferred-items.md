# Phase 10 deferred items (out-of-scope discoveries)

Items found while executing a plan that the plan did not own. Each is recorded, not
fixed, per the scope boundary of the executor rules. Evidence tier: code and
measurement observations on the local build tree; nothing here is a compatibility or
hardware claim.

## D-10-17-01: `integration.cold-hello` and `integration.cold-startup` stop working after 99 gate runs in one build tree

Found by plan 10-17 (Task 3, first rehearsal attempt). Not caused by Phase 10.

| Item | Observation | Status |
|------|-------------|--------|
| symptom | `integration.cold-hello` printed `not ok cold startup evidence directory prepared` in 0.02 s and failed `scripts/gate.sh` at its ctest step (254 tests, 1 failed); the rest of the gate passed | resolved |
| cause | `make_evidence_dir` in `runner/tests/test_cold_boot.c` (lines 124 to 139, a Phase 6 test) creates `build/gate/06-05-cold-hello-attempt-NN` for NN from 01 to 99 and returns false when all 99 exist. Every gate run in the same build tree consumes one attempt number and nothing removes them. At the failure `06-05-cold-hello-attempt-01` to `-99` all existed (the 99th came from the stage-measurement gate run of this plan) and `06-05-cold-startup-attempt-*` stood at 94, so `integration.cold-startup` would have failed after five more gate runs | resolved |
| scope | a long-lived local build tree only: a fresh clone or a hosted runner starts at attempt 01. The hosted CI, the first Phase 10 rehearsal cause and every gate before this one were unaffected | resolved |
| workaround used | the 193 attempt directories (about 1.5 MB) were moved, not deleted, into `build/gate/archived-closed-phase-runs/` (the AUTONOMY v1.2 rule "prune closed-phase run directories" in its non-destructive form); both tests then pass at attempt 01. A broader prune of `build/gate/08-04-evidence` (3,404 entries, 467 MB) and `build/gate/09-05-evidence` (2,044 entries) was refused by the permission system and was not done; neither blocks any test | resolved |
| not fixed | `runner/tests/test_cold_boot.c` is a closed Phase 6 test, outside the files of plan 10-17 | resolved |
| suggested owner | Phase 11 test hygiene: either lift the 99 cap (a per-process unique suffix, as the Phase 9 evidence directories use), or add the two attempt-directory families to the documented local prune step. The local preflight could also print a WARN when `ls -d build/gate/06-05-cold-hello-attempt-* | wc -l` reaches 90 | resolved |
