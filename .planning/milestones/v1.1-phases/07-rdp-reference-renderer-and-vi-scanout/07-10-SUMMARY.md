---
phase: 07-rdp-reference-renderer-and-vi-scanout
plan: "10"
subsystem: emulation
tags: [n64, vi, filters, output, c17]
requires:
  - phase: 07-09
    provides: [rational VI register timing and geometry]
provides:
  - Identity-bound integer VI filter pipeline and actual tap metadata
  - Producing-boundary output reservation and atomic observer delivery
  - Supervised runner cadence and genuine whole/split evidence
requirements-completed: []
requirements-contributed: [VID-03, VID-05, VID-06]
status: complete
completed: 2026-11-17
plan_head_before: 8deaaf55f319f31d49726df5b63e2b20f779534f
plan_head_after: ef6c8ae8a0df270b1d981aef5654f7d2005c9e1b
---

# Plan07-10: VI filters and lossless raw output

Composite `ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9` retains descriptor10
and advances model18/state23. Its identity binds AA/resample modes, coverage
background selection/rounding, divot, integer gamma/gamma-dither, conditional
16-bit dither smoothing, edge policy and stage order. Primary field/equation
facts and unresolved hardware rules remain distinct from these named software
choices in the [preflight](07-10-VI-FILTER-FIELDS.md),
[contract](07-10-RAW-CONTRACT.md) and [spec](../../../docs/spec/vi.md).
Earlier profile identities/bytes and unfiltered behavior remain pinned.

Raw packets report actual selected filters, source format/geometry/field and
tap span. Output capacity is checked at a producing field boundary, leaving
the exact event pending when full. Atomic copy/drain does not advance guest
time or change the canonical digest. The test-only supervisor cadence knob
is absent from production; accepted16/32tick comparisons use actual packets
and machine digests.

## Implementation, review and repair

[Execution evidence](07-10-EXECUTION-EVIDENCE.md) records the accepted v9 AA
packet RED before filter implementation; v8 is an unchanged negative control.
It retains setup/compiler/inventory/registry failures separately. Required
profile/digest/capability/scheduler/runner wiring extends the estimated file
list within the already approved policy. Coupled registrations were committed
as one cohesive verified implementation, avoiding cosmetic partial staging.

[Independent review](07-10-REVIEW-INITIAL.md) found valid lower-row taps clipped
to a visible-derived source height, a whole/split test issuing equivalent calls,
and fixture failures able to skip assertions. Parent old-core proof reached
wrong AA/dither pixels and source spans. The sampler now uses checked backing
RDRAM bounds without the invented bottom cutoff; unused height bookkeeping is
removed. Independent [production recheck](07-10-REVIEW-PRODUCTION-RECHECK.md)
and [test-only recheck](07-10-REVIEW-TEST-RECHECK.md) resolve all findings.
One-row next-row/actual-RDRAM-end literals, forced setup failure and genuinely
different166 versus83/165/166 requests cover the gaps. A dither source-span
expectation changes18 to24 from independently derived row3/column2 taps;
existing pixel goldens were preserved. The evidence profile-ID typo is fixed.

## Reached verification

- Final exact normal VI-filter and output suite checkers pass745/381checks,
  including the actual supervised runner cadence; normal baseline6/6 and
  focused ASan/UBSan9/9 pass. Current profile1309 and prior VI timing1813checks
  retain old-profile coverage.
- Changed production/tests pass existing read-only, network-disabled GCC13
  warning checks. Only core receives the frame-size cap.
- Strict source/object/body guard80sources and52mutation controls pass;
 156entry inventory remains exact. Complete `sh scripts/gate.sh` passes in61s
  (CTest31s, inventory selftest14s, lint7s), with original logs/exit retained
  as `build/gate/07-10-review-final-full-gate*`.
- Forced valid fixture advance failure exits1 with one counted assertion;
  old-library boundary regressions fail only after reaching actual packets.
- The128tick runner attempt lost117records from the fixed256record trace ring
  before output/no-progress handling. It is not supported cadence evidence or
  an accepted retry defect. No observer/execution bound was increased.
- Schema/UI gates have no blocker; codebase drift skips without STRUCTURE.md.
  Full native/ASan/compiler/hosted qualification remains Plan11; no hardware,
  private/oracle run, broad compatibility or VID-04 claim is made.

## Exact artifacts and remaining qualification

Original63entry binding and its preceding40entry immutable snapshot remain
verified. Review-repair normal94/instrumented35entry archives are
`build/gate/07-10-review-final-retained-artifacts/{normal,asan}/binding.json`;
final full-gate retention is
`build/gate/07-10-reviewed-gate-retained-artifacts/binding.json`.
Normal build ID is4f10a216af1f0564c70eeb58590dbb08fe534486f86afb7c64e06ff602daf5b9;
instrumented is4e93628787d5e272fad1cbbbad8a36798c8bb6680820331b69255559a5c0a975
(with t64-build-v1 prefix). Actual compiled revision annotation04bac95 is
preserved; verified worktree source hashes bind the later repair commit,
without relabeling binaries or repeating tests merely for metadata.

Both hosted jobs pass exact checkpoint8def244 in run44198383459; attempt1
selected authored boot identities/bounds/state/transcript compare. macOS
job443s/Gate428s, Linux188s/Gate181s; queues3/2s. Original records and
collector/comparator/metrics are retained under
`build/evidence/07-10-hosted-44198383459/` and `build/gate/07-10-hosted-*`.
Those uploads qualify selected startup records; native VI owners execute in
each gate, without an external image differential. D2/D5/D6,
expired-resource and protected-asset stops remain unchanged. No user action is
needed for the next authored qualification plan.
