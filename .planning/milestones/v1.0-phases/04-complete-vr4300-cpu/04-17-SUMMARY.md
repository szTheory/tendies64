---
phase: 04-complete-vr4300-cpu
plan: "17"
subsystem: cpu
tags: [arithmetic, independent-vectors, rounding, sanitizer]
requires:
  - phase: 04-11
    provides: Audited explicit-context arithmetic closure and immediate smoke tests.
provides:
  - Independent raw arithmetic category/rounding matrix before COP1 consumers.
  - Enforced family, operation, expected-source and flag coverage.
affects: [phase-04-cop1, phase-04-qualification]
actuals:
  tasks: 2
  commits: 2
plan_head_before: ca585e2f27149fdb67e258643a8c0369d5a32baf
plan_head_after: 2e2a02d31e59334f74b7fb009efa974a29593875
tech-stack:
  added: []
  patterns: [Independent bit/rational intervals, fail-closed coverage inventory]
key-files:
  created: []
  modified: [core/tests/test_softfloat.c, docs/evidence/softfloat-closure-audit.md]
requirements-completed: ["CPU-07", "CPU-09"]
coverage:
  - id: D1
    description: Signed conversion boundaries execute across all four rounding modes with interleaved context isolation.
    requirement: CPU-07
    verification:
      - kind: unit
        ref: core/tests/test_softfloat.c#unit.core.softfloat-isolation
        status: pass
    human_judgment: false
  - id: D2
    description: All raw operations, edge categories, formats and rounding-sensitive arithmetic have independently expected rows.
    requirement: CPU-09
    verification:
      - kind: unit
        ref: core/tests/test_softfloat.c#unit.core.softfloat32
        status: pass
      - kind: integration
        ref: Four-target gate and ASan selections; source/object lint
        status: pass
    human_judgment: false
duration: 60min
completed: 2026-11-12
status: complete
---

# Phase 4 Plan 17: Independent Raw Arithmetic Matrix Summary

`core/tests/test_softfloat.c` exercises production wrappers;
`docs/evidence/softfloat-closure-audit.md` records per-family derivations,
per-mode bits/flags and the complete expected coverage inventory.

## Results and Evidence

- Task1 covers two signed upper-bound conversions under RM0–RM3 and an exact
  interleaved operation with independent context state.
- Task2 has126 unique families ×4 modes=504 vectors, repeated in both
  existing raw-format selections. It covers all26 operations, both formats,
  halfway/neighbor cases, signed zeros, subnormal/normal endpoints,
  infinities/qNaNs/sNaNs, signed W/L bounds and outside neighbors, signed
  overflow/underflow, inexact/divide-zero/invalid and context isolation.
- Expected values use independent bit decomposition, rational bounds and
  integer-square/midpoint intervals. No expected value comes from SoftFloat,
  host floating point or another emulator. Non-exact add/sub/mul/div/sqrt
  rows exercise rounding in each format/mode beyond the exact base cases.
- The executing inventory checks literal126 family count, unique/nonempty IDs,
  valid format/op/source class, all26 base operation×RM cells, every required
  category and all3 derivation classes per format/mode, five flags per format,
  and rounding-sensitive arithmetic cells. No separate omission-mutation
  execution is claimed.
- Grounded gate configure/build and raw32/raw64/isolation/CPU-path selection
  passed4/4. Source/object lint passed82 files/zero violations. Full all-target
  ASan/UBSan configure/build and the same affected selection passed4/4.
  Inventories remain107 gate/109 ASan with no new registration.
- The unchanged production arithmetic repair479a147 separately passed both
  full hosted107-test lanes. No successful full local107-test rerun is claimed
  for this test-only expansion; its four owning targets were freshly executed.
  Final matrix source hosted verification is tracked in the CI log.

## Task Commits

1. **Task 1: signed conversion boundary** - `58fbc5a` (test).
2. **Task 2: complete raw edge matrix** - `2e2a02d` (test).

## Corrections and Limits

- The binary32 predecessor5effffff represents2^63−2^39; its signed64 expected
  result was corrected to7fffff8000000000 from exact decomposition.
- A guessed sqrt2 table and a misread mismatch list briefly suggested a
  production contradiction. Integer-square/midpoint derivation established
  binary64 RM0/1/2/3 low words bcd/bcc/bcc/bcd, agreeing with production.
  These are corrected expectation attempts, not demonstrated implementation
  defects. No wrapper or audited helper changed for this matrix.
- The dispatch-to-final window is60 minutes, including the separately owned
  Plan11 Linux-width repair479a147. That fix is recorded in11-SUMMARY and is
  excluded from this plan's two task commits.
- Existing unrelated core large-stack and duplicate-library diagnostics remain;
  no SoftFloat warning appeared. No new dependency or copied-source identity
  was introduced. Hardware-specific FS/E/FCR31/destination behavior remains
  with Plans12/13; raw IEEE success does not qualify physical-console behavior.

## Next Work

Execute Plan12 COP1 register/control/memory/branch ownership. No owner decision
is needed; existing rights, physical and release stops remain.

## Self-Check: PASSED

