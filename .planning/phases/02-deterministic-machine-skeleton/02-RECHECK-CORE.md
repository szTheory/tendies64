# Phase 2 Core Recheck — Iteration 1

**Verdict:** Runtime fixes verified; one warning remains in the profile-serialization regression test.

**Scope:** Core findings CR-01 and WR-01–WR-04, plus tests changed in `4f7ddc6..c739c25`. No broad source rediscovery.

## Verified fixes

- **CR-01:** Both digest APIs validate the full 32-byte output range against machine-owned storage and the admitted image before hashing or copying (`core/t64_digest.c:65-68`, `core/t64_machine.c:503-506`, `core/t64_internal.h:232-249`). The regression checks both APIs reject machine and image aliases and compares the affected owned bytes (`core/tests/test_digest_trace.c:103-148`).
- **WR-01:** PI delay is profile base ticks plus byte length divided by the nonzero profile divisor, with checked addition and scheduler tick-overflow handling (`core/t64_pi.c:79-92`, `core/t64_machine.c:124`). The custom 5/1 profile test proves a two-byte DMA stays busy through tick 6 and completes at tick 7 (`core/tests/test_pi_dma.c:223-277`).
- **WR-02:** Sequence limit handles 64 bits without shifting by 64, and reservation/commit enforce the profile limit (`core/t64_scheduler.c:22-26,109-114,154-188`). The 8-bit test accepts sequences 254 and 255, then verifies refusal leaves the digest, queue count, and counter unchanged; the existing 64-bit maximum case remains (`core/tests/test_scheduler.c:331-426`).
- **WR-03:** Multi-config generators are explicitly refused at configure time. An independent `Ninja Multi-Config` configure reached this guard and failed with its intended message; the current gate tree uses single-config Ninja with `CMAKE_BUILD_TYPE=Release`.
- **WR-04 implementation:** Serialization checks range overflow and source/output and output/output overlap before writing; query and too-small paths set the required length without serializing (`core/t64_machine.c:346-381`). Target tests cover null outputs, overflowing ranges, output overlap on success and too-small paths, required-length query, and unchanged sentinel bytes (`core/tests/test_profile.c:361-409`).

## Warning

### WR-05: Source-alias regression uses an invalid profile

**Severity:** WARNING  
**File:** `core/tests/test_profile.c:359,382-386`

**Issue:** The test clears `changed.pi_model_version[0]` at line 359, then passes that invalid descriptor as both the source and byte output at lines 382-386. `t64_profile_serialize` rejects it during `profile_fields_are_valid` before reaching the new source-overlap check. The test therefore still passes if the source-alias guard is removed, leaving the claimed source/bytes overlap regression uncovered.

**Fix:** Keep the invalid-descriptor check separate. Prove a fresh valid mutable profile copy serializes into a separate buffer successfully, then use that valid copy as both source and byte output; assert `T64_STATUS_INVALID_ARGUMENT` and that the profile bytes remain unchanged.

## Verification

- Focused CTest: `unit.core.profile`, `unit.core.scheduler`, `unit.core.digest-trace`, and `unit.core.pi-dma` passed (4/4).
- Existing full-gate and fuzz results are recorded in `02-REVIEW-FIX.md`. This bounded recheck did not rerun the full gate or full fuzz campaign.
