---
phase: 11-installed-consumer-and-freeze-candidate
plan: 22
subsystem: docs-and-testing
tags: [freeze-wave, api-contract, additive-rule, carry-ins, w2-01, w1-03, d-10-17-01, t8, case-map]
status: complete

requires:
  - phase: 11-installed-consumer-and-freeze-candidate
    provides: the reviewed INSTALL wave with the Freeze base (11-21), the 11-27 advance span checks, the 11-29 header and spec text fixes and the class table docs/abi/status-classes.txt (11-02)
provides:
  - docs/spec/api.md, the written contract of the 35 public functions of the freeze candidate (postconditions, status classes, struct_size rule, ABI version rule, threading, additive-extension rule, candidate-then-final amendment rule), a software-interface claim only
  - a pointer from docs/spec/machine-contract.md to api.md
  - carry-in W2-01 closed: case z-product-bound (CTest unit.core.state.v26-z-product-bound, COST 1) and the two mutant rows z-limit-half and z-divisor-plus-one, RED survived, GREEN killed
  - carry-ins W1-03 (the ceil rule of the v8 clock in docs/spec/vi.md), D-10-17-01 (per-process run directory in test_cold_boot.c) and T8 (scripts/check-installed-case-map.py, CTest repo.installed-case-map) closed
  - PIN-IMPACT "Carry-in triage" final: 13 rows, 8 done and 5 deferred with reasons, final G3 count; PIN-IMPACT "Freeze steps" started
affects: [11-23, 11-24, 11-25]

actuals:
  tokens: 23054
  tasks: 3
  commits: 5

plan_head_before: ca0a615426f3ba1bc7a78f1ea7ad932e62c6c45c
plan_head_after: ec471dffaaf55a95048cde3a9eb51a9c8d33043a

tech-stack:
  added: []
  patterns:
    - "a mutant-driver RED run needs its rows committed (the driver builds a throwaway worktree from HEAD and refuses a dirty tree), so the RED milestone is its own test commit before the GREEN commit"
    - "a boundary case for a written-out copy of a live check: bisect the live acceptance for the largest accepted magnitude per input, compare it with an independent limit / bound, then send that magnitude and the next one through the copy"
    - "a case-map checker whose allowlist of script names lives in the checker, with seeded defects that fail with their own rule beside a clean control"

key-files:
  created:
    - docs/spec/api.md
    - scripts/check-installed-case-map.py
    - .planning/phases/11-installed-consumer-and-freeze-candidate/11-22-SUMMARY.md
  modified:
    - docs/spec/machine-contract.md
    - docs/spec/vi.md
    - core/tests/test_state_import_v26.c
    - core/CMakeLists.txt
    - scripts/p10-mutants.py
    - scripts/check-test-inventory.sh
    - scripts/preflight-phase11-regexes.txt
    - runner/tests/test_cold_boot.c
    - tools/abi/CMakeLists.txt
    - .planning/phases/11-installed-consumer-and-freeze-candidate/PIN-IMPACT.md

key-decisions:
  - "api.md describes the code as it is: every status order and out-parameter answer was read from the code of each entry (not only the header comments), so it records that admit_rom answers INVALID_ROM for a NULL span and a wrapping or overlapping span, that video/audio info and copy answer NOT_READY for an empty ring or a profile without the device, that a later advance after a terminal stop still answers TIME_REVERSED or LIMIT_REVERSED first, and that trace and the ISViewer capture are not lossless (only the video and audio rings are)"
  - "The dy boundary of the Z product proof is reachable only for the tallest triangle (row bound exactly 2^30), so the dzdy probe uses YH -8192 and YL 8191; there the largest accepted magnitude is 2^31 - 1, which is exactly where the y_bound + 1 divisor mutant disagrees"
  - "repo.installed-case-map is registered with FIXTURES_REQUIRED t64_consumer and its preflight count row is 3 (itself, installed.consumer.build and install.setup), not the plan's 1"
  - "No requirement is marked complete: API-02 and API-03 close with the bound record (11-23) and the final audit (11-25)"

patterns-established:
  - "FREEZE steps record form: commits, what the document or case pins, RED and GREEN evidence, COST and TIMEOUT of each new name, gate and preflight result, observations for the next plan"

requirements-completed: []

