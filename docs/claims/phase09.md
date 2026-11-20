# Phase 9 claims

This file holds the Phase 9 claim blocks: one block per pair of a profile id and a fixture set,
ten in all. Every block uses only the tiers `unit-tested` and `regression-pinned` of the
[template](TEMPLATE.md). The expected values come from authored literals and independent
oracle scripts kept outside the code under test; no hardware capture, no other emulator and
no commercial software was used, and none is claimed. The scope, the exact bindings, the
native and hosted records and the limits are in `docs/evidence/phase09-qualification.md`;
the mapping of the roadmap success criteria to named tests and claim blocks is in
`docs/evidence/phase09-success-criteria.md`.

Policy labels: every tie rule, timing figure, busy ceiling, block mirror and button mapping
named in the Phase 9 specs under `docs/spec/` is a named software-profile policy of this
project, not a measured hardware fact. Community facts that informed a policy are restated
with their source and fetch date in those specs and are not copied here as evidence. Hosted
evidence compares platform-invariant outputs and digests only; the two hosted platforms
build with different identities by design.

How a block is counted. The counted unit is stated in the paragraph before each block and the
`pass` and `total` values equal the number of counted units, every one of which passed in the
retained originals of the exact-SHA native qualification. Blocks 1 to 5 count the accepted
authored ROM row that reported `pass` in the real supervised runner (one of one each); the
wrong-value controls are named and are not counted. Blocks 6 to 10 count registered CTest
names: each counted name is listed by a suite of `docs/spec/phase09-cases.json`, is a Phase 9
or baseline row of the test inventory and is a passed testcase in the retained normal-gate
JUnit. Any other test a paragraph mentions is named as a supporting test and is not counted.
A raw check total inside a test is not a measure of strength and is not used as one.

Limits that apply to every block. The suites that exercise more than one family id are bound
here to the stem id only; the other family ids they exercise are listed in the evidence
document and are not claimed. The state, digest, container and replay formats are
provisional until the Phase 10 freeze; no version marker moved during Phase 9, so nothing
here states a durable format. A claim holds for the exact build and source commit named in
the evidence document. The state digests recorded for the first hosted pair
(`docs/evidence/phase09-first-hosted-pair.json`) are valid for source `2db1be4d` only; the
qualifying pair in the evidence document supersedes them. No application interface or binary
interface is frozen. The Phase 8 claim in `phase08.md` is separate and is not renewed here.

Publication and resume limits (they bound blocks 5, 8, 9 and 10). A runner that fails to
publish a leaf publishes nothing. A supervisor failure after the runner has published
(a nonzero exit, a crash after output, a missing report or a malformed report) can leave the
leaf beside a failure record, so a caller accepts a leaf only with exit status 0 and a `pass`
record. A same-user swap, or a kill between the publisher's rename and its withdrawal, can
also leave a leaf; no protection against another process of the same user is claimed. A
resumed runner run never reports `pass`: a runner-published state resumes into the closed
unknown-log record, which carries the uninterrupted run's guest tick and state digest. Only
the resume mechanism and the digest equality are claimed, never a runner pass.

Wording rule: the three words reserved by the [template](TEMPLATE.md) stay out of this file
entirely, and no prose line starts with the project name followed by a space.

## SAVE-01: Controller reads through the modeled serial interface

Counting unit: one accepted authored ROM row that reported `pass` in the real supervised
runner. Counted (1): `phase09-pad-poll-si-v1`, run by `runner.pad-poll` on the stem profile.
The guest polls the standard controller on port 0 four times through the modeled serial
interface while input is scheduled at guest ticks, and prints a 68-byte transcript that must
equal the literal derived by `fixtures/pad/poll_oracle.py` (the scenario literals, the button
encoding and the strictly-after visibility rule are written there, outside the ROM and the
core). Count readings are model time of the named software-profile timing policy. Wrong-value
controls, named and not counted, run through `controls.real.pad-wrong-value`: a shifted
script, a neutral script, a wrong ROM (`phase09-pad-poll-wrong-si-v1`) and a replacement
ROM, each ending as the harness expects (a guest failure or a refusal), after the accepted
baseline of the same setup. Supporting tests, not counted: `unit.core.cpu.si-store-budget`,
`unit.core.si.atomic-queue-full`, `unit.core.si.device`, `unit.core.input.latch-partition`,
`unit.core.joybus.parser` and `controls.real.pad-wrong-value`.

