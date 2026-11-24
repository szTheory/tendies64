---
phase: 10-carry-in-closure-before-the-freeze
plan: 07
subsystem: testing
tags: [vg-02, vg-03, vg-04, vg-05, vi-serrate, rgba5551-alpha, perspective-floor, shade-floor, bayer-dither, mutants, ctest, independent-literals]

requires:
  - phase: 10-carry-in-closure-before-the-freeze
    provides: plan 10-06 core/tests/test_vg_gaps.c case table, the PHASE10_TESTS inventory block, the regex count rows and the seven-row mutant set of scripts/p10-mutants.py
  - phase: 07-rdp-reference-renderer-and-vi-scanout
    provides: the VG-02 to VG-05 gap records and the survivor mutants of 07-VERIFICATION.md, the v8 and v9 VI policies (vi.md) and the texture, shade and blender policies (rdp.md)
provides:
  - four more cases in core/tests/test_vg_gaps.c (rgba5551-alpha-clear, serrate-filter-parity, perspective-floor, shade-floor-bayer)
  - CTest names unit.core.vg.{rgba5551-alpha-clear,serrate-filter-parity,perspective-floor,shade-floor-bayer} (labels unit;phase10), nine vg names in total
  - eight new mutant rows in scripts/p10-mutants.py, all KILLED by a named check of the new binary; the full table is 15 KILLED, one expected SURVIVED control and one expected INVALID control
affects: [10-08, 10-09, phase-11]

actuals:
  tokens: 10633
  tasks: 4
  commits: 4

tech-stack:
  added: []
  patterns:
    - "a VI vector seeds RGBA5551 rows at 0x3000, programs the short-field window (H_SYNC 15, V_SYNC 8: boundaries at ticks 83 and 166) and drains each packet before the next boundary so field 1 is observable"
    - "a vector is built so the correct operand pair is uniform and the expected literal is independent of the filter weights, while each wrong pairing gives a different literal"
    - "a boundary table is pinned by sweeping the input across all 8 values below the quantizer threshold in every cell, which fixes each table entry exactly"
    - "when production text is repeated in a sibling function, the mutant anchor starts at the unique function header and runs to the edit"

key-files:
  created: []
  modified:
    - core/tests/test_vg_gaps.c
    - core/CMakeLists.txt
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase10-regexes.txt
    - scripts/p10-mutants.py

key-decisions:
  - "All four vector groups pass against the current code on the first run: no VG-02, VG-03, VG-04 or VG-05 defect exists, so no fix plan is needed"
  - "The VG-04 pair S = -131073, W = 131072 is confirmed against the tile path (mask 3 wrap, period 8: floor selects texel 6, truncation texel 7) and kept; clamp would have made the two candidates coincide, so the tile uses wrap"
  - "VG-05 (b) calls t64_rdp_quantize_rgba5551_v6 directly on a blender-v6 machine (as test_rdp_blender.c does) and sweeps all 16 matrix cells over c = 72..79 rather than naming one pixel"
  - "The VG-04 and VG-05 floor mutants sit on the path each vector reaches (v4 texel function, shade_channel); text-identical sibling copies are not mutated (see Issues Encountered)"

patterns-established:
  - "Plans of the VG series add the case, add_test, PHASE10_TESTS line, regex row and mutant rows in one commit and run the kill after it"

requirements-completed: [VER-06]

