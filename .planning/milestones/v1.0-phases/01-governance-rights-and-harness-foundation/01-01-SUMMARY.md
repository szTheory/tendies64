---
phase: 01-governance-rights-and-harness-foundation
plan: "01"
subsystem: infra
tags: [c17, cmake, ninja, ctest, sha256, utf-8, canonical-json, gate]
status: complete

requires: []
provides:
  - "scripts/gate.sh: the single local gate command (GOV-05) with steps banner, sh-n, configure, build, ctest, summary"
  - "CMake root and gate presets (version 6) with noTestsAction error; T64_WERROR option; t64_target_warnings()"
  - "t64_core static library from the two allowlisted core files (CONTEXT A-1) and the provisional facade header"
  - "t64_common library: FIPS 180-4 SHA-256, RFC 3629 UTF-8 validator, bounded canonical JSON writer"
  - "tests/t64_test.h shared C test header; CTest test unit.common (label unit)"
affects: [01-02, 01-03, 01-04, 01-05, 01-06, 01-07, 01-08, 01-09, 01-13]

actuals:
  tokens: 10200
  tasks: 2
  commits: 2
plan_head_before: f1c87d887891c7758f99791c233d9a9ce8b29dd5
plan_head_after: 9d090101bcd2fa13a051b99ad7fa36a916178f24

tech-stack:
  added: []
  patterns:
    - "Warnings via t64_target_warnings(tgt) on every target; -Werror behind the T64_WERROR cache option"
    - "Tools auto-discovered by a CONFIGURE_DEPENDS glob over tools/*/CMakeLists.txt (except tools/common)"
    - "Tests include tests/t64_test.h by name via target_include_directories(... ${CMAKE_SOURCE_DIR}/tests)"
    - "Sticky-error bounded writer; NUL-terminated output; no allocation"

key-files:
  created:
    - CMakeLists.txt
    - CMakePresets.json
    - scripts/gate.sh
    - core/t64_core_stub.c
    - core/include/tendies64/t64_facade.h
    - tools/common/CMakeLists.txt
    - tools/common/t64_sha256.h
    - tools/common/t64_sha256.c
    - tools/common/t64_utf8.h
    - tools/common/t64_utf8.c
    - tools/common/t64_jsonw.h
    - tools/common/t64_jsonw.c
    - tools/common/tests/test_common.c
    - tests/t64_test.h
  modified:
    - .gitignore

key-decisions:
  - "t64_core is declared in the root CMakeLists.txt; no core/CMakeLists.txt exists (CONTEXT A-1)"
  - "JSON writer keys are copied into a per-depth 127-byte buffer so ordering is checked on unescaped bytes without relying on caller memory lifetime; longer keys are refused (T64_JSONW_E_KEY)"
  - "t64_test.h treats zero checks as failure so a test that asserts nothing cannot pass"

patterns-established:
  - "Gate step helper: step NAME CMD... prints [gate] NAME ok (Ns) or [gate] NAME FAILED; no step output is ever piped"
  - "Independent test vectors: FIPS/NIST published values and host shasum, never this implementation"

requirements-completed: [GOV-05]

coverage:
  - id: D1
    description: "scripts/gate.sh configures, builds and tests the core stub and t64_common, printing versions, per-step seconds and [gate] PASS"
    requirement: "GOV-05"
    verification:
      - kind: integration
        ref: "rm -rf build/gate && ./scripts/gate.sh"
        status: pass
    human_judgment: false
  - id: D2
    description: "SHA-256 matches FIPS 180-4 vectors; UTF-8 validator rejects the RFC 3629 negative set; canonical writer refuses out-of-order keys"
    requirement: "GOV-05"
    verification:
      - kind: unit
        ref: "ctest --preset gate -R unit.common (checks=122 failures=0)"
        status: pass
    human_judgment: false
  - id: D3
    description: "core/ holds exactly the two allowlisted files; an empty CTest run is an error"
    requirement: "GOV-05"
    verification:
      - kind: other
        ref: "find core -type f; ctest --preset gate -R nosuchtest exits 8"
        status: pass
    human_judgment: false

duration: 9min
completed: 2026-11-10
---

# Phase 1 Plan 01: Gate, CMake presets, core stub and t64_common Summary

**One `./scripts/gate.sh` command builds the core stub and a shared C17 library (FIPS 180-4 SHA-256, RFC 3629 UTF-8, bounded canonical JSON writer) under `-Werror` and runs CTest with zero-tests-is-error, on Apple clang 21 / CMake 4.4.3 / Ninja 1.13.2.**

## Accomplishments

