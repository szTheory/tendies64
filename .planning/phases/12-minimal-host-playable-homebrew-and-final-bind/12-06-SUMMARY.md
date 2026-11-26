---
phase: 12-minimal-host-playable-homebrew-and-final-bind
plan: 06
subsystem: review
tags: [review, scratch-clone, wave0-checklist, title, push, hosted-ci, gate-growth, host-base]

requires:
  - phase: 12-01
    provides: the Wave 0 base, scripts/preflight-phase12.sh and its regex table, PIN-IMPACT (decision record, ledger with the G4 exception and its reversal rule)
  - phase: 12-02
    provides: title-spec.md, the stdlib-only oracle, the input scripts and title.expected.json (harness.title-oracle)
  - phase: 12-03
    provides: the frozen title guest (7b96296), its builder and target, the two build-only manifest rows
  - phase: 12-04
    provides: the runner rows, runner.title-two-run and the two controls, the runner-recorded pins, DR-34
  - phase: 12-05
    provides: the three installed-consumer title cases, t64c_title.h, the consumer-recorded pins, the TITLE ledger stub
provides:
  - 12-REVIEW.md section "TITLE" (Tasks 1 to 3: scratch-clone gate and preflight, Wave 0 checks w1 to w5, checks (a) to (k), decision read, findings T-01 to T-09, push and hosted pair)
  - origin/main 884153ac143eb04f167a27aa57bdd7faf67605b7 (fast-forward from dc247fa, no tag), hosted run 45887012740 green on both jobs
  - docs/ci-reviews/LOG.md Runs rows 45795103375 (recovered) and 45887012740, finding F-137
  - PIN-IMPACT "Gate-growth ledger" TITLE row filled (hosted delta, final seconds, projected cold margin 128 s)
  - PIN-IMPACT "Wave bases" line "- Host base: 0836e40f583f8b47486ee93b0de36889e5626f0b"
affects: [12-07, 12-08, 12-12, 12-18]

actuals:
  tokens: 12200
  tasks: 3
  commits: 4
plan_head_before: 9ea70f769ca90f69206eeb1cf5db1660bfde9272
plan_head_after: 043f117cc10648c264f3b488f5721b09741abe20

tech-stack:
  added: []
  patterns:
    - "a review clone of the local repository gets the public fetch URL, a disabled push URL and a fetched origin/main before the preflight, so the pii range and head-ci steps see the real published head"
    - "an independent literal reader in a scratch file (python3 -I, no import of the checker) rebuilds the tone table and the save images from the spec text and the nonzero ranges before comparing the generated header"

key-files:
  created:
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-REVIEW.md
  modified:
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/PIN-IMPACT.md
    - docs/ci-reviews/LOG.md

key-decisions:
  - "Review result: zero blocking findings, zero warnings, nine info rows (T-01 to T-09); no new numbered plan is needed"
  - "Pushed 884153ac143eb04f167a27aa57bdd7faf67605b7 to main as a fast-forward from dc247fa (39 commits), no tag; hosted run 45887012740 green: gate-macos 582 s (ctest 323 s), gate-linux 263 s, 336 of 336 on both"
  - "Projected cold gate-macos 772 s (warm 582 s plus the F-136 cold cost of 190 s), margin 128 s: above the 120 s warning level, so the G4 reversal rule is not triggered; installed.consumer.title-persist (7.30 s hosted macOS) stays the first candidate and 12-12 is likely to cross the warning level with the HOST names (T-09)"
  - "Host base 0836e40f583f8b47486ee93b0de36889e5626f0b, recorded by a commit that changes only PIN-IMPACT.md"

patterns-established:
  - "Before closeout, a review plan reconciles the finding run ids of docs/ci-reviews/LOG.md with its Runs rows and appends any missing row from the completed run (here the F-136 cold dispatch)"

requirements-completed: []

coverage:
  - id: D1
    description: "The W0 and TITLE head passes the full gate and the preflight in an isolated clone"
    requirement: "API-06"
    verification:
      - kind: integration
        ref: "build/12-review/title/src: sh scripts/gate.sh PASS 336/336, total 236 s; sh scripts/preflight-phase12.sh --no-replay PASS 51 s"
        status: pass
    human_judgment: false
  - id: D2
    description: "Wave 0 checklist w1 to w5 and TITLE checks (a) to (k), each control read from its log"
    requirement: "API-06"
    verification:
      - kind: integration
        ref: "digest sweep 1599 shared lines 0 differences; ctest -R '^(harness\\.title-oracle|runner\\.title-two-run|controls\\.real\\.title-|installed\\.consumer\\.title)' 9 of 9; check-consumer-includes files=19 findings=0; rights-scan findings=0; own literal reader checks=81 failures=0"
        status: pass
    human_judgment: false
  - id: D3
    description: "Reviewed head pushed with no tag, hosted pair green, TITLE ledger row and Host base recorded"
    requirement: "API-06"
    verification:
      - kind: integration
        ref: "gh run 45887012740 completed success for origin/main 884153a, no tag; Host base verify (one 40-hex line, recording commit changes only PIN-IMPACT.md) exit 0"
        status: pass
    human_judgment: false

