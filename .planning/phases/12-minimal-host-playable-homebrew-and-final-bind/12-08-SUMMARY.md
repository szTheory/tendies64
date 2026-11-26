---
phase: 12-minimal-host-playable-homebrew-and-final-bind
plan: 08
subsystem: host
tags: [host, pump, c99, input-mapping, t64-input, scripted-source, frame-copy, two-speeds, mutants, fake-clock]

requires:
  - phase: 12-07
    provides: the standalone host project t64h, the pump with its input hook seam, check-host.sh (build, run CASE), the HOST guards and the t64h_sha256 copy
  - phase: 12-05
    provides: fixtures/title/title.expected.json, the committed t64.input scripts and the consumer-recorded pins
  - phase: 12-04
    provides: the runner-recorded pins of the short scenario
provides:
  - host/pump/t64h_script.[ch]: the strict t64.input v1 reader (closed keys, decimal-string ticks, port 0, ranges, size and event limits, one named rule per refusal) and the scripted source behind the pump's input hook (lazy queueing, QUEUE_FULL deferral by lowering the bound, nothing queued after a terminal stop)
  - host/pump/t64h_input.[ch]: t64h_raw_pad (no SDL type) and t64h_map_pad, the one mapping table of DR-12
  - host/pump/t64h_frames.[ch]: t64h_frame_copy (stride_bytes, destination pitch, 320x240 limit, checks before the first write) and t64h_swap_s16be
  - host.pump.title-two-speeds: the title's short and long scripts through the pump at 240 Hz and 3 Hz of fake time, byte-identical and equal to the oracle and the headless pins (scripts/check-host-title.py)
  - scripts/p12-mutants.py (8 pump and 4 title mutants, 2 controls; --list, --check-anchors, --run) and CTest repo.p12-mutant-anchors
  - five CTest names (host.pump.title-two-speeds, host.pump.script, host.pump.input-map, host.pump.frames, repo.p12-mutant-anchors), PIN-IMPACT DR-36 and the HOST ledger update
affects: [12-09, 12-10, 12-12, 12-14, 12-17, 12-18]

actuals:
  tokens: 33400
  tasks: 4
  commits: 7
plan_head_before: fb5a7d7eb1d9051bd56594df6e7c4fb65382e787
plan_head_after: 16cd072ce9f0c334803cb9c86b80afff9bb73bd6

tech-stack:
  added: []
  patterns:
    - "an input hook that may lower the advance bound, so a deferred event can never be passed by the guest"
    - "a comparator that reads the lines of a binary and judges them against an oracle file and recorded pins, with a seeded selftest run first and scratch overrides by environment variable"
    - "seed documents written with a single quote for the double quote and unquoted before the read"
    - "a mutant driver whose kill needs a failed check line of its own test file, so a crash or a timeout is never a kill"

key-files:
  created:
    - host/pump/t64h_script.h
    - host/pump/t64h_script.c
    - host/pump/t64h_input.h
    - host/pump/t64h_input.c
    - host/pump/t64h_frames.h
    - host/pump/t64h_frames.c
    - scripts/check-host-title.py
    - scripts/p12-mutants.py
  modified:
    - host/pump/t64h_pump.h
    - host/pump/t64h_pump.c
    - host/tests/t64h_tests.c
    - host/CMakeLists.txt
    - scripts/check-host.sh
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase12-regexes.txt
    - tools/hostcheck/CMakeLists.txt
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/PIN-IMPACT.md

key-decisions:
  - "DR-36 (PIN-IMPACT): the input hook takes uint64_t *bound and may lower it to the tick of an event the core refused with QUEUE_FULL, so the guest never passes an unqueued event; a deferred event the guest already reached is an input error; the deferred-events counter is t64h_script_source.deferred"
  - "The vertical axis is inverted by complement (-1 - value), not negation, so full host down (+32767) reaches -128; the dead zones are -2047..2047 horizontally and -2048..2046 vertically (4,095 values each); Z needs a left trigger of at least 16,384"
  - "The 4,096-event cap of the reader is unreachable at the 65,536-byte cap (a minimal event is 46 bytes); its refusal is pinned through raised byte limits on the same code path, as replay.md does for its caps"
  - "API-05 and API-06 are not marked complete: the SDL shell and its lane (12-09), the API-07 evaluation and the final audit still stand between them and a completed requirement"

