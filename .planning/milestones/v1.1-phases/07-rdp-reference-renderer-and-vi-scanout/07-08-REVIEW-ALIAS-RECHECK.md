---
phase: 07-rdp-reference-renderer-and-vi-scanout
reviewed: 2026-11-17T17:45:56Z
review_type: focused_recheck
finding: CR-01
disposition: fixed
status: other-findings-open
---

# Plan 07-08 CR-01 Recheck

**Reviewed:** 2026-11-17T17:45:56Z

## Disposition

CR-01 is fixed in the current source. In the v7 blender raster path, `t64_rdp_memory_coverage_invalidate()` now runs only when `write_color` is true (`core/t64_rdp_raster.c:809-829`). When false, the renderer updates only the logical coverage sidecar; it does not write color/alpha RAM bytes, and depth evaluation also suppresses the depth update. Thus a coverage-only result at aliased color/depth addresses preserves the existing hidden depth value.

The check is at the call site rather than inside the common invalidator, so actual backing writes keep their existing behavior. CPU scalar and masked writes, cache writeback, PI DMA, and both SP-to-RDRAM DMA paths invalidate after committing the bytes. RDP fill/copy invalidates after its RAM writes. An actual color write still invalidates the affected granules, and depth commit invalidates before storing its new hidden value. The depth-profile predicate remains the only gate for clearing hidden depth, so v6 behavior is unchanged; its coverage invalidation is followed by the same coverage authorship.

The retained alias reproduction reports 95 checks with 3 failures before this change and 95 checks with no failures after it. In that accepted stream, coverage-only output preserves visible depth `0400` and hidden depth `83`; the next compare accepts and writes color `07c1`, after which the actual color write invalidates hidden depth. I inspected the source and saved logs but did not run tests or builds in this recheck.

This closes CR-01 only. WR-01 through WR-03 from the initial report remain open; this recheck does not establish an overall clean review or final test gate.
