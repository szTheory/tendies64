---
phase: 07-rdp-reference-renderer-and-vi-scanout
reviewed: 2026-11-17T09:12:06Z
review_type: focused_recheck
prior_review: 07-05-REVIEW-INITIAL.md
rechecked_findings: 4
resolved: 4
remaining: 0
new_findings: 0
status: resolved
---

# Plan 07-05 Review Recheck

Rechecked only CR-01, CR-02, WR-01, and WR-02 from the initial review against the repair diff from `2a04371943a25f3e6ec8985bc77caa76ed08e631`. The initial review artifact remains unchanged.

## Finding dispositions

- **CR-01 — RESOLVED.** Triangle preflight decodes the primitive tile and validates it at `core/t64_rdp_raster.c:235-255`. LOD selection now starts from that tile and uses the header's relative maximum level (`:422-468`); raster sampling uses the selected tile (`:567-584`). The bounds check rejects a relative chain extending past tile 7 before effects. This follows the documented W0 tile and level fields and the project contract. Tests cover nonzero primitive tiles, a different tile-0 decoy, level-0 versus level-1 selection, and tile-7-plus-level-1 refusal with pixel and coverage snapshots (`core/tests/test_rdp_texture.c:874-949`).
- **CR-02 — RESOLVED.** Box and three-point alpha now use floor division like RGB (`core/t64_rdp_tmem.c:908-954`), matching the software contract in `docs/spec/rdp.md:197-203`. New controls exercise all 16 alpha patterns for box and three-point weights on the diagonal and both triangle halves (`core/tests/test_rdp_texture.c:953-987`).
- **WR-01 — RESOLVED.** The new independent coefficient fixture supplies nonzero signed S/T/W bases and x/edge/y derivatives to both 0x0A and 0x0E, with a nonuniform tile and four expected pixels (`core/tests/test_rdp_texture.c:989-1035`). The parent-provided Fraction check confirms each derivative-lane zero mutation changes an expected sample.
- **WR-02 — RESOLVED.** W-zero and excessive-derivative packets now assert `RENDER_RANGE`, the consumed command pointer, unchanged seeded framebuffer bytes, and unchanged full coverage plane after prior accepted work (`core/tests/test_rdp_texture.c:607-663`). The out-of-range tile chain case has the same no-effect assertions (`:899-918`).

## Repair review

The tile additions stay within the existing machine state and profile; no new serialization or reset state is needed. The preflight range expression permits tile 0 through 7 at the exact boundary and rejects a chain whose last declared descriptor would exceed 7. The row renderer already retains the pending triangle words for interpolation, and clears them only after the final row, so reading the packet header during LOD selection uses the active command. I found no new correctness issue in the focused repair diff.

The parent reports that the 20 registered texture cases pass in normal and ASAN builds, the source/object guards and mutations pass, the affected C units compile under the existing warning policy, and the full gate passes. I did not rerun suites; retained build evidence is recorded in the phase summary.

---

_Rechecked: 2026-11-17T09:12:06Z_  
_Scope: four initial findings only_
