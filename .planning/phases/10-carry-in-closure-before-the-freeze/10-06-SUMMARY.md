---
phase: 10-carry-in-closure-before-the-freeze
plan: 06
subsystem: testing
tags: [vg-01, rdp-depth, z-exponent-table, vi-clock, ntsc, mutants, ctest, independent-literals]

requires:
  - phase: 10-carry-in-closure-before-the-freeze
    provides: plan 10-01 preflight, PHASE10_TESTS inventory block, regex count rows and the scripts/p10-mutants.py driver; plan 10-05 Wave 1 base f6ab8c2
  - phase: 07-rdp-reference-renderer-and-vi-scanout
    provides: descriptor 10 depth policy (rdp.md V7), the eight-row Z exponent table (07-08-DEPTH-FIELDS.md) and the VG-01 and NTSC clock gap records (07-VERIFICATION.md)
provides:
  - core/tests/test_vg_gaps.c, a new binary with five cases (decal-equality, primitive-z-saturation, delta-ceiling, z-exponent-boundaries, ntsc-clock)
  - CTest names unit.core.vg.{decal-equality,primitive-z-saturation,delta-ceiling,z-exponent-boundaries,ntsc-clock} (labels unit;phase10)
  - seven new mutant rows in scripts/p10-mutants.py, all KILLED by a named check of the new binary
affects: [10-07, 10-08, 10-09, phase-11]

actuals:
  tokens: 11191
  tasks: 4
  commits: 4

tech-stack:
  added: []
  patterns:
    - "a new test binary copies the helpers of an oracle-bound test file and never includes or edits it"
    - "a case table with --case and --list-cases, one CTest name per case, expected values as hex literals with their derivation in the comment"
    - "numeric edges pinned on the boundary and one step either side, with a same-setup wrong-value control that reports its first difference at the intended byte"
    - "a decode proof without a decode routine: decal probes at decoded -2 .. +2 with tolerance 1 fix the decoded value to the unit"
    - "a VI vector enables the VI at machine tick 0 before the guest runs, because the authored guest programs and enables the VI itself"

key-files:
  created:
    - core/tests/test_vg_gaps.c
  modified:
    - core/CMakeLists.txt
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase10-regexes.txt
    - scripts/p10-mutants.py

key-decisions:
  - "All five vectors pass against the current code on the first run: no VG-01, Z-table or NTSC-clock defect exists, so no fix plan is needed and descriptor 10 may carry depth into Wave 2"
  - "The decode of the exponent table is proved with decal probes (tolerance 1) rather than opaque compares, because an opaque compare passes every nearer value and cannot fail on one"
  - "Two mutant rows beyond the plan list (vg-zexp-decode-top, vg-zexp-compress-round) give the exponent case a recorded failing direction; the decode mutant survives the existing test_rdp_depth"
  - "The primitive-Z cap mutant is also killed by the existing test_rdp_depth (recorded exit 1, as plan 10-01 observed), so the recorded kill is the named check of primitive-z-saturation"

patterns-established:
  - "Plans of the VG series append a case to core/tests/test_vg_gaps.c, a CTest name, an inventory line, a regex row and a mutant row in one commit"
  - "A mutant row is committed together with its test, because the driver builds the throwaway worktree from HEAD; the kill run follows the commit"

requirements-completed: [VER-06]

