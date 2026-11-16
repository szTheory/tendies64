---
phase: 04-complete-vr4300-cpu
plan: "02"
subsystem: cpu
tags: [exceptions, eret, precise-state, sanitizer]
requires:
  - phase: 04-01
    provides: Prepared VR4300 profile and explicit v9 state.
  - phase: 04-14
    provides: Closed authored diagnostic protocol and protected inventory.
provides:
  - Shared precise guest exception entry with non-retiring scheduled continuation.
  - Scheduled exception and full-width ERET controls preserving legacy behavior.
affects: [phase-04-integer, phase-04-cp0, phase-04-mmu]
actuals:
  tasks: 2
  commits: 8
plan_head_before: 86a56eac66cfb1f9a54eb55e52672b6fcb3243fe
plan_head_after: 296b6af
tech-stack:
  added: []
  patterns: [Reserve-before-commit guest exception entry, side-effect-free access preflight]
key-files:
  created: [core/t64_exception.c, core/tests/test_cpu_exceptions.c]
  modified: [core/t64_cpu.c, core/t64_internal.h, core/t64_bus.c, core/tests/test_cpu_path.c, core/CMakeLists.txt, scripts/check-test-inventory.sh]
key-decisions:
  - Keep architectural guest exceptions separate from model-unsupported operations and host scheduling/resource errors.
  - Preserve EPC and BD together under old EXL; select the documented bootstrap vector as explicit qualified policy.
requirements-completed: ["CPU-02", "CPU-03", "CPU-09"]
coverage:
  - id: D1
    description: Actual BREAK enters a handler and schedules continuation without retiring the fault.
    requirement: CPU-02
    verification:
      - kind: unit
        ref: core/tests/test_cpu_path.c#unit.core.cpu-path
        status: pass
    human_judgment: false
  - id: D2
    description: Named exception, priority, nested-state, return and scheduling-failure controls execute through the scheduled interpreter.
    requirement: CPU-03
    verification:
      - kind: unit
        ref: core/tests/test_cpu_exceptions.c#unit.core.cpu-exceptions
        status: pass
      - kind: integration
        ref: sh scripts/gate.sh; full ASan build and three affected CTests
        status: pass
    human_judgment: false
duration: 35min
completed: 2026-11-12
status: complete
---

# Phase 4 Plan 2: Precise Guest Exceptions Summary

The common entry in `core/t64_exception.c` and scheduled controls in
`core/tests/test_cpu_exceptions.c` distinguish guest faults from host/model
stops and continue the machine after a non-retiring exception.

## Performance

Two tasks; eight implementation/registration files and four portable RED
records. The observed first-RED-to-final-implementation commit span was 35
minutes (2026-11-12T01:53:53Z to 2026-11-12T02:28:53Z), excluding pre-commit
preparation and parent closeout. This is elapsed workflow time, not throughput.

## Accomplishments

- Scheduled BREAK enters an authored RDRAM handler, leaves retirement unchanged
  for the fault, and schedules its continuation. Shared exception entry reserves
  scheduling before committing Cause/EPC/EXL/BD, selects the BEV-dependent
  vector and cancels the delay state while preserving the old-EXL capture pair.
- The final 22-case scheduled suite covers named integer/RI/coprocessor faults,
  alignment and no-entry translation classification, Watch and instruction/data
  bus faults, ERET selection/clears/full-width target and scheduling refusal.
  Side-effect-free bus probes distinguish mapped unsupported device operations
  from guest bus faults without prematurely performing a load/store.

## Task Commits

1. **Task 1: scheduled BREAK handler** - `eac8ac5` (test), `2db6675` (feat).
2. **Task 2: named exceptions and return** - `b169b9c` (test), `2c5affd` (test), `a0b53e1` (test), `ddbe244` (test), `0d24eaa` (test), `296b6af` (feat).

## Automated Checks

- Task1's registered CPU-path test compiled, passed nine legacy assertions,
  then observed BREAK guest-completion at tick1 with zero retirement, Cause/EPC
  zero and no handler store. The intended failure was classified RED_EVIDENCE_OK.
- Task2's primary RED executed SYSCALL at the selected PC/tick and observed
  model-unsupported (reason7/outcome3), Cause/EPC unchanged and zero retirement
  rather than guest ExcCode8. Valid unchanged JUnit received RED_EVIDENCE_OK.
  Later trap/Watch controls retain their actual recorded RED observations;
  post-GREEN TLBS and LWC1/CpU controls have no claimed earlier RED history.
- The final focused gate build and CPU-exceptions/CPU-path/boot-entry tests pass.
  The exception target runs 22 cases. Inventory selftest passed 190 removal and
  five malformed controls, with exact 94 gate/96 ASan registered names.
- Full all-target ASan/UBSan configure/build succeeded, followed by all three
  affected tests passing under ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 and
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1. No full 96-test sanitizer
  rerun is claimed.
- The final single common gate run passed all 94 CTests, inventory/evidence
  controls and core lint. Raw JUnit/logs remain ignored and private.

## Deviations and Issues

- Added core/t64_bus.c beyond the plan's file list for the narrowly required
  read-free address-map/range probe; no public API or device implementation
  expansion was introduced. Existing v9 fields already cover this state, so
  no digest serializer edit was required.
- Additional test-first assertions were initially split into several commits
  and RED classifier cycles. Parent directed reuse of the valid primary RED
  and direct implementation, preserving the real ordering instead of inventing
  RED history for the two later controls.
- A normal task commit first encountered the workspace's read-only .git
  boundary. The authorized scoped escalation succeeded without bypassing hooks.
- The instrumented build emitted nonfatal frame-size warnings in CPU/existing
  core routines. No baseline-size equality or general stack-safety proof is
  asserted; affected runtime tests had no sanitizer failure.
- Parent corrected earlier summary task-reference formatting to the installed
  parser's canonical task rows so later scope checks can resolve exact commits.

## Evidence Limits and Readiness

Full TLB translation, caches and COP1 behavior remain in their later plans.
No-entry TLB classification here is not a claim that all mapped translations
work. The BFC00380 bootstrap general vector remains the selected table policy
with the manual prose conflict disclosed; the prepared BEV-clear handler test
uses 80000180. Legacy warm-v1 behavior remains under its own policy. No external
ROM/source result or hardware measurement was used. Shared requirement IDs stay
pending their unfinished sibling plans. Ready for Plan04-03.

## Self-Check: PASSED

All eight source/test commits, the new exception module, 22-case suite and
portable evidence records exist. Checks follow the last source change. Owner
instruction/configuration edits and the parent's CI review remain unstaged.
