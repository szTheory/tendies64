---
phase: 02-deterministic-machine-skeleton
plan: "01"
subsystem: core
tags: [c17, cmake, sha256, deterministic-core, machine-profile]

# Dependency graph
requires: []
provides:
  - Opaque host-allocated machine lifecycle with owned canonical content admission and bounded idle guest-time advancement
  - Versioned deterministic profile identity, build fingerprint, and separate Git source revision evidence
  - Shared authored SHA-256 target and production-core determinism lint boundary
affects: [phase-02-device-slices, profile-selection, core-lint]

# Actuals (#2632), measured from the realized plan diff (80,753 chars / 4).
actuals:
  tokens: 20188
  tasks: 3
  commits: 6
plan_head_before: 7e065589295b28ac9130a9f80d6d483904456704
plan_head_after: a426f6430c29d4f070d2c112a230d3d1a4bbcfaa
commits: 6

# Tech tracking
tech-stack:
  added: []
  patterns:
    - C17 opaque instance API with host allocator ownership
    - Explicit big-endian versioned profile serialization
    - Source-byte build fingerprint separated from machine state and Git revision
    - Production source glob separated from host-only test executables

key-files:
  created:
    - core/CMakeLists.txt
    - core/include/tendies64/t64_machine.h
    - core/t64_machine.c
    - core/tests/test_tracer.c
    - core/tests/test_profile.c
    - docs/profile-limits.md
    - .planning/phases/02-deterministic-machine-skeleton/02-01-red-evidence.json
    - .planning/phases/02-deterministic-machine-skeleton/02-01-profile-red-evidence.json
  modified:
    - CMakeLists.txt
    - tools/common/CMakeLists.txt
    - tools/lint/core-lint.sh

key-decisions:
  - "Keep the machine API provisional and make the named NUS-CPU-03 target explicitly unqualified."
  - "Use model RSP-cycle ticks and versioned atomic PI/SP policies as deterministic choices, not measured hardware timing."
  - "Keep build fingerprint and Git source revision outside profile and machine-state identity."
  - "Preserve the synthetic facade behavior and stop claims at the tested skeleton boundary."

requirements-completed: [PROF-01, PROF-02, PROF-03, PROF-04, CORE-01, BUS-04]

coverage:
  - id: D1
    description: "The public machine seam owns admitted image bytes, advances idle model time to an absolute bound, hashes state, and releases its two host allocations."
    requirement: CORE-01
    verification:
      - kind: unit
        ref: "core/tests/test_tracer.c#unit.core.tracer"
        status: pass
    human_judgment: false
  - id: D2
    description: "The named profile has fixed canonical bytes and behavior-field hashing; unknown selection refuses and ROM country metadata does not choose TV/region."
    requirement: PROF-01
    verification:
      - kind: unit
        ref: "core/tests/test_profile.c#unit.core.profile"
        status: pass
    human_judgment: false
  - id: D3
    description: "Core and host tools share one authored SHA-256 target, with production core and hash sources covered by the lint gate."
    requirement: PROF-03
    verification:
      - kind: integration
        ref: "./scripts/gate.sh (unit.common, core-lint selftests, core-lint)"
        status: pass
    human_judgment: false
  - id: D4
    description: "Profile limits state unsupported, deferred, and unknown areas with concrete evidence needed for promotion."
    requirement: PROF-04
    verification:
      - kind: other
        ref: "docs/profile-limits.md; repo.docs and gov.claims.repo in ./scripts/gate.sh"
        status: pass
    human_judgment: true
    rationale: "The document structure is checked, but deciding whether future evidence justifies a hardware-support claim requires domain judgment."
  - id: D5
    description: "Build identity is a source/build fingerprint without local paths or timestamps, separate from profile and state identity; test output names its evidence tier."
    requirement: PROF-02
    verification:
      - kind: unit
        ref: "core/tests/test_tracer.c and core/tests/test_profile.c"
        status: pass
      - kind: integration
        ref: "gov.pii.repo in ./scripts/gate.sh"
        status: pass
    human_judgment: false

# Metrics
duration: 31min
completed: 2026-11-10
status: complete
---

# Phase 02 Plan 01: Deterministic Machine Skeleton Summary

**Owned-content machine tracer, versioned NTSC skeleton profile, source-derived build identity, shared SHA-256, and explicit limits on hardware claims**

## Performance

- **Duration:** 31 min
- **Started:** 2026-11-10T16:02:31Z (first recorded TDD run)
- **Completed:** 2026-11-10T16:33:33Z
- **Tasks:** 3
- **Files modified:** 11

## Accomplishments

- Added a provisional opaque C17 machine seam. It validates host-provided allocations, copies authored canonical-order bytes into owned storage, advances idle guest time to an absolute tick, produces repeatable content/state digests, and releases owned allocations on destruction.
- Added the versioned `ntsc-cart-4m-skeleton` descriptor and canonical serialization. Every behavior-affecting field contributes to profile identity; build fingerprint and source revision remain separate from profile and machine-state identity.
- Consolidated SHA-256 into one target shared by core and host tools, extended the production-core lint boundary, and documented unsupported, deferred, and unknown behavior with promotion evidence.
- Preserved the synthetic facade and labeled native output as `unit-tested synthetic counters; no guest execution`.

