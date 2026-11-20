---
phase: 09-controller-save-media-and-replay
reviewed: 2026-11-20
depth: deep
files_reviewed: 105
files_reviewed_list:
  - .github/workflows/ci.yml
  - core/CMakeLists.txt
  - core/include/tendies64/t64_machine.h
  - core/t64_ai.c
  - core/t64_bus.c
  - core/t64_cpu.c
  - core/t64_digest.c
  - core/t64_internal.h
  - core/t64_isviewer.c
  - core/t64_joybus.c
  - core/t64_machine.c
  - core/t64_observe.c
  - core/t64_pi.c
  - core/t64_rsp.c
  - core/t64_save.c
  - core/t64_scheduler.c
  - core/t64_si.c
  - core/t64_state.c
  - core/t64_warm_devices.c
  - core/tests/fuzz_state_import.c
  - core/tests/state_import_helpers.h
  - core/tests/test_digest_v25.c
  - core/tests/test_joybus.c
  - core/tests/test_save.c
  - core/tests/test_save_transfer.c
  - core/tests/test_sched_ledger.c
  - core/tests/test_sched_matrix.c
  - core/tests/test_si.c
  - core/tests/test_si_profile.c
  - core/tests/test_state_export.c
  - core/tests/test_state_import.c
  - docs/claims/phase09.md
  - docs/evidence/phase07-oracle-authority.json
  - docs/evidence/phase09-first-hosted-pair.json
  - docs/spec/machine-contract.md
  - docs/spec/phase09-cases.json
  - docs/spec/profile-membership-sites.txt
  - docs/spec/raw-capture.md
  - docs/spec/replay.md
  - docs/spec/result-protocol.md
  - docs/spec/save-media.md
  - docs/spec/si-pif-controller.md
  - docs/spec/state-container.md
  - docs/spec/state-field-rules.txt
  - docs/spec/state-mutants.txt
  - fixtures/manifest.json
  - fixtures/pad/poll.S
  - fixtures/pad/poll.ld
  - fixtures/pad/poll_oracle.py
  - fixtures/phase09/admit.S
  - fixtures/phase09/admit.ld
  - fixtures/replay/mix.S
  - fixtures/replay/mix.input.json
  - fixtures/replay/mix.ld
  - fixtures/replay/mix_oracle.py
  - fixtures/save/eeprom.S
  - fixtures/save/eeprom.ld
  - fixtures/save/roundtrip.S
  - fixtures/save/roundtrip.ld
  - fixtures/save/roundtrip_oracle.py
  - fixtures/save/save_oracle.py
  - fixtures/save/sram.S
  - fixtures/save/sram.ld
  - fixtures/state/decode_container.py
  - runner/CMakeLists.txt
  - runner/t64_artifact_publisher.c
  - runner/t64_artifact_publisher.h
  - runner/t64_real.c
  - runner/t64_real_fixture.c
  - runner/t64_real_fixture.h
  - runner/t64_real_result.h
  - runner/t64_replay.c
  - runner/t64_replay.h
  - runner/t64_result_check.c
  - runner/t64_save_flags.h
  - runner/t64_supervisor.c
  - runner/t64_verified_input.h
  - runner/tests/replay_mix_golden.h
  - runner/tests/test_artifact_publisher.c
  - runner/tests/test_pad_poll.c
  - runner/tests/test_phase09_runner.c
  - runner/tests/test_replay.c
  - runner/tests/test_save_fixtures.c
  - runner/tests/test_save_roundtrip.c
  - runner/tests/test_state_runner.c
  - scripts/build-pad-fixture.sh
  - scripts/build-phase09-admit-fixture.sh
  - scripts/build-replay-fixture.sh
  - scripts/build-save-fixtures.sh
  - scripts/check-ci-yml.sh
  - scripts/check-phase09-cases.py
  - scripts/check-rdp-oracle-authority.py
  - scripts/check-rsp-no-hle.py
  - scripts/check-state-field-use.py
  - scripts/check-test-inventory.sh
  - scripts/collect-boot-evidence.sh
  - scripts/gate.sh
  - scripts/k5-mutants.py
  - scripts/list-membership-sites.py
  - scripts/qualify-phase09.py
  - scripts/qualify-rdp-vi.py
  - scripts/rights-scan.sh
  - scripts/tests/test_phase09_qualification.py
  - scripts/tests/test_rsp_no_hle.py
  - tools/rights/t64_blobscan.c
findings:
  critical: 5
  warning: 15
  info: 32
  total: 52
status: issues_found
---

# Phase 9 Code Review Report

Independent deep review in five lanes (gsd-code-reviewer agents, one per lane). Lane texts below are verbatim; the orchestrator only concatenated them and counted. Review base: HEAD 6963bf0 (commits after the Phase 9 source base cfe4d34).

## Scope manifest (executor-derived)

Derived from `git diff --name-only --diff-filter=AMR PHASE09_SOURCE_BASE..HEAD`, `.planning/` and `docs/ci-reviews/` excluded; every path is assigned to exactly one lane (shared headers in L1 and L2).

```
.github/workflows/ci.yml
core/CMakeLists.txt
core/include/tendies64/t64_machine.h
core/t64_ai.c
core/t64_bus.c
core/t64_cpu.c
core/t64_digest.c
core/t64_internal.h
core/t64_isviewer.c
core/t64_joybus.c
core/t64_machine.c
core/t64_observe.c
core/t64_pi.c
core/t64_rsp.c
core/t64_save.c
core/t64_scheduler.c
core/t64_si.c
core/t64_state.c
core/t64_warm_devices.c
core/tests/fuzz_state_import.c
core/tests/state_import_helpers.h
core/tests/test_digest_v25.c
core/tests/test_joybus.c
core/tests/test_save.c
core/tests/test_save_transfer.c
core/tests/test_sched_ledger.c
core/tests/test_sched_matrix.c
core/tests/test_si.c
core/tests/test_si_profile.c
core/tests/test_state_export.c
core/tests/test_state_import.c
docs/claims/phase09.md
docs/evidence/phase07-oracle-authority.json
docs/evidence/phase09-first-hosted-pair.json
docs/spec/machine-contract.md
docs/spec/phase09-cases.json
docs/spec/profile-membership-sites.txt
docs/spec/raw-capture.md
docs/spec/replay.md
docs/spec/result-protocol.md
docs/spec/save-media.md
docs/spec/si-pif-controller.md
docs/spec/state-container.md
docs/spec/state-field-rules.txt
docs/spec/state-mutants.txt
fixtures/manifest.json
fixtures/pad/poll.S
fixtures/pad/poll.ld
fixtures/pad/poll_oracle.py
fixtures/phase09/admit.S
fixtures/phase09/admit.ld
fixtures/replay/mix.S
fixtures/replay/mix.input.json
fixtures/replay/mix.ld
fixtures/replay/mix_oracle.py
fixtures/save/eeprom.S
fixtures/save/eeprom.ld
fixtures/save/roundtrip.S
fixtures/save/roundtrip.ld
fixtures/save/roundtrip_oracle.py
fixtures/save/save_oracle.py
fixtures/save/sram.S
fixtures/save/sram.ld
fixtures/state/decode_container.py
runner/CMakeLists.txt
runner/t64_artifact_publisher.c
runner/t64_artifact_publisher.h
runner/t64_real.c
runner/t64_real_fixture.c
runner/t64_real_fixture.h
runner/t64_real_result.h
runner/t64_replay.c
runner/t64_replay.h
runner/t64_result_check.c
runner/t64_save_flags.h
runner/t64_supervisor.c
runner/t64_verified_input.h
runner/tests/replay_mix_golden.h
runner/tests/test_artifact_publisher.c
runner/tests/test_pad_poll.c
runner/tests/test_phase09_runner.c
runner/tests/test_replay.c
runner/tests/test_save_fixtures.c
runner/tests/test_save_roundtrip.c
runner/tests/test_state_runner.c
scripts/build-pad-fixture.sh
scripts/build-phase09-admit-fixture.sh
scripts/build-replay-fixture.sh
scripts/build-save-fixtures.sh
scripts/check-ci-yml.sh
scripts/check-phase09-cases.py
scripts/check-rdp-oracle-authority.py
scripts/check-rsp-no-hle.py
scripts/check-state-field-use.py
scripts/check-test-inventory.sh
scripts/collect-boot-evidence.sh
scripts/gate.sh
scripts/k5-mutants.py
scripts/list-membership-sites.py
scripts/qualify-phase09.py
scripts/qualify-rdp-vi.py
scripts/rights-scan.sh
scripts/tests/test_phase09_qualification.py
scripts/tests/test_rsp_no_hle.py
tools/rights/t64_blobscan.c
```

Lane assignment:

```
Lane table (executor, mechanical; every manifest path is assigned by prefix, the two shared headers are in both L1 and L2):

| Lane | Scope | Paths |
| --- | --- | --- |
| L1 | core device models and identity | 16 |
| L2 | state container, importer, audit scripts | 11 |
| L3 | runner | 20 |
| L4 | harness, guards, CI, scanners | 22 |
| L5 | tests, fixtures, oracles, specs, claims, evidence | 38 |

Union check: the union of L1 to L5 equals the manifest (105 paths); paths in two lanes: 2 (the shared headers); unassigned paths: 0.

L1: core/include/tendies64/t64_machine.h, core/t64_ai.c, core/t64_bus.c, core/t64_cpu.c, core/t64_digest.c, core/t64_internal.h, core/t64_isviewer.c, core/t64_joybus.c, core/t64_machine.c, core/t64_observe.c, core/t64_pi.c, core/t64_rsp.c, core/t64_save.c, core/t64_scheduler.c, core/t64_si.c, core/t64_warm_devices.c
L2: core/include/tendies64/t64_machine.h, core/t64_internal.h, core/t64_state.c, core/tests/fuzz_state_import.c, core/tests/state_import_helpers.h, core/tests/test_state_export.c, core/tests/test_state_import.c, docs/spec/state-container.md, docs/spec/state-field-rules.txt, docs/spec/state-mutants.txt, scripts/check-state-field-use.py
L3: runner/t64_artifact_publisher.c, runner/t64_artifact_publisher.h, runner/t64_real.c, runner/t64_real_fixture.c, runner/t64_real_fixture.h, runner/t64_real_result.h, runner/t64_replay.c, runner/t64_replay.h, runner/t64_result_check.c, runner/t64_save_flags.h, runner/t64_supervisor.c, runner/t64_verified_input.h, runner/tests/replay_mix_golden.h, runner/tests/test_artifact_publisher.c, runner/tests/test_pad_poll.c, runner/tests/test_phase09_runner.c, runner/tests/test_replay.c, runner/tests/test_save_fixtures.c, runner/tests/test_save_roundtrip.c, runner/tests/test_state_runner.c
L4: .github/workflows/ci.yml, core/CMakeLists.txt, runner/CMakeLists.txt, scripts/build-pad-fixture.sh, scripts/build-phase09-admit-fixture.sh, scripts/build-replay-fixture.sh, scripts/build-save-fixtures.sh, scripts/check-ci-yml.sh, scripts/check-phase09-cases.py, scripts/check-rdp-oracle-authority.py, scripts/check-rsp-no-hle.py, scripts/check-test-inventory.sh, scripts/collect-boot-evidence.sh, scripts/gate.sh, scripts/k5-mutants.py, scripts/list-membership-sites.py, scripts/qualify-phase09.py, scripts/qualify-rdp-vi.py, scripts/rights-scan.sh, scripts/tests/test_phase09_qualification.py, scripts/tests/test_rsp_no_hle.py, tools/rights/t64_blobscan.c
L5: core/tests/test_digest_v25.c, core/tests/test_joybus.c, core/tests/test_save.c, core/tests/test_save_transfer.c, core/tests/test_sched_ledger.c, core/tests/test_sched_matrix.c, core/tests/test_si.c, core/tests/test_si_profile.c, docs/claims/phase09.md, docs/evidence/phase07-oracle-authority.json, docs/evidence/phase09-first-hosted-pair.json, docs/spec/machine-contract.md, docs/spec/phase09-cases.json, docs/spec/profile-membership-sites.txt, docs/spec/raw-capture.md, docs/spec/replay.md, docs/spec/result-protocol.md, docs/spec/save-media.md, docs/spec/si-pif-controller.md, fixtures/manifest.json, fixtures/pad/poll.S, fixtures/pad/poll.ld, fixtures/pad/poll_oracle.py, fixtures/phase09/admit.S, fixtures/phase09/admit.ld, fixtures/replay/mix.S, fixtures/replay/mix.input.json, fixtures/replay/mix.ld, fixtures/replay/mix_oracle.py, fixtures/save/eeprom.S, fixtures/save/eeprom.ld, fixtures/save/roundtrip.S, fixtures/save/roundtrip.ld, fixtures/save/roundtrip_oracle.py, fixtures/save/save_oracle.py, fixtures/save/sram.S, fixtures/save/sram.ld, fixtures/state/decode_container.py
```

## Lane L1

# Lane L1: Code Review Report

## Summary

Scope: the 16 files of `build/gate/09-10-review-scope-L1.txt` at HEAD (6963bf0), read in full, with the direct callers and
callees (`core/t64_memory.c`, `core/t64_state.c`, `core/t64_vi.c`, `core/t64_rdp.c`, `core/t64_rdp_pipeline.c`,
`scripts/check-rsp-no-hle.py`, the L1 specs) read where a finding needed them. Nothing in this report is a seed copied without
evidence; every finding below was reproduced or traced to a line.

Method beyond reading. Every reproduction ran in scratch builds outside the repository (core library built from HEAD, from the
Phase 9 source base cfe4d34 and from a mutated copy; scratch programs linked against them). No tracked file was edited and no
qualification or full suite was run; the only suites I executed were the four L1 binaries `test_si`, `test_save`,
`test_save_transfer` and `test_sched_matrix` against a deliberately mutated tree (WR-02). Checks that found nothing:

- joybus parse, plan and execute: 1.2 million random 64-byte images (biased toward the escape and count bytes, with plausible
  EEPROM frames) plus random imported-style tables, under ASan and UBSan against EEPROM and stem machines, asserting entry bounds,
  ordering, write confinement to the count byte and receive area, and a canary after the image: zero violations.
- SI, PI, SRAM, EEPROM random activity (frames written to RDRAM, DRAM address and RD64B/WR64B/STATUS writes with valid and invalid
  values, PI SRAM and ROM DMAs, SRAM word stores, host save imports in all three formats, timed inputs, random advances) followed
  by state export, import onto a fresh peer, a field comparison of the PI, SI, save, scheduler and RDRAM state and a 30,000-tick
  continuation digest comparison: 160 rounds over the e4k, e16k and s32k ids, zero differences.
- registered-profile identity: all 17 pre-Phase-9 ids and the four family ids serialize within the 2,048-byte limit even with
  maximal policy text; the status table is unique and contiguous 0 to 33; the four family ids and the isviewer allow-list agree.

Necessity (category 4): the rubric was read; no finding rests on it in this lane (the one unreachable branch, `save_request_prefix`
returning RESOURCE_EXHAUSTED for NULL storage, is commented as unreachable and is dead-code style at most).

Process disclosure (the executor should know before trusting any file under `build/fixtures/`): while building a scratch tree I
ran `ninja -t clean` against a scratch build directory whose custom-command outputs are declared under the repository's ignored
`build/fixtures/`. That deleted the generated fixture set there. I regenerated it immediately with the repository's own fixture
targets (`t64-boot-fixtures`, `t64-rdp-fill-fixture`, `t64-audio-fixture` and the four `t64-phase09-*-fixture` targets, one job
at a time, same pinned toolchain under `build/tools`); every build step finished without error (the builder scripts assert the
manifest hashes), `git status` stayed clean, and every fixture family the CMake custom commands declare (boot and hello variants, rdp-fill,
audio, and the Phase 9 admit, pad, eeprom, sram, roundtrip and replay-mix ROMs) is present again next to the untouched `rsp-smoke*`
directories. File modification times and the compiled `t64_rom_pack` binary are new, and I cannot prove the file modes match the
originals; re-verify any hash or mode you recorded from that directory if it matters. A stray `scripts/__pycache__/` that my guard import created was removed.

