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
before scheduling one CPU step. The actual first word, `0x3044D236`, is
`ANDI a0,v0,0xd236` at `0xA4000040`; it retires at guest tick 1. The following
word, `0x3C1DA400` (`LUI` at `0xA4000044`), is unsupported. The machine stops
explicitly at tick 2 with one retired instruction and no completion event.
This establishes one partial entry instruction only. It establishes no
continued IPL3 routine, output, working device set, or running game.

## Independent identity goldens

The test's descriptor and initial-state goldens are fixed literals derived
from the field orders below and the qualified bytes, not copied from an
emulator-produced digest. The profile descriptor digest is
`f67e9fb4ad2c065e9ec4dc3048f447a8755255a281c602d2ac8c6cc5bcf38faa`.
The initial state digest, taken after exact admission and before dispatch, is
`21217cbc347896be9e97f7edcddd912a2fe4c9b0f151dd2e3809887477b6b42d`.
The existing v1 synthetic profile golden remains separate and unchanged.

Profile serialization v2 is SHA-256 over the following byte sequence:

1. ASCII domain `T64PDESC`, then u32 version `2`.
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
its final LF; its digest is
`a6eeba466ae8804a8dec79ce8c5dece40ea5d25b29724d53f7e1ebe46ffd5cc9`. That
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
Ubuntu CI provides these with `zlib1g-dev`), `awk`, `dd`, `find`, SHA-256 and
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
