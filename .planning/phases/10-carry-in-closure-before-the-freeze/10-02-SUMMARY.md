---
phase: 10-carry-in-closure-before-the-freeze
plan: 02
subsystem: testing
tags: [k1, z-refusal, digest-sweep, mutants, ctest, pin-impact, c17, python]

requires:
  - phase: 10-carry-in-closure-before-the-freeze
    provides: plan 10-01 preflight, PIN-IMPACT.md, scripts/p10-mutants.py with both controls, the PHASE10_TESTS inventory block
  - phase: 09-controller-save-media-and-replay
    provides: the five shipped composite ids (ai-v1 and the four si-v1 ids), the v25 digest, scripts/k5-mutants.py
provides:
  - test_k1_refusal.c with unit.core.k1.dispatch-refusal (10 refusals) and unit.core.k1.apply-refusal (20 refusals), 30 literal machine digests, label phase10
  - the p10-mutants.py mutant k1-predicate-widened (depth predicate widened to descriptors 11 and 12), KILLED by both K1 cases
  - scripts/digest-sweep.py and tools/digest-sweep/t64_digest_sweep.c, the committed two-tree digest sweep with one forced build identity and a proven failing direction (--selftest)
  - PIN-IMPACT.md section 7 "Sweep baseline" (1599 shared lines for the 21 shipped ids)
affects: [10-03, 10-05, 10-08, 10-09, 10-11, 10-13, 10-18, phase-11]

actuals:
  tokens: 15658
  tasks: 4
  commits: 8

tech-stack:
  added: []
  patterns:
    - "characterization test stepped one RDP event at a time so dispatch acceptance and apply refusal are separate checks"
    - "two-tree comparison: git archive into scratch trees, one forced T64_BUILD_ID, library-only build, one printer program, sorted line-by-line compare"
    - "a selftest that can fail: identical trees give 0 differences, a seeded one-character digest-domain edit gives differences confined to one id, a symbolic base is refused with exit 2"

key-files:
  created:
    - core/tests/test_k1_refusal.c
    - scripts/digest-sweep.py
    - tools/digest-sweep/t64_digest_sweep.c
  modified:
    - core/CMakeLists.txt
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase10-regexes.txt
    - scripts/p10-mutants.py
    - .planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md

key-decisions:
  - "K1 pins Z refusal on all five shipped composite ids (ai-v1 and the four si-v1 ids) at both refusal paths: SetZImage and SetPrimDepth refused at dispatch (clock +1, buffer busy +1), the four Z triangles accepted at dispatch and refused at apply with no further counter change"
  - "The 30 K1 digest literals do not depend on the build identity (no packet is emitted), verified by a comment-only core edit that changed the build id while all literals still matched; no literal was dropped"
  - "The sweep keeps resource-limit stops as yields with the replay player's bound of 64 calls without progress, because the digest of a stalled machine depends on the number of advance calls (14 lines differed between a bound of 8 and 64)"
  - "--base accepts only a 40-hex lowercase commit id (exit 2 otherwise); --head-extra-id adds the four Phase 10 ids to the head list only"

patterns-established:
  - "Every sweep consumer extracts its base with the PIN-IMPACT base idiom and passes it to --base, never HEAD; the self-comparison in --selftest is the one documented exclusion"
  - "A new Phase 10 CTest name gets a PHASE10_TESTS entry, label phase10 and a count row in the same commit"

requirements-completed: [VID-07, CORE-06]

