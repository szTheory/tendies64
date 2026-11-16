---
phase: 03-first-headless-rom-through-open-ipl3
plan: "06"
subsystem: emulator-output
tags: [n64, isviewer, deterministic-state, cpu-bus]
requires:
  - phase: 03-05
    provides: bounded pinned IPL3 execution through authored app entry
provides:
  - Profile-scoped libdragon ISViewer software compatibility and bounded byte capture
  - Actual authored hello output and guest overflow refusal evidence
  - Warm-profile state-v7 serialization with preserved state-v6 meaning
affects: [Phase 3 headless runner, output ownership, boot evidence]
actuals:
  tokens: 20443
  tasks: 1
  commits: 2
plan_head_before: e2072595a1fb2e38192759fca6af76df440191a1
plan_head_after: a9d68808dbe75fb6b1ef0dae4f4915e28ff1dfd9
commits: 2
tech-stack:
  added: []
  patterns: [profile-scoped memory-mapped device, explicit versioned state encoding]
key-files:
  created:
    - core/t64_isviewer.c
    - core/tests/test_isviewer.c
    - .planning/phases/03-first-headless-rom-through-open-ipl3/03-06-isviewer-red.json
    - .planning/phases/03-first-headless-rom-through-open-ipl3/03-06-sh-red.json
  modified:
    - core/t64_internal.h
    - core/t64_bus.c
    - core/t64_cpu.c
    - core/t64_digest.c
    - core/t64_machine.c
    - core/CMakeLists.txt
    - core/tests/test_boot_entry.c
    - core/tests/test_tracer.c
    - core/tests/test_warm_devices.c
    - scripts/check-test-inventory.sh
    - fixtures/boot-contract.json
    - docs/spec/boot-limits.md
key-decisions:
  - "Model the pinned libdragon magic register as writable storage with per-instance readback, not a fixed hardware value."
  - "Use warm-profile state serialization v7 for all ISViewer state while preserving the prior v6 encoding and legacy synthetic bytes."
  - "Aligned CPU halfword loads and stores use one width-2 bus transaction; width-8 and partial-store behavior stays bounded."
patterns-established:
  - "Guest output remains deterministic per-machine state with no host I/O in advance."
  - "A bounded advance yield continues only with verified monotonic guest-time progress and a fixed overall tick limit."
requirements-completed: []
requirements-pending: [BOOT-04, RUN-05]
coverage:
  - id: D1
    description: "Execute the pinned IPL3 and authored hello guest through exact 30-byte ISViewer capture, preserving the unsupported BREAK stop."
    verification:
      - kind: integration
        ref: "unit.core.isviewer#actual hello captures the independent exact 30-byte transcript"
        status: pass
    human_judgment: false
  - id: D2
    description: "Refuse the actual guest's 4097th output byte after a full 4096-byte capture without partial append."
    verification:
      - kind: integration
        ref: "unit.core.isviewer#actual guest fills 4096 bytes then cannot append its 4097th byte"
        status: pass
    human_judgment: false
duration: 45min
completed: 2026-11-11
status: complete
---

# Phase 3 Plan 6: Bounded ISViewer Guest Output Summary

**Profile-scoped ISViewer capture now records exact authored guest output and overflow state, with independently checked state-v7 identity.**

## Created Paths

- `core/t64_isviewer.c` implements the fixed-address, profile-scoped ISViewer software device.
- `core/tests/test_isviewer.c` covers register lanes, exact byte capture, per-machine isolation, reset, digest mutations, and real guest output.
- `.planning/phases/03-first-headless-rom-through-open-ipl3/03-06-isviewer-red.json` records the valid magic-register RED.
- `.planning/phases/03-first-headless-rom-through-open-ipl3/03-06-sh-red.json` records the actual guest SH RED and the missing width-2 bus transaction.

## Performance

- **Duration:** 45 minutes
- **Started:** 2026-11-11T10:04:36Z
- **Completed:** 2026-11-11T10:50:19Z
- **Tasks:** 1
- **Files modified:** 16

## Accomplishments

