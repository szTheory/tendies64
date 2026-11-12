# Phase 4 Plan 09 RED Evidence

The ignored private record and complete JUnit report are in
`build/private-cpu-diagnostic/task1-red-record.json` and
`build/private-cpu-diagnostic/task1-red.xml`. They retain the exact command,
CTest output, report bytes, absolute report path, run timestamps, exit status,
target and semantic assessment. No private workstation path is reproduced in
this document.

## Task 1: stale execution before targeted invalidation

- **Command:** `ctest --preset gate --no-tests=error -R '^unit\.core\.cpu-path$' --output-on-failure --output-junit <ignored private report path>`
- **Target:** `unit.core.cpu-path`; `core/tests/test_cpu_path.c`, case 10.
- **Run:** 2026-11-12 09:55:36 UTC; report modified 2026-11-12 09:55:36 UTC.
- **Exit:** 8. Classifier: `RED_EVIDENCE_OK` (`target_test_failed`), JUnit, one registered test executed, no report errors.
- **Expected:** After the guest stores `0x24080002` through the uncached KSEG1 alias and branches back, the valid I-cache line still executes the prior instruction and leaves GPR8 at 1 until `Hit_Invalidate I`.
- **Observed:** Backing bytes were `24080002`, but the second execution left GPR8 at 2 before invalidation. `last_pc_full` was `0xffffffff80001000`; attempt outcome was retired (`1`). Cases 1–9 and 11–20 passed.
- **Assessment:** The guest store, branch, and second instruction execution all completed. The mismatch is the planned stale-code behavior, not a setup or fixture failure.

## Invalid setup attempt

The first build of the tracer did not compile because the test referenced a
nonexistent `cpu.instruction_word` field. This was a harness-authoring error,
not behavioral RED evidence. The assertion was corrected to observe GPR8,
`last_pc_full`, backing bytes, and retirement outcome. The corrected target
then built, ran the guest case, and produced the classifier-accepted RED above.

The first post-implementation focused run then stopped at `CACHE` before
invalidation: the test encoded base register `a1` but initialized `t5`. The
fixture now initializes `a1`; the corrected focused gate passed both
`unit.core.cpu-path` and `unit.core.cpu-mmu`. That run is a distinct tracer
setup correction, not RED evidence.

## Task 2

Task 2's RED evidence will be appended after its focused behavior matrix exists.
