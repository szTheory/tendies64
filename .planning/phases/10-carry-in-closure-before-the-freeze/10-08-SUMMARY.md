---
phase: 10-carry-in-closure-before-the-freeze
plan: 08
subsystem: ci
tags: [review, scratch-clone, wave-1, ver-06, re-derivation, mutants, push, hosted-ci, ci-review-log, gate-growth-ledger]

requires:
  - phase: 10-carry-in-closure-before-the-freeze
    provides: plans 10-06 and 10-07 (the nine unit.core.vg CTest names, the 15 mutant rows and the Wave 1 base f6ab8c2) as the Wave 1 diff under review; plan 10-05 for the clone-detached-at-the-head review method (W0-01)
  - phase: 07-rdp-reference-renderer-and-vi-scanout
    provides: the primary-document policy text (rdp.md V7, vi.md, 07-08 exponent table) that the independent calculator was written from
provides:
  - 10-REVIEW.md Wave 1 section (scratch gate, 94-row re-derivation, checks, 15-row mutant table, sibling-copy assessment, findings W1-01 to W1-05, push and hosted pair)
  - the second Phase 10 push (origin/main 33f6e9c to a0c0a38, 18 commits) after a passing 63 s preflight, and a green hosted pair (run 45321391540)
  - LOG.md Runs row and F-121, the Wave 1 warm sample and gate-growth measurement
  - PIN-IMPACT.md Gate-growth ledger Wave 1 row and the Wave 2 base 6dd785986f6aa78c6da5573a558b15b386a4edee
affects: [10-09, 10-13, 10-18, 10-19, phase-11]

actuals:
  tokens: 6025
  tasks: 3
  commits: 4

tech-stack:
  added: []
  patterns:
    - "a reviewer writes a fresh calculator from the documents before reading the committed literals, checks the calculator's own constants against the documents by program, then compares extracted literals (94 rows)"
    - "a residual observation is classified by measurement: throwaway mutants applied one at a time in the scratch clone, the whole gate ctest run, then reverted"
    - "the mutant driver runs in the review scratch clone, never in the shared build tree"

key-files:
  created: []
  modified:
    - .planning/phases/10-carry-in-closure-before-the-freeze/10-REVIEW.md
    - .planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md
    - docs/ci-reviews/LOG.md

key-decisions:
  - "Wave 1 (plans 10-06 and 10-07) passes the full gate in an isolated clone (236 of 236, 123 s), all 94 literal comparisons against a calculator written from the documents match, no production or oracle-bound file moved, the digest sweep reports 0 differences over 1599 lines, and 15 mutants are KILLED with both controls as expected: no blocking finding, so VER-06's named gaps VG-01 to VG-05 and the NTSC clock vector are independently confirmed closed"
  - "The three sibling copies of the perspective and major-edge floor (rgba8 texel path, LOD projection, texture major-edge) are residual information, not defects: five scratch mutants all pass the whole gate ctest except the repo.rsp-no-hle body seal; recorded as deferred item DEF-W1-01"
  - "The second hosted pair is green (gate-macos 326 s, ctest 187 s, 236 names): the nine new names are no visible hosted cost inside the F-119 band, recorded as F-121 and the Wave 1 ledger row under the G4 exception"
  - "The Wave 2 base 6dd785986f6aa78c6da5573a558b15b386a4edee is recorded in a commit that changes only PIN-IMPACT.md (b510f97)"

patterns-established:
  - "A review plan reads the plan text as a claim and re-runs every cited command; a method correction learned in the previous review (clone detached at the head) is applied without waiting for the plan wording"

requirements-completed: [VER-06]

