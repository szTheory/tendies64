# VI scanout contracts

This document covers the legacy v2 diagnostic profile, the later v8 composite
VI profile, and the v9 filtered VI profile. All emit canonical machine packets,
separate from host scaling, filtering, files and display APIs. Authored software
behavior is not hardware qualification, and VID-04 remains blocked.

## Composite v9 filtered profile

Profile `ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9` inherits v8's
clock, register, latch, geometry, interrupt and queue behavior. It uses
descriptor version 10, machine model version 18 and state serialization
version 23. Profiles v2 through v8 retain their prior identity and output
behavior. The new `vi_output_policy` is
`5551-8888-aa53mdtl-q8f-df3-dvmed-bq20f-gd16r8-sqrt255-pcspan-v3`; its filter
choices are deterministic integer software approximations, not recovered
hardware kernels or VID-04 evidence.

STATUS bits 0–1 select the inherited mode. Bits 8–9 select anti-alias behavior;
bit 16 selects the 5551 smoothing approximation; bit 4 selects divot median;
bit 3 enables integer gamma; bit 2 enables gamma dither only when gamma is
enabled. Modes 0 and 1 use the same partial-coverage color policy. Mode 2
treats coverage as full; mode 3 uses the nearest sample and bypasses
fractional resampling. Partial coverage selects AA before smoothing, so AA and
the dither filter are mutually exclusive for that sample. 8888 bypasses the
dither filter. Source alpha remains the nearest center's decoded alpha.

AA searches a 5×3 neighborhood for full-coverage colors. It selects the
candidate with greatest RGB Manhattan distance from the foreground, breaking
ties in top-to-bottom, left-to-right order. If none exists, foreground is its
own background. For coverage `c` in 0..7, each output channel is
`floor((c*foreground + (8-c)*background)/8)`. Coverage is the RDP logical
sidecar; alpha is not used to guess coverage. The 5551 dither approximation
averages expanded RGB over a 3×3 neighborhood, rounding down. Divot applies to
partial-coverage mode 0/1 samples after the backend choice, using the
component-wise median of left, center and right.

The selected software stage order is AA-or-smoothing, optional divot,
fractional resampling, optional gamma dither, then gamma. Modes 0–2 use
bilinear RGB resampling at the existing Q10 coordinate. The implementation
retains both fractional coordinates through four-tap selection and floors the
weighted Q20 sum once. Serrate uses the selected parity and same-parity next
row. Mode 3 uses the nearest floor sample. Gamma maps each channel to
`floor(sqrt(255*channel))` through bounded integer square root. Gamma dither
uses the fixed 4×4 rank matrix
`[0,8,2,10;12,4,14,6;3,11,1,9;15,7,13,5]`: ranks below 8 subtract one, the
others add one, with saturation before gamma. It consumes no RDP RNG.

Horizontal taps clamp to the source row. Top taps clamp to the first row of
the selected parity. When another tap would exceed RDRAM, the sampler reuses
the valid center and does not reject the packet. The v9 `source_bytes` field
reports the bounding span from ORIGIN through the highest valid source-color
tap actually read, including filter and resample neighbors. The `filter`
packet field reports selected stages: bit 0 AA, bit 1 smoothing, bit 2 divot,
bit 3 Q20 resampling, bit 4 gamma dither, and bit 5 gamma. These are v9 packet
semantics; v8 packet bytes and metadata remain unchanged.

## Composite v8 profile

Profile `ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v8` adds timed scanout while
retaining the inherited RDP policies. It uses descriptor version 10, machine
model version 17 and state serialization version 22. The earlier v2 through v7
profile IDs and state domains retain their existing meanings.

The documented register fields follow Nintendo SDK `rcp.h` at VI base
`0x04400000`. The offsets from `0x00` through `0x34`, in four-byte steps, are
STATUS, ORIGIN, WIDTH, INTR, CURRENT, BURST, V_SYNC, H_SYNC, LEAP, H_START,
V_START, V_BURST, X_SCALE and Y_SCALE. The source documents STATUS bits 0–4,
6, 8–9 and 16; ORIGIN bits 0–23; WIDTH bits 0–11; INTR and CURRENT bits 0–9;
BURST bits 0–29; V_SYNC bits 0–9; H_SYNC bits 0–11 and 16–20; LEAP bits 0–11
and 16–27; H_START, V_START and V_BURST bits 0–9 and 16–25; and X_SCALE and
Y_SCALE bits 0–11 and 16–27. These are known-field unions, not verified bus
write masks. STATUS bit 16 is retained despite the SDK header's conflicting
16-bit comment. Unselected raw bits remain readable as written and take no
scanout effect. STATUS mode 1 is refused; modes 0, 2 and 3 select blank,
RGBA5551 and RGBA8888 behavior respectively.

### Clock and boundary policy

The profile binds
`vi_clock_policy=rsp62500000-ntsc48681812-halfline-rational-v2`. Guest ticks
run at the inherited 62,500,000 ticks per second. The VI source clock is
48,681,812 Hz, the NTSC constant documented by the SDK. These rates are inputs
to the software clock conversion; they do not establish a silicon timing
measurement.

