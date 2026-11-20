# Plan 07-06: RDP SetCombine Raw Contract

Scope: pin raw selector fields and a small independent arithmetic witness for Plan 07-06. This is not a silicon-accuracy claim. Source links below were opened successfully on 2026-11-17; only facts directly stated by primary documentation are labeled primary facts.

## Primary evidence and packet framing

- **[PRIMARY FACT]** SGI, *Nintendo Ultra64 RDP Command Summary*, v2.0 (1996), Table 19, printed pp. 25–26: SetCombine command field is six-bit `0x3C` in raw bits 61–56; it implements `(A-B)*C+D` separately for RGB and alpha, and carries separate selector groups `cycle 0` and `cycle 1`. The neighboring diagram/table gives the raw positions below. [Source PDF](https://ultra64.ca/files/documentation/silicon-graphics/SGI_RDP_Command_Summary.pdf).
- **[PRIMARY FACT]** Nintendo, *Nintendo 64 Programming Manual Draft*, §13, Figures 13-10/13-11, printed pp. 196–198: selector meanings below. §13 pp. 200–201 says one-cycle `mode1` and `mode2` should match and two-cycle mode performs two interpolation computations. Its custom-mode text defines `COMBINED` as the combined output of the first mode. [Source PDF](https://ultra64.ca/files/documentation/nintendo/Nintendo_64_Programming_Manual_NU6-06-0030-001G_HQ.pdf).
- **[PRIMARY FACT]** SDK `gbi.h` revision 1.138 (1998-12-24): GBI SetCombine byte is `0xFC`; its `gsDPSetCombineLERP` packing maps the first mode to SGI cycle 0 fields and second mode to cycle 1 fields. Thus raw opcode field `0x3C` is distinct from the wire/GBI command byte `0xFC` (the top two command-byte bits are `11`). The same header maps `G_CYC_2CYCLE` to `1 << 20`, with `G_MDSFT_CYCLETYPE=20`. [Source header](https://ultra64.ca/files/documentation/online-manuals/man/header/gbi.htm).
- **[PRIMARY FACT]** SGI Table 20, printed p. 27, assigns `cycle_type` to raw bits 53–52: `00`=one-cycle, `01`=two-cycle, `10`=copy, `11`=fill. With all other fields zero, the two-cycle SetOtherModes command word is `0xEF10000000000000` (GBI byte `0xEF`, raw six-bit opcode `0x2F`).

## SetCombine packet bit contract

All positions are **64-bit packet bit numbers**, with bit 63 most significant. W0 is the upper 32 bits and W1 the lower 32 bits. Widths follow the inclusive bit ranges.

| Packet word | Bits | Width | Field |
|---|---:|---:|---|
| W0 | 61–56 | 6 | raw opcode `0x3C` (wire byte `0xFC`) |
| W0 | 55–52 | 4 | RGB A, cycle 0 |
| W0 | 51–47 | 5 | RGB C, cycle 0 |
| W0 | 46–44 | 3 | alpha A, cycle 0 |
| W0 | 43–41 | 3 | alpha C, cycle 0 |
| W0 | 40–37 | 4 | RGB A, cycle 1 |
| W0 | 36–32 | 5 | RGB C, cycle 1 |
| W1 | 31–28 | 4 | RGB B, cycle 0 |
| W1 | 27–24 | 4 | RGB B, cycle 1 |
| W1 | 23–21 | 3 | alpha A, cycle 1 |
| W1 | 20–18 | 3 | alpha C, cycle 1 |
| W1 | 17–15 | 3 | RGB D, cycle 0 |
| W1 | 14–12 | 3 | alpha B, cycle 0 |
| W1 | 11–9 | 3 | alpha D, cycle 0 |
| W1 | 8–6 | 3 | RGB D, cycle 1 |
| W1 | 5–3 | 3 | alpha B, cycle 1 |
| W1 | 2–0 | 3 | alpha D, cycle 1 |

The selector letters always feed `(A-B)*C+D`; do not reorder fields to match the GBI function argument presentation. SGI Table 19 and SDK packing agree on these positions.

## Selector denominator

Entries marked `zero` are documented zero aliases, not unknown encodings. All codes in the documented selector widths are accounted for.

| Channel / slot | Width | Code → selected source |
|---|---:|---|
| RGB A | 4 | 0 Combined; 1 Texel0; 2 Texel1; 3 Primitive; 4 Shade; 5 Environment; 6 One; 7 Noise; 8–15 Zero |
| RGB B | 4 | 0 Combined; 1 Texel0; 2 Texel1; 3 Primitive; 4 Shade; 5 Environment; 6 Key Center; 7 Convert K4; 8–15 Zero |
| RGB C | 5 | 0 Combined; 1 Texel0; 2 Texel1; 3 Primitive; 4 Shade; 5 Environment; 6 Key Scale; 7 Combined Alpha; 8 Texel0 Alpha; 9 Texel1 Alpha; 10 Primitive Alpha; 11 Shade Alpha; 12 Environment Alpha; 13 LOD Fraction; 14 Primitive LOD Fraction; 15 Convert K5; 16–31 Zero |
| RGB D | 3 | 0 Combined; 1 Texel0; 2 Texel1; 3 Primitive; 4 Shade; 5 Environment; 6 One; 7 Zero |
| Alpha A, B, D | 3 each | 0 Combined Alpha; 1 Texel0 Alpha; 2 Texel1 Alpha; 3 Primitive Alpha; 4 Shade Alpha; 5 Environment Alpha; 6 One; 7 Zero |
| Alpha C | 3 | 0 LOD Fraction; 1 Texel0 Alpha; 2 Texel1 Alpha; 3 Primitive Alpha; 4 Shade Alpha; 5 Environment Alpha; 6 Primitive LOD Fraction; 7 Zero |

**Evidence:** Nintendo Manual §13 Figures 13-10/13-11 (printed pp. 196–198), visually inspected, with SDK `gbi.h` selector constants as an independent producer declaration. Numeric aliases such as code 6/7 are slot-specific; e.g. RGB A=7 is Noise, RGB B=7 is K4, and RGB C=7 is Combined Alpha.

## Cycle behavior, width, and arithmetic limits

- **[PRIMARY FACT]** Nintendo Manual §13, printed pp. 200–201: in one-cycle mode it recommends matching `mode1`/`mode2`; in two-cycle mode the combiner performs two linear-interpolation calculations. SDK packing identifies those modes with cycle-0 and cycle-1 groups. The manual identifies `COMBINED` as the output of the first mode, so cycle-1 `COMBINED` reads cycle-0 output in the two-cycle path.
- **[PRIMARY GAP]** The SGI note and Nintendo one-cycle note both require/recommend equal selector groups in one-cycle mode, but neither says which packed group is evaluated when the groups differ. No primary selector/cycle structural contradiction was found; unequal-group one-cycle behavior is unspecified by these sources.
- **[REVERSIBLE SOFTWARE POLICY]** For this first deterministic profile, one-cycle mode evaluates the cycle-0 group once and does not evaluate cycle 1. Two-cycle mode evaluates cycle 0 then cycle 1; cycle-1 `COMBINED` receives the clamped cycle-0 result. Keep this one-cycle choice explicit and profile-bound; do not describe it as vendor-confirmed.
- **[PRIMARY FACT]** Nintendo Manual §13, “Dithering,” printed p. 212, says the RDP pipeline keeps full 8-bit RGB-component precision before framebuffer dithering. §14, printed p. 248, describes most operations as 8-bit-per-component RGBA pixels. The manual’s 5-bit alpha-mux statement at printed p. 313 belongs to the **blender** mux and does not establish color-combiner alpha precision.
- **[PRIMARY GAP]** The cited primary sources do not establish SetCombine signed intermediate width, C normalization denominator, rounding rule, per-cycle clamp point, or combiner alpha intermediate width. Do not present these as hardware facts.
- **[REVERSIBLE SOFTWARE POLICY]** Use unsigned 8-bit RGBA sources/results. Interpret C as unsigned 0..255 over denominator 255; compute `floor_div((A-B)*C,255)+D` using a signed wide intermediate, then clamp to 0..255 after each evaluated cycle. This defines negative rounding toward negative infinity and makes two-cycle feedback deterministic. It is a software contract pending stronger primary evidence.
- **[REVERSIBLE SOFTWARE POLICY]** Retain the established `xorshift32-seed-1` noise and `zero-undefined-memory-color` policy names, per-instance state, refusal-without-consumption, and identity binding. Primary sources describe random/noise uses but do not settle the required noise update cadence per pixel/sample; make cadence an explicit deterministic software choice and preserve it in profile identity. No hardware accuracy claim follows from repeatable policy output.

## Independent literal witness

The first literal deliberately gives cycle 0 and cycle 1 different, fully defined nonzero selector fields, so a one-cycle group choice has distinct outputs without an undefined `COMBINED` input. This violates the vendor recommendation to program equal groups in one-cycle mode on purpose; it tests the explicit software policy below, not asserted hardware behavior.

| Group | RGB A, B, C, D | Alpha A, B, C, D |
|---|---|---|
| Cycle 0 | Texel0(1), Primitive(3), Shade(4), Environment(5) | Texel0A(1), EnvironmentA(5), Texel1A(2), PrimitiveA(3) |
| Cycle 1 | Texel1(2), Environment(5), Shade(4), Primitive(3) | Texel1A(2), PrimitiveA(3), EnvironmentA(5), ShadeA(4) |

The one-cycle SetOtherModes word is `0xEF00000000000000`. The independently assembled SetCombine packet is **`0xFC1214443556D6DC`** (W0=`0xFC121444`; W1=`0x3556D6DC`). Under the cycle-0 software policy, expected RGBA is **(110,67,92,96)**. If a software implementation instead evaluates packed cycle 1, the same defined inputs produce **(20,67,0,117)**, so this packet distinguishes the policy choice. `cycle_type=00` is the one-cycle setting.

For feedback, use a second, two-cycle witness whose cycle-1 selectors deliberately include `COMBINED` (code 0). `cycle_type=01` is set by the separate two-cycle SetOtherModes command above.

| Group | RGB A, B, C, D | Alpha A, B, C, D |
|---|---|---|
| Cycle 0 | Texel0(1), Primitive(3), Shade(4), Environment(5) | Texel0A(1), EnvironmentA(5), Texel1A(2), PrimitiveA(3) |
| Cycle 1 | Combined(0), Texel1(2), LOD Fraction(13), Primitive(3) | CombinedA(0), Texel0A(1), Primitive LOD Fraction(6), ShadeA(4) |

Using the table positions, the independently assembled SetCombine packet is **`0xFC12140D321AD6CC`**: W0=`0xFC12140D`; W1=`0x321AD6CC`. Its six-bit opcode field is `0x3C`, not `0xFC`. The cycle0 RGB fields yield W0 selector bits `1<<20 | 4<<15 | 1<<12 | 2<<9 | 13`; cycle1 RGB A/C are `0/13`. W1 selector fields are `(3<<28)|(2<<24)|(6<<18)|(5<<15)|(5<<12)|(3<<9)|(3<<6)|(1<<3)|4`.

Software-policy inputs (all distinct/nonzero except where selectors explicitly use Combined): Texel0 RGBA=(220,100,40,200); Texel1=(10,200,30,128); Primitive=(20,30,10,16); Shade=(128,64,192,100); Environment=(10,50,70,40); LOD Fraction=64; Primitive LOD Fraction=64.

Independent arithmetic under the stated policy:

| Result | RGB calculations | Alpha calculation | RGBA |
|---|---|---|---|
| Cycle 0 | `floor((220-20)*128/255)+10=110`; `floor((100-30)*64/255)+50=67`; `floor((40-10)*192/255)+70=92` | `floor((200-40)*128/255)+16=96` | (110,67,92,96) |
| Cycle 1 | `floor((110-10)*64/255)+20=45`; `floor((67-200)*64/255)+30=-4→0`; `floor((92-30)*64/255)+10=25` | `floor((96-200)*64/255)+100=73` | **(45,0,25,73)** |

The cycle-0 output is the explicit one-cycle software-policy result `(110,67,92,96)`; the two-cycle result `(45,0,25,73)` therefore discriminates cycle type and feedback. As a valid selector decoy, change cycle-1 RGB A from Combined code 0 to Texel0 code 1: the two-cycle result changes to `(72,4,12,73)`. This is an authored arithmetic witness, not a hardware oracle or a full selector-coverage fixture.

## Remaining limits

The selector denominator, command fields, command-byte distinction, and two-cycle feedback relation are sufficiently specified for planning and a software implementation. Keep the one-cycle unequal-group choice and arithmetic rules above labeled as reversible policy. Exact hardware behavior for those policies, especially combiner alpha precision/rounding and noise cadence, remains unproven; VID-04 remains blocked. No protected manuals, diagrams, code, or assets are copied into this repository.

**Preflight consistency correction:** Parent mechanical field extraction and integer arithmetic checks confirm both literal selector maps. The initial two-cycle arithmetic used a different Primitive D vector from the declared inputs; the corrected result is (45,0,25,73), with decoy (72,4,12,73). The initial draft is retained under build/gate/07-06-preflight-initial.md; no implementation or hardware claim depended on it.

## Register and missing-state clarification

The v5 software profile requires a fetched SetCombine before triangles;
missing state is refused before scheduling with no own pixel, coverage or RNG
effects. Fill/copy remain bypass operations. K4/K5 are decoded as signed9 and
clamped to0..255 before use as byte-valued sources; this is explicit software
policy pending hardware precision evidence.

The SDK gDPSetPrimColor producer distinguishes minimum level in the upper
command word's bits15:8 from primitive LOD fraction in bits7:0. Therefore the
fraction is raw64 bits39:32. An independent witness FA0012A7636E7984 selects
minimum18 and fraction167. Reading raw bits47:40 instead returns18. This source
fact is not covered by the provisional arithmetic policy.
