---
phase: 10-carry-in-closure-before-the-freeze
plan: 13
subsystem: ci
tags: [review, scratch-clone, wave-2, vid-07, core-06, identity-bump, completeness-audit, single-bump, differential, mutants, push, hosted-ci, ci-review-log, gate-growth-ledger]

requires:
  - phase: 10-carry-in-closure-before-the-freeze
    provides: plans 10-09 to 10-12 (the four si-v2 ids, depth on descriptor 13, the t64.state.si.26 walker, state 26 export with container version word 2, the state 26 importer, 12 new CTest names and 10 new mutants) as the Wave 2 diff under review; the Wave 2 base 6dd7859 (plan 10-08); the clone-detached-at-the-head method (plan 10-05, W0-01); the preflight and mutant driver (plan 10-01)
provides:
  - 10-REVIEW.md Wave 2 section (scratch gate, ASan selection of all 24 Phase 10 names, sweeps from two bases, member audit, hand-derived sizes, single-bump check, pairing probe, D10 differential, 27-row mutant table, findings W2-01 to W2-07, push and hosted pair)
  - the third Phase 10 push (origin/main a0c0a38 to f57b019, 33 commits) after a passing 100 s preflight, and a green hosted pair (run 45530265365)
  - LOG.md Runs row and F-122, the Wave 2 warm sample and gate-growth measurement
  - PIN-IMPACT.md Gate-growth ledger Wave 2 row and the Wave 3 base 1cded8de34b0c2f5c5bea903095ffc7f1e55d6fc
affects: [10-14, 10-15, 10-17, 10-18, 10-19, phase-11]

actuals:
  tokens: 9642
  tasks: 3
  commits: 4

tech-stack:
  added: []
  patterns:
    - "a reviewer extracts the function text under review verbatim from the committed source and calls it beside the live function it claims to equal (6,000,000 packets plus exact boundary bisection), then perturbs the copy to prove the probe can fail"
    - "a completeness claim over a structure is checked by a byte-flip probe against the stream when the committed table does not cover that structure (VI state)"
    - "a pin-strength gap is found by adding throwaway mutants of the pinned constants to the scratch clone's driver table, committing them on a throwaway branch there and removing the branch"

key-files:
  created: []
  modified:
    - .planning/phases/10-carry-in-closure-before-the-freeze/10-REVIEW.md
    - .planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md
    - docs/ci-reviews/LOG.md

key-decisions:
  - "Wave 2 (plans 10-09 to 10-12, the one identity and state bump) is clean: the full gate passes in an isolated clone (248 of 248, 159 s), all 24 Phase 10 names pass under ASan with no sanitizer report, the shipped-id sweep from both the Wave 0 and the Wave 2 base is 1599 lines and 0 differences (the four v25 export lines included), the RDP state has 100 members and every one has a row, the VI state and both tables are complete by byte flip, exactly one tuple (13/21/26), one domain (t64.state.si.26) and one container version value (2) are new, and 25 mutants are KILLED with both controls as expected: no blocking finding"
  - "D10 (the written-out Z product bound in core/t64_state.c instead of refactoring the sealed preflight) is accepted: the copy equals the live function line by line, 6,000,000 random packets and 22,140 exact-boundary comparisons show 0 mismatches, the two early returns it omits are dead for an in-flight triangle, and no sealed body moved. The pin is weak though (W2-01): two perturbed bounds survive every committed test, so a boundary parity case and two mutants should follow as a new numbered plan"
  - "The membership-list observation of plan 10-09 is real but bounded (W2-02): the exact-tuple predicates are not generator tokens, so six site rows and three details are missing from docs/spec/profile-membership-sites.txt; this weakens discovery for a later family, not v26 behavior. scripts/list-membership-sites.py is not edited (Phase 9 bound path); a follow-up plan should add the two tokens and regenerate the list"
  - "The third hosted pair is green but is the slowest warm gate-macos sample so far (496 s, Gate 424 s): the steps no Wave 2 edit touches are 1.3 to 2 times slower in the same run, so the 12 new names (53.91 s macOS, 29.47 s Linux) cannot be separated from runner spread; the cold projection is 682 s (margin 218 s) and plan 10-17 should re-read it before the cold run of plan 10-19"
  - "The Wave 3 base 1cded8de34b0c2f5c5bea903095ffc7f1e55d6fc is recorded in a commit that changes only PIN-IMPACT.md (b51079a)"

