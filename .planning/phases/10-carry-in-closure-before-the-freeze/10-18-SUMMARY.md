---
phase: 10-carry-in-closure-before-the-freeze
plan: 18
subsystem: review
tags: [wave-3-review, final-audit, scratch-clone, sanitizer-selection, digest-sweep, mutants, hosted-pair, gate-growth-ledger, qualification-base]

requires:
  - phase: 10-carry-in-closure-before-the-freeze
    provides: the composite guest, rows and two-run evidence (plans 10-14, 10-15), the provisional-name sweep and Phase 9 triage (10-16), the qualifier and its rehearsal (10-17), the wave bases, the mutant driver, the digest sweep and the preflight (10-01, 10-02)
provides:
  - 10-REVIEW.md "## Wave 3" (scratch gate 254 of 254, ASan phase10 30 of 30, composite evidence independence, unrelaxed raw-capture exclusion, findings W3-01 to W3-07) and "## Final audit" (re-run commands, rows for SC1-SC5, six requirements, every CONTEXT decision and every plan prohibition) and the hosted-pair section
  - the audited head f081d39 pushed to origin/main with a green hosted pair (run 45548987145) and its LOG.md Runs row and finding F-123
  - the Gate-growth ledger Waves 3 and 4 row (measured hosted seconds beside the 10-17 projection)
  - "- Qualification base: ad39b6be2f803af45af15d4ed511b72a89460339" in PIN-IMPACT.md "Wave bases", in a commit that changes only that file
affects: [10-19, phase-11]

plan_head_before: 856b1e917832da68cba42f6d0319b0016332bc2d
plan_head_after: 38607173f993cf06ffdde661606596fdbb79ebe8

actuals:
  tokens: 14795
  tasks: 4
  commits: 6

tech-stack:
  added: []
  patterns:
    - "a final review runs in a clone detached at the head with a copied toolchain, the clone archive hash equal to the head archive hash before and after every run, and every long re-run (sweep, mutants, ASan) in the clone, never in the shared tree"
    - "an audit matrix row is either a command re-run now or a recorded disposition cross-checked against the tree, and the one pending row names its owner"
    - "a wrong-value control on a failing run reuses the read-back record of the device and says so, because a failing run publishes no artifact"

key-files:
  created:
    - .planning/phases/10-carry-in-closure-before-the-freeze/10-18-SUMMARY.md
  modified:
    - .planning/phases/10-carry-in-closure-before-the-freeze/10-REVIEW.md
    - .planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md
    - docs/ci-reviews/LOG.md
    - scripts/qualify-phase10.py

key-decisions:
  - "No blocking finding in Wave 3 or the final audit: the gate passes 254 of 254 and the ASan selection 30 of 30 in a clone detached at the head, the sweep reports 0 differences over the 21 shipped ids from the Wave 0 base, 25 mutants are KILLED with both controls as expected, and the authority state is unchanged (--require-approved exits 2, trusted pins None)"
  - "Plan 10-15 deviations accepted: the one literal of runner/tests/test_state_runner.c (v2 to v3, case and inventory unchanged, no pin hashes the file) and the wrong-ROM save comparison on the read-back record (a failing run publishes no save; the transcript is compared whole against the oracle)"
  - "The mutants stage derivation sentence of scripts/qualify-phase10.py now says 25 mutants and 2 controls (commit 4dd936a, text only), committed before the scratch audit so that the audited head and the qualification base follow the last source edit"
  - "The audited head f081d39 is pushed (origin/main f57b019 to f081d39, 29 commits) and its hosted pair run 45548987145 is green: gate-macos 385 s (Gate 337 s, ctest 196 s of 254), gate-linux 233 s; warm plus the 186 s Binutils build delta projects a cold 571 s, margin 329 s, below the 10-17 projection of 696 to 735 s"
  - "The Qualification base is ad39b6be2f803af45af15d4ed511b72a89460339, recorded before this summary (the earlier waves' order) in the planning-only commit 3860717; the summary, STATE and ROADMAP commits that follow change only .planning/"
  - "Carried items now count five against guardrail G3 (W2-01, W2-02, DEF-W1-01, W1-03 and D-10-17-01): no further gap can be carried into Phase 11 without closing one"

patterns-established:
  - "The reviewer's own source edit (one sentence) goes in before the audited head is fixed, and every later commit of the plan is planning-only, so the tested tree is the pushed tree"

requirements-completed: [GATE-02, GATE-01, VER-06, VID-07, CORE-06, RUN-07]