coverage:
  - id: D1
    description: "The Wave 1 head passes the full gate (sh scripts/gate.sh, default environment) in a clone detached at the reviewed head, outside the shared tree"
    requirement: "VER-06"
    verification:
      - kind: integration
        ref: "sh scripts/gate.sh in build/10-review/w1/src (clone detached at 37151eb, archive SHA-256 equal to the main repository's): [gate] PASS, total 123s, ctest 236 of 236"
        status: pass
    human_judgment: false
  - id: D2
    description: "Independent re-derivation of the VG literals: a calculator written from the documents before the committed literals were read matches all 94 compared values (NTSC deadlines 1,042,709 and 3973, all 17 Z exponent boundaries and the far word, the decal tolerance case, saturation, nine delta rows, alpha clear, serrate literals, perspective floor words, shade floor words, Bayer word)"
    requirement: "VER-06"
    verification:
      - kind: other
        ref: "build/10-review/w1/calc.py and compare.py (uncommitted): rows=94 mismatches=0; Z table and Bayer matrix cross-checked against the documents by program"
        status: pass
    human_judgment: false
  - id: D3
    description: "Wave 1 changed no production source and no oracle-bound file: empty production-path diff, empty intersection with the 13 EXPECTED_CORPUS_PATHS, digest sweep 0 differences"
    requirement: "VER-06"
    verification:
      - kind: other
        ref: "git diff --name-only f6ab8c2..HEAD filtered for core/*.c, core/*.h, core/include, runner/*.c (empty); python3 -B scripts/digest-sweep.py --base f6ab8c2 --head HEAD: 1599 shared lines, 0 differences, 0 head-only lines, exit 0"
        status: pass
    human_judgment: false
  - id: D4
    description: "All 15 mutants of the table are KILLED by a named check of the new binary (a failed CHECK line of its own file after a passing baseline), the equivalent control SURVIVES and the non-compiling control is INVALID"
    requirement: "VER-06"
    verification:
      - kind: other
        ref: "python3 -B scripts/p10-mutants.py --check-anchors (exit 0) and --run (exit 0, 40 s) in the scratch clone; 10-REVIEW.md Wave 1 mutant table"
        status: pass
    human_judgment: false
  - id: D5
    description: "Reading of each new case for a reachable comparator, and classification of the three sibling copies of the perspective floor as residual (five scratch mutants, whole gate ctest each, only the repo.rsp-no-hle body seal fails)"
    requirement: "VER-06"
    verification: []
    human_judgment: true
    rationale: "Whether a case's comparator is reachable and whether an unpinned sibling is residual or a defect is the reviewer's classification; the mechanical evidence is the passing controls, the killed mutants and the survived sibling mutants, not a proof that no other site differs"
  - id: D6
    description: "The reviewed head is pushed by pinned SHA after a passing preflight and its hosted pair is green; the Runs row, F-121, the Wave 1 ledger row and the Wave 2 base are recorded"
    requirement: "VER-06"
    verification:
      - kind: integration
        ref: "sh scripts/preflight-phase10.sh PASS 63 s; run 45321391540 (ci.yml, push, head a0c0a38) completed success on both jobs; Task 3 verify commands pass (hosted completed success; grep -c '^| F-121 ' is 1; Wave 2 base one 40-hex line and a one-file recording commit)"
        status: pass
    human_judgment: false

duration: 28min
completed: 2026-11-24
status: complete
commits: 4
plan_head_before: 37151ebea980b6fad54edcc493b25d60dc00d4bd
plan_head_after: b510f978efd9a13ca58fc75745a00c5f3f9b08f4
---

# Phase 10 Plan 08: Wave 1 review, push and hosted pair Summary

**Wave 1 (plans 10-06 and 10-07, VER-06) was reviewed independently in a clone detached at the head (full gate 236 of 236 in 123 s), 94 literal comparisons against a calculator written from the documents match, no production or oracle-bound file moved (sweep 1599 lines, 0 differences), 15 mutants are KILLED with both controls as expected, and the head was pushed (18 commits, 63 s preflight) with a green hosted pair, run 45321391540; no blocking finding, five info findings, the Wave 1 ledger row, F-121 and the Wave 2 base are recorded.**

## Performance

- **Duration:** 28 min (2026-11-24T03:21:37Z to about 03:50Z, of which the hosted run was 6 min)
- **Tasks:** 3 (Task 1 tracer, Tasks 2 and 3 auto)
- **Files:** 3 modified (`10-REVIEW.md` +205 lines, `PIN-IMPACT.md`, `docs/ci-reviews/LOG.md`); no source file touched
- **Host:** 1-minute load 7 to 16 during the runs (other sessions on the same machine), so local seconds are upper bounds

