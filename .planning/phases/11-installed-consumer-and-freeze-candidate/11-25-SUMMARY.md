---
phase: 11-installed-consumer-and-freeze-candidate
plan: 25
subsystem: qualification
tags: [freeze-wave, final-audit, qualification, claims, evidence, hosted-pair, pinned-sha]
status: complete

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: the bound freeze-candidate record and four goldens (11-23), the Phase 11 qualifier with its hermetic test, the fresh-archive lane and a verified rehearsal (11-24), the SHAPE and INSTALL reviews (11-15, 11-21) and the defect fixes 11-26 to 11-29
provides:
  - 11-REVIEW.md "FINAL": the independent audit of the whole phase in a scratch clone (rows for VER-05, API-01 to API-04, SC1 to SC5 and every CONTEXT decision; api.md table against the tests; record and claim wording), zero blocking findings
  - the Qualification base 9b17d8106ce6d1479fd29587931c06136bae66c4 and the final native record 20261126T091014977323Z-aa393c22 (passed on the first attempt, --verify exit 0 after every later commit)
  - docs/claims/phase11.md (four unit-tested, authored-scope software-interface blocks for the installed-consumer suite) and docs/evidence/phase11-qualification.md
  - origin/main at dc247faaa3f5ceda17870c5e66b7ec181c767637 with a green warm hosted pair (run 45792136442) and no tag; LOG.md F-135 and the FREEZE row of the Gate-growth ledger
affects: [phase-11-verification, 12]

actuals:
  tokens: 22671
  tasks: 4
  commits: 5

plan_head_before: 2c53513c0ddf2b1e31e0bbc261f7629ce2268658
plan_head_after: 519352d32bf5a1453dc652b063bc77e3422ae237

tech-stack:
  added: []
  patterns:
    - "a final audit reads the FREEZE wave that no wave review covered, plus every requirement, success criterion and CONTEXT decision, one row each with evidence and a verdict"
    - "a postcondition table is checked against the tests by a scratch reader (status named after a direct call) plus a hand read of the table-driven tests and the defensive branches it cannot reach"
    - "evidence documents excluded from the bound inputs are written after the record, and the record is re-verified after each later commit"

key-files:
  created:
    - docs/claims/phase11.md
    - docs/evidence/phase11-qualification.md
    - .planning/phases/11-installed-consumer-and-freeze-candidate/11-25-SUMMARY.md
  modified:
    - .planning/phases/11-installed-consumer-and-freeze-candidate/11-REVIEW.md
    - .planning/phases/11-installed-consumer-and-freeze-candidate/PIN-IMPACT.md
    - docs/ci-reviews/LOG.md

key-decisions:
  - "the qualifier's three stale derivation constants (installed, sanitize-shipped, sanitize-instrumented) stay unrefreshed by orchestrator ruling: every limit holds at least 3.4 times the final seconds and an edit would be a non-defect bound-input change"
  - "the claim build_id keeps the 0.0.0+ prefix of the untouched project(VERSION), as Phase 10; the package version 0.1.0 is the ABI candidate number, stated in the evidence document"
  - "the four claim blocks count registered CTest names only (3, 17, 4 and 1); the negative controls, the in-tree API tests and the local sanitizer and TSan lanes are named as supporting evidence and not counted"
  - "the cold gate-macos run with the final 329-name set is orchestrator-only after the push; the ledger row and F-135 carry the warm numbers and the explicit pending text, no projected cold figure"
  - "VER-05 and API-01 to API-04 are not marked complete in REQUIREMENTS.md: the orchestrator runs the phase verification after the cold run"

patterns-established:
  - "FINAL audit record form: scratch gate, sweep, freeze against a fresh install, authority, CI-file diff, claims, PII range/tree/history, tags, the bisect-window publication check against every hosted run head, COST/TIMEOUT audit, log-level installed run, table-vs-tests, wording, requirement/SC/decision rows, carry-in table, findings"

requirements-completed: []

