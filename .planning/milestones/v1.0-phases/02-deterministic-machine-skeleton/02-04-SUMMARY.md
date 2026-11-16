---
phase: 02-deterministic-machine-skeleton
plan: "04"
subsystem: core-memory
tags: [physical-bus, RDRAM, SP, MI, digest, trace]
requires:
  - phase: 02-deterministic-machine-skeleton
    provides: deterministic machine instance, scheduler, tracing, and schema-v2 digest from plan 03
provides:
  - checked physical RDRAM, SP-bank, and read-only cartridge access
  - explicit unsupported/unmapped/refusal bus outcomes and MI pending/mask/IRQ state
  - schema-v3 canonical digest covering the new future-affecting state
affects: [phase-02 consumers, deterministic-core tests]
actuals:
  tokens: 15793
  tasks: 2
  commits: 3
tech-stack:
  added: []
  patterns: [per-instance checked bus access, explicit device refusal, separated pending and mask state, versioned canonical digest]
key-files:
  created: [core/t64_bus.c, core/t64_mi.c, core/tests/test_bus.c]
  modified: [core/include/tendies64/t64_machine.h, core/t64_internal.h, core/t64_machine.c, core/t64_digest.c, core/CMakeLists.txt, core/tests/test_profile.c, core/tests/test_digest_trace.c, docs/spec/machine-contract.md]
key-decisions:
  - "MI mask commands clear then set, so set wins when both command bits are present; IRQ is recomputed as pending AND mask."
  - "Adding SP and MI state advances the canonical serialization domain and profile version from v2 to v3."
patterns-established:
  - "Physical reads and writes preflight width, alignment, range, and device support before mutation; adjacent unsupported ranges never alias RAM."
  - "Bus and MI observations record guest ticks without changing machine progress."
requirements-completed: [BUS-01, CORE-02, CORE-06]
duration: "37 min"
completed: 2026-11-10
status: complete
plan_head_before: 4e9ca9f93490f9a02f87a57f895669518edf21ee
plan_head_after: b4cba7f2f036767c28b0e8dc5065290fa96f22d1
---

# Phase 2 Plan 4: Physical Bus and MI Summary

**Checked per-instance physical memory and MI access with explicit refusals, ticked observations, and a schema-v3 deterministic digest.**

## Performance

- **Duration:** Approximately 4 hours.
- **Started:** Exact start time was not recorded.
- **Completed:** 2026-11-10.
- **Tasks:** 2.
- **Files modified:** 11.

## Accomplishments

- Implemented half-open physical RAM/SP/cartridge regions with checked byte and aligned word access, bounded ROM reads, and explicit unsupported/unmapped/refusal statuses.
- Added six-bit MI pending and mask state, paired clear/set commands with set-wins behavior, and an explicit derived IRQ line.
- Extended reset, traces, and canonical digest coverage; tests exercise boundaries, refused-operation atomicity, MI commands, exact event ticks, and state mutation.

## Task Commits

1. **Task 1: Read and write checked physical memory with no alias fallback** — `7e11251` (`feat`).
2. **Task 2: Keep MI pending, masks and derived IRQ independently observable** — `f38bbc7` (`feat`).
3. **Digest compatibility correction** — `b4cba7f` (`fix`), versioning the newly extended canonical state as schema v3.

## Verification

- Focused bus, ROM-admission, scheduler, profile, and digest-trace tests passed.
- `./scripts/gate.sh` passed: 68/68 tests and production lint with 0 violations.
- Bus expectation comments point to the exact Decision B source or identify model policy; synthetic MI event tests do not claim CPU exception behavior.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing compatibility requirement] Versioned the canonical digest for the new state fields.**
- **Found during:** Full gate after Task 2.
- **Issue:** The existing v2 digest golden changed when SP and MI state were added; v2 had not reserved bytes for these fields.
- **Fix:** Advanced the serialization domain and profile version to v3 and updated expected profile/state digests from independently encoded documented bytes.
- **Files modified:** `core/t64_machine.c`, `core/t64_digest.c`, `core/tests/test_profile.c`, `core/tests/test_digest_trace.c`, `docs/spec/machine-contract.md`.
- **Verification:** Focused profile and digest-trace tests and the final full gate passed.
- **Committed in:** `b4cba7f`.

**Total deviations:** 1 correctness fix. The public machine header was also updated because the planned public per-instance bus calls required declarations there.

## Issues Encountered

The first full gate identified the expected v2 digest golden as stale after serialization changed. The schema version and independently derived expected values were updated, then focused and full verification passed.

## User Setup Required

None.

## Next Phase Readiness

Physical bus and MI skeleton behavior is available for later machine devices. CPU virtual translation, exception entry, open-bus behavior, and unimplemented MMIO remain outside this contract.

## Self-Check: PASSED

The three plan implementation commits are ancestors of the recorded plan head. Summary path and commit references were verified before the summary commit.

---
*Phase: 02-deterministic-machine-skeleton*
*Completed: 2026-11-10*
