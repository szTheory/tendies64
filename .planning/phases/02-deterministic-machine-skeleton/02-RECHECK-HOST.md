---
phase: 02-deterministic-machine-skeleton
rechecked: 2026-11-10T22:57:28Z
scope: host and fuzz fixes in 4f7ddc6..c739c25
status: clean
findings: 0
---

# Phase 2 Host Review Recheck

**Result:** VERIFIED — the three host findings (WR-50–WR-52) are fixed in the reviewed diff. No additional host or fuzz regression was found.

## Checks

- **WR-50 — special files:** `runner/t64_model_result_check.c:72-85` opens input with `O_NONBLOCK`, then checks the opened descriptor with `fstat` and rejects non-regular files before reading. `runner/t64_skeleton_probe.c:422-440` retains `O_NOFOLLOW`, adds `O_NONBLOCK`, checks for a regular file before `ftruncate` or writing, and rejects special files. FIFO regressions cover checker input and probe output in `runner/tests/test_model_result.c:298-321` and `runner/tests/test_skeleton_probe.c:360-378`.
- **WR-51 — subprocess bounds:** Both subprocess helpers use `CLOCK_MONOTONIC` deadlines and nonblocking `waitpid` polling; timeout cleanup kills the process group, reaps the direct child, and polls for group disappearance during bounded cleanup (`runner/tests/test_model_result.c:129-199`; `runner/tests/test_skeleton_probe.c:43-115`). Both CTest registrations have a 60-second `TIMEOUT` (`runner/CMakeLists.txt:55-75`). The tests fork a descendant and assert a bounded 100 ms run returns the timeout status (`runner/tests/test_model_result.c:317-320,345-355`; `runner/tests/test_skeleton_probe.c:380-386,612-621`).
- **WR-52 — refusal snapshot:** The fuzzer saves machine and image pointers and capacities, requires exact metadata equality before and after a refusal, and compares fixed saved spans only after the metadata check (`core/tests/fuzz_rom_admission.c:294-319,331-343,361-400`). The zero and shortened machine-capacity controls exercise rejection before comparison.
- **Coverage and production boundary:** The diff changes no production admission implementation. `FUZZ_MUTATIONS` remains 250,000, and the campaign still requires exactly 250,000 completed mutations, 148 fixed cases, all header offsets, and all 12 refusal branches (`core/tests/fuzz_rom_admission.c:10,784-799`). The fix report records the full 250,000-mutation campaign at 69.10 seconds within its 120-second bound; I did not rerun the full campaign.

---

_Independent host recheck; no source changes made._
