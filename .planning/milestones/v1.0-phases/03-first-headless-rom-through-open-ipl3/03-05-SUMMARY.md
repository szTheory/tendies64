---
phase: 03-first-headless-rom-through-open-ipl3
plan: "05"
subsystem: boot
tags: [ipl3, warm-devices, pi-dma, sp-dma, cache, mips]
requires:
  - phase: 03-04
    provides: qualified CPU, CP0, CACHE subset and pinned live IPL3 path
provides:
  - profile-scoped warm reset, probe and SP/PI DMA path through authored app entry
  - independently pinned warm-policy, profile and initial-state identities
  - exact KUSEG first-output-instruction prerequisite for plan 03-06
affects: [03-06, phase-3-verification, boot-claims]
actuals:
  tokens: 35372
  tasks: 2
  commits: 5
tech-stack:
  added: []
  patterns:
    - exact profile-qualified device exceptions with transactional refusal
    - bounded trace drains and completion-time DMA byte checks
key-files:
  created:
    - core/t64_warm_devices.c
    - core/tests/test_warm_devices.c
    - .planning/phases/03-first-headless-rom-through-open-ipl3/03-05-task2-green.json
    - .planning/phases/03-first-headless-rom-through-open-ipl3/03-05-cache-green.json
  modified:
    - core/t64_sp.c
    - core/t64_internal.h
    - core/t64_cpu.c
    - core/t64_machine.c
    - core/tests/test_boot_entry.c
    - core/tests/test_cpu_path.c
    - core/tests/test_tracer.c
    - fixtures/boot-contract.json
    - docs/spec/boot-limits.md
    - scripts/check-test-inventory.sh
key-decisions:
  - "Warm-device policy v3 names both exact zero-source SP banks and the reached reset/status behavior."
  - "CACHE operations 0x08, 0x09, 0x11 and 0x15 use coherent-functional warm semantics with no line timing."
  - "The authored app reaches its first ISViewer store, but KUSEG address translation refuses it before a device handler."
patterns-established:
  - "Every warm-device exception is tied to the active profile and exact width/value/address tuple."
  - "Live DMA evidence pairs explicit device, issue token and completion before checking copied bytes."
requirements-completed: []
coverage:
  - id: D1
    description: "The prepared warm profile implements its bounded device reset/probe rules and exact delayed two-bank SP/PI transfers."
    verification:
      - kind: unit
        ref: "ctest --preset gate -R '^unit\\.core\\.(warm-device-policy|boot-entry|bus|cpu-path|sp-dma|pi-dma|scheduler|tracer)$'"
        status: pass
      - kind: unit
        ref: "focused ASan/UBSan CTest set: warm-device-policy, boot-entry, cpu-path, tracer, SP/PI DMA, ROM fuzz, sanitizer negative control"
        status: pass
    human_judgment: false
  - id: D2
    description: "The pinned loader reaches the authored app entry and first output instruction; the output address is refused before device handling."
    verification:
      - kind: integration
        ref: "unit.core.warm-device-policy live loader trace"
        status: pass
    human_judgment: true
    rationale: "The first output instruction carries KUSEG t0=0x13ff0000 and is refused by address translation, so no guest output is captured; plan 03-06 owns the end-to-end boundary."
duration: 138min
completed: 2026-11-11
status: complete
plan_head_before: 156a24f50c1fecbed6ef973035944be51f8f3985
plan_head_after: 3c5d6379d2e20f9acacecdbdc006fb9cea73bd4f
commits: 5
---

# Phase 3 Plan 05: Bounded warm devices and loader DMA

Created first: [core/t64_warm_devices.c](../../../core/t64_warm_devices.c) and [core/tests/test_warm_devices.c](../../../core/tests/test_warm_devices.c). They implement and check the profile-scoped warm device and DMA rules used by the pinned loader.

## Performance

- **Duration:** 138 minutes from the first plan commit
- **Started:** 2026-11-11T07:25:32Z
- **Completed:** 2026-11-11T09:43:26Z
- **Tasks:** 2
- **Files modified:** 20 plan files before this summary

## Accomplishments

- Added active warm-device policy version 3, with selected software values MI_VERSION=0 and RI_SELECT=1. MI_VERSION=0 selects the pinned loader's non-iQue branch; RI_SELECT=1 selects its prepared warm branch. Neither value is a NUS-CPU-03 register measurement.
- Implemented the reached SP_STATUS set-HALT/clear-BROKE-INTR-SIG command, exact MI_INIT_MODE and AI/SI interrupt-clear tuples, and the profile-scoped ignored MI_INTR width-4 write `0x555`. Primary-document facts and the chosen compatibility behavior are distinguished in [boot-limits.md](../../../docs/spec/boot-limits.md).
- Qualified the exact absent-source SP descriptor `0x00802000`, raw length `0x0fff`, one 4096-byte row to either IMEM offset `0x1000` or DMEM offset zero. Only the selected bank changes at completion. Independent tests cover adjacent/partial tuples, queue refusal, cancellation, reset/status preservation, deadlines and per-instance isolation.
- Extended the active coherent-functional CACHE policy to operations `0x08`, `0x09`, `0x11`, and `0x15`; CACHE `0x11` is tested at the live virtual address `0x807f8ff0`, translated to `0x007f8ff0`, outside installed RDRAM. The functional operation records state without reading or writing host/RDRAM bytes.
- Qualified authored z64 admission by embedded payload identity, header entry/load-size structure, installed-RAM bounds and a 1 MiB whole-cartridge ceiling. Rejections remain transactional.
- Rebound policy/profile/initial-state identities to `831a0a58…`, `5775f197…`, and `a3820a5b…`. A separate encoder reproduced the previous policy/profile/state literals before deriving the new values. Descriptor v2, state v6 and the v1 synthetic goldens remain unchanged.
- The live loader reached authored entry PC `0x80000400` at tick 530174. PI token 2 copied 184 bytes from cartridge `0x10001000` to RDRAM `0x400`, matched at completion tick 523701. SP token 7 completed the IMEM transfer at tick 529645; queued token 8 completed the exact DMEM zero fill at tick 530169.

