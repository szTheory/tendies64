---
phase: 11-installed-consumer-and-freeze-candidate
plan: 24
subsystem: qualification
tags: [freeze-wave, qualifier, fresh-archive, hermetic-test, rehearsal, build-identity, evidence-docs-unbound]
status: complete

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: the bound freeze-candidate record and four goldens (11-23), the sanitizer and TSan lanes of check-install.sh (11-20), the installed consumer (11-16 to 11-19), the digest sweep with the Wave 0 base, and the Phase 10 qualifier and hermetic test as the template
provides:
  - scripts/check-install.sh fresh-archive, a LOCAL or manual lane (no CTest name, not in ci.yml) that builds a git archive of HEAD outside the repository as the gate preset configures it, installs only t64_core into a prefix with a space and runs the consumer smoke and composite cases against it
  - scripts/qualify-phase11.py, a thin delta of the Phase 10 qualifier with a twelve-stage table, four base labels, claims with final_abi and release false, the installed archive and consumer build ids, the freeze-record binding and the two evidence documents excluded from the bound inputs
  - scripts/tests/test_phase11_qualification.py and CTest harness.phase11-qualification (25 cases, COST 8, TIMEOUT 150, label phase11)
  - one full rehearsal that passed and verified (record 20261126T084217919248Z-dedb2052, 614 s) with every stage's seconds against its limit in PIN-IMPACT "Freeze steps"
affects: [11-25]

actuals:
  tokens: 38310
  tasks: 3
  commits: 5

plan_head_before: 5a9f5269a09b08f7b6ce3b3202faec9cf4fd9bfd
plan_head_after: 25acb55a879f1a65d5ecdc9702030095e67606c9

tech-stack:
  added: []
  patterns:
    - "a qualifier is a thin copy of the previous phase's: constants, the stage table, the artifact list and one extra binding change; the helpers stay a verbatim copy"
    - "a stage's own output is checked against the record it belongs to: the fresh-archive lane's archived commit equals the record commit, its build id equals the installed archive's, the freeze summary line equals the machine-read block"
    - "documents that cite a record are excluded from the bound inputs by name, so writing them after the record never invalidates it (W-9)"

key-files:
  created:
    - scripts/qualify-phase11.py
    - scripts/tests/test_phase11_qualification.py
    - .planning/phases/11-installed-consumer-and-freeze-candidate/11-24-SUMMARY.md
  modified:
    - scripts/check-install.sh
    - runner/CMakeLists.txt
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase11-regexes.txt
    - .planning/phases/11-installed-consumer-and-freeze-candidate/PIN-IMPACT.md

key-decisions:
  - "the installed suite stage is the whole phase11 label with JUnit (75 names, equal to the PHASE11_TESTS block), preceded by the cheap abi-cheap stage whose first command is an incremental gate build, so the cheap stages never run against a stale tree"
  - "the fresh-archive lane keeps its installed prefix and consumer under build/gate/fresh-archive (ignored) for the qualifier to read, and removes the exported source and build tree (mktemp, outside the repository) at exit"
  - "the qualifier requires core, installed_core and installed_consumer to carry one build id: the fresh archive was configured exactly as the gate, and the id hashes content and build policy, not paths (measured equal)"
  - "claims are compared as the JSON value false itself (a plain dict comparison accepts 0); this is fixed in the Phase 11 copy only, the closed Phase 10 qualifier is untouched"
  - "the table keeps the earlier measured constants although three stages ran 20 to 25 percent higher in the rehearsal; every limit still holds 3.6 times the observed seconds, and a refresh is a commit before the Qualification base (recorded in PIN-IMPACT for 11-25)"
  - "VER-05 and API-03 are not marked complete: 11-25 also declares them (requirements.ready-ids blocks them) and the final audit and run are 11-25's"

patterns-established:
  - "FREEZE record form for a qualifier plan: commits, the lane, the identity facts, the qualifier delta, the defect the new case found, the hermetic test with RED and GREEN, the new name with COST and TIMEOUT, gate growth, preflight, the rehearsal record id with host load, the stage table with seconds against limit, observations for the final run"

requirements-completed: []

