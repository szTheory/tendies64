---
phase: 02-deterministic-machine-skeleton
plan: "03"
subsystem: machine-scheduling
tags: [C, integer-time, deterministic-scheduler, SHA-256, observer-trace]

requires:
  - phase: 02-deterministic-machine-skeleton
    provides: Machine/profile lifecycle and transactional ROM admission from plan 02.
provides:
  - Bounded per-instance scheduler with inclusive guest-time and cumulative-counter stops.
  - Versioned field-wise digest of canonical future-affecting machine state.
  - Copy-only trace ring with FIFO drain, visible saturated drop accounting, and no digest effect.
affects: [02-deterministic-machine-skeleton, DMA, device-boundary-observation]

actuals:
  tokens: 25691
  tasks: 3
  commits: 6
commits: 6
plan_head_before: fffb1eea73f1e4c07384c649ad3901a5c8c41f42
plan_head_after: f5f958a5c13175c4a7a45c85e48d16f14c065f0d

tech-stack:
  added: []
  patterns:
    - Scheduler reservations preflight capacity, sequence and deadlines before queue mutation.
    - Canonical state is streamed as fixed-width big-endian fields in logical queue order.
    - Tracing is an optional bounded observer with no host callback in advance.

key-files:
  created:
    - core/t64_scheduler.c
    - core/t64_digest.c
    - core/t64_trace.c
    - core/tests/test_scheduler.c
    - core/tests/test_digest_trace.c
    - .planning/phases/02-deterministic-machine-skeleton/02-03-task1-red-evidence.json
    - .planning/phases/02-deterministic-machine-skeleton/02-03-task2-red-evidence.json
    - .planning/phases/02-deterministic-machine-skeleton/02-03-task3-red-evidence.json
  modified:
    - core/include/tendies64/t64_machine.h
    - core/t64_internal.h
    - core/t64_machine.c
    - core/CMakeLists.txt
    - core/tests/test_profile.c
    - docs/spec/machine-contract.md

key-decisions:
  - "Keep the approved machine timing and device policy fields unchanged; advance only state serialization to v2 for the expanded digest schema."
  - "Keep instruction and frame counter events explicitly synthetic; this plan supplies no CPU or VI execution evidence."
  - "Use a fixed 256-record observer ring that drops new records on overflow and exposes a saturating u64 drop count plus a saturation flag."

requirements-completed: [CORE-01, CORE-02, CORE-03, CORE-06, PROF-06]

coverage:
  - id: D1
    description: "Integer-only scheduling orders events by tick and enqueue sequence, applies bounded inclusive stops, and leaves deterministic refusal/resource-stop state."
    requirement: CORE-01
    verification:
      - kind: unit
        ref: unit.core.scheduler
        status: pass
    human_judgment: false
  - id: D2
    description: "The v2 state digest covers future-affecting fields in logical order and excludes storage layout and observer state."
    requirement: CORE-02
    verification:
      - kind: unit
        ref: unit.core.digest-trace
        status: pass
    human_judgment: false
  - id: D3
    description: "Optional trace records carry guest ticks, drain in FIFO order, expose overflow, and leave scheduler results and state digest unchanged."
    requirement: CORE-06
    verification:
      - kind: unit
        ref: unit.core.digest-trace
        status: pass
      - kind: other
        ref: "sh scripts/gate.sh"
        status: pass
    human_judgment: false

duration: 47min
completed: 2026-11-10
status: complete
---

# Phase 2 Plan 3: Deterministic Scheduler, Digest, and Trace Summary

**A bounded guest-time scheduler, canonical v2 machine digest, and observer-only trace ring are covered by focused synthetic tests.**

## Performance

- **Duration:** 47 minutes
- **Started:** 2026-11-10T17:26:07Z
- **Completed:** 2026-11-10T18:12:51Z
- **Tasks:** 3
- **Files changed:** 14 before this summary

## Accomplishments

