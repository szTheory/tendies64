---
phase: 11-installed-consumer-and-freeze-candidate
plan: 17
subsystem: abi
tags: [abi, layout-pins, nm-golden, api-04, installed-archive, lint-objects, seeded-violations, path-scan]

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: the relocatable tendies64 package, the t64_install fixture (moved prefix with a space) and the act-image Linux undefined set of plan 11-16
provides:
  - generated layout pins (docs/abi/layout.c) compiled against the source and the installed header in C99, C11 and C++17, regenerated and diffed on every run
  - the nm golden of the installed archive (35 public functions, every other global t64i_, no data symbol) and a byte-level absolute-path scan of the installed tree
  - API-04 on the shipped members: ar x from the spaced, moved prefix, lint-objects.sh --allow with the three-row installed allowlist, the archive-level undefined set equal to the recorded per-OS rows, a disassembly scan, a Layer-1 scan of the installed header, and four seeded violations that must fail
  - nine new CTest names inside the existing gate (repo.abi-layout, install.abi-header, install.abi-layout, install.abi-symbols, install.abs-paths, api04.members, api04.disasm, api04.headers, api04.seeded)
affects: [11-18, 11-19, 11-20, 11-21, 11-23, 11-25]

actuals:
  tokens: 23740
  tasks: 4
  commits: 4

plan_head_before: e281ce241b57cd2034326a5bf84378ae530c0e3f
plan_head_after: 371f58cd9d3963e2f2041cba27e8b74160bd946a

tech-stack:
  added: []
  patterns:
    - "a generated pin file is compiled in every language it must hold in (C99 negative-array typedef, C11 _Static_assert, C++17 static_assert) and also regenerated and diffed, because pins alone cannot see an added member or constant"
    - "an archive is checked as extracted members, never through the lint's own compile, because only the members carry the shipped flags; the archive-level undefined set is compared with a recorded per-OS set so an allowlisted but new import still fails"
    - "every check that guards a boundary has a seeded violation that must fail, and a neutered copy of the checker must report those seeds as not detected (the RED)"
    - "the fixture closure of CTest -R counts install.setup in every family that requires t64_install (api04: 2 and 5, not 1 and 4)"

key-files:
  created:
    - docs/abi/layout.c
    - docs/abi/public-symbols.txt
    - docs/abi/installed-undefined.txt
    - scripts/check-api04.sh
    - tools/lint/allowlist-installed.txt
    - tools/lint/installed-seeds/seed_host_services.c
    - tools/lint/installed-seeds/seed_hw_counter.c
    - tools/lint/installed-seeds/seed_banned_include.h
  modified:
    - scripts/check-abi.py
    - tools/lint/lint-objects.sh
    - tools/abi/CMakeLists.txt
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase11-regexes.txt
    - .planning/phases/11-installed-consumer-and-freeze-candidate/PIN-IMPACT.md

key-decisions:
  - "The layout pins use one macro with three branches: C++ static_assert, C11 _Static_assert and, for C99, a negative-array typedef, because _Static_assert is a diagnosed extension under -std=c99 -pedantic -Werror (the plan asked for C99 and C++17); a C11 compile was added so the C11 branch is exercised too"
  - "The path scan reads bytes in Python (the archive included) instead of grep -rl plus strings -a: the same detection of printable path strings with no strings(1) dependency, and the two roots are named by position, never by value, so a path cannot reach a log"
  - "The expected set docs/abi/installed-undefined.txt is the whole archive-level undefined set per OS (allowlisted names included), compared in both directions, so a new allowlisted import and a vanished import both fail"
  - "A stale installed-allowlist row is not a finding: the __stack_chk_guard row is any-platform by the ADR amendment but x86_64 Linux does not import it, so a row-must-be-used check would fail hosted gate-linux"
  - "The Linux rows of the recorded set are copied from the act-image table of 11-16 only and stay unconfirmed until the hosted gate-linux run of plan 11-21"

patterns-established:
  - "a plan that registers fixture-bearing names counts the install.setup closure in its preflight rows (second time: api04.*)"
  - "a neutered copy of a checker (no-op scanners) reporting its seeds as NOT DETECTED is the RED of a checker task"

