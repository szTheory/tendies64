---
phase: 04-complete-vr4300-cpu
plan: "12"
subsystem: cpu
tags: [VR4300, COP1, FCR31, FR0, FR1, TDD]

# Dependency graph
requires:
  - phase: 04-11
    provides: portable, context-owned SoftFloat conversion seam
  - phase: 04-17
    provides: independent raw arithmetic matrix and COP1 machine-state context
provides:
  - COP1 transfers, FCR0/FCR31 control, FR register pairing, CVT.D.W, and precise FPU traps
  - BC1F/T/FL/TL through the canonical branch and delay-slot path
  - Inventory-owned COP1 regression coverage and sanitized behavioral RED records
affects: [04-13 COP1 arithmetic, 04-18 CPU source verification, VR4300 CPU]

# Actuals; token estimate is changed-diff characters divided by four.
actuals:
  tokens: 17824
  tasks: 2
  commits: 5
plan_head_before: 47bbf88c99002bb0d6628c605fcddd4eec66ea99
plan_head_after: 9d71c3e5e58e34705bb4f2aaa3fcadf5e3dc87c0

# Tech tracking
tech-stack:
  added: []
  patterns:
    - COP1 stages register and memory effects before architectural commit
    - COP1 branch conditions use the canonical CPU delay and target state
key-files:
  created:
    - core/t64_cop1.c
    - core/tests/test_cop1.c
    - .planning/phases/04-complete-vr4300-cpu/04-12-RED-evidence.md
  modified:
    - core/t64_cpu.c
    - core/t64_memory.c
    - core/t64_internal.h
    - core/tests/test_cpu_path.c
    - core/CMakeLists.txt
    - scripts/check-test-inventory.sh
key-decisions:
  - "Keep FCR31 and FPR state in each machine; the raw arithmetic context does not own architectural control state."
  - "Recognize COP1 branches only in the VR4300 profile so warm-v1 retains its prior refusal behavior."
patterns-established:
  - "BC1 uses the same branch delay, target, likely annulment, and exception path as integer branches."
  - "FR=0 doubleword FPR values pair even/odd words; FR=1 keeps independent 64-bit registers."
requirements-completed: [CPU-02, CPU-07, CPU-09]
coverage:
  - id: D1
    description: "COP1 transfers, conversion, cached FPR memory, and guest trap effects work with independent expected values."
    requirement: CPU-02
    verification:
      - kind: unit
        ref: "unit.core.cpu-path; unit.core.softfloat-isolation"
        status: pass
    human_judgment: false
  - id: D2
    description: "COP1 register layouts, control fields, branches, memory faults, and exception priority are covered."
    requirement: CPU-07
    verification:
      - kind: unit
        ref: "unit.core.cop1-state; unit.core.cpu-control; unit.core.cpu-memory"
        status: pass
    human_judgment: false

# Metrics
duration: 58min
completed: 2026-11-12
status: complete
---

# Phase 4 Plan 12: COP1 State, Transfers, Memory, and Branch Summary

**VR4300 COP1 transfers, FR pairing, FCR controls, cached FPR memory, precise traps, and BC1 delay behavior are implemented and covered.**

## Performance

- **Duration:** about 58 minutes from dispatch to handoff
- **Started:** approximately 2026-11-12T15:03:00Z
- **Completed:** 2026-11-12T16:00:31Z
- **Tasks:** 2
- **Files modified:** 9

## Accomplishments

- Added MFC1/DMFC1/MTC1/DMTC1, CFC1/CTC1, LWC1/LDC1/SWC1/SDC1, FR=0 pairing, FR=1 independent lanes, CVT.D.W, FCR0 identity, FCR31 masking, and Cause/Enable/E trap handling.
- Routed BC1F/T/FL/TL through the existing branch pipeline. Tests cover taken and not-taken paths, delay execution, target execution, likely annulment, delay-slot faults, CU1 priority, and invalid encodings.
- Added `unit.core.cop1-state`, registered it in the test inventory, and added warm-v1 refusal coverage. Tests cover all FPR indices using decoded MTC1/MFC1 readback, FCR31 walking bits and all five Cause/Enable pairs, and positive/negative memory paths.
- Kept the transfer, conversion, and memory implementation within the existing SoftFloat source closure; no new copied arithmetic code or external diagnostic was used.

