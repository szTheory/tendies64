---
phase: 01-governance-rights-and-harness-foundation
plan: "11"
subsystem: testing
tags: [c17, negative-controls, matrix, inventory, suite, golden, supervisor, synthetic-guest, run-02, run-04]
status: complete

requires:
  - phase: 01-governance-rights-and-harness-foundation
    provides: "synthetic runner, supervisor, classifier and result protocol (01-02); t64-result-check seal and suite (01-07)"
provides:
  - "19-case live control matrix (tests/controls/matrix.tsv) with scripts, independently derived goldens and INVENTORY"
  - "Synthetic guest directives hang, abort, segv, exit0-no-result, exit7, the logrep line and the test-only --ignore-log-limit flag"
  - "run-suite.sh: live suite seal plus dropped, truncated, drift and inventory controls"
  - "CTest controls.<id> for 19 ids plus controls.suite, controls.suite-dropped, controls.suite-truncated, controls.inventory-drift, controls.inventory"
  - "tests/controls/README.md and the spec grammar, flag and column updates"
affects: [01-12, 01-13]

actuals:
  tokens: 8900
  tasks: 3
  commits: 3
plan_head_before: d16bbe775f35b2424ccdb35dc0e2400d810d19c5
plan_head_after: 47a8fdb35860791c7078cff0833793ffd95b40bc

tech-stack:
  added: []
  patterns:
    - "Matrix rows carry runner_flags and log_limit columns appended after exit, so field numbers 1-12 are unchanged"
    - "run-case.sh --result-out lets run-suite.sh reuse the verified canonical result of each case"
    - "Every negative control asserts exit code, rule id and detail, not merely non-zero"

key-files:
  created:
    - tests/controls/INVENTORY
    - tests/controls/run-suite.sh
    - tests/controls/README.md
    - tests/controls/scripts/ (15 new .t64s scripts; deleted-fixture and tampered-digest reuse positive.t64s)
    - tests/controls/golden/ (17 new goldens)
  modified:
    - runner/t64_synthetic.c
    - tests/controls/matrix.tsv
    - tests/controls/run-case.sh
    - tests/controls/CMakeLists.txt
    - docs/spec/result-protocol.md

key-decisions:
  - "Host-misbehavior behaviours are new `end` directives (not standalone lines), so the grammar keeps exactly one end line and the host action fires inside advance when the bound reaches the end tick"
  - "exit7 is quiesce plus a post-report exit 7, so the report is complete and valid and only the exit code distinguishes the case"
  - "logrep is stored as one event with a repeat count, so a 64 MiB flood needs no 64 MiB buffer"
  - "run-case.sh validates every result with t64-result-check result, so a control cannot pass on a result the consumer would refuse"
  - "No classifier or supervisor change was needed: every case matched the locked precedence table and the supervisor's existing process handling"

requirements-completed: [RUN-02, RUN-04]

coverage:
  - id: D1
    description: "Each of the 19 live matrix cases yields its exact outcome, detail and exit code; each runs at slice sizes 1 and 7 with byte-identical canonical results equal to an independently derived golden"
    requirement: "RUN-02"
    verification:
      - kind: integration
        ref: "ctest --preset gate -L controls (19 controls.<id> tests passed)"
        status: pass
    human_judgment: false
  - id: D2
    description: "Wrong expected values (count mismatch, FAIL line, output after Done!) are guest-fail; missing, tampered, truncated, malformed, timed-out, hung, crashed and silent runs never pass"
    requirement: "RUN-04"
    verification:
      - kind: integration
        ref: "controls.wrong-counts, fail-reported, fail-after-done, truncated-verdict, log-invalid-utf8, log-over-limit, deleted-fixture, tampered-digest, abort, segv, exit0-no-result, exit-nonzero, hang, log-flood-backstop"
        status: pass
    human_judgment: false
  - id: D3
    description: "A spin-forever guest is timeout/guest-bound and a hung runner process is harness-error/wall-backstop"
    requirement: "RUN-04"
    verification:
      - kind: integration
        ref: "controls.spin (timeout, exit 12) and controls.hang (wall-backstop, exit 1, 1.47 s for two runs)"
        status: pass
    human_judgment: false
  - id: D4
    description: "The full matrix is sealed into a suite verified against tests/controls/INVENTORY; dropped-case, truncated-suite and inventory-drift controls each fail as harness-error with the expected rule id and detail"
    requirement: "RUN-04"
    verification:
      - kind: integration
        ref: "controls.suite, controls.suite-dropped, controls.suite-truncated, controls.inventory-drift, controls.inventory"
        status: pass
    human_judgment: false
