---
phase: 03-first-headless-rom-through-open-ipl3
plan: "07"
subsystem: emulator-observation
tags: [C17, CMake, ISViewer, deterministic-input, output-drain]
requires:
  - phase: 03-06
    provides: warm machine state, per-instance output capture, canonical v7 state identity, and trace queue
provides:
  - provisional public output drain and scheduled trace-only input seams
  - bounded caller-copy validation, observer cursor lifecycle, and drain-schedule digest invariance
  - explicit trace-only input policy bound to boot-profile identity
affects: [phase-03-headless-runner, phase-03-profile-qualification, machine-contract]
actuals:
  tokens: 16846
  tasks: 1
  commits: 2
plan_head_before: 77b19eda956e76502566582ae6b63d0157364427
plan_head_after: acee5a1c4516f4bcf72abc2add46d2c8b38a5431
tech-stack:
  added: []
  patterns:
    - observer-only drain cursors are excluded from canonical guest identity
    - trace input reserves scheduler capacity before committing its event
key-files:
  created:
    - core/t64_observe.c
    - .planning/phases/03-first-headless-rom-through-open-ipl3/03-07-seams-red.json
  modified:
    - core/include/tendies64/t64_machine.h
    - core/t64_internal.h
    - core/t64_machine.c
    - core/t64_digest.c
    - core/tests/test_boot_entry.c
    - core/tests/test_isviewer.c
    - core/tests/test_tracer.c
    - fixtures/boot-contract.json
    - docs/spec/machine-contract.md
key-decisions:
  - "Keep the v7 serialized layout: the drain cursor is observer-only, while pending input event tuples are already represented in v7."
  - "Expose port-0 input as a scheduled trace event with signed 8-bit axes; it does not update SI/controller state or invoke host input."
  - "Preserve the existing test-inventory registration and baseline because unit.core.isviewer was already present."
patterns-established:
  - "Validate every caller span, overlap, and owned-memory alias before any output copy or result mutation."
  - "Keep retained output lifetime totals and sticky loss independent of host drain timing."
requirements-completed: []
requirements-pending: [RUN-01, RUN-05, PROF-05]
metrics:
  duration: 47min
  completed: 2026-11-11
  status: complete
---

# Phase 3 Plan 07: Bounded Output Drain and Timed Trace Input Summary

**Added checked caller-owned output copying and deterministic trace-only input submission without changing the v7 serialized state layout.**

## Created Paths

- `core/t64_observe.c` implements the public output drain and timed input submission seams.
- `.planning/phases/03-first-headless-rom-through-open-ipl3/03-07-seams-red.json` records the behavioral RED evidence.

## Performance

- **Duration:** 47 minutes
- **Started:** 2026-11-11T10:50:20Z
- **Completed:** 2026-11-11T11:37:15Z
- **Tasks:** 1
- **Files modified or created:** 11

## Accomplishments

- Added `t64_machine_output_drain` with required-size reporting, bounded chunk copying, retained-prefix SHA-256, total retained byte count, and sticky capture-loss reporting. Invalid or aliased result/data spans are rejected before changing caller memory or machine state; short buffers copy and consume nothing.
- Added `t64_machine_queue_input` for scheduled port-0 trace events with `uint16_t` buttons and signed 8-bit axes. Event insertion uses existing reserve-before-commit scheduler checks and never changes SI/controller state or calls host input.
- Kept drain position outside canonical guest identity and reset it with guest/output lifecycle. Tests establish that drain and trace schedules do not change the canonical digest when guest-visible queued input and output are otherwise identical.
- Bound the explicit input and observer policy to the active boot-policy identity, then independently rederived profile and initial-state expectations with the test reference encoders. Preserved legacy synthetic identity and v6 meaning; the serialized state remains v7.
- Used the existing `unit.core.isviewer` inventory entry and baseline; no duplicate inventory entry or new runtime dependency was needed.

## Automated Checks

- Behavioral RED: the first failure was the named ready-instance behavior, “ready output drain returns the captured guest bytes”; the seeded instance reached the assertion and the API stub returned `T64_STATUS_RESOURCE_EXHAUSTED` instead of `T64_STATUS_OK`. The evidence classifier accepted it as `RED_EVIDENCE_OK`.
- Focused gate tests passed: ISViewer, digest/trace, scheduler, tracer, CPU path, CP0/cache, bus, warm-device policy, and boot entry (9/9).
- Full gate suite passed (78/78); inventory remained at the expected 78 gate and 80 ASan tests.
- Fresh ASan/UBSan build and full test suite passed (80/80), including the sanitizer negative control. The final guest-output integration assertion was rebuilt and rerun under both gate and ASan/UBSan (1/1 each).
- Core lint passed for 19 files with 0 violations, including after the final integration assertion.
- Boot-contract identity consistency, `git diff --check`, and the tree PII scan passed; the scan covered 486 files with no findings.
- No hosted CI run or push was needed for this slice. The true runner and final hosted cross-host proof remain outside this plan’s evidence.

## Task Commits

1. **Task 1 RED: behavioral output and input seam checks** — `c8d4b2f` (`test`)
2. **Task 1 GREEN: bounded output drain and timed trace input** — `acee5a1` (`feat`)

The plan metadata and state update are recorded in a separate documentation commit after this summary’s self-check.

## Files Created/Modified

- `core/t64_observe.c` — output drain and scheduled trace-only input implementation.
- `core/include/tendies64/t64_machine.h` — provisional public seam declarations and contracts.
- `core/t64_internal.h`, `core/t64_machine.c`, `core/t64_digest.c` — per-instance observer cursor lifecycle and canonical-state separation.
- `core/tests/test_isviewer.c` — behavioral API, alias/refusal, lifecycle, ordering, and digest-invariance checks.
- `core/tests/test_boot_entry.c`, `core/tests/test_tracer.c` — independent identity expectations and event classification checks.
- `fixtures/boot-contract.json`, `docs/spec/machine-contract.md` — active policy identity and caller/lifecycle contract.
- `.planning/phases/03-first-headless-rom-through-open-ipl3/03-07-seams-red.json` — portable RED evidence.

## Decisions Made

- The output cursor remains host observer state and is excluded from v7 guest identity; pending input event fields already fit the serialized v7 event tuple.
- Input axes are bounded to `[-128, 127]`, and input events are trace-only at their scheduled tick and queue order.
- Output drain remains available after terminal CPU stop so already captured bytes can be observed, while queue submission rejects terminal machines.
- Existing test inventory registration and baselines remain authoritative.

## Deviations from Plan

The inventory script did not need a change: `unit.core.isviewer` was already registered in both gate and ASan inventories. Preserving it avoided a duplicate entry. No other plan deviations.

## Issues Encountered

The initial scaffold returned a resource-exhausted status for the ready-instance RED assertion, as intended. The implementation then passed the focused and integrated checks. The fresh sanitizer build also emitted existing large-stack-frame warnings in digest, machine, CPU, and SP sources; the observer implementation introduced no such warning.

## Next Phase Readiness

The host observation/input seams and their refusal and digest invariants are implemented and locally verified. `RUN-01`, `RUN-05`, and `PROF-05` remain pending until evidence from the true runner and final hosted cross-host proof exists. A captured transcript ending at unsupported CPU BREAK is not evidence of guest completion or a passing ROM.

## Self-Check: PASSED

Both created paths exist, and both task commits are ancestors of the current plan head.
