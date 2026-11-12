# Phase 4 Plan 10 RED Evidence

The complete JUnit reports and machine-readable records are retained under
ignored `build/private-cpu-diagnostic/`. They include the executed commands,
full unchanged CTest output, report paths, real exit codes, target files,
independent expectations, observed values, run timestamps, and semantic
assessments. No raw workstation path is reproduced here.

## Task 1: cached guest store remains dirty until hit writeback

- **Command:** `ctest --preset gate --no-tests=error -R '^unit\.core\.cpu-path$' --output-on-failure --output-junit <ignored report>`
- **Target:** `unit.core.cpu-path`; `core/tests/test_cpu_path.c`, case 11.
- **Run:** 2026-11-12 11:01:17–11:01:18 UTC; report modified during the run.
- **Exit:** 8. Classifier: `RED_EVIDENCE_OK` (`target_test_failed`); one registered
  CTest target ran the complete 22-case TAP matrix.
- **Expected:** After the guest SW through KSEG0, RDRAM stays zero and the
  selected 16-byte line is valid, dirty, tagged `0x00002000`, and contains
  `0x12345678`. A subsequent `Hit_Write_Back D` publishes the bytes and clears
  dirty while leaving the line valid.
- **Observed:** The SW retired, but RDRAM changed immediately to `0x12345678`
  and the D-cache line remained invalid and clean. The planned first assertion
  failed before the CACHE instruction ran.
- **Semantic assessment:** The guest instruction fetch, register setup, KSEG0
  address translation, and SW all completed. The observed direct backing
  mutation is the intended pre-implementation behavior mismatch, not setup
  failure.

## Discarded setup attempts

- The initial test draft did not compile because it referenced helper names
  that do not exist in `test_cpu_path.c`. The test now sets full PC state and
  reads the expected word directly from the line bytes. This was a compile
  failure, not behavioral evidence.
- A subsequent run used a zero-extended KSEG address in the base register and
  entered the address-error handler before retiring the SW. The register now
  contains the canonical sign-extended address. That run was a fixture fault,
  not behavioral evidence.
