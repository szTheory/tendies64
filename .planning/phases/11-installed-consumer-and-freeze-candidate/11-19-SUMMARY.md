---
phase: 11-installed-consumer-and-freeze-candidate
plan: 19
subsystem: installed-consumer
tags: [installed-consumer, host-shaped-loop, backpressure, drain-cadence, two-instance, negative-controls, symbol-collision, plain-cc, doctored-header]

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: the relocatable tendies64 package and moved-prefix driver (11-16), the cheap-first ABI and API-04 checks (11-17), the composite fixture through the installed package with the run engine, allocator ledger and class table (11-18), the symbol prefix (11-11)
provides:
  - a host-shaped loop through the installed package with no SDL and no host time: frame-paced advances, drains in host order, a forced backpressure stop that resumes after a drain, and a stall detector that is shown able to fail
  - drain-cadence and host-delay independence: four drain schedules and a 1 ms delay after every advance give one digest and equal outputs
  - two instances with disjoint allocators interleaved in one process, the first destroyed while the second is mid-run
  - installed.controls.<id>: ten negative controls driven by tests/installed-consumer/controls.tsv and `check-install.sh control ID` (missing package, two version mismatches, visible source tree with its seed, doctored ABI and size headers, relocated prefix with spaces, symbol collision, plain cc line)
  - one-time evidence that the symbol-collision control fails against an unprefixed archive, by a duplicate-symbol link error for t64_sha256_init and by changed results for f32_add
affects: [11-20, 11-21, 11-23, 11-25]

actuals:
  tokens: 17000
  tasks: 3
  commits: 4

plan_head_before: c6f28f679af6982abd72caee70890ec27def621d
plan_head_after: 461197d5f6ea4ed4bf640c9b8109533a7c061dfd

tech-stack:
  added: []
  patterns:
    - "a host-shaped loop separates the two kinds of RESOURCE_LIMIT stop: one that moved the guest tick is the per-advance event budget and is resumed; one that moved nothing is backpressure, forces a drain, and the next advance must move or the loop fails with 'stall without progress'"
    - "a negative control is one row of a TSV, one CTest name and one driver action, and passes only when its exact expected text or create status is observed and logged next to the observed value"
    - "a control that must be shown able to fail has a seed row (the source-tree scan finds an extra -I) or a one-time scratch run recorded in PIN-IMPACT (the unprefixed archive)"

key-files:
  created:
    - tests/installed-consumer/hostloop.c
    - tests/installed-consumer/collision.c
    - tests/installed-consumer/controls.tsv
    - tests/installed-consumer/smoke.c
    - tests/installed-consumer/smoke_main.c
  modified:
    - tests/installed-consumer/main.c
    - tests/installed-consumer/CMakeLists.txt
    - tests/installed-consumer/cases.txt
    - tests/installed-consumer/t64c_cases.h
    - tests/installed-consumer/t64c_alloc.h
    - scripts/check-install.sh
    - tools/abi/CMakeLists.txt
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase11-regexes.txt
    - .planning/phases/11-installed-consumer-and-freeze-candidate/PIN-IMPACT.md

key-decisions:
  - "The host frame budget is 8192 guest ticks, a host policy value: one real 60 Hz frame at 62,500,000 ticks per second would hold the whole composite run. Inputs are queued before the frame that holds their tick; the digest is the same as when they are queued up front."
  - "Only a RESOURCE_LIMIT stop that moved nothing is backpressure. The 210 stops per run that moved the tick are the per-advance event budget and are resumed without a drain."
  - "The smoke case moved into its own file with a small t64c_smoke target, built only by the controls that doctor or relocate a prefix, because the whole consumer reads create-info members (a shrunk create-info would not compile) and a full consumer build per control is costly."
  - "The consumer's counting allocator aligns by arithmetic over malloc instead of aligned_alloc, so the consumer and the plain cc row compile as C99 like the public header."
  - "No pkg-config file (DR-29): the plain cc line is the package smoke without CMake."

patterns-established:
  - "a control driver action writes everything it observes to its own log and a single judge compares it with the row, so every control states expected and observed"

requirements-completed: []

