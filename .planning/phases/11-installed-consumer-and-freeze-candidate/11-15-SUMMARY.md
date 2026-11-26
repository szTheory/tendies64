---
phase: 11-installed-consumer-and-freeze-candidate
plan: 15
subsystem: api
tags: [review, scratch-clone, shape-wave, push, hosted-ci, ci-review-log, gate-growth-ledger, install-base]

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: the SHAPE wave 11-02 to 11-14 (struct_size layouts, create-info handshake, class table, poison, reset and unload, prefix, init macros) and the bound commit e66625a that closed DEV-11-01
provides:
  - an independent review of W0 and the SHAPE wave in a scratch clone (full gate PASS, checks (a) to (n), decision-by-decision table, findings S-01 to S-12, no blocking finding)
  - the first Phase 11 push (167a084 to 43b31e2, fast-forward) and a green hosted pair (run 45736981784)
  - LOG.md Runs row and finding F-127, the SHAPE row of the Gate-growth ledger with a margin warning
  - the Install base 06e31ecab5b84c08996aba79cb3ab6e29bb021cc
affects: [11-16, 11-17, 11-20, 11-21, 11-23]

actuals:
  tokens: 10600
  tasks: 3
  commits: 4

plan_head_before: 1d451b9b250f3707d8d055c590cc5e87bcff045d
plan_head_after: 38a0e69298d2e2b85656176777b0f824b76b5942

tech-stack:
  added: []
  patterns:
    - "independent hunk classification by reverse transform: strip the init-macro and call forms from the added text, canonicalize all-zero brace lists, compare with the removed text, hand-read only the residue"
    - "a review records a warm hosted sample with explicit cold projections when the cold run (cache deletion plus dispatch) is not a step the plan names"

key-files:
  created:
    - .planning/phases/11-installed-consumer-and-freeze-candidate/11-REVIEW.md
  modified:
    - .planning/phases/11-installed-consumer-and-freeze-candidate/PIN-IMPACT.md
    - docs/ci-reviews/LOG.md

key-decisions:
  - "No SHAPE finding is blocking: the three warnings (S-01 public-header text, S-02 masked inconsistency branches in output drain and output status, S-03 state-container NULL-machine wording) are text or unreachable defensive branches that stay re-bindable until the final bind; they go to a new numbered plan whose placement (before 11-23 binds the header hash, or as a later candidate amendment) is the orchestrator's decision"
  - "The one non-mechanical change in the codemod range (11-08, test_state_import.c NULL-machine row) is accepted: it replaces an indeterminate read with the documented untouched-output check"
  - "The pushed head is the reviewed source tree plus review records only; the cold gate-macos time is projected (757 to 810 s), not measured, because no cache deletion or dispatch is named by the plan"

patterns-established:
  - "a change that makes an output untouched must grep the tests that read that output (S-04 lesson)"

requirements-completed: []

coverage:
  - id: D1
    description: "SHAPE head passes the full gate in an isolated scratch clone"
    requirement: API-01
    verification:
      - kind: integration
        ref: "sh scripts/gate.sh in build/11-review/shape/src (clone detached at 1d451b9): [gate] PASS, total 162s, ctest 275 of 275"
        status: pass
    human_judgment: false
  - id: D2
    description: "Mechanical checks (a) to (n), decision table and findings recorded in 11-REVIEW.md SHAPE"
    requirement: API-02
    verification:
      - kind: other
        ref: "digest-sweep 1599/0; one bound commit e66625a with 17/92/1 hunks re-derived; --expect-blocked 0, --require-approved 2; nm 35 + 220; check-api-init files=183 findings=0; check-abi header and classes 0 findings"
        status: pass
    human_judgment: false
  - id: D3
    description: "Reviewed head pushed after the full preflight; hosted pair green; LOG.md and ledger rows; Install base"
    verification:
      - kind: other
        ref: "preflight PASS 102 s; run 45736981784 completed success (gate-macos 445 s, gate-linux 251 s); F-127; Install base verify"
        status: pass
    human_judgment: false

