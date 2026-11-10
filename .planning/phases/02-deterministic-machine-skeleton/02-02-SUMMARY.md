---
phase: 02-deterministic-machine-skeleton
plan: "02"
subsystem: machine
tags: [ROM, lifecycle, allocator, C]

requires:
  - phase: 02-deterministic-machine-skeleton
    provides: Deterministic machine, profile, content identity, and state digest baseline from plan 02-01.
provides:
  - Transactional canonical ROM admission for all three supported dump orders.
  - Checked allocator descriptor adoption and replacement lifecycle guarantees.
  - Read-only ROM size/header observations and host precondition contract.
affects: [02-deterministic-machine-skeleton]

actuals:
  tokens: 23150
  tasks: 2
  commits: 4
plan_head_before: aa0399bfed639d96d8372b2fa240ba9d969cd03d
plan_head_after: 123f1dc0ef7ba583e11dd33a644745cd43075804

tech-stack:
  added: []
  patterns:
    - Candidate ownership is prepared and validated before the live machine is changed.
    - Host allocation spans use checked half-open ranges for alias rejection.

key-files:
  created:
    - core/t64_internal.h
    - core/t64_rom.c
    - core/tests/test_rom_admission.c
    - core/tests/test_lifecycle.c
    - docs/spec/machine-contract.md
  modified:
    - core/t64_machine.c
    - core/include/tendies64/t64_machine.h
    - core/CMakeLists.txt
    - core/tests/test_profile.c
    - core/tests/test_tracer.c

key-decisions:
  - "Normalize the three known raw dump orders into newly owned canonical bytes before committing admission."
  - "Expose the fixed 64-byte header and admitted size through read-only observers, with unchanged outputs on refusal."
  - "Validate both reported storage and requested storage extents before cleanup so an aliased descriptor is never released as a new allocation."
  - "Document readable/stable spans, truthful capacity, no allocator reentry, and cross-instance disjointness as host preconditions the core cannot independently prove."

patterns-established:
  - "ROM candidate preparation owns all temporary storage and cannot expose a partial live update."
  - "Refusal tests compare digest, content, profile, header, size, and allocator ownership state."

requirements-completed: [BUS-04, PROF-03, PROF-06]

coverage:
  - id: D1
    description: "The public admission path accepts all three supported raw byte orders and exposes identical canonical identity and fixed header bytes."
    requirement: PROF-03
    verification:
      - kind: unit
        ref: "unit.core.rom-admission"
        status: pass
    human_judgment: false
  - id: D2
    description: "Invalid admission and allocator descriptors preserve machine state and ownership; valid replacement installs the new image before releasing the old one."
    requirement: PROF-06
    verification:
      - kind: unit
        ref: "unit.core.lifecycle"
        status: pass
      - kind: unit
        ref: "unit.core.rom-admission"
        status: pass
    human_judgment: false
  - id: D3
    description: "The machine contract states host readability, stable-span, allocator-capacity, reentry, and disjoint-storage preconditions."
    requirement: BUS-04
    verification:
      - kind: other
        ref: "repo.docs-selftest"
        status: pass
    human_judgment: true
    rationale: "The core cannot establish whether a caller's memory is truly readable or whether a host allocator reports truthful backing capacity."

duration: 40min
completed: 2026-11-10
status: halted
---

# Phase 2 Plan 2: Deterministic Machine Skeleton Summary

**Canonical ROM ownership and allocator-safe replacement now have executable boundary, metadata, refusal, and lifecycle contracts.**

## Performance

- **Duration:** 40 minutes
- **Started:** 2026-11-10T11:39:08-05:00
- **Completed:** 2026-11-10T12:19:35-05:00
- **Tasks:** 2
- **Files modified:** 12

## Accomplishments

- Added a private ROM candidate pipeline that validates lengths and orders, normalizes all supported formats, parses the fixed header, and computes identity before changing live machine state.
- Added runtime-generated boundary and refusal tests, checking canonical bytes, digest, all header bytes, profile, state, and allocation ownership.
- Added allocator alias/range protections, deterministic replacement checks, read-only ROM size/header observers, and a machine contract describing host preconditions honestly.