coverage:
  - id: D1
    description: "docs/spec/api.md has one postcondition row per public function (35, the function set equal to docs/abi/public-symbols.txt), the error classes with a pointer to docs/abi/status-classes.txt, the RESOURCE_LIMIT wording, the bus-write carve-out and the bus-read value_out statement, the struct_size rule, the ABI version rule and SameMinorVersion, the installed header set, the t64i_ prefix rule, copy-out rings, host time never an input, the threading statement and the additive-extension rule, and states that the candidate is a software-interface claim only"
    requirement: API-02
    verification:
      - kind: command
        ref: "the plan's verify: grep -c over api.md prints 34 (at least 6), sh scripts/check-docs.sh ok, sh scripts/pii-scan.sh --tree 0 findings; also sh scripts/check-claims.sh --files docs/spec/api.md ok; table row set compared with docs/abi/public-symbols.txt by script (identical)"
        status: pass
    human_judgment: true
    rationale: "whether each postcondition sentence is a faithful reading of the code is a reviewer judgment; 11-25 compares the table with the tests (T-11-49)"
  - id: D2
    description: "W2-01: a committed boundary case pins the Z product bound against the live acceptance and the two surviving mutants are rows of scripts/p10-mutants.py that are KILLED"
    requirement: API-03
    verification:
      - kind: command
        ref: "build/gate/11-22-task2/red.txt (z-limit-half SURVIVED, z-divisor-plus-one SURVIVED) and green.txt (both KILLED by test_state_import_v26:z-product-bound); ctest -R '^(unit.core.state.v26-z-product-bound|repo.p10-mutant-anchors)$' passes"
        status: pass
    human_judgment: false
  - id: D3
    description: "W1-03, D-10-17-01 and T8 are done and PIN-IMPACT 'Carry-in triage' ends with every item done or deferred with a reason (13 rows)"
    requirement: API-03
    verification:
      - kind: command
        ref: "grep -c over the 13 triage rows prints 13; grep -c 'attempt <= 99u' runner/tests/test_cold_boot.c prints 0; both cold-boot tests passed twice in one --repeat until-fail:2 invocation; repo.installed-case-map passes; sh scripts/check-test-inventory.sh --preset gate PASS count=326"
        status: pass
    human_judgment: false

duration: 30min
completed: 2026-11-26
---

# Phase 11 Plan 22: docs/spec/api.md and the Phase 10 carry-in closures Summary

**The freeze candidate has its written contract (docs/spec/api.md: 35 postcondition rows, the three error classes, the struct_size and ABI version rules, threading, the additive-extension rule and the amendment rule, a software-interface claim only), the Z product bound of the state importer is pinned at its boundary by a case that kills two previously surviving mutants, and the four remaining Phase 10 carry-ins (W2-01, W1-03, D-10-17-01, T8) are closed with the triage table final.**

## Performance

- **Duration:** about 30 min (reading about 10 min, api.md about 8 min, the boundary case and mutant runs about 6 min, Task 3 and the checks about 8 min)
- **Tasks:** 3 (Task 1 `tracer`, Tasks 2 and 3 `auto`; Task 2 `tdd`)
- **Files modified:** 10 modified, 3 created (including this summary); no production core source (`core/CMakeLists.txt` gains the test registration only)

## Accomplishments

