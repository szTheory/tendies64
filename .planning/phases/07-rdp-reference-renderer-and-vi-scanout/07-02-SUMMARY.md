---
phase: 07-rdp-reference-renderer-and-vi-scanout
plan: "02"
subsystem: emulation
tags: [n64, rdp, dpc, rsp, c17, ctest]

requires:
  - phase: 07-01
    provides: profile, VI timing, raw-video consumer, and retained fill-path evidence
provides:
  - Incremental DPC transport, counters, raster effect scheduling, and FullSync/MI fence behavior
  - Fetched RSP COP0 DPC access on the composite profile, with legacy profile refusal retained
  - Registered transport ordering and refusal cases plus reviewed source/body controls
affects: [phase-07-rdp, phase-07-qualification, rsp, rdp, dpc]

actuals:
  tokens: 24549
  tasks: 2
  commits: 4

tech-stack:
  added: []
  patterns:
    - One shared DPC register path for CPU MMIO and fetched RSP COP0
    - Reserve scheduled work before consuming command or raster state

key-files:
  created: []
  modified:
    - core/CMakeLists.txt
    - core/t64_bus.c
    - core/t64_digest.c
    - core/t64_internal.h
    - core/t64_isviewer.c
    - core/t64_machine.c
    - core/t64_rdp.c
    - core/t64_rsp.c
    - core/tests/test_rdp.c
    - core/tests/test_vi.c
    - docs/spec/rdp-vi-cases.json
    - docs/spec/rdp.md
    - runner/t64_real.c
    - runner/t64_real_fixture.c
    - runner/tests/test_rdp_video.c
    - scripts/check-rdp-vi-cases.py
    - scripts/check-rsp-no-hle.py
    - scripts/check-test-inventory.sh
    - scripts/tests/test_rsp_no_hle.py

key-decisions:
  - "Version the affected composite profile semantics while preserving older profile identity and DP_UNAVAILABLE refusal."
  - "Decode DPC counters at bus offsets +0x10..+0x1c as read-only counters with their documented clear commands."
  - "Treat fetch progress, raster work, busy state, and FullSync completion as distinct observable boundaries."

patterns-established:
  - "CPU and fetched RSP accesses share DPC register methods and transport effects."
  - "A continuation event is reserved before the current raster row mutates output."

requirements-completed: []

duration: 67m
completed: 2026-11-16
status: complete
commits: 4
plan_head_before: d6335fe08f190f393af161469212c531ded59c0a
plan_head_after: 14eb58cc09c7b78a1e7fd63e9c40db86fb11bf55
---

# Phase 07 Plan 02: DPC Transport and Fetched RSP DP Access Summary

**Incremental DPC fetch, raster effects, counters, and FullSync fences now share one transport path across CPU MMIO and fetched RSP accesses on the composite profile.**

## Performance

- **Duration:** About 67 minutes from the first retained Plan07-02 evidence; executor startup time was not captured.
- **Tasks:** 2 of 2.
- **Task commits:** 4, measured from the persisted plan base.
- **Diff size:** 98,197 characters across the 19 modified files (24,549 chars/4 estimate tokens).

## Accomplishments

- Implemented START, END, CURRENT, STATUS, and the four read-only DPC counters, including clear commands, source-backed freeze/flush/XBUS behavior, incremental fetch, and separate raster work.
- Added ordered FullSync completion after preceding work, including the DP pending-mask and MI interrupt behavior. Reset, serialization, digest, and the composite profile version include state that affects future work.
- Added the omitted CPU bus decode for DPC counters at `+0x10..+0x1c`, scoped to this transport. This was explicitly authorized by the parent after the plan file list was found to omit the bus owner.
- Routed fetched RSP COP0 registers 8–15 through the same DPC accessors for the composite profile. Older profiles still refuse with `T64_RSP_REFUSAL_DP_UNAVAILABLE`.
- Registered and exercised the transport matrix, including fetched RSP fill, older-profile refusal, append/resume, partial fetch, unsupported command after prior work, and same-tick ordering.
- Preserved the existing fixture and accepted-path controls; no protected guest material or external oracle was used.

## Verification