patterns-established:
  - "check-host.sh run CASE has pure cases (input-map, frames) that need no ROM, a script case that is given each committed script with the events python3 reads from the same file, and title-two-speeds that pipes the binary's lines into the comparator"
  - "check-host.sh --host-build DIR builds and runs a copy of host/ beside the gate tree (the mutant driver uses it)"

requirements-completed: []

coverage:
  - id: D1
    description: "The title's short and long scripts run through the pump's scripted source with a fake clock at 240 Hz and 3 Hz give byte-identical video checkpoints, audio packets and stream, transcript, exported save and final digest, equal to fixtures/title/title.expected.json and to the headless pins (runner for short, consumer for long)"
    requirement: "API-06"
    verification:
      - kind: integration
        ref: "ctest -R '^host\\.pump\\.title-two-speeds$' (check-host.sh run title-two-speeds: short 3 frames, 2 audio packets, completion tick 3693938; long 12 frames, 4 audio packets, completion tick 13078313; both speeds equal; comparator selftest first)"
        status: pass
    human_judgment: false
  - id: D2
    description: "The input mapping table, the strict t64.input reader, the scripted source rules and the frame and audio helpers are pinned by tests that need no SDL and no device"
    requirement: "API-05"
    verification:
      - kind: unit
        ref: "ctest -R '^host\\.pump\\.(input-map|script|frames)$' (checks 105, 281 and 39, failures 0); grep -c -E '#include <SDL|SDL_[A-Za-z]' host/pump/*.c prints 0 per file"
        status: pass
    human_judgment: false
  - id: D3
    description: "scripts/p12-mutants.py holds 8 pump and 4 title mutants with their killing tests and anchors, gated by repo.p12-mutant-anchors; the kill run is the 12-12 review's"
    requirement: "API-05"
    verification:
      - kind: unit
        ref: "ctest -R '^repo\\.p12-mutant-anchors$'; python3 -B scripts/p12-mutants.py --list (12 mutants, 2 controls); scratch emulation of --run on a plain copy of HEAD: 12 KILLED, EQUIV SURVIVED, INVALID INVALID"
        status: pass
    human_judgment: true
  - id: D4
    description: "The full gate passes with the five new names and the host sources compile under the Linux act-image compiler"
    requirement: "API-05"
    verification:
      - kind: integration
        ref: "sh scripts/gate.sh PASS 348 of 348 names, 149 s; sh scripts/preflight-phase12.sh --no-replay PASS 46 s; host sources with gcc 13.3 -std=c99 -O2 -Werror compile"
        status: pass
    human_judgment: false

duration: 33min
completed: 2026-11-26
status: complete
---

# Phase 12 Plan 08: Host Input Mapping, Strict Scripted Source, Frame Helpers and Two-Speed Equality Summary

**The title's short and long input scripts run through the pump's scripted source with a fake clock at 240 Hz and 3 Hz of fake time and give byte-identical frames, audio, transcript, exported save and final digest at both speeds, equal to the oracle and to the headless pins; the logical input mapping, the strict t64.input reader, the scripted source rules and the frame and audio helpers are pinned by device-free cases; a mutant driver holds 8 pump and 4 title mutants with named killing tests and gated anchors; the full gate passes 348 of 348.**

## Performance

- **Duration:** about 33 min
- **Started:** 2026-11-26T20:44:00Z
- **Completed:** 2026-11-26T21:17:00Z
- **Tasks:** 4 of 4 (Task 1 a tracer, Tasks 2 and 3 tdd, Task 4 auto)
- **Files:** 8 created, 9 modified; 7 commits before this record

## Accomplishments

