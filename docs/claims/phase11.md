# Phase 11 claims

This file holds the Phase 11 claim blocks: four blocks for the installed-consumer suite, one
profile id and four fixture sets. Every block uses only the tier `unit-tested` of the
[template](TEMPLATE.md) and is a software-interface claim about the installed package of the
freeze candidate (ABI 0.1, package 0.1.0, `docs/abi/FREEZE-CANDIDATE.md`): what the public C API
did, through a clean relocated install, for an independent consumer that includes only the
installed header. The expected values are hand-written literals of the consumer, the independent
expectation of the Phase 10 composite fixture and the status-class table, all kept outside the
code under test; no hardware capture, no other emulator, no oracle material and no commercial
software was used, and none is claimed. The scope, the exact bindings, the native and hosted
records, the exceptions recorded for the owner and the limits are in
`docs/evidence/phase11-qualification.md`.

How a block is counted. The counted unit is stated in the paragraph before each block. Every
block counts registered CTest names: each counted name is one of the `PHASE11_TESTS` names of
`scripts/check-test-inventory.sh` (label `phase11`) and is a passed testcase in the retained JUnit
of the `installed` stage of the final native qualification (record `20261126T091014977323Z-aa393c22`), and passed
again inside that record's full gate stage. The `pass` and `total` values equal the number of
counted names. Any other test a paragraph mentions is named as a supporting test and is not
counted; the negative controls are named and not counted. A raw check total inside a case is not
a measure of strength and is not used as one.

Three-way scope (PIN-IMPACT DR-32, checker finding B-1). The installed consumer equals the in-tree
runner on every compared value, and equals the independent literals of
`fixtures/composite/composite.expected.json` on the video packets, the audio packets, the save
image, the transcript and the poll state. The completion tick (599357), the stop reason
(`GUEST_COMPLETION`), the final state digest and the video packet count (3) have no independent
source: they are committed as `fixtures/composite/composite.runner-pins.json` with the label
"runner-recorded pin, not independent" and the citation of the Phase 10 supervised runs, and no
block calls them independent.

Limits that apply to every block. The blocks bind the one profile id that every case of the
installed consumer uses, `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k`; no other id is exercised
through the installed package. A claim holds for the exact build and source commit named in the
evidence document, on the two data models the layout pins cover (64-bit pointers: macOS arm64
and Linux x86_64). ABI 0.1 is a freeze candidate and not a final or stable interface: between
the candidate and the Phase 12 final bind only defect-driven amendments with evidence and a
re-run of this suite may change it, and the final bind may confirm it or list amendments. The
package version 0.1.0 is the ABI candidate number, not a release version; nothing was released
or tagged and no package channel exists. State blobs stay build-identity-bound and outside the
interface promise. The sanitizer and two-thread runs are local lanes of the qualification, not
gate names, and are recorded in the evidence document, not counted here. The Phase 8 to 10
claims in `phase08.md`, `phase09.md` and `phase10.md` are separate and are not renewed here.
Phase 7 VID-04 stays deferred and unclaimed: the oracle request stays blocked, and no oracle was
executed, copied or used.

Wording rule: the three words reserved by the [template](TEMPLATE.md) stay out of this file
entirely, and no prose line starts with the project name followed by a space.

## VER-05: The composite fixture through the installed package, three ways

