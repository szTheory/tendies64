---
phase: 04-complete-vr4300-cpu
plan: "07"
subsystem: cpu
tags: [memory, unaligned-merges, reverse-endian, precise-faults, sanitizer]
requires:
  - phase: 04-06
    provides: Paired TLB translation and precise memory fault context.
provides:
  - Staged integer scalar and merge memory accesses with exact byte effects.
  - Independent endian/merge/fault tests and documented RE model limits.
affects: [phase-04-link, phase-04-cache, phase-04-qualification]
actuals:
  tasks: 2
  commits: 5
plan_head_before: b1b71da79fcdd7cf3f5ea313c6d4e8e2dcd27148
plan_head_after: 2294c27
tech-stack:
  added: []
  patterns: [Staged memory effects, width-preserving bus dispatch]
key-files:
  created: [core/t64_memory.c, core/tests/test_cpu_memory.c, docs/spec/cpu-memory.md]
  modified: [core/t64_cpu.c, core/t64_bus.c, core/t64_internal.h, core/tests/test_cpu_path.c, core/tests/test_cpu_exceptions.c, core/CMakeLists.txt, scripts/check-test-inventory.sh, docs/spec/cpu-profile.md]
key-decisions:
  - RE uses physical byte lanes; inferred width-four rule remains model policy.
  - A merge pair commits one instruction at a time.
requirements-completed: ["CPU-04", "CPU-05", "CPU-09"]
coverage:
  - id: D1
    description: Page-edge merges preserve exact bytes and prior valid pair effects on a later fault.
    requirement: CPU-05
    verification:
      - kind: unit
        ref: core/tests/test_cpu_path.c#unit.core.cpu-path
        status: pass
    human_judgment: false
  - id: D2
    description: Scalar lanes, every merge offset, fault priority and refusal boundaries have independent expected values.
    requirement: CPU-04
    verification:
      - kind: unit
        ref: core/tests/test_cpu_memory.c#unit.core.cpu-memory
        status: pass
      - kind: integration
        ref: Common gate and affected instrumented tests
        status: pass
    human_judgment: false
duration: 95min
completed: 2026-11-12
status: complete
---

# Phase 4 Plan 7: Integer Memory Summary

`core/t64_memory.c` stages aligned scalar and all eight unaligned merge
operations; `core/tests/test_cpu_memory.c` checks independent byte tables,
mode restrictions, reverse-endian selection and precise fault boundaries.

## Accomplishments

- Translation and preflight precede destination/store commit. Page-edge pairs
  preserve the first instruction's valid effect when their second member
  faults. Stores preserve all unselected sentinel bytes.
- User RE selects physical lanes within the aligned eight-byte container.
  Kernel/fetch behavior stays BE. Scalar accesses preserve their transaction
  width; unsupported wide/masked MMIO is refused before device mutation.
- Watch read/write controls and EXL masking, delay-slot context, 32-bit-mode
  doubleword RI and physical range faults have native assertions.
- The public [memory contract](../../../docs/spec/cpu-memory.md) records the
  selected lanes, primary references, inference and reversal evidence.

## Task Commits

1. **Task 1: page-edge memory path** - `e2a54d7` (test), `bdeddc9` (feat).
2. **Task 2: complete matrix and boundary review** - `4866d60` (feat), `521f554` (fix), `2294c27` (docs).

## Automated Checks

- Task1's actual pre-implementation JUnit reached page-edge LWL: expected
  ffffffffdead7788, observed0000000011227788. The contemporaneous execution
  report recorded RED_EVIDENCE_OK before implementation; retained unchanged
  evidence also reclassified successfully. Its run/report timestamps are
  retained without inventing an independent classifier timestamp.
- The provisional Task2 fixed-address byte-swap vector was semantically wrong
  and is excluded, along with a stale executable attempt. Corrected actual
  pre-implementation RE LBU expected88 from PA+7, observed11 from PA+0;
  classifier RED_EVIDENCE_OK preceded the corrected implementation.
- Parent review found scalar RE preflight probed the original physical lane.
  An actual cartridge-end regression reproduced unsupported stop instead of
  guest DBE; its unchanged JUnit received RED_EVIDENCE_OK before the correction.
  LBU/LHU/LW boundary cases now check the actual selected physical range.
  DBE leaves BadVAddr unchanged under the existing CP0 policy.
- The initial completed common gate passed100/100 tests, exact100 gate/102
  ASan inventory, all84 integer rows, coverage controls and23-file lint with
  zero violations. Full all-target ASan build and11 affected tests passed.
- After the bounded preflight correction, full all-target ASan build and6
  affected CPU-memory/path/MMU/bus/exception/integer tests passed. The final common gate passed100/100 tests and lint with zero violations.
- Hosted source4866d60 passed both Linux and macOS:100 tests, zero lint
  violations, guest evidence and publication scans. The exact correction
  source521f554 then passed the same two hosted lanes and all100 tests,
  lint and publication checks. Both measured runs are tracked in
  docs/ci-reviews/LOG.md.
- Portable notes and public specifications pass the unwaived tree PII scan.
  No external diagnostic ROM, capture, result or tool receipt was published.

## Deviations and Evidence Limits

- Primary review rejected the provisional RE interpretation. Byte/halfword
  physical XOR and unchanged doubleword transfer are explicit in the manual;
  LW/SW's abbreviated entries omit RE positioning. The width-four XOR rule
  is a coherent inference supported by LL/LWC/SWC and LWR/SWR tables, not a
  silicon measurement. Every discarded vector remains distinguished from
  valid behavioral RED evidence.
- GREEN development corrected a bad LWL register-index expectation, a store
  fixture reset that wiped earlier writes, a preserved RI destination vector,
  and WatchLo's read bit. Final review fixed width-four RE merge preflight
  indexing and then the scalar lane check. The bounded regression's original
  BadVAddr expectation was corrected; its DBE-vs-unsupported assertion remains
  the valid observed defect.
- Existing exception tests and two public specifications required narrow
  omitted-file edits. No new persistent state required a serializer change.
- ASan reports4992-byte stage_memory_effect and3296-byte decode_memory_effect
  frames, above the2048-byte diagnostic threshold. These remain measured
  nonfatal warnings, alongside prior MMU/exception warnings; runtime checks
  passed without establishing general stack safety.
- LL/SC, cache fill/writeback and COP1 remain later slices. Native model
  assertions do not establish physical accuracy or broad ROM compatibility.
  The CPU-05 unclassified flag remains explicit pending later qualification.

## Next Work

Execute Plan08 LL/SC and committed external-writer overlap policy. No owner
decision is needed for that local work. Existing external-asset, physical
measurement and release-channel stops remain in force.

## Self-Check: PASSED

