# Open IPL3 warm-entry limits

This document records the qualified software profile and the exact boundary
reached by the pinned IPL3 and authored hello fixture, including the captured
guest transcript. It does not claim an authentic IPL2 handoff, cold boot, full
RSP, public output-copy ABI, broad compatibility, or hardware accuracy.

## Input admission and prepared profile

The profile `ntsc-cart-4m-openipl3-warm-v1` keeps three identities distinct:
the pinned 4096-byte libdragon asset, its 4032-byte executable payload, and an
authored cartridge containing that payload. The pinned asset comes from
libdragon commit `98154dafd5ac50e74ee09a5de1035d24a53f54f0`, path
`boot/bin/ipl3_compat.z64`; the build-only rights record is in
`fixtures/boot-contract.json`. The asset SHA-256 is
`f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068`. Bytes
`[0x40, 0x1000)` have SHA-256
`f716a90b1d36ec4253d096790791256d3543455e54473a755ff8c3d1199b15d5` and are
copied to SP DMEM offset `0x40`, entry PC `0xA4000040`. The 64-byte cartridge
header is not copied; the final eight payload bytes are linker-reserved and
are not claimed as executed code.

Admission accepts the exact pinned 4096-byte source asset or an authored
big-endian z64 cartridge that passes all of these checks:

- The full 4032-byte pinned payload identity matches at offset `0x40`.
- The image is larger than the flat payload offset `0x1000`, is word-aligned,
  and is no larger than 1 MiB.
- Header entry at `0x08` is aligned KSEG0, and header load size at `0x10` is
  nonzero and equals the complete admitted image size after `0x1000`.
- The entry plus load size lies inside the installed 4 MiB RDRAM.

Failure is transactional: a rejected candidate leaves the admitted image and
canonical state unchanged. This is a bounded authored-fixture qualification,
not general cartridge compatibility. The existing 1 MiB ceiling and the exact
embedded payload check prevent loading bytes from beyond the admitted image.

The selected profile is NTSC/US cartridge media, 4 MiB RDRAM, no expansion,
peripheral, or save device, with prepared warm seed version 1 (`s3=0`, `s4=1`,
`s5=1`, `s6=0`, `s7=1`). Other GPRs, HI/LO and CP0 fields start at zero.
These are deterministic software inputs, not measured NUS-CPU-03 reset values.
In particular, `MI_VERSION=0` is a project-selected value that takes the
non-iQue branch in this pinned IPL3, and `RI_SELECT=1` is a project-selected
nonzero value that enters the prepared warm path. Neither is a hardware
register observation or silicon calibration.

The absent-memory rule covers exactly one four-byte RDRAM word at
`0x00400000`: reads return zero and writes are no-ops. Adjacent addresses are
refused. This profile rule is not an open-bus model and does not make arbitrary
unmapped reads return zero.

## Warm-device policies and evidence labels

Hardware-document facts and software policy are separated in the active policy
input and digest in `fixtures/boot-contract.json`. Nintendo SDK `rcp.h` documents
the SP status command bits (including `0x00aaaa0e`), the read-only MI_INTR
register, and the MI/AI/SI interrupt-clear commands. The retrieved header is
SHA-256
`982f5ae01956783653d1085ef31c3d936f471f09bd1e65e27a578fae498f972f`
([document](https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm));
the exact locator is recorded in `docs/spec/machine-contract.md` and the phase
evidence records. The command effects listed below are source-backed where
stated; their use in this prepared profile is still a software compatibility
choice, not evidence of reset-time silicon state.

- SP_STATUS is modeled as permanently HALTed for this profile: HALT is one;
  BROKE, SSTEP, INTR_BREAK and SIG0–7 remain zero. Accept zero/no-op and the
  exact set-HALT, clear-BROKE/INTR/SIG0–7 command `0x00aaaa0e`; refuse commands
  that would change the fixed status. Clearing SP's MI interrupt uses the
  existing serialized MI pending state. A status write does not cancel an
  active or queued DMA. There is no RSP instruction engine.
- The SDK identifies MI_INTR as read-only. This profile accepts only one
  compatibility exception: exact width-4 value `0x555` is ignored, leaving
  MI pending/mask and DMA state unchanged. Other writes and adjacent read-only
  registers are refused. Ignoring this exact command is software policy, not a
  documented hardware write effect.
- MI_INIT_MODE accepts its documented width-4 `0x800` clear-DP-interrupt
  command. AI_STATUS and SI_STATUS accept only the width-4 zero command that
  clears their respective pending interrupt. Other values are refused.
