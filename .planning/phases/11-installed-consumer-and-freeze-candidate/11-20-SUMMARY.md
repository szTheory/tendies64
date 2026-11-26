---
phase: 11-installed-consumer-and-freeze-candidate
plan: 20
subsystem: installed-consumer
tags: [installed-consumer, call-sequence-generator, refusal-is-a-no-op, class-table, asan, ubsan, tsan, poison, shared-test-header]

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: the relocatable tendies64 package and moved-prefix driver (11-16), the consumer's class table and counting allocator (11-18), the host-shaped loop (11-19), the status-class table and the poison and defect machinery (11-02, 11-03, 11-04, 11-26)
provides:
  - a deterministic call-sequence generator through the installed library (constant-seed LCG owned by the consumer) that snapshots every observation before each call and asserts bit-equality after any refused-class status, proves guest-stops resume or are terminal where the class table says, and proves one deliberate defect poisons the instance through the consumer's own allocator storage (no test-only symbol)
  - `check-abi.py classes --consumer-table`: the consumer's hand-written class table is compared with `docs/abi/status-classes.txt` on every gate run (six seeded breakages in `repo.abi-selftest`)
  - local ASan and UBSan lanes against the shipped Release archive and against an instrumented library, and a local two-thread, one-instance-each ThreadSanitizer lane with a seeded race that shows it can fail
  - one shared test header for the duplicated counting allocator, fixture readers and poison helpers of three API unit tests (review item S-09)
  - finding F-20-A: `t64_machine_advance` has no span check on its request and result
affects: [11-21, 11-23, 11-25]

actuals:
  tokens: 35500
  tasks: 4
  commits: 6

plan_head_before: 93b8293f5d2e1c39bd279e1340828fa07120f894
plan_head_after: 949dcbec40250a9ef2ba52da5d9983aaa33542fe

tech-stack:
  added: []
  patterns:
    - "a refusal is checked by two observations of different cost: a cheap snapshot (about ten read-only calls) after every refused call, and the 22 ms machine digest over a burst of refusals, verified before the next call that may change state"
    - "a defect is triggered through the public allocator contract: the consumer owns the instance's storage, finds the stored profile digest there exactly once and flips a bit, so the shipped library's own invariant check answers INTERNAL_DEFECT"
    - "a lane that must be able to fail has a seeded control: --seed-fault 1 to 3 for the generator, --seed-race for ThreadSanitizer, and a check that the sanitizer runtime is linked on the side that must be instrumented"

key-files:
  created:
    - tests/installed-consumer/sequence.c
    - tests/installed-consumer/threads.c
    - core/tests/api_test_support.h
    - .planning/phases/11-installed-consumer-and-freeze-candidate/deferred-items.md
  modified:
    - tests/installed-consumer/main.c
    - tests/installed-consumer/CMakeLists.txt
    - tests/installed-consumer/cases.txt
    - tests/installed-consumer/hostloop.c
    - tests/installed-consumer/t64c_alloc.h
    - tests/installed-consumer/t64c_cases.h
    - scripts/check-abi.py
    - scripts/check-install.sh
    - scripts/check-consumer-includes.sh
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase11-regexes.txt
    - tools/abi/CMakeLists.txt
    - core/tests/test_api_poison.c
    - core/tests/test_api_lifecycle.c
    - core/tests/test_api_struct_size.c
    - .planning/phases/11-installed-consumer-and-freeze-candidate/PIN-IMPACT.md

key-decisions:
  - "The machine digest (22 ms, 4 MiB of RDRAM) is compared over a refusal burst, not after every call: it is taken before the first refused call after a state change and again before the next call that may change state, and a failure names the step range and its calls. The cheap snapshot is compared after every refused call. The gate verifies every second burst (default --digest-stride 2, about 1.1 s); the local sanitizer lane verifies every burst against the shipped library"
  - "The black-box defect trigger (assumption A5) held: the profile digest the instance stores is found exactly once in the consumer's own allocator storage, so no `t64i_test_force_defect` prototype is declared in the consumer and no deviation is needed for it"
  - "Variants that may change the instance are kept one time in six in the gate sequence, so state changes (and machine digests) stay rare; two fixed milestones run the guest to the pinned digest with a forced backpressure stall, and one fixed step triggers the defect"
  - "The two-thread case is built only with T64C_THREADS=ON and is not in cases.txt, so it has no CTest name and cannot run in the hosted gate"

patterns-established:
  - "a classified call is bracketed (begin takes the snapshot, end classifies by the consumer's own class table and compares); raw setup calls that change the instance close the open burst first"
  - "a shared test header is `static inline` only and includes the public header alone, so a test that uses a part of it compiles clean and still sees what an installed consumer sees"

