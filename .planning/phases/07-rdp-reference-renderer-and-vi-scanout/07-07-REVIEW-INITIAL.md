---
phase: 07-rdp-reference-renderer-and-vi-scanout
reviewed: 2026-11-17T15:19:13Z
depth: standard
files_reviewed: 21
files_reviewed_list:
  - .planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-07-EXECUTION-EVIDENCE.md
  - core/CMakeLists.txt
  - core/include/tendies64/t64_machine.h
  - core/t64_bus.c
  - core/t64_digest.c
  - core/t64_internal.h
  - core/t64_isviewer.c
  - core/t64_machine.c
  - core/t64_pi.c
  - core/t64_rdp.c
  - core/t64_rdp_pipeline.c
  - core/t64_rdp_raster.c
  - core/t64_rdp_tmem.c
  - core/t64_sp.c
  - core/t64_vi.c
  - core/tests/test_rdp_blender.c
  - docs/spec/rdp-vi-cases.json
  - docs/spec/rdp.md
  - scripts/check-rsp-no-hle.py
  - scripts/check-test-inventory.sh
  - scripts/tests/test_rsp_no_hle.py
findings:
  critical: 1
  warning: 2
  info: 1
  total: 4
  necessity: 0
status: issues_found
---

# Phase 07: Code Review Report

**Reviewed:** 2026-11-17T15:19:13Z  
**Depth:** standard  
**Files Reviewed:** 21  
**Status:** issues_found

## Summary

The scoped implementation has a profile-identity blocker and two gaps in the tests’ independent raw-field and shared-edge oracles. I traced the RDRAM invalidation hooks through successful CPU scalar/masked/cache writeback, PI completion, both SP DMA commits, and the fill/copy/raster and VI consumers; the accepted paths have hooks after backing mutation, while rejected or unselected operations do not invalidate. No separate correctness defect was established in those routes. The execution evidence reports passing normal and ASAN blender suites and preserves earlier failures; this review did not run or independently rehash those artifacts. The commit history places production work before the shared-edge test-only follow-up, so this report does not claim every production slice followed saved RED evidence. D2/D5/D6 remain stops; no hardware or VID-04 conclusion is supported.

## Narrative Findings (AI reviewer)

### Critical Issues

#### CR-01: BLOCKER — Descriptor v9 omits profile and boot policy fields

**Severity:** BLOCKER  
**File:** core/t64_machine.c:1194-1209, 1040-1072, 1351-1374  
**Issue:** The new blender profile uses descriptor version 9, but validation only requires the RDP/VI strings for versions 5–8, triangle policy for 7–8, and combiner policy for 8. More seriously, write_profile only serializes the boot metadata and RDP/VI policy fields for descriptor versions 5–8. Consequently, profile hashing and serialization omit the v6 profile’s asset_sha256, payload_sha256, boot_policy_sha256, hle_seed_reset_version, rdp_work_policy, vi_clock_policy, rdp_noise_policy, rdp_undefined_color_policy, vi_output_policy, rdp_triangle_policy, and rdp_combiner_policy. t64_machine_create then checks the known v6 ID by comparing these incomplete hashes in is_rdram_b_t1_profile; changing or blanking those fields leaves the candidate hash equal to the canonical profile and is accepted as the pinned v6 profile. The profile digest and serialized bytes therefore fail to identify the selected boot and blender/VI contract.
**Fix:** Add descriptor version 9 to the explicit validation and serialization branches. Serialize its boot metadata, shared RDP/VI policies, triangle policy, and combiner/blender policy before hashing or returning serialized bytes. Add profile tests that change each included policy/boot field and verify the hash and serialized bytes change, and that a mutated descriptor with the known v6 ID is rejected. Update the v6 VI policy string to identify the RGBA8888 scanout path as well as RGBA5551.

### Warnings

#### WR-01: WARNING — Selector matrix does not independently test raw SetOtherModes fields

**Severity:** WARNING  
**File:** core/tests/test_rdp_blender.c:208-237, 324-402; core/t64_rdp_pipeline.c:422-426  
**Issue:** The all-selector test builds modes with blender_modes, whose P/A/M/B shifts duplicate the production decoder’s shifts, then calls t64_rdp_blend_pixel directly. The fetched literal packet reaches the raw command path but covers only a subset of the eight selector slots. A shared field-position error in the helper and implementation can therefore pass the matrix even though the primary field table is wrong. The production shifts match the pinned table on inspection; the gap is that the matrix does not independently prove this raw decoding contract.
**Fix:** Add table-driven fetched SetOtherModes packets with literal selector bits derived directly from 07-07-BLENDER-FIELDS.md, and assert independent expected output for every selector in each cycle. Keep the current helper only for arithmetic cases that do not claim raw-field coverage.

#### WR-02: WARNING — Shared-edge case checks only accumulated coverage

**Severity:** WARNING  
**File:** core/tests/test_rdp_blender.c:711-721  
**Issue:** rdp_triangle_coverage is OR-accumulated across primitives, but the second draw assigns combined_mask from that cumulative plane and then asserts it is all ones. These checks establish only that the union is full; they do not establish that the second primitive owns exactly the complementary samples or that it has no overlap. A second primitive covering all samples would satisfy these assertions as well, so the case does not discriminate the promised shared-edge tie behavior.
**Fix:** Capture each primitive’s mask independently, such as by drawing each literal on a fresh instance, and assert the two masks are disjoint complements with the expected populations (10 and 6) before checking the accumulated full mask and logical coverage 8.

### Info

#### IN-01: INFO — RDP profile overview omits the new v6 profile

**Severity:** INFO  
**File:** docs/spec/rdp.md:3-12, 356-362  
**Issue:** The new section documents the v6 blender profile, but the introductory profile inventory still describes only earlier profiles and says there are three while listing five. The overview now omits the profile this change adds and gives readers an inconsistent scope.
**Fix:** Update the introduction to list all six composite profiles and their versioned responsibilities.

---

_Reviewed: 2026-11-17T15:19:13Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: standard_
