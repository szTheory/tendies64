---
phase: 02-deterministic-machine-skeleton
fixed_at: "2026-11-10T22:51:30Z"
review_path: .planning/phases/02-deterministic-machine-skeleton/02-REVIEW.md
iteration: 1
findings_in_scope: 8
fixed: 8
skipped: 0
status: all_fixed
---

# Phase 2: Code Review Fix Report

**Fixed at:** 2026-11-10T22:51:30Z  
**Source review:** `.planning/phases/02-deterministic-machine-skeleton/02-REVIEW.md`  
**Iteration:** 1

**Summary:**
- Findings in scope: 8
- Fixed: 8
- Skipped: 0

## Fixed Issues

### CR-01: Digest output can overwrite the live machine

**Files modified:** `core/t64_digest.c`, `core/t64_internal.h`, `core/t64_machine.c`, `core/tests/test_digest_trace.c`  
**Commit:** `fb7dc2c`  
**Applied fix:** Shared the checked machine/image output-span validator with both digest APIs. Added machine and image alias refusals that verify owned bytes remain unchanged.  
**Focused check:** `unit.core.digest-trace` passed.

### WR-01: PI timing ignores the selected profile's duration fields

**Files modified:** `core/t64_pi.c`, `core/tests/test_pi_dma.c`  
**Commit:** `21771af`  
**Applied fix:** PI deadlines now use the profile base and byte divisor with checked 64-bit addition; the named profile's tick-9 completion remains covered. Added a copied-profile tick-7 regression.  
**Focused check:** `unit.core.pi-dma` passed.

### WR-02: Scheduler does not enforce profile sequence width

**Files modified:** `core/t64_scheduler.c`, `core/tests/test_scheduler.c`  
**Commit:** `f3016e3`  
**Applied fix:** Reservation and commit enforce the configured sequence maximum, with a 64-bit-safe maximum calculation. Added 8-bit boundary and transactional-refusal coverage.  
**Focused check:** `unit.core.scheduler` passed.

### WR-03: Multi-config Debug and Release builds share one build ID

**Files modified:** `core/CMakeLists.txt`  
**Commit:** `ca21039`  
**Applied fix:** CMake now fails configuration with guidance when a multi-config generator is selected.  
**Focused check:** Single-config configure passed; the Ninja Multi-Config negative control failed at the new guard as expected.

### WR-04: Profile serialization can overwrite its own output length buffer

**Files modified:** `core/t64_machine.c`, `core/include/tendies64/t64_machine.h`, `core/tests/test_profile.c`  
**Commit:** `c5e35a6`  
**Applied fix:** Serialization validates source, byte-output, and length-output ranges before writing. Added success, short-buffer, source-alias, null, and overflow controls while retaining the required-length query behavior.  
**Focused check:** `unit.core.profile` passed.

### WR-50: Special-file paths can block the model tools

**Files modified:** `runner/t64_model_result_check.c`, `runner/t64_skeleton_probe.c`, `runner/tests/test_model_result.c`, `runner/tests/test_skeleton_probe.c`  
**Commit:** `1012401`  
**Applied fix:** Model input opens are nonblocking and reject non-regular files after `fstat`; probe outputs are nonblocking, no-follow, checked as regular files before truncation or writing. Added FIFO input and output regressions.  
**Focused checks:** `unit.model-result` and `unit.core.profile-result` passed.

### WR-51: New subprocess tests have no hang bound

**Files modified:** `runner/tests/test_model_result.c`, `runner/tests/test_skeleton_probe.c`, `runner/CMakeLists.txt`  
**Commit:** `38aef57`  
**Applied fix:** Test subprocess helpers use monotonic deadlines, separate process groups, and kill/reap cleanup; both CTest registrations have finite 60-second timeouts. Existing cases now exercise a hanging child with a descendant.  
**Focused checks:** `unit.model-result` and `unit.core.profile-result` passed.

### WR-52: Refusal snapshot can silently shrink

**Files modified:** `core/tests/fuzz_rom_admission.c`  
**Commit:** `c739c25`  
**Applied fix:** Refusal checks require the saved machine/image pointers and exact capacities before comparing the saved full spans. Zero-capacity and shrunken-capacity controls confirm the oracle rejects changed metadata.  
**Focused check:** ASan/UBSan fuzz sample passed with one mutation and both capacity controls.

## Verification

All checks ran in the main checkout because `workflow.use_worktrees` is disabled. The final normal gate passed all 73 registered tests; its inventory self-check retained the expected 73 normal and 75 sanitizer entries, and core lint reported no violations. The final ASan/UBSan run passed all 14 selected component, model-result, full fuzz, and negative-control tests. The full 250,000-mutation campaign completed in 69.10 seconds within its existing 120-second bound. Leak detection was disabled for this host; address and undefined-behavior sanitizers remained active.

---

_Fixed: 2026-11-10T22:51:30Z_  
_Fixer: the agent (gsd-code-fixer)_  
_Iteration: 1_
