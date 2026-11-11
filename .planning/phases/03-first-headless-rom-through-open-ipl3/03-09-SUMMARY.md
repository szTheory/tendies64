---
phase: 03-first-headless-rom-through-open-ipl3
plan: "09"
subsystem: runner
tags: [C, POSIX, sha256, input-schedule, descriptor-handoff, CTest]
requires:
  - phase: 03-first-headless-rom-through-open-ipl3
    provides: prepared warm profile and canonical t64.input v1 parser
provides:
  - verified immutable fixture and schedule snapshot preparation API
  - synchronized replacement proof using inherited descriptors and a path-reopen negative control
affects: [03-10, 03-11, result-protocol]
actuals:
  tokens: 18353
  tasks: 2
  commits: 4
commits: 4
plan_head_before: 26cc7d6b45b170ef826b244d3e34ebd9263ccabb
plan_head_after: 1bc5f7d62451fcbc92e8b0d8636d6ab24a85083b
tech-stack:
  added: []
  patterns:
    - O_NOFOLLOW and O_NONBLOCK source opens followed by regular-file fstat
    - exclusive private snapshots reopened read-only, reverified, unlinked and rewound
    - explicit pipe barriers for deterministic replacement and process cleanup controls
key-files:
  created:
    - runner/t64_verified_input.c
    - runner/t64_verified_input.h
    - runner/tests/test_fixture_descriptor.c
    - .planning/phases/03-first-headless-rom-through-open-ipl3/03-09-descriptor-red.json
  modified:
    - runner/t64_supervisor.c
    - runner/CMakeLists.txt
    - scripts/check-test-inventory.sh
    - docs/spec/result-protocol.md
    - .planning/phases/03-first-headless-rom-through-open-ipl3/03-09-PLAN.md
key-decisions:
  - "Keep --real refused until the real producer is implemented in 03-11; this helper is preparation evidence, not actual CPU-runner proof."
  - "Keep raw schedule file length/SHA distinct from the canonical parsed schedule SHA."
  - "Limit legacy O_NONBLOCK hardening to avoiding FIFO open stalls; the synthetic original-path race remains open."
patterns-established:
  - "Only explicitly inherited fixture/input descriptors have close-on-exec cleared in the child."
  - "Same-length replacements are checked against a descriptor consumer and a labeled path-reopen control."
requirements-completed: []
duration: 58min
completed: 2026-11-11
status: complete
---

# Phase 3 Plan 09: Verified Descriptor Input Preparation Summary

**Verified read-only fixture and schedule snapshots now survive synchronized same-length source replacement; `--real` remains refused until 03-11.**

## Created Paths

- `runner/t64_verified_input.c` and `runner/t64_verified_input.h` implement owned immutable snapshots.
- `runner/tests/test_fixture_descriptor.c` contains preparation faults, inherited-FD admission, replacement and cleanup controls.
- `.planning/phases/03-first-headless-rom-through-open-ipl3/03-09-descriptor-red.json` records the valid Task 1 RED.
- `.planning/phases/03-first-headless-rom-through-open-ipl3/03-09-SUMMARY.md` records this execution.

## Performance

- **Duration:** 57 min
- **Started:** 2026-11-11T12:40:33Z
- **Completed:** 2026-11-11
- **Tasks:** 2
- **Plan-owned files:** 8, including the authorized Task 2 marker correction in the plan.

## Accomplishments

- `t64_verified_input_prepare/release` opens no-follow, nonblocking regular sources, bounds and hashes the copy, syncs and chmods it read-only, closes the writer, reopens and verifies the same inode/length/SHA, unlinks, rewinds, and retains only the owned read-only descriptor. The fixture ceiling matches the selected warm profile’s 1 MiB ROM maximum, the schedule cap is 65536 bytes, and the API cannot exceed the legacy 64 MiB harness maximum.
- The synchronized control starts both consumers behind pipe barriers, replaces private fixture and schedule copies with same-length changed bytes and logical input, then releases them. The inherited-FD helper hashes, parses, and core-admits the originals without source paths in argv. A separately labeled path-reopen control observes both replacements. Raw schedule-file SHA remains separate from canonical parsed schedule SHA.
- `--real` is recognized only in the supervisor option region and exits before launch until 03-11. Tests cover a runner-owned `--real` after `--` and a fixture option value equal to `--real`. Legacy synthetic `check_fixture` now opens with `O_NONBLOCK` and keeps its post-open regular-file check; the original-path TOCTOU limitation remains documented.
- Preparation tests cover missing, symlink, FIFO, oversize, digest mismatch, interrupted/short copy, temp-open/finalize/reopen/readback/hash/unlink faults, and invalid, closed, writable, or still-named descriptors. Child failure and process-group cleanup use bounded pipe waits and verify descendant pipe EOF and parent FD baseline.

