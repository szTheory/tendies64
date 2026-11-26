---
phase: 12-minimal-host-playable-homebrew-and-final-bind
plan: 04
subsystem: runner
tags: [title, supervised-runner, raw-capture, wrong-input-control, refused-frame-control, runner-recorded-pins, runner-exclusions, decision-DR-34]

requires:
  - phase: 12-03
    provides: the frozen title guest (source revision 7b962961fcb8e1c1600dc84dae8a67f0c88dce26), the build-only ROMs phase12-title-s32k and phase12-title-refused-s32k and their manifest rows
  - phase: 12-02
    provides: fixtures/title/title.expected.json and the input scripts (the independent expectation), harness.title-oracle
provides:
  - runner table rows phase12-title-s32k (pass-eligible, long cap, completion PC 0x80000820, transcript pin) and phase12-title-refused-s32k (never pass-eligible), the only production runner edit
  - runner/tests/test_title_two_run.c with cases two-run, wrong-input and refused-frame (one binary, one CTest name per case)
  - CTest names runner.title-two-run (COST 0.96), controls.real.title-wrong-input (COST 0.62), controls.real.title-refused-frame (COST 0.72)
  - fixtures/title/title.runner-pins.json (label "runner-recorded pin, not independent"; completion tick, stop reason, final state digest, video packet count of the short scenario)
  - decision DR-34 (runner scope for the title) and the amended plans 12-04, 12-05, 12-06 and 12-18, ROADMAP lines and 12-VALIDATION rows
  - the TITLE stub row of the Gate-growth ledger
affects: [12-05, 12-06, 12-08, 12-18]

actuals:
  tokens: 23337
  tasks: 3
  commits: 8
plan_head_before: a72d9761604a2032a89f626179b3f1fd7af1fc0b
plan_head_after: de56e2440d0f4ab46b7e6ee3433167a8e6f45ad0

tech-stack:
  added: []
  patterns:
    - "a runner limit is recorded as an executable fact (a case that fails when the limit is lifted and names the comparison that can return), not only as prose"
    - "a control is judged only by what the runner can show; every judged field has a same-setup control that changes exactly that field in a copy of the observation and must be refused with its own text"

key-files:
  created:
    - runner/tests/test_title_two_run.c
    - fixtures/title/title.runner-pins.json
  modified:
    - runner/t64_real_fixture.c
    - runner/CMakeLists.txt
    - docs/spec/result-protocol.md
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase12-regexes.txt
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/PIN-IMPACT.md
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-04-PLAN.md
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-05-PLAN.md
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-06-PLAN.md
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-18-PLAN.md
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-VALIDATION.md
    - .planning/ROADMAP.md

key-decisions:
  - "DR-34 (orchestrator decision B, 2026-11-26, costly): the runner stays at the manifest rows. runner.title-two-run closes on its one raw-capture run plus a recorded exclusion text; the save-image equality and any cross-run-kind final-digest check move to plan 12-05 and the 12-08 pump equality; controls.real.title-refused-frame shows only what the runner can show (non-pass, no completion, capture-error at the 4,000,000 tick cap, no transcript); the refused ROM's blank-frame assertion moves to plan 12-05 as a third consumer case. Reversal path: a runner contract change in a numbered plan."
  - "The CTest name runner.title-two-run keeps its DR-27 spelling although it is now one run."
  - "The refused-frame comparator reads the oracle's refused section (completes, transcript) and a recorded runner limit literal (capture-error, host-error, tick 4,000,000); a lifted limit (a published bundle, another detail) fails the case and names the moment the blank assertion can return to the runner."

requirements-completed: []

