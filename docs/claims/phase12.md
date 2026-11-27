# Phase 12 claims

This file holds the Phase 12 claim blocks: three blocks, one profile id and three fixture sets.
Every block uses only the tier `unit-tested` of the [template](TEMPLATE.md). The blocks are
authored-scope claims: an authored title built in this repository with the pinned GNU Binutils
2.45 and the separately rights-cleared open IPL3, run through the supervised runner, through the
installed package and through the host pump, and the ABI record bound at 1.0. The expected
values come from the title's own behavior authority and its standard-library oracle
(`fixtures/title/title-spec.md`, `fixtures/title/title_oracle.py`, committed before the guest
source), from hand-written expectations of the host test cases and from the bound record and its
four goldens, all kept outside the code under test. No hardware capture, no other emulator, no
oracle material and no commercial software was used, and none is claimed. The scope, the exact
bindings, the native and hosted records, the exceptions recorded for the owner and the limits are
in `docs/evidence/phase12-qualification.md`.

How a block is counted. The counted unit is stated in the paragraph before each block. Every
block counts registered CTest names. The names of blocks 1 and 2 are `PHASE12_TESTS` names of
`scripts/check-test-inventory.sh` (label `phase12`); each is a passed testcase in the retained
JUnit of the `phase12-suite` stage of the final native qualification (record
`20261127T033336673294Z-40b7d2d9`) and passed again inside that record's full gate stage. The names of block 3 are
the `repo.abi*` and `install.*` names of the gate preset; each passed in the record's `abi-cheap`
stage and again in its gate stage. The `pass` and `total` values equal the number of counted
names. Any other test a paragraph mentions is named as a supporting test and is not counted. A
raw check total inside a case is not a measure of strength and is not used as one.

Recorded pins. The completion tick, the stop reason, the final state digest and the video packet
count of a run have no independent source. The short scenario's four values are committed in
`fixtures/title/title.runner-pins.json` at the top level with the label "runner-recorded pin, not
independent" (the supervised runner's raw-capture run of plan 12-04), and the long scenario's four
values under the nested key `consumer` with the label "consumer-recorded pin, not independent"
(the installed consumer of plan 12-05). Every case that reports one of them prints its label, and
no block calls a recorded pin independent; the frames, the audio packets, the transcript and the
save image are compared with the independent oracle values.

Limits that apply to every block. The blocks bind the one profile id that every title case uses,
`ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k` (the profile of the Phase 10 composite fixture, so
the title and that fixture share an identity); no other id is exercised. A claim holds for the
exact build and source commit named in the evidence document, on the two data models the layout
pins cover (64-bit pointers: macOS arm64 and Linux x86_64). ABI 1.0 and package 1.0.0 are the ABI
identity and the package's ABI number, not a release version: `project(VERSION)` stays 0.0.0,
nothing was released or tagged and no package channel exists. The pump runs against an injected
fake clock: equality at two fake-clock speeds says that the pacing does not change the results; it
says nothing about speed on a device. The optional SDL3 shell is outside every block. Its local
lane (`sh scripts/check-host.sh sdl`) is self-checking host glue under SDL's dummy drivers with a
virtual gamepad, and says nothing about rendering correctness, timing or audio fidelity; the
SC2 virtual-joystick clause is closed only by a lane PASS, and the final record holds
`sdl_lane_status` `pass` (`sc2_virtual_joystick` `closed`). The physical-controller
play-through is the owner-attested, non-blocking checklist `docs/evidence/phase12-host-manual.md`,
which is pending (not run) and is no evidence here. The LibFuturo fit evaluation
(`docs/evidence/phase12-libfuturo-fit.md`, conclusion "no adapter now") is a written evaluation,
not a claim, and no adapter exists. The Phase 8 to 11 claims in `phase08.md`, `phase09.md`,
`phase10.md` and `phase11.md` are separate and are not renewed here. Phase 7 VID-04 stays deferred
and unclaimed: the oracle request stays blocked, and no oracle was executed, copied or used.

Wording rule: the three words reserved by the [template](TEMPLATE.md) stay out of this file
entirely, and no prose line starts with the project name followed by a space.

## API-06: The authored title through the supervised runner and the installed consumer

