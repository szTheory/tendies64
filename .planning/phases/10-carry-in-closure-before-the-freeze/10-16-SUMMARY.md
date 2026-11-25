---
phase: 10-carry-in-closure-before-the-freeze
plan: 16
subsystem: docs
tags: [provisional-name-sweep, freeze-readiness, phase9-triage, relabel-gate, test-replay-link, phase11-carry-ins]

requires:
  - phase: 10-carry-in-closure-before-the-freeze
    provides: the build-identity-bound statement and container version 2 (plan 10-11), the format string pairing (plan 10-15), the preflight (plan 10-01), the section 5 to 11 records of PIN-IMPACT.md
provides:
  - docs/spec relabel of the 15 stale "until Phase 10" sites in the 7 live spec docs to the Phase 11 freeze candidate (final ABI bind at the end of Phase 12); the container format strings v1 and v2 and their pairing with states 25 and 26 settled in Phase 10
  - PIN-IMPACT.md section 12 (every relabel and settle site with old and new phrase) and section 13 (Phase 9 carry-in triage, eight rows, and the Phase 10 findings deferred to the freeze-candidate review)
  - test_replay no longer links the dead t64_artifact_publisher.c
  - a Carry-ins line in ROADMAP Phase 11
affects: [10-17, 10-18, 10-19, phase-11]

plan_head_before: 9c60de008ce38d0fe806836b8755102e1cf792c8
plan_head_after: 8c44c03c159fcc51a595b299753e317649e3b2ab

actuals:
  tokens: 8737
  tasks: 3
  commits: 4

tech-stack:
  added: []
  patterns:
    - "a relabel replaces one literal phrase everywhere (provisional until the Phase 11 freeze candidate (final ABI bind at the end of Phase 12)) so a later phase can find every site with one search; the stale-label gate is whitespace-normalized so a wrapped phrase counts"
    - "a dead link is proved dead from both sides before it is removed: nm -u of the target's other objects against the symbols the dropped object defines, then the case inventory checker on the relinked Phase 9 binary"
    - "a comment count that gates later work is re-measured with a wrap-aware, case-insensitive search (git grep -n -i -E 'until (the )?Phase 10'); a one-line lowercase grep undercounts"

key-files:
  created: []
  modified:
    - docs/spec/raw-capture.md
    - docs/spec/result-protocol.md
    - docs/spec/machine-contract.md
    - docs/spec/replay.md
    - docs/spec/state-container.md
    - docs/spec/si-pif-controller.md
    - docs/spec/save-media.md
    - runner/CMakeLists.txt
    - .planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md
    - .planning/ROADMAP.md

key-decisions:
  - "Only the container format strings t64.state-container.v1 and v2 and their pairing with states 25 and 26 are settled in Phase 10 (plans 10-11 and 10-15); every other provisional name (state flag names, closed recorder-failure and state-export detail names, the third SI trace kind name, T64_REAL_KEEP_WORKSET, the info call) was not decided by Phase 10 and stays provisional until the Phase 11 freeze candidate"
  - "The deferred production comments are 20 lines in 8 files by a wrap-aware search, not 19; the ROADMAP Phase 11 Carry-ins line says so and gives the command that must print nothing when Phase 11 is done"
  - "Phase 9 triage: split of unit.core.state.import deferred, replay-driver duplication deferred, dead publisher link done (3deee7d), stale production comments deferred; four further items the 09-REVIEW-FIX tagged for Phase 10 (L3 IN-01, L3 IN-06, L1 IN-04 with L2 IN-02 and IN-04, L4 IN-02) deferred to Phase 11 with reasons"
  - "W2-01, W2-02, DEF-W1-01 and W1-03 are recorded as deferred to the Phase 11 freeze-candidate review at the dispatch direction; this plan fixes none of them"

requirements-completed: [CORE-06, GATE-01]

