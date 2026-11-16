# VI diagnostic scanout contract

This document describes the first bounded raw scanout path for profile
`ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v1`. Output is a canonical machine
packet, separate from host scaling, filtering, files and display APIs.

## Source facts and model policy

Nintendo SDK `rcp.h` documents the VI register offsets and the NTSC clock
constant `VI_NTSC_CLOCK = 48681812`. It identifies STATUS, ORIGIN, WIDTH,
CURRENT, BURST, V_SYNC, H_SYNC, H_START, V_START, X_SCALE and Y_SCALE registers.
The constant is a sourced input; it does not establish how Tendies64's guest
tick maps to physical scan lines or prove this implementation's timing is
hardware-accurate.

The new profile binds these model values:

- `vi_clock_policy`: `rsp62500000-ntsc48681812-rational-v1`.
- Guest tick unit: `model-rsp-cycle`; one CPU tick per modeled RSP cycle in
  this profile.
- VI source clock: 48,681,812 cycles per second, from the SDK constant.
- Guest tick rate: 62,500,000 ticks per second, a Tendies64 software model
  input.
- Field work period: `(H_SYNC + 1) × (V_SYNC + 1)` VI cycles. Exact fractional
  phase is accumulated from elapsed guest time; the scheduler orders equal
  ticks by its existing sequence number.
- Geometry uses the programmed active interval times the 10-bit fractional
  scale, rounded down by integer division. Output starts at the programmed
  source offset; this tracer uses zero offsets and unit scale `0x400`.

The fixture programs source origin `0x3000`, source width 4, H/V active
intervals of 4 and 3, H/V sync values 4 and 2, and X/Y scale `0x400`. This
model yields a 4 × 3 output and a 15-cycle field period. From the deterministic
VI enable at guest tick 538862, the first field boundary is at 538882:
`ceil(15 × 62,500,000 / 48,681,812) = 20` guest ticks. Later periods can
alternate between integer tick deltas as rational phase carries its remainder;
the first interval is not a general claim about every field.

The only accepted source format in this tracer is big-endian RGBA5551. Each
five-bit channel is expanded as `(value << 3) | (value >> 2)` and the alpha
bit becomes 0 or 255. The profile policy
`rgba5551-nearest-no-filter-rgba8-two-packet-v1` names this implementation
choice. The source pixels `0xf801` and `0x07c1` therefore become RGBA8
`(255,0,0,255)` and `(0,255,0,255)`, respectively. This channel expansion is
the authored model expectation; it is not a measurement of VI silicon.

The raw packet includes geometry, source address and format, output encoding,
field/parity, programmed register values, guest start/end ticks, command
progress, Full Sync progress, profile/build identity and the timing-policy
name. A fixed-capacity two-packet queue is lossless. `t64_video_info` observes
the oldest packet; `t64_video_copy(NULL, 0, &length)` reports its required
size without consuming it, while a full successful copy dequeues exactly that
packet. Invalid or undersized destinations do not partially copy or advance
the queue.

VI registers, exact phase remainder, the scheduled field event, event
generation, field sequence and produced packet bytes/metadata are hashed in the
per-instance machine state. The bounded packet ring uses the producer sequence
to choose slots. Its observer head advances for every successful copy and is
not reset when the ring becomes empty, so a given produced packet history has
the same physical slots regardless of drain cadence. The observer head and
queue occupancy are excluded from the digest.

When the ring is full, the scheduler leaves the original VI field event at the
front of the event queue. That event is not popped, traced or rescheduled, and
guest time does not advance past it. A successful host copy only advances the
observer head and frees one slot; the next advance dispatches the same pending
event with its original tick and sequence. Host copy-out does not change the
guest digest or scheduler sequence.

## Limits and evidence

This first diagnostic covers only the programmed 4 × 3 RGBA5551 source window,
one no-filter RGBA8 conversion and the first rational field boundary. Standard
TV modes, other formats, interlace, filters, latches and broader timing vectors
belong to later plans. VID-04 remains blocked and is not satisfied by this
authored tracer.
