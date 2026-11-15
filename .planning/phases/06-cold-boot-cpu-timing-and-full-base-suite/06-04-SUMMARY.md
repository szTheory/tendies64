---
phase: 06-cold-boot-cpu-timing-and-full-base-suite
plan: "04"
subsystem: cpu-cp0-scheduler-timing
status: complete
completed: 2026-11-15
requires:
  - phase: 06-03
    provides: Native RDRAM protocol and access/state ownership
provides:
  - Atomic T1 fetched-cache state on resource refusal
  - Fetched CP0, MDU and cross-device scheduler contract checks
  - Named deferred T2 evidence checklist
affects: [06-05, 06-08]
requirements-completed: []
actuals:
  tokens: 10441
  tasks: 2
  commits: 4
key-files:
  created: []
  modified: [core/t64_cpu.c, core/tests/test_cpu_time.c, core/CMakeLists.txt, docs/spec/cpu-timing.md]
coverage:
  - deliverable: T1 cache rollback and fetched CP0 ordering
    human_judgment: false
    verification:
      - {kind: ctest, ref: unit.core.cpu-time, status: pass}
      - {kind: ctest, ref: unit.core.cpu-cp0, status: pass}
  - deliverable: CPU/RSP/DMA and per-profile deterministic scheduler ordering
    human_judgment: false
    verification:
      - {kind: ctest, ref: unit.core.cpu-time, status: pass}
      - {kind: ctest, ref: unit.core.scheduler, status: pass}
      - {kind: ctest, ref: unit.core.determinism, status: pass}
---

# Phase 6 Plan 04: Fetched T1 timing and scheduler integration

A fetched instruction could fill its I-cache line before its exact successor
resource preflight refused. The meaningful RED reached tick overflow, full
queue and exhausted sequence refusals and observed cache mutation. The T1-only
fix restores the fetched line on those refusals, preserving legacy behavior.

Both new profiles now have literal fetched MDU, Wired/Random/TLBWR,
Count/Compare and exception assertions. The approved compat-v2 policy owns
Wired modulo64 and pre-step TLBWR indexing. Incorrect initial strict-profile
and division-by-zero expectations were corrected as invalid test oracles.

An authored DDIV interval contains actual RSP ADDIU retirement, equal-tick PI
completion before RSP issue, SP DMA completion, and interrupt sampling at the
CPU successor boundary. Idle Count/Compare and canonical digests agree within
each profile across advance partitions and independent interleaved instances.
The profiles' digests are not compared to each other.

## Verification and deviations

Both all-target builds passed. The executor's Task1 focused normal/ASan
selection passed, followed by the six-owner scheduler selection in both.
Parent closeout reran their ten-owner union after original final logs were
accidentally overwritten by inventory discovery before retention. This fresh
normal/ASan union passes; its original logs, output and hashes are retained
under ignored `build/gate/06-04-retained-checks/manifest.json`. It is a distinct
closeout run, not a reconstruction of the overwritten logs. Exact inventories
remain133/135. No broad gate was repeated.

The real public digest requires normally created READY machines. A minimal
CMake fixture environment/dependency change lets the test admit the exact
cleared IPL3 asset before running raw authored CPU/RSP words. This authorized
scope deviation replaces invalid stack/synthetic-admission fixtures; it does
not execute ROM boot. Intermediate compile, oracle and fixture failures remain
recorded in `build/gate/06-04-execution-checkpoint.md`. Task2 required no product
fix and introduced no behavioral RED; existing scheduling behavior passed the
new assertions. The T2 checklist records needed evidence, not implemented
pipeline/cache/bus timing.

Source commits are a7e39a5,7de2ff9,1dfcba6,643e590. Realized diff characters
per four supply the actuals estimate, not model billing. Protected owner edits
remain unchanged and unstaged.

## Remaining work

No actual ROM boot, external diagnostic, hardware qualification or T2 accuracy
claim follows from these native checks. CPU-08 remains pending phase-level
verification. The earlier stopped external attempt still has no valid member
result and its historical executable-retention gap remains disclosed.
Continue Plan05's authored Tier A/B hello comparison and determinism;
no user action is needed.