## Task Commits

1. **Task 1 RED: Admit all byte orders through the production transaction** — `87d9697` (`test`)
2. **Task 1 GREEN: Normalize and own raw ROM admission candidates** — `e54210a` (`feat`)
3. **Task 2 RED: Enforce allocator, aliasing and replacement postconditions** — `688b8da` (`test`)
4. **Task 2 GREEN: Enforce lifecycle storage and observer contracts** — `123f1dc` (`feat`)

## Files Created/Modified

- `core/t64_rom.c` and `core/t64_internal.h` — candidate validation, normalization, fixed header parsing, and private ownership.
- `core/t64_machine.c` and `core/include/tendies64/t64_machine.h` — transactional admission/replacement and size/header observers.
- `core/tests/test_rom_admission.c` and `core/tests/test_lifecycle.c` — authored in-memory ROM patterns and public-boundary acceptance/refusal checks.
- `core/CMakeLists.txt`, `core/tests/test_profile.c`, and `core/tests/test_tracer.c` — test registration and expectations aligned with the admitted orders and error classification.
- `docs/spec/machine-contract.md` — host obligations and refusal effects.
- `.planning/phases/02-deterministic-machine-skeleton/02-02-red-evidence.json` and `.planning/phases/02-deterministic-machine-skeleton/02-02-task2-red-evidence.json` — portable RED evidence reports.

## Decisions Made

- Keep all supported order conversion inside the candidate path and commit only after allocation, normalization, header parsing, and hashing succeed.
- Keep header observation fixed at 64 bytes; undersized buffers report the required size without writing partial output.
- Treat callback descriptors as claims: check their reported extents and alignment, but document that true memory accessibility and actual backing capacity remain host responsibilities.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Kept regression expectations consistent with the admission contract**
- **Found during:** Task 1
- **Issue:** Existing profile and tracer expectations did not reflect the newly specified three supported raw orders and the invalid-ROM status for unknown signatures.
- **Fix:** Updated the profile policy digest expectation and tracer status assertions alongside the production behavior.
- **Files modified:** `core/tests/test_profile.c`, `core/tests/test_tracer.c`
- **Verification:** Focused profile and tracer tests passed.
- **Committed in:** `e54210a`

**Total deviations:** 1 auto-fixed (Rule 1).
**Impact on plan:** The updates align existing tests with the planned production contract.

## Verification

- `cmake --build --preset gate` — passed.
- `ctest --preset gate --no-tests=error -R '^unit\\.core\\.(lifecycle|rom-admission|tracer|profile)$' --output-on-failure` — 4/4 passed after the final source changes.
- `./scripts/gate.sh` — build and lint passed; CTest reported 64/65 passed. The only failure was `gov.pii.repo`, with six `P-PRIVATE-PATH` findings in previously committed RED evidence blobs.
- Current working versions of both evidence files use portable source labels and contain no private checkout paths. The gate scans commits, so changing current files alone does not resolve findings in earlier blobs.

## Issues Encountered

The full gate identified two historical evidence artifacts that still contain absolute checkout paths:

| Commit | Affected artifact |
|---|---|
| `87d9697` | `.planning/phases/02-deterministic-machine-skeleton/02-02-red-evidence.json` |
| `688b8da` | `.planning/phases/02-deterministic-machine-skeleton/02-02-task2-red-evidence.json` |

The current versions are sanitized and the focused code tests pass. Per the parent executor's instruction, history cleanup and the final full-gate rerun are pending at handoff; this summary remains `halted` until that gate passes. No history rewrite or waiver was made here.

## User Setup Required

None.

## Next Phase Readiness

The ROM/lifecycle implementation is ready for the next machine slice after the parent removes the two historical path findings and confirms the full gate. This work establishes current reset/refusal boundaries; future machine-state fields must be added to the same digest and reset audit. It does not establish DMA or scheduler correctness.

---
*Phase: 02-deterministic-machine-skeleton*
*Completed: 2026-11-10*

## Self-Check: PASSED

- Task source, test, and contract artifacts exist.
- All four task commits are ancestors of the current plan head.
- The only pending gate issue is the recorded historical PII scan failure.
