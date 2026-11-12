---
phase: 04-complete-vr4300-cpu
plan: "09"
subsystem: cpu
tags: [instruction-cache, aliases, precise-fills, sanitizer]
requires:
  - phase: 04-08
    provides: Physical reservations and accepted-writer overlap notifications.
provides:
  - Functional 16KiB virtual-index physical-key instruction cache.
  - Executed stale/invalidate behavior and independent cache geometry coverage.
affects: [phase-04-data-cache, phase-04-qualification]
actuals:
  tasks: 2
  commits: 3
plan_head_before: 0e8269fd43411ce306a700041de6bc40e6fc2c70
plan_head_after: 6b096e7f99228bea57b86f254ce7aefb674c7416
tech-stack:
  added: []
  patterns: [Staged full-line fills, non-snooping instruction visibility]
key-files:
  created: [core/t64_cache.c, core/tests/test_cpu_cache.c, docs/spec/cpu-cache.md]
  modified: [core/t64_cpu.c, core/t64_internal.h, core/tests/test_cpu_path.c, core/tests/test_cpu_memory.c, core/CMakeLists.txt, scripts/check-test-inventory.sh, docs/spec/cpu-profile.md]
key-decisions:
  - Preserve the equivalent aligned physical-line key and document its mapping to hardware PTag.
  - Task2 is honest green-start verification; remove its inapplicable TDD marker.
requirements-completed: ["CPU-06", "CPU-09"]
coverage:
  - id: D1
    description: Actual guest execution observes stale code until targeted invalidation; refused fill preserves the old line.
    requirement: CPU-06
    verification:
      - kind: unit
        ref: core/tests/test_cpu_path.c#unit.core.cpu-path
        status: pass
    human_judgment: false
  - id: D2
    description: Geometry, synonyms, attributes, accepted backing, no-snoop and state identity have native expected assertions.
    requirement: CPU-09
    verification:
      - kind: unit
        ref: core/tests/test_cpu_cache.c#unit.core.cpu-icache
        status: pass
      - kind: integration
        ref: Repaired four-target gate and instrumented selections; first lint tail
        status: pass
    human_judgment: false
duration: 63min
completed: 2026-11-12
status: complete
---

# Phase 4 Plan 9: Functional I-cache Summary

`core/t64_cache.c` implements the new-profile instruction cache;
`core/tests/test_cpu_cache.c` independently exercises its geometry, visibility
and refusal boundaries. Warm-v1 retains its separate coherent-functional path.

## Accomplishments

- The 512x32-byte direct-mapped cache uses virtual index and translated
  physical identity. A complete staged fill commits data/key/validity only
  after all bytes in one accepted backing range pass validation.
- Executed code stays stale after uncached backing changes until matching
  Hit_Invalidate I or index replacement. Refused fills preserve prior line
  state and do not publish an attempted instruction.
- The11-case matrix covers indices0/511, final word offset28, same-index
  replacement and mismatched invalidation, virtual synonyms, K0/TLB uncached
  bypass, RDRAM/DMEM/IMEM/cartridge backing, range refusal, PI/SP no-snoop
  behavior, existing v9 digest sensitivity and machine isolation.
- The public cache contract documents functional scope, primary facts,
  internal key encoding, unsupported boundaries and reversal evidence.

## Task Commits

1. **Task 1: executed stale code and cache path** - `5dd1cf4` (test), `2c2b01a` (feat).
2. **Task 2: geometry and documented scope** - `6b096e7` (test/docs).

## Automated Checks

- Task1 actual pre-implementation CPU-path report expected stale GPR8=1
  after a guest KSEG1 backing store, observed2 with backing opcode24080002.
  Exit8 and RED_EVIDENCE_OK preceded implementation. Post-feature CPU-path
  and MMU passed2/2; full all-target ASan build and the same selection passed2/2.
- Task2's first behavioral CPU-icache run passed1/1 at10:22:52Z. Seven unused
  helper-parameter compile diagnostics were corrected before that run.
  This verification-only task began green; its TDD marker was removed with
  rationale under AUTONOMY. No Task2 architectural RED is claimed.
- The exact planned configure/build and CPU-icache/path/CP0-cache selection
  passed3/3; full all-target ASan build and that selection also passed3/3.
- The one full local gate passed101/102 CTests, failing CPU-memory because
  its synthetic opcode fixture rewrote cached code between independent cases.
  The test fixture now explicitly invalidates that fetch line after rewrites,
  including hand-built delay-slot programs; production cache behavior stays
  intact. The repaired CPU-memory/I-cache/path/CP0-cache selection passed4/4.
  The first previously unreached lint tail passed24 files/zero violations.
  A successful repeated full local gate is not claimed.
- After the fixture correction, final full all-target ASan build succeeded and
  all4 affected instrumented targets passed. Inventory controls passed with
  102 gate/104 ASan names and all84 integer opcode rows.
- Unwaived PII tree/history scans pass. Fresh full hosted verification of
  final source is tracked separately in docs/ci-reviews/LOG.md.

## Deviations and Evidence Limits

- The manual's hardware PTag is PA31:12; the internal field stores PA aligned
  to32 bytes. At a fixed virtual index, valid translation fixes PA11:5 from
  the page offset. The keys are equivalent, and no supported operation yet
  exposes the field as TagLo. Temporary raw-PTag-as-storage assertions were
  withdrawn as an unsupported internal-format assumption, not a lookup bug.
  Their failed report remains private and excluded from valid RED evidence.
  The software encoding and eventual diagnostic mapping are explicit in the
  public spec; a future tag operation must preserve that distinction.
- Task2 adds tests/docs over already implemented behavior; the marker removal
  records that fact instead of manufacturing a failure. The CPU-memory setup
  correction and CPU/header/profile omissions are narrow required changes.
- Existing v9 serialization already hashes every line's encoded key, bytes
  and valid bit; no serializer change was necessary. The portable evidence
  filename was normalized without rewriting history.
- ASan retains existing large-stack diagnostics and runner duplicate-library
  warnings. Neither the new cache module nor its test produced a warning.
  Passing instrumented checks do not establish general stack safety.
- No cache miss timing, arbitration, pipeline or physical-console qualification
  is claimed. D-cache/writeback and remaining primary CACHE operations are
  next. CPU-06's unclassified flag remains pending later qualification.

## Next Work

Execute Plan10 write-back D-cache and primary CACHE operations. No owner
decision is needed; existing rights, physical and release stops remain.

## Self-Check: PASSED

