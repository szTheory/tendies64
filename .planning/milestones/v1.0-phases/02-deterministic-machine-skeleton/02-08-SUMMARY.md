---
phase: 02-deterministic-machine-skeleton
plan: "08"
subsystem: core
tags: [n64, native-model, determinism, ctest, sanitizer]
requires:
  - phase: 02-deterministic-machine-skeleton
    provides: named machine profile, public machine API, PI/SP/MI model, strict model-result v1 checker
provides:
  - named-profile model probe with exact PI/SP/MI observations and legacy evidence rejection
  - cross-device deterministic workload tests with exact bytes, counters, refusal and trace checks
  - exact 73-test gate and 75-test ASan inventory enforced before public CTest
affects: [phase-02-verification, local-gate, test-inventory]
actuals:
  tokens: 29273
  tasks: 3
  commits: 6
  plan_head_before: adb80b90f0a004b6fb39cdb3149ed1484e534164
  plan_head_after: 11daadb90e09c6a7367c254f401c70864a9bb917
tech-stack:
  added: []
  patterns: [public-api integration tests, explicit CTest inventory, isolated inventory mutation controls]
key-files:
  created:
    - runner/t64_skeleton_probe.c
    - runner/tests/test_skeleton_probe.c
    - core/tests/test_determinism.c
    - scripts/check-test-inventory.sh
    - docs/evidence/phase02-core.md
    - .planning/phases/02-deterministic-machine-skeleton/02-08-task1-red-evidence.json
    - .planning/phases/02-deterministic-machine-skeleton/02-08-task2-red-evidence.json
  modified:
    - runner/CMakeLists.txt
    - core/CMakeLists.txt
    - docs/profile-limits.md
    - scripts/gate.sh
    - .planning/phases/02-deterministic-machine-skeleton/02-VALIDATION.md
key-decisions:
  - Keep t64.model-result v1 separate from guest result, identity, seal, suite and claims protocols.
  - Reset trace observation at a defined boundary; preserve canonical digest version 5.
  - Pin all 61 prior registrations and all 12 Phase 2 registrations explicitly; require 73 gate and 75 ASan tests.
  - Label synthetic counter results as native model evidence with no guest-execution or hardware claim.
patterns-established:
  - Real producer tests use independently encoded expectations and pass emitted records through the production checker.
  - Determinism checks compare exact bytes and state digests across execution and observation schedules.
  - Gate inventory selftests use isolated CTest trees and the same comparison function as the public gate.
requirements-completed: [CORE-01, CORE-02, CORE-03, CORE-06, PROF-01, PROF-02, PROF-03, PROF-04, PROF-06, BUS-01, BUS-02, BUS-04]
coverage:
  - id: D1
    description: Named profile probe reports separate profile/build identities and exact native PI/SP/MI observations.
    requirement: PROF-01
    verification:
      - kind: integration
        ref: "ctest --preset gate --no-tests=error -R '^unit\\.core\\.profile-result$'"
        status: pass
    human_judgment: false
  - id: D2
    description: Model records remain isolated from guest result, identity, seal, suite and claim evidence.
    requirement: PROF-02
    verification:
      - kind: integration
        ref: "unit.core.profile-result and unit.model-result in the final 73-test gate"
        status: pass
    human_judgment: false
  - id: D3
    description: Cross-device outputs and state remain equal across isolated, interleaved, partitioned and traced schedules.
    requirement: CORE-01
    verification:
      - kind: integration
        ref: "unit.core.determinism; final gate and ASan component run"
        status: pass
    human_judgment: false
  - id: D4
    description: Public gate rejects missing or malformed test inventories before running CTest.
    verification:
      - kind: other
        ref: "sh scripts/check-test-inventory.sh --selftest; final gate inventory 73/73"
        status: pass
    human_judgment: false
  - id: D5
    description: Profile limitations and local native-model evidence state unresolved hardware and boot behavior.
    requirement: PROF-06
    verification:
      - kind: other
        ref: "docs/profile-limits.md; docs/evidence/phase02-core.md; repo.docs"
        status: pass
    human_judgment: false
duration: 54min
completed: 2026-11-10
status: complete
---

# Phase 02 Plan 08: Final Determinism and Evidence Guard Summary

**Named-profile native evidence, cross-device determinism and exact 73/75-test gate enforcement for the N64 skeleton**

## Performance

- **Duration:** 54 minutes
- **Started:** 2026-11-10T21:17:54Z
- **Completed:** 2026-11-10T22:11:44Z
- **Tasks:** 3
- **Files modified:** 13, including this summary

## Accomplishments

- Added `t64-skeleton-probe` for `ntsc-cart-4m-skeleton`; its canonical `t64.model-result` v1 record carries independently checked profile/build identity, content/state digests, PI/SP deadlines and MI pending state. Unchanged guest result, identity, seal, suite and claims paths refuse those bytes, including the valid-suite-footer case; positive legacy controls and v1 goldens pass.
- Added `unit.core.determinism` for exact PI/SP bytes, one-shot/partitioned and interleaved schedules, synthetic bounds, PI cancellation, SP promotion/source changes, refusal immutability, trace equality/overflow and zero host allocations during `advance`.
- Added a POSIX inventory checker to the existing gate. It pins all 61 earlier test names plus the 12 Phase 2 names, rejects malformed or disabled enumerations, and verifies each of the 73 gate and 75 ASan names is required by an isolated removal control.
- Recorded measured local results and model limits in [phase evidence](../../../docs/evidence/phase02-core.md), [profile limits](../../../docs/profile-limits.md) and the updated [validation ledger](./02-VALIDATION.md).

