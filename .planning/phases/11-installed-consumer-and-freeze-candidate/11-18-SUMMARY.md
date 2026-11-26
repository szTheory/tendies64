---
phase: 11-installed-consumer-and-freeze-candidate
plan: 18
subsystem: installed-consumer
tags: [installed-consumer, composite-fixture, three-way, runner-recorded-pins, independent-literals, accepted-and-refused, status-classes, rings, save, state, bus]

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: the relocatable tendies64 package, the moved-prefix consumer driver and smoke case (11-16), the cheap-first ABI and API-04 checks (11-17), the shape wave that gave every entry its documented check order (11-02 to 11-15, 11-26)
provides:
  - the Phase 10 composite fixture run through the installed package: 25 checks against the independent literals of composite.expected.json and the labeled runner-recorded pins of the new composite.runner-pins.json
  - scripts/check-consumer-literals.py: the generated, diffed and self-tested link between the committed fixture JSON and the consumer's constants (installed.consumer.literals)
  - installed.consumer.three-way: the in-tree runner (the two supervised runs of runner.composite-two-run) against the installed consumer on every compared value, one line per value naming its source class
  - 17 operation cases (nine in Task 2, eight in Task 3), each with accepted and refused lines; refusals assert the exact status, its class in the consumer's own class table and an unchanged digest (or still no image) with sentinel outputs untouched
  - a closed case list (tests/installed-consumer/cases.txt, 19 binary cases) enforced by installed.consumer.build
  - 20 new CTest names, every one with a measured COST
affects: [11-19, 11-20, 11-21, 11-23, 11-25]

actuals:
  tokens: 54467
  tasks: 3
  commits: 3

plan_head_before: 0ebb5a4db16242634f3d80a48f10308907abbf8a
plan_head_after: 3ff3a7947e547fcb9ebd72fc5efc051f38adee5c

tech-stack:
  added: []
  patterns:
    - "an expected value has a source class in the file that carries it: independent literals and runner-recorded pins live in two labeled sections of one generated header, and a script, not a person, regenerates and diffs it"
    - "a refusal assertion is four things: the exact status, its class from the consumer's own table, the machine digest (or NOT_READY when there is no image) and a sentinel byte copy of every output"
    - "a consumer case that must reach a state the public API only reaches indirectly builds it from public calls (a PI DMA into the SRAM window for SAVE_BUSY, a trailer recomputed with the consumer's own SHA-256 for identity-binding refusals)"

key-files:
  created:
    - fixtures/composite/composite.runner-pins.json
    - scripts/check-consumer-literals.py
    - tests/installed-consumer/cases.txt
    - tests/installed-consumer/composite.c
    - tests/installed-consumer/ops.c
    - tests/installed-consumer/t64c_alloc.h
    - tests/installed-consumer/t64c_cases.h
    - tests/installed-consumer/t64c_classes.h
    - tests/installed-consumer/t64c_composite.h
    - tests/installed-consumer/t64c_sha256.c
    - tests/installed-consumer/t64c_sha256.h
  modified:
    - tests/installed-consumer/CMakeLists.txt
    - tests/installed-consumer/main.c
    - scripts/check-install.sh
    - tools/abi/CMakeLists.txt
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase11-regexes.txt
    - .planning/phases/11-installed-consumer-and-freeze-candidate/PIN-IMPACT.md

key-decisions:
  - "Runner-recorded pins are a committed, sourced fixture (composite.runner-pins.json) with the exact label 'runner-recorded pin, not independent'; the generator refuses any other label and requires the cited Phase 10 record to contain the tick (599,357) and the digest (B-1, DR-32). The consumer prints them as 'runner-recorded pin' lines, never as independent."
  - "The three-way leg that the runner supplies is the supervised pair of runner.composite-two-run (a raw-capture run and a save-out run), not a re-pointed runner; the consumer's own exit status carries the independent-literal leg in the same run."
  - "The consumer's class table t64c_classes.h was written in this plan (X-macro rows, by hand from docs/abi/status-classes.txt) instead of waiting for 11-20, because every refusal here asserts its class; 11-20 adds the check-abi.py comparison and the generator."
  - "A machine without an image has no digest (t64_machine_digest answers NOT_READY), so a refusal on such an instance asserts that it still answers NOT_READY instead of comparing digests."
  - "Behaviors the header or the contract does not document are not asserted: no case pins the byte-swapped and word-reversed dump orders (refused by the authored profile), the frame-event limit above 0, pre-admission bus and trace behavior, or *profile_out after a refused lookup."