coverage:
  - id: D1
    description: "The real supervised runner runs the title on the short scenario in raw-capture mode and the frames (3 packets of 49,152 bytes), both audio packets and the transcript equal fixtures/title/title.expected.json; the four runner-recorded pins are labeled and compared"
    requirement: "API-06"
    verification:
      - kind: integration
        ref: "ctest -R '^runner\\.title-two-run$' (log: 3 'checkpoint N equal' lines, audio and transcript equal, 'compare pins (runner-recorded pin, not independent)')"
        status: pass
    human_judgment: false
  - id: D2
    description: "The two runner exclusions are executable facts: a run without raw capture ends video-drain-error at the first packet, and raw capture with the save-out producer is a usage refusal, so the runner produces no save image of the title"
    requirement: "API-06"
    verification:
      - kind: integration
        ref: "ctest -R '^runner\\.title-two-run$' (two 'exclusion:' log lines)"
        status: pass
    human_judgment: false
  - id: D3
    description: "The wrong-input control fails at checkpoint 2 with the comparator's own text on a non-blank frame, and the same frames equal the short-wrong expectation; the refused-frame control judges only the runner-visible limit and refuses each of six changed fields with its own text"
    requirement: "API-06"
    verification:
      - kind: integration
        ref: "ctest -R '^controls\\.real\\.title-(wrong-input|refused-frame)$'; seeded failures A to E recorded in PIN-IMPACT Title expectations"
        status: pass
    human_judgment: false
  - id: D4
    description: "The save image equality and the refused ROM blank-frame assertion are reassigned to plan 12-05 in its must_haves and tasks; 12-06, 12-18, ROADMAP and 12-VALIDATION text follows; frontmatter valid"
    requirement: "API-06"
    verification:
      - kind: structural
        ref: "node gsd-tools.cjs frontmatter validate and verify plan-structure on 12-04, 12-05, 12-06, 12-18 (valid, no errors)"
        status: pass
    human_judgment: false

duration: 122min
completed: 2026-11-26
status: complete
---

# Phase 12 Plan 04: Title Through the Supervised Runner Summary

**The authored title runs through the real supervised runner in one raw-capture run whose 3 frames, 2 audio packets and 20-byte transcript equal the independent oracle, with the four labeled runner-recorded pins, a wrong-input control that fails at checkpoint 2, a refused-frame control that shows only the runner-visible limit, and the two runner exclusions recorded as executable facts under decision DR-34.**

## Performance

- **Duration:** about 122 min of wall time from the 12-03 head, including the Rule 4 stop for the orchestrator decision; this continuation (decision B applied) took about 20 min
- **Completed:** 2026-11-26
- **Tasks:** 3 of 3 (Task 1 tracer, Task 2, Task 3), with the scope of Tasks 2 and 3 amended by decision B
- **Files:** 2 created, 12 modified (one production runner file, `runner/t64_real_fixture.c`, rows only; no oracle-bound file, no expected value taken from a runner run)

## Accomplishments

- Runner rows `phase12-title-s32k` (pass-eligible, `T64_REAL_GUEST_TICKS_LONG_CAP`, completion PC `0x80000820`, transcript pin of the short expectation) and `phase12-title-refused-s32k` (never pass-eligible). The Phase 9 raw-capture exclusion and every cap are unchanged.
- `runner.title-two-run`: one raw-capture run of the short scenario completes at guest tick 3,693,938 (stop `authored-completion`) with 3 video packets of 49,152 bytes, 2 audio packets and the transcript; every frame digest, both audio digests and the transcript equal `title.expected.json`, each followed by a same-setup control (an expected digest or byte changed in a copy is reported at its exact position). The four pins (completion tick, stop reason `GUEST_COMPLETION`, final state digest `57b683df...`, 3 packets) are in `fixtures/title/title.runner-pins.json` and compared by the case.
- Recorded runner exclusions (decision B, item 1): a run of the title without `--raw-capture-dir` ends harness-error `video-drain-error`, stop `host-error`, at guest tick 1,589,376 (the drain of the first packet); raw capture together with the artifact pair and save-export options is refused as harness-error `usage`, exit 2, tick 0, no raw directory. The case fails if either limit is lifted.
- `controls.real.title-wrong-input`: the short-wrong script judged against the short expectation passes the run like the accepted run and fails at checkpoint 2 (`548e2016...` expected, `e063ea0a...` actual, non-blank); the same frames equal the short-wrong expectation of the oracle.
- `controls.real.title-refused-frame` (decision B, item 3): the refused ROM ends harness-error `capture-error`, stop `host-error` at tick 4,000,000, no transcript, no published bundle, no completion, judged against the oracle's `refused` section; six same-setup controls (completion, outcome, detail, stop tick, transcript length, bundle state) are each refused with their own `refused-frame mismatch` text. The limit is named in the control text and in PIN-IMPACT.
- Decision DR-34 and the amended plans (items 4 and 5): see below.

## Task Commits

