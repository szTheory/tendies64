# Phase 10 claims

This file holds the Phase 10 claim blocks: one block per pair of a profile id and a fixture set,
eight in all. Every block uses only the tier `unit-tested` of the [template](TEMPLATE.md). The
expected values come from hand-derived literals and independent oracle scripts kept outside the
code under test; no hardware capture, no other emulator, no oracle material and no commercial
software was used, and none is claimed. The scope, the exact bindings, the native and hosted
records, the exceptions recorded for the owner and the limits are in
`docs/evidence/phase10-qualification.md`.

Policy labels: every Z mode rule, exponent decode, hidden-sidecar layout, tie rule, scan-out
expansion and timing figure named in the Phase 10 specs under `docs/spec/` is a named
software-profile policy of this project, not a measured hardware fact. The depth sidecar is a
logical software model with no statement about physical hidden bits, and the Z mode precedence
is software policy. Hosted evidence compares platform-invariant outputs and digests only; the
two hosted platforms build with different identities by design.

How a block is counted. The counted unit is stated in the paragraph before each block and the
`pass` and `total` values equal the number of counted units, every one of which passed in the
retained originals of the final native qualification (record
`20261125T134440416529Z-b9d957c3`). Blocks 1, 2 and 4 to 8 count registered CTest names: each
counted name is one of the 30 `PHASE10_TESTS` names of `scripts/check-test-inventory.sh`
(label `phase10`) and is a passed testcase both in the retained gate-preset JUnit and in the
retained ASan and UBSan JUnit of that record. Block 3 counts the accepted authored ROM row that
reported `pass` in the real supervised runner (one of one); the wrong-value control is named
and is not counted. Any other test a paragraph mentions is named as a supporting test and is
not counted. A raw check total inside a test is not a measure of strength and is not used as
one.

Limits that apply to every block. The suites that exercise more than one registered id are
bound here to the one id named in the block; the other ids they exercise are listed in the
evidence document and are not claimed. The state, digest, container and replay formats and the
runner flags and records are build-identity-bound and provisional until the Phase 11 freeze
candidate: a state blob is not promised to import into another build, no durable format is
stated and no application interface or binary interface is frozen. A claim holds for the exact
build and source commit named in the evidence document. The Phase 8 and Phase 9 claims in
`phase08.md` and `phase09.md` are separate and are not renewed here. Phase 7 VID-04 stays
deferred and unclaimed: the oracle request stays blocked, and no oracle was executed, copied
or used.

Wording rule: the three words reserved by the [template](TEMPLATE.md) stay out of this file
entirely, and no prose line starts with the project name followed by a space.

## VID-07: Depth-tested drawing on the descriptor 13 family

Counting unit: registered CTest names. Counted (3): `unit.core.rdp.depth-composite-accepted`
(SetZImage, SetPrimDepth and a primitive-Z triangle write the color bytes `0x07 0xc1`, the
depth bytes `0x09 0xa8` and the hidden sidecar byte `0x83`; a farther second primitive is
rejected and leaves color, depth and sidecar; a nearer third replaces the pixel),
`unit.core.rdp.depth-composite-wrong-depth` (the same setups compared with expected windows in
which one literal byte is changed must diverge at exactly that byte, and five setups that each
differ in one input must diverge at the byte that input controls) and
`unit.core.rdp.depth-composite-sidecar` (the four Z triangle opcodes, fifteen mode vectors, the
aliased 16-bit and 32-bit color and Z images, the invalidation of exactly the overlapped
sidecar granules by a CPU store, a PI DMA and an SP DMA, and the clear by reset). The literals
were derived by hand from the V7 depth policy and the printed exponent table that
`docs/spec/rdp.md` cites, checked with a scratch calculator kept outside the repository, and
no test calls the code under test to produce an expected value. The mutant rows
`v26-sidecar-invalidation-removed` (killed by the sidecar case) and `v26-digest-omit-scalar`
and `v26-digest-omit-hidden` (killed by `unit.core.digest.v26-completeness`) were KILLED in the
final qualification. The tests run on the four registered ids of the descriptor 13 family; this
block binds the stem id and the three save ids are exercised and not claimed. Supporting tests,
not counted: `unit.core.k1.dispatch-refusal` and `unit.core.k1.apply-refusal` (the shipped
composite ids keep refusing the depth commands), `unit.core.profile.v26-identity`,
`unit.core.profile.v26-isviewer`, `unit.core.profile.v26-predicate-matrix` and
`repo.p10-mutant-anchors`.