patterns-established:
  - "a plan that adds names to a fixture-bearing family counts the install.setup closure in its preflight row (third time: 5, 15 and 23 for ^installed.consumer.)"
  - "a COST map that refuses to configure a case without a measured cost, so a case cannot enter the gate unmeasured"

requirements-completed: []

coverage:
  - id: D1
    description: "The composite fixture reproduced through the installed package against the independent literals (video, audio, transcript, poll words, save) and the four labeled runner-recorded pins"
    requirement: VER-05
    verification:
      - kind: integration
        ref: "ctest -R '^installed\\.consumer\\.(composite|literals)$' (4 of 4 with the fixture closure); t64c --case composite at 4096, 1024, 256 and 16 ticks per advance: checks=25 failures=0; scratch copy with one video literal byte changed fails (checks=25 failures=1)"
        status: pass
    human_judgment: false
  - id: D2
    description: "Three-way agreement stated honestly: consumer equals the in-tree runner on every compared value; consumer equals the independent literals where one exists; the four runner values are labeled pins"
    requirement: VER-05
    verification:
      - kind: integration
        ref: "sh scripts/check-install.sh three-way (ctest installed.consumer.three-way): digest 97f27027...1267c on the raw run, the save run and the consumer, tick 599357, GUEST_COMPLETION, 3 video packets, 2 audio packets, save and transcript byte for byte"
        status: pass
    human_judgment: false
  - id: D3
    description: "Every API-01 operation has an accepted case and a refusal through the installed library, with the status class and the unchanged digest asserted"
    requirement: API-01
    verification:
      - kind: integration
        ref: "ctest -R '^installed\\.consumer\\.ops-' (17 of 17): every case prints accepted and refused lines; seeded copies with a wrong class and a wrong expected status fail ops-unload (6) and ops-input (2)"
        status: pass
    human_judgment: false
  - id: D4
    description: "Ring semantics: an undrained run stops with RESOURCE_LIMIT at backpressure, a drain lets the next advance progress, no packet is lost"
    requirement: API-01
    verification:
      - kind: integration
        ref: "installed.consumer.ops-rings: stall at tick 540952, ring full (2 queued, 0 free), drain, completion at 599357, 3 video packets equal to the 48-byte literal, 2 audio packets equal to the literals"
        status: pass
    human_judgment: false
  - id: D5
    description: "The closed case list and the gate: inventory, include guard, init guard, preflight and the full gate"
    verification:
      - kind: integration
        ref: "sh scripts/check-test-inventory.sh --preset gate (309), sh scripts/check-consumer-includes.sh (files=11 findings=0), python3 -B scripts/check-api-init.py (files=196 findings=0), sh scripts/preflight-phase11.sh --no-replay PASS (67 s), sh scripts/gate.sh PASS 309 of 309 (ctest 99 s, total 167 s)"
        status: pass
    human_judgment: false

duration: 35min
completed: 2026-11-26
status: complete
---

# Phase 11 Plan 18: INSTALL composite three-way and the accepted and refused case per operation Summary

**The authored composite fixture runs through the installed, relocated package and reproduces the independent literals of composite.expected.json (video, audio, transcript, poll words, save) and the four runner-recorded pins labeled as such, agrees byte for byte with the in-tree runner on every compared value, and every public operation has an accepted and a refused case that asserts the exact status, its class in the consumer's own table and an unchanged digest.**

## Performance

- **Duration:** about 35 min (builds and test runs at host load 5 to 15; no wall backstop was hit and no limit changed)
- **Started:** 2026-11-26T03:07Z
- **Completed:** 2026-11-26T03:42Z
- **Tasks:** 3 of 3
- **Files modified:** 18 in the code and record commits (11 created, 7 modified)

## Accomplishments

