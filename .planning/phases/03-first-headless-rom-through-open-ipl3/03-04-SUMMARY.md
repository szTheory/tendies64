---
phase: 03-first-headless-rom-through-open-ipl3
plan: "04"
subsystem: cpu-boot
tags: [mips-iii, vr4300, cp0, cache, kseg, c17, ctest]
requires:
  - phase: 03-first-headless-rom-through-open-ipl3
    provides: Pinned open IPL3 fixture, warm-entry profile, and qualified host gate from plans 03-01 through 03-03
provides:
  - Shared production CPU execution path for the qualified integer, control, delay-slot, load/store, and KSEG subset
  - Bounded CP0 and coherent-functional cache inventory behavior with explicit unsupported stops
  - Current boot policy identity and independently derived profile/state identity vectors
affects: [phase-03-following-plans, cpu, boot, profile-identity, determinism]
actuals:
  tokens: 22569
  tasks: 2
  commits: 6
commits: 6
plan_head_before: 825a53cbe044b6817f02b5dacae522e133d1b2a5
plan_head_after: d10fbfa126d3b1a28bf7295b2d6af92c2973785e
tech-stack:
  added: []
  patterns:
    - Production tracing and CPU path tests use the same execution functions
    - Unsupported instructions and address modes stop with PC/opcode and do not retire
    - Profile and initial-state goldens are independently serialized from documented field order and pinned bytes
key-files:
  created:
    - core/t64_cpu.c
    - core/tests/test_cpu_path.c
  modified:
    - core/t64_internal.h
    - core/t64_machine.c
    - core/CMakeLists.txt
    - core/tests/test_tracer.c
    - core/tests/test_boot_entry.c
    - scripts/check-test-inventory.sh
    - docs/spec/boot-limits.md
    - fixtures/boot-contract.json
key-decisions:
  - "Count advances once per successful instruction retirement as deterministic software policy; it is not hardware cycle timing."
  - "CACHE behavior is a coherent-functional warm subset over the pinned inventory, with no general timing or cache-fidelity claim."
  - "Update actively hashed CPU/cache policy values when executed behavior changes; the policy digest distinguishes the new descriptor and state without changing profile ID or serialization versions."
patterns-established:
  - "Test current dynamic IPL3 coverage separately from later static inventory candidates."
  - "Keep all unsupported guest operations observable and non-retiring."
requirements-completed: []
coverage:
  - id: D1
    description: "The pinned warm-entry path executes the supported integer/control/memory slice and stops at the unsupported MI_VERSION read."
    verification:
      - kind: unit
        ref: "core/tests/test_cpu_path.c#--cpu-path (8 cases)"
        status: pass
      - kind: integration
        ref: "unit.core.tracer and unit.core.boot-entry"
        status: pass
    human_judgment: false
  - id: D2
    description: "The pinned CP0 and CACHE candidate subset has explicit masks, state effects, unsupported behavior, and v6 digest coverage."
    verification:
      - kind: unit
        ref: "core/tests/test_cpu_path.c#--cp0-cache (6 cases)"
        status: pass
      - kind: unit
        ref: "unit.core.digest-trace"
        status: pass
    human_judgment: false
  - id: D3
    description: "The warm-entry policy and its current dynamic stop boundary are documented with separate historical and current observations."
    verification:
      - kind: other
        ref: "boot-contract JSON validation, independent policy digest check, and repo.docs"
        status: pass
    human_judgment: false
duration: 73min
completed: 2026-11-11
status: complete
---

# Phase 3 Plan 04: Qualified CPU Path Summary

**Extracted production CPU execution into `core/t64_cpu.c` and added independent checks in `core/tests/test_cpu_path.c` for the qualified IPL3 integer path and bounded CP0/cache policy.**

## Performance

- **Duration:** 73 min from the first RED commit to summary authoring
- **Started:** 2026-11-11T05:51:29Z
- **Completed:** 2026-11-11T07:04:32Z
- **Tasks:** 2
- **Files changed:** 10

## Accomplishments

- The production tracer and machine scheduler now use the extracted CPU path for the qualified integer/control, delay-slot, load/store, and KSEG0/KSEG1 behavior. The actual pinned IPL3 retires 37 instructions and stops at tick 38 on `LW s1,4(v0)` (`0x8C510004`) at PC `0xA4000080`, the unsupported MI_VERSION read at `0xA4300004`.
- Added independent instruction snippets for MIPS III sign extension, shifts, 64-bit arithmetic, signed `MULT`/HI/LO, branch/link delay slots, endian and partial loads/stores, alignment, virtual aliases, overflow, and explicit unsupported stops.
- Implemented the pinned CP0 inventory and the named coherent-functional cache subset. CP0/cache state remains in the v6 digest, and unknown registers, selects, encodings, and address modes stop without retirement.
- Corrected the actively hashed CPU/cache policy labels and rebound their SHA-256. The descriptor v2 and initial state v6 goldens were independently recalculated; profile ID, descriptor/state layout versions, and v1 synthetic identity remain unchanged.

