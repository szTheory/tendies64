---
phase: 11-installed-consumer-and-freeze-candidate
plan: 26
subsystem: api
tags: [defect-fix, shape-review, poison, status-classes, public-header, output-drain, output-status, state-container]

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: the SHAPE review of plan 11-15 (findings S-01 to S-03) and the poison, defect-class and status-class machinery of 11-03, 11-04 and 11-12
provides:
  - t64_machine_output_drain and t64_machine_output_status answer the two unreachable internal inconsistencies (drain cursor and capture length, ring head and count) as INTERNAL_DEFECT with the instance poisoned and every output untouched
  - white-box cases for both sites beside unit.core.api.defect-sites (RED run recorded, GREEN)
  - class-table notes on the RESOURCE_EXHAUSTED and INTERNAL_DEFECT rows naming the two defect sites
  - corrected public-header text (T64_API wording, destroy from an allocator callback, v1-sized profile descriptor) and the corrected state-import check 1 line, before the header hash is bound at 11-23
affects: [11-16, 11-20, 11-23]

actuals:
  tokens: 4843
  tasks: 3
  commits: 5

plan_head_before: c29a9738f8eeccdbec039641ba63609ee96c87ea
plan_head_after: 44ab26d6b0e8f3e9f50078fd45f6707aa87c20f0

tech-stack:
  added: []
  patterns:
    - "an internal invariant that state import already rejects is a defect site at the call that finds it (poison, INTERNAL_DEFECT, outputs untouched), never a refused-class answer or a quiet zero"
    - "a defect-site case runs a healthy control call first, so the branch under test is the one reached"

key-files:
  created: []
  modified:
    - core/t64_observe.c
    - core/tests/test_api_poison.c
    - docs/abi/status-classes.txt
    - core/include/tendies64/t64_machine.h
    - docs/spec/state-container.md
    - .planning/phases/11-installed-consumer-and-freeze-candidate/PIN-IMPACT.md
    - .planning/phases/11-installed-consumer-and-freeze-candidate/11-26-PLAN.md

key-decisions:
  - "The optional C_VISIBILITY_PRESET hidden of S-01 is not taken: the plan is header text only, so the header now says T64_API marks the public functions and promises no visibility attribute, with the t64i_ prefix and the archive symbol check as the enforced property"
  - "The ring_free helper of t64_machine_output_status is removed: with a bad count now a defect, its floor at 0 was dead code and its comment would have been false"
  - "A status text-pending value stays 0 when the boot is not prepared or the capture policy is not the ISViewer policy: that is a legal state, only the cursor and capture-length range checks are defect sites"

patterns-established:
  - "a new test local of a public struct type starts from its INIT form (repo.api-init-guard), including locals used only as expected-value copies"

requirements-completed: []

coverage:
  - id: D1
    description: "S-02: t64_machine_output_drain and t64_machine_output_status poison and answer INTERNAL_DEFECT for the two unreachable inconsistency groups, outputs untouched, valid states unchanged"
    requirement: API-02
    verification:
      - kind: unit
        ref: "ctest --preset gate -R '^(unit\\.core\\.api\\.|repo\\.abi)': 17 of 17; RED run build/gate/11-26-task1/red.txt (36 failed checks before the change)"
        status: pass
      - kind: other
        ref: "python3 -B scripts/digest-sweep.py --base f9124ae9fce134f31b645c6f2911a6f27fa41b9a --head HEAD: 1599 shared lines, 0 differences"
        status: pass
    human_judgment: false
  - id: D2
    description: "S-01 and S-03: header comment text and the state-import check 1 line are true; header diff is comment lines only"
    requirement: API-01
    verification:
      - kind: other
        ref: "python3 -B scripts/check-abi.py header --include core/include (c99, c++17, findings=0); sh scripts/check-docs.sh; sh scripts/check-stale-freeze-text.sh"
        status: pass
    human_judgment: false
  - id: D3
    description: "Full gate green at the committed head"
    verification:
      - kind: integration
        ref: "sh scripts/gate.sh: stopped at ctest, 274 of 275; the one failure gov.pii.repo is a history hit in commit df72747 (owner stop)"
        status: fail
    human_judgment: true
    rationale: "The failing name reports two P-PRIVATE-PATH hits in the immutable history of the orchestrator's plan-file commit df72747; clearing it is a history repair or an owner waiver, neither of which this executor may do"

