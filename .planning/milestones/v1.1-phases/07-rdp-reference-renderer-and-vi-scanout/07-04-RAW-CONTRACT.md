# Plan 07-04: Raw Triangle Packet Contract

**Scope:** Reconcile only raw triangle command lengths and coefficient fields for raw RDP opcodes 0x08–0x0F. This note is a source/layout preflight, not a decoder, raster fixture, or accuracy claim.

## Finding

The structural packet layout and full word counts are reconciled by two primary sources: SGI’s Nintendo Ultra64 RDP Command Summary v2.0 (1996) and Nintendo/SGI U.S. Patent 6,556,197 B1 (published 2003-04-29; original assignees Nintendo Co Ltd and Silicon Graphics Inc). The patent’s raw command diagrams independently show the same coefficient rows and word positions. Nintendo’s SDK header independently confirms the opcode option bits and six-bit hardware opcode. [CITED: SGI RDP Command Summary v2.0, pp. 1, 12–20, https://ultra64.ca/files/documentation/silicon-graphics/SGI_RDP_Command_Summary.pdf; U.S. Patent 6,556,197 B1, FIGS. 65–76, https://patents.google.com/patent/US6556197B1/en; Nintendo SDK gbi.h, “generated RDP commands” and triangle masks, https://ultra64.ca/files/documentation/online-manuals/man/header/gbi.htm]

The owner-provided SGI PDF locator at /silicon/ currently returns 404. The working Ultra64 locator is /silicon-graphics/. Its PDF is 48 pages; the cover says Version 2.0 and ©1996 SGI, and the pages say “Last Modified 4/11/96”. The PDF metadata records a 2005 Ghostscript reproduction by Adam Gashlin. The local ignored PDF, working Ultra64 download, and hcs64.com/RDP_COMMANDS.pdf have identical SHA-256: fe019ef28498d759292145f06716ac11620240e643dd65809d15ca4a6f1c6ad3. Thus the hcs64 copy is not an independent revision or source. [CITED: SGI PDF cover and page footers, pp. 1, 12–20, https://ultra64.ca/files/documentation/silicon-graphics/SGI_RDP_Command_Summary.pdf; local and public SHA-256 comparison, 2026-11-16; PDF metadata inspected with pdfinfo]

## Opcode and length table

The raw opcode is the six-bit field at bits 61–56 of the first 64-bit word. The SDK GBI byte uses the 0xC8–0xCF range; its low six bits are the raw RDP opcode (for example, G_TRI_SHADE is byte 0xCC, raw field 0x0C). Do not compare or store these as the same representation. [CITED: SGI Table 12, p. 13; Nintendo SDK gbi.h, lines 199–233, https://ultra64.ca/files/documentation/online-manuals/man/header/gbi.htm]

Detailed diagrams number each 64-bit word row. The eight labels 0–7 across the top of a diagram are byte lanes; each 64-bit row is two 32-bit halves, high then low. For the fully enabled packet, the rows are Edge 0–3, Shade 4–11, Texture 12–19, Z 20–21. Optional groups concatenate in Edge, Shade, Texture, Z order; omitted groups consume no rows, so subsequent groups shift earlier. [CITED: SGI pp. 12–20; Patent FIGS. 65–76]

| Raw opcode | GBI byte | Present groups | Group lengths in 64-bit words | Total words | Bytes |
|---|---:|---|---:|---:|---:|
| 0x08 | 0xC8 | Edge | 4 | 4 | 32 |
| 0x09 | 0xC9 | Edge, Z | 4 + 2 | 6 | 48 |
| 0x0A | 0xCA | Edge, Texture | 4 + 8 | 12 | 96 |
| 0x0B | 0xCB | Edge, Texture, Z | 4 + 8 + 2 | 14 | 112 |
| 0x0C | 0xCC | Edge, Shade | 4 + 8 | 12 | 96 |
| 0x0D | 0xCD | Edge, Shade, Z | 4 + 8 + 2 | 14 | 112 |
| 0x0E | 0xCE | Edge, Shade, Texture | 4 + 8 + 8 | 20 | 160 |
| 0x0F | 0xCF | Edge, Shade, Texture, Z | 4 + 8 + 8 + 2 | 22 | 176 |

The SGI Table 11 (p. 12) prints “4” in each present Edge/Shade/Texture/ZBuffer cell but gives no unit. Those values cannot be used as 64-bit word lengths: the detailed diagrams and patent assign 4, 8, 8, and 2 rows respectively. Treat Table 11 as identifying which groups are present and their order; its numeric group-count unit is undocumented. Nintendo’s Intro to the N64 separately says a triangle command is close to 180 bytes and that removing texture shortens it by 64 bytes, consistent with a 22-word maximum (176 bytes) and an eight-word texture group. [CITED: SGI Table 11, p. 12, and Tables 12–15, pp. 13–20; Patent FIGS. 65–76; Nintendo Intro to the N64, §4-2-7 “Drawing Triangles”, https://ultra64.ca/files/documentation/nintendo/Nintendo_64_Intro_to_the_N64.pdf]

The final row of SGI Table 15 (p. 20) labels DzDy, frac as word 23, but the adjacent Z diagram labels it word 21. Patent FIG. 75 also labels the field in word 21; Patent FIG. 76 repeats the table’s “23” typo. The two-row Z layout is words 20 and 21, so use word 21 for DzDy, frac; do not treat the repeated table typo as a second layout. [CITED: SGI Table 15 and ZBuffer Coefficients diagram, p. 20; Patent FIGS. 75–76, sheet 79]

## Field layout

Bit positions below are inclusive, with bit 63 the most significant bit. Source diagrams are 64-bit rows; “HI32 / LO32” below means bits 63–32 / 31–0, not two independently numbered 64-bit rows.

| Group row(s) | Fields, high to low | Source-backed representation |
|---|---|---|
| Edge W0 | command 61–56; lft 55; Level 53–51; tile 50–48; YL 45–32; YM 29–16; YH 13–0 | YL/YM/YH are explicitly labeled signed s11.2, 14 bits each. lft=0 means left-major; lft=1 means right-major. |
| Edge W1 | XL integer 63–48; XL fraction 47–32; DxLDy integer 31–16; DxLDy fraction 15–0 | Each pair is split into 16-bit integer and fraction halves. |
| Edge W2 | XH integer 63–48; XH fraction 47–32; DxHDy integer 31–16; DxHDy fraction 15–0 | Major-edge X and inverse-slope fields. |
| Edge W3 | XM integer 63–48; XM fraction 47–32; DxMDy integer 31–16; DxMDy fraction 15–0 | Middle-edge X and inverse-slope fields. |
| Shade W4 | Red, Green, Blue, Alpha integer; 16 bits each from high to low | Four 16-bit integer fields. |
| Shade W5 | DrDx, DgDx, DbDx, DaDx | Four 16-bit X-derivative integer fields. |
| Shade W6 | Red, Green, Blue, Alpha fraction | Four 16-bit fractions. |
| Shade W7 | DrDx, DgDx, DbDx, DaDx fraction | Four 16-bit fractions. |
| Shade W8 / W9 | DrDe, DgDe, DbDe, DaDe integer / DrDy, DgDy, DbDy, DaDy integer | Four 16-bit fields per row. |
| Shade W10 / W11 | DrDe, DgDe, DbDe, DaDe fraction / DrDy, DgDy, DbDy, DaDy fraction | Four 16-bit fractions per row. |
| Texture W12–W15 | S, T, W, unused; DsDx, DtDx, DwDx, unused; fractions of those rows in the same order | Each named value occupies 16 bits; W is described as normalized inverse depth. |
| Texture W16–W19 | DsDe, DtDe, DwDe, unused; DsDy, DtDy, DwDy, unused; fractions of those rows in the same order | Each named value occupies 16 bits. |
| Z W20 | Z integer 63–48; Z fraction 47–32; DzDx integer 31–16; DzDx fraction 15–0 | Four 16-bit fields. |
| Z W21 | DzDe integer 63–48; DzDe fraction 47–32; DzDy integer 31–16; DzDy fraction 15–0 | Four 16-bit fields; final fraction is in W21. |

The source diagrams specify the split positions and integer/fraction labels, but do not explicitly state the signed encoding for XL/XH/XM, the edge slope pairs, or the shade/texture/Z derivative pairs. Only YL/YM/YH carry an explicit signed-format label in these layouts. Do not silently infer two’s-complement sign extension for those other fields from the diagram alone. A primary Nintendo/SGI producer specification or source that states their signed encoding is still needed for a source-backed numeric conformance claim. Owner decision D-11 separately authorizes the named software-policy decoder and authored negative cases below. [CITED: SGI Tables 12–15, pp. 13–20; Patent FIGS. 65–76]

## Literal layout witnesses

These are authored raw-word witnesses derived from the primary diagrams, not raster expectations or validated producer packets. Each Wn is one 64-bit word shown as HI32 / LO32. Positive field values are used so the witnesses do not assume an undocumented signed encoding. For coordinates and gradients split into integer/fraction halves, “integer + fraction/65536” is a derivation from the labeled 16-bit integer and 16-bit fraction halves, not a vendor-declared signed coefficient type.

### Edge-only packet, raw opcode 0x08

| Word | HI32 / LO32 | Expected field extraction |
|---|---|---|
| W0 | 0800000C / 00080005 | raw opcode 0x08; lft=0; Level=0; tile=0; YL=0x000C = 3; YM=0x0008 = 2; YH=0x0005 = 1.25 in signed s11.2 |
| W1 | 00028000 / 00010000 | XL=2 + 0x8000/65536; DxLDy=1 + 0/65536 |
| W2 | 00014000 / 00008000 | XH=1 + 0x4000/65536; DxHDy=0 + 0x8000/65536 |
| W3 | 00018000 / 0000C000 | XM=1 + 0x8000/65536; DxMDy=0 + 0xC000/65536 |

The corresponding full 64-bit literals are W0=0x0800000C00080005, W1=0x0002800000010000, W2=0x0001400000008000, W3=0x000180000000C000. Field positions and fractional scales come from SGI Table 12 and Patent FIGS. 65–67; the literals were composed directly from those positions, independent of any Tendies64 helper. This packet is not a valid-geometry or pixel fixture.

### Shade layout witness, raw opcode 0x0C

Use the same W1–W3 edge values above and W0=0x0C00000C00080005. Append these eight 64-bit shade rows:

| Word | HI32 / LO32 | Expected decoded fields, high to low |
|---|---|---|
| W4 | 00010002 / 00030004 | Red=1, Green=2, Blue=3, Alpha=4 integer |
| W5 | 00050006 / 00070008 | DrDx=5, DgDx=6, DbDx=7, DaDx=8 integer |
| W6 | 0009000A / 000B000C | Red fraction=9, Green=10, Blue=11, Alpha=12 |
| W7 | 000D000E / 000F0010 | DrDx fraction=13, DgDx=14, DbDx=15, DaDx=16 |
| W8 | 00110012 / 00130014 | DrDe=17, DgDe=18, DbDe=19, DaDe=20 integer |
| W9 | 00210022 / 00230024 | DrDy=33, DgDy=34, DbDy=35, DaDy=36 integer |
| W10 | 00310032 / 00330034 | DrDe fraction=49, DgDe=50, DbDe=51, DaDe=52 |
| W11 | 00410042 / 00430044 | DrDy fraction=65, DgDy=66, DbDy=67, DaDy=68 |

For example, the positive split value Red is 1 + 9/65536 and DrDx is 5 + 13/65536. This vector is a shade-field order/offset witness only; it makes no claim about signed derivative encodings, interpolation, or pixels. Source rows are SGI Table 13, pp. 16–17, independently confirmed by Patent FIGS. 69–71.

## Numeric encoding audit

The patent's descriptive text and actual diagrams do not close the numeric gap. FIGS. 65–76 (Sheets 76–79) independently show the raw edge, shade, texture, and Z rows, but do not label the split coefficients as signed or unsigned and do not state a two's-complement convention. The patent's explicit signed-integer example is FIG. 12B (Sheet 21): it describes the integer part of an example matrix as 16-bit signed integers, with 16-bit fractional parts. It does not connect this example matrix format to the RDP triangle coefficient rows. Do not transfer that matrix format to the RDP packet. A full-text check found no two's-complement declaration attached to the triangle fields. [CITED: U.S. Patent 6,556,197 B1, FIGS. 12A–12B, Sheet 21, and FIGS. 65–76, Sheets 76–79, https://patents.google.com/patent/US6556197B1/en]

The primary SGI layouts explicitly label only YL/YM/YH as `s11.2`; they do not define the meaning of `s` as a bit encoding convention. For the other edge fields, shade, texture, and Z rows, the SGI tables/diagrams identify integer and fraction halves or named coefficient fields but do not declare signedness or two's-complement encoding. Nintendo's SDK `gbi.h` says that the RSP generates these raw RDP triangle commands and that the hardware reads the low six opcode bits; it defines the option masks but no coefficient packing/type. The Nintendo programming manual documents signed fixed-point texture-rectangle inputs and separately documents blender Z calculations (`0,15.3` and `s15`); those are different primitive/input and pipeline values, not declarations for the raw triangle packet fields. [CITED: SGI Tables 12–15 and diagrams, pp. 13–20, https://ultra64.ca/files/documentation/silicon-graphics/SGI_RDP_Command_Summary.pdf; Nintendo SDK gbi.h, “generated RDP commands” and triangle masks, https://ultra64.ca/files/documentation/online-manuals/man/header/gbi.htm; Nintendo 64 Programming Manual, Fig. 15-1 and §16 “Z Calculation” / “Z Image Format”, https://ultra64.ca/files/documentation/nintendo/Nintendo_64_Programming_Manual_NU6-06-0030-001G_HQ.pdf]

The manual's Z discussion is the closest related primary declaration: for z-buffer calculations in the blender, it gives an 18-bit `0,15.3` Z format and a 16-bit DeltaZ used as `s15`; the surrounding section says setup calculates dZdx/dZdy and the blender combines their magnitudes for DeltaZ. It does not identify that composite blender DeltaZ type with the raw triangle `Z`, `DzDx`, `DzDe`, or `DzDy` packet halves, so that relationship remains unresolved here. [CITED: Nintendo 64 Programming Manual, pp. 320–322, §16 “Z Calculation” and “Z Image Format”, https://ultra64.ca/files/documentation/nintendo/Nintendo_64_Programming_Manual_NU6-06-0030-001G_HQ.pdf]

**Exact remaining numeric gap:** no inspected primary Nintendo/SGI source establishes the sign encoding for YL/YM/YH; the signedness/encoding of XL/XM/XH and DxLDy/DxMDy/DxHDy; or the signedness/encoding and complete numeric types of the shade, texture, and Z base/derivative fields. The split halves and word positions are known, and the literal witnesses exercise those positions, but a signed numeric decoder or negative-coefficient expected vector still lacks a source-backed contract. The positive readings in the vectors are layout-derived values only.

## Gate status

**Structural packet lengths and bit-field positions: resolved. Primary numeric proof: unresolved. Owner-approved software numeric policy: authorized by D-11 (2026-11-17).** The former numeric pre-decoder stop is relaxed only for the explicitly named policy below. This does not turn an inference into a vendor fact or authorize hardware-conformance claims. No source code, ROM, emulator oracle, console, or test was used; no protected document or asset was added to the repository.

## Sources inspected

- SGI, Nintendo Ultra64 RDP Command Summary v2.0: cover and printed pp. 12–20, especially Table 11, Tables 12–15, and the edge, shade, texture, and Z diagrams. Working mirror: https://ultra64.ca/files/documentation/silicon-graphics/SGI_RDP_Command_Summary.pdf
- Nintendo SDK gbi.h, revision 1.138 dated 1998-12-24: generated RDP triangle command bytes, four option masks, and lower-six-bit hardware note. https://ultra64.ca/files/documentation/online-manuals/man/header/gbi.htm
- Nintendo Co Ltd / Silicon Graphics Inc, U.S. Patent 6,556,197 B1, published 2003-04-29: FIGS. 65–68 edge; 69–71 shade; 72–74 texture; 75–76 Z. https://patents.google.com/patent/US6556197B1/en
- Nintendo 64 Programming Manual, draft: Fig. 15-1 documents texture-rectangle formats; §16, pp. 320–322, documents blender Z/DeltaZ formats, which are not raw triangle coefficient declarations. https://ultra64.ca/files/documentation/nintendo/Nintendo_64_Programming_Manual_NU6-06-0030-001G_HQ.pdf
- Nintendo, Intro to the N64, §4-2-7 “Drawing Triangles”: approximate full command size and 64-byte texture removal cross-check. https://ultra64.ca/files/documentation/nintendo/Nintendo_64_Intro_to_the_N64.pdf

## Mechanical layout-witness check

The orchestrator decoded the positive W0 witness directly from the pinned
bit positions and caught a misplaced YL byte before any decoder implementation.
Corrected W0 is `0x0800000C00080005`: YL=12, YM=8, YH=5. The corresponding
shade word is `0x0C00000C00080005`. This arithmetic check settles no additional
signed-format question; the primary numeric evidence gap remains unchanged; D-11 governs implementation authority.

## Owner-approved software numeric policy — D-11

Name the implementation policy `triangle-twos-complement-q16-v1` and bind it to
the affected profile semantics. These rows are an owner-selected software
contract, not newly discovered vendor facts:

| Field class | Software interpretation |
| --- | --- |
| YL/YM/YH | Sign-extend the14-bit field as two's-complement; scale by1/4. |
| XL/XM/XH and edge slopes | Combine the integer/fraction halves into32bits; sign-extend as two's-complement; scale by1/65536. |
| Shade base/derivative pairs | Combine each documented pair into32bits and use the same signed1/65536 policy. Final channel range/clamp is explicitly documented software behavior. |
| Texture and Z pairs, when their owning slices implement them | Use the same provisional32-bit two's-complement pair policy; preserve source gaps and state any further depth/perspective policy in that slice. |

Use defined wide integer arithmetic and explicit floor/rounding rules; do not
rely on C17 out-of-range signed casts or negative right shifts. Negative/extreme
coefficient tests are authored software-policy tests. Positive layout witnesses
above still test primary positions independently. This policy does not settle
subpixel coverage, color/depth precision, overflow quirks or hardware timing.

Primary SDK generated-triangle notes also say raw edge-only triangles use the
primitive color and are not fill-cycle primitives. The first accepted triangle
vector must set an appropriate non-fill cycle explicitly; COPY2/FILL3 may not
be repurposed merely to share rectangle helpers. Pin the selected supported
cycle/primitive-color behavior in the slice and exercise a literal valid mode
before expanding its matrix. [Primary SDK generated-command notes](https://ultra64.ca/files/documentation/online-manuals/man/header/gbi.htm)

D-11 policy consistency was adversarially checked after the owner decision:
primary structure and corrected positive witnesses remain distinct from the
software numeric table; no concrete inconsistency was found. The primary numeric
gap and all rights/hardware qualification stops remain recorded.
