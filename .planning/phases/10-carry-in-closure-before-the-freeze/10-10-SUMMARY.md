---
phase: 10-carry-in-closure-before-the-freeze
plan: 10
subsystem: core
tags: [rdp-depth, descriptor-13, z-buffer, hidden-sidecar, wrong-value-control, predicate-matrix, k1, mutants, docs, vid-07]

requires:
  - phase: 10-carry-in-closure-before-the-freeze
    provides: the four si-v2 ids with RDP depth active and the v26 digest walker (plan 10-09), the K1 refusal tests (plan 10-02), the digest sweep and the Wave 2 base 6dd7859 (plans 10-02 and 10-08), the mutant driver (plan 10-01), the preflight (plan 10-01)
  - phase: 07-rdp-reference-renderer-and-vi-scanout
    provides: the depth command words and pixel literals of core/tests/test_rdp_depth.c (copied, never edited) and the V7 policy and Z exponent table
affects: [10-11, 10-12, 10-13, 10-14, phase-11]

actuals:
  tokens: 16000
  tasks: 4
  commits: 5

tech-stack:
  added: []
  patterns:
    - "expected depth literals derived by hand from the z_format[8] table and the V7 rules in a scratch calculator outside the repo, cross-checked against the oracle-bound Phase 7 literals, never from z_compress or z_decompress"
    - "wrong-value control: one literal byte of the expected window changed, first difference required at exactly that byte; one-input setup changes required to diverge at the byte the input controls"
    - "sidecar writers proved by comparing the whole 64-granule window against the before snapshot with only the written granules zeroed, with sentinel entries in rows the triangle does not reach"

key-files:
  created:
    - core/tests/test_rdp_depth_composite.c
  modified:
    - core/tests/test_profile_v26.c
    - core/CMakeLists.txt
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase10-regexes.txt
    - scripts/p10-mutants.py
    - docs/spec/rdp.md
    - docs/spec/vi.md
    - docs/spec/machine-contract.md
    - .planning/PROJECT.md
    - .planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md

key-decisions:
  - "The wrong-depth control compares a 5 byte window of the covered pixel (color word, depth word, sidecar entry), not the 128 byte Z image: the set of covered pixels is a rasterization result that is not independently derived here. A scratch probe showed 10 covered pixels (rows 1 to 4) but that observation is not used as an expected value"
  - "The sidecar writer checks use 8 byte aligned DMA rows (granules 16 to 19 for PI, 24 to 27 for SP) and a 1 byte and a 4 byte CPU store; the expected window after a write is the before snapshot with the written granules set to zero, written by the test, with sentinels 0x81, 0x82, 0x83 at granules 0, 43, 63"
  - "Mutant replacements are (void)granule;, (void)rdp->primitive_z; and (void)machine->rdp_depth_hidden; so each compiles under Werror and cannot be INVALID; the digest mutants omit the primitive_z scalar and the hidden-table emission"
  - "Docs cite the primary sources by title, edition and page only (no URL, no archive or workstation path); machine-contract.md also gets the t64.state.si.26 domain row and the v26 stream layout, since the identity bump is in this wave"

patterns-established:
  - "A tracer test is already the final shape of its case: Task 2 only widened the id loop and added cases, so the tracer verify re-run proved the expansion base"
  - "A mutant row that needs a new test binary adds its source to TEST_SOURCES in the same commit as the test, so the driver compiles it with the gate warning flags"

requirements-completed: [VID-07]

