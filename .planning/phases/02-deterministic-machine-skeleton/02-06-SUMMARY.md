---
phase: 02-deterministic-machine-skeleton
plan: "06"
subsystem: deterministic-core
tags: [N64, SP, DMA, scheduler, deterministic-core, digest-v5]
requires:
  - phase: 02-deterministic-machine-skeleton
    provides: checked RDRAM bus, scheduler event reservations, PI DMA patterns, canonical state digest v4
provides:
  - checked bidirectional SP DMA with selected-bank wrapping and completion-time source reads
  - immutable active and pending requests with reserved completion capacity and visible third-request drops
  - canonical SP future-state serialization in profile and digest schema v5
affects: [later device phases, CPU bus integration, deterministic-core tests]
actuals:
  tokens: 19622.75
  tasks: 2
  commits: 6
commits: 6
plan_head_before: 8c809fff5910207a5a95ea22916adc294ded012b
plan_head_after: c8bc680e364b25c5a70f6fee44aa05fb706af17c
tech-stack:
  added: []
  patterns: [reserve completion capacity before acceptance, immutable queued DMA descriptors, completion-time source reads, independent model goldens]
key-files:
  created: [core/t64_sp.c, core/tests/test_sp_dma.c, .planning/phases/02-deterministic-machine-skeleton/02-06-red-task1.json, .planning/phases/02-deterministic-machine-skeleton/02-06-red-task2.json]
  modified: [core/t64_internal.h, core/t64_bus.c, core/t64_machine.c, core/t64_digest.c, core/CMakeLists.txt, core/include/tendies64/t64_machine.h, core/tests/test_bus.c, core/tests/test_digest_trace.c, core/tests/test_profile.c, docs/spec/machine-contract.md, docs/profile-limits.md]
key-decisions:
  - "Use the selected 12 + payload/8 RSP-cycle model equation with the profile's 1:1 integer CPU tick conversion; this is not a hardware-accuracy claim."
  - "Accept one pending SP request by reserving its event at issue; promotion reuses that event, and a third request only updates its length latch and emits a drop trace."
  - "Advance profile and state serialization from v4 to v5 because SP latches, active/pending descriptors, and token state were not reserved in the prior schema."
patterns-established:
  - "Validate every complete DMA row and the full deadline before mutating request state or reserving externally visible busy state."
  - "Queued device work stores immutable request fields and reads source bytes only at its reserved completion tick."
requirements-completed: [BUS-02, CORE-01, CORE-02, CORE-06]
coverage:
  - id: D1
    description: "SP DMA validates and copies rectangular rows in either direction at a deterministic completion tick, with selected-bank wrap and source-at-completion visibility."
    requirement: BUS-02
    verification:
      - kind: unit
        ref: "unit.core.sp-dma"
        status: pass
      - kind: other
        ref: "./scripts/gate.sh"
        status: pass
    human_judgment: false
  - id: D2
    description: "Active and pending work retain reserved completion capacity through queue pressure, reset, third-request refusal, and same-tick PI/SP ordering."
    requirement: CORE-02
    verification:
      - kind: unit
        ref: "unit.core.sp-dma; unit.core.pi-dma; unit.core.scheduler; unit.core.bus"
        status: pass
      - kind: other
        ref: "./scripts/gate.sh"
        status: pass
    human_judgment: false
  - id: D3
    description: "SP latches, active and pending descriptors, and token state participate in canonical digest and profile schema v5."
    requirement: CORE-06
    verification:
      - kind: unit
        ref: "unit.core.digest-trace; unit.core.profile; unit.core.sp-dma"
        status: pass
      - kind: other
        ref: "./scripts/gate.sh (production core lint)"
        status: pass
    human_judgment: false
duration: 33m22s
completed: 2026-11-10
status: complete
---

# Phase 02 Plan 06: Scheduled SP DMA Summary

**SP DMA now copies validated rows at model completion ticks, preserves two accepted requests under queue pressure, and records all future-affecting SP state in digest v5.**

## Performance

- **Duration:** 33m 22s.
- **Started:** 2026-11-10T19:43:16Z.
- **Completed:** 2026-11-10T20:16:38Z.
- **Tasks:** 2.
- **Files modified:** 15, measured from the persisted plan commit range.

## Accomplishments

