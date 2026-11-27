---
phase: 12-minimal-host-playable-homebrew-and-final-bind
plan: 18
subsystem: qualification
tags: [final-audit, qualification-base, final-qualification, claims, evidence, owner-checklist, hosted-pair, gate-growth]

requires:
  - phase: 12-17
    provides: scripts/qualify-phase12.py and harness.phase12-qualification, the rehearsal, the pushed bind head 9f4e39a with a green hosted pair
  - phase: 12-14
    provides: the bound record at ABI 1.0 (package 1.0.0, bound_minor 0) and the four post-bind goldens
provides:
  - 12-REVIEW.md "FINAL": the final independent audit of 12-01 to 12-17 in a scratch clone (0 blocking, 0 warnings, 5 info; SC2 closed by "[host-sdl] PASS")
  - the Qualification base 656dc00 and the one final native qualification record 20261127T033336673294Z-40b7d2d9 (15 stages, first attempt, sdl-lane pass, sc2_virtual_joystick closed)
  - docs/claims/phase12.md (three unit-tested blocks), docs/evidence/phase12-qualification.md, docs/evidence/phase12-host-manual.md (owner checklist template, not run)
  - the qualified head f089171 pushed as a pinned-SHA fast-forward with a green hosted pair (run 45937541960), LOG Runs row and F-141, the FINAL warm ledger row
affects: [phase-12-verification, orchestrator-cold-measurement, v1.2-milestone-close]

actuals:
  tokens: 22917
  tasks: 4
  commits: 5
plan_head_before: 18636c8aa35870628e5f99d50e8ab350d623c680
plan_head_after: 75fe74ffb082e9241326f372f3e7b7441c931186

tech-stack:
  added: []
  patterns:
    - "the pushed head is the commit of the excluded evidence documents (a message without [skip ci], so the push triggers ci.yml); the records of its hosted run follow in a local [skip ci] commit of the excluded documents and planning files, after which the record still verifies"

key-files:
  created:
    - docs/claims/phase12.md
    - docs/evidence/phase12-qualification.md
    - docs/evidence/phase12-host-manual.md
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-18-SUMMARY.md
  modified:
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-REVIEW.md
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/PIN-IMPACT.md
    - docs/ci-reviews/LOG.md

key-decisions:
  - "The final native qualification ran exactly once (one final record); the Task 2 verify text that starts with --native was satisfied by that run and its --verify, not by a second native run that would have created a second final record"
  - "Claim block 3 counts the 12 repo.abi* and install.* names, each passed in the record's abi-cheap stage and again in its gate stage; the api04, major-rule and version-control names are supporting"
  - "The owner checklist keeps the plan's relaunch line and states that the shell keeps no host-side save file (DR-19), so a relaunch starts with an empty save slot; the build id field names the strings command because the shell prints no build id"
  - "The pushed SHA is the Task 3 documents commit f089171 (no [skip ci]); the Task 4 records commit 75fe74f and the SUMMARY commit stay local"

patterns-established:
  - "Final audit row for SC2 carries the audit's own lane line verbatim, and the verdict is derived from it"

requirements-completed: [API-05, API-06, API-07]