requirements-completed: []

coverage:
  - id: D1
    description: "Every refused status of a deterministic call sequence is a proven no-op through the installed library, guest-stops resume or are terminal exactly where the class table says, and no caller fault poisons"
    requirement: API-02
    verification:
      - kind: integration
        ref: "ctest -R '^installed\\.consumer\\.sequence$': 400 steps, 1140 classified calls, 278 refused, 624 guest-stop results, 1 defect, 5743 checks, 0 failures"
        status: pass
    human_judgment: false
  - id: D2
    description: "A deliberate defect poisons the instance and only destroy and the poison query succeed, with outputs untouched and every host allocation released"
    requirement: API-02
    verification:
      - kind: integration
        ref: "installed.consumer.sequence: the defect at step 350 answers INTERNAL_DEFECT, a sweep of 128 calls answers POISONED or the poison query, destroy leaves no outstanding allocation"
        status: pass
    human_judgment: false
  - id: D3
    description: "The consumer's class table equals the status-class table"
    requirement: API-02
    verification:
      - kind: unit
        ref: "ctest -R '^repo\\.abi-(status-classes|selftest)$': 47 rows compared, consumer-table-controls with six seeds"
        status: pass
    human_judgment: false
  - id: D4
    description: "The generator and the consumer suite are clean under ASan and UBSan against the shipped library and against an instrumented library, and two instances on two threads are race-free under TSan"
    requirement: API-02
    verification:
      - kind: integration
        ref: "sh scripts/check-install.sh sanitize shipped (24 cases, 20,000 steps, 59 s), sanitize instrumented (24 cases, 20,000 steps, 299 s), tsan (both digests 97f270...1267c, 5 s) and tsan --seed-race (reported): local lanes, exit 0"
        status: pass
    human_judgment: false
  - id: D5
    description: "The duplicated allocator, fixture reader and poison helpers of the API unit tests are one shared header"
    verification:
      - kind: unit
        ref: "ctest -R '^unit\\.core\\.api\\.' (13 names): every check count equal before and after"
        status: pass
    human_judgment: false

duration: 52min
completed: 2026-11-26
status: complete
---

# Phase 11 Plan 20: INSTALL refusal-is-a-no-op harness, sanitizer lanes and two-thread TSan run Summary

**A constant-seed call-sequence generator drives the installed library through 16 operations and 143 argument variants and proves that every refused status leaves every observation bit-equal (278 refused calls in the 400-step gate run, 14,480 in the 20,000-step sanitizer run), that guest-stops resume or stay terminal as the class table says, that no caller fault poisons, and that one deliberate defect, triggered by flipping the stored profile digest in the consumer's own allocator storage, poisons the instance (128 poisoned calls answered, outputs untouched, everything released); the same generator and the whole consumer suite are clean under ASan and UBSan against the shipped archive and an instrumented one, and two instances on two threads are race-free under TSan.**

## Performance