**PD-3 `write_generation` decision (seed (g)): sound at both sites** (`t64_machine_save_import` in `core/t64_save.c:453-455` and the last write of `state_commit` in
`core/t64_state.c`): each increment is the final effect of a path that cannot refuse afterwards, saturates, applies to a profile with a device only, never reaches
the digest or the container, and a refused import never touches it; the only weakness is that the value is not unique across replacement (IN-03).

**Old-profile behavior: unchanged.** I built the Phase 9 source base (cfe4d34) and HEAD with the same build identity string and compared the profile hash, the admission status,
the advance result and the canonical state digest at ten checkpoints (ticks 0, 5, 1,000, 20,000, 200,000, 400,000, 538,000, 539,500, 541,000, 600,000) for all 17 registered pre-Phase-9
ids (including the shipped Phase 8 descriptor-11 id with live AI and VI packets) running three authored ROMs (`hello.z64`, `rdp-fill.z64`, `audio-pcm.z64`): zero byte differences in 51 runs;
reading found no changed gate for descriptors below 12 (the CPU preflight now calls `t64_schedule_check_capacity` but performs the same checks in the same order).

Headline: one BLOCKER. The v25 canonical state, which the digest, the container and the importer all share, does not contain the RDP combiner block or the RDP memory-coverage table
although the family behaves on both, so export followed by import silently drops behavior-relevant state, reports success and reproduces an equal digest (CR-01). Two WARNINGs:
the CPU store budget of D-44 is keyed to the unadjusted effective address and misses full-word `SWR` stores that start an RD64B (WR-01), and nothing (test or guard) pins the coverage
invalidation of the two new RDRAM writers (WR-02).

## Narrative Findings (AI reviewer)

### Critical Issues

#### CR-01: The v25 state is blind to the RDP combiner block and the memory-coverage table; state export and import drop them with status OK and an equal digest

**Severity:** BLOCKER
**File:** `core/t64_digest.c:307,649-695` (gates), `core/t64_internal.h:1263-1270` (depth predicate), `core/t64_state.c:2825-2828` (commit zeroes coverage), consumers `core/t64_vi.c:320-331`, `core/t64_rdp_pipeline.c:211-239`, `core/t64_rdp.c:457`
**Issue:** `has_rdp_state` is true for v25 (through the filter-v9 predicate), but the combiner fields (`combine_mode` and its valid flag, environment, fog and blend colors and their valid flags, `primitive_lod_fraction`, the three key vectors and their valid flags) are hashed only under `has_v19_state || has_v20_state || has_v21_state` (`t64_digest.c:677`) and the memory-coverage table only under `has_v20_state || has_v21_state` (`t64_digest.c:669`). `has_v21_state` is `t64_has_rdp_depth_profile()`, which requires `descriptor_version == 10u` (`t64_internal.h:1263-1270`, the K1 amendment), so it is false for descriptors 11 and 12. The container is the digest preimage, the importer reads only those sections, and `state_commit` zeroes the coverage table (`t64_state.c:2825`) and the whole `rdp` struct before applying, so those members never travel. The family does behave on them: `rdp.c:457` refuses a triangle packet when `combine_mode_valid` is false on a combiner profile; `rdp_pipeline.c` combines with the environment, fog and blend colors; the VI scan-out reads the coverage table into every filtered sample (`vi.c:320,330`). The Phase 8 v24 digest has the same hole (inherited by 09-01 as K1) but it is pinned and cannot change; v25 is new and can.

Reproduction (scratch programs against the HEAD core library, stem id `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1`):

1. Setting `combine_mode`/`combine_mode_valid`, the three colors and two coverage bytes on an admitted machine leaves `t64_machine_digest` unchanged (equal SHA-256 before and after). Export (6,351,551 bytes), then import into a fresh admitted peer returns OK with reason 0; the peer has `combine_mode_valid` 0, all three colors 0 and both coverage bytes 0, and its digest equals the source digest.
2. No contrived state is needed. Take the authored fixture `audio-pcm.z64` (4,728 bytes, SHA-256 cd9556444480b8b02b043207013e7ce5bf5697a1c040058bcde1348e55e80b99) on the stem id, advance to guest tick 540,000 (video and audio packets drained as a host would), export, import into a fresh peer. Status OK, digests equal, but a field-by-field comparison shows the memory-coverage table differs in 84 bytes (12 bytes for `rdp-fill.z64`, 4,552 bytes, SHA-256 b469340e7caa0ac341a2a5b525aa7e3b39c8a47f295ecd10710ae695e46dfc3d, at its completion tick). Every other compared member (pi, sp, rsp, cpu, boot, cache, guest devices, output, rdp, vi, ai, si, RDRAM, SP memory, triangle coverage, packet storages, scheduler queue) is byte-equal at the several dozen checkpoints I compared across `rdp-fill`, `audio-pcm`, `pad-poll`, `replay-mix` and the EEPROM and SRAM fixtures; the coverage table is the only loss. (The `rdp` struct compares equal there only because those fixtures never issue SetCombine or a color command; that is the same blind spot, not evidence against it.)
3. Continuation proves it matters: run both machines forward with identical guest code (the same two-instruction loop written at one RDRAM address in both and the program counter aimed at it) for 4.2 million ticks, draining packets identically. The digests differ at every 700,000-tick checkpoint (six of six). Control in the same setup: zero the source's coverage table before the export and the digests are equal at every checkpoint, so the table alone is the cause.

