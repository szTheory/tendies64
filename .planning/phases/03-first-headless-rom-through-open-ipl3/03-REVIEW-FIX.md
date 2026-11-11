---
phase: 03-first-headless-rom-through-open-ipl3
fixed_at: 2026-11-11T18:28:03Z
review_path: .planning/phases/03-first-headless-rom-through-open-ipl3/03-REVIEW.md
iteration: 1
findings_in_scope: 2
fixed: 2
skipped: 0
status: all_fixed
---

# Phase 03: Code Review Fix Report

**Fixed at:** 2026-11-11T18:28:03Z
**Source review:** `.planning/phases/03-first-headless-rom-through-open-ipl3/03-REVIEW.md`
**Iteration:** 1

**Summary:**
- Findings in scope: 2
- Fixed: 2
- Skipped: 0

## Fixed Issues

### CR-01: Warm ROM admission hashes an overlapping span before range refusal

**Files modified:** `core/t64_internal.h`, `core/t64_machine.c`, `core/t64_rom.c`, `core/tests/test_boot_entry.c`
**Commit:** `4ee0283`
**Applied fix:** Factored the candidate's length, alignment, checked interval, and machine/current-image overlap checks into a read-free ROM input preflight. Warm admission now runs it before checking the raw bytes against the profile's qualification hashes, and candidate preparation reuses the same checks. Regression cases cover the RDRAM allocation tail, pointer interval overflow, and current-image overlap and tail; each verifies unchanged machine storage and allocator ownership. Raw-byte profile qualification remains in place.

### CR-02: Missing `log_sha256` crashes the supervisor during capture validation

**Files modified:** `runner/t64_real.c`, `runner/t64_supervisor.c`, `runner/tests/test_real_runner.c`
**Commit:** `ba08b27`
**Applied fix:** Capture matching now runs only after strict schema validation succeeds. The matcher also checks that `log_sha256` exists and is either the supported null case or a string before using it. A test-only producer fault removes that key from an otherwise valid report produced by the actual hello guest path; the integration test verifies canonical `harness-error/report-invalid` output and workset cleanup. Process and capture precedence are unchanged.

## Verification

**Location:** Main checkout (`workflow.use_worktrees=false`).

- CR-01 pre-fix regression: ASan reported a heap-buffer-overflow in warm asset SHA-256 at the end of the valid machine/RDRAM allocation. The test was adjusted to snapshot owned storage because machine digest is unavailable before first admission, then rerun to the same intended ASan failure.
- CR-01 after fix: `unit.core.boot-entry` passed in both gate and ASan/UBSan builds. The ASan run used `ASAN_OPTIONS=detect_leaks=0`.
- CR-02 pre-fix regression: the actual-path supervisor exited 139 on a parseable child report missing `log_sha256`; the strict child checker rejected it and the supervisor workset remained.
- CR-02 after fix: `integration.open-ipl3-hello` passed in gate and ASan/UBSan builds. The sanitizer run used `ASAN_OPTIONS=detect_leaks=0` and exercised cleanup.
- Final committed tree: `cmake --build --preset gate && ctest --preset gate` passed all 91 tests.

---

_Fixed: 2026-11-11T18:28:03Z_
_Fixer: the agent (gsd-code-fixer)_
_Iteration: 1_
