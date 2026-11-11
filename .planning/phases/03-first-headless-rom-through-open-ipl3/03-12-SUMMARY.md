---
phase: 03-first-headless-rom-through-open-ipl3
plan: "12"
subsystem: testing
tags: [ipl3, guest-boundaries, determinism, output-capture, inventory]

requires:
  - phase: 03-11
    provides: Actual IPL3-to-hello producer, supervisor, and strict result checker.
provides:
  - Six real guest failure controls with exact checked result boundaries.
  - A real warm-profile determinism and future-state field audit.
  - Exact 91-test gate and 93-test sanitizer inventories with per-name removal checks.
affects: [phase-03-validation, headless-runner, machine-contract, phase-03-13]

actuals:
  tokens: 44945
  tasks: 2
  commits: 3
commits: 3
plan_head_before: be4071552ee8568b59b83e9e59e16ad9bcbc2790
plan_head_after: d25f9b762a07b04242d4de166341dbecdf39fd3e

tech-stack:
  added: []
  patterns:
    - Real negative controls use the production descriptor, CPU, supervisor, and result-checking path.
    - Determinism comparisons use independent expected values and compare only canonical guest evidence.
    - Guest output distinguishes attempted bytes from the retained capture prefix.

key-files:
  created:
    - core/tests/test_boot_determinism.c
    - .planning/phases/03-first-headless-rom-through-open-ipl3/03-12-task1-red.json
  modified:
    - runner/tests/test_real_controls.c
    - core/tests/test_boot_entry.c
    - core/tests/test_isviewer.c
    - docs/spec/machine-contract.md
    - docs/spec/result-protocol.md
    - docs/spec/boot-limits.md
    - scripts/check-test-inventory.sh

key-decisions:
  - "Only the seven generated rights-cleared named variants enter the real guest path; unknown fixture identities remain refused and only the original positive fixture can pass."
  - "The v8 observer records exact attempted flush bytes with checked arithmetic separately from the retained 4096-byte capture; an inexact saturated value is never emitted as an exact result total."
  - "Local determinism establishes repeatability for this pinned warm profile and authored inputs on this host; hosted Linux/GCC and macOS/Clang equality remains Plan 13 evidence."

patterns-established:
  - "Use real-run result and PC/opcode evidence for guest boundaries; fixture identity alone does not prove guest entry or retirement."
  - "Continue bounded resource yields monotonically and compare equivalent completed stops, with trace drains kept within the observer ring capacity."

requirements-completed: [BOOT-04, RUN-03]

coverage:
  - id: D1
    description: "The six named real guest controls reject empty halt/bound, wrong transcript, unsupported opcode, capture overflow, and identity mismatches at their intended boundaries."
    verification:
      - kind: integration
        ref: runner/tests/test_real_controls.c#controls.real.empty-halt
        status: pass
      - kind: integration
        ref: runner/tests/test_real_controls.c#controls.real.empty-bound
        status: pass
      - kind: integration
        ref: runner/tests/test_real_controls.c#controls.real.wrong-transcript
        status: pass
      - kind: integration
        ref: runner/tests/test_real_controls.c#controls.real.unsupported-opcode
        status: pass
      - kind: integration
        ref: runner/tests/test_real_controls.c#controls.real.output-overflow
        status: pass
      - kind: integration
        ref: runner/tests/test_real_controls.c#controls.real.identity-mismatch
        status: pass
    human_judgment: false
  - id: D2
    description: "Equivalent completed actual guest runs preserve canonical state, content, profile, input, log, and stop evidence across partitioning, tracing, draining, and interleaving on this host."
    verification:
      - kind: integration
        ref: core/tests/test_boot_determinism.c#integration.open-ipl3-determinism
        status: pass
      - kind: other
        ref: sh scripts/check-test-inventory.sh --preset gate
        status: pass
    human_judgment: false
  - id: D3
    description: "Exact guest output accounting preserves attempted totals above the retained capture limit and refuses to claim an exact total after counter saturation."
    verification:
      - kind: unit
        ref: core/tests/test_isviewer.c#attempted-byte-total-and-overflow
        status: pass
      - kind: integration
        ref: runner/tests/test_real_controls.c#controls.real.output-overflow
        status: pass
    human_judgment: false

duration: 80min
completed: 2026-11-11
status: complete
---

# Phase 3 Plan 12: Prove Real Failure Controls and Guest Determinism Summary

**The actual guest path now proves its six negative boundaries and repeatable warm-entry state while keeping exact attempted output separate from captured bytes.**

