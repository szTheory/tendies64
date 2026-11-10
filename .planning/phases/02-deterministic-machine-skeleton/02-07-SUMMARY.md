---
phase: 02-deterministic-machine-skeleton
plan: "07"
subsystem: testing
tags: [asan, ubsan, rom-admission, fuzzing, canonical-json]
requires:
  - phase: 02-06
    provides: deterministic machine state, lifecycle, and digest APIs
provides:
  - Seeded ASan/UBSan production ROM admission campaign and sanitizer negative control
  - Strict t64.model-result v1 consumer separate from guest result protocols
  - Production validation for reported ROM storage alignment
affects: [02-08, result-protocol, rom-admission, validation]
actuals:
  tokens: 16834
  tasks: 2
  commits: 4
  plan_head_before: d98d10e57d93b515e74169c40a90c5a3143f3eb2
  plan_head_after: 74813fd8b0aba23b3816f7a91cbb742d13f4b36d
tech-stack:
  added: []
  patterns:
    - Public production admission exercised for every authored fuzz case
    - Strict bounded schema validation with canonical JSON re-emission
key-files:
  created:
    - core/tests/fuzz_rom_admission.c
    - core/tests/sanitizer_fault.c
    - scripts/expect-sanitizer.sh
    - runner/t64_model_result_check.c
    - runner/tests/test_model_result.c
    - .planning/phases/02-deterministic-machine-skeleton/02-07-task1-red-evidence.json
    - .planning/phases/02-deterministic-machine-skeleton/02-07-task2-red-evidence.json
  modified:
    - core/CMakeLists.txt
    - core/t64_rom.c
    - runner/CMakeLists.txt
    - docs/spec/result-protocol.md
key-decisions:
  - "Use full owned machine and normalized-content byte equality to verify refusal immutability after a measured preflight; retain direct public digest assertions at boundary controls."
  - "Keep native model observations in t64.model-result v1 and preserve the existing t64.result v1 and t64.suite v1 protocols."
  - "Validate build identity format in the consumer; the producer seam compares the emitted identity with the actual production build."
patterns-established:
  - "Every fuzz input reaches t64_machine_admit_rom; no test parser or semantic fork is used."
  - "A structurally valid model-mismatch remains visible as model-mismatch and is never projected into a guest outcome."
requirements-completed: [BUS-04, PROF-02, PROF-04]
coverage:
  - id: D1
    description: "A fixed-seed sanitizer campaign exercises production ROM admission and normalization across mutations and refusal cases."
    requirement: "BUS-04"
    verification:
      - kind: integration
        ref: "ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset asan -R '^fuzz\\.rom-admission$' --output-on-failure -V"
        status: pass
      - kind: integration
        ref: "sanitizer.negative-control"
        status: pass
    human_judgment: false
  - id: D2
    description: "Canonical native model observations are checked by a closed v1 schema separate from guest result records."
    requirement: "PROF-02"
    verification:
      - kind: unit
        ref: "unit.model-result"
        status: pass
      - kind: unit
        ref: "unit.result-goldens"
        status: pass
    human_judgment: false
duration: 37min
completed: 2026-11-10
status: complete
---

# Phase 2 Plan 07: Production Admission Evidence and Model Result Summary

**A 250,000-mutation ASan/UBSan campaign now exercises production ROM admission, and a separate strict checker validates native model observations.**

## Performance

- **Duration:** 37 minutes
- **Started:** 2026-11-10 15:34 -05:00
- **Completed:** 2026-11-10 16:11 -05:00
- **Tasks:** 2
- **Files modified:** 11

## Accomplishments

- Added the exact `0x7a64c0de` campaign: 250,000 mutations plus 148 fixed cases. It made 250,148 production admission calls (316 accepted, 249,832 refused) in 30.88 seconds; all three byte orders and 60 header fields were reached. No ASan/UBSan finding occurred; this is not a memory-safety proof.
- Added a sanitizer negative control requiring a sanitizer report and nonzero child exit; it rejects missing-report controls.
- Added a distinct strict `t64.model-result` v1 checker with 37 production-invocation tests, including canonical match/mismatch records and refusals for malformed and legacy guest records.
- Fixed production ROM admission to verify the candidate address satisfies its reported alignment. Existing t64.result v1 and t64.suite v1 behavior remains unchanged.

## Task Commits

1. **Task 1 RED:** `1d405c9` — sanitizer campaign and negative-control tests.
2. **Task 1 GREEN:** `5c8ebc9` — reject misaligned ROM candidate storage.
3. **Task 2 RED:** `6058815` — model-result contract tests.
4. **Task 2 GREEN:** `74813fd` — implement strict native model-result validation.

The plan summary is committed separately from the four task commits.

## Verification

- Preflight: 10,000 mixed cases in 1.26 seconds. Full ASan/UBSan campaign: 250,148 calls in 30.88 seconds under the 120-second timeout.
- `sanitizer.negative-control`, `unit.model-result` (37 cases), and `unit.result-goldens` passed.
- `./scripts/gate.sh` passed all 71 CTests, configure/build, shell syntax, lint self-test, and production core lint in 21 seconds.
- TDD RED evidence is preserved in the two plan-local JSON records; both failed on the intended assertions and were machine-validated.

## Decisions Made

- Compare all owned state and normalized image bytes on refusals; keep public digest assertions at boundary controls.
- Keep model observations separate; a valid mismatch is not a guest failure or pass. Plan 02-08 compares the emitted build id with the actual production identity.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Reject storage whose reported alignment is false**

- **Found:** Task 1 RED showed the allocator's reported misalignment was accepted and changed state (`BAD_STORAGE` expected, `OK` observed).
- **Fix:** Validate the returned address against reported alignment in `core/t64_rom.c`.
- **Verification/commit:** Full campaign passed; `5c8ebc9`.

**2. [Rule 3 - Build blocker] Preserve ASan frame-size warnings without failing the instrumented target**

- **Found:** ASan instrumentation enlarged existing core stack frames beyond the threshold and `-Werror` stopped the sanitizer build.
- **Fix:** ASan builds keep the frame warning visible but nonfatal; normal build policy is unchanged.
- **Verification/commit:** ASan build and campaign passed; `1d405c9`.

### Measured Harness Adjustment

The refusal oracle compares all owned machine/RAM and normalized content bytes instead of rehashing several MiB per case. The 10,000-case preflight took 1.26 seconds; direct public digest checks remain at boundary controls, and unchanged bytes preserve all canonical serializer inputs.

**Total deviations:** 2 auto-fixed issues and 1 measured harness adjustment.
**Impact on plan:** The production admission path and required sanitizer coverage are preserved; no dependency, fixture asset, cache, or reduced campaign was added.

## Issues Encountered

- Host ASan does not support `detect_leaks=1`; the run used `detect_leaks=0` with ASan and UBSan active. The next producer plan performs exact build identity comparison.

## User Setup Required

None.

## Next Phase Readiness

Plan 02-08 can consume the strict separate model-result contract and compare its build identity against the production code exercised. The campaign establishes bounded parser evidence only, not broad compatibility, hardware accuracy, or memory safety.

---
*Phase: 02-deterministic-machine-skeleton*
*Completed: 2026-11-10*

## Self-Check: PASSED

The summary, campaign, and checker files exist; all four task commits are ancestors of HEAD; the measured task commit count is four; and the plan-scoped file scan found no untracked stub patterns.
