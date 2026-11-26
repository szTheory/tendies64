---
phase: 11-installed-consumer-and-freeze-candidate
plan: 23
subsystem: abi-freeze-record
tags: [freeze-wave, freeze-candidate, abi-record, header-hash, layout, padding-holes, seeded-mutants, amendment-rule]
status: complete

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: docs/spec/api.md (11-22), the 11-29 header text, the goldens docs/abi/status-classes.txt (11-02), docs/abi/layout.c and docs/abi/public-symbols.txt (11-17), the installed prefix of install.setup (11-16) and the hosted-green INSTALL wave (11-21, run 45775107983)
provides:
  - docs/abi/header.sha256 and docs/abi/FREEZE-CANDIDATE.md, the bound freeze-candidate record (ABI 0.1, package 0.1.0) with a machine-read block, the amendment rule, the Phase 12 hand-off and an empty amendment log
  - scripts/check-abi.py freeze mode and CTest repo.abi-freeze-record, which recomputes the four goldens from the installed tree and fails on any difference
  - CTest repo.abi-freeze-selftest, 21 seeded cases (one per bound element and kind of incompatible change, plus controls)
  - S-05 handled: the eight implicit 4-byte padding holes are measured from the installed header and bound as padding_hole rows; per-target statement of the layout pins in the record
affects: [11-24, 11-25]

actuals:
  tokens: 14272
  tasks: 3
  commits: 3

plan_head_before: 0d04fb3e26e2f0c0ff70f6b3b4523a4c128958b0
plan_head_after: 6bd542c085f5ade383790894c00c3881f156f31e

tech-stack:
  added: []
  patterns:
    - "a bound record is checked three ways: the installed tree against each golden file, each golden file's own SHA-256 against the record's machine-read block, and the derived counts, versions, amendment rows and padding holes against the block"
    - "a check's seeded cases are shown RED by blinding the checker one compare at a time in scratch copies, when the checker is complete before its cases are written"
    - "the amendment rule is mechanical: the ABI minor of the installed header must equal 1 plus the number of amendment rows"
    - "the layout probe cache is keyed by header bytes, so a seeded self-test pays one cc probe per distinct header text"

key-files:
  created:
    - docs/abi/FREEZE-CANDIDATE.md
    - docs/abi/header.sha256
    - .planning/phases/11-installed-consumer-and-freeze-candidate/11-23-SUMMARY.md
  modified:
    - scripts/check-abi.py
    - tools/abi/CMakeLists.txt
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase11-regexes.txt
    - docs/spec/api.md
    - docs/abi/installed-undefined.txt
    - .planning/phases/11-installed-consumer-and-freeze-candidate/PIN-IMPACT.md

key-decisions:
  - "header_sha256 of the block binds the manifest golden docs/abi/header.sha256 (one 'sha256  include/<path>' line per installed file, LF-normalised), which the check also recomputes from the installed include tree; so a second installed header or any edit of the one header fails it, and a CRLF copy passes"
  - "the check compares the installed tree with each golden file (symbols, regenerated layout text, class table, manifest) and the record with each golden's own SHA-256, so a coordinated edit of a header and a golden fails on the hash, and a lone golden edit fails on the tree"
  - "the amendment rule is enforced by the check (ABI minor = 1 + amendment rows), so a re-bind cannot be silent: a golden change needs the minor increment in the header and a row in the log"
  - "S-05 is bound, not removed: the probe measures member sizes, the check derives the eight 4-byte holes and the block lists them; explicit reserved members stay a Phase 12 final-bind question and would be an amendment"
  - "the record is wording-checked as a candidate: no stable-ABI promise, no distribution statement, the final bind may raise the major only when no amendment is open"
  - "API-03 is not marked complete: 11-24 and 11-25 also declare it (requirements.ready-ids blocks it) and the final audit is 11-25's"

patterns-established:
  - "FREEZE record form: commits, what the record binds, the S-05 handling, the seeded cases with RED and GREEN, COST and TIMEOUT of each new name, preflight and sweep, observations for the next plan"

requirements-completed: []

