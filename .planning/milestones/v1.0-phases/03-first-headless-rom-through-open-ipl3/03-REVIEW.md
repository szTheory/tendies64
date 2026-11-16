---
phase: 03-first-headless-rom-through-open-ipl3
reviewed: 2026-11-11T21:14:22Z
depth: deep
files_reviewed: 17
files_reviewed_list:
  - .github/workflows/ci.yml
  - core/t64_internal.h
  - core/t64_machine.c
  - core/t64_rom.c
  - core/tests/test_boot_entry.c
  - docs/evidence/phase3-ci.md
  - docs/spec/boot-limits.md
  - runner/t64_real.c
  - runner/t64_result_check.c
  - runner/t64_supervisor.c
  - runner/tests/test_fixture_descriptor.c
  - runner/tests/test_real_runner.c
  - runner/tests/test_result_v2.c
  - scripts/check-ci-yml.sh
  - scripts/collect-boot-evidence.sh
  - scripts/compare-boot-evidence.sh
  - scripts/gate.sh
findings:
  critical: 1
  warning: 1
  info: 0
  total: 2
  necessity: 0
status: issues_found
reviewed_through: 8eea81c35ab2d7069ea413997c3d9d36dbd99c2e
incremental_base: be85859cc23ca2066ae140865d9111db05d27647
scope_boundary: Phase 3 through plan 13 source; hosted evidence remains externally blocked
---

# Phase 03: Code Review Report

**Reviewed:** 2026-11-11T21:14:22Z

**Depth:** deep

**Files Reviewed:** 17

**Status:** issues_found

## Summary

This is the incremental review of the explicit 17-file delta from `be85859cc23ca2066ae140865d9111db05d27647` through `8eea81c35ab2d7069ea413997c3d9d36dbd99c2e`. Combined with the prior review, coverage includes the 43-file Phase 3 scope through plan 13. The prior CR-01 ROM admission ordering and CR-02 malformed-report crash were rechecked: both fixes and their regression coverage are present. Two new CI checker gaps remain: it accepts softened required-step failure handling and does not bind each named host job to its required runner. The current workflow has the intended job-to-runner mapping and default step failure behavior, but the checker will not catch regressions to those guarantees.

Hosted evidence remains unavailable: the candidate run is queued with zero jobs behind a stuck predecessor. This review makes no hosted-pair or hardware-accuracy claim; Phase 3 remains incomplete pending that external evidence.

## Critical Issues

### CR-03: CI checker accepts required steps that can fail without failing the job

**Classification:** BLOCKER

**File:** `scripts/check-ci-yml.sh:185`
**Issue:** The workflow validator records required step names and commands but does not validate step-level `continue-on-error` or skip conditions. A temporary mutation adding `continue-on-error: true` to the Gate step was accepted by the checker; the same mutation on the rights scan was also accepted. If Gate or a required scan fails softly, GitHub can still report the job successful and upload the evidence artifact. The collector selects successful jobs and can then treat evidence from that job as eligible despite a failed gate or scan.
**Fix:** Validate that Gate, evidence emission, rights scan, PII scan, and upload retain required success semantics. Reject `continue-on-error: true` and conditions that skip a required step; add checker self-tests that mutate each required step this way and expect rejection.

## Warnings

### WR-01: CI checker does not bind host job names to runner labels

**Classification:** WARNING

**File:** `scripts/check-ci-yml.sh:192-195`
**Issue:** The checker accepts either approved runner label for either job. A temporary mutation assigning `gate-macos` to `ubuntu-24.04` passed validation. The collector and comparator select records by job name, so a workflow regression could run both named jobs on the same host while preserving the labels used to compare them, weakening the intended cross-host evidence.
**Fix:** Require `gate-linux` to use `ubuntu-24.04` and `gate-macos` to use `macos-26`; add a mutation test for each wrong mapping.

---

_Reviewed: 2026-11-11T21:14:22Z_

_Reviewer: the agent (gsd-code-reviewer)_

_Depth: deep_
