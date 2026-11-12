---
phase: 04-complete-vr4300-cpu
plan: "04"
subsystem: cpu
tags: [cp0, privilege, field-masks, sanitizer]
requires:
  - phase: 04-03
    provides: Reviewed integer/control execution and precise exception integration.
provides:
  - Central staged CP0 transfer, privilege, width and field-owner contracts.
  - Independent register/bit/side-effect matrix and digest-visible Config window.
affects: [phase-04-timer, phase-04-mmu, phase-04-cache]
actuals:
  tasks: 2
  commits: 5
plan_head_before: b578ba4b5ab61f5fb29bd971967ce4d512e0008e
plan_head_after: 18feb8a
tech-stack:
  added: []
  patterns: [Central staged register effects, independent field-level expected values]
key-files:
  created: [core/t64_cp0.c, core/tests/test_cpu_cp0.c, docs/spec/cpu-cp0.md]
  modified: [core/t64_cpu.c, core/t64_internal.h, core/t64_digest.c, core/tests/test_cpu_path.c, core/tests/test_boot_entry.c, core/CMakeLists.txt, scripts/check-test-inventory.sh, docs/spec/cpu-profile.md]
key-decisions:
  - Use defined CP0 transfer widths and preserve hardware-owned fields; manual-undefined widths remain qualified.
  - Prepared Config initialization window starts closed and is serialized under v9 only.
requirements-completed: ["CPU-03", "CPU-04", "CPU-09"]
coverage:
  - id: D1
    description: Real CP0 instructions enforce privilege, transfer widths and corrected Status field policy.
    requirement: CPU-03
    verification:
      - kind: unit
        ref: core/tests/test_cpu_path.c#unit.core.cpu-path
        status: pass
    human_judgment: false
  - id: D2
    description: CP0 fields, side effects, refusal atomicity and the digest-visible window have independent native controls.
    requirement: CPU-04
    verification:
      - kind: unit
        ref: unit.core.cpu-cp0; unit.core.boot-entry; unit.core.digest-trace
        status: pass
      - kind: integration
        ref: Full ASan build with affected selections; gate 96 unaffected cases plus corrected boot-entry check
        status: pass
    human_judgment: false
duration: 50min
completed: 2026-11-12
status: complete
---

# Phase 4 Plan 4: CP0 Register Contracts Summary

The staged effects in `core/t64_cp0.c` and independent matrix in
`core/tests/test_cpu_cp0.c` centralize CP0 semantics while preserving the
legacy warm profile's separate rules.

## Performance

Two tasks; eleven implementation/specification/registration files and two
portable RED records. The observed first-RED-to-final-implementation commit
span was 50 minutes (2026-11-12T03:56:47Z to 2026-11-12T04:46:53Z), excluding
pre-commit preparation and parent closeout. This is elapsed workflow time.

## Accomplishments

- Protected guest CP0 transfers enforce mode/CU0, defined widths, Status field
  ownership and atomic Config refusals before architectural writes. Config's
  initialization-window state is per-machine and digest-visible under v9.
- Field-level controls cover Status set/clear bits, all seven PageMask values,
  CP0 register reads/writes, transfer/selector restrictions, Cause and Compare
  preservation, Wired/Random effects and refusal atomicity. Random's low-five
  selection steps on retirement; Count timing remains explicitly deferred.

## Task Commits

1. **Task 1: protected transfer tracer** - `6b76020` (test), `1cf8d4d` (feat), `1100371` (fix).
2. **Task 2: register matrix** - `3010473` (test), `18feb8a` (feat).

## Automated Checks

- Task1's registered CPU-path test ran 13 assertions and reached MTC0 Status,
  stopping unsupported before retirement or mutation (Status34200000,
  Cause/EPC zero). Actual unchanged JUnit received RED_EVIDENCE_OK.
- Task2 executed MTC0 Index, expected8000003f from initial80000000 and input
  ffffffff, observed unsupported with Index unchanged. The adjacent path test
  passed. Actual unchanged JUnit received RED_EVIDENCE_OK.
- Focused gate path/exception and CP0 selections pass. Exact inventory contains
  97 gate/99 ASan names; inventory selftest and the earlier 84-row integer
  coverage/four mutation controls remain passing.
- Full all-target ASan builds succeeded. The affected CP0/path/cache/exception/
  digest selection passed 5/5. After the test-reference correction below,
  another all-target instrumented build and the touched boot-entry test pass.
  The options remain ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 and
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1. No full 99-test sanitizer
  rerun is claimed.
- The sole common gate passed 96/97; only the independent prepared-v9 reference
  failed. After the bounded reference/specification fix, unit.core.boot-entry
  passes normally and instrumented. No second local full-gate result is claimed.
  Parent's fresh unwaived gov.pii.repo also passes before publication.

## Deviations and Issues

- The initial tracer used DMFC0 on a 32-bit register, which the manual leaves
  undefined (p.419). The corrected test reads seeded 64-bit EPC0123456789abcdef;
  focused normal/instrumented checks then pass. An edit-time printf argument
  mismatch was fixed before GREEN and was not behavioral evidence.
- ASan showed a new3248-byte prepare_cp0_write frame. Splitting TLB, exception
  and system register handlers removed the CP0-specific warning; refreshed
  builds/tests passed. Existing unrelated frame/link warnings remain visible.
- The gate failure was initially suspected to involve Random stepping; inspection
  identified the actual cause: the independent v9 reference omitted the newly
  serialized cp0_config_init_window. The reference, sensitivity check and seed
  golden now include it. The new independently derived prepared digest is
  dfd84ba75420a9cf7f0883c18b78fbf8999cf13f41dd79d28879a5d724b6720e.
  Warm-v1/v8 and all authored fixtures remain unchanged.
- Updating core/tests/test_boot_entry.c and docs/spec/cpu-profile.md is the
  minimal omitted-file adjustment required by the planned new v9 state field.
  No broader API or behavior expansion was introduced.
- Parent cross-checked Random's unusual writable bit5 against NEC p.147:
  the manual explicitly permits it while ignoring it for TLB selection. The
  general read-only description is qualified by that explicit exception.

## Evidence Limits and Readiness

The masks, revision/RP choices and prepared Config-window policy are documented
with their evidence and version triggers. This is native model evidence, not
silicon qualification. Guest-time Count/Compare ordering belongs to Plan05;
complete TLB/cache operations remain later slices. Shared requirement IDs stay
pending their unfinished sibling summaries. Fresh full hosted verification is
tracked separately for source18feb8a. Ready for Plan04-05.

## Self-Check: PASSED

All five task commits, both portable RED records, the CP0 specification and
exact test registration exist. All local checks are accounted for with their
actual scope. Owner instruction/configuration and parent CI-log edits remain
unstaged; no new waiver or protected input was introduced.