coverage:
  - id: D1
    description: "K1 characterization of Z refusal on the shipped descriptor 11 id and the four descriptor 12 ids: SetZImage and SetPrimDepth refused at dispatch, four Z triangles refused at apply, each with refusal reason, last opcode, status bits, both DPC counters, untouched depth members, an all-zero sidecar and a literal machine digest"
    requirement: "VID-07"
    verification:
      - kind: unit
        ref: "ctest --test-dir build/gate -R '^unit\\.core\\.k1\\.' (2 of 2 passed; refusals=10 and refusals=20 printed by the cases)"
        status: pass
    human_judgment: false
  - id: D2
    description: "The mutant 'depth predicate widened to descriptors 11 and 12' is killed by the K1 cases through the mutant driver; both cases fail on their own when run against the widened predicate"
    requirement: "VID-07"
    verification:
      - kind: integration
        ref: "python3 -B scripts/p10-mutants.py --run k1-predicate-widened --no-controls (KILLED; 75 and 40 CHECK lines in the dispatch and apply cases against the widened predicate)"
        status: pass
    human_judgment: false
  - id: D3
    description: "Committed two-tree digest sweep over the 21 shipped ids, up to nine ROMs each and ten checkpoints with one forced build identity, sorted by id, ROM and checkpoint"
    requirement: "CORE-06"
    verification:
      - kind: integration
        ref: "base=$(git rev-parse HEAD) && python3 -B scripts/digest-sweep.py --base \"$base\" --head HEAD (1599 shared lines, 0 differences, 0 head-only lines; 21 of 21 ids with digest lines; export lines for the four si-v1 ids)"
        status: pass
    human_judgment: false
  - id: D4
    description: "The sweep can fail: a seeded one-character edit of the state version 20 digest domain string reports differences confined to the rdp-blender-v6 id, identical trees report 0, and a symbolic base is refused with exit 2"
    requirement: "CORE-06"
    verification:
      - kind: integration
        ref: "python3 -B scripts/digest-sweep.py --selftest (exit 0: identical trees 0 differences, seeded edit detected with 64 differences, symbolic base refused)"
        status: pass
    human_judgment: false
  - id: D5
    description: "Pre-bump baseline recorded in PIN-IMPACT.md section 7 (commit, shared line count, per-id admitted ROM sets, seconds per run)"
    requirement: "CORE-06"
    verification:
      - kind: other
        ref: "grep -c '^## 7. Sweep baseline' .planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md (1); sh scripts/pii-scan.sh --tree (0 findings); sh scripts/preflight-phase10.sh (PASS, 83 s)"
        status: pass
    human_judgment: false

duration: 32min
completed: 2026-11-24
status: complete
commits: 8
plan_head_before: 9b88d3590a3cfed7324587b1ee7f1ae754c2b741
plan_head_after: cd99788d6cd9fb54eb41123e32fdea449512650b
---

# Phase 10 Plan 02: K1 Z-refusal characterization and two-tree digest sweep Summary

**Z refusal on the five shipped composite ids is pinned at dispatch and at apply by 30 literal-digest refusals before any identity edit, the widened-predicate mutant is killed, and a committed two-tree digest sweep (21 ids, 1599 lines, one forced build identity, proven failing direction) replaces the unrecorded 09-10 harness.**

## Performance

- **Duration:** 32 min
- **Started:** 2026-11-24T00:56:00Z (estimated from the first shell command; not recorded at start)
- **Completed:** 2026-11-24T01:28:00Z
- **Tasks:** 4
- **Files modified:** 8 (3 created, 5 modified)

## Accomplishments

- `core/tests/test_k1_refusal.c` pins, on `ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1` and the four `-si-v1` ids, that SetZImage (0x3e) and SetPrimDepth (0x2e) are refused at dispatch (10 refusals) and that the Z triangles 0x09, 0x0b, 0x0d and 0x0f pass dispatch and are refused at apply (20 refusals). Each refusal checks the refusal reason (`T64_RDP_REFUSAL_UNSUPPORTED_OPCODE`), last opcode field, last original word, status error and busy bits, both DPC counters (dispatch makes clock +1 and buffer busy +1, apply makes none), untouched depth members, an all-zero hidden depth table and a literal machine digest. CTest names `unit.core.k1.dispatch-refusal` and `unit.core.k1.apply-refusal`, label `phase10`.
- The mutant `k1-predicate-widened` (anchor: the `descriptor_version == 10u` conjunction of `t64_has_rdp_depth_profile`, replacement admits 10, 11 and 12) is KILLED by the driver. Run by hand against the same edit, the dispatch case reports 75 CHECK lines and the apply case 40, so each case catches it alone.
- `scripts/digest-sweep.py` and `tools/digest-sweep/t64_digest_sweep.c`: a two-tree sweep that extracts a 40-hex base and a head (ref or `WORKTREE`) with `git archive` into `build/10-sweep`, forces `set(T64_BUILD_ID "t64-build-v1-sweep")` in both scratch copies, builds only `t64_core` and `t64_sha256`, runs one printer against the same `build/fixtures`, sorts by id, ROM and checkpoint and compares. Baseline: 1599 shared lines, 0 differences (1460 digest lines, 115 stop lines, 20 skip lines, 4 export lines), 21 of 21 ids with digest lines, export lines for the four si-v1 ids.
- `--selftest` proves the failing direction: identical trees 0 differences, a seeded one-character change of the state version 20 domain string in the scratch copy reports 64 differences confined to `rdp-blender-v6`, and `--base HEAD` exits 2.
- PIN-IMPACT.md section 7 records the baseline (commit, line kinds, per-id coverage, seconds) without writing any `- Wave N base:` line.

