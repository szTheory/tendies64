---
phase: 03-first-headless-rom-through-open-ipl3
plan: "11"
subsystem: boot-runner
tags: [ipl3, headless, t64-result-v2, verified-descriptors, supervisor]

requires:
  - phase: 03-10
    provides: Strict real-result v2 validation and descriptor preparation.
provides:
  - Descriptor-backed actual IPL3-to-hello producer and supervisor path.
  - Core-content digest attestation and source-replacement proof through actual admission.
  - Process, report, capture, and untrusted-progress error precedence for real runs.
affects: [phase-03-validation, headless-runner, result-protocol]

actuals:
  tokens: 43466
  tasks: 2
  commits: 4
commits: 4
plan_head_before: 5769faf102d097c0701537c404bef6d4c0efc340
plan_head_after: 0413fe840e21ddf247ca03d386bce3d4b80cbc5c

tech-stack:
  added: []
  patterns:
    - Test-only sibling binaries isolate bounded process/report fault injection from production CLI.
    - Checked child reports can retain trusted progress under an explicit supervisor error override.

key-files:
  created:
    - runner/t64_real.c
    - runner/t64_real_result.c
    - runner/t64_real_result.h
  modified:
    - runner/t64_supervisor.c
    - runner/tests/test_real_runner.c
    - runner/t64_result_check.c
    - runner/tests/test_result_v2.c
    - runner/CMakeLists.txt
    - scripts/check-test-inventory.sh
    - docs/spec/result-protocol.md

key-decisions:
  - "Only the authored completion PC/opcode paired with the pinned whole-fixture identity qualifies hello completion."
  - "The supervisor retains input and raw-capture identities when independently known; null input/log/state identities are limited to untrusted postlaunch harness errors."
  - "The full source IPL3 SHA is pinned provenance; runtime verification hashes the embedded payload and normalized whole authored ROM because the original source asset is not a runtime input."

patterns-established:
  - "Real child inputs are immutable, unlinked, read-only verified descriptors; caller paths are never forwarded."
  - "Completion is a non-retiring software stop convention and carries no hardware BREAK exception claim."

requirements-completed: [BOOT-04, RUN-03]

coverage:
  - id: D1
    description: "Pinned authored hello executes through real IPL3/DMA and emits the exact checked v2 transcript."
    requirement: BOOT-04
    verification:
      - kind: integration
        ref: runner/tests/test_real_runner.c#integration.open-ipl3-hello
        status: pass
    human_judgment: false
  - id: D2
    description: "Empty bounded output is boot-fail, and supervisor errors cannot preserve a guest pass."
    requirement: RUN-03
    verification:
      - kind: integration
        ref: runner/tests/test_real_runner.c#bounded-and-supervisor-fault-cases
        status: pass
      - kind: unit
        ref: runner/tests/test_result_v2.c#partial-null-and-error-precedence
        status: pass
    human_judgment: false
  - id: D3
    description: "Replacing both caller paths after snapshot verification leaves actual core admission and output bound to the original bytes."
    verification:
      - kind: integration
        ref: runner/tests/test_real_runner.c#controls.real.open-ipl3-source-replaced
        status: pass
    human_judgment: false

duration: 87min
completed: 2026-11-11
status: complete
---

# Phase 3 Plan 11: Run and Supervise Actual IPL3-to-Hello Summary

**A verified descriptor-backed run now reaches authored hello completion through the actual core, and the supervisor checks report, captured bytes, and process status before issuing a result.**

## Performance

- **Duration:** 87 minutes from the first committed RED artifact through the implementation commit.
- **Started:** 2026-11-11T14:40:59Z (first RED commit).
- **Completed:** 2026-11-11T16:07:52Z (implementation commit).
- **Tasks:** 2 completed.
- **Files changed:** 25, measured from the plan ledger base through the implementation commit.

## Accomplishments

- Added `t64-runner-real` for verified inherited fixture/input snapshots, canonical input parsing, actual core admission, bounded real CPU/PI/SP trace and exact guest-output capture.
- Added supervisor `--real` mode. It independently checks the child report against the raw capture and actual process result, writes canonical v2 atomically, and gives host/report errors precedence over guest text.
- Added a synchronized two-path replacement control after both verified snapshots are finalized and before child launch. The child receives only those snapshots; an independent core test and the producer both verify normalized core content digest equals the whole-ROM hash.
- Added bounded test-only cases for nonzero exit, missing/truncated/oversize/malformed/unknown report, closed capture descriptor, crash, wall kill, and missing input. The nonzero-exit case retains checked guest progress and state while overriding the guest pass.
- Preserved the existing descriptor-helper/original-path reopen control and updated its stale `--real` refusal assertion to expect the canonical prelaunch usage record for an incomplete invocation.
- Kept only `BOOT-04` and `RUN-03` complete. `BOOT-01`, `RUN-01`, and `RUN-05` remain pending because this plan establishes only the pinned prepared-warm smoke path, empty input schedule, and existing output-register contract.

## Task Commits

1. **Task 1 RED:** `483448d` — actual IPL3 completion test failed on the observed `CPU_UNSUPPORTED` stop while retaining the real PC/opcode/tick/output evidence.
2. **Task 1 GREEN:** `98557df` — descriptor-attested real producer and strict real v2 output.
3. **Task 2 RED:** `f223a5b` — actual supervisor integration failed because real mode was still unavailable.
4. **Task 2 GREEN:** `0413fe8` — real supervisor, replacement, result validation and bounded fault controls.

