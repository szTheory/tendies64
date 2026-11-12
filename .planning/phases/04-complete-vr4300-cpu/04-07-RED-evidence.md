# Plan 04-07 portable RED evidence

## Task 1: page-edge LWL/LWR/SWL/SWR

Before the staged memory path was added, the registered CPU-path target ran
all 16 internal cases and reached the page-edge LWL assertion. The expected
result was `0xffffffffdead7788` after reading physical bytes `0x2ffe–0x2fff`;
the observed result was `0x0000000011227788`. The report also records that the
fixture, mapped instruction fetch, initial TLB mapping, and target instruction
all executed. CTest exited 8 for the intended behavioral failure. The
unchanged raw JUnit and private record are under
`build/private-cpu-diagnostic/04-07-task1-red.*`; the evidence classifier
returned `RED_EVIDENCE_OK` for `unit.core.cpu-path`.

The original run started at `2026-11-12T07:21:15Z`; the failing report was
written at `2026-11-12T07:21:16Z`. A contemporaneous execution update reported
`RED_EVIDENCE_OK` before the Task 1 implementation commit. During this
continuation, the unchanged private record and report were classified again
successfully; the report path itself does not encode classifier invocation
time.

## Task 2: corrected reverse-endian lanes and merge tables

The first Task 2 vector was based on the incorrect expectation that user RE
reverses bytes within a fixed-width transfer. It used LHU expected `0x2211`
and observed `0x1122`. The primary-manual review rejected that expectation;
this attempt is preserved in ignored `build/private-cpu-diagnostic/04-07-task2-red.*`
and `04-07-task2-provisional-wrong-vector.*` files, and it is excluded from
valid RED evidence.

The corrected test stores physical bytes `11 22 33 44 55 66 77 88` at
`PA 0x2000` and maps `VA 0x1000` there. With fixed `BigEndianMem=1` and
user-mode `Status.RE=1`, the corrected first assertion expected LBU to select
`PA+7` (`0x88`); the provisional implementation selected `PA+0` (`0x11`). The
fixture and registered target executed; CTest exited 8 on this target test.
The unchanged report was captured at `2026-11-12T08:13:57Z`; the classifier
returned `RED_EVIDENCE_OK` for `unit.core.cpu-memory`. Its raw JUnit and private
record are `build/private-cpu-diagnostic/04-07-task2-corrected-red.*`.

The independent expectations use the NEC *VR4300/VR4305/VR4310 64-Bit
Microprocessor User's Manual*, U10504EJ7V0UM00, SHA-256
`b7c1900f1707a4b238ebd82af1669e9628e4df4875ceabfa012a9d95b4716b75`, public
copy [VR43XX.pdf](https://hack64.net/docs/VR43XX.pdf). For the asymmetric byte
fixture, LHU selects `PA+6..7` (`0x7788`), LW selects `PA+4..7`
(`0x55667788`), and LD selects `PA+0..7`
(`0x1122334455667788`). Stores from source
`0xA1B2C3D4E5F60718` place SB at `+7`, SH at `+6..7`, SW at `+4..7`, and SD
at `+0..7`, preserving the big-endian byte order within each selected
physical range.

The manual explicitly documents the byte/half/double lane rules on pp. 439–441,
450–451, 486, 491, and 501, with the container diagram and memory mapping in
Figure 3-2 (pp. 62–63) and Tables 16-2/16-3 (pp. 367–368). LW/SW pseudocode
omits the lane transform; the selected width-4 mapping is a coherent model
inference corroborated by LL p. 453, LWC p. 460, and SWC p. 516, not a claim
about independently resolved silicon behavior.

All offsets for LWL/LWR/SWL/SWR use independently listed physical-lane and
register-byte expectations in `core/tests/test_cpu_memory.c`. LDL/LDR/SDL/SDR
also cover offsets 0–7 against separate expected values derived from their
manual partial-operation equations and tables (pp. 445–446, 497, 499–500).
The corrected fixture keeps both 32-bit halves asymmetric and verifies byte,
halfword, word, and doubleword scalar accesses, stores, and unchanged BE
instruction fetch/kernel behavior.

Raw JUnit, console output, and classifier records remain ignored under
`build/private-cpu-diagnostic/`; this file contains only portable test facts
and the selected model interpretation.