For the selected model, one horizontal period is
`(H_SYNC & 0x0fff) + 1`; `V_SYNC & 0x03ff` is the number of half-lines in a
field. The field period is therefore `(H_SYNC.low12 + 1) * V_SYNC.low10 / 2`
source-clock cycles. The implementation stores doubled source-phase units,
so odd half-lines remain exact. Elapsed guest ticks, not CPU retirement or host
time, advance VI phase. Quotient/remainder arithmetic carries fractional
phase across split advances. Only the next bounded boundary is scheduled.
Same-tick events use the scheduler's existing tick/sequence FIFO; a zero-delay
VI event is allowed when accumulated phase has crossed multiple short
boundaries, and the machine's event budget and output queue bound the work.

The primary source says CURRENT is sampled once per full line and advances in
half-line units; this profile samples at even half-line boundaries. INTR
compares against that sampled half-line. Serrate alternates field parity,
starting at field zero; progressive mode reports parity zero. CURRENT writes
clear VI's MI pending interrupt immediately but do not change sampled
CURRENT. The source documents the interrupt-line clear; clearing the MI
pending latch and retaining the sampled readback are explicit software policy.
INTR writes update the active comparison target immediately, without changing
phase or retroactively comparing the already sampled CURRENT. The new target
is used at the next full-line sample. A CURRENT acknowledgment followed by an
INTR write at the same guest tick therefore does not itself retrigger VI.

### Register updates and scanout geometry

When scanout is disabled, accepted register writes update raw readback and the
active set. While enabled, geometry and timing writes update raw readback and
become active at the next field boundary. The boundary emits the packet using
the old active set, then latches pending writes. A write performed after the
boundary at that same guest tick therefore affects the following field.
STATUS mode 0 follows the same next-field rule: the current field completes,
then scanout disables without emitting later fields. A zero V_SYNC starts no
timer. Empty or invalid geometry refuses activation without changing the
prior register or event state.

The v8 geometry policy is
`vi_output_policy=rgba5551-rgba8888-rawrw-q10step-serrate2-nextfield-nf-v2`:

- H_START and V_START pack start in bits 16–25 and end in bits 0–9. Endpoints
  are end-exclusive. Horizontal output span is the difference in pixels;
  vertical half-line span is divided by two, rounding down.
- WIDTH is source row pitch in pixels. X_SCALE and Y_SCALE low 12-bit fields
  are Q10 source steps per output pixel/row; high 12-bit fields are Q10 source
  offsets. Output width and height are the active spans multiplied by 1024
  and divided by the corresponding nonzero step, rounding down. Sample
  coordinate `i` is `floor((offset + i * step) / 1024)`.
- Progressive scan uses those source rows directly. Serrate maps each output
  row to source row `2 * sampled_row + field_parity`; first-field parity is
  zero. No filter is applied (`filter=0`). The whole addressed source span is
  checked against RDRAM before activation, with no clipping.
- STATUS mode 2 reads big-endian 16-bit RGBA5551: each RGB5 channel expands as
  `(value << 3) | (value >> 2)` and alpha bit 0 maps to 0/255. Mode 3 reads
  RGBA8888 bytes: RGB bytes pass through and alpha expands the low five bits
  of byte four using the same five-bit rule. The mode-3 alpha packing is a
  retained software interpretation, not a claim about the ambiguous primary
  16-bit STATUS comment.
- BURST, LEAP, V_BURST, gamma, divot, anti-alias and dither control fields are
  retained in raw register state but do not alter this no-filter output.

Each packet reports its actual source format, address, width, pitch, source
offsets, H/V endpoints, Q10 steps, field and parity, filter, timing and output
policy, profile/build identity, and guest start/end ticks. Active and pending
register banks, phase units and remainder, next event boundary, current/field
state, latch state, event generation and produced packet metadata are included
in the v8 canonical digest. Observer queue cursors and host buffers remain
excluded. A full queue preserves the original scheduled field event; copying
one packet frees capacity without advancing guest time or changing the digest.

The small authored witness uses H_SYNC 15 and V_SYNC 8: one field is 64 source
cycles and the first deadline is 83 guest ticks. CURRENT reaches half-line 2
after 16 source cycles, at tick 21; INTR=2 raises the VI pending bit there. A
separate H_SYNC 0, V_SYNC 1 stress vector exercises sub-tick field periods and
lossless queue resumption. These are deterministic software expectations,
not standard NTSC-mode or hardware-accuracy results.

## Legacy v2 diagnostic profile

The following section records the original bounded scanout path for profile
`ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v2`; its v1 timing and output policies
remain unchanged.

## Source facts and model policy

Nintendo SDK `rcp.h` documents the VI register offsets and the NTSC clock
constant `VI_NTSC_CLOCK = 48681812`. It identifies STATUS, ORIGIN, WIDTH,
CURRENT, BURST, V_SYNC, H_SYNC, H_START, V_START, X_SCALE and Y_SCALE registers.
The constant is a sourced input; it does not establish how Tendies64's guest
tick maps to physical scan lines or prove this implementation's timing is
hardware-accurate.

The v2 profile binds these model values:

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
