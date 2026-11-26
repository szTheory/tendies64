---
phase: 11-installed-consumer-and-freeze-candidate
plan: 16
subsystem: build
tags: [cmake-package, install, relocation, installed-consumer, sha256-fold, consumer-guard, hosted-linux, tracer]

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: the shape wave (public header, t64i_ prefix header, handshake, init guard) and the SHAPE review of plan 11-15 with the Install base
provides:
  - sha256 folded into t64_core (no private link), core copy renamed t64i_sha256_* by private compile definitions, so install(EXPORT) works and host tools keep t64_sha256_*
  - relocatable package tendies64 0.1.0 (SameMinorVersion, component Development, six files) with tendies64_BUILD_ID and tendies64_BUILD_TYPE in the config and the in-tree alias tendies64::core
  - scripts/check-install.sh (setup, build, run) and the independent consumer tests/installed-consumer (find_package from a moved prefix with spaces; smoke case asserts ABI version, build identity and allocation balance)
  - CTest names install.setup, installed.consumer.build, installed.consumer.smoke (fixtures t64_install and t64_consumer) and repo.consumer-include-guard (+ selftest)
  - repo.api-init-guard without the oracle-bound exclusion, scanning the consumer root
  - first hosted install-from-build-tree pair on macOS and Linux (run 45742744889, both success) and the act-image Linux undefined set for plan 11-17
affects: [11-17, 11-18, 11-19, 11-20, 11-21]

actuals:
  tokens: 11038
  tasks: 4
  commits: 8

plan_head_before: 88c2bf9beaf2236bff204cb9c61173983fd2b16c
plan_head_after: 55194cc76fb3f890fbff4eeb80b945b7a46d17fc

tech-stack:
  added: []
  patterns:
    - "the installed package is proved only through find_package from a prefix that was moved after install and whose path contains spaces; the source tree is invisible to the consumer (separate top-level project, include guard, original-path grep)"
    - "a folded third-party-style source keeps its host-tool names and gets the t64i_ names for the core copy through private compile definitions on the one target, so the generated prefix header does not change"
    - "CTest -R adds the fixture setup tests of every matched test, so preflight count rows for fixture-bearing families include the setup names"

key-files:
  created:
    - core/tendies64Config.cmake.in
    - scripts/check-install.sh
    - scripts/check-consumer-includes.sh
    - tests/installed-consumer/CMakeLists.txt
    - tests/installed-consumer/main.c
    - tests/installed-consumer/t64c_check.h
  modified:
    - core/CMakeLists.txt
    - scripts/check-rsp-no-hle.py
    - scripts/check-api-init.py
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase11-regexes.txt
    - scripts/preflight-phase11.sh
    - tools/abi/CMakeLists.txt
    - docs/ci-reviews/LOG.md
    - .planning/phases/11-installed-consumer-and-freeze-candidate/PIN-IMPACT.md

key-decisions:
  - "The sha256 rename uses the four private compile definitions of RESEARCH, not the generator: core/t64_prefix.h stays at 35 public functions and 220 renames, and the archive carries 224 t64i_ names (220 plus the four sha256 names)"
  - "The package version is the literal 0.1.0 with SameMinorVersion (the ABI candidate number); project(VERSION) stays 0.0.0"
  - "The consumer carries its own counting allocator (aligned_alloc, per-instance ledger) and check macros, includes only <tendies64/t64_machine.h> and C system headers, and is compared with tendies64_BUILD_ID, which the installed config records"
  - "Only setup, build and run are implemented in check-install.sh: the plan's action lists exactly those; the control subcommand named in the artifact line belongs to the negative-control plan 11-19"
  - "tests/installed-consumer is named explicitly in the init guard's default roots although tests already contains it (185 files scanned either way), so a later narrowing of the roots cannot drop a consumer"

patterns-established:
  - "a plan that registers fixture-bearing CTest names counts the setup closure in its preflight regex rows"

requirements-completed: []

