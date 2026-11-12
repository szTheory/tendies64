---
phase: 04-complete-vr4300-cpu
plan: "01"
subsystem: cpu
tags: [vr4300, prepared-profile, determinism, sanitizer]
requires:
  - phase: 03
    provides: Pinned cleared IPL3 and qualified legacy warm-profile fixtures.
provides:
  - Executable descriptor/model v3 CPU profile with the selected prepared seed.
  - Explicit v9 serialization and independent state, reset and isolation checks.
affects: [phase-04-exceptions, phase-04-mmu, phase-04-cop1]
actuals:
  tasks: 2
  commits: 4
plan_head_before: d53f8f5a13eb89b831170ca2585a5e2e3850f301
plan_head_after: 71bccef
tech-stack:
  added: []
  patterns: [Descriptor-selected seed, explicit versioned big-endian serialization]
key-files:
  created: [docs/spec/cpu-profile.md]
  modified: [core/t64_machine.c, core/t64_internal.h, core/t64_digest.c, core/t64_cpu.c, core/tests/test_boot_entry.c, core/tests/test_profile.c, core/tests/test_digest_trace.c]
key-decisions:
  - Preserve warm-v1 descriptor v2/state v8 while selecting descriptor/model v3/state v9 for the new CPU profile.
  - Prepared CP0 and clock values are software policy, without a hardware-reset accuracy claim.
requirements-completed: ["CPU-01", "CPU-03", "CPU-09"]
coverage:
  - id: D1
    description: New CPU profile admits the cleared payload and retires its real first ANDI.
    requirement: CPU-01
    verification:
      - kind: unit
        ref: core/tests/test_boot_entry.c#unit.core.boot-entry
        status: pass
    human_judgment: false
  - id: D2
    description: Explicit v3 metadata and v9 CPU state have independent golden, sensitivity, reset, isolation and observer-exclusion checks.
    requirement: CPU-09
    verification:
      - kind: unit
        ref: unit.core.profile; unit.core.boot-entry; unit.core.digest-trace
        status: pass
      - kind: integration
        ref: sh scripts/gate.sh; full ASan build and affected three CTests
        status: pass
    human_judgment: false
duration: 36min
completed: 2026-11-12
status: complete
---

# Phase 4 Plan 1: Executable CPU Profile Summary

The selected seed in `core/t64_machine.c` and explicit serialization in
`core/t64_digest.c` make the new CPU profile executable and identifiable.

## Performance

Two tasks; eight implementation/specification files and two portable RED evidence
records. The observed first-RED-to-final-implementation commit span was 36 minutes
(2026-11-12T00:24:10Z to 2026-11-12T01:00:08Z), excluding pre-commit preparation
and parent closeout. This is elapsed workflow time, not emulator throughput.

## Accomplishments

- The new profile validates descriptor v3, selects the documented CP0/GPR seed,
  admits the pinned cleared payload transactionally, and retires its actual
  entry ANDI through the canonical interpreter.
- State v9 explicitly includes full-width CPU addresses/outcome, paired TLB,
  timer phase, reservation, split caches and COP1 prepared state. Independent
  golden/reference, per-field mutation, reset and instance-isolation checks
  pass; the observer cursor remains excluded. Legacy warm-profile tests and
  all seven authored hello fixture identities remain unchanged.

## Task Commits

1. **Task 1: profile/entry** - `b9d547b` (test), `95d84d4` (feat): observed unknown-profile RED, then actual entry GREEN.
2. **Task 2: state identity** - `85da4e1` (test), `71bccef` (feat): observed prepared-state digest RED, then explicit v9 GREEN.

## Automated Checks

- Task 1 configured and compiled successfully, then the registered boot-entry
  test observed UNKNOWN_PROFILE with null output at the intended assertion.
  Its unchanged JUnit was classified RED_EVIDENCE_OK. The later focused
  boot-entry/profile pair passed.
- Task 2's registered boot-entry test reached the intended reference digest
  assertion: expected a8eb98f7b08cbaa900f2b9f8f8d197c5eca9aec3dff2ba2b5f78a7926c3aaa57,
  observed 46f12fc25bfd4f0a7bc8f2047b9e41b3d26f519270f555001436ffc63c25e5e7.
  Its unchanged JUnit was classified RED_EVIDENCE_OK. Raw reports stay ignored;
  04-01-task1-red-evidence.json and 04-01-task2-red-evidence.json retain only
  portable observations.
- Final focused boot-entry/profile/digest-trace selection passed 3/3. The final
  common gate passed 91/91 CTests and core lint reported zero violations.
- Full all-target ASan/UBSan configure/build succeeded. The same three affected
  tests passed with ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 and
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1. No full 93-test sanitizer
  rerun is claimed for this slice.

## Deviations and Issues

- Extended the existing digest-trace test by three lines to exercise observer
  exclusion through v9; this is the plan's named reuse seam, omitted from its
  files_modified list. No registration or CMake change was needed.
- Corrected a new test's allocation expectation from one to two owned
  allocations (machine storage and admitted image).
- The first sanitized Task 2 classifier input lacked required raw-output
  fields and was rejected; the actual registered test was rerun and classified
  from valid JUnit. The rejected evidence-format attempt was not treated as RED.
- Gate lint found strcmp references in the restructured profile lookup. A
  bounded exact matcher removed them; the final gate passed after that fix.
- The ASan preset emitted nonfatal frame-size warnings. The new serializer
  helper was 2736 bytes, the new seed preparer 4992 bytes, and the largest
  reported modified function was t64_machine_digest at 7648 bytes. Existing
  untouched functions also warned. No pre-plan ASan frame measurements were
  captured, so unchanged frame sizes are not claimed. These bounded functions
  do not allocate the enlarged CPU state on the stack; instrumentation warnings
  remain visible. Passing affected sanitizer tests establish no observed runtime
  fault, not a general proof of stack safety.

## Evidence Limits and Readiness

This slice prepares and hashes exception/TLB/cache/timer/COP1 state; it does not
implement those behaviors. Count remains retirement-stepped until its owning
later plan. Seed values and the manual's vector conflict remain explicitly
qualified software policy. The judgment-prohibition flag is non-authoritative
under AUTONOMY; no human validation or risk acceptance is asserted. No external
diagnostic source, ROM or output was used. Shared requirement IDs remain gated
on unfinished sibling plans. Ready for Plan 04-14, then Plan 04-02.

## Self-Check

All four implementation/test commits, the specification and both sanitized RED
records exist. Worker final evidence follows the last code change. Owner edits
to AGENTS.md and configuration remain unstaged; parent owns state closeout.