- **Tracer (Task 1, d5a1469).** `host/pump/t64h_script.[ch]` hold the strict reader and the source behind the pump's hook; the hook now takes `uint64_t *bound` and may lower it (DR-36). Case `title-two-speeds` runs each `--script` at each `--slice-hz` speed (the fake clock advances 1e9 / hz ns per step until the guest completes) and prints one JSON line per run. `scripts/check-host-title.py` judges the lines: at least two distinct speeds per script, every run-dependent value equal across the speeds and equal to `title.expected.json` (frames in order, audio packets and stream, transcript, save) and to the recorded pins (short against the runner section, long against the consumer section); every mismatch names its key and speed; a seeded selftest (12 edited sets of lines plus a changed pin and an empty set, each of which it must report by key) runs first. The short script at 240 Hz (15 steps, 0 mid-step drain stalls) and at 3 Hz (1 step, 1 drain stall) completes at tick 3,693,938 with the runner's final digest `57b683df...`.
- **Mapping and reader rules (Task 2, 953953c).** `t64h_map_pad` is one table: axis divided by 256 (truncating) and clamped, dead zone below 8, vertical inverted by complement, A 15, B 14, Start 12, D-pad up, down, left, right 11 to 8, Z from a left trigger of 16,384, every other control unmapped, a pad that is not present neutral. Case `input-map` (105 checks) pins the rows, compares all 65,536 axis values with a formula written by sign and magnitude, counts 4,095 zero outputs per axis and checks each mapped and unmapped control, the trigger threshold and removal. Case `script` (281 checks) holds 50 seeded documents (one per rule plus accepted variants), the 65,536 and 65,537 byte boundary, the event limits through raised and lowered limits, the committed scripts against the events python3 reads from the same files (3 and 12), the source against a model of a two-entry queue (lazy queueing, QUEUE_FULL deferral, an unreachable deferred event, another refusal, NULL arguments) and against the real title (3 of 4 events queued before the completion at tick 3,693,938, no source call after it).
- **Frames and the long script (Task 3, e14942b, 40b7bbb).** `t64h_frame_copy` honors `stride_bytes` and a destination pitch, refuses over 320x240 (321 and 241 refused, 320x240 accepted), a bad geometry, another encoding, a short source or a destination one row short (left untouched), all before the first write; `t64h_swap_s16be` swaps S16BE (0x12 0x34 is 0x1234, 0x80 0x00 is -32768). Case `frames` has 39 checks. `title-two-speeds` now runs the long script too: 12 frames, 4 audio packets, a 20-byte transcript, the save image, completion tick 13,078,313 and the final digest equal at both speeds, equal to the oracle and to the consumer-recorded pins. Mid-step drain stalls (information only): short 0 at 240 Hz and 1 at 3 Hz, long 0 at 240 Hz and 5 at 3 Hz; steps 15 and 51 at 240 Hz, 1 at 3 Hz.
- **Mutants (Task 4, ddf7834, 3f09aab).** `scripts/p12-mutants.py` (frame of `p10-mutants.py`): `--list` prints "ID, killing CTest name, anchor file, expected class, title" for P12-P01 to P12-P08, P12-T01 to P12-T04 and the controls P12-C-EQUIV and P12-C-INVALID; `--check-anchors` (CTest `repo.p12-mutant-anchors`, COST 0.10) requires every anchor unique and different from its replacement, every killing test registered and every case present in the host test binary; `--run` builds in a throwaway worktree (pump mutants rebuild `host/` through `check-host.sh --host-build`, title mutants rebuild the title ROM from copied pinned prerequisites and are judged through the comparator), requires a clean tree, prints no absolute path and writes `build/gate/12-p12-results.txt`.
- **Records (16cd072).** PIN-IMPACT DR-36 and the HOST ledger update (12 names, 8.49 s of COST, projection for 12-12).

## Task Commits

1. **Task 1: scripted source and the short script at two speeds** - `d5a1469` (feat)
2. **Task 2: input mapping table, reader and source cases** - `953953c` (feat)
3. **Task 3: frame helpers, the long script, COST** - `e14942b` (feat), `40b7bbb` (fix: init form in the frames case)
4. **Task 4: mutant driver and anchors** - `ddf7834` (feat), `3f09aab` (fix: the P02 mutant)
5. **Records: DR-36 and the HOST ledger** - `16cd072` (docs)

## RED reports

