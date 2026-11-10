---
phase: 02-deterministic-machine-skeleton
reviewed: "2026-11-10"
depth: standard
files_reviewed: 35
files_reviewed_list:
  - CMakeLists.txt
  - core/CMakeLists.txt
  - core/include/tendies64/t64_machine.h
  - core/t64_bus.c
  - core/t64_digest.c
  - core/t64_internal.h
  - core/t64_machine.c
  - core/t64_mi.c
  - core/t64_pi.c
  - core/t64_rom.c
  - core/t64_scheduler.c
  - core/t64_sp.c
  - core/t64_trace.c
  - core/tests/fuzz_rom_admission.c
  - core/tests/sanitizer_fault.c
  - core/tests/test_bus.c
  - core/tests/test_determinism.c
  - core/tests/test_digest_trace.c
  - core/tests/test_lifecycle.c
  - core/tests/test_pi_dma.c
  - core/tests/test_profile.c
  - core/tests/test_rom_admission.c
  - core/tests/test_scheduler.c
  - core/tests/test_sp_dma.c
  - core/tests/test_tracer.c
  - runner/CMakeLists.txt
  - runner/t64_model_result_check.c
  - runner/t64_skeleton_probe.c
  - runner/tests/test_model_result.c
  - runner/tests/test_skeleton_probe.c
  - scripts/check-test-inventory.sh
  - scripts/expect-sanitizer.sh
  - scripts/gate.sh
  - tools/common/CMakeLists.txt
  - tools/lint/core-lint.sh
findings:
  critical: 1
  warning: 7
  info: 0
  total: 8
  necessity: 0
status: issues_found
---

# Phase 2: Consolidated Code Review

Independent core and host reviews of the phase diff from the Phase 1 close. Original reports retain scoped evidence. No risk is accepted; all eight actionable findings enter the authorized fix loop.

## Critical Issues

### CR-01: Digest output can overwrite the live machine

**Classification:** BLOCKER
**File:** `core/t64_machine.c:486-496`; `core/t64_digest.c:57-68,142`
**Issue:** Both digest APIs only reject null pointers and an unadmitted machine. They do not reject output spans overlapping the machine or its admitted image. The machine allocation begins at the returned `t64_machine *` (`core/t64_machine.c:392-414`), so a caller can pass `(uint8_t *)machine` as the 32-byte output. The functions return success after overwriting the allocator and machine-storage fields at the start of the machine object; later calls or destruction can then use corrupted state or callback pointers.
**Fix:** Validate each complete 32-byte output range against machine-owned and image-owned storage before hashing or copying, and return `T64_STATUS_INVALID_ARGUMENT` without writing on overlap. Share the same checked-range helper used by the other observation APIs and add alias-refusal coverage for both digest functions.

## Warnings

### WR-01: PI timing ignores the selected profile's duration fields

**Classification:** WARNING
**File:** `core/t64_pi.c:48-86` (especially line 82)
**Issue:** PI completion delay is hard-coded as `1 + length / 2`, even though each profile carries and hashes `pi_duration_base_ticks` and `pi_duration_bytes_divisor` (`core/t64_machine.c:44-45,201-202`). `t64_machine_create` accepts copied descriptors with other valid timing values, so a machine can report a profile identity that specifies different PI timing while still completing at the hard-coded tick. For example, a copied profile with base 5 and divisor 1 accepts a 2-byte request but still schedules it after 2 ticks instead of 7.
**Fix:** Derive the PI delay from the selected profile's base and divisor using checked 64-bit arithmetic, or reject timing configurations the implementation cannot honor. Add a test that changes those fields, creates the machine, starts a request, and checks its completion tick.

### WR-02: Scheduler does not enforce profile sequence width

**Classification:** WARNING
**File:** `core/t64_scheduler.c:140-175` (especially lines 171-175)
**Issue:** The descriptor accepts `sequence_width_bits` from 1 through 64 (`core/t64_machine.c:121-122`) and includes it in profile identity, but commit marks sequence exhaustion only at `UINT64_MAX`. A valid 8-bit profile can therefore enqueue sequence 256 and continue, although its declared sequence range ends at 255. This makes exhaustion behavior disagree with the hashed profile.
**Fix:** Compute the maximum sequence value from `sequence_width_bits` (handling 64 bits without shifting by 64) and mark/refuse exhaustion at that profile-specific limit. Add a narrow-width profile test that advances through the boundary and confirms reservation refusal is transactional.

### WR-03: Multi-config Debug and Release builds share one build ID

**Classification:** WARNING
**File:** `core/CMakeLists.txt:34-51`
**Issue:** The build manifest uses `CMAKE_BUILD_TYPE` as its active configuration, but that variable is empty for CMake multi-config generators such as Ninja Multi-Config, Xcode, and Visual Studio. Debug and Release artifacts produced from the same configured tree therefore embed the same `T64_BUILD_ID`, even though their effective compile flags and objects differ. Since build identity is emitted with native results, this can misidentify which artifact produced evidence.
**Fix:** Generate a configuration-specific manifest and ID for each active configuration, incorporating `$<CONFIG>` and its effective compile settings into the ID embedded in that configuration's core object.

### WR-04: Profile serialization can overwrite its own output length buffer

**Classification:** WARNING
**File:** `core/t64_machine.c:346-368`
**Issue:** `t64_profile_serialize` does not check whether `length_out` overlaps `bytes_out`. With overlapping outputs, the buffer-too-small path writes the required length into bytes that its contract says remain unchanged; the success path writes the length after serialization and can corrupt the canonical byte stream while returning `T64_STATUS_OK`.
**Fix:** Before writing either output, validate the required serialized-byte range and the `length_out` range and reject overlap with `T64_STATUS_INVALID_ARGUMENT`. Also refuse output ranges that overlap the source profile so serialization cannot mutate its input. Add tests for overlap on both success and buffer-too-small paths.


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


## Review provenance

- Core: 02-REVIEW-CORE.md (critical and warnings 01–04).
- Host/tooling: 02-REVIEW-HOST.md (warnings 50–52).
- Reviewed production source is unchanged since final local gate; review reports and state metadata are additional artifacts.
