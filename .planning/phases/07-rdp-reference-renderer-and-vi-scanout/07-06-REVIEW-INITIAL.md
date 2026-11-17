---
phase: 07-rdp-reference-renderer-and-vi-scanout
reviewed: 2026-11-17T11:58:10Z
depth: standard
files_reviewed: 17
files_reviewed_list:
  - core/CMakeLists.txt
  - core/include/tendies64/t64_machine.h
  - core/t64_digest.c
  - core/t64_internal.h
  - core/t64_isviewer.c
  - core/t64_machine.c
  - core/t64_rdp.c
  - core/t64_rdp_pipeline.c
  - core/t64_rdp_raster.c
  - core/t64_rdp_tmem.c
  - core/tests/test_rdp_combiner.c
  - docs/spec/rdp-vi-cases.json
  - docs/spec/rdp.md
  - runner/t64_real.c
  - scripts/check-rsp-no-hle.py
  - scripts/check-test-inventory.sh
  - scripts/tests/test_rsp_no_hle.py
findings:
  critical: 1
  warning: 4
  info: 0
  total: 5
  necessity: 0
status: issues_found
---

# Phase 07: Code Review Report

**Reviewed:** 2026-11-17T11:58:10Z  
**Depth:** standard  
**Files Reviewed:** 17  
**Status:** issues_found

## Summary

Reviewed the full Plan 07-06 source diff and traced the triangle admission, scheduling, raster, combiner, texture, digest, and profile paths. One reachable v5 path bypasses the newly specified combiner when no SetCombine has been latched. Four evidence or contract gaps remain around cycle-1 selectors, fetched texture input wiring, raster noise cadence, and K4/K5 conversion. The parent-provided gate and focused test results establish that the registered cases pass; they do not cover these gaps. I did not rerun tests.

## Narrative Findings (AI reviewer)

## Critical Issues

### CR-01: V5 triangles without SetCombine silently use the legacy renderer

**Severity:** BLOCKER  
**File:** `core/t64_rdp.c:402-412`; raster branch at `core/t64_rdp_raster.c:565-621`  
**Issue:** Triangle admission checks the profile, packet bounds, and Y range, then schedules the draw without checking `combine_mode_valid`. During rasterization the new combiner runs only when that flag is true; otherwise an accepted edge or textured triangle falls through to the older direct renderer. A fresh v5 machine has no valid SetCombine until one is fetched, so a valid DPC triangle sent before SetCombine is rendered with different semantics instead of being refused or using a documented default. This also skips the v5 policy’s per-covered-pixel noise advancement. The v5 specification says supported triangle pixels use the combiner and names fill/copy as the bypass paths (`docs/spec/rdp.md:317-323,333-338`).

**Fix:** Before scheduling a v5 triangle, either refuse it when SetCombine is not valid (without framebuffer, coverage, or noise effects), or define and test an explicit deterministic default combine mode. Add an accepted raw DPC setup with no SetCombine to exercise that behavior.

## Warnings

### WR-01: Selector denominator checks do not cover cycle 1

**Severity:** WARNING  
**File:** `core/tests/test_rdp_combiner.c:502-585`  
**Issue:** The complete RGB and alpha selector loops construct modes with `cycle0_mode` and invoke `cycle_type=0`. The separate two-cycle literal exercises only selected cycle-1 fields, so the implementation’s cycle-1 extraction and selector mappings are not checked across their field widths and zero aliases. This falls short of Plan 07-06’s “cover each selector” acceptance requirement, even though `combine_cycle` has a separate cycle-1 extraction branch (`core/t64_rdp_pipeline.c:156-177`).

**Fix:** Add a cycle-1 mode packer and independently expected two-cycle cases that vary every RGB A/B/C/D and alpha A/B/C/D code, including each slot’s zero aliases and defined source distinctions.

### WR-02: No accepted fetched textured triangle reaches the new combiner inputs

**Severity:** WARNING  
**File:** `core/tests/test_rdp_combiner.c:181-224,381-413,615-645`; consumer at `core/t64_rdp_raster.c:569-604`  
**Issue:** The DPC-to-RDRAM-to-VI combiner consumer uses an untextured `0x08` triangle. The selector cases populate `t64_rdp_combiner_inputs` directly and call `t64_rdp_combine_pixel`; the fetched key/LOD case similarly runs that helper after selected register commands. No accepted raw textured triangle proves the new RGBA8 TMEM reader, Texel0/Texel1 channels and alpha, texture LOD choice, and combiner wiring together. SetConvert/K4/K5 input wiring is also not exercised by a fetched SetConvert command in this suite. These direct-input tests can pass if the actual register-to-raster path is disconnected or supplies incorrect values.

**Fix:** Add an accepted raw `0x0A` or `0x0E` DPC triangle with authored texture memory/tile setup and literal SetCombine selectors that consume Texel0 and Texel1 alpha. Assert exact RDRAM output and command acceptance; include a fetched SetConvert packet for K4/K5 source selection.

### WR-03: Noise tests do not establish accepted raster-pixel cadence

**Severity:** WARNING  
**File:** `core/tests/test_rdp_combiner.c:256-283,285-322,670-687`; raster caller at `core/t64_rdp_raster.c:560-620`  
**Issue:** The exact seed/cadence and interleaving cases call `t64_rdp_combine_pixel` directly. The only raw consumer does not select Noise, and the refusal case uses a cycle type that rejects the triangle before rendering. The tests therefore do not establish that a fetched draw advances the RNG once for each covered pixel, skips uncovered/clipped pixels, shares the byte across cycles, and preserves the state on refusal. Those conditions are part of the named deterministic software policy and the plan’s acceptance requirements; this is an evidence gap, not a claim about hardware cadence.

**Fix:** Add a fetched triangle whose independent literal selects Noise and assert exact covered output pixels, final RNG state, and unchanged state for uncovered/clipped or refused work. Include a two-cycle selector that consumes the same noise byte in both cycles.

### WR-04: K4/K5 signed-to-byte conversion is not stated or tested at negative inputs

**Severity:** WARNING  
**File:** `core/t64_rdp_pipeline.c:65-67,85-86`; tests at `core/tests/test_rdp_combiner.c:399-400,662-666`; policy summary at `docs/spec/rdp.md:325-338`  
**Issue:** K4 and K5 are converted with `signed9` followed by `clamp_u8`, but the documented software policy only says that key and convert sources are decoded from command fields. It does not state this source conversion, and the selector test seeds convert state directly with nonnegative `0x55`, `0x7b`, and `0xff` values. The selected negative-value behavior is therefore neither an explicit profile rule nor independently tested. This is a software-contract gap; no hardware precision is implied.

**Fix:** State the signed-9-bit-to-unsigned-byte conversion explicitly as reversible v5 software policy and add literal K4/K5 selector cases using fetched SetConvert commands at negative and positive boundaries, with independently calculated expected output.

---

_Reviewed: 2026-11-17T11:58:10Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: standard_