- Implemented the libdragon software contract at physical `0x13ff0000`: writable per-instance magic readback, 512-byte big-endian buffer, word-count flush at `+0x14`, high-halfword update without flush, and low-halfword flush at `+0x16`.
- Flushes append exactly 0, 1, 3, 4, or 512 requested bytes, exclude word padding, and allow repeat appends. A 513-byte request or a request beyond the 4096-byte lifetime capture sets sticky loss and refuses the entire append before copying.
- Corrected aligned CPU scalar halfword access so `LH`, `LHU`, and `SH` reach the bus as width-2 operations. The real hello RED showed `SH` at PC `0x80000474` (`0xa50e0016`) had been split into byte writes and refused; the corrected path preserves the single count-lane transaction.
- Executed the pinned loader and authored hello guest to capture the exact 30-byte transcript `Tendies64 hello via open IPL3\n` (SHA-256 `86594505bea53c9843f9eaf07e4cf0121cbe0055670324674fd8385c88ffd18e`). The guest still stops at unsupported `BREAK` at tick 530438, PC `0x80000478`, opcode `0x0000000d`; captured output does not imply guest completion.
- Executed the authored overflow guest: it fills all 4096 capture bytes, then refuses byte 4097 with sticky loss; its unsupported boundary remains explicit.
- Versioned the warm profile to state 7. The v7 digest includes the magic register, pending count, full buffer, captured length and bytes, and sticky loss. An independent test encoder reproduces the prior v6 policy/profile/state identities before checking the current v7 profile and initial-state hashes. Legacy descriptor-1/state-5 synthetic identities remain unchanged.
- Kept the existing warm-device integration check bounded at tick 530180, immediately before the first output store, while retaining DMA, bank, queue, copy, isolation, and trace assertions.

## Identity Values

The active boot-policy digest is `c64cfcfdbd3ccdd08fdafbc63539af5b8026caeb70fad173bee933a93e96b141`; the v7 profile descriptor digest is `77049094be14d6fcd1f9321ef19ea01726a11c803e0f350a77ff0e6a24b40837`; and the admitted-source initial-state digest is `9c22e908bec3f08ce063f88e760ca61c97c5656766e46d7514ad44f0151e71b6`.

The test-only reference encoder also reproduced the preserved v6 policy/profile/state digests: `831a0a58ef8f82bc54f880ad5a1259bddf1b3d2e21680ac6637ea5994705a7fe`, `5775f19766dc0ae070178dc87f2e6b0f11a9e5dc587fa380588f3b82b7d768d2`, and `a3820a5bf34b713b5903e42f455a1c579a57799a48411d3d33ed59786c3e93bf`.

## TDD Gate Compliance

- **RED:** Commit `64560f6` added a behavioral magic-register test. A second valid RED, recorded in `03-06-sh-red.json`, executed the actual guest and failed the named exact-transcript assertion at the real SH PC/opcode with no count-lane flush or partial output. `gsd_run check tdd-red-evidence` returned `RED_EVIDENCE_OK` for that semantic assertion.
- **GREEN:** Commit `a9d6880` implements the mapped device and width-2 CPU bus access. The registered ISViewer test now passes all 93 assertions, including actual hello capture and actual overflow refusal.
- **REFACTOR:** No separate refactor commit was needed; the focused and integrated checks passed on the implemented structure.

## Automated Checks

- `ctest --preset gate --no-tests=error`: **78/78 passed**.
- `ctest --preset gate --no-tests=error -R '^unit\.core\.(isviewer|boot-entry|tracer|warm-device-policy|cpu-path|bus)$' --output-on-failure`: **6/6 passed**.
- `scripts/check-test-inventory.sh --preset gate`: **78 tests**; `--selftest`: gate and ASan removal controls plus malformed-inventory controls passed. The exact ASan inventory is **80 tests**.
- `sh tools/lint/core-lint.sh`: **18 files, 0 violations**.
- Fresh ASan/UBSan build and focused CTest run: **8/8 passed** for ISViewer, boot-entry, tracer, warm-device policy, CPU path, CP0/cache, bus, and digest/trace. A final tracer rerun after updating its v7 policy expectation also passed. The compiler emitted `-Wframe-larger-than=2048` warnings in several core routines; there were no sanitizer runtime errors.
- `ctest --preset gate --no-tests=error -R '^repo\.docs$' --output-on-failure`, the current fixture JSON/policy/transcript hash checks, and `git diff --check` passed.

