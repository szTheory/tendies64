---
phase: 04-complete-vr4300-cpu
plan: "06"
subsystem: cpu
tags: [mmu, tlb, privilege, precise-faults, sanitizer]
requires:
  - phase: 04-05
    provides: Guest-time clock synchronization and interrupt ordering.
provides:
  - Shared full-width MMU translation and precise refill/retry execution.
  - Complete native segment, paired-page, permission, management and shutdown matrix.
affects: [phase-04-memory, phase-04-cache, phase-04-qualification]
actuals:
  tasks: 2
  commits: 3
plan_head_before: af6e225105dec567f6ce59542adda55811c88275
plan_head_after: 565dba4ff26d441078670d3d87b0507b82918710
tech-stack:
  added: []
  patterns: [Pure attribute-bearing translation, precise committed TLB context]
key-files:
  created: [core/t64_mmu.c, core/tests/test_cpu_mmu.c]
  modified: [core/t64_cpu.c, core/t64_cp0.c, core/t64_internal.h, core/t64_exception.c, core/tests/test_cpu_path.c, core/tests/test_cpu_exceptions.c, core/CMakeLists.txt, scripts/check-test-inventory.sh, docs/spec/cpu-profile.md, docs/spec/cpu-cp0.md]
key-decisions:
  - Invalid entries participate in matching; preserve the zero seed and explicitly qualify its duplicate-tag shutdown consequence.
  - Preserve the missed original formal RED guard as a process deviation; later current controls do not replace that history.
requirements-completed: ["CPU-03", "CPU-04", "CPU-09"]
process-deviations:
  - Original RED observations lack retained formal classifier reports.
  - Tests and implementation were committed together.
coverage:
  - id: D1
    description: Actual guest refill handler installs a mapping and retries the load without publishing failed-access effects.
    requirement: CPU-03
    verification:
      - kind: unit
        ref: core/tests/test_cpu_path.c#unit.core.cpu-path
        status: pass
      - kind: integration
        ref: Current isolated wrong-refill-vector control; RED_EVIDENCE_OK then production GREEN
        status: pass
    human_judgment: false
  - id: D2
    description: Page/mode/permission/management/context and duplicate-shutdown boundaries have independently expected native cases.
    requirement: CPU-04
    verification:
      - kind: unit
        ref: core/tests/test_cpu_mmu.c#unit.core.cpu-mmu
        status: pass
      - kind: integration
        ref: Current isolated XKPHYS-domain control; repaired99-test stage; lint; full ASan build and10 affected tests
        status: pass
    human_judgment: false
duration: 67min
completed: 2026-11-12
status: complete
---

# Phase 4 Plan 6: Paired TLB and Virtual Memory Summary

The pure translation contract in `core/t64_mmu.c` and native matrix in
`core/tests/test_cpu_mmu.c` connect full-width CPU accesses to precise TLB
faults, committed context, guest repair and retry.

## Performance

Two tasks. The baseline-to-final-feature-commit window is67 minutes
(2026-11-12T05:34:01Z to06:41:07Z), including dispatch and verification;
the two feature commits themselves span32 minutes. Parent documentation and
formal-control remediation follow that window. These are timestamped workflow
windows, not coding-effort or emulator-throughput measurements.

## Accomplishments

- Fetch/data share full-width mode, alignment, segment, permission, physical
  domain and cache-attribute validation before bus access. A real guest TLB
  miss enters refill, installs a paired mapping with TLBWI, ERETs and retries
  successfully while preserving failed-load destination/memory/cache/link state.
- The matrix covers32 entries, all7 page sizes and halves/endpoints, 32-bit and
  extended regions, ASID/both-G/V/D, C attributes, refill/invalid/modified/XTLB,
  nested context, TLBP/R/WI/WR with pre-retirement Random/Wired and duplicate
  shutdown/reset. XContext region/BadVPN2 update in both address modes.

## Task Commits

1. **Task 1: refill and retry** - `244d019` (feat, combined source/test/evidence).
2. **Task 2: matrix and qualified evidence** - `2c547b0` (feat, combined source/test/evidence), `565dba4` (docs).

## Automated Checks

- Original Task1 semantic RED observed mapped LW ExcCode2, BadVAddr4000,
  EntryHi0, unchanged destinationfeedface/retired0, but common vector80000180
  instead of refill80000000. Original Task2 semantic RED observed XKPHYS width
  refusal returning address fault without translation but domainBUS rather
  than XKPHYS. Their formal pre-implementation classifier reports were not
  performed/retained; no original RED_EVIDENCE_OK claim is made.