coverage:
  - id: D1
    description: "VG-01 (a): decal comparison at equality on descriptor 10 - old primitive Z raw 0x2000 dZ 9 stores 0x1000 with hidden 0x83; a new primitive dZ 2 passes at raw 0x2001 (difference 8) and 0x1fff, fails at 0x2002 and 0x1ffe (difference 16); depth is never updated"
    requirement: "VER-06"
    verification:
      - kind: unit
        ref: "ctest -R '^unit\\.core\\.vg\\.decal-equality$' (1 of 1); p10-mutants vg01-decal-strict KILLED by test_vg_gaps:decal-equality"
        status: pass
    human_judgment: false
  - id: D2
    description: "VG-01 (b): primitive Z saturation - raw 0x7ffe, 0x7fff, 0x8000 and 0xffff with dZ 0 store 0xffc0, 0xffe0, 0xffe0 and 0xffe0"
    requirement: "VER-06"
    verification:
      - kind: unit
        ref: "ctest -R '^unit\\.core\\.vg\\.primitive-z-saturation$' (1 of 1); p10-mutants vg01-zcap-7ffe KILLED by test_vg_gaps:primitive-z-saturation"
        status: pass
    human_judgment: false
  - id: D3
    description: "VG-01 (c): per-pixel delta ceiling - abs(dzdx)+abs(dzdy) of 8193 gives delta 2 and code 1, 8192 and 8191 give code 0, mixed signs, 24577 gives code 2 and the 0x7fffffff pair clamps to 65535 (visible 0xffff, hidden 0x83)"
    requirement: "VER-06"
    verification:
      - kind: unit
        ref: "ctest -R '^unit\\.core\\.vg\\.delta-ceiling$' (1 of 1); p10-mutants vg01-delta-floor KILLED by test_vg_gaps:delta-ceiling"
        status: pass
    human_judgment: false
  - id: D4
    description: "The 17 Z exponent boundary values store visible words 0x0000 .. 0xfffc as hex literals, each decoded value is fixed to the unit by 81 decal probes, and the far word 0xfffc decodes to 0x3ffff, without calling any production compression routine"
    requirement: "VER-06"
    verification:
      - kind: unit
        ref: "ctest -R '^unit\\.core\\.vg\\.z-exponent-boundaries$' (1 of 1, printed count line: 17 visible-word literals, 17 decoded-value literals, 81 decode probes, 8 primitive-Z rows); grep -cE '(z_compress|z_decompress)[[:space:]]*\\(' core/tests/test_vg_gaps.c prints 0; p10-mutants vg-zexp-decode-top and vg-zexp-compress-round KILLED"
        status: pass
    human_judgment: false
  - id: D5
    description: "Standard NTSC VI clock vector - H_SYNC 3093 and V_SYNC 525 give the first field boundary exactly 1,042,709 ticks after the enabling write (none at 1,042,708) and the first sampled line at tick 3973 (none at 3972)"
    requirement: "VER-06"
    verification:
      - kind: unit
        ref: "ctest -R '^unit\\.core\\.vg\\.ntsc-clock$' (1 of 1); p10-mutants vi-clock-48600000 KILLED by test_vg_gaps:ntsc-clock"
        status: pass
    human_judgment: false
  - id: D6
    description: "The five names are registered, inventoried and counted, the oracle-bound files are untouched, and the preflight, the full gate suite and the ASan build pass"
    requirement: "VER-06"
    verification:
      - kind: integration
        ref: "ctest --preset gate 232 of 232; ctest --preset asan -R '^unit\\.core\\.vg\\.' 5 of 5; sh scripts/preflight-phase10.sh PASS (28 s at start, 63 s at end); sh scripts/check-test-inventory.sh --preset gate and --selftest PASS; git diff --name-only 74ae659 HEAD lists none of the 13 oracle-bound paths"
        status: pass
    human_judgment: false
  - id: D7
    description: "VER-06 boundary edge: every vector sits exactly on its boundary and one step either side (tolerance 8 vs 16, delta 8192 vs 8193, raw 0x7fff vs 0x8000, deadline 1,042,709 vs 1,042,708)"
    requirement: "VER-06"
    verification: []
    human_judgment: true
    rationale: "Whether a vector set is complete at its boundaries is a classification the executor makes by reading the derivations; the mechanical evidence is the killed mutants, not a proof that no other edge exists"

duration: 22min
completed: 2026-11-24
status: complete
commits: 4
plan_head_before: 74ae6594bb1b23adcd433edf3d42353b0ab5b141
plan_head_after: 362afff7b7fafa51c17b905466ab27b9b5d0ecf0
---

# Phase 10 Plan 06: VG-01, Z exponent boundaries and the NTSC VI clock vector Summary

**A new test binary pins VG-01 (decal equality, primitive-Z saturation, ceiling delta) on descriptor 10, all 17 Z exponent range edges with the 0xfffc far word, and the standard-NTSC field deadline of 1,042,709 ticks, each from hand-derived hex literals, and seven mutants (four named by the plan plus three added) are all KILLED by a named check of that binary; every vector passed against the current code, so no defect was found.**

## Performance

- **Duration:** about 22 min (about 2026-11-24T02:33Z to 02:55Z; the start was not stamped at the first command, so it is read from the first preflight run and the commit times 21:41 to 21:49 local)
- **Tasks:** 4 (Task 1 tracer, Tasks 2 to 4 auto)
- **Files:** 1 created (`core/tests/test_vg_gaps.c`, 830 lines), 4 modified (CMake registration, inventory block, regex rows, mutant driver)
- **Host:** 1-minute load 6.9 to 8.6 throughout, so local seconds are upper bounds; the five new tests add 0.25 s of native test time

