# Audio interface and packet contract

## Scope and evidence

Audio DMA is enabled only by the composite profile
`ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1` (descriptor 11,
machine model 19, state serialization 24). The previous v9 profile keeps its
warm AI status/read and zero-write behavior; nonzero AI control/status writes
remain refused there.

The profile policy string is
`fetched-rdram-stereo-s16be-rational-62500000-v1`. The sample byte layout,
source-read timing, setting latches and host packet encoding below are explicit
reversible software-profile choices. They are not claims about undocumented
silicon behavior. The primary register facts and source tension are recorded in
[Phase 08 research](../../.planning/phases/08-audio-interface-and-raw-capture/08-RESEARCH.md).

## Modeled registers

All offsets are relative to `0x04500000`; accesses are 32-bit. A refused write
does not mutate the AI state or enqueue work.

| Offset | Register | Implemented behavior |
| --- | --- | --- |
| `0x00` | `AI_DRAM_ADDR` | Write-only. Stages `value & 0x00fffff8`; high bits and low three bits are ignored. The effective address must be within RDRAM. A nonzero length submission without a staged address refuses. Reads are unsupported. |
| `0x04` | `AI_LEN` | The effective length is `value & 0x0003fff8`. Zero is an accepted no-op that preserves any staged address. A nonzero length consumes the staged address and submits a DMA descriptor; it must be at most `0x3fff8` and wholly within RDRAM. Reads report bytes remaining in the active descriptor, or zero when idle. |
| `0x08` | `AI_CONTROL` | Write-only; only bit 0 is accepted. Zero pauses an active descriptor, cancels its pending sample event, and preserves its byte cursor, rational phase, and remaining enabled-time delay. One resumes that delay. Valid descriptors may be queued while disabled and wait for enable. Reads are unsupported. |
| `0x0c` | `AI_STATUS` | Read bit 30 is busy and bit 31 is full. Any write acknowledges MI AI pending bit `0x04` only; unrelated MI sources are preserved. |
| `0x10` | `AI_DACRATE` | Writable values are limited to bits `[13:0]`. When a nonzero BITRATE is set, a write is accepted only if the pair satisfies the supported ratio below. The active descriptor uses its latched value. |
| `0x14` | `AI_BITRATE` | Writable values are limited to bits `[3:0]`; zero writes refuse, although reset state is zero. A nonzero value is accepted only when the DACRATE pair satisfies the supported ratio below. The raw packet model preserves the value but does not resample or transform PCM from it. |

The modeled queue holds two descriptors. While full, an address write may stage
the next address, but a refused length write leaves that staged address and the
queue unchanged. Completion that transitions the queue from two descriptors to
one sets MI AI pending bit `0x04`; completing a single descriptor does not
synthesize that full-to-not-full interrupt. A status write clears only bit
`0x04`, preserving unrelated MI sources. Reset removes AI-owned events and
descriptors, clears only MI AI pending, and leaves other device sources intact.

For bounded software-profile support, nonzero BITRATE and DACRATE must satisfy
`DACRATE + 1 >= 66 * (BITRATE + 1)`. A submission also requires this valid pair.
An unsupported pair is refused without changing register or DMA state; this is
not a claim about electrical bus-stop behavior.

The source material distinguishes an 18-bit AI length field with low three
bits ignored (largest aligned register value `0x3fff8`, 262,136 bytes) from the
SDK API's 262,144-byte transfer ceiling. The implementation uses the smaller
register-sized aligned maximum. These are separate documented limits; the MMIO
field is not widened to 19 bits. The SDK also calls out the specific condition
`((address + length) & 0x3fff) == 0x2000`; this is one known DMA caveat and must
not be generalized into a rule for every 8 KiB crossing.

## Timing and source policy

Guest progress uses integer ticks at 62,500,000 ticks per second. For NTSC,
the sample clock is 48,681,812 Hz and the descriptor rate is
`48,681,812 / (DACRATE + 1)`. The core carries an exact integer remainder and
schedules each sample boundary with checked ceiling division; host wall time
does not advance audio.

Each sample event reads four current RDRAM bytes as one interleaved stereo
frame. DACRATE and BITRATE are latched when a descriptor becomes active;
subsequent setting writes apply to the next descriptor. The guest scheduler's
existing tick/sequence ordering resolves same-tick events. These are named
software policies because the cited register documentation does not settle
source latching or live setting-write effects.

When the bounded output queue cannot accept the next packet, the producing
sample event remains pending. The scheduler does not commit its source cursor,
phase, packet bytes or MI effects until output capacity is available. Draining
a packet frees host-observation capacity without advancing guest time.

## Raw packet API

One completed descriptor produces one packet containing its exact source
bytes. The packet metadata includes structure size, descriptor sequence, first
sample index, frame count, rational rate, first/last guest ticks, byte length,
two channels, 16 bits per sample, encoding `T64_AUDIO_ENCODING_S16BE`, profile
ID and build ID. S16BE names the software interpretation of the unchanged raw
bytes; the core does not convert or normalize samples.

`t64_audio_info()` copies metadata for the oldest available packet.
`t64_audio_copy()` copies and dequeues that complete packet on success. A
`NULL`/zero-capacity query or undersized destination reports the required byte
length and leaves the packet queued; refused copies do not dequeue or modify
destination bytes. Observer destination addresses and queue-drain position are
host state, not canonical guest state. Producer state and packet contents are
included in the machine's canonical digest.

## Authored tracer

`fixtures/audio/pcm.S` performs fetched CPU writes to AI and ordinary RDP/VI
registers, copying eight independently authored sample words into two
16-byte DMA buffers. The actual supervised runner compares the two returned
audio packets byte-for-byte and validates raw video output from the same guest
run. The wrong-sample build changes only the first left sample and reaches the
same packet comparator after accepted setup. Fixture identity, rights and
local-only build status are in `fixtures/manifest.json`.
