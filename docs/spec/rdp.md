# RDP fill, TMEM, copy and triangle software contract

This document describes five registered composite profiles. V2
`ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v2` (descriptor6/model11/state16)
owns DPC/VI/TMEM/copy; v3 `ntsc-cart-4m-openipl3-rdram-b-t1-rdp-triangle-v3`
(7/12/17) adds edges/shading; v4 `ntsc-cart-4m-openipl3-rdram-b-t1-rdp-texture-v4`
(7/13/18) adds textures; v5 `ntsc-cart-4m-openipl3-rdram-b-t1-rdp-combiner-v5`
(8/14/19) adds the combiner; v6 `ntsc-cart-4m-openipl3-rdram-b-t1-rdp-blender-v6`
(9/15/20) adds blending, address-owned coverage and RGBA8888 scanout. Earlier
registered profiles preserve their behavior. This is a bounded software
contract; full RDP support, hardware timing and hardware accuracy remain
unqualified.

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
| SetPrimColor | `0x3a` | primitive RGBA and primitive LOD fraction |
| SetEnvColor | `0x3b` | environment RGBA |
| SetCombine | `0x3c` | raw cycle-0/cycle-1 RGB and alpha selector groups |
| SetFogColor | `0x38` | fog RGBA |
| SetBlendColor | `0x39` | blend RGBA |
| SetKeyGB / SetKeyR | `0x2a` / `0x2b` | key center, scale, and width parameters |
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
retain the `DP_UNAVAILABLE` refusal. The v2 profile binds
`dpc-fetch-effect-tmem-copy-row-event-1tick-v3`; its future-affecting RDP,
TMEM, tile, TLUT, conversion, VI, and inherited state remains serialized in
the v16 digest. The v3 and v4 profiles add versioned triangle state while
preserving the older profile identities and state bytes. Host output cursors
and buffers remain outside canonical guest state.

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

Textured triangles use the v4 profile and bind
`triangle-twos-complement-q16-logical-coverage-texture-policy-v1`. V4 keeps the
same per-instance coverage plane and adds no per-device or host-global state;
the plane costs 2,097,152 bytes per v4 machine. Its digest domain is
`t64.state.rdp.vi.texture.18` and includes the fetched triangle packet, TMEM,
tile descriptors, active raster state, and coverage plane. V3 remains the
edge/shade profile and retains its previous identity and digest domain.

V4 renders only edge-plus-texture `0x0a` and edge-plus-shade-plus-texture
`0x0e`, in addition to the v3 `0x08` and `0x0c` cases. The four Z-bearing
variants `0x09`, `0x0b`, `0x0d`, and `0x0f` remain refused. Texture coefficient
lanes use the primary packet order: for `0x0a`, S/T/W bases and derivatives
occupy W4–W11; for `0x0e`, the preceding shade group occupies W4–W11 and the
texture group occupies W12–W19. The primary sources name the coefficients and
derivative directions but do not settle raw coefficient signedness, Q16
precision, anchor, accumulation order, division rounding, or W-zero/negative
hardware behavior. D-11 supplies the provisional signed 32-bit two's-complement
Q16 policy; these details remain software behavior, not vendor facts.

For each covered pixel center, v4 evaluates S/T/W from the edge anchor using
signed 64-bit products and explicit floor division. It refuses the whole
triangle before writing if any bounded intermediate cannot be proven safe, if
W is zero or its clipped range crosses zero, if a projected coordinate leaves
signed Q16, or if the selected tile/TMEM span is invalid. Affine mode uses S/T
directly. Perspective mode computes `floor(S*65536/W)` and the corresponding T
value; negative W is accepted when the clipped W range stays negative.

Tile shifts, SL/TL-relative coordinates, clamp, mask and mirror are applied
before sampling. Point mode chooses the containing texel. Box mode averages
four channels with floor division by four (including its one-bit alpha); use away from the documented
four-texel center is a named software extension. Three-point mode uses the
three nearest samples with the Figure 15-5 triangle weights and floors each
channel; exact ties choose the first triangle. These weights, tie choice, and
rounding are implementation policy rather than printed vendor equations.
Raw W0 bits50:48 select the primitive tile; bits53:51 select its maximum mip
level (zero declares only the primitive tile). LOD-off uses that named tile.
LOD-on validates the declared chain through primitive tile plus maximum level
before effects; a chain outside descriptors0..7 is refused. Unrelated valid
descriptors cannot extend the header limit.
LOD uses the maximum absolute adjacent perspective-corrected S/T difference,
with minimum 0.5, bounded integer log2 level selection, and Q0.8 fraction.
Detail and sharpen use the two selected tile levels with floor-rounded channel
blends; enabling both is refused. V4 supports RGBA16/32, YUV16, CI4/8 with a
valid TLUT, IA4/8/16, and I4/8 triangle sampling. Shade-texture mode modulates
5-bit RGB by interpolated 8-bit shade with floor division by 255; alpha is set
when the texture alpha bit is set and shade alpha is at least 128. All of these
rounding and combiner choices are deterministic software policy.