## Accomplishments

- **Task 1 (tracer, `bd37f85`).** The reviewed head `37151eb` was built and tested outside the shared tree in a local clone detached at the head plus a copy of `build/tools/install` (the W0-01 method; the archive SHA-256 of the clone equals the main repository's). `sh scripts/gate.sh`: `[gate] PASS`, total 123 s, ctest 236 of 236 in 70 s; the nine new names ran in 0.00 to 0.02 s each (ntsc-clock 0.10 s in the cost data). First attempt passed.
- **Task 2 (`a0c0a38`).** A fresh calculator was written from `vi.md`, the 07-08 exponent table and `rdp.md` before the committed literals were read; its Z table and Bayer matrix were then checked against the documents by program, and a second script compared 94 extracted literals (NTSC 1,042,709 and 3973 with exact rational arithmetic, all 17 exponent boundaries with their decoded values, the decal tolerance case, saturation, nine delta-ceiling rows, alpha clear, serrate literals, perspective floor texel words, shade floor words, Bayer words): 0 mismatches. The production-path diff since the Wave 1 base is empty, none of the 13 oracle-bound paths moved, the digest sweep reports 1599 shared lines and 0 differences, and `p10-mutants.py --check-anchors` and `--run` (scratch clone) exit 0 with 15 KILLED, the SURVIVED control and the INVALID control as expected. Every case was read for a reachable comparator: each control asserts its divergence byte and every mutant sits on the line its vector reaches. Findings W1-01 to W1-05 (all info).
- **Sibling-copy assessment (orchestrator item, inside Task 2).** Five throwaway mutants of the three sibling floor copies (S and T of `t64_rdp_tmem_triangle_pixel_rgba8`, S and T of `project_texture_q16`, the `from_major` of `texture_channel_q16`) were applied one at a time in the scratch clone and the whole gate ctest run each time: only `repo.rsp-no-hle` fails ("executable body changed; fetched-path review is required", the sealed-body guard); 235 of 236 pass. They are residual, not defects, and recorded as DEF-W1-01.
- **Task 3 (`6dd7859`, then `b510f97`).** The preflight passed on the clean committed tree (63 s, 221 units compiled under the act image cc, 11 post-Gate steps replayed, PII clean on the tree and the 18-commit range). `origin/main` moved from `33f6e9c` to `a0c0a38` by a pinned-SHA fast-forward push. Run `45321391540` completed with success on both jobs with a Binutils cache hit on both. The Runs row and F-121 are in `LOG.md`; the PIN-IMPACT.md ledger has its Wave 1 row; the Wave 2 base is recorded in a commit that changes only that file.
- **Hosted after-timings (against the F-120 row and the F-119 band).** gate-macos run 326 s (Gate 288 s, ctest 187 s of 236 names, test-inventory-selftest 38 s, audio records 21 s) against 346 s (298, 184, 42, 28); gate-linux run 230 s (Gate 181 s, ctest 115 s) against 229 s. The nine new names add 0.29 s (macOS) and 0.51 s (Linux); `unit.core.state.import` again starts 2nd (172.55 s macOS, 103.11 s Linux). The macOS ctest delta of +3 s is inside the 52 s spread of F-119, so no growth is read.

## Re-derivation (committed literal against a calculator written from the documents)

| Vector | Derived | Committed | Match |
|--------|---------|-----------|-------|
| NTSC first field deadline | 812,175 cycles * 62,500,000 / 48,681,812 = 1,042,708.4657, ceil 1,042,709 | 1,042,709 (`0x000fe915`) | yes |
| NTSC first sampled line | 3972.2227, ceil 3973 | 3973 | yes |
| Z exponent boundaries (17 values) | visible 0x0000 .. 0xfffc, decoded 0x00000 .. 0x3ffff; the rows tile 0 .. 0x3ffff | the three 17-entry arrays | yes (34 + 1) |
| decal tolerance | tolerance max(2, 8) = 8; raw 0x2001 and 0x1fff pass at difference 8, 0x2002 and 0x1ffe fail at 16 | five vectors, `expected_equality` | yes |
| primitive-Z saturation | 0xffc0, 0xffe0, 0xffe0, 0xffe0 | `vectors` | yes |
| delta ceiling | nine plane rows from floor and ceil arithmetic | nine rows | yes (18) |
| alpha clear, serrate, perspective, shade, Bayer | 255,0,0,0; 198,74,247,255 and 24,231,99,0; 0x384d, 0x404f, 0x09cd, 0x0a0f; 0x3c17 and 0x3bd7; 0x5309, 0x4ac9, 0x4a53 | the literals of cases 6 to 9 | yes |

## Task Commits

1. **Task 1: scratch gate of the Wave 1 head** - `bd37f85` (docs)
2. **Task 2: re-derivation, checks, mutant table, findings** - `a0c0a38` (docs); this is the head that was pushed and run
3. **Task 3: hosted pair, F-121, ledger row** - `6dd7859` (docs)
4. **Task 3: Wave 2 base record (only PIN-IMPACT.md)** - `b510f97` (docs)

**Plan metadata:** this SUMMARY and the STATE and ROADMAP updates follow in docs(10-08) commits. The pushed head `a0c0a38` differs from the gated tree `37151eb` only by `10-REVIEW.md`.

## Files Created/Modified

- `.planning/phases/10-carry-in-closure-before-the-freeze/10-REVIEW.md` - the Wave 1 section (scratch gate, re-derivation, checks, mutant table, reading, sibling-copy assessment, findings, push, hosted pair)
- `.planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md` - the Wave 1 ledger row and the `- Wave 2 base:` line
- `docs/ci-reviews/LOG.md` - one Runs row and F-121

## Decisions Made

See key-decisions. The ones later plans need: the Wave 2 base literal; DEF-W1-01 (an optional follow-up vector, not required); and that the second warm sample shows nine added names as no visible hosted cost.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Premise correction] The scratch tree is a clone, not a bare archive**
- **Found during:** Task 1
- **Issue:** the plan repeats the 10-05 recipe "git archive into a scratch tree", which cannot pass the gate (finding W0-01 of plan 10-05: no repository of its own, no history).
- **Fix:** a local clone detached at the head plus a copy of `build/tools/install`, as the orchestrator's method correction and W0-01 prescribe; the archive of the clone and of the main repository hash the same.
- **Files modified:** none (scratch only, under the ignored `build/10-review/`)
- **Verification:** `[gate] PASS`, 236 of 236
- **Commit:** `bd37f85` (record)