- **Duration:** 52min (host load 5 to 19 from unrelated work; no wall backstop was hit and no limit changed)
- **Tasks:** 4 of 4 (the plan's three and the S-09 fold ordered by the orchestrator)
- **Files modified:** 19 (4 created, 15 modified, PIN-IMPACT included)

## Accomplishments

- **The generator (Task 1, tracer).** `sequence.c` owns a 64-bit LCG (seed `0x7e4d1e5c0ffee064`) and draws 16 operations with 143 argument variants: admit (valid, NULL, zero length, one byte short, a refusing allocator, NULL machine), advance by tick, instruction count and frame-event count with small, large, reversed and equal values and NULL or short request and result, queue_input (past, future, bad port, bad axis, fill to QUEUE_FULL), video and audio copy, output drain, 24 observer variants, save export and import, state export and import (truncated, damaged magic, flipped body byte, unknown flag, short report, valid), reset, unload, bus read and write (including the PI and SI device refusals), trace, the poison query, create with descriptor mutations (S-07: event queue capacity 0, sequence width 0, no RDRAM, short structs, another ABI minor, no allocator and more) and a full drain. Before every classified call it observes the content digest, output status, the oldest video and audio packet info, save info, CPU stop detail, ROM size and header, the poison query and the allocator's live count and bad releases; after a call the consumer's own table (`t64c_classes.h`) calls refused it compares them bit for bit. The machine digest (22 ms) is compared over a refusal burst. Guest-stop rules: every stop reason is a guest-stop row, time never goes backwards, BOUND is exactly at the bound, a larger bound moves or stalls with a full output ring and resumes after a full drain, a terminal stop stays the same stop. The two milestones replace the image, stop at an instruction count and a frame-event count, run to completion with no drain until the ring stalls the guest, drain, resume and end at tick 599357 with digest `97f270274d456e883ceaa11f100f92c7fbf3150e89031fa51164e79991f1267c`. The defect step finds the profile digest exactly once in slot 0 of the consumer's allocations (assumption A5 held), flips a bit, and `t64_machine_admit_rom` answers INTERNAL_DEFECT.
- **Class table equality (Task 1).** `check-abi.py classes --consumer-table` compares the 47 hand-written rows of `t64c_classes.h` with `docs/abi/status-classes.txt` for name, class and resume; `repo.abi-status-classes` runs it and `repo.abi-selftest` has a `consumer-table-controls` case with six seeded breakages.
- **ASan and UBSan lanes (Task 2).** `check-install.sh sanitize shipped|instrumented` builds the consumer with `-fsanitize=address,undefined -fno-sanitize-recover=all` against a fresh install (the Release archive of the gate, or the `asan` preset's Debug archive) and runs all 24 cases with the sequence at 20,000 steps. It checks which side is instrumented by the sanitizer runtime symbols (archive 0 and consumer 34 for the shipped lane, archive 733 for the instrumented lane). Both lanes passed with no sanitizer report: shipped 59 s, instrumented 299 s.
- **TSan lane (Task 3).** `threads.c` (built only with `T64C_THREADS=ON`) runs the stalled-host loop of the hostloop case on two instances in two threads at once; `check-install.sh tsan` builds `t64_core` and the consumer with `-fsanitize=thread`: no report, both digests `97f270...1267c`, 5 s. `tsan --seed-race` makes both threads advance one instance and passes only when ThreadSanitizer reports it (9 reports).
- **Shared test header (Task 4, S-09).** `core/tests/api_test_support.h` replaces three allocator copies, two fixture readers and two sets of eight poison helpers; the reentrancy probe of the poison test rides on the allocator's hook. All 13 `unit.core.api.*` names keep their check counts.

## Task Commits

1. **Task 1 (tracer): the call-sequence generator, the consumer-table check and its registration** - `25e48db` (feat)
2. **Task 1 follow-up: the milestone covers the instruction-count and frame-event stops** - `530f568` (feat)
3. **Task 2: ASan and UBSan lanes** - `fe742c6` (feat)
4. **Task 3: two-thread TSan lane** - `4f2f74a` (feat)
5. **Task 4: shared API test header (S-09)** - `b7cb15d` (test)
6. **Final figures in PIN-IMPACT, the deferred-items file and this summary** - the docs commit that holds this file

## Verification

| Check | Result |
|-------|--------|
| `ctest -R '^(installed\.consumer\.sequence\|repo\.abi-status-classes)$'` | passed (with `install.setup` and `installed.consumer.build`); `installed.consumer.sequence` 1.0 to 1.2 s at host load 7 to 11 (`COST 1.10`) |
| Sequence output (400 steps) | 1140 calls: ok 60, refused 278 (at least 100), guest-stop 624, defect 1; 27 refusal bursts, 13 machine digests verified; stop reasons BOUND 8, INSTRUCTION_COUNT 4, FRAME_EVENT_COUNT 2, RESOURCE_LIMIT 606, GUEST_COMPLETION 4; 5743 checks, 0 failures; identical on repeated runs |
| Seeded faults (`--seed-fault 1 2 3`, stride 1) | each fails with its own text: a refused call that moved the machine digest, one that changed save info, a poison before the defect step; fault 0 passes |
| `sh scripts/check-install.sh sanitize shipped` | exit 0, 24 of 24 cases, sequence 20,000 steps stride 1: 21,481 calls, 14,480 refused, 99,281 checks, 0 failures, 59 s |
| `sh scripts/check-install.sh sanitize instrumented` | exit 0, 24 of 24 cases, sequence 20,000 steps stride 8: 98,522 checks, 0 failures, 299 s |
| `sh scripts/check-install.sh tsan` and `tsan --seed-race` | exit 0 and exit 0: no report with one instance per thread, both digests `97f270...1267c`; the seeded race is reported (9 reports) |
| `ctest -N -R '^installed\.consumer\.threads'` and `'^installed\.consumer\.'` | 0 and 28 (27 plus sequence, `install.setup` and `installed.consumer.build` included) |
| Class table | `check-abi.py classes ... --consumer-table`: constants=47 rows=47 consumer_rows=47 findings=0; `--selftest`: 15 cases, 0 failed |
| S-09 | `ctest -R '^unit\.core\.api\.'`: 13 of 13, every `checks=` count equal before and after |
| Guards | `check-api-init.py` files=203 findings=0; `check-consumer-includes.sh` files=17 findings=0 and its selftest; `check-test-inventory.sh --preset gate` count=324; `sh scripts/pii-scan.sh --tree` findings=0 before every commit |
| gcc 13.3 (act image) | `sequence.c` clean with the consumer's strict flags at `-std=c17 -O3` and as plain `-std=c99` |
| `installed.controls.plain-cc`, `installed.controls.symbol-collision` | passed with `sequence.c` in the consumer (the plain `cc -std=c99` line compiles it) |
| `sh scripts/gate.sh` | PASS, 324 of 324 names (was 323), ctest 91 s, total 157 s, started at host load 8.8 after waiting for the load to fall below 9; `installed.consumer.sequence` 1.24 s inside the parallel run |
| `sh scripts/preflight-phase11.sh --no-replay` | PASS, 47 s (ctest-counts with `^installed\.consumer\.` = 28, abi-cheap 4 s, guards 2 s, head-ci, timeout-audit, pii 0 findings over the tree and 12 commits, linux-compile 233 units with the act image cc 13.3.0 in 40 s) |

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Plan count] The `installed.consumer.` count is 28, not 27**
- **Found during:** Task 1 (`ctest -N`)
- **Issue:** the plan's 27 (26 plus sequence) is stale: 11-19 already brought the count to 27 with the `install.setup` fixture closure.
- **Fix:** the preflight row is 28 (27 plus sequence).
- **Files modified:** `scripts/preflight-phase11-regexes.txt`
- **Commit:** `25e48db`

**2. [Rule 3 - Blocking] The machine digest costs 22 ms, so it is compared over a refusal burst**
- **Found during:** Task 1 (measuring the digest and an admitted instance)
- **Issue:** the plan snapshots the machine digest before and after every call; at 22 ms a 400-step run would take about 10 s native, far above the cost the gate margin allows.
- **Fix:** the cheap snapshot is compared after every refused call; the machine digest is compared over a burst of refusals (before the first one after a state change, again before the next call that may change state), state-changing variants are drawn one time in six, and the gate verifies every second burst (`--digest-stride 2`, 1.1 s native). The local shipped sanitizer lane verifies every burst (867). A refusal that changed the machine fails its burst and names the step range and calls (shown with `--seed-fault 1`).
- **Files modified:** `tests/installed-consumer/sequence.c`
- **Commit:** `25e48db`

**3. [Rule 3 - Blocking] Files outside the plan's list**
- **Issue:** the plan's file list omits files that the work needs: `tests/installed-consumer/t64c_alloc.h` (the ledger records each slot's size so the consumer can scan the instance's storage), `t64c_cases.h` (options and prototypes), `hostloop.c` (`t64c_hostloop_once`, the silent one-run helper of the thread case), `scripts/check-consumer-includes.sh` (`<pthread.h>` joins the fixed list of system headers a consumer may include), and, for S-09, `core/tests/api_test_support.h` and the three API unit tests.
- **Commits:** `25e48db`, `4f2f74a`, `b7cb15d`

**4. [Rule 2 - Missing critical functionality] The lanes must be shown able to fail**
- **Issue:** a harness whose checks never fire proves nothing, and a sanitizer lane could silently run with an uninstrumented library.
- **Fix:** `--seed-fault 1..3` on the generator (a refused call that moves the digest, one that changes an observation, a caller-side poison), a check that the sanitizer runtime is linked on the side that must be instrumented (archive and consumer), and `tsan --seed-race`. The faults and the race are one-time evidence in PIN-IMPACT; they have no CTest name.
- **Files modified:** `tests/installed-consumer/sequence.c`, `tests/installed-consumer/threads.c`, `scripts/check-install.sh`
- **Commits:** `25e48db`, `fe742c6`, `4f2f74a`

**5. [Rule 1 - Plan text] Small wording corrections**
- The plan's scratch prefixes are `mktemp` directories; the lanes use directories below the ignored `build/gate/sanitize-lane/` (a prefix with a space, no path outside the repository), because the CTest fixture `install.setup` clears the install-check directory.
- The plan's "400 in the gate" holds for the number of steps; the machine digest is verified on every second burst there (deviation 2).
- The generator's two milestones and the stop-reason checks of the milestone are additions to the plan's grammar (they make the terminal, stall and resume rules and the pinned digest part of the gate run).
- The plan asks the generator to add an aliasing-span row for advance only as an option (S-06); it is the finding below.

**Total deviations:** 5 (one count, two blocking, one missing functionality, one wording). **Impact:** none on scope or evidence tier; the gate run verifies half of the refusal bursts with the machine digest, the local lane all of them.

### Finding outside the plan

**F-20-A: `t64_machine_advance` has no span check on its request and result.** A probe found that every out-pointer entry refuses a pointer into the instance's own allocation with `INVALID_ARGUMENT` except `t64_machine_advance` (request and result), which answers OK and writes the result into the instance, and `t64_machine_bus_read` (`value_out`, no header promise). The header promises the refusal for advance. Not fixed here (pre-existing, outside this plan's files, and a failing row cannot enter the gate); logged in `deferred-items.md` and PIN-IMPACT with the suggested plan. API-02 is therefore not marked complete.

