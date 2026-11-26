---
phase: 12-minimal-host-playable-homebrew-and-final-bind
plan: 09
subsystem: host
tags: [host, sdl3, dependency-audit, policy-scan, virtual-gamepad, dummy-drivers, local-lane, window-surface-blit]

requires:
  - phase: 12-08
    provides: the pump's input hook, t64h_map_pad and t64h_raw_pad, t64h_frame_copy, the host project and check-host.sh
  - phase: 12-07
    provides: the standalone host project t64h with the T64H_SDL option, repo.host-include-guard and repo.host-isolation, the HOST gate-growth stub
provides:
  - docs/evidence/sdl3-host-dependency-audit.md: the pre-use audit of the link-only system SDL3 (zlib license, Homebrew core formula, no candidate on ubuntu-24.04, no vendoring, committed before the first host/sdl line)
  - scripts/check-host-policy.sh (twelve rules, 30-case selftest) and the CTest names repo.host-policy and repo.host-policy-selftest
  - host/sdl: the optional SDL3 shell (static library t64h_sdl_shell, executable t64h_sdl, headless smoke t64h_sdl_smoke), built only with -DT64H_SDL=ON
  - sh scripts/check-host.sh sdl: the local lane (recorded skip line when SDL3 is absent, dummy drivers, virtual gamepad, sanitized lines only)
  - PIN-IMPACT "Host steps" (SC2 virtual-joystick clause closed by a PASS, DR-19 fit test), DR-37 and the HOST ledger row for the two names
affects: [12-10, 12-12, 12-17, 12-18]

actuals:
  tokens: 19100
  tasks: 3
  commits: 3
plan_head_before: 82baf6f772e66324d98f5a35f1e8b6cc03122c2c
plan_head_after: 5490b2f828f5883f76604acf8b73f0968461d904

tech-stack:
  added: []
  patterns:
    - "a link-only system dependency audited in a committed document before its first build, with a mechanical policy scan (selftest asserts every rule has a seeded failing case)"
    - "a local lane that is never a CTest name: probe by a scratch find_package project, skip line when absent, sanitized stdout only, raw logs under an ignored build directory"
    - "a smoke whose expected values are written out independently of the table under test, and whose power is shown by scratch mutants"

key-files:
  created:
    - docs/evidence/sdl3-host-dependency-audit.md
    - scripts/check-host-policy.sh
    - host/sdl/t64h_sdl.c
    - host/sdl/t64h_sdl.h
    - host/sdl/t64h_sdl_main.c
    - host/sdl/t64h_sdl_smoke.c
  modified:
    - host/CMakeLists.txt
    - scripts/check-host.sh
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase12-regexes.txt
    - tools/hostcheck/CMakeLists.txt
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/PIN-IMPACT.md

key-decisions:
  - "DR-37 (PIN-IMPACT): the shell is a static library linked by the executable and the smoke (two files more than the plan listed); a packet the host cannot show is counted and consumed, never held; QUEUE_FULL on the live pad does not lower the bound; twelve policy rules; the lane probes with a scratch find_package project and a found SDL3 never skips"
  - "SC2's virtual-joystick clause is closed by a PASS of the lane on this host (SDL 3.4.10, macOS 26.6.2); a skip stays the distinct state not closed"
  - "DR-19 re-confirmed: the host-side save file does not fit one task and is omitted"
  - "API-05 is not marked complete: a real window, a real display and a physical controller are the owner checklist of 12-18, and the dummy-driver smoke is host glue only"

patterns-established:
  - "scripts/check-host.sh sdl is the only way the shell is built; no CTest name, workflow step or gate stage mentions it"
  - "a virtual trigger axis at 0 reads half travel through the SDL gamepad layer; rest is -32768"

requirements-completed: []