coverage:
  - id: D1
    description: "The 15 stale 'provisional until Phase 10' sites of the 7 live spec docs are relabeled to the Phase 11 freeze candidate or settled, and the relabel gate prints stale 0"
    requirement: "CORE-06"
    verification:
      - kind: other
        ref: "python3 -c relabel gate over the 7 spec docs (stale 15 at the plan start, 14 after Task 1, 0 after Task 2); sh scripts/check-docs.sh prints check-docs: ok; sh scripts/check-claims.sh prints errors=0"
        status: pass
    human_judgment: false
  - id: D2
    description: "No closed-phase record and no pinned file moved: docs/claims and docs/evidence, the oracle authority checker with PINNED_REQUEST_SHA256 e131cbe7..., fixtures/manifest.json, fixtures/state/decode_container.py and docs/spec/phase09-cases.json are byte-identical to the plan start"
    requirement: "GATE-01"
    verification:
      - kind: other
        ref: "git diff --quiet 9c60de0..HEAD over those paths exits 0; ctest -R harness.rdp-oracle-authority and harness.phase09-qualification pass"
        status: pass
    human_judgment: false
  - id: D3
    description: "The dead t64_artifact_publisher.c link is removed from test_replay only; the replay CTest names pass and the Phase 9 replay case inventory is unchanged"
    requirement: "CORE-06"
    verification:
      - kind: integration
        ref: "ctest -R '^(unit\\.replay\\.suite|runner\\.replay-mix)$' (2 of 2); python3 -B scripts/check-phase09-cases.py --build build/gate --suite replay (15 cases, no inventory error); sh scripts/preflight-phase10.sh PASS in 86 s with 228 compile units"
        status: pass
    human_judgment: false
  - id: D4
    description: "Every Phase 9 item tagged for Phase 10 has a recorded disposition with a reason (three plan items plus the stale comments row and four further review items), and the Phase 10 warnings W2-01, W2-02 and DEF-W1-01 are recorded as freeze-candidate review carry-ins"
    requirement: "CORE-06"
    verification:
      - kind: other
        ref: "PIN-IMPACT.md section 13 (T1 to T8) and 13.1; grep -c 'Phase 9 carry-in triage' prints 1; ROADMAP Phase 11 Carry-ins line"
        status: pass
    human_judgment: true
    rationale: "The deferral reasons are recorded judgments; whether the freeze-candidate review accepts W2-01, W2-02 and DEF-W1-01 as later work is an orchestrator and owner decision, not a test result"
  - id: D5
    description: "The build-identity-bound, outside-the-ABI-promise statement written in plan 10-11 is present in state-container.md and machine-contract.md (checked, not rewritten)"
    requirement: "CORE-06"
    verification:
      - kind: other
        ref: "grep -c build-identity-bound: state-container.md 2, machine-contract.md 1"
        status: pass
    human_judgment: false

duration: 12min
completed: 2026-11-24
status: complete
---

# Phase 10 Plan 16: Provisional-name sweep and Phase 9 carry-in triage Summary

**All 15 stale "provisional until Phase 10" labels of the 7 live spec docs now name the Phase 11 freeze candidate (final ABI bind at the end of Phase 12) or are settled (the container format strings v1 and v2), the dead publisher link is gone from `test_replay`, and every Phase 9 and Phase 10 item left open has a recorded disposition and reason.**

## Performance

- **Duration:** about 12 min (reading from 03:25Z, the commit ledger written at 03:28Z, SUMMARY at about 03:38Z)
- **Tasks:** 3
- **Commits:** 4 task commits (Task 3 is two commits so the triage row can name the commit of the link removal)
- **Files modified:** 10 (7 spec docs, `runner/CMakeLists.txt`, `PIN-IMPACT.md`, `ROADMAP.md`)

## Accomplishments