duration: 31min
completed: 2026-11-26
status: complete
---

# Phase 12 Plan 06: TITLE Review, Push and Host Base Summary

**Independent scratch-clone review of the W0 plan and the TITLE wave (gate 336/336, preflight, Wave 0 checks w1 to w5, checks (a) to (k) control by control, 0 digest differences) with no blocking finding, then a routine fast-forward push of 884153a whose hosted pair is green (gate-macos 582 s, gate-linux 263 s), the TITLE ledger row at a projected cold margin of 128 s and the Host base 0836e40.**

## Performance

- **Duration:** about 31 min
- **Started:** 2026-11-26T19:36:00Z
- **Completed:** 2026-11-26T20:07:00Z
- **Tasks:** 3 of 3
- **Files:** 1 created, 2 modified (records only: no production, test, fixture or CMake file)

## Accomplishments

- Task 1 (tracer): clone `build/12-review/title/src` detached at `9ea70f7` with a copied `build/tools/install` (git archive digests equal); `sh scripts/gate.sh` PASS 336/336 in 236 s (ctest 124 s) at load 7 to 17; `sh scripts/preflight-phase12.sh --no-replay` PASS in 51 s after the clone's `origin` was pointed at the public repository (T-05); 7 `phase12` names equal the block.
- Task 2: Wave 0 checks (w1 base parent and planning in the base, w2 preflight deltas, w3 23 count rows and owned zero rows, w4 every PIN-IMPACT section with 34 DR rows, w5 API-07 pin without a path) and checks (a) oracle before guest and stdlib-only, (b) scope cap 522 lines with no exception, TLB, Z, texture or RSP use and a polled VI frame lock, (c) build-only rows and no tracked ROM, (d) every runner control and both DR-34 exclusions read from the log, (e) the three consumer cases (99, 60 and 25 checks) and an independent literal reader (81 checks), (f) labels, (g) digest sweep 0 differences, (h) authority blocked, (i) no CI, gate, header or golden change, (j) PII range clean, (k) COST and TIMEOUT on all 7 names. Decision table for route A, scope cap, profile identity, builder copy, checkpoint digests, recorded pins, controls, runner scope, per-case ROM and G4 position.
- Task 3: shared-tree gate PASS (173 s), full preflight PASS (68 s, act Linux compile and post-gate replay), PII and rights scans clean; push `884153a` (fast-forward, no tag); hosted run 45887012740 green on both jobs; LOG Runs rows and F-137; TITLE ledger row; Host base.

## Task Commits

1. **Task 1: scratch-clone gate and preflight** - `bf88150` (docs)
2. **Task 2: Wave 0 checklist, checks (a) to (k), decision read and findings** - `884153a` (docs; the pushed head)
3. **Task 3: hosted pair, F-137, TITLE ledger row, review Task 3** - `0836e40` (docs); **Host base** - `043f117` (docs, PIN-IMPACT.md only)

**Plan metadata:** the commit of this summary with STATE and ROADMAP follows.

`commits: 4` is `git rev-list --count 9ea70f769ca90f69206eeb1cf5db1660bfde9272..043f117cc10648c264f3b488f5721b09741abe20`, read from the persisted ledger file of the plan.

## Findings (12-REVIEW.md "TITLE")

| id | severity | finding (short) | disposition |
|----|----------|-----------------|-------------|
| T-01 | info | `installed.consumer.title-persist` 2.55 s native against the 0.9 s projection (the export needs the whole long run) | accepted; first G4 reversal candidate |
| T-02 | info | per-case ROM mapping in two places (CMake `T64_CONSUMER_ROM_<case>`, sanitize-lane case statement, which also ignores `--rom` for the title cases); label by name prefix | accepted; a ROM column in `cases.txt` when another ROM-mapped case arrives |
| T-03 | info | `title-refused` uses a hand bound of 5,000,000 and a floor of 3 packets | accepted |
| T-04 | info | preflight copy carries one later delta (`-FA .`, 12-05 Rule 3) | accepted |
| T-05 | info | preflight in a local clone needs the public fetch URL and origin/main | procedure note for 12-12 and 12-18 |
| T-06 | info | this plan's second Task 2 verify counts 33 DR rows (DR-34 makes 34) | plan-text defect; satisfied with 34 |
| T-07 | info | pins file shape (runner top level, consumer nested) differs from the DR-07 wording | accepted; 12-08 reads the `consumer` keys |
| T-08 | info | `runner.title-two-run` is one run (DR-34) | accepted |
| T-09 | info | projected cold margin 128 s, 8 s above the 120 s warning level; HOST names likely cross it at 12-12 | no action now; 12-12 applies the reversal rule first |

