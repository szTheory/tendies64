# Plan 07-09 VI register, timing and geometry preflight

**Checked:** 2026-11-17  
**Scope:** Primary register/field/timing facts for Plan 07-09; no code, builds, tests, guest assets, hardware, or oracle use.  
**Confidence:** MEDIUM. The register facts come from Nintendo's SGI-authored SDK header and Nintendo's programming manuals; several scanout/timing rules remain unspecified there.

## Source basis and authority

The primary register source is Nintendo SDK `rcp.h` (header copyright identifies Silicon Graphics, Inc., 1995), VI block, lines 471–593. [CITED: https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm] The primary behavior sources are Nintendo's *Programming Manual*, Chapter 30, sections 30.1.3.1, 30.1.3.4, 30.1.4.5 and 30.1.5, and the Nintendo Functions Reference `osVi` page. [CITED: https://jrra.zone/n64/doc/pro-man/pro30/30-01.htm; https://ultra64.ca/files/documentation/online-manuals/functions_reference_manual_2.0i/os/osVi.html]

## Register inventory

Base address is `0x04400000`. The known-fields masks below are arithmetic unions of the documented bit ranges; **they are not verified bus write masks**. The SDK header marks each listed register R/W. It does not define the treatment of every reserved or undocumented bit. [CITED: Nintendo SDK `rcp.h`, VI register block, lines 471–593]

| Register | Address | Access | Documented fields (bit range: width) | Known-fields mask | Source-backed meaning |
|---|---:|---|---|---:|---|
| STATUS / CONTROL | `0x04400000` | R/W | type `[1:0]:2`; gamma-dither `[2]:1`; gamma `[3]:1`; divot `[4]:1`; serrate `[6]:1`; AA `[9:8]:2`; dither-filter `[16]:1` | `0x0001035F`* | Type 0 blank; 1 reserved; 2 is labeled 16-bit but its listed component widths sum to 18; 3 is 32-bit RGBA. Serrate is on for interlace/off otherwise. AA modes 0–3 are defined in the header. |
| ORIGIN | `0x04400004` | R/W | `[23:0]:24` | `0x00FFFFFF` | Frame-buffer origin in bytes. |
| WIDTH | `0x04400008` | R/W | `[11:0]:12` | `0x00000FFF` | Frame-buffer line width in pixels. |
| INTR | `0x0440000C` | R/W | `[9:0]:10` | `0x000003FF` | Interrupt when current half-line equals V_INTR. |
| CURRENT | `0x04400010` | R/W | `[9:0]:10` | `0x000003FF` | Current half-line, sampled once per line. LSB is constant within a field; in interlaced mode it gives field number. Any write clears the interrupt line. |
| BURST / TIMING | `0x04400014` | R/W | H-sync width `[7:0]:8`; burst width `[15:8]:8`; V-sync width `[19:16]:4`; burst start from H-sync `[29:20]:10` | `0x3FFFFFFF` | Video timing/burst controls; the source labels V-sync width in half-lines and burst start in pixels from H-sync. |
| V_SYNC | `0x04400018` | R/W | `[9:0]:10` | `0x000003FF` | Number of half-lines per field. |
| H_SYNC | `0x0440001C` | R/W | line duration `[11:0]:12`; leap pattern `[20:16]:5` | `0x001F0FFF` | Lower field is total line duration in 1/4-pixel units; upper field is a 5-bit leap pattern used for PAL only. |
| LEAP | `0x04400020` | R/W | `[11:0]:12`; `[27:16]:12` | `0x0FFF0FFF` | Both fields are described as copies of the named H_SYNC field; see source ambiguity below. |
| H_START | `0x04400024` | R/W | end `[9:0]:10`; start `[25:16]:10` | `0x03FF03FF` | Active horizontal video end/start, in screen pixels. End is low; start is high. |
| V_START | `0x04400028` | R/W | end `[9:0]:10`; start `[25:16]:10` | `0x03FF03FF` | Active vertical video end/start, in screen half-lines. End is low; start is high. |
| V_BURST | `0x0440002C` | R/W | end `[9:0]:10`; start `[25:16]:10` | `0x03FF03FF` | Color-burst enable end/start, in half-lines. End is low; start is high. |
| X_SCALE | `0x04400030` | R/W | reciprocal scale `[11:0]:12`; subpixel offset `[27:16]:12` | `0x0FFF0FFF` | Both are 2.10 fields; low field is reciprocal horizontal scale-up factor. |
| Y_SCALE | `0x04400034` | R/W | reciprocal scale `[11:0]:12`; subpixel offset `[27:16]:12` | `0x0FFF0FFF` | Both are 2.10 fields; low field is reciprocal vertical scale-up factor. |