- Relabel gate (whitespace-normalized): stale 15 at the plan start (raw-capture 1, result-protocol 2, machine-contract 5, replay 2, state-container 3, si-pif-controller 0, save-media 2), 14 after the tracer, 0 after Task 2. Only the one settlement sentence still contains "Phase 10" in the seven docs. `check-docs` and `check-claims` pass; `docs/claims` and `docs/evidence` are untouched.
- Settled in Phase 10 (plans 10-11 and 10-15): the container format strings `t64.state-container.v1` (state 25) and `t64.state-container.v2` (state 26) and their pairing, as state-format constants outside the ABI promise. The two "Phase 10 item or follow-up" scope sentences are now "a Phase 11 item" with the same content. The two sentences that waited for "the state content freeze at the end of plan 09-06" (`si-pif-controller.md`, and the identical one in `save-media.md`) now state the current rule: state content frozen by state versions 25 and 26, no API or ABI freeze.
- `test_replay` no longer links `t64_artifact_publisher.c`: neither `t64_replay.c.o` nor `test_replay.c.o` referenced any of the 8 symbols it defines, and the relinked binary defines 0 `t64_artifact` symbols. `unit.replay.suite` and `runner.replay-mix` pass, `check-phase09-cases.py --suite replay` validates the unchanged 15-case inventory, and the preflight compile drops from 229 to 228 units.
- `PIN-IMPACT.md` section 12 lists every relabel and settle site with the old and new phrase; section 13 holds the eight-row Phase 9 triage and section 13.1 the Phase 10 carry-ins (W2-01, W2-02, DEF-W1-01, W1-03) with reasons. ROADMAP Phase 11 carries them in one Carry-ins line.
- The build-identity statement of plan 10-11 is present (`build-identity-bound` count 2 in `state-container.md`, 1 in `machine-contract.md`) and was not rewritten.

## Task Commits

1. **Task 1: relabel one doc end to end (tracer)** - `f206e7e` (docs). Gate 15 to 14; `raw-capture.md:132`; `PIN-IMPACT.md` section 12 started. The tracer verify passed, so expansion continued.
2. **Task 2: relabel or settle every remaining site** - `80c819c` (docs). Gate 14 to 0; check-docs, check-claims and PII scan pass.
3. **Task 3: Phase 9 triage and the dead publisher link** - `3deee7d` (chore, the CMake edit) and `8c44c03` (docs, the triage record and the ROADMAP Carry-ins line).

**Plan metadata:** the SUMMARY, STATE and ROADMAP progress commit follows this file.

## Files Created/Modified

- `docs/spec/raw-capture.md`, `result-protocol.md`, `machine-contract.md`, `replay.md`, `state-container.md`, `si-pif-controller.md`, `save-media.md` - relabels and settlements only; no sentence changes its claim.
- `runner/CMakeLists.txt` - one source dropped from the `test_replay` target.
- `.planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md` - sections 12 and 13.
- `.planning/ROADMAP.md` - the Phase 11 Carry-ins line.

## Decisions Made

See key-decisions. The ones later plans need: Phase 11 verifies the production-comment relabel with `git grep -n -i -E 'until (the )?Phase 10' -- core runner tools` printing nothing (20 lines today), and `fixtures/state/decode_container.py` keeps its one occurrence because it is a pinned Phase 9 oracle.

## Deviations from Plan

### Auto-fixed Issues

None needed: no bug and no blocker appeared.

### Additions beyond the plan text

**1. [Rule 2 - consistency] Second settle row S2 (`save-media.md:208-209`)**
- **Found during:** Task 2
- **Issue:** the plan names only `si-pif-controller.md:250` for the stale "until the state content freeze at the end of plan 09-06" sentence, but `save-media.md` carries the identical sentence for section 11; the relabel regex does not count either.
- **Fix:** settled with the same current rule and recorded as row S2 beside S1 in `PIN-IMPACT.md` section 12.
- **Files modified:** `docs/spec/save-media.md`
- **Commit:** `80c819c`