```t64-claim
build_id: 0.0.0+t64-build-v1-b58d1ab6cd27f97c5ec87d2cb1ae90ce7ae2b877be61830c643342d6bbe2de22
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-si-v1
tier: unit-tested
fixture_set: phase09-pad-poll
manifest_digest: sha256:b75864fdf4404a4691bfed12ccaceaca22879e60ea739ef3cf89ca1eee6e80e0
pass: 1
total: 1
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: authored-transcript-literal
oracle_revision: pad-poll-transcript-sha256:715aa3854b455dc9fd8e038e1aa27e5d200ea311acbfd032930cdc760498d3f6
not_claimed: other ports or peripherals, physical controllers, joybus timing against hardware, rumble, the Controller Pak, other builds, other profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+t64-build-v1-b58d1ab6cd27f97c5ec87d2cb1ae90ce7ae2b877be61830c643342d6bbe2de22 on profile ntsc-cart-4m-openipl3-rdram-b-t1-si-v1: unit-tested for phase09-pad-poll @ sha256:b75864fdf4404a4691bfed12ccaceaca22879e60ea739ef3cf89ca1eee6e80e0 (1/1; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: authored-transcript-literal pad-poll-transcript-sha256:715aa3854b455dc9fd8e038e1aa27e5d200ea311acbfd032930cdc760498d3f6. Not claimed: other ports or peripherals, physical controllers, joybus timing against hardware, rumble, the Controller Pak, other builds, other profiles and fixture sets, unexercised behavior, commercial software.
```

## SAVE-02 and SAVE-04: Explicit EEPROM 4 Kbit selection

Counting unit: one accepted authored ROM row that reported `pass` in the real supervised
runner. Counted (1): `phase09-eeprom-rw-e4k-v1`, run by `runner.save-fixtures` on the explicit
`-e4k` profile id. The guest reads and writes 8-byte blocks through the modeled channel and
waits out the busy window in guest time, and prints a 191-byte transcript that must equal the
literal of `fixtures/save/save_oracle.py` (scenario `eeprom-4k`; the block table, the explicit
modulo rule for the 4 Kbit mirror and the busy deadline are written there, outside the ROM and
the core). The wrong-value control, named and not counted, is `phase09-eeprom-rw-wrong-e4k-v1`
run through `controls.real.save-wrong-byte`, together with a ROM and profile mismatch control;
the unit coverage of the same device is counted in block 6.

```t64-claim
build_id: 0.0.0+t64-build-v1-b58d1ab6cd27f97c5ec87d2cb1ae90ce7ae2b877be61830c643342d6bbe2de22
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e4k
tier: unit-tested
fixture_set: phase09-eeprom-rw
manifest_digest: sha256:b75864fdf4404a4691bfed12ccaceaca22879e60ea739ef3cf89ca1eee6e80e0
pass: 1
total: 1
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: authored-transcript-literal
oracle_revision: eeprom-rw-e4k-transcript-sha256:03891120e98176bc11f8d9f6cde6278ff33ef0ebe9018d2f24ce083b3d2c6502
not_claimed: FlashRAM, save-chip timing, title-based detection, battery or wear behavior, the six millisecond ceiling or the block mirror as measured hardware facts, protection of published save files against a same-user swap, other builds, other profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+t64-build-v1-b58d1ab6cd27f97c5ec87d2cb1ae90ce7ae2b877be61830c643342d6bbe2de22 on profile ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e4k: unit-tested for phase09-eeprom-rw @ sha256:b75864fdf4404a4691bfed12ccaceaca22879e60ea739ef3cf89ca1eee6e80e0 (1/1; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: authored-transcript-literal eeprom-rw-e4k-transcript-sha256:03891120e98176bc11f8d9f6cde6278ff33ef0ebe9018d2f24ce083b3d2c6502. Not claimed: FlashRAM, save-chip timing, title-based detection, battery or wear behavior, the six millisecond ceiling or the block mirror as measured hardware facts, protection of published save files against a same-user swap, other builds, other profiles and fixture sets, unexercised behavior, commercial software.
```