- **Tracer (Task 1).** `composite.c` creates a machine through the installed archive, admits the authored ROM, queues the two literal inputs, advances in bounded slices with a drain after every advance and compares 25 values: the three 48-byte video packets, both S16BE audio packets, the 188-byte transcript (bytes and SHA-256), the three polled words, the 32,768-byte save (the 16-byte record at offset 256, zero elsewhere, and SHA-256), and the four pins. The case prints `independent literal: ...` and `runner-recorded pin, not independent: ...` lines; the SHA-256 is the consumer's own (`t64c_sha256.c`, written from FIPS 180-4 and self-checked against three published vectors at start). A RESOURCE_LIMIT stop is resumable, and a stall is declared only when the tick, the drained packets and the machine digest all stand still.
- **Literals (Task 1).** `scripts/check-consumer-literals.py` generates `t64c_composite.h` in two labeled sections from `composite.expected.json`, `composite.input.json` and the new `composite.runner-pins.json`; `--check` diffs the committed header, refuses a pins file whose label is not exact, and requires the cited Phase 10 record to contain `599,357` and the digest; the seeded `--selftest` (7 cases) runs inside `installed.consumer.literals`.
- **Three-way (Task 2).** `scripts/check-install.sh three-way` runs the supervised raw-capture and save-out runs of the in-tree runner and the consumer's composite case (16 ticks per advance like the runner), then prints one line per value with its source class and compares video, audio, save and transcript with `cmp`. Both runner runs and the consumer end at digest `97f270274d456e883ceaa11f100f92c7fbf3150e89031fa51164e79991f1267c`, tick 599357, GUEST_COMPLETION, 3 video and 2 audio packets.
- **Operation cases (Tasks 2 and 3).** Seventeen cases in `ops.c`: lifetime (including the allocator refusing the k-th allocation for every k and REENTRANT from inside an allocator callback), handshake, struct_size (every in/out struct one byte below v1, larger structs with 16 untouched sentinel bytes), profile, admission (the previous image is kept after every refusal), input (QUEUE_FULL at 255 queued, retry after an advance), advance (all three request kinds and the terminal stop), reset (a second run after reset equals the first), unload, rings, text, save (R0 to R9 in the documented order, the three byte orders, SAVE_BUSY from a PI DMA), state (round trip, a second instance, ten hostile blobs with the documented status and report reason), queries, trace, bus (the cartridge window equals the file bytes; a rejected PI request adds exactly one PI_REFUSED record and moves no public observation) and poison-query.
- **Backpressure resolved.** The undrained composite stalls at guest tick 540952 after 260 per-advance-budget stops: the video ring is full, a further advance repeats the stop with an unchanged digest, and one drain lets it run to completion at 599357 with all 3 packets (RESEARCH ASSUMED item closed in PIN-IMPACT).
- **Gate.** `sh scripts/gate.sh` PASS 309 of 309 (ctest 99 s, total 167 s; 289 names, 96 s and 156 s at 11-17).

## Task Commits

1. **Task 1 (tracer): composite fixture through the installed package against independent literals** - `588da28` (feat)
2. **Task 2: three-way agreement and the lifetime to unload operation cases** - `d244b6e` (feat)
3. **Task 3: ring, text, save, state, query, trace, bus and poison-query cases, PIN-IMPACT record** - `3ff3a79` (feat)

## Verification

