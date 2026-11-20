---
phase: 08-audio-interface-and-raw-capture
plan: "03"
subsystem: audio
status: complete
tags: [pcm, output, determinism, backpressure]
requires:
  - phase: 08-02
    provides: Checked AI cadence and dispatch admission
provides:
  - Six safe-copy and lossless packet-capacity contracts
  - Whole/split, interleaved, equal-tick and independent stream-capacity witnesses
  - Actual supervised runner comparison under distinct advance/drain schedules
affects: [08-04, 08-05, 09]
actuals:
  tokens: 16308
  tasks: 2
  commits: 2
tech-stack:
  added: []
  patterns: [accepted baseline before semantic comparison]
key-files:
  created: []
  modified: [core/tests/test_ai.c, runner/tests/test_audio_capture.c, core/CMakeLists.txt, runner/CMakeLists.txt, docs/spec/audio-cases.json, docs/spec/audio-interface.md]
requirements-completed: [AUD-01, AUD-02, AUD-03]
requirements-contributed: [AUD-01, AUD-02, AUD-03]
key-decisions:
  - Existing production behavior passes; add coverage without unnecessary production edits.
  - Use a valid faster cadence for one-call comparison within the existing event budget.
coverage:
  - id: D1
    description: Packet capacity reservation atomic copy and canonical observer ownership
    requirement: AUD-02
    verification:
      - kind: unit
        ref: scripts/check-audio-cases.py --build build/gate --suite output --run
        status: pass
      - kind: unit
        ref: scripts/check-audio-cases.py --build build/asan --suite output --run
        status: pass
    human_judgment: false
  - id: D2
    description: Core and actual-runner deterministic stream comparisons
    requirement: AUD-03
    verification:
      - kind: integration
        ref: scripts/check-audio-cases.py --build build/gate --suite determinism --run
        status: pass
      - kind: integration
        ref: scripts/check-audio-cases.py --build build/asan --suite determinism --run
        status: pass
    human_judgment: false
completed: 2026-11-18
plan_head_before: aaac5c62649a8e6ae2976d987a0c14eb9e984b70
plan_head_after: 6280d2aeec05e6f3ac8c784224ea2565b70dc36e
---

# Phase8 Plan03: lossless packets and deterministic drains

All six output contracts and five determinism cases pass in normal and
instrumented builds. Existing production semantics already satisfy them;
this slice changes tests, registrations, exact inventory and documentation.
Task1 is `0e3e759`; Task2 is `6280d2a`. Six files changed; actuals use diff
characters/4. Whole-phase requirement acceptance still awaits capture/review/
qualification, despite the plan-schema requirement list above.

## Evidence

Owning checker prefixes retain commands, source/discovery results, unique JUnit
and before/after LastTest, including failures:

- `build/gate/08-03-output-final-normal-*`
- `build/asan/08-03-output-final-asan-*`
- `build/gate/08-03-determinism-final-normal-v3-*`
- `build/asan/08-03-determinism-final-asan-v2-*`
- `build/gate/08-03-tracer-final-normal-*`

The parent freshly parsed all five owning nonempty JUnits with no failure,
error or skipped case; source/body/object check passes after final edits.
Owner-edit blob identities remain unchanged and unstaged. Prior binary/core
artifacts were retained and hash-checked before replacement.

Tests reach reserved-before-first-sample/full-same-event, exactly-once resume,
atomic info/copy, overlap/RAM/image/address overflow refusals and producer
history versus observer ownership. One-call and uneven-partition advances
produce identical nonempty packets/metadata/digest at equal guest ticks;
interleaved machines use distinct data/rates. AI/VI event order and independent
stream fullness are exercised. Two supervised literal-fixture runs at test-only
advance steps16/32 produce identical two-packet PCM, metadata, completion,
CPU tick and canonical digest. Normal tracer positive and reached wrong-sample
control remain passing. No sanitizer finding occurred; existing instrumented
stack warnings remain retained.

## Honest failed attempts and lessons

A full-queue snapshot originally preceded legal earlier events. Compare actual
state once the AI event is blocked, not an assumed event tick: the last legal
event can be earlier. Drain preserves that actual tick/digest and pending event.
An interleave test accidentally copied one packet twice; corrected test-only.
A source patch-scope/build or incomplete temporary inventory failure did not
reach CTest and is not semantic evidence.

The first single-call comparison requested more than the4096-event budget at
rate1103/15; it stopped at tick4094 before the desired bound. The valid131/1
rate pair satisfies132>=66*2 and reaches four samples at tick677 in one real
public advance. The uneven side reaches the same bound and drains earlier.
No budget was increased and the whole side was not silently partitioned.

The first runner schedule invocation hit CLI usage before guest startup.
The existing test-only option belongs to `t64-supervisor-test`; routing through
that target reaches both actual outputs. No production CLI or caps changed.
All these failed originals remain distinct from passing evidence; no fabricated
RED or production fix is claimed.

## Next useful action and self-check

Plan04 implements general raw A/V capture and transactional no-clobber files.
Both commits are reachable, all required test files exist, exact inventory and
source guard pass. Full review/qualification remains05. Oracle/VID-04, rights,
release and expired-window stops persist; no user action is needed.

## Self-Check: PASSED