## SAVE-02 and SAVE-04: Explicit EEPROM 16 Kbit selection

Counting unit: one accepted authored ROM row that reported `pass` in the real supervised
runner. Counted (1): `phase09-eeprom-rw-e16k-v1`, run by `runner.save-fixtures` on the explicit
`-e16k` profile id. It prints a 192-byte transcript that must equal the literal of
`fixtures/save/save_oracle.py` (scenario `eeprom-16k`). No separate wrong-value row exists for
this ROM; the wrong-value row of block 2 shows that the comparator refuses a wrong byte, and
that row is not counted here.

```t64-claim
build_id: 0.0.0+t64-build-v1-b58d1ab6cd27f97c5ec87d2cb1ae90ce7ae2b877be61830c643342d6bbe2de22
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e16k
tier: unit-tested
fixture_set: phase09-eeprom-rw
manifest_digest: sha256:b75864fdf4404a4691bfed12ccaceaca22879e60ea739ef3cf89ca1eee6e80e0
pass: 1
total: 1
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: authored-transcript-literal
oracle_revision: eeprom-rw-e16k-transcript-sha256:d9a5bf956cce18ea6bc3fd563ae060d7dda3f8554a881b62214ccf89a7f4ca95
not_claimed: FlashRAM, save-chip timing, title-based detection, battery or wear behavior, the six millisecond ceiling or the block mirror as measured hardware facts, a wrong-value control specific to the 16 Kbit transcript, other builds, other profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+t64-build-v1-b58d1ab6cd27f97c5ec87d2cb1ae90ce7ae2b877be61830c643342d6bbe2de22 on profile ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e16k: unit-tested for phase09-eeprom-rw @ sha256:b75864fdf4404a4691bfed12ccaceaca22879e60ea739ef3cf89ca1eee6e80e0 (1/1; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: authored-transcript-literal eeprom-rw-e16k-transcript-sha256:d9a5bf956cce18ea6bc3fd563ae060d7dda3f8554a881b62214ccf89a7f4ca95. Not claimed: FlashRAM, save-chip timing, title-based detection, battery or wear behavior, the six millisecond ceiling or the block mirror as measured hardware facts, a wrong-value control specific to the 16 Kbit transcript, other builds, other profiles and fixture sets, unexercised behavior, commercial software.
```

## SAVE-02 and SAVE-04: Explicit SRAM selection

Counting unit: one accepted authored ROM row that reported `pass` in the real supervised
runner. Counted (1): `phase09-sram-rw-s32k-v1`, run by `runner.save-fixtures` on the explicit
`-s32k` profile id. The guest moves data through the SRAM window by word access and by a PI
transfer and prints a 101-byte transcript that must equal the literal of
`fixtures/save/save_oracle.py` (scenario `sram-32k`). The wrong-value control, named and not
counted, is `phase09-sram-rw-wrong-s32k-v1` run through `controls.real.save-wrong-byte` after
the accepted baseline of the same setup.