The registered `unit.core.rdp.texture` cases independently cover fetched
LoadTile input through raw VI, affine/perspective sampling, point/box/three-point
filters, shade texture, LOD/detail/sharpen, tile mirror boundaries, odd TMEM
rows, the supported format/size matrix, negative gradients, W-zero and signed
negative-W policy, fractional ties, final-group truncation, old-profile
refusal, and split/interleaved digest behavior. They establish software
consistency only; hardware texture precision and output remain unqualified.

SGI's [RDP command summary, Tables 12–15 and diagrams](https://ultra64.ca/files/documentation/silicon-graphics/SGI_RDP_Command_Summary.pdf)
and [Patent 6,556,197, Figures 65–76](https://patents.google.com/patent/US6556197B1/en)
reconcile the raw 0x08–0x0f layouts. In each present group the word counts are
Edge 4, Shade 8, Texture 8, and Z 2, appended in that order. Therefore the
full packet sizes are 4, 6, 12, 14, 12, 14, 20, and 22 64-bit words for
opcodes 0x08 through 0x0f. Every packet is length-checked against both DPC_END
and its selected RDRAM/DMEM bank before any triangle effect. V3 renders
edge-only 0x08 and edge-plus-shade 0x0c. V4 adds edge-plus-texture 0x0a and
edge-plus-shade-plus-texture 0x0e; the four Z-bearing variants remain refused
without drawing. V2 refuses all triangle opcodes at the first word.

Primary diagrams label the 14-bit Y fields s11.2 and identify the
integer/fraction half positions. The inspected primary sources do not settle
the bit encoding of signed Y or the other signed coefficients. D-11 therefore
defines Y as 14-bit two's-complement divided by four, and the v3 software
policy joins the other edge and shade integer/fraction pairs as signed
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
sample mask is retained in the canonical, digest-covered logical coverage
plane. The last-sample diagnostic fields are separate internal observations;
the plane has no physical address/format mapping yet. This grid and tie
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

## Color combiner profile and software policy

V5 requires a latched SetCombine before any triangle; otherwise it refuses at
that triangle with RENDER_RANGE before scheduling pixel, coverage or noise
effects. Earlier fill/copy work remains, and fill/copy bypass this requirement.

The v5 profile adds a bounded color-combiner implementation for supported edge
and textured triangle pixels. It retains v4 texture sampling and the v3 edge
and shade paths; fill and copy continue to bypass the combiner. Unsupported
cycle types and unimplemented triangle packet forms are refused before
framebuffer writes. The implementation accepts cycle 0 (one-cycle) and cycle
0 followed by cycle 1 (two-cycle), with the first clamped RGBA result available
as `Combined` feedback in cycle 1.

The primary selector table and raw SetCombine bit positions are recorded in
`07-06-RAW-CONTRACT.md`. This profile binds the reversible software policy
`rgba8-floor255-clamp-feedback-noise1-pixel-zero-v1`: sources and intermediate
results use 8-bit RGBA; C uses denominator 255; signed products are divided
with floor rounding; each evaluated cycle clamps to 0–255. One-cycle mode uses
the cycle-0 group once, even when the two packed groups differ. This follows
the explicit software choice for the first profile, not a vendor guarantee.

Noise uses per-instance xorshift32 seeded with 1. Each accepted covered pixel
advances the generator once, and its low byte is reused for selected noise
channels and both cycles. Refused commands consume no noise. Key and convert
sources are decoded from their command fields. K4/K5 are sign-extended from
nine bits and clamped to0..255 before selection; this is v5 software policy,
not a vendor precision claim. Primitive LOD fraction is SetPrimColor bits39:32,
distinct from its adjacent minimum-level field. Undefined color sources resolve
to zero under the profile's policy. These are deterministic software rules,
not claims about exact hardware arithmetic, precision, or noise cadence.

Combiner registers, selectors, key parameters, and RNG state are included in
the v19 digest domain `t64.state.rdp.vi.combiner.19`; the descriptor hashes the
combiner policy. Older profiles keep their existing descriptor bytes and
digest domains. The `unit.core.rdp.combiner` suite includes a literal fetched
DPC-to-triangle-to-RDRAM/VI consumer, independent one- and two-cycle arithmetic
vectors, selector denominator checks, refusal-without-pixel-or-noise effects,
and interleaved instance/digest controls. It establishes this software
contract only; VID-04 hardware accuracy remains unproven.

## V6 blender and logical coverage policy

V6 uses descriptor 9, machine model 15, state 20 and the profile ID
`ntsc-cart-4m-openipl3-rdram-b-t1-rdp-blender-v6`. It preserves v5 behavior
for the older profile. The raw field positions and primary-source limits are
captured in `07-07-BLENDER-FIELDS.md`; software choices below are versioned
policy and are not hardware-accuracy claims.

The existing 16-sample software mask remains a separate 1024×1024 plane. Its
population `n` maps to logical coverage `floor((n+1)/2)`, giving values 0–8.
This rounds half upward and does not claim the vendor's coverage-dither mask.
A second per-machine sidecar has one byte per physical 16-bit RDRAM granule:
bit 7 marks the entry valid, bits 3:0 hold logical coverage 0–8, and bits 6:4
are zero. Invalid entries read as zero. RGBA16 reads one granule; RGBA32 reads
the minimum of its two halfword entries and returns zero if either is invalid.
An RDP output authors one entry for 16-bit pixels or both entries for 32-bit
pixels. Partial byte writes invalidate only the touched granules, and masked
writes invalidate only selected lanes. This is a logical software mapping;
it does not assign a byte address or bit lane to the hardware's physical
hidden RDRAM bits.

V6 fill and copy pixels author full logical coverage. Guest-state reset during
ROM admission clears the sidecar; `t64_machine_destroy` releases its enclosing
RDRAM allocation. A checked allocation accounts for odd RDRAM lengths with
ceiling division. The v20 digest hashes all sidecar bytes after
RAM and the existing sample plane, under the domain
`t64.state.rdp.vi.blender.20`. CPU scalar/masked writes, committed cache
writeback, PI completion and both SP-to-RDRAM DMA paths invalidate only after
the backing bytes are accepted. Cached stores remain invisible until writeback.

Blender RGB selectors use pixel, memory, blend-register and fog-register
colors. Alpha selectors use pixel alpha, fog alpha, shade alpha or zero.
`alpha8 >> 3` gives the 5-bit A factor; B is `32-A`, memory alpha `>>3`, 32,
or zero. Each cycle computes `floor((A*P+B*M)/(A+B))`, clamps to RGB8, and
returns RGB zero when the denominator is zero. Cycle 1 P=0 feeds back the
already normalized cycle-0 result. The selector fields are decoded separately
for both cycles. These conversions and normalized feedback are software
policy; primary text only describes the numerator feedback case with B=1-A.

The rest of the v6 mode behavior is pinned as follows:

- `image_read_en=0` supplies black memory color and zero old coverage;
  `image_read_en=1` reads the selected color image and logical sidecar.
- `force_blend` always enables blending. Otherwise antialias permits it for
  nonzero partial coverage (1–7); full coverage passes pixel RGB directly.
- `cvg_times_alpha` maps coverage to
  `floor((coverage*pixel_alpha+127)/255)`. `alpha_cvg_select` maps that result
  back to alpha8 with `floor((coverage*255+4)/8)` before alpha mux selection.
- Alpha compare accepts equality. Its ordinary threshold is blend-register
  alpha; when `dither_alpha_en` is set, it uses the low byte of the existing
  per-pixel xorshift state without advancing the generator again. Rejection
  suppresses color/alpha writes while coverage destination state still
  updates.
- Coverage destinations are clamp `min(8, old+new)`, wrap zero to zero and
  otherwise `1+((sum-1)%8)`, full 8, and save old. Overflow means `sum>8`;
  exact 8 is not overflow. `color_on_cvg` suppresses the color write when
  overflow is false, while coverage still updates. A resulting coverage of
  zero suppresses the write.
- RGB dither uses these 4×4 matrices (magic-square and Bayer); noise uses the
  current xorshift state's low three bits, and disabled contributes zero:

  ```text
  magic:       Bayer:
  0 6 1 7      0 4 1 5
  4 2 5 3      6 2 7 3
  1 7 0 6      1 5 0 4
  5 3 4 2      7 3 6 2
  ```

  RGB8 channels add that 0–7 bias, clamp to 255, then take the upper five
  bits. Alpha dither uses the first matrix for pattern, its inverse for
  inverted pattern, noise low three bits, or zero when disabled. RGBA5551
  alpha compares against `128 + bias - 4` (or 128 when disabled); RGBA8888
  alpha adds the same signed bias before taking five bits. These are explicit
  deterministic costs/ties, not vendor dithering equivalence.
- RGBA16 stores RGB5551 and logical coverage only in the sidecar. RGBA8888
  stores RGB8, then alpha bits 7:5 as `coverage-1` and alpha bits 4:0 as the
  v6 five-bit alpha; VI expands the low five alpha bits and ignores the
  coverage code. No 16-bit hidden-bit packing is inferred.

The literal two-cycle packet checks a five-sample pixel, hence logical
coverage 3, with old coverage 6. Its wrap result is 1 and `color_on_cvg`
allows the expected RGB output. Registered tests also cover the RGBA8888
consumer path. The authored results establish this versioned software
contract only; they do not complete VID-04 or replace the active D2/D5/D6
stops.

## V7 depth policy

V7 is a new composite profile (`ntsc-cart-4m-openipl3-rdram-b-t1-rdp-depth-v7`,
descriptor 10, machine model 16, state serialization 21). It keeps the v6
combiner, blender, color sidecar, and VI rules while binding the following
depth-specific software policy. Earlier profile identifiers, semantics, and
serialized bytes remain unchanged.

The raw Set Z Image address is an aligned byte address. A Z pixel has the same
linear index as its color pixel, using the color image width; one 16-bit
visible word is stored per pixel. The software hidden sidecar has one byte per
RDRAM halfword: bit 7 means valid, bits 1:0 contain the low two logical dZ
bits, and bits 6:2 are zero. Depth writes set this entry. Any accepted backing
write by CPU, PI, SP, or RDP clears only entries overlapping its written
halfwords, after the bytes commit. The sidecar is allocated with checked
ceiling division, reset with guest state, and included in v21 canonical
digests. This is a logical model; it makes no claim about physical hidden-bit
packing.

The software numeric policies are explicit and reversible:

- Triangle Z and derivatives join each literal signed integer/fraction pair
  as two's-complement Q16.16. At a pixel center, the plane is evaluated as
  `base + floor(dx*(x-major)/65536) + floor(de*(major-xh)/65536) +
  floor(dy*(y-yh)/65536)`. The 18-bit `0,15.3` conversion uses signed floor
  division by 8192, then clamps to `0..0x3ffff` before any write.
- Primitive Z is `min(raw16, 0x7fff) << 3`; its fractional three bits are
  zero. Primitive DeltaZ is unsigned. Per-pixel delta is
  `ceil((abs(dx)+abs(dy))/8192)`, clamped to 16 bits. The pixel delta and
  stored delta are compared by their maximum.
- The primary eight-row exponent/mantissa decode table is applied exactly.
  Compression selects the containing exponent range, floors the mantissa,
  and clamps at the 18-bit bounds. The visible word stores exponent, mantissa,
  and the high two dZ bits. The low two bits go to the sidecar. There is no
  guessed physical address mapping.
- Opaque mode passes when `newZ <= oldZ + maxDZ`, saturating the right side at
  `0x3ffff`; equality passes. Interpenetrating mode also passes when incoming
  logical coverage exceeds stored color coverage. Transparent mode uses the
  opaque comparison and never updates depth, even when `z_update_en=1`.
  Decal mode passes only when `abs(newZ-oldZ) <= maxDZ` and never updates
  depth, even when `z_update_en=1`. Compare disabled passes; update disabled
  never writes. These mode rules and the update override are software policy,
  not confirmed silicon behavior.
- Any v6 blender result that suppresses the color write, including alpha
  rejection or color-on-coverage/save coverage, also suppresses depth update.
  The existing v6 destination-coverage result remains observable. Depth
  comparison occurs before committing output writes; rejected or unselected
  output lanes do not alter depth.
- An uninitialized visible word is read from RDRAM as stored and an invalid
  hidden entry contributes low dZ zero. `0xfffc` therefore decodes to the
  saturated far value under the documented table. No hidden value is inferred
  from neighboring pixels.

The authored depth matrix pins the exact raw words and independently expected
values for every compression row and transition, zero/maximum delta and
priority boundary, less/equal/greater comparison, four distinguishable depth
modes, compare/update combinations, coverage and alpha interactions, all
eight triangle packet lengths and their final Z-group words, atomic truncation
and bounds refusals, 16/32-bit aliases, external-write invalidation, reset,
split/interleaved state, digest sensitivity, and FullSync ordering. These
results qualify only this v7 software contract; they are not VID-04 or
hardware-accuracy evidence.