## Task Commits

1. **Task 1: Move a COP1 value through FPR memory and an exception handler** — `1d10132` (RED test/evidence), `8cbef8b` (implementation); [test_cpu_path.c](../../../core/tests/test_cpu_path.c#L2419), [t64_cop1.c](../../../core/t64_cop1.c#L1).
2. **Task 2: Cover register layout, control fields and COP1 branches** — `9730078` (RED test/evidence), `6b64b2f` (corrected branch target vector and recaptured RED), `9d71c3e` (implementation/tests); [test_cop1.c](../../../core/tests/test_cop1.c#L1), [t64_cpu.c](../../../core/t64_cpu.c#L463).

## Files Created/Modified

- [t64_cop1.c](../../../core/t64_cop1.c) — COP1 register/control ownership, transfer/conversion decode, and precise operation traps.
- [t64_cpu.c](../../../core/t64_cpu.c) — canonical COP1 branch handling and result commit, with warm-v1 gating.
- [t64_memory.c](../../../core/t64_memory.c) — shared staged MMU/cache path for COP1 memory-width operations.
- [t64_internal.h](../../../core/t64_internal.h) — COP1 result and decoder interfaces.
- [test_cpu_path.c](../../../core/tests/test_cpu_path.c) — end-to-end transfer/conversion/memory/handler tracer and warm-v1 refusal check.
- [test_cop1.c](../../../core/tests/test_cop1.c) — branch, FCR/FPR layout, transfer, memory, and trap assertions.
- [CMakeLists.txt](../../../core/CMakeLists.txt) and [check-test-inventory.sh](../../../scripts/check-test-inventory.sh) — exact `unit.core.cop1-state` registration.
- [04-12-RED-evidence.md](./04-12-RED-evidence.md) — portable task-level RED evidence.

## TDD Gate Compliance

- **Task 1 RED:** The complete `unit.core.cpu-path` matrix reached MTC1 opcode `0x44880800` at canonical PC `0xffffffff80001000`; it expected retirement but observed status 14, Cause `0x00000000`, model-unsupported outcome, and zero retirement. The unchanged JUnit report was classified `RED_EVIDENCE_OK`; its raw report and JSON remain in ignored private diagnostics.
- **Task 2 RED:** `unit.core.cop1-state` fetched BC1T opcode `0x45010002` at canonical PC `0xffffffff80001000`; it expected a delay target at `0xffffffff8000100c` but observed RI Cause `0x00000028`, EPC `0xffffffff80001000`, and handler PC `0xffffffff80000180`. The unchanged JUnit report was classified `RED_EVIDENCE_OK` before implementation.
- Both task RED records reached the planned guest instruction and behavior assertion. Their commands, timestamps, independent expectations, and sanitized actuals are in [04-12-RED-evidence.md](./04-12-RED-evidence.md).

## Automated Checks

- `sh scripts/check-test-inventory.sh --selftest` — passed; exact removal controls covered 108 gate and 110 ASan names.
- `sh scripts/check-test-inventory.sh --preset gate` — passed; inventory count 108.
- `tools/lint/core-lint.sh` (invoked with sh) — passed; 83 production files, zero violations.
- `cmake --preset gate && cmake --build --preset gate` — full gate all-target build passed.
- `ctest --preset gate --no-tests=error -R '^(unit\.core\.cpu-path|unit\.core\.softfloat-isolation|unit\.core\.cop1-state|unit\.core\.cpu-control|unit\.core\.cpu-memory)$' --output-on-failure` — passed, 5/5.
- `cmake --preset asan && cmake --build --preset asan` — full all-target ASan/UBSan build passed.
- `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset asan --no-tests=error -R '^(unit\.core\.cpu-path|unit\.core\.softfloat-isolation|unit\.core\.cop1-state|unit\.core\.cpu-control|unit\.core\.cpu-memory)$' --output-on-failure` — passed, 5/5.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Incorrect test vector] Corrected BC1 target arithmetic before implementation.**
- **Found during:** Task 2 RED review
- **Issue:** The initial offset `+2` expectation used `PC+16`; MIPS branch target is `PC+4+(offset×4)`, or `PC+12` here.
- **Fix:** Corrected the test and independently derived expected target, placed the target instruction at the right address, recaptured the RED, and reran `gsd_run check tdd-red-evidence` successfully.
- **Files modified:** `core/tests/test_cop1.c`, `04-12-RED-evidence.md`
- **Commit:** `6b64b2f`

**2. [Rule 1 - Profile behavior] Kept COP1 branches inside the VR4300 profile and preserved exception priority.**
- **Found during:** Task 2 implementation review
- **Issue:** Initial branch classification would have expanded warm-v1 behavior, and an early nested-branch refusal could bypass CU1 Unusable handling for a COP1 branch in a delay slot.
- **Fix:** Gated COP1 branch recognition by the new profile and ran CU1/encoding checks before nested-branch refusal; added warm-v1 and delay-slot priority regressions.
- **Files modified:** `core/t64_cpu.c`, `core/tests/test_cpu_path.c`, `core/tests/test_cop1.c`
- **Commit:** `9d71c3e`

**Total deviations:** 2 auto-fixed issues. Both were resolved within the planned behavior and test scope.

## Issues Encountered

- The first implementation build used a nonexistent `conversion.flags` field; changed it to the existing `conversion.exception_flags` result before continuing.
- The prior FR=0 test expected an odd-register CVT path to be refused. The manual specifies paired even-register behavior, so the test now verifies the low/high words in the even/odd pair.

## Residual Qualification Notes

- This plan implements CVT.D.W and transfer/control/memory/branch state. The wider arithmetic, comparison, rounding, and exception matrix remains in Plan 13.
- The full ASan/UBSan build completed and the focused instrumented tests passed. Clang emitted a nonfatal Debug/ASan frame-size warning for `t64_cop1_execute` (4192 bytes against the configured 2048-byte warning threshold); other existing core functions also emitted frame-size warnings. The new function uses fixed-size locals and the exercised paths showed no sanitizer failure. Keep this diagnostic visible for later native closeout review.
- Plan 18 owns public native source verification. This plan did not run an external CPU diagnostic or make broader hardware-accuracy claims.
- The pinned NEC *VR4300/VR4305/VR4310 User's Manual*, 7th edition (August 2000), supports the FR/FPR behavior (pp. 208–210), FCR fields and FCR0 identity (pp. 211–216), and BC1 target/delay/likely behavior (p. 229, §7.5.5, Table 7-12). Public PDF: [VR43XX User's Manual](https://hack64.net/docs/VR43XX.pdf), SHA-256 `b7c1900f1707a4b238ebd82af1669e9628e4df4875ceabfa012a9d95b4716b75`.

## Next Phase Readiness

Plan 13 can build on the per-machine FCR/FPR state and the bounded SoftFloat seam. Parent CI and review are running against source tip `9d71c3e`; routine STATE/ROADMAP/requirements updates and the summary commit remain parent-owned.

## Self-Check: PASSED

- Summary file exists at the planned path.
- The five listed task commits are ancestors of source tip `9d71c3e`.
- Measured plan history is 5 commits from `47bbf88c99002bb0d6628c605fcddd4eec66ea99` through `9d71c3e5e58e34705bb4f2aaa3fcadf5e3dc87c0`.

---
*Phase: 04-complete-vr4300-cpu*
*Completed: 2026-11-12*
