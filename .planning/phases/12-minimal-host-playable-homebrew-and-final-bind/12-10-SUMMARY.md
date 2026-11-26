---
phase: 12-minimal-host-playable-homebrew-and-final-bind
plan: 10
subsystem: host
tags: [api-07, libfuturo, fit-evaluation, evidence-guard, defect-triage, amendment-slot]

requires:
  - phase: 12-09
    provides: the HOST wave (pump, scripted source, SDL shell, policy scan) whose API observations the triage closes
  - phase: 12-01
    provides: PIN-IMPACT "API-07 pin" (the local, unpublished pin the evaluation compares against), the empty "Host-found API defects" section and DR-22 and DR-23
provides:
  - docs/evidence/phase12-libfuturo-fit.md: the written API-07 evaluation at the pinned commit (ten-axis comparison, five LibFuturo-side deltas, conclusion "no adapter now")
  - scripts/check-api07-evaluation.sh and the CTest name repo.api07-evaluation (seven rules, 32-case selftest, COST 1.34)
  - PIN-IMPACT "Host-found API defects": 15 rows (C-1 to C-6, H-01 to H-09), one disposition each, and the line "12-11 trigger: no"
affects: [12-11, 12-12, 12-13, 12-14, 12-15, 12-17, 12-18]

actuals:
  tokens: 10300
  tasks: 3
  commits: 4
plan_head_before: 7a0c17d1cda02b45606d8611e4c07c1585971fe1
plan_head_after: 5a28fc05252f439a92df3ecf7a7dc3ee028acc14

tech-stack:
  added: []
  patterns:
    - "an evidence document pinned to an external commit by a 40-hex line that a guard compares with the planning record, with a no-support-claim rule and a no-adapter-code scan over the code roots (tracked files only in a git tree)"
    - "a triage table whose verify counts rows, dispositions and the trigger line, so the amendment slot opens only on recorded evidence"

key-files:
  created:
    - docs/evidence/phase12-libfuturo-fit.md
    - scripts/check-api07-evaluation.sh
  modified:
    - tools/hostcheck/CMakeLists.txt
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase12-regexes.txt
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/PIN-IMPACT.md

key-decisions:
  - "API-07 conclusion: no adapter now. Reasons: the other contract is experimental by its own words and promises no binary ABI, its input is one word sampled at the start of an advance (lossy against tick-stamped events and stick axes), and it names no profile for this machine; every delta is a change on the LibFuturo side and none touches the Tendies64 core API"
  - "Host-found defect triage: 15 rows, 13 accept, 2 document (C-1 done in 12-07; H-03 owed to 12-15), 0 amend, so the line reads '12-11 trigger: no' and the amendment slot closes with no code"
  - "H-02 (no guest-tick query) is accepted on evidence: an advance with the frame-event stop and target 0 answers OK at the current tick, changes no digest and is documented behavior (api.md 8 and 8.2)"

patterns-established:
  - "the CTest name of a guard with a selftest runs --with-selftest, so one cheap name holds both the scan and its seeded rules"

requirements-completed: [API-07]

coverage:
  - id: D1
    description: "A written LibFuturo fit evaluation exists, pinned to the 40-hex commit of PIN-IMPACT 'API-07 pin' (a local, unpublished commit, said so in words), compares ten axes from committed objects only, lists the deltas as LibFuturo-side changes and concludes 'no adapter now' with the adapter optional and never shaping the core API"
    requirement: "API-07"
    verification:
      - kind: unit
        ref: "ctest -R '^repo\\.api07-evaluation$' (1 of 1, scan plus selftest); sh scripts/check-api07-evaluation.sh prints findings=0; grep -c '^| ' docs/evidence/phase12-libfuturo-fit.md prints 17; grep -c -i -E 'supports|compatible with libfuturo' prints 0; sections 'What an adapter would need' and 'Conclusion' present"
        status: pass
    human_judgment: false
  - id: D2
    description: "The guard fails on a missing document, a missing or malformed pin line, a record row that is missing, doubled or without a 40-hex value, a document pin that differs from the record, a Conclusion without 'no adapter now', a line that claims support, and the project name in a tracked file under core, runner, host, tools or tests; each rule has a failing seed"
    requirement: "API-07"
    verification:
      - kind: unit
        ref: "sh scripts/check-api07-evaluation.sh --selftest: 32 lines ok (25 failing seeds, 6 accepted controls, the empty-scan error), failures=0, and the run fails when a rule has no seeded case; git grep -i -c libfuturo -- core runner host tools tests prints nothing"
        status: pass
    human_judgment: false
  - id: D3
    description: "Every API observation of plans 12-02 to 12-09 is a row of the triage with evidence, 'breaks a postcondition' and exactly one disposition, and the trigger line agrees with the number of amend rows"
    requirement: "API-07"
    verification:
      - kind: other
        ref: "the plan's Task 3 verify: 15 rows, 6 C rows, 15 disposition cells, 0 amend rows, one line '12-11 trigger: no' (exit 0)"
        status: pass
    human_judgment: false
  - id: D4
    description: "No core, runner, host, header, golden, workflow or gate-script file changed, and the full gate passes with the one new name"
    requirement: "API-07"
    verification:
      - kind: integration
        ref: "git diff --name-only 7a0c17d..HEAD -- core runner host tests docs/abi .github scripts/gate.sh scripts/check-ci-yml.sh CMakeLists.txt tools/abi is empty; sh scripts/gate.sh PASS 351 of 351 names, 161 s (ctest 92 s)"
        status: pass
    human_judgment: false