```t64-claim
build_id: 0.0.0+t64-build-v1-b6c298e66f1cbecb45d9f939a62d1c5b93dbc466c1ba7cf7980c291b2f136884
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-si-v2
tier: unit-tested
fixture_set: phase10-depth-composite
manifest_digest: sha256:a5788a02d12041c0f44f5a8a3aec226f27fbac16d457f4e85b865ad0bd0a2d88
pass: 3
total: 3
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: authored-hand-derived-literals
oracle_revision: depth-composite-test-sha256:c27ebfa0cd94c17877f98626b751fef7001b6b4e69741279f8a6999e5c1cf8a8
not_claimed: the three save ids of the same family, depth behavior of any other profile id, pixel agreement with hardware or with another emulator, Z behavior outside the authored vectors and mode rows, any claim about physical hidden bits, other builds, other profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+t64-build-v1-b6c298e66f1cbecb45d9f939a62d1c5b93dbc466c1ba7cf7980c291b2f136884 on profile ntsc-cart-4m-openipl3-rdram-b-t1-si-v2: unit-tested for phase10-depth-composite @ sha256:a5788a02d12041c0f44f5a8a3aec226f27fbac16d457f4e85b865ad0bd0a2d88 (3/3; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: authored-hand-derived-literals depth-composite-test-sha256:c27ebfa0cd94c17877f98626b751fef7001b6b4e69741279f8a6999e5c1cf8a8. Not claimed: the three save ids of the same family, depth behavior of any other profile id, pixel agreement with hardware or with another emulator, Z behavior outside the authored vectors and mode rows, any claim about physical hidden bits, other builds, other profiles and fixture sets, unexercised behavior, commercial software.
```

## CORE-06: State 26 round trip with depth active

Counting unit: registered CTest names. Counted (4): `unit.core.state.import-v26-roundtrip`
(idle, mid Z-triangle and mid scan-out states of the descriptor 13 ids, with depth active,
imported into fresh machines that then continue equal: canonical digest, color and depth
bytes, the 2,097,152 hidden bytes, the video and audio packets and the save bytes; a state 25
blob leaves the hidden table zero on a state 25 machine), `unit.core.state.import-v26-hostile`
(the sixteen authored hostile rows of `docs/spec/state-mutants-v26.txt`, every refusal leaving
the target allocation byte for byte as it was, and the v26 fuzz windows),
`unit.core.digest.v26-completeness` (every member of the RDP state, every byte of the coverage
table, strided TMEM bytes and the first, a middle and the last byte of the hidden depth table
flipped one bit at a time must change the canonical stream; the five depth members are plain
rows, not exclusions) and `unit.core.digest.v26-strip-differential` (with the markers, the
identity spans and the new-span classes removed, a v26 stream equals the v25 stream of the
corresponding si-v1 id for machines built by the same operations, so the v25 framing is
unchanged). One version marker holds: state serialization 26, digest domain `t64.state.si.26`,
container version word 2 with the format string `t64.state-container.v2`. The mutant rows
`v26-import-skip-hidden` and `v26-import-depth-after-proof` (killed by the round trip),
`v26-import-addr-unchecked` and `v26-import-z-products-unchecked` (killed by the hostile rows)
and the two digest omissions were KILLED in the final qualification. A round trip compares a
machine with its own export, so a layout that both sides share wrongly would pass it; the layout
is pinned separately by the export-side tests and the independent version 2 decoder, which are
supporting tests and are not counted: `unit.core.state.export-v26`,
`unit.core.state.export-v26-bounds`, `runner.state-format-v26` and `runner.composite-state-v2`.
The suites run on the four registered ids; this block binds the stem id and the three save ids
are exercised and not claimed.