coverage:
  - id: D1
    description: "Final independent audit in a scratch clone of 2c53513: gate 329/329, sweep 1599 shared lines and 0 differences against the Wave 0 base, check-abi.py freeze against a fresh install 0 findings, authority blocked (exit 0 and 2), no tag on a Phase 11 commit, PII clean over range, tree and history, no ci.yml/check-ci-yml.sh/gate.sh edit since the Wave 0 base, no hosted run head inside the DEV-11-01 window; rows for VER-05, API-01..04, SC1..SC5 and every CONTEXT decision"
    requirement: VER-05
    verification:
      - kind: command
        ref: "cd build/11-review/final/src && sh scripts/gate.sh ([gate] PASS, 329 of 329); python3 -B scripts/digest-sweep.py --base f9124ae9... --head HEAD (0 differences)"
        status: pass
    human_judgment: false
  - id: D2
    description: "Final native qualification from the recorded Qualification base: twelve stages passed on the first attempt (689.49 s), record verified, and verified again after the planning, documents and ledger commits"
    requirement: API-02
    verification:
      - kind: command
        ref: "python3 -B scripts/qualify-phase11.py --native (record 20261126T091014977323Z-aa393c22) then --verify RECORD exit 0 at aa393c2, 62ab601, dc247fa and 519352d"
        status: pass
    human_judgment: false
  - id: D3
    description: "Phase 11 claims (four unit-tested blocks, software-interface only, not_claimed covers compatibility, accuracy, hardware behavior, final ABI, release, tag, package channel and oracle comparison; runner-recorded pins never called independent) and the qualification evidence (bases, build ids, four golden hashes, stage table, lanes, hosted runs, gate growth, recorded exceptions for owner review)"
    requirement: API-03
    verification:
      - kind: command
        ref: "sh scripts/check-claims.sh (files=75 blocks=26 errors=0); grep -c -i -w -E 'accurate|compatible|supports' docs/claims/phase11.md (0); grep -c 'runner-recorded pin' (9 and 3); sh scripts/pii-scan.sh --tree (0 findings)"
        status: pass
    human_judgment: false
  - id: D4
    description: "Push of the qualified head at a pinned SHA with a green hosted pair and no tag; first hosted runs of the five FREEZE names on both operating systems"
    requirement: API-04
    verification:
      - kind: command
        ref: "git push origin dc247fa...:refs/heads/main (fast-forward from 4b13a90); run 45792136442 completed success (gate-macos 539 s, gate-linux 271 s, 329 of 329 on both); git tag --points-at prints nothing"
        status: pass
    human_judgment: false
  - id: D5
    description: "Whether the recorded exceptions (DEV-11-01, the G5 file and plan counts, the Phase 11 G4 exception, DR-07, INFO-03, DR-31, the accepted candidate limits and the five deferred carry-ins) are acceptable is an owner review; none is worded as an approval"
    verification: []
    human_judgment: true
    rationale: "the evidence document lists them for owner review; only the owner can accept them"

duration: 43min
completed: 2026-11-26
---

# Phase 11 Plan 25: Final audit, qualification, claims and evidence, hosted pair at a pinned SHA Summary

**The whole phase was audited independently in a scratch clone with zero blocking findings. Then the final native qualification (record `20261126T091014977323Z-aa393c22`, 12 stages, first attempt) verified from the recorded Qualification base `9b17d81`. The four software-interface claim blocks and the evidence document were committed. The qualified head `dc247faaa3f5ceda17870c5e66b7ec181c767637` is on `main` with a green warm hosted pair (run 45792136442: gate-macos 539 s, gate-linux 271 s) and no tag.**

## Performance

- **Duration:** about 43 min (08:57Z to 09:40Z), of which 690 s was the native qualification and 9 min the hosted pair
- **Started:** 2026-11-26T08:57Z
- **Completed:** 2026-11-26T09:40Z
- **Tasks:** 4 (Task 1 `tracer`, Tasks 2 to 4 `auto`)
- **Files:** 2 created (the claim and evidence documents) and 3 modified (11-REVIEW.md, PIN-IMPACT.md, LOG.md), plus this summary. No source, header, golden, bound input, `ci.yml` or `gate.sh` changed

## Accomplishments

