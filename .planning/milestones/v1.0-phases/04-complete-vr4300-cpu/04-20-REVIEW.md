---
phase: 04-complete-vr4300-cpu
reviewed: 2026-11-13T17:09:31Z
depth: standard
files_reviewed: 31
files_reviewed_list:
  - core/t64_bus.c
  - core/t64_cache.c
  - core/t64_cop1.c
  - core/t64_cp0.c
  - core/t64_cpu.c
  - core/t64_digest.c
  - core/t64_internal.h
  - core/t64_isviewer.c
  - core/t64_machine.c
  - core/t64_memory.c
  - core/t64_mmu.c
  - core/tests/test_boot_entry.c
  - core/tests/test_cop1.c
  - core/tests/test_cpu_cache.c
  - core/tests/test_cpu_cp0.c
  - core/tests/test_cpu_integer.c
  - core/tests/test_cpu_memory.c
  - core/tests/test_cpu_mmu.c
  - core/tests/test_cpu_path.c
  - core/tests/test_profile.c
  - docs/spec/cpu-cache.md
  - docs/spec/cpu-compatibility.md
  - docs/spec/cpu-cop1.md
  - docs/spec/cpu-cp0.md
  - docs/spec/cpu-diagnostic.md
  - docs/spec/cpu-memory.md
  - runner/t64_cpu_diagnostic.c
  - runner/tests/test_cpu_diagnostic.c
  - scripts/build-qualified-cpu-diagnostic.py
  - scripts/run-cpu-diagnostic.py
  - scripts/tests/test_cpu_diagnostic_run_controls.py
findings:
  critical: 0
  warning: 1
  info: 0
  total: 1
  necessity: 0
status: issues_found
---

# Phase 04 Plan 20: Code Review Report

**Reviewed:** 2026-11-13T17:09:31Z
**Depth:** standard
**Files Reviewed:** 31
**Status:** issues_found

## Summary

Reviewed the Plan20 source, tests, runner/scripts and specification delta, tracing the new profile policy through COP1, CP0/MMU, cache and memory paths and the diagnostic profile selection. One warning remains in the v2 FR0 alias scope: all conversion instructions currently receive odd-index aliasing, including scalar-only conversions outside the documented selected paths. No tests or builds were run for this review.

## Warnings

### WR-01: V2 FR0 aliasing applies to every conversion

**File:** `core/t64_cop1.c:456-459`
**Severity:** WARNING

**Issue:** `compat_fr0_odd_alias` is enabled whenever `conversion` is true. That flag is then passed to every source read and used to rewrite every destination (`core/t64_cop1.c:491-492`, `714-716`). The conversion predicate includes scalar-width forms such as CVT.W.S (format S, function `0x24`) and CVT.S.W (format W, function `0x20`), so an odd scalar source or destination is silently redirected to the even register. The documented v2 contract limits aliases to covered double-format paths and selected 32-bit operands whose conversion path uses paired register selection (`docs/spec/cpu-cop1.md:149-153`). This broad predicate changes unrelated scalar conversion behavior and is not covered by the added odd ADD.D, CVT.D.L and CVT.D.S controls.

**Fix:** Select aliasing per operand and conversion path: normalize odd indices for covered FR0 paired 64-bit operations, and only for the specifically selected 32-bit conversion operands that use paired selection. Leave scalar-only conversion source and destination indices untouched. Add v2 controls for an odd scalar source and destination outside the selected set, while retaining the existing paired-path alias controls.

---

_Reviewed: 2026-11-13T17:09:31Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_

## Re-review disposition

**WR-01: Resolved.** The clarified contract intentionally retains odd-source
aliasing for covered conversions, including the selected scalar forms; it
normalizes destinations only when the result occupies a 64-bit FR0 pair. The
original finding and its history above are preserved.

The updated gates match that contract: the regular conversion path normalizes
`fd` only when `destination_width == 64` (`core/t64_cop1.c:715-718`), and the
MOV path does so only when `source_width == 64` (`core/t64_cop1.c:497-500`).
The new CVT.S.W control exercises an odd source and odd 32-bit destination and
asserts the fetched PC/opcode and retirement, preserves the even-neighbor
sentinel, and writes the requested odd destination while retaining its upper
word (`core/tests/test_cop1.c:831-856`). The COP1 spec and phase context now
state the same source-alias and destination-width rule
(`docs/spec/cpu-cop1.md:149-155`, `04-20-CONTEXT.md:16`).

No remaining defect was found in this bounded re-review. Tests and builds were
not rerun.