patterns-established:
  - "A review of a bump that adds structure checks the completeness table against the struct by program, not by reading, and probes the structures the table does not name"
  - "A design decision the plan summary flags for the reviewer (D10) is judged against the live function with a differential, not against the author's two-packet parity case"

requirements-completed: [VID-07, CORE-06]

coverage:
  - id: D1
    description: "The Wave 2 head passes the full gate (sh scripts/gate.sh, default environment) and all 24 Phase 10 names under ASan in a clone detached at the reviewed head, outside the shared tree"
    requirement: "VID-07"
    verification:
      - kind: integration
        ref: "sh scripts/gate.sh in build/10-review/w2/src (clone detached at 96c8711, archive SHA-256 034384e9... equal to the main repository's): [gate] PASS, total 159s, ctest 248 of 248; cmake --preset asan, cmake --build --preset asan, ctest --preset asan -L phase10: 24 of 24 passed in 155.17 s, 0 sanitizer reports; ctest -N -L phase10 counts 24 = the PHASE10_TESTS block"
        status: pass
    human_judgment: false
  - id: D2
    description: "Shipped ids are byte identical across the whole bump: the digest sweep from the Wave 0 base and from the Wave 2 base to the head reports 0 differences over 1599 lines of the 21 shipped ids (four v25 export lines included), the four si-v2 ids appear as head-only lines (64 digest lines and one export line each, 296 head-only lines with their stop and skip lines), and K1 and the F2 pins pass"
    requirement: "VID-07"
    verification:
      - kind: other
        ref: "python3 -B scripts/digest-sweep.py --base WAVE0_BASE --head HEAD: 1599 shared lines, 0 differences, 0 head-only lines (also in the shared tree at f32a01f); with four --head-extra-id from WAVE0_BASE and from WAVE2_BASE: 296 head-only lines; ctest -R '^(unit.core.k1..*|harness.phase09-qualification|repo.si-membership-sites)$' 4 of 4"
        status: pass
    human_judgment: false
  - id: D3
    description: "The v26 completeness member list equals struct t64_rdp_state (100 members, 0 without a row, 0 rows naming no member, 3 observation exclusions, 19 tile fields times 8 tiles, size assert 5144), the VI state and both tables are complete by byte flip with the same inert set on all five family ids, and the v26 idle sizes are re-derived by hand from state-container.md (10,545,971, 10,546,563, 10,548,099, 10,578,819)"
    requirement: "CORE-06"
    verification:
      - kind: other
        ref: "program comparison of core/t64_internal.h against core/tests/test_digest_v26.c rows; byte-flip probe over 1,208 VI bytes on five ids (417 inert: observer cursors 16, packet identity strings 384, padding 17); python size calculation equal to the test literals"
        status: pass
    human_judgment: false
  - id: D4
    description: "Exactly one identity and state bump: one new tuple 13/21/26, one new digest domain t64.state.si.26, one new container version value 2 (T64_STATE_CONTAINER_VERSION_V2, v1 unchanged), no second family, version or domain"
    requirement: "CORE-06"
    verification:
      - kind: other
        ref: "git diff WAVE2_BASE HEAD on core/t64_internal.h: one added T64_STATE_CONTAINER_VERSION line, UINT32_C(2), no removed line; added version and domain literals listed in 10-REVIEW.md Wave 2 check (e)"
        status: pass
    human_judgment: false
  - id: D5
    description: "The T-10-52 version pairing at the reader: every wrong (version word, state word) pair refuses at 0x008 with the container version reason and leaves the machine unchanged, the frozen v25 container-version row still refuses version 2, a v26 blob rewritten to version 1 is refused"
    requirement: "CORE-06"
    verification:
      - kind: other
        ref: "core/tests/test_state_import.c row 'container version' (unit.core.state.import passes in the gate); unit.core.state.export-v26 pairing rows and mutant v26-reader-pairing-dropped KILLED; independent probe over 2 si-v1 and 4 si-v2 ids x 5 version words x 6 state words: 180 cases, 6 identity pairs imported, 144 pairing refusals at position 8, 0 bad"
        status: pass
    human_judgment: false
  - id: D6
    description: "The importer has no apply-before-validate path and every new member has a rule: stores go through STATE_STORE or state_rd_bytes (apply mode only), the hidden table closed-set check runs in the validate pass, the field-use audit is findings=0 with rows for depth_image_address and rdp_depth_hidden, and the other four scalars carry BOOL_01 or INDEX_LT with their own hostile rows"
    requirement: "CORE-06"
    verification:
      - kind: other
        ref: "read of git diff WAVE2_BASE HEAD -- core/t64_state.c; python3 -B scripts/check-state-field-use.py --root . --check: findings=0; docs/spec/state-mutants-v26.txt rows v26-depth-valid-two, v26-primitive-z-beyond-16-bits, v26-primitive-dz-beyond-16-bits, v26-primitive-valid-two"
        status: pass
    human_judgment: false
  - id: D7
    description: "All 25 mutants of the table are KILLED by a named check of their own test (a failed CHECK line after a passing baseline), the equivalent control SURVIVES and the non-compiling control is INVALID"
    requirement: "CORE-06"
    verification:
      - kind: other
        ref: "python3 -B scripts/p10-mutants.py --check-anchors (exit 0) and --run (exit 0, 117 s together) in the scratch clone; 10-REVIEW.md Wave 2 mutant table"
        status: pass
    human_judgment: false
  - id: D8
    description: "Whether the written-out Z product bound (design decision D10 of plan 10-12) is the right design, and whether the membership-list observation weakens the membership check"
    requirement: "CORE-06"
    verification:
      - kind: other
        ref: "6,000,000-packet differential and 22,140 boundary comparisons of the verbatim copy against t64_rdp_depth_triangle_preflight (0 mismatches; three perturbed copies detected); two probe mutants of the bound SURVIVED the committed tests; membership generator probed with the two tokens added (6 rows, 3 details missing)"
        status: pass
    human_judgment: true
    rationale: "Accepting a duplicated sealed arithmetic and classifying a discovery-aid gap as a warning are reviewer judgments; the mechanical evidence shows the copy is exact today and not pinned, and the list gap is a documented omission"
  - id: D9
    description: "The reviewed head is pushed by pinned SHA after a passing preflight and its hosted pair is green; the Runs row, F-122, the Wave 2 ledger row and the Wave 3 base are recorded"
    requirement: "VID-07"
    verification:
      - kind: integration
        ref: "sh scripts/preflight-phase10.sh PASS 100 s; run 45530265365 (ci.yml, push, head f57b019) completed success on both jobs; Task 3 verify commands pass (hosted completed success; grep -c '^| F-122 ' is 1; Wave 3 base one 40-hex line and a one-file recording commit)"
        status: pass
    human_judgment: false