- The zero-source SP DMA qualification covers only DRAM-to-SP, source
  `0x00802000`, raw length `0x0fff`, one 4096-byte row, no skip, and either
  full IMEM at SP address `0x1000` or full DMEM at address zero. The destination
  bank alone is zero-filled at the scheduled completion tick. The initial
  IMEM-only assumption was contradicted by the pinned loader's DMEM request;
  the two exact tuples are now separately tested. Other sources, directions,
  offsets, sizes and profiles remain refused before mutation.
- CACHE operations `0x08`, `0x09`, `0x11` and `0x15` use the named
  coherent-functional warm policy. Instruction fetch and data operations use
  shared backing bytes, so these operations record their operation and address
  without a cache-line writeback or host/RDRAM access. The dynamically reached
  CACHE `0x11` address was virtual `0x807f8ff0`, translated to physical
  `0x007f8ff0`, beyond installed RDRAM. This remains valid because the
  functional operation does not dereference that address. No per-line timing
  or general cache fidelity is modeled; unsupported operations and address
  modes still stop.

PI and SP issue/completion deadlines and atomic visibility are deterministic
software scheduler policy. They do not claim hardware transfer timing. Queue,
token, cancellation, bounds, row and bank rules remain checked independently.

## Independently derived identities

The policy digest is SHA-256 over the exact UTF-8 `policy_digest_input` string
in `fixtures/boot-contract.json`, including its final LF:
`4e3cf89c61b325ed76d950caa7752b2b8fcbca6a01a08dc202cc5271ace98a2b`. It binds
the CPU's exact non-retiring software debug-stop convention for BREAK
`0x0000000d` outside delay slots, along with cache and warm-device policies
and the libdragon ISViewer software contract: writable magic readback,
512-byte big-endian buffer, count-register lanes, exact unpadded flush, reset
state, sticky 4096-byte retained prefix, and an exact u64 total of requested
flush bytes including refused attempts. On u64 saturation the API reports the
maximum with an explicit inexact flag; the real runner fails closed instead of
writing that saturated value as an exact result total.

The active profile descriptor v2 digest is
`d8c12cb2a00410a2153ca62da318166f3ca029dea8e3d6b9c841262cb8203c71`; the
admitted-source initial state v8 digest is
`d2212ea39cadf8642cab2451fbcea484f5c6997d51619faef41e9ada24aa9724`. The
test-only reference encoder preserves prior policy, profile and state values
as independent historical vectors, then checks the new active values. State
v6 and v7 keep their previous meanings and bytes; the warm profile now selects
state v8. The v1 synthetic goldens remain unchanged.

Profile serialization uses ASCII domain `T64PDESC`, u32 serialization version
1, length-prefixed UTF-8 fields and big-endian u32 values in the order in
`core/t64_machine.c`; its v2 tail contains the asset, payload and policy hashes
plus seed-reset version. State v8 preserves the warm v6 CPU, boot, cache and
device fields, then includes the output policy/readiness, magic register,
pending byte count and full 512-byte buffer, retained length and full 4096-byte
capture, sticky loss flag, total attempted flush bytes, and total exactness
flag in fixed order. State v7 remains verifiable with its previous output
fields. Independent tests mutate every future-affecting output field, check
the independent initial-state vector, and exercise saturation. Allocator
addresses, unused queue storage, host time and trace data remain outside the
digest.

## Observed loader and authored-entry path

The preserved 03-01 qualification snapshot remains unchanged in
`fixtures/boot-contract.json`. Current observations are recorded separately.
The live fixture smoke reaches tick 100 with 100 retired instructions, next PC
`0xA4000354`, last PC `0xA4000350`, and opcode `0x240AF000`; this is a bounded
smoke point, not an unsupported stop. Traces are drained in intervals no
longer than 16 guest ticks, with no dropped records or saturation.

The actual pinned loader issues SP token 1 at tick 81 for the exact IMEM
zero-source descriptor; it completes at tick 605. The path also issues PI
token 1 at tick 520733 and completes it at 520858 (DRAM `0x003fff08`, cartridge
`0x10000b08`, 248 bytes), then PI token 2 at tick 523608 and completes it at
523701 (DRAM `0x400`, cartridge `0x10001000`, 184 bytes). Token 2's copied bytes
match at completion. SP token 7 transfers the later IMEM image from DRAM
`0x003f8000`, issue tick 525537, completion tick 529645. SP token 8 is queued at
tick 525547 for exact zero-source DMEM; it completes at tick 530169 after token
7. Independent device tests check deadline visibility, both banks, cancellation,
queue refusal, reset behavior and per-instance ordering.