1. **Task 1 (tracer): rows and the supervised raw-capture run compared with the oracle** - `97ef591` (feat)
2. **Task 2: wrong-input control and the runner-recorded pins** - `cd4d917` (feat); comparator text printed and checkpoints named truthfully - `4ed1f4f` (test)
3. **Rule 1 fix: result protocol names 18 pinned transcript rows** - `8dd37a8` (fix)
4. **Measurements, seeded failures and runner constraints recorded (first executor's stop)** - `a50342c` (docs)
5. **Decision B applied: refused-frame control and the recorded exclusions** - `bfc9939` (feat)
6. **Decision DR-34 and the amended plans** - `8832d08` (docs)
7. **TITLE ledger stub with the full-gate measurement** - `de56e24` (docs)

**Plan metadata:** the commit of this summary with STATE and ROADMAP follows.

`commits: 8` is `git rev-list --count a72d9761604a2032a89f626179b3f1fd7af1fc0b..de56e2440d0f4ab46b7e6ee3433167a8e6f45ad0`, read from the persisted ledger file of the plan.

## Decision B (DR-34) and the amended plans

The first executor stopped at Task 2 with a Rule 4 question: the runner cannot produce a save image of the title (a run without raw capture cannot drain a 49,152-byte packet, and the artifact producers are refused with raw capture) and cannot show any frame of the refused ROM (the raw bundle finalizes only on a completion with audio). The orchestrator decided on 2026-11-26 that the runner stays at the manifest rows. Recorded as DR-34 in PIN-IMPACT, rating costly, with the reversal path "a runner contract change in a numbered plan":

| Item | Result |
|------|--------|
| (1) `runner.title-two-run` | closes on its one raw-capture run plus the recorded exclusion text (executable) |
| (2) save-image equality, final-digest cross-check from a second run kind | moved to plan 12-05 (`installed.consumer.title` and `title-persist` read the save through the installed API against the oracle save digest; the consumer's final digest is compared with the runner pin) and to the 12-08 pump equality |
| (3) `controls.real.title-refused-frame` | registered for what the runner shows only; the limit is named in the control text and PIN-IMPACT; inventory name, preflight row `^controls\.real\.title-` 1 to 2, COST, comparator seeded |
| (4) "every frame is blank" for the refused ROM | moved to plan 12-05 as a third consumer case `title-refused` (new CTest name `installed.consumer.title-refused`; the consumer compares every drained packet with `blank_sha256` and expects no audio, no transcript, no completion and the blank save digest) |
| (5) text amendments | 12-04 (must_haves, objective, Task 1 closing line, Task 2 name, action, read_first and acceptance, Task 3 seeds, success criteria), 12-05 (must_haves truths 1 and a new truth for `title-refused`, Task 2 action and verify regex, acceptance, Task 3 seeds and ledger names, objective, output, verification, success criteria), 12-06 (34 DR rows in three places, the control log lines of review item (d), item (e) names), 12-18 (counted names and the limits sentence), ROADMAP plan lines 12-04 and 12-05, 12-VALIDATION rows 12-04 and 12-05; PIN-IMPACT DR-08, DR-09, DR-27 and registration-map rows R-10 and R-13 follow. Frontmatter of the four plans validates; `verify plan-structure` reports no errors. 12-08, 12-12 and 12-17 cite neither a runner save-out run nor a runner-side blank assertion and are unchanged. |

For plan 12-05's executor: the preflight comment in `scripts/preflight-phase12-regexes.txt` already says "3 names plus 2 = 5"; the row `^installed\.consumer\.title` stays 0 until 12-05 raises it (and `^installed\.consumer\.` by 3).

## Seeded failures (scratch copies under `build/12-04-seed`, never committed)

Seeds A to C are the first executor's (frame digest, wrong-run digest, transcript byte). After decision B two more were run directly on `test_title_two_run` through `T64_TITLE_EXPECTED`:

| Seed | Scratch change | First failed assertion |
|------|----------------|------------------------|
| D | oracle `refused.completes` set to true | `FAIL verdict.ok` of case refused-frame; text "refused-frame mismatch: oracle completes=1, run completed=0 (harness-error, capture-error) at guest tick 4000000, 0 transcript bytes" |
| E | oracle `refused.transcript` made 17 bytes long | `FAIL verdict.ok` of case refused-frame; text "refused-frame mismatch: transcript expected 17 bytes, captured 0" |

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] The result protocol document counted 17 pinned transcript rows**
- **Found during:** Task 1 (`runner.fixture-transcript-pin` compares the document's number with the runner table)
- **Issue:** the title row adds one fixture row with its own transcript pin, so `docs/spec/result-protocol.md` and `runner.fixture-transcript-pin` disagreed (17 versus 18)
- **Fix:** the number is now 18 (the same fix plan 10-14 made for the composite rows)
- **Files modified:** docs/spec/result-protocol.md
- **Commit:** `8dd37a8`

**2. [Rule 1 - Bug] The checkpoint log line printed "equal" for a frame that differed** (`4ed1f4f`)
- Fixed in the test: it now prints DIFFERS and the comparator text on a mismatch (seen with the seeded copies).

### Plan-scope deviations

**3. [Rule 4 - decided by the orchestrator] Plan scope amended (decision B, DR-34)**
- **Found during:** Task 2
- **What changed:** the save-out run, the final-digest equality across two runs, the save comparator and the refused ROM's blank-frame assertion left this plan; see the table above. `runner.title-two-run` is one run although its name says two (the name keeps its DR-27 spelling).
- **Commits:** `bfc9939`, `8832d08`

**4. [Consequence of 3] A new CTest name for plan 12-05**
- The blank-frame assertion needs the refused ROM, and the installed consumer reads exactly one `--rom`, so plan 12-05 gains a third case and the CTest name `installed.consumer.title-refused` (about 0.5 s native). DR-09, DR-27 and the registration map follow, and the projection of about 23 new names becomes 24 (information only; the ledger stub says so). Plan 12-05 registers the new name; this plan registers none of it.

**5. [Measurement note] COST of `runner.title-two-run` is 0.96, not 0.62**
- The case now includes the two recorded exclusion runs (a run to the first packet and a refused invocation); wrong-input is 0.62 and refused-frame 0.72 (the slowest of four runs at host load about 5.5).

## Authentication Gates

None.

## Issues Encountered

- A first probe of the supervisor failed with `usage` because the work directory had not been created; the test creates it with `mkdtemp` and the probe was corrected. No limit was widened.
- Host load rose to 12 during the full gate (it passed: no wall backstop tripped).

## Verification Results

| Check | Result |
|-------|--------|
| `cmake --build --preset gate` (final head) | up to date, no warnings as errors |
| `ctest -R '^(runner\.title-two-run\|controls\.real\.title-)'` | 3 of 3 passed (0.94 to 0.96, 0.61 to 0.62, 0.66 to 0.72 s) |
| `ctest -R '^runner\.(composite\|phase09)'` | passed unchanged (generic row checks accept the new rows) |
| seeded failures A to E | each first failed assertion recorded in PIN-IMPACT |
| `sh scripts/check-test-inventory.sh --preset gate` | PASS count=333 |
| `ctest -N -L phase12` | 4 names (the PHASE12_TESTS block) |
| COST of the three names via `--show-only=json-v1` | 0.96, 0.62, 0.72 |
| sampling pin guards (`harness.rdp-oracle-authority`, `repo.si-membership-sites`, `harness.state-field-use-audit`, `repo.p10-mutant-anchors`, `repo.rsp-no-hle`) | 5 of 5 passed |
| `sh scripts/gate.sh` (output under `build/12-04-gate.log`) | PASS 333/333, total 260 s (ctest 160 s) at host load 8 to 12 |
| `sh scripts/preflight-phase12.sh --no-replay` (at the final head) | PASS (ctest-counts: `^controls\.real\.title-` 2) |
| `sh scripts/pii-scan.sh --tree` before each commit | findings=0 |
| frontmatter validate and `verify plan-structure` for 12-04, 12-05, 12-06, 12-18 | valid, no errors |
| `git diff` of the plan's range for `/Users/`, `/private/`, `/tmp/` | none |

## Known Stubs

None.

## Threat Flags

None. T-12-11 (pins presented as independent) is mitigated by the exact label, the citation and the comparison with the oracle only for frames, audio and transcript; T-12-12 (a control passing for an unrelated reason) by the seeded failures A to E and the six same-setup controls of the refused-frame case; T-12-13 (cap or raw-capture limit) by at most 3 packets of 49,152 bytes, the unchanged long cap and the recorded exclusions.

## Next Phase Readiness

Plan 12-05 (installed consumer title cases) can start. It now owns, besides the short, long, shifted and persistence scenarios, the title's save image equality against the oracle save digest and the new `title-refused` case (every drained packet of the refused ROM equals `blank_sha256`; no audio, no transcript, no completion; blank save digest). API-06 stays incomplete until 12-05 and the later TITLE plans close it. Nothing here is orchestrator-only or owner-gated and no push is named; the local commits since `origin/main` (`dc247fa`) now include this plan's eight commits and this summary.

## Self-Check: PASSED

- FOUND: runner/tests/test_title_two_run.c, fixtures/title/title.runner-pins.json, the three CTest names in `build/gate`, DR-34 and the TITLE stub in PIN-IMPACT
- FOUND commits: 97ef591, cd4d917, 4ed1f4f, 8dd37a8, a50342c, bfc9939, 8832d08, de56e24 (all ancestors of HEAD)