coverage:
  - id: D1
    description: "The final head passes the full gate (254 of 254) and the sanitizer selection ctest -L phase10 (30 of 30, 0 reports) in a clone detached at the head whose archive hash equals the head archive hash"
    requirement: "GATE-01"
    verification:
      - kind: integration
        ref: "sh scripts/gate.sh (123 s, [gate] PASS) and cmake --preset asan, cmake --build --preset asan, ctest --preset asan -L phase10 (128.29 s) in build/10-review/final/src"
        status: pass
    human_judgment: false
  - id: D2
    description: "RUN-07 evidence is independent and the raw-capture exclusion is unchanged: the committed expectation equals the stdlib-only oracle, one manifest-moving commit, zero changed runner lines mention raw capture, and every combination of the raw directory with the pair, save-out, state export and replay is refused at tick 0 next to an accepted baseline"
    requirement: "RUN-07"
    verification:
      - kind: integration
        ref: "python3 -B fixtures/composite/composite_oracle.py --check fixtures/composite/composite.expected.json; git log --format=%H WAVE0_BASE..HEAD -- fixtures/manifest.json; manual supervised runs (baseline raw and save-out runs pass at tick 599,357 with digest 97f27027...1267c, eight refused combinations exit 2 usage)"
        status: pass
    human_judgment: false
  - id: D3
    description: "The final audit matrix maps SC1-SC5, the six requirements, every CONTEXT decision and every plan prohibition to a command re-run now or a recorded disposition; only the SC5 cold run is pending"
    requirement: "GATE-02"
    verification:
      - kind: other
        ref: "python3 -B scripts/digest-sweep.py --base WAVE0_BASE --head HEAD (1599 shared lines, 0 differences; 296 head-only lines with the four si-v2 ids); python3 -B scripts/p10-mutants.py --run (25 KILLED, controls as expected); ctest subset of five names; check-rsp-no-hle PASS; --require-approved exit 2"
        status: pass
    human_judgment: true
    rationale: "The matrix rows that cite a recorded disposition rest on reading the earlier plans' records against the tree; the owner may want to read the T-10-49 exception and the G3 count before the qualification"
  - id: D4
    description: "The audited head is pushed by pinned SHA and its hosted pair is green; the hosted seconds are recorded against the 10-17 projection in LOG.md (row and F-123), 10-REVIEW.md and the Gate-growth ledger Waves 3 and 4 row"
    requirement: "GATE-01"
    verification:
      - kind: other
        ref: "gh run list --workflow ci.yml --branch main (headSha f081d39 completed success); run 45548987145 gate-macos 385 s, gate-linux 233 s"
        status: pass
    human_judgment: false
  - id: D5
    description: "The Qualification base is recorded as one 40-hex literal in a commit that changes only PIN-IMPACT.md"
    requirement: "GATE-01"
    verification:
      - kind: other
        ref: "git diff --name-only ad39b6be2f803af45af15d4ed511b72a89460339 HEAD equals the PIN-IMPACT.md path at the recording commit 3860717"
        status: pass
    human_judgment: false

duration: 27min
completed: 2026-11-25
status: complete
---

# Phase 10 Plan 18: Wave 3 review, final independent audit, push and the hosted pair Summary

**The reviewed head passes the full gate (254 of 254) and the 30-name ASan selection in a clone whose archive hash equals the head's, the digest sweep shows 0 differences over the 21 shipped ids, 25 mutants are killed, every audit row is evidenced except the SC5 cold run, and the audited head f081d39 is on origin/main with a green hosted pair (gate-macos 385 s warm against the 900 s limit) and the Qualification base recorded.**

## Performance

- **Duration:** 27 min (2026-11-25T04:27:49Z to 04:54:59Z), host 1-minute load 6 to 10.7 during the scratch runs, so every second is an upper bound
- **Tasks:** 4 (Task 1 tracer, Tasks 2 to 4 auto) plus one dispatch-directed text correction before Task 1
- **Files:** 4 touched (`10-REVIEW.md`, `PIN-IMPACT.md`, `docs/ci-reviews/LOG.md`, `scripts/qualify-phase10.py` by one line); 367 lines added, 2 deleted
- **New CTest names:** none

## Accomplishments