## Authentication Gates

None.

## Issues Encountered

- Unrelated host load (5 to 19) from another project on the machine; the lanes and the gate ran without widening any limit, and the recorded seconds are the ones measured under that load.
- The first 20,000-step instrumented run ended with exit status 2 because `check-install.sh` was edited while it ran (the shell read the changed tail after the lane had finished all its work, 289 s, clean); it was run again unedited.
- `check-install.sh --work` needs an absolute directory (a relative one made the consumer configure miss the package).

## Known Stubs

None. No placeholder, empty-value or mock data flows into any file this plan created.

## Threat Flags

None. The consumer deliberately damages one bit in storage its own allocator gave the library (the defect step), in the test only; `check-install.sh sanitize` and `tsan` write only below the ignored build tree and run no network access; no sanitizer or TSan run is in the hosted gate or `ci.yml`.

## Threat Model Disposition

| Threat | Result |
|--------|--------|
| T-11-42 (refused calls with side effects) | mitigated: the cheap snapshot is bit-equal after every refused call (278 in the gate run, 14,480 in the sanitizer run) and the machine digest after every verified burst (13 in the gate run, 867 in the local shipped run); `--seed-fault` shows the checks fail |
| T-11-43 (memory errors reachable by a host) | mitigated: ASan and UBSan with `-fno-sanitize-recover=all` against the shipped archive and an instrumented one, 24 cases and 20,000 steps each, no report |
| T-11-44 (data races between instances) | mitigated: two threads, one instance each, library and consumer instrumented, no report; the seeded race is reported, so the lane can fail |
| T-11-SC (package installs) | accepted: none were made |

