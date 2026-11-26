---
phase: 12-minimal-host-playable-homebrew-and-final-bind
plan: 07
subsystem: host
tags: [host, pump, c99, fake-clock, drain-order, stall, include-guard, sdl-isolation, installed-package]

requires:
  - phase: 11
    provides: the installed tendies64 package (moved prefix of install.setup), the consumer include guard, hostloop.c, t64c_sha256, check-api-init.py
  - phase: 12-03
    provides: the frozen title guest and build/fixtures/phase12-title-s32k.z64 (gate build target t64-phase12-title-fixture)
  - phase: 12-05
    provides: the oracle values of fixtures/title (title.expected.json, title.short.input.json) and the title-in-the-consumer evidence the pump reproduces
  - phase: 12-06
    provides: the Host base 0836e40f583f8b47486ee93b0de36889e5626f0b and the TITLE ledger row
provides:
  - host/ standalone project t64h (C99, warnings as errors, find_package(tendies64 0.1), option T64H_SDL OFF with an empty branch for 12-09), never referenced by the root build
  - host/pump/t64h_pump.[ch]: pure-C pump (injected clock, origin-relative tick bound ns/16, max_ticks cap, drain order video-audio-text, held-packet sink refusal, retry-once stall rule, terminal latch, input hook seam)
  - host/tests/t64h_tests.c with cases drain-order, clock and stall on the real core through the moved prefix, and t64h_sha256 (renamed copy of the consumer's)
  - scripts/check-host.sh (build, run CASE), tools/hostcheck/CMakeLists.txt (script-only registrations), scripts/check-host-includes.sh (23-case selftest), scripts/check-host-isolation.sh (seeded trees and archives), check-api-init.py root host
  - the api.md section 8.3 tick-rate sentence (62,500,000 ticks per second, one tick per 16 ns, host policy)
  - seven CTest names (host.pump.build, drain-order, clock, stall, repo.host-include-guard, repo.host-include-guard-selftest, repo.host-isolation), PIN-IMPACT DR-35 and the HOST ledger stub
affects: [12-08, 12-09, 12-10, 12-12, 12-14, 12-18]

actuals:
  tokens: 26900
  tasks: 3
  commits: 4
plan_head_before: 10d1b8355d5a53741f60714bcaed35be52e525ca
plan_head_after: cec9345bc6d4e0a97605c302e1b6a01751a2ed05

tech-stack:
  added: []
  patterns:
    - "a host-side counting allocator aligned by arithmetic over malloc (raw pointer in the word before the block), no t64c dependency"
    - "a fake clock whose reading the case sets, and recording sinks that log the drain pass number read from the pump's own counter, so an ordering claim is checked per pass and can fail"
    - "a guard whose scanner failure is an error (awk exit 2), never a clean scan, and a selftest seed for every rule"

key-files:
  created:
    - host/CMakeLists.txt
    - host/pump/t64h_pump.h
    - host/pump/t64h_pump.c
    - host/tests/t64h_tests.c
    - host/tests/t64h_sha256.c
    - host/tests/t64h_sha256.h
    - scripts/check-host.sh
    - scripts/check-host-includes.sh
    - scripts/check-host-isolation.sh
    - tools/hostcheck/CMakeLists.txt
  modified:
    - scripts/check-api-init.py
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase12-regexes.txt
    - docs/spec/api.md
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/PIN-IMPACT.md

key-decisions:
  - "DR-35 (PIN-IMPACT): a sink refusal keeps the packet in the pump (the copy already dequeued it), the next pass offers it first, and guest movement while a packet is held is not progress, so a refusing sink ends in T64H_STALL after exactly one retry and loses nothing"
  - "The clock is read at init (origin) and once per step; the bound is (now - origin) / 16 capped by max_ticks and never behind the guest tick, so steps of any size reach the same tick; a terminal step reads no clock"
  - "The drain runs at the end of every step and at every backpressure stop inside the loop, never after a budget stop; a NULL sink drains and drops its kind"
  - "The caller owns the buffers; a packet larger than the buffer is T64H_API_ERROR with BUFFER_TOO_SMALL and stays in the ring (T-12-20)"
  - "API-05 is not marked complete: the input mapping and the scripted source (12-08) and the SDL shell (12-09) are still to come"

patterns-established:
  - "scripts/check-host.sh reads the expected neutral-frame digest and the short-script events from fixtures/title with python3 -I -B, so no expected value lives in the host test source"

requirements-completed: []

coverage:
  - id: D1
    description: "host/ is a standalone project that builds warnings-as-errors against the moved installed prefix only and is never referenced by the root build"
    requirement: "API-05"
    verification:
      - kind: integration
        ref: "ctest -R '^host\\.pump\\.build$' (check-host.sh build: configure, build, no source-tree path outside host/ and no original install path in the build tree); git grep -c 'host/' -- CMakeLists.txt prints nothing"
        status: pass
    human_judgment: false
  - id: D2
    description: "The pump drains video, then audio, then text, converts the fake clock by ns/16 from the origin, refuses a backward clock, caps a step, treats RESOURCE_LIMIT with and without movement correctly and latches a terminal stop"
    requirement: "API-05"
    verification:
      - kind: integration
        ref: "ctest -R '^host\\.pump\\.(drain-order|clock|stall)$' on the real core and the title ROM (checks 16, 110 and 47, failures 0)"
        status: pass
    human_judgment: false
  - id: D3
    description: "The host include guard, the SDL isolation guard and the init-form roots hold, each with seeded failures"
    requirement: "API-05"
    verification:
      - kind: unit
        ref: "ctest -R '^repo\\.(host-include-guard|host-include-guard-selftest|host-isolation|api-init-guard.*)$' (23 include seeds, 14 isolation seeds, 21 api-init cases)"
        status: pass
    human_judgment: false
  - id: D4
    description: "The tick-rate sentence is in api.md section 8.3 and the full gate is green with the seven new names"
    requirement: "API-05"
    verification:
      - kind: integration
        ref: "grep -c '62,500,000' docs/spec/api.md prints 1; sh scripts/gate.sh PASS 343 of 343 names, 190 s"
        status: pass
    human_judgment: false

duration: 33min
completed: 2026-11-26
status: complete
---

# Phase 12 Plan 07: Host Pump Tracer Summary

**A standalone pure-C host pump on the installed header alone: an injected clock turned into a guest-tick bound by ns/16 from the origin, a drain in video, audio, text order inside the advance loop, a retry-once stall rule that holds a refused packet and loses nothing, a terminal latch, three fake-clock cases on the real core and the title ROM, a sibling include guard and an SDL isolation guard with seeded failures, and the api.md tick-rate sentence; the full gate passes 343 of 343.**

## Performance

- **Duration:** about 33 min
- **Started:** 2026-11-26T20:09:00Z
- **Completed:** 2026-11-26T20:42:00Z
- **Tasks:** 3 of 3 (Task 1 a tracer, Task 2 tdd, Task 3 auto)
- **Files:** 10 created, 5 modified; 4 commits before this record

## Accomplishments

- **Tracer (Task 1, 159eb88).** `host/` is a standalone CMake project (`t64h`, C99 without extensions, `-Werror` with the consumer's warning set, `find_package(tendies64 0.1 CONFIG REQUIRED)`, `T64H_SDL` OFF with an empty branch). `scripts/check-host.sh build` configures it into `BUILD/host-check` against the moved prefix of `install.setup` alone and asserts that neither `build.ninja` nor `compile_commands.json` names a source-tree directory and that nothing names the original install path; `run CASE` passes the title ROM, the neutral-frame digest of `title.expected.json` and the three events of `title.short.input.json`, read by `python3 -I -B`. `host.pump.drain-order` runs the short script through the pump with a fake clock (seven slices of 262,144 ticks, then one jump to completion): every drain pass delivers video, then audio, then text, one pass (pass 8) holds all three kinds, the counts equal the oracle (3 video, 2 audio, 20 text bytes), the packet sequences are consecutive and the first video packet is `8227044b...` as expected. Mutating the pump to drain audio before video makes the case fail ("order WRONG"), so the order check can fail.
- **Clock and stall rules (Task 2, 5637430).** `host.pump.clock`: `t64h_ticks_from_ns` (0, 15, 16, 31 and the maximum), the carry kept from the origin (two 8 ns steps are one tick; 40 irregular steps each end at `now / 16`), a backward reading refused and counted with the guest tick, the advance counter and the machine digest unchanged and the reading not taken, an equal reading accepted, `max_ticks` caps one step (1,000 then 2,000) and a cap of 0 moves nothing without TIME_REVERSED. `host.pump.stall`: a video sink that refuses everything ends the step in `T64H_STALL` ("stall without progress") at guest tick 4,717,495 after exactly one retry (2 backpressure stops, 2 refusals) and, once it accepts, the held packet and the ring come out in sequence (4 packets) with the guest moving on; a sink that refuses once lets the short script finish (3 video, 2 audio, 20 text bytes, 1 refusal, consecutive sequences); two steps after completion answer `T64H_TERMINAL` with the same tick and stop reason, the advance count, the input hook calls, the digest and the sinks untouched; a 1,024-byte video buffer is `T64H_API_ERROR` with `BUFFER_TOO_SMALL` and the 49,152-byte packet is still queued.
- **Guards and the sentence (Task 3, d66562b).** `scripts/check-host-includes.sh` (HOST-INCLUDE, HOST-SDL-INCLUDE, HOST-SDL-IDENT, HOST-PUMP-TIME, HOST-CMAKE; 23 seeded cases and the empty-scan error), `scripts/check-host-isolation.sh` (ISO-SDL, ISO-ROOT, ISO-ARCHIVE, ISO-GOLDEN over 494 files and two archives; 14 seeded cases including a real seeded archive), `check-api-init.py` default roots plus `host` (two seeded cases, 211 files, 0 findings) and one sentence in `docs/spec/api.md` section 8.3. The preflight (`--no-replay`) passes in 66 s, the three host sources compile clean with the Linux act-image compiler (gcc 13.3, `-O2`, the host warning set plus `-Werror`), and the full gate passes 343 of 343 names in 190 s (ctest 106 s).
- **Records (cec9345).** DR-35 and the HOST gate-growth ledger stub in PIN-IMPACT (a projection of +16 to +24 s hosted, flagged as not a measurement).

## Task Commits

1. **Task 1: pump tracer** - `159eb88` (feat)
2. **Task 2: clock and stall rules** - `5637430` (feat)
3. **Task 3: guards, init-form roots, api.md sentence** - `d66562b` (feat)
4. **Records: DR-35 and the HOST ledger stub** - `cec9345` (docs)

## RED reports (Task 2, run against the Task 1 tracer pump)

The Task 1 pump counted each step's delta alone, did not refuse a backward clock, ignored `max_ticks`, used the research Pattern 4 stall flag (reset by any movement) and had no terminal latch. Every behavior below failed as a real defect of that pump (none was verification-only); the run took 0.2 s (clock) and 1.1 s (stall).

| Case | First failed assertion and observed value | Other failures |
|------|------------------------------------------|----------------|
| clock | `t64h_tests.c:627` `result.tick is 0, expected 1`: two steps of 8 ns advanced 0 ticks in total (the carry was lost per step) | 40 of the irregular steps ended below `now / 16` (for example 4 against 6); the backward clock answered `ok` (counter 0, expected 1; 43 advances, expected 42); the cap was ignored (`result.tick is 1000017, expected 1037` and 2037); 49 of 110 checks failed |
| stall | `t64h_tests.c:709` `stall_retries is 2, expected 1`: the Pattern 4 flag was reset by the movement that the held copy made possible, so the refusing sink cost two retries (3 backpressure stops, expected 2) | the recovery step did not move (per-step arithmetic); both steps after completion answered `ok` and called advance and the input hook again (1,035 against 1,033); 9 of 47 checks failed |
| drain-order | none: it is the Task 1 case and passed | small-buffer sub-run passed immediately (the Task 1 pump already sizes from `t64_machine_output_status`): verification-only, kept as the T-12-20 pin |

GREEN changed only the pump: origin-relative arithmetic, the backward refusal, the cap with the never-behind clamp, the held-aware stall rule (movement clears the stall watch only while nothing is held; a pass that consumed something clears it) and the terminal latch.

## Deviations from Plan

### Auto-fixed and strengthened

**1. [Rule 2 - Missing critical functionality] drain-order runs the short script, not "no input for three fields"**
- **Found during:** Task 1 design
- **Issue:** with no input the title produces only video, so no drain pass holds all three kinds and the order assertion would be vacuous.
- **Fix:** the case queues the three events of `title.short.input.json` (passed by `check-host.sh`, read from the fixture, queued directly before the machine runs) and asserts that one pass delivers video, audio and text in that order. The first video packet is still the neutral frame, equal to `--expect`.
- **Files modified:** host/tests/t64h_tests.c, scripts/check-host.sh
- **Commit:** 159eb88

**2. [Rule 2 - Missing critical functionality] sink refusal needs a held packet (DR-35)**
- **Found during:** Task 1 design of the stall rule
- **Issue:** `t64_video_copy` dequeues, so a refusing sink can not leave the packet in the ring; the plan's "retry once" model assumed it could. Pattern 4's rule then costs two retries because the held copy frees a slot.
- **Fix:** the pump keeps a refused packet, offers it first at the next pass and treats movement while something is held as no progress; recorded as DR-35. Result: one retry, STALL, nothing lost.
- **Files modified:** host/pump/t64h_pump.c, host/pump/t64h_pump.h
- **Commit:** 5637430

**3. [Rule 2 - Missing critical functionality] additions that make a mitigation or a guard able to fail**
- T-12-20 (BUFFER_TOO_SMALL as an API error with the packet left in the ring) got a sub-run of `host.pump.stall` (0.2 s); the plan only named the mitigation.
- `check-host-includes.sh` gained HOST-SDL-IDENT, HOST-PUMP-TIME and HOST-CMAKE beyond the include and SDL3-include rules the plan lists (the pump and its tests name no SDL type, the pump includes no `<time.h>`, the host CMake names no source-tree path).
- `check-host-isolation.sh` has its own selftest (`--with-selftest`, run inside the single registered name `repo.host-isolation`), including a real archive with an SDL symbol, so the guard is shown able to fail. Its archive scan always reads the core archive of the build tree (the file `cmake --install` copies byte for byte) and the installed copy when readable, because `install.setup` removes and rebuilds `install-check` and could race a scan of the moved prefix alone.
- `check-host.sh build` asserts that the build names no source-tree path and no original install path (T-12-22 in build form).
- A scanner failure is an error in `check-host-includes.sh` (an awk failure was first reported as a clean scan by the unfinished version and was caught by its own selftest).
- **Commits:** 159eb88, 5637430, d66562b

**4. Plan text adaptations (no rule)**
- The input hook (`t64h_input_fn`, called before every advance, never after a terminal stop) exists now as the attachment point the 12-08 plan names; DR-11's counter of deferred events arrives with the 12-08 source. "Queues nothing after a terminal stop" is pinned in 12-07 by the hook call count and the unchanged advance count and digest.
- The tracer pump omitted by design (and Task 2 completed) the carry, the backward refusal, the cap, the stall rule and the terminal latch, so that the RED run was genuine; the Task 1 commit says so in the source header of that version.

**Total deviations:** 3 auto-added (Rule 2) and 1 plan-text adaptation. **Impact:** more coverage than planned, no scope outside host/, scripts, the registration file and one api.md sentence.

## Auth gates

None.

## Issues Encountered

- A first version of `check-host-includes.sh` passed the awk header list with embedded newlines, which the macOS awk refuses; the scan printed an awk error and still counted the files as clean. The selftest reported the 12 seeds that should have failed. Fixed by a space-separated list and by making an awk exit above 1 an error (exit 2).
- `repo.host-isolation` first failed on its own CMake comment (the registration file under `tools/` named the SDL3 tokens); reworded without the tokens, which is also the rule for later plans that touch `tools/hostcheck`.
- Host load was 8 to 16 during every measurement; no limit was widened.

## Verification Results

| Check | Result |
|-------|--------|
| `cmake --build --preset gate && ctest -R '^host\.pump\.(build\|clock\|stall\|drain-order)$'` | 5 of 5 (with `install.setup`) |
| `ctest -N -R '^host\.pump\.'` | `Total Tests: 3` after Task 1, `5` after Task 2 |
| `ctest -R '^repo\.(host-include-guard\|host-include-guard-selftest\|host-isolation\|api-init-guard.*)$'` | 5 of 5 |
| `sh scripts/check-host-includes.sh --selftest` | 23 seeds, one rule id each, failures 0 |
| `sh scripts/check-host-isolation.sh --build build/gate --with-selftest` | 14 seeds ok; files 494, archives 2, findings 0 |
| `python3 -B scripts/check-api-init.py` and `--selftest` | files 211, findings 0; 21 cases, failed 0 |
| `sh scripts/check-test-inventory.sh --preset gate` | PASS, 340 then 343 names |
| `sh scripts/preflight-phase12.sh --no-replay` | PASS 66 s (linux-compile 234 units gcc 13.3; ctest counts incl. `^host\.pump\.` 5 and `^repo\.host-` 3) |
| Host sources with the act-image gcc 13.3 (`-std=c99 -O2` host warning set `-Werror`) | all three compile |
| `sh scripts/gate.sh` | PASS 343 of 343 names, 190 s total, ctest 106 s |
| `grep -c -E 'clock_gettime\|gettimeofday\|[^_a-z]time\(\|nanosleep\|usleep\|pthread_\|fopen' host/pump/t64h_pump.c` | 0 |
| `git grep -c 'host/' -- CMakeLists.txt` | prints nothing |
| `git diff --name-only 0836e40..HEAD -- CMakeLists.txt core/include docs/abi .github scripts/gate.sh scripts/check-ci-yml.sh` | empty |
| `ctest --show-only=json-v1 -R '^host\.pump\.drain-order$'` | REQUIRED_FILES names `phase12-title-s32k.z64`; FIXTURES_REQUIRED `t64_host` |
| `sh scripts/pii-scan.sh --tree` | findings 0 |

Native seconds of the new names (`CTestCostData.txt` averages of the gate run): `host.pump.build` 0.57, `host.pump.drain-order` 0.44, `host.pump.clock` 0.11, `host.pump.stall` 1.42, `repo.host-include-guard` 0.05, `repo.host-include-guard-selftest` 1.16, `repo.host-isolation` 1.36 (5.10 s; COST 0.50, 0.45, 0.12, 1.40, 0.05, 1.15, 1.30 = 4.97 s). Gate-growth: see the HOST stub in PIN-IMPACT (a projection, not a measurement; the cold margin could fall under the 120 s warning level with the 12-08 and 12-09 names).

## Known Stubs

- `host/CMakeLists.txt` `if(T64H_SDL)` is an empty branch, by plan design; plan 12-09 fills it. No data path is stubbed. Not recorded in a windows ledger (the repository has none).

## Threat Flags

None: the host test binary reads one file (the ROM named by `--rom`) and nothing else; no network, no new endpoint, no trust-boundary change beyond the plan's threat model (T-12-20 to T-12-23 are mitigated and pinned: `host.pump.stall` small buffer, `host.pump.stall` retry-once rule, `repo.host-include-guard(-selftest)` plus the build-only-the-moved-prefix assertion of `host.pump.build`, `repo.host-isolation`).

## API observations for the 12-10 triage (candidates, none a defect, nothing changed)

- **O-1 no non-destructive peek.** `t64_video_copy` and `t64_audio_copy` dequeue, so a host whose sink can refuse a packet must hold it itself (DR-35); the effective backpressure is ring plus held packet. An additive "copy without dequeue" would be an amendment and is not recommended; the held-packet rule works.
- **O-2 no guest-tick query.** No public call reports the current guest tick (the state-import report carries none); the pump learns it only from `result.tick` and therefore requires a machine at tick 0 at init. Resuming a pump on an imported state would need a tick source. Documented in the pump header; relevant only if a later host resumes imported states.
- **O-3 (extends C-5).** A `RESOURCE_LIMIT` stop that moved nothing also arises from a masked handler status, not only from ring pressure; two such stops at the same tick would read as the permanent stall of api.md section 8.1 under the documented rule. Not observed in any authored guest.
- C-1 (tick rate) is documented by the new sentence; C-4 (maximum video packet size) is a host constant of the pump (`T64H_VIDEO_BUFFER_BYTES`, 307,200).

## Next Phase Readiness

12-08 attaches the scripted source behind `t64h_input_fn`, adds `t64h_input` (mapping), the frame and audio helpers and the two-speed title comparison on this pump and these cases; `check-host.sh run` already passes the title ROM, `--expect` and the short events. 12-12 measures the HOST gate growth (the stub names `host.pump.stall` and the two selftests as the first reversal candidates after `installed.consumer.title-persist`), and 12-14 moves the `find_package(tendies64 0.1 ...)` line of `host/CMakeLists.txt` (PIN-IMPACT B-33). API-05 stays pending until 12-08 and 12-09.

## Self-Check: PASSED

All ten created files, the SUMMARY and the four commits (159eb88, 5637430, d66562b, cec9345) were found; the working tree was clean apart from this record.
