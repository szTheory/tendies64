---
phase: 11-installed-consumer-and-freeze-candidate
plan: 02
subsystem: api
tags: [abi, c-api, t64_api, extern-c, status-classes, selftest, symbol-visibility]

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: plan 11-01 decision record (DR-01 to DR-03, DR-15, DR-16, DR-21), Wave 0 base, scripted preflight and the PHASE11 regex table
provides:
  - public header with T64_API on all 31 public functions, extern "C" guards, stdbool.h only for C, T64_ABI_VERSION_MAJOR 0 / MINOR 1 / T64_ABI_VERSION and uint32_t t64_abi_version(void)
  - statuses 34 to 38 (ABI_MISMATCH, STRUCT_TOO_SMALL, REENTRANT, POISONED, INTERNAL_DEFECT) appended after 33 with no shipped value moved
  - docs/abi/status-classes.txt, the class table (39 statuses and 8 stop reasons, one row each), and scripts/check-abi.py with header mode, classes mode and a ten-case seeded selftest
  - CTest names repo.abi-header, repo.abi-status-classes, repo.abi-selftest and unit.core.api.abi-version (PHASE11_TESTS block, inventory 254 to 258)
  - PIN-IMPACT.md "Shape steps" table started, with the 11-02 row (sweep 0 differences)
affects: [11-03, 11-04, 11-05, 11-11, 11-12, 11-13, 11-16, 11-17, 11-23]

actuals:
  tokens: 9540
  tasks: 3
  commits: 3
plan_head_before: 137fc0ad6d1d8eefb7054b575955956486db7ecd
plan_head_after: c1362fcc8392fdca14a13009ab45bfb0872f9015

tech-stack:
  added: []
  patterns:
    - "tracer through every shape-wave layer (header, core, in-tree public-only test, ABI checker, inventory) before the expansion tasks"
    - "closed status-class table checked against the header, with seeded selftest cases run on temporary copies under build/ and a scratch-copy mutation of the checker to prove the selftest itself can fail"
    - "script-only CTest registrations in tools/abi picked up by the tools glob, each with LABELS repo;phase11, TIMEOUT and COST 1"

key-files:
  created:
    - core/tests/test_api_surface.c
    - docs/abi/status-classes.txt
    - scripts/check-abi.py
    - tools/abi/CMakeLists.txt
  modified:
    - core/include/tendies64/t64_machine.h
    - core/t64_machine.c
    - core/CMakeLists.txt
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase11-regexes.txt
    - .planning/phases/11-installed-consumer-and-freeze-candidate/PIN-IMPACT.md

key-decisions:
  - "T64_ABI_VERSION is spelled ((T64_ABI_VERSION_MAJOR << 16) | T64_ABI_VERSION_MINOR), a constant expression of the two UINT32_C values (DR-02); 0x00000001 for 0.1"
  - "T64_API is not overridable (no #ifndef guard): a host-defined export macro would be a second public surface that the freeze record would have to bind"
  - "The five new statuses carry a class comment line each in the header and are classified in docs/abi/status-classes.txt: ABI_MISMATCH, STRUCT_TOO_SMALL, REENTRANT refused; POISONED, INTERNAL_DEFECT defect"
  - "SEQUENCE_EXHAUSTED is the one refused row with RESUME terminal (permanent until the machine is reset), following the class-table inventory of 11-01"

patterns-established:
  - "Every new Phase 11 CTest name is added to the PHASE11_TESTS block and the preflight regex row in the same commit that registers it, with LABELS <kind>;phase11, a COST of the measured native seconds rounded up and a TIMEOUT at least 3.5 times those seconds"
  - "A checker that can fail ships a seeded selftest whose expected finding text is distinct per case"

requirements-completed: []

