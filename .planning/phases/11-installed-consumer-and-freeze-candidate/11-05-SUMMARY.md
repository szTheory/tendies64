---
phase: 11-installed-consumer-and-freeze-candidate
plan: 05
subsystem: api
tags: [init-macros, codemod, runner, bound-path-refusal, scratch-root, shape-wave]

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: plan 11-04 (T64_OUTPUT_STATUS_INIT in its final form, 35 public functions, the retained slot types); decision record DR-09 and the Wave 0 base
provides:
  - ten public initializer macros in t64_machine.h expanding to today's layouts (T64_HOST_ALLOCATOR_INIT, T64_STOP_REQUEST_INIT, T64_STOP_RESULT_INIT and _WITH, T64_CPU_STOP_DETAIL_INIT, T64_OUTPUT_DRAIN_RESULT_INIT, T64_VIDEO_PACKET_INFO_INIT, T64_AUDIO_PACKET_INFO_INIT, T64_STATE_IMPORT_REPORT_INIT, T64_SAVE_INFO_INIT), valid in C99 and C++17
  - scripts/codemod-phase11.py, a mechanical re-runnable initializer rewrite (--dir, --check, --scratch-root, --selftest; --create and --trace stubs for 11-12) that refuses every EXPECTED_CORPUS_PATHS path and the digest printer, with the one recorded --scratch-root exception
  - the runner/ group (12 files) on the macros, with unchanged results
affects: [11-06, 11-07, 11-08, 11-09, 11-10, 11-12, 11-13, 11-14, 11-15]

actuals:
  tokens: 15500
  tasks: 2
  commits: 3
plan_head_before: 1486df949e1b9f9d701f23edfc6a10e8036c42f9
plan_head_after: 7f50c69c430fca0882c3c13fb2d236300911fbcc

tech-stack:
  added: []
  patterns:
    - "an initializer macro lists every member explicitly (no {0}), so the flip changes only the first element and -Wmissing-field-initializers stays quiet"
    - "the codemod reads the bound-path list from the shared tree's checker with ast and fails closed (exit 2) on a refusal, an unreadable list or a path outside the tree"
    - "a zero-only type with sentinel values becomes INIT plus named member assignments, so a leading struct_size cannot shift a positional list"

key-files:
  created:
    - scripts/codemod-phase11.py
  modified:
    - core/include/tendies64/t64_machine.h
    - runner/t64_real.c
    - runner/t64_base_diagnostic.c
    - runner/t64_cold_boot.c
    - runner/t64_cpu_diagnostic.c
    - runner/t64_replay.c
    - runner/t64_rsp_diagnostic.c
    - runner/t64_rsp_smoke.c
    - runner/t64_skeleton_probe.c

key-decisions:
  - "Macro bodies list every member explicitly (23+6+2 integer members, 2 size_t and 4 text members for the video packet info) instead of {0}: the flip then edits only the first element, and a member count mismatch is a -Wmissing-field-initializers error (checked by dropping one element)"
  - "T64_AUDIO_PACKET_INFO_INIT sets struct_size to sizeof (the existing leading member); every other macro is all zero or, for the host allocator, stop request and stop result with values, takes the members as arguments"
  - "Struct members of the public types are listed by the codemod and left alone; the three in runner/ (t64_raw_capture.c lines 41 and 49, t64_audio_observation.h line 32) are filled by whole-struct copy from macro-initialized locals"
  - "A memset of a converted object is reported for hand conversion, because after the flip it would clear the leading struct_size; it is detected within the block that declares the object"

patterns-established:
  - "Codemod output is checked by `--check` (exit 1 while a site remains) and by `--selftest` (14 text cases, each also run a second time to prove it converts nothing)"

requirements-completed: []

