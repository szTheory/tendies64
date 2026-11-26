---
phase: 12-minimal-host-playable-homebrew-and-final-bind
plan: 13
subsystem: abi-bind-preparation
tags: [abi, freeze-record, check-abi, selftest, bind-staging, pin-site-map, multi-line-proof]

requires:
  - phase: 12-12
    provides: the Bind base 49d41ac and the closed HOST review (no blocking finding, no amend row, "12-11 trigger: no")
  - phase: 11-23
    provides: the freeze candidate record, `check-abi.py freeze` and its selftest at ABI 0.1
provides:
  - scripts/check-abi.py rule generalized by the optional block keys `bound_minor` and `candidate_amendments` (bound record) beside the unchanged candidate rule; `freeze --write` keeps both keys and refuses a lone one
  - 16 more freeze selftest cases (21 became 37), green on the unchanged ABI 0.1 tree and, in a mock bound ABI 1.0 tree, with no change
  - PIN-IMPACT "Bind pin-site map" re-measured at the Bind base header (13 "provisional" lines, 15 text sites, multi-line aware proof commands with their values) and "Bind staging" (10 files, exception reason, proof and resume rule, owner confirmation)
affects: [12-14, 12-15, 12-16, 12-17, 12-18]

actuals:
  tokens: 9000
  tasks: 3
  commits: 3
plan_head_before: 85b8085b62e4fa95c522a1833020e64f47076b1d
plan_head_after: 7d594272858f87d4a2d6eb3e14241425d670c59b

tech-stack:
  added: []
  patterns:
    - "a rule selftest seeds its own rebound scratch copy (header version edited, layout.c regenerated, log rows set, --write, bound keys put back last), so a case never depends on whether the committed record is a candidate or a bound record"
    - "a wanted text with a {name} placeholder that the seed fills in (the minor raised by one), so a seed needs no hard-coded anchor of the header"
    - "proof commands for a wrapped phrase: joined text through tr and a two-rule sed, with both the line-count form and the occurrence form recorded"

key-files:
  created:
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-13-SUMMARY.md
  modified:
    - scripts/check-abi.py
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/PIN-IMPACT.md

key-decisions:
  - "Block rule: with both keys present the ABI minor must equal bound_minor + (rows - candidate_amendments); with neither, minor == 1 + rows with the unchanged message; a lone or non-decimal key, or a log shorter than candidate_amendments, is its own finding (the bound messages name 'bound minor' or both keys)"
  - "Canonical place of the keys: directly after the `amendments:` line; `freeze --write` re-emits them there byte for byte, and refuses (exit 1, nothing written) while only one is present"
  - "The Bind staging set is exactly the 10 files of the plan; the B-34 relabel (a test comment) is left out on purpose so that the 12-14 commit stays at 10 files"
  - "No ABI byte moved: core/include, docs/abi, every CMake file, the consumer, host/ and the controls are unchanged from the Bind base"

patterns-established:
  - "an executor that must edit the record's block at the bind inserts the two bound lines by hand and runs --write; the writer keeps them"

requirements-completed: []

coverage:
  - id: D1
    description: "check-abi.py holds both the candidate rule and the bound rule; both-or-neither keys, pre-bind rows not counted, key preservation by --write, generalized amendment-missing seed"
    requirement: "API-05"
    verification:
      - kind: integration
        ref: "ctest -R '^repo\\.abi' 8 of 8 on the unchanged ABI 0.1 tree; ctest -L repo 28 of 28; freeze selftest cases=37 failed=0 here and in a mock bound ABI 1.0 tree"
        status: pass
      - kind: other
        ref: "six rule mutants of the new code, each killed by a named case (M1 to M6 in the mutation spot check)"
        status: pass
    human_judgment: false
  - id: D2
    description: "Bind pin-site map re-measured with multi-line aware proofs and the line '- Header provisional lines at the Bind base: 13'"
    requirement: "API-06"
    verification:
      - kind: integration
        ref: "the plan's Task 3 verify (recorded count equals the live count, section present, 10 rows) exit 0; proof commands 1 to 7 of the section run verbatim: 13, 1, 14, 13, 12, 29/17/15, 11 files"
        status: pass
    human_judgment: false
  - id: D3
    description: "Bind staging lists the 10 files of the coupled bind commit with the exception reason, proof and resume rule and the owner confirmation of ABI 1.0 / package 1.0.0 as an ABI identity"
    requirement: "API-06"
    verification:
      - kind: integration
        ref: "awk count of '| S-' rows inside '## Bind staging' = 10; the OWNER CONFIRMED line present"
        status: pass
    human_judgment: false