coverage:
  - id: D1
    description: "VG-03: an RGBA5551 source pixel with bit 0 clear (0xf800) scans out as 255,0,0,0 through the v8 renderer and the v9 read function while every bit-0-set neighbor keeps alpha 255; the 48-byte literal is hand-derived and a wrong-value control reports the forced-255 literal at byte 3 on both paths"
    requirement: "VER-06"
    verification:
      - kind: unit
        ref: "ctest -R '^unit\\.core\\.vg\\.rgba5551-alpha-clear$' (1 of 1, 94 checks); p10-mutants vg03-alpha255-v9 and vg03-alpha255-v8 KILLED by test_vg_gaps:rgba5551-alpha-clear"
        status: pass
    human_judgment: false
  - id: D2
    description: "VG-02: a v9 packet with STATUS bit 6 (read back as 0x242), field 1 and a vertical fraction of 512 equals the uniform literal built from source rows 2n+1 and 2n+3 (198,74,247,255), field 0 gives the even-row literal (24,231,99,0), source_bytes are 64 and 56, and the parity-ignored, 2n+1/2n+2 blend (111,152,173,255) and one-byte controls are rejected at the intended byte"
    requirement: "VER-06"
    verification:
      - kind: unit
        ref: "ctest -R '^unit\\.core\\.vg\\.serrate-filter-parity$' (1 of 1, 83 checks); p10-mutants vg02-parity-ignored and vg02-next-row KILLED by test_vg_gaps:serrate-filter-parity"
        status: pass
    human_judgment: false
  - id: D3
    description: "VG-04: with S = -131073 (or T) and W = 131072 the exact quotient -65536.5 floors to -65537, which the mask-3 wrap tile maps to texel 6 (words 0x384d S lane, 0x09cd T lane) where truncation would select texel 7 (0x404f, 0x0a0f); two exact-quotient rows (-131072) show the correction needs a remainder"
    requirement: "VER-06"
    verification:
      - kind: unit
        ref: "ctest -R '^unit\\.core\\.vg\\.perspective-floor$' (1 of 1, 236 checks); p10-mutants vg04-floor-off-s and vg04-floor-off-t KILLED by test_vg_gaps:perspective-floor"
        status: pass
    human_judgment: false
  - id: D4
    description: "VG-05 (a): the shaded triangle words 0x3c17 at pixel (1,2) and 0x3bd7 at pixel (2,2) follow the floored major-displacement product (truncation gives 0x4417 and 0x3c17); (b) all 16 cells of the printed Bayer matrix quantize as the table says for c = 72..79 (128 values, each also at x+4, y+8) and the named pixel (2,1) at c = 8m-7 gives 0x5309 (a shift of 2 gives 0x4ac9)"
    requirement: "VER-06"
    verification:
      - kind: unit
        ref: "ctest -R '^unit\\.core\\.vg\\.shade-floor-bayer$' (1 of 1, 316 checks); p10-mutants vg05-shade-trunc and vg05-bayer-shift2 KILLED by test_vg_gaps:shade-floor-bayer"
        status: pass
    human_judgment: false
  - id: D5
    description: "All of VG-01 to VG-05 and the NTSC vector have independent literals and at least one killed mutant; the nine names are registered, inventoried and counted; oracle-bound files and production code are untouched"
    requirement: "VER-06"
    verification:
      - kind: integration
        ref: "ctest --preset gate 236 of 236 (88 s at load about 12); ctest --preset asan -R '^unit\\.core\\.vg\\.' 9 of 9; python3 -B scripts/p10-mutants.py --run (rc 0, 15 KILLED, control-equivalent-depth-cap SURVIVED as expected, control-no-compile INVALID as expected); sh scripts/preflight-phase10.sh PASS (36 s without replay at start, 66 s full at end); sh scripts/check-test-inventory.sh --preset gate and --selftest PASS; git diff --name-only 2a1ba27 HEAD lists five files, none oracle-bound and none under core/t64_*"
        status: pass
    human_judgment: false
  - id: D6
    description: "VER-06 precision edge: floor versus truncation is exercised on negative operands with nonzero remainders (VG-04 quotient -65536.5, VG-05 products -0.5 and -1.5), the only places the two differ, and the Bayer table is pinned in every cell"
    requirement: "VER-06"
    verification: []
    human_judgment: true
    rationale: "Whether the floor-versus-truncation and table edges are exhaustive is a classification the executor makes by reading the derivations; the mechanical evidence is the killed mutants, not a proof that no other site differs"

duration: 22min
completed: 2026-11-24
status: complete
commits: 4
plan_head_before: 2a1ba27ae64740c7de521a56952b7c6eb54de752
plan_head_after: 0491e4749da0c5be3ba86812fe3d6721a079a212
---

# Phase 10 Plan 07: VG-02 to VG-05 Summary

**Four more cases in the new test_vg_gaps binary pin the RGBA5551 alpha clear on both VI paths, the v9 serrate row pair with a weight-independent literal, the perspective floor on the S and T lanes, and the shade floor with every cell of the Bayer matrix, each from hand-derived literals, and eight new mutants are all KILLED; every vector passed against the current code, so no defect was found.**

## Performance

- **Duration:** about 22 min (about 2026-11-24T02:57Z to 03:19Z; commits at 22:02, 22:04, 22:08 and 22:13 local)
- **Tasks:** 4 (Task 1 tracer, Tasks 2 to 4 auto)
- **Files:** 5 modified (`core/tests/test_vg_gaps.c` 830 to 1459 lines, CMake registration, inventory block, regex rows, mutant driver); production code and the oracle-bound files untouched
- **Host:** 1-minute load 6.7 at the start, 13 at the end, so seconds are upper bounds; the four new names run in 0.00 s each at ctest resolution