coverage:
  - id: D1
    description: "On each of the four si-v2 ids, SetZImage, SetPrimDepth Z 0x1357 dZ 9 and a primitive-Z triangle give color 0x07c1, depth 0x09a8 and sidecar 0x83 at the covered pixel; a farther second primitive (raw 0x2000) leaves all three; a nearer third (raw 0x0800) writes color 0x003f, depth 0x0400, sidecar 0x83"
    requirement: "VID-07"
    verification:
      - kind: unit
        ref: "ctest unit.core.rdp.depth-composite-accepted (488 checks, 0 failures; ASan pass)"
        status: pass
    human_judgment: false
  - id: D2
    description: "The wrong-depth control: a changed literal byte diverges at exactly that byte (five window bytes), and five one-input setups (Z 0x1358, Z 0x2000, dZ 0x10, dZ 2, the accepted setup) diverge at the byte that input controls (indices 3, 2, 3, 4, none)"
    requirement: "VID-07"
    verification:
      - kind: unit
        ref: "ctest unit.core.rdp.depth-composite-wrong-depth (1196 checks, 0 failures; ASan pass)"
        status: pass
    human_judgment: false
  - id: D3
    description: "All four Z triangle opcodes (0x09, 0x0b, 0x0d, 0x0f), the opaque, transparent, interpenetrating and decal modes, the compare and update pairs and the far word 0xfffc behave on the four ids as rdp.md V7 states; the aliased 16-bit and 32-bit color and Z images keep or clear the hidden entry as V7 states"
    requirement: "VID-07"
    verification:
      - kind: unit
        ref: "ctest unit.core.rdp.depth-composite-sidecar (7716 checks, 0 failures; ASan pass); the 15 mode vectors agree with the scratch calculator and with the oracle-bound literals of test_rdp_depth.c"
        status: pass
    human_judgment: false
  - id: D4
    description: "Accepted CPU stores (1 and 4 bytes), PI DMA and SP DMA over the depth image clear exactly the overlapped sidecar granules (the whole 64 entry window compared), a store just past the image changes nothing, and a fresh admission clears the whole sidecar and the depth image state"
    requirement: "VID-07"
    verification:
      - kind: unit
        ref: "ctest unit.core.rdp.depth-composite-sidecar; python3 -B scripts/p10-mutants.py --run v26-sidecar-invalidation-removed: KILLED by test_rdp_depth_composite:depth-composite-sidecar (the descriptor 10 test_rdp_depth also fails, exit 1, recorded)"
        status: pass
    human_judgment: false
  - id: D5
    description: "Predicate matrix over all 25 registered ids: t64_has_rdp_depth_profile true on exactly seven ids (rdp-depth-v7, rdp-vi-v8, rdp-vi-filter-v9, the four si-v2 ids), false on the descriptor 11 id and the four descriptor 12 ids; t64_has_si_profile, the two exact tuple predicates and t64_has_ai_device match a hand-written table"
    requirement: "VID-07"
    verification:
      - kind: unit
        ref: "ctest unit.core.profile.v26-predicate-matrix (prints rows=25, 228 checks, 0 failures); a scratch copy with one expected literal flipped fails with two CHECK lines"
        status: pass
    human_judgment: false
  - id: D6
    description: "The 21 shipped ids keep their digests: sweep against the Wave 2 base, and K1 refusal unchanged"
    requirement: "VID-07"
    verification:
      - kind: other
        ref: "python3 -B scripts/digest-sweep.py --base 6dd785986f6aa78c6da5573a558b15b386a4edee --head WORKTREE: 1599 shared lines, 0 differences, 0 head-only; with four --head-extra-id: 0 differences, 292 head-only lines (64 digest and 9 stop or skip per si-v2 id); ctest unit.core.k1.dispatch-refusal and unit.core.k1.apply-refusal pass"
        status: pass
    human_judgment: false
  - id: D7
    description: "The two v26 digest omission mutants moved from plan 10-09 are killed by the completeness test"
    requirement: "CORE-06"
    verification:
      - kind: other
        ref: "python3 -B scripts/p10-mutants.py --run v26-digest-omit-scalar v26-digest-omit-hidden: both KILLED by test_digest_v26:v26-completeness; controls: equivalent edit SURVIVED, non-compiling edit INVALID"
        status: pass
    human_judgment: false
  - id: D8
    description: "rdp.md V7 prints the eight-row Z exponent table with its citations and names the four si-v2 ids; vi.md, machine-contract.md and PROJECT.md state depth active on descriptor 13 and inactive on descriptors 11 and 12"
    requirement: "VID-07"
    verification:
      - kind: other
        ref: "grep -c NU6-06-0030-001G docs/spec/rdp.md gives 1; sh scripts/check-docs.sh ok; sh scripts/pii-scan.sh --tree findings=0"
        status: pass
    human_judgment: false
  - id: D9
    description: "VID-07 boundary edge: the depth matrix covers the 16-bit and 32-bit alias cases and the far value; classification of that edge and of the wording that the tests prove RDRAM color, depth and sidecar bytes but not VI packets"
    requirement: "VID-07"
    verification: []
    human_judgment: true
    rationale: "The edge was classified by the plan as a manual judgment; the alias cases and the far word are asserted, but whether they are the boundaries the lens names for the rendering role, and whether the vi.md wording is the right scope of the claim, is a reviewer's call"

