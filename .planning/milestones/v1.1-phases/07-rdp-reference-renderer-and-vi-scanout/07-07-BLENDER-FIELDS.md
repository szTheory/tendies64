# Plan 07-07 Blender Primary Field Preflight

**Checked:** 2026-11-17  
**Scope:** Raw SetOtherModes blend selectors and controls, blender equation/coverage units, and memory coverage descriptions. This pins primary inputs for planning; software arithmetic and sidecar behavior remain versioned profile choices.

## Primary SetOtherModes fields

The 64-bit command is W0=`bits 63..32`, W1=`bits 31..0`. The wire command byte is `0xEF`; its RDP opcode field is six-bit `0x2F` (`bits 61..56`). The SDK producer uses `0xEF` for RDP commands, while SGI Table 20 specifies the `0x2F` field.

| Field | Raw bit(s) | Source-defined meaning |
|---|---:|---|
| `cycle_type` | 53:52 | `00` one-cycle, `01` two-cycle, `10` copy, `11` fill. |
| RGB dither select | 39:38 | `0` magic-square, `1` Bayer, `2` noise, `3` disabled. |
| Alpha dither select | 37:36 | `0` pattern, `1` inverted pattern, `2` noise, `3` disabled. |
| Reserved | 35:32 | SGI says default value `0xF`; this is a reserved-field value, not a reset-state claim. |
| `m1a` / P color | cycle 0: 31:30; cycle 1: 29:28 | Selector values: `0` input pixel RGB (cycle 1: prior-cycle blended RGB), `1` memory RGB, `2` blend-register RGB, `3` fog-register RGB. |
| `m1b` / A alpha | cycle 0: 27:26; cycle 1: 25:24 | Selector values: `0` combiner output alpha, `1` fog alpha, `2` stepped shade alpha, `3` zero. |
| `m2a` / M color | cycle 0: 23:22; cycle 1: 21:20 | Same color-source selector values as P. |
| `m2b` / B factor | cycle 0: 19:18; cycle 1: 17:16 | Selector values: `0` `1-A`, `1` memory alpha, `2` one, `3` zero. |
| `force_blend` | 14 | Force blender enable. |
| `alpha_cvg_select` | 13 | Select coverage or coverage-times-alpha as pixel alpha. |
| `cvg_times_alpha` | 12 | Use coverage-times-alpha for pixel alpha and coverage. |
| `cvg_dest` | 9:8 | `00` clamp/normal, `01` wrap, `10` zap/full, `11` save memory coverage. |
| `color_on_cvg` | 7 | Gate color write on coverage overflow. |
| `image_read_en` | 6 | Enable color/coverage read-modify-write. |
| `antialias_en` | 3 | When force-blend is off, permit blending based on coverage bits. |
| `dither_alpha_en` | 1 | In alpha compare, use random threshold noise when set; otherwise use blend alpha. |
| `alpha_compare_en` | 0 | Enable conditional color write on alpha compare. |

