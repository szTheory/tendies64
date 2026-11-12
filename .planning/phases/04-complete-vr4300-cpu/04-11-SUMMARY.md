---
phase: 04-complete-vr4300-cpu
plan: "11"
subsystem: cpu
tags: [softfloat, explicit-context, provenance, arithmetic, sanitizer]
requires:
  - phase: 04-10
    provides: Functional caches and precise memory execution status.
provides:
  - Pre-audited portable binary32/binary64 arithmetic closure with explicit context.
  - FR1 CVT.D.W tracer and independent integration evidence for all selected operations.
affects: [phase-04-arithmetic-matrix, phase-04-cop1, phase-04-qualification]
actuals:
  tasks: 2
  commits: 7
plan_head_before: 463b868d8e7f629766c186f7ea8574c536b7fa52
plan_head_after: 01a205d445767a560e9cdbcfdfd901801ad83a2f
tech-stack:
  added: [Pinned BSD-3-Clause SoftFloat generic source subset]
  patterns: [Explicit arithmetic context, per-file audited provenance, integer-only IEEE kernels]
key-files:
  created: [core/softfloat/sources.cmake, core/tests/test_softfloat.c, core/t64_softfloat.c, docs/evidence/softfloat-closure-audit.md]
  modified: [core/t64_cpu.c, core/t64_internal.h, core/tests/test_cpu_path.c, core/CMakeLists.txt, provenance/copies.json, scripts/check-test-inventory.sh, tools/lint/core-lint.sh, tools/lint/lint-objects.sh, docs/spec/cpu-profile.md]
key-decisions:
  - Audit the exact selected generic branch before copying; no target specialization or ambient arithmetic state.
  - Restrict the tracer to FR1 until Plan12 supplies paired register mapping.
  - Validate CPU opcode/Cause/full-PC setup and require a real partial-dispatch positive control.
requirements-completed: ["CPU-07", "CPU-09"]
coverage:
  - id: D1
    description: Actual CVT.D.W executes exact conversion with independent machine state and supported-mode boundaries.
    requirement: CPU-07
    verification:
      - kind: unit
        ref: core/tests/test_cpu_path.c#unit.core.cpu-path
        status: pass
    human_judgment: false
  - id: D2
    description: Every selected raw operation, context/flag behavior, provenance, build identity and lint closure has current evidence.
    requirement: CPU-09
    verification:
      - kind: unit
        ref: core/tests/test_softfloat.c#unit.core.softfloat32
        status: pass
      - kind: integration
        ref: Full107-test gate, source/object controls, build-ID mutation and four-target ASan
        status: pass
    human_judgment: false
duration: 89min
completed: 2026-11-12
status: complete
---

# Phase 4 Plan 11: Explicit-context Arithmetic Summary

`core/softfloat/sources.cmake` names the audited closure;
`core/tests/test_softfloat.c` checks all selected raw wrappers independently.
The CPU tracer implements only prepared FR1 CVT.D.W; later plans own the
remaining COP1 mapping, prechecks and architectural flag/trap commits.

## Source and License Qualification

- Official Berkeley SoftFloat commitf74b1e48110ac3a27dd49b787d164e55e42d81d1
  and its COPYING were verified before incorporation. The tracer's upstream
  i32_to_f64 SHA-256 isde334caa22edfcfc2a35572a2e858f0c500ad5018fc4335b46dfb003e0076183.
  The active BSD-3-Clause license byte-compares to pinned COPYING.
- The frozen50 generic C/five upstream-header denominator has unique per-file
  source/hash/notice/adaptation/test/owner records. Two authored headers supply
  portable platform and integer NaN/context glue. The tracer's copied section
  and old destination record were removed when the exact source file replaced it.
- The selected FAST_INT64 branch was preprocessed before copying; definition
  signatures were excluded from calls while address-taken targets were kept.
  No selected source was missing. Only authored NaN/common-NaN/context helpers
  remained. The [closure audit](../../../docs/evidence/softfloat-closure-audit.md)
  records the exact table and adaptations.