coverage:
  - id: D1
    description: "The system SDL3 package is audited before first use, SDL3 is link-only and never vendored, fetched or copied, and the host's own rules (no FetchContent, ExternalProject, vendored SDL source, install rule, bundle, icon, product name in a literal, window title exactly 't64 minimal host (development)', no renderer, GPU path or keyboard event) are mechanical"
    requirement: "API-05"
    verification:
      - kind: unit
        ref: "ctest -R '^repo\\.host-policy(-selftest)?$' (2 of 2; the selftest has 30 seeded failing cases, 9 clean controls and the empty-scan error, and fails when a rule has no seeded case); git merge-base --is-ancestor e38cd65 9940ebc (the audit precedes the first host/sdl commit); grep -c -i zlib docs/evidence/sdl3-host-dependency-audit.md prints 6"
        status: pass
    human_judgment: false
  - id: D2
    description: "A virtual gamepad under SDL's dummy drivers exercises button and axis mapping through the shell's own pad sampler (30 rows), the title plays through the pump into a window and an audio stream, and the window surface holds the scaled frame pixels"
    requirement: "API-05"
    verification:
      - kind: integration
        ref: "sh scripts/check-host.sh sdl: [host-sdl] sdl 3.4.10, video-driver dummy, audio-driver dummy, mapping rows 30/30, title ok, blit ok, frames 8, audio ok, [host-sdl] PASS; ten scratch mutants each fail with their named check"
        status: pass
    human_judgment: false
  - id: D3
    description: "The core, the runner and the gate stay SDL-free: no CTest name mentions sdl, the gate build and host.pump.build never look for SDL3, and no workflow, CI scanner, gate script or root CMakeLists.txt changed since the Host base"
    requirement: "API-05"
    verification:
      - kind: integration
        ref: "ctest -N | grep -c -i sdl prints 0; host.pump.build logs name no SDL (0 matches) and the host cache holds T64H_SDL OFF; git diff --name-only <Host base> HEAD -- .github scripts/check-ci-yml.sh scripts/gate.sh CMakeLists.txt is empty; sh scripts/gate.sh PASS 350 of 350 names, 186 s"
        status: pass
    human_judgment: false
  - id: D4
    description: "SC2 virtual-joystick clause state, with the hosted Linux reason and the evidence tier recorded"
    requirement: "API-05"
    verification:
      - kind: other
        ref: "PIN-IMPACT 'Host steps': '- SC2 virtual-joystick clause: closed (PASS, SDL 3.4.10, macOS 26.6.2)', '- DR-19 fit test: does not fit one task of 12-09; host-side save file omitted (...)', tier self-checking host glue only, hosted Linux: ubuntu-24.04 has no libsdl3-dev candidate, the lane is local"
        status: pass
    human_judgment: true

duration: 24min
completed: 2026-11-26
status: complete
---

# Phase 12 Plan 09: SDL3 Dependency Audit, Optional Shell, Local Lane and Host Policy Summary

**The system SDL3 package is audited and recorded before first use; the optional SDL3 shell sits on the pump (window-surface nearest blit at an integer scale, S16BE audio stream, controller only) and builds only with -DT64H_SDL=ON; `sh scripts/check-host.sh sdl` runs its headless smoke under dummy drivers with a virtual gamepad and printed `[host-sdl] PASS` here (SDL 3.4.10), closing SC2's virtual-joystick clause; `repo.host-policy` and its selftest make the D4, D6 and D7 rules mechanical; no CTest name builds SDL and the full gate passes 350 of 350.**

## Performance

- **Duration:** about 24 min (about 8 of them waiting for host load to fall below 8.5 before the full gate)
- **Started:** 2026-11-26T21:16:00Z
- **Completed:** 2026-11-26T21:40:00Z
- **Tasks:** 3 of 3 (Task 1 auto, Task 2 tracer, Task 3 auto)
- **Files:** 6 created, 6 modified; 3 commits before this record

## Accomplishments