## Task Commits

1. **Task 1 RED: owned machine tracer** - `2f1ba64` (test)
2. **Task 1 GREEN: owned deterministic machine tracer** - `d6ec8dc` (feat)
3. **Task 2: shared and linted SHA implementation** - `fd47137` (refactor)
4. **Task 3 RED: profile serialization** - `433f7aa` (test)
5. **Task 3 GREEN: versioned skeleton profile identity** - `ddc808f` (feat)
6. **Source-revision refresh after ref changes** - `a426f64` (fix)

Both TDD RED runs were registered failures on the planned missing production behavior and passed semantic RED-evidence classification before their implementations were added.

## Files Created/Modified

- `core/include/tendies64/t64_machine.h`, `core/t64_machine.c` - provisional machine, allocation, admission, advancement, profile, digest, and identity contracts.
- `core/tests/test_tracer.c`, `core/tests/test_profile.c` - end-to-end ownership and profile identity tests with independent golden serialization.
- `core/CMakeLists.txt`, `CMakeLists.txt` - production/test source separation, identity fingerprint, and core test registration.
- `tools/common/CMakeLists.txt`, `tools/lint/core-lint.sh` - one shared SHA target and production-only determinism lint coverage.
- `docs/profile-limits.md` - current descriptor, evidence boundary, and promotion criteria.
- `.planning/phases/02-deterministic-machine-skeleton/02-01-red-evidence.json`, `.planning/phases/02-deterministic-machine-skeleton/02-01-profile-red-evidence.json` - TDD RED evidence.

## Decisions Made

- The NUS-CPU-03 label is an unqualified target. The open-IPL3 route records intent; boot remains unimplemented.
- Guest ticks, scheduler order, PI/SP atomic timing, queue capacity, and refusal policies are explicit versioned model choices, not hardware measurements.
- Source-derived build identity and Git revision are reported as evidence only; neither enters the profile hash or machine-state digest.
- The C interface remains provisional so later device plans can refine ownership, refusal, and ABI details without claiming compatibility prematurely.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking build issue] Kept the identity macro local to the core target**

- **Found during:** Task 3 focused build
- **Issue:** Propagating `T64_BUILD_ID` publicly collided with the synthetic runner's existing `T64_BUILD_ID` definition under warnings-as-errors.
- **Fix:** Made the generated identity definitions private to `t64_core` and exposed identity through the machine API getters used by tests.
- **Files modified:** `core/CMakeLists.txt`
- **Verification:** Full `./scripts/gate.sh` passed after the fix.
- **Committed in:** `ddc808f`

**2. [Rule 3 - Blocking build issue] Refreshed source revision when the Git ref changes**

- **Found during:** Final identity verification
- **Issue:** Configure-time source revision stayed at the prior commit after a later task commit, so test evidence did not identify the revision used to build the final binaries.
- **Fix:** Added the Git `HEAD`, active branch ref, and packed-ref files as CMake configure dependencies. Reconfiguration now updates the separately reported source revision when the ref advances.
- **Files modified:** `core/CMakeLists.txt`
- **Verification:** Reconfigured and rebuilt; tracer and profile output report `a426f6430c29d4f070d2c112a230d3d1a4bbcfaa`, matching the producing source commit. Full `./scripts/gate.sh` passed.
- **Committed in:** `a426f64`

**Total deviations:** 2 auto-fixed build/identity issues. Both were directly within plan scope.

## Verification

- `cmake --preset gate` - passed.
- `cmake --build --preset gate` - passed with warnings-as-errors.
- `ctest --preset gate --no-tests=error -R '^unit\.core\.(tracer|profile)$' --output-on-failure` - both tests passed.
- Direct tracer: 51 checks, 0 failures. Direct profile: 161 checks, 0 failures.
- `./scripts/gate.sh` - passed all 63 CTests, all 27 core-lint selftest seeds, and the production core lint scan (`files=6`, `violations=0`). Existing hash vectors and synthetic controls passed.
- Emitted profile SHA-256: `87e91129228d0dde39c761fa160e7faa75849818a6fe2752bbfa0c935d787aee`.
- Emitted build ID: `t64-build-v1-daf289b351ce2c13300ebabaee1109577f61fb1fbf518e5f5aeb963ab19763a2`.
- Emitted source revision: `a426f6430c29d4f070d2c112a230d3d1a4bbcfaa`.

## Issues Encountered

The profile is a deterministic skeleton only. CPU execution, PIF/IPL3 boot, hardware timing, VI/audio output, expansion devices, and a stable external ABI remain unsupported, deferred, or unknown as recorded in `docs/profile-limits.md`. The passing tracer is not guest execution or hardware-accuracy evidence.

## User Setup Required

None - no external service configuration or package installation was required.

## Next Phase Readiness

The next device slices have an owned-content lifecycle, guest-time boundary, canonical profile identity, build evidence, and linted hash primitive to build on. They must preserve the documented profile qualification limits and add behavior only with slice-specific evidence.

---
*Phase: 02-deterministic-machine-skeleton*
*Completed: 2026-11-10*

## Self-Check: PASSED

- Summary file exists at the required phase path.
- All six plan commits are ancestors of the current HEAD.