```t64-claim
build_id: 0.0.0+t64-build-v1-b58d1ab6cd27f97c5ec87d2cb1ae90ce7ae2b877be61830c643342d6bbe2de22
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-s32k
tier: unit-tested
fixture_set: phase09-sram-rw
manifest_digest: sha256:b75864fdf4404a4691bfed12ccaceaca22879e60ea739ef3cf89ca1eee6e80e0
pass: 1
total: 1
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: authored-transcript-literal
oracle_revision: sram-rw-transcript-sha256:52b0f1caee3ec470f4217236ce6f3da94e6ff1c15fd93143a68f7364b732a026
not_claimed: FlashRAM, save-chip timing, title-based detection, battery or wear behavior, SRAM timing against hardware, other builds, other profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+t64-build-v1-b58d1ab6cd27f97c5ec87d2cb1ae90ce7ae2b877be61830c643342d6bbe2de22 on profile ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-s32k: unit-tested for phase09-sram-rw @ sha256:b75864fdf4404a4691bfed12ccaceaca22879e60ea739ef3cf89ca1eee6e80e0 (1/1; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: authored-transcript-literal sram-rw-transcript-sha256:52b0f1caee3ec470f4217236ce6f3da94e6ff1c15fd93143a68f7364b732a026. Not claimed: FlashRAM, save-chip timing, title-based detection, battery or wear behavior, SRAM timing against hardware, other builds, other profiles and fixture sets, unexercised behavior, commercial software.
```

## SAVE-03 and SAVE-04: Save import and export through the runner

Counting unit: one accepted authored ROM row that reported `pass` in the real supervised
runner. Counted (1): `phase09-save-roundtrip-s32k`, run by `runner.save-roundtrip` on the
explicit `-s32k` profile id with a verified import file. The guest reads two blocks, rewrites
two, and prints four 36-byte lines (144 bytes) that must equal the literal of
`fixtures/save/roundtrip_oracle.py`, which writes the three byte-order selections (raw, swap16
and wordrev32) as explicit offset mappings from the policy text. The runner then publishes the
final image through the no-clobber publisher; the exported bytes are checked against the
oracle's export digest. Other cases of the same CTest name are supporting, not counted: five
format pairs, the EEPROM passthrough, a mis-declared format that reaches the transcript
comparator (the wrong-value control) and thirteen refused imports. The transforms are authored
policy of this project, not a hardware fact.

```t64-claim
build_id: 0.0.0+t64-build-v1-b58d1ab6cd27f97c5ec87d2cb1ae90ce7ae2b877be61830c643342d6bbe2de22
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-s32k
tier: unit-tested
fixture_set: phase09-save-roundtrip
manifest_digest: sha256:b75864fdf4404a4691bfed12ccaceaca22879e60ea739ef3cf89ca1eee6e80e0
pass: 1
total: 1
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: authored-transform-vectors
oracle_revision: save-roundtrip-transcript-sha256:649b62940ace25d9e4c31e12381f38df779a5bbef90dcf0baf1be1458e6b75c4
not_claimed: third-party save file formats, other emulators' conventions for byte order, protection of exported files against a same-user swap or a kill between publication steps, other builds, other profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+t64-build-v1-b58d1ab6cd27f97c5ec87d2cb1ae90ce7ae2b877be61830c643342d6bbe2de22 on profile ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-s32k: unit-tested for phase09-save-roundtrip @ sha256:b75864fdf4404a4691bfed12ccaceaca22879e60ea739ef3cf89ca1eee6e80e0 (1/1; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: authored-transform-vectors save-roundtrip-transcript-sha256:649b62940ace25d9e4c31e12381f38df779a5bbef90dcf0baf1be1458e6b75c4. Not claimed: third-party save file formats, other emulators' conventions for byte order, protection of exported files against a same-user swap or a kill between publication steps, other builds, other profiles and fixture sets, unexercised behavior, commercial software.
```

## SAVE-02 and SAVE-04: Save device refusals and postconditions

Counting unit: registered CTest names. Counted (2): `unit.core.save.device` (selection,
EEPROM commands, blocks, busy window and timing, SRAM window and DMA, refusals, reset to
blank, the digest section) and `unit.core.save.import-export` (the import and export
interface: ordered refusal rows, each proven to reach its row, with the digest, device
bytes, deadline, scheduler queue and tick compared before and after each refused request, a
sweep, a seeded campaign and the transform vectors). Both are listed by the
`save-device` and `save-transfer` suites of the case inventory. Unselected, unknown and
deferred requests are refused with the stated postconditions; a refused import leaves the
canonical digest unchanged. Supporting tests, not counted: `controls.real.save-wrong-byte`
and `runner.save-roundtrip`. The three save profile ids these suites also run are not claimed
here; this block binds the stem id.

