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