```t64-claim
build_id: 0.0.0+t64-build-v1-b6c298e66f1cbecb45d9f939a62d1c5b93dbc466c1ba7cf7980c291b2f136884
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-si-v2
tier: unit-tested
fixture_set: phase10-state-26-roundtrip
manifest_digest: sha256:a5788a02d12041c0f44f5a8a3aec226f27fbac16d457f4e85b865ad0bd0a2d88
pass: 4
total: 4
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: authored-layout-literals-and-hostile-rows
oracle_revision: state-mutants-v26-sha256:92fc88ae665da0fe659ce3b734eebb748253823e19838a9854d928554903e7ca
not_claimed: import of a state exported by another build, a durable or frozen state or container format, cross-version import beyond the refused rows, the three save ids of the same family, the container decoder comparison of the export side, states of other emulators, a stable layout before the Phase 11 freeze candidate, other builds, other profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+t64-build-v1-b6c298e66f1cbecb45d9f939a62d1c5b93dbc466c1ba7cf7980c291b2f136884 on profile ntsc-cart-4m-openipl3-rdram-b-t1-si-v2: unit-tested for phase10-state-26-roundtrip @ sha256:a5788a02d12041c0f44f5a8a3aec226f27fbac16d457f4e85b865ad0bd0a2d88 (4/4; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: authored-layout-literals-and-hostile-rows state-mutants-v26-sha256:92fc88ae665da0fe659ce3b734eebb748253823e19838a9854d928554903e7ca. Not claimed: import of a state exported by another build, a durable or frozen state or container format, cross-version import beyond the refused rows, the three save ids of the same family, the container decoder comparison of the export side, states of other emulators, a stable layout before the Phase 11 freeze candidate, other builds, other profiles and fixture sets, unexercised behavior, commercial software.
```

## RUN-07: One composite guest through the real runner