Counting unit: registered CTest names. Counted (3): `installed.consumer.composite` (the consumer,
built only against the moved install prefix, admits the authored composite ROM, runs it to
completion and compares three video packets, two audio packets, the 32,768-byte save image, the
188-byte transcript and the polled words with the independent literals of its own header
`tests/installed-consumer/t64c_composite.h`, and the completion tick, stop reason, final digest
and packet count with the runner-recorded pins under their label), `installed.consumer.literals`
(`scripts/check-consumer-literals.py --check`: the INDEPENDENT section of that header equals
`fixtures/composite/composite.expected.json` and `composite.input.json`, the pins section equals
`composite.runner-pins.json`, whose label must be exactly "runner-recorded pin, not independent",
and the cited Phase 10 record contains the pinned tick and digest) and
`installed.consumer.three-way` (the in-tree runner's raw-capture and save-out runs and the
installed consumer agree on every compared value: tick 599357, `GUEST_COMPLETION`, the digest
`97f270274d456e883ceaa11f100f92c7fbf3150e89031fa51164e79991f1267c` and 3 packets as
runner-recorded pins, not independent; the packets, the save and the transcript byte for byte as
independent literals). The runner stays linked to the in-tree core and was not pointed at the
installed package. Supporting tests, not counted: `install.setup`, `installed.consumer.build`,
`installed.consumer.smoke`, `repo.consumer-include-guard` and its selftest (the consumer includes
nothing outside `tendies64/`), `repo.installed-case-map`, and the ten negative controls
`installed.controls.missing-package`, `-version-mismatch-minor`, `-version-mismatch-patch`,
`-visible-source-tree`, `-visible-source-tree-seed`, `-doctored-header-abi`,
`-doctored-header-size`, `-relocated-spaces`, `-symbol-collision` and `-plain-cc`, each of which
fails, or for the four positive rows passes, for its own stated reason.

```t64-claim
build_id: 0.0.0+t64-build-v1-35e6e1f1d4a1b5fd50b579c0cfb9dd36ac658099d0a207db610006433c1a22d5
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k
tier: unit-tested
fixture_set: phase11-installed-composite
manifest_digest: sha256:a5788a02d12041c0f44f5a8a3aec226f27fbac16d457f4e85b865ad0bd0a2d88
pass: 3
total: 3
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: independent-composite-literals
oracle_revision: composite-expected-sha256:32de79ec5162a37697ef828b1761cf32e6976bd5efffc6c8c21c9dba6729cd19
not_claimed: compatibility with any console, title or other emulator, accuracy against hardware, hardware behavior, a final or stable ABI (ABI 0.1 is a candidate), a release, a tag, any package channel, any oracle comparison, independence of the completion tick, stop reason, final state digest and video packet count (each a runner-recorded pin, not independent), the runner's own claims, other builds, other profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+t64-build-v1-35e6e1f1d4a1b5fd50b579c0cfb9dd36ac658099d0a207db610006433c1a22d5 on profile ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k: unit-tested for phase11-installed-composite @ sha256:a5788a02d12041c0f44f5a8a3aec226f27fbac16d457f4e85b865ad0bd0a2d88 (3/3; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: independent-composite-literals composite-expected-sha256:32de79ec5162a37697ef828b1761cf32e6976bd5efffc6c8c21c9dba6729cd19. Not claimed: compatibility with any console, title or other emulator, accuracy against hardware, hardware behavior, a final or stable ABI (ABI 0.1 is a candidate), a release, a tag, any package channel, any oracle comparison, independence of the completion tick, stop reason, final state digest and video packet count (each a runner-recorded pin, not independent), the runner's own claims, other builds, other profiles and fixture sets, unexercised behavior, commercial software.
```

## API-01: One accepted case and one refusal per operation through the installed package

Counting unit: registered CTest names. Counted (17): `installed.consumer.ops-lifetime`,
`-handshake`, `-struct-size`, `-profile`, `-admission`, `-input`, `-advance`, `-reset`, `-unload`,
`-rings`, `-text`, `-save`, `-state`, `-queries`, `-trace`, `-bus` and `-poison-query`. Each case
calls the operations of its name through the installed header and archive with at least one
accepted call and its stated postcondition and at least one refused call, whose expected status
and untouched outputs are hand-written in `tests/installed-consumer/ops.c` from the header and
`docs/spec/api.md`, never read from the core at run time; the refused class of each status comes
from the consumer's own table `t64c_classes.h`, which `repo.abi-status-classes` compares with
`docs/abi/status-classes.txt`. The rings case forces output backpressure and drains it. Supporting
tests, not counted: the in-tree `unit.core.api.*` names, `repo.abi-header`, `install.abi-header`,
`install.abi-layout`, `install.abi-symbols`, `repo.abi-freeze-record` and
`repo.abi-freeze-selftest`.

