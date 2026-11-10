---
phase: 01-governance-rights-and-harness-foundation
plan: "02"
subsystem: infra
tags: [c17, supervisor, runner, synthetic-guest, canonical-json, golden, ctest, result-protocol]
status: complete

requires:
  - phase: 01-governance-rights-and-harness-foundation
    provides: "gate.sh, CMake presets, t64_core stub and facade, t64_common (SHA-256, UTF-8, canonical JSON writer), t64_test.h"
provides:
  - "t64-runner-synthetic: scripted guest behind t64_facade_ops, drive loop, five-line report writer"
  - "t64-supervisor: process-group backstop, atomic canonical t64.result v1 and non-canonical run-meta"
  - "t64_classify / t64_observation / t64_verdict: pure six-outcome precedence function and strict report parser"
  - "CTest controls.positive, controls.empty-halt (label controls) and unit.classify (label unit)"
  - "tests/controls: matrix.tsv, run-case.sh, scripts/, golden/ (directory-owned corpora)"
  - "docs/spec/result-protocol.md: the t64.result v1 contract"
affects: [01-07, 01-11, 01-13]

actuals:
  tokens: 22000
  tasks: 2
  commits: 2
plan_head_before: eed21532ed54e3fb3fb943ae57dbc81adb34b462
plan_head_after: 9c8c44fffaf8a29f8445d3f96fa569c02d1e167e

tech-stack:
  added: []
  patterns:
    - "Runner targets get _POSIX_C_SOURCE=200809L (plus _DARWIN_C_SOURCE on Apple) via T64_HOST_DEFS; core never does"
    - "Classifier is a pure library (t64_classify) linked by both the supervisor and its unit test"
    - "Goldens derived with host shasum over printf-produced bytes, never from supervisor output"
    - "Matrix rows become CTest tests at configure time; the matrix file is a CMAKE_CONFIGURE_DEPENDS input"

key-files:
  created:
    - runner/CMakeLists.txt
    - runner/t64_num.h
    - runner/t64_synthetic.c
    - runner/t64_supervisor.c
    - runner/t64_classify.h
    - runner/t64_classify.c
    - runner/tests/test_classify.c
    - tests/controls/CMakeLists.txt
    - tests/controls/matrix.tsv
    - tests/controls/run-case.sh
    - tests/controls/scripts/positive.t64s
    - tests/controls/scripts/empty-halt.t64s
    - tests/controls/golden/positive.json
    - tests/controls/golden/empty-halt.json
    - docs/spec/result-protocol.md
  modified:
    - CMakeLists.txt

key-decisions:
  - "Fixture present but not a regular file, over 64 MiB, or unreadable is harness-error/usage (the closed detail enum has no dedicated value for it)"
  - "A PASS line is a line starting 'PASS ' and the verdict line is exactly 'Done!' (synthetic-only grammar, documented in the spec)"
  - "report-invalid also covers a parsed report whose stop_reason is none, and log_bytes_total above 2^53-1 is rejected by the report parser so the JSON writer can never overflow"
  - "Supervisor caps --log-limit at 64 MiB and runner argv at 50 entries; both are usage errors beyond that"
  - "Counts for guest outcomes come from the captured (possibly truncated) bytes; log_digest is the digest of those bytes"

patterns-established:
  - "Process conditions in the classifier are only evaluated when the runner was launched; pre-launch fixture states resolve afterwards, so precedence stays one ordered function"
  - "Unclassified is reachable only through impossible observations (never reaped, nothing observed, garbage enum), keeping it a true defensive default"

requirements-completed: [RUN-02]

coverage:
  - id: D1
    description: "A positive synthetic control yields pass/positive-match exit 0 and an empty-output control yields boot-fail/empty-output exit 11; both canonical results are byte-identical across slice sizes 1 and 7 and equal independently derived goldens"
    requirement: "RUN-02"
    verification:
      - kind: integration
        ref: "ctest --preset gate -L controls (controls.positive, controls.empty-halt)"
        status: pass
    human_judgment: false
  - id: D2
    description: "The six-outcome precedence table is one pure function; every single-run detail value, the precedence edges and the report-invalid sub-conditions are exact-checked"
    requirement: "RUN-02"
    verification:
      - kind: unit
        ref: "ctest --preset gate -R unit.classify (classify rows: 53, checks=183 failures=0)"
        status: pass
    human_judgment: false
  - id: D3
    description: "The supervisor kills a hung runner and its grandchild via the process group, reports exec failure, bad arguments and a missing report, and always writes a canonical result"
    requirement: "RUN-02"
    verification:
      - kind: other
        ref: "manual live runs (hang with background grandchild, exit-0-no-result, spin past wall budget, missing binary, unknown option); recorded under Measured results"
        status: pass
    human_judgment: false
  - id: D4
    description: "docs/spec/result-protocol.md states the t64.result v1 contract"
    requirement: "RUN-02"
    verification:
      - kind: other
        ref: "grep -c '^## ' = 7; contains wall-backstop and schema_version"
        status: pass
    human_judgment: false