| Check | Result |
|-------|--------|
| `ctest -R '^installed\.consumer\.(composite\|literals)$'` | 4 of 4 passed (with install.setup and installed.consumer.build); composite checks=25 failures=0 |
| `ctest -R '^installed\.consumer\.(three-way\|ops-(lifetime\|handshake\|struct-size\|profile\|admission\|input\|advance\|reset\|unload))$'` | 12 of 12 passed (10 names plus the two fixture names) |
| `ctest -R '^installed\.consumer\.ops-(rings\|text\|save\|state\|queries\|trace\|bus\|poison-query)$'` | 10 of 10 passed (8 names plus the two fixture names) |
| Every ops case prints accepted and refused lines | lifetime 9/9, handshake 4/9, struct-size 6/26, profile 8/12, admission 5/15, input 7/8, advance 8/11, reset 6/3, unload 4/8, rings 18/18, text 9/9, save 13/22, state 12/27, queries 9/28, trace 11/5, bus 7/14, poison-query 4/3 (accepted/refused lines) |
| Seeded failures, shown once each | one video literal byte changed in a scratch copy: composite fails (checks=25 failures=1); class of NOT_READY changed in the consumer table and one expected status changed: ops-unload 6 failures, ops-input 2 failures; `--selftest` of the literal checker: 7 of 7 reported as seeded |
| Slice independence | composite at 4096 (default), 1024, 256 and 16 ticks per advance: checks=25 failures=0 each, digest 97f270...67c |
| `sh scripts/check-test-inventory.sh --preset gate`, `sh scripts/check-consumer-includes.sh`, `python3 -B scripts/check-api-init.py` | PASS count=309; files=11 findings=0; files=196 findings=0 |
| `ctest -N -R '^installed\.consumer\.'` and COST on every new name | 23 (22 names plus install.setup); every name has COST equal to its measured seconds and TIMEOUT 60 or 120 |
| gcc 13.3 (act image) of the four consumer sources at -O3 with the consumer's flags | clean (no new warning under -Werror) |
| `sh scripts/preflight-phase11.sh --no-replay` | PASS, 67 s at load 6 (ctest-counts, abi-cheap 7 s, guards, timeout-audit 162 TIMEOUTs 0 too low, pii, linux-compile 233 units 55 s) |
| `sh scripts/gate.sh` | PASS, 309 of 309, ctest 99 s, total 167 s at load 5 to 16 |
| `sh scripts/pii-scan.sh --tree` | findings=0 before the Task 3 commit and after |

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Plan count] The `^installed\.consumer\.` counts include install.setup**
- **Found during:** Task 1 (`ctest -N -R`)
- **Issue:** the plan's 4, 14 and 22 (and `Total Tests: 1` for the smoke regex) ignore the `t64_install` fixture: CTest adds `install.setup` through `installed.consumer.build`, as the 11-16 summary already recorded. The real counts are 5, 15 and 23, and `-N -R '^installed\.consumer\.smoke$'` is 3.
- **Fix:** the preflight row is 23 (5 after Task 1, 15 after Task 2); the names and their registration are as planned (`installed.consumer.smoke` registered exactly once, by its explicit 11-16 registration, and the foreach skips that row).
- **Files modified:** `scripts/preflight-phase11-regexes.txt`
- **Commits:** `588da28`, `d244b6e`, `3ff3a79`

**2. [Rule 3 - Blocking] Three files outside the plan's list**
- **Found during:** Tasks 1 and 2
- **Issue:** `-Wmissing-prototypes` under `-Werror` and the refusal assertions of Task 2 need shared declarations and a class table: `t64c_sha256.h`, `t64c_cases.h` (argument record, fixture and run engine shared by composite.c and ops.c) and `t64c_classes.h`.
- **Fix:** the three headers were added. `t64c_classes.h` is the table plan 11-20 lists as its own artifact; it was written by hand from `docs/abi/status-classes.txt` as X-macro rows (`X(NAME, CLASS, RESUME)`) because every refusal of this plan asserts its class. Plan 11-20 therefore adds the `check-abi.py classes --consumer-table` comparison against an existing file instead of writing the table.
- **Files modified:** `tests/installed-consumer/t64c_sha256.h`, `t64c_cases.h`, `t64c_classes.h`
- **Commits:** `588da28`, `d244b6e`

**3. [Rule 1 - Documented behavior] Assertions adapted to what the header and the contract say**
- **Found during:** Tasks 2 and 3 (first runs)
- **Issue:** several planned assertions assumed behavior the library does not have or does not document: `t64_machine_digest` answers NOT_READY before an admission and after an unload, so a no-image refusal cannot compare digests; the frame-event request limit 1 is never reached through the public API (the counter counts synthetic frame events that only the in-tree scheduler tests schedule); the byte-swapped and word-reversed dump orders are refused by the authored profile's admission; bus access and trace enable and drain need no image; `*profile_out` after a refused lookup is not documented.
- **Fix:** a no-image refusal asserts that the machine still answers NOT_READY; the frame-event kind is asserted at its reachable limit 0; none of the other undocumented behaviors is pinned. They are listed as observations in PIN-IMPACT for the review plans.
- **Files modified:** `tests/installed-consumer/ops.c`
- **Commits:** `d244b6e`, `3ff3a79`

**4. [Rule 1 - Bug] The first stall rule counted budget stops**
- **Found during:** Task 1 (first run of the composite case at 4096 ticks)
- **Issue:** the run engine failed with "stalled at tick 268288": it capped the total number of RESOURCE_LIMIT stops at 64, but a 4096-tick slice meets the per-advance event budget (4096 events) 260 times in a normal run; each such stop moves the tick.
- **Fix:** the cap is on consecutive stops, and a stall is declared only when the tick, the drained packets and the machine digest stand still across two stops.
- **Files modified:** `tests/installed-consumer/composite.c`
- **Commit:** `588da28`

**Total deviations:** 4 auto-fixed (two count or scope, one blocking, one bug). **Impact:** none on scope or evidence tier; all inside the plan's file families.

