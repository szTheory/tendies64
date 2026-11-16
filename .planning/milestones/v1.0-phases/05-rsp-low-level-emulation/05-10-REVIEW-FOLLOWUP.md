---
phase: 05-rsp-low-level-emulation
reviewed: 2026-11-14T16:51:39Z
depth: deep
files_reviewed: 9
files_reviewed_list:
  - core/t64_internal.h
  - core/t64_rsp.c
  - core/t64_sp.c
  - core/tests/test_rsp_control.c
  - docs/spec/rsp-profile.md
  - scripts/check-rsp-no-hle.py
  - scripts/tests/test_rsp_no_hle.py
  - scripts/check-rsp-evidence.py
  - scripts/tests/test_rsp_evidence.py
findings:
  critical: 5
  warning: 1
  info: 0
  total: 6
  necessity: 0
status: issues_found
---

# Phase 05: Scoped Code Review Follow-up

**Reviewed:** 2026-11-14T16:51:39Z  
**Depth:** deep  
**Files Reviewed:** 9  
**Status:** issues_found

## Summary

The scoped review confirms that the prior DMA refusal atomicity finding is fixed: fetched c2/c3 admission is projected before DMA commit, and refusal tests compare DMA state, memory, scheduler state, trace output, and the next event. The prior no-HLE finding is fixed for the cited RSP decode helpers through a complete static-helper inventory in t64_rsp.c. The broader helper guard still misses the host SP write entry path.

Five blockers remain in the new evidence checker: it does not bind native or ASan build IDs to their actual artifacts, can accept a native gate log without test results, derives private membership from a mutable inventory, trusts self-declared private outcome rows without checking the retained C output, and misses quoted private identity fields in public reports. One warning remains for the unguarded SP DMA entry helpers. No private diagnostic records were read. Per dispatch, I did not rerun builds, CTests, or guest execution.

## Narrative Findings (AI reviewer)

### CR-02: [BLOCKER] Build IDs are not bound to the artifacts they identify

**File:** scripts/check-rsp-evidence.py:747-759, 1053-1056  
**Classification:** BLOCKER  
**Issue:** The checker validates a native or ASan build ID only against the ID format, then validates the artifact hash against a path selected in the same receipt. It never verifies that the artifact is the canonical build output or that the artifact contains the claimed ID. The accepted synthetic baseline demonstrates the gap: its placeholder artifact bytes and arbitrary well-formed IDs pass the same validation function. A stale or unrelated artifact can therefore be relabeled with a plausible build ID and accepted after updating the receipt hashes.
**Fix:** Bind each ID to a fixed canonical artifact and an independently captured build identity from that artifact or its canonical build record. Add a control that changes an ID to a different valid t64-build-v1 value while leaving the artifact and build record unchanged; the checker must refuse it.
**Owner:** Plan 05-10 evidence checker and its authored controls.

### CR-03: [BLOCKER] Native gate evidence can pass without native test results

**File:** scripts/check-rsp-evidence.py:761-787, 873-889  
**Classification:** BLOCKER  
**Issue:** The native log check requires only the strings for gate PASS, inventory PASS, and a lint summary. It does not require any named CTest rows, passing outcomes, or the expected inventory count. In contrast, the ASan log is parsed for individual passing rows. The accepted synthetic native log has no native test result rows, so the checker can report final evidence success without proving the native tests ran.
**Fix:** Parse the native CTest output using the required gate inventory, require each expected test exactly once with Passed status, reject skipped/disabled/not-run rows, and verify the reported total against the owning inventory. Add an authored mutation that removes native test rows while retaining the existing marker strings.
**Owner:** Plan 05-10 evidence checker and scripts/tests/test_rsp_evidence.py.

### CR-04: [BLOCKER] Private completeness is measured against an editable inventory

