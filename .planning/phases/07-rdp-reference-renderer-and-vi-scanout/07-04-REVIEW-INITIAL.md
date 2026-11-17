---
phase: 07-rdp-reference-renderer-and-vi-scanout
reviewed: 2026-11-17T06:09:26Z
depth: standard
files_reviewed: 15
files_reviewed_list:
  - core/CMakeLists.txt
  - core/include/tendies64/t64_machine.h
  - core/t64_digest.c
  - core/t64_internal.h
  - core/t64_isviewer.c
  - core/t64_machine.c
  - core/t64_rdp.c
  - core/t64_rdp_raster.c
  - core/tests/test_rdp_triangles.c
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

# Phase 07-04: Code Review Report

**Reviewed:** 2026-11-17T06:09:26Z
**Depth:** standard
**Files Reviewed:** 15
**Status:** issues_found

## Summary

The review found one alignment bug reachable through accepted custom profile descriptors, two contract-documentation contradictions, and two gaps in the triangle tests' evidence. The recorded gate and sanitizer results cover the built-in profile; they do not cover the odd-sized custom profile or the unasserted shade and coverage fields described below.

## Narrative Findings (AI reviewer)

### Critical Issues

#### CR-01: BLOCKER — Coverage plane can be misaligned for accepted profiles

**File:** `core/t64_machine.c:1587-1592`
**Severity:** BLOCKER
**Issue:** `t64_machine_create` places the `uint16_t` coverage plane directly at `ram + profile->rdram_bytes`. A custom descriptor with model 12/state 17 and an odd `rdram_bytes` passes `profile_fields_are_valid` (which only requires this size to be nonzero) and is not rejected by the pinned-profile check. The resulting pointer is not aligned for `uint16_t`; an accepted triangle later dereferences it at `core/t64_rdp_raster.c:251`, which is undefined behavior and can fault on strict-alignment targets. The built-in 4 MiB profile is aligned, but the public constructor accepts the custom descriptor case.
**Fix:** Align the coverage offset up to `_Alignof(uint16_t)` and include that padding in the allocation size, or reject odd `rdram_bytes` for model 12/state 17 before allocating.

### Warnings

#### WR-01: WARNING — Documentation calls canonical coverage test-only

**File:** `docs/spec/rdp.md:188-197`
**Severity:** WARNING
**Issue:** Line 193 says the sample mask is retained only as an internal test diagnostic. Earlier lines 157-162 say the 1024×1024 plane persists until ROM admission and is included in the v17 state digest; `core/t64_digest.c:504-521` hashes it as canonical state. The text conflates the persistent logical plane with the last-sample diagnostic fields, obscuring which state affects determinism and later coverage mapping.
**Fix:** State that the logical sample plane is canonical, digest-covered state that currently has no public mapping, and distinguish it from `triangle_last_coverage_*`, which is test observation state.

#### WR-02: WARNING — Primary-source wording overstates Y sign encoding

**File:** `docs/spec/rdp.md:178-182`
**Severity:** WARNING
**Issue:** The paragraph says the primary diagrams identify signed 14-bit `s11.2` Y fields and then says sign encoding is unstated only for the other coefficient fields. The raw contract records that no inspected primary source establishes the Y fields' bit encoding either. D-11 authorizes the implementation's two's-complement sign extension for Y as software policy; the current wording implies that interpretation is settled by the primary source and leaves the Y part of the approved policy unstated.
**Fix:** Preserve the source-backed `s11.2` label while stating that its bit encoding remains unresolved. Explicitly document D-11's 14-bit two's-complement `/4` Y policy separately from the 32-bit Q16 policy for the other edge and shade pairs.

#### WR-03: WARNING — Shading test does not discriminate the W4–W11 layout

**File:** `core/tests/test_rdp_triangles.c:173-192`
**Severity:** WARNING
**Issue:** The accepted shade packet sets a nonzero red base and red `dR/dx`; the remaining shade fields are zero, and the test checks one output pixel. It cannot detect swapped or misplaced green, blue, or alpha fields, nor ignored or misread `d/de` and `d/dy` rows. The authored evidence therefore does not independently establish the documented W4–W11 decoding behavior.
**Fix:** Add a literal accepted packet with distinct values in all four channel bases and derivatives, including nonzero `d/de` and `d/dy`, then assert independently calculated output pixels that distinguish each field and row.

#### WR-04: WARNING — Truncation controls observe only one pixel

**File:** `core/tests/test_rdp_triangles.c:453-490`
**Severity:** WARNING
**Issue:** Each group-boundary truncation checks one framebuffer pixel for the sentinel value, but does not compare the rest of the seeded framebuffer or the new logical coverage plane. Since the plane is canonical state included in the digest, a malformed packet that partially changed another pixel or coverage entry could pass these controls. The later prior-work control at lines 516-535 also checks only one pixel.
**Fix:** For each malformed packet, compare the full seeded framebuffer region and coverage plane against snapshots taken after valid setup; retain separate assertions for refusal status, pointer position, and previously committed work.

---

_Reviewed: 2026-11-17T06:09:26Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: standard_