duration: 17min
completed: 2026-11-26
status: complete
---

# Phase 11 Plan 26: SHAPE defect fixes (S-01, S-02, S-03) Summary

**Output drain and output status now poison and answer INTERNAL_DEFECT for a drain cursor or capture length out of range and for a ring head or count out of range (white-box cases, sweep 1599 lines with 0 differences, no sealed body moved), the public header text and the state-import check 1 line are corrected before the header hash is bound, and the gate stops at 274 of 275 only on a PII hit in the history of the plan-file commit df72747.**

## Performance

- **Duration:** about 17 min (4 min of it waiting for host load to fall)
- **Started:** 2026-11-26T01:50Z
- **Completed:** 2026-11-26T02:07Z
- **Tasks:** 3 of 3
- **Files modified:** 7 (6 source or documentation files plus PIN-IMPACT.md; the plan file in the first commit)

## Accomplishments

- **S-02.** `core/t64_observe.c`: `t64_machine_output_drain` routes a capture length beyond the capture buffer or a drain cursor beyond the capture length through `t64_internal_defect` (was `RESOURCE_EXHAUSTED`); `t64_machine_output_status` does the same for a video or audio ring head or count out of range, a capture length beyond the buffer and a cursor beyond the capture (was a quiet 0 or a floored free count). State import rejects all of these, so no caller input reaches them. Both functions write their output only after the checks, so a defect leaves it untouched. Reachable (valid) paths compute the same values: sweep 0 differences, `unit.core.api.output-status` and the whole `unit.core.api.*` family pass.
- **Tests (RED then GREEN).** `core/tests/test_api_poison.c` gains `defect_output_drain` (2 breaks) and `defect_output_status` (6 breaks), registered in `case_defect_sites`; each row calls the healthy instance first (control answers `OK`), breaks one member, expects `INTERNAL_DEFECT`, a poisoned instance, the result/data span/status struct byte for byte as poisoned by the test, and `POISONED` on the next call. RED run (`build/gate/11-26-task1/red.txt`, ignored): 36 failed checks, all in the new cases (drain got 14 want 38; status got 0 want 38).
- **Class table.** `docs/abi/status-classes.txt`: the `RESOURCE_EXHAUSTED` row says the drain no longer answers it for a bad cursor or capture length; the `INTERNAL_DEFECT` row lists the output observer inconsistencies as defect sites. `check-abi.py classes`: `constants=47 rows=47 findings=0`.
- **S-01.** `core/include/tendies64/t64_machine.h`, comment lines only: `T64_API` marks the public functions and hides nothing (no visibility attribute is promised; the `t64i_` prefix and the archive symbol check are the enforced property); `t64_machine_destroy` is unguarded by design and must not be called from inside one of the instance's own allocator callbacks; the descriptor returned by `t64_machine_profile` is the instance's v1-sized copy and a consumer built against a larger descriptor checks `struct_size` before any later member.
- **S-03.** `docs/spec/state-container.md` check 1: a NULL machine answers INVALID_ARGUMENT at the entry guard with the report untouched, only a NULL blob answers `ARG_NULL` (the paragraph is rewrapped).
- **Records.** PIN-IMPACT Shape-steps row 11-26 and an evidence paragraph; S-05 is carried to 11-23 (per-target offsets in the layout table, explicit reserved members as a Phase 12 final-bind question) and S-09 to 11-20 (one shared test header).

## Task Commits

