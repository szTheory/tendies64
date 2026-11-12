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

### RED runs

The first run is retained as a discarded setup attempt. Its test used a
zero-extended KSEG0 PC. A follow-up diagnostic showed Cause `0x00000010`
(AdEL), `last_opcode=0`, and a redirect to the general exception vector, so
the guest CVT.D.W opcode was never fetched. The JUnit format classifier
accepted that process failure, but semantic inspection disqualified it; it
did not authorize implementation.

The fixture now uses the canonical sign-extended address
`0xffffffff80001000`. The fresh pre-implementation run used
`cmake --preset gate && cmake --build --preset gate && ctest --preset gate --no-tests=error -R '^(unit\.core\.cpu-path)$' --output-on-failure --output-junit <ignored private JUnit report>`.
The CMake configure and all-target gate build passed; CTest ran exactly one
test and exited 8 after the existing CPU-path cases completed.

- Target test: `unit.core.cpu-path`; target source:
  `core/tests/test_cpu_path.c`; classifier: `RED_EVIDENCE_OK`
  (`target_test_failed`).
- First intended behavior failure was TAP case 14. The test fetched opcode
  `0x468008a1` (matching `last_opcode`) and then failed the CVT.D.W result
  assertion: expected FPR2 `c1e0000000000000`; observed sentinel
  `1122334455667788`, status 14, Cause 0, model-unsupported outcome, and
  retired count 0. The PC remained canonical at the intended KSEG0 address.
  This is a valid feature RED, not a setup or fetch failure.
- Full raw JUnit and classifier records remain under ignored
  `build/private-cpu-diagnostic`; no checkout or temporary absolute path is
  included here.

### FR=0 support-boundary RED

Review confirmed that Status.FR can be cleared by guest CP0 state before
Plan12. The initial tracer used independent FPR slots in that mode even though
paired-register mapping is outside this slice. Before the guard, a canonical
FR=0 run fetched opcode `0x468008a1`, wrote `0xc1e0000000000000` over the
destination sentinel, and retired. The new test requires a model-unsupported
stop with no destination write and no retirement until Plan12 provides paired
mapping. Its exact `unit.core.cpu-path` JUnit record was classified
`RED_EVIDENCE_OK`; no source guard was present during that run.

## Task 2: selected arithmetic closure

An initial test-only attempt used a temporary f32-add placeholder without
checking the already implemented conversion. It was discarded and is not RED
evidence. The corrected pre-feature test exercises both operations through a
real production raw dispatch: the existing signed conversion must continue to
work, then binary32 addition must produce the exact sum of `1` and `2`.

The RED command was `cmake --preset gate && cmake --build --preset gate &&
ctest --preset gate --no-tests=error -R
'^(unit\.core\.softfloat32)$' --output-on-failure --output-junit <ignored
private JUnit report>`. Configure and all-target gate build passed; CTest ran
exactly one test and exited 8. The report was fresh at `2026-11-12T13:08:52Z`.
The target `unit.core.softfloat32` in `core/tests/test_softfloat.c` was
classified `RED_EVIDENCE_OK` (`target_test_failed`).

The existing `-2^31` to binary64 operation passed with status 0, bits
`0xc1e0000000000000`, and no flags. The new `1.0 + 2.0` case failed its
behavior assertions: dispatch returned `T64_STATUS_NOT_READY` (7), and the
result and flag sentinels remained `0x55667788` and `0xff`. The exact sum is
`3 = 1.5 × 2^1`, binary32 bits `0x40400000`, with no flags. The operation
reached the production dispatch after the existing conversion passed, so the
failure is missing arithmetic behavior rather than test setup. The complete
JUnit report, unchanged console output, command, and classifier record remain
under ignored `build/private-cpu-diagnostic`.
