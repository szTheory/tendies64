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

## Wave 1

Plans 10-06 and 10-07 (phase-wave 1, VER-06: the VG-01 to VG-05 gap vectors and the standard-NTSC
VI clock vector). Reviewed by plan 10-08 (Task 1 to Task 3 below). Wave 1 is a test-only wave: the
claim under review is that it added tests, a mutant table and registrations, and moved no production
source and no oracle-bound file.

| Item | Value |
|------|-------|
| Wave 1 base | `f6ab8c26d008b8301b729b5aae2715e1b1079318` (extracted from PIN-IMPACT.md "Wave bases" with the 10-01 base extraction idiom: exactly one 40-hex line, an existing commit, an ancestor of the head) |
| Reviewed head | `37151ebea980b6fad54edcc493b25d60dc00d4bd` (15 commits after the base; 11 files changed, 5 of them outside `.planning/`: `core/CMakeLists.txt`, `core/tests/test_vg_gaps.c`, `scripts/check-test-inventory.sh`, `scripts/p10-mutants.py`, `scripts/preflight-phase10-regexes.txt`; 1775 insertions and 2 deletions outside `.planning/`) |
| Host | Darwin arm64, 1-minute load 7.1 to 11.9 during the scratch gate (other sessions on the same machine, so seconds are upper bounds for a quiet host) |

### Task 1: scratch gate of the Wave 1 head

Method: the W0-01 form, not a bare `git archive` tree. `git clone --no-checkout --no-hardlinks` of
the main repository into `build/10-review/w1/src`, `git checkout --detach 37151eb...`, and a copy (not
a symlink) of `build/tools/install` into the clone so the pinned Binutils 2.45 is reused. `git status
--short` of the clone was empty, and `git archive HEAD` of the clone and of the main repository have the
same SHA-256 (`5c9db0eb4cc5b7374774c6d3dd51fed8145e941659c03362d9a32db3b80d1825`), so the scratch tree
is exactly the archive of the head. Command: `sh scripts/gate.sh` inside the clone, default environment
(no `CI` or `GITHUB_*` variable). The shared tree was not written (`git status --short` of the main tree
was empty before and after).

Result: `[gate] PASS`, `[gate] total 123s`, exit 0, first attempt. Steps in seconds: sh-n 0,
lint-selftest 4, fixture-qualification 1, configure 0, build 6, boot-evidence-selftest 12,
test-inventory-selftest 20, test-inventory 0, ctest 71 (`100% tests passed out of 236`, `Total Test time
(real) = 70.33 sec`), core-lint 9. The CTest count is the 227 names of the Wave 0 head plus the nine
Phase 10 `unit.core.vg.*` names (five from plan 10-06, four from plan 10-07). The nine names ran in
0.00 to 0.02 s each at ctest resolution (`unit.core.vg.ntsc-clock` is the longest at 0.10 s in
`CTestCostData.txt`); the `phase10` label total is 0.80 s for 12 tests.

### Task 2: independent re-derivation, no-production-change proof, mutants and reading

`WAVE1_BASE` is `f6ab8c26d008b8301b729b5aae2715e1b1079318`, read from PIN-IMPACT.md with the base
extraction idiom (exactly one 40-hex line, an existing commit, an ancestor of the head). `HEAD` is the
head of the Task 1 commit (`bd37f85`, which adds only this file to the gated tree `37151eb`).

**Re-derivation.** A fresh calculator was written before the committed literals were read, from the
documents only (`docs/spec/vi.md` "Clock and boundary policy" and the v9 filter text,
`07-08-DEPTH-FIELDS.md` exponent table, `docs/spec/rdp.md` "V7 depth policy", the shade, perspective and
Bayer text). Its constants were then checked against the documents by program: the eight-row exponent
table equals the 07-08 table and the 4 x 4 Bayer matrix equals the matrix printed in `rdp.md`. A second
script extracted the literals from `core/tests/test_vg_gaps.c` and compared them with the derived values:
94 comparisons, 0 mismatches. Both scripts live under the ignored `build/10-review/w1/` and are not
committed. The sampled rows named by the plan, then the rest:

| Value | Document and method | Derived | Committed literal | Match |
|-------|---------------------|---------|-------------------|-------|
| NTSC first field deadline | vi.md: (3093+1) * 525 / 2 = 812,175 source cycles; ticks = 812,175 * 62,500,000 / 48,681,812 = 1,042,708.4657 exactly (`fractions.Fraction`); ceil (vi.md line 196 states the ceil rule) | 1,042,709 (floor would be 1,042,708) | `first_field_ticks` 1042709, byte literal `0x000fe915` | yes |
| NTSC first sampled line | 3094 cycles * 62,500,000 / 48,681,812 = 3972.2227, ceil | 3973 | `first_line_ticks` 3973 | yes |
| NTSC mutant clock 48,600,000 | same arithmetic, other clock | 1,044,464 and 3979 | comment values 1,044,464 and 3979 | yes (the mutant moves both deadlines, so it is reachable) |
| Z exponent boundary, exponent 0 | 07-08 table: shift 6, add 0; linear 0x3f and 0x40 and 0x1ffff | visible 0x0000, 0x0004, 0x1ffc; decoded 0x00000, 0x00040, 0x1ffc0 | `visible`, `decoded` arrays | yes |
| Z exponent boundary, exponent 3 | shift 3, add 0x38000; linear 0x38000 and 0x3bfff | visible 0x6000 and 0x7ffc; decoded 0x38000 and 0x3bff8 | same arrays | yes |
| Z exponent boundary, exponent 7 and the far word | shift 0, add 0x3f800; linear 0x3f800 and 0x3ffff | visible 0xe000 and 0xfffc; decoded 0x3f800 and 0x3ffff | same arrays; `decoded[16]` 0x3ffff | yes |
| all 17 linear, visible and decoded values | lowest and highest value of each of the eight ranges plus the lossy 0x3f; the rows tile 0 .. 0x3ffff (asserted) | 17 visible and 17 decoded words | 17 + 17 + 17 literals | yes (34 comparisons) |
| decal tolerance case | old primitive Z 0x2000, dZ 9: linear 0x10000, visible 0x1000, code 3, hidden 0x83, stored delta 8; new dZ 2: tolerance max(2, 8) = 8; raw 0x2001 and 0x1fff differ by 8 (pass), 0x2002 and 0x1ffe by 16 (fail) | tolerance 8; pass, pass, fail, pass, fail for raw 0x2000, 0x2001, 0x2002, 0x1fff, 0x1ffe | `vectors` (drawn, drawn, untouched, drawn, untouched), `expected_equality` 07 c1 10 00 83 | yes |
| primitive-Z saturation | min(raw, 0x7fff) << 3, all in exponent 7 | 0xffc0, 0xffe0, 0xffe0, 0xffe0 for raw 0x7ffe, 0x7fff, 0x8000, 0xffff | `vectors` | yes |
| delta ceiling, nine rows | plane evaluated with floor at pixel offsets 0x8000 and 0x18000 from base 0x00080000, ceil(sum / 8192) clamped to 65535, code = index of the top bit | visible 0x0004 (0x0000 for the sixth row, 0xffff for the clamp row) and hidden 0x81, 0x80, 0x80, 0x80, 0x81, 0x81, 0x82, 0x81, 0x83 | `vectors` (nine rows) | yes (18 comparisons) |
| RGBA5551 alpha clear | (v << 3) \| (v >> 2); alpha bit 0 to 0 or 255 | 0xf800 -> 255,0,0,0 | `expected` byte 3 = 0 | yes |
| serrate X, Y, blend | 0xc27d -> 198,74,247,255; 0x1f18 -> 24,231,99,0; 2^19 weights: floor of the means | 111,152,173,255; `source_bytes` 64 and 56 | `pixel_x`, `pixel_y`, `pixel_blend`, 64, 56 | yes |
| perspective floor | S = -131073, W = 131072: floor(S * 65536 / W) = -65537 (texel 6 after the mask-3 wrap), truncation -65536 (texel 7); texel word ((x+1) << 11) \| ((y+1) << 6) \| (((x+y) & 31) << 1) \| 1 | 0x384d, 0x404f, 0x09cd, 0x0a0f; exact rows 0x404f and 0x0a0f; operand split 0xfffd / 0xffff | `floor_word`, `truncated_word`, operand words | yes |
| shade floor | packet words W4 to W7 rebuilt from the bases and slopes; floor of Dx * 0.5 and Dx * 1.5 pixels | 0x3c17 and 0x3bd7 (truncation 0x4417 and 0x3c17) | `shade`, 0x3c17, 0x3bd7, 0x4417 | yes |
| Bayer | (c + bias) >> 3 with the printed matrix; the 128-value rule c + bias >= 80 gives 10, else 9 | pixel (2,1) bias 7: 0x5309; bias 3: 0x4ac9; 79 on every channel: 0x4a53 | `printed`, 0x5309, 0x4ac9, 0x4a53 | yes |

The derivation uses one assumption that is not a primary-document sentence: the texel coordinate of the
perspective vector is floor(Q16 quotient / 65536) taken modulo the mask-3 period 8 (the 10-07 summary
and the test comment state the same mapping). It reproduces both texel words and both truncation
words, so it is consistent, not independently proven by the tile code.

**Mechanical checks.**

| Check | Command or method | Result |
|-------|-------------------|--------|
| (a) no production source | `git diff --name-only WAVE1_BASE..HEAD` filtered for `core/*.c`, `core/*.h`, `core/include/`, `runner/*.c` and `runner/*.h` | empty (exit 1 of `grep`); the 11 changed files are 6 planning records and `core/CMakeLists.txt`, `core/tests/test_vg_gaps.c`, `scripts/check-test-inventory.sh`, `scripts/p10-mutants.py`, `scripts/preflight-phase10-regexes.txt` |
| (b) no oracle-bound or bound-CI file | the 13 `EXPECTED_CORPUS_PATHS` read from `scripts/check-rdp-oracle-authority.py` by `ast`, plus `fixtures/manifest.json`, `scripts/qualify-rdp-vi.py`, `docs/spec/rdp-vi-cases.json`, `docs/spec/phase09-cases.json`, `docs/spec/audio-cases.json`, `scripts/qualify-audio.py`, `scripts/qualify-phase09.py`, `docs/evidence/phase08-audio.md`, `.github/workflows/ci.yml`, `scripts/check-ci-yml.sh`, `scripts/gate.sh` | empty intersection with the 13 paths; the 11 other files unchanged |
| (c) digest sweep | `python3 -B scripts/digest-sweep.py --base WAVE1_BASE --head HEAD` | `sweep: 1599 shared lines, 0 differences, 0 head-only lines`, exit 0 (21 shipped ids, two trees built with the forced build identity, 39 s per tree sweep at 1-minute load 12 to 16) |
| (d) mutant anchors | `python3 -B scripts/p10-mutants.py --check-anchors` in the scratch clone | exit 0 |
| (e) mutants | `python3 -B scripts/p10-mutants.py --run` in the scratch clone (no mutant run in the shared tree) | exit 0, 40 s; table below |
| (f) measured commit counts | `git rev-list --count` over the `plan_head_before..plan_head_after` pairs of the 10-06 and 10-07 summaries | 4 and 4, equal to their `commits:` |

**Mutant table** (the `--run` classification of the Wave 1 head: baseline of 12 checks passed in the
throwaway worktree first; a kill needs a failed CHECK line of the new test file itself):

| Mutant | Expected | Class | Killed by |
|--------|----------|-------|-----------|
| control-equivalent-depth-cap | SURVIVED | SURVIVED | - |
| k1-predicate-widened | KILLED | KILLED | test_k1_refusal:apply-refusal |
| vg01-decal-strict | KILLED | KILLED | test_vg_gaps:decal-equality |
| vg01-zcap-7ffe | KILLED | KILLED | test_vg_gaps:primitive-z-saturation (the existing `test_rdp_depth` also fails, exit 1) |
| vg01-delta-floor | KILLED | KILLED | test_vg_gaps:delta-ceiling |
| vg-zexp-decode-top | KILLED | KILLED | test_vg_gaps:z-exponent-boundaries (the existing `test_rdp_depth` exits 0, so only the new case sees it) |
| vg-zexp-compress-round | KILLED | KILLED | test_vg_gaps:z-exponent-boundaries (`test_rdp_depth` exit 1) |
| vi-clock-48600000 | KILLED | KILLED | test_vg_gaps:ntsc-clock |
| vg03-alpha255-v9 | KILLED | KILLED | test_vg_gaps:rgba5551-alpha-clear |
| vg03-alpha255-v8 | KILLED | KILLED | test_vg_gaps:rgba5551-alpha-clear |
| vg02-parity-ignored | KILLED | KILLED | test_vg_gaps:serrate-filter-parity |
| vg02-next-row | KILLED | KILLED | test_vg_gaps:serrate-filter-parity |
| vg04-floor-off-s | KILLED | KILLED | test_vg_gaps:perspective-floor |
| vg04-floor-off-t | KILLED | KILLED | test_vg_gaps:perspective-floor |
| vg05-shade-trunc | KILLED | KILLED | test_vg_gaps:shade-floor-bayer |
| vg05-bayer-shift2 | KILLED | KILLED | test_vg_gaps:shade-floor-bayer |
| control-no-compile | INVALID | INVALID | - |

That is 15 KILLED (the plan's 13: K1 plus 12 VG and VI rows, and the two exponent rows that plan 10-06
added), the SURVIVED control and the INVALID control, each as expected; no SURVIVED or INVALID mutant.

**Reading of each new case for a reachable comparator.** Every case in `core/tests/test_vg_gaps.c` was
read. Each has a hand derivation above it, literals typed as hex, a same-setup wrong-value control and
at least one killed mutant. Each control asserts the byte index where its wrong copy first differs from
the observed bytes, and all nine cases pass in the gate, so every control does diverge at its intended
byte (decal-equality byte 3, primitive-z-saturation byte 1, delta-ceiling byte 2, z-exponent-boundaries
byte 0, ntsc-clock byte 7, rgba5551-alpha-clear byte 3 on both paths, serrate-filter-parity bytes 0, 0 and
21, perspective-floor byte 0 on the two inexact rows and byte 1 on all four, the two exact-quotient
rows having equal floor and truncation words by design, shade-floor-bayer bytes 0, 0 and 1 for the
shade words and 0 and 1 for the Bayer words). Every mutant sits on the production line that its vector
reaches, and its kill is a failed CHECK of that case after a green baseline, so a green process never
stands for a reached comparator. No case has an unreachable comparator.

