# Phase 05 authored RSP smoke evidence

## Tested artifact and provenance

This is local software evidence for the authored CPU→SP DMA→RSP→CPU path in
profile `ntsc-cart-4m-openipl3-rsp-functional-v1` (machine model 6, state
serialization 12, SP model
`rsp-functional-issue-dma-divider-integer-v1`). The tests use generated,
rights-cleared local fixtures and fresh machines. No private diagnostic tree,
protected ROM, or hardware measurement is part of this evidence.

The authored fixture source is pinned to commit
`3b3e10c786f46002bc6a65b5193cf01633303828`, which contains both fixture inputs.
The builder checks that the working bytes equal those committed inputs before
assembly. The source and build recipes are:

| Input | Identity |
| --- | --- |
| `fixtures/rsp/rsp-smoke.S` | SHA-256 `b773aee4a3fba8e4ef1710658d26609ffbd9d70b7692528af56d0d71d6f92a83` |
| `fixtures/rsp/rsp-smoke.ld` | SHA-256 `af860586790a1dcec95f548d07b1abbb535c864d53eef955105a91f7fa6620bf` |
| `scripts/build-rsp-smoke.sh` | SHA-256 `d0bfbb1aa9eed17d622ad0a393ad451365963c07ff8a78400b2e8579842afa27` |
| `scripts/test-rsp-smoke.sh` | SHA-256 `76ed5951d0cfa1d503d6c87810e384bcbf7e898e7d891fd0209dd0e01221af98` |
| Runner and test implementation | Commits `a9b36742c83276f55cb94a6dfa3ebfcef55006de` and `59559143aacd6c6a7cd6abe251f9aa7d228814dc`; `runner/t64_rsp_smoke.c` SHA-256 `b1c3e12ec610d86899424c59c29529e047bc7a16eec5c558248dc8179f83f0ed`; `tests/test_rsp_smoke.py` SHA-256 `13a33d952eb5e5f50019efd26cda5478aeb10a5deb47ad9d2ab272880e32d7d3` |
| Fixture/source documentation | Commit `1d4c585` (`feat(05-09): build authored RSP smoke fixtures`) |

The cross-assembler toolchain is GNU Binutils 2.45. Each executable is checked
against the qualified local tool receipt before use:

| Tool | SHA-256 |
| --- | --- |
| Assembler | `a57235c0f2986576ce3d1068ec70d03c74f6a84ae5b5abe606780b4f00a8f684` |
| Linker | `aa8c8ddc6fd3a2579fa5bb41719eda0e70ff4be3275d481e2814b72d701213e2` |
| Objcopy | `da223203af0487c6ea655a9693d4f4c15c5219189b7f52c378525d7440791da9` |
| Objdump | `be48cea3c0407ab9dc65e4458f3d8f4ffab018a97a7d66de41ecdde8770626d5` |

The host build used Apple Clang 21.0.0 (`clang-2100.1.1.101`). The ROM packer
is the existing project tool built from `tools/fixture/t64_rom_pack.c`
(SHA-256 `d91119ded68875c08342a088050c820ab47d885ea9f05b8e835de8fe0a6c2a93`)
and `tools/common/t64_sha256.c` (SHA-256
`de587155f2c10e3a5c8cc9814f9c14cbe485bc20554ea725bbe73b914e053940`). The
builder runs the packer's selftest and verifies the completed image composition
before any test consumer opens a ROM.

