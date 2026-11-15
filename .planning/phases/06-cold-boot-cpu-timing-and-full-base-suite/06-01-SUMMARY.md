---
phase: 06-cold-boot-cpu-timing-and-full-base-suite
plan: "01"
subsystem: cold-profiles-and-cpu-timing
status: complete
completed: 2026-11-15
requires:
  - phase: 05
    provides: Functional RSP and approved compat-v2 CPU policies
provides:
  - Two explicit cold/T1 profile identities and state13 domain
  - Versioned static instruction-class timing and transactional successor preflight
affects: [06-09, 06-03, 06-04]
requirements-completed: []
actuals:
  tokens: 13788
  tasks: 2
  commits: 6
key-files:
  created: [docs/spec/cpu-timing.md]
  modified: [core/t64_machine.c, core/t64_internal.h, core/t64_digest.c, core/t64_cpu.c, core/t64_exception.c, core/tests/test_profile.c, core/tests/test_boot_entry.c, core/tests/test_cpu_time.c]
---

# Plan 01 — Explicit cold profiles and T1 boundaries

Both permanent A/B profiles execute the canonical core, inherit compat-v2 CPU
and functional RSP capabilities, and use distinct boot-policy identities in a
shared new state domain. Historical profile descriptors/state domains remain
separate. Tier A has the selected preset; Tier B starts with RI_SELECT zero.
The cold register protocol and actual IPL3 startup are subsequent work.

The new profiles use issue intervals 1, 6, 38, 9 and 70 for ordinary, MULT,
DIV, DMULT and DDIV classes respectively, including unsigned forms. The NEC
manual establishes stalls; adding one issue interval is versioned software
policy, not a hardware measurement. Successor tick/queue/sequence resources
are checked before instruction effects; exceptions retain the base interval.
Old profiles keep their timing. [Timing specification](../../../docs/spec/cpu-timing.md)
records sources, Count/Random ownership and T2 limits.

## Verification

Meaningful REDs establish missing profile lookup and the old tick1 MDU
schedule while valid opcodes retire with expected results. An initial invalid
MULT encoding was corrected before the retained behavioral RED. Literal raw
instruction fields and stall values are anchored to the primary manual.

All-target normal and ASan/UBSan builds precede focused checks. Profile,
boot-entry and digest owners pass for the first task. Final CPU timing,
exceptions and profile owners pass in both configurations after final source
and test edits. Tests cover all eight MDU opcodes on both new profiles, a
delay-slot DIV successor at tick40, base exception/interrupt intervals without
retirement/Random stepping, and preflight refusals preserving tested HI/LO,
PI-store, CP0 Count and exception state. Final original logs and task1 JUnit
are retained under ignored `build/gate/06-01-retained-checks/`; the manifest
records their hashes and semantic source. No source-annotation-only rerun was
claimed. Documentation, rights/PII tree scans and diff checks pass.

The six source/test commits and exact commands/results are retained in
`build/gate/06-01-execution-checkpoint.md`. Final implementation source is
7dab8a6. Actuals use realized diff characters divided by four, not provider
token billing. Owner instruction/config edits stayed unchanged and unstaged.

## Remaining work

Execute Plan09's bounded cleared-byte cold probe next, before qualification
and protocol expansion. BUS-03 and CPU-08 remain phase-level pending: no
actual cold startup, whole-suite result, final review, hosted run or hardware
qualification is established by this plan. No user action is needed.
