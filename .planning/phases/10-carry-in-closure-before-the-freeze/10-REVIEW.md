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

### Task 2: mechanical checks (a) to (i)

`BASE` is `4ac633e880e0c26005cb03092e8d8485e721034c` for every check, read from PIN-IMPACT.md with
the base extraction idiom (exactly one 40-hex line, an existing commit, an ancestor of the head). `HEAD`
is the head of the plan's Task 1 commit (`cb65dae`, which adds only this file to `87e7628`; the
`87e7628` tree is the one that the scratch gate ran).

| Check | Command or method | Result |
|-------|-------------------|--------|
| (a) no oracle-bound file moved | `git diff --name-only BASE..HEAD` against the 13 `EXPECTED_CORPUS_PATHS` read from `scripts/check-rdp-oracle-authority.py` by `ast` | empty intersection (13 paths read; 26 files changed up to `87e7628`, none of the 13). The oracle-adjacent files that did change are the authority checker, its record `.json` and `.md` and its test, all orchestrator or plan 10-03 edits. `fixtures/manifest.json`, `scripts/qualify-rdp-vi.py`, `docs/spec/rdp-vi-cases.json`, `docs/spec/phase09-cases.json`, `docs/spec/audio-cases.json`, `scripts/qualify-audio.py`, `scripts/qualify-phase09.py` and `docs/evidence/phase08-audio.md` are byte-identical to BASE. No file of `core/t64_*.c`, `core/t64_internal.h` or `core/include` changed (no production source) |
| (b) authority still blocked | `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked`; `--require-approved` | exit 0 with "authority record valid; blocked: no oracle operation is authorized"; exit 2 with "no pinned competent owner/legal decision; oracle operations remain blocked". `TRUSTED_LOCAL_DECISION_SHA256` and `TRUSTED_PUBLICATION_DECISION_SHA256` are still `None` (the test pins both) |
| (c) one bound-CI commit | `git log --format=%H BASE..HEAD -- .github/workflows/ci.yml scripts/check-ci-yml.sh scripts/gate.sh` | exactly one commit, `4ca1054bfcc07e7f91f2efe006ee40046cf6018f`; `scripts/gate.sh` has no diff; the `ci.yml` diff is the single line `timeout-minutes: 10` to `15` of `gate-macos` |
| (d) CI scanner | `sh scripts/check-ci-yml.sh --selftest` | `cases=138 failures=0` (133 plus the five `mut_timeout` cases) |
| (e) mutant anchors | `python3 -B scripts/p10-mutants.py --check-anchors` | exit 0. Beyond the plan: `--run` in the scratch clone classified `control-equivalent-depth-cap` SURVIVED, `k1-predicate-widened` KILLED and `control-no-compile` INVALID, and each K1 case alone kills the widened predicate (75 failed CHECK lines for `dispatch-refusal`, 40 for `apply-refusal`, scratch copy edited and reverted, so a change at either refusal boundary is seen) |
| (f) digest sweep | `python3 -B scripts/digest-sweep.py --base BASE --head HEAD` | `sweep: 1599 shared lines, 0 differences, 0 head-only lines`, exit 0 (21 shipped ids, two trees built with the forced build identity, 38 s per tree sweep); the only changes in `core/` are `core/CMakeLists.txt` (test registrations and `COST`) and the new test source |
| (g) CTest name counts | `ctest --test-dir build/gate -N -R <regex>` for each row of `scripts/preflight-phase10-regexes.txt` | 17 rows, all equal to the expected count (for example `^unit\.core\.k1\.` 2, `^unit\.core\.profile` 6, `^unit\.core\.vg\.` 0, `^runner\.composite` 0) |
| (h) 39 row hashes | a fresh `python3` snippet: `json.dumps(row, sort_keys=True, separators=(",", ":"), ensure_ascii=True)`, SHA-256, against `corpus.fixture_manifest.bound_rows` of `docs/evidence/phase07-oracle-authority.json` | 39 manifest rows, 39 bound rows, the same ids in the same order, 39 distinct ids, 0 hash mismatches |
| (i) Phase 9 claim binding | manifest SHA-256 at BASE; the 39-row view digest `{schema, schema_version, entries}` from the BASE manifest; `git diff --quiet BASE -- docs/claims/phase09.md docs/evidence/phase09-qualification.md fixtures/manifest.json`; the `ast.unparse` comparison of the `F2_PINS` literal; PIN-IMPACT.md section 9 | file digest `b75864fdf4404a4691bfed12ccaceaca22879e60ea739ef3cf89ca1eee6e80e0` (equals the constant and the live file); view digest `beb2b1c8b209480199ce53d0ce30c51887bfb0769d7a6d522fd7bbe7bf2aec5f` (equals the `PHASE09_CLAIM_MANIFEST_VIEW_SHA256` literal); the two closed Phase 9 records and the manifest unchanged; `F2_PINS` equal; section 9 states the authority basis ("delegated autonomy ... not an owner approval of this specific comparator change") and the rejected alternative (re-pinning the ten closed blocks) |

