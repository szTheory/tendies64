---
phase: 07-rdp-reference-renderer-and-vi-scanout
reviewed: 2026-11-18T05:52:56Z
depth: deep
files_reviewed: 4
files_reviewed_list:
  - docs/evidence/phase07-oracle-authority.json
  - docs/evidence/phase07-oracle-authority.md
  - scripts/check-rdp-oracle-authority.py
  - scripts/tests/test_rdp_oracle_authority.py
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
  necessity: 0
status: clean
---

# Phase 07: Plan12 Task1 Review Recheck

**Reviewed:** 2026-11-18T05:52:56Z  
**Depth:** deep, focused repair recheck  
**Files Reviewed:** 4  
**Status:** clean

## Summary

All seven initial findings and the two storage-safety findings from the prior recheck are addressed. The request digest binds both decision types, production trust pins remain unset, live comparison readiness remains blocked, and the storage checks reject unsafe modes and symlinked test roots without changing their targets. The direct integration test passed. No CTest, full gate, oracle operation, upstream access, or broad tree scan was performed in this recheck. All reviewed files meet quality standards after these focused repairs; no unresolved issues were found.

## Recheck of Initial Findings

| Initial finding | Disposition |
| --- | --- |
| CR-01: Decision omitted full request digest | **Resolved.** Both decision schemas require `request_sha256`; local and publication validators compare it with the canonical full request. Mutation controls refresh the request source pin while retaining the old decision and verify refusal. |
| WR-01: 216 rows presented as prepared black-box cases | **Resolved.** The request identifies native membership rows and blocks classification, mapping, corpus export, replay adapter, and execution until each is ready. |
| WR-02: No elapsed-time limits | **Resolved.** The request proposes acquisition, build, per-case, and aggregate caps plus process-group cleanup; it explicitly grants no active run window. |
| WR-03: Acquisition path was in the worktree | **Resolved.** The proposal uses TMPDIR, resolves paths, and refuses in-worktree paths including existing symlinks. The focused controls cover an external sibling, an in-tree path, and a symlink into the synthetic worktree. |
| WR-04: Private-path check only matched prefixes | **Resolved.** The checker scans nested text and has controls for prose, a nested file URL, and a list entry containing a Windows path. |
| WR-05: Positive synthetic test bypassed admission | **Resolved.** A complete synthetic request/audit/decision traverses `_require_local_approved` using a short-lived test-only pin patched only within a context manager. The production CLI has no pin override. |
| WR-06: Integration test did not require all control names | **Resolved.** The test compares the exact 22-name set and count. |

## Recheck of Final Residuals

| Prior residual | Disposition |
| --- | --- |
| WR-07: Test setup could chmod a symlink target | **Resolved.** `ensure_private_test_root` creates/checks each build-path component with `lstat`, refuses symlinks and non-directories, and never chmods an existing path. Its symlink and parent-link controls verify that the target mode stays unchanged and no child is created. |
| WR-08: Storage proposal ignored its 0700 mode | **Resolved.** `_validate_storage_proposal` requires exactly `0700`; the existing 22-control selftest mutates it to `0777` and expects refusal. |

## Narrative Findings (AI reviewer)

No unresolved findings.

---

_Reviewed: 2026-11-18T05:52:56Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: deep, focused repair recheck_
