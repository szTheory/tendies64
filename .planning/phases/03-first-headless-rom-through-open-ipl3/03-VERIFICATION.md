---
phase: 03-first-headless-rom-through-open-ipl3
verified: 2026-11-11T22:09:52Z
status: passed
score: 6/6 roadmap truths verified
covered_files:
  - .github/workflows/ci.yml
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-01-PLAN.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-01-SUMMARY.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-02-PLAN.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-02-SUMMARY.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-03-PLAN.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-03-SUMMARY.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-04-PLAN.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-04-SUMMARY.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-05-PLAN.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-05-SUMMARY.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-06-PLAN.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-06-SUMMARY.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-07-PLAN.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-07-SUMMARY.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-08-PLAN.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-08-SUMMARY.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-09-PLAN.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-09-SUMMARY.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-10-PLAN.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-10-SUMMARY.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-11-PLAN.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-11-SUMMARY.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-12-PLAN.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-12-SUMMARY.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-13-PLAN.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-13-SUMMARY.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-CI-REVIEW-SCOPE.json
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-EDGE-COVERAGE.json
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-PRECI-REVIEW-SCOPE.json
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-PRIMARY-EVIDENCE-INDEX.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-PROBE-LEDGER.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-REGRESSION-EVIDENCE.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-REVIEW-DISPOSITION.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-REVIEW-FIX.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-REVIEW.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-SECURITY.md
  - .planning/phases/03-first-headless-rom-through-open-ipl3/03-VALIDATION.md
  - build/evidence-run-43103669084/gate-linux.json
  - build/evidence-run-43103669084/gate-macos.json
  - core/CMakeLists.txt
  - core/include/tendies64/t64_machine.h
  - core/t64_bus.c
  - core/t64_cpu.c
  - core/t64_digest.c
  - core/t64_internal.h
  - core/t64_isviewer.c
  - core/t64_machine.c
  - core/t64_observe.c
  - core/t64_rom.c
  - core/t64_scheduler.c
  - core/t64_sp.c
  - core/t64_warm_devices.c
  - core/tests/test_boot_determinism.c
  - core/tests/test_boot_entry.c
  - core/tests/test_cpu_path.c
  - core/tests/test_isviewer.c
  - core/tests/test_tracer.c
  - core/tests/test_warm_devices.c
  - docs/evidence/phase3-ci.md
  - docs/evidence/zlib-host-prerequisite-audit.md
  - docs/spec/boot-limits.md
  - docs/spec/result-protocol.md
  - fixtures/boot-contract.json
  - fixtures/hello/hello.S
  - fixtures/hello/hello.ld
  - fixtures/manifest.json
  - runner/CMakeLists.txt
  - runner/t64_input.c
  - runner/t64_input.h
  - runner/t64_real.c
  - runner/t64_real_fixture.c
  - runner/t64_real_fixture.h
  - runner/t64_real_result.c
  - runner/t64_real_result.h
  - runner/t64_result_check.c
  - runner/t64_supervisor.c
  - runner/t64_verified_input.c
  - runner/t64_verified_input.h
  - runner/tests/test_fixture_descriptor.c
  - runner/tests/test_input.c
  - runner/tests/test_real_controls.c
  - runner/tests/test_real_runner.c
  - runner/tests/test_result_v2.c
  - scripts/build-boot-fixtures.sh
  - scripts/check-ci-yml.sh
  - scripts/check-test-inventory.sh
  - scripts/collect-boot-evidence.sh
  - scripts/compare-boot-evidence.sh
  - scripts/gate.sh
  - tools/fixture/t64_rom_pack.c
covered_digest: "v4:sha256:f78acaf9122d3550be1387c80bc3bf9d6e8f07ded32162c2d5e8d5e896a51901"
behavior_unverified: 0
overrides_applied: 0
---

# Phase 3: First Headless ROM Through Open IPL3 Verification

**Phase goal:** A hello ROM boots through HLE-PIF seeding and a pinned open IPL3 with no Nintendo boot bytes, prints through ISViewer, and the headless runner reports a pass with identical digests across platforms.

**Status:** `passed` — all six roadmap criteria are verified. No human verification item remains.

## Observable truths

