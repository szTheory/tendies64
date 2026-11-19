# Phase 9 claim skeleton

Status: this file is a skeleton. It carries no results, no counts and no pins.
Real claim blocks, counts and exact identities arrive with the final qualification
plan of Phase 9 (09-11); until then every block below is a documentation example
with placeholder values, fenced so the repository claim check does not treat it
as a claim. Nothing here says that any Phase 9 behavior works.

Each family below restates, in authored words, the evidence tier it may use and
what it never covers. The same boundary applies to every family: other builds,
other profiles and fixture sets, behavior no fixture exercises, and commercial
software are outside every future claim.

Policy labels: every tie rule and every timing figure in the Phase 9 specs is a
named software-profile policy of this project, not a measured hardware fact.
Community facts that inform a policy are restated with their source URL and fetch
date in the specs under `docs/spec/`, never copied here as evidence. Hosted
evidence compares platform-invariant outputs and digests only. The identity
record the families refer to is in the [machine contract](../spec/machine-contract.md).

Wording rule: the three words reserved by the [template](TEMPLATE.md) stay out of
this file entirely, and no prose line starts with the project name followed by a
space.

## SAVE-01: Controller reads through the modeled serial interface

Tier when real: `unit-tested`.

Future claim: an authored ROM reads the scheduled button and axis values of the standard controller on port 0 through the modeled serial interface and its joybus wire policy, at the scheduled guest ticks of the model's named timing policy; Count readings are model time.

Not claimed: other ports or peripherals, physical controllers, joybus timing against hardware, rumble, the Controller Pak, other builds, profiles and fixture sets, unexercised behavior, commercial software.

```t64-claim-example
build_id: 0.0.0+placeholder
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-si-v1
tier: unit-tested
fixture_set: si-joybus-controller-read
manifest_digest: sha256:0000000000000000000000000000000000000000000000000000000000000000
pass: 0
total: 0
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: authored-placeholder
oracle_revision: placeholder-0
not_claimed: other ports or peripherals, physical controllers, joybus timing against hardware, rumble, the Controller Pak, other builds, profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+placeholder on profile ntsc-cart-4m-openipl3-rdram-b-t1-si-v1: unit-tested for si-joybus-controller-read @ sha256:0000000000000000000000000000000000000000000000000000000000000000 (0/0; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: authored-placeholder placeholder-0. Not claimed: other ports or peripherals, physical controllers, joybus timing against hardware, rumble, the Controller Pak, other builds, profiles and fixture sets, unexercised behavior, commercial software.
```

## SAVE-02/04: Explicit save media selection

Tier when real: `unit-tested`.

Future claim: explicitly selected EEPROM 4 and 16 Kbit and SRAM 32 KiB follow the authored block, busy-window and refusal contracts; unselected, unknown and deferred requests are refused with the stated postconditions (a fixture that reports its own verdict may be recorded under the self-checking tier instead).

Not claimed: FlashRAM, save-chip timing, title-based detection, battery or wear behavior, the six millisecond ceiling or the mirror as measured hardware facts, other builds, profiles and fixture sets, unexercised behavior, commercial software.

```t64-claim-example
build_id: 0.0.0+placeholder
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e4k
tier: unit-tested
fixture_set: save-media-eeprom-sram
manifest_digest: sha256:0000000000000000000000000000000000000000000000000000000000000000
pass: 0
total: 0
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: authored-placeholder
oracle_revision: placeholder-0
not_claimed: FlashRAM, save-chip timing, title-based detection, battery or wear behavior, the six millisecond ceiling or the mirror as measured hardware facts, other builds, profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+placeholder on profile ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e4k: unit-tested for save-media-eeprom-sram @ sha256:0000000000000000000000000000000000000000000000000000000000000000 (0/0; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: authored-placeholder placeholder-0. Not claimed: FlashRAM, save-chip timing, title-based detection, battery or wear behavior, the six millisecond ceiling or the mirror as measured hardware facts, other builds, profiles and fixture sets, unexercised behavior, commercial software.
```

## SAVE-03: Save export and import

Tier when real: `unit-tested`.

Future claim: exported save bytes round-trip with the named transforms; a refused import leaves the canonical digest unchanged.

Not claimed: third-party save formats, other builds, profiles and fixture sets, unexercised behavior, commercial software.

