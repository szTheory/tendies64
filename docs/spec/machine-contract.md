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
command/span, VI, AI, RI, and SI addresses are explicit unsupported windows.
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
[02-DECISION-B.md](../../.planning/phases/02-deterministic-machine-skeleton/02-DECISION-B.md)
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
[Decision B](../../.planning/phases/02-deterministic-machine-skeleton/02-DECISION-B.md):
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
v5 encoding streams the `t64.state.v5` domain and fixed-width big-endian
fields. It includes the complete profile hash, immutable admitted content
size and hash, parsed header metadata, loaded-image flag, guest tick,
cumulative counters, sticky error and halt state, RDRAM bytes, SP DMEM and IMEM
bytes, six-bit MI pending and mask state, the derived IRQ line, all PI raw
latches and sticky error, PI active/canceled flags and immutable descriptor,
active/next token and token-exhaustion state, all SP raw latches, both complete
SP descriptors and SP next-token/exhaustion state, and the logical ordered event
queue with its count and next-sequence/exhaustion state. Busy/full are derived
from descriptor validity. Event records encode tick, sequence, device, kind,
value and token. PI state was not reserved in v3, so the selected profile and
state schema advanced together to v4; SP state was not reserved in v4, so they
now advance together to v5. The profile digest is
`b7d1600ad960b4e04fdc3ec372675edc47b26b15be8e9cd84cbab4fd75835647`; the
independently assembled default fixture state digest is
`6813739ff2fc1cffac6d85fa86df0c593e3035f8a640c642e37790acd96b6e77`.

The open IPL3 warm profile preserves separate historical v6 and v7 encodings
and uses the active `t64.state.v8` encoding. Its v8 extension includes pinned
boot asset, payload and policy hashes; prepared boot offsets and seed fields;
CPU GPR/HI/LO/CP0, retirement and stop state; cache and prepared guest-device
state; and retained ISViewer state, including attempted-byte total and its
saturation flag. The active state serialization version is 8 and the profile
digest is `d8c12cb2a00410a2153ca62da318166f3ca029dea8e3d6b9c841262cb8203c71`.
The independent initial warm-entry state vector is
`d2212ea39cadf8642cab2451fbcea484f5c6997d51619faef41e9ada24aa9724`; the
tests retain the prior v6 and v7 vectors as historical evidence.

| Private field or state | Digest treatment | Reason |
| --- | --- | --- |
| Selected profile and its policy/version fields | Include profile hash | Fixes timing, capacity and model policy for this instance |
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
| RAM pointer, event handler pointers and build/source identity | Exclude | Storage/code identity is not serialized machine state |
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
The event only records a deterministic timed input observation. It is not
consumed by SI, PIF, or a controller model. Invalid port/axis values, past
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
| Output-drain cursor | Yes | Reset to zero with guest output; observer-only and excluded from the canonical digest |

Refusal assertions compare every present machine observation, scheduler state,
trace records/drop observation and ownership counter. Later device slices
must extend this snapshot and preserve the same no-change refusal guarantee.
