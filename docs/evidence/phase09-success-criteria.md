# Phase 9 roadmap success criteria map

Status: complete and bound to the exact qualified commit `d3211e957315afbb6bf83c91c49a111d0adbeb58` (the local and pushed tag
`phase09-f2`). The map was drafted in plan 09-10 and checked by that plan's goal-backward
verification before the F2 identity freeze; plan 09-11 completed the Pass evidence column from
the retained originals of the exact-SHA native qualification and changed no mapping. The tier of
every claim block is unit-tested or regression-pinned, and no statement of this file says
anything about hardware, other emulators or commercial software.

Bindings. Source and tag: `d3211e957315afbb6bf83c91c49a111d0adbeb58`, `phase09-f2`. Native record SHA-256
`4310bc4c0667a1d26c03d3de7e0754225ee4853872b228f76a6484ebad6fa483` (normal gate JUnit 224 of 224 passed, ASan and UBSan JUnit 226 of 226 passed).
Hosted record SHA-256 `a91c5d545a36ef95602cc477615034bbfa059b03f06d1eb8db9d21de26c5c97f`: run 44795894664, attempt 1, both jobs success on this
commit, collected and verified in final mode by `scripts/qualify-phase09.py`. The hosted pair
covers the authored pad-poll rows only and is not the count source of any claim block.

How the map is checked: `build/gate/09-11-scratch/check-criteria-map.py` (untracked, a
copy of the 09-10 checker extended in 09-11) requires every named CTest name to be registered
and listed in the test inventory, every suite to exist in `docs/spec/phase09-cases.json`, every
case name to be printed by the owner binary `--list-cases` and by the CTest command that runs
it, every named CTest name to be a passed testcase in the retained normal-gate JUnit and in the
retained ASan JUnit of the native record, every claim pair to be one of the ten blocks of
`docs/claims/phase09.md`, and the four criteria and the seven requirement ids to appear. Only
registered CTest names are in the named-test cells; a validation-only name is a case under its
registered owner.

The criterion text is quoted from `.planning/ROADMAP.md` (Phase 9, Success Criteria 1 to 4).