| # | Roadmap criterion | Status | Code and evidence |
|---|---|---|---|
| 1 | Primary documents are indexed by SHA/source, and boot claims cite them or hardware-derived tests. | VERIFIED | [03-PRIMARY-EVIDENCE-INDEX.md](03-PRIMARY-EVIDENCE-INDEX.md) records source, digest and cited sections without storing the manuals. Boot policy and tests distinguish cited facts from software policy. |
| 2 | The headless hello path reports a machine-readable pass, expected ISViewer text, bounded guest time, and scheduled input trace events. | VERIFIED | `runner/t64_real.c` admits and executes the fixture, queues timed events and checks the transcript; `runner/tests/test_input.c:692-747` exercises a real boot with 128 scheduled events and checks ordered input traces, bound behavior and deterministic state. Exact 7c hosted gate passed. A supplemental nonempty `t64-supervisor --real` run returned `pass` with 127 events, input digest `763bda82…9bd6d`, and the expected 30-byte transcript, but that existing binary embeds source `d3ad1c8093fba2aa9b637d3cdaf74e244875346a`. The checked `d3ad..7c` source delta contains only `scripts/check-ci-yml.sh` and `scripts/collect-boot-evidence.sh`, with no core/runner changes; the exact 7c common gate also ran the actual input-trace control. The local run is supplemental evidence, not relabeled as 7c. |
| 3 | IPL3 identity, license and maintenance ownership are pinned; repository/CI gates exclude Nintendo boot bytes. | VERIFIED | `fixtures/boot-contract.json` records SHA-256, MD5, source revision and cleared Unlicense rights. `scripts/gate.sh` runs rights/fixture checks; CI publishes only sanitized result JSON. No Nintendo PIF/IPL/CIC bytes or raw manuals are in the reviewed source/evidence scopes. |
| 4 | Empty output is boot-fail; unmodeled boot behavior remains explicitly deferred. | VERIFIED | `runner/t64_result_check.c:501-518,1072-1107` enforces empty-output boot-fail and completion/bound rules. [boot-limits.md](../../../docs/spec/boot-limits.md) documents PIF challenge, CIC and timeout limits and does not claim “boot solved.” Negative real-runner controls are registered in the gate. |
| 5 | Linux/GCC and macOS/Clang produce identical guest identities and state digest for the same run. | VERIFIED | Actual pair: [gate-linux.json](../../../build/evidence-run-43103669084/gate-linux.json) and [gate-macos.json](../../../build/evidence-run-43103669084/gate-macos.json), selector `--sha 7c0ea9b5f3076122d506074aa7d3a2283cf9e2a6 --run 43103669084 --attempt 1`. Both jobs succeeded on attempt 1; fixture/profile/IPL3/payload/input/log/bound/state identities match. State digest is `2e336d7b…f2f7c`; compiler build IDs differ, as permitted. |
| 6 | The fixture digest represents the exact bytes admitted by the guest. | VERIFIED | `runner/t64_real_fixture.c` opens and validates the fixture descriptor; `runner/t64_real.c` passes that verified source into admission and checks the admitted content digest. `runner/tests/test_fixture_descriptor.c` covers replacement/refusal behavior. The hosted result binds fixture SHA `fa276cc1…f27c7c`. |

## Requirements coverage