requirements-completed: []

coverage:
  - id: D1
    description: "Every public struct size, member offset and integer or string-size constant is pinned in a generated file that compiles against the source and the installed header as C99, C11 and C++17 and regenerates identically"
    requirement: API-03
    verification:
      - kind: unit
        ref: "ctest -R '^(repo\\.abi-layout|install\\.abi-header|install\\.abi-layout)$' (4 of 4 with the install.setup closure); check-abi.py --selftest 14 of 14 (layout-offset-edited, layout-header-drift seeds)"
        status: pass
    human_judgment: false
  - id: D2
    description: "The exported surface and the path hygiene of the installed package are pinned (35 public functions, 224 t64i_ names, no data symbol, no private path)"
    requirement: API-03
    verification:
      - kind: unit
        ref: "ctest -R '^install\\.(abi-symbols|abs-paths)$' (3 of 3); check-abi.py --case symbols-controls and abspaths-controls"
        status: pass
    human_judgment: false
  - id: D3
    description: "API-04 holds on the members of the installed archive on macOS: the post-allowlist undefined set equals the six recorded Darwin rows, no hardware clock or entropy instruction exists, and the installed header passes the Layer-1 scan"
    requirement: API-04
    verification:
      - kind: integration
        ref: "ctest -R '^api04\\.(members|disasm|headers)$'; Q6: ar x from the spaced, moved prefix works (86 members, route in-place)"
        status: pass
    human_judgment: false
  - id: D4
    description: "The API-04 checks can fail: seeded clock_gettime, fopen, getentropy and pthread_create imports give four L2-UNDEF-SYMBOL lines, an allowlisted but unrecorded import fails the set comparison, a hardware counter read fails the disassembly scan and a banned include fails L1-INCLUDE-BANNED"
    requirement: API-04
    verification:
      - kind: integration
        ref: "ctest -R '^api04\\.seeded$'; RED build/gate/11-17-task4/red.txt (three of four seeds not detected with the scanners neutered)"
        status: pass
    human_judgment: false
  - id: D5
    description: "The Linux rows of the recorded undefined set are act-image measurements and are not yet confirmed by a hosted measurement"
    requirement: API-04
    verification:
      - kind: other
        ref: "docs/abi/installed-undefined.txt Linux rows; PIN-IMPACT 'Install steps' 11-17 rows; hosted gate-linux run of plan 11-21"
        status: pass
    human_judgment: true
    rationale: "Copied from the act-image table of 11-16 (gcc 13.3.0, x86_64 under emulation); the hosted run of 11-21 confirms or refutes them and a mismatch is a finding for a new numbered plan"

duration: 25 min
completed: 2026-11-26T03:06Z
status: complete
---

# Phase 11 Plan 17: Cheap-first ABI checks and API-04 on the shipped archive Summary

**Generated layout pins compiled against the source and the installed header in three languages and regenerated on every run, an nm golden and a byte-level path scan of the installed tree, and API-04 enforced on the extracted members of the installed archive (installed allowlist of three rows, recorded per-OS undefined set, disassembly and header scans) with four seeded violations that must fail.**

## Performance

- **Duration:** 25 min
- **Started:** 2026-11-26T02:41Z
- **Completed:** 2026-11-26T03:06Z
- **Tasks:** 4 of 4
- **Files modified:** 14 (8 created, 6 modified)

## Accomplishments