- **Audit and policy scan (Task 1, e38cd65).** Before any SDL build a scratch CMake probe (`find_package(SDL3 3.2.0 CONFIG)` in a two-line project, deleted afterwards, nothing installed) found SDL3 3.4.10. `docs/evidence/sdl3-host-dependency-audit.md` follows the zlib audit's form: package and providers (Homebrew core formula on macOS; Ubuntu archive `libsdl3-dev`: no candidate on ubuntu-24.04, 3.2.20 on 25.10, 3.4.2 on 26.04 as the research measured), official source, zlib license, version found, acquisition trust (the installed SBOM's upstream archive hash is recorded as the package's own statement, not re-derived), purpose, lifetime, link-only use, no redistribution (D6), a D4 owner stop for any copy, and the dated pre-use statement. `scripts/check-host-policy.sh` has twelve rules (HP-FETCH, HP-EXTPROJ, HP-ADDSUB, HP-VENDOR-DIR, HP-VENDOR-FILE, HP-INSTALL, HP-BUNDLE, HP-ICON, HP-PRODUCT, HP-TITLE, HP-RENDERER, HP-KEYBOARD), strips comments before the C rules and string contents before the token rules, reads a title call that spans lines, and its selftest has 30 seeded failing cases, 9 clean controls and the empty-scan error, and fails if a rule has no seeded case. The first selftest took 4.4 s; reading a small template instead of all of host/ and a no-comment fast path in the line scanner brought it to 1.4 s (COST 1.60 from the gate's averages). `repo.host-policy` (COST 0.08) and `repo.host-policy-selftest` are registered; `PHASE12_TESTS` and the `^repo\.host-` row (3 to 5) follow.
- **Shell, smoke and lane (Task 2, tracer, 9940ebc).** `host/sdl/t64h_sdl.c` (336 non-blank lines, cap 360) copies each video packet with `t64h_frame_copy` into a tight RGBA buffer and blits it to the window surface at width times scale by height times scale with `SDL_SCALEMODE_NEAREST`; queues audio packets as S16BE stereo at the whole-Hz rounded packet rate and counts underruns (SDL plays silence while the stream is empty); samples the first gamepad each turn into `t64h_raw_pad` and maps it with `t64h_map_pad` (a removed pad is neutral; no keyboard code); queues a changed logical pad to the machine at the guest tick reached; and steps the pump with `SDL_GetTicksNS` as its clock, at most 65,536 ticks between event pumps. `t64h_sdl_main.c` is `--rom PATH [--scale N]`. `t64h_sdl_smoke.c` requires both driver variables set to dummy, attaches a virtual gamepad, runs 30 mapping rows (7 mapped and 8 unmapped buttons, horizontal axis at five values, vertical at three, the right stick, the left trigger at four values, the right trigger) through `t64h_sdl_sample_pad`, then plays the title for 8 frames and checks the window title and size, the frame geometry, a queued input event, the audio stream and, by `SDL_ReadSurfacePixel`, the scaled pixels at nine points around the title's square. `sh scripts/check-host.sh sdl` probes, configures `host/` with `-DT64H_SDL=ON` into `build/host-sdl`, builds, exports both dummy drivers, runs the smoke and prints only its fixed lines. Lane output on this host (macOS 26.6.2 arm64, SDL from the Homebrew core formula, about 2 s of configure, build and run):

```
[host-sdl] sdl 3.4.10
[host-sdl] video-driver dummy
[host-sdl] audio-driver dummy
[host-sdl] mapping rows 30/30
[host-sdl] title ok
[host-sdl] blit ok
[host-sdl] frames 8
[host-sdl] audio ok
[host-sdl] PASS
```

- **Records and gate (Task 3, 5490b2f).** PIN-IMPACT "Host steps" (new section) holds the exact SC2 line `- SC2 virtual-joystick clause: closed (PASS, SDL 3.4.10, macOS 26.6.2)`, the DR-19 line (does not fit one task; host-side save file omitted, with the measured reason), the lane record, the skip state, the hosted Linux reason (ubuntu-24.04 has no libsdl3-dev candidate; the lane is local), the tier (self-checking host glue only) and the smoke's mutant discrimination; DR-37 records the plan's refinements; the 12-09 file-count row and the HOST ledger row are updated (14 names, 10.17 s of COST, projected cold margin about 81 to 96 s, not a measurement). The full gate passes 350 of 350 names in 186 s (ctest 110 s, host load 8 to 12).

