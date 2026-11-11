---
phase: 03-first-headless-rom-through-open-ipl3
reviewed: 2026-11-11T18:11:01Z
depth: deep
files_reviewed: 43
files_reviewed_list:
  - core/CMakeLists.txt
  - core/include/tendies64/t64_machine.h
  - core/t64_bus.c
  - core/t64_cpu.c
  - core/t64_digest.c
  - core/t64_internal.h
  - core/t64_isviewer.c
  - core/t64_machine.c
  - core/t64_observe.c
  - core/t64_scheduler.c
  - core/t64_sp.c
  - core/t64_warm_devices.c
  - core/tests/test_boot_determinism.c
  - core/tests/test_boot_entry.c
  - core/tests/test_cpu_path.c
  - core/tests/test_isviewer.c
  - core/tests/test_tracer.c
  - core/tests/test_warm_devices.c
  - fixtures/boot-contract.json
  - fixtures/hello/hello.S
  - fixtures/hello/hello.ld
  - fixtures/manifest.json
  - runner/CMakeLists.txt
  - runner/t64_input.c
  - runner/t64_input.h
  - runner/t64_real.c
  - runner/t64_real_fixture.c
  - runner/t64_real_fixture.h
  - runner/t64_real_result.c
  - runner/t64_real_result.h
  - runner/t64_result_check.c
  - runner/t64_supervisor.c
  - runner/t64_verified_input.c
  - runner/t64_verified_input.h
  - runner/tests/test_fixture_descriptor.c
  - runner/tests/test_input.c
  - runner/tests/test_real_controls.c
  - runner/tests/test_real_runner.c
  - runner/tests/test_result_v2.c
  - scripts/build-boot-fixtures.sh
  - scripts/check-test-inventory.sh
  - scripts/gate.sh
  - tools/fixture/t64_rom_pack.c
findings:
  critical: 2
  warning: 0
  info: 0
  total: 2
  necessity: 0
status: issues_found
reviewed_through: be85859cc23ca2066ae140865d9111db05d27647
scope_boundary: Phase 3 through plan 12; plan 13 excluded
---

# Phase 03: Code Review Report

**Reviewed:** 2026-11-11T18:11:01Z
**Depth:** deep
**Files Reviewed:** 43
**Status:** issues_found

## Summary

Reviewed the explicit Phase 3 source scope through plan 12 at `be85859cc23ca2066ae140865d9111db05d27647`, including cross-file admission, result validation, capture, and process handling. Two verified blockers remain: warm ROM admission reads an overlapping input before refusing it, and the supervisor can crash on a parseable report missing `log_sha256`. Plan 13 was not reviewed. A bounded ASan reproduction confirmed the admission over-read; no broad test suite was run for this review.

## Critical Issues

### CR-01: Warm ROM admission hashes an overlapping span before range refusal

**Classification:** BLOCKER
**File:** `core/t64_machine.c:699`
**Issue:** For the warm profile, `t64_machine_admit_rom` calls `openipl3_asset_matches` before `t64_rom_candidate_prepare` validates address overflow and overlap with machine/image storage (`core/t64_rom.c:138-146`). A valid warm-profile machine plus an input pointer 64 bytes before the end of its machine allocation and a 4096-byte length makes the profile hash read past that known allocation. The bounded ASan reproduction reports a heap-buffer-overflow in SHA-256 instead of the promised pre-read refusal.
**Fix:** Validate the input length, checked address interval, and overlap with machine storage and the current image before any profile-specific byte inspection. Reuse or factor the read-free validation performed by `t64_rom_candidate_prepare` so both paths preserve the same refusal ordering.

### CR-02: Missing `log_sha256` crashes the supervisor during capture validation

**Classification:** BLOCKER
**File:** `runner/t64_supervisor.c:789`
**Issue:** `real_report_matches_capture` dereferences the result of `t64_json_get(root, "log_sha256")` without checking for `NULL`. A parseable child report can pass the preceding capture and identity checks while omitting this field. `real_run_mode` calls this helper even when the strict schema checker has already rejected the report (`runner/t64_supervisor.c:1416-1421`), so malformed report handling segfaults instead of failing closed and writing the supervisor result.
**Fix:** Check that the node exists before reading its type, and skip capture matching once schema validation fails. The malformed report should follow the existing report-invalid/error path and cleanup.

---

_Reviewed: 2026-11-11T18:11:01Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: deep_
