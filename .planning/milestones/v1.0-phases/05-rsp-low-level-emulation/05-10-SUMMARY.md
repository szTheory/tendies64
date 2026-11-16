---
phase: 05-rsp-low-level-emulation
plan: "10"
subsystem: testing
tags: [rsp, qualification, evidence, security, c17]
requires:
  - phase: 05-09
    provides: authored CPU-to-SP-to-RSP fixtures and exact registered inventory
provides:
  - Source-anchored exact local classification and original C-record validation
  - Corrected DMA refusal atomicity and synchronous guest HALT behavior
  - Configured review dispositions, secured threat register and actual final receipt
affects: [06-cold-boot-cpu-timing-and-full-base-suite]
actuals:
  tokens: 114370
  tasks: 3
  commits: 7
tech-stack:
  added: []
  patterns: [independent-source-closure, immutable-executable-binding, semantic-evidence-reuse]
key-files:
  created:
    - scripts/check-rsp-evidence.py
    - scripts/tests/test_rsp_evidence.py
    - docs/evidence/phase05-final-rsp-evidence.json
    - docs/evidence/phase05-native-rsp.md
    - docs/evidence/phase05-review.md
    - docs/evidence/phase05-security.md
  modified:
    - core/t64_rsp.c
    - core/t64_sp.c
    - core/tests/test_rsp_control.c
    - scripts/check-rsp-no-hle.py
    - scripts/prepare-rsp-diagnostic.py
    - scripts/verify-rsp-diagnostic-qualification.py
    - scripts/run-rsp-diagnostic.py
    - runner/t64_rsp_diag_protocol.c
key-decisions:
  - "A complete independently anchored classification partition does not imply suite success."
  - "Preserve immutable failed contexts; qualify an explicit fresh context after a reviewed host fix."
  - "Reuse unchanged core semantics with their actual source identity; refresh exact provenance artifacts separately."
patterns-established:
  - "Accepted fixtures invoke the complete verifier entry and normal dispatch, not only helper checks."
  - "Required membership is checked against pinned source qualification before original C-record validation."
requirements-completed: [RSP-01, RSP-02, RSP-03, RSP-04, RSP-05]
coverage:
  - id: final-local-partition
    description: Every required local RSP member is authenticated or individually classified with a bound reason
    requirement: RSP-04
    verification:
      - kind: integration
        ref: "python3 -B scripts/check-rsp-evidence.py --check"
        status: pass
    human_judgment: false
  - id: portable-reference-and-dma
    description: Fetched scalar/vector reference, DMA/control and preserved profile contracts have native and instrumented evidence
    requirement: RSP-01
    verification:
      - kind: integration
        ref: "build/gate/05-10-final-gate.log (semantic baseline 3a3c8ca)"
        status: pass
      - kind: unit
        ref: "build/asan/05-10-final-ctest.log (semantic baseline 3a3c8ca)"
        status: pass
    human_judgment: false
  - id: evidence-and-no-hle
    description: Source/object and final evidence controls reject bounded bypasses, stale artifacts and resealed reduced membership
    requirement: RSP-05
    verification:
      - kind: unit
        ref: "scripts/tests/test_rsp_evidence.py and scripts/tests/test_rsp_no_hle.py"
        status: pass
    human_judgment: false
  - id: reviewed-artifacts
    description: Actual reviewed artifacts and authored-only hosted pair are bound to the final source
    verification:
      - kind: other
        ref: "docs/evidence/phase05-final-rsp-evidence.json and 05-SECURITY.md"
        status: pass
    human_judgment: false
completed: "2026-11-15"
status: complete
---

# Plan 05-10: Exact classification and artifact evidence

The current functional RSP profile has an independently source-anchored local
classification partition, preserved original C records, scoped configured
review closure and an actual passing final evidence check.

## Accomplishments

- Fetched MTC0 DMA admission is staged before the current DMA opportunity can
  change memory or queues. Guest self-HALT retires alone, preserves its resume
  PC and allows DMA to continue; asynchronous host and unsupported single-step
  requests retain their explicit refusal policies.
- The no-HLE guard closes the RSP/SP function inventory and host entry routes.
  The final checker binds fixed executable bytes and runtime identities,
  registered passing tests, independent source membership, original C outputs
  and attempts, hosted artifacts, authored composition and actual reports.