## Accomplishments

- **Task 1 (tracer, `e45b433`).** `rgba5551-alpha-clear` scans the Phase 7 witness with pixel (0,0) changed to 0xf800 through the v8 profile (STATUS 0x02) and the v9 filter profile (STATUS 0x302: mode 2, anti-alias mode 3 nearest sample, no filter bit) and asserts the same 48-byte literal on both: pixel (0,0) is 255,0,0,0 and the bit-0-set neighbors keep alpha 255. `vg03-alpha255-v9` and `vg03-alpha255-v8` are KILLED. The tracer was re-run end to end before expansion (ctest 1 of 1, both mutants KILLED, rc 0).
- **Task 2 (`3a594ee`).** `serrate-filter-parity` on the v9 profile with STATUS 0x242 (bit 6 read back), a 4 x 8 source whose odd rows are X = 0xc27d and even rows Y = 0x1f18, and Y_SCALE offset 0.5 (vertical fraction 512). Field 1 gives X everywhere for any weight; field 0 gives Y; `source_bytes` 64 and 56 follow from the highest tap row (7 and 6). The wrong pairings (Y at byte 0; the 111,152,173,255 blend at byte 0) and a one-byte change are rejected at the intended byte. `vg02-parity-ignored` and `vg02-next-row` are KILLED.
- **Task 3 (`e3afa3e`).** `perspective-floor` on the texture-v4 profile: an 8 x 8 authored tile with mask 3 wrap, a constant-S/T/W perspective triangle. S = -131073 with W = 131072 has the exact quotient -65536.5; floor gives -65537 (texel 6 after the wrap) and truncation -65536 (texel 7). The T lane repeats on rows, and two exact-quotient rows (-131072) show that the correction needs a remainder. `vg04-floor-off-s` and `vg04-floor-off-t` are KILLED.
- **Task 4 (`0491e47`).** `shade-floor-bayer`: (a) a 0x0c triangle on the v3 profile where Dx = -1 on R and G (B is an exact control lane) gives 0x3c17 at pixel (1,2) and 0x3bd7 at pixel (2,2), where truncation gives 0x4417 and 0x3c17; (b) on the blender-v6 profile with RGB dither selector 1, every cell of the printed Bayer matrix is pinned for c = 72..79 and the named pixel (2,1) at c = 8m-7 gives 0x5309. `vg05-shade-trunc` and `vg05-bayer-shift2` are KILLED. Then the full mutant run, both controls, the preflight and the gate suite.

## Independent derivations (each is a comment above its case; the scratch check script lives outside the repository)

| Vector | Literal | Derivation |
|--------|---------|------------|
| RGBA5551 alpha | 255,0,0,0 for 0xf800 | channel 31 expands to (31 << 3) \| (31 >> 2) = 255; alpha bit 0 maps to 0 or 255 (vi.md) |
| serrate X, Y | 198,74,247,255 and 24,231,99,0 | 0xc27d is R5 24, G5 9, B5 30, alpha 1; 0x1f18 is R5 3, G5 28, B5 12, alpha 0; expand 24 -> 198, 9 -> 74, 30 -> 247, 3 -> 24, 28 -> 231, 12 -> 99 |
| serrate wrong blend | 111,152,173,255 | weights 2^19 each: floor((198+24)/2), floor((74+231)/2), floor((247+99)/2) |
| serrate source_bytes | 64 and 56 | highest row 7 (field 1) and 6 (field 0), column 3: (7*4+3+1)*2 and (6*4+3+1)*2 |
| perspective S | -131073 * 65536 / 131072 = -65536.5, floor -65537 | wrap period 8: texel 6; truncation -65536 gives texel 7 |
| texel words | 0x384d, 0x404f, 0x09cd, 0x0a0f | ((x+1) << 11) \| ((y+1) << 6) \| (((x+y) & 31) << 1) \| 1 at (6,0), (7,0), (0,6), (0,7) |
| shade | 0x3c17 and 0x3bd7 | R 4194304 - 1 -> 63 (5-bit 7), G 8388609 - 2 -> 127 (5-bit 15); truncation 64 and 128 |
| Bayer | 0x5309 | bias 7 at (2,1): 73 -> 10, 89 -> 12, 32 -> 4, alpha bit 1; shifted-by-2 bias 3 gives 0x4ac9 |