coverage:
  - id: D1
    description: "A host-shaped loop through the installed package is deterministic regardless of drain timing, host delay or a second instance, with a forced backpressure stop that resumes after a drain"
    requirement: VER-05
    verification:
      - kind: integration
        ref: "ctest -R '^installed\\.consumer\\.(hostloop|hostloop-cadence|hostloop-delay|two-instance)$' (6 of 6 with the fixture closure): digest 97f27027...1267c in every run, one backpressure stop at guest tick 540952 in each run that skips draining"
        status: pass
    human_judgment: false
  - id: D2
    description: "Every packaging, header-skew and collision failure mode fails loudly with a precise signal"
    requirement: VER-05
    verification:
      - kind: integration
        ref: "ctest -R '^installed\\.controls\\.' (11 of 11 with install.setup): missing package, version 0.2 and 0.1.5, visible source tree and its seed, create status 34 and 35 from doctored headers, relocated prefix with spaces, symbol collision, plain cc line"
        status: pass
    human_judgment: false
  - id: D3
    description: "The symbol-collision control can fail: against an unprefixed archive it fails by a duplicate symbol (t64_sha256_init) and, with f32_add alone, by changed results"
    requirement: VER-05
    verification:
      - kind: integration
        ref: "one-time scratch run recorded in PIN-IMPACT.md Install steps 11-19 (logs in the ignored build tree)"
        status: pass
    human_judgment: false
  - id: D4
    description: "The gate and the preflight hold with the fourteen new names"
    verification:
      - kind: integration
        ref: "sh scripts/gate.sh PASS 323 of 323 (ctest 105 s, total 183 s at host load 10 to 13, started after waiting for the load to fall below 10); sh scripts/preflight-phase11.sh --no-replay PASS (100 s)"
        status: pass
    human_judgment: false

duration: 36min
completed: 2026-11-26
status: complete
---

# Phase 11 Plan 19: INSTALL host-shaped loop, negative controls, symbol collision and plain cc Summary

**The installed package behaves identically under any drain schedule, host delay and a second instance (one digest, 97f270...1267c, in every run, with a forced backpressure stop at guest tick 540952 that resumes after a drain), and ten negative controls, one CTest name each, show that a missing package, a version mismatch, a visible source tree, a doctored header (status 34 and 35), a relocated prefix with spaces and a host that owns f32_add and t64_sha256_init are all detected, with the collision control shown failing against an unprefixed archive.**

## Performance

- **Duration:** 36min (builds and runs at host load 8 to 40 from unrelated work on the machine; no wall backstop was hit and no limit changed)
- **Tasks:** 3 of 3
- **Files modified:** 15 (5 created, 10 modified)

## Accomplishments

- **Host-shaped loop (Task 1, tracer).** `hostloop.c` advances in 8192-tick frames, queues each input before the frame that holds its tick, drains video then audio then ISViewer text in host order, treats a RESOURCE_LIMIT stop that moved the tick as the resumable per-advance budget (210 per run) and one that moved nothing as backpressure (drain, resume; the next advance must move or the loop fails with "stall without progress"). `hostloop` forces a stalled host (no drain in frames 64 to 71) into one backpressure stop at tick 540952 and resumes it to completion at 599357; a seeded host that never drains is stopped by the detector. `hostloop-cadence` runs four schedules (every slice, every third, only on a stop, none until the first backpressure stop): one digest and equal outputs. `hostloop-delay` adds 284 sleeps of 1 ms: the same digest. `two-instance` interleaves two machines with disjoint ledgers (different slice sizes and drain cadences), destroys the first at tick 303104 with the second mid-run and checks the second's digest and ledger are unchanged, then runs it through its own backpressure stop to the same literals. The loop reads no clock.
- **Negative controls (Task 2).** `controls.tsv` has one row per control, `check-install.sh control ID` performs it in its own directory and passes only when the row's exact text or create status is observed (the log prints expected and observed). Missing package, version 0.2 and 0.1.5 ("The version found is not compatible with the version requested"), the visible-source-tree scan (compile database and `build.ninja` name nothing of the repository outside the consumer's own sources, build tree and the moved prefix) with a seed row that must be detected, a doctored ABI minor (create answers 34, machine NULL), a create-info without its last member (35, machine NULL) and a prefix copied and moved to a second path with spaces. RED: with the rows and the CTest registration but no subcommand all eight failed with `unknown subcommand: control`, exit 2.
- **Symbol collision and plain cc (Task 3).** `collision.c` defines the host's own `f32_add` and `t64_sha256_init` with wrong results and call counters. Against the installed archive the link succeeds, the archive has `t64i_f32_add` and `t64i_sha256_init` and defines or references neither unprefixed name, and the composite case ends at the pinned digest with `f32_add calls: 0` and `t64_sha256_init calls: 0`. One-time evidence: an unprefixed scratch archive fails the same row with `duplicate symbol '_t64_sha256_init'`, and with `f32_add` alone the link succeeds silently but the composite fails (bound reached before completion, a different digest, 5105 calls of the host's `f32_add`), so the control is shown able to fail both ways. The plain `cc -std=c99` line compiles the six consumer sources and links the installed archive without CMake and the smoke case runs; the `.pc` file is dropped (DR-29).

