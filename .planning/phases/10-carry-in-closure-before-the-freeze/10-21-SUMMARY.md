---
phase: 10-carry-in-closure-before-the-freeze
plan: 21
subsystem: requalification
tags: [gap-closure, requalification, native-qualification, wave-20-review, scratch-clone, push, hosted-pair, evidence, claims, core-06]

requires:
  - phase: 10-carry-in-closure-before-the-freeze
    provides: the committed VI completeness rows, the VI omission mutant and the refreshed qualifier text (10-20), the superseded qualification record and its evidence and claim documents (10-19), the qualifier, the base extraction rule and the clone review recipe (10-17, 10-18)
provides:
  - the independent review of the plan 10-20 wave in a detached clone (gate 254 of 254, ASan run of the completeness case, 28 mutant rows) with checks (a) to (g) and findings R21-01 to R21-03 in 10-REVIEW.md
  - a new Qualification base 76451024e148073804e162841c3d0a3543bd2567 as the only record line of PIN-IMPACT.md section 5 (the plan 10-18 line relabelled historical)
  - one native qualification record 20261125T155456703622Z-1fc0b1da (qualified commit 1fc0b1d, first attempt, --verify exit 0) and PIN-IMPACT.md section 17
  - the verified head 85ba477 pushed to origin/main as a fast-forward by pinned SHA (26 commits, no tag), its hosted pair run 45650890027 green, LOG.md Runs row and finding F-126, and the Gate-growth ledger gap-closure row
  - the refreshed sanitized evidence document and claim file for the re-qualified tree, and the Phase 11 carry-ins line of ROADMAP.md with W1-03 and D-10-17-01
affects: [phase-10-verification, phase-11]

plan_head_before: 2a840a9885879b6306a51991fe7676249920605c
plan_head_after: 3a863e117d37d99bd494ee96350a917b62d8b610

actuals:
  # chars/4 over the added lines of the realized diff of the five task commits (this summary and the STATE and ROADMAP progress edits are not in it)
  tokens: 11553
  tasks: 4
  commits: 5

tech-stack:
  added: []
  patterns:
    - "a gap-closure review adds independent single-line probes of the walker (five member omissions and two exclusions made live) in the scratch clone, so the committed case is shown to see more than the one member its mutant removes"
    - "a record is superseded, not rewritten: the old record, its attempts and its evidence stay as a labelled history subsection beside the new record"
    - "the hosted pair of the pushed head is bound by head SHA (gh run list --commit) and the pushed SHA is a state file (build/gate/10-21-pushed-sha.txt), so a restart resumes at the wait"

key-files:
  created:
    - .planning/phases/10-carry-in-closure-before-the-freeze/10-21-SUMMARY.md
  modified:
    - .planning/phases/10-carry-in-closure-before-the-freeze/10-REVIEW.md
    - .planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md
    - docs/ci-reviews/LOG.md
    - docs/evidence/phase10-qualification.md
    - docs/claims/phase10.md
    - .planning/ROADMAP.md

key-decisions:
  - "The review found no blocking finding; the three info findings are recorded, none edits a bound source: R21-01 (the identity-strings exclusion sentence over-states build_id, wording only), R21-02 (the size pins are the LP64 layout, a mismatch fails the build), R21-03 (10-VALIDATION.md still carries the old mutant counts)."
  - "The new base was taken at the review commit and recorded in its own PIN-IMPACT-only commit; the qualified commit H2 is that recording commit and the pushed head H3 is H2 plus the planning-only section 17 commit, so both heads have the same bound source snapshot."
  - "The cold-cache hosted run (731 s of 900 s, F-124) was measured on the earlier audited tree and was not repeated: the gap closure added test time only (+1.46 s macOS, +2.83 s Linux for one existing case), so the evidence says the cold figure for H3 would be about 733 s and is a projection."
  - "W2-03 is recorded closed in the evidence document and not carried: the carried items stay at five (W2-01, W2-02, DEF-W1-01, W1-03, D-10-17-01), now all five listed in the ROADMAP Phase 11 carry-ins line."

patterns-established:
  - "Load discipline for a native qualification: a plain gate PASS first, then poll the 1-minute load to 8 or below (50 s here), then the qualifier; the start load is recorded even when slightly above 8 (9.10)."

requirements-completed: [CORE-06]