```t64-claim
build_id: 0.0.0+t64-build-v1-35e6e1f1d4a1b5fd50b579c0cfb9dd36ac658099d0a207db610006433c1a22d5
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k
tier: unit-tested
fixture_set: phase11-installed-ops
manifest_digest: sha256:a5788a02d12041c0f44f5a8a3aec226f27fbac16d457f4e85b865ad0bd0a2d88
pass: 17
total: 17
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: authored-hand-derived-postconditions
oracle_revision: consumer-ops-sha256:3f38c1a240ac5514eeb41af9489b9b887ed8378de18fb4174f4a3ef74d0be4ef
not_claimed: compatibility with any console, title or other emulator, accuracy against hardware, hardware behavior, a final or stable ABI (ABI 0.1 is a candidate), a release, a tag, any package channel, any oracle comparison, a postcondition the cases do not call, layouts on targets with pointers narrower than 64 bits, other builds, other profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+t64-build-v1-35e6e1f1d4a1b5fd50b579c0cfb9dd36ac658099d0a207db610006433c1a22d5 on profile ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k: unit-tested for phase11-installed-ops @ sha256:a5788a02d12041c0f44f5a8a3aec226f27fbac16d457f4e85b865ad0bd0a2d88 (17/17; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: authored-hand-derived-postconditions consumer-ops-sha256:3f38c1a240ac5514eeb41af9489b9b887ed8378de18fb4174f4a3ef74d0be4ef. Not claimed: compatibility with any console, title or other emulator, accuracy against hardware, hardware behavior, a final or stable ABI (ABI 0.1 is a candidate), a release, a tag, any package channel, any oracle comparison, a postcondition the cases do not call, layouts on targets with pointers narrower than 64 bits, other builds, other profiles and fixture sets, unexercised behavior, commercial software.
```

## API-01 and API-03: The host-shaped loop and two instances

Counting unit: registered CTest names. Counted (4): `installed.consumer.hostloop` (frame-paced
bounded advances, video and audio drained in host order, a forced backpressure stop with
`RESOURCE_LIMIT` at guest tick 540952 resumed after a drain, and a seeded host that never drains
detected as a stall without progress), `installed.consumer.hostloop-cadence` (four drain
schedules end at the same tick and digest), `installed.consumer.hostloop-delay` (a 1 ms host
delay between slices changes nothing the core reports; host wall time is never an input) and
`installed.consumer.two-instance` (two instances interleaved in one process on different
schedules both end at the pinned tick and digest, and destroying the first leaves the second's
digest and allocator ledger unchanged). Each run compares its outputs with the independent
literals and its tick, stop reason and digest with the runner-recorded pins under their label.
Supporting test, not counted: the local two-thread run `sh scripts/check-install.sh tsan` (one
instance per thread, no ThreadSanitizer report), recorded in the evidence document.

```t64-claim
build_id: 0.0.0+t64-build-v1-35e6e1f1d4a1b5fd50b579c0cfb9dd36ac658099d0a207db610006433c1a22d5
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k
tier: unit-tested
fixture_set: phase11-installed-hostloop
manifest_digest: sha256:a5788a02d12041c0f44f5a8a3aec226f27fbac16d457f4e85b865ad0bd0a2d88
pass: 4
total: 4
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: independent-composite-literals
oracle_revision: composite-expected-sha256:32de79ec5162a37697ef828b1761cf32e6976bd5efffc6c8c21c9dba6729cd19
not_claimed: compatibility with any console, title or other emulator, accuracy against hardware, hardware behavior, a final or stable ABI (ABI 0.1 is a candidate), a release, a tag, any package channel, any oracle comparison, independence of the completion tick, stop reason, final state digest and video packet count (each a runner-recorded pin, not independent), host wall-clock pacing or real-time output on a device, concurrent calls on one instance, other builds, other profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+t64-build-v1-35e6e1f1d4a1b5fd50b579c0cfb9dd36ac658099d0a207db610006433c1a22d5 on profile ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k: unit-tested for phase11-installed-hostloop @ sha256:a5788a02d12041c0f44f5a8a3aec226f27fbac16d457f4e85b865ad0bd0a2d88 (4/4; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: independent-composite-literals composite-expected-sha256:32de79ec5162a37697ef828b1761cf32e6976bd5efffc6c8c21c9dba6729cd19. Not claimed: compatibility with any console, title or other emulator, accuracy against hardware, hardware behavior, a final or stable ABI (ABI 0.1 is a candidate), a release, a tag, any package channel, any oracle comparison, independence of the completion tick, stop reason, final state digest and video packet count (each a runner-recorded pin, not independent), host wall-clock pacing or real-time output on a device, concurrent calls on one instance, other builds, other profiles and fixture sets, unexercised behavior, commercial software.
```