**2. Triage has eight rows, not four.** The plan asks for four rows (split, replay-driver, dead link, stale comments); `09-REVIEW-FIX.md` tags four more items "owner Phase 10" (L3 IN-01, L3 IN-06, L1 IN-04 with L2 IN-02 and IN-04, L4 IN-02), so rows T5 to T8 give them a disposition rather than leave them silently inherited. All four are deferred to Phase 11 with a reason. **Commit:** `8c44c03`.

**3. ROADMAP Carry-ins line (dispatch direction).** `ROADMAP.md` is not in the plan's file list; the dispatch asked for W2-01, W2-02 and DEF-W1-01 to be added to Phase 11 as carry-ins if the plan wording allows. One docs-only line after the Phase 11 Order field does that; the Order field itself is unchanged. **Commit:** `8c44c03`.

**4. Count correction 19 to 20.** The plan and ROADMAP say 19 deferred comments. A wrap-aware, case-insensitive search finds 20 lines in 8 files (one phrase wraps over a comment star at `t64_machine.h:598-599`, one reads "until the Phase 10 interface review" at `:120`). Nothing in code was touched; the correction is recorded in `PIN-IMPACT.md` T4 and the Carry-ins line.

**5. Two commits for Task 3** so row T3 can cite the removal commit `3deee7d`.

**Total deviations:** 0 auto-fixed, 5 additions (all docs or record-keeping). **Impact:** none on any pin, digest or build identity.

## Issues Encountered

None. The first full preflight ran at host load 9.5 (above the quiet-host threshold of 8, a WARN only): 86 s, PASS.

## Threat Flags

None. Docs and one CMake source-list removal; no new endpoint, auth path, file access or schema.

## Known Stubs

None.

## Authentication Gates

None.

## Verification Results

| Check | Result |
|-------|--------|
| relabel gate over the 7 docs | `stale 0` (was 15) |
| `sh scripts/check-docs.sh`, `sh scripts/check-claims.sh` | `check-docs: ok`, `errors=0` |
| `git diff --quiet 9c60de0..HEAD` over `docs/claims`, `docs/evidence`, `scripts/check-rdp-oracle-authority.py`, `fixtures/manifest.json`, `fixtures/state/decode_container.py`, `docs/spec/phase09-cases.json` | exit 0; `PINNED_REQUEST_SHA256` still `e131cbe7...` |
| `cmake --build --preset gate`, `ctest -R '^(unit\.replay\.suite\|runner\.replay-mix)$'` | build ok; 2 of 2 passed (`ctest -N` reports 2) |
| `python3 -B scripts/check-phase09-cases.py --build build/gate --suite replay` | validated, 15 cases, no inventory error |
| `ctest --preset gate -R 'replay\|phase09\|oracle-authority\|test-inventory\|rights\|state-field-use'` | 12 of 12 passed; `check-test-inventory.sh --preset gate` PASS (253 names) |
| `grep -c build-identity-bound` | `state-container.md` 2, `machine-contract.md` 1 |
| `sh scripts/preflight-phase10.sh` (clean committed tree after `3deee7d`) | PASS, 86 s (linux-compile 46 s with 228 units, post-gate-replay 38 s) |

## Next Phase Readiness

Ready for 10-17 (thin-delta Phase 10 qualifier and rehearsal). Items the orchestrator should know: `GATE-01` stays open until the measured cold run of plan 10-19, and `requirements.ready-ids` withholds both `CORE-06` and `GATE-01` while plans 10-17 to 10-19 still declare them. The preflight has not been pushed; `origin/main` is `f57b019` and plan 10-18 pushes.

## Self-Check: PASSED

- Files: the seven spec docs, `runner/CMakeLists.txt`, `PIN-IMPACT.md` and `ROADMAP.md` exist.
- Commits `f206e7e`, `80c819c`, `3deee7d` and `8c44c03` are ancestors of HEAD; `git rev-list --count 9c60de0..HEAD` is 4 before this file.
- All task acceptance criteria and the plan-level verification re-ran green (table above).
