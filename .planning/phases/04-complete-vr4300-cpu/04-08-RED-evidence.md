# Plan 04-08 portable RED evidence

## Task 1: LL/SC with scheduled DMA interference

Before the linked-memory implementation, the registered `unit.core.cpu-path`
target ran all 19 internal cases. Its first linked-memory assertion followed
16 passing cases and expected LL to return status 0 and load `0x12345678` into
GPR8. It observed status 14 (model unsupported) and GPR8 `0x0000000000000000`.
The scheduled PI and SP guest paths also reached their LL instructions and
observed status 14. Fixture admission, guest fetch, and registration worked.
CTest exited 8; its selected test took 0.27 seconds.

The focused command was `ctest --preset gate --no-tests=error -R
'^unit\.core\.cpu-path$' --output-on-failure`; fixture setup first ran
`cmake --preset gate && cmake --build --preset gate`. JUnit capture went to
the ignored private diagnostics directory.

The recorded run began at `2026-11-12T08:59:32Z`; the private JUnit report was
written at `2026-11-12T08:59:33Z`. The classifier completed at
`2026-11-12T08:59:52Z` and returned `RED_EVIDENCE_OK` for
`unit.core.cpu-path`. The unchanged raw report, console output, and classifier
record remain ignored under `build/private-cpu-diagnostic/04-08-task1-*`.

## Task 2: targeted D-cache invalidation

Before the targeted D-line link effect was added, the registered
`unit.core.cpu-link` target established a reservation for physical bytes
`[0x2004, 0x2008)`, then executed CACHE Hit Invalidate D on line `0x2000`.
The expected result was an invalid reservation followed by SC result 0 and
unchanged bytes. The first behavioral assertion instead observed
`linked_valid=1`; SC returned 1 and wrote `0xdeadbeef`. Setup and the cache
instruction succeeded. CTest exited 8; the selected test took 0.38 seconds.

The focused command was `ctest --preset gate --no-tests=error -R
'^unit\.core\.cpu-link$' --output-on-failure`, with JUnit capture written to
the ignored private diagnostics directory.

The recorded run began at `2026-11-12T09:16:55Z`; the private JUnit report was
written at `2026-11-12T09:16:55.936Z`. The classifier completed at
`2026-11-12T09:17:10Z` and returned `RED_EVIDENCE_OK` for
`unit.core.cpu-link`. Its unchanged raw report, console output, and classifier
record remain ignored under `build/private-cpu-diagnostic/04-08-task2-*`.

## Additional review regressions

A later source review added assertions for cacheable TLB aliases to SP backing
memory and for refused conditional stores. A first SP-DMA test draft used the
SP-to-RDRAM register while expecting a read-to-SP transfer; its all-zero
destination observation was invalid and was discarded. The corrected test
started SP read-to-DMEM and read-to-IMEM transfers from RDRAM, then ran before
the fixes. In that registered CTest run, the first failed assertion observed
`linked_valid=1` after a committed overlapping DMEM bus write where 0 was
expected. The remaining failures showed the same result for IMEM bus writes
and scheduled DMA into both banks. An SC to cacheable read-only cartridge
memory was refused, but observed `linked_valid=0` where the uncompleted
reservation should remain valid. CTest exited 8; the corrected target run
took 0.313 seconds and wrote its ignored JUnit report at
`2026-11-12T09:38:32Z`.

The command was `ctest --test-dir build/gate --output-on-failure -R
'^unit\.core\.cpu-link$'`. The previous wrong-direction attempt remains
separate from these valid pre-fix observations.

The pre-fix observations are retained in the ignored
`build/private-cpu-diagnostic/04-08-contract-gap-red-valid.xml`. After the
fixes, the registered `unit.core.cpu-link`, `unit.core.cpu-path`, and
`unit.core.digest-trace` targets passed, followed by the full local gate.

The initial Task 1 and Task 2 REDs were independently classified before their
implementation changes. These later review regressions supplement that task
evidence; they are not represented as the original TDD RED captures.