## Task Commits

1. **Task 1: named-profile probe and legacy rejection** — `71384ab` (RED), `a1354a1` (GREEN).
2. **Task 2: cross-device determinism** — `48c0d2f` (RED), `4d544bf` (GREEN).
3. **Task 3: exact inventory gate** — `3fcfc01`.

Measured evidence documentation: `11daadb`. The final metadata commit contains this summary and is recorded in the repository history after its self-check.

## Files Created/Modified

- `runner/t64_skeleton_probe.c`, `runner/tests/test_skeleton_probe.c` — named-profile model producer and real consumer/claims seam tests.
- `core/tests/test_determinism.c` — cross-device determinism and refusal/observer checks.
- `scripts/check-test-inventory.sh`, `scripts/gate.sh` — closed expected test inventory and pre-CTest gate enforcement.
- `docs/profile-limits.md`, `docs/evidence/phase02-core.md` — measured native-model evidence with unsupported and unknown behavior retained.
- `.planning/phases/02-deterministic-machine-skeleton/02-VALIDATION.md` — measured local validation map, with hosted CI and independent review explicitly pending.

## Decisions Made

- Native model records remain a separate protocol and cannot be promoted into guest or support claims.
- The cross-device trace count is measured at a reset observation boundary; canonical digest serialization remains v5.
- The exact CTest names are authored explicitly rather than inferred from the live test list.
- No protected test assets, dependencies, CI jobs or machine-accuracy claims were introduced.

## TDD Gate Compliance

- **Task 1 RED:** `02-08-task1-red-evidence.json` was accepted by `gsd_run check tdd-red-evidence` as `RED_EVIDENCE_OK`. The target test ran and failed its intended producer-success assertion because the producer executable was not implemented yet. GREEN passed the producer/checker/legacy-seam tests.
- **Task 2 RED:** `02-08-task2-red-evidence.json` was accepted as `RED_EVIDENCE_OK`. The target determinism test executed 1,931 assertions and failed only its exact trace-record and overflow-count assertions. Inspection showed setup and precondition reads were inside the captured trace window. The test now resets trace at the completion observation boundary and pins five resulting records and 49 dropped records; the final target passes with 1,933 assertions.
- **Task 3:** The prescribed selftest is the inventory checker’s command-line test and invokes the same comparison used by the public gate. A separate CTest registration was not added because that would change the exact inventory under test. An initial selftest attempt exposed incorrect CTest test-file syntax in its isolated fixture; that fixture was fixed before the implementation commit, and the final selftest passed 148 removal controls plus five malformed-inventory controls. This fixture failure was not counted as valid RED evidence.

## Deviations from Plan

### Environment adjustment

- The plan requested `ASAN_OPTIONS=detect_leaks=1`, but LeakSanitizer is unsupported in this host environment. The final planned ASan/UBSan component, fuzz and negative-control run used `detect_leaks=0`; address and undefined-behavior instrumentation remained active, the fixed campaign stayed under 120 seconds, and no campaign work was reduced.

### Auto-fixed test harness issue

- **Found during:** Task 3 inventory selftest.
- **Issue:** The isolated CTest fixture used the configure-time `add_test(NAME ... COMMAND ...)` form, which CTest interpreted as a test named `NAME` in a raw `CTestTestfile.cmake`.
- **Fix:** Emit the native CTest test-file form and require every negative control to match its specific rejection reason.
- **Files modified:** `scripts/check-test-inventory.sh`.
- **Verification:** 148 individual removal controls and five malformed-inventory controls passed.
- **Committed in:** `3fcfc01`.

**Total deviations:** one environment adjustment and one selftest-fixture correction. No production behavior was changed outside the plan.

## Issues Encountered

- The test-only trace counts were initially based on a wider trace window; exact assertions were corrected after isolating the completion records. No machine implementation change was needed.
- The output wrapper around the final gate attempted to assign zsh’s reserved `status` variable and returned an error after the gate process finished. The captured gate log ends with `[gate] PASS`, including 73/73 CTests; the reporting-wrapper error did not affect the gate.

## User Setup Required

None.

## Next Phase Readiness

The three Plan 02-08 tasks and local checks are complete. The parent orchestrator still owns independent code review, ASVS L1 security review, goal-backward verification, hosted Linux/macOS CI, push and phase completion. This summary does not claim those checks passed, and no user action is needed for the local work.

---
*Phase: 02-deterministic-machine-skeleton*
*Completed: 2026-11-10*

## Self-Check: PASSED

- Summary, validation ledger and measured evidence files exist.
- All six task/evidence commits listed above are ancestors of the plan head.
