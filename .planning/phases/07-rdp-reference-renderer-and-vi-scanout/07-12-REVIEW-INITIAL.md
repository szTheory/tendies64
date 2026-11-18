---
phase: 07-rdp-reference-renderer-and-vi-scanout
reviewed: 2026-11-18T05:13:17Z
depth: deep
files_reviewed: 6
files_reviewed_list:
  - docs/evidence/phase07-oracle-authority.json
  - docs/evidence/phase07-oracle-authority.md
  - runner/CMakeLists.txt
  - scripts/check-rdp-oracle-authority.py
  - scripts/check-test-inventory.sh
  - scripts/tests/test_rdp_oracle_authority.py
findings:
  critical: 1
  warning: 6
  info: 0
  total: 7
  necessity: 0
status: issues_found
---

# Phase 07: Code Review Report (Initial)

**Reviewed:** 2026-11-18T05:13:17Z  
**Depth:** deep  
**Files Reviewed:** 6  
**Status:** issues_found

## Summary

The authority record is currently blocked, and its request/audit digests and the 40-character upstream revision identities are internally consistent. The fail-closed default remains safe today. The approved-decision validator does not bind owner/legal authority to the complete requested restrictions, however, so a later request change can retain a stale approval pin. The proposed comparison scope also overstates what the current 216-row native test inventory can execute against a black-box oracle and lacks an elapsed-time bound. No CTest or broad gate was run during this review; a bounded no-write probe confirmed that changing the storage root, output cap, and build command leaves the same synthetic approval accepted.

## Narrative Findings (AI reviewer)

### Critical Issues

#### CR-01: Approval does not bind the full requested operation restrictions

**Classification:** BLOCKER  
**File:** scripts/check-rdp-oracle-authority.py:69-78, 359-415, 418-467

**Issue:** Neither decision schema contains the full request digest. Local approval validation binds the oracle IDs/revisions, fixed operation list, corpus digest, and rights-audit digest, but not the rest of request, including build commands, private storage location, output limits, or other restrictions. Publication validation has the same gap. `_validate_record` separately checks the current request against `PINNED_REQUEST_SHA256`, but changing that source pin does not invalidate an already trusted decision: the unchanged decision still validates against the changed request. A bounded probe confirmed that one synthetic grant remained accepted after changing `private_root`, `outputs_max_bytes_total`, and the Angrylion build command. After a real decision is pinned, a request edit plus request-pin refresh could therefore change approved restrictions without renewing the authority decision.

**Fix:** Add a `request_sha256` field to both decision schemas and require it to equal the canonical digest of the entire request. Require a new trusted decision pin whenever any request field changes; cover mutations to recipe, storage, output, and operation restrictions in selftests.

### Warnings

#### WR-01: The requested 216 black-box cases are inventory rows, not prepared oracle cases

**Classification:** WARNING  
**File:** docs/evidence/phase07-oracle-authority.md:12; docs/evidence/phase07-oracle-authority.json:139-151

**Issue:** The request describes black-box execution on 216 cases, while the bound manifest is a native membership inventory of suite/CTest/binary/case labels. It includes refusal and negative-control cases such as `replacement-fixture-refused` and repeats the same case names under separate suites. The inventory checker verifies compiled Tendies64 test binaries and CTest membership; it provides no replay adapter or mapping from each row to an external renderer input/output. The two listed build-only fixtures do not make every native unit/control row executable by a black-box oracle. This authority request therefore promises a 216-case differential corpus that is not prepared.

**Fix:** Describe the 216 items as suite-scoped membership rows, classify which rows produce valid oracle-comparable inputs and outputs, and specify the replay adapter and exact fixture mapping before requesting authority for a case count.

#### WR-02: The proposed bounded run has no elapsed-time limits

**Classification:** WARNING  
**File:** docs/evidence/phase07-oracle-authority.json:71-80, 144-152

**Issue:** The request caps checkout/build bytes, retained output, bytes per case, and case count, but it sets no deadline for acquisition, build, an individual comparison, or the aggregate run. A build or case that hangs can run indefinitely while staying within every listed storage limit, so “bounded build” and “bounded execution” do not bound resource use.

**Fix:** Add explicit acquisition/build deadlines, per-case and aggregate wall-clock limits, and a process-group termination rule to the pinned request. Include the limits in the full decision binding from CR-01.

#### WR-03: The “out-of-tree” acquisition path is inside the Tendies64 checkout

**Classification:** WARNING  
**File:** docs/evidence/phase07-oracle-authority.md:12, 14; docs/evidence/phase07-oracle-authority.json:144-153

**Issue:** The request calls acquisition out-of-tree but places the oracle checkout and build under `build/private/phase07-oracle/...`, which is inside the repository root. `build/` is ignored, so an acquired checkout would also be hidden from ordinary `git status` review. This does not establish the promised boundary and risks placing upstream code inside the Tendies64 worktree despite the separate copy-provenance restrictions.

**Fix:** Put the proposed checkout/build root outside the repository and validate that the resolved path is not inside the worktree, or explicitly revise the requested boundary and explain how the local source copy is permitted and inspected.

#### WR-04: Private-path rejection only catches strings that begin with a path

**Classification:** WARNING  
**File:** scripts/check-rdp-oracle-authority.py:127-138

The private-path probe used embedded POSIX home paths, file URLs, and Windows home paths in nested prose. These authored values bypassed the old prefix-only guard.

**Fix:** Detect workstation paths anywhere in a string, including POSIX, `file:` URLs, and Windows drive paths, and add controls for nested fields and embedded paths. Keep the tree PII scan as an independent check.

#### WR-05: The positive synthetic control bypasses the admission path

**Classification:** WARNING  
**File:** scripts/check-rdp-oracle-authority.py:469-490, 573-580, 775-794

**Issue:** `approved-scoped-synthetic` calls `_validate_approved_decision` directly. No positive control calls `_require_local_approved` with a synthetic trusted pin, so the acceptance path's full record validation, local/publication separation, pin comparison, and dispatch are never exercised together. The integration test calls the CLI only for the currently blocked and forged-refusal cases. A regression in the actual positive pre-use path could leave the authority gate permanently unusable without failing these controls.

**Fix:** Add an isolated authored control that supplies a complete synthetic request/audit/decision and test-only pin, then traverses `_require_local_approved` end to end. Keep the real source pin unset and confirm the production CLI still refuses.

#### WR-06: The integration test does not require two selftest controls

**Classification:** WARNING  
**File:** scripts/tests/test_rdp_oracle_authority.py:139-157

**Issue:** The checker emits 18 named selftest controls, but this test requires only 16. It omits `incomplete-rights-audit-refusal` and `request-revision-pin`; removing either selftest and its assertion would not fail the outer test, even though these controls cover required rights and request-identity enforcement.

**Fix:** Require both omitted control names in the expected selftest output, or compare the complete exact control set and count.

---

_Reviewed: 2026-11-18T05:13:17Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: deep_