*STATUS has a primary-source inconsistency: its leading comment limits the valid-bit range to bit 15, but later in the same header it defines `VI_CTRL_DITHER_FILTER_ON` as bit 16. Preserve bit 16 as a documented field; do not silently truncate STATUS to 16 bits. Undocumented/reserved-bit readback and write behavior still need a named software policy. [CITED: Nintendo SDK `rcp.h`, lines 475–497 and 576–586]

**LEAP wording ambiguity:** H_SYNC `[20:16]` is called a 5-bit PAL leap pattern (`h_sync_period`), while the LEAP register's two 12-bit ranges are each described as duplicating the field named `h_sync_period`. The header does not explain this width/name mismatch. Keep the full raw LEAP word, but do not infer a PAL cadence from this comment; PAL is outside this selected NTSC profile. [CITED: Nintendo SDK `rcp.h`, lines 533–544]

## Primary timing, field and latch facts

- `V_SYNC[9:0]` denotes half-lines per field; `CURRENT[9:0]` reports a half-line and is sampled once per line. In interlace, `CURRENT` bit 0 identifies the field and remains constant during that field; the manual describes odd/even fields as alternating. `SERRATE` is documented on for interlaced operation and off for non-interlaced operation. [CITED: Nintendo SDK `rcp.h`, VI block; Nintendo Programming Manual §30.1.3.4 and §30.1.4.5]
- `INTR[9:0]` compares against the current half-line. Any write to `CURRENT` clears the VI interrupt line. The SDK header separately defines `MI_INTR_VI = 0x08` and `MI_INTR_MASK_VI = 0x08`; the VI comments do not specify whether a CURRENT write also clears the MI pending latch, nor whether the written value changes CURRENT readback. Keep those effects explicit in the planned MI integration instead of inferring them from the compare description. [CITED: Nintendo SDK `rcp.h`, lines 435–467 and 507–519]
- Nintendo's manual gives nominal modes as NTSC 525 scan lines/60 Hz, PAL 625/50 Hz and MPAL 525/60 Hz. It describes an NTSC frame as about 1/30 second, with alternating odd/even fields at about 1/60-second intervals. These are TV-mode descriptions, not a conversion from Tendies64 guest ticks to exact VI event ticks. [CITED: Nintendo Programming Manual §30.1.3.1 and §30.1.3.4]
- The OS `osViSetMode` path overwrites VI registers at the next vertical retrace; the manual says scaling and special-feature changes made through the OS are updated at the next V-blank. This documents the library's managed mode-change behavior, **not a hardware latch rule for arbitrary guest MMIO writes**. [CITED: Nintendo Functions Reference `osVi`, display-mode and VI-management sections; Programming Manual §30.1.5]

## Source clocks vs. existing model

| Value | Status | Provenance |
|---|---|---|
| NTSC VI source clock `48,681,812 Hz` | SDK constant | Nintendo SDK `rcp.h`: `VI_NTSC_CLOCK 48681812` |
| PAL VI source clock `49,656,530 Hz` | SDK constant | Nintendo SDK `rcp.h`: `VI_PAL_CLOCK 49656530` |
| MPAL VI source clock `48,628,316 Hz` | SDK constant | Nintendo SDK `rcp.h`: `VI_MPAL_CLOCK 48628316` |
| Guest tick rate `62,500,000 ticks/s` | Tendies64 software input | Existing profile text; not sourced hardware clock |
| Field work period `(H_SYNC + 1) × (V_SYNC + 1)` | Tendies64 software policy | Existing profile text; not a primary-source timing equation |