Counting unit: registered CTest names. Counted (7): `harness.title-oracle` (the oracle's own
selftest and `--check` of `fixtures/title/title.expected.json` against the spec),
`runner.title-two-run` (the real supervised runner makes one raw-capture run of the short
scenario and compares its three 49,152-byte video packets, its two audio packets and its 20-byte
transcript with the oracle values, and the four runner-recorded pins under their label; the run
kinds the runner cannot make are executable facts of the same case, DR-34: a run without raw
capture ends at the drain of the first video packet, and the artifact producers are refused
together with raw capture, so the runner produces no save image of the title),
`controls.real.title-wrong-input` (the short-wrong script fails at checkpoint 2 under the short
expectation and passes under its own), `controls.real.title-refused-frame` (the refused-frame ROM
ends non-pass with no completion, `capture-error` and stop `host-error` at the 4,000,000-tick cap,
no transcript and no published bundle, with six same-setup controls each refused for its own
reason), `installed.consumer.title` (the consumer, built only against the moved install prefix,
runs the short, long and shifted scenarios and compares every checkpoint frame, the audio packets,
the transcript and the 32,768-byte save image read through `t64_machine_save_export` with the
independent literals of `tests/installed-consumer/t64c_title.h`, and the recorded pins under their
labels), `installed.consumer.title-persist` (the long run's save export imported into a fresh
instance shows the restored position; a no-import control shows the fresh-boot frame instead) and
`installed.consumer.title-refused` (every drained packet of the refused ROM equals the blank-frame
digest of 49,152 zero bytes, with no audio, no transcript and no completion, and the save export
equals the blank save digest). The runner compares the frames, audio and transcript of one
raw-capture run, and the installed consumer compares the save image and the refused ROM's blank
frames, because the runner produces neither (DR-34). The runner stays linked to the in-tree core.
Supporting tests, not counted: `install.setup`, `installed.consumer.build`,
`installed.consumer.literals` (the INDEPENDENT section of the title header equals the oracle
files, and the RECORDED PINS section equals the pins file under its exact labels),
`repo.installed-case-map`, `repo.consumer-include-guard` and its selftest.

```t64-claim
build_id: 0.0.0+t64-build-v1-06a20d33f9a16380b4f97721549b37dcf8ad39421459f7e702821e74dd897203
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k
tier: unit-tested
fixture_set: phase12-title-headless
manifest_digest: sha256:247205fa800c05764ee58008a89b48a5989acfde4b05c49cb1cf2fe7f04c7297
pass: 7
total: 7
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: independent-title-oracle
oracle_revision: title-expected-sha256:26ec0fb8702b2fa9cad0b3671361274e52a18beb7ef43a671d4f8b76ab347de8
not_claimed: compatibility with any console, title or other emulator, accuracy against hardware, hardware behavior, real-time play, throughput, latency, A/V sync, SDL rendering correctness, physical-controller play, LibFuturo support, any oracle comparison, a release, a tag, any package channel, independence of the completion tick, stop reason, final state digest and video packet count (each a runner-recorded or consumer-recorded pin, not independent), a save image or frames of the refused ROM shown by the runner (the installed consumer shows them, DR-34), the runner's own claims, other builds, other profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+t64-build-v1-06a20d33f9a16380b4f97721549b37dcf8ad39421459f7e702821e74dd897203 on profile ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k: unit-tested for phase12-title-headless @ sha256:247205fa800c05764ee58008a89b48a5989acfde4b05c49cb1cf2fe7f04c7297 (7/7; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: independent-title-oracle title-expected-sha256:26ec0fb8702b2fa9cad0b3671361274e52a18beb7ef43a671d4f8b76ab347de8. Not claimed: compatibility with any console, title or other emulator, accuracy against hardware, hardware behavior, real-time play, throughput, latency, A/V sync, SDL rendering correctness, physical-controller play, LibFuturo support, any oracle comparison, a release, a tag, any package channel, independence of the completion tick, stop reason, final state digest and video packet count (each a runner-recorded or consumer-recorded pin, not independent), a save image or frames of the refused ROM shown by the runner (the installed consumer shows them, DR-34), the runner's own claims, other builds, other profiles and fixture sets, unexercised behavior, commercial software.
```