## Mutant results (`build/gate/10-p10-results.txt`, one `--run` of every row)

| Mutant | Class | Killed by |
|--------|-------|-----------|
| k1-predicate-widened | KILLED | test_k1_refusal:apply-refusal |
| vg01-decal-strict | KILLED | test_vg_gaps:decal-equality |
| vg01-zcap-7ffe | KILLED | test_vg_gaps:primitive-z-saturation |
| vg01-delta-floor | KILLED | test_vg_gaps:delta-ceiling |
| vg-zexp-decode-top (10-06) | KILLED | test_vg_gaps:z-exponent-boundaries |
| vg-zexp-compress-round (10-06) | KILLED | test_vg_gaps:z-exponent-boundaries |
| vi-clock-48600000 | KILLED | test_vg_gaps:ntsc-clock |
| vg03-alpha255-v9 | KILLED | test_vg_gaps:rgba5551-alpha-clear |
| vg03-alpha255-v8 | KILLED | test_vg_gaps:rgba5551-alpha-clear |
| vg02-parity-ignored | KILLED | test_vg_gaps:serrate-filter-parity |
| vg02-next-row | KILLED | test_vg_gaps:serrate-filter-parity |
| vg04-floor-off-s | KILLED | test_vg_gaps:perspective-floor |
| vg04-floor-off-t | KILLED | test_vg_gaps:perspective-floor |
| vg05-shade-trunc | KILLED | test_vg_gaps:shade-floor-bayer |
| vg05-bayer-shift2 | KILLED | test_vg_gaps:shade-floor-bayer |
| control-equivalent-depth-cap | SURVIVED (expected) | - |
| control-no-compile | INVALID (expected) | - |

## Task Commits

1. **Task 1: VG-03 alpha clear on the v8 and v9 paths (tracer)** - `e45b433` (test)
2. **Task 2: VG-02 v9 serrate parity** - `3a594ee` (test)
3. **Task 3: VG-04 perspective floor** - `e3afa3e` (test)
4. **Task 4: VG-05 shade floor and Bayer table** - `0491e47` (test)

**Plan metadata:** this SUMMARY and the STATE, ROADMAP and REQUIREMENTS updates follow in docs(10-07) commits.

## Files Created/Modified

- `core/tests/test_vg_gaps.c` - four cases appended to the case table, with the helpers `configure_short_vi`, `scan_out_5551`, `fill_packet`, `set_logical_texel16`, `authored_texel`, `run_perspective_triangle` and `run_shade_triangle` (copied idioms of the oracle-bound tests, never included)
- `core/CMakeLists.txt` - four `add_test` names added to the `unit;phase10` property block
- `scripts/check-test-inventory.sh` - four names in the `PHASE10_TESTS` block (236 names in the gate preset, 238 in the asan preset)
- `scripts/preflight-phase10-regexes.txt` - `^unit\.core\.vg\.` now 9, plus one row for each new name
- `scripts/p10-mutants.py` - eight mutant rows; constants `TMEM_V4_PERSPECTIVE_HEAD`, `TMEM_V4_S_FLOOR` and `TMEM_V4_T_FLOOR` for the anchors of the texel path

## Decisions Made

See key-decisions. The ones later plans need: the short-field VI window helper (`scan_out_5551`, boundaries at ticks 83 and 166) for any further VI vector; the sweep technique for table pins; the anchor-from-header technique when production text repeats.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Added vectors] Extra rows inside the planned cases**
- **Found during:** Tasks 2, 3 and 4
- **Issue:** the plan names a single vector per gap; a single row leaves a neighboring defect unkilled or a premise unchecked.
- **Fix:** VG-02 also asserts field 0 (the even-row literal), `source_bytes` 64 and 56 and the three-literal control; VG-04 adds two exact-quotient rows (the correction applies only when the remainder is nonzero); VG-05 (b) pins every cell of the Bayer matrix at c = 72..79 (128 values, each also at x+4, y+8) instead of one named pixel, keeping the named pixel (2,1).
- **Files modified:** `core/tests/test_vg_gaps.c`
- **Commits:** `3a594ee`, `e3afa3e`, `0491e47`

**2. [Rule 1 - Premise correction] The plan counts 13 KILLED mutants; the table has 15**
- **Found during:** Task 4
- **Issue:** the Task 4 criterion says 13 KILLED (k1 plus 12 VG/VI), but plan 10-06 added two rows beyond its list (vg-zexp-decode-top, vg-zexp-compress-round).
- **Fix:** none needed; the full run lists 15 KILLED, the expected SURVIVED control and the expected INVALID control, which satisfies the intent of the criterion.
- **Commit:** none (a count)

