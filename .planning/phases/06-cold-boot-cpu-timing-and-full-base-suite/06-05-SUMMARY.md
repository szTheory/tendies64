---
phase: 06-cold-boot-cpu-timing-and-full-base-suite
plan: "05"
subsystem: authored-cold-boot-and-determinism
status: complete
completed: 2026-11-15
requires:
  - phase: 06-03
    provides: Native cold protocol
  - phase: 06-04
    provides: T1 CPU and scheduler contract
provides:
  - Actual exact-profile Tier A/B hello with ordered cold-protocol evidence
  - Canonical architectural BREAK qualification without CPU semantic changes
  - Complete-boot partition, interleave, refusal and reset checks
  - Cheap consistency gates and immutable per-attempt artifact retention
affects: [06-06, 06-07, 06-08]
requirements-completed: [BUS-03, CPU-08]
actuals:
  tokens: 34925
  tasks: 2
  commits: 9
key-files:
  created: [docs/evidence/phase06-cold-boot.md, .planning/phases/06-cold-boot-cpu-timing-and-full-base-suite/06-CALIBRATION-MODEL-RESOLUTION.md]
coverage:
  - deliverable: Authenticated exact-output preset and cold-protocol boot
    human_judgment: false
    verification:
      - {kind: ctest, ref: integration.cold-hello, status: pass}
      - {kind: ctest, ref: runner.cold-boot-controls, status: pass}
      - {kind: ctest, ref: unit.core.rdram-protocol, status: pass}
  - deliverable: Per-profile complete-boot deterministic state and lifecycle
    human_judgment: false
    verification:
      - {kind: ctest, ref: integration.cold-determinism, status: pass}
      - {kind: ctest, ref: integration.open-ipl3-determinism, status: pass}
---

# Phase 6 Plan 05: Authored boot comparison and deterministic lifecycle

Both exact named profiles reach the authored terminal boundary and emit the
same literal30-byte hello output under the unchanged limits. Tier B actually
executes the required ordered RI/RDRAM/MI families and capacity scan. The
fixture contracts reuse unchanged cleared bytes. Details, artifact identity,
expectation sources and limits are in
[the authored evidence report](../../../docs/evidence/phase06-cold-boot.md).

The runner authenticates architectural BREAK9 at the exact instruction and
preceding retirement, outside a delay slot. It reports a qualified terminal;
the canonical CPU still takes its architectural exception and returns its
ordinary bound result. It never adds a CPU success shortcut or requires
Tier A's prepared path to execute Tier B's cold protocol.

## Evidence-driven corrections and scope

Actual stopped runs demonstrated missing MI upper-mode commands, packed-ID
remaps, a disproven refresh ordering precondition and excluded ISViewer
activation for the two new profiles. Native reached-operation regressions
preceded their fixes. The first pending-exception capture was secondary to
the real output-store fault; richer first-vector evidence resolved it.
A static PI source-address inference also misread LUI's upper-half immediate.
Actual PI state disproved it: zero calibrated capacity caused the refused
RDRAM destination, and the cartridge source was valid. Neither justified a
fixture change or weakened PI bounds.

D-03 permits explicitly versioned software feedback derived from the cleared
algorithm. A configured read-only advisor compared the bounded choices. The
selected pure visibility rule covers cached/uncached reads and both SP-DMA
paths, preserves backing/cache contents, uses already-hashed mode fields and
resets with the protocol. It is not analog or hardware qualification. See
[the resolution](06-CALIBRATION-MODEL-RESOLUTION.md) and
[the contract](../../../docs/spec/rdram-boot.md).

The declared source union was expanded to include these demonstrated owner
fixes. The final overlapping runner/manifest/registration/inventory/test work
was committed as one coherent closure after separate native-fix commits,
avoiding an inconsistent registration/inventory intermediate. Source commits
are01b4416,650139d,9673728,98bc092,a4794c5,8ec623a,ffe8c78,292405f,2aa20c0.
Realized diff characters per four supply actuals, not model billing.

## Verification and process lessons

The formal Task1 RED reached the production compare refusal. Invalid setup
and TAP-format captures remain separately recorded. Task2 verifies already
implemented passing behavior; its inapplicable TDD marker was removed with a
recorded rationale, and its first report is GREEN.

Fast normal controls first check accepted profile/device access, calibration
visibility across read owners, exact registered selections, complete TAP
assertion counts and inventory. They caught an inherited startup-name
replacement, a cache-selection typo and a report header/count issue before
longer final checks. The two new manifest recipes later exceeded the schema's
length bound; only their metadata was shortened and rights/schema rechecked,
without repeating unaffected core tests. The durable cheap-gate policy now
explicitly includes manifest schema/rights before long work.

All-target normal and ASan/UBSan builds pass. Native feedback and its twelve
owning checks pass in both configurations. Final five-owner selections also
pass in both: new full cold determinism, unchanged warm determinism,
boot-entry, profile and runner controls. Both exact inventories pass135/137,
retaining inherited names and adding cold hello/determinism; the inventory
selftest passes. Final current-core startup and A/B hello comparisons run
serialized after instrumented work and pass. No full-phase gate was repeated
for bookkeeping.

Cold determinism checks actual reached MDU cuts with independent literal
costs, protocol cuts, one-shot advances, interleaved independent instances,
exact terminal/output, whole-state preservation on refused altered-IPL3
admission, and reset during boot followed by fresh-state equality and complete
reexecution. A/B states are compared only within their own profile.

Every actual failed attempt remains separate, with exact bytes retained before
subsequent rebuilds. The original Plan09 archive is unchanged. Commands,
intermediate invalid oracles and raw retention hashes are in ignored
`build/gate/06-05-execution-checkpoint.md`; final original logs and binaries
are under `06-05-final-owning-checks/` and
`06-05-final-authored-comparisons/`. The final artifact's source annotation
predates its integration closure, accurately disclosed in the public report.

## Remaining work

BUS-03 and CPU-08 remain phase-level pending final review/qualification. No
external diagnostic, physical test, final coherent phase gate or hosted run
is claimed here. The earlier stopped Plan02 external attempt still has no
valid member result and a missing historical executable; nothing here repairs
or replays it. Continue Plan06's full-source classification and permitted local
lanes. No user action is needed.