## Accomplishments

- **Task 1 (tracer, `7edef46`).** The new binary, the case-table idiom, helpers copied from the oracle-bound depth test (never included or edited), CTest `unit.core.vg.decal-equality`, the inventory line, the count rows and mutant `vg01-decal-strict` work end to end. The tracer was re-verified end to end before expansion (ctest 1 of 1, mutant KILLED). Derivation: the old primitive Z raw 0x2000 with dZ 9 is linear 0x10000, stored visible 0x1000, hidden 0x83, decoded delta 8; a new primitive dZ 2 gives tolerance max(2, 8) = 8, so raw 0x2001 (difference 8) passes decal and raw 0x2002 (difference 16) fails, and so do 0x1fff (8) and 0x1ffe (16) on the lower side. The control reports its first difference at byte 3.
- **Task 2 (`962a5b8`).** `primitive-z-saturation` stores 0xffc0, 0xffe0, 0xffe0, 0xffe0 for raw 0x7ffe, 0x7fff, 0x8000, 0xffff (derived from `min(raw, 0x7fff) << 3` and exponent 7). `delta-ceiling` drives a Z plane through nine coefficient pairs and asserts the stored delta code: 8193 gives code 1 (hidden 0x81), 8192 and 8191 and 0 give code 0, both mixed-sign pairs of absolute sum 8193 give code 1 (one of them also exercises the lossy linear 0x3f, visible 0x0000), 24577 gives code 2, 24576 gives code 1, and the 0x7fffffff pair clamps to delta 65535, code 15, visible 0xffff, hidden 0x83. Mutants `vg01-zcap-7ffe` and `vg01-delta-floor` are KILLED.
- **Task 3 (`f21658c`).** `z-exponent-boundaries` produces each of the 17 linear values 0x3f to 0x3ffff exactly (a plane with base = linear * 8192 and zero slopes; the eight multiples of 8 also through primitive Z), asserts the 17 visible words 0x0000, 0x0004, 0x1ffc, 0x2000, 0x3ffc, 0x4000, 0x5ffc, 0x6000, 0x7ffc, 0x8000, 0x9ffc, 0xa000, 0xbffc, 0xc000, 0xdffc, 0xe000, 0xfffc, and fixes the decoded value of each (0x00000 to 0x3ffff) with 81 decal probes at decoded -2 .. +2. It prints one count line and asserts the 0xfffc far word decodes to 0x3ffff. The case never calls a production compression routine (the grep gate prints 0). Two mutant rows were added to give it a failing direction (deviation 1).
- **Task 4 (`362afff`).** `ntsc-clock` programs H_SYNC 3093 and V_SYNC 525 on the v8 machine, enables the VI at machine tick 0 and asserts: no packet at tick 1,042,708 and exactly one packet whose end tick is 1,042,709 and whose start tick is the enabling tick; and, from the same clock, the VI interrupt and VI_CURRENT = 2 appear at tick 3973 and not 3972. Mutant `vi-clock-48600000` is KILLED.

## Independent derivations (each is a comment above its case, never a production output)

| Vector | Literal | Derivation |
|--------|---------|------------|
| decal tolerance | 8 | max(new delta 2, stored delta 1 << floor(log2 9) = 8) |
| decal equality | raw 0x2001 passes, 0x2002 fails | linear 0x10008 and 0x10010 against stored decode (0x400 << 6) = 0x10000 |
| saturation | 0xffc0 / 0xffe0 | exponent 7 mantissa linear - 0x3f800 = 0x7f0 / 0x7f8, shifted by 2, plus 0xe000 |
| ceiling | 8193 -> code 1 | ceil(8193 / 8192) = 2, floor(log2 2) = 1 |
| exponent table | 17 words and 17 decodes | the printed eight-row table, rows tile 0 .. 0x3ffff exactly |
| NTSC field | 1,042,709 | 812,175 * 62,500,000 / 48,681,812 = 1,042,708.4657..., ceiling (exact rational arithmetic, scratch script, not committed) |
| NTSC line 2 | 3973 | 3094 * 62,500,000 / 48,681,812 = 3972.2227..., ceiling |

## Mutant results (`build/gate/10-p10-results.txt`, one `--run` of every row)