## Task Commits

1. **Task 1 (tracer): host-shaped loop, cadences, delay and two instances** - `caf2da0` (feat)
2. **Task 2: negative-control matrix** - `6b29352` (feat)
3. **Task 3: symbol-collision control and plain cc row** - `e117e55` (feat)
4. **PIN-IMPACT record of the three tasks** - `461197d` (docs)

## Verification

| Check | Result |
|-------|--------|
| `ctest -R '^installed\.consumer\.(hostloop\|hostloop-cadence\|hostloop-delay\|two-instance)$'` | 6 of 6 passed (4 names plus `install.setup` and `installed.consumer.build`) |
| `ctest -R '^installed\.controls\.'` | 11 of 11 passed (10 names plus `install.setup`) |
| `ctest -N -R '^installed\.consumer\.'` and `'^installed\.controls\.'` | 27 and 11 (the preflight rows) |
| Cadence output | four runs, digest `97f270274d456e883ceaa11f100f92c7fbf3150e89031fa51164e79991f1267c` in each, drains 284, 25, 2 and 21, a backpressure stop at tick 540952 in the two runs that skip draining |
| Collision log | `f32_add calls: 0`, `t64_sha256_init calls: 0`, final state digest `97f270...1267c`, 25 checks, 0 failures |
| Unprefixed scratch run | link fails with `duplicate symbol '_t64_sha256_init'`; with f32_add alone the composite fails with `f32_add calls: 5105` and a different digest |
| gcc 13.3 (act image) of every consumer source | clean with the consumer's strict flags at -std=c17 -O3 and clean as plain -std=c99 |
| `sh scripts/check-consumer-includes.sh`, `python3 -B scripts/check-api-init.py`, `sh scripts/check-test-inventory.sh --preset gate` | files=15 findings=0; files=200 findings=0; PASS count=323 |
| `sh scripts/preflight-phase11.sh --no-replay` | PASS, 100 s (ctest-counts, abi-cheap 13 s, guards, head-ci, timeout-audit 176 TIMEOUTs 0 too low, pii, linux-compile 233 units 80 s) |
| `sh scripts/gate.sh` | PASS, 323 of 323 names (was 309), ctest 105 s, total 183 s at load 10 to 13 |
| `sh scripts/pii-scan.sh --tree` | findings=0 before every commit |

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Plan count] The installed.* counts include install.setup**
- **Found during:** Task 1 and Task 2 (`ctest -N -R`)
- **Issue:** the plan's 26, 8 and 10 and the EXPECT==4, 8 and 2 of the verify lines ignore the `t64_install` fixture: CTest adds `install.setup` (and `installed.consumer.build` for the consumer names).
- **Fix:** the preflight rows are 27 for `^installed\.consumer\.` and 9 then 11 for `^installed\.controls\.`; the names are as planned.
- **Files modified:** `scripts/preflight-phase11-regexes.txt`
- **Commits:** `caf2da0`, `6b29352`, `e117e55`

**2. [Rule 3 - Blocking] The doctored-size header does not compile the whole consumer**
- **Found during:** Task 2 (first run of `doctored-header-size`)
- **Issue:** `ops.c` reads `info.profile` in two cases, so a header whose create-info lacks that member makes the consumer fail to compile, and a full -O3 consumer build per control is costly anyway.
- **Fix:** the smoke case moved to `smoke.c` and a small `t64c_smoke` target (`smoke_main.c`, EXCLUDE_FROM_ALL) is built by the doctored and relocated controls only; the smoke case prints `smoke: create status=N machine=NULL|set`, which the status rows read. Files outside the plan list: `smoke.c`, `smoke_main.c`, `t64c_cases.h`.
- **Files modified:** `tests/installed-consumer/main.c`, `smoke.c`, `smoke_main.c`, `t64c_cases.h`, `CMakeLists.txt`
- **Commit:** `6b29352`