**2. [Rule 1 - Premise correction] Tasks 1 and 2 were committed before the push**
- **Found during:** Task 3
- **Issue:** the plan lists the 10-REVIEW.md commit after the hosted run; the task commit protocol requires a commit per task, as in plan 10-05.
- **Fix:** Tasks 1 and 2 were committed first; the pushed and hosted head `a0c0a38` is a docs commit that differs from the gated tree `37151eb` only by `10-REVIEW.md`.
- **Commit:** `bd37f85`, `a0c0a38`

**3. [Rule 1 - Premise correction] The mutant driver ran in the scratch clone**
- **Found during:** Task 2
- **Issue:** the plan's verify command runs `p10-mutants.py --run` at the repository root, but the review rule (10-REVIEW.md header, AUTONOMY lessons) keeps mutant runs out of the shared tree.
- **Fix:** `--check-anchors` and `--run` ran in the clone at the head; results are the table in 10-REVIEW.md. The driver builds its own throwaway worktree, so the shared tree was never written.
- **Commit:** `a0c0a38`

**4. [Rule 2 - Added check] Sibling-copy assessment by measurement**
- **Found during:** Task 2
- **Issue:** the orchestrator asked for the 10-07 observation (three unpinned sibling copies) to be assessed and classified.
- **Fix:** five throwaway mutants in the scratch clone (never committed, reverted, `git status --short` clean afterwards); classified residual/info with deferred item DEF-W1-01.
- **Commit:** `a0c0a38`

**5. [Rule 1 - Premise correction] Counts and a path pattern in the plan text**
- **Found during:** Task 2
- **Issue:** the plan counts 13 mutants (the table has 15: plan 10-06 added two rows) and says no `core/*.c` path may change (the new test is `core/tests/test_vg_gaps.c`, a test file; "production source" means `core/*.c`, `core/*.h`, `core/include` and `runner/*.c`).
- **Fix:** the review records the 15 and applies the pattern to production paths.
- **Commit:** `a0c0a38`