coverage:
  - id: D1
    description: "The plan 10-20 wave is independently reviewed in a clone detached at the reviewed head (archive hash equal before and after): gate 254 of 254, ASan run of unit.core.digest.v26-completeness, 28 mutant rows (26 KILLED, both controls as expected); 120 compiler members against 120 rows with exactly six named exclusions; 791, 400 and 17 over 1,208 bytes re-derived; seven extra walker probes caught"
    requirement: "CORE-06"
    verification:
      - kind: integration
        ref: "build/10-review/gap/src: sh scripts/gate.sh [gate] PASS 149 s (254 of 254); cmake --preset asan && cmake --build --preset asan && ctest --preset asan -R '^unit\\.core\\.digest\\.v26-completeness$' 1 of 1 passed (8.16 s, 14 s in all, 0 sanitizer reports); python3 -B scripts/p10-mutants.py --run 105 s, 26 KILLED, control-equivalent-depth-cap SURVIVED, control-no-compile INVALID; git archive hash 5e12a7ba... equal before and after; seven probe edits of core/t64_digest.c all fail the case (exit 8), the restored tree passes"
        status: pass
    human_judgment: false
  - id: D2
    description: "The new Qualification base is the one 40-hex record line of PIN-IMPACT.md section 5, recorded after the review in a PIN-IMPACT-only commit, with the plan 10-18 value kept as a historical line that read_bases does not match"
    requirement: "CORE-06"
    verification:
      - kind: other
        ref: "sed base extraction idiom yields one 40-hex literal (76451024...), git diff --name-only <base> HEAD equals the PIN-IMPACT.md path at the recording commit, git diff --name-only <reviewed head> <base> lists only .planning/ paths, grep -c '^- Qualification base: ' is 1"
        status: pass
    human_judgment: false
  - id: D3
    description: "The native qualification passed on the first attempt on the tree that contains the VI rows (record 20261125T155456703622Z-1fc0b1da, commit_sha 1fc0b1d, rehearsal false, complete, empty drift, new base bound) and --verify accepts it; no limit widened, the qualifier unchanged since the base"
    requirement: "CORE-06"
    verification:
      - kind: integration
        ref: "sh scripts/gate.sh [gate] PASS 139 s (254 of 254) at load 6.91; load wait 50 s to 7.54; python3 -B scripts/qualify-phase10.py --native exit 0: ci-scanner 2.98 s, inventory 1.10, oracle 0.22, gate 128.13, phase10-junit 7.02, asan 125.10, mutants 105.00, sweep 40.77, total 410.78 s; python3 -B scripts/qualify-phase10.py --verify <record> exit 0; git diff --quiet <base> HEAD -- scripts/qualify-phase10.py"
        status: pass
    human_judgment: false
  - id: D4
    description: "The verified head 85ba477 is on origin/main as a fast-forward (26 commits, no tag, no force) and the ci.yml push run of exactly that SHA completed with gate-macos and gate-linux success; the Runs row, finding F-126 and the Gate-growth ledger row are recorded"
    requirement: "CORE-06"
    verification:
      - kind: other
        ref: "preflight PASS 69 s, pii-scan --tree and --range findings=0, rights-scan tree=1316 history=40 findings=0; git push origin <sha>:refs/heads/main f081d39..85ba477; gh run list --workflow ci.yml --commit <sha> --event push: 45650890027 completed success; jobs gate-linux=success,gate-macos=success; grep -c 45650890027 docs/ci-reviews/LOG.md is 2; ledger row present"
        status: pass
    human_judgment: false
  - id: D5
    description: "The evidence document and claim file name the new record, the qualified commit H2 and the pushed head H3; claim block 2 names the VI rows and the VI omission mutant; every exception is kept (item 3 at 21 plans, item 8 with W2-03 closed, new item 9); VID-04 stays deferred and unclaimed; the Phase 11 carry-ins line lists W1-03 and D-10-17-01"
    requirement: "CORE-06"
    verification:
      - kind: other
        ref: "sh scripts/check-claims.sh --files docs/claims/phase10.md (claims: files=1 blocks=8 errors=0), sh scripts/check-docs.sh ok, sh scripts/pii-scan.sh --tree findings=0, the four Task 4 verify commands (mutant row, 28 rows and no 27 rows, record id in both documents, H2 and H3 in the evidence, closed Phase 9 records byte equal to the Wave 0 base)"
        status: pass
    human_judgment: false