## Performance

- **Duration:** About 80 minutes from the first behavioral RED through the final scoped implementation commit and verification.
- **Started:** 2026-11-11T11:27:37-05:00 (first real empty-BREAK RED).
- **Completed:** 2026-11-11T12:47:02-05:00 (Task 2 implementation commit; final verification completed immediately afterward).
- **Tasks:** 2 completed.
- **Files changed:** 28 plan files, measured from the plan base and excluding the two human-dirty workspace files.

## Accomplishments

- Added six production-path controls for empty BREAK, empty bound, wrong transcript, unsupported opcode, output overflow, and identity mismatch. The existing descriptor-helper replacement control and actual CPU-backed replacement control both remain registered.
- Kept unknown fixture identities refused and limited pass to the original positive fixture. The empty-BREAK control verifies entry from the actual observed stop PC equal to the authored entry; it does not invent a prior retirement.
- Corrected ISViewer attempted-byte accounting. Every nonzero flush contributes its full requested length, including refused bytes after the 4096-byte retained prefix fills. The counter uses checked arithmetic, commits no partial capture on overflow, and v8 serialization and profile identity bind the added exactness state. Independent vectors and the historical v6/v7 vectors remain.
- Added `integration.open-ipl3-determinism`: actual one-shot and 1/7/31-tick partitions, trace on/off, drain schedules, resource-yield continuation, and two interleaved instances compare at equivalent completed bounds. Deadline checks cover bounds 0/1, 99/100/101, inclusive equal-tick input order, stop behavior, time reversal, and tick-add overflow refusal.
- Audited future-affecting warm CPU/CP0, cache, device, boot, input-queue, and output fields against the v8 digest. Independent v8 expected values are retained alongside v6/v7 qualification vectors. Host allocator context, trace records, and output-drain cursor remain outside canonical guest identity.
- Added canonical result-subset comparison across repeated actual positive runs, excluding compiler-specific `build_id`; the known altered HLE-seed profile digest is rejected by the identity checker. Mutation cases identify the refusal layer and do not claim a checker can detect an arbitrary forged but syntactically valid final state digest.
- Registered and protected the exact 91 gate and 93 sanitizer test identities. The selftest removed each name once, rejected duplicate/disabled/unexpected/empty/enumeration-error cases, and reported 184 per-name removals plus five malformed-inventory controls.

## Task Commits

1. **Task 1 RED:** `e228199` — real empty-BREAK assertion observed a valid named variant refused before guest admission.
2. **Task 1 implementation:** `edaf79d` — real guest boundaries, result handling, exact attempted output accounting, fixture registry, and independent coverage.
3. **Task 2:** `d25f9b7` — actual guest determinism, field audit, identity comparison, and exact inventory registration.

## Key Files

- `runner/tests/test_real_controls.c` — real negative and identity controls, including repeated-run canonical evidence comparison.
- `runner/t64_real.c`, `runner/t64_supervisor.c`, and `runner/t64_real_fixture.c` — admitted named guest variants, bounded execution, exact report/capture/process validation, and fixture identity mapping.
- `core/t64_isviewer.c`, `core/t64_digest.c`, and `core/tests/test_isviewer.c` — exact attempted-byte observation, v8 identity, and overflow regression coverage.
- `core/tests/test_boot_determinism.c` — actual warm-profile partition/interleave/trace/drain comparisons, deadline checks, and field audit.
- `docs/spec/machine-contract.md` and `docs/spec/result-protocol.md` — active identity, output ownership, and evidence limits.
- `scripts/check-test-inventory.sh` — exact expected test identities and per-name omission selftest.

## Decisions Made

- Preserve exact source asset, embedded payload, and authored ROM identities as distinct facts; no third source-asset descriptor is forwarded to the child.
- Count full attempted guest flush lengths, even after capture capacity is exhausted. If the u64 counter saturates, mark it inexact and fail closed instead of serializing the saturated number as an exact total.
- Keep trace-only logical inputs and v1/model result contracts unchanged. Do not describe trace scheduling as SI/controller consumption.
- Keep cross-compiler and cross-OS equality as Plan 13 work; this plan proves local repeatability only.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] The output-loss flag did not establish an exact attempted-byte count.**
- **Found during:** Task 1, real overflow control.
- **Issue:** A full capture plus a loss indicator could not distinguish a 4097-byte request from a larger refused flush, so reporting `retained + 1` would invent a total.
- **Fix:** Track the full requested flush length in the core's attempted-total counter with checked u64 arithmetic; add an explicit exactness flag to the serialized warm state and bump the active state format to v8. Keep v6/v7 vectors and independently derive the new profile and initial state vectors.
- **Files modified:** `core/include/tendies64/t64_machine.h`, `core/t64_internal.h`, `core/t64_isviewer.c`, `core/t64_digest.c`, `core/t64_machine.c`, `core/t64_observe.c`, `core/tests/test_isviewer.c`, `core/tests/test_boot_entry.c`, `core/tests/test_tracer.c`, `docs/spec/machine-contract.md`, `docs/spec/boot-limits.md`.
- **Verification:** Core overflow tests cover refused multi-byte requests, checked arithmetic, and no partial append; the actual overflow guest reports 4097 attempted bytes while retaining exactly 4096.
- **Committed in:** `edaf79d`.

