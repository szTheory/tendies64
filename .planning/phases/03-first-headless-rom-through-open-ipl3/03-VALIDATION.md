---
phase: "03"
slug: first-headless-rom-through-open-ipl3
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-11-10"
---

# Phase 3 — Validation Strategy

This is a planning contract. No Phase 3 behavior has passed yet. The planner
must replace provisional contract rows with exact task/wave ownership and
immediate automated verification/fails-when pairs before execution.

## Test Infrastructure

| Property | Value |
|---|---|
| Framework | Existing CTest and authored C assertions; no test framework installation |
| Configuration | CMakeLists.txt, CMakePresets.json, scripts/gate.sh, exact test inventory and core lint |
| Quick check | Owning named CTest with `--no-tests=error --output-on-failure` after its target and runtime producer dependencies build |
| Full gate | `./scripts/gate.sh`, existing Linux/GCC and macOS/Clang jobs |
| Sanitizers | Full all-target ASan/UBSan build; current local host uses `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` |
| Runtime | Baseline hosted execution measured 38s Linux /58s macOS, excluding queue. New CPU/fixture cost unmeasured; keep early slices bounded and measure. |

## Sampling Rate

- Build and run the owning named behavioral test after relevant changes; compile
  all producer/checker dependencies of consumer tests.
- Run the full gate at meaningful integration/wave boundaries and before final
  verification; preserve all baseline registrations. Repeat only after relevant
  changes or unresolved failures warrant it.
- Exercise the second hosted compiler early in the first buildable slice; review
  every hosted run's measured queue/setup/build/test cost.
- Keep apps and tests in the background/headless by default. A timeout does not
  justify taking focus or removing an explicit resource/no-retry limit.

## Provisional Contract Verification Map

The 17 identities below are proposals from research; planner/task ownership
must finalize them. Do not accept a count-only inventory or silently drop any
of the existing 73 gate /75 sanitizer registrations. No pass is claimed.

| CTest identity | Required behavior | Current status |
|---|---|---|
| `unit.core.boot-entry` | HLE profile/seed identity, artifact digest check, DMEM copy and exact CPU entry. | Pending; created with owning feature |
| `unit.core.cpu-path` | Qualified instruction decode/retirement, delay slots, virtual KSEG mapping and explicit unsupported stop. | Pending; created with owning feature |
| `unit.core.cp0-cache` | Reachable CP0 masks/state and CACHE semantics under the named software policy. | Pending; created with owning feature |
| `unit.core.warm-device-policy` | RI preset, absent-memory probe, zero-source DMA and reset-register subset. | Pending; created with owning feature |
| `unit.core.isviewer` | Magic/buffer/lane/count/partial/repeated/overflow semantics and bounded drain. | Pending; created with owning feature |
| `runner.result-v2` | Strict real v2, dual-read v1, unknown-field/version rejection, six outcomes and independent golden. | Pending; created with owning feature |
| `runner.fixture-descriptor` | Admitted bytes come from verified unlinked descriptor; source replacement cannot alter guest input/result digest. | Pending; created with owning feature |
| `integration.open-ipl3-hello` | Actual pinned IPL3 and authored hello execute; assert CPU milestones and exact ISViewer bytes. | Pending; created with owning feature |
| `integration.open-ipl3-determinism` | Same process with different advance partitions and trace on/off yields same guest output/state. | Pending; created with owning feature |
| `controls.real.empty-halt` | Empty halted guest is boot-fail, never pass. | Pending; created with owning feature |
| `controls.real.empty-bound` | Empty output at guest bound is boot-fail, not timeout/pass. | Pending; created with owning feature |
| `controls.real.wrong-transcript` | Wrong or incomplete non-empty output is guest-fail/timeout by contract, never pass. | Pending; created with owning feature |
| `controls.real.unsupported-opcode` | Unsupported instruction stops explicitly and cannot pass. | Pending; created with owning feature |
| `controls.real.output-overflow` | Truncated/lost capture is rejected as pass. | Pending; created with owning feature |
| `controls.real.source-replaced` | Replace original fixture after supervisor verification; descriptor bytes/digest remain the admitted original. | Pending; created with owning feature |
| `controls.real.identity-mismatch` | Altered IPL3/tool/fixture/profile identity fails before or during admission as specified. | Pending; created with owning feature |
| `controls.real.input-partition` | Same canonical timed schedule is traced at the same guest ticks under partition/trace toggles. | Pending; created with owning feature |