| Case | Run against | First failed assertion and observed value | Other failures |
|------|-------------|-------------------------------------------|----------------|
| input-map | a stub `t64h_map_axis` and `t64h_map_pad` that return neutral | `t64h_tests.c:1210` `(unsigned)(pad.x + 128) is 128, expected 0` (the first pinned row, -32768 horizontal) | 45 failed checks before the sweep stopped at its first failing value; the button rows, the trigger and the removal rows also failed |
| script | a reader that accepts every document and reads nothing (a scratch edit of `t64h_script.c`, restored afterwards) | `seed "unknown key in an event": rule ok, expected unknown-key` | 64 of 221 checks: every refusal seed, the size and event limits, the committed scripts (0 events instead of 3 and 12) and the terminal case (no events, so the title never completed) |
| frames | a copy that ignores the geometry (one `memcpy` of the packet) and an identity swap | `t64h_tests.c:1880` `rows_equal` (the stride-520 packet: the stub reads rows at the row length) | 20 failed checks: the guard sentinel, the pitch case, 321 and 241, the short destination and source, the geometry, encoding and NULL answers, the swap |

The tight 128x96 stride-512 copy passed against the frames stub (stride and pitch agree there), so the case's first red row is the stride-520 one. GREEN changed only the production files: the real `t64h_input.c`, the real reader and the real `t64h_frames.c`.

## Acceptance results

| Check | Result |
|-------|--------|
| comparator output, short script | `3 and 240 Hz equal; 3 frames, 2 audio packets (stream), 20-byte transcript, save image, completion tick 3693938 and final digest equal the oracle and the runner pins` |
| comparator output, long script | `3 and 240 Hz equal; 12 frames, 4 audio packets (stream), 20-byte transcript, save image, completion tick 13078313 and final digest equal the oracle and the consumer pins` |
| one digit of a scratch copy of `title.runner-pins.json` passed through `T64_TITLE_PINS` | the comparator fails at both speeds: `key final_digest differs from the runner pin final_state_digest: got "57b683df...", want "07b683df..."` (the copy stays in the session scratch area, not committed) |
| `grep -c -E '#include <SDL\|SDL_[A-Za-z]' host/pump/t64h_input.c host/pump/t64h_script.c` | 0 and 0 |
| `grep -c -E 'clock_gettime\|gettimeofday\|nanosleep\|usleep\|SDL_[A-Za-z]' host/pump/*.c host/pump/*.h` | 0 for every file |
| `ctest -N -R '^host\.pump\.'` | `Total Tests: 9` (eight host names plus install.setup) |
| `--list`: 12 `P12-[PT]nn` lines and 2 `P12-C-` lines; every second field is in `ctest -N` and every third field is a file | yes |

## Deviations from Plan

### Auto-fixed and strengthened

**1. [Rule 2 - Missing critical functionality] the full strict reader is in the Task 1 tracer**
- **Found during:** Task 1 design
- **Issue:** the plan stages "finish the script reader's refusals" in Task 2, but the reader is the trust boundary of T-12-24 (untrusted text) and a lenient reader committed even for one task contradicts the mitigation.
- **Fix:** `t64h_script.c` carries every rule from the first commit; Task 2's RED for the case `script` ran against an accept-all reader (a scratch edit, restored), recorded above.
- **Commit:** d5a1469

**2. [Rule 1 - Bug in the plan text] the committed short script has 3 events, not 4**
- **Found during:** Task 2
- **Issue:** the behavior bullet says the committed short script "parses to 4 events"; `title.short.input.json` and `title.expected.json` (`inputs.short.events`) both say 3 (the long script has 12).
- **Fix:** the case compares each committed script with the events python3 reads from the same file (3 and 12), so no count is a literal of the case. The sub-case "nothing is queued after completion" adds one event beyond the completion to the three, which is where a fourth event appears.
- **Commit:** 953953c

**3. [Rule 2 - Missing critical functionality] the hook may lower the bound**
- **Found during:** Task 1 design of "QUEUE_FULL deferred to the next step"
- **Issue:** with the hook signature of 12-07 a source can only skip an event; the guest then runs past its tick and the event is refused later (TIME_REVERSED), which is a lost input.
- **Fix:** `t64h_input_fn` takes `uint64_t *bound`; the source lowers it to the deferred event's tick, the pump never raises it and keeps it at the guest tick or above (DR-36, pinned by the model-queue cases of `script`).
- **Files modified:** host/pump/t64h_pump.[ch], host/tests/t64h_tests.c
- **Commit:** d5a1469