coverage:
  - id: D1
    description: "docs/abi/FREEZE-CANDIDATE.md holds a machine-read block binding the ABI candidate 0.1 (package 0.1.0) to the four goldens (header manifest, public symbols, layout pins, status classes) each by its own SHA-256, the counts (35 functions, 39 statuses, 8 stop reasons, 14 structs, 165 members, 190 constants), the amendment count and the eight padding holes; repo.abi-freeze-record recomputes all of it from the installed tree"
    requirement: API-03
    verification:
      - kind: command
        ref: "cmake --build --preset gate && ctest --test-dir build/gate -R '^repo\\.abi-freeze-record$' passes (0.15 to 0.24 s); docs/abi/header.sha256 has one line; the block holds 35, 39 and 8; full sh scripts/gate.sh PASS 328 of 328"
        status: pass
    human_judgment: false
  - id: D2
    description: "every kind of incompatible change fails the freeze check with its own text: removed and reordered struct member, renumbered, unclassified and reclassified status, removed and added public function, edited header comment, second installed header, edited symbol, layout and header golden, edited count and golden hash, ABI minor raised with no amendment row, dropped padding row, missing block; the clean and CRLF controls pass"
    requirement: API-03
    verification:
      - kind: command
        ref: "ctest -R '^repo\\.abi-(freeze-selftest|selftest)$' passes (21 freeze cases, 15 older cases); build/gate/11-23-task2/red.txt (nine blinded checkers, 9 of 9 RED) and green.txt"
        status: pass
    human_judgment: false
  - id: D3
    description: "the record prose states what is and is not bound, the evidence tier (software-interface claim only), the hosted run of the installed-consumer suite, the layout targets and padding holes, the candidate-then-final rule with the amendment steps and the minor increment, the Phase 12 final bind hand-off and an empty amendment log; docs/spec/api.md points at the record"
    requirement: API-03
    verification:
      - kind: command
        ref: "grep -c over the record prints 21 (at least 3); sh scripts/check-docs.sh ok; sh scripts/check-claims.sh --files docs/spec/api.md docs/abi/FREEZE-CANDIDATE.md ok; sh scripts/pii-scan.sh --tree 0 findings; no release, tag or channel word in the record"
        status: pass
    human_judgment: true
    rationale: "whether the wording makes no stable-ABI promise and no distribution statement is a reviewer judgment; 11-25 reads the record in the final audit"

duration: 12min
completed: 2026-11-26
---

# Phase 11 Plan 23: The bound freeze-candidate record and the incompatible-change check Summary

**The freeze candidate (ABI 0.1, package 0.1.0) is bound: `docs/abi/FREEZE-CANDIDATE.md` carries a machine-read block over the LF-normalised header manifest, the 35 public symbols, the layout pins (with the eight S-05 padding holes) and the 39+8 status classes, `repo.abi-freeze-record` recomputes all of it from the installed tree and fails on any difference, and 21 seeded cases show every kind of incompatible change failing with its own text; the record is a candidate only and the final bind is the last plan of Phase 12.**

## Performance

- **Duration:** about 12 min of wall clock for the three tasks (reading 3 min, freeze mode and cases 6 min, prose, records and checks 3 min; the full gate ran 155 s in the background)
- **Tasks:** 3 (Task 1 `tracer`, Task 2 `auto` with `tdd`, Task 3 `auto`)
- **Files:** 7 modified, 2 created plus this summary; no production source, public header, oracle-bound file, `ci.yml` or `gate.sh` changed

## Accomplishments

- **Task 1 (tracer).** `scripts/check-abi.py freeze --record FILE --prefix DIR [--goldens DIR] [--nm-file FILE] [--write]`. It hashes every file below the installed `include/` after CRLF and CR become LF (one `sha256  include/<path>` line per file, the format of `docs/abi/header.sha256`), runs the symbols comparison (`nm` of the installed archive against `public-symbols.txt`), regenerates the layout pins from the installed header with the probe (cached by header bytes) and diffs them with `layout.c`, runs the class check of the installed header against `status-classes.txt`, reads the ABI version from the header and the package version from the installed `tendies64ConfigVersion.cmake`, and compares all of it, each golden's own SHA-256, the counts, the amendment rows and the padding holes with the block. Any difference prints `freeze-record: KEY differs: ...` and the re-bind procedure and exits 1; `--write` rewrites `header.sha256` and the block, and refuses while another golden does not match the tree. The probe also measures `sizeof` of every member (layout.c is regenerated byte for byte unchanged), which gives the holes. Native 0.15 to 0.24 s.
- **Task 2 (seeded cases).** 21 cases in `repo.abi-freeze-selftest`, each on a scratch copy under `build/`: the seven the plan lists (member removed, member reordered, status renumbered, function removed, unclassified status, comment edit, clean) and 14 more: function added, a second installed header, class changed, edited symbol, layout and header goldens, edited count, edited golden hash in the record, ABI minor raised with no amendment row, dropped padding row, missing block, the CRLF control, and two `--write` controls (idempotent over the bound tree; refuses with an unclassified status and leaves record and manifest untouched). RED: the checker is complete from Task 1, so each case was shown to need its compare by blinding the checker one compare at a time in nine scratch copies (manifest, symbols, layout, classes, golden hashes, counts, amendment rule, padding rows, LF normalisation); 9 of 9 fail exactly the cases that target them (`build/gate/11-23-task2/red.txt`); GREEN is the unmodified checker (`green.txt`, 21 of 21, native 0.66 to 0.86 s).
- **Task 3 (record prose).** `docs/abi/FREEZE-CANDIDATE.md`: status as a candidate with the evidence tier, what is bound (table of the four goldens, what each is recomputed from, which names hold it on every gate run) and what is not (the facade header, the runner, state blobs, `t64i_` names, profile behavior, narrow-pointer targets), the evidence (hosted run 45775107983 at `4b13a90`, byte-identical header, only a test registration in `core/CMakeLists.txt` since), the layout targets and the eight padding holes (S-05), the rule between this record and the final bind (evidence, installed-consumer re-run, minor increment, amendment row; the check enforces minor = 1 + rows), the re-bind procedure, the Phase 12 hand-off (the final bind may raise the major only when no amendment is open; no promise that it will), the block, and an empty amendment log. `docs/spec/api.md` points at the record (status line, section 14, section 16). PIN-IMPACT "Freeze steps" has the record hashes, the S-05 handling, RED and GREEN, COST of the two names, the preflight and the sweep.

