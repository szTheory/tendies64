---
phase: "08"
slug: "audio-interface-and-raw-capture"
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-11-18"
---

# Phase08 validation strategy

## Test Infrastructure

Existing C17/CMake/CTest with gate and ASAN presets; no install or dependency.
Quick run: `python3 -B scripts/check-audio-cases.py --build build/gate --suite
tracer --run --artifact-prefix 08-01-tracer`, then `--suite profile` with a
distinct artifact prefix. This new checker is created by08-01 Task1 before use;
it validates the nonempty exact dispatcher/CTest inventory and actual accepted
fetched-CPU/wrong-sample consumer checks.
Full qualification: `sh scripts/gate.sh`, complete owning ASAN inventory and
existing qualified GCC13 source route for affected C units through the new
`scripts/qualify-audio.py --native` owning wrapper, created by08-05 Task1.
No Phase08 test has run yet; new files/targets are pending creation.

## Sampling Rate

After each behavior task run its focused normal and instrumented checks where
applicable. After each wave run the complete affected Phase08 integration group
and relevant old-profile/device regressions. Before final qualification run cheap
actual setup, profile/device wiring, rights/schema/source/inventory and accepted
consumer controls. Review/fix first, then one cohesive full qualification; repeat
only affected checks after relevant changes, not passing suites for prose.
Tests run in background; preserve existing jobs4/twoCIjobs/ten-minute bounds.

## Per-Task Verification Map

Commands below use `python3 -B scripts/check-audio-cases.py --build build/gate
--suite SUITE --run --artifact-prefix PREFIX` unless stated otherwise. The
checker builds the owning targets, runs source/body/object inventory checks,
checks exact `--list-cases` output against `docs/spec/audio-cases.json`, preserves
LastTest around every discovery/run including failure, and writes a unique
absolute JUnit per invocation. All listed targets/cases are pending creation.

| Plan/task exact name | Requirement | Suite / prefix | Exact owning CTest names | Creation dependency / threat |
| --- | --- | --- | --- | --- |
|08-01.1 Prove fetched PCM across two DMAs in the real runner | AUD-01/02/03 | tracer /08-01-tracer | unit.core.ai.tracer; runner.audio-pcm; controls.real.audio-wrong-sample | Same task creates checker, fixture, targets, inventory and tests; T-08-01–04 |
|08-01.2 Prove composite inheritance and old profile identity | AUD-01/03 | profile /08-01-profile | unit.core.ai.profile |08-01.1; T-08-04 |
|08-02.1 Complete register queue and MI transition behavior | AUD-01/03 | registers /08-02-registers | unit.core.ai.registers |08-01 and same-task registration; T-08-05/07 |
|08-02.2 Prove partial DMA rational timing and active write policies | AUD-01/03 | cadence /08-02-cadence | unit.core.ai.cadence |08-02.1 registration; T-08-06/08 |
|08-03.1 Prove reserved packet backpressure atomic copy and canonical state | AUD-01/03 | output /08-03-output | unit.core.ai.output |08-02 and same-task registration; T-08-09–11 |
|08-03.2 Prove partition interleave and actual runner drain independence | AUD-02/03 | determinism /08-03-determinism | unit.core.ai.determinism; controls.real.audio-drain-cadence |08-03.1 and runner registration amendment; T-08-11/12 |
|08-04.1 Capture actual general video and audio packets transactionally | RUN-06/AUD-02/03 | capture /08-04-capture | runner.raw-capture |08-03 and same-task registration; T-08-13–17 |
|08-04.2 Prove no-clobber and failure behavior on supported hosts | RUN-06 | capture-errors /08-04-errors | controls.real.raw-capture-errors |08-04.1 registration; T-08-13–17 |
|08-05.1 Bind the exact authored denominator and adversarial evidence controls | all four | qualification /08-05-contract plus qualify-audio.py --selftest | harness.audio-qualification | Same task creates wrapper/selftests/registration and CI wiring; T-08-18–22 |
|08-05.2 Qualify the reviewed final source and report exact bounded support | all four | qualify-audio.py --native; --collect-hosted; --verify-evidence | Full existing normal/ASAN inventory, existing reviewed GCC13 route and both existing hosted jobs |08-05.1 plus independent source-review readiness; T-08-18–22 |

