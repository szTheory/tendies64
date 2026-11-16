# RDP fill tracer contract

This document describes the first bounded RDP path for Tendies64's
`ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v1` profile. It is a named software
model and authored regression vector. It does not claim full RDP support,
hardware timing, or hardware accuracy.

## Source facts

The retained SGI RDP Command Summary v2.0 is an ignored local research input at
`build/research/07-primary/SGI_RDP_Command_Summary.pdf`. Its command field is
bits 61–56 (pp. 2 and 21). Table 16 (p. 21) gives Fill Rectangle's `XL` and
`YL` in bits 55–44 and 43–32 and `XH` and `YH` in bits 23–12 and 11–0; all four
coordinates are 10.2 values, with `XL/YL` described as the bottom-right corner
and `XH/YH` as the top-left corner. That table does not specify fractional
sample rounding or endpoint inclusion. Table 25 (p. 34) defines Set Fill
Color's packed bits 31–0 and says that for a 16-bit RGBA color image those bits
represent two horizontally adjacent 16-bit pixels. Table 27 (p. 36) documents
the scissor box's 10.2 fields. The Full Sync description (p. 41) waits for
preceding primitive buffers to be read and written.

Nintendo's `gbi.h` names the full GBI command byte `G_SETFILLCOLOR` as `0xF7`
and documents the lower-six-bit direct operation field. Thus `0xF7 & 0x3f`
and direct field `0x37` select Set Fill Color. Tendies64 retains the original
word and command byte for observation; it does not rewrite fixture bytes to
normalize the opcode.

## Implemented software policy

The fixture issues these original big-endian 64-bit words through ordinary
CPU stores to uncached RDRAM and DPC START/END registers:

| Word | Direct operation | Fields used by this tracer |
| --- | --- | --- |
| `3f10000300003000` | Set Color Image (`0x3f`) | RGBA, 16-bit, width 4, address `0x3000` |
| `2d0000000000c00c` | Set Scissor (`0x2d`) | top-left `(0,0)`, bottom-right `(3,3)` in 10.2 |
| `2f30000000000000` | Set Other Modes (`0x2f`) | fill cycle selector |
| `37000000f80107c1` | Set Fill Color (`0x37`) | packed halfwords `0xf801`, `0x07c1` |
| `3600c00800000000` | Fill Rectangle (`0x36`) | bottom-right `(3,2)`, top-left `(0,0)` |
| `2900000000000000` | Full Sync (`0x29`) | completion fence after prior fill rows |

The current model interprets whole-pixel 10.2 rectangle endpoints as inclusive
for this fill command. Its authored `(0,0)` through `(3,2)` vector therefore
writes 4 × 3 16-bit pixels. The model writes high halfword `0xf801` at even
horizontal positions and low halfword `0x07c1` at odd positions, in
big-endian RDRAM byte order. These endpoint and byte-order statements describe
the implementation policy tested here; the cited SGI tables alone do not
specify fractional rounding or endpoint inclusion.

The implementation admits only this command subset. It fetches one complete
64-bit word before applying that command's effects, schedules fill rows as
bounded one-guest-tick work, and schedules Full Sync completion behind the
fill. DPC CURRENT reaching END does not imply fill completion. MI owns the DP
pending bit, mask and guest clear. Unknown opcodes refuse the command stream;
they do not synthesize a frame or Full Sync.

Profile-bound policy fields are:

- `rdp_work_policy`: `fill16-row-event-1tick-v1`.
- `rdp_noise_policy`: `xorshift32-seed-1-v1`.
- `rdp_undefined_color_policy`: `zero-color-v1`.

These are versioned software choices. State that can affect future RDP output,
including command progress, fill bounds, policy state and fences, is included
in the machine digest. Host runner cursors and file output are not guest state.

The focused tests separately compare the six RDRAM words and the raw VI
packet. The supervised positive fixture runs through the ordinary CPU, bus,
machine scheduler and actual headless runner. Its same-setup wrong-pixel
control changes one expected byte and must reach the pixel comparator.

## Limits and evidence

Only the six listed direct commands and the one RGBA16 fill path are covered.
Copy, texture, triangle, combiner, blender, coverage and depth behavior remain
for later plans. The implementation is not differential-tested against an
external renderer. VID-04 remains blocked; this tracer does not satisfy it.
The retained SGI PDF is a local research locator and is not redistributed.
