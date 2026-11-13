---
phase: 04-complete-vr4300-cpu
plan: "20"
status: verified
verdict: SECURED
threats_closed: 3
threats_open: 0
asvs_level: 1
security_block_on: high
---

# Compatibility v2 security verification

The configured `gsd-security-auditor` returned **SECURED** for the three
registered Plan20 threats and a narrow post-review COP1 correction addendum.
Root persisted the returned verdicts; the auditor wrote no files, ran no
tests/builds/scans and inspected no private actual diagnostic data. All
dispositions are mitigate; no risk acceptance or transfer was inferred.

| Threat | Severity | Status | Verified mitigation |
|---|---|---|---|
| T-04-40: version, instance, state and fault isolation | high | closed | Canonical immutable v2 descriptor checked before create/admit, per-machine policies/latch, explicit v11 domain and future-state serialization, staged COP2/FPR effects, old-profile controls and independent state vectors. |
| T-04-41: selector or targeted denominator conceals missing cases | high | closed | Closed profile-to-option mapping, immutable executable/seal checks before and after calls, actual runtime/inventory identity comparison, unchanged full source membership and shared C aggregation. Targeted evidence explicitly cannot close the full gate. |
| T-04-42: zero-data fallback bypasses devices, fetch or stores | high | closed | V2-only complete bounded ranges, overflow/probe/aperture exclusions, CPU data/D-cache-only callsites, unchanged global bus/fetch/DMA paths and store guards before line, register or reservation mutation. |

Source evidence is in `core/t64_machine.c`, `t64_internal.h`, `t64_digest.c`,
`t64_cpu.c`, `t64_cop1.c`, `t64_cp0.c`, `t64_mmu.c`, `t64_memory.c`,
`t64_cache.c`, `t64_bus.c`, the production adapter and selection scripts.
Owning controls cover canonical profile/state identity, latch/exception
ordering, range boundaries, mapped-device/fetch exclusion, store atomicity,
raw PageMask refusal, high-address faults and sealed cross-profile results.

## Bounded rechecks

The saved 39-threat closure remains the baseline. Changed instruction/store
commit, CP0 privilege/field atomicity, MMU address/fault checks, cache/tag and
dirty-writeback transactions, coprocessor result/context isolation, unsigned
arithmetic, selector/member binding, resource bounds and private output remain
closed. No source/tool/package/asset acquisition or copied helper was added;
the auditor repeated no supply-chain audit.

## COP1 correction addendum

The independent follow-up confirms conversion destinations normalize only for
64-bit results, with the separate MOV path gated by source/result width.
Selected conversion source aliases remain v2-only. Decoded indices are
five-bit; normalized pairs are at most30, scalar commits validate the index
and preserve upper words, and trapped results return before publication.
The distinct-source/odd-destination regression checks exact fetched PC/opcode,
retirement, unchanged even neighbor and the requested destination. Strict
and compat-v1 remain gated off. No new serialized field or selector was added.
The earlier T-04-40/41/42 verdict therefore remains SECURED.

## Verification and limits

Root supplied successful owning controls and the full native gate. The broad
instrumented selection was pending at the initial audit dispatch and later
passed. After the COP1 correction, complete target builds and six owning
COP1/three-diagnostic checks pass in each configuration. Earlier broad runs
retain their actual pre-correction identities; no old artifact is relabeled.

No Plan20 SUMMARY existed during audit, so its executor Threat Flags section
could not be checked. No unmapped attack surface or new correction flag was
identified. This is mitigation verification, not SC4 completion, public rights
clearance or silicon qualification. Final publication scans, exact-source
hosted CI and the actual complete-set gate remain Root's separate duties.
