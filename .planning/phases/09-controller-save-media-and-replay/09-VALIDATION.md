---
phase: "09"
slug: "controller-save-media-and-replay"
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-11-19"
---

# Phase 9 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution. The authoritative requirement-to-test map, with the independent expected-value source and wrong-value control for each row, is the `## Validation Architecture` section of `09-RESEARCH.md`; the table below is derived from it row for row and is refined per task by the plans.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | C17 executables on `tests/t64_test.h`, CTest (labels unit, controls, integration, harness, repo, gov), Python harness tests under `scripts/tests/` |
| **Config file** | `CMakePresets.json` (`gate`: Ninja, Release, `T64_WERROR=ON`, jobs 4, `noTestsAction: error`; `asan`: Debug, ASan+UBSan) |
| **Quick run command** | `<built binary> --case <name>` then `python3 -B scripts/check-phase09-cases.py --build build/gate --suite <suite> --run --artifact-prefix <plan-tag>` (created in 09-02) |
| **Full suite command** | `sh scripts/gate.sh` (only at the end of 09-03, 09-06 and 09-11); ASan: `ctest --preset asan` |
| **Estimated runtime** | focused plan regex under 60 s; full gate about 130 s Linux and 275 s macOS warm cache (cold macOS 461-507 s) |

---

## Sampling Rate

- **After every task commit:** the owning `--case` binaries (seconds) and the inventory check for registered names
- **After every plan wave:** focused CTest regex through the preset, ASan selection for core changes, old-profile pins and refusals
- **Before `/gsd-verify-work`:** native normal, ASan/UBSan, gcc:13 and both hosted jobs green on one SHA (09-11)
- **Max feedback latency:** 60 seconds for the focused regex; the full gate is reserved for 09-03, 09-06 and 09-11
- Cheap-first order every plan: fixture identity and rights, claims check, `ast.parse`, PII and rights scans; gcc:13 `-Werror` on changed units; one accepted operation through the real runner; then `--case`, CTest regex, ASan selection, old-profile pins.

---

## Per-Task Verification Map

