# Phase 9 authored controller, save media, state and replay qualification

The reviewed source `d3211e957315afbb6bf83c91c49a111d0adbeb58` (the tag `phase09-f2`) passed the complete normal gate, the full
ASan/UBSan inventory and the pinned GCC 13 route natively, the new full continuation matrix step
of the native qualifier, and both existing hosted jobs on the same commit. The exact-binding
verifier (`python3 -B scripts/qualify-phase09.py --verify-evidence`, run without the rehearsal
flag, with the checkout at exactly this commit and a clean tree) then accepted the native record,
both hosted artifact archives and the combined platform pair. The bounded claims are the ten
blocks of [phase09](../claims/phase09.md); the roadmap success criteria map to named tests and
claim blocks in [the success criteria map](phase09-success-criteria.md).

Evidence tier: **unit-tested** for nine blocks and **regression-pinned** for one (block 9, the
replay goldens). Every expected value is an authored literal or comes from an independent oracle
script kept outside the code under test; the machine under test did not produce its own
expectation. This is not hardware-derived, not emulator-differential, and uses no commercial
software, ROM, firmware, save file or oracle. Every tie rule, timing figure, busy ceiling, block
mirror and button mapping is a named software-profile policy of this project, not a measured
hardware fact.

## Native record

The qualifying run is `python3 -B scripts/qualify-phase09.py --native --sha d3211e957315afbb6bf83c91c49a111d0adbeb58`
(no rehearsal flag; the record states `rehearsal: false`, status complete and an empty absent-
pending list, so all bound source files existed).

| Check | Observed result |
| --- | --- |
| Complete normal gate (`scripts/gate.sh`) | 224 CTests passed (JUnit 224 of 224 testcases, 0 failures), inventory and core lint passed |
| Full ASan/UBSan CTest | 226 of 226 testcases passed (the 224 gate names plus the two sanitizer-only names that predate Phase 9) |
| Pinned GCC 13 route | 64 C units (47 core, 17 runner) compiled with `-Werror` |
| Full continuation matrix step `normal-state-import-full-matrix` | passed, `unit.core.state.import` with every row at every partition size (the matrix JUnit is the only proof the full environment took effect; the step records are unsealed originals) |
| Phase 9 case inventory | 13 suites, 40 CTest names listed, 159 cases, snapshot `3f490463f872e1c72d7ac61cbe260be4fbd3cb7f39f3f1d68c1e4d2d5eb49bdf` (equal to the F2 pin) |
| Source inputs bound | 288 files, manifest `bc627e753bdd5fac378abe3206b3da9a7c386be907a5eec5ffd422ec40370dca` |
| Fixture manifest | `sha256:b75864fdf4404a4691bfed12ccaceaca22879e60ea739ef3cf89ca1eee6e80e0` |
| Native record SHA-256 | `4310bc4c0667a1d26c03d3de7e0754225ee4853872b228f76a6484ebad6fa483` |

| Native step | Seconds | Limit |
| --- | ---: | ---: |
| shared native stage | 1,816.5 | 4,800 |
| normal gate (`scripts/gate.sh`) | 219.5 | 600 |
| ASan configure | 0.2 | 120 |
| ASan build | 11.1 | 600 |
| ASan and UBSan full CTest (226 of 226) | 1,476.5 | 3,000 |
| GCC 13 closure (64 units) | 54.0 | 300 |
| `normal-state-import-full-matrix` | 53.6 | 300 |
| 26 inventory checks (13 suites, normal and sanitizer builds) and 2 source-body checks | 0.1 to 0.3 each | 720 and 120 |

Each limit is a kill deadline of the frozen qualifier. The longest sanitizer test was
`unit.core.state.import` at 1,441 s against its registered TIMEOUT of 2,400 s; this run was about 28
percent slower than the rehearsal of the previous plan on the same machine (stage 1,421.6 s), and the
margin is below two times. The original command, exit status, LastTest log, JUnit and the executable
and library bytes of every step are retained privately and bound by hash; none of them is published.

## Hosted record

