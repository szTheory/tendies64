---
phase: "04"
slug: complete-vr4300-cpu
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-11-11"
---

# Phase 4 — Validation Strategy

Planning seed: no Phase 4 implementation or execution is claimed. The planner
must replace the area map with exact plan/task/test/threat ownership and final
inventory counts before planning passes.

## Test Infrastructure

| Property | Value |
|---|---|
| Framework/config | C17 CTest; `CMakePresets.json`, core/runner CMake registration |
| Existing exact inventory | 91 gate / 93 ASan names; preserve all |
| Quick command | `ctest --preset gate --no-tests=error -R '<owning exact names>' --output-on-failure`, after target build |
| Common gate | `sh scripts/gate.sh` |
| Sanitizers | all-target `cmake --build --preset asan`; applicable CTest with existing ASan/UBSan environment and intentional fault control |
| Runtime | Unknown for new cases; measure, do not invent a feedback estimate |

## Sampling Rate

Add behavioral tests in the owning slice and run focused checks after relevant
changes. Run full gates at coherent inventory boundaries and final source;
reuse fresh passing evidence for metadata-only changes. No new CI lane/cache
or timeout increase. Local external-ROM checks stay explicitly separate and
cannot execute or upload in public CI.

## Requirement-to-Behavior Map

| Requirement | Required behavior and independent edge/negative proof |
|---|---|
| CPU-01 | 32/64-bit arithmetic, extension, shifts, multiply/divide and overflow effects |
| CPU-02 | Branch/likely/jump, annulment, delay-slot faults and nested EPC/BD |
| CPU-03 | Defined CP0 masks/side effects, exception/interrupt priority and Count phase/order |
| CPU-04 | Segments, privilege, all TLB page sizes, ASID/global, refill/invalid/modified/XTLB/address errors |
| CPU-05 | Unaligned merge/endian/page boundaries; LL/SC/LLD/SCD and explicit invalidation policy |
| CPU-06 | Functional split-cache visibility, aliases, writeback, tags and invalidations with written scope |
| CPU-07 | Integer-only arithmetic, all four rounding modes and both formats, FCR31/FR/FS/E, instance isolation |
| CPU-09 | Independent cited/derived vectors, no same-implementation oracle; complete edge/negative coverage |
| VER-01 | Qualified pinned local CPU suite, exact complete per-test/value/group status; private outputs |
| VER-03 | Exact native and local inventories with every omitted/reduced/duplicate case rejected |

## Wave 0 Requirements

None. Existing infrastructure is sufficient; new tests belong to their features.
Qualification of a copy/package/asset precedes its use within the relevant slice.

## Manual-Only Verification

None planned. Physical hardware evidence is outside this phase. Explicit rights
or tool qualification failures block their affected use; native tests do not
substitute for an unmet local-suite criterion. No owner sign-off is inferred.

## Validation Sign-Off

- [ ] Every task has an immediate automated failure direction.
- [ ] Exact test/plan/task/threat map and final inventory counts are fixed.
- [ ] Every requirement has independent positive/edge/negative coverage.
- [ ] Private diagnostic input/output stays outside public gates/artifacts.
- [ ] Implementation and actual applicable checks pass.

Approval: pending automated post-implementation validation; no human approval claimed.
