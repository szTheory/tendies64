---
phase: 02-deterministic-machine-skeleton
plan: "05"
subsystem: core-bus
tags: [PI, DMA, scheduler, digest, trace, C17]
requires:
  - phase: 02-deterministic-machine-skeleton
    provides: checked bus, 4 MiB RDRAM, MI state, scheduler reservation and schema-v3 digest
provides:
  - queued atomic PI ROM-to-RDRAM DMA with immutable descriptors and completion tokens
  - cancellation, status-clear FIFO, refusal, trace and reset contracts
  - schema-v4 digest covering all PI future-affecting state
affects: [later device phases, CPU bus integration, deterministic-core tests]
actuals:
  tokens: 23633
  tasks: 2
  commits: 4
commits: 4
plan_head_before: 7aa121f8b056f5df9c7eb048476724df4325472c
plan_head_after: b5d5cecf2d02492f5e6a1b335a63d23306b85ff7
tech-stack:
  added: []
  patterns: [reserved completion before busy, immutable DMA descriptor, token-matched cancellation, explicit canonical device-state serialization]
key-files:
  created: [core/t64_pi.c, core/tests/test_pi_dma.c, .planning/phases/02-deterministic-machine-skeleton/02-05-task1-red-evidence.json, .planning/phases/02-deterministic-machine-skeleton/02-05-task2-red-evidence.json]
  modified: [core/t64_bus.c, core/t64_internal.h, core/t64_machine.c, core/t64_digest.c, core/CMakeLists.txt, core/tests/test_bus.c, core/tests/test_digest_trace.c, core/tests/test_profile.c, core/include/tendies64/t64_machine.h, docs/spec/machine-contract.md, docs/profile-limits.md]
key-decisions:
  - "Keep the `1 + N/2` deadline and atomic source-read-at-completion behavior as named profile policies, not hardware timing claims."
  - "Advance the profile and canonical state serialization from v3 to v4 because PI future state had no reserved encoding."
patterns-established:
  - "A queued device request reserves its completion and nonwrapping identity before exposing busy state."
  - "Cancellation preserves the reserved token/deadline; the matched completion skips writes and raises completion once."
requirements-completed: [BUS-01, BUS-02, CORE-01, CORE-02, CORE-06]
coverage:
  - id: D1
    description: "PI ROM DMA copies the full admitted source span atomically at its reserved guest-tick deadline and sets pending PI."
    requirement: BUS-02
    verification:
      - kind: unit
        ref: "unit.core.pi-dma; unit.core.bus"
        status: pass
      - kind: other
        ref: "./scripts/gate.sh"
        status: pass
    human_judgment: false
  - id: D2
    description: "PI cancellation, same-tick clear ordering, stale-token refusal and replacement reset preserve the specified future state."
    requirement: CORE-02
    verification:
      - kind: unit
        ref: "unit.core.pi-dma; unit.core.digest-trace; unit.core.scheduler"
        status: pass
    human_judgment: false
  - id: D3
    description: "PI latches, descriptor, cancellation state and tokens participate in canonical state schema v4."
    requirement: CORE-06
    verification:
      - kind: unit
        ref: "unit.core.digest-trace; unit.core.profile"
        status: pass
      - kind: other
        ref: "./scripts/gate.sh (core lint: zero violations)"
        status: pass
    human_judgment: false
duration: 39m54s
completed: 2026-11-10
status: complete
---

# Phase 02 Plan 05: Scheduled PI ROM DMA Summary

**Reserved PI DMA now copies admitted cartridge bytes at a deterministic completion event, with cancellation-safe identity and a versioned digest audit.**

## Performance

- **Duration:** 39m 54s.
- **Started:** 2026-11-10T18:57:31Z.
- **Completed:** 2026-11-10T19:37:25Z.
- **Tasks:** 2.
- **Files modified:** 15, measured from the plan commit range.

## Accomplishments

