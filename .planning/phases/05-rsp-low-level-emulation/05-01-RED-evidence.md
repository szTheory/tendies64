# Phase 05 Plan 01 RED Evidence

This record preserves two distinct behavior boundaries. The profile-admission
failure established only that the new profile had not yet been admitted. The
corrected instruction failure established a fetched VADD refusal after profile
admission and scheduler execution were working.

## Profile admission RED

- Command: `ctest --test-dir build/gate --no-tests=error --output-on-failure --output-junit build/05-01-red-admission.xml -R '^unit.core.sp-dma$'`
- Exit: 8.
- Result: the eleven existing SP assertions passed. The added RSP profile
  admission assertion failed because the new profile was not registered.
- Classification: `gsd_run check tdd-red-evidence
  build/gate/build/tdd-red-admission.json` returned `RED_EVIDENCE_OK`.
- Scope: this is admission RED only; it says nothing about instruction
  behavior.

## Corrected fetched VADD RED

- Command: `cmake --preset gate && cmake --build --preset gate && ctest
  --test-dir build/gate --no-tests=error --output-on-failure --output-junit
  build/05-01-red-vadd-corrected.xml -R
  '^unit.core.(sp-dma|profile|digest-trace|determinism|scheduler)$'`
- Exit: 8. The four non-SP tests passed; `unit.core.sp-dma` failed only at the
  new VADD retirement assertion. Its preceding twelve SP/admission assertions
  passed.
- Observed operation: the production scheduler fetched word `0x4a0108d0` at
  RSP PC `0x014` after five prefix instructions. The attempt outcome was
  `REFUSED` (2), retired count stayed 5, PC stayed `0x014`, and the RSP state
  snapshot was unchanged.
- Build identity from the CTest output: `t64-build-v1-bec092f344bf21ae2fb10bf021e2d7973857e338006e41999078be81feed62cb`.
- Source revision from the CTest output: `48048042dfb011fe0337c9b39e692ef5b3e4b15e`.
- Classification: `gsd_run check tdd-red-evidence
  build/gate/build/tdd-red-vadd-corrected.json` returned `RED_EVIDENCE_OK`.

The accepted test word encodes COP2 vector computation (`bit 25 = 1`), element
0, `vt=1`, `vs=1`, `vd=3`, and VADD function `0x10`. The earlier attempt using
`0x480108d0` is retained as an invalid test attempt, not RED evidence: its
`rs=0` selects COP2 move space rather than vector computation. The encoding was
corrected before the valid RED run. The primary reference is the Silicon
Graphics, *Nintendo Ultra64 RSP Programmer's Guide*, pp. 56-57, 67, 233-234:
<https://ultra64.ca/files/documentation/silicon-graphics/SGI_Nintendo_64_RSP_Programmers_Guide.pdf>.

No ROM or external diagnostic asset was used; the test authors the IMEM words
in a synthetic fixture.

## Portable validation notes

- Task 1 normal command: `cmake --preset gate && cmake --build --preset gate && ctest --test-dir build/gate --no-tests=error --output-on-failure -R '^unit.core.(sp-dma|profile|digest-trace|determinism|scheduler)$'`.
  Result: all five focused tests passed after the all-target build.
- Task 2 normal command: `cmake --preset gate && cmake --build --preset gate && ctest --test-dir build/gate --no-tests=error --output-on-failure -R '^unit.core.(rsp-tracer|sp-dma|profile)$'`.
  Result: all three focused tests passed after the all-target build.
- Task 2 sanitizer command: `cmake --preset asan && cmake --build --preset asan && env ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build/asan --no-tests=error --output-on-failure -R '^unit.core.(rsp-tracer|sp-dma|profile)$'`.
  Result: all three focused tests passed after the all-target ASan/UBSan build.
- Inventory checks: `sh scripts/check-test-inventory.sh --preset gate` measured
  112 tests; `--preset asan` measured 114. The self-test passed all 112 gate
  removals, 114 sanitizer removals, and five malformed-inventory controls.
- Core source/object lint: `sh tools/lint/core-lint.sh` reported 84 files and
  zero violations.
- Normal build identity:
  `t64-build-v1-d8ea08a68e12daf986dcaa8c5c94b4dc8959b9415edfd4a6786a79a15e7dfb28`.
  ASan/UBSan build identity:
  `t64-build-v1-5ef2b221877096960c2af57afa4c1ce308418ca3d4020806c5d1770b82571973`.
  At configure time the revision macro was `9fa62ec68be8d4008611b932011dd8deaf27849f`;
  the build identity fingerprints the production source and policy inputs used
  by these builds.
- The sanitizer build emitted a visible 3200-byte frame-size warning for
  `t64_rsp_issue_event`; other existing core frame warnings were also visible.
  The configured ASan preset treats these warnings as nonfatal. No warning was
  suppressed or waived for the RSP file.

The commands and summarized results above are the portable record. Build logs,
JUnit output, and workstation-specific paths remain uncommitted build outputs.