coverage:
  - id: D1
    description: "Public header carries T64_API on every public function, extern C guards, the ABI version constants and t64_abi_version(), and compiles alone as C99 pedantic and C++17 pedantic with -Werror"
    requirement: "API-01"
    verification:
      - kind: unit
        ref: "ctest -R '^(unit\\.core\\.api\\.abi-version|repo\\.abi-header)$' (2 of 2 passed); grep -c '^T64_API ' prints 31; grep -c 'extern \"C\"' prints 1"
        status: pass
    human_judgment: false
  - id: D2
    description: "Statuses 34 to 38 appended after 33 with no shipped status or stop-reason value changed, and the status-class table lists all 39 statuses and 8 stop reasons exactly once"
    requirement: "API-02"
    verification:
      - kind: integration
        ref: "ctest -R '^repo\\.abi-status-classes$'; python3 -B scripts/check-abi.py classes (constants=47 rows=47 findings=0); git diff shows no removed #define line"
        status: pass
    human_judgment: false
  - id: D3
    description: "The class-table check fails on an unclassified, duplicated, renumbered, orphan or malformed row and on an unknown class or resume value, each proven by a seeded selftest case; the selftest itself fails against a weakened checker"
    requirement: "API-02"
    verification:
      - kind: integration
        ref: "ctest -R '^repo\\.abi-selftest$' (10 cases ok); RED report build/gate/11-02-task2/red.txt (10 of 10 failing before classes mode); mutation of a scratch copy of the checker failed 4 of 10 cases"
        status: pass
    human_judgment: false
  - id: D4
    description: "The additive surface moves nothing observable: sweep against the Wave 0 base reports 0 differences, sealed bodies and the no-HLE checker unchanged"
    requirement: "API-03"
    verification:
      - kind: integration
        ref: "python3 -B scripts/digest-sweep.py --base f9124ae9fce134f31b645c6f2911a6f27fa41b9a --head HEAD (1599 shared lines, 0 differences); python3 -B scripts/check-rsp-no-hle.py --build build/gate exit 0; sh scripts/preflight-phase11.sh PASS"
        status: pass
    human_judgment: false

duration: 15min
completed: 2026-11-25
status: complete
---

# Phase 11 Plan 02: Additive Public Surface and Status-Class Table Summary

**T64_API plus extern "C" guards and a 0.1 ABI version query on the public header, statuses 34 to 38 appended after 33, and a 47-row status-class table whose checker provably fails on an unclassified or renumbered constant; the digest sweep against the Wave 0 base stays at 0 differences.**

## Performance

- **Duration:** 15 min
- **Started:** 2026-11-25T19:36:34Z
- **Completed:** 2026-11-25T19:51:49Z (PIN-IMPACT row committed; this summary and the state update follow)
- **Tasks:** 3 of 3
- **Files modified:** 10 (4 created, 6 modified, counting the PIN-IMPACT record)

## Accomplishments

- Tracer (Task 1): one symbol, `t64_abi_version`, travels through every layer the shape wave touches. The header gained `T64_API` (default visibility under GCC and Clang, empty elsewhere) on all 31 public declarations, `extern "C"` guards, `stdbool.h` only when `__cplusplus` is not defined, and the constants `T64_ABI_VERSION_MAJOR` 0, `T64_ABI_VERSION_MINOR` 1 and `T64_ABI_VERSION`. `core/t64_machine.c` defines the query next to `t64_build_identity`. `core/tests/test_api_surface.c` (public header and `tests/t64_test.h` only) checks the query against the constants and the build identity prefix `t64-build-v1-`. `scripts/check-abi.py header` compiles the header alone as C99 and C++17 with `-pedantic -Wall -Wextra -Werror -fsyntax-only` (`c99 ok`, `c++17 ok`); seeded broken headers (a missing include, a C++ keyword as a member) were reported by it and a missing C++ compiler is exit 2. The tracer gate re-ran the automated verify end to end and passed, so expansion proceeded.
- Statuses and table (Task 2): `T64_STATUS_ABI_MISMATCH` 34, `STRUCT_TOO_SMALL` 35, `REENTRANT` 36, `POISONED` 37 and `INTERNAL_DEFECT` 38 follow `STATE_INVARIANT` 33 with a class comment line each. `docs/abi/status-classes.txt` holds 39 status rows and 8 stop-reason rows in the format `NAME :: VALUE :: CLASS :: RESUME :: note` with the CONTEXT decisions (UNSUPPORTED_ROM_ORDER and STOP_REASON_HALT reserved, RESOURCE_LIMIT resumable after drain or yield with the permanent-stall note, QUIESCENT a guest-stop, CPU_UNSUPPORTED a terminal guest-stop that neither poisons nor claims the guest chose to stop). `check-abi.py classes` reports unclassified, duplicate, renumbered, orphan, malformed, unknown-class and unknown-resume findings, and ten seeded selftest cases (one baseline-clean plus nine breakages, each with its own finding text) prove each one. The RED report (`build/gate/11-02-task2/red.txt`) shows 10 of 10 cases failing first with the table absent and then with the table present but no classes mode; the GREEN report is beside it.
- Proof (Task 3): the two-tree sweep against the Wave 0 base reports `1599 shared lines, 0 differences, 0 head-only lines`; `harness.rdp-oracle-authority`, `repo.si-membership-sites`, `harness.state-field-use-audit`, `repo.p10-mutant-anchors`, `repo.rsp-no-hle` and the six qualification tests pass; `check-rsp-no-hle.py --build build/gate` exits 0 with the checker unchanged; the full `sh scripts/preflight-phase11.sh` (229-unit gcc 13.3.0 compile and the post-gate replay included) reports PASS in 77 s. PIN-IMPACT.md has a "Shape steps" section with the 11-02 row.