duration: 31min
completed: 2026-11-25
status: complete
commits: 4
plan_head_before: 96c8711188a287756393c1871e7dda1e0a73f4af
plan_head_after: b51079a14cfd8458582901f087ec06454bd8c9b6
---

# Phase 10 Plan 13: Wave 2 review, push and hosted pair Summary

**Wave 2 (plans 10-09 to 10-12, the one identity and state bump, VID-07 and CORE-06) was reviewed independently in a clone detached at the head (full gate 248 of 248 in 159 s, all 24 Phase 10 names clean under ASan), the shipped ids are byte identical across the whole bump (sweep 1599 lines, 0 differences from two bases), every one of the 100 RDP state members has a digest row, exactly one tuple, domain and container version value are new, the version pairing refuses every wrong pair, 25 mutants are KILLED, and the head was pushed (33 commits, 100 s preflight) with a green hosted pair, run 45530265365; no blocking finding, two warnings (an unpinned Z product bound, a membership list that lost six sites), the Wave 2 ledger row, F-122 and the Wave 3 base are recorded.**

## Performance

- **Duration:** 31 min (2026-11-25T01:44Z to 02:16Z, of which the hosted run was 8 min)
- **Tasks:** 3 (Task 1 tracer, Tasks 2 and 3 auto)
- **Files:** 3 modified (`10-REVIEW.md` +293 lines, `PIN-IMPACT.md`, `docs/ci-reviews/LOG.md`); no source file touched
- **Host:** 1-minute load 12 to 15 during the runs (other sessions on the same machine), so local seconds are upper bounds

## Accomplishments