**2. [Rule 1 - Bug] A defensive producer detail fell outside the closed result-v2 enum.**
- **Found during:** Task 1, producer error-path review.
- **Issue:** The core-content mismatch defensive branch could emit an unrecognized detail.
- **Fix:** Map that host-side failure to the admitted host-error representation and retain a regression check for invalid fixture refusal versus absent-fixture not-started.
- **Files modified:** `runner/t64_real.c`, `runner/tests/test_real_controls.c`, `runner/tests/test_result_v2.c`.
- **Verification:** Focused result-v2 and real-control tests passed; the final 91-test gate passed.
- **Committed in:** `edaf79d`.

## Issues Encountered

- The first semantic RED used the generated, rights-cleared `hello-empty-break` variant and failed because the real supervisor refused the fixture before guest admission. This was the intended missing production path, not a setup or executable failure; the recorded assertion is in `03-12-task1-red.json`.
- The first ASan inventory enumeration used a stale generated CMake tree and lacked the new registrations. Reconfiguring the ASan preset fixed the generated tree; the one per-name inventory selftest was run once and passed.
- The complete ASan/UBSan build emitted existing stack-frame-size and duplicate-library warnings. It completed successfully; no new failure was found.

## Automated Checks

- `ctest --preset gate --no-tests=error -R '^controls\.real\.(empty-halt|empty-bound|wrong-transcript|unsupported-opcode|output-overflow|identity-mismatch)$' --output-on-failure` — six real controls passed in the focused Task 1 run.
- `sh scripts/check-test-inventory.sh --selftest` — passed once; 91/91 gate names and 93/93 sanitizer names were each tested for omission, plus five malformed-inventory controls.
- `sh scripts/check-test-inventory.sh --preset gate` — passed, 91 names.
- `sh scripts/check-test-inventory.sh --preset asan` — passed, 93 names.
- `cmake --build --preset gate && ctest --preset gate --no-tests=error --output-on-failure` — full build and all 91 tests passed; total CTest time 22.87 seconds.
- `cmake --build --preset asan` — all sanitizer targets built successfully.
- `ASAN_OPTIONS=detect_leaks=0 ctest --preset asan --no-tests=error -R '^(integration\.open-ipl3-determinism|controls\.real\.identity-mismatch|controls\.real\.output-overflow|unit\.core\.boot-entry|unit\.core\.isviewer|unit\.core\.tracer)$' --output-on-failure` — all six selected ASan/UBSan tests passed; the real determinism integration took 58.74 seconds. This is a focused selection, not the full sanitizer inventory run.

## Limits

- Determinism evidence covers the pinned warm-entry profile, authored hello ROM, and input bytes on the current host. Cross-compiler/cross-OS equality requires the two actual hosted artifacts and comparator in Plan 13; no hosted comparison is claimed here.
- The actual output-overflow guest demonstrates 4097 attempted bytes and 4096 retained. Counter saturation is directly injected in core tests but is unreachable through the bounded producer; if exactness is false in a real producer result, it emits a postlaunch `harness-error/host-error` with zero trusted progress counts and null unavailable output/state identities, never a pass or saturated exact total.
- This evidence does not establish cold IPL2/PIF execution, general game compatibility, hardware timing accuracy, or controller consumption. Logical inputs are trace-only; the boot path is a prepared warm-entry profile.

## Next Phase Readiness

Plan 13 can collect and compare sanitized, exact-SHA Linux/GCC and macOS/Clang guest evidence for the same tested implementation. No user action is needed for this plan's local work.

## Self-Check: PASSED

- Both created files exist: `core/tests/test_boot_determinism.c` and `03-12-task1-red.json`.
- All task commits exist in history: `e228199`, `edaf79d`, and `d25f9b7`.