The boot asset is the generated `ipl3_compat.z64` from
[n64brew/libdragon commit `98154dafd5ac50e74ee09a5de1035d24a53f54f0`](https://github.com/n64brew/libdragon/tree/98154dafd5ac50e74ee09a5de1035d24a53f54f0).
Its source asset is covered by the upstream Unlicense. The checked 4096-byte
asset SHA-256 is
`f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068`; the
exact embedded 4032-byte payload (source offset `0x40`) SHA-256 is
`f716a90b1d36ec4253d096790791256d3543455e54473a755ff8c3d1199b15d5`. The
source compiler and signer for that upstream generated asset are unknown; no
source rebuild claim is made.

All six generated ROMs are 5056 bytes and remain ignored, local-only build
outputs. The exact image digests and required outcomes are:

| Fixture | ROM SHA-256 | Expected authenticated result |
| --- | --- | --- |
| Baseline | `5611f3b5c17c2d05408728fa2c9dfaedd193d3d6da83355be4c5ee61771651a0` | Pass transcript and exact terminal retirements |
| Variant 1: wrong result | `75b7e33b47d4587cd164209a5c3a65c9c77340edf4ac303995ce07288f5d2f5d` | CPU-visible expected-byte failure |
| Variant 2: unsupported opcode | `746da9236eb9b5f5814ce551fe8c261069cda8332bfc259a8db0d135a04fb895` | Fetched opcode refusal at RSP PC `0x01c` |
| Variant 3: missing BREAK | `bed368d793c42f7f37494ef5be2ec29b4e968ff5e738d5289228890954591715` | Missing-BREAK classification at PC `0x09c` |
| Variant 4: altered delay | `26f23580fdacea86270b1967c363f40c1d486a0350a9efff33877975ab0ffc58` | Terminal CPU delay-slot mismatch |
| Variant 5: same-bank wrap | `bef1d150d9fbec4d0ac886b19945d034fdd67b105fa6956bd1d346ce8e6b41d1` | CPU-visible expected-byte failure |

The project-authored sources are Apache-2.0 under the repository license. The
ROMs contain only those sources plus the separately identified Unlicense
payload. The manifest records the fixture digests, rights, recipe, boot
identity, profile, and expected provenance. Binaries and test output are not
committed or uploaded.

## Normal and sanitizer checks

Both configurations completed an all-target configure and build before their
focused CTests. The normal test inventory contained 127 tests; the sanitizer
inventory contained 129. The owning integration is registered exactly as
`integration.rsp-rom-smoke` in both configurations and runs serially.

| Configuration | Result | Local log |
| --- | --- | --- |
| Normal build and owner test | Configure/build passed; owner CTest passed in 6.37s. | `build/gate/05-09-load-use-gate.log`, `build/gate/05-09-load-use-owning.log` |
| ASan/UBSan build and owner test | Configure/build passed; owner CTest passed in 18.15s with `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. | `build/asan/05-09-load-use-gate.log`, `build/asan/05-09-load-use-owning.log` |
| Inventory controls | Exact counts 127 normal / 129 ASan; 256 removals and five malformed-input controls passed. | `build/gate/05-09-inventory.log`, `build/asan/05-09-inventory.log`, `build/gate/05-09-inventory-selftest.log` |
| SP/output route regressions | `unit.core.isviewer`, `unit.core.warm-device-policy`, and `unit.core.bus` passed in both configurations. | `build/gate/05-09-route-regressions.log`, `build/asan/05-09-route-regressions.log` |
| Manifest contract | `gov.manifest.repo` passed after all six fixture entries were authored. | `build/gate/05-09-step16-manifest-ctest.log` |

The focused commands were:

```sh
cmake --preset gate
cmake --build --preset gate --parallel 4
ctest --preset gate --no-tests=error -R '^integration\.rsp-rom-smoke$'

cmake --preset asan
cmake --build --preset asan --parallel 4
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --preset asan --no-tests=error -R '^integration\.rsp-rom-smoke$'
```

The owner test verifies each receipt and manifest digest, begins with the
baseline, and runs every negative ROM in a fresh machine. It also runs the same
baseline with boot advance partitions of at most 16 ticks and at most 1 tick.
The test compares every reported semantic field except the selected partition:
the exact 19-byte transcript, CPU and RSP retirement counts, RSP branch and
delay, SU/VU pair, BREAK, stall count, DMEM windows, VCO, load-use/input words,
VSAR slices, divider words, SP status/semaphore, terminal CPU branch/delay,
and final guest tick. The runner prints these values; the test compares them
field-for-field across the two schedules. It also requires the scalar-ready
stall at fetched RSP PC `0x044`, where ADDU
consumes the immediately preceding LW result; a different vector stall cannot
substitute for this load-use observation.

The formal Task 2 RED record was a real guest refusal at CPU PC `0x800004b4`
while writing SP status (`0xae880010`) on the model-6/state-12 profile. The
profile-specific production route fix resolved that failure and is covered by
the focused bus, warm-device, and IsViewer regressions above. Earlier malformed
digest/setup attempts were discarded and are not counted as behavioral RED.

The successful baseline reports RSP BREAK PC `0x09c`, 40 RSP retirements, 38
stall slots (including the scalar-ready load-use stall at PC `0x044`), SP
status `0x00000003`, semaphore set, and guest tick `531446`.
The terminal CPU branch is fetched and retired as `0x1000ffff` at
`0x800006c0`; its fetched delay instruction is NOP at `0x800006c4`. The
transcript bytes are
`5436342d5253502d534d4f4b453a504153530a` (`T64-RSP-SMOKE:PASS\n`). CPU checks
the 32-byte DMEM result window
`8899aabb7fffeeff00000000000000000000000000000000008899aabb7fffee`, VCO
`0x00000002`, load-use and input word `0x11223344`, VSAR outputs
`0x0000/0x0001/0x7ffe`, and divider outputs `0x0000/0xffff` before writing
that transcript. The runner prints and compares these semantic values across
the two advance partitions.

The wrong-result negative has a retained concrete result in
`build/asan/05-09-step16-variant1-guest.log`: its CPU follows the guest failure
path with actual byte `0x80` where `0x7f` is expected, after 40 RSP
retirements. The remaining negative tests assert their distinct fetched
refusal, missing-BREAK, delay-slot-mismatch, and same-bank CPU-failure
classifications. These checks reject setup errors and incomplete trace
evidence; an output line or BREAK alone is insufficient.

Every individual guest remains bounded to 10,000,000 guest ticks, maximum
advance 16, a 10-second runner deadline, a 12-second outer subprocess deadline,
and 4096 captured bytes. The six-case CTest batch is serial with a separate
120-second aggregate timeout to include fixture build and validation. An
earlier aggregate 12-second CTest attempt expired at the suite level; no guest
classification was retained from that attempt, so it is not counted as a
behavioral result. The
validation policy assigns 12 seconds to each guest subprocess, and the final
test registration preserves that per-case limit while bounding the batch.

## Evidence scope

The hand-derived expectations use the *Nintendo Ultra64 RSP Programmer's
Guide*, Version 1.1, public PDF and pages recorded in
[rsp-smoke.md](../spec/rsp-smoke.md), plus integer arithmetic. The profile's
divider zero-input behavior and the documented VADDC overview/Appendix
conflict are named software policies. This evidence proves only deterministic
execution and checking in the stated Tendies64 software profile. It is not a
silicon proof, full-ROM compatibility result, timing-accuracy result, or
hardware equivalence claim.