duration: 28min
completed: 2026-11-26
status: complete
---

# Phase 12 Plan 10: API-07 LibFuturo Evaluation and the Host-Found Defect Triage Summary

**A written fit evaluation of the sibling LibFuturo contract, pinned to its local, unpublished commit and concluding "no adapter now", is guarded by `repo.api07-evaluation` (seven rules, 32-case selftest); the 15 API observations of the TITLE and HOST waves are triaged with evidence into 13 accept, 2 document and 0 amend, so `12-11 trigger: no` and the amendment slot closes with no code. No core, header, golden or bound file changed.**

## Performance

- **Duration:** about 28 min (about 8 of them waiting for unrelated host load to fall before the full gate)
- **Started:** 2026-11-26T21:43:00Z
- **Completed:** 2026-11-26T22:11:00Z
- **Tasks:** 3 of 3 (Task 1 tracer, Task 2 auto, Task 3 auto)
- **Files:** 2 created, 4 modified; 4 commits before this record

## Accomplishments

- **Pin and guard (Task 1 tracer, c1d1b2d).** The pin was re-verified from committed objects only: the commit exists (object type commit), it is the head of the sibling checkout, 0 commits follow it, the history is 1,169 commits and no remote is configured, so the pin is the same local, unpublished SHA as the Phase 11 read and PIN-IMPACT. The document skeleton carried the pin line, the date and subject, the published status in words, the method (four files read by name through `git show`) and the Conclusion heading. `scripts/check-api07-evaluation.sh` (POSIX sh and awk; `--root`, `--selftest`, `--with-selftest`) has seven rules: DOC, PIN-LINE (exactly one `Pinned commit:` line with exactly one 40-hex value and no longer hex run), PIN-RECORD (the PIN-IMPACT section "API-07 pin" has exactly one `Pinned commit` row with one 40-hex value), PIN-MATCH, CONCLUSION, CLAIM (the phrase "LibFuturo support" with no word "no" before it on its line) and ADAPTER (the project name, case-insensitive, in a tracked file under core, runner, host, tools or tests: `git grep` in a git tree, `grep -r` in a plain tree). The selftest builds trees in a temporary directory: 25 failing seeds (each rule at least once, the five code roots, a long hex run, an upper-case SHA, a second pin line, a record with no, two or a hex-less row), 6 accepted controls (including a token outside the code roots and an untracked file in a git tree), the empty-scan error, and a final check that every rule id has a failing seed. `repo.api07-evaluation` runs `--with-selftest` (one cheap name, registration comment without the project name), is in `PHASE12_TESTS`, and its preflight row is raised from 0 to 1.
- **Evaluation (Task 2, 14f330a).** The comparison covers ten axes (instance and allocation, content admission, input, time, video, audio, media and saves, snapshots, errors and postconditions, ABI promise) with the LibFuturo shape at the pin, the Tendies64 shape as bound (ABI 0.1 candidate, dated by a sentence in the method), and a fit word defined once (compatible, convertible with loss, missing): 3 compatible (instance, media, snapshots in kind), 6 convertible with loss and 1 missing (the ABI promise). "What an adapter would need" lists five deltas (a tick-stamped input call or an explicit cadence; an RGBA format or a stated alpha rule and more than one image per run; byte order and a per-packet rate for audio; an admission entry, that is a registered profile and a time unit for this machine; a defect class), each marked a LibFuturo-side change. The Conclusion opens with "no adapter now", gives the reasons, says that any adapter would be optional, outside the core and must not shape the core API, that no adapter code, build target or test exists and that no LibFuturo support exists or is claimed, and names the trigger of a re-evaluation (a published, versioned contract or a profile for this machine, with a new pin). No filesystem path, no copied text beyond identifiers, no other project's name.
- **Triage (Task 3, 5a28fc0 and 41b4c33).** PIN-IMPACT "Host-found API defects" now holds 15 rows with seven columns (ID, found by plan, observation, evidence, breaks a postcondition, disposition, reason): C-1 to C-6 of the research and H-01 to H-09 from the SUMMARYs of plans 12-04, 12-05 and 12-07 to 12-09 (O-1 to O-5 of the observation sections, the 1,000,000-tick slice stop, the 128 against 4,096 event caps, the runner video-drain limit of DR-34, and the pump speed figure). Every row says "no" to "breaks a postcondition": the HOST wave produced no failing test of the bound API and no recorded contradiction of a promise. Dispositions: C-1 document (done in 12-07), H-03 document (open: one sentence for `docs/spec/api.md` 8.1, carried by 12-15), the other 13 accept with a reason. The line `12-11 trigger: no` follows the table. The "API-07 pin" section records that the pin did not move.
- **Probe for H-02 (not committed).** A throwaway C99 probe against the moved prefix, outside the repository, showed that a host can read the guest tick without moving it: after an advance that stopped at tick 3,581 an advance with `T64_STOP_FRAME_EVENT_COUNT` and target 0 answered OK with reason FRAME_EVENT_COUNT at tick 3,581, left the machine digest identical, and the next bound advance moved on. This is documented behavior (api.md 8 and 8.2) and the code agrees (`advance_counter_bound` of `core/t64_scheduler.c`), so "no guest-tick query" is not a missing postcondition. The same probe saw a 250,000-tick bound stop at tick 3,581 (the event budget stop of row H-06).
- **The one near-defect (H-03).** `core/t64_scheduler.c` pops the event and moves the guest tick to the event's tick before the handler runs, and a whitelisted non-OK handler status then ends the advance as `RESOURCE_LIMIT`; so the event is consumed, the instance is resumable and the next advance continues. api.md 8.1 lists the masked status among the causes but ends "A stall with no progress after a full drain is permanent" without limiting it to ring backpressure, which would mislead a host that judges progress by tick movement alone. The header holds no such sentence, so the repair is one api.md sentence (no golden, no amendment).