## Throwaway measurement (information only; no throughput or real-time claim)

One figure, as the plan asks and only here: `t64h_tests --case title-two-speeds` with the long script at the two fake-clock speeds 240 and 3 Hz (the case needs two speeds, so the 12-field long script runs twice) took 2.10 s of wall time (2.08 s user) at host load about 7.6 and produced 24 fields (12 video packets per speed; 26.2 million guest ticks), which is about 11 fields per second of wall time, including the process start, the ROM read and the SHA-256 digests. Information only; no throughput or real-time claim. It is not copied to the audit, PIN-IMPACT or any other record (the audit document holds no such figure by its acceptance check).

## Task Commits

1. **Task 1: SDL3 dependency audit and the host policy scan** - `e38cd65` (feat)
2. **Task 2: SDL shell, virtual-gamepad smoke and the local lane** - `9940ebc` (feat, tracer)
3. **Task 3: Host steps, DR-37, ledger row and COST from the gate** - `5490b2f` (docs)

Tracer gate: the lane's automated verify (`out=$(sh scripts/check-host.sh sdl ...)` with the PASS-or-skip grep) passed end to end before and after Task 3, so the tracer was verified end to end before expansion (nothing followed it but the records and the gate).

## Acceptance results

| Check | Result |
|-------|--------|
| `sh scripts/check-host-policy.sh --selftest` | `host-policy selftest: failures=0`; 30 lines `selftest ok: <seed> reports <RULE>`, 9 accepted controls, the empty scan an error |
| `git merge-base --is-ancestor e38cd65 9940ebc` (audit commit before the first host/sdl commit) | yes |
| `grep -c . host/sdl/t64h_sdl.c` | 336 (cap 360) |
| `grep -c -i -E 'fps\|frames per second' docs/evidence/sdl3-host-dependency-audit.md` | 0 |
| `ctest --test-dir build/gate -N \| grep -c -i sdl` | 0 |
| `sh scripts/check-host-includes.sh` and `sh scripts/check-host-policy.sh` | files 16, findings 0 each |
| lane skip branch (a scratch copy of the lane whose probe ignores the Homebrew prefix) | prints `[host-sdl] SKIP: SDL3 >= 3.2.0 not found (link-only system package; this lane installs nothing)`, exit 0 |
| lane failure branch (a found SDL3 with a garbage ROM) | `[host-sdl] FAIL: shell open`, exit 1 (never a skip) |
| scratch mutants of a copy of host/ (not committed): A and B swapped, left trigger dropped, vertical axis not inverted, wrong window title, input never queued, audio never queued, no gamepad opened, blit width halved, blit offset by one, driver variable not dummy | each fails with its named check (`mapping row 0`, `mapping row 25`, `mapping row 20`, `window title`, `input events`, `audio stream`, `the shell found no gamepad`, `blit`, `blit`, `the dummy drivers are not selected by the environment`) |
| `host.pump.build` logs and cache | 0 matches of sdl in the configure and build logs; `T64H_SDL:BOOL=OFF` |
| `git diff --name-only <Host base> HEAD -- .github scripts/check-ci-yml.sh scripts/gate.sh CMakeLists.txt` | empty; also empty for `core`, `runner` and `docs/abi` |
| the three `host/sdl` sources with the act image gcc 13.3.0, `-std=c99 -O2`, host warning set, `-Werror`, SDL3 headers copied temporarily outside the repository | all three compile (not linked, not run) |
| `sh scripts/preflight-phase12.sh --no-replay` | PASS 56 s (host load 9.3: warning printed, not a failure) |
| `sh scripts/gate.sh` | PASS 350 of 350 names, 186 s total, ctest 110 s |
| `sh scripts/pii-scan.sh --tree` | findings 0 |

## Deviations from Plan

### Auto-fixed and strengthened