## Deviations from Plan

### Auto-fixed Issues

None (the reviewer fixes nothing).

### Other deviations (recorded for transparency)

- **[Plan-text defect] Task 2 verify 2 counts 33 DR rows.** The must-have and the action say 34 (DR-34 amended them) but the automated body kept `-eq 33`; as written it exits 1, with 34 it exits 0. Recorded as T-06; no later plan counts DR rows.
- **[Rule 3 - Blocking, review environment] The clone preflight failed its pii step at first.** In a clone of the local repository `origin` is the source repository, so `--range origin/main..HEAD` scanned 0 commits (`P-ZERO-SCANNED`) and `head-ci` could not query GitHub. The scratch clone's fetch URL was set to the public repository (push URL disabled) and `+main:refs/remotes/origin/main` fetched; the rerun passed. No repository file changed (T-05).
- **[Policy reconciliation] A Runs row was added for run 45795103375.** The orchestrator's F-136 cold dispatch had a finding row but no Runs row; AUTONOMY section 2 asks to reconcile finding run ids with Runs rows before closeout, so `ci-review.sh 45795103375 --append` recovered it from the completed run without relaunching CI.
- **Commit messages.** The Task 1, Task 3 and Host base commits carry `[skip ci]` (records only); the Task 2 commit, which is the pushed head, does not, so the push triggered the hosted pair.
- `requirements-completed` is empty on purpose: API-06 also needs the minimal host (plans 12-07 to 12-09).

## Authentication Gates

None.

## Issues Encountered

- Unrelated host load (a virtual machine and a desktop application) rose from 6.8 to 17 during the scratch gate; the gate passed with no wall backstop and no limit widened.
- The tool shell is zsh; verify bodies were checked with `bash -n` and run with `bash` where needed.

## Verification Results

| Check | Result |
|-------|--------|
| clone `sh scripts/gate.sh` | PASS 336/336, total 236 s, ctest 124 s |
| clone `sh scripts/preflight-phase12.sh --no-replay` | PASS, 51 s (second attempt, T-05) |
| Task 2 verify 1 (base idiom, digest sweep) | `sweep: 1599 shared lines, 0 differences, 0 head-only lines`, 46 s |
| Task 2 verify 2 (w1, w4) | exit 1 as written (33), exit 0 with 34 (T-06) |
| Task 2 verify 3 | ctest 9 of 9 passed; consumer-includes files=19 findings=0; rights-scan findings=0 |
| shared-tree `sh scripts/gate.sh` at `884153a` | PASS 336/336, total 173 s, ctest 102 s |
| `sh scripts/preflight-phase12.sh` (full) | PASS, 68 s |
| `sh scripts/pii-scan.sh --tree`, `--range origin/main..HEAD`; `sh scripts/rights-scan.sh --tree --range origin/main..HEAD` | 0, 0 (39 commits), 0 findings |
| Task 3 verify 1 (hosted run of origin/main, no tag) | `completed success` for `884153ac143eb04f167a27aa57bdd7faf67605b7`, no tag |
| Task 3 verify 2 (Host base) | one 40-hex line, existing commit, recording commit changes only PIN-IMPACT.md |
| hosted run 45887012740 | gate-macos 582 s (Gate 508 s, ctest 323 s), gate-linux 263 s (Gate 222 s, ctest 149 s), 336 of 336 on both; TITLE names 24.34 s macOS, 17.65 s Linux |

## Known Stubs

None.

## Threat Flags

None. T-12-17 is mitigated by the full preflight pii step, `pii-scan --tree` and `--range` and the rights scan before the push (logs stay under `build/`); T-12-18 by the log-level reading of every control in check (d) and (e); T-12-19 by the cold projection against 900 s with the reversal rule (not triggered, margin 128 s).

## Next Phase Readiness

The HOST wave (plan 12-07) starts from the reviewed, hosted-green head; HOST_BASE is `0836e40f583f8b47486ee93b0de36889e5626f0b`. Notes for later plans: 12-08 takes the consumer-recorded pins from the nested `consumer` object of `fixtures/title/title.runner-pins.json` (T-07); 12-12 sets up its review clone as in T-05 and must expect the projected cold margin to fall under 120 s with the HOST names (T-09), in which case the reversal rule applies first (`installed.consumer.title-persist` and the selftests to a local lane, a new numbered plan). origin/main is `884153a`; the three local records commits after it (`0836e40`, `043f117` and the summary commit) go out with the next push a plan names (12-12). The final cold gate-macos measurement after the 12-18 push stays orchestrator-only.

## Self-Check: PASSED

- FOUND: .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-REVIEW.md ("## TITLE", Tasks 1 to 3), PIN-IMPACT "- Host base:" line and the filled TITLE ledger row, LOG.md rows 45795103375, 45887012740 and F-137
- FOUND commits: bf88150, 884153a, 0836e40, 043f117 (all ancestors of HEAD; 884153a equals origin/main)
