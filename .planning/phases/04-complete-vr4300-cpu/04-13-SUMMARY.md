---
phase: 04-complete-vr4300-cpu
plan: "13"
subsystem: cpu
tags: [COP1, VR4300, SoftFloat, FCR31, rounding, TDD]

# Dependency graph
requires:
  - phase: 04-12
    provides: per-machine FPR/FCR state, FR pairing, exception and branch seam
  - phase: 04-17
    provides: audited explicit-context SoftFloat arithmetic closure
provides:
  - COP1 arithmetic, comparisons, conversions and VR4300 exception translation
  - Independent COP1 edge vectors and exact gate inventory entries
  - Documented FCR31, FS, NaN and deterministic model policies
affects: [04-18 native CPU verification, 04-16 private qualification, VR4300 CPU]

# Actuals: changed-diff characters divided by four, measured from the plan base.
actuals:
  tokens: 27340
  tasks: 2
  commits: 4
plan_head_before: 8101be81ce3baf887e15b477ad0adb3b956b907c
plan_head_after: cc629a4f601a9e981536fe25dd5f5eb32c8e242f

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Translate FCR31 rounding and causes at the COP1 boundary around explicit-context arithmetic.
    - Precheck VR4300 exceptional inputs and stage FPR/condition writes until exception handling completes.

key-files:
  created:
    - docs/spec/cpu-cop1.md
    - .planning/phases/04-complete-vr4300-cpu/04-13-RED-evidence.md
  modified:
    - core/t64_cop1.c
    - core/t64_cpu.c
    - core/t64_internal.h
    - core/tests/test_cpu_path.c
    - core/tests/test_cop1.c
    - core/CMakeLists.txt
    - scripts/check-test-inventory.sh

key-decisions:
  - "Map FCR31 RP/RM to SoftFloat maximum/minimum rounding; SoftFloat's directed mode numbers are reversed from the VR4300 selectors."
  - "Treat unsupported W/L source operation encodings as E and preserve the destination."
  - "Use positive canonical quiet NaN payloads and FCR0 0x00000b00 as documented deterministic model policy; neither is a measured revision claim."

requirements-completed: [CPU-07, CPU-09]

coverage:
  - id: D1
    description: Arithmetic, comparisons, conversions, FCR31 exceptions and destination suppression follow the documented VR4300 model.
    requirement: CPU-07
    verification:
      - kind: unit
        ref: unit.core.cop1-arithmetic; unit.core.cop1-exceptions; unit.core.cop1-state; unit.core.cpu-path
        status: pass
      - kind: integration
        ref: sh scripts/gate.sh (110-test full local gate)
        status: pass
    human_judgment: false
  - id: D2
    description: Expected bits, flags and exception outcomes come from manual rules and independent bit/rational derivations.
    requirement: CPU-09
    verification:
      - kind: unit
        ref: unit.core.cop1-arithmetic; unit.core.cop1-exceptions; docs/spec/cpu-cop1.md
        status: pass
    human_judgment: false

# Metrics
duration: 58min
completed: 2026-11-12
status: complete
---

# Phase 4 Plan 13: COP1 Arithmetic and Exception Summary

**VR4300 COP1 now executes arithmetic, comparisons and conversions with per-machine FCR31 translation, complete directed rounding and independent edge vectors.**

## Performance

- **Duration:** approximately 58 minutes from dispatch through final verification.
- **Started:** approximately 2026-11-12T16:03:00Z.
- **Completed:** 2026-11-12T17:01:41Z.
- **Tasks:** 2.
- **Files modified:** 9 tracked plan files.

## Accomplishments

- Implemented the defined S/D arithmetic, comparison and conversion matrix through the canonical CPU path. COP1 now maps VR4300 RM selectors, maintains last-operation Causes and sticky masked Flags, and suppresses trapped destination and condition writes.
- Added executable, independently expected vectors across both precisions and all rounding modes. Coverage includes all 16 compare predicates, signed W/L limits, signed zero, infinities, NaNs, denormal input/result rules, FS flush directions, enabled exceptions, FR=0 pairs, FR=1 lanes, compare-driven BC1 and interleaved machines.
- Added [cpu-cop1.md](../../../docs/spec/cpu-cop1.md) with FCR31 masks, opcode coverage, FS/tininess policy, primary manual provenance, and explicit model limitations.
- Preserved warm-v1 behavior and the existing per-machine ownership boundary. No external ROM or private diagnostic corpus entered the tests or repository.

## Task Commits

