# Plan 07-05: Raw Texture Contract

**Scope:** Implementation preflight for triangle S/T/W interpolation and texture sampling. This records primary facts, derived expectations, and reversible software-profile choices separately. It is not hardware qualification.

## Raw fields and selected numeric policy

For raw triangle opcode `0x0A` (edge + texture), the texture group occupies packet words W4–W11; `0x0B` has the same texture offsets before its trailing Z group. In `0x0E/0x0F`, shade precedes texture, so the same eight-word texture group occupies packet words W12–W19. Each Wn is one 64-bit command word; the four lanes are 16-bit high-to-low fields. The SGI summary labels each lane integer or fraction and identifies W as normalized inverse depth; its diagram does not declare signed encoding or a fractional bit count. [SGI RDP Command Summary v2.0, Table 14 and diagram, pp. 18–19](https://ultra64.ca/files/documentation/silicon-graphics/SGI_RDP_Command_Summary.pdf)

| Texture-group row | Packet word, 0x0A/0x0B | Packet word, 0x0E/0x0F | Bits 63–48 | 47–32 | 31–16 | 15–0 |
|---|---:|---:|---|---|---|---|
| G0 | W4 | W12 | S integer | T integer | W integer | unused |
| G1 | W5 | W13 | dS/dx integer | dT/dx integer | dW/dx integer | unused |
| G2 | W6 | W14 | S fraction | T fraction | W fraction | unused |
| G3 | W7 | W15 | dS/dx fraction | dT/dx fraction | dW/dx fraction | unused |
| G4 | W8 | W16 | dS/de integer | dT/de integer | dW/de integer | unused |
| G5 | W9 | W17 | dS/dy integer | dT/dy integer | dW/dy integer | unused |
| G6 | W10 | W18 | dS/de fraction | dT/de fraction | dW/de fraction | unused |
| G7 | W11 | W19 | dS/dy fraction | dT/dy fraction | dW/dy fraction | unused |

**Vendor facts:** SGI describes S/T as texture coordinates, W as normalized inverse depth, and each derivative by its X, edge, or Y direction. Nintendo’s programming-manual draft says the rasterizer supplies `s/w`, `t/w`, and `1/w` for texture mapping; texture coordinates can be perspective-corrected and are then converted to tile-relative coordinates. The manual separately calls vertex texture coordinates `S10.5` (five fractional bits). That vertex format does not establish the raw triangle coefficient format. [Nintendo 64 Programming Manual draft, §§13 “RS: Rasterizer”, 14 “Texture Pipeline” and “Texture State”, pp. 184–186, 215–216, 219–221](https://ultra64.ca/files/documentation/nintendo/Nintendo_64_Programming_Manual_NU6-06-0030-001G_HQ.pdf)

**D-11 software policy:** Use `triangle-twos-complement-q16-v1`: join each integer/fraction pair as a signed 32-bit two’s-complement Q16 value (`raw / 65536`). This is the owner-approved deterministic profile policy, not a Nintendo/SGI signed-type claim. Keep the source gap attached to the implementation and profile identity. [07-CONTEXT.md, D-11](07-CONTEXT.md); [07-04-RAW-CONTRACT.md](07-04-RAW-CONTRACT.md)

**Texture interpolation policy:** The base S/T/W values are anchored at `(XH,YH)`. At pixel center `(x+0.5,y+0.5)`, evaluate each channel as

```text
base + floor(dx * (x_center - major_x(y_center)) / 65536)
     + floor(de * (major_x(y_center) - XH) / 65536)
     + floor(dy * (y_center - YH) / 65536)
```

All values and products are signed Q16/Q32 with explicit floor division. This anchor and operation order are deterministic Tendies64 policy, following the already documented shade interpolation rule; the primary table names derivatives but does not specify the interpolation origin, product rounding, or accumulation order.

## Primitive header controls

SGI Table12 identifies W0 tile in bits50:48 and level in bits53:51. Tile names
its primitive descriptor, and level caps the mip chain relative to that tile;
zero means the primitive level only. The software profile validates the whole
declared chain before drawing when LOD is enabled, refusing indices outside0..7.
Other valid descriptors cannot supply a larger implicit maximum. Raw witness
`0x0A89002000100000` selects tile1 and maximum level1 while retaining right-major
and YL/YM/YH32/16/0. Box and three-point floor policy applies to each decoded
channel, including one-bit alpha; mixed-alpha half ties therefore floor to zero.

## Perspective, tile, LOD, and filter controls

The raw `Set Other Modes` command has six-bit opcode `0x2F` (GBI byte `0xEF`). SGI Table 20 places perspective at bit 51, detail at 50, sharpen at 49, LOD at 48, sample type at 45, midpoint at 44, and per-cycle filter/conversion controls at 43–41. The Nintendo SDK header defines `G_TP_PERSP`, `G_TL_TILE/LOD`, `G_TD_CLAMP/SHARPEN/DETAIL`, `G_TF_POINT/AVERAGE/BILERP`, and their mode shifts. [SGI Table 20, pp. 27–29](https://ultra64.ca/files/documentation/silicon-graphics/SGI_RDP_Command_Summary.pdf); [Nintendo SDK `gbi.h`, revision 1.138, “Set Other Modes” constants and `gDPSetOtherMode`](https://ultra64.ca/files/documentation/online-manuals/man/header/gbi.htm)

| Control | Primary-backed behavior | Plan 07-05 software contract |
|---|---|---|
| Affine / perspective | `G_TP_NONE` / `G_TP_PERSP`; manual documents per-pixel perspective correction and the `s/w,t/w,1/w` rasterizer values. | Affine samples S,T directly. Perspective computes `floor(S_q16 * 65536 / W_q16)` and the same for T. Signed W is allowed; W=0 or a conservative W interval crossing zero rejects the whole triangle before its first pixel. Division and W-sign rules are software policy. |
| Tile coordinates | Image coordinates become tile-relative by subtracting SL/TL. Tile shift, mask, mirror and clamp are descriptor controls; mask zero clamps to inclusive SL/SH and TL/TH bounds. [Manual draft, §§14 “Texture Image Space” and “Tile Attributes”, pp. 219–225](https://ultra64.ca/files/documentation/nintendo/Nintendo_64_Programming_Manual_NU6-06-0030-001G_HQ.pdf) | Apply descriptor shifts, subtract SL/TL, then use the descriptor’s wrap/mirror/clamp controls. For the literal below, tile 0 has zero shift/mask/mirror and clamps both axes to inclusive 0..7. Point coordinates map to the texel whose top-left unit square contains them (`floor` after tile transform). |
| Tile / LOD | LOD off selects the primitive tile. LOD uses differences of adjacent perspective-corrected coordinates; the manual names `min_level`, `max_level`, `prim_tile`, `l_tile` and signed `s.0.8 l_frac`. Mip tile selection is clamped to max level; trilinear MIP uses two cycles and the combiner. [Manual draft, §14 “LOD Enabled”, pp. 229–233](https://ultra64.ca/files/documentation/nintendo/Nintendo_64_Programming_Manual_NU6-06-0030-001G_HQ.pdf) | `G_TL_TILE` is the fixture default. For LOD-enabled software behavior use `rho=max(|dS/dx|,|dT/dx|,|dS/dy|,|dT/dy|)` after perspective correction; `lod=max(rho,0.5)`, `l_tile=clamp(floor(log2(max(lod,1))),0,max_level)`, and `l_frac=floor(256 * frac(lod / 2^l_tile))`, clamped to 0..255. Select `prim_tile+l_tile`, refusing indices outside tiles 0..7. This max-axis norm, minimum, and quantization are named software policy; the manual does not give enough arithmetic detail for them to be hardware claims. |
| Detail / sharpen | The mode bits are exclusive in the manual. Detail mode combines the finest base level with a detail tile selected from `prim_tile`; the detail tile shift controls texel ratio. Sharpen uses the two finest MIP levels; Figure 14-8 gives `P = A + (B-A)*(l_frac-1.0)`. Prim Min Level is fixed-point 0.5 by default. [Manual draft, §§14 “Detail Texture”, “Sharpen Mode”, Figure 14-8, pp. 234–238](https://ultra64.ca/files/documentation/nintendo/Nintendo_64_Programming_Manual_NU6-06-0030-001G_HQ.pdf); [SGI Table 22, p. 31](https://ultra64.ca/files/documentation/silicon-graphics/SGI_RDP_Command_Summary.pdf) | Reject both detail and sharpen enabled together. Detail uses `prim_tile` for detail and `prim_tile+1` for base in magnification, clamps its blend fraction to configured `min_level`, and floors channel results. Sharpen applies the cited extrapolation equation with the software Q0.8 fraction. These are documented-rule implementations with software rounding/selection details. |
| Point / box / three-point | Nintendo says point selects the nearest texel; box average is exact when the sample lies at the center of four texels; RDP “bilerp” uses three nearest texels, with triangle choice based on the sample’s side of the diagonal. Figure 15-5 depicts TL/TR/BR versus TL/BL/BR. [Manual draft, §13 “Filter Types”, pp. 193–195; §15 “Bilinear Filtering” and Figure 15-5, pp. 274–276](https://ultra64.ca/files/documentation/nintendo/Nintendo_64_Programming_Manual_NU6-06-0030-001G_HQ.pdf) | Point uses the containing unit texel. Box averages four channels with floor division by four; using it away from the documented center case is a named software extension. Three-point uses Q16 fractions `fs,ft`: if `fs>=ft`, `TL*(1-fs)+TR*(fs-ft)+BR*ft`; otherwise `TL*(1-ft)+BL*(ft-fs)+BR*fs`. Equality chooses the first triangle; channel results floor and clamp to channel range. The triangle equations are barycentric derivations from Figure 15-5, not printed vendor equations. |

For an 8×8 RGBA16, clamp, point, tile-LOD-off fixture, these mode commands select one-cycle filtered-color conversion, point sampling, no LOD/detail/sharpen, and the indicated perspective mode:

| Mode | `G_TP_*` | Raw 64-bit `Set Other Modes` word |
|---|---|---|
| Perspective | `G_TP_PERSP` | `0xEF080C0000000000` |
| Affine | `G_TP_NONE` | `0xEF000C0000000000` |

Here `G_TC_FILT=6<<9` sets the SDK’s filtered-color conversion value; `G_TF_POINT=0` leaves sample_type at 0 (SGI’s 1×1 point sample). The raw command’s opcode field is `0x2F`, while its GBI command byte is `0xEF`; do not confuse the representations. The bit constants and command packing are observed in Nintendo’s published SDK header, not copied as implementation code.

## Independent literal packet and expected texels

This raw `0x0A` edge-plus-texture packet uses the primary row positions above and D-11’s signed Q16 policy. The edges define a triangle with `YH=0`, `YM=4`, `YL=8`, right-side major edge `XH=4`, upper minor slope `-1`, and lower minor edge `XL=0`, slope `0`. Pixel `(1,4)` is inside the triangle under the already selected profile coverage rule. Texture values are anchored at `(XH,YH)=(4,0)`. All numbers were composed from the pinned field table, not a Tendies64 encoder or decoder.

| Packet word | Literal 64-bit value | Expected field interpretation |
|---|---:|---|
| W0 | `0x0A80002000100000` | raw opcode 0x0A; right-major; YL=8, YM=4, YH=0 |
| W1 | `0x0000000000000000` | XL=0, dXL/dy=0 |
| W2 | `0x0004000000000000` | XH=4, dXH/dy=0 |
| W3 | `0x00000000FFFF0000` | XM=0, dXM/dy=−1 |
| W4 | `0x0006FFFE00020000` | S=6, T=−2, W=2 integer lanes |
| W5 | `0x0000000000000000` | dS/dx, dT/dx, dW/dx integer lanes = 0 |
| W6 | `0x0000000000000000` | S/T/W base fractions = 0 |
| W7 | `0x8000400040000000` | dS/dx=+0.5, dT/dx=+0.25, dW/dx=+0.25 fractions |
| W8–W11 | `0x0000000000000000` each | all d/de and d/dy lanes = 0 |

At pixel center `(1.5,4.5)`, the major edge is X=4, so Δx=−2.5, Δe=0, Δy=4.5. Independent arithmetic gives S=4.75, T=−2.625, W=1.375. The tile’s logical texels are `(3,0)=0xF801` red, `(4,0)=0x07C1` green, `(3,7)=0x003F` blue and `(4,7)=0xFFFF` white; all other texels may be zero. This preloaded logical tile state isolates coefficient/sampling behavior from the separate texture-loading contract.

| Mode and signed result | Tile coordinate after clamp | Point texel expected |
|---|---:|---:|
| Perspective: S/W=38/11, T/W=−21/11 | `(3.4545…, 0)` | `(3,0)` = `0xF801` |
| Affine: S=4.75, T=−2.625 | `(4.75, 0)` | `(4,0)` = `0x07C1` |
| Negative T incorrectly treated as unsigned (discriminator only) | T clamps to 7 | perspective `(3,7)` = `0x003F` |

The fixture checks sign policy, both nonzero texture derivatives, W normalization, clamp, point selection, and affine/perspective mode selection. It does not prove vendor coefficient signedness, actual RDP rounding, or a hardware pixel result.

## Refusal bounds and remaining limits

Before the first pixel effect, use checked signed-64 multiply/add and conservative extrema over the clipped triangle to prove every interpolation product/sum and perspective numerator fits; prove W cannot cross zero in the clipped domain; and prove interpolated S/T/W fit signed Q16 before division. Reject the complete triangle if those bounds cannot be established, if W is zero/crossing zero, if tile/LOD selection leaves tile 0..7, or if any shifted/tile coordinate cannot be safely represented. Clamp wide coordinates before narrowing. Bound raster work to the profile’s 1024×1024 maximum extent and at most sixteen coverage samples per pixel; texture filtering then reads at most four texels (three for the triangular rule). These are deterministic software refusal limits, not hardware limits.

**Unresolved primary/hardware gaps:** no inspected primary Nintendo/SGI declaration establishes raw texture coefficient signedness, confirms Q16 for those raw pairs, specifies the S/T/W interpolation anchor or rounding, settles W=0/negative hardware behavior, gives exact LOD arithmetic, or prints triangle-filter barycentric weights/tie/rounding. D-11 plus the explicit Plan 07-05 software policy makes implementation possible without claiming those gaps are resolved. VID-04 remains blocked; no oracle, private guest, silicon test, protected asset, or publication is used or authorized here. No manuals, code, or image assets were added to the repository.

**Sources verified:** SGI’s linked 48-page PDF identifies itself as *Nintendo Ultra64 RDP Command Summary*, Version 2.0, ©1996, with page footers last modified 4/11/96. Nintendo’s linked programming manual identifies itself as a draft. Nintendo’s `gbi.h` page identifies revision 1.138 dated 1998-12-24. Each linked source opened successfully during this preflight.