- **Task 1 (tracer).** `build/10-review/final/src` is a `git clone --no-checkout --no-hardlinks` detached at `4dd936a` with a copied `build/tools/install`; its `git archive` hash (`d2d95246...`) equals the head's before and after every run. `sh scripts/gate.sh` gives `[gate] PASS` in 123 s (254 of 254, ctest 72.73 s); `ctest --preset asan -L phase10` gives 30 of 30 in 128.29 s with 0 sanitizer reports. The task's verify is the task's own execution, so the tracer gate is that one passing run (`Tracer verified end-to-end`).
- **Task 2.** The composite expectation equals the stdlib-only oracle (2,737 bytes); `fixtures/manifest.json` moved in exactly one commit (`f027b05`) since both the Wave 0 and Wave 3 bases, after the source commit `419275d` that its rows name; no added or removed line of the supervisor or real runner diff mentions raw capture; by hand, six combinations of `--raw-capture-dir` with the pair, save-out, state export (v1 and v2) and replay exit 2 `harness-error` / `usage` at tick 0 next to an accepted raw baseline (pass at tick 599,357, digest `97f27027...1267c`, largest raw file 2,273 of 69,632 bytes). The two plan 10-15 deviations are assessed and accepted (W3-01, W3-02).
- **Task 3.** `## Final audit`: 15 commands re-run now (sweep 1599 shared lines and 0 differences, 296 head-only lines of the four si-v2 ids, 25 mutants KILLED with both controls as expected, K1 and F2 pins and the scanner selftest, the sealed-body guard, the authority state, closed and frozen files identical to the Wave 0 base) and rows for SC1 to SC5, the six requirements, 11 locked decisions, 10 autonomous decisions, 3 discretion items and 39 plan prohibitions. Only SC5's cold run is pending.
- **Task 4.** Preflight PASS in 82 s; `f081d392...:refs/heads/main` moved `origin/main` from `f57b019` (29 commits, no tag); run `45548987145` is green on both jobs with a Binutils cache hit and no cache change. Hosted seconds: gate-macos run 385 s (Gate 337 s, ctest 196 s of 254 names), gate-linux 233 s; the Waves 3 and 4 row of the Gate-growth ledger and finding F-123 record them, and the `Qualification base` line is recorded in its own commit.

## Task Commits

1. **Pre-task text correction (dispatch):** `4dd936a` (fix) the mutants stage derivation says 25 mutants and 2 controls
2. **Task 1:** scratch gate and ASan selection - `4a8cefa` (docs)
3. **Task 2:** Wave 3 checks and findings W3-01 to W3-06 - `f29aa79` (docs)
4. **Task 3:** final audit matrix, W3-07 and the PIN-IMPACT.md section 14 note - `f081d39` (docs)
5. **Task 4:** hosted pair, F-123 and the ledger row - `ad39b6b` (docs); the Qualification base - `3860717` (docs, PIN-IMPACT.md only)

**Plan metadata:** this SUMMARY and the STATE and ROADMAP updates follow in docs(10-18) commits (planning only).

## Files Created/Modified

- `.planning/phases/10-carry-in-closure-before-the-freeze/10-REVIEW.md` - "## Wave 3", "## Final audit" and the push and hosted-pair section
- `.planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md` - the Waves 3 and 4 ledger row, the section 14 correction note, the `Qualification base` line
- `docs/ci-reviews/LOG.md` - the Runs row for 45548987145 and finding F-123
- `scripts/qualify-phase10.py` - one derivation sentence (text only)

## Findings (none blocking)

| id | severity | finding | disposition |
|----|----------|---------|-------------|
| W3-01 | info | one literal of `runner/tests/test_state_runner.c` moved from the v2 to the v3 string; case, list, inventory and pins unchanged | accepted |
| W3-02 | info | wrong-ROM save comparison runs on the guest's read-back record, because a failing run publishes no save; the transcript is compared whole against the oracle | accepted |
| W3-03 | info | `PINNED_REQUEST_SHA256` is `e131cbe7...` unchanged from the Wave 1 base; it was `a4bb7774...` at the Wave 0 base (one recorded orchestrator rebind) | recorded |
| W3-04 | info | the mutants stage sentence said 26 mutants | fixed in `4dd936a` |
| W3-05 | info | oracle independence is in code and values, not authorship | recorded |
| W3-06 | info | D-10-17-01 still open; the shared tree stands at 2 and 2 attempts | recorded |
| W3-07 | info | T-10-49 (Phase 9 claim-binding comparator) re-checked, constants reproduce from the live manifest | recorded exception, flagged for owner review |

## Decisions Made

See `key-decisions` above. The two that matter for later plans: the derivation sentence was corrected before the audit (so the audited head and the qualification base follow the last source edit), and the carried-item count is at the G3 limit of five.

## Deviations from Plan

