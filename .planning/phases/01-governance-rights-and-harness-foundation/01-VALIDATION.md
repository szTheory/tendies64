---
phase: "1"
slug: "governance-rights-and-harness-foundation"
status: validated
nyquist_compliant: true
wave_0_complete: true
created: "2026-11-09"
---

# Phase 1 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> Source: `01-RESEARCH.md` section "Validation Architecture".

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CTest (CMake 3.25 floor) + in-repo C asserts (`tests/t64_test.h`) + POSIX sh drivers; no third-party framework |
| **Config file** | `CMakePresets.json` (`gate` presets), `tests/controls/matrix.tsv`, `tools/lint/rules.txt` |
| **Quick run command** | `ctest --preset gate -R <name>`; `sh tools/lint/core-lint.sh --selftest`; `sh scripts/rights-scan.sh --selftest` |
| **Full suite command** | `./scripts/gate.sh` (the only command CI runs) |
| **Estimated runtime** | under 60 seconds locally |

---

## Sampling Rate

- **After every task commit:** the relevant quick command (under 30 s)
- **After every plan wave:** `./scripts/gate.sh`
- **Before `/gsd-verify-work`:** full gate green on `gate-linux` and `gate-macos`, every completed CI run of the phase recorded in `docs/ci-reviews/LOG.md` (the first run's review is the VER-04 phase-gate minimum), Rust spike result recorded
- **Max feedback latency:** 60 seconds

---

## Per-Task Verification Map

| Success criterion | Behavior | Test type | Automated command | File exists | Status |
|-------------------|----------|-----------|-------------------|-------------|--------|
| SC1 | ADRs, policy and claims template exist with required headings, statuses and reversal evidence | doc check | `sh scripts/check-docs.sh` | ❌ W0 | ⬜ pending |
| SC2 | Gate runs locally; CI runs it verbatim; explicit labels; SHA pins; every completed run reviewed (first run minimum) | static + recorded | `sh scripts/check-ci-yml.sh`; a row per completed run id (first run included) in `docs/ci-reviews/LOG.md` | ❌ W0 | ⬜ pending |
| SC3 | Six outcomes classify exactly; wrong value fails; missing, malformed, timeout and crash never pass; inventory change detected | CTest control matrix | `ctest --preset gate -L controls` | ❌ W0 | ⬜ pending |
| SC4a | Core reading time, files, threads, entropy or mutable globals fails both lint layers | seeded | `sh tools/lint/core-lint.sh --selftest` | ❌ W0 | ⬜ pending |
| SC4b | Protected or unmanifested binary in tree, commit range or staging fails | seeded temp repos | `sh scripts/rights-scan.sh --selftest` | ❌ W0 | ⬜ pending |
| SC4c | Manifest entry missing any required field fails | seeded | `t64-manifest-check` over `tests/seeded/manifest/bad_*.json` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

### Negative controls and green-on-empty (all required)

- Lint scanning zero files fails; rights scan over zero blobs fails; manifest with zero entries fails; zero CTest tests fails (`noTestsAction: error`).
- Every lint rule id has at least one seeded violation; a rule with none fails the selftest. Seeds must be observable at `-O2`.
- Positive controls: `good.c` passes on gcc, clang and Apple clang; a clean text-only temp repo passes the rights scan; a valid manifest passes.
- Control matrix: positive, wrong counts, failure after Done, deleted fixture, tampered digest, truncated verdict, non-UTF8 or over-long output, spin, empty output, abort, segv, exit0-no-result, hang, dropped inventory case, truncated suite. Each asserts exact outcome and exit code, runs twice (second at a different slice size), and canonical bytes must match.
- Seeded protected-looking inputs (ROM magic, late-NUL files) are generated into temp dirs at test time and never committed.

---

## Wave 0 Requirements

- [ ] `CMakeLists.txt`, `CMakePresets.json`, `tests/t64_test.h`, `tools/common/*` (JSON, SHA-256 with FIPS 180-4 vectors, UTF-8 with RFC 3629 negatives)
- [ ] `scripts/gate.sh` skeleton (`LC_ALL=C`, versions banner, timed steps), `scripts/check-docs.sh`, `scripts/check-ci-yml.sh`
- [ ] Seeded directories `tests/seeded/{manifest,provenance,claims}` and `tools/lint/selftest/`
- [ ] ADR-0002 (language/build/ABI) accepted before the first `core/*.c` lands. Superseded in detail by CONTEXT amendment A-1: exactly `core/t64_core_stub.c` and `core/include/tendies64/t64_facade.h` may exist before acceptance, and any other file under `core/` fails `DOC-CORE-BEFORE-ADR` (plan 01-04) until ADR-0002 is accepted (plan 01-08)

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Critical-path review recorded for every completed CI run of the phase (the first run's row is the phase-gate minimum) | VER-04 | Needs real hosted runs | Run `scripts/ci-review.sh <run-id> --append docs/ci-reviews/LOG.md` for each completed run of the phase's pushed SHAs (plan 01-13 automates this and verifies coverage) |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency under 60 s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** approved 2026-11-10 (61 automated tests, every task has an automated verify; gate 16-20 s local, 15-27 s hosted)
