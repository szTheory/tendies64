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

## Source-review fix RED/GREEN

- Pre-fix command: `build/gate/core/test_sp_dma --rsp-tracer`.
  Exit: 1. The two newly added controls failed against the buggy source: queued
  PI DMA had not completed at tick 1 because RSP BREAK globally halted the
  machine, and overlapping seeded CPU link reservations remained valid after
  SP DMEM SW. Output is retained as the ignored build artifact
  `build/gate/build/05-01-review-red.txt`.
- The PI control schedules a real PI DMA event and verifies its copied
  synthetic bytes, completion, interrupt, and continued guest-tick advancement.
  The reservation controls seed native fixture link state for widths 4 and 8;
  they do not execute guest LL/LLD or claim LL/SC instruction-path coverage.
- Fix: RSP BREAK now sets RSP HALT/BROKE and the optional interrupt without
  halting the machine. Accepted SP DMEM SW invalidates overlapping CPU link
  reservations through the existing physical-range helper. Pre-effect staging
  and refusal behavior are unchanged. The existing fetched VADD tracer now
  expects the reached guest-tick bound while retaining its SP HALT/BROKE and
  interrupt assertions; the PI test claims device completion, not CPU opcode
  execution.
- Normal validation: `cmake --preset gate && cmake --build --preset gate`,
  followed by
  `ctest --test-dir build/gate --no-tests=error --output-on-failure --output-junit build/gate/build/05-01-review-green-normal.xml -R '^unit.core.(rsp-tracer|sp-dma|profile|cpu-link|scheduler|digest-trace)$'`.
  The all-target build completed and all six tests passed. Console logs are
  `build/gate/build/05-01-review-green-normal-build.log` and
  `build/gate/build/05-01-review-green-normal-ctest.log`.
- Sanitizer validation: `cmake --preset asan && cmake --build --preset asan`,
  followed by
  `env ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build/asan --no-tests=error --output-on-failure --output-junit build/asan/build/05-01-review-green-asan.xml -R '^unit.core.(rsp-tracer|sp-dma|profile|cpu-link|scheduler|digest-trace)$'`.
  The all-target build completed and all six tests passed. Console logs are
  `build/asan/build/05-01-review-green-asan-build.log` and
  `build/asan/build/05-01-review-green-asan-ctest.log`.
- These builds include the selected new-profile SP duration policy of 12 model
  cycles; the five existing profile identities and policies remain unchanged.
  Normal build identity:
  `t64-build-v1-019d7175db451a975d80844d8c855f047d65b69b891f0e252875a879f34da625`.
  ASan/UBSan build identity:
  `t64-build-v1-505473c0e24df8c98ee70bca09c628f73b152246ef4e229e0dbdc3df1fb68ef3`.
  Both configured revision macros name the prior HEAD
  `f0ce649d9be11e8808b50d155e03dee659e6d58f`; build identities fingerprint the
  current source and profile policy inputs.
- `sh tools/lint/core-lint.sh` passed with 84 files and zero violations.
  Inventory checks remain 112 tests for gate and 114 for ASan. The sanitizer
  build reports a visible 3168-byte frame warning for `t64_rsp_issue_event`
  (limit 2048), alongside existing core frame warnings; the preset treats it
  as nonfatal and it was not suppressed.
