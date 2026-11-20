---
phase: 08-audio-interface-and-raw-capture
plan: "02"
subsystem: audio
status: complete
tags: [ai, registers, cadence, dma, scheduler]
requires:
  - phase: 08-01
    provides: Actual fetched PCM tracer and versioned composite
provides:
  - Source-backed masks and explicit register/control software policy
  - Exact rational sample, activation latch, pause/resume and ordering witnesses
  - Transactional arithmetic refusal before scheduler dequeue
affects: [08-03, 08-04, 08-05, 09]
actuals:
  tokens: 24108
  tasks: 2
  commits: 4
tech-stack:
  added: []
  patterns: [checked predispatch admission, masked-input setup guards]
key-files:
  created: []
  modified: [core/t64_ai.c, core/t64_scheduler.c, core/t64_digest.c, core/tests/test_ai.c, docs/spec/audio-interface.md]
key-decisions:
  - Apply the documented software-profile policies without claiming silicon corner accuracy.
  - Detect sample/event arithmetic refusal before removing an event or changing time.
requirements-completed: [AUD-01, AUD-03]
requirements-contributed: [AUD-01, AUD-03]
coverage:
  - id: D1
    description: Nine register queue MI and control cases
    requirement: AUD-01
    verification:
      - kind: unit
        ref: scripts/check-audio-cases.py --build build/gate --suite registers --run
        status: pass
    human_judgment: false
  - id: D2
    description: Nine cadence partial-DMA ordering latch reset and overflow cases
    requirement: AUD-03
    verification:
      - kind: unit
        ref: scripts/check-audio-cases.py --build build/gate --suite cadence --run
        status: pass
      - kind: unit
        ref: scripts/check-audio-cases.py --build build/asan --suite cadence --run
        status: pass
    human_judgment: false
  - id: D3
    description: Actual supervised PCM and reached wrong-sample regression
    verification:
      - kind: integration
        ref: scripts/check-audio-cases.py --build build/gate --suite tracer --run
        status: pass
    human_judgment: false
completed: 2026-11-18
plan_head_before: 04f0ec86502d4f26c294cf18284967f9ffaeaca5
plan_head_after: be83730dbfbcfc9062e7e97a2940b49958f138ac
---

# Phase8 Plan02: AI registers and exact cadence

The complete register and cadence matrices pass. AI masks, effective-zero and
maximum LEN, staged addresses, checked RAM spans, two-entry queue, MI isolation,
rate validation and disable/resume follow the documented profile. Partial LEN,
per-sample live RAM, activation-time rate latching, rational before/on/after
boundaries, CPU/VI same-tick order and reset have discriminating witnesses.
These software policies do not establish unsourced electrical/hardware corners.

## Task commits

- Task1 RED `6911c46`; implementation `d9fbf7c`.
- Task2 RED `6016dc8`; implementation `be83730`.

Nine files changed. Actual token estimate is realized diff characters/4, not
model usage. Full phase requirements remain pending subsequent plans and review.

## Verified outcomes and exact artifacts

- Final normal registers:
  `build/gate/08-02-registers-final-r1-registers-1795028237304695000-12443-registers-junit.xml`.
- Final normal cadence:
  `build/gate/08-02-cadence-overflow-green-r2-cadence-1795028205403608000-11420-cadence-junit.xml`.
- Final ASan/UBSan cadence:
  `build/asan/08-02-cadence-asan-r1-cadence-1795028240836607000-12563-cadence-junit.xml`.
- Final actual runner tracer:
  `build/gate/08-02-tracer-final-r1-tracer-1795028372294211000-18256-tracer-junit.xml`.
- Task1 also passed instrumented registers before Task2 source changes:
  `build/asan/08-02-registers-asan-r1-registers-1795026150800568000-55131-registers-junit.xml`.

Each owning prefix retains commands, discovery, source guard, before/after
LastTest and JUnit. Parent parsed all five nonempty JUnits, found no failed,
errored or skipped tests, verified owner edits, and freshly ran the source/body/
object guard after the final source edit. Isolated source copies pass unchanged;
AI preflight-body and scheduler-link mutations reject under
`build/gate/08-02-source-body-mutations/`.

Plan01 artifacts were retained and SHA256-verified before replacement in
`build/gate/08-02-preserved-plan01/` and
`build/asan/08-02-preserved-plan01/`. Historical qualified evidence remains
immutable and is not relabeled as current C qualification.

## Corrections, deviations and limits

Overflow tests found that sample dispatch could pop/advance before later
arithmetic refusal. A read-only predispatch check now validates counters,
generation, tick, sequence and queue capacity before scheduler mutation; refusal
also preserves generation. Tests isolate the initial CPU event so snapshots
measure the AI boundary, rather than demanding no earlier legal guest progress.

BITRATE31 and LEN12 were incorrect test expectations: four-bit bitrate masks31
to15; eight-byte LEN alignment masks12 to8. The invalid attempts remain retained
and are not the formal TDD evidence. Corrected setup helpers assert register
masks/readback and frame-count-derived effective lengths before timing checks.
The valid latch RED uses bitrate7→15 and a legal eight-byte descriptor. A cached
KSEG0 CPU store initially hid new bytes from DMA; an uncached KSEG1 store now
isolates same-tick ordering through the actual CPU path. A misspelled internal
constant stopped compilation only; no semantic RED was claimed from it.

Required extra files beyond the planned roster were `core/t64_internal.h`,
`core/t64_digest.c` and `scripts/check-rsp-no-hle.py` for pause/preflight state and
source seals. No limits, dependencies, protected assets or hardware authority
changed. Whole/split, observer capacity/copy ownership and actual host drain
independence remain Plan03 work. Full current-source qualification remains05.

## Next useful action

Execute08-03 lossless output and determinism. Preserve current binary bindings
before builds. Oracle/VID-04 and publication/release stops remain unchanged.
No user action is required.

## Self-Check: PASSED

All four source commits are reachable on this branch; required modified files
exist, fresh source/body/object verification passes, and retained owning JUnits
are nonempty and passing. Whole-phase review and qualification remain pending.