**Sibling copies of the perspective floor (assessment of the 10-07 observation).** Plan 10-07 named
three copies of the floor text that no VG mutant pins: the S and T corrections of
`t64_rdp_tmem_triangle_pixel_rgba8` (`core/t64_rdp_tmem.c:1218-1221`, combiner profiles v5 and later),
the S and T corrections of `project_texture_q16` (`core/t64_rdp_raster.c:556-557`, the LOD path) and the
`from_major` floor of `texture_channel_q16` (`core/t64_rdp_raster.c:495`). The texts equal the pinned v4
lines (`core/t64_rdp_tmem.c:1147-1150`, the `from_major` of `shade_channel` at `:472`) apart from the
variable names. To classify them with a measurement instead of an opinion, five throwaway mutants (S and
T of the rgba8 copy, S and T of the projection, the `from_major` truncation) were applied one at a time in
the scratch clone (the correction replaced by `(void)remainder;` as `scripts/p10-mutants.py` does, so the
build stays warning-clean; the truncation as plain division, as `vg05-shade-trunc` does), the
gate preset rebuilt and the whole `ctest --preset gate` run, then reverted (`git status --short` clean
afterwards, scratch tree rebuilt). In all five the only failing test was `repo.rsp-no-hle`, whose message
is "executable body changed; fetched-path review is required" (the sealed-body guard that fires on any
text edit of a sealed function). All 235 other tests passed, so no behavioral test kills a change of
those copies. They are residual, not defects: the code is correct against `rdp.md` and equal to the
pinned copies, and any edit of them fails closed at the seal.

### Findings

Columns: id, severity (blocking, warning, info), file and line, finding, disposition. No blocking
finding was found and no warning was found.

| id | severity | file:line | finding | disposition |
|----|----------|-----------|---------|-------------|
| W1-01 | info | `core/t64_rdp_tmem.c:1218-1221`, `core/t64_rdp_raster.c:495` and `:556-557` | the three sibling copies of the floor text are not pinned behaviorally by any test (five scratch mutants, whole gate ctest each: only the `repo.rsp-no-hle` body seal fails). VER-06's named gaps VG-01 to VG-05 and the NTSC vector are closed; this is the same class of gap on code paths the gaps did not name | residual, deferred item DEF-W1-01: a small follow-up vector (a combiner-profile v5 perspective row through the rgba8 path, an LOD row through `project_texture_q16`, a negative-gradient texture row for `texture_channel_q16`) can be added as a new numbered plan if its cost is worth it before the freeze; it is not required by VER-06 and widens no scope here |
| W1-02 | info | `core/tests/test_vg_gaps.c` (all wrong-value controls) | a same-setup control compares the observed bytes with a byte-edited copy of the committed literal and asserts the divergence index; it shows that the comparator reports the intended byte, not that production would behave differently. The discriminating evidence is the killed mutants, whose kill needs a failed CHECK line after a passing baseline | none; same reading as W0-03 (the kill column names the last failing kill check, and every Wave 1 row has exactly one) |
| W1-03 | info | `core/tests/test_vg_gaps.c:730-744` (comment) and `docs/spec/vi.md:196` | the NTSC comment cites "Clock and boundary policy" for the rounding ("first whole tick at or after the exact phase"), but that section does not state the rounding; the ceil rule is printed in the tracer example of `vi.md` (`ceil(15 x 62,500,000 / 48,681,812) = 20`) and the v8 profile inherits the clock. 1,042,709 needs the ceil (floor gives 1,042,708) | none for the vector; one sentence stating the ceil in the v8 clock section can go into the Phase 10 documentation sweep |
| W1-04 | info | `core/tests/test_vg_gaps.c` delta-ceiling rows 8191 and 0 | the depth word stores only the priority code, so 8191, 8192 and 0 store the same code 0 and are not independent edges; the effective edges are 8192 (kills `+ 8192`) and 8193 (kills a floor or `+ 8190`). Recorded by plan 10-06 as a note | none; accepted |
| W1-05 | info | plan 10-08 text | the plan counts 13 mutants; the table has 15 KILLED because plan 10-06 added two rows (deviation 2 of 10-07 records the count) | none |

Deferred item DEF-W1-01 (from W1-01): behavioral pins for the three sibling copies of the perspective
and major-edge floor. Not required by VER-06, not a defect, no blocking effect on Wave 2. If taken, it is
a new numbered plan with its own mutant rows; otherwise it stays recorded for the Phase 10 close-out.

### Task 3: preflight, push and the hosted pair

Preflight on the clean committed tree (`sh scripts/preflight-phase10.sh`, head `a0c0a38`): `[preflight]
PASS`, total 63 s (oracle-authority 0 s; ctest-counts 24 regex rows, 0 mismatches, 12 tests carry the
label `phase10` as the `PHASE10_TESTS` block says; host-load 0 s at load 7.67; head-ci 1 s, `origin/main`
`33f6e9c` completed with success; timeout-audit 1 s, 97 TIMEOUTs, 0 too low, 0 unmeasured, lowest ratio
2.9 on `unit.core.state.import`; pii 0 s with the tree and the 18-commit range at 0 findings; linux-compile
35 s with 221 units under the act image cc 13.3.0, because `core/CMakeLists.txt` changed since the
preflight base; post-gate-replay 26 s with 11 `ci.yml` steps). No host-load WARN.

Push (AUTONOMY section 4 routine push to the public repository, no force, no tag): the pinned refspec
`a0c0a388d08d34fde358609e432b40c5c8ca9377:refs/heads/main` moved `origin/main` from `33f6e9c` to
`a0c0a38` (fast-forward, 18 commits: 4 `docs(10-05)`, 4 `test(10-06)`, 2 `docs(10-06)`, 4 `test(10-07)`, 2
`docs(10-07)` and 2 `docs(10-08)`). Before the push: no binary or ROM-extension file in the range, no
workstation path or e-mail address in any added line, 13 changed files (5 outside `.planning/` and
`docs/`: all text), a single commit author (the public no-reply identity), the PII scan clean on the
tree and the range, and no protected material (the new test is authored, its literals are hand
derivations, no oracle or ROM content).

Hosted pair: run `45321391540` (`ci.yml`, event `push`, head `a0c0a38`), status completed, conclusion
**success** on both jobs, Binutils cache hit on both with the key of F-111 and F-120 (`69d0bfd4...`), no
cache deleted or changed. The run was found by `headSha` equal to `git rev-parse origin/main` and waited
for with `gh run watch` (one wait, no retry needed).

| Measure | gate-macos | gate-linux | Wave 0 reference (F-120, run 45311817943) |
|---------|------------|------------|-------------------------------------------|
| queue | 2 s | 3 s | 2 s, 3 s |
| run | 326 s | 230 s | 346 s, 229 s |
| Gate step | 288 s | 181 s | 298 s, 181 s |
| ctest (`Total Test time`), 236 of 236 | 187 s | 115 s | 184 s, 114 s (227 names) |
| test-inventory-selftest | 38 s | 8 s | 42 s, 9 s |
| audio qualification records | 21 s | 28 s | 28 s, 26 s |
| `unit.core.state.import` start position and seconds | 2nd of 236, 172.55 s | 2nd of 236, 103.11 s | 2nd of 227, 162.57 s and 101.53 s |
| the other three COST tests (start position, seconds) | state-field-use-audit 3rd 60.91 s, rsp-no-hle 4th 43.49 s, phase09-qualification 5th 26.83 s | 3rd 37.32 s, 4th 40.52 s, 5th 9.29 s | 3rd 56.09 s, 4th 38.56 s, 5th 21.56 s (macOS) |
| the nine `unit.core.vg.*` names, summed | 0.29 s | 0.51 s | not present |

Reading (F-119 noise band): the macOS ctest wall is 3 s above the Wave 0 row and the macOS run 20 s
below it; both deltas are inside the 52 s spread of F-119, so the nine new names are recorded as no
visible hosted cost, not as growth and not as a saving. The macOS run of 326 s is the lowest of seven
samples (326 to 487 s); one more sample does not change the F-120 reading that the COST start order
cannot be told apart from runner spread. The Linux run of 230 s lies inside the earlier 189 to 305 s.
Recorded in `docs/ci-reviews/LOG.md`: the Runs row for the run and finding F-121. The Wave 1 row of the
PIN-IMPACT.md "Gate-growth ledger" is added in the same commit. The cold measurement stays plan 10-19 and
no hosted cache was deleted here.

Plan 10-08 reviewed 10-06 and 10-07 without a blocking finding: the Wave 1 head passes the gate in
isolation, 94 literal comparisons match a calculator written from the documents, no production or
oracle-bound file moved, the sweep reports 0 differences, 15 mutants are KILLED with both controls as
expected, and the pushed head's hosted pair is green. Wave 2 (plans 10-09 to 10-13, VID-07 and CORE-06)
may start at the Wave 2 base recorded in PIN-IMPACT.md "Wave bases".

## Wave 2

Plans 10-09 to 10-12 (phase-wave 2: the one identity and state bump of Phase 10, VID-07 and CORE-06): the
four si-v2 ids (descriptor 13, machine model 21, state 26, digest domain `t64.state.si.26`), RDP depth on
that family, the state 26 export with container version word 2, and the state 26 importer. This is the
highest-risk wave (Phase 9's blocking digest-completeness defect surfaced only in an independent review),
so the review re-derives and re-measures instead of reading the plan summaries. Reviewed by plan 10-13
(Task 1 and Task 2 below).

| Item | Value |
|------|-------|
| Wave 2 base | `6dd785986f6aa78c6da5573a558b15b386a4edee` (extracted from PIN-IMPACT.md "Wave bases" with the 10-01 base extraction idiom: exactly one 40-hex line, an existing commit, an ancestor of the head) |
| Wave 0 base | `4ac633e880e0c26005cb03092e8d8485e721034c` (same idiom, for the whole-phase sweep) |
| Reviewed head | `96c8711188a287756393c1871e7dda1e0a73f4af` (30 commits after the Wave 2 base; 24 files changed outside `.planning/`, 7,235 insertions and 132 deletions; the five production sources that changed are `core/t64_digest.c`, `core/t64_internal.h`, `core/t64_isviewer.c`, `core/t64_machine.c` and `core/t64_state.c`) |
| Host | Darwin arm64, 1-minute load 12 to 15 during every run below (other sessions on the same machine, so every second count is an upper bound for a quiet host) |

### Task 1: scratch gate and sanitizer selection of the Wave 2 head

Method: the W0-01 form. `git clone --no-checkout --no-hardlinks` of the main repository into
`build/10-review/w2/src`, `git checkout --detach 96c8711...`, and a copy (not a symlink) of
`build/tools/install` into the clone so the pinned Binutils 2.45 is reused. `git status --short` of the
clone was empty, and `git archive HEAD` of the clone and of the main repository have the same SHA-256
(`034384e90eb56ed01eca0dbf3cf18fa79d5a2041331c7dddd956ca7120b01685`), so the scratch tree is exactly the
archive of the head (the equality was checked again after the last scratch edit was reverted). Every
command below ran in that clone with no `CI` or `GITHUB_*` variable; the shared tree was not written
(`git status --short` of the main tree was empty before and after).

**Gate.** `sh scripts/gate.sh`: `[gate] PASS`, `[gate] total 159s`, exit 0, first attempt. Steps in
seconds: sh-n 0, lint-selftest 6, fixture-qualification 1, configure 1, build 7, boot-evidence-selftest 14,
test-inventory-selftest 21, test-inventory 1, ctest 98 (`100% tests passed out of 248`, `Total Test time
(real) = 97.74 sec`), core-lint 10. The CTest count is the 236 names of the Wave 1 head plus the 12 Wave 2
names (3 `unit.core.profile.v26-*`, 2 `unit.core.digest.v26-*`, 3 `unit.core.rdp.depth-composite-*`, 2
`unit.core.state.export-v26*`, 2 `unit.core.state.import-v26-*`). The gate total is 36 s above the Wave 1
review (123 s) and the ctest step 27 s above it (71 s). The 12 new names add 15.13 s of native test time in
that run (`CTestCostData.txt` of the scratch gate, one run each, none carries `COST`): `import-v26-hostile`
8.63, `export-v26` 2.20, `import-v26-roundtrip` 2.20, `export-v26-bounds` 1.57, `digest.v26-completeness`
0.40, `digest.v26-strip-differential` 0.05, `depth-composite-sidecar` 0.03 and the other five 0.006 to 0.016
each. The 1-minute load of 12 to 15 against 7 to 12 at Wave 1 accounts for the rest of the 27 s, so every
local figure is an upper bound and the hosted pair is the measurement that counts (Task 3).

**Sanitizer selection.** `cmake --preset asan`, `cmake --build --preset asan` (357 build steps, exit 0) and
`ctest --preset asan -L phase10 --output-on-failure`: exit 0, `100% tests passed out of 24`, `Total Test
time (real) = 155.17 sec` (`phase10 = 280.49 sec*proc`). `ctest -N -L phase10` reports 24 tests and the
`PHASE10_TESTS` block of `scripts/check-test-inventory.sh` lists 24 names, so the run count equals the
block. No sanitizer report: 0 lines matching `ERROR: AddressSanitizer` or `runtime error` in the log. The
run overlapped one digest sweep (two trees), so each second is an upper bound.

| Phase 10 name (ASan preset) | seconds | | Phase 10 name (ASan preset) | seconds |
|-----------------------------|---------|-|-----------------------------|---------|
| `repo.p10-mutant-anchors` | 0.08 | | `unit.core.vg.decal-equality` | 0.16 |
| `unit.core.k1.dispatch-refusal` | 5.42 | | `unit.core.vg.primitive-z-saturation` | 0.14 |
| `unit.core.k1.apply-refusal` | 10.56 | | `unit.core.vg.delta-ceiling` | 0.14 |
| `unit.core.profile.v26-identity` | 0.30 | | `unit.core.vg.z-exponent-boundaries` | 0.58 |
| `unit.core.profile.v26-isviewer` | 0.14 | | `unit.core.vg.ntsc-clock` | 3.95 |
| `unit.core.profile.v26-predicate-matrix` | 0.16 | | `unit.core.vg.rgba5551-alpha-clear` | 0.13 |
| `unit.core.digest.v26-completeness` | 5.80 | | `unit.core.vg.serrate-filter-parity` | 0.12 |
| `unit.core.digest.v26-strip-differential` | 0.86 | | `unit.core.vg.perspective-floor` | 0.13 |
| `unit.core.rdp.depth-composite-accepted` | 0.15 | | `unit.core.vg.shade-floor-bayer` | 0.12 |
| `unit.core.rdp.depth-composite-wrong-depth` | 0.22 | | `unit.core.state.export-v26` | 34.36 |
| `unit.core.rdp.depth-composite-sidecar` | 0.60 | | `unit.core.state.export-v26-bounds` | 23.79 |
| `unit.core.state.import-v26-roundtrip` | 47.97 | | `unit.core.state.import-v26-hostile` | 144.60 |

The slowest sanitizer name is `unit.core.state.import-v26-hostile` at 144.60 s against its sanitizer TIMEOUT of
900 s (6.2 times); `import-v26-roundtrip` 47.97 s against 600 s (12.5 times); `export-v26` and `export-v26-bounds`
34.36 and 23.79 s against their 300 s (8.7 and 12.6 times). All four are above the 2 times line of F-108.

### Task 2: whole-bump byte identity, completeness audit and single-bump check

`WAVE0_BASE` and `WAVE2_BASE` are the two 40-hex literals above, read from PIN-IMPACT.md with the base
extraction idiom (`sed` on the exact label, `grep -cEx '[0-9a-f]{40}'` equal to 1, `git cat-file -e`, an
ancestor of the head; never `HEAD`). `HEAD` is the head of the plan's Task 1 commit (`f32a01f`, which adds only
this file to the gated tree `96c8711`). Every command ran in the scratch clone of Task 1.