**Sources:** SGI, *Nintendo Ultra64 RDP Command Summary* v2.0, Table 20, printed pp. 27–29, [PDF](https://ultra64.ca/files/documentation/silicon-graphics/SGI_RDP_Command_Summary.pdf); Nintendo, *Nintendo 64 Programming Manual Draft*, §16, “Blender,” pp. 310–317, [PDF](https://ultra64.ca/files/documentation/nintendo/Nintendo_64_Programming_Manual_NU6-06-0030-001G_HQ.pdf); SDK `gbi.h` revision 1.138, SetOtherModes and render-mode declarations, [header](https://ultra64.ca/files/documentation/online-manuals/man/header/gbi.htm).

The SDK producer maps the four per-cycle selectors as `GBL_c1(m1a,m1b,m2a,m2b)` at shifts `30,26,22,18` and `GBL_c2(...)` at `28,24,20,16`. Its source values are `G_BL_CLR_IN=0`, `G_BL_CLR_MEM=1`, `G_BL_CLR_BL=2`, `G_BL_CLR_FOG=3`; `G_BL_1MA=0`, `G_BL_A_MEM=1`, `G_BL_A_IN=0`, `G_BL_A_FOG=1`, `G_BL_A_SHADE=2`, `G_BL_1=2`, `G_BL_0=3`. These producer declarations agree with the manual P/A/M/B input tables. [SDK `gbi.h`, revision 1.138, lines 667–682](https://ultra64.ca/files/documentation/online-manuals/man/header/gbi.htm)

## Blend equation, coverage, and memory facts

- **[PRIMARY FACT]** The manual gives `color = (a*p + b*m)/(a+b)`: `p` and `m` select pixel/memory/register/fog RGB, `a` selects combiner/fog/shade/zero alpha, and `b` selects `1-a`, memory alpha, one, or zero. The two-cycle path has a separate mux set. Cycle-1 P selector `0` receives cycle-0 blended RGB; the text describes this feedback input as cycle 0's numerator and says it works when B is `1-A`. The manual says A/B alpha-mux outputs have 5-bit precision while color channels are 8-bit. It does not specify an exact software conversion/rounding contract for those mux values. [Manual §16, “Color Blend Hardware,” pp. 310–313](https://ultra64.ca/files/documentation/nintendo/Nintendo_64_Programming_Manual_NU6-06-0030-001G_HQ.pdf)
- **[PRIMARY FACT]** Manual §16 describes a 4×4 subpixel mask and dithers its 16 covered/uncovered bits to coverage values up to 8; coverage `8` is full (`1.0`) and `1` is `0.125`. Figure 16-4 labels the coverage dither mask `0xA5A5` and shows popcount examples. Stored memory coverage is `coverage - 1`; reading adds one. Zero coverage means no framebuffer write. The initial-clear guidance sets coverage bits to all ones. [Manual §16, “Coverage Unit” and “Coverage Calculation,” pp. 306–315](https://ultra64.ca/files/documentation/nintendo/Nintendo_64_Programming_Manual_NU6-06-0030-001G_HQ.pdf)
- **[PRIMARY FACT]** SGI defines coverage destinations as clamp, wrap, force-full (“zap”), and save; the SDK uses `CVG_DST_CLAMP=0`, `CVG_DST_WRAP=0x100`, `CVG_DST_FULL=0x200`, and `CVG_DST_SAVE=0x300`. The manual describes wrap as old plus new coverage greater than unity; at exact unity it is not a wrap. Thus, in its logical 0..8 coverage units, a sum of 8 stays 8, while 9 wraps to 1. The manual gives no packed-bit arithmetic for every case. [SGI Table 20, p. 28; Manual §16, “Transparent Surfaces,” pp. 330–331; SDK `gbi.h` lines 648–657](https://ultra64.ca/files/documentation/silicon-graphics/SGI_RDP_Command_Summary.pdf)
- **[PRIMARY FACT]** Nintendo says hidden bits arise from 9-bit RDRAM: they are logically the two low bits of each 18-bit word. In 16-bit RGBA mode they are used for coverage; in 32-bit RGBA mode three coverage bits occupy the three most-significant bits of the 8-bit alpha channel and hidden bits are ignored. The manual says non-RDP masters access only the 16-bit word and can indirectly affect hidden bits by changing its LSB. It does not provide a host byte-address/hidden-bit packing map or fully spell out how the three-bit 16-bit-mode stored coverage value maps across color and hidden storage. Do not invent that mapping. [Manual §16, “Color Image Format,” pp. 318–320](https://ultra64.ca/files/documentation/nintendo/Nintendo_64_Programming_Manual_NU6-06-0030-001G_HQ.pdf)

## Existing software state and required conversion

**[VERIFIED: `docs/spec/rdp.md:169-176,256-265`]** Existing v3/v4 software stores “a machine-owned 1024x1024 plane of 16-bit sample masks” and says “The raster software policy samples each pixel at the sixteen centers whose X and Y offsets are 1/8, 3/8, 5/8, and 7/8.” The document also says this is provisional software state and its sample/tie policy is not defined by the inspected primary sources. This is not the vendor coverage value or a claim about the hardware's sample mask. The actual raster code accumulates one bit per covered software sample (`core/t64_rdp_raster.c:517-553`).

Before blending, software mask population `0..16` must be converted to the primary logical coverage scale `0..8`; retain the software mask separately if it is still needed. **[REVERSIBLE SOFTWARE POLICY RECOMMENDATION]** A simple deterministic conversion is round-half-up, `coverage8 = (popcount(mask) + 1) / 2`, with `coverage_fraction = coverage8 / 8`. This does not claim to reproduce the vendor's `0xA5A5` hardware coverage-dither mask or sample placement. Alpha factors also need an explicit software representation; the witness below uses exact rational factors and floors the final channel division. Neither is a hardware precision claim.

The primary sources give no RDP power-on/reset default for these mode fields. The reserved nibble's `0xF` is not such a default. Software must name an initialization default or refuse a blend while SetOtherModes state is missing; neither behavior is a primary-source fact.

## Independently assembled raw witness

**SetOtherModes literal:** W0=`0xEF10003F`, W1=`0x02707048`, 64-bit command=`0xEF10003F02707048`. Bit extraction gives command byte `0xEF`, raw opcode bits 61:56=`0x2F`, cycle type `01`, alpha dither select `3`, reserved nibble `0xF`; cycle 0 selectors `(P,A,M,B)=(0,0,1,0)`, cycle 1 `(0,2,3,0)`. `force_blend`, `alpha_cvg_select`, `cvg_times_alpha`, `image_read_en`, and `antialias_en` are one; `cvg_dest=clamp`, `color_on_cvg=0`, alpha compare and `dither_alpha_en` are zero. The primary diagrams and SDK producer declarations independently anchor these fields.

| Input | RGBA or alpha |
|---|---:|
| Pixel / combiner output | `(200,120,80,255)` |
| Memory pixel | `(40,24,16,192)` |
| Fog register | `(16,8,4,64)` |
| Shade alpha | `128` |
| Initial logical memory coverage | `3` (stored memory coverage value `2`) |
| New pixel coverage | `4` (`4/8`, with pixel alpha `255/255`) |

For this **software-policy arithmetic witness**, use exact rational factors: cycle-0 `A=4/8=1/2`, `B=1-A`; cycle-1 `A=128/255`, `B=127/255`. Cycle 0 produces `floor((P+M)/2) = (120,72,48)`. Cycle 1 P selector `0` feeds that result back; `floor((128*(120,72,48)+127*(16,8,4))/255) = **(68,40,26)**`. With `cvg_dest=clamp`, logical coverage is `min(8,3+4)=7`, whose stored logical value is `7-1=6`. For 32-bit RGBA, the manual places that stored three-bit coverage value (`110`) in alpha bits 7:5. This witness asserts RGB before color-image quantization and logical/stored coverage; it does not assert the unsettled 16-bit hidden-bit byte mapping or 5-bit hardware alpha rounding.

## Unresolved implementation details

- Choose and version exact alpha conversion/precision, channel rounding/clamp, alpha-compare threshold tie, dither progression, and blend/memory effects for each destination mode.
- Convert the existing 16-bit software mask population into logical 0..8 coverage under an explicit software rule; do not call the current 16 software samples the vendor's sample/dither pattern.
- Keep logical stored coverage separate from a guessed 16-bit RDRAM hidden-bit/byte layout. Source covers 32-bit alpha coverage placement but not host byte mapping.
- Define deterministic missing SetOtherModes state before implementation; documentation does not define hardware reset values.