- **Task 1 (tracer): final audit.** A detached `--no-hardlinks` clone of `2c53513` with `build/tools/install` copied. `git archive` SHA-256 is equal in the clone and the main tree. Results:
  - `sh scripts/gate.sh` PASS: 329 of 329, total 145 s, ctest 85 s.
  - Sweep: 1599 shared lines, 0 differences.
  - The fresh-archive lane, then `check-abi.py freeze` against its prefix: 0 findings.
  - Authority still blocked: exit 0 and 2. The checker diff is the one `PINNED_REQUEST_SHA256` line, and the bound files moved only in `e66625a`.
  - Claims: errors=0. PII: 0 findings over 168 commits, 1435 files and 1337 history commits.
  - No Phase 11 tag (the three Phase 9 tags are older than the Wave 0 base). Empty CI-file diff.
  - No hosted run head lies in the 12-commit DEV-11-01 window. Every pushed head descends from `e66625a`.
  - All 75 `phase11` names have COST and TIMEOUT.

  The api.md section 12 table agrees with the code and the tests: a scratch reader plus a hand read of the poison and struct-size tables and of three check orders. "## FINAL" has rows for VER-05, API-01 to API-04, SC1 to SC5 and 40 CONTEXT decisions. Findings: zero blocking, no warning, four info (FN-01 to FN-04). The tracer gate re-ran both verifies (the clone gate and the sweep): `⚡ Tracer verified end-to-end - expanding`.
- **Task 2: Qualification base and the final run.**
  - `- Qualification base: 9b17d8106ce6d1479fd29587931c06136bae66c4`, in a commit that changes only PIN-IMPACT.md. The base verify passes.
  - The run waited for the 1-minute load to fall below 8: unrelated system processes had raised it to 15.8.
  - `python3 -B scripts/qualify-phase11.py --native` passed on its first attempt: 689.49 s, load 7.11 at the start and 18.37 at the end.
  - Record identity: 188 inputs, 75 inventory names, core, installed archive and consumer build id `t64-build-v1-35e6e1f1...22d5`, the four golden hashes equal to the block.
  - `- Final record:` line, then `--verify` exit 0. The record id, stage table and attempts are in PIN-IMPACT "Freeze steps".
- **Task 3: claims and evidence.**
  - `docs/claims/phase11.md` has four `unit-tested` blocks bound to `0.0.0+t64-build-v1-35e6e1f1...22d5` and `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k`: composite and three-way 3/3, ops 17/17, host loop and two instances 4/4, sequence 1/1. Each block has a counting-unit paragraph, the statement rendered from the keys, and not_claimed covering compatibility, accuracy, hardware behavior, final ABI, release, tag, package channel and oracle comparison, plus a limits paragraph.
  - `docs/evidence/phase11-qualification.md` holds the audited source and head, the identity table (four bases, six build ids, ABI 0.1 and package 0.1.0, the record and its four golden hashes), the stage table, the local lanes, the final audit, the hosted runs of 11-15 to 11-21 and the cold run, gate growth against 900 s, 11 recorded exceptions for owner review (none an approval), the SC table, the claims index, the limits and the reproduction steps.
  - The DR-32 three-way scope is stated in both files. "runner-recorded pin" appears 9 times and 3 times.
  - After the commit: check-claims errors=0, PII clean, `--verify` exit 0.
- **Task 4: push and hosted pair.**
  - Full preflight PASS (81 s). `pii-scan --tree` and `--range origin/main..HEAD` (24 commits) and the rights scan are clean.
  - `git push origin dc247fa...:refs/heads/main`: a fast-forward from `4b13a90`, no force, no tag.
  - Run 45792136442 green on both jobs, warm, no rerun:
    - gate-macos 539 s (Gate 462 s, ctest 279 s).
    - gate-linux 271 s (Gate 220 s, ctest 144 s; cmake 3.31.6, gcc 13.3.0).
  - The five FREEZE names pass on their first hosted runs on both operating systems: 28.50 s on macOS and 7.28 s on Linux.
  - LOG.md has the Runs row and F-135. The PIN-IMPACT ledger has the FREEZE row: warm margin 361 s; Phase 11 growth +171 s of job and +85 s of ctest against the Phase 10 close.
  - The cold figure is left as "pending, orchestrator-only (to be recorded by the orchestrator after this plan)".

## Task Commits

1. **Task 1: final audit** - `9b17d81` (docs)
2. **Task 2: Qualification base** - `aa393c2` (docs, PIN-IMPACT only); **final record id, stages and attempts** - `62ab601` (docs)
3. **Task 3: claims and evidence** - `dc247fa` (docs; the pushed head)
4. **Task 4: Runs row, F-135 and the FREEZE ledger row** - `519352d` (docs, `[skip ci]`, local, not pushed)

