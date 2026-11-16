---
phase: 05-rsp-low-level-emulation
reviewed: 2026-11-15T01:12:30Z
depth: deep
files_reviewed: 6
files_reviewed_list:
  - scripts/check-rsp-evidence.py
  - scripts/tests/test_rsp_evidence.py
  - scripts/check-rsp-no-hle.py
  - scripts/tests/test_rsp_no_hle.py
  - core/t64_rsp.c
  - core/tests/test_rsp_control.c
findings:
  critical: 2
  warning: 0
  info: 0
  total: 2
status: issues_found
---

# Phase 05: Plan 05-10 Review Closure

**Reviewed:** 2026-11-15T01:12:30Z  
**Scope:** Corrections to CR-02 through CR-06 and WR-02, plus the guest self-HALT pairing guard.  
**Files reviewed:** 6

## Summary

CR-02, CR-03, CR-05, and WR-02 are resolved in the reviewed source. The guest self-HALT guard and its control test are consistent with the stated policy. CR-04 and CR-06 remain blockers: the private projection can still be resealed together with its inventory, and the privacy scan does not recognize `selected_rom_sha256` as a private ROM identity field.

This is a source-only closure review. I did not run tests, builds, guest execution, Git commands, or inspect private diagnostic records. The parent-reported green results remain parent-owned evidence and were not independently repeated here.

## Prior Finding Dispositions

| Finding | Disposition | Basis |
|---|---|---|
| CR-02 | Resolved | The receipt is restricted to fixed native and ASan artifact paths, hashes each artifact, reads its runtime identity from that artifact, and requires the receipt build ID to equal the reported canonical build hash. |
| CR-03 | Resolved | The native test set is derived from the six owning inventory blocks, fixed at 127 unique names, and every required row must occur exactly once with `Passed`; marker-only logs are rejected. |
| CR-04 | **Open — BLOCKER** | The projection and inventory are still jointly mutable; the C inventory check receives only the inventory, and no independently trusted projection digest/source snapshot pins the original member set. |
| CR-05 | Resolved | Each member's batch, captured stdout/stderr, and attempt receipt are read and cross-checked; the fixed C executable revalidates inventory and complete/incomplete batch classification, including the explicit verification output. A complete C exit-3 result is accepted only as a fully partitioned non-pass classification, not as a suite pass. |
| CR-06 | **Open — BLOCKER** | The privacy scan recognizes exact `rom_sha256`/`rom_hash`-style keys but not the private inventory field `selected_rom_sha256`; no control covers this field. |
| WR-02 | Resolved | `start_dma` and `t64_sp_write_register` are in the closed SP function inventory and body digests, with host-entry anonymous-DMEM-effect mutations. |

## Remaining Blockers

### CR-04: Private membership can still be reduced by resealing both snapshots

**Classification:** BLOCKER  
**File:** `scripts/check-rsp-evidence.py:1006-1018, 1032-1038`  
**Control gap:** `scripts/tests/test_rsp_evidence.py:1751-1803`

**Issue:** The checker verifies that the projection hash agrees with the hash repeated in the inventory identity, then compares the projection rows with the inventory rows. Both files are under the same mutable private evidence root. The C validation call at lines 1032-1038 is given only the inventory path; it does not independently regenerate or authenticate the projection. The current shrink mutation removes a value from the inventory while deliberately retaining the original projection, so it exercises only a one-sided edit. If both projection and inventory are shortened together, their counts and projection digest can be resealed consistently and the comparison no longer has an independent original-member anchor. The subsequent C checks establish consistency of the supplied inventory and batch directory, not that the supplied projection is the original source-derived set.

**Fix:** In the final evidence check, invoke the existing bounded source-closure verifier with `python3 -B scripts/verify-rsp-diagnostic-qualification.py --verify-source-closure build/private-rsp-diagnostic/qualification.json` and require its independently rebuilt source membership to match the private inventory before accepting the partition. This path validates the pinned qualification/closure chain and source checkout without invoking the identity executable or guest. Add the authored control requested for this finding: remove one member from both projection and inventory, reseal dependent hashes/counts and partition, and require rejection against the pinned qualification membership.

**Owner:** Plan 05-10 evidence checker and authored private-evidence controls.

### CR-06: `selected_rom_sha256` bypasses public privacy scanning

**Classification:** BLOCKER  
**File:** `scripts/check-rsp-evidence.py:476-496`  
**Control gap:** `scripts/tests/test_rsp_evidence.py:57-68`

**Issue:** The ROM identity matcher names only `rom_sha256`, `rom_id`, `rom_hash`, and `rom_identity`. It does not match the private inventory key `selected_rom_sha256`. Because the checker applies this byte scan to public receipt and report contents, a report or nested public receipt field containing `"selected_rom_sha256":"<64-hex-value>"` is not covered by the one approved authored-composition `rom_sha256` exception and does not match another privacy marker. The quoted-field controls exercise `rom_sha256` but omit `selected_rom_sha256`.

**Fix:** Treat `selected_rom_sha256` and equivalent member-hash keys as private identity fields. Keep the exception path-bound to the one closed composition field in the public receipt, and add a quoted `selected_rom_sha256` mutation to both receipt and report controls.

**Owner:** Plan 05-10 evidence checker and `scripts/tests/test_rsp_evidence.py`.

## Self-HALT Pairing Guard

No new finding. The decoder marks only an accepted guest-origin c4 write that newly sets HALT as `guest_self_halt`, and excludes that instruction from VADD pairing. The control test observes one retired MTC0, no paired second instruction, continuing DMA, host asynchronous HALT refusal without state change, and resume from the saved PC. The adjacent existing control continues to cover refusal of guest single-step changes while DMA is in flight.

---

_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: deep, scoped closure_