coverage:
  - id: D1
    description: "The header defines the DR-09 macros except T64_MACHINE_CREATE_INFO_INIT (born in 11-12) and T64_OUTPUT_STATUS_INIT (final since 11-04), each a positional brace initializer in today's member order; no struct layout changes"
    requirement: "API-01"
    verification:
      - kind: unit
        ref: "scratch probe (every macro as a static initializer, a local and, in C, a compound literal) compiled with -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wmissing-field-initializers -Werror (and -Wstrict-prototypes -Wcast-qual -Wundef for C) as c99, c11, c17 and c++17 with Apple clang 21: bad=0, sizes 488 and 280 unchanged; dropping one element of T64_SAVE_INFO_INIT fails to compile; python3 -B scripts/check-abi.py header --include core/include --header tendies64/t64_machine.h prints c99 ok, c++17 ok, findings=0; sh scripts/preflight-phase11.sh linux-compile compiled the 231 gate compile units (every .c of the build, the runner among them) with gcc 13.3.0"
        status: pass
    human_judgment: false
  - id: D2
    description: "scripts/codemod-phase11.py is a mechanical, re-runnable rewrite: declarations with brace initializers, uninitialized locals, compound literals; arrays, designated or commented initializers, several declarators and memset of a converted object are reported for hand conversion"
    requirement: "API-01"
    verification:
      - kind: unit
        ref: "python3 -B scripts/codemod-phase11.py --selftest: 14 of 14 (each case re-run on its own output converts 0); RED --check over the 12 runner files: exit 1, 41 sites (40 convertible, 1 hand) at plan_head_before; GREEN: exit 0, 0 sites after the two conversion commits"
        status: pass
    human_judgment: false
  - id: D3
    description: "The codemod refuses to write any EXPECTED_CORPUS_PATHS path or the digest printer in the shared tree (exit 2), and --scratch-root writes bound paths only under a marked scratch clone below build/"
    requirement: "API-01"
    verification:
      - kind: integration
        ref: "exit 2 and no write for core/tests/test_vi.c, ./core/tests/../tests/test_rdp.c, tools/digest-sweep/t64_digest_sweep.c and /etc/hosts; --scratch-root build exits 2 with 'scratch root refused' (not below build/, not its own toplevel, no marker); also refused: the shared toplevel, a plain subdirectory of a clone, a clone without the marker, a path escaping the root; accepted: a git clone --no-hardlinks below build/ with .t64-bound-scratch in its parent (test_vi.c converted there, 88 sites, 2 hand, syntax-checked with the project warning set; the shared test_vi.c unchanged); harness.rdp-oracle-authority passed after each commit"
        status: pass
    human_judgment: false
  - id: D4
    description: "The runner/ production consumers use the macros and every runner and integration test passes unchanged"
    requirement: "API-01"
    verification:
      - kind: integration
        ref: "cmake --build --preset gate; ctest -R '^runner\\.' 25 of 25 after the tracer commit; ctest -R '^(runner\\.|integration\\.)' 31 of 31 at the head; harness.(rsp|base) 11 of 11; sampling set (harness.rdp-oracle-authority, repo.si-membership-sites, harness.state-field-use-audit, repo.p10-mutant-anchors, repo.rsp-no-hle) 5 of 5; full sh scripts/preflight-phase11.sh with replay PASS 80 s; GUARD MUTATION (T64_STOP_FRAME_EVENT_COUNT in place of T64_STOP_GUEST_TICK in the one T64_STOP_REQUEST_INIT call of runner/t64_real.c): 12 or more runner tests failed, the file was restored byte for byte (sha256 prefix e5a2c3dc424b59e4 before and after) and 25 of 25 passed again"
        status: pass
    human_judgment: false

duration: 22min
completed: 2026-11-25
status: complete
---

# Phase 11 Plan 05: Public Init Macros and the Initializer Codemod (runner/ group) Summary

**Ten public initializer macros with today's layouts, a re-runnable codemod that refuses oracle-bound paths outside a marked scratch clone, and the 12-file runner/ group converted (40 sites by the tool, 1 memset removed by hand) with the runner and integration CTests unchanged.**

## Performance

- **Duration:** 22 min
- **Started:** 2026-11-25T21:03Z
- **Completed:** 2026-11-25T21:25Z (this summary and the state update follow)
- **Tasks:** 2 of 2 (plus one small tool commit between them)
- **Files modified:** 10 (1 created, 9 modified)

## Accomplishments