Run [44795894664](https://github.com/szTheory/tendies64/actions/runs/44795894664), attempt 1, workflow `ci`,
source `d3211e957315afbb6bf83c91c49a111d0adbeb58`. The hosted record (collected by `--collect-hosted`, SHA-256 `a91c5d545a36ef95602cc477615034bbfa059b03f06d1eb8db9d21de26c5c97f`) binds the
native record by its SHA-256 `4310bc4c0667a1d26c03d3de7e0754225ee4853872b228f76a6484ebad6fa483`.

| Job | Result | Duration | Artifact ID | Archive bytes | Archive digest | Phase 9 manifest SHA-256 |
| --- | --- | ---: | --- | ---: | --- | --- |
| `gate-linux` | success | 203 s | 14659700422 | 10883 | `sha256:edaead89654dd5721101c0c62fcd3d37cab0d904e3771320e897e214cd18155b` | `a34e9839463c48cb4b16c4ec1505ae6fd3bf574196aa0eeddf1d8eb7562fdaa9` |
| `gate-macos` | success | 487 s | 14658583889 | 10885 | `sha256:2381952de432c9db7f0659f1684f4f78ddbcd4bccc6a026057969460b2ae0cdc` | `724bde6495d44e93b73122859b94c562791e8d91bad1f255e5f2c11cd02fd16b` |

Both archives were downloaded as original ZIPs by artifact ID and checked against the API digest,
the exact run, attempt, job and commit, and the strict member list before any public projection
was read. The Phase 9 sibling of each archive holds exactly one `manifest.json` (4,922 bytes), the
only Phase 9 file that leaves the machine. The hosted jobs run the five authored pad-poll rows
(`accepted-baseline` passes at guest tick 626,225 with a 68-byte transcript; the shifted script, the
neutral script and the wrong ROM end in a guest failure with their own transcripts; the replacement
ROM is refused with no guest run). Linux and macOS agree on every row, state digest, guest tick and
inventory field. Their build identities differ by platform because the toolchains differ:

| Job | Core build ID | Runner build ID |
| --- | --- | --- |
| `gate-linux` | `t64-build-v1-287b26a37a8b297d2ae779570bf203820c4a852c22915059396b2ec49804db7a` | `t64-runner-v1-1227ea81024f3c6bc8c998184ff2e35ddac5d9419001c3da7d4596434db4560e` |
| `gate-macos` | `t64-build-v1-2032f8e231702e6a74184f62676bf6b9138f9fe9f889f3ddf397623edb4d3691` | `t64-runner-v1-f2e26b52475440c58760c625a07771670d3991ad5bbb22795018e59c07d53255` |

The native normal build used for the claim blocks is
`t64-build-v1-b58d1ab6cd27f97c5ec87d2cb1ae90ce7ae2b877be61830c643342d6bbe2de22` (the claim
`build_id` values carry the prefix `0.0.0+`). The hosted pair does not count any claim block: the
counts come from the retained native originals. The first hosted pair of plan 09-03.1 (run
44632478137, source `2db1be4d`, `docs/evidence/phase09-first-hosted-pair.json`) was a rehearsal; its
state digests are valid for that source only and this pair supersedes it.

## Timing against the 600-second job limit

| | `gate-linux` | `gate-macos` |
| --- | ---: | ---: |
| queue | 1 s | 2 s |
| run (warm) | 203 s | 487 s |
| Gate step | 159 s (ctest 101 s) | 410 s (ctest 251 s) |
| audio qualification records | 30 s | 42 s |
| Phase 9 case run, emit, PII scan | 2, 0, 0 s | 5, 0, 1 s |
| upload | 2 s | 1 s |
| Binutils cache | hit | hit |
| increment over the Phase 8 reference (129 s, 275 s) | +74 s | +212 s |
| rehearsal of plan 09-10 on the previous commit (warm) | 242 s | 433 s |
| warm limit 540 s | pass | pass (53 s of margin) |
| cold projection (warm plus 84 s or 186 s of Binutils build) | 287 s | **673 s** |
| cold limit 600 s | pass | **73 s over** |

The cold figure is a projection and no cold run was spent: both Binutils cache entries were checked
fresh before the push (key `69d0bfd4...`, created 2026-11-19) and both hit. The macOS projection
exceeds the limit again after 619 s at the rehearsal (accepted there as DEV-17); it is recorded as a
deviation of this plan, extending DEV-17, because the pair is the unchanged rehearsed code plus two
hermetic test cases and no logic edit can repair it without a new candidate cycle. The spread of
run times of the same macOS job on near identical code is 349, 420, 433 and 487 s. A cold macOS run
would be cancelled at the 10 minute limit. The measured candidate levers (not tried, all in bound
source) are in `docs/ci-reviews/LOG.md` F-111. The log rows F-111 to F-113 record the timing, the
native cost and the verifier tail of this run.

## F2 identity table

The literals are pinned in `F2_PINS` of `scripts/tests/test_phase09_qualification.py` and compared with
their independent carriers in every gate run by the case `f2-identity-pins` (the runner table, the
manifest rows, the oracle scripts run in the case, the golden and input file bytes and the inventory
bytes), each with a same-setup control. Changing any of them is a declared qualification reset.

| Profile id | Length | Profile hash (SHA-256) |
| --- | ---: | --- |
| `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1` | 38 | `661b161dfee9a10857efe3f98d840c09f05cbf0492dfde59e8ec44190dabb6d5` |
| `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e4k` | 42 | `e8dcb8dc80f2d1efabe83ac0fd8b98c8211622750b70ff142e667b0023f63ce1` |
| `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e16k` | 43 | `22837f619e9537c3c678fbb484747177cbaee93b0bdbfa80d72ac028fda74f3f` |
| `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-s32k` | 43 | `52c47d606e2a0b4dce7a743327082c505ea6f062648cf37349f6e50ca398e87a` |

| Fixture row | ROM SHA-256 | Size | Profile suffix |
| --- | --- | ---: | --- |
| `phase09-admit-e16k` | `f0fbe7d56aa06bbd0bc9eff1961b3985bbcf104264bf7203040ea9194b510588` | 4264 | -e16k |
| `phase09-admit-e4k` | `5015baa4127a56dca9322a453bd2880ead9c3beb08f3f3dd83aada26f8272d18` | 4264 | -e4k |
| `phase09-admit-long-si-v1` | `6e803800afe03de0604ceb541deabee9a1db1a3a7d6ea397d90d8537c3f77e33` | 4280 | (stem) |
| `phase09-admit-s32k` | `73344e3f57c93b715147a0430714c86d46f6f7929c14556f8c0511d7791d2df8` | 4264 | -s32k |
| `phase09-admit-si-v1` | `b3a97e238137feece98b4b39e2f76973a531500c35b39470bfcd6cd23ae67460` | 4264 | (stem) |
| `phase09-admit-wrong-si-v1` | `1220f8a1230ac83b28601028e8b1ecca84bbc89e8d78c3284a8af8fe51f6f29a` | 4264 | (stem) |
| `phase09-eeprom-rw-e16k-v1` | `af3267db49012adeb032d7dd5ca95b4dd8397abb4b8559696a852ab7f2236812` | 5584 | -e16k |
| `phase09-eeprom-rw-e4k-v1` | `0f3cf3c72a0312d90c915f5d148817f9d5f201b96f1575abbc96a81e389be0e9` | 5584 | -e4k |
| `phase09-eeprom-rw-wrong-e4k-v1` | `4cab313efe535b2c690d9b2e786d7ac3a3e6b8b39cb490e46b27e417dc339cec` | 5584 | -e4k |
| `phase09-pad-poll-si-v1` | `29a005be19b2f86e866d00cfb3ab2f8aac39e376d85a5e5d24e78c113a80dab6` | 4688 | (stem) |
| `phase09-pad-poll-wrong-si-v1` | `84bacc101384d52b109a4c41e4b4fac340143a89ad79a4fc4460c5890923a8f6` | 4688 | (stem) |
| `phase09-replay-mix-si-v1` | `79c59149052719baa7080c22d44a21c8d924add1f45b16ced31bb9810bf37df0` | 4616 | (stem) |
| `phase09-save-roundtrip-s32k` | `ec744b26449cafbc73fb5c0e8ebaceb58573cfd224bb8e30adae361927f977eb` | 5000 | -s32k |
| `phase09-sram-rw-s32k-v1` | `9616bd3bfa3d27deed4dcefe206df0b2dc3269424a14bc831210ce0cb8ac07db` | 4808 | -s32k |
| `phase09-sram-rw-wrong-s32k-v1` | `783f46337a32a61a5e1b3683bac30474674dfdf11fd2aebcad166dd2b753ce67` | 4808 | -s32k |

| Transcript pin | Bytes | SHA-256 | Source |
| --- | ---: | --- | --- |
| `admit-0` | 20 | `5fc4d0d36f6eb18aeaa37457e14d3020e14614b97b9e94c6c219f6036624fbc9` | literal text |
| `admit-1` | 20 | `bd084623780adfd35591144d77f299f3107a3399fe536230c6c891ed7de4f015` | literal text |
| `admit-2` | 20 | `c8ba071062fca5ba87baf198de9cf41bf387fa286754844410b644907eaa2bba` | literal text |
| `admit-3` | 20 | `6a0845a8f00f0078e2dc05dddb9daf4ff66430dd2c74010054759107ab25130f` | literal text |
| `pad-baseline` | 68 | `715aa3854b455dc9fd8e038e1aa27e5d200ea311acbfd032930cdc760498d3f6` | fixtures/pad/poll_oracle.py baseline |
| `pad-shifted` | 68 | `4c78899114ba1e1d4111c02a84a059e3943d6c3d00208c533bbc3e7faeb07910` | fixtures/pad/poll_oracle.py shifted |
| `pad-neutral` | 68 | `caf55c819271e372f4e4185520a49ed74c36d79c870bfa30d1dbc064c9f9f137` | fixtures/pad/poll_oracle.py neutral |
| `pad-wrong-rom` | 68 | `07de0176dbe67a8c313b1871e019b588d00793e8a4002fae4ce3f6cecf333315` | fixtures/pad/poll_oracle.py wrong-rom |
| `save-eeprom-4k` | 191 | `03891120e98176bc11f8d9f6cde6278ff33ef0ebe9018d2f24ce083b3d2c6502` | fixtures/save/save_oracle.py eeprom-4k |
| `save-eeprom-16k` | 192 | `d9a5bf956cce18ea6bc3fd563ae060d7dda3f8554a881b62214ccf89a7f4ca95` | fixtures/save/save_oracle.py eeprom-16k |
| `save-eeprom-wrong-4k` | 191 | `87609f3fa10ce472755b3b501cfb80a457c16be2c20e536b190b8e2002598a7c` | fixtures/save/save_oracle.py eeprom-wrong-4k |
| `save-sram-32k` | 101 | `52b0f1caee3ec470f4217236ce6f3da94e6ff1c15fd93143a68f7364b732a026` | fixtures/save/save_oracle.py sram-32k |
| `save-sram-wrong-32k` | 101 | `29174dfa593b7e9b6cc9e979ff7312e1fc39b5fc41ef9b280e8833dfe049f06c` | fixtures/save/save_oracle.py sram-wrong-32k |
| `roundtrip-baseline` | 144 | `649b62940ace25d9e4c31e12381f38df779a5bbef90dcf0baf1be1458e6b75c4` | fixtures/save/roundtrip_oracle.py baseline |
| `roundtrip-swap16-as-raw` | 144 | `3717361f6b106331021025e25a738598b01a52bffb68bd33507b0ec948369128` | fixtures/save/roundtrip_oracle.py swap16-as-raw |
| `replay-mix` | 72 | `b16f6575b5f94bee4018f7ef12ad0ca0e1c8134a033479b78e7d8527d20eabe2` | fixtures/replay/mix_oracle.py |

| Input pin | SHA-256 |
| --- | --- |
| `pad-baseline` | `e70838a961dfa08c13d9baf4cbf48fb8e3d14029c348a6e0181771b93ca8d1ba` |
| `pad-shifted` | `4c830af94b3e98080f2e1b5da75581599a2523fe2df60d45b06beabc0f35c979` |
| `pad-neutral` | `6d5593bc2d95465b44ec7ce7785e5ff7d7d53605de9b039a021bc697d226186d` |
| `pad-wrong-rom` | `330be83d0a0bf653375cdb71a5332ac82a6bc2f077e3c63a38b634d91b0a56f2` |
| `replay-mix` | `e61dcd21ca31d936142622e6ece22f6689f2eed816f2fb4bd50d5efca6a4acbf` |

| Other pin | SHA-256 |
| --- | --- |
| replay golden header `runner/tests/replay_mix_golden.h` | `3004debb649da008a662f84f5657a42962c6a020e19ab12098d0529e3443d490` |
| oracle script `fixtures/pad/poll_oracle.py` | `a5e7c143d9a093572037f8acbb29d7c66f844baee6ee94a39c72c442c4c8cfe2` |
| oracle script `fixtures/save/save_oracle.py` | `a004436952b41eed65d565eb52085548103a353aedc0159b421f70b11cd34600` |
| oracle script `fixtures/save/roundtrip_oracle.py` | `e12ce2f5c8171c0aa3aa4dda6193499a30e4d29bdfb1f7827d8fb7d8a76fcc51` |
| oracle script `fixtures/replay/mix_oracle.py` | `70188af9c254722517ec5537e5071641c82220f9ed452ca5a32332f9fb77ce97` |
| oracle script `fixtures/state/decode_container.py` | `e02a755112851f0ef137573d50562c0e17a0dd538c1335b7bb5bc8b1376011c8` |
| case inventory `docs/spec/phase09-cases.json` | `3f490463f872e1c72d7ac61cbe260be4fbd3cb7f39f3f1d68c1e4d2d5eb49bdf` |
| fixture manifest `fixtures/manifest.json` (computed, bound by each claim block) | `b75864fdf4404a4691bfed12ccaceaca22879e60ea739ef3cf89ca1eee6e80e0` |

## Claims index

| Block | Profile id | Fixture set | Tier | Counting unit | Counted |
| ---: | --- | --- | --- | --- | --- |
| 1 | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1` | `phase09-pad-poll` | unit-tested | accepted ROM row | 1 of 1: `phase09-pad-poll-si-v1` |
| 2 | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e4k` | `phase09-eeprom-rw` | unit-tested | accepted ROM row | 1 of 1: `phase09-eeprom-rw-e4k-v1` |
| 3 | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e16k` | `phase09-eeprom-rw` | unit-tested | accepted ROM row | 1 of 1: `phase09-eeprom-rw-e16k-v1` |
| 4 | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-s32k` | `phase09-sram-rw` | unit-tested | accepted ROM row | 1 of 1: `phase09-sram-rw-s32k-v1` |
| 5 | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-s32k` | `phase09-save-roundtrip` | unit-tested | accepted ROM row | 1 of 1: `phase09-save-roundtrip-s32k` |
| 6 | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1` | `phase09-save-refusals` | unit-tested | registered CTest names | 2 of 2: `unit.core.save.device`, `unit.core.save.import-export` |
| 7 | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1` | `phase09-scheduler-order` | unit-tested | registered CTest names | 4 of 4: `unit.core.sched.matrix`, `unit.core.sched.ledger`, `unit.core.sched.characterize-fifo`, `unit.core.profile.quantum-identity` |
| 8 | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1` | `phase09-state-roundtrip` | unit-tested | registered CTest names | 7 of 7: `unit.core.digest.sink-legacy-identity`, `unit.core.digest.v25-reference`, `unit.core.digest.v25-strip-differential`, `unit.core.state.export`, `unit.core.state.import`, `runner.state-export`, `harness.state-field-use-audit` |
| 9 | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1` | `phase09-replay-mix-goldens` | regression-pinned | registered CTest names | 1 of 1: `runner.replay-mix` |
| 10 | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1` | `phase09-replay-format` | unit-tested | registered CTest names | 1 of 1: `unit.replay.suite` |

Blocks 1 to 5 count the accepted authored ROM row that reported `pass` in the real supervised runner
(the case marker and completion tick are in the retained normal LastTest log: pad-poll 626,225;
EEPROM 4 Kbit 1,567,331; EEPROM 16 Kbit 1,567,338; SRAM 539,986; save round trip 540,634). Blocks 6 to 10
count registered CTest names that a suite of the case inventory lists and that are passed testcases in
the retained normal JUnit; every counted name also passed in the retained ASan JUnit. The
wrong-value controls (`controls.real.pad-wrong-value`, `controls.real.save-wrong-byte` and the
wrong-format case of `runner.save-roundtrip`) are named in the claim prose and not counted.

## Candidate, attempt and deviation history

One candidate commit was needed in this plan: the qualifying native run and the hosted run both passed
on `d3211e957315afbb6bf83c91c49a111d0adbeb58` on the first attempt. No failed attempt belongs to this plan. The earlier runs of the phase,
none of which is qualification evidence:

| Run or record | Source | Result | Class |
| --- | --- | --- | --- |
| 44623897192, 44630469793, 44631218639 (plan 09-03.1) | `1b0a1ae`, `41e5d8a`, `e804998` | hosted Linux build failures from three earlier-plan defects, each fixed and replayed locally before the next push | blocking repairs of earlier code |
| 44632478137 (plan 09-03.1) | `2db1be4` | first hosted pair, both jobs success; rehearsal, superseded by this pair | rehearsal |
| 44776046324 (plan 09-10) | `49069a7` | both jobs failed at the Phase 9 emit: a test cleanup scope removed retained evidence (FX-38) | defect found and fixed |
| 44784312630 (plan 09-10) | `16849e7` | gate-macos failed at Gate on a registered 60 s TIMEOUT (FX-39) | defect found and fixed |
| 44790405712 (plan 09-10) | `8dd6af1` (tag `phase09-fr`) | rehearsal pair, both jobs success, rehearsal collector and verifier pass; macOS cold projection 619 s accepted as DEV-17 | rehearsal |
| native rehearsals (plan 09-10) | `16849e7`, `8dd6af1` | exit 0 in 1,652 s and 1,486 s; one earlier start on `0d31e0e` was stopped after about 6 minutes | rehearsal |

Frozen-file declarations carried from plan 09-10 (logic changes to files frozen at `phase09-fh`, each
covered by the rehearsals and now by this qualification): FD-01 `scripts/tests/test_rsp_no_hle.py`, FD-02
the raster function inventory literal of `scripts/check-rsp-no-hle.py`, FD-03
`runner/t64_artifact_publisher.c` and `.h`, FD-04 `_check_source_route` of the same guard, FD-05
`scripts/qualify-phase09.py` (limits as constants and the full-matrix step), FD-06
`scripts/qualify-rdp-vi.py` (five stage limits). Deviations DEV-01 to DEV-17 are in
`.planning/phases/09-controller-save-media-and-replay/09-REVIEW-FIX.md`.

Deviations of this plan:

1. The macOS cold projection of 673 s exceeds the 600 s limit by 73 s (see Timing); recorded as an
   extension of DEV-17, not repaired, reported to the orchestrator.
2. One planned test-only edit of the bound test file, declared before the qualifying run: the check
   `claim-skeleton-has-no-real-block` is replaced by `claims-final-blocks`, and `f2-identity-pins` is added
   (suite `f2`, no new CTest name; they run inside `harness.phase09-qualification`).
3. The public projection directory of the collector is a fresh directory outside the private store
   (`build/gate/09-11-public-d3211e957315`), because the collector refuses a public output path under the
   private root; the same correction was made in plans 09-03.1 and 09-10.

Freeze check from the rehearsed commit `8dd6af1` (tag `phase09-fr`) to the qualified commit: the trees
`core`, `runner`, `fixtures`, `tools`, `tools/rights` and `.github` are identical to the FR record; the
`scripts` tree differs by exactly `scripts/tests/test_phase09_qualification.py`; the frozen Python files are
identical by `ast.dump` of every function, class and import and the frozen shell and workflow files
are byte-identical, checked with a scratch comparison that rejects a mutated copy of the qualifier and of the
gate script after an accepted baseline in the same setup.

## Review and security

This plan schedules no second review. The independent review (`09-REVIEW.md`: five lanes, 52 findings),
the security audit (`09-SECURITY.md`: secured at ASVS level 1, 86 of 86 threats closed) and the
goal-backward verification (`09-VERIFICATION.md`: passed 13 of 13) ran in plan 09-10 before the F2
freeze; its one fix cycle is `09-REVIEW-FIX.md` (FX-01 to FX-39 and the FR freeze record) and the plan
summary is `09-10-SUMMARY.md`, all under `.planning/phases/09-controller-save-media-and-replay/`. The
verification digest recorded there predates the later edits and is recomputed by the confirmatory
verification that follows this plan. The threats T-09-83 to T-09-86 of this plan (freeze, disclosure,
claim wording and hosted scope) are mitigated by the freeze check, the scans before every commit, the
claim checker and the final-mode verifier described here.

## Family ids exercised and not claimed

The claim blocks of the suite-level tests bind the stem id only. The suites below also exercise other
ids, which are listed here and are not claimed: the three save family ids (`-e4k`, `-e16k`, `-s32k`)
in `unit.core.save.device`, `unit.core.save.import-export` (refusal requests across the four family ids),
`unit.core.sched.matrix` (the three save ids equal themselves across whole, 7-tick and 4,096-tick runs),
`unit.core.state.export` (the stem carries state ids 1 to 10 and 16, the save ids 1 to 11 and 16),
`unit.core.state.import` (77 refusal table rows over five families and the `save-media-roundtrip` case)
and `runner.state-export`; the legacy profile ids of the 17 pre-Phase-9 idle digests and the Phase 8
profile in `unit.core.digest.sink-legacy-identity` and the state tests (unchanged by Phase 9; the Phase 8
profile keeps its recorded limitation for the RDP continuation members).

## Limitations and preserved stops

- Authored scope only: one authored ROM per claimed row, authored literals and independent oracle
  scripts. No hardware measurement, other-emulator comparison or commercial title was used.
- The state, digest, container and replay formats and the runner flags and records are provisional
  until the Phase 10 freeze; no version marker moved in Phase 9, and no application or binary interface
  is frozen. A claim holds for the exact build and source commit named here.
- The boundary matrix has 17 rows (B01 to B17); four run every partition size, thirteen are sampled in
  the gate, hosted and sanitizer runs, and the full set runs only in the native step
  `normal-state-import-full-matrix`, whose step records are unsealed originals.
- A resumed runner run never reports `pass`; only the resume mechanism and the digest equality are
  claimed. A failed publish publishes nothing, but a supervisor failure after the runner published can
  leave a leaf beside a failure record, so a caller accepts a leaf only with exit 0 and a `pass`
  record. A same-user swap or a kill between the publisher's rename and its withdrawal can leave a leaf.
- The seeded triangle campaign at the gate default reaches refusal reason 40 only; the field readers are
  reached by the registered sanitizer counts and the named mutants. Raw check totals are not strength
  evidence.
- The supervisor size limits (`RLIMIT_FSIZE`, `RLIMIT_CORE`) are applied in the child with discarded
  results; the exact-backstop statement does not hold on a host whose inherited hard limit is lower.
- A new Phase 8 native qualification on this tree would be refused closed by its own verifier (F-106);
  no Phase 8 requalification is claimed. A manifest of a rehearsal commit is distinguished from a
  qualifying one by `workflow_sha` only.
- Hosted time: warm 487 s on macOS is inside 540 s but the cold projection 673 s is over 600 s (recorded
  deviation, see Timing). Native qualification costs about 30 minutes, 80 percent of it the sanitizer
  run of `unit.core.state.import`.
- Raw state, save, replay and capture files stay private and are never uploaded. The only hosted upload
  from Phase 9 is the one public `manifest.json` per job; this document holds digests, sizes, counts and
  public run links only.
- Phase 7 VID-04 and Plan 13 remain blocked on oracle authority and the parallel-rdp rights audit. No
  oracle was executed, copied or used, no ROM, firmware, PIF or IPL image, 64DD media or save file was
  used, and no release, naming, channel, hardware or paid-service decision was made.

## Reproduction

The private records are untracked build outputs kept on the machine that ran the qualification (their
digests are above); the check below needs them and the public repository checkout.

```sh
git switch --detach phase09-f2
python3 -B scripts/qualify-phase09.py --verify-evidence --repo szTheory/tendies64 --sha d3211e957315afbb6bf83c91c49a111d0adbeb58 \
  --run 44795894664 --attempt 1 \
  --native-record build/gate/09-11-phase09-private/native-d3211e957315afbb6bf83c91c49a111d0adbeb58/native-record.json \
  --hosted-record build/gate/09-11-phase09-private/hosted-d3211e957315-44795894664-1-1795202282515508000/hosted-record.json
git switch main
```

It must print `qualify-phase09: complete native, ASan, GCC13, and both hosted platform records verified`
and no rehearsal statement. The claim file check is `sh scripts/check-claims.sh --files docs/claims/phase09.md`
(`claims: files=1 blocks=10 errors=0`). The commits after the tag change documents only:
`git diff --name-only phase09-f2..HEAD` lists this file, the success criteria map, the review log and
planning records.