- Added PI register access and scheduled ROM-to-RDRAM transfers. Requests validate complete source/destination ranges and arithmetic, reserve completion capacity before busy state, and copy owned ROM bytes only at the inclusive model deadline.
- Added tests for register masks, 2-byte and 16-byte requests, a 4 MiB boundary transfer, semantic and resource refusal, immutable busy requests, cancellation at deadline minus one, same-tick status-clear FIFO, stale tokens, trace records, and no allocator calls during advancement.
- Audited reset/replacement and every PI future-state field. Bumped profile/state serialization to v4 and updated independently encoded profile and state goldens.
- Documented which PI register/alignment/count facts come from Nintendo SDK sources and which timing, raw-length, busy-start and cancellation behaviors are selected model policies.

## Task Commits

1. **Task 1 RED:** `495f3f6` — portable PI bus assertion exposed the unsupported register path.
2. **Task 1 GREEN:** `4a05b78` — scheduled PI ROM DMA implementation and focused transfer tests.
3. **Task 2 RED:** `4d7c4b2` — future-state digest assertion failed when the PI latch changed under schema v3.
4. **Task 2 GREEN:** `b5d5cec` — cancellation and state audit, v4 schema/profile goldens, and contract updates.

**Plan commits measured from the persisted ledger:** 4 (`7aa121f8b056f5df9c7eb048476724df4325472c` through `b5d5cecf2d02492f5e6a1b335a63d23306b85ff7`).

## Verification

- Focused PI DMA, bus, digest-trace, scheduler, and profile tests passed: 5/5.
- `./scripts/gate.sh` passed: 69/69 CTest cases and production core lint with zero violations.
- `./scripts/pii-scan.sh --tree` found no findings; staged diff whitespace checks passed.
- Exact model timing and atomic visibility are tested as profile behavior; no hardware cycle-accuracy claim is made.

## Decisions Made

- Kept PI duration `1 + N/2` guest ticks and source-read-at-completion as explicit model policies. Domain timing registers are observable latches but do not alter the chosen model equation.
- Versioned the state digest and selected profile as v4 because the PI latches, active descriptor, token and cancellation flags were not reserved in schema v3.

## Deviations from Plan

### Necessary integration updates

**1. Added narrow public trace constants and updated existing bus/digest/profile tests.**
- **Found during:** Tasks 1 and 2.
- **Reason:** The planned PI observer records need stable names, and newly serialized PI state changes both profile and default state golden values. Existing bus unsupported-window expectations and reset/digest audits also needed to follow the new device map.
- **Fix:** Added PI trace kind identifiers, adjusted the bus test, extended digest/reset coverage, and updated independently encoded v4 goldens and profile documentation.
- **Files modified:** `core/include/tendies64/t64_machine.h`, `core/tests/test_bus.c`, `core/tests/test_digest_trace.c`, `core/tests/test_profile.c`, `core/t64_machine.c`, `docs/profile-limits.md`.
- **Verification:** Focused bus, PI, digest-trace, scheduler and profile tests, followed by the full gate.

**Total deviations:** 1 necessary integration update. No new dependencies or protected data were used.

## TDD Gate Compliance

- **Task 1 RED:** `02-05-task1-red-evidence.json` records a portable TAP assertion and the pre-implementation PI register failure. GREEN: `4a05b78`; transfer and bus tests pass.
- **Task 2 RED:** `02-05-task2-red-evidence.json` records the unchanged v3 digest after PI latch mutation. GREEN: `b5d5cec`; schema-v4 mutation, golden, cancellation and reset tests pass.

## User Setup Required

None.

## Next Phase Readiness

The deterministic core now exposes the PI ROM transfer boundary and its model limitations. Later device work can use the same reservation, immutable-descriptor, token, digest, and observer-trace patterns.

## Self-Check: PASSED

- Summary file exists at the planned path.
- All four task commits are ancestors of the verified plan head.
- Stub-pattern scan of plan-created/modified source and contract files found no new placeholders.
- Both committed RED evidence records pass the evidence validator.

---
*Phase: 02-deterministic-machine-skeleton*
*Completed: 2026-11-10*
