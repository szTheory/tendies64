# RDP fill, TMEM and copy tracer contract

This document describes the bounded RDP software model in profile
`ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v2` (descriptor 6, machine model 11,
state serialization 16). It covers fetched DPC commands, texture loads, and
point-sampled copy rectangles. It does not claim full RDP support, hardware
timing, or hardware accuracy.

## Primary source facts

Nintendo's [tile descriptor section](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro13/13-05.html)
defines SetTile's format, size, line, TMEM address, palette, clamp, mirror,
mask, and shift fields. SetTileSize and each load command also set the tile's
SL/TL/SH/TH bounds; LoadTile's bounds describe the loaded tile, while
LoadBlock and LoadTLUT bounds may need a later SetTileSize.

The [TMEM organization section](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro13/13-08.html)
describes a 4 KiB texture store, split into low and high 2 KiB halves. CI
indices use the lower half and TLUT entries use the upper half; CI4 prepends a
tile palette number, and the palette is replicated across four banks. YUV16
loads contain paired Y samples with shared U/V. The manual also shows
big-endian texel layouts and separate layouts for 4-, 8-, 16-, and 32-bit
formats.

The [texture loading section](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro13/13-09.html)
describes LoadTile row padding to 64-bit TMEM words, LoadBlock as a linear
stream whose DXT accumulator advances the line count, and LoadTLUT's expansion
of each 16-bit entry into four copies in one 64-bit word. It identifies both
RGBA16 and IA16 TLUT values and a maximum LoadBlock size of 2048 texels.

Nintendo's [texture restrictions](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro13/13-11.html)
state that copy mode does not support direct 32-bit RGBA or YUV tiles, that
copy mode implicitly disables clamp, and that wrap or mirror applies to 4-,
8-, and 16-bit texels. The model refuses direct RGBA32/YUV copy tiles. A
caller can describe RGBA32 or YUV source bytes as RGBA16 when intentionally
copying their 16-bit halves, as the manual recommends.

The [texture rectangle section](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro14/14-01.html)
defines a rectangle using screen-space bounds and start S/T plus DSDX/DTDY.
Its 100-by-100 example uses lower-right bounds at `(200,200)` for an origin of
`(100,100)`. The section says copy mode transfers four horizontal texels per
clock and uses an S step of four. The [SetConvert reference](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/n64man/gdp/gDPSetConvert.htm)
defines six signed 9-bit coefficients and separates texture-filter YUV
conversion from color-combiner postprocessing. The [texture filter section](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro12/12-05.html)
describes those pipeline stages.

The source command fields used here are pinned as follows. Values in the
operation column are the raw six-bit RDP command fields retained from the
original fetched word.

| Command | Raw operation | Fields consumed by this model |
| --- | ---: | --- |
| SetTextureImage | `0x3d` | format, size, width minus one, RDRAM address |
| SetTile | `0x35` | format, size, line words, TMEM word, tile, palette, clamp/mirror/mask/shift |
| SetTileSize | `0x32` | tile and 10.2 `ULS/ULT/LRS/LRT` bounds |
| LoadTile | `0x34` | tile and rectangular 10.2 source bounds |
| LoadBlock | `0x33` | tile, source start, texel count, DXT |
| LoadTLUT | `0x30` | tile and palette source bounds |
| SetConvert | `0x2c` | signed 9-bit `K0..K5` fields |
| TextureRectangle / Flip | `0x24` / `0x25` | screen bounds in the first word; S/T/DSDX/DTDY in the second word |

## Per-machine texture state

Each machine owns 4096 bytes of TMEM, eight tile descriptors, the texture
image descriptor, TLUT format/validity, conversion coefficients, and any
pending two-word texture rectangle. Loads and sampling use bounded integer
arithmetic. A malformed or truncated packet is refused before its load or
rectangle writes TMEM or framebuffer pixels. LoadTile validates the complete
RDRAM source and padded TMEM rows before copying any texel. LoadBlock validates
each DXT-selected 64-bit destination word before changing TMEM. LoadTLUT
validates the complete source span and high-half destination range before
replicating any entry.

