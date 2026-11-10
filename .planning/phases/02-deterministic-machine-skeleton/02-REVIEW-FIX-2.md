---
phase: 02-deterministic-machine-skeleton
fixed_at: "2026-11-10T23:08:43Z"
review_path: .planning/phases/02-deterministic-machine-skeleton/02-REVIEW.md
iteration: 2
findings_in_scope: 1
fixed: 1
skipped: 0
status: all_fixed
---

# Phase 2: Code Review Fix Report

**Fixed at:** 2026-11-10T23:08:43Z  
**Source review:** `.planning/phases/02-deterministic-machine-skeleton/02-REVIEW.md`  
**Iteration:** 2

**Summary:**
- Findings in scope: 1
- Fixed: 1
- Skipped: 0

## Fixed Issues

### WR-05: Source-alias regression uses an invalid profile

**Files modified:** `core/tests/test_profile.c`  
**Commit:** `05b298f`  
**Applied fix:** Kept the invalid-profile check separate. A fresh valid mutable descriptor now first serializes successfully to separate storage, then aliases the source with a byte array backed by the full union storage. The alias refusal must leave the complete backing bytes unchanged.

## Verification

All checks ran in the main checkout because `workflow.use_worktrees` is disabled.

- `cmake --preset asan` — configured successfully.
- `cmake --build --preset asan` — full build without a target restriction succeeded and rebuilt the component and runner executables, including `test_profile`.
- `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset asan -R '^(unit\.core\..*|unit\.model-result|sanitizer\.negative-control)$' --timeout 120 --output-on-failure` — 13/13 passed. This includes the refreshed profile, model-result, and sanitizer negative-control checks. The unchanged full fuzz campaign was excluded.
- The refreshed ASan profile executable contains build ID `t64-build-v1-43235b5150ec1f6bf5bb419365585e6e02e9b54e3c23ab2be7eef06b4cfe871f` and production source revision `c739c25df618bdac8f485e1859b40a8cd60278b6`. It was built after the test source edit and before the test-only commit; the normal build below contains the committed revision.
- `cmake --build --preset gate --target test_profile` followed by `ctest --preset gate -R '^unit\.core\.profile$' --timeout 120 --output-on-failure` — passed 1/1. The refreshed gate profile executable contains build ID `t64-build-v1-8d4783f27e47dfb906d39e73743d95a3709b163b47712cd1bc76ed7d7bcbc254` and source revision `05b298fdf682f572e3fcf6f2e5d292450acdaf62`.
- `./scripts/gate.sh` — passed all 73 normal tests and core lint reported zero violations. Inventory checks retained 73 normal and 75 sanitizer tests.

The iteration-1 report's 14-test ASan result overstated which executables were fresh: only the fuzz target had been rebuilt, while component and runner binaries were stale. This iteration replaces that component evidence with the full ASan build and 13-test refreshed selection; it does not claim the earlier 14-test selection was fresh. The prior current full fuzz result remains unchanged: 250,148 oracle calls in 69.10 seconds under its 120-second bound; it was not rerun.

---

_Fixed: 2026-11-10T23:08:43Z_  
_Fixer: the agent (gsd-code-fixer)_  
_Iteration: 2_
