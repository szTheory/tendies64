# Plan 07-10 VI filter-fields preflight

**Checked:** 2026-11-17  
**Scope:** Primary Nintendo VI filter facts for Plan07-10; no source edits, builds, tests, guests, hardware, or emulator-oracle use.  
**Confidence:** MEDIUM. The enable conditions and silhouette interpolation are documented. Several filter equations, edge rules, and stage ordering are not.

## Primary findings

| Area | Primary fact | What the source does not settle |
|---|---|---|
| AA STATUS `[9:8]` | Nintendo SDK [`rcp.h`](https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm), VI block: `0` = AA+resample, always fetch extra lines; `1` = AA+resample, fetch extra lines only if needed; `2` = resample only and treat samples as fully covered; `3` = neither, replicate pixels without interpolation. Modes 0 and 1 differ in fetch policy, not a documented visible-color equation. | The manual does not give the exact fractional resampling kernel, sample phase, edge extension, or fixed-point rounding. The header does not explain what “if needed” tests.
| Silhouette AA | Nintendo *Programming Manual*, Ch. 16, printed pp. 326–327, gives `out = coverage × foreground + (1 − coverage) × background`. Foreground is the stored pixel; background comes from fully covered pixels in a 5×3 neighborhood. The Nintendo [graphics tutorial](https://ultra64.ca/files/documentation/online-manuals/man-v5-2/allman52/tutorial/graphics/6/6_5.htm) describes selecting the most different color in that neighborhood. | Neither source defines the color-distance metric, candidate tie-break, neighborhood boundary behavior, or integer rounding. Keep these as named software policy.
| Coverage input | The primary manual, Ch. 16 printed pp. 306–307, describes a 4×4 subpixel mask dithered to coverage 0–8; coverage 8 is full and 1 is 1/8. VI AA consumes coverage. Tendies64's current [RDP software contract](../../../docs/spec/rdp.md) keeps its logical coverage sidecar separately from RGBA color; do not infer coverage from framebuffer alpha or an undocumented hidden-bit byte layout. | Tendies64's mask-to-0–8 rule is a software representation, not a claim that the hardware uses that exact mask or mapping. Keep it unchanged for the existing profile.
| Divot | [`osViSetSpecialFeatures`](https://ultra64.ca/files/documentation/online-manuals/functions_reference_manual_2.0i/os/osViSetSpecialFeatures.html): divot acts only in AA mode and only outside full-coverage regions; it targets one-pixel notches when silhouettes overlap. The programming manual notes it can thin/break decal lines at silhouettes. | No primary pixel-selection equation, exact trigger pattern, tie rule, or order relative to AA/resampling is given. A literal corrected RGB value cannot be independently claimed from these docs.
| Gamma and gamma dither | STATUS bit 3 enables gamma; bit 2 enables gamma dither. The SDK header calls both normally on; the OS feature reference says gamma dither has effect only when gamma is enabled. | No exact gamma transfer table/equation, gamma-dither matrix/amplitude/rounding, or ordering between gamma and gamma dither is specified. The manual's advice to square already corrected colors to undo a prior correction is not a forward hardware transfer equation.
| Dither filter | STATUS bit 16 enables the VI filter. Nintendo's [`gDPSetColorDither`](https://ultra64.ca/files/documentation/online-manuals/functions_reference_manual_2.0i/gdp/gDPSetColorDither.html) explains that RDP color is 8-bit before a 16-bit framebuffer stores 5-bit channels, and that VI uses neighboring pixels to recover information dispersed by ordered dither; it recommends the Magic Square pattern when VI recovery is enabled. The programming manual, printed p. 501, says the filter fetches three scanlines and reduces them to one. | The VI recovery matrix, exact reconstruction, channel rounding, boundary handling, and ordering against scaling/gamma are not specified. Do not substitute the RDP's dither matrix for a VI reconstruction formula.
| Filter choice / ordering | Nintendo's [VI reference](https://ultra64.ca/files/documentation/online-manuals/functions_reference_manual_2.0i/os/osVi.html) states the backend can run AA or dither recovery, not both at once. A partial-coverage pixel selects AA and disables dither recovery there. Thus with both control bits set, full-coverage pixels may use dither recovery while partial-coverage pixels use AA. | Sources do not provide a complete total order for resampling, divot, gamma, and gamma dither. Treat the documented AA-versus-dither choice as a conditional selection rule, not evidence for an otherwise ordered hardware pipeline.
| Defaults / formats | OS mode defaults are gamma on, gamma dither on, divot on, dither filter on for 16-bit and off for 32-bit; `osViSetMode` restores mode-table defaults. This is an SDK mode-table policy, not a silicon reset guarantee. `rcp.h` calls STATUS type 2 “5/5/5/3 (16 bit)” (component widths sum to 18) and separately names dither filter bit 16 despite describing STATUS valid bits as `[15:0]`. | Keep the existing profile's RGBA5551 interpretation and bytes. The header comments are inconsistent; do not reinterpret legacy data or use alpha as coverage to resolve them.

The OS reference also describes preset field behavior: low-resolution interlace weights adjacent lines 75/25 and 25/75 on alternating fields; high-resolution normal interlace uses each field's own rendered lines; deflicker interlace averages adjacent lines. These are mode-table descriptions, not a reason to rewrite Plan09's explicitly selected raw-register/serrate software policy.

## Literal independent witness

Raw STATUS is a 32-bit value. The documented pieces decode mechanically:

| STATUS | Decode |
|---:|---|
| `0x00000002` | Type 2; AA mode 0; gamma, gamma dither, divot and dither filter off. |
| `0x00000202` | Type 2; AA mode 2 (resample only/full coverage); the same special features off. |
| `0x0001001E` | Type 2 + bits 2, 3, 4 and 16; AA mode 0. Both AA and dither-filter controls are requested; documented per-pixel selection must use AA for partial coverage and cannot apply both filters there. |

For STATUS `0x00000002`, provide a 3×5 neighborhood whose fourteen fully covered neighbors all contain RGB `(40,120,200)`. Give the center partial pixel coverage `2/8` and RGB `(200,80,40)`. The neighborhood makes the background color unambiguous regardless of “most different” tie rules. Nintendo's equation yields, channel by channel, `(1/4 × 200 + 3/4 × 40, 1/4 × 80 + 3/4 × 120, 1/4 × 40 + 3/4 × 200) = (80,110,160)` exactly; no integer rounding is involved. With STATUS `0x00000202` at an integer sample position, coverage is treated as full, so the same input remains `(200,80,40)` before any separately enabled output feature. This is a real behavior contrast, not alternate encodings of one case.

## Plan guidance and reversible software boundary

- Preserve existing VI profile IDs, output bytes, and the no-filter path. Keep raw source pixels and the separate coverage plane as distinct inputs to VI; packet metadata must name the selected source format, actual filter behavior, field, and profile.
- Implement AA's documented rational equation directly. Name and test any added integer rounding, “most different” metric/tie, and edge-neighborhood policy; these additions are software approximation, not primary facts.
- Treat fractional resampling, divot correction, gamma, gamma dither, and VI dither recovery as separate named software policies until their numerical rules are selected. A conservative initial filter profile may keep under-specified divot/gamma-dither/dither-recovery paths disabled while retaining readable raw STATUS bits; if Plan10 requires them enabled, specify exact deterministic lookup/equation, tie, edge, and stage rules before writing assertions. Do not call either choice hardware-accurate.
- For AA modes 0 and 1, assert equal documented pixel results and keep their extra-line fetch distinction out of output expectations unless a separate memory-access model is explicitly selected. Mode 2 must bypass coverage-driven AA; mode 3 must replicate without interpolation.
- Record that “both control bits set” is valid input but does not mean both back-end filters execute on one pixel. Filter metadata should report the actual selected per-pixel path, rather than merely echoing control bits.

## Sources

- Nintendo / SGI SDK [`rcp.h`](https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm), VI STATUS definitions and mode bits.
- Nintendo *N64 Programming Manual*, Ch. 16, “Coverage Unit” (printed pp. 306–307) and “Video Filter” (printed pp. 326–327), [manual PDF](https://ultra64.ca/files/documentation/nintendo/Nintendo_64_Programming_Manual_NU6-06-0030-001G_HQ.pdf).
- Nintendo [graphics tutorial, §6.5 Video Filter](https://ultra64.ca/files/documentation/online-manuals/man-v5-2/allman52/tutorial/graphics/6/6_5.htm).
- Nintendo [Functions Reference: `osVi`](https://ultra64.ca/files/documentation/online-manuals/functions_reference_manual_2.0i/os/osVi.html), [`osViSetSpecialFeatures`](https://ultra64.ca/files/documentation/online-manuals/functions_reference_manual_2.0i/os/osViSetSpecialFeatures.html), and [`gDPSetColorDither`](https://ultra64.ca/files/documentation/online-manuals/functions_reference_manual_2.0i/gdp/gDPSetColorDither.html).
