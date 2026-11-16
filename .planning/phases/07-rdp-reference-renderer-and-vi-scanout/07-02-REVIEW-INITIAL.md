---
phase: 07-rdp-reference-renderer-and-vi-scanout
reviewed: 2026-11-16T15:08:25Z
depth: standard
files_reviewed: 19
files_reviewed_list:
  - core/CMakeLists.txt
  - core/t64_bus.c
  - core/t64_digest.c
  - core/t64_internal.h
  - core/t64_isviewer.c
  - core/t64_machine.c
  - core/t64_rdp.c
  - core/t64_rsp.c
  - core/tests/test_rdp.c
  - core/tests/test_vi.c
  - docs/spec/rdp-vi-cases.json
  - docs/spec/rdp.md
  - runner/t64_real.c
  - runner/t64_real_fixture.c
  - runner/tests/test_rdp_video.c
  - scripts/check-rdp-vi-cases.py
  - scripts/check-rsp-no-hle.py
  - scripts/check-test-inventory.sh
  - scripts/tests/test_rsp_no_hle.py
findings:
  critical: 0
  warning: 1
  info: 1
  total: 2
  necessity: 0
status: issues_found
---

# Phase 07 Plan 02: Code Review Report

**Reviewed:** 2026-11-16T15:08:25Z  
**Depth:** standard  
**Files Reviewed:** 19  
**Status:** issues_found

## Summary

Reviewed the exact 19-file Plan07-02 source scope, tracing DPC fetch/effect/fence ordering, register and counter bounds, freeze/flush and XBUS selection, fetched RSP COP0 routing, scheduler reservation order, reset behavior, state hashing, and source/body controls. The profile version and policy updates are consistently carried through the machine, digest, ISViewer, bus map, and runner bindings. No confirmed implementation or security vulnerability was found. One required transport case does not actually exercise END extension, and one added internal field is redundant and misleading.

The Plan07-02 summary records 146 normal-gate tests, 13 affected ASAN/UBSan owners, and all 29 no-HLE mutation seeds passing; it also records that the full ASAN suite was not rerun. I did not rerun tests during this review.

## Warnings

### WR-01: Append/resume case never extends DPC END

**Severity:** WARNING  
**File:** `core/tests/test_rdp.c:947-975`  
**Issue:** `case_append_end_resume` initializes `gpr[2]` to `0x110`, and both fetched MTC0 instructions write that same value to DPC END. The first write already sets END to `0x110`; the later write therefore does not append commands past the original bound. The listed fill and FullSync were in range from the start, so the test can pass even if extending END while work is pending is broken.  
**Fix:** Start the first list with END `0x108`, then have the later fetched MTC0 write `0x110` from a different source register or a register updated by the program. Assert END is `0x108` before the extension and becomes `0x110` while the original fill is still pending, then assert the appended command completes.

## Info

### IN-01: `dpc_dma_active` is redundant and remains true after input drains

**Severity:** INFO  
**File:** `core/t64_internal.h:645`  
**Also:** `core/t64_rdp.c:436`, `core/t64_digest.c:506`  
**Issue:** The new flag has no execution or status read path; it is only assigned and included in the digest. It tracks the overall `busy` state, so it remains true after CURRENT reaches END while a command effect, raster work, or fence is pending. The public DMA_BUSY result correctly derives from remaining input (`CURRENT < END`), but the separately hashed flag does not represent that distinction and adds no future-affecting state.  
**Fix:** Remove the redundant field and its digest entry, or maintain it as an independent “input remains” state and use that same state when reporting DMA_BUSY. Keep the profile’s versioned digest contract consistent with the chosen layout.

---

_Reviewed: 2026-11-16T15:08:25Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: standard_