### Not exercised through the installed consumer (stated, not hidden)

- Stop reasons QUIESCENT, CPU_UNSUPPORTED and HALT are not reachable with the authored composite ROM through the public API (HALT is reserved); their class rows are in the consumer's table but no case produces them. BOUND, INSTRUCTION_COUNT, FRAME_EVENT_COUNT (limit 0), RESOURCE_LIMIT and GUEST_COMPLETION are produced and asserted.
- Save check R3 (a profile with no save device) needs a second profile and a ROM it admits; the consumer carries one ROM, so R3 is not asserted. TICK_OVERFLOW, SEQUENCE_EXHAUSTED and BAD_STORAGE are not reachable from the public API with the consumer's allocator and the queue sizes of this profile.
- The ISViewer text drain never produces a partial copy: the documented behavior is BUFFER_TOO_SMALL with nothing copied, which is what the case asserts.

## Authentication Gates

None.

## Issues Encountered

- Host load was 5 to 16 (unrelated VM and Docker work); no run failed on a wall backstop and no limit was widened.
- `ops-state` is the heaviest new name (1.08 to 1.10 s native): the container is 10.5 MB and ten full validations run per case. It is the first name to trim if the orchestrator's measured cold gate-macos run is short of the 900 s limit; the other 19 new names cost 0.02 to 0.6 s each.

## Known Stubs

None. No placeholder, empty-value or mock data flows into any file this plan created.

## Threat Flags

None. `check-install.sh three-way` writes only under the build tree (`BUILD/install-check/three-way`, the same guarded work directory as the other subcommands) and runs the in-tree supervisor with no network; the consumer's `--out DIR` writes test output files only when asked. No ROM, save or firmware is committed or uploaded: the composite ROM is built in the build tree by the existing fixture target.

## Threat Model Disposition

| Threat | Result |
|--------|--------|
| T-11-35 (hostile state and save blobs through the installed API) | mitigated: ops-state refuses ten hostile blobs with the documented status and report reason, ops-save refuses wrong kind, format, size and a busy device in the documented order; every refusal leaves the digest unchanged |
| T-11-36 (expected values derived from the producer) | mitigated: literals are constants generated from the committed JSON and diffed by installed.consumer.literals; the four runner values are labeled pins; the hash is the consumer's own; the three-way compares against the in-tree runner (G6) |
| T-11-37 (protected content in tests) | mitigated: only the authored composite ROM of the build tree is used; nothing is committed or uploaded |
| T-11-SC (package installs) | accepted: none were made |

## Next Phase Readiness

- **Plan 11-19** (host-shaped loop, two instances, negative controls) can reuse `t64c_cases.h` (fixture, run engine, drains), `t64c_alloc.h` and the three-way driver; the `control` subcommand of `check-install.sh` is still its to add.
- **Plan 11-20** finds `t64c_classes.h` already written (X-macro rows); it adds the comparison in `check-abi.py` and the call-sequence generator.
- **Gate growth (orchestrator).** Twenty new names, about 6.6 s of native test time in total (about 28 s more hosted at the 4.3 multiplier): the measured cold gate-macos run, taken before the 11-21 ledger row, must confirm the margin against 900 s. Warm local ctest grew from 96 s to 99 s.
- **Hosted confirmation still owed.** The Linux undefined set of 11-16 and 11-17 remains an act-image measurement; this plan adds the first hosted run of `installed.consumer.three-way` and the 17 cases on gate-linux and gate-macos (the sources compile clean under gcc 13.3 in the act image, but only the next push proves the hosted link and run).
- **Push state.** Nothing was pushed by this plan; `origin/main` is `e5a2e3c`. The local commits after it go out with the next push a plan names.
- **Observations for the review plans** (PIN-IMPACT, 11-18 row): the authored profile refuses the two non-big-endian dump orders that machine-contract.md lists as accepted; the frame-event request limit above 0 is unreachable from the public API.

## Self-Check: PASSED

- FOUND: fixtures/composite/composite.runner-pins.json, scripts/check-consumer-literals.py, tests/installed-consumer/{cases.txt,composite.c,ops.c,t64c_alloc.h,t64c_cases.h,t64c_classes.h,t64c_composite.h,t64c_sha256.c,t64c_sha256.h}
- FOUND commits: 588da28, d244b6e, 3ff3a79 (each an ancestor of HEAD; `git rev-list --count` from the base is 3)
