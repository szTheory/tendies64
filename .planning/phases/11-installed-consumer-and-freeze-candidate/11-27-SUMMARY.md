---
phase: 11-installed-consumer-and-freeze-candidate
plan: 27
subsystem: api
tags: [defect-fix, span-check, advance, installed-consumer, call-sequence-generator, public-header, bus-read]

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: finding F-20-A from the aliasing probe of plan 11-20, the struct_size prologues of 11-13 (review candidate g), the shared test header of 11-20 and the defect-fix pattern of 11-26
provides:
  - t64_machine_advance judges the spans of its request and result (machine storage, admitted image, packet storages) before either struct_size and before the admission answer; INVALID_ARGUMENT with nothing read or written
  - in-tree advance span rows inside unit.core.api.not-ready-table (RED run recorded, GREEN) and the same five calls per live block in the installed consumer's ops-advance
  - the generator's 15th advance variant (aliasing) with the burst bracketed around the deliberate placement write
  - an explicit header statement that the bus read value_out span is the caller's responsibility, with the reason and the way to reverse it recorded
  - the generator's drain-all fixed for NOT_READY after an unload (latent defect of 11-20)
affects: [11-21, 11-23]

actuals:
  tokens: 11828
  tasks: 3
  commits: 6

plan_head_before: c46860aa473c27c87985750c1c4a472f5444a3c4
plan_head_after: 50b277f21af5dd941cce8be4ffd9bb7e7b1b7e3c

tech-stack:
  added: []
  patterns:
    - "an aliasing test writes its stop objects into a live block, takes the digest after that deliberate write, and writes the saved bytes back last"
    - "a hand-built white-box machine that calls a public entry records its own storage blocks"

key-files:
  created: []
  modified:
    - core/t64_machine.c
    - core/include/tendies64/t64_machine.h
    - core/tests/test_api_lifecycle.c
    - core/tests/test_cpu_time.c
    - core/tests/test_sched_matrix.c
    - tests/installed-consumer/ops.c
    - tests/installed-consumer/sequence.c
    - tools/abi/CMakeLists.txt
    - docs/abi/status-classes.txt
    - .planning/phases/11-installed-consumer-and-freeze-candidate/PIN-IMPACT.md
    - .planning/phases/11-installed-consumer-and-freeze-candidate/deferred-items.md

key-decisions:
  - "The bus read value_out is NOT span-checked: the header says its span is the caller's responsibility. t64_machine_bus_read is the entry the RSP uses for every guest data load (t64_rsp_issue_event, a sealed body), so a wrapper check adds four range comparisons to a guest-driven hot path, and the alternative edits core/t64_rsp.c, outside this plan's list. Reversible in a plan that names core/t64_rsp.c"
  - "The advance spans are the known v1 sizes (sizeof of the types): the caller's struct_size is not known when the spans are judged, as for every other entry"
  - "The aliasing rows write real stop objects into the instance's own blocks and take the digest after that placement (the placement is the test's deliberate write), then restore the saved bytes; no sentinel-free placement is possible for a request with a valid struct_size"

patterns-established:
  - "a public entry that gains a span check breaks any white-box test that builds the machine by hand: grep for hand-built machines (has_image set on a memset object) before the change and run the full gate, not only the api family"

requirements-completed: []

coverage:
  - id: D1
    description: "F-20-A and candidate (g): advance refuses a request or result inside the instance with INVALID_ARGUMENT before struct_size and admission, writes nothing, leaves the machine unchanged; valid input unchanged"
    requirement: API-02
    verification:
      - kind: unit
        ref: "unit.core.api.not-ready-table checks=2475 failures=0 (RED run build/gate/11-27-task1/red.txt: failures=61); ctest -R '^(unit\\.core\\.api\\.|repo\\.abi)': 18 of 18"
        status: pass
      - kind: other
        ref: "python3 -B scripts/digest-sweep.py --base f9124ae9fce134f31b645c6f2911a6f27fa41b9a --head HEAD: 1599 shared lines, 0 differences"
        status: pass
    human_judgment: false
  - id: D2
    description: "The installed consumer and the generator hold the same rule through the installed package, and can fail"
    requirement: API-01
    verification:
      - kind: integration
        ref: "installed.consumer.ops-advance (68 checks) and installed.consumer.sequence (400 steps, 5469 checks); sanitize shipped lane 20,000 steps 98,461 checks; against the pre-fix library ops-advance fails 35 of 68 and sequence fails its two new checks"
        status: pass
    human_judgment: false
  - id: D3
    description: "Bus value pointer decision stated in the header and recorded"
    requirement: API-01
    verification:
      - kind: other
        ref: "python3 -B scripts/check-abi.py header --include core/include (c99 ok, c++17 ok, findings=0); the sentence is grep-able in the header and in PIN-IMPACT"
        status: pass
    human_judgment: false
  - id: D4
    description: "Full gate green at the final code head"
    verification:
      - kind: integration
        ref: "sh scripts/gate.sh at fa42f29: [gate] PASS, 324 of 324, ctest 98 s, total 165 s"
        status: pass
    human_judgment: false

