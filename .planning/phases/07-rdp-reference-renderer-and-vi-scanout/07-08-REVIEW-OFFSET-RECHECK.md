---
phase: 07-rdp-reference-renderer-and-vi-scanout
reviewed: 2026-11-17T17:52:48Z
review_type: focused_production_recheck
finding: WR-01-production
severity: blocker
disposition: fixed
status: integration-test-pending
---

# Plan 07-08 Texture Offset Recheck

**Reviewed:** 2026-11-17T17:52:48Z

## Disposition

The newly reported production defect is confirmed and fixed in the current source. The texture bounds preflight and runtime sampler now both select coefficient offset W12 for opcodes `0x0e` and `0x0f` (`core/t64_rdp_raster.c:71-72, 405-416`). The packet contract places the `0x0f` groups in Edge, Shade, Texture, Z order, with Texture at W12-W19 and Z at W20-W21 (`docs/spec/rdp.md:236-243`; `07-08-DEPTH-FIELDS.md:28-41`). Before the repair, both helpers selected W4 for `0x0f`, interpreting shade coefficients as S/T/W; this could produce incorrect texture coordinates and color for accepted textured, shaded Z triangles. Both helpers now use the same correct offset.

The change preserves neighboring opcode offsets: texture-only `0x0a`/`0x0b` still use W4, and non-Z shaded-texture `0x0e` remains at W12. The triangle profile gate still refuses Z opcodes when the machine lacks the depth profile (`core/t64_rdp_raster.c:181-193`), preserving the earlier profile's `0x0f` refusal.

The retained literal decoder probe failed for `0x0f` before the repair (it returned 64/128/192 in Q16 instead of 0/0/1) and passes for both `0x0e` and `0x0f` afterward. This probe validates coefficient selection only; the full fetched textured-shade-Z color/VI regression is still pending. I inspected source and saved probe logs without running a build or test.

The production portion of WR-01 is fixed. This does not close the separate packet-fixture/oracle test warning or WR-02/WR-03, and it does not establish an overall clean review.