## API-02: The call-sequence generator through the installed package

Counting unit: registered CTest names. Counted (1): `installed.consumer.sequence` (a seeded
generator draws 400 calls over every operation with valid and invalid arguments; before each
classified call it observes the content digest, output status, the oldest packet infos, save
info, CPU stop detail, ROM size and header, the poison query and the allocator counters, and
after every call the consumer's class table calls refused it compares them bit for bit; a
guest-stop must make progress or be terminal as the table says; a backpressure stall resumes
after a drain; a flipped bit in the stored profile digest, found in the consumer's own allocator
storage, makes admission answer `INTERNAL_DEFECT`, after which every call but destroy and the
poison query answers `POISONED` with outputs untouched and destroy releases every block). In the
gate the machine digest is compared over every second refusal burst; the local lanes
`sh scripts/check-install.sh sanitize shipped` and `sanitize instrumented` run 20,000 steps
under AddressSanitizer and UndefinedBehaviorSanitizer against the shipped library (a digest
comparison after every burst) and an instrumented library (after every eighth burst); they are
qualification stages recorded in the evidence document, not counted.
Supporting tests, not counted: `repo.abi-status-classes`, `unit.core.api.refusals-never-poison`,
`unit.core.api.poison` and `unit.core.api.defect-sites`.

```t64-claim
build_id: 0.0.0+t64-build-v1-35e6e1f1d4a1b5fd50b579c0cfb9dd36ac658099d0a207db610006433c1a22d5
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k
tier: unit-tested
fixture_set: phase11-installed-sequence
manifest_digest: sha256:a5788a02d12041c0f44f5a8a3aec226f27fbac16d457f4e85b865ad0bd0a2d88
pass: 1
total: 1
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: refusal-snapshot-and-status-class-table
oracle_revision: status-classes-sha256:570548fc95fb3997a0f750bbe8cfdf443d6eadf4c0e5a38e5fe5d12ee50e6e5e
not_claimed: compatibility with any console, title or other emulator, accuracy against hardware, hardware behavior, a final or stable ABI (ABI 0.1 is a candidate), a release, a tag, any package channel, any oracle comparison, a machine-digest comparison after every refusal burst in the gate (every second burst there), call sequences the generator does not draw, other builds, other profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+t64-build-v1-35e6e1f1d4a1b5fd50b579c0cfb9dd36ac658099d0a207db610006433c1a22d5 on profile ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k: unit-tested for phase11-installed-sequence @ sha256:a5788a02d12041c0f44f5a8a3aec226f27fbac16d457f4e85b865ad0bd0a2d88 (1/1; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: refusal-snapshot-and-status-class-table status-classes-sha256:570548fc95fb3997a0f750bbe8cfdf443d6eadf4c0e5a38e5fe5d12ee50e6e5e. Not claimed: compatibility with any console, title or other emulator, accuracy against hardware, hardware behavior, a final or stable ABI (ABI 0.1 is a candidate), a release, a tag, any package channel, any oracle comparison, a machine-digest comparison after every refusal burst in the gate (every second burst there), call sequences the generator does not draw, other builds, other profiles and fixture sets, unexercised behavior, commercial software.
```