**File:** scripts/check-rsp-evidence.py:450-507, 665-690, 1181-1205  
**Classification:** BLOCKER  
**Issue:** _inventory_members derives the expected set only from the registrations and values in the inventory file. The sidecar, audit, and inventory hashes are all supplied by the same mutable private evidence tree. Removing an inventory value or registration, adjusting declared_counts, then recomputing the inventory hash, audit rows, and sidecar partition produces a smaller internally consistent denominator; the checker has no independent source-derived inventory anchor to reject it. The current dropped-row mutation changes audit.member_results, not the original inventory.
**Fix:** Bind the sidecar to the original C-produced source-derived inventory or an independently captured C verification receipt, and verify that anchor before deriving the denominator. Add an authored control that drops an original inventory member and recomputes every dependent summary/hash; it must still be rejected.
**Owner:** Plan 05-10 evidence checker and its private-evidence controls.

### CR-05: [BLOCKER] Private pass rows are self-attested instead of checked against C output

**File:** scripts/check-rsp-evidence.py:518-649, 652-695, 917-1017  
**Classification:** BLOCKER  
**Issue:** The checker hashes the retained batch, stdout, stderr, and attempt files against digests repeated in the audit, but does not parse or otherwise validate the C batch or captured output. For a pass, it trusts the classification strings in the audit and attempt receipt plus a self-reported zero exit code; the adapter argv is only checked to be a list. It also never validates the audit's explicit_verify_complete value. The accepted synthetic baseline contains arbitrary synthetic batch JSON and a synthetic complete-verification value, showing these fields are not authenticated by the validation path. If a refusal row and its attempt receipt are relabeled pass, the self-reported exit and terminal-retirement fields are updated, and the audit/sidecar hashes are refreshed, the original refusal output is not inspected and cannot contradict the new label.
**Fix:** Reuse the existing C-authenticated batch/result protocol and saved complete-verification result to validate the actual retained evidence, without executing guests or replacing the C parser. Bind every member classification and identity to that output. Add a mutation that relabels a refusal and its attempt receipt as pass while leaving the original batch/stdout unchanged, plus a non-passing complete-verification result; both must be rejected.
**Owner:** Plan 05-10 evidence checker and its authored private-evidence controls.

### CR-06: [BLOCKER] JSON-quoted private result fields bypass the public privacy scan

**File:** scripts/check-rsp-evidence.py:343-353; scripts/tests/test_rsp_evidence.py:39-48  
**Classification:** BLOCKER  
**Issue:** The privacy pattern for classification and rom_sha256 expects a colon immediately after the field name, so it does not match JSON keys with the required closing quote. A harmless synthetic JSON record containing a quoted rom_sha256 field is accepted by privacy_scan; a quoted classification value is missed for the same reason. Public reports or hosted records can therefore include private identity/result fields without failing the privacy gate. The current quoted-field controls cover private_build_id, inventory_sha256, and source_name, but not classification or rom_sha256.
**Fix:** Make the field matcher quote-aware or validate public JSON against explicit privacy-safe schemas. Add quoted classification and rom_sha256 mutations, including a non-sensitive synthetic digest, and require refusal.
**Owner:** Plan 05-10 evidence checker and scripts/tests/test_rsp_evidence.py.

## Warnings

### WR-02: [WARNING] No-HLE route inventory omits the SP DMA entry helpers

**File:** scripts/check-rsp-no-hle.py:100-123, 350-403; scripts/tests/test_rsp_no_hle.py:160-212; core/t64_sp.c:231-241, 851-888  
**Classification:** WARNING  
**Issue:** The checker closes the new RSP static-helper map and explicitly digests the new guest status helpers, but its SP function inventory only closes names beginning t64_sp_rsp_dma_. The effect-producing start_dma dispatcher and t64_sp_write_register entry are absent from the digest routes and mutation controls. A DMEM-conditioned anonymous effect added to either host MMIO path would not change any currently pinned body; it could bypass the reviewed RSP DMA route without matching the token scan. Existing SP mutation controls cover admission_preflight, not these entry helpers.
**Fix:** Add start_dma and t64_sp_write_register to the reviewed body inventory, close the SP static-helper set or equivalent entry map, and add an anonymous DMEM-conditioned mutation in this host DMA path that must be rejected.
**Owner:** scripts/check-rsp-no-hle.py and scripts/tests/test_rsp_no_hle.py.

---

_Reviewed: 2026-11-14T16:51:39Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: deep_