1. **Task 1: Execute a rounded operation with a trapped destination and guest recovery** — `ced3ea5` (RED), `7279750` (implementation); [test_cpu_path.c](../../../core/tests/test_cpu_path.c#L601), [t64_cop1.c](../../../core/t64_cop1.c#L214).
2. **Task 2: Complete every arithmetic, comparison and conversion edge** — `900e8ec` (RED), `cc629a4` (implementation); [test_cop1.c](../../../core/tests/test_cop1.c#L697), [cpu-cop1.md](../../../docs/spec/cpu-cop1.md#L1).

## TDD Gate Compliance

- Task 1's `unit.core.cpu-path` RED fetched `ADD.S` opcode `0x460208c0` at full PC `0xffffffff80001000` and observed RI instead of the expected FPE/inexact result. The unchanged JUnit evidence was classifier-confirmed as `RED_EVIDENCE_OK`; the focused tracer and recovery checks pass after implementation.
- Task 2's `unit.core.cop1-arithmetic` RED exposed reversed directed RM=2/RM=3 results for S and D exact ties. `unit.core.cop1-exceptions` exposed an invalid `CVT.W.W` that retired and changed f3 instead of raising E. Both unchanged JUnit reports were classifier-confirmed as `RED_EVIDENCE_OK` before implementation.
- Sanitized evidence is in [04-13-RED-evidence.md](./04-13-RED-evidence.md). Machine reports remain in the ignored local diagnostic directory.

## Automated Checks

- Focused gate configure/build and six-test selection passed: `unit.core.cop1-arithmetic`, `unit.core.cop1-exceptions`, `unit.core.cop1-state`, `unit.core.softfloat32`, `unit.core.softfloat64`, and `unit.core.softfloat-isolation` (6/6).
- Full all-target ASan configure/build completed; the same six instrumented tests passed with the inherited ASan/UBSan options (6/6).
- `sh scripts/check-test-inventory.sh --selftest` passed, including removal controls; exact inventories passed at 110 gate names and 112 ASan names.
- `tools/lint/core-lint.sh` (invoked with sh) passed with 83 production files and zero violations.
- One final `scripts/gate.sh` (invoked with sh) passed: all 110 CTests, fixture and boot-evidence checks, inventory checks, and core lint completed successfully.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Corrected the directed rounding mode translation.**
- **Found during:** Task 2 RED matrix.
- **Issue:** FCR31 RP/RM were passed directly to SoftFloat, where mode values 2 and 3 mean minimum and maximum respectively.
- **Fix:** Translate FCR31 RP to SoftFloat maximum and RM to minimum; apply the same corrected mapping to ROUND/TRUNC/CEIL/FLOOR conversion selectors.
- **Files modified:** `core/t64_cop1.c`, `core/tests/test_cop1.c`.
- **Verification:** Binary32/binary64 exact halfway vectors and conversion/overflow/FS matrices pass in gate and ASan.
- **Committed in:** `cc629a4`.

**2. [Rule 1 - Bug] Reject invalid FP-to-integer operations encoded with W/L source format.**
- **Found during:** Task 2 RED matrix.
- **Issue:** The decoder interpreted W-format `CVT.W.W` bits as a double source, retired the instruction, and changed the destination.
- **Fix:** Permit W/L formats only for CVT.S/D.W/L; other W/L operation encodings raise E and preserve FPR state.
- **Files modified:** `core/t64_cop1.c`, `core/tests/test_cop1.c`.
- **Verification:** The exact invalid opcode raises FPE with E-only Cause, preserves sticky Flags and leaves the destination unchanged.
- **Committed in:** `cc629a4`.

**Total deviations:** 2 in-scope correctness fixes; no scope or architecture changes.

## Issues Encountered

- Clang's ASan Debug build reports a nonfatal 3360-byte stack-frame warning for `t64_cop1_execute` against the existing 2048-byte warning threshold. Plan 12 had recorded 4192 bytes before this plan. The full ASan build and focused tests passed; warning flags and policy were not changed.
- The manual documents VR4300 NaN polarity opposite to the generic SoftFloat wrapper. COP1 performs the VR-specific precheck and documents its deterministic positive canonical quiet payload. This policy is software modeling detail, not measured silicon behavior.

## Residual Qualification Notes

The pinned NEC *VR4300/VR4305/VR4310 User's Manual*, seventh edition (August 2000), supports the listed behavior. Public PDF: [VR43XX User's Manual](https://hack64.net/docs/VR43XX.pdf), SHA-256 `b7c1900f1707a4b238ebd82af1669e9628e4df4875ceabfa012a9d95b4716b75`. Expected outputs are independent software-model vectors; this plan did not measure a VR4300 or use the private diagnostic. The prohibition against treating generic arithmetic agreement or native tests as hardware qualification remains in force; Plan 18 owns public source verification and later qualification plans own their scoped evidence.

## User Setup Required

None.

## Next Phase Readiness

Execute Plan 15 next: the separate authored diagnostic adapter and CPU-profile flat loader. Plan 18 later closes public native verification before Plan 16 private qualification. Hardware-level accuracy and private diagnostic completion remain unclaimed.

---
*Phase: 04-complete-vr4300-cpu*
*Completed: 2026-11-12*

## Self-Check: PASSED

- Summary and COP1 specification exist at their planned paths.
- All four plan task commits are ancestors of source tip `cc629a4`.
- No implementation stub or placeholder patterns were found in the new COP1 source, tests, or specification.