- **Task 1 (tracer, `f32a01f`).** The reviewed head `96c8711` was built and tested outside the shared tree in a local clone detached at the head plus a copy of `build/tools/install` (the W0-01 method; the archive SHA-256 of the clone equals the main repository's before and after the scratch edits). `sh scripts/gate.sh`: `[gate] PASS`, total 159 s, ctest 248 of 248 (98 s). The ASan preset built (357 steps) and `ctest --preset asan -L phase10` passed 24 of 24 in 155 s with no sanitizer report; the label count equals the `PHASE10_TESTS` block. The slowest sanitizer name is `import-v26-hostile` at 144.60 s against its 900 s TIMEOUT. First attempt passed.
- **Task 2 (`f57b019`).** (a) The digest sweep reports 1599 shared lines and 0 differences from the Wave 0 base and from the Wave 2 base to the head; with the four `--head-extra-id` ids each si-v2 id appears as 64 head-only digest lines and one export line (296 head-only lines with the stop and skip lines), which closes the W0-02 gap by reading lines instead of counts. (b) K1, the F2 pins and the membership test pass; `check-rsp-no-hle.py` passes with no re-pin and no sealed-body file or `core/include` file differs from the Wave 2 base. (c) The 100 members of `t64_rdp_state` were compared with the table rows by program (0 missing, 0 extra, 3 observation exclusions, 19 tile fields times 8), the VI state, which the test does not cover, was byte-flipped (417 inert bytes, all documented, identical on five ids). (d) All four v26 idle sizes were recomputed by hand from the document. (e) One tuple, one domain, one container value (2, v1 unchanged). Also: an independent 180-case probe of the version pairing, a read of the importer for apply-before-validate paths (none), the design decision D10 judged against the live preflight by differential (accepted), and `--check-anchors` and `--run` in the scratch clone (25 KILLED, both controls as expected). Findings W2-01 to W2-07.
- **Task 3 (`1cded8d`, then `b51079a`).** The preflight passed on the clean committed tree (100 s, 227 units compiled under the act image cc, 11 post-Gate steps replayed, PII clean on the tree and the 33-commit range). `origin/main` moved from `a0c0a38` to `f57b019` by a pinned-SHA fast-forward push. Run `45530265365` completed with success on both jobs with a Binutils cache hit on both. The Runs row and F-122 are in `LOG.md`; the PIN-IMPACT.md ledger has its Wave 2 row; the Wave 3 base is recorded in a commit that changes only that file.
- **Hosted after-timings (against F-121, F-120 and the F-119 band).** gate-macos run 496 s (Gate 424 s, ctest 260 s of 248 names, test-inventory-selftest 49 s, audio records 36 s) against 326 s (288, 187, 38, 21); gate-linux run 249 s (Gate 198 s, ctest 125 s) against 230 s (181, 115). The 12 new names add 53.91 s (macOS) and 29.47 s (Linux); `unit.core.state.import` starts 2nd again (216.18 s macOS, 103.59 s Linux). The macOS sample is the slowest of eight, but build, boot-evidence-selftest, core-lint and lint-selftest, which no Wave 2 edit touches, are 1.3 to 2 times slower in the same run, so growth and runner speed cannot be separated.

## Independent evidence beyond the plan (probes are kept uncommitted under the ignored `build/10-review/w2/` and the session scratch area)

| Probe | What it did | Result |
|-------|-------------|--------|
| Z product differential (D10) | the importer's `state_z_products_bounded` extracted verbatim from `core/t64_state.c`, called beside the live `t64_rdp_depth_triangle_preflight` on a state 26 machine | 6,000,000 packets over four Z opcodes (3,694,534 accepted, 2,305,466 refused): 0 mismatches; 4,000 bisected setups, 22,140 boundary comparisons: 0 mismatches; limit divisor 2, `abs_de >=` and `y_bound + 1` perturbations of the copy give 308,462, 476 and 2 random mismatches, so the probe can fail |
| pin strength of the copy | two throwaway mutants (limit divisor 2; `dy` divisor `y_bound + 1`) in the scratch clone's driver table | both SURVIVED the hostile and roundtrip cases: W2-01 |
| VI byte flip | every byte of `t64_vi_state` (1,208) flipped on the four si-v2 ids and `si-v1` | 417 inert on every id: observer cursors 16, packet identity strings 384, padding 17; the rest change the stream |
| version pairing | 5 version words x 6 state words x 6 ids, header patched, imported into a fresh machine | 180 cases, 6 identity pairs imported, 144 refused at 0x008 with the container version reason, 0 bad, digest unchanged by every refusal |
| membership generator | the generator imported with the two exact-tuple predicates added as tokens | 6 site rows and 3 details missing from the committed list: W2-02 |

## Task Commits

1. **Task 1: scratch gate and ASan selection of the Wave 2 head** - `f32a01f` (docs)
2. **Task 2: sweeps, audit, single-bump check, D10, mutant table, findings** - `f57b019` (docs); this is the head that was pushed and run
3. **Task 3: hosted pair, F-122, Wave 2 ledger row** - `1cded8d` (docs)
4. **Task 3: Wave 3 base record (only PIN-IMPACT.md)** - `b51079a` (docs)

**Plan metadata:** this SUMMARY and the STATE and ROADMAP updates follow in docs(10-13) commits. The pushed head `f57b019` differs from the gated tree `96c8711` only by `10-REVIEW.md`.

## Files Created/Modified

- `.planning/phases/10-carry-in-closure-before-the-freeze/10-REVIEW.md` - the Wave 2 section (scratch gate, ASan table, sweeps, audit, single-bump check, pairing, D10, mutant table, findings, push, hosted pair)
- `.planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md` - the Wave 2 ledger row and the `- Wave 3 base:` line
- `docs/ci-reviews/LOG.md` - one Runs row and F-122

## Decisions Made

See key-decisions. The ones later plans need: the Wave 3 base literal; W2-01 (a boundary parity case and two mutants for the Z product bound) and W2-02 (two generator tokens and a regenerated membership list) as follow-ups; and that the warm gate-macos sample of this wave is the highest so far.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Premise correction] The scratch tree is a clone, not a bare archive**
- **Found during:** Task 1
- **Issue:** the plan repeats the 10-05 recipe "git archive scratch clone", which cannot pass the gate (finding W0-01 of plan 10-05).
- **Fix:** a local clone detached at the head plus a copy of `build/tools/install`, as the orchestrator's method note and W0-01 prescribe; the archive of the clone and of the main repository hash the same, checked again after the scratch edits were reverted.
- **Files modified:** none (scratch only, under the ignored `build/10-review/`)
- **Verification:** `[gate] PASS`, 248 of 248
- **Commit:** `f32a01f` (record)

