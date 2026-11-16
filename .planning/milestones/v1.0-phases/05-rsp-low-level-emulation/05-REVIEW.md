---
phase: 05-rsp-low-level-emulation
reviewed: 2026-11-14T15:23:05Z
depth: deep
files_reviewed: 63
files_reviewed_list:
  - .github/workflows/rsp-stress.yml
  - LICENSES/ISC.txt
  - core/CMakeLists.txt
  - core/t64_bus.c
  - core/t64_digest.c
  - core/t64_internal.h
  - core/t64_isviewer.c
  - core/t64_machine.c
  - core/t64_rsp.c
  - core/t64_rsp_divider.c
  - core/t64_rsp_divider.h
  - core/t64_sp.c
  - core/tests/test_cop1.c
  - core/tests/test_rsp_compare.c
  - core/tests/test_rsp_control.c
  - core/tests/test_rsp_divider.c
  - core/tests/test_rsp_fuzz.c
  - core/tests/test_rsp_multiply.c
  - core/tests/test_rsp_scalar.c
  - core/tests/test_rsp_transfers.c
  - core/tests/test_rsp_vector.c
  - core/tests/test_sp_dma.c
  - docs/evidence/phase05-authored-rsp.md
  - docs/evidence/rsp-divider-provenance.md
  - docs/spec/rsp-diagnostic.md
  - docs/spec/rsp-instructions.md
  - docs/spec/rsp-profile.md
  - docs/spec/rsp-smoke.md
  - fixtures/manifest.json
  - fixtures/rsp/rsp-smoke.S
  - fixtures/rsp/rsp-smoke.ld
  - provenance/copies.json
  - runner/CMakeLists.txt
  - runner/t64_cpu_diag_protocol.c
  - runner/t64_cpu_diagnostic.c
  - runner/t64_diag_common.c
  - runner/t64_diag_common.h
  - runner/t64_rsp_diag_protocol.c
  - runner/t64_rsp_diag_protocol.h
  - runner/t64_rsp_diagnostic.c
  - runner/t64_rsp_smoke.c
  - runner/tests/test_cpu_diag_protocol.c
  - runner/tests/test_fixture_descriptor.c
  - runner/tests/test_rsp_diag_protocol.c
  - scripts/audit-rsp-divider.py
  - scripts/build-boot-fixtures.sh
  - scripts/build-qualified-rsp-diagnostic.py
  - scripts/build-rsp-smoke.sh
  - scripts/check-ci-yml.sh
  - scripts/check-rsp-no-hle.py
  - scripts/check-test-inventory.sh
  - scripts/prepare-rsp-diagnostic.py
  - scripts/rsp-stress.sh
  - scripts/run-rsp-diagnostic.py
  - scripts/test-rsp-smoke.sh
  - scripts/tests/test_rsp_build.py
  - scripts/tests/test_rsp_divider_audit.py
  - scripts/tests/test_rsp_no_hle.py
  - scripts/tests/test_rsp_qualification.py
  - scripts/tests/test_rsp_run.py
  - scripts/verify-rsp-diagnostic-build.py
  - scripts/verify-rsp-diagnostic-qualification.py
  - tools/lint/core-lint.sh
findings:
  critical: 1
  warning: 1
  info: 0
  total: 2
  necessity: 0
status: issues_found
---

# Phase 05: Code Review Report

**Reviewed:** 2026-11-14T15:23:05Z  
**Depth:** deep  
**Files Reviewed:** 63  
**Status:** issues_found

## Summary

The review found one refusal-atomicity defect in the fetched RSP issue path and one gap in the no-HLE mutation guard. The first can let an already-active DMA beat change memory and queue state during an RSP operation that is subsequently refused. The second leaves effect-producing helper bodies outside the semantic route digests and mutation tests. Plan 10's final evidence checker and final artifact gates remain pending; this report does not claim an aggregate diagnostic pass.

## Narrative Findings (AI reviewer)

### CR-01: [BLOCKER] A refused MTC0 DMA request commits the active DMA step first

**File:** `core/t64_rsp.c:2113-2138,2192-2203`; `core/t64_sp.c:179-195,425-505`; contract: `docs/spec/rsp-profile.md:104-107`
**Classification:** BLOCKER
**Issue:** `t64_rsp_issue_event` preflights and commits the currently active DMA step at lines 2113-2138 before it performs the fetched instruction's CP0 write at lines 2192-2203. For MTC0 c2/c3, that bus write reaches `start_rsp_dma`, whose queue-capacity and DRAM-span refusals happen at `core/t64_sp.c:179-195`. If a nonconflicting final beat of an already accepted DMA is due in the same opportunity, `t64_sp_rsp_dma_commit` can write memory and complete/promote the queue at lines 460-505 before the new invalid-span or full-queue request is refused. This contradicts the documented refusal contract that invalid spans and full queues are rejected before memory or queue mutation. It is a deterministic software-contract violation; it does not establish anything about hardware arbitration.
**Fix:** Stage and validate the MTC0 c2/c3 descriptor and admission conditions before committing the active DMA step, then commit the instruction opportunity atomically. Add a fetched MTC0 refusal case with a final active beat and queued descriptor; assert that refusal leaves memory, descriptors, tokens, and queue promotion state unchanged.

### WR-01: [WARNING] No-HLE guards do not cover effect-producing helper bodies

**File:** `scripts/check-rsp-no-hle.py:67-78,314-337`; `scripts/tests/test_rsp_no_hle.py:98-129`; representative helpers: `core/t64_rsp.c:586-760,852-1095`
**Classification:** WARNING
**Issue:** The normalized body digests cover seven dispatch/entry routes, but not effect-producing decode/planning helpers such as `rsp_decode_vector_basic`, `rsp_decode_divider`, and `rsp_decode_vector_multiply`. The token scan rejects known task/signature/content-identity names, while the mutation suite inserts its anonymous DMEM-conditioned effect at the `t64_rsp_issue_event` fetch anchor. A DMEM-conditioned fabricated effect added inside an unpinned helper can therefore leave the route digests unchanged and avoid the named-token scan and existing mutation cases, despite the plan requiring detection of task/signature-based replacement mutations.
**Fix:** Extend the semantic route guard to the effect-producing helper bodies (or an equivalent reviewed helper inventory), and add a mutation case that inserts an anonymous DMEM-conditioned effect into a decode helper and requires rejection.

---

_Reviewed: 2026-11-14T15:23:05Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: deep_
