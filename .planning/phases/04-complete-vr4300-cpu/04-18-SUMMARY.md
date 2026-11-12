---
phase: 04-complete-vr4300-cpu
plan: "18"
subsystem: cpu-verification
tags: [vr4300, native-cpu, asan, inventory, hosted-ci, security]
requires:
  - phase: 04-15
    provides: Authored CPU diagnostic adapter, profile loader policy, and legacy fixture boundary.
provides:
  - Portable native CPU evidence bound to final source and build identities.
  - Reviewed local-only diagnostic output and exact-source hosted verification.
affects: [04-16, cpu-diagnostic, native-evidence, cpu-qualification]
actuals:
  tokens: 11496
  tasks: 2
  commits: 4
plan_head_before: 7680bd6975253a08798f7fec7e10221ba231d25e
plan_head_after: 52921057f385e0add7f85f16896bdddfa4901d6f
tech-stack:
  added: []
  patterns:
    - Bind local evidence to source revision plus per-configuration build identity.
    - Keep authored local diagnostic output out of public CI and evidence uploads.
key-files:
  created:
    - docs/evidence/phase04-native-cpu.md
    - .planning/phases/04-complete-vr4300-cpu/04-REVIEW.md
    - .planning/phases/04-complete-vr4300-cpu/04-SECURITY.md
    - .planning/phases/04-complete-vr4300-cpu/04-18-SUMMARY.md
  modified:
    - runner/CMakeLists.txt
    - runner/t64_cpu_diagnostic.c
    - runner/tests/test_cpu_diagnostic.c
key-decisions:
  - Keep public native results limited to authored source and the exact tested software profile; do not infer private qualification or hardware behavior.
  - Restrict both authored diagnostic entry points and output destinations to local use, including strict-POSIX builds.
  - Preserve the existing sanitizer limits, test inventories, warm-v1 boundary, and hosted workflow.
requirements-completed: [CPU-09, VER-03]
coverage:
  - id: D1
    description: Public authored native CPU behavior has full gate, inventory, lint, and final-source diagnostic regression evidence.
    requirement: VER-03
    verification:
      - kind: integration
        ref: "sh scripts/gate.sh on c8e59ef; final 900833d full target rebuild and runner.cpu-diagnostic.* selection"
        status: pass
      - kind: unit
        ref: "check-test-inventory.sh --preset gate; --preset asan; --selftest"
        status: pass
    human_judgment: false
  - id: D2
    description: Independent arithmetic expectations, reviewed native mitigations, and both hosted jobs bind to the exact public source.
    requirement: CPU-09
    verification:
      - kind: integration
        ref: "cmake --preset asan; all-target build; 51-test selection on c8e59ef; final-source diagnostic selection on 900833d"
        status: pass
      - kind: other
        ref: "Hosted run 43293884688: gate-linux and gate-macos for 900833d; configured code/security reviews"
        status: pass
    human_judgment: false
duration: 78min
completed: 2026-11-12
status: complete
---

# Phase 4 Plan 18: Public Native CPU Verification Summary

**The authored native CPU path passed local and exact-source hosted verification, with reviewed local-only diagnostic output and a separate pending private qualification.**

## Performance

- **Duration:** About 78 minutes from dispatch through the final audit, artifacts, and observed handoff.
- **Started:** 2026-11-12T18:58:38Z (approximate dispatch time).
- **Completed:** 2026-11-12T20:17:01Z (parent observation of completed handoff).
- **Tasks:** 2.
- **Files changed:** 7, including the source fix, portable evidence, review, security report, and this summary.

## Accomplishments

- Verified the full native gate on `c8e59ef`: 111/111 tests, core lint 83 files/0 violations, and exact inventories 111 gate/113 ASan. The inventory self-test passed its 224 removal controls and five malformed-input controls.
- Built every ASan target and passed the 51-test CPU/profile/legacy/protocol/adapter/fuzz/fault selection on `c8e59ef`. Fuzz completed within the unchanged 120-second limit.
- After the final strict-POSIX portability fix at `900833d`, rebuilt all normal and ASan targets and passed all three CPU diagnostic controls in both configurations. The final exact inventory checks remained 111/113.
- Configured code review found no remaining native findings. The saved audit closed 33 threats for Plans01–15 and 17; a targeted Plan18 audit closed T-04-35 and T-04-36. Plan16 and its external qualification remain unverified.
- Both existing hosted jobs passed the exact final source: run43293884688, `gate-linux` job131446911234 and `gate-macos` job131446911538. Each passed111/111 tests, lint83 files with zero violations, authored evidence, and scans.
- Preserved the descriptor-v3 CPU profile's 4 MiB payload policy and the separate warm-v1 1 MiB ceiling/seven legacy fixture identities.