Effect on the roadmap: success criterion 3 ("reproduces the canonical digest exactly") is satisfied only because the digest cannot see the lost state; CORE-04 "continue equal" is false for any machine with a populated coverage table or a set combiner block (the executor's builders B05 to B07 avoid exactly these states, which is why the matrix is green). The same blindness weakens every digest-equality comparison in the family (replay checkpoints, partition and interleave equality): two live runs that differ only in a SetCombine or in coverage marks have equal digests until a later render exposes the difference in RDRAM.
**Fix:** Extend the v25 walker, not the legacy ones: emit the combiner block and the memory-coverage table in section 8 behind `has_v25_state` as `T64_DIGEST_CLASS_NEW` spans (so the strip-and-compare differential against v24 still holds), add the matching writer/reader rules and field-rule rows for the importer (the coverage table needs the same one-reader-per-section treatment as the triangle coverage), regenerate the v25 goldens as a recorded F1 defect fix (the container grows by 2,097,152 bytes; the plan's original 8.4 MB size bound assumed two 2 MiB coverage arrays, that is, this layout). The smaller alternative, failing closed at export and import when any of those members is non-default, keeps the digest blind for live determinism evidence and is not recommended. Controls to prove it: a unit test that flips each combiner member and one coverage byte once and requires the v25 digest to change (the section-10 field-flip pattern), and a boundary-matrix row whose builder leaves SetCombine, SetEnvColor and a drawn fill populated with the T3 continuation comparison (the continuation scenario above is a ready construction).

### Warnings

#### WR-01: The D-44 store budget is computed from the unadjusted effective address, so a full-word `SWR` that starts an RD64B retires after the preflight said there was room

**Severity:** WARNING
**File:** `core/t64_cpu.c:1483-1523` (budget), `core/t64_memory.c:116-136` and `core/t64_memory.c:294-305` (the store that is actually performed), `core/t64_bus.c:803-805` (mask 0x0f routes to `t64_bus_write`)
**Issue:** `cpu_successor_extra_event_budget` translates the effective address of the instruction and returns 2 only when the physical address equals `T64_SI_BASE + T64_SI_REG_RD64B` exactly (and 1 for WR64B). `memory_access_info` marks `SWL` and `SWR` as 32-bit stores, but `translate_effect` aligns a partial store down to its bus width (`t64_memory.c:127-129`), and `merge_store_mask` gives `SWR` at offset 3 a full 0x0f mask, which `t64_bus_write_masked` forwards to `t64_bus_write(address, 4, value)`. So `swr $t1, 7($t0)` with `$t0 = 0xA4800000` and `$t1 = 0x1FC007C0` performs the RD64B start (two scheduler events) while the budget saw physical 0x04800007 and asked for one extra slot. The reverse-endian case has the same shape (`translate_effect` XORs the lane at `t64_memory.c:132-135`, the budget does not), but I did not reproduce that one.

Reproduction (scratch program on the stem id, event queue limit 16, `t64_cpu_step` called directly as `core/tests/test_si.c` does): with room for 2 events, the `sw $t1, 4($t0)` control returns QUEUE_FULL with 3 instructions retired and the SI idle (atomic, as D-44 requires). The `swr` form returns QUEUE_FULL (11) with 4 instructions retired, `si.active_valid` true with direction RD64B, the SI execute and complete events queued and no CPU successor event; advancing to tick 50,000 completes the DMA and the CPU never runs again (retired stays 4, not stopped, stop reason BOUND). With room for 3 the same `swr` is accepted, so the budget is simply one slot short. The sequence-exhaustion refusal has the same arithmetic.

The reachable condition is narrow (queue within two of its limit, which a guest cannot cause alone; sequence exhaustion needs a test-twin width), which is why this is a WARNING; but D-44 and the 09-03 plan state atomic refusal for the store forms that start the DMA, and `case_cpu_store_budget` exercises only `sw` forms and the 8-byte `sd`.
**Fix:** Budget from the address the store will use: for a store of width 4 compute the aligned 4-byte word of the translated address (and, when `cpu_reverse_endian_active`, also the lane-XORed word) and return 2 or 1 when that word is the RD64B or WR64B register; or, more simply and conservatively, return 2 for any store whose 8-byte-aligned span contains `T64_SI_BASE + 4` or `T64_SI_BASE + 0x10`. Control: add `swr` at offset 3 (and `swl` at offset 0) rows to `case_cpu_store_budget` in `core/tests/test_si.c`, each as a refuse-then-accept pair with the accepted baseline in the same setup and the sequence-width analogue.

#### WR-02: Nothing pins the RDP coverage invalidation of the two new RDRAM writers; the mutant survives every L1 suite and the no-HLE guard

**Severity:** WARNING
**File:** `core/t64_si.c:313-316` (RD64B completion), `core/t64_save.c:221-227` (SRAM to RDRAM DMA apply, reached from `core/t64_pi.c:300-310`), `scripts/check-rsp-no-hle.py:1007` and `:241-305`
**Issue:** The memory-coverage sidecar is read by the VI scan-out and the blender, so every RDRAM writer must invalidate it (`t64_pi.c:328` does for the ROM DMA; the guard prints "accepted CPU/cache/PI/SP writes invalidate per-instance coverage sidecar" as route evidence). Phase 9 added two RDRAM writers, the SI RD64B completion and the SRAM-to-RDRAM DMA apply, which call `t64_rdp_memory_coverage_invalidate` and `t64_cpu_link_invalidate_range` correctly today. But the guard pins only `t64_bus_write`, `t64_pi_complete` and the other listed bodies; the SRAM write now happens in `t64_save_sram_dma_apply`, outside the pinned `t64_pi_complete` body, and `t64_si_complete_event` is not pinned at all. Evidence: in a scratch copy of HEAD with both `t64_rdp_memory_coverage_invalidate` calls deleted, `check-rsp-no-hle.py` still prints PASS, and `test_si` (533,819 checks), `test_save` (7,908), `test_save_transfer` (15,008) and `test_sched_matrix` (1,926,163) all report zero failures. No test sets a coverage mark and then runs either DMA. Combined with CR-01 (coverage is not in the state), a stale mark here would be invisible to every digest comparison.
**Fix:** Add one control per writer: a unit case that sets coverage marks over the destination (through `t64_rdp_memory_coverage_write`), runs an RD64B and an SRAM-to-RDRAM DMA, and asserts `t64_rdp_memory_coverage_read` returns 0 over the overwritten span and is unchanged outside it, with the out-of-tree mutant recorded as RED. Extend the guard (data only: new pinned bodies and an evidence line) to `t64_si_complete_event` and `t64_save_sram_dma_apply`, or at least make the printed route evidence list them, because the current line claims a coverage-invalidation property over a writer set that no longer includes every writer.

### Info

#### IN-01: `docs/spec/si-pif-controller.md` still says the cartridge channel answers "absent" for every profile id

**Severity:** INFO
**File:** `docs/spec/si-pif-controller.md:26-29` (code: `core/t64_joybus.c:113-130`, `core/t64_save.c:73-86`)
**Issue:** The scope paragraph says channel 4 "answers absent for every profile id in this slice; the save devices of a later slice replace that answer". Plan 09-04 did replace it: on the three EEPROM ids the cartridge channel answers the info, read and write triples. The paragraph is scoped "in this slice", so it is history rather than a false present-tense claim, but it sits in the current spec of the family and the 09-10 drift sweep did not list it.
**Fix:** Reword to "on the stem and SRAM ids the cartridge channel answers absent; on the EEPROM ids it is answered by the save device (see save-media.md)".

#### IN-02: Stale "reserved" comment on the save-media section id

**Severity:** INFO
**File:** `core/t64_internal.h:1829`
**Issue:** `T64_STATE_SECTION_SAVE_MEDIA` is documented as "Reserved: the save-media section lands with the save devices (plan 09-04)", but `core/t64_digest.c:981-996` emits it for the three save ids and the importer reads it.
**Fix:** Replace the comment with "Emitted by the v25 walker for the three save ids (plan 09-04); the stem stream stops at section 10."

#### IN-03: `write_generation` restarts at 0 on replacement, and the saturating increment exists twice

**Severity:** INFO
**File:** `core/t64_save.c:40-41,453-455`, `core/t64_state.c` (`state_commit`, last statement)
**Issue:** Soundness of PD-3 is stated in the Summary. Two residual points. (1) A host that tracks "content changed" by comparing a remembered generation can alias across replacement: value 1 after an import into ROM A, replacement resets to 0, a later import into ROM B is 1 again with different content; hosts must track an admission epoch as well (the spec says "since creation or the last replacement" but not that consequence). Guest EEPROM writes never advance it, which the spec already records as a Phase 10 follow-up. (2) The same two-line expression (`!= UINT64_MAX` then `++`) is written at both sites, as seed R-05 notes; both guard on equivalent conditions (a profile with a device) so there is no behavior difference today.
**Fix:** State the aliasing consequence in `docs/spec/save-media.md` "Import generation and replacement"; optionally move the increment into one internal helper declared in `core/t64_internal.h` so a future change cannot touch one site only.

#### IN-04: The public bus-debug comment promises no partial effects on a rejected write; sticky-error refusals contradict it

**Severity:** INFO
**File:** `core/include/tendies64/t64_machine.h:636-648` (`t64_machine_bus_write`), `core/t64_si.c:140-149`, `core/t64_pi.c:79-85`
**Issue:** "Rejected writes have no partial effects" is false for the PI (busy start sets the sticky error and latches the length) and now also for the SI (an overlap or a misaligned DRAM address sets the sticky DMA_ERROR and emits a refusal trace record before returning `BUS_DEVICE_REFUSED`). The refusal trace kind used (`T64_SI_TRACE_KIND_REFUSED`, 3, `core/t64_internal.h:865`) has no public name beside `T64_TRACE_KIND_SI_EXECUTE` and `T64_TRACE_KIND_SI_COMPLETE`, although a host draining the trace sees it under `T64_TRACE_DEVICE_SI`.
**Fix:** Reword the comment to name the sticky-error and trace effects of refused device writes, and add the third SI trace kind to the public header (provisional) next to the other two.

#### IN-05: `rsp_observed_*` is cleared by the state commit but not by replacement (seed (f), narrowed)

**Severity:** INFO
**File:** `core/t64_state.c` (`state_commit`, the `rsp_observed_*` assignments), `core/t64_machine.c:2120-2147` (`reset_guest_state`)
**Issue:** `reset_guest_state` does not clear the diagnostic `rsp_observed_*` members and `rsp_observation_valid`, so a machine replaced after RSP activity carries the previous run's observation; the state commit does clear them. I found no digest, observer or evidence path of the composite family that depends on the difference: the digest excludes the members (`core/t64_internal.h:1115-1127`), and the only readers are `runner/t64_rsp_diagnostic.c` and `runner/t64_rsp_smoke.c`, which build a machine per run. The difference is therefore not a Phase 9 defect, only a pre-existing stale-after-replacement quirk that the commit happens not to reproduce.
**Fix:** If replacement should equal a fresh machine for observers as well, clear the same members in `reset_guest_state`; otherwise record the difference in the replacement table of `docs/spec/machine-contract.md`.

## Seed dispositions (lane L1)

- (a) CORE-04 versus the canonical state: **confirmed, and broader than stated.** The digest, not only the importer, is blind to the combiner block and the memory-coverage table; reproduced on authored fixture states with a continuation digest difference and a zeroed-coverage control (CR-01). Decision needed: extend the v25 state (recommended, new-class spans keep strip-and-compare).
- (f) commit zeroes `rsp_observed_*` but replacement does not: **narrowed to INFO** (IN-05); no digest, observer or evidence path of the family depends on it.
- (g) PD-3 `write_generation`: **sound** at both sites (Summary, IN-03); the doubled increment is seed R-05, confirmed low and behavior-neutral.
- Seed section 4 (guard data classification): I agree the `scripts/check-rsp-no-hle.py` changes since phase09-fh are data (two roster entries and four body digests, no logic line changed) and I verified that the four refreshed digests (`decode_region`, `t64_bus_write`, `t64_bus_write_masked`, `t64_pi_complete`) pin the current bodies including the SI and SRAM branches, which I reviewed and found sound (SRAM window carve-out before the domain-2 arm, width other than 4 unsupported, masked and wide accesses unsupported or refused, PI SRAM branch applying only at the completion event and only when not canceled). What the guard does not pin is WR-02.
- Seed section 5 entries touching L1: 09-01 #2 (descriptor 12 refuses create under an unregistered id) confirmed sound and matches `core/t64_machine.c:2445-2449`; 09-03 #2 (section 10 carries `next_token`, `token_exhausted`) sound, the digest and the spec table agree field for field; 09-04 #4 (width 8 and masked accesses on the SRAM window answer BUS_UNSUPPORTED) matches `core/t64_bus.c:571-573,845-846`; 09-05 #7 (a zero export capacity constructs no span) sound (`core/t64_save.c:380-390`).
- Seeds (b), (c), (d), (e) and the tracer lines concern the runner, the audit script, the importer in `core/t64_state.c` and the matrix cost, which are outside this lane's scope; I add no evidence for or against them beyond reading `state_section_save_media` (`core/t64_state.c:2013-2040`), whose deadline bound (`deadline <= tick + window`, window 0 for SRAM) and PI-direction rule I found consistent with `core/t64_save.c` and `core/t64_pi.c`.

---

_Reviewed: 2026-11-20T08:05:00Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: deep_

## Lane L2

# Lane L2: Code Review Report

## Summary

Scope: the validate-then-apply importer and exporter in `core/t64_state.c`, the closed handler table, the container layout and decoder (`docs/spec/state-container.md`, `fixtures/state/decode_container.py` read as a callee), the field-rule and mutant inventories, the three test sources and the field-use audit script. Every in-scope file was read in full; callers and callees (`core/t64_digest.c`, `core/t64_rdp.c`, `core/t64_rdp_raster.c`, `core/t64_rdp_tmem.c`, `core/t64_rdp_pipeline.c`, `core/t64_sp.c`, `core/t64_pi.c`, `core/t64_joybus.c`, `core/t64_save.c`, `core/t64_vi.c`) were read where a finding needed them.

Method beyond reading: out-of-tree scratch builds (core compiled with AddressSanitizer and UBSan, nothing written to a tracked path; the working tree was clean at the end) drove the real exporter and importer with hostile and reachable states.

- Hostile blobs: hash-valid single-field extreme-value sweeps (4 values per aligned 32-bit position) over sections 2, 5, 6, 9, 10 and 11 of the S1 blob and of the B03, B05, B08, B09, B10, B14, B15, B16 and render boundary blobs, plus the RSP tail of section 7 (B04): about 6,000 trials, every accepted blob advanced 3,000 ticks, every refusal compared byte for byte against the machine allocation. No sanitizer fault and no refusal-atomicity failure was seen in what completed. The section 7 body of S1 and the section 8 bodies were still running under heavy host load when this report was written; their results are not claimed.
- Reachable states: 9 authored Phase 9 ROMs on the matching profile ids, 897 export-then-import cycles each (every 7 ticks to tick 3,000, then every 997 and 29,999, plus 400 instruction-count mid-tick stops), 8,073 cycles in all, zero refusals. No evidence of over-refusal by the closed per-device rules on those states.
- Targeted attacks on the RDP in-flight state (CR-01, CR-02), the report span (WR-01), the observer rule (WR-02), the audit scan (WR-03) and the campaign statistics (WR-04).

Overall: the container parsing, checked arithmetic, caps-before-use, one-reader-per-section structure, closed handler table and per-device pass are sound as far as tested; refusals were byte-identical in every sweep trial. Two defects are serious. The v25 state silently drops live RDP behavior (combiner block, memory coverage), so an accepted import with an equal digest continues differently (CR-01). The importer accepts an in-flight RDP triangle render whose packet words were never bounded by the acceptance rules, which gave a reproduced UBSan signed-overflow after an accepted import and a latent out-of-array write (CR-02).

## Seed trace

Hostile blob of the tracer: the real runner state of the long admit fixture (6,351,511 bytes), observer entry mutated by the independent re-signer in `runner/tests/test_state_runner.c` (`mutate_observer_range`, lines 2062-2096; the video packet head, the second observer u64 at body offset 8, set to the 64-bit maximum; entry hash and trailer re-signed, declared digest untouched).

- Inventory row: `observer-range :: STATE_RANGE :: OBSERVER_RANGE :: 16 :: LEN_LE` (`docs/spec/state-mutants.txt:48`).
- Pre-validation passes: `state_verify_trailer` (`core/t64_state.c:2532`) accepts the re-signed trailer, `state_parse_table` (2633-2764) accepts the re-signed observer entry hash (2726-2731), so the blob reaches the readers. Sections 1 to 10 read cleanly in `state_run_sections` (2135-2160, validate mode).
- Reader that evaluates the observer body: `state_section_observer` (2046-2085), dispatched at 2123-2125. It reads all five u64 values (2055-2059), then checks the drain cursor (2060-2061, rule `LEN_LE`), the two heads (2062-2064, rule `INDEX_LT`) and the two counts (2065-2067, rule `COUNT_CAP`), all with reason `T64_STATE_REASON_OBSERVER_RANGE`.
- Refusal raised: for a maximum video head the failing statement is the head check at `core/t64_state.c:2062-2064` (rule `INDEX_LT`, not `LEN_LE`). `STATE_CHECK` (533-539) calls `state_fail` (520-529), which calls `state_report_failure` (507-518); `state_status_for` (458-503) maps the reason to `STATE_RANGE` (32). `state_run_sections` stops, `state_import_attempt` (2891-2917) returns `report->status` at 2909-2914 before `state_check_devices`, the declared-digest recompute and the commit, and `t64_machine_state_import` (2919-2938) copies the report and clears the scratch.
- Observed runner outcome: refusal, stop refused, guest tick 0, reason `OBSERVER_RANGE`; baseline row `none` resumed to the uninterrupted tick. The refusal outcome follows from the code: the runner maps an importer refusal to the prelaunch refusal before the first advance, so guest tick 0 and the diagnostic reason name are expected; the baseline resumes because an unmutated blob passes every check and the commit.
- The refusal leaves the machine untouched: in validate mode every store is gated on `STATE_MODE_APPLY` (`STATE_STORE`, 541-546; the copy in `state_rd_bytes`, 588), only the machine-owned scratch is written and it is cleared at 2931-2933. Confirmed by reproduction on an S1 blob: for each of the five observer u64 values set to the maximum, the importer refused with status 32, reason 40, section 16, and the sweeps compared the whole machine allocation after every refusal without a difference.
- Mismatch found by the trace: the rule is not the inventory's. My reproduction printed rule 11 (`LEN_LE`) for the first u64 (drain cursor), rule 2 (`INDEX_LT`) for the video head (the tracer's mutation), rule 1 for the counts, rule 2 for the audio head. The tracer compares only the reason (`runner/tests/test_state_runner.c:2343-2349`), so the tracer blob reaches (STATE_RANGE, OBSERVER_RANGE, 16, INDEX_LT), not the tuple of the row it is named after. See WR-02.

## Position on seed (a)

The hidden-state gap is real and reproduced (CR-01). Position: it does not contradict the literal text of roadmap success criterion 3 or of CORE-04 ("a restore-then-digest round trip is exact"): in my reproduction the digest after import equals the source digest. It does contradict the phase goal ("machine state ... reproduce runs exactly"), decision D-32/D-30's continuation ladder, and the pending claim text `docs/claims/phase09.md:141` ("reproduces the canonical digest and the continuation digests for authored machines on the same build"): an authored machine with a combine mode set diverges. The three stated options are not exhaustive and are not mutually exclusive:

1. Extend the canonical state (F1 deviation): the only option that makes continuation equal and makes the digest see the difference; changes every v25 literal, golden and replay checkpoint.
2. Fail closed at export (the importer cannot detect it: the blob carries no trace of the members): export refuses when `combine_mode_valid`, any of the environment, fog, blend or key valid flags, `primitive_lod_fraction` or any non-zero memory-coverage entry is present. Honest, cheap (a 2 MiB scan), but it makes export unusable for most RDP-using guests.
3. Bound the claim scope: necessary in any case until 1 or 4 lands, but it leaves a silent import-time divergence in the code.
4. Missing from the list: carry the hidden members in a new non-canonical section outside the digest, like the observer section 16. The F1 digest bytes and every pinned literal stay untouched, continuation becomes equal, and the container version is already provisional. The cost is that the digest stays blind to those members (equal digest, different future), which also limits what the CORE-05 replay checkpoint digests can detect.

Evidence supports: options 2 plus 3 now, with option 4 (or 1) as the real fix, and CR-02's validation added in the same change (see CR-02, coupling).

## Seed dispositions

- (a) confirmed and sharpened, CR-01.
- (b) narrowed: no pass rule is weakened; the library-level ladder is the CORE-04 evidence, the runner adds mechanism only (IN-01).
- (c) confirmed and sharpened: the scan misses `x->a.b` accesses; 91 used members with a one-token fix, not 72; the stated "72 of 472" mixes used members with declared members (WR-03).
- (d) confirmed in part: the render-extent rule (f2a81b5) is sound for fill and copy renders but leaves the triangle render path open (CR-02); the RDP continuation-event rule showed no over-refusal in my runs, but those runs contained no RDP workload except the dedicated B05 row (a dedicated every-tick RDP program run was still in flight at the time of writing, not claimed).
- (e) confirmed as a limit only (IN-05).
- (f) refuted as a defect: harmless to the digest and to the importer's observers; the one reader of `rsp_observed_*`, `runner/t64_rsp_diagnostic.c:529-533`, is a per-run diagnostic. See IN-06.
- (g) not a defect; the decision is defensible; the duplicated expression is an INFO (IN-06).

## Narrative Findings (AI reviewer)

### Critical Issues

#### CR-01: The v25 state drops live RDP behavior; an accepted import with an equal digest continues differently

**Severity:** BLOCKER
**File:** `core/t64_state.c:1585-1631` and `2821-2828` (zeroing), `core/t64_digest.c:300-312` and `669-694` (digest conditions), `core/t64_internal.h:1239-1250` (`t64_has_rdp_combiner_profile`), `core/t64_rdp.c:457`, `core/t64_rdp_pipeline.c:187-189`, `core/t64_vi.c:320,330`
**Issue:** For the composite family `t64_has_rdp_combiner_profile` and `t64_has_rdp_blender_profile` are true (through `t64_has_vi_filter_v9_profile`), so triangle drawing needs `rdp.combine_mode_valid` (`core/t64_rdp.c:457`, `core/t64_rdp_pipeline.c:187-189`), and the VI scan-out reads the RDP memory-coverage table (`core/t64_vi.c:320,330`). The v25 walker hashes the combiner block only under `has_v19/v20/v21` (lines 677-694) and the coverage table only under `has_v20/v21` (669), which are the exact older tuples, and `t64_has_rdp_depth_profile` requires descriptor version 10, so none of these members is in the digest, the container or any reader. `state_commit` zeroes `machine->rdp` (2821) and `rdp_memory_coverage` (2825-2828).

Reproduction (scratch harness over the real exporter and importer, stem id, authored RDP commands from `core/tests/test_rdp_combiner.c`): a source machine runs color image, scissor, one-cycle mode, primitive color, environment color and set-combine through the DP registers, then exports. The peer imports with status OK and `digests_equal(source, peer)` is true. Peer: `combine_mode_valid` 0, `environment_color_valid` 0. Both machines then receive the same four-word triangle and advance 1,500 ticks: the source draws it (`triangle_count` 1, `refusal_reason` 0), the peer refuses it (`triangle_count` 0, `refusal_reason` 5 render range), the framebuffers differ and the digests differ. Any real guest that sets a combine mode once and draws triangles later diverges after any export taken after the set-combine. The digest is blind to the cause, so equal digests after import prove nothing about the next frame, and every comparator of the ladder and the boundary matrix (all digest or result-tuple comparisons) is structurally unable to see this class: B05 holds a plain command word and injects a coverage value with a direct test-only edit (`core/tests/test_state_import.c:2442-2455`), B06 and B07 use `build_vi_frame_with(false)` on purpose (2512-2514 comment), and no row has a combine mode set.

**Fix:** Decide before F2 and record it (see "Position on seed (a)"). Minimum for this phase: export refuses (a named `RESOURCE_EXHAUSTED`-class or dedicated status, with a control that a machine with a set-combine refuses and a machine without it exports) and the CORE-04 claim text names the unsupported members; real fix: a section outside the digest carrying the combiner block and the memory-coverage table with importer rules, plus a boundary row that sets a combine mode, draws, exports mid-sequence and compares the second triangle's framebuffer on source and peer. Control that proves it: the reproduction above as a registered case (source and peer drawing the same triangle after import must agree on triangle count and framebuffer bytes).

#### CR-02: An in-flight RDP triangle render is accepted without the acceptance-time bounds; reproduced signed overflow after an accepted import, latent out-of-array write

**Severity:** BLOCKER
**File:** `core/t64_state.c:1513-1542`, `1554-1562`, `1585-1617`; consumers `core/t64_rdp_raster.c:385-399` (`shade_channel`, `texture_channel_q16` 401-420), `661-662` and `811` (coverage index and write), acceptance side `core/t64_rdp_raster.c:227` (`y1 > 1024u` clamp), `261` and `268` (`shade_products_are_bounded`, `texture_products_are_bounded`)
**Issue:** A triangle command is validated when it is accepted (`t64_rdp_triangle_bounds`: the row range is clamped to the 1,024-row coverage array at 227, shade and texture products are proven to fit at 261 and 268), and the rows are rendered later from `pending_triangle_words` without re-checking. The reader (`state_read_rdp`) bounds `pending_triangle_word_count` (1514-1519) but reads the words as free `F64` values (1525), takes `render_triangle`, `shade_triangle`, `render_y_begin`, `render_y_end` and `render_y` from the blob unconstrained, and its late render-extent rule (1585-1617) bounds only the framebuffer bytes against RDRAM (`last_row <= ram`, 4,194,304 rows allowed), never the 1,024 rows of `rdp_triangle_coverage`, and never the products.

Reproduction, accepted blobs (status OK) produced from a real mid-triangle-render export by editing the section 8 body and re-encoding with valid hashes:
- Signed overflow, reachable now: word count 12, opcode shade-triangle, flip bit set, a major-edge slope of 0x7FFFFFFF, shade slope words at 0x7FFF/0xFFFF, `shade_triangle` 1, row 1,000. The import returned OK; the first advance aborted under UBSan: `core/t64_rdp_raster.c:394:41: runtime error: signed integer overflow: 2147483647 * -2148557356055 cannot be represented in type 'int64_t'`. The shade values are computed before the combiner refuses the pixel, so the zeroed combiner (CR-01) does not mask it.
- Out-of-array write, latent: the same kind of blob with rows 1,500 and 2,040 (a valid 14-bit signed y range reaches row 2,047) was accepted; once the peer holds a combine mode, which a fix of CR-01 by carrying the combiner block would provide (reproduced by setting the combine fields on the peer after import), rendering writes `rdp_triangle_coverage[render_y * 1024 + x]` past the 2 MiB array: 4 u16 words landed in the adjacent `rdp_memory_coverage` table (the tables are adjacent in the machine allocation). A write is bounded by the allocation (maximum row 2,047) but is an array-bounds violation and corrupts hidden state; today it is blocked only because the importer zeroes the combine mode.

This contradicts the importer contract (header comment of `t64_machine_state_import`, spec "Scope and contract": "cannot cause an out-of-range access, an unbounded loop or undefined arithmetic") and shows the field-use audit's "one real hole" statement (state-container.md:758-768) was the first of a class: any post-acceptance consumer that trusts acceptance-time proofs. The audit could not see it: `render_y` is listed with site class `s` and mapped to one rule, `ADDR_RANGE` (`docs/spec/state-field-rules.txt:68`), which is true of its RDRAM use and false of the coverage subscript at 662.

**Fix:** When `render_active && render_triangle` the reader must establish what `t64_rdp_triangle_bounds` established: require `pending_triangle_word_count` equal to the word count of the opcode in word 0, restrict word 0 to the opcodes the profile accepts (0x08, 0x0c, and the texture opcodes only with the tile and level rules), require `render_y_end < T64_RDP_TRIANGLE_COVERAGE_HEIGHT`, and either re-run the product bounds (move `shade_products_are_bounded` and `texture_products_are_bounded` to a shared validator called with the blob words) or refuse any triangle render in progress (`render_active` with `render_triangle`) at import. Add the two reproductions as mutant rows (an overflowing shade slope and a row at or above 1,024, each with an accepted baseline) and make this the same change as the CR-01 fix, because the second part of this finding becomes live the moment the combiner block is carried.

### Warnings

#### WR-01: A refusal for an argument error writes the report into a span that aliases the machine; spec and header say an unusable report span is never written

**Severity:** WARNING
**File:** `core/t64_state.c:2408-2445` (`state_check_arguments`), `2934-2936`; test `core/tests/test_state_import.c:1313-1323`; spec `docs/spec/state-container.md:278-283`
**Issue:** `*report_usable` is set to `report_out != NULL` (2417) and cleared only in the branch at 2436-2445, which runs after the NULL (2418), length (2422), flags (2426) and blob-overlap (2430) returns. Those four refusals return with the flag still set, and `t64_machine_state_import` then copies the report into `report_out` (2934-2936) without any disjointness check. Reproduction: with a report pointer inside the machine's RDRAM (`machine->ram + 0x20000`), a refusal with flags 0x80 (ARG_FLAGS), length 0 (ARG_LENGTH) and a NULL blob (ARG_NULL) each overwrote the aliased bytes; only a valid blob plus an aliasing report (the one case the existing test covers) left the bytes untouched. A refused import therefore changes the machine, contradicting "a refusal leaves the machine bit-identical" and the spec sentence "an unusable report span is never written".
**Fix:** Check the report span first (before the NULL, length and flags returns, whenever `machine` is non-NULL), or leave `*report_usable` false for every early return that precedes the report check. Control: extend the case `container-refusals` so each argument refusal runs once with an aliasing report span and compares the span byte for byte.

#### WR-02: The tracer mutant does not reach the tuple of the inventory row it is named after, and the tracer never compares the rule

**Severity:** WARNING
**File:** `runner/tests/test_state_runner.c:2046,2088,2343-2349` (read as callee), `docs/spec/state-mutants.txt:48`, `core/tests/test_state_import.c:4691-4693`
**Issue:** The inventory row, and the in-process mutant, edit the first observer u64 (drain cursor, offset 0, rule `LEN_LE`). The tracer edits the second (`OBSERVER_VIDEO_HEAD_AT` 8), which the importer refuses with rule `INDEX_LT` (`core/t64_state.c:2062-2064`; reproduced: rule 2), and the tracer checks only the reason string the child prints (the child prints no rule), so the tracer record `reason=OBSERVER_RANGE` proves the reason but not the tuple the plan promised ("must reach exactly its own tuple"). A comparator that cannot see the rule would also pass if the drain-cursor check were deleted and only the head check remained. The reported `position` is the end of the observer body (6,351,639 in my run) for every observer failure because all five values are read before any check, so it does not locate the member either (the spec calls it a byte position of the failure).
**Fix:** Make the tracer mutate the drain cursor (offset 0) so it matches the row, or add a second row for the head with rule `INDEX_LT`; have the direct runner print the rule and compare it; check each check in `state_section_observer` right after its read so the position names the field.

#### WR-03: The field-use scan is blind to nested member accesses and its name-level mapping cannot express two different bounds

**Severity:** WARNING
**File:** `scripts/check-state-field-use.py:217-219` (and the docstring 22-26), `scripts/check-state-field-use.py:563-573` (self-test seed), `docs/spec/state-container.md:751-756`, `docs/spec/state-field-rules.txt:17-18,68`
**Issue:** `member_pattern` accepts `->name` or `.name` only when the dot is not preceded by an identifier character (`(?<![A-Za-z0-9_])\.`), so `machine->rsp.dma_beat_cursor`, `cpu->tlb[i].asid`'s identifier-preceded forms and every `x->device.member` access are invisible. Removing the lookbehind (a one-token change in a scratch copy) raises the used-member list from 72 to 91; the 19 additional names (one is the summary line) include `gpr`, `pc`, `pc_full`, `tlb`, `active`, `active_length`, `captured_bytes`, `packet_lengths`, `payload_length`, `asset_sha256`, `payload_sha256`, `policy_sha256`, `icache`, `dcache`, `other_modes`, `device`, `length`, `pending`, `rdram_protocol`, none of which is in `state-field-rules.txt`. The spec statement "72 names of 472 reachable members" compares used members with all declared members; the honest figures are 72 used (91 with the fix) of 472 declared. The self-test seeds only a `->` access (the `pi->seeded_audit_index` seed), so no seed could have detected the blind spot. Separately, the mapping is one row per member name and a reader that merely mentions the token satisfies it (`mentions()`, 438-439), so `render_y :: ADDR_RANGE :: state_read_rdp` is accepted while the coverage subscript at `core/t64_rdp_raster.c:662` needs a different bound (CR-02).
**Fix:** Allow `.name` after identifiers (and add a nested-access seed to the self-test), map the 18 new members, and require the audit to list the consuming site per use class rather than one row per name, or at least one row per (structure, member) pair.

#### WR-04: The campaign and the sweep reach almost no reader rule at the sampled counts, and two assertions cannot fail

**Severity:** WARNING
**File:** `core/tests/fuzz_state_import.c:65-125,199-201`, `core/tests/test_state_import.c:4887-4934`
**Issue:** `mutate` truncates or extends the blob (cases 1 and 2) without updating the header total, so those iterations always end in `TOTAL_LENGTH`; `blob_resign(copy, n, rng % 8)` leaves the trailer stale in 4 of 8 draws, which always ends in `TRAILER_SHA256`; half of the flips land at a uniformly random byte (mostly RDRAM and the coverage array, which have no rule). I ran the real campaign (seed 0x5EED0911) on the S1 blob: with the gate default of 12 iterations all 12 are refusals at the container level (reasons 8, 12, 19, 21, 24); with the sanitizer-preset value of 120 only 6 refusals reach a reader or later (3 `FIELD_RANGE`, 1 `FIELD_DERIVED`, 1 `OBSERVER_RANGE`, 1 `DECLARED_DIGEST`) and 3 are accepted, 9 of 120 (7.5 percent). `case_mutation_sweep` asserts `refusals + accepted == iterations` (4919), which is true by construction, and `iterations > 0`; its six body offsets of 6.35 MB all fall in RDRAM or the coverage array. The mutant table (52 rows) is the real evidence for the rules; the campaign adds close to nothing in either preset, and the spec text presents it as sanitizer-scaled evidence.
**Fix:** After a structural edit always update the header total and re-sign the trailer; restrict byte flips to non-RDRAM sections; assert a minimum count of refusals with reason 26 or higher and of acceptances so a campaign that never reaches a reader fails.

### Info

#### IN-01: A runner-published state can never complete a supervised import cleanly (seed b)

**Severity:** INFO
**File:** `docs/spec/state-container.md:507-517`, `runner/tests/test_state_runner.c:2356-2361` (read as callee)
**Issue:** A state exported by the supervised runner, imported through the supervised path, always ends in `report-error` / `report-invalid` (its observer cursor is at the log length, the log is replayed as zero bytes); a resumed run never reports `pass`. The only end-to-end equality evidence is the direct-runner row, whose comparison is genuine (completion tick and final state digest equal the uninterrupted run, `runner/tests/test_state_runner.c:2312-2321`), for one ROM that uses no RDP combiner. No pass rule is weakened, but the claim text must say the runner shows the mechanism only and that the user-level export-then-import flow through the supervisor ends in an error record.
**Fix:** State this in the CORE-04 claim; decide in the fix cycle whether a resumed run needs its own closed verdict.

#### IN-02: Dead scratch facts, an unused public rule constant and an imprecise position

**Severity:** INFO
**File:** `core/t64_state.c:1052-1054` (`facts->queue_count`, `next_sequence`, `sequence_exhausted` are stored and never read; `reserved` in `core/t64_internal.h` is never touched), `core/include/tendies64/t64_machine.h:498` (`T64_STATE_RULE_MASKED_AT_USE` is never emitted by the importer; only the audit mapping uses it), `core/t64_state.c:520-529` (`state_fail` reports the position after the failing read)
**Issue:** Dead stores and an unreachable public constant; the position of a field failure is the byte after the value read (or the end of the section for the observer), not the member.
**Fix:** Remove the dead facts, keep `MASKED_AT_USE` out of the public closed list or document it as audit-only, record the member start before a read.

#### IN-03: Header contract gaps

**Severity:** INFO
**File:** `core/include/tendies64/t64_machine.h:405` and `core/t64_state.c:361-366`; `core/t64_state.c:1056-1105` (apply re-reads the blob)
**Issue:** The export comment says every refusal leaves the caller bytes intact, while the BAD_STORAGE path clears `layout.total` bytes of the caller buffer (the spec documents the clear). The import comment does not say the blob must not change during the call; apply mode re-reads the validated bytes and several stores (`entry_count`, `device_count`) are not clamped in apply mode, so a blob modified concurrently by another thread could defeat validation. The core is single-threaded by contract, but the contract is not stated for the blob.
**Fix:** One sentence each in the header.

#### IN-04: Retained packet metadata is accepted unconstrained

**Severity:** INFO
**File:** `core/t64_state.c:1640-1713` (video slot), `1767-1814` (audio slot; `info.struct_size` at 1781)
**Issue:** Only the slot byte lengths are bounded; `struct_size`, `width`, `height`, `packet_bytes`, `source_bytes`, `byte_length` and the other metadata are free, so `t64_video_info` and `t64_audio_info` can publish values inconsistent with the retained bytes (memory-safe: the copy functions use the bounded length). The runner validates `struct_size` and the packet fields (`runner/t64_real.c:737`), but another host would not.
**Fix:** For a produced slot (non-zero length) require `struct_size == sizeof`, `byte_length == length`, and `packet_bytes` equal to width times height times four (video), as `FIELD_DERIVED`.

#### IN-05: Sanitizer cost and registration (seed e)

**Severity:** INFO
**File:** `core/CMakeLists.txt:841-846,1092-1100`
**Issue:** All 27 cases run in one test process; the sanitizer block raises the timeout to 1800 s for a 631.8 s measurement. My own runs, on a host with a load average above 40, were several times slower than unloaded, so the margin is real but not large for a loaded runner, and a liveness hang in the campaign is only reported after 30 minutes. `T64_STATE_MATRIX_FULL` stays native only, which the spec states.
**Fix:** Register the heavy cases as separate CTest entries with their own timeouts; keep the 1800 s only for the aggregate.

#### IN-06: Seeds (f) and (g), and a test-only edit in B05

**Severity:** INFO
**File:** `core/t64_state.c:2850-2870`, `core/t64_save.c:453-455`, `core/tests/test_state_import.c:2454`
**Issue:** (f) `state_commit` zeroes `rsp_observed_*` and `rsp_observation_valid` (2850-2861); the digest never reads them, and the only reader is the per-run RSP diagnostic (`runner/t64_rsp_diagnostic.c:529-533`), which after an import correctly reports no attempt. `core/t64_machine.c` has no write to those members, so a replacement keeps the previous ROM's last observation: an inconsistency in admission, not in the importer. (g) The generation rule is sound (last write of the commit, never touched by a refusal, saturating, never in the container or digest); the saturating increment is written twice (`core/t64_state.c:2867-2870`, `core/t64_save.c:453-455`), which is a maintenance risk with no behavior difference. B05 reaches its `reached` predicate by writing `rdp_triangle_coverage[1234]` directly, so its boundary is not a reachable machine state (spec deviation 3 states the command word substitution but not this edit).
**Fix:** Zero the observation record at admission as well or note the asymmetry in the spec; share one increment helper; state the direct edit in the B05 row text.

---

_Reviewed: 2026-11-20T06:56:00Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: deep_

## Lane L3

# Lane L3: Code Review Report

## Summary

Scope: the runner publisher, supervisor, real runner, replay module, result checker,
save flag header, verified-input header, fixture table and the seven runner test
binaries (20 paths). I read every in-scope file, read the direct callees outside the
list that the findings needed (`runner/t64_verified_input.c`, `runner/t64_num.h`,
`tools/common/t64_json.c`, `runner/CMakeLists.txt`, the state container spec and the
replay, save media, raw capture and result protocol specs), and compared the case
inventory `docs/spec/phase09-cases.json` with the `cases[]` tables of the seven test
binaries (all seven agree exactly: 9, 2, 5, 6, 9, 15 and 12 cases). No test or
qualification command was run, and no tracked file was edited.

Result: no BLOCKER. The publisher is not exposed to the Phase 8 CR-01 class (statement
below). The descriptor intake (verified snapshots, the import, save, replay and state
descriptors), the replay JSON parser caps, the supervisor child-argument arithmetic
(slot sum 45 of 64, rechecked by hand) and the Phase 9 option-pair validators held up
against the cases I tried to break. Every `T64_SUPERVISOR_TESTING` and
`T64_REAL_RUNNER_TESTING` fault path I found is compiled only into the
`t64-supervisor-test` and `t64-runner-test` twins and `test_artifact_publisher`
(`runner/CMakeLists.txt` lines 322-351 and 379-386 and 706-722); I found no twin fault
reachable from `t64-supervisor` or `t64-runner-real` (one unguarded carry from Phase 8
is IN-06). Three WARNINGs are filed: a publisher failure after the rename has
committed leaves a published file next to a failure record and a stranded stage (WR-01),
the runner's own exported state re-imports into a result record that its own checker
refuses (WR-02), and the result protocol still states the pre-Phase 9 pass rule
(WR-03).

### Artifact publisher versus the Phase 8 CR-01 class

**Statement: `runner/t64_artifact_publisher.c` is not exposed to the CR-01 class.** It
cannot publish a substituted staging entry or leaf for any principal other than the
runner's own effective user. Evidence, in the order of the transaction:

- **Parent bound by descriptor.** `open_artifact_directory` (lines 164-175, called at
  412-439) walks the path one component at a time with `openat(...O_NOFOLLOW |
  O_DIRECTORY | O_CLOEXEC)` (93-105, 109-161), refuses `.` and `..` and symlink
  components, and requires `st_uid == geteuid()` and no group or world write bit
  (`& 0022`, line 169). The held `parent_fd` is used for every later effect; the path
  string is never used again. A second principal therefore has no write access to the
  parent in which the stage lives, which is the second remedy the Phase 8 review named
  ("reject destination parents whose ownership/permissions allow another principal to
  replace the stage entry").
- **Stage bound by descriptor.** `create_stage_directory` (226-265) does `mkdirat(parent_fd,
  name, 0700)`, records the created device and inode before any later check (241-245,
  so cleanup can still remove its own empty directory), reopens it with
  `open_directory_nofollow(parent_fd, name)` and requires the opened identity to equal
  the created one, then `fchmod(fd, 0700)` and a mode-exact name re-check (250-259).
  `stage_fd` is held for the life of the publisher.
- **Leaf bound by descriptor.** The staged file is created through
  `openat(stage_fd, leaf, O_CREAT|O_EXCL|O_NOFOLLOW, 0600)` (271-272) and checked
  with `fstat` for one link, owner and mode 0600 (274-278). All bytes are written
  through the held `file_fd` (`write_all_fd`, 286-298, which retries `EINTR`, loops
  over short writes and treats a zero or negative count as failure), never by name.
- **Rename source is descriptor-relative.** Line 549-550 is
  `rename_noreplace(publisher->stage_fd, publisher->leaf, publisher->parent_fd,
  publisher->leaf)`: both ends are `(directory descriptor, leaf)` pairs. The stage name
  inside the parent (`publisher->stage_name`) is never a rename source. This is the
  difference from the raw-capture publisher that CR-01 hit, which renamed the stage by
  its name from the caller-selected parent after checking that same name
  (`runner/t64_raw_capture.c`, fixed by cc18852). The no-replace flag
  (`RENAME_EXCL` / `RENAME_NOREPLACE`, 177-193) makes an occupied leaf `EEXIST`, and a
  leaf planted as a symlink is also refused because rename never follows the target.
- **Identity checks.** `staged_identity_holds` (509-528) compares the held descriptor
  (device, inode, one link, size equal to `written`) with the entry in the stage
  (device, inode, one link, owner, mode 0600) and with the stage entry in the parent.
  After the rename, lines 561-567 `fstatat` the published leaf and compare device and
  inode with the held file. The hook point (539-544) sits before that check, and
  `test_artifact_publisher.c` mounts four substitutions at it (staged file replaced by
  a symlink, parent renamed away and replaced by a symlink, target pre-planted, stage
  swapped for a look-alike) with the specific expected errors.
- **Leaf grammar.** `leaf_is_valid` (63-89) admits exactly one dot, a 1 to 32
  character stem of `a-z0-9-` not starting with `-`, and a three-letter lowercase
  extension, so no slash, dot component, leading dot (stage names begin with `.`) or
  stage-name collision can be named. The buffer (`ART_LEAF_CAPACITY` 40) holds the 36
  character maximum.
- **Short and failed writes, cap.** `t64_artifact_publisher_write` (488-505) refuses a
  write that would pass the cap with `byte_cap - written` (no wrap; the test passes
  `SIZE_MAX`) before touching the file, and a failed write latches `error` so
  `finalize` refuses.

What the design does not, and cannot, defend: a substitution by the runner's own
effective user between `staged_identity_holds` and `renameat2` (a user who owns the
0700 stage can swap the leaf after any check; the post-rename check at 561-567 then
reports `artifact-substitution`). That is outside the CR-01 threat (another principal)
and no control in the suite substitutes after the check, because the hook runs before
it. The publisher is exposed to the different problem filed as WR-01: failures after
the rename has already committed.

### Supervisor option-pair refusal (R-01 to R-03)

The supervisor refuses, before any snapshot, work directory or fork, every Phase 9
option pair that a spec or plan defines as refused: `parse_args` runs the artifact pair
rule (916-925), `save_flags_valid` (547-580), `replay_flags_valid` (595-620),
`state_export_flags_valid` (628-647) and `state_import_flags_valid` (659-681) in
sequence under `!bad &&`, and a failure returns before `real_run_mode` (2380-2387,
`usage` record, exit 2). Between them the validators refuse every cross-family
combination (the replay validator refuses save, raw and the pair-less forms; the two
state validators refuse save, replay and each other; the save validator refuses raw), so
the save family (import and export together), REPLAY-REC, REPLAY-PLAY, STATE-OUT and
STATE-IN are mutually exclusive, all are refused with raw capture, and synthetic mode refuses
every Phase 9 flag (995-1021; verified for all 19 Phase 9 flag names). The answer to "does it
refuse every unlisted pair" is no, by design, for six pairs: SAVE-IN, SAVE-OUT,
REPLAY-REC, REPLAY-PLAY and RAW each combined with INPUT (the custom schedule options),
and INPUT with PAIR, are accepted (R-03). I found no defect in that: the schedule is
not an artifact producer, REPLAY-PLAY with a schedule that has events is refused by the
child with a published `input-refused` report (`t64_real.c` 1337-1341), and STATE-IN
with INPUT is the only pair refused for a documented reason. Matrix rows without a
test remain as the seed lists (R-02).

### Seed dispositions

- **Seed 1 (tracer), runner-test side.** Sound with one reporting weakness (IN-03). The
  mutation offsets are literal layout constants, the expected reason comes from the
  inventory file (`test_state_runner.c` 2108-2148, never from the production reader),
  the observed reason is read from the child's diagnostic stream, the whole stream must
  equal the closed line (`expect_diagnostic`, 1662-1673), the blob lives only in the 0700 private directory
  (`tracer_private_dir`, 2200-2206; `write_new_private` is `O_EXCL` 0600) and is
  unlinked and the evidence tree removed at the end (2363-2372). Not equivalent: the
  baseline imports and the mutant refuses at the same setup.
- **Seed (b), a resumed run cannot report pass.** Confirmed as a limit and narrowed:
  `t64_real.c` 1844-1851 compares the core retirement count with a count taken only
  from the post-import trace, and the milestone flags (1900-1908) can never be set, so
  the closed `harness-error` / `host-error` record is the only outcome. The CORE-04
  evidence through the runner is adequate for a state exported by the core API (the
  case compares tick, digest and log bytes with the uninterrupted run), but not for
  the state the runner itself publishes, whose re-import ends in `report-invalid`;
  that gap is WR-02.
- **R-01.** Confirmed: implemented (supervisor 1004-1019) and untested.
- **R-02.** Confirmed. `case_flag_combinations` (`test_replay.c` 2955-3060) covers
  record with play, without the pair, with raw capture, play without the report leaf
  and bad intervals and leaves; the state rows cover STATE-OUT and STATE-IN against the
  save and replay families. No test crosses a save flag with a replay flag.
- **R-03.** Narrowed: accepted-by-code, no defect (see the statement above).
- **R-04.** Confirmed: the child parse at `t64_real.c` 2372-2374 is reached only through
  the supervisor forwarding (2096-2101); every other child descriptor option has a
  direct-runner use in a test.
- **R-06.** Confirmed with arithmetic: the sum is 1+2+2+2+2+2+2+2+4+4+8+8+4+2 = 45 and
  matches the pushes in `real_run_mode` (2032-2127). `argv-bound` exercises 19 of the 45
  slots and `argv-max-save` 27; both end in non-pass records by construction
  (`video-observation-invalid` on a family id), so they prove forwarding and parse, not
  a passing maximum run. The compile-time assertion is the real bound.
- **R-05 and seeds (a), (c) to (g)** are core or audit scope, not this lane.

## Narrative Findings (AI reviewer)

### Critical Issues

None.

### Warnings

#### WR-01: Publisher failure after the rename has committed leaves a published file and a stranded stage

**Severity:** WARNING
**File:** `runner/t64_artifact_publisher.c:560-578` and `581-601`;
`runner/t64_real.c:2105-2118`, `2134-2147`, `2166-2176`, `2200-2209`
**Issue:** `t64_artifact_publisher_finalize` sets `committed = true` at line 560, right
after the no-replace rename, and only then runs two more checks that can fail: the
published-leaf identity check (561-567, `artifact-substitution`) and
`fsync(publisher->parent_fd)` (568-571, `artifact-write`). Both return false before the
stage removal at 572-577, and `t64_artifact_publisher_abort` returns immediately for a
committed publisher (line 583). So after a post-commit failure (a) the complete leaf is
already visible at its public name, (b) the empty 0700 `.t64-art-stage-*` directory stays
in the artifact directory, and (c) all three callers report failure anyway and call
`abort` as if it withdrew the publication: the state export maps it to
`state-export-publish` and calls `abort` (2105-2118), the save export to the publisher
error name (2138-2147), the replay record and the play report to the publisher error
(2169-2176, 2201-2209). The specs promise the opposite: `docs/spec/save-media.md` line
379 ("every other outcome aborts the publisher and publishes nothing") and the comment at
`t64_real.c` 2081-2085 ("every failure ... removes the private stage").

Concrete scenario: a filesystem whose directory `fsync` fails (EIO after a writeback
error, or EINVAL on a filesystem without directory sync) passes `t64_artifact_preflight`
(302-399 never calls `fsync` on the parent, it only proves the no-replace rename), runs
the guest, publishes a complete blob and then ends `harness-error` /
`state-export-publish`. The next run with the same leaf is refused with `artifact-exists`,
and a harness that lists the directory sees a valid private blob next to a failure
record. On every such filesystem this is deterministic, not a race. No control in
`test_artifact_publisher.c` or the real-runner suites forces a post-rename failure.
**Fix:** Make "failure means nothing published" true or change the contract explicitly:
(1) run the stage removal on every exit after the rename; (2) when a post-rename check
fails, `unlinkat(parent_fd, leaf, 0)` only if the entry is still the held device and
inode, otherwise report the substitution without touching it; (3) add an
`fsync(parent_fd)` step to the preflight probe so an unsupported filesystem is refused
before the guest; (4) add a `T64_REAL_RUNNER_TESTING` hook that fails the post-rename
`fsync` and a control that asserts the chosen contract (leaf absent and stage removed, or
leaf present and result success), in the same setup as an accepted baseline.

#### WR-02: The runner's own exported state re-imports into a record its own checker refuses

**Severity:** WARNING
**File:** `runner/t64_real.c:1976-1992` (with the import at `1399-1442`);
`runner/t64_result_check.c:1297-1302`; `runner/tests/test_state_runner.c:1754-1775`,
`2316-2328`
**Issue:** The runner exports the state at authored completion after it has drained its
whole log, so the observer cursor sits at the log length. An import restores that cursor
and the machine's attempted-byte total, so the resumed run delivers no earlier byte:
`drain_output` returns `copied_bytes` 0 and `total_attempted_bytes` 20 for the admit
row. The finalize block then writes `log_bytes = 0`, `log_bytes_total = 20` and no
`log_sha256` (1976-1992; the digest is set only when the two counts are equal or the
detail is `output-overflow`). The checker refuses that shape: line 1297 requires
`log_bytes == log_bytes_total` unless the stop is `output-overflow` or the detail is
`capture-invalid`. The result is that the one artifact the runner can publish with
`--state-export` cannot be imported by the runner into a valid record: the child writes
a record that its own checker rejects, and only the supervisor turns it into the closed
`report-error` / `report-invalid` form. The test asserts exactly that as the accepted
outcome (`test_state_runner.c` 1766-1775, and the tracer baseline row `none` at
2356-2361). By contrast, the saturated-total case at `t64_real.c` 1963-1975 already
emits the closed unknown-log form (null digest, zero counts) that the supervisor accepts
(`t64_supervisor.c` 1282-1293). The end state is equal (tick and digest match run A), so
this is an evidence-composition gap, not a state-corruption bug, but it means CORE-04
"export and import the same state" is demonstrated through the runner only for a state
the core API produced.
**Fix:** In `finalize`, when `output_total_exact` is true, `output_total != output_length`
and the detail is not `output-overflow`, take the same branch as the saturated total
(zero counts, no digest, `host-error`), so a resumed run of a runner-published state ends
in the accepted `harness-error` / `host-error` record. Change the `import-accepted-continues`
and tracer-baseline expectations from `report-invalid` to that record, with the checker
accepting it in the same setup.

#### WR-03: The result protocol still states the pre-Phase 9 pass rule

**Severity:** WARNING
**File:** `docs/spec/result-protocol.md:560-563` against `runner/t64_real.c:853-863`,
`runner/t64_result_check.c:843-892` and `runner/t64_real_fixture.c:64-153`
**Issue:** The spec says that exactly the manifest fixture, "its expected 30-byte
transcript (86594505...)", `positive-match` and nonzero retirement can produce `pass`.
Since 09-02 (K4) a registered row is judged by its own pin: fifteen rows of the table
carry a pinned length and SHA-256 (20, 68, 72, 101, 144, 191 and 192 bytes) and produce
`pass` under them, in both the runner (`transcript_matches_pin`) and the checker
(`real_row_pin`, used by `real_pass_matches_registered_fixture` and the
authored-completion branch). The tests assert 20-byte passes
(`test_phase09_runner.c` 779-780, 899-900). The document that defines what a `pass`
record means is therefore wrong for every Phase 9 row, and a claim writer or the next
checker author following it would either reject valid records or weaken the pin check.
The same file's closed `detail` list (514-519) is not updated for the vocabulary the
code now accepts (the artifact and state-export names are added only in later bullets,
530-545).
**Fix:** Rewrite 560-563 to say that the pin is the registered row's own length and
SHA-256, with the legacy 30-byte hello pin for rows that carry none, and point to the
fixture table. Add a drift control that compares the spec's pin sentence with
`t64_real_fixture.c` (the repository already has spec-versus-source checks for the other
vocabularies).

### Info

#### IN-01: A legal replay-record interval can be guaranteed to fail late with a generic `host-error`

**Severity:** INFO
**File:** `runner/t64_supervisor.c:606-612`, `runner/t64_real.c:1304-1308` and
`1816-1825`, `runner/t64_replay.c:804-807` and `855-876`, `runner/tests/test_replay.c:1318-1320`
**Issue:** The supervisor and the child accept any interval that is a multiple of 16 not
above the bound, but the recorder allows 4,096 checkpoints (`T64_REPLAY_MAX_CHECKPOINTS`).
On a row with a large bound (for example the replay-mix row, whose bound can reach
4,000,000) an interval of 16 is accepted, the guest runs 65,552 ticks and the 4,097th
checkpoint latches `T64_REPLAY_CHECKPOINT_LIMIT`; the run ends with the generic detail
`host-error` and nothing published. The op cap has the mirror property: 65,536 input ops
cannot fit the 4 MiB byte cap (the test comment at 1318-1320 says so), so the byte cap
bites only at `t64_replay_recorder_finish` after the guest has run. Both fail closed;
neither names its cause in the closed record.
**Fix:** Refuse, at option parse, an interval below `bound / 4096` when the row cannot
complete earlier (or document the bound in `docs/spec/replay.md`), and give the recorder
failures their own closed detail instead of `host-error`.

#### IN-02: Dead and duplicated replay code is compiled into the production runner

**Severity:** INFO
**File:** `runner/t64_replay.c:1504-1597`, `1601-1620`, `1205-1353`, `1380-1383`;
`runner/t64_real.c:1631-1860`
**Issue:** `t64_replay_publish` has no caller anywhere in the repository (the runner
publishes through its own `publish_report` at `t64_real.c` 1163-1172), and
`t64_replay_play`, `t64_replay_report_check` and `t64_replay_player_end_at` are used only
by `test_replay.c`, yet all four are linked into `t64-runner-real`. The in-process driver
also duplicates the runner's play loop with different terminal logic (the driver sets
`GUEST_BOUND` when `tick == end_at`, line 1585; the runner only when
`current_tick == guest_ticks`, `t64_real.c` 1780-1782). The in-process unit cases
that play a replay (`identity-binding`, `at-matters`, `partition`,
`queue-full-divergence`, through `play_on` and `play_golden`) validate the driver; the
runner loop is covered only by the real controls, so a defect in the runner's op-aligned stops would
not be caught by those cases.
**Fix:** Drive the unit cases through the same stepping function the runner uses, or
move the in-process driver and `t64_replay_publish` out of the production module into the
test target.

#### IN-03: The tracer prints an expected status it never observes

**Severity:** INFO
**File:** `runner/tests/test_state_runner.c:2260-2262`, `2337-2348`
**Issue:** The result line prints `expected=STATE_RANGE/OBSERVER_RANGE` (status and
reason from the inventory row), but only the reason is compared (`2347`); the runner's
prelaunch record cannot carry the status, so `STATE_RANGE` is never observed in the
runner path. A reader of the tracer line (seed 1, `build/gate/09-10-tracer.txt`) can take
both halves as checked. The reason-to-status mapping is a core fact
(`core/t64_state.c` `state_status_for`), not a runner one.
**Fix:** Print only the reason in the `expected=` field, or call the in-process import in
the same binary and compare the status with the inventory column there.

#### IN-04: Documentation drift in two sentences

**Severity:** INFO
**File:** `runner/t64_real.c:1115-1118`; `docs/spec/state-container.md:552-556`
**Issue:** The twin-only fault comment says "The production runner never publishes; with
the pair it only runs the preflight", which has been false since 09-05, 09-07 and 09-08.
The child-only names paragraph says a caller never passes the descriptor options "(the
runner refuses a descriptor it did not receive from its supervisor)". The runner checks
the shape of a descriptor (unlinked regular file, read-only, offset 0, `read_snapshot`
218-276) and cannot know its origin; the direct-runner tests build their own
(`test_state_runner.c` 1625-1635, `test_replay.c` 2726-2752), and a directly started
child skips the supervisor's exact-size and SHA-256 verification for the state and replay
snapshots (`exact_size` 0).
**Fix:** Correct both sentences (the second to "accepts only an unlinked read-only
regular file; the size and SHA-256 are verified by the supervisor only").

#### IN-05: Five of the seven runner tests never remove their run directories

**Severity:** INFO
**File:** `runner/tests/test_replay.c`, `test_phase09_runner.c`, `test_artifact_publisher.c`,
`test_pad_poll.c`, `test_save_fixtures.c`
**Issue:** `test_state_runner.c` (`remove_tree`) and `test_save_roundtrip.c`
(`remove_dir_tree`) delete every blob and run directory they create; the other five
contain no removal call. Their run directories under the per-test evidence directories
(`runner/CMakeLists.txt` 575, 637, 660, 684, 722: `09-02-evidence`, `09-03-evidence`,
`09-07-evidence`, `09-04-evidence`) keep authored replays (`.rpl`), 100,000-byte probe
files, retained stages and results across runs. The content is authored, not user data,
and the directories are in the ignored build tree, so the effect is disk growth and
stale-state risk, not leakage.
**Fix:** Share one `remove_tree` helper (the tests already duplicate `read_file`,
`join_path`, `prepare_evidence`, `parse_record`, `checker_accepts` and `open_snapshot`)
and call it at the end of each case.

#### IN-06: Two Phase 8 carries in `t64_supervisor.c` matter for the Phase 9 lenses

**Severity:** INFO
**File:** `runner/t64_supervisor.c:1105-1120`, `2295-2302`
**Issue:** `workset_cleanup` honors the environment variable `T64_REAL_KEEP_WORKSET` in
the production binary (not under a testing macro) and then leaves the report and the
capture in the work directory; the capture holds the closed import reason line and the
guest transcript. And in real mode a failed `--meta` write is ignored (`(void)atomic_write`),
so the supervisor can exit 0 with a valid result while an older `meta.json` at the same
path stays in place (synthetic mode reports the failure at 2620-2625). Both predate
Phase 9 (fb14108 and earlier) and neither leaks blob bytes today.
**Fix:** Guard the retention switch with `T64_SUPERVISOR_TESTING`; treat a failed meta
write in real mode as exit 1 or write a null meta record.

---

_Reviewed: 2026-11-20T06:05:00Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: deep_

## Lane L4

# Lane L4: Code Review Report

## Summary

Scope: the 22 paths of `build/gate/09-10-review-scope-L4.txt` read in full (the large Python files, the CMake
registrations, every builder and scanner) plus the direct callees outside the list that a finding needed
(`core/t64_pi.c`, `core/t64_save.c`, `core/t64_si.c`, `core/tests/test_state_import.c`, `tests/t64_test.h`,
`runner/t64_real.c`). I also read `docs/evidence/phase07-oracle-authority.json` (read-only owner-stop file). Nothing was
edited. Read-only checks run: the no-HLE guard in source-only mode (PASS), `check-ci-yml.sh` (ok),
`list-membership-sites.py --check` (0), `k5-mutants.py --check-anchors` (0), `check-rdp-oracle-authority.py
--expect-blocked` (valid, blocked), `rights-scan.sh --tree` (findings=0), and a Python recomputation of all 17 freeze
hashes against the `phase09-fh` tag. No qualification, no suite run.

The harness is carefully built: the qualifier fails closed in every mode, the exact-row and closed-key-set
projection is checked field by field, the shared native stage binds `tests`, `tools`, `docs/spec` and
`CMakePresets.json` (so the narrower `SOURCE_PATHS` tuple is not a gap, see the seed disposition), the CI scanner pins
every new command, the upload list and the order, and the inventory is an exact set. One defect will stop the final
native qualification deterministically (CR-01). Two weaker controls could let a later gate pass on thinner evidence than
the plans intended (WR-01, WR-02). The oracle authority files changed only by identity rebinds. Necessity
(`necessity-review.md`) was read; no necessity finding was raised, and the L4 contract has no `necessity:` key.

## Narrative Findings (AI reviewer)

### Critical Issues

#### CR-01: The shared native stage kills the full ASan CTest at 600 s; one Phase 9 test needs at least 631.8 s

**Severity:** BLOCKER
**File:** `scripts/qualify-rdp-vi.py:419-430` (limit at line 425), `scripts/qualify-rdp-vi.py:388-390,435-437`,
`scripts/qualify-phase09.py:1638-1643,1749`, `core/CMakeLists.txt:1093-1100`
**Issue:** `_native_command_plan` item 0 of `scripts/qualify-phase09.py` runs `qualify-rdp-vi.py --native`. That stage runs
`ctest --preset asan --no-tests=error` under `_run_recorded(..., timeout_seconds=600.0)`
(`scripts/qualify-rdp-vi.py:419-426`); a timeout raises `Refusal("full ASan/UBSan CTest inventory failed, timed out, or lacks
JUnit")` (`:429-430`). Plan 09-09.1 measured `unit.core.state.import` at 631.8 s under the asan preset and raised only
that test's CTest `TIMEOUT` to 1800 (`core/CMakeLists.txt:1100`; measurement in `09-09.1-SUMMARY.md` lines 177 and 189).
A single test that needs 631.8 s cannot finish inside a 600 s wrapper, and with four jobs sharing the CPU the full-suite
wall time is at least that. The wrapper limit is the Phase 7 value, unchanged since: the last rehearsal measured the
whole ASan CTest at 389.7 s (`09-03.1-SUMMARY.md:209`), before the 09-08 and 09-09 state tests existed. Path: `python3 -B
scripts/qualify-phase09.py --native --sha <HEAD> --out <private>` -> `shared-native-gates` -> gate (ok) -> asan configure
and build -> asan CTest -> at t = 600 s `_run_bounded` kills the process group -> `Refusal` -> no native record is sealed, so
`--collect-hosted` and `--verify-evidence` can never run. The failure is closed (no false pass), but
`scripts/qualify-rdp-vi.py` is a frozen, bound file (IDENTICAL against `phase09-fh`, in `NATIVE_INPUT_PATHS`), so the fix
must land in the 09-10 fix cycle (D-46); an edit after F2 is a declared qualification reset and costs a native plus hosted
cycle (D-06, F-098). The same hard-coded block has two more limits that nobody re-derived for Phase 9 (IN-01).
**Fix:** In `scripts/qualify-rdp-vi.py` raise the asan full-CTest limit above the largest registered `TIMEOUT` (1800) with
margin (for example 2400), and raise the outer `shared-native-gates` limit in `scripts/qualify-phase09.py:1641` and the `max_timeout=2400` bound in
`_verify_rdp_native_receipt` (`:1749`; the plan loop in `_verify_native_record` follows `_native_command_plan`) so that
gate + asan build + asan CTest + gcc13 cannot exceed it (for example 3600; today the worst case 600 + 600 + 1800 + 120
already exceeds 2400). Update the pinned
assertion at `scripts/tests/test_phase09_qualification.py:899`. Add a control that reads `TIMEOUT` from
`ctest --test-dir build/asan --show-only=json-v1` and requires the wrapper limit to exceed the maximum, so a later TIMEOUT
raise cannot silently outgrow the wrapper again.

### Warnings

#### WR-01: Nothing in the qualified command set runs the full state-import matrix; 12 of 16 boundaries get one partition size

**Severity:** WARNING
**File:** `core/CMakeLists.txt:1093-1100`, `core/tests/test_state_import.c:3096-3119,2243`,
`scripts/qualify-phase09.py:1638-1661`
**Issue:** D-32 requires T3 continuation at one-shot and 1/7/31-tick partitions for the boundary set, and plan 09-09.1
(`09-09.1-PLAN.md:59,140,319`) put `T64_STATE_MATRIX_FULL=1` into the sanitizer-preset registration of
`unit.core.state.import`. The registration sets only the fuzz and sweep counts (`core/CMakeLists.txt:1099`);
deviation 5 (`09-09.1-SUMMARY.md:221`) dropped the matrix flag with the rationale "no added memory-safety coverage". That
answers the wrong axis: the full matrix is continuation evidence for CORE-04, not memory-safety evidence. `partition_steps`
(`test_state_import.c:3107-3119`) adds the one-shot peer and the 1, 31 and 4,096 partitions only when `b->full ||
matrix_full()`; only B01, B07, B10 and B14 carry `full = true`, so B02-B06, B08, B09, B11-B13, B15 and B16 run only the
7-tick partition against the one-shot source reference in the gate preset, the hosted jobs and the ASan run. The native
plan runs `shared-native-gates` with only `T64_GATE_JUNIT_OUT` in its environment and adds no step that sets
`T64_STATE_MATRIX_FULL` (`qualify-phase09.py:1638-1661`); the only full run is the manual command in `09-10-PLAN.md:327`,
which no record binds. Scenario: a regression in the dirty-peer or partition handling that appears only at a 1-tick or 31-tick
slice of, say, the AI paused-partial or EEPROM-busy boundary passes the qualified gate on both platforms and both presets,
while the claim text for criterion 3 reads as if the D-32 set ran.
**Fix:** Either (a) set `T64_STATE_MATRIX_FULL=1` in the registration of `unit.core.state.import` (09-09.1 measured the
boundaries suite at 18.2 s against 11.3 s, about +7 s per job; ASan already has TIMEOUT 1800), or (b) add one recorded native
plan step, for example `normal-state-import-full-matrix` (`ctest --test-dir build/gate -R '^unit\.core\.state\.import$'
--output-junit <private>` with `T64_STATE_MATRIX_FULL=1` in the recorded environment), verified by `_verify_native_record`
like the other steps. Add a control to `test_phase09_qualification.py` that rejects a native plan without it. If the owner
keeps the sample, the claim block must say "sampled partitions" for 12 rows.

#### WR-02: The no-HLE guard's PI pin lost the SRAM write; the new RDRAM writers' invalidation is neither pinned nor tested

**Severity:** WARNING
**File:** `scripts/check-rsp-no-hle.py:770,950-956,1007`, `core/t64_pi.c:296-312`, `core/t64_save.c:210-234`,
`core/t64_si.c:291-318`, `scripts/tests/test_rsp_no_hle.py:113,759`
**Issue:** 09-04 re-pinned `core/t64_pi.c :: t64_pi_complete` and the seed text treats that as covering the SRAM
directions ("save PI completion injected-effect mutation"). In the current body the SRAM-to-RDRAM copy and its
`t64_rdp_memory_coverage_invalidate` and `t64_cpu_link_invalidate_range` calls are not in `t64_pi_complete`; they moved to
`t64_save_sram_dma_apply` (`core/t64_save.c:221-226`), which has no body pin. The SI RD64B completion
(`core/t64_si.c:303-317`) writes RDRAM and invalidates the same two ledgers and is likewise unpinned; the roster only
admits `t64_save.c` and `t64_si.c` by file name. The guard's pass line "accepted CPU/cache/PI/SP writes invalidate
per-instance coverage sidecar" (`check-rsp-no-hle.py:1007`) is printed unconditionally, unlike the depth-commit check at
`:950-956` which does assert the token. A grep for `coverage` in `core/tests/test_save.c`, `test_si.c` and
`test_save_transfer.c` finds nothing, so no Phase 9 DMA test observes the invalidation either. Scenario (by reading, not
run): delete `core/t64_save.c:224-225`; the guard stays PASS, the Phase 9 DMA tests stay green, and a later RDP blend or VI
read of a framebuffer region that SRAM DMA rewrote sees stale coverage marks, and an LL/SC pair on that RDRAM word does not
see the intervening write.
**Fix:** In `_check_source_route` add token checks for `t64_rdp_memory_coverage_invalidate` and
`t64_cpu_link_invalidate_range` in the SRAM-to-RDRAM branch of `t64_save_sram_dma_apply` and in the RD64B branch of
`t64_si_complete_event` (the pattern of `:950-956`), or pin those two bodies; add a seed to `test_rsp_no_hle.py`
that removes each call; add one behavior test per writer (DMA into a coverage-bearing region, then a coverage read).

### Info

#### IN-01: The other hard limits of the shared native stage were not re-derived for Phase 9 growth

**Severity:** INFO
**File:** `scripts/qualify-rdp-vi.py:388-390,435-437`, `scripts/qualify-phase09.py:1664-1676`
**Issue:** The gcc13 closure is compiled under `timeout_seconds=120.0` (`:436`). Since the Phase 7 base the changed unit set
is 64 files and 87,882 lines (the 09-03.1 rehearsal compiled 51 files and 58,518 lines in 47.7 s inside the native run and
62 to 69 s standalone, `09-03.1-SUMMARY.md:209,315`). A linear projection gives about 72 to 104 s against a hard kill at 120 s,
with `-O3` on the 5,000-line `core/tests/test_state_import.c` and the 3,000-line replay and matrix tests; nothing has measured
it since 2db1be4. The asan build limit (600 s, `:390`) was measured warm (11.0 s); a cold serial ASan build of the grown tree is
unmeasured.
**Fix:** Measure the closure and a cold ASan build once; set each limit to at least twice the measurement; fold into CR-01.

#### IN-02: The case inventory proves the case list, not that a CTest runs every listed case

**Severity:** INFO
**File:** `scripts/check-phase09-cases.py:240-256`, `docs/spec/phase09-cases.json`
**Issue:** The checker equates each binary's `--list-cases` with the union of the inventory, and `check-test-inventory.sh` equates
the registered names with a fixed set. Nothing connects a case to a registered CTest that runs it. I traced the current
tree by hand (`test_si` 1+2+8+3 cases over four names, `test_save`, `test_joybus`, `test_sched_matrix`, `test_pad_poll`,
`test_save_fixtures`, `test_replay` unit/real, the single-argument runner cases) and every listed case is reached, so this is a
limit, not a defect. Scenario: a new `test_si.c` row with `suite == NULL` lists, matches an updated inventory, and no CTest
runs it.
**Fix:** Optional follow-up for the checker: also require each case's finish name or suite to map to a registered name of the
same inventory suite.

#### IN-03: The runner build-identity input list omits headers and a source that `t64-runner-real` compiles

**Severity:** INFO
**File:** `runner/CMakeLists.txt:12-24`
**Issue:** `T64_RUNNER_BUILD_INPUTS` hashes `t64_real.c`, `t64_real_result.c`, `t64_real_fixture.*`, the capture and publisher
modules, `t64_replay.*` and `t64_save_flags.h`, but not the headers that `runner/t64_real.c` includes
(`t64_real_result.h`, widened in 09-08; `t64_verified_input.h`, changed in Phase 9; `t64_input.h`;
`t64_audio_observation.h`; `t64_video_observation.h`) nor `t64_input.c`, which is linked into `t64-runner-real`. A header-only
edit leaves `t64-runner-v1-...` unchanged. The public manifest also carries `real_runner_sha256`, so identity is not lost;
only the build-id string is incomplete.
**Fix:** Add the missing runner sources and headers to the list (a runner-CMake edit, so a bound-source change).

#### IN-04: The public hosted manifest carries no rehearsal marker

**Severity:** INFO
**File:** `scripts/qualify-phase09.py:45-49,58,1491`
**Issue:** The `rehearsal` stamp exists only in the private native and hosted records and on stdout. The closed public key set
has no such field, so a manifest from a rehearsal SHA and one from the final SHA are distinguishable only by `workflow_sha`.
Binding is by SHA and the pair verifier checks the stamp in both private records, so no wrong pass follows; a reader of the
uploaded artifact alone cannot tell.
**Fix:** None required for correctness; if the key set ever opens (it is frozen), add a `rehearsal` boolean.

#### IN-05: The committed first-pair record is coupled to authored rows that may still change

**Severity:** INFO
**File:** `scripts/tests/test_phase09_qualification.py:1316,1340`, `scripts/qualify-phase09.py:94-171`
**Issue:** `test_real_pair_accepted_and_context_bound` asserts `rows == HOSTED_ROWS[:len(rows)]` against
`docs/evidence/phase09-first-hosted-pair.json`. Any data edit to an existing row (a profile hash re-pin after a fix that changes
profile identity, a fixture hash, a transcript hash) fails `harness.phase09-collect-real` until a new hosted pair is committed.
The comment "later plans append rows only" does not hold across a fix cycle that touches identity.
**Fix:** Record in the 09-10 fix plan that an identity-affecting fix regenerates the first pair (a hosted run), or compare only
the fields the pair test needs.

#### IN-06: The oracle request pin is re-bound on every fixtures-manifest edit

**Severity:** INFO
**File:** `scripts/check-rdp-oracle-authority.py:23`, `docs/evidence/phase07-oracle-authority.json:6,22-23,47`
**Issue:** Four commits (1b0a1ae, ebc0a1c, 503d95d, cec0446) rewrote `PINNED_REQUEST_SHA256`, the manifest `sha256`, its
`git_blob_id_sha1` and the `bound_authored_inputs[1]` pair because the request pins the whole `fixtures/manifest.json`. I
verified these are the only changes since the base (five leaves in the JSON, one constant in the script); `local_decision`,
`publication_decision`, `rights_audit` and every authority field are untouched; `PINNED_REQUEST_SHA256` equals the canonical
SHA-256 of the current `request` object; the recorded manifest digest and blob id equal the file; `--expect-blocked` reports
"blocked: no oracle operation is authorized". The pattern (09-02 deviation 1, unresolved) means each later manifest row costs
another owner-stop file edit, and a pin that is re-bound by whoever edits the manifest ratchets nothing.
**Fix:** Owner decision (not an L4 edit): pin only the two rows the request names, so unrelated manifest rows stop touching
the owner-stop files.

### Seed dispositions

- (e) sanitizer cost: confirmed, and narrower than the framing: raising the CTest `TIMEOUT` is necessary but insufficient
  because the shared native wrapper kills the run at 600 s (CR-01); the missing matrix flag is a separate evidence gap (WR-01).
- Freeze baseline (seed 4): confirmed 13 identical, 3 DATA, 1 LOGIC. I recomputed all 17 hashes (every record equals the
  current file; 13 equal `phase09-fh`) and read the four diffs. The three DATA verdicts are data: `check-rsp-no-hle.py`
  changed only two roster entries and five digest literals (no function body), `check-test-inventory.sh` only added name
  rows, `rights-scan.sh` only added selftest rows. FD-01 is the only logic change and is benign (see the required statements).
- 09-02 deviation 2 (`cmake` dropped from `SOURCE_PATHS`): sound; `test_source_paths_exist_and_cover_guards` requires every
  entry to exist and be tracked.
- `SOURCE_PATHS` omits `tests/`, `tools/common`, `tools/lint`, `tools/fixture`, `CMakePresets.json`: refuted as a gap.
  `_native_source_snapshot` (`qualify-rdp-vi.py:1820-1878`, re-verified by `_verify_rdp_native_receipt`) binds
  `CMakePresets.json`, `scripts`, `tools`, `fixtures`, `tests` and `docs/spec` and refuses any untracked, ignored or modified
  file, before and after every command.
- 09-03.1 deviation 9 (row key by fixture file hash plus input hash): sound; the fixture hash is read from the file named in
  the retained command, and `row_observation` compares the runner's own reported identity against the authored row.
- 09-03.1 deviation 10 (GCC closure limited to `core`, `runner`, `tools/common`): narrowed; the Phase 9 change to
  `tools/rights/t64_blobscan.c` is outside the local closure (hosted Linux gcc with Werror covers it); see IN-01 for the
  closure's time margin.
- 09-03.1 deviation 11 (LastTest retention): sound; the first check must equal the owning log byte for byte, later copies must
  be the owning log or contain no test sections.
- 09-09.1 deviations 4 and 5 (fuzz counts, matrix flag): the counts are a recorded cost decision (the ASan campaign ran 120
  iterations and two blobs of 12 plus 12 offsets); the matrix flag is WR-01.
- 09-09.1 deviation 10 (data rows only in `phase09-cases.json` and `check-test-inventory.sh`): confirmed by the diff; the
  inventory now holds exactly 45 Phase 9 names, no ASan-only Phase 9 name, and every name is registered or in the case
  inventory.
- Seeds (a) to (d), (f), (g), the tracer, and the option-pair findings R-01 to R-06: not in L4.

### Required statements

- Frozen files: no frozen file changed silently. The only logic change since the tag is
  `scripts/tests/test_rsp_no_hle.py` (FD-01): two helpers, new mutation seeds and pin-deletion controls, and loops that widen
  three existing functions. I read the whole diff; no existing check is removed or weakened, `seeds_run` must still equal the
  declared `MUTATION_SEEDS` count, and each new control proves its clean baseline first. I agree it is the only LOGIC verdict and
  disagree with none of the three DATA verdicts. `runner/t64_supervisor.c` and the CMake files changed after the tag as ordinary
  plan content and are not in the baseline.
- Guard pins: every pin in `check-rsp-no-hle.py` covers the right body (the guard passes on the tree and each seed rejects). One
  drift: after 09-04 the `t64_pi_complete` pin no longer contains the SRAM write or its invalidation, and the SI and save
  completion bodies are not pinned (WR-02).
- Wrong-reason passes: a later gate could pass on thinner evidence through WR-01 (sampled partitions, nothing recorded for the
  full matrix) and WR-02 (unguarded invalidation), and the hosted rows evidence only the SAVE-01 pad-poll rows (the other
  criteria rest on job success plus the native per-name pass records, which is the design). CR-01 fails closed, so it makes the
  gate fail for the wrong reason rather than pass for one.
- Oracle files: `scripts/check-rdp-oracle-authority.py` and `docs/evidence/phase07-oracle-authority.json` changed only by identity
  rebinds; no authority or decision field was loosened (IN-06).

---

_Reviewed: 2026-11-20T05:59:30Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: deep_

## Lane L5

# Lane L5: Code Review Report

## Summary

All 38 scope files were read in full. The eight core test files, the four Python oracles, the seven assembler
sources and the specs were cross-read against the production code they claim to test (`core/t64_si.c`,
`core/t64_save.c`, `core/t64_joybus.c`, `core/t64_pi.c`, `core/t64_digest.c`) and against each other. I ran the
existing gate binaries only for reading results and built throwaway mutants and probes in a scratch area outside
the repository (an edited object swapped into a scratch copy of `build/gate/core/libt64_core.a`, linked against the
existing test objects); no tracked file was touched, `git status` is clean, and the only file written is this report.

What holds up. The independent Python oracles (`poll_oracle.py`, `save_oracle.py`, `roundtrip_oracle.py`,
`mix_oracle.py`) import only the standard library and reproduce the pinned transcript hashes and the hosted-pair log
hashes exactly; the case lists of every built binary in `docs/spec/phase09-cases.json` equal `--list-cases`; all
CTest names cited by the specs and the inventory are registered; the F1 golden table in
`docs/spec/machine-contract.md` equals the table in `core/tests/test_sched_matrix.c` row for row (58 rows); the
62 mutation rows and 22 report rows of `docs/spec/replay.md` equal the tables of `runner/tests/test_replay.c`; the
`fixtures/manifest.json` Phase 9 rows resolve (source revision, blob equal to HEAD, source-content hash recomputed
from the builder's input list, built ROM bytes equal to the pinned SHA-256 and size); the status ranges 20 to 25 and
26 to 33 equal the public header; `list-membership-sites.py --check`, `check-claims.sh`, `pii-scan.sh --tree`,
`rights-scan.sh` and the manifest checker are clean; the claim skeleton has only `t64-claim-example` fences and
none of the three reserved words; no blob, save, replay, state, PIF or IPL file is tracked; no workstation path,
user name or email appears in the scope (the public repository slug appears in the manifest and the hosted-pair
record, as everywhere else in the tree). The hand-derived section lengths of the F1 table (32, 4,194,308, 8,200,
276, 16 + 40 n, 43,724, 404, 20 + storage) re-add correctly.

What does not hold up. One blocker: the canonical v25 digest (and so the container, the replay checkpoints, the F1
goldens and the strip differential) is blind to RDP combiner, blender and memory-coverage state that the family
runs and that the spec says is hashed (CR-01, confirms seed (a) with my own reproduction). Four warnings are tests
or spec text that do not prove what they claim: a promised SI invariant with no control (WR-01), the "no data
written on timeout or absent" rule that every fixture frame hides behind a 0xFF fill (WR-02), a digest-visible DMA
side effect with no spec text and no test (WR-03) and stale scope statements in the SI spec (WR-04).

## Narrative Findings (AI reviewer)

### Critical Issues

#### CR-01: The canonical digest omits RDP combiner, blender and memory-coverage state on descriptors 11 and 12; the spec says it is hashed and nothing in this lane can see the gap

**Severity:** BLOCKER
**File:** `docs/spec/machine-contract.md:318-319,340-341,918-920,934`; `core/t64_digest.c:301-315,649-684`;
`core/tests/test_sched_matrix.c:2948-3009`; `core/tests/test_digest_v25.c:766-827`; `core/tests/test_si_profile.c:137-138`
**Issue:** The walker guards the combiner block (`has_v19_state`, `has_v20_state`: exact model/state tuples 14/19 and
15/20) and the memory-coverage table plus the combiner registers (`has_v20_state || has_v21_state`, where `has_v21_state`
is `t64_has_rdp_depth_profile`) with predicates that are false for the Phase 8 audio profile (descriptor 11) and for
all four composite-family ids (descriptor 12), although `test_si_profile.c:137-138` pins `P_COMB | P_BLEND` true for the
family and `rdp_memory_coverage` is allocated. The spec says the opposite: the member audit rows
`rdp_memory_coverage | 8, when the profile has the blender side table` (line 919) and `rdp | 8 | ... combiner, blender
and depth state as the profile has them` (line 934), "Each version includes everything the earlier ones do for the
state its profile has" (line 318) and "v16 to v23: the RDP/VI block (... combiner, blender and depth state ...)" (line 340).

Reproduction (probe linked against the unmodified `build/gate/core/libt64_core.a`, authored asset only):
1. Stem id `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1`, admitted, digest D0. XOR `machine->rdp.environment_color`,
   `combine_mode`, `blend_color`, or `machine->rdp_memory_coverage[0]`: digest stays D0 ("UNCHANGED"). On the
   descriptor 10 id `...-rdp-vi-filter-v9` the same flips change the digest.
2. A flip sweep over the first byte of every member of `t64_rdp_state` (100 members): 24 change nothing on the stem id
   against 4 on filter-v9 (`pending_triangle_words`, `triangle_last_coverage_x/y/mask`). The 20 extra blind members are
   `primitive_lod_fraction`, `environment_color`, `fog_color`, `blend_color`, `combine_mode`, `key_center`, `key_scale`,
   `key_width` and their seven `*_valid` flags, plus the five depth members (`depth_image_address`, `primitive_z`,
   `primitive_dz`, `depth_image_valid`, `primitive_depth_valid`).
3. Same digest, different future: two stem machines built exactly like `build_vi_frame` in `test_digest_v25.c:653-699`
   (four authored 5551 pixels at 0x3000, same VI registers), one with `t64_rdp_memory_coverage_write(machine, addr, 2, 8)`
   on the four pixels, one without. Both digests equal (`21e81299...cb731`); after the first VI field the packet
   metadata differs: `source_bytes` 8 against 16.

So two machines the digest declares identical continue differently. CORE-04 ("continue equal") and CORE-05 ("reproduces
the recorded checkpoint digests") are stated on exactly this digest; the importer zeroes the same members; the 68-row
member audit presents two of them as carried. None of this lane's controls can catch it: the strip differential
(`test_digest_v25.c`) compares the v25 walker with the v24 walker, which share the blind spot; the F1 goldens
(`test_sched_matrix.c:2949-3008`) pin whatever the walker emits ("the walker compared with itself", as the spec itself
says at `machine-contract.md:893-894`) and sections 2 to 7, 10 and 11 have hand lengths but section 8 does not; the SI and
save flip tests flip only their own section's fields; no test flips the members of the carried rows of the audit table.
**Fix:** Record a decision first (it is a recorded F1 defect fix, D-06): (a) extend the v25 canonical RDP block with the
combiner, blender and memory-coverage members for descriptor 12 (new state version or a recorded F1 deviation with the
goldens regenerated), or (b) make export and import fail closed for any state whose RDP combiner or coverage members are
non-default, or (c) bound the CORE-04 and CORE-05 claim scope to states in which those members are provably default and
say so in `docs/claims/phase09.md`. In every case correct `machine-contract.md` lines 318-319, 340-341, 918-919 and 934
to name the exact-tuple guards. Add the control that proves it: a digest-completeness test that, for each member of the
audit rows marked carried and for every scalar member of `t64_rdp_state`, flips one byte on each of the four family ids,
the AI profile and filter-v9 and requires a digest change unless the member is on a named exclusion list with a reason;
plus the marked-versus-unmarked coverage pair above, asserting equal digests imply equal packet metadata.

### Warnings

#### WR-01: "The active descriptor is immutable" has no control; a completion that reads the live latch passes every in-lane test

**Severity:** WARNING
**File:** `docs/spec/si-pif-controller.md:42`; `core/t64_si.c:303-312`; `core/tests/test_si.c:1570-1582`
**Issue:** The register table says a write to `SI_DRAM_ADDR` during a DMA changes only the latch. The only test that
writes `SI_DRAM_ADDR` while a DMA is active is `case_reset_replace` (`test_si.c:1572-1573`), and it replaces the machine
before the DMA completes, so nothing observes where the transfer lands. No other in-lane test writes the register after a
start. Reproduction: compile `core/t64_si.c` with the RD64B completion copy reading `si->dram_address` instead of
`si->active_dram_address` (line 311; builds clean under the gate warning flags with `-Werror`), swap it into a scratch
copy of the core archive and link the existing test objects. Results, all green: `test_si --suite device` (1,068 checks),
`--suite latch` (532,278), `--suite atomic` (183), `test_save --suite device` (7,908), `test_joybus --suite parser` (229),
`test_sched_matrix --case cross-device` and `--case partition`. The mutant is not equivalent: start RD64B with the latch
at A, write the latch to B before the completion tick, and the image lands at B.
**Fix:** Add a case to `unit.core.si.device`: accepted baseline (start RD64B at A, complete, image at A), then the same
setup with the latch rewritten to B at start plus 1,000 ticks, asserting the image at A, RDRAM at B untouched, and the
latch reading B. Do the same for WR64B (source read at start, so a later latch write changes nothing).

#### WR-02: The "timeout or absent writes no receive data" rule cannot be told from "fills the receive area with 0xFF" because every frame uses a 0xFF fill

**Severity:** WARNING
**File:** `docs/spec/si-pif-controller.md:129-130`; `docs/spec/save-media.md:61-62`; `core/tests/test_save.c:243-252,1118-1147,1473-1474`;
`core/tests/test_joybus.c:303-335,629-645`; `fixtures/save/save_oracle.py:70-73`; `fixtures/save/eeprom.S:273-354`
**Issue:** The specs state that a timeout or absent answer writes no data and "the 0xFF fill bytes stay as they were".
`build_frame` in `test_save.c` fills the receive area with 0xFF (line 249), `test_joybus.c` frames use 0xFF fill, the
`eeprom.S` frames end every receive area with `.byte 0xff` runs, and the oracle's busy read returns `0xff * 8`
("fill bytes untouched"). The expected value of an untouched byte and of a written 0xFF are therefore the same. Only the
SI absent case on channel 1 uses a 0xAA marker (`test_si.c:1234-1250`). Reproductions (scratch archive swaps, gate flags,
`-Werror`): (a) a memset of 0xFF over the receive area in the joybus TIMEOUT branch (`core/t64_joybus.c:285-286`):
`test_si --suite device`, `test_save --suite device` (7,908), `test_joybus --suite parser` (229) and
`test_sched_matrix --case cross-device` all pass; (b) the same memset in `t64_save_channel4_execute` for the TIMEOUT
class and the busy read (`core/t64_save.c:116-117,130-132`): `test_save --suite device` and the matrix pass; (c) memset in
the ABSENT branch as well: only `test_si --suite device` fails (2 checks), through its 0xAA frame.
**Fix:** Use a distinct non-0xFF fill (for example 0xA5) in `build_frame`, in the `test_joybus.c` timeout and absent
frames and in the `unanswered_rows` and busy-read rows, and keep the hard-coded 0xFF only where the spec says a response
byte is 0xFF. For the guest-visible fixtures, extend the oracle's EEPROM busy read and unanswered rows to carry the fill
as a parameter and give the ROM frames a 0xA5 receive fill, so the transcript bytes show "untouched" (re-pin the ROM rows).

#### WR-03: SI read completion and SRAM-to-RDRAM DMA invalidate the CPU link and the RDP coverage table, with no spec text and no test

**Severity:** WARNING
**File:** `core/t64_si.c:313-316`; `core/t64_save.c:224-225`; `docs/spec/si-pif-controller.md` and `docs/spec/save-media.md` (silent)
**Issue:** Both new RDRAM writers call `t64_rdp_memory_coverage_invalidate` and `t64_cpu_link_invalidate_range`. Those calls
change digest-visible state on the family (the memory-coverage table is behavior-bearing, see CR-01) and guest-visible
behavior (a store-conditional after the DMA). Neither spec mentions them, and no in-lane test observes them. Reproduction:
remove both pairs of calls in a scratch copy of the archive; `test_si` (all suites), `test_save --suite device`,
`test_save_transfer --case in-flight-real`, `test_sched_matrix --case partition`, `test_cpu_link` and `test_pi_dma` all stay
green. The ROM PI path has such tests (`test_cpu_link.c`, `test_rdp_blender.c`); the two new writers have none.
**Fix:** Specify the effect in both documents (an RDRAM write by SI or SRAM DMA clears the LL link when the span covers
it and resets the coverage marks of the span), then add one control each: an LL, an RD64B or SRAM read DMA over the linked
word, an SC that must fail, and a coverage mark written before and read as cleared after, each next to an accepted
baseline with a non-overlapping span.

#### WR-04: `si-pif-controller.md` still says channel 4 is absent for every profile and that only channel 0 has a device

**Severity:** WARNING
**File:** `docs/spec/si-pif-controller.md:26-29,112-115`
**Issue:** Lines 27-29: "The cartridge channel (channel 4) answers 'absent' for every profile id in this slice; the
save devices of a later slice replace that answer behind the same internal interface." Lines 112-115: "Only channel 0
has a device in this slice ... Every other channel ... reports no device." Plan 09-04 shipped the replacement: on the e4k
and e16k ids channel 4 answers info, read and write and the timeout class (`core/t64_joybus.c:113-135`,
`core/t64_save.c:73-150`, `docs/spec/save-media.md:43-73`, `core/tests/test_save.c`), and the same spec's timing table and
Determinism checks still read as if the slice ended at 09-03. A reader of the SI spec alone gets the wrong device map.
**Fix:** Rewrite both passages: channels 0 to 3 as now, channel 4 answers absent on the ids without an EEPROM (stem, s32k)
and is answered by the EEPROM on the e4k and e16k ids, with a pointer to `save-media.md`; drop "in this slice".

### Info

#### IN-01: Controls that reach only a local helper, and one accept half that asserts too little

**Severity:** INFO
**File:** `core/tests/test_si.c:340-364,1134-1136,1314-1322`; `core/tests/test_joybus.c:533-573`; `core/tests/test_save.c:1599-1601`;
`core/tests/test_sched_matrix.c:2599-2605,3153-3191`
**Issue:** Several "wrong-value" controls prove only that a test helper rejects a wrong literal: the all-false "swapped"
direction tuple (`test_si.c:1136`, which is not what a swapped model would produce), `compare_timing(+1)`, `tick_matches(+1)`,
`count_matches` with generation 3, the `swapped_decode` helper in `test_joybus.c`, and the F1 controls, which perturb a copy
of the stream and reach `golden_label` rather than the walker. They are harmless but do not show that the production path
under test would be caught. The accept halves of `budget_room_pair` and `budget_sequence_pair` (`test_si.c:352-356,378-382`)
assert only "not refused for capacity", so a store that failed for another reason would pass the baseline. The direction
itself is covered by the literal four-way memcmp (`test_si.c:1117-1133`), so there is no hole in the actual direction check.
**Fix:** Keep the helper controls but label them as comparator self-checks; assert `T64_STATUS_OK` and
`retired_instructions == 4` in the accept halves.

#### IN-02: Stale header comment in the ledger test

**Severity:** INFO
**File:** `core/tests/test_sched_ledger.c:4-8,218-220`
**Issue:** The file header says the SI rows "are declared reserved and must not appear until a reservation site exists"
and then, in the next sentence, that they were reserved until 09-03; the table (lines 150-151) and `case_ledger` now require
them to be present, and `compare_ledger`'s comment still describes reserved rows. The code is right; the comment misleads.
**Fix:** Delete the first sentence and the "no reserved row may be observed" remark, or say the reserved mechanism is unused.

#### IN-03: `T64_CHECK(true)` pads the `checks=` totals that the review summaries cite

**Severity:** INFO
**File:** `core/tests/test_sched_matrix.c:1186,1355,2462,2596,2621,2640,3102,3143`
**Issue:** Eight success arms count a vacuous check so a green run and a failing run both increment the counter. Totals such as
`checks=1153376` are quoted as evidence in the plan summaries and the 09-10 drift report; they include these.
**Fix:** Drop the no-op checks or stop quoting raw check counts as a strength measure.

#### IN-04: Dead digest and literal backslash-n in diagnostics

**Severity:** INFO
**File:** `core/tests/test_save.c:720,734`; `core/tests/test_si.c:821,872`
**Issue:** `dma_refusal_rows` computes `before` (a full digest) and discards it with `(void)before`; the six refusal rows then
pin the status, the busy bit and the storage but not the sticky error or any other field (the existing PI behavior also
rewrites the length latch on refusal, `core/t64_pi.c:96`, which is why the digest cannot simply be compared). Two
diagnostics in `test_si.c` use `\\n`, so they print a backslash and an `n` and no line break.
**Fix:** Restore the latch as `test_save.c:987` does and compare the digest, or delete the dead capture; use `\n`.

#### IN-05: PD-3 write_generation: the "saturating" clause has no control, the increment exists twice, the public comment omits the state path

**Severity:** INFO
**File:** `core/t64_save.c:453-455`; `core/t64_state.c:2867-2870`; `docs/spec/save-media.md:330-338`;
`core/include/tendies64/t64_machine.h:552-554`
**Issue:** Seed (g) narrowed. The decision is consistent across code and both specs (one increment per successful save or
state import on a profile with a device, a refusal never touches it, replacement resets it, it stays out of the digest and
the container; all of that is tested for the save path in `test_save_transfer.c:475-485,689,721,1337,1442,1634`). Open points:
the saturating clause (`!= UINT64_MAX`) is documented as "saturating" but no test sets the counter to `UINT64_MAX` and imports
(the state is unreachable in practice: 2^64 imports), the same expression is written twice, and the public header says the
counter "counts successful imports" without naming the state import.
**Fix:** One shared helper; one direct-state control at `UINT64_MAX` in `test_save_transfer.c`; one sentence in the header.

#### IN-06: Mutant results asserted in the specs cannot be reproduced from the tracked tree

**Severity:** INFO
**File:** `docs/spec/si-pif-controller.md:262-266`; `docs/spec/save-media.md:411-429`
**Issue:** The specs state, in the present tense, that nine SI mutants, six save mutants and seven transfer mutants each
compile under the gate warning flags and fail a named comparator. Only `scripts/k5-mutants.py` (plan 09-06, nine rows) is
tracked; the 09-03 to 09-05 drivers lived in an ignored build directory and the results exist only as tables in the plan
summaries. This review found survivors in the same files (WR-01 to WR-03), so the sets were not exhaustive.
**Fix:** State that the results are historical records in the summaries, or commit the mutant list as data next to
`k5-mutants.py` and add the three survivors above.

#### IN-07: One sentence of the input paragraph contradicts the family behavior

**Severity:** INFO
**File:** `docs/spec/machine-contract.md:626-636`
**Issue:** The paragraph says that on the family the dispatched input event feeds the per-port latch the SI samples, and
then that "Input records are observer-only". On the family the latch is canonical state (section 10) and guest-visible.
**Fix:** Scope the sentence to the profiles without the family tuple.

#### IN-08: `mix.S` accepts any spin count; `admit.S` checks its own

**Severity:** INFO
**File:** `fixtures/replay/mix.S:31-33,163-168`
**Issue:** `T64_MIX_SPIN` has no range assertion: 0 wraps the delay loop to 2^32 iterations and a value above 32 bits is
truncated by the `lui`/`ori` pair. `fixtures/phase09/admit.S:31-36` asserts its range. The committed default is pinned by
the manifest hash, so this is a build-time hazard only for an override.
**Fix:** `.if (T64_MIX_SPIN < 1) || (T64_MIX_SPIN > 0x7fffffff)` with `.error`, as in `admit.S`.

#### IN-09: Phase 9 plans keep editing the Phase 7 authority record

**Severity:** INFO
**File:** `docs/evidence/phase07-oracle-authority.json:6,22-23,47`
**Issue:** The record binds the prepared oracle request to the SHA-256 of `fixtures/manifest.json`; the current value
(`b75864fd...`) matches HEAD and `scripts/check-rdp-oracle-authority.py --expect-blocked` passes ("blocked: no oracle operation
is authorized"). Every manifest edit, including any in the 09-10 fix cycle, breaks that guard until the record is rebound
again. That is the known 09-02 open issue, not a new defect; it is a cost to plan for before F2, and the record's blocked
decision is unchanged by the rebinds.
**Fix:** Rebind in the same commit as any manifest change, or decouple the request from the whole-file hash.

## Seed dispositions

- (a) CORE-04 versus the canonical state: confirmed with my own reproduction (CR-01), and widened: the Phase 8 audio profile
  has the same blind spot (inherited since v24), the spec member audit states the opposite, and 24 of 100 RDP members are
  unhashed on the family against 4 on filter-v9.
- (b) A resumed run cannot report pass: no L5 evidence either way. The claim skeleton (`docs/claims/phase09.md:137-191`) carries
  no counts and no pins; its CORE-04 and CORE-05 blocks must not be completed until CR-01 is decided.
- (c) Field-use scan sees 72 of 472: narrowed for this lane. The scan maps fields used as indexes and lengths to import rules; it
  does not answer digest membership, which is the question CR-01 turns on, and the 68-row member audit that answers it is
  hand-written and wrong for `rdp_memory_coverage` and the `rdp` row.
- (g) write_generation: sound as decided, narrowed to IN-05; R-05 (two copies of the saturating increment) confirmed as low.
- 09-01 deviation 1 (AI mid-DMA golden): sound. `test_digest_v25.c:210-266` stops one tick before the first sample event, and
  the three digest cases pass at HEAD (124, 72 and 579 checks).
- 09-02 deviation 1 (Phase 7 request pins the manifest): confirmed, IN-09.
- 09-04 deviation 1 (seven distinct frame images): confirmed against `fixtures/save/eeprom.S` (frames i, w1, w2, w3, r1, r5, r4).
- 09-06 deviations 3 and 4 (W-ALL v_sync 16, own program): confirmed in `test_sched_matrix.c:1371-1400` (store at tick 3,008).
- Drift sweep: status ranges and the stale 09-08 note confirmed fixed; R-01, R-02, R-03, R-04 and R-06 are runner-lane items with
  no L5 evidence.
- Freeze baseline, sanitizer cost, importer rules (d) to (f): not in this lane.

## Required statements

Comparators whose expected value comes from the code under test. None of the policy or timing literals does: timings,
button bytes, block patterns, the D-24 order and the transform vectors are authored literals or independent encoders
(`first_violated_row`, `encode_section10`, `encode_section11`, `ref_parse`, the Python oracles). The following compare
production with production and are labelled as consistency or regression pins, not oracles:
`test_sched_matrix.c` `check_quantum_binding` (`t64_profile_serialize` plus `t64_sha256`) and `check_mi_cpu_sampling_order`
(`completion = T64_SI_READ_TAIL_TICKS`); `test_si_profile.c` `external_digest` and `check_ai_equivalence` (family against the AI
profile); `test_si.c` `case_cpu_store_budget` (the 8-byte SI store is expected to equal the AI window's outcome); the whole F1
golden table, the section 8 and 9 hashes, and the v25 against v24 strip differential (all production against production, and CR-01
shows they share a blind spot); the ledger's observed set.

Same-setup controls I could not confirm reach the comparator of the behavior they guard (they reach only a local helper):
`test_si.c:1136` (swapped direction tuple), `test_si.c:1315-1322` (`compare_timing` +1), `test_joybus.c:565-573`
(`swapped_decode`), `test_save.c:1600` (`tick_matches` +1), `test_sched_matrix.c:2604` (`count_matches` generation 3) and
`test_sched_matrix.c:3153-3191` (F1 controls on a copied stream). Controls I confirmed do reach their comparator: the transposed
order literals of `check_pair_row`, the `swap16`/`wordrev32` crossed vectors, the E - 1 input control, the D-24 refusal matrix after
its accepted baseline, the 0xAA absent frame, and the mutants of this review's WR-01 to WR-03 show where the existing
controls stop.

_Reviewed: 2026-11-20T06:05:00Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: deep_

