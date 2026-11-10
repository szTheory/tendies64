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
| Instruction/frame counters | No | Reset to the profile's initial value when introduced |
| CPU/RSP registers and SP banks | No | Reset to the profile's initial state when introduced |
| Scheduler queue and enqueue sequence | No | Empty queue and initial sequence when introduced |
| DMA descriptors, device latches, and interrupt state | No | Reset to the profile's initial state when introduced |
| Trace ring and observer drop count | No | Clear records and drops when introduced; keep observer state out of the machine digest |

There is no observer ring or drop counter in the current slice. Refusal
assertions therefore compare every present machine observation and ownership
counter. Later device slices must extend this snapshot and preserve the same
no-change refusal guarantee.