## Requirement Coverage

| Requirement | Automated proof |
|---|---|
| BOOT-01 | Actual owned IPL3 copy/CPU entry, verified instruction/deadline semantics, observed IPL3-to-hello milestones; explicit unsupported stop |
| BOOT-02 | Source/binary rights and maintenance record, exact IPL3 size/SHA256/MD5; altered/truncated/missing/mismatched asset refusal |
| BOOT-03 | Boot-limits/claim checks; prepared entry and source-build limits named; protected asset and overclaim controls |
| BOOT-04 | Original assembly/tool/packer provenance, exact captured hello bytes, actual execution and negative transcript/overflow cases |
| RUN-01 | Fixed reset, profile/input/content identity, bounded canonical timed schedule, guest-time stop, strict real v2 result |
| RUN-03 | Real empty output at halt AND bound yields boot-fail, never pass/timeout |
| RUN-05 | Magic, 512-byte buffer, word/halfword byte lanes, exact unpadded 0/1/3/4/512-byte flushes, oversize/repeated/error behavior |
| PROF-05 | Same fixture/profile/input/bound yields equal state/log evidence across both compilers/OSes, interleave, partition and trace variations |

## Critical Negative Controls and Ownership

- Source replacement synchronized after supervisor verification cannot change
  runner-admitted bytes or their digest. Copy/hash/open/descriptor/schedule errors
  refuse launch; every exit closes/unlinks/reaps owned resources.
- Wrong text, no output, unsupported instruction, output overflow, invalid v2,
  unknown fields/version/identity, host wall kill/crash and report/capture failures
  never pass. Preserve legacy v1 goldens and native-model/guest separation.
- Independent expected ISA/bus/device/output values cite primary hardware facts
  or labeled software/model contracts; matching this emulator's output is not an
  independent hardware oracle.
- Existing sanitizer child must still demonstrate active fault detection. New
  CPU/bus/output/descriptor code receives relevant sanitizer checks; retain the
  established full admission campaign and its existing bounds when affected.
- Rights/PII/private-path checks run before evidence publication. No manual bytes,
  protected assets, raw host captures or unmanifested binaries enter repo/CI.

## Wave 0 and Task Ownership

Existing CTest infrastructure is available. New tests are authored and registered
in the same slice as their behavior, with deliberate failing controls. The
planner must fill task/wave/requirement/threat ownership and executable commands;
a scaffold-only setup plan is insufficient. Tool/disassembly qualification must
precede the bounded CPU opcode inventory and implementation.

## Manual-Only Verifications

No required physical, UI or owner-held-file check is planned. Broad judgment
prohibitions remain prominently flagged/non-authoritative under the autonomous
soft-gate policy. Any concrete rights, authority or missing-evidence stop is kept.

## Validation Sign-Off

- [ ] Every executable task has immediate automated verify and fails-when
- [ ] Every required behavior/control has task/wave/threat ownership
- [ ] No three consecutive tasks lack executable verification
- [ ] Exact baseline plus added test registrations are enforced
- [ ] First actual IPL3/hello execution and strict guest-result path pass
- [ ] Current-source sanitizers and negative control pass
- [ ] Existing hosted Linux/GCC and macOS/Clang gates pass same inputs
- [ ] Nyquist coverage audited after implementation; no planned test counted as passed

**Approval:** pending implementation and review; no routine owner acknowledgment.
