# Phase 12 minimal host, playable homebrew and final bind qualification

The audited source is the tree of `656dc00433b24629f88b01795e17ef55225adf64` (the Qualification base
recorded in the Phase 12 pin-impact record; it is the head that the final independent audit read in a
scratch clone, `18636c8aa35870628e5f99d50e8ab350d623c680`, plus the commit of the audit record, a planning
file). The one final native qualification (`python3 -B scripts/qualify-phase12.py --native`, no
rehearsal flag) passed on the head `40b7d2d9997452a32619b677eeed84b4d68ea660` (named H below: the base
plus the commit of the `Qualification base` line, which changes only the pin-impact record) on its first
attempt, and the exact-binding verifier (`python3 -B scripts/qualify-phase12.py --verify RECORD`) accepted
the record against that tree, again after the record's planning commit, and again after this document,
the claim file and the owner checklist template were committed: the qualifier excludes these three
documents from its bound inputs, as it excludes `.planning/` and `docs/ci-reviews/LOG.md`, so writing
them after the record does not invalidate it. The commit of the three documents is the head pushed to
`main` by plan 12-18; its warm hosted pair is recorded below, in `docs/ci-reviews/LOG.md` and in the
pin-impact Gate-growth ledger. The bounded claims are the three blocks of
[phase12](../claims/phase12.md).

Evidence tier: **unit-tested, authored scope** (every block of the claim file is tier `unit-tested`),
and the ABI block is a **software-interface claim only**. The blocks say what an authored title built
in this repository did through the supervised runner, through the installed package and through the
host pump with an injected fake clock, and that the installed tree equals the ABI record bound at 1.0.
Every expected value comes from the title's behavior authority and its standard-library oracle
(committed before the guest source), from hand-written expectations of the host and consumer cases, or
from the bound record and its four goldens; the machine under test did not produce its own
expectation, and the recorded pins are labelled as such. This is not hardware-derived, not
emulator-differential, and uses no commercial software, ROM, firmware, save file or oracle material. No
claim is made here about hardware behavior, about agreement with any console, title or other emulator,
about real-time play, throughput, latency or A/V sync, about SDL rendering correctness, about play with a
physical controller, about LibFuturo support, or about a release: nothing was released or published as
a package, no package channel exists, and no tag was created (the commits are named by their 40-hex
ids).

## Source and build identity