duration: 12min
completed: 2026-11-26
status: complete
---

# Phase 12 Plan 13: Bind Preparation - Generalized Freeze Minor Rule, Fresh Pin-Site Measurement, Bind Staging Summary

**`scripts/check-abi.py` now holds a bound record (block keys `bound_minor` and `candidate_amendments`: minor == bound_minor + rows - candidate_amendments) beside the unchanged candidate rule, `freeze --write` keeps both keys, 16 new selftest cases (37 in all, all green here and in a mock ABI 1.0 tree) cover both rules, and PIN-IMPACT carries the Bind pin-site map re-measured with multi-line aware proofs (13 "provisional" lines, 15 text sites) and the fixed 10-file Bind staging list, with no ABI byte moved.**

## Performance

- **Duration:** about 12 min of execution
- **Started:** 2026-11-26T22:51:50Z
- **Completed:** 2026-11-26T23:03Z
- **Tasks:** 3 of 3
- **Files modified:** 2 (`scripts/check-abi.py`, `PIN-IMPACT.md`) plus this summary

## Accomplishments

- Task 1 (tracer): the bound rule runs end to end through the checker's selftest. A rebound-scratch seed turns the scratch copy of the committed tree into a candidate or a bound record at any ABI version, so the first three cases (candidate clean, bound clean at ABI 1.0, bound minor raised to 1.1 with no row) did not need the committed record to change.
- Task 2: both-or-neither keys (a lone key is a finding naming both), decimal check, pre-bind rows not counted (a record with `candidate_amendments: 1` and one dated row passes at minor 0 and fails at minor 1), a log shorter than `candidate_amendments` is a finding, `--write` keeps both keys byte for byte and refuses a lone one, and the `freeze-amendment-missing` seed raises whatever minor the header holds by one (no `UINT32_C(1)` anchor; at minor 0 it writes 1).
- Task 3: the Bind pin-site map was re-measured. The counts equal the planning values (no header or outside-header site moved after the Wave 0 base); rows B-23, B-28 and B-33 got their new lines, B-26 and B-27 are marked done, B-36 and B-37 are new observation rows, and the proof commands are recorded with their values at HEAD. "Bind staging" fixes the 10 files of the 12-14 commit.

## Task Commits

1. **Task 1: bound freeze minor rule tracer** - `129f314` (feat)
2. **Task 2: generalized freeze minor rule** - `2f177d3` (feat)
3. **Task 3: pin-site map and Bind staging (PIN-IMPACT.md only)** - `7d59427` (docs, `[skip ci]`)

Plan metadata: the SUMMARY, STATE and ROADMAP commit follows this file.

## RED reports (first failures recorded, as the plan requires)

Task 1, three cases added before any rule change (`build/12-13/task1-red.log`):

- `freeze-candidate-clean`: ok (the control: the candidate rule already held).
- `freeze-bound-clean`: FAILED: freeze mode exited 1, wanted 0 (`check-abi: freeze findings=3`; the block lines `bound_minor` and `candidate_amendments` were reported as "record malformed: unknown key").
- `freeze-bound-minor-raised`: FAILED: output lacks the "ABI minor 1 ... after the bind (the bound minor is 0)" text; the output held `freeze-record: record malformed: unknown key: bound_minor`.