| Mutant | Class | Killed by | Also killed by existing test_rdp_depth |
|--------|-------|-----------|-----------------------------------------|
| vg01-decal-strict | KILLED | test_vg_gaps:decal-equality | not recorded (the 07 record says it survives) |
| vg01-zcap-7ffe | KILLED | test_vg_gaps:primitive-z-saturation | yes (exit 1, as plan 10-01 observed) |
| vg01-delta-floor | KILLED | test_vg_gaps:delta-ceiling | not recorded |
| vi-clock-48600000 | KILLED | test_vg_gaps:ntsc-clock | not applicable |
| vg-zexp-decode-top (added) | KILLED | test_vg_gaps:z-exponent-boundaries | no (exit 0: survives the existing test) |
| vg-zexp-compress-round (added) | KILLED | test_vg_gaps:z-exponent-boundaries | yes (exit 1) |
| k1-predicate-widened | KILLED | test_k1_refusal:apply-refusal | not applicable |
| control-equivalent-depth-cap | SURVIVED (expected) | - | - |
| control-no-compile | INVALID (expected) | - | - |

## Task Commits

1. **Task 1: VG tracer, decal equality** - `7edef46` (test)
2. **Task 2: primitive-Z saturation and delta ceiling** - `962a5b8` (test)
3. **Task 3: Z exponent table boundaries and the far word** - `f21658c` (test)
4. **Task 4: standard-NTSC VI clock vector** - `362afff` (test)

**Plan metadata:** this SUMMARY and the STATE, ROADMAP and REQUIREMENTS updates follow in docs(10-06) commits.

## Files Created/Modified

- `core/tests/test_vg_gaps.c` - new; helpers copied from `test_rdp_depth.c` and `test_vi.c`, five cases, `--case` and `--list-cases`
- `core/CMakeLists.txt` - executable `test_vg_gaps` (links `t64_core t64_sha256`, depends on `t64-rdp-fill-fixture`) and five `unit.core.vg.*` names with labels `unit;phase10` and the fill-ROM and IPL3 environment
- `scripts/check-test-inventory.sh` - five names in the `PHASE10_TESTS` block (232 names in the gate preset)
- `scripts/preflight-phase10-regexes.txt` - `^unit\.core\.vg\.` now 5, plus rows for `decal-equality`, `z-exponent-boundaries` and `ntsc-clock`
- `scripts/p10-mutants.py` - `test_vg_gaps` in `TEST_SOURCES` and seven mutant rows

## Decisions Made

See key-decisions. The ones later plans need: the case-table idiom and the commit-with-test rule for mutant rows (10-07 appends VG-02 to VG-05 the same way); the guest-programs-VI constraint for any VI vector; and that descriptor 10 depth and the Z table already behave as the primary documents say.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing critical] Task 3 had no recorded failing direction**
- **Found during:** Task 3
- **Issue:** the plan lists four mutant rows and none for the exponent case, but AUTONOMY section 2 makes the killed mutant the failing direction of a test that starts green.
- **Fix:** two rows added to `scripts/p10-mutants.py`: `vg-zexp-decode-top` (the exponent 7 decode add 0x3f800 raised to 0x3f801) and `vg-zexp-compress-round` (mantissa floor written as round-half-up). Both are KILLED by `z-exponent-boundaries`; the decode mutant survives the existing `test_rdp_depth`, so the new case closes a gap the old test leaves.
- **Files modified:** `scripts/p10-mutants.py`
- **Verification:** `python3 -B scripts/p10-mutants.py --run` classifies both KILLED
- **Commit:** `f21658c`

**2. [Rule 1 - Premise correction] "fail one step nearer in opaque mode" cannot fail**
- **Found during:** Task 3
- **Issue:** in opaque mode a pixel passes when `newZ <= oldZ + maxDZ`, so a nearer value always passes; the plan sentence cannot be satisfied as worded.
- **Fix:** the decode proof uses decal mode, whose rule is `abs(newZ - oldZ) <= maxDZ` (rdp.md V7): with the stored word as the old depth, no hidden byte and a zero-slope plane, the tolerance is 1, so probes at decoded -1, 0 and +1 pass and at -2 and +2 fail. That fixes each decoded literal to the unit (81 probes, the 4 out of range removed).
- **Files modified:** `core/tests/test_vg_gaps.c`
- **Commit:** `f21658c`