coverage:
  - id: D1
    description: "check-install.sh fresh-archive builds git archive HEAD outside the repository as the gate preset configures it, only t64_core, installs the Development component into a prefix with a space, asserts the six-file package and that no installed byte names the exported tree, its build tree, the repository or a per-user path, builds the independent consumer against it and runs smoke (13 checks) and composite (25 checks); the installed archive carries the gate's build id"
    requirement: VER-05
    verification:
      - kind: command
        ref: "sh scripts/check-install.sh fresh-archive (3.1 to 4 s; prints 'fresh-archive ok'); build ids of build/gate/core/libt64_core.a, the installed archive and the consumer executable are one value"
        status: pass
    human_judgment: false
  - id: D2
    description: "scripts/qualify-phase11.py binds the files changed since the Wave 0 base plus 21 tool paths (the freeze record, its four goldens, the lanes and the qualifier), the installed archive and consumer identity and the four golden hashes, refuses a dirty tree, reads four 40-hex base labels, keeps tier 'unit-tested, authored scope' with hardware_accuracy, oracle_comparison, final_abi and release false, and runs twelve stages each with a measured value and a limit of at least twice it"
    requirement: API-03
    verification:
      - kind: unit
        ref: "scripts/tests/test_phase11_qualification.py#stage-table-shape, #tier-and-claims-pinned, #claims-pinned, #artifact-identity-and-freeze-binding, #bases-refuse-head-short-duplicate-missing, #dirty-tree-refuses"
        status: pass
    human_judgment: false
  - id: D3
    description: "harness.phase11-qualification runs 25 hermetic cases in scratch git repositories (never a nested CTest): refusals for head, short, duplicate and missing bases, dirty tree, changed input, rehearsal as final, failed and missing stages, tier and claims, the evidence documents unbound while every other document is bound, the lane output rules and real stage commands starting"
    requirement: VER-05
    verification:
      - kind: unit
        ref: "ctest --test-dir build/gate -R '^harness\\.phase11-qualification$' (8.46 s); python3 -B scripts/tests/test_phase11_qualification.py --list-cases prints 25 names; sh scripts/check-test-inventory.sh --preset gate PASS count=329"
        status: pass
    human_judgment: false
  - id: D4
    description: "a full rehearsal of the twelve-stage table passed (gate 329 of 329, sanitizers against the shipped and the instrumented library, TSan, fresh archive, sweep 1599 shared lines and 0 differences, freeze check) and verified with --verify RECORD --rehearsal; the same record is refused as final"
    requirement: VER-05
    verification:
      - kind: command
        ref: "python3 -B scripts/qualify-phase11.py --native --rehearsal (record 20261126T084217919248Z-dedb2052, 614.07 s) then --verify RECORD --rehearsal exit 0; --verify RECORD without --rehearsal exit 1 'refused as final'"
        status: pass
    human_judgment: false
  - id: D5
    description: "whether a 20 to 25 percent gap between the table's measured constants and the rehearsal seconds of three stages should be refreshed before the Qualification base is a reviewer judgment (no limit is near its deadline)"
    verification: []
    human_judgment: true
    rationale: "the constants are derivation text, not limits; whether a refresh is worth a bound-input change before the final run is an audit decision for 11-25"

duration: 23min
completed: 2026-11-26
---

# Phase 11 Plan 24: The Phase 11 qualifier, the fresh-archive lane and the rehearsal Summary

**`scripts/qualify-phase11.py` (a thin delta of the Phase 10 qualifier) runs twelve stages that bind the files changed since the Wave 0 base, the installed archive's build id (equal to the gate's), the four freeze-record golden hashes and every local lane, including the new `check-install.sh fresh-archive`; `harness.phase11-qualification` tests its refusals in 25 hermetic cases, and one full rehearsal passed and verified in 614 s.**

## Performance

- **Duration:** about 23 min of wall clock for the three tasks after the reading (the full rehearsal ran 614 s in the background)
- **Started:** 2026-11-26T08:31:46Z
- **Completed:** 2026-11-26T08:55Z
- **Tasks:** 3 (Task 1 `tracer`, Task 2 `auto` with `tdd`, Task 3 `auto`)
- **Files:** 5 modified, 2 created plus this summary; no production source, public header, bound golden, oracle-bound file, `ci.yml` or `gate.sh` changed; nothing pushed

## Accomplishments

