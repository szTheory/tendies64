---
phase: 04-complete-vr4300-cpu
plan: "05"
subsystem: cpu
tags: [guest-time, timer, interrupts, determinism, sanitizer]
requires:
  - phase: 04-04
    provides: Staged CP0 transfers and pending-bit ownership.
provides:
  - Shared guest-tick Count synchronization and precise timer interrupt entry.
  - Same-boundary, wrap, idle, FIFO and partition-invariance coverage.
affects: [phase-04-mmu, phase-04-qualification]
actuals:
  tasks: 2
  commits: 3
plan_head_before: 4191eb3c9d0d727d68424c7979c2d99505c3b747
plan_head_after: 59a3e43
tech-stack:
  added: []
  patterns: [Shared event/idle clock synchronization, modular bounded timer arithmetic]
key-files:
  created: [core/tests/test_cpu_time.c]
  modified: [core/t64_cp0.c, core/t64_cpu.c, core/t64_internal.h, core/t64_scheduler.c, core/tests/test_cpu_path.c, core/CMakeLists.txt, scripts/check-test-inventory.sh, docs/spec/cpu-cp0.md, docs/spec/cpu-profile.md]
key-decisions:
  - Clock synchronization and new equality precede interrupt sampling; CP0 writes commit only for an executed instruction.
  - Treat the broader already-implemented Task2 behavior as verification-only GREEN, without manufactured RED history.
requirements-completed: ["CPU-03", "CPU-09"]
coverage:
  - id: D1
    description: A real Count/Compare guest path enters a handler without retiring the interrupt and resumes after Compare clear.
    requirement: CPU-03
    verification:
      - kind: unit
        ref: core/tests/test_cpu_path.c#unit.core.cpu-path
        status: pass
    human_judgment: false
  - id: D2
    description: Phase, equality, ownership, masks, preemption, FIFO and partition/numeric boundaries have exact native checks.
    requirement: CPU-09
    verification:
      - kind: unit
        ref: core/tests/test_cpu_time.c#unit.core.cpu-time
        status: pass
      - kind: integration
        ref: sh scripts/gate.sh; full ASan build and seven affected CTests
        status: pass
    human_judgment: false
duration: 26min
completed: 2026-11-12
status: complete
---

# Phase 4 Plan 5: Guest-Time Timer and Interrupts Summary

The shared synchronization in `core/t64_cp0.c` and boundary matrix in
`core/tests/test_cpu_time.c` make Count follow deterministic guest time through
CPU attempts and idle scheduler jumps.

## Performance

Two tasks; ten implementation/specification/registration files and two portable
evidence records. The observed first-RED-to-final-test commit span was 26
minutes (2026-11-12T05:02:01Z to 2026-11-12T05:28:05Z), excluding preparation
and parent closeout. The preceding CP0 lint repair is attributed to Plan04-04.

## Accomplishments

- New-profile Count advances once per two selected guest ticks, through idle
  time and non-retiring faults, using one synchronization primitive. Newly
  reached equality/IP7 precedes interrupt sampling; Compare clear wins at
  commit, Count writes retain phase and check written equality, and preempted
  instructions do not commit. MI/software/timer ownership remains distinct.
- The time matrix checks half-phase endpoints, equality and register writes,
  interrupt masks/owners, max-tick partitions and Count wrap, delay-slot CP0
  preemption, Random retirement-only stepping and equal-tick MI/CPU FIFO.
  Large jumps use bounded modular arithmetic rather than per-tick iteration.
  Warm-v1 retains its exact retirement-based Count behavior.

## Task Commits

1. **Task 1: timer handler tracer** - `7ff1309` (test), `889f637` (feat).
2. **Task 2: boundary matrix** - `59a3e43` (test).

The prerequisite lint repair `2867031` is recorded under Plan04-04 rather than
counted again as timer implementation.

## Automated Checks

- Task1 compiled, discovered and executed its registered CPU-path test. At
  tick8 it observed Count8, IP7 set, EPC0, Compare2 and no handler marker;
  expected the guest-time model's Count4 and completed interrupt-handler path.
  The actual unchanged JUnit received RED_EVIDENCE_OK.
- The post-tracer gate build and CPU-path/CPU-CP0/scheduler selection passed3/3;
  core lint passed with zero violations.
- Task2's corrected first behavioral run starts GREEN because Task1 already
  supplies the tested behavior. Its JUnit reports121 checks and zero failures;
  no RED is claimed. An earlier setup-invalid run is excluded. The plan's
  inapplicable Task2 TDD marker is removed with this rationale under AUTONOMY.
- Final full all-target ASan configure/build succeeded and the affected
  seven-target selection passed7/7 under
  ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 and
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1. No full100-test sanitizer
  rerun is claimed.
- The final common gate passes all98 registered tests and core lint. Inventory
  selftests pass with98 gate/100 ASan names, and84 opcode rows/four coverage
  mutation controls remain intact.

## Deviations and Issues

- Added the narrow internal clock declarations and updated the existing CP0/
  profile specifications outside the plan's file list to document its approved
  time ownership and selected clock mapping. No new public API was introduced.
- The implementation deliberately leaves the prepared equality deadline at
  UINT64_MAX until the first positive synchronization materializes the next
  strict equality, including a complete32-bit wrap from initial equality.
  This preserves the prepared seed identity and zero-time observation effects;
  the policy is explicit in cpu-profile.md.
- Task2 is verification-only, not an additional missing-behavior implementation.
  The portable GREEN record preserves its actual result and excludes the invalid
  setup attempt rather than relabeling it as an intended defect.
- Before timer work, the active executor fixed the preceding CP0 local-name
  lint failure in an isolated commit. Both exact hosted gates at2867031 then
  passed all97 tests, lint and publication checks. The failure and repair are
  retained in the CI log; AUTONOMY now requires executing previously unreached
  gate-tail stages after bounded repairs.

## Evidence Limits and Readiness

The manual's Count/Compare facts are separated from the selected1:1 CPU:RSP
and two-tick mapping. These native results do not establish physical clock
rates, instruction cycle costs, hardware accuracy or broad ROM compatibility.
Full TLB/cache/COP1 behavior remains later work. Shared requirement IDs stay
pending their unfinished sibling summaries. Ready for Plan04-06.

## Self-Check: PASSED

All three timer commits, the actual RED and GREEN evidence, current clock
specifications and exact registration exist. The full gate and affected
sanitizer checks follow the final source changes. Owner instructions and
configuration remain unstaged; parent owns state/roadmap and CI closeout.