**1. [Dispatch directive - not a deviation rule] The mutants stage sentence was corrected before Task 1**
- **Found during:** orchestrator dispatch (10-17 "Known small inaccuracies")
- **Issue:** `scripts/qualify-phase10.py` (not in the plan's `files_modified`) said "26 mutants and 2 controls"; the driver lists 25 and 2.
- **Fix:** one sentence, committed first (`4dd936a`) so the clone audits the corrected tree and the qualification base follows the last source edit; `harness.phase10-qualification` passes (5.9 s); the historical rehearsal record in PIN-IMPACT.md section 14 keeps its original text and gains a correction note.
- **Verification:** `ctest -R '^harness\.phase10-qualification$'` 1 of 1; the clone gate ran 254 of 254 on the corrected tree.
- **Commit:** `4dd936a`

**2. [Rule 3 - Method] The scratch tree is a clone, not a bare `git archive`**
- **Found during:** Task 1 (W0-01 of the Wave 0 review, repeated in the dispatch)
- **Fix:** `git clone --no-checkout --no-hardlinks`, detached at the head, a copied `build/tools/install`, archive hash equal to the head's.
- **Commit:** `4a8cefa`

**3. [Order] The `Qualification base` was recorded before this SUMMARY**
- The earlier waves recorded their base before their summary; plan 10-19 accepts any `.planning/` and `docs/ci-reviews/LOG.md` change after the base, so the summary, STATE and ROADMAP commits that follow do not move it.

**4. [Interpretation] Tracer gate**
- The tracer's verify command is the task's own execution (the gate and the sanitizer selection), so there is no separate expansion step to hold back; the single passing run is the gate's evidence.

**Total deviations:** 4 (1 dispatch-directed text edit, 1 method, 1 ordering, 1 interpretation). **Impact on plan:** none on scope or evidence.

## Issues Encountered

- A few commands in my first attempts used unquoted shell here-documents that expanded `$W0:` and `$1`; they failed loudly and were re-run with quoted here-documents. No file was affected.
- `PINNED_REQUEST_SHA256` read at the Wave 0 base is `a4bb7774...`, so the plan's wording "unchanged" holds from the Wave 1 base on (W3-03).

## Authentication Gates

None. `gh` was already authenticated; the cache listing was read-only and no hosted cache was deleted.

## User Setup Required

None - no external service configuration required.

## Known Stubs

None.

## Threat Flags

None new. T-10-43 mitigated (supervisor and runner diffs read since the Wave 0 base plus six live refused invocations); T-10-44 mitigated (`--require-approved` exit 2 and trusted pins `None` re-checked); T-10-45 mitigated (preflight PII step before the push, a separate search of added lines, 0 findings). T-10-SC accepted (no package install).

## Next Phase Readiness

Plan 10-19 can start. For the orchestrator:

1. **Qualification base:** `ad39b6be2f803af45af15d4ed511b72a89460339` (PIN-IMPACT.md "Wave bases"); the planning-only recording commit is `3860717`.
2. **Unpushed:** `ad39b6b` and `3860717` (planning and LOG.md only) plus this plan's summary, STATE and ROADMAP commits. `origin/main` is `f081d39`, the audited tree with a green pair. The cold run can be dispatched on it or after a routine push of the planning commits; `git diff --name-only <base> <run head>` lists only `.planning/` and `docs/ci-reviews/LOG.md` either way.
3. **Cold run prerequisites:** both Binutils entries exist (`binutils-Linux-X64-69d0bfd4...` 31.10 MiB, `binutils-macOS-ARM64-69d0bfd4...` 5.03 MiB, created 2026-11-19); delete only the macOS entry. Warm gate-macos is 385 s; the cold projection is 571 s (margin 329 s); the G4 reversal rule (cold above 900 s) stays the stop-loss.
4. **Cold-boot counters (D-10-17-01):** `build/gate/06-05-cold-hello-attempt-NN` and `06-05-cold-startup-attempt-NN` stand at 2 each, so the qualification has room (the cap is 99); nothing was archived.
5. **G3:** carried items count five with D-10-17-01 (W2-01, W2-02, DEF-W1-01, W1-03, D-10-17-01): the limit is five, so Phase 11 planning must close one before carrying another.
6. **Owner review flags to repeat in 10-19:** T-10-49 (Phase 9 claim-binding comparator, delegated autonomy basis) and the G4 exception; VID-04 stays deferred and unclaimed.
7. `requirements.ready-ids` keeps all six requirements unchecked while plan 10-19 declares them.

---
*Phase: 10-carry-in-closure-before-the-freeze*
*Completed: 2026-11-25*

## Self-Check: PASSED

`10-REVIEW.md` (sections "## Wave 3", "## Final audit" and the hosted-pair section), `PIN-IMPACT.md` (ledger row, section 14 note, Qualification base line), `docs/ci-reviews/LOG.md` (Runs row and F-123) and `scripts/qualify-phase10.py` exist; `4dd936a`, `4a8cefa`, `f29aa79`, `f081d39`, `ad39b6b` and `3860717` are ancestors of HEAD; `git rev-list --count 856b1e917832da68cba42f6d0319b0016332bc2d..38607173f993cf06ffdde661606596fdbb79ebe8` is 6; the acceptance greps hold (one "## Wave 3", one "## Final audit", rows SC1 to SC5, one Waves 3 and 4 ledger row, one 40-hex `Qualification base` line, one Runs row for run 45548987145); the plan verify commands passed (scratch gate and ASan selection, the oracle check and the manifest and closed-record check, the sweep and the mutant run in the clone, the hosted-run check `completed success` for `origin/main`, and the base check that `git diff --name-only <base> HEAD` equals the PIN-IMPACT.md path at the recording commit).