1. **Plan-file path repair (Rule 3, see Deviations)** - `766d20a` (docs)
2. **Task 1: S-02 defect branches, white-box cases, class notes** - `6e53ecb` (fix)
3. **Task 1 follow-up: INIT forms for the four new test locals (repo.api-init-guard)** - `8d7f9de` (fix)
4. **Task 2: S-01 header text and S-03 documentation line** - `259784a` (docs)
5. **Task 3: PIN-IMPACT row, gate result and carried items** - `44ab26d` (docs, PIN-IMPACT.md only)

## Verification

| Check | Result |
|-------|--------|
| `ctest --preset gate -R '^(unit\.core\.api\.\|repo\.abi)'` | 17 of 17 passed (after the change) |
| `python3 -B scripts/check-rsp-no-hle.py --build build/gate` | `PASS: rsp-no-hle sources_scanned=85`, checker unchanged (neither edited function is a sealed body) |
| `python3 -B scripts/gen-prefix-header.py --check build/gate/core/libt64_core.a` | `ok (35 public functions, 220 renames)` |
| `python3 -B scripts/check-abi.py classes ...` | `constants=47 rows=47 findings=0` |
| `python3 -B scripts/check-abi.py header --include core/include` | c99 ok, c++17 ok, findings=0; header diff has comment lines only |
| `python3 -B scripts/check-api-init.py` | `files=183 findings=0 excluded=1` |
| `python3 -B scripts/digest-sweep.py --base f9124ae9fce134f31b645c6f2911a6f27fa41b9a --head HEAD` (also `--head WORKTREE` at the Task 1 state) | `1599 shared lines, 0 differences, 0 head-only lines` (both) |
| `sh scripts/check-docs.sh`, `sh scripts/check-stale-freeze-text.sh`, `sh tools/lint/core-lint.sh` | ok, `findings=0`, `violations=0` |
| `sh scripts/pii-scan.sh --tree` | `findings=0` (1379 files) |
| `sh scripts/pii-scan.sh` (full history) and `--range origin/main..HEAD` | **2 findings**, both `P-PRIVATE-PATH` in commit `df72747` (not from this plan; see Issues) |
| `sh scripts/gate.sh` (committed tree `8d7f9de`, output under `build/`, load 8.8 at start) | stopped at ctest after 154 s: 274 of 275 passed, the one failure `gov.pii.repo`; **no `[gate] PASS`** |
| `sh scripts/preflight-phase11.sh --no-replay` | fails at its `pii` step (same history hit); every other step ok, `linux-compile` 63 s ok, run with only the `pii` step stubbed: `[preflight] PASS` 66 s |

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Workstation paths in the plan file's `execution_context`**
- **Found during:** Task 1 (`sh scripts/pii-scan.sh --tree` before the first commit)
- **Issue:** `11-26-PLAN.md` lines 50 and 51 named two absolute home paths, so the tree scan (a Task 3 verify and a gate name) reported `P-PRIVATE-PATH` x2.
- **Fix:** replaced the two lines with the tilde form the other plans use.
- **Files modified:** `.planning/phases/11-installed-consumer-and-freeze-candidate/11-26-PLAN.md`
- **Commit:** `766d20a`

**2. [Rule 1 - Bug] New test locals had no INIT initializer (my own change)**
- **Found during:** Task 3 (`preflight-phase11.sh` guards step, `repo.api-init-guard`)
- **Issue:** four locals (`t64_output_drain_result` result and expected copy, `t64_output_status` status and expected copy) in the new cases were uninitialized, which `scripts/check-api-init.py` refuses.
- **Fix:** initialize with `T64_OUTPUT_DRAIN_RESULT_INIT` and `T64_OUTPUT_STATUS_INIT`; the poison helper still overwrites them with the sentinel before each call.
- **Files modified:** `core/tests/test_api_poison.c`
- **Commit:** `8d7f9de`

**3. [Rule 1 - Plan verify defect] `check-abi.py header` needs `--include core/include`**
- **Found during:** Task 2
- **Issue:** the plan's verify line runs `python3 -B scripts/check-abi.py header` with no argument and exits 2 (usage). The review record of 11-15 uses `--include core/include`.
- **Fix:** ran it with `--include core/include` (c99 ok, c++17 ok, findings=0). Recorded in PIN-IMPACT.