duration: 27min
completed: 2026-11-24
status: complete
commits: 5
plan_head_before: 3d5b3bd4f58e9eb25377e561d7ba6824733e515f
plan_head_after: 6a019297e0786a2ccb934f6ba857689d31820c35
---

# Phase 10 Plan 10: Depth on the descriptor 13 family, wrong-depth control and K1 documentation Summary

**Z-tested drawing with independently derived depth literals (color 0x07c1, depth 0x09a8, sidecar 0x83) works on all four si-v2 ids, a wrong-depth control diverges at the intended byte, CPU, PI and SP writes and reset clear exactly the matching sidecar granules (the invalidation mutant and both v26 digest omission mutants are KILLED), a 25-id predicate matrix scopes depth to seven ids, and the 21 shipped ids stay byte-identical (sweep 1599 lines, 0 differences).**

## Performance

- **Duration:** 27 min (2026-11-24T04:17Z to about 04:44Z); host 1-minute load 12 to 34, so seconds are upper bounds
- **Tasks:** 4 (Task 1 tracer, Tasks 2 to 4 auto), 5 commits (one added after Task 4, see Deviations)
- **Files:** 11 (10 planned plus `PIN-IMPACT.md`); 1 new test binary, 4 new CTest names

## Accomplishments

- **Task 1 (tracer, `4014766`).** `core/tests/test_rdp_depth_composite.c` copies the helpers of `test_rdp_depth.c` onto the openIPL3 `create_admitted` path. On `...-si-v2` the 13 command words of the accepted-effect case give color bytes `0x07 0xc1`, depth bytes `0x09 0xa8` and sidecar `0x83` (low bits 3), with the derivation in a comment (`0x1357 * 8 = 0x9ab8`, row 0, mantissa `0x26a`, dZ 9 has delta code 3). A farther second primitive (raw `0x2000`, linear `0x10000` against `0x9a80 + 9`) leaves color, depth and entry; a nearer third (raw `0x0800`) replaces the pixel (`0x003f`, `0x0400`, `0x83`). Tracer gate: the `<verify>` re-run on the committed tree passed (auto mode off, `end-of-phase` mode, automated verify only), so expansion went ahead.
- **Task 2 (`9a5dfd8`, plus `6a01929`).** The accepted case runs on the four ids. `depth-composite-wrong-depth` changes one literal byte of the five byte window at a time (first difference at exactly that index) and runs five one-input setups. `depth-composite-sidecar` has, per id: the four Z opcodes with the Phase 7 literals (`0x07c1/0x09a8/0x83` for 0x09, 0x0b, 0x0d; `0x8231/0x0004/0x80` for 0x0f), 15 mode vectors (opaque, decal, interpenetrating with stored coverage 1 and 8, transparent, update-only, compare-only, neither, and the far word), the aliased 16-bit and 32-bit color and Z images, the CPU, PI DMA and SP DMA invalidation windows, and reset. Mutant rows `v26-sidecar-invalidation-removed`, `v26-digest-omit-scalar` and `v26-digest-omit-hidden` and the two test sources are in `scripts/p10-mutants.py`; all three are KILLED.
- **Task 3 (`d8b638a`).** `v26-predicate-matrix` in `test_profile_v26.c` checks five predicates on 25 ids against a hand-written table and prints `rows=25`. The sweep against the Wave 2 base and the K1 tests are recorded in `PIN-IMPACT.md` section 2.2.
- **Task 4 (`57d38b2`).** `rdp.md` V7 prints the eight-row table (exponent, shift, add, decode, range) with citations by title, edition and page, names the four ids, states the evidence tier and the three new test names; `vi.md` gets a descriptor 13 section; `machine-contract.md` gets the K1 text and both table rows, the `t64.state.si.26` domain row and the v26 layout; `PROJECT.md` carries the K1 notes.
- **Independent literals.** A scratch calculator in the session scratchpad (outside the repo, not committed) implements the `z_format[8]` table and the V7 rules from the documents, asserts that the table tiles the 18-bit range, and reproduces every literal used here plus all 14 vectors of `test_rdp_depth.c` `case_depth_compare_update_and_coverage_modes`; no production helper was called and no oracle source was opened (D5).

## Verification

