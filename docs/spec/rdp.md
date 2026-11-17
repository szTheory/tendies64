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

LoadBlock's last-texel field is not scaled like LoadTile: the primary
[SDK texture-block producer](https://ultra64.ca/files/documentation/online-manuals/man/header/gbi.htm)
passes the final texel index directly. A zero-origin eight-texel transfer uses
raw LRS=7. Tile Line is a per-T skip count for LoadBlock, with zero selecting
contiguous words ([SGI usage notes, PDF page9](https://hcs64.com/files/RDP_COMMANDS.pdf#page=9)).
The DXT accumulator selects odd-word half swapping; that swap stays within the
word and does not enlarge its validated span.

There is a primary-source inconsistency for the general coordinate description:
SGI labels the LoadBlock coordinates12.0 while Nintendo's low-level function
reference labels them10.2. The SDK's standard producer independently settles
unshifted LRS for zero-origin blocks. This profile retains its existing10.2
source-origin software policy; the zero-origin producer path is tested, and
nonzero-origin hardware conformance is not established by that vector.


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
| SetOtherModes | `0x2f` | cycle type and TLUT enable/type |
| TextureRectangle / Flip | `0x24` / `0x25` | screen bounds in the first word; S/T/DSDX/DTDY in the second word |

TextureRectangle word 0 carries 10.2 `XH/YH/XL/YL`; word 1 carries signed
16-bit `S`, `T`, `DSDX`, and `DTDY` in bits 63:48, 47:32, 31:16, and 15:0.
This copy profile accepts COPY cycle type 2 and the positive `DSDX = 4 << 10`
step. Copy-mode lower-right screen endpoints are inclusive; each 10.2 endpoint
is floored to a whole pixel, so equal endpoints select one pixel. Scissor
lower-right bounds keep their existing exclusive behavior. SetOtherModes TLUT
enable and type are bits 47 and 46; the type bit selects RGBA16 (0) or IA16
(1) when enabled. Other low bits do not enable a TLUT.

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
copies are refused. Copy rectangles require COPY cycle type 2 and the
documented positive DSDX step of `4 << 10`; other DSDX values are refused
before any rectangle pixel is written. S, T, and DTDY use signed 16-bit
interpretation. Coordinate stepping and conversion from 10.5 fixed point use
bounded signed arithmetic with explicit floor division, including negative
steps. Starting S/T values advance with the rectangle when scissor clipping
removes pixels from its left or top edge. Sampling is point-based, applies the
selected tile's software mask, mirror, or clamp policy before TMEM addressing,
and writes RGBA16 pixels to RDRAM. The copy path does not implement filtering,
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

## Triangle profile and software raster policy

Triangle commands use the separately versioned
`ntsc-cart-4m-openipl3-rdram-b-t1-rdp-triangle-v3` profile (descriptor 7,
machine model 12, state serialization 17). Its descriptor binds
`triangle-twos-complement-q16-logical-coverage-mask-v1`; v2 retains its prior command refusals,
descriptor bytes, and v16 digest. The v3 state digest uses
`t64.state.rdp.vi.triangle.17` and includes pending fetched packet words,
primitive color, active triangle state, and a machine-owned 1024x1024 plane of
16-bit sample masks (2,097,152 additional bytes per v3 machine instance). The
plane is indexed by logical raster `(x,y)` at fixed stride 1024; nonempty
triangle masks accumulate with bitwise OR until ROM admission resets the
machine. It continues to include the version-16 RDP/VI and RDRAM protocol
state. This is provisional software raster state, not a claim about hidden
coverage-bit packing in RDRAM. Plan07-07 owns the address/format mapping and
CPU, DMA, fill, copy, and raster write invalidation rules.

SGI's [RDP command summary, Tables 12–15 and diagrams](https://ultra64.ca/files/documentation/silicon-graphics/SGI_RDP_Command_Summary.pdf)
and [Patent 6,556,197, Figures 65–76](https://patents.google.com/patent/US6556197B1/en)
reconcile the raw 0x08–0x0f layouts. In each present group the word counts are
Edge 4, Shade 8, Texture 8, and Z 2, appended in that order. Therefore the
full packet sizes are 4, 6, 12, 14, 12, 14, 20, and 22 64-bit words for
opcodes 0x08 through 0x0f. Every packet is length-checked against both DPC_END
and its selected RDRAM/DMEM bank before any triangle effect. This slice renders
only edge-only 0x08 and edge-plus-shade 0x0c; the other six complete layouts
are consumed as validated packets and refused without drawing. V2 refuses all
triangle opcodes at the first word.

Primary diagrams explicitly identify signed 14-bit s11.2 Y fields and the
integer/fraction half positions. Their sign encoding for other coefficient
fields is unstated. The v3 profile therefore applies the owner's software
policy: all other edge and shade integer/fraction pairs are joined as signed
32-bit two's-complement Q16 values. This is an implementation contract, not a
vendor format claim. The SDK notes identify edge-only triangles as
primitive-color operations and say they cannot render in fill cycle; the
renderer requires one-cycle mode and never borrows the fill/copy rectangle
path.

The raster software policy samples each pixel at the sixteen centers whose X
and Y offsets are 1/8, 3/8, 5/8, and 7/8. Y coverage is half-open
`[YH,YL)`. The `lft` bit selects whether the major edge is left or right;
horizontal coverage is left-inclusive and right-exclusive. A pixel is written
when at least one sample is covered; partial coverage is not blended, and the
sample mask is retained only as an internal test diagnostic. This grid and tie
rule are Tendies64 policy because the inspected primary sources do not define
sample positions or tie ownership. Each scheduled row checks at most
1024 × 16 samples, clips to the scissor, validates the complete RDRAM target
range before drawing, and advances only after reserving the next event.

Shade fields are decoded from W4–W11 in the primary row order. For each pixel
center, the software policy computes channel Q16 as the base plus `d/dx`
times the displacement from the major edge, `d/de` times major-edge
displacement from XH, and `d/dy` times Y displacement from YH. Each product
uses signed 64-bit intermediates and explicit floor division; the resulting
channel is floored to an integer, clamped to 0–255, and packed as RGBA5551
(top five bits of RGB; alpha is one at 128 or above). This is a named software
interpolation and packing rule, not a claim about RDP precision, dithering,
coverage blending, or hardware output. The primary numeric gap remains open.
The implementation bounds each shade product before any framebuffer write;
an input whose required signed 64-bit intermediate would exceed that bound is
refused before drawing.

Authored literal raw cases exercise both edge orientations, negative X and
slopes, degenerate/equal-Y and single-sample shapes, complementary masks on a
shared diagonal, signed shade Dx, overlapping command order, a 1024-wide image,
finite extreme coefficients, every coefficient-group truncation boundary,
all complete composite packet lengths, and v2 refusal. These are software
policy tests; they are not hardware qualification or oracle comparisons.

## Authored evidence and limits

The registered `unit.core.rdp.copy` matrix reaches these cases through fetched
DPC commands:

- `load-tile-layout`, `load-block-dxt`, `load-tlut-ci`, `load-tlut-boundary`
- `rgba-16-32`, `ia-i-4-8-16`, `truncated-load`, `yuv-conversion`
- `copy-rect-flip`, `copy-tile-origin`, `tile-wrap-mirror-clamp`
- `tmem-first-last`, `adjacent-tile-no-alias`, `scissor-touch`, `single-texel`,
  `zero-area`, `bad-format`, `truncated-copy`, `overflow-rejected`,
  `copy-first-difference`
- Literal raw-command cases cover COPY-cycle admission, nonzero and negative
  S/T, negative DTDY traversal, RGBA16/IA16/disabled TLUT modes, inclusive and
  fractional screen endpoints, flipped stepping, empty scissor clipping, and
  preservation of the source origin under left/top clipping.

The expected TMEM bytes, RGBA16 RDRAM values, raw VI packet and rejection
effects are authored literals derived from the cited layouts and equations;
they are not produced by the implementation under test. The accepted fill
fixture remains a separate RDP/VI path. The implementation is not
differential-tested against an external renderer. No protected guest, oracle,
console test, or VID-04 qualification evidence is used here. This slice
contributes implementation and authored evidence to VID-01 and VID-05 but
completes neither requirement. The D2 external-rights and D6 release stops
remain active.