## Task Commits

1. **Task 1 (tracer): pinned document skeleton, guard, registration** - `c1d1b2d` (feat)
2. **Task 2: comparison, deltas, conclusion** - `14f330a` (docs)
3. **Task 3: COST of `repo.api07-evaluation` from the gate's cost data (1.25 to 1.34)** - `41b4c33` (chore)
4. **Task 3: triage, trigger line, pin re-verification** - `5a28fc0` (docs, `[skip ci]`)

## Verification Results

| Check | Result |
|-------|--------|
| `cmake --build --preset gate && ctest -R '^repo\.api07-evaluation$' --no-tests=error` | 1 of 1 passed (1.6 to 2.2 s wall at load 8 to 12; cost data average 1.33 s over 10 runs) |
| `sh scripts/check-api07-evaluation.sh --selftest` | 32 ok (25 failing seeds, 6 accepted, the empty-scan error), failures 0 |
| `sh scripts/check-api07-evaluation.sh` | files 508, findings 0 |
| `git grep -i -c libfuturo -- core runner host tools tests` | prints nothing (exit 1) |
| `grep -c '^| '` on the document | 17 (12 in the axis table); `grep -c -i -E 'supports|compatible with libfuturo'` 0; sections "What an adapter would need" and "Conclusion", the words "optional" and "must not shape the core API" present |
| The Task 3 verify of the plan (trigger line, 15 rows, 6 C rows, one disposition cell each, trigger line equals the amend count of 0) | exit 0; per-row check: 15 rows, 7 cells, 1 disposition cell each |
| `sh scripts/check-test-inventory.sh --preset gate` | PASS count=351 |
| `sh scripts/check-claims.sh` | files 78, blocks 26, errors 0 (the document passes the reserved-word rule) |
| `sh scripts/preflight-phase12.sh --no-replay` | PASS 66 s (ctest counts with the raised `^repo\.api07-evaluation$` row, linux-compile 55 s) |
| `sh scripts/gate.sh` (output under the ignored `build/`) | PASS 351 of 351 names, 161 s total (ctest 92 s), host load 9 to 12 |
| `sh scripts/pii-scan.sh --tree` before each commit | findings 0 |
| `git diff --name-only 7a0c17d..HEAD -- core runner host tests docs/abi .github scripts/gate.sh scripts/check-ci-yml.sh CMakeLists.txt tools/abi` | empty |

Native seconds of the new name: 1.2 to 1.8 (cost data average 1.33 s; COST 1.34). Gate-growth for the 12-12 ledger: 1 name, +1.34 s of COST, 351 names in all.