## Task Commits

1. **Task 1: header hash and the recomputed record** - `95e33dc` (feat)
2. **Task 2: seeded incompatible changes** - `88cd84c` (test)
3. **Task 3: record prose, api.md pointer, Freeze steps** - `6bd542c` (docs)

`commits: 3` in the frontmatter is `git rev-list --count 0d04fb3..6bd542c` (the measured instrument); the summary, STATE and ROADMAP commit follows it.

## Verification

| Check | Result |
|-------|--------|
| Task 1 verify (`cmake --build --preset gate`, `ctest -R '^repo\.abi-freeze-record$'`), tracer gate re-run | pass, 0.15 s (ctest lists 2 tests: `install.setup` joins it) |
| Task 1 acceptance | `header.sha256` has 1 line; the block holds `public_function_count: 35`, `status_count: 39`, `stop_reason_count: 8` |
| Task 2 verify (`ctest -R '^repo\.abi-(freeze-selftest\|selftest)$'`) | pass (3 listed with the fixture): `repo.abi-selftest` 15 cases, `repo.abi-freeze-selftest` 21 cases; every freeze case prints its own expected text (`green.txt`) |
| Task 3 verify (`grep -c` over the record, `ctest -R '^repo\.abi-freeze-record$'`, `sh scripts/preflight-phase11.sh --no-replay`) | 21 (at least 3), pass, PASS in 55 s (22 count rows, 0 mismatches; 16 abi-cheap names; 181 TIMEOUTs, 0 too low, lowest ratio 2.9; 233 units with the act image) |
| `python3 -B scripts/digest-sweep.py --base f9124ae9fce134f31b645c6f2911a6f27fa41b9a --head WORKTREE` | `sweep: 1599 shared lines, 0 differences, 0 head-only lines` |
| `sh scripts/gate.sh` (clean committed tree at `6bd542c`) | PASS, 328 of 328, ctest 94 s, total 155 s (load 6.6 to 12.4; no limit widened); `repo.abi-freeze-record` 0.24 s, `repo.abi-freeze-selftest` 0.86 s in the gate |
| `sh scripts/check-docs.sh`, `sh scripts/check-claims.sh --files` (api.md, the record), `sh scripts/pii-scan.sh --tree` | ok, ok, 0 findings (1431 files) before each docs commit |
| Acceptance of Task 3 | the record states the candidate-then-final rule and has an empty amendment log table; no release, tag or channel word appears in it |

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug in the plan text] The count rows and the ctest totals include the fixture closure**
- **Found during:** Task 1
- **Issue:** the plan says `^repo\.abi` becomes 6 and 7 and the verify expects `Total Tests` of 1 and 2; `ctest -N -R` lists `install.setup` with the first name that needs `FIXTURES_REQUIRED t64_install`, so the measured counts are 7 and 8 (and 2 and 3 in the verifies), as in the plan 11-16 lesson.
- **Fix:** the row is 7 after Task 1 and 8 after Task 2, with a comment naming the closure.
- **Files modified:** `scripts/preflight-phase11-regexes.txt`
- **Commit:** `95e33dc`, `88cd84c`

