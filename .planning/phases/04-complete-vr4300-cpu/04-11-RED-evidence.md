# Phase 4 Plan 11 RED evidence

## Task 1: CVT.D.W machine path

### Pre-use source and license audit

- Verified official repository `ucb-bar/berkeley-softfloat-3` at commit
  `f74b1e48110ac3a27dd49b787d164e55e42d81d1`; fetched commit identity matched
  the complete requested SHA.
- Inspected upstream `COPYING.txt` and `source/i32_to_f64.c` at that commit.
  `COPYING.txt` says its terms apply to the whole release and each source file;
  the source notice names John R. Hauser and The Regents of the University of
  California, with the BSD three conditions and warranty disclaimer.
- Exact source SHA-256:
  `de334caa22edfcfc2a35572a2e858f0c500ad5018fc4335b46dfb003e0076183`.
  The active `LICENSES/BSD-3-Clause.txt` was copied verbatim from that commit's
  `COPYING.txt` and byte-compared before source incorporation.
- The planned adaptation is limited to `i32_to_f64`: an authored bounded
  leading-zero helper and integer bit packing replace the source helper/macro;
  the result uses an explicit context and raw bits. The provenance record names
  `szTheory` as maintenance owner and this independent CPU-path assertion.
- Primary source links: [pinned source](https://github.com/ucb-bar/berkeley-softfloat-3/blob/f74b1e48110ac3a27dd49b787d164e55e42d81d1/source/i32_to_f64.c),
  [pinned license](https://github.com/ucb-bar/berkeley-softfloat-3/blob/f74b1e48110ac3a27dd49b787d164e55e42d81d1/COPYING.txt).

### Independent expected result

For `-2147483648`, `-2^31 = -1 × 2^31`. Binary64 therefore has sign bit 1,
biased exponent `1023 + 31 = 1054 = 0x41e`, and zero fraction, yielding
`0xc1e0000000000000` exactly. The conversion is exact, so it raises no IEEE
flags.

### RED run

- Command: `cmake --preset gate && cmake --build --preset gate && ctest --preset gate --no-tests=error -R '^(unit\.core\.cpu-path)$' --output-on-failure --output-junit <ignored private JUnit report>`.
- The CMake configure and all-target gate build passed. CTest ran exactly one
  test and exited 8 after its existing CPU-path cases completed.
- Target test: `unit.core.cpu-path`; target source:
  `core/tests/test_cpu_path.c`; classifier: `RED_EVIDENCE_OK` (`target_test_failed`).
- First planned behavior failure was TAP case 14, the CVT.D.W assertion:
  expected FPR2 `c1e0000000000000`; observed sentinel
  `1122334455667788`, status 0, guest-exception outcome, retired count 0.
  The test reached the guest opcode and result assertion after fixture setup;
  this was not a build, discovery, or fixture failure.
- Full raw JUnit and classifier record remain under ignored
  `build/private-cpu-diagnostic`; no checkout or temporary absolute path is
  included here.