| Check | Result |
|-------|--------|
| `ctest -R '^unit\.core\.rdp\.depth'` | 4 of 4 pass (the oracle-bound `unit.core.rdp.depth` plus three new names) |
| `ctest -N -R '^unit\.core\.rdp\.depth'` / `'^unit\.core\.profile'` | 4 and 9 names (HEAD 1 and 8) |
| `ctest -R '^(unit\.core\.profile\.v26-predicate-matrix\|unit\.core\.k1\..*)$'` | 3 of 3 pass |
| mutants (final tree) | `v26-sidecar-invalidation-removed`, `v26-digest-omit-scalar`, `v26-digest-omit-hidden` KILLED; `k1-predicate-widened` KILLED; controls SURVIVED and INVALID as expected; `--check-anchors` exit 0 |
| digest sweep, Wave 2 base `6dd785986f6aa78c6da5573a558b15b386a4edee` (40-hex checked, `git cat-file -e`), `--head WORKTREE` | 1599 shared lines, 0 differences; with four `--head-extra-id` 292 head-only lines |
| `ctest --preset gate` (after Task 4; JUnit `build/gate/10-10/gate-final.xml`) | 244 of 244 pass, 106 s at host load above 12; `check-test-inventory.sh --preset gate` PASS count 244 |
| ASan (`ctest --preset asan`, the four new names) | pass (0.11, 0.17, 0.45, 0.13 s) |
| `sh scripts/check-docs.sh`, `sh scripts/pii-scan.sh --tree` | ok, findings 0 |
| `sh scripts/preflight-phase10.sh` (clean committed tree at `6a01929`) | PASS 86 s: 35 regex rows 0 mismatches, 20 phase10 labels, 224 units compiled with the act image cc 13.3.0, 11 post-Gate steps replayed, PII clean (host load 33.9, WARN) |
| new names, native seconds | 0.002 + 0.006 + 0.018 + 0.008 = 0.033 s (first gate run, `CTestCostData.txt`; none carries `COST`) |

## Task Commits

1. **Task 1: depth accepted effect on the descriptor 13 family (tracer)** - `4014766` (test)
2. **Task 2: wrong-depth control, Z opcodes, modes, sidecar writers and three mutant rows** - `9a5dfd8` (test)
3. **Task 3: predicate matrix over 25 ids and the Wave 2 shipped-id proof** - `d8b638a` (test)
4. **Task 4: Z exponent table, depth and K1 documentation** - `57d38b2` (docs)
5. **Alias cases of the VID-07 boundary edge** - `6a01929` (test)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing critical] Alias cases added to the sidecar case**
- **Found during:** Task 4 review of the plan truths
- **Issue:** the truth "VID-07 boundary edge (manually classified, flagged): the depth matrix covers the 16-bit and 32-bit alias cases and the far value" names the alias cases, but no task action lists them; the far word was covered, the aliases were not.
- **Fix:** `check_aliases` in `test_rdp_depth_composite.c` (command words and literals of `case_aliased_coverage_and_alpha_preserve_hidden_depth`): a coverage-only write on the aliased RGBA16 image keeps the word and entry 0x83 and records coverage 0x88, a real color write then clears it; an alpha-rejected RGBA32 write keeps bytes and entry, the opaque write stores (0, 255, 0, 255) and clears both halfword entries. A scratch copy with one literal changed fails on all four ids.
- **Files modified:** `core/tests/test_rdp_depth_composite.c`, `docs/spec/rdp.md` (one sentence)
- **Commit:** `6a01929`

**2. [Rule 3 - Premise] The mutants need a committed tree**
- **Found during:** Task 2
- **Issue:** `scripts/p10-mutants.py --run` refuses a tree with tracked changes outside `.planning/`, and builds its worktree from HEAD, so the three mutants cannot run in the task's own verify before the commit.
- **Fix:** they ran on `9a5dfd8` right after the commit (all KILLED, no fix commit needed) and again on the final tree.

**3. [Rule 2 - Added record] PIN-IMPACT.md section 2.2**
- **Issue:** the plan asks for the sweep summary in `PIN-IMPACT.md` "Wave 2 shipped-id proof", but the file is not in `files_modified` (the same situation as plan 10-09 deviation 7).
- **Fix:** section 2.2 is part of the Task 3 commit, as the 11th file. Commit `d8b638a`.

**4. [Rule 2 - Missing critical] State 26 digest row in machine-contract.md**
- **Issue:** the plan names the K1 text and table row of `machine-contract.md`; the identity bump of plan 10-09 also needs its domain row and stream layout in the same wave (CONTEXT [Docs]).
- **Fix:** the `t64.state.si.26` row and a v26 bullet (scalars before the work policy version, hidden table after the coverage table). Commit `57d38b2`.

