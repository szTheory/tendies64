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

## Task 2: verification-only coverage

Task 2 adds tests and documentation for the I-cache behavior implemented in
Task 1. The first focused behavioral run was green, so the task's inapplicable
`tdd="true"` marker was removed under the verification-only exception in
`.planning/AUTONOMY.md`. No failure was manufactured.

- **Initial command:** `ctest --preset gate --no-tests=error -R '^unit\.core\.cpu-icache$' --output-on-failure --output-junit <ignored private report path>`
- **Initial run:** 2026-11-12 10:22:52 UTC; exit 0; one CTest target passed with
  the original 10-case TAP matrix. The unchanged JUnit and console output are
  in ignored `build/private-cpu-diagnostic/task2-baseline.xml` and
  `task2-baseline.console.txt`.
- **Final command:** `cmake --preset gate && cmake --build --preset gate && ctest --preset gate --no-tests=error -R '^(unit\.core\.cpu-icache|unit\.core\.cpu-path|unit\.core\.cp0-cache)$' --output-on-failure --output-junit <ignored private report path>`
- **Final targets:** `unit.core.cpu-icache`, `unit.core.cpu-path`, and
  `unit.core.cp0-cache`; cache assertions are in `core/tests/test_cpu_cache.c`.
- **Final run:** 2026-11-12 10:43:23–10:43:24 UTC; report modified
  2026-11-12 10:43:24 UTC; exit 0. All 3 CTest targets passed. The cache
  target's TAP plan contains 11 cases and all 11 passed. The complete report,
  console output, exact command, absolute report path, expectation, observed
  result, and timestamps are in ignored
  `build/private-cpu-diagnostic/task2-focused-record.json` and
  `task2-focused.xml`.
- **Expected:** Index/tag boundaries, aliases, cache attributes, every accepted
  backing domain, invalid zeroed prepared lines, refused cartridge fill,
  invalidation/refusal effects, DMA no-snoop behavior, and v9 digest
  sensitivity/isolation match the documented model; existing CPU path and
  CACHE controls remain green.
- **Observed:** All 3 targets passed. The cartridge range case returned
  `T64_STATUS_BUS_RANGE` while preserving its existing line and prior opcode
  output. Both fresh machines had 512 invalid zeroed lines. The K0=2 bypass and
  same-index mismatched-tag invalidation assertions passed.

## Withdrawn and corrected attempts

- An initial Task 2 build failed because seven test callbacks retained an
  unused asset parameter. The callback signatures were corrected; this was a
  compile setup failure, not behavioral evidence.
- A temporary test assertion treated the manual's hardware PTag bits
  PA[31:12] as the required C struct encoding. The implementation instead
  stores the 32-byte-aligned physical line key; paired with the virtual index,
  it is an equivalent lookup identity under the validated translations and
  does not create a guest-visible or TagLo mismatch. That assertion was
  withdrawn, its raw report is retained as
  `build/private-cpu-diagnostic/task2-withdrawn-tag-key-assertion.xml`, and it
  was not classified as RED.
- The first cartridge-boundary test draft compared a failed fetch's output
  with a sentinel even though a successful fetch had already written the
  opcode. The correct expectation is that a refused refill leaves the prior
  opcode output unchanged; after fixing that assertion, the 11-case target
  passed.

## ASan and inventory

- **Full instrumented build:** `cmake --preset asan && cmake --build --preset asan`,
  with `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and
  `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`; all targets built with
  exit 0 at 2026-11-12 10:42:55 UTC.
- **Warnings:** The full build reported existing nonfatal 2048-byte stack-frame
  diagnostics in exception, MMU, PI, CP0, memory, digest, SP, machine, and CPU
  functions, plus duplicate-library linker warnings in runner targets. No
  warning named `t64_cache.c` or `test_cpu_cache.c`.
- **Instrumented selection:** the same three CTest targets above passed 3/3
  at 2026-11-12 10:43:05–10:43:06 UTC. The full JUnit, console, command,
  absolute report path, and timestamps are retained in ignored
  `build/private-cpu-diagnostic/task2-asan-focused-record.json`; the full
  build output and warning review are in `task2-asan-build-record.json`.
- **Inventory:** `sh scripts/check-test-inventory.sh --selftest` passed 84
  opcode rows, 206 removal controls, and 5 malformed-name controls. Gate
  inventory is 102 names and ASan inventory is 104 names.

## Full gate, test-fixture repair, and gate tail

- **Initial full gate:** `sh scripts/gate.sh` ran once from 2026-11-12
  10:46:03 to 10:46:37 UTC and exited 1 in CTest. Inventory controls passed;
  101 of 102 CTests passed. The sole failure was `unit.core.cpu-memory`.
- **Cause and fix:** That memory test installs a new synthetic instruction
  directly into RDRAM before each independent `run_one` assertion but did not
  invalidate the instruction line after the I-cache became functional. The
  harness therefore correctly re-executed its stale cached instruction. The
  fixture now invalidates only its synthetic code line after backing rewrites,
  including the two hand-built delay-slot programs. No guest memory behavior
  or D-link behavior was changed.
- **Repair selection:** `cmake --build --preset gate --target test_cpu_memory test_cpu_cache test_cpu_path && ctest --preset gate --no-tests=error -R '^(unit\.core\.cpu-memory|unit\.core\.cpu-icache|unit\.core\.cpu-path|unit\.core\.cp0-cache)$' --output-on-failure` exited 0; all 4 targets passed.
- **Previously unreached gate tail:** `sh tools/lint/core-lint.sh` passed with
  24 files and 0 violations. The original broad gate was not repeated after
  this scoped fixture fix.
- **Final ASan repair check:** After the full ASan configure/build
  (`cmake --preset asan && cmake --build --preset asan`) completed with exit 0
  at 2026-11-12 10:48:47 UTC, the same
  four affected targets passed under the prescribed ASan/UBSan settings at
  10:48:57–10:48:58 UTC. Complete build output, JUnit, console, command,
  absolute report path, and timestamps are in ignored
  `build/private-cpu-diagnostic/task2-asan-final-record.json`; the original
  full-gate log and measured result are in `task2-final-gate-record.json`.