coverage:
  - id: A1
    description: "The whole phase is audited independently in a scratch clone before the final qualification: full gate, sweep 0 differences, bound record from a fresh install, oracle authority blocked, claims scan, SDL lane, PII range, and a row per requirement, success criterion, CONTEXT decision and deferred idea"
    requirement: "API-05"
    verification:
      - kind: integration
        ref: "clone build/12-review/final/src at 18636c8: sh scripts/gate.sh [gate] PASS 354/354 (216 s); digest-sweep 1599 shared lines 0 differences; check-install.sh fresh-archive then check-abi.py freeze findings=0 (abi 1.0, package 1.0.0); check-rdp-oracle-authority.py --expect-blocked exit 0; check-claims.sh errors=0; check-host.sh sdl [host-sdl] PASS (SDL 3.4.10); pii-scan --range W0..HEAD 87 commits 0 findings"
        status: pass
      - kind: judgment
        ref: "12-REVIEW.md FINAL: API-05, API-06, API-07 met; SC1 to SC4 closed (SC2 with the lane line [host-sdl] PASS); 27 CONTEXT decision rows implemented; 15 deferred ideas absent; 0 blocking, 0 warnings, FR-01 to FR-05 info"
        status: pass
    human_judgment: false
  - id: A2
    description: "The final native qualification runs after the Qualification base, passes every stage and verifies before and after the evidence documents' commit"
    requirement: "API-06"
    verification:
      - kind: integration
        ref: "python3 -B scripts/qualify-phase12.py --native at H 40b7d2d: exit 0, 15 stages one attempt each, 1304.01 s, record 20261127T033336673294Z-40b7d2d9 (sdl-lane pass, sc2_virtual_joystick closed); --verify RECORD exit 0 after the record commit, after the documents commit and after the records commit"
        status: pass
    human_judgment: false
  - id: A3
    description: "Phase 12 claims, evidence and the owner checklist template are written at the tier the evidence supports"
    requirement: "API-05"
    verification:
      - kind: unit
        ref: "sh scripts/check-claims.sh errors=0 (files=81 blocks=29; phase12.md blocks=3); reserved-word grep 0; 'not run' 1 and no PASS mark or date in the checklist; pin labels present in both documents; pii-scan --tree 0 findings; 31 doc-reader names pass"
        status: pass
      - kind: judgment
        ref: "the checklist is a template marked not run; no agent filled, attested or marked it"
        status: pass
    human_judgment: true
  - id: A4
    description: "The qualified head is pushed as a routine pinned-SHA fast-forward with a green hosted pair and no tag"
    requirement: "API-07"
    verification:
      - kind: integration
        ref: "git push origin f089171611d077fd2df8fe47f81ec61d0f0fb108:refs/heads/main (fast-forward from 9f4e39a); ci.yml run 45937541960 success: gate-macos 559 s, gate-linux 286 s, 354 of 354 names on both; git tag --points-at the SHA prints nothing; Task 4 verify printed rc 0"
        status: pass
    human_judgment: false

duration: about 52 min wall (03:21Z to 04:13Z; the final qualification 22 min of it, the hosted pair about 9 min)
completed: 2026-11-27
status: complete
---

# Phase 12 Plan 18: QUALIFY (final) Summary

**Phase 12 is audited, qualified and on main: the final independent audit in a scratch clone found 0 blocking findings and closed SC2 with its own `[host-sdl] PASS` line; the one final native qualification (record `20261127T033336673294Z-40b7d2d9`, 15 stages on the first attempt, SDL lane `pass`) binds the post-bind tree at ABI 1.0; three `unit-tested` claim blocks, the qualification evidence and the not-run owner checklist template are committed; and the qualified head `f089171` was pushed as a fast-forward with no tag and a green hosted pair (run 45937541960, gate-macos 559 s, gate-linux 286 s, 354 names).**

## Performance

- **Duration:** about 52 min wall (03:21Z to 04:13Z on 2026-11-27)
- **Tasks:** 4 of 4 (Task 1 is the tracer: its verify passed and expansion went ahead)
- **Files:** 3 new documents outside `.planning`, `docs/ci-reviews/LOG.md`, 12-REVIEW.md, PIN-IMPACT.md and this summary

## Accomplishments

