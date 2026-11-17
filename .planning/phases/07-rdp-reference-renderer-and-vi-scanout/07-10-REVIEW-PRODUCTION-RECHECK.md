---
phase: 07-rdp-reference-renderer-and-vi-scanout
reviewed: 2026-11-17T23:03:41Z
depth: deep
files_reviewed: 2
files_reviewed_list:
  - core/t64_vi.c
  - scripts/check-rsp-no-hle.py
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
  necessity: 0
status: clean
---

# Phase 07: Production Recheck Report

**Reviewed:** 2026-11-17T23:03:41Z  
**Depth:** deep  
**Files Reviewed:** 2  
**Status:** clean for the CR-01 production recheck only

## Summary

The focused production repair removes the visible-derived `source_height` tap cutoff while retaining checked per-tap address arithmetic and the RDRAM end-span test. Invalid taps still return the supplied valid-center fallback before decoding or increasing the reported source span. `geometry_for()` continues to validate the actual visible center extent and its last byte against RDRAM; the removed extent bookkeeping did not weaken that admission check.

The sample-reading helper is only used by the v9 filter path, so this change does not alter the older profiles' render path. Removing the unused geometry field and its legacy assignment has no remaining consumer. The source/body seal changes cover the two modified functions (`geometry_for` and `vi_read_source_pixel`). I found no remaining production defect in this narrow recheck.

I did not rerun builds or tests. The retained old-library regression logs and source guard result were reviewed as parent-provided evidence; this report does not close the separate test-only findings or the overall Plan07-10 review.

---

_Reviewed: 2026-11-17T23:03:41Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: deep_
