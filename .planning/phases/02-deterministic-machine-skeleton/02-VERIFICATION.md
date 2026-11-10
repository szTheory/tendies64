---
phase: 02-deterministic-machine-skeleton
verified: 2026-11-10T23:34:18Z
status: gaps_found
score: "39/40 plan truths; roadmap criteria 6/6"
covered_files:
  - .planning/phases/02-deterministic-machine-skeleton/02-01-PLAN.md
  - .planning/phases/02-deterministic-machine-skeleton/02-01-SUMMARY.md
  - .planning/phases/02-deterministic-machine-skeleton/02-02-PLAN.md
  - .planning/phases/02-deterministic-machine-skeleton/02-02-SUMMARY.md
  - .planning/phases/02-deterministic-machine-skeleton/02-03-PLAN.md
  - .planning/phases/02-deterministic-machine-skeleton/02-03-SUMMARY.md
  - .planning/phases/02-deterministic-machine-skeleton/02-04-PLAN.md
  - .planning/phases/02-deterministic-machine-skeleton/02-04-SUMMARY.md
  - .planning/phases/02-deterministic-machine-skeleton/02-05-PLAN.md
  - .planning/phases/02-deterministic-machine-skeleton/02-05-SUMMARY.md
  - .planning/phases/02-deterministic-machine-skeleton/02-06-PLAN.md
  - .planning/phases/02-deterministic-machine-skeleton/02-06-SUMMARY.md
  - .planning/phases/02-deterministic-machine-skeleton/02-07-PLAN.md
  - .planning/phases/02-deterministic-machine-skeleton/02-07-SUMMARY.md
  - .planning/phases/02-deterministic-machine-skeleton/02-08-PLAN.md
  - .planning/phases/02-deterministic-machine-skeleton/02-08-SUMMARY.md
  - CMakeLists.txt
  - core/CMakeLists.txt
  - core/include/tendies64/t64_machine.h
  - core/t64_bus.c
  - core/t64_digest.c
  - core/t64_internal.h
  - core/t64_machine.c
  - core/t64_mi.c
  - core/t64_pi.c
  - core/t64_rom.c
  - core/t64_scheduler.c
  - core/t64_sp.c
  - core/t64_trace.c
  - core/tests/fuzz_rom_admission.c
  - core/tests/sanitizer_fault.c
  - core/tests/test_bus.c
  - core/tests/test_determinism.c
  - core/tests/test_digest_trace.c
  - core/tests/test_lifecycle.c
  - core/tests/test_pi_dma.c
  - core/tests/test_profile.c
  - core/tests/test_rom_admission.c
  - core/tests/test_scheduler.c
  - core/tests/test_sp_dma.c
  - core/tests/test_tracer.c
  - docs/ci-reviews/LOG.md
  - docs/evidence/phase02-core.md
  - docs/profile-limits.md
  - docs/spec/machine-contract.md
  - docs/spec/result-protocol.md
  - runner/CMakeLists.txt
  - runner/t64_model_result_check.c
  - runner/t64_skeleton_probe.c
  - runner/tests/test_model_result.c
  - runner/tests/test_skeleton_probe.c
  - scripts/check-test-inventory.sh
  - scripts/expect-sanitizer.sh
  - scripts/gate.sh
  - tools/common/CMakeLists.txt
  - tools/lint/core-lint.sh
covered_digest: "v4:sha256:a83e4c264db34e131dea55389d9d43f17638b9620d0fbd091349ca831950817a"
behavior_unverified: 0
overrides_applied: 0
decision_coverage:
  honored: 14
  total: 14
  not_honored: []
gaps:
  - truth: "The existing gate passes on both hosted operating-system/toolchain lanes for the exact phase source."
    status: failed
    reason: "Run 42923015327 at 32de4f7b664c5e8083ff22a44bda8a46f67d88c6 fails compiling runner/tests/test_skeleton_probe.c on Linux GCC 13.3 with -Werror=format-truncation. CTest never starts on Linux; macOS passes 73/73."
    artifacts:
      - path: "runner/tests/test_skeleton_probe.c"
        issue: "Unchecked snprintf joins at lines 541-547 append suffixes to a 512-byte buffer, and GCC cannot prove the result fits."
    missing:
      - "Bound or safely join the test paths, then obtain a passing Linux hosted gate on the corrected exact source."