duration: 38min
completed: 2026-11-26
status: complete
---

# Phase 11 Plan 27: advance span checks (finding F-20-A) Summary

**`t64_machine_advance` now judges the spans of its request and result before either `struct_size` and before the admission answer (a span inside the machine storage, the admitted image or a packet storage is `INVALID_ARGUMENT`, nothing read or written), the same rule is held by an in-tree case, the installed consumer and the call-sequence generator (each shown to fail against the pre-fix library), the bus read `value_out` is documented as the caller's responsibility, and the full gate is 324 of 324 with 0 digest differences.**

## Performance

- **Duration:** about 38 min (builds and one gate run under host load 5 to 10; no limit was widened)
- **Started:** 2026-11-26T05:12Z
- **Completed:** 2026-11-26T05:50Z
- **Tasks:** 3 of 3
- **Files modified:** 11 (8 source, test or build files, 1 class-table line, 2 records)

## Accomplishments

- **The fix.** `core/t64_machine.c`: after the entry guard and the NULL checks, `t64_machine_advance` applies `t64_internal_make_range` and `t64_internal_observation_range_is_disjoint` (the helper the observer entries use) to the request and then the result; only then are `struct_size` values read, then the admission answer. The spans are the v1 sizes. A valid call takes the same path as before (sweep 0 differences). Review candidate (g) of 11-13 is closed by the same change.
- **In-tree rows (RED then GREEN).** Inside the existing `unit.core.api.not-ready-table` (no new CTest name): for each live block of the counting allocator, four stop objects are written 16 bytes apart in the block, the original bytes saved and restored, and five refused calls must answer `INVALID_ARGUMENT` with a result outside the instance untouched, the placed bytes unchanged, the digest taken after the placement unchanged and the instance not poisoned; one control with both spans outside still answers `STRUCT_TOO_SMALL`. Three instances (legacy profile with and without an image, VI and AI profile). RED `checks=2475 failures=61`, GREEN `checks=2475 failures=0` (was 2324).
- **Installed consumer.** `ops-advance` runs the same five calls over each live block of the consumer's own ledger (4 blocks), the digest once per block, plus the control: 68 checks, 0 failures. The generator's `op_advance` gains a 15th variant (`seq_advance_aliased`): one of five shapes at a 16-byte aligned place in the upper half of a random live block (payload only: `sizeof(t64_machine)` is 79,616 bytes against about 4.2 MiB), the refusal burst closed before and after the deliberate write. Against a scratch library built from the pre-fix `t64_machine.c`, `ops-advance` fails 35 of 68 checks and the sequence fails both new checks.
- **Bus value pointer.** Decision: not span-checked; header sentence "The value_out span of t64_machine_bus_read is the caller's responsibility" (NULL stays `INVALID_ARGUMENT`; `bus_write` has no pointer). Reason and the way to reverse it are in PIN-IMPACT.
- **Class table.** Row 1 needs no change; the `STRUCT_TOO_SMALL` row now says advance judges the spans first.
- **Records.** PIN-IMPACT "Install steps" 11-27 (fix, rows, RED and GREEN, pre-fix run, new generator figures, lane, decision, deviations, gate, sweep); `deferred-items.md` marks F-20-A resolved with the commit.

## Task Commits

1. **Task 1: advance span checks and the in-tree rows (RED run recorded, GREEN)** - `d3d3570` (fix)
2. **Task 1 follow-up: INIT forms for the six new test objects (repo.api-init-guard)** - `17d4e20` (test)
3. **Task 2: consumer ops-advance and generator rows, bus value_out header text, class-table wording, two measured COST values** - `6e6bf36` (feat)
4. **Gate follow-up: hand-built white-box machines in test_cpu_time and test_sched_matrix record their storage** - `18c5219` (test)
5. **Gate follow-up: generator full drain stops at NOT_READY after an unload** - `fa42f29` (fix)
6. **Task 3: PIN-IMPACT row, deferred-items, gate and sweep figures** - `50b277f` (docs)