- **Task 1 (tracer): final independent audit.** A `git clone --no-checkout` of the repository into `build/12-review/final/src`, detached at `18636c8` (the 12-17 closeout head; `git archive` SHA-256 equal in clone and main tree), fetch URL the public repository with push disabled (T-05), `build/tools/install` copied. In the clone: `sh scripts/gate.sh` `[gate] PASS` 354 of 354 (total 216 s, ctest 126 s, load 8.5 to 7.5); `digest-sweep.py --base 60b9e3c3...` 1599 shared lines, 0 differences (63 s); `check-install.sh fresh-archive` plus `check-abi.py freeze` against that prefix: `abi=1.0 package=1.0.0 symbols=35 layout=14/165/190 holes=8 statuses=39 stop_reasons=8`, 0 findings; oracle authority blocked (`--require-approved` exit 2); `check-claims.sh` errors=0; `check-host.sh sdl` nine lines ending `[host-sdl] PASS` (SDL 3.4.10); `pii-scan --range W0..HEAD` 87 commits, 0 findings; no `ci.yml`, `check-ci-yml.sh`, `gate.sh` or root CMake edit since W0; no bound file changed since the bind commit; no tag or release made in the phase. 12-REVIEW.md "FINAL" holds the check table, rows for API-05, API-06, API-07, SC1 to SC4 (the SC2 row carries the lane line and says closed), 27 CONTEXT decision rows, 15 deferred-idea rows (all absent), the carry-in table check (HR-06's api.md 8.1 sentence present) and five info findings FR-01 to FR-05.
- **Task 2: Qualification base and the final native qualification.** The audit was committed (`656dc00`), the `- Qualification base: 656dc00...` line alone (`40b7d2d`, H). At a quiet host (1-minute load 4.99) `python3 -B scripts/qualify-phase12.py --native` ran once and passed all 15 stages on the first attempt in 1304.01 s (no host sleep, no retry, no limit widened; the load rose to 41 during the gate from unrelated processes, and the tightest limit ratios were 1.76, 1.95 and 1.99). `--verify RECORD` exited 0. PIN-IMPACT "Qualification steps" holds `- Final record: build/gate/12-qualification-private/20261127T033336673294Z-40b7d2d9/record.json`, the record id, `Final record sdl-lane status: pass`, the binding and the stage table (`849920a`).
- **Task 3: claims, evidence and the owner checklist.** `docs/claims/phase12.md`: block 1 `phase12-title-headless` (7 of 7: the oracle, the runner name, the two title controls and the three consumer title cases; DR-34 split stated), block 2 `phase12-host-pump` (8 of 8 `host.pump.*` names, the two-speed equality), block 3 `phase12-bound-abi-record` (12 of 12 `repo.abi*` and `install.*` names, software interface only); build id `0.0.0+t64-build-v1-06a20d33...`, profile `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k`, the not_claimed list of the plan plus per-block items, a limits paragraph with the recorded SDL status (`pass`, `closed`). `docs/evidence/phase12-qualification.md`: audited commit, bases, build ids, the four golden hashes, ABI 1.0 / package 1.0.0 with the D6 sentence, the stage table with `sdl-lane` pass, the local lanes and the mutant table, the hosted runs of 12-06, 12-12, F-139 and 12-17, gate growth against 900 s, the LibFuturo pin and conclusion, the success-criteria table, the claims index and nine "Recorded exceptions for owner review" (none an approval). `docs/evidence/phase12-host-manual.md`: "human-needed, non-blocking, owner-attested; not run", the label, the two commands, blank fields and the seven pass-or-fail lines. Committed as `f089171`; `--verify` still exits 0.
- **Task 4: final push and hosted pair.** The full preflight passed (49 s; `linux-compile` skipped because no C file changed since `9f4e39a`); PII range, tree and rights scans 0 findings; `git push origin f0891716...:refs/heads/main`, a fast-forward from `9f4e39a`, no tag. Run 45937541960 green on its first attempt (warm). LOG.md Runs row and F-141; the evidence document's hosted row and text; PIN-IMPACT FINAL warm ledger row and the pushed SHA (`75fe74f`, local); `--verify` exits 0 after it.

## Task Commits

1. **Task 1: final independent audit (12-REVIEW FINAL)** - `656dc00433b24629f88b01795e17ef55225adf64` (docs, `[skip ci]`)
2. **Task 2: Qualification base** - `40b7d2d9997452a32619b677eeed84b4d68ea660` (docs, `[skip ci]`, PIN-IMPACT only; this is H, the qualified commit)
3. **Task 2: final record lines** - `849920a310484ce7ff711e22322ec46bb7a59335` (docs, `[skip ci]`)
4. **Task 3: claims, evidence, owner checklist template** - `f089171611d077fd2df8fe47f81ec61d0f0fb108` (docs; the pushed head, origin/main)
5. **Task 4: hosted pair records, F-141, FINAL warm ledger row** - `75fe74ffb082e9241326f372f3e7b7441c931186` (docs, `[skip ci]`, local)

Plan metadata: the SUMMARY, STATE, ROADMAP and REQUIREMENTS commit follows this file (local).

## Final record and hosted pair

| Item | Value |
|------|-------|
| Final record id | `20261127T033336673294Z-40b7d2d9` (private, `rehearsal: false`, commit H `40b7d2d`, SHA-256 `24d5db59...7c04a3b`) |
| Stages (seconds of limit, one attempt each) | ci-scanner 2.56/90, inventory 0.76/60, oracle 0.20/60, abi-cheap 8.97/720, phase12-suite 14.45/180, host 5.81/180, sdl-lane 1.92/180, gate 307.28/600, sanitize-shipped 150.85/300, sanitize-instrumented 682.32/1200, tsan 9.33/120, fresh-archive 4.09/120, sweep 61.19/300, freeze 0.22/60, mutants 53.24/240; total 1304.01 s |
| sdl-lane status | `pass` (SDL 3.4.10, mapping rows 30/30, 8 frames); `sc2_virtual_joystick` `closed` |
| Build id | `t64-build-v1-06a20d33f9a16380b4f97721549b37dcf8ad39421459f7e702821e74dd897203` (core, installed core, installed consumer, host tests) |
| Pushed SHA | `f089171611d077fd2df8fe47f81ec61d0f0fb108` (fast-forward from `9f4e39a`, no tag) |
| Hosted run | 45937541960, success, no rerun, Binutils cache hit on both jobs (warm) |
| gate-macos | job 559 s, Gate 499 s, ctest 320.69 s, 354 names; the 25 `phase12` names 97.63 s |
| gate-linux | job 286 s, Gate 234 s, ctest 155.35 s, 354 names; the 25 `phase12` names 43.92 s |
| Gate growth | against the Phase 11 warm pair (F-135, 539 s): job +20 s, ctest +42 s; warm margin 341 s; cold projections 749 s and 580 s; reversal rule not triggered |

## Deviations from Plan

### Plan wording

**1. Task 2's second verify starts with `--native`.** Read literally it would run a second native qualification after the first and make a second final record. The action runs the qualifier once and then `--verify` on that record; the one native run (exit 0) and its `--verify` (exit 0, and again after each later commit) satisfy the verify. No second record exists.

**2. Task 1's sweep verify** was run both in the clone and in the main tree at the identical commit `18636c8`, both 0 differences.

**3. Claim block 3 heading.** The bind has no requirement id of its own; the block is headed "SC4" and counts the `repo.abi*` and `install.*` names (12), each passed in the record's `abi-cheap` stage and its gate stage (the plan's "repo.abi and install names").