**4. [Rule 3 - Blocking issue] a guard finding after the Task 3 commit**
- **Found during:** Task 3 verification (the guard names were run after the first commit)
- **Issue:** `repo.api-init-guard` (roots include `host/` since 12-07) flagged two `t64_video_packet_info` declarations without the INIT form in the frames case.
- **Fix:** both initialized with `T64_VIDEO_PACKET_INFO_INIT`; lesson: run the guard names before each code commit.
- **Commit:** 40b7bbb

**5. [Rule 1 - Bug] the first "queue after advance" mutant was equivalent**
- **Found during:** Task 4 (scratch emulation of `--run`)
- **Issue:** a hook call after every advance of the loop queues each event before the next poll (each advance of a step is a few thousand ticks), so the mutant SURVIVED.
- **Fix:** P12-P02 now calls the hook once at the end of a step, which the 3 Hz run of `host.pump.title-two-speeds` kills (`completed` fails: the guest ran to the bound without the events and the late call is TIME_REVERSED).
- **Commit:** 3f09aab

**6. Plan text adaptations (no rule)**
- `host.pump.input-map` and `host.pump.frames` need no ROM, so they carry no REQUIRED_FILES; `host.pump.script` and `host.pump.title-two-speeds` do (one short real-core sub-case, four real-core runs).
- The plan's "seeded files" are seed documents inside the case, written with a single quote for the double quote.
- The 4,097-event refusal is reached through raised limits (see DR-36 item 3); the default limits answer `size` for the same document. The Task 2 commit message says 47 seeds; the count is 50.
- `check-host.sh` gained `--host-build DIR` (used by the mutant driver) and a `cd` to the repository root for the cases that pass relative script paths.

**Total deviations:** 4 auto-fixed (2 Rule 2, 1 Rule 1 in the plan text, 1 Rule 3) plus 1 mutant fix (Rule 1) and the plan text adaptations. **Impact:** no scope outside host/, scripts, the registration file and PIN-IMPACT; no core, runner, header or bound-file edit.

## Auth gates

None.

## Issues Encountered

- **`--run` of the driver was not executed in this session.** The permission classifier denied `python3 -B scripts/p12-mutants.py --run ...` (it creates and removes a git worktree and runs `git checkout` in it). I did not work around that on the repository. Instead a scratch script (not committed, outside the repository) extracted `git archive HEAD` into a plain directory, imported the driver's table and classification rule and made the same `check-host.sh --host-build` and `build-title-fixture.sh --build` calls there: the baseline built (the title ROM built from the scratch copy equals its manifest row), and 12 mutants were KILLED by their named case (P12-P01 `clock`, P02 `title-two-speeds`, P03 `drain-order`, P04 and P08 `stall`, P05 `frames`, P06 and P07 `input-map`, T01 to T04 `title-two-speeds`), P12-C-EQUIV SURVIVED and P12-C-INVALID was INVALID (one run, P02 after its fix). What is unexercised: the worktree creation and removal, the `Worktree.revert` path (`git checkout -- <files>` and the clean-status proof), the `RESULTS` file write and the clean-tree and prerequisite checks of `run_mode`. The 12-12 review is therefore the first execution of those lines: if the classifier blocks `--run` there too, the orchestrator has to run it.
- Host load was 6 to 12 during measurements; no limit was widened. `host.pump.title-two-speeds` measured 2.81, 3.02 and 3.28 s native.

## Verification Results

| Check | Result |
|-------|--------|
| `cmake --build --preset gate && ctest -R '^host\.pump\.'` | 9 of 9 (with `install.setup`) |
| `ctest -N -R '^host\.pump\.'` | `Total Tests: 6` after Task 1, `8` after Task 2, `9` after Task 3 |
| `ctest -R '^repo\.(host-include-guard\|host-include-guard-selftest\|host-isolation\|api-init-guard.*\|p12-mutant-anchors)$'` | 6 of 6 |
| `python3 -B scripts/check-api-init.py` | files 217, findings 0 |
| `sh scripts/check-test-inventory.sh --preset gate` | PASS, 348 names |
| `sh scripts/preflight-phase12.sh --no-replay` | PASS 46 s (ctest counts incl. `^host\.pump\.` 9 and `^repo\.p12-mutant-anchors$` 1; linux-compile 234 units gcc 13.3) |
| host sources (`t64h_pump.c`, `t64h_script.c`, `t64h_input.c`, `t64h_frames.c`, `t64h_tests.c`, `t64h_sha256.c`) with the act-image gcc 13.3, `-std=c99 -O2`, host warning set, `-Werror` | all six compile |
| `sh scripts/gate.sh` | PASS 348 of 348 names, 149 s total, ctest 89 s |
| `sh scripts/pii-scan.sh --tree` | findings 0 |
| `git diff --name-only fb5a7d7..HEAD -- core runner docs/abi tools/abi CMakeLists.txt` | empty (no core, runner, header or golden edit; the root build never references host/) |