**3. [Rule 1 - Premise correction] The VI vector enables at tick 0**
- **Found during:** Task 4
- **Issue:** the plan says to enable the VI at "a recorded tick T0"; the authored guest `fixtures/rdp/fill.S` programs and enables the VI itself within its first few hundred ticks, so an enable after advancing the guest would collide with the guest's own writes.
- **Fix:** the VI is enabled at machine tick 0 before the guest runs and T0 is read from the machine (`t0`), asserted equal to the packet's start tick. The guest's later register writes are latched and cannot change the first boundary, which is computed from the active registers.
- **Files modified:** `core/tests/test_vg_gaps.c`
- **Commit:** `362afff`

**4. [Rule 2 - Added vectors] Extra rows inside the planned cases**
- **Found during:** Tasks 1, 2 and 4
- **Issue:** the plan names the boundary and one step either side for each edge; some single-sided rows would leave a mutant unkilled.
- **Fix:** decal equality also tests the lower side (raw 0x1fff passes, 0x1ffe fails); the ceiling case adds two mixed-sign pairs (absolute value of each lane) and 24577 and 24576 (code 2 against code 1, where a floor changes the code again); the NTSC case adds the first sampled line (tick 3973, interrupt and VI_CURRENT = 2) from the same clock.
- **Commit:** `7edef46`, `962a5b8`, `362afff`

### Notes (not deviations)

- **Mutant rows are committed with their test.** The driver builds its throwaway worktree from HEAD, so a kill can only be shown after the test is committed; each task's ctest verify ran first and its mutant verify after the commit (all passed, no follow-up commit was needed).
- **Stored delta codes, not deltas.** The depth word stores only the priority code, so 8191, 8192 and 0 all store code 0 and cannot be told apart from each other; the effective edges of the ceiling are the rows 8192 (kills `+ 8192`) and 8193 (kills a floor or `+ 8190`). The 8191 and 0 rows pin the code-0 side and the clamp, not an independent edge.

**Total deviations:** 4 (2 Rule 2, 2 Rule 1)
**Impact on plan:** none on scope, files or oracle-bound paths; the additions strengthen the recorded kills.

## Issues Encountered

- No vector failed: VG-01, the Z exponent table and the NTSC field clock behave exactly as `docs/spec/rdp.md`, the 07-08 table and `docs/spec/vi.md` say. No fix plan is requested.
- Host load stayed between 6.9 and 8.6 (other sessions), so the seconds are upper bounds.
- The ASan build prints one existing `-Wframe-larger-than` warning for `state_check_si` in `core/t64_state.c` (not touched by this plan).

## Authentication Gates

None.

## User Setup Required

None - no external service configuration required.

## Known Stubs

None.

## Threat Flags

None new. T-10-13 mitigated (every literal has a derivation comment from rdp.md, vi.md or the 07-08 table; the grep gate for production compression helpers prints 0). T-10-14 mitigated (new binary only; `git diff --name-only 74ae659 HEAD` lists five files, none of the 13 oracle-bound paths, and the preflight oracle-authority step passes).

## Next Phase Readiness

- Plan 10-07 (VG-02 to VG-05) appends cases to the same table: add the case, the `add_test` line, the inventory line, the regex row (`^unit\.core\.vg\.` is 5 now) and the mutant rows in one commit, then run the kill after the commit.
- Plan 10-08 (Wave 1 review) should re-derive the literals above independently, run the review in a clone detached at the head (W0-01), and add the Wave 1 row to the Gate-growth ledger: 232 names (227 before), native test time +0.25 s for the five names, full gate suite 232 of 232 in 89 s at load about 7.
- Wave 2 may enable depth under the new registered ids: VG-01 is closed on descriptor 10 and no defect is open.
- Orchestrator items: (1) the four test commits and the docs(10-06) commits are local and unpushed (plan 10-08 pushes); (2) `build/gate/10-p10-results.txt` holds the mutant table above; (3) `requirements.ready-ids` decides whether VER-06 can be checked off (VG-02 to VG-05 remain in plan 10-07).

---
*Phase: 10-carry-in-closure-before-the-freeze*
*Completed: 2026-11-24*

## Self-Check: PASSED

`core/tests/test_vg_gaps.c` and the four modified files exist; the four task commits (`7edef46`, `962a5b8`, `f21658c`, `362afff`) are ancestors of HEAD and the evaluation scope lists them; the plan-level checks passed on the committed tree: `ctest -R '^unit\.core\.vg\.'` 5 of 5, `python3 -B scripts/p10-mutants.py --run` (every mutant as expected, both controls as expected), `sh scripts/preflight-phase10.sh` PASS (63 s), `sh scripts/check-test-inventory.sh --selftest` PASS, `ctest --preset gate` 232 of 232, ASan 5 of 5.
