# Phase 4 Plan 13 RED Evidence

## Task 1: rounded COP1 operation and precise exception recovery

- **Target:** `unit.core.cpu-path`; test case “ADD.S rounding modes trap precisely, recover, and preserve E-only Cause” in `core/tests/test_cpu_path.c`.
- **Command:** `ctest --preset gate --no-tests=error -R '^(unit\.core\.cpu-path)$' --output-on-failure` (CTest also wrote an unchanged JUnit report to ignored local diagnostics).
- **Expected:** The fetched `ADD.S f3,f1,f2` at canonical PC `0xffffffff80001000` raises FPE Cause 15, replaces the prior FCR31 Cause with Inexact Cause, leaves enabled Inexact Flag clear, and preserves the destination. The guest handler then clears FCR31 and resumes the add; `CVT.S.S` raises E-only Cause without writing its destination.
- **Observed:** The exact opcode `0x460208c0` was fetched at `0xffffffff80001000`, but CP0 Cause was `0x00000028` (RI), attempt outcome was 2, FCR31 remained `0x00010090`, and f3 remained `0x1122334455667788`. The expected FPE Cause and Inexact Cause were absent.
- **Assessment:** Valid behavioral RED. The full gate build succeeded, CTest ran exactly the selected target, and the failure followed the intended guest instruction rather than a setup, fixture, or fetch error.
- **Classifier:** `RED_EVIDENCE_OK` (`target_test_failed`). The unchanged JUnit report and full classifier record remain local and ignored.

An earlier draft ran several arithmetic instructions before checking the target and reached the exception handler first. That run was invalid RED evidence. The fixture was corrected to stop immediately after the intended `ADD.S`; the result above is the verified RED record.