The model stores 4-bit values as high/low nibbles, 8-bit values as bytes, and
16-/32-bit values in big-endian byte order. LoadTile rows use the tile's 64-bit
line stride and swap the two 32-bit halves on odd rows. LoadBlock advances its
DXT accumulator once per 64-bit source word and maps each word into the
selected tile line; odd-line half swapping is applied after the destination
line is selected. TLUT loads accept RGBA16 and IA16 sources, replicate each
entry four times, and retain the source interpretation for CI copy sampling.

Supported load formats are RGBA16/32, YUV16, CI4/8, IA4/8/16, and I4/8. Copy
sampling accepts RGBA16, CI4/8, IA4/8/16, and I4/8; direct RGBA32 and YUV
copies are refused. Copy rectangles require the documented DSDX step of
`4 << 10`, use point sampling, apply S/T mask, mirror, or clamp before TMEM
addressing, clip to the scissor and color-image bounds, and write RGBA16
pixels to RDRAM. Texture S coordinates are relative to the selected tile's
ULS bound after wrapping/clamping. The copy path does not implement filtering,
combining, alpha compare, depth, coverage, or the rest of the RDP pipeline.

YUV16 bytes are loaded and retained in the paired source layout. The small
`t64_rdp_tmem_convert_yuv` helper exercises the texture-filter stage only: it
centers U/V at 128, applies signed `K0..K3` with integer division by 128, and
clamps each output channel to 0–255. This is the named deterministic software
policy used by the authored vector; it does not implement the color-combiner
`K4/K5` stage and is not used to make YUV legal in copy mode. The authored
YUV vector is therefore load-plus-conversion-state evidence, not a rendered
YUV pixel claim.

## RDP transport and profile identity

The composite profile maps DPC START, END, CURRENT, STATUS, and the four
read-only counters at `0x04100000..0x0410001c`. START/END must be 8-byte
aligned. With XBUS clear, complete command words must fit in RDRAM; with XBUS
set, they must fit in the selected 4 KiB SP DMEM bank. A two-word texture
rectangle is prevalidated as a whole before its first word starts a copy.
DPC fetch, command effect, raster rows, and FullSync completion are separate
scheduled events. Counter writes are refused; STATUS clear commands clear
24-bit counters.

Fetched RSP MFC0/MTC0 instructions for COP0 registers 8 through 15 use the
same DPC accessors as CPU MMIO on this composite profile. Earlier profiles
retain the `DP_UNAVAILABLE` refusal. The profile version and
`rdp_work_policy` (`dpc-fetch-effect-tmem-copy-row-event-1tick-v3`) identify
the new TMEM/copy state and its event ordering. All future-affecting RDP,
TMEM, tile, TLUT, conversion, VI, and inherited machine state is serialized
into the v16 digest. Host output cursors and buffers remain outside canonical
guest state.

## Authored evidence and limits

The registered `unit.core.rdp.copy` matrix reaches these cases through fetched
DPC commands:

- `load-tile-layout`, `load-block-dxt`, `load-tlut-ci`, `load-tlut-boundary`
- `rgba-16-32`, `ia-i-4-8-16`, `truncated-load`, `yuv-conversion`
- `copy-rect-flip`, `copy-tile-origin`, `tile-wrap-mirror-clamp`
- `tmem-first-last`, `adjacent-tile-no-alias`, `scissor-touch`, `single-texel`,
  `zero-area`, `bad-format`, `truncated-copy`, `overflow-rejected`,
  `copy-first-difference`

The expected TMEM bytes, RGBA16 RDRAM values, raw VI packet and rejection
effects are authored literals derived from the cited layouts and equations;
they are not produced by the implementation under test. The accepted fill
fixture remains a separate RDP/VI path. The implementation is not
differential-tested against an external renderer. No protected guest, oracle,
console test, or VID-04 qualification evidence is used here. This slice
contributes implementation and authored evidence to VID-01 and VID-05 but
completes neither requirement. The D2 external-rights and D6 release stops
remain active.