## TDD Evidence

All three RED records passed `gsd-tools.cjs check tdd-red-evidence` with `RED_EVIDENCE_OK`, and semantic inspection confirmed each target failed on the planned unsupported behavior:

- Task 1: `build/tdd-red/03-04-task1.json` — the target LUI assertion failed because execution stopped at PC `0xA4000044` after the entry ANDI, before the expected LUI retirement.
- Task 2 CP0: `build/tdd-red/03-04-task2.json` — the Count assertion failed at the test's MFC0 opcode because MFC0 was unsupported and no instruction retired.
- Task 2 signed MULT: `build/tdd-red/03-04-task2-mult.json` — actual execution of the BAL helper exposed a reached signed MULT opcode that stopped without retirement; the independent HI/LO assertion failed as intended.

The GREEN checks passed after implementation. No additional refactor was needed.

## Pinned Inventory Coverage

| Inventory row | Pinned PC/opcode | Evidence and current reachability |
|---|---|---|
| Entry and qualified integer/control/load/store/KSEG path | Entry `0xA4000040` (`ANDI 0x3044D236`); current stop `0xA4000080` (`LW 0x8C510004`) | The real tracer reaches the unsupported device read after 37 retirements. Eight independent CPU-path cases cover sign extension, shifts, signed multiply, branch/link delay slots, memory lanes/endian, KSEG aliases, alignment, overflow, and unsupported stops. |
| MFC0 Count and MTC0 Cause/Count/Compare/WatchLo | `0xA4000064`; `0xA400006C`–`0xA4000078` | These CP0 accesses are dynamically reached before the MI_VERSION stop. The six CP0/cache cases independently check Count/Compare progression, Cause and WatchLo masks, Compare clear, negative encodings, and digest sensitivity. |
| MTC0 TagLo/TagHi | `0xA4000148` (`0x4080E000`); `0xA400014C` (`0x4080E800`) | Later static candidates; not reached by the current trace. CP0 mask tests exercise both fields. |
| CACHE index stores | op `0x9`, `0xA4000158`–`0xA4000164`; op `0x8`, `0xA4000180`–`0xA400018C` | Later static candidates; not reached by the current trace. The coherent-memory test exercises both indexed operations and records their canonical state. |
| CACHE hit writeback/invalidate | op `0x15`, `0xA40001D4` and `0xA40001DC` | Later static candidates; not reached by the current trace. The coherent-memory test exercises this operation. No per-line timing claim is made. |
| Unsupported MI_VERSION read | `0xA4000080`, `LW s1,4(v0)` (`0x8C510004`) | Current explicit stop: tick 38, 37 retired instructions, unsupported instruction not retired. Tracer and boot-entry assertions preserve this exact boundary. |

The entry-only result from 03-01 remains preserved in `dynamically_observed_coverage_at_qualification`, with its 2026-11-10 snapshot label. The separate current observation records the 37-retirement trace. The later TagLo/TagHi/CACHE rows are static artifact candidates with independent snippet coverage, not claims of dynamic execution.

## Identity Update

The prior `entry-andi-exact-unsupported-stop-v1` and `unimplemented-stops-v1` strings were active inputs to the boot-policy digest even after the implementation had grown beyond those behaviors. Under the parent-approved bounded correction, the current policy labels are `integer-control-load-store-cp0-inventory-subset-v2` and `coherent-functional-warm-subset-v1`.

- Boot-policy SHA-256: `5d6aacdc47cbb6fb3ad80e782935fdb330af559ec8e9143656ffedd5069cf984`
- Profile descriptor v2 SHA-256: `d5a2b703c15ec8707e61a369530c524e6e88ef6d5548f0df2239a9a93a05c746`
- Initial state v6 SHA-256: `1c7e4086d89234c9d304e40f7e17468ddc8fcd04316904ddd7b5bc87913e96ea`

An independent generator followed the documented profile/state field order and pinned fixture bytes. It first reproduced the prior profile and state goldens exactly, then calculated the new values; it did not call production profile or state serialization to produce expected results. The existing descriptor v1, state v5, and v1 synthetic goldens were not changed. The fixture producer pins asset/toolchain properties rather than contract-file bytes, so no unrelated manifest or source pin changed.

## Requirement Status

