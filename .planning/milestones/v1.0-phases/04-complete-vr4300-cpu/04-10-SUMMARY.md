---
phase: 04-complete-vr4300-cpu
plan: "10"
subsystem: cpu
tags: [data-cache, writeback, primary-cache-ops, precise-faults, sanitizer]
requires:
  - phase: 04-09
    provides: Functional I-cache and documented internal physical-line key.
provides:
  - Functional 8KiB write-back D-cache and nonsnooping aliases.
  - Manual-derived primary CACHE/tag operations and precise writeback bus faults.
affects: [phase-04-cop1, phase-04-qualification]
actuals:
  tasks: 2
  commits: 4
plan_head_before: fa57a9fd70f3ba612312f76770116d1cd9457e93
plan_head_after: ca53982ca059f07cacf6a998b6946fa080a84965
tech-stack:
  added: []
  patterns: [Staged line replacement, attributed writeback, explicit execution status]
key-files:
  created: []
  modified: [core/t64_cache.c, core/t64_memory.c, core/t64_bus.c, core/t64_cpu.c, core/t64_internal.h, core/tests/test_cpu_cache.c, core/tests/test_cpu_link.c, core/tests/test_cpu_memory.c, core/tests/test_cpu_path.c, core/CMakeLists.txt, scripts/check-test-inventory.sh, docs/spec/cpu-cache.md, docs/spec/cpu-memory.md, docs/spec/cpu-profile.md]
key-decisions:
  - Follow the primary table's valid-line D index writeback and unconditional matching I writeback.
  - Preserve internal aligned-line keys while mapping TagLo hardware PTag/state explicitly.
  - Architectural unmapped/range writeback errors enter DBE; mapped model refusals remain distinct.
requirements-completed: ["CPU-05", "CPU-06", "CPU-09"]
coverage:
  - id: D1
    description: Actual cached stores/writeback and scheduled PI/SP transfers expose cached-versus-backing visibility.
    requirement: CPU-06
    verification:
      - kind: unit
        ref: core/tests/test_cpu_path.c#unit.core.cpu-path
        status: pass
    human_judgment: false
  - id: D2
    description: CACHE/tag operations, reservations, aliases, refusals and precise bus faults have native assertions.
    requirement: CPU-09
    verification:
      - kind: unit
        ref: core/tests/test_cpu_cache.c#unit.core.cpu-cache-ops
        status: pass
      - kind: integration
        ref: Final gate and refreshed nine-target instrumented selection
        status: pass
    human_judgment: false
duration: 85min
completed: 2026-11-12
status: complete
---

# Phase 4 Plan 10: Write-back Cache and Primary Operations Summary

The native cache implementation in `core/t64_cache.c` and guest/cache
assertions in `core/tests/test_cpu_cache.c` complete functional split-cache
visibility and primary CACHE operations for the selected profile.

## Accomplishments

- The512x16-byte D-cache stages fills and old dirty writeback acceptance before
  replacement; byte-masked stores commit dirty data after backing validation.
  Uncached accesses and PI/SP DMA see backing independently of valid cache
  lines. Ordinary stores, maintenance writeback and external writers have
  distinct reservation effects.
- Primary operations cover I index invalidate; D index writeback-invalidate;
  I/D index load/store tag; D create-dirty-exclusive; I/D hit invalidate;
  I fill; D hit writeback-invalidate; and I/D hit writeback. Tests retain the
  primary table citations, including clean valid D index writeback and matching
  I writeback independent of a dirty bit.
- Internal aligned-line keys map explicitly to/from hardware TagLo PTag/state
  using page-offset/index bits. Undefined operation/state/region combinations
  refuse before operation mutation.
- Explicit cache/memory status reaches the existing precise DBE seam for
  BUS_UNMAPPED/BUS_RANGE. CACHE writeback and ordinary cached LW/SW eviction
  faults preserve the old line, link and backing, and do not retire.
  Mapped readonly-device refusals remain model stops.

## Task Commits

1. **Task 1: dirty store and scheduled writers** - `98276a8` (test), `38d3336` (feat).
2. **Task 2: primary operations and fault coverage** - `906c604` (test), `ca53982` (feat).

## Automated Checks

- Task1 actual pre-implementation guest SW changed RDRAM from0 to12345678
  immediately and left the selected D-line invalid, instead of retaining
  dirty data until writeback. Exit8/RED_EVIDENCE_OK preceded implementation.
  Missing-helper and zero-extended-KSEG setup attempts were excluded.
- Task1 CPU-path/link/PI-DMA/SP-DMA passed4/4 after the fixtures checked
  cached committed bytes and observed DMA before subsequent SC. Full all-target
  ASan build and the same4 instrumented targets passed.
- Task2 actual pre-implementation I Hit_Write_Back0x18 returned status14 and
  left backing10111213 instead ofa0a1a2a3, preserving line/tag/link.
  Exit8/RED_EVIDENCE_OK preceded the primary-operation implementation.
- Parent review's later guest regressions confirmed a valid dirty tag04900000
  was BUS_UNMAPPED, but CACHE WB-invalidate and cached LW/SW eviction originally
  stopped unsupported without the required cause/EPC. The corrected paths
  produce ExcCode7 with no retirement or failed-operation state mutation.
  These supplement the original Task2 RED rather than replacing its history.
- The exact Task2 I-cache/D-cache/CACHE-ops/link selection passed4/4.
  Coverage includes hit/miss/refused writeback, line endpoints, tag state,
  digest/isolation, cached user-RE SB/SH/SW/SD lanes and both SP DMA directions.
- The final full all-target ASan build and all9 affected instrumented targets
  passed after the last cached-RE/SP/readonly-refusal additions. The selection
  includes I/D/cache-ops/link/memory/path/integer and PI/SP DMA.
- The one final common gate passed104/104 CTests, fixture qualification,
  inventory and core lint24 files/zero violations. No failed full-gate stage
  or unreached tail occurred.
- Inventory controls preserve104 gate/106 ASan names and all84 integer rows.
  Portable records/specifications pass unwaived tree/history PII checks;
  final sourceca53982 then passed both full hosted lanes:104 tests, zero lint
  violations, guest evidence and publication checks. Timings and review are
  tracked in docs/ci-reviews/LOG.md.

## Deviations and Evidence Limits

- New write-back visibility required explicit setup/oracles in existing tests:
  cached LL/SC checks inspect cache bytes; no-snoop snapshots precede later SC;
  the independent lane matrix uses uncached TLB windows. The reused unsupported
  CACHE fixture now uses undefined0x1c because I index-invalidate0 is supported,
  with synthetic instruction-index reset after code rewrites.
- Narrow CPU/header/spec/status-seam omissions were required. No new persistent
  scratch fault state or serializer change was introduced; v9 already binds
  all cache, dirty and reservation fields.
- ASan reports a new3728-byte t64_cpu_cache_operation frame, alongside prior
  CPU/memory/MMU/exception/digest/machine/DMA large-stack diagnostics and
  duplicate-library warnings. These remain measured nonfatal warnings;
  passing runtime checks do not establish general stack safety.
- Cache timing/arbitration and physical-console accuracy are not qualified.
  Non-snooping, reservation and prepared-state policies remain selected model
  choices. CPU-06's unclassified evidence flag and CPU-05's non-authoritative
  unverified-prohibition flag are retained; no human acceptance is invented.

## Next Work

Execute Plan11's pre-audited explicit-context arithmetic closure, then Plan17's
raw arithmetic matrix before COP1 consumers. No user decision is needed for
authorized local work; provenance and existing rights/physical/release stops
remain binding.

## Self-Check: PASSED