## Key Files

- `runner/t64_real.c` and `runner/t64_real_result.c` — actual guest producer and canonical v2 writer.
- `runner/t64_supervisor.c` — immutable input preparation, child supervision, strict report/capture/process validation, and atomic result output.
- `runner/tests/test_real_runner.c` — direct/supervised positive paths, actual replacement control, and bounded supervisor faults.
- `runner/t64_result_check.c` and `runner/tests/test_result_v2.c` — constrained postlaunch null identities and independent accept/reject cases.
- `runner/tests/test_fixture_descriptor.c` — retained the earlier helper proof while matching the new canonical incomplete-real invocation refusal.
- `docs/spec/result-protocol.md` — actual public command, provenance distinction, replacement timing, and v2 error rules.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Rounded sub-pointer allocator alignment for the host allocator.**
- **Found during:** Task 1.
- **Issue:** `posix_memalign` refused the core's valid one-byte allocation alignment, causing setup refusal before guest execution.
- **Fix:** The producer rounds only the host allocation alignment to pointer alignment and reports the actual host allocation alignment in the returned storage descriptor.
- **Files modified:** `runner/t64_real.c`.

**2. [Rule 1 - Bug] Kept empty output at the guest bound a boot failure.**
- **Found during:** Task 1.
- **Issue:** The initial result classified an empty bounded run as timeout.
- **Fix:** Emit `boot-fail` / `empty-output`, with no pass classification.
- **Files modified:** `runner/t64_real.c`, `runner/tests/test_real_runner.c`.

**3. [Rule 1 - Blocking test update] Reconciled the 03-09 helper assertion with the now-implemented real mode.**
- **Found during:** Task 2 focused verification.
- **Issue:** The helper expected `--real` to be unavailable and to write no result, which stopped being true once real mode was connected.
- **Fix:** It now checks the canonical prelaunch usage refusal for its intentionally incomplete invocation; descriptor replacement and original-path negative checks remain intact.
- **Files modified:** `runner/tests/test_fixture_descriptor.c`.

**4. [Rule 2 - Required error representation] Allowed only genuinely unavailable v2 identities on postlaunch untrusted errors.**
- **Found during:** Task 2 contract reconciliation.
- **Issue:** The prior checker required all identities after launch even when no child report could be trusted.
- **Fix:** Keep supervisor-known identities and independently hashed input/capture facts; require null state, zero verified progress and no milestones when child progress is untrusted. A checked report followed by nonzero exit retains its identities and progress under a host-error override. Independent checker tests cover acceptance and rejection boundaries.
- **Files modified:** `runner/t64_result_check.c`, `runner/tests/test_result_v2.c`, `docs/spec/result-protocol.md`.

## Limits

- Evidence is for the single pinned authored fixture, prepared warm-entry profile, empty logical-input schedule, and fixed guest bound. It does not establish cold IPL2/PIF behavior, general game compatibility, or hardware accuracy.
- The original 4,096-byte source IPL3 asset is not a runtime input. The profile/manifest carries its qualified provenance SHA; runtime rehashes the embedded 4,032-byte payload and normalized 4,280-byte authored ROM. No absent-source byte or prefix-equality claim is made.
- The replacement barrier occurs after snapshot finalization and input validation, before child launch. The actual producer then admits only the verified inherited descriptors; the test does not claim the child was already waiting before replacement.
- Logical input scheduling is exercised as a schema/parser and admission path, not evidence of SI/PIF input consumption.

## Automated Checks

- `runner/tests/test_real_runner.c` — direct and supervised guest behavior, replacement race, process/report/capture faults, and exact transcript checks.
- `runner/tests/test_result_v2.c` — partial-null untrusted-error acceptance, mixed-null pass rejection, and harness-error precedence.
- `runner/tests/test_fixture_descriptor.c` — prior descriptor snapshot/reopen proof remains green.
- `cmake --preset gate` — passed.
- `cmake --build --preset gate` — passed.
- `ctest --preset gate --no-tests=error --output-on-failure` — 84/84 passed.
- Focused CTest set (integration, descriptor, result-v2, replacement, positive, empty-bound) — 7/7 passed.
- ASan/UBSan focused set with `ASAN_OPTIONS=detect_leaks=0` — 8/8 passed: core tracer/boot-entry/CPU path, direct descriptor, both replacement controls, actual integration/fault suite, and result-v2.
- `sh scripts/check-test-inventory.sh --preset gate` — passed, 84 tests.
- `sh scripts/check-test-inventory.sh --preset asan` — passed, 86 tests.
- `git diff --check` — passed before the implementation commit.

## TDD Gate Compliance

- **Task 1 RED:** `03-11-task1-red.json` and `03-11-task1-red.tap` record the genuine core-backed failure at the actual authored completion boundary. The output includes the observed stop detail and TAP failure; the test did not use a missing binary or hardcoded-false producer.
- **Task 1 GREEN:** `98557df` makes that exact producer path pass and validates actual trace/output evidence.
- **Task 2 RED:** `03-11-task2-red.json` and `03-11-task2-red.tap` record the actual supervisor integration refusing because `--real` was unavailable.
- **Task 2 GREEN:** `0413fe8` connects the same supervised path and the positive/fault suite passes against the actual producer.

## Self-Check: PASSED

- Summary and both task RED evidence pairs exist.
- Task 1 RED/GREEN and Task 2 RED/GREEN commits are ancestors of HEAD.
- `query verify-summary` found the required files and commits.
