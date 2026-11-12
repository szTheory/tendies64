# Phase 4 Plan 12 RED evidence

Raw, unchanged JUnit reports and classifier records remain under ignored
`build/private-cpu-diagnostic/`. The portable record below omits workstation
paths and includes the actual assertion, opcode, full canonical PC, and run
times.

## Task 1: COP1 transfer, conversion, cached memory, and CU1 handler

- **Command:** `cmake --preset gate && cmake --build --preset gate && ctest --preset gate --no-tests=error -R '^(unit\.core\.cpu-path)$' --output-on-failure --output-junit ../private-cpu-diagnostic/04-12-task1-red-junit.xml`
- **Target:** `unit.core.cpu-path`, source `core/tests/test_cpu_path.c`; one CTest target ran the complete 26-case TAP matrix.
- **Run:** `2026-11-12T15:13:16Z`; the unchanged JUnit report was modified at `2026-11-12T15:13:19.591Z`.
- **Exit:** 8. The classifier returned `RED_EVIDENCE_OK` (`target_test_failed`).
- **Expected:** MTC1 `0x44880800` retires, then CVT.D.W, SDC1, LDC1, and DMFC1 carry `-2^31` as exact binary64 `0xc1e0000000000000` through the MMU and cached memory. A separate CU1-disabled MTC1 enters and executes a guest handler without changing its FPR destination.
- **Observed:** The prior 14 TAP cases passed. Case 15 fetched `last_opcode=0x44880800` at canonical `PC=0xffffffff80001000`, then returned status 14 (`T64_STATUS_RESOURCE_EXHAUSTED`), Cause `0x00000000`, attempt outcome 3 (model unsupported), and retired count 0.
- **Semantic assessment:** This is a valid behavior RED. Configure and full gate build succeeded; the target ran and failed at the intended MTC1 retirement assertion. The expected opcode and canonical KSEG0 PC confirm guest fetch reached the target instruction; this was not a setup, address, or fetch failure.
- **Independent expectation:** `-2^31 = -1 × 2^31`; binary64 sign is 1, biased exponent is `1023 + 31 = 1054 = 0x41e`, and the fraction is zero, yielding `0xc1e0000000000000` exactly.
- **Primary reference:** NEC *VR4300/VR4305/VR4310 User's Manual*, 7th edition, August 2000, pp. 208–210 (FR/FPR layout), pp. 557–558 (32-bit-mode value/store rules), and pp. 236–244 (floating-point operation semantics), [public manual PDF](https://hack64.net/docs/VR43XX.pdf), SHA-256 `b7c1900f1707a4b238ebd82af1669e9628e4df4875ceabfa012a9d95b4716b75`.
