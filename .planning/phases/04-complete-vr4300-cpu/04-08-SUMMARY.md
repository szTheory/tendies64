---
phase: 04-complete-vr4300-cpu
plan: "08"
subsystem: cpu
tags: [load-linked, store-conditional, dma, precise-faults, sanitizer]
requires:
  - phase: 04-07
    provides: Staged translated memory access and precise physical lane preflight.
provides:
  - Physical-address-and-width LL/LLD and one-shot SC/SCD reservations.
  - Committed writer overlap and targeted D-line invalidation with native tests.
affects: [phase-04-cache, phase-04-qualification]
actuals:
  tasks: 2
  commits: 5
plan_head_before: 81673b0d57dff288011cc33ecf4b08450dfd0aaa
plan_head_after: 7e09bed8f221aacdc711ae6d6f2037b124b5ac9d
tech-stack:
  added: []
  patterns: [Per-machine reservations, committed physical interval notifications]
key-files:
  created: [core/tests/test_cpu_link.c]
  modified: [core/t64_memory.c, core/t64_cpu.c, core/t64_bus.c, core/t64_internal.h, core/t64_pi.c, core/t64_sp.c, core/tests/test_cpu_path.c, core/CMakeLists.txt, scripts/check-test-inventory.sh, docs/spec/cpu-memory.md, docs/spec/cpu-profile.md]
key-decisions:
  - Exact physical start and width determine a one-shot conditional decision.
  - Actual committed byte overlap invalidates; adjacency and refused writes preserve links.
  - Reservation behavior and cache invalidation granule remain explicit model policy.
requirements-completed: ["CPU-05", "CPU-09"]
coverage:
  - id: D1
    description: Guest LL/SC programs observe one-shot decisions and scheduled PI/SP writes.
    requirement: CPU-05
    verification:
      - kind: unit
        ref: core/tests/test_cpu_path.c#unit.core.cpu-path
        status: pass
    human_judgment: false
  - id: D2
    description: Reservation aliases, widths, faults, overlap, cache invalidation and isolation have independent native assertions.
    requirement: CPU-09
    verification:
      - kind: unit
        ref: core/tests/test_cpu_link.c#unit.core.cpu-link
        status: pass
      - kind: integration
        ref: Common gate and affected instrumented checks
        status: pass
    human_judgment: false
duration: 57min
completed: 2026-11-12
status: complete
---

# Phase 4 Plan 8: LL/SC and Writer Invalidation Summary

`core/tests/test_cpu_link.c` exercises the selected reservation policy;
`docs/spec/cpu-memory.md` distinguishes that policy from primary-manual
facts and unresolved physical-console behavior.

## Accomplishments

- LL/LLD establish a physical start/width reservation after successful load;
  SC/SCD validate access before their exact, one-shot result/store decision.
  Physical aliases, reverse-endian lanes, source/result aliasing and rt=0
  retain their defined effects. Faulted/refused accesses preserve the link;
  completed conditional decisions consume it and ERET/reset clear it.
- Accepted bus, masked, PI and SP backing commits notify actual physical bytes.
  RDRAM, DMEM and IMEM are covered; adjacency and rejected writes preserve
  reservations. Targeted D-line invalidation applies the selected 16-byte
  physical line interval. This does not add DMA cache snooping.
- Existing v9 serialization already hashes reservation identity and CP0
  LLAddr. Native digest/isolation assertions verify that contract without an
  artificial serializer change.

## Task Commits

1. **Task 1: guest LL/SC and scheduled writers** - `396ae72` (test), `52457c3` (test), `60d8ba4` (feat).
2. **Task 2: reservation matrix and review corrections** - `26ba531` (test), `7e09bed` (feat).

## Automated Checks

- Task1 actual pre-implementation CPU-path JUnit reached LL after the
  existing16 cases: expected status0/GPR8=12345678, observed unsupported
  status14/GPR8=0. Exit8 and RED_EVIDENCE_OK preceded implementation.
  The expanded PI/SP guest tracers retained the same valid first failure.
- Task1 gate configure/build and CPU-path/PI-DMA/SP-DMA selection passed3/3;
  full all-target ASan build and the same instrumented selection passed3/3.
- Task2 actual pre-implementation CPU-link JUnit reached LL, targeted CACHE
  0x11 and SC. Expected invalidation of linked bytes[2004,2008); observed
  linked_valid1, SC1 and memorydeadbeef. Exit8 and RED_EVIDENCE_OK preceded
  the targeted D-line correction.
- Narrow parent review identified missing SP destination notifications and
  premature consumption on a refused SC. Added pre-fix CTest reproduced both.
  Corrected tests cover cacheable DMEM/IMEM aliases, direct/masked writes,
  scheduled SP writes into both banks and readonly-cartridge SC refusal.
- After review corrections, full all-target ASan build and all6 affected
  instrumented tests passed. The final common gate passed101/101 tests and
  core lint with zero violations.
- Exact inventory is101 gate/103 ASan; all84 integer coverage rows and
  omission controls remain intact. Portable evidence/specifications pass the unwaived PII tree/history scan.
- Final source7e09bed passed both Linux and macOS hosted gates:101 tests,
  zero lint violations, guest evidence and publication checks. Measured CI
  review is tracked in docs/ci-reviews/LOG.md. No external diagnostic ROM or
  derived result is public.

## Deviations and Evidence Limits

- A test-constant width error was corrected before valid Task1 RED. The SP
  tracer initially reached SC at tick11 before DMA completion at tick13;
  its guest instruction gap was corrected to exercise actual interference,
  and its instruction-count stop reason was asserted. A later SP regression draft
  used the wrong DMA direction; that zero-destination attempt was discarded,
  and corrected pre-fix runs established the actual invalidation defect.
  These setup repairs
  are distinguished from production defects.
- Review expanded writer coverage beyond RDRAM because cacheable aliases can
  establish reservations in SP backing. Both SP DMA directions and both banks
  now notify their actual destinations. SC consumes the link only after an
  accepted write or completed no-store decision, preserving it on bus refusal.
- Necessary CPU/header/spec edits were omitted from the original file list.
  The existing digest serializer needed no change. New tests use authored
  values and cleared fixtures; no external emulator source or ROM was used.
- One-shot matching, linked-byte overlap, fault/ERET effects and targeted
  16-byte invalidation remain selected deterministic model policy with
  documented reversal evidence. LLAddr bits[31:4] are separately cited.
  Native success does not establish silicon behavior or broad compatibility.
- The ASan build retains existing large-stack diagnostics in CPU, MMU,
  exception, digest, machine and DMA routines. No new warning was reported
  from this slice. Passing runtime checks do not establish general stack safety.
- CPU-05's unclassified evidence flag remains explicit pending qualification;
  no human validation or risk acceptance is invented.

## Next Work

Execute Plan09 functional I-cache and stale-code visibility. No user decision
is needed. Existing rights, physical-measurement and release stops remain.

## Self-Check: PASSED