Counting unit: one accepted authored ROM row that reported `pass` in the real supervised
runner. Counted (1): `phase10-composite-s32k`, run by `runner.composite-two-run` on the `-s32k`
id twice: a raw-capture run (three video frames, two audio packets, a bundle of 2,449 bytes)
and a save-out run (a 32,768-byte save image). The guest draws two Z triangles over a 4x3
window (the farther one is rejected), plays two 16-byte audio packets, reads three controller
polls from an input script and writes and reads back a 16-byte SRAM record. Both runs report
`pass` and the same final state digest, and each output (the 48-byte video packet, the two
audio packets, the save image and the 188-byte transcript) is compared with its own expectation
committed from the independent twin oracle `fixtures/composite/composite_oracle.py` (standard
library only, written from the authored constants and the policy text, never from a ROM, the
core or the runner), each with a wrong-value control. The wrong-value controls, named and not
counted, are the corrupted-expectation cases inside `runner.composite-two-run` and
`controls.real.composite-wrong-rom` (a ROM that differs in one record word; its failing
outcome and transcript are compared whole against the oracle's wrong transcript, and its save
comparison uses the record the guest read back, because a failing run publishes no save).
Supporting tests, not counted: `harness.composite-oracle` (the committed expectation equals the
oracle's own output), `runner.composite-state-v2` and `runner.state-format-v26`.

```t64-claim
build_id: 0.0.0+t64-build-v1-b6c298e66f1cbecb45d9f939a62d1c5b93dbc466c1ba7cf7980c291b2f136884
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k
tier: unit-tested
fixture_set: phase10-composite-run
manifest_digest: sha256:a5788a02d12041c0f44f5a8a3aec226f27fbac16d457f4e85b865ad0bd0a2d88
pass: 1
total: 1
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: independent-twin-oracle-script
oracle_revision: composite-transcript-sha256:da07d916f0314db05e8d609326242d7f944098e96ce19a96339ef1cd6a7aad39
not_claimed: video or audio timing against hardware, output on a host device, the save image as a third-party save file format, a runner pass for a resumed run, other guests, other builds, other profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+t64-build-v1-b6c298e66f1cbecb45d9f939a62d1c5b93dbc466c1ba7cf7980c291b2f136884 on profile ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k: unit-tested for phase10-composite-run @ sha256:a5788a02d12041c0f44f5a8a3aec226f27fbac16d457f4e85b865ad0bd0a2d88 (1/1; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: independent-twin-oracle-script composite-transcript-sha256:da07d916f0314db05e8d609326242d7f944098e96ce19a96339ef1cd6a7aad39. Not claimed: video or audio timing against hardware, output on a host device, the save image as a third-party save file format, a runner pass for a resumed run, other guests, other builds, other profiles and fixture sets, unexercised behavior, commercial software.
```

## VER-06: VG-01, Z exponent boundaries and the depth rules on the depth profile

Counting unit: registered CTest names. The nine `unit.core.vg.*` names are cases of the one
binary `core/tests/test_vg_gaps.c`, each from hand-derived hex literals checked against a
calculator written from the documents, and each has at least one KILLED mutant in the final
qualification. A case that runs on more than one registered id is bound here to one id and the
other id is listed as exercised and not claimed (blocks 5 and 8). Counted (4):
`unit.core.vg.decal-equality`, `unit.core.vg.primitive-z-saturation`,
`unit.core.vg.delta-ceiling` (VG-01: decal equality, primitive-Z saturation and the ceiling
delta, mutants `vg01-decal-strict`, `vg01-zcap-7ffe` and `vg01-delta-floor`) and
`unit.core.vg.z-exponent-boundaries` (all seventeen range edges of the exponent table and the
`0xfffc` far word, mutants `vg-zexp-decode-top` and `vg-zexp-compress-round`).

```t64-claim
build_id: 0.0.0+t64-build-v1-b6c298e66f1cbecb45d9f939a62d1c5b93dbc466c1ba7cf7980c291b2f136884
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-rdp-depth-v7
tier: unit-tested
fixture_set: phase10-vg-depth
manifest_digest: sha256:a5788a02d12041c0f44f5a8a3aec226f27fbac16d457f4e85b865ad0bd0a2d88
pass: 4
total: 4
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: authored-hand-derived-literals
oracle_revision: vg-gaps-test-sha256:2256189e3c2a8fa405c7bb3833c7c434dfa26ef2cbf0860aa8fe1af3ac3fee31
not_claimed: depth behavior outside the authored vectors, agreement with hardware or with another emulator, the later registered ids that share the V7 policy, other builds, other profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+t64-build-v1-b6c298e66f1cbecb45d9f939a62d1c5b93dbc466c1ba7cf7980c291b2f136884 on profile ntsc-cart-4m-openipl3-rdram-b-t1-rdp-depth-v7: unit-tested for phase10-vg-depth @ sha256:a5788a02d12041c0f44f5a8a3aec226f27fbac16d457f4e85b865ad0bd0a2d88 (4/4; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: authored-hand-derived-literals vg-gaps-test-sha256:2256189e3c2a8fa405c7bb3833c7c434dfa26ef2cbf0860aa8fe1af3ac3fee31. Not claimed: depth behavior outside the authored vectors, agreement with hardware or with another emulator, the later registered ids that share the V7 policy, other builds, other profiles and fixture sets, unexercised behavior, commercial software.
```

## VER-06: The standard NTSC clock and the RGBA5551 alpha clear on the v8 VI profile

Counting unit: registered CTest names. Counted (2): `unit.core.vg.ntsc-clock` (the
standard-NTSC field deadline of 1,042,709 ticks against a literal, mutant `vi-clock-48600000`)
and `unit.core.vg.rgba5551-alpha-clear` (the RGBA5551 alpha clear on this profile's scan-out
path, mutants `vg03-alpha255-v8` and `vg03-alpha255-v9`). The alpha-clear case also runs the
v9 path on `ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9`; that id is exercised here and
not claimed (block 6 binds it for its own name).

```t64-claim
build_id: 0.0.0+t64-build-v1-b6c298e66f1cbecb45d9f939a62d1c5b93dbc466c1ba7cf7980c291b2f136884
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v8
tier: unit-tested
fixture_set: phase10-vg-vi-clock
manifest_digest: sha256:a5788a02d12041c0f44f5a8a3aec226f27fbac16d457f4e85b865ad0bd0a2d88
pass: 2
total: 2
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: authored-hand-derived-literals
oracle_revision: vg-gaps-test-sha256:2256189e3c2a8fa405c7bb3833c7c434dfa26ef2cbf0860aa8fe1af3ac3fee31
not_claimed: the v9 filtered path of the alpha-clear case, scan-out timing against hardware or against another emulator, any video standard other than the authored NTSC vector, other builds, other profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+t64-build-v1-b6c298e66f1cbecb45d9f939a62d1c5b93dbc466c1ba7cf7980c291b2f136884 on profile ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v8: unit-tested for phase10-vg-vi-clock @ sha256:a5788a02d12041c0f44f5a8a3aec226f27fbac16d457f4e85b865ad0bd0a2d88 (2/2; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: authored-hand-derived-literals vg-gaps-test-sha256:2256189e3c2a8fa405c7bb3833c7c434dfa26ef2cbf0860aa8fe1af3ac3fee31. Not claimed: the v9 filtered path of the alpha-clear case, scan-out timing against hardware or against another emulator, any video standard other than the authored NTSC vector, other builds, other profiles and fixture sets, unexercised behavior, commercial software.
```

## VER-06: The serrate row pair on the v9 filtered VI profile

Counting unit: registered CTest names. Counted (1): `unit.core.vg.serrate-filter-parity` (the
v9 serrate row pair with a weight-independent literal, mutants `vg02-parity-ignored` and
`vg02-next-row`).

```t64-claim
build_id: 0.0.0+t64-build-v1-b6c298e66f1cbecb45d9f939a62d1c5b93dbc466c1ba7cf7980c291b2f136884
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9
tier: unit-tested
fixture_set: phase10-vg-serrate
manifest_digest: sha256:a5788a02d12041c0f44f5a8a3aec226f27fbac16d457f4e85b865ad0bd0a2d88
pass: 1
total: 1
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: authored-hand-derived-literals
oracle_revision: vg-gaps-test-sha256:2256189e3c2a8fa405c7bb3833c7c434dfa26ef2cbf0860aa8fe1af3ac3fee31
not_claimed: filter behavior outside the authored row pair, agreement with hardware or with another emulator, other builds, other profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+t64-build-v1-b6c298e66f1cbecb45d9f939a62d1c5b93dbc466c1ba7cf7980c291b2f136884 on profile ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9: unit-tested for phase10-vg-serrate @ sha256:a5788a02d12041c0f44f5a8a3aec226f27fbac16d457f4e85b865ad0bd0a2d88 (1/1; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: authored-hand-derived-literals vg-gaps-test-sha256:2256189e3c2a8fa405c7bb3833c7c434dfa26ef2cbf0860aa8fe1af3ac3fee31. Not claimed: filter behavior outside the authored row pair, agreement with hardware or with another emulator, other builds, other profiles and fixture sets, unexercised behavior, commercial software.
```

## VER-06: The perspective floor on the texture profile

Counting unit: registered CTest names. Counted (1): `unit.core.vg.perspective-floor` (the
perspective floor on the S and T lanes, mutants `vg04-floor-off-s` and `vg04-floor-off-t`).
Three sibling copies of the same floor are not pinned behaviorally and stay a recorded
carry-in (DEF-W1-01 in the evidence document); they are not claimed.

```t64-claim
build_id: 0.0.0+t64-build-v1-b6c298e66f1cbecb45d9f939a62d1c5b93dbc466c1ba7cf7980c291b2f136884
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-rdp-texture-v4
tier: unit-tested
fixture_set: phase10-vg-perspective
manifest_digest: sha256:a5788a02d12041c0f44f5a8a3aec226f27fbac16d457f4e85b865ad0bd0a2d88
pass: 1
total: 1
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: authored-hand-derived-literals
oracle_revision: vg-gaps-test-sha256:2256189e3c2a8fa405c7bb3833c7c434dfa26ef2cbf0860aa8fe1af3ac3fee31
not_claimed: the sibling copies of the floor in the combiner texel path, the LOD projection and the major edge, texture behavior outside the authored vectors, other builds, other profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+t64-build-v1-b6c298e66f1cbecb45d9f939a62d1c5b93dbc466c1ba7cf7980c291b2f136884 on profile ntsc-cart-4m-openipl3-rdram-b-t1-rdp-texture-v4: unit-tested for phase10-vg-perspective @ sha256:a5788a02d12041c0f44f5a8a3aec226f27fbac16d457f4e85b865ad0bd0a2d88 (1/1; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: authored-hand-derived-literals vg-gaps-test-sha256:2256189e3c2a8fa405c7bb3833c7c434dfa26ef2cbf0860aa8fe1af3ac3fee31. Not claimed: the sibling copies of the floor in the combiner texel path, the LOD projection and the major edge, texture behavior outside the authored vectors, other builds, other profiles and fixture sets, unexercised behavior, commercial software.
```

## VER-06: The shade floor and the Bayer matrix

Counting unit: registered CTest names. Counted (1): `unit.core.vg.shade-floor-bayer` (the shade
floor on the triangle profile and every cell of the Bayer matrix on the blender profile,
mutants `vg05-shade-trunc` and `vg05-bayer-shift2`). The case runs on two registered ids;
this block binds `ntsc-cart-4m-openipl3-rdram-b-t1-rdp-triangle-v3` and the blender id
`ntsc-cart-4m-openipl3-rdram-b-t1-rdp-blender-v6` is exercised and not claimed.

```t64-claim
build_id: 0.0.0+t64-build-v1-b6c298e66f1cbecb45d9f939a62d1c5b93dbc466c1ba7cf7980c291b2f136884
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-rdp-triangle-v3
tier: unit-tested
fixture_set: phase10-vg-shade-bayer
manifest_digest: sha256:a5788a02d12041c0f44f5a8a3aec226f27fbac16d457f4e85b865ad0bd0a2d88
pass: 1
total: 1
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: authored-hand-derived-literals
oracle_revision: vg-gaps-test-sha256:2256189e3c2a8fa405c7bb3833c7c434dfa26ef2cbf0860aa8fe1af3ac3fee31
not_claimed: the blender profile's Bayer matrix as a separate claim, shading behavior outside the authored vectors, agreement with hardware or with another emulator, other builds, other profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+t64-build-v1-b6c298e66f1cbecb45d9f939a62d1c5b93dbc466c1ba7cf7980c291b2f136884 on profile ntsc-cart-4m-openipl3-rdram-b-t1-rdp-triangle-v3: unit-tested for phase10-vg-shade-bayer @ sha256:a5788a02d12041c0f44f5a8a3aec226f27fbac16d457f4e85b865ad0bd0a2d88 (1/1; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: authored-hand-derived-literals vg-gaps-test-sha256:2256189e3c2a8fa405c7bb3833c7c434dfa26ef2cbf0860aa8fe1af3ac3fee31. Not claimed: the blender profile's Bayer matrix as a separate claim, shading behavior outside the authored vectors, agreement with hardware or with another emulator, other builds, other profiles and fixture sets, unexercised behavior, commercial software.
```