- **Task 1 (tracer).** `sh scripts/check-install.sh fresh-archive` exports `git archive HEAD` (1434 files, no `.git`) into a `mktemp -d` directory outside the repository, configures it as the gate preset does (Ninja, Release, `T64_WERROR=ON`, `-Wdev -Werror=dev --warn-uninitialized`), builds only `t64_core` (3 s), installs the Development component into `build/gate/fresh-archive/prefix with space`, asserts the six-file package, scans every installed byte for the exported tree, its build tree, the repository and per-user locations (`check-abi.py abspaths`), builds the consumer against that prefix and runs smoke (13 checks) and composite (25 checks). The fresh archive carries the same build id as the gate's archive (`t64-build-v1-35e6e1f1...22d5`; the id hashes content and build policy, not paths), and the consumer executable embeds exactly that one id. The qualifier frame (copy of `qualify-phase10.py`) ran two stages and its rehearsal record verified (`⚡ Tracer verified end-to-end - expanding`).
- **Task 2 (full table and the hermetic test).** Twelve stages in the order ci-scanner, inventory, oracle, abi-cheap, installed (`ctest -L phase11` with JUnit, 75 names against the `PHASE11_TESTS` block), gate, sanitize-shipped, sanitize-instrumented, tsan, fresh-archive, sweep (Wave 0 base, `SWEEP_SHARED_LINES` 1599) and freeze (`check-abi.py freeze` on the prefix the fresh-archive stage installed), each with a measured value and a limit of at least twice it, enforced by the test. Lane output is parsed and tied to the record (every case of `cases.txt` ran, the instrumented side is proven by runtime references, the archived commit equals the record commit, the printed freeze summary equals the machine-read block). The record also binds the freeze record and the SHA-256 of its four golden files, which must equal the block. The two evidence documents `docs/claims/phase11.md` and `docs/evidence/phase11-qualification.md` join the inherited exclusions (`.planning/`, `docs/ci-reviews/LOG.md`); editing them never refuses a record, editing any other document or bound input does. `harness.phase11-qualification`: 25 cases, COST 8, TIMEOUT 150, label phase11, `PHASE11_TESTS` 75 names, regex row 1, inventory 329 names.
- **Task 3 (rehearsal).** `python3 -B scripts/qualify-phase11.py --native --rehearsal` passed and `--verify RECORD --rehearsal` verified (record `20261126T084217919248Z-dedb2052`, 188 bound inputs, 75 inventory names, 614.07 s, host load 5.91 to 7.63); PIN-IMPACT "Freeze steps" lists every stage's seconds against its limit, the record id and the host load.

## Task Commits

1. **Task 1: fresh-archive lane and a two-stage qualifier frame** - `93efcd8` (feat), `822500a` (fix: the tracer tool list)
2. **Task 2: hermetic cases, RED** - `292b63c` (test); **full stage table, test registration, inventory and regex rows, GREEN** - `dedb205` (feat)
3. **Task 3: rehearsal record and stage seconds** - `25acb55` (docs)

`commits: 5` in the frontmatter is `git rev-list --count 5a9f526..25acb55` (the measured instrument, from the persisted ledger); the summary, STATE and ROADMAP commit follows it.

## Verification

| Check | Result |
|-------|--------|
| Task 1 verify (`sh scripts/check-install.sh fresh-archive`, then `qualify-phase11.py --native --rehearsal` and `--verify RECORD --rehearsal`) | pass: fresh-archive ok in 3 s; the tracer record `20261126T083650266129Z-822500a4` names `installed_core` and `installed_consumer` with `t64-build-v1-35e6e1f1...22d5` and verifies (exit 0) |
| Task 2 verify (`cmake --build --preset gate`, the selftest, `ctest -R '^harness\.phase11-qualification$'`, `check-test-inventory.sh --preset gate`) | pass: 25 of 25 in 7.9 s, ctest 1 of 1 in 8.46 s, inventory PASS count=329; `--list-cases` prints 25 names (at least 23) |
| RED and GREEN of Task 2 | `build/gate/11-24-task2/red.txt`: three failures against the two-stage frame (`stage-table-shape`, `real-stage-commands-start`, `claims-pinned`); `green.txt`: 25 of 25 |
| Task 3 verify (`sh scripts/preflight-phase11.sh --no-replay`, the native rehearsal) | preflight PASS in 45 s and again in 42 s after the PIN-IMPACT commit (16 abi-cheap names, head-ci green on `4b13a90`, 233 units under the act image, pii 0 findings); the native rehearsal is the run recorded: 12 stages ok, exit 0 |
| Rehearsal verify at the later head `25acb55` | `--verify RECORD --rehearsal` exit 0 (PIN-IMPACT is excluded); `--verify RECORD` without `--rehearsal` exit 1, 'a rehearsal record is not final evidence; it is refused as final' |
| Gate inside the rehearsal (clean committed tree at `dedb205`) | `scripts/gate.sh` PASS, 329 of 329, ctest wall 86.25 s, total 141 s at load 5.9 to 7.6; the sweep printed `sweep: 1599 shared lines, 0 differences, 0 head-only lines` |
| Native seconds of the new name | `harness.phase11-qualification` 8.46 s alone, 8.77 s in the gate (`COST 8`) |
| `sh scripts/pii-scan.sh --tree` before each commit that carries documentation | 0 findings (1434 files) |

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug in the copied code] The claims comparison accepted `release: 0`**
- **Found during:** Task 2 (the new case `claims-pinned`, RED run)
- **Issue:** the Phase 10 qualifier compares `record["claims"] == CLAIMS`; in Python `0 == False`, so a record that says `"release": 0` or `"final_abi": 0` was accepted as a claim of false.
- **Fix:** the Phase 11 verify requires the same key set and every value `is False`; `claims-pinned` covers true, dropped, extra and `0` values. The closed Phase 10 qualifier and its test were not edited.
- **Files modified:** `scripts/qualify-phase11.py`
- **Verification:** RED in `build/gate/11-24-task2/red.txt`, GREEN 25 of 25
- **Committed in:** `dedb205`