duration: 30min
completed: 2026-11-10
---

# Phase 1 Plan 02: Runner, supervisor, classifier and result protocol Summary

**A synthetic guest behind the facade, driven by `t64-runner-synthetic` and classified by `t64-supervisor`, yields slice-independent canonical `t64.result` v1 bytes that match independently derived goldens, with the six-outcome precedence extracted into a pure function proven by 53 table rows.**

## Accomplishments

- Tracer slice proven end to end (script, runner, supervisor, canonical result, golden `cmp` in ctest) for one pass and one boot-fail, then expanded.
- `t64-runner-synthetic`: strict script grammar v1 (`log`, `logx`, `end quiesce|spin`, bounds as specified), guest state in an instance struct, drive loop with `min(tick + slice, bound)`, capture written with `write(2)` up to the log limit while still counting, exactly five-line report.
- `t64-supervisor`: argument validation, `lstat` fixture check (regular file, at most 64 MiB), digest check before launch, private `mkdtemp` work dir with `O_CREAT|O_EXCL|O_NOFOLLOW` 0600 files, fork/exec with CLOEXEC exec-failure pipe, own process group, `RLIMIT_CORE` 0 and `RLIMIT_FSIZE` of log limit plus 64 KiB, polled `waitpid` with 2 ms naps against a `CLOCK_MONOTONIC` deadline, SIGTERM/SIGINT handlers without `SA_RESTART`, SIGPIPE ignored, group SIGKILL plus blocking reap, `.tmp` then `rename` for the result and run-meta.
- `t64_classify`: `t64_classify`, `t64_parse_report`, name and exit-code helpers; the precedence table in `docs/spec/result-protocol.md` is the single source.
- `docs/spec/result-protocol.md`: seven sections covering the evidence stamp, canonical object, precedence and exit codes, runner report, script grammar, command lines and the control matrix.

## Measured results

| Item | Value |
|------|-------|
| `./scripts/gate.sh`, clean `build/gate` (Darwin arm64, Apple clang 21.0.0, CMake 4.4.3, Ninja 1.13.2) | `[gate] total 1s` (sh-n 0s, configure 1s, build 0s, ctest 0s), `[gate] PASS` |
| `./scripts/gate.sh`, warm tree | 0.29 s wall |
| Tests in the gate | `unit.common`, `unit.classify`, `controls.positive`, `controls.empty-halt` (4 of 4 passed) |
| `unit.classify` | `classify rows: 53`, `checks=183 failures=0` (acceptance floor: 20 rows) |
| Acceptance greps | `"outcome":"pass"` count 1, `"detail":"empty-output"` count 1, `/` in goldens 0, `find core -type f` 2, `^## ` in the spec 7 |

Live supervisor runs (scratch fixtures, results not committed):

| Case | Observed |
|------|----------|
| `sh` runner that backgrounds a `sleep` grandchild then sleeps, 400 ms budget | `harness-error/wall-backstop`, exit 1, returned in under 1 s, no `sleep` left running a second later |
| Runner that exits 0 without a report | `harness-error/no-result`, exit 1 |
| Synthetic `spin` guest with bound 10^12 and slice 1, 300 ms budget | `harness-error/wall-backstop`, exit 1, about 1 s |
| Synthetic `spin` guest, bound 1000, slice 1000 | `boot-fail/empty-output`, exit 11 (empty output wins over timeout, per RUN-03) |
| Missing runner binary | `harness-error/exec-failed`, exit 1 |
| Unknown option | `harness-error/usage`, exit 1, `fixture_digest` null, `fixture_id` `none` |
| Work dir after every run | empty (private dir removed) |

Golden derivation (independent of the code under test):

- Profile hash: `printf '{"profile_id":"synthetic-stub","schema":"t64.profile","schema_version":1}\n' | shasum -a 256` (74 bytes with the LF, 73 without), giving `3c9058c6...bde1e`.
- Fixture digests: `shasum -a 256` over the two committed scripts.
- `positive` capture: `printf 'PASS alpha\nPASS beta\nDone!\n' | shasum -a 256`, 27 bytes, 3 lines, 2 PASS; guest tick 40 read from `end 40 quiesce`; stop reason `guest-halt` because 40 is below the bound 1000.
- `empty-halt` capture: SHA-256 of the empty string (`e3b0c442...b855`), 0 bytes; guest tick 5 from `end 5 quiesce`.
- Each golden is one hand-assembled `printf` line plus LF; no supervisor output was copied.

