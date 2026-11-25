---
phase: "11"
slug: installed-consumer-and-freeze-candidate
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-11-25"
---

# Phase 11 — Validation Strategy

> Per-phase validation contract. Source: 11-RESEARCH.md "Validation Architecture" (test names are proposed; plans own the final names). Evidence tier: unit-tested, authored scope; a software-interface claim only. No hardware, oracle or release authority is implied.

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CTest on Ninja, C tests with `tests/t64_test.h`, Python/sh harness tests, `scripts/digest-sweep.py` two-tree sweep |
| **Config file** | `CMakePresets.json` (gate, asan), `core/CMakeLists.txt`, `runner/CMakeLists.txt`, `scripts/check-test-inventory.sh` |
| **Quick run command** | `cmake --build --preset gate --target t64_core && ctest --preset gate -R '^(repo\.abi|install|installed|api04)\.'` |
| **Full suite command** | `./scripts/gate.sh` |
| **Estimated runtime** | quick about 10 s native; full gate cold hosted macOS 731 s of 900 s today |

## Sampling Rate

- **After every task commit:** run the touched area's CTest names plus `harness.rdp-oracle-authority`, `repo.si-membership-sites`, `harness.state-field-use-audit`, `repo.p10-mutant-anchors`
- **After every plan wave:** `./scripts/gate.sh` once after the shape wave; digest sweep against the recorded wave base; local ASan consumer run
- **Before `/gsd-verify-work`:** full gate green, hosted pair green, qualifier record
- **Max feedback latency:** 90 s for the scripted preflight; 15 s for cheap-first ABI checks

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 11-XX-XX | TBD | TBD | API-01 | — | header self-contained C99 pedantic and C++17; layout asserts source then installed | cheap | `ctest -R '^repo\.abi-(header\|layout)$'` | ❌ W0 | ⬜ pending |
| 11-XX-XX | TBD | TBD | API-01 | — | every operation incl. reset/unload, struct_size, ABI mismatch accepted and refused | unit + installed | `ctest -R '^unit\.core\.api\.\|^installed\.consumer\.ops$'` | ❌ W0 | ⬜ pending |
| 11-XX-XX | TBD | TBD | API-02 | — | status/stop table lists each constant once; unclassified fails | script | `ctest -R '^repo\.abi-status-classes$'` | ❌ W0 | ⬜ pending |
| 11-XX-XX | TBD | TBD | API-02 | — | refusal is a no-op; guest-stop resumes; defect poisons (ASan/UBSan local, native small N) | consumer | `ctest -R '^installed\.consumer\.sequence$'` | ❌ W0 | ⬜ pending |
| 11-XX-XX | TBD | TBD | API-03 | — | freeze record recomputed; incompatible change fails | script | `ctest -R '^repo\.abi-freeze-record$'` | ❌ W0 | ⬜ pending |
| 11-XX-XX | TBD | TBD | API-04 | — | installed members import no file, clock, entropy or thread symbol; seeded objects fail | script | `ctest -R '^api04\.'` | ❌ W0 | ⬜ pending |
| 11-XX-XX | TBD | TBD | VER-05 | — | relocated install, consumer composite three-way, negative controls | script + consumer | `ctest -R '^install\.\|^installed\.consumer\.composite$\|^installed\.controls\.'` | ❌ W0 | ⬜ pending |
| 11-XX-XX | TBD | TBD | VER-05 | — | host-shaped loop, two instances, drain-cadence independence | consumer | `ctest -R '^installed\.consumer\.hostloop$'` | ❌ W0 | ⬜ pending |
| 11-XX-XX | TBD | TBD | API-01 | — | consumer includes only `tendies64/` headers | grep | `ctest -R '^repo\.consumer-include-guard$'` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

## Wave 0 Requirements

- [ ] `scripts/preflight-phase11.sh` — scripted preflight under about 90 s (copy of the Phase 10 script)
- [ ] printer shim in `tools/digest-sweep/t64_digest_sweep.c` and a recorded Phase 11 wave base file
- [ ] `PHASE11_TESTS` block and `ALL` entry in `scripts/check-test-inventory.sh`
- [ ] `tests/installed-consumer/` project, `docs/abi/` goldens and generator, `docs/spec/api.md`
- [ ] `scripts/qualify-phase11.py` thin delta and `docs/claims/phase11.md`

## Manual-Only Verifications

All phase behaviors have automated verification. The Linux install path is unverified until a hosted run (the act image has no cmake).

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 90 s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