duration: 27min
completed: 2026-11-26
status: complete
---

# Phase 11 Plan 15: SHAPE-wave independent review, first push and Install base Summary

**The SHAPE wave passes the full gate in a scratch clone (275 of 275, 162 s), the bound commit re-classifies independently as 17 call-expression, 92 call-object and 1 owner-approved hunks, the digest sweep shows 0 differences over 1599 lines, no finding blocks, and the pushed head 43b31e2 is green on hosted macOS (445 s, warm) and Linux (251 s), with `repo.abi-prefix` passing on ELF for the first time.**

## Performance

- **Duration:** about 27 min
- **Started:** 2026-11-26T01:21Z
- **Completed:** 2026-11-26T01:48Z
- **Tasks:** 3 of 3
- **Files modified:** 3 (11-REVIEW.md new, PIN-IMPACT.md, docs/ci-reviews/LOG.md)

## Accomplishments

- Clone gate: `git clone --no-checkout --no-hardlinks` into `build/11-review/shape/src`, detached at `1d451b9`, `build/tools/install` copied in; `git archive` SHA-256 equal to the shared head; `sh scripts/gate.sh` `[gate] PASS`, total 162 s, ctest 92 s, 275 of 275.
- Checks (a) to (n) re-run in the clone: sweep `1599 shared lines, 0 differences`; one bound commit (`e66625a`) and the other four bound inputs plus `runner/tests/test_rdp_video.c` unchanged; the reviewer's own reverse-transform classifier gives 17 / 92 / 1 over 110 hunks (the 1 is `test_rdp.c` 486, owner answer A); authority `--expect-blocked` 0 and `--require-approved` 2 with only the pin line changed; the five no-HLE checker commits each change only their named entries; 35 unprefixed plus 220 `t64i_` globals; stale-text grep empty; init guard 183 files 0 findings; ABI header and class table clean; CI files untouched; PII over the range clean; no DEV-11-01 window commit on any remote; 21 phase11 names all with COST; the 511 codemod hunks and 250 flip-caller hunks are initializer or call forms except one recorded check change.
- Decision-by-decision table against every locked shape decision: all implemented; findings S-01 to S-12 (no blocking, three warnings, nine info), every review candidate (a) to (i) dispositioned.
- Full preflight PASS (102 s), PII tree and 79-commit range clean, rights scan clean; push `167a084..43b31e2` fast-forward; hosted run 45736981784 green; F-127 and its Runs row; SHAPE ledger row; Install base `06e31ecab5b84c08996aba79cb3ab6e29bb021cc`.

## Task Commits

1. **Task 1: clone gate** - `e7bfc4d` (docs)
2. **Task 2: mechanical checks, decision read, findings** - `43b31e2` (docs; the pushed head)
3. **Task 3: hosted pair, F-127, ledger row** - `06e31ec` (docs); **Install base** - `38a0e69` (docs, PIN-IMPACT.md only)

## Findings for the orchestrator

| id | severity | finding | proposed disposition |
|----|----------|---------|----------------------|
| S-01 | warning | header text: "a host sees exactly the marked functions" is false for the static archive (no hidden visibility; 255 globals visible); destroy from inside an own allocator callback is not refused or excluded; `t64_machine_profile` hands out a borrowed v1-sized descriptor without a "check struct_size first" note | new numbered plan; best before 11-23 binds the header hash |
| S-02 | warning | `t64_machine_output_drain` answers RESOURCE_EXHAUSTED and `t64_machine_output_status` reports zeros for internal inconsistencies (cursor beyond capture, ring head or count out of range) that state import already rejects; by AMBIG-A/B these are defect sites | same new plan (two branches to `t64_internal_defect`, a class-table note, one white-box case) |
| S-03 | warning | `docs/spec/state-container.md:448` still says a NULL machine gets `ARG_NULL` | same new plan (doc line) |
| S-04 to S-12 | info | recorded codemod check change, implicit padding, struct_size read before span checks, (b) not reproducible through the public API, defensive branches, duplicated test helpers, window evidence form, one COST under load, no-HLE prefix fallback | accepted or folded into 11-17 / 11-20 as listed in 11-REVIEW.md |