coverage:
  - id: D1
    description: "sha256 folded into t64_core and a relocatable tendies64 package (six files, SameMinorVersion 0.1.0, build id and type recorded) installable from the existing gate tree"
    requirement: VER-05
    verification:
      - kind: integration
        ref: "ctest --test-dir build/gate -R '^(install\\.setup|installed\\.consumer\\.(build|smoke))$' (3 of 3, local and both hosted jobs); python3 -B scripts/gen-prefix-header.py --check build/gate/core/libt64_core.a (35 public functions, 220 renames); python3 -B scripts/check-rsp-no-hle.py --build build/gate (0 findings after the isolated refresh)"
        status: pass
    human_judgment: false
  - id: D2
    description: "An independent consumer finds the moved package with spaces in its path and proves header, library and build identity agree and that create/destroy balances every allocation"
    requirement: VER-05
    verification:
      - kind: integration
        ref: "scripts/check-install.sh build then run smoke: smoke checks=13 failures=0; no file of the moved prefix or the consumer tree names the original install path"
        status: pass
    human_judgment: false
  - id: D3
    description: "Consumer independence and init forms are guarded in the gate"
    requirement: VER-05
    verification:
      - kind: unit
        ref: "ctest -R '^repo\\.(consumer-include-guard|consumer-include-guard-selftest|api-init-guard)$' (3 of 3); sh scripts/check-consumer-includes.sh --selftest (12 seeds reported, 4 clean controls, empty scan exit 2); python3 -B scripts/check-api-init.py --selftest 19 of 19; RED build/gate/11-16-task2/red.txt (12 seeded cases undetected with the rules stubbed)"
        status: pass
    human_judgment: false
  - id: D4
    description: "Install-from-build-tree path proven on hosted macOS and Linux inside the existing gate step"
    requirement: VER-05
    verification:
      - kind: integration
        ref: "ci.yml run 45742744889 at head e5a2e3c8ad11d40991c0c2048999d339261e3146: gate-linux 279 s and gate-macos 496 s, both success, 280 of 280 names; LOG F-128"
        status: pass
    human_judgment: false
  - id: D5
    description: "The Linux undefined set measured in the act image is the input of plan 11-17 and is not yet confirmed by a hosted measurement"
    verification:
      - kind: other
        ref: "PIN-IMPACT 'Install steps': 5 names from the act-image object compile (86 objects, 35 unprefixed and 224 t64i_ globals)"
        status: pass
    human_judgment: true
    rationale: "A measurement in the act image (gcc 13.3.0, amd64 emulation); the hosted gate-linux run of 11-21 must confirm or refute it before docs/abi/installed-undefined.txt is bound"

duration: 28min
completed: 2026-11-26
status: complete
---

# Phase 11 Plan 16: INSTALL tracer Summary

**A package installed from the existing gate tree into a prefix with spaces, moved, then found, linked and run by an independent consumer that asserts the ABI version, the build identity and a balanced create/destroy, proven on hosted macOS and Linux (run 45742744889, both success), with the consumer's independence guarded by a seeded rule and the Linux undefined set measured for plan 11-17.**

## Performance

- **Duration:** about 28 min of work (about 7 min waiting for builds at host load 10 to 20 and about 8 min for the hosted run)
- **Started:** 2026-11-26T02:10Z
- **Completed:** 2026-11-26T02:40Z
- **Tasks:** 4 of 4
- **Files modified:** 15 (6 created, 9 modified)

## Accomplishments

