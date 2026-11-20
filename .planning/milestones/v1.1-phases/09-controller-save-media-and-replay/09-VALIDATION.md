---
phase: "09"
slug: "controller-save-media-and-replay"
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-11-19"
---

# Phase 9 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution. The authoritative requirement-to-test map, with the independent expected-value source and wrong-value control for each row, is the `## Validation Architecture` section of `09-RESEARCH.md`; the table below is derived from it row for row as a MAPPING of requirement to owning case labels (several labels are cases inside a consolidated CTest suite name per the 45-name budget; each plan's SUMMARY records the mapping); it holds no executable commands.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | C17 executables on `tests/t64_test.h`, CTest (labels unit, controls, integration, harness, repo, gov), Python harness tests under `scripts/tests/` |
| **Config file** | `CMakePresets.json` (`gate`: Ninja, Release, `T64_WERROR=ON`, jobs 4, `noTestsAction: error`; `asan`: Debug, ASan+UBSan) |
| **Quick run command** | `<built binary> --case <name>` then `python3 -B scripts/check-phase09-cases.py --build build/gate --suite <suite> --run --artifact-prefix <plan-tag>` (created in 09-02) |
| **Full suite command** | `sh scripts/gate.sh` (full local gate at the end of 09-03, 09-03.1, 09-06 and the 09-10 rehearsal; exact-SHA native gate in 09-11); ASan: `ctest --preset asan` |
| **Estimated runtime** | focused plan regex under 60 s; full gate about 130 s Linux and 275 s macOS warm cache (cold macOS 461-507 s) |

---

## Sampling Rate

- **After every task commit:** the owning `--case` binaries (seconds) and the inventory check for registered names
- **After every plan wave:** focused CTest regex through the preset, ASan selection for core changes, old-profile pins and refusals
- **Before `/gsd-verify-work`:** native normal, ASan/UBSan, gcc:13 and both hosted jobs green on one SHA (09-11)
- **Max feedback latency:** 60 seconds for the focused regex; the full local gate runs at the end of 09-03, 09-03.1, 09-06 and 09-10 (rehearsal) and the exact-SHA native gate in 09-11; one non-qualifying hosted rehearsal runs after the 09-10 fix cycle
- Cheap-first order every plan: fixture identity and rights, claims check, `ast.parse`, PII and rights scans; gcc:13 `-Werror` on changed units; one accepted operation through the real runner; then `--case`, CTest regex, ASan selection, old-profile pins.

---

## Per-Task Verification Map

