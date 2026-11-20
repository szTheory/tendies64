---
phase: 07-rdp-reference-renderer-and-vi-scanout
plan: "09"
subsystem: emulation
tags: [n64, vi, timing, c17]
requires:
  - phase: 07-08
    provides: [depth and complete RDP transport]
provides:
  - Versioned rational VI half-line scheduling and register latches
  - Register-derived geometry, source formats, field parity and IRQ behavior
  - Literal timing, source-span and predecessor-identity regressions
requirements-completed: []
requirements-contributed: [VID-03, VID-06]
status: complete
completed: 2026-11-17
plan_head_before: c1f391d85aa4999275439397335480721e7c9c65
plan_head_after: c49b0a8864da522c3c36ebd076260b1d2b24a9a2
---

# Plan07-09: VI register geometry and guest timing

Composite v8 retains descriptor10, advances model17/state22, and binds its
half-line clock, next-field latch and Q10 sampler policies. All fourteen VI
registers have raw readback; CURRENT acknowledges MI immediately and INTR
updates the next line comparison without retroactive triggering. Packets expose
actual source pitch/format, decoded endpoints, field/parity and timing policy.
Source bounds use RDRAM independently of output packet capacity. Earlier
profile hashes and serialized lengths remain pinned.

The [primary fields](07-09-VI-FIELDS.md) and
[software contract](07-09-RAW-CONTRACT.md) distinguish SDK facts from chosen
terminal counts, latches, source-alpha and sampling rules. These are authored
software results, not hardware accuracy, PAL/MPAL support or VID-04 completion.
Filtering and broader qualification remain in the following plans.

## Implementation and independent repair

[Initial execution evidence](07-09-EXECUTION-EVIDENCE.md) retains its original
commands, failures and tested identities. The first valid consumer was GREEN;
no preimplementation semantic RED was established. Setup refusals are not
relabeled. The initial implementation commit was amended with owner approval
solely to remove private workstation paths before publication: original local
fd9d466 corresponds to sanitized1860fa5 with identical production code. Public
history was unchanged; retained artifact annotations keep their actual values.

[Independent review](07-09-REVIEW-INITIAL.md) found an old H_SYNC boundary
surviving a next-field timing update, an output-height limit incorrectly applied
to source rows, stale parity and packed endpoint metadata, plus missing v7
byte goldens. Parent reproduction reached all production defects against the
retained library. Parent also found that INTR writes incorrectly latched with
geometry. The [focused independent production recheck](07-09-REVIEW-PRODUCTION-RECHECK.md)
resolves the source defects; the [orchestrator test-only recheck](07-09-REVIEW-TEST-RECHECK.md)
closes predecessor goldens and independent literal/edge oracles. Corrected
fixture mistakes and their invalid evidence remain explicitly distinguished.

## Reached verification

- Final registered VI inventory29cases; normal and ASan/UBSan1813checks each,
  zero failures. Distinct last-row colors prove actual interlaced source rows.
- Profile/accepted runner/wrong-pixel/RDP-fill owners pass4/4; profile1122checks.
  Focused sanitizer VI/profile/fill owners pass3/3; final row-oracle change
  received another scoped normal/instrumented VI check.
- Changed production/tests pass existing read-only, network-disabled GCC13
  warnings-as-errors checks; core alone receives the core frame-size limit.
- Strict source/object/body guard80sources and51mutation controls pass.
- Complete final `sh scripts/gate.sh` passes153tests and all wrapper stages,
  including reachable-history privacy, rights/provenance and core lint, in109s.
  CTest64s, inventory selftest16s and core-lint15s are recorded measurements.
  The earlier152/153 gate failure is retained: private document paths were
  fixed through the explicitly approved unpublished-commit amendment, not waived.
- Schema drift and frontend gate have no blocker; codebase drift skips because
  no STRUCTURE.md exists. No full ASan inventory or private/oracle run is claimed.

## Exact artifacts and remaining qualification

Final gate log/LastTest/exit are `build/gate/07-09-approved-final-full-gate*`.
Original implementation28-entry, review84/22-entry and final test-only30/17-entry
normal/instrumented source/binary/log archives remain retained and hash-verified.
The final approved-gate archive is
`build/gate/07-09-approved-final-retained-artifacts/binding.json`.
Normal build ID is d27ec6cd2f74e48491e87828ae423a1c15a4aaec490959eb0cec92b84ff1ce6f;
instrumented build ID is6732912ab78d1d3a1c289df28af4d85a701c45b3995ba9ad753a4742f619f2f2
(with the standard t64-build-v1 prefix). Compiler/build policy differs; they
are not interchangeable binaries. Saved bindings identify actual source bytes,
provenance annotations and executable hashes, including earlier uncommitted
verified worktrees. Metadata-only closeout does not relabel those binaries.

Both hosted jobs pass exact checkpointfc83c58 in run44174870063; collected
attempt1 selected authored boot records match guest identities, bounds, state
and transcript. macOS job385s/Gate367s, Linux174s/Gate167s, queues4/2s.
Records and collector/comparator/metrics logs are retained under
`build/evidence/07-09-hosted-44174870063/` and `build/gate/07-09-hosted-*`.
These selected uploaded boot records qualify that authored startup contract;
native VI owners run in each hosted gate and are not an external RDP image
differential.
Rights/oracle/release and expired-resource stops remain unchanged. Plan10's
primary preflight and explicit integer software-filter contract are ready.
