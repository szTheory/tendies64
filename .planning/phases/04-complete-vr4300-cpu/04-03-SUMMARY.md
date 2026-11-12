---
phase: 04-complete-vr4300-cpu
plan: "03"
subsystem: cpu
tags: [integer, control-flow, portable-arithmetic, sanitizer]
requires:
  - phase: 04-02
    provides: Precise scheduled exceptions and full-width return state.
provides:
  - Full-width integer branch/link semantics and an arithmetic/delay-fault tracer.
  - Reviewed 84-row integer/control matrix with exact coverage rejection controls.
affects: [phase-04-cp0, phase-04-mmu, phase-04-qualification]
actuals:
  tasks: 2
  commits: 5
plan_head_before: 7f6414d1b33f3d34c4a80d00585e2e267e333c73
plan_head_after: f08e95c0c7a1f4fd927ce62cf317e28c138e41cd
tech-stack:
  added: []
  patterns: [Portable unsigned limb arithmetic, explicit opcode coverage inventory]
key-files:
  created: [core/tests/test_cpu_integer.c, docs/spec/cpu-instruction-coverage.md]
  modified: [core/t64_cpu.c, core/tests/test_cpu_path.c, core/CMakeLists.txt, scripts/check-test-inventory.sh]
key-decisions:
  - Manual-undefined division and branch-in-slot cases stop unsupported without partial architectural mutation.
  - REGIMM link writes PC+8 unconditionally; likely forms only annul the untaken slot.
requirements-completed: ["CPU-01", "CPU-02", "CPU-09"]
coverage:
  - id: D1
    description: Arithmetic, branch-delay overflow, guest handler, full-width JALR alias and likely annulment have exact scheduled observations.
    requirement: CPU-02
    verification:
      - kind: unit
        ref: core/tests/test_cpu_path.c#unit.core.cpu-path
        status: pass
    human_judgment: false
  - id: D2
    description: Reviewed integer/control opcode rows and independently expected cases execute, with missing-row/encoding/case/source rejection.
    requirement: CPU-01
    verification:
      - kind: unit
        ref: unit.core.cpu-integer; unit.core.cpu-control; check-test-inventory --selftest
        status: pass
      - kind: integration
        ref: Full ASan build and four affected CTests; final gate code checks; repaired unwaived gov.pii.repo
        status: pass
    human_judgment: false
duration: 52min
completed: 2026-11-12
status: complete
---

# Phase 4 Plan 3: Integer and Control Instructions Summary

The implementation in `core/t64_cpu.c` and matrix in
`core/tests/test_cpu_integer.c` execute the reviewed integer/control operations
through the canonical scheduled CPU and shared precise-exception path.

## Performance

Two tasks; six implementation/specification files and two portable RED records.
The observed first-RED-to-final-implementation commit span was 52 minutes
(2026-11-12T02:43:56Z to 2026-11-12T03:36:11Z), excluding preparation and parent
publication repair/closeout. The three later commit timestamps changed during
the documented unpublished-history repair; this span records the original
implementation events, not the replacement commits' creation time.

## Accomplishments

- Full-width branch targets and source/link alias handling preserve the authored
  DADDU/ADDU path, delay-slot overflow EPC/BD, handler store/retirement and
  untaken likely-slot annulment.
- Two registered matrix modes execute ALU/immediate/shift, compare/trap,
  HI/LO multiply/divide and integer jump/branch/link families with reviewed
  invalid encodings and defined SYNC behavior. The 84-row coverage table must
  have encoding, case and expected-source entries; four mutation controls
  reject omissions. Portable 32-bit limbs implement full products without
  compiler-specific wide integers or host undefined signed arithmetic.

## Task Commits

1. **Task 1: arithmetic/control tracer** - `9e5306c` (test), `cf27f29` (feat).
2. **Task 2: complete reviewed matrix** - `5984c21` (test), `55201c0` (feat), `f08e95c` (refactor).

## Automated Checks

- Task1's corrected registered tracer reached the actual JALR target assertion
  before target fetch: expected 00000001fffffffe, observed fffffffffffffffe.
  Arithmetic, overflow-slot handler, EPC/BD, store, retirement and likely-slot
  observations passed first. Actual JUnit received RED_EVIDENCE_OK.
- Task2's corrected registered test reached missing MULTU: ffffffff times2
  expected HI1 and sign-extended LOfffffffffffffffe, observed both zero.
  Earlier ALU/shift cases passed. Actual JUnit received RED_EVIDENCE_OK.
- Final focused CPU-integer/CPU-control/CPU-path/CPU-exceptions checks pass 4/4.
  Full all-target ASan build succeeds and those four tests pass under
  ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 and
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1. No full 98-test sanitizer
  rerun is claimed.
- Inventory selftest passes 194 removal controls and four coverage mutations;
  exact inventory is 96 gate/98 ASan, retaining the earlier names. Parent's
  post-repair coverage/inventory check again passes all 84 rows and 96 names.
- The sole final common-gate run passed 95/96 tests and core lint; only
  gov.pii.repo failed on the local citation path. After correcting only citation
  metadata and unpublished history, the unwaived gov.pii.repo test passes.
  All production C/test/registration/script bytes match the already verified
  implementation. A second local full gate is not claimed; fresh hosted
  verification of the sanitized source is the publication follow-up.

## Deviations and Issues

- First tracer run used the wrong store base register and faulted before the
  intended assertion. It was corrected and rerun; that attempt was not valid RED.
- Early matrix vectors included accidental ADD overflow, clobbered shift inputs,
  an incorrect DIVU remainder, and a conditional REGIMM link assumption. They
  were corrected. NEC pp.79,395–397 establish unconditional link writes for
  normal and likely REGIMM forms; the valid RED is the observed missing MULTU.
- The inventory selftest initially repeated the same coverage-table validation
  for every unrelated removal case. It was stopped, that validation was moved
  to once per selftest, and all removal/coverage failure controls then passed.
- ASan measured intermediate execute_special growth to3616 bytes. Extracting
  new arithmetic helpers reduced it to2336 versus2256 before Task2;
  t64_cpu_step is4704 versus4640. Remaining small nonfatal warnings are visible;
  affected runtime tests report no sanitizer failure.
- A workstation cache path was committed in the new manual citation. The final
  gate caught it before publication. A temporary waiver was removed without
  commitment; its temporary passing scan is not the accepted evidence. Parent
  rebuilt only the three unpublished offending/descendant commits in a temporary
  checkout using normal commits, preserving owner edits and all code bytes.
  Only the citation and its portable evidence metadata changed. The incorrect
  digest transcription was also corrected against the actual verified manual.
  Public history and the existing historical waiver file were unchanged.
  The final unwaived scanner passes. AUTONOMY now explicitly extends pre-commit
  path sanitization to specifications and citations.

## Evidence Limits and Readiness

The manual leaves signed quotient overflow and doubleword divide-by-zero
unspecified; these are explicitly unsupported unchanged-state cases, without
invented hardware outcomes. Undefined branch-in-slot forms are likewise
qualified. Native vectors and software policy are not hardware measurements.
The CPU-01/CPU-02 unclassified edge flags remain non-authoritative under
AUTONOMY, with no implied human validation. Shared requirement IDs remain
pending unfinished sibling plans. Ready for Plan04-04 CP0 behavior.

## Self-Check: PASSED

All five sanitized task commits, both RED records, the 84-row table and exact
test registrations exist. Owner instruction/configuration edits remain intact;
no new waiver or public-history rewrite was used. Hosted source verification
and parent state closeout are tracked separately.