**4. Owner checklist details.** The shell prints no build id, so the field names `strings build/host-sdl/t64h_sdl | grep t64-build-v1-`; the qualified build id is printed beside it. The plan's line "a relaunch restores the position" is kept, with a sentence that the shell keeps no host-side save file (DR-19), so a relaunch boots with an empty save slot; the restored position is proven headlessly by `installed.consumer.title-persist`. Nothing is filled in and the file holds no PASS mark or date.

**5. Pushed head.** The pushed SHA is the Task 3 documents commit (its message has no `[skip ci]`, so the push triggers `ci.yml`); the Task 4 records commit and the plan metadata commit stay local, as in 11-25.

### Auto-fixed Issues

**1. [Rule 1 - Bug] The sampled load range of the final record in PIN-IMPACT** was first written as "15 to 41"; the samples were 7 to 41. Corrected in the Task 4 commit (planning file only).

### Environmental findings (not plan defects)

- **Docker is still unresponsive** (not started or restarted). Before this push the preflight `linux-compile` step was skipped legitimately (no C change since `9f4e39a`), which is not a Linux compile pass; the Linux evidence is the hosted gate-linux job (runs 45932117914 and 45937541960). Final audit FR-03.
- **Host load** rose to 41 during the qualifier's gate stage and 19 to 21 during the instrumented sanitizer stage (another project's Erlang VM and builds, `fseventsd`); the run started at 4.99 after the bounded wait and no limit was hit.
- `.DS_Store` was added to the scratch clone's own `.git/info/exclude`, as in the main repository since 12-16.

## Authentication Gates

None.

## Issues Encountered

None beyond the environmental findings above. No `.planning/WINDOWS.md` was created.