- **Tracer (Task 1).** The macros and the codemod exist and were applied end to end to the public-header-only client `runner/t64_real.c`, which builds and passes the 25 `runner.` tests. The macro bodies list every member explicitly; a scratch probe compiled them as static initializers, locals and (C only) compound literals in c99, c11, c17 and c++17 under the project warning set, and gcc 13.3.0 compiled the converted runner units in the preflight `linux-compile` step.
- **Codemod.** `scripts/codemod-phase11.py` (stdlib, `python3 -B`) rewrites declarations with brace lists, uninitialized locals and file-scope objects, and compound literals; it masks comments, strings and preprocessor lines, tracks brace context (a struct member is listed, never rewritten), skips parameters, pointers, prototypes and `extern`, wraps long lines the way the surrounding code does, and prints a per-file count (0 included). `--check` exits 1 while a site remains. `--selftest` runs 14 text cases, each twice. It reads `EXPECTED_CORPUS_PATHS` from the shared tree's checker with `ast` and fails closed.
- **Bound-path safety (T-11-14).** An explicit bound path or the printer exits 2 before anything is written; a directory argument skips them and lists them as excluded. `--scratch-root ROOT` (the W-5 exception, implemented once here for 11-12 to 11-14) requires ROOT strictly below `build/` of the shared tree, its own git toplevel, a different toplevel from the shared one, `.t64-bound-scratch` in its parent, and no link from a target to a shared bound file; every other root exits 2 with "scratch root refused". `--create` and `--trace` print that they land in 11-12 and exit 2.
- **runner/ group (Task 2).** 31 sites in 7 more files by the tool; the other four files have 0 convertible sites. The runner and integration tests (31), the diagnostic harness tests (11) and the sampling guards (5) pass; the 280-byte `_Static_assert` of `runner/t64_audio_observation.h` is unchanged.

## Task Commits

1. **Task 1: Tracer, macros plus the codemod applied to runner/t64_real.c** - `2be1265` (feat)
2. **Tool refinement between the tasks** - `74238f7` (fix): layout of a compound literal that follows another argument; the struct-member note no longer claims how a member is filled
3. **Task 2: the rest of the runner/ group** - `7f50c69` (refactor)

**Plan metadata:** the summary and state commits that follow this file. The measured `commits:` count of 3 is `git rev-list --count 1486df9..7f50c69` at write time.

## Per-file converted counts (consolidated into PIN-IMPACT by plan 11-10)

| File | Converted | Brace | Uninit | Compound | Hand |
|------|-----------|-------|--------|----------|------|
| runner/t64_real.c | 9 | 3 | 5 | 1 | 1 (memset removed by hand) |
| runner/t64_base_diagnostic.c | 4 | 2 | 1 | 1 | 0 |
| runner/t64_cold_boot.c | 7 | 5 | 0 | 2 | 0 |
| runner/t64_cpu_diagnostic.c | 4 | 2 | 1 | 1 | 0 |
| runner/t64_replay.c | 3 | 2 | 1 | 0 | 0 |
| runner/t64_rsp_diagnostic.c | 5 | 3 | 1 | 1 | 0 |
| runner/t64_rsp_smoke.c | 5 | 4 | 0 | 1 | 0 |
| runner/t64_skeleton_probe.c | 3 | 3 | 0 | 0 | 0 |
| runner/t64_raw_capture.c | 0 | 0 | 0 | 0 | 0 |
| runner/t64_raw_capture.h | 0 | 0 | 0 | 0 | 0 |
| runner/t64_audio_observation.h | 0 | 0 | 0 | 0 | 0 |
| runner/t64_supervisor.c | 0 | 0 | 0 | 0 | 0 |
| **Total** | **40** | 24 | 9 | 7 | 1 |

`runner/t64_supervisor.c` (checker item I-3, a `t64_profile_descriptor` lookup and hash) has 0 sites: it uses no brace initializer of a macro type, and it is rebuilt and re-tested here and again at the flip. The one `brace` site of `t64_real.c` that is not a plain macro is `t64_cpu_stop_detail stop_detail`, which held the values `UINT64_MAX, UINT32_MAX, UINT32_MAX` as sentinels; it is now `T64_CPU_STOP_DETAIL_INIT` followed by three named member assignments, so a leading `struct_size` cannot shift the values.