`commits: 5` in the frontmatter is `git rev-list --count 2c53513..519352d`, measured from the persisted ledger. The summary, STATE and ROADMAP commit follows it.

## Verification

| Check | Result |
|-------|--------|
| Task 1 verify (clone `sh scripts/gate.sh`; sweep with the Wave 0 base idiom) | `[gate] PASS` 329/329; `sweep: 1599 shared lines, 0 differences, 0 head-only lines` |
| Task 2 verify (base idiom and an empty non-planning diff; `--native`, then `--verify` of the `- Final record:` path) | base ok; native record written, verify exit 0 |
| Task 3 verify (`check-claims.sh`, `pii-scan.sh --tree`, `--verify`) | `claims: files=75 blocks=26 errors=0`; 0 findings; record verified against the current tree |
| Task 3 acceptance | reserved-word grep 0 on the claim file; the evidence names the record id and the four golden hashes; `runner-recorded pin` 9 and 3; no file calls the tick, stop reason, digest or packet count independent |
| Task 4 verify (origin/main 40-hex, completed success, no tag) | `dc247faaa3f5ceda17870c5e66b7ec181c767637 completed success`, no tag |
| Record re-verify after `519352d` | exit 0 (LOG.md and `.planning/` are excluded inputs) |

## Deviations from Plan

### Auto-fixed Issues

None.

### Plan text read, not deviations

- Task 1 ran the sweep inside the clone at `2c53513`, the reviewed head. The plan's automated sweep line runs in the shared tree at HEAD, which at that point was `2c53513` plus one planning commit with the same bound source. The shared-tree sweep also ran inside the final record (stage `sweep`, 0 differences at `aa393c2`).
- The `- Final record:` line and the Task 2 rows went into a new "### 11-25" subsection of PIN-IMPACT "Freeze steps" in two planning commits: the line was written before `--verify` and committed with the record rows.
- Requirements were not marked complete, by orchestrator ruling. The phase verification follows the orchestrator's cold run.

---

**Total deviations:** 0 auto-fixed.
**Impact on plan:** none; no bound input changed after the Qualification base.

## Issues Encountered

- Host load from unrelated system processes (media analysis, file indexing, a VM) reached 15.8 before the qualification. The run waited for it to fall below 8, and the run then passed on its first attempt. No limit was widened.
- One background `gh` poll failed on a transient connection reset. The run's status was read again and was complete and green. No rerun was needed.

## Authentication Gates

None.

## Known Stubs

None.

## Threat Flags

None. T-11-56: check-claims errors=0, every block has a not_claimed list and the reserved-word grep prints 0. T-11-57: PII scans of the tree, the range and the history before the push are clean; raw logs stay under `build/gate/11-qualification-private`. T-11-58: the "Recorded exceptions for owner review" section says that none of the items is an owner approval.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- **Orchestrator-only, next:** the cold gate-macos run with the final 329-name set. Delete the macOS Binutils cache entry, then run `gh workflow run ci.yml --ref main` at `dc247fa`. Record it in its own LOG row or finding and in the FREEZE ledger row, which now reads "pending, orchestrator-only".
- **Orchestrator:** the Phase 11 verification. If it passes, mark VER-05 and API-01 to API-04 complete.
- `519352d` (LOG.md and PIN-IMPACT, `[skip ci]`) and this summary commit are local and unpushed; they go out with the next push.
- Phase 12 carries the final-bind text items FN-02 and FN-03, plus S-05 (reserved members) and I-05 (the bus-read span) as open amendment questions.

---
*Phase: 11-installed-consumer-and-freeze-candidate*
*Completed: 2026-11-26*

## Self-Check: PASSED

Created files exist (`docs/claims/phase11.md`, `docs/evidence/phase11-qualification.md`, this summary); commits `9b17d81`, `aa393c2`, `62ab601`, `dc247fa` and `519352d` are ancestors of HEAD and `dc247fa` is `origin/main`; `sh scripts/pii-scan.sh --tree` reports 0 findings; the final record verifies at `519352d`.
