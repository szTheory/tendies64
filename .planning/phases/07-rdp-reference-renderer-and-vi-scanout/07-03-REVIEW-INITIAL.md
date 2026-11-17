---
phase: 07-rdp-reference-renderer-and-vi-scanout
reviewed: 2026-11-16T21:36:33Z
depth: standard
files_reviewed: 20
files_reviewed_list:
  - core/CMakeLists.txt
  - core/t64_digest.c
  - core/t64_internal.h
  - core/t64_isviewer.c
  - core/t64_machine.c
  - core/t64_rdp.c
  - core/t64_rdp_tmem.c
  - core/tests/test_rdp.c
  - core/tests/test_rdp_copy.c
  - core/tests/test_vi.c
  - docs/spec/rdp-vi-cases.json
  - docs/spec/rdp.md
  - docs/spec/vi.md
  - fixtures/manifest.json
  - runner/t64_real.c
  - runner/t64_real_fixture.c
  - runner/tests/test_rdp_video.c
  - scripts/check-rsp-no-hle.py
  - scripts/check-test-inventory.sh
  - scripts/tests/test_rsp_no_hle.py
findings:
  critical: 0
  warning: 5
  info: 0
  total: 5
  necessity: 0
status: issues_found
---

# Phase 07: Code Review Report

**Reviewed:** 2026-11-16T21:36:33Z  
**Depth:** standard  
**Files Reviewed:** 20  
**Status:** issues_found

## Summary

Reviewed the exact 20-file Plan07-03 scope against the submitted source. The TMEM load bounds and per-machine digest coverage are present, and the profile v2 descriptor, ISViewer identity, and runner pins agree. Five raw RDP command decoding/geometry defects remain in the texture copy path. The 147-case normal gate and affected-owner ASAN results are retained evidence from the phase summary; the full 149-case ASAN inventory was not run. A focused independent raw-word reproduction against the retained normal library is preserved at `build/gate/07-03-review-copy-contract/repro.log`.

## Warnings

### WR-01: Copy rectangles require fill cycle and reject copy cycle

**Classification:** WARNING  
**File:** `core/t64_rdp_tmem.c:536-541`  
**Issue:** `t64_rdp_tmem_validate_copy` accepts cycle type `3` and refuses every other value. The RDP encodes `G_CYC_COPY` as `2` and `G_CYC_FILL` as `3` ([gDPSetCycleType](https://ultra64.ca/files/documentation/online-manuals/man/n64man/gdp/gDPSetCycleType.html)). The authored copy helper also sets `3` at `core/tests/test_rdp_copy.c:194-196`, so its successful copy cases exercise fill mode. With a raw SetOtherModes word carrying copy cycle `2`, the retained implementation reports refusal 5 and completes no copy; the output is in the focused reproduction log.  
**Fix:** Require the documented copy cycle value `2`, and construct copy tests with raw cycle `2` so the public path is exercised.

### WR-02: Texture rectangle S/T fields use the wrong layout

**Classification:** WARNING  
**File:** `core/t64_rdp_tmem.c:601-605`  
**Issue:** The second TextureRectangle word carries 16-bit S in bits 63:48 and 16-bit T in bits 47:32, both signed S10.5 values ([RDP command layout](https://hcs64.com/files/RDP_COMMANDS.pdf), [Nintendo macro reference](https://ultra64.ca/files/documentation/online-manuals/functions_reference_manual_2.0i/gsp/gSPTextureRectangle.html)). This code extracts 12-bit values at shifts 44 and 32. A standard raw S value of 32 is interpreted as 512, so a rectangle starting at the second texel samples zero instead of that texel. The helper at `core/tests/test_rdp_copy.c:208-209` shifts S by 44 and masks the bug. The raw-word reproduction shows this result after using cycle 3 only to pass the separate cycle predicate.  
**Fix:** Decode both full 16-bit fields from their specified positions, sign-extend them, and update test packet construction to use those positions.

### WR-03: CI copy reads TLUT mode from unrelated low bits

**Classification:** WARNING  
**File:** `core/t64_rdp.c:437-440`  
**Issue:** SetOtherModes is stored as a raw 64-bit command, but `tlut_format` is taken from bits 15:14. The TLUT enable/type fields are bits 47:46 ([RDP command layout](https://hcs64.com/files/RDP_COMMANDS.pdf)). Consequently a standard enabled RGBA16 or IA16 TLUT setting does not produce the expected internal mode; `t64_rdp_tmem_validate_copy` then rejects CI at `core/t64_rdp_tmem.c:550-553`. The current tests inject mode values into bits 15:14 at `core/tests/test_rdp_copy.c:435-440` and `470-475`. A focused raw-word run with a loaded CI4 palette and TLUT bits 47:46 set to `11` reaches refusal 6; cycle 3 was used only to get past WR-01's separate predicate.  
**Fix:** Decode the enable and type fields from bits 47:46, preserve the disabled state, and build RGBA16/IA16 CI cases from standard raw SetOtherModes words.

### WR-04: Negative DTDY is treated as a large positive step

**Classification:** WARNING  
**File:** `core/t64_rdp_tmem.c:604-605, 743-748`  
**Issue:** DTDY is a signed S5.10 field, but the low 16 bits are stored and multiplied as `uint32_t`. A valid `DTDY=-1024` therefore advances T by a large positive amount. The retained raw-word reproduction accepts the rectangle but writes zero for its second row instead of sampling the prior source row (`expected=07c1,f801`, observed row1 `0000`). The field's signed format is documented by the [TextureRectangle reference](https://ultra64.ca/files/documentation/online-manuals/functions_reference_manual_2.0i/gsp/gSPTextureRectangle.html).  
**Fix:** Sign-extend DTDY before coordinate arithmetic and keep fixed-point stepping signed through the point-sample conversion; otherwise reject negative steps explicitly if they are outside the selected software policy.

### WR-05: Copy rectangle lower-right coordinates are treated as exclusive

**Classification:** WARNING  
**File:** `core/t64_rdp.c:208-211`  
**Issue:** The bounds helper subtracts one from XL/YL for every texture rectangle and treats equal upper/lower values as empty. In copy mode, the documented rectangle includes the lower-right coordinate and ignores the two fractional screen-coordinate bits; for example, `0` through `19<<2` covers 20 columns ([Nintendo TextureRectangle reference](https://ultra64.ca/files/documentation/online-manuals/functions_reference_manual_2.0i/gsp/gSPTextureRectangle.html)). A valid one-pixel copy rectangle with XH=XL=0 and YH=YL=0 is therefore turned into a zero-area rectangle. The `single-texel` test avoids this by encoding XL/YL as 4 through the helper at `core/tests/test_rdp_copy.c:205-209`; `zero-area` at `:771-780` asserts the opposite semantics for equal endpoints.  
**Fix:** Apply copy-mode inclusive endpoint rules when deriving pixel bounds, and change the raw rectangle test vectors so their encoded endpoints follow copy mode.

---

_Reviewed: 2026-11-16T21:36:33Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: standard_
