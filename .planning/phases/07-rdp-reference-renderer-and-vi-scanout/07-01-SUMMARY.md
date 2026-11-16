---
phase: 07-rdp-reference-renderer-and-vi-scanout
plan: "01"
subsystem: graphics
tags: [rdp, vi, c17, ctest, fixture-provenance]
requires: []
provides:
  - Scheduled RDP fill effects and FullSync completion through normal machine dispatch.
  - Guest-time diagnostic VI scanout delivered as a raw RGBA8 packet.
  - Authored fill-to-frame fixture, actual headless runner comparison, and refusal controls.
affects: [phase-07, rdp, vi, profile-serialization, fixture-qualification]
actuals:
  tokens: 49692
  tasks: 2
  commits: 4
plan_head_before: 18dcaa0c4849f4c2841d1fb4c14959600e42b257
plan_head_after: 29c25b7cdd0469e4aaed42fc9609a925d3aedc94
tech-stack:
  added: []
  patterns:
    - Per-machine scheduled RDP state with bounded incremental effects.
    - Exact rational guest-time VI phase and bounded lossless raw packet queue.
    - Separate authored RDP fixture qualification preserves the legacy hello builder pin.
key-files:
  created:
    - core/t64_rdp.c
    - core/t64_vi.c
    - fixtures/rdp/fill.S
    - scripts/build-rdp-fill-fixture.sh
    - scripts/check-rdp-vi-cases.py
    - runner/tests/test_rdp_video.c
  modified:
    - core/t64_machine.c
    - core/t64_scheduler.c
    - core/t64_digest.c
    - core/CMakeLists.txt
    - runner/t64_real.c
    - runner/t64_supervisor.c
    - runner/CMakeLists.txt
    - fixtures/manifest.json
    - docs/spec/rdp.md
    - docs/spec/vi.md
    - docs/spec/rdp-vi-cases.json
key-decisions:
  - "Keep this tracer to one authored fill mode and the programmed diagnostic VI geometry; later Phase 07 plans own broader modes."
  - "Only descriptor v5 serializes the new RDP/VI policy fields, preserving older and unknown-version legacy layouts."
  - "Move RDP fixture building to its own helper so the historical boot builder and RSP whole-file pin remain byte-identical."
patterns-established:
  - "Guest device effects advance through the scheduler; the runner reads raw packets without rendering into machine memory."
  - "Fixture source, composition, rights, and generated identities are independently qualified and build-only."
requirements-completed: []
requirements-contributed: [VID-01, VID-02, VID-03, VID-05, VID-06]
coverage:
  - id: D1
    description: "The authored CPU guest reaches scheduled RDP fill and the actual runner checks the raw VI frame."
    requirement: VID-01
    verification:
      - kind: unit
        ref: "CTest unit.core.rdp.fill"
        status: pass
      - kind: integration
        ref: "CTest runner.rdp-fill"
        status: pass
    human_judgment: false
  - id: D2
    description: "VI emits the independently expected diagnostic packet and refuses malformed or undersized copy requests atomically."
    requirement: VID-02
    verification:
      - kind: unit
        ref: "CTest unit.core.vi.fill"
        status: pass
      - kind: integration
        ref: "CTest controls.real.rdp-wrong-pixel"
        status: pass
    human_judgment: false
  - id: D3
    description: "Profile serialization and digest cover the new device policies while existing profile descriptors retain their prior encoding."
    requirement: VID-05
    verification:
      - kind: unit
        ref: "CTest unit.core.profile"
        status: pass
      - kind: integration
        ref: "./scripts/gate.sh"
        status: pass
    human_judgment: false
duration: "3h 47m since earliest retained RED run; exact executor start was not preserved"
completed: 2026-11-16
status: complete
---

# Phase 07 Plan 01: Production fill-to-frame tracer Summary

**A CPU-authored RDP fill now travels through scheduled machine effects and the actual headless runner into an independently checked raw VI frame.**

## Performance

- **Duration:** 3h 47m from the earliest retained target RED run; exact executor start was not preserved.
- **Started evidence:** 2026-11-16T01:38:00-05:00 (`unit.core.rdp.fill` RED log).
- **Completed:** 2026-11-16.
- **Tasks:** 2.
- **Files modified:** 32.

