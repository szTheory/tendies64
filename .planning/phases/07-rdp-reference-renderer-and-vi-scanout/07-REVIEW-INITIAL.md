---
phase: 07-rdp-reference-renderer-and-vi-scanout
reviewed: 2026-11-16T11:10:50Z
depth: standard
files_reviewed: 33
files_reviewed_list:
  - core/CMakeLists.txt
  - core/include/tendies64/t64_machine.h
  - core/t64_bus.c
  - core/t64_cpu.c
  - core/t64_digest.c
  - core/t64_internal.h
  - core/t64_isviewer.c
  - core/t64_machine.c
  - core/t64_rdp.c
  - core/t64_scheduler.c
  - core/t64_vi.c
  - core/tests/test_rdp.c
  - core/tests/test_vi.c
  - docs/spec/rdp-vi-cases.json
  - docs/spec/rdp.md
  - docs/spec/vi.md
  - fixtures/manifest.json
  - fixtures/rdp/fill.S
  - fixtures/rdp/fill.ld
  - runner/CMakeLists.txt
  - runner/t64_real.c
  - runner/t64_real_fixture.c
  - runner/t64_real_fixture.h
  - runner/t64_result_check.c
  - runner/t64_supervisor.c
  - runner/t64_video_observation.h
  - runner/tests/test_rdp_video.c
  - scripts/build-rdp-fill-fixture.sh
  - scripts/check-rdp-vi-cases.py
  - scripts/check-rsp-no-hle.py
  - scripts/check-test-inventory.sh
  - scripts/tests/test_base_evidence.py
  - scripts/tests/test_rsp_no_hle.py
findings:
  critical: 2
  warning: 0
  info: 0
  total: 2
  necessity: 0
status: issues_found
---

# Phase 07: Code Review Report

**Reviewed:** 2026-11-16T11:10:50Z  
**Depth:** standard  
**Files Reviewed:** 33  
**Status:** issues_found

## Summary

Reviewed the explicit Plan 01 source cohort and its RDP/VI contracts. The new v14 profile's machine digest omits the profile's RDP/VI and inherited warm state, and the intended video queue digest also omits the queue's logical occupancy and order. Both defects can make distinct observable machine states share a digest. This was a static review; the recorded gate and focused tracer logs were not rerun and do not establish digest correctness. This review covers Plan 01 only and does not claim Phase 07 is complete. VID-04 remains blocked by the D5/asset audit recorded in the phase materials.

## Narrative Findings (AI reviewer)

### CR-01: v14 profile bypasses its machine-state digest block

**Classification:** BLOCKER  
**File:** `core/t64_digest.c:207-211, 314, 452`  
**Issue:** `has_v14_state` is true for the new RDP/VI profile, but `has_rsp_state` only recognizes v12 and v13, and `has_warm_state` is built from v6-v11 plus `has_rsp_state`. For v14, `has_warm_state` is therefore false, so the enclosing block at line 314 skips the entire v14 RDP/VI hashing block at line 452 as well as inherited warm/RSP state. For two otherwise identical admitted machines, a valid VI register write can change future video output while leaving `t64_machine_digest` unchanged. This violates the RDP/VI state-digest contract in `docs/spec/rdp.md:63-65` and `docs/spec/vi.md:56-59`.

**Fix:** Include v14 in the inherited state predicate so it also enables the enclosing block, for example:

```c
const bool has_rsp_state =
    has_v12_state || has_v13_state || has_v14_state;
```

Keep the existing version-specific predicates for legacy profiles intact.

### CR-02: v14 queue digest omits queue occupancy and head

**Classification:** BLOCKER  
**File:** `core/t64_digest.c:509-554`  
**Issue:** The v14 digest loop hashes each physical packet slot's length, bytes, and metadata, but never hashes `vi.packet_head` or `vi.packet_count` (`core/t64_internal.h:632-639`). Those fields determine which packet is oldest and whether the queue is empty. A successful `t64_video_copy` can change only the head/count while leaving the slot bytes and metadata in place (`core/t64_vi.c:519-527`); the next `t64_video_info` then changes from returning a packet to `T64_STATUS_NOT_READY`, and queue capacity for later fields changes. Even after CR-01 makes this block reachable, these observably different states can still share a digest. The contract explicitly includes queue state in the machine digest (`docs/spec/vi.md:56-59`).

**Fix:** Hash the queue head and count, then hash live entries in logical queue order from `packet_head` rather than treating every backing slot as a live packet.

---

_Reviewed: 2026-11-16T11:10:50Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: standard_
