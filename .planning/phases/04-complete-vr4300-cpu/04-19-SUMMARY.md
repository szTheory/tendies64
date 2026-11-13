---
phase: 04-complete-vr4300-cpu
plan: "19"
subsystem: cpu-compatibility
tags: [vr4300, software-policy, profile-isolation, sealed-selection]
requires:
  - phase: 04-18
    provides: Native CPU verification and local diagnostic boundary.
provides:
  - Distinct compat-v1 model4/state10/semantic2 with strict behavior preserved.
  - Explicit sealed compatibility selection through the production adapter.
affects: [04-16, 04-20, cpu-qualification]
plan_head_before: 5028c4ffa58055242c73218c3e09b251208f962a
plan_head_after: 61c9c174428e55594f9d8cb45e4ef8de4f63507c
tech-stack:
  added: []
  patterns: [per-machine-policy, versioned-state-domain, immutable-profile-selection]
key-files:
  created: [docs/spec/cpu-compatibility.md]
  modified: [core/t64_machine.c, core/t64_cpu.c, core/t64_cp0.c, core/t64_memory.c, core/t64_digest.c, scripts/run-cpu-diagnostic.py]
requirements-completed: []
completed: 2026-11-13
status: complete
---

# Plan19 — Compatibility foundation

Implemented and pushed `61c9c17`. The new software profile has a distinct
identity, seed policy marker and v10 state domain. Narrow CP0 transfer,
division, nested branch and uncached linked-memory policies apply to that
profile. Strict model3/state9 and warm vectors/refusals are preserved.
Explicit branch origin preserves EPC/BD through displaced delay slots.
Seal v2 records the selected profile; legacy v1 selects strict. Loader,
runtime, inventory and shared aggregation bind the actual selected identity.

## Verification

- Complete native gate: all111 registered checks, inventory and core lint pass.
- All instrumented targets build; the39-test owning selection, including
  admission fuzz and sanitizer negative control, passes within existing caps.
- Added review controls pass owning normal/instrumented checks. The final
  all-target instrumented build and all three diagnostic controls pass.
- Configured deep review and focused follow-up disposition all three coverage
  warnings. The final annul side-effect/retirement assertion was independently
  requested and verified by the orchestrator in both builds.
- Incremental security verdict is SECURED for both new registered threats.
- [Hosted run43452732705](https://github.com/szTheory/tendies64/actions/runs/43452732705)
  verifies this exact source on Ubuntu24.04 and macOS26, including the complete
  gate, authored evidence scans and sanitized upload. Rights/PII/provenance
  scans passed before commit and again before push.

[Validation](04-19-VALIDATION.md), [review](04-19-REVIEW.md),
[security](04-19-SECURITY.md) and [native evidence](../../../docs/evidence/phase04-native-cpu.md)
retain exact scope, vectors, failed fixture attempts and test-first limits.
No private diagnostic result or input enters this summary or hosted evidence.

## Limits and next slice

This completes the native compatibility foundation and gate binding, not
Plan16 or SC4. Actual qualification remains incomplete. Plan20 addresses
the observed additional gaps under a distinct compat-v2 profile while keeping
strict and the published compat-v1 stable. Required membership, authenticated
completion, rights and all execution limits remain unchanged.

## Threat Flags

No unregistered threat flag was identified in the Plan19 implementation.
The later Plan20 policies have their own registered threats and verification;
the Plan19 audit does not pre-approve those source changes. No risk was accepted.

## Lessons

Preserve a real negative baseline before interpreting a green wrapper. Keep
long sanitizer/fuzz work separate from wall-capped actual guest runs; retain
failed attempts and investigate one case before a justified retry. Fix
review-control gaps with owning tests and reuse unaffected native evidence.
The hosted wait remains dominated by qualification, not new emulator work;
keep the two useful platform jobs without speculative caches or concurrency.