| Item | Value |
| --- | --- |
| audited source (Qualification base) | `656dc00433b24629f88b01795e17ef55225adf64` |
| qualified commit **H** (`commit_sha` of the record) | `40b7d2d9997452a32619b677eeed84b4d68ea660`; the one path between the base and H is the pin-impact record |
| reviewed head of the final audit | `18636c8aa35870628e5f99d50e8ab350d623c680` (`.planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-REVIEW.md` "FINAL"); the base adds only that record |
| wave bases bound by the record | Wave 0 `60b9e3c34fc0efc0ff64296cfa31415565b847b2`, Host base `0836e40f583f8b47486ee93b0de36889e5626f0b`, Bind base `49d41ac9e1966bd9cdb9e953b61d0355a1178554`, Qualification base `656dc00433b24629f88b01795e17ef55225adf64` |
| bind commit | `03ccb01c28179522d357d866c1e21aebfade7b0a` ("final ABI bind at 1.0", the one coupled commit of plan 12-14, proven by a full gate in a scratch clone first) |
| core build id, equal for the gate archive, the installed archive (built by the fresh-archive lane from `git archive` of H outside the repository), the installed consumer executable and the host test binary (the claim `build_id` values carry the prefix `0.0.0+`, the untouched `project(VERSION)`) | `t64-build-v1-06a20d33f9a16380b4f97721549b37dcf8ad39421459f7e702821e74dd897203` |
| runner build id (the in-tree supervised runner of `runner.title-two-run`) | `t64-runner-v1-bab106e9f6484a894809133c67c4385472b2462ef646ad44195e3162a42660b6` |
| runner test twin build id | `t64-runner-v1-eb6db04c066f0727b85ba6507db6190ed10739965f3575012e06e829b9089d70` |
| ASan and UBSan core build id (in-tree sanitizer tree) | `t64-build-v1-44dbfa2f8db9929ebfb57f8c85bca599dd56f98b24fd15c3917b11c28ffc5a54` |
| ABI and package | ABI **1.0** (`T64_ABI_VERSION_MAJOR` 1, `T64_ABI_VERSION_MINOR` 0); package version **1.0.0** with `SameMajorVersion`. ABI 1.0 and package 1.0.0 are the ABI identity and the ABI number, not a release version: `project(VERSION)` stays 0.0.0, and tags, a release channel and release naming stay unselected (D6). The owner confirmed the 1.0 numbers on 2026-11-26 as an ABI identity constant; the record lists them as its "Owner-review item (D6)" |
| bound record `docs/abi/FREEZE-CANDIDATE.md` | SHA-256 `856290c58397444abdec149cc842eb7a53b2bd15ea2028418ee2033ed775e162`; block `abi_version: 1.0`, `package_version: 1.0.0`, `amendments: 0`, `bound_minor: 0`, `candidate_amendments: 0`; 35 public functions, 39 statuses, 8 stop reasons, 14 structs, 165 members, 190 constants, 8 padding holes |
| golden `header_sha256` (`docs/abi/header.sha256`) | `b4f5705c87e25f94ac56091f305cd2364acd432210914fc1093cb8e94f992dac` (one line: `include/tendies64/t64_machine.h` `4524c0c31f37f2512b15f4bd1f7ed223c1dc4120a3a234b779a0d62d6f8020c3`) |
| golden `public_symbols_sha256` (`docs/abi/public-symbols.txt`) | `c45ff33c495f8c0935dd2b0362b528c631037483fb6affb42b7d2d908a252d39` |
| golden `layout_sha256` (`docs/abi/layout.c`) | `7897b5f8b19b74cf1f141ce2cfebbb75abd0c9bb058d3c5be38b78c17dbd7520` |
| golden `status_classes_sha256` (`docs/abi/status-classes.txt`) | `570548fc95fb3997a0f750bbe8cfdf443d6eadf4c0e5a38e5fe5d12ee50e6e5e` |
| fixture manifest `fixtures/manifest.json` (bound by each claim block; rows `phase12-title-s32k` and `phase12-title-refused-s32k` added in Phase 12: authored-here, cleared, build-only, toolchain GNU Binutils 2.45) | `sha256:247205fa800c05764ee58008a89b48a5989acfde4b05c49cb1cf2fe7f04c7297` |
| title source (frozen) | `fixtures/title/title.S` and `title.ld` at revision `7b962961fcb8e1c1600dc84dae8a67f0c88dce26` (522 non-empty lines); builder `scripts/build-title-fixture.sh` |
| title behavior authority, oracle and expectation | `fixtures/title/title-spec.md` `11c8bb00569fb7da567e88546a960bcf7829cbfb23eac4a0ce285dff4611753f`, `fixtures/title/title_oracle.py` `e0f262685b063e8ca329c398507426ad847a3ab1d7e2d73eecee4e3616245d16`, `fixtures/title/title.expected.json` `26ec0fb8702b2fa9cad0b3671361274e52a18beb7ef43a671d4f8b76ab347de8` |
| recorded pins `fixtures/title/title.runner-pins.json` | `286f43ca55c71971d59f0d1e7645d9f6441ba9e69a3e40712bfc892672b49fc5`: top level "runner-recorded pin, not independent" (short scenario, completion tick 3693938, `GUEST_COMPLETION`), nested `consumer` "consumer-recorded pin, not independent" (long scenario, completion tick 13078313, `GUEST_COMPLETION`) |
| profile id of every title case | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k` |

Recorded pins. The completion tick, the stop reason, the final state digest and the video packet count
of a run have no independent source: the short scenario's values are runner-recorded pins (label
"runner-recorded pin, not independent", the supervised raw-capture run of plan 12-04) and the long
scenario's values are consumer-recorded pins (label "consumer-recorded pin, not independent", the
installed consumer of plan 12-05). The runner, the consumer and the pump comparator print the label
next to each of them, and no claim calls them independent. The frames, the audio packets and stream,
the transcript and the save image are compared with the oracle values, which never read a ROM, the core,
the runner or a build output.

## Native record

The record is private build output kept on the machine that ran it
(`build/gate/12-qualification-private/20261127T033336673294Z-40b7d2d9/record.json`, SHA-256
`24d5db59e6f2bd3fd9f2a8a3a5090123d776e761298a1fc458737fb1d7c04a3b`); this document holds digests,
counts and seconds only. The record id is `20261127T033336673294Z-40b7d2d9`. It states `rehearsal: false`,
`status: complete`, a clean tree, an empty `qualification_drift_paths`, `tier: unit-tested, authored
scope`, `sdl_lane_status: pass` and `sc2_virtual_joystick: closed`, and the claims `hardware_accuracy`,
`oracle_comparison`, `release`, `real_time` and `libfuturo_adapter` the JSON value `false` and `final_abi`
`true`, which means only that the record is bound at ABI 1.0 as `docs/abi/FREEZE-CANDIDATE.md` states (an
ABI identity, not a release). It binds 87 source inputs (every file changed since the Wave 0 base outside
the exclusions plus the stage tools; manifest SHA-256
`0a88d0e83627ae55e90223bb512464b5fd8e7c075b4bc73bf869bc6c948585dc`), the 25 `PHASE12_TESTS` names (names
SHA-256 `8fc10a2181051898f1cc219a5b7c021c5666b6b3407d791a978927fa9c9ec273`, equal to the 25 names of
`ctest -N -L phase12 -FA .`), the four bases, the build ids above and the bound record with the SHA-256 of
each of its four goldens, which equal its machine-read block. Total 1304.01 s on the qualifier clock
(03:33:36Z to 03:55:20Z on 2026-11-27); host 1-minute load 4.99 at the start, after a bounded wait for it
to fall below 9, and 6.83 at the end, with samples from 7 to 41 during the gate and sanitizer stages from
unrelated processes, so the seconds are upper bounds. The record passed on its first attempt: there is no
failed attempt of this qualification, and no limit, backstop or source changed. A rehearsal of the same
table (record `20261127T020322125432Z-9f4e39a3`, plan 12-17, 1301.36 s at `9f4e39a`) is labelled as one
and refused as final evidence by the verifier.

| Stage | Seconds | Limit | Observed |
| --- | ---: | ---: | --- |
| ci-scanner | 2.56 | 90 | `check-ci-yml.sh` ok, selftest 138 cases |
| inventory | 0.76 | 60 | exact test inventory of the gate preset passes, count 354 |
| oracle | 0.20 | 60 | `--expect-blocked` exit 0 (blocked: no oracle operation is authorized); `--require-approved` exit 2 |
| abi-cheap | 8.97 | 720 | incremental gate build, then the 16 cheap ABI, install and API-04 names (`100% tests passed out of 16`) |
| phase12-suite | 14.45 | 180 | `ctest -L phase12` with JUnit: 27 testcases (the 25 names of the block and the fixture setups `install.setup` and `installed.consumer.build`), 0 failed, 0 skipped (JUnit SHA-256 `f1d8c0430667a61aa1949046935ccde7033e8f0a09b083be54e99e8dcff56ab5`, 39.8 s summed) |
| host | 5.81 | 180 | `check-host.sh build` against the moved prefix, then the 8 `host.pump` names with `install.setup` (`100% tests passed out of 9`); the host test binary carries the core build id |
| sdl-lane | 1.92 | 180 | status **pass**: `[host-sdl] sdl 3.4.10`, `video-driver dummy`, `audio-driver dummy`, `mapping rows 30/30`, `title ok`, `blit ok`, `frames 8`, `audio ok`, `[host-sdl] PASS`; `sc2_virtual_joystick` closed |
| gate (`scripts/gate.sh`) | 307.28 | 600 | PASS, total 307 s; `ctest --preset gate` 354 of 354 passed, wall 155.44 s |
| sanitize-shipped | 150.85 | 300 | the 27 consumer cases under AddressSanitizer and UndefinedBehaviorSanitizer against the shipped (uninstrumented) installed library, 0 sanitizer references in the archive and 34 in the consumer; the call-sequence generator at 20,000 steps and digest stride 1: 21,186 calls (2,478 ok, 14,518 refused, 1,561 guest-stop, 1 defect, 2,628 calls on the poisoned instance), 1,275 refusal bursts, 1,285 machine digests verified, 0 refusals without a digest check, `checks=98461 failures=0`; no sanitizer report |
| sanitize-instrumented | 682.32 | 1,200 | the same 27 cases against an instrumented library (733 references in the archive, 36 in the consumer), sequence 20,000 steps at stride 8, `checks=97335 failures=0`; no sanitizer report |
| tsan | 9.33 | 120 | two threads, one instance each, both at the digest `97f27027...1267c`, `checks=18 failures=0` (564 and 21 ThreadSanitizer references); no report |
| fresh-archive | 4.09 | 120 | `git archive` of H (1,524 files) built outside the repository as the gate preset configures it, six files installed into a prefix with a space, consumer smoke (`checks=13 failures=0`) and composite (`checks=25 failures=0`) pass, the consumer requests `tendies64` 1.0, installed build id equal to the gate's |
| sweep | 61.19 | 300 | `sweep: 1599 shared lines, 0 differences, 0 head-only lines` against the Wave 0 base |
| freeze | 0.22 | 60 | `check-abi.py freeze` against the fresh-archive prefix: `abi=1.0 package=1.0.0 symbols=35 layout=14/165/190 holes=8 statuses=39 stop_reasons=8`, 0 findings |
| mutants | 53.24 | 240 | `p12-mutants.py --run`: 12 of 12 mutants killed by their named tests, the equivalent control survived and the non-compiling control was invalid (table below) |

Each limit is a kill deadline of the qualifier, derived from measured seconds in its own source with a
ratio of at least two. The tightest ratios of limit to observed seconds are 1.76 (the instrumented
sanitizer lane), 1.95 (the gate) and 1.99 (the shipped sanitizer lane), all taken at the unrelated load
named above; the rehearsal at similar load gave 1.72, 2.03 and 2.58. No limit was hit and none was
widened; the constants are derivation text and were not refreshed, because an edit of the qualifier after
the Qualification base would be a bound-input change and none of them is a defect.

## Local lanes

The SDL lane, the sanitizer, TSan and fresh-archive runs and the full mutant run are local lanes, run
only by the qualifier and by hand; none is a gate name and none is in `ci.yml`. Their results in the final
record are the `sdl-lane` (1.92 s), `sanitize-shipped` (150.85 s), `sanitize-instrumented` (682.32 s),
`tsan` (9.33 s), `fresh-archive` (4.09 s) and `mutants` (53.24 s) rows above, all on H and dated
2026-11-27 by the record.

- SDL lane (`sh scripts/check-host.sh sdl`): status **pass**, SDL 3.4.10 (the system package of the
  planning host, macOS 26.6.2 arm64, link-only; the zlib license and the audit are in
  `docs/evidence/sdl3-host-dependency-audit.md`), dummy video and audio drivers, a virtual gamepad through
  the 30 mapping rows, 8 title frames blitted at the integer scale and checked at sampled pixel blocks, and
  an audio stream with no dropped packet. The final independent audit printed the same lane line
  `[host-sdl] PASS` in its scratch clone. This closes the SC2 virtual-joystick clause on this host as
  self-checking host glue: no window is shown and no sound is played under the dummy drivers, so it binds
  no rendering, timing or audio statement. A host without SDL3 would record `skipped` and the clause
  "not closed"; that did not happen here. Hosted Linux has no libsdl3 package on ubuntu-24.04, so the lane
  is local only.
- ThreadSanitizer: the evidence of the threading statement (distinct instances are independent; one
  instance needs external serialization) for two instances on two threads only; not counted in a block.
- Fresh archive: the exported tree of H, not the work tree, builds the library and the consumer; this is
  also the prefix of the `freeze` stage.
- Mutants (`python3 -B scripts/p12-mutants.py --run`, a throwaway worktree under `build/12-mutants`, host
  build 0.7 s, title ROM build 0.6 s equal to its manifest row, baseline 10.5 s over 7 checks). A kill
  needs a failed check line of `host/tests/t64h_tests.c` or a comparator MISMATCH line; a crash or a
  timeout is never a kill.

| ID | Mutant | Expected | Result | Killing test | Seconds |
| --- | --- | --- | --- | --- | ---: |
| P12-P01 | tick divisor 16 written as 15 | KILLED | KILLED | `host.pump.clock` | 1.1 |
| P12-P02 | input hook once at the end of a step | KILLED | KILLED | `host.pump.title-two-speeds` | 3.4 |
| P12-P03 | ISViewer text drain skipped | KILLED | KILLED | `host.pump.drain-order` | 1.2 |
| P12-P04 | refused video packet dropped, not held | KILLED | KILLED | `host.pump.stall` | 2.7 |
| P12-P05 | frame rows read at the row length, not `stride_bytes` | KILLED | KILLED | `host.pump.frames` | 0.8 |
| P12-P06 | dead zone one unit too wide | KILLED | KILLED | `host.pump.input-map` | 0.8 |
| P12-P07 | vertical axis by negation, not complement | KILLED | KILLED | `host.pump.input-map` | 0.8 |
| P12-P08 | guest completion treated as resumable | KILLED | KILLED | `host.pump.stall` | 2.3 |
| P12-T01 | title D-pad right and left bits swapped | KILLED | KILLED | `host.pump.title-two-speeds` | 5.5 |
| P12-T02 | title horizontal step 3 pixels | KILLED | KILLED | `host.pump.title-two-speeds` | 5.5 |
| P12-T03 | one title tone sample 0x2001 | KILLED | KILLED | `host.pump.title-two-speeds` | 5.5 |
| P12-T04 | title press count at slot offset 8 | KILLED | KILLED | `host.pump.title-two-speeds` | 5.5 |
| P12-C-EQUIV | control: division by 16 as a shift by 4 | SURVIVED | SURVIVED | - | 5.0 |
| P12-C-INVALID | control: a non-compiling pump source | INVALID | INVALID | - | 0.8 |

## Final independent audit

Before the Qualification base, the whole phase was audited in a scratch clone of the head
`18636c8aa35870628e5f99d50e8ab350d623c680` (`.planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-REVIEW.md`,
section "FINAL"): the full gate 354 of 354, the sweep against the Wave 0 base with 0 differences, the
bound record recomputed from a fresh install with 0 findings, the oracle authority still blocked, the
claims scan with 0 errors, the SDL lane line `[host-sdl] PASS` (SDL 3.4.10), no tag or release made in the
phase, no workstation path or personal data in the 87 commits of the phase, and no edit of `ci.yml`,
`check-ci-yml.sh`, `gate.sh` or the root `CMakeLists.txt` since the Wave 0 base. Every requirement
(API-05, API-06, API-07), every success criterion (SC2 closed by that lane line), every CONTEXT decision
and every deferred idea (each confirmed absent) has a row with its evidence and verdict. Result: zero
blocking findings, no warning, five info rows (FR-01 to FR-05, accepted). The earlier wave reviews are the
TITLE and HOST sections of the same record; their two warnings were resolved before the bind (HR-04 by
the measured cold run F-139, HR-06 by the H-03 sentence of plan 12-15).

## Hosted records

Every hosted run of Phase 12 is a `ci.yml` run on `main` with the two jobs `gate-macos` and `gate-linux`;
the rows of `docs/ci-reviews/LOG.md` (Runs table and findings F-137 to F-140) hold the measured seconds.
Seconds are job run times; "warm" means the Binutils cache was hit on both jobs.

| Run | Plan | Head | Event | Result | `gate-macos` s | `gate-linux` s | names | LOG finding |
| --- | --- | --- | --- | --- | ---: | ---: | ---: | --- |
| 45887012740 | 12-06 (TITLE review) | `884153ac143e` | push | success, warm | 582 | 263 | 336 | F-137 |
| 45904452436 | 12-12 (HOST review) | `de21fcf7d5c3` | push | success, warm | 572 | 306 | 351 | F-138 |
| 45906047013 | cold dispatch (orchestrator) | `de21fcf7d5c3` | workflow_dispatch | success, **cold** (macOS cache entry deleted first) | **547** | 280 | 351 | F-139 |
| 45932117914 | 12-17 (bind and qualifier head) | `9f4e39a3caa6` | push | success, warm | 527 | 277 | 354 | F-140 |
The hosted pair of the qualified head is the push of plan 12-18 (a routine fast-forward of the pinned
SHA, no tag): the first hosted run of the Qualification base and the three documents, and a second hosted
Linux compile and run of the 12-16 and 12-17 changes. Its run id, its seconds and the final warm
Gate-growth ledger row are written into this table, `docs/ci-reviews/LOG.md` and PIN-IMPACT by Task 4 of
plan 12-18, after the run, as a change of the excluded documents.

### Gate growth against the 900-second limit

The measured cold `gate-macos` runs bracket the phase: **729 s** of the 900 s job limit at the Phase 11
close (run 45795103375 at 329 names, F-136) and **547 s** at the HOST head (run 45906047013 at 351 names,
F-139, margin 353 s; the cold run was faster than the warm sample of the same head, 572 s, because the
hosted runner's warm spread of up to 105 s dominates the cache effect). The BIND names added after it
(three names, 33.30 s on hosted macOS) give 580 s from that measurement (margin 320 s) and a conservative
717 s from the 12-17 warm sample plus the F-136 cold cost of 190 s (margin 183 s). The reversal rule of the
Phase 12 G4 exception has not triggered (no measured or projected cold run above 900 s, no cancelled run,
and since F-139 no projected margin under 120 s). The cold run with the final 354-name set is an
orchestrator step after the push of plan 12-18 and is not claimed here; no projection is presented as a
measurement.

## API-07: LibFuturo fit evaluation

`docs/evidence/phase12-libfuturo-fit.md` is the written API-07 evaluation (plan 12-10). It is pinned to
`Pinned commit: 4177a43e67312e2db88ab21cba13aa7350063105` of the sibling LibFuturo project, a local,
unpublished SHA (no remote configured; read from committed objects only, the working tree was not read),
equal to the pin of the Phase 11 paper read and of PIN-IMPACT "API-07 pin". Its ten-axis comparison
rates 3 axes as a one-to-one fit, 6 as convertible with loss and 1 as missing, and its conclusion is
"no adapter now": any adapter stays optional and must not shape the core API; no adapter code, target or
test exists, no core API changed for it, and no record claims LibFuturo support. `repo.api07-evaluation`
compares the pin line with the pin-impact row and fails on a mismatch.

## Roadmap success criteria

| Criterion | Evidence |
| --- | --- |
| 1. an authored title (16x16 square by D-pad or stick, one AI tone on A, one save slot; frame-locked on the VI interrupt; no Z, textures, RSP microcode or TLB) has manifest rows and produces expected checkpoint digests headlessly through the runner and the installed consumer | claim block 1 (7 names); the two manifest rows; the frame lock is a poll of the MI VI pending bit cleared by a VI_CURRENT write (DR-03); 12-REVIEW "TITLE" (b) and "FINAL" API-06 row |
| 2. the host pump is unit-tested in the required gate; scripted input gives byte-identical video, audio and save results to the headless run at two fake-clock speeds; a virtual joystick under SDL dummy drivers exercises button and axis mapping | claim block 2 (8 names, `host.pump.title-two-speeds` at 3 Hz and 240 Hz of fake time); the record's `sdl-lane` stage, status pass, `sc2_virtual_joystick` closed, and the final audit's lane line `[host-sdl] PASS` (SDL 3.4.10, mapping rows 30/30): host glue only, no rendering claim |
| 3. the SDL3 shell is optional, off by default and in a non-gating lane; the core builds and links with no SDL dependency and a build without the host target succeeds | `T64H_SDL` OFF by default; no CTest name and no workflow step builds SDL; the gate build has no host target; the fresh-archive stage builds `t64_core` alone from `git archive`; `repo.host-isolation`, `repo.host-policy`; supporting, not counted in a block |
| 4. a written LibFuturo fit evaluation pinned to a commit SHA exists with an expected no-adapter-now conclusion, and the final ABI bind is made, or each defect-driven amendment is recorded with evidence and a re-run installed-consumer suite | the API-07 section above; the bind commit `03ccb01` and claim block 3 (12 names); no amendment was needed (PIN-IMPACT "Host-found API defects": 15 rows, 0 amend, `12-11 trigger: no`); the installed-consumer suite re-ran after the bind (plan 12-16: 50 of 50) and runs in every gate |

## Claims index

| Block | Profile id | Fixture set | Tier | Counting unit | Counted |
| ---: | --- | --- | --- | --- | --- |
| 1 | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k` | `phase12-title-headless` | unit-tested | registered CTest names | 7 of 7: `harness.title-oracle`, `runner.title-two-run`, `controls.real.title-wrong-input`, `controls.real.title-refused-frame`, `installed.consumer.title`, `installed.consumer.title-persist`, `installed.consumer.title-refused` |
| 2 | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k` | `phase12-host-pump` | unit-tested | registered CTest names | 8 of 8: `host.pump.build`, `-clock`, `-drain-order`, `-stall`, `-input-map`, `-script`, `-frames`, `-title-two-speeds` |
| 3 | `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k` | `phase12-bound-abi-record` | unit-tested | registered CTest names | 12 of 12: `repo.abi-header`, `-status-classes`, `-selftest`, `-prefix`, `-layout`, `-freeze-record`, `-freeze-selftest`, `install.setup`, `install.abi-header`, `-abi-layout`, `-abi-symbols`, `install.abs-paths` |

The claim blocks count 27 passed registered names: 15 of the 25 `PHASE12_TESTS` names (blocks 1 and 2),
each a passed testcase of the retained JUnit of the record's `phase12-suite` stage (0.016 to 4.212 s each)
and passed again in its full gate stage, and 12 `repo.abi*` and `install.*` names (block 3), each passed in
the record's `abi-cheap` stage and again in its gate stage. The other 10 `PHASE12_TESTS` names are supporting
tests and are not counted: the host guards `repo.host-include-guard`, its selftest, `repo.host-isolation`,
`repo.host-policy`, its selftest and `repo.p12-mutant-anchors`, the API-07 guard `repo.api07-evaluation`,
the bind proofs `unit.core.api.abi-major-rule` and `installed.controls.version-mismatch-major`, and
`harness.phase12-qualification`. The negative controls of other families are named in the claim prose and
not counted.

## Recorded exceptions for owner review

These items were decided under the delegated autonomy of the project policy, or are deviations the
phase recorded, and are listed here so the owner can review them. **None of them is an owner
approval**, and none changes a Phase 11 or earlier claim, tier or authority. The one owner decision of
the phase is the confirmation of the ABI 1.0 / package 1.0.0 numbers on 2026-11-26 as an ABI identity
(item 2); it is cited, not widened.

1. **Phase 12 G4 gate-growth exception (PIN-IMPACT "Gate-growth ledger", DR-26).** The required gate grew
   from 329 to 354 CTest names (25 `phase12` names) without removing equal time; no equal-time offset
   exists, and the names are the required evidence of SC1 to SC4. The SDL lane, the sanitizer, TSan,
   fresh-archive and full mutant runs stay local; no `ci.yml`, `check-ci-yml.sh` or `gate.sh` edit was
   made. Measured: cold `gate-macos` 547 s of 900 s at 351 names (run 45906047013, F-139, margin 353 s) and 729 s at
   the Phase 11 close (F-136); warm `gate-macos` 527 to 582 s over the phase's pushes (F-137, F-138, F-140). The reversal rule (a measured or projected cold run above 900 s, a cancelled run,
   or a projected margin under 120 s moves non-essential names, `installed.consumer.title-persist` and the
   selftests first, to a local lane) has not triggered. The cold run with the final 354-name set is the
   orchestrator's, after the push of plan 12-18.
2. **The D6 owner-review item of the 1.0 numbers (DR-25).** ABI 1.0 and package 1.0.0 are bound as the
   ABI identity and the package's ABI number, not as a release version; `project(VERSION)` stays 0.0.0 and
   tags, the release channel and release naming stay unselected. The owner confirmed the numbers on
   2026-11-26 and may choose release versioning later and independently; reversing the constant needs an
   ABI-breaking amendment, which is why it is listed here.
3. **The 12-11 outcome (DR-23).** The amendment slot was not triggered: the 15 host-found API observations
   (C-1 to C-6, H-01 to H-09) ended in 13 accept, 2 document (C-1 in `docs/spec/api.md` 8.3 by plan 12-07,
   H-03 in 8.1 by plan 12-15) and 0 amend, so plan 12-11 closed with a one-line SUMMARY, no code and no
   commit, and the bind needed no amendment row.
4. **Carry-ins deferred to v1.3, none dropped (PIN-IMPACT "Carry-in triage", 11 rows).** Done in Phase 12:
   FN-02, FN-03 and FN-04. Accepted: FN-01 and S-05 (the eight implicit padding holes stay bound as such,
   DR-24). No action: OIPL3. Deferred with reasons: T2 (replay-driver duplication in the runner), T5
   (closed recorder-failure detail names), T6 (`T64_REAL_KEEP_WORKSET`) and DEF-W1-01 (three unpinned
   sibling copies of the perspective floor in sealed RDP bodies) to v1.3, and T1 (splitting
   `unit.core.state.import`) beyond v1.2. Five carried items, the G3 limit. The final audit adds two text
   items for a later plan (FR-01: the comment of the non-installed `t64_facade.h`; FR-02: one sentence of
   `docs/spec/api.md` 2.3) and the HOST info items HR-01 and HR-02 as candidates.
5. **The owner checklist is pending.** `docs/evidence/phase12-host-manual.md` (human-needed, non-blocking,
   owner-attested) is a template marked "not run"; no agent filled, attested or marked it, and nothing in
   this document or the claims rests on it. The shell keeps no host-side save file (DR-19), so its
   relaunch line is expected to show the start position; the restored position is shown headlessly by
   `installed.consumer.title-persist`.
6. **Runner scope for the title (DR-34, orchestrator decision B of 2026-11-26).** The runner cannot drain a
   49,152-byte video packet without raw capture and refuses artifact producers together with raw capture,
   so `runner.title-two-run` is one raw-capture run with the two exclusions recorded as executable facts
   (the name keeps its spelling), and the save-image equality and the refused ROM's blank-frame assertion
   are the installed consumer's (`installed.consumer.title`, `-persist`, `-refused`). A runner contract
   change in a numbered plan would give the runner its save-out run back.
7. **Plan-count and file-count exceptions (ROADMAP Phase 12 G5 note, PIN-IMPACT "Plan file-count
   exceptions").** The phase took 18 plans against a guideline of about 10 to 15 (four TITLE plans and
   their review, four HOST plans with the conditional slot and their review, four BIND plans, the
   qualifier and this plan; DR-30); eight plans are on or above the 10-file warning band, each with its
   reason; no plan had more than 4 tasks.
8. **Local Linux evidence limited by the environment (final audit FR-03).** The Docker engine of the
   planning host did not answer from plan 12-16 on, so the act Linux run of the three version controls is
   recorded as `act-linux: SKIP` and the preflight `linux-compile` step failed with "docker is unavailable"
   before the pushes of plans 12-17 and 12-18; no engine was started or restarted. This is an
   environmental limitation and not a pass: the Linux compile and run evidence is the hosted `gate-linux`
   job of each push (run 45932117914 of plan 12-17 and the run of the 12-18 push recorded under "Hosted records").
9. **Accepted limits of the bound ABI (DR-24, review items S-05 and I-05).** The `value_out` span of
   `t64_machine_bus_read` stays the caller's responsibility, the eight implicit padding holes are bound
   and pinned for 64-bit pointers only, and the "an older minor is accepted" half of the create rule is
   not exercised until a library minor above 0 exists (`unit.core.api.abi-major-rule` prints NOT
   EXERCISED); `docs/spec/api.md` section 15 lists them.

## Limitations and preserved stops

- Authored scope only: the authored title, its spec-derived oracle and hand-written host and consumer
  expectations. No hardware measurement, other-emulator comparison, oracle operation or commercial title
  was used or is claimed.
- The pump equality is a measured fact of this title and its two scripts at two fake-clock speeds; it is
  no proof for any guest, says nothing about speed on a device (the unthrottled pump ran below real time
  on the planning host, PIN-IMPACT H-09, information only) and makes no latency or A/V sync statement.
- The SDL3 shell is optional, outside the gate and outside every claim block. Its local lane is
  self-checking host glue under SDL's dummy drivers (no window is shown and no sound is played); it binds
  no rendering, timing or audio statement.
- A software-interface claim about the record bound at ABI 1.0: a later change to a bound element follows
  the rule after the bind (a recorded amendment with its cost), never a silent edit. The layout pins hold
  for targets with 64-bit pointers (macOS arm64 and Linux x86_64); on other data models no layout is
  bound.
- The installed package is one configuration (Release) built by the project's own preset; the claims hold
  for the exact build id above. State blobs and container formats stay build-identity-bound and outside
  the interface promise.
- Raw logs, state, save, capture and record files stay private and are never uploaded; this document
  holds digests, sizes, counts and public run numbers only.
- D4 (SDL3 link-only against a system install, never vendored), D5 (no oracle material), D6 (no tag, no
  release, no channel) and D7 (the neutral window title "t64 minimal host (development)", no icon, bundle
  or product name) stay in force. Phase 7 VID-04 and the oracle request stay blocked. No ROM, firmware, PIF
  or IPL image of a commercial product, 64DD media or commercial save file was used; the only embedded
  IPL3 is the separately rights-cleared Unlicense payload recorded in the fixture manifest. The closed
  Phase 6 to 11 claims and evidence documents are unchanged.

## Reproduction

The private record is an untracked build output kept on the machine that ran the qualification; the
check below needs it, the build trees the stages created and a checkout of the qualified history.

```sh
python3 -B scripts/qualify-phase12.py --verify build/gate/12-qualification-private/20261127T033336673294Z-40b7d2d9/record.json
sh scripts/check-claims.sh --files docs/claims/phase12.md
```

The first command must print `qualify-phase12: record verified against the current tree` and the line
`qualify-phase12: sdl-lane status: pass; sc2_virtual_joystick: closed` at H and at any later head
whose changes after the Qualification base are limited to `.planning/`, `docs/ci-reviews/LOG.md`,
`docs/claims/phase12.md`, `docs/evidence/phase12-host-manual.md` and this document; any other changed path
refuses it by design. The second prints `claims: files=1 blocks=3 errors=0`. The closed Phase 11 records
are unchanged: `git diff --quiet 60b9e3c34fc0efc0ff64296cfa31415565b847b2 HEAD -- docs/claims/phase11.md
docs/evidence/phase11-qualification.md` exits 0.