**2. [Rule 2 - Missing critical functionality] The amendment rule and the padding holes are checked, not only written**
- **Found during:** Task 1 (threats T-11-51, T-11-52 and the S-05 carry-in)
- **Issue:** the plan binds four goldens and counts; T-11-52 ("undocumented amendment") would rest on prose alone, and S-05 asks the layout table to handle per-target offsets and padding.
- **Fix:** the block carries `amendments` and the check requires the ABI minor of the installed header to equal 1 plus the number of rows of the amendment log (seeded case `freeze-amendment-missing`); the probe measures member sizes, the check derives the eight holes and binds them as `padding_hole` rows (seeded case `freeze-padding-record-edited`); the record says which targets the pins hold on.
- **Files modified:** `scripts/check-abi.py`, `docs/abi/FREEZE-CANDIDATE.md`
- **Commit:** `95e33dc`, `88cd84c`, `6bd542c`

### Process deviations

**3. RED for Task 2 is a blinded checker**
- **What:** the plan says "save red.txt (cases before the freeze mode handled each)". The freeze mode is complete from Task 1, so the cases cannot run before it. Task 1 was committed without the case section (a trimmed `check-abi.py`) so the two commits stay separable, and RED is nine scratch copies of the checker with one compare blinded each, which fail exactly the cases that target that compare (`build/gate/11-23-task2/red.txt`). The driver is a local script and is not committed.

**4. The three-language compile of `layout.c` is not repeated in freeze mode**
- **What:** the plan says "run the symbols, layout and classes checks against the installed prefix". Freeze mode runs their comparisons (nm against the golden, the regenerated layout text against `layout.c`, the class check against the installed header); the C99, C11 and C++17 compile of the pins stays in `repo.abi-layout` and `install.abi-layout` on the same prefix. Equality of the regenerated text with the golden is the stronger statement, and the compile would cost 0.2 s more per case.
- **Files modified:** none.

**5. One file outside the plan's list**
- **What:** `docs/abi/installed-undefined.txt` (comment only: the Linux rows are confirmed by hosted run 45775107983), as the orchestrator's note for 11-23 asked; the rows are unchanged. The plan's list also names no `docs/abi/layout.c` edit, and none was made (the file regenerates unchanged).
- **Commit:** `6bd542c`

**Total deviations:** 5 recorded (1 plan-text, 1 added check, 3 process). **Impact:** none on the delivered evidence.

## Authentication Gates

None.

## Issues Encountered

- Host load rose to 12 during the digest sweep and the gate (unrelated VM/Docker load); nothing was rerun and no limit widened.
- The hosted pair for the commits of this plan has not run (nothing is pushed); the first hosted run of `repo.abi-freeze-record` and `repo.abi-freeze-selftest` on Linux (gcc 13.3, cmake 3.31) comes with the next push. The record is checked on the same layout, symbol and package-file inputs that `install.abi-layout`, `install.abi-symbols` and the installed consumer already pass there; a mismatch would be a finding for a new numbered plan.

## Known Stubs

None.

## Threat Flags

None. T-11-51 (a silent ABI change after the candidate bind) is mitigated by `repo.abi-freeze-record` with the 21 seeded cases; T-11-52 (an undocumented amendment) by the minor-equals-1-plus-rows rule of the check and the amendment steps of the record.

## Notes for the orchestrator

- **Nothing is pushed.** Everything after `4b13a90` is local: the four 11-21 record commits, plan 11-22's commits and its summary, and this plan's `95e33dc`, `88cd84c`, `6bd542c` plus the summary commit. Run `sh scripts/pii-scan.sh --range origin/main..HEAD` before the next push. No orchestrator-only or owner step is open in this plan.
- **For 11-24 and 11-25.** From this head any change of the header, the public symbol set, a layout or a class row fails `repo.abi-freeze-record`; 11-24 (qualifier) and 11-25 (records) must change no bound input. 11-25: the FREEZE ledger row adds `unit.core.state.v26-z-product-bound`, `repo.installed-case-map` (11-22) and `repo.abi-freeze-record`, `repo.abi-freeze-selftest` (this plan, 0.86 s of COST), 328 names in the gate, with the measured cold run; compare the api.md postcondition table with the tests (T-11-49); read the record wording in the final audit (D3 is a judgment).
- **Requirements.** API-03 is not marked complete: 11-24 and 11-25 also declare it (`requirements.ready-ids` reports it blocked); the record and its check are this plan's delivery, the final audit is 11-25's.
- **Phase 12 hand-off.** The final bind either confirms the candidate or lists amendments; replacing the eight padding holes with explicit reserved members is an open final-bind question recorded in the record (it would be an amendment).

## Self-Check: PASSED

- `docs/abi/FREEZE-CANDIDATE.md`, `docs/abi/header.sha256` and this summary exist (the summary is committed by the commit that follows this check)
- commits `95e33dc`, `88cd84c` and `6bd542c` are ancestors of HEAD
- `ctest --test-dir build/gate -R '^repo\.abi-(freeze-record|freeze-selftest|selftest)$'` passes; the full gate passed at `6bd542c`
