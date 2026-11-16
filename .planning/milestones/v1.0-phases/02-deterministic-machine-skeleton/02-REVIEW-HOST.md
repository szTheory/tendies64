---
phase: 02-deterministic-machine-skeleton
reviewed: 2026-11-10T22:23:32Z
depth: standard
files_reviewed: 10
files_reviewed_list:
  - core/tests/fuzz_rom_admission.c
  - core/tests/sanitizer_fault.c
  - runner/CMakeLists.txt
  - runner/t64_model_result_check.c
  - runner/t64_skeleton_probe.c
  - runner/tests/test_model_result.c
  - runner/tests/test_skeleton_probe.c
  - scripts/check-test-inventory.sh
  - scripts/expect-sanitizer.sh
  - scripts/gate.sh
findings:
  critical: 0
  warning: 3
  info: 0
  total: 3
  necessity: 0
status: issues_found
---

# Phase 2: Host Harness Code Review

**Reviewed:** 2026-11-10T22:23:32Z
**Depth:** standard
**Files Reviewed:** 10
**Status:** issues_found

## Summary

Reviewed the assigned fuzzing, native model-result, probe, sanitizer-control, and test-inventory files with their connected production admission API and JSON helpers. The model/guest evidence boundary and canonical schema checks are enforced. Three robustness defects remain: special-file paths can block the model tools, two new CTest wrappers have no hang bound, and the admission fuzzer can miss a refusal that shrinks its captured machine-storage capacity.

## Warnings

### WR-50: Special-file paths can block the model tools

**Severity:** WARNING
**File:** `runner/t64_model_result_check.c:76-83`; `runner/t64_skeleton_probe.c:422-436`
**Issue:** `read_bounded_file` calls blocking `open(path, O_RDONLY)` before checking `S_ISREG`. Opening a FIFO for reading waits indefinitely for a writer, so the subsequent regular-file check is never reached. The probe's `--out` path similarly uses blocking `O_WRONLY`; a FIFO with no reader hangs, while other writable special files can make the probe report success without producing an output file. I reproduced both paths blocking on FIFOs with the current gate binaries.
**Fix:** Open paths in nonblocking mode, then `fstat` and reject anything that is not a regular file before reading or writing. Keep `O_NOFOLLOW` on the output path and apply it to input if symlinks are not part of the input contract.

### WR-51: New subprocess tests have no hang bound

**Severity:** WARNING
**File:** `runner/tests/test_skeleton_probe.c:25-81`; `runner/tests/test_model_result.c:109-138`; `runner/CMakeLists.txt:50-74`
**Issue:** Both test helpers call blocking `waitpid(child, ..., 0)` with no deadline, and neither new CTest registration sets a `TIMEOUT`. A hang in the probe, model checker, result checker, or claims script therefore leaves the public gate waiting indefinitely instead of reporting a bounded test failure.
**Fix:** Add finite CTest `TIMEOUT` properties for both registrations and make the subprocess helpers enforce a monotonic deadline that kills and reaps a timed-out child (including its process group when it can spawn descendants).

### WR-52: Refusal snapshot can silently shrink

**Severity:** WARNING
**File:** `core/tests/fuzz_rom_admission.c:330-357`
**Issue:** The refusal oracle only rejects a machine-storage capacity that grows past the saved size (line 332), then compares using the current capacity (line 357). If a refusal accidentally changes that capacity to zero or a small value, the check can compare no bytes or only a prefix and report the owned state unchanged. Most seeded mutation refusals also skip the direct digest comparison, so this can conceal a real refusal-side mutation despite the full-state snapshot claim.
**Fix:** Require `machine->machine_storage.capacity == machine_snapshot_capacity` before comparing, record a failure on any mismatch, and never use a smaller current capacity as the snapshot comparison length.

---

_Reviewed: 2026-11-10T22:23:32Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: standard_