## Accomplishments

- Added the bounded scheduled DP fill path, MI FullSync fence, and guest-time VI scanout with per-machine state, digest/serialization, packet metadata, and atomic raw packet copy-out.
- Added the CPU-authored fill guest and exercised ordinary admission, bus dispatch, scheduler progress, RDRAM expectations, VI output, and semantic milestones through the headless runner. The same valid setup reaches the wrong-pixel comparator and reports its first difference.
- Added refusal coverage for invalid spans, truncated/unsupported commands, scheduler exhaustion, empty/zero-area cases, bad copy capacity/overlap, replacement fixtures, and production source ownership.
- Kept the old boot fixture builder and the RSP smoke source pin byte-identical. Restored the historical hello fixture source/oracle revisions and isolated RDP generation, tool checks, composition, and rights qualification in `scripts/build-rdp-fill-fixture.sh`.
- The generated RDP ROM remains build-only and retains its verified 4552-byte identity (`b469340e…`); the 456-byte authored payload retains `e2fc24f7…`. The embedded IPL3 bytes are separately identified and rights-qualified.

## Verification

- `./scripts/gate.sh` — passed; 145/145 CTests, inventory, shell syntax, fixture qualification, evidence self-tests, and core lint passed. Full log: `build/gate/07-01-final-normal-gate.log`.
- `cmake --preset asan` and `cmake --build --preset asan -j2` — passed.
- `ctest --test-dir build/asan --no-tests=error --output-on-failure` — passed; 147/147 tests. Full log and retained `LastTest.log`: `build/07-01-asan-ctest.log`, `build/asan/07-01-asan-LastTest.log`.
- Focused fill suite — passed 5/5, including `runner.rdp-fill`, same-setup wrong-pixel rejection, and replacement-fixture refusal. The owning `LastTest.log` is retained at `build/gate/07-01-fixture-final-LastTest.log`.
- `sh scripts/build-boot-fixtures.sh --qualify`, `sh scripts/build-rdp-fill-fixture.sh --qualify`, and `sh scripts/build-rsp-smoke.sh --build` — passed. The RSP build produced all six authored smoke variants with its exact historical source pin intact.
- `python3 -B scripts/tests/test_base_evidence.py` — passed, 14 tests including public receipt mutation rejection and preserved hello oracle/source pins.
- RSP no-HLE source/object check — passed for 76 sources; its mutation suite passed with the exact roster guard. Manifest schema check and the nonempty gate inventory passed (145 cases).

## Task Commits

1. **Task 1: Prove one CPU fill to raw-frame path end to end** — `05e4bb8` (`feat`).
2. **Task 2: Reject malformed fill admission and prove tracer ownership guards** — `e48daa7` (`test`).

## Compatibility Follow-up Commits

- `0a12480` — restored the legacy builder, split the RDP helper, fixed v5-only profile validation, and pinned the unchanged hello receipt identities.
- `29c25b7` — updated the RDP fixture source/content provenance to the isolated helper.

## TDD Gate Compliance

- The plan is `type: execute` with task-level TDD. Task 1's target `unit.core.rdp.fill` failed on the intended missing DP register behavior before implementation; the retained report is `build/gate/07-01-red-unit.rdp.fill.log`. The task's atomic feature commit includes the tests and implementation, and the final unit and actual-runner tests pass.
- An early `runner.rdp-fill` failure was a fixture/report setup failure before the semantic assertion, so it was not counted as RED. Task 2 added a test-only production source-roster mutation guard; its passing ownership mutation suite is covered by the final gate.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Preserve legacy profile descriptors while validating the new v5 policy fields**

- **Found during:** Full normal gate.
- **Issue:** The new policy validation treated synthetic descriptor version 18 as requiring fields only serialized by descriptor v5, breaking `unit.core.profile`.
- **Fix:** Validate the new fields only for descriptor version 5; preserve the prior layout for older and unknown versions.
- **Files modified:** `core/t64_machine.c`.
- **Verification:** `unit.core.profile`, normal gate, and ASAN CTest passed.
- **Committed in:** `0a12480`.