- Added checked SP DMA in both directions. Length rounds up to an 8-byte boundary, count adds one row, skip advances each next DRAM row, the selected DMEM or IMEM bank wraps at 4096 bytes, and each complete RDRAM row is bounds-checked before acceptance.
- Made transfers visible only at scheduled completion. Tests independently calculate expected rows and deadlines, verify source mutations before completion, exercise maximum fields and endpoint bounds, and confirm no SP interrupt is raised by DMA completion.
- Added immutable active and pending descriptors. Both completion events are reserved on acceptance; promotion reuses the pending reservation. A third request updates only its specified length latch and emits a drop trace; scheduler capacity, tick, sequence, token, or row failures leave request state unchanged.
- Covered reset/content replacement, stale events, no allocation while advancing, trace ordering, and equal-tick PI/SP FIFO behavior. Updated the independent profile and state goldens and documented the selected policies and evidence limits.

## Verification

- Focused SP DMA, PI DMA, scheduler, digest/trace, profile, and bus suite passed: 6/6.
- The canonical `./scripts/gate.sh` passed: all 70 CTest cases passed and production core lint reported zero violations.
- `./scripts/pii-scan.sh --tree` reported zero findings; `git diff --check` passed.
- SP DMA has 11 TAP cases, including independently derived row addresses, bytes, deadlines, refusal atomicity, queue promotion, digest-field mutations, reset, and same-tick ordering.

## Task Commits

1. **Task 1 RED attempt:** `486fc29` — added the delayed-copy test slice. Its fixture initially omitted content admission, so this was not treated as accepted RED evidence.
2. **Task 1 fixture correction:** `55d30fd` — admitted synthetic content in the fixture before production changes.
3. **Task 1 RED:** `b4a1b8b` — captured portable failures for row shape, direction, wrap, and bounds.
4. **Task 1 GREEN:** `8761e44` — implemented checked, scheduled SP DMA and bus integration.
5. **Task 2 RED:** `298c60f` — added pending queue and digest-v5 failure cases.
6. **Task 2 GREEN:** `c8bc680` — implemented guaranteed promotion, reset/digest coverage, independent goldens, and contract documentation.

**Plan commits measured from the persisted ledger:** 6 (`8c809fff5910207a5a95ea22916adc294ded012b` through `c8bc680e364b25c5a70f6fee44aa05fb706af17c`).

## Decisions Made

- The selected transfer deadline is 12 plus payload bytes divided by 8 RSP cycles, converted with the chosen profile's 1:1 integer tick ratio. This is an explicit deterministic model policy, not a hardware accuracy claim.
- Internal transfer addresses wrap in the selected 4 KiB SP bank. Rows are read and written in increasing row/byte order, with source reads at completion.
- The named third-request policy preserves already accepted work and exposes the drop through the length latch and trace; it does not invent an SP guest error bit or assert SP pending.
- State and profile serialization use v5 because the added SP latches, descriptors, and token state were not reserved in v4.

## Deviations from Plan

None. Narrow bus, lifecycle, public trace-name, profile-golden, digest-golden, and profile-limit updates were required integrations for the planned SP registers, reset behavior, tracing, and schema bump.

## Issues Encountered

The first task 1 test fixture did not admit its synthetic content, so its failure did not prove the intended SP behavior. The fixture was corrected and the semantic RED evidence was regenerated before production implementation. No unresolved issue remains.

## TDD Gate Compliance

- **Task 1 RED:** `02-06-red-task1.json` records portable TAP failures after the fixture correction; the semantic RED evidence check passed before implementing SP behavior.
- **Task 1 GREEN:** `8761e44` implements row decoding, checked bounds, completion-time copying, and bus routing; the focused SP and bus tests passed.
- **Task 2 RED:** `02-06-red-task2.json` records pending-queue and digest-state failures; the semantic RED evidence check passed before queue/state implementation.
- **Task 2 GREEN:** `c8bc680` implements reserved pending promotion and v5 state coverage; focused integration tests and the final gate passed.

## User Setup Required

None.

## Next Phase Readiness

The core exposes deterministic SP DMA register, copy, queue, trace, reset, and digest behavior for later device integration. Timing and bank-wrap policies remain selected model behavior with the limitations recorded in the machine contract; this plan makes no hardware-accuracy claim.

## Self-Check: PASSED

- Summary path is the planned `.planning/phases/02-deterministic-machine-skeleton/02-06-SUMMARY.md`.
- All six task commits are ancestors of the recorded plan head.
- The source/docs placeholder scan found no TODO, FIXME, or placeholder text.
- Both sanitized RED evidence files were accepted by the semantic evidence check before their corresponding implementation work.

---
*Phase: 02-deterministic-machine-skeleton*
*Completed: 2026-11-10*