The repository's exact policy fields are:

`vi_clock_policy`: `rsp62500000-ntsc48681812-rational-v1`.
Guest tick unit: `model-rsp-cycle`.
Guest tick rate: 62,500,000 ticks per second, a Tendies64 software model input.
Field work period: `(H_SYNC + 1) × (V_SYNC + 1)` VI cycles.

[VERIFIED: `docs/spec/vi.md:18-26`]

This mapping must remain labeled as the current software profile policy. The primary register header describes H_SYNC as a **line duration in 1/4-pixel units**, but V_SYNC as a **half-line count per field**; it does not state that either register is a terminal count, define the `+1` convention, or provide the exact register-units-to-guest-ticks formula. Multiplying one H_SYNC line-duration unit by each V_SYNC half-line therefore raises a likely missing half-line conversion/fencepost concern; that is a dimensional inference, not a verified hardware period. The current product is not hardware-timing evidence. Preserve existing `ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v2` behavior under D-08; any retained/new latch and phase defaults are reversible, named software policies under D-05/D-08, not new owner decision gates. [CITED: Nintendo SDK `rcp.h`; ASSUMED: dimensional interpretation; VERIFIED: `.planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-CONTEXT.md:55-60,79-83`]

## Independent literal field and timing witness

The following is an authored discriminating vector, **not a Nintendo standard-mode preset**. Fields are unpacked from the raw words using the SDK positions above; the geometry and timing results are kept separate from silicon claims.

| Register | Literal word | Mechanical decode |
|---|---:|---|
| STATUS | `0x00000002` | Type `[1:0]=2`, documented as a 16-bit pixel mode. |
| ORIGIN | `0x00003000` | Frame-buffer byte origin `0x3000`. |
| WIDTH | `0x00000004` | Four pixels per source line. |
| INTR | `0x00000002` | Compare target: current half-line 2. |
| CURRENT | read `0x00000000` | Current half-line 0; field bit 0. Do not treat this initial read as a write. |
| BURST | `0x00000000` | All documented burst/sync-width subfields zero. |
| V_SYNC | `0x00000002` | Raw field value 2; primary comment says this register is half-lines per field and does not state `+1`. |
| H_SYNC | `0x00000004` | Raw line-duration field 4; no PAL leap bits set. |
| LEAP | `0x00000000` | Both documented 12-bit fields zero. |
| H_START | `0x00040008` | Start `4`, end `8`; horizontal coordinate difference 4 pixels. |
| V_START | `0x00020008` | Start `2`, end `8`; vertical coordinate difference 6 half-lines. |
| V_BURST | `0x00000000` | Burst start/end zero. |
| X_SCALE | `0x00000400` | Low 2.10 scale is `0x400` (=1.0); high offset is zero. |
| Y_SCALE | `0x00000400` | Low 2.10 scale is `0x400` (=1.0); high offset is zero. |

From the documented type and width, a packed software buffer has row payload `4 pixels × 2 bytes = 8 bytes`; if the model chooses contiguous rows, row 0 begins at `0x3000` and row 1 at `0x3008`. This row-address progression is a software packing inference, not an explicit equation in the primary header. The source establishes the H coordinate span and V half-line span, but not inclusive/exclusive endpoint handling, conversion of half-line span to output rows, crop/sample origin, or rounding under scaling. Do not claim that these words imply a primary-specified 4×3 packet. [CITED: Nintendo SDK `rcp.h`; ASSUMED: contiguous software row progression]

The repository's fixture text is:

The fixture programs source origin `0x3000`, source width 4, H/V active intervals of 4 and 3, H/V sync values 4 and 2, and X/Y scale `0x400`.