duration: 33min
completed: 2026-11-25
status: complete
---

# Phase 10 Plan 21: Review, re-qualification, push and refreshed evidence Summary

**The plan 10-20 gap closure is independently reviewed (clone gate 254 of 254, 26 KILLED mutants, all 120 members of the VI state audited), the tree with the VI rows is natively qualified on the first attempt (record `20261125T155456703622Z-1fc0b1da`, 410.78 s, `--verify` exit 0), the verified head `85ba477` is on origin/main with a green hosted pair (gate-macos 368 s, gate-linux 254 s), and the evidence and claims now rest on that record.**

## Performance

- **Duration:** about 33 min (2026-11-25T15:41Z to 16:14Z), host 1-minute load 5 to 14 (the qualifier started at 9.10 after a 50 s wait for 8 or below)
- **Tasks:** 4 of 4 (Task 1 tracer, Tasks 2 to 4 auto); no checkpoint, no auth gate
- **Files:** 6 modified plus this summary; no source file, CMake file, inventory file or pin changed, no qualifier limit moved
- **Added lines of the five task commits:** 282 insertions and 75 deletions across the 6 files
- **New CTest names:** none (254 gate names, 30 `phase10`)

## Accomplishments

- **Task 1 (tracer): review, then the base.** Preflight PASS in 114 s on the clean tree. A `git clone --no-checkout --no-hardlinks` detached at `2a840a9` with a copied `build/tools/install` (archive hash `5e12a7ba...` equal before and after) ran `sh scripts/gate.sh` (`[gate] PASS`, 149 s, 254 of 254), the ASan run of `unit.core.digest.v26-completeness` (8.16 s, 0 sanitizer reports) and the full mutant run (105 s: 26 KILLED, the equivalent control SURVIVED, the non-compiling control INVALID, each as expected). Checks (a) to (f) of the plan plus (g): a scratch C program and script found 120 compiler members and 120 distinct rows (0 missing, 0 extra), six named exclusions, 791 member, 400 excluded and 17 padding bytes over 1,208 re-derived from `offsetof` and `sizeof` (padding runs 4 at 188, 4 at 300, 4 at 788, 5 at 1203), both exclusion reasons re-read against `core/t64_digest.c`, `core/t64_internal.h` and `core/t64_state.c`, the RED confirmed as a behavioral SURVIVED (be683bf before 0ed0d75), only `core/tests/test_digest_v26.c` changed under `core`, `runner` and `tools`, protected paths equal to the base, `limit_seconds` literals of the qualifier unchanged, and seven extra one-line probes of the walker (five omissions, two exclusions made live) all caught. Commit `7645102`. The base `76451024e148073804e162841c3d0a3543bd2567` was recorded in `1fc0b1d` (PIN-IMPACT-only); the plan 10-18 line is the historical line.
- **Task 2: the native re-qualification.** Origin/main status checked first (`f081d39`, run 45548987145 success, equal to `git rev-parse origin/main`); counters 10 and 10; plain `sh scripts/gate.sh` PASS in 139 s at load 6.91; a 50 s wait to load 7.54. `python3 -B scripts/qualify-phase10.py --native` passed on **the first attempt**: record `20261125T155456703622Z-1fc0b1da` (SHA-256 `1099e397...487fc`), `commit_sha` `1fc0b1d`, `rehearsal false`, complete, empty drift, base bound; stages ci-scanner 2.98 s, inventory 1.10, oracle 0.22, gate 128.13, phase10-junit 7.02, asan 125.10, mutants 105.00, sweep 40.77; total 410.78 s; load 9.10 at start, 7.28 at end. `--verify` exit 0. The four build ids equal the plan 10-19 record's. PIN-IMPACT.md section 17, commit `85ba477` (this is the pushed head H3).
- **Task 3: the push and the hosted pair.** Preflight PASS in 69 s, PII tree and range and the rights scan at 0 findings, no workstation path, secret, binary or protected material in the 26-commit range. `git push origin 85ba477...:refs/heads/main` moved origin/main `f081d39` to `85ba477` (fast-forward, no force, no tag; the remote still lists only the three Phase 9 tags). Run **45650890027** (`ci.yml`, event push, head `85ba477`) completed success on both jobs: gate-macos 368 s (Gate 315 s, ctest 194 s on 254 names), gate-linux 254 s (Gate 202 s, ctest 128 s), Binutils cache hit on both, no cache change. `unit.core.digest.v26-completeness` ran 3.13 s (macOS) and 3.85 s (Linux) against 1.67 s and 1.02 s before the VI rows; the ctest wall moved by -2 s and +2 s against F-123. Runs row, finding **F-126** and the ledger row recorded, commit `d6e2b48`.
- **Task 4: evidence, claims, ROADMAP.** The evidence document names the base, H2 and H3, the new record and stage table, 28 mutant rows with `v26-digest-omit-vi-phase-cycles`, the VI half of SC2 (120 rows, 417 inert of 1,208, five ids), the new warm pair, and the plan 10-19 record and its four attempts as a labelled history subsection; exception 3 counts 21 plans, exception 8 records W2-03 closed (not carried), new exception 9 records the re-qualification. The claim file names the new record and extends CORE-06 with the VI rows and the VI omission mutant (pass and total unchanged at 4 of 4). ROADMAP.md Phase 11 carry-ins now list W1-03 and D-10-17-01. Commit `3a863e1`.