```t64-claim
build_id: 0.0.0+t64-build-v1-b58d1ab6cd27f97c5ec87d2cb1ae90ce7ae2b877be61830c643342d6bbe2de22
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-si-v1
tier: unit-tested
fixture_set: phase09-save-refusals
manifest_digest: sha256:b75864fdf4404a4691bfed12ccaceaca22879e60ea739ef3cf89ca1eee6e80e0
pass: 2
total: 2
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: authored-literal-expectations
oracle_revision: phase09-cases-sha256:3f490463f872e1c72d7ac61cbe260be4fbd3cb7f39f3f1d68c1e4d2d5eb49bdf
not_claimed: FlashRAM, save-chip timing, title-based detection, battery or wear behavior, the six millisecond ceiling or the block mirror as measured hardware facts, third-party save formats, the three save profile ids exercised by the same suites, other builds, other profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+t64-build-v1-b58d1ab6cd27f97c5ec87d2cb1ae90ce7ae2b877be61830c643342d6bbe2de22 on profile ntsc-cart-4m-openipl3-rdram-b-t1-si-v1: unit-tested for phase09-save-refusals @ sha256:b75864fdf4404a4691bfed12ccaceaca22879e60ea739ef3cf89ca1eee6e80e0 (2/2; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: authored-literal-expectations phase09-cases-sha256:3f490463f872e1c72d7ac61cbe260be4fbd3cb7f39f3f1d68c1e4d2d5eb49bdf. Not claimed: FlashRAM, save-chip timing, title-based detection, battery or wear behavior, the six millisecond ceiling or the block mirror as measured hardware facts, third-party save formats, the three save profile ids exercised by the same suites, other builds, other profiles and fixture sets, unexercised behavior, commercial software.
```

## CORE-02: Guest-time scheduler ordering

Counting unit: registered CTest names. Counted (4): `unit.core.sched.matrix` (the tie rows,
the cross-device, partition, interleave and resource-limit cases, the producers of the
interrupt controller, the count wrap and the quantum binding), `unit.core.sched.ledger` (the
closed event ledger), `unit.core.sched.characterize-fifo` (the same-tick first-in first-out
pins) and `unit.core.profile.quantum-identity` (the quantum is part of the profile identity
and fixed at one). All four are listed by suites of the case inventory. The order is
produced by one guest-time scheduler under authored coincident, partition and interleave
cases and is compared across schedules against literal dispatch orders. Because the order is
also pinned as regression behavior, a consistently wrong tie would still pass. Supporting
test, not counted: `repo.sched-k5-anchors` (the anchors of the out-of-tree mutant driver; the
name is registered but no suite of the case inventory lists it).

```t64-claim
build_id: 0.0.0+t64-build-v1-b58d1ab6cd27f97c5ec87d2cb1ae90ce7ae2b877be61830c643342d6bbe2de22
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-si-v1
tier: unit-tested
fixture_set: phase09-scheduler-order
manifest_digest: sha256:b75864fdf4404a4691bfed12ccaceaca22879e60ea739ef3cf89ca1eee6e80e0
pass: 4
total: 4
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: authored-literal-expectations
oracle_revision: phase09-cases-sha256:3f490463f872e1c72d7ac61cbe260be4fbd3cb7f39f3f1d68c1e4d2d5eb49bdf
not_claimed: hardware ordering, cycle-level timing, any other quantum, a consistently wrong tie caught by the regression pins, other builds, other profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+t64-build-v1-b58d1ab6cd27f97c5ec87d2cb1ae90ce7ae2b877be61830c643342d6bbe2de22 on profile ntsc-cart-4m-openipl3-rdram-b-t1-si-v1: unit-tested for phase09-scheduler-order @ sha256:b75864fdf4404a4691bfed12ccaceaca22879e60ea739ef3cf89ca1eee6e80e0 (4/4; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: authored-literal-expectations phase09-cases-sha256:3f490463f872e1c72d7ac61cbe260be4fbd3cb7f39f3f1d68c1e4d2d5eb49bdf. Not claimed: hardware ordering, cycle-level timing, any other quantum, a consistently wrong tie caught by the regression pins, other builds, other profiles and fixture sets, unexercised behavior, commercial software.
```