## API-05 and API-06: The host pump, its unit rules and the scripted two-speed equality

Counting unit: registered CTest names. Counted (8): `host.pump.build` (the standalone `host/`
project configures and builds against the moved install prefix with `T64H_SDL` OFF and includes
nothing outside `tendies64/`), `host.pump.clock` (the guest-tick bound is the injected clock's
nanoseconds from the origin divided by 16, a backward reading is refused and counted, a terminal
stop reads no clock), `host.pump.drain-order` (video, then audio, then ISViewer text, inside the
advance loop at every backpressure stop and at the end of each step), `host.pump.stall` (a refusing
sink holds its packet, is retried once and then ends in "stall without progress", and loses
nothing once it accepts again; a guest completion latches), `host.pump.input-map` (the fixed
mapping table: buttons to the standard controller mask, the stick divided by 256 and clamped to
-128..127 with the dead zone, the vertical axis by complement, port 0 only, a removed pad
neutral), `host.pump.script` (the strict `t64.input` v1 reader with one named rule per refusal and
the QUEUE_FULL deferral by lowering the advance bound), `host.pump.frames` (frame copies honor
`stride_bytes` and refuse an oversized packet or a short destination; the audio byte swap) and
`host.pump.title-two-speeds` (the title's short and long scripts through the scripted source at
3 Hz and 240 Hz of fake time give byte-identical frames, audio packets and stream, transcript and
exported save at both speeds, equal to the oracle values, and a completion tick and final digest
equal to the runner-recorded pin for short and the consumer-recorded pin for long, under their
labels, judged by `scripts/check-host-title.py`). The expected values of the first seven names are
hand-derived in `host/tests/t64h_tests.c` from the pump rules of the decision record (DR-11 to
DR-14, DR-35, DR-36), never read from the pump at run time; the eighth uses the title oracle.
Supporting tests, not counted: `install.setup`, `repo.host-include-guard` and its selftest,
`repo.host-isolation`, `repo.host-policy` and its selftest, `repo.p12-mutant-anchors`, and the
local mutant run of the qualification (12 mutants of the pump and the title, each killed by its
named test), recorded in the evidence document.

```t64-claim
build_id: 0.0.0+t64-build-v1-06a20d33f9a16380b4f97721549b37dcf8ad39421459f7e702821e74dd897203
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k
tier: unit-tested
fixture_set: phase12-host-pump
manifest_digest: sha256:247205fa800c05764ee58008a89b48a5989acfde4b05c49cb1cf2fe7f04c7297
pass: 8
total: 8
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: independent-title-oracle-and-hand-derived-pump-rules
oracle_revision: title-expected-sha256:26ec0fb8702b2fa9cad0b3671361274e52a18beb7ef43a671d4f8b76ab347de8
not_claimed: compatibility with any console, title or other emulator, accuracy against hardware, hardware behavior, real-time play, throughput, latency, A/V sync, SDL rendering correctness, physical-controller play, LibFuturo support, any oracle comparison, a release, a tag, any package channel, speed or pacing on a device (the clock is an injected fake clock), fake-clock speeds other than 3 Hz and 240 Hz, input scripts other than the title's short and long scripts, independence of the completion tick and final digest (each a runner-recorded or consumer-recorded pin, not independent), the SDL3 shell and its local lane, other builds, other profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+t64-build-v1-06a20d33f9a16380b4f97721549b37dcf8ad39421459f7e702821e74dd897203 on profile ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k: unit-tested for phase12-host-pump @ sha256:247205fa800c05764ee58008a89b48a5989acfde4b05c49cb1cf2fe7f04c7297 (8/8; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: independent-title-oracle-and-hand-derived-pump-rules title-expected-sha256:26ec0fb8702b2fa9cad0b3671361274e52a18beb7ef43a671d4f8b76ab347de8. Not claimed: compatibility with any console, title or other emulator, accuracy against hardware, hardware behavior, real-time play, throughput, latency, A/V sync, SDL rendering correctness, physical-controller play, LibFuturo support, any oracle comparison, a release, a tag, any package channel, speed or pacing on a device (the clock is an injected fake clock), fake-clock speeds other than 3 Hz and 240 Hz, input scripts other than the title's short and long scripts, independence of the completion tick and final digest (each a runner-recorded or consumer-recorded pin, not independent), the SDL3 shell and its local lane, other builds, other profiles and fixture sets, unexercised behavior, commercial software.
```