## Task Commits

1. **Task 1: Verify the complete authored native path** — evidence and review records in `6294d7d`.
2. **Task 2: Close review findings and exact-source verification** — `c8e59ef` (local-output boundary), `900833d` (strict-POSIX portability), `5292105` (persisted executed-scope security verification).

**Plan head before:** `7680bd6975253a08798f7fec7e10221ba231d25e`

**Plan head after task, evidence, and security commits:** `52921057f385e0add7f85f16896bdddfa4901d6f`

## Evidence and Limits

- [Native CPU evidence](../../../docs/evidence/phase04-native-cpu.md) records profile scope, build identities, local commands/results, sanitization, review outcomes, exact hosted jobs, and measured critical path.
- [Code review](04-REVIEW.md) records the 101-path native scope and both bounded source supplements. [Security verification](04-SECURITY.md) records the 33 saved native-scope closures plus the two Plan18 threat checks.
- The hosted run's wall time was567 seconds and measured critical path564 seconds. The macOS queue wait was229 seconds; its fixture qualification was the largest gate step at189 seconds. The parent-owned [CI review log](../../../docs/ci-reviews/LOG.md) contains per-step timings. No cache, job, concurrency, or limit change was made.
- The sanitizer build retained existing large-frame and duplicate-library warnings. The oversized supervised guest remains in the normal gate and selected out of ASan after exceeding the unchanged child wall cap under instrumentation. ASan still covers the large in-process IPL3 startup and a supervised small guest; this is not a general stack-safety claim.
- The first hosted Linux build exposed hidden `S_ISVTX` under strict POSIX feature settings. The final source uses the standardized `01000` mask only when the macro is hidden; confinement predicates are unchanged. The final Linux/macOS pair passed.
- No private ROM, source, capture, external tool, derived diagnostic result, or private diagnostic metadata was used or published for this native evidence. Plan16's tool/asset pre-use audit and private SC4 qualification remain pending; this summary does not claim them secured.

## Deviations from Plan

### Auto-fixed Issues

**1. Added the missing local output boundary found by the configured security review.**

- **Found during:** Task 2 review.
- **Issue:** The authored diagnostic entry point could run in CI/required-fixture mode, and its batch destination did not enforce a private location.
- **Fix:** Refuse both authored and local entry points under `CI=true` or `T64_REQUIRE_FIXTURES=1`; accept output only in an ignored repository destination or secure temporary directory; fail closed when Git inspection is unavailable. Preserve exclusive mode-0600 output creation.
- **Files modified:** `runner/CMakeLists.txt`, `runner/t64_cpu_diagnostic.c`, `runner/tests/test_cpu_diagnostic.c`.
- **Verification:** Full gate and 51-test ASan selection passed on `c8e59ef`; all three owning controls passed after final source updates and on both hosted jobs.
- **Commit:** `c8e59ef`.

**2. Added a strict-POSIX sticky-bit fallback after the first hosted Linux build.**

- **Found during:** Exact-source hosted verification of `c8e59ef`.
- **Issue:** Linux's strict `_POSIX_C_SOURCE=200809L` build hid `S_ISVTX`, stopping compilation before test/lint/evidence steps. The macOS job passed on that earlier revision.
- **Fix:** Use a named `mode_t` sticky-bit mask that selects `S_ISVTX` when exposed and POSIX `01000` otherwise; the same directory predicate and containment checks remain.
- **Files modified:** `runner/t64_cpu_diagnostic.c`.
- **Verification:** Normal and full all-target ASan builds; all three diagnostic CTests in both configurations; supplemental code and security review; exact-source Linux/macOS hosted run passed.
- **Commit:** `900833d`.

**Total deviations:** 2 necessary security/portability fixes. No production limits, test registrations, hosted jobs, or sanitizer bounds changed.

## Issues Encountered

The initial `c8e59ef` Linux hosted attempt stopped at compile time because of strict POSIX macro visibility. This was fixed and verified before the final `900833d` pair; both final hosted jobs succeeded. No unresolved native issue remains.

## User Setup Required

None.

## Next Phase Readiness

Plan18's public native verification is complete. Plan16 remains the separate owner of external tool/asset qualification and private SC4 evidence.

## Self-Check: PASSED

- Evidence, code-review, security, and summary artifacts exist at the listed paths.
- Source, evidence, and security commits are ancestors of the current head.
- The plan ledger measures four commits from `7680bd6975253a08798f7fec7e10221ba231d25e` through `52921057f385e0add7f85f16896bdddfa4901d6f`.

---
*Phase: 04-complete-vr4300-cpu*
*Completed: 2026-11-12*
