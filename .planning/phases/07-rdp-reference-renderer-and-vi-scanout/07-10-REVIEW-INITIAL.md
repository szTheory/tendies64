---
phase: 07-rdp-reference-renderer-and-vi-scanout
reviewed: 2026-11-17T22:52:50Z
depth: deep
files_reviewed: 19
files_reviewed_list:
  - core/CMakeLists.txt
  - core/include/tendies64/t64_machine.h
  - core/t64_digest.c
  - core/t64_internal.h
  - core/t64_machine.c
  - core/t64_scheduler.c
  - core/t64_vi.c
  - core/tests/test_profile.c
  - core/tests/test_vi.c
  - core/tests/test_video_output.c
  - docs/spec/rdp-vi-cases.json
  - docs/spec/vi.md
  - runner/CMakeLists.txt
  - runner/t64_real.c
  - runner/t64_supervisor.c
  - runner/tests/test_rdp_video.c
  - scripts/check-rsp-no-hle.py
  - scripts/check-test-inventory.sh
  - scripts/tests/test_rsp_no_hle.py
findings:
  critical: 1
  warning: 2
  info: 0
  total: 3
  necessity: 0
status: issues_found
---

# Phase 07: Code Review Report

**Reviewed:** 2026-11-17T22:52:50Z  
**Depth:** deep  
**Files Reviewed:** 19  
**Status:** issues_found

## Summary

Reviewed the Plan07-10 source diff, its changed filter/output tests, profile and digest paths, runner consumers, policy spec, and relevant plan/evidence. One reachable sampler bug contradicts the selected RDRAM-bounded neighborhood policy. Two test gaps can let the output whole/split check compare identical call sequences and let some filter setup failures skip assertions. I did not rerun builds or tests; the retained owner logs and artifact binding were reviewed as evidence, not treated as silicon or VID-04 authority. The execution-evidence document also has a profile-ID typo at line 5 (`...rdp-vi-v9`; canonical ID is `...rdp-vi-filter-v9`).

## Critical Issues

### CR-01: Filter taps are clipped to the visible scanout height

**Severity:** BLOCKER  
**File:** `core/t64_vi.c:309`  
**Issue:** `geometry.source_height` is computed from the visible scanout window at lines 129–138, then `vi_read_source_pixel()` rejects every tap at or below that derived limit before checking whether its bytes are in RDRAM. For a one-row output, an AA neighborhood's row-1 samples are valid RAM but are discarded; the filter substitutes the foreground instead. The same guard can discard a valid fractional bilinear `y1` or smoothing tap near the final visible row. This changes output pixels and understates `source_bytes`. The Plan07-10 contract permits top-parity clamping and requires per-tap RDRAM bounds; it does not specify a bottom-of-visible-window clamp.

**Fix:** Remove the visible-height rejection for filter taps. Keep the checked address arithmetic and per-pixel RDRAM span check as the validity boundary, retaining the valid-center fallback only when the resulting span is outside RDRAM. Add an actual one-row-window test with a distinct in-RDRAM next-row neighbor and assert both the filtered pixel and expanded `source_bytes`.

## Warnings

### WR-01: The output whole/split test advances both machines field by field

**Severity:** WARNING  
**File:** `core/tests/test_video_output.c:441-445`  
**Issue:** `advance_two_fields()` calls `advance_one_field()` twice (lines 155–156), and the comparison machine also calls `advance_one_field()` twice. Both paths therefore issue the same two `t64_machine_advance()` requests; the later `tick+1` checks are also identical. The named case cannot detect a difference between one requested advance spanning two field boundaries and split requests, despite promising that comparison.

**Fix:** Have one machine issue a single guest-tick request covering both field boundaries, with the other using separate bounds at those boundaries. Compare final tick, packet metadata/bytes, field/sequence, and digest at the same guest boundary.

### WR-02: Some VI filter setup failures can silently skip their assertions

**Severity:** WARNING  
**File:** `core/tests/test_vi.c:276-280`  
**Issue:** When `advance_guest_to_tick()` returns false for an unexpected non-resource stop reason, `render_filter_fixture()` frees its packet and returns `NULL` without recording a failed check. Each filter case returns immediately when that helper returns `NULL` (for example line 438), so the suite can report success without reaching that case's pixel and metadata assertions. The packet-allocation failure path at lines 247–250 has the same silent-skip behavior.

**Fix:** Record a failed test check on every unexpected helper failure, or return an explicit setup result that each case asserts before bailing out. Keep only intentionally invalid-input cases as non-failing refusals.

---

_Reviewed: 2026-11-17T22:52:50Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: deep_