- **Task 1 focused checks:** the registered DPC suite and fill/VI owner tests passed. The accepted fill runner and wrong-pixel comparator control passed before the full gate.
- **Task 2 RED:** the accepted composite fixture loaded and `t64_rsp_start` succeeded, then the first fetched MTC0 `DPC_START` was refused. The older-profile refusal control passed. The retained RED evidence was classified `RED_EVIDENCE_OK` for `unit.core.dp.transport` (CTest exit 8, one target failure, 294 checks with 42 assertion failures, and no setup/compile/empty-test failure).
- **Task 2 GREEN:** `python3 -B scripts/check-rdp-vi-cases.py --build build/gate --suite dp --run --artifact-prefix 07-02-task2-green2` passed all 16 registered cases. The fetched instruction tests reach the accepted operation or intended refusal. The partial-packet control injects the malformed half-command after fetched START/END because the bus END register enforces alignment and cannot itself create that bound.
- **Normal gate:** `./scripts/gate.sh` passed all 146 CTest tests, including inventory/self-tests, the fixture qualification and accepted runner, the wrong-pixel control, RDP/VI/RSP units, and RSP smoke integration. Log: `build/gate/07-02-full-normal-gate.log`; preserved owning `LastTest.log`: `build/gate/07-02-preserved-full-normal-gate-LastTest.log` (SHA-256 `d0337254a706cf17336b342242eede7a84e803df798b7eefeba7e0ecfddb37c1`).
- **Qualified artifact identities:** `build/gate/core/test_dp_transport` SHA-256 `7070f2d41203c14fa2bcbe01dee9900ab253a46f3bd31d0bf2aefc24930de924`; `build/gate/core/libt64_core.a` `5dbd3c5ac22694010f8f816ac02502ecb21222ea0283a8bcccb6590edc0ee3f0`; `build/gate/runner/t64-runner-real` `0d2e9596c2c6ddc288b27dfbecf9d097cd63f95afcb079dd655f8158a8e24730`; `build/gate/runner/test_rdp_video` `f115188a36993defb83d752bbd09d1e11cb16d04d48e5e5aef9543fd54b1238a`; ASAN `build/asan/core/test_dp_transport` `7b06e7d94172965429c0152b80a790a0d0ab9458b63cab8e138dc291f32aaa37`.
- **ASAN/UBSan:** the ASAN preset build and source/object route check passed; all 13 affected owners passed, including DPC transport, RDP/VI fill, RSP controls/fuzz, fill runner, and RSP smoke. Log: `build/asan/07-02-asan-affected-console.log`; exit status `0`. A full ASAN suite was not rerun for this plan; previously observed BASE-wide timeout behavior and its isolated controls are recorded in the retained Plan01 evidence. No timeout or concurrency limits were changed.
- **Source, rights, and provenance:** `python3 -B scripts/check-rsp-no-hle.py --root . --build build/gate` passed the 76-source seal checks; `python3 -B scripts/tests/test_rsp_no_hle.py` passed all 29 mutation seeds and guest controls. `sh scripts/check-provenance.sh` reported 964 files, 57 copied-looking entries, and zero errors. `sh scripts/rights-scan.sh --tree` reported zero findings.

## Task Commits

| Task | Commit |
| --- | --- |
| DPC transport behavior matrix (RED) | `b21e35ba70ea3e64cfe67df08ddfa9dedd1ea8e4` |
| DPC transport and FullSync fences | `524af6b3f3d15c8287c2abfa2f5e516659966325` |
| Fetched RSP DPC transport matrix (RED) | `9f1080232e7e5ae51dde9366bfb4e7efa8c12a04` |
| Route fetched RSP DPC accesses | `14eb58cc09c7b78a1e7fd63e9c40db86fb11bf55` |

## Deviations from Plan

- **Bus-owner omission:** The plan's `files_modified` list omitted `core/t64_bus.c`, which owns CPU MMIO decode. Parent authorized the exact DPC counter window addition and its directly affected strict bus controls; no unrelated bus decoding changed.
- **Missing planned test file:** `core/tests/test_rsp.c` referenced by Task 2 does not exist. The fetched-instruction cases were added to the existing `core/tests/test_rdp.c` dispatcher and registered in the owning inventory.
- **Instruction visibility timing:** Initial Task 2 GREEN sampling occurred before the existing three-tick MTC0 load/store hazard retired. The assertions were moved to the actual retirement/event tick; the focused rerun passed all 16 cases.
- **Partial-packet reachability:** A bus write to END is aligned, so the malformed half-command was injected into the scheduled fetch boundary after a fetched RSP START/END sequence to reach the real truncation guard.

## Issues Encountered

- The first Task 2 run exposed the intended composite-profile refusal and provided the RED evidence. Two early observation points were also too soon for the existing RSP issue timing; they were corrected and the owning focused suite passed.
- One local JUnit wrapper attempt used the reserved zsh name `status`; the CTest run was preserved and repeated with a working capture wrapper. This did not require a source change.

## TDD Gate Compliance

- Both tasks registered their executable cases before RED and ran their owning suite before proceeding to the next task.
- Task 2's accepted operation failed specifically at fetched `DPC_START` before implementation; the profile refusal control passed. The RED report and semantic evidence are retained at `build/gate/07-02-task2-red-evidence.xml` and `build/gate/07-02-task2-red-evidence-record.json`.
- The subsequent focused GREEN reached 16 nonempty cases in the owning suite. The full normal gate and affected ASAN/UBSan suite passed after implementation.

## User Setup Required

None.

## Next Phase Readiness

This plan contributes implementation and authored test evidence to VID-01 and VID-02, but completes neither requirement as a whole; `requirements-completed` is intentionally empty. VID-04 remains blocked pending the later authority and actual-comparison work. The D2 external-rights and D6 release stops remain in force. No hardware timing accuracy or broad compatibility claim follows from these tests.

## Self-Check: PASSED

The summary file exists, all four task commit hashes are ancestors of the current plan head, the recorded plan ledger counts four task commits, and the verification/artifact references above were confirmed from retained build outputs.

---
*Phase: 07-rdp-reference-renderer-and-vi-scanout*
*Completed: 2026-11-16*