## Verification

| Check | Result |
|-------|--------|
| `ctest --preset gate -R '^(unit\.core\.api\.\|repo\.abi)'` | 18 of 18 passed (after the change) |
| `ctest --preset gate -R '^(installed\.consumer\.ops-advance\|installed\.consumer\.sequence\|unit\.core\.api\.\|repo\.abi)'` | 22 of 22 passed |
| `ctest -N` counts | `^unit\.core\.api\.` 13 and `^installed\.consumer\.` 28, both unchanged; whole preset 324 |
| `python3 -B scripts/check-rsp-no-hle.py --build build/gate` | passes, no sealed body touched, no digest refresh commit |
| `python3 -B scripts/gen-prefix-header.py --check build/gate/core/libt64_core.a` | `ok (35 public functions, 220 renames)` |
| `python3 -B scripts/check-abi.py header --include core/include` | c99 ok, c++17 ok, findings=0; class table checked by `repo.abi-status-classes` (passes) |
| `python3 -B scripts/digest-sweep.py --base f9124ae9fce134f31b645c6f2911a6f27fa41b9a --head HEAD` (also `--head WORKTREE` at the Task 1 and Task 2 states) | `1599 shared lines, 0 differences, 0 head-only lines` |
| `sh scripts/gate.sh` (output under `build/gate/`, load 5 to 8 at start) | first run at the Task 2 head: 322 of 324, the two failures below (fixed); at `18c5219`: PASS 151 s; at `fa42f29`: `[gate] PASS`, 324 of 324, ctest 98 s, total 165 s |
| `sh scripts/check-install.sh sanitize shipped` | first run failed on the latent generator defect below (fixed); second run: 24 cases passed, 20,000-step sequence 98,461 checks 0 failures, no sanitizer report, 73 s |
| Generator, 20,000 steps, stride 1 | 21,186 calls (2,478 ok, 14,518 refused, 1,561 guest-stop, 1 defect, 2,628 on the poisoned instance), 1,275 bursts, 1,285 digests verified, 98,461 checks, 0 failures; the three `--seed-fault` controls still fail as before and fault 0 passes |
| `sh scripts/preflight-phase11.sh --no-replay` | PASS 56 s at `50b277f` (49 s at the Task 2 head) |
| `sh scripts/pii-scan.sh --tree` and `--range origin/main..HEAD` | `findings=0` (1420 files; 20 commits) |

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Two white-box tests built a machine by hand and broke**
- **Found during:** Task 3 (first full gate run; the Task 1 verify names did not include them)
- **Issue:** `unit.core.cpu-time` (736 failed checks) and `unit.core.sched.matrix` (44) memset a `t64_machine`, set `has_image`, and call `t64_machine_advance`. With no recorded machine or image storage the span helper has no range to judge against, so advance refused the call. A machine from `t64_machine_create` always records both.
- **Fix:** each hand-built machine records itself as its own storage block plus a one-byte stand-in image block (`record_storage`); no production code changed.
- **Files modified:** `core/tests/test_cpu_time.c`, `core/tests/test_sched_matrix.c` (outside the plan's list)
- **Commit:** `18c5219`

**2. [Rule 1 - Bug] New test objects had no INIT initializer (my own change)**
- **Found during:** Task 2 (family run of `repo.api-init-guard`)
- **Issue:** the six stop objects of the in-tree rows were declared bare; `scripts/check-api-init.py` refuses that. The Task 1 head `d3d3570` therefore fails `repo.api-init-guard` (the Task 1 verify list did not include it); no history rewrite was made.
- **Fix:** forward fix with the `T64_STOP_REQUEST_INIT` and `T64_STOP_RESULT_INIT` forms.
- **Commit:** `17d4e20`

**3. [Rule 1 - Bug] Latent generator defect: the full drain failed on NOT_READY after an unload**
- **Found during:** Task 3 (`sanitize shipped`, step 14814 of 20,000)
- **Issue:** `seq_drain_all` stops its video and audio loops at `NOT_READY` but its output-drain loop treated it as a failure, so a drain-all step drawn while the instance was unloaded failed the run. The 11-20 stream never drew one; the 15-variant stream does (the 400-step gate run still never does).
- **Fix:** the output-drain loop stops at `NOT_READY` too.
- **Files modified:** `tests/installed-consumer/sequence.c`
- **Commit:** `fa42f29`

**4. [Rule 2 - Records] Measured COST of two existing names (outside the plan's list)**
- **Issue:** `installed.consumer.ops-advance` measures 0.61 to 0.65 s (was 0.37) and `installed.consumer.sequence` 1.15 to 1.23 s (was 1.10) after the new rows; the project keeps COST equal to the measured native seconds.
- **Fix:** `T64_CONSUMER_COST_ops-advance` 0.65 and `T64_CONSUMER_COST_sequence` 1.20 in `tools/abi/CMakeLists.txt`; no name added, TIMEOUT unchanged.
- **Commit:** `6e6bf36`

**5. [Plan verify] `check-abi.py classes` needs its arguments**
- The plan's Task 2 verify runs `python3 -B scripts/check-abi.py classes` bare, which exits 2 (usage). The same check runs with its arguments inside `repo.abi-status-classes`, which passed in every run above.

**6. [Method] RED and GREEN in one commit; consumer RED by a scratch library**
- The project convention is one commit per task with test and code, so the RED run is a recorded run (`build/gate/11-27-task1/red.txt`). The consumer rows cannot run against the old library through the gate, so the pre-fix `t64_machine.c` was compiled with the library's own flags into a scratch archive and the consumer relinked (under `build/gate/11-27-red/`, ignored).

**7. [Method] The decision record sits in Task 3's commit, and the generator stream changed**
- Task 2's action asks for the bus decision "in PIN-IMPACT" while the file belongs to Task 3's list; the record is in the Task 3 commit. The 15th variant changes the deterministic random stream, so the 11-20 figures (1,140 and 21,481 calls) are superseded by the numbers above and in PIN-IMPACT; nothing pins them.

**Total deviations:** 4 auto-fixed (1 Rule 3, 2 Rule 1, 1 Rule 2 records) and 3 notes. **Impact:** no digest, state byte, status value, public signature or layout moved; the header gained comment lines only.

## Issues Encountered

- Host load stayed 5 to 10 for the gate runs and spiked to 24 after the last one (unrelated processes); no limit was widened.
- Not done: the `instrumented` and `tsan` lanes were not re-run (the plan names `sanitize shipped` once). The generator, which both lanes also run, passed at 20,000 steps in the shipped lane.

## Known Stubs

None.

## Threat Flags

None: the plan removes an unchecked read and write path (T-11-66, T-11-67 mitigated) and adds no network, auth or file-access surface. The one accepted gap is the bus read `value_out` span, now stated in the header.

## Next Phase Readiness

- **11-21 must verify this plan** (as the orchestrator ordered): the span order in `t64_machine_advance`, the `not-ready-table` rows, the consumer and generator rows, the header sentence for `t64_machine_bus_read`, and decide whether the bus read should get the uniform rule (a span check in the wrapper together with the RSP moving to `t64_bus_read`, a plan that names `core/t64_rsp.c` and needs a digests-only refresh commit of `check-rsp-no-hle.py`).
- 11-23 binds the header text now: the bus statement is in it. S-05 stays with 11-23.
- Plan 11-27 ran out of wave order (after 11-20, before 11-21): STATE keeps 11-21 as the next plan.
- Nothing was pushed. Unpushed local commits since `c6f28f6`: the 11-19 and 11-20 commits, this plan's six commits, this SUMMARY and the STATE and ROADMAP updates.

## Self-Check: PASSED

- FOUND: core/t64_machine.c, core/include/tendies64/t64_machine.h, core/tests/test_api_lifecycle.c, core/tests/test_cpu_time.c, core/tests/test_sched_matrix.c, tests/installed-consumer/ops.c, tests/installed-consumer/sequence.c, tools/abi/CMakeLists.txt, docs/abi/status-classes.txt, PIN-IMPACT.md (row 11-27), deferred-items.md, build/gate/11-27-task1/red.txt
- FOUND: commits d3d3570, 17d4e20, 6e6bf36, 18c5219, fa42f29, 50b277f (ancestors of HEAD)
- Measured: `git rev-list --count c46860a..HEAD` = 6 at the SUMMARY write