## Files Created/Modified

- `core/t64_isviewer.c`, `core/t64_internal.h`, `core/t64_bus.c` — add the bounded device and route supported widths through the existing bus.
- `core/t64_cpu.c` — preserve aligned width-2 scalar bus operations.
- `core/t64_digest.c`, `core/t64_machine.c` — add explicit warm state-v7 identity and policy selection while retaining v6 behavior.
- `core/tests/test_isviewer.c`, `core/tests/test_boot_entry.c`, `core/tests/test_tracer.c`, `core/tests/test_warm_devices.c` — exercise exact output, identities, width behavior, and the bounded prior smoke point.
- `core/CMakeLists.txt`, `scripts/check-test-inventory.sh` — register the new test in gate and ASan inventories.
- `fixtures/boot-contract.json`, `docs/spec/boot-limits.md` — record the active policy, independently derived identities, actual guest observations, and claim limits.
- `.planning/phases/03-first-headless-rom-through-open-ipl3/03-06-isviewer-red.json`, `.planning/phases/03-first-headless-rom-through-open-ipl3/03-06-sh-red.json` — retain portable semantic RED evidence.

## Decisions Made

- Model the magic register as storage that accepts guest writes and returns the stored value; the pinned libdragon code performs two different write/readback checks.
- Serialize all future-affecting output state under warm state version 7. Do not reinterpret v6 or synthetic v5 bytes.
- Keep capture internal and deterministic, with no host file I/O, callback, clock, or public copy/drain ABI in this slice.

## Deviations from Plan

### Authorized contract corrections and auto-fixes

**1. [Rule 1 - Bug] Preserve scalar halfword bus transactions**
- **Found during:** Task 1, actual guest RED
- **Issue:** CPU scalar halfword operations were decomposed into byte bus operations, so the valid low-halfword count-lane `SH` could not reach the device as one operation.
- **Fix:** Route aligned width-2 scalar reads and writes as one bus transaction, preserving width-8 and partial-store behavior.
- **Files modified:** `core/t64_cpu.c`
- **Verification:** Actual hello capture plus CPU path, bus, ISViewer, and ASan/UBSan regressions passed.
- **Committed in:** `a9d6880`

**2. [Authorized state correction] Version the warm output state**
- **Found during:** Task 1, state ownership review
- **Issue:** The previous warm output state contained only policy/readiness booleans and could not represent magic, pending buffer/count, capture, or sticky loss.
- **Fix:** Select warm state version 7 and add fixed-order serialization; keep v6 and legacy synthetic encodings intact, and bind all active device limits/lanes/reset behavior to the boot-policy digest.
- **Files modified:** `core/t64_internal.h`, `core/t64_digest.c`, `core/t64_machine.c`, `fixtures/boot-contract.json`, `core/tests/test_boot_entry.c`, `core/tests/test_isviewer.c`, `core/tests/test_tracer.c`, `docs/spec/boot-limits.md`
- **Verification:** Independent reference encoding reproduces old v6 identities and matches the new v7 profile/state digests; focused and full gate tests passed.
- **Committed in:** `a9d6880`

**Total deviations:** 2. Both corrections were authorized for this slice and were limited to requirements of the actual mapped output contract.

## Issues Encountered

One direct invocation of the warm-device binary omitted its registered `T64_TEST_HELLO_PATH` environment and failed fixture admission. The registered CTest environment passed; no product change was needed.

## User Setup Required

None.

## Next Phase Readiness

The exact guest-output and overflow behavior is locally verified. `BOOT-04` and `RUN-05` remain pending for the public output consumer/headless runner proof; this plan does not claim that API/ABI or a guest pass. No hosted CI, push, cross-host run, authentic IPL2 handoff, or hardware-accuracy claim was made.

---
*Phase: 03-first-headless-rom-through-open-ipl3*
*Completed: 2026-11-11*

## Self-Check: PASSED

The summary and both portable RED evidence files exist. RED commit `64560f6` and GREEN commit `a9d6880` are ancestors of the current plan head.