The first task's exact command starts with `cmake --preset gate && cmake
--build --preset gate` before the checker. The final qualification wrapper
owns `sh scripts/gate.sh`, the complete ASAN preset inventory and current
reviewed GCC13 route; it preserves logs/results and fails for unreached tails.
All high mitigation gaps block the owning scope; no such gap is accepted.

## Exact Case Dispatcher Map

`core/tests/test_ai.c` implements `--list-cases` and the core suite dispatchers;
`runner/tests/test_audio_capture.c` implements the real-runner case dispatcher
and `--list-cases`. Each suite's case list is exact, nonempty and ordered in the
pending `docs/spec/audio-cases.json`; unknown, omitted or duplicate names fail.

| Suite | Exact cases |
| --- | --- |
| tracer | core: cpu-two-dma-literal-pcm; runner: runner-literal-pcm, reached-wrong-sample |
| profile | new-profile-inherited-devices, old-profile-pins, descriptor-policy-mutations, old-profile-ai-refusals |
| registers | register-fields-and-readback, address-before-length, length-zero-and-maximum, rdram-span-endpoints, dma-two-entry-full, mi-full-falling-edge, status-ack-isolation, control-disable-enable, documented-boundary-policy |
| cadence | rational-before-on-after, partial-length-readback, inflight-source-mutation, active-rate-latch, bitrate-zero-and-ratio, same-tick-store-sample, pause-phase-resume, reset-owned-events, arithmetic-overflow-refusal |
| output | reserve-before-first-sample, output-full-same-event, drain-resume-exactly-once, audio-info-copy-atomic, copy-alias-and-overflow, canonical-producer-history |
| determinism | core: whole-versus-split, interleaved-machine-isolation, equal-tick-ai-vi-order, independent-audio-video-full; runner: runner-drain-pacing |
| capture | actual-av-bundle, general-frame-geometry, literal-pcm-files, manifest-exact-identities, capture-drain-resume |
| capture-errors | existing-file-directory-untouched, unsafe-target-parent, racing-finalization, interrupted-stage, output-cap-boundaries, short-write-and-eintr, io-write-close-fsync-rename-failures, missing-zero-truncated-output, host-error-guest-state |
| qualification | accepted-original-binding, changed-sample-rejected, wrong-profile-build-input-rejected, missing-zero-truncated-rejected, zero-or-omitted-case-resealed-rejected, stale-or-missing-command-result-rejected, raw-upload-rejected, pii-projection-rejected, hosted-platform-witness-required |

Source/body/composition mutation controls remain in the existing
`scripts/tests/test_rsp_no_hle.py` owner, extended in08-01 before a new production
unit is broadly tested. This includes AI register/sample/admission body changes
and unknown source/object controls. Evidence/inventory controls are created in
08-05 and must exercise the actual new verifier. No individual test result is
claimed yet.

## Focused Instrumented Checks

After each behavior task configure/build using `cmake --preset asan` and
`cmake --build --preset asan`, then run the same exact owning checker suite
with `--build build/asan` and a distinct `08-NN-SUITE-asan` artifact prefix.
Run cheap identity/source/profile/rights/accepted-consumer checks before these
longer commands. Reuse the unchanged instrumented build; do not rebuild/retest
passing broad scopes solely for documentation. Source/body mutation selftests
and final full gate/ASAN/GCC qualification remain required. CTest usesjobs4;
manual batches retain unique absolute JUnit and immediate owning LastTest.

## Flagged Assumptions and Preserved Stops

The edge report's AUD-02/AUD-03 categories remain labeled unclassified even
though their explicit resolved contracts are carried by01/03. Their execution
evidence is pending, not a planning pass. Descriptor-less recalled prohibitions
remain flagged-unverified for judgment review: no authored-to-hardware claim
upgrade, no ROM/raw upload and no bypass of existing oracle/private-window/
physical/paid/install/publication/release stops. No SPEC was supplied. All five
explicit resolved edge contracts are lifted into plan must-have truths.

## Wave0 Requirements

Implement the new fixture/AI/audio/capture test registrations and inventory rows
inside their owning vertical slices. Existing harness/build infrastructure is
available. No new test framework or standalone stub-only plan is required.

## Manual-Only Verifications

None intended. Unresolved hardware details are explicit software-profile policy
with authored witnesses, not a fabricated manual/silicon verification. Any actual
rights, physical, publication or release stop remains outside this test strategy.

## Validation Sign-Off

- [ ] Every task has an exact owning automated command or grounded creation dependency.
- [ ] No three consecutive behavior tasks lack automated feedback.
- [ ] New references are created in their declared wave before execution.
- [ ] No watch mode, silently empty discovery, or test over rebuilt owning binaries.
- [ ] Exact original LastTest retained before CTest/discovery and immediately afterward.
- [ ] Manual batches have unique absolute JUnit; return codes and failures retained.
- [ ] Final source/artifact identities and required completed scopes verified.
- [ ] nyquist_compliant becomes true only after reached execution evidence.

Approval: delegated planning strategy; execution evidence pending, no hardware approval.
