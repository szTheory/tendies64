# Open IPL3 warm-entry limits

This document defines the current, byte-qualified software entry profile and
its evidence boundary. It does not claim a cold boot, a hardware IPL2 handoff,
or execution of the authored `hello` ROM.

## Exact input and software profile

The `ntsc-cart-4m-openipl3-warm-v1` profile accepts only the pinned 4096-byte
big-endian `ipl3_compat.z64` asset. Its source is libdragon commit
`98154dafd5ac50e74ee09a5de1035d24a53f54f0`, path
`boot/bin/ipl3_compat.z64`; the upstream source and generated binary rights
are recorded as cleared for build-only use in `fixtures/boot-contract.json`.
The asset SHA-256 is
`f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068` and its
MD5 is `71ce6a00936394ad364fbe584234fd82`. Only bytes `[0x40, 0x1000)` are
copied: the 4032-byte payload has SHA-256
`f716a90b1d36ec4253d096790791256d3543455e54473a755ff8c3d1199b15d5` and is
placed at SP DMEM offset `0x40`, corresponding to entry PC `0xA4000040`.
The 64-byte source header is not copied. The final eight payload bytes are a
linker-reserved signature region, not claimed as executed code.

These identities are deliberately separate. The 4096-byte upstream asset is
the exact core-admission input; its copied payload is 4032 bytes; the authored
`hello.z64` fixture has its own 184-byte header and is currently 4280 bytes.
This plan's test observes the exact 4096-byte admission and does not attempt to
admit or execute the authored ROM. Its build-only provenance and payload/ROM
hashes are independently checked by the fixture producer and repository
manifest gate.

The selected software profile records an unqualified `NUS-CPU-03` target,
NTSC, US region, cartridge media, 4 MiB RDRAM, no expansion, peripherals, or
save device, and a warm reset seed (reset version 1): `s3=0`, `s4=1`, `s5=1`,
`s6=0`, `s7=1`. Other GPRs, HI/LO, and CP0 fields begin at zero. It admits
exactly 4096 input bytes. This is an explicit descriptor policy, not evidence
that hardware or a real IPL2 supplied these registers.

Admission copies the bytes and records the asset, payload, and policy hashes
before scheduling the CPU. The actual first word, `0x3044D236`, is
`ANDI a0,v0,0xd236` at `0xA4000040`; it retires at guest tick 1. The following
`LUI` retires, and execution takes a BAL helper path that includes signed
32-bit multiply and HI/LO transfers. The profile then retires the reached
CP0 and CACHE instructions and stops at the first unsupported device read,
described below. This remains a partial warm-entry trace: it does not
establish output, a complete device set, authored-ROM execution, or a running
game.

The fixture contract preserves the 03-01 initial qualification snapshot from
2026-11-10, including its one-instruction coverage record and unsupported LUI
boundary. Its active policy identity now names the implemented subset:
`cpu_decode_policy=integer-control-load-store-cp0-inventory-subset-v2` and
`cache_policy=coherent-functional-warm-subset-v1`. This identity correction
updates the policy digest and therefore the profile and initial state hashes;
the asset, profile ID, descriptor version, state serialization version, and v1
synthetic identity remain unchanged. The new current execution observation is
recorded beside the historical 03-01 snapshot in `boot-contract.json`.

## Independent identity goldens

The test's descriptor and initial-state goldens are fixed literals derived
independently from the field orders below and the pinned fixture bytes, not
copied from an emulator-produced digest. The profile descriptor digest is
`d5a2b703c15ec8707e61a369530c524e6e88ef6d5548f0df2239a9a93a05c746`.
The initial state digest, taken after exact admission and before dispatch, is
`1c7e4086d89234c9d304e40f7e17468ddc8fcd04316904ddd7b5bc87913e96ea`.
The existing v1 synthetic profile golden remains separate and unchanged.

The version-2 profile descriptor uses profile serialization version 1 and is
SHA-256 over the following byte sequence:

1. ASCII domain `T64PDESC`, then u32 serialization version `1`.
2. Each text value as u32 byte length followed by those UTF-8 bytes, without a
   terminator, in this order: `profile_id`, `console_target`,
   `console_qualification`.
3. u32 values in this order: TV standard, region, media kind, RDRAM bytes,
   expansion bytes, peripheral flags, save kind.