Plan verify commands: the sweep (exit 0, `0 differences`); `--expect-blocked` (exit 0, text
present); the closed-record `git diff --quiet` plus `ctest --test-dir build/gate -R
'^harness\.phase09-qualification$'` (1 of 1 passed, 7.9 s). The shared tree was only read
(`ctest -N`, the two read-only ctest runs, the sweep report directory `build/gate/10-sweep`
that the sweep tool owns); no clean, fixture rebuild or mutant run happened there.

Reading of each Wave 0 diff against its plan's must-haves:

| Plan | Read | Result |
|------|------|--------|
| 10-01 | `scripts/preflight-phase10.sh` header, argument handling, step frame and the timeout audit (the remaining steps are exercised end to end by the Task 3 run); `scripts/p10-mutants.py` lines 1 to 385 (table, anchors, tree check, worktree build, classification); regex rows; inventory block | the two controls and the anchor test match the must-haves; the dirty-tree rule ignores only `.planning/` |
| 10-02 | `core/tests/test_k1_refusal.c` in full; `tools/digest-sweep/t64_digest_sweep.c` in full; `scripts/digest-sweep.py` in full | K1 pins both refusal paths on the five shipped ids with 10 and 20 refusals; the digest literals are asserted; the sweep keeps the 64-yield bound of `runner/t64_replay.c` (same `stalled > 64` test), forces one build id and refuses a non-40-hex base |
| 10-03 | `check-rdp-oracle-authority.py` diff, the record, `test_rdp_oracle_authority.py` diff, `test_phase09_qualification.py` comparator and controls | per-row lookup by id, duplicate, missing, changed and empty refusals; the Phase 9 comparator uses the pinned digest and the per-row view, refuses a missing, duplicated, re-id'd or changed Phase-9-era row, a schema change and malformed JSON, and every refused control asserts that all its lines mention the manifest |
| 10-04 | `ci.yml`, `check-ci-yml.sh`, the three CMake `COST` lines, the timeout audit change | the bound is per job and killed by single-line scanner mutants (recorded in PIN-IMPACT.md 10.3); `COST` sits outside the sanitizer conditional; no other consumer of the 10-minute value exists outside `check-ci-yml.sh` (searched `scripts`, `docs`, `runner`, `tools`, `tests`, `core`; the remaining mentions are historical records) |

### Findings

Columns: id, severity (blocking, warning, info), file and line, finding, disposition. No blocking
finding was found.

| id | severity | file:line | finding | disposition |
|----|----------|-----------|---------|-------------|
| W0-01 | warning | plan 10-05 Task 1 method (also the scratch instructions of 10-08, 10-13 and 10-18) | the literal "git archive HEAD" scratch tree has no repository, so six gate tests fail for that reason alone (table above) and a one-commit `git init` lacks the history that `scripts/build-boot-fixtures.sh` needs | resolved for this wave by the clone method (attempt 3); the orchestrator should word the later review plans' scratch step the same way (clone, detach at the head, copy `build/tools/install`) |
| W0-02 | info | `scripts/digest-sweep.py:374-387` (`coverage_problems`) | the "id has an admitted ROM" and "SI id has an export line" checks run on the BASE list only, so an id passed with `--head-extra-id` that admits no ROM produces no differences and no head-only line and passes silently | carry to plans 10-09, 10-10 and 10-13: they must assert that each of the four si-v2 ids appears as head-only digest lines (10-13 check (a) already reads the head-only lines); not a Wave 0 defect because Wave 0 passes no extra id |
| W0-03 | info | `scripts/p10-mutants.py:396-403` (`classify`) | the failing-check column holds the last failing kill check, not every one; a mutant with several kill checks does not show which of them kills it | none for Wave 0 (both K1 cases were run alone against the widened predicate above and both kill it); later mutant rows that claim a kill by a named check should run that check alone |
| W0-04 | info | `scripts/preflight-phase10.sh` timeout audit (`df5442f`) | the four tests with a `COST` are now held to 2 times that hosted `COST` instead of 3.5 times native seconds: ratios 2.86 (`unit.core.state.import` 600 s on 210 s), 3.38, 5.0 and 3.43, all above the 2 times line of F-108; the thinnest margin is `unit.core.state.import` | none; the hosted run of Task 3 shows the real hosted seconds against it |
| W0-05 | info | `scripts/tests/test_phase09_qualification.py:962-1020` (T-10-49, recorded exception) | the ten closed Phase 9 claim blocks no longer compare `manifest_digest` with the live manifest file; they compare it with the pinned qualification-time digest plus a 39-row view. Re-checked above: the constants reproduce, no closed record or `F2_PINS` literal moved, authority basis and rejected alternative are recorded | recorded exception stands; the plan 10-19 summary and the claim documents must show it to the owner (it is not an owner approval of this comparator change) |