| Criterion | Requirements | Named tests | Independent expected-value source | Wrong-value control | Claim blocks | Pass evidence |
| --- | --- | --- | --- | --- | --- | --- |
| 1. A timed-input script drives the standard controller over SI/PIF joybus, and an authored ROM reports the expected button states at the expected guest times | SAVE-01 | primary: `runner.pad-poll` (suite `controller`; cases `poll-timing`, `accepted-poll-sequence`); supporting: `unit.core.si.device` (suite `si-core`; cases `tracer-poll`, `registers`, `status`, `pif-command`, `dma-timing`, `refusals`, `reset-replace`, `digest-section`), `unit.core.joybus.parser` (suite `si-core`; cases `frame-vectors`, `controller-responses`, `button-vectors`, `channels-and-ports`, `total-bounded`), `unit.core.input.latch-partition` (suite `si-core`; cases `latch-strictly-after`, `latch-partition`, `latch-port-decode`), `unit.core.cpu.si-store-budget` (suite `si-core`; case `cpu-store-budget`), `unit.core.si.atomic-queue-full` (suite `si-core`; cases `capacity-rule`, `atomic-queue-full`) | the authored transcript literals produced by the independent Python oracle `fixtures/pad/poll_oracle.py` (scenarios baseline, shifted, neutral and wrong-rom), and the literal joybus frame and button vectors in `core/tests/test_joybus.c` | `controls.real.pad-wrong-value` (suite `controller`; cases `same-setup-wrong-script`, `same-setup-wrong-rom`, `replacement-fixture-refused`): the accepted run and the wrong-value run share one setup and the wrong value reaches its own comparator | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1` / `phase09-pad-poll` | Passed in the retained originals of `d3211e9` (normal JUnit and ASan JUnit): `runner.pad-poll`, `controls.real.pad-wrong-value`, `unit.core.si.device`, `unit.core.joybus.parser`, `unit.core.input.latch-partition`, `unit.core.cpu.si-store-budget`, `unit.core.si.atomic-queue-full`. Claim block 1 (`ntsc-cart-4m-openipl3-rdram-b-t1-si-v1` / `phase09-pad-poll`, tier unit-tested, counted 1 of 1: row `phase09-pad-poll-si-v1`, accepted case completion at guest tick 626,225). |
| 2. EEPROM (4 and 16 Kbit, with the busy window and block rules) and SRAM work only when the profile selects them explicitly. Unknown device requests are refused with defined postconditions. Save bytes round-trip through import and export with endianness handling, and a refused import leaves the digest unchanged | SAVE-02, SAVE-03, SAVE-04 | primary: `unit.core.save.device` (suite `save-device`; cases `storage-blank`, `eeprom-tracer`, `sram-window`, `sram-dma`, `eeprom-commands`, `eeprom-blocks`, `eeprom-busy`, `eeprom-timing`, `selection`, `refusals`, `digest-section`, `reset-blank`), `runner.save-fixtures` (suite `save-fixtures`; cases `eeprom-4k-accepted`, `eeprom-16k-accepted`, `sram-accepted`), `unit.core.save.import-export` (suite `save-transfer`; cases `tracer-sram-raw`, `transform-vectors`, `import-export-roundtrip`, `refusal-matrix`, `no-caller-read`, `export-refusals`, `in-flight-real`, `replacement`, `mutation-bounded`), `runner.save-roundtrip` (suite `save-transfer`; cases `sram-roundtrip`, `format-matrix`, `eeprom-passthrough`, `import-refused`, `export-publication`, `no-save-bytes-in-result`, `argv-max-save`, `upload-allowlist-save`) | the authored transcript and transform literals of the independent oracles `fixtures/save/save_oracle.py` and `fixtures/save/roundtrip_oracle.py`, and the literal byte-order vectors of `docs/spec/save-media.md` | `controls.real.save-wrong-byte` (suite `save-fixtures`; cases `same-setup-wrong-byte-eeprom`, `same-setup-wrong-byte-sram`, `rom-profile-mismatch-refused`) and `runner.save-roundtrip` (suite `save-transfer`; case `same-setup-wrong-format`): each wrong value runs next to an accepted baseline of the same setup | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e4k` / `phase09-eeprom-rw`; `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e16k` / `phase09-eeprom-rw`; `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-s32k` / `phase09-sram-rw`; `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-s32k` / `phase09-save-roundtrip`; `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1` / `phase09-save-refusals` | Passed in the retained originals of `d3211e9` (normal JUnit and ASan JUnit): `unit.core.save.device`, `runner.save-fixtures`, `unit.core.save.import-export`, `runner.save-roundtrip`, `controls.real.save-wrong-byte`. Claim blocks 2 to 6 (unit-tested; counted 1 of 1 each for the rows `phase09-eeprom-rw-e4k-v1` (completion tick 1,567,331), `phase09-eeprom-rw-e16k-v1` (1,567,338), `phase09-sram-rw-s32k-v1` (539,986) and `phase09-save-roundtrip-s32k` (540,634), and 2 of 2 for `unit.core.save.device` and `unit.core.save.import-export` in block 6). |
| 3. Exporting and re-importing machine state reproduces the canonical digest exactly, and a recorded input replay bound to profile, build and content hash reproduces the original run's digests | CORE-04, CORE-05 | primary: `unit.core.state.export` (suite `state-export`; cases `export-layout`, `decoder-controls`, `states-and-ids`, `observer-continuity`, `idempotence-shape`, `non-interference`, `refusals`, `size-arithmetic`), `unit.core.state.import` (suite `state-import`; cases `roundtrip`, `idempotence`, `continuation-partitions`, `rdp-state-carry`, `boundary-reached`, `boundary-roundtrip`, `boundary-continuation`, `mid-tick-resume`, `save-media-roundtrip`, `observer-flag`, `drop-observers-outcomes`), `runner.state-export` (suite `state-export`; cases `export-accepted`, `export-decode`, `import-accepted-continues`), `harness.state-field-use-audit` (suite `state-audit`; cases `baseline-clean`, `new-indexed-member-unmapped`, `nested-member-unmapped`, `mapping-row-removed`, `unknown-rule-name`, `reader-lacks-member`, `doc-count-drift`), `unit.replay.suite` (suite `replay`; cases `format-roundtrip`, `report-record`, `independent-original-run`, `identity-binding`, `partition`, `at-matters`, `queue-full-divergence`), `runner.replay-mix` (suite `replay`; cases `record-play-match`, `flag-combinations`) | the reference container decoder with literal offsets `fixtures/state/decode_container.py`, the independent re-signing and structural encoder of `core/tests/state_import_helpers.h`, the v25 reference encoder of `core/tests/test_digest_v25.c`, the mutant inventory `docs/spec/state-mutants.txt` authored before the C mutant table, and for replay the independent input script and golden digests `fixtures/replay/mix_oracle.py` and `runner/tests/replay_mix_golden.h` | `unit.core.state.import` (suite `state-import`; cases `binding-refusals`, `container-refusals`, `field-range-refusals`, `invariant-refusals`, `refusal-untouched`, `boundary-perturbation`, `mutant-refusals`, `mutation-sweep`), `runner.state-export` (suite `state-export`; cases `import-refused-integrity`, `import-refused-build`, `import-option-refusals`, `import-bound-below-tick`), `unit.replay.suite` (suite `replay`; case `mutation-matrix`), `runner.replay-mix` (suite `replay`; cases `mismatch-profile`, `mismatch-build`, `mismatch-content`, `play-diverged`): hash-valid hostile blobs, one-field perturbations and header mismatches each reach their own closed refusal or divergence next to an accepted baseline | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1` / `phase09-state-roundtrip`; `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1` / `phase09-replay-mix-goldens`; `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1` / `phase09-replay-format` | Passed in the retained originals of `d3211e9` (normal JUnit and ASan JUnit): `unit.core.state.export`, `unit.core.state.import`, `runner.state-export`, `harness.state-field-use-audit`, `unit.replay.suite`, `runner.replay-mix`. Claim blocks 8, 9 and 10 (block 8 unit-tested, counted 7 of 7; block 9 regression-pinned, counted 1 of 1 (`runner.replay-mix`); block 10 unit-tested, counted 1 of 1 (`unit.replay.suite`)). The full continuation matrix ran in the native step `normal-state-import-full-matrix` (passed, 53.6 s; its step records are unsealed originals). |
| 4. CPU, RSP, RDP, VI, AI, PI, SI and MI activity is ordered by the same guest-time scheduler, including coincident-event and partition/interleave checks; the CPU-to-RSP quantum remains part of profile identity (CORE-02 completion). | CORE-02 | primary: `unit.core.sched.matrix` (suite `sched-matrix`; cases `tracer-coincident`, `cross-device`, `partition`, `interleave`, `resource-limit-resume`, `mi-producers`, `count-wrap`, `quantum-binding`, `state-content-f1`), `unit.core.sched.ledger` (suite `identity`; case `ledger`), `unit.core.sched.characterize-fifo` (suite `identity`; case `characterize-fifo`), `unit.core.profile.quantum-identity` (suite `identity`; case `quantum-identity`); supporting: `repo.sched-k5-anchors` | the literal tick tables and goldens of `core/tests/test_sched_matrix.c` and the closed (device, kind) table of `core/tests/test_sched_ledger.c`, whose derivations from the device sources are recorded in the 09-01 and 09-06 summaries | `repo.sched-k5-anchors` with the out-of-tree mutant driver `scripts/k5-mutants.py` (an equivalence control and a compile control keep each mutant honest) and `unit.core.profile.quantum-identity` (suite `identity`; case `quantum-identity`): a quantum other than 1 is refused and the profile hash changes when only the quantum differs | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1` / `phase09-scheduler-order` | Passed in the retained originals of `d3211e9` (normal JUnit and ASan JUnit): `unit.core.sched.matrix`, `unit.core.sched.ledger`, `unit.core.sched.characterize-fifo`, `unit.core.profile.quantum-identity`, `repo.sched-k5-anchors`. Claim block 7 (unit-tested, counted 4 of 4; `repo.sched-k5-anchors` is a supporting name that no suite of the case inventory lists). |

## Requirement ids

SAVE-01 (criterion 1), SAVE-02 (criterion 2), SAVE-03 (criterion 2), SAVE-04 (criterion 2),
CORE-02 (criterion 4), CORE-04 (criterion 3) and CORE-05 (criterion 3).

## Claim blocks

`docs/claims/phase09.md` holds ten blocks, one per (profile id, fixture set) pair of the column
above, in this order: 1 pad poll (1 of 1), 2 EEPROM 4 Kbit (1 of 1), 3 EEPROM 16 Kbit (1 of 1),
4 SRAM (1 of 1), 5 save round trip (1 of 1), 6 save refusals (2 of 2), 7 scheduler order (4 of 4),
8 state round trip (7 of 7), 9 replay mix goldens (1 of 1, regression-pinned) and 10 replay format
(1 of 1). The profile id of blocks 2 to 5 carries the suffix `-e4k`, `-e16k` or `-s32k`; the
others use the stem id. Blocks 1 to 5 count the accepted authored ROM row that reported `pass`;
blocks 6 to 10 count registered CTest names. The counts are checked against the retained
originals by a scratch reconciliation and by the `claims-final-blocks` case of
`scripts/tests/test_phase09_qualification.py`.

## Limits

These are recorded facts. The independent review, the security audit and the goal-backward verification of
plan 09-10 decided what each one means for the criteria; the claim blocks restate them in their
`not_claimed` lists and prose.

- Criterion 3 says "exactly": the exported canonical state is the v25 digest preimage. Plan 09-09.1
  recorded that the RDP combiner block and the RDP memory-coverage table were not part of it; the fix
  cycle of plan 09-10 carries both in the tail of section 8 (the case `rdp-state-carry`, the row B17 and
  the completeness sweep of `v25-reference` pin it), so a state holding a pending triangle packet is
  claimed to continue equal on the composite family. The depth members stay inactive there (decision K1)
  and the Phase 8 audio profile does not hash these members (its digest is pinned and unchanged).
- A run resumed from an imported state ends in a closed harness-error record and never reports a pass
  verdict (plan 09-09); its guest tick, final state digest and log bytes are compared instead. A state
  published by the runner itself resumes into the closed unknown-log form (both log counts zero, no
  digest; fix cycle of plan 09-10, FX-11) with the uninterrupted run's guest tick and final state digest,
  so the log bytes are compared only for a state exported by the core API at a tick before the log.
- Replay `save_init` in the `image` form is verified in process only; the real runner exercises the
  blank form.
- Every timing, tie, busy window and button mapping in the named tests is a named software-profile
  policy of this project, not a measured fact.