## CORE-04: Machine state export and import

Counting unit: registered CTest names. Counted (7): `unit.core.digest.sink-legacy-identity`,
`unit.core.digest.v25-reference` and `unit.core.digest.v25-strip-differential` (the digest
framing and its independent reference encoder), `unit.core.state.export` and
`runner.state-export` (the container exporter in process and through the real runner, read back by
independent decoders), `unit.core.state.import`
(the validate-then-apply importer, its refusal rows, mutants, boundary matrix and campaigns)
and `harness.state-field-use-audit` (the audit that every behavior-bearing field has a
reader rule). Exported state re-imports and reproduces the canonical digest and the
continuation digests for authored machines on the same build. The composite state of the four
family ids and the authored fixtures carries the RDP combiner block and its memory coverage
table; the Phase 8 descriptor 11 profile keeps its recorded limitation. The boundary matrix has
17 rows, B01 to B17. Rows B01, B07, B10 and B14 run every partition size; the other thirteen
rows are sampled (a seven-tick peer against a one-shot source) in the gate, hosted and
sanitizer runs; the full set runs only in the native step `normal-state-import-full-matrix`
of the exact-SHA native qualification, whose step records are unsealed originals. The
seeded triangle campaign at the gate default of 12 iterations reaches refusal reason 40 only;
the importer's field readers are reached by the registered sanitizer counts and by the named
mutant rows, not by the gate-default run.

```t64-claim
build_id: 0.0.0+t64-build-v1-b58d1ab6cd27f97c5ec87d2cb1ae90ce7ae2b877be61830c643342d6bbe2de22
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-si-v1
tier: unit-tested
fixture_set: phase09-state-roundtrip
manifest_digest: sha256:b75864fdf4404a4691bfed12ccaceaca22879e60ea739ef3cf89ca1eee6e80e0
pass: 7
total: 7
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: authored-reference-decoder-and-literals
oracle_revision: state-decoder-sha256:e02a755112851f0ef137573d50562c0e17a0dd538c1335b7bb5bc8b1376011c8
not_claimed: cross-build or cross-version import, a durable or stable state format, states of other emulators, a runner pass for a resumed run, full-partition coverage of the sampled matrix rows outside the native step, importer field-reader reach from the gate-default run, protection of published states against a same-user swap, other builds, other profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+t64-build-v1-b58d1ab6cd27f97c5ec87d2cb1ae90ce7ae2b877be61830c643342d6bbe2de22 on profile ntsc-cart-4m-openipl3-rdram-b-t1-si-v1: unit-tested for phase09-state-roundtrip @ sha256:b75864fdf4404a4691bfed12ccaceaca22879e60ea739ef3cf89ca1eee6e80e0 (7/7; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: authored-reference-decoder-and-literals state-decoder-sha256:e02a755112851f0ef137573d50562c0e17a0dd538c1335b7bb5bc8b1376011c8. Not claimed: cross-build or cross-version import, a durable or stable state format, states of other emulators, a runner pass for a resumed run, full-partition coverage of the sampled matrix rows outside the native step, importer field-reader reach from the gate-default run, protection of published states against a same-user swap, other builds, other profiles and fixture sets, unexercised behavior, commercial software.
```

## CORE-05: Input replay checkpoints