**Total deviations:** 4 (2 Rule 2 behavior or doc additions, 1 Rule 2 record, 1 Rule 3). **Impact:** none on scope or pins; one file beyond the plan list and one commit beyond the four task commits.

## Review findings and observations (information only)

- **Info (coverage value, interpenetrating rows):** the two interpenetrating vectors depend only on the incoming coverage of the covered pixel lying in 2 to 8 (stored coverage 1 passes, stored 8 does not); the exact value comes from the rasterizer and is not independently derived here. The same holds for the pixel set of the triangle: a scratch probe saw 10 covered pixels in rows 1 to 4, which is consistent with YH 1 and YL 5 but is not used as an expected value, so the checks assert uncovered rows (sentinels) and the one covered pixel only.
- **Info (evidence scope of vi.md):** the new tests assert RDRAM color, depth and sidecar bytes, not VI packets; `vi.md` says the VI bytes of the new ids rest on the unchanged v9 code and its existing tests. A packet-level check on a descriptor 13 id belongs to RUN-07 (plan 10-14 onward).
- **Info (sweep command):** the plan's printed sweep command has no `--head-extra-id`, so it reports 0 head-only lines; both forms were run and both report 0 differences.
- **Info (stale log):** `build/gate/Testing/Temporary/LastTestsFailed.log` is not cleared by a fully passing run; it listed `65:unit.core.digest.v26-strip-differential` from an earlier session while ctest reported 244 of 244 passing and the test passes alone.
- **Info (unchanged tooling coverage):** `scripts/list-membership-sites.py` still has no tokens for the exact predicates (observation from plan 10-09); it was not edited.

## Issues Encountered

- A scratch probe linked without `libt64_sha256.a` failed once (undefined `t64_sha256_final`); the library is a link dependency of `t64_core`, which CMake adds for the real test.
- The shell `cat -A` and `date` flags differ on macOS; no effect on results.

## Authentication Gates

None.

## User Setup Required

None - no external service configuration required.

## Known Stubs

None.

## Threat Flags

None new. T-10-21 mitigated (the sidecar case covers CPU, PI and SP writes and reset on four ids; the invalidation mutant is killed), T-10-22 mitigated (rdp.md, vi.md and the tests' header state "unit-tested, authored scope", the logical sidecar and software-policy precedence).

## Next Phase Readiness

- Plans 10-11 and 10-12: the state 26 exporter and importer are still to come; the importer must read the five depth scalars inside the RDP block before the render proof and the hidden table in the tail, as `docs/spec/machine-contract.md` (v26 bullet) and `check_v26_depth_layout` fix. The depth-composite tests give a ready command list (`first_pass` in `test_rdp_depth_composite.c`) for a mid-primitive round trip.
- Plan 10-13: the sweep report is `build/gate/10-sweep/6dd785986f6a-worktree.txt` (local); later sweeps keep passing `--head-extra-id` for the four ids.
- Orchestrator items: (1) all commits of this plan are local and unpushed (plan 10-13 pushes); (2) `requirements.ready-ids` decides VID-07 (plans 10-09 and 10-10 both name it); (3) local scratch `build/gate/10-10` (gate JUnit and log, ASan JUnit, LastTest copy) and `build/gate/10-sweep` may be pruned; (4) the Gate-growth ledger row for Wave 2 is plan 10-13's, with this plan's four names costing 0.033 s native (first gate run) and 0.86 s under ASan.

---
*Phase: 10-carry-in-closure-before-the-freeze*
*Completed: 2026-11-24*

## Self-Check: PASSED

The new test file, the ten edited files and this file exist; the five plan commits (`4014766`, `9a5dfd8`, `d8b638a`, `57d38b2`, `6a01929`) are ancestors of HEAD; the acceptance criteria were re-run: `ctest -N -R '^unit\.core\.rdp\.depth'` reports 4, `^unit\.core\.profile` reports 9, the matrix prints `rows=25`, the wrong-depth control asserts the exact first-difference index, `grep -c NU6-06-0030-001G docs/spec/rdp.md` is 1, rdp.md lists exponents 0 to 7 with adds `0x00000` to `0x3f800`, and machine-contract.md names descriptor 13 depth-active and descriptors 11 and 12 depth-inactive; sweep, mutants, K1, ASan, docs checks and the full preflight passed on the final tree.
