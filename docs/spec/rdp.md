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

Nintendo SDK `rcp.h` revision 1.21 lists DPC START, END, CURRENT, STATUS and
the four read-only counters at `0x04100000` through `0x0410001c`. It defines
the STATUS write commands for XBUS, freeze, flush and counter clears, plus the
read bits for XBUS, freeze, flush, GCLK, TMEM, pipe, command, command-buffer,
DMA and START/END valid state. These are register definitions; they do not
specify Tendies64's per-event guest-time costs. See [`rcp.h`, DP Command
Registers](https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm).

Nintendo's GBI header defines the full command bytes `0xE6` through `0xE9` for
LoadSync, PipeSync, TileSync and FullSync, and says the RDP uses the command
byte's low six bits. The resulting raw RDP fields are `0x26` through `0x29`.
The SGI summary's Sync Load label `0x31` is retained as a source discrepancy;
Tendies64 follows the Nintendo definition for the supported LoadSync path.
FullSync's DP event indicates that the final RDP pipeline command completed.
See [`gbi.h`](https://ultra64.ca/files/documentation/online-manuals/man/header/gbi.htm),
[`gDPFullSync`](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/n64man/gdp/gDPFullSync.htm)
and [RDP synchronization §12.2](https://ultra64.ca/files/documentation/online-manuals/man-v5-2/allman52/pro-man/pro12/12-02.htm).

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

The implementation admits only this command subset. Each DPC work event
fetches one complete 64-bit word and advances CURRENT; a later event applies
that command's effects. A Fill Rectangle then schedules one row per guest
tick, and a FullSync fence completes on its own later event after earlier
fill rows. DPC CURRENT reaching END therefore does not imply command effect or
fill completion. PipeSync, TileSync and LoadSync retain distinct decoded
commands and ordered fetch/effect boundaries. Since this path implements no
TMEM loads, tile state or other primitive classes, these sync commands have no
additional modeled drain effect. MI owns the DP pending bit, mask and guest
clear. Unknown opcodes stop at their encountered word without applying its
effects or synthesizing a FullSync.

## DPC transport and status policy

The selected composite profile maps DPC `+0x00` through `+0x1c` as eight
32-bit registers. START and END must be 8-byte aligned and within the selected
command source. With XBUS clear, a complete word must fit in RDRAM; with XBUS
set, the command source is the profile's 4 KiB SP DMEM bank. This software
model refuses a command word that crosses the end of that bank; it does not
wrap a DPC packet across DMEM. An incomplete word at END is refused before
that word has effects. Writing a counter register is refused; STATUS counter
clear commands clear the corresponding 24-bit read-only counter.

In machine model 10/state domain 15, fetched RSP MFC0/MTC0 instructions for
COP0 registers 8 through 15 use these same DPC read/write handlers and offsets
as CPU MMIO. MFC0 exposes START, END, CURRENT, STATUS and all four counters;
MTC0 may write START, END or STATUS, while CURRENT and the counters remain
read-only. Before this composite profile, fetched RSP accesses to COP0
registers 8 through 15 retain the `DP_UNAVAILABLE` refusal and do not dispatch
DP work.

STATUS is composed from separately retained transport state. XBUS, freeze,
flush and START/END valid reflect their DPC control state. START_GCLK is set
while work is active and unpaused; CMD_BUSY reflects a pending command effect
or unfetched command; DMA_BUSY reflects remaining command input; PIPE_BUSY
reflects raster rows in progress. CBUF_READY is always reported by this
bounded model, while TMEM_BUSY remains clear because no TMEM operations are
implemented. Freeze and flush pause queued RDP callbacks without consuming
the pending command, raster row or fence; clearing both resumes the retained
work through the scheduler.

The four counter values wrap at 24 bits. The software increments CLOCK for
fetch, effect, raster-row and FullSync events, BUFBUSY for command fetches,
PIPEBUSY for fill rows and leaves TMEM at zero. These units are deterministic
model accounting, not measured RDP hardware clocks. Event reservations occur
before each committed state or pixel effect; the existing scheduler orders
same-tick callbacks by sequence.

Profile-bound policy fields are:

- `rdp_work_policy`: `dpc-fetch-effect-fill-row-event-1tick-v2`.
- `rdp_noise_policy`: `xorshift32-seed-1-v1`.
- `rdp_undefined_color_policy`: `zero-color-v1`.

These are versioned software choices. The composite profile uses machine model
version 10 and state serialization version 15 for the added fetch/effect
stages, DPC controls, counters and fence generation. State that can affect
future RDP output, including command progress, the pending word, fill bounds,
policy state and fences, is included in the versioned machine digest. Host
runner cursors and file output are not guest state. Pre-composite profiles
retain their earlier DP-unavailable RSP behavior.

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
