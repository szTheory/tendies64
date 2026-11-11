---
phase: 03-first-headless-rom-through-open-ipl3
plan: "03"
subsystem: boot
tags: [mips, open-ipl3, warm-entry, cmake, ctest, binutils]
requires:
  - phase: 03-02
    provides: Qualified IPL3 fixture, authored payload and ROM identities, and pinned source-built Binutils
provides:
  - Registered prepared-RDRAM warm-entry test with independent profile and v6 state goldens
  - Written scope limits for the first supported IPL3 instruction and unsupported-opcode stop
  - Incremental fixture producer wired into the canonical local and hosted gate
affects: [03-04, 03-06, 03-11, boot-fixtures]
actuals:
  tokens: 13162
  tasks: 1
  commits: 6
plan_head_before: 4f32c6079a6a5ff6d5124d2c34a49ea7f3e5b53f
plan_head_after: 0445a566809b4c35b4a573157cd6bb97c677b194
tech-stack:
  added: []
  patterns:
    - Explicit CMake fixture-producer dependency shared by all consumers without recursive Ninja builds
    - Host-specific qualified tool hashes are computed and checked in ignored local receipts
    - Independent test goldens follow the written field order rather than production serialization output
key-files:
  created:
    - core/tests/test_boot_entry.c
    - docs/spec/boot-limits.md
    - .planning/phases/03-first-headless-rom-through-open-ipl3/03-03-task1-red.json
    - .planning/phases/03-first-headless-rom-through-open-ipl3/03-03-task1-green.json
  modified:
    - core/CMakeLists.txt
    - scripts/gate.sh
    - scripts/check-test-inventory.sh
    - scripts/build-boot-fixtures.sh
    - fixtures/boot-contract.json
    - fixtures/manifest.json
    - docs/ci-reviews/LOG.md
key-decisions:
  - "The prepared-RDRAM descriptor 2 and machine state version 6 identify a software policy; they do not prove a hardware IPL2 handoff."
  - "Keep per-host tool binary SHA-256 checks in ignored receipts; do not pin Darwin binaries to hashes that change with build paths and toolchain details."
  - "Only hosted Ubuntu CI may install the single zlib1g-dev header prerequisite; local gates fail with an actionable message and never run sudo or apt."
patterns-established:
  - "Retire ANDI at guest tick 1, then stop explicitly at the unsupported LUI at tick 2."
  - "Refresh fixture source-content and revision pins when the authorized producer changes."
requirements-completed: [BOOT-02]
coverage:
  - id: D1
    description: "Prepared-RDRAM warm-entry software smoke checks and explicit limits; full authored-ROM execution remains later work."
    requirement: BOOT-01
    verification:
      - kind: integration
        ref: "sh scripts/gate.sh; unit.core.boot-entry"
        status: pass
    human_judgment: true
    rationale: "The tested path retires one IPL3 instruction and then halts on an unsupported opcode; it does not demonstrate the full HLE-PIF/open-IPL3 flow required by BOOT-01."
duration: 52min
completed: 2026-11-11
status: complete
---

# Phase 3 Plan 03: Inspectable Warm Entry Summary

**The canonical gate now verifies the bounded warm-entry contract and provisions its qualified fixture producer on both hosted platforms.**

## Performance

- **Duration:** 52 minutes, measured from the first persisted task artifact.
- **Started:** 2026-11-10T23:43:48-05:00
- **Completed:** 2026-11-11T00:35:31-05:00
- **Tasks:** 1
- **Files modified:** 14, including evidence, CI cost records and sequential planning state.

## Accomplishments

- Added `unit.core.boot-entry` to the exact gate and ASan inventories. It checks copied DMEM, numeric entry and seed fields, descriptor/state identity, refusal cases, one real instruction retirement, and independently derived profile and version-6 state goldens.
- Documented that descriptor 2 and state version 6 are explicit software policy. The smoke path retires `ANDI a0,v0,0xd236` at `0xA4000040` on guest tick 1, then stops at unsupported `LUI` at `0xA4000044` on tick 2. This does not execute the authored hello ROM or establish a hardware handoff.
- Added the incremental `t64-boot-fixtures` producer to the CMake graph. All consuming boot targets, including the existing tracer, depend on it and receive fixed artifact/input paths without recursively invoking the same Ninja build.
- Preserved all prior registrations: the gate inventory is 74 tests, the ASan inventory is 76, and inventory selftests reject all 148 individual removals and five malformed controls.
- Recorded local and hosted evidence against source SHA `0445a566809b4c35b4a573157cd6bb97c677b194`. Both existing hosted jobs passed.

## Independent Identities

- Profile: `f67e9fb4ad2c065e9ec4dc3048f447a8755255a281c602d2ac8c6cc5bcf38faa`
- Initial state v6: `21217cbc347896be9e97f7edcddd912a2fe4c9b0f151dd2e3809887477b6b42d`
- Source asset: `f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068`
- Copied payload: `f716a90b1d36ec4253d096790791256d3543455e54473a755ff8c3d1199b15d5`
- Boot policy: `a6eeba466ae8804a8dec79ce8c5dece40ea5d25b29724d53f7e1ebe46ffd5cc9`

The test goldens were derived from the documented field order, independently of production serialization. The 4096-byte source asset, 4032-byte copied payload and 4280-byte authored ROM remain separate identities; the authored header load-size field is 184 bytes.

## Task Commits