- Parent's separate current regression controls reintroduced only the wrong
  normal-refill offset and the wrong refusal-domain assignment in isolated
  archive copies. Actual unchanged JUnit reports for CPU-path case11 and MMU
  case3 received RED_EVIDENCE_OK / target_test_failed. Production sources,
  archive and executables remained unchanged, and production tests then passed
  2/2. The portable evidence file distinguishes these current controls from
  the original history and excludes its initial setup-invalid harness attempts.
- Post-tracer gate configure/build and CPU-path/exception selection passed2/2.
  The MMU matrix, inventory selftest and exact99 gate/101 ASan inventory passed;
  all84 integer opcode rows and existing coverage controls remain present.
- The sole full gate initially passed98/99 CTests, stopping at warm-v1 CACHE.
  After the profile dispatch fix, the gate-equivalent CTest stage passed99/99.
  Its completed preset log was reconciled after the session handle expired;
  the stale failure list was not treated as a new failure or reason to rerun.
  The previously unreached core-lint tail then passed22 files/zero violations.
- Refreshed full all-target ASan build succeeded. The final10 affected tests,
  including MMU/path/CP0/exceptions/time, cache, boot/digest/determinism and
  open-IPL3 integration passed under the required ASAN/UBSAN options. The
  sanitizer negative control also passed separately. No full101-test sanitizer
  rerun is claimed.
- Parent's fresh unwaived PII scan passes after portable specification/evidence
  updates. Fresh full hosted verification is attached to source565dba4.

## Deviations and Issues

- Shared exception context and existing exception fixtures required narrow
  omitted-file edits. No new persistent state was needed beyond the already
  hashed TLB/CP0/shutdown fields; no artificial serializer change was made.
- Invalid entries still match tags before V is checked (NEC §5.4.9). Zero seed
  tags can therefore duplicate-match ASID0/tag0 and select TS shutdown. This
  is tested/documented without changing the seed golden or asserting reset
  silicon contents. Earlier fixtures now explicitly isolate their intended
  refill or invalid conditions.
- The protected-CP0 fixture needed a valid user mapping so fetch would not
  precede the intended MFC0 protection fault. An unadmitted-machine exception
  setup in the matrix was corrected; that setup failure is not RED evidence.
- Manual review corrected XContext region/BadVPN2 masks and writes in all modes;
  new supervisor-region and sign-extended KSEG3 assertions cover the distinction
  from 32-bit EntryHi's zero-extended VPN field.
- The sole integration failure was a real policy-boundary error: new TLB
  preflight reached warm-v1 CACHE. Restricting it to the VR4300 profile preserves
  the historical unsupported warm case, while a separate VR4300 supported-hit
  CACHE operand proves refill before cache/reservation effects. No full cache
  line/writeback fidelity is claimed.
- ASan reports2528-byte t64_mmu_translate (new module) and2912-byte
  t64_cpu_exception_enter (touched code), above the2048-byte diagnostic threshold.
  They remain nonfatal measured warnings; no baseline equality or warning fix
  is claimed. Other diagnostics include legacy/digest routines. The build and
  affected instrumented runtime tests pass, without claiming general stack safety.
- Both original formal RED classifications were missed and source/tests were
  committed together. This historical workflow deviation is retained explicitly.
  Current isolated controls validate the regression tests, not the missing
  original guard. AUTONOMY now explicitly applies that guard to tdd=true tasks
  even under an execute plan/global TDD-off setting.

## Evidence Limits and Readiness

The CPU-04 unclassified edge flag and CPU-03 judgment-prohibition flag remain
non-authoritative under AUTONOMY. No silicon validation, human approval or risk
acceptance is implied. C attribute propagation is not implementation of every
cache algorithm. Cache lines, full memory merges/RE/Watch, LL/SC and COP1 remain
their later slices. Shared requirements remain gated on unfinished siblings.
Ready for Plan04-07; no user action is needed.

## Self-Check: PASSED

Both feature commits, qualified public documentation, semantic history and
separate current control records exist. Required current behavior checks are
accounted for with the historical process deviation preserved. Owner AGENTS
and configuration edits remain intact and unstaged; raw reports and mutants
remain private/ignored. Parent owns state and CI closeout.