## Task Commits

1. **Task 1: K1 tracer (SetZImage dispatch refusal on one id)** - `093fb27` (test)
2. **Task 2: K1 over five ids and both paths, 30 digest literals** - `639f06c` (test)
   **Task 2: mutant row k1-predicate-widened** - `687b628` (test)
3. **Task 3: sweep program and two-tree driver** - `ba5eb78` (feat)
4. **Task 4: --selftest** - `1ebd3d3` (test); **baseline record** - `2c0b014` (docs)
5. **Fix: yield bound 64** - `18e41b4` (fix); **baseline refresh** - `cd99788` (docs)

**Plan metadata:** the SUMMARY and the STATE, ROADMAP and REQUIREMENTS updates follow in a docs(10-02) commit.

## Files Created/Modified

- `core/tests/test_k1_refusal.c` - K1 cases `dispatch-refusal` and `apply-refusal` (copies command words from the bound `test_rdp_depth.c`, never edits it)
- `core/CMakeLists.txt` - `test_k1_refusal` target and the two CTest names (labels `unit;phase10`, IPL3 environment)
- `scripts/check-test-inventory.sh` - two names appended to `PHASE10_TESTS`
- `scripts/preflight-phase10-regexes.txt` - rows 1 (dispatch), 2 (`^unit\.core\.k1\.` prefix), 1 (apply)
- `scripts/p10-mutants.py` - `test_k1_refusal` source and the `k1-predicate-widened` row
- `tools/digest-sweep/t64_digest_sweep.c` - per-tree printer (public API only)
- `scripts/digest-sweep.py` - two-tree driver, coverage check, report, `--selftest`
- `.planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md` - section 7 "Sweep baseline"

## Decisions Made

See key-decisions. In short: K1 covers all five shipped composite ids, literal digests stay because they are build-identity independent (checked), the sweep mirrors the replay player's yield bound, and `--head-extra-id` is the hook for plans 10-09 onward.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Task 2 committed in two commits**
- **Found during:** Task 2 (the mutant run)
- **Issue:** `scripts/p10-mutants.py` builds its throwaway worktree from `HEAD` and refuses a modified tracked file outside `.planning/`, so the K1 test it kills the mutant with had to be committed before `--run`, and the mutant row before its first run.
- **Fix:** test and registration in `639f06c`, mutant row in `687b628`, then `--run` on the clean tree.
- **Verification:** `--run k1-predicate-widened --no-controls` KILLED; `--check-anchors` 0.
- **Committed in:** `639f06c`, `687b628`

**2. [Rule 1 - Bug] The sweep printer first treated a resource-limit stop as terminal, then used a yield bound of 8**
- **Found during:** Task 3 (first run: every ROM of most ids ended at about tick 3827), then after Task 4 (a check of the bound)
- **Issue:** a `RESOURCE_LIMIT` stop is a yield that the host answers with another advance call (the replay player rule). Ending there left most ids with 5 checkpoints; the bound of 8 differed from the player's 64 in 14 digest lines of stalled `rdp-fill` runs, because a stalled machine's digest depends on the number of advance calls.
- **Fix:** continue yields in the printer (`ba5eb78`), bound set to the player's 64 (`18e41b4`); the baseline record was refreshed (`cd99788`).
- **Verification:** `--selftest` PASS with the final program; 1599 shared lines, 0 differences.
- **Committed in:** `ba5eb78`, `18e41b4`