## Deviations from Plan

### Auto-fixed and strengthened

**1. [Rule 2 - Missing critical functionality] guard rules split and a rule-coverage check**
- **Found during:** Task 1 design
- **Issue:** the plan names five failure modes; a guard that reports them under fewer ids cannot say whether the document or the planning record is at fault, and a missing record would otherwise read as a mismatch.
- **Fix:** seven rule ids (DOC, PIN-LINE, PIN-RECORD, PIN-MATCH, CONCLUSION, CLAIM, ADAPTER), a PIN-IMPACT lookup by path suffix under `.planning` (so a later archive of the phase directory still resolves), a git-tree seed that shows untracked build outputs are ignored, and a selftest check that every rule id has a failing seed.
- **Commit:** c1d1b2d

**2. [Rule 1 - plan wording] "pre-1.0 on the LibFuturo side by its own words"**
- **Found during:** Task 2
- **Issue:** the four files named by the plan do not say "pre-1.0": the README and the contract say "experimental" and "no stable binary or plugin ABI", and the research found a tag of a different number in the checkout. Writing "pre-1.0" would claim a version fact the named sources do not give.
- **Fix:** the document says "experimental by its own words" and "promises no stable binary or plugin ABI"; the conclusion's reasons follow that wording.
- **Commit:** 14f330a

**3. Plan text adaptations (no rule)**
- The CTest name runs `--with-selftest` (the plan registers only `repo.api07-evaluation` and says its selftest seeds each rule), so scan and seeds are one name.
- COST was set from the tracer measurement (1.25) and corrected to the gate's cost-data average (1.34) in a small extra commit, as 12-09 did for its names.
- The Wave 0 placeholder columns of the triage section (ID, found by plan, postcondition, evidence, disposition; "amend, accept or defer") were replaced by the plan's seven columns and the three dispositions amend, document and accept; DR-23 still reads the same trigger.
- Besides the two files PIN-IMPACT names, the README, the LICENSE and the committed file list at the pin were read (the method says so).

**Total deviations:** 1 auto-strengthened (Rule 2), 1 plan wording corrected (Rule 1), plus plan text adaptations. **Impact:** no scope outside docs/evidence, scripts, the registration file, the preflight row, the inventory block and PIN-IMPACT.

## Auth gates

None.

## Issues Encountered

- **Host load.** Unrelated load (VM and Docker) was 9 to 18 around the preflight and gate; I waited about 8 minutes for it to fall and the gate passed at 9 to 12 with no wall backstop tripping and no limit widened.
- **A loop variable clobbered by a helper** (`_r` shared between the selftest loop and `mk_tree`) made the first selftest run fail on a missing directory; fixed before the first commit by renaming the loop variable.

## Known Stubs

None.

## Threat Flags

None new. T-12-33 (path or private details): no path of the sibling checkout, no external query, `pii-scan --tree` findings 0, and the document names no other project. T-12-34 (a support claim): rule CLAIM, the grep acceptance and the no-adapter scan over the code roots. T-12-35 (an adapter shaping the core API): the deltas are LibFuturo-side and the triage admits only postcondition defects to the amendment path (none found).

## Next Phase Readiness

- **12-11** reads `12-11 trigger: no` and closes with a one-line SUMMARY and no code.
- **12-15 must carry H-03:** one sentence in `docs/spec/api.md` 8.1 (a masked handler status consumes its event and the next advance continues; the permanent stall is the ring-backpressure case). The plan's own text for 12-15 lists api.md but not this sentence; its read_first names PIN-IMPACT "Host-found API defects", where the row says so.
- **12-12** appends the HOST gate-growth ledger row: `repo.api07-evaluation` is 1 name, COST 1.34 (cost data average 1.33 s), 351 names in the local gate; the first hosted run of the guard (Linux awk, `git init` inside the selftest) comes at the push 12-12 names, and a hosted mismatch would be a finding for a numbered plan. The preflight `linux-compile` step compiles only, so the script has not run on Linux yet.
- **Milestone close:** the guard finds PIN-IMPACT by the path suffix under `.planning`; archiving the phase directory keeps it working as long as the suffix and the "API-07 pin" section survive, and removing `.planning` would make `repo.api07-evaluation` fail PIN-RECORD (a decision for the archive step, not for this plan).
- **12-17 and 12-18** cite API-07 as met by the pinned evaluation; nothing here is orchestrator-only or owner-gated, and no push is named by this plan (origin/main is still 884153a; the local commits since go out at 12-12).

## Self-Check: PASSED

Created files exist (the evaluation, the guard, this summary) and the four commits c1d1b2d, 14f330a, 41b4c33 and 5a28fc0 are ancestors of HEAD.