## Task Commits

1. **Task 1 RED:** `fb54491` — failing warm reset/probe behavior.
2. **Task 1 GREEN:** `f18558a` — admit qualified authored cartridges and execute warm reset/probe rules.
3. **Task 2 RED:** `5e6b4a3` — pin exact zero-source SP descriptor and malformed tuples.
4. **CACHE RED:** `c464341` — CACHE operation 0x11 negative control.
5. **Task 2 GREEN:** `3c5d637` — complete warm loader DMA and integration boundaries.

**Unpublished evidence sanitation:** The private paths were removed from four local plan-evidence commit objects before this task commit. Their replacement IDs are `f18558a540e60fb361195d3b5ba1b49bf70bb5a4`, `5e6b4a325738fdff9462e223e542635ed255426e`, and `c4643417ba15fdb6345988f4a98b837e6421953d` for the rewritten commits; `fb54491` was unchanged. Remote main remained `0445a566`; no push or public-history rewrite occurred.

## TDD Gate Compliance

- SP RED evidence in `03-05-task2-red.json` and corrected CACHE RED evidence in `03-05-cache-red.json` both passed `gsd-tools check tdd-red-evidence`; the focused implementation tests passed afterward.
- The first CACHE RED attempt was invalid: its sentinel store used the same out-of-RDRAM base as CACHE and stopped before reaching the operation. That attempt is disclosed in the evidence. The corrected setup used a KSEG0 sentinel store; against the pre-CACHE allow-list it retired that store, then stopped at PC `0xa4000044` on opcode `0xbd110000` without retiring CACHE 0x11. The corrected case then passed with the implementation.
- No refactor-only changes were needed. The final integration assertions and policy update are included in the GREEN commit.

## Automated Checks

- Focused gate tests: 8/8 passed (`warm-device-policy`, `boot-entry`, `bus`, `cpu-path`, `SP/PI DMA`, `scheduler`, `tracer`).
- Full gate run before local evidence sanitation: 76/77 passed. The only failure was `gov.pii.repo`, which found private paths in unpublished task evidence. After parent sanitation, the focused PII and rights repository checks passed 2/2; no source changed during sanitation.
- Test inventory: gate 77; ASan 79. Inventory removal tests passed all 148 controls and 5 malformed controls.
- Core lint: 17 files, 0 violations.
- Fresh focused ASan/UBSan set: 8/8 passed with address and undefined-behavior checks enabled (`ASAN_OPTIONS=detect_leaks=0`). Sanitizer compilation showed the existing nonfatal frame-size warnings.
- `git diff --check` passed. No hosted CI run or push was performed.

## Deviations from Plan

### Auto-fixed Issues

1. **Observed loader disproved the IMEM-only zero-source assumption.** The real loader issued the same exact absent-source row to DMEM as well as IMEM. The qualification now allows only the two observed bank tuples; both banks have direct boundary, deadline, and cancellation checks. No generic zero-source behavior was added.
2. **Reached status/reset operations required a narrower live-path policy.** Added only the exact documented command encodings used by the loader and the authorized exact ignored MI_INTR write. SP status remains permanently HALTed in this software profile; status writes preserve active/pending DMA. Unsupported encodings and adjacent read-only registers still refuse.
3. **Later device support made earlier smoke stops obsolete.** `boot-entry`, `tracer`, and `cpu-path` now stop at the bounded tick-100 prefix and preserve their admission, initial state, and early instruction checks. The warm-device test owns the full loader-to-entry/output-instruction trace. Trace drains occur at intervals no longer than 16 ticks and report no drops or saturation.
4. **The first output boundary is a guest-address prerequisite.** At PC `0x8000041c`, opcode `0xad0a0000` (`sw t2,0(t0)`), the full guest `t0` is `0x0000000013ff0000`. `fixtures/hello/hello.S:6` defines this as `T64_ISVIEWER_BASE=0x13ff0000`, a KUSEG address. `t64_cpu_translate` refuses it before any bus or ISViewer handler. This is not only a missing output device; 03-06 owns the fixture-address contract. No virtual-address mode was broadened here.

**Total deviations:** 4 scoped corrections. They were necessary to match the actual pinned loader and keep its unsupported boundaries explicit.

## Remaining Limits and Next Phase

No application output bytes are captured yet. Plan 03-06 must address the `0x13ff0000` KUSEG fixture constant at the authorized end-to-end boundary. No RSP engine, cold RDRAM calibration, authentic IPL2 handoff, broad cartridge compatibility, hosted current-source run, or hardware accuracy claim is included. BOOT-01, BOOT-03 and PROF-05 remain unchecked until owning end-to-end and phase verification.

---
*Phase: 03-first-headless-rom-through-open-ipl3*
*Plan: 05*

## Self-Check: PASSED

The first two created artifact paths and this summary exist. All five plan task commits are ancestors of the current HEAD.
