---
phase: 03-first-headless-rom-through-open-ipl3
plan: "10"
subsystem: runner
tags: [C, CTest, JSON, result-protocol, guest-evidence]
requires:
  - phase: 03-first-headless-rom-through-open-ipl3
    provides: verified fixture and input descriptor preparation from Plan 03-09
provides:
  - strict closed t64.result v2 validation for real guest-execution reports
  - independent behavioral cases for v2, v1 compatibility, and model-result separation
affects: [03-11, result-protocol, runner, evidence]
actuals:
  tokens: 15275
  tasks: 1
  commits: 2
commits: 2
plan_head_before: a69d215a06968d1552494222a8c9536c304ee537
plan_head_after: 742ec02c840ca109ff9251f89730b90e3b9fdddc
tech-stack:
  added: []
  patterns:
    - version-specific validation over the existing strict parser and canonical emitter
    - outcome classification bound to guest stops, milestones, retirement and transcript identity
key-files:
  created:
    - runner/tests/test_result_v2.c
    - .planning/phases/03-first-headless-rom-through-open-ipl3/03-10-result-v2-red.json
  modified:
    - docs/spec/result-protocol.md
    - runner/t64_result_check.c
    - runner/CMakeLists.txt
    - scripts/check-test-inventory.sh
key-decisions:
  - "Keep v1 results and native model observations isolated from real guest v2 reports."
  - "Treat a structurally valid unit-authored v2 report as contract evidence, not proof of guest execution."
  - "Leave RUN-01, RUN-03 and BOOT-03 pending until phase-level guest evidence exists."
patterns-established:
  - "Real v2 pass requires exact fixture and transcript identities, all ordered milestones and nonzero instruction retirement."
  - "Empty output at completion or bound remains boot-fail; host/report errors retain harness-error precedence."
requirements-completed: []
coverage:
  - id: D1
    description: Strict t64.result v2 checker with v1 and model-result isolation.
    verification:
      - kind: unit
        ref: runner/tests/test_result_v2.c#canonical-real-v2-and-mutations
        status: pass
    human_judgment: false
duration: 41min
completed: 2026-11-11
status: complete
---

# Phase 3 Plan 10: Strict Real Result V2 Contract Summary

The independent behavioral suite in `runner/tests/test_result_v2.c` has a portable pre-feature failure record in `.planning/phases/03-first-headless-rom-through-open-ipl3/03-10-result-v2-red.json`.

## Performance

- **Duration:** 41 min
- **Started:** 2026-11-11T13:46:24Z
- **Completed:** 2026-11-11T14:27:08Z
- **Tasks:** 1
- **Plan-owned files:** 7, including portable RED evidence and this summary

## Accomplishments

- Added a strict `t64.result` v2 reader with exact canonical keys, closed identity shapes, pinned profile/IPL3/fixture data, stop/detail/outcome relationships, milestone order and CPU-retirement requirements. A pass also requires the exact 30-byte expected transcript identity.
- Kept v1 parsing, golden records, suite validation and `t64.model-result` separate. The expanded v2 suite checks valid pass, raw malformed JSON, key/version/type/identity failures, empty and incomplete output, unsupported execution, overflow, and prelaunch null identities.
- Documented the v2 field set, canonical form, identity meanings, outcome precedence and the evidence limit: unit-authored JSON validates the contract but does not prove a guest ran.
- Registered `runner.result-v2` in both preset inventories.

## Task Commits

1. Task 1 RED — `7fa9dec` (`test(03-10): add failing test for real result v2`)
2. Task 1 implementation — `742ec02` (`feat(03-10): validate real result v2`)

## Files Created/Modified

- Created `runner/tests/test_result_v2.c` with an independently authored v2 pass and mutation cases; created the portable RED evidence JSON.
- Updated `runner/t64_result_check.c`, `runner/CMakeLists.txt`, and `scripts/check-test-inventory.sh` for dual-version parsing and test registration.
- Updated `docs/spec/result-protocol.md` with the real v2 contract and its limits.

## Decisions Made

- V2 pins the first real profile and source/payload identities while leaving `fixture_sha256` as the normalized authored whole-ROM hash, independent from the upstream IPL3 asset bytes.
- V1's synthetic behavior and `t64.model-result` remain separate; broad guest-run requirements stay pending until real phase evidence is available.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Preserved the legacy version diagnostic during v2 dispatch.**

- **Found during:** Task 1 focused regression checks
- **Issue:** A legacy synthetic v1 seed with `schema_version: 2` was initially routed into the v2 field validator, which returned `R-UNKNOWN-FIELD` instead of the established `R-VERSION`.
- **Fix:** Dispatch recognizes the exact locked synthetic v1 stamp as legacy before key validation; v2-shaped records with unknown fields still use v2 validation and reject under the v2 key contract.
- **Files modified:** `runner/t64_result_check.c`
- **Verification:** Focused v1/v2/model tests passed, followed by the full 82-test gate.
- **Committed in:** `742ec02`

**Total deviations:** 1 auto-fixed bug
**Impact on plan:** Preserved existing v1 diagnostics without loosening v2 validation.

## Automated Checks

- TDD RED: the independently authored canonical v2 pass compiled and ran against the pre-feature checker, which exited 1 with `R-VERSION`; portable evidence is recorded in the RED JSON file above.
- Focused gate CTest: v2, v1 goldens, v1 result seeds, suite seeds and model-result passed, 5/5.
- `cmake --preset gate` and `cmake --build --preset gate` passed. The linker emitted an ignored duplicate `t64_common` library warning.
- `ctest --preset gate --no-tests=error --output-on-failure` passed, 82/82.
- Focused ASan `runner.result-v2` passed, 1/1. Gate inventory passed with 82 tests; ASan inventory passed with 84.
- `repo.docs` and `repo.docs-selftest` passed, 2/2; `git diff --check` passed.

## Issues Encountered

The first full-gate CTest invocation yielded before its terminal status was captured. I ran one bounded, explicitly captured invocation and confirmed 82/82 passing; no result is claimed from the earlier yield.

## Requirements and Limits

No broad RUN or BOOT requirement is completed by this plan. The test report is independently authored structural input and does not represent actual guest execution. The actual producer, CPU run and phase-level boot evidence remain with the later Phase 3 plans; no runtime `--real` support is claimed here.

## Next Phase Readiness

Plan 03-11 can emit the closed v2 shape once its real runner has collected the pinned input, output, state and CPU milestone evidence. It must still prove actual guest execution independently.

## User Setup Required

None.

## Self-Check: PASSED

- Created files exist, including the portable RED evidence and this summary.
- RED and implementation commits are ancestors of HEAD.
