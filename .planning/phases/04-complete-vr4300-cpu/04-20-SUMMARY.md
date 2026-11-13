---
phase: 04-complete-vr4300-cpu
plan: "20"
subsystem: cpu-compatibility
tags: [versioned-policy, native-regression, profile-isolation]
requires:
  - phase: 04-19
    provides: Isolated compatibility foundation and sealed selection.
provides:
  - Immutable compat-v2 model5/state11/semantic3 with strict and compat-v1 preserved.
  - Reviewed cache, coprocessor, CP0 and data-window policies with owning controls.
affects: [04-16, cpu-qualification]
plan_head_before: c3856d1e7613ad28a59ff5fd61c0ce415e8f6ebc
plan_head_after: f702f0ca32069f0b9b30040727c3617eab85b786
requirements-completed: [CPU-09]
completed: 2026-11-13
status: complete
---

# Plan20 — Versioned compatibility extensions

The approved software policies are isolated in compat-v2, including COP2 latch
state, selected FR0 aliases, cache/tag handling, CP0/TLB projection and bounded
unpopulated-data reads. Writes, fetches and mapped-device errors retain their
specified refusal boundaries. Strict and compat-v1 retain their identities.
The runner selects v2 explicitly; seals, inventories and real execution agree.

## Verification

The complete local native gate and all-target sanitizer build/owning selection
pass. After review found a scalar conversion destination defect, a meaningful
fetched-opcode regression established RED; the corrected normal and sanitizer
all-target builds and six owning checks pass. Earlier broad evidence retains
its pre-correction artifact identity. Independent profile/state vectors agree.

[Hosted run43476881467](https://github.com/szTheory/tendies64/actions/runs/43476881467)
passes the exact final source on Linux and macOS: full native gate, inventory,
core lint, authored evidence emission, publication scans and upload. No external
diagnostic input or result enters those jobs. Rights/PII/provenance scans pass.

The configured reviewer resolves its one finding after focused re-review.
The security auditor closes all three registered threats and confirms the
final correction. [Validation](04-20-VALIDATION.md), [RED evidence](04-20-RED-evidence.md),
[review](04-20-REVIEW.md), [fix proof](04-20-REVIEW-FIX.md),
[security](04-20-SECURITY.md) and [native evidence](../../../docs/evidence/phase04-native-cpu.md)
record exact scopes, fixture mistakes and tested identities.

## Threat Flags and limitations

No additional unregistered threat flag was identified. No risk acceptance or
hardware validation is claimed. Undefined-operation policies remain software
choices. Targeted attempts do not establish whole-set completeness; Plan16
owns the unchanged authoritative gate and private evidence. No new dependency,
execution-cap expansion, public diagnostic permission or hardware test occurred.

## Lessons

Distinguish conversion source selection from result width. Assert fetched PC,
opcode, retirement, neighbor sentinels and preserved upper words before
interpreting a mismatch. Fix fixture initialization at admission rather than
widening the modeled data windows. Serialize heavy sanitizer work and bounded
guest runs; reuse unaffected verification after a narrow reviewed correction.