- All selected C/headers are built, identity-bound and source/object linted
  using production include/define semantics. No global/TLS/host-FP exception,
  runtime-helper waiver or mutable-state allowlist was added.

## Task Commits

1. **Task 1: audited conversion and mode boundary** - `d172d9d` (test/audit, original setup RED discarded), `79afd09` (test), `afccc69` (feat), `4c4be9b` (test), `c04448a` (fix).
2. **Task 2: raw dispatch and exact closure** - `88e9bd5` (test), `01a205d` (feat).

## Automated Checks

- Initial Task1 zero-extended KSEG PC caused an earlier fetch fault; it is
  excluded. Before production implementation, the corrected canonical test
  fetched468008a1, expectedc1e0000000000000, observed destination
  sentinel1122334455667788/status14/Cause0 and received RED_EVIDENCE_OK.
  GREEN fixed a packing adaptation that incorrectly ORed overlapping exponent
  and significand fields instead of using unsigned addition.
- A separate real FR0 boundary RED fetched the conversion and observed a
  destination write/retirement. The temporary FR1 guard now stops FR0 before
  either effect. Exact conversion, interleaved FPR/FCR31 state and disabled-CU1
  suppression pass in the canonical CPU path.
- Task2's placeholder-only draft is excluded. The valid pre-copy partial
  dispatcher first executed the existing conversion correctly, then f32 1+2
  returned NOT_READY/status7 with unchanged sentinels instead of40400000.
  Its actual report received RED_EVIDENCE_OK before kernel incorporation.
- Strict production C17 compile/link succeeded after justified width/sign
  casts, corrected f64 magnitude signatures and context/header visibility.
  All26 operations have positive/negative smoke cases; four rounding modes,
  five IEEE flag classes, before/after tininess, NaNs/invalid conversions and
  interleaved contexts pass across the three raw test targets.
- The initial exact-halfway tininess probe was a poor discriminator. The
  replacement is independently derived maxSubnormal×nextUp(1):
  minNormal−2^-172 for binary32 and−2^-1126 for binary64. It distinguishes
  before-rounding UF|IX from after-rounding IX.
- Provenance checks/selftests passed. Editing a selected helper changed the
  build ID, and restoring it restored the original ID. Source/object lint
  and their negative controls passed with no exceptions.
- The final common gate passed107/107 tests, fixture/inventory checks and
  lint82 files/zero violations. Inventory is107 gate/109 ASan, preserving
  all84 integer rows. Full all-target ASan build and CPU-path plus the three
  SoftFloat targets passed4/4 with the required sanitizer environment.
- Fresh unwaived PII tree/history checks pass. Hosted final-source verification
  is recorded separately in docs/ci-reviews/LOG.md.

## Deviations and Evidence Limits

- Raw flags accumulate in the caller context and the result reflects that
  accumulation. Authored raw NaN policy selects/quiets the first NaN; invalid
  integer conversion uses the minimum-integer sentinel. These raw-layer
  choices are distinct from later VR4300 prechecks/FCR31 commits.
- The temporary upstream path was re-located/reacquired with the official pin
  reverified; persistent ignored source receipts avoid repeating that work
  after continuation. No additional code was copied before the corrected audit.
- Necessary internal-header and lint-object-script edits were omitted from
  the original file list. Parent AUTONOMY guards now require opcode/Cause/full-PC
  context for CPU RED and avoid pushing documentation over unfinished child
  RED ancestry on a shared branch.
- ASan reports core large-stack and duplicate-library diagnostics; no SoftFloat
  frame warning appeared. Passing runtime checks do not establish general
  memory safety, exhaustive IEEE correctness or physical-console accuracy.
- No remaining COP1 opcode/mode or broad ROM compatibility is claimed.
  Plan17 expands the raw edge matrix before Plans12/13 consume the closure.
  Existing native/unclassified and judgment flags remain, without invented
  human validation or risk acceptance.

## Next Work

Execute Plan17 raw arithmetic boundary/category matrix, then Plan12. No owner
decision is needed for this authorized local work. Rights, physical tests and
release-channel stops remain binding.

## Self-Check: PASSED