Task 2, thirteen cases added (11 seeded plus 2 special write cases; `build/12-13/task2-red.log`), 5 of 37 failed first:

- `freeze-bound-rows-below-candidate`: the minimal rule printed "the log has -1 after the bind" instead of the `candidate_amendments` finding.
- `freeze-bound-minor-alone` and `freeze-bound-candidate-alone`: freeze mode exited 0, wanted 1 (a lone key fell through to the candidate rule).
- `freeze-bound-write-keeps-keys`: "--write dropped or changed the block line 'bound_minor: 0'".
- `freeze-bound-write-refuses-partial`: "--write over a lone bound_minor exited 0".

The other new cases (candidate minor 2 with and without a row, bound post-bind row, row without minor, pre-bind row not counted, pre-bind row paying nothing, non-decimal key, `freeze-amendment-missing-bound`) passed at once because the Task 1 rule already covered them; the generalized `freeze-amendment-missing` seed is test-side only.

## Mutation spot check (not committed)

A copy of the checker with one rule edit, run against the freeze selftest, each killed:

| Mutant | Killed by |
|--------|-----------|
| M1 pre-bind rows counted (`after = rows`) | freeze-bound-candidate-row-not-counted, -pays-nothing, freeze-bound-write-keeps-keys |
| M2 no "log shorter than candidate_amendments" finding | freeze-bound-rows-below-candidate |
| M3 lenient minor (`>` for `!=`) | freeze-bound-row-without-minor |
| M4 `--write` drops the bound keys | freeze-bound-write-keeps-keys |
| M5 candidate rule applied to a bound record | 9 bound cases |
| M6 lone key ignored | crash (KeyError) of the selftest, so the run fails |

## Files Created/Modified

- `scripts/check-abi.py` - `BOUND_KEYS`, `bound_numbers`, `minor_findings`, `block_values(bound=...)`, `write_freeze` key preservation, the docstring, and the selftest additions (`header_with_version`, `record_with_bound`, `record_with_rows`, `seed_rebound`, `seed_minor_raised`, 13 rule cases, 2 special write cases)
- `.planning/phases/12-minimal-host-playable-homebrew-and-final-bind/PIN-IMPACT.md` - "Bind pin-site map" counts, B-23, B-26 to B-28, B-33, B-36 and B-37 rows and the proof commands; new section "Bind staging"

## Decisions Made

See the frontmatter. In short: the rule text names "bound minor" in every bound mismatch; the keys live after `amendments:`; the staging set stays at exactly 10 files.

## Deviations from Plan

### Auto-fixed Issues

None (no defect was found in existing code).

### Rule 2 additions (hardening of the planned threat T-12-52)

- The checker also reports a non-decimal `bound_minor` or `candidate_amendments` and a log with fewer dated rows than `candidate_amendments` (otherwise the second reads as a negative row count and the first would crash). Cases: `freeze-bound-not-decimal`, `freeze-bound-rows-below-candidate`, `freeze-bound-candidate-row-pays-nothing` (a pre-bind row must not pay for a minor raised after the bind).
- `freeze --write` refuses (exit 1, nothing written) while the block holds only one bound key. Case: `freeze-bound-write-refuses-partial`.

### Other deviations (recorded for transparency)

