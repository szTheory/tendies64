# Phase 4 Plan 13 RED Evidence

## Task 1: rounded COP1 operation and precise exception recovery

- **Target:** `unit.core.cpu-path`; test case “ADD.S rounding modes trap precisely, recover, and preserve E-only Cause” in `core/tests/test_cpu_path.c`.
- **Command:** `ctest --preset gate --no-tests=error -R '^(unit\.core\.cpu-path)$' --output-on-failure` (CTest also wrote an unchanged JUnit report to ignored local diagnostics).
- **Expected:** The fetched `ADD.S f3,f1,f2` at canonical PC `0xffffffff80001000` raises FPE Cause 15, replaces the prior FCR31 Cause with Inexact Cause, leaves enabled Inexact Flag clear, and preserves the destination. The guest handler then clears FCR31 and resumes the add; `CVT.S.S` raises E-only Cause without writing its destination.
- **Observed:** The exact opcode `0x460208c0` was fetched at `0xffffffff80001000`, but CP0 Cause was `0x00000028` (RI), attempt outcome was 2, FCR31 remained `0x00010090`, and f3 remained `0x1122334455667788`. The expected FPE Cause and Inexact Cause were absent.
- **Assessment:** Valid behavioral RED. The full gate build succeeded, CTest ran exactly the selected target, and the failure followed the intended guest instruction rather than a setup, fixture, or fetch error.
- **Classifier:** `RED_EVIDENCE_OK` (`target_test_failed`). The unchanged JUnit report and full classifier record remain local and ignored.

An earlier draft ran several arithmetic instructions before checking the target and reached the exception handler first. That run was invalid RED evidence. The fixture was corrected to stop immediately after the intended `ADD.S`; the result above is the verified RED record.

## Task 2: arithmetic and exception edge matrix

- **Exception target:** `unit.core.cop1-exceptions`; test case “invalid W-format conversion raises E and suppresses its destination” in `core/tests/test_cop1.c`.
- **Command:** `ctest --preset gate --no-tests=error -R '^(unit\.core\.cop1-exceptions)$' --output-on-failure`.
- **Expected:** The fetched invalid `CVT.W.W` opcode `0x468008e4` at canonical PC `0xffffffff80001000` raises FPE Cause 15, sets only E in the FCR31 Cause field, preserves prior sticky flags, and leaves f3 unchanged.
- **Observed:** The instruction retired with CP0 Cause `0x00000000`; FCR31 became `0x0000107c`, clearing the prior Cause without setting E; f3 changed from `0x1122334455667788` to `0x1122334400000000`.
- **Assessment:** Valid behavioral RED. The full gate build succeeded, the named CTest executed the guest opcode at the expected canonical PC, and the failure came from the planned exception and destination assertions.
- **Classifier:** `RED_EVIDENCE_OK` (`target_test_failed`). The unchanged JUnit report and full classifier record remain local and ignored.

- **Arithmetic target:** `unit.core.cop1-arithmetic`; binary32 and binary64 `ADD` exact halfway vectors in `core/tests/test_cop1.c`.
- **Command:** `ctest --preset gate --no-tests=error -R '^(unit\.core\.cop1-arithmetic)$' --output-on-failure`.
- **Expected:** For positive exact ties `1 + 2^-24` and `1 + 2^-53`, FCR31 RM=RP yields the next representable value, while RM=RM rounds to 1.0; nearest-even and toward-zero results also match their hand-derived bit patterns.
- **Observed:** Both precisions reverse the directed modes: RM=RP returns 1.0 and RM=RM returns the next representable value. Nearest-even and toward-zero pass; FCR31 retains the selected architectural RM value.
- **Assessment:** Valid behavioral RED. The target CTest executed both formats and failed the planned exact-result assertions; there was no setup or fixture failure.
- **Classifier:** `RED_EVIDENCE_OK` (`target_test_failed`). The unchanged JUnit report and full classifier record remain local and ignored.