## Next Phase Readiness

- **11-21 (push, hosted run).** Unpushed since `c6f28f6` are the 11-19 and 11-20 commits. The first hosted run gives the first Linux ELF run of `installed.consumer.sequence` (gcc 13.3; the sequence compiled clean there in the act image), the controls, the Linux undefined-set confirmation and the cold gate-macos measurement. Gate growth of this plan: one new name, `installed.consumer.sequence` at 1.0 to 1.2 s native (`COST 1.10`, about 5 s at the hosted multiplier 4.3); no sanitizer or TSan name.
- **Counts.** `ctest -N -R '^installed\.consumer\.'` is 28 with the fixture closure; the gate has 324 names (323 plus one).
- **Open for the orchestrator.** Decide F-20-A (a new numbered defect-fix plan for the advance span check, in the manner of 11-26). API-02 is delivered locally and not marked complete until that is decided and the hosted run is green.
- **11-23.** The class table equality check is in the gate; the layout table still owns S-05 (per-target offsets of the padded structs). The poison helpers have one place to extend when a public struct gains a member; `poison_import_report` (`state_import_helpers.h`), `poison_save_info` (save tests) and `poison_drain_result` (`test_isviewer.c`) are still separate copies.

## Self-Check: PASSED

- FOUND: `tests/installed-consumer/sequence.c`, `tests/installed-consumer/threads.c`, `core/tests/api_test_support.h`, `.planning/phases/11-installed-consumer-and-freeze-candidate/deferred-items.md`
- FOUND commits: `25e48db`, `530f568`, `fe742c6`, `4f2f74a`, `b7cb15d`, `949dcbe` (each an ancestor of HEAD; `git rev-list --count` from the base is 6)
- Acceptance criteria re-run: the sequence output reports 278 refused (at least 100) and 1 defect for 400 steps and PIN-IMPACT records that A5 held; PIN-IMPACT "Install steps" has both sanitizer rows with seconds and steps and the TSan record with both digests `97f270...1267c`; `ctest -N -R '^installed\.consumer\.threads'` reports 0 and a case list that names `threads` is refused by `check-install.sh build`
