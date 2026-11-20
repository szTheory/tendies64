# Provisional machine and ROM contract

The C interface is provisional. This document records the observable ownership,
admission, refusal, and reset behavior that the native core currently
implements. It does not claim boot, CPU execution, hardware timing, or a frozen
external ABI.

## Raw ROM admission

`t64_machine_admit_rom` accepts one caller-provided readable byte span. The
host selects and reads the file; the core does not open paths, inspect file
names, unpack archives, or apply patches. The caller keeps the span readable
and unchanged until the call returns.

The selected profile sets the initial accepted length to 4096 through
67108864 bytes, inclusive, with a length divisible by four. The first four
bytes must be one of these raw dump signatures:

| Input order | Signature | Normalized storage |
| --- | --- | --- |
| Canonical big endian | `80 37 12 40` | Big endian |
| Byte swapped | `37 80 40 12` | Big endian |
| 32-bit word reversed | `40 12 37 80` | Big endian |

The core validates the length and checked input address interval before it
reads a signature or requests candidate storage. It checks the input interval
against the live machine and admitted image before reading the input. The
candidate is a separate core-owned copy. Normalization, fixed-header parsing,
and the full-image SHA-256 finish before the candidate is installed.

The fixed header is exactly the first 64 normalized bytes. The title is the
20 raw bytes at `0x20..0x33`, the game code is the four raw bytes at
`0x3B..0x3E`, and version is the byte at `0x3F`. These fields are not strings
and unknown values are accepted. Header bytes never choose TV standard,
region, CIC, save media, rights, or a boot route.

On success, the candidate image, size, header, metadata, and content digest
replace the previous content as one commit. The previous image is released
only after the new candidate is complete and installed. A successful content
replacement preserves the selected profile and allocator, resets guest tick to
zero, and clears all current RDRAM bytes.

For a refused admission, the existing image allocation and bytes, content
digest, header and size observations, profile, guest tick/RDRAM state digest,
and outstanding allocation ownership remain unchanged. A distinct invalid
candidate allocation is released once when its returned and requested address
intervals can both be checked as disjoint from protected storage. A descriptor
that may alias the caller span or live storage is never passed to `release`.

`t64_machine_rom_size` writes the admitted image size only on success. Before
admission it returns `T64_STATUS_NOT_READY` without changing the output.
`t64_machine_rom_header` copies all 64 normalized header bytes on success. On a
loaded machine, passing a null output with capacity zero queries the required
length and returns `T64_STATUS_BUFFER_TOO_SMALL`; the required length is 64.
Any too-small output leaves its bytes unchanged and reports the same required
length. Null/invalid outputs and outputs overlapping machine-owned storage are
refused without changing machine state. Before admission, both observation
functions return `T64_STATUS_NOT_READY` without changing outputs.

## Host allocator contract

The host supplies both allocation and release callbacks. An allocation request
specifies a byte count and alignment. If `allocate` returns false, it retains
responsibility for any partial work and the core does not call `release`. If it
returns true, the descriptor must identify a live allocation with the requested
backing capacity and alignment. The core checks for a non-null pointer, enough
reported capacity, a reported alignment at least as strict as requested, an
aligned address, nonwrapping half-open pointer ranges, and disjointness from
the machine's live storage and current admission input.

When a true allocation result is invalid but both its reported range and the
requested range are representable and disjoint from protected storage, the
core passes the exact returned descriptor to `release` once. For creation,
protected storage also includes the caller's allocator descriptor, profile,
and output slot. If either range overlaps the machine, current image, input
span, or a caller-owned creation argument, the core refuses without releasing
it. If pointer arithmetic cannot establish the returned range, the core
refuses and cannot safely establish ownership to release.

The host remains responsible for the actual readable/writable backing size,
the lifetime and stability of caller spans during admission, correct callback
reports, disjoint allocations across separate machine instances, and no
re-entry into the core from an allocator or release callback. The core can
check the descriptor it receives; it cannot measure whether a callback's
reported capacity is truthful or inspect the real allocation object. Tests
that return a smaller capacity verify rejection of that descriptor, not
detection of a lying allocator.

Callbacks run only during machine creation, admission, replacement, and
destruction. Scheduled guest execution and read-only observations do not call
the host.

## Physical bus and MI

The provisional host debug bus uses physical addresses between calls to
`t64_machine_advance`; it does not translate CPU virtual addresses or run guest
instructions. Widths are one byte or an aligned four-byte word. Words use
explicit big-endian byte assembly. Each access validates its complete range
before reading or mutating storage; a refusal leaves output and machine state
unchanged.

The selected profile exposes RDRAM at `[0x00000000, 0x00400000)`. Other
addresses in the vendor RDRAM aperture `[0x00400000, 0x03f00000)` and the
RDRAM-register window are recognized but unsupported for this profile. The
SP aperture has separate 4 KiB DMEM and IMEM byte banks at
`[0x04000000, 0x04001000)` and `[0x04001000, 0x04002000)`; other SP, DP
command/span, VI, AI, RI, and SI addresses are explicit unsupported windows,
except that the composite SI profile family decodes the SI window
`[0x04800000, 0x04900000)` to the modeled serial interface described in
[si-pif-controller.md](si-pif-controller.md): four 32-bit registers at offsets
`0x00`, `0x04`, `0x10` and `0x18`, with every other offset unsupported. Every
older profile keeps the warm status stub for that window, and the PIF window
stays unsupported for every profile. The SRAM id of that family also decodes the
32 KiB window `[0x08000000, 0x08008000)` to aligned 32-bit word access (carved
out of the domain 2 unsupported arm; widths 1, 2 and 8, a misaligned word and
every address outside it are refused as before), described in
[save-media.md](save-media.md); every other profile keeps the unsupported arm
unchanged.
PI's word registers at `0x04600000..0x04600030` implement the ROM DMA model
below. MI interrupt-pending is read-only, and MI interrupt-mask supports
the paired clear/set commands.