- Added FIFO scheduling by `(tick, sequence)`, inclusive idle-time advancement, cumulative synthetic instruction/frame stops, halt precedence, and stable queue/tick/sequence/counter/budget limits.
- Replaced the partial state hash with a v2 streaming digest covering profile/content identity, parsed metadata, tick/counters/error/halt, RDRAM, and logical pending events. Added independent golden, mutation, exclusion, relocation, and partition-equivalence checks.
- Added per-machine trace enablement, admission and scheduler records, whole-record FIFO drains, invalid-output refusal, reset behavior, and visible saturating drop accounting.
- Extended the machine contract and refusal tests to include scheduler and observer state. No CPU, VI, or hardware-accuracy behavior was introduced.

## Task Commits

1. **Task 1: Dispatch exact bounded synthetic workloads** — `bedc31d` (RED), `0671ccf` (implementation).
2. **Task 2: Audit canonical future state and compare partitions** — `72b4e39` (RED), `5dcae5c` (implementation).
3. **Task 3: Add guest-time boundary tracing** — `24cbd45` (RED), `f5f958a` (implementation).

## Files Created or Modified

- `core/t64_scheduler.c` — bounded queue and deterministic stop processing.
- `core/t64_digest.c` — fixed-width canonical state encoding and SHA-256.
- `core/t64_trace.c` — observer ring, safe drain, and drop accounting.
- `core/tests/test_scheduler.c`, `core/tests/test_digest_trace.c` — synthetic scheduler, digest, trace, partition, reset, and refusal tests.
- `core/include/tendies64/t64_machine.h`, `core/t64_internal.h`, `core/t64_machine.c`, `core/CMakeLists.txt` — provisional API and instance integration.
- `core/tests/test_profile.c` — updated the expected profile hash for serialization version 2.
- `docs/spec/machine-contract.md` — scheduler stop/refusal semantics, digest field audit, and observer contract.
- Three `02-03-task*-red-evidence.json` files — portable assertion excerpts without raw JUnit or checkout paths.

## TDD Execution

- Task 1 RED failed because an unattainable instruction bound was rejected as an invalid stop kind; GREEN passed the focused scheduler suite.
- Task 2 RED showed that enqueuing future work left the prior digest unchanged; GREEN added an independently assembled digest golden and field-by-field checks.
- Task 3 RED showed an enabled trace drain returning no scheduler record. The unchanged JUnit report was checked transiently by the GSD classifier; only a sanitized, path-free assertion excerpt was committed. GREEN passed the trace suite.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Separate descriptor hashing from the fixed runtime queue limit**
- **Found during:** Task 1.
- **Issue:** General profile validation rejected a descriptor with queue capacity above 256, preventing profile serialization/hash mutation tests from exercising that field.
- **Fix:** Kept nonzero capacity validation for the descriptor and enforced the 256-entry maximum when creating a machine instance.
- **Files modified:** `core/t64_machine.c`.
- **Verification:** Profile, scheduler, and digest focused tests passed; the final gate passed.
- **Committed in:** `0671ccf`.

**Total deviations:** 1 auto-fixed bug.
**Impact on plan:** The runtime limit remains 256; descriptor identity can now be tested independently of whether a machine may be created from that descriptor.

## Issues Encountered

The convenience `core-lint --files` invocation lacked the project's shared include paths and could not compile its scoped file list. I did not repeat that scoped command; the canonical gate ran production core lint across 11 files with zero violations.

## Verification

`sh scripts/gate.sh` passed after the final source changes: configure/build, all 67 CTest cases, and production core lint. The focused scheduler and digest/trace suites also passed. The whole-tree PII scan reported no findings before the implementation commits.

## User Setup Required

None.

## Next Phase Readiness

Later device work can schedule completion events through one guest-time authority and extend the digest/trace field audits. Synthetic counters remain test seams and do not establish CPU or VI execution.

## Self-Check: PASSED

The summary file exists, all six task commits are ancestors of the current HEAD, the diff has no whitespace errors, and the PII tree scan reported zero findings.

---
*Phase: 02-deterministic-machine-skeleton*
*Completed: 2026-11-10*