- **Layout pins (Task 1, tracer).** `scripts/check-abi.py layout --write` runs a probe program against the header and generates `docs/abi/layout.c`: 14 structs, 165 members and 190 constants (188 integer values, 2 string-size pins), one `T64_LAYOUT_PIN` per line. `layout --check` compiles it against an include directory as C99, C11 and C++17 (`-pedantic -Wall -Wextra -Werror -fsyntax-only`) and regenerates and diffs, so an added, removed or changed member or constant fails. `repo.abi-layout` checks `core/include`; `install.abi-header` and `install.abi-layout` check the installed header in the moved prefix (compile alone as C99 and C++17, then the pins). Editing one offset in a temporary copy (`offsetof(t64_machine_create_info, allocator) == 8u` to `16u`) fails all three compiles with the static-assertion text `8 == 16`; the same case is permanent as `selftest layout-offset-edited`, next to `layout-header-drift` (a member added to a struct of a header copy).
- **nm golden and path scan (Task 2).** `docs/abi/public-symbols.txt` is the 35 public functions of the installed archive (no banner, `wc -l` is 35); the archive defines 224 further globals, all `t64i_`, and every global is an `nm` text symbol. `install.abs-paths` reads the bytes of every file of the moved prefix, archive included, for the source root, the build root, `/Users/`, `/home/`, `/private/` and `/var/folders` (6 files, 0 findings); the two roots are reported by position, never by value, and an install prefix without the archive is a usage error. The preflight `abi-cheap` step ran the cheap names in 1 s at that point.
- **API-04 on the members (Task 3).** `lint-objects.sh --allow FILE` merges an allowlist for one run (default unchanged). `tools/lint/allowlist-installed.txt` holds exactly the three ADR-0006 amendment rows. `scripts/check-api04.sh members` extracts the archive with `ar x` from its own path inside the spaced, moved prefix, runs the lint with the installed allowlist over all 86 members in one invocation, and compares the archive-level undefined set (`__stack_chk_fail`, `__stack_chk_guard`, `bzero`, `memcmp`, `memcpy`, `memset` on macOS) with the recorded rows of `docs/abi/installed-undefined.txt`; the control without the installed allowlist names exactly the two stack-protector names (26 lines). The two lint runs run side by side. **Q6 (spaced-prefix `ar x`): it works**, route in-place, 86 members, the BSD symbol-table entry `__.SYMDEF SORTED` excluded; the copy fallback stays in the script and is recorded in PIN-IMPACT.
- **Disassembly, header scan and seeds (Task 4).** `api04.disasm` (95,916 instruction lines, none of CNTVCT, CNTPCT, RNDR, rdtsc, rdtscp, rdrand, rdseed, rdpmc; an empty disassembly is a failure), `api04.headers` (Layer-1 scan of the installed header) and `api04.seeded`: members plus `seed_host_services.c` give exactly four `L2-UNDEF-SYMBOL` lines (`clock_gettime`, `fopen`, `getentropy`, `pthread_create`), members plus an object importing the allowlisted `memmove` fail the set comparison, members plus `seed_hw_counter.c` fail the disassembly scan naming `mrs x8, CNTVCT_EL0`, and an include directory plus `seed_banned_include.h` fails with `L1-INCLUDE-BANNED`.

## Task Commits

1. **Task 1 (tracer): layout pins, source and installed header** - `b4060bb` (feat)
2. **Task 2: nm golden and absolute-path scan** - `aa13db0` (feat)
3. **Task 3: API-04 member lint with the installed allowlist and the recorded undefined set** - `581a5b2` (feat)
4. **Task 4: disassembly scan, installed-header scan, seeded violations** - `371f58c` (feat)

## Verification

