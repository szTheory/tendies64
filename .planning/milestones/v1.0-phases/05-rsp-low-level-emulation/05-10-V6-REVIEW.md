---
phase: 05-rsp-low-level-emulation
reviewed: 2026-11-15T01:26:13Z
depth: deep
files_reviewed: 5
files_reviewed_list:
  - scripts/prepare-rsp-diagnostic.py
  - scripts/verify-rsp-diagnostic-qualification.py
  - scripts/run-rsp-diagnostic.py
  - scripts/tests/test_rsp_qualification.py
  - scripts/tests/test_rsp_run.py
findings:
  critical: 1
  warning: 0
  info: 0
  total: 1
status: issues_found
---

# Phase 05: V6 Qualification Review

**Reviewed:** 2026-11-15T01:26:13Z  
**Depth:** deep, scoped to the v6 host-only qualification changes  
**Files reviewed:** 5

## Summary

The v6 path pins its production source to the reviewed baseline, closes the host-script allowlist, checks the prior source/tool/asset qualification before identity use, hashes both executables around metadata queries, and binds the exact binaries and binding in a private archive. The runner verifies the binding before adapter dispatch and keeps guest execution local-only. One blocker remains: the saved normal and sanitizer test evidence validators do not prove the complete named test inventories.

This was a source-only review. I did not run builds, tests, Git commands, guest execution, or inspect private qualification records. The parent-reported qualification and runner test results were not repeated.

## Critical Issues

### CR-01: V6 accepts gate logs without proving every registered test ran

**Classification:** BLOCKER  
**Files:** `scripts/prepare-rsp-diagnostic.py:148-158, 161-192`; `scripts/verify-rsp-diagnostic-qualification.py:409-432, 448-473`  
**Control gap:** `scripts/tests/test_rsp_qualification.py:513-758`

**Issue:** The normal-gate checks only search for an inventory count and a matching `100% tests passed out of N` summary. They do not parse any native CTest row or require the gate success marker, so a normal log containing only those two lines satisfies the check. The sanitizer check counts lines marked `Passed` and requires a subset of named tests, but it does not compare the full CTest name set to the owning ASan inventory or compare the inventory count to the CTest total. An omitted registered test can therefore be replaced by an arbitrary passing row while the summary remains internally consistent. The v6 binding stores these validations as its saved normal and sanitizer proof, and the source-scope verifier repeats the same permissive checks. The authored v6 controls do not exercise marker-only normal logs or substituted ASan test names.

**Fix:** Reuse the strict registered-test row validation used by the final evidence checker in both the v6 producer and verifier. Require the normal log to contain every exact owning native test once with `Passed`; require the sanitizer inventory count and CTest total to match, with every selected test name appearing exactly once and passing and no unregistered names. Add authored controls for a marker-only normal log and an ASan log that substitutes an unknown name for a registered test.

**Owner:** V6 qualification verifier and `scripts/tests/test_rsp_qualification.py`.

## Scope Notes

- No additional blocker was found in the reviewed source/binding allowlist, executable hash ordering, source-closure-before-identity sequence, archive receipt binding, or runner pre-use dispatch path.
- The v6 output collision guard is intentional. The owner confirmed no real v6 binding/archive exists and that the final seven-file source set will be committed before the first real binding is created, so I found no current rebind/collision issue.
- This report does not qualify or claim any guest result.

---

_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: deep, scoped v6 review_

## Parent clarification of the accepted correction

The instrumented inventory and selected owning test run have distinct totals.
The full instrumented registration inventory remains 129; the approved owning
selection contains 31 tests. Their totals must not be equated. The accepted
correction requires every selected CTest row exactly once as Passed, an exact
selected-run summary, all required owners, and no name outside the registered
native set plus the two instrumented additions. Full inventory validation
remains separate. This clarifies the review's remedy without expanding the
approved test selection or withdrawing its concrete unknown-name finding.