deferred:
  - truth: "CORE-02 device execution beyond the Phase 2 scheduler skeleton (CPU/RSP instruction execution, RDP, VI, AI, and SI)."
    addressed_in: "Phases 4, 5, 7, 8, and 9"
    evidence: "ROADMAP goals/success criteria assign CPU to Phase 4, RSP to Phase 5, RDP/VI to Phase 7, audio to Phase 8, and controller/SI to Phase 9."
unverified_prohibitions:
  - statement: "The canonical core must not acquire host clocks, files, entropy, threads, physical input, presentation, or mutable global machine state."
    verdict: "No violation observed in the production core; the determinism lint is wired into the gate."
    flag: "unverified-prohibition — human review recommended; judgment-tier, non-authoritative check."
  - statement: "Tests and admission must not introduce protected ROM, PIF, IPL, firmware, save, commercial-media or unlicensed copied-source material."
    verdict: "No protected binary fixture is present; the manifest retains pending/forbidden assets out of CI and the hosted rights/provenance checks pass on macOS."
    flag: "unverified-prohibition — human review recommended; judgment-tier, non-authoritative check."
  - statement: "Model, fuzz, digest, or probe success must not become a boot, hardware-accuracy, broad-compatibility, or memory-safety claim."
    verdict: "No overclaim observed; the model protocol and profile limits label the evidence as synthetic/native model evidence."
    flag: "unverified-prohibition — human review recommended; judgment-tier, non-authoritative check."
---

# Phase 2: Deterministic Machine Skeleton Verification Report

**Phase Goal:** A machine instance with a named profile advances in guest time over the memory map, RDRAM, MI, PI and SP DMA, and produces the same state digest whether it runs alone or interleaved with another instance.

