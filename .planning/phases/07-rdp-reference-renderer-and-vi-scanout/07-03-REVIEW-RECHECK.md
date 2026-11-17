---
phase: 07-rdp-reference-renderer-and-vi-scanout
reviewed: 2026-11-17T03:52:27Z
depth: standard
source: 4eeb32b17fcfed2ca18152c49a95e9215f1cd406
baseline: b2778f475f7f20b8c01c57cc5158549839b9ad7a
files_reviewed: 9
files_reviewed_list:
  - core/t64_digest.c
  - core/t64_internal.h
  - core/t64_rdp.c
  - core/t64_rdp_tmem.c
  - core/tests/test_rdp_copy.c
  - docs/spec/rdp-vi-cases.json
  - docs/spec/rdp.md
  - scripts/check-rsp-no-hle.py
  - scripts/tests/test_rsp_no_hle.py
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
  necessity: 0
status: clean
---

# Plan07-03 Review Recheck

**Reviewed:** 2026-11-17T03:52:27Z  
**Depth:** standard, bounded to the nine-file correction diff  
**Status:** clean; all five prior findings and the clipped-origin correction are verified in the current source.

## Recheck disposition

| Prior issue | Disposition and source evidence |
| --- | --- |
| WR-01: Copy cycle admission | Fixed. `t64_rdp_tmem_validate_copy` requires cycle value 2 (`core/t64_rdp_tmem.c:552-557`). The literal `0x2f20000000000000` cases use COPY mode. |
| WR-02: S/T word layout | Fixed. The fetched second word is decoded as signed 16-bit S/T at shifts 48/32 (`core/t64_rdp_tmem.c:605-627`). Literal cases use nonzero S/T and assert independently authored framebuffer bytes (`core/tests/test_rdp_copy.c:904-940,1021-1051`). |
| WR-03: TLUT controls | Fixed. SetOtherModes decodes enable/type from bits 47/46 and clears TLUT mode when disabled (`core/t64_rdp.c:426-433`). Literal RGBA16, IA16, and disabled-with-low-bit-decoys cases assert colors, refusal, and preserved output (`core/tests/test_rdp_copy.c:943-1018`). |
| WR-04: Negative DTDY | Fixed for the selected software policy. Fields are sign-extended, stepped in `int64_t`, and floored without relying on implementation-defined signed right shift (`core/t64_rdp_tmem.c:25-39,605-642,773-798`). The literal negative traversal case checks both independently authored output pixels (`core/tests/test_rdp_copy.c:1021-1051`). |
| WR-05: Copy endpoint semantics | Fixed. Both 10.2 endpoints are floored and equal endpoints remain a one-pixel copy (`core/t64_rdp.c:201-236`). Literal equal and fractional-endpoint cases assert the selected pixel (`core/tests/test_rdp_copy.c:904-940,1116-1182`). Scissor upper bounds remain exclusive. |
| Scissor clipping origin | Fixed. `begin_copy` advances S/T from the original rectangle origin to the clipped start, with axes swapped for Flip (`core/t64_rdp_tmem.c:628-642`). The 4x4 literal texture has distinct row/column values and checks the clipped 2x2 output (`core/tests/test_rdp_copy.c:1185-1221`). |

The corrected raw words are literal constants rather than words produced by the packet helper under review. The fractional-origin case places a decoy at the old texture address and checks the distinct source at `TEXTURE_ADDRESS + 16`; the disabled-TLUT case sets unrelated low bits and verifies refusal without changing the framebuffer. The DSDX restriction remains explicit (`DSDX == 4096`) and is checked before any rectangle writes; its raw case verifies no copy, unchanged framebuffer, and that the pending rectangle remains the current command (`core/t64_rdp_tmem.c:558`, `core/tests/test_rdp_copy.c:1085-1113`).

Signed state remains `int32_t`; digest serialization casts each value back to `uint32_t`, preserving the same 32-bit representation (`core/t64_internal.h:650-656`, `core/t64_digest.c:487-492`). The new sign/floor helpers are included in the reviewed helper inventory and body digests. The mutation suite includes a raw copy-cycle body mutation and enforces its declared seed count. The retained guard log reports 77 source/object edges and 32 mutation controls (`build/gate/07-03-raw-fix-no-hle-mutations-final.log`).

## Evidence and limits

Reviewed the exact nine-file diff and the raw contract/fix report. The current source binding names source `4eeb32b`; retained normal and ASAN consumer bindings are under `build/gate/07-03-raw-fix-retained-artifacts/{gate,asan}/binding.json`. Parent-run evidence records the 147-case normal preset, the 11 affected ASAN/UBSan owners, the strengthened copy owner at 2123 checks with no failures, and core lint at 90 files with no violations. No separate probe or broad suite was run during this recheck. The full 149-case ASAN inventory remains unrun; this review makes no hardware-accuracy, rendered-YUV, VID-completion, or phase-completion claim.

The original review remains the record of the broader 20-file Plan03 review. This recheck does not replace it or re-audit unrelated code.
