---
phase: 04-complete-vr4300-cpu
reviewed: 2026-11-13T14:59:57Z
depth: deep
files_reviewed: 23
files_reviewed_list:
  - core/t64_cop1.c
  - core/t64_cp0.c
  - core/t64_cpu.c
  - core/t64_digest.c
  - core/t64_exception.c
  - core/t64_internal.h
  - core/t64_isviewer.c
  - core/t64_machine.c
  - core/t64_memory.c
  - core/tests/test_boot_entry.c
  - core/tests/test_cpu_cp0.c
  - core/tests/test_cpu_integer.c
  - core/tests/test_cpu_link.c
  - core/tests/test_profile.c
  - runner/t64_cpu_diagnostic.c
  - runner/tests/test_cpu_diagnostic.c
  - scripts/build-qualified-cpu-diagnostic.py
  - scripts/run-cpu-diagnostic.py
  - scripts/tests/test_cpu_diagnostic_run_controls.py
  - docs/spec/cpu-compatibility.md
  - docs/spec/cpu-memory.md
  - docs/spec/cpu-cp0.md
  - docs/spec/cpu-diagnostic.md
findings:
  critical: 0
  warning: 3
  info: 0
  total: 3
  necessity: 0
status: issues_found
---

# Phase 04 Plan 19: Code Review Report

**Reviewed:** 2026-11-13T14:59:57Z
**Depth:** deep
**Files Reviewed:** 23
**Status:** issues_found

## Summary

Reviewed the compatibility CPU implementation, profile/state identity and diagnostic selection paths against the phase plan and four CPU contracts. The strict 3/9 gates, compatibility 4/10 identity, v10 branch-origin digesting, and runtime profile checks appear consistent on source inspection. Three warning-level control gaps leave newly added edge behavior and the v2 profile-to-inventory threat without direct authored regression assertions. No builds or tests were rerun for this review.

## Warnings

### WR-01: WARNING — Nested likely-branch annul path has no regression control

**Severity:** WARNING
**File:** `core/tests/test_cpu_integer.c:1072-1117`
**Issue:** The new nested-branch control exercises only a non-likely JALR, and the displaced-slot fault control also uses JALR. Neither enters the new not-taken likely branch path at `core/t64_cpu.c:2249-2260`, which clears the pending outer target and resumes at the inner branch's PC+8. The existing likely-branch matrix covers branches outside a pending delay slot, so it cannot detect a regression in this compatibility-only transition.
**Fix:** Add nested likely-branch vectors for taken and not-taken outcomes. Assert the taken case executes the outer target as the inner delay instruction, and the not-taken case skips that instruction, clears pending-delay/origin state, and resumes at the inner branch's PC+8.

### WR-02: WARNING — Compatibility uncached LL/SC fault atomicity is untested

**Severity:** WARNING
**File:** `core/tests/test_cpu_link.c:1025-1093`
**Issue:** The new compatibility test covers only a successful uncached LL/SC pair. The existing refused-SC atomicity control at `core/tests/test_cpu_link.c:980-1023` uses the strict cacheable cartridge path. The newly admitted uncached bus path at `core/t64_memory.c:503-507` and its SC commit/early-return logic at `core/t64_memory.c:546-565` therefore have no direct regression proving a rejected uncached SC preserves the reservation, source/result register, backing bytes, and retirement state.
**Fix:** Add a compatibility-profile case that first establishes an uncached reservation, then makes the matching SC fail through the uncached bus path (for example, an uncached write to read-only cartridge backing). Assert the reservation, GPR, backing bytes, and non-retirement state remain unchanged.

### WR-03: WARNING — v2 compatibility seal control does not exercise inventory binding

**Severity:** WARNING
**File:** `scripts/tests/test_cpu_diagnostic_run_controls.py:240-259`
**Issue:** This test upgrades the strict fixture seal to v2/compat, but leaves its strict inventory and artifacts in place and calls only `adapter_json(..., "--runtime-identity-json")`. It verifies option selection and seal/context drift, but never exercises the profile-digest comparison in `scripts/run-cpu-diagnostic.py:549-559`, compatibility inventory validation, or aggregation under the selected profile. A regression that stopped binding the v2 selection to the inventory could still pass this control.
**Fix:** Add a v2 compatibility fixture whose inventory identity contains the compatibility profile digest and validate it through `load_qualified_inputs` and the production adapter. Include a negative case pairing the compatibility seal with the strict inventory (and a cross-profile batch set) and require refusal; retain a v1 seal control proving legacy selection remains strict.

---

_Reviewed: 2026-11-13T14:59:57Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: deep_

## Focused Re-review — Regression Controls

**Reviewed:** 2026-11-13T15:13:44Z
**Scope:** Follow-up assertions for WR-01, WR-02, and WR-03 only. No tests or builds were rerun.

### WR-01: WARNING — Nested likely-branch annul control still misses slot side effects

**Disposition:** Partially resolved. `core/tests/test_cpu_integer.c:1120-1195` now exercises nested BEQL taken and not-taken paths. The taken case checks the displaced outer target executes before the inner target. The not-taken case checks pending-delay/origin clearing and resumes at the inner branch's PC+8. However, the annulled instruction at PC+8 writes `gpr[12]`, and the test never asserts `gpr[12]` remains zero. An implementation that performs that annulled instruction's side effect while still setting the checked PC/state can pass these assertions.

**Severity:** WARNING
**Fix:** In the not-taken case, assert `machine->cpu.gpr[12] == 0u` after the BEQL step (and optionally assert exactly one instruction retired) to prove the annulled slot had no effect.

### WR-02: WARNING — Compatibility uncached LL/SC fault atomicity is tested

**Disposition:** Resolved by the follow-up control at `core/tests/test_cpu_link.c:1095-1153`. It establishes an uncached LL reservation on read-only cartridge backing, then checks the refused SC preserves the linked physical address and width, source/result register, retirement count, and backing bytes. These assertions cover the atomicity state named in the original warning.

### WR-03: WARNING — v2 profile-to-inventory binding is tested through load and aggregation

**Disposition:** Resolved by `scripts/tests/test_cpu_diagnostic_run_controls.py:261-354`. The control loads a v2 compatibility seal with an inventory whose identity uses the real compatibility adapter's profile digest, encodes every registration through the real C adapter, and verifies the complete set. It then checks that a strict-profile batch is rejected by compatibility aggregation and that a strict-profile inventory re-sealed under compatibility is rejected by `load_qualified_inputs`. The test stubs only the separate upstream qualification verifier; adapter identity, encoding, and aggregation calls remain real. Existing v1 strict-fixture controls continue to exercise the legacy strict selection path.

The original findings and frontmatter counts above are retained as the initial review record. WR-01 remains open pending the direct annulled-slot side-effect assertion; WR-02 and WR-03 are closed by the cited controls.

## Final orchestrator disposition

Added a direct `gpr[12] == 0` assertion immediately after the not-taken nested
BEQL, plus an exact two-retired-branches assertion. The owning CPU control
test passed in both normal and instrumented builds after that change.
WR-01 is resolved by those explicit side-effect and retirement assertions.
WR-02 and WR-03 retain the independent re-review dispositions above. Current
open findings: zero. This last narrow assertion correction was verified by
the orchestrator; no further independent re-review is claimed.
