---
phase: "12"
slug: minimal-host-playable-homebrew-and-final-bind
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-11-26"
---

# Phase 12 — Validation Strategy

> Per-phase validation contract. Source: 12-RESEARCH.md "Validation Architecture"; the CTest names below are the ones the plans register (PIN-IMPACT DR-27 of 12-01 is the binding list). Evidence tier: unit-tested, authored scope; a software-interface claim only. No hardware, oracle, real-time, rendering, compatibility or release authority is implied; the SDL smoke is self-checking host glue only.

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CTest on Ninja (`cmake --preset gate`), C test programs (`tests/t64_test.h`, the host's `t64h_tests --case NAME`), Python and sh checkers, `scripts/digest-sweep.py` two-tree sweep |
| **Config file** | `CMakePresets.json` (gate, asan), `core/CMakeLists.txt`, `runner/CMakeLists.txt`, `tools/abi/CMakeLists.txt`, `tools/hostcheck/CMakeLists.txt` (new), `scripts/check-test-inventory.sh` (`PHASE12_TESTS`), `scripts/preflight-phase12-regexes.txt` (new) |
| **Quick run command** | `sh scripts/preflight-phase12.sh --no-replay` then `ctest --test-dir build/gate -L phase12` |
| **Full suite command** | `./scripts/gate.sh` (hosted `gate-macos` 15 min, `gate-linux` 10 min limits) |
| **Estimated runtime** | preflight about 25 to 70 s without replay; full gate about 145 s local; cold hosted gate-macos 729 s of 900 s at the Phase 11 close (F-136), Phase 12 adds about +15 to +25 s (estimate, replaced by the 12-06 and 12-12 measurements) |

## Sampling Rate

- **After every task commit:** the task's own CTest names plus the sampling pin guards `harness.rdp-oracle-authority`, `repo.si-membership-sites`, `harness.state-field-use-audit`, `repo.p10-mutant-anchors`, `repo.rsp-no-hle` when core/, runner/, the manifest or CMake changed; `sh scripts/preflight-phase12.sh --no-replay` at each plan's commit point
- **After every plan wave:** `./scripts/gate.sh` in a scratch clone (12-06 for W0 and TITLE, 12-12 for HOST with the full p12 mutant run, 12-14 Task 4 for the staged bind before its commit, 12-18 final audit); digest sweep against the Wave 0 base (0 differences)
- **Before `/gsd-verify-work`:** full gate green, hosted pair green at the pinned 12-18 SHA, final qualification record verified, orchestrator cold gate-macos run recorded
- **Max feedback latency:** 70 s for the scripted preflight; 15 s for the cheap-first ABI and host names

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 12-01-01 | 12-01 | 1 | API-05, API-06, API-07 | T-12-01 | preflight frame, Wave 0 base after the planning commit, zero rows for every Phase 12 family | script | `sh scripts/preflight-phase12.sh` | ❌ W0 | ⬜ pending |
| 12-02-01..03 | 12-02 | 2 | API-06 | T-12-04, T-12-05, T-12-06 | oracle from the spec only (stdlib imports), refuses ambiguous stamps (200,000-tick margin), blank digest distinct | unit | `ctest -R '^harness\.title-oracle$'` | ❌ W0 | ⬜ pending |
| 12-03-01..03 | 12-03 | 3 | API-06 | T-12-08, T-12-10 | guest equals the oracle on every scenario before the source freeze (throwaway probe-check, exit status) | probe | `sh build/12-03-probe/probe-check.sh --scenarios all` | ❌ W0 | ⬜ pending |
| 12-03-04 | 12-03 | 3 | API-06 | T-12-07, T-12-09 | manifest rows authored-here / cleared / build-only; no committed ROM; no rebind | policy | `ctest -R '^(gov\.manifest\.\|gov\.rights\.\|harness\.rdp-oracle-authority$)'` | exists | ⬜ pending |
| 12-04-01..02 | 12-04 | 4 | API-06 | T-12-11, T-12-13 | one supervised raw-capture run equals the oracle and the two runner exclusions are recorded; runner pins labeled, never independent | integration | `ctest -R '^runner\.title-two-run$'` | ❌ W0 | ⬜ pending |
| 12-04-02..03 | 12-04 | 4 | API-06 | T-12-12 | wrong-input fails at checkpoint 2; refused frame non-pass with no completion, capture-error at the cap, no transcript (blank frames are asserted by installed.consumer.title-refused, 12-05) | control | `ctest -R '^controls\.real\.title-(wrong-input\|refused-frame)$'` | ❌ W0 | ⬜ pending |
| 12-05-01..03 | 12-05 | 5 | API-06 | T-12-14, T-12-15 | installed consumer only (moved prefix); short, long, shifted, persist | integration | `ctest -R '^installed\.consumer\.title'` | ❌ W0 | ⬜ pending |
| 12-07-01 | 12-07 | 7 | API-05 | T-12-20, T-12-22 | pump built only against the moved prefix; drain order video, audio, text | build + unit | `ctest -R '^host\.pump\.(build\|drain-order)$'` | ❌ W0 | ⬜ pending |
| 12-07-02 | 12-07 | 7 | API-05 | T-12-21 | fake clock ns/16 with carry, backward clock refused, stall detector, terminal latch | unit | `ctest -R '^host\.pump\.(clock\|stall)$'` | ❌ W0 | ⬜ pending |
| 12-07-03 | 12-07 | 7 | API-05 | T-12-22, T-12-23 | host includes only the installed header; no SDL in core, runner, tools, root CMake or the archive | policy | `ctest -R '^repo\.host-(include-guard\|include-guard-selftest\|isolation)$'` | ❌ W0 | ⬜ pending |
| 12-08-01, 12-08-03 | 12-08 | 8 | API-05, API-06 | T-12-27 | title through the pump at 240 Hz and 3 Hz fake clocks equals the oracle and the headless pins | integration | `ctest -R '^host\.pump\.title-two-speeds$'` | ❌ W0 | ⬜ pending |
| 12-08-02 | 12-08 | 8 | API-05 | T-12-24, T-12-25 | mapping table, strict t64.input reader, lazy queue, QUEUE_FULL deferral | unit | `ctest -R '^host\.pump\.(input-map\|script)$'` | ❌ W0 | ⬜ pending |
| 12-08-03 | 12-08 | 8 | API-05 | T-12-26 | frame copy honors stride, refuses oversize and short destinations; S16BE swap | unit | `ctest -R '^host\.pump\.frames$'` | ❌ W0 | ⬜ pending |
| 12-08-04, 12-12-01 | 12-08, 12-12 | 8, 12 | API-05, API-06 | T-12-40 | 12 mutants each killed by a named host test (anchors gated; full run in the 12-12 clone) | mutation | `ctest -R '^repo\.p12-mutant-anchors$'`; `python3 -B scripts/p12-mutants.py --run` (local) | ❌ W0 | ⬜ pending |
| 12-09-01 | 12-09 | 9 | API-05 | T-12-28, T-12-30 | no vendored or fetched SDL, no bundle or icon, neutral window title only | policy | `ctest -R '^repo\.host-policy(-selftest)?$'` | ❌ W0 | ⬜ pending |
| 12-09-02 | 12-09 | 9 | API-05 | T-12-31, T-12-32 | SDL glue smoke under dummy drivers with a virtual gamepad; SC2 clause closed only by PASS | local lane | `sh scripts/check-host.sh sdl` (not a CTest name) | ❌ W0 | ⬜ pending |
| 12-10-01..02 | 12-10 | 10 | API-07 | T-12-33, T-12-34 | evaluation pinned to the 40-hex SHA, "no adapter now", no adapter token in code roots | policy | `ctest -R '^repo\.api07-evaluation$'` | ❌ W0 | ⬜ pending |
| 12-13-01..02 | 12-13 | 13 | SC4 (bind) | T-12-52 | minor rule holds both the candidate and a bound record; green at ABI 0.1 | script | `ctest -R '^repo\.abi-freeze-(record\|selftest)$'` | exists (extended) | ⬜ pending |
| 12-14-01..04 | 12-14 | 14 | SC4 (bind) | T-12-42, T-12-43, T-12-44, T-12-54 | one coupled bind commit proven by a scratch-clone gate; record bound at 1.0; version controls fail for their own reason | script + consumer | `ctest -R '^(repo\.abi\|install\.\|installed\.\|api04\.\|unit\.core\.api\.)'` | exists | ⬜ pending |
| 12-15-03 | 12-15 | 15 | SC4 (bind) | T-12-45, T-12-55 | stale phase promises cannot return (header-scoped provisional rule, wrapped seed) | script | `ctest -R '^repo\.stale-freeze-text'` | exists (extended) | ⬜ pending |
| 12-16-01 | 12-16 | 16 | SC4 (bind) | T-12-56 | create-time rule at major 1 (1.0 accepted; 1.1, 2.0, 0.1 refused) | unit | `ctest -R '^unit\.core\.api\.abi-major-rule$'` | ❌ W0 | ⬜ pending |
| 12-16-02..03 | 12-16 | 16 | SC4 (bind) | T-12-57, T-12-58 | major-version request refused with its own headline; Linux run or recorded skip | control | `ctest -R '^installed\.controls\.version-mismatch-(minor\|patch\|major)$'` | ❌ W0 | ⬜ pending |
| 12-17-01..02 | 12-17 | 17 | API-05, API-06, API-07 | T-12-47, T-12-48 | qualifier binds the post-bind tree; SDL pass and skip recorded distinctly | harness | `ctest -R '^harness\.phase12-qualification$'` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

## Wave 0 Requirements

- [ ] `scripts/preflight-phase12.sh` and `scripts/preflight-phase12-regexes.txt` (12-01); `PHASE12_TESTS` block and concatenation entry in `scripts/check-test-inventory.sh` (12-02)
- [ ] `fixtures/title/title-spec.md`, `title_oracle.py`, the four input scripts and `title.expected.json` before any guest source (12-02)
- [ ] `host/` scaffold, `scripts/check-host.sh`, `scripts/check-host-includes.sh` (+selftest), `scripts/check-host-isolation.sh`, `scripts/check-api-init.py` roots, `tools/hostcheck/CMakeLists.txt` (12-07)
- [ ] `scripts/p12-mutants.py` (copy of `p10-mutants.py`, 8 pump and 4 title mutants) and `repo.p12-mutant-anchors` (12-08)
- [ ] `scripts/check-host-policy.sh` (+selftest) and `docs/evidence/sdl3-host-dependency-audit.md` before the first SDL build (12-09)
- [ ] `scripts/check-api07-evaluation.sh` (+selftest) (12-10)
- [ ] `scripts/qualify-phase12.py` thin delta and `scripts/tests/test_phase12_qualification.py` (12-17); `docs/claims/phase12.md` (12-18)
- Framework install: none (SDL3 is link-only against an already present system package; no package is installed)

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Physical-controller play-through of the title in the SDL shell | API-05, API-06 | owner-held hardware; human-needed, non-blocking, owner-attested, never a must-have | `docs/evidence/phase12-host-manual.md` (template written by 12-18, marked not run; never filled by an agent) |
| SDL glue smoke (dummy drivers, virtual gamepad) | API-05 (ROADMAP SC2 clause) | local, non-gating lane by decision; ubuntu-24.04 has no libsdl3-dev candidate | `sh scripts/check-host.sh sdl`; the clause is closed only by `[host-sdl] PASS`, a skip is a recorded not-closed state |
| Version-mismatch controls on the Linux CMake major | SC4 (bind) | the hosted gate-linux runs them; the act image run is a local pre-push check | 11-28 procedure in 12-16 Task 3; recorded as `act-linux: PASS (cmake X)` or `act-linux: SKIP: <reason>` |

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 70 s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