4. Text values in this order: boot route, boot status, guest tick unit.
5. u32 values: CPU/RSP ratio numerator, ratio denominator, synchronization
   quantum.
6. Text values: timing tier, scheduler version, PI model version, SP model
   version.
7. u32 values: ROM minimum bytes, ROM maximum bytes, event queue capacity,
   sequence width, advance event budget, PI duration base ticks, PI duration
   bytes divisor, SP duration base RSP cycles, SP duration bytes divisor.
8. Text values: ROM order policy, event order policy, queue exhaustion
   policy, sequence exhaustion policy, advance-budget policy, guest-time
   bound policy, halt-precedence policy, PI register policy, SP register
   policy, device-error policy, DMA-cancel policy, DMA-visibility policy.
9. u32 descriptor version, machine model version, and state serialization
   version; then 32-byte asset SHA-256, 32-byte copied-payload SHA-256,
   32-byte boot-policy SHA-256; finally u32 seed-reset version.

All integers are unsigned big-endian. The boot-policy SHA-256 is over the
exact UTF-8 `policy_digest_input` in `fixtures/boot-contract.json`, including
its final LF; its current digest is
`5d6aacdc47cbb6fb3ad80e782935fdb330af559ec8e9143656ffedd5069cf984`. That
input explicitly binds source revision/path and asset/payload identities,
entry and DMEM locations, profile fields, seed values, CPU/event behavior,
unsupported/completion codes, and cache/device/output policy.

State serialization v6 retains the full `t64.state.v5` sequence documented in
`machine-contract.md`: domain `t64.state.v6` and u32 version 6, profile hash,
admitted content digest, loaded-image flag and size, fixed header/title/game
code/version, tick and counters, sticky error/halt, RDRAM size and bytes, SP
DMEM/IMEM sizes and bytes, MI pending/mask/line, PI latches and active request,
SP latches and descriptors, and ordered queue state/events. It then appends:

1. Asset, payload, and policy SHA-256 bytes (32 bytes each).
2. u32 source offset, payload length, DMEM offset, entry PC, seed reset
   version, then seed values for s3 through s7.
3. 32 u64 GPR values, u64 HI and LO, and 32 u64 CP0 values.
4. u64 retired-instruction count; u32 PC, last PC, last opcode, delay-slot
   PC, delay target, delay-slot-pending flag, stopped flag, stop reason,
   stop PC, and stop opcode.
5. u32 cache policy version, last operation, last PC, last address, and
   operation-seen flag.
6. u32 guest-device policy version, MI version, RI select, MI-version-known
   flag, and RI-select-known flag.
7. u32 output policy version, video-output-ready flag, and audio-output-ready
   flag.

All state integers are big-endian; bool-like flags are encoded as u32 zero or
one; byte arrays are included verbatim. Event fields use the v5 encoding.
Allocator pointers, host time, padding, unused queue storage, build identity,
and observer data are excluded. The ordering and sizes above permit a separate
vector generator to calculate the checked-in expected digest without calling
the machine digest API.

## Bounded CPU, CP0, and cache execution

The shared production CPU path implements the pinned warm path's reached
integer, branch/delay-slot, load/store, and KSEG0/KSEG1 operations. It also
implements the dynamically reached signed 32-bit `MULT` and `MFLO` helper
instructions. `MULT` writes sign-extended 32-bit HI/LO words; overflow and
arithmetic use unsigned host operations. Independent instruction snippets
check arithmetic, delay slots, alignment, partial stores, and virtual-address
translation.

The reached CP0 subset is `MFC0 Count` and `MTC0 Cause`, `Count`, `Compare`,
`WatchLo`, `TagLo`, and `TagHi`, all with select 0. Other registers, selects,
and coprocessor operations stop with the current PC/opcode and do not retire.
Register writes use these masks: Cause permits software-pending bits 9:8 and
preserves other bits; WatchLo uses `0xFFFFFFFB`; TagLo uses `0x0FFFFFC0`; the
VR4300 TagHi value is zero for this 32-bit physical-address profile. Count and
Compare use all 32 bits. These fields and instruction forms follow the NEC
VR4300 manual's CP0 instruction, Cause, WatchLo, and cache-tag descriptions
(§§3.2.6, 5.4.8, 6.3.6, and 6.3.8; document SHA-256 is recorded in the
[primary evidence index](../../.planning/phases/03-first-headless-rom-through-open-ipl3/03-PRIMARY-EVIDENCE-INDEX.md)).