- **BOOT-01 — partial:** guest execution begins on the pinned IPL3 and reaches 37 retired instructions, but stops at MI_VERSION before a completed IPL3 boot. It remains pending phase-level verification.
- **BOOT-03 — supported, pending phase-level verification:** boot-limits records the unmodeled PIF challenge/response, CIC variants, and timeout behavior, while retaining the explicit partial-boot boundary.
- **PROF-05 — partial, pending:** local deterministic CPU, CP0, cache, and digest checks passed. This changed candidate has not run the hosted Linux/GCC and macOS/Clang matrix; the parent owns the next coherent hosted candidate.

No requirement checkbox was marked complete by this bounded plan.

## Task Commits

1. Task 1 RED — `278e30a` (`test(03-04): add failing CPU-path integration assertion`)
2. Task 1 GREEN — `76381ca` (`feat(03-04): execute qualified integer CPU path`)
3. Task 2 CP0 RED — `d324765` (`test(03-04): add CP0 and cache behavior cases`)
4. Additional reached-MULT RED — `8c1e5f7` (`test(03-04): cover reached signed multiply HI LO`)
5. Task 2 GREEN — `3559726` (`feat(03-04): execute bounded CP0 and cache path`)
6. Evidence clarification — `d10fbfa` (`docs(03-04): distinguish traced CP0 from static candidates`)

## Deviations from Plan

**1. Updated smoke stops after real CPU progress.** The earlier tracer and boot-entry checks expected unsupported LUI/CP0 boundaries. Once the production decoder advanced the actual path, those checks now assert the observed MI_VERSION stop at PC `0xA4000080`, tick 38, after 37 retirements. Admission, reset, asset/payload identity, and independent initial-state checks remain in place. The tracer capture buffer now holds the full 256-record capacity and asserts no dropped or saturated records.

**2. Corrected active policy identity.** The prior CPU/cache policy labels no longer described executed behavior but still contributed to the canonical profile and state identities. Updated the labels, digest constant, contract, expected profile/state literals, and boot-limits. This was explicitly authorized and bounded; no profile-ID or wire-version change was needed.

**3. Added a valid RED for signed MULT when the actual helper path exposed it.** The reached BAL helper used signed 32-bit MULT before Task 2 could complete. A dedicated independent HI/LO test failed at that opcode before support was added.

## Issues Encountered

The sanitizer build emitted visible frame-size warnings because ASan/UBSan instrumentation pushes functions over the configured 2048-byte diagnostic threshold. New CPU warnings were `t64_cpu_step` (2368 bytes) and `execute_special` (2256 bytes). Existing warnings were `t64_sp_complete` (2064), `decode_descriptor` (2608), `t64_machine_digest` (6368), `write_profile` (2384), and `prepare_openipl3_warm_entry` (2480). CMake keeps these warnings nonfatal only in the sanitizer preset; sanitizer tests passed, and normal core lint reported no violations. No unrelated warning cleanup was made.

## Next Phase Readiness

Phase 3 remains in progress. The current trace stops at the unsupported MI_VERSION device read, and the later static CP0/cache candidates are independently tested but not dynamically reached. Warm admission still accepts only the exact 4096-byte source asset; the authored 4280-byte ROM and its flat payload/header path remain future integration work. No hosted run or push was started for this CPU slice.

## Automated Checks

- `cmake --preset gate` and `cmake --build --preset gate` — passed; CPU and test targets rebuilt.
- Focused gate CTest selection for tracer, boot-entry, cpu-path, cp0-cache, and digest-trace — 5/5 passed.
- `ctest --preset gate --no-tests=error --output-on-failure` — all 76 tests passed.
- `sh scripts/check-test-inventory.sh --preset gate` — passed with the expected 76-test inventory.
- `cmake --preset asan`, sanitizer target build, and focused ASan/UBSan CTest selection — 5/5 passed with `ASAN_OPTIONS=detect_leaks=0`; address and undefined-behavior checks remained active.
- `sh tools/lint/core-lint.sh` — passed, 16 source/header files and zero violations.
- Boot-contract JSON parsing, final-LF policy digest recomputation, C policy-byte check, independent old/new profile/state vectors, and `git diff --check` — passed.

The whole local CTest suite and normal core lint were run separately; the bundled `scripts/gate.sh` command was not invoked. No hosted CI run or push was part of this slice.

## Self-Check: PASSED

- All ten changed implementation/evidence paths exist, including the first two created artifacts `core/t64_cpu.c` and `core/tests/test_cpu_path.c`.
- All six task/evidence commits listed above are ancestors of the current plan head `d10fbfa`.
- Human edits to `AGENTS.md` and `.planning/config.json` were not included in plan commits.
