---
phase: 07-rdp-reference-renderer-and-vi-scanout
plan: "11"
subsystem: qualification
tags: [rdp, vi, evidence, ctest, sanitizers, ci]
requires:
  - phase: 07-10
    provides: [reviewed renderer, VI filters and raw output]
provides:
  - Original-record-bound authored RDP/VI qualification and strict public projection
  - Complete local normal and instrumented inventories plus qualified GCC source closure
  - Actual exact-run cross-host raw artifact verification and bounded support claims
requirements-completed: [VID-01, VID-02, VID-03, VID-05, VID-06]
requirements-contributed: []
status: complete
completed: 2026-11-17
plan_head_before: d17e9e03a9250de5ed0d1dfb03456317c6ffe841
plan_head_after: e6f54668afec23e603f4ce61fc44fd9b91d216da
---

# Plan11: authored renderer and VI qualification

The complete authored qualification passes. Exact hosted sourcee6f5466 passes
both jobs, and independently downloaded ZIPs from run44228987686 attempt1
verify API identities/digests, strict extraction/membership and decoded bytes.
Linux/macOS RDRAM, VI packets, profile, policy and observed DP/MI/VI state agree.
Selected boot semantics also compare. The
[public receipt](../../../docs/evidence/phase07-hosted.json) preserves distinct
host build/executable identities. This is authored software evidence, not an
external emulator comparison or hardware truth. VID-04 stays incomplete.

## Implementation and actual controls

The helper binds original commands, process outcomes, executable/fixture/profile
identity and the independently pinned source-derived case denominator. Missing,
swapped, zero/reduced/resealed, missing-v9 and forged success records refuse.
Public bytes must match an independent authored contract even after both host
records and dependent hashes are resealed. The exporter reads actual bus/VI API
bytes:24 RDRAM bytes at0x3000 and48 RGBA8 bytes,4×3 with stride16. Public JSON
encodes those same bytes; private binaries/original records are not uploaded.

The [initial review](07-11-SEGMENT-A-REVIEW-INITIAL.md),
[repair recheck](07-11-SEGMENT-A-REVIEW-RECHECK.md) and
[final recheck](07-11-SEGMENT-A-REVIEW-FINAL.md) close public forgery, descendant
cleanup, actual ZIP-path bypass, dirty-source binding and streaming byte bounds.
Actual writing-grandchild controls establish cleanup and stopped output.
Downloads retain bounded original ZIPs and API records, validate producer exit,
size and digest before strict extraction, and reject malformed members.

The first hosted run44225542279 passed both normal gates but refused its real
emitter before production because a fresh-directory constructor received an
existing directory. [The repair](07-11-HOSTED-EMITTER-REPAIR.md) and
[independent review](07-11-HOSTED-EMITTER-REVIEW.md) retain that failure, a reached
old-entry regression and the passing corrected entry. Cheap preflight now calls
that actual entry with isolated public storage. Local authored workflow IDs and
mock transport are never counted as hosted proof. The final collector control
has34 reached rejection cases; actual hosted collection is separate.

## Qualification and exact retained artifacts

- [Original native qualification](07-11-NATIVE-EVIDENCE.md): full157-test normal
  gate77.894s; full159-case ASan/UBSan suite238.655s with no failure/error/skip;
  pinned read-only/network-disabled GCC13 route23.579s for30 changed C files
  plus two test variants. Source binding covers505 inputs and passed post-run.
- All188 final normal/instrumented artifact copies were retained and verified
  against originals by SHA-256 and Git blob ID; parent rechecked them. Original
  command/outcome/logs, owning LastTest and instrumented JUnit remain private.
- The later Python-only emitter correction passed a fresh complete normal gate
  in60.269s and owning ASan-preset harness in2.03s. No C/build/fixture/spec or
  workflow input changed, so the full C-source instrumentation/compiler evidence
  is reused explicitly. New93 normal artifacts are retained separately; all95
  original sanitizer pairs were reverified unchanged. Timing-formatter and
  incomplete local-control setup failures remain distinct from test failures.
- Exact successful hosted artifact IDs14394183002/14394158461 and their archive
  digests are bound in the public receipt. Both individual upload verifiers,
  downloaded rights/PII scans and selected boot comparator pass. Actual original
  ZIP/API/collector records are retained under ignored build paths, not inferred
  from this summary.

Private originals are under `build/gate/07-qualification/` and
`build/gate/07-11-hosted-emitter-repair/qualified-followup/`; hosted records are
`build/evidence/07-11-hosted-44228987686/` and
`build/gate/07-qualification/collect-hosted-e6f54668afec-rkv_wbtz/`.

## Security, cost and remaining scope

The [independent security audit](07-SECURITY.md) records38 closed applicable
authored rows and zero reachable high/critical implemented-scope gaps. Three
future oracle rows remain open across the41-row register. No risk was accepted,
oracle functionality fabricated, private window renewed, or release authorized.

Successful CI wall476s includes a38s macOS queue; macOS job437s/Gate420s and
Linux242s. Measured macOS gate costs include fixture qualification225s and
CTest94s. [CI review](../../../docs/ci-reviews/LOG.md) retains failed and successful
runs. The concrete low-cost improvement is real-entry preflight before long
work; no cache, dependency, job, concurrency or resource limit was expanded.

Plan12 Task1's authored authority request/checker preparation is next. Its real
owner/legal checkpoint and all oracle operations remain stopped until genuine
documented scope changes. Authored vectors do not replace VID-04. No user action
is needed for the authorized preparation; Phase07 is not complete.