1. **RED:** `bdd397f` — add the failing boot-entry inventory identity and evidence.
2. **GREEN:** `325dfbd` — implement warm-entry assertions, documentation, CMake producer and gate integration.
3. `34be928` — correct qualified source-tool installation, host-specific receipt checks and bounded diagnostics.
4. `e3ca6ea` — refresh fixture source metadata for the builder change.
5. `7bcfb8b` — add narrowly guarded hosted Linux zlib-header provisioning.
6. `0445a56` — bind manifest source hashes and revisions to the final builder.

## Automated Checks

- TDD RED was accepted as `RED_EVIDENCE_OK`: the inventory command failed specifically because `unit.core.boot-entry` was absent. The direct TAP wrapper reported the expected assertion failure, not a discovery or compile error.
- Focused boot-entry and tracer CTests passed 2/2; the gate inventory selftest passed with 148 removal and five malformed controls. Inventory counts were 74 gate and 76 ASan registrations.
- `scripts/build-boot-fixtures.sh --qualify` passed with the current source pins and reused healthy locally qualified tools.
- Final `sh scripts/gate.sh` passed in 24 seconds on macOS arm64: fixture qualification, CMake configure/build, all 74 CTests, inventory checks and core lint (15 files, zero violations).
- `scripts/rights-scan.sh --tree --history` passed with zero findings (463 tree items, 879 history items). `scripts/pii-scan.sh --tree --history` passed with zero findings (463 files, 204 commits).
- Hosted push run `42966035712`, attempt 1, completed successfully for the exact source SHA above. `gate-linux` passed in 98 seconds (2 seconds queued); `gate-macos` passed in 302 seconds (3 seconds queued). Wall time was 306 seconds and the critical path was 305 seconds. The measured row is recorded in `docs/ci-reviews/LOG.md`.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking build issue] Install only the Binutils components that were built.**

- **Found during:** First hosted qualification, run `42962237106`.
- **Issue:** Generic `make install` tried to enter the unbuilt `etc` directory after the recipe built only binutils, gas and ld.
- **Fix:** Use `make install-binutils install-gas install-ld` after those matching build targets.
- **Files modified:** `scripts/build-boot-fixtures.sh`.
- **Verification:** Clean Linux and macOS source-build reproductions, final fixture qualification, and the successful exact-SHA hosted run.

**2. [Rule 3 - Missing host prerequisite] Provision zlib headers only inside hosted Ubuntu CI.**

- **Found during:** First hosted qualification, run `42962237106`.
- **Issue:** Ubuntu 24.04 lacked `zlib.h` while the pinned Binutils recipe selected system zlib.
- **Fix:** The builder installs only `zlib1g-dev` when `CI=true`, `GITHUB_ACTIONS=true`, and the host is Linux. A local missing-header check exits with an actionable prerequisite message and never invokes `sudo` or `apt`.
- **Files modified:** `scripts/build-boot-fixtures.sh`, `fixtures/boot-contract.json`, and `fixtures/manifest.json`.
- **Verification:** Reproduced the clean Linux build and ran the final full hosted gate. The workflow remains exactly two jobs with one canonical gate command per job.

**3. [Rule 1 - Incorrect host identity pin] Preserve computed Darwin tool hashes instead of fixed constants.**

- **Found during:** Clean macOS source-build reproduction.
- **Issue:** Newly built tool hashes vary with build path and compiler/SDK details, so fixed Darwin binary hashes rejected a valid source-built toolchain.
- **Fix:** Keep computing and checking the actual per-host SHA-256 values in the ignored qualification receipt; remove only the invalid fixed Darwin constants. Update source-content and revision pins to the resulting builder.
- **Files modified:** `scripts/build-boot-fixtures.sh`, `fixtures/manifest.json`.
- **Verification:** macOS qualification and the exact-SHA hosted macOS gate passed.

The first hosted attempt (`42962237106`) failed both jobs during tool qualification, before CMake. Its bounded, sanitized diagnostic finding is recorded as F-025. The corrected candidate passed both existing jobs; no extra lane or workflow command was added.

## Threat Flags

| Flag | File | Description |
|------|------|-------------|
| threat_flag: supply_chain | `scripts/build-boot-fixtures.sh` | Hosted Ubuntu qualification installs only the distribution `zlib1g-dev` prerequisite behind explicit GitHub Actions/Linux guards; local gates do not run package provisioning. |

## Scope Limits

This proves a prepared-RDRAM software warm-entry smoke only. Descriptor 1/state 5 and v1 synthetic goldens remain unchanged. No authored hello execution, expanded payload/header admission, hardware handoff, cold boot, PIF challenge/response, or hardware-accuracy claim is established. Later CPU/warm integration owns full payload/header admission and policy identity.

## User Setup Required

None. Local qualification requires the documented host tools, `jq`, `curl`, and source-build prerequisites; if the system zlib header is missing, the builder names the needed package and exits without installing it. Hosted Ubuntu provisioning is restricted to the workflow environment.

## Next Phase Readiness

The registered smoke and gate chain are ready for the next planned integration slice. BOOT-01, BOOT-03, BOOT-04, RUN-01/03/05 and PROF-05 remain pending; this partial entry test does not satisfy them. BOOT-02 was already complete from the pinned fixture plan.

## Self-Check: PASSED

- Summary and final green evidence files exist and parse.
- All six task commits are ancestors of the plan head.
- Exact-SHA hosted run `42966035712` is recorded successful in the green evidence.
