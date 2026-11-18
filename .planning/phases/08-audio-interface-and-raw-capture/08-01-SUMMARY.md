---
phase: 08-audio-interface-and-raw-capture
plan: "01"
subsystem: audio
status: complete
tags: [ai, dma, pcm, deterministic, runner]
requires:
  - phase: 07-11
    provides: Qualified authored CPU/RSP/PI/RDP/VI and raw-output foundation
provides:
  - Fetched guest stereo PCM across two DMA descriptors through the actual runner
  - Versioned composite profile with inherited operations and historical identity pins
  - Exact test inventory, source-body mutation controls and retained raw diagnostics
affects: [08-02, 08-03, 08-04, 08-05, 09]
actuals:
  tokens: 52727
  tasks: 2
  commits: 3
tech-stack:
  added: []
  patterns: [guest-timed per-machine AI, independent literal PCM comparator]
key-files:
  created: [core/t64_ai.c, fixtures/audio/pcm.S, scripts/check-audio-cases.py, runner/tests/test_audio_capture.c]
  modified: [core/t64_machine.c, core/t64_scheduler.c, core/t64_digest.c, core/tests/test_profile.c, runner/t64_real.c]
key-decisions:
  - Preserve historical profile bytes and refusals; AI composite uses descriptor11/model19/state24.
  - Authored PCM establishes software transport, not hardware accuracy or oracle authority.
requirements-completed: [AUD-01, AUD-02, AUD-03]
requirements-contributed: [AUD-01, AUD-02, AUD-03]
coverage:
  - id: D1
    description: Fetched two-DMA literal PCM and reached wrong-sample control in actual runner
    requirement: AUD-01
    verification:
      - kind: integration
        ref: scripts/check-audio-cases.py --build build/gate --suite tracer --run
        status: pass
      - kind: integration
        ref: scripts/check-audio-cases.py --build build/asan --suite tracer --run
        status: pass
    human_judgment: false
  - id: D2
    description: Composite inherited operations and historical identity/refusal pins
    requirement: AUD-03
    verification:
      - kind: integration
        ref: scripts/check-audio-cases.py --build build/gate --suite profile --run
        status: pass
      - kind: integration
        ref: scripts/check-audio-cases.py --build build/asan --suite profile --run
        status: pass
    human_judgment: false
completed: 2026-11-18
plan_head_before: d36eaad4e362819ea8a89babe8efcdadb449534a
plan_head_after: 79e8ab5a9e020c5964f046c603501a63768f319b
---

# Phase8 Plan01: fetched PCM tracer

Fetched guest stores now produce independently expected signed stereo samples
across two DMA descriptors through the canonical bus, scheduler, MI, packet API
and supervised headless runner. The wrong-sample fixture reaches the same sample
comparator and is rejected. The composite also executes inherited CPU/RSP/PI
operations and authored RDP/VI output. Historical profiles retain identity and
AI refusal behavior.

## Task commits and scope

- RED profile lookup: `b339bef`.
- Task1 production tracer, fixture, inventory and runner: `fb14108`.
- Task2 profile golden bytes and mutation controls: `79e8ab5`.

Execution commits span 2026-11-18T09:47:15-05:00 through 2026-11-18T12:46:24-05:00; this is elapsed commit
wall time, not active CPU time. 35 files changed; actuals use realized
diff characters divided by four, not model billing tokens.

The summary requirement list follows the plan schema. These are contributions;
whole-phase requirements remain unchecked until the remaining matrices, capture,
review and qualification pass. No hardware or broad compatibility claim follows.

## Verification and retained evidence

Normal and ASan/UBSan profile suites pass all five registered tests and tracer
suites pass all three. The source/body mutation harness passes all55 seeds,
including new AI MMIO/sample/scheduler mutations. A fresh parent source/body/object
check passes81 reviewed sources; parent parsed all four nonempty owning JUnits
with no failures and verified source artifacts and owner-edit identities.

Private evidence remains under `build/gate/08-01-evidence/`,
`build/gate/08-01-profile-*`, `build/gate/08-01-tracer-*`, and matching
`build/asan/08-01-*-asan-*`. The successful literal and wrong-sample runs finish
at guest tick551001 without resource yields. Passing producer bytes, commands,
source/executable/fixture identities and traces are copied in
`build/gate/08-01-evidence/passing-audio-tracer/`.

Normal Release AI compilation with the production frame warning as an error
passes; the largest measured AI frame is432 bytes. Instrumented Debug/O0
frame-size warnings are retained, with no sanitizer findings. Rights/provenance,
source-path and whitespace checks pass. Existing owner edits remain unstaged.

## Necessary corrections and evidence limits

- The new profile was missing IsViewer activation. A retained first-exception
  trace identifies DBE at the fetched completion store. Activation now admits
  only the exact new profile; the prior profile's refusal is pinned.
- The old fixed video observation assumed small atomic pipe records. Bounded
  concurrent exact-length readers now handle multi-write audio/video records,
  retain both streams and clean up the child; the original video control passes.
- The real runner emitted a yield-limit detail missing from the closed result
  vocabulary. Only emitted negative details were added, with rejection tests for
  unknown details and incompatible outcomes. A valid machine digest is retained
  independently from observation failures. Original failing reports remain intact.
- Fixture timing initially exhausted the existing total64-yield cap. Actual
  progress/packet records identified video backpressure; fixture VI timing was
  corrected without changing runner limits or scheduler budgets. Core tests now
  account for actual advance progress across the existing event budget.
- The guest transcript length included alignment NULs; exact authored length is
  now asserted. The independently expected VI pattern follows the authored
  alternating fill word rather than an incorrect solid-color assumption.
- Source-body seal maintenance initially used unnormalized text; the cheap gate
  stopped before CTest. Correct normalization and mutation controls now pass.
- The first supervised failure's temporary originals were deleted by the old
  harness and cannot be recovered. Later attempts retain command, original
  report, metadata, pipe bytes and reader status on every exit. Fresh retained
  attempts do not replace or reclassify that historical evidence gap.

Narrow necessary files beyond the original roster include IsViewer gating,
closed report vocabulary, observation pipe helpers and legacy video test wiring.
No dependencies, resource-limit increases, protected assets or oracle use were
introduced. Remaining register/cadence/capacity/capture matrices belong to the
following plans; this slice does not establish those unexecuted criteria.

## Next useful action

Execute08-02 register/DMA/MI and rational-cadence matrices. Preserve existing
qualified binaries before subsequent builds. Phase7/VID-04 and all authority,
rights, release and physical-measurement stops remain in effect. No user action
is required.

## Self-Check: PASSED

Source files exist, all three commits are reachable on this branch, retained
normal/instrumented owning JUnits are nonempty and passing, and the source/body
check passed after the last source edit. Independent source review and cohesive
full qualification remain scheduled in08-05.
