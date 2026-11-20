---
phase: 07-rdp-reference-renderer-and-vi-scanout
reviewed: 2026-11-17T13:11:59Z
review_type: focused_recheck
diff_base: 29bc425089f39b7f413ca599df9a5a36add4ae54
files_reviewed: 7
files_reviewed_list:
  - core/t64_rdp.c
  - core/t64_rdp_raster.c
  - core/tests/test_rdp_combiner.c
  - docs/spec/rdp.md
  - .planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-06-RAW-CONTRACT.md
  - docs/spec/rdp-vi-cases.json
  - scripts/check-rsp-no-hle.py
rechecked_findings: 6
resolved: 6
new_findings: 0
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
status: clean
---

# Phase 07-06: Focused Review Recheck

**Reviewed:** 2026-11-17T13:11:59Z  
**Scope:** The five findings in `07-06-REVIEW-INITIAL.md` and the additional primary-register finding in `07-06-REVIEW-ADDITIONAL.md`, against the current worktree diff from `29bc425089f39b7f413ca599df9a5a36add4ae54`.

## Summary

All six findings are resolved in the current source, documentation, and registered regression cases. I found no new defect in the repair diff. Parent-provided normal, focused ASAN/UBSan, source/object guard, and compile results cover the new cases; I did not rerun suites.

## Finding Dispositions

### CR-01: V5 triangles without SetCombine silently use the legacy renderer — RESOLVED

`apply_pending_command` now refuses a v5 triangle lacking `combine_mode_valid` before bounds admission and scheduling (`core/t64_rdp.c:402-411`). All v5 covered triangle pixels enter the combiner branch; the prior legacy fallback is no longer conditional on a latched mode (`core/t64_rdp_raster.c:565-619`). `case_missing_combine_refuses_after_prior_fill` reaches the refusal after a successful fill and checks the refused triangle leaves the full framebuffer snapshot, complete coverage plane, and RNG unchanged while preserving the earlier fill (`core/tests/test_rdp_combiner.c:285-347`). `set_refusal` clears busy and pending-command state, so the D-04 stop-at-encountered-command path remains restartable (`core/t64_rdp.c:69-88`; DP_START checks the cleared busy state at `:622-633`).

### WR-01: Selector denominator checks do not cover cycle 1 — RESOLVED

`case_cycle1_selector_denominator` now exercises every encoded cycle-1 RGB A/B/C/D and alpha A/B/C/D value, including zero aliases, against distinct fixed inputs and expected values (`core/tests/test_rdp_combiner.c:605-728`). The new case is in the real case dispatcher and inventory (`core/tests/test_rdp_combiner.c:1172-1173`, `docs/spec/rdp-vi-cases.json:185`).

### WR-02: No accepted fetched textured triangle reaches the new combiner inputs — RESOLVED

`case_fetched_rgba32_texture_combiner` seeds two authored RGBA32 textures, fetches tile configuration and SetCombine through DPC, submits a textured `0x0A` packet, and asserts exact RDRAM and VI bytes after acceptance (`core/tests/test_rdp_combiner.c:900-1003`). The literal selects Texel0, Texel1, and their alpha channels across two cycles. `case_fetched_convert_signed_boundaries` fetches SetConvert packets before selecting K4/K5 (`:1085-1138`). Both cases are registered (`docs/spec/rdp-vi-cases.json:188-189`).

### WR-03: Noise tests do not establish accepted raster-pixel cadence — RESOLVED

`case_fetched_noise_triangle_cadence` submits an accepted two-cycle edge triangle with RGB A selecting Noise in both cycles and scissoring it to one covered pixel. It checks exact RDRAM output, VI output, complete coverage count, and final RNG state. A second accepted triangle fully misses the scissor and preserves framebuffer, coverage, and RNG snapshots (`core/tests/test_rdp_combiner.c:1006-1083`). The new case is registered at `docs/spec/rdp-vi-cases.json:190`.

### WR-04: K4/K5 signed-to-byte conversion is not stated or tested at negative inputs — RESOLVED

The RDP specification now states the reversible policy: sign-extend nine bits and clamp to 0–255 (`docs/spec/rdp.md:339-342`), also recorded in the raw contract. Fetched SetConvert tests assert coefficient values and independent K4/K5 output at -256, -1, 0, 1, and 255 (`core/tests/test_rdp_combiner.c:1085-1138`).

### Additional primary-register finding: SetPrimColor primitive LOD field — RESOLVED

The decoder now reads primitive LOD fraction from raw bits 39:32 (`core/t64_rdp.c:494-499`), matching the SDK producer field documented in the raw contract. The fetched literal `0xFA0012A7636E7984` has adjacent bytes `0x12` and `0xA7`; the test checks both positions, the decoded fraction 167, and its effect on combiner output (`core/tests/test_rdp_combiner.c:843-875`). The prior incorrect bit-position claim has been corrected in `docs/spec/rdp.md` and `07-06-RAW-CONTRACT.md`.

## Repair Regression Check

The no-SetCombine refusal calls the shared `set_refusal` path. That path clears `busy`, `command_pending`, and `pending_triangle_word_count` while retaining the earlier accepted work and the refused command location. A subsequent DP_START is therefore admitted, consistent with D-04’s stopped-command policy. I found no repair-caused issue.

---

_Reviewer: the agent (gsd-code-reviewer)_  
_Review type: focused recheck_