- Tracer slice proven end to end: gate -> presets -> core stub + `t64_common` -> CTest SHA-256 vectors, then expanded.
- Root `CMakeLists.txt`: C17, extensions off, curated warning list, `-Werror` behind `T64_WERROR`, core-only `-Wdouble-promotion -Wframe-larger-than=2048` (both compile warning-free on Apple clang 21), `_POSIX_C_SOURCE` never defined for `t64_core`, and the tool glob so later plans add `tools/<name>/` without editing the root file.
- `CMakePresets.json` (version 6): `gate` configure/build/test/workflow presets; `noTestsAction: error` verified (`ctest --preset gate -R nosuchtest` exits 8).
- `core/` holds exactly `core/include/tendies64/t64_facade.h` and `core/t64_core_stub.c`. The facade matches the interfaces block (`T64_STOP_BOUND/HALT`, `t64_advance_result`, `t64_facade_ops`, `t64_core_identity`).
- `tools/common`: SHA-256, UTF-8 validator and canonical JSON writer, with `unit.common` at 122 checks and 0 failures.
- `scripts/gate.sh` (mode 100755, `set -eu`, `LC_ALL=C`): banner (uname, cc, cmake, ninja, git, awk, each non-fatal), `sh-n` (zero scripts is a failure), `configure`, `build`, `ctest`, timing summary, `[gate] total Ns`, `[gate] PASS`. Nothing branches on `CI`.

## Measured results

| Item | Value |
|------|-------|
| Test executable | `build/gate/tools/common/test_common` |
| Summary line | `unit.common: checks=122 failures=0` (acceptance floor: 20 checks) |
| `./scripts/gate.sh` total (clean `build/gate`, Darwin arm64, Apple clang 21.0.0, CMake 4.4.3) | `[gate] total 1s` (steps: sh-n 0s, configure 0s, build 0s, ctest 1s) |
| Targets | 30 s per-task feedback and under 60 s full gate: both met with wide margin |
| `grep -c '^build/$' .gitignore` | 1 |
| `git ls-files -s scripts/gate.sh` | mode 100755 |

The 1 s total is a single warm-machine measurement on a tiny tree; it will grow as plans 01-02 onward add targets, and hosted-runner timing is first measured in plan 01-13.

Mutation sanity check (scratch copies, not committed): relaxing the E0 overlong bound and the equal-key order check each made `unit.common` report `failures=1`, so those vectors are able to fail.

## Task Commits

1. Task 1 (tracer): `bed5d9c` - gate, presets, core stub, `t64_common` SHA-256, vector test
2. Task 2: `9d09010` - UTF-8 validator, canonical JSON writer, `tests/t64_test.h`, `.gitignore`

Tracer feedback gate (auto mode): `<verify>` re-run end to end after Task 1, passed, expanded.

## Deviations from Plan

None - plan executed exactly as written.

Two interpretation notes, neither a conflict with a measured fact:
- The `.gitignore` entry for `build/` is added in Task 2 as the plan lists, so `build/` was briefly untracked between the Task 1 run and the Task 2 commit. No build output was committed.
- `t64_sha256_hex` takes a 32-byte digest and writes 64 lowercase hex characters plus NUL (the interfaces block does not fix its signature).

## Known Stubs

`core/t64_core_stub.c` is an intentional placeholder under CONTEXT A-1 (no machine behavior; exists so the lint never scans zero files). Behavioral core code waits on ADR-0002 acceptance; this is not a goal-blocking stub and no WINDOWS ledger entry was needed.

## Threat Flags

None. Threat register items covered: T-01-01 (accept; FIPS vectors pin correctness), T-01-02 (mitigated: bounded buffer, ascending keys, UTF-8 validation, 2^53 bound, all with negative tests), T-01-03 (mitigated: curated warnings, `T64_WERROR` option), T-01-SC-01 (no package installs, nothing vendored).

## Notes for later plans

- Gate step names: `banner` (printed, not timed), `sh-n`, `configure`, `build`, `ctest`, summary. Plan 01-03 inserts `lint-selftest` before configure and `core-lint` after ctest; 01-09 adds the single `CI=true` block.
- Later test targets need `target_include_directories(... PRIVATE ${CMAKE_SOURCE_DIR}/tests)` to include `t64_test.h`.
- `sh-n` currently checks only `scripts/gate.sh`; it picks up new `*.sh` under `scripts/`, `tools/` and `tests/` automatically.
- Local CMake is 4.4.3; the 3.25 floor and gcc 13 / clang 18 acceptance of the core-only flags are confirmed by CI in plan 01-13, not here.

## Self-Check: PASSED

- FOUND: CMakeLists.txt, CMakePresets.json, scripts/gate.sh, core/t64_core_stub.c, core/include/tendies64/t64_facade.h, tools/common/{CMakeLists.txt,t64_sha256.[ch],t64_utf8.[ch],t64_jsonw.[ch],tests/test_common.c}, tests/t64_test.h
- FOUND commits: bed5d9c, 9d09010 (both ancestors of HEAD)