| Check | Result |
|-------|--------|
| `cmake --build --preset gate && ctest -R '^(repo\.abi-layout\|install\.abi-header\|install\.abi-layout)$'` (Task 1, run again at the commit as the tracer gate) | build 0, 4 of 4 passed (`install.setup` joins through the fixture), 0.18 to 0.47 s each |
| Editing one offset in a temporary copy of `layout.c` | `layout --check` exits 1 with three compile findings (C99 negative array, C11 `static assertion failed ... '8 == 16'`, C++17) and the regeneration diff |
| `ctest -R '^install\.(abi-symbols\|abs-paths)$'`, `test "$(wc -l < docs/abi/public-symbols.txt)" -eq 35` | 3 of 3 passed; 35 lines (`t64_abi_version`, `t64_machine_reset`, `t64_machine_unload`, `t64_machine_is_poisoned`, `t64_machine_output_status` among them) |
| `sh scripts/preflight-phase11.sh --no-replay` (after Task 2 and after Task 4) | PASS (67 s and 94 s at load 15 to 33); `abi-cheap` 1 s with the five cheap names and 8 s with all fourteen at that load (target 15 s) |
| `ctest -R '^api04\.members$'` and `-R '^api04\.(disasm\|headers\|seeded)$'` | 2 of 2 and 4 of 4 passed (`install.setup` in each closure); members 7.8 s, seeded 6.6 s, disasm 0.8 s, headers 0.14 s at load 15 to 40 |
| RED Task 3 (`build/gate/11-17-task3/red.txt`) | members with an empty installed allowlist: exit 1, 26 `L2-UNDEF-SYMBOL` lines, only `__stack_chk_fail` and `__stack_chk_guard`; with the three rows: exit 0 |
| RED Task 4 (`build/gate/11-17-task4/red.txt`) | seeded with the lint, Layer-1 scanner and disassembly patterns neutered: exit 1, three of four seeds `NOT DETECTED`; real script: 4 of 4 detected, exit 0 |
| `git diff --quiet f9124ae9fce134f31b645c6f2911a6f27fa41b9a -- tools/lint/allowlist.txt` | exit 0 (source allowlist unchanged) |
| `grep -v '^#' tools/lint/allowlist-installed.txt \| grep -c .` and the last row | 3; the `_GLOBAL_OFFSET_TABLE_` row ends in `\|Linux` |
| `sh tools/lint/core-lint.sh --selftest`, `ls tools/lint/selftest/bad_installed*` | 27 of 27 seeds; no such file |
| `sh scripts/check-test-inventory.sh --preset gate` | PASS, 289 names (280 plus nine) |
| `python3 -B scripts/check-abi.py --selftest` | 14 of 14 cases (10 classes cases and the four new ones), about 0.7 s |
| `sh scripts/pii-scan.sh --tree` | findings=0 (1395 files, before every docs and evidence commit) |
| Linux pre-flight in the act image (gcc 13.3.0, GNU binutils 2.42) | every new check passes on a Linux archive of the 86 objects; undefined set equals the recorded Linux rows; GNU `ar x` from a spaced path works; 4 of 4 seeds detected (`rdtsc` on x86_64) |
| `sh scripts/gate.sh` at `371f58c` | PASS, 289 of 289, ctest 96 s, total 156 s (load 9 to 11) |

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] `_Static_assert` is rejected under `-std=c99 -pedantic -Werror`**
- **Found during:** Task 1 (design, confirmed with a one-line probe: `'_Static_assert' is a C11 extension [-Werror,-Wc11-extensions]`)
- **Issue:** the plan maps the pins to `_Static_assert` in C and compiles as C99 pedantic with `-Werror`; that combination is a hard error.
- **Fix:** one macro with three branches (C++ `static_assert`, C11 `_Static_assert`, C99 negative-array typedef) and a C11 compile added next to C99 and C++17 so every branch is exercised.
- **Files modified:** `scripts/check-abi.py`, `docs/abi/layout.c`
- **Commit:** `b4060bb`

**2. [Rule 1 - Plan count] The `^api04\.` and `^install\.` preflight rows and the ctest run counts include `install.setup`**
- **Found during:** Tasks 1 to 4 (the 11-16 lesson, second time)
- **Issue:** every name that requires the `t64_install` fixture pulls `install.setup` into a `-R` match, so `^api04\.` is 2 after Task 3 and 5 after Task 4 (plan: 1 and 4), the Task 1 verify runs 4 tests (plan: 3) and the Task 2 verify runs 3 (plan: 2). The plan's `^install\.` rows (3, then 5) already counted it.
- **Fix:** rows set to 2 and 5 with a comment; no name or registration changed.
- **Files modified:** `scripts/preflight-phase11-regexes.txt`
- **Commit:** `581a5b2`, `371f58c`

**3. [Plan reading] The path scan is a byte search in Python, not `grep -rl` plus `strings -a`**
- The plan names `grep -rl` over the tree and `strings -a` over the archive. One Python scan of the bytes of every file finds the same printable path strings with no `strings(1)` dependency (hosted Linux and macOS both carry it, but a dependency is a failure mode); the roots are reported by position. Effect: equal or stronger detection, no new tool.

### Additions beyond the plan (no scope change)