| Task ID | Plan | Requirement | Threat Ref | Secure Behavior / Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|-------------|------------|----------------------------|-----------|-------------------|-------------|--------|
| 9-V01 | 09-03 | SAVE-01 | RESEARCH §Security Domain | SI RD64B store needs 2 extra events, WR64B 1; 8-byte store defined; atomic `QUEUE_FULL`/`SEQUENCE_EXHAUSTED`;  | unit | `ctest --preset gate -R "unit.core.cpu.si-store-budget"` | ❌ W0 | ⬜ pending |
| 9-V02 | 09-03 | SAVE-01 | RESEARCH §Security Domain | Register map, RD64B (PIF to RDRAM) versus WR64B (RDRAM to PIF) both directions, STATUS clears MI SI, PIF addre | unit | `ctest --preset gate -R "unit.core.si.registers|unit.core.si.status|unit.core.si.pif-command"` | ❌ W0 | ⬜ pending |
| 9-V03 | 09-03 | SAVE-01 | RESEARCH §Security Domain | Execute tick E and completion C from literals | unit | `ctest --preset gate -R "unit.core.si.dma-timing"` | ❌ W0 | ⬜ pending |
| 9-V04 | 09-03 | SAVE-01 | RESEARCH §Security Domain | Total over all 512-bit inputs; escapes; count masks; 16 single-button vectors; ports 1-3 and channel 4 without | unit | `ctest --preset gate -R "unit.core.joybus.parser|unit.core.joybus.fuzz-bounded"` | ❌ W0 | ⬜ pending |
| 9-V05 | 09-03 | SAVE-01 | RESEARCH §Security Domain | Strictly-after latch (X at 10 then Y at 10: sample at 10 sees old, 11 sees Y), whole versus split, later call  | unit | `ctest --preset gate -R "unit.core.input.latch-partition|unit.core.si.refusals|unit.core.si.atomic-queue-full"` | ❌ W0 | ⬜ pending |
| 9-V06 | 09-03 | SAVE-01 | RESEARCH §Security Domain | Authored ROM reads scheduled buttons/axes through the real runner at inputs E-1, E, E+1 | integration | `ctest --preset gate -R "runner.pad-poll|controls.real.pad-wrong-value"` | ❌ W0 | ⬜ pending |
| 9-V07 | 09-03 | SAVE-01 | RESEARCH §Security Domain | Collector/verifier on real hosted artifacts | unit | `ctest --preset gate -R "harness.phase09-collect-real"` | ❌ W0 | ⬜ pending |
| 9-V08 | 09-04 | SAVE-02 | RESEARCH §Security Domain | Selection is the profile; header byte `0x3F` nibble ignored; four ids | unit | `ctest --preset gate -R "unit.core.save.selection"` | ❌ W0 | ⬜ pending |
| 9-V09 | 09-04 | SAVE-02 | RESEARCH §Security Domain | Exact lengths info(1,3)/read(2,8)/write(10,1); `0xFF` on channel 4 times out; 4 Kbit `block & 0x3F`; 16 Kbit 2 | unit | `ctest --preset gate -R "unit.core.save.eeprom|unit.core.save.eeprom-mirror-and-16k"` | ❌ W0 | ⬜ pending |
| 9-V10 | 09-04 | SAVE-02 | RESEARCH §Security Domain | Busy at `busy_until - 1`, idle at `busy_until`, idle after; write while busy replies `0x80`, window not extend | unit | `ctest --preset gate -R "unit.core.save.eeprom-busy-triple"` | ❌ W0 | ⬜ pending |
| 9-V11 | 09-04 | SAVE-02 | RESEARCH §Security Domain | `[0x08000000, 0x08008000)`; PI DMA both directions duration `1 + N/2`; effect atomic at completion; 32-bit ali | unit | `ctest --preset gate -R "unit.core.save.sram"` | ❌ W0 | ⬜ pending |
| 9-V12 | 09-04 | SAVE-02/04 | RESEARCH §Security Domain | Real-runner round through authored ROMs on `-e4k`, `-e16k`, `-s32k` (K3 limits apply) | integration | `ctest --preset gate -R "runner.eeprom-rw|runner.sram-rw|controls.real.save-wrong-byte"` | ❌ W0 | ⬜ pending |
| 9-V13 | 09-04 | SAVE-04 | RESEARCH §Security Domain | `SAVE_UNKNOWN_DEVICE`, `SAVE_UNSUPPORTED_DEVICE`, `SAVE_DEVICE_MISMATCH` and wire-level unsupported requests w | unit | `ctest --preset gate -R "unit.core.save.refusals"` | ❌ W0 | ⬜ pending |
| 9-V14 | 09-05 | SAVE-03 | RESEARCH §Security Domain | RAW/SWAP16/WORDREV32; busy cleared; `write_generation` increments; time and queue unchanged | unit | `ctest --preset gate -R "unit.core.save.import-export"` | ❌ W0 | ⬜ pending |
| 9-V15 | 09-05 | SAVE-03 | RESEARCH §Security Domain | Check order table row by row; digest, bytes, deadline, counters, trace unchanged; too-small export reports req | unit | `ctest --preset gate -R "unit.core.save.refusal-matrix|unit.save.mutation"` | ❌ W0 | ⬜ pending |
| 9-V16 | 09-05 | SAVE-03 | RESEARCH §Security Domain | Runner fd import then export via publisher; refused import | integration | `ctest --preset gate -R "runner.save-roundtrip|controls.real.save-import-refused|harness.upload-allowlist-save"` | ❌ W0 | ⬜ pending |
| 9-V17 | 09-01 | CORE-02 | RESEARCH §Security Domain | Closed `(device, kind)` table equals the reservation sites plus SI; pairwise ties in both insertion orders on  | unit | `ctest --preset gate -R "unit.core.sched.ledger|unit.core.sched.characterize-fifo"` | ❌ W0 | ⬜ pending |
| 9-V18 | 09-01 | CORE-02 | RESEARCH §Security Domain | Q != 1 and ratio != 1:1 refused with allocator counters unchanged; Q=1 accepted; hash changes when only quantu | unit | `ctest --preset gate -R "unit.core.profile.quantum-identity"` | ❌ W0 | ⬜ pending |
| 9-V19 | 09-06 | CORE-02 | RESEARCH §Security Domain | Whole versus splits at {T-1, T, T+1} and steps {1,3,7,4096,one-shot}; 2 and 3 interleaved machines (one other  | unit | `ctest --preset gate -R "unit.core.sched.cross-device|unit.core.sched.partition|unit.core.sched.interleave|unit.core.sched.resource-limit-resume|unit.core.sched.mi-producers"` | ❌ W0 | ⬜ pending |
| 9-V20 | 09-01 | CORE-04 | RESEARCH §Security Domain | Legacy digests byte-identical through the sink; v25 equals an independent reference encoder; stripped v25 stre | unit | `ctest --preset gate -R "unit.core.digest.sink-legacy-identity|unit.core.digest.v25-reference|unit.core.digest.v25-strip-differential"` | ❌ W0 | ⬜ pending |
| 9-V21 | 09-08 | CORE-04 | RESEARCH §Security Domain | Container fields and sections decoded by an independent reference decoder with literal offsets; export does no | unit | `ctest --preset gate -R "unit.core.state.export-layout|unit.core.state.idempotence-shape|unit.core.state.non-interference|runner.state-export|controls.real.state-export-fsize"` | ❌ W0 | ⬜ pending |
| 9-V22 | 09-09 | CORE-04 | RESEARCH §Security Domain | T1-T4; per-boundary "reached" assertion; both drop-observers outcomes; structure-aware mutants that keep magic | unit | `ctest --preset gate -R "unit.core.state.roundtrip|unit.core.state.idempotence|unit.core.state.continuation|unit.core.state.boundary-matrix|unit.core.state.stalled-observer-stop|unit.core.state.refusal|unit.core.state.mutation|harness.state-field-use-audit|controls.real.state-import-refused"` | ❌ W0 | ⬜ pending |
| 9-V23 | 09-07 | CORE-05 | RESEARCH §Security Domain | Closed keys, caps, `at` ordering, identity binding, divergence report | unit | `ctest --preset gate -R "unit.replay.format|unit.replay.mutation|unit.replay.partition|unit.replay.at-matters|unit.replay.independent-original-run|unit.replay.queue-full-divergence|runner.replay-mix"` | ❌ W0 | ⬜ pending |
| 9-V24 | 09-07 | CORE-05 | RESEARCH §Security Domain | Each mismatch refused before guest execution with a named field | integration | `ctest --preset gate -R "controls.real.replay-mismatch-profile|controls.real.replay-mismatch-build|controls.real.replay-mismatch-content"` | ❌ W0 | ⬜ pending |
| 9-V25 | 09-02/11 | Harness | RESEARCH §Security Domain | Mechanism and gates | unit | `ctest --preset gate -R "harness.fsize-artifact-real-fault|harness.fsize-report-capture-still-bounded|runner.artifact-publisher|harness.phase09-stub-fails-closed|harness.rights-scan-state-replay|harness.upload-allowlist-artifacts|runner.profile-table-id-length|harness.phase09-qualification"` | ❌ W0 | ⬜ pending |

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