Mutation sanity check (scratch copies, not committed): disabling the `fail-reported` rule, the `output-after-done` rule, the `empty-output` rule or the wall-backstop rule each made `unit.classify` fail.

## Task Commits

1. Task 1 (tracer): `1dafcdc` - synthetic runner, supervisor, controls and goldens inside the gate
2. Task 2: `9c8c44f` - pure classifier, `unit.classify`, supervisor wiring and `docs/spec/result-protocol.md`

Tracer feedback gate (auto mode): `<verify>` re-run end to end after Task 1, passed, expanded.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] `_POSIX_C_SOURCE=200809L` alone hides `O_NOFOLLOW` and `mkdtemp` on macOS**
- **Found during:** Task 1 first build
- **Issue:** Apple clang 21 reported `use of undeclared identifier 'O_NOFOLLOW'` and an implicit declaration of `mkdtemp` with only `_POSIX_C_SOURCE=200809L`. RESEARCH Pitfall 7 measured only `fork`, `sigaction` and `clock_gettime`.
- **Fix:** `runner/CMakeLists.txt` builds `T64_HOST_DEFS` and adds `_DARWIN_C_SOURCE` on Apple. Linux exposes both under 200809L (not exercised here; CI in plan 01-13 confirms). Core targets are untouched.
- **Files modified:** runner/CMakeLists.txt
- **Commit:** 1dafcdc

**2. [Rule 3 - Blocking] `-Wcast-qual` rejects casting string literals into the exec argv**
- **Found during:** Task 1 first build
- **Fix:** a small `dupstr` heap copy for each appended argv entry; no casts.
- **Commit:** 1dafcdc

**3. [Rule 2 - Missing critical functionality] Added `runner/t64_num.h`**
- A strict, overflow-checked decimal parser is needed by the runner, supervisor and report parser. It is a header-only helper, listed here because it is not in `files_modified`.
- **Commit:** 1dafcdc

### Interpretation notes (no conflict with a measured fact)

- The interfaces block does not say where `usage`-class failures that are not argument errors land (unusable work dir, a non-regular fixture, fork failure). All map to `harness-error/usage`.
- `report-invalid` also fires for a parsed report with no valid stop reason, and the parser caps `log_bytes_total` at 2^53-1 so canonical integers can never overflow.
- Matrix `fixture_sha256` is pinned for every row, including `deleted` and `tampered` modes (the pinned digest is the original script's); only `ok` rows exist so far.

## Known Stubs

None. `core/t64_core_stub.c` remains the intentional placeholder recorded in plan 01-01.

## Threat Flags

None beyond the plan's register. Mitigations applied: T-01-04 (own process group, monotonic backstop, group SIGKILL and blocking reap, output to files, `RLIMIT_FSIZE`/`RLIMIT_CORE`; live-verified with a grandchild), T-01-05 (private `mkdtemp`, `O_EXCL|O_NOFOLLOW` 0600, `.tmp` then rename, `lstat` regular-file check, 64 MiB cap), T-01-06 (exact five-line report, overflow-checked decimals, `fstat` before reading, runner exit code never decides the outcome, all unit-tested), T-01-07 (no wall time, hostname or path in the canonical object; goldens contain no `/`; scan of `runner/`, `tests/controls/` and `docs/spec/` for home or temp paths and `@` found nothing), T-01-SC-02 (no installs, nothing vendored).

## Notes for later plans

- Plan 01-07 (suite consumer): `t64_detail_name` and `t64_detail_outcome` already cover `inventory-mismatch` and `suite-truncated`; consumers reject unknown fields and unknown `schema_version`.
- Plan 01-11: add script directives `hang abort segv exit0-no-result exit7 logrep` in `syn_parse` and the guest in `runner/t64_synthetic.c` (today any other directive exits 2). `run-case.sh` fixes `--log-limit` at 65536; the over-limit control needs a per-row value, so add a matrix column or an override then. `run-case.sh` uses `--bin-dir $<TARGET_FILE_DIR:t64-supervisor>` and expects both binaries there.
- Crash controls may leave `.ips` files in the user's macOS diagnostic reports directory; the spec notes this.
- Linux acceptance of `_POSIX_C_SOURCE=200809L` without `_DARWIN_C_SOURCE` and of gcc 13 / clang 18 flags is first confirmed in CI (plan 01-13).

## Self-Check: PASSED

- FOUND: CMakeLists.txt, runner/{CMakeLists.txt,t64_num.h,t64_synthetic.c,t64_supervisor.c,t64_classify.h,t64_classify.c,tests/test_classify.c}, tests/controls/{CMakeLists.txt,matrix.tsv,run-case.sh,scripts/positive.t64s,scripts/empty-halt.t64s,golden/positive.json,golden/empty-halt.json}, docs/spec/result-protocol.md
- FOUND commits: 1dafcdc, 9c8c44f (both ancestors of HEAD)