- **Fold and export (Task 1, tracer).** `tools/common/t64_sha256.c` is a `t64_core` source and the private link of `t64_sha256` is gone (`grep -c` 0), so `install(EXPORT)` succeeds. The four sha256 names of the core copy are renamed `t64i_sha256_*` by private compile definitions on `t64_core` only, so `core/t64_prefix.h` and `scripts/gen-prefix-header.py` did not change (`--check`: 35 public functions, 220 renames) and host tools keep linking `t64_sha256`. The only link fallout was `test_tracer` and `test_isviewer`, which called `t64_sha256_*` through the old transitive link and now list `t64_sha256`. The package is `lib/libt64_core.a`, `include/tendies64/t64_machine.h` and `tendies64{Config,ConfigVersion,Targets,Targets-release}.cmake`, component `Development`, version `0.1.0` `SameMinorVersion` (a literal; `project(VERSION 0.0.0)` untouched, count 1), config records `tendies64_BUILD_ID` and `tendies64_BUILD_TYPE`, alias `tendies64::core`.
- **Isolated inventory refresh.** `scripts/check-rsp-no-hle.py` reported exactly three findings, all about the one object `core/CMakeFiles/t64_core.dir/__/tools/common/t64_sha256.c.o`; the second commit adds that one expected object edge, archive member and on-disk object and the checker reports none (`repo.rsp-no-hle` passes, 18 s local). No body digest, pin or seed moved.
- **Driver and consumer.** `scripts/check-install.sh setup` runs `cmake --install ... --component Development` into `prefix with space`, moves it to `moved prefix` and asserts exactly the six files; `build` configures `tests/installed-consumer` (Ninja, Release, the gate compiler, compile commands on) against the moved prefix and asserts that neither the moved prefix nor the consumer tree mentions the original install path; `run smoke` executes it (13 checks, 0 failures: ABI version equals `T64_ABI_VERSION`, `t64_build_identity()` equals the config's `tendies64_BUILD_ID`, create and destroy through a counting allocator leave outstanding 0 and no bad release). The moved prefix holds no `/Users/`, `/private/` or `/var/folders` string.
- **Guards (Task 2).** `scripts/check-consumer-includes.sh` (rules CONSUMER-INCLUDE and CONSUMER-CMAKE) with a selftest of 12 seeded violations (the three mandatory seeds, an angle-bracket internal include, a quoted public path, a `..` public path, a computed include, a denied local header, and four CMake leaks), four clean controls and the empty-scan error. RED first: with both rules stubbed all 12 seeded cases passed undetected (`build/gate/11-16-task2/red.txt`, ignored). `repo.api-init-guard` lost `--exclude-oracle-bound` (0 findings over 185 files including the nine bound files) and names the consumer root; two selftest cases seed it (19 of 19).
- **Linux pre-flight (Task 3).** Full `sh scripts/preflight-phase11.sh` PASS (linux-compile 233 units under gcc 13.3.0). In the act image the 86 core objects (85 core and SoftFloat plus the folded sha256) define 259 globals, 35 unprefixed and 224 `t64i_`; `gen-prefix-header.py --check` on the Linux archive is `ok`; the archive-level Linux undefined set is `_GLOBAL_OFFSET_TABLE_`, `__stack_chk_fail`, `memcmp`, `memcpy`, `memset` (recorded in PIN-IMPACT "Install steps" for plan 11-17).
- **Push and hosted pair (Task 4).** Pushed `e5a2e3c8ad11d40991c0c2048999d339261e3146` (fast-forward from `43b31e2`, 20 commits, no tag) after both PII scans, a green preflight and a green full gate (280 of 280, 176 s). Hosted run 45742744889: gate-linux 279 s, gate-macos 496 s, both success. On gate-linux the six files installed under `lib/`, the moved prefix was found, the consumer linked and ran, `repo.abi-prefix` passed on the ELF archive and `repo.rsp-no-hle` passed with the folded member. LOG Runs row and F-128, and the INSTALL tracer row of the Gate-growth ledger, are recorded.

## Task Commits

1. **Task 1 (tracer), commit A: sha256 fold and package export** - `31df5de` (feat)
2. **Task 1, commit B: isolated no-HLE inventory refresh** - `c197e4c` (chore)
3. **Task 1, commit C: install driver, consumer, install CTest names** - `904b55c` (feat)
4. **Task 2: consumer independence guard and init guard over every consumer** - `ea83392` (feat)
5. **Deviation: preflight row count for the fixture closure** - `0213aba` (fix)
6. **Deviation: preflight guards default** - `dd271a1` (fix)
7. **Task 3: PIN-IMPACT "Install steps", Linux pre-flight and undefined set** - `e5a2e3c` (docs)
8. **Task 4: hosted evidence, LOG row and F-128, ledger row** - `55194cc` (docs)

## Verification

| Check | Result |
|-------|--------|
| `cmake --build --preset gate && python3 -B scripts/check-rsp-no-hle.py --build build/gate && ctest -R '^(install\.setup\|installed\.consumer\.(build\|smoke))$'` | build 0, checker 0 findings, 3 of 3 passed |
| `grep -c -F 'set(T64_BUILD_ID ...)' core/CMakeLists.txt` and `python3 -B scripts/digest-sweep.py --selftest` | 1; selftest PASS |
| `grep -c -F 'target_link_libraries(t64_core PRIVATE t64_sha256)' core/CMakeLists.txt` | 0 (was 1) |
| COST on all three install names (`ctest --show-only=json-v1`) | 0.05, 0.43, 0.02 (also TIMEOUT 60, 120, 60, labels, fixtures) |
| `grep -c 'VERSION 0.0.0' CMakeLists.txt` | 1 |
| `sh scripts/check-consumer-includes.sh`, `--selftest`, `python3 -B scripts/check-api-init.py` | files=3 findings=0; failures=0; files=185 findings=0 |
| `ctest -R '^repo\.(consumer-include-guard\|consumer-include-guard-selftest\|api-init-guard)$'` | 3 of 3 passed |
| `grep -c 'exclude-oracle-bound' tools/abi/CMakeLists.txt` | 0 |
| `sh scripts/preflight-phase11.sh` (clean committed tree, twice) | PASS (77 s and 66 s at load 12 to 16) |
| `sh scripts/pii-scan.sh --tree` and `--range origin/main..HEAD` | findings=0 and findings=0 (20 commits) |
| `sh scripts/gate.sh` at `e5a2e3c` | PASS, 280 of 280, ctest 102 s, total 176 s |
| Hosted `ci.yml` run for the pushed SHA | `completed success` on both jobs (run 45742744889) |
| `gh run` install names, gate-macos / gate-linux | install.setup 0.46 s / 0.04 s, consumer build 1.74 s / 0.30 s, smoke 0.06 s / 0.02 s |

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Two tests lost their transitive sha256 link**
- **Found during:** Task 1 (full gate build after the fold)
- **Issue:** `test_tracer` and `test_isviewer` call `t64_sha256_*` and linked it only through the old `PRIVATE` link of `t64_core` (a static library passes it on as link-only); with the sources folded the symbols are no longer exported, so both failed to link (`_t64_sha256_init` and friends undefined).
- **Fix:** `target_link_libraries(test_tracer PRIVATE t64_core t64_sha256)` and the same for `test_isviewer` in `core/CMakeLists.txt`. They were the only two link failures of `ninja -k 0`.
- **Files modified:** `core/CMakeLists.txt`
- **Commit:** `31df5de`

**2. [Rule 1 - Plan count] The `^installed\.consumer\.` preflight row is 3, not 2**
- **Found during:** Task 3 (first full preflight)
- **Issue:** `ctest -N -R '^installed\.consumer\.'` lists `install.setup` as well, because CTest adds the setup test of every required fixture; the plan's count of 2 ignored the `t64_install` fixture, so `ctest-counts` failed.
- **Fix:** row set to 3 with a comment explaining the closure; the two names and their registration are unchanged.
- **Files modified:** `scripts/preflight-phase11-regexes.txt`
- **Commit:** `0213aba`

**3. [Rule 1 - Bug] Preflight guards step still ran the init guard with the removed flag**
- **Found during:** Task 3 (preflight output `check-api-init.py passed (--exclude-oracle-bound)`, 11 fewer files)
- **Issue:** `run_guard` uses the registered arguments and falls back to a default when they are empty; the registration is now flagless, so the old default silently restored the weaker scan.
- **Fix:** the default is now no argument; comment updated. `scripts/preflight-phase11.sh` is not in the plan's file list.
- **Files modified:** `scripts/preflight-phase11.sh`
- **Commit:** `dd271a1`

**4. [Plan reading] The `control` subcommand is not implemented**
- The artifact line lists "setup, build, run, control subcommands" while the task action lists setup, build and run only; the negative controls (missing package, version mismatch, doctored header, collision) belong to plan 11-19. No control subcommand was added; the driver is ready to take one.

**Total deviations:** 3 auto-fixed (1 blocking, 2 bug or count) and 1 plan reading. **Impact:** none on scope or evidence tier; all within the plan's file families.

### Notes that are not deviations

- The rename mechanism was open in the plan ("the 11-11 mechanism ... or the four private compile definitions of the recipe; record the choice"); the compile definitions were chosen (see key-decisions).
- `tests/installed-consumer` was already under the default scan root `tests` of the init guard; naming it explicitly changes no scanned file count.
- The hosted run does not print the archive-level undefined set, so the Linux table of PIN-IMPACT stays an act-image measurement.

## Authentication Gates

None.

## Issues Encountered

- Host load stayed between 10 and 20 (unrelated VM and Docker work); no limit was widened and no run failed on a wall backstop.
- Each commit changes `T64_SOURCE_REVISION`, so the first build after a commit recompiles the core units; builds were batched between commits.

## Known Stubs

None. No placeholder, empty-value or mock data flows into any file this plan created.

## Threat Flags

None. `scripts/check-install.sh` writes only under the build tree (`BUILD/install-check`, refuses the repository root, the build root and the home directory as a work directory); the consumer reads no environment, network or file.

## Threat Model Disposition

| Threat | Result |
|--------|--------|
| T-11-27 (installed files leak a path) | relocation by move plus the original-path grep of the moved prefix and the consumer tree passes locally and hosted; no `/Users/`, `/private/` or `/var/folders` string in the moved prefix; the absolute-path scan CTest is plan 11-17 |
| T-11-28 (consumer independence) | `repo.consumer-include-guard` with a 12-seed selftest; the consumer is never `add_subdirectory`'d |
| T-11-29 (header or library identity) | the smoke case compares `t64_build_identity()` with `tendies64_BUILD_ID` and `t64_abi_version()` with `T64_ABI_VERSION` |
| T-11-30 (pushed commits) | full preflight pii step, `--tree` and `--range origin/main..HEAD` clean before the push |

## Next Phase Readiness

- **Plan 11-17** writes the Linux rows of `docs/abi/installed-undefined.txt` from the PIN-IMPACT table only (five names) and the macOS rows from the real archive (six names measured here: `__stack_chk_fail`, `__stack_chk_guard`, `bzero`, `memcmp`, `memcpy`, `memset`).
- **Plans 11-17 to 11-21** that count CTest names with `-R` on `installed.*` or `install.*` families must include the fixture setup closure (`install.setup`, and `installed.consumer.build` for names requiring `t64_consumer`).
- **Gate-growth ledger warning (orchestrator).** The INSTALL tracer row records a warm gate-macos job of 496 s (445 s at the SHAPE review, 368 s at F-126). Two of three cold projections are under the 120 s margin warning (861 s with margin 39 s, 842 s with margin 58 s; the third is 763 s); none is above 900 s, so the reversal rule is not triggered. A measured cold run (orchestrator-only) should replace the projections before the 11-21 row.
- **Push state.** `origin/main` is `e5a2e3c`. The local commits after it (`55194cc` and the closeout commits of this plan) are documentation only and go out with the next push a plan names.

## Self-Check: PASSED

- FOUND: core/tendies64Config.cmake.in, scripts/check-install.sh, scripts/check-consumer-includes.sh, tests/installed-consumer/CMakeLists.txt, tests/installed-consumer/main.c, tests/installed-consumer/t64c_check.h
- FOUND commits: 31df5de, c197e4c, 904b55c, ea83392, 0213aba, dd271a1, e5a2e3c, 55194cc (each an ancestor of HEAD; `git rev-list --count` from the base is 8)
