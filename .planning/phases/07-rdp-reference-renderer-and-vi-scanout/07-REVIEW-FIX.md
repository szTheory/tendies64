---
phase: 07-rdp-reference-renderer-and-vi-scanout
fixed_at: 2026-11-16T12:28:10Z
review_path: .planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-REVIEW-INITIAL.md
iteration: 1
findings_in_scope: 2
fixed: 1
skipped: 1
status: resolved_with_validation_caveat
---

# Phase 07: Code Review Fix Report

**Fixed at:** 2026-11-16T12:28:10Z  
**Source review:** [initial report](07-REVIEW-INITIAL.md)  
**Iteration:** 1

## Summary

Both critical allegations were checked against the implementation and the locked VI contract. CR-01 was disproven as written. CR-02 identified a real host-drain determinism defect, but hashing the host queue head and occupancy would make observer activity part of guest identity. The repair preserves packet bytes and metadata in the digest while preventing queue drains from changing guest scheduling or physical producer-slot history.

## Fixed Issues

### CR-02: Host video drains changed guest state and digest history

**Files modified:** `core/t64_internal.h`, `core/t64_scheduler.c`, `core/t64_vi.c`, `core/t64_digest.c`, `core/tests/test_vi.c`, `docs/spec/vi.md`, `docs/spec/rdp-vi-cases.json`, `scripts/check-rsp-no-hle.py`

**Applied fix:** When the packet ring is full, scheduler dispatch now stops before popping or tracing the pending VI event. Copying a packet only advances the observer cursor and frees a slot; it does not change guest time, the scheduler sequence, or the pending event's tick and order. The observer head is no longer reset when the ring becomes empty, so physical producer slots follow produced-frame history regardless of drain cadence. The digest continues to cover packet bytes and metadata in those stable physical slots, while excluding observer head and occupancy.

**Original allegation and disposition:** The review reported that omitted head/count hashing allowed distinct queue states to share a digest and proposed hashing those fields. That mechanism is not compatible with D06/D07's host-copy contract. The RED reproducer instead showed that `t64_video_copy` itself rescheduled a previously popped full-queue VI event, changing the digest and scheduler sequence; resetting the empty ring head also made packet-slot hashes depend on drain cadence. The new regression tests cover both confirmed defects without hashing host cursors or removing packet-history coverage.

**Commit:** `6752bba` (`fix(07): restore VI host-drain determinism`).

## Skipped Issues

### CR-01: v14 profile bypasses its machine-state digest block

**File:** `core/t64_digest.c:207-211, 314, 452`

**Reason:** Disproven against current source. The enclosing digest block is enabled by `t64_has_t1_policy(machine)`, which includes the v14 profile. The review's suggested change to `has_rsp_state` is unnecessary and would alter an unrelated inherited predicate. The authored regression confirms that a valid VI origin bus write changes the digest and that inherited warm boot state also changes it.

## Verification

All checks ran in the main project checkout.

- **RED:** `build/gate/07-vi-repair-red-LastTest.log` — confirmed copy-induced digest/scheduler mutation, cadence-dependent digest, and changed equal-tick event ordering. The VI and inherited-state digest sensitivity assertions passed, disproving CR-01. SHA-256: `3a75ad12aef76948113c65fd67eecaf60c87c2646d1e7f0dde8e5b1b5b150234`.
- **Focused GREEN:** authored VI tests passed with `unit.core.vi.fill`; `build/gate/07-vi-repair-green-focused-LastTest.log`. SHA-256: `797d0dea62a09708f44a3b7af0343598bc5fc94b7885279d2a29342df31c2cdd`.
- **Normal full gate:** `sh scripts/gate.sh` passed all 145 tests and gate checks. Owning LastTest: `build/gate/07-vi-repair-normal-LastTest.log` (SHA-256 `224fd8811e2f7fc4d66096a2bd78789431612cbc6d40824709909902416e68`); gate log: `build/gate/07-vi-repair-normal-gate.log`.
- **ASAN full gate:** build and inventory checks passed. Full CTest passed 146/147; only `harness.base-run` timed out in the four-job run. The recorded output shows long concurrent tests (`controls.hang`, `unit.core.rsp-control`, and `unit.core.digest-trace`) completing at 924–939 seconds. CTest's `/bin/ps: Operation not permitted` appeared after it declared the BASE timeout, during cleanup. Full LastTest: `build/asan/07-vi-repair-asan-LastTest.log` (SHA-256 `45ec0c2a604667bad5a325677403002a3ae570a3dcda45f2672e9a296a1f8722`); CTest log: `build/asan/07-vi-repair-asan-ctest.log`.
- **ASAN consumers retained:** exact copies of the VI/RDP tests, runner binaries, core archive, authored fixture, build log and CTest logs are bound in `build/gate/07-vi-repair-retained-artifacts/asan/binding.json`; all 12 recorded files were hash-verified. The prior Plan01 binding remains unchanged.
- **Isolated ASAN BASE rerun:** with the existing timeout and environment, `ctest --preset asan -R '^harness.base-run$' --output-on-failure` passed all 13 cases in 2.50 seconds. LastTest: `build/asan/07-vi-repair-asan-isolated-base-run-LastTest.log` (SHA-256 `1b96476568481e6eccd75fe58b36c53e8dcaf2aac5df56170256ac9a36c6a59e`). This supports a parallel-run contention timeout; the original full-suite failure remains recorded.
- **Seal and inventory checks:** the no-HLE source/object check passed for 76 source units with exact archive/object routes; the full normal gate's negative mutation controls passed. Test inventory and built-binary case-list checks passed for normal and ASAN presets.

---

_Fixed: 2026-11-16T12:28:10Z_  
_Fixer: the agent (gsd-code-fixer)_  
_Iteration: 1_