- `repo.abi-selftest` gained four seeded cases (`layout-offset-edited`, `layout-header-drift`, `symbols-controls`, `abspaths-controls`), so the layout, golden and path checks are proven able to fail without a new CTest name (selftest 0.7 s).
- String-valued `T64_*` defines (the two state-container format strings) are pinned by `sizeof`, not only the integer constants.
- `scripts/check-api04.sh` was written whole and committed with Task 3, so its `disasm`, `headers` and `seeded` subcommands are in that commit; each was run and verified in Task 4 with its own RED (`build/gate/11-17-task4/red.txt`) and registered there.

**Total deviations:** 2 auto-fixed (1 blocking, 1 count) and 1 plan reading. **Impact:** none on scope, evidence tier or the file list.

### Notes that are not deviations

- Requirements API-03 and API-04 stay open: API-03 is bound by plan 11-23, and API-04 still needs the hosted Linux measurement of plan 11-21 (both plans list it); this plan delivers the local macOS enforcement and the seeds.
- Host load ran 10 to 43 (unrelated VM and Docker work); no limit was widened. The lint-heavy names are the costly ones (about 7 s each at that load, a third of it the lint's per-object process spawns).

## Authentication Gates

None.

## Issues Encountered

- A first PII tree scan flagged `/Users/<name>` in a path-scan seed string; the seed text now holds `/Users/` with no name component.
- The `lint-objects.sh` messages name the member path under the temporary directory; `check-api04.sh` strips it, so logs name members only.

## Known Stubs

None. No placeholder, empty-value or mock data flows into any file this plan created.

## Threat Flags

None. `scripts/check-api04.sh` writes only below a `mktemp -d` directory under `TMPDIR` (refused when the path contains a space), runs `ar x`, `nm`, `objdump` and the cc on seeds there, and has no network or environment dependence beyond `CC`, `NM`, `AR` and `OBJDUMP`.

## Threat Model Disposition

| Threat | Result |
|--------|--------|
| T-11-31 (host clock or entropy in the core) | members lint with the archive-level undefined set equal to the recorded rows, the disassembly scan and the four seeded violations all pass on macOS; Linux confirmation is the hosted run of 11-21 |
| T-11-32 (allowlist widening) | separate three-row installed allowlist with ADR-0006 rows; `tools/lint/allowlist.txt` identical to the Wave 0 base; the set comparison catches the allowlisted `memmove` seed |
| T-11-33 (absolute paths) | `install.abs-paths` over the installed tree and archive bytes: 0 findings, seeded cases prove it can fail |
| T-11-34 (ABI drift between source and installed header) | layout pins compiled against both include directories in three languages plus the regeneration diff; the header-drift seed fails |

## Next Phase Readiness

- Plan 11-18 onward can rely on the cheap-first names (nine, about 16 s of test time at host load 15 to 40, `abi-cheap` 8 s) and on `scripts/check-api04.sh`; every new INSTALL name must keep its `COST` set (gate-macos growth warning of the INSTALL tracer ledger row stands: the nine names add roughly 45 to 70 s of hosted test time, mostly `api04.members` and `api04.seeded`, which share the lint cost; the orchestrator's measured cold run is still pending).
- Plan 11-21 (hosted gate-linux) must confirm or refute the act-measured Linux rows of `docs/abi/installed-undefined.txt`, and also exercises `api04.*` and `install.abi-*` on ELF for the first time (`objdump`, `readelf`, GNU `nm` and `ar` behaviors were assumed from the act image and RESEARCH).
- Plan 11-23 binds `docs/abi/layout.c`, `docs/abi/public-symbols.txt` and `docs/abi/installed-undefined.txt` (re-bind command in the layout banner).
- **Push state.** Nothing was pushed; `origin/main` is `e5a2e3c`. All local commits after it are unpushed and go out with the next push a plan names.

## Self-Check: PASSED

- FOUND: docs/abi/layout.c, docs/abi/public-symbols.txt, docs/abi/installed-undefined.txt, scripts/check-api04.sh, tools/lint/allowlist-installed.txt, tools/lint/installed-seeds/seed_host_services.c, tools/lint/installed-seeds/seed_hw_counter.c, tools/lint/installed-seeds/seed_banned_include.h, scripts/check-abi.py, tools/lint/lint-objects.sh
- FOUND commits: b4060bb, aa13db0, 581a5b2, 371f58c (each an ancestor of HEAD; `git rev-list --count` from the base is 4)