Native seconds of the new names (three ctest runs, host load 6 to 11): `host.pump.title-two-speeds` 2.81, 3.02, 3.28 (COST 3.00), `host.pump.script` 0.34 to 0.40 (COST 0.38), `host.pump.input-map` 0.01 to 0.02 (COST 0.02), `host.pump.frames` 0.01 to 0.02 (COST 0.02), `repo.p12-mutant-anchors` 0.07 (COST 0.10): 3.52 s of COST added, 8.49 s for all twelve host names. The title-two-speeds figure is above the research projection of 1.8 s because it runs about 33.5 million guest ticks (four runs); the gate-growth projection for 12-12 is in the HOST ledger row of PIN-IMPACT (not a measurement: cold margin about 89 to 101 s, under the 120 s warning level; `host.pump.title-two-speeds` is now the first host candidate of the reversal rule, with fewer ticks rather than removal as its first step because it is the evidence of success criterion 2).

## Known Stubs

None. `host/CMakeLists.txt` `if(T64H_SDL)` stays the empty branch of 12-07 (plan 12-09 fills it).

## Threat Flags

None. The test binary reads the files named by `--script` (bounded read, refused by the reader beyond 65,536 bytes) and `--rom`; no network, no new endpoint. T-12-24 (hostile or oversized script) is mitigated and pinned by the seeds and limits of `host.pump.script`; T-12-25 (queue exhaustion) by lazy queueing, deferral by lowering the bound and no queueing after a terminal stop (`host.pump.script`); T-12-26 (frame copy overrun) by the checks before the first write (`host.pump.frames`, mutant P12-P05); T-12-27 (a speed claim) by lines without any time figure (`info` counters are never compared).

## API observations for the 12-10 triage (candidates, none a defect, nothing changed)

- **O-4 no query of free event-queue slots.** `t64_machine_queue_input` answers QUEUE_FULL only after the fact and the capacity (256 in every authored profile) is shared with scheduler events, so a host that must not lose input defers by lowering its advance bound (DR-36). Not a defect; an additive "free slots" read would remove the guess.
- **O-5 the refusal of a late event is against the tick reached, as documented.** Measured on the title ROM: after an advance that stopped at tick 3,581, `queue_input` at tick 10 and at tick 3,580 answered TIME_REVERSED and at tick 3,581 answered OK, so the api.md text (section 10.2, "a tick at or after the current guest tick") holds; the first draft of mutant P02 had suggested otherwise only because each advance of a step is short.
- The runner parser caps a schedule at 128 events and the host reader at 4,096 (DR-13, intended); a script that the host accepts above 128 events is not a script the runner can run.

## Next Phase Readiness

12-09 adds the SDL shell on this pump (the mapping table and `t64h_frame_copy` are its inputs; the shell fills `t64h_raw_pad` and calls `t64h_map_pad`, copies packets with `t64h_frame_copy`, swaps audio with `t64h_swap_s16be`), the host policy guard and the SDL lane; it must keep `tools/hostcheck/CMakeLists.txt` and `host/pump` free of SDL tokens (the isolation and include guards scan them). 12-12 runs `python3 -B scripts/p12-mutants.py --run` in the review clone (first execution of the worktree code, see Issues), records the table in PIN-IMPACT "Host steps" and measures the HOST gate growth. API-05 and API-06 stay pending.

## Self-Check: PASSED

All eight created files and this SUMMARY exist; the seven commits d5a1469, 953953c, e14942b, 40b7bbb, ddf7834, 3f09aab and 16cd072 are ancestors of HEAD; the working tree was clean apart from this record.