| Requirement | Status | Evidence |
|---|---|---|
| BOOT-01 | SATISFIED | [t64_machine.c](../../../core/t64_machine.c#L467) copies the pinned payload into SP DMEM, seeds documented warm-entry registers and starts guest execution at `0xa4000040`; hosted milestones show IPL3 entry. Rights/gate checks cover absence of Nintendo boot bytes. |
| BOOT-02 | SATISFIED | [boot-contract.json](../../../fixtures/boot-contract.json#L9) pins revision, SHA-256, MD5, license and maintenance owner. |
| BOOT-03 | SATISFIED | [boot-limits.md](../../../docs/spec/boot-limits.md) records deferred boot behavior; result validation rejects empty output as pass. |
| BOOT-04 | SATISFIED | Actual hosted pair reports `pass`, `positive-match`, authored hello completion and the exact 30-byte log digest. |
| RUN-01 | SATISFIED | `runner/t64_real.c` wires profile, verified fixture, timed input and guest bound to the real runner; supplemental nonempty run and current-source input control provide schedule/trace evidence (source identities stated above). |
| RUN-03 | SATISFIED | [t64_result_check.c](../../../runner/t64_result_check.c#L501) and real empty-output controls classify an empty guest log as boot-fail. |
| RUN-05 | SATISFIED | [t64_isviewer.c](../../../core/t64_isviewer.c#L81) implements bounded byte capture and count-lane semantics; [boot-limits.md](../../../docs/spec/boot-limits.md) and result protocol document the interface; actual guest output is 30 bytes. |
| PROF-05 | SATISFIED | The exact run/attempt Linux/macOS pair has matching guest state digest and identities. |

No additional Phase 3 requirement is orphaned.

## Artifacts and wiring

The plan-declared artifact checks passed (84/84). The important links are present and exercised:

| From | To | Wiring evidence |
|---|---|---|
| Authored fixture builder and manifest | Verified descriptor loader | `scripts/build-boot-fixtures.sh` produces the manifest-bound image; runner descriptor tests reject replacement and invalid identity. |
| Timed schedule parser | Real runner and machine input queue | `runner/t64_real.c` parses, hashes and queues events; `runner/tests/test_input.c` observes scheduled ticks on the actual boot. |
| Machine bus | ISViewer capture and expected transcript | `core/t64_isviewer.c` captures bounded bytes; real runner compares expected bytes and digest before pass. |
| Real result | Closed result-v2 checker | `runner/t64_result_check.c` validates result semantics, milestones and empty-output failures. |
| Hosted gate jobs | Exact-run collector and comparator | `scripts/collect-boot-evidence.sh` and `scripts/compare-boot-evidence.sh` validate the two records and selectors. |

The generic key-link query did not recognize the CMake target indirection; the target registration and runtime paths were manually traced through `core/CMakeLists.txt` and `runner/CMakeLists.txt`. No broken or orphaned link was found.

## Behavioral and CI evidence

| Check | Result |
|---|---|
| Hosted actual pair, run `43103669084`, attempt `1`, SHA `7c0ea9b5f3076122d506074aa7d3a2283cf9e2a6` | Both `gate-linux` and `gate-macos` succeeded; actual result records show matching guest state and transcript. |
| `sh scripts/compare-boot-evidence.sh --sha 7c0ea9b5f3076122d506074aa7d3a2283cf9e2a6 --run 43103669084 --attempt 1 build/evidence-run-43103669084/gate-linux.json build/evidence-run-43103669084/gate-macos.json` | Passed exact selectors and guest identity/state comparison. |
| Supplemental nonempty real-runner invocation | Passed on existing build identity `t64-build-v1-7371840eaf653335c9ef7e27c1d9be18226e54085baa0f3a695fe6d6caf07bab`, source `d3ad1c8093fba2aa9b637d3cdaf74e244875346a`; 127-event input digest `763bda82…9bd6d`, tick `530438`, bound `530500`, expected output. This is not a 7c local build claim. |
| Probe discovery | No phase-declared or conventional `scripts/*/tests/probe-*.sh` probes exist. |
| Anti-pattern scan | No unresolved `TBD`/`FIXME`/`XXX`, TODO/HACK/placeholder, or empty implementation found in the explicit 43-file pre-CI plus 17-file CI review scopes. |

## Retained judgment flags and evidence limits

Three `verification: judgment` prohibitions remain visible as **unverified-prohibition — human review recommended** flags. Under `.planning/AUTONOMY.md`’s explicit autonomous soft-gate they are non-authoritative and do not create an acknowledgment gate. No human approval or risk acceptance is claimed:

1. Prepared warm-entry state or agreement with another emulator is not evidence of authentic IPL2/cold boot, hardware accuracy or broad compatibility.
2. Input trace proves scheduled event delivery, not guest-visible controller consumption.
3. Byte identity does not establish rights, and a downloaded/generated artifact does not establish reproducible source regeneration.

Five planning edge-classification flags remain in `03-EDGE-COVERAGE.json` and `03-PROBE-LEDGER.md`: `EDGE-03-03`, `EDGE-03-08`, `EDGE-03-09`, `EDGE-03-10` and `EDGE-03-14`. They were not removed or reclassified here; their corresponding empty-output, cross-host-state, boot-milestone, identity/rights and pass-eligibility behaviors have direct code, test or hosted-pair evidence in this report.

The supported claim is the named prepared warm profile only. This report does not claim authentic IPL2 or cold boot, controller consumption, broad game compatibility, source regeneration, or hardware accuracy. Manuals remain source/hash/section references only; no raw protected assets or public media are included. The zlib host-prerequisite review records a retrospective pre-use audit miss; it does not claim owner approval, and the security record lists no accepted risks. The full 93-registration ASan/UBSan suite is not claimed as rerun on exact source `7c0ea9b5`; earlier full-suite and later affected instrumented checks are recorded in the phase evidence. API archive identity is GitHub metadata; extracted JSON records were checked directly.

All four review findings are recorded fixed, and the security audit records 27/27 code mitigations closed. No unresolved implementation blocker was found. No probe or human verification remains.

---

_Verified: 2026-11-11T22:09:52Z — gsd-verifier_
