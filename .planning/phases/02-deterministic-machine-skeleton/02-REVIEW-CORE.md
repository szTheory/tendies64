---
phase: 02-deterministic-machine-skeleton
reviewed: 2026-11-10T22:26:12Z
depth: standard
files_reviewed: 25
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
  - tools/common/CMakeLists.txt
  - tools/lint/core-lint.sh
findings:
  critical: 1
  warning: 4
  info: 0
  total: 5
  necessity: 0
status: issues_found
---

# Phase 02: Code Review Report

**Reviewed:** 2026-11-10T22:26:12Z
**Depth:** standard
**Files Reviewed:** 25
**Status:** issues_found

## Summary

Reviewed the assigned production, component-test, build, and lint files against the machine ownership, profile, scheduler, DMA, trace, and digest contracts. Found one blocker and four warnings. The main risks are unchecked digest output aliasing that can corrupt a live machine, profile fields that do not control the behavior they identify, and a build fingerprint that conflates configurations under multi-config generators. Targeted tests were inspected; no source files were changed.

## Critical Issues

### CR01: Digest output can overwrite the live machine

**Classification:** BLOCKER
**File:** `core/t64_machine.c:486-496`; `core/t64_digest.c:57-68,142`
**Issue:** Both digest APIs only reject null pointers and an unadmitted machine. They do not reject output spans overlapping the machine or its admitted image. The machine allocation begins at the returned `t64_machine *` (`core/t64_machine.c:392-414`), so a caller can pass `(uint8_t *)machine` as the 32-byte output. The functions return success after overwriting the allocator and machine-storage fields at the start of the machine object; later calls or destruction can then use corrupted state or callback pointers.
**Fix:** Validate each complete 32-byte output range against machine-owned and image-owned storage before hashing or copying, and return `T64_STATUS_INVALID_ARGUMENT` without writing on overlap. Share the same checked-range helper used by the other observation APIs and add alias-refusal coverage for both digest functions.

## Warnings

### WR01: PI timing ignores the selected profile's duration fields

**Classification:** WARNING
**File:** `core/t64_pi.c:48-86` (especially line 82)
**Issue:** PI completion delay is hard-coded as `1 + length / 2`, even though each profile carries and hashes `pi_duration_base_ticks` and `pi_duration_bytes_divisor` (`core/t64_machine.c:44-45,201-202`). `t64_machine_create` accepts copied descriptors with other valid timing values, so a machine can report a profile identity that specifies different PI timing while still completing at the hard-coded tick. For example, a copied profile with base 5 and divisor 1 accepts a 2-byte request but still schedules it after 2 ticks instead of 7.
**Fix:** Derive the PI delay from the selected profile's base and divisor using checked 64-bit arithmetic, or reject timing configurations the implementation cannot honor. Add a test that changes those fields, creates the machine, starts a request, and checks its completion tick.

### WR02: Scheduler does not enforce profile sequence width

**Classification:** WARNING
**File:** `core/t64_scheduler.c:140-175` (especially lines 171-175)
**Issue:** The descriptor accepts `sequence_width_bits` from 1 through 64 (`core/t64_machine.c:121-122`) and includes it in profile identity, but commit marks sequence exhaustion only at `UINT64_MAX`. A valid 8-bit profile can therefore enqueue sequence 256 and continue, although its declared sequence range ends at 255. This makes exhaustion behavior disagree with the hashed profile.
**Fix:** Compute the maximum sequence value from `sequence_width_bits` (handling 64 bits without shifting by 64) and mark/refuse exhaustion at that profile-specific limit. Add a narrow-width profile test that advances through the boundary and confirms reservation refusal is transactional.

### WR03: Multi-config Debug and Release builds share one build ID

**Classification:** WARNING
**File:** `core/CMakeLists.txt:34-51`
**Issue:** The build manifest uses `CMAKE_BUILD_TYPE` as its active configuration, but that variable is empty for CMake multi-config generators such as Ninja Multi-Config, Xcode, and Visual Studio. Debug and Release artifacts produced from the same configured tree therefore embed the same `T64_BUILD_ID`, even though their effective compile flags and objects differ. Since build identity is emitted with native results, this can misidentify which artifact produced evidence.
**Fix:** Generate a configuration-specific manifest and ID for each active configuration, incorporating `$<CONFIG>` and its effective compile settings into the ID embedded in that configuration's core object.

### WR04: Profile serialization can overwrite its own output length buffer

**Classification:** WARNING
**File:** `core/t64_machine.c:346-368`
**Issue:** `t64_profile_serialize` does not check whether `length_out` overlaps `bytes_out`. With overlapping outputs, the buffer-too-small path writes the required length into bytes that its contract says remain unchanged; the success path writes the length after serialization and can corrupt the canonical byte stream while returning `T64_STATUS_OK`.
**Fix:** Before writing either output, validate the required serialized-byte range and the `length_out` range and reject overlap with `T64_STATUS_INVALID_ARGUMENT`. Also refuse output ranges that overlap the source profile so serialization cannot mutate its input. Add tests for overlap on both success and buffer-too-small paths.

---

_Reviewed: 2026-11-10T22:26:12Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: standard_