**Total deviations:** 5 (3 Rule 1 premise corrections of the plan text, 1 Rule 1 ordering, 1 Rule 2 added check)
**Impact on plan:** none on scope or evidence; deviation 1 repeats the method correction of 10-05.

## Review findings (by severity)

| Severity | Count | Rows |
|----------|-------|------|
| blocking | 0 | none |
| warning | 0 | none |
| info | 5 | W1-01 sibling copies of the floor text unpinned behaviorally (residual, DEF-W1-01); W1-02 controls prove the comparator, the mutants prove the behavior; W1-03 the NTSC comment cites a vi.md section that does not state the ceil rule (it is printed in the tracer example); W1-04 delta rows 8191 and 0 are not independent edges; W1-05 plan counts 13 mutants, table has 15 |

## Issues Encountered

- No vector failed and no defect was found: Wave 1 behaves as `rdp.md`, `vi.md` and the 07-08 table say. No fix plan is requested.
- The first mutant-run attempt was started as a shell background job and looked finished when its wrapper exited; the driver itself kept running and finished with exit 0 (40 s). No evidence was lost.
- A first sibling mutant removed a floor correction without a `(void)` use of the remainder and failed the `-Werror` build; the probe was corrected to the form `p10-mutants.py` uses before any result was read.
- The push command used the pinned SHA with the refspec in braces (the 10-05 zsh lesson), no local failure.
- The `build/10-review/w1` clone (220 MB with its build), `build/10-sweep` (50 MB) remain under the ignored `build/` for the orchestrator to prune.

## Authentication Gates

None. `gh` was authenticated for the push, the read-only run reads and the log download.

## User Setup Required

None - no external service configuration required.

## Known Stubs

None.

## Threat Flags

None new. T-10-17 mitigated (the preflight `pii` step passed on the tree and the 18-commit range before the push, the range held no binary, ROM-extension file, workstation path or e-mail address, and the remote is the public szTheory repository; the calculators stay uncommitted under the ignored `build/`; the review text names no workstation path). No hosted cache was deleted.

## Next Phase Readiness

- Wave 2 starts at the base `6dd785986f6aa78c6da5573a558b15b386a4edee` (`PIN-IMPACT.md` section 5); plan 10-09 (the identity bump under four new registered ids) may begin. Every later Wave 1 check reads the Wave 1 base `f6ab8c2...`, never HEAD.
- Plans 10-09 to 10-13: assert that the four si-v2 ids appear as head-only digest lines in the sweep (W0-02, still open); the Wave 2 review (plan 10-13) uses the same clone detached at the head.
- DEF-W1-01 (behavioral pins for the sibling floor copies) is optional; if the orchestrator wants it, it is a new numbered plan with its own mutant rows.
- Orchestrator items: (1) `origin/main` is `a0c0a38`; `6dd7859`, `b510f97`, this SUMMARY and the STATE and ROADMAP updates are local and unpushed; (2) `requirements.ready-ids` decides whether VER-06 can be checked off; (3) the scratch clones and sweep trees under `build/` may be pruned; (4) PIN-IMPACT.md "Gate-growth ledger" now has Wave 0 and Wave 1 rows and the next row is plan 10-13's.

---
*Phase: 10-carry-in-closure-before-the-freeze*
*Completed: 2026-11-24*

## Self-Check: PASSED

`10-REVIEW.md`, `PIN-IMPACT.md`, `docs/ci-reviews/LOG.md` and this file exist; the four task commits (`bd37f85`, `a0c0a38`, `6dd7859`, `b510f97`) are ancestors of HEAD and `a0c0a38` is an ancestor of `origin/main`; `10-REVIEW.md` has the "## Wave 1" section with the 40-hex head, `[gate] PASS`, the run id and `success`; the Wave 2 base line is one 40-hex literal in a commit that changes only PIN-IMPACT.md; `grep -c "^| F-121 "` is 1; the Task 3 verify commands passed.
