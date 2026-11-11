---
phase: 03-first-headless-rom-through-open-ipl3
plan: "08"
subsystem: runner/input
tags: [C17, JSON, SHA-256, deterministic-input, fixture-provenance]
requires:
  - phase: 03-07
    provides: scheduled trace-only input API and canonical machine digest
provides:
  - closed bounded t64.input v1 parser with owned canonical bytes and SHA-256 identity
  - actual-boot input trace evidence across guest-time partitions and trace modes
affects: [headless-runner, phase-03-profile-qualification, fixture-manifest]
actuals:
  tokens: 14961.25
  tasks: 1
  commits: 3
plan_head_before: d05fa28eb9c717edf2a933164de66b84347f25a5
plan_head_after: a4b3475fd667aeb92f6f9a0947d54aa233a7e74a
tech-stack:
  added: []
  patterns:
    - decoded-length field comparisons keep closed JSON keys and values from accepting hidden NUL suffixes
    - owned canonical schedule bytes and digest are produced only after complete validation
    - fresh machines receive the complete schedule before bounded guest execution
key-files:
  created:
    - runner/t64_input.c
    - runner/t64_input.h
    - runner/tests/test_input.c
    - .planning/phases/03-first-headless-rom-through-open-ipl3/03-08-input-red.json
  modified:
    - runner/CMakeLists.txt
    - scripts/check-test-inventory.sh
    - scripts/build-boot-fixtures.sh
    - fixtures/manifest.json
key-decisions:
  - "Keep schema v1 closed and canonical: uint64 ticks are decimal strings, event arrays preserve equal-tick source order, and SHA-256 covers canonical bytes including the final LF."
  - "Queue validated inputs only on a fresh owned machine before execution; scheduled source-6/kind-1 traces prove timing, not SI or controller consumption."
  - "Pin the fixture producer source at the commit containing the exact four hashed source inputs, and preserve all seven generated ROM identities."
requirements-completed: []
requirements-pending: [RUN-01, PROF-05]
metrics:
  duration: 27min
  completed: 2026-11-11
  status: complete
---

# Phase 3 Plan 08: Canonical Timed Input Schedules Summary

**Closed t64.input v1 parsing now gives every accepted schedule stable bytes and verifies scheduled trace timing through the authored hello's pre-BREAK bound.**

## Created Paths

- `runner/t64_input.c` — bounded parser, canonical writer, owned schedule lifecycle, and SHA-256 identity.
- `runner/t64_input.h` — parser limits, event accessors, status contract, and lifecycle API.
- `runner/tests/test_input.c` — 51 TAP checks for schema edges and the actual IPL3/hello input-partition control.
- `.planning/phases/03-first-headless-rom-through-open-ipl3/03-08-input-red.json` — portable semantic RED evidence accepted by the TDD evidence checker.

## Performance

- **Duration:** 24 minutes
- **Started:** 2026-11-11T11:52:02Z
- **Completed:** 2026-11-11T12:18:37Z
- **Tasks:** 1
- **Files modified or created:** 8

## Accomplishments

- Implemented closed `t64.input` schema v1 parsing with a 65,536-byte limit, a 128-event limit, exact-length key/value comparisons, canonical unsigned-decimal uint64 tick strings, nondecreasing time validation, and full port/button/axis bounds.
- Kept equal-tick events in input-array order. The parser now returns caller-owned canonical bytes and a digest only after all fields and events pass validation; refusal leaves the caller's schedule pointer unchanged.
- Registered `controls.real.input-partition` with the boot-fixture producer dependency and CTest fixture paths. The control parses and queues a 128-event schedule before running the pinned IPL3 and authored hello on fresh instances.
- Drained matching input source/kind trace records at intervals no longer than 16 guest ticks. The run includes the input at tick 530437 and retains the event at tick 530438 as pending. Two traced partitions and one untraced partition end with the same canonical machine digest and exact 30-byte output.
- Corrected the generated empty schedule and metadata to canonical `t64.input` v1. Refreshed all seven authored hello fixture recipes, source-content hashes, and source revisions; fixture qualification passed and all seven ROM digests remained unchanged.

## TDD Evidence

- **RED:** The compiled target failed the named TAP behavior `accepts an empty canonical t64.input v1 schedule`: expected `T64_INPUT_OK`, observed `T64_INPUT_INVALID_SCHEMA` from the linkable scaffold. `gsd_run check tdd-red-evidence` returned `RED_EVIDENCE_OK`; the complete portable record is committed at the path listed above.
- **GREEN:** The production parser and expanded control passed 51 TAP checks, including exact raw-size and event-count boundaries, malformed/closed-field cases, equal-tick order, canonical identity, and actual guest-time trace behavior.

## Automated Checks

- Focused gate control: `controls.real.input-partition` passed (1/1).
- Full gate configure/build/test passed (79/79); test inventory passed at 79.
- Full ASan/UBSan configure/build/test passed with `ASAN_OPTIONS=detect_leaks=0` (81/81); test inventory passed at 81.
- `sh scripts/build-boot-fixtures.sh --qualify` passed after source pins were refreshed. The generated empty schedule bytes matched canonical JSON including its final LF, and all seven ROM hashes matched their prior manifest identities.
- `sh scripts/pii-scan.sh --tree` scanned 491 files with zero findings.
- `git diff --check` passed before the feature commit. The feature commit introduced no file deletions.

## Task Commits

1. **TDD RED: empty canonical schedule behavior** — `e64edcc` (`test`)
2. **Canonical fixture producer and metadata format** — `e8b1673` (`fix`)
3. **Bounded parser and real input-partition control** — `a4b3475` (`feat`)

## Modified Paths

- `runner/CMakeLists.txt` — builds the parser and registers the real input control with fixture environment and producer dependency.
- `scripts/check-test-inventory.sh` — adds exactly `controls.real.input-partition`, raising gate/ASan inventories to 79/81.
- `scripts/build-boot-fixtures.sh` — emits the canonical empty v1 schedule and matching format metadata.
- `fixtures/manifest.json` — refreshes the seven authored hello entries to the pinned producer source revision and content hash without changing ROM digests or rights records.

## Decisions Made

- Tick values remain strings so the complete uint64 range is exact and canonical.
- The parser remains host-side and detached from core advancement. Tests queue an entire validated schedule only on a fresh machine; no arbitrary-machine batch transaction is claimed.
- The input trace establishes scheduling time and order only. It does not establish controller consumption, hardware accuracy, or guest completion.

## Deviations from Plan

None. The fixture schema and manifest correction was explicit delegated scope. No core source, protected-media, rights, or licensing records changed.

## Issues Encountered

The initial test harness compile exposed a missing format annotation and a malformed negative-case JSON literal; both were corrected before the semantic RED/GREEN cycle. The sanitizer build showed existing large-stack-frame warnings in core digest, SP, CPU, and machine functions; the new parser and control emitted none.

## Next Phase Readiness

The parser, fixture identity, and bounded trace evidence are ready for the subsequent runner work. `RUN-01` and `PROF-05` remain pending until the broader runner and final hosted proof establish their full scope. The current observation ends at the fixed pre-BREAK bound and makes no controller-consumption or guest-pass claim.

## Self-Check: PASSED

All four created paths exist, all three task commits are ancestors of the plan head, and the persisted plan ledger measures three task commits.