**4. [Method] Wording shaped by the plan's literal greps**
- The verify requires `must not be called from inside` and `INVALID_ARGUMENT at the entry guard` as single-line literals, so the destroy sentence is worded around the first and the state-container line writes INVALID_ARGUMENT without backticks (the table row of the same document keeps its backticks).

**5. [Method] RED and GREEN in one commit; `ring_free` removed**
- The project convention (11-03, 11-04) is one commit per task with test and code, so the RED run is a recorded run (`build/gate/11-26-task1/red.txt`), not a failing commit; a failing head would break the every-head-passes rule. `ring_free` (floor at 0 for a count above capacity) became dead once such a count is a defect, so it and its now-false comment are removed.

**Total deviations:** 3 auto-fixed (1 Rule 3, 2 Rule 1) and 2 method notes. **Impact:** none on the evidence; no digest, state byte, status value, public signature or layout moved.

## Issues Encountered

- **Gate not green: `gov.pii.repo` (owner stop, reported, not fixed).** The full gate stops at ctest with 274 of 275 passed. The one failure is `gov.pii.repo`, whose full-history scan reports two `P-PRIVATE-PATH` hits in the added lines of commit `df72747` (the orchestrator's commit that added `11-26-PLAN.md`). The tree is clean (`766d20a` fixed the plan-file lines); the history hit remains because the commit is immutable for this executor, and `pii-scan.sh` states that a real hit is an owner stop (never rewrite history). It will also fail the hosted gate, and `preflight-phase11.sh` fails at `pii` (`--range origin/main..HEAD`) until it is cleared, so it blocks the next push. `sh tools/lint/core-lint.sh`, the stage the ctest stop prevented, passes (`violations=0`). The orchestrator decides: repair the unpublished range (the earlier unpublished PII repair of 11-15 is the precedent) or an owner-approved history waiver for the pair (commit `df72747`, the plan file, 2 hits) in `tools/rights/pii-history-waivers.tsv`.
- Host load stayed 9 to 17 for most of the run (unrelated processes); the gate started after the load fell to 8.8 and no limit was widened.

## Known Stubs

None.

## Threat Flags

None: the plan adds no network, auth or file-access surface; the two defect branches are unreachable by caller input (T-11-64) and the header text is corrected before the freeze hash (T-11-65).

## Next Phase Readiness

- Resolve the `gov.pii.repo` history hit before any push (orchestrator or owner, see Issues); then 11-16 can run unchanged. Plan 11-26 ran out of wave order: STATE keeps 11-16 as the next plan.
- 11-20 owns S-09 (one shared test header for the duplicated poison, allocator and fixture-reader helpers); 11-23's layout table owns S-05 (per-target offsets of the six padded public structs).
- Nothing was pushed. Unpushed local commits now: `06e31ec`, `38a0e69`, `7f076cc`, `517fe00`, `df72747`, `c29a973`, this plan's five commits, this SUMMARY and the STATE and ROADMAP updates.

## Self-Check: PASSED

- FOUND: core/t64_observe.c, core/tests/test_api_poison.c, docs/abi/status-classes.txt, core/include/tendies64/t64_machine.h, docs/spec/state-container.md, PIN-IMPACT.md (row 11-26), build/gate/11-26-task1/red.txt
- FOUND: commits 766d20a, 6e53ecb, 8d7f9de, 259784a, 44ab26d (ancestors of HEAD)
- Measured: `git rev-list --count c29a973..HEAD` = 5 at the SUMMARY write

## Orchestrator note (2026-11-25)

The `gov.pii.repo` history hit named above was caused by the orchestrator-written plan file and was repaired by the orchestrator by rebuilding the unpublished range after 11-15 without the path (final tree identical; the separate path-replacement commit became redundant and was folded into the plan-file commit, so `766d20a` no longer exists). Commit SHAs in this record were re-pointed to the rebuilt commits. The gate re-run after the repair is recorded by the next plan.