## SC4: The ABI record bound at 1.0, software interface only

Counting unit: registered CTest names. Counted (12): `repo.abi-header` (the public headers
compile alone), `repo.abi-status-classes` (the status-class table against the header),
`repo.abi-selftest` (the ABI checker's own seeded cases), `repo.abi-prefix` (every internal
symbol of the archive carries the private prefix), `repo.abi-layout` (the generated layout pins of
`docs/abi/layout.c`), `repo.abi-freeze-record` (the four goldens recomputed from the installed
prefix equal the machine-read block of `docs/abi/FREEZE-CANDIDATE.md`: ABI 1.0, package 1.0.0,
`bound_minor` 0, `candidate_amendments` 0, 0 amendment rows, 35 public functions, 39 statuses, 8
stop reasons, 14 structs, 165 members, 190 constants and 8 padding holes), `repo.abi-freeze-selftest`
(the seeded cases of the bound-record and candidate rules), `install.setup` (a clean install of the
Development component into a relocated prefix with a space), `install.abi-header`,
`install.abi-layout` and `install.abi-symbols` (the same pins against the installed tree and
archive) and `install.abs-paths` (no source or build path below the prefix). The expected values
are the bound record and its four goldens, which the one coupled bind commit of plan 12-14 wrote
after a full proof in a scratch clone and which no later commit changed. Supporting tests, not
counted: `api04.members`, `api04.disasm`, `api04.headers` and `api04.seeded` (the archive imports
no file, clock, entropy or thread symbol), `unit.core.api.abi-major-rule` (the create-time rule at
major 1), `installed.controls.version-mismatch-minor`, `-patch` and `-major` (requests 1.1, 1.0.5
and 2.0 each refused with the headline tied to its own argument) and the `freeze` stage of the
record (the same check against the prefix of a `git archive` build outside the repository).

```t64-claim
build_id: 0.0.0+t64-build-v1-06a20d33f9a16380b4f97721549b37dcf8ad39421459f7e702821e74dd897203
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k
tier: unit-tested
fixture_set: phase12-bound-abi-record
manifest_digest: sha256:247205fa800c05764ee58008a89b48a5989acfde4b05c49cb1cf2fe7f04c7297
pass: 12
total: 12
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: bound-freeze-record
oracle_revision: freeze-record-sha256:856290c58397444abdec149cc842eb7a53b2bd15ea2028418ee2033ed775e162
not_claimed: compatibility with any console, title or other emulator, accuracy against hardware, hardware behavior, real-time play, throughput, latency, A/V sync, SDL rendering correctness, physical-controller play, LibFuturo support, any oracle comparison, a release, a tag, any package channel, a release version (ABI 1.0 and package 1.0.0 are the ABI identity and the package's ABI number), a change of a bound element outside the amendment rule of the bound record, layouts on targets with pointers narrower than 64 bits, state blob or container formats, a postcondition the counted names do not call, other builds, other profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+t64-build-v1-06a20d33f9a16380b4f97721549b37dcf8ad39421459f7e702821e74dd897203 on profile ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k: unit-tested for phase12-bound-abi-record @ sha256:247205fa800c05764ee58008a89b48a5989acfde4b05c49cb1cf2fe7f04c7297 (12/12; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: bound-freeze-record freeze-record-sha256:856290c58397444abdec149cc842eb7a53b2bd15ea2028418ee2033ed775e162. Not claimed: compatibility with any console, title or other emulator, accuracy against hardware, hardware behavior, real-time play, throughput, latency, A/V sync, SDL rendering correctness, physical-controller play, LibFuturo support, any oracle comparison, a release, a tag, any package channel, a release version (ABI 1.0 and package 1.0.0 are the ABI identity and the package's ABI number), a change of a bound element outside the amendment rule of the bound record, layouts on targets with pointers narrower than 64 bits, state blob or container formats, a postcondition the counted names do not call, other builds, other profiles and fixture sets, unexercised behavior, commercial software.
```
