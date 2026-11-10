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
v2 encoding streams the `t64.state.v2` domain and fixed-width big-endian
fields. It includes the complete profile hash, immutable admitted content
size and hash, parsed header metadata, loaded-image flag, guest tick,
cumulative counters, sticky error and halt state, RDRAM bytes, and the logical
ordered event queue with its count and next-sequence/exhaustion state. Event
records encode tick, sequence, device, kind, value and token. The state schema
version is part of the selected descriptor and was advanced to 2; the profile's
machine and device policy values remain unchanged.

| Private field or state | Digest treatment | Reason |
| --- | --- | --- |
| Selected profile and its policy/version fields | Include profile hash | Fixes timing, capacity and model policy for this instance |
| Admitted image length and content digest | Include | Identifies the immutable installed content |
| Parsed header, title, game code and version | Include | Records normalized content metadata explicitly |
| Loaded-image flag | Include | Distinguishes lifecycle state; digest returns not-ready before admission |
| Guest tick, instruction/frame counters, sticky error, halt | Include | Changes future advancement and stop results |
| RDRAM bytes | Include | Writable machine memory |
| Queue count, logical event fields, next sequence and exhausted flag | Include | Determines future dispatch and enqueue behavior |
| Allocator callbacks/context and machine/image storage descriptors | Exclude | Host ownership and pointer locations are not machine behavior |
| RAM pointer, event handler pointers and build/source identity | Exclude | Storage/code identity is not serialized machine state |
| Queue head, unused event slots, structure padding | Exclude | Physical layout is not logical queue state |
| Trace configuration, records and drop observation | Exclude | Observer state must not alter guest state or its digest |

The field-mutation, exclusion, relocation, golden-vector, interleaving and
partition tests cover this audit. Future device/register/request state must be
added to the encoding and this table with its own tests when introduced.

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
| Instruction/frame counters | Yes | Reset to zero |
| CPU/RSP registers and SP banks | No | Reset to the profile's initial state when introduced |
| Scheduler queue, next sequence and exhaustion state | Yes | Empty queue, initial sequence and available sequence |
| DMA descriptors, device latches, and interrupt state | No | Reset to the profile's initial state when introduced |
| Trace ring and observer drop count | No | Clear records and drops when introduced; keep observer state out of the machine digest |

The trace observer is added in the following task. Refusal assertions compare
every present machine observation and ownership counter. Later device slices
must extend this snapshot and preserve the same no-change refusal guarantee.