## Task Commits

1. **Task 1: Tracer, T64_API, extern "C", ABI constants and t64_abi_version** - `df1f668` (feat)
2. **Task 2: Append statuses 34-38 and bind the status-class table** - `a396fa9` (feat)
3. **Task 3: Record the additive surface sweep in PIN-IMPACT shape steps** - `c1362fc` (docs)

**Plan metadata:** the summary and state commit that follows this file (docs: complete plan). The measured `commits:` count of 3 is `git rev-list --count 137fc0a..c1362fc` at write time.

## Files Created/Modified

- `core/include/tendies64/t64_machine.h` - T64_API, extern "C", ABI constants, `t64_abi_version`, statuses 34 to 38
- `core/t64_machine.c` - `t64_abi_version` definition
- `core/tests/test_api_surface.c` - public-header-only test, case `abi-version` (new)
- `core/CMakeLists.txt` - `test_api_surface` target and `unit.core.api.abi-version`
- `docs/abi/status-classes.txt` - class table golden (new)
- `scripts/check-abi.py` - header mode, classes mode, selftest (new)
- `tools/abi/CMakeLists.txt` - the three `repo.abi-*` registrations (new)
- `scripts/check-test-inventory.sh` - `PHASE11_TESTS` block and the `ALL` join
- `scripts/preflight-phase11-regexes.txt` - rows `^repo\.abi` 3 and `^unit\.core\.api\.` 1
- `.planning/phases/11-installed-consumer-and-freeze-candidate/PIN-IMPACT.md` - "Shape steps" section

## Decisions Made

Binding decisions are DR-01 to DR-03, DR-15, DR-16 and DR-21 of PIN-IMPACT.md; this plan implemented them without amendment. Choices inside the discretion left by the plan are in the frontmatter (the `T64_ABI_VERSION` spelling, no override of `T64_API`, SEQUENCE_EXHAUSTED as the one terminal refused row).

## Deviations from Plan

### Auto-fixed Issues

None. Rules 1 to 3 did not fire.

### Plan text applied with additions or adjustments (recorded for transparency)

- **COST property on the four new names.** The task text sets LABELS and TIMEOUT only; DR-21 (the standing rule) requires a COST equal to the measured native seconds rounded up, so each name carries `COST 1` (measured 0.00 to 0.94 s). TIMEOUT is 30 for the unit test and 60 for the three repo checks, above 3.5 times the measured seconds and 2 times COST; the preflight `timeout-audit` passes (111 TIMEOUTs, 0 too low).
- **Forward reference in the class table.** The plan wording for `BUS_DEVICE_REFUSED` cites `docs/spec/api.md`, which does not exist yet (plan 11-11 writes the ABI rules there). The row cites the header comment of `t64_machine_bus_write` as the existing source and names `docs/spec/api.md` as where the ABI rules land; the table is re-bindable until the final bind.
- **Three extra selftest cases and two extra finding kinds.** Beyond the plan's list (unclassified-status, duplicate-row, renumbered-value, removed-row, orphan-row, unknown-class, baseline-clean) the checker also reports `unknown-resume` and `malformed` rows, and the selftest has `unclassified-stop-reason`, `unknown-resume` and `malformed-row`; each finding kind has its own seeded case, as the plan's truth requires.
- **One commit per task.** Task 2 is `tdd="true"`, but the plan specifies a single commit with the RED report saved under `build/gate/11-02-task2/`; the RED evidence is that report (two RED runs) rather than a separate test commit.