The hardware Count register advances continuously at half PClock (§6.3.3).
The deterministic software policy here advances Count by one after each
successful instruction retirement. `MFC0` reads Count before that retirement
increment; the increment wraps at 32 bits. Count/Compare equality sets Cause
IP7, and writing Compare clears IP7, following §6.3.4. Only this pending bit
is modeled; exception entry, interrupt delivery, and processor-cycle timing
are not implemented. This cadence is a guest-time policy, not a hardware
timing claim.

The reached CACHE encodings are `0x8` Index Store Tag I, `0x9` Index Store Tag
D, and `0x15` Hit Writeback Invalidate D (NEC manual §16, CACHE operation
table; same indexed document hash). The pinned path writes zero TagLo/TagHi
before its index tag stores, leaving those lines invalid. Under the named
`coherent-functional` policy, instruction fetches and data accesses share the
backing bytes, so there is no hidden dirty line to write back. The emulator
records the last supported cache operation, PC, translated physical address,
and an operation-seen flag in the already hashed v6 cache state. It does not
model per-line cache timing or claim general cache fidelity. Unsupported
cache operations and unsupported address modes stop explicitly.

The actual byte-pinned IPL3 now reaches the unsupported MI_VERSION load:
`LW s1,4(v0)` (`0x8C510004`) at virtual address `0xA4300004`, PC
`0xA4000080`. The stop occurs at guest tick 38 after 37 retired instructions.
The profile's MI device slice does not implement MI_VERSION, so this boundary
is retained as an explicit unsupported stop. The warm profile's updated
identity is independently bound by descriptor digest
`d5a2b703c15ec8707e61a369530c524e6e88ef6d5548f0df2239a9a93a05c746` and
initial v6 state digest
`1c7e4086d89234c9d304e40f7e17468ddc8fcd04316904ddd7b5bc87913e96ea`; the v1
synthetic golden remains unchanged.

## Evidence limits and prerequisites

The Nintendo 64 Programming Manual v5.2 §6.10 describes IPL responsibilities
at an aggregate level. Its SHA-256 is
`07de17d36db262657a0f3a3eb1e8c12449a3e440eef911ff3517618e24c75040`.
The NEC VR4300/VR4305/VR4310 documentation sections 4.2–4.3, 5.2.4, 5.4.9,
and 11.3–11.5 have SHA-256
`b7c1900f1707a4b238ebd82af1669e9628e4df4875ceabfa012a9d95b4716b75` and
support the processor/address/cache context. Neither source specifies this
asset's original IPL2 handoff values. The original IPL2 register state,
PIF/CIC variants, cold-start behavior, and challenge execution remain unknown.
There is no cold calibration or RSP implementation in this slice.

For PI and SP scheduling, this profile uses the explicit skeleton policies
already listed in `machine-contract.md`: guest tick deadlines and FIFO event
sequence determine dispatch; host wall-clock is only a watchdog/backstop and
does not advance guest time. Inclusive guest-time advances dispatch events at
the requested tick before returning the bound, while an existing or newly
dispatched halt takes precedence as documented by the profile's
`halt-at-bound-wins-v1` policy. PI/SP event duration and cancellation rules
are model policy, not hardware timing claims.

The fixture producer needs a C compiler, `jq`, `curl`, GNU Binutils 2.45
MIPS64-ELF tools (qualified from the pinned source when absent), `make`, `tar`
with xz support, system zlib development headers and library (`zlib.h`/`libz`;
the fixture builder provisions `zlib1g-dev` only on hosted Ubuntu CI when
missing), `awk`, `dd`, `find`, SHA-256 and
MD5 tools (`shasum` or `sha256sum`, and `md5` or `md5sum`), plus CMake and
Ninja. It downloads only the hash-pinned public source asset and, when
required, the hash-pinned Binutils source archive. Generated ROMs stay
build-only and are checked against the local fixture manifest; no manual,
firmware, or protected bytes are included.

Each host's four Binutils executable SHA-256 values are recorded in the
ignored `build/tools/qualified-host-tools.json` receipt and checked against
that host's receipt on reuse. The pinned Binutils archive hash and version
identify source/toolchain origin; per-host executable hashes can differ with
compiler, SDK, and build path and do not enter guest or profile identity.