**3. [Rule 2 - Missing critical] Additions inside the files the plan names**
- **Found during:** Tasks 1 to 3
- **Issue/Fix:** (a) the dispatch case also pins 10 machine digests and the apply case is stepped one event at a time (dispatch accepted, then apply refused) with a `dpc_current` check, so that "no dispatch refusal" is an assertion and not an inference; (b) a count row for `^unit\.core\.k1\.dispatch-refusal$` stays beside the two rows the plan names; (c) the driver adds `--head-extra-id`, a coverage check (an id without an admitted ROM, or a si-v1 id without an export line, in the base tree exits 2), the SHA-256 of every candidate fixture in the report, and one generated authored 4 KiB candidate (`authored-4k`, the image `test_digest_v25.c` uses, admitted only by the skeleton id and refused with status 5 by the other 20); the eight fixture files alone already gave all 21 ids digest lines.
- **Files modified:** `core/tests/test_k1_refusal.c`, `scripts/preflight-phase10-regexes.txt`, `scripts/digest-sweep.py`, `tools/digest-sweep/t64_digest_sweep.c`
- **Verification:** all plan verify commands and acceptance criteria pass (see Self-Check)

---

**Total deviations:** 3 (1 Rule 3, 1 Rule 1, 1 Rule 2)
**Impact on plan:** all inside the files the plan names; no oracle-bound file, no production source and no shipped id's descriptor, digest or Z refusal changed.

## Issues Encountered

- Host load stayed between 7.9 and 10 (other sessions), so every timing is an upper bound; the printer took 38 s per tree with a yield bound of 8 and 41 to 60 s with 64.
- One stray `scripts/__pycache__` appeared from an in-process import (not run with `-B`); it was removed with `rm` before any commit.
- The comment-only build identity probe (an appended comment line in `core/t64_rdp.c`, reverted with `git checkout -- core/t64_rdp.c`) changed the build id and left all 30 literals valid; the widened-predicate RED probe edited `core/t64_internal.h` in the working tree and was reverted the same way; both were rebuilt clean afterwards.

## Authentication Gates

None.

## User Setup Required

None - no external service configuration required.

## Known Stubs

None.

## Threat Flags

None. T-10-03 is mitigated (the printer opens fixtures read-only, the driver writes only under `build/10-sweep` and `build/gate/10-sweep`, never `build/fixtures` or the main build tree); T-10-04 is mitigated (the equality claim is a committed, re-runnable two-tree tool whose failing direction is proven by `--selftest`).

## Next Phase Readiness

- Plans 10-05 and 10-08 can run `python3 -B scripts/digest-sweep.py --base "$base" --head HEAD` with the base extracted from PIN-IMPACT.md section 5; one pair run takes about one minute on this host. Plans 10-09 onward pass the four new ids with `--head-extra-id`. If plan 10-11 renames `t64_machine_state_export` or changes how the export size is queried, the printer's `export_hash` is the one place to update.
- K1 is in place before the first identity edit: plan 10-09 must keep `unit.core.k1.*` and the `k1-predicate-widened` mutant green when it widens `t64_has_rdp_depth_profile` with the v26 term outside the descriptor 10 conjunction.
- No blockers; no defect found in earlier plans.

---
*Phase: 10-carry-in-closure-before-the-freeze*
*Completed: 2026-11-24*

## Self-Check: PASSED

All eight created and modified files exist, the eight task commits (`093fb27`, `639f06c`, `687b628`, `ba5eb78`, `1ebd3d3`, `2c0b014`, `18e41b4`, `cd99788`) are ancestors of HEAD, and the plan-level checks passed on the committed tree: `ctest -N -R '^unit\.core\.k1\.'` 2 tests, `ctest --preset gate` 227 of 227 passed, `python3 -B scripts/p10-mutants.py --check-anchors` 0, `--run k1-predicate-widened --no-controls` KILLED, `python3 -B scripts/digest-sweep.py --selftest` exit 0, `--base HEAD`, `main` and a 12-hex prefix exit 2 with "base must be a 40-hex commit id", `sh scripts/check-test-inventory.sh --preset gate` (227 names) and `--selftest` PASS, `sh tools/lint/core-lint.sh` 0 violations, `sh scripts/pii-scan.sh --tree` 0 findings, `sh scripts/preflight-phase10.sh` PASS (83 s), and no file of the 14 oracle-bound paths or any other file under `core/` besides `core/CMakeLists.txt` and `core/tests/test_k1_refusal.c` is in the diff.