**Verified:** 2026-11-10T23:34:18Z  
**Status:** gaps_found  
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| # | Roadmap success criterion | Status | Evidence |
|---|---|---|---|
| 1 | The named first profile fixes NTSC, cartridge, 4 MiB RDRAM, the intended open-IPL3 route, and CPU:RSP quantum in its profile hash; results identify profile and build; limits state promotion evidence. | ✓ VERIFIED | `core/t64_machine.c` defines and serializes the full descriptor; `core/tests/test_profile.c` checks profile fields, changed-field hashing, identity separation, and country independence; `runner/t64_skeleton_probe.c` emits a strict `t64.model-result` record; `docs/profile-limits.md` labels the target unqualified and lists limits. The successful macOS `unit.core.profile-result` and `unit.core.profile` checks are in run [42923015327](https://github.com/szTheory/tendies64/actions/runs/42923015327). |
| 2 | Guest-time, instruction-count, and frame-event bounds stop with the correct reason; isolated/interleaved instances agree. | ✓ VERIFIED | `core/t64_scheduler.c` owns ordered dispatch and all stop modes. `core/tests/test_scheduler.c` checks bounds and precedence; `core/tests/test_determinism.c` compares isolated, interleaved, one-shot, and partitioned device workloads. MacOS hosted `unit.core.scheduler` and `unit.core.determinism` passed. Counter events are synthetic; no CPU or frame execution is claimed. |
| 3 | PI/SP transfers occur as scheduled events under the documented model policies; MI masks remain separate from pending state. | ✓ VERIFIED | PI and SP requests reserve completion events in `core/t64_pi.c` and `core/t64_sp.c`; `core/t64_bus.c` routes register access; `core/t64_mi.c` holds pending and masks separately. `unit.core.pi-dma`, `unit.core.sp-dma`, and `unit.core.bus` passed on macOS. Exact policy and source labels are in `docs/spec/machine-contract.md`. The timing values are synthetic model policy, not hardware timing. |
| 4 | Bounded ROM admission rejects malformed/oversized/unknown-order inputs without changing state; header country does not select TV or region. | ✓ VERIFIED | `core/t64_rom.c` bounds and normalizes the candidate before `core/t64_machine.c` commits it. `core/tests/test_rom_admission.c` checks refusal digests and metadata, all byte orders, and size boundaries; `core/tests/test_profile.c` checks country independence across explicit descriptors. `unit.core.rom-admission` passed on macOS; the ASan/UBSan admission campaign also passed locally. |
| 5 | Device-boundary traces carry guest ticks and do not alter the digest. | ✓ VERIFIED | `core/t64_trace.c` copies into a bounded per-instance ring; BUS, MI, PI, SP, scheduler, and admission paths call the observer. `core/tests/test_digest_trace.c` and `core/tests/test_determinism.c` check trace-on/off/overflow digest equality and visible drops; macOS `unit.core.digest-trace` and `unit.core.determinism` passed. |
| 6 | Production ROM admission is exercised by a bounded ASan/UBSan campaign covering header-field reads and a sanitizer negative control. | ✓ VERIFIED | `core/tests/fuzz_rom_admission.c` calls the public admission path. The final local ASan/UBSan CTest selection passed 14/14; `build/asan/Testing/Temporary/LastTest.log` records 250,000 mutations plus 148 fixed cases (250,148 calls) in 44.57 s, and the negative-control child was detected. This is bounded parser evidence, not a memory-safety proof. |

**Score:** 6/6 roadmap criteria verified. The 40 detailed plan predicates add implementation detail; 39/40 are verified. The sole failed predicate is the two-hosted-lanes completion requirement below.

### Detailed Plan Must-Have Coverage

| Plan | Truths | Status | Evidence |
|---|---:|---|---|
| 02-01 | 6/6 | ✓ VERIFIED | Public lifecycle/profile entry points, canonical profile serialization, build identity, SHA link, country handling, and profile-limits behavior are exercised by `unit.core.tracer` and `unit.core.profile`; code is in `core/t64_machine.c`, `core/t64_digest.c`, and `core/CMakeLists.txt`. |
| 02-02 | 5/5 | ✓ VERIFIED | Candidate normalization and transactional replacement/refusal paths are implemented in `core/t64_rom.c` and `core/t64_machine.c`; `unit.core.rom-admission` and `unit.core.lifecycle` passed. |
| 02-03 | 5/5 | ✓ VERIFIED | Queue ordering, exact stop rules, canonical state, and observer overflow/equality are implemented in `core/t64_scheduler.c`, `core/t64_digest.c`, and `core/t64_trace.c`; `unit.core.scheduler` and `unit.core.digest-trace` passed. |
| 02-04 | 4/4 | ✓ VERIFIED | The physical map, unsupported results, SP banks, and MI state are connected through `core/t64_bus.c` and `core/t64_mi.c`; `unit.core.bus` passed. |
| 02-05 | 5/5 | ✓ VERIFIED | PI address/length checks, reservation, delayed copy, cancellation tokens, pending IRQ, digest and trace behavior are in `core/t64_pi.c`; `unit.core.pi-dma` passed. |
| 02-06 | 5/5 | ✓ VERIFIED | SP row/skip/bank-wrap decode, completion-time source reads, active/pending promotion, refusal, and no synthetic SP IRQ are implemented in `core/t64_sp.c`; `unit.core.sp-dma` passed. |
| 02-07 | 4/4 | ✓ VERIFIED | The final-source sanitizer run exercised the production fuzzer and caught its deliberate fault; model-result parsing remains a separate strict consumer in `runner/t64_model_result_check.c`. |
| 02-08 | 5/6 | ✗ FAILED | The real model probe, legacy consumer refusals, cross-device determinism, and exact inventory gate are implemented and passed on macOS. The plan’s requirement that both hosted lanes pass is not met: Linux GCC fails while compiling `runner/tests/test_skeleton_probe.c`. |

### Seven Flagged Assumptions Reviewed

| Assumption | Finding | Evidence |
|---|---|---|
| CORE-01: one scheduler owns progress | Supported for this skeleton. Public advance enters `t64_scheduler_advance`; PI/SP completions reserve events there. Host bus operations are explicit out-of-band debug stimuli between advances. | `core/t64_machine.c`, `core/t64_scheduler.c`, `core/t64_pi.c`, `core/t64_sp.c`; scheduler and DMA tests. |
| CORE-02: only present devices have behavior | Supported and bounded. The implemented device paths are RDRAM, MI, PI, SP DMA, and synthetic stop counters; CPU/RDP/VI/AI/SI execution is not present or claimed. | `core/t64_bus.c`, `docs/profile-limits.md`, `docs/spec/machine-contract.md`. |
| CORE-06: meaningful implemented boundaries use a copy-only observer | Supported for current paths. The ring makes no host callback and is called at machine, bus, MI, PI, SP, and scheduled-event boundaries. | `core/t64_trace.c`, its callers, and `core/tests/test_determinism.c`. |
| PROF-03: region consumers use the descriptor | Supported. Profile selection sets TV/region; the ROM parser preserves metadata but does not use country to choose behavior. | `core/t64_machine.c`, `core/t64_rom.c`, `core/tests/test_profile.c`, `docs/spec/machine-contract.md`. |
| PROF-06: writable machine state is per instance | Supported for current production code. Machine, RAM, device, queue, and observer state live in `t64_machine`; core lint rejects mutable globals; interleaved-instance digest tests pass. | `core/t64_internal.h`, `core/tests/test_determinism.c`, `tools/lint/core-lint.sh`. |
| BUS-01: this map is sufficient for the skeleton but not open-bus fidelity | Supported only as a scoped skeleton map. Known windows have checked behavior, unsupported windows return explicit outcomes, and documentation disclaims open-bus behavior. | `core/t64_bus.c`, `core/tests/test_bus.c`, `docs/spec/machine-contract.md`. |
| BUS-04: raw readable spans are a caller precondition | Supported as an explicit C API contract. The core checks declared length, pointer-range overflow and overlap, but cannot prove an arbitrary caller pointer is readable or that allocator capacity reports are truthful. | `core/include/tendies64/t64_machine.h`, `core/t64_rom.c`, `docs/spec/machine-contract.md`, `core/tests/test_lifecycle.c`. |

### Judgment-Tier Prohibitions

No violation was observed for the three kept prohibitions. They remain explicitly flagged as judgment-tier, non-authoritative checks; human review is recommended and none is silently treated as an enforced pass:

1. The core must not acquire host clocks, files, entropy, threads, physical input, presentation, or mutable global machine state. The production lint is wired into the gate and the source scan found no violation.
2. Tests and admission must not introduce protected content or incompatible copied source. No protected binary fixture is present; the rights and provenance checks pass on the successful macOS gate. The fixture manifest keeps unresolved external ROM rights pending and those assets out of public CI.
3. Native model, fuzz, digest, or probe results must not be presented as boot, hardware-accuracy, broad-compatibility, or memory-safety proof. The model-result schema, profile limits, and evidence docs state the narrower claims.

These three items remain `unverified-prohibition — human review recommended` because their authored verification tier is `judgment`. Their verdicts are non-authoritative.

## Required Artifacts

The GSD artifact query passed all 25 plan-declared artifacts for existence and substantive content. I traced wiring manually because its string matcher reports false negatives for ordinary C compilation and function-pointer dispatch.

| Artifact group | Status | Evidence |
|---|---|---|
| Public machine API and per-instance storage (`core/include/tendies64/t64_machine.h`, `core/t64_internal.h`) | ✓ VERIFIED | Public lifecycle/advance/admission contracts and owned machine state are used by production and tests. |
| Admission/profile/digest/scheduler/trace (`core/t64_machine.c`, `core/t64_rom.c`, `core/t64_digest.c`, `core/t64_scheduler.c`, `core/t64_trace.c`) | ✓ VERIFIED | Substantive implementations; CMake compiles `core/t64_*.c` into `t64_core`; public-entry tests reach them. |
| Bus and DMA (`core/t64_bus.c`, `core/t64_mi.c`, `core/t64_pi.c`, `core/t64_sp.c`) | ✓ VERIFIED | Device registers route through the bus; scheduled completions use reserved scheduler events and per-instance state. |
| Component and integration tests (`core/tests/test_*.c`, `core/tests/fuzz_rom_admission.c`) | ✓ VERIFIED | The macOS hosted gate runs all registered tests; final local ASan selection ran core tests, fuzz, and negative control. |
| Model producer/consumer (`runner/t64_skeleton_probe.c`, `runner/t64_model_result_check.c`, `runner/tests/test_skeleton_probe.c`, `runner/tests/test_model_result.c`) | ✓ VERIFIED | CMake links production producer/checker. Probe integration tests exercise actual emitted bytes and legacy rejection paths. |
| Gate and inventory (`scripts/gate.sh`, `scripts/check-test-inventory.sh`, `scripts/expect-sanitizer.sh`) | ✓ VERIFIED | The gate checks the exact test inventory before CTest; macOS gate completed. |
| Contract/evidence docs (`docs/profile-limits.md`, `docs/spec/machine-contract.md`, `docs/spec/result-protocol.md`, `docs/evidence/phase02-core.md`) | ✓ VERIFIED | Describe model limits, caller ownership, timing assumptions, real local measurements, and claim boundaries. |

## Key Link Verification

| From | To | Via | Status | Details |
|---|---|---|---|---|
| `t64_machine_admit_rom` | `core/t64_rom.c` | Candidate preparation before one commit | ✓ WIRED | Direct call in `core/t64_machine.c`; fuzzer invokes the public admission API. |
| `t64_machine_advance` | `core/t64_scheduler.c` | One per-instance bounded queue | ✓ WIRED | Public advance delegates to scheduler; tests observe time/counter stop behavior. |
| `core/t64_bus.c` | `core/t64_mi.c`, `core/t64_pi.c`, `core/t64_sp.c` | Register decode and device calls | ✓ WIRED | Read/write switch paths call the matching per-device handler. |
| PI/SP request handlers | Scheduler and completion handlers | Reserve completion slot, commit token event, dispatch function pointer | ✓ WIRED | `core/t64_pi.c` and `core/t64_sp.c` reserve actual future events; `core/t64_scheduler.c` calls the stored handler. |
| `runner/t64_skeleton_probe.c` | model checker and legacy consumers | Emitted record consumed by production checkers | ✓ WIRED | `unit.core.profile-result` validates the emitted model result and exercises legacy refusal paths; this named test passed in the hosted macOS run. |

## Data-Flow Trace (Level 4)

| Artifact | Data | Source | Produces real state/data | Status |
|---|---|---|---|---|
| `t64_skeleton_probe` | Profile/hash, content/state digests, guest tick, model assertion counts | Selected descriptor plus the machine APIs and canonical digest over owned state | Yes; it admits authored bytes, performs actual bus/DMA work, then serializes observations | ✓ FLOWING |
| PI completion | Destination RDRAM bytes | Machine-owned normalized ROM bytes read at scheduled completion | Yes; exact bytes are compared in tests | ✓ FLOWING |
| SP completion | SP bank or RDRAM bytes | Per-instance RAM/DMEM/IMEM read at scheduled completion | Yes; rows, skips, wrap, and completion-time source changes are exercised | ✓ FLOWING |

## Behavioral Spot-Checks

| Behavior | Command/evidence | Result | Status |
|---|---|---|---|
| Full existing gate on macOS arm64 / Apple Clang | Run 42923015327, `./scripts/gate.sh` | Build and all 73/73 tests passed; includes profile/result, scheduler, bus, PI, SP, digest/trace, determinism, inventory, rights and provenance checks | ✓ PASS |
| Full existing gate on Linux x86_64 / GCC 13.3 | Same exact run and SHA | Build stopped compiling `runner/tests/test_skeleton_probe.c`; warnings are errors; CTest did not run | ✗ FAIL |
| Final local ASan/UBSan selection | `ctest --preset asan ... -R '^(unit\.core\..*|unit\.model-result|fuzz\.rom-admission|sanitizer\.negative-control)$'` as recorded in `02-FINAL-LOCAL-EVIDENCE.md` and `LastTest.log` | 14/14 passed; fuzz executed 250,148 production admission calls in 44.57 s; separate negative control detected | ✓ PASS |

## Probe Execution

| Probe | Command/evidence | Result | Status |
|---|---|---|---|
| Production named-profile probe | Hosted `unit.core.profile-result` invokes `t64-skeleton-probe` as a child and validates its emitted record | Passed on the exact SHA in the successful macOS 73-test gate | ✓ PASS |

## Requirements Coverage

| Requirement | Source plan | Status | Evidence |
|---|---|---|---|
| CORE-01 | 02-01, 02-03, 02-05–08 | ✓ SATISFIED | Per-instance scheduler and interleaved/isolated tests. |
| CORE-02 | 02-03–08 | DEFERRED beyond this skeleton | Current queue orders synthetic counter events and implemented PI/SP paths. CPU/RSP execution, RDP, VI, AI, and SI device behavior is explicitly outside this phase and appears in later roadmap goals listed under Deferred Items. |
| CORE-03 | 02-03, 02-08 | ✓ SATISFIED for stop-bound contract | Guest tick is advanced directly; instruction/frame stop bounds are synthetic scheduler events, not instruction/frame execution. |
| CORE-06 | 02-03–08 | ✓ SATISFIED | Timestamped device-boundary observer and trace-on/off/overflow equality tests. |
| PROF-01 | 02-01, 02-07–08 | ✓ SATISFIED | Named descriptor and profile hash in real model-result output. |
| PROF-02 | 02-01, 02-07–08 | ✓ SATISFIED for native model results | The separate model-result schema requires profile hash/ID and actual build identity; legacy guest-result bytes remain unchanged. |
| PROF-03 | 02-01–02 | ✓ SATISFIED | Profile controls TV/region; country byte is metadata only. |
| PROF-04 | 02-01, 02-07–08 | ✓ SATISFIED | Profile-limits document has unsupported/deferred/unknown dispositions and promotion evidence. |
| PROF-06 | 02-02–03, 02-08 | ✓ SATISFIED | State is per instance; exact digests match isolated and interleaved runs. |
| BUS-01 | 02-04–08 | ✓ SATISFIED for documented map/MI skeleton | Checked RDRAM/SP/ROM/PI/MI map; unsupported and unmapped results are explicit. No open-bus fidelity is claimed. |
| BUS-02 | 02-05–08 | ✓ SATISFIED for named PI/SP model | Delayed copies, lengths/alignment, queues, cancellation and completion-time visibility are tested. Timing is model policy, not hardware evidence. |
| BUS-04 | 02-01–02, 02-07 | ✓ SATISFIED | Bounded production admission, three byte orders, refusal immutability, header-field fuzz, ASan/UBSan, and negative control. |

No Phase 2 requirement is orphaned from the plans. CORE-02's remaining device work is explicitly deferred to later roadmap phases; no CPU/boot/hardware-accuracy claim is made here.

### Decision Coverage

The decision-coverage gate reported 14/14 trackable CONTEXT decisions honored and none missing. The gate is non-blocking.

## Anti-Patterns Found

No unresolved TODO/FIXME/XXX markers or implementation stubs were found in the Phase 2 code paths. `XXXXXX` matches in tests/docs are `mkstemp`/`mktemp` filename templates, not placeholders. The Linux format warning is a real test build blocker, not a stub.

| File | Line | Pattern | Severity | Impact |
|---|---:|---|---|---|
| `runner/tests/test_skeleton_probe.c` | 541-547 | Potentially truncating `snprintf` path joins under GCC 13 | 🛑 Blocker | Linux gate stops before CTest; see gap above. |

## Human Verification Required

The three judgment-tier prohibitions above remain flagged for human review. No violation was observed, but the automated verifier cannot silently convert those non-authoritative judgments to verified prohibitions. The phase is already `gaps_found` because the hosted Linux gate failed.

## Gaps Summary

The machine skeleton, six roadmap success criteria, all current-scope behaviors, and final local sanitizer evidence are supported by code and passing behavioral checks. The exact-SHA hosted macOS lane passes 73/73 tests. Phase completion is blocked by one concrete portability issue: GCC 13 rejects the test's bounded-path construction under `-Werror=format-truncation`, so the Linux hosted gate never reaches CTest. No retry or source edit was made during verification. The current owner can fix the path construction and run the specified follow-up validation.

---

_Verified: 2026-11-10T23:34:18Z_  
_Verifier: gsd-verifier_
