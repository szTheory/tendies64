---
phase: "06"
slug: "cold-boot-cpu-timing-and-full-base-suite"
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-11-15"
---

# Phase 06 — Validation Strategy

## Test Infrastructure

| Property | Value |
|---|---|
| Framework | CMake/CTest native C, shell/Python authored qualification controls |
| Config | `CMakePresets.json`, owning `CMakeLists.txt` |
| Quick check | Build all owning targets, then `ctest --preset gate --no-tests=error -R '<owning selection>'` |
| Full checks | Canonical gate and `ctest --preset asan --no-tests=error`, after all-target instrumented build |
| Runtime | Measure first actual selections; do not invent estimates or change guest caps |

## Sampling Rate

Run owning automated checks after substantive tasks; use canonical gates at
coherent source boundaries and before phase verification. Four-job presets
apply when no wall-capped diagnostic is active. Do not rebuild under CTest.
No three consecutive implementation tasks may lack automated verification.
Every plan must supply executable owning commands and explicit failure cases.

## Per-Task Verification Map

The planner fills exact task/plan/wave mappings once decomposition is written.

| Area | Requirement | Secure behavior | Test owners | Status |
|---|---|---|---|---|
| New profiles/state | BUS-03, CPU-08 | Existing identities frozen; complete future state | profile, digest, reset | pending |
| Cold protocol | BUS-03 | Reached independent sequences; atomic width/span/refusal | boot, bus, MI, new RDRAM controls | pending |
| T1 scheduling | CPU-08 | Literal costs; overflow preflight; Count/Random ordering | CPU time/CP0, scheduler, RSP/DMA | pending |
| Full BASE context | VER-02 | Independent denominator; original C results; full pre-use | source qualification, protocol/inventory/evidence controls | pending |
| Claims/lanes | VER-02 | No private leaks or unsupported claim; actual lane receipts | rights/PII, claims, authored hosted gate | pending |

## Wave 0 Requirements

Existing infrastructure is available; no framework installation. New test
owners and CTest registrations must precede their consuming implementation
checks. Accepted setup and per-case reached assertions prevent vacuous passes.
Planner must replace area rows with exact task-to-threat/test commands.

## Manual-Only Verifications

No new manual, physical or foreground check is authorized or required. External
rights/device blockers remain explicit private evidence limits. Release channel
remains an existing publication stop, not a stop on local phase work.

## Validation Sign-Off

- [ ] Exact per-task automated commands and phase-unique threat references
- [ ] Required new test owners registered; inherited inventories plus additions
- [ ] Complete pre-use and normal-dispatch mutation controls
- [ ] Actual bounded early cold startup and qualified member retained
- [ ] Final reviews, native gates and authored-only hosted evidence
- [ ] `nyquist_compliant: true` after actual validation

**Approval:** pending actual execution and coverage audit.