**1. [Rule 3 - Blocking issue] two files more than the plan listed, and the shell is a library**
- **Found during:** Task 2
- **Issue:** the plan builds the executable from `t64h_sdl.c` and compiles the same code again as a helper library for the smoke; the first draft with `main` inside `t64h_sdl.c` under a guard counted 370 non-blank lines, over the 360 cap of the acceptance check, and compiled the shell twice.
- **Fix:** the shell is `t64h_sdl.c` plus a header `t64h_sdl.h`, built once as the static library `t64h_sdl_shell`; the executable's `main` is `t64h_sdl_main.c`; the smoke links the library. `files_modified` grows from 10 to 12 (recorded in DR-37 and the file-count table).
- **Files modified:** host/sdl/t64h_sdl.h, host/sdl/t64h_sdl_main.c (new), host/CMakeLists.txt
- **Commit:** 9940ebc

**2. [Rule 2 - Missing critical functionality] a blit check and a neutral trigger in the smoke**
- **Found during:** Task 2 (scratch mutants)
- **Issue:** a mutant that halved the blit width survived the plan's checks (window title, frames, mapping rows), and a virtual trigger axis left at 0 reads half travel (16,383) through the gamepad layer, so a pad at rest would not be neutral.
- **Fix:** the smoke reads window-surface pixels with `SDL_ReadSurfacePixel` at nine points (the corners of every scaled block) against the frame buffer and sets trigger axes to -32768 at rest; it prints two more fixed lines (`blit ok`, `audio ok`). Both blit mutants now fail.
- **Commit:** 9940ebc

**3. [Rule 2 - Missing critical functionality] the smoke refuses to run on a real display**
- **Found during:** Task 2 design
- **Issue:** the plan says the smoke requires both driver variables to be set; a value of x11 or cocoa would pop a window and read a real pad.
- **Fix:** it requires both to equal dummy and re-checks the driver names SDL reports after init; the lane sets them itself. A directory given to `--host-build` that is neither empty nor a CMake tree is refused (the lane removes the directory it builds in).
- **Commit:** 9940ebc

**4. Plan text adaptations (no rule)**
- "Host steps" did not exist in PIN-IMPACT: plan 12-09 is its first writer, so it created the section (before the gate-growth ledger); 12-12 appends the mutant table.
- The throwaway figure: `title-two-speeds` needs at least two slice speeds, so the long script ran at 240 and 3 Hz (24 fields) instead of one run; DR-20 says "with the real clock", the plan says the title-two-speeds run, which uses the fake clock: the plan's method was followed.
- Smoke lines: the plan's list had seven lines; the lane prints nine (`blit ok`, `audio ok` added), all fixed text.
- COST of the two names was first set to 0.07 and 1.35 from the local runs and corrected to 0.08 and 1.60 from the gate's `CTestCostData.txt` averages (0.08 and 1.62 s).
- API-05 stays pending (see below); `requirements-completed` is empty.

**Total deviations:** 3 auto-fixed (1 Rule 3, 2 Rule 2) and the plan text adaptations. **Impact:** no core, runner, header, golden, workflow, CI scanner, gate script or root CMakeLists.txt edit; the only edits outside host/ and scripts/ are the registration file and PIN-IMPACT.

## Auth gates

None. Nothing was installed; SDL3 was already present.

## Issues Encountered