Admitted cartridge bytes are read-only at `[0x10000000, 0x1fc00000)` and are
bounded by the admitted image length. Other documented cartridge-domain
windows and `[0x1fc00000, 0x1fc00800)` PIF space are unsupported. Unassigned
addresses are unmapped. The supported profile behavior is a checked software
model; unsupported results do not assert the hardware's open-bus response.
The map and SP banks follow the cited vendor register map; the 4 MiB installed
RDRAM extent and refusal behavior are profile policy. See the
[Nintendo 64 Introductory Manual §1.2.4](https://ultra64.ca/files/documentation/online-manuals/man/kantan/step1/2-4.html)
and [Nintendo SDK `rcp.h` map and register definitions](https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm).
The exact retrieved hashes and locators are recorded in
[02-DECISION-B.md](../../.planning/milestones/v1.0-phases/02-deterministic-machine-skeleton/02-DECISION-B.md)
and beside the independent expectations in
`core/tests/test_bus.c`.

MI keeps six pending bits (SP, SI, AI, VI, PI, DP) separate from six mask
bits. Pending is read-only through the bus and changes through device-owned
helpers. Mask writes use paired clear/set command bits; simultaneous clear
and set uses the selected set-wins model policy, and reserved command bits
are ignored. The exposed derived line is
`(mi_pending & mi_mask) != 0`; this equation is spec-derived, not a hardware
measurement. Mask changes never acknowledge pending state. SP DMA completion
does not synthesize an SP interrupt in this slice.

Successful bus reads and writes and MI state transitions may emit observer
records at the current guest tick. Bus records identify operation width and
direction in `kind`, with physical address in the upper 32 bits and data in the
lower 32 bits of `value`. MI records pack pending bits, mask bits, and line
state into `value`. These records do not change execution or the state digest.

## PI ROM DMA model

The PI exposes 32-bit registers at `0x04600000` through `0x04600030` on
four-byte offsets: DRAM address, cartridge address, read length, write length,
status, then domain 1 and domain 2 latency/pulse-width/page-size/release-duration
latches. DRAM address and both length latches retain 24 bits; the cartridge
address retains 32 bits. Timing latches retain 8, 8, 4, and 2 bits in that
order for each domain. Reads return the last masked writes. These widths and
field meanings follow the cited SDK `rcp.h` PI map (lines 667-767).

Only PI write-length starts work in this ROM profile. Read-length writes retain
their masked latch and are refused because cartridge writes are unsupported.
PI status exposes DMA_BUSY (bit 0), IO_BUSY (bit 1), and ERROR (bit 2); this
model keeps IO_BUSY clear because it does not model program-I/O requests.
Status writes use bit 0 to reset the PI controller and bit 1 to clear pending
PI in MI. Clearing pending PI does not change MI masks. When both bits are
written, reset is applied first and the pending clear second; a later event at
the same tick follows scheduler FIFO and may set PI pending again.

The source documents the register map, status meanings, and the raw/software
DMA constraints of an even byte count, at most 16 MiB, 8-byte RDRAM alignment,
and 2-byte PI alignment. The following details are explicitly selected
skeleton behavior where the source does not establish raw-register edges or
electrical timing:

| Behavior | Selected model contract | Evidence label |
| --- | --- | --- |
| Write-length encoding | `N = (write_length & 0x00ffffff) + 1`; accept even `N` only. Keep the masked raw latch on semantic refusal. | `N-1` decode and odd raw-register refusal are model policy/inference from the API's even byte-count rule. |
| Address range | Require full source span inside the admitted cartridge image and full destination span inside installed 4 MiB RDRAM; perform additions in checked 64-bit arithmetic before accepting. | Profile limit and refusal behavior. |
| Request identity | Snapshot DRAM address, cartridge address, byte count, token, and completion event before setting active/busy. Later raw-latch writes do not alter the accepted descriptor. | Deterministic machine policy. |
| Duration and visibility | Reserve one event at `current_tick + 1 + N/2`; read owned ROM bytes and atomically copy the full span at that completion event. Domain timing latches do not affect this equation. | Named model timing and visibility policy; not measured hardware timing or progressive bus arbitration. |
| Busy start | Retain the original active request, retain the new masked write-length latch, refuse the second request, and set sticky ERROR. | Explicit compatibility policy; the SDK's program-I/O ERROR description does not prove this second-DMA edge. |
| Reset while active | Mark the active request canceled, clear ERROR, retain its busy state, token, and already-reserved deadline. Do not accept a replacement request until that completion returns the controller to idle. | Token-preserving cancellation policy. |
| Canceled completion | At the original deadline, match the token, skip the copy, clear active/busy once, and set PI pending once. Reset while idle clears ERROR without creating an event or interrupt. | Model policy; tests cover exact deadline and stale-token negative controls. |
| Resource refusal | Queue, tick, sequence, or token exhaustion refuses before changing the request latch, descriptor, busy state, or digest. Semantic invalid starts may update the masked raw latch; busy start is the named exception with sticky ERROR. | Reservation and checked-arithmetic policy. |

The source is read from the machine-owned admitted ROM at completion, not
snapshotted at request time. Thus a scheduled event cannot observe a partial
destination copy, and guest source changes before completion would be visible
under this model. PI completion sets MI pending independent of its mask; only
the status clear-interrupt command clears pending PI. Every accepted request
has one reserved queue slot before busy becomes visible. A cancellation reuses
that event rather than allocating another one. PI trace records identify issue,
refusal, cancellation and completion at their model ticks; traces have no host
sink and remain outside the canonical digest.

The register definitions and alignment/count constraints are pinned in
[Decision B](../../.planning/milestones/v1.0-phases/02-deterministic-machine-skeleton/02-DECISION-B.md):
Nintendo SDK `rcp.h`, PI lines 667-767, SHA-256
`982f5ae01956783653d1085ef31c3d936f471f09bd1e65e27a578fae498f972f`, and
`osPiRawStartDma`, SHA-256
`560ef2b35b5cb96b2c175608e137d70203ab1f1e3cb4a14572935bf266f4a83b`. Those
references do not establish the model's `1 + N/2` timing, source-read-at-event
visibility, reset edge ordering, or busy-start ERROR rule.

## SP DMA model

The SP control block at `0x04040000` implements `SP_MEM_ADDR`, `SP_DRAM_ADDR`,
`SP_RD_LEN`, `SP_WR_LEN`, read-only `SP_STATUS`, `SP_DMA_FULL`, and
`SP_DMA_BUSY`. `SP_RD_LEN` transfers RDRAM to the selected 4 KiB DMEM/IMEM bank;
`SP_WR_LEN` transfers that bank to RDRAM. `SP_STATUS` reports DMA_BUSY in bit 2
and DMA_FULL in bit 3. The dedicated full/busy registers return 0 or 1. Status,
semaphore, and SP PC writes remain unsupported in this phase.

SP address and length latches retain their masked 13-bit, 24-bit, and 24-bit
values. Each descriptor separately records those raw values and the aligned
starts: initial SP and RDRAM low three bits are ignored for memory access,
while the source low bits remain observable in the descriptor and digest. The
length fields decode as `L = F & 0xfff`, `C = (F >> 12) & 0xff`, and
`S = (F >> 20) & 0xf`; bytes per line are `(L | 7) + 1`, line count is `C + 1`,
and each next DRAM row starts at the previous row start plus its length and
`S`. Before accepting either direction, the implementation checks every full
row with 64-bit arithmetic against the installed 4 MiB RDRAM span. It does not
wrap or partially accept a row. Payload bytes advance contiguously in the
selected internal bank and wrap through its 12-bit address field at 4 KiB.
That bank-wrap behavior is an explicit address-field inference, not a hardware
measurement.

Each accepted request has an immutable active or pending descriptor and its
completion event reserved in the bounded scheduler before the corresponding
busy state becomes visible. The pending request starts at the active request's
deadline; its event is already in the queue, so promotion consumes no new
capacity and cannot fail under later queue pressure. A third request while both
slots are occupied retains its masked triggering length latch, emits one
observer-only `SP_DROP` record, returns the bus-device-refused status, and leaves
both descriptors, reserved events, guest error state, and MI pending bits intact.
Resource refusal for a row, deadline, token, sequence, or queue-capacity problem
does not change the triggering length latch or either accepted descriptor.
Busy remains set across promotion; full is set exactly while both descriptors
are occupied.

For the named `sp_dma_skeleton_v1` policy, each request costs
`12 + payload_bytes/8` RSP-cycle units. The profile records the integer
CPU:RSP ratio and uses an exact conversion to GuestTick; the selected profile is
1:1, so an 8-byte request completes 13 ticks after its start and a 48-byte
request completes 18 ticks after its start. These are deterministic model
deadlines, not measured hardware latency. Completion reads source bytes from
the current owned RAM/SP bank at the scheduled event and copies all rows
atomically in increasing row/byte order. Advancement allocates no snapshot and
calls no host callback. A pending source can therefore change after issue and
before its own completion. SP DMA completion does not set MI SP pending; PI and
SP events at the same tick retain the scheduler's global `(tick, sequence)`
order. Successful content replacement clears SP descriptors and scheduler
events, preventing old requests from writing into replacement state.

## Deterministic scheduler and state digest

The private scheduler orders admitted work by the pair `(guest_tick,
sequence)`. Equal-tick work therefore runs in enqueue order. A guest-tick stop
is inclusive: events at the requested tick run before the machine reports the
bound, and an empty queue advances the idle guest tick to that bound. A bound
below the current tick is refused without changing machine state. A counter
stop below its cumulative counter is likewise refused; an equal counter
returns immediately without dispatch. For a larger counter target, dispatch
stops as soon as the selected cumulative instruction or frame-event counter
reaches the target, leaving later events at that same tick queued. These
counters are driven only by explicitly labeled synthetic test events in this
slice; they are not evidence of CPU execution or video frames.

A halt already present wins before dispatch. A halt event dispatched at an
inclusive time bound wins over the bound result. If the event queue empties
before a counter target is reached, the result is quiescent at the last
dispatched tick. Each advance dispatches at most the profile's 4096-event
budget. Budget exhaustion, an invalid queued handler, a handler error, or a
counter overflow reports a resource-limit result and retains already completed
work. Counter overflow is checked before removing its event, so that event
remains pending. Queue capacity, past deadlines, sequence exhaustion and
tick-add overflow are rejected before machine-state mutation; rejected
reservations do not consume sequence values.

`t64_machine_digest` is a versioned SHA-256 encoding, not a memory dump. The
profile's state serialization version selects the encoding. Every encoding
starts with a domain string and then streams fixed-width big-endian fields; no
padding, pointer or host layout is hashed. One walker, `t64_digest_stream`,
produces the stream and a sink either hashes it, copies it into a buffer, or
only counts it, so a digest and a buffer copy are the same bytes. A write past a
buffer's capacity sets the sink's overflow flag, copies nothing and still
advances the running length, which is how a size query works.

The encodings that exist in `core/t64_digest.c`, by state version:

| State version | Domain string | Registered profile ids |
| --- | --- | --- |
| 5 | `t64.state.v5` | `ntsc-cart-4m-skeleton` |
| 6, 7 | `t64.state.v6`, `t64.state.v7` | historical encodings, kept for fixed test vectors |
| 8 | `t64.state.v8` | `ntsc-cart-4m-openipl3-warm-v1` |
| 9, 10, 11 | `t64.state.v9`, `t64.state.v10`, `t64.state.v11` | `...-vr4300-v1`, `...-vr4300-compat-v1`, `...-vr4300-compat-v2` |
| 12 | `t64.state.rsp.functional.12` | `...-rsp-functional-v1` |
| 13 | `t64.state.rdram.t1.13` | `...-rdram-a-t1-v1`, `...-rdram-b-t1-v1` |
| 16 | `t64.state.rdp.vi.tmem.16` | `...-rdram-b-t1-rdp-vi-v2` |
| 17 | `t64.state.rdp.vi.triangle.17` | `...-rdp-triangle-v3` |
| 18 | `t64.state.rdp.vi.texture.18` | `...-rdp-texture-v4` |
| 19 | `t64.state.rdp.vi.combiner.19` | `...-rdp-combiner-v5` |
| 20 | `t64.state.rdp.vi.blender.20` | `...-rdp-blender-v6` |
| 21 | `t64.state.rdp.vi.depth.21` | `...-rdp-depth-v7` |
| 22 | `t64.state.rdp.vi.scanout.22` | `...-rdp-vi-v8` |
| 23 | `t64.state.rdp.vi.filter.23` | `...-rdp-vi-filter-v9` |
| 24 | `t64.state.audio.ai.24` | `...-rdp-vi-filter-v9-ai-v1` |
| 25 | `t64.state.si.25` | the four composite SI family ids |

State versions 14 and 15 have no encoding. Each version includes everything the
earlier ones do for the state its profile has. The v5 encoding hashes the
domain, the serialization version word, the complete profile hash, the admitted
content digest, the loaded-image flag and length, the parsed header, title, game
code and ROM version, the guest tick, instruction and frame-event counters,
sticky error and halt flag, the RDRAM size word and bytes, the SP DMEM and IMEM
size words and bytes, the MI pending and mask bits and derived IRQ line, all PI
latches, flags, descriptor and tokens, all SP latches, both descriptors and
tokens, and then the ordered event queue (count, next sequence, exhaustion flag
and each event's tick, sequence, device, kind, value and token). Busy and full
are derived from descriptor validity. The extensions, in order of appearance:

* v6 to v8 (warm): pinned boot asset, payload and policy hashes, prepared boot
  offsets and seed fields, CPU GPR/HI/LO/CP0 with retirement and stop state,
  cache and prepared guest-device state, and the ISViewer buffer, retained
  prefix and loss flag; v8 adds the attempted-byte total and its saturation
  flag.
* v9 to v11: the full VR4300 state (TLB, instruction and data cache lines,
  FPRs, pending exception, count/compare latches, link state); v10 adds the
  branch origin; v11 adds the CP2 transfer latch.
* v12: the RSP register, pipeline, DMA-cursor and scoreboard state.
* v13: the RDRAM protocol model (RI latches and per-device register state).
* v16 to v23: the RDP/VI block (DPC state, command, texture, combiner, blender
  and depth state as each version adds it, the VI registers and phase, and the
  retained video packet slots with their metadata); v22 and v23 add the
  latched scanout fields and the filter and output-policy metadata.
* v24: the audio block (AI latches, rate latch and phase, descriptor queue,
  packet history hash, and the retained audio packet slots).

Through v24 the retained video and audio packet metadata includes the profile
id and the build identity string. The profile digest of the default skeleton
profile is `b7d1600ad960b4e04fdc3ec372675edc47b26b15be8e9cd84cbab4fd75835647`; the
independently assembled default fixture state digest is
`6813739ff2fc1cffac6d85fa86df0c593e3035f8a640c642e37790acd96b6e77`. The open
IPL3 warm profile has the profile digest
`d8c12cb2a00410a2153ca62da318166f3ca029dea8e3d6b9c841262cb8203c71` and the
independent initial warm-entry state vector
`d2212ea39cadf8642cab2451fbcea484f5c6997d51619faef41e9ada24aa9724`; the tests
retain the prior v6 and v7 vectors as historical evidence. The legacy encodings
are unchanged by the sink: the idle digest of each of the 17 registered
pre-family ids, and the Phase 8 audio profile with a DMA active before its
first sample dispatches, are pinned as literal values captured before the sink
seam existed.

The v25 encoding frames the stream in sections. After the domain
`t64.state.si.25` the stream is a repetition of a 32-bit big-endian section id
followed by that section's body, in ascending id order. Section bodies sit at
the block boundaries of the v24 stream and no field is reordered or resized, so
v25 is not comparable to v24 byte for byte but its shared sections are.

| Id | Section | Body |
| --- | --- | --- |
| 1 | identity | serialization version word, profile digest, content digest, image flag and length, header, title, game code, ROM version |
| 2 | machine core | tick, instruction count, frame-event count, sticky error, halt flag |
| 3 | rdram | RDRAM byte-count word, RDRAM bytes |
| 4 | sp memory | DMEM size word and bytes, IMEM size word and bytes |
| 5 | interrupt, PI and SP | MI pending, mask and IRQ line; all PI state; all SP state |
| 6 | scheduler | queue count, next sequence, exhaustion flag, ordered events |
| 7 | warm devices | boot, CPU, cache, guest-device, ISViewer, RSP and RDRAM protocol state |
| 8 | rdp and vi | the RDP/VI block, including the retained video packet slots |
| 9 | audio | the AI block, including the retained audio packet slots |
| 10 | SI and PIF | emitted by the composite family: SI registers and active descriptor, the 64-byte PIF RAM, the parsed joybus table and the four per-port input latches, every field in the new-span class; the field table is in [si-pif-controller.md](si-pif-controller.md) |
| 11 | save media | emitted by the three save ids only: kind, size, EEPROM deadline, active PI save direction and the raw storage, every field in the new-span class; the field table is in [save-media.md](save-media.md) |
| 16 | observer continuity | reserved; outside the digest |

Reserved ids emit nothing until their section exists; sections 10 and 11 are the
new-class sections emitted so far (section 11 only when the profile has a save
device, so the id without one keeps markers 1 to 10). Stream bytes carry one of
four span classes for comparison: body (ordinary state), identity (the domain,
the serialization word and profile digest, and in v24 the retained packet
profile and build ids), marker (a section id) and new (a section with no v24
counterpart). The v25 stream omits the retained packet `profile_id` and
`build_id` from the hashed bytes; both stay stored in the packet metadata, and a
later import re-derives them because profile and build binding is exact.

The strip rule used by the differential test: removing the marker, identity and
new spans from a v25 stream, and the identity spans from the v24 stream of the
Phase 8 profile, leaves equal bytes for machines built through the same
operations. The test applies it to an idle machine, an audio DMA with a packet in
progress, an audio machine after its first packet, a machine with a retained
video packet, and a machine with a pending RDP command word. An independent
reference encoder, written from this table and the machine fields without
calling production code, reproduces the domain, the section ids and the bodies
of sections 1, 2, 3, 4 and 6 byte for byte. It is not a second implementation
of every device section: sections 5, 7, 8 and 9 are covered by the differential
against the v24 stream, which the pinned legacy literals anchor.

| Private field or state | Digest treatment | Reason |
| --- | --- | --- |
| Selected profile and its policy/version fields, including `si_policy`, `save_policy`, `save_kind` and `peripheral_flags` for descriptor 12 | Include profile hash | Fixes timing, capacity, model and device-set policy for this instance |
| Admitted image length and content digest | Include | Identifies the immutable installed content |
| Parsed header, title, game code and version | Include | Records normalized content metadata explicitly |
| Loaded-image flag | Include | Distinguishes lifecycle state; digest returns not-ready before admission |
| Guest tick, instruction/frame counters, sticky error, halt | Include | Changes future advancement and stop results |
| RDRAM bytes | Include | Writable machine memory |
| SP DMEM and IMEM bytes | Include | Writable SP-local memory banks |
| MI pending bits, mask bits, and derived IRQ line | Include | Device interrupt state and the CPU seam's current line |
| PI raw address/length/timing latches and sticky error | Include | Register readback and later request behavior |
| PI active/canceled state, immutable descriptor, active/next token, exhaustion | Include | Determines whether the reserved event may copy or complete, and identity of later work |
| SP raw latches, active/pending immutable descriptors, next token and exhaustion | Include | Determines row addresses, completion ticks, promotion and request identity; busy/full are derived |
| Queue count, logical event fields, next sequence and exhausted flag | Include | Determines future dispatch and enqueue behavior |
| Warm boot asset/payload/policy hashes, offsets, entry and HLE seed | Include in warm v6-v8 | Binds the prepared entry and future boot state to source and policy |
| Warm CPU registers, CP0, delay slot, retirement and terminal-stop state | Include in warm v6-v8 | Determines subsequent instruction behavior and stop results |
| Warm cache policy/last operation and prepared MI/RI device state | Include in warm v6-v8 | Determines later cache/device accesses and readback |
| ISViewer register buffer, retained prefix, loss, attempted total and saturation | Include in warm v7-v8; v8 adds exact-total fields | Determines future output and whether its total remains exact |
| Allocator callbacks/context and machine/image storage descriptors | Exclude | Host ownership and pointer locations are not machine behavior |
| RAM pointer, event handler pointers and build/source identity | Exclude, except the retained packet profile and build ids through v24 | Storage/code identity is not machine state; v24 and earlier hash the ids inside retained packet metadata, v25 does not |
| Queue head, unused event slots, structure padding | Exclude | Physical layout is not logical queue state |
| Trace configuration, records, drop observation, and output-drain cursor | Exclude | Observer state must not alter guest state or its digest |

The field-mutation tests toggle every PI and SP latch, flag, descriptor value,
token, and exhaustion state, plus each warm CPU/CP0 register slot and every
serialized cache, boot, guest-device, pending-input and output field.
Independent reference encoders and fixed v6/v7/v8 vectors pin field order;
agreement between production digests alone is not hardware-accuracy evidence.
The actual open IPL3 guest is compared at the same 530500-tick request bound
using one-shot and 1/7/31-tick partitions, trace enabled and disabled runs,
different output-drain timing, and two interleaved machines. Trace-on logical
31-tick partitions drain at substeps no larger than 16 ticks, and resource
limit yields continue to the same guest stop. These local checks establish
repeatability for the pinned warm profile, authored hello ROM and input bytes
on the current host. Hosted compiler comparison is recorded separately in
Phase 3 Plan 13.

The observer is disabled by default and can be enabled per machine. When
enabled, successful ROM admission emits a machine-boundary record at guest tick
zero, each dispatched scheduler event emits a record at its scheduled guest
tick before its handler runs, and successful bus accesses and MI transitions
emit records at the current guest tick. Records contain fixed-width tick,
device, kind and value fields. The 256-record ring keeps older records when full and
drops the new record; the cumulative u64 drop count saturates and reports a
separate saturation flag. Draining copies whole records in FIFO order, outside
advance. A zero-capacity drain may pass a null record buffer, copies zero
records and leaves the queue intact. Invalid nonzero output buffers are refused
without draining. Neither trace enablement nor draining calls the host or
changes the canonical digest.

## Composite SI profile family

This section is provisional. The ids, descriptor fields, constants and section
numbers below are revisable until the Phase 10 interface review; nothing here is
an API or ABI freeze.

Four registered profile ids form one family. All share descriptor version 12,
machine model version 20 and state serialization version 25, and they extend
the Phase 8 machine (descriptor 11, model 19, state 24) with identity for a
standard controller on port 0 and an explicit save device. The family carries
identity, validation and the state-stream framing, and the modeled SI/PIF and
standard controller described in [si-pif-controller.md](si-pif-controller.md);
the save devices are described in [save-media.md](save-media.md).

| Profile id | Id length | `save_kind` |
| --- | --- | --- |
| `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1` | 38 | 0, no save device |
| `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e4k` | 42 | 1, EEPROM 4 Kbit |
| `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e16k` | 43 | 2, EEPROM 16 Kbit |
| `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-s32k` | 43 | 3, SRAM 32 KiB |

`peripheral_flags` bit 0 (value 1) names the standard controller on port 0. The
descriptor gains two text fields after `ai_policy`: `si_policy`, with the label
`si-hle-joybus-wire-v1`, and `save_policy`, with the label
`save-explicit-eeprom4k16k-sram32k-busy6ms-v1`. Both are project-chosen
software-profile policy names, not hardware measurements, and their text is
final at this point of the contract. Descriptor 12 serializes them immediately
after `ai_policy`. Scheduler version and event-order policy stay
`tick-sequence-fifo-v1`: this family adds no class ranking to equal-tick order.

Descriptor 12 validates the numeric fields that older versions only hash. The
`save_kind` must be 0 to 3, `peripheral_flags` may not set any bit other than
bit 0, the synchronization quantum must be 1, and the CPU:RSP ratio must be
1:1. Hash, serialize and create each refuse a descriptor that breaks one of
these rules, return no machine and leave allocator counters unchanged. Machine
creation also refuses a descriptor 12 under an id outside the four above, and
every registered id is hash-pinned at creation: a descriptor that differs from
its canonical bytes in any field is refused. Older descriptor versions keep
their hashing unchanged, including a non-zero `save_kind` or `peripheral_flags`
in a hash-only vector, but `t64_machine_create` refuses a non-zero value of
either field for a version below 12 before any allocation.

The save device requests answer three status values, `T64_STATUS_SAVE_UNKNOWN_DEVICE`
(20), `T64_STATUS_SAVE_UNSUPPORTED_DEVICE` (21) and `T64_STATUS_SAVE_DEVICE_MISMATCH`
(22), through `t64_machine_save_info`; the values and that function are provisional
until the Phase 10 interface review. Host import and export of the save bytes
(`t64_machine_save_import`, `t64_machine_save_export`, the three byte-order
selections, the statuses 23 to 25 and the observer-only import generation) are
specified in `docs/spec/save-media.md` under Host import and export; they are
provisional on the same terms.

The synchronization quantum is measured in RSP issue opportunities, equal to
guest ticks in these profiles. A quantum or ratio other than 1 and 1:1 is a new
profile, not a runtime setting. The quantum is bound through `profile_digest`,
which hashes the descriptor serialization; it is not a separate key of any
`t64.result` record. The hash-level effect is demonstrated on a descriptor 11
copy: changing only the quantum alters the serialized bytes in exactly one
32-bit window and therefore the hash.

The family inherits every Phase 8 device: CPU, RSP, PI, SP, RDP, VI and AI use
the same behavior as the Phase 8 profile, the ISViewer window is available at
its Phase 8 address, and the AI register window and events are enabled through
one shared device gate. RDP depth is not part of the descriptor 11 or
descriptor 12 state; this is inherited unchanged from Phase 8 and this contract
states no depth coverage.

The state digest of the family uses the `t64.state.si.25` domain and the
sectioned framing described with the digest encodings. The v25 stream is not
comparable to the v24 stream of the Phase 8 profile.

## Closed event ledger

The scheduler dispatches events keyed by a device id and a kind. The set of
pairs is closed: a pair appears in the ledger only when a reservation site
exists in `core/t64_*.c`, and a later state import refuses an unknown pair.

| Device | Kind | Event | Reservation site |
| --- | --- | --- | --- |
| 5 | 1 | CPU step | CPU scheduling, exception entry, successor preflight |
| 3 | 1 | PI DMA completion | PI cartridge DMA start |
| 4 | 1 | SP DMA completion | legacy SP DMA start (profiles without the functional RSP) |
| 4 | 2 | RSP issue | RSP issue scheduling; SP DMA on the functional-RSP profiles also uses it |
| 6 | 1 | timed input submission | `t64_machine_queue_input` |
| 7 | 1 | RDP work | RDP command work scheduling |
| 8 | 1 | VI field | VI register write and field events |
| 9 | 1 | AI sample | AI DMA start and sample scheduling |
| `0xffff0001` | 1 to 5 | synthetic test events | test scheduling helpers: no-op, instruction, frame, halt, repeat |
| 10 | 1 | SI execute | SI read start (RD64B), at the start tick plus the handshake total |
| 10 | 2 | SI complete | SI read and write start, at the completion tick |

Event order stays `tick-sequence-fifo-v1` for every device: events dispatch by
guest tick and then by reservation sequence, and no device has a rank. Two
events placed at one tick therefore dispatch in the order they were reserved.
The ledger test compares an observed set against this table and rejects an
unlisted pair, a missing row and a reserved row that appears (no row is reserved
since the SI start is a reservation site); a transposed copy of the table is
rejected. The order test places two events at one tick in
both insertion orders for every pair of ledger rows using label-only
events through the real scheduler, and for the pairs an existing builder can
place at one tick it also uses the real device events: PI with SP completion
(legacy profile), VI with AI sample, and the CPU step with each of PI
completion, RSP issue, timed input, RDP work, VI field and AI sample. SP
completion with any CPU or functional-profile device has no builder, because
the functional-RSP profiles issue SP DMA through RSP issue events; those pairs
are covered by the label-only case alone.

The save devices add no scheduler pair: the SRAM DMA reuses the PI completion
pair and the EEPROM busy window is a stored deadline with no event, so the table
above is unchanged by them.

These are regression pins and self-comparison. The model chooses no hardware
tie order and no measurement supports one, so a consistently wrong tie would
pass these tests; the order is a stated software-profile policy, not hardware
evidence. The list of version-membership and reservation sites is kept in
`docs/spec/profile-membership-sites.txt`, regenerated and compared by
`scripts/list-membership-sites.py --check`; a later plan refreshes the list in
the commit that adds its site.

## F0 identity record

This record freezes the identity slice of the composite family at its first
point (F0). It is provisional until Phase 10 and implies no API or ABI freeze.

* Profile ids: `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1`, and the same id with
  the suffixes `-e4k`, `-e16k` and `-s32k`.
* Tuple: descriptor version 12, machine model version 20, state serialization
  version 25; `peripheral_flags` 1; `save_kind` 0, 1, 2 and 3 respectively.
* Labels: `si_policy` is `si-hle-joybus-wire-v1`; `save_policy` is
  `save-explicit-eeprom4k16k-sram32k-busy6ms-v1`.
* State stream: domain `t64.state.si.25`, 32-bit big-endian section ids 1 to 9
  in ascending order with ids 10, 11 and 16 reserved, and the span classes body,
  identity, marker and new, as described with the digest encodings.
* Frozen at F0: the version-membership pattern (the listed sites), the section
  ids and framing, the id scheme and the numeric-field validation. A later change
  is a recorded deviation with its cost, not a silent edit.

## Output observation and timed input

`t64_machine_output_drain` is a provisional host observation call, not a frozen
ABI. The caller serializes it with `t64_machine_advance` and other calls on the
same instance. It copies the not-yet-observed suffix of that instance's
retained ISViewer capture into caller-owned storage and reports the copied
length, required length, total bytes requested by all nonzero guest flushes
(including refused flushes), whether that total is exact, SHA-256 of the
complete retained prefix, and sticky loss state. The guest counts every
nonzero flush request with checked u64 arithmetic, even when the retained
4096-byte prefix is full. A u64 overflow saturates the counter and marks it
inexact; the public observation does not present that saturated value as an
exact total. A short buffer reports the required length and observations
without copying or advancing the cursor. Repeated drains with no new bytes
copy zero. Host copying never releases the 4096-byte guest capture or changes
guest identity.

Before writing either output, drain validates pointer ranges, address overflow,
pairwise result/data overlap, and overlap with machine storage, the admitted
image, and RDRAM. Invalid or null nonempty spans leave caller bytes and the
instance unchanged. The result span is required. A null data span is allowed
only with zero capacity. Draining is available after admission, including
after a terminal CPU stop; successful image replacement resets both guest
output and the observer cursor.

`t64_machine_queue_input` accepts an absolute guest tick, port 0, 16 button
bits, and X/Y axes in the signed 8-bit range `[-128,127]`. The scheduled event
value stores buttons in bits 0-15, X in bits 16-23, and Y in bits 24-31 using
two's-complement octets. It uses the existing per-instance scheduler's
reserve-then-commit path and sequence ordering. Its stable trace identity is
device 6, kind 1; event kind numbers are interpreted with their device source.
On the composite SI profile family the event value also carries the port in
bits 32 and 33 and the dispatched event feeds a strictly-after per-port latch
that the modeled SI samples: a poll sampled at tick `E` sees the last input
stamped strictly before `E` (see [si-pif-controller.md](si-pif-controller.md));
the public call keeps accepting port 0 only. On every other profile the event
only records a deterministic timed input observation and no SI, PIF or
controller model consumes it. Invalid port/axis values, past
ticks, unready or terminal machines, a full queue, and sequence exhaustion
refuse without partial queue or sequence mutation. Input records are
observer-only; the pending event tuple is included in state-v8 identity until
it dispatches.

## Replacement reset snapshot

This table is the reset audit for content replacement. New future-affecting
state must be added here and to the reset, digest, and refusal tests when it is
introduced.

| State | Present in this slice? | Replacement result |
| --- | --- | --- |
| Selected profile and allocator | Yes | Preserve |
| Admitted image storage and size | Yes | Install complete new candidate |
| Normalized 64-byte header, title, game code, version, content digest | Yes | Install values parsed from the candidate |
| Guest tick | Yes | Reset to zero |
| RDRAM | Yes | Clear to zero |
| SP DMEM and IMEM | Yes | Clear both 4 KiB banks to zero |
| MI pending bits, mask bits, derived IRQ line | Yes | Reset to zero/false |
| Instruction/frame counters | Yes | Reset to zero |
| CPU/RSP registers | No | Reset to the profile's initial state when introduced |
| Scheduler queue, next sequence and exhaustion state | Yes | Empty queue, initial sequence and available sequence |
| PI raw latches, error, active/canceled descriptor and token state | Yes | Clear latches and descriptor, clear active/error/exhaustion flags, reset next token to 1 |
| Trace records, drop count and enablement | Yes | Clear records and drops; preserve enablement and emit a new admission record when enabled |
| SI registers, active descriptor, PIF RAM, parsed table, sticky error, input latches and tokens (composite family) | Yes | Cancel the SI events, clear MI SI pending, clear all SI state and set the next token to 1 |
| Save device storage and EEPROM deadline (composite family save ids) | Yes | Fill with the blank contents (0xFF for the EEPROM, 0x00 for the SRAM) and clear the deadline on a successful replacement; a refused replacement leaves both unchanged |
| Save import generation (`write_generation`, composite family save ids; observer-only, outside the digest) | Yes | Reset to zero on a successful replacement together with the blank storage and the cleared deadline; a refused replacement leaves bytes, deadline and generation unchanged |
| Output-drain cursor | Yes | Reset to zero with guest output; observer-only and excluded from the canonical digest |

Refusal assertions compare every present machine observation, scheduler state,
trace records/drop observation and ownership counter. Later device slices
must extend this snapshot and preserve the same no-change refusal guarantee.

## F1 state content record

This record freezes the content of the v25 state stream at its second point
(F1), after the SI and PIF section (10) and the save-media section (11) exist.
It is provisional until Phase 10 and implies no API or ABI freeze.

F1 statement: the section ids, the framing and the per-section content of the
stream frozen here equal the F0 table above plus the section 10 and 11 bodies
specified in [si-pif-controller.md](si-pif-controller.md) and
[save-media.md](save-media.md). A later change to the content of any section is
a recorded defect fix with the affected goldens regenerated in the same plan and
the dependent plans re-run, never a silent edit. Replay checkpoints (plan
09-07), the state container (plan 09-08) and the readers (plan 09-09) are built
on this content.

The goldens below are the length and the SHA-256 of each section body (the bytes
between a section marker and the next marker) of six authored states. Section 1
carries the profile digest and is not pinned (no literal profile hash exists
before F2). The authored states are: `idle-stem`, `idle-e4k`, `idle-e16k` and
`idle-s32k`, the four ids admitted with the cleared openIPL3 asset at tick 0
with the boot CPU event queued; `all-stem`, the stem id at tick 60,000 after the
all-devices workload W-ALL of `core/test_sched_matrix` (CPU, RSP, RDP, VI, AI,
PI and SI live, three host inputs), where only the CPU step and the next VI line
event remain queued; and `all-s32k`, the SRAM id after the same workload with
four authored words written to the SRAM window first (`0x00000001`,
`0x23456789`, `0xABCDEF01` and `0xFFFFFFFF` at `0x08000000`, `0x08000004`,
`0x08007FF8` and `0x08007FFC`). The rows are produced by
`core/test_sched_matrix --print-sections` and compared by the case
`state-content-f1`.

| State | Section id | Length | SHA-256 |
| --- | --- | --- | --- |
| idle-stem | 2 | 32 | 66687aadf862bd776c8fc18b8e9f8e20089714856ee233b3902a591d0d5f2925 |
| idle-stem | 3 | 4194308 | 092c3dd93f8ac49c788bedfbd21e919be94c99512009c180f25b602babd2c6cd |
| idle-stem | 4 | 8200 | 1b38310e5191489195ce799377f93da6368cc18437eb60c19ed4e886a8271d02 |
| idle-stem | 5 | 276 | 15cc6a687b141c3e5e90854599f9d2908b95749664c0418184927a628195be20 |
| idle-stem | 6 | 56 | 2d04db54e23eaa8219575be70c424d2ea274da8770485b9d957de66bf6541853 |
| idle-stem | 7 | 43724 | f6948a6a0e1d39e936f5761429c6c96361ab663b3066f4f440a2b1e6b10acb47 |
| idle-stem | 8 | 2103016 | 87adef4ee0974db59ba1eba88437d01f7db040c709aa90c5e70f312e65138065 |
| idle-stem | 9 | 336 | 52a3e0804d93dc525ec3c67ef8ac5b01756ecf0513e36f3c19435e4c82cb5d29 |
| idle-stem | 10 | 404 | aa393fb5f348ef124a46175d478d4246a351c24f2b479c9ce574c158b5bbadba |
| idle-e4k | 2 | 32 | 66687aadf862bd776c8fc18b8e9f8e20089714856ee233b3902a591d0d5f2925 |
| idle-e4k | 3 | 4194308 | 092c3dd93f8ac49c788bedfbd21e919be94c99512009c180f25b602babd2c6cd |
| idle-e4k | 4 | 8200 | 1b38310e5191489195ce799377f93da6368cc18437eb60c19ed4e886a8271d02 |
| idle-e4k | 5 | 276 | 15cc6a687b141c3e5e90854599f9d2908b95749664c0418184927a628195be20 |
| idle-e4k | 6 | 56 | 2d04db54e23eaa8219575be70c424d2ea274da8770485b9d957de66bf6541853 |
| idle-e4k | 7 | 43724 | f6948a6a0e1d39e936f5761429c6c96361ab663b3066f4f440a2b1e6b10acb47 |
| idle-e4k | 8 | 2103016 | 87adef4ee0974db59ba1eba88437d01f7db040c709aa90c5e70f312e65138065 |
| idle-e4k | 9 | 336 | 52a3e0804d93dc525ec3c67ef8ac5b01756ecf0513e36f3c19435e4c82cb5d29 |
| idle-e4k | 10 | 404 | aa393fb5f348ef124a46175d478d4246a351c24f2b479c9ce574c158b5bbadba |
| idle-e4k | 11 | 532 | ce276b54e42d1e377ef7d43f652d49f51acfdd62213af71868cc4db2b9945636 |
| idle-e16k | 2 | 32 | 66687aadf862bd776c8fc18b8e9f8e20089714856ee233b3902a591d0d5f2925 |
| idle-e16k | 3 | 4194308 | 092c3dd93f8ac49c788bedfbd21e919be94c99512009c180f25b602babd2c6cd |
| idle-e16k | 4 | 8200 | 1b38310e5191489195ce799377f93da6368cc18437eb60c19ed4e886a8271d02 |
| idle-e16k | 5 | 276 | 15cc6a687b141c3e5e90854599f9d2908b95749664c0418184927a628195be20 |
| idle-e16k | 6 | 56 | 2d04db54e23eaa8219575be70c424d2ea274da8770485b9d957de66bf6541853 |
| idle-e16k | 7 | 43724 | f6948a6a0e1d39e936f5761429c6c96361ab663b3066f4f440a2b1e6b10acb47 |
| idle-e16k | 8 | 2103016 | 87adef4ee0974db59ba1eba88437d01f7db040c709aa90c5e70f312e65138065 |
| idle-e16k | 9 | 336 | 52a3e0804d93dc525ec3c67ef8ac5b01756ecf0513e36f3c19435e4c82cb5d29 |
| idle-e16k | 10 | 404 | aa393fb5f348ef124a46175d478d4246a351c24f2b479c9ce574c158b5bbadba |
| idle-e16k | 11 | 2068 | f83831868f121ac1d54cbd1c034f73592c75695302ae5b10f4bb4c8f20c0c4de |
| idle-s32k | 2 | 32 | 66687aadf862bd776c8fc18b8e9f8e20089714856ee233b3902a591d0d5f2925 |
| idle-s32k | 3 | 4194308 | 092c3dd93f8ac49c788bedfbd21e919be94c99512009c180f25b602babd2c6cd |
| idle-s32k | 4 | 8200 | 1b38310e5191489195ce799377f93da6368cc18437eb60c19ed4e886a8271d02 |
| idle-s32k | 5 | 276 | 15cc6a687b141c3e5e90854599f9d2908b95749664c0418184927a628195be20 |
| idle-s32k | 6 | 56 | 2d04db54e23eaa8219575be70c424d2ea274da8770485b9d957de66bf6541853 |
| idle-s32k | 7 | 43724 | f6948a6a0e1d39e936f5761429c6c96361ab663b3066f4f440a2b1e6b10acb47 |
| idle-s32k | 8 | 2103016 | 87adef4ee0974db59ba1eba88437d01f7db040c709aa90c5e70f312e65138065 |
| idle-s32k | 9 | 336 | 52a3e0804d93dc525ec3c67ef8ac5b01756ecf0513e36f3c19435e4c82cb5d29 |
| idle-s32k | 10 | 404 | aa393fb5f348ef124a46175d478d4246a351c24f2b479c9ce574c158b5bbadba |
| idle-s32k | 11 | 32788 | 96a813dddb2aed9e4a8070099a5e2afa9d8ed98b29c5a04409bed28e6eb36659 |
| all-stem | 2 | 32 | 457af84927bf7a656d18d99b13659c780b5ddcb8ff4255abeae1b573e7e288ce |
| all-stem | 3 | 4194308 | 8750bf1a32a94778f11024ad176e8fe449af78baa1fa1f1a1b7b793afe7079e4 |
| all-stem | 4 | 8200 | b0f31494ac3e56d1a19d67e697a61176c808f67e3aa008dce54d72d25c9dcba7 |
| all-stem | 5 | 276 | d76402d0c97abeead364cda2fd7ba107632608da576dea659f05d0667e8f648e |
| all-stem | 6 | 96 | da4194e598f9e3e8b0db720185fa6a21194d3c6fd542aa92d0689782632f3c9a |
| all-stem | 7 | 43724 | 12d7b7b731d26cb0c69fd817e787950ce8c4e6bdb0b69d5ac9dc526bdddfa88b |
| all-stem | 8 | 2103112 | 68e538d595cc112a0f82d1a94539818a58aee372a41adeeb3de5c0ca3dfa705c |
| all-stem | 9 | 344 | bdb91a273e1d7a36634d1837c9ef25c274b5abb72591eaa2805b26a47189f58e |
| all-stem | 10 | 404 | 22b74d14eb39b1c125207961e65022947a75724aff09de581405802198483142 |
| all-s32k | 2 | 32 | 457af84927bf7a656d18d99b13659c780b5ddcb8ff4255abeae1b573e7e288ce |
| all-s32k | 3 | 4194308 | 8750bf1a32a94778f11024ad176e8fe449af78baa1fa1f1a1b7b793afe7079e4 |
| all-s32k | 4 | 8200 | b0f31494ac3e56d1a19d67e697a61176c808f67e3aa008dce54d72d25c9dcba7 |
| all-s32k | 5 | 276 | d76402d0c97abeead364cda2fd7ba107632608da576dea659f05d0667e8f648e |
| all-s32k | 6 | 96 | da4194e598f9e3e8b0db720185fa6a21194d3c6fd542aa92d0689782632f3c9a |
| all-s32k | 7 | 43724 | 12d7b7b731d26cb0c69fd817e787950ce8c4e6bdb0b69d5ac9dc526bdddfa88b |
| all-s32k | 8 | 2103112 | 68e538d595cc112a0f82d1a94539818a58aee372a41adeeb3de5c0ca3dfa705c |
| all-s32k | 9 | 344 | bdb91a273e1d7a36634d1837c9ef25c274b5abb72591eaa2805b26a47189f58e |
| all-s32k | 10 | 404 | 22b74d14eb39b1c125207961e65022947a75724aff09de581405802198483142 |
| all-s32k | 11 | 32788 | 18027aa2804723811fc26bbe7869393c01a51b8f74981ca5f2228f3a0b70dfbb |

Where the lengths come from, and what the goldens are. The lengths of sections
2 to 7, 10 and 11 were derived by hand from the field tables before any capture
and equal the walker (32; 4,194,308; 8,200; 276; 16 plus 40 per queued event;
43,724; 404; and 20 plus the storage size). The lengths of sections 8 and 9 are
captured and checked by a differential: the v24 stream of the Phase 8 audio
profile in the same idle state, minus its identity spans, equals the 132
non-identity bytes of section 1, the hand-derived sections 2 to 7 and the
captured sections 8 and 9. The hash of every section is a regression pin: it is
the walker compared with itself, so a consistently wrong field would pass. The
case also shows that a changed body byte reports the label `section-golden` and
a changed pinned length reports `section-length`. Nothing here is hardware
evidence, and the hosted jobs of plan 09-11 add the check on a second platform.

### State member audit

Every member of `struct t64_machine` in `core/t64_internal.h` has exactly one
row: the canonical section that carries it, the observer-continuity section (16)
or an exclusion with its reason class (allocator, pointer, trace, drain cursor,
rsp observation, padding, queue head), and whether replacement resets it and by
which function. The member list is the set of declarations in the structure body (comment lines excluded), and has 67 members. Build and source identity is a compile-time string, not a
member, and is excluded as behavior (the retained packet metadata keeps the
profile and build ids only through v24).

| Member | Section | Reset by replacement | Reason |
| --- | --- | --- | --- |
| allocator | excluded | no, preserved | allocator: host callbacks and context are not machine behavior |
| machine_storage | excluded | no, preserved | allocator: the host storage descriptor of the machine allocation |
| image_storage | excluded | yes, `t64_machine_admit_rom` installs the candidate storage | pointer: the storage location is host layout; the image is identified by the content digest in section 1 |
| profile | 1, through `profile_digest` | no, preserved | the descriptor is hash-pinned and every policy field, including the synchronization quantum, is bound through its hash |
| profile_digest | 1, identity span | no, preserved | identity: the hash of the descriptor serialization |
| content_digest | 1 | yes, `t64_machine_admit_rom` | the admitted content identity |
| ram | 3, the RDRAM bytes | yes, `reset_guest_state` | pointer: the location is excluded, the bytes are section 3 |
| rdp_triangle_coverage | 8, when the profile has the triangle side table | yes, `reset_guest_state` | pointer: the location is excluded, the table is hashed with the RDP block |
| rdp_memory_coverage | 8, when the profile has the blender side table | yes, `reset_guest_state` | pointer: the location is excluded, the table is hashed with the RDP block |
| rdp_depth_hidden | 8, only when the depth predicate holds (not for this family) | yes, `reset_guest_state` | pointer: the location is excluded, the table is hashed with the RDP block when present |
| sp_dmem | 4 | yes, `reset_guest_state` | SP-local memory bank |
| sp_imem | 4 | yes, `reset_guest_state` | SP-local memory bank |
| mi_pending | 5 | yes, `reset_guest_state` | device interrupt state |
| mi_mask | 5 | yes, `reset_guest_state` | device interrupt state |
| mi_irq_line | 5 | yes, `reset_guest_state` | derived line kept as state for the CPU seam |
| pi | 5 | yes, `t64_pi_reset_machine_state` | latches, descriptor and tokens |
| sp | 5 | yes, `t64_sp_reset_machine_state` | latches, descriptors and tokens |
| rsp | 7 | yes, `reset_guest_state` (halted again on the functional-RSP profiles) | registers, pipeline, scoreboard and DMA cursors |
| cpu | 7 | yes, `reset_guest_state` then the warm entry preparation | registers, CP0, TLB, caches, FPRs, timer and link state |
| boot | 7 | yes, `reset_guest_state` then the warm entry preparation | prepared boot hashes, offsets and seed fields |
| cache | 7 | yes, `reset_guest_state` | cache policy and last operation |
| guest_devices | 7 | yes, `reset_guest_state` | prepared MI and RI state and the RDRAM protocol model |
| output | 7 | yes, `reset_guest_state` | ISViewer window, retained prefix and totals |
| rdp | 8 | yes, `t64_rdp_reset_machine_state` | DPC, command, texture, combiner, blender and depth state as the profile has them |
| vi | 8 | yes, `t64_vi_reset_machine_state` | VI registers, phase and the retained packet metadata |
| video_storage | excluded | no, preserved | allocator: the host storage descriptor of the retained video slots |
| video_packets | 8, the retained slot bytes up to each slot length | yes, `t64_vi_reset_machine_state` | pointer: the location is excluded, drained slots are hashed |
| ai | 9 | yes, `t64_ai_reset_machine_state` | AI latches, rate latch, descriptor queue and packet history hash |
| si | 10 | yes, `t64_si_reset_machine_state` | SI registers, active descriptor, PIF RAM, parsed table and input latches |
| save | 11, with `write_generation` observer-only | yes, `t64_save_reset_machine_state` | kind, size, EEPROM deadline and raw storage are canonical; the import generation is outside the digest and travels with observer state if the container carries it (decided in plan 09-08) |
| audio_storage | excluded | no, preserved | allocator: the host storage descriptor of the retained audio slots |
| audio_packets | 9, the retained slot bytes up to each slot length | yes, `t64_ai_reset_machine_state` | pointer: the location is excluded, drained slots are hashed |
| output_drain_cursor | observer-continuity (16) | yes, `reset_guest_state` | drain cursor: outside the digest; observer state that gates stop points, carried by the container section 16 (content decided in plan 09-08) |
| image_length | 1 | yes, `t64_machine_admit_rom` | the admitted image length |
| tick | 2 | yes, `t64_machine_reset_execution_state` | guest time |
| instruction_count | 2 | yes, `t64_machine_reset_execution_state` | cumulative counter |
| frame_event_count | 2 | yes, `t64_machine_reset_execution_state` | cumulative counter |
| sticky_error_status | 2 | yes, `t64_machine_reset_execution_state` | sticky host-error status |
| rom_header | 1 | yes, `t64_machine_admit_rom` | normalized header |
| rom_title | 1 | yes, `t64_machine_admit_rom` | normalized title |
| rom_game_code | 1 | yes, `t64_machine_admit_rom` | normalized game code |
| rom_version | 1 | yes, `t64_machine_admit_rom` | normalized version |
| has_image | 1 | yes, `t64_machine_admit_rom` | loaded-image flag |
| halted | 2 | yes, `t64_machine_reset_execution_state` | halt flag |
| events | 6, the logical queue only | yes, queue emptied by `t64_machine_reset_execution_state` | pointer and padding: the handler pointers and the unused ring slots are excluded; handlers are re-derived from the closed event ledger |
| queue_head | excluded | yes, `t64_machine_reset_execution_state` (set to 0) | queue head: the physical ring position is not logical queue state |
| queue_count | 6 | yes, `t64_machine_reset_execution_state` | scheduler state |
| next_sequence | 6 | yes, `t64_machine_reset_execution_state` | scheduler state |
| sequence_exhausted | 6 | yes, `t64_machine_reset_execution_state` | scheduler state |
| trace_records | excluded | yes, `t64_trace_reset` | trace: observer ring, never alters guest state |
| trace_head | excluded | yes, `t64_trace_reset` | trace: ring head |
| trace_count | excluded | yes, `t64_trace_reset` | trace: ring count |
| trace_drop_count | excluded | yes, `t64_trace_reset` | trace: drop observation |
| trace_drop_count_saturated | excluded | yes, `t64_trace_reset` | trace: drop observation |
| trace_enabled | excluded | no, preserved | trace: configuration |
| rsp_observed_pc | excluded | no, rewritten at the next RSP attempt | rsp observation: diagnostic only |
| rsp_observed_opcode | excluded | no, rewritten at the next RSP attempt | rsp observation: diagnostic only |
| rsp_observed_second_pc | excluded | no, rewritten at the next RSP attempt | rsp observation: diagnostic only |
| rsp_observed_second_opcode | excluded | no, rewritten at the next RSP attempt | rsp observation: diagnostic only |
| rsp_observed_tick | excluded | no, rewritten at the next RSP attempt | rsp observation: diagnostic only |
| rsp_observed_ready_tick | excluded | no, rewritten at the next RSP attempt | rsp observation: diagnostic only |
| rsp_observed_dma_cursor | excluded | no, rewritten at the next RSP attempt | rsp observation: diagnostic only |
| rsp_observed_stall_reason | excluded | no, rewritten at the next RSP attempt | rsp observation: diagnostic only |
| rsp_observed_refusal_reason | excluded | no, rewritten at the next RSP attempt | rsp observation: diagnostic only |
| rsp_observed_dma_action | excluded | no, rewritten at the next RSP attempt | rsp observation: diagnostic only |
| rsp_observed_outcome | excluded | no, rewritten at the next RSP attempt | rsp observation: diagnostic only |
| rsp_observation_valid | excluded | no, rewritten at the next RSP attempt | rsp observation: diagnostic only |
