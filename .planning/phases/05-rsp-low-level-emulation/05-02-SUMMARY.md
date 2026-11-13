---
phase: 05-rsp-low-level-emulation
plan: "02"
subsystem: rsp-control-dma
requires:
  - phase: 05-01
    provides: Fetched RSP reference entry and distinct functional profile.
provides:
  - Profile-scoped SP control, setup12/eight-byte beat DMA and issue readiness.
  - Authored control/refusal/row/queue/partition regression matrix.
affects: [05-03, 05-04, rsp-native-evidence]
plan_head_before: a62cd80a317f44665d5751ea2a2b2d543bf86aa3
plan_head_after: d7479bc
actuals: {tasks: 2, commits: 5}
requirements-completed: []
completed: 2026-11-13
status: complete
---

# Plan02 — SP control, DMA and issue model

The functional RSP profile supports masked halted PC writes, halt/resume,
status commands, semaphore read-to-set, progressive DMA, queue promotion,
and bounded instruction pairing/readiness/branch behavior. All five historical
profiles retain their own capability predicates and policies. New authored
controls assert accepted setup before specific refusals and compare future
state, queue, readiness and exact bytes. DP execution remains unsupported.

## Evidence and corrections

The computer restart terminated the executor. Its original semantic SP_PC
RED commit and partial source survived; the replacement resumed those edits
and fixed their compile/link seams. The second task captured an actual
active-PC IMEM overwrite before adding the transactional guard. Both original
RED artifacts remain preserved. Fixture setup errors encountered later were
corrected and recorded separately; they were not counted as behavioral RED.

Parent source review found MTC2 destination timing and missing COP-move
load/store hazards. Added regression controls failed against those exact
behaviors before correction. The permanent tests now assert the guide-backed
delays, with tracer tick expectations adjusted but output bytes preserved.
[Review evidence and limits](05-02-review-evidence.md) records the distinction.
The [model contract](../../../docs/spec/rsp-profile.md) separates primary
document facts from functional timing, simultaneous command precedence,
BREAK-PC and conflict policies.

- Final all-target normal and ASan/UBSan builds pass.
- Final focused normal and instrumented selections each pass10 tests, covering
  RSP control/tracer, SP DMA, CPU links, profile, scheduler, digest, determinism,
  bus and legacy warm-device behavior. Saved raw logs establish the result.
- Exact inventory is113 normal/115 instrumented, preserving prior members and
  adding only unit.core.rsp-control. Core lint reports84 files/zero violations;
  docs, diff, rights and PII checks pass.
- Tested normal build identity:
  `t64-build-v1-ff04964385daa32a0856ab012773e2c2305936cfc1e6c20b10ff293fd30c346c`.
  Instrumented:
  `t64-build-v1-eb6397df082710b466b99266f838c4b708ba5d1a3e7b4744cbcdcf4f36b5d915`.
  Both carry179cd40 provenance annotation plus the tested correction delta;
  content identity binds the actual tested source. No annotation-only rebuild.

Final correction logs are retained at build/gate/05-02-review-normal-build.log,
build/gate/05-02-review-normal-ctest.log, build/asan/05-02-review-build.log and
build/asan/05-02-review-ctest.log. The final normal selection took7.48s and the
serial instrumented selection121.37s; these are native verification costs,
not emulator throughput. The canonical combined gate then passed all113 tests,
inventory/negative controls and lint in52s on d7479bc. Its saved log is
build/gate/05-02-canonical-gate.log. Hosted candidate verification is tracked
separately; no result is inferred for an unfinished run.

## Scope, threats and next action

No external diagnostic, ROM execution, hardware measurement, tool install or
new copied implementation occurred. Native tests establish this functional
subset; no whole RSP requirement is closed here. Span/admission and issue
atomicity threats have direct authored controls. Configured independent phase
code/security review and final qualification remain Plan10 work.

Plan03 now owns source-derived private RSP qualification and an actual guarded
component build; Plan04 immediately follows with a bounded startup before
broad ISA expansion. No user action is needed.

## Lesson

Distinguish the destination register class when converting timing rules into
readiness fields. A passing load or move case does not cover the other class.
Before bus guards land, make authored fixture HALT preconditions explicit;
do not weaken production guards to preserve invalid setup. Use saved semantic
RED once; repeating its unchanged classifier at each GREEN step adds no evidence.