**(a) Digest sweep.** `python3 -B scripts/digest-sweep.py` (two trees built with the forced build identity):

| Run | Result | Seconds |
|-----|--------|---------|
| `--base WAVE0_BASE --head HEAD` (the plan's command) | `sweep: 1599 shared lines, 0 differences, 0 head-only lines`, exit 0 | 51 |
| the same plus four `--head-extra-id` (the si-v2 ids) | `sweep: 1599 shared lines, 0 differences, 296 head-only lines`, exit 0 | 52 |
| `--base WAVE2_BASE --head HEAD` plus the four `--head-extra-id` | `sweep: 1599 shared lines, 0 differences, 296 head-only lines`, exit 0 | 72 |

Reading: over the 21 shipped ids, 1599 lines (1460 digest lines, 115 stop lines, 20 skip lines and the four
si-v1 export lines) are byte identical from the Wave 0 base to the head, so the whole of Phase 10 so far
(Waves 0 to 2) is invisible to every shipped id, the v25 export lines included. The W0-02 gap (an extra id that
admits no ROM would pass silently) was closed by reading the head-only lines instead of the count:
`comm -13` of the sorted base and head `sweep.txt` gives, for each of the four si-v2 ids, 64 digest lines and 1
export line, and over the four ids 32 stop lines and 4 skip lines (8 stop and 1 skip each, as for the si-v1
ids), 4 x 65 + 36 = 296; `comm -23` (lines only in the base) is empty. K1 and the F2 pins:
`ctest -R '^(unit\.core\.k1\..*|harness\.phase09-qualification|repo\.si-membership-sites)$'` gives 4 of 4
passed (`unit.core.k1.dispatch-refusal`, `unit.core.k1.apply-refusal`, `harness.phase09-qualification`,
`repo.si-membership-sites`).

**(b) Sealed bodies and bound files.** `python3 -B scripts/check-rsp-no-hle.py --build build/gate`: `PASS:
rsp-no-hle sources_scanned=85`, exit 0, no re-pin. The ten sealed-body source files
(`core/t64_bus.c`, `t64_si.c`, `t64_ai.c`, `t64_pi.c`, `t64_save.c`, `t64_rdp.c`, `t64_rdp_depth.c`,
`t64_rdp_pipeline.c`, `t64_rdp_raster.c`, `t64_rdp_tmem.c`) have no diff against `WAVE2_BASE`, and neither does
the public header directory `core/include`. The 13 `EXPECTED_CORPUS_PATHS` of `scripts/check-rdp-oracle-authority.py`
(read by `ast`) have an empty intersection with the diff from `WAVE0_BASE`; `fixtures/manifest.json`,
`docs/spec/phase09-cases.json`, `docs/spec/audio-cases.json`, `scripts/qualify-audio.py`,
`scripts/qualify-phase09.py`, `scripts/qualify-rdp-vi.py`, `docs/spec/rdp-vi-cases.json`, `scripts/gate.sh`,
`.github/workflows/ci.yml`, `scripts/check-ci-yml.sh`, `scripts/list-membership-sites.py`,
`fixtures/state/decode_container.py`, `core/tests/test_state_import.c`, `core/tests/test_state_export.c`,
`core/tests/state_import_helpers.h` and `docs/spec/state-mutants.txt` have no diff against `WAVE2_BASE` (the
last five are the frozen Phase 9 state files that the 10-11 and 10-12 plans had to leave alone; of the files
in this list only the two CI files differ from `WAVE0_BASE`, by the single Wave 0 commit `4ca1054`). The 24
files that changed outside `.planning/` are 6 in `core/`, 6 in `core/tests/`, 7 in `docs/spec/`, 3 in
`scripts/`, 1 in `fixtures/state/` (the new `decode_container_v2.py`) and 1 in `tools/digest-sweep/`.

**(c) Completeness audit against the struct definitions.** Done by program over the committed sources, then by
a byte-flip probe (probe sources are kept outside the repository, not committed):

| Structure | Method | Result |
|-----------|--------|--------|
| `t64_rdp_state` (100 members) | the members of the struct in `core/t64_internal.h` read by program and compared with the `M`, `MX` and `MI` rows of `rdp_members` in `core/tests/test_digest_v26.c`; `MTILE` fields compared with `t64_rdp_tile` | 0 members without a row, 0 rows naming no member; 3 `MX` exclusions (`triangle_last_coverage_x`, `_y` and `_mask`, test observation state written by the row handler); `convert_coefficients` 0 to 5, `pending_triangle_words` 0 to 3 (the four words the count of 4 makes state; the sweep flips the rest as inert), `key_center`, `key_scale` and `key_width` 0 to 2 as rows; the 19 `MTILE` fields equal the 19 tile fields for all eight tiles; the five depth members (`depth_image_address`, `depth_image_valid`, `primitive_z`, `primitive_dz`, `primitive_depth_valid`) are plain `M` rows, not exclusions; `_Static_assert(sizeof(t64_rdp_state) == 5144u)` holds in the gate (so a new member cannot enter unnoticed) |
| memory coverage table, hidden depth table | read of the sweep | first, a middle and the last byte of each, flipped, must change the stream |
| `t64_vi_state` (1,208 bytes) | the test file has no VI rows, so the plan's reading of "the VI state" was done by a probe: every byte of `machine->vi` flipped one bit at a time on the four si-v2 ids and on `si-v1`, the stream compared with its baseline | 417 inert bytes on every id, the same set on all five: `packet_head` and `packet_count` (8 + 8, the documented host observer cursors), the 64-byte `profile_id` and 128-byte `build_id` of each of the two packet slots (384, the identity strings that the v25 framing does not hash, bound by the identity section and the header instead) and 17 bytes of padding; every other byte changes the stream. The Wave 2 diff of `t64_digest.c` adds, removes and reorders no VI member: its two VI-adjacent hunks only rename the framing predicate (`has_v25_state` to `has_v25_framing`, true for both families) at the section 8 marker and at the packet identity strings, and `has_v22_state || has_v23_state` is true for both families, so v26 equals v25 here by construction |
| the depth effect itself | `core/t64_rdp_depth.c` read for every write | `t64_rdp_depth_commit_pixel` writes only RDRAM (`machine->ram`, the visible word) and `rdp_depth_hidden`; both are in the stream |

**(d) One v26 idle size by hand.** From `docs/spec/state-container.md` only (not from the test): total length is
`0x120 + 56 x section count + canonical length + 40 + 32`; the canonical length is 15 domain bytes plus 4 + body
for each canonical section. The idle stem state 25 bodies are 168, 32, 4,194,308, 8,200, 276, 56, 43,724,
4,200,260, 336 and 404 for ids 1 to 10 and 11 table entries: 288 + 616 + (15 + 40 + 8,447,764) + 72 =
8,448,795. State 26 grows section 8 only, by 20 bytes of scalars, 4 bytes of hidden length and 4,194,304 / 2 =
2,097,152 hidden bytes, 2,097,176 in all, so the idle stem is 8,448,795 + 2,097,176 = **10,545,971**, equal to the
literal `10545971u` of `core/tests/test_state_export_v26.c` and to the document. The same calculation for the
save ids (section 11 of 532, 2,068 and 32,788 bytes plus its id and table entry) gives 10,546,563, 10,548,099 and
10,578,819, equal to the other three literals.

**(e) Single bump.** `git diff WAVE2_BASE HEAD` on the production sources (added lines only):

| Item | Added | Result |
|------|-------|--------|
| profile tuple | `.descriptor_version = 13u`, `.machine_model_version = 21u`, `.state_serialization_version = 26u` once in `T64_SI_V2_FAMILY_COMMON_FIELDS`, and the matching exact predicate `t64_has_si_v26_profile` | exactly one new tuple, 13 / 21 / 26; every other added version literal is a re-indented existing one (16, 17, 21, 22 in `t64_has_rdp_depth_profile`) or the `13u` arm of an existing descriptor-12 list |
| digest domain | `domain_v26[] = "t64.state.si.26"` in `core/t64_digest.c` and `STATE_DOMAIN_V26` in `core/t64_state.c` | exactly one new domain text; the 17 earlier texts and `t64.state.si.25` are unchanged |
| container version word | `git diff WAVE2_BASE HEAD -- core/t64_internal.h`, one added line: `#define T64_STATE_CONTAINER_VERSION_V2 UINT32_C(2)` (line 1904); no added or removed line changes `#define T64_STATE_CONTAINER_VERSION UINT32_C(1)` (line 1899); `state_container_version_for` (`core/t64_state.c:122`) maps only state 26 to the V2 constant | exactly one new container value, 2; v1 unchanged |
| format string | `T64_STATE_CONTAINER_FORMAT` | still `t64.state-container.v1` (`core/include/tendies64/t64_machine.h:410`, no diff): the v2 format string and its pairing are plan 10-15 |

No second family, version or domain was introduced. The shipped sweep above shows the state 25 export of the
four si-v1 ids is byte identical (their export lines are shared lines with 0 differences).

**Version pairing at the reader (T-10-52).** Committed evidence: the frozen v25 hostile row `container version`
(value 2 at 0x008 of a state 25 blob, `core/tests/test_state_import.c:879`) still refuses with
`CONTAINER_VERSION` (`unit.core.state.import` passes in the gate); `unit.core.state.export-v26` refuses (2, 26)
rewritten to (1, 26) and to (2, 25), and (1, 25) rewritten to (2, 25) and to (1, 26), at 0x008 on all four ids,
with the machine unchanged; the mutant `v26-reader-pairing-dropped` is killed. Independent probe: for the two
si-v1 ids and the four si-v2 ids, an exported blob had its version word set to each of 0, 1, 2, 3 and
0xffffffff and its state word to each of 24, 25, 26, 27, 0 and 0xffffffff (30 combinations per id, header
patched without re-signing) and was imported into a fresh machine of the same id: 180 cases, the 6 unmodified
pairs ((1, 25) on state 25 ids, (2, 26) on state 26 ids) imported, the other 174 were refused, the 144 whose
version word differs from the word that belongs to the state word were refused with reason `CONTAINER_VERSION`
at position 8, and the digest of the machine was unchanged by every refused import.

**Importer reading.** `git diff WAVE2_BASE HEAD -- core/t64_state.c`, written for the importer half: every new
store goes through `STATE_STORE` or `state_rd_bytes`, which copy only in apply mode; the five scalars and the
hidden length are read and checked in the validate pass (`ADDR_RANGE`, `BOOL_01`, 16-bit `INDEX_LT`,
`PROFILE_DERIVED`, per-byte `ENUM_CLOSED` only when `mode == VALIDATE`); the commit zeroes the structures first and
then applies the sections, and `state_commit` writes `depth_image_address = 0` only for a machine that is not state
26. No apply-before-validate path exists. The domain text is a function of the machine (`state_domain_for`) and
both texts are 15 bytes, so the digest recomputation and the domain check cannot read past the same
`STATE_DOMAIN_BYTES`. The field-use audit (`scripts/check-state-field-use.py --root . --check`) gives
`findings=0` and lists exactly `depth_image_address` and `rdp_depth_hidden` among the new members, which have the
rows `ADDR_RANGE` and `ENUM_CLOSED`; the other four scalars have no audit row (the audit lists only members its
scan sees stored or looped), carry their rule in the reader (`F32_U16`, `state_rd_bool`), appear in the table of
`docs/spec/state-container.md` and have their own hostile rows in `docs/spec/state-mutants-v26.txt`
(`v26-depth-valid-two`, `v26-primitive-z-beyond-16-bits`, `v26-primitive-dz-beyond-16-bits`,
`v26-primitive-valid-two`).

**Design decision D10 (the written-out Z product bound).** Assessed against the sealed
`t64_rdp_depth_triangle_preflight` (`core/t64_rdp_depth.c:83`): `state_z_products_bounded`
(`core/t64_state.c:1490`) is the same arithmetic line by line (the same `yh16`, `yl16`, `xh16`, `dxh`, `y_bound`,
`edge_bound`, `x_bound`, the same quarter of `INT64_MAX` limit and the same three division tests). Two
differences were read and found harmless: the copy does not return early for `x0 >= x1` or `y_begin > y_end`, but
the importer reaches the copy only after `t64_rdp_triangle_extent`, which returns `first > last` for an empty
span (refused by the proof's `first <= last` check) and refuses `x0 > x1`, so the copy runs only for `x0 < x1`
and `first <= last`; and the live checks on the depth image (valid, aligned to 8, primitive depth, extent inside
RDRAM) are the extent function's own checks on the blob values. Independent differential, with the function
text extracted verbatim from the committed source and called beside the live preflight on a machine of the
state 26 stem id: 6,000,000 packets over the four Z opcodes (3,694,534 accepted, 2,305,466 refused, field
values from a table of edge and power-of-two cases mixed with uniform values), 0 mismatches; and for 4,000 random
edge and scissor setups the largest accepted magnitude of each of `dzdx`, `dzde` and `dzdy` found by bisection
against the live preflight, the copy compared at that value, one above it and the negative edge: 22,140 comparisons,
0 mismatches. Sanity of the probe: with the limit divisor 2 instead of 4 it reports 308,462 random and 10,140
boundary mismatches, with the `dy` divisor `y_bound + 1` it reports 2 random mismatches, and with `abs_de >=`
in place of `>` it reports 476 random and 2,338 boundary mismatches, so it can fail. Sealed bodies: no sealed
body text moved (check (b)). The mutant `v26-import-z-products-unchecked` is KILLED by `import-v26-hostile`.
Verdict: the design is accepted (no refactor of a sealed body, the copy equals the live function on every
packet tried), with the pinning weakness of finding W2-01.

**(f) Mutant classification.** `python3 -B scripts/p10-mutants.py --check-anchors` exit 0, then `--run` in the
scratch clone (the tree is committed and clean, no mutant in the shared tree): exit 0, 117 s together; baseline of
17 checks in the throwaway worktree passed first; a kill needs a failed CHECK line of the new test itself.

| Mutant | Expected | Class | Killed by |
|--------|----------|-------|-----------|
| control-equivalent-depth-cap | SURVIVED | SURVIVED | - |
| k1-predicate-widened | KILLED | KILLED | test_k1_refusal:apply-refusal |
| vg01-decal-strict, vg01-zcap-7ffe, vg01-delta-floor, vg-zexp-decode-top, vg-zexp-compress-round | KILLED | KILLED | test_vg_gaps (decal-equality, primitive-z-saturation, delta-ceiling, z-exponent-boundaries twice) |
| vi-clock-48600000, vg03-alpha255-v9, vg03-alpha255-v8, vg02-parity-ignored, vg02-next-row, vg04-floor-off-s, vg04-floor-off-t, vg05-shade-trunc, vg05-bayer-shift2 | KILLED | KILLED | test_vg_gaps (ntsc-clock, rgba5551-alpha-clear twice, serrate-filter-parity twice, perspective-floor twice, shade-floor-bayer twice) |
| v26-sidecar-invalidation-removed | KILLED | KILLED | test_rdp_depth_composite:depth-composite-sidecar |
| v26-digest-omit-scalar, v26-digest-omit-hidden | KILLED | KILLED | test_digest_v26:v26-completeness |
| v26-export-omit-hidden, v26-export-version-word-1, v26-reader-pairing-dropped | KILLED | KILLED | test_state_export_v26:export-v26-idle |
| v26-import-skip-hidden, v26-import-depth-after-proof | KILLED | KILLED | test_state_import_v26:roundtrip |
| v26-import-addr-unchecked, v26-import-z-products-unchecked | KILLED | KILLED | test_state_import_v26:hostile |
| control-no-compile | INVALID | INVALID | - |

25 KILLED, the SURVIVED control and the INVALID control, each as expected; no SURVIVED or INVALID mutant
among the 25. To test the pin strength of D10, two probe mutants were added to the table of the scratch clone
(a scratch commit on a throwaway branch, removed again; the clone is back at the head and its archive hash is
unchanged): the product limit divisor 2 instead of 4, and the `dy` product test against `y_bound + 1`. Both
are SURVIVED (the hostile and roundtrip cases both pass with either), which is finding W2-01.

### Findings

Columns: id, severity (blocking, warning, info), file and line, finding, disposition. No blocking finding was
found.

| id | severity | file:line | finding | disposition |
|----|----------|-----------|---------|-------------|
| W2-01 | warning | `core/t64_state.c:1490` (`state_z_products_bounded`) and `core/tests/test_state_import_v26.c:1295` (`check_preflight_parity`) | the written-out copy equals the live preflight today (D10, 6,000,000-packet differential, 22,140 boundary comparisons), but nothing committed pins its constants: the parity case compares two packets (a zero Z plane accepted and a saturated one refused) and the one mutant removes the whole check. A limit of `INT64_MAX / 2`, a divisor `y_bound + 1` or the same drift in a later edit of either function passes every committed test (two probe mutants SURVIVED under the project's own driver), and a copy that is more permissive than the preflight lets a crafted packet reach the signed 64-bit products of `t64_rdp_depth_evaluate_pixel` | D10 accepted. A new numbered fix plan should add a boundary parity (bisection of each derivative against the live preflight on a few edge setups, which costs well under a second) and the two mutants as table rows; it widens no scope and moves no sealed body. Not blocking: the code is correct now and both functions are sealed or covered by the hostile row |
| W2-02 | warning | `scripts/list-membership-sites.py:28-44` (`MEMBERSHIP_TOKENS`) and `docs/spec/profile-membership-sites.txt` | the two exact-tuple predicates `t64_has_si_v25_profile` and `t64_has_si_v26_profile` are not tokens, so the sites that now consult only them left the list: measured by adding both tokens to a copy of the generator, the list would gain `core/t64_internal.h :: t64_has_si_profile` and the five importer sites `state_commit`, `state_domain_for`, `state_parse_table`, `state_read_rdp` and `state_read_rdp_carried`, and three existing rows (`t64_digest_stream`, `t64_has_rdp_depth_profile`, `state_version_registered`) would name the predicates; `repo.si-membership-sites` passes because it compares the generator with the committed file under the shipped token set (regenerated equals committed). This weakens the discovery aid ("a family added later must be wired into each listed site"), not the v26 behavior: the exact-tuple registry, the domain check, `unit.core.profile.v26-predicate-matrix` (25 ids) and the digest tests are independent guards | recorded; `scripts/list-membership-sites.py` is not edited here (Phase 9 bound path, listed in `scripts/qualify-phase09.py` `SOURCE_PATHS`, which a closed record verifies only at its own commit). A later numbered plan (before the Phase 11 freeze or at the Phase 10 close-out) should add the two tokens and regenerate the list in one commit; until then a future family author must grep for `t64_has_si_v26_profile`, not read the list |
| W2-03 | info | `core/tests/test_digest_v26.c:741-818` | the completeness table covers `t64_rdp_state` and the two tables only; the plan's check (c) also names the VI state, which has no rows. The VI walker is unchanged against the Wave 2 base and the probe above shows the same inert byte set (observer cursors, the two identity strings per slot, padding) on the five family ids | none for the bump (v26 equals v25 outside the new span, shown by `v26-strip-differential`); a VI row set can join a later completeness test if the owner wants VI covered in the v26 file |
| W2-04 | info | `runner/t64_real.c:1273` | the runner refuses a state export or import on any profile whose state version is not 25, and `T64_STATE_CONTAINER_FORMAT` stays `t64.state-container.v1`, so no runner path can label a container v2 blob as v1 before plan 10-15 adds the v2 format string and pairing (fail closed) | none; plan 10-15 |
| W2-05 | info | `core/CMakeLists.txt` (the asan block of the two import names) | the sanitizer TIMEOUTs of `unit.core.state.export-v26` and `export-v26-bounds` stay at the gate value 300 s against 34.36 and 23.79 s of ASan (8.7 and 12.6 times); `import-v26-hostile` is 144.60 s against 900 s (6.2 times), against 111.9 and 119.4 s in the 10-12 record (this run overlapped a digest sweep at 1-minute load 14) | none; the preflight timeout audit holds the gate values and F-108 holds the ratio |
| W2-06 | info | `docs/spec/state-container.md:150-195`, `core/tests/test_state_export_v26.c:94` | the idle sizes 10,545,971, 10,546,563, 10,548,099 and 10,578,819 are derived by the test as the measured state 25 size plus 2,097,176; the review recomputed all four from the documented section lengths and the layout (check (d)), so the literals do not rest on the measurement alone | none |
| W2-07 | info | `.planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md` section 9 | T-10-49 (the 10-03 recorded exception: the ten closed Phase 9 claim blocks compare the pinned Phase 9 manifest digest and a per-row view, not the live manifest file) is unchanged: no byte of `scripts/tests/test_phase09_qualification.py`, `docs/claims/phase09.md`, `docs/evidence/phase09-qualification.md`, `fixtures/manifest.json` or the authority checker and record changed since the Wave 0 head, `harness.phase09-qualification` passes in the scratch gate, and plan 10-19 requires the section "Recorded exceptions for owner review" and a repeat in its summary | recorded exception stands and stays flagged; it is not an owner approval of the comparator change |

Closing note (plan 10-20): W2-03 is CLOSED by plan 10-20. The VI rows were added to the committed case `v26-completeness` (commits `0ed0d75` and `49c935a`), the mutant row `v26-digest-omit-vi-phase-cycles` (commit `be683bf`) SURVIVED the case before the rows existed and is KILLED by it after them, and the record is in the section "Gap closure (plans 10-20 and 10-21)" below and in PIN-IMPACT.md section 16. The W2-03 row above is the finding as it was written at the Wave 2 head and is not rewritten.

### Task 3: preflight, push and the hosted pair

Preflight on the clean committed tree (`sh scripts/preflight-phase10.sh`, head `f57b019`): `[preflight] PASS`,
total 100 s (oracle-authority 0 s; ctest-counts 40 regex rows, 0 mismatches, and 24 tests carry the label
`phase10` as the `PHASE10_TESTS` block says; host-load 0 s with the WARN at load 11.70; head-ci 1 s, `origin/main`
`a0c0a38` completed with success; timeout-audit 0 s, 101 registered TIMEOUTs, 0 too low, 0 unmeasured, lowest
ratio 2.9 on `unit.core.state.import` against its 210 s `COST`; pii 1 s with the tree (1,291 files) and the
33-commit range at 0 findings; linux-compile 56 s with 227 units under the act image cc 13.3.0, because C changed
since the preflight base `a0c0a38` (first path `core/CMakeLists.txt`); post-gate-replay 41 s with 11 `ci.yml` steps
replayed and the outputs removed). No sanitizer or timeout finding.

Push (AUTONOMY section 4 routine push to the public repository, no force, no tag): the pinned refspec
`f57b019d1c60c96bd11b6cab8cad4db287a291d9:refs/heads/main` moved `origin/main` from `a0c0a38` to `f57b019`
(fast-forward, 33 commits: 4 `docs(10-08)`, 3 `docs(10-09)`, 3 `docs(10-10)`, 3 `docs(10-11)`, 3 `docs(10-12)`, 2
`docs(10-13)`, 2 `feat(10-09)`, 1 `feat(10-11)`, 1 `feat(10-12)`, 2 `test(10-09)`, 4 `test(10-10)`, 1 `test(10-11)`
and 4 `test(10-12)`). Before the push: 35 changed files, 10 under `.planning/`, 8 under `docs/` and 17 others (the
five production sources, `core/CMakeLists.txt`, the six test sources, `fixtures/state/decode_container_v2.py`,
`scripts/check-test-inventory.sh`, `scripts/p10-mutants.py`, `scripts/preflight-phase10-regexes.txt` and
`tools/digest-sweep/t64_digest_sweep.c`), all text, no binary and no ROM, disc, save, audio or image extension in
the range, no workstation path, e-mail address (other than the public no-reply commit identity, the only author),
key or token pattern in any added line, the PII scan clean on the tree and on the range, and no protected
material (the new tests are authored, their literals are hand derivations, the decoder is authored stdlib code, no
oracle or ROM content).

Hosted pair: run `45530265365` (`ci.yml`, event `push`, head `f57b019`), status completed, conclusion **success** on
both jobs, Binutils cache hit on both with the key of F-111, F-120 and F-121 (`69d0bfd4...`), no cache deleted or
changed. The run was found by `headSha` equal to `git rev-parse origin/main` and waited for with one bounded `gh
run watch`.

| Measure | gate-macos | gate-linux | Wave 1 reference (F-121, run 45321391540) |
|---------|------------|------------|-------------------------------------------|
| queue | 4 s | 3 s | 2 s, 3 s |
| run | 496 s | 249 s | 326 s, 230 s (F-120: 346 s, 229 s) |
| Gate step | 424 s | 198 s | 288 s, 181 s |
| ctest (`Total Test time`), 248 of 248 | 260 s | 125 s | 187 s, 115 s (236 names) |
| sh-n, lint-selftest, fixture-qualification, configure | 0, 10, 3, 3 s | 0, 18, 1, 1 s | lint-selftest 7 s (macOS, F-121), 15 s (Linux, F-120) |
| build | 34 s | 22 s | 17 s (macOS, F-121), 20 s (Linux, F-120) |
| boot-evidence-selftest | 39 s | 9 s | 19 s (macOS, F-121), 9 s (Linux, F-120) |
| test-inventory-selftest | 49 s | 10 s | 38 s, 8 s |
| core-lint | 23 s | 10 s | 15 s (macOS, F-121), 9 s (Linux, F-120) |
| audio qualification records | 36 s | 27 s | 21 s, 28 s |
| `unit.core.state.import` start position and seconds | 2nd of 248, 216.18 s | 2nd of 248, 103.59 s | 2nd of 236, 172.55 s and 103.11 s |
| the other three COST tests (start position, seconds) | state-field-use-audit 3rd 72.80 s, rsp-no-hle 4th 45.96 s, phase09-qualification 5th 28.39 s | 3rd 37.85 s, 4th 41.25 s, 5th 9.45 s | 3rd 60.91 s, 4th 43.49 s, 5th 26.83 s (macOS) |
| the 12 Wave 2 names, summed | 53.91 s (import-v26-hostile 31.42, roundtrip 9.80, export-v26 6.39, export-v26-bounds 4.25, digest.v26-completeness 1.67, the other seven 0.02 to 0.11) | 29.47 s (16.62, 4.63, 3.98, 2.63, 1.02, the other seven 0.02 to 0.34) | not present |

Reading (F-119 noise band, F-122): this is the slowest warm gate-macos sample so far (496 s; the earlier seven
were 326 to 487 s) and its Gate step is 136 s above the Wave 1 sample, more than the 52 s spread, so the plan's
rule adds a finding row (F-122). The decomposition points at the runner, not at the 12 names: the steps that no
Wave 2 edit touches are slow too (lint-selftest 10 against 7 s, build 34 against 17 to 19 s,
boot-evidence-selftest 39 against 19 to 21 s, core-lint 23 against 15 to 19 s) and so is the critical-path test
whose v25 flow Wave 2 did not change (`unit.core.state.import` 216.18 s against 172.55 s). On Linux the run is 19 s
above Wave 1 and the ctest wall 10 s above it, inside the spread. The 12 names add 53.91 s of test seconds on macOS
and 29.47 s on Linux; spread over the runner's cores that is the upper bound of their share of the 73 s ctest
delta, so the two effects cannot be separated from one sample. Margin to the 900 s limit: 404 s warm; the cold
projection with the 186 s Binutils build delta is 682 s (margin 218 s), against 673 s (F-111). That is thinner
than the earlier rows, so plan 10-17 should re-read the measured total before the cold run of plan 10-19, and the
reversal rule of the G4 exception stays the stop-loss. Recorded in `docs/ci-reviews/LOG.md`: the Runs row for the
run and finding F-122. The Wave 2 row of the PIN-IMPACT.md "Gate-growth ledger" is added in the same commit. The
cold measurement stays plan 10-19 and no hosted cache was deleted here.

Plan 10-13 reviewed 10-09 to 10-12 without a blocking finding: the Wave 2 head passes the full gate in isolation
(248 of 248) and all 24 Phase 10 names under ASan, the digest sweep reports 0 differences over the 21 shipped ids
from both the Wave 0 and the Wave 2 base (including the four v25 export lines), the RDP state has 100 members and
every one has a row, the VI state and the two tables are complete, the idle sizes are re-derived by hand, exactly
one tuple, one domain and one container version value are new, the version pairing refuses every wrong pair, the
importer has no apply-before-validate path, 25 mutants are KILLED with both controls as expected, D10 is accepted
(with the pin-strength warning W2-01) and the pushed head's hosted pair is green. Wave 3 (plans 10-14 to 10-18)
may start at the Wave 3 base recorded in PIN-IMPACT.md "Wave bases".

## Wave 3

Plans 10-14 to 10-17 (phase-wave 3 RUN-07, the one manifest-moving commit, and the phase-wave 4 tooling: the
provisional-name sweep and Phase 9 triage of 10-16, the qualifier of 10-17), reviewed by plan 10-18 (Tasks 1 and
2 below), then the one final independent audit of the whole phase ("## Final audit", Task 3) and the push
(Task 4). The review re-runs commands and reads the diffs instead of trusting the plan summaries.

| Item | Value |
|------|-------|
| Wave 3 base | `1cded8de34b0c2f5c5bea903095ffc7f1e55d6fc` (extracted from PIN-IMPACT.md "Wave bases" with the 10-01 base extraction idiom: exactly one 40-hex line, an existing commit, an ancestor of the head) |
| Wave 0 base | `4ac633e880e0c26005cb03092e8d8485e721034c` (same idiom, for the whole-phase checks) |
| Reviewed head | `4dd936a41050f4fe4eebdea2f705dbe9ed4a7df0` (25 commits after the Wave 3 base; 27 files changed outside `.planning/`, 5,419 insertions and 50 deletions). It is the plan 10-17 head `856b1e9` plus one commit, `4dd936a`, that corrects one sentence of `scripts/qualify-phase10.py` (the mutants stage derivation said "26 mutants and 2 controls", the driver lists 25 and 2; text only, the limits, the command and the check are unchanged) so that the qualification base is taken after the last source edit. Every later commit of this plan changes only `.planning/` and `docs/ci-reviews/LOG.md`, so the reviewed tree is the pushed tree |
| Host | Darwin arm64, 1-minute load 6.3 to 10.7 during the runs below (other sessions on the same machine, so every second count is an upper bound for a quiet host) |

### Task 1: scratch gate and sanitizer selection of the final head

Method: the W0-01 form of Wave 0, not a bare `git archive` tree. `git clone --no-checkout --no-hardlinks` of the
main repository into `build/10-review/final/src`, `git checkout --detach 4dd936a...`, and a copy (not a symlink)
of `build/tools/install` so the pinned Binutils 2.45 is reused. `git status --short` of the clone was empty, and
`git archive HEAD` of the clone and of the main repository have the same SHA-256
(`d2d9524616a88fe99c0a42d9921c0b09eb827a46c492db520716ef842b7a60a7`), so the scratch tree is exactly the archive of
the head (the equality was checked again after the last command of this review). Every command ran in that clone
with no `CI` or `GITHUB_*` variable; the shared tree was only read. The cold-boot attempt counters of the Phase 6
tests (D-10-17-01) start at 01 in a fresh clone; the shared tree stands at 2 attempts of each family after the
archive move of plan 10-17, so the 10-19 qualification has the full 99.

**Gate.** `sh scripts/gate.sh`: `[gate] PASS`, `[gate] total 123s`, exit 0, first attempt. Steps in seconds: sh-n
0, lint-selftest 3, fixture-qualification 1, configure 1, build 4, boot-evidence-selftest 11,
test-inventory-selftest 20, test-inventory 1, ctest 72 (`100% tests passed out of 254`, `Total Test time (real) =
72.73 sec`), core-lint 10. The CTest count is the 248 names of the Wave 2 head plus the five names of plan 10-15
(`runner.composite-two-run`, `controls.real.composite-wrong-rom`, `runner.composite-state-v2`,
`harness.composite-oracle`, `runner.state-format-v26`) and the qualifier's `harness.phase10-qualification`. The gate
total is 36 s below the Wave 2 review (159 s) and 33 s below the rehearsal of plan 10-17 (155.88 s); those runs
were at load 12 to 15, this one at 6.3 to 10.2, so the difference is mostly the host, not the code.

The 30 `PHASE10_TESTS` names ran 23.23 s in total in that gate (one run each, `CTestCostData.txt` of the scratch
gate, none carries `COST`; `ctest -N -L phase10` reports 30 and the block lists 30). The six names that Waves 3 and
4 added are 9.50 s of it: `harness.phase10-qualification` 6.954 (the qualifier's own hermetic test, dominated by
its subprocess runs), `runner.state-format-v26` 1.441, `runner.composite-two-run` 0.543, `controls.real.composite-wrong-rom`
0.271, `runner.composite-state-v2` 0.249 and `harness.composite-oracle` 0.046. The slowest names of the whole gate in
this run were `unit.core.state.import` 46.6 s, `harness.state-field-use-audit` 17.5 s, `runner.cpu-diagnostic` 15.5 s and
`repo.rsp-no-hle` 14.8 s.

**Sanitizer selection.** `cmake --preset asan` (0 s), `cmake --build --preset asan` (361 build steps, exit 0, 4 s
with the compiler cache) and `ctest --preset asan -L phase10 --output-on-failure`: exit 0, `100% tests passed out of
30`, `Total Test time (real) = 128.29 sec` (`phase10 = 280.87 sec*proc`). The `PHASE10_TESTS` block lists 30 names,
so the run count equals the block. No sanitizer report: 0 lines matching `ERROR: AddressSanitizer` or `runtime
error` in the log.

| Phase 10 name (ASan preset) | seconds | | Phase 10 name (ASan preset) | seconds |
|-----------------------------|---------|-|-----------------------------|---------|
| `unit.core.state.import-v26-hostile` | 120.95 | | `unit.core.digest.v26-completeness` | 3.98 |
| `unit.core.state.import-v26-roundtrip` | 36.75 | | `unit.core.k1.dispatch-refusal` | 3.69 |
| `runner.state-format-v26` | 31.08 | | `unit.core.vg.ntsc-clock` | 2.28 |
| `unit.core.state.export-v26` | 24.76 | | `unit.core.digest.v26-strip-differential` | 0.54 |
| `unit.core.state.export-v26-bounds` | 16.22 | | `unit.core.rdp.depth-composite-sidecar` | 0.34 |
| `runner.composite-two-run` | 13.46 | | `unit.core.vg.z-exponent-boundaries` | 0.33 |
| `unit.core.k1.apply-refusal` | 7.34 | | `controls.real.composite-wrong-rom` | 6.54 |
| `harness.phase10-qualification` | 6.64 | | `harness.composite-oracle` | 0.05 |
| `runner.composite-state-v2` | 4.80 | | the other thirteen names | 0.05 to 0.19 each |

The slowest sanitizer name is `unit.core.state.import-v26-hostile` at 120.95 s against its sanitizer TIMEOUT of 900
s (7.4 times); of the Wave 3 and 4 names `runner.state-format-v26` is 31.08 s against 300 s (9.7 times),
`runner.composite-two-run` 13.46 s against 120 s (8.9 times) and the other four are at least 17 times below their
limits. All are above the 2 times line of F-108.

### Task 2: composite evidence independence and the unrelaxed raw-capture exclusion

`WAVE0_BASE` and `WAVE3_BASE` are the two 40-hex literals above (base extraction idiom, never `HEAD`). `HEAD` is the
reviewed head `4dd936a`. Commands ran in the main tree where they only read (the oracle script, `git diff`,
`git log`, the supervisor and the fixture ROM, with scratch output in a private directory outside the repository)
and in the scratch clone where they needed a build.

| # | Check | Command or method | Result |
|---|-------|-------------------|--------|
| a | the committed expectation equals the independent oracle | `python3 -B fixtures/composite/composite_oracle.py --check fixtures/composite/composite.expected.json` | exit 0, "composite expectation equals the oracle (2737 bytes)" |
| b | the oracle is independent of the producer | read of `fixtures/composite/composite_oracle.py` | imports only `argparse`, `hashlib`, `json`, `os`, `sys` and `fractions`; no import from `runner/`, `core/` or `tools/`, no subprocess, no `sys.path` edit; the only files it opens are the input script named by `--input`, the `--write` target and the `--check` file; the header says it reads no ROM, core, runner or tool. A search for `runner`, `core/`, `tools/` and `build/` finds only comment sentences (header lines 9 and 45). No blocking finding |
| c | one manifest-moving commit | `git log --format=%H WAVE0_BASE..HEAD -- fixtures/manifest.json` and the same from `WAVE3_BASE` | exactly one commit in both ranges, `f027b055a1cd14e7cb84e0fd9f9a31b243f175cb` (`feat(10-14): composite manifest and runner rows`); the manifest holds 41 rows, the 39 of the Wave 0 base equal in canonical JSON and in position, and two appended (`phase10-composite-s32k`, `phase10-composite-wrong-s32k`) |
| d | two-commit shape of plan 10-14 | `git show --stat` of both commits and `git cat-file` | `419275d` (guest source, linker script, builder, oracle, input script, the CMake registrations of the fixture build; no manifest row and no runner row) precedes `f027b05` (`fixtures/manifest.json`, `runner/t64_real_fixture.c`, one sentence of `docs/spec/result-protocol.md`, `PIN-IMPACT.md`); `419275d` holds `fixtures/composite/composite.S` and is the `source.revision` of both new rows, so each row names a commit that holds its source. Accepted: the shape is what the Phase 9 rows do and it keeps the hash chain honest |
| e | the oracle request pin | `PINNED_REQUEST_SHA256` at `WAVE0_BASE`, `WAVE1_BASE`, `WAVE3_BASE` and the head | `a4bb7774...` at the Wave 0 base, `e131cbe7e255052e706668b6cb0d7192c4ac877b65bd7c3a33e65194962927ab` at the Wave 1 base, the Wave 3 base and the head: it moved once (`b64098f`, the 10-03 orchestrator rebind of GATE-02, identity only) and the two composite rows of this wave moved it not at all, which is the GATE-02 outcome on a real appended row. `harness.rdp-oracle-authority` passes on the 41-row manifest |
| f | closed Phase 9 records | `git diff --quiet WAVE0_BASE HEAD -- docs/claims/phase09.md docs/evidence/phase09-qualification.md`; `ast` comparison of the `F2_PINS` and `F2_PIN_KEYS` assignments of `scripts/tests/test_phase09_qualification.py` | both documents byte identical to the Wave 0 base (exit 0); both literals `ast.unparse` equal to the Wave 0 base |
| g | supervisor and real runner diff | `git diff WAVE0_BASE..HEAD -- runner/t64_supervisor.c runner/t64_real.c` (46 insertions, 11 deletions) | only the container format pairing of plan 10-15 Task 3: a comment and the closed-set test `{v1, v2}` in `state_export_flags_valid`; a `format` member of `state_request`, the function `state_request_admitted` (export admitted as (25, v1) or (26, v2), import only on 25, everything else the existing prelaunch refusal at guest tick 0) replacing the old state-25-only guard, and the parser accepting the same closed set. Zero added or removed lines contain the word "raw". The exclusion lines read in place are unchanged: `runner/t64_supervisor.c:588` (save), `:627` (replay), `:664` (state export), `:696` (state import) and `:955` (the artifact pair), each carrying `raw_capture_dir_seen`. No blocking finding |
| h | live refused invocations (the manual supervised run of the plan) | the supervisor of the shared gate preset on `build/fixtures/phase10-composite-s32k.z64` with `composite.input.json`, scratch output outside the repository, an accepted baseline first | baseline raw run: `pass`, `positive-match`, guest tick 599,357, state digest `97f270274d456e883ceaa11f100f92c7fbf3150e89031fa51164e79991f1267c`; baseline save-out run: the same outcome, tick and digest, `composite.sra` 32,768 bytes. `--raw-capture-dir` with the artifact pair, with the save-out options, with `--state-export` (v1 and v2 strings) and with `--replay-record`, and with a save-out leaf and no artifact pair: each exit 2, `harness-error` / `usage`, guest tick 0, nothing created. Controls without the raw directory: the v2 state export on this state 26 profile passes (10,578,907 bytes, same digest) and the v1 string on it is the runner's prelaunch pairing refusal (`capture-invalid`, tick 0), a different layer from the usage refusal |
| i | raw capture sizes | the files of the baseline raw run (limit 69,632 bytes per file, `docs/spec/raw-capture.md`) | `manifest.json` 2,273 bytes, three video frames of 48 bytes, two audio packets of 16 bytes: the largest file is 3.3 percent of the limit. (The test removes its evidence directory at exit, so the run of the same arguments is the measurement; `build/gate/10-15-evidence/state-format` is an empty scope directory left behind, 0 bytes.) |
| j | rights rows | `fixtures/manifest.json` | both rows: `source_rights` `authored-here`, `binary_rights` `cleared`, `distribution` `["build-only"]`, profile `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k` |

**Assessments the plan dispatch asks for.**

1. *Plan 10-15 deviation 1: one literal of `runner/tests/test_state_runner.c`* (a Phase 9 inventoried file) changed from
   `t64.state-container.v2` to `t64.state-container.v3` as the "wrong string" of case `export-refusals`, because
   v2 joined the closed set. Read: `git diff 925aca4^ 925aca4 -- runner/tests/test_state_runner.c` is one literal
   and a four-line comment (5 insertions, 1 deletion). The case name `export-refusals` and the case list are unchanged;
   `docs/spec/phase09-cases.json` is byte identical to the Wave 0 base and
   `python3 -B scripts/check-phase09-cases.py --build build/gate --suite state-export` prints "validated nonempty
   state-export phase09 case inventory" (20 cases over `core/test_state_export` and `runner/test_state_runner`);
   the F2 pins name no file hash of `test_state_runner.c` (`inventory_sha256` is the digest of the unchanged
   inventory file; `git grep test_state_runner` outside `.planning/` finds only the inventory, a spec mention and
   `runner/CMakeLists.txt`); the Phase 9 source binding of `scripts/qualify-phase09.py` (`SOURCE_PATHS` names
   `runner` as a directory) is verified only at the Phase 9 qualification commit, as for the six other `runner/`
   files that Phase 10 changed. By hand, with the supervisor of the shared tree: `t64.state-container.v3` and
   `t64.state-container.v0` on the same setup as an accepted v2 export are `harness-error` / `usage` at tick 0
   (the control still controls: a string outside the closed set is the usage refusal). The v2-on-25 pairing that
   the old literal exercised is now covered, next to accepted baselines, by `runner.state-format-v26`.
   **Accepted** (finding W3-01, info). One reading to carry: plan 10-11 prohibition P2 says this file gets "no new
   cases" and its "v25 bound assertions stay as they are"; no case was added and the changed literal is a
   negative-control string, not a v25 bound assertion.
2. *Plan 10-15 deviation 2: the wrong-ROM control's save comparison.* The wrong ROM carries the accepted pin, fails
   it (`guest-fail`, `wrong-transcript`) and the supervisor exports a save only after a pass, so no image of the
   failing run exists to compare. The case asserts, in order: the accepted ROM in the same setup gives an image equal
   to the expectation and a transcript equal to the oracle's; the wrong ROM ends `guest-fail` / `wrong-transcript` /
   `authored-completion` with the wrong fixture id and the profile, publishes nothing (empty artifact directory),
   and its captured transcript equals the oracle's `wrong.transcript` byte for byte (a whole 188-byte comparison
   including the `SR` line the guest read back from the save device); then it places the 16 bytes of that `SR`
   record into the accepted expected image and runs the same first-difference comparator, which must report offset
   271, the oracle's independent `wrong.save_first_difference` (record offset 256 plus the last byte of the 16, a
   one-bit change of the last word; the oracle prints 0x27 against 0x26). I re-derived 271 from the oracle output
   (`--variant wrong` gives the record `5436344344109c649846d0a71026a826` against the accepted
   `...a827`). Strength: the transcript comparison is strong and independent (the whole read-back record is
   predicted by the oracle, and it fails the runner's pin); the derived save comparison proves that the comparator
   discriminates exactly that one-bit save error and reports the right offset, but it does not prove that a
   failing run's exported 32,768-byte file would differ, because by design no such file exists. The full-image
   comparator has its own controls on the accepted ROM (save offset 12,345 and a mutated expectation file fails the
   case). **Accepted** (finding W3-02, info): the weakened form is what the platform can observe; a stronger form
   would need the supervisor to export on failure, which is a result-protocol change and out of this phase.
3. *Plan 10-14 two-commit shape:* check d. **Confirmed**; check c shows the one manifest-moving commit and check e
   the unchanged `PINNED_REQUEST_SHA256` (`e131cbe7...`) since the Wave 1 base, with `docs/claims/phase09.md`,
   `docs/evidence/phase09-qualification.md` and the `F2_PINS` literal byte identical or `ast` equal to the Wave 0
   base (check f).
4. *The raw-capture exclusion is not relaxed:* check g (no changed line mentions raw) and check h (every
   combination refused at tick 0, with a same-setup accepted baseline).

### Findings (Wave 3)

Columns: id, severity (blocking, warning, info), file and line, finding, disposition. No blocking finding was
found.

| id | severity | file:line | finding | disposition |
|----|----------|-----------|---------|-------------|
| W3-01 | info | `runner/tests/test_state_runner.c:1229` | the Phase 9 wrong-format literal moved from the v2 to the v3 string; case, case list, inventory and pins unchanged (assessment 1) | accepted |
| W3-02 | info | `runner/tests/test_composite_two_run.c:1035-1040` | the wrong-ROM save comparison runs on the guest's read-back record placed into the accepted image, because a failing run publishes no save (assessment 2) | accepted; no stronger form exists without a supervisor change |
| W3-03 | info | `scripts/check-rdp-oracle-authority.py:23` | `PINNED_REQUEST_SHA256` is `e131cbe7...` unchanged from the Wave 1 base on; it was `a4bb7774...` at the Wave 0 base and moved once by the recorded 10-03 orchestrator rebind. A reader of "unchanged since the Wave 0 base" must read it as "unchanged by any Wave 3 or 4 plan" | recorded |
| W3-04 | info | `scripts/qualify-phase10.py:465` | the mutants stage derivation sentence said 26 mutants (driver: 25 mutants and 2 controls, 27 rows; the stage check itself is exact) | fixed in `4dd936a` before the audit and the qualification base (text only; `harness.phase10-qualification` 5.9 s) |
| W3-05 | info | `fixtures/composite/composite_oracle.py` | the oracle is independent of the producer in code and in values (standard library only, no ROM or build output read, constants from the authored source and the policy text of the spec documents), not in authorship: the guest and the oracle have one author. Mitigation: a wrong-value control for each of the four outputs, a mutated expectation file that fails the case, and the second computation by `--check` | recorded; evidence tier stays "unit-tested, authored scope" |
| W3-06 | info | `build/gate/06-05-cold-hello-attempt-NN`, `runner/tests/test_cold_boot.c:124-139` | D-10-17-01 (the 99 attempt cap of two Phase 6 tests in a long-lived build tree) is still open and still a Phase 11 test hygiene item; the shared tree stands at 2 attempts of each family after the archive move of plan 10-17, so the qualification of plan 10-19 and its few full gate runs have room | recorded in `deferred-items.md`; checked again at the end of this plan |
| W3-07 | info | `scripts/tests/test_phase09_qualification.py:955-1020` (T-10-49, recorded exception) | re-checked on the final head: the three constants reproduce from the Wave 0 base (`PHASE09_CLAIM_MANIFEST_SHA256` equals the SHA-256 of the Wave 0 manifest, `b75864fdf4404a4691bfed12ccaceaca22879e60ea739ef3cf89ca1eee6e80e0`; a fresh script that does not import the test builds the 39-row view from the live 41-row manifest and from the Wave 0 manifest and gets `beb2b1c8b209480199ce53d0ce30c51887bfb0769d7a6d522fd7bbe7bf2aec5f` both times, the 39 rows equal in canonical JSON and position, the two appended rows being the composite rows). Only the two 10-03 commits (`9790b7a`, `b64098f`) touched the comparator, the checker, its record and its test since the Wave 0 base; `harness.phase09-qualification` passes in the scratch gate. It is still a recorded exception whose basis is delegated autonomy plus GATE-02, not an owner approval of this comparator change | recorded exception stands and stays flagged for owner review; plan 10-19 must surface it in `docs/evidence/phase10-qualification.md` and its summary |

## Final audit

The one independent audit of the whole phase, before the qualification. Every row below is either a command
re-run in this plan ("now", in the scratch clone detached at the reviewed head `4dd936a` of the Wave 3 section
unless a row says the shared tree) or a recorded disposition that I read and cross-checked against the tree
("recorded"). Bases: `WAVE0_BASE` `4ac633e880e0c26005cb03092e8d8485e721034c`, `WAVE3_BASE`
`1cded8de34b0c2f5c5bea903095ffc7f1e55d6fc` (both 40-hex literals from PIN-IMPACT.md, never `HEAD`). No oracle
material (D5) was used and VID-04 stays deferred and unclaimed.

### Commands re-run now

| # | Command | Result |
|---|---------|--------|
| 1 | `python3 -B scripts/digest-sweep.py --base WAVE0_BASE --head HEAD` (21 shipped ids, two trees built with the forced build identity) | `sweep: 1599 shared lines, 0 differences, 0 head-only lines`, exit 0, 43 s |
| 2 | the same plus the four `--head-extra-id` of the si-v2 ids, then `comm` of the two sorted `sweep.txt` files | `1599 shared lines, 0 differences, 296 head-only lines`, exit 0, 49 s; `comm -23` (lines only in the base) is empty and `comm -13` is 4 x 65 (64 digest lines and 1 export line for each of the four ids) plus 32 stop lines and 4 skip lines = 296, so every extra id admits a ROM (W0-02 closed) |
| 3 | `python3 -B scripts/p10-mutants.py --check-anchors`, then `--run` (clean tree, out-of-tree worktree) | exit 0 both; 27 rows: 25 KILLED, `control-equivalent-depth-cap` SURVIVED and `control-no-compile` INVALID, each as expected; no SURVIVED or INVALID mutant among the 25; 98 s. The kill checks are `test_k1_refusal:apply-refusal`, the nine `test_vg_gaps` checks, `test_rdp_depth_composite:depth-composite-sidecar`, `test_digest_v26:v26-completeness` (two), `test_state_export_v26:export-v26-idle` (three), `test_state_import_v26:roundtrip` (two) and `:hostile` (two) |
| 4 | `ctest -R '^(unit\.core\.k1\..*\|harness\.phase09-qualification\|harness\.rdp-oracle-authority\|repo\.ci-yml-selftest)$'` | 5 of 5 passed, 12.40 s (K1 dispatch 0.20 s and apply 0.37 s, the F2 pins 7.84 s, the per-row oracle binding 2.19 s, the scanner selftest 1.80 s) |
| 5 | `python3 -B scripts/check-rsp-no-hle.py --build build/gate` (sealed-body guard) | `PASS: rsp-no-hle sources_scanned=85`, exit 0, no re-pin; the ten sealed-body sources (`core/t64_bus.c`, `t64_si.c`, `t64_ai.c`, `t64_pi.c`, `t64_save.c`, `t64_rdp.c`, `t64_rdp_depth.c`, `t64_rdp_pipeline.c`, `t64_rdp_raster.c`, `t64_rdp_tmem.c`) have no diff against `WAVE0_BASE` |
| 6 | `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked` and `--require-approved` (shared tree) | exit 0 ("authority record valid; blocked: no oracle operation is authorized") and exit 2 ("no pinned competent owner/legal decision; oracle operations remain blocked"); `TRUSTED_LOCAL_DECISION_SHA256` and `TRUSTED_PUBLICATION_DECISION_SHA256` are still `None` (lines 29 and 30) |
| 7 | `git diff --name-only WAVE0_BASE..HEAD` against the 13 `EXPECTED_CORPUS_PATHS` read from the checker by `ast` | empty intersection (83 files changed, 58 outside `.planning/`: 15 in `core`, 15 in `docs`, 12 in `scripts`, 7 in `fixtures`, 7 in `runner`, 1 in `tools`, 1 in `.github`) |
| 8 | closed and frozen files against `WAVE0_BASE`: `scripts/qualify-audio.py`, `docs/spec/audio-cases.json`, `docs/evidence/phase08-audio.md`, `scripts/qualify-rdp-vi.py`, `docs/spec/rdp-vi-cases.json`, `scripts/qualify-phase09.py`, `docs/spec/phase09-cases.json`, `scripts/gate.sh`, `docs/claims/`, `docs/evidence/phase09-qualification.md`, `fixtures/rdp/`, `fixtures/state/decode_container.py`, `core/tests/test_state_import.c`, `core/tests/test_state_export.c`, `docs/spec/state-mutants.txt`, `scripts/list-membership-sites.py` | `git diff --quiet` exits 0 for every one (identical); `core/include` differs by the one `T64_STATE_CONTAINER_FORMAT_V2` macro and two comment edits of plan 10-15 (no signature, no struct, no existing value) |
| 9 | `git log --format=%h WAVE0_BASE..HEAD -- .github/workflows/ci.yml scripts/check-ci-yml.sh scripts/gate.sh` | one commit, `4ca1054` (`ci(10-04): gate-macos timeout-minutes 15 with a per-job CI-TIMEOUT bound`); the `ci.yml` diff is the single line `timeout-minutes: 10` to `15`; the `check-ci-yml.sh` diff changes the CI-TIMEOUT rule and adds its `mut_timeout` selftest cases only |
| 10 | the relabel gate of PIN-IMPACT.md section 12; `sh scripts/check-docs.sh`; `sh scripts/check-claims.sh` | `stale 0`; `check-docs: ok`; `claims: files=69 blocks=14 errors=0` |
| 11 | one version marker at the head: `core/t64_internal.h`, `core/t64_digest.c`, `core/t64_state.c`, `core/t64_machine.c` | `T64_STATE_CONTAINER_VERSION` is still 1 (line 1899) and `T64_STATE_CONTAINER_VERSION_V2` is the one new value 2 (line 1904); the digest domain texts are `t64.state.si.25` and the one new `t64.state.si.26` (`t64_digest.c:278-279`, `t64_state.c:407-408`); one descriptor 13 tuple (13 / model 21 / state 26) and the four si-v2 ids (`t64_machine.c:1198-1226`) |
| 12 | `git ls-files` for build outputs and rights-bearing or private blobs; `git tag -l` and `git ls-remote --tags origin`; `git.create_tag` | 0 tracked files under `build/`; 0 tracked files with a `.sta .rpl .rpr .sra .eep .z64 .n64 .v64 .rom .bin` extension; the only tags are `phase09-fh`, `phase09-fr` and `phase09-f2` (locally and at `origin`, none from Phase 10); `create_tag` is false in `.planning/config.json` |
| 13 | `sh scripts/pii-scan.sh --tree`, plus a search of every added line since `WAVE0_BASE` for workstation path and e-mail patterns | `findings=0` on 1,307 files; 0 added lines name a workstation path; the only e-mail-shaped strings are a Python decorator (`@contextlib.contextmanager`, 3) and the reserved `noreply@example.invalid` (1) |
| 14 | plan shape: tasks per plan, plan order, durations | 19 plans, no decimal plans, 3 or 4 tasks each; the 111 commits that name a plan since `WAVE0_BASE` are in non-decreasing plan order (0 out-of-order pairs); the Phase 10 plan durations recorded in STATE.md are 12 to 44 min (longest 10-17 at 44 and 10-15 at 41); `parallelization` is false |
| 15 | scratch gate and ASan selection of the head | Wave 3 section Task 1: 254 of 254 in 123 s; 30 of 30 under ASan in 128.29 s, 0 sanitizer reports |

### Success criteria

| Id | Criterion (ROADMAP Phase 10) | Evidence (now or recorded) | Result | Plans |
|----|------------------------------|----------------------------|--------|-------|
| SC1 | an authored depth fixture on the new composite ids gives Z-tested pixels and depth equal to independent literals; a wrong-depth control fails; shipped ids keep digests and Z refusal byte identical across the 17 pre-Phase-9 profiles and the Phase 8 and 9 composites | now: `unit.core.rdp.depth-composite-accepted`, `-wrong-depth` and `-sidecar` pass natively and under ASan (literals from a calculator over the `z_format[8]` table and the V7 rules, wrong-depth compares a 5-byte window of the covered pixel); rows 1 and 2 (0 differences over the 21 ids and 296 head-only lines of the four si-v2 ids); row 4 (K1 dispatch and apply, F2 pins); `unit.core.profile.v26-predicate-matrix` (25 ids) in the gate and under ASan; mutants `k1-predicate-widened` and `v26-sidecar-invalidation-removed` KILLED | verified | 10-02, 10-09, 10-10 (review 10-13) |
| SC2 | a state exported mid-primitive and mid-scan-out with depth active imports and continues to the same frames, digest and outputs; one version marker; a flip-every-member test sees each RDP and VI member; docs state that blobs are build-identity-bound and outside the ABI promise | now: `unit.core.state.import-v26-roundtrip` (idle, a Z triangle in flight and a scan-out with a retained video packet, `test_state_import_v26.c:287-717`) and `unit.core.digest.v26-completeness` with `-strip-differential` pass in the gate and under ASan; mutants `v26-digest-omit-scalar`, `v26-digest-omit-hidden`, `v26-import-skip-hidden` and `v26-import-depth-after-proof` KILLED; row 11 (one tuple, one domain, one container value); `grep -n build-identity-bound` finds `docs/spec/state-container.md:17` and `docs/spec/machine-contract.md:9`. Recorded: the 100 members of `t64_rdp_state` and both tables have committed rows; the VI half was covered at the audited head only by the Wave 2 probe (W2-03), and committed VI rows were added by plan 10-20 (the section "Gap closure") | verified | 10-09, 10-11, 10-12, 10-15 (format string), review 10-13 |
| SC3 | VG-01 to VG-05 and the standard-NTSC VI clock vector each have independently derived expected values and at least one failing mutant | now: the nine `unit.core.vg.*` names pass in the gate and under ASan; row 3 (14 VG and clock mutants KILLED, each by the named `test_vg_gaps` check). Recorded: the 94 literal comparisons were matched against a calculator written from the documents in the Wave 1 section; sibling copies of the floor are residual (DEF-W1-01) | verified | 10-06, 10-07 (review 10-08) |
| SC4 | one composite guest reports exact raw video and audio from a capture run and exact save bytes from a save-out run with the same final digest, each output against its own independent expectation and each with a wrong-value control | now: `runner.composite-two-run`, `harness.composite-oracle`, `controls.real.composite-wrong-rom` and `runner.composite-state-v2` pass in the gate and under ASan; Wave 3 checks a, h and i (by hand: raw run and save-out run both `pass` at guest tick 599,357 with state digest `97f27027...1267c`, a 2,449-byte bundle, a 32,768-byte save); controls at video frame 0 offset 17, audio packet 1 offset 5, save offset 12,345, transcript offset 100 and the wrong ROM at save offset 271 (Wave 3 assessment 2) | verified | 10-14, 10-15 |
| SC5 | a measured cold-cache hosted gate-macos run finishes inside its limit; before and after step timings, the 15-minute limit with its scanner rule and the pinned limit relation are recorded; adding a manifest row no longer requires an oracle-request rebind | now: row 9 and row 4 (`repo.ci-yml-selftest`: 138 cases, the per-job CI-TIMEOUT bound with `mut_timeout` cases); `PINNED_REQUEST_SHA256` unchanged by the two appended composite rows and `harness.rdp-oracle-authority` passing on 41 rows (Wave 3 check e). Recorded: warm timings F-120 to F-122 and the Gate-growth ledger (PIN-IMPACT.md section 6, 6.1) with this plan's hosted sample added in Task 4. The measured cold-cache run itself: **pending 10-19 (orchestrator)** | pending 10-19 (orchestrator) | 10-03, 10-04, 10-05, 10-17; 10-19 |

### Requirements

| Id | Requirement | Evidence (now or recorded) | Result | Plans |
|----|-------------|----------------------------|--------|-------|
| R1 | GATE-02 per-row oracle pins; no rebind for an appended row; `local_decision` blocked | row 6 (blocked, `--require-approved` exit 2, pins `None`); `harness.rdp-oracle-authority` in row 4; the 39-row pin record of PIN-IMPACT.md section 8 re-derived in the Wave 0 section (39 of 39 row hashes equal); the two composite rows appended with `PINNED_REQUEST_SHA256` unchanged (Wave 3 check e) | verified | 10-03, 10-14 |
| R2 | GATE-01 COST order, guard disposition, `timeout-minutes` 15 with scanner rule and selftest, before and after timings, measured cold run | rows 4 and 9; recorded: COST 210, 71, 48 and 35 on the four tests (10-04), the `rsp-no-hle` guard removal declined with a reason (F-116), F-120 to F-122 warm samples; the measured cold run is row SC5 | verified; its cold-run half is row SC5 | 10-04, 10-05, 10-17 |
| R3 | VER-06 VG-01 to VG-05 and the NTSC vector with independent literals and failing mutants | row 3 and SC3 | verified | 10-06, 10-07 |
| R4 | VID-07 depth active on composite profiles under new ids, shipped ids byte identical, independent literals and a wrong-value control | SC1 | verified | 10-09, 10-10 |
| R5 | CORE-06 one state and container marker, depth members, exact mid-primitive and mid-scan-out round trip, flip-every-member test, build-identity docs, format string pairing | SC2 plus `runner.state-format-v26` and `runner.composite-state-v2` (the v2 string paired with state 26, every wrong pair refused at tick 0, the independent decoder accepts the blob) | verified | 10-09, 10-11, 10-12, 10-15 |
| R6 | RUN-07 one composite guest through the real runner twice with equal digest, independent expectations, exclusion not relaxed | SC4 and Wave 3 checks g and h (no changed runner line mentions raw capture; every combination refused at tick 0) | verified | 10-14, 10-15 |

### Locked decisions of 10-CONTEXT.md

| Id | Decision | Evidence (now or recorded) | Result | Plans |
|----|----------|----------------------------|--------|-------|
| L1 | depth only under new registered ids; shipped descriptor 11 and 12 ids and the 17 earlier ids byte identical (one-way) | rows 1, 2 and 11; PIN-IMPACT.md section 2 recorded the ids before the first digest pin | verified | 10-01, 10-09, 10-10 |
| L2 | VID-07 and the CORE-06 marker are one bump (state 26 domain, container 2); v24 and v25 byte identical | row 11 (one tuple, one domain, one container value); rows 1 and 2 (the v25 export lines of the shipped ids are shared lines with 0 differences) | verified | 10-09, 10-11, 10-12 |
| L3 | CORE-06 premise corrected; completeness flip test written first (RED) | recorded: `test(10-09)` RED (`42f64db`) precedes `feat(10-09)` GREEN (`831c947`) in `git log`; the RED failed on exactly the depth members | verified | 10-09 |
| L4 | blobs documented as build-identity-bound and outside the ABI promise | SC2 (`state-container.md:17`, `machine-contract.md:9`) | verified | 10-11 |
| L5 | wave order 0 to 4 (preflight and pin map, GATE-02 then GATE-01; VER-06; the identity bump; RUN-07 with the one manifest commit; review per wave and one qualification) | row 14 (commits in plan order); Wave 3 check c (one manifest-moving commit); four review sections | verified | all |
| L6 | GATE-02: per-row pins; identity-only rebinds orchestrator-only; no oracle, rights or publication authority | R1 and row 6; recorded: the only commit touching the checker is the orchestrator's `b64098f` | verified | 10-03 |
| L7 | GATE-01: COST properties, guard removal measured and declined, `timeout-minutes` 15 with scanner rule and selftest together, timings recorded, success is a measured cold run | R2 and row 9 (one commit changed the workflow, the scanner and its selftest together); the measured cold run is SC5 | verified; its cold-run half is row SC5 | 10-04, 10-05, 10-19 |
| L8 | RUN-07: one authored guest, two runs, equal digest, exclusion not relaxed, fits the per-file capture limit | SC4; Wave 3 check i (largest raw file 2,273 of 69,632 bytes) and g | verified | 10-14, 10-15 |
| L9 | Phase 9 items tagged for this phase triaged as done, deferred or dropped with a reason | recorded: PIN-IMPACT.md section 13 rows T1 to T8 (one done, seven deferred to Phase 11 with reasons; section 13.1 lists the findings carried) | verified | 10-16 |
| L10 | provisional-name sweep so Phase 11 inherits no stale labels | row 10 (`stale 0`); the added lines of the docs diff since `WAVE3_BASE` name only the Phase 11 candidate, none adds an API or ABI freeze statement | verified | 10-16 |
| L11 | plans of at most 4 tasks and about 45 minutes, no decimal plans, a defect fix is a new plan, per-wave scratch review, parallelization false | row 14; no defect fix plan was needed; four review sections (Waves 0 to 3) | verified | all |

### Decisions taken autonomously and Claude's discretion

| Id | Decision | Evidence (now or recorded) | Result | Plans |
|----|----------|----------------------------|--------|-------|
| A1 | test layout: new test files and new CTest names; the bound files never edited | row 7 (empty intersection with the 13 corpus paths) and row 8 (`qualify-rdp-vi.py`, `rdp-vi-cases.json`, `fixtures/rdp` identical); the core tests added are new files (`test_k1_refusal.c`, `test_vg_gaps.c`, `test_profile_v26.c`, `test_digest_v26.c`, `test_rdp_depth_composite.c`, `test_state_export_v26.c`, `test_state_import_v26.c`) plus window additions to `fuzz_state_import.c` | verified | 10-02, 10-06 to 10-12 |
| A2 | GATE-02 keeps whole-file hashes of the other 13 files | the 13 bound paths are unchanged (row 7) and no Phase 10 test lives in a bound file; recorded in PIN-IMPACT.md section 3 | verified | 10-01, 10-03 |
| A3 | `rsp-no-hle` guard: schedule last, measure, decline if it needs a qualifier edit | row 8 (`qualify-audio.py`, `audio-cases.json` identical); recorded: the repeated cost is the CTest name in the audio-records step, so the removal was declined (F-116) | verified | 10-04 |
| A4 | no new hosted CI step | row 9 (one `ci.yml` line changed, no step added) | verified | 10-01, 10-04 |
| A5 | identity naming: descriptor 13 / model 21 / state 26 mirroring the four SI ids; membership widened in the header instead of editing sealed bodies | row 11 and row 5 (ten sealed bodies identical to the Wave 0 base) | verified | 10-09 |
| A6 | one scripted preflight under about 90 s cited by every plan | recorded: PASS runs of 23 s (no C change), 67, 76, 82, 86 and 100 s (with C changes); this plan's run is recorded in Task 4 | verified | 10-01 |
| A7 | thin delta qualifier, no new dependency | recorded: `scripts/qualify-phase10.py` (stdlib only), its hermetic test passes in the gate (6.95 s) and under ASan (6.64 s), the rehearsal of PIN-IMPACT.md section 14 verified; the sentence correction of this plan is Wave 3 finding W3-04 | verified | 10-17 |
| A8 | fuzz windows and field-use audit rows for each new member | `core/tests/fuzz_state_import.c` and `docs/spec/state-field-rules.txt` changed; `harness.state-field-use-audit` passes in the gate (`findings=0` in the Wave 2 section) | verified | 10-12 |
| A9 | review per wave in scratch clones plus one final audit | the Wave 0, 1, 2 and 3 sections of this file and this section | verified | 10-05, 10-08, 10-13, 10-18 |
| A10 | docs updated in the bump wave (rdp.md, vi.md, machine-contract K1 text, state-container, PROJECT.md) | `docs/spec/rdp.md`, `vi.md`, `machine-contract.md`, `state-container.md` are in the changed list of row 7 and `.planning/PROJECT.md` changed in `57d38b2` | verified | 10-10, 10-11 |
| C1 | exact id values and tuple naming | PIN-IMPACT.md section 2 (four ids, 13 / 21 / 26, profile hashes pinned in 2.1) and row 11 | verified | 10-01, 10-09 |
| C2 | plan decomposition (12 to 16 estimated) | 19 plans under the recorded G5 exception of the ROADMAP (four review plans and the qualifier split) | verified | planning |
| C3 | optional `make -j2` boot-fixtures change | declined, recorded in PIN-IMPACT.md section 3 (it changes the Binutils cache key and a bound script for a cold-only benefit) | verified | 10-01 |

### Prohibitions of the plans' must_haves

Each row names the plan and the statement in short form. "Holds" means the command or the read named in the row confirms it on the audited head.

| Id | Plan prohibition | Evidence (now or recorded) | Result |
|----|------------------|----------------------------|--------|
| P01.1 | 10-01: the preflight never edits the checker, the pin or the bound files | the only commit that touches `scripts/check-rdp-oracle-authority.py` since the Wave 0 base is `b64098f` (10-03, orchestrator); row 7 | holds |
| P01.2 | 10-01: no workstation path, host name or e-mail in the preflight output or PIN-IMPACT.md | row 13 | holds |
| P02.1 | 10-02: the sweep and K1 only observe the shipped ids | rows 1 and 4 | holds |
| P02.2 | 10-02: only authored or cleared ROMs in the sweep | the ROM stems in the base sweep are `audio-pcm`, `hello`, `ipl3_compat`, `phase09-admit-e16k`, `-e4k`, `-s32k`, `-si-v1` and `rdp-fill`, all authored or the cleared openIPL3 asset (manifest rights rows); no protected ROM, IPL or save | holds |
| P03.1 | 10-03: an executor never edits the checker, the pin, the trusted pins or the Phase 7 record | the commits touching them are `b64098f` (named "orchestrator") and its test commit `9790b7a`; the pins are `None` (row 6) | holds |
| P03.2 | 10-03: no oracle, rights or publication authority granted | row 6 | holds |
| P03.3 | 10-03: no closed Phase 9 claim, evidence, tier, count, oracle revision or F2 pin changed | row 8; Wave 3 check f; W3-07 | holds |
| P04.1 | 10-04: no scanner rule other than CI-TIMEOUT relaxed | row 9 (the `check-ci-yml.sh` diff) and `repo.ci-yml-selftest` in row 4 | holds |
| P04.2 | 10-04: the closed Phase 8 qualifier and record not edited | row 8 | holds |
| P05.1 | 10-05: the reviewer never ran a clean, a fixture rebuild or a mutant run in the shared tree | recorded: the Wave 0 section (all builds in `build/10-review/w0`); the same method here, and the shared tree has no uncommitted change before this plan's commits | holds |
| P05.2 | 10-05: no push while red, no hosted cache deleted | recorded: `gh cache list` shows the two Binutils entries created 2026-11-19 with the key `69d0bfd4...` of F-111, unchanged | holds |
| P06.1, P07.1 | 10-06, 10-07: no expected value from a production helper, an emulator or an oracle | recorded: the literals are derived in comments from `rdp.md` and `vi.md`; the Wave 1 review matched 94 of 94 against a document-derived calculator; no oracle path in row 7 | holds |
| P06.2, P07.2 | 10-06, 10-07: the bound tests and `rdp-vi-cases.json` not edited, production code unchanged in 10-07 | rows 7 and 8; the production sources changed since the Wave 0 base are the five of the Wave 2 identity bump (`t64_digest.c`, `t64_internal.h`, `t64_isviewer.c`, `t64_machine.c`, `t64_state.c`) | holds |
| P08.1, P13.1, P18.1 | 10-08, 10-13, 10-18: a blocking finding is returned for a new plan, never fixed in a review | no blocking finding was found in Waves 0 to 3; the commits of plans 10-05, 10-08 and 10-13 touch only `.planning/` and `docs/ci-reviews/LOG.md`; the one source edit of this plan (`4dd936a`) is the text correction W3-04, not a finding fix | holds |
| P09.1 | 10-09: shipped ids unchanged | rows 1, 2 and 4 | holds |
| P09.2 | 10-09: RED is a behavioral assertion failure, not a lookup failure or a placeholder | L3 (`42f64db` before `831c947`); recorded in the 10-09 summary and the Wave 2 section (the flip test failed for exactly the depth members) | holds |
| P10.1 | 10-10: VID-07 labelled unit-tested, authored scope | recorded: `docs/spec/rdp.md` and the Wave 2 record say the sidecar is a logical model and the Z mode precedence software policy; the evidence tier of this phase is "unit-tested, authored scope" | holds |
| P10.2 | 10-10: no literal from a production helper, no emulator or oracle source opened | recorded: literals from a scratch calculator over the table and the V7 rules (10-10 summary); row 7 | holds |
| P11.1 | 10-11: no blob portability promise; blobs private and never committed | SC2 (build-identity docs); row 12 (0 tracked `.sta` or blob files) | holds |
| P11.2 | 10-11: the Phase 9 export tests get no new cases and keep their v25 assertions | row 8 (`test_state_export.c` identical); `test_state_runner.c` has one changed negative-control literal and no case (W3-01) | holds |
| P12.1 | 10-12: `test_state_import.c` and `state-mutants.txt` not edited | row 8 | holds |
| P12.2 | 10-12: no import accepts out-of-range depth data and refusal never partially applies | `unit.core.state.import-v26-hostile` passes natively and under ASan (120.95 s); mutants `v26-import-addr-unchecked` and `v26-import-z-products-unchecked` KILLED; recorded: the 180-case version pairing probe and the importer reading of the Wave 2 section | holds |
| P14.1, P15.1 | 10-14, 10-15: the raw-capture exclusion not relaxed, no new result key | Wave 3 checks g and h; the `result-protocol.md` diff since the Wave 0 base is relabels and one pass-rule sentence (row count and transcript lengths), no key | holds |
| P14.2 | 10-14: existing phase09 rows and fields not edited | Wave 3 check c (39 rows equal in canonical JSON and position) and f | holds |
| P14.3 | 10-14: no protected bytes beyond the pinned openIPL3 asset; rights rows authored-here, cleared, build-only | Wave 3 check j; row 12 (no ROM or save extension tracked); `gov.rights.repo` passes in the gate | holds |
| P15.2 | 10-15: expected values never from a runner or core output; the expectation regenerated only by the oracle | Wave 3 checks a and b (`harness.composite-oracle`, stdlib-only oracle) | holds |
| P16.1 | 10-16: no closed-phase record edited | row 8; no commit of plan 10-16 touches `docs/claims` or `docs/evidence`; the Phase 7 oracle-authority record (`docs/evidence/phase07-oracle-authority.json` and `.md`) was changed once, by the 10-03 orchestrator rebind `b64098f` under the owner approval of 2026-11-23 | holds |
| P16.2 | 10-16: no API or ABI freeze statement added | row 10 and the freeze-word search of the docs diff since `WAVE3_BASE` (every added line says provisional until the Phase 11 freeze candidate or "nothing here is an API or ABI freeze") | holds |
| P17.1 | 10-17: the qualifier edits no closed qualifier and adds no `ci.yml` step | rows 8 and 9 | holds |
| P17.2 | 10-17: the private record stays under `build/gate/10-qualification-private`, nothing private committed | row 12 (0 tracked files under `build/`); row 13; PIN-IMPACT.md section 14 names the directory by its repo-relative path only | holds |
| P18.2 | 10-18: no oracle, rights or publication authority granted; VID-04 deferred | row 6; REQUIREMENTS.md line 42 and the ROADMAP keep VID-04 unmapped and deferred; no claim document of Phase 10 exists | holds |
| P19.1 to P19.3 | 10-19: no hardware or ABI-freeze claim, no tag or release, no private record or local path committed | not yet executable (the plan runs after this one); at the audited head `docs/claims/phase10.md` and `docs/evidence/phase10-qualification.md` do not exist, there is no Phase 10 tag (row 12), and no private file is tracked (row 12); plan 10-19 re-checks each at its own commit | holds at the audited head |

### Open findings and carried items

Read from Waves 0 to 3; none is blocking.

| Item | Severity | State | Counted against G3 |
|------|----------|-------|--------------------|
| W2-01 the Z product bound is not pinned by a boundary test (two probe mutants survive) | warning | deferred to the Phase 11 freeze-candidate review (PIN-IMPACT.md 13.1, ROADMAP Phase 11 Carry-ins) | yes |
| W2-02 two exact-tuple predicates are not tokens of the membership-site list | warning | deferred, same record | yes |
| DEF-W1-01 three unpinned copies of the perspective floor | info | deferred, same record | yes |
| W1-03 one sentence stating the `ceil` rule in `docs/spec/vi.md` | info | deferred, same record | yes |
| D-10-17-01 the 99 attempt cap of two Phase 6 cold-boot tests in a long-lived build tree | info | `deferred-items.md`; workaround in place (counters at 2 and 2) | not yet counted by the ROADMAP; adding it makes five carried items, the limit of G3 ("at most 5 carried warning-level gaps"), so no further gap can be carried into Phase 11 without closing one |
| W0-05 / W2-07 / W3-07 T-10-49 the Phase 9 claim-binding comparator | recorded exception | flagged for owner review; plan 10-19 surfaces it | no (not a gap) |
| the G4 exception (the required gate grows without an offset) | recorded exception | ROADMAP Phase 10; the Gate-growth ledger and its reversal rule | no (not a gap) |

### Verdict

No blocking finding. Row for row, the phase is evidenced on the audited head except the measured cold-cache hosted run
of SC5 (and with it the cold-run half of GATE-01), which is plan 10-19 and orchestrator-only. The two warning-level
carried items (W2-01, W2-02) and the three info items are recorded for Phase 11. The audited head is pushed and its
hosted pair is read in the next section.

## Audited head: preflight, push and the hosted pair

Task 4 of plan 10-18. The audited head is `f081d392763ad384c3d50ae1b24b709d5af3b7d4`: the reviewed head `4dd936a` plus
three commits that change only `.planning/` (`4a8cefa`, `f29aa79`, `f081d39`; `git diff --name-only 4dd936a f081d39`
lists only `.planning/` paths), so the tree that the scratch gate, the sanitizer selection and the audit re-runs
tested is the tree that was pushed.

**Preflight** on the clean committed tree (`sh scripts/preflight-phase10.sh`, head `f081d39`): `[preflight] PASS`,
total 82 s (oracle-authority 0 s; ctest-counts 46 regex rows, 0 mismatches, and 30 tests carry the label `phase10`
as the `PHASE10_TESTS` block says; host-load 0 s with the WARN at load 9.01; head-ci 1 s, `origin/main` `f57b019`
completed with success; timeout-audit 0 s, 107 registered TIMEOUTs, 0 too low, 0 unmeasured, lowest ratio 2.9 on
`unit.core.state.import` against its 210 s `COST`; pii 1 s with the tree (1,307 files) and the 29-commit range at 0
findings; linux-compile 47 s with 228 units under the act image cc 13.3.0, because C changed since the preflight
base `f57b019` (first path `core/CMakeLists.txt`); post-gate-replay 33 s with 11 `ci.yml` steps replayed and the
outputs removed). No sanitizer or timeout finding.

**Push** (AUTONOMY section 4 routine push to the public repository, no force, no tag): the pinned refspec
`f081d392763ad384c3d50ae1b24b709d5af3b7d4:refs/heads/main` moved `origin/main` from `f57b019` to `f081d39`
(fast-forward, 29 commits: 5 `docs(10-16)`, 4 `feat(10-15)`, 4 `docs(10-13)`, 3 `docs(10-18)`, 3 `docs(10-17)`, 2
`feat(10-17)`, 2 `feat(10-14)`, 2 `docs(10-15)`, 2 `docs(10-14)`, one `fix(10-18)` and one `chore(10-16)`). Before the
push: 38 changed files, 10 under `.planning/` and 28 others (core header and CMake, seven spec documents, the
composite fixture sources and manifest, the runner and its two new tests, the qualifier and its test, scripts and
`docs/ci-reviews/LOG.md`), all text, no binary and no ROM, disc, save or state-blob extension in the range, no
workstation path, key, token or e-mail address in any added line, the PII scan clean on the tree and on the range,
and no protected material (the guest, oracle and expectation are authored, the ROMs are build outputs that are not
tracked). The refspec names only `refs/heads/main`, so no tag was pushed (`git ls-remote --tags origin` still lists only
the three Phase 9 tags).

**Hosted pair:** run `45548987145` (`ci.yml`, event `push`, head `f081d39`), status completed, conclusion **success** on
both jobs, Binutils cache hit on both with the key of F-111 and F-120 to F-122 (`69d0bfd4...`), no cache saved,
deleted or changed (`gh cache list` after the run: the Linux entry of 31.10 MiB and the macOS entry of 5.03 MiB,
both created 2026-11-19). The run was found by `headSha` equal to `git rev-parse origin/main` and waited for with one
bounded `gh run watch` (finished in 6 min 29 s of wall time).

| Measure | gate-macos | gate-linux | Wave 2 reference (F-122, run 45530265365) |
|---------|------------|------------|-------------------------------------------|
| queue | 2 s | 2 s | 4 s, 3 s |
| run | 385 s | 233 s | 496 s, 249 s |
| Gate step | 337 s | 191 s | 424 s, 198 s |
| ctest (`Total Test time`), 254 of 254 | 196 s | 126 s | 260 s, 125 s (248 names) |
| sh-n, lint-selftest, fixture-qualification, configure | 0, 8, 2, 1 s | 0, 11, 2, 0 s | lint-selftest 10 s (macOS), 18 s (Linux) |
| build | 22 s | 22 s | 34 s, 22 s |
| boot-evidence-selftest | 37 s | 10 s | 39 s, 9 s |
| test-inventory-selftest, test-inventory | 51 s, 1 s | 9 s, 0 s | 49 s, 10 s |
| core-lint | 17 s | 10 s | 23 s, 10 s |
| audio qualification records | 28 s | 28 s | 36 s, 27 s |
| `unit.core.state.import` start position and seconds | 2nd of 254, 162.23 s | 2nd of 254, 104.62 s | 2nd of 248, 216.18 s and 103.59 s |
| the other three COST tests (start position, seconds) | state-field-use-audit 3rd 57.83 s, rsp-no-hle 4th 37.63 s, phase09-qualification 5th 22.79 s | 3rd 38.05 s, 4th 41.20 s, 5th 9.28 s | 3rd 72.80 s, 4th 45.96 s, 5th 28.39 s (macOS) |
| the 30 Phase 10 names, summed | 63.07 s | 41.25 s | the 12 Wave 2 names alone: 53.91 s and 29.47 s |
| the six Wave 3 and 4 names, summed | 19.32 s (qualifier test 13.39, `state-format-v26` 3.50, `composite-two-run` 1.13, `composite-state-v2` 0.64, `composite-wrong-rom` 0.57, `composite-oracle` 0.09) | 9.67 s (3.47, 3.34, 1.46, 0.60, 0.75, 0.05) | not present |

Reading (item 7 of the plan dispatch, F-119 noise band, F-123): this is a fast warm sample, not a slow one. gate-macos ran
385 s against the 496 s of Wave 2 and sits inside the 326 to 496 s range of the earlier warm samples of Phases 9 and 10; the
ctest wall of 196 s is 64 s below the Wave 2 sample and 12 s above the Wave 0 sample (F-120, 184 s, with 27 fewer
names), inside the 52 s spread of F-119, so no growth of the gate is read. The steps that no Phase 10 edit touches are lower than in the slow Wave 2 sample (build 22 against 34 s, core-lint 17
against 23 s, lint-selftest 8 against 10 s) or equal to it (boot-evidence-selftest 37 against 39 s), which again says that
runner speed, not the new names, moves these numbers. Against the warning of plan 10-17 (a warm gate-macos run above 700 s is a
warning for 10-19): 385 s is 315 s below it and 515 s below the 900 s limit. Against the projection of PIN-IMPACT.md
section 6.1 (cold gate-macos 696 s packed to 735 s serialized from the 496 s sample, 709 to 806 s from the F-111 baseline,
94 s margin at worst): the 30 names add 63.07 s of summed test time on macOS where 85.7 to 132.9 s were projected, the six
Wave 3 and 4 names 19.32 s where 34.2 to 53.0 s were projected, and warm plus the 186 s Binutils build delta is 571 s,
margin 329 s. The serialized-ceiling warning is therefore not approached by this sample; one sample cannot retire it,
because the Wave 2 sample alone swung 136 s, and the measured cold run stays plan 10-19. Recorded in
`docs/ci-reviews/LOG.md`: the Runs row for the run and finding F-123. The Waves 3 and 4 row of the PIN-IMPACT.md
"Gate-growth ledger" is added in the same commit. No hosted cache was deleted here, and the G4 reversal rule (a cold run
above 900 s) stays the stop-loss.

## Gap closure (plans 10-20 and 10-21)

Written by plan 10-20 (gap closure after `10-VERIFICATION.md`, status `gaps_found`). Evidence tier: code
observations and measurements on this host at the commits named; nothing here is a hardware or compatibility claim.
Plan 10-21 appends its independent review of this closure below.

**The gap.** ROADMAP Phase 10 success criterion 2 says "a flip-every-member test sees each RDP and VI member". At the
audited head the committed case `v26-completeness` of `core/tests/test_digest_v26.c` had rows for the RDP block and the
two tables only; the VI half rested on the uncommitted probe of the Wave 2 section (417 inert bytes of 1,208), which is
finding W2-03. CORE-06 (one version marker, round trip, completeness test, build-identity-bound docs) was therefore
evidenced for the RDP half by a committed test and for the VI half by a probe.

**What plan 10-20 committed** (test source, a driver row and qualifier text; no production source):

| Commit | Change |
|--------|--------|
| `be683bf` (RED) | `scripts/p10-mutants.py` row `v26-digest-omit-vi-phase-cycles`: the `hash_u64(sink, vi->phase_cycles)` call of the VI section of `t64_digest_stream` is replaced by a cast-to-void read; kill check `test_digest_v26 --case v26-completeness` |
| `0ed0d75` (GREEN) | `vi_members`: 120 rows over `t64_vi_state` (114 member rows including all 37 members of both `t64_video_packet_info` slots, and 6 named exclusion rows: `packet_head`, `packet_count` and the per-slot `profile_id` and `build_id`); `_Static_assert` on `sizeof(t64_vi_state) == 1208` and `sizeof(t64_video_packet_info) == 488`; the helper `check_vi_completeness`; the descriptor 13 stem id flips every one of the 1,208 bytes and requires exactly 417 inert flips (791 member bytes change the stream, 400 excluded bytes and 17 padding bytes do not); one flip is tied to the SHA-256 digest |
| `49c935a` | the representative sweep (first byte of every row, last byte of every excluded row, every padding byte) on the three save ids and on the state 25 stem id `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1`; header and case comments |
| `7e9f3b8` | `scripts/qualify-phase10.py`: the mutants stage derivation says 26 mutants and 2 controls, 28 rows; the gate and asan derivation texts carry the figures of the passing plan 10-19 record; no limit literal moved |

The two named exclusion reasons were re-derived from the code, not copied from this review: the observer cursors
(`core/t64_internal.h`: "Host observer cursor and occupancy; neither is part of guest identity"; `core/t64_digest.c`: the
packet comment of the VI section) and the packet identity strings (`core/t64_digest.c` hashes them only under
`!has_v25_framing`; the v25 and v26 families bind identity through the domain and the profile digest of the identity
section). The layout was confirmed from the compiler before any literal was written: 1,208 and 488 bytes, 17 padding
bytes (4 after `refusal_reason`, 4 after `filter` in each slot, 5 after `latch_pending`). No CTest name was added: the
gate still has 254 names, 30 carry the label `phase10`, and `test_digest_v26 --list-cases` lists two cases.

**RED, by behavior.** With the mutant row committed and no VI row (`be683bf`, tree clean),
`python3 -B scripts/p10-mutants.py --run v26-digest-omit-vi-phase-cycles --no-controls` exited 1 and classified the row
SURVIVED against the expected KILLED (3.2 s). The classification is attributable to the omitted member: a scratch probe
(not committed) built the retained-packet state of the stem id (`build_vi_frame`) and compared the canonical streams
before and after flipping bit 0 of `vi.phase_cycles`. On the mutated build the stream is 10,545,043 bytes and the flip
leaves it equal (the committed case passes there, exit 0, 1,556 checks, 0 failures); on the unmutated tree the stream is
10,545,051 bytes and the same flip changes it. The member is real and the equality is caused by the omission alone.

**GREEN.** After `0ed0d75` the same command exits 0 and classifies the row KILLED by `test_digest_v26:v26-completeness`
(5.9 s). Under the mutated build the case reports all eight bytes of `phase_cycles` as "not in the canonical stream", an
inert count of 425 against the required 417 and a failed digest tie. After `49c935a` the representative sweep reports the
same member on the other four ids (a scratch control with the mutated digest object: 12 member-byte failures, the
eight bytes of the stem and one on each of the three save ids and the state 25 stem, and 5 digest-tie failures).

**Case cost.** Native: 0.40 s in the Wave 2 row of the Gate-growth ledger, 1.08 to 1.10 s after the rows (three runs at
1-minute load 14.9 to 15.8). Under ASan and UBSan 7.92 s, no sanitizer report. The budget of the plan (5 s native at load
8 or below) holds with room, so the representative mode was not narrowed.

**Full mutant run** on the clean committed tree at `49c935a`: `python3 -B scripts/p10-mutants.py --run`, 28 rows: 26
KILLED, `control-equivalent-depth-cap` SURVIVED and `control-no-compile` INVALID, every row as expected, 147 s wall at
1-minute load 10.7 to 13.5.

**Consequence for the record of plan 10-19.** The qualifier source and a bound test source changed, so the plan 10-19
native record no longer verifies against the tree; that is by design. Plan 10-21 re-qualifies. Plan 10-20 Task 4 records
the local gate, K1, sweep, protected-path and full-preflight results in PIN-IMPACT.md section 16.

**W2-03 is CLOSED.** The VI half of success criterion 2 is met by a committed test: every member of `t64_vi_state` is
flipped, each flip of a member byte changes the canonical stream, the inert bytes are named exclusions with reasons or
padding, and a mutant that drops a VI member from the walker is killed by the case.