| Task ID | Plan | Requirement | Threat Ref | Secure Behavior / Behavior | Test Type | Owning cases (mapping only; the executable commands are each plan's `<automated>` blocks and use the registered suite names) | File Exists | Status |
|---------|------|-------------|------------|----------------------------|-----------|-------------------|-------------|--------|
| 9-V01 | 09-03 | SAVE-01 | RESEARCH §Security Domain | SI RD64B store needs 2 extra events, WR64B 1; 8-byte store defined; atomic `QUEUE_FULL`/`SEQUENCE_EXHAUSTED`;  | unit | `unit.core.cpu.si-store-budget` | ❌ W0 | ⬜ pending |
| 9-V02 | 09-03 | SAVE-01 | RESEARCH §Security Domain | Register map, RD64B (PIF to RDRAM) versus WR64B (RDRAM to PIF) both directions, STATUS clears MI SI, PIF addre | unit | `unit.core.si.device` (cases `registers`, `status`, `pif-command` of `core/test_si`) | ❌ W0 | ⬜ pending |
| 9-V03 | 09-03 | SAVE-01 | RESEARCH §Security Domain | Execute tick E and completion C from literals | unit | `unit.core.si.device` (cases `dma-timing` of `core/test_si`) | ❌ W0 | ⬜ pending |
| 9-V04 | 09-03 | SAVE-01 | RESEARCH §Security Domain | Total over all 512-bit inputs; escapes; count masks; 16 single-button vectors; ports 1-3 and channel 4 without | unit | `unit.core.joybus.parser` (cases `frame-vectors`, `controller-responses`, `button-vectors`, `channels-and-ports`, `total-bounded` of `core/test_joybus`) | ❌ W0 | ⬜ pending |
| 9-V05 | 09-03 | SAVE-01 | RESEARCH §Security Domain | Strictly-after latch (X at 10 then Y at 10: sample at 10 sees old, 11 sees Y), whole versus split, later call  | unit | `unit.core.input.latch-partition`; `unit.core.si.device` (cases `refusals` of `core/test_si`); `unit.core.si.atomic-queue-full` | ❌ W0 | ⬜ pending |
| 9-V06 | 09-03 | SAVE-01 | RESEARCH §Security Domain | Authored ROM reads scheduled buttons/axes through the real runner at inputs E-1, E, E+1 | integration | `runner.pad-poll, controls.real.pad-wrong-value` | ❌ W0 | ⬜ pending |
| 9-V07 | 09-03 | SAVE-01 | RESEARCH §Security Domain | Collector/verifier on real hosted artifacts | unit | `harness.phase09-collect-real` | ❌ W0 | ⬜ pending |
| 9-V08 | 09-04 | SAVE-02 | RESEARCH §Security Domain | Selection is the profile; header byte `0x3F` nibble ignored; four ids | unit | `unit.core.save.device` (cases `selection` of `core/test_save`) | ❌ W0 | ⬜ pending |
| 9-V09 | 09-04 | SAVE-02 | RESEARCH §Security Domain | Exact lengths info(1,3)/read(2,8)/write(10,1); `0xFF` on channel 4 times out; 4 Kbit `block & 0x3F`; 16 Kbit 2 | unit | `unit.core.save.device` (cases `eeprom-commands`, `eeprom-blocks` of `core/test_save`) | ❌ W0 | ⬜ pending |
| 9-V10 | 09-04 | SAVE-02 | RESEARCH §Security Domain | Busy at `busy_until - 1`, idle at `busy_until`, idle after; write while busy replies `0x80`, window not extend | unit | `unit.core.save.device` (cases `eeprom-busy`, `eeprom-timing` of `core/test_save`) | ❌ W0 | ⬜ pending |
| 9-V11 | 09-04 | SAVE-02 | RESEARCH §Security Domain | `[0x08000000, 0x08008000)`; PI DMA both directions duration `1 + N/2`; effect atomic at completion; 32-bit ali | unit | `unit.core.save.device` (cases `sram-window`, `sram-dma` of `core/test_save`) | ❌ W0 | ⬜ pending |
| 9-V12 | 09-04 | SAVE-02/04 | RESEARCH §Security Domain | Real-runner round through authored ROMs on `-e4k`, `-e16k`, `-s32k` (K3 limits apply) | integration | `runner.save-fixtures` (cases `eeprom-4k-accepted`, `eeprom-16k-accepted`, `sram-accepted` of `runner/test_save_fixtures`); `controls.real.save-wrong-byte` | ❌ W0 | ⬜ pending |
| 9-V13 | 09-04 | SAVE-04 | RESEARCH §Security Domain | `SAVE_UNKNOWN_DEVICE`, `SAVE_UNSUPPORTED_DEVICE`, `SAVE_DEVICE_MISMATCH` and wire-level unsupported requests w | unit | `unit.core.save.device` (cases `refusals` of `core/test_save`) | ❌ W0 | ⬜ pending |
| 9-V14 | 09-05 | SAVE-03 | RESEARCH §Security Domain | RAW/SWAP16/WORDREV32; busy cleared; `write_generation` increments; time and queue unchanged | unit | `unit.core.save.import-export` | ❌ W0 | ⬜ pending |
| 9-V15 | 09-05 | SAVE-03 | RESEARCH §Security Domain | Check order table row by row; digest, bytes, deadline, counters, trace unchanged; too-small export reports req | unit | `unit.core.save.import-export` (cases `refusal-matrix`, `no-caller-read`, `export-refusals`, `mutation-bounded` of `core/test_save_transfer`) | ❌ W0 | ⬜ pending |
| 9-V16 | 09-05 | SAVE-03 | RESEARCH §Security Domain | Runner fd import then export via publisher; refused import | integration | `runner.save-roundtrip` (cases `sram-roundtrip`, `import-refused`, `upload-allowlist-save` of `runner/test_save_roundtrip`) | ❌ W0 | ⬜ pending |
| 9-V17 | 09-01 | CORE-02 | RESEARCH §Security Domain | Closed `(device, kind)` table equals the reservation sites plus SI; pairwise ties in both insertion orders on  | unit | `unit.core.sched.ledger, unit.core.sched.characterize-fifo` | ❌ W0 | ⬜ pending |
| 9-V18 | 09-01 | CORE-02 | RESEARCH §Security Domain | Q != 1 and ratio != 1:1 refused with allocator counters unchanged; Q=1 accepted; hash changes when only quantu | unit | `unit.core.profile.quantum-identity` | ❌ W0 | ⬜ pending |
| 9-V19 | 09-06 | CORE-02 | RESEARCH §Security Domain | Whole versus splits at {T-1, T, T+1} and steps {1,3,7,4096,one-shot}; 2 and 3 interleaved machines (one other  | unit | `unit.core.sched.matrix` (cases `cross-device`, `partition`, `interleave`, `resource-limit-resume`, `mi-producers` of `core/test_sched_matrix`) | ❌ W0 | ⬜ pending |
| 9-V20 | 09-01 | CORE-04 | RESEARCH §Security Domain | Legacy digests byte-identical through the sink; v25 equals an independent reference encoder; stripped v25 stre | unit | `unit.core.digest.sink-legacy-identity, unit.core.digest.v25-reference, unit.core.digest.v25-strip-differential` | ❌ W0 | ⬜ pending |
| 9-V21 | 09-08 | CORE-04 | RESEARCH §Security Domain | Container fields and sections decoded by an independent reference decoder with literal offsets; export does no | unit | `unit.core.state.export` (cases `export-layout`, `idempotence-shape`, `non-interference` of `core/test_state_export`); `runner.state-export` (cases `export-size-cap`, `export-fsize-at-state-size` of `runner/test_state_runner`) | ❌ W0 | ⬜ pending |
| 9-V22 | 09-09 | CORE-04 | RESEARCH §Security Domain | T1-T4; per-boundary "reached" assertion; both drop-observers outcomes; structure-aware mutants that keep magic | unit | `unit.core.state.import` (cases `roundtrip`, `idempotence`, `continuation-partitions`, `boundary-reached`, `boundary-roundtrip`, `boundary-continuation`, `boundary-perturbation`, `boundary-event-rules`, `mid-tick-resume`, `stalled-observer-stop`, `drop-observers-outcomes`, `binding-refusals`, `container-refusals`, `field-range-refusals`, `invariant-refusals`, `refusal-untouched`, `mutant-inventory`, `mutant-refusals`, `mutation-sweep`, `fuzz-campaign` of `core/test_state_import`); `harness.state-field-use-audit`; `runner.state-export` (cases `import-refused-integrity`, `import-refused-build`, `import-option-refusals`, `import-bound-below-tick` of `runner/test_state_runner`) | ❌ W0 | ⬜ pending |
| 9-V23 | 09-07 | CORE-05 | RESEARCH §Security Domain | Closed keys, caps, `at` ordering, identity binding, divergence report | unit | `unit.replay.suite` (cases `format-roundtrip`, `mutation-matrix`, `partition`, `at-matters`, `independent-original-run`, `queue-full-divergence` of `runner/test_replay`); `runner.replay-mix` | ❌ W0 | ⬜ pending |
| 9-V24 | 09-07 | CORE-05 | RESEARCH §Security Domain | Each mismatch refused before guest execution with a named field | integration | `runner.replay-mix` (cases `mismatch-profile`, `mismatch-build`, `mismatch-content` of `runner/test_replay`) | ❌ W0 | ⬜ pending |
| 9-V25 | 09-02/11 | Harness | RESEARCH §Security Domain | Mechanism and gates | unit | `harness.fsize-artifact-real-fault`; `harness.fsize-report-capture-still-bounded`; `runner.artifact-publisher`; `harness.phase09-stub-fails-closed`; `gov.rights.selftest`; `harness.upload-allowlist-artifacts`; `runner.profile-table-id-length`; `harness.phase09-qualification` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `docs/spec/phase09-cases.json` and `scripts/check-phase09-cases.py` (copy of the audio checker) before the first new suite
- [ ] `core/tests/test_digest_v25.c`, the predicate golden matrix and the membership-site list (09-01)
- [ ] `runner/tests/test_artifact_publisher.c`, the real-mode test fault, `scripts/tests/test_phase09_qualification.py` (09-02)
- [ ] Admit ROM source and build script (09-02); fixture sources for `pad`, `save` and `replay` when their plans start
- [ ] A profile-hash print helper for re-pinning runner rows

*No new framework: CTest, the T64 macros and python3 already exist.*

---

## Manual-Only Verifications

All phase behaviors have automated verification. The EEPROM, SI and joybus timing policies are labeled software-profile policy; a hardware measurement would be an owner stop and is not planned.

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency under 60 s for the focused regex
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending (execution evidence required; no hardware approval implied)