## New non-bound callers (grep first)

`runner/*.c` and `runner/*.h` contain the 12 planned files and no other user of a macro type. The three new non-bound callers named in the running notes are core/tests files that belong to the core/tests groups, not to this directory group: `core/tests/test_api_surface.c` (0 remaining sites), `core/tests/test_api_poison.c` (13 convertible, 3 hand, all memset) and `core/tests/test_api_lifecycle.c` (18 convertible, 15 hand, all memset), all three listed in plan 11-09. State at the end of this plan for the later plans (`--check`): core/tests 620 sites (55 hand: 48 memset of a macro-type object, 7 arrays of `t64_audio_packet_info` in `test_ai.c` and `t64_video_packet_info` in `test_vg_gaps.c`), 9 bound files excluded; runner/tests 30 sites (2 hand, both memset), 1 bound file excluded; `tests/` and `tools/` 0.

## Files Created/Modified

- `core/include/tendies64/t64_machine.h` - ten macros after their types, with the usage comment
- `scripts/codemod-phase11.py` - the codemod (created)
- `runner/t64_real.c`, `runner/t64_base_diagnostic.c`, `runner/t64_cold_boot.c`, `runner/t64_cpu_diagnostic.c`, `runner/t64_replay.c`, `runner/t64_rsp_diagnostic.c`, `runner/t64_rsp_smoke.c`, `runner/t64_skeleton_probe.c` - initializer forms only

## Decisions Made

Binding decisions are DR-09 and the W-5 exception of the plan; the choices inside the plan's discretion are in the frontmatter (explicit member lists, struct members listed and left alone, memset detection).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing critical functionality] A memset of a converted object removed by hand**
- **Found during:** Task 1 (the tool reported it)
- **Issue:** `runner/t64_real.c` zeroed `import_report` with `memset` right after its declaration. With the declaration now `T64_STATE_IMPORT_REPORT_INIT` the call is redundant, and after the flip (`t64_state_import_report` gains a leading `struct_size`) it would clear the field the callee requires, so the import would refuse at run time while the runner still compiled.
- **Fix:** the one `(void)memset(&import_report, 0, sizeof import_report);` line is removed; nothing between the declaration and the call used the report.
- **Files modified:** `runner/t64_real.c`
- **Commit:** `2be1265`

### Plan text applied with additions or adjustments (recorded for transparency)

- **Codemod additions beyond the plan text.** `--selftest` (14 cases); the "INIT plus named member assignments" form for a zero-only type that holds sentinel values (21 such `t64_cpu_stop_detail` sites existed across core/tests, runner and runner/tests at the first scan, one of them in `t64_real.c`); detection of `memset` of a converted object (reported as a hand site, 48 in core/tests, 2 in runner/tests, 1 here); line wrapping that mimics the surrounding code. The plan said the tool reports array and member sites; it also reports these.
- **A third commit.** `74238f7` refines the tool between the two task commits (layout and a note) so that the Task 2 conversion commit lists only runner files; the plan expects two commits.
- **Reported member sites.** The plan says to hand-convert "struct-member" sites the tool reports. The three members in runner/ cannot take an initializer and are filled by `entry->info = *info;` and `packet->info = info;` from macro-initialized locals, so they needed no edit.
- **`T64_AUDIO_PACKET_INFO_INIT` sets `struct_size`.** As the plan says; today the callee overwrites it (`t64_audio_info` writes `info.struct_size`), and the runner never reads it before the call.
- **Macro argument names.** The macro parameters are `allocate_fn`, `release_fn`, `context_ptr`, `request_kind`, `request_value`, `stop_reason` and `stop_tick` (the plan names the arguments `allocate, release, context`, `kind, value` and `reason, tick`); a macro body names no member, so this only keeps the parameter names from reading like member names.
- **Optional ledger.** The cross-phase `.planning/WINDOWS.md` ledger was not appended (best-effort; no stub, skipped test or unrun verify exists).
- **Full `ctest --preset gate` not re-run.** The plan's verification set (runner and integration CTests, sampling guards) plus the diagnostic harness tests and the full preflight with replay passed; `cmake --build --preset gate` builds every target. No core source changed, so no digest or sealed body can move.