- **Commit message count.** The Task 2 commit message says "15 more cases"; that commit added 13 (11 seeded and 2 special) and Task 1 added 3, 16 in all (the count in PIN-IMPACT B-27 and here). History is not rewritten.
- **COST of `repo.abi-freeze-selftest`.** It rose from 0.77-0.93 s to about 1.1 s standalone (1.0 to 1.6 s under host load 6 to 7) against COST 0.7 in `tools/abi/CMakeLists.txt`; the plan prohibits any CMake edit, so the figure is only reported (a scheduling hint, TIMEOUT 60 leaves a wide margin, hosted growth about +1.5 s). A later plan that may edit `tools/abi/CMakeLists.txt` can raise the COST.
- **Plan numbering.** The 12-13 plan names the scratch-clone proof "12-14 Task 3"; in the 12-14 plan it is Task 4. DR-25 and the 12-14 frontmatter count 11 files because they include PIN-IMPACT; the bind commit holds the 10 of "Bind staging". Both are stated in that section.
- **B-34 left out of the staged set.** The "minor below the library's" wording of three test files is a comment edit that would take the bind commit to 13 files and break the exact-10 verify of 12-14; recorded in "Bind staging" (carry in 12-15 or 12-16 only if wanted).
- **Observation rows B-36 and B-37.** `scripts/check-host-policy.sh` line 387 (a seeded `find_package(tendies64 0.1 ...)` line, never configured) and the Phase 11 qualifier and its hermetic test (scratch records at 0.1) were found while measuring the version pins; neither needs an edit.
- **Branch.** Commits were made on `main` as in every earlier sequential plan; nothing was pushed.

## Authentication Gates

None.

## Issues Encountered

None. Host load stayed at 6 to 7; no limit was widened.

## Verification Results

| Check | Result |
|-------|--------|
| Task 1 verify `ctest -R '^repo\.abi-freeze-(record\|selftest)$'` | 3 of 3 (install.setup fixture included) |
| Task 2 verify `ctest -R '^repo\.abi'` and `git diff --quiet HEAD -- core/include docs/abi core/CMakeLists.txt tests/installed-consumer host` | 8 of 8; no diff |
| Plan check: zero diff of `core docs/abi docs/spec tests host runner tools fixtures CMakeLists.txt` and of `docs/abi/header.sha256` and the record between the Bind base and HEAD | none |
| `ctest -L '^repo$'` | 28 of 28 |
| `sh scripts/preflight-phase12.sh --no-replay` (after Task 2 and after Task 3) | PASS (14 s each) |
| `sh scripts/pii-scan.sh --tree`, `sh scripts/rights-scan.sh --tree` | 0 findings |
| `ctest -R 'consumer\.literals\|repo\.api07-evaluation\|repo\.installed-case-map\|repo\.stale-freeze'` (readers of PIN-IMPACT) | 7 of 7 |
| Task 3 verify (recorded count equals live count 13, section present, 10 `S-` rows) | exit 0 |
| `python3 -B scripts/check-abi.py --freeze-selftest` in a mock bound tree (header ABI 1.0, record with `bound_minor: 0` and `candidate_amendments: 0` written by `--write`) | cases=37 failed=0, `freeze findings=0` |
| Full `scripts/gate.sh` | not run: the plan changes one script and one record, covered by the names above |

## Handoff for 12-14

- The committed record has no bound keys yet. 12-14 Task 3 inserts `bound_minor: 0` and `candidate_amendments: 0` (the 12-11 rows, none) into the block and then runs `check-abi.py freeze --write`; the writer keeps them and places them directly after `amendments:`. The mock run above shows the whole freeze selftest passes unchanged in that world, so no further selftest edit is needed at the bind.
- The proof commands of "Bind pin-site map" (values 13, 1, 14, 13, 12, 29/17/15, 11 files at HEAD) must print 0 for the header after 12-14 and 12-15; command 7 may keep naming `scripts/check-stale-freeze-text.sh` only.
- COST of `repo.abi-freeze-selftest` (see Deviations) is open for a plan that may edit the CMake file.

## Known Stubs

None.

## Threat Flags

None. T-12-52 is mitigated by the 16 seeded cases and the six-mutant check (both rules, both-or-neither keys, pre-bind rows not counted, green on the committed candidate); T-12-53 by the fresh measurement and the joined-text proofs with both the line count and the occurrence count, with no scan path narrowed.

## Self-Check: PASSED

- FOUND: `scripts/check-abi.py` contains `bound_minor` (27 lines) and `candidate_amendments` (21 lines); PIN-IMPACT "Bind staging" with 10 `S-` rows and the `- Header provisional lines at the Bind base: 13` line
- FOUND commits: 129f314, 2f177d3, 7d59427 (ancestors of HEAD)