- **Speed observation for the owner (information only, no claim).** The unthrottled pump ran about 12.5 million guest ticks per CPU second on this host (the gate's Release build), against 62.5 million ticks per second for a real-time guest, so the SDL shell with its real clock would play the title well below real time here. No record claims real-time speed (AGENTS.md: no real-time claim); the owner checklist of 12-18 should say so when a person plays it, and a faster host or core change is outside this plan.
- **No real display, no physical controller.** The pass is self-checking host glue only (dummy drivers, a virtual pad); nothing was shown on a screen or played on a device. Both belong to the owner checklist template of 12-18 (`docs/evidence/phase12-host-manual.md`); that is why API-05 is not marked complete.
- **The shell is compiled but not run on Linux.** ubuntu-24.04 has no libsdl3-dev candidate and the act image has no SDL3, so the lane can not run there (DR-17). A compile-only check of `t64h_sdl.c`, `t64h_sdl_main.c` and `t64h_sdl_smoke.c` with the act image's gcc 13.3.0 (`-std=c99 -O2` and the host warning set with `-Werror`; `--network none`; a temporary copy of the installed SDL3 headers outside the repository, removed afterwards) compiled all three; nothing was linked or run on Linux. The preflight `linux-compile` step reruns only the gate's compile units, so it does not cover `host/` at all.
- Homebrew lists a newer stable SDL3 (3.4.20) than the installed 3.4.10; the lane prints the version it ran with, and nothing was upgraded.
- Host load was 8 to 12 during the gate and was waited down to under 8.5 before it started; no limit was widened.

## Verification Results

| Check | Result |
|-------|--------|
| `cmake --build --preset gate && ctest -R '^repo\.host-policy(-selftest)?$' --no-tests=error` | 2 of 2 |
| `ctest -N -R '^repo\.host-'` | `Total Tests: 5` (the preflight row is 5) |
| `ctest -R '^(repo\.host-\|host\.pump\.)'` | 14 of 14 (host.pump names include install.setup) |
| `python3 -B scripts/check-api-init.py` (via `repo.api-init-guard`) | clean over host/ including host/sdl |
| `sh scripts/check-test-inventory.sh` (via the gate) | PASS, 350 names |
| `sh scripts/gate.sh` | PASS 350 of 350 names (348 + 2), 186 s, ctest 110 s |
| `sh scripts/check-host.sh sdl` | PASS (lines above) |
| `sh scripts/pii-scan.sh --tree` | findings 0 |

Native seconds of the new names (CTestCostData averages over four runs at host load 8 to 12): `repo.host-policy` 0.08 s (COST 0.08), `repo.host-policy-selftest` 1.62 s (COST 1.60): 1.68 s of COST added, 10.17 s for all fourteen host names. The SDL smoke is no CTest name and adds nothing to the hosted gate.

## Known Stubs

None. `host/CMakeLists.txt`'s `if(T64H_SDL)` branch is filled; no empty value flows to a display.

## Threat Flags

None new. T-12-28 (vendoring or fetching SDL) is mitigated by `repo.host-policy` and its selftest, `find_package(... CONFIG)` only and the committed audit; T-12-29 (local paths in lane logs) by sanitized stdout under a closed character set, raw logs under the ignored `build/host-sdl` and `pii-scan --tree` (findings 0); T-12-30 (product name in host strings) by the HP-PRODUCT and HP-TITLE rules; T-12-31 (the lane leaking into the gate) by zero CTest names mentioning sdl and no workflow, scanner, gate or root build edit; T-12-32 (the smoke read as rendering correctness) by the tier line recorded with the result; T-12-SC (a package install) by installing nothing, with the audit committed before the first SDL build.

## Next Phase Readiness

12-10 (the LibFuturo evaluation) is independent of the shell. 12-12 reviews the HOST wave in a scratch clone: it should run `python3 -B scripts/p12-mutants.py --run`, record the table under PIN-IMPACT "Host steps", re-run the lane on its own host (a SKIP there is the state "not closed (SKIP: ...)", never closed), measure the hosted growth of the fourteen host names (the ledger row projects a cold margin of about 81 to 96 s; `repo.host-policy-selftest` is the next reversal candidate after `host.pump.title-two-speeds`), and check that the audit commit still precedes `host/sdl`. 12-17 and 12-18 carry the SC2 line as one of the two states. API-05 and API-06 stay pending until the final audit and the owner's manual checklist.

## Self-Check: PASSED

The six created files and this SUMMARY exist; the three commits e38cd65, 9940ebc and 5490b2f are ancestors of HEAD; the working tree held only this record and the one PIN-IMPACT line when the check ran.