### Task 3: preflight, push and the hosted pair

Preflight on the clean committed tree (`sh scripts/preflight-phase10.sh`, head `33f6e9c`):
`[preflight] PASS`, total 67 s (oracle-authority 1, ctest-counts 0 with 17 rows and 0
mismatches, host-load 0 at load 6.53, head-ci 1, timeout-audit 0 with 97 TIMEOUTs and the lowest
ratio 2.9 on `unit.core.state.import` against its 210 s `COST`, pii 1 with `--tree` 0 findings and
`origin/main..HEAD` 48 commits 0 findings, linux-compile 37 with 220 units under the act image
cc 13.3.0, post-gate-replay 27 with 11 `ci.yml` steps). No host-load WARN.

Push (AUTONOMY section 4 routine push to the public repository, no force, no tag): the pinned
refspec `33f6e9ce8615f690ba14a7ed318324335b7a61a8:refs/heads/main` moved `origin/main` from
`612a3ad` to the head (fast-forward, 48 commits: 16 `docs(10-0x)`, 7 `test(10-0x)`, 6
`docs(10)`, 4 `feat(10-0x)`, 4 `docs`, 3 `fix(10-0x)`, 2 `docs(phase-10)`, one each of
`perf(10-0x)`, `ci(10-0x)`, `docs(state)`, `docs(resume)`, `chore(config)` and `chore(10)`). Before
the push: no binary or ROM-extension file in the range, no workstation path or e-mail address in
any added line, 17 changed files outside `.planning/` (all text), the PII scan clean on the tree
and the range. A first attempt of the same command failed locally on a shell expansion of the
refspec and published nothing.

Hosted pair: run `45311817943` (`ci.yml`, event `push`, head `33f6e9c`), status completed,
conclusion **success** on both jobs, Binutils cache hit on both with the same key as F-111
(`69d0bfd4...`), no cache deleted or changed.

| Measure | gate-macos | gate-linux | Reference (F-111, run 44795894664) |
|---------|------------|------------|------------------------------------|
| queue | 2 s | 3 s | 2 s, 1 s |
| run | 346 s | 229 s | 487 s, 203 s (F-114: 435 s, 305 s) |
| Gate step | 298 s | 181 s | 410 s, 159 s |
| ctest (`Total Test time`) | 184 s | 114 s | 251 s, 101 s (F-114: 215 s, 167 s) |
| audio qualification records | 28 s | 26 s | 42 s, 30 s |
| test-inventory-selftest, boot-evidence-selftest, build, core-lint, lint-selftest | 42, 21, 19, 19, 7 s | 9, 9, 20, 9, 15 s | 47, 35, 30, 26, 11 s (macOS) |
| `unit.core.state.import` start position and seconds | 2nd of 227, 162.57 s | 2nd of 227, 101.53 s | 66th without COST (local fresh-runner simulation, plan 10-04), 210.19 s (macOS) |
| the other three COST tests (start position, seconds) | state-field-use-audit 3rd 56.09 s, rsp-no-hle 4th 38.56 s, phase09-qualification 5th 21.56 s | 3rd 37.31 s, 4th 38.90 s, 5th 8.69 s | without COST in the same simulation 126th, 125th and 174th; 70.62 s, 48.34 s and 35.26 s (macOS) |

Reading (F-119 noise band): the macOS run of 346 s is 3 s below the lowest of the five earlier
samples (349 to 487 s), and the macOS ctest wall is 67 s below F-111 and 31 s below F-114. That
agrees with the F-111 estimate of up to about 40 s for the start order, but one sample cannot
be told apart from runner spread, so it is recorded as the GATE-01 warm after-measurement and
not as a proven saving. The Linux run of 229 s lies inside the earlier 189 to 305 s. The start
order itself is a measured fact on both runners (2nd to 5th). Recorded in
`docs/ci-reviews/LOG.md`: the Runs row for the run and finding F-120. The Wave 0 row of the
PIN-IMPACT.md "Gate-growth ledger" is added in the same commit. The cold measurement stays plan
10-19 and no hosted cache was deleted here.