### Notes (not deviations)

- **VG-05 (b) calls the quantizer directly.** `t64_rdp_quantize_rgba5551_v6` is called on a blender-v6 machine with `other_modes` set to selector 1 (as `test_rdp_blender.c` does for its Bayer row), because the mutant sits in `color_dither_bias`, which only that quantizer reaches. The expected values are hex literals and a transcription of the printed matrix, not production output.
- **VG-04 uses a wrap tile.** The research example S = -131073, W = 131072 was confirmed (assumption A7), but a clamped tile maps both candidates to texel 0; the tile therefore uses mask 3 with clamp off, which the tile validation accepts for an 8 x 8 tile.
- **Mutant rows are committed with their test** (the driver builds from HEAD); each task's ctest ran before its commit and the kills after it, with no follow-up commit.

**Total deviations:** 2 (1 Rule 2, 1 Rule 1)
**Impact on plan:** none on scope, files or oracle-bound paths; the additions strengthen the recorded kills.

## Issues Encountered

- No vector failed: VG-02 to VG-05 behave exactly as `docs/spec/vi.md` and `docs/spec/rdp.md` say. No fix plan is requested.
- **Residual sibling copies (observation, not a defect).** The perspective floor text is repeated in `t64_rdp_tmem_triangle_pixel_rgba8` (combiner profiles v5 and later) and in `project_texture_q16` of `core/t64_rdp_raster.c` (the LOD path), and the `from_major` floor expression is repeated in `texture_channel_q16`. The VG-04 and VG-05 vectors run on the v4 texel path and on `shade_channel`, so these siblings are not mutated or pinned by this plan. The plan did not ask for them and this plan does not widen scope; plan 10-08 (Wave 1 review) or the orchestrator can decide whether a small follow-up vector is worth its cost.
- Host load rose from 6.7 to 13 during the run, so the 88 s gate time and the 66 s preflight are upper bounds.
- The ASan build prints the existing `-Wframe-larger-than` warning for `state_check_si` in `core/t64_state.c` (not touched by this plan).

## Authentication Gates

None.

## User Setup Required

None - no external service configuration required.

## Known Stubs

None.

## Threat Flags

None new. T-10-15 mitigated (every literal has a derivation comment from vi.md or rdp.md and a scratch exact-arithmetic check outside the repository; VG-02 is weight-independent; each vector reaches its comparator, proved by a killed mutant). T-10-16 mitigated (new binary only; `git diff --name-only 2a1ba27 HEAD` lists five files, none of the oracle-bound test files, `docs/spec/rdp-vi-cases.json` or `core/t64_*`, and the preflight oracle-authority step passes).

## Next Phase Readiness

- Plan 10-08 (Wave 1 review) should re-derive the literals above independently, run the review in a clone detached at the head (W0-01), and add the Wave 1 gate-growth row: 236 names (232 before 10-07, 227 at the Wave 0 base), the four new names at 0.00 s native each, full gate suite 236 of 236 in 88 s at load about 12, ASan vg 9 of 9.
- VER-06 is complete in the plans: VG-01 to VG-05 and the NTSC vector each have independent literals and at least one killed mutant. `requirements.ready-ids` decides whether the checkbox flips.
- Orchestrator items: (1) the eight test and docs commits of plans 10-06 and 10-07 are local and unpushed (plan 10-08 pushes); (2) `build/gate/10-p10-results.txt` holds the mutant table above; (3) decide whether the sibling copies named under Issues Encountered need a follow-up vector.

---
*Phase: 10-carry-in-closure-before-the-freeze*
*Completed: 2026-11-24*

## Self-Check: PASSED

`core/tests/test_vg_gaps.c` and the four other modified files exist; the four task commits (`e45b433`, `3a594ee`, `e3afa3e`, `0491e47`) are ancestors of HEAD and the evaluation scope lists them; the plan-level checks passed on the committed tree: `ctest -R '^unit\.core\.vg\.'` 9 of 9, `python3 -B scripts/p10-mutants.py --run` (every mutant as expected, both controls as expected), `sh scripts/preflight-phase10.sh` PASS (66 s), `sh scripts/check-test-inventory.sh --selftest` PASS, `ctest --preset gate` 236 of 236, ASan vg 9 of 9.
