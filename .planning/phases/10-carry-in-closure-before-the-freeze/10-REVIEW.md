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
| Reviewed head | `96c8711188a287756393c1871e7dda1e0a73f4af` (30 commits after the Wave 2 base; 24 files changed outside `.planning/`, 7,235 insertions and 132 deletions; the four production sources that changed are `core/t64_digest.c`, `core/t64_internal.h`, `core/t64_isviewer.c`, `core/t64_machine.c` and `core/t64_state.c`) |
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