- **Task 1 (tracer).** `docs/spec/api.md` (16 sections). The postcondition table has 35 rows with the columns function, accepted postcondition, refusals in check order, NULL machine, poisoned and image; its function set equals `docs/abi/public-symbols.txt`. It cites the check orders of the header and of each entry's code, the class table as the single source (`docs/abi/status-classes.txt`), the `RESOURCE_LIMIT` wording, the frame-event counter text of the header, the `t64_machine_bus_write` carve-out and the caller's-responsibility statement for the `value_out` span of `t64_machine_bus_read`, the pinned Open IPL3 admission narrowing, the `struct_size` rules, the create handshake and the ABI version rule with `SameMinorVersion`, poison, reset, unload, `output_status`, threading (distinct instances independent, one instance externally serialized) and the additive-extension rule (size fields, additive profiles, devices and error subcodes). It states that the candidate is a software-interface claim only with no compatibility, accuracy or hardware claim. The tracer gate (the plan's verify re-run end to end, interactive mode) passed before Task 2.
- **Task 2 (W2-01).** RED: the two mutant rows `z-limit-half` (limit `INT64_MAX / 2`) and `z-divisor-plus-one` (the dy divisor `y_bound + 1`) were committed first and both SURVIVED the committed hostile case (`red.txt`). GREEN: case `z-product-bound` bisects, per derivative, the largest magnitude the live acceptance (`t64_rdp_triangle_bounds`) takes, compares it with `limit / bound` computed from the packet words (dzdx 63,160,376; dzde 63,161,283; dzdy 2,147,483,647) and sends that magnitude and the next through the importer (accepted, then refused with STATE_RANGE, FIELD_RANGE, section 8, LEN_LE); both rows are now KILLED by it (`green.txt`). Native 0.75 s, `COST 1`.
- **Task 3.** W1-03: the v8 "Clock and boundary policy" section of `docs/spec/vi.md` states the `ceil` rule and formula and the two NTSC figures the test comment cites. D-10-17-01: `make_evidence_dir` names the run directory `06-05-cold-<mode>-pid<pid>-run-<n>` with no fixed cap. T8: `scripts/check-installed-case-map.py` and CTest `repo.installed-case-map` (24 cases, 3 allowlisted script names, 27 registered names; ten seeded defects fail with their own rule beside a clean control). PIN-IMPACT "Carry-in triage" is final (13 rows: 8 done, 5 deferred, none dropped) with the G3 count, and "Freeze steps" starts.

## Task Commits

1. **Task 1: docs/spec/api.md and the machine-contract pointer** - `c987d8a` (docs)
2. **Task 2, RED: the two mutant rows (both survive)** - `2425c47` (test)
3. **Task 2, GREEN: the boundary case, CTest name, inventory and regex rows** - `445a990` (test)
4. **Task 3: W1-03 sentence, D-10-17-01 suffix, T8 checker and CTest name** - `8388042` (test)
5. **Task 3: final carry-in dispositions, G3 count, Freeze steps** - `ec471df` (docs)

`commits: 5` in the frontmatter is `git rev-list --count ca0a615..ec471df` (the measured instrument); the summary and the STATE and ROADMAP commits follow it.

## Verification

| Check | Result |
|-------|--------|
| Task 1 verify (`grep -c`, `check-docs.sh`, `pii-scan.sh --tree`) | 34 (at least 6), ok, 0 findings; `check-claims.sh --files` ok |
| Task 2 verify (`p10-mutants.py --run z-limit-half z-divisor-plus-one`, ctest of the case and `repo.p10-mutant-anchors`) | both KILLED (3.2 s and 3.0 s), exit 0; ctest 2 of 2 passed |
| Task 3 verify (cmake build, the two cold tests twice, `repo.installed-case-map`, inventory) | cold-hello 0.14 s and cold-startup 0.08 s passed in both runs of `--repeat until-fail:2`; case map passes (0.11 s); inventory PASS count=326 |
| Task 3 verify 2 (triage rows) | 13 |
| Acceptance (`grep -c 'attempt <= 99u'`) | 0 |
| `sh scripts/preflight-phase11.sh --no-replay` | PASS in 48 s (179 TIMEOUTs, 0 too low, lowest ratio 2.9; 233 units with the act image) |
| `sh scripts/gate.sh` (clean committed tree at `8388042`) | PASS, 326 of 326, ctest 81 s, total 142 s (load 5.6 to 7.2; no limit widened) |
| `sh scripts/pii-scan.sh --tree` | 0 findings (1428 files) before each docs commit |

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] The mutant driver needs the rows committed for its RED run**
- **Found during:** Task 2
- **Issue:** `scripts/p10-mutants.py --run` builds its throwaway worktree from `HEAD` and exits 2 on a modified tracked file, so the plan's "add the rows, run (RED), add the case, run (GREEN), commit test and rows together" cannot run in one uncommitted state.
- **Fix:** two commits: the rows alone with the committed hostile case as their kill check (RED, both SURVIVED, `2425c47`), then the case, the CTest registration and the switch of the kill check to `z-product-bound` (GREEN, `445a990`). The RED rows are a true TDD test commit; nothing is pushed between them.
- **Files modified:** `scripts/p10-mutants.py`, `core/tests/test_state_import_v26.c`, `core/CMakeLists.txt`, `scripts/check-test-inventory.sh`, `scripts/preflight-phase11-regexes.txt`
- **Commit:** `2425c47`, `445a990`