**2. [Rule 1 - Premise correction] Tasks 1 and 2 were committed before the push, and the mutant driver ran in the scratch clone**
- **Found during:** Tasks 2 and 3
- **Issue:** the plan lists the 10-REVIEW.md commit after the hosted run, and its verify command runs `p10-mutants.py --run` at the repository root; the task commit protocol requires a commit per task and the review rule keeps mutant runs out of the shared tree.
- **Fix:** Tasks 1 and 2 were committed first (the pushed head `f57b019` is a docs commit that differs from the gated tree `96c8711` only by `10-REVIEW.md`); `--check-anchors` and `--run` ran in the clone. The plan's literal sweep verify command also ran in the shared tree at `f32a01f` and gave 0 differences.
- **Commit:** `f32a01f`, `f57b019`

**3. [Rule 2 - Added check] Independent probes for the orchestrator's assessment items**
- **Found during:** Task 2
- **Issue:** items (1) D10, (3) T-10-52, (4) the membership observation and the VI part of check (c) asked for an assessment that the plan's commands do not give (the test file has no VI rows; the parity case has two packets).
- **Fix:** the four probes of the table above (Z product differential, VI byte flip, version pairing, membership generator) and two probe mutants in the scratch clone's driver table, committed on a throwaway branch of the clone and removed; nothing was added to the repository.
- **Commit:** `f57b019` (record)

**4. [Rule 1 - Added row] A finding row was added although the sample is mostly runner speed**
- **Found during:** Task 3
- **Issue:** the plan adds a finding row only if the hosted Gate or ctest grew by more than 30 s over F-120 beyond the F-119 spread; the Gate step grew by 126 s (ctest by 76 s) over F-120.
- **Fix:** F-122 records the per-step seconds and the decomposition (unchanged steps are slow too), and the ledger row says the new names cannot be separated from runner spread.
- **Commit:** `1cded8d`

**Total deviations:** 4 (2 Rule 1 premise corrections of the plan text, 1 Rule 2 added check, 1 Rule 1 added finding row). **Impact on plan:** none on scope or evidence; deviation 1 repeats the method correction of 10-05 and 10-08.

## Review findings (by severity)

| Severity | Count | Rows |
|----------|-------|------|
| blocking | 0 | none |
| warning | 2 | W2-01 the written-out Z product bound is unpinned (two perturbed bounds survive every committed test; a boundary parity case and two mutants should follow); W2-02 the membership list lost six site rows and three details because the exact-tuple predicates are not generator tokens (discovery aid only; `scripts/list-membership-sites.py` is a Phase 9 bound path and was not edited) |
| info | 5 | W2-03 the digest completeness file has no VI rows (probe shows the VI state is complete); W2-04 the runner refuses state export and import on any profile whose state version is not 25 and keeps the v1 format string until plan 10-15; W2-05 sanitizer TIMEOUT ratios (6.2 to 12.6 times); W2-06 the idle size literals re-derived from the section lengths; W2-07 the T-10-49 recorded exception is unchanged and stays flagged |