The authored app entry at `0x80000400` executes at tick/retirement 530174. Its
first output instruction is `sw t2,0(t0)` at PC `0x8000041c`, tick 530181, after
530180 retirements, with `t2=0xffffffffedcba987`. The authored fixture uses the
uncached KSEG1 alias `0xffffffffb3ff0000`, translating to physical
`0x13ff0000`; supported virtual-address modes are unchanged. The pinned loader
and authored hello then execute through magic readback and flush the exact
transcript `Tendies64 hello via open IPL3\n` (30 bytes). The exact BREAK
`0x0000000d` at tick 530438, PC `0x80000478`, now produces a non-retiring
software debug stop after 530437 retired instructions. This stop is not a
hardware exception or guest verdict: the runner separately requires the
manifest-bound fixture digest, PC and opcode before treating it as authored
completion. Other BREAK encodings and delay-slot BREAKs remain unsupported. A
separate authored control fills 4096 bytes and
observes sticky refusal of byte 4097. The bytes remain in per-machine internal
capture state; this slice adds no public output-copy ABI or host I/O callback.

## Remaining evidence limits

The Nintendo 64 Programming Manual v5.2 §6.10 and the NEC VR4300/VR4305/VR4310
manual support the aggregate IPL3 responsibilities and CPU/address/cache
context; their pinned hashes and locators are in the phase primary evidence
index. They do not establish original IPL2 register values, cold-start
behavior, PIF/CIC variants, or the prepared warm values. Local tests establish
only the stated deterministic software subset and authored fixture route.

## Verification record

The current local gate passed all 91 registered gate tests, the collector and
comparator selftests, rights/PII checks, and core lint with zero violations in
57 seconds. The full all-target ASan/UBSan build and exact 93-name inventory
passed; the parallel sanitizer suite passed 93/93 in 121.53 seconds with
address and undefined checks active and leak detection disabled for this
macOS host. The intentional sanitizer negative control detected its injected
fault. That full run preceded later test-only fixture setup checks; after those
checks, the all-target sanitizer build passed and the changed descriptor and
hello integration tests passed 2/2. The full 93-test sanitizer suite was not
repeated after the test-only changes.

The first hosted candidate was commit
`73e63e04ca2bc21ac5c7e80adbc6b6041cc55285`, run 43087470400 attempt 1. Linux
failed compilation because GCC treated four ignored `ftruncate` results in
test-only report mutation cases as errors. macOS completed its gate and
uploaded one sanitized record, but Linux uploaded none, so these records do
not establish a cross-host pair. The test-only file operations now check their
results. The previous sanitizer failure was an assertion-budget issue: a
1000 ms test-only wall cap killed before the delayed child could produce its
30-byte transcript. The cap is now 2500 ms while the child delay remains 3 s;
the test still verifies watchdog kill precedence, and production watchdog
semantics are unchanged. A new exact-SHA hosted run and paired comparison are
required before claiming the two-host result.

The next candidate, `d6adbec2c1a28c00b31786b55ca3b832658b6185`, also failed
Linux compilation in run 43091181251 attempt 1: ignored test-only `symlink`,
`ftruncate` and `write` results remained in the fixture descriptor and source
replacement setup. macOS passed but its artifact cannot form a pair without
the Linux record. Those fixture-creation and barrier-release results are now
checked, and a cached Ubuntu 24.04/GCC 13.3 x86_64 container syntax-checked the
three affected C translation units with the CI warning flags. This
compile-only check is not a complete Linux build or guest evidence; the next
hosted run remains authoritative.

The `d3ad1c8093fba2aa9b637d3cdaf74e244875346a` push reached a successful Linux
gate and evidence upload, but the macOS job's upload step was marked canceled
at the 10-minute job timeout. The macOS gate, emitter and sanitization steps
had passed. The run and job APIs remained stale at `in_progress`; normal and
force-cancel requests returned HTTP 409, and the job-log API returned HTTP 502.
The underlying upload cause is unknown. The retained Linux artifact is not
paired with another run and does not count as hosted proof. A live GitHub Status
API read at 2026-11-11T20:29:11.880Z reported Actions operational; this does not
explain this individual upload failure.

The collector now accepts verified older-attempt artifact metadata while
downloading and validating only the exact selected attempt's two records. The
new local gate passed 91/91 tests in 160 seconds, including a mocked valid
attempt-2 pair with a retained attempt-1 artifact and rejection of a current
artifact mislabeled as attempt 1. This collector-only compatibility change
does not alter guest behavior, CI job count, action pin or timeout. A fresh
normal push produced the required exact-run Linux/macOS pair: run 43103669084,
attempt 1, source `7c0ea9b5f3076122d506074aa7d3a2283cf9e2a6`. Both jobs passed
and the actual guest records agree on the named warm profile's fixture,
guest-stop bound, transcript and state digest. The two compiler build IDs
differ, as expected. This is smoke evidence for this authored fixture and
profile only; it does not claim authentic IPL2 state, cold boot, hardware
accuracy, or general ROM compatibility. See `docs/evidence/phase3-ci.md` for
the artifact IDs, API archive metadata, extracted JSON hashes, and measured
CI timings.
