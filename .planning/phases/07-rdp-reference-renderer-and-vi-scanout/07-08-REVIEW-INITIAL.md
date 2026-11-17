---
phase: 07-rdp-reference-renderer-and-vi-scanout
reviewed: 2026-11-17T17:36:22Z
depth: standard
files_reviewed: 18
files_reviewed_list:
  - .planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-08-EXECUTION-EVIDENCE.md
  - core/CMakeLists.txt
  - core/include/tendies64/t64_machine.h
  - core/t64_digest.c
  - core/t64_internal.h
  - core/t64_machine.c
  - core/t64_rdp.c
  - core/t64_rdp_depth.c
  - core/t64_rdp_pipeline.c
  - core/t64_rdp_raster.c
  - core/tests/test_profile.c
  - core/tests/test_rdp_blender.c
  - core/tests/test_rdp_depth.c
  - docs/spec/rdp-vi-cases.json
  - docs/spec/rdp.md
  - scripts/check-rsp-no-hle.py
  - scripts/check-test-inventory.sh
  - scripts/tests/test_rsp_no_hle.py
findings:
  critical: 1
  warning: 3
  info: 0
  total: 4
  necessity: 0
status: issues_found
---

# Phase 07-08: Code Review Report

**Reviewed:** 2026-11-17T17:36:22Z  
**Depth:** standard  
**Files Reviewed:** 18  
**Status:** issues_found

## Summary

Reviewed the Plan 07-08 source, tests, specification, and retained execution evidence, tracing depth metadata through the shared RDP invalidation path and the accepted command consumers. One reachable RDP state corruption and three gaps in the promised test discrimination/refusal coverage need attention. No source or test files were changed, and I did not run builds or tests.

The evidence records the 11 owning normal cases and 16 affected normal cases passing, but the saved aggregate 16-case `LastTest` output was overwritten, so that aggregate cannot be independently confirmed from the retained log. The corrected accepted-triangle run did render color but had depth `0x09a8` and hidden-depth `0x83` failures; the initial setup refusal was not a RED run, and implementation scaffolding predates the accepted RED. I make no hardware-accuracy or VID04 claim; D2, D5, and D6 remain stops.

## Narrative Findings (AI reviewer)

### CR-01: BLOCKER — Coverage-only RDP output erases aliased depth metadata

**File:** `core/t64_rdp_pipeline.c:252-272`; called from `core/t64_rdp_raster.c:809-829`  
**Issue:** `t64_rdp_memory_coverage_invalidate()` clears `rdp_depth_hidden` for any invalidated RDRAM granule. The v7 raster path invokes it before writing coverage even when `write_color` is false. A valid color-on-coverage or alpha-rejected fragment can update coverage while suppressing both color and depth writes. Since accepted SetColorImage and SetZImage commands do not require non-overlapping ranges (`core/t64_rdp.c:471-505`), a stream can point both images at the same granule; the coverage-only fragment then clears the existing hidden depth value despite no depth write. Subsequent depth comparison can see a different low-dZ value and reject a fragment that would otherwise pass.
**Fix:** Split color/coverage invalidation from depth-sidecar invalidation. Clear hidden depth only after an actual guest RAM write to the depth granule (or a permitted depth update), not when coverage metadata alone changes.

### WR-01: WARNING — Z triangle tests do not independently prove every coefficient lane or packet layout

**File:** `core/tests/test_rdp_depth.c:344-398, 497-523`  
**Issue:** The opcode `0x0f` fixture appends texture words before shade words (`374-381`), while the documented packet layout places shade coefficients before texture coefficients and Z/DZ last. It therefore does not exercise a valid textured-and-shaded `0x0f` payload, although the assertions pass. Its packet-length check also compares against `t64_rdp_triangle_word_count()`, the production helper. The separate Q16 vector checks only the final compressed depth/hidden values; its Z fraction is zero, and the documented coefficient perturbations for Dx/De collapse to the same compressed visible result, so those assertions do not discriminate those lanes after compression.
**Fix:** Assemble each packet in documented field order and assert literal lengths independently of production helpers. Use per-lane signed/fractional vectors whose removal or swap changes the expected compressed visible or hidden result, including a nonzero Z fraction.

### WR-02: WARNING — Compare/update matrix omits the disabled/disabled combination

**File:** `core/tests/test_rdp_depth.c:587-677`  
**Issue:** The test covers compare+update (`0x34`), compare-off/update-on (`0x24`), and compare-on/update-off (`0x14`), but never compare-off/update-off (`0x04`). Plan 07-08 explicitly calls for all four combinations, so a regression in that mode pair is not detected.
**Fix:** Add an accepted `0x04` case with independent expected color, visible depth, and hidden-depth values.

### WR-03: WARNING — Refusal snapshots and recovery do not cover the promised state

**File:** `core/tests/test_rdp_depth.c:748-817`  
**Issue:** The main truncated-packet check snapshots color and visible depth, then checks hidden depth, but not memory coverage or RNG state (`756-769`). Prefix cases do check coverage but start fresh and do not snapshot RNG after prior successful work (`771-797`). The out-of-bounds case checks only two color bytes and one hidden byte, omitting visible depth, coverage, and RNG (`807-817`). No refusal path is followed by a valid command on that same machine, despite the recovery claim. This leaves the plan's no-partial-mutation and post-refusal recovery contract incompletely tested.
**Fix:** After a successful baseline, snapshot all affected color, coverage, visible/hidden depth, and RNG state around each refusal; assert every snapshot is unchanged, then run a valid command and verify normal output on the same instance.

---

_Reviewed: 2026-11-17T17:36:22Z_  
_Reviewer: gsd-code-reviewer_  
_Depth: standard_