Counting unit: registered CTest names. Counted (1): `runner.replay-mix`, the real supervised
record and play cases of the authored replay-mix ROM (a recorded run played back to its
checkpoint digests, three identity mismatches refused, a diverged play ended through the
harness-error record, and the flag combinations). The original run comes from the input
script `fixtures/replay/mix.input.json`, written independently of the core, and from the
transcript literal of `fixtures/replay/mix_oracle.py`; the golden header
`runner/tests/replay_mix_golden.h` holds operation and checkpoint digests only, for the
scripted pre-queue and the just-in-time submission modes, so the goldens do not depend on
the build. The tier is `regression-pinned`: a change to a digest is caught, and a value that
was always wrong would not be.

```t64-claim
build_id: 0.0.0+t64-build-v1-b58d1ab6cd27f97c5ec87d2cb1ae90ce7ae2b877be61830c643342d6bbe2de22
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-si-v1
tier: regression-pinned
fixture_set: phase09-replay-mix-goldens
manifest_digest: sha256:b75864fdf4404a4691bfed12ccaceaca22879e60ea739ef3cf89ca1eee6e80e0
pass: 1
total: 1
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: independently-authored-input-script-and-golden-digests
oracle_revision: replay-golden-sha256:3004debb649da008a662f84f5657a42962c6a020e19ab12098d0529e3443d490
not_claimed: portability across builds, platforms or profiles, equality across submission schedules, protection of replay reports against a same-user swap, other builds, other profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+t64-build-v1-b58d1ab6cd27f97c5ec87d2cb1ae90ce7ae2b877be61830c643342d6bbe2de22 on profile ntsc-cart-4m-openipl3-rdram-b-t1-si-v1: regression-pinned for phase09-replay-mix-goldens @ sha256:b75864fdf4404a4691bfed12ccaceaca22879e60ea739ef3cf89ca1eee6e80e0 (1/1; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: independently-authored-input-script-and-golden-digests replay-golden-sha256:3004debb649da008a662f84f5657a42962c6a020e19ab12098d0529e3443d490. Not claimed: portability across builds, platforms or profiles, equality across submission schedules, protection of replay reports against a same-user swap, other builds, other profiles and fixture sets, unexercised behavior, commercial software.
```

## CORE-05: Replay format and refusals

Counting unit: registered CTest names. Counted (1): `unit.replay.suite`, the in-process cases
of the replay module: the closed-key parser under explicit limits, the canonical writer whose
bytes equal the identity literals derived separately in a script, the recorder over the real
accepted call, the mutation matrix of malformed records, the report record, the three-family
binding, partition and the effect of the `at` field, and the queue-full divergence. The
`save_init` image form is verified in process only; the real runner refuses replay together
with the save flags, so a supervised run records the blank form only.

```t64-claim
build_id: 0.0.0+t64-build-v1-b58d1ab6cd27f97c5ec87d2cb1ae90ce7ae2b877be61830c643342d6bbe2de22
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-si-v1
tier: unit-tested
fixture_set: phase09-replay-format
manifest_digest: sha256:b75864fdf4404a4691bfed12ccaceaca22879e60ea739ef3cf89ca1eee6e80e0
pass: 1
total: 1
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: authored-literal-expectations
oracle_revision: phase09-cases-sha256:3f490463f872e1c72d7ac61cbe260be4fbd3cb7f39f3f1d68c1e4d2d5eb49bdf
not_claimed: the save_init image form in a supervised run (verified in process only, the real runner exercises only the blank form), portability across builds, platforms or profiles, other builds, other profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+t64-build-v1-b58d1ab6cd27f97c5ec87d2cb1ae90ce7ae2b877be61830c643342d6bbe2de22 on profile ntsc-cart-4m-openipl3-rdram-b-t1-si-v1: unit-tested for phase09-replay-format @ sha256:b75864fdf4404a4691bfed12ccaceaca22879e60ea739ef3cf89ca1eee6e80e0 (1/1; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: authored-literal-expectations phase09-cases-sha256:3f490463f872e1c72d7ac61cbe260be4fbd3cb7f39f3f1d68c1e4d2d5eb49bdf. Not claimed: the save_init image form in a supervised run (verified in process only, the real runner exercises only the blank form), portability across builds, platforms or profiles, other builds, other profiles and fixture sets, unexercised behavior, commercial software.
```