## Decisions Made

See key-decisions. The ones later plans need: the Install base literal; the S-01 to S-03 follow-up plan placement (orchestrator); the measured-cold-run recommendation before the 11-21 ledger row; 11-17 should pin per-target offsets (S-05); 11-20 can add an advance aliasing-span row, descriptor mutations and the shared poison helpers (S-06, S-07, S-09).

## Deviations from Plan

### Method deviations (no code change)

**1. [Rule 3 - Blocking] Task 2 verify run against the reviewed tree, not re-swept at the later docs head**
- **Found during:** Task 2
- **Issue:** the plan's verify runs `digest-sweep.py --base BASE --head HEAD`; after the Task 1 commit HEAD is the reviewed tree plus 11-REVIEW.md only, and re-running the 54 s sweep in the shared tree would write `build/10-sweep` there (T-11-26 asks review builds to stay in the clone).
- **Fix:** the sweep ran in the clone at the reviewed head `1d451b9` (0 differences); in the shared tree the verify's base and bound-commit parts ran unchanged, plus `git diff --quiet 1d451b9 HEAD -- core runner tools tests scripts fixtures docs` (exit 0), and the second verify line (authority, init guard, stale text) passed there.
- **Files modified:** none

**2. [Rule 3 - Blocking] Cold gate-macos time projected, not measured**
- **Found during:** Task 3
- **Issue:** the push run restored the Binutils cache on both jobs (warm); a cold measurement needs the cache entry deleted and a dispatched run, which the plan does not name and Phase 10 (F-126) recorded as orchestrator-only.
- **Fix:** three cold projections recorded (757, 791, 810 s; margins 143, 109, 90 s) with the ledger's under-120 s warning; no cache deleted, no extra run.
- **Files modified:** PIN-IMPACT.md ledger row, LOG.md F-127

**3. Ledger native seconds source**
- The plan names `CTestCostData.txt`; the shared tree's file blends the COST property (PIN-IMPACT "Shape steps" note), so the row uses the scratch clone's fresh file (one run per name: 8.23 s, equal to the `Passed` sum 8.22 s).

**Total deviations:** 3 method deviations, no auto-fixed code. **Impact:** none on evidence; the reviewer fixed nothing.

## Issues Encountered

- Host load 5 to 15 during the clone gate and the preflight (unrelated processes); nothing failed, no limit widened.
- The hosted macOS job was 77 s slower than F-126 while steps no SHAPE edit touches also slowed; the attributable growth is bounded by the 19.62 s summed time of the 21 names.

## Unpushed commits

`origin/main` is `43b31e2` (pushed by this plan). `06e31ec`, `38a0e69` (Install base), this SUMMARY and the STATE and ROADMAP updates are local; the next routine push (11-21 by the plan set) carries them.

## Self-Check: PASSED

- FOUND: .planning/phases/11-installed-consumer-and-freeze-candidate/11-REVIEW.md (sections Task 1, Task 2, Task 3 under "## SHAPE (11-01 to 11-14)")
- FOUND: commits e7bfc4d, 43b31e2, 06e31ec, 38a0e69 (ancestors of HEAD); origin/main = 43b31e2c0511155622dd8428dcd2f2368fe40501
- FOUND: "- Install base: 06e31ecab5b84c08996aba79cb3ab6e29bb021cc" (one line, its commit changes only PIN-IMPACT.md)
- FOUND: LOG.md Runs row 45736981784 and finding F-127
