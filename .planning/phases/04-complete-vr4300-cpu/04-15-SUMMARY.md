---
phase: 04-complete-vr4300-cpu
plan: "15"
subsystem: cpu-diagnostic
tags: [vr4300, cpu-profile, diagnostics, rom-admission, loader]
requires:
  - phase: 04-13
    provides: VR4300 CPU execution and v3 profile
  - phase: 04-14
    provides: bounded diagnostic protocol and inventory contracts
provides:
  - Separate supervised local CPU diagnostic using owned immutable input, canonical core, capture, and shared protocol validation
  - Descriptor-v3-only 4 MiB flat-image admission and matching CPU-profile packer mode
  - Authored larger-image startup, stack, BSS, layout, identity, and refusal controls
affects: [04-16, 04-18, cpu-diagnostic, profile-admission]
actuals:
  tokens: 38415
  tasks: 2
  commits: 5
plan_head_before: 63bbf57ecfe9c9b98e9b3bffb4817abdb3a777b6
plan_head_after: effdccd4be40a7799cf031c7749aa4e3cf5466a7
tech-stack:
  added: []
  patterns:
    - Production and tests link one closed/versioned diagnostic protocol and aggregator
    - Profile-selected image limits preserve the warm-v1 boundary
key-files:
  created:
    - runner/t64_cpu_diagnostic.c
    - runner/tests/test_cpu_diagnostic.c
    - scripts/build-cpu-diagnostic.sh
    - .planning/phases/04-complete-vr4300-cpu/04-15-RED-evidence.md
  modified:
    - runner/t64_cpu_diag_protocol.c
    - runner/tests/test_cpu_diagnostic.c
    - core/t64_machine.c
    - tools/fixture/t64_rom_pack.c
    - fixtures/manifest.json
decisions:
  - Inventory v2 binds selector and selected-ROM digest to each registration value so selector-patched ROMs can differ while sharing a sealed inventory.
  - The new CPU profile alone uses the 0x00401000 image ceiling and checked 4 MiB physical range; warm-v1 remains capped at 0x00100000.
  - External tool and asset qualification remains with Plan16; this plan used only the authored guest and exact cleared IPL3 asset.
patterns-established:
  - Capture actual compact guest start/outcome/done records and validate them through the shared protocol module.
  - Keep ASan's production child wall bound unchanged and select only the oversized supervised positive control out of sanitizer builds.
requirements-completed: [VER-01, VER-03, CPU-09]
coverage:
  - id: D1
    description: Separate local CPU diagnostic validates real authored guest records through the canonical machine and shared aggregator.
    requirement: VER-01
    verification:
      - kind: integration
        ref: "ctest --preset gate --no-tests=error -R '^runner\\.cpu-diagnostic$'"
        status: pass
      - kind: unit
        ref: "scripts/check-test-inventory.sh --selftest; scripts/check-test-inventory.sh --preset gate"
        status: pass
    human_judgment: false
  - id: D2
    description: CPU-profile flat admission and packer enforce exact 4 MiB payload bounds while warm-v1 identities remain unchanged.
    requirement: CPU-09
    verification:
      - kind: integration
        ref: "sh scripts/build-cpu-diagnostic.sh --selftest && sh scripts/build-cpu-diagnostic.sh --qualify"
        status: pass
      - kind: unit
        ref: "ctest --preset gate --no-tests=error -R '^(runner\\.cpu-diagnostic|unit\\.core\\.boot-entry|unit\\.core\\.profile|unit\\.core\\.rom-admission)$'"
        status: pass
    human_judgment: false
duration: 108min
completed: 2026-11-12
status: complete
---

# Phase 4 Plan 15: Supervised CPU Diagnostic and 4 MiB Loader Summary

**A local CPU diagnostic now validates real guest records through the canonical core, and descriptor v3 alone admits the checked 4 MiB flat-image profile.**

## Performance

- **Duration:** About 1 hour 48 minutes from dispatch, including implementation, checks, and closeout.
- **Started:** 2026-11-12T17:06:03Z (approximate dispatch time).
- **Completed:** 2026-11-12T18:54:17Z.
- **Tasks:** 2.
- **Files modified:** 17.

## Accomplishments

- Built a separate `t64-cpu-diagnostic` path over verified owned-FD snapshots, bounded canonical-core advances, actual guest capture, and the shared closed/versioned protocol and aggregator. Guest start/outcome/done records and traced park milestones are validated as observed output.
- Updated the shared inventory/batch contract to bind each registration value to its own selector and selected-ROM digest. A native two-ROM positive control passes; swapped and unregistered bindings are rejected by the production consumer.
- Added the CPU-profile-only `0x00401000` image limit, exact `image_length - 0x1000` load length, aligned sign-extended KSEG0 entry, and checked `physical_entry + payload <= 0x00400000` rule. Descriptor/profile and v9 boot metadata goldens now include the policy change.
- Ran an authored image just above 1 MiB through the cleared IPL3 into the app. Before first output, it checks the prepared aligned stack, stack read/write, zero-filled BSS reservation, and linked code placement. Malformed order, excessive size, physical overflow, and changed IPL3 bytes are refused without changing the prior canonical state.
- Added `--cpu-profile` to the packer and an offline authored-only `scripts/build-cpu-diagnostic.sh` selftest/qualification path. Rebuilt all seven legacy hello images and verified their original hashes remain exact.
- Kept the warm-v1 1 MiB ceiling and public hello/collector boundaries unchanged. No external ROM, source, capture, tool installation, or hosted diagnostic was used.

## Automated Checks

