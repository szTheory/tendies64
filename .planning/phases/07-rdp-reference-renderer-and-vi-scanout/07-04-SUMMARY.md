---
phase: 07-rdp-reference-renderer-and-vi-scanout
plan: "04"
subsystem: emulation
tags: [n64, rdp, triangles, coverage, c17]
requires:
  - phase: 07-03
    provides: [TMEM and copy foundation, composite profile compatibility]
provides:
  - Bounded fetched edge and shade triangles with retained logical sample coverage
  - Versioned v3 profile and canonical digest ownership
  - Independent literal packet, adjacency, refusal and ownership controls
affects: [phase-07, rdp, raster, profile-serialization]
key-files:
  created: [core/t64_rdp_raster.c, core/tests/test_rdp_triangles.c]
  modified: [core/t64_rdp.c, core/t64_internal.h, core/t64_machine.c, core/t64_digest.c, core/t64_isviewer.c, core/include/tendies64/t64_machine.h, core/CMakeLists.txt, runner/t64_real.c, docs/spec/rdp.md, docs/spec/rdp-vi-cases.json, scripts/check-test-inventory.sh, scripts/check-rsp-no-hle.py, scripts/tests/test_rsp_no_hle.py]
requirements-completed: []
requirements-contributed: [VID-01, VID-05]
status: complete
completed: 2026-11-17
commits: 3
plan_head_before: 261282707e7e686f9bca7e9f71517791495ba10e
plan_head_after: bc402fb
---

# Plan07-04: Triangle edges and shading

Raw edge-only and shaded triangles now render incrementally through DPC under
the explicitly approved software numeric policy. This is implementation
completion; independent source review and hosted qualification follow.

## Behavior and ownership

The reconciled eight packet lengths are checked before command effects.
Supported edge/shade packets have bounded integer arithmetic and row work;
unsupported complete texture/depth variants refuse. Literal tests cover signed
coefficients, both orientations, fractional/equal Y, shared edges, one sample,
degeneracy, draw order, maximum extent and malformed groups. Old v2 reaches its
raw triangle refusal without unsupported setup masking the assertion.

The v3 profile is descriptor7/model12/state17. Its per-instance logical
1024×1024 uint16 sample-mask plane costs2,097,152bytes. Reset, re-admission,
instance isolation and digest sensitivity are asserted. Plan07-07 still owns
address/format mapping and non-raster write effects; this plane does not claim
physical hidden-bit packing. Compatibility plumbing beyond the original file
list implements the already approved policy, without a new approval boundary.

## Evidence

- Valid compiled RED reached raw0x08 and discriminating signed slope fields;
  earlier compilation/setup failures are retained and are not counted as RED.
- Commits: `9e657e54` test RED; `4519c4b6` production/profile/digest; `bc402fb`
  expanded matrix/inventory/source/object controls.
- `sh scripts/gate.sh`:148/148 CTests, fixture and inventory controls, and final
  core lint passed in51s on final sources. The earlier49s gate was before the
  retained coverage plane and is not final-source evidence.
- Focused triangles passed10/10 registered cases in normal and ASan/UBSan.
  Twelve affected sanitizer owners passed in42.22s. Full sanitizer inventory
  was not run.
- Strict ownership covers78 production sources;34 mutation controls passed,
  including a modified raster body and a missing object.
- Normal retained artifacts:
  `build/gate/07-04-full-gate-coverage-plane-retained-artifacts/binding.json`.
  Instrumented retained artifacts:
  `build/asan/07-04-coverage-plane-retained-artifacts/binding.json`.
  Parent verified all21/20 consumer, log and source entries against saved
  SHA-256/size values and current committed content. Recorded source_head is the
  pre-production commit while the source hash map binds the tested body; do not
  relabel this as a build made after the later task commits or pre-use retention.
- ASan compile frame diagnostics: row2272B, event4032B, apply4832B,
  create2560B. They are compile estimates, not runtime peaks. Paths have bounded
  call depth, no recursion, and plane storage belongs to the instance.
- Wave hooks: no schema drift; codebase drift skipped for absent STRUCTURE.md;
  no frontend/UI gate applicable. Protected owner-dirty files unchanged.

## Limits and next action

The [raw contract](07-04-RAW-CONTRACT.md) distinguishes primary packet layouts
from owner-approved D-11 numeric behavior. Grid, tie and interpolation behavior
remain software policy. Source numeric gaps, hardware precision and VID-04 are
not qualified. No oracle, private diagnostic replay, physical test, release or
tag was authorized or performed. D2/D5/D6 stops remain. No user action is needed;
the orchestrator owns review, any fixes, hosted qualification and Plan07-05.

## Review fixes and final local qualification

The initial independent review found one accepted custom-profile alignment bug
and four documentation/test gaps. A tiny public-constructor reproduction under
UBSan confirmed an odd RDRAM size produced a misaligned uint16 coverage pointer;
its failed log is retained at `build/gate/07-04-review-repro/alignment.log`.
The constructor now includes checked alignment padding. The same reproduction
passes with zero alignment remainder after repair.

Two additional registered cases exercise that custom allocation and distinct
literal RGBA bases and dx/de/dy fields. Three independently calculated shade
pixels include opposite alpha threshold outcomes. Every malformed-group control
now compares the whole seeded 8×8 framebuffer and complete coverage plane;
prior completed raster work is snapshot and retained. Documentation correctly
separates canonical coverage from diagnostics and D-11's Y policy from primary
source facts.

Final repair evidence: normal and ASAN triangle inventories12/12 cases;
source/object guard78sources and34mutations; full `sh scripts/gate.sh`148tests
plus core lint PASS in52s. Normal and instrumented retained bindings are
`build/{gate,asan}/07-04-review-fix-retained-artifacts/binding.json`.
The [focused recheck](07-04-REVIEW-RECHECK.md) resolves all five initial findings.
This repair changes storage placement, not the built-in profile's software
semantics, so no additional profile version is needed. Hosted qualification
remains pending and the full ASAN inventory remains unrun.

The bounded GCC13 read-only/network-disabled compile check passes all seven
changed C translation units using each target's core/test warning policy. The
first attempt incorrectly applied the core stack-frame warning to a test ROM
buffer; it is retained as a check-configuration failure, not a source defect.

## Hosted qualification

Reviewed checkpoint `3e5723527211b218378223f5b50a53530b4ee964` passes both jobs in
[run44031076519](https://github.com/szTheory/tendies64/actions/runs/44031076519).
The exact selected boot-evidence artifacts were collected and compared with
source/run/attempt/job selectors. Guest identities, bounds, canonical state and
authored transcript match; platform build IDs may differ. Collection and compare
logs: `build/gate/07-04-hosted-{collect,compare}.log`; records:
`build/evidence/07-04-hosted-44031076519/`. This is selected authored boot evidence,
not uploaded triangle output or differential/hardware qualification. Native
triangle assertions passed within both hosted gates.

macOS job338s/Gate325s is the critical path; Linux job149s/Gate138s, queues3s.
This is consistent with prior qualification; no cache/concurrency/dependency
change is justified. Plan07-05 is next, after its focused raw contract preflight.