```t64-claim-example
build_id: 0.0.0+placeholder
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-s32k
tier: unit-tested
fixture_set: save-export-import
manifest_digest: sha256:0000000000000000000000000000000000000000000000000000000000000000
pass: 0
total: 0
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: authored-placeholder
oracle_revision: placeholder-0
not_claimed: third-party save formats, other builds, profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+placeholder on profile ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-s32k: unit-tested for save-export-import @ sha256:0000000000000000000000000000000000000000000000000000000000000000 (0/0; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: authored-placeholder placeholder-0. Not claimed: third-party save formats, other builds, profiles and fixture sets, unexercised behavior, commercial software.
```

## CORE-02: Guest-time scheduler ordering

Tier when real: `unit-tested`.

Future claim: events are ordered by one guest-time scheduler under the authored coincident, partition and interleave cases; the order is pinned as regression behavior and compared across schedules, so a consistently wrong tie would still pass; the interrupt controller is ordered through its producers' events; the quantum is part of the profile identity and fixed at one.

Not claimed: hardware ordering, cycle-level timing, any other quantum, other builds, profiles and fixture sets, unexercised behavior, commercial software.

```t64-claim-example
build_id: 0.0.0+placeholder
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-si-v1
tier: unit-tested
fixture_set: scheduler-ordering-matrix
manifest_digest: sha256:0000000000000000000000000000000000000000000000000000000000000000
pass: 0
total: 0
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: authored-placeholder
oracle_revision: placeholder-0
not_claimed: hardware ordering, cycle-level timing, any other quantum, other builds, profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+placeholder on profile ntsc-cart-4m-openipl3-rdram-b-t1-si-v1: unit-tested for scheduler-ordering-matrix @ sha256:0000000000000000000000000000000000000000000000000000000000000000 (0/0; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: authored-placeholder placeholder-0. Not claimed: hardware ordering, cycle-level timing, any other quantum, other builds, profiles and fixture sets, unexercised behavior, commercial software.
```

## CORE-04: Machine state export and import

Tier when real: `unit-tested`.

Future claim: exported machine state re-imports and reproduces the canonical digest and the continuation digests for authored machines on the same build.

Not claimed: cross-build or cross-version import, states of other emulators, other builds, profiles and fixture sets, unexercised behavior, commercial software.

```t64-claim-example
build_id: 0.0.0+placeholder
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e16k
tier: unit-tested
fixture_set: state-export-import
manifest_digest: sha256:0000000000000000000000000000000000000000000000000000000000000000
pass: 0
total: 0
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: authored-placeholder
oracle_revision: placeholder-0
not_claimed: cross-build or cross-version import, states of other emulators, other builds, profiles and fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+placeholder on profile ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e16k: unit-tested for state-export-import @ sha256:0000000000000000000000000000000000000000000000000000000000000000 (0/0; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: authored-placeholder placeholder-0. Not claimed: cross-build or cross-version import, states of other emulators, other builds, profiles and fixture sets, unexercised behavior, commercial software.
```

## CORE-05: Input replay

Tier when real: `regression-pinned`.

Future claim: a recorded input replay bound to profile, build and content hash reproduces the recorded checkpoint digests (regression-pinned), with format and refusals unit-tested; the original run comes from an independently authored script and the goldens are build-independent.

Not claimed: portability across builds, platforms or profiles, equality across submission schedules, other fixture sets, unexercised behavior, commercial software.

```t64-claim-example
build_id: 0.0.0+placeholder
profile_id: ntsc-cart-4m-openipl3-rdram-b-t1-si-v1
tier: regression-pinned
fixture_set: input-replay
manifest_digest: sha256:0000000000000000000000000000000000000000000000000000000000000000
pass: 0
total: 0
guest_fail: 0
boot_fail: 0
timeout: 0
missing: 0
harness_error: 0
skipped: 0
oracle_kind: authored-placeholder
oracle_revision: placeholder-0
not_claimed: portability across builds, platforms or profiles, equality across submission schedules, other fixture sets, unexercised behavior, commercial software
statement: Tendies64 0.0.0+placeholder on profile ntsc-cart-4m-openipl3-rdram-b-t1-si-v1: regression-pinned for input-replay @ sha256:0000000000000000000000000000000000000000000000000000000000000000 (0/0; missing 0, skipped 0, timeout 0, harness-error 0, guest-fail 0, boot-fail 0). Oracle: authored-placeholder placeholder-0. Not claimed: portability across builds, platforms or profiles, equality across submission schedules, other fixture sets, unexercised behavior, commercial software.
```
