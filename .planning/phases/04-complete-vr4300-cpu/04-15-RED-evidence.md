# Phase 4 Plan 15 RED Evidence

## Task 1: CPU-profile authored output through the canonical machine

**Target:** `runner.cpu-diagnostic` in `runner/tests/test_cpu_diagnostic.c`

**Command:** `cmake --build --preset gate --target test_cpu_diagnostic && ctest --preset gate --no-tests=error -R '^runner\.cpu-diagnostic$' --output-on-failure --output-junit <private JUnit report>`

**Classifier:** `RED_EVIDENCE_OK` (`target_test_failed`, one target test executed)

The authored guest reaches image entry `0x80000400` through the cleared IPL3
payload on `ntsc-cart-4m-openipl3-vr4300-v1`. Its first ISViewer store is opcode
`0xAD090000` at canonical PC/EPC `0xFFFFFFFF8000040C`. The store does not
retire: the guest records Cause `0x0000001C` (ExcCode 7, DBE), attempt outcome
`guest exception`, address `0xFFFFFFFFB3FF0000`, and general vector
`0xFFFFFFFF80000180`. The guest then misses the authored park loop at
`0x80000420` / opcode `0x08000108`; retained capture remains empty (`0` bytes,
exact total `0`).

This is a behavioral failure after actual guest entry. The v3 profile does not
route the bounded ISViewer helper as an active device. The fix reuses the same
policy-v3 helper for descriptor v3 / state v9 while retaining the exact warm-v1
profile predicate.

One earlier test-harness attempt used an allocator alignment below
`posix_memalign`'s minimum and failed ROM admission. The allocator was corrected
and the guest was rerun. That setup failure is excluded from RED evidence.

The unchanged JUnit report and full console output remain in ignored local
build storage. This portable record contains only the command, target, expected
behavior, and observed guest values; it contains no raw report or checkout path.

## Task 2: descriptor-v3 exact flat-image limit

**Target:** `runner.cpu-diagnostic` in `runner/tests/test_cpu_diagnostic.c`

**Command:** `cmake --build --preset gate --target test_cpu_diagnostic && ctest --preset gate --no-tests=error -R '^runner\.cpu-diagnostic$' --output-failure --output-junit <private JUnit report>`

**Classifier:** `RED_EVIDENCE_OK` (`target_test_failed`, one target test executed)

The actual canonical CPU-profile machine evaluated the authored image at the
planned boundary: image length `0x00401000`, payload/load length `0x00400000`,
and aligned canonical KSEG0 entry `0x80000000`. It returned
`T64_STATUS_INVALID_ROM` (status 5), where admission should succeed with the
exact cleared IPL3 payload. The unchanged JUnit report was classified before
loader-policy changes. Raw reports remain in ignored build storage; this entry
contains no raw report or private path.

## Task 1 contract correction: member-bound patched-ROM identities

**Target:** `runner.cpu-diagnostic` in `runner/tests/test_cpu_diagnostic.c`

**Command:** `ctest --preset gate --no-tests=error -R '^runner\.cpu-diagnostic$' --output-on-failure --output-junit <private JUnit report>`

**Classifier:** `RED_EVIDENCE_OK` (`target_test_failed`, one target test executed)

The authored v2 inventory carries selector and selected-ROM SHA-256 on each
required value. Its first registration has two distinct selector/ROM bindings.
The production executable ran and returned
`classification=harness-error status=invalid-inventory-or-profile` when asked
to validate that canonical inventory. This isolates the contract gap: the
shared implementation still accepts only the earlier inventory shape with
one global selected-ROM binding. The unchanged report was classified before
the schema implementation changed. The report and classifier record stay in
ignored build storage; this portable evidence contains no raw records or
private paths.
