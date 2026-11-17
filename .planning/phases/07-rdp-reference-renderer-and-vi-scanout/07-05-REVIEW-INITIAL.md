---
phase: 07-rdp-reference-renderer-and-vi-scanout
reviewed: 2026-11-17T08:53:22Z
depth: standard
files_reviewed: 15
files_reviewed_list:
  - core/CMakeLists.txt
  - core/t64_digest.c
  - core/t64_internal.h
  - core/t64_isviewer.c
  - core/t64_machine.c
  - core/t64_rdp.c
  - core/t64_rdp_raster.c
  - core/t64_rdp_tmem.c
  - core/tests/test_rdp_texture.c
  - docs/spec/rdp-vi-cases.json
  - docs/spec/rdp.md
  - runner/t64_real.c
  - scripts/check-rsp-no-hle.py
  - scripts/check-test-inventory.sh
  - scripts/tests/test_rsp_no_hle.py
findings:
  critical: 2
  warning: 2
  info: 0
  total: 4
  necessity: 0
status: issues_found
---

# Phase 07-05: Code Review Report

**Reviewed:** 2026-11-17T08:53:22Z  
**Depth:** standard  
**Files Reviewed:** 15  
**Status:** issues_found

## Summary

Reviewed the complete 15-file change and traced fetched texture-triangle commands through packet acceptance, preflight, rasterization, TMEM sampling, state/digest handling, VI readback, and guard scripts. Two reachable pixel-correctness defects need fixes. Two warnings describe missing tests that leave coefficient decoding and whole-triangle refusal behavior under-specified. The recorded D-11 signed-Q16 and rounding policies are treated as software policy, not hardware claims. The parent-reported normal gate, targeted sanitizer cases, and source-guard mutations passed; those checks do not cover the findings below.

## Narrative Findings (AI reviewer)

### Critical Issues

#### CR-01: Texture triangle ignores the packet's primitive tile

**Severity:** BLOCKER  
**File:** `core/t64_rdp_raster.c:235-251,421-423,445-466,566-590`; dispatch at `core/t64_rdp.c:377-399`

**Issue:** The accepted 0x0A/0x0E W0 header includes a 3-bit tile field (bits 50–48; [SGI RDP Command Summary v2.0, Table 12](https://ultra64.ca/files/documentation/silicon-graphics/SGI_RDP_Command_Summary.pdf)). The dispatch passes the packet to triangle bounds without rejecting a nonzero field, but bounds preflight always validates tile 0. Raster sampling also starts at tile 0, and LOD levels are derived from tile indices beginning at 0. Thus a valid packet naming tile 1 can be refused because tile 0 is invalid, or accepted and rendered from tile 0 instead of its named primitive tile. The RAW CONTRACT specifies that LOD selection uses `prim_tile + l_tile` and refuses only indices outside 0..7.

**Fix:** Decode the primitive tile from W0 and pass it through preflight, base sampling, and LOD selection. Validate the selected tile and its reachable LOD descriptors before any pixel effects. Add fixtures where tile 0 and the named nonzero tile contain different texels, plus a case where only the named tile is valid.

#### CR-02: Filtered one-bit alpha rounds up instead of following the floor policy

**Severity:** BLOCKER  
**File:** `core/t64_rdp_tmem.c:908-920,922-954`; policy at `docs/spec/rdp.md:197-203`

**Issue:** The documented software contract floors all channels for box and three-point filtering. RGB follows that rule, but box alpha returns 1 when at least two of four texels have alpha set (`alpha >= 2`), and three-point alpha returns 1 at a weighted sum of at least one half (`alpha >= 32768`). For a valid two-of-four box pattern, floor(2/4) is 0 but the implementation returns 1; a three-point half tie is likewise rounded up instead of floored. This changes the resulting RGBA16 pixel for valid filtered textures.

**Fix:** Apply the same floor division to alpha as to RGB before packing the one-bit channel. Add box and three-point fixtures with mixed alpha bits, including the 2-of-4 and exact-half cases.

### Warnings

#### WR-01: Texture coefficient tests do not distinguish all raw rows

**Severity:** WARNING  
**File:** `core/tests/test_rdp_texture.c:244-294,393-419`

**Issue:** The 0x0A test asserts selected values in W0, W4, and W7 and checks one output pixel, leaving other texture coefficient rows zero. The 0x0E shade-texture test supplies one nonzero texture base value at packet word 17, leaves the following texture rows zero, fills the tile white, and checks one pixel. An incorrect word offset or ignored derivative row can therefore pass while the fixture still exercises the intended opcode.

**Fix:** Add independent 0x0A and 0x0E packets with distinct nonzero S/T/W base and derivative fields across x, edge, and y rows. Use a nonuniform tile and assert expected pixels at multiple coordinates so each row changes an observable sample.

#### WR-02: W-zero refusal does not prove coverage is unchanged, and texture overflow lacks a refusal control

**Severity:** WARNING  
**File:** `core/tests/test_rdp_texture.c:607-637`; preflight at `core/t64_rdp_raster.c:64-147`

**Issue:** The W-zero case snapshots and compares the framebuffer only. It does not verify that the canonical coverage plane is unchanged, even though the requirement is whole-triangle refusal before pixel or coverage effects. The truncation test at lines 719-760 snapshots coverage, but it does not exercise W-zero or arithmetic refusal. The texture tests also have no accepted-packet case that exceeds the interpolation/product bounds checked by `texture_products_are_bounded`, so that refusal path has no direct control.

**Fix:** For W-zero and an over-bound texture derivative packet, first establish retained pixels and coverage, then assert the expected render-range refusal and compare full framebuffer and coverage snapshots after each refusal.

---

_Reviewed: 2026-11-17T08:53:22Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: standard_