**Total deviations:** 0 auto-fixed, 4 recorded adjustments. **Impact:** none on scope; every plan truth and acceptance criterion holds.

## Authentication Gates

None.

## Issues Encountered

- The host was busy (load 10 to 15, mostly this plan's own builds and the preflight replay); every measured check is a sweep or a pass/fail test, none a wall-time gate, so no limit was widened. The first preflight run printed a stale-build warning because a `CMakeLists.txt` was newer than `build.ninja`; a rebuild cleared it and the later runs were clean.
- zsh does not expose `PIPESTATUS`, so one build exit status printed empty; the build was re-run and reported `no work to do`.

## Verification Results

| Check | Result |
|-------|--------|
| `cmake --build --preset gate` | builds clean (273 steps after the header change) |
| `ctest -R '^(unit\.core\.api\.abi-version\|repo\.abi-header)$'` | 2 of 2 passed (0.00 s, 0.09 to 0.16 s) |
| `ctest -R '^repo\.abi-(status-classes\|selftest)$'` | 2 of 2 passed (0.03 to 0.08 s, 0.45 to 0.94 s) |
| `python3 -B scripts/check-abi.py classes --header ... --table ...` | `constants=47 rows=47 findings=0` |
| `python3 -B scripts/check-abi.py --selftest` | 10 cases ok; 10 of 10 failed before classes mode (RED); 4 of 10 fail against a weakened scratch copy of the checker |
| `grep -c '^T64_API '` / `grep -c 'extern "C"'` / `grep -c '^#define T64_STATUS_'` / table rows | 31 / 1 / 39 / 47 |
| header `git diff` removed `#define` lines | none (no value 0 to 33, no stop reason 1 to 8 changed) |
| `sh scripts/check-test-inventory.sh --preset gate` and `--selftest` | PASS, count 258 (254 + 4); selftest 518 removal controls |
| `sh scripts/preflight-phase11.sh` (full, with replay) | PASS, total 77 s (linux-compile 229 units 44 s, post-gate-replay 31 s) |
| two-tree digest sweep against the Wave 0 base | `1599 shared lines, 0 differences, 0 head-only lines` |
| `harness.rdp-oracle-authority`, `repo.si-membership-sites`, `harness.state-field-use-audit`, `repo.p10-mutant-anchors`, `repo.rsp-no-hle` | 5 of 5 passed |
| `python3 -B scripts/check-rsp-no-hle.py --build build/gate`; checker `git diff` | exit 0; checker unchanged |
| all `repo.*` (12) and all `*qualification` (6) tests | passed |
| `sh scripts/pii-scan.sh --tree` | findings=0 |

## Known Stubs

None.

## Threat Flags

None. The new exported surface is the symbol-visibility attribute and one read-only query; no network, auth, file or schema surface was added. T-11-04 (header and library skew) is mitigated up to the planned point (the constants and the query exist; the create-time handshake is plan 11-12 and the doctored-header controls 11-19), T-11-05 by `repo.abi-status-classes` with the seeded selftest, and T-11-06 by `repo.abi-header`.

## Next Phase Readiness

Plan 11-03 (poison and in-call members, the defect sites) can return the names 36 to 38 that now exist and are classified. Plans 11-12 and 11-13 return 34 and 35. Later plans that add a status or stop reason must add its table row in the same commit (the check fails otherwise) and raise `^repo\.abi` or `^unit\.core\.api\.` rows when they register names. No owner decision is pending and nothing in this plan is orchestrator-only.

## Self-Check: PASSED

- FOUND: core/tests/test_api_surface.c, docs/abi/status-classes.txt, scripts/check-abi.py, tools/abi/CMakeLists.txt, core/include/tendies64/t64_machine.h, PIN-IMPACT.md "Shape steps"
- FOUND commits: df1f668, a396fa9, c1362fc (all ancestors of HEAD)