## Verification Results

| Check | Result |
|-------|--------|
| Task 1 verify 1 (`sh scripts/gate.sh` in the clone) | `[gate] PASS`, 354 of 354 |
| Task 1 verify 2 (base idiom, sweep against the Wave 0 base) | 1599 shared lines, 0 differences (clone and main tree) |
| Task 1 verify 3 (SDL lane and the SC2 row rule) | lane `[host-sdl] PASS`; one SC2 row, closed with the PASS line; rc 0 |
| Task 2 verify 1 (Qualification base 40-hex, a commit, no non-planning change since) | rc 0 |
| Task 2 verify 2 (`--native`, `- Final record:` line, `--verify`) | native exit 0 (once), `--verify` exit 0 |
| Task 3 verify (`check-claims.sh`, `pii-scan --tree`, `--verify`) | `claims: files=81 blocks=29 errors=0`; 0 findings; verified |
| Task 3 acceptance | reserved words 0; `not run` 1, no PASS mark, no date; pin labels 8 and 4; sdl-lane status `pass` equals the record |
| Doc readers (31 names incl. fixture closure: `gov.*`, `repo.docs*`, `repo.stale-freeze*`, `repo.abi-freeze*`, `repo.api07*`, `repo.host*`, `installed.consumer.literals`, `repo.installed-case-map`, `repo.ci*`) | 100% of 31 |
| Preflight (full, with replay) | `[preflight] PASS` 49 s |
| `pii-scan --range origin/main..HEAD` (7 commits), `--tree`, `rights-scan --tree --range` | 0 findings each |
| Task 4 verify (completed success run for the pushed SHA, no tag) | rc 0, run 45937541960 |
| `--verify` after the last task commit | verified |
| Closed Phase 11 records unchanged since W0 | `git diff --quiet` exit 0 |
| Post-commit deletion check | no file deleted by any commit |

## Handoff (orchestrator)

- **Cold measurement (orchestrator-only):** delete the macOS Binutils cache entry with `gh cache delete`, dispatch `ci.yml` on main, record the gate-macos job seconds against 900 s as LOG finding F-142 and the final cold ledger row. Current projections: 749 s and 580 s.
- **Local, unpushed:** `75fe74f` (LOG F-141, the evidence document's hosted row, PIN-IMPACT ledger) and the plan metadata commit. Both are documentation and planning only and leave `--verify` valid.
- **Phase verification inputs:** 12-REVIEW.md "FINAL"; the final record above (`python3 -B scripts/qualify-phase12.py --verify build/gate/12-qualification-private/20261127T033336673294Z-40b7d2d9/record.json` must print `record verified` and `sdl-lane status: pass; sc2_virtual_joystick: closed`); `sh scripts/check-claims.sh --files docs/claims/phase12.md` prints `blocks=3 errors=0`.
- **Owner items (not blocking):** the checklist `docs/evidence/phase12-host-manual.md` (not run); the recorded exceptions of the evidence document (G4 exception, the D6 item of the 1.0 numbers, the 12-11 outcome, carry-ins to v1.3, DR-34, plan-count exceptions, the Docker limitation, the accepted ABI limits).
- **Text items for a later plan:** FR-01 (`t64_facade.h` comment), FR-02 (`docs/spec/api.md` 2.3 sentence), HR-01 and HR-02 candidates.

## Known Stubs

None. The owner checklist is a template by design (CONTEXT: human-needed, non-blocking, never a must-have) and is not a stub of the plan's goal.

## Threat Flags

None. T-12-49: `check-claims.sh` errors=0, the not_claimed lists, the reserved-word grep 0, and the SC2 verdict tied to the recorded lane line. T-12-50: `pii-scan` tree and range and the preflight pii step before the push; raw logs and the record stay under the ignored `build/`. T-12-51: "Recorded exceptions for owner review" says none is an approval; the checklist is marked not run and holds no filled field.

## Self-Check: PASSED

The created files (`docs/claims/phase12.md`, `docs/evidence/phase12-qualification.md`, `docs/evidence/phase12-host-manual.md`) exist; the five commits above are ancestors of HEAD (`git merge-base --is-ancestor`), and `f089171` is `origin/main`.