**2. [Rule 3 - Blocking] The tracer's tool list named a test file that did not exist yet**
- **Found during:** Task 1 (first `--native --rehearsal`: "bound source input is absent")
- **Issue:** the copied tool list already included `scripts/tests/test_phase11_qualification.py`, which Task 2 creates.
- **Fix:** the tracer commit lists only files that exist; the Task 2 commit adds the test file to the bound tools.
- **Files modified:** `scripts/qualify-phase11.py`
- **Committed in:** `822500a`, `dedb205`

### Plan text read, not deviations

- The plan's stage list names "installed suite (install, installed, api04, repo.abi names)"; the table runs the whole `phase11` label with JUnit (75 names), which contains all of them and is checked against `PHASE11_TESTS`, and the cheap `abi-cheap` stage runs the regex of the preflight.
- Beyond the plan text, the fresh-archive lane asserts the six-file package and runs the absolute-path scan, and the qualifier ties the lane's own output (archived commit, build id, freeze summary, sanitizer runtime references, TSan digests) to the record; these are additions that cost nothing at run time.

---

**Total deviations:** 2 auto-fixed (1 bug in a copy, 1 blocking)
**Impact on plan:** both fixes were needed for a correct qualifier; no scope growth, no change to any bound input of Phase 11.

## Issues Encountered

- A stray `scripts/__pycache__/qualify-phase11.cpython-314.pyc` (an interpreter import without `-B`) was untracked at the first commit and made `sh scripts/pii-scan.sh --tree` report one finding after the commit; it was never staged, was removed and the scan was clean from then on. Every later run used `python3 -B`.
- Three stages ran above the table's measured constants in the rehearsal (installed 11.02 s against 8.8, sanitize-shipped 73.59 s against 59, sanitize-instrumented 328.46 s against 299); all limits hold at least 3.6 times the observed seconds, so nothing was widened and nothing was re-run. Recorded for 11-25 in PIN-IMPACT.

## Authentication Gates

None.

## Known Stubs

None.

## Threat Flags

None: the plan's register is covered by the delivered mitigations and the new lane adds no network endpoint or trust boundary (T-11-53: the bound inputs, tool paths, installed archive identity and golden hashes, with verify refusing a changed input; T-11-54: raw logs stay under `build/gate/11-qualification-private`, the committed record text carries no workstation path; T-11-55: a rehearsal is labelled and refused as final, covered by `verify-refuses-rehearsal-as-final`; T-11-SC: no package install).

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 11-25 can write the Qualification base, run the audit and the final `--native` run (no `--rehearsal`) and then `--verify RECORD`; the two evidence documents are the only files it may write after the record; the freeze record, the four goldens, `check-install.sh`, `check-abi.py`, the qualifier and its test are bound inputs now (any edit is a bound-input change and must precede the Qualification base).
- 11-25 also owns the push, the hosted pair, the cold gate-macos measurement and the FREEZE ledger row (the names of 11-22, 11-23 and `harness.phase11-qualification`: 8 s of COST).
- Unpushed since `4b13a90`: the 11-21 records and the commits of 11-22, 11-23 and 11-24.

---
*Phase: 11-installed-consumer-and-freeze-candidate*
*Completed: 2026-11-26*

## Self-Check: PASSED

Created files exist (`scripts/qualify-phase11.py`, `scripts/tests/test_phase11_qualification.py`, this summary); modified `scripts/check-install.sh` carries the `fresh-archive` subcommand; commits `93efcd8`, `822500a`, `292b63c`, `dedb205` and `25acb55` are ancestors of HEAD; `sh scripts/pii-scan.sh --tree` reports 0 findings.