---

# Phase 1 Plan 11: Negative-control matrix Summary

**A 19-case live control matrix drives the synthetic runner through every outcome (pass, timeout, boot-fail, guest-fail, missing-fixture, harness-error, including crash, hang, silent exit, exit 7 and a log flood killed by RLIMIT_FSIZE), each run twice at slice sizes 1 and 7 against independently derived goldens, then sealed into a suite checked against a pinned INVENTORY with live dropped, truncated and drift controls.**

## Accomplishments

- Tracer slice proven end to end: `spin` (timeout/guest-bound, exit 12) through new matrix columns, a pinned INVENTORY, a sealed and verified suite, and the five live suite controls, then expanded.
- `t64_synthetic.c`: `end` directives `hang`, `abort`, `segv`, `exit0-no-result`, `exit7`; `logrep <tick> <count> <text>` (total at most 64 MiB, stored as one repeated event); the test-only `--ignore-log-limit` flag.
- 17 new cases on top of 01-02's two (positive, empty-halt): the 15 rows of the plan table plus the Discretion rows positive-unhandled, fail-reported, empty-bound, exit-nonzero and log-flood-backstop.
- `run-case.sh` now passes `runner_flags` and `log_limit`, validates both canonical results with `t64-result-check result`, and can export the verified result for the suite.
- `run-suite.sh` modes `suite|dropped|truncated|drift|inventory`; each negative mode asserts exit 1, the exact rule id (`S-INVENTORY`, `S-TRUNCATED`) and the exact echoed detail (`inventory-mismatch`, `suite-truncated`).
- `tests/controls/README.md` (case table, golden derivation, twice-run rule, suite controls, `.ips` note, Phase 3 and 4 replay obligation) and `docs/spec/result-protocol.md` (grammar, flag, columns, 19-case matrix).

## Measured results

| Item | Value |
|------|-------|
| `ctest --preset gate -L controls` | 24 of 24 passed (19 cases plus 5 suite controls), 14.5 s serial |
| `ctest --preset gate -L controls -N` | 24 tests (after Task 1: 8) |
| `ctest --preset gate -R controls.hang` | 1.47 s for two runs (about 735 ms each against the 700 ms bound) |
| `./scripts/gate.sh` (Darwin arm64) | `[gate] total 35s` (sh-n 0s, lint-selftest 3s, configure 0s, build 0s, ctest 32s, core-lint 0s), `[gate] PASS`; full ctest 46 of 46 passed |
| `log-flood-backstop` wait status | 25 (SIGXFSZ), `runner-signalled`, supervisor returned in 4 ms |
| Acceptance greps | `"outcome":"timeout"` in spin.json 1; wall-backstop in hang.json 1; runner-exit-nonzero in exit-nonzero.json 1; `output-after-done` in fail-after-done.json 1; golden files with `"outcome":"pass"` 2 |
| INVENTORY | 19 lines; `awk ... matrix.tsv \| LC_ALL=C sort \| cmp - INVENTORY` exits 0 |
| README | 6 `## ` headings; no line starts with `Tendies64 ` and none contains the whole words accurate, compatible or supports |
| `scripts/pii-scan.sh --tree` | findings=0 |

Mutation checks (temporary, restored): appending an id to INVENTORY made `controls.inventory` fail ("sorted matrix ids differ from INVENTORY") and `controls.suite` fail at seal with S-INVENTORY.

Golden derivation: each golden was assembled with `printf` from facts read off the script (digest of the script via `shasum -a 256`, capture bytes written with `printf` and hashed, counts from the text, tick from the `end` line or the bound). The tampered-digest golden uses the digest of `positive.t64s` plus one `#` byte; log-over-limit hashes the first 64 of its 100 bytes. No golden came from supervisor output, and every golden matched the supervisor byte for byte on the first run.

