# Plan03 raw-command correction contract

These are externally defined input fields, independent of the implementation's
encoders and sampler. Fix them together and test fixed raw words before a long
suite. The initial green copy matrix mirrored several incorrect input fields.
Its output comparisons remain historical observations, not proof of a valid raw
RDP copy contract. The source is unpublished.

| Input | Field contract | Primary source |
| --- | --- | --- |
| SetOtherModes | cycle bits53:52: COPY=2, FILL=3 | [SGI Table20, PDF page27](https://hcs64.com/files/RDP_COMMANDS.pdf#page=27) |
| SetOtherModes | TLUT enable bit47, type bit46 (RGBA16=0, IA16=1); disabled remains disabled | [SGI Table20](https://hcs64.com/files/RDP_COMMANDS.pdf#page=27) |
| TextureRectangle word0 | XH55:44, YH43:32, tile26:24, XL23:12, YL11:0 | [SGI Table17, PDF page22](https://hcs64.com/files/RDP_COMMANDS.pdf#page=22) |
| TextureRectangle word1 | S63:48 and T47:32: signed16-bit S10.5; DSDX31:16 and DTDY15:0: signed16-bit S5.10 | [SGI Table17](https://hcs64.com/files/RDP_COMMANDS.pdf#page=22) |
| Copy screen rectangle | Inclusive endpoints; ignore the two fractional screen bits | [Nintendo TextureRectangle reference](https://ultra64.ca/files/documentation/online-manuals/functions_reference_manual_2.0i/gsp/gSPTextureRectangle.html) |
| Copy stepping | Documented ordinary DSDX is4<<10; DTDY is normally1<<10; Flip exchanges the varying axes | [Nintendo reference](https://ultra64.ca/files/documentation/online-manuals/functions_reference_manual_2.0i/gsp/gSPTextureRectangle.html) |

Literal examples for independent tests (not an encoder shared with production):

- SetOtherModes COPY: `0x2f20000f00000000` (also uses the documented reserved default nibble).
- COPY + RGBA16 TLUT: `0x2f20800f00000000`; COPY + IA16 TLUT: `0x2f20c00f00000000`.
- One-pixel rectangle at screen0,0 on tile0: first word `0x2400000000000000`.
- Second word S=32,T=0,DSDX=4096,DTDY=1024: `0x0020000010000400`.

Signed coordinate arithmetic must stay defined in C17. If a derivative remains
outside the selected profile, refuse it explicitly and atomically rather than
accepting an unsigned reinterpretation. In particular, repair accepted negative
DTDY and test nonzero/signed S and T. Equal copy endpoints are one pixel; test
empty output using a genuinely clipped rectangle, not the former false equality
rule. Keep scissor/quad-lane limitations named as software policy where hardware
behavior has not been established; this repair does not imply VID-04 or hardware
qualification.

Parent reproduced the original four dynamic failures with the ignored standalone
raw-word program and actual authored ROM at
`build/gate/07-03-parent-raw-contract.log`. For the coordinate/palette cases the
old reproduction uses cycle3 only to isolate those bugs behind the separate
incorrect cycle predicate. Permanent regressions must use valid cycle2.

## Hosted follow-up: LoadBlock producer

A literal LRS=7 vector exposed the LoadTile-style endpoint shift in LoadBlock.
The primary SDK producer packs the texel-count-minus-one directly, resolving the
standard zero-origin path despite the low-level function page's generic10.2
label. SGI page9 explicitly defines Tile Line as words to skip per T and zero
for contiguous loading. The corrected vectors use literal SetTextureImage width1,
SetTile line0 and LoadBlock words, check DXT swapping and a nonzero line skip,
and preserve earlier bytes on an end-of-TMEM refusal. See the documented
nonzero-origin source inconsistency in `docs/spec/rdp.md`.
