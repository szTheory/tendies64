---
phase: "10"
slug: carry-in-closure-before-the-freeze
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-11-23"
---

# Phase 10 — Validation Strategy

> Per-phase validation contract. Source: 10-RESEARCH.md "Validation Architecture" (read it for mutant lists and the pin-impact map).

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CTest over C test binaries (`tests/t64_test.h`) plus Python harness tests |
| **Config file** | `CMakePresets.json` (gate, asan), `core/CMakeLists.txt`, `runner/CMakeLists.txt` |
| **Quick run command** | `ctest --test-dir build/gate -R '<task regex>' --output-on-failure` after `cmake --build --preset gate` |
| **Full suite command** | `ctest --preset gate` and `sh scripts/gate.sh` |
| **Estimated runtime** | quick ~5-60 s; full gate ~140-220 s native |

## Sampling Rate

- **After every task commit:** the task's named CTest regex plus `ctest -R '^harness\.rdp-(oracle-authority|vi-qualification)$'` and `python3 -B scripts/check-rsp-no-hle.py --build build/gate`
- **After every plan wave:** `ctest --preset gate`, `sh scripts/preflight-phase10.sh` (<= 90 s), act Linux compile for any C change, post-Gate CI step replay, TIMEOUT audit, `ctest -N -R` regex count check
- **Before verify-work:** full gate green, ASan selection of new names, hosted pair green on final head, one forced cold macOS run
- **Max feedback latency:** 90 s (task level)

## Per-Task Verification Map

Filled by the planner per task; requirement-level map:

| Requirement | Test Type | Automated Command | File Exists |
|-------------|-----------|-------------------|-------------|
| GATE-02 | harness (python) | `ctest -R '^harness\.rdp-oracle-authority$'`; `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked` | extend `scripts/tests/test_rdp_oracle_authority.py` |
| GATE-01 | scanner selftest | `ctest -R '^repo\.ci-yml(-selftest)?$'`; COST in `ctest --show-only=json-v1` | edit `scripts/check-ci-yml.sh` |
| GATE-01 (measured) | hosted cold run | `gh cache delete`, `gh workflow run`, `gh run view --json jobs` | orchestrator step |
| VER-06 | unit + mutant driver | `ctest -R 'unit\.core\.(vg|vi\.clock)'`; `python3 -B scripts/p10-mutants.py --run` | ❌ W0 (new files; never edit the 14 oracle-bound files) |
| VID-07 | unit | `ctest -R 'unit\.core\.(profile|digest|rdp\.depth)'`; K1 characterization before/after; digest sweep | ❌ W0 |
| CORE-06 | unit | `ctest -R 'unit\.core\.(digest\.v26|state\.(export|import))'`; `harness.state-field-use-audit` | ❌ W0 |
| RUN-07 | runner integration | `ctest -R 'runner\.composite'` | ❌ W0 |

## Wave 0 Requirements

- [ ] `scripts/preflight-phase10.sh` (act compile, post-Gate replay, ctest -N counts, TIMEOUT audit, oracle check, PII scan, head CI, host load)
- [ ] committed two-tree digest sweep tool with forced build identity (17 ids x authored ROMs x checkpoints)
- [ ] `core/tests/test_k1_refusal.c` characterization of Z refusal on ids 11/12, captured before any predicate edit
- [ ] `scripts/p10-mutants.py` with anchors for VG and VID mutants
- [ ] `core/tests/test_digest_v26.c` RED completeness test before the walker change
- [ ] pin-impact map committed

## Manual-Only Verifications

All phase behaviors have automated verification. The hosted cold macOS run is orchestrator-executed, not human.

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 90 s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