- The current-core qualification, independent full preflight, single bounded
  local run and same-directory completion verification are retained privately.
  Each required member has an authenticated result or individually bound
  reason. Outcomes remain private; completeness is not represented as suite
  success. No guest retry or deadline/capture expansion occurred.
- Configured full and scoped reviews are preserved with parent dispositions.
  The configured ASVS1/high-blocking follow-up returned SECURED for all four
  remaining rows; it did not repeat the prior22 closures.

## Actual verification

The core semantic baseline is `3a3c8ca`. Its canonical gate passes the exact127
registrations and lint86/0 in92s. Its all-target ASan/UBSan build passes the31
owning RSP/shared CPU protocol/regression tests, admission fuzz and sanitizer
negative control in125.98s, with exact129 registration inventory. Original
logs and artifacts remain archived under
`build/phase05-artifact-snapshots/3a3c8cae992a/`.

Host-only correction `61542f9` changes no core/build-policy input. Normal and
instrumented provenance-refresh builds retain the actual compiler-policy
build IDs and profile identities. Exact new binaries and identity records are
archived under `build/phase05-artifact-snapshots/61542f9b2c3a/`, with the older
semantic test source explicitly recorded. Current qualification30, runner12
and evidence7 authored controls pass, including the full verifier entry,
normal dispatch and undefined-global checks. An isolated source copy passes
all three authored harness suites without private diagnostic roots.

Both required hosted jobs pass the complete127-test gate on the corrected
source in [the exact final run](https://github.com/szTheory/tendies64/actions/runs/43665538190).
Their sanitized artifacts, API identities and measured critical-path review
are recorded in the [receipt](../../../docs/evidence/phase05-final-rsp-evidence.json)
and [CI ledger](../../../docs/ci-reviews/LOG.md). Required rights/privacy tree
and exact push-range scans pass. No private input or result is uploaded.

The actual final check prints `PASS: final RSP evidence and artifact bindings`;
its current log is `build/gate/05-10-final-evidence-check.log`. The active local
v3 sidecar points to the new audited records; its prior historical version is
preserved separately. No record is relabeled as execution by a newer binary.

## Source commits

| Commit | Work |
|---|---|
| a598f44 | Correct nonzero-member batch retrieval. |
| fcd2ee4 | Honor bounded protocol text fields. |
| 0b8786b | Validate the then-current pre-use binding. |
| 41ff2f3 | Correct DMA refusal ordering and guest HALT semantics; strengthen source guards. |
| 578e86a | Add actual artifact/C-record evidence binding and authored controls. |
| 3a3c8ca | Add reviewed-core qualification and independent source/privacy corrections. |
| 61542f9 | Exercise the complete verifier and qualify an explicit immutable refresh. |

`actuals.tokens` is the source diff character estimate divided by four, not
provider token usage. Active work duration was not reliably measured across
sessions/restart and is not reconstructed from chat elapsed time.

## Deviations and lessons

Review and actual integration exposed substantive defects beyond report
assembly. They were fixed with scoped controls and the required affected
verification. Early setup failures, invalid RED setups and historical results
retain their original labels. In particular, v6 pre-use stopped before adapter
or guest use on undefined digest-pattern names: a helper-only fixture had
missed the complete entry. The full-entry regression and fixed refresh
preserve that failure and its immutable archive. Verification now precedes
fresh run-directory creation.

Membership coverage rejects a two-sided reseal against the independently
qualified source. Quoted selected-ROM aliases remain private even if their
value equals the one approved authored composition digest. Exact qualified
executables and bindings are archived before later rebuilds. Missing CI Runs
rows were recovered from completed-run metadata without relaunching CI.

## Limits and stops

The portable scalar C path is the permanent reference. No separate optimized
VU backend exists, so no optimized differential execution is claimed. Timing,
ambiguous flags/clamp/divider/VSAR behavior and unsupported conflicts retain
their named software policies and evidence limits. Hardware accuracy and
general compatibility are not established. An authenticated guest failure
without a retained failing assertion does not localize its semantic cause.

The exact executable from an older historical private run was not retained;
that limit is disclosed rather than replaced with a newer artifact. Current
qualification retains exact binaries and bindings. D2 private publication,
physical testing, unapproved copied inputs and release-channel stops remain
effective. No human validation, risk acceptance or release is invented.

Phase-goal verification is the next action; this plan summary does not itself
mark the phase or milestone complete.