## Files Created/Modified

- Created the snapshot API, descriptor test, and portable Task 1 RED evidence listed under Created Paths.
- Modified `runner/t64_supervisor.c`, `runner/CMakeLists.txt`, `scripts/check-test-inventory.sh`, and `docs/spec/result-protocol.md` for the staged refusal, test wiring, legacy FIFO hardening, inventory, and ownership boundary.
- Updated `03-09-PLAN.md` to remove Task 2's invalid TDD marker after parent authorization.

## Automated Checks

- Valid Task 1 RED: the descriptor test compiled and ran its first TAP assertion, which observed `T64_VERIFIED_INPUT_ERROR (status=2)` for successful preparation. `gsd_run check tdd-red-evidence` accepted the portable evidence as `RED_EVIDENCE_OK`.
- `cmake --build --preset gate` and `ctest --preset gate --no-tests=error --output-on-failure` — passed, 81/81.
- Focused gate `runner.fixture-descriptor` and `controls.real.source-replaced` — passed, 2/2.
- ASan/UBSan focused descriptor and replacement tests with `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` — passed, 2/2.
- `sh scripts/check-test-inventory.sh --preset gate` — passed, 81 tests; `--preset asan` — passed, 83 tests.
- `sh scripts/pii-scan.sh --tree` — 0 findings. `git diff --check` — passed.

## Task Commits

1. Task 1 RED evidence — `89cd7b9` (`test(03-09): add failing verified descriptor case`).
2. Task 1 implementation — `5ccf132` (`feat(03-09): prepare verified immutable input descriptors`).
3. Task 2 scaffold — `68dd2a5` (`test(03-09): add synchronized source replacement assertion`). This commit contained a hardcoded-false placeholder and was not a valid semantic RED; it remains in history and is not credited as TDD evidence.
4. Task 2 verification and cleanup controls — `1bc5f7d` (`feat(03-09): verify descriptor replacement and cleanup`).

The Task 2 `tdd="true"` marker was removed with parent authorization because this task verifies implemented behavior. The placeholder RED was corrected without rewriting history; Task 2 has GREEN verification only. Task 1 retains genuine RED/GREEN evidence.

## Decisions Made

- Keep this API as a preparation seam. The production `--real` path stays refused until 03-11 connects it to actual CPU-runner admission.
- Keep raw descriptor bytes and canonical parsed schedule identity as separate digests when formatting differs.
- Preserve synthetic v1 behavior and describe its remaining path race accurately.

## Deviations from Plan

**1. Verification-only Task 2 no longer carries `tdd="true"`.** Parent authorized this reconciliation after the attempted hardcoded-false scaffold failed semantic TDD review. The scaffold commit remains in history but is not counted as RED evidence; Task 2 received verification-only GREEN.

**2. Legacy fixture open uses `O_NONBLOCK`.** This bounded hardening was explicitly authorized because a path replaced by a FIFO between `lstat` and `open` could block. The post-open regular-file check remains, and the synthetic original-path race is still documented.

## Issues Encountered

The first descendant-cleanup check treated an immediate process-group probe as the sole success signal. It observed leader exit 9 but `group_gone_at_reap=0`; the descendant’s exclusive pipe closed and the test FD count returned to baseline. The TAP diagnostic recorded leader PID/PGID `96481`, descendant PID `96482`, `descendant_pipe_closed=1`, and `fds_clean=1`. This was a dead/reparented child still visible during the immediate probe, not a live worker holding resources. After the test returned, `kill -0 -- -96481` reported “no such process” (ESRCH). The assertion now records both PIDs and the PGID, waits for descendant pipe EOF with a bounded poll, and checks the FD baseline; the targeted control and full gate pass.

## Requirements and Limits

No broad user outcome is completed by this preparation slice. RUN-01 and BOOT-04 remain pending in REQUIREMENTS.md because the real supervisor-to-CPU path and actual guest output are not implemented here; BOOT-02 was already complete. The descriptor helper control is foundation evidence only. Plan 03-11 must repeat replacement through actual CPU-runner admission before SC6 closes. Synthetic v1 bytes and goldens remain unchanged, and the legacy synthetic path still has its original-path race.

## Next Phase Readiness

Plan 03-11 can consume the owned snapshot API for actual CPU-runner admission. It must repeat replacement through the real consumer and establish actual guest evidence before `--real` is enabled.

## User Setup Required

None.

## Self-Check: PASSED

- Created paths exist, including the portable Task 1 RED evidence and this summary.
- All four task commits are ancestors of HEAD.
- Final tree PII scan found zero findings; `git diff --check` passed.