**2. [Rule 1 - Bug in the plan text] The preflight count row of repo.installed-case-map is 3**
- **Found during:** Task 3
- **Issue:** the plan says to raise the regex row to 1; `ctest -N -R '^repo\.installed-case-map$'` lists the fixture closure (`install.setup`, `installed.consumer.build`) with the name because of `FIXTURES_REQUIRED t64_consumer`, as in the plan 11-16 lesson, so the measured count is 3 and a row of 1 would fail the preflight.
- **Fix:** the row is 3 with a comment line naming the closure.
- **Files modified:** `scripts/preflight-phase11-regexes.txt`
- **Commit:** `8388042`

### Process deviations

**3. "Public unload" was already in the reset snapshot**
- **What:** the plan asks for a "Public unload" row in `docs/spec/machine-contract.md`; plan 11-04 added it (line 756 at this head). Only the pointer to `api.md` was added after the ABI notes.
- **Commit:** `c987d8a`

**4. The api.md table is written by hand, not generated**
- **What:** threat T-11-49 says the table is "generated from header comments and the NOT_READY table". No generator exists in the plan's file list; the table was written from the header comments, the code of each entry and the 11-04 NOT_READY table, and the function set was compared with `docs/abi/public-symbols.txt` by script (identical). No CTest name checks the table; the mitigation therefore rests on the 11-25 review that compares it with the tests, and 11-23's record or a later plan could add a cheap row-set check.
- **Files modified:** none beyond `docs/spec/api.md`.

**5. Task 3 closes in two commits**
- **What:** the PIN-IMPACT rows cite the commit hashes of the code they close, so the records went into a docs commit after the code commit (`8388042`, then `ec471df`).

**Total deviations:** 5 recorded (2 auto-fixed, 3 process). **Impact:** none on the delivered evidence.

## Authentication Gates

None.

## Issues Encountered

- `ctest -N` lists the fixture closure with a name (deviation 2).
- Host load stayed between 3.8 and 7.2; no limit was widened and nothing was rerun for load.
- `build/gate/10-p10-results.txt` now holds the two rows of this plan's last driver run (the file is overwritten by every `--run`).

## Known Stubs

None.

## Threat Flags

None. T-11-48 (hostile blob overflow in the Z product path) is mitigated by the boundary case and the two killed mutants; T-11-49 (contract text versus behavior) by reading each entry's code and by the 11-25 comparison with the tests (see deviation 4); T-11-50 (new docs) by the PII scan before every docs commit.

## Notes for the orchestrator

- **Nothing is pushed.** Local commits since `4b13a90`: `189afa8`, `0d17d6f`, `27cc3c0`, `ca0a615` (11-21 records) and this plan's commits; they go out with the next push a plan names. No orchestrator-only or owner step is open.
- **For 11-23.** `docs/abi/installed-undefined.txt` still says in a comment that the Linux rows are "NOT yet confirmed" by a hosted run; 11-21 confirmed them (run 45775107983) and the file is re-bindable until 11-23. `docs/spec/api.md` section 14 says the header hash and the freeze record are added by 11-23; 11-23 Task 3 adds the pointer from api.md to the record and the header hash must be taken at a head that contains this plan's api.md and the 11-29 header text.
- **For 11-25.** Compare the api.md table with the tests (T-11-49); the FREEZE ledger row records two more names (1.12 s of COST, `unit.core.state.v26-z-product-bound` and `repo.installed-case-map`) with the measured cold run.
- **Phase 10 qualification record.** `scripts/p10-mutants.py --list` has 30 rows now; the Phase 10 qualification record names 28 (a re-run of that qualifier would list 30).
- **Phase 6 evidence checker.** `scripts/check-base-evidence.py` resolves the closed Phase 6 records by the regex `attempt-[0-9]{2}`; the new run directories (`pid<pid>-run-<n>`) are not resolved by it, and a future re-record of that evidence would need the regex extended (not in this plan's files).
- **Requirements.** None marked complete (API-02 and API-03 close with 11-23 and 11-25).

## Self-Check: PASSED

- `docs/spec/api.md`, `scripts/check-installed-case-map.py` and this summary exist and are committed (the summary by the commit that follows this check)
- commits `c987d8a`, `2425c47`, `445a990`, `8388042` and `ec471df` are ancestors of HEAD
- `ctest --test-dir build/gate -R '^(unit.core.state.v26-z-product-bound|repo.installed-case-map|repo.p10-mutant-anchors)$'` passes; the full gate passed at `8388042`