**Total deviations:** 1 auto-fixed (Rule 2), 6 recorded adjustments. **Impact:** none on scope; every plan truth and acceptance criterion holds.

## Authentication Gates

None.

## Issues Encountered

- The host load stayed between 8 and 14 during the native runs; every timing here is a pass/fail or a count and no limit was widened. The runner tests took 38 s and the runner-plus-integration set 51 s.
- A first codemod run on the runner group produced compound-literal call arguments that overflowed 80 columns or wrapped awkwardly; the tool was refined (`74238f7`) and the seven affected files were re-converted from their committed text before any commit.

## Open items for later plans (not acted on here)

- The 48 `memset` sites of macro-type objects in core/tests (and 2 in runner/tests) are hand sites for 11-06 to 11-10: each must become removal (the declaration already zeroes) or an assignment from the INIT form, never a `memset`, once the flip puts `struct_size` first. `test_api_lifecycle.c` and `test_api_poison.c` hold most of the new ones.
- The init-form guard of 11-10 (`scripts/check-api-init.py`) should treat `memset` of a macro-type object as a violation; the codemod's `--check` already reports it.
- The `--create` and `--trace` modes still print "lands in 11-12"; the bound-path safety they must reuse is in `expand()` and `validate_scratch_root()`.

## Verification Results

| Check | Result |
|-------|--------|
| `cmake --build --preset gate` | builds clean (header edit rebuilt every unit) |
| `ctest -R '^runner\.'` after Task 1 | 25 of 25 passed |
| `ctest -R '^(runner\.\|integration\.)'` at the head | 31 of 31 passed (51 s, load 14) |
| `ctest -R '^harness\.(rsp\|base\|cpu)'` | 11 of 11 passed |
| sampling guards (`harness.rdp-oracle-authority`, `repo.si-membership-sites`, `harness.state-field-use-audit`, `repo.p10-mutant-anchors`, `repo.rsp-no-hle`) | 5 of 5 passed |
| `python3 -B scripts/codemod-phase11.py --check` over the 12 runner files | RED: exit 1, 41 sites; GREEN: exit 0, 0 sites |
| `python3 -B scripts/codemod-phase11.py --selftest` | 14 of 14 |
| bound path, printer, outside-tree and scratch-root refusals | exit 2 each, nothing written; marked scratch clone accepted |
| guard mutation (wrong stop kind in `runner/t64_real.c`) | 12+ runner tests failed; restored byte for byte; 25 of 25 passed |
| `python3 -B scripts/check-abi.py header --include core/include --header tendies64/t64_machine.h` | c99 ok, c++17 ok, findings=0 |
| `sh scripts/preflight-phase11.sh` (full, with replay) | PASS, 80 s (linux-compile 231 units gcc 13.3.0, post-gate-replay 29 s) |
| `sh scripts/pii-scan.sh --tree` | findings=0 |
| `git show --name-only` of the task commits | no oracle-bound file, no deletion; `runner/t64_audio_observation.h` unchanged |

## Known Stubs

`--create` and `--trace` of the codemod are intentional stubs (print "lands in 11-12", exit 2) named by the plan; plan 11-12 implements them.

## Threat Flags

None. T-11-14 (oracle-bound files) is mitigated by the exit-2 refusal, the scratch-root conditions and `harness.rdp-oracle-authority` after each commit; T-11-15 (runner semantics) by one compiling commit per task, the runner and integration tests after each and the guard mutation.

## Next Phase Readiness

Plans 11-06 to 11-10 can apply the same tool group by group (`--dir` on the listed files, then `--check`); the expected hand sites are the memset calls and the seven arrays listed above. The flip (11-12) changes only the macro bodies for the runner/ sites. API-01 is not complete: the remaining codemod groups, the flip, the struct_size prologues and the installed consumer are ahead. Nothing in this plan is orchestrator-only and no owner decision is pending.

## Self-Check: PASSED

- FOUND: core/include/tendies64/t64_machine.h, scripts/codemod-phase11.py, runner/t64_real.c and the seven converted runner files
- FOUND commits: 2be1265, 74238f7, 7f50c69 (all ancestors of HEAD)