**3. [Rule 3 - Blocking] -std=c99 cannot compile the consumer's allocator**
- **Found during:** Task 3 (trying the plan's `cc -std=c99`)
- **Issue:** `t64c_alloc.h` used `aligned_alloc`, a C11 function: under strict C99 it is undeclared (an error on clang, an implicit declaration returning int, which truncates pointers on 64-bit, on gcc 13).
- **Fix:** the counting allocator aligns by arithmetic over `malloc` and keeps the raw pointer for `free`; all 37 installed.* names pass with it.
- **Files modified:** `tests/installed-consumer/t64c_alloc.h`
- **Commit:** `e117e55`

**4. [Rule 1 - Plan text] Small plan wording corrected**
- `t64c_collision` is built from every source of the consumer plus `collision.c` (the composite case lives in `composite.c`), not from `main.c` plus `collision.c` alone.
- `controls.tsv` has six columns (id, action, argument, kind, expected, detail), one more than the plan's five, so two rows can share an action.
- A controls scratch directory is `<install-check>/controls/<id>` (removed on success, kept on failure) instead of a `mktemp` directory under a fixed path, so parallel controls cannot collide and a failure leaves its files.
- The consumer project has a cache variable `T64C_REQUIRED_VERSION` (default 0.1) that only the two version rows change.
- `check-install.sh` answers an unknown subcommand with `unknown subcommand: NAME` (needed for the RED evidence) and unsets the environment's `CMAKE_PREFIX_PATH` for every consumer configure, so only the named prefix can supply the package.

**Total deviations:** 4 (one count, two blocking, one wording). **Impact:** none on scope or evidence tier.

### Observed outside the plan

- The hosted run of the previous push first failed gate-macos on `harness.rsp-run` (a 0.1 s outer deadline in `test_closed_pipes_do_not_end_the_outer_deadline`) and passed on rerun; the preflight `head-ci` step failed while that run was red and passed after the rerun. Nothing was changed for it (the orchestrator recorded it for 11-21).
- A second project on the same host ran its CI at `-j18` for several minutes (load 20 to 42). The full gate waited for the load to fall below 10; no limit was widened.

## Authentication Gates

None.

## Issues Encountered

- Host load of 15 to 42 from unrelated work; the gate was started by a script that waits for the load to fall below 10.
- `installed.controls.symbol-collision` is the heaviest of the new names (1.2 to 2.2 s: a full -O3 build of the collision consumer); the doctored and relocated controls use the small smoke target. Half a second of each configure-only control is CMake's compiler detection of the consumer project (a language-less stub would be 0.08 s but would not be the real consumer).

## Known Stubs

None. No placeholder, empty-value or mock data flows into any file this plan created.

## Threat Flags

None. `check-install.sh control` writes only under the build tree (the install-check scratch directory), copies the moved prefix and edits only that copy of the installed header, never the source header; it runs no network access and unsets nothing but the environment's `CMAKE_PREFIX_PATH` for the child cmake.

## Threat Model Disposition

| Threat | Result |
|--------|--------|
| T-11-38 (symbol interposition) | mitigated: the collision control asserts the link, the archive's names, the unchanged digest and zero calls; the unprefixed scratch run fails by duplicate symbol and by changed results |
| T-11-39 (doctored header) | mitigated: `doctored-header-abi` observes status 34 and `doctored-header-size` status 35, machine NULL in both |
| T-11-40 (source tree visible to the consumer) | mitigated: `visible-source-tree` scans the compile database and the ninja file, and its seed row must be detected |
| T-11-41 (host timing influencing results) | mitigated: four drain cadences, 284 host delays and an interleaved second instance give one digest |
| T-11-SC (package installs) | accepted: none were made |

## Next Phase Readiness

- **Plan 11-20** finds the consumer sources in the final shape of this plan: `smoke.c`, `hostloop.c`, `ops.c`, `composite.c`, `collision.c`, `t64c_alloc.h` (C99, arithmetic alignment) and the class table `t64c_classes.h`; `cases.txt` lists 23 binary cases.
- **Counts.** `^installed\.consumer\.` is 27 and `^installed\.controls\.` is 11 with the fixture closure; a plan that adds names must count `install.setup` and, for consumer names, `installed.consumer.build`.
- **Gate growth (orchestrator).** Fourteen new names, about 7.5 s of native test time in total (about 32 s at the hosted multiplier 4.3 if serial; the controls run in parallel). The measured cold gate-macos run still has to confirm the margin against 900 s before the 11-21 ledger row.
- **Hosted confirmation still owed.** This plan has not been pushed. The first hosted run of the controls (the CMake messages, `cp -R` and `nm -P` of the prefix, `cc -std=c99` with gcc 13.3 on Linux, the symbol-collision link on ELF) is the next push; the sources compile clean under gcc 13.3 in the act image but only a hosted run proves the install and link paths.
- **VER-05** now has every ingredient locally (relocation 11-16, the composite three-way and independent literals 11-18, the controls here); it is not marked complete here because the hosted confirmation and the phase verification are still ahead. **API-01** needs the later plans.

## Self-Check: PASSED

- FOUND: tests/installed-consumer/{hostloop.c,collision.c,controls.tsv,smoke.c,smoke_main.c}
- FOUND commits: caf2da0, 6b29352, e117e55, 461197d (each an ancestor of HEAD; `git rev-list --count` from the base is 4)
