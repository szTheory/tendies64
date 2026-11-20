# Plan 07-08 Depth Fields — Primary Preflight

**Scope:** Primary-source field facts for Set Z Image, Set Prim Depth, Set Other Modes, and Z-enabled triangle packets. This is a decoder-start preflight, not implementation or hardware-accuracy evidence.

## Source-backed command fields

Raw RDP command IDs are the six bits at bits 61–56 of the 64-bit command word. The SDK header uses an 8-bit GBI command byte and documents that the RDP uses its low six bits; keep those representations distinct. [CITED: SGI, *Nintendo Ultra64 RDP Command Summary* v2.0, pp. 2–3, 28, 35; Nintendo SDK `gbi.h` rev. 1.138, “Graphics Commands” and RDP opcode notes, https://ultra64.ca/files/documentation/online-manuals/man/header/gbi.htm]

| Operation | Raw RDP opcode / SDK byte | Primary field facts | Alignment / semantics |
|---|---|---|---|
| Set Z Image | `0x3e` / GBI `0xfe` | Base DRAM address is bits 25–0 of the command; other Set-Z fields are zero in the diagram. `gDPSetDepthImage` emits the Set-Z command with format/size zero and width 1. | Nintendo's manual says the depth-image pointer must be 64-bit aligned (low three address bits zero). The image pointer is a byte address. [CITED: SGI RDP Command Summary, pp. 3–4; Nintendo 64 Programming Manual draft, p. 320, “Image Alignment Requirements”; SDK `gbi.h`, lines represented by “G_SETZIMG” and `gDPSetDepthImage`, same URL] |
| Set Prim Depth | `0x2e` / GBI `0xee` | In the raw 64-bit command word 0, Primitive Z occupies bits 31–16 and Primitive Delta Z bits 15–0 (the low 32-bit half). The SDK's `Gfx` pair places that packed low half in `words.w1`, with `z` high and `dz` low. | The manual describes this register as the primitive-wide source for Z-buffer rectangles and identifies its Z and DeltaZ values as 16-bit register values. [CITED: SGI RDP Command Summary, p. 35, Table 26; Nintendo 64 Programming Manual draft, pp. 320–322, “Depth Source” / “Z Calculation”; SDK `gbi.h`, `gDPSetPrimDepth`, same URL] |
| Set Other Modes | `0x2f` / GBI `0xef` | `z_source_sel` bit 2 (`0` pixel Z, `1` primitive Z); `z_compare_en` bit 4; `z_update_en` bit 5; `image_read_en` bit 6; `color_on_cvg` bit 7; `cvg_dest` bits 9–8 (`0` clamp, `1` wrap, `2` zap/full, `3` save); `z_mode` bits 11–10 (`0` opaque, `1` interpenetrating, `2` transparent, `3` decal). | These are one-bit or two-bit control fields in the raw mode word, not separate RDP commands. SDK constants corroborate primitive/pixel selection and render-mode masks. [CITED: SGI RDP Command Summary, pp. 27–28, Table 20; SDK `gbi.h`, depth-source/render-mode constants, https://ultra64.ca/files/documentation/online-manuals/man/header/gbi.htm] |

Independent raw-word checks (fields are decoded directly by the cited bit positions):

```text
Set Z Image:   0x3e00000000001000  -> opcode 0x3e, byte address 0x1000 (64-bit aligned)
Set PrimDepth: 0x2e00000013570009  -> opcode 0x2e, primitive Z 0x1357, primitive DeltaZ 0x0009
Other Modes:   0x2f00000000000034  -> opcode 0x2f, primitive-Z source + Z compare + Z update,
                                     opaque z_mode (bits 11:10 are zero)
```

For the last word, `0x34 = (1<<5) | (1<<4) | (1<<2)`: update, compare, and primitive-Z source. The SDK defines `G_ZS_PIXEL` as zero and `G_ZS_PRIM` as one at the Z-source shift. This example is raw command-layout evidence, not a measured hardware result. [CITED: SGI RDP Command Summary, pp. 27–28, Table 20; SDK `gbi.h`, `G_ZS_PIXEL`, `G_ZS_PRIM`, and render-mode constants, same URL]

## Triangle packet lengths and Z-group positions

The summary says optional coefficient groups concatenate in Edge, Shade, Texture, ZBuffer order. Counting the detailed 64-bit rows gives Edge=4 words, Shade=8, Texture=8, ZBuffer=2; the table's repeated “4” entries identify group presence/order and are not the detailed row lengths. [CITED: SGI RDP Command Summary, Table 11 and detailed command diagrams/tables, printed pp. 12–20]

| Raw opcode | Groups | Total 64-bit words | Bytes | Z group start (word index) |
|---:|---|---:|---:|---:|
| `0x08` | Edge | 4 | 32 | — |
| `0x09` | Edge, Z | 6 | 48 | 4 |
| `0x0a` | Edge, Texture | 12 | 96 | — |
| `0x0b` | Edge, Texture, Z | 14 | 112 | 12 |
| `0x0c` | Edge, Shade | 12 | 96 | — |
| `0x0d` | Edge, Shade, Z | 14 | 112 | 12 |
| `0x0e` | Edge, Shade, Texture | 20 | 160 | — |
| `0x0f` | Edge, Shade, Texture, Z | 22 | 176 | 20 |

For each Z-enabled packet, its final two words are the Z group. In each first Z word, the four 16-bit lanes from high to low are Z integer, Z fraction, DzDx integer, DzDx fraction. In the second word they are DzDe integer, DzDe fraction, DzDy integer, DzDy fraction. The diagram labels the last fraction in word 21; Table 15's row text says word 23 for that same lane. Use word 21: the diagram shows the two-row group at words 20 and 21, and the packet length for opcode `0x0f` is 22 words. Preserve the table-label typo in source notes rather than extending the packet. [CITED: SGI RDP Command Summary, Table 15 and “ZBuffer Coefficients” diagram, printed p. 20; detailed group diagrams, pp. 12–20]

### Independent nonzero Z-group literal

For an opcode `0x0f` packet, the packet layout puts the Z group at W20–W21. This literal is a field-layout witness only; preceding words are omitted and it does not claim valid raster geometry or pixel behavior.

```text
W20 = 0x0005000300070009 -> Z integer=5, Z fraction=3, DzDx integer=7, DzDx fraction=9
W21 = 0x000b000d00110013 -> DzDe integer=11, DzDe fraction=13,
                            DzDy integer=17, DzDy fraction=19
```

Expected 16-bit lane tuple: `(5, 3, 7, 9, 11, 13, 17, 19)`. Under D-11's software Q16 pairing policy, the same literal's joined 32-bit pairs are `Z=0x00050003`, `DzDx=0x00070009`, `DzDe=0x000b000d`, and `DzDy=0x00110013`. Joining each adjacent integer/fraction lane is not a competing or wrong layout; it preserves the primary field order while applying the software numeric policy. A real wrong layout that swaps the two W21 coefficient pairs decodes `DzDy=0x000b000d` and `DzDe=0x00110013`, contrary to the diagram's high-to-low DzDe-then-DzDy order. Another real error follows Table 15's “word 23” text and misses the nonzero DzDy fraction at W21's final lane. This literal is independent of any Tendies64 packer or decoder. [CITED: SGI RDP Command Summary, Table 15 and “ZBuffer Coefficients” diagram, printed p. 20] [VERIFIED: `.planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-CONTEXT.md:157-162`]

## Primary depth arithmetic, compare, and coverage behavior

- The manual describes blender calculations as an 18-bit fixed-point `0,15.3` Z and a 16-bit signed `s15` DeltaZ. It states that stepped linear Z is stored in a 14-bit exponent/mantissa form (3 exponent bits, 11 mantissa bits). Its `z_format[8]` table gives the decode shift/add pairs; for example exponent 3 decodes as `(mantissa << 3) + 0x38000`. [CITED: Nintendo 64 Programming Manual draft, pp. 322–324, “Z Image Format,” Figure 16-12, and decode pseudocode]
- Complete manual decode table, copied from `z_format[8]` at printed p. 324:

  | Exponent | Shift | Add | Decode from stored 11-bit mantissa `m` |
  |---:|---:|---:|---|
  | 0 | 6 | `0x00000` | `(m << 6) + 0x00000` |
  | 1 | 5 | `0x20000` | `(m << 5) + 0x20000` |
  | 2 | 4 | `0x30000` | `(m << 4) + 0x30000` |
  | 3 | 3 | `0x38000` | `(m << 3) + 0x38000` |
  | 4 | 2 | `0x3c000` | `(m << 2) + 0x3c000` |
  | 5 | 1 | `0x3e000` | `(m << 1) + 0x3e000` |
  | 6 | 0 | `0x3f000` | `(m << 0) + 0x3f000` |
  | 7 | 0 | `0x3f800` | `(m << 0) + 0x3f800` |

  [CITED: Nintendo 64 Programming Manual draft, printed p. 324, `z_format[8]` and `zvalue = (mantissa << z_format[exponent].shift) + z_format[exponent].add`, https://ultra64.ca/files/documentation/nintendo/Nintendo_64_Programming_Manual_NU6-06-0030-001G_HQ.pdf]
- The manual says DeltaZpix is `|dZdx| + |dZdy|`; memory DeltaZ uses four bits, described as priority encoding the index of the most significant one bit. It compares using the maximum of pixel DeltaZ and stored DeltaZ. Named Max-Z, farther, nearer, and in-front tests are shown in equations 6–9; exact branch behavior also combines the Z relation with coverage to determine surface correlation. [CITED: Nintendo 64 Programming Manual draft, pp. 321–324, equations 4–10]
- The documented mode effects are not interchangeable: opaque antialiased rendering uses Z and coverage correlation; the manual says transparent surfaces read depth for opaque-surface occlusion but do not modify depth, and says decal surfaces do not modify depth because the underlying coplanar surface already has the correct depth. However, the manual's prose does not specify whether those mode behaviors override `z_update_en=1`. SDK XLU/decal render-mode macros omit `Z_UPD`, which establishes the standard composed settings, not the precedence for that unusual combination. Keep the override question as an explicit software-profile policy/gap; do not claim the SDK omission proves hardware gating. Interpenetrating modes are separately named. [CITED: Nintendo 64 Programming Manual draft, pp. 327–336, especially pp. 332 and 335, “Blender Modes and Assumptions”; SDK `gbi.h` render-mode constants/macros at https://ultra64.ca/files/documentation/online-manuals/man/header/gbi.htm]
- Coverage and hidden storage are part of depth behavior: the manual attributes two extra bits per color or Z pixel to 9-bit RDRAM. Figure 16-13 lists `Zexp(3) + Zmantissa(11) + dZ(4)` = 18 logical bits. Combining that diagram's high-to-low field order with the manual's statement that the two hidden bits are the two LSBs of each 18-bit word gives a logical split of visible 16 bits = Zexp(3) + Zmantissa(11) + high dZ(2), hidden 2 bits = low dZ(2). The manual says hidden bits are accessed directly by the RDP memory interface and also describes their logical placement as LSBs associated with 16-bit words. This is a logical field split; it does not specify physical sideband addressing or every master/write effect. The manual also explains that coverage lets the VI identify silhouette pixels after rendering, while Z-based surface correlation distinguishes internal edges from silhouettes. [CITED: Nintendo 64 Programming Manual draft, pp. 318–320 and 324–329, Figures 16-10/16-13 and “Blender Modes and Assumptions”]

### Primitive Z versus triangle Z

The manual expressly places a primitive-wide Z in the 16-bit PrimDepth register, selected via `G_ZS_PRIM`; it recommends the `z` argument range `0x0000`–`0x7fff` (usually near-to-far) and gives the RSP screen-Z expression `32*((z/w)*Viewport.vscale[2] + Viewport.vtrans[2])`. With its usual viewport values, the manual says that expression ranges `0x0`–`0x7fc0`. Separately, its blender section says the 16-bit primitive Z register supplies the 15 integer bits of the internal 18-bit `0,15.3` Z. [CITED: Nintendo 64 Programming Manual draft, pp. 208–209 and 321–322, “Depth Source” / “Z Calculation”]

This material does not state an explicit bit-level conversion from the PrimDepth register value to the blender's 18-bit `0,15.3` value (for example, whether and exactly how the 3 fractional bits are formed). Do not infer a shift/rounding rule from the screen-Z producer expression. SGI's raw triangle table separately calls its W20 base field “Inverse Depth” and lays it out as 16-bit integer plus 16-bit fraction, with separate Dz derivative pairs; that raw packet layout does not prove the PrimDepth register shares the same numeric encoding. D-11 authorizes a software rule for unresolved triangle coefficient numerics, not a hardware fact about primitive-Z conversion. [CITED: SGI RDP Command Summary, printed p. 20; Nintendo 64 Programming Manual draft, pp. 208–209 and 321–324]

## Explicit gaps and policy boundary

- **Raw triangle coefficient numeric encoding:** The SGI diagrams establish the 16-bit integer/fraction lanes and names for Z, DzDx, DzDe, and DzDy, but the inspected primary fields do not define their signed encoding. **Inference from distinct sections:** the manual's blender-level `s15` DeltaZ describes the blender quantity, while SGI's raw triangle table separately labels Dz coefficient integer/fraction lanes; do not transfer the blender declaration to packet fields. Owner D-11 states: “Owner approved proceeding with an explicit two's-complement software-profile policy for triangle coefficient encodings whose primary numeric representation remains unresolved.” It also states: “The reconciled primary packet lengths and field positions remain mandatory.” This authorizes software semantics only; it does not establish hardware representation or accuracy. [CITED: SGI RDP Command Summary, p. 20; Nintendo 64 Programming Manual draft, pp. 321–324] [VERIFIED: `.planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-CONTEXT.md:157-162`]
- **Compression precision:** The manual documents the 18-bit to 14-bit depth reduction and says precision is lost, but this preflight does not establish every silicon rounding/tie, clipping, or saturation case. Keep such behavior as an explicit software-profile policy unless a primary source or approved measurement supplies it. [CITED: Nintendo 64 Programming Manual draft, pp. 322–325; implementation details beyond cited text remain open]
- **Hidden-lane mapping:** The logical field split of 14 Z bits plus 4 dZ bits across 16 visible bits plus 2 hidden bits is derivable from the primary diagram's field order and the manual's two-hidden-LSB rule. This does not define a physical sideband-address representation, all CPU/PI/SP write/readback/fill/copy/reset effects, or whether a software model should store hidden state as a separate array. Preserve the source-backed logical split; keep any physical mapping and software ownership choices explicit. [CITED: Nintendo 64 Programming Manual draft, pp. 318–320 and 324]
- **Primitive-Z conversion:** The source gives the PrimDepth range, RSP screen-Z producer formula, and the internal 15-integer-bit role of the 16-bit register, but the inspected pages do not give a precise register-to-`0,15.3` bit conversion. Keep the conversion rule as a named software-profile policy unless another primary source resolves it; do not transfer D-11's triangle Q16 policy to primitive Z. [CITED: Nintendo 64 Programming Manual draft, pp. 208–209 and 321–322]
- **Z-update precedence:** The manual's primary algorithm text states that transparent and decal surfaces do not modify depth. The checked SDK standard XLU/decal render-mode compositions also leave `Z_UPD` clear. The inspected primary text does not settle whether setting `z_update_en=1` alongside those `z_mode` values still suppresses writes or enables them. Treat unconditional suppression as unproven hardware behavior; choose and name the software-profile rule, and test both the standard setting and the explicitly combined setting. [CITED: Nintendo 64 Programming Manual draft, pp. 332, 335; SDK `gbi.h` render-mode macros, same URL]
- **Hardware precision and timing:** These manuals document formats and algorithms, not silicon measurements of internal rounding, stage timing, same-tick order, or all boundary behavior. Tests can establish Tendies64 policy behavior only; no hardware-accuracy claim follows from this preflight.

## Primary sources inspected

- Silicon Graphics, *Nintendo Ultra64 RDP Command Summary*, Version 2.0 (1996), especially Tables 11, 15, 20, 26 and the detailed coefficient diagrams, printed pp. 12–20, 27–28, 35: https://ultra64.ca/files/documentation/silicon-graphics/SGI_RDP_Command_Summary.pdf
- Nintendo/SGI, *Nintendo 64 Programming Manual*, NU6-06-0030-001G, draft, especially “Depth Source,” “Image Alignment Requirements,” “Z Calculation,” “Z Image Format,” and “Blender Modes and Assumptions,” printed pp. 208, 320–336: https://ultra64.ca/files/documentation/nintendo/Nintendo_64_Programming_Manual_NU6-06-0030-001G_HQ.pdf
- Nintendo SDK `gbi.h`, revision 1.138, dated 1998-12-24: RDP command-byte low-six-bit note; `G_SETZIMG`, `G_SETPRIMDEPTH`, Z-source constants, mode fields, and packet macros: https://ultra64.ca/files/documentation/online-manuals/man/header/gbi.htm

**Readiness:** Structural command fields and all packet lengths are pinned by the supplied primary material, with the Table 15 word-23 typo corrected by the two-row diagram. One nonzero independent raw literal is supplied. Once Plan 07-07 qualifies as the predecessor, implementation may proceed under D-11's named software-profile numeric policy and the plan's named hidden-state policy; these primary-source gaps do not add an owner decision gate. Authored tests establish only those software policies, not hardware accuracy or VID-04 evidence.
