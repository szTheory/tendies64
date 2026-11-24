# Phase 10 independent review record

One section per phase-wave, written by the review plans (10-05 Wave 0, 10-08 Wave 1,
10-13 Wave 2, 10-18 Waves 3 and 4). Each review reads the wave's diff fresh in a scratch
copy of the committed head under `build/10-review/` (never in the shared build tree: no
`ninja -t clean`, no fixture rebuild, no mutant run there) and re-runs the cited commands
instead of trusting the plan summaries. Evidence tier: code observation and command results
at the commit named in each section; nothing here is a compatibility or hardware claim.
No oracle material (D5) was used. All paths are repo-relative.

## Wave 0

Plans 10-01 to 10-04 (phase-wave 0). Reviewed by plan 10-05 (Task 1 and Task 2 below).

| Item | Value |
|------|-------|
| Wave 0 base | `4ac633e880e0c26005cb03092e8d8485e721034c` (extracted from PIN-IMPACT.md "Wave bases" with the 10-01 base extraction idiom; an ancestor of the head) |
| Reviewed head | `87e76283968be0cdc0df4c37d025d792071e14a7` (31 commits after the base; 26 files changed, 17 of them outside `.planning/`) |
| Host | Darwin arm64, 1-minute load 7.5 to 8.4 during the scratch gate (other sessions on the same machine, so seconds are upper bounds for a quiet host) |

### Task 1: scratch gate of the Wave 0 head

Command: `sh scripts/gate.sh` inside `build/10-review/w0/src`, default environment (no
`CI` variable), with a copy (not a symlink) of `build/tools/install` so the pinned Binutils
2.45 is reused. The shared tree was not written: `git status --short` of the main tree was
empty before and after the runs.

Result of the accepted run (attempt 3): `[gate] PASS`, `[gate] total 119s`, exit 0.
Steps in seconds: sh-n 0, lint-selftest 4, fixture-qualification 1, configure 0, build 4,
boot-evidence-selftest 12, test-inventory-selftest 19, test-inventory 1, ctest 69
(`100% tests passed out of 227`, `Total Test time (real) = 69.17 sec`), core-lint 9. The
CTest count is the 224 names of the Wave 0 base plus the three Phase 10 names
`repo.p10-mutant-anchors`, `unit.core.k1.dispatch-refusal` and `unit.core.k1.apply-refusal`.

Method deviation (plan 10-05 Rule 3, recorded because later review plans repeat this
method). The plan's literal extraction, `git archive HEAD` into `build/10-review/w0/src`,
does not give a gate-capable tree, and the first two attempts failed for that reason, not
for a defect of the head:

| Attempt | Scratch tree | Result | Cause |
|---------|--------------|--------|-------|
| 1 | `git archive` extraction only | gate FAILED at ctest: 221 of 227 passed. Failed: `gov.claims.repo`, `gov.provenance.repo`, `repo.sched-k5-anchors`, `repo.p10-mutant-anchors`, `runner.cpu-diagnostic`, `harness.phase09-qualification` (four of its sub-tests) | the tree has no repository of its own and sits inside the ignored `build/` of the main repository, so `git ls-files` and `git show` see no tracked file. Five of the six name that cause in their own output on a re-run ("no files: nothing was scanned", "file not tracked: ...", "fails closed ... when git is unavailable"); the four failing sub-tests of `harness.phase09-qualification` were not traced individually and pass on the identical tree in attempt 3 |
| 2 | extraction plus `git init` and one scratch commit | gate FAILED at fixture-qualification: "manifest source revision does not contain fixtures/hello/hello.S" | the fixture script resolves the manifest's source revisions from history, which a one-commit repository lacks |
| 3 | `git clone --no-checkout --no-hardlinks` of the main repository into `build/10-review/w0/src`, then `git checkout --detach 87e7628...` | gate PASS | a complete history and index; `git status --short` empty; `git archive` of the head from the main repository and from the scratch clone have the same SHA-256 (`4b73bbef7df90190...`), so the tree is exactly the archive of the head |

Plans 10-08, 10-13 and 10-18 should use the attempt 3 form (clone, detach at the head,
copy `build/tools/install`) when they say "git archive scratch clone". The three attempts'
logs stay local under `build/10-review/` (ignored, not committed).