- Focused gate controls: 4/4 passed (`runner.cpu-diagnostic`, `unit.core.boot-entry`, `unit.core.profile`, `unit.core.rom-admission`).
- Full gate: 111/111 passed, including fixture qualification, inventory/rights/provenance checks, and all gate tail checks.
- Full all-target ASan/UBSan build completed. The same four affected instrumented tests passed 4/4.
- Test inventory selftest passed with 111 gate and 113 ASan registrations; exact gate inventory passed.
- Core lint passed for 83 files with zero violations.
- `sh scripts/build-cpu-diagnostic.sh --selftest` passed 15 bounds checks; its authored-only `--qualify` packed a 4,280-byte guest image at entry `0x80000400`.
- The seven warm hello fixture hashes match the pre-change manifest exactly.

## Task Commits

1. **Task 1: Run one authored CPU batch through a separate supervised adapter** - `4dc6eb6` (RED), `f3baab6` (adapter routing), `0606873` (per-value ROM identity binding).
2. **Task 2: Qualify new-profile flat-loader, image bounds and startup seams** - `d917dff` (profile, packer, tests, script, and docs); `effdccd` (required legacy fixture recipe provenance refresh after verifying all seven ROM hashes).

## Files Created/Modified

- [RED evidence](04-15-RED-evidence.md#L1) - Portable actual-behavior RED records, including the first store's opcode, canonical PC/EPC, Cause, attempted address, and initial exact-boundary rejection.
- [CPU diagnostic executable](../../../runner/t64_cpu_diagnostic.c#L1) and shared protocol files - Supervised owned-input adapter and production protocol consumer.
- [CPU diagnostic tests](../../../runner/tests/test_cpu_diagnostic.c#L253) - Real guest records, distinct member bindings, above-warm-limit IPL3 startup, stack/BSS/layout checks, and transactional malformed-input controls.
- [Machine profile admission](../../../core/t64_machine.c#L81) - Descriptor-v3-only loader bound and profile-selected flat-image validation.
- [CPU-profile packer](../../../tools/fixture/t64_rom_pack.c#L21) and [offline qualification script](../../../scripts/build-cpu-diagnostic.sh#L1) - Separate CPU-profile packing bounds without changing warm defaults.
- [CPU profile contract](../../../docs/spec/cpu-profile.md#L127) and [fixture manifest](../../../fixtures/manifest.json#L100) - New loader policy and refreshed recipe source identity; existing asset and ROM hashes remain unchanged.

## Decisions Made

- Each sealed inventory member owns its selector and selected-ROM hash; the shared inventory identity does not assume every patched value uses one ROM.
- The descriptor-v3 CPU profile owns the larger loader policy. Warm-v1 keeps its 1 MiB limit and its prior fixture identities.
- `--qualify` covers only the authored local loader path. External source/tool/asset audit and private qualification remain for Plan16, and public native-source verification remains for Plan18.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Refreshed the legacy fixture recipe source pin after the packer changed.**

- **Found during:** Task 2 final gate.
- **Issue:** Fixture qualification correctly rejected the old recipe pin because it covered the previous packer bytes.
- **Fix:** Updated the hello records' source snapshot to the Task2 source commit and the recipe content hash, then rebuilt and compared all seven existing ROM hashes before running qualification again.
- **Files modified:** `fixtures/manifest.json`.
- **Verification:** Full fixture qualification and the full gate passed; every legacy ROM SHA-256 remained identical.
- **Committed in:** `effdccd`.

**2. [Test-scope adjustment] Retained the oversized supervised positive control in the normal gate and selected it out only under ASan.**

- **Found during:** Task 2 ASan checks.
- **Issue:** The 1 MiB-plus-4-byte supervised guest exceeded the unchanged production child wall bound under ASan instrumentation and returned `child-or-wall-bound`.
- **Fix:** Kept the large executable positive control in the normal gate. ASan still runs the large in-process IPL3 startup and the supervised small guest; no production wall cap or sanitizer limits changed. The failed attempt is retained privately and is not reported as a pass.
- **Verification:** Normal gate positive control passed; final affected ASan selection passed 4/4.

**3. [Integration contract] Replaced inventory-global ROM identity with member bindings.**

The original v1 contract could not aggregate distinct selector-patched ROMs under one sealed inventory. Inventory and batch v2 retain common source/build/profile identity while resolving each member's selector and selected-ROM digest. The production two-ROM positive and swapped/unregistered negative controls pass. Actual guest records use the closed `t64.cpu-run` v1 decoder; an AB-only device probe cannot establish a completed batch. Source membership is sealed before building and built-ROM bindings afterward, as recorded in the Plan16 addendum.

**Total deviations:** 3.
**Impact on plan:** One provenance record was updated because the packer itself is part of the legacy fixture recipe; one instrumented test selection avoids claiming that an unchanged bounded child completed under a slower sanitizer build.

## Issues Encountered

- The first exact CPU-profile image test reached the intended admission assertion and returned `T64_STATUS_INVALID_ROM` for image `0x00401000`, payload `0x00400000`, and entry `0x80000000`. The portable observation is recorded in [RED evidence](04-15-RED-evidence.md#L33); admission now succeeds at that exact boundary.
- One early allocator setup had an alignment below `posix_memalign`'s minimum and was excluded from RED evidence. The corrected test harness then reached and recorded the intended guest behavior.
- The first full gate stopped at the expected stale fixture recipe pin; after the scoped metadata repair, the full gate passed.

## User Setup Required

None. No external tools or assets were installed or fetched.

## Next Phase Readiness

Execute Plan18 next for public native verification and configured reviews. Plan16 follows with the external pre-use audit and private qualification. The public hello allowlist, seven fixture identities, and collector remain closed to external diagnostic input.

## Self-Check: PASSED

- Summary file exists and has no trailing whitespace.
- All five measured Plan15 commits are ancestors of the verified source tip.

---
*Phase: 04-complete-vr4300-cpu*
*Completed: 2026-11-12*