**2. [Rule 3 - Blocking compatibility] Keep historical fixture authority unchanged while qualifying the new RDP fixture**

- **Found during:** Full normal gate.
- **Issue:** Appending RDP generation to the shared hello builder changed the exact file pinned by the RSP smoke check and changed legacy hello provenance fields used by BASE receipt validation.
- **Fix:** Restored the shared builder byte-for-byte, returned all nine hello fixture source/oracle pins to their prior identities, and added an independent RDP build/qualification path with explicit CMake dependencies and manifest provenance. No pin was widened.
- **Files modified:** `core/CMakeLists.txt`, `runner/CMakeLists.txt`, `fixtures/manifest.json`, `scripts/build-boot-fixtures.sh`, `scripts/build-rdp-fill-fixture.sh`, `scripts/tests/test_base_evidence.py`.
- **Verification:** Both fixture qualifiers passed; RSP smoke retained its exact source pin and all six builds passed; BASE evidence controls and the complete normal/ASAN gates passed.
- **Committed in:** `0a12480`, `29c25b7`.

**Total deviations:** 2 automatic fixes (one Rule 1 bug, one Rule 3 compatibility fix). Both were required to preserve legacy behavior and the approved fixture authority; the tracer scope remains one fill mode.

## Issues Encountered

The first full normal gate exposed the profile descriptor regression, RSP source-pin mismatch, and BASE receipt manifest mismatch. Each cause was corrected and the complete gate passed afterward. No private BASE campaign was replayed, and no historical report was relabeled.

## User Setup Required

None.

## Next Phase Readiness

The Phase 07-01 tracer is complete and verified. Broader RDP modes and standard VI timing remain for their owning plans. VID-04 remains explicitly blocked and is not satisfied by this authored tracer; no hardware-accuracy claim is made.

## Self-Check: PASSED

- The summary file was committed successfully.
- All four plan commits are ancestors of the plan head recorded above.
- The normal gate, ASAN build, and complete ASAN CTest run have retained logs.

---
*Phase: 07-rdp-reference-renderer-and-vi-scanout*
*Completed: 2026-11-16*

## Orchestrator handoff check

The nonempty fill suite was rerun successfully after handoff. Schema, codebase
and UI post-wave checks reported no block. No merge or runtime source change
followed the full passing native/instrumented cohorts, so those exact gates
serve the serial-wave regression check; no broad suite was replayed for tracking.
Current normal/instrumented consumers, authored ROM/build metadata, manifest
and owning logs were retained with checked SHA-256 values under
`build/gate/07-01-retained-artifacts/binding.json`. This is honest post-use
retention, not a claim that pre-use archival occurred.

Requirement fields record contribution only: this fill tracer does not complete
all VID-01/02/03/05/06 obligations, remaining command/filter/timing work, or VID-04.
All active requirement statuses remain pending until their full assigned work
and evidence pass. No historical private campaign was replayed.

## Post-tracer review repair

Independent review allegations were checked with a cheap real authored consumer
before changing production. The inherited digest allegation was disproven; a
valid VI register write and inherited warm-state mutation both change the v14
digest. The host-cursor hashing proposal conflicted with the approved observer
boundary. A focused regression instead exposed copy-time event rescheduling,
equal-tick reorder and drain-dependent producer-slot hashing. Commit `6752bba`
retains blocked VI events before pop, removes copy-time scheduling and preserves
the ring cursor across empty state. Existing produced packet bytes/metadata remain
hashed; observer cursor/occupancy remain outside canonical identity.

The full normal gate passes. The full instrumented run passed all rendering
checks but timed out in one BASE harness; an isolated unchanged-limit rerun passed
all its controls. The original failure and the unproven contention inference are
retained in [the fix record](07-REVIEW-FIX.md). Independent re-review finds no new
source defect. Exact repaired normal and instrumented consumers/bindings are
retained under `build/gate/07-vi-repair-retained-artifacts/`. No original pre-use
retention gap is relabeled. Hosted qualification of repaired source `69ed85c` passed on Linux and macOS
(run43869199965). Exact selected authored artifacts were collected and compared;
see the measured review in `docs/ci-reviews/LOG.md` (F-082).
