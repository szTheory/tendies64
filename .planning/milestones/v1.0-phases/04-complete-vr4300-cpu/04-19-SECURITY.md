---
phase: 04-complete-vr4300-cpu
plan: "19"
status: verified
verdict: SECURED
threats_closed: 2
threats_open: 0
asvs_level: 1
security_block_on: high
---

# Compatibility profile security verification

The configured `gsd-security-auditor` returned **SECURED** for the two newly
registered threats and the changed portions of previously closed mitigations.
The orchestrator persisted the returned verdict; the auditor wrote no files.
All dispositions are mitigate. No risk acceptance or transfer was inferred.

| Threat | Severity | Status | Verified mitigation |
|---|---|---|---|
| T-04-38: compatibility semantics or identity leak into strict callers | high | closed | Canonical descriptor hashes checked before create/admission, copied per-machine descriptors, per-machine policy selection, distinct v10 state domain and origin serialization, strict v9 field exclusion and retained strict/warm vectors/refusals. |
| T-04-39: unsealed profile selection contaminates inventory/results | high | closed | Closed v2 selection, v1 strict-only interpretation, before/after immutable seal and executable checks, actual runtime profile/core comparison, explicit build seal selection, guest identity verification, shared aggregation and authored cross-profile controls. |

The source evidence is in `core/t64_machine.c`, `t64_internal.h`,
`t64_digest.c`, `t64_cpu.c`, `t64_cp0.c`, `t64_memory.c`,
`t64_exception.c`, `runner/t64_cpu_diagnostic.c`,
`scripts/run-cpu-diagnostic.py` and `build-qualified-cpu-diagnostic.py`.
The profile/boot, CPU integer/control/link, CP0 and routing controls exercise
the corresponding identities, policies, atomic refusals and cross-profile
failures. The synthetic routing control stubs only upstream qualification;
its actual runtime, inventory, encoding and aggregation calls use C.

## Bounded mitigation rechecks

The saved 37-threat audit remains the baseline. The auditor rechecked changed
parts of instruction commit, integer/branch arithmetic, zero/extreme division,
CP0 privilege and field atomicity, conditional-store fault ordering, exact
membership, transcript/resource bounds, snapshots/selectors, ignored private
storage, qualification and disclosure boundaries. Those portions remain closed.
Zero-divisor guards precede host division; signed extrema use unsigned host
arithmetic. MMU/privilege checks precede compatibility admission, and rejected
stores return before reservation/result commit. Delay-slot exception paths
share the explicit compatibility origin and exception entry clears it.

No new external input, dependency, copied helper or tool acquisition was
introduced. The auditor repeated no supply-chain audit, build, test or scan;
the orchestrator supplied the saved native and instrumented verification
evidence described in [validation](04-19-VALIDATION.md).

## Limits

No Plan19 SUMMARY existed during the audit, so its executor Threat Flags
section could not be checked. No unmapped attack surface was identified in
the inspected delta. Actual private diagnostic results were not inspected or
reproduced. This is mitigation verification, not SC4 completion or silicon
qualification. Final publication scans remain the orchestrator's before-push
gate; this report does not claim they ran after it was written.

No user action is needed for this mitigation verdict.
