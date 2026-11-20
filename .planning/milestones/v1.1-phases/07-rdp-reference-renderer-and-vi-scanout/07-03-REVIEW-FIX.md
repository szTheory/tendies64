---
phase: 07-03
fixed_at: 2026-11-17T03:41:40Z
review_path: .planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-03-REVIEW.md
iteration: 1
findings_in_scope: 5
fixed: 5
skipped: 0
status: all_fixed
---

# Phase 07-03: Code Review Fix Report

**Source review:** `07-03-REVIEW.md`  
**Source commit:** `4eeb32b`  
**Source binding:** `build/gate/07-03-raw-fix-source-binding.json`

## Fixed issues

- **WR-01 — Copy cycle:** Copy rectangles now require cycle type 2. Literal raw command regressions fail against the old decoder and pass against the fix.
- **WR-02 — S/T layout:** The fetched second rectangle word now decodes full signed 16-bit S and T fields from bits 63:48 and 47:32.
- **WR-03 — TLUT controls:** TLUT enable/type now comes from bits 47/46, including RGBA16, IA16, disabled mode, and low-bit independence.
- **WR-04 — Signed stepping:** S/T and DSDX/DTDY remain signed through bounded fixed-point stepping and point sampling. Unsupported DSDX values are refused before writes.
- **WR-05 — Copy endpoints:** Copy-mode lower endpoints are inclusive after flooring 10.2 screen coordinates; equal endpoints render one pixel. Scissor-empty and left/top-clipped-origin cases are covered.

## Verification

The initial independent raw-word regression failed on the old source:

```text
ctest --test-dir build/gate -R '^unit\.core\.rdp\.copy$' --output-on-failure
```

Evidence: `build/gate/07-03-raw-fix-red-ctest.log` and `build/gate/07-03-raw-fix-expanded-red-ctest.log`.

The corrected raw-copy suite passes in normal and ASAN/UBSan builds. The final fractional-endpoint case uses a distinct source pattern at `TEXTURE_ADDRESS + 16`, puts a decoy at the base address, and checks both setup bytes and the fetched output pixel.

```text
cmake --build build/gate --target test_rdp_copy
ctest --test-dir build/gate -R '^unit\.core\.rdp\.copy$' --output-on-failure
cmake --build build/asan --target test_rdp_copy
ctest --test-dir build/asan -R '^unit\.core\.rdp\.copy$' --output-on-failure
```

Both copy inventories pass after that final test-data correction (`build/gate/normal-copy-inventory.log`, `build/asan/asan-copy-inventory.log`). The broader normal preset CTest cohort passed 147/147 after removing only the attributable generated Python bytecode. Its first run is preserved and records two scanner failures caused by that bytecode; see `07-03-raw-fix-full-normal.log` and `07-03-raw-fix-full-normal-retry.log` under `build/gate/`.

The fill, DP append/resume, profile, 11 affected ASAN/UBSan consumers, strict 77-source/object/body check, and 32 no-HLE mutation controls passed. Logs and SHA-256 bindings are listed in the source-binding JSON. The full ASAN suite was not rerun; its previously recorded caveat remains unchanged.

## Scope and limits

The production arithmetic and state fields stay the same width; digest serialization explicitly casts the signed values back to their 32-bit representation. No profile version change was needed. Plan02 IN-01 (`dpc_dma_active`) was already resolved by removal in the initial Plan03 state/digest change. Quad-lane/scissor hardware accuracy, rendered YUV, and VID04 remain unqualified.

Verification ran in the main checkout on the source bytes now recorded in commit `4eeb32b`; the binding records those bytes rather than claiming the commit existed before every check. This report is intentionally uncommitted for the parent workflow to include in its closeout.

The orchestrator verified every source, final-copy binary and log hash in the
source binding. The preset workflow did not include the `scripts/gate.sh`
core-lint tail; that required stage passed separately under the parent
(`build/gate/07-03-raw-fix-parent-core-lint.log`).
Earlier wrapper self-tests and fixture inputs are unchanged.

Corrected consumers and logs were copied and hash-verified under
`build/gate/07-03-raw-fix-retained-artifacts/{gate,asan}/binding.json`
before the next source build. Parent direct copy owner:2123checks/0failures.