## macOS crash reports (`.ips`)

Measured by running each control alone: `abort` produced 2 reports (one per run), `segv` 2, and `log-flood-backstop`, `hang` and `exit0-no-result` none. All were named `t64-runner-synthetic-<timestamp>.ips` in the user diagnostic reports directory. I deleted every `t64-runner-synthetic-*.ips` file (only files from this project's runner; other programs' files untouched). A later full gate run left none behind (checked after 3 s), so report creation on this host is not consistent. The README documents the side effect.

## Task Commits

1. Task 1 (tracer): `82e6400` - spin control, matrix columns, INVENTORY, run-suite.sh and the five live suite controls
2. Task 2: `80b8110` - host-misbehavior directives, `logrep`, `--ignore-log-limit` and eight host and fixture-mode cases
3. Task 3: `47a8fdb` - eight guest-verdict cases, README and spec

Tracer feedback gate (auto mode): `<verify>` re-run end to end after Task 1, passed, expanded.

## Deviations from Plan

None - plan executed exactly as written, with the interpretation notes below (none conflicts with a measured fact).

### Interpretation notes

- **No supervisor or classifier edit:** `runner/t64_supervisor.c` and `runner/t64_classify.c` are in the plan's `files_modified` but needed no edit. The supervisor's existing checks (no-result, runner-exit-nonzero, runner-signalled for SIGXFSZ/SIGABRT/SIGSEGV, group kill and reap on the backstop, `RLIMIT_FSIZE` of log limit plus 64 KiB, `RLIMIT_CORE` 0) already produced every expected result, and every guest-verdict case agreed with the locked precedence table. `unit.classify` therefore needs no follow-up for a precedence gap.
- **Where the host actions live:** the plan names the directives without a syntax; they were added as `end` directives (exactly one per script, last line), which keeps the grammar's "one end line" rule.
- **Column order:** `runner_flags` and `log_limit` are appended after `exit` (columns 13 and 14), so existing field numbers did not move. Every row carries an explicit `log_limit`.
- **`run-case.sh --result-out`:** an extra option (not in the plan) so `run-suite.sh` reuses each case's verified result instead of duplicating supervisor invocation logic.
- **Suite controls re-run the matrix:** each of the four suite-mode tests runs every case (about 2.2 to 2.9 s each, serial), which keeps each control self-contained. The gate's ctest step is 32 s in total; it could be trimmed later by caching one sealed suite, but is within the 60 s budget.
- **Replay note for Phases 3 and 4:** the real-core runner must accept the same case ids; INVENTORY makes dropping one a failing control.

**Total deviations:** 0.

## Known Stubs

None.

## Threat Flags

None beyond the plan's register. Mitigations applied: T-01-38 (log-flood-backstop proves RLIMIT_FSIZE kills a runner that ignores the log limit; files rather than pipes from 01-02), T-01-39 (RLIMIT_CORE 0; `.ips` files created by the controls deleted; README documents the side effect), T-01-40 (exact outcome, detail and exit per case, byte compare with independent goldens, INVENTORY pin with live dropped, truncated and drift controls), T-01-41 (twice-run at slices 1 and 7 with `cmp`; harness-error results null capture-dependent fields), T-01-SC-11 (no installs).

## Self-Check: PASSED

- FOUND: tests/controls/{INVENTORY,run-suite.sh,README.md,matrix.tsv,run-case.sh,CMakeLists.txt}, runner/t64_synthetic.c, docs/spec/result-protocol.md; 17 scripts (19 cases; deleted-fixture and tampered-digest reuse positive.t64s) and 19 goldens (`ls tests/controls/scripts | wc -l` 17, `ls tests/controls/golden | wc -l` 19)
- FOUND commits: 82e6400, 80b8110, 47a8fdb (all ancestors of HEAD)
- Acceptance criteria and plan-level verification re-run after Task 3: `ctest --preset gate -L controls` 24 of 24, `./scripts/gate.sh` exits 0 with `[gate] PASS` in 35 s.