[VERIFIED: `docs/spec/vi.md:31-37`] Using those existing software-period inputs and rates, `(4+1)×(2+1)=15` modeled VI cycles, then `ceil(15×62,500,000/48,681,812)=20` guest ticks. Changing the actual H_SYNC field from `4` to `5` (while V_SYNC remains `2`) gives `18` modeled cycles and `ceil(18×62,500,000/48,681,812)=24` guest ticks. This is a real input/effect contrast, unlike splitting/rejoining the same packed word; both results validate only the named software policy. [VERIFIED: `docs/spec/vi.md:24-36` for the policy and arithmetic independently recomputed]

**Format contrast:** Keep the same `WIDTH=4` but change STATUS type from `2` to `3`. The primary header describes these as 16-bit and 32-bit respectively, so packed software row payload changes from 8 bytes to 16 bytes. This is a genuinely different geometry/source-stride case, not an equivalent representation. The SDK header labels mode 2 as 16-bit even though its listed component widths total 18. Nintendo's manual states that 16-bit pixels have 5 bits for each RGB channel. Preserve the existing profile's named RGBA5551 conversion behavior; document the primary-format ambiguity instead of changing it or claiming the source resolved its alpha bit. [CITED: Nintendo SDK `rcp.h`; Nintendo Programming Manual §30.1.3.5; VERIFIED: `docs/spec/vi.md:39-45`]

## Plan-facing gaps and direction

| Gap | Plan 07-09 should do |
|---|---|
| Exact VI-clock-to-register-unit conversion, H/V terminal-count interpretation, and LEAP effect | Retain the current profile policy where already identity-bound; give any selected standard-NTSC conversion a named reversible profile policy and independent literal expected ticks. Do not describe the current product formula as a hardware-derived field period. |
| Direct MMIO write visibility/latch boundary | Use a named, reversible software latch default. The documented next-V-blank behavior applies to libultra-managed updates only. Test before/on/after the chosen boundary and preserve older profile identity/state bytes where unchanged. |
| Active endpoint inclusivity, scaled dimension rounding, source crop/address stepping, and which source line a field/parity selects | Make these explicit software sampler policies and pin nonzero offsets, non-unit scales, width-4/16/32-bit cases, and parity. Keep packet width/height/stride/format/field/filter/build/profile/timing metadata equal to the actual selected software geometry. |
| STATUS reserved-bit/readback behavior and the format-2 16-bit component description | Preserve the documented bit-16 field despite the header's `[15:0]` comment; name a reversible reserved-bit policy. Keep the current RGBA5551 software profile behavior; leave hardware alpha interpretation unresolved. |
| CURRENT write vs. MI pending/IRQ state | Assert the documented any-write interrupt-line clear. Separately specify and test MI pending/mask propagation; don't infer that CURRENT writeback changes its sampled line value. |

These are implementation-policy gaps already covered by approved D-05/D-07/D-08 discretion. They do not require a new owner checkpoint. Plan 07-09 remains authored software evidence; no VI hardware-accuracy or VID-04 claim follows from this preflight. [VERIFIED: `.planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-CONTEXT.md:55-60,69-83`; `.planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-09-PLAN.md:67-70`]

## Sources

- Nintendo SDK `rcp.h`, VI registers and clock constants, lines 471–593; source header copyright line 5 identifies Silicon Graphics, Inc. (1995): https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm
- Nintendo 64 Programming Manual, Chapter 30, §§30.1.3.1, 30.1.3.4, 30.1.3.5, 30.1.4.5, 30.1.5: https://jrra.zone/n64/doc/pro-man/pro30/30-01.htm
- Nintendo Functions Reference, VI Management page, sections on display modes and VI management, copyright 1998: https://ultra64.ca/files/documentation/online-manuals/functions_reference_manual_2.0i/os/osVi.html
- Existing Tendies64 VI model and source-vs-policy boundary: `docs/spec/vi.md:9-45`.