## Issues Encountered

- No vector failed and no defect was found in the bump: the Wave 2 head behaves as `state-container.md`, `machine-contract.md` and the plan summaries say. No blocking fix plan is requested.
- The first version of the Z product probe compared the unperturbed copy by mistake in its perturbation run (an include path preference of the quoted include); it was corrected before any result was recorded and all three perturbations were then run against the right source.
- The first VI probe reported 392 inert bytes beyond the observer cursors and the struct padding on every id; they are the `profile_id` and `build_id` strings of the two packet slots (384 bytes) and 8 bytes of padding inside the packet records, which the v25 framing leaves out of the stream by design (`!has_v25_framing` in `core/t64_digest.c`), so they are recorded as the documented inert set under W2-03, not as a gap.
- Background jobs in the shell showed "completed" when their wrapper exited while the inner job kept running; every result was read from the log files after the exit file existed. No evidence was lost.
- The hosted macOS run was 170 s slower than the Wave 1 sample (see F-122); nothing failed. The `build/10-review` clones (578 MB with their builds, three review waves) and `build/10-sweep` (101 MB) remain under the ignored `build/` for the orchestrator to prune.

## Authentication Gates

None. `gh` was authenticated for the push, the read-only run reads and the log download.

## User Setup Required

None - no external service configuration required.

## Known Stubs

None.

## Threat Flags

None new. T-10-30 mitigated (the member audit against the struct by program, the byte-flip probe of the VI state, the hand-derived sizes, the RED-first completeness test already in place and the mutant table rerun). T-10-31 mitigated (the preflight `pii` step passed on the tree and the 33-commit range before the push, the range held no binary, ROM-extension file, workstation path, e-mail address other than the public no-reply identity, key or token pattern, and the remote is the public szTheory repository; the probes stay uncommitted; the review text names no workstation path). No hosted cache was deleted.

## Next Phase Readiness

- Wave 3 starts at the base `1cded8de34b0c2f5c5bea903095ffc7f1e55d6fc` (`PIN-IMPACT.md` section 5); plan 10-14 (the composite fixture rows and the manifest commit) may begin. Every later Wave 2 check reads the Wave 2 base `6dd7859...` and the Wave 0 base, never HEAD.
- Orchestrator items: (1) `origin/main` is `f57b019`; `1cded8d`, `b51079a`, this SUMMARY and the STATE and ROADMAP updates are local and unpushed; (2) `requirements.ready-ids` still blocks VID-07 and CORE-06 (plans 10-15 to 10-19 declare them), so no requirement was checked off; (3) decide whether W2-01 (boundary parity case and two mutants, well under a second of test time) and W2-02 (two tokens and a regenerated list in one commit) become a new numbered plan before the Phase 11 freeze; (4) plan 10-17 re-reads the gate-macos total against the cold projection of 682 s (margin 218 s) before plan 10-19 measures it; (5) T-10-49 stays flagged for the 10-19 summary; (6) the scratch clones and sweep trees under `build/` may be pruned; (7) PIN-IMPACT.md "Gate-growth ledger" now has Wave 0 to Wave 2 rows and the next row is plan 10-18's.

---
*Phase: 10-carry-in-closure-before-the-freeze*
*Completed: 2026-11-25*

## Self-Check: PASSED

`10-REVIEW.md`, `PIN-IMPACT.md`, `docs/ci-reviews/LOG.md` and this file exist; the four task commits (`f32a01f`, `f57b019`, `1cded8d`, `b51079a`) are ancestors of HEAD and `f57b019` is an ancestor of `origin/main`; `10-REVIEW.md` has the "## Wave 2" section with the 40-hex head, `[gate] PASS`, the run id and `success`; the Wave 3 base line is one 40-hex literal in a commit that changes only PIN-IMPACT.md; `grep -c "^| F-122 "` is 1; the Task 3 verify commands passed; `git rev-list --count 96c8711188a287756393c1871e7dda1e0a73f4af..b51079a14cfd8458582901f087ec06454bd8c9b6` is 4.
