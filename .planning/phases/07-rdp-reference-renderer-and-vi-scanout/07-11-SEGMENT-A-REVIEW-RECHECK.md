---
phase: 07-rdp-reference-renderer-and-vi-scanout
reviewed: 2026-11-18T02:05:42Z
depth: deep
files_reviewed: 3
files_reviewed_list:
  - scripts/qualify-rdp-vi.py
  - scripts/collect-boot-evidence.sh
  - scripts/tests/test_rdp_vi_qualification.py
findings:
  critical: 0
  warning: 1
  info: 0
  total: 1
  necessity: 0
status: issues_found
---

# Phase 07 Plan 11 Segment A: Repair Recheck

**Reviewed:** 2026-11-18T02:05:42Z  
**Depth:** deep  
**Files Reviewed:** 3  
**Status:** issues_found

## Summary

The four findings from the initial Segment A review are resolved in the changed callpaths. Public verification now compares decoded bytes with the independent authored constants; the bounded runner kills and checks its process group; the collector downloads each selected artifact ID, checks its API size and digest, then applies the strict ZIP parser; and native qualification pins relevant source inputs before and during execution. The retained final helper record's script hashes match the current files, its private control says 32 rejection cases passed, and the process-tree and source-drift controls contain the expected evidence. The collector result remains mocked-only (`hosted_proof: false`).

One new warning remains: the collector enforces the archive byte limit only after `gh api` has written the entire response to disk. Full native, ASan/UBSan, GCC, and live hosted runs were not run in this recheck; the execution contract reserves them for the later segment.

## Recheck of Initial Findings

| Finding | Result | Current code and evidence |
|---|---|---|
| CR-01: resealed public raw bytes | **Resolved** | `scripts/qualify-rdp-vi.py:962-966` compares decoded public RDRAM and VI bytes against `EXPECTED_SOURCE` and `EXPECTED_PACKET`. The collector control mutates and reseals both host archives and reports rejection; retained record: `build/gate/07-qualification/upload-layout-control.7v1xg1/control.json`. |
| CR-02: timeout leaves descendants alive | **Resolved** | `scripts/qualify-rdp-vi.py:151-253` starts a new session, terminates the process group and escalates to `SIGKILL`; `_run_recorded` refuses a nonempty group at lines 285-294. The retained deadline record reports the grandchild dead, the group empty, and no marker growth after return: `build/gate/07-qualification/deadline-control-jyqzpddo/deadline-control.json`. |
| WR-01: real ZIP path bypasses strict parser | **Resolved** | `scripts/collect-boot-evidence.sh:259-284` gets the exact artifact ID, retains the raw ZIP and API records, checks archive size and digest, then calls `--extract-upload` before the exact tree check. The mock emits ZIP bytes through this same API-ID path. Its proof is not hosted proof. |
| WR-02: native run labels a dirty tree as HEAD | **Resolved** | `scripts/qualify-rdp-vi.py:63-67, 1630-1697` snapshots regular tracked inputs, rejects relevant tracked/untracked/ignored or symlink drift, and rechecks around native stages. The private source-drift control verifies those refusals and allows the specified owner-file edits: `build/gate/07-qualification/native-source-control-0c8ec25e/control.json`. |

## Warnings

### WR-01: WARNING — Artifact response is size-checked only after it is written

**File:** `scripts/collect-boot-evidence.sh:262-267`  
**Issue:** The metadata check caps the declared artifact size, but line 263 redirects the complete `gh api` response into a persistent archive before lines 265-267 measure it. If the endpoint returns a body larger than the declared size or the one-megabyte limit, the collector eventually refuses it but can first consume unbounded disk space. The existing `digest-mismatch` control uses a small archive and therefore does not exercise this over-limit response path.

**Fix:** Stream the download through a byte counter capped at `MAX_UPLOAD_BYTES + 1`; terminate and reap the downloader as soon as it exceeds the cap, remove the partial archive, and retain the existing exact size and digest checks. Add a mock response larger than the cap while API metadata remains within the allowed limit.

## Bounded Evidence Checks

- Independently recomputed SHA-256 values for the saved final collector command's helper and collector match the current source files. Its record identifies source `a059e64d65ffcf10f88d75b865bfd237f4ac30f3`, return code 0, and 7.99 seconds. The retained upload control reports 32 rejection cases and `hosted_proof: false`.
- The deadline control records the spawned grandchild as no longer alive and the write marker unchanged after its post-return wait.
- The source-drift control records refusal for tracked, post-run, untracked, ignored, and committed-symlink changes while permitting the named owner-file changes.
- The parent reports the focused CTest passed 1/1. I did not rerun it or the broader native/ASan/GCC/hosted matrix in this review recheck.
- The source-root guard passed before this report was written; protected `AGENTS.md` and `.planning/config.json` Git blob IDs still match the supplied values.

---

_Reviewed: 2026-11-18T02:05:42Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: deep_