## Task Commits

1. **Task 1: independent review of the 10-20 wave** - `7645102` (docs, `10-REVIEW.md`)
2. **Task 1: new Qualification base** - `1fc0b1d` (docs, `PIN-IMPACT.md` only)
3. **Task 2: native re-qualification record, section 17** - `85ba477` (docs, `PIN-IMPACT.md`; the pushed head)
4. **Task 3: hosted pair, F-126 and the ledger row** - `d6e2b48` (docs, `docs/ci-reviews/LOG.md`, `PIN-IMPACT.md`)
5. **Task 4: evidence, claims and the ROADMAP carry-ins line** - `3a863e1` (docs)

**Plan metadata:** this summary with the STATE, ROADMAP progress and REQUIREMENTS updates follows in one docs(10-21) commit.

## Published commits

`git log origin/main..HEAD` before the push listed 26 commits (the plan said about 16): 22 `docs` (7 `docs(10-19)`, 5 `docs(10)` (the verification report, the security and validation audits and the two gap-closure plans), 4 `docs(10-18)`, 3 `docs(10-20)`, 3 `docs(10-21)`: the review, the base and the section 17 record), 3 `test(10-20)` and 1 `chore(10-20)`. Files changed against origin/main: 19, of which 13 are `.planning/` records and 6 are `core/tests/test_digest_v26.c`, `scripts/p10-mutants.py`, `scripts/qualify-phase10.py`, `docs/ci-reviews/LOG.md`, `docs/claims/phase10.md` and `docs/evidence/phase10-qualification.md`. The two commits after the push (`d6e2b48`, `3a863e1`) and this plan's metadata commit stay local; the next push carries them.

## Review findings (Wave 20)

| id | severity | finding | disposition |
|----|----------|---------|-------------|
| R21-01 | info | `VI_IDENTITY_STRINGS_REASON` says the v25 and v26 families bind identity through the domain and the profile digest; true for `profile_id`, not for `build_id`, which the v25 state simply does not carry (the importer re-supplies it) | recorded; wording only in a bound test source, so not edited here; candidate one-sentence fix for the Phase 11 review, not a G3 gap |
| R21-02 | info | the size pins 1,208 and 488 and the 17 padding bytes are the LP64 layout; a mismatch fails the build | recorded; the hosted pair compiled and ran the case on macOS and Linux |
| R21-03 | info | `10-VALIDATION.md` lines 54, 143 and 157 still say "25 KILLED" and "27 rows" | orchestrator action item |

No blocking and no warning finding.

## Deviations from Plan

None of substance; the plan executed as written. Notes, none changing a result or a limit:

1. **First push attempt failed harmlessly (method).** `git push origin "$sha:refs/heads/main"` in the shell of this host expanded `$sha:r` (a history-style modifier) and git refused the refspec (`src refspec ...efs/heads/main does not match any`); nothing was pushed (origin/main stayed `f081d39`). The retry used `"${sha}:refs/heads/main"`, which printed the same pinned SHA and the expected `f081d39..85ba477`.
2. **No plan commit ledger file (protocol 0c).** The per-plan ledger file `gsd-plan-head-before-10-21` was not created before the first commit; `plan_head_before` is the reviewed head `2a840a9` that the plan itself wrote to `build/gate/10-21-reviewed-head.txt` before any commit, which is the same commit, and `commits: 5` is `git rev-list --count 2a840a9..3a863e1`.
3. **Extra review probes (check g).** Seven one-line probes of the walker in the scratch clone are beyond checks (a) to (f); they cost about one minute and strengthen the review; the clone was restored and its build passes.
4. **Qualifier started at load 9.10.** The load wait ended at 7.54 and the qualifier's own start sample was 9.10, slightly above the plan's 8; the record says so (`load_start`) and every second is an upper bound.
5. **Published 26 commits, not about 16.** The count at push time; every one is a reviewed planning, test or docs commit of the phase (the scans covered the whole range).

### Auto-fixed Issues

None. No auto-fix attempt was used.

## Authentication Gates

None. `gh` was already authenticated; the cache listing was read-only and no hosted cache was deleted or dispatched.

## Known Stubs

None. The probe programs live under `build/10-review/gap/probe` (ignored, not committed).

## Threat Flags

None new. T-10-57 (the base follows the review, the reviewed-head-to-base diff is `.planning/` only, the record binds the base and `--verify` ran before any document commit), T-10-58 (no qualifier change since the base, plain gate and load wait first, the first attempt passed so no failure had to be copied), T-10-59 (preflight PII step, tree and range scans and the rights scan before the push, repo-relative paths in the documents), T-10-60 (pinned SHA, fast-forward ancestor check, no force, no tag, no tag at the SHA) and T-10-61 (check-claims 8 blocks 0 errors, tier unchanged, VID-04 unclaimed, the cold run stated as not repeated) are mitigated as planned; T-10-SC accepted (no dependency added).

## Items for the orchestrator

1. **origin/main is `85ba477`** (the verified head H3, hosted pair green). Unpushed local commits: `d6e2b48`, `3a863e1` and this plan's metadata commit; the next push (phase close) carries them. `git diff --name-only 85ba477 HEAD` then lists planning records, `docs/ci-reviews/LOG.md`, `docs/evidence/phase10-qualification.md` and `docs/claims/phase10.md` only.
2. **Re-verify Phase 10.** `10-VERIFICATION.md` (`gaps_found`, SC2 VI half) can be re-run: the VI rows are committed, reviewed, qualified (record `20261125T155456703622Z-1fc0b1da`) and pushed.
3. **`--verify` cannot be repeated at a later head** (the evidence and claim documents are bound inputs newer than the base); it ran once at H2 before they were committed, as for plan 10-19.
4. **`10-VALIDATION.md` carries the old mutant counts** (R21-03): refresh at the phase close.
5. **The cold-cache run was not repeated.** The measured 731 s (F-124) is for the earlier audited tree; a cold run of H3 is orchestrator-only (cache deletion and dispatch) and is not needed by any criterion; the evidence says the cold figure for H3 would be about 733 s.
6. **Owner review flags kept in the evidence:** T-10-49 (Phase 9 claim-binding comparator), the G4 and G5 exceptions (G5 now 21 plans), the one orchestrator rebind of `PINNED_REQUEST_SHA256`, W3-01 and W3-02. VID-04 stays deferred and unclaimed.
7. **Carried items** stay at five (W2-01, W2-02, DEF-W1-01, W1-03, D-10-17-01); all five are in the ROADMAP Phase 11 carry-ins line now. The cold-boot counters (D-10-17-01) stand at 12 and 12 in the shared tree (cap 99, workaround threshold 90).
8. `requirements-completed: [CORE-06]` is declared here.

## Self-Check: PASSED

Files found: `10-REVIEW.md`, `PIN-IMPACT.md`, `docs/ci-reviews/LOG.md`, `docs/evidence/phase10-qualification.md`, `docs/claims/phase10.md`, `.planning/ROADMAP.md` and `build/gate/10-21-pushed-sha.txt` and `10-21-run-id.txt`. Commits `7645102`, `1fc0b1d`, `85ba477`, `d6e2b48` and `3a863e1` are ancestors of HEAD, and `85ba477` is an ancestor of `origin/main`.
